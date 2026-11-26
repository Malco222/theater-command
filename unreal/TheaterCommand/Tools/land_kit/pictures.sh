#!/bin/bash
# Brian's picture sheet of the ground battle's vehicles, painted from LandKit's own bodies and poses (no engine): builds
# the painter, paints each panel, then lays them out on one sheet 1600 pixels wide (needs python3 with Pillow).
# Usage: Tools/land_kit/pictures.sh <sheet.png>   (the panels go to ${LAND_KIT_OUT:-$TMPDIR/land_kit}/pictures)
# LAND_KIT_MARKS=1 paints the national marks' own sheet instead (release 2, -TheaterLandMarks: each owner's period
# marking as the game paints it on her skin, whole vehicles and close looks).
# LAND_KIT_ANYYEAR=1 paints the trial word "anyyear"'s own sheet instead (release 2: each vehicle by the month, as
# shipped, beside the same one with no coat or mark chosen by the year).
# LAND_KIT_ALLMARKS=1 paints the trial word "allmarks"'s own sheet instead (release 2: the British flash as shipped and as
# the map's, on the dark coat and on stone, then each of thirty-eight nations' marks close, on a dark and on a light
# coat, and from the play eye's distance; lay it with LAND_SHEET_ACROSS=6 LAND_SHEET_WIDE=3200); =flash the flash alone.
# LAND_KIT_HOUSE=1 paints the farmhouse's own sheet instead (release 2, -TheaterLandTrial=houses: the bocage's farmhouse built
# to the vehicles' detail, whole and in its three hurts, near, middle and far, and stills across its 90 m and 300 m
# switches as the game's lens shows them); LAND_KIT_HOUSE=farm the barn's and the byre's; LAND_KIT_HOUSE=church the
# village church's (nave, chancel and tower, close looks at its door, window, belfry and cross, its levels and switches);
# LAND_KIT_HOUSE=village the village's houses (render, cream limestone and rubble, a shop, a cottage, close looks at the
# shopfront, the render come away and the splinters' strikes, its hurts, its levels and switches).
# LAND_KIT_ONLY=<panel,panel> paints only those panels (and the zz_ close looks
# for working on it); LAND_KIT_CAMERA=1 writes each panel's camera beside it, for measuring.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_KIT_OUT:-${TMPDIR:-/tmp}/land_kit}"
mkdir -p "$OUT/pictures"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_picture.cpp" "$SRC/Battle/LandKit.cpp" "$SRC/Battle/LandHouse.cpp" \
  "$SRC/Battle/Fates.cpp" -o "$OUT/land_picture"
"$OUT/land_picture" "$ROOT/Content" "$OUT/pictures"
python3 "$HERE/land_sheet.py" "$OUT/pictures" "${1:-$OUT/vehicles-sheet.png}"
