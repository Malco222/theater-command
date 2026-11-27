#!/bin/bash
# Churned ground between dug-in lines on the campaign map, its cloud test (release 2, the campaign map look builder;
# the art director's sheet art-director/fronts-alive/fronts-alive-look.md, piece 1): builds Campaign/FrontChurn (the
# game's own code, no engine) with the strictest warnings and checks it. It grows by the sheet's months and heals over
# a year, craters first; it is never over 4 units wide, about 2.4 at its core when whole and 1.0 when it first shows,
# its width wanders and its edge never runs straight for more than about 3 units, with no steps; it is strongest where
# the armies face each other and nothing far from them; the core is at most a quarter darker than its ground, browner,
# mottled, the fields still showing through; the woods thin and the snow breaks in its core and keep at its edge; the
# mud and the desert take their own tones; the fine veil only darkens, a little; the craters are 0.15 to 0.5 across,
# in clusters, none in the soft edge, about a third of the core where it is whole, near the pit colour and never black.
# Usage: Tools/front_churn/run.sh                (the checks)
#        Tools/front_churn/run.sh --map BOARD OUT (drawings over the game's own ground: draw_churn.py lays the board
#                                                 from the game's own files and calls this)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${FRONTCHURN_OUT:-${TMPDIR:-/tmp}/front_churn}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${FRONTCHURN_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/front_churn_test.cpp" "$SRC/Campaign/FrontChurn.cpp" "$SRC/Campaign/SeasonTurn.cpp" "$SRC/Campaign/FrontLine.cpp" -o "$OUT/front_churn_test"
"$OUT/front_churn_test" "$@"
