#pragma once

#include "api/Models.h"

// HTTPS client for the departure API (https://nvv-api.edupixel.de).
// Adds the bearer token to every /v1 request. The token is never logged
// and never leaves this class.
class DepartureApi {
public:
    DepartureApi(const char* baseUrl, const char* token, uint32_t timeoutMs);

    // GET /v1/stops/{stopId}/departures?limit={limit}
    ApiResult fetchDepartures(const String& stopId, int limit, StationBoard& out);

    // GET /v1/stops?query={query}
    ApiResult searchStops(const String& query, std::vector<Stop>& out);

private:
    ApiResult get(const String& pathAndQuery, String& body);

    const char* baseUrl_;
    const char* token_;
    uint32_t timeoutMs_;
};
