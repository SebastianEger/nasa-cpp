#pragma once

#include <nlohmann/json.hpp>

namespace nasacpp
{
    // Starts Wi-Fi in station mode (reconnects automatically) or, in a QEMU test build, the emulated ethernet.
    // Configuration keys: ssid, password, hostname.
    void networkStart(const nlohmann::json &cfg);

    // Returns true while an IP address is assigned.
    bool networkConnected();
} // namespace nasacpp
