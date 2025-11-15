#include <tcp_client.h>
#include <nasa_protocol.h>
#include <nasa_msg_handler.h>

#include <stdint.h>
#include <fstream>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

using namespace nasacpp;

struct MsgInfo
{
    MessageNumber nr;
    std::string name;
    std::string value_template;
};

int main(int argc, char **argv)
{
    // read main config file
    std::string config_filename = "/etc/nasa-cpp/cfg/tcp_fsv_list.json";

    // check argument
    if (argc > 1)
    {
        config_filename = std::string(argv[1]);
    }

    // load config file
    spdlog::info("Main: Loading config file {}", config_filename);
    std::ifstream f(config_filename);
    json config_main = json::parse(f);

    // init NASA to Home Assistant bridge
    Address sa;
    sa.a_class = AddressClass::JIGTester;
    sa.channel = 0x00;
    sa.address = 0x00;

    std::vector<MsgInfo> msg_infos;
    std::unordered_map<MessageNumber, std::unique_ptr<NasaMsgHandler>> msg_handlers;

    auto f_fsv_list = std::ifstream(config_main["fsv_list"]);
    json fsv_list = json::parse(f_fsv_list);
    for (auto &fsv_cfg : fsv_list)
    {
        MessageNumber msg_nr = (MessageNumber)std::stol(std::string(fsv_cfg["message_number"]), nullptr, 16);
        msg_handlers[msg_nr] = std::make_unique<NasaMsgHandler>(msg_nr, sa, DataType::Read);

        std::string value_template;
        if (fsv_cfg.contains("hass"))
        {
            if (fsv_cfg["hass"].contains("value_template"))
            {
                value_template = fsv_cfg["hass"]["value_template"];
            }
        }

        msg_infos.emplace_back(msg_nr, fsv_cfg["name"], value_template);
    }
    spdlog::info("Main: Init {} NASA messages", msg_handlers.size());

    // init tcp client
    TcpClient tcp_client(config_main["tcp"]["address"], config_main["tcp"]["port"]);

    // init buffer
    std::deque<uint8_t> buffer;
    buffer.resize(10000);

    // start loop
    bool have_all_values = false;
    while (!have_all_values)
    {
        // get recv data and put it into buffer
        int n_new_bytes = tcp_client.getRecvData(buffer);
        if (n_new_bytes)
        {
            spdlog::info("Main: [RECV] {} bytes", n_new_bytes);
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
                    if (msg_handlers.contains(msg.message_number))
                    {
                        msg_handlers[msg.message_number]->updateValue(msg);
                    }
                }
            }
        }

        // progress tx
        have_all_values = true;
        std::vector<Packet> pkts_tx;
        for (auto &msg_handler : msg_handlers)
        {
            if (!msg_handler.second->hasVal())
            {
                have_all_values = false;
                msg_handler.second->getPacket(pkts_tx);
            }
        }
        if (!pkts_tx.empty() && !pkts_rx.empty())
        {
            spdlog::info("Main: [SEND] {} NASA packets ...", pkts_tx.size());
            tcp_client.send(NasaProtocol::encode(pkts_tx));
        }

        // wait
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // open output file
    std::string output_filename = "fsv_list.csv";
    spdlog::info("Main: Writing values to {}", output_filename);
    std::ofstream output_file(output_filename);

    // header
    output_file << "Name,Number,Value,ValueTemplate\n";

    // values
    for (const auto &msg_info : msg_infos)
    {
        output_file << msg_info.name << "," + to_hex_string(msg_info.nr)
                    << ","
                    << std::to_string(msg_handlers[msg_info.nr]->getVal())
                    << "," << msg_info.value_template << std::endl;
    }

    // close
    output_file.close();
    return 0;
}