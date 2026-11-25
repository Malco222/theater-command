#!/bin/bash
# The men's worn look in the cloud (release 2, behind tc.Look.Worn: LandMen::Crowd::SetWearMix, the soldier files' own
# wear and weather with the field's soil, and each paint's roughness): builds land_worn_men_test.cpp with g++ and clang
# under the Mac's strict warnings and runs both on Content's soldier files; then the OFF proof against the code before
# the worn look: the same program's --dump built against Battle/LandMen as it stood at LAND_WORN_BASE (default 1330c56f,
# the commit the worn look was made on; read with git show) and against today's, every byte of every look the crowd
# makes with the switch off compared (each man's and rider's at every level, the banked far sets, the body sets and
# their helmets, the uniforms that age off and on). Without git or that commit, said and skipped.
# Usage: Tools/land_show/worn.sh [soldiers folder]  (LAND_WORN_OUT: where; default ${TMPDIR:-/tmp}/land_worn)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
SOLDIERS="${1:-$ROOT/Content/Models/Soldiers}"
ARMIES="$ROOT/Content/Data/LandBattles/armies.json"
OUT="${LAND_WORN_OUT:-${TMPDIR:-/tmp}/land_worn}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
STRICT="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror"
$CXX $STRICT -I"$SRC" -I"$HERE" -pthread "$HERE/land_worn_men_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_worn_men_test"
STATUS=0
"$OUT/land_worn_men_test" "$SOLDIERS" "$ARMIES" "$SOLDIERS/pose_bank.json" || STATUS=1
if command -v clang++ >/dev/null 2>&1; then
  clang++ $STRICT -Wunreachable-code-aggressive -I"$SRC" -I"$HERE" -pthread "$HERE/land_worn_men_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_worn_men_test_clang"
  "$OUT/land_worn_men_test_clang" "$SOLDIERS" "$ARMIES" "$SOLDIERS/pose_bank.json" > "$OUT/clang.txt" || { cat "$OUT/clang.txt"; STATUS=1; }
  tail -n 1 "$OUT/clang.txt" | sed 's/^/clang: /'
fi
BASE="${LAND_WORN_BASE:-1330c56f}"
if command -v git >/dev/null 2>&1 && git -C "$ROOT" cat-file -e "$BASE^{commit}" 2>/dev/null; then
  mkdir -p "$OUT/base/Battle"
  git -C "$ROOT" show "$BASE:./Source/TheaterCommand/Battle/LandMen.h" > "$OUT/base/Battle/LandMen.h"
  git -C "$ROOT" show "$BASE:./Source/TheaterCommand/Battle/LandMen.cpp" > "$OUT/base/Battle/LandMen.cpp"
  # (the base's own header first: the test's dump uses only what the crowd had then, -DLAND_WORN_OLD)
  $CXX $STRICT -DLAND_WORN_OLD -I"$OUT/base" -I"$HERE" -pthread "$HERE/land_worn_men_test.cpp" "$OUT/base/Battle/LandMen.cpp" -o "$OUT/land_worn_men_base"
  "$OUT/land_worn_men_base" --dump "$OUT/looks_base.bin" "$SOLDIERS" "$ARMIES" "$SOLDIERS/pose_bank.json"
  "$OUT/land_worn_men_test" --dump "$OUT/looks_now.bin" "$SOLDIERS" "$ARMIES" "$SOLDIERS/pose_bank.json"
  if cmp -s "$OUT/looks_base.bin" "$OUT/looks_now.bin"; then
    echo "OFF: every look with the switch off is the code's at $BASE, to the byte ($(wc -c < "$OUT/looks_now.bin") bytes)"
  else
    echo "FAIL: OFF: the looks with the switch off are not the code's at $BASE ($(cmp "$OUT/looks_base.bin" "$OUT/looks_now.bin" || true))"
    STATUS=1
  fi
else
  echo "no commit $BASE here (git show): the comparison with the code before the worn look is skipped"
fi
echo "built in $OUT"
exit $STATUS
