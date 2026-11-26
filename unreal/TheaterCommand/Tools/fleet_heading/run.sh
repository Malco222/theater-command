#!/bin/bash
# Fleets that keep their heading on the campaign map, their cloud test (release 2; Brian, 26 November 2026: "the wake
# of the boats when moving don't follow the direction they came from"): builds Campaign/FleetHeading.h (the game's own
# code, no engine) with the strictest warnings and checks it. A fleet laid on the yaw of her way has her bows along it
# and her wake back the way she came, by the sail's own reckoning of a yaw. Her standard, turned back about the piece's
# upright, faces the eye and stands where it stood whatever way she lies, and is not turned at all where she faces the
# table. A fleet that lies headed has her bows along her way to the end of the old sail. The heading of a fleet not
# seen to sail in: the way she moved, else out from her port, else out of her home sea, else her own. And the old
# sail's helm, copied here, to say how near her last leg she comes to rest.
# Usage: Tools/fleet_heading/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${FLEETHEADING_OUT:-${TMPDIR:-/tmp}/fleet_heading}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${FLEETHEADING_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/fleet_heading_test.cpp" -o "$OUT/fleet_heading_test"
"$OUT/fleet_heading_test" "$@"
