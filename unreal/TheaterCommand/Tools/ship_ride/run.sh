#!/bin/bash
# A fleet under way on the campaign map, second part, its cloud test (release 2): builds Campaign/ShipRide.h (the
# game's own code, no engine) with the strictest warnings and checks it. A ship heels outward in her turn and not
# where she is swung round lying still; her roll has weight (it lags, rolls back once, and dies) and comes out the
# same at any frame rate; the swell is small, never the same for two ships, and gone when the eye is far; her masts
# lean the way they should; no part of her bottom shows above the sea; and no class of ship in the game puts her lee
# rail under, by the measurements of the hulls in Campaign/CampaignShips.cpp.
# Usage: Tools/ship_ride/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SHIPRIDE_OUT:-${TMPDIR:-/tmp}/ship_ride}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${SHIPRIDE_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/ship_ride_test.cpp" -o "$OUT/ship_ride_test"
"$OUT/ship_ride_test" "$@"
