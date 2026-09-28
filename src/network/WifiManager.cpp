#include "network/WifiManager.h"

#include <WiFi.h>

#include "config/AppSettings.h"

void WifiManager::begin(const String& ssid, const String& password) {
    ssid_ = ssid;
    password_ = password;
    WiFi.persistent(false);  // credentials live in our own NVS namespace
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname("haltestellenanzeige");
    disconnectedSinceMs_ = millis();
    connect();
}

void WifiManager::connect() {
    ++attempts_;
    lastAttemptMs_ = millis();
    Serial.printf("[wifi] connecting to '%s' (attempt %d)\n", ssid_.c_str(), attempts_);
    WiFi.disconnect();
    WiFi.begin(ssid_.c_str(), password_.c_str());
}

void WifiManager::loop() {
    const bool connected = isConnected();
    if (connected && !wasConnected_) {
        Serial.printf("[wifi] connected, IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        everConnected_ = true;
        attempts_ = 0;
    } else if (!connected && wasConnected_) {
        Serial.println("[wifi] connection lost");
        disconnectedSinceMs_ = millis();
    }
    wasConnected_ = connected;

    // The driver reconnects on its own most of the time; if it doesn't,
    // start over periodically.
    if (!connected && millis() - lastAttemptMs_ >= settings::WIFI_RETRY_INTERVAL_MS) {
        connect();
    }
}

bool WifiManager::isConnected() const { return WiFi.status() == WL_CONNECTED; }

uint32_t WifiManager::disconnectedForMs() const {
    return isConnected() ? 0 : millis() - disconnectedSinceMs_;
}
