#!/bin/bash
# The air's dramatic moments' cloud test (effects-r2; the owner's items 2, 3 and 4 of 26 November): the effects kit's
# own code (Effects/TheaterFxCore), no engine, built with g++ and clang++ under the strict flags and -ffp-contract=off
# (the Mac's Unreal toolchain passes it), run with the game's recipe and the overlays the air scene's words lay on it,
# and the two compilers held to each other (the time lines aside).
#
#     Tools/fx_air_ten/run.sh [out folder]          (default ${TMPDIR:-/tmp}/fx_air_ten)
#
# What it checks: air_ten_test.cpp's head. FX_AIR_TEN_ONE=g++ builds with one compiler only.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
RECIPE="$ROOT/Content/Effects/battle-fx.json"
OUT="${1:-${TMPDIR:-/tmp}/fx_air_ten}"
mkdir -p "$OUT"
FLAGS="-std=c++20 -O2 -ffp-contract=off -Wall -Wextra -Wshadow -Wconversion -Werror"
STATUS=0
BUILT=()
for CC in ${FX_AIR_TEN_ONE:-g++ clang++}; do
  if ! command -v "$CC" > /dev/null 2>&1; then
    echo "no $CC here: skipped"
    continue
  fi
  TAG=gcc
  EXTRA=""
  if [ "$CC" = "clang++" ]; then TAG=clang; EXTRA="-Wunreachable-code-aggressive -Wno-sign-conversion"; fi
  # (clang's -Wconversion takes in -Wsign-conversion, which the kit has never been held to; g++'s does not)
  nice -n 10 $CC $FLAGS $EXTRA -I"$SRC" -c "$SRC/Effects/TheaterFxCore.cpp" -o "$OUT/core_$TAG.o"
  nice -n 10 $CC $FLAGS $EXTRA -I"$SRC" -c "$SRC/Battle/LandJson.cpp" -o "$OUT/json_$TAG.o"
  nice -n 10 $CC $FLAGS $EXTRA -Wno-keyword-macro -I"$SRC" "$HERE/air_ten_test.cpp" "$OUT/core_$TAG.o" "$OUT/json_$TAG.o" -o "$OUT/air_ten_test_$TAG" 2>&1 | grep -v "keyword-macro" || true
  mkdir -p "$OUT/$TAG"
  echo "== air_ten_test, $CC"
  nice -n 10 "$OUT/air_ten_test_$TAG" "$RECIPE" "$ROOT/Content/Effects" "$OUT/$TAG" | tee "$OUT/$TAG/test.txt" || STATUS=1
  BUILT+=("$TAG")
done
if [ "${#BUILT[@]}" -eq 2 ]; then
  Strip() { grep -v -e "time:" -e " ms a frame" -e "us a call" "$1"; }
  if diff <(Strip "$OUT/gcc/test.txt") <(Strip "$OUT/clang/test.txt") > "$OUT/compilers.diff"; then
    echo "  ok   g++ and clang++ print the same checks and numbers"
  else
    echo "  FAIL g++ and clang++ differ (see $OUT/compilers.diff)"
    STATUS=1
  fi
fi
if [ "$STATUS" -eq 0 ]; then echo "fx_air_ten: all passed, in $OUT"; else echo "fx_air_ten: FAILED, in $OUT"; fi
exit $STATUS
