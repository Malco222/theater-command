#!/bin/bash
# The sea show's cloud test: builds the show (the game's own Battle/SeaShow and SeaFeed, no engine) and plays a recorded
# fleet action through it (the JSON reader is the air feed's, which brings the air show with it). Usage: Tools/sea_show/run.sh [recording.json ...]  (default: every recording, the stand-ins
# and the convoy battle's real nights)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SEA_SHOW_OUT:-${TMPDIR:-/tmp}/sea_show}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
# (and the convoy looks' core, for the scene's own rule a boat's hits' looks are put out by, which the show's hits are held to)
$CXX ${SEA_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/sea_show_test.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  "$SRC/Battle/ConvoyLooksCore.cpp" \
  -o "$OUT/sea_show_test"
# (with the stand-ins: a hand-made convoy night until the battle's own recordings of one land, ruling 241; and the D1 stager's
# Italian flying boat on a British boat, June 1942, so the aircraft-on-a-boat check is run every time, not only when asked;
# and the real nights: the naval rules' wolf pack against a convoy (D2), fought by the computer on both sides and recorded
# in the scene's own names (naval-battles/wolfpack-r2/recordings: September 1940, March 1943, January 1944; each a 720 s
# window of its night on the night's own clock), so every change is played against the battle's own convoy nights)
# (and the naval rules' own-water nights, Brian 25 November 12:52Z, "the friendly ships would pass though right in front of
# torpedoes": the light ships, Samar and the two lines refought with the rules' switch keeping own ships out of a running
# salvo's water, naval-battles/own-water-r2/recordings, in Tools/sea_show/own-water)
# (a checkout without the recordings plays the rest: a glob that matches nothing drops out, never a path that is not there)
shopt -s nullglob
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/SeaBattles/*.json "$HERE"/stand-ins/*.json "$HERE"/recordings/*.json "$HERE"/own-water/*.json; fi
shopt -u nullglob
# (the real nights' known failures, the naval rules' side until each fix lands: Tools/sea_show/known_failures.txt, each a
# check on a night and the most it may measure; within it sea_show_test prints KNOWN and passes, past it or any other
# failure fails the run)
export SEA_SHOW_KNOWN="${SEA_SHOW_KNOWN:-$HERE/known_failures.txt}"
# (the recordings made with the naval rules' own-water switch on: Tools/sea_show/own_water.txt, a name a line, the rules'
# recorder writing no mark of it; on those nights no track's head may pass through an own ship anywhere)
export SEA_SHOW_OWN_WATER="${SEA_SHOW_OWN_WATER:-$HERE/own_water.txt}"
STATUS=0
for F in "$@"; do
  "$OUT/sea_show_test" "$F" "$OUT" || STATUS=1
done
# The view under the water (U1): the director and the look (Battle/SeaUnderCore) over the same recordings (the real nights too), the shots
# PlanShots plans mirrored from the game's own source (Tools/sea_show/sea_under_test.cpp).
$CXX ${SEA_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/sea_under_test.cpp" "$SRC/Battle/SeaUnderCore.cpp" "$SRC/Battle/ConvoyLooksCore.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" \
  "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/sea_under_test"
"$OUT/sea_under_test" "$SRC" "$@" || STATUS=1
echo "tracks in $OUT"
exit $STATUS
