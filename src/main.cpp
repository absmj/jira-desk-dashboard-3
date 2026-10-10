#include <Arduino.h>
#include <LittleFS.h>
#include <esp_partition.h>

#include "app/AlertEngine.h"
#include "app/AlertLoader.h"
#include "app/JsonFile.h"
#include "app/Pager.h"
#include "app/Screens.h"
#include "app/TextScreens.h"
#include "app/SimClock.h"
#include "app/SprintData.h"
#include "app/SprintLoader.h"
#include "audio/BuzzerAudio.h"
#include "ble/BleManager.h"
#if defined(AUDIO_SIM_MP3)
#include "audio/LittleFsCard.h"
#include "audio/SimMp3Audio.h"
#endif
#include "pins.h"

#ifndef LCD_I2C_ADDR
#define LCD_I2C_ADDR 0x27  // most PCF8574 backpacks; some use 0x3F (platformio.ini build_flags)
#endif

#if defined(DISPLAY_HD44780)
// 16x2 character LCD behind a PCF8574 I2C backpack (docs/lcd1602.md). Text-only: renderTextPage().
#include <Wire.h>
#include "display/Hd44780Display.h"
#include "display/Pcf8574Port.h"
static Pcf8574Port lcdPort(LCD_I2C_ADDR);
static Hd44780Display display(lcdPort, 16, 2);
#elif defined(DISPLAY_ILI9341)
#include "display/Ili9341Display.h"
static Ili9341Display display(pins::kTftCs, pins::kTftDc, pins::kTftRst);
#else
#include "display/Pcd8544Display.h"
static Pcd8544Display display(pins::kLcdClk, pins::kLcdDin, pins::kLcdDc, pins::kLcdCe,
                              pins::kLcdRst, 55);
#endif

#ifndef DEV_CLOCK_SPEED
#define DEV_CLOCK_SPEED 1
#endif

static SprintData sprint;      // zero-initialised: valid == false until a file loads
static AlertConfig alerts;     // zero-initialised: count == 0 -> nothing ever fires
static Pager pager;
static AlertEngine engine;
static SimClock simClock;
static BuzzerAudio buzzer(pins::kBuzzer);  // dev speaker
#if defined(AUDIO_SIM_MP3)
// Dev: "MP3 files" live in LittleFS under /MP3/NNNN.mp3 and are played as buzzer melodies.
static LittleFsCard card;
static SimMp3Audio simMp3(card, buzzer);
static IAudio& audio = simMp3;
#else
static IAudio& audio = buzzer;
#endif

static BleManager ble;  // real receiver only with -DBLE_RECEIVER (supermini); a no-op stub in the simulator
static uint32_t lastPollMs = 0;

// Boot diagnostic: where the firmware EXPECTS the filesystem, and what is actually there.
// "littlefs" in the first bytes = a valid image; all FF = blank flash (image not merged
// or merged at another offset).
static void dumpFsPartition() {
    const esp_partition_t* p = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
    if (!p) {
        Serial.println("[fs] no spiffs/littlefs partition in the partition table");
        return;
    }
    Serial.printf("[fs] partition '%s' offset=0x%x size=0x%x\n", p->label,
                  static_cast<unsigned>(p->address), static_cast<unsigned>(p->size));
    uint8_t b[16] = {0};
    if (esp_partition_read(p, 0, b, sizeof(b)) != ESP_OK) {
        Serial.println("[fs] cannot read the partition");
        return;
    }
    Serial.print("[fs] first bytes:");
    for (uint8_t v : b) Serial.printf(" %02X", v);
    Serial.println();
}

// Boot result, repeated in the 5 s heartbeat (the monitor often connects after the boot lines were printed).
static bool gFsOk = false;
static char gSprintState[48] = "not loaded";
static char gAlertsState[48] = "not loaded";

static void loadData() {
    const char* err = nullptr;
    JsonDocument doc;

    if (readJsonFile(LittleFS, "/device/sprint.json", doc, &err) && fillSprint(doc, sprint, &err)) {
        Serial.println("[data] sprint.json loaded");
        snprintf(gSprintState, sizeof gSprintState, "OK");
    } else {
        Serial.printf("[data] sprint.json rejected: %s\n", err);
        snprintf(gSprintState, sizeof gSprintState, "ERR %s", err ? err : "?");
    }

    doc.clear();
    err = nullptr;
    if (readJsonFile(LittleFS, "/device/alerts.json", doc, &err) && fillAlerts(doc, alerts, &err)) {
        Serial.printf("[data] alerts.json loaded: %u rules, %u skipped\n",
                      static_cast<unsigned>(alerts.count), static_cast<unsigned>(alerts.skipped));
        snprintf(gAlertsState, sizeof gAlertsState, "OK %u rules", static_cast<unsigned>(alerts.count));
    } else {
        Serial.printf("[data] alerts.json rejected: %s\n", err);
        snprintf(gAlertsState, sizeof gAlertsState, "ERR %s", err ? err : "?");
    }
}

void setup() {
    Serial.begin(115200);
    // Native USB: the monitor reconnects after every reset, so wait (max 3 s) for it, otherwise the boot lines are lost.
    for (uint32_t t0 = millis(); !Serial && millis() - t0 < 3000;) delay(10);
    delay(300);  // let USB CDC enumerate on real hardware
    Serial.println("[boot] Jira Desk Dashboard - Phase 2b");

#if defined(DISPLAY_HD44780)
    Wire.begin(pins::kI2cSda, pins::kI2cScl);  // shared with the DS3231 later (different address)
    pager.setTaskRows(2);                      // two task rows fit a 16x2 page
#endif
    display.begin();
    audio.begin();
    audio.setVolume(20);

    if (!LittleFS.begin(false)) {  // false = never auto-format: protect existing data
        Serial.println("[fs] LittleFS mount failed: the flash partition is blank or has no valid image.");
        Serial.println("[fs] Run `pio run -e sim -t buildfs`, rebuild, and check merge_firmware output.");
        dumpFsPartition();
    } else {
        gFsOk = true;
        loadData();
    }

    // Dev clock: starts Sunday 2026-10-04 09:30 and runs DEV_CLOCK_SPEED times faster.
    // 09:30 (not 09:50) so the 09:55 "standup" minute is not swallowed by the two
    // data alerts that fire right after boot while their screens are showing.
    simClock.begin(DateTime{Date{2026, 10, 4}, 9, 30, 0}, millis(), DEV_CLOCK_SPEED);

    ble.begin("JiraDesk");

    pager.rebuild(sprint, millis());
    Serial.printf("[pager] %u pages\n", static_cast<unsigned>(pager.pageCount()));
}

void loop() {
    const uint32_t now = millis();
    audio.tick(now);

    // Heartbeat every 5 s: proves the program is running even if the monitor was opened late.
    static uint32_t lastBeatMs = 0;
    if (static_cast<uint32_t>(now - lastBeatMs) >= 5000) {
        lastBeatMs = now;
        Serial.printf("[alive] %lu s | pages=%u | fs=%s | sprint=%s | alerts=%s\n",
                      static_cast<unsigned long>(now / 1000), static_cast<unsigned>(pager.pageCount()),
                      gFsOk ? "OK" : "MOUNT FAILED", gSprintState, gAlertsState);
    }

    // A file received over Bluetooth was verified, stored and swapped in: refresh what depends on it.
    switch (ble.poll(now, sprint, alerts)) {
    case 1: pager.rebuild(sprint, now); Serial.println("[ble] new sprint.json active"); break;
    case 2: engine.reset(); Serial.println("[ble] new alerts.json active"); break;
    default: break;
    }
#if defined(AUDIO_SIM_MP3)
    uint16_t doneTrack = 0;
    if (simMp3.takeFinished(doneTrack)) {
        Serial.printf("[audio] track %u finished\n", static_cast<unsigned>(doneTrack));
    }
#endif

    // Poll the rules 4x per second; only one alert on screen at a time.
    if (static_cast<uint32_t>(now - lastPollMs) >= 250) {
        lastPollMs = now;
        if (!pager.alertActive()) {
            if (const AlertRule* r = engine.poll(alerts, sprint, simClock.now(now))) {
                pager.showAlert("Diqqət!", r->text[0] ? r->text : r->id, now);
                if (r->track && !audio.play(r->track, now)) {
                    Serial.printf("[audio] track %u not found (/MP3/%04u.mp3)\n",
                                  static_cast<unsigned>(r->track), static_cast<unsigned>(r->track));
                }
                Serial.printf("[alert] %s track=%u\n", r->id, static_cast<unsigned>(r->track));
            }
        }
    }

#if defined(DISPLAY_HD44780)
    // A long alert message is shown two lines at a time, so it needs a redraw even without a page change.
    static uint32_t lastAlertDrawMs = 0;
    const bool alertTick = pager.alertActive() && static_cast<uint32_t>(now - lastAlertDrawMs) >= kAlertChunkMs / 2;
#else
    const bool alertTick = false;
#endif
    if (pager.tick(now) || alertTick) {
        const DateTime dt = simClock.now(now);
#if defined(DISPLAY_HD44780)
        lastAlertDrawMs = now;
        renderTextPage(display, sprint, dt.date, pager, now);
#else
        renderPage(display, sprint, dt.date, pager);
#endif
    }
}
