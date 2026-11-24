#!/bin/bash
# The crews' bail-out's cloud test (release 2, -TheaterLandBailOut): builds Battle/LandBail and Battle/Fates (the
# game's own code, no engine) with the Mac's strict warnings and plays every bail-out the fates table names.
# Usage: Tools/land_bail/run.sh (three ways: the infantry's clips, the soldiers' bail clips, and those with smoke)
# The first also builds the crowd's man each crewman becomes (Battle/LandMen) from the soldier files: LAND_BAIL_SOLDIERS,
# another soldiers folder (default Content/Models/Soldiers), such as the soldiers thread's files before they come in.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_BAIL_OUT:-${TMPDIR:-/tmp}/land_bail}"
mkdir -p "$OUT"
CXX="${CXX:-clang++}"
command -v "$CXX" >/dev/null 2>&1 || CXX=g++
$CXX -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -I"$SRC" -I"$ROOT/Tools/land_show" -pthread "$HERE/land_bail_test.cpp" "$SRC/Battle/LandBail.cpp" \
	"$SRC/Battle/Fates.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_bail_test"
"$OUT/land_bail_test" "$ROOT/Content" "${LAND_BAIL_SOLDIERS:-$ROOT/Content/Models/Soldiers}"
LAND_BAIL_CLIPS=1 "$OUT/land_bail_test" "$ROOT/Content"
LAND_BAIL_CLIPS=1 LAND_BAIL_SMOKE=1 "$OUT/land_bail_test" "$ROOT/Content"
