#pragma once
#include <stdint.h>

// A character display (HD44780 16x2 and similar). Screens hand over whole UTF-8 lines;
// the driver decides how each character is shown (ROM character, custom glyph or a plain
// ASCII stand-in). Pure C++ (no Arduino include) so the layout code is tested on a PC.
class ITextDisplay {
public:
    virtual ~ITextDisplay() = default;

    virtual bool begin() = 0;
    virtual uint8_t cols() const = 0;
    virtual uint8_t rows() const = 0;

    // Replaces the whole screen. lines[i] is UTF-8, shorter lines are padded with spaces,
    // longer ones are cut at cols() code points. Only changed rows are sent to the panel.
    virtual void show(const char* const* lines, uint8_t lineCount) = 0;

    virtual void setBacklight(bool on) = 0;
};
