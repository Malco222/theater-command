#!/bin/bash
# Bundles the campaign rules for the Unreal game.
#
# The rules are the browser prototype's own TypeScript (src/campaign). This puts them, with the
# doorway in Rules/campaign-bridge.ts, into one JavaScript file the game runs:
# Content/Rules/campaign-rules.js. Run it again whenever the rules or the data change.
#
# It needs Node.js (unreal/.tools/node, or one already on the PATH) and the prototype's own
# dependencies (npm ci, in the repository's top folder).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$PROJECT/../.." && pwd)"
[ -d "$REPO/unreal/.tools/node/bin" ] && export PATH="$REPO/unreal/.tools/node/bin:$PATH"

if ! command -v node > /dev/null; then
  echo "Node.js was not found. The rules cannot be bundled without it." >&2
  exit 1
fi
if [ ! -x "$REPO/node_modules/.bin/esbuild" ]; then
  (cd "$REPO" && npm ci --ignore-scripts --no-audit --no-fund)
fi

OUT="$PROJECT/Content/Rules/campaign-rules.js"
STAMP="$(cd "$REPO" && git log -1 --format='%h %cs' -- src data 2> /dev/null || echo unknown)"
"$REPO/node_modules/.bin/esbuild" "$PROJECT/Rules/campaign-bridge.ts" \
  --bundle --format=iife --platform=neutral --target=es2020 --charset=utf8 --log-level=warning \
  --banner:js="// The campaign rules, bundled from the browser prototype (src/campaign, data/campaign) as of $STAMP. Do not edit: run Tools/build_rules.sh." \
  --outfile="$OUT"
echo "Rules bundled: $(wc -c < "$OUT" | tr -d ' ') bytes, from $STAMP"
