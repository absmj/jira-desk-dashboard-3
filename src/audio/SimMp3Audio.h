#pragma once
// DEV-ONLY simulation of "play track N from the DFPlayer's SD card".
//
// What it models (behaviour):
//   - /MP3/NNNN.mp3 must exist, otherwise play() fails (like a missing file on the SD card)
//   - playback lasts as long as the file would: bytes * 8 / bitrate
//   - a new play() restarts with the new track
//   - when playback ends, a "finished" event is queued (DFPlayer reports this over UART)
//   - volume is 0..30
// What it does NOT model: the DFPlayer UART protocol, real MP3 decoding.
// The sound itself comes from the `speaker` (the buzzer melody for the same track number).
//
// Pure C++ (no Arduino), so it is covered by the host tests.
#include <stdint.h>

#include "IAudio.h"

class IMp3Card {
public:
    virtual ~IMp3Card() = default;
    // True if /MP3/NNNN.mp3 exists; writes its size in bytes.
    virtual bool trackBytes(uint16_t track, uint32_t& bytes) = 0;
};

class SimMp3Audio : public IAudio {
public:
    static constexpr uint32_t kAssumedKbps = 64;     // the test files are 64 kbps CBR
    static constexpr uint32_t kMinDurationMs = 200;  // tiny files still play briefly
    static constexpr uint8_t kMaxVolume = 30;

    SimMp3Audio(IMp3Card& card, IAudio& speaker) : card_(card), speaker_(speaker) {}

    bool begin() override { return speaker_.begin(); }
    bool play(uint16_t track, uint32_t nowMs) override;
    void stop() override;
    bool isPlaying() const override { return playing_; }
    void tick(uint32_t nowMs) override;
    void setVolume(uint8_t v) override { volume_ = v > kMaxVolume ? kMaxVolume : v; }

    uint8_t volume() const { return volume_; }
    uint32_t durationMs() const { return durationMs_; }
    // DFPlayer-style "track finished" event. Returns true once per finished track.
    bool takeFinished(uint16_t& track);

    static uint32_t durationFromBytes(uint32_t bytes);

private:
    IMp3Card& card_;
    IAudio& speaker_;
    bool playing_ = false;
    uint16_t track_ = 0;
    uint32_t startMs_ = 0;
    uint32_t durationMs_ = 0;
    uint8_t volume_ = 20;
    bool finishedPending_ = false;
    uint16_t finishedTrack_ = 0;
};
