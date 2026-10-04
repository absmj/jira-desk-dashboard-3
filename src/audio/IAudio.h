#pragma once
// Audio output abstraction.
//   dev (Wokwi):  BuzzerAudio    - a buzzer plays a short melody per track number
//   prod:         DfPlayerAudio  - Phase 6, plays NNNN.mp3 from the DFPlayer's own SD card
// The alert code only knows this interface.
#include <stdint.h>

class IAudio {
public:
    virtual ~IAudio() = default;

    virtual bool begin() = 0;
    // Track number as in the file name: 0002.mp3 -> 2. Returns false if it cannot be played.
    virtual bool play(uint16_t track) = 0;
    virtual void stop() = 0;
    virtual bool isPlaying() const = 0;
    // Non-blocking: call from loop() as often as possible.
    virtual void tick(uint32_t nowMs) = 0;
};
