#!/bin/bash
# The sea's look from the rules' field: its cloud test (Battle/SeaLookField.{h,cpp}; the release 2 ocean plan, step 4).
# No engine: g++ and clang, -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror (clang also -Wshadow-all). Kept apart from
# run.sh so the ocean's own runner and its counts do not move.
#   - make_stand_ins: the three look stand-ins (Tools/sea_waves/stand-ins) written again from their recordings, byte for
#     byte;
#   - sea_field_test: every recording and stand-in the game and its tests play, read through SeaLookField and checked
#     against the show's own reader (SeaFeed) for its place, date and light; the stand-ins say what they were made to
#     say; every shape the rules may send.
# Usage: Tools/sea_waves/run_sea_field.sh   (SEA_FIELD_OUT: where it writes; default $TMPDIR/sea_field)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SEA_FIELD_OUT:-${TMPDIR:-/tmp}/sea_field}"
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
again() {
  rm -rf "$OUT/stand-ins"
  python3 -I -B "$HERE/stand-ins/make_stand_ins.py" "$OUT/stand-ins" || return 1
  local BAD=0
  for F in "$OUT"/stand-ins/*.json; do
    cmp "$F" "$HERE/stand-ins/$(basename "$F")" || BAD=1
  done
  return $BAD
}
step "make_stand_ins (written again, byte for byte Tools/sea_waves/stand-ins)" again
shopt -s nullglob
PLAYED=("$ROOT"/Content/Data/SeaBattles/*.json "$ROOT"/Tools/sea_show/recordings/*.json "$ROOT"/Tools/sea_show/stand-ins/*.json "$ROOT"/Tools/sea_waves/stand-ins/*.json)
shopt -u nullglob
for CXX in g++ clang++; do
  FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror"
  if [ "$CXX" = "clang++" ]; then FLAGS="$FLAGS -Wshadow-all"; fi
  if ! command -v "$CXX" > /dev/null; then
    echo "== sea_field_test ($CXX): not installed here"
    continue
  fi
  rm -f "$OUT/sea_field_test_$CXX"
  step "sea_field_test (build, $CXX)" $CXX $FLAGS -I"$SRC" "$HERE/sea_field_test.cpp" "$SRC/Battle/SeaLookField.cpp" "$SRC/Battle/SeaNightSky.cpp" "$SRC/Battle/SeaFeed.cpp" \
    "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" \
    "$SRC/Effects/TheaterFxCore.cpp" -o "$OUT/sea_field_test_$CXX"
  if [ -x "$OUT/sea_field_test_$CXX" ]; then
    step "sea_field_test ($CXX, ${#PLAYED[@]} recordings and stand-ins)" "$OUT/sea_field_test_$CXX" "${PLAYED[@]}"
  fi
done
echo "written in $OUT"
if [ "$STATUS" -eq 0 ]; then
  echo "SEA FIELD: ALL PASSED"
else
  echo "SEA FIELD: FAILED: ${FAILED[*]}"
fi
exit $STATUS
