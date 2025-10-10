#include "nasa_protocol_types.hpp"

namespace nasacpp
{
    // MESSAGESETTYPE
    MessageSetType getMessageSetType(const MessageNumber &msg_nr)
    {
        return (MessageSetType)(((uint32_t)msg_nr & 1536) >> 9);
    }

    // ADDRESS
    Address Address::parse(const std::string &str)
    {
        Address address;
        char *pEnd;
        address.a_class = (AddressClass)strtol(str.c_str(), &pEnd, 16);
        pEnd++; // .
        address.channel = strtol(pEnd, &pEnd, 16);
        pEnd++; // .
        address.address = strtol(pEnd, &pEnd, 16);
        return address;
    }

    void Address::decode(const std::vector<uint8_t> &data, unsigned int index)
    {
        a_class = (AddressClass)data[index];
        channel = data[index + 1];
        address = data[index + 2];
    }

    void Address::encode(std::vector<uint8_t> &data) const
    {
        data.push_back((uint8_t)a_class);
        data.push_back(channel);
        data.push_back(address);
    }

    uint32_t Address::to_uint32_t() const
    {
        return ((uint32_t)address) | ((uint32_t)channel << 8) | ((uint32_t)a_class << 16) | ((uint32_t)0x00 << 24);
    }

    // COMMAND
    void Command::decode(const std::vector<uint8_t> &data, unsigned int index)
    {
        packet_information = ((int)data[index] & 128) >> 7 == 1;
        protocol_version = (uint8_t)(((int)data[index] & 96) >> 5);
        retry_count = (uint8_t)(((int)data[index] & 24) >> 3);
        packet_type = (PacketType)(((int)data[index + 1] & 240) >> 4);
        data_type = (DataType)((int)data[index + 1] & 15);
        packet_number = data[index + 2];
    }
    void Command::encode(std::vector<uint8_t> &data) const
    {
        data.push_back((uint8_t)((((int)packet_information ? 1 : 0) << 7) + ((int)protocol_version << 5) + ((int)retry_count << 3)));
        data.push_back((uint8_t)(((int)packet_type << 4) + (int)data_type));
        data.push_back(packet_number);
    }

    // MESSAGESET
    MessageSet::MessageSet(MessageNumber message_number, long val)
    {
        this->message_number = message_number;
        this->value = val;
        this->type = getMessageSetType(message_number);
    }

    MessageSet MessageSet::decode(const std::vector<uint8_t> &data, unsigned int index, int capacity)
    {
        MessageSet set = MessageSet((MessageNumber)((uint32_t)data[index] * 256U + (uint32_t)data[index + 1]));
        switch (set.type)
        {
        case Enum:
            set.value = (int)data[index + 2];
            set.size = 3;
            break;
        case Variable:
            set.value = (int)data[index + 2] << 8 | (int)data[index + 3];
            set.size = 4;
            break;
        case LongVariable:
            set.value = (int)data[index + 2] << 24 | (int)data[index + 3] << 16 | (int)data[index + 4] << 8 | (int)data[index + 5];
            set.size = 6;
            break;

        case Structure:
            if (capacity != 1)
            {
                return set;
            }
            Buffer buffer;
            set.size = data.size() - index - 3; // 3=end bytes
            buffer.size = set.size - 2;
            for (int i = 0; i < buffer.size; i++)
            {
                buffer.data[i] = data[i];
            }
            set.structure = buffer;
            break;
        default:
            break;
        }
        return set;
    }

    void MessageSet::encode(std::vector<uint8_t> &data) const
    {
        uint16_t message_number = (uint16_t)this->message_number;
        data.push_back((uint8_t)((message_number >> 8) & 0xff));
        data.push_back((uint8_t)(message_number & 0xff));

        switch (type)
        {
        case Enum:
            data.push_back((uint8_t)value);
            break;
        case Variable:
            data.push_back((uint8_t)(value >> 8) & 0xff);
            data.push_back((uint8_t)(value & 0xff));
            break;
        case LongVariable:
            data.push_back((uint8_t)((value & 0xff000000) >> 24));
            data.push_back((uint8_t)((value & 0x00ff0000) >> 16));
            data.push_back((uint8_t)((value & 0x0000ff00) >> 8));
            data.push_back((uint8_t)(value & 0x000000ff));
            break;
        case Structure:
            for (int i = 0; i < structure.size; i++)
            {
                data.push_back(structure.data[i]);
            }
            break;
        default:
            break;
        }
    }

    // PACKET
    std::atomic<uint8_t> Packet::pkt_counter = 0;
    Packet Packet::create(Address sa, Address da, DataType data_type, MessageNumber message_number, int value)
    {
        Packet packet = createa_partial(sa, da, data_type);
        MessageSet message(message_number);
        message.value = value;
        packet.messages.push_back(message);

        return packet;
    }

    Packet Packet::createToBroadcastSelf(Address sa, DataType data_type, MessageNumber message_number, int value)
    {
        Address da;
        da.a_class = AddressClass::BroadcastSelfLayer;
        da.channel = 0xFF;
        da.address = 0x20;

        Packet packet = createa_partial(sa, da, data_type);
        MessageSet message(message_number);
        message.value = value;
        packet.messages.push_back(message);

        return packet;
    }

    Packet Packet::createa_partial(Address sa, Address da, DataType data_type)
    {
        Packet packet;
        packet.sa = sa;
        packet.da = da;
        packet.command.packet_information = true;
        packet.command.packet_type = PacketType::Normal;
        packet.command.data_type = data_type;
        packet.command.packet_number = pkt_counter++;
        return packet;
    }

    DecodeResult Packet::decode(const std::vector<uint8_t> &data)
    {
        if (data[0] != 0x32)
        {
            return DecodeResult::InvalidStartByte;
        }

        if (data.size() < 16 || data.size() > 1500)
        {
            return DecodeResult::UnexpectedSize;
        }

        int size = (int)data[1] << 8 | (int)data[2];
        if (size + 2 != data.size())
        {
            return DecodeResult::SizeDidNotMatch;
        }

        if (data[data.size() - 1] != 0x34)
        {
            return DecodeResult::InvalidEndByte;
        }

        uint16_t crc_actual = crc16(data, 3, size - 4);
        uint16_t crc_expected = (int)data[data.size() - 3] << 8 | (int)data[data.size() - 2];
        if (crc_expected != crc_actual)
        {
            return DecodeResult::CrcError;
        }

        unsigned int cursor = 3;

        sa.decode(data, cursor);
        cursor += sa.size;

        da.decode(data, cursor);
        cursor += da.size;

        command.decode(data, cursor);
        cursor += command.size;

        int capacity = (int)data[cursor];
        cursor++;

        messages.clear();
        for (int i = 1; i <= capacity; ++i)
        {
            MessageSet set = MessageSet::decode(data, cursor, capacity);
            messages.push_back(set);
            cursor += set.size;
        }

        return DecodeResult::Ok;
    }

    std::vector<uint8_t> Packet::encode() const
    {
        std::vector<uint8_t> data;

        data.push_back(0x32);
        data.push_back(0); // size
        data.push_back(0); // size
        sa.encode(data);
        da.encode(data);
        command.encode(data);

        data.push_back((uint8_t)messages.size());
        for (int i = 0; i < messages.size(); i++)
        {
            messages[i].encode(data);
        }

        int endPosition = data.size() + 1;
        data[1] = (uint8_t)(endPosition >> 8);
        data[2] = (uint8_t)(endPosition & (int)0xFF);

        uint16_t checksum = crc16(data, 3, endPosition - 4);
        data.push_back((uint8_t)((unsigned int)checksum >> 8));
        data.push_back((uint8_t)((unsigned int)checksum & (unsigned int)0xFF));
        data.push_back(0x34);
        return data;
    }
} // namespace nasacpp
