#!/bin/bash
# The season turns on the campaign map, its cloud test (release 2, the campaign map look builder): builds
# Campaign/SeasonTurn (the game's own code, no engine) with the strictest warnings and checks it. The turn takes about
# three and a half seconds, starts with nothing changed and ends with everything changed and no wet band; its front is
# soft (no tenth of a second changes much of the land); the mud stops inside the rules' ground and thins to patches
# near its edge; it is wettest by the water and on the low ground, soaks outward from the water and the low ground
# first, hardly touches the woods, shows no cell edge, has few pools and no white specks, is about the art director's
# wet earth (78, 62, 46), never black and never one smooth brown; the quick mix the map paints each frame with stays
# near the paint itself; the arrivals spread evenly over the turn in their own order, and smoothing takes the step out
# of a province's edge; snow comes from the north and the high ground first and goes from the south and the low
# ground first.
# Usage: Tools/season_turn/run.sh                (the checks)
#        Tools/season_turn/run.sh --map BOARD OUT (drawings of the turn over the game's own east: draw_east.py lays
#                                                 the board from the game's own files and calls this)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SEASONTURN_OUT:-${TMPDIR:-/tmp}/season_turn}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${SEASONTURN_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/season_turn_test.cpp" "$SRC/Campaign/SeasonTurn.cpp" -o "$OUT/season_turn_test"
"$OUT/season_turn_test" "$@"
