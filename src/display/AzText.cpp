#include "AzText.h"

#include <string.h>

namespace aztext {

namespace {

struct Glyph {
    uint32_t cp;
    uint8_t rows[kGlyphRows];  // r0..r7, 5-bit rows, bit 4 = left column
};

// Cell is 5 wide x 8 tall. The built-in font uses rows 0..6; lowercase letters
// sit on rows 2..6 (x-height), so rows 0..1 hold accents and row 7 holds the
// cedilla. Capitals that carry an accent above (Ö İ Ğ) cannot keep full capital
// height in 8 rows and are slightly compressed: this is a documented limit of
// a 5x7 grid, not a bug.
const Glyph kGlyphs[] = {
    // ə  (schwa)
    {0x0259, {0b00000, 0b00000, 0b01110, 0b00001, 0b11111, 0b10001, 0b01110, 0b00000}},
    // Ə
    {0x018F, {0b01110, 0b10001, 0b00001, 0b11111, 0b10001, 0b10001, 0b01110, 0b00000}},
    // ı  (dotless i)
    {0x0131, {0b00000, 0b00000, 0b01100, 0b00100, 0b00100, 0b00100, 0b01110, 0b00000}},
    // İ  (dotted capital I, compressed to x-height with the dot above)
    {0x0130, {0b00100, 0b00000, 0b01110, 0b00100, 0b00100, 0b00100, 0b01110, 0b00000}},
    // ö
    {0x00F6, {0b01010, 0b00000, 0b01110, 0b10001, 0b10001, 0b10001, 0b01110, 0b00000}},
    // Ö  (dots drawn wide so they do not fuse with the top arc)
    {0x00D6, {0b10001, 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110, 0b00000}},
    // ü
    {0x00FC, {0b01010, 0b00000, 0b10001, 0b10001, 0b10001, 0b10011, 0b01101, 0b00000}},
    // Ü
    {0x00DC, {0b01010, 0b00000, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110, 0b00000}},
    // ç
    {0x00E7, {0b00000, 0b00000, 0b01110, 0b10001, 0b10000, 0b10001, 0b01110, 0b00100}},
    // Ç
    {0x00C7, {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110, 0b00100}},
    // ş
    {0x015F, {0b00000, 0b00000, 0b01111, 0b10000, 0b01110, 0b00001, 0b11110, 0b00100}},
    // Ş
    {0x015E, {0b01110, 0b10001, 0b10000, 0b01110, 0b00001, 0b10001, 0b01110, 0b00100}},
    // ğ  (simplified breve: a bar above the g)
    {0x011F, {0b01110, 0b00000, 0b01111, 0b10001, 0b10001, 0b01111, 0b00001, 0b01110}},
    // Ğ  (bar above a compressed G)
    {0x011E, {0b01110, 0b00000, 0b01110, 0b10000, 0b10111, 0b10001, 0b01110, 0b00000}},
};

}  // namespace

uint32_t nextCodepoint(const char*& p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == 0) return 0;
    if (c < 0x80) {
        ++p;
        return c;
    }
    int extra;
    uint32_t cp;
    if ((c & 0xE0) == 0xC0) {
        extra = 1;
        cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        extra = 2;
        cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        extra = 3;
        cp = c & 0x07;
    } else {
        ++p;
        return 0xFFFD;
    }
    ++p;
    for (int i = 0; i < extra; ++i) {
        const unsigned char t = static_cast<unsigned char>(*p);
        if ((t & 0xC0) != 0x80) return 0xFFFD;  // truncated sequence; t is reprocessed
        cp = (cp << 6) | (t & 0x3F);
        ++p;
    }
    return cp;
}

size_t length(const char* utf8) {
    size_t n = 0;
    const char* p = utf8;
    while (*p) {
        nextCodepoint(p);
        ++n;
    }
    return n;
}

void fit(char* out, size_t outSize, const char* utf8, uint8_t maxChars) {
    if (outSize == 0) return;
    const size_t len = length(utf8);
    const bool truncated = len > maxChars;
    const size_t keep = truncated ? (maxChars > 0 ? maxChars - 1 : 0) : len;

    size_t o = 0;
    const char* p = utf8;
    for (size_t i = 0; i < keep; ++i) {
        const char* start = p;
        nextCodepoint(p);
        const size_t n = static_cast<size_t>(p - start);
        if (o + n + 2 > outSize) break;  // keep room for '.' and NUL
        memcpy(out + o, start, n);
        o += n;
    }
    if (truncated && maxChars > 0 && o + 2 <= outSize) out[o++] = '.';
    out[o] = '\0';
}

const uint8_t* glyphRows(uint32_t cp) {
    for (const Glyph& g : kGlyphs)
        if (g.cp == cp) return g.rows;
    return nullptr;
}

void draw(IDisplay& d, int16_t x, int16_t y, const char* utf8, uint8_t size, bool inverted) {
    if (size == 0) size = 1;
    const char* p = utf8;
    int16_t cx = x;
    for (;;) {
        const uint32_t cp = nextCodepoint(p);
        if (cp == 0) break;

        if (cp >= 0x20 && cp < 0x7F) {
            char buf[2] = {static_cast<char>(cp), '\0'};
            d.text(cx, y, buf, size, inverted);
        } else if (const uint8_t* rows = glyphRows(cp)) {
            for (uint8_t r = 0; r < kGlyphRows; ++r) {
                for (uint8_t c = 0; c < 5; ++c) {
                    if (rows[r] & (0x10 >> c)) {
                        d.fillRect(cx + c * size, y + r * size, size, size, !inverted);
                    }
                }
            }
        } else {
            d.text(cx, y, "?", size, inverted);
        }
        cx += kCellW * size;
    }
}

}  // namespace aztext
