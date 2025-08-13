# nasa-cpp: Master Your Heat Pump!

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++: 20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-green.svg)](https://www.linux.org/)

A powerful C++ implementation of the Samsung NASA protocol for heat pump monitoring and control, with seamless Home Assistant integration.

![Dashboard Example](res/example-dashboard.png)

## 🌟 Features

- **Complete NASA Protocol Implementation**
  - Full support for monitoring and control operations
  - Built with modern C++20 for reliability and performance
  
- **Home Assistant Ready**
  - Built-in MQTT bridge for instant integration
  - Customizable sensor and control configurations
  - Example dashboard and sensor configurations included
  
- **Advanced Tools**
  - Field Setting Value (FSV) reader and exporter
  - Real-time monitoring capabilities
  - Configurable safety controls
  
- **Docker Support**
  - Easy deployment with Docker containers
  - Docker Compose configuration included
  - Development environment ready

## 📋 Prerequisites

- Linux environment (tested on modern distributions)
- Docker (recommended) or:
  - C++20 compatible compiler
  - CMake 3.12 or higher
  - Ninja build system
  - PahoMQTT C++ client
  - nlohmann-json 3.12.0

## 🚀 Quick Start

1. **Clone the Repository**
   ```bash
   git clone https://github.com/yourusername/nasa-cpp.git
   cd nasa-cpp
   ```

2. **Hardware Setup**
   - Connect your RS485 adapter to your heat pump
   - Configure network settings
   - See [Hardware Guide](HARDWARE.md) for detailed instructions

3. **Build the Project**
   ```bash
   # Using Docker (recommended)
   docker build -t nasa-cpp:latest .

   # Or manually
   mkdir build && cd build
   cmake .. -GNinja
   ninja
   ```

4. **Configure**
   - Copy example configs from `cfg/` directory
   - Update settings for your environment
   - See [Applications Guide](APPS.md) for configuration details

5. **Run**
   ```bash
   # Using Docker Compose
   docker compose up -d

   # Or run specific tools
   docker run --network host -v ./cfg:/etc/nasa-cpp nasa-cpp:latest tcp_hass_bridge
   ```

## 🛠️ Applications

### 1. Home Assistant Bridge (`tcp_hass_bridge`)
Connects your heat pump to Home Assistant via MQTT.
- Real-time monitoring
- Remote control capabilities
- Customizable dashboard
- [Configuration Guide](APPS.md#1-tcp-hass-bridge)

### 2. FSV Tool (`tcp_fsv_list`)
Field Setting Value management tool.
- Read all FSV values
- Export to CSV
- Safety controls
- [Usage Guide](APPS.md#2-tcp-fsv-list)

## 📝 Configuration

All configuration is done via JSON files in the `cfg/` directory:

```
cfg/
├── tcp_hass_bridge.json    # MQTT bridge configuration
├── tcp_fsv_list.json       # FSV tool configuration
└── AE08BXYDGG-MIM03EN/    # Model-specific configurations
    ├── control_msg_list.json
    ├── fsv_msg_list.json
    └── sensor_msg_list.json
```

See [Applications Guide](APPS.md) for detailed configuration options.

## 🔧 Development

1. **Build Development Environment**
   ```bash
   docker build --target dev -t nasa-cpp:dev .
   ```

2. **Run Tests** (if enabled)
   ```bash
   cmake -DBUILD_TESTS=ON ..
   ninja test
   ```

3. **Build Samples** (if enabled)
   ```bash
   cmake -DBUILD_SAMPLES=ON ..
   ninja
   ```

## 🤝 Contributing

Contributions are welcome! Here's how you can help:

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to your branch
5. Open a Pull Request

Please ensure your code follows the project's coding style and includes appropriate tests.

## 📚 Documentation

- [Applications Guide](APPS.md) - Detailed application documentation
- [Hardware Setup](HARDWARE.md) - Hardware connection and configuration
- [Example Dashboard](res/example-dashboard.yaml) - Home Assistant dashboard configuration
- [Example Sensors](res/example-sensors.yaml) - Sensor configurations

## 🙏 Credits

This project builds on the work of several excellent projects:

- [esphome_samsung_ac](https://github.com/lanwin/esphome_samsung_ac) - Core NASA protocol implementation
- [EHS-Sentinel](https://github.com/echoDaveD/EHS-Sentinel) - Python NASA implementation
- [samsung-nasa-mqtt](https://github.com/70p4z/samsung-nasa-mqtt) - MQTT integration reference
- [NASA Protocol Wiki](https://wiki.myehs.eu/wiki/NASA_Protocol) - Protocol documentation

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 💬 Support

- Open an issue for bugs or feature requests
- Check existing issues for solutions
- Review the documentation for common questions

---

**Made with ❤️ for heat pump enthusiasts**
