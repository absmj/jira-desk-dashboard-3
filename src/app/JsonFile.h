#pragma once
#include <ArduinoJson.h>
#include <FS.h>

// Reads a JSON file from any Arduino filesystem (LittleFS now, SD possible later)
// into `doc`. Rejects missing, oversized or malformed files with a static message.
bool readJsonFile(fs::FS& fs, const char* path, JsonDocument& doc, const char** err = nullptr);
