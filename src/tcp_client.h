#pragma once

#include "byte_transport.h"

#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks-inl.h>

namespace nasacpp
{
    // TCP connection to an RS485/TCP adapter. Receives on a background thread and reconnects
    // automatically.
    class TcpClient : public ByteTransport
    {
    public:
        TcpClient(const std::string &address, int port);
        ~TcpClient();

        template <template <typename> typename Container>
        int getRecvData(Container<uint8_t> &data)
        {
            std::lock_guard<std::mutex> lock(mtx_recv_data_);
            data.insert(data.end(), recv_data_.begin(), recv_data_.end());
            int size = recv_data_.size();
            recv_data_.clear();
            return size;
        }

        std::vector<uint8_t> getRecvData();
        int send(const std::vector<uint8_t> &data);

        void forceReconnect();

        // ByteTransport interface
        int read(std::deque<uint8_t> &data) override { return getRecvData(data); }
        int write(const std::vector<uint8_t> &data) override { return send(data); }
        bool connected() override;
        void reconnect() override { forceReconnect(); }

    private:
        bool connect2server();
        void loop();

    private:
        std::atomic_bool shutdown_ = false;
        std::atomic_bool connected_ = false;

        // recv thread
        std::thread loop_thrd_;

        // tcp connection
        std::string address_;
        int port_;
        std::mutex mtx_socket_;
        int socket_ = -1;

        // buffer
        std::mutex mtx_recv_data_;
        std::vector<uint8_t> recv_data_;

        // logger
        std::shared_ptr<spdlog::logger> logger_ = spdlog::stdout_color_st("TcpClient");
    };
} // namespace nasacpp
