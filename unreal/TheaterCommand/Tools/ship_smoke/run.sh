#!/bin/bash
# Funnel smoke for a fleet on the campaign map, its cloud test (release 2): builds Campaign/ShipSmoke.h (the game's
# own code, no engine) with the strictest warnings and checks it. A ship lying still shows a pale wisp and one under
# way a plume that streams astern; it is thicker and darker while she works up to speed; a plume begins at its
# funnel, grows and thins as it goes, never stands apart in beads and never jumps; its colour is never taken for a
# light by night; the puffs it costs are few and counted; on a real course (Campaign/ShipWay.h) it lies along the
# water she crossed, round her turns; each puff is as thick and dark as the fire that made it, so that the plume never
# changes colour all at once; and it is the same whatever the frame rate, for nothing is kept between frames.
# Usage: Tools/ship_smoke/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SHIPSMOKE_OUT:-${TMPDIR:-/tmp}/ship_smoke}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${SHIPSMOKE_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/ship_smoke_test.cpp" -o "$OUT/ship_smoke_test"
"$OUT/ship_smoke_test" "$@"
