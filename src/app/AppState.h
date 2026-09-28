#pragma once

// Runtime states of the application. Every state has a defined screen –
// the display is never left black without an explanation.
enum class AppState {
    BOOTING,          // showBoot
    UNCONFIGURED,     // no valid config in NVS -> goes straight to CONFIG_PORTAL
    CONFIG_PORTAL,    // showSetup (access point + web page)
    CONNECTING_WIFI,  // showStatus "Verbinde mit WLAN"
    FETCHING_DATA,    // showStatus "Lade Abfahrten" (first fetch only)
    ONLINE,           // showDepartures, status LIVE
    STALE,            // showDepartures, status VERALTET (old data still shown)
    WIFI_ERROR,       // last data with status KEIN WLAN, or error screen if no data yet
    API_ERROR,        // error screen (no usable data)
};

inline const char* appStateName(AppState s) {
    switch (s) {
        case AppState::BOOTING: return "BOOTING";
        case AppState::UNCONFIGURED: return "UNCONFIGURED";
        case AppState::CONFIG_PORTAL: return "CONFIG_PORTAL";
        case AppState::CONNECTING_WIFI: return "CONNECTING_WIFI";
        case AppState::FETCHING_DATA: return "FETCHING_DATA";
        case AppState::ONLINE: return "ONLINE";
        case AppState::STALE: return "STALE";
        case AppState::WIFI_ERROR: return "WIFI_ERROR";
        case AppState::API_ERROR: return "API_ERROR";
    }
    return "?";
}
