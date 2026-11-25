#!/bin/sh
# Builds and runs the sameness meter: tools/squad-life/run.sh [piece] [seed] [upto] [mode]
set -e
cd "$(dirname "$0")/../.."
npx esbuild tools/squad-life/sameness.ts --bundle --platform=node --format=esm --outfile=dist/sameness.mjs --log-level=warning
node dist/sameness.mjs "$@"
