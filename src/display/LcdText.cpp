#include "LcdText.h"

#include <string.h>

#include "AzText.h"

namespace lcdtext {

char transliterate(uint32_t cp) {
    switch (cp) {
        case 0x0259: return 'e';  // ə
        case 0x018F: return 'E';  // Ə
        case 0x0131: return 'i';  // ı
        case 0x0130: return 'I';  // İ
        case 0x00F6: return 'o';  // ö
        case 0x00D6: return 'O';  // Ö
        case 0x00FC: return 'u';  // ü
        case 0x00DC: return 'U';  // Ü
        case 0x00E7: return 'c';  // ç
        case 0x00C7: return 'C';  // Ç
        case 0x015F: return 's';  // ş
        case 0x015E: return 'S';  // Ş
        case 0x011F: return 'g';  // ğ
        case 0x011E: return 'G';  // Ğ
        default: return 0;
    }
}

namespace {

// Finds or allocates a slot for cp. `used` = slots already referenced in this frame
// (they must not be overwritten). Returns -1 if every slot is in use by other letters.
int slotFor(uint32_t cp, const uint8_t* bitmap, GlyphCache& cache, bool* used) {
    for (uint8_t i = 0; i < kSlots; ++i) {
        if (cache.cp[i] == cp) {
            used[i] = true;
            return i;
        }
    }
    int pick = -1;
    for (uint8_t i = 0; i < kSlots; ++i) {          // empty slot first
        if (cache.cp[i] == 0 && !used[i]) { pick = i; break; }
    }
    if (pick < 0) {
        for (uint8_t i = 0; i < kSlots; ++i) {       // else any slot not needed by this frame
            if (!used[i]) { pick = i; break; }
        }
    }
    if (pick < 0) return -1;
    cache.cp[pick] = cp;
    memcpy(cache.bitmap[pick], bitmap, 8);
    cache.dirty[pick] = true;
    used[pick] = true;
    return pick;
}

}  // namespace

void encode(const char* const* lines, uint8_t lineCount, GlyphCache& cache, Frame& out) {
    bool used[kSlots] = {};
    out.substituted = 0;
    if (out.cols > kMaxCols) out.cols = kMaxCols;
    if (out.rows > kMaxRows) out.rows = kMaxRows;

    for (uint8_t r = 0; r < out.rows; ++r) {
        memset(out.code[r], ' ', kMaxCols);
        const char* p = (r < lineCount && lines[r]) ? lines[r] : "";
        for (uint8_t c = 0; c < out.cols; ++c) {
            const uint32_t cp = aztext::nextCodepoint(p);
            if (cp == 0) break;
            uint8_t code = '?';
            if (cp >= 0x20 && cp < 0x7F) {
                code = static_cast<uint8_t>(cp);
                if (code == '\\') code = '/';   // the ROM shows a yen sign at 0x5C
                if (code == '~') code = '-';    // and an arrow at 0x7E
            } else if (const uint8_t* rows = aztext::glyphRows(cp)) {
                const int s = slotFor(cp, rows, cache, used);
                if (s >= 0) {
                    code = static_cast<uint8_t>(s);
                } else {
                    const char t = transliterate(cp);
                    code = t ? static_cast<uint8_t>(t) : '?';
                    ++out.substituted;
                }
            }
            out.code[r][c] = code;
        }
    }
}

}  // namespace lcdtext
