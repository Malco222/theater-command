#!/bin/bash
# The sea free eye's cloud test (the sea free eye plan, section 5): builds the sea's side of the free camera
# (Battle/SeaFreeEyeGlue, the code the game runs), the camera's core (Battle/FreeEye), the sea show and the fleet's
# waves (plain C++, no engine) with Tools/sea_free_eye/sea_free_eye_test.cpp, on g++ and on clang++ (the Mac builds with
# clang and its -Wunreachable-code-aggressive), with the Mac's warnings as errors, and plays every sea recording through
# both builds. Each build runs twice and must print the same to the byte both times; the g++ and clang++ runs must
# print the same to the byte too (the Mac and the cloud must agree): any difference fails the run.
# Exits non-zero on a failed check, a build failure or a difference.
# Usage: Tools/sea_free_eye/run.sh [--quick] [recording.json ...]   (default: every recording Tools/sea_show/run.sh plays)
#   SEA_FREE_EYE_SRC=path/to/Source/TheaterCommand  tests another copy of the source (Tools/sea_free_eye/breakages.sh)
#   SEA_FREE_EYE_ONE=1  builds with g++ only and runs once (the breakages)
#   SEA_FREE_EYE_OUT    where the builds go (default ${TMPDIR:-/tmp}/sea_free_eye)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="${SEA_FREE_EYE_SRC:-$ROOT/Source/TheaterCommand}"
OUT="${SEA_FREE_EYE_OUT:-${TMPDIR:-/tmp}/sea_free_eye}"
mkdir -p "$OUT"
# (a shared machine: stop rather than fill its disk)
FREE_KB="$(df -Pk "$OUT" | awk 'NR == 2 { print $4 }')"
if [ "$FREE_KB" -lt 2097152 ]; then
  echo "sea_free_eye: under 2 GB free where the builds go ($OUT): not building"
  exit 2
fi
QUICK=()
if [ "${1:-}" = "--quick" ]; then QUICK=(--quick); shift; fi
# (a checkout without the recordings plays the rest: a glob that matches nothing drops out)
shopt -s nullglob
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/SeaBattles/*.json "$ROOT"/Tools/sea_show/stand-ins/*.json "$ROOT"/Tools/sea_show/recordings/*.json; fi
shopt -u nullglob

# The test, the glue, the core, the show (with what the show brings: the air feed's JSON reader and the air show behind
# it, the convoy looks' core) and the fleet's waves (the real gale's drawn surface and its SampleNear).
UNITS=("$HERE/sea_free_eye_test.cpp" "$SRC/Battle/SeaFreeEyeGlue.cpp" "$SRC/Battle/FreeEye.cpp"
  "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp"
  "$SRC/Battle/AirShowTroops.cpp" "$SRC/Battle/AirMarks.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp"
  "$SRC/Battle/ConvoyLooksCore.cpp" "$SRC/Battle/FleetWaves.cpp" "$SRC/Battle/SeaFace.cpp")
FLAGS=(-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -fno-exceptions -fno-rtti -I"$SRC")

# build <compiler> <name> [extra flags...]: one object a unit, kept by what the unit reads after preprocessing (so the
# breakages rebuild only what a breakage touched), linked to $OUT/<name>.
build() {
  local CC="$1" NAME="$2"; shift 2
  local CACHE="$OUT/objects-$NAME" OBJS=() U KEY
  mkdir -p "$CACHE"
  for U in "${UNITS[@]}"; do
    KEY="$( { echo "$CC $* ${FLAGS[*]}"; "$CC" "${FLAGS[@]}" "$@" -E "$U"; } | sha1sum | cut -c1-40)"
    if [ ! -s "$CACHE/$KEY.o" ]; then
      "$CC" "${FLAGS[@]}" "$@" -c "$U" -o "$CACHE/$KEY.tmp" && mv "$CACHE/$KEY.tmp" "$CACHE/$KEY.o" || return 1
    fi
    OBJS+=("$CACHE/$KEY.o")
  done
  "$CC" "${OBJS[@]}" -o "$OUT/$NAME"
}

CXX="${CXX:-g++}"
CLANG="$(command -v clang++ || ls /usr/bin/clang++-[0-9]* 2>/dev/null | sort -V | tail -n 1 || true)"
[ -n "${SEA_FREE_EYE_ONE:-}" ] && CLANG=""
STATUS=0
build "$CXX" sea_free_eye_test
if [ -n "$CLANG" ]; then build "$CLANG" sea_free_eye_test_clang -Wunreachable-code-aggressive; fi

echo "== g++"
"$OUT/sea_free_eye_test" "$SRC" "$HERE/director.sha" ${QUICK[@]+"${QUICK[@]}"} "$@" > "$OUT/gcc.txt" || STATUS=1
cat "$OUT/gcc.txt"
if [ -z "${SEA_FREE_EYE_ONE:-}" ]; then
  "$OUT/sea_free_eye_test" "$SRC" "$HERE/director.sha" ${QUICK[@]+"${QUICK[@]}"} "$@" > "$OUT/gcc-again.txt" || true
  if ! cmp -s "$OUT/gcc.txt" "$OUT/gcc-again.txt"; then
    echo "FAIL: g++'s two runs printed differently:"; diff "$OUT/gcc.txt" "$OUT/gcc-again.txt" | head -n 20 || true
    STATUS=1
  fi
fi
if [ -n "$CLANG" ]; then
  echo "== $CLANG"
  "$OUT/sea_free_eye_test_clang" "$SRC" "$HERE/director.sha" ${QUICK[@]+"${QUICK[@]}"} "$@" > "$OUT/clang.txt" || STATUS=1
  "$OUT/sea_free_eye_test_clang" "$SRC" "$HERE/director.sha" ${QUICK[@]+"${QUICK[@]}"} "$@" > "$OUT/clang-again.txt" || true
  if ! cmp -s "$OUT/clang.txt" "$OUT/clang-again.txt"; then
    echo "FAIL: clang++'s two runs printed differently:"; diff "$OUT/clang.txt" "$OUT/clang-again.txt" | head -n 20 || true
    STATUS=1
  fi
  if cmp -s "$OUT/gcc.txt" "$OUT/clang.txt"; then
    echo "g++ and clang++ printed the same to the byte ($(wc -l < "$OUT/clang.txt") lines)"
  else
    echo "FAIL: g++ and clang++ printed differently:"; diff "$OUT/gcc.txt" "$OUT/clang.txt" | head -n 40 || true
    STATUS=1
  fi
  tail -n 1 "$OUT/clang.txt"
elif [ -z "${SEA_FREE_EYE_ONE:-}" ]; then
  echo "(no clang++ here: built with g++ only)"
fi
exit $STATUS
