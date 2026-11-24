#!/bin/bash
# The gore falls' cloud test (Effects, release 2, -TheaterLandGore): builds Battle/LandMen (the game's own code, no engine)
# and plays a small battle of its own, shell, mortar and rifle kills at known distances from their bursts, with the gore
# twins off and on, then a second of 384 men killed all round their bursts and a third of 240 killed by shells bursting on
# them from guns all round (the bursts' and the guns' sides, the limbs away from them, the big throw a field gun's or a
# howitzer's alone), a fourth of 210 men of each twin in the twins folder killed standing, running or kneeling with the
# burst or the gun 31 to 49 degrees round ahead on his left or his right (the band), and a sweep of every twin's gore
# falls (LandMen::GoreFalls) every 3 degrees all round a man. See land_gore_test.cpp for what it checks.
# Usage: Tools/land_gore/run.sh [twins folder] [out folder]  (twins: Content/Models/Soldiers unless given; with an out
# folder it paints gore_falls.ppm there)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_GORE_OUT:-${TMPDIR:-/tmp}/land_gore}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
FLAGS="${LAND_GORE_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I$SRC -I$ROOT/Tools/land_show"
$CXX $FLAGS -pthread "$HERE/land_gore_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_gore_test"
if command -v clang++ >/dev/null 2>&1; then
  clang++ ${LAND_GORE_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -I"$SRC" -I"$ROOT/Tools/land_show" -pthread "$HERE/land_gore_test.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/land_gore_test_clang"
fi
"$OUT/land_gore_test" "$ROOT/Content/Models/Soldiers" "${1:-$ROOT/Content/Models/Soldiers}" "$ROOT/Content/Data/LandBattles/armies.json" ${2:+"$2"}
