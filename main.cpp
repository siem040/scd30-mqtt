#include "scd30_i2c.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include "env_loader.h"
#include "pahomqtt/src/MQTTClient.h"

// Configuration Variables
std::string BROKER_URI;
std::string CLIENT_ID;
std::string TOPIC_TEMP;
std::string TOPIC_HUM;
std::string TOPIC_CO2;
std::string USERNAME;
std::string PASSWORD;
long TIMEOUT_MS = 10000L;
int INTERVAL_SEC = 600;

// Wrapper for sensirion sleep
#define sensirion_hal_sleep_us sensirion_i2c_hal_sleep_usec

// Optimization: Connect once, publish all 3, then disconnect
// to avoid 3 separate connection handshakes per measurement
int publish_measurements(float co2, float temp, float hum) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    if ((rc = MQTTClient_create(&client, BROKER_URI.c_str(), CLIENT_ID.c_str(),
        MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to create client, return code %d\n", rc);
        return rc;
    }

    conn_opts.keepAliveInterval = 10;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = 5; // 5 seconds connection timeout
    
    if (!USERNAME.empty()) {
        conn_opts.username = USERNAME.c_str();
        conn_opts.password = PASSWORD.c_str();
    }

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        MQTTClient_destroy(&client);
        return rc;
    }

    // Define topics and values
    const char* topics[] = {TOPIC_CO2.c_str(), TOPIC_TEMP.c_str(), TOPIC_HUM.c_str()};
    float values[] = {co2, temp, hum};

    for(int i = 0; i < 3; ++i) {
        char payload[32];
        snprintf(payload, sizeof(payload), "%.2f", values[i]);

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = payload;
        pubmsg.payloadlen = (int)strlen(payload);
        pubmsg.qos = 1;
        pubmsg.retained = 0;

        MQTTClient_deliveryToken token;
        int pub_rc = MQTTClient_publishMessage(client, topics[i], &pubmsg, &token);
        if (pub_rc != MQTTCLIENT_SUCCESS) {
            printf("Failed to publish to %s, return code %d\n", topics[i], pub_rc);
        } else {
            MQTTClient_waitForCompletion(client, token, TIMEOUT_MS);
        }
    }

    if ((rc = MQTTClient_disconnect(client, 1000)) != MQTTCLIENT_SUCCESS)
        printf("Failed to disconnect, return code %d\n", rc);

    MQTTClient_destroy(&client);
    return rc;
}

int main(void) {
    // Load Configuration
    auto env = load_env(".env");
    if (env.empty()) {
        printf("Warning: No .env file found or empty. Using defaults/failures may occur if keys are missing.\n");
    }

    // Use get_env_value to populate globals, providing defaults where appropriate
    BROKER_URI = get_env_value(env, "BROKER_URI", "tcp://localhost:1883");
    CLIENT_ID = get_env_value(env, "CLIENT_ID", "SCD30_Publisher");
    TOPIC_TEMP = get_env_value(env, "TOPIC_TEMP", "siem/bedroom/temperature");
    TOPIC_HUM = get_env_value(env, "TOPIC_HUM", "siem/bedroom/humidity");
    TOPIC_CO2 = get_env_value(env, "TOPIC_CO2", "siem/bedroom/co2");
    USERNAME = get_env_value(env, "USERNAME", "your_username");
    PASSWORD = get_env_value(env, "PASSWORD", "your_password");
    
    std::string interval_str = get_env_value(env, "INTERVAL", "600");
    INTERVAL_SEC = std::atoi(interval_str.c_str());
    if (INTERVAL_SEC < 2) INTERVAL_SEC = 2; // Minimum for SCD30

    printf("Starting SCD30 Publisher...\n");
    printf("Broker: %s\n", BROKER_URI.c_str());
    printf("Interval: %d seconds\n", INTERVAL_SEC);

    int16_t error = 0;

    sensirion_i2c_hal_init();
    scd30_init(SCD30_I2C_ADDR_61);

    // Stop potential existing periodic measurement
    scd30_stop_periodic_measurement();
    scd30_soft_reset();
    sensirion_hal_sleep_us(2000000);

    uint8_t major = 0;
    uint8_t minor = 0;
    error = scd30_read_firmware_version(&major, &minor);
    if (error != 0) {
        printf("Error reading firmware version: %i\n", error);
        return error;
    }
    printf("SCD30 Firmware Version: %u.%u\n", major, minor);

    // Start periodic measurement with 2 second interval
    error = scd30_start_periodic_measurement(0);
    if (error != 0) {
        printf("Error starting periodic measurement: %i\n", error);
        return error;
    }
    printf("Started periodic measurement.\n");

    float co2_concentration = 0.0;
    float temperature = 0.0;
    float humidity = 0.0;
    uint16_t data_ready = 0;
    bool running = true;

    while (running) {
        error = scd30_blocking_read_measurement_data(&co2_concentration, &temperature, &humidity);
        if (error != 0) {
            printf("Error reading measurement data: %i\n", error);
            running = false;
            continue;
        }

        printf("Read: CO2=%.2f, Temp=%.2f, Hum=%.2f\n", co2_concentration, temperature, humidity);
        // Publish all measurements in one connection session
        publish_measurements(co2_concentration, temperature, humidity);
        
        // Sleep for configurable interval
        sleep(INTERVAL_SEC);
    }

    // In unreachable code (infinite loop), but for completeness:
    error = scd30_stop_periodic_measurement();
    if (error != NO_ERROR) {
        return error;
    }
    return 0;
}
