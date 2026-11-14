#pragma once

#include <byte_transport.h>

#include <driver/uart.h>
#include <nlohmann/json.hpp>

namespace nasacpp
{
    // Direct connection to the NASA bus through an RS485 transceiver on a UART,
    // e.g. the M5Stack Atomic RS485 Base (TX GPIO22, RX GPIO19, automatic direction control).
    // NASA uses 9600 baud, 8 data bits, even parity, 1 stop bit.
    class UartTransport : public ByteTransport
    {
    public:
        // Configuration keys: tx_pin, rx_pin, de_pin (-1 = automatic direction control), baud_rate.
        explicit UartTransport(const nlohmann::json &cfg, uart_port_t port = UART_NUM_1);
        ~UartTransport() override;

        int read(std::deque<uint8_t> &data) override;
        int write(const std::vector<uint8_t> &data) override;
        bool connected() override { return true; }
        void reconnect() override;

    private:
        uart_port_t port_;
    };
} // namespace nasacpp
