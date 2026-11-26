#!/bin/bash
# THE FLEET'S CLOUDS AND WEATHER, the runner (the release 2 ocean plan, step 13). Beside Tools/sea_waves/run.sh, which
# keeps its own steps and counts: nothing here touches them.
#   - weather_days: the two stand-in weather days (Tools/sea_waves/stand-ins) written again from the recordings they
#     come from, byte for byte the committed two, so the day a Mac plays is the day this test holds;
#   - sea_sky_test, built with g++ and with clang (clang also -Wshadow-all): Battle/SeaSky.{h,cpp} held to the plan -
#     the five switches as the scene takes them, the day from the field's sky (and from each stand-in day on disk), the
#     laying through AirSky with the field's own wind, the dome's mapping and its painting from the shot's own eye
#     (against a painter with no bounding box, byte for byte), the cloud shade, the far rain and its squall, the deck's
#     far sheet;
#   - sky_dome_check: M_SkyDome and M_DeckShade as Tools/build_content.py builds them, through the stand-in Unreal
#     module, and every parameter Battle/SeaWeather.cpp sets on each;
#   - room_scan: the key lamp's light function, which the clouds' shade takes, given back when the scene ends;
#   - switches_scan: the five weather switches in the plan's table, the "Sea look:" line and the game's source;
#   - graph_pin: the 15 pinned materials unmoved by the new one.
# No engine: C++ with -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror, Python with python3 -I -B.
# Usage: Tools/sea_waves/run_weather.sh   (SEA_WEATHER_OUT: where it writes; default $TMPDIR/sea_weather)
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
OUT="${SEA_WEATHER_OUT:-${TMPDIR:-/tmp}/sea_weather}"
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

# The core's own files: the sky, the air sky it lays through, and what those two need to link.
CORE=("$SRC/Battle/SeaSky.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" ${AIR_TROOPS[@]+"${AIR_TROOPS[@]}"} "$SRC/Battle/AirDesk.cpp" \
  "$SRC/Battle/AirGround.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Effects/TheaterFxCore.cpp")
FLAGS=(-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC")

# The stand-in weather day, written again and held to the committed one (Samar's squall dawn is make_stand_ins.py's,
# held by run_sea_field.sh).
DAYS="$OUT/stand-ins"
rm -rf "$DAYS"
days_again() {
  python3 -I -B "$HERE/stand-ins/weather_days.py" "$DAYS" || return 1
  local OK=0
  local DAY
  for DAY in north-atlantic-deck.json; do
    if cmp "$DAYS/$DAY" "$HERE/stand-ins/$DAY"; then
      echo "$DAY: byte for byte the committed day"
    else
      echo "$DAY: written again, it is not the committed day"
      OK=1
    fi
  done
  return $OK
}
step "weather_days (the stand-in day, written again)" days_again

for CC in "${SEA_WEATHER_CXX:-g++}" "${SEA_WEATHER_CLANG:-clang++}"; do
  if ! command -v "$CC" > /dev/null 2>&1; then
    echo "== sea_sky_test ($CC): no such compiler, skipped"
    continue
  fi
  MORE=()
  case "$CC" in *clang*) MORE=(-Wshadow-all);; esac
  BIN="$OUT/sea_sky_test.$(basename "$CC")"
  rm -f "$BIN"
  step "sea_sky_test (build, $CC)" "$CC" "${FLAGS[@]}" "${MORE[@]}" ${SEA_WEATHER_FLAGS:-} "$HERE/sea_sky_test.cpp" "${CORE[@]}" -pthread -o "$BIN"
  if [ -x "$BIN" ]; then
    sky_test() {
      "$1" "$OUT" 2>&1 | tee "$OUT/sea_sky_test.$(basename "$2").log"
      return "${PIPESTATUS[0]}"
    }
    step "sea_sky_test ($CC)" sky_test "$BIN" "$CC"
  fi
done

step "sky_dome_check (M_SkyDome and M_DeckShade, and what the game sets on them)" python3 -I -B "$HERE/sky_dome_check.py"
step "room_scan (the key lamp's light function given back)" python3 -I -B "$HERE/room_scan.py"
step "switches_scan (the five weather switches)" python3 -I -B "$HERE/switches_scan.py"
step "graph_pin (the 15 pinned materials unmoved)" python3 -I -B "$HERE/graph_pin.py" --out "$OUT"
echo "written in $OUT"
if [ "$STATUS" -eq 0 ]; then
  echo "SEA WEATHER: ALL PASSED"
else
  echo "SEA WEATHER: FAILED: ${FAILED[*]}"
fi
exit $STATUS
