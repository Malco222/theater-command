#!/bin/bash
# The sea show's cloud test: builds the show (the game's own Battle/SeaShow and SeaFeed, no engine) and plays a recorded
# fleet action through it (the JSON reader is the air feed's, which brings the air show with it). Usage: Tools/sea_show/run.sh [recording.json ...]  (default: every recording, and the stand-ins)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SEA_SHOW_OUT:-${TMPDIR:-/tmp}/sea_show}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
# (and the convoy looks' core, for the scene's own rule a boat's hits' looks are put out by, which the show's hits are held to)
$CXX ${SEA_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/sea_show_test.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  "$SRC/Battle/ConvoyLooksCore.cpp" \
  -o "$OUT/sea_show_test"
# (with the stand-ins: a hand-made convoy night until the battle's own recordings of one land, ruling 241; and the D1 stager's
# Italian flying boat on a British boat, June 1942, so the aircraft-on-a-boat check is run every time, not only when asked)
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/SeaBattles/*.json "$HERE"/stand-ins/*.json; fi
STATUS=0
for F in "$@"; do
  "$OUT/sea_show_test" "$F" "$OUT" || STATUS=1
done
# The view under the water (U1): the director and the look (Battle/SeaUnderCore) over the same recordings, the shots
# PlanShots plans mirrored from the game's own source (Tools/sea_show/sea_under_test.cpp).
$CXX ${SEA_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/sea_under_test.cpp" "$SRC/Battle/SeaUnderCore.cpp" "$SRC/Battle/ConvoyLooksCore.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" \
  "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/sea_under_test"
"$OUT/sea_under_test" "$SRC" "$@" || STATUS=1
echo "tracks in $OUT"
exit $STATUS
