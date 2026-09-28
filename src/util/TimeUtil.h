#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <time.h>

namespace timeutil {

// POSIX TZ string for Europe/Berlin including the DST rules
// (last Sunday in March 02:00 -> CEST, last Sunday in October 03:00 -> CET).
constexpr const char* TZ_EUROPE_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

// Anything before 2024 means "the clock was never set".
constexpr int64_t MIN_VALID_EPOCH = 1704067200;  // 2024-01-01T00:00:00Z

// Parses an RFC 3339 / ISO 8601 timestamp with offset, as sent by the API,
// e.g. "2026-09-25T12:04:00+02:00" or "2026-09-25T10:04:00.123Z".
// Returns Unix epoch seconds (UTC), or 0 if the text can't be parsed.
int64_t parseIso8601(const char* text);

// Whole minutes from `now` until `departure`, rounded down, e.g. 119 s -> 1.
// Negative if the departure is in the past.
int minutesUntil(int64_t departure, int64_t now);

// "HH:MM" in local time (uses the process TZ, set via setenv/configTzTime).
String formatClock(int64_t epoch);

}  // namespace timeutil
