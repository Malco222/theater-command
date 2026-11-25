#!/bin/sh
# Builds and runs the land battle's paired measure (paired.ts says what it measures and how to read it):
#   tools/land-gate/run.sh [--set S1,S2,S3,S4,S5,SF] [--mirror attack|defend] [--switch pierce] [--seeds N] [--arm off] [--placebo] [--like] [--json file]
set -e
cd "$(dirname "$0")/../.."
npx esbuild tools/land-gate/paired.ts --bundle --platform=node --format=esm --outfile=dist/land-gate.mjs --log-level=warning
node dist/land-gate.mjs "$@"
