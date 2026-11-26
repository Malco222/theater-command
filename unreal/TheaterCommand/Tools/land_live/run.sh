#!/bin/bash
# The live ground battle's cloud test (release 2): the rules fight a battle live (src/battle/land/live.ts, as the 3D game
# will ask for it), deltas.ts writes what the game hears, and the game's own LandFeed grows the battle from it
# (land_live_test.cpp). Usage: Tools/land_live/run.sh [key] [seed] [me] [seconds a tick]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
REPO="$(cd "$ROOT/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_LIVE_OUT:-${TMPDIR:-/tmp}/land_live}"
mkdir -p "$OUT"
"$REPO/node_modules/.bin/esbuild" "$HERE/deltas.ts" --bundle --platform=node --format=esm --outfile="$OUT/land-live-deltas.mjs" --log-level=warning
CORES="$SRC/Battle/LandJson.cpp $SRC/Battle/LandFeed.cpp $SRC/Battle/LandPlay.cpp"
CXX="${CXX:-g++}"
$CXX -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_live_test.cpp" $CORES -o "$OUT/land_live_test"
if command -v clang++ >/dev/null 2>&1; then
  clang++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -I"$SRC" "$HERE/land_live_test.cpp" $CORES -o "$OUT/land_live_test_clang"
fi
STATUS=0
run() {
  if [ ! -f "$OUT/$1/deltas.jsonl" ] || [ -n "${LAND_LIVE_FIGHT:-}" ]; then node "$OUT/land-live-deltas.mjs" "$OUT/$1" "${@:2}"; fi
  "$OUT/land_live_test" "$OUT/$1" || STATUS=1
}
if [ "$#" -gt 0 ]; then run one "$@"; else
  run normandy normandy-1944 11 att 0.5 orders
  run normandy-film normandy-1944 11 none 1.7
fi
exit $STATUS
