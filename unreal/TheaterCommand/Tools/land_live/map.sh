#!/bin/bash
# The map of the field's cloud test (release 2): builds Battle/LandFieldMap (the game's own code, no engine) and holds
# what the general's screen relies on: where the map stands, how a place falls on it and back, the field painted small,
# a unit's mark, the view's outline cut to the map (land_map_test.cpp says each). No battle is fought: seconds.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_LIVE_OUT:-${TMPDIR:-/tmp}/land_live}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I$SRC"
$CXX $FLAGS "$HERE/land_map_test.cpp" "$SRC/Battle/LandFieldMap.cpp" -o "$OUT/land_map_test"
# The Mac builds with clang and its -Wunreachable-code-aggressive: build with it too where it is here.
if command -v clang++ >/dev/null 2>&1; then
  clang++ $FLAGS -Wunreachable-code-aggressive "$HERE/land_map_test.cpp" "$SRC/Battle/LandFieldMap.cpp" -o "$OUT/land_map_test_clang"
  "$OUT/land_map_test_clang" > /dev/null || { echo "FAIL (the clang build)"; exit 1; }
fi
"$OUT/land_map_test"
