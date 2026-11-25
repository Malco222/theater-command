#!/bin/bash
# Armies with weight on the campaign map, its cloud test (release 2): builds Campaign/ArmyCarry.h (the game's own
# code, no engine) with the strictest warnings and checks it. The rounded road keeps its ends and never leaves the
# corner it rounds; a piece is clear of the table before it goes, over its place before it lands, and lands moving;
# it hangs back from the hand as it starts, swings on as it stops and swings wide round a bend, never past its most,
# the same at any frame rate; it lands all but upright, rocks once and is still; the low edge of its base never goes
# under the table; and the dust runs out, rises and is gone, more for a bigger army.
# Usage: Tools/army_carry/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${ARMYCARRY_OUT:-${TMPDIR:-/tmp}/army_carry}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${ARMYCARRY_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/army_carry_test.cpp" -o "$OUT/army_carry_test"
"$OUT/army_carry_test" "$@"
