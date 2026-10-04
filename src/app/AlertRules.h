#pragma once
// Data-driven sound/alert rules (from /device/alerts.json). Pure C++.
#include <stdint.h>

constexpr uint8_t kMaxRules = 16;

enum class Repeat : uint8_t { Once, Daily, Cooldown };

// Inclusive numeric range built from {"gte": a, "lte": b}. Unused = no condition.
struct Range {
    bool used;
    int32_t min;
    int32_t max;
};

struct AlertRule {
    char id[20];
    bool enabled;

    // Conditions. All the ones that are present must hold (logical AND).
    Range daysLeft;
    Range progress;
    Range overdue;
    Range blocked;
    bool needsSprint;     // true if any of the four ranges above is used
    bool hasTime;         // "time": "HH:MM" -> matches during that exact minute
    uint16_t timeMin;     // minute of day
    uint8_t daysMask;     // bit0 = Sunday .. bit6 = Saturday; 0 = every day

    // Action
    uint16_t track;       // 0001.mp3 -> 1; 0 = no sound
    char text[32];        // message shown on screen (UTF-8)

    // Repetition
    Repeat repeat;
    uint16_t cooldownMin; // used when repeat == Cooldown
};

struct AlertConfig {
    bool hasQuiet;
    uint16_t quietFrom;   // minute of day, inclusive
    uint16_t quietTo;     // minute of day, exclusive; may be < quietFrom (crosses midnight)
    AlertRule rules[kMaxRules];
    uint8_t count;
    uint8_t skipped;      // rules ignored because they were invalid
};
