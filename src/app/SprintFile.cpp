#include "SprintFile.h"
#include "SprintLoader.h"

static constexpr size_t kMaxJsonBytes = 8192;   // yaddaşı qorumaq üçün sərt limit

bool loadSprintFile(fs::FS& fs, const char* path, SprintData& out, const char** err) {
    File f = fs.open(path, FILE_READ);
    if (!f)                     { if (err) *err = "file not found"; return false; }
    if (f.size() > kMaxJsonBytes) { f.close(); if (err) *err = "file too large"; return false; }

    if (f.peek() == 0xEF) { f.read(); f.read(); f.read(); }   // BOM-u burax

    JsonDocument doc;
    const DeserializationError e = deserializeJson(doc, f);    // birbaşa fayldan oxuyur
    f.close();
    if (e) { if (err) *err = e.c_str(); return false; }

    return fillSprint(doc, out, err);
}