#include "JsonFile.h"

static constexpr size_t kMaxJsonBytes = 8192;  // hard cap: protects RAM from a bad file

bool readJsonFile(fs::FS& fs, const char* path, JsonDocument& doc, const char** err) {
    File f = fs.open(path, FILE_READ);
    if (!f) {
        if (err) *err = "file not found";
        return false;
    }
    if (f.size() > kMaxJsonBytes) {
        f.close();
        if (err) *err = "file too large";
        return false;
    }
    if (f.peek() == 0xEF) {  // UTF-8 BOM added by some Windows editors: skip it
        f.read();
        f.read();
        f.read();
    }
    const DeserializationError e = deserializeJson(doc, f);  // reads straight from the file
    f.close();
    if (e) {
        if (err) *err = e.c_str();
        return false;
    }
    return true;
}
