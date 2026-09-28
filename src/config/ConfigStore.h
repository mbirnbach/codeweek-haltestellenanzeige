#pragma once

#include "config/DeviceConfig.h"

// Reads/writes DeviceConfig in NVS (ESP32 Preferences), namespace "haltestelle".
//
// NVS keys: wifi_ssid, wifi_password, stop_id, stop_name, configured
class ConfigStore {
public:
    DeviceConfig load();
    bool save(const DeviceConfig& config);

    // Removes everything; the next boot starts the setup portal.
    void clear();
};
