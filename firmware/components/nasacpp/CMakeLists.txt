# The portable part of the nasa-cpp library (../../../src), built as an ESP-IDF component.
# The Paho MQTT client is Linux only; the firmware uses its own EspMqttClient instead.
set(NASACPP_SRC ${CMAKE_CURRENT_LIST_DIR}/../../../src)

idf_component_register(
    SRCS
        ${NASACPP_SRC}/nasa_protocol.cpp
        ${NASACPP_SRC}/nasa_protocol_types.cpp
        ${NASACPP_SRC}/nasa_protocol_helper.cpp
        ${NASACPP_SRC}/nasa_msg_handler.cpp
        ${NASACPP_SRC}/nasa_hass_bridge.cpp
        ${NASACPP_SRC}/nasa_bridge_runner.cpp
        ${NASACPP_SRC}/tcp_client.cpp
    INCLUDE_DIRS ${NASACPP_SRC}
    REQUIRES lwip pthread)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    # header-only dependencies, same versions as the Linux build (see Dockerfile)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR do-not-add-subdirectory)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR do-not-add-subdirectory)
    FetchContent_MakeAvailable(nlohmann_json spdlog)

    target_include_directories(${COMPONENT_LIB} SYSTEM PUBLIC
        ${nlohmann_json_SOURCE_DIR}/single_include
        ${spdlog_SOURCE_DIR}/include)
    # pthread_self() asserts on FreeRTOS tasks that were not created through pthread (e.g. the main task)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC SPDLOG_NO_THREAD_ID)
endif()
