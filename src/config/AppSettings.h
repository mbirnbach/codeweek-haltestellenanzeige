#pragma once

#include <stdint.h>

// Fixed firmware settings for maintainers (not for workshop participants).

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "include/secrets.h fehlt: kopiere include/secrets.example.h nach include/secrets.h und trage den API-Token ein."
#endif

#ifndef API_BASE_URL
#error "API_BASE_URL is missing in include/secrets.h"
#endif
#ifndef API_TOKEN
#error "API_TOKEN is missing in include/secrets.h"
#endif

namespace settings {

constexpr const char* FIRMWARE_VERSION = "1.0.0";

// --- Departure API ----------------------------------------------------------
constexpr const char* API_URL = API_BASE_URL;
constexpr uint32_t API_TIMEOUT_MS = 10000;
// The backend refreshes its NVV cache about every 60 s; polling faster
// doesn't give fresher data.
constexpr uint32_t POLL_INTERVAL_MS = 60000;
// Retry after a failed fetch: 10 s, 20 s, 40 s, ... capped at 2 min.
constexpr uint32_t RETRY_MIN_MS = 10000;
constexpr uint32_t RETRY_MAX_MS = 120000;
// Departures requested per fetch. More than DEPARTURE_COUNT, so rows can
// be filled locally as buses leave between two fetches.
constexpr int FETCH_LIMIT = 10;
// Our own fetches failing for this long -> show data as stale.
constexpr uint32_t STALE_AFTER_MS = 3 * 60000;

// --- Wi-Fi --------------------------------------------------------------------
// After this long without a connection at startup, show the Wi-Fi error
// screen (retrying continues in the background).
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
// While disconnected, restart the connection attempt this often.
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 30000;  // weak signal can need >15 s

// --- Setup portal ------------------------------------------------------------
constexpr const char* SETUP_AP_PREFIX = "Abfahrt-Setup-";
// Empty = open network (simplest for the workshop).
constexpr const char* SETUP_AP_PASSWORD = "";
// If setup was opened on an already configured device and nobody uses it,
// return to normal operation after this time.
constexpr uint32_t PORTAL_IDLE_TIMEOUT_MS = 10 * 60000;

// --- Time -------------------------------------------------------------------
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.google.com";
constexpr const char* NTP_SERVER_3 = "ptbtime1.ptb.de";

// --- Hardware-independent reconfigure trigger ------------------------------------
// The ESP32 BOOT button (GPIO0) exists on practically every ESP32 board,
// including the CYD. Holding it for SETUP_HOLD_MS opens the setup portal.
// TODO(HARDWARE): verify the BOOT button is reachable in the enclosure.
constexpr int SETUP_BUTTON_PIN = 0;
constexpr uint32_t SETUP_HOLD_MS = 3000;

}  // namespace settings
