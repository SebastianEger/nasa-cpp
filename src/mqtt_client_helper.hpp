#pragma once

#include <string>

#include <spdlog/spdlog.h>

#include <mqtt/async_client.h>

namespace nasacpp
{
    class MqttClient;

    class action_listener : public virtual mqtt::iaction_listener
    {
        std::string name;

        void on_failure(const mqtt::token &tok) override {}
        void on_success(const mqtt::token &tok) override {}

    public:
        action_listener(const std::string &name) : name(name) {}
    };

    class callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener
    {
        std::mutex mtx_data;
        std::vector<mqtt::const_message_ptr> data;

        mqtt::async_client &cli_;
        mqtt::connect_options &connOpts_;

        void reconnect()
        {
            // wait 2.5 seconds for next reconnect
            std::this_thread::sleep_for(std::chrono::milliseconds(2500));
            try
            {
                cli_.connect(connOpts_, nullptr, *this);
            }
            catch (const mqtt::exception &exc)
            {
                spdlog::error("Error: {}", exc.what());
            }
        }

        void on_failure(const mqtt::token &tok) override
        {
            spdlog::warn("MqttClient: Connection attempt failed");
            reconnect();
        }

        void on_success(const mqtt::token &tok) override {}

        void connected(const std::string &cause) override
        {
            spdlog::info("MqttClient: Connected!");
        }

        void connection_lost(const std::string &cause) override
        {
            spdlog::warn("MqttClient: Connection lost");
            if (!cause.empty())
            {
                spdlog::warn("MqttClient: Cause: {}", cause);
            }

            spdlog::info("MqttClient: Reconnecting ...");
            reconnect();
        }

        void message_arrived(mqtt::const_message_ptr msg) override
        {
            std::lock_guard<std::mutex> lock(mtx_data);
            data.push_back(msg);
        }

        void delivery_complete(mqtt::delivery_token_ptr token) override {}

    public:
        friend MqttClient;

        callback(mqtt::async_client &cli, mqtt::connect_options &connOpts)
            : cli_(cli), connOpts_(connOpts)
        {
        }
    };
} // namespace nasacpp
