#!/bin/bash
# Busy ports on the campaign map, their cloud test (release 2): builds Campaign/PortLife.h (the game's own code, no
# engine) with the strictest warnings and checks it. A port's round is closed and has no corner; she never comes nearer
# the quay than the ships alongside it allow; her times run forward, she is never faster than her best nor stopped
# under way, her head never swings faster than a ship's may, and she slows up the harbour; by the clock she never jumps,
# lying or sailing, and two ships of one port never meet; a fairway is found only where every point of it has water and
# room, the longest there is, over two thousand coasts made by chance; her wake lies on the water she has crossed and
# nowhere else; two ports whose rounds would cross are told apart.
# Usage: Tools/port_life/run.sh [scene.json]   (writes one port's round and its ships by the clock, for Tools/port_life/picture.py)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${PORTLIFE_OUT:-${TMPDIR:-/tmp}/port_life}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${PORTLIFE_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/port_life_test.cpp" -o "$OUT/port_life_test"
"$OUT/port_life_test" "$@"
