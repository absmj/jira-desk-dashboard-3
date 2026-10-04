#pragma once
// Development clock. Replaced by the DS3231 in Phase 3 (same DateTime type).
//
// Starts at a chosen date/time when begin() is called and then follows millis().
// `speed` makes simulated time run faster than real time: with speed = 60, one
// real second is one simulated minute, so a rule set for 09:55 fires 5 s after
// a 09:50 start. Use speed = 1 for real time.
#include <stdint.h>

#include "SprintData.h"

struct DateTime {
    Date date;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

// Minutes since 1970-01-01 00:00 (for "was this already fired this minute/day").
int32_t absMinute(const DateTime& t);
inline uint16_t minuteOfDay(const DateTime& t) { return static_cast<uint16_t>(t.hour * 60 + t.minute); }

class SimClock {
public:
    void begin(const DateTime& start, uint32_t nowMs, uint16_t speed = 1);
    DateTime now(uint32_t nowMs) const;

private:
    int64_t baseSeconds_ = 0;  // simulated seconds since 1970 at begin()
    uint32_t startMs_ = 0;
    uint16_t speed_ = 1;
};
