#!/bin/bash
# usage: build70.sh name [repo] [out]   (as build.sh, for Version 70 or later: bundles harness/stop70.ts in front of
# harness/name.ts, so that a plan's "stop" or TC_STOP=n sets the history dial; out defaults to "out70")
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="${2:-/home/user/tc70}"; OUT="${3:-out70}"
mkdir -p "$HERE/entry70" "$HERE/$OUT"
printf "import '../stop70';\nimport '../%s';\n" "$1" > "$HERE/entry70/$1.ts"
cd "$REPO" && npx esbuild "$HERE/entry70/$1.ts" --bundle --platform=node --format=esm --alias:@tc="$REPO/src" --outfile="$HERE/$OUT/$1.mjs" --log-level=warning
