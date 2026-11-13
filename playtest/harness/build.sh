#!/bin/bash
# usage: build.sh name [repo] [out]   (bundles harness/name.ts to harness/<out>/name.mjs against the rules in the repo; out defaults to "out";
# repo defaults to the checkout this folder sits in, when it sits in one)
HERE="$(cd "$(dirname "$0")" && pwd)"; HOME_REPO="$(cd "$HERE/../.." 2>/dev/null && pwd)"; [ -d "$HOME_REPO/src/campaign" ] || HOME_REPO=/home/user/theater-command
REPO="${2:-$HOME_REPO}"; OUT="${3:-out}"
cd "$REPO" && mkdir -p "$HERE/$OUT" && npx esbuild "$HERE/$1.ts" --bundle --platform=node --format=esm --alias:@tc="$REPO/src" --outfile="$HERE/$OUT/$1.mjs" --log-level=warning
