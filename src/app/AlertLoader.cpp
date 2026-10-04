#include "AlertLoader.h"

#include <string.h>

#include "SprintData.h"  // copyStr

namespace {

bool fail(const char** err, const char* msg) {
    if (err) *err = msg;
    return false;
}

// "HH:MM" -> minute of day. Strict: exactly 5 characters.
bool parseHm(const char* s, uint16_t& out) {
    if (!s || strlen(s) != 5 || s[2] != ':') return false;
    for (int i : {0, 1, 3, 4})
        if (s[i] < '0' || s[i] > '9') return false;
    const int h = (s[0] - '0') * 10 + (s[1] - '0');
    const int m = (s[3] - '0') * 10 + (s[4] - '0');
    if (h > 23 || m > 59) return false;
    out = static_cast<uint16_t>(h * 60 + m);
    return true;
}

// "0002.mp3" -> 2. Strict: 4 digits + ".mp3", number 1..9999.
bool parseTrack(const char* s, uint16_t& out) {
    if (!s || strlen(s) != 8 || strcmp(s + 4, ".mp3") != 0) return false;
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        n = n * 10 + (s[i] - '0');
    }
    if (n < 1) return false;
    out = static_cast<uint16_t>(n);
    return true;
}

int dayIndex(const char* s) {
    static const char* const kNames[7] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
    for (int i = 0; i < 7; ++i)
        if (strcmp(s, kNames[i]) == 0) return i;
    return -1;
}

// {"gte": a, "lte": b} -> Range. Returns false if the key is present but not an object
// or has neither bound.
bool readRange(JsonVariantConst v, Range& r) {
    r = Range{false, 0, 0};
    if (v.isNull()) return true;  // condition not present
    if (!v.is<JsonObjectConst>()) return false;
    const bool hasMin = !v["gte"].isNull();
    const bool hasMax = !v["lte"].isNull();
    if (!hasMin && !hasMax) return false;
    r.used = true;
    r.min = hasMin ? v["gte"].as<int>() : INT32_MIN;
    r.max = hasMax ? v["lte"].as<int>() : INT32_MAX;
    return true;
}

bool readRule(JsonObjectConst j, AlertRule& r) {
    memset(&r, 0, sizeof(r));
    r.enabled = j["enabled"] | true;

    copyStr(r.id, sizeof(r.id), j["id"] | "");
    if (r.id[0] == '\0') return false;

    JsonObjectConst w = j["when"];
    if (w.isNull()) return false;

    if (!readRange(w["daysLeft"], r.daysLeft)) return false;
    if (!readRange(w["progress"], r.progress)) return false;
    if (!readRange(w["overdue"], r.overdue)) return false;
    if (!readRange(w["blocked"], r.blocked)) return false;
    r.needsSprint = r.daysLeft.used || r.progress.used || r.overdue.used || r.blocked.used;

    if (!w["time"].isNull()) {
        if (!parseHm(w["time"] | "", r.timeMin)) return false;
        r.hasTime = true;
    }
    if (!w["days"].isNull()) {
        if (!w["days"].is<JsonArrayConst>()) return false;
        for (JsonVariantConst d : w["days"].as<JsonArrayConst>()) {
            const int idx = dayIndex(d | "");
            if (idx < 0) return false;
            r.daysMask |= static_cast<uint8_t>(1u << idx);
        }
    }
    // A rule with no condition at all would fire constantly: reject it.
    if (!r.needsSprint && !r.hasTime && r.daysMask == 0) return false;

    if (!j["audio"].isNull()) {
        if (!parseTrack(j["audio"] | "", r.track)) return false;
    }
    copyStr(r.text, sizeof(r.text), j["text"] | "");

    const char* rep = j["repeat"] | "daily";
    if (strcmp(rep, "once") == 0) {
        r.repeat = Repeat::Once;
    } else if (strcmp(rep, "daily") == 0) {
        r.repeat = Repeat::Daily;
    } else if (strcmp(rep, "cooldown") == 0) {
        r.repeat = Repeat::Cooldown;
        const int c = j["cooldownMin"] | 0;
        if (c < 1 || c > 1440) return false;
        r.cooldownMin = static_cast<uint16_t>(c);
    } else {
        return false;
    }
    return true;
}

}  // namespace

bool fillAlerts(const JsonDocument& doc, AlertConfig& out, const char** err) {
    if ((doc["v"] | 0) != 1) return fail(err, "unsupported version");
    if (!doc["rules"].is<JsonArrayConst>()) return fail(err, "rules missing");

    AlertConfig tmp;
    memset(&tmp, 0, sizeof(tmp));

    JsonObjectConst q = doc["quietHours"];
    if (!q.isNull()) {
        if (!parseHm(q["from"] | "", tmp.quietFrom) || !parseHm(q["to"] | "", tmp.quietTo))
            return fail(err, "quietHours invalid");
        tmp.hasQuiet = tmp.quietFrom != tmp.quietTo;
    }

    for (JsonVariantConst v : doc["rules"].as<JsonArrayConst>()) {
        if (tmp.count >= kMaxRules) break;
        AlertRule r;
        if (v.is<JsonObjectConst>() && readRule(v.as<JsonObjectConst>(), r)) {
            tmp.rules[tmp.count++] = r;
        } else {
            ++tmp.skipped;
        }
    }
    out = tmp;  // commit
    return true;
}

bool parseAlertsJson(const char* json, AlertConfig& out, const char** err) {
    JsonDocument doc;
    const DeserializationError e = deserializeJson(doc, json);
    if (e) return fail(err, e.c_str());
    return fillAlerts(doc, out, err);
}
