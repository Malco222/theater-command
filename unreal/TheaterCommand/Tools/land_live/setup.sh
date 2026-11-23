#!/bin/bash
# The setup step as the general's walk plays it, under the memory checkers (release 2): a live Normandy battle opened
# with his side deploying, two of his units set down in the woods (each place a reset piece), the battle begun and the
# fight after it (setup-deltas.ts), played frame by frame through the game's own cores as the scene plays them
# (land_live_setup_test.cpp); and the scene's crash locator, crashed on purpose (land_fell_test.cpp).
# LAND_SETUP_PLACES="block:x:y,..." sets the units and their woods (the walk's own by default).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
REPO="$(cd "$ROOT/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
B="$SRC/Battle"
OUT="${LAND_LIVE_OUT:-${TMPDIR:-/tmp}/land_live}"
mkdir -p "$OUT"
STATUS=0
"$REPO/node_modules/.bin/esbuild" "$HERE/setup-deltas.ts" --bundle --platform=node --format=esm --outfile="$OUT/land-live-setup.mjs" --log-level=warning
node "$OUT/land-live-setup.mjs" "$OUT/normandy-walk" normandy-1944 11 90
CXX="${CXX:-g++}"
# (the address and undefined-behaviour checkers where the compiler has them, and the library's own bounds checks)
CHECKS="-fsanitize=address,undefined -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS"
echo 'int main(){return 0;}' > "$OUT/asan-probe.cpp"
$CXX $CHECKS "$OUT/asan-probe.cpp" -o "$OUT/asan-probe" 2>/dev/null || CHECKS="-fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS"
CORES="$B/LandJson.cpp $B/LandFeed.cpp $B/LandPlay.cpp $B/LandMen.cpp $B/LandCommand.cpp $B/LandKit.cpp $B/Fates.cpp $B/LandEye.cpp $B/LandGeneralEye.cpp $B/LandDesk.cpp $B/LandField.cpp $B/LandSteppe.cpp $B/LandSight.cpp $B/LandCrater.cpp $B/LandSound.cpp"
$CXX -std=c++20 -O1 -g $CHECKS -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_live_setup_test.cpp" $CORES -o "$OUT/land_live_setup_test"
"$OUT/land_live_setup_test" "$OUT/normandy-walk" "$ROOT/Content" || STATUS=1
# (the crash locator: no checkers, which keep crash handlers of their own)
$CXX -std=c++20 -O1 -g -fno-omit-frame-pointer -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_fell_test.cpp" "$B/LandFell.cpp" -o "$OUT/land_fell_test"
mkdir -p "$OUT/fell"
"$OUT/land_fell_test" "$OUT/fell" 2>/dev/null | tail -1 || STATUS=1
if command -v clang++ >/dev/null 2>&1; then
  clang++ -std=c++20 -O1 -g -fno-omit-frame-pointer -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -I"$SRC" "$HERE/land_fell_test.cpp" "$B/LandFell.cpp" -o "$OUT/land_fell_test_clang"
  "$OUT/land_fell_test_clang" "$OUT/fell" 2>/dev/null | tail -1 || STATUS=1
fi
if [ "$STATUS" = 0 ]; then echo PASS; else echo FAIL; fi
exit $STATUS
