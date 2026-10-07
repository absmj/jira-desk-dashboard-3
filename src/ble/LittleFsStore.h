#pragma once
#include <FS.h>
#include <string.h>

#include "UploadApplier.h"

// Writes <path>.tmp, checks the size, then renames it over the old file. If power is lost before
// the rename, the old file is still there (a leftover .tmp is overwritten by the next upload).
class LittleFsStore : public IFileStore {
public:
    explicit LittleFsStore(fs::FS& fs) : fs_(fs) {}

    bool writeAtomic(const char* path, const uint8_t* data, size_t n) override {
        char tmp[40];
        const size_t pl = strlen(path);
        if (pl + 5 > sizeof(tmp)) return false;
        memcpy(tmp, path, pl);
        memcpy(tmp + pl, ".tmp", 5);

        if (!fs_.exists("/device")) fs_.mkdir("/device");
        {
            File f = fs_.open(tmp, FILE_WRITE);
            if (!f) return false;
            const size_t w = f.write(data, n);
            f.close();
            if (w != n) { fs_.remove(tmp); return false; }
        }
        {
            File f = fs_.open(tmp, FILE_READ);
            const bool good = f && f.size() == n;
            if (f) f.close();
            if (!good) { fs_.remove(tmp); return false; }
        }
        // LittleFS rename replaces an existing target atomically. Only if that is refused do we
        // fall back to remove + rename (a short window without the file; the next boot would then
        // fall back to "no data" instead of corrupt data).
        if (fs_.rename(tmp, path)) return true;
        fs_.remove(path);
        if (fs_.rename(tmp, path)) return true;
        fs_.remove(tmp);
        return false;
    }

private:
    fs::FS& fs_;
};
