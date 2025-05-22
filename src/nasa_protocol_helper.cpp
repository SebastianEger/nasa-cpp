#include "nasa_protocol_helper.hpp"
#include "nasa_protocol_types.hpp"
#include <vector>

namespace nasacpp
{
    template <>
    std::string to_hex_string(const long &number)
    {
        char str[10];
        snprintf(str, sizeof(str), "%02lx", number); // Use of snprintf for security reasons.
        return std::string(str);
    }

    template <>
    std::string to_hex_string(const MessageNumber &number)
    {
        return to_hex_string((long) number);
    }

    template <>
    std::string to_hex_string(const std::vector<uint8_t> &data)
    {
        std::string str;
        str.reserve(data.size() * 2); // Memory reservations are made to increase efficiency.
        for (uint8_t byte : data)
        {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", byte);
            str += buf;
        }
        return str;
    }

    template <>
    std::string to_string(const DecodeResult &t)
    {
        switch (t)
        {
        case DecodeResult::Ok:
            return "Ok";
        case DecodeResult::InvalidStartByte:
            return "InvalidStartByte";
        case DecodeResult::InvalidEndByte:
            return "InvalidEndByte";
        case DecodeResult::SizeDidNotMatch:
            return "SizeDidNotMatch";
        case DecodeResult::UnexpectedSize:
            return "UnexpectedSize";
        case DecodeResult::CrcError:
            return "CrcError";
        default:
            return std::string();
        }
    }

    template <>
    std::string to_string(const Address &a)
    {
        char str[9];
        sprintf(str, "%02x.%02x.%02x", (uint8_t)a.a_class, (uint8_t)a.channel, (uint8_t)a.address);
        return std::string(str);
    }

    template <>
    std::string to_string(const Command &c)
    {
        std::string str;
        str += "{";
        str += "PacketInformation: " + std::to_string(c.packet_information) + ";";
        str += "ProtocolVersion: " + std::to_string(c.protocol_version) + ";";
        str += "RetryCount: " + std::to_string(c.retry_count) + ";";
        str += "PacketType: " + std::to_string((int)c.packet_type) + ";";
        str += "DataType: " + std::to_string((int)c.data_type) + ";";
        str += "PacketNumber: " + std::to_string(c.packet_number);
        str += "}";
        return str;
    }

    template <>
    std::string to_string(const MessageSet &ms)
    {
        switch (ms.type)
        {
        case Enum:
            return "Enum " + to_hex_string((long)ms.message_number) + " = " + to_hex_string(ms.value);
        case Variable:
            return "Variable " + to_hex_string((long)ms.message_number) + " = " + to_hex_string(ms.value);
        case LongVariable:
            return "LongVariable " + to_hex_string((long)ms.message_number) + " = " + to_hex_string(ms.value);
        case Structure:
            return "Structure #" + to_hex_string((long)ms.message_number) + " = " + std::to_string(ms.structure.size);
        default:
            return "Unknown";
        }
    }

    template <>
    std::string to_string(const Packet &pkt)
    {
        std::string str;
        str += "#Packet Src:" + to_string(pkt.sa) + " Dst:" + to_string(pkt.da) + " " + to_string(pkt.command) + "\n";

        for (int i = 0; i < pkt.messages.size(); i++)
        {
            if (i > 0)
            {
                str += "\n";
            }
            str += " > " + to_string(pkt.messages[i]);
        }

        return str;
    }
} // namespace nasacpp
