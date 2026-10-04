#!/usr/bin/env bash
# Builds and runs the host-side tests with plain g++ (no PlatformIO, no hardware).
# Usage: tools/native_tests/run.sh [--glyphs]
set -euo pipefail
cd "$(dirname "$0")/../.."

OUT="${TMPDIR:-/tmp}/jira_native_tests"
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -g \
    src/app/SprintData.cpp src/app/Pager.cpp src/app/Screens.cpp src/app/SampleData.cpp \
    src/display/AzText.cpp tools/native_tests/test_main.cpp -o "$OUT"
"$OUT" "$@"
