#pragma once

#include <stdint.h>
#include <string>

// Code from https://github.com/omerfaruk-aran/esphome_samsung_hvac_bus

namespace nasacpp
{
    template <template <typename> typename Container>
    uint16_t crc16(const Container<uint8_t> &data, int startIndex, int length)
    {
        uint16_t crc = 0;
        for (int index = startIndex; index < startIndex + length; ++index)
        {
            crc = crc ^ ((uint16_t)((uint8_t)data[index]) << 8);
            for (uint8_t i = 0; i < 8; i++)
            {
                if (crc & 0x8000)
                {
                    crc = (crc << 1) ^ 0x1021;
                }

                else
                {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }

    template <typename T>
    std::string to_hex_string(const T &number);

    template <typename T>
    std::string to_string(const T &t);
} // namespace nasacpp
