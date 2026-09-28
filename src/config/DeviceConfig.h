#pragma once

#include <Arduino.h>

// Per-device settings chosen in the setup portal and stored in NVS.
// (The participants' design settings are compile-time constants in
// src/student/display_config.h and are NOT stored here.)
struct DeviceConfig {
    String wifiSsid;
    String wifiPassword;
    String stopId;    // e.g. "2200002" – the API needs the id, not the name
    String stopName;  // e.g. "Kassel Königsplatz" – for display only
    bool configured = false;

    // True if everything needed for normal operation is present and sane.
    bool isComplete() const;
};

namespace config_rules {

// Same rules as the API/802.11: SSID 1–32 bytes, WPA password empty (open
// network) or 8–63 bytes, stop id 1–12 digits.
bool isValidSsid(const String& ssid);
bool isValidWifiPassword(const String& password);
bool isValidStopId(const String& id);
bool isValidStopName(const String& name);

}  // namespace config_rules
