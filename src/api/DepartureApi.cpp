#include "api/DepartureApi.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "api/DepartureParser.h"
#include "api/RootCertificates.h"
#include "util/TextUtil.h"

namespace {
// Largest response we accept (limit=20 departures is ~10 KB).
constexpr int MAX_BODY_BYTES = 32 * 1024;
}  // namespace

DepartureApi::DepartureApi(const char* baseUrl, const char* token, uint32_t timeoutMs)
    : baseUrl_(baseUrl), token_(token), timeoutMs_(timeoutMs) {}

ApiResult DepartureApi::fetchDepartures(const String& stopId, int limit, StationBoard& out) {
    String body;
    ApiResult r = get(String("/v1/stops/") + text::urlEncode(stopId) + "/departures?limit=" + String(limit), body);
    if (!r.ok()) return r;
    r = parser::parseDepartures(body.c_str(), body.length(), out);
    if (!r.ok()) Serial.println("[api] departures: unexpected JSON");
    return r;
}

ApiResult DepartureApi::searchStops(const String& query, std::vector<Stop>& out) {
    String body;
    ApiResult r = get(String("/v1/stops?query=") + text::urlEncode(query), body);
    if (!r.ok()) return r;
    r = parser::parseStops(body.c_str(), body.length(), out);
    if (!r.ok()) Serial.println("[api] stops: unexpected JSON");
    return r;
}

ApiResult DepartureApi::get(const String& pathAndQuery, String& body) {
    ApiResult result;
    if (WiFi.status() != WL_CONNECTED) {
        result.error = ApiError::NotConnected;
        return result;
    }

    WiFiClientSecure client;
    client.setCACert(API_ROOT_CA_BUNDLE);
    client.setTimeout((timeoutMs_ + 999) / 1000);  // seconds

    HTTPClient http;
    http.setTimeout(timeoutMs_);
    http.setConnectTimeout(timeoutMs_);
    http.setReuse(false);

    // Only the path is logged – never headers, never the token.
    const String url = String(baseUrl_) + pathAndQuery;
    Serial.printf("[api] GET %s\n", pathAndQuery.c_str());
    const uint32_t started = millis();

    if (!http.begin(client, url)) {
        result.error = ApiError::Network;
        return result;
    }
    http.addHeader("Authorization", String("Bearer ") + token_);
    http.addHeader("Accept", "application/json");

    const int status = http.GET();
    result.httpStatus = status;

    if (status <= 0) {
        // Negative values are HTTPClient/TLS errors (connection, timeout, ...).
        Serial.printf("[api] request failed: %s\n", HTTPClient::errorToString(status).c_str());
        result.error = ApiError::Network;
        http.end();
        return result;
    }

    const int size = http.getSize();  // -1 if unknown (chunked)
    if (size > MAX_BODY_BYTES) {
        Serial.printf("[api] response too large (%d bytes)\n", size);
        result.error = ApiError::Parse;
        http.end();
        return result;
    }
    body = http.getString();
    http.end();

    Serial.printf("[api] HTTP %d, %u bytes, %lu ms\n", status, body.length(),
                  static_cast<unsigned long>(millis() - started));

    if (status != 200) {
        result.error = parser::errorFromHttpStatus(status);
        result.code = parser::parseErrorCode(body.c_str(), body.length());
        Serial.printf("[api] error: %s (%s)\n", apiErrorName(result.error), result.code.c_str());
        body = String();
    }
    return result;
}
