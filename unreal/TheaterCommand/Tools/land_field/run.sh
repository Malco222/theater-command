#!/bin/bash
# The land battle's ground in the cloud: builds Battle/LandField (no engine) with the test beside it, lays the ground of
# each recorded land battle, times it, counts it and checks it. Usage: Tools/land_field/run.sh [recording.json ...]
# (default: every recording in Content/Data/LandBattles).
# LAND_FIELD_PICTURES=<folder> also paints each ground: a soldier's eye toward the village, 30 m up over the bocage,
# and the whole field from above (PNG, 1600 pixels wide; ImageMagick's convert writes them). LAND_FIELD_EYE="x y
# angle" puts the soldier's eye at a place of one's own (metres from the field's south-west corner; the angle in degrees
# from east, counter-clockwise: 90 looks north. The test prints it back as degrees from north).
# LAND_FIELD_GAPS=1 lists every run of hedgerow 8 m or longer that a man's eye sees over (what is near it, and why).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_FIELD_OUT:-${TMPDIR:-/tmp}/land_field}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${LAND_FIELD_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" -I"$HERE" \
  "$HERE/land_field_test.cpp" "$HERE/land_field_picture.cpp" "$SRC/Battle/LandField.cpp" "$SRC/Battle/LandSteppe.cpp" "$SRC/Battle/LandScape.cpp" "$SRC/Battle/LandDesert.cpp" "$SRC/Battle/LandMed.cpp" "$SRC/Battle/LandTropic.cpp" "$SRC/Battle/LandFarm.cpp" "$SRC/Battle/LandSnow.cpp" "$SRC/Battle/LandDry.cpp" "$SRC/Battle/LandJson.cpp" "$SRC/Battle/LandCrater.cpp" \
  -pthread -o "$OUT/land_field_test"
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/LandBattles/*.json; fi
STATUS=0
for F in "$@"; do
  case "$F" in *armies.json) continue ;; esac
  if [ -n "${LAND_FIELD_PICTURES:-}" ]; then
    mkdir -p "$LAND_FIELD_PICTURES"
    "$OUT/land_field_test" "$F" "$LAND_FIELD_PICTURES" || STATUS=1
  else
    "$OUT/land_field_test" "$F" || STATUS=1
  fi
done
exit $STATUS
