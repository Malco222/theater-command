#!/bin/bash
# The ground-attack fighters' cloud test (Effects, release 2, -TheaterLandAirSupport): builds Battle/LandAir and
# Battle/LandFeed (the game's own code, no engine) with the Mac's strict warnings and plays three strikes of its own
# through them. See land_air_test.cpp for what it checks. With an out folder it also draws air_paths.png there.
# Usage: Tools/land_air/run.sh [out folder]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_AIR_OUT:-${TMPDIR:-/tmp}/land_air}"
mkdir -p "$OUT"
CXX="${CXX:-clang++}"
command -v "$CXX" >/dev/null 2>&1 || CXX=g++
$CXX -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -I"$SRC" "$HERE/land_air_test.cpp" "$SRC/Battle/LandAir.cpp" "$SRC/Battle/LandFeed.cpp" "$SRC/Battle/LandJson.cpp" -o "$OUT/land_air_test"
"$OUT/land_air_test" ${1:+"$1"}
if [ -n "${1:-}" ]; then python3 "$HERE/paths.py" "$1/air_paths.txt" "$1/air_paths.png"; fi
