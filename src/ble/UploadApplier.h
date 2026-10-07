#pragma once
#include <stddef.h>
#include <stdint.h>

#include "../app/AlertRules.h"
#include "../app/SprintData.h"

// Where verified files are persisted. LittleFsStore implements it on the device, tests use a RAM fake.
class IFileStore {
public:
    virtual ~IFileStore() = default;
    // Must leave the old file untouched when it returns false.
    virtual bool writeAtomic(const char* path, const uint8_t* data, size_t n) = 0;
};

constexpr uint8_t kFileSprint = 1, kFileAlerts = 2;

// Validates `json` (already CRC-checked), stores it, and only then swaps the live data.
// Returns the status code of docs/ble-protocol.md: 0 ok, 3 JSON rejected / unknown file, 4 storage error.
// A rejected or failed upload never changes `sprint`, `alerts` or the stored file.
uint8_t applyUpload(uint8_t fileId, const uint8_t* json, size_t n, SprintData& sprint, AlertConfig& alerts,
                    IFileStore& store);
