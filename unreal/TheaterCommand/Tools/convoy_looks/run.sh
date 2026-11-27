#!/bin/bash
# The submarine scene's looks' cloud test (ruling 241, D1): builds Battle/ConvoyLooksCore (the game's own code, no
# engine) and checks the boat rows against the stage's code-built model (read from its source), the boat's states and
# her dive (never a pop), the depth charges' domes and columns by depth, a pattern in the water before its bursts, a
# starshell's fall and light, a crushed boat's boils, a boat blown riding high, a rescuer's nets (and the rescue against the
# sea scene's wreck model, read from its source); then plays the sea show's stand-in convoy night and wolf pack's
# nights (two boats strike; the same with the captor abeam; one boat abandoned and scuttled, none striking; read only), and
# the convoy battle's real nights (the naval rules' wolf pack against a convoy, D2: Tools/sea_show/recordings), through the
# show itself and checks the looks, the model the stage draws and the scene's men in the water and on a struck boat's
# casing against it; and the deck guns' crews (Battle/DeckCrewCore) against the sailor files, the guns' models and the
# show's nights.
# Usage: Tools/convoy_looks/run.sh [recording.json ...]   (default: the stand-in convoy night, the wolf pack's three and the
# real nights; CONVOY_LOOKS_STORY=1 prints a dive, the bursts and a starshell)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${CONVOY_LOOKS_OUT:-${TMPDIR:-/tmp}/convoy_looks}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${CONVOY_LOOKS_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/convoy_looks_test.cpp" "$SRC/Battle/ConvoyLooksCore.cpp" "$SRC/Battle/Fates.cpp" \
  "$SRC/Battle/DeckCrewCore.cpp" "$SRC/Battle/SeaSailors.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/convoy_looks_test"
"$OUT/convoy_looks_test" "$SRC"
# (against the show: the game's own SeaShow and its feed, as the sea show's cloud test builds them)
$CXX ${CONVOY_LOOKS_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/convoy_looks_show.cpp" "$SRC/Battle/ConvoyLooksCore.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" \
  "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp" \
  "$SRC/Battle/DeckCrewCore.cpp" "$SRC/Battle/SeaSailors.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/convoy_looks_show"
# (the gun crews against the show: the sailor files the scene reads, from the game's Content)
export CONVOY_LOOKS_CONTENT="$ROOT/Content"
# (the looks each real night must tell, the design director's ruling of 03:40Z, 26 November: Tools/convoy_looks/night_looks.txt,
# a night and its looks a line; convoy_looks_show holds each night in it to its line, draws every look it tells every time it
# is told, and asks each look told on one real night played at the least; the table not read fails the run. A real night is
# also every recording played from the real nights' folder, Tools/sea_show/recordings, in the table or not: one with no line
# there fails, and each counts across the real nights, so the table emptied or cut short never stops them being asked)
export CONVOY_LOOKS_NIGHTS="${CONVOY_LOOKS_NIGHTS:-$HERE/night_looks.txt}"
export CONVOY_LOOKS_REAL="$ROOT/Tools/sea_show/recordings"
# (a checkout without the real nights' recordings plays the rest: a glob that matches nothing drops out)
shopt -s nullglob
STATUS=0
if [ "$#" -eq 0 ]; then
  set -- "$ROOT/Tools/sea_show/stand-ins/convoy-night.json" "$ROOT/Tools/sea_show/stand-ins/wolf-pack-surrender.json" "$ROOT/Tools/sea_show/stand-ins/wolf-pack-abeam.json" \
    "$ROOT/Tools/sea_show/stand-ins/wolf-pack-abandon.json" "$ROOT"/Tools/sea_show/recordings/*.json
  # (where the real nights' recordings are here at all, every night of the table is among them, so none of its lines goes
  # unasked for a name that matches no recording; a recording there with no line in the table convoy_looks_show fails)
  REAL=("$ROOT"/Tools/sea_show/recordings/*.json)
  if [ "${#REAL[@]}" -gt 0 ] && [ -r "$CONVOY_LOOKS_NIGHTS" ]; then
    while IFS=$'\t' read -r NIGHT _; do
      case "$NIGHT" in '' | '#'*) continue ;; esac
      if [ ! -f "$ROOT/Tools/sea_show/recordings/$NIGHT.json" ]; then
        echo "FAIL: the looks table's night $NIGHT has no recording in Tools/sea_show/recordings"
        STATUS=1
      fi
    done < "$CONVOY_LOOKS_NIGHTS"
  fi
fi
shopt -u nullglob
"$OUT/convoy_looks_show" "$@" || STATUS=1
exit $STATUS
