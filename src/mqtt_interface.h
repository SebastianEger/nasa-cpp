#pragma once

#include <string>
#include <vector>

namespace nasacpp
{
    // A received MQTT message.
    struct MqttMessage
    {
        std::string topic;
        std::string payload;
    };

    // Platform independent MQTT client interface.
    // Implementations: PahoMqttClient (Linux), EspMqttClient (ESP32 firmware).
    class MqttClient
    {
    public:
        virtual ~MqttClient() = default;

        virtual void publish(const std::string &topic, const std::string &payload, bool retained = false) = 0;
        virtual void subscribe(const std::string &topic) = 0;

        // Returns and clears all messages received since the last call.
        virtual std::vector<MqttMessage> getData() = 0;
    };
} // namespace nasacpp
