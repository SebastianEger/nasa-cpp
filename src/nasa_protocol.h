#pragma once

#include "nasa_protocol_types.hpp"

#include <deque>
#include <assert.h>
#include <thread>
#include <mutex>

#include <spdlog/sinks/stdout_color_sinks-inl.h>

namespace nasacpp
{
    class NasaProtocol
    {
    public:
        NasaProtocol();
        ~NasaProtocol();

    public:
        void insertDataRx(const std::vector<uint8_t> &data);
        std::vector<Packet> getPacketsRx();

        static std::vector<uint8_t> encode(const std::vector<Packet> &pkts);
        static std::vector<Packet> decode(std::deque<uint8_t> &data);

    private:
        void loop();

    private:
        bool shutdown_ = false;
        std::thread loop_thread_;

        std::mutex mtx_data_in_;
        std::deque<uint8_t> data_in_;

        std::mutex mtx_data_processed_;
        std::vector<Packet> data_processed_;

        static std::shared_ptr<spdlog::logger> logger_;
    };
} // namespace nasacpp
