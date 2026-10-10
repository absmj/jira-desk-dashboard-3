#include "Hd44780.h"

namespace {
constexpr uint8_t kClear = 0x01, kEntryMode = 0x04, kDisplayCtrl = 0x08, kFunctionSet = 0x20;
constexpr uint8_t kSetCgram = 0x40, kSetDdram = 0x80;
}  // namespace

void Hd44780::write8(uint8_t value, bool rs) {
    port_.writeNibble(value >> 4, rs);
    port_.writeNibble(value & 0x0F, rs);
    port_.delayUs(50);  // every instruction except clear/home takes <= 37 us
}

void Hd44780::command(uint8_t c) { write8(c, false); }
void Hd44780::data(uint8_t d) { write8(d, true); }

void Hd44780::init(bool twoLines) {
    port_.delayUs(50000);          // >40 ms after Vcc reaches 2.7 V
    port_.writeNibble(0x03, false);
    port_.delayUs(4500);
    port_.writeNibble(0x03, false);
    port_.delayUs(4500);
    port_.writeNibble(0x03, false);
    port_.delayUs(150);
    port_.writeNibble(0x02, false);  // switch to 4-bit
    port_.delayUs(150);

    command(kFunctionSet | 0x00 | (twoLines ? 0x08 : 0x00));  // 4-bit, N lines, 5x8 font
    command(kDisplayCtrl);                                     // display off
    clear();
    command(kEntryMode | 0x02);                                // increment, no shift
    command(kDisplayCtrl | 0x04);                              // display on, cursor off, blink off
}

void Hd44780::clear() {
    write8(kClear, false);
    port_.delayUs(2000);  // clear takes ~1.5 ms
}

void Hd44780::setCursor(uint8_t col, uint8_t row) {
    static const uint8_t kRowBase[4] = {0x00, 0x40, 0x14, 0x54};
    if (row > 3) row = 3;
    command(kSetDdram | static_cast<uint8_t>(kRowBase[row] + col));
}

void Hd44780::createChar(uint8_t slot, const uint8_t* rows8) {
    command(kSetCgram | static_cast<uint8_t>((slot & 7) << 3));
    for (uint8_t i = 0; i < 8; ++i) data(rows8[i] & 0x1F);
}
