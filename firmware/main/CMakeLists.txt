idf_component_register(
    SRCS main.cpp esp_mqtt_client.cpp uart_transport.cpp status_led.cpp network.cpp
    INCLUDE_DIRS .
    REQUIRES nasacpp mqtt esp_wifi esp_eth esp_netif esp_event nvs_flash spiffs esp_driver_uart esp_driver_rmt)

# SPIFFS image with the configuration and the message lists
# - config.json: firmware/config.json (falls back to config.example.json)
# - message lists: ../msg/<model>, another directory: -D NASA_MSG_DIR=<dir>
if(NOT NASA_MSG_DIR)
    set(NASA_MSG_DIR ${PROJECT_DIR}/../msg/AE08BXYDGG-MIM03EN)
endif()
# - another config file: -D NASA_CONFIG_FILE=<path>
if(NASA_CONFIG_FILE)
    set(NASA_CONFIG ${NASA_CONFIG_FILE})
else()
    set(NASA_CONFIG ${PROJECT_DIR}/config.json)
endif()
if(NOT EXISTS ${NASA_CONFIG})
    message(WARNING "firmware/config.json not found, using config.example.json. Copy it to config.json and enter your Wi-Fi and MQTT settings.")
    set(NASA_CONFIG ${PROJECT_DIR}/config.example.json)
endif()

set(spiffs_dir ${CMAKE_BINARY_DIR}/spiffs_image)
configure_file(${NASA_CONFIG} ${spiffs_dir}/config.json COPYONLY)
foreach(list sensor_msg_list.json control_msg_list.json fsv_msg_list.json)
    if(EXISTS ${NASA_MSG_DIR}/${list})
        configure_file(${NASA_MSG_DIR}/${list} ${spiffs_dir}/${list} COPYONLY)
    endif()
endforeach()
spiffs_create_partition_image(storage ${spiffs_dir} FLASH_IN_PROJECT)
