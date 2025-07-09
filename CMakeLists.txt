cmake_minimum_required(VERSION 3.12...3.20)
project(nasa-cpp)
set(CMAKE_CXX_STANDARD 20)

# CMake
include(cmake/add.cmake)

# Options
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_SAMPLES "Build samples" OFF)

# Depdencies
find_package(PahoMqttCpp REQUIRED)
find_package(nlohmann_json 3.12.0 REQUIRED)

# Lib
file(GLOB SRC src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SRC})
target_link_libraries(${PROJECT_NAME} PUBLIC PahoMqttCpp::paho-mqttpp3)
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
list(APPEND INSTALL_TARGETS ${PROJECT_NAME})

# Apps
add_app(tcp_hass_bridge)
add_app(tcp_fsv_list)

# Samples
if(BUILD_SAMPLES)
  add_sample(tcp_reader)
  add_sample(nasa_reader)
  add_sample(mqtt_pub_sub)
  add_sample(nasa_mqtt_reader)
  add_sample(nasa_send_dhw_on)
endif()

# Install
list(APPEND INSTALL_TARGETS tcp_hass_bridge)
list(APPEND INSTALL_TARGETS tcp_fsv_list)
install(TARGETS
          ${INSTALL_TARGETS}
        RUNTIME           # Following options apply to runtime artifacts.
          COMPONENT Runtime
        LIBRARY           # Following options apply to library artifacts.
          COMPONENT Runtime
          NAMELINK_COMPONENT Development
        ARCHIVE           # Following options apply to archive artifacts.
          COMPONENT Development
          DESTINATION lib/static
        FILE_SET HEADERS  # Following options apply to file set HEADERS.
          COMPONENT Development
        )
install(DIRECTORY 
          cfg/
        DESTINATION
          /etc/nasa-cpp
        COMPONENT 
          config       
        )

# Tests
if(BUILD_TESTS)
    # GOOGLETEST
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
        )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    include(GoogleTest)
    add_test(nasa_protocol)
endif()
