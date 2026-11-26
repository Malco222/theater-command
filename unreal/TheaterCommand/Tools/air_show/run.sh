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
  "$HERE/air_show_test.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_show_test"
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/AirBattles/*.json; fi
STATUS=0
for F in "$@"; do
  "$OUT/air_show_test" "$F" "$OUT" || STATUS=1
done
# Smooth flying (Battle/AirShow's SetSmooth; Brian, 24 November: "why is the plane shaking in the test video?"): a
# minute of a campaign raid with a view at every step of the rules, as the game feeds a live raid, played at 60 frames
# a second. No aircraft's tremor may be what a chase camera shows (air_smooth_probe.cpp; AIR_SMOOTH_OLD=1 reports how
# release 1.5 flew it: 74 of its 143 aircraft trembled).
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_smooth_probe.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_smooth_probe"
"$OUT/air_smooth_probe" "$HERE/data/raid-steps.json" 60 200 260 || STATUS=1
# A raid on troops (Battle/AirMarks, Battle/AirShowTroops; Brian, 24 November 2026: "if a bomber group is bombing troops
# instead of a factory or town and they get intercepted than i want that playable"): every hit on a piece of the column
# shown once as its bomb or rounds arrive, every other bomb clear of every piece, the strafing pass within the flight's
# limits, a piece that is out never changed again. A recording with no marks has nothing to check. Its dump,
# <key>-marks.csv, is drawn by Tools/air_show/marks_picture.py.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_marks_test.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_marks_test"
for F in "$@"; do
  "$OUT/air_marks_test" "$F" "$OUT" || STATUS=1
done
# The airscrews (Battle/AirScrew): every sculpted aircraft's found where its maker put them, each blade in one.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_screw_test.cpp" "$SRC/Battle/AirScrew.cpp" -o "$OUT/air_screw_test"
"$OUT/air_screw_test" "$ROOT"/Content/Models/*.json || STATUS=1
# The light shapes (Battle/AirLight, AS1): every aircraft's far part taken down for its copies past 1.5 km, held within
# half a pixel there of the far part, its size and its paints' shares kept.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_light_test.cpp" "$SRC/Battle/AirLight.cpp" -o "$OUT/air_light_test"
"$OUT/air_light_test" "$ROOT"/Content/Models/*.json || STATUS=1
# The tallies (Battle/AirTally): which mark goes in which slot of an ace's victories or a bomber's record, against the
# modeller's own layout (tallies.fill).
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_tally_test.cpp" "$SRC/Battle/AirTally.cpp" -o "$OUT/air_tally_test"
"$OUT/air_tally_test" "$ROOT/Content/Data/Tally/name-letters.json" || STATUS=1
# The bombers' nose art (Battle/AirNoseArt, release 2): the pack (Tools/pack_nose_art.py), the panel on the B-17 against
# her near shape (on her skin on both sides, facing out, reading from left to right), and which aircraft wears which
# painting: the recordings given played through the show, and two raids the rules made with their seats
# (nose_art_raid.json, tools/air-record/nose-art-raid.ts): one played through its first named losses, one whose
# ticket's order is not by corps.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_nose_art_test.cpp" "$SRC/Battle/AirNoseArt.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_nose_art_test"
"$OUT/air_nose_art_test" "$ROOT/Content/Data/NoseArt" "$ROOT/Content/Models" "$HERE/nose_art_raid.json" "$@" || STATUS=1
# Each aircraft's own figures (Battle/AirPerf and Content/Data/Air/performance.json; Brian, 24 November: "i want the
# planes flying with realistic capabilities and tactics"): every figure's source and mark, every model the files, the
# scene, the recordings and the rules (doctrine.ts, ships.ts, when the rules sit two folders up) name found, each type's
# limits inside their bounds, and the orderings the record is sure of.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_perf_test.cpp" "$SRC/Battle/AirPerf.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_perf_test"
RULES_AIR="$ROOT/../../src/battle/air"
if [ -d "$RULES_AIR" ]; then
  "$OUT/air_perf_test" "$ROOT/Content/Data/Air/performance.json" "$ROOT/Content/Models" "$RULES_AIR" "$@" || STATUS=1
else
  "$OUT/air_perf_test" "$ROOT/Content/Data/Air/performance.json" "$ROOT/Content/Models" "$@" || STATUS=1
fi
# The groups' earned colours (Battle/AirUnitColours, release 2): the art director's palette as the game carries it, every
# aircraft model's spinner, nose band and tail found and coloured at every step, and a hand-made raid's board read
# (unit_colours_raid.json: place.corpsMarks and its groups' corps).
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_unit_colours_test.cpp" "$SRC/Battle/AirUnitColours.cpp" "$SRC/Battle/AirScrew.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_unit_colours_test"
"$OUT/air_unit_colours_test" "$ROOT/Content/Data/Air/unit-colours.json" "$ROOT/Content/Models" "$HERE/unit_colours_raid.json" || STATUS=1
# The clouds turned on the graphics chip (Battle/AirSky's Chip and Battle/AirSkyChip.inl, the text the chip runs):
# every point the chip draws against the painter's, on the ground's two skies and every recording given.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_sky_chip_test.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_sky_chip_test"
"$OUT/air_sky_chip_test" "$@" || STATUS=1
# The sea's face (Battle/SeaFace): each stage's shore picture, as AAirStage makes it from the stage's land and reef,
# and the whitecaps' picture; both written to $OUT for the stand-in frames.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/sea_face_test.cpp" "$SRC/Battle/SeaFace.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/sea_face_test"
"$OUT/sea_face_test" "$ROOT/Content/Data/Stages" "$OUT" || STATUS=1
# The sky's picture (the clouds as the scene lays and turns them, drawn by a small software painter): AIR_SKY_PICTURES=1.
if [ "${AIR_SKY_PICTURES:-}" = "1" ]; then
  $CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
    "$HERE/air_sky_picture.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirLand.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
    -o "$OUT/air_sky_picture"
  for F in "$@"; do "$OUT/air_sky_picture" "$F" "$OUT" || STATUS=1; done
fi
# The ground's pictures under a raid with no stage (Battle/AirLand), looked at from the bomber: AIR_LAND_PICTURES=1.
# (Bremen's sun from its latitude at half past twelve, sun time; the north given here stands only for a recording from
# before place.ground.north, and Bremen's now carries its own. The ground is read as the scene reads it, through the
# anisotropic filter; AIR_LAND_ANISO=1 for plain trilinear, AIR_LAND_W and AIR_LAND_H for the picture's size, and
# AIR_LAND_CAM="eye east north up  look east north up  lens" in metres from the aim for one shot of one's own.)
if [ "${AIR_LAND_PICTURES:-}" = "1" ]; then
  $CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
    "$HERE/air_land_picture.cpp" "$SRC/Battle/AirLand.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
    -pthread -o "$OUT/air_land_picture"
  for F in "$@"; do
    case "$F" in
      *bremen*) "$OUT/air_land_picture" "$F" "$ROOT/Content/Data/AirGround/towns.json" "$OUT" -0.730 -0.684 53.08 12.5 || STATUS=1 ;;
      *) "$OUT/air_land_picture" "$F" "$ROOT/Content/Data/AirGround/towns.json" "$OUT" || STATUS=1 ;;
    esac
  done
fi
# The ground kept on disk (Battle/AirGroundKeep.h; release 2, the air battle's hitch, rows flyPaintEarly and
# flyGroundKeep), on Bremen's place: the key changes with each thing the painter reads and with nothing else, a kept
# ground packed and unpacked (and through a file) is the same bytes, a file cut short, lengthened or altered is refused,
# and the folder keeps the six grounds last used.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_ground_keep_test.cpp" "$SRC/Battle/AirLand.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_ground_keep_test"
"$OUT/air_ground_keep_test" "$ROOT/Content/Data/AirBattles/bremen-b17.json" "$ROOT/Content/Data/AirGround/towns.json" "$OUT" || STATUS=1
echo "tracks in $OUT"
exit $STATUS
