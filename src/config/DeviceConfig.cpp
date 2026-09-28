#include "config/DeviceConfig.h"

namespace config_rules {

bool isValidSsid(const String& ssid) { return ssid.length() >= 1 && ssid.length() <= 32; }

bool isValidWifiPassword(const String& password) {
    return password.length() == 0 || (password.length() >= 8 && password.length() <= 63);
}

bool isValidStopId(const String& id) {
    if (id.length() < 1 || id.length() > 12) return false;
    for (unsigned int i = 0; i < id.length(); ++i) {
        if (id[i] < '0' || id[i] > '9') return false;
    }
    return true;
}

bool isValidStopName(const String& name) { return name.length() >= 1 && name.length() <= 96; }

}  // namespace config_rules

bool DeviceConfig::isComplete() const {
    return configured && config_rules::isValidSsid(wifiSsid) && config_rules::isValidWifiPassword(wifiPassword) &&
           config_rules::isValidStopId(stopId) && config_rules::isValidStopName(stopName);
}
