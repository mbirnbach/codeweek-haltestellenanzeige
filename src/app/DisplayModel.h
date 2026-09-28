#pragma once

#include <Arduino.h>

// What the departure screen shows, already decided and formatted.
// Every display backend (Serial, CYD) renders exactly this – no backend
// makes its own decisions about which departures to show or how.

constexpr int MAX_DEPARTURE_ROWS = 6;

// Longest text in the time column ("sofort", "12 min", "AUSFALL").
constexpr int MAX_TIME_CHARS = 8;

enum class BoardStatus : uint8_t {
    Live,     // fresh data
    Stale,    // data is old: backend couldn't reach NVV, or our fetch failed
    Offline,  // no Wi-Fi at the moment, showing the last known data
};

struct DepartureRow {
    String line;         // "4", "RT1"
    String destination;  // "Druseltal"
    String platform;     // "A", may be empty
    String time;         // "3 min", "sofort", "AUSFALL"
    bool cancelled = false;

    bool operator==(const DepartureRow& o) const {
        return line == o.line && destination == o.destination && platform == o.platform && time == o.time &&
               cancelled == o.cancelled;
    }
    bool operator!=(const DepartureRow& o) const { return !(*this == o); }
};

struct DisplayModel {
    String stopName;
    String clock;  // "11:42", empty = don't show
    bool showPlatform = false;

    DepartureRow rows[MAX_DEPARTURE_ROWS];
    int rowCount = 0;  // 0 = "Keine Abfahrten"

    BoardStatus status = BoardStatus::Live;
    String statusText;  // empty when live; "VERALTET · Stand 11:40", "KEIN WLAN · ..."

    bool operator==(const DisplayModel& o) const {
        if (stopName != o.stopName || clock != o.clock || showPlatform != o.showPlatform ||
            rowCount != o.rowCount || status != o.status || statusText != o.statusText) {
            return false;
        }
        for (int i = 0; i < rowCount; ++i) {
            if (rows[i] != o.rows[i]) return false;
        }
        return true;
    }
    bool operator!=(const DisplayModel& o) const { return !(*this == o); }
};

// Full-screen message for every state without a departure board
// (booting, connecting, errors). Never leave the screen black.
struct StatusScreen {
    String title;   // "Kein WLAN"
    String detail;  // "Verbinde mit 'Schule-WLAN' ..."
    String hint;    // "BOOT-Taste 3 s halten = Setup"
    bool isError = false;
};

// Shown while the setup portal is running.
struct SetupScreen {
    String apName;  // Wi-Fi network to join with the phone
    String url;     // "http://192.168.4.1"
    String step;    // what's happening right now
};
