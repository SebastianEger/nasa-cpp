#pragma once
#include "mqtt_client.h"
#include "nasa_mqtt_handler.hpp"

#include <set>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace nasacpp
{
    class NasaHassBridge : public MqttClient
    {

    public:
        NasaHassBridge(const Address &sa, const json &cfg);
        ~NasaHassBridge();

    public:
        void publishToHass(const MessageSet &msg);
        void getNasaPackets(std::vector<Packet> &pkts);
        void loadMsgListRead(const std::string &filename);
        void loadMsgListRequest(const std::string &filename);
        void loadMsgListWrite(const std::string &filename);

    private:
        void addMsg(const json &cfg, const DataType &data_type);
        void loadMsgList(const std::string &filename, const DataType &data_type);
        static std::shared_ptr<NasaMqttHandler> createHandler(const std::string &type,
                                                              const std::string &topic_state,
                                                              const MessageNumber &msg_nr,
                                                              const Address &sa, const DataType &data_type,
                                                              const std::string &device_class = std::string());

        void loop();

    protected:
        std::mutex mtx_handler_;
        std::map<uint16_t, std::shared_ptr<NasaMqttHandler>> msgnr_handler_;
        std::map<std::string, std::shared_ptr<NasaMqttHandler>> topic_handler_;

        bool shutdown_ = false;
        std::thread loop_thread_;

        // NASA source address
        Address sa_;

        // Device name
        std::string device_name_;
    };
} // namespace nasacpp
