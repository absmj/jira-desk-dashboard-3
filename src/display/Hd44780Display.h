#pragma once
#include "Hd44780.h"
#include "ITextDisplay.h"
#include "LcdText.h"

// ITextDisplay for an HD44780 character LCD. Hardware-independent: give it any IHd44780Port.
class Hd44780Display : public ITextDisplay {
public:
    Hd44780Display(IHd44780Port& port, uint8_t cols = 16, uint8_t rows = 2)
        : lcd_(port), cols_(cols), rows_(rows) {}

    bool begin() override;
    uint8_t cols() const override { return cols_; }
    uint8_t rows() const override { return rows_; }
    void show(const char* const* lines, uint8_t lineCount) override;
    void setBacklight(bool on) override { lcd_.setBacklight(on); }

    uint8_t lastSubstituted() const { return frame_.substituted; }

private:
    Hd44780 lcd_;
    uint8_t cols_, rows_;
    lcdtext::GlyphCache cache_;
    lcdtext::Frame frame_;
    uint8_t shown_[lcdtext::kMaxRows][lcdtext::kMaxCols] = {};
    bool haveShown_ = false;
};
