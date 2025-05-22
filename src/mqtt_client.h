#pragma once

#include "mqtt_client_helper.hpp"

namespace nasacpp
{
    class MqttClient
    {
    public:
        MqttClient(const std::string &address = "localhost", const int port = 1883, const std::string &client_id = std::string{"nasa_ha_mqtt_client"});

        void publish(const std::string &topic, const std::string &payload, bool retained = false);
        void subscribe(const std::string &topic);

        std::vector<mqtt::const_message_ptr> getData();

    protected:
        mqtt::async_client::ptr_t cli_;
        mqtt::connect_options conn_options_;
        std::unique_ptr<callback> cb_;
        std::chrono::seconds timeout_ = std::chrono::seconds(10);

        action_listener sub_listener_;
        action_listener pub_listener_;

        int qos_ = 1;
    };
} // namespace nasacpp
