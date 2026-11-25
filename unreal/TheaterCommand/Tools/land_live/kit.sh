#!/bin/bash
# The live ground battle's vehicles' cloud test (release 2): builds Battle/LandKit (the game's own code, no engine) and
# grows each test battle piece by piece (LandKit::Battle::Extend, from header.json and deltas.jsonl) beside the same
# battle read whole (whole.json), comparing them frame by frame (land_live_kit_test.cpp). Builds with clang too when it
# is here, with the Mac's strictest warning (a loop that can run only once fails the Mac build).
# Usage: Tools/land_live/kit.sh [folder ...]   (default: the two that run.sh writes, normandy and normandy-film)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_LIVE_OUT:-${TMPDIR:-/tmp}/land_live}"
mkdir -p "$OUT"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I$SRC"
CORES="$SRC/Battle/LandKit.cpp $SRC/Battle/Fates.cpp"
if command -v clang++ >/dev/null 2>&1; then
  clang++ $FLAGS -Wunreachable-code-aggressive "$HERE/land_live_kit_test.cpp" $CORES -o "$OUT/land_live_kit_test_clang"
fi
CXX="${CXX:-g++}"
$CXX $FLAGS "$HERE/land_live_kit_test.cpp" $CORES -o "$OUT/land_live_kit_test"
STATUS=0
if [ "$#" -gt 0 ]; then FOLDERS=("$@"); else FOLDERS=("$OUT/normandy" "$OUT/normandy-film"); fi
for F in "${FOLDERS[@]}"; do
  if [ ! -f "$F/deltas.jsonl" ]; then echo "no $F/deltas.jsonl: run Tools/land_live/run.sh first"; STATUS=1; continue; fi
  "$OUT/land_live_kit_test" "$ROOT/Content" "$F" || STATUS=1
done
exit $STATUS
