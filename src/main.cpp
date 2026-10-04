#include <Arduino.h>

#include "app/Pager.h"
#include "app/SampleData.h"
#include "app/Screens.h"
#include "app/SprintData.h"
#include "pins.h"

#if defined(DISPLAY_ILI9341)
#include "display/Ili9341Display.h"
static Ili9341Display display(pins::kTftCs, pins::kTftDc, pins::kTftRst);
#else
#include "display/Pcd8544Display.h"
static Pcd8544Display display(pins::kLcdClk, pins::kLcdDin, pins::kLcdDc, pins::kLcdCe,
                              pins::kLcdRst, 55);
#endif

static SprintData sprint;
static Pager pager;

// Fixed "today" until the DS3231 is added in Phase 3.
static const Date kToday = {2026, 10, 4};

// Demo only: shows the alert interrupt once, 20 s after boot. In Phase 6 this is
// triggered by the rules in /device/alerts.json together with an MP3.
static bool demoAlertShown = false;

void setup() {
    Serial.begin(115200);
    delay(300);  // let USB CDC enumerate on real hardware
    Serial.println("[boot] Jira Desk Dashboard - Phase 2");

    display.begin();
    makeSampleData(sprint);
    pager.rebuild(sprint, millis());
    Serial.printf("[pager] %u pages\n", static_cast<unsigned>(pager.pageCount()));
}

void loop() {
    const uint32_t now = millis();

    if (!demoAlertShown && now > 20000) {
        demoAlertShown = true;
        pager.showAlert("Xəbərdarlıq", "Sprintin bitməsinə 14 gün qalıb", now);
        Serial.println("[demo] alert shown");
    }

    if (pager.tick(now)) {
        const Page p = pager.current();
        Serial.printf("[pager] kind=%u index=%u\n", static_cast<unsigned>(p.kind),
                      static_cast<unsigned>(p.index));
        renderPage(display, sprint, kToday, pager);
    }
}
