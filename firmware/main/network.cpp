#include "network.h"

#include <atomic>
#include <cstring>
#include <string>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <sdkconfig.h>

#if CONFIG_ETH_USE_OPENETH
#include <esp_eth.h>
#endif

namespace nasacpp
{
    static const char *TAG = "network";
    static std::atomic_bool s_connected = false;

#if CONFIG_ETH_USE_OPENETH

    // QEMU test build: emulated OpenCores ethernet instead of Wi-Fi
    static void onEthEvent(void *, esp_event_base_t, int32_t, void *data)
    {
        auto *event = static_cast<ip_event_got_ip_t *>(data);
        ESP_LOGI(TAG, "Ethernet connected, IP " IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;
    }

    void networkStart(const nlohmann::json &)
    {
        ESP_LOGW(TAG, "QEMU test build: using emulated ethernet instead of Wi-Fi");
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t *netif = esp_netif_new(&netif_cfg);

        eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
        eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
        phy_cfg.autonego_timeout_ms = 100;
        esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_cfg);
        esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_cfg);
        esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
        esp_eth_handle_t eth = nullptr;
        ESP_ERROR_CHECK(esp_eth_driver_install(&eth_cfg, &eth));
        ESP_ERROR_CHECK(esp_netif_attach(netif, esp_eth_new_netif_glue(eth)));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &onEthEvent, nullptr));
        ESP_ERROR_CHECK(esp_eth_start(eth));
    }

#else

    static void onWifiEvent(void *, esp_event_base_t base, int32_t id, void *data)
    {
        if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
        {
            if (s_connected)
            {
                ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting ...");
            }
            s_connected = false;
            esp_wifi_connect();
        }
        else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
        {
            auto *event = static_cast<ip_event_got_ip_t *>(data);
            ESP_LOGI(TAG, "Connected, IP " IPSTR, IP2STR(&event->ip_info.ip));
            s_connected = true;
        }
    }

    void networkStart(const nlohmann::json &cfg)
    {
        const std::string ssid = cfg.value("ssid", std::string());
        const std::string password = cfg.value("password", std::string());
        const std::string hostname = cfg.value("hostname", std::string("nasa-cpp"));

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_t *netif = esp_netif_create_default_wifi_sta();
        esp_netif_set_hostname(netif, hostname.c_str());

        wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &onWifiEvent, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &onWifiEvent, nullptr));

        wifi_config_t wifi_cfg = {};
        strncpy((char *)wifi_cfg.sta.ssid, ssid.c_str(), sizeof(wifi_cfg.sta.ssid) - 1);
        strncpy((char *)wifi_cfg.sta.password, password.c_str(), sizeof(wifi_cfg.sta.password) - 1);
        wifi_cfg.sta.threshold.authmode = password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "Connecting to Wi-Fi '%s' as '%s' ...", ssid.c_str(), hostname.c_str());
    }

#endif

    bool networkConnected()
    {
        return s_connected;
    }
} // namespace nasacpp
