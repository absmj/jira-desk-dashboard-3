#pragma once
#include <ArduinoJson.h>
#include "SprintData.h"

// Parse olunmuş sənədi `out`-a köçürür. `out` yalnız uğurlu olanda dəyişir.
bool fillSprint(const JsonDocument& doc, SprintData& out, const char** err = nullptr);

// RAM-dakı JSON mətni üçün (testlər, sonra BLE)
bool parseSprintJson(const char* json, SprintData& out, const char** err = nullptr);