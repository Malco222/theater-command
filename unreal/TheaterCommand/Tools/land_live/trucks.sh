#!/bin/bash
# Men in lorries' cloud test (release 1.5, the Battle rules card's row "trucks"): fights a battle with lorried blocks
# twice through the rules (Tools/land_live/trucks-deltas.ts, the row on and off), then plays each read whole with the
# game's own vehicles and men, the lorried men seated and getting down as the scene shows them (land_live_trucks_test.cpp,
# LandBench::RideFor). Builds with g++ and, where it is here, clang with the Mac's strictest warning.
# Usage: Tools/land_live/trucks.sh <ticket.json> [seconds]   (only the Poznan ticket, joins/poznan-holder, has lorries)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
REPO="$(cd "$ROOT/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_LIVE_OUT:-${TMPDIR:-/tmp}/land_live}/trucks"
TICKET="${1:?usage: trucks.sh <ticket.json> [seconds]}"
UPTO="${2:-1000000}"
mkdir -p "$OUT/on" "$OUT/off"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I$SRC"
CORES="$SRC/Battle/LandKit.cpp $SRC/Battle/LandMen.cpp $SRC/Battle/Fates.cpp"
${CXX:-g++} $FLAGS "$HERE/land_live_trucks_test.cpp" $CORES -o "$OUT/land_live_trucks_test"
if command -v clang++ >/dev/null 2>&1; then
  clang++ $FLAGS -Wunreachable-code-aggressive "$HERE/land_live_trucks_test.cpp" $CORES -o "$OUT/land_live_trucks_test_clang"
fi
cd "$REPO"
npx esbuild "$HERE/trucks-deltas.ts" --bundle --platform=node --format=esm --outfile="$OUT/trucks-deltas.mjs" --log-level=warning
LAND_LIVE_TICKET="$TICKET" node "$OUT/trucks-deltas.mjs" "$OUT/off"
LAND_LIVE_TICKET="$TICKET" LAND_LIVE_TRUCKS=1 node "$OUT/trucks-deltas.mjs" "$OUT/on"
STATUS=0
"$OUT/land_live_trucks_test" "$ROOT/Content" "$OUT/on" "$UPTO" || STATUS=1
"$OUT/land_live_trucks_test" "$ROOT/Content" "$OUT/off" "$UPTO" off || STATUS=1
exit $STATUS
