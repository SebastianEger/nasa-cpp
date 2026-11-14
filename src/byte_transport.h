#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace nasacpp
{
    // Raw byte connection to the NASA bus.
    // Implementations: TcpClient (RS485/TCP adapter), UartTransport (ESP32 firmware).
    class ByteTransport
    {
    public:
        virtual ~ByteTransport() = default;

        // Appends all bytes received since the last call to data and returns the number of new bytes.
        virtual int read(std::deque<uint8_t> &data) = 0;

        // Sends data and returns the number of bytes written, or a negative value on error.
        virtual int write(const std::vector<uint8_t> &data) = 0;

        virtual bool connected() = 0;

        // Drops the current connection and connects again (no-op where not applicable).
        virtual void reconnect() = 0;
    };
} // namespace nasacpp
