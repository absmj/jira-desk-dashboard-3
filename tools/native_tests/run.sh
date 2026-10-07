#!/usr/bin/env bash
# Builds and runs the host-side tests with plain g++ (no PlatformIO build, no hardware).
#   tools/native_tests/run.sh [--glyphs]
#
# test_main  : pager, dates, text, screens        (no dependencies)
# test_json  : JSON loaders, alerts, clock        (needs ArduinoJson 7 headers)
#
# ArduinoJson is looked up in $ARDUINOJSON_SRC, then in PlatformIO's library folder
# (.pio/libdeps/<env>/ArduinoJson/src, present after the first `pio run`).
set -euo pipefail
cd "$(dirname "$0")/../.."

OUT="${TMPDIR:-/tmp}/jira_native_build"
mkdir -p "$OUT"
WARN="-std=c++17 -Wall -Wextra -Wno-unused-parameter -g"

g++ $WARN \
    src/app/SprintData.cpp src/app/Pager.cpp src/app/Screens.cpp src/app/SampleData.cpp \
    src/display/AzText.cpp src/audio/SimMp3Audio.cpp tools/native_tests/test_main.cpp -o "$OUT/test_main"
"$OUT/test_main" "$@"

AJ="${ARDUINOJSON_SRC:-}"
if [ -z "$AJ" ]; then
    for d in .pio/libdeps/*/ArduinoJson/src; do
        [ -d "$d" ] && AJ="$d" && break
    done
fi
if [ -z "$AJ" ]; then
    echo
    echo "ArduinoJson not found - skipping JSON tests."
    echo "Run 'pio run' once, or: git clone --depth 1 --branch v7.4.2 https://github.com/bblanchon/ArduinoJson.git"
    echo "and set ARDUINOJSON_SRC=<that folder>/src"
    exit 0
fi

echo
g++ $WARN -I"$AJ" \
    src/app/SprintData.cpp src/app/SimClock.cpp src/app/SprintLoader.cpp src/app/AlertLoader.cpp \
    src/app/AlertEngine.cpp src/app/SampleData.cpp tools/native_tests/test_json.cpp -o "$OUT/test_json"
"$OUT/test_json"

echo
echo "BLE receiver logic (frames come from the real web/protocol.js)"
g++ $WARN -I"$AJ" \
    src/app/SprintData.cpp src/app/SprintLoader.cpp src/app/AlertLoader.cpp \
    src/ble/TransferSession.cpp src/ble/UploadApplier.cpp tools/native_tests/test_ble.cpp -o "$OUT/test_ble"
if command -v node >/dev/null 2>&1; then
    node tools/native_tests/gen_frames.js > "$OUT/frames.txt"
    "$OUT/test_ble" "$OUT/frames.txt"
else
    "$OUT/test_ble"
fi
