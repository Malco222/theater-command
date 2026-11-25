#!/bin/bash
# Heavy weather's cloud test (the look bible's S6: "Spray sheets over the bow from force 6; ice on rails and guns in the
# Arctic and the winter Atlantic"; the release 2 ocean plan, steps 6(b) and 12(b), and step 6 for the ice). Built beside
# Tools/sea_waves/run.sh, which it does not touch:
#   - spray_test (Battle/FleetSpray): the plan's own rules (none at Beaufort 3; the thresholds by class; green water and
#     spindrift only from 7, and only under the gale; at most 80 live puffs of the kit's 3,000; only the 4 ships nearest the
#     eye; head seas throwing more than following; the sheet to a destroyer's bridge, its fan, its fall; the 1.5 s rest;
#     the same spray every play; a frame's cost), then every recorded action, stand-in and real night through the show
#     itself over a wind sweep (her own wind, Beaufort 6, and Beaufort 8 with the gale);
#   - ice_test (Battle/ShipIce): Overland's predictor and NOAA's classes, the climatology (nothing in the tropics, the
#     Mediterranean or the northern summer; heavy on a January Murmansk run, in the Denmark Strait and on the cold western
#     side of the Atlantic), the glaze on the battle modeller's own American destroyer (only up, forward and windward
#     surfaces, her sides below the deck bare, the colours within the glaze, the alpha never carried into the wake's band),
#     a frost never set leaving her byte for byte as she was, the same ice every time, and the cost at lay time;
#   - switches_scan: the plan's switch table against the game's own (-TheaterSeaSpray= now built, -TheaterSeaIce= new);
#   - measure_test: the "Sea look:" line with them built.
# Both the C++ builds are made with g++ and with clang++ where it is there, at -std=c++20 -O2 -Wall -Wextra -Wshadow
# -Werror (clang also -Wshadow-all), and run from each.
# Usage: Tools/sea_waves/run_storm.sh [recording.json ...]   (STORM_OUT: where it builds and writes; default
# $TMPDIR/sea_storm. SPRAY_ONLY=1 or ICE_ONLY=1 for one of them alone.)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
# (release 2's air troops join: AirFeed.cpp and AirShow.cpp call AirMarks and the column code, so where those files are
# present they are linked beside them, as sea_show's run.sh does; a branch from before that join has neither)
AIR_TROOPS=()
for AIR_PART in AirShowTroops.cpp AirMarks.cpp; do
  if [ -f "$SRC/Battle/$AIR_PART" ]; then AIR_TROOPS+=("$SRC/Battle/$AIR_PART"); fi
done
OUT="${STORM_OUT:-${TMPDIR:-/tmp}/sea_storm}"
mkdir -p "$OUT"
cd "$ROOT" || exit 1
STATUS=0
FAILED=()
step() {
  local NAME="$1"
  shift
  echo "== $NAME"
  if "$@"; then
    echo "== $NAME: passed"
  else
    echo "== $NAME: FAILED"
    STATUS=1
    FAILED+=("$NAME")
  fi
}

# (the recordings, the stand-ins and the convoy battle's real nights, as the sea show's runner takes them; a checkout
# without the real nights plays the rest, a glob that matches nothing dropping out)
shopt -s nullglob
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/SeaBattles/*.json "$ROOT"/Tools/sea_show/stand-ins/*.json "$ROOT"/Tools/sea_show/recordings/*.json; fi
shopt -u nullglob

SHOW=("$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" ${AIR_TROOPS[@]+"${AIR_TROOPS[@]}"} "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp")

# g++ and clang++ (clang with -Wshadow-all too), each building and running both tests.
for PAIR in "gnu:${CXX:-g++}" "clang:${CLANGXX:-clang++}"; do
  NAME="${PAIR%%:*}"
  COMP="${PAIR#*:}"
  if ! command -v "$COMP" > /dev/null 2>&1; then
    echo "== $NAME ($COMP): not on this machine, left out"
    continue
  fi
  FLAGS=(-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror)
  if [ "$NAME" = "clang" ]; then FLAGS+=(-Wshadow-all); fi
  if [ "${ICE_ONLY:-0}" != "1" ]; then
    rm -f "$OUT/spray_test.$NAME"
    step "spray_test (build, $NAME)" "$COMP" ${STORM_FLAGS:-} "${FLAGS[@]}" -I"$SRC" "$HERE/spray_test.cpp" "$SRC/Battle/FleetSpray.cpp" "${SHOW[@]}" -o "$OUT/spray_test.$NAME"
    if [ -x "$OUT/spray_test.$NAME" ]; then
      step "spray_test ($NAME)" "$OUT/spray_test.$NAME" "$@"
    fi
  fi
  if [ "${SPRAY_ONLY:-0}" != "1" ]; then
    rm -f "$OUT/ice_test.$NAME"
    step "ice_test (build, $NAME)" "$COMP" ${STORM_FLAGS:-} "${FLAGS[@]}" -I"$SRC" "$HERE/ice_test.cpp" "$SRC/Battle/ShipIce.cpp" -o "$OUT/ice_test.$NAME"
    if [ -x "$OUT/ice_test.$NAME" ]; then
      step "ice_test ($NAME)" "$OUT/ice_test.$NAME" "$ROOT/Content/Models/Ships"
    fi
  fi
done

# The switches: the plan's table against the game's own, and the "Sea look:" line with the two built (step 0's own tools,
# run here too so a switch added in this step is held to them without waiting for Tools/sea_waves/run.sh).
step "switches_scan (self-test)" python3 -I -B "$HERE/switches_scan.py" --self-test
step "switches_scan" python3 -I -B "$HERE/switches_scan.py"
rm -f "$OUT/measure_test" "$OUT/measure_lines.txt"
step "measure_test (build)" "${CXX:-g++}" ${STORM_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/measure_test.cpp" -o "$OUT/measure_test"
if [ -x "$OUT/measure_test" ]; then
  step "measure_test" "$OUT/measure_test" "$SRC" "$OUT"
fi

echo
if [ "$STATUS" -eq 0 ]; then
  echo "run_storm: every step passed (its logs and builds in $OUT)"
else
  echo "run_storm: FAILED: ${FAILED[*]}"
fi
exit $STATUS
