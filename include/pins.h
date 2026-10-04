#pragma once
// GPIO assignment for ESP32-C3 (SuperMini and the Wokwi DevKitM-1 model).
//
// The full plan is listed so later phases do not collide with Phase 1.
// Only the Nokia 5110 pins are used in Phase 1.
//
//   GPIO  Use                      Notes
//   0     Nokia CLK                free GPIO
//   1     Nokia DIN                free GPIO
//   2     Nokia RST                strapping pin; harmless here, see README
//   3     Nokia DC                 free GPIO
//   10    Nokia CE                 free GPIO
//   4     SD SCK       (Phase 4)   default hardware SPI pins
//   5     SD MISO      (Phase 4)
//   6     SD MOSI      (Phase 4)
//   7     SD CS        (Phase 4)
//   8     DS3231 SDA   (Phase 3)   strapping pin + onboard LED; module pull-up is fine
//   9     DS3231 SCL   (Phase 3)   strapping pin + BOOT button; module pull-up is fine
//   20    DFPlayer RX  (Phase 6)   ESP TX -> DFPlayer RX (through 1k resistor)
//   21    DFPlayer TX  (Phase 6)   DFPlayer TX -> ESP RX
//   18/19 USB D-/D+                reserved for native USB, never used

namespace pins {
constexpr int8_t kLcdClk = 0;
constexpr int8_t kLcdDin = 1;
constexpr int8_t kLcdDc  = 3;
constexpr int8_t kLcdCe  = 10;
constexpr int8_t kLcdRst = 2;

// DEV only: passive buzzer standing in for the DFPlayer (GPIO7 is not needed for
// an SD module because the JSON files live in internal flash).
constexpr int8_t kBuzzer = 7;

// Simulation-only ILI9341 (hardware SPI on the default pins: SCK=4, MISO=5, MOSI=6).
constexpr int8_t kTftCs  = 10;
constexpr int8_t kTftDc  = 3;
constexpr int8_t kTftRst = 2;
}  // namespace pins
