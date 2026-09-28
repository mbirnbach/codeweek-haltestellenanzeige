#include "util/TimeUtil.h"

#include <stdio.h>

namespace timeutil {

namespace {

bool readDigits(const char*& p, int count, int& out) {
    out = 0;
    for (int i = 0; i < count; ++i) {
        if (*p < '0' || *p > '9') return false;
        out = out * 10 + (*p - '0');
        ++p;
    }
    return true;
}

bool expect(const char*& p, char c) {
    if (*p != c) return false;
    ++p;
    return true;
}

// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's
// days_from_civil). Independent of the TZ setting and of timegm(), which
// the ESP32 newlib doesn't provide.
int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

}  // namespace

int64_t parseIso8601(const char* text) {
    if (text == nullptr) return 0;
    const char* p = text;
    int year, month, day, hour, minute, second;
    if (!readDigits(p, 4, year) || !expect(p, '-') || !readDigits(p, 2, month) || !expect(p, '-') ||
        !readDigits(p, 2, day) || !(expect(p, 'T') || expect(p, ' ')) || !readDigits(p, 2, hour) ||
        !expect(p, ':') || !readDigits(p, 2, minute) || !expect(p, ':') || !readDigits(p, 2, second)) {
        return 0;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) {
        return 0;
    }

    // Optional fractional seconds are ignored.
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') ++p;
    }

    int offsetSeconds = 0;
    if (*p == 'Z' || *p == 'z') {
        ++p;
    } else if (*p == '+' || *p == '-') {
        const int sign = (*p == '-') ? -1 : 1;
        ++p;
        int oh, om;
        if (!readDigits(p, 2, oh) || !expect(p, ':') || !readDigits(p, 2, om)) return 0;
        offsetSeconds = sign * (oh * 3600 + om * 60);
    } else {
        return 0;  // no offset: ambiguous, refuse
    }
    if (*p != '\0') return 0;

    const int64_t local = daysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
    return local - offsetSeconds;
}

int minutesUntil(int64_t departure, int64_t now) {
    const int64_t diff = departure - now;
    // Floor division, so -30 s is -1 minute (already left), not 0.
    int64_t minutes = diff / 60;
    if (diff % 60 != 0 && diff < 0) --minutes;
    return static_cast<int>(minutes);
}

String formatClock(int64_t epoch) {
    time_t t = static_cast<time_t>(epoch);
    struct tm local;
    localtime_r(&t, &local);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", local.tm_hour, local.tm_min);
    return String(buf);
}

}  // namespace timeutil
