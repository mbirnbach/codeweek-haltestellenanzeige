#include "app/DisplayModelBuilder.h"

#include "student/config_checks.h"
#include "student/student_code.h"
#include "util/TextUtil.h"
#include "util/TimeUtil.h"

namespace {

constexpr int64_t DEPARTED_GRACE_SECONDS = 60;
constexpr unsigned int MAX_TICKER_LENGTH = 400;
constexpr const char* TICKER_SEPARATOR = "  +++  ";

// "Tram 4: ..." mentions line 4, "14: ..." doesn't.
bool mentionsLine(const String& msg, const String& line) {
    const String tag = line + ":";
    for (int from = 0; from + static_cast<int>(tag.length()) <= static_cast<int>(msg.length()); ++from) {
        if (msg.substring(from, from + tag.length()) == tag && (from == 0 || msg[from - 1] == ' ')) return true;
    }
    return false;
}

bool clockValid(int64_t now) { return now >= timeutil::MIN_VALID_EPOCH; }

String statusText(const StationBoard& board, BoardStatus status) {
    if (status == BoardStatus::Live) return String();  // fresh data needs no label

    const int64_t asOf = board.sourceUpdatedAt != 0 ? board.sourceUpdatedAt : board.servedAt;
    String stand = asOf != 0 ? String(" · Stand ") + timeutil::formatClock(asOf) : String();
    if (status == BoardStatus::Offline) return String("KEIN WLAN") + stand;
    return String("VERALTET") + stand;
}

}  // namespace

String departureTimeText(const Departure& dep, int64_t now) {
    if (dep.cancelled) return text::utf8Truncate(CANCELLED_TEXT, MAX_TIME_CHARS, "");

    // Without a clock we can't count down; show the departure time instead.
    if (!clockValid(now)) return timeutil::formatClock(dep.effectiveTime());

    int minutes = timeutil::minutesUntil(dep.effectiveTime(), now);
    if (minutes < 0) minutes = 0;

    // Student code: whatever it returns, keep the layout intact.
    String t = text::sanitize(formatMinutes(minutes));
    t.trim();
    if (t.length() == 0) t = String(minutes) + " min";
    return text::utf8Truncate(t, MAX_TIME_CHARS, "");
}

DisplayModel buildDisplayModel(const StationBoard& board, int64_t now, BoardStatus status) {
    DisplayModel m;
    m.stopName = board.stop.name;
    m.clock = (SHOW_CLOCK && clockValid(now)) ? timeutil::formatClock(now) : String();
    m.showPlatform = SHOW_PLATFORM;
    m.status = status;
    m.statusText = statusText(board, status);

    // The next DEPARTURE_COUNT departures that haven't left yet.
    const Departure* upcoming[MAX_DEPARTURE_ROWS];
    int upcomingCount = 0;
    for (const Departure& dep : board.departures) {
        if (upcomingCount >= DEPARTURE_COUNT) break;
        // Already gone since the last fetch? Times are minute-precise, so a
        // vehicle is shown as "now" until a minute after its departure time
        // (same rule as the backend). Cancelled ones: by schedule.
        const int64_t t = dep.cancelled ? dep.scheduled : dep.effectiveTime();
        if (clockValid(now) && t + DEPARTED_GRACE_SECONDS < now) continue;
        upcoming[upcomingCount++] = &dep;
    }

    // Messages of these departures, each once, prefixed with the line
    // ("1: Zusatzfahrt") unless the message already names it ("Tram 4: ...").
    for (int i = 0; i < upcomingCount; ++i) {
        const Departure& dep = *upcoming[i];
        if (dep.firstMessage.length() == 0) continue;
        String msg = dep.firstMessage;
        if (!mentionsLine(msg, dep.line)) msg = dep.line + ": " + msg;
        if (m.ticker.indexOf(msg) >= 0) continue;
        if (m.ticker.length() > 0) m.ticker += TICKER_SEPARATOR;
        m.ticker += msg;
    }
    if (m.ticker.length() > MAX_TICKER_LENGTH) m.ticker = text::utf8Truncate(m.ticker, MAX_TICKER_LENGTH, "...");

    // Like on the real signs, the message line replaces the last departure.
    const int rows = m.ticker.length() > 0 ? DEPARTURE_COUNT - 1 : DEPARTURE_COUNT;
    for (int i = 0; i < upcomingCount && m.rowCount < rows; ++i) {
        const Departure& dep = *upcoming[i];
        DepartureRow& row = m.rows[m.rowCount++];
        row.line = dep.line;
        row.destination = dep.destination;
        row.platform = dep.platform;
        row.cancelled = dep.cancelled;
        row.time = departureTimeText(dep, now);
    }
    return m;
}
