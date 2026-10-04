#include "SprintData.h"

static bool isLeap(unsigned y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static unsigned daysInMonth(unsigned y, unsigned m) {
    static const uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeap(y)) ? 29 : kDays[m - 1];
}

bool isValidDate(const Date& d) {
    if (d.y < 2000 || d.y > 2099) return false;
    if (d.m < 1 || d.m > 12) return false;
    return d.d >= 1 && d.d <= daysInMonth(d.y, d.m);
}

bool parseDate(const char* s, Date& out) {
    if (!s || strlen(s) != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) continue;
        if (s[i] < '0' || s[i] > '9') return false;
    }
    Date t;
    t.y = static_cast<uint16_t>((s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 +
                                (s[3] - '0'));
    t.m = static_cast<uint8_t>((s[5] - '0') * 10 + (s[6] - '0'));
    t.d = static_cast<uint8_t>((s[8] - '0') * 10 + (s[9] - '0'));
    if (!isValidDate(t)) return false;
    out = t;
    return true;
}

// Days since 1970-01-01 (proleptic Gregorian). Howard Hinnant's algorithm.
static int32_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= (m <= 2) ? 1 : 0;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned mp = (m > 2) ? m - 3 : m + 9;
    const unsigned doy = (153 * mp + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

int32_t daysSinceEpoch(const Date& d) { return daysFromCivil(d.y, d.m, d.d); }

int32_t daysBetween(const Date& a, const Date& b) { return daysSinceEpoch(b) - daysSinceEpoch(a); }

Date civilFromDays(int32_t z) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = static_cast<int>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;
    return Date{static_cast<uint16_t>(y + (m <= 2 ? 1 : 0)), static_cast<uint8_t>(m),
                static_cast<uint8_t>(d)};
}

uint8_t weekdayOf(const Date& d) {
    // 1970-01-01 was a Thursday (4). 0 = Sunday .. 6 = Saturday.
    const int32_t days = daysSinceEpoch(d);
    return static_cast<uint8_t>(((days % 7) + 7 + 4) % 7);
}

uint8_t progressPct(const SprintData& s) {
    if (s.total == 0) return 0;
    uint32_t pct = (static_cast<uint32_t>(s.done) * 100u + s.total / 2u) / s.total;
    return pct > 100 ? 100 : static_cast<uint8_t>(pct);
}

int32_t daysLeft(const SprintData& s, const Date& today) { return daysBetween(today, s.end); }

bool isOpenTask(const Task& t) { return !(t.status[0] == 'D' && t.status[1] == 'N'); }

uint8_t openTaskCount(const SprintData& s) {
    uint8_t n = 0;
    const uint8_t limit = s.taskCount > kMaxTasks ? kMaxTasks : s.taskCount;
    for (uint8_t i = 0; i < limit; ++i)
        if (isOpenTask(s.tasks[i])) ++n;
    return n;
}
