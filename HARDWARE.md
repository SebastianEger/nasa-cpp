# RS485 to TCP
Any RS485-to-ETH or RS485-to-WiFi adapter, e.g:
- [Waveshare Industrial Grade Serial Server RS232/485 to WiFi and Ethernet](https://amzn.eu/d/hNWsWez)
- [Waveshare RS485 to ETH Single Serial Server](https://amzn.eu/d/28VrSgu)

## Example: Waveshare Industrial Grade Serial Server RS232/485 to WiFi and Ethernet
### Wiring
|  SAMSUNG   | ADAPTER |
| -------- | ------- |
| V1 | DC (6-36V) |
| V2 | GND |
| F1 |  A  |
| F2 |  B  |

Best option is to open up the WiFi-Kit (if installed) and connect the RSD485 adapter with a 2x2 shielded cable. <br>
Otherwise, connect it directly to the control unit.

### Uart Setting
|  Setting   | Value |
| -------- | ------- |
| Baudrate | 9600 |
| Data Bits | 8 |
| Parity |  even  |
| Stop |  1  |
| Baudrate adaptive (RFC2117) |  Disable  |

### Network A Setting
|  Setting   | Value |
| -------- | ------- |
| Mode | Server |
| Protocol | TCP |
| Port |  26 (or choose your own) |
| TCP connection password authentication |  Disable  |