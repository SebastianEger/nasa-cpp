// tcp_hass_bridge: bridges the NASA bus (through an RS485/TCP adapter) to MQTT with Home Assistant discovery.

#include <tcp_client.h>
#include <paho_mqtt_client.h>
#include <nasa_hass_bridge.h>
#include <nasa_bridge_runner.h>

#include <fstream>

using namespace nasacpp;

int main(int argc, char **argv)
{
    // logger
    std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_st("Main");

    // read main config file
    std::string config_filename = "/etc/nasa-cpp/cfg/tcp_hass_bridge.json";
    if (argc > 1)
    {
        config_filename = std::string(argv[1]);
    }
    logger->info("Loading config file {}", config_filename);
    std::ifstream f(config_filename);
    json config_main = json::parse(f);

    // mqtt connection
    PahoMqttClient mqtt(config_main["mqtt"]["address"], config_main["mqtt"]["port"], config_main["mqtt"]["client_id"]);

    // init NASA to Home Assistant bridge
    Address sa;
    sa.a_class = AddressClass::JIGTester;
    sa.channel = 0x00;
    sa.address = 0x00;

    NasaHassBridge nasa_hass_bridge(mqtt, sa, config_main);

    // init tcp client
    TcpClient tcp_client(config_main["tcp"]["address"], config_main["tcp"]["port"]);

    // run
    BridgeRunner runner(tcp_client, nasa_hass_bridge, BridgeOptions::fromConfig(config_main));
    runner.run();
}
