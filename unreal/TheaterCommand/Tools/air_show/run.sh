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
# The bombers and the strategy camera as a Mac plays them (air_smooth_probe --shake; Brian, 25 November 13:27Z: "the
# bomber is shaking in the video too like the fighter"): each bomber keeping her place in her box without weaving or
# rocking, by model and at x1, x4 and x10, the chase camera riding her, and the strategy camera's picture steady and its
# look never jumping, at 60 frames a second and at a Mac's uneven ones; the raid's minute (fed live, and late), its
# busiest forty seconds (data/raid-busy.json; at 30 frames a second too, at x10, and on other dice, where its boxes
# weaved and its strategy camera trailed off the picture before they kept their places) and the set pieces (at x10 too,
# where the strategy camera's look must be within 100 m of what it is after as often as the old camera's). Flown as
# release 1.5 flew it (AIR_SMOOTH_OLD=1), the report must be the release 2 build's before the check, to the byte
# (data/shake-old-30899121.txt). That build (30899121) flew the He 111s in weaves, banked to 20 degrees, about once a
# bomber's air minute, and the single-engined bombers banked to 61 to 75 degrees in their places, and its strategy
# camera's look jumped 14 to 18 times a real minute at x1 and 52 to 64 at x4 (410 in the busiest forty seconds at x4 on
# a Mac's frames).
for P in "60 1" "60 4" "60 10" "mac 1" "mac 4" "mac 10"; do
  "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" $P live 200 260 || STATUS=1
done
"$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" mac 1 late 200 260 || STATUS=1
for P in "60 1" "30 1" "60 4" "mac 4" "60 10" "30 10" "mac 10"; do
  "$OUT/air_smooth_probe" --shake "$HERE/data/raid-busy.json" $P live 290 330 || STATUS=1
done
"$OUT/air_smooth_probe" --shake "$HERE/data/raid-busy.json" mac 4 live 290 330 3 || STATUS=1
for R in bremen-b17 oahu midway-fleet; do
  for P in "60 1" "mac 4" "60 10"; do
    "$OUT/air_smooth_probe" --shake "$ROOT/Content/Data/AirBattles/$R.json" $P replay || STATUS=1
  done
done
if ! { AIR_SMOOTH_OLD=1 "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" 60 1 live 200 260;
       AIR_SMOOTH_OLD=1 "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" mac 4 live 200 260; } |
     cmp -s - "$HERE/data/shake-old-30899121.txt"; then
  echo "FAIL: flown as release 1.5 (AIR_SMOOTH_OLD=1), air_smooth_probe --shake's report is not data/shake-old-30899121.txt's"
  STATUS=1
fi
# The Battle rules card's "The raid camera holds steady" (flySteadyEye; Brian, 26 November 2026 04:11Z: "the bombing
# video camera is super choppy"), off until the Mac and the art director pass it: off, the scene's eye is the strategy
# camera as it was (AimEye's hard rings, 30899121's) and nothing else changes. With its row off (AIR_SMOOTH_EYE=0) the
# report must be, to the byte, what 2464374bb's own Battle files give with the scene's eye on the old camera
# (data/shake-eye-off-2464374bb.txt: the raid's minute at 60 frames x1 and on a Mac's frames x4, and Bremen's B-17s at
# 60 frames x1, replayed). Both cameras are measured either way; the game's own gates are in AimEye.
if ! { AIR_SMOOTH_EYE=0 "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" 60 1 live 200 260;
       AIR_SMOOTH_EYE=0 "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" mac 4 live 200 260;
       AIR_SMOOTH_EYE=0 "$OUT/air_smooth_probe" --shake "$ROOT/Content/Data/AirBattles/bremen-b17.json" 60 1 replay; } |
     cmp -s - "$HERE/data/shake-eye-off-2464374bb.txt"; then
  echo "FAIL: with the raid camera's row off (AIR_SMOOTH_EYE=0), air_smooth_probe --shake's report is not data/shake-eye-off-2464374bb.txt's"
  STATUS=1
fi
# The Battle rules card's "Each bomber keeps her place in her box" (flyBoxKeep; AirShow::Show::SetBoxKeep; Brian, 25
# November 13:27Z: "the bomber is shaking in the video too like the fighter"), off until the Mac and the art director
# pass it: off, the bombers fly as 30899121 flew them. The same three runs as above, to the byte:
#  - both rows off (AIR_SMOOTH_EYE=0 AIR_SMOOTH_BOX=0): data/shake-rows-off-30899121.txt, the report of this probe built
#    against 30899121's own Battle files (exported with git show; with only the two additions the probe needs to build
#    and measure, Check.SlotGap and AirDesk::Overview, which change no flying), AIR_PROBE_NO_BOX_ROW, Smooth on;
#  - the camera's row alone on (AIR_SMOOTH_BOX=0): data/shake-eye-only-30899121.txt, the same 30899121 build with the
#    scene's eye on AirDesk::Overview; (the bombers' row alone on is the check above, 2464374bb's flying)
#  - both on: data/shake-rows-on-2464374bb.txt, 2464374bb's own Battle files and probe.
# Both off, the He 111s weave (0.94 swings over 1 degree a bomber's air minute at x1, banked over 5 degrees 9.8% of the
# time) as 30899121's did; the bombers' row on, 0.00 and 0.0%.
if ! { AIR_SMOOTH_EYE=0 AIR_SMOOTH_BOX=0 "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" 60 1 live 200 260 || true;
       AIR_SMOOTH_EYE=0 AIR_SMOOTH_BOX=0 "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" mac 4 live 200 260 || true;
       AIR_SMOOTH_EYE=0 AIR_SMOOTH_BOX=0 "$OUT/air_smooth_probe" --shake "$ROOT/Content/Data/AirBattles/bremen-b17.json" 60 1 replay || true; } |
     cmp -s - "$HERE/data/shake-rows-off-30899121.txt"; then
  echo "FAIL: with both rows off (AIR_SMOOTH_EYE=0 AIR_SMOOTH_BOX=0), air_smooth_probe --shake's report is not data/shake-rows-off-30899121.txt's"
  STATUS=1
fi
if ! { AIR_SMOOTH_BOX=0 "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" 60 1 live 200 260 || true;
       AIR_SMOOTH_BOX=0 "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" mac 4 live 200 260 || true;
       AIR_SMOOTH_BOX=0 "$OUT/air_smooth_probe" --shake "$ROOT/Content/Data/AirBattles/bremen-b17.json" 60 1 replay || true; } |
     cmp -s - "$HERE/data/shake-eye-only-30899121.txt"; then
  echo "FAIL: with the bombers' row off (AIR_SMOOTH_BOX=0), air_smooth_probe --shake's report is not data/shake-eye-only-30899121.txt's"
  STATUS=1
fi
if ! { "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" 60 1 live 200 260;
       "$OUT/air_smooth_probe" --shake "$HERE/data/raid-steps.json" mac 4 live 200 260;
       "$OUT/air_smooth_probe" --shake "$ROOT/Content/Data/AirBattles/bremen-b17.json" 60 1 replay; } |
     cmp -s - "$HERE/data/shake-rows-on-2464374bb.txt"; then
  echo "FAIL: with both rows on, air_smooth_probe --shake's report is not data/shake-rows-on-2464374bb.txt's"
  STATUS=1
fi
# The strategy camera as the player sees it over Bremen's B-17s at 60 frames x1 (air_eye_probe: AimEye's strategy
# camera frame by frame), each of its frames (<prefix>-over.csv, 3.4 MB, held by its cksum) to the byte with each row
# off and on: both off, 30899121's (its own Battle files; its look jumped 20 px and more 92.6 times a real minute through the
# battle, 106.6 in the fight); the camera's row alone on, 30899121's flying under 2464374bb's camera (0.4); the bombers' row alone on,
# 2464374bb's flying under the old camera (78.6); both on, 2464374bb's (0.0).
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -DROWS -DBOXROW -DMARKS -I"$SRC" \
  "$HERE/air_eye_probe.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_eye_probe"
for EB in "0 0 3481327893 3448371 30899121" "1 0 731922131 3597993 30899121-under-2464374bb's-camera" "0 1 4087172811 3451605 2464374bb-under-the-old-camera" "1 1 4260831152 3605140 2464374bb"; do
  read -r E1 B1 K1 K2 WHO <<< "$EB"
  if ! CAMROW_EYE=$E1 CAMROW_BOX=$B1 "$OUT/air_eye_probe" rec "$ROOT/Content/Data/AirBattles/bremen-b17.json" "$OUT/eye-e${E1}b${B1}" x1 > "$OUT/eye-e${E1}b${B1}.txt"; then
    echo "FAIL: air_eye_probe did not run with CAMROW_EYE=$E1 CAMROW_BOX=$B1"
    STATUS=1
  elif [ "$(cksum < "$OUT/eye-e${E1}b${B1}-over.csv" | awk '{print $1, $2}')" != "$K1 $K2" ]; then
    echo "FAIL: with CAMROW_EYE=$E1 CAMROW_BOX=$B1, air_eye_probe's strategy camera is not $WHO's (cksum $(cksum < "$OUT/eye-e${E1}b${B1}-over.csv"), wanted $K1 $K2)"
    STATUS=1
  else
    echo "pass: with CAMROW_EYE=$E1 CAMROW_BOX=$B1, the strategy camera's every frame over Bremen is $WHO's"
  fi
done
# The player's zoom with the raid camera's row on (AAirBattle::Zoom, AimEye, AirDesk::Overview::ZoomEase): a notch of his
# wheel or trackpad glides there instead of landing whole in one frame (15% then); air_zoom_test.cpp says how it is judged.
$CXX ${AIR_SHOW_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/air_zoom_test.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirDesk.cpp" "$SRC/Battle/AirGround.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  -o "$OUT/air_zoom_test"
"$OUT/air_zoom_test" || STATUS=1
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
