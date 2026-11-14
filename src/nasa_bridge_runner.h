#pragma once

#include "byte_transport.h"
#include "nasa_hass_bridge.h"

#include <atomic>
#include <chrono>
#include <deque>

namespace nasacpp
{
    struct BridgeOptions
    {
        // seconds without received data before the transport is reconnected
        int timeout = 30;
        // receive buffer size in bytes; the buffer is cleared when it is exceeded
        size_t buffer_limit = 10000;
        // never send anything to the heat pump
        bool read_only = false;

        static BridgeOptions fromConfig(const json &cfg);
    };

    // Moves data between the NASA bus (ByteTransport) and MQTT (NasaHassBridge).
    // Used by tcp_hass_bridge on Linux and by the ESP32 firmware.
    class BridgeRunner
    {
    public:
        BridgeRunner(ByteTransport &transport, NasaHassBridge &bridge, const BridgeOptions &options);

        // one iteration: read, decode, publish, send pending commands, check timeout
        void step();

        // calls step() forever
        [[noreturn]] void run(std::chrono::milliseconds period = std::chrono::milliseconds(200));

        // statistics, e.g. for a status LED
        uint32_t packetsReceived() const { return packets_rx_; }
        uint32_t packetsSent() const { return packets_tx_; }
        // milliseconds since the last byte was received
        int64_t msSinceLastData() const;

    private:
        ByteTransport &transport_;
        NasaHassBridge &bridge_;
        BridgeOptions options_;

        std::deque<uint8_t> buffer_;
        std::chrono::steady_clock::time_point tp_last_data_;
        std::atomic<int64_t> last_data_ms_ = 0;

        std::atomic<uint32_t> packets_rx_ = 0;
        std::atomic<uint32_t> packets_tx_ = 0;

        std::shared_ptr<spdlog::logger> logger_ = spdlog::stdout_color_st("BridgeRunner");
    };
} // namespace nasacpp
