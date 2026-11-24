#!/bin/bash
# Writes the three sample staged recordings of the month's submarine scene (form D1) into a folder, for the sea scene's
# cloud test (Tools/sea_show/run.sh <folder>/*.json). Usage: Tools/sub_scene/run.sh [out-dir]   (default: $TMPDIR/sub_scene)
# It needs Node.js and the prototype's own dependencies (npm ci, in the repository's top folder), as Tools/build_rules.sh does.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$(cd "$HERE/../.." && pwd)"
REPO="$(cd "$PROJECT/../.." && pwd)"
[ -d "$REPO/unreal/.tools/node/bin" ] && export PATH="$REPO/unreal/.tools/node/bin:$PATH"
OUT="${1:-${TMPDIR:-/tmp}/sub_scene}"
BUILD="$(mktemp -d)"
"$REPO/node_modules/.bin/esbuild" "$HERE/samples.ts" --bundle --platform=node --format=esm --log-level=warning --outfile="$BUILD/samples.mjs"
node "$BUILD/samples.mjs" "$OUT"
rm -rf "$BUILD"
