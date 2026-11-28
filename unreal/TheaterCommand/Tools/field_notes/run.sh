#!/bin/bash
# Field notes (release 2), checked in the cloud: builds Battle/FieldNotes (the game's own code, no engine) with both
# compilers' strict flags and plays a new general's first battle through it, frame by frame, as the scene fills the
# facts: the setup step, his view, a unit in hand, an order, the pace, a unit pointed at, a minute fought, then the
# closing note; LATER, NOTES OFF, a note held behind the after card, the notebook written and read back, FIRST
# COMMAND's arming again, and the shipped words (Content/Data/Battle/field-notes.json: 30 words a note at most), every
# key a note names against LandBattleScreen.cpp's OnKeyDown, and every screen word it names against the screen.
# Usage: Tools/field_notes/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${FIELD_NOTES_OUT:-${TMPDIR:-/tmp}/field_notes}"
mkdir -p "$OUT"
for CC in g++ clang++; do
  if ! command -v $CC > /dev/null; then
    echo "$CC not found: skipped"
    continue
  fi
  $CC -std=c++20 -O1 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -I"$SRC" \
    "$HERE/field_notes_test.cpp" "$SRC/Battle/FieldNotes.cpp" "$SRC/Battle/LandJson.cpp" -o "$OUT/field_notes_$CC"
  "$OUT/field_notes_$CC" "$ROOT/Content/Data/Battle/field-notes.json" "$SRC/Battle/LandBattleScreen.cpp"
done
