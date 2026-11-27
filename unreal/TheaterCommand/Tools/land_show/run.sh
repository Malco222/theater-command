#!/bin/bash
# The men's cloud test: builds Battle/LandMen (the game's own code, no engine) and plays a recorded ground battle's men
# through it at 60 frames a second, checking the falls, the bodies, the shots, the feet and the frame's cost.
# Usage: Tools/land_show/run.sh [recording.json ...]  (default: every recording in Content/Data/LandBattles)
# LAND_MEN_PICTURES=1 also paints the picture sheets (the falls, and a moment of the battle) with the crowd's own
# skinning: LAND_MEN_PICTURES_OUT for where (default: the output folder).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_SHOW_OUT:-${TMPDIR:-/tmp}/land_show}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
FLAGS="${LAND_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I$SRC -I$HERE"
$CXX $FLAGS -pthread "$HERE/land_show_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_show_test"
# The Mac builds with clang and its -Wunreachable-code-aggressive: build with it too where it is here.
if command -v clang++ >/dev/null 2>&1; then
  clang++ ${LAND_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -I"$SRC" -I"$HERE" -pthread "$HERE/land_show_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_show_test_clang"
fi
if [ "$#" -eq 0 ]; then
  set --
  for F in "$ROOT"/Content/Data/LandBattles/*.json; do
    case "$F" in */armies.json) ;; *) set -- "$@" "$F" ;; esac
  done
fi
# The pose bank's test (far men as baked poses, SO1): the bake's cost, banked men against the live ones, the same play.
$CXX $FLAGS -pthread "$HERE/land_bank_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_bank_test"
# The bank in vertex-animation form (mid and far men as instances): the buffers, the material's blend, the pop at 25 m.
$CXX $FLAGS -pthread "$HERE/land_vat_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_vat_test"
# The crowd's poses' picture as LandVat hands it to M_LandCrowd (Battle/LandVatPack.h): layout, lookups, halfs, bounds.
$CXX $FLAGS -pthread "$HERE/land_vat_pack_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_vat_pack_test"
STATUS=0
# The men's front by the place on the earth (the Pacific past the date line, Formosa off the China coast).
$CXX $FLAGS -pthread "$HERE/land_front_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_front_test"
"$OUT/land_front_test" || STATUS=1
# No man drawn in a wall's stone (Battle/LandStone.h, the crowd's SetStone): the walls' own answers, and the crowd on
# each recording with walls laid through resting men's places.
$CXX $FLAGS -pthread "$HERE/land_stone_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_stone_test"
if command -v clang++ >/dev/null 2>&1; then
  clang++ ${LAND_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -I"$SRC" -I"$HERE" -pthread "$HERE/land_stone_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_stone_test_clang"
fi
for F in "$@"; do
  "$OUT/land_show_test" "$F" "$ROOT/Content/Models/Soldiers" "$ROOT/Content/Data/LandBattles/armies.json" || STATUS=1
  "$OUT/land_stone_test" "$F" "$ROOT/Content/Models/Soldiers" "$ROOT/Content/Data/LandBattles/armies.json" || STATUS=1
  "$OUT/land_bank_test" "$F" "$ROOT/Content/Models/Soldiers" "$ROOT/Content/Data/LandBattles/armies.json" "$ROOT/Content/Models/Soldiers/pose_bank.json" || STATUS=1
  "$OUT/land_vat_test" "$F" "$ROOT/Content/Models/Soldiers" "$ROOT/Content/Data/LandBattles/armies.json" "$ROOT/Content/Models/Soldiers/pose_bank.json" || STATUS=1
  "$OUT/land_vat_pack_test" "$F" "$ROOT/Content/Models/Soldiers" "$ROOT/Content/Data/LandBattles/armies.json" "$ROOT/Content/Models/Soldiers/pose_bank.json" || STATUS=1
done
if [ "${LAND_MEN_PICTURES:-}" = "1" ]; then
  PICS="${LAND_MEN_PICTURES_OUT:-$OUT}"
  mkdir -p "$PICS"
  $CXX $FLAGS -pthread "$HERE/land_men_picture.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_men_picture"
  "$OUT/land_men_picture" "$1" "$ROOT/Content/Models/Soldiers" "$ROOT/Content/Data/LandBattles/armies.json" "$OUT" || STATUS=1
  python3 "$HERE/land_men_sheet.py" "$OUT" "$PICS" || STATUS=1
fi
echo "built in $OUT"
exit $STATUS
