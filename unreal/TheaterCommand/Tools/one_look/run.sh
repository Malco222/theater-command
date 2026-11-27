#!/bin/bash
# One look for the whole war on the campaign map, its cloud test (release 2; Brian, 26 November 2026: "have the paint
# be on it regardless of the year"). Two parts. The table (one_look_check.py): the art director's "one_look" in
# Content/Data/Campaign/unit-table.json against the sculpted models and the map's own source: each dominion's last
# dated mark and fin are whole, its own and without red; each coat is the coat of the machine the model follows, by
# that machine's own file, or the model's own with a black nose and boss (the Mustang: Brian, 27 November 2026, "silver
# with black paint"); each mark drawn as another is one the painter draws; Germany's sculpted tanks take the
# nation's grey in every year. The reckoning (one_look_test.cpp): builds Campaign/OneLook.h (the game's own code, no
# engine) with the strictest warnings and runs it over the table's own months: with one look the last dated mark
# stands in every month and never changes; without, the marks change by the month as before.
# Usage: Tools/one_look/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${ONELOOK_OUT:-${TMPDIR:-/tmp}/one_look}"
mkdir -p "$OUT"
python3 -I "$HERE/one_look_check.py" "$ROOT" "$OUT"
CXX="${CXX:-g++}"
$CXX ${ONELOOK_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror -I"$SRC" -I"$OUT" \
  "$HERE/one_look_test.cpp" -o "$OUT/one_look_test"
"$OUT/one_look_test" "$@"
