#!/bin/bash
# The sea show's cloud test: builds the show (the game's own Battle/SeaShow and SeaFeed, no engine) and plays a recorded
# fleet action through it (the JSON reader is the air feed's, which brings the air show with it). Usage: Tools/sea_show/run.sh [recording.json ...]  (default: every recording)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SEA_SHOW_OUT:-${TMPDIR:-/tmp}/sea_show}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${SEA_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/sea_show_test.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/SeaFeed.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/sea_show_test"
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/SeaBattles/*.json; fi
STATUS=0
for F in "$@"; do
  "$OUT/sea_show_test" "$F" "$OUT" || STATUS=1
done
echo "tracks in $OUT"
exit $STATUS
