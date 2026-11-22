#!/bin/bash
# The live men's cloud test (release 2): builds Battle/LandMen (the game's own code, no engine) and plays each test
# folder's battle twice, the crowd grown from its header and deltas (Crowd::Extend) against the crowd built from the
# battle read whole, frame by frame at 60 frames a second with the screen 2 battle seconds behind the latest delta.
# Usage: Tools/land_live/men.sh [folder ...]  (default: the two folders Tools/land_live/run.sh writes)
# LAND_LIVE_LAG=s puts the screen s battle seconds behind the latest delta instead of 2.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_LIVE_OUT:-${TMPDIR:-/tmp}/land_live}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I$SRC -I$ROOT/Tools/land_show"
$CXX $FLAGS "$HERE/land_live_men_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_live_men_test"
# The Mac builds with clang and its -Wunreachable-code-aggressive: build with it too where it is here.
if command -v clang++ >/dev/null 2>&1; then
  clang++ $FLAGS -Wunreachable-code-aggressive "$HERE/land_live_men_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_live_men_test_clang"
fi
if [ "$#" -eq 0 ]; then set -- "$OUT/normandy" "$OUT/normandy-film"; fi
STATUS=0
for F in "$@"; do
  "$OUT/land_live_men_test" "$F" "$ROOT/Content/Models/Soldiers" "$ROOT/Content/Data/LandBattles/armies.json" "${LAND_LIVE_LAG:-2}" || STATUS=1
done
exit $STATUS
