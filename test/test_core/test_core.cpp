// Host unit tests for the hardware-independent core:
//   pio test -e native
//
// Expectations that depend on participant settings (DEPARTURE_COUNT,
// formatMinutes, ...) are computed from those settings, so the tests keep
// passing after the workshop changes.

#include <unity.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "api/DepartureParser.h"
#include "app/DisplayModelBuilder.h"
#include "config/DeviceConfig.h"
#include "display/BoardFormatter.h"
#include "student/config_checks.h"
#include "student/student_code.h"
#include "util/TextUtil.h"
#include "util/TimeUtil.h"

namespace {

std::string fixture(const char* name) {
    std::string dir = __FILE__;
    dir = dir.substr(0, dir.find_last_of('/'));  // test/test_core
    std::ifstream in(dir + "/../fixtures/" + name);
    TEST_ASSERT_TRUE_MESSAGE(in.good(), name);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

StationBoard parseFixture(const char* name) {
    const std::string json = fixture(name);
    StationBoard board;
    const ApiResult r = parser::parseDepartures(json.c_str(), json.size(), board);
    TEST_ASSERT_TRUE_MESSAGE(r.ok(), name);
    return board;
}

void assertEq(const char* expected, const String& actual) { TEST_ASSERT_EQUAL_STRING(expected, actual.c_str()); }
void assertEq(const String& expected, const String& actual) {
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), actual.c_str());
}

const int64_t EDGE_NOW = 1790330652;  // 2026-09-25T12:04:12+02:00 (servedAt of the edge fixture)

}  // namespace

void setUp() {
    setenv("TZ", timeutil::TZ_EUROPE_BERLIN, 1);
    tzset();
}
void tearDown() {}

// --- time ---------------------------------------------------------------------

void test_parse_iso8601() {
    TEST_ASSERT_EQUAL_INT64(1790594520, timeutil::parseIso8601("2026-09-28T13:22:00+02:00"));
    TEST_ASSERT_EQUAL_INT64(1790594520, timeutil::parseIso8601("2026-09-28T11:22:00Z"));
    TEST_ASSERT_EQUAL_INT64(1790594520, timeutil::parseIso8601("2026-09-28T13:22:00.541808+02:00"));
    TEST_ASSERT_EQUAL_INT64(1768460400, timeutil::parseIso8601("2026-01-15T08:00:00+01:00"));
    TEST_ASSERT_EQUAL_INT64(0, timeutil::parseIso8601(""));
    TEST_ASSERT_EQUAL_INT64(0, timeutil::parseIso8601(nullptr));
    TEST_ASSERT_EQUAL_INT64(0, timeutil::parseIso8601("2026-09-28T13:22:00"));  // no offset
    TEST_ASSERT_EQUAL_INT64(0, timeutil::parseIso8601("2026-13-28T13:22:00Z"));
    TEST_ASSERT_EQUAL_INT64(0, timeutil::parseIso8601("garbage"));
}

void test_minutes_until_rounds_down() {
    TEST_ASSERT_EQUAL_INT(0, timeutil::minutesUntil(1000, 1000));
    TEST_ASSERT_EQUAL_INT(0, timeutil::minutesUntil(1059, 1000));
    TEST_ASSERT_EQUAL_INT(1, timeutil::minutesUntil(1060, 1000));
    TEST_ASSERT_EQUAL_INT(1, timeutil::minutesUntil(1119, 1000));
    TEST_ASSERT_EQUAL_INT(-1, timeutil::minutesUntil(970, 1000));
}

void test_clock_uses_berlin_time_with_dst() {
    assertEq("13:22", timeutil::formatClock(1790594520));                          // summer, CEST
    assertEq("08:00", timeutil::formatClock(1768460400));                          // winter, CET
    // 2026-10-25: 03:00 CEST -> 02:00 CET (01:00 UTC)
    assertEq("02:59", timeutil::formatClock(timeutil::parseIso8601("2026-10-25T00:59:00Z")));
    assertEq("02:00", timeutil::formatClock(timeutil::parseIso8601("2026-10-25T01:00:00Z")));
    // 2026-03-29: 02:00 CET -> 03:00 CEST
    assertEq("01:59", timeutil::formatClock(timeutil::parseIso8601("2026-03-29T00:59:00Z")));
    assertEq("03:00", timeutil::formatClock(timeutil::parseIso8601("2026-03-29T01:00:00Z")));
}

// --- text ---------------------------------------------------------------------

void test_utf8_helpers() {
    TEST_ASSERT_EQUAL_INT(20, text::utf8Length("Ihringshäuser Straße"));
    assertEq("Ihringshäuser St.", text::utf8Truncate("Ihringshäuser Straße", 17));
    assertEq("Kassel.", text::utf8Truncate("Kassel Königsplatz", 8));  // no "Kassel ."
    assertEq("Kö", text::utf8Truncate("Königsplatz", 2, ""));
    assertEq("Kurz", text::utf8Truncate("Kurz", 10));
    assertEq("Straße    ", text::fitColumn("Straße", 10));
    assertEq("  ß", text::fitColumn("ß", 3, true));
    assertEq("Ihringshaeuser Strasse - Oe", text::toAscii("Ihringshäuser Straße · Ö"));
    assertEq("k%C3%B6nigsplatz%20a%26b", text::urlEncode("königsplatz a&b"));
    assertEq("a b", text::sanitize("a\nb"));
}

// --- parser ---------------------------------------------------------------------

void test_parse_real_departures_fixture() {
    const StationBoard b = parseFixture("departures_koenigsplatz_20.json");
    assertEq("2200002", b.stop.id);
    assertEq("Kassel Königsplatz", b.stop.name);
    TEST_ASSERT_FALSE(b.stale);
    TEST_ASSERT_NOT_EQUAL(0, b.servedAt);
    TEST_ASSERT_EQUAL_UINT(20, b.departures.size());

    const Departure& d0 = b.departures[0];
    assertEq("3", d0.line);
    assertEq("Ihringshäuser Straße", d0.destination);
    assertEq("A", d0.platform);
    TEST_ASSERT_TRUE(d0.realtime);
    TEST_ASSERT_EQUAL_INT64(timeutil::parseIso8601("2026-09-28T13:24:00+02:00"), d0.effectiveTime());

    for (size_t i = 1; i < b.departures.size(); ++i) {
        TEST_ASSERT_TRUE(b.departures[i - 1].effectiveTime() <= b.departures[i].effectiveTime());
    }
}

void test_parse_edge_cases() {
    const StationBoard b = parseFixture("departures_edge_cases.json");
    TEST_ASSERT_TRUE(b.stale);
    TEST_ASSERT_EQUAL_INT64(0, b.sourceUpdatedAt);           // null
    TEST_ASSERT_EQUAL_UINT(5, b.departures.size());           // "Broken time" dropped
    // Sorted by estimated-else-scheduled time.
    assertEq("8", b.departures[0].line);
    assertEq("10", b.departures[1].line);
    assertEq("RT1", b.departures[2].line);
    assertEq("3", b.departures[3].line);
    assertEq("4", b.departures[4].line);

    TEST_ASSERT_TRUE(b.departures[2].cancelled);
    assertEq("Fahrt fällt aus", b.departures[2].firstMessage);
    assertEq("", b.departures[2].platform);                  // no platform at all
    assertEq("C", b.departures[3].platform);                 // estimated platform wins
    TEST_ASSERT_EQUAL_INT64(0, b.departures[4].estimated);   // null estimate
    TEST_ASSERT_TRUE(b.departures[1].partiallyCancelled);
}

void test_parse_real_stops_fixture() {
    const std::string json = fixture("stops_koenigsplatz.json");
    std::vector<Stop> stops;
    TEST_ASSERT_TRUE(parser::parseStops(json.c_str(), json.size(), stops).ok());
    TEST_ASSERT_TRUE(stops.size() >= 2);
    assertEq("2200002", stops[0].id);
    assertEq("Kassel Königsplatz", stops[0].name);
}

void test_parse_errors() {
    StationBoard b;
    std::vector<Stop> stops;
    TEST_ASSERT_EQUAL(ApiError::Parse, parser::parseDepartures("{", 1, b).error);
    TEST_ASSERT_EQUAL(ApiError::Parse, parser::parseDepartures("[]", 2, b).error);
    TEST_ASSERT_EQUAL(ApiError::Parse, parser::parseStops("{}", 2, stops).error);

    const std::string err = fixture("error_unauthorized.json");
    assertEq("unauthorized", parser::parseErrorCode(err.c_str(), err.size()));
    TEST_ASSERT_EQUAL(ApiError::Unauthorized, parser::errorFromHttpStatus(401));
    TEST_ASSERT_EQUAL(ApiError::NotFound, parser::errorFromHttpStatus(404));
    TEST_ASSERT_EQUAL(ApiError::Server, parser::errorFromHttpStatus(503));
    TEST_ASSERT_EQUAL(ApiError::Network, parser::errorFromHttpStatus(-1));
}

// --- display model -----------------------------------------------------------------

void test_model_from_edge_cases() {
    const StationBoard b = parseFixture("departures_edge_cases.json");
    const DisplayModel m = buildDisplayModel(b, EDGE_NOW, BoardStatus::Stale);

    // "8" left 102 s ago -> gone. "10" left 12 s ago -> still "now".
    const int expectedRows = DEPARTURE_COUNT < 4 ? DEPARTURE_COUNT : 4;
    TEST_ASSERT_EQUAL_INT(expectedRows, m.rowCount);
    assertEq("10", m.rows[0].line);
    assertEq(formatMinutes(0), m.rows[0].time);
    assertEq("RT1", m.rows[1].line);
    assertEq("AUSFALL", m.rows[1].time);
    TEST_ASSERT_TRUE(m.rows[1].cancelled);
    assertEq("3", m.rows[2].line);
    assertEq(formatMinutes(3), m.rows[2].time);  // 12:07:30 - 12:04:12 = 3 min 18 s
    assertEq("VERALTET · Stand 12:04", m.statusText);
    assertEq(SHOW_CLOCK ? "12:04" : "", m.clock);
}

void test_model_respects_departure_count() {
    const StationBoard b = parseFixture("departures_koenigsplatz_20.json");
    const DisplayModel m = buildDisplayModel(b, b.servedAt, BoardStatus::Live);
    TEST_ASSERT_EQUAL_INT(DEPARTURE_COUNT, m.rowCount);
    assertEq("", m.statusText);  // no status bar when live
    const auto lines = board_text::departures(m, false);
    for (const String& line : lines) TEST_ASSERT_TRUE(line.indexOf("LIVE") < 0);
    TEST_ASSERT_EQUAL_INT(3 + DEPARTURE_COUNT + 1, static_cast<int>(lines.size()));
    TEST_ASSERT_EQUAL(SHOW_PLATFORM, m.showPlatform);
}

void test_model_counts_down_locally() {
    const StationBoard b = parseFixture("departures_edge_cases.json");
    // Four minutes later: 10 and RT1 are gone, "3" leaves now, "4" in 1 min.
    const DisplayModel m = buildDisplayModel(b, EDGE_NOW + 4 * 60, BoardStatus::Live);
    TEST_ASSERT_EQUAL_INT(2, m.rowCount);
    assertEq("3", m.rows[0].line);
    assertEq(formatMinutes(0), m.rows[0].time);
    assertEq("4", m.rows[1].line);
    assertEq(formatMinutes(1), m.rows[1].time);
}

void test_model_without_clock_shows_departure_time() {
    const StationBoard b = parseFixture("departures_edge_cases.json");
    const DisplayModel m = buildDisplayModel(b, 0, BoardStatus::Live);
    assertEq("", m.clock);
    assertEq("12:02", m.rows[0].time);  // nothing dropped without a clock
}

// --- Serial board ----------------------------------------------------------------

void test_board_lines_have_fixed_width() {
    const StationBoard b = parseFixture("departures_edge_cases.json");
    const DisplayModel m = buildDisplayModel(b, EDGE_NOW, BoardStatus::Stale);
    const auto lines = board_text::departures(m, /*ansi=*/false);
    for (const String& line : lines) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(board_text::WIDTH, text::utf8Length(line), line.c_str());
    }
    // Long destination is cut, cancelled row still ends with AUSFALL.
    bool found = false;
    for (const String& line : lines) {
        if (line.startsWith("RT1")) {
            found = true;
            TEST_ASSERT_TRUE(line.endsWith("AUSFALL"));
            TEST_ASSERT_TRUE(line.indexOf("Hofgeismar-Hümme") >= 0);
        }
    }
    TEST_ASSERT_TRUE(found);
}

void test_board_without_departures() {
    StationBoard b;
    b.stop.name = "Kassel Königsplatz";
    const DisplayModel m = buildDisplayModel(b, EDGE_NOW, BoardStatus::Live);
    const auto lines = board_text::departures(m, false);
    bool found = false;
    for (const String& line : lines) found |= line.indexOf("Keine Abfahrten") >= 0;
    TEST_ASSERT_TRUE(found);
}

// --- config ---------------------------------------------------------------------

void test_device_config_rules() {
    DeviceConfig c;
    TEST_ASSERT_FALSE(c.isComplete());
    c.wifiSsid = "Schule";
    c.wifiPassword = "geheim123";
    c.stopId = "2200002";
    c.stopName = "Kassel Königsplatz";
    TEST_ASSERT_FALSE(c.isComplete());  // configured flag missing
    c.configured = true;
    TEST_ASSERT_TRUE(c.isComplete());

    TEST_ASSERT_TRUE(config_rules::isValidWifiPassword(""));  // open network
    TEST_ASSERT_FALSE(config_rules::isValidWifiPassword("kurz"));
    TEST_ASSERT_FALSE(config_rules::isValidStopId("22a"));
    TEST_ASSERT_FALSE(config_rules::isValidStopId(""));
    TEST_ASSERT_FALSE(config_rules::isValidSsid("123456789012345678901234567890123"));
}

void test_student_settings_are_sane() {
    TEST_ASSERT_TRUE(DEPARTURE_COUNT >= 3 && DEPARTURE_COUNT <= MAX_DEPARTURE_ROWS);
    TEST_ASSERT_EQUAL_INT(4, config_checks::utf8Chars("grün"));
    TEST_ASSERT_TRUE(formatMinutes(0).length() > 0);
    TEST_ASSERT_TRUE(formatMinutes(15).length() > 0);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_iso8601);
    RUN_TEST(test_minutes_until_rounds_down);
    RUN_TEST(test_clock_uses_berlin_time_with_dst);
    RUN_TEST(test_utf8_helpers);
    RUN_TEST(test_parse_real_departures_fixture);
    RUN_TEST(test_parse_edge_cases);
    RUN_TEST(test_parse_real_stops_fixture);
    RUN_TEST(test_parse_errors);
    RUN_TEST(test_model_from_edge_cases);
    RUN_TEST(test_model_respects_departure_count);
    RUN_TEST(test_model_counts_down_locally);
    RUN_TEST(test_model_without_clock_shows_departure_time);
    RUN_TEST(test_board_lines_have_fixed_width);
    RUN_TEST(test_board_without_departures);
    RUN_TEST(test_device_config_rules);
    RUN_TEST(test_student_settings_are_sane);
    return UNITY_END();
}
