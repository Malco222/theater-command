#!/bin/bash
# The free camera's cloud test: builds the core (Battle/FreeEye, plain C++, no engine) and its test with g++ and, where
# it is here, clang++ (the Mac builds with clang and its -Wunreachable-code-aggressive), with the Mac's warnings as
# errors, and runs both. Exits non-zero on any FAULT or build failure.
# Usage: Tools/free_eye/run.sh   (FREE_EYE_SRC=path/to/FreeEye.cpp tests another copy of the core: breakages.sh)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${FREE_EYE_OUT:-${TMPDIR:-/tmp}/free_eye}"
mkdir -p "$OUT"
CORE="${FREE_EYE_SRC:-$SRC/Battle/FreeEye.cpp}"
CXX="${CXX:-g++}"
$CXX -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -fno-exceptions -fno-rtti -I"$SRC" "$HERE/free_eye_test.cpp" "$CORE" -o "$OUT/free_eye_test"
CLANG="$(command -v clang++ || ls /usr/bin/clang++-[0-9]* 2>/dev/null | sort -V | tail -n 1 || true)"
if [ -n "$CLANG" ] && [ -z "${FREE_EYE_ONE:-}" ]; then
  "$CLANG" -std=c++20 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -fno-exceptions -fno-rtti -I"$SRC" "$HERE/free_eye_test.cpp" "$CORE" -o "$OUT/free_eye_test_clang"
fi
STATUS=0
echo "== g++"
"$OUT/free_eye_test" || STATUS=1
if [ -n "$CLANG" ] && [ -z "${FREE_EYE_ONE:-}" ]; then
  echo "== $CLANG"
  "$OUT/free_eye_test_clang" > "$OUT/clang.txt" || STATUS=1
  # (the two compilers must agree to the last digit printed, as the Mac and the cloud must)
  "$OUT/free_eye_test" > "$OUT/gcc.txt" || true
  if ! cmp -s "$OUT/gcc.txt" "$OUT/clang.txt"; then
    echo "NOTE: g++ and clang++ printed different numbers:"; diff "$OUT/gcc.txt" "$OUT/clang.txt" | head -n 20 || true
  fi
  grep -E "^FAULT|faults?$" "$OUT/clang.txt" || true
elif [ -z "$CLANG" ]; then
  echo "(no clang++ here: built with g++ only)"
fi
exit $STATUS
