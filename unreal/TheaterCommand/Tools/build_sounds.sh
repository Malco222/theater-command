#!/bin/bash
# Makes the game's sound effects and brings them into the game.
# The effects are synthesised by make_sounds.mjs (it needs Node.js); Unreal then imports them.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$PROJECT/../.." && pwd)"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
[ -d "$REPO/unreal/.tools/node/bin" ] && export PATH="$REPO/unreal/.tools/node/bin:$PATH"
if ! command -v node > /dev/null; then
  echo "Node.js was not found. The sound effects cannot be made without it." >&2
  exit 1
fi
mkdir -p "$PROJECT/Saved/Sounds"
node "$HERE/make_sounds.mjs" "$PROJECT/Saved/Sounds"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT/TheaterCommand.uproject" -run=pythonscript -script="$HERE/build_sounds.py" -unattended -nosplash -nullrhi -stdout 2>&1 | grep -E "Theater Command:|Error|error:|Traceback" || true
ls "$PROJECT/Content/Campaign/Sounds/" 2> /dev/null | wc -l | xargs echo "sounds in the game:" || { echo "The sounds were not imported." >&2; exit 1; }
# Note which version of the effects this was made from, so the launcher knows when to make them again.
shasum "$HERE/make_sounds.mjs" | cut -d' ' -f1 > "$PROJECT/Saved/Sounds/made-from.txt"
