#include "config/ConfigStore.h"

#include <Preferences.h>

namespace {
constexpr const char* NVS_NAMESPACE = "haltestelle";
}

DeviceConfig ConfigStore::load() {
    DeviceConfig c;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) return c;  // nothing saved yet
    c.wifiSsid = prefs.getString("wifi_ssid", "");
    c.wifiPassword = prefs.getString("wifi_password", "");
    c.stopId = prefs.getString("stop_id", "");
    c.stopName = prefs.getString("stop_name", "");
    c.configured = prefs.getBool("configured", false);
    prefs.end();
    return c;
}

bool ConfigStore::save(const DeviceConfig& config) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return false;
    bool ok = true;
    // "configured" is written last and only if everything else succeeded, so
    // an interrupted save never looks like a complete configuration.
    ok &= prefs.putBool("configured", false) > 0;
    ok &= prefs.putString("wifi_ssid", config.wifiSsid) == config.wifiSsid.length();
    ok &= prefs.putString("wifi_password", config.wifiPassword) == config.wifiPassword.length();
    ok &= prefs.putString("stop_id", config.stopId) == config.stopId.length();
    ok &= prefs.putString("stop_name", config.stopName) == config.stopName.length();
    if (ok) ok &= prefs.putBool("configured", config.configured) > 0;
    prefs.end();
    return ok;
}

void ConfigStore::clear() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return;
    prefs.clear();
    prefs.end();
}
