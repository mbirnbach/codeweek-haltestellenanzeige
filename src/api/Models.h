#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <vector>

// Normalized data from the departure API (https://nvv-api.edupixel.de).
// Only the fields the display needs are kept.

struct Stop {
    String id;    // e.g. "2200002" – what gets stored in NVS
    String name;  // e.g. "Kassel Königsplatz"
};

struct Departure {
    String line;
    String destination;
    int64_t scheduled = 0;       // epoch seconds (UTC)
    int64_t estimated = 0;       // epoch seconds, 0 = no realtime prognosis
    String platform;             // estimated platform if known, else scheduled
    bool realtime = false;
    bool cancelled = false;
    bool partiallyCancelled = false;
    String firstMessage;         // first disruption/info text, may be empty

    // Best known departure time: realtime estimate if available.
    int64_t effectiveTime() const { return estimated != 0 ? estimated : scheduled; }
};

struct StationBoard {
    Stop stop;
    int64_t sourceUpdatedAt = 0;  // 0 = unknown
    int64_t servedAt = 0;         // server time when the answer was produced
    bool stale = false;           // backend could not refresh from NVV
    std::vector<Departure> departures;  // sorted by effectiveTime()
};

// Kinds of API failures the app reacts to differently.
enum class ApiError {
    None,
    NotConnected,   // no Wi-Fi
    Network,        // DNS/TLS/timeout/connection failure
    Unauthorized,   // 401: token wrong or revoked
    NotFound,       // 404: stop id unknown
    RateLimited,    // 429
    BadRequest,     // 400: e.g. invalid query
    Server,         // 5xx: upstream_unavailable, data_too_stale, ...
    Parse,          // unexpected JSON
};

struct ApiResult {
    ApiError error = ApiError::None;
    int httpStatus = 0;
    String code;  // API error code, e.g. "data_too_stale" (never contains secrets)

    bool ok() const { return error == ApiError::None; }
};

const char* apiErrorName(ApiError e);
