#!/bin/bash
# Runs the game, takes the photographer's fixed set of pictures into the folder given
# (default: Saved/Pictures), prints how fast each view was drawing, and quits.
# It runs without sound, so it does not play music at whoever is at the computer.
# Extra arguments are passed to the game, e.g. -dpcvars=r.Shadow.Virtual.Enable=0
# THEATER_WINDOW=1728x1117 in the environment takes the pictures at another size (1600x1000 unless told).
# THEATER_SOUND=quiet runs it WITH sound, every sound and the music played too softly to hear ("-TheaterQuiet"):
# for proving in the log which recordings and which pieces of music play ("-LogCmds=LogTheater Verbose").
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$HERE/../TheaterCommand.uproject"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
OUT="${1:-$HERE/../Saved/Pictures}"
shift || true
mkdir -p "$OUT"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJECT" -game -windowed -TheaterWindow="${THEATER_WINDOW:-1600x1000}" -TheaterShots="$OUT" -unattended -nosplash $([ "${THEATER_SOUND:-}" = "quiet" ] && echo "-TheaterQuiet" || echo "-nosound") "$@" > /dev/null 2>&1 &
GAME=$!
wait "$GAME"
# If the run crashed, Unreal leaves a crash reporter behind to send its report. On 17 November 2026 one of them stuck
# and used a whole processor core for eight hours, which slowed the game and spoiled every timing taken beside it.
# A reporter for this run is given twenty seconds to finish and is then stopped. (Only this run's own: by its number.)
for _ in 1 2 3 4 5 6 7 8 9 10; do
  pgrep -f "CrashReportClient .*TheaterCommand-pid-$GAME-" > /dev/null || break
  sleep 2
done
if pgrep -f "CrashReportClient .*TheaterCommand-pid-$GAME-" > /dev/null; then
  pkill -f "CrashReportClient .*TheaterCommand-pid-$GAME-"
  echo "This run crashed, and its crash reporter was stopped after twenty seconds. The report is under ~/Library/Application Support/Epic/UnrealEngine/5.8/Saved/Crashes."
fi
grep "Photographer:\|LogTheater: Error\|LogTheater: Warning\|LogMaterial: Error\|LogShaderCompilers: Error" "$HOME/Library/Logs/TheaterCommand/TheaterCommand.log" | sed 's/^.*LogTheater: Display: //' | cut -c1-260
ls "$OUT"
