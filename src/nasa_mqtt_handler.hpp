#pragma once

#include "nasa_msg_handler.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <spdlog/spdlog.h>

namespace nasacpp
{
    class NasaMqttHandler : public NasaMsgHandler
    {
    public:
        NasaMqttHandler(const std::string &topic, const MessageNumber &message_number, const Address &sa, const DataType &data_type = DataType::Read)
            : topic_(topic), NasaMsgHandler(message_number, sa, data_type) {}

        const std::string &getTopic() const
        {
            return topic_;
        }

    public:
        virtual json init(const json &init_param = json())
        {
            return init_param;
        }

        virtual std::string getMqttPayload()
        {
            if (val_is_ == INT_MAX)
            {
                return std::string();
            }
            return std::to_string(val_is_);
        }

        virtual void updateValueNew(const std::string &mqtt_payload)
        {
            // only update val if val should be changed
            if (data_type_ == DataType::Request || data_type_ == DataType::Write)
            {
                val_new_ = std::stoi(mqtt_payload, nullptr, 0);
            }
        }

    protected:
        const std::string topic_;
    };

    class NasaMqttHandlerEnum : public NasaMqttHandler
    {
    public:
        using NasaMqttHandler::NasaMqttHandler;

        virtual json init(const json &init_param) override
        {
            json init_out = init_param;

            // process options (key, val)
            if (init_param.contains("options"))
            {
                std::vector<std::string> opt_v;
                for (auto &[key, val] : init_param["options"].items())
                {
                    addSelectOption((int)val, key);
                    opt_v.push_back(key);
                }

                // replace enum options with options list
                init_out["options"] = opt_v;
            }

            return init_out;
        }

        virtual std::string getMqttPayload() override
        {
            if (!i_str_.count((int)val_is_))
            {
                spdlog::warn("NasaMqttHandlerEnum::getMqttPayload: No option for value {}", (int)val_is_);
                return std::string();
            }
            return i_str_[(int)val_is_];
        }

        virtual void updateValueNew(const std::string &mqtt_payload)
        {
            if (!str_i_.count(mqtt_payload))
            {
                spdlog::warn("NasaMqttHandlerEnum::updateValueNew: No option for value {}", mqtt_payload);
                return;
            }

            val_new_ = str_i_[mqtt_payload];
            retry_ctr_ = 0;
        }

    protected:
        void addSelectOption(int i, std::string str)
        {
            i_str_[i] = str;
            str_i_[str] = i;
        }

    private:
        std::unordered_map<int, std::string> i_str_;
        std::unordered_map<std::string, int> str_i_;
    };

    class NasaMqttHandlerBinary : public NasaMqttHandler
    {
    public:
        using NasaMqttHandler::NasaMqttHandler;

        virtual std::string getMqttPayload() override
        {
            if (val_is_ == 0)
            {
                return "OFF";
            }
            return "ON";
        }

        virtual void updateValueNew(const std::string &mqtt_payload)
        {
            if (mqtt_payload == "ON")
            {
                val_new_ = 1;
            }
            else
            {
                val_new_ = 0;
            }
            retry_ctr_ = 0;
        }
    };
} // namespace nasacpp
