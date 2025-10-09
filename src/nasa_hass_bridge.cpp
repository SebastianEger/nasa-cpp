#include "nasa_hass_bridge.h"
#include <fstream>

namespace nasacpp
{
    NasaHassBridge::NasaHassBridge(const Address &sa, const json &cfg)
        : MqttClient(cfg["mqtt"]["address"], cfg["mqtt"]["port"], cfg["mqtt"]["client_id"]), sa_(sa)
    {
        NasaMqttHandler::disable_write_send = cfg["disable_write_send"];
        NasaMqttHandler::initial_read_out = cfg["initial_read_out"];

        device_name_ = "Samsung EHS";

        // load message lists
        if (cfg.contains("read_msg_list"))
        {
            loadMsgListRead(cfg["read_msg_list"]);
        }
        if (cfg.contains("request_msg_list"))
        {
            loadMsgListRequest(cfg["request_msg_list"]);
        }
        if (cfg.contains("write_msg_list"))
        {
            loadMsgListWrite(cfg["write_msg_list"]);
        }

        spdlog::info("NasaHassBridge: Initialized {} NASA messages", topic_handler_.size());
        loop_thread_ = std::thread(&NasaHassBridge::loop, this);
    }

    NasaHassBridge::~NasaHassBridge()
    {
        shutdown_ = true;
        loop_thread_.join();
    }

    void NasaHassBridge::publishToHass(const MessageSet &msg)
    {
        std::lock_guard<std::mutex> lock(mtx_handler_);
        if (!msgnr_handler_.contains((uint16_t)msg.message_number))
        {
            return;
        }
        auto &handler = msgnr_handler_[(uint16_t)msg.message_number];
        if (handler)
        {
            handler->updateValue(msg);
            publish(handler->getTopic(), handler->getMqttPayload(), handler->getDataType() == DataType::Write || handler->getDataType() == DataType::Request);
        }
    }

    void NasaHassBridge::getNasaPackets(std::vector<Packet> &pkts)
    {
        std::lock_guard<std::mutex> lock(mtx_handler_);
        for (auto &msg_hdl : msgnr_handler_)
        {
            // check if handler is not null
            if (msg_hdl.second)
            {
                msg_hdl.second->getPackets(pkts);
            }
        }
    }

    void NasaHassBridge::loop()
    {
        while (!shutdown_)
        {
            auto recv_msgs = MqttClient::getData();
            for (auto &msg : recv_msgs)
            {
                std::lock_guard<std::mutex> lock(mtx_handler_);
                auto &handler = topic_handler_[msg->get_topic()];
                if (handler)
                {
                    handler->updateValueNew(msg->get_payload());
                    handler->resetRetryCounter();
                }
                else
                {
                    spdlog::warn("NasMqttClient: No handler for {}", msg->get_topic());
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void NasaHassBridge::addMsg(const json &cfg, const DataType &data_type)
    {
        // name
        std::string name = cfg["name"]; // entity name

        // get message number
        uint16_t msg_nr = std::stol(std::string(cfg["message_number"]), nullptr, 16);

        json cfg_hass = cfg["hass"];
        std::string type = cfg_hass["type"]; // hass and handler type

        // init topic names
        std::string topic_name = device_name_ + " " + name;
        std::transform(topic_name.begin(), topic_name.end(), topic_name.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });
        std::replace(topic_name.begin(), topic_name.end(), ' ', '_');

        std::string topic_prefix = "homeassistant/" + type + "/" + topic_name;
        std::string topic_config = topic_prefix + "/config";
        std::string topic_state = topic_prefix + "/state";
        std::string topic_set = topic_prefix + "/set";

        // get device class
        std::string device_class;
        if (cfg_hass.contains("device_class"))
        {
            device_class = cfg_hass["device_class"];
        }

        // mqtt config payload
        json payload;

        // create handler
        std::lock_guard<std::mutex> lock(mtx_handler_);
        auto handler = createHandler(type, topic_state, (MessageNumber)msg_nr, sa_, data_type, device_class);
        if (handler && !msgnr_handler_.contains(msg_nr))
        {
            // init payload with hanlder initialization
            payload = handler->init(cfg_hass);

            // update maps
            msgnr_handler_[msg_nr] = std::move(handler);
            topic_handler_[topic_state] = msgnr_handler_[msg_nr];
        }
        else
        {
            spdlog::error("NasaHassBridge: Invalid handler type {0} or handler for this message number {1} already exists", type, (int)msg_nr);
            return;
        }

        // extend payload
        payload["device"] = {{"name", device_name_}, {"identifiers", "samsung_nasa_cpp"}, {"manufacturer", "Samsung"}, {"model", "Mono HQ Quiet"}};
        payload["unique_id"] = std::string(payload["device"]["identifiers"]) + "_" + topic_prefix.substr(topic_prefix.rfind("/") + 1);
        payload["name"] = cfg["name"];
        payload["state_topic"] = topic_state;

        // set command topic
        if (type == "switch" || type == "select" || type == "number")
        {
            payload["command_topic"] = topic_set;
        }

        // create and publish config message
        mqtt::message_ptr pubmsg = mqtt::make_message(topic_config, payload.dump());
        pubmsg->set_retained(true);
        pubmsg->set_qos(qos_);
        cli_->publish(pubmsg, nullptr, pub_listener_)->wait_for(timeout_);

        // init command topic callback
        if (payload.contains("command_topic"))
        {
            subscribe(topic_set);
            topic_handler_[topic_set] = msgnr_handler_[msg_nr];
        }

        // finished!
        spdlog::info("NasaHassBridge: Add topic {}", topic_config);
    }

    void NasaHassBridge::loadMsgList(const std::string &filename, const DataType &data_type)
    {
        // check if file exists
        if (!std::filesystem::exists(filename))
        {
            return;
        }

        // check if file is not empty
        std::ifstream f(filename, std::ios::binary | std::ios::ate);
        if (f.tellg())
        {
            f = std::ifstream(filename);
            json msg_list = json::parse(f);
            for (auto &msg_cfg : msg_list)
            {
                addMsg(msg_cfg, data_type);
            }
        }
    }

    void NasaHassBridge::loadMsgListRead(const std::string &filename)
    {
        spdlog::info("NasaHassBridge: Loading READ msg list {}", filename);
        loadMsgList(filename, DataType::Read);
    }

    void NasaHassBridge::loadMsgListRequest(const std::string &filename)
    {
        spdlog::info("NasaHassBridge: Loading REQUEST msg list {}", filename);
        loadMsgList(filename, DataType::Request);
    }

    void NasaHassBridge::loadMsgListWrite(const std::string &filename)
    {
        spdlog::info("NasaHassBridge: Loading WRITE msg list {}", filename);
        loadMsgList(filename, DataType::Write);
    }

    std::shared_ptr<NasaMqttHandler> NasaHassBridge::createHandler(const std::string &type,
                                                                   const std::string &topic_state,
                                                                   const MessageNumber &msg_nr,
                                                                   const Address &sa,
                                                                   const DataType &data_type,
                                                                   const std::string &device_class)
    {
        std::shared_ptr<NasaMqttHandler> handler = nullptr;
        if ((type == "sensor" || type == "number") && device_class != "enum")
        {
            handler = std::make_shared<NasaMqttHandler>(topic_state, msg_nr, sa, data_type);
        }
        else if (type == "switch" || type == "binary_sensor")
        {
            handler = std::make_shared<NasaMqttHandlerBinary>(topic_state, msg_nr, sa, data_type);
        }
        else if (type == "select" || device_class == "enum")
        {
            handler = std::make_shared<NasaMqttHandlerEnum>(topic_state, msg_nr, sa, data_type);
        }
        return handler;
    }
} // namespace nasacpp
