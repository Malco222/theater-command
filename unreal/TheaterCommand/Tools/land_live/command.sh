#!/bin/bash
# The general's clipboard's cloud test (release 2): LandCommand reads a live battle as the 3D game grows it, makes the
# orders a player's clicks would make, and the rules are given them at the same moments of the same battle
# (command-check.ts). Uses the Normandy battle run.sh fought (its own folder unless LAND_LIVE_OUT says another).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
REPO="$(cd "$ROOT/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_LIVE_OUT:-${TMPDIR:-/tmp}/land_live}"
mkdir -p "$OUT"
"$REPO/node_modules/.bin/esbuild" "$HERE/command-check.ts" --bundle --platform=node --format=esm --outfile="$OUT/land-command-check.mjs" --log-level=warning
# (the battle run.sh fought is reused only when the rules that fought it are the rules here: a fingerprint of the land
# rules' sources beside it; otherwise it is fought again, so a stale battle never hides a new rule)
STAMP="$(cat "$REPO"/src/battle/land/*.ts "$REPO"/src/battle/core/*.ts "$REPO"/data/battle/*.json "$ROOT/Rules/land-bridge.ts" "$HERE/deltas.ts" | cksum | cut -d' ' -f1)"
if [ ! -f "$OUT/normandy/deltas.jsonl" ] || [ "$(cat "$OUT/normandy/rules.stamp" 2>/dev/null)" != "$STAMP" ]; then
  rm -rf "$OUT/normandy"
  "$REPO/node_modules/.bin/esbuild" "$HERE/deltas.ts" --bundle --platform=node --format=esm --outfile="$OUT/land-live-deltas.mjs" --log-level=warning
  node "$OUT/land-live-deltas.mjs" "$OUT/normandy" normandy-1944 11 att 0.5 orders
  echo "$STAMP" > "$OUT/normandy/rules.stamp"
fi
node "$OUT/land-command-check.mjs" setup "$OUT/normandy-setup" normandy-1944 11
# (neighbours join the fight, release 2: the Normandy battle with a corps coming up from next door on each side)
"$REPO/node_modules/.bin/esbuild" "$HERE/join-check.ts" --bundle --platform=node --format=esm --outfile="$OUT/land-live-join.mjs" --log-level=warning
STATUS=0
node "$OUT/land-live-join.mjs" "$OUT/normandy-join" normandy-1944 11 || STATUS=1
# (a campaign battle opened live from its ticket, release 2: the same battle as the set piece's own from the piece's ticket)
"$REPO/node_modules/.bin/esbuild" "$HERE/ticket-check.ts" --bundle --platform=node --format=esm --outfile="$OUT/land-live-ticket.mjs" --log-level=warning
node "$OUT/land-live-ticket.mjs" normandy-1944 11 || STATUS=1
CORES="$SRC/Battle/LandJson.cpp $SRC/Battle/LandFeed.cpp $SRC/Battle/LandCommand.cpp"
CXX="${CXX:-g++}"
$CXX -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_command_test.cpp" $CORES -o "$OUT/land_command_test"
if command -v clang++ >/dev/null 2>&1; then
  clang++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -I"$SRC" "$HERE/land_command_test.cpp" $CORES -o "$OUT/land_command_test_clang"
fi
"$OUT/land_command_test" "$OUT/normandy" "$OUT/normandy-setup" "$OUT/normandy-join" || STATUS=1
node "$OUT/land-command-check.mjs" check "$OUT/normandy" "$OUT/normandy-setup" normandy-1944 11 0.5 || STATUS=1
if [ "$STATUS" = 0 ]; then echo PASS; else echo FAIL; fi
exit $STATUS
