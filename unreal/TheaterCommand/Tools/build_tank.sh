#!/bin/bash
# Sculpts the trial tank (art fix 12) with Blender, run without its window, and writes it where the game reads it.
# Blender is the free program from blender.org; nothing else is needed, and nothing is fetched.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BLENDER="${BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}"
if [ ! -x "$BLENDER" ]; then
  echo "Blender was not found at $BLENDER. Download it from https://www.blender.org/download/ and put it in Applications," >&2
  echo "or run this with BLENDER=/path/to/Blender." >&2
  exit 1
fi
mkdir -p "$HERE/../Content/Models"
"$BLENDER" --background --factory-startup --python "$HERE/build_tank.py" -- --out "$HERE/../Content/Models/TrialTank.json" 2>&1 | grep -E "Theater Command|Error|Traceback|error" || true
ls -la "$HERE/../Content/Models/TrialTank.json"
