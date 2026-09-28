#pragma once

#include <stdint.h>

// Wall clock: NTP with Europe/Berlin time zone (DST handled by the POSIX
// TZ rule). Until NTP answers, the API's "servedAt" timestamp is used to
// set the clock, so countdowns work even if NTP is blocked (school networks).
class TimeSync {
public:
    void begin();

    // Called after every successful API response.
    void setFromServerIfUnsynced(int64_t serverEpoch);

    // Epoch seconds, or 0 if the clock is still unknown.
    int64_t now() const;
    bool isValid() const { return now() != 0; }
};
