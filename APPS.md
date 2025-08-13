# NASA-CPP Applications Guide

This document describes the applications included in the nasa-cpp project and their configuration options.

## 1. TCP-HASS-Bridge

The TCP-HASS-Bridge application connects your Samsung heat pump to Home Assistant via MQTT, enabling monitoring and control through the Home Assistant interface.

> 📝 Check [Hardware](HARDWARE.md) documentation for TCP connection setup.

### Configuration

Configuration file: [`cfg/tcp_hass_bridge.json`](cfg/tcp_hass_bridge.json)

#### Network Settings
| Parameter | Default Value | Description |
|-----------|--------------|-------------|
| `tcp::address` | `192.168.178.25` | IP address of the TCP server (RS485 adapter) |
| `tcp::port` | `26` | Port number of the TCP server |
| `mqtt::address` | `localhost` | Address of the MQTT broker |
| `mqtt::port` | `1883` | Port number of the MQTT broker |
| `mqtt::client_id` | `tcp_hass_bridge` | Client ID for MQTT connection |

#### Message Configuration
| Parameter | Default Value | Description |
|-----------|--------------|-------------|
| `read_msg_list` | `/etc/nasa-cpp/AE08BXYDGG-MIM03EN/sensor_msg_list.json` | Path to sensor message definitions |
| `request_msg_list` | `/etc/nasa-cpp/AE08BXYDGG-MIM03EN/control_msg_list.json` | Path to control message definitions |
| `write_msg_list` | `/etc/nasa-cpp/AE08BXYDGG-MIM03EN/fsv_msg_list.json` | Path to FSV message definitions |

#### Safety and Operation
| Parameter | Default Value | Description |
|-----------|--------------|-------------|
| `read_only` | `false` | When true, only receives data, no commands sent |
| `disable_write_send` | `true` | Safety guard to prevent FSV value changes |
| `initial_read_out` | `true` | Read all values on startup |
| `timeout` | `30` (seconds) | Connection timeout before reconnect |
| `buffer_limit` | `10000` (bytes) | Buffer size limit before clearing |

### Running the Bridge

1. Ensure the command is set to `tcp_hass_bridge` in [`compose.yml`](compose.yml):
```yaml
command: tcp_hass_bridge
```

2. Start the container:
```bash
docker compose up -d
```

### Home Assistant Integration

#### Dashboard
- An example dashboard configuration is provided in [`res/example-dashboard.yaml`](res/example-dashboard.yaml)
- Additional sensors configuration: [`res/example-sensors.yaml`](res/example-sensors.yaml)

![Home Assistant Dashboard](res/example-dashboard.png)

## 2. TCP-FSV-List

The TCP-FSV-List application reads all field setting values from your heat pump and exports them to a CSV file. This is useful for diagnostics and configuration verification.

### Configuration

Configuration file: [`cfg/tcp_fsv_list.json`](cfg/tcp_fsv_list.json)

The configuration parameters are similar to TCP-HASS-Bridge, but only TCP connection settings are required.

### Running FSV List

```bash
# Run with local config mounted
docker run --network host -v ./cfg:/etc/nasa-cpp -w /host nasa-cpp:latest tcp_fsv_list
```

The tool will:
1. Connect to your heat pump
2. Read all available FSV values
3. Export them to a CSV file in the current directory

> ⚠️ Note: This tool is read-only and cannot modify FSV values.

