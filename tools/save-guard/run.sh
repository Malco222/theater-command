#!/bin/bash
# The save guard: every saved campaign in saves/ beside this script (Brian's own, from his Mac) is loaded on TREE and
# played on with no orders, first by the rules alone (saveguard.ts), then in the built page with real presses
# (savepage.cjs). It changes nothing in TREE.
#
#   bash run.sh TREE OUT [months]
#
# TREE    a checkout of the browser game with node_modules in it; dist/play.html built for the page half (chain.sh
#         builds it first; without it the page half is passed over and says so)
# OUT     a folder of your own for the logs and pictures
# months  how many months to play after loading (5 for the rules, 2 in the page; default 5)
#
# It prints two lines a save, each ending "errors: [...]". An empty list is a pass. Everything else is in OUT.
# About 5 seconds a save for the rules and 20 for the page.
HERE="$(cd "$(dirname "$0")" && pwd)"
TREE="$(cd "$1" && pwd)"; OUT="$2"; MONTHS="${3:-5}"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
# One source for the repo and for any tree: the script's own imports ('../../src/', '../../data/') are pointed at TREE.
sed "s#'\.\./\.\./src/#'$TREE/src/#g; s#'\.\./\.\./data/#'$TREE/data/#g" "$HERE/saveguard.ts" > "$OUT/saveguard.ts"
( cd "$TREE" && npx esbuild "$OUT/saveguard.ts" --bundle --platform=node --format=esm --outfile="$OUT/saveguard.mjs" --log-level=error ) > "$OUT/bundle.txt" 2>&1
if [ ! -s "$OUT/saveguard.mjs" ]; then echo "save guard: the check does not build against this tree: $(head -3 "$OUT/bundle.txt" | tr '\n' ' ' | cut -c1-300) errors: [\"build\"]"; exit 0; fi
for SAVE in "$HERE"/saves/*.json; do
  case "$SAVE" in *.expect.json) continue;; esac
  NAME="$(basename "$SAVE" .json)"
  node "$OUT/saveguard.mjs" "$SAVE" "$MONTHS" > "$OUT/$NAME-rules.txt" 2>&1
  echo "save guard, rules ($NAME): $(grep -c '^ok' "$OUT/$NAME-rules.txt") ok, $(grep -c '^WATCH' "$OUT/$NAME-rules.txt") to watch, $(grep '^errors' "$OUT/$NAME-rules.txt" | cut -c1-400)$(grep -q '^errors' "$OUT/$NAME-rules.txt" || echo "errors: [\"no end line: $(tail -1 "$OUT/$NAME-rules.txt" | cut -c1-200)\"]")"
  if [ -f "$TREE/dist/play.html" ]; then
    PM=$(( MONTHS < 2 ? MONTHS : 2 ))
    node "$HERE/savepage.cjs" "$TREE" "$SAVE" "$OUT/pages" "$PM" > "$OUT/$NAME-page.txt" 2>&1
    echo "save guard, page ($NAME): $(grep -c '^ok' "$OUT/$NAME-page.txt") ok, $(grep '^errors' "$OUT/$NAME-page.txt" | cut -c1-400)$(grep -q '^errors' "$OUT/$NAME-page.txt" || echo "errors: [\"no end line: $(tail -1 "$OUT/$NAME-page.txt" | cut -c1-200)\"]")"
  else
    echo "save guard, page ($NAME): passed over, $TREE/dist/play.html is not built"
  fi
done
# Every line to watch, so that a reader of the chain's own lines sees them without opening the logs.
grep -h '^WATCH' "$OUT"/*-rules.txt "$OUT"/*-page.txt 2>/dev/null | sed 's/^/  /' | cut -c1-300
