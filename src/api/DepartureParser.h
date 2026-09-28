#pragma once

#include <stddef.h>

#include "api/Models.h"

// JSON -> model parsing for the departure API. Pure functions without any
// network or hardware dependency, so they are unit-tested on the host
// against captured real responses (test/fixtures/).

namespace parser {

// Upper bound for departures kept from one response (API maximum is 20).
constexpr size_t MAX_DEPARTURES = 20;
// Upper bound for stop search results sent to the setup portal.
constexpr size_t MAX_STOPS = 15;

// GET /v1/stops/{id}/departures
ApiResult parseDepartures(const char* json, size_t length, StationBoard& out);

// GET /v1/stops?query=...
ApiResult parseStops(const char* json, size_t length, std::vector<Stop>& out);

// {"error":{"code":"...","message":"..."}} -> "..." (empty if not present)
String parseErrorCode(const char* json, size_t length);

// Maps an HTTP status (other than 200) to an error kind.
ApiError errorFromHttpStatus(int status);

}  // namespace parser
