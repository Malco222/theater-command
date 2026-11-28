#!/bin/bash
# The replay map (release 2), checked in the cloud: builds Battle/LandReplay (the game's own code, no engine) with both
# compilers' strict flags, lays the shipped battles' recordings as the map (every unit of every view where the view has
# it; the woods, town and wet ground shaded cell for cell; the ways), a live battle's views made by hand (his side's,
# the enemy's before and after they are met), the 20 seconds played at 30 frames a second, and the screen's words and
# keys and the ship list's row read from their files.
# Usage: Tools/replay_map/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${REPLAY_MAP_OUT:-${TMPDIR:-/tmp}/replay_map}"
mkdir -p "$OUT"
for CC in g++ clang++; do
  if ! command -v $CC > /dev/null; then
    echo "$CC not found: skipped"
    continue
  fi
  $CC -std=c++20 -O1 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -I"$SRC" \
    "$HERE/replay_map_test.cpp" "$SRC/Battle/LandReplay.cpp" "$SRC/Battle/LandFeed.cpp" "$SRC/Battle/LandJson.cpp" -o "$OUT/replay_map_$CC"
  "$OUT/replay_map_$CC" "$ROOT/Content/Data/LandBattles/normandy-1944.json" "$ROOT/Content/Data/LandBattles/prokhorovka-1943.json" \
    "$SRC/Battle/LandBattleScreen.cpp" "$SRC/Release2Ship.cpp" "$SRC/Battle/LandBattleReplay.cpp"
done
