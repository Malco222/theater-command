#!/bin/bash
# Sculpts the tanks (art fix 12) with Blender, run without its window, and writes them where the game reads them.
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
# The trial tank, and then every nation's tank under the name the game knows it by.
"$BLENDER" --background --factory-startup --python "$HERE/build_tank.py" -- --out "$HERE/../Content/Models/TrialTank.json" 2>&1 | grep -E "Theater Command|Error|Traceback|error" || true
"$BLENDER" --background --factory-startup --python "$HERE/build_tank.py" -- --all "$HERE/../Content/Models" 2>&1 | grep -E "Theater Command|Error|Traceback|error|File \"" || true
ls -la "$HERE/../Content/Models/"
