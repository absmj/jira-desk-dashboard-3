#pragma once
// UTF-8 -> HD44780 character codes. Pure C++, tested on a PC.
//
// The HD44780 ROM has ASCII but none of the Azerbaijani letters, and only 8 programmable
// characters (CGRAM slots 0..7). Per frame we collect the distinct special letters that are
// on screen (at most 8) and give each a slot; slots keep their bitmap between frames so
// unchanged letters are not uploaded again. A frame that needs more than 8 distinct letters
// shows the extra ones as plain ASCII (ə -> e, ş -> s, ...). Unknown characters become '?'.
#include <stddef.h>
#include <stdint.h>

namespace lcdtext {

constexpr uint8_t kSlots = 8;
constexpr uint8_t kMaxCols = 20;
constexpr uint8_t kMaxRows = 4;

struct GlyphCache {
    uint32_t cp[kSlots] = {};          // code point loaded in each slot, 0 = empty
    uint8_t bitmap[kSlots][8] = {};    // what the panel's CGRAM holds for each slot
    bool dirty[kSlots] = {};           // slot changed since the last upload
};

struct Frame {
    uint8_t cols = 16;
    uint8_t rows = 2;
    uint8_t code[kMaxRows][kMaxCols] = {};
    uint8_t substituted = 0;           // letters shown as ASCII because the 8 slots were full
};

// Encodes `lineCount` UTF-8 lines into `out` (padding with spaces), updating the cache.
void encode(const char* const* lines, uint8_t lineCount, GlyphCache& cache, Frame& out);

// ASCII stand-in for a special letter, or 0 if there is none.
char transliterate(uint32_t cp);

}  // namespace lcdtext
