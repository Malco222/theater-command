#!/bin/bash
# Sculpts the aircraft with Blender, run without its window, and writes them where the game reads them.
# Blender is the free program from blender.org; nothing else is needed, and nothing is fetched.
#   ./Tools/build_plane.sh                 every machine
#   ./Tools/build_plane.sh Bf109,Zero      only these
#   LOOK=/some/folder ./Tools/build_plane.sh   also writes Blender's own pictures of each, for checking shapes
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BLENDER="${BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}"
if [ ! -x "$BLENDER" ]; then
  echo "Blender was not found at $BLENDER. Download it from https://www.blender.org/download/ and put it in Applications," >&2
  echo "or run this with BLENDER=/path/to/Blender." >&2
  exit 1
fi
mkdir -p "$HERE/../Content/Models"
ARGS=(--all "$HERE/../Content/Models")
[ -n "${1:-}" ] && ARGS+=(--only "$1")
[ -n "${LOOK:-}" ] && mkdir -p "$LOOK" && ARGS+=(--look "$LOOK")
"$BLENDER" --background --factory-startup --python "$HERE/build_plane.py" -- "${ARGS[@]}" 2>&1 | grep -E "Theater Command|Error|Traceback|error|File \"" || true
# The bomb bays (Tools/bay_doors.py): a bay with two doors cut in the belly of each bomber that carries its bombs inside.
python3 "$HERE/bay_doors.py" "$HERE/../Content/Models" ${1:+--only "$1"}
