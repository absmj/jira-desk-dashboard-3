#include "Ili9341Display.h"

static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Hardware SPI on the default pins of the board (GPIO4 SCK, GPIO6 MOSI, GPIO5 MISO).
Ili9341Display::Ili9341Display(int8_t cs, int8_t dc, int8_t rst)
    : tft_(cs, dc, rst), fg_(rgb565(20, 30, 20)), bg_(rgb565(176, 196, 160)) {}

bool Ili9341Display::begin() {
    tft_.begin();
    tft_.setRotation(1);  // landscape, 320x240
    tft_.fillScreen(rgb565(0, 0, 0));
    tft_.setTextWrap(false);
    clear();
    return true;
}

void Ili9341Display::clear() {
    tft_.fillRect(kOffsetX, kOffsetY, 84 * kScale, 48 * kScale, bg_);
}

void Ili9341Display::text(int16_t x, int16_t y, const char* s, uint8_t textSize, bool inverted) {
    tft_.setTextSize(textSize * kScale);
    tft_.setTextColor(inverted ? bg_ : fg_);  // no background arg = transparent text
    tft_.setCursor(kOffsetX + x * kScale, kOffsetY + y * kScale);
    tft_.print(s);
}

void Ili9341Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
    tft_.fillRect(kOffsetX + x * kScale, kOffsetY + y * kScale, w * kScale, h * kScale,
                  on ? fg_ : bg_);
}

void Ili9341Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    // 1 virtual pixel border, drawn as four filled strips
    fillRect(x, y, w, 1, true);
    fillRect(x, y + h - 1, w, 1, true);
    fillRect(x, y, 1, h, true);
    fillRect(x + w - 1, y, 1, h, true);
}

void Ili9341Display::progressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct) {
    if (pct > 100) pct = 100;
    drawRect(x, y, w, h);
    const int16_t innerW = w - 4;
    const int16_t fillW = (innerW * pct) / 100;
    if (fillW > 0) fillRect(x + 2, y + 2, fillW, h - 4, true);
}
