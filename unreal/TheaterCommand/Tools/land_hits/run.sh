#!/bin/bash
# Bullet hits' cloud test (Crowd::SetHits, -TheaterLandHits): builds Battle/LandMen (the game's own code, no engine)
# with the Mac's strict warnings, with g++ and with clang++, and runs each over the battles given (default: every
# recording in Content/Data/LandBattles), checking the jolt, the falls' variety, where a man was struck and the
# wounded who stay down, and that every part off poses and fells every man as before. See land_hits_test.cpp.
# The two compilers' outputs must be the same, line for line.
# Usage: Tools/land_hits/run.sh [battle.json ...]
# LAND_HITS_BASE=<git revision>: also builds the off half against that revision's LandMen (git show) and asks the
# same hashes of it (every part off is the base, to the bit). LAND_HITS_SOLDIERS: another soldiers folder.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
SOLDIERS="${LAND_HITS_SOLDIERS:-$ROOT/Content/Models/Soldiers}"
ARMIES="$ROOT/Content/Data/LandBattles/armies.json"
OUT="${LAND_HITS_OUT:-${TMPDIR:-/tmp}/land_hits}"
mkdir -p "$OUT"
STRICT="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -ffp-contract=off -I$SRC -I$ROOT/Tools/land_show"
g++ $STRICT -pthread "$HERE/land_hits_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_hits_test_gcc"
clang++ $STRICT -Wunreachable-code-aggressive -pthread "$HERE/land_hits_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_hits_test_clang"
if [ "$#" -eq 0 ]; then
  set --
  for F in "$ROOT"/Content/Data/LandBattles/*.json; do
    case "$F" in */armies.json) ;; *) set -- "$@" "$F" ;; esac
  done
fi
STATUS=0
"$OUT/land_hits_test_gcc" "$SOLDIERS" "$ARMIES" "$@" > "$OUT/gcc.txt" || STATUS=1
"$OUT/land_hits_test_clang" "$SOLDIERS" "$ARMIES" "$@" > "$OUT/clang.txt" || STATUS=1
cat "$OUT/gcc.txt"
if cmp -s "$OUT/gcc.txt" "$OUT/clang.txt"; then echo "ok: g++ and clang++ print the same"; else echo "FAIL: g++ and clang++ differ"; diff "$OUT/gcc.txt" "$OUT/clang.txt" | head -20; STATUS=1; fi
if [ -n "${LAND_HITS_BASE:-}" ]; then
  BASE="$OUT/base/Battle"
  mkdir -p "$BASE"
  git -C "$ROOT" show "$LAND_HITS_BASE:unreal/TheaterCommand/Source/TheaterCommand/Battle/LandMen.cpp" > "$BASE/LandMen.cpp"
  git -C "$ROOT" show "$LAND_HITS_BASE:unreal/TheaterCommand/Source/TheaterCommand/Battle/LandMen.h" > "$BASE/LandMen.h"
  for CC in g++ clang++; do
    $CC -I"$OUT/base" $STRICT -DLAND_HITS_BASE -pthread "$HERE/land_hits_test.cpp" "$BASE/LandMen.cpp" -o "$OUT/land_hits_base_$CC"
    "$OUT/land_hits_base_$CC" "$SOLDIERS" "$ARMIES" "$@" > "$OUT/base_$CC.txt" || STATUS=1
  done
  grep '^off ' "$OUT/gcc.txt" > "$OUT/off_now.txt" || true
  for CC in g++ clang++; do
    if cmp -s "$OUT/off_now.txt" "$OUT/base_$CC.txt"; then echo "ok: every part off is $LAND_HITS_BASE to the bit ($CC's base)"; else echo "FAIL: off differs from $LAND_HITS_BASE ($CC's base)"; diff "$OUT/off_now.txt" "$OUT/base_$CC.txt"; STATUS=1; fi
  done
fi
echo "built in $OUT"
exit $STATUS
