#!/bin/bash
# Brian's picture sheet of the ground battle's vehicles, painted from LandKit's own bodies and poses (no engine): builds
# the painter, paints each panel, then lays them out on one sheet 1600 pixels wide (needs python3 with Pillow).
# Usage: Tools/land_kit/pictures.sh <sheet.png>   (the panels go to ${LAND_KIT_OUT:-$TMPDIR/land_kit}/pictures)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_KIT_OUT:-${TMPDIR:-/tmp}/land_kit}"
mkdir -p "$OUT/pictures"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_picture.cpp" "$SRC/Battle/LandKit.cpp" \
  "$SRC/Battle/Fates.cpp" -o "$OUT/land_picture"
"$OUT/land_picture" "$ROOT/Content" "$OUT/pictures"
python3 "$HERE/land_sheet.py" "$OUT/pictures" "${1:-$OUT/vehicles-sheet.png}"
