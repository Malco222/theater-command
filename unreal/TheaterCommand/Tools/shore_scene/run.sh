#!/bin/bash
# Writes the shelling scene's sample staged recordings (sea-scene/shore-guns/plan.md R1) into a folder, for the sea
# scene's cloud test (Tools/sea_show/run.sh <folder>/*.json). Usage: Tools/shore_scene/run.sh [out-dir]
# (default: $TMPDIR/shore_scene). It needs Node.js and the prototype's own dependencies (npm ci, in the repository's top
# folder), as Tools/build_rules.sh does.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$(cd "$HERE/../.." && pwd)"
REPO="$(cd "$PROJECT/../.." && pwd)"
[ -d "$REPO/unreal/.tools/node/bin" ] && export PATH="$REPO/unreal/.tools/node/bin:$PATH"
OUT="${1:-${TMPDIR:-/tmp}/shore_scene}"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
"$REPO/node_modules/.bin/esbuild" "$HERE/samples.ts" --bundle --platform=node --format=esm --log-level=warning --outfile="$BUILD/samples.mjs"
node "$BUILD/samples.mjs" "$OUT"
