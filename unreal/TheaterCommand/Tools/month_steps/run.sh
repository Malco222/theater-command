#!/bin/bash
# The month laid in steps (tc.Speed.MonthInSteps): the cloud's test of Campaign/MonthSteps, the game's own code with no
# engine in it. Builds it with g++ and checks the orders of the two modes, the frame budget (the first step always, cheap
# steps sharing a frame, a dear one alone, the order never changed), the season's parts put at the head of the queue,
# the queue drained at once and dropped. It cannot see the engine's side (Campaign/MonthLaying.cpp): that is the 3D
# pre-check's to compile and map run 7's to time.
# Usage: Tools/month_steps/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SOURCE="$(cd "$HERE/../../Source/TheaterCommand" && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
g++ -std=c++20 -O1 -Wall -Wextra -Werror -I"$SOURCE" "$SOURCE/Campaign/MonthSteps.cpp" "$HERE/month_steps_test.cpp" -o "$OUT/month_steps_test"
"$OUT/month_steps_test"
