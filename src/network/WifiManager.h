#pragma once

#include <Arduino.h>

// Station-mode Wi-Fi for normal operation: connect, notice drops, retry.
// Never blocks.
class WifiManager {
public:
    void begin(const String& ssid, const String& password);
    void loop();

    bool isConnected() const;
    // How long the connection has been down (0 while connected).
    uint32_t disconnectedForMs() const;
    // True once a connection succeeded since boot (helps to tell
    // "wrong password" from "router temporarily away").
    bool hasEverConnected() const { return everConnected_; }
    const String& ssid() const { return ssid_; }
    int attempts() const { return attempts_; }

private:
    void connect();

    String ssid_;
    String password_;
    uint32_t lastAttemptMs_ = 0;
    uint32_t disconnectedSinceMs_ = 0;
    bool wasConnected_ = false;
    bool everConnected_ = false;
    int attempts_ = 0;
};
