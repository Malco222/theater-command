#!/bin/bash
# Makes the game's sound effects and brings them into the game.
# The effects are synthesised by make_sounds.mjs (it needs Node.js); Unreal then imports them.
# The real recordings in Sounds/Real (see Sounds/Real/CREDITS.md) are imported beside them, to
# /Game/Campaign/Sounds/Real: the game plays a recording wherever it has one for the moment, and the
# synthesised stand-in otherwise (Source/TheaterCommand/Campaign/CampaignSounds.cpp).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$PROJECT/../.." && pwd)"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
# What the sounds are made from, as one stamp: the synthesiser's script and every real recording in Sounds/Real.
# ("build_sounds.sh --stamp" prints it and does nothing else: the launcher asks, to know when to make them again.)
# (LC_ALL=C: the order of the files, and so the stamp, is the same in every shell and from every window)
STAMP="$( (export LC_ALL=C; cat "$HERE/make_sounds.mjs"; cat "$PROJECT"/Sounds/Real/*.wav 2> /dev/null || true) | shasum | cut -d' ' -f1)"
if [ "${1:-}" = "--stamp" ]; then
  echo "$STAMP"
  exit 0
fi
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
echo "$STAMP" > "$PROJECT/Saved/Sounds/made-from.txt"
