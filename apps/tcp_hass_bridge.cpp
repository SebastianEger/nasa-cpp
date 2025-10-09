#include <tcp_client.h>
#include <nasa_protocol.h>
#include <nasa_hass_bridge.h>

#include <fstream>

using namespace nasacpp;

int main(int argc, char **argv)
{
    // read main config file
    std::string config_filename = "/etc/nasa-cpp/tcp_hass_bridge.json";
    if (argc > 1)
    {
        config_filename = std::string(argv[1]);
    }
    spdlog::info("Main: Loading config file {}", config_filename);
    std::ifstream f(config_filename);
    json config_main = json::parse(f);

    int timeout = 30;
    if (config_main.contains("timeout"))
    {
        timeout = config_main["timeout"];
    }

    int buffer_limit = 10000;
    if (config_main.contains("buffer_limit"))
    {
        buffer_limit = config_main["buffer_limit"];
    }
    
    // init NASA to Home Assistant bridge
    Address sa;
    sa.a_class = AddressClass::JIGTester;
    sa.channel = 0x00;
    sa.address = 0x00;

    NasaHassBridge nasa_hass_bridge(sa, config_main);

    // init tcp client
    TcpClient tcp_client(config_main["tcp"]["address"], config_main["tcp"]["port"]);

    // init buffer
    std::deque<uint8_t> buffer;

    // loop
    std::chrono::time_point<std::chrono::system_clock> tp_last_data = std::chrono::system_clock::now();
    while (true)
    {
        if(buffer.size() > buffer_limit)
        {
            spdlog::warn("Main: Buffer limit reached, clear buffer");
            buffer.clear();
        }

        // get recv data and put it into buffer
        int n_new_bytes = tcp_client.getRecvData(buffer);
        if (n_new_bytes)
        {
            spdlog::info("Main: [RECV] {} bytes", n_new_bytes);
            tp_last_data = std::chrono::system_clock::now();
        }

        // decode packets and publish to mqtt
        auto pkts_rx = NasaProtocol::decode(buffer);
        if (!pkts_rx.empty())
        {
            spdlog::info("Main: [DECODE] {} NASA packets", pkts_rx.size());
            for (auto &pkt_rx : pkts_rx)
            {
                for (auto &msg : pkt_rx.messages)
                {
                    nasa_hass_bridge.publishToHass(msg);
                }
            }
        }

        // only send packet if pkts_tx is not empty, this makes sure no data is sent if the connection is lost
        if (!config_main["read_only"] && !pkts_rx.empty())
        {
            // progress tx
            std::vector<Packet> pkts_tx;
            nasa_hass_bridge.getNasaPackets(pkts_tx);
            if (!pkts_tx.empty())
            {
                spdlog::info("Main: [SEND] {} NASA packets ...", pkts_tx.size());
                tcp_client.send(NasaProtocol::encode(pkts_tx));
            }
        }

        // check connection timeout
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - tp_last_data).count() > timeout && tcp_client.connected())
        {
            spdlog::warn("Main: No data received since {} seconds, forcing TcpClient to reconnect!", timeout);
            tcp_client.forceReconnect();
            tp_last_data = std::chrono::system_clock::now();
        }

        // wait
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return 0;
}