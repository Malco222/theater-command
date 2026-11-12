#!/bin/bash
# Runs the game, takes the photographer's fixed set of pictures into the folder given
# (default: Saved/Pictures), prints how fast each view was drawing, and quits.
# It runs without sound, so it does not play music at whoever is at the computer.
# Extra arguments are passed to the game, e.g. -dpcvars=r.Shadow.Virtual.Enable=0
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$HERE/../TheaterCommand.uproject"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
OUT="${1:-$HERE/../Saved/Pictures}"
shift || true
mkdir -p "$OUT"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJECT" -game -windowed -TheaterWindow=1600x1000 -TheaterShots="$OUT" -unattended -nosplash -nosound "$@" > /dev/null 2>&1
grep "Photographer:\|LogTheater: Error\|LogTheater: Warning\|LogMaterial: Error\|LogShaderCompilers: Error" "$HOME/Library/Logs/TheaterCommand/TheaterCommand.log" | sed 's/^.*LogTheater: Display: //' | cut -c1-260
ls "$OUT"
