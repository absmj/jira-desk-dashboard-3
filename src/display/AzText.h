#pragma once
// UTF-8 text with custom glyphs for the Azerbaijani letters that the built-in
// Adafruit 5x7 font lacks: ə Ə ı İ ö Ö ü Ü ç Ç ş Ş ğ Ğ.
//
// ASCII is drawn with the display's own font; the 14 special letters are drawn
// pixel by pixel from the table in AzText.cpp. Any other non-ASCII character is
// shown as '?'. The web app must send text as Unicode NFC (String.normalize).
#include <stddef.h>
#include <stdint.h>

#include "IDisplay.h"

namespace aztext {

constexpr uint8_t kCellW = 6;  // advance per character at size 1
constexpr uint8_t kCellH = 8;
constexpr uint8_t kGlyphRows = 8;

// Decodes one code point and advances p. Returns 0 at end of string and
// 0xFFFD for malformed input (p always advances by at least one byte).
uint32_t nextCodepoint(const char*& p);

// Number of code points (not bytes).
size_t length(const char* utf8);

// Copies at most maxChars code points into out. If the text is longer, the last
// kept character is replaced by '.'. Never cuts a multi-byte character.
void fit(char* out, size_t outSize, const char* utf8, uint8_t maxChars);

// 8 rows, 5 bits each (bit 4 = leftmost column), or nullptr if cp has no glyph.
const uint8_t* glyphRows(uint32_t cp);

// Draws text with the top-left at (x, y). inverted = draw pixels "off", used on
// top of a filled background.
void draw(IDisplay& d, int16_t x, int16_t y, const char* utf8, uint8_t size = 1,
          bool inverted = false);

}  // namespace aztext
