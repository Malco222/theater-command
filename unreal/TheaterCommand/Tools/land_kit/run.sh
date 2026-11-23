#!/bin/bash
# The ground battle's vehicles' cloud test: builds Battle/LandKit (the game's own code, no engine) and plays every
# tank, gun and lorry of a recorded battle through it at 60 frames a second, checking what Brian's brief asks of them
# (no NaN, bodies joined, the turret no faster than its control, the barrel's recoil on every shot, a wreck still,
# nothing sunk into or floating over the ground) and timing it. Builds with clang too when it is here, with the Mac's
# strictest warning (a loop that can run only once fails the Mac build).
# Usage: Tools/land_kit/run.sh [recording.json]   (default: Content/Data/LandBattles/normandy-1944.json)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_KIT_OUT:-${TMPDIR:-/tmp}/land_kit}"
mkdir -p "$OUT"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I$SRC"
# LandCrater's scars read the field's hedges and lanes (OldLinesOf), so the field's own code links in too.
FIELD="$SRC/Battle/LandField.cpp $SRC/Battle/LandSteppe.cpp $SRC/Battle/LandScape.cpp $SRC/Battle/LandDesert.cpp $SRC/Battle/LandMed.cpp $SRC/Battle/LandTropic.cpp $SRC/Battle/LandFarm.cpp $SRC/Battle/LandSnow.cpp $SRC/Battle/LandDry.cpp $SRC/Battle/LandJson.cpp"
if command -v clang++ >/dev/null 2>&1; then
  clang++ $FLAGS -Wunreachable-code-aggressive "$HERE/land_kit_test.cpp" "$SRC/Battle/LandKit.cpp" "$SRC/Battle/Fates.cpp" "$SRC/Battle/LandCrater.cpp" $FIELD \
    -o "$OUT/land_kit_test_clang"
fi
CXX="${CXX:-g++}"
$CXX $FLAGS "$HERE/land_kit_test.cpp" "$SRC/Battle/LandKit.cpp" "$SRC/Battle/Fates.cpp" "$SRC/Battle/LandCrater.cpp" $FIELD -o "$OUT/land_kit_test"
"$OUT/land_kit_test" "$ROOT/Content" "$@"
