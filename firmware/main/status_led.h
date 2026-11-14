#pragma once

#include <cstdint>

#include <driver/rmt_tx.h>

namespace nasacpp
{
    // Single SK6812/WS2812 RGB LED (M5Stack Atom Lite: GPIO27) used as status indicator.
    class StatusLed
    {
    public:
        enum class State
        {
            Error,           // red, blinking: configuration missing or invalid
            NetworkWait,     // blue, blinking: connecting to Wi-Fi
            MqttWait,        // yellow: connecting to the MQTT broker
            Running,         // green: data from the heat pump, short flash on every received packet
            NoBusData,       // red: connected, but no data from the heat pump
        };

        explicit StatusLed(int gpio);

        // call periodically (e.g. every 100 ms); activity = packets were received since the last call
        void update(State state, bool activity = false);

    private:
        void set(uint8_t r, uint8_t g, uint8_t b);

        rmt_channel_handle_t channel_ = nullptr;
        rmt_encoder_handle_t encoder_ = nullptr;
        uint32_t tick_ = 0;
        uint32_t last_rgb_ = 0xFFFFFFFF;
    };
} // namespace nasacpp
