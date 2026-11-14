#include "esp_mqtt_client.h"

#include <esp_log.h>

namespace nasacpp
{
    static const char *TAG = "mqtt";

    EspMqttClient::EspMqttClient(const nlohmann::json &cfg)
    {
        uri_ = "mqtt://" + cfg.value("address", std::string("localhost")) + ":" + std::to_string(cfg.value("port", 1883));
        client_id_ = cfg.value("client_id", std::string("nasa_cpp_esp32"));
        username_ = cfg.value("username", std::string());
        password_ = cfg.value("password", std::string());

        esp_mqtt_client_config_t mqtt_cfg = {};
        mqtt_cfg.broker.address.uri = uri_.c_str();
        mqtt_cfg.credentials.client_id = client_id_.c_str();
        if (!username_.empty())
        {
            mqtt_cfg.credentials.username = username_.c_str();
            mqtt_cfg.credentials.authentication.password = password_.c_str();
        }
        mqtt_cfg.session.keepalive = 30;
        mqtt_cfg.buffer.size = 2048;
        mqtt_cfg.network.reconnect_timeout_ms = 5000;

        client_ = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, &EspMqttClient::onEvent, this);

        ESP_LOGI(TAG, "Connecting to %s", uri_.c_str());
        esp_mqtt_client_start(client_);
    }

    EspMqttClient::~EspMqttClient()
    {
        esp_mqtt_client_stop(client_);
        esp_mqtt_client_destroy(client_);
    }

    void EspMqttClient::publish(const std::string &topic, const std::string &payload, bool retained)
    {
        if (connected_)
        {
            esp_mqtt_client_publish(client_, topic.c_str(), payload.data(), payload.size(), 1, retained ? 1 : 0);
        }
    }

    void EspMqttClient::subscribe(const std::string &topic)
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            topics_.push_back(topic);
        }
        if (connected_)
        {
            esp_mqtt_client_subscribe(client_, topic.c_str(), 1);
        }
    }

    std::vector<MqttMessage> EspMqttClient::getData()
    {
        std::lock_guard<std::mutex> lock(mtx_rx_);
        std::vector<MqttMessage> out;
        out.swap(rx_);
        return out;
    }

    void EspMqttClient::onEvent(void *arg, esp_event_base_t, int32_t, void *event_data)
    {
        static_cast<EspMqttClient *>(arg)->handleEvent(static_cast<esp_mqtt_event_handle_t>(event_data));
    }

    void EspMqttClient::handleEvent(esp_mqtt_event_handle_t event)
    {
        switch (event->event_id)
        {
        case MQTT_EVENT_CONNECTED:
        {
            ESP_LOGI(TAG, "Connected");
            connected_ = true;
            std::lock_guard<std::mutex> lock(mtx_);
            for (const auto &topic : topics_)
            {
                esp_mqtt_client_subscribe(client_, topic.c_str(), 1);
            }
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            if (connected_)
            {
                ESP_LOGW(TAG, "Disconnected");
            }
            connected_ = false;
            break;

        case MQTT_EVENT_DATA:
        {
            // payloads larger than the buffer arrive in several parts
            std::lock_guard<std::mutex> lock(mtx_rx_);
            if (event->current_data_offset == 0)
            {
                partial_.topic.assign(event->topic, event->topic_len);
                partial_.payload.clear();
            }
            partial_.payload.append(event->data, event->data_len);
            if (event->current_data_offset + event->data_len >= event->total_data_len)
            {
                rx_.push_back(std::move(partial_));
                partial_ = {};
            }
            break;
        }

        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "Error (type %d)", event->error_handle ? (int)event->error_handle->error_type : -1);
            break;

        default:
            break;
        }
    }
} // namespace nasacpp
