#include "SprintLoader.h"

namespace {
bool fail(const char** err, const char* msg) { if (err) *err = msg; return false; }

uint16_t toU16(int v) { return v < 0 ? 0 : (v > 65535 ? 65535 : static_cast<uint16_t>(v)); }

Pace parsePace(const char* s) {
    if (strcmp(s, "ahead") == 0)  return Pace::Ahead;
    if (strcmp(s, "behind") == 0) return Pace::Behind;
    return Pace::OnTrack;
}

bool validStatus(const char* s) {
    return !strcmp(s, "TD") || !strcmp(s, "IP") || !strcmp(s, "BL") || !strcmp(s, "DN");
}
}  // namespace

bool fillSprint(const JsonDocument& doc, SprintData& out, const char** err) {
    if ((doc["v"] | 0) != 1) return fail(err, "unsupported version");

    SprintData tmp;                      // müvəqqəti: yarımçıq fayl canlı datanı korlamasın
    memset(&tmp, 0, sizeof(tmp));

    JsonObjectConst sp = doc["sprint"];
    if (sp.isNull()) return fail(err, "sprint missing");

    copyStr(tmp.name, sizeof(tmp.name), sp["name"] | "");
    if (tmp.name[0] == '\0') return fail(err, "sprint.name empty");

    if (!parseDate(sp["start"] | "", tmp.start)) return fail(err, "sprint.start invalid");
    if (!parseDate(sp["end"] | "", tmp.end))     return fail(err, "sprint.end invalid");
    if (daysBetween(tmp.start, tmp.end) < 0)     return fail(err, "sprint.end before start");

    const int done  = sp["done"]  | -1;
    const int total = sp["total"] | -1;
    if (done < 0 || total < 0 || done > total || total > 9999)
        return fail(err, "sprint.done/total invalid");
    tmp.done  = static_cast<uint16_t>(done);
    tmp.total = static_cast<uint16_t>(total);

    // statistika məcburi deyil: olmayan dəyər 0 olur
    JsonObjectConst st = doc["stats"];
    copyStr(tmp.stats.topName, sizeof(tmp.stats.topName), st["topAssignee"]["name"] | "");
    tmp.stats.topCount  = toU16(st["topAssignee"]["count"] | 0);
    tmp.stats.todo      = toU16(st["byStatus"]["todo"] | 0);
    tmp.stats.inProgress= toU16(st["byStatus"]["inProgress"] | 0);
    tmp.stats.done      = toU16(st["byStatus"]["done"] | 0);
    tmp.stats.overdue   = toU16(st["overdue"] | 0);
    tmp.stats.blocked   = toU16(st["blocked"] | 0);
    tmp.stats.pace      = parsePace(st["pace"] | "onTrack");

    // tasklar: limitdə dayan, xarab olanı atla
    for (JsonObjectConst t : doc["tasks"].as<JsonArrayConst>()) {
        if (tmp.taskCount >= kMaxTasks) break;
        const char* status = t["s"] | "";
        const char* title  = t["t"] | "";
        if (!validStatus(status) || title[0] == '\0') continue;

        Task& dst = tmp.tasks[tmp.taskCount++];
        copyStr(dst.key,      sizeof(dst.key),      t["k"] | "");
        copyStr(dst.title,    sizeof(dst.title),    title);
        copyStr(dst.status,   sizeof(dst.status),   status);
        copyStr(dst.assignee, sizeof(dst.assignee), t["a"] | "");
    }

    tmp.valid = true;
    out = tmp;                            // yalnız burada "commit"
    return true;
}

bool parseSprintJson(const char* json, SprintData& out, const char** err) {
    // bəzi Windows redaktorları faylın əvvəlinə BOM (EF BB BF) qoyur
    if ((uint8_t)json[0] == 0xEF && (uint8_t)json[1] == 0xBB && (uint8_t)json[2] == 0xBF) json += 3;
    JsonDocument doc;
    const DeserializationError e = deserializeJson(doc, json);
    if (e) return fail(err, e.c_str());
    return fillSprint(doc, out, err);
}