#include "mqtt_client.h"
#include <spdlog/spdlog.h>

namespace nasacpp
{
    MqttClient::MqttClient(const std::string &address, const int port, const std::string &client_id) : sub_listener_("subscriber"), pub_listener_("publisher")
    {
        // create client
        std::string server_uri = "mqtt://" + address + ":" + std::to_string(port);
        cli_ = std::make_shared<mqtt::async_client>(server_uri, client_id);

        // set connection options
        conn_options_.set_clean_session(false);

        // create callback
        cb_ = std::make_unique<callback>(*cli_, conn_options_);

        // set callback
        cli_->set_callback(*cb_);

        // connect
        try
        {
            spdlog::info("MqttClient: Connecting to the MQTT server '" + server_uri + "'...");
            auto conntok = cli_->connect(conn_options_, nullptr, *cb_);
            conntok->wait();
        }
        catch (const mqtt::exception &exc)
        {
            spdlog::error("MqttClient: Unable to connect to MQTT server: '" + server_uri + "'" + exc.to_string());
        }
    }

    void MqttClient::publish(const std::string &topic, const std::string &payload, bool retained)
    {
        try
        {

            mqtt::message_ptr pubmsg = mqtt::make_message(topic, payload);
            pubmsg->set_qos(qos_);
            pubmsg->set_retained(retained);
            cli_->publish(pubmsg, nullptr, pub_listener_)->wait_for(timeout_);
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            spdlog::error("MqttClient: Unable to publish message, error: {}", e.what());
        }
    }

    void MqttClient::subscribe(const std::string &topic)
    {
        cli_->subscribe(topic, qos_, nullptr, sub_listener_);
    }

    std::vector<mqtt::const_message_ptr> MqttClient::getData()
    {
        std::vector<mqtt::const_message_ptr> new_data;
        {
            std::lock_guard<std::mutex> lock(cb_->mtx_data);
            new_data = cb_->data;
            cb_->data.clear();
        }
        return new_data;
    }
} // namespace nasacpp
