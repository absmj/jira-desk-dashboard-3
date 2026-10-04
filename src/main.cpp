#include <Arduino.h>
#include <LittleFS.h>

#include "app/AlertEngine.h"
#include "app/AlertLoader.h"
#include "app/JsonFile.h"
#include "app/Pager.h"
#include "app/Screens.h"
#include "app/SimClock.h"
#include "app/SprintData.h"
#include "app/SprintLoader.h"
#include "audio/BuzzerAudio.h"
#include "pins.h"

#if defined(DISPLAY_ILI9341)
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
static BuzzerAudio audio(pins::kBuzzer);  // dev stand-in for the DFPlayer

static uint32_t lastPollMs = 0;

static void loadData() {
    const char* err = nullptr;
    JsonDocument doc;

    if (readJsonFile(LittleFS, "/device/sprint.json", doc, &err) && fillSprint(doc, sprint, &err)) {
        Serial.println("[data] sprint.json loaded");
    } else {
        Serial.printf("[data] sprint.json rejected: %s\n", err);
    }

    doc.clear();
    err = nullptr;
    if (readJsonFile(LittleFS, "/device/alerts.json", doc, &err) && fillAlerts(doc, alerts, &err)) {
        Serial.printf("[data] alerts.json loaded: %u rules, %u skipped\n",
                      static_cast<unsigned>(alerts.count), static_cast<unsigned>(alerts.skipped));
    } else {
        Serial.printf("[data] alerts.json rejected: %s\n", err);
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);  // let USB CDC enumerate on real hardware
    Serial.println("[boot] Jira Desk Dashboard - Phase 2b");

    display.begin();
    audio.begin();

    if (!LittleFS.begin(false)) {  // false = never auto-format: protect existing data
        Serial.println("[fs] LittleFS mount failed (no filesystem image in flash?)");
    } else {
        loadData();
    }

    // Dev clock: starts Sunday 2026-10-04 09:30 and runs DEV_CLOCK_SPEED times faster.
    // 09:30 (not 09:50) so the 09:55 "standup" minute is not swallowed by the two
    // data alerts that fire right after boot while their screens are showing.
    simClock.begin(DateTime{Date{2026, 10, 4}, 9, 30, 0}, millis(), DEV_CLOCK_SPEED);

    pager.rebuild(sprint, millis());
    Serial.printf("[pager] %u pages\n", static_cast<unsigned>(pager.pageCount()));
}

void loop() {
    const uint32_t now = millis();
    audio.tick(now);

    // Poll the rules 4x per second; only one alert on screen at a time.
    if (static_cast<uint32_t>(now - lastPollMs) >= 250) {
        lastPollMs = now;
        if (!pager.alertActive()) {
            if (const AlertRule* r = engine.poll(alerts, sprint, simClock.now(now))) {
                pager.showAlert("Diqqət!", r->text[0] ? r->text : r->id, now);
                if (r->track) audio.play(r->track);
                Serial.printf("[alert] %s track=%u\n", r->id, static_cast<unsigned>(r->track));
            }
        }
    }

    if (pager.tick(now)) {
        const DateTime dt = simClock.now(now);
        renderPage(display, sprint, dt.date, pager);
    }
}
