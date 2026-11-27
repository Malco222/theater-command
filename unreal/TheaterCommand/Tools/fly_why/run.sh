#!/bin/bash
# The raid card says why FLY IT YOURSELF is not offered (a mend in his game: the design director's known-faults line of
# 27 November 2026 23:13Z, corrected 23:16Z). Reads the controller's and the card's source as the Mac builds them and
# holds the mend: the card's words, a line kept with every refusal and cleared with every offer, the log and the rules'
# calls as before, the card's line drawn only where there is one, and the rules' refusals whole sentences. The bridge
# probe of a refused raid is the rules' own test, tests/air-fly-why.test.ts (npx vitest run tests/air-fly-why.test.ts).
# Usage: Tools/fly_why/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
RULES="$(cd "$ROOT/../.." && pwd)"
OUT="${FLY_WHY_OUT:-${TMPDIR:-/tmp}/fly_why}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${FLY_WHY_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror "$HERE/fly_why_test.cpp" -o "$OUT/fly_why_test"
"$OUT/fly_why_test" "$SRC" "$RULES/src/battle/air/ticket.ts"
