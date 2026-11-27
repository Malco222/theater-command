#!/bin/bash
# Churned ground between dug-in lines on the campaign map, its cloud test (release 2, the campaign map look builder;
# the art director's sheet art-director/fronts-alive/fronts-alive-look.md, piece 1, and her read of the first drawings,
# churn-prototype-read-1127.md): builds Campaign/FrontChurn (the game's own code, no engine) with the strictest warnings
# and checks it. It grows by the sheet's months and heals over a year, craters first, then breaking into islands; it
# is never over 4 units wide nor under about 1.4 where it shows, about 2.4 at its core when whole and 1.0 when it first
# shows; weaker, it breaks into patches two to five units long; its middle strays up to half a unit; two pairs join at
# the stronger with no notch, and an end breaks up and dies; the veil's edge is crisp, never straight for more than
# about 3 units, with tongues up to about 0.8; it is strongest where the armies face each other and half along the rest
# of their border; it fades out under about four pixels wide; the core is at most a quarter darker than its ground,
# browner, mottled, the fields showing through; the woods thin and the snow breaks in its core and keep at its edge;
# the mud and the desert take their own tones; the fine veil only darkens; the craters are 0.15 to 0.5 across, in
# clusters, none in the soft edge, their pit floors at most about 7% of the band, a sunny rim 7D6748, pits 4A3B2B and
# the shaded third of large ones 2B231B, pools only in large pits in the mud months; a few flecks of spoil.
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
