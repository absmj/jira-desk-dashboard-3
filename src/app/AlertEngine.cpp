#include "AlertEngine.h"

#include <string.h>

namespace {

bool inRange(const Range& r, int32_t v) { return !r.used || (v >= r.min && v <= r.max); }

bool inQuiet(const AlertConfig& cfg, uint16_t minOfDay) {
    if (!cfg.hasQuiet) return false;
    if (cfg.quietFrom < cfg.quietTo) return minOfDay >= cfg.quietFrom && minOfDay < cfg.quietTo;
    return minOfDay >= cfg.quietFrom || minOfDay < cfg.quietTo;  // crosses midnight
}

}  // namespace

void AlertEngine::reset() { memset(st_, 0, sizeof(st_)); }

const AlertRule* AlertEngine::poll(const AlertConfig& cfg, const SprintData& sprint,
                                   const DateTime& now) {
    const uint16_t mod = minuteOfDay(now);
    if (inQuiet(cfg, mod)) return nullptr;

    const int32_t nowAbs = absMinute(now);
    const uint8_t wd = weekdayOf(now.date);

    for (uint8_t i = 0; i < cfg.count; ++i) {
        const AlertRule& r = cfg.rules[i];
        if (!r.enabled) continue;
        if (r.daysMask != 0 && !(r.daysMask & (1u << wd))) continue;
        if (r.hasTime && r.timeMin != mod) continue;

        if (r.needsSprint) {
            if (!sprint.valid) continue;
            if (!inRange(r.daysLeft, daysLeft(sprint, now.date))) continue;
            if (!inRange(r.progress, progressPct(sprint))) continue;
            if (!inRange(r.overdue, sprint.stats.overdue)) continue;
            if (!inRange(r.blocked, sprint.stats.blocked)) continue;
        }

        State& s = st_[i];
        if (s.fired) {
            switch (r.repeat) {
                case Repeat::Once:
                    continue;
                case Repeat::Daily:
                    if (s.lastAbsMin / 1440 == nowAbs / 1440) continue;  // already today
                    break;
                case Repeat::Cooldown:
                    if (nowAbs - s.lastAbsMin < r.cooldownMin) continue;
                    break;
            }
        }
        s.fired = true;
        s.lastAbsMin = nowAbs;
        return &r;
    }
    return nullptr;
}
