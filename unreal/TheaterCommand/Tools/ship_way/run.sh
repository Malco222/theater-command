#!/bin/bash
# A fleet under way on the campaign map, its cloud test (release 2): builds Campaign/ShipWay.h (the game's own code, no
# engine) with the strictest warnings and checks it. The course begins on her bows and ends as the table wants her; her
# heading never jumps from one point to the next, on a circle or turning where she lies; no circle is tighter than asked
# unless the land made it so; no point of it is ashore, over six hundred seas made by chance; a corner is rounded a
# berth to seaward; a course with no room is the way exactly as the map found it. Her times: she gathers way slowly and
# carries it in, her head never swings faster than her helm, and no voyage takes more than half as long again as was
# asked. Her wake lies along the water her sterns have crossed.
# Usage: Tools/ship_way/run.sh [scenes.json]   (writes the scenes it checked, for Tools/ship_way/picture.py)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SHIPWAY_OUT:-${TMPDIR:-/tmp}/ship_way}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${SHIPWAY_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/ship_way_test.cpp" -o "$OUT/ship_way_test"
"$OUT/ship_way_test" "$@"
