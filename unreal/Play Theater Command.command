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
PIECE="$(cat Tools/make_music*.mjs | shasum | cut -d' ' -f1)"
if [ ! -f Content/Campaign/Music/Theme.uasset ] || [ ! -f Content/Campaign/Music/Battle.uasset ] || [ "$(cat Saved/Music/made-from.txt 2> /dev/null)" != "$PIECE" ]; then
  echo "Making the music (only when it is new)..."
  ./Tools/build_music.sh > /tmp/theater-command-music.log 2>&1 || true
fi

# The sound effects the same way: made the first time, and again when they have been rewritten.
EFFECTS="$(shasum Tools/make_sounds.mjs | cut -d' ' -f1)"
if [ ! -d Content/Campaign/Sounds ] || [ "$(cat Saved/Sounds/made-from.txt 2> /dev/null)" != "$EFFECTS" ]; then
  echo "Making the sound effects (only when they are new)..."
  ./Tools/build_sounds.sh > /tmp/theater-command-sounds.log 2>&1 || true
fi

echo "Starting Theater Command..."
"$ENGINE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/TheaterCommand.uproject" -game -windowed > /tmp/theater-command-game.log 2>&1 &
disown
sleep 2
