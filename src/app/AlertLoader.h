#pragma once
#include <ArduinoJson.h>

#include "AlertRules.h"

// Parsed alerts.json -> AlertConfig. `out` is only modified on success.
// Structural problems (version, rules not an array) reject the whole file;
// a single invalid rule is skipped and counted in out.skipped.
bool fillAlerts(const JsonDocument& doc, AlertConfig& out, const char** err = nullptr);

// For tests and for JSON arriving in RAM.
bool parseAlertsJson(const char* json, AlertConfig& out, const char** err = nullptr);
