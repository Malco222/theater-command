#!/bin/bash
# Double-click this file in Finder to play.
# It brings the game's code up to date, then opens the campaign map in its own window.
# The first time takes a minute or two while Unreal prepares its materials.

cd "$(dirname "$0")/TheaterCommand" || exit 1
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"

if [ ! -d "$ENGINE" ]; then
  echo "Unreal Engine 5.8 was not found at: $ENGINE"
  echo "Install it from the Epic Games Launcher, then try again."
  read -n 1 -s -r -p "Press any key to close."
  exit 1
fi

echo "Bringing the game up to date..."
if ! ./Tools/build.sh > /tmp/theater-command-build.log 2>&1; then
  echo "The game's code did not build. The details are in /tmp/theater-command-build.log"
  tail -n 30 /tmp/theater-command-build.log
  read -n 1 -s -r -p "Press any key to close."
  exit 1
fi

# Bring the rules up to date with the browser version's, when the tools to do it are here.
./Tools/build_rules.sh > /tmp/theater-command-rules.log 2>&1 || true

# Make the music the first time, and again whenever the piece has been rewritten (it is not kept in the
# repository). It takes about half a minute.
# (and when one of the score's recordings is added to Music/Score: the stamp covers the scripts and the recordings)
PIECE="$(./Tools/build_music.sh --stamp 2> /dev/null)"
if [ ! -f Content/Campaign/Music/Theme.uasset ] || [ ! -f Content/Campaign/Music/Battle.uasset ] || [ "$(cat Saved/Music/made-from.txt 2> /dev/null)" != "$PIECE" ]; then
  echo "Making the music (only when it is new)..."
  ./Tools/build_music.sh > /tmp/theater-command-music.log 2>&1 || true
fi

# The sound effects the same way: made the first time, and again when they have been rewritten.
# (and when a real recording has been added to Sounds/Real: the stamp covers the synthesiser and every recording)
EFFECTS="$(./Tools/build_sounds.sh --stamp 2> /dev/null)"
if [ ! -d Content/Campaign/Sounds ] || [ "$(cat Saved/Sounds/made-from.txt 2> /dev/null)" != "$EFFECTS" ]; then
  echo "Making the sound effects (only when they are new)..."
  ./Tools/build_sounds.sh > /tmp/theater-command-sounds.log 2>&1 || true
fi

echo "Starting Theater Command..."
# (-TheaterWorld: the whole-world map. The grand campaign is then played on the wider chart, with North America's
# provinces; the history scenarios keep to Europe, with the world beyond as their backdrop. Take the flag out to
# go back to the table laid with Europe alone.)
# (-TheaterSpeedLog, the speed detective's ask of 18 November 2026: a few lines of timings in the game's log every five
# seconds, and nothing different on screen. And each session's log is kept under its own date in
# ~/Library/Logs/TheaterCommand/Sessions, where a later test run cannot write over it: the only reading there is
# of a long evening's play in this window, on this Mac.)
SESSIONS="$HOME/Library/Logs/TheaterCommand/Sessions"
KEPT=()
if mkdir -p "$SESSIONS" 2> /dev/null; then
  KEPT=("-abslog=$SESSIONS/session-$(date +%Y-%m-%d-%H%M%S).log")
fi
"$ENGINE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/TheaterCommand.uproject" -game -windowed -TheaterWorld -TheaterSpeedLog "${KEPT[@]}" > /tmp/theater-command-game.log 2>&1 &
disown
sleep 2
