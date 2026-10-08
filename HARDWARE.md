# Hardware Guide

This guide describes how to connect a Samsung heat pump to nasa-cpp: the hardware you need, the
wiring, the RS485 adapter settings and how to verify the connection.

## Contents

- [Overview](#overview)
- [Safety](#safety)
- [Required hardware](#required-hardware)
- [Wiring](#wiring)
- [Adapter configuration](#adapter-configuration)
- [nasa-cpp configuration](#nasa-cpp-configuration)
- [Verifying the connection](#verifying-the-connection)
- [Troubleshooting](#troubleshooting)

## Overview

Samsung heat pumps communicate over a two-wire RS485 bus using the NASA protocol. nasa-cpp does not
access the bus directly. Instead, it connects over TCP to a serial device server (RS485-to-Ethernet or
RS485-to-Wi-Fi adapter), which forwards the raw bus traffic in both directions.

```
┌──────────────┐   RS485 (F1/F2)   ┌─────────────────┐   TCP (e.g. port 26)   ┌──────────────┐
│ Samsung heat │───────────────────│ RS485 / TCP     │────────────────────────│ nasa-cpp     │
│ pump         │───────────────────│ adapter         │          LAN           │ (any Linux   │
│              │  12 V DC (V1/V2)  │ (TCP server)    │                        │  host)       │
└──────────────┘                   └─────────────────┘                        └──────────────┘
```

The adapter can be powered from the heat pump's 12 V supply terminals, so no extra power supply is
needed in most installations.

## Safety

> [!WARNING]
> Switch off and isolate the heat pump at the circuit breaker before opening any housing. The indoor
> and outdoor units contain mains voltage. If you are not qualified to work on electrical installations,
> have the adapter installed by a professional. Opening the unit may affect your warranty.

- Never connect anything to the mains or the indoor/outdoor power terminals. Only the low-voltage
  terminals described below are used.
- Measure the supply voltage on V1/V2 before you connect the adapter, and check that it is within the
  adapter's input range.
- Start with `read_only` enabled in nasa-cpp and keep `disable_write_send` enabled until you have
  verified that the values read are correct.

## Required hardware

| Component | Requirements | Notes |
|---|---|---|
| RS485 serial device server | RS485 half-duplex, TCP server mode, 9600 baud with even parity, 6–36 V DC input | See tested adapters below |
| Cable | Shielded twisted pair, 2 × 2 × 0.5 mm² (e.g. LiYCY 2×2×0.5) | One pair for data (F1/F2), one pair for power (V1/V2) |
| Network | Ethernet or Wi-Fi with a fixed IP address for the adapter | Use a DHCP reservation or a static IP |
| Host | Any Linux machine on the same network (e.g. Raspberry Pi, NAS, server) | Runs nasa-cpp, ideally with Docker |

### Tested adapters

| Adapter | Interfaces | Notes |
|---|---|---|
| [Waveshare RS232/485 to WiFi and Ethernet serial server](https://amzn.eu/d/hNWsWez) | Ethernet, Wi-Fi | Industrial grade, DIN rail mount |
| [Waveshare RS485 to ETH serial server](https://amzn.eu/d/28VrSgu) | Ethernet | Compact, low cost |

Other serial device servers should work as long as they provide a raw TCP server (no Modbus gateway
mode and no protocol conversion) and support the serial settings listed below.

## Wiring

### Terminal assignment

| Heat pump terminal | Signal | Adapter terminal |
|---|---|---|
| V1 | +12 V DC supply | DC+ / VIN (6–36 V) |
| V2 | Ground | GND |
| F1 | RS485 data A | A / RS485+ |
| F2 | RS485 data B | B / RS485− |

The terminal labels refer to the communication terminal block that the Samsung Wi-Fi kit or wired
remote controller connects to. Terminal names can differ between models and controller kits. Check
the installation manual of your unit and measure before connecting.

### Connection point

1. **At the Wi-Fi kit (recommended):** if a Samsung Wi-Fi kit is installed, open it and connect the
   adapter in parallel to its V1/V2/F1/F2 terminals. This point is easy to reach, already provides
   12 V, and does not require opening the indoor unit.
2. **At the indoor unit or controller kit:** if no Wi-Fi kit is installed, connect to the corresponding
   terminals of the indoor unit or control kit that the wired remote controller or Wi-Fi kit would
   use.

### Cabling notes

- Use one twisted pair for F1/F2 and the other pair for V1/V2.
- Connect the cable shield to ground on **one** end only (usually the adapter side) to avoid ground
  loops.
- Keep the data cable away from mains and compressor power cables.
- The bus is usually short and already terminated by the heat pump. Do **not** enable an additional
  termination resistor on the adapter unless you see communication errors on long cable runs.

## Adapter configuration

Configure the adapter through its web interface or configuration tool (for Waveshare adapters:
*Vircom* or the built-in web page).

### Serial port

| Setting | Value | Notes |
|---|---|---|
| Baud rate | 9600 | NASA bus speed |
| Data bits | 8 | |
| Parity | Even | Required. With "None" all packets fail to decode |
| Stop bits | 1 | |
| Flow control | None | |
| Baud rate adaptive (RFC 2217) | Disabled | Can change the serial settings unexpectedly |

### Network

| Setting | Value | Notes |
|---|---|---|
| Work mode | TCP server | nasa-cpp connects as a TCP client |
| Local port | 26 | Any free port works if it matches the nasa-cpp configuration |
| IP address | Static or DHCP reservation | Must match `tcp.address` in nasa-cpp |
| Protocol / transfer mode | Raw TCP (transparent) | Not Modbus TCP, not HTTP/MQTT modes |
| Max. TCP connections | ≥ 1 | Each running nasa-cpp application uses one connection |
| TCP authentication / registration packet | Disabled | Extra bytes would corrupt the data stream |
| Heartbeat packet | Disabled | Same reason as above |

## nasa-cpp configuration

Set the adapter's address and port in the configuration file of each application you use
(`cfg/tcp_hass_bridge.json`, `cfg/tcp_fsv_list.json`, `cfg/tcp_nasa_logger.json`):

```json
"tcp": {
    "address": "192.168.178.25",
    "port": 26
}
```

nasa-cpp reconnects automatically if no data is received for `timeout` seconds (default 30).

## Verifying the connection

1. **Network:** check that the adapter accepts TCP connections:

   ```bash
   nc -vz 192.168.178.25 26
   ```

2. **Bus traffic:** the heat pump sends data continuously. Raw NASA packets start with `0x32` and end
   with `0x34`:

   ```bash
   timeout 5 nc 192.168.178.25 26 | xxd | head
   ```

   No output means the adapter receives nothing from the bus: check the wiring and the serial settings.

3. **Decoding:** run `tcp_nasa_logger`. It writes every decoded packet to
   `logs/nasa_log.txt`:

   ```bash
   docker run --rm --network host -v ./cfg:/etc/nasa-cpp/cfg:ro -v ./logs:/var/nasa-cpp/logs \
       ghcr.io/sebastianeger/nasa-cpp:main tcp_nasa_logger
   ```

   Lines with `[DECODE]` confirm that the serial settings are correct. If your adapter accepts only one
   TCP connection, stop the logger before you start another application.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `nc` cannot connect | Wrong IP or port, adapter not in TCP server mode, or connection limit reached | Check the network settings; stop other clients connected to the adapter |
| Connection works but no data arrives | A/B swapped or wrong terminals | Swap the A/B wires on the adapter and check the wiring |
| Data arrives but nothing is decoded | Wrong baud rate or parity, or extra bytes from heartbeat/registration packets | Set 9600 8E1 and disable heartbeat and registration packets |
| Frequent reconnects (`forcing TcpClient to reconnect`) | Unstable Wi-Fi, or the adapter drops idle connections | Use Ethernet, disable idle timeouts on the adapter, or increase `timeout` |
| Occasional decode errors | Interference or a missing ground reference | Use shielded twisted pair, ground the shield on one side, route away from power cables |
| Values can be read but commands have no effect | `read_only` is enabled, or `disable_write_send` blocks FSV writes | Check the safety options in `cfg/tcp_hass_bridge.json` (see [APPS.md](APPS.md)) |
