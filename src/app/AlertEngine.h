#pragma once
// Decides which alert (if any) fires right now. Pure C++: the caller passes in
// the current date/time and sprint data, so it runs the same with the dev
// SimClock, the DS3231 later, and in host-side tests.
//
// Policies (documented behaviour):
//  * At most ONE rule fires per poll(); call again for the next one.
//  * Nothing fires during quiet hours, and the rule is NOT marked as fired, so a
//    rule whose condition is still true fires once the quiet period ends.
//  * "time" rules match only during that exact minute. If the device is off or in
//    quiet hours at that minute, the event is skipped, not replayed later.
//  * Fired-state lives in RAM only for now: after a reboot, "once" and "daily"
//    rules whose condition is still true can fire again.
#include "AlertRules.h"
#include "SimClock.h"
#include "SprintData.h"

class AlertEngine {
public:
    void reset();  // forget what has fired (call when a new alerts.json is loaded)

    // Returns the rule that fires now, or nullptr.
    const AlertRule* poll(const AlertConfig& cfg, const SprintData& sprint, const DateTime& now);

private:
    struct State {
        bool fired;
        int32_t lastAbsMin;
    };
    State st_[kMaxRules] = {};
};
