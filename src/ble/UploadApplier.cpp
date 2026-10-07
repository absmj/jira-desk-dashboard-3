#include "UploadApplier.h"

#include <ArduinoJson.h>

#include "../app/AlertLoader.h"
#include "../app/SprintLoader.h"

uint8_t applyUpload(uint8_t fileId, const uint8_t* json, size_t n, SprintData& sprint, AlertConfig& alerts,
                    IFileStore& store) {
    JsonDocument doc;
    if (deserializeJson(doc, reinterpret_cast<const char*>(json), n)) return 3;

    if (fileId == kFileSprint) {
        static SprintData tmp;  // static: too big for the loop task's stack
        if (!fillSprint(doc, tmp)) return 3;
        if (!store.writeAtomic("/device/sprint.json", json, n)) return 4;
        sprint = tmp;
        return 0;
    }
    if (fileId == kFileAlerts) {
        static AlertConfig tmp;
        if (!fillAlerts(doc, tmp)) return 3;
        if (!store.writeAtomic("/device/alerts.json", json, n)) return 4;
        alerts = tmp;
        return 0;
    }
    return 3;
}
