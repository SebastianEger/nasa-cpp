# ESP32 Firmware

This firmware runs the nasa-cpp MQTT bridge (`tcp_hass_bridge`) directly on an ESP32. With an
RS485 base, the ESP32 replaces both the RS485/TCP adapter and the Linux host: it connects to the
heat pump's bus and to your MQTT broker over Wi-Fi.

The firmware uses the same library, message lists and Home Assistant MQTT discovery as the Linux
application. Home Assistant and the [nasa-web dashboard](../web/README.md) work with it unchanged.

> [!NOTE]
> The firmware has been tested in the ESP32 emulator (QEMU), with both the UART and the TCP
> transport. Please report results on real hardware in an issue.

## Contents

- [Supported hardware](#supported-hardware)
- [Wiring](#wiring)
- [Configuration](#configuration)
- [Building](#building)
- [Flashing](#flashing)
- [Status LED](#status-led)
- [Troubleshooting](#troubleshooting)
- [Testing without hardware](#testing-without-hardware)

## Supported hardware

| Component | Notes |
|---|---|
| [M5Stack Atom Lite](https://docs.m5stack.com/en/core/ATOM%20Lite) | ESP32-PICO-D4, 4 MB flash; reference board for this firmware |
| [M5Stack Atomic RS485 Base](https://docs.m5stack.com/en/atom/Atomic%20RS485%20Base) | RS485 transceiver with automatic direction control and a DC-DC converter that can power the Atom from the bus supply. Sold together with the Atom Lite as the *ATOM RS485 kit* (K045) |

Other ESP32 boards with 4 MB flash work as well. Set the UART pins, an optional driver-enable pin
and the LED pin in the configuration.

Instead of an RS485 base, the firmware can also connect over TCP to an existing RS485/TCP adapter
(`"transport": "tcp"`). It then replaces only the Linux host.

## Wiring

| Heat pump terminal | Atomic RS485 Base |
|---|---|
| V1 (+12 V DC) | DC input (+) |
| V2 (ground) | GND |
| F1 (RS485 A) | A |
| F2 (RS485 B) | B |

The Atom Lite communicates with the base on GPIO22 (TX) and GPIO19 (RX). These are the defaults in
the configuration.

> [!WARNING]
> Switch off the heat pump before wiring and read the safety notes in [HARDWARE.md](../HARDWARE.md).
> Measure the voltage on V1/V2 and check it against the input range printed in the base's
> documentation before you connect it. Do not power the Atom from USB and the bus at the same time
> unless the base's documentation states that this is safe.

## Configuration

Copy `config.example.json` to `config.json` and edit it. `config.json` is ignored by git because it
contains your Wi-Fi password.

```bash
cd firmware
cp config.example.json config.json
```

| Option | Default | Description |
|---|---|---|
| `wifi.ssid`, `wifi.password` | – | Wi-Fi network (2.4 GHz) |
| `wifi.hostname` | `nasa-cpp` | Host name of the device in your network |
| `mqtt.address`, `mqtt.port` | – / `1883` | MQTT broker; IP address or host name |
| `mqtt.client_id` | `nasa_cpp_atom` | MQTT client ID; must be unique on the broker |
| `mqtt.username`, `mqtt.password` | empty | Broker credentials; leave empty for anonymous access |
| `transport` | `uart` | `uart` for an RS485 base, `tcp` for an RS485/TCP adapter |
| `uart.tx_pin`, `uart.rx_pin` | `22`, `19` | UART pins connected to the RS485 transceiver |
| `uart.de_pin` | `-1` | Driver-enable pin for transceivers without automatic direction control; `-1` = automatic |
| `uart.baud_rate` | `9600` | NASA bus speed (always 8 data bits, even parity, 1 stop bit) |
| `tcp.address`, `tcp.port` | – | RS485/TCP adapter, only with `"transport": "tcp"` |
| `led_pin` | `27` | Status LED (SK6812); `-1` disables it |
| `read_msg_list`, `request_msg_list`, `write_msg_list` | `/spiffs/…` | Message lists; remove an entry to disable that group |
| `read_only`, `disable_write_send`, `initial_read_out`, `timeout`, `buffer_limit` | | Same meaning as for `tcp_hass_bridge`, see [APPS.md](../APPS.md#1-tcp_hass_bridge) |

The configuration and the message lists from [`msg/AE08BXYDGG-MIM03EN`](../msg/AE08BXYDGG-MIM03EN)
are stored in a SPIFFS partition that is built and flashed together with the firmware. To use
other message lists, build with `-D NASA_MSG_DIR=<directory>`.

## Building

The easiest way is the official ESP-IDF Docker image (ESP-IDF v5.5). Run this from the repository
root:

```bash
docker run --rm -v "$PWD":/project -w /project/firmware espressif/idf:v5.5.6 \
    sh -c "idf.py build && idf.py merge-bin -o nasa-cpp-esp32-full.bin"
```

With a local [ESP-IDF v5.5 installation](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/get-started/):

```bash
cd firmware
idf.py build
idf.py merge-bin -o nasa-cpp-esp32-full.bin
```

The build downloads nlohmann/json and spdlog from GitHub, in the same versions as the Linux build.

Results in `firmware/build/`:

| File | Content |
|---|---|
| `nasa-cpp-esp32-full.bin` | Complete 4 MB flash image: bootloader, partition table, firmware and configuration |
| `nasa-cpp-esp32.bin` | Firmware only |
| `storage.bin` | Configuration and message lists only |

## Flashing

Connect the Atom Lite by USB-C. The serial port is usually `/dev/ttyUSB0` on Linux and
`/dev/cu.usbserial-*` on macOS.

**esptool** (`pip install esptool`):

```bash
esptool.py --chip esp32 -p /dev/ttyUSB0 -b 460800 write_flash 0x0 firmware/build/nasa-cpp-esp32-full.bin
```

If flashing fails, retry with `-b 115200`.

**Browser:** open the [ESP web flasher](https://espressif.github.io/esptool-js/) in Chrome or Edge,
connect, and flash `nasa-cpp-esp32-full.bin` at address `0x0`.

**ESP-IDF** (local installation or Docker with `--device /dev/ttyUSB0`):

```bash
idf.py -p /dev/ttyUSB0 flash monitor     # firmware + configuration, then show the log
idf.py -p /dev/ttyUSB0 storage-flash     # only the configuration and message lists
```

The log is printed on the USB serial port at 115200 baud (`idf.py monitor`, or any serial terminal).

## Status LED

| LED | Meaning |
|---|---|
| Red, blinking | Configuration missing or invalid, or the message lists could not be loaded (see the log) |
| Blue, blinking | Connecting to Wi-Fi |
| Yellow | Connecting to the MQTT broker |
| Green | Running; brighter flash whenever packets are received from the heat pump |
| Red | Running, but no data from the heat pump for `timeout` seconds |

## Troubleshooting

| Symptom | Check |
|---|---|
| Stays blue | Wi-Fi name and password; the ESP32 supports 2.4 GHz networks only |
| Stays yellow | `mqtt.address`, port and credentials; is the broker reachable from the Wi-Fi network? |
| Red after start | Wiring of F1/F2 (swap A/B), and `uart.tx_pin`/`uart.rx_pin` (swap them if in doubt) |
| Values arrive, commands have no effect | `read_only` and `disable_write_send` in `config.json` |
| Resets or `out of memory` in the log | Remove `write_msg_list` to drop the FSV entities and save memory |

With the default message lists, about 125 KB of heap stays free during operation (measured in the
emulator, without Wi-Fi). The firmware logs the free heap once per minute.

## Testing without hardware

The firmware can run in Espressif's QEMU, which is included in the ESP-IDF Docker image. The test
build uses an emulated Ethernet interface instead of Wi-Fi (`sdkconfig.qemu`), and the host is
reachable at `10.0.2.2`. Use a configuration with `"transport": "tcp"` that points to an RS485/TCP
adapter or a simulator, and `"led_pin": -1`.

```bash
cd firmware
idf.py -B build-qemu -D SDKCONFIG=build-qemu/sdkconfig \
    -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.qemu" -D NASA_CONFIG_FILE=<qemu-config.json> build
cd build-qemu && esptool.py --chip esp32 merge_bin --fill-flash-size 4MB -o flash_image.bin @flash_args
qemu-system-xtensa -nographic -machine esp32 -drive file=flash_image.bin,if=mtd,format=raw \
    -nic user,model=open_eth -global driver=timer.esp32.timg,property=wdt_disable,value=true
```

To test the UART transport, replace `-nographic` with
`-display none -monitor none -serial stdio -serial tcp:<host>:<port>`. QEMU then connects the
firmware's UART1 to a TCP server that plays the role of the bus.
