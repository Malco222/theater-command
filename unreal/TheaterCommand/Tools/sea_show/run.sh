#!/bin/bash
# The sea show's cloud test: builds the show (the game's own Battle/SeaShow and SeaFeed, no engine) and plays a recorded
# fleet action through it (the JSON reader is the air feed's, which brings the air show with it). Usage: Tools/sea_show/run.sh [recording.json ...]  (default: every recording, the stand-ins,
# the convoy battle's real nights and the naval rules' own-water recordings)
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
# Italian flying boat on a British boat, June 1942, so the aircraft-on-a-boat check is run every time, not only when asked;
# and the real nights: the naval rules' wolf pack against a convoy (D2), fought by the computer on both sides and recorded
# in the scene's own names (naval-battles/wolfpack-r2/recordings, naval round 6a of 26 November: September 1940, March 1943,
# January 1944; each a 720 s window of its night on the night's own clock), so every change is played against the battle's
# own convoy nights; and the naval rules' own-water recordings (recordings/own-water-r2, as own-water-<name>.json: light
# ships, two battle lines and Samar, the last two also with the sailors' rules; Brian 25 November 12:52Z, "the friendly ships
# would pass though right in front of torpedoes"; recorded with the "Friendly torpedoes" switch on and the computer on both
# sides; test recordings only, Content/Data/SeaBattles stays the rules' own), so the torpedo holds and words play the rules'
# real holds and turns clear, and sea_show_test holds them strictly (own_water.txt names them)
# (a checkout without the recordings plays the rest: a glob that matches nothing drops out, never a path that is not there)
shopt -s nullglob
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/SeaBattles/*.json "$HERE"/stand-ins/*.json "$HERE"/recordings/*.json "$HERE"/recordings/own-water-r2/*.json; fi
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
# PlanShots plans mirrored from the game's own source (Tools/sea_show/sea_under_test.cpp). (Its own known failures on the
# real nights, kept as the show's are: Tools/sea_show/known_failures_under.txt, each a check on a night and the most it may
# measure; within it sea_under_test prints KNOWN and passes, past it or any other failure fails the run.)
export SEA_UNDER_KNOWN="${SEA_UNDER_KNOWN:-$HERE/known_failures_under.txt}"
$CXX ${SEA_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/sea_under_test.cpp" "$SRC/Battle/SeaUnderCore.cpp" "$SRC/Battle/ConvoyLooksCore.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" \
  "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/sea_under_test"
"$OUT/sea_under_test" "$SRC" "$@" || STATUS=1
# The torpedo words (Battle/SeaWordsCore; the naval rules behind their "Friendly torpedoes" switch): every word string for
# string, the fields through SeaFeed and the show from views written in the test (the player's order, which the computer's
# sides never record), then the same recordings through the show, the tags held to the fields the views set, and the lines
# pinned for the own-water recordings string for string (Tools/sea_show/sea_words_test.cpp).
$CXX ${SEA_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/sea_words_test.cpp" "$SRC/Battle/SeaWordsCore.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" \
  "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/sea_words_test"
"$OUT/sea_words_test" "$@" || STATUS=1
echo "tracks in $OUT"
exit $STATUS
