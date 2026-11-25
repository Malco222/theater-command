#!/bin/bash
# The cloud test of rain and snow seen falling in the ground battle (release 2, the look bible's G10): builds the battle's
# pick (Battle/LandWeather), the drops' core (Effects/WeatherFallCore) and the graphics chip's own text
# (Effects/WeatherFallChip.inl, as C++: weather_fall_chip.h), with no engine, and checks them against each other and
# against Content/Effects/land-weather.json. Usage: Tools/land_weather/run.sh
# CXX=clang++ builds it with clang (the Mac's compiler); both are run by the pre-check.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_WEATHER_OUT:-${TMPDIR:-/tmp}/land_weather}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${LAND_WEATHER_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -I"$SRC" -I"$HERE" \
  "$HERE/land_weather_test.cpp" "$SRC/Battle/LandWeather.cpp" "$SRC/Effects/WeatherFallCore.cpp" "$SRC/Battle/LandJson.cpp" \
  -o "$OUT/land_weather_test"
"$OUT/land_weather_test" "$ROOT/Content/Effects/land-weather.json"
