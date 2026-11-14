#include "paho_mqtt_client.h"

namespace nasacpp
{
    PahoMqttClient::PahoMqttClient(const std::string &address, const int port, const std::string &client_id) : sub_listener_("subscriber"), pub_listener_("publisher")
    {
        // create client
        std::string server_uri = "mqtt://" + address + ":" + std::to_string(port);
        cli_ = std::make_shared<mqtt::async_client>(server_uri, client_id);

        // set connection options
        conn_options_.set_clean_session(false);

        // create callback
        cb_ = std::make_unique<callback>(*cli_, conn_options_, logger_);

        // set callback
        cb_->on_connected_ = [this]()
        { resubscribe(); };
        cli_->set_callback(*cb_);

        // connect
        try
        {
            logger_->info("Connecting to the MQTT server '" + server_uri + "'...");
            auto conntok = cli_->connect(conn_options_, nullptr, *cb_);
            conntok->wait();
        }
        catch (const mqtt::exception &exc)
        {
            logger_->error("Unable to connect to MQTT server: '" + server_uri + "'" + exc.to_string());
        }
    }

    void PahoMqttClient::publish(const std::string &topic, const std::string &payload, bool retained)
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
            logger_->error("Unable to publish message, error: {}", e.what());
        }
    }

    void PahoMqttClient::subscribe(const std::string &topic)
    {
        {
            std::lock_guard<std::mutex> lock(mtx_topics_);
            topics_.push_back(topic);
        }
        try
        {
            if (cli_->is_connected())
            {
                cli_->subscribe(topic, qos_, nullptr, sub_listener_);
            }
        }
        catch (const std::exception &e)
        {
            logger_->error("Unable to subscribe to {}, error: {}", topic, e.what());
        }
    }

    void PahoMqttClient::resubscribe()
    {
        std::lock_guard<std::mutex> lock(mtx_topics_);
        for (const auto &topic : topics_)
        {
            try
            {
                cli_->subscribe(topic, qos_, nullptr, sub_listener_);
            }
            catch (const std::exception &e)
            {
                logger_->error("Unable to subscribe to {}, error: {}", topic, e.what());
            }
        }
    }

    std::vector<MqttMessage> PahoMqttClient::getData()
    {
        std::vector<mqtt::const_message_ptr> new_data;
        {
            std::lock_guard<std::mutex> lock(cb_->mtx_data);
            new_data = cb_->data;
            cb_->data.clear();
        }
        std::vector<MqttMessage> out;
        out.reserve(new_data.size());
        for (const auto &msg : new_data)
        {
            out.push_back({msg->get_topic(), msg->get_payload_str()});
        }
        return out;
    }
} // namespace nasacpp
