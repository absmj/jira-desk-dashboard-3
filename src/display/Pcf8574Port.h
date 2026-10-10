#pragma once
// Device-only: HD44780 behind the common PCF8574 "I2C backpack".
// Bit map of the usual backpack: P0 RS, P1 RW, P2 EN, P3 backlight, P4..P7 D4..D7.
// Some boards wire it differently (rare): then only the constants below change.
#include <Arduino.h>
#include <Wire.h>

#include "Hd44780.h"

class Pcf8574Port : public IHd44780Port {
public:
    explicit Pcf8574Port(uint8_t address = 0x27, TwoWire& wire = Wire) : addr_(address), wire_(wire) {}

    // Call Wire.begin(sda, scl) first (shared with the DS3231).
    void writeNibble(uint8_t nibble, bool rs) override {
        const uint8_t base = static_cast<uint8_t>(((nibble & 0x0F) << 4) | (rs ? kRs : 0) | (backlight_ ? kBl : 0));
        put(base);
        put(base | kEn);
        delayMicroseconds(1);   // Enable pulse >= 450 ns
        put(base);
    }
    void delayUs(uint32_t us) override {
        if (us >= 2000) delay((us + 999) / 1000); else delayMicroseconds(us);
    }
    void setBacklight(bool on) override {
        backlight_ = on;
        put(on ? kBl : 0);
    }

private:
    static constexpr uint8_t kRs = 0x01, kEn = 0x04, kBl = 0x08;
    void put(uint8_t b) {
        wire_.beginTransmission(addr_);
        wire_.write(b);
        wire_.endTransmission();
    }
    uint8_t addr_;
    TwoWire& wire_;
    bool backlight_ = true;
};
