#!/bin/bash
# Night fighting at sea, its looks: the cloud test for Battle/SeaNightCore (the release 2 sea's -TheaterSeaNightFight=,
# off by default; the look bible's S7, "Star shells hang and light the water; gun flashes light the hulls; dark water
# still shows its waves"). No engine: the core and the test are plain C++20, built with g++ and with clang++ (both, since
# the Mac builds with clang), -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror (clang also -Wshadow-all).
# It holds the core to its brief:
#   - the night itself: the night share read from the recording's light (ConvoyLook::SkyLit), and night only with the sun
#     6 degrees under (nothing at all by day or in twilight);
#   - the searchlights by navy and by size (the USN's 24- and 36-inch, the IJN's 110 and 150 cm, the RN's, the
#     Kriegsmarine's), their reach, their spread and their mirrors;
#   - made-up fleets and convoys: beams only at night, never one laid on a ship not in sight for that side (a hidden boat
#     stays hidden, and a sweep runs the same with her there or not), at most MostBeams beams and two on one boat, a hold
#     taking a sweep's slot, the first beam's delay and a hold's length, the escorts holding a surfaced boat;
#   - the fleet's star shells: beyond the target along the line of fire, 400-600 m up, burning about 48 s and sinking at
#     3 m/s, at most MostStarsPerSide burning a side, none inside a convoy (those are the rules' own) and none by day;
#   - the battles' own recordings and the night stand-in (Tools/sea_waves/stand-ins/night-cruisers.json) played through
#     the show itself (Battle/SeaShow, the player's side as the scene takes it): the same rules frame by frame, nothing
#     at all where the recording has no night, and the same beams and star shells in two plays (one deterministic stream);
#   - a frame's cost for 40 ships, every one firing, under 50 microseconds on the mean;
#   - the effects kit's flash lamps for the naval guns, measured as the scene fires them today (a finding for the
#     effects thread, not a change: Content/Effects/battle-fx.json is read, never written).
# The two existing runners (Tools/sea_show/run.sh, Tools/convoy_looks/run.sh) keep their baseline counts: the switch is
# off by default and nothing the show decides reads the night.
# Usage: Tools/sea_waves/run_night.sh   (SEA_NIGHT_OUT: where the test writes; default $TMPDIR/sea_night)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SEA_NIGHT_OUT:-${TMPDIR:-/tmp}/sea_night}"
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

# (the core, the show that reads the recordings, and the effects kit's core for the gun flashes' lamps)
SOURCES=("$HERE/sea_night_test.cpp"
  "$SRC/Battle/SeaNightCore.cpp" "$SRC/Battle/ConvoyLooksCore.cpp"
  "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp"
  "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirSky.cpp"
  "$SRC/Effects/TheaterFxCore.cpp")
WARN=(-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror)

rm -f "$OUT/sea_night_test" "$OUT/sea_night_test_clang"
# (the cloud's four cores are shared: one build at a time, out of the way)
step "sea_night_test (build, g++)" nice -n 10 "${CXX:-g++}" ${SEA_NIGHT_FLAGS:-} "${WARN[@]}" -I"$SRC" "${SOURCES[@]}" -o "$OUT/sea_night_test"
if command -v "${CLANGXX:-clang++}" > /dev/null 2>&1; then
  step "sea_night_test (build, clang++ -Wshadow-all: the Mac's compiler)" nice -n 10 "${CLANGXX:-clang++}" ${SEA_NIGHT_FLAGS:-} "${WARN[@]}" -Wshadow-all -I"$SRC" "${SOURCES[@]}" -o "$OUT/sea_night_test_clang"
else
  echo "== sea_night_test (build, clang++): no clang++ here, skipped (the Mac pre-check builds with it)"
fi

# (the night stand-in first, then the battles' own recordings and the sea show's, as the sea show's runner finds them)
shopt -s nullglob
PLAYED=("$ROOT"/Tools/sea_waves/stand-ins/night-cruisers.json
  "$ROOT"/Content/Data/SeaBattles/*.json "$ROOT"/Tools/sea_show/stand-ins/*.json "$ROOT"/Tools/sea_show/recordings/*.json)
shopt -u nullglob
night_test() {
  local BIN="$1"
  local LOG="$2"
  if [ ! -x "$BIN" ]; then
    echo "not built: $BIN"
    return 1
  fi
  if [ "${#PLAYED[@]}" -lt 2 ]; then
    echo "no recordings found (Content/Data/SeaBattles, Tools/sea_show)"
    return 1
  fi
  nice -n 10 "$BIN" "$SRC" "$ROOT/Content" "${PLAYED[@]}" 2>&1 | tee "$LOG"
  return "${PIPESTATUS[0]}"
}
step "sea_night_test (${#PLAYED[@]} recordings and stand-ins, g++)" night_test "$OUT/sea_night_test" "$OUT/sea_night_test.log"
if [ -x "$OUT/sea_night_test_clang" ]; then
  step "sea_night_test (the same, built by clang++)" night_test "$OUT/sea_night_test_clang" "$OUT/sea_night_test_clang.log"
fi

echo "written in $OUT"
if [ "$STATUS" -eq 0 ]; then
  echo "SEA NIGHT: ALL PASSED"
else
  echo "SEA NIGHT: FAILED: ${FAILED[*]}"
fi
exit $STATUS
