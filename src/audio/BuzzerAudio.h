#pragma once
#include <Arduino.h>

#include "IAudio.h"

// DEV-ONLY stand-in for the DFPlayer: plays a short melody on a passive buzzer.
// It does NOT play MP3 files and is not equivalent to the real audio path.
class BuzzerAudio : public IAudio {
public:
    explicit BuzzerAudio(int8_t pin) : pin_(pin) {}

    bool begin() override;
    bool play(uint16_t track, uint32_t nowMs) override;
    void stop() override;
    bool isPlaying() const override { return melody_ != nullptr; }
    void tick(uint32_t nowMs) override;

    struct Note {
        uint16_t hz;  // 0 = rest
        uint16_t ms;
    };

private:
    void tone(uint16_t hz);

    int8_t pin_;
    const Note* melody_ = nullptr;
    uint8_t length_ = 0;
    uint8_t index_ = 0;
    uint32_t noteEndMs_ = 0;
};
