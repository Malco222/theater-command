#!/bin/bash
# The effects kit's first batch of the owner's film details (26 November, "do 2-11"), its cloud test (the kit's
# Effects/TheaterFxCore, no engine), built with g++ and clang++ under the strict flags and -ffp-contract=off (the Mac's
# Unreal toolchain passes it), run, and held to each other:
#
#     Tools/fx_ten/run.sh [out folder]          (default ${TMPDIR:-/tmp}/fx_ten)
#
#   1. fx_ten_dump: every piece of five scripted fights (land guns, a broadside close on the water, a flak barrage of
#      every kind, turrets far off, guns on sand), with the shipped recipe alone, held to golden-default.sha256 (the
#      kit before this batch printed it): with no word given, not one piece moves. FX_TEN_BEFORE=<an older
#      Source/TheaterCommand> builds the dump against that kit too and diffs the two whole.
#   2. fx_ten_test: each part with its word on (its overlay laid over the recipe) and off: it fires when it should and
#      only then, its counts and lives bounded, its dice its own (every piece the kit lays without it laid the same
#      with it), its cap holding in the densest case; the time lines (the densest frame's cost) are kept out of the
#      compilers' comparison.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
FX="$ROOT/Content/Effects"
RECIPE="$FX/battle-fx.json"
OUT="${1:-${TMPDIR:-/tmp}/fx_ten}"
mkdir -p "$OUT"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -ffp-contract=off"
NICE="${FX_TEN_NICE:-nice -n 10}"
STATUS=0
BUILT=()
for CC in g++ clang++; do
  if ! command -v "$CC" > /dev/null 2>&1; then
    echo "no $CC here: skipped"
    continue
  fi
  EXTRA=""
  TAG=gcc
  if [ "$CC" = "clang++" ]; then EXTRA="-Wunreachable-code-aggressive"; TAG=clang; fi
  $NICE $CC $FLAGS $EXTRA -I"$SRC" -c "$SRC/Effects/TheaterFxCore.cpp" -o "$OUT/core_$TAG.o"
  $NICE $CC $FLAGS $EXTRA -I"$SRC" -c "$SRC/Battle/LandJson.cpp" -o "$OUT/json_$TAG.o"
  $NICE $CC $FLAGS $EXTRA -I"$SRC" "$HERE/fx_ten_dump.cpp" "$OUT/core_$TAG.o" "$OUT/json_$TAG.o" -o "$OUT/fx_ten_dump_$TAG"
  $NICE $CC $FLAGS $EXTRA -I"$SRC" "$HERE/fx_ten_test.cpp" "$OUT/core_$TAG.o" "$OUT/json_$TAG.o" -o "$OUT/fx_ten_test_$TAG"
  mkdir -p "$OUT/$TAG"
  echo "== fx_ten_dump, $CC: no word given, every piece as before"
  $NICE "$OUT/fx_ten_dump_$TAG" "$RECIPE" > "$OUT/$TAG/default.txt"
  GOT="$(sha256sum "$OUT/$TAG/default.txt" | cut -d' ' -f1)"
  WANT="$(grep " $TAG\$" "$HERE/golden-default.sha256" | cut -d' ' -f1)"
  if [ "$GOT" = "$WANT" ]; then
    echo "  ok   every piece, $(wc -l < "$OUT/$TAG/default.txt") lines, the same to the byte as before ($GOT)"
  else
    echo "  FAIL the pieces moved: $GOT, not $WANT (FX_TEN_BEFORE=<the older Source/TheaterCommand> to diff)"
    STATUS=1
  fi
  echo "== fx_ten_test, $CC"
  $NICE "$OUT/fx_ten_test_$TAG" "$FX" "$OUT/$TAG" | tee "$OUT/$TAG/test.txt" || STATUS=1
  BUILT+=("$TAG")
done
# The two compilers to each other: the same pieces and the same checks (the time lines aside).
if [ "${#BUILT[@]}" -eq 2 ]; then
  A="$OUT/${BUILT[0]}"
  B="$OUT/${BUILT[1]}"
  # (the two compilers' pieces differ in their last digits, as the kit's preview frames do, so each dump is held to
  # its own compiler's golden above; here the checks are held to each other: each check's name and verdict)
  Strip() { grep -E "^  (ok|FAIL) " "$1" | sed -E 's/^  (ok|FAIL) +/\1 /; s/  +.*$//'; }
  if diff <(Strip "$A/test.txt") <(Strip "$B/test.txt") > "$OUT/compilers.diff"; then
    echo "  ok   g++ and clang++ pass and fail the same checks"
  else
    echo "  FAIL g++ and clang++ differ (see $OUT/compilers.diff)"
    STATUS=1
  fi
fi
# The kit before, when given: the dump built against it, the two diffed whole.
if [ -n "${FX_TEN_BEFORE:-}" ] && [ "${#BUILT[@]}" -ge 1 ]; then
  $NICE g++ $FLAGS -I"$FX_TEN_BEFORE" "$HERE/fx_ten_dump.cpp" "$FX_TEN_BEFORE/Effects/TheaterFxCore.cpp" "$FX_TEN_BEFORE/Battle/LandJson.cpp" -o "$OUT/fx_ten_dump_before"
  $NICE "$OUT/fx_ten_dump_before" "$RECIPE" > "$OUT/default-before.txt"
  if cmp -s "$OUT/default-before.txt" "$OUT/${BUILT[0]}/default.txt"; then
    echo "  ok   the kit before and now lay the same pieces"
  else
    diff "$OUT/default-before.txt" "$OUT/${BUILT[0]}/default.txt" | head -20
    STATUS=1
  fi
fi
rm -f "$OUT"/core_*.o "$OUT"/json_*.o
exit $STATUS
