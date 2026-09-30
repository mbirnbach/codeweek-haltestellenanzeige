#include "app/AppController.h"

#include <WiFi.h>

#include "app/DisplayModelBuilder.h"
#include "config/AppSettings.h"
#include "student/config_checks.h"

namespace {

// Rendering is cheap; once per second keeps the countdown and clock exact.
constexpr uint32_t RENDER_INTERVAL_MS = 1000;
// Some hint must always be visible on error screens.
constexpr const char* SETUP_HINT = "BOOT-Taste 3 s halten = Einrichtung";

// Set before a software restart to open the setup portal on the next boot.
// RTC memory survives ESP.restart(), but not a power cycle.
RTC_NOINIT_ATTR uint32_t setupRequest;
constexpr uint32_t SETUP_REQUEST_MAGIC = 0x5E7A9B01;

}  // namespace

AppController::AppController(Display& display)
    : display_(display),
      api_(settings::API_URL, API_TOKEN, settings::API_TIMEOUT_MS),
      portal_(api_, store_, display) {}

void AppController::begin() {
    pinMode(settings::SETUP_BUTTON_PIN, INPUT_PULLUP);
    display_.begin();
    display_.showBoot(settings::FIRMWARE_VERSION);
    setState(AppState::BOOTING);

    const bool setupRequested = setupRequest == SETUP_REQUEST_MAGIC;
    setupRequest = 0;

    config_ = store_.load();
    if (!config_.isComplete() || setupRequested) {
        Serial.println(setupRequested ? "[app] setup requested -> setup portal"
                                      : "[app] no valid configuration -> setup portal");
        setState(AppState::UNCONFIGURED);
        setState(AppState::CONFIG_PORTAL);
        portal_.begin(config_);
        return;
    }
    Serial.printf("[app] stop %s '%s', Wi-Fi '%s'\n", config_.stopId.c_str(), config_.stopName.c_str(),
                  config_.wifiSsid.c_str());
    startNormalOperation();
}

void AppController::startNormalOperation() {
    setState(AppState::CONNECTING_WIFI);
    wifi_.begin(config_.wifiSsid, config_.wifiPassword);
    time_.begin();
    render(true);
}

void AppController::loop() {
    handleSerialCommand();
    display_.loop();

    if (state_ == AppState::CONFIG_PORTAL) {
        portal_.loop();
        if (portal_.finished()) {
            Serial.println("[app] setup finished, restarting");
            delay(200);
            ESP.restart();
        }
        if (portal_.idleTimedOut()) {
            Serial.println("[app] setup portal unused, back to normal operation");
            ESP.restart();
        }
        return;
    }

    checkSetupTriggers();
    loopNormal();
}

void AppController::setState(AppState s) {
    if (s == state_) return;
    Serial.printf("[app] state %s -> %s\n", appStateName(state_), appStateName(s));
    state_ = s;
}

// ---------------------------------------------------------------------------
// Normal operation
// ---------------------------------------------------------------------------

void AppController::loopNormal() {
    wifi_.loop();
    const uint32_t now = millis();

    if (!wifi_.isConnected()) {
        if (state_ != AppState::CONNECTING_WIFI) setState(AppState::WIFI_ERROR);
        // At startup, show "connecting" for a while before calling it an error.
        if (state_ == AppState::CONNECTING_WIFI && wifi_.disconnectedForMs() > settings::WIFI_CONNECT_TIMEOUT_MS) {
            setState(AppState::WIFI_ERROR);
        }
        render();
        return;
    }

    if (state_ == AppState::CONNECTING_WIFI || state_ == AppState::WIFI_ERROR) {
        setState(hasData() ? AppState::STALE : AppState::FETCHING_DATA);
        render(true);
        nextFetchMs_ = now;  // fetch immediately after (re)connecting
    }

    if (static_cast<int32_t>(now - nextFetchMs_) >= 0) fetchNow();

    render();
}

void AppController::fetchNow() {
    if (!hasData()) {
        setState(AppState::FETCHING_DATA);
        render(true);
    }

    StationBoard fresh;
    const ApiResult r = api_.fetchDepartures(config_.stopId, settings::FETCH_LIMIT, fresh);
    if (!r.ok()) {
        onFetchFailed(r);
        return;
    }

    time_.setFromServerIfUnsynced(fresh.servedAt);
    board_ = fresh;
    // Keep the name chosen in setup; the API name is only a fallback.
    if (config_.stopName.length() > 0) board_.stop.name = config_.stopName;
    lastSuccessMs_ = millis();
    lastError_ = ApiResult{};
    failedFetches_ = 0;
    retryDelayMs_ = 0;
    nextFetchMs_ = millis() + settings::POLL_INTERVAL_MS;
    Serial.printf("[app] %u departures%s\n", static_cast<unsigned>(board_.departures.size()),
                  board_.stale ? " (backend says: stale)" : "");

    setState(board_.stale ? AppState::STALE : AppState::ONLINE);
    render(true);
}

void AppController::onFetchFailed(const ApiResult& r) {
    lastError_ = r;
    ++failedFetches_;

    // Bounded exponential backoff: 10 s, 20 s, 40 s, ... max 2 min.
    retryDelayMs_ = retryDelayMs_ == 0 ? settings::RETRY_MIN_MS : retryDelayMs_ * 2;
    if (retryDelayMs_ > settings::RETRY_MAX_MS) retryDelayMs_ = settings::RETRY_MAX_MS;
    // A rejected token or unknown stop won't fix itself quickly.
    if (r.error == ApiError::Unauthorized || r.error == ApiError::NotFound) retryDelayMs_ = settings::RETRY_MAX_MS;
    nextFetchMs_ = millis() + retryDelayMs_;
    Serial.printf("[app] fetch failed (%s), retry in %lu s\n", apiErrorName(r.error),
                  static_cast<unsigned long>(retryDelayMs_ / 1000));

    if (r.error == ApiError::NotConnected) {
        setState(AppState::WIFI_ERROR);
    } else {
        // Old data stays on screen (marked stale); without data: error screen.
        setState(hasData() ? AppState::STALE : AppState::API_ERROR);
    }
    render(true);
}

BoardStatus AppController::boardStatus() const {
    if (state_ == AppState::WIFI_ERROR || state_ == AppState::CONNECTING_WIFI) return BoardStatus::Offline;
    if (board_.stale || failedFetches_ > 0 || millis() - lastSuccessMs_ > settings::STALE_AFTER_MS) {
        return BoardStatus::Stale;
    }
    return BoardStatus::Live;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void AppController::render(bool force) {
    const uint32_t now = millis();
    if (!force && now - lastRenderMs_ < RENDER_INTERVAL_MS) return;
    lastRenderMs_ = now;

    // Useful data wins: keep showing it whatever else is wrong.
    if (hasData() && state_ != AppState::API_ERROR) {
        const DisplayModel m = buildDisplayModel(board_, time_.now(), boardStatus());
        if (!boardShown_ || m != shownModel_) {
            display_.showDepartures(m);
            shownModel_ = m;
            boardShown_ = true;
            shownScreenKey_ = "";
        }
        return;
    }
    boardShown_ = false;

    StatusScreen s;
    switch (state_) {
        case AppState::BOOTING:
        case AppState::UNCONFIGURED:
        case AppState::CONFIG_PORTAL:
            return;  // boot and setup screens are drawn elsewhere
        case AppState::CONNECTING_WIFI:
            s.title = "Verbinde mit WLAN";
            s.detail = String("\"") + config_.wifiSsid + "\"";
            s.hint = "Bitte kurz warten ...";
            break;
        case AppState::FETCHING_DATA:
            s.title = "Lade Abfahrten";
            s.detail = config_.stopName;
            s.hint = "Bitte kurz warten ...";
            break;
        case AppState::WIFI_ERROR:
            s.isError = true;
            s.title = "Kein WLAN";
            s.detail = String("\"") + config_.wifiSsid + "\" nicht erreichbar";
            s.hint = wifi_.hasEverConnected() ? String("Versuche es weiter ...") : String(SETUP_HINT);
            break;
        case AppState::API_ERROR:
        case AppState::ONLINE:
        case AppState::STALE:
            showErrorScreen();
            return;
    }
    const String key = s.title + "|" + s.detail + "|" + s.hint;
    if (force || key != shownScreenKey_) {
        display_.showStatus(s);
        shownScreenKey_ = key;
    }
}

void AppController::showErrorScreen() {
    StatusScreen s;
    s.isError = true;
    s.title = "Keine Abfahrtsdaten";
    switch (lastError_.error) {
        case ApiError::Unauthorized:
            s.detail = "Server lehnt Zugang ab (API-Token)";
            s.hint = "Bitte Betreuer fragen";
            break;
        case ApiError::NotFound:
            s.detail = "Haltestelle unbekannt";
            s.hint = SETUP_HINT;
            break;
        case ApiError::Network:
            s.detail = "Server nicht erreichbar";
            s.hint = "Hat das WLAN Internet? Neuer Versuch ...";
            break;
        case ApiError::RateLimited:
            s.detail = "Zu viele Anfragen";
            s.hint = "Neuer Versuch gleich ...";
            break;
        default:
            s.detail = "Server hat gerade keine Daten";
            s.hint = "Neuer Versuch gleich ...";
            break;
    }
    const String key = s.title + "|" + s.detail;
    if (key != shownScreenKey_) {
        display_.showStatus(s);
        shownScreenKey_ = key;
    }
}

// ---------------------------------------------------------------------------
// Reconfiguration
// ---------------------------------------------------------------------------

void AppController::checkSetupTriggers() {
    // BOOT button held for SETUP_HOLD_MS (works without any display input).
    if (digitalRead(settings::SETUP_BUTTON_PIN) == LOW) {
        if (buttonDownSinceMs_ == 0) {
            buttonDownSinceMs_ = millis();
            Serial.println("[app] BOOT button pressed (hold 3 s for setup)");
        }
        if (millis() - buttonDownSinceMs_ >= settings::SETUP_HOLD_MS) {
            Serial.println("[app] BOOT button held -> setup");
            openSetup();
        }
    } else {
        buttonDownSinceMs_ = 0;
    }

    // Display-specific gesture (e.g. long touch on the CYD).
    if (display_.setupGestureDetected()) openSetup();
}

void AppController::openSetup() {
    // Restart and open the portal on the clean boot path (same as the very
    // first setup) instead of switching the running Wi-Fi from station to
    // access point mode. The saved values stay (prefilled in the portal);
    // nothing is lost if setup is abandoned.
    Serial.println("[app] restarting into setup portal");
    StatusScreen s;
    s.title = "Einrichtung";
    s.detail = "Startet neu ...";
    display_.showStatus(s);
    setupRequest = SETUP_REQUEST_MAGIC;
    Serial.flush();
    delay(300);
    ESP.restart();
}

void AppController::factoryReset() {
    Serial.println("[app] clearing configuration");
    store_.clear();
    delay(100);
    ESP.restart();
}

void AppController::handleSerialCommand() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c != '\n' && c != '\r') {
            if (serialLine_.length() < 32) serialLine_ += c;
            continue;
        }
        String cmd = serialLine_;
        serialLine_ = "";
        cmd.trim();
        if (cmd.length() == 0) continue;

        if (cmd == "setup") {
            openSetup();
        } else if (cmd == "reset") {
            factoryReset();
        } else if (cmd == "fetch") {
            nextFetchMs_ = millis();
        } else if (cmd == "status") {
            Serial.printf("[app] state=%s wifi=%s stop=%s (%s) lastError=%s failures=%d heap=%u\n",
                          appStateName(state_), wifi_.isConnected() ? "connected" : "disconnected",
                          config_.stopId.c_str(), config_.stopName.c_str(), apiErrorName(lastError_.error),
                          failedFetches_, static_cast<unsigned>(ESP.getFreeHeap()));
            boardShown_ = false;  // re-print the board
        } else {
            Serial.println("[app] commands: status | fetch | setup | reset");
        }
    }
}
