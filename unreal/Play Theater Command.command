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

echo "Starting Theater Command..."
"$ENGINE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/TheaterCommand.uproject" -game -windowed > /tmp/theater-command-game.log 2>&1 &
disown
sleep 2
