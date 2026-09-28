#include "network/TimeSync.h"

#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#include "config/AppSettings.h"
#include "util/TimeUtil.h"

void TimeSync::begin() {
    configTzTime(timeutil::TZ_EUROPE_BERLIN, settings::NTP_SERVER_1, settings::NTP_SERVER_2,
                 settings::NTP_SERVER_3);
}

void TimeSync::setFromServerIfUnsynced(int64_t serverEpoch) {
    if (isValid() || serverEpoch < timeutil::MIN_VALID_EPOCH) return;
    struct timeval tv = {static_cast<time_t>(serverEpoch), 0};
    settimeofday(&tv, nullptr);
    Serial.printf("[time] clock set from API server time: %s (NTP not answered yet)\n",
                  timeutil::formatClock(serverEpoch).c_str());
}

int64_t TimeSync::now() const {
    const int64_t t = static_cast<int64_t>(time(nullptr));
    return t >= timeutil::MIN_VALID_EPOCH ? t : 0;
}
