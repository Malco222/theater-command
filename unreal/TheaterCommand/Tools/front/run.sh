#!/bin/bash
# The drawn front's cloud test: builds Campaign/FrontLine (the game's own code, no engine) with the strictest warnings
# and checks it on small made-up maps: a straight front, a staircase, three and four countries meeting at a point, a
# front split by a neutral, islands, an enclave, a gap the table leaves, thinning, the ink's clock, the nib's waver,
# and that teeth far from a change stay where they stood.
# Usage: Tools/front/run.sh [pieces.txt [picture.ppm]] ...   (the pieces of the game's own map: pieces_from_map.py)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${FRONT_OUT:-${TMPDIR:-/tmp}/front}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${FRONT_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/front_test.cpp" "$SRC/Campaign/FrontLine.cpp" -o "$OUT/front_test"
"$OUT/front_test" "$@"
