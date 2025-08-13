# Hardware Setup Guide

This guide explains how to set up the RS485 connection between your Samsung heat pump and your network for use with nasa-cpp.

## Supported Hardware

### RS485 to TCP/IP Adapters
Any RS485-to-Ethernet or RS485-to-WiFi adapter should work. Tested options include:

- [Waveshare Industrial Grade Serial Server RS232/485 to WiFi and Ethernet](https://amzn.eu/d/hNWsWez)
  - Professional-grade option
  - Supports both WiFi and Ethernet
  - Industrial-grade reliability
  
- [Waveshare RS485 to ETH Single Serial Server](https://amzn.eu/d/28VrSgu)
  - Budget-friendly option
  - Ethernet-only
  - Compact size

## Detailed Setup Guide

### Wiring Instructions

#### Connection Points
| Samsung Heat Pump | RS485 Adapter |
|------------------|---------------|
| V1              | DC (6-36V)    |
| V2              | GND           |
| F1              | A (RS485+)    |
| F2              | B (RS485-)    |

#### Installation Options
1. **Preferred Method - WiFi Kit Integration:**
   - Open the WiFi Kit (if installed)
   - Connect the RS485 adapter using a 2x2 shielded cable
   - Ensures clean installation and reliable connection

2. **Alternative Method - Direct Connection:**
   - Connect directly to the control unit
   - Use when WiFi Kit is not installed or accessible

> ⚠️ Important: Always use shielded cable for RS485 connections to minimize interference.

### Adapter Configuration

#### UART Settings
| Setting | Value | Notes |
|---------|-------|-------|
| Baudrate | 9600 | Standard NASA protocol rate |
| Data Bits | 8 | Required for NASA protocol |
| Parity | Even | Must match heat pump settings |
| Stop Bits | 1 | Standard setting |
| Baudrate Adaptive (RFC2117) | Disable | Can cause communication issues if enabled |

#### Network Settings (Example for Server Mode)
| Setting | Value | Notes |
|---------|-------|-------|
| Mode | Server | Adapter listens for connections |
| Protocol | TCP | Required for nasa-cpp |
| Port | 26 | Default, can be changed in config |
| TCP Authentication | Disable | Not required for local network |

## Troubleshooting

### Common Issues

1. **No Connection:**
   - Verify wiring polarity (A/B connections)
   - Check power supply to adapter
   - Confirm network settings match configuration

2. **Intermittent Communication:**
   - Check cable shielding
   - Verify baudrate settings
   - Look for interference sources

3. **Data Errors:**
   - Verify parity setting is "Even"
   - Ensure proper grounding
   - Check cable length (max 1200m for RS485)

### Testing Connection

1. Use a network tool like `netcat` to test TCP connection:
```bash
nc -v [adapter-ip] [port]
```

2. Check adapter's status LEDs:
   - Power LED should be steady
   - TX/RX LEDs should flash during communication

## Additional Notes

- Keep RS485 cables away from power lines to avoid interference
- Consider using a dedicated power supply for the adapter
- Regular monitoring of communication quality is recommended
- Document your specific configuration for future reference