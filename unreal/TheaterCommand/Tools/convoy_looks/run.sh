#!/bin/bash
# The submarine scene's looks' cloud test (ruling 241, D1): builds Battle/ConvoyLooksCore (the game's own code, no
# engine) and checks the boat rows against the stage's code-built model (read from its source), the boat's states and
# her dive (never a pop), the depth charges' domes and columns by depth, a pattern in the water before its bursts, a
# starshell's fall and light, a crushed boat's boils, a boat blown riding high, a rescuer's nets (and the rescue against the
# sea scene's wreck model, read from its source); then plays the sea show's stand-in convoy night (read only) through the
# show itself and checks the looks, the model the stage draws and the scene's men in the water against it.
# Usage: Tools/convoy_looks/run.sh [recording.json ...]   (default: the stand-in convoy night; CONVOY_LOOKS_STORY=1
# prints a dive, the bursts and a starshell)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${CONVOY_LOOKS_OUT:-${TMPDIR:-/tmp}/convoy_looks}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${CONVOY_LOOKS_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/convoy_looks_test.cpp" "$SRC/Battle/ConvoyLooksCore.cpp" -o "$OUT/convoy_looks_test"
"$OUT/convoy_looks_test" "$SRC"
# (against the show: the game's own SeaShow and its feed, as the sea show's cloud test builds them)
$CXX ${CONVOY_LOOKS_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" \
  "$HERE/convoy_looks_show.cpp" "$SRC/Battle/ConvoyLooksCore.cpp" "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" \
  "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirSky.cpp" "$SRC/Effects/TheaterFxCore.cpp" -o "$OUT/convoy_looks_show"
if [ "$#" -eq 0 ]; then set -- "$ROOT/Tools/sea_show/stand-ins/convoy-night.json"; fi
"$OUT/convoy_looks_show" "$@"
