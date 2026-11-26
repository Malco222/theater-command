#!/bin/bash
# Bundles the land battle rules for the 3D game's live ground battle (release 2).
#
# The rules are the browser prototype's own TypeScript (src/battle/land). This puts them, with the
# doorway in Rules/land-bridge.ts, into one JavaScript file the game runs in a rules engine of its own:
# Content/Rules/land-rules.js. The campaign's bundle (build_rules.sh, campaign-rules.js) is separate and
# not touched. Run it again whenever the land rules or their data change.
#
# It needs Node.js (unreal/.tools/node, or one already on the PATH) and the prototype's own
# dependencies (npm ci, in the repository's top folder).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$PROJECT/../.." && pwd)"
[ -d "$REPO/unreal/.tools/node/bin" ] && export PATH="$REPO/unreal/.tools/node/bin:$PATH"

if ! command -v node > /dev/null; then
  echo "Node.js was not found. The land rules cannot be bundled without it." >&2
  exit 1
fi
if [ ! -x "$REPO/node_modules/.bin/esbuild" ]; then
  (cd "$REPO" && npm ci --ignore-scripts --no-audit --no-fund)
fi

OUT="$PROJECT/Content/Rules/land-rules.js"
mkdir -p "$(dirname "$OUT")"
STAMP="$(cd "$REPO" && git log -1 --format='%h %cs' -- src/battle data/battle 2> /dev/null || echo unknown)"
# (always from the project's folder, so the bundle's path notes read the same whoever runs it, from wherever)
cd "$PROJECT"
"$REPO/node_modules/.bin/esbuild" "$PROJECT/Rules/land-bridge.ts" \
  --bundle --format=iife --platform=neutral --target=es2020 --charset=utf8 --log-level=warning \
  --banner:js="// The land battle rules, bundled from the browser prototype (src/battle, data/battle) as of $STAMP. Do not edit: run Tools/build_land_rules.sh." \
  --outfile="$OUT"
echo "Land rules bundled: $(wc -c < "$OUT" | tr -d ' ') bytes, from $STAMP"
