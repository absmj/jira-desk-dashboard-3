#pragma once
#include <Adafruit_PCD8544.h>

#include "IDisplay.h"

// IDisplay implementation for the Nokia 5110 (PCD8544), 84x48 monochrome.
// Uses software SPI so the pins can be any free GPIO and the hardware SPI bus
// stays free for the microSD card in a later phase.
class Pcd8544Display : public IDisplay {
public:
    Pcd8544Display(int8_t clk, int8_t din, int8_t dc, int8_t ce, int8_t rst, uint8_t contrast = 55);

    bool begin() override;
    int16_t width() const override { return 84; }
    int16_t height() const override { return 48; }

    void clear() override;
    void flush() override;
    void text(int16_t x, int16_t y, const char* s, uint8_t textSize = 1,
              bool inverted = false) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) override;
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h) override;
    void progressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct) override;

private:
    Adafruit_PCD8544 lcd_;
    uint8_t contrast_;
};
