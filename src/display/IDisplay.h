#pragma once
#include <stdint.h>

// Minimal drawing surface used by all screens. Screens never include the
// PCD8544 library directly, so the panel can be swapped without touching them.
// Pure C++ (no Arduino include) so screens can be tested on a PC.
class IDisplay {
public:
    virtual ~IDisplay() = default;

    virtual bool begin() = 0;
    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;

    virtual void clear() = 0;
    virtual void flush() = 0;  // push the frame buffer to the panel

    // ASCII text with the built-in 6x8 font (textSize 1 = 6x8 px per character).
    // Non-ASCII text (Azerbaijani letters) goes through aztext::draw(), which
    // calls this for ASCII and draws custom glyphs with fillRect().
    virtual void text(int16_t x, int16_t y, const char* s, uint8_t textSize = 1,
                      bool inverted = false) = 0;

    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) = 0;
    virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h) = 0;

    // Horizontal progress bar, pct is clamped to 0..100.
    virtual void progressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct) = 0;
};
