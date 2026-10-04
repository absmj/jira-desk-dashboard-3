#pragma once
// DEV-ONLY: pretends the internal LittleFS flash is the DFPlayer's SD card.
// Looks for /MP3/NNNN.mp3 (same layout as the real card).
#include <Arduino.h>
#include <LittleFS.h>

#include "SimMp3Audio.h"

class LittleFsCard : public IMp3Card {
public:
    bool trackBytes(uint16_t track, uint32_t& bytes) override {
        char path[24];
        snprintf(path, sizeof(path), "/MP3/%04u.mp3", static_cast<unsigned>(track));
        if (!LittleFS.exists(path)) return false;
        File f = LittleFS.open(path, "r");
        if (!f) return false;
        bytes = static_cast<uint32_t>(f.size());
        f.close();
        return true;
    }
};
