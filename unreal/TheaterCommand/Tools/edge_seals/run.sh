#!/bin/bash
# The news seals' cloud test (release 2, campaign map item 5): builds Campaign/EdgeSeals (the game's own code, no
# engine) and checks it: the card's items in the card's order, the fold into "+N", each item's class, sign and eight
# words, and the seals laid along the screen's edge clear of the frame, of each other and of the screen's own edge,
# again and again over random frames and places; where the edge is full, the last items folded into "+N" in the card's
# ranking, none lost without trace; and that laying them again allocates nothing.
# Usage: Tools/edge_seals/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${EDGE_SEALS_OUT:-${TMPDIR:-/tmp}/edge_seals}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${EDGE_SEALS_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/edge_seals_test.cpp" "$SRC/Campaign/EdgeSeals.cpp" -o "$OUT/edge_seals_test"
"$OUT/edge_seals_test"
