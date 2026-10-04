#include "BuzzerAudio.h"

namespace {

constexpr uint8_t kLedcChannel = 0;  // only used by Arduino-ESP32 core 2.x

using Note = BuzzerAudio::Note;

const Note kTrack1[] = {{880, 150}};
const Note kTrack2[] = {{1200, 120}, {0, 80}, {1200, 120}, {0, 80}, {1200, 120}};  // urgent
const Note kTrack3[] = {{523, 120}, {659, 120}, {784, 120}, {1046, 200}};          // milestone
const Note kTrack4[] = {{300, 250}, {0, 100}, {300, 250}};                         // warning
const Note kTrack5[] = {{784, 200}, {988, 200}, {1175, 300}};                      // chime
const Note kUnknown[] = {{880, 200}};

}  // namespace

bool BuzzerAudio::begin() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    return ledcAttach(pin_, 2000, 8);
#else
    ledcSetup(kLedcChannel, 2000, 8);
    ledcAttachPin(pin_, kLedcChannel);
    return true;
#endif
}

void BuzzerAudio::tone(uint16_t hz) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWriteTone(pin_, hz);
#else
    ledcWriteTone(kLedcChannel, hz);
#endif
}

bool BuzzerAudio::play(uint16_t track) {
    switch (track) {
        case 1: melody_ = kTrack1; length_ = sizeof(kTrack1) / sizeof(Note); break;
        case 2: melody_ = kTrack2; length_ = sizeof(kTrack2) / sizeof(Note); break;
        case 3: melody_ = kTrack3; length_ = sizeof(kTrack3) / sizeof(Note); break;
        case 4: melody_ = kTrack4; length_ = sizeof(kTrack4) / sizeof(Note); break;
        case 5: melody_ = kTrack5; length_ = sizeof(kTrack5) / sizeof(Note); break;
        default: melody_ = kUnknown; length_ = 1; break;  // any other number: one beep
    }
    index_ = 0;
    tone(melody_[0].hz);
    noteEndMs_ = millis() + melody_[0].ms;
    return true;
}

void BuzzerAudio::stop() {
    tone(0);
    melody_ = nullptr;
}

void BuzzerAudio::tick(uint32_t nowMs) {
    if (!melody_) return;
    if (static_cast<int32_t>(nowMs - noteEndMs_) < 0) return;  // wrap-safe "not yet"
    ++index_;
    if (index_ >= length_) {
        stop();
        return;
    }
    tone(melody_[index_].hz);
    noteEndMs_ = nowMs + melody_[index_].ms;
}
