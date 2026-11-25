#!/bin/bash
# The looks picker's cloud test: builds Battle/Fates (the game's own code, no engine), reads the game's table
# (Content/Data/Battle/fates.json) and plays thousands of blows through it, checking that it never shows the same
# look twice running, that each look shows where it should, and that every look can be reached.
# Usage: Tools/fates/run.sh [table.json]   (FATES_STORY=1 prints a sample run of hits in plain words)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${FATES_OUT:-${TMPDIR:-/tmp}/fates}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${FATES_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/fates_test.cpp" "$SRC/Battle/Fates.cpp" -o "$OUT/fates_test"
"$OUT/fates_test" "${1:-$ROOT/Content/Data/Battle/fates.json}"
