#pragma once
// HD44780 command layer (4-bit mode) on top of a "port" that knows how to move one nibble.
// Pure C++: the port is Pcf8574Port on the device and a fake panel in the PC tests.
#include <stdint.h>

class IHd44780Port {
public:
    virtual ~IHd44780Port() = default;
    // Sends the low 4 bits of `nibble` to D4..D7 and pulses Enable. rs = false: command, true: data.
    virtual void writeNibble(uint8_t nibble, bool rs) = 0;
    virtual void delayUs(uint32_t us) = 0;
    virtual void setBacklight(bool on) = 0;
};

class Hd44780 {
public:
    explicit Hd44780(IHd44780Port& port) : port_(port) {}

    // Power-on initialisation for a 2+ line 5x8 display in 4-bit mode (datasheet "initializing by instruction").
    void init(bool twoLines = true);

    void command(uint8_t c);
    void data(uint8_t d);
    void clear();
    void setCursor(uint8_t col, uint8_t row);                 // rows 0..3 (20x4 layout)
    void createChar(uint8_t slot, const uint8_t* rows8);      // CGRAM slot 0..7, 8 rows of 5 bits
    void setBacklight(bool on) { port_.setBacklight(on); }

private:
    void write8(uint8_t value, bool rs);
    IHd44780Port& port_;
};
