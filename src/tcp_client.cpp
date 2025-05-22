#include "tcp_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

namespace nasacpp
{
    TcpClient::TcpClient(const std::string &address, int port) : address_(address), port_(port)
    {
        // create loop thread
        loop_thrd_ = std::thread(&TcpClient::loop, this);

        // wait till connection is ready
        while (!connected_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    TcpClient::~TcpClient()
    {
        // request shutdown and wait for thread
        shutdown_ = true;
        loop_thrd_.join();
    }

    bool TcpClient::connect2server()
    {
        close(socket_);
        socket_ = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port_);

        if (inet_pton(AF_INET, address_.c_str(), &serv_addr.sin_addr) <= 0)
        {
            spdlog::error("TcpClient: Invalid address/ Address not supported");
            return false;
        }

        spdlog::info("TcpClient: Trying to connect to server ...");
        if (connect(socket_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            spdlog::warn("TcpClient: Connection Failed");
            return false;
        }
        spdlog::info("TcpClient: Connected!");

        // test if the socket is in non-blocking mode
        if (fcntl(socket_, F_GETFL) & O_NONBLOCK)
        {
            // socket is non blocking
            return true;
        }

        // put the socket in non-blocking mode
        if (fcntl(socket_, F_SETFL, fcntl(socket_, F_GETFL) | O_NONBLOCK) < 0)
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    void TcpClient::forceReconnect()
    {
        connected_ = false;
    }

    bool TcpClient::connected()
    {
        return connected_;
    }

    void TcpClient::loop()
    {
        while (!shutdown_)
        {
            connected_ = connect2server();
            while (!shutdown_ && connected_)
            {
                uint8_t buffer[1024] = {0};
                int n_read = 0;
                {
                    try
                    {
                        n_read = recv(socket_, buffer, sizeof(buffer), 0);
                    }
                    catch (const std::exception &e)
                    {
                        spdlog::error("TcpClient: Could not read socket, what: {}", e.what());
                    }
                }

                if (n_read)
                {
                    std::lock_guard<std::mutex> lock(mtx_recv_data_);
                    for (int i = 0; i < n_read; ++i)
                    {
                        recv_data_.push_back(buffer[i]);
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // wait for next reconnect attempt
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::vector<uint8_t> TcpClient::getRecvData()
    {
        std::lock_guard<std::mutex> lock(mtx_recv_data_);
        auto data_out = recv_data_;
        recv_data_.clear();
        return data_out;
    }

    int TcpClient::send(const std::vector<uint8_t> &data)
    {
        std::lock_guard<std::mutex> lock(mtx_socket_);
        return ::send(socket_, (const char *)data.data(), data.size(), 0);
    }
} // namespace nasacpp
