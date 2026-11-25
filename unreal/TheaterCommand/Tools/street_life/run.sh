#!/bin/bash
# Street cars on the campaign map, their cloud test (release 2): builds Campaign/StreetLife.h (the game's own code, no
# engine) with the strictest warnings and checks it. A car leaves her stop from rest and comes to her terminus at rest;
# she is never faster than her best, never goes backward, never gathers or loses way faster than a street car may; on a
# short line she never reaches her best; by the clock she never jumps, stands at each end as long as she should, and is
# in the same place a round later; no two lines keep step; an arm is worked only where its cobble is laid unbroken from
# the square, her stop is clear of the square and her terminus short of the cobble's end, over two thousand avenues
# made by chance.
# Usage: Tools/street_life/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${STREETLIFE_OUT:-${TMPDIR:-/tmp}/street_life}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${STREETLIFE_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/street_life_test.cpp" -o "$OUT/street_life_test"
"$OUT/street_life_test" "$@"
