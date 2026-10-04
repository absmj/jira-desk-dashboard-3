#pragma once
// Sprint data model. Pure C++ (no Arduino dependency) so it can be unit-tested
// on a PC. All strings are fixed-size UTF-8 buffers: no heap allocation.
//
// The web app computes all statistics (top assignee, status counts, pace...)
// and sends them ready-made; the device only stores and displays them.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct Date {
    uint16_t y;
    uint8_t m;
    uint8_t d;
};

enum class Pace : uint8_t { OnTrack = 0, Ahead = 1, Behind = 2 };

constexpr uint8_t kMaxTasks = 24;

struct Task {
    char key[12];       // "PRJ-101"
    char title[40];     // UTF-8, ~20 Azerbaijani chars worst case
    char status[3];     // "TD" to do, "IP" in progress, "BL" blocked, "DN" done
    char assignee[24];  // optional
};

struct Stats {
    char topName[24];      // assignee with the most open tasks
    uint16_t topCount;     // that assignee's open task count
    uint16_t todo;
    uint16_t inProgress;
    uint16_t done;
    uint16_t overdue;
    uint16_t blocked;
    Pace pace;
};

struct SprintData {
    bool valid;            // false until a dataset has been received/loaded
    char name[24];
    Date start;
    Date end;
    uint16_t done;         // completed tasks
    uint16_t total;        // all tasks in the sprint
    Stats stats;
    Task tasks[kMaxTasks]; // only the tasks worth listing; web app trims the rest
    uint8_t taskCount;
};

// Safe bounded copy, always NUL-terminated.
inline void copyStr(char* dst, size_t n, const char* src) {
    if (n == 0) return;
    strncpy(dst, src ? src : "", n - 1);
    dst[n - 1] = '\0';
}

// ---- dates ------------------------------------------------------------------
bool isValidDate(const Date& d);
// Parses "YYYY-MM-DD". Returns false (and leaves out untouched) if malformed.
bool parseDate(const char* s, Date& out);
// Whole days from a to b (negative if b is before a).
int32_t daysBetween(const Date& a, const Date& b);
// Days since 1970-01-01, and the inverse.
int32_t daysSinceEpoch(const Date& d);
Date civilFromDays(int32_t days);
// 0 = Sunday .. 6 = Saturday.
uint8_t weekdayOf(const Date& d);

// ---- derived values -----------------------------------------------------------
uint8_t progressPct(const SprintData& s);                  // 0..100, rounded
int32_t daysLeft(const SprintData& s, const Date& today);  // may be negative
bool isOpenTask(const Task& t);                            // status != "DN"
uint8_t openTaskCount(const SprintData& s);
