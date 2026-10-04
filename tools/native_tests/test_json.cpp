// Host-side tests that need ArduinoJson: sprint loader, alert parser, engine, clock.
#include <stdio.h>
#include <string.h>

#include "../../src/app/AlertEngine.h"
#include "../../src/app/AlertLoader.h"
#include "../../src/app/SampleData.h"
#include "../../src/app/SimClock.h"
#include "../../src/app/SprintLoader.h"

static int g_failed = 0, g_checks = 0;
#define CHECK(c)                                                     \
    do {                                                             \
        ++g_checks;                                                  \
        if (!(c)) {                                                  \
            ++g_failed;                                              \
            printf("  FAIL line %d: %s\n", __LINE__, #c);            \
        }                                                            \
    } while (0)

static DateTime at(int y, int mo, int d, int h, int mi, int s = 0) {
    return DateTime{Date{(uint16_t)y, (uint8_t)mo, (uint8_t)d}, (uint8_t)h, (uint8_t)mi, (uint8_t)s};
}

static const char* kSprintJson = R"({
  "v": 1,
  "sprint": {"name":"SPRINT 24","start":"2026-10-01","end":"2026-10-18","done":26,"total":40},
  "stats": {"topAssignee":{"name":"Əli Məmmədov","count":5},
            "byStatus":{"todo":6,"inProgress":8,"done":26},
            "overdue":2,"blocked":1,"pace":"behind"},
  "tasks": [
    {"k":"PRJ-101","t":"Giriş ekranı","s":"IP","a":"Əli Məmmədov"},
    {"k":"PRJ-102","t":"API testləri","s":"TD"},
    {"k":"PRJ-103","t":"","s":"TD"},
    {"k":"PRJ-104","t":"Bad status","s":"XX"}
  ]})";

static void testSprintLoader() {
    printf("sprint.json loader\n");
    SprintData s;
    memset(&s, 0, sizeof s);
    const char* err = nullptr;
    CHECK(parseSprintJson(kSprintJson, s, &err));
    CHECK(s.valid && strcmp(s.name, "SPRINT 24") == 0);
    CHECK(s.end.m == 10 && s.end.d == 18 && s.done == 26 && s.total == 40);
    CHECK(strcmp(s.stats.topName, "Əli Məmmədov") == 0 && s.stats.pace == Pace::Behind);
    CHECK(s.taskCount == 2);  // empty title and bad status skipped

    char bom[1024];
    snprintf(bom, sizeof bom, "\xEF\xBB\xBF%s", kSprintJson);
    SprintData b;
    memset(&b, 0, sizeof b);
    CHECK(parseSprintJson(bom, b, &err));

    SprintData before = s;
    const char* bad[] = {
        "{",
        "{\"v\":2,\"sprint\":{}}",
        "{\"v\":1}",
        "{\"v\":1,\"sprint\":{\"name\":\"\",\"start\":\"2026-10-01\",\"end\":\"2026-10-18\",\"done\":1,\"total\":2}}",
        "{\"v\":1,\"sprint\":{\"name\":\"S\",\"start\":\"2026-13-01\",\"end\":\"2026-10-18\",\"done\":1,\"total\":2}}",
        "{\"v\":1,\"sprint\":{\"name\":\"S\",\"start\":\"2026-10-20\",\"end\":\"2026-10-18\",\"done\":1,\"total\":2}}",
        "{\"v\":1,\"sprint\":{\"name\":\"S\",\"start\":\"2026-10-01\",\"end\":\"2026-10-18\",\"done\":5,\"total\":2}}",
    };
    for (const char* j : bad) {
        err = nullptr;
        CHECK(!parseSprintJson(j, s, &err));
        CHECK(err != nullptr);
    }
    CHECK(memcmp(&before, &s, sizeof s) == 0);  // failed parses never touch live data
}

static const char* kAlertsJson = R"({
  "v": 1,
  "quietHours": {"from":"19:00","to":"08:30"},
  "rules": [
    {"id":"standup","when":{"time":"09:55","days":["mon","tue","wed","thu","fri","sun"]},
     "audio":"0005.mp3","repeat":"daily","text":"Standup"},
    {"id":"half_done","when":{"progress":{"gte":50}},"audio":"0003.mp3","repeat":"once"},
    {"id":"has_overdue","when":{"overdue":{"gte":1}},"audio":"0004.mp3","repeat":"cooldown","cooldownMin":30},
    {"id":"last_day","when":{"daysLeft":{"lte":1}},"audio":"0002.mp3","repeat":"daily"},
    {"id":"off","enabled":false,"when":{"time":"10:00"},"audio":"0001.mp3"},
    {"id":"","when":{"time":"10:00"}},
    {"id":"no_cond","when":{}},
    {"id":"bad_time","when":{"time":"25:99"}},
    {"id":"bad_audio","when":{"time":"10:00"},"audio":"song.wav"},
    {"id":"bad_cooldown","when":{"time":"10:00"},"repeat":"cooldown"}
  ]})";

static void testAlertParser() {
    printf("alerts.json parser\n");
    AlertConfig c;
    memset(&c, 0, sizeof c);
    const char* err = nullptr;
    CHECK(parseAlertsJson(kAlertsJson, c, &err));
    CHECK(c.count == 5);    // 4 active + 1 disabled (kept, enabled=false)
    CHECK(c.skipped == 5);  // empty id, no condition, bad time, bad audio, bad cooldown
    CHECK(c.hasQuiet && c.quietFrom == 19 * 60 && c.quietTo == 8 * 60 + 30);

    const AlertRule& st = c.rules[0];
    CHECK(strcmp(st.id, "standup") == 0 && st.hasTime && st.timeMin == 9 * 60 + 55);
    CHECK(st.track == 5 && st.repeat == Repeat::Daily && !st.needsSprint);
    CHECK((st.daysMask & (1u << 0)) && (st.daysMask & (1u << 1)) && !(st.daysMask & (1u << 6)));
    CHECK(strcmp(st.text, "Standup") == 0);

    CHECK(c.rules[1].progress.used && c.rules[1].progress.min == 50 && c.rules[1].repeat == Repeat::Once);
    CHECK(c.rules[2].repeat == Repeat::Cooldown && c.rules[2].cooldownMin == 30);
    CHECK(c.rules[3].daysLeft.used && c.rules[3].daysLeft.max == 1);
    CHECK(!c.rules[4].enabled);

    AlertConfig keep = c;
    CHECK(!parseAlertsJson("{\"v\":1}", c, &err));
    CHECK(!parseAlertsJson("{\"v\":3,\"rules\":[]}", c, &err));
    CHECK(!parseAlertsJson("not json", c, &err));
    CHECK(memcmp(&keep, &c, sizeof c) == 0);
}

static void testClock() {
    printf("SimClock / weekday\n");
    CHECK(weekdayOf(Date{2026, 10, 4}) == 0);   // Sunday
    CHECK(weekdayOf(Date{2026, 10, 5}) == 1);   // Monday
    CHECK(weekdayOf(Date{1970, 1, 1}) == 4);    // Thursday
    CHECK(weekdayOf(Date{2028, 2, 29}) == 2);   // Tuesday

    for (int32_t d : {-5, 0, 1, 365, 20000, 20365, 25000}) {
        CHECK(daysSinceEpoch(civilFromDays(d)) == d);
    }
    Date x = civilFromDays(daysSinceEpoch(Date{2028, 2, 29}));
    CHECK(x.y == 2028 && x.m == 2 && x.d == 29);

    SimClock clk;
    clk.begin(at(2026, 10, 4, 9, 50), 1000, 1);
    DateTime t = clk.now(1000 + 65 * 1000);
    CHECK(t.hour == 9 && t.minute == 51 && t.second == 5);

    clk.begin(at(2026, 10, 4, 9, 50), 0, 60);  // 1 s real = 1 min simulated
    t = clk.now(5000);
    CHECK(t.hour == 9 && t.minute == 55 && t.second == 0);
    t = clk.now(15 * 60 * 1000);               // 15 real minutes = 15 simulated hours
    CHECK(t.date.d == 5 && t.hour == 0 && t.minute == 50);  // crossed midnight

    clk.begin(at(2026, 12, 31, 23, 59, 30), 0xFFFFFF00u, 1);  // millis() wrap + year change
    t = clk.now(0xFFFFFF00u + 60000);
    CHECK(t.date.y == 2027 && t.date.m == 1 && t.date.d == 1 && t.minute == 0);
}

static AlertConfig loadAlerts() {
    AlertConfig c;
    memset(&c, 0, sizeof c);
    const char* err = nullptr;
    parseAlertsJson(kAlertsJson, c, &err);
    return c;
}

static bool fires(AlertEngine& e, const AlertConfig& c, const SprintData& s, const DateTime& t,
                  const char* id) {
    const AlertRule* r = e.poll(c, s, t);
    return r && strcmp(r->id, id) == 0;
}

static void testEngine() {
    printf("alert engine\n");
    SprintData s;
    makeSampleData(s);  // progress 65 %, 2 overdue, ends 2026-10-18
    SprintData noData;
    memset(&noData, 0, sizeof noData);
    AlertConfig c = loadAlerts();

    // Time rule: only inside its minute, once per day (daily), weekday mask respected.
    {
        AlertEngine e;
        c.rules[1].enabled = c.rules[2].enabled = c.rules[3].enabled = false;  // isolate "standup"
        CHECK(e.poll(c, s, at(2026, 10, 4, 9, 54, 59)) == nullptr);
        CHECK(fires(e, c, s, at(2026, 10, 4, 9, 55, 0), "standup"));       // Sunday: in mask
        CHECK(e.poll(c, s, at(2026, 10, 4, 9, 55, 30)) == nullptr);        // same minute: no repeat
        CHECK(e.poll(c, s, at(2026, 10, 4, 9, 56, 0)) == nullptr);
        CHECK(fires(e, c, s, at(2026, 10, 5, 9, 55, 0), "standup"));       // next day fires again
        CHECK(e.poll(c, s, at(2026, 10, 10, 9, 55, 0)) == nullptr);        // Saturday: not in mask
    }

    // Data rules: once / cooldown / daily, with the exact policy documented in AlertEngine.h.
    {
        AlertEngine e;
        c = loadAlerts();
        c.rules[0].enabled = false;
        DateTime t = at(2026, 10, 4, 12, 0);
        CHECK(fires(e, c, s, t, "half_done"));      // progress 65 >= 50, once
        CHECK(fires(e, c, s, t, "has_overdue"));    // overdue 2 >= 1
        CHECK(e.poll(c, s, t) == nullptr);          // last_day not met (14 days left); others done
        CHECK(e.poll(c, s, at(2026, 10, 4, 12, 29)) == nullptr);              // cooldown 30 min
        CHECK(fires(e, c, s, at(2026, 10, 4, 12, 30), "has_overdue"));        // cooldown elapsed
        CHECK(e.poll(c, s, at(2026, 10, 5, 12, 0)) != nullptr);               // overdue again (>30 min)
        // "once" never repeats, even days later
        bool halfAgain = false;
        for (int day = 5; day < 12; ++day) {
            const AlertRule* r = e.poll(c, s, at(2026, 10, day, 12, 0));
            if (r && strcmp(r->id, "half_done") == 0) halfAgain = true;
        }
        CHECK(!halfAgain);
        // last_day: becomes true on the 17th/18th, then once per day
        // On the 17th (1 day left) both has_overdue (cooldown elapsed) and last_day are due;
        // they come out one per poll, in rule order.
        const AlertRule* r1 = e.poll(c, s, at(2026, 10, 17, 12, 0));
        const AlertRule* r2 = e.poll(c, s, at(2026, 10, 17, 12, 0));
        CHECK(r1 && r2);
        CHECK((r1 && strcmp(r1->id, "has_overdue") == 0) && (r2 && strcmp(r2->id, "last_day") == 0));
        bool lastDayFired = false, lastDayTwiceSameDay = false;
        AlertEngine e2;
        int count = 0;
        for (int m = 0; m < 30; ++m) {
            const AlertRule* r = e2.poll(c, s, at(2026, 10, 18, 12, m));
            if (r && strcmp(r->id, "last_day") == 0) {
                lastDayFired = true;
                ++count;
            }
        }
        lastDayTwiceSameDay = count > 1;
        CHECK(lastDayFired && !lastDayTwiceSameDay);
    }

    // Quiet hours (cross midnight): nothing fires, and the rule is not consumed.
    {
        AlertEngine e;
        c = loadAlerts();
        c.rules[0].enabled = c.rules[2].enabled = c.rules[3].enabled = false;
        CHECK(e.poll(c, s, at(2026, 10, 4, 20, 0)) == nullptr);   // 20:00 is quiet
        CHECK(e.poll(c, s, at(2026, 10, 5, 3, 0)) == nullptr);    // 03:00 is quiet
        CHECK(e.poll(c, s, at(2026, 10, 5, 8, 29)) == nullptr);   // until 08:30
        CHECK(fires(e, c, s, at(2026, 10, 5, 8, 30), "half_done"));  // fires after quiet ends
    }

    // Rules that depend on sprint data never fire without data; time-only rules still do.
    {
        AlertEngine e;
        c = loadAlerts();
        CHECK(e.poll(c, noData, at(2026, 10, 4, 12, 0)) == nullptr);
        CHECK(fires(e, c, noData, at(2026, 10, 4, 9, 55), "standup"));
    }

    // Only one alert per poll: simultaneous rules come out one at a time, in order.
    {
        AlertEngine e;
        c = loadAlerts();
        c.rules[0].enabled = false;
        DateTime t = at(2026, 10, 18, 12, 0);  // half_done, has_overdue and last_day all true
        const AlertRule* a = e.poll(c, s, t);
        const AlertRule* b = e.poll(c, s, t);
        const AlertRule* d = e.poll(c, s, t);
        CHECK(a && b && d);
        CHECK(strcmp(a->id, "half_done") == 0 && strcmp(b->id, "has_overdue") == 0 &&
              strcmp(d->id, "last_day") == 0);
        CHECK(e.poll(c, s, t) == nullptr);
        e.reset();
        CHECK(e.poll(c, s, t) != nullptr);  // reset forgets what fired
    }
}

int main() {
    testSprintLoader();
    testAlertParser();
    testClock();
    testEngine();
    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
