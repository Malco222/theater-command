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
# The finer B-17 (the Battle rules card's "Finer B-17", row b17Fine; Brian, 25 November 2026: "why don't we have the new
# b-17 models in the game?"): Content/Models/B17Fine.json is in the folder the airscrews', light shapes', nose art's,
# figures' and earned colours' tests above read, so each has checked her already. Here: the row off reads what the game
# read before it (B17.json and panels.json the base's bytes, the flight core's B-17 the same numbers), the row on reads
# her everywhere (her file, her engines, her panel, her lines' colours, each airscrew on its own engine), and the B-17
# raid flown through the show with her engines.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_b17_fine_test.cpp" "$SRC/Battle/AirNoseArt.cpp" "$SRC/Battle/AirUnitColours.cpp" "$SRC/Battle/AirScrew.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_b17_fine_test"
"$OUT/air_b17_fine_test" "$ROOT/Content/Models" "$ROOT/Content/Data/NoseArt" "$SRC" || STATUS=1
mkdir -p "$OUT/b17fine"
AIR_SHOW_B17_FINE=1 "$OUT/air_show_test" "$ROOT/Content/Data/AirBattles/bremen-b17.json" "$OUT/b17fine" || STATUS=1
# Turning airscrews (the Battle rules card's "Turning airscrews", row flyPropSpin; Brian, 26 November 2026 04:00Z: "the
# b-17 propellers didnt look like they were spinning"): every aircraft's far mesh, past 300 m the whole of the Overview,
# loses its standing blades and its solid blur disc and keeps everything else; each screw's blades kept apart for an
# engine feathered; the near disc's smear and the far haze drawn at the battle's real scale; the row off draws what it
# drew before (its source). Content/Models and Old. Then the light shapes made from those far shapes, held to them.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_prop_spin_test.cpp" "$SRC/Battle/AirScrew.cpp" -o "$OUT/air_prop_spin_test"
"$OUT/air_prop_spin_test" "$SRC" "$ROOT"/Content/Models/*.json "$ROOT"/Content/Models/Old/*.json || STATUS=1
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -DAIR_LIGHT_SCREWS -I"$SRC" \
  "$HERE/air_light_test.cpp" "$SRC/Battle/AirLight.cpp" "$SRC/Battle/AirScrew.cpp" -o "$OUT/air_light_screws_test"
"$OUT/air_light_screws_test" "$ROOT"/Content/Models/*.json "$ROOT"/Content/Models/Old/*.json || STATUS=1
# Bomber markings (the Battle rules card's "Bomber markings", row flyBomberMarks; Brian, 25 November 2026, of the approved
# studio pictures: "Build it into the game"): the markings' data file (Tools/air_marks/make_b17_marks.py) read as the
# game reads it, made on her file, wound as it, on her skin near and far, reading the right way round, apex forward,
# clear of her star and bar; who wears which letters, the same at any cadence, never by date, the show's record untouched.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_letters_test.cpp" "$SRC/Battle/AirLetters.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_letters_test"
"$OUT/air_letters_test" "$ROOT/Content/Data/AirMarks/b17-fine-marks.json" "$ROOT/Content/Models" "$SRC" "$@" || STATUS=1
# Worn B-17 paint (the Battle rules card's "Worn B-17 paint", row flyB17Skin; Brian, 26 November 2026: "i want the new
# ones in the game"): the model thread's paint for her shape, Content/Data/AirSkin/B17Fine.json (Battle/AirSkin, made by
# Tools/make_air_skin.py), read with her counts and made for B17Fine.json's very bytes; every fault refused, a key by
# month or year among them; the law gives the tool's check colours; the earned colours' model and the nose-art panel
# under it; the Unreal side from its source. Then the tool's own test on small made-up files.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_b17_skin_test.cpp" "$SRC/Battle/AirSkin.cpp" "$SRC/Battle/LandJson.cpp" "$SRC/Battle/AirUnitColours.cpp" "$SRC/Battle/AirScrew.cpp" -o "$OUT/air_b17_skin_test"
"$OUT/air_b17_skin_test" "$ROOT/Content/Models/B17Fine.json" "$ROOT/Content/Data/AirSkin/B17Fine.json" "$SRC" || STATUS=1
python3 -I -B "$ROOT/Tools/air_skin/test_make_air_skin.py" || STATUS=1
# The B-17 paint sheen (the Battle rules card's "B-17 paint sheen", row flyB17Sheen; Brian, 26 November 2026: "make the
# new ones work"): her own finish a point (the model thread's gloss and metal), Content/Data/AirSkin/B17Fine-sheen.json
# (Battle/AirSheen, made by Tools/make_air_skin.py --sheen-out in the same run as her overlay), read with her counts and
# made from the overlay's very files; M_AirPaint's law: nothing carried is PlaneInk's numbers exactly, the glare guard,
# no garbage between corners; the launch word; M_AirPaint's nodes and the Unreal side from their source.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_b17_sheen_test.cpp" "$SRC/Battle/AirSheen.cpp" "$SRC/Battle/AirSkin.cpp" "$SRC/Battle/LandJson.cpp" "$SRC/Battle/AirUnitColours.cpp" "$SRC/Battle/AirScrew.cpp" -o "$OUT/air_b17_sheen_test"
"$OUT/air_b17_sheen_test" "$ROOT/Content/Models/B17Fine.json" "$ROOT/Content/Data/AirSkin/B17Fine-sheen.json" "$ROOT/Content/Data/AirSkin/B17Fine.json" "$SRC" "$ROOT/Tools/build_content.py" || STATUS=1
# ... and M_AirPaint's own nodes (build_content.py's "if air:" block, run on plain numbers) against AirSheen's law, case
# for case on a fixed grid: wiring and all, not only names and defaults.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_b17_sheen_grid.cpp" "$SRC/Battle/AirSheen.cpp" "$SRC/Battle/LandJson.cpp" -o "$OUT/air_b17_sheen_grid"
"$OUT/air_b17_sheen_grid" > "$OUT/air_b17_sheen_grid.txt" || STATUS=1
python3 -I -B "$HERE/air_b17_sheen_material.py" "$ROOT/Tools/build_content.py" "$OUT/air_b17_sheen_grid.txt" || STATUS=1
# The silver Mustang (row flyMustangMetal): her issue-13 file beside the old one, its finish read as the game reads it
# (AirSheen::ReadOwn), M_AirPaint's law on her numbers under her own tune (floor 0.25, no glare guard), the Unreal side.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_mustang_metal_test.cpp" "$SRC/Battle/AirSheen.cpp" "$SRC/Battle/LandJson.cpp" -o "$OUT/air_mustang_metal_test"
"$OUT/air_mustang_metal_test" "$ROOT/Content/Models/MustangBare.json" "$ROOT/Content/Models/Mustang.json" "$SRC" || STATUS=1
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
echo "tracks in $OUT"
exit $STATUS
