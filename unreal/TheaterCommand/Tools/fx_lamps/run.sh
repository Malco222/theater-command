#!/bin/bash
# The flashes' lamps' cloud test (the effects kit's Effects/TheaterFxCore, no engine): the lamp rule a scene opts into
# with SetLampRule and the naval rows (naval_heavy, naval_medium, naval_light) at sea, built with g++ and clang++ under
# the strict flags and -ffp-contract=off (the Mac's Unreal toolchain passes it), run, and held to each other.
#
#     Tools/fx_lamps/run.sh [out folder]          (default ${TMPDIR:-/tmp}/fx_lamps)
#
#   1. fx_lamps_test: the rule's checks (a broadside 2 km off over a slick burning 6 km off, by the eye and what the
#      view can see, a salvo one lamp, ships 500 m apart never one, also firing toward each other, 3 lamps asked and
#      never over 4, every lamp a number, the rows' strength night and day and their reach, the light on her side by
#      its shape and, at the shipped lamp_cd, its level), the rows' table, the cost a frame with forty ships firing,
#      and the night fight from above (fx-lamps-night.ppm, and .png with its labels where python3 has Pillow). Both
#      compilers must print the same (the time lines aside) and draw the same picture to the byte.
#   2. fx_lamps_dump: every lamp of a scripted fight on land and at sea with no scene opting in, printed exactly, held
#      to golden-default-lamps.sha256 (the kit before the lamp rule printed it): not one lamp moves unless a scene asks.
#      FX_LAMPS_BEFORE=<an older Source/TheaterCommand> builds the dump against that kit too and diffs the two whole.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
RECIPE="$ROOT/Content/Effects/battle-fx.json"
OUT="${1:-${TMPDIR:-/tmp}/fx_lamps}"
mkdir -p "$OUT"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -ffp-contract=off"
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
  # (the kit is built once a compiler, as an object, and linked into both)
  $CC $FLAGS $EXTRA -I"$SRC" -c "$SRC/Effects/TheaterFxCore.cpp" -o "$OUT/core_$TAG.o"
  $CC $FLAGS $EXTRA -I"$SRC" -c "$SRC/Battle/LandJson.cpp" -o "$OUT/json_$TAG.o"
  $CC $FLAGS $EXTRA -I"$SRC" "$HERE/fx_lamps_test.cpp" "$OUT/core_$TAG.o" "$OUT/json_$TAG.o" -o "$OUT/fx_lamps_test_$TAG"
  $CC $FLAGS $EXTRA -I"$SRC" "$HERE/fx_lamps_dump.cpp" "$OUT/core_$TAG.o" "$OUT/json_$TAG.o" -o "$OUT/fx_lamps_dump_$TAG"
  mkdir -p "$OUT/$TAG"
  echo "== fx_lamps_test, $CC"
  "$OUT/fx_lamps_test_$TAG" "$RECIPE" "$OUT/$TAG" | tee "$OUT/$TAG/test.txt" || STATUS=1
  echo "== fx_lamps_dump, $CC: no scene opting in, every lamp as before"
  "$OUT/fx_lamps_dump_$TAG" "$RECIPE" > "$OUT/$TAG/default-lamps.txt"
  GOT="$(sha256sum "$OUT/$TAG/default-lamps.txt" | cut -d' ' -f1)"
  WANT="$(cut -d' ' -f1 "$HERE/golden-default-lamps.sha256")"
  if [ "$GOT" = "$WANT" ]; then
    echo "  ok   the default lamps, $(wc -l < "$OUT/$TAG/default-lamps.txt") lines, the same to the byte as before ($GOT)"
  else
    echo "  FAIL the default lamps moved: $GOT, not $WANT (FX_LAMPS_BEFORE=<the older Source/TheaterCommand> to diff)"
    STATUS=1
  fi
  BUILT+=("$TAG")
done
# The two compilers to each other: the same text (the time lines and the folder aside) and the same picture.
if [ "${#BUILT[@]}" -eq 2 ]; then
  A="$OUT/${BUILT[0]}"
  B="$OUT/${BUILT[1]}"
  Strip() { grep -v -e "time:" -e "us on the mean" -e "picture written" -e "^The flashes' lamps at sea" "$1"; }
  if diff <(Strip "$A/test.txt") <(Strip "$B/test.txt") > "$OUT/compilers.diff" \
    && cmp -s "$A/fx-lamps-night.ppm" "$B/fx-lamps-night.ppm" \
    && cmp -s "$A/fx-lamps-night-labels.txt" "$B/fx-lamps-night-labels.txt" \
    && cmp -s "$A/default-lamps.txt" "$B/default-lamps.txt"; then
    echo "  ok   g++ and clang++ print the same lamps, checks and table and draw the same picture"
  else
    echo "  FAIL g++ and clang++ differ (see $OUT/compilers.diff)"
    STATUS=1
  fi
fi
# The kit before, when given: the dump built against it, the two diffed whole.
if [ -n "${FX_LAMPS_BEFORE:-}" ] && [ "${#BUILT[@]}" -ge 1 ]; then
  g++ $FLAGS -I"$FX_LAMPS_BEFORE" "$HERE/fx_lamps_dump.cpp" "$FX_LAMPS_BEFORE/Effects/TheaterFxCore.cpp" "$FX_LAMPS_BEFORE/Battle/LandJson.cpp" -o "$OUT/fx_lamps_dump_before"
  "$OUT/fx_lamps_dump_before" "$RECIPE" > "$OUT/default-lamps-before.txt"
  if cmp -s "$OUT/default-lamps-before.txt" "$OUT/${BUILT[0]}/default-lamps.txt"; then
    echo "  ok   the kit before and now hand out the same lamps"
  else
    diff "$OUT/default-lamps-before.txt" "$OUT/${BUILT[0]}/default-lamps.txt" | head -20
    STATUS=1
  fi
fi
if [ "${#BUILT[@]}" -ge 1 ] && command -v python3 > /dev/null 2>&1; then
  python3 -I "$HERE/picture.py" "$OUT/${BUILT[0]}" || true
fi
if [ "$STATUS" -eq 0 ]; then echo "fx_lamps: all passed, in $OUT"; else echo "fx_lamps: FAILED, in $OUT"; fi
exit $STATUS
