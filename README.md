# nasa-cpp: master your heat pump!
Implementation of the Samsung NASA protocol in C++ for monitoring and control. <br>
If you have a Samsung heat pump and want to integrate it into Home Assistant or want to create your own control unit, this project is for you! <br>
A NASA to Home Assistant bridge is already implemented and ready to use! <br>
This project is still work in progress and there might some missing NASA messages <br>

# Quick Start
1) If you do not already have one, get a RS485 to ETH or WiFi adapter and connect it to your WiFi kit or control unit
2) Build this project and update the tcp_hass_bridge config file
3) Start the service and add the MQTT device to your Home Assistant instance
4) Have fun!
5) Optional: Read out your FSV values with tcp_fsv_list

# Setups
### AE08BXYDGG-MIM03EN
Samsung Mono Quiet 8kW R32 (AE080BXYDGG/EU) + Control Kit (MIM-03EN) + Wi-Fi Kit 2.0 (MIM-H04N)

# Applications
1) [tcp_hass_bidge](APPS.md#tcp-hass-bridge)
2) [tcp_fsv_list](APPS.md#tcp-fsv-list)

# How to build
```
docker build -t nasa-cpp:latest .
```
# Development
```
docker build --target dev -t nasa-cpp:dev .
```

# Credits
- Main part of this project builds on Ianwin's work, so many thanks to him [https://github.com/lanwin/esphome_samsung_ac]
- Awesome NASA python project with lots of informations about NASA messages [https://github.com/echoDaveD/EHS-Sentinel]
- Another awesome NASA python project [https://github.com/70p4z/samsung-nasa-mqtt]
- Helpful documentation about the NASA protocol [https://wiki.myehs.eu/wiki/NASA_Protocol]