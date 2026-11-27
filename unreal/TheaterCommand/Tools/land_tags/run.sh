#!/bin/bash
# The field tags' placing in the cloud (the trial word tagroom, the UI director's ruling of 27 November 2026): builds
# Tools/land_tags/land_tags_test.cpp against Battle/LandTagPlace.h, the plain header the battle screen places its tags
# with, and runs it. Usage: Tools/land_tags/run.sh. LAND_TAGS_OUT=<folder> (default: under TMPDIR).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_TAGS_OUT:-${TMPDIR:-/tmp}/land_tags}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX ${LAND_TAGS_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_tags_test.cpp" -o "$OUT/land_tags_test"
"$OUT/land_tags_test"
