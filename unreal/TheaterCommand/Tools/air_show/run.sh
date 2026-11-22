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
  "$HERE/air_show_test.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_show_test"
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/AirBattles/*.json; fi
STATUS=0
for F in "$@"; do
  "$OUT/air_show_test" "$F" "$OUT" || STATUS=1
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
# The clouds turned on the graphics chip (Battle/AirSky's Chip and Battle/AirSkyChip.inl, the text the chip runs):
# every point the chip draws against the painter's, on the ground's two skies and every recording given.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_sky_chip_test.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_sky_chip_test"
"$OUT/air_sky_chip_test" "$@" || STATUS=1
# The sea's face (Battle/SeaFace): each stage's shore picture, as AAirStage makes it from the stage's land and reef,
# and the whitecaps' picture; both written to $OUT for the stand-in frames.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/sea_face_test.cpp" "$SRC/Battle/SeaFace.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/sea_face_test"
"$OUT/sea_face_test" "$ROOT/Content/Data/Stages" "$OUT" || STATUS=1
# The sky's picture (the clouds as the scene lays and turns them, drawn by a small software painter): AIR_SKY_PICTURES=1.
if [ "${AIR_SKY_PICTURES:-}" = "1" ]; then
  $CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
    "$HERE/air_sky_picture.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirLand.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
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
    "$HERE/air_land_picture.cpp" "$SRC/Battle/AirLand.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
    -pthread -o "$OUT/air_land_picture"
  for F in "$@"; do
    case "$F" in
      *bremen*) "$OUT/air_land_picture" "$F" "$ROOT/Content/Data/AirGround/towns.json" "$OUT" -0.730 -0.684 53.08 12.5 || STATUS=1 ;;
      *) "$OUT/air_land_picture" "$F" "$ROOT/Content/Data/AirGround/towns.json" "$OUT" || STATUS=1 ;;
    esac
  done
fi
echo "tracks in $OUT"
exit $STATUS
