#include "uart_transport.h"

#include <esp_log.h>

namespace nasacpp
{
    static const char *TAG = "uart";

    UartTransport::UartTransport(const nlohmann::json &cfg, uart_port_t port) : port_(port)
    {
        const int tx_pin = cfg.value("tx_pin", 22);
        const int rx_pin = cfg.value("rx_pin", 19);
        const int de_pin = cfg.value("de_pin", -1);
        const int baud_rate = cfg.value("baud_rate", 9600);

        uart_config_t uart_cfg = {};
        uart_cfg.baud_rate = baud_rate;
        uart_cfg.data_bits = UART_DATA_8_BITS;
        uart_cfg.parity = UART_PARITY_EVEN;
        uart_cfg.stop_bits = UART_STOP_BITS_1;
        uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        uart_cfg.source_clk = UART_SCLK_DEFAULT;

        ESP_ERROR_CHECK(uart_driver_install(port_, 4096, 0, 0, nullptr, 0));
        ESP_ERROR_CHECK(uart_param_config(port_, &uart_cfg));
        // with a DE pin, the UART drives it as RTS in RS485 half duplex mode
        ESP_ERROR_CHECK(uart_set_pin(port_, tx_pin, rx_pin, de_pin >= 0 ? de_pin : UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_set_mode(port_, de_pin >= 0 ? UART_MODE_RS485_HALF_DUPLEX : UART_MODE_UART));

        ESP_LOGI(TAG, "UART%d: %d baud 8E1, TX GPIO%d, RX GPIO%d, DE %s", (int)port_, baud_rate, tx_pin, rx_pin,
                 de_pin >= 0 ? std::to_string(de_pin).c_str() : "auto");
    }

    UartTransport::~UartTransport()
    {
        uart_driver_delete(port_);
    }

    int UartTransport::read(std::deque<uint8_t> &data)
    {
        size_t available = 0;
        uart_get_buffered_data_len(port_, &available);
        if (available == 0)
        {
            return 0;
        }

        uint8_t buffer[256];
        int total = 0;
        while (available > 0)
        {
            int n = uart_read_bytes(port_, buffer, std::min(available, sizeof(buffer)), 0);
            if (n <= 0)
            {
                break;
            }
            data.insert(data.end(), buffer, buffer + n);
            total += n;
            available -= std::min(available, (size_t)n);
        }
        return total;
    }

    int UartTransport::write(const std::vector<uint8_t> &data)
    {
        int n = uart_write_bytes(port_, data.data(), data.size());
        uart_wait_tx_done(port_, pdMS_TO_TICKS(1000));
        return n;
    }

    void UartTransport::reconnect()
    {
        ESP_LOGW(TAG, "No data, flushing UART input");
        uart_flush_input(port_);
    }
} // namespace nasacpp
