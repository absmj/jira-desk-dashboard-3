#pragma once
#include <Adafruit_ILI9341.h>
#include <SPI.h>

#include "IDisplay.h"

// SIMULATION-ONLY stand-in for the Nokia 5110.
//
// Presents the same 84x48 "virtual pixel" surface as Pcd8544Display, but each
// virtual pixel is drawn as a kScale x kScale block on a 320x240 ILI9341, so the
// existing screens render unchanged. It is not equivalent to the real panel:
// colours, contrast, refresh behaviour and speed all differ.
class Ili9341Display : public IDisplay {
public:
    Ili9341Display(int8_t cs, int8_t dc, int8_t rst);

    bool begin() override;
    int16_t width() const override { return 84; }
    int16_t height() const override { return 48; }

    void clear() override;
    void flush() override {}  // drawing is immediate, nothing to push
    void text(int16_t x, int16_t y, const char* s, uint8_t textSize = 1,
              bool inverted = false) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) override;
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h) override;
    void progressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct) override;

private:
    static constexpr int16_t kScale = 3;  // 84*3 = 252, 48*3 = 144
    static constexpr int16_t kOffsetX = (320 - 84 * kScale) / 2;
    static constexpr int16_t kOffsetY = (240 - 48 * kScale) / 2;

    Adafruit_ILI9341 tft_;
    uint16_t fg_;
    uint16_t bg_;
};
