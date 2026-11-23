#!/bin/bash
# The crews' bail-out's cloud test (release 2, -TheaterLandBailOut): builds Battle/LandBail and Battle/Fates (the
# game's own code, no engine) with the Mac's strict warnings and plays every bail-out the fates table names.
# Usage: Tools/land_bail/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_BAIL_OUT:-${TMPDIR:-/tmp}/land_bail}"
mkdir -p "$OUT"
CXX="${CXX:-clang++}"
command -v "$CXX" >/dev/null 2>&1 || CXX=g++
$CXX -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -I"$SRC" "$HERE/land_bail_test.cpp" "$SRC/Battle/LandBail.cpp" "$SRC/Battle/Fates.cpp" -o "$OUT/land_bail_test"
"$OUT/land_bail_test" "$ROOT/Content"
