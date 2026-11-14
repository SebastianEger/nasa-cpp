#pragma once

#include <mqtt_interface.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <mqtt_client.h> // esp-mqtt
#include <nlohmann/json.hpp>

namespace nasacpp
{
    // MqttClient implementation on top of ESP-IDF's esp-mqtt.
    //
    // Subscriptions are remembered and repeated after every (re)connect. Messages published while
    // disconnected are dropped; retained messages (e.g. Home Assistant discovery) are kept by the broker.
    class EspMqttClient : public MqttClient
    {
    public:
        // Configuration keys: address, port, client_id, username, password.
        explicit EspMqttClient(const nlohmann::json &cfg);
        ~EspMqttClient() override;

        void publish(const std::string &topic, const std::string &payload, bool retained = false) override;
        void subscribe(const std::string &topic) override;
        std::vector<MqttMessage> getData() override;

        bool connected() const { return connected_; }

    private:
        static void onEvent(void *arg, esp_event_base_t base, int32_t event_id, void *event_data);
        void handleEvent(esp_mqtt_event_handle_t event);

        esp_mqtt_client_handle_t client_ = nullptr;
        std::string uri_, client_id_, username_, password_;

        std::atomic_bool connected_ = false;

        std::mutex mtx_;
        std::vector<std::string> topics_;

        std::mutex mtx_rx_;
        std::vector<MqttMessage> rx_;
        MqttMessage partial_;
    };
} // namespace nasacpp
