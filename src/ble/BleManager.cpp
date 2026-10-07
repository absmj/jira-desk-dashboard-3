#ifdef BLE_RECEIVER
#include "BleManager.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <NimBLEDevice.h>

#include "LittleFsStore.h"
#include "TransferSession.h"
#include "UploadApplier.h"

namespace {
const char* kService = "4a5b0001-7d8e-4f10-9a2b-3c4d5e6f7081";
const char* kRx = "4a5b0002-7d8e-4f10-9a2b-3c4d5e6f7081";
const char* kStatus = "4a5b0003-7d8e-4f10-9a2b-3c4d5e6f7081";

TransferSession session;           // shared between the BLE task (onWrite) and loop(): guarded by `lock`
SemaphoreHandle_t lock = nullptr;  // a mutex, not a critical section: the CRC of 8 KB must not block interrupts
NimBLECharacteristic* statusChar = nullptr;
volatile bool replyPending = false;
uint8_t replyCode = 0, replyFile = 0;
volatile bool completePending = false;

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        const NimBLEAttValue v = c->getValue();
        xSemaphoreTake(lock, portMAX_DELAY);
        const TransferSession::Reaction r = session.onFrame(v.data(), v.length(), millis());
        if (r.kind == TransferSession::Reaction::Reply) { replyPending = true; replyCode = r.status; replyFile = r.fileId; }
        if (r.kind == TransferSession::Reaction::Complete) completePending = true;
        xSemaphoreGive(lock);
    }
};
RxCallbacks rxCallbacks;

void notifyStatus(uint8_t code, uint8_t fileId) {
    const uint8_t msg[2] = {code, fileId};
    if (statusChar) statusChar->notify(msg, 2);
    Serial.printf("[ble] status code=%u file=%u\n", code, fileId);
}
}  // namespace

void BleManager::begin(const char* deviceName) {
    lock = xSemaphoreCreateMutex();
    NimBLEDevice::init(deviceName);
    NimBLEServer* server = NimBLEDevice::createServer();
    server->advertiseOnDisconnect(true);
    NimBLEService* svc = server->createService(kService);
    NimBLECharacteristic* rx = svc->createCharacteristic(kRx, NIMBLE_PROPERTY::WRITE);
    rx->setCallbacks(&rxCallbacks);
    statusChar = svc->createCharacteristic(kStatus, NIMBLE_PROPERTY::NOTIFY);
    svc->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(kService);   // the web app filters on this
    adv->enableScanResponse(true);   // name goes in the scan response: a 128-bit UUID leaves little room
    adv->setName(deviceName);
    adv->start();
    Serial.printf("[ble] advertising as '%s'\n", deviceName);
}

uint8_t BleManager::poll(uint32_t nowMs, SprintData& sprint, AlertConfig& alerts) {
    bool doReply = false, doApply = false;
    uint8_t code = 0, file = 0;
    xSemaphoreTake(lock, portMAX_DELAY);
    session.tick(nowMs);
    if (replyPending) { replyPending = false; doReply = true; code = replyCode; file = replyFile; }
    if (completePending) { completePending = false; doApply = true; }
    xSemaphoreGive(lock);

    if (doReply) notifyStatus(code, file);
    if (!doApply) return 0;

    // The buffer is stable here: while the session is Complete it rejects every frame.
    static LittleFsStore store(LittleFS);
    file = session.fileId();
    code = applyUpload(file, session.data(), session.size(), sprint, alerts, store);
    xSemaphoreTake(lock, portMAX_DELAY);
    session.finish();
    xSemaphoreGive(lock);
    notifyStatus(code, file);
    if (code != 0) return 0;
    return file == kFileSprint ? 1 : 2;
}
#else
#include "BleManager.h"
void BleManager::begin(const char*) {}
uint8_t BleManager::poll(uint32_t, SprintData&, AlertConfig&) { return 0; }
#endif
