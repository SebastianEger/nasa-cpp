#include <tcp_client.h>
#include <spdlog/spdlog.h>

int main(int argc, char **argv)
{
    // init logger
    std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_st("Main");
    
    // init tcp client
    nasacpp::TcpClient tcp_client("192.168.178.25", 26);

    // start
    std::chrono::time_point<std::chrono::system_clock> tp_last_data = std::chrono::system_clock::now();
    while (true)
    {
        auto new_data = tcp_client.getRecvData();
        if (!new_data.empty())
        {
            tp_last_data = std::chrono::system_clock::now();
            logger->info("Recveived new data with size: {0}", new_data.size());
        }

        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - tp_last_data).count() > 10)
        {
            tcp_client.forceReconnect();
            tp_last_data = std::chrono::system_clock::now();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}