#!/bin/bash
# Crater variety in the cloud (release 2, -TheaterLandCraterVariety): builds Battle/LandCrater with crater_sheet.cpp,
# checks it, and paints a barrage's holes with the switch off and on (PPM, and JPEG beside them when ImageMagick's
# convert is there). Usage: Tools/land_crater/run.sh [ground word ...] (default: earth sand snow mud blackearth).
# LAND_CRATER_PICTURES=<folder> (default: the build folder).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_CRATER_OUT:-${TMPDIR:-/tmp}/land_crater}"
PICS="${LAND_CRATER_PICTURES:-$OUT}"
mkdir -p "$OUT" "$PICS"
CXX="${CXX:-g++}"
$CXX ${LAND_CRATER_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/crater_sheet.cpp" "$SRC/Battle/LandField.cpp" "$SRC/Battle/LandHouse.cpp" "$SRC/Battle/LandSteppe.cpp" "$SRC/Battle/LandScape.cpp" "$SRC/Battle/LandDesert.cpp" "$SRC/Battle/LandMed.cpp" "$SRC/Battle/LandTropic.cpp" "$SRC/Battle/LandFarm.cpp" "$SRC/Battle/LandSnow.cpp" "$SRC/Battle/LandDry.cpp" "$SRC/Battle/LandAsia.cpp" "$SRC/Battle/LandAmerica.cpp" "$SRC/Battle/LandPlain.cpp" "$SRC/Battle/LandJson.cpp" "$SRC/Battle/LandCrater.cpp" \
  -pthread -o "$OUT/crater_sheet"
if [ "$#" -eq 0 ]; then set -- earth sand snow mud blackearth; fi
STATUS=0
for W in "$@"; do
  echo "$W:"
  "$OUT/crater_sheet" "$PICS" "$W" || STATUS=1
  if command -v convert > /dev/null; then
    for P in "$PICS"/craters-"$W"-*.ppm "$PICS"/scorch-"$W"-*.ppm; do [ -e "$P" ] && convert "$P" -quality 88 "${P%.ppm}.jpg" && rm -f "$P"; done
  fi
done
exit $STATUS
