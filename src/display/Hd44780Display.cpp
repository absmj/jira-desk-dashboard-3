#include "Hd44780Display.h"

#include <string.h>

bool Hd44780Display::begin() {
    frame_.cols = cols_;
    frame_.rows = rows_;
    lcd_.init(rows_ > 1);
    haveShown_ = false;
    cache_ = lcdtext::GlyphCache{};
    return true;
}

void Hd44780Display::show(const char* const* lines, uint8_t lineCount) {
    lcdtext::encode(lines, lineCount, cache_, frame_);

    bool anyDirty = false;
    for (uint8_t s = 0; s < lcdtext::kSlots; ++s) {
        if (!cache_.dirty[s]) continue;
        lcd_.createChar(s, cache_.bitmap[s]);
        cache_.dirty[s] = false;
        anyDirty = true;
    }
    // A reused CGRAM slot changes what already-written cells look like even when their codes are
    // unchanged, so after any upload every row is rewritten.
    for (uint8_t r = 0; r < rows_; ++r) {
        if (haveShown_ && !anyDirty && memcmp(shown_[r], frame_.code[r], cols_) == 0) continue;
        lcd_.setCursor(0, r);
        for (uint8_t c = 0; c < cols_; ++c) lcd_.data(frame_.code[r][c]);
        memcpy(shown_[r], frame_.code[r], cols_);
    }
    haveShown_ = true;
}
