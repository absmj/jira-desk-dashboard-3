#include "SimMp3Audio.h"

uint32_t SimMp3Audio::durationFromBytes(uint32_t bytes) {
    // bytes * 8 bits / (kbps * 1000 bit/s) * 1000 ms = bytes * 8 / kbps  (64-bit: no overflow)
    const uint64_t ms = static_cast<uint64_t>(bytes) * 8u / kAssumedKbps;
    if (ms < kMinDurationMs) return kMinDurationMs;
    return ms > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(ms);
}

bool SimMp3Audio::play(uint16_t track, uint32_t nowMs) {
    uint32_t bytes = 0;
    if (!card_.trackBytes(track, bytes) || bytes == 0) return false;  // missing/empty file

    speaker_.stop();  // restart semantics: new track replaces the old one
    playing_ = true;
    track_ = track;
    startMs_ = nowMs;
    durationMs_ = durationFromBytes(bytes);
    speaker_.play(track, nowMs);
    return true;
}

void SimMp3Audio::stop() {
    speaker_.stop();
    playing_ = false;
}

void SimMp3Audio::tick(uint32_t nowMs) {
    speaker_.tick(nowMs);
    if (!playing_) return;
    if (static_cast<uint32_t>(nowMs - startMs_) < durationMs_) return;  // wrap-safe
    speaker_.stop();
    playing_ = false;
    finishedPending_ = true;
    finishedTrack_ = track_;
}

bool SimMp3Audio::takeFinished(uint16_t& track) {
    if (!finishedPending_) return false;
    finishedPending_ = false;
    track = finishedTrack_;
    return true;
}
