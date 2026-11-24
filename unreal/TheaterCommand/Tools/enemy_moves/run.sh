#!/bin/bash
# The enemy's month near you, its cloud test (release 2): builds Campaign/EnemyMoves (the game's own code, no engine)
# with the strictest warnings and checks it: the arrows grow from nothing to whole, an attack's mark lands as its arrow
# arrives, a landing (no start) is a mark alone, the edges lie under the inks, the framing holds every point, the zoom
# stays in the watch's range, and the pauses are long enough to read.
# Usage: Tools/enemy_moves/run.sh [arrows.txt drawing.json]   (lays the arrows in a file, for Tools/enemy_moves/picture.py)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${MOVES_OUT:-${TMPDIR:-/tmp}/enemy_moves}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${MOVES_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/enemy_moves_test.cpp" "$SRC/Campaign/EnemyMoves.cpp" -o "$OUT/enemy_moves_test"
"$OUT/enemy_moves_test" "$@"
