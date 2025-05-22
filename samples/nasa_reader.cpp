#include <tcp_client.h>
#include <nasa_protocol.h>

#include <spdlog/spdlog.h>

int main(int argc, char **argv)
{
    nasacpp::TcpClient tcp_client("192.168.178.25", 26);
    nasacpp::NasaProtocol nasa_protocol;

    while (true)
    {
        nasa_protocol.insertDataRx(tcp_client.getRecvData());
        auto pkts = nasa_protocol.getPacketsRx();
        for (auto &pkt : pkts)
        {
            spdlog::info(to_string(pkt));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}