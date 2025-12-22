#include "scd30_i2c.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include "pahomqtt/src/MQTTClient.h"

#define CLIENTID    "SCD30_Publisher"
#define BROKER_URI  "tcp://localhost:1883"
#define TOPIC_TEMP  "siem/bedroom/temperature"
#define TOPIC_HUM   "siem/bedroom/humidity"
#define TOPIC_CO2   "siem/bedroom/co2"
#define TIMEOUT     10000L
#define USERNAME "your_username"
#define PASSWORD "your_password"

#define sensirion_hal_sleep_us sensirion_i2c_hal_sleep_usec

int publish_measurements(float co2, float temp, float hum) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    if ((rc = MQTTClient_create(&client, BROKER_URI, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to create client, return code %d\n", rc);
        return rc;
    }

    conn_opts.keepAliveInterval = 10;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = 5; // 5 seconds connection timeout
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        MQTTClient_destroy(&client);
        return rc;
    }

    // Define topics and values
    const char* topics[] = {TOPIC_CO2, TOPIC_TEMP, TOPIC_HUM};
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
            MQTTClient_waitForCompletion(client, token, TIMEOUT);
        }
    }

    if ((rc = MQTTClient_disconnect(client, 1000)) != MQTTCLIENT_SUCCESS)
        printf("Failed to disconnect, return code %d\n", rc);

    MQTTClient_destroy(&client);
    return rc;
}

int main(void) {
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
        // Poll for data ready statuc
        error = scd30_get_data_ready(&data_ready);
        if (error != 0) {
            printf("Error reading data ready status: %i\n", error);
            running = false;
            continue;
        }
        if (!data_ready) {
            continue;
        }
        error = scd30_read_measurement_data(&co2_concentration, &temperature, &humidity);
        if (error != 0) {
            printf("Error reading measurement data: %i\n", error);
            running = false;
            continue;
        }

        printf("Read: CO2=%.2f, Temp=%.2f, Hum=%.2f\n", co2_concentration, temperature, humidity);
        // Publish all measurements in one connection session
        publish_measurements(co2_concentration, temperature, humidity);
        // Sleep for 600 seconds (SCD30 default measurement interval is 2s)
        sleep(600);
    }

    // In unreachable code (infinite loop), but for completeness:
    error = scd30_stop_periodic_measurement();
    if (error != NO_ERROR) {
        return error;
    }
    return 0;
}
