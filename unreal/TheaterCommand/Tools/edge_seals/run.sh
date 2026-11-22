#!/bin/bash
# The news seals' cloud test (release 2, campaign map item 5): builds Campaign/EdgeSeals (the game's own code, no
# engine) and checks it: the card's items in the card's order, the fold into "+N", each item's class, sign and eight
# words, and the seals laid along the screen's edge clear of the frame, of each other and of the screen's own edge,
# again and again over random frames and places; where the edge is full, the last items folded into "+N" in the card's
# ranking, none lost without trace; that laying them again allocates nothing; and the art director's ruling of 22
# November: each seal's colour and pulse (red the dated warning's alone, the news band's brass for war and peace and
# the news, no star pressed in), and the look's limits (the sign about 18 px with no stroke under 1.5 px, the hollow lit
# from the upper left, four small uneven runs, short tails, the pulse ring under twice the disc and dimmer than it,
# rising from nothing). And the seal set of the ruling of 22 November, 06:45Z: every pressable order sign in
# Content/CityOrders has its seal picture in Content/CityOrders/Seal (none for the star or the night raid's), each read
# from its PNG and measured here, its thinnest stroke and narrowest gap or counter at least 16 of 192 (15.5 allowed for
# the antialiasing) and as EdgeSeals' table has them; a sign pressed only where its finest part holds 1.5 px, so from
# 1920 by 1080 up every one, and the three lines below.
# The seal pictures and the table are drawn and written by Tools/edge_seals/seal_signs.py (python3, PIL and numpy).
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
"$OUT/edge_seals_test" "$ROOT/Content/CityOrders"
