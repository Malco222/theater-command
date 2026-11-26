#!/bin/bash
# The effects kit's sea items of release 2's ten dramatic moments (items 9, 10, 11: Effects/TheaterFxSea.inl), tested
# in the cloud with no engine: built with g++ and clang++ under the strict flags and -ffp-contract=off (the Mac's
# Unreal toolchain passes it), run, and the two compilers' prints held to each other (the time lines aside).
#
#     Tools/fx_sea/run.sh [out folder]          (default ${TMPDIR:-/tmp}/fx_sea)
#
#   fx_sea_test: DeckDrench (where and when its head lands, at most two a ship, nothing out of reach), DeckWash and the
#   ship's frame (ShipRide: the sheets ride her within a centimetre through a minute of turning and heeling, strength,
#   stop), the oil lane (width, drift and darkness by age, its caps, a 20-minute track, a ship that stops and one that
#   sinks), the deck's brass (rolling past 2 degrees of heel and not below, a coaming, over the side, the caps, the
#   ride, the aircraft-style cases kept off for her alone), and the AA guns' haze (the overlay off: as today; on: its
#   tails, its one-in-N puffs, its cap of hanging puffs through a fleet's barrage, shedding past four fifths of the
#   room); and that none of them draws on the battle's shared seed.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
RECIPE="$ROOT/Content/Effects/battle-fx.json"
HAZE="$ROOT/Content/Effects/battle-fx-sea-haze.json"
OUT="${1:-${TMPDIR:-/tmp}/fx_sea}"
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
  $CC $FLAGS $EXTRA -I"$SRC" -c "$SRC/Effects/TheaterFxCore.cpp" -o "$OUT/core_$TAG.o"
  $CC $FLAGS $EXTRA -I"$SRC" -c "$SRC/Battle/LandJson.cpp" -o "$OUT/json_$TAG.o"
  $CC $FLAGS $EXTRA -I"$SRC" "$HERE/fx_sea_test.cpp" "$OUT/core_$TAG.o" "$OUT/json_$TAG.o" -o "$OUT/fx_sea_test_$TAG"
  mkdir -p "$OUT/$TAG"
  echo "== fx_sea_test, $CC"
  "$OUT/fx_sea_test_$TAG" "$RECIPE" "$HAZE" "$OUT/$TAG" | tee "$OUT/$TAG/test.txt" || STATUS=1
  BUILT+=("$TAG")
done
if [ "${#BUILT[@]}" -eq 2 ]; then
  Strip() { grep -v -e "time:" -e " ms " -e "^The sea's dramatic" "$1"; }
  if diff <(Strip "$OUT/${BUILT[0]}/test.txt") <(Strip "$OUT/${BUILT[1]}/test.txt") > "$OUT/compilers.diff"; then
    echo "  ok   g++ and clang++ print the same checks and numbers"
  else
    echo "  FAIL g++ and clang++ differ (see $OUT/compilers.diff)"
    STATUS=1
  fi
fi
if [ "$STATUS" -eq 0 ]; then echo "fx_sea: all passed, in $OUT"; else echo "fx_sea: FAILED, in $OUT"; fi
exit $STATUS
