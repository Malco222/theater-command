#!/bin/bash
# The night sky over the sea's cloud test (Battle/SeaNightSky.{h,cpp}, Battle/SeaNightStars.inl; look bible S7; the
# release 2 ocean plan, step 11(f)). No engine: g++ and clang, -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror (clang also
# -Wshadow-all). Kept apart from run.sh so the ocean's own runner and its counts do not move.
#   - make_star_table: the star table written again from the catalogue, byte for byte, when the catalogue is given
#     (NIGHT_SKY_CATALOGUE=path to bsc5-short.json; skipped, and said, when it is not);
#   - night_sky_test: the catalogue, the clock, precession, the sun, the hour of night, real stars where they stand, how
#     many are laid by moon, twilight and cloud, the moon's face, the mesh and its cost.
# Usage: Tools/sea_waves/run_night_sky.sh   (NIGHT_SKY_OUT: where it writes; default $TMPDIR/night_sky)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${NIGHT_SKY_OUT:-${TMPDIR:-/tmp}/night_sky}"
mkdir -p "$OUT"
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
if [ -n "${NIGHT_SKY_CATALOGUE:-}" ]; then
  table_again() {
    python3 -I -B "$HERE/make_star_table.py" "$NIGHT_SKY_CATALOGUE" "$OUT/SeaNightStars.inl" || return 1
    cmp "$OUT/SeaNightStars.inl" "$SRC/Battle/SeaNightStars.inl"
  }
  step "make_star_table (written again, byte for byte Battle/SeaNightStars.inl)" table_again
else
  echo "== make_star_table: skipped (NIGHT_SKY_CATALOGUE not given)"
fi
for CXX in g++ clang++; do
  FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror"
  [ "$CXX" = clang++ ] && FLAGS="$FLAGS -Wshadow-all"
  rm -f "$OUT/night_sky_test_$CXX"
  step "night_sky_test (build, $CXX)" $CXX $FLAGS -I"$SRC" "$HERE/night_sky_test.cpp" "$SRC/Battle/SeaNightSky.cpp" -o "$OUT/night_sky_test_$CXX"
  if [ -x "$OUT/night_sky_test_$CXX" ]; then
    step "night_sky_test ($CXX)" "$OUT/night_sky_test_$CXX"
  fi
done
echo "written in $OUT"
if [ "$STATUS" -eq 0 ]; then
  echo "NIGHT SKY: ALL PASSED"
else
  echo "NIGHT SKY: FAILED: ${FAILED[*]}"
fi
exit $STATUS
