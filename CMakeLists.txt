cmake_minimum_required(VERSION 3.10)
project(scd30_mqtt_driver)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Paho MQTT C library
set(PAHO_BUILD_STATIC TRUE CACHE BOOL "Build static library" FORCE)
set(PAHO_BUILD_SHARED FALSE CACHE BOOL "Build shared library" FORCE)
set(PAHO_BUILD_DOCUMENTATION FALSE CACHE BOOL "Build documentation" FORCE)
set(PAHO_BUILD_SAMPLES FALSE CACHE BOOL "Build sample programs" FORCE)
add_subdirectory(pahomqtt)

# Include directories for SCD30 driver
include_directories(pi-scd30)

# SCD30 Driver Source Files
set(SCD30_SOURCES
    pi-scd30/scd30_i2c.c
    pi-scd30/sensirion_common.c
    pi-scd30/sensirion_i2c.c
    pi-scd30/sensirion_i2c_hal.c
)

# Create executable
add_executable(scd30_mqtt main.cpp env_loader.cpp ${SCD30_SOURCES})

# Link Paho MQTT libraries (paho-mqtt3c is the synchronous C client)
# Also link pthread as it is often required by Paho on Linux
target_link_libraries(scd30_mqtt PRIVATE paho-mqtt3c)

# If on Linux (likely for Raspberry Pi), link i2c if needed, though header based access might not need explicit link if using dev files
# but usually for raw i2c access via devices on linux no special lib needed beyond standard headers.
# However, Paho usually needs network libs.
if(UNIX)
    target_link_libraries(scd30_mqtt PRIVATE pthread)
endif()
