#!/bin/bash
# The air show's cloud test: builds the show (the game's own Battle/AirShow and AirFeed, no engine) and plays a
# recorded air battle through it. Usage: Tools/air_show/run.sh [recording.json ...]  (default: every recording)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${AIR_SHOW_OUT:-${TMPDIR:-/tmp}/air_show}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_show_test.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_show_test"
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/AirBattles/*.json; fi
STATUS=0
for F in "$@"; do
  "$OUT/air_show_test" "$F" "$OUT" || STATUS=1
done
echo "tracks in $OUT"
exit $STATUS
