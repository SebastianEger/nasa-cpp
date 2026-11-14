#include "status_led.h"

#include <esp_log.h>

namespace nasacpp
{
    static const char *TAG = "led";

    // 10 MHz resolution: 1 tick = 0.1 us
    static constexpr uint32_t kResolutionHz = 10 * 1000 * 1000;
    // the LED is very bright, keep it subtle
    static constexpr uint8_t kBrightness = 40;

    StatusLed::StatusLed(int gpio)
    {
        if (gpio < 0)
        {
            return;
        }

        rmt_tx_channel_config_t channel_cfg = {};
        channel_cfg.gpio_num = (gpio_num_t)gpio;
        channel_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
        channel_cfg.resolution_hz = kResolutionHz;
        channel_cfg.mem_block_symbols = 64;
        channel_cfg.trans_queue_depth = 4;
        if (rmt_new_tx_channel(&channel_cfg, &channel_) != ESP_OK)
        {
            ESP_LOGW(TAG, "No RMT channel for the status LED");
            channel_ = nullptr;
            return;
        }

        // SK6812 timing: 0 = 0.3 us high / 0.9 us low, 1 = 0.6 us high / 0.6 us low, MSB first
        rmt_bytes_encoder_config_t encoder_cfg = {};
        encoder_cfg.bit0 = {.duration0 = 3, .level0 = 1, .duration1 = 9, .level1 = 0};
        encoder_cfg.bit1 = {.duration0 = 6, .level0 = 1, .duration1 = 6, .level1 = 0};
        encoder_cfg.flags.msb_first = 1;
        ESP_ERROR_CHECK(rmt_new_bytes_encoder(&encoder_cfg, &encoder_));
        ESP_ERROR_CHECK(rmt_enable(channel_));
        set(0, 0, 0);
    }

    void StatusLed::set(uint8_t r, uint8_t g, uint8_t b)
    {
        if (!channel_)
        {
            return;
        }
        uint32_t rgb = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
        if (rgb == last_rgb_)
        {
            return;
        }
        last_rgb_ = rgb;

        auto scale = [](uint8_t v)
        { return (uint8_t)((uint16_t)v * kBrightness / 255); };
        // the LED expects GRB order
        static uint8_t grb[3];
        grb[0] = scale(g);
        grb[1] = scale(r);
        grb[2] = scale(b);
        rmt_transmit_config_t tx_cfg = {};
        rmt_transmit(channel_, encoder_, grb, sizeof(grb), &tx_cfg);
        rmt_tx_wait_all_done(channel_, 100);
    }

    void StatusLed::update(State state, bool activity)
    {
        ++tick_;
        const bool blink_on = (tick_ / 5) % 2 == 0; // ~1 Hz with a 100 ms update period
        switch (state)
        {
        case State::Error:
            blink_on ? set(255, 0, 0) : set(0, 0, 0);
            break;
        case State::NetworkWait:
            blink_on ? set(0, 0, 255) : set(0, 0, 0);
            break;
        case State::MqttWait:
            set(255, 160, 0);
            break;
        case State::Running:
            activity ? set(0, 255, 0) : set(0, 60, 0);
            break;
        case State::NoBusData:
            set(255, 0, 0);
            break;
        }
    }
} // namespace nasacpp
