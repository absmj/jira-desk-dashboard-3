#include "Pcd8544Display.h"

// Adafruit_GFX colour on this 1-bit panel: 1 = pixel on (dark), 0 = pixel off.
// Plain constants are used because the macro names (BLACK/WHITE) differ
// between library versions, and PCD8544_BLACK does not exist in v2.x.
static constexpr uint16_t kOn = 1;
static constexpr uint16_t kOff = 0;

Pcd8544Display::Pcd8544Display(int8_t clk, int8_t din, int8_t dc, int8_t ce, int8_t rst,
                               uint8_t contrast)
    : lcd_(clk, din, dc, ce, rst), contrast_(contrast) {}

bool Pcd8544Display::begin() {
    // The driver cannot report a missing panel (write-only bus), so this
    // always returns true. Verify on real hardware by looking at the glass.
    lcd_.begin(contrast_);
    lcd_.setTextWrap(false);
    lcd_.clearDisplay();
    lcd_.display();
    return true;
}

void Pcd8544Display::clear() { lcd_.clearDisplay(); }

void Pcd8544Display::flush() { lcd_.display(); }

void Pcd8544Display::text(int16_t x, int16_t y, const char* s, uint8_t textSize, bool inverted) {
    lcd_.setTextSize(textSize);
    lcd_.setTextColor(inverted ? kOff : kOn);
    lcd_.setCursor(x, y);
    lcd_.print(s);
}

void Pcd8544Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
    lcd_.fillRect(x, y, w, h, on ? kOn : kOff);
}

void Pcd8544Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    lcd_.drawRect(x, y, w, h, kOn);
}

void Pcd8544Display::progressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct) {
    if (pct > 100) pct = 100;
    drawRect(x, y, w, h);
    const int16_t innerW = w - 4;  // 1px border + 1px gap on each side
    const int16_t fillW = (innerW * pct) / 100;
    if (fillW > 0) fillRect(x + 2, y + 2, fillW, h - 4, true);
}
