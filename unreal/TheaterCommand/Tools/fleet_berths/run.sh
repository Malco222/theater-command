#!/bin/bash
# Fleets that lie apart on the campaign map, their cloud test (release 2; the art director's finds on the Mac's
# pictures of 26 November 2026: two fleets in one sea stood on each other, and a ship in the Channel lay over
# Cotentin's tip): builds Campaign/FleetBerths.h (the game's own code, no engine) with the strictest warnings and
# checks it. First on made-up seas, where the answer is known: open water, a channel, an islet by the middle, a
# middle on land, a neighbour's sea, a neck of land, a strait too narrow for anyone. Then on the chart's own coasts
# (src/generated/campaign-map.json, written out plainly by chart_coasts.py): every sea has its berths, each on open
# water by a count of its own, each in its own sea, each clear of the others, each reached by water from the first.
# The game asks its terrain how far the shore is; this test measures to the chart's coasts themselves.
# Usage: Tools/fleet_berths/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
CHART="$ROOT/../../src/generated/campaign-map.json"
OUT="${FLEETBERTHS_OUT:-${TMPDIR:-/tmp}/fleet_berths}"
mkdir -p "$OUT"
python3 -I "$HERE/chart_coasts.py" "$CHART" "$OUT/coasts.txt"
CXX="${CXX:-g++}"
$CXX ${FLEETBERTHS_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/fleet_berths_test.cpp" -o "$OUT/fleet_berths_test"
"$OUT/fleet_berths_test" "$OUT/coasts.txt" "$OUT/berths.txt"
