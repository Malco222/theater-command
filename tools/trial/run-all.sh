#!/bin/bash
# TRIAL (far-side seats rig). Plays every seat of the build, four at a time, and prints each verdict.
#
#   tools/trial/run-all.sh [OUT_DIR] [SEED] [MONTHS] [SEATS]
#
# OUT_DIR  where each seat's full printout goes (default: dist/trial/out)  SEED    default 7
# MONTHS   default 12                                                       SEATS   "SAF IRN ..." (default: every seat of the build)
# Build first: node tools/trial/build.mjs. Each run is well under a minute for a year (about 2 seconds a month).
cd "$(dirname "$0")/../.." || exit 2
OUT="${1:-dist/trial/out}"; SEED="${2:-7}"; MONTHS="${3:-12}"
BIN=dist/trial/seat-play.mjs
[ -f "$BIN" ] || { echo "build first: node tools/trial/build.mjs"; exit 2; }
SEATS="${4:-$(node "$BIN" list | awk '/^  [A-Z]/ {print $1}' | tr '\n' ' ')}"
mkdir -p "$OUT"
echo "$SEATS" | tr ' ' '\n' | grep . | xargs -P 4 -I{} sh -c "node $BIN {} $SEED $MONTHS > $OUT/{}-seed$SEED.txt 2>&1"
fail=0
for s in $SEATS; do grep -h '^VERDICT' "$OUT/$s-seed$SEED.txt" || { echo "VERDICT $s seed $SEED: FAIL: no verdict line (see $OUT/$s-seed$SEED.txt)"; }; grep -q '^VERDICT .*: PASS' "$OUT/$s-seed$SEED.txt" || fail=1; done
exit $fail
