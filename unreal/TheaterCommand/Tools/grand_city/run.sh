#!/bin/bash
# Grander great cities on the campaign map, their cloud test (release 2): builds Campaign/GrandCity.h (the game's own
# code, no engine) with the strictest warnings and checks it. Every answer is the same each time it is asked, and none
# is thrown on the town builder's dice. The heart keeps its cornice and never stands taller at its edge than at the
# square; the terraces step down from it; a turret or a dome stays under a capital's own dome. The suburb never takes
# a block the builder may lay, never passes the town's own measure, thins toward its rim, keeps off the roads out of
# the town; its villas stand on their lawns, clear of each other and of their trees, and two gardens never meet, over
# towns made by chance.
# Usage: Tools/grand_city/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${GRANDCITY_OUT:-${TMPDIR:-/tmp}/grand_city}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${GRANDCITY_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" \
  "$HERE/grand_city_test.cpp" -o "$OUT/grand_city_test"
"$OUT/grand_city_test" "$@"
