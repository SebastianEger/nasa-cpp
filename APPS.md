# Applications Guide

This guide describes the applications included in nasa-cpp and their configuration options. All
applications connect to the heat pump through an RS485/TCP adapter; see [HARDWARE.md](HARDWARE.md)
for the hardware setup.

## Contents

1. [`tcp_hass_bridge`](#1-tcp_hass_bridge): MQTT / Home Assistant bridge
2. [`tcp_fsv_list`](#2-tcp_fsv_list): field setting export
3. [`tcp_nasa_logger`](#3-tcp_nasa_logger): packet logger
4. [`nasa-web`](#4-nasa-web): web dashboard

## Common behavior

- Each application reads a JSON configuration file. The default is
  `/etc/nasa-cpp/cfg/<application>.json`; another path can be given as the first argument.
- In Docker, mount the repository's `cfg/` directory to `/etc/nasa-cpp/cfg`. The message lists from
  `msg/` are included in the image under `/etc/nasa-cpp/msg`.
- The applications reconnect to the adapter automatically if no data is received for `timeout` seconds.

Common options:

| Option | Type | Default | Description |
|---|---|---|---|
| `tcp.address` | string | – | IP address or host name of the RS485/TCP adapter |
| `tcp.port` | integer | – | TCP port of the adapter |
| `timeout` | integer | `30` | Seconds without received data before the TCP connection is re-established |
| `buffer_limit` | integer | `10000` | Receive buffer size in bytes; the buffer is cleared when it is exceeded |

`timeout` and `buffer_limit` are used by `tcp_hass_bridge` and `tcp_nasa_logger`.

## 1. `tcp_hass_bridge`

Connects the heat pump to an MQTT broker. Every entry of the configured message lists becomes an
entity that is announced through Home Assistant MQTT discovery (`homeassistant/<type>/<id>/config`).
Values received from the bus are published to the entity's state topic. Commands received on its
command topic are sent to the heat pump.

Configuration file: [`cfg/tcp_hass_bridge.json`](cfg/tcp_hass_bridge.json)

| Option | Type | Default in `cfg/` | Description |
|---|---|---|---|
| `mqtt.address` | string | `localhost` | MQTT broker address |
| `mqtt.port` | integer | `1883` | MQTT broker port |
| `mqtt.client_id` | string | `tcp_hass_bridge` | MQTT client ID; must be unique on the broker |
| `read_msg_list` | path | `/etc/nasa-cpp/msg/AE08BXYDGG-MIM03EN/sensor_msg_list.json` | Values that are only read (sensors) |
| `request_msg_list` | path | `/etc/nasa-cpp/msg/AE08BXYDGG-MIM03EN/control_msg_list.json` | Values that can be set with *request* packets (operating controls) |
| `write_msg_list` | path | `/etc/nasa-cpp/msg/AE08BXYDGG-MIM03EN/fsv_msg_list.json` | Values that can be set with *write* packets (field settings) |
| `read_only` | boolean | `false` | If `true`, nothing is sent to the heat pump. Values are only collected from regular bus traffic |
| `disable_write_send` | boolean | `true` | If `true`, changes to *write* values (FSV) are rejected and the old value is restored |
| `initial_read_out` | boolean | `true` | Actively request all values that are still unknown after start-up |

The message list options are optional; omit one to disable that group. `read_only` is required.

> [!CAUTION]
> Field settings (FSV) configure the installation itself. Leave `disable_write_send` set to `true`
> unless you need to change a field setting and know the correct value.

Commands are retried up to 10 times until the heat pump confirms the new value.

### Running

```bash
docker compose up -d nasa-cpp      # with command: tcp_hass_bridge in compose.yml
```

### Home Assistant

The entities appear under the device **Samsung EHS** once the MQTT integration is set up in Home
Assistant. Examples are provided in `res/`:

- [`res/example-dashboard.yaml`](res/example-dashboard.yaml): dashboard (see screenshot below)
- [`res/example-sensors.yaml`](res/example-sensors.yaml): statistics, utility meters and COP template sensors

![Home Assistant dashboard](res/example-dashboard.png)

## 2. `tcp_fsv_list`

Reads all field setting values once and writes them to `fsv_list.csv` in the current working
directory. The application exits when every value has been received. It only sends *read* requests
and never changes a setting.

Configuration file: [`cfg/tcp_fsv_list.json`](cfg/tcp_fsv_list.json)

| Option | Type | Default in `cfg/` | Description |
|---|---|---|---|
| `fsv_list` | path | `/etc/nasa-cpp/msg/AE08BXYDGG-MIM03EN/fsv_msg_list.json` | Message list with the FSV entries to read |

### Running

Stop other applications first if your adapter accepts only one connection.

```bash
docker run --rm --network host -v ./cfg:/etc/nasa-cpp/cfg:ro -v "$PWD":/host -w /host \
    ghcr.io/sebastianeger/nasa-cpp:main tcp_fsv_list
```

The output has the columns `Name`, `Number`, `Value` (raw value) and `ValueTemplate` (the scaling
template from the message list, e.g. `{{ (value | float) / 10 }}`).

## 3. `tcp_nasa_logger`

Decodes all packets on the bus and writes them to `/var/nasa-cpp/logs/nasa_log.txt`. It is passive
(it never sends anything) and is useful to verify a new installation, analyze unknown messages, or
create message lists for other models.

Configuration file: [`cfg/tcp_nasa_logger.json`](cfg/tcp_nasa_logger.json). Only the common options apply.

### Running

```bash
docker run --rm --network host -v ./cfg:/etc/nasa-cpp/cfg:ro -v ./logs:/var/nasa-cpp/logs \
    ghcr.io/sebastianeger/nasa-cpp:main tcp_nasa_logger
```

The log file grows continuously; stop the logger once you have captured enough data.

## 4. `nasa-web`

A standalone web dashboard with live values, history, energy statistics, controls and FSV overview.
It uses the MQTT topics published by `tcp_hass_bridge`, so the bridge must be running. Deployment,
configuration and the HTTP API are documented in [web/README.md](web/README.md).

```bash
docker compose up -d nasa-web      # http://<host>:8080
```
