#!/bin/bash
# The ground-attack fighters' cloud test (Effects, release 2, -TheaterLandAirSupport): builds Battle/LandAir and
# Battle/LandFeed (the game's own code, no engine) with the Mac's strict warnings, and rounding as the Mac's build does
# (-ffp-contract=off: no a*b+c fused, as Unreal's Mac toolchain passes it), and plays three strikes of its own
# through them. See land_air_test.cpp for what it checks. With an out folder it also draws air_paths.png there, and
# chutes.png (the downed pilots: the art director's notes of 24 November). It reads the soldier files' go_prone ends
# (Content/Models/Soldiers) for the pilots' swap, and builds the crowd's man each downed pilot becomes (Battle/LandMen) from
# them. LAND_AIR_SOLDIERS: another soldiers folder (default Content/Models/Soldiers), such as the soldiers thread's files
# before they come into Content.
# Usage: Tools/land_air/run.sh [out folder]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_AIR_OUT:-${TMPDIR:-/tmp}/land_air}"
mkdir -p "$OUT"
CXX="${CXX:-clang++}"
command -v "$CXX" >/dev/null 2>&1 || CXX=g++
$CXX -std=c++20 -O2 -ffp-contract=off -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -I"$SRC" -I"$ROOT/Tools/land_show" -pthread "$HERE/land_air_test.cpp" "$SRC/Battle/LandAir.cpp" \
	"$SRC/Battle/LandFeed.cpp" "$SRC/Battle/LandJson.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_air_test"
LAND_AIR_SOLDIERS="${LAND_AIR_SOLDIERS:-$ROOT/Content/Models/Soldiers}" LAND_AIR_ARMIES="$ROOT/Content/Data/LandBattles/armies.json" "$OUT/land_air_test" ${1:+"$1"}
if [ -n "${1:-}" ]; then
  python3 "$HERE/paths.py" "$1/air_paths.txt" "$1/air_paths.png"
  python3 "$HERE/chutes.py" "$1/air_paths.txt" "$1/chute_silk.txt" "$1/chutes.png"
fi
