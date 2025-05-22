#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <vector>

namespace nasacpp
{
    class TcpClient
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

        bool connected();
        void forceReconnect();

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
        int socket_;

        // buffer
        std::mutex mtx_recv_data_;
        std::vector<uint8_t> recv_data_;
    };
} // namespace nasacpp
