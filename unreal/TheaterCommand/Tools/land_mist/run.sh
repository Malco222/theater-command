#!/bin/bash
# The blood mist's cloud test (-TheaterLandHitMist; Effects R2): builds Battle/LandMen and Effects/TheaterFxCore (the
# game's own code, no engine) with the Mac's strict warnings, with g++ and with clang++, and runs each over the battles
# given (default: every recording in Content/Data/LandBattles). See land_mist_test.cpp. The two compilers' outputs must
# be the same, line for line (the Draw timings go to stderr, in <out>/<compiler>.err).
# LAND_MIST_BASE=<git revision>: also the word-absent proof, that revision's Tools/land_hits test (every part off, each
# part alone and all on, the variety over the battles) built against this tree's LandMen and against its own, with both
# compilers: every line the same.
# Usage: Tools/land_mist/run.sh [battle.json ...]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
FX="$ROOT/../../tools/battle-fx"
SOLDIERS="${LAND_MIST_SOLDIERS:-$ROOT/Content/Models/Soldiers}"
ARMIES="$ROOT/Content/Data/LandBattles/armies.json"
OUT="${LAND_MIST_OUT:-${TMPDIR:-/tmp}/land_mist}"
NICE="${LAND_MIST_NICE:-nice -n 10}"
mkdir -p "$OUT"
for B in land_mist_test_gcc land_mist_test_clang hits_base_g++ hits_now_g++ hits_base_clang++ hits_now_clang++; do rm -f "${OUT:?}/$B"; done
STRICT="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -ffp-contract=off -I$SRC -I$ROOT/Tools/land_show"
python3 "$FX/preview/flatten.py" "$FX/fx.json" > "$OUT/recipe.txt"
cmp -s "$FX/fx.json" "$ROOT/Content/Effects/battle-fx.json" || { echo "FAIL: tools/battle-fx/fx.json and Content/Effects/battle-fx.json differ"; exit 1; }
OLD_REV="${LAND_MIST_BASE:-4060f0923}"
git -C "$ROOT" show "$OLD_REV:tools/battle-fx/fx.json" > "$OUT/fx_old.json"
python3 "$FX/preview/flatten.py" "$OUT/fx_old.json" > "$OUT/recipe_old.txt"
$NICE g++ $STRICT -pthread "$HERE/land_mist_test.cpp" "$SRC/Battle/LandMen.cpp" "$SRC/Effects/TheaterFxCore.cpp" -o "$OUT/land_mist_test_gcc" &
B1=$!
$NICE clang++ $STRICT -Wunreachable-code-aggressive -pthread "$HERE/land_mist_test.cpp" "$SRC/Battle/LandMen.cpp" "$SRC/Effects/TheaterFxCore.cpp" -o "$OUT/land_mist_test_clang" &
B2=$!
wait $B1 || { echo "FAIL: g++ did not build the test"; exit 1; }
wait $B2 || { echo "FAIL: clang++ did not build the test"; exit 1; }
if [ "$#" -eq 0 ]; then
  set --
  for F in "$ROOT"/Content/Data/LandBattles/*.json; do
    case "$F" in */armies.json) ;; *) set -- "$@" "$F" ;; esac
  done
fi
STATUS=0
$NICE "$OUT/land_mist_test_gcc" "$SOLDIERS" "$ARMIES" "$OUT/recipe.txt" "$OUT/recipe_old.txt" "$@" > "$OUT/gcc.txt" 2> "$OUT/gcc.err" &
P1=$!
$NICE "$OUT/land_mist_test_clang" "$SOLDIERS" "$ARMIES" "$OUT/recipe.txt" "$OUT/recipe_old.txt" "$@" > "$OUT/clang.txt" 2> "$OUT/clang.err" &
P2=$!
wait $P1 || STATUS=1
wait $P2 || STATUS=1
cat "$OUT/gcc.txt"
cat "$OUT/gcc.err"
if cmp -s "$OUT/gcc.txt" "$OUT/clang.txt"; then echo "ok: g++ and clang++ print the same"; else echo "FAIL: g++ and clang++ differ"; diff "$OUT/gcc.txt" "$OUT/clang.txt" | head -20 || true; STATUS=1; fi
if [ -n "${LAND_MIST_BASE:-}" ]; then
  # The word absent: the base's own land_hits test, against the base's LandMen and against this tree's.
  # (the test kept apart from the base's headers: a quoted include looks first beside the file that includes it)
  BASE="$OUT/base/Battle"
  mkdir -p "$BASE" "$OUT/base_test"
  git -C "$ROOT" show "$LAND_MIST_BASE:unreal/TheaterCommand/Source/TheaterCommand/Battle/LandMen.cpp" > "$BASE/LandMen.cpp"
  git -C "$ROOT" show "$LAND_MIST_BASE:unreal/TheaterCommand/Source/TheaterCommand/Battle/LandMen.h" > "$BASE/LandMen.h"
  git -C "$ROOT" show "$LAND_MIST_BASE:unreal/TheaterCommand/Tools/land_hits/land_hits_test.cpp" > "$OUT/base_test/land_hits_test.cpp"
  BSTRICT="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -ffp-contract=off -I$ROOT/Tools/land_show"
  for CC in g++ clang++; do
    $NICE $CC -I"$OUT/base" $BSTRICT -pthread "$OUT/base_test/land_hits_test.cpp" "$BASE/LandMen.cpp" -o "$OUT/hits_base_$CC" &
    H1=$!
    $NICE $CC -I"$SRC" $BSTRICT -pthread "$OUT/base_test/land_hits_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/hits_now_$CC" &
    H2=$!
    wait $H1 || { echo "FAIL: $CC did not build the base's test against the base"; exit 1; }
    wait $H2 || { echo "FAIL: $CC did not build the base's test against this tree"; exit 1; }
    $NICE "$OUT/hits_base_$CC" "$SOLDIERS" "$ARMIES" "$@" > "$OUT/hits_base_$CC.txt" 2>&1 &
    $NICE "$OUT/hits_now_$CC" "$SOLDIERS" "$ARMIES" "$@" > "$OUT/hits_now_$CC.txt" 2>&1 &
    wait
    if cmp -s "$OUT/hits_base_$CC.txt" "$OUT/hits_now_$CC.txt"; then
      echo "ok: $LAND_MIST_BASE's land_hits test prints the same against this tree's LandMen ($CC, $(wc -l < "$OUT/hits_now_$CC.txt") lines: every part off, each alone, all on)"
    else
      echo "FAIL: $LAND_MIST_BASE's land_hits test differs against this tree's LandMen ($CC)"; diff "$OUT/hits_base_$CC.txt" "$OUT/hits_now_$CC.txt" | head -20 || true; STATUS=1
    fi
  done
fi
echo "built in $OUT"
exit $STATUS
