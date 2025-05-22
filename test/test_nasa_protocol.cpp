#include <gtest/gtest.h>
#include "../src/nasa_protocol.h"


TEST(NasaProtocol, CommandRead)
{
    std::vector<uint8_t> data;
    data.push_back(0xC0);
    data.push_back(0x11);

    Command command;
    command.decode(data, 0);

    ASSERT_EQ(command.packet_information, 1);
    ASSERT_EQ(command.data_type, DataType::Read);
    ASSERT_EQ(command.packet_type, PacketType::Normal);
    ASSERT_EQ(command.protocol_version, 2);
}

TEST(NasaProtocol, CommandWrite)
{
    std::vector<uint8_t> data;
    data.push_back(0xC0);
    data.push_back(0x12);

    Command command;
    command.decode(data, 0);

    ASSERT_EQ(command.packet_information, 1);
    ASSERT_EQ(command.data_type, DataType::Write);
    ASSERT_EQ(command.packet_type, PacketType::Normal);
    ASSERT_EQ(command.protocol_version, 2);
}

TEST(NasaProtocol, CommandRequest)
{
    std::vector<uint8_t> data;
    data.push_back(0xC0);
    data.push_back(0x13);

    Command command;
    command.decode(data, 0);

    ASSERT_EQ(command.packet_information, 1);
    ASSERT_EQ(command.data_type, DataType::Request);
    ASSERT_EQ(command.packet_type, PacketType::Normal);
    ASSERT_EQ(command.protocol_version, 2);
}

TEST(NasaProtocol, Address)
{
    Address a;

    a.a_class = AddressClass::WiredRemote;
    a.channel = 0x01;
    a.address = 0x02;

    ASSERT_EQ(a.to_uint32_t(), (uint32_t)0x00500102);
}