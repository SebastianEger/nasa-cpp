#include "nasa_protocol.h"

#include <iostream>
#include <spdlog/spdlog.h>

namespace nasacpp
{
    NasaProtocol::NasaProtocol()
    {
        // create loop thread
        loop_thread_ = std::thread(&NasaProtocol::loop, this);
    }

    NasaProtocol::~NasaProtocol()
    {
        // request shutdown and wait for thread
        shutdown_ = true;
        loop_thread_.join();
    }

    void NasaProtocol::insertDataRx(const std::vector<uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(mtx_data_in_);
        data_in_.insert(data_in_.end(), data.begin(), data.end());
    }

    void NasaProtocol::loop()
    {
        while (!shutdown_)
        {
            std::vector<Packet> new_packets;
            {
                std::unique_lock<std::mutex> lock(mtx_data_in_);
                new_packets = decode(data_in_);
            }
            {
                std::lock_guard<std::mutex> lock(mtx_data_processed_);
                data_processed_.insert(data_processed_.end(), new_packets.begin(), new_packets.end());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::vector<Packet> NasaProtocol::getPacketsRx()
    {
        std::lock_guard<std::mutex> lock(mtx_data_processed_);
        auto data_copy = data_processed_;
        data_processed_.clear();
        return data_copy;
    }

    std::vector<uint8_t> NasaProtocol::encode(const std::vector<Packet> &pkts)
    {
        std::vector<uint8_t> data_out;
        for (auto &pkt : pkts)
        {
            auto pkt_data = pkt.encode();
            data_out.insert(data_out.end(), pkt_data.begin(), pkt_data.end());
        }
        return data_out;
    }

    std::vector<Packet> NasaProtocol::decode(std::deque<uint8_t> &data)
    {
        std::vector<Packet> pkts_out;
        while (data.size() > 16)
        {
            // check first byte
            if (data[0] != 0x32)
            {
                // remove first byte from buffer, iterate till new first byte is found
                data.pop_front();
                continue;
            }

            // check expected packet size
            int expected_size = (int)data[1] << 8 | (int)data[2];
            if (expected_size > 1500 || expected_size < 16)
            {
                // remove first byte from buffer, iterate till new first byte is found
                data.pop_front();
                continue;
            }

            // check buffer size
            if (data.size() < expected_size + 2)
            {
                // wait for more data
                return pkts_out;
            }

            // create packet
            Packet new_packet;
            auto decode_result = new_packet.decode({data.begin(), data.begin() + expected_size + 2});
            if (decode_result == DecodeResult::Ok)
            {
                switch (new_packet.command.data_type)
                {
                case DataType::Ack:
                    break;

                case DataType::Nack:
                    break;

                default:
                    pkts_out.push_back(new_packet);
                    break;
                }
            }
            else
            {
                spdlog::warn("NasaProtocol: [DECODE] result: {}", to_string(decode_result));
            }

            // remove packet from buffer
            data.erase(data.begin(), data.begin() + expected_size + 2);
        }
        return pkts_out;
    }
} // namespace nasacpp
