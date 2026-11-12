#!/bin/bash
# Makes the campaign's music and brings it into the game.
# The piece is composed and synthesised by make_music.mjs (it needs Node.js); Unreal then imports it.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$PROJECT/../.." && pwd)"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
[ -d "$REPO/unreal/.tools/node/bin" ] && export PATH="$REPO/unreal/.tools/node/bin:$PATH"
if ! command -v node > /dev/null; then
  echo "Node.js was not found. The music cannot be made without it." >&2
  exit 1
fi
mkdir -p "$PROJECT/Saved/Music"
node "$HERE/make_music.mjs" "$PROJECT/Saved/Music/theme.wav"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT/TheaterCommand.uproject" -run=pythonscript -script="$HERE/build_music.py" -unattended -nosplash -nullrhi -stdout 2>&1 | grep -E "Theater Command:|Error|error:|Traceback" || true
ls -la "$PROJECT/Content/Campaign/Music/" 2> /dev/null || { echo "The music was not imported." >&2; exit 1; }
