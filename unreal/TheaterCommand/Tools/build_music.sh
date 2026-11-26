#!/bin/bash
# Makes the campaign's music and brings it into the game.
# The piece is composed and synthesised by make_music.mjs (it needs Node.js); Unreal then imports it.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$PROJECT/../.." && pwd)"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
# What the music is made from, as one stamp: the three pieces' own scripts, and the names and sizes of the score's
# recordings in Music/Score. ("build_music.sh --stamp" prints it and does nothing else: the launcher asks.)
# (LC_ALL=C: the order of the files, and so the stamp, is the same in every shell and from every window)
STAMP="$( (export LC_ALL=C; cat "$HERE"/make_music*.mjs; for F in "$PROJECT"/Music/Score/*.wav; do [ -f "$F" ] && echo "$(basename "$F") $(wc -c < "$F" | tr -d ' ')"; done; true) | shasum | cut -d' ' -f1)"
if [ "${1:-}" = "--stamp" ]; then
  echo "$STAMP"
  exit 0
fi
[ -d "$REPO/unreal/.tools/node/bin" ] && export PATH="$REPO/unreal/.tools/node/bin:$PATH"
if ! command -v node > /dev/null; then
  echo "Node.js was not found. The music cannot be made without it." >&2
  exit 1
fi
mkdir -p "$PROJECT/Saved/Music"
node "$HERE/make_music.mjs" "$PROJECT/Saved/Music/theme.wav"
node "$HERE/make_music_battle.mjs" "$PROJECT/Saved/Music/battle.wav"
node "$HERE/make_music_map_room.mjs" "$PROJECT/Saved/Music/map_room.wav"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT/TheaterCommand.uproject" -run=pythonscript -script="$HERE/build_music.py" -unattended -nosplash -nullrhi -stdout 2>&1 | grep -E "Theater Command:|Error|error:|Traceback" || true
ls -la "$PROJECT/Content/Campaign/Music/" 2> /dev/null || { echo "The music was not imported." >&2; exit 1; }
# Note which version of the piece this was made from, so the launcher knows when to make it again.
echo "$STAMP" > "$PROJECT/Saved/Music/made-from.txt"
