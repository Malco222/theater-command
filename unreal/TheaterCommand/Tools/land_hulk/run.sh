#!/bin/bash
# Old wrecks' cloud test (release 2, -TheaterLandOldWrecks): builds Battle/LandHulk (the game's own code, no engine)
# and this test with the Mac's strict warnings (-Wconversion, both compilers where they are here), the field, feed,
# sight, shell-hole and vehicle cores it reads with their own tests' flags, and lays the wrecks on every recording's
# field as the scene would: the same twice, none on a lane, a bank, a house, a wall, water, marsh or standing corn,
# clear of every recorded vehicle's way by 8 m, fewer as the months go, the campaign's key read whatever is odd in it.
# Paints placement_, look_, look_far_, look_side_, scorch_, lorry_ and track_<recording> into the out folder (PNG where
# python3 has Pillow, else PPM).
# Usage: Tools/land_hulk/run.sh [recording.json ...]   (default: every recording)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_HULK_OUT:-${TMPDIR:-/tmp}/land_hulk}"
mkdir -p "$OUT/obj"
STRICT="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -I$SRC"
CORE_FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I$SRC"
CORES="LandJson LandFeed LandPlay LandSight LandCrater LandKit Fates LandField LandHouse LandSteppe LandScape LandDesert LandMed LandTropic LandFarm LandSnow LandDry LandAsia LandAmerica LandPlain LandCity"
build() {
  local CXX="$1" TAG="$2" EXTRA="$3"
  local OBJS=()
  for C in $CORES; do
    $CXX $CORE_FLAGS $EXTRA -c "$SRC/Battle/$C.cpp" -o "$OUT/obj/$C.$TAG.o" &
    OBJS+=("$OUT/obj/$C.$TAG.o")
  done
  $CXX $STRICT $EXTRA -c "$SRC/Battle/LandHulk.cpp" -o "$OUT/obj/LandHulk.$TAG.o" &
  $CXX $STRICT $EXTRA -c "$HERE/land_hulk_test.cpp" -o "$OUT/obj/land_hulk_test.$TAG.o" &
  wait
  $CXX "$OUT/obj/land_hulk_test.$TAG.o" "$OUT/obj/LandHulk.$TAG.o" "${OBJS[@]}" -o "$OUT/land_hulk_test_$TAG"
}
build "${CXX:-g++}" gcc ""
# The Mac builds with clang and its -Wunreachable-code-aggressive: build with it too where it is here.
CLANG="$(command -v clang++ || true)"
if [ -n "$CLANG" ]; then
  build "$CLANG" clang "-Wunreachable-code-aggressive"
fi
if [ "$#" -eq 0 ]; then
  set --
  for F in "$ROOT"/Content/Data/LandBattles/*.json; do
    case "$F" in *armies.json) ;; *) set -- "$@" "$F" ;; esac
  done
fi
STATUS=0
"$OUT/land_hulk_test_gcc" "$ROOT/Content" "$OUT" "$@" | tee "$OUT/gcc.log" || STATUS=1
if [ -n "$CLANG" ]; then
  # (the clang build's own run: every wreck where the gcc build laid it, its lines the same but for their times)
  mkdir -p "$OUT/clang"
  "$OUT/land_hulk_test_clang" "$ROOT/Content" "$OUT/clang" "$@" > "$OUT/clang.log" 2>&1 || STATUS=1
  if diff <(grep -E '^ +[0-9]+  ' "$OUT/gcc.log") <(grep -E '^ +[0-9]+  ' "$OUT/clang.log") >/dev/null; then
    echo "clang: $(tail -n 1 "$OUT/clang.log"); every wreck laid where the gcc build laid it"
  else
    echo "FAIL: the clang build laid the wrecks otherwise than the gcc build"
    STATUS=1
  fi
fi
if python3 -c "import PIL" >/dev/null 2>&1; then
  python3 "$HERE/picture.py" "$OUT"
fi
echo "logs and pictures in $OUT"
exit $STATUS
