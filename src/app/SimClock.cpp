#include "SimClock.h"

int32_t absMinute(const DateTime& t) {
    return daysSinceEpoch(t.date) * 1440 + t.hour * 60 + t.minute;
}

void SimClock::begin(const DateTime& start, uint32_t nowMs, uint16_t speed) {
    baseSeconds_ = static_cast<int64_t>(daysSinceEpoch(start.date)) * 86400 + start.hour * 3600 +
                   start.minute * 60 + start.second;
    startMs_ = nowMs;
    speed_ = speed == 0 ? 1 : speed;
}

DateTime SimClock::now(uint32_t nowMs) const {
    // uint32 subtraction stays correct across the millis() wrap
    const uint64_t elapsedMs = static_cast<uint32_t>(nowMs - startMs_);
    const int64_t total = baseSeconds_ + static_cast<int64_t>(elapsedMs * speed_ / 1000);
    const int32_t days = static_cast<int32_t>(total / 86400);
    const int32_t secOfDay = static_cast<int32_t>(total % 86400);

    DateTime t;
    t.date = civilFromDays(days);
    t.hour = static_cast<uint8_t>(secOfDay / 3600);
    t.minute = static_cast<uint8_t>((secOfDay % 3600) / 60);
    t.second = static_cast<uint8_t>(secOfDay % 60);
    return t;
}
