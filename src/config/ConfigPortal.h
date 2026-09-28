#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include <vector>

#include "api/DepartureApi.h"
#include "config/ConfigStore.h"
#include "display/Display.h"

// Setup portal: the ESP opens its own Wi-Fi network ("Abfahrt-Setup-XXXX"),
// a phone joins it and gets a small web page (captive portal) to
//   1. choose the local Wi-Fi and test the connection (AP+STA mode),
//   2. search a stop – the ESP proxies the search to the API, so the API
//      token never reaches the browser,
//   3. save everything to NVS.
// After saving, the caller restarts the device into normal operation.
class ConfigPortal {
public:
    ConfigPortal(DepartureApi& api, ConfigStore& store, Display& display);

    void begin(const DeviceConfig& current);
    void loop();

    // Configuration saved; restart soon (after the HTTP answer went out).
    bool finished() const { return finished_ && millis() - finishedAtMs_ > 1500; }
    // Opened on an already configured device and unused for a long time.
    bool idleTimedOut() const;

private:
    enum class WifiTest { Idle, Connecting, Connected, Failed };
    struct Network {
        String ssid;
        int rssi;
        bool secure;
    };

    void setupRoutes();
    void handleRoot();
    void handleState();
    void handleScan();
    void handleWifiConnect();
    void handleWifiStatus();
    void handleStopSearch();
    void handleSave();
    void handleCaptiveRedirect();

    void sendJson(int status, const String& json);
    void sendError(int status, const char* message);
    void updateWifiTest();
    void showStep(const String& step);

    DepartureApi& api_;
    ConfigStore& store_;
    Display& display_;
    WebServer server_{80};
    DNSServer dns_;

    DeviceConfig current_;   // config at portal start (may be empty)
    DeviceConfig pending_;   // what will be saved
    String apName_;
    String step_;

    WifiTest wifiTest_ = WifiTest::Idle;
    uint32_t wifiTestStartedMs_ = 0;
    std::vector<Network> networks_;
    bool scanRunning_ = false;

    uint32_t lastActivityMs_ = 0;
    bool finished_ = false;
    uint32_t finishedAtMs_ = 0;
};
