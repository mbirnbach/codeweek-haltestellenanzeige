#include "config/ConfigPortal.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include <algorithm>

#include "config/AppSettings.h"
#include "config/PortalPage.h"
#include "util/TextUtil.h"

namespace {

const IPAddress AP_IP(192, 168, 4, 1);
constexpr uint32_t WIFI_TEST_TIMEOUT_MS = 20000;
constexpr size_t MAX_NETWORKS = 20;

String makeApName() {
    const uint64_t mac = ESP.getEfuseMac();
    char suffix[5];
    // Last two MAC bytes, so every device in the room has its own name.
    snprintf(suffix, sizeof(suffix), "%02X%02X", static_cast<unsigned>((mac >> 32) & 0xFF),
             static_cast<unsigned>((mac >> 40) & 0xFF));
    return String(settings::SETUP_AP_PREFIX) + suffix;
}

int rssiToQuality(int rssi) {
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return 2 * (rssi + 100);
}

}  // namespace

ConfigPortal::ConfigPortal(DepartureApi& api, ConfigStore& store, Display& display)
    : api_(api), store_(store), display_(display) {}

void ConfigPortal::begin(const DeviceConfig& current) {
    current_ = current;
    pending_ = current;
    pending_.configured = false;
    apName_ = makeApName();
    lastActivityMs_ = millis();

    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    const char* apPassword = settings::SETUP_AP_PASSWORD[0] ? settings::SETUP_AP_PASSWORD : nullptr;
    WiFi.softAP(apName_.c_str(), apPassword);
    Serial.printf("[portal] access point '%s' started, open http://%s\n", apName_.c_str(),
                  AP_IP.toString().c_str());

    // Captive portal: answer every DNS name with our own address.
    dns_.setErrorReplyCode(DNSReplyCode::NoError);
    dns_.start(53, "*", AP_IP);

    setupRoutes();
    server_.begin();

    // Reconfiguring an already set-up device: try the saved Wi-Fi right away,
    // so changing only the stop needs no password.
    if (config_rules::isValidSsid(current_.wifiSsid)) {
        WiFi.begin(current_.wifiSsid.c_str(), current_.wifiPassword.c_str());
        wifiTest_ = WifiTest::Connecting;
        wifiTestStartedMs_ = millis();
    }
    showStep("Warte auf Handy ...");
}

void ConfigPortal::loop() {
    dns_.processNextRequest();
    server_.handleClient();
    updateWifiTest();
}

bool ConfigPortal::idleTimedOut() const {
    return current_.isComplete() && !finished_ && millis() - lastActivityMs_ > settings::PORTAL_IDLE_TIMEOUT_MS;
}

void ConfigPortal::showStep(const String& step) {
    if (step == step_) return;
    step_ = step;
    SetupScreen s;
    s.apName = apName_;
    s.url = String("http://") + AP_IP.toString();
    s.step = step;
    display_.showSetup(s);
}

void ConfigPortal::updateWifiTest() {
    if (wifiTest_ != WifiTest::Connecting) return;
    const wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
        wifiTest_ = WifiTest::Connected;
        Serial.printf("[portal] Wi-Fi test ok, IP %s\n", WiFi.localIP().toString().c_str());
        showStep("WLAN ok - Haltestelle waehlen");
        return;
    }
    const uint32_t elapsed = millis() - wifiTestStartedMs_;
    // WL_CONNECT_FAILED / WL_NO_SSID_AVAIL can show up briefly while
    // connecting, so only trust them after a few seconds.
    if ((elapsed > 5000 && (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL)) || elapsed > WIFI_TEST_TIMEOUT_MS) {
        wifiTest_ = WifiTest::Failed;
        WiFi.disconnect(false);  // keep the access point running
        Serial.printf("[portal] Wi-Fi test failed (status %d)\n", static_cast<int>(st));
        showStep("WLAN fehlgeschlagen");
    }
}

// ---------------------------------------------------------------------------
// HTTP routes
// ---------------------------------------------------------------------------

void ConfigPortal::setupRoutes() {
    server_.on("/", HTTP_GET, [this] { handleRoot(); });
    server_.on("/config/state", HTTP_GET, [this] { handleState(); });
    server_.on("/config/wifi/scan", HTTP_GET, [this] { handleScan(); });
    server_.on("/config/wifi", HTTP_POST, [this] { handleWifiConnect(); });
    server_.on("/config/wifi/status", HTTP_GET, [this] { handleWifiStatus(); });
    server_.on("/config/stops", HTTP_GET, [this] { handleStopSearch(); });
    server_.on("/config/save", HTTP_POST, [this] { handleSave(); });
    // Everything else (including the OS captive-portal checks like
    // /generate_204 or /hotspot-detect.html) goes to the setup page.
    server_.onNotFound([this] { handleCaptiveRedirect(); });
}

void ConfigPortal::sendJson(int status, const String& json) {
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(status, "application/json; charset=utf-8", json);
}

void ConfigPortal::sendError(int status, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    String out;
    serializeJson(doc, out);
    sendJson(status, out);
}

void ConfigPortal::handleCaptiveRedirect() {
    lastActivityMs_ = millis();
    server_.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
    server_.send(302, "text/plain", "");
}

void ConfigPortal::handleRoot() {
    lastActivityMs_ = millis();
    showStep("Handy verbunden");
    server_.sendHeader("Cache-Control", "no-store");
    server_.send_P(200, "text/html; charset=utf-8", PORTAL_PAGE);
}

void ConfigPortal::handleState() {
    lastActivityMs_ = millis();
    JsonDocument doc;
    doc["ap"] = apName_;
    doc["version"] = settings::FIRMWARE_VERSION;
    doc["ssid"] = pending_.wifiSsid;
    doc["hasPassword"] = pending_.wifiPassword.length() > 0;  // never the password itself
    doc["stopName"] = current_.stopName;
    switch (wifiTest_) {
        case WifiTest::Idle: doc["wifi"] = "idle"; break;
        case WifiTest::Connecting: doc["wifi"] = "connecting"; break;
        case WifiTest::Connected: doc["wifi"] = "connected"; break;
        case WifiTest::Failed: doc["wifi"] = "failed"; break;
    }
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void ConfigPortal::handleScan() {
    lastActivityMs_ = millis();
    const int16_t result = WiFi.scanComplete();

    if (result >= 0) {
        networks_.clear();
        for (int i = 0; i < result; ++i) {
            const String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) continue;  // hidden network
            const int rssi = WiFi.RSSI(i);
            auto it = std::find_if(networks_.begin(), networks_.end(), [&](const Network& n) { return n.ssid == ssid; });
            if (it != networks_.end()) {
                it->rssi = std::max(it->rssi, rssi);
            } else {
                networks_.push_back({ssid, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
            }
        }
        WiFi.scanDelete();
        std::sort(networks_.begin(), networks_.end(), [](const Network& a, const Network& b) { return a.rssi > b.rssi; });
        if (networks_.size() > MAX_NETWORKS) networks_.resize(MAX_NETWORKS);
        scanRunning_ = false;
        Serial.printf("[portal] scan found %u networks\n", static_cast<unsigned>(networks_.size()));
    } else if (!scanRunning_ && wifiTest_ != WifiTest::Connecting) {
        // Not started yet (or failed): start an async scan; the page polls.
        if (WiFi.scanNetworks(/*async=*/true) == WIFI_SCAN_RUNNING) scanRunning_ = true;
    }

    JsonDocument doc;
    doc["scanning"] = scanRunning_;
    JsonArray arr = doc["networks"].to<JsonArray>();
    for (const Network& n : networks_) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = n.ssid;
        o["quality"] = rssiToQuality(n.rssi);
        o["secure"] = n.secure;
    }
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void ConfigPortal::handleWifiConnect() {
    lastActivityMs_ = millis();
    String ssid = server_.arg("ssid");
    String password = server_.arg("password");
    ssid.trim();

    // Empty password field + same network as before = keep saved password.
    if (password.length() == 0 && ssid == current_.wifiSsid) password = current_.wifiPassword;

    if (!config_rules::isValidSsid(ssid)) {
        sendError(400, "Der WLAN-Name ist ungültig.");
        return;
    }
    if (!config_rules::isValidWifiPassword(password)) {
        sendError(400, "Das WLAN-Passwort muss 8 bis 63 Zeichen lang sein (oder leer bei offenem WLAN).");
        return;
    }

    pending_.wifiSsid = ssid;
    pending_.wifiPassword = password;

    Serial.printf("[portal] testing Wi-Fi '%s'\n", ssid.c_str());  // never log the password
    showStep("Verbinde mit WLAN ...");
    WiFi.disconnect(false);
    WiFi.begin(ssid.c_str(), password.c_str());
    wifiTest_ = WifiTest::Connecting;
    wifiTestStartedMs_ = millis();

    sendJson(200, "{\"status\":\"connecting\"}");
}

void ConfigPortal::handleWifiStatus() {
    lastActivityMs_ = millis();
    JsonDocument doc;
    doc["ssid"] = pending_.wifiSsid;
    switch (wifiTest_) {
        case WifiTest::Idle: doc["status"] = "idle"; break;
        case WifiTest::Connecting: doc["status"] = "connecting"; break;
        case WifiTest::Connected: doc["status"] = "connected"; break;
        case WifiTest::Failed:
            doc["status"] = "failed";
            doc["message"] = "Verbindung fehlgeschlagen. Stimmen WLAN-Name und Passwort? Ist das WLAN in Reichweite?";
            break;
    }
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void ConfigPortal::handleStopSearch() {
    lastActivityMs_ = millis();
    String q = server_.arg("q");
    q.trim();
    const int len = text::utf8Length(q);
    if (len < 2 || len > 64) {
        sendError(400, "Bitte 2 bis 64 Zeichen eingeben.");
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        sendError(409, "Erst mit dem WLAN verbinden (Schritt 1).");
        return;
    }

    showStep("Suche Haltestelle ...");
    std::vector<Stop> stops;
    // The ESP calls the API with the token; the browser only sees id + name.
    const ApiResult r = api_.searchStops(q, stops);
    if (!r.ok()) {
        showStep("Suche fehlgeschlagen");
        switch (r.error) {
            case ApiError::Unauthorized:
                sendError(502, "Der Server lehnt das Gerät ab (API-Zugang). Bitte Betreuer fragen.");
                break;
            case ApiError::BadRequest:
                sendError(400, "Diese Suche versteht der Server nicht. Anders schreiben?");
                break;
            case ApiError::RateLimited:
                sendError(503, "Zu viele Anfragen. Bitte kurz warten.");
                break;
            default:
                sendError(502, "Der Abfahrts-Server ist gerade nicht erreichbar. Hat das WLAN Internet?");
                break;
        }
        return;
    }
    showStep("Haltestelle antippen");

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const Stop& s : stops) {
        JsonObject o = arr.add<JsonObject>();
        o["id"] = s.id;
        o["name"] = s.name;
    }
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void ConfigPortal::handleSave() {
    lastActivityMs_ = millis();
    if (wifiTest_ != WifiTest::Connected) {
        sendError(409, "Erst mit dem WLAN verbinden (Schritt 1).");
        return;
    }
    String stopId = server_.arg("stop_id");
    String stopName = server_.arg("stop_name");
    stopId.trim();
    stopName.trim();
    if (!config_rules::isValidStopId(stopId) || !config_rules::isValidStopName(stopName)) {
        sendError(400, "Bitte eine Haltestelle aus der Liste antippen.");
        return;
    }

    pending_.stopId = stopId;
    pending_.stopName = text::sanitize(stopName);
    pending_.configured = true;

    if (!pending_.isComplete() || !store_.save(pending_)) {
        sendError(500, "Speichern fehlgeschlagen. Bitte nochmal versuchen.");
        return;
    }
    Serial.printf("[portal] saved: Wi-Fi '%s', stop %s '%s'\n", pending_.wifiSsid.c_str(), pending_.stopId.c_str(),
                  pending_.stopName.c_str());
    showStep("Gespeichert - Neustart ...");
    sendJson(200, "{\"status\":\"saved\"}");
    finished_ = true;
    finishedAtMs_ = millis();
}
