#!/usr/bin/env bash
# Renders the CYD screens on this computer (real CYDRenderer + Adafruit GFX,
# no board needed) for several participant settings, as PNG files.
#
#   tools/render_cyd.sh [out-dir]        default: .pio/cyd-render
#
# Each variant is the current src/student/display_config.h with a few values
# replaced; the real file is never modified.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="${1:-.pio/cyd-render}"
mkdir -p "$OUT"

pio pkg install -e cyd >/dev/null
pio pkg install -e native >/dev/null
GFX=".pio/libdeps/cyd/Adafruit GFX Library"
JSON=".pio/libdeps/native/ArduinoJson/src"

render() {  # render <name> [sed expressions...]
  local name="$1"; shift
  local tmp; tmp=$(mktemp -d)
  mkdir -p "$tmp/student" "$OUT/$name"
  cp src/student/display_config.h "$tmp/student/display_config.h"
  for expr in "$@"; do sed -i '' -E "$expr" "$tmp/student/display_config.h"; done
  clang++ -std=c++17 -O1 -w -DARDUINO=10800 -DCYD_HOST_RENDER=1 -DPROGMEM= \
    -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 \
    -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 \
    -I"$tmp" -Isrc -Itools/cyd_render/shim -Itest/native_shim -I"$GFX" -I"$JSON" \
    tools/cyd_render/main.cpp src/display/CYDRenderer.cpp src/app/DisplayModelBuilder.cpp \
    src/api/DepartureParser.cpp src/student/student_code.cpp src/util/TextUtil.cpp src/util/TimeUtil.cpp \
    "$GFX/Adafruit_GFX.cpp" -o "$tmp/cyd_render"
  "$tmp/cyd_render" "$OUT/$name" test/fixtures/departures_koenigsplatz_20.json test/fixtures/departures_edge_cases.json
  for f in "$OUT/$name"/*.ppm; do sips -s format png "$f" --out "${f%.ppm}.png" >/dev/null && rm "$f"; done
  rm -rf "$tmp"
  echo "$OUT/$name"
}

render default
render led-rows3 's/DEPARTURE_COUNT = [0-9]+/DEPARTURE_COUNT = 3/'
render led-rows4 's/DEPARTURE_COUNT = [0-9]+/DEPARTURE_COUNT = 4/'
render led-rows6-platform 's/DEPARTURE_COUNT = [0-9]+/DEPARTURE_COUNT = 6/' 's/SHOW_PLATFORM( *)= false/SHOW_PLATFORM\1= true/'
render led-green-black 's/BACKGROUND_COLOR = COLOR_[A-Z]+/BACKGROUND_COLOR = COLOR_BLACK/' \
  's/TEXT_COLOR( *)= COLOR_[A-Z]+/TEXT_COLOR\1= COLOR_GREEN/' 's/LINE_COLOR( *)= COLOR_[A-Z]+/LINE_COLOR\1= COLOR_YELLOW/' \
  's/NOW_TEXT = "[^"]*"/NOW_TEXT = "jetzt!"/'
render modern-rows5 's/FONT = FONT_[A-Z]+/FONT = FONT_MODERN/' 's/BACKGROUND_COLOR = COLOR_[A-Z]+/BACKGROUND_COLOR = COLOR_BLACK/' \
  's/TEXT_COLOR( *)= COLOR_[A-Z]+/TEXT_COLOR\1= COLOR_WHITE/' 's/LINE_COLOR( *)= COLOR_[A-Z]+/LINE_COLOR\1= COLOR_YELLOW/' \
  's/TIME_COLOR( *)= COLOR_[A-Z]+/TIME_COLOR\1= COLOR_CYAN/' 's/SHOW_PLATFORM( *)= false/SHOW_PLATFORM\1= true/'
