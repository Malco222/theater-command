#!/bin/bash
# The bargains' cloud test (release 2: a neutral's bargains and the volunteer legions on the campaign map). Builds the
# game's own pure code for them (Campaign/BargainFeed, and Campaign/BargainLook where it is in the tree; no engine) with
# each *_test.cpp in this folder, three ways: g++ with the strictest warnings, clang++ with its own (unreachable code,
# every shadow), and once more under the address and undefined-behaviour sanitizers; and runs each on the rules' real
# answers in fixtures/: TC.bargainsLook()'s own function (Rules/bargains-look.ts bargainsLookOf) in a grand campaign
# with both switches on (TC_BARGAINS=1 TC_LEGIONS=1, seed 1939, Britain at the table, history stop 1, bidding on the
# first round it can), asked every month: Britain as a bidder, Sweden as a seller and the United States as an outsider
# in August 1941, Britain with an agent placed by hand in Stockholm on a copy of that month, Britain in June 1940 when
# Finland's exports to it are first seen cut off at sea, and the answer while both are off.
# Usage: Tools/bargains/run.sh [fixtures folder]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${BARGAINS_OUT:-${TMPDIR:-/tmp}/bargains}"
FIXTURES="${1:-$HERE/fixtures}"
mkdir -p "$OUT"

UNITS=("$SRC/Campaign/BargainFeed.cpp")
[ -f "$SRC/Campaign/BargainLook.cpp" ] && UNITS+=("$SRC/Campaign/BargainLook.cpp")

build_and_run() {
  local how="$1" cxx="$2"; shift 2
  for test in "$HERE"/*_test.cpp; do
    local name; name="$(basename "$test" .cpp)"
    echo "== $name ($how)"
    "$cxx" "$@" -std=c++20 -I"$SRC" "$test" "${UNITS[@]}" -o "$OUT/$name-$how"
    "$OUT/$name-$how" "$FIXTURES"
  done
}

build_and_run gcc "${CXX:-g++}" -O2 -Wall -Wextra -Wshadow -Werror
if command -v "${CLANGXX:-clang++}" > /dev/null; then
  build_and_run clang "${CLANGXX:-clang++}" -O2 -Wall -Wextra -Wshadow-all -Wunreachable-code-aggressive -Werror
else
  echo "(clang++ not found: its build is skipped)"
fi
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  build_and_run sanitized "${CXX:-g++}" -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Wshadow -Werror
echo "Every bargains test passed."
