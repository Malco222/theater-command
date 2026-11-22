#!/bin/bash
# The battle schools' button on a corps' card (release 2), checked in the cloud: builds Campaign/BattleSchool (the
# game's own code, no engine) and runs its cases. Where Node.js is at hand it then runs each rules bundle given the way
# the game runs it (probe.mjs: a grand campaign and a history scenario, TC.school pressed as the card presses it) and
# checks the button on every corps card it showed.
# Usage: Tools/school/run.sh [campaign-rules.js ...]   (default: Content/Rules/campaign-rules.js; release 1's shows none)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SCHOOL_OUT:-${TMPDIR:-/tmp}/school}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${SCHOOL_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/school_test.cpp" "$SRC/Campaign/BattleSchool.cpp" -o "$OUT/school_test"
"$OUT/school_test"
if command -v node > /dev/null; then
  [ $# -eq 0 ] && set -- "$ROOT/Content/Rules/campaign-rules.js"
  N=0
  for RULES in "$@"; do
    N=$((N + 1))
    node "$HERE/probe.mjs" "$RULES" "$OUT/cards-$N.tsv"
    "$OUT/school_test" "$OUT/cards-$N.tsv"
  done
else
  echo "Node.js was not found: the rules bundles were not run."
fi
