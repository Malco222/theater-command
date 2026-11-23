#!/bin/bash
# The lorried men's seats' cloud test (release 2; Brian, 23 November 14:10Z: "troops are stuck under the truck"): the
# rules fight a campaign ticket with lorried infantry live (deltas.ts, LAND_LIVE_TICKET), and the game's own vehicles and
# men (LandKit, LandMen; no engine) play it whole with the carriers' benches handed to the men as the scene hands them
# (land_live_seat_test.cpp): every man aboard sits in a carrier's bed, none on the ground under her, none on another.
# Usage: Tools/land_live/seat.sh <ticket.json> [seconds]   (a {ticket, live} file with lorried blocks)
# LAND_LIVE_FIGHT=1 fights it again (set it whenever the rules changed).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
REPO="$(cd "$ROOT/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_LIVE_OUT:-${TMPDIR:-/tmp}/land_live}"
TICKET="${1:?usage: seat.sh <ticket.json> [seconds]}"
mkdir -p "$OUT"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I$SRC"
CORES="$SRC/Battle/LandKit.cpp $SRC/Battle/LandMen.cpp $SRC/Battle/Fates.cpp"
if command -v clang++ >/dev/null 2>&1; then
  clang++ $FLAGS -Wunreachable-code-aggressive "$HERE/land_live_seat_test.cpp" $CORES -o "$OUT/land_live_seat_test_clang"
fi
CXX="${CXX:-g++}"
$CXX $FLAGS "$HERE/land_live_seat_test.cpp" $CORES -o "$OUT/land_live_seat_test"
if [ ! -f "$OUT/seat/whole.json" ] || [ -n "${LAND_LIVE_FIGHT:-}" ]; then
  "$REPO/node_modules/.bin/esbuild" "$HERE/deltas.ts" --bundle --platform=node --format=esm --outfile="$OUT/land-live-deltas.mjs" --log-level=warning
  LAND_LIVE_TICKET="$TICKET" node "$OUT/land-live-deltas.mjs" "$OUT/seat" ticket 0 att 0.5
fi
"$OUT/land_live_seat_test" "$ROOT/Content" "$OUT/seat" "${2:-120}"
