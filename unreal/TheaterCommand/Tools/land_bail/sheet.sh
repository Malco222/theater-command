#!/bin/bash
# The bail clips' picture (Effects, release 2): a crewman out of her turret and off her hull in the soldiers' bail
# clips, and a man on fire running and rolling, as the scene lays LandBail's poses on a rider's seat. bail_clips.ppm.
# Usage: Tools/land_bail/sheet.sh [soldiers folder with the bail clips] [out folder]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${2:-${TMPDIR:-/tmp}/land_bail_sheet}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" -I"$ROOT/Tools/land_show" -pthread "$HERE/bail_sheet.cpp" "$SRC/Battle/LandMen.cpp" \
	"$SRC/Battle/LandBail.cpp" "$SRC/Battle/Fates.cpp" -o "$OUT/bail_sheet"
"$OUT/bail_sheet" "${1:-$ROOT/Content/Models/Soldiers}" "$ROOT/Content/Data/LandBattles/armies.json" "$OUT"
