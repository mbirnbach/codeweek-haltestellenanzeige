#include "app/DisplayModelBuilder.h"

#include "student/config_checks.h"
#include "student/student_code.h"
#include "util/TextUtil.h"
#include "util/TimeUtil.h"

namespace {

constexpr const char* CANCELLED_TEXT = "AUSFALL";
constexpr int64_t DEPARTED_GRACE_SECONDS = 60;

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
    if (dep.cancelled) return CANCELLED_TEXT;

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

    for (const Departure& dep : board.departures) {
        if (m.rowCount >= DEPARTURE_COUNT) break;

        // Already gone since the last fetch? Times are minute-precise, so a
        // vehicle is shown as "now" until a minute after its departure time
        // (same rule as the backend). Cancelled ones: by schedule.
        const int64_t t = dep.cancelled ? dep.scheduled : dep.effectiveTime();
        if (clockValid(now) && t + DEPARTED_GRACE_SECONDS < now) continue;

        DepartureRow& row = m.rows[m.rowCount++];
        row.line = dep.line;
        row.destination = dep.destination;
        row.platform = dep.platform;
        row.cancelled = dep.cancelled;
        row.time = departureTimeText(dep, now);
    }
    return m;
}
