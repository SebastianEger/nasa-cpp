// nasa-cpp firmware: Samsung NASA bus <-> MQTT / Home Assistant bridge on an ESP32 (M5Stack Atom Lite).
//
// Uses the same library as the Linux application tcp_hass_bridge (../src): the message lists, the
// Home Assistant discovery and the bridge loop are identical; only MQTT, the bus connection and the
// network are ESP32 specific. Configuration and message lists are read from the SPIFFS partition.

#include "esp_mqtt_client.h"
#include "network.h"
#include "status_led.h"
#include "uart_transport.h"

#include <nasa_bridge_runner.h>
#include <nasa_hass_bridge.h>
#include <tcp_client.h>

#include <fstream>
#include <memory>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

using namespace nasacpp;

static const char *TAG = "nasa-cpp";
static constexpr const char *kConfigFile = "/spiffs/config.json";
static constexpr int kDefaultLedPin = 27;

[[noreturn]] static void fail(StatusLed &led, const char *message)
{
    ESP_LOGE(TAG, "%s", message);
    while (true)
    {
        led.update(StatusLed::State::Error);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static bool mountStorage()
{
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = "/spiffs";
    conf.partition_label = "storage";
    conf.max_files = 4;
    conf.format_if_mount_failed = false;
    return esp_vfs_spiffs_register(&conf) == ESP_OK;
}

static bool loadConfig(json &cfg)
{
    std::ifstream f(kConfigFile);
    if (!f.is_open())
    {
        return false;
    }
    cfg = json::parse(f, nullptr, false);
    return !cfg.is_discarded() && cfg.is_object();
}

static void logHeap(const char *stage)
{
    ESP_LOGI(TAG, "[heap] %s: free %u bytes, minimum %u bytes, largest block %u bytes", stage,
             (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // configuration
    json cfg;
    if (!mountStorage() || !loadConfig(cfg))
    {
        StatusLed led(kDefaultLedPin);
        fail(led, "Could not read " "/spiffs/config.json" ". Flash the storage partition (see firmware/README.md).");
    }
    StatusLed led(cfg.value("led_pin", kDefaultLedPin));
    logHeap("config loaded");

    // network
    networkStart(cfg.value("wifi", json::object()));
    while (!networkConnected())
    {
        led.update(StatusLed::State::NetworkWait);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // mqtt; the bridge announces all entities right away, so wait for the connection first
    EspMqttClient mqtt(cfg.value("mqtt", json::object()));
    while (!mqtt.connected())
    {
        led.update(StatusLed::State::MqttWait);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    Address sa;
    sa.a_class = AddressClass::JIGTester;
    sa.channel = 0x00;
    sa.address = 0x00;
    std::unique_ptr<NasaHassBridge> bridge;
    try
    {
        bridge = std::make_unique<NasaHassBridge>(mqtt, sa, cfg);
    }
    catch (const std::exception &e)
    {
        ESP_LOGE(TAG, "Invalid message list: %s", e.what());
        fail(led, "Could not load the message lists");
    }
    logHeap("message lists loaded");

    // connection to the NASA bus
    std::unique_ptr<ByteTransport> transport;
    const std::string transport_type = cfg.value("transport", std::string("uart"));
    if (transport_type == "tcp")
    {
        const json &tcp = cfg.value("tcp", json::object());
        ESP_LOGI(TAG, "Bus connection: TCP %s:%d", tcp.value("address", std::string()).c_str(), tcp.value("port", 26));
        transport = std::make_unique<TcpClient>(tcp.value("address", std::string()), tcp.value("port", 26));
    }
    else
    {
        transport = std::make_unique<UartTransport>(cfg.value("uart", json::object()));
    }

    BridgeOptions options = BridgeOptions::fromConfig(cfg);
    BridgeRunner runner(*transport, *bridge, options);
    ESP_LOGI(TAG, "Running%s", options.read_only ? " (read only)" : "");

    uint32_t last_rx = 0;
    uint32_t loops = 0;
    while (true)
    {
        runner.step();

        const uint32_t rx = runner.packetsReceived();
        StatusLed::State state;
        if (!networkConnected())
        {
            state = StatusLed::State::NetworkWait;
        }
        else if (!mqtt.connected())
        {
            state = StatusLed::State::MqttWait;
        }
        else if (runner.msSinceLastData() > options.timeout * 1000LL)
        {
            state = StatusLed::State::NoBusData;
        }
        else
        {
            state = StatusLed::State::Running;
        }
        led.update(state, rx != last_rx);
        last_rx = rx;

        if (++loops % 600 == 0) // every minute
        {
            ESP_LOGI(TAG, "packets received %u, sent %u", (unsigned)rx, (unsigned)runner.packetsSent());
            logHeap("running");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
