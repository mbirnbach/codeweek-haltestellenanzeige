#pragma once

#include "api/Models.h"
#include "app/DisplayModel.h"

// Turns API data + current time into the DisplayModel. Pure function
// (no hardware, no network), unit-tested on the host.
//
// - picks the next DEPARTURE_COUNT departures (student setting)
// - drops departures that have already left since the last fetch
// - cancelled departures are shown as "AUSFALL", never as a normal time
// - minute text comes from the student's formatMinutes()
//
// `now` is epoch seconds; values before 2024 mean "clock unknown".
DisplayModel buildDisplayModel(const StationBoard& board, int64_t now, BoardStatus status);

// The time column text for one departure (also used by tests).
String departureTimeText(const Departure& dep, int64_t now);
