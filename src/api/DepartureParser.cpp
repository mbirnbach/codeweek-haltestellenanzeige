#include "api/DepartureParser.h"

#include <ArduinoJson.h>

#include <algorithm>

#include "util/TextUtil.h"
#include "util/TimeUtil.h"

namespace parser {

namespace {

String str(JsonVariantConst v, size_t maxChars = 64) {
    const char* s = v | "";
    return text::utf8Truncate(text::sanitize(String(s)), static_cast<int>(maxChars), "");
}

ApiResult parseError() {
    ApiResult r;
    r.error = ApiError::Parse;
    return r;
}

}  // namespace

ApiResult parseDepartures(const char* json, size_t length, StationBoard& out) {
    // Only keep the fields we use; keeps RAM usage low on the ESP32.
    JsonDocument filter;
    filter["stop"]["id"] = true;
    filter["stop"]["name"] = true;
    filter["sourceUpdatedAt"] = true;
    filter["servedAt"] = true;
    filter["stale"] = true;
    JsonObject f = filter["departures"].add<JsonObject>();
    f["line"] = true;
    f["destination"] = true;
    f["scheduled"] = true;
    f["estimated"] = true;
    f["platformScheduled"] = true;
    f["platformEstimated"] = true;
    f["realtime"] = true;
    f["cancelled"] = true;
    f["partiallyCancelled"] = true;
    f["messages"][0] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json, length, DeserializationOption::Filter(filter));
    if (err || !doc.is<JsonObject>()) return parseError();

    JsonArrayConst deps = doc["departures"];
    if (!doc["stop"].is<JsonObjectConst>() || deps.isNull()) return parseError();

    StationBoard board;
    board.stop.id = str(doc["stop"]["id"], 16);
    board.stop.name = str(doc["stop"]["name"]);
    board.sourceUpdatedAt = timeutil::parseIso8601(doc["sourceUpdatedAt"] | "");
    board.servedAt = timeutil::parseIso8601(doc["servedAt"] | "");
    board.stale = doc["stale"] | false;

    for (JsonObjectConst d : deps) {
        if (board.departures.size() >= MAX_DEPARTURES) break;
        Departure dep;
        dep.line = str(d["line"], 8);
        dep.destination = str(d["destination"]);
        dep.scheduled = timeutil::parseIso8601(d["scheduled"] | "");
        dep.estimated = timeutil::parseIso8601(d["estimated"] | "");  // null -> 0
        dep.platform = str(d["platformEstimated"], 6);
        if (dep.platform.length() == 0) dep.platform = str(d["platformScheduled"], 6);
        dep.realtime = d["realtime"] | false;
        dep.cancelled = d["cancelled"] | false;
        dep.partiallyCancelled = d["partiallyCancelled"] | false;
        dep.firstMessage = str(d["messages"][0], 120);

        if (dep.scheduled == 0) continue;  // unusable without a time
        board.departures.push_back(dep);
    }

    std::stable_sort(board.departures.begin(), board.departures.end(),
                     [](const Departure& a, const Departure& b) { return a.effectiveTime() < b.effectiveTime(); });

    out = board;
    return ApiResult{};
}

ApiResult parseStops(const char* json, size_t length, std::vector<Stop>& out) {
    JsonDocument filter;
    filter[0]["id"] = true;
    filter[0]["name"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json, length, DeserializationOption::Filter(filter));
    if (err || !doc.is<JsonArray>()) return parseError();

    out.clear();
    for (JsonObjectConst s : doc.as<JsonArrayConst>()) {
        if (out.size() >= MAX_STOPS) break;
        Stop stop;
        stop.id = str(s["id"], 16);
        stop.name = str(s["name"]);
        if (stop.id.length() == 0 || stop.name.length() == 0) continue;
        out.push_back(stop);
    }
    return ApiResult{};
}

String parseErrorCode(const char* json, size_t length) {
    JsonDocument doc;
    if (deserializeJson(doc, json, length)) return String();
    return str(doc["error"]["code"], 32);
}

ApiError errorFromHttpStatus(int status) {
    if (status == 200) return ApiError::None;
    if (status == 400) return ApiError::BadRequest;
    if (status == 401 || status == 403) return ApiError::Unauthorized;
    if (status == 404) return ApiError::NotFound;
    if (status == 429) return ApiError::RateLimited;
    if (status >= 500) return ApiError::Server;
    if (status <= 0) return ApiError::Network;
    return ApiError::Server;
}

}  // namespace parser

const char* apiErrorName(ApiError e) {
    switch (e) {
        case ApiError::None: return "none";
        case ApiError::NotConnected: return "not_connected";
        case ApiError::Network: return "network";
        case ApiError::Unauthorized: return "unauthorized";
        case ApiError::NotFound: return "not_found";
        case ApiError::RateLimited: return "rate_limited";
        case ApiError::BadRequest: return "bad_request";
        case ApiError::Server: return "server";
        case ApiError::Parse: return "parse";
    }
    return "?";
}
