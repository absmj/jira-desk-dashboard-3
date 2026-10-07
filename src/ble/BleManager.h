#pragma once
#include <stdint.h>

#include "../app/AlertRules.h"
#include "../app/SprintData.h"

// BLE GATT server that receives sprint.json / alerts.json (docs/ble-protocol.md).
// Hardware-only: Wokwi cannot simulate BLE, so this is built for the `supermini` env only
// (flag -DBLE_RECEIVER) and has NOT been run on a device yet.
class BleManager {
public:
    // Call once after LittleFS is mounted.
    void begin(const char* deviceName);
    // Call from loop(). Applies a completed upload and notifies the web app.
    // Returns 1 if a new sprint was applied, 2 if new alerts were applied, 0 otherwise.
    uint8_t poll(uint32_t nowMs, SprintData& sprint, AlertConfig& alerts);
};
