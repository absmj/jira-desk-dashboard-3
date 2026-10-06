#!/usr/bin/env bash
# The JSON the web app produces must be accepted by the firmware's own loader (C++, ArduinoJson).
#   ARDUINOJSON_SRC=<ArduinoJson>/src web/test/device_roundtrip.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
AJ="${ARDUINOJSON_SRC:-}"
if [ -z "$AJ" ]; then for d in .pio/libdeps/*/ArduinoJson/src; do [ -d "$d" ] && AJ="$d" && break; done; fi
[ -n "$AJ" ] || { echo "set ARDUINOJSON_SRC"; exit 1; }
OUT="${TMPDIR:-/tmp}/jira_native_build"; mkdir -p "$OUT"
node -e "
const C=require('./web/convert.js'), d=require('./web/sample.js');
const r=C.convert(d,{today:'2026-10-05'}); if(r.errors.length){console.error(r.errors);process.exit(1)}
require('fs').writeFileSync('$OUT/web_sprint.json', r.json);"
g++ -std=c++17 -Wall -Wextra -I"$AJ" src/app/SprintData.cpp src/app/SprintLoader.cpp \
    tools/native_tests/parse_sprint_file.cpp -o "$OUT/parse_sprint_file"
"$OUT/parse_sprint_file" "$OUT/web_sprint.json" | tee "$OUT/roundtrip.txt"
grep -q "done=2/10 tasks=8 blocked=3 overdue=1 top=Əli Məmmədov(3)" "$OUT/roundtrip.txt" && echo "roundtrip OK"
