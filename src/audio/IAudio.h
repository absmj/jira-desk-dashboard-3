#pragma once
// Audio output abstraction.
//   dev (Wokwi):  SimMp3Audio over BuzzerAudio - models DFPlayer behaviour, not its UART protocol
//   prod:         DfPlayerAudio (Phase 6)      - plays /MP3/NNNN.mp3 from the DFPlayer's own SD card
// The alert code only knows this interface.
#include <stdint.h>

class IAudio {
public:
    virtual ~IAudio() = default;

    virtual bool begin() = 0;
    // Track number as in the file name: 0002.mp3 -> 2. Returns false if it cannot be played
    // (for example the file does not exist). Calling play() while something is playing
    // switches to the new track.
    virtual bool play(uint16_t track, uint32_t nowMs) = 0;
    virtual void stop() = 0;
    virtual bool isPlaying() const = 0;
    // Non-blocking: call from loop() as often as possible.
    virtual void tick(uint32_t nowMs) = 0;
    // 0..30, the DFPlayer range. Implementations without volume control ignore it.
    virtual void setVolume(uint8_t /*volume*/) {}
};
