#!/bin/bash
# The ships' paint's cloud test (the ship detail plan, round 1). Cuts the ship branch of FMiniatureMesh::Sculpted out of
# Campaign/CampaignMiniatures.cpp twice, the reference as it stood at 55419eb6 (from git) and the tree's own, builds both
# against the engine's stand-ins (paint_shim.h) with Battle/ShipPaintCore, the game's own code, and holds them to every
# ship file in Content/Models/Ships: off, bit for bit the reference's; on, the art director's numbers. Built with g++ and
# with clang++, each -O2 -std=c++20 -ffp-contract=off. Then the reel's own small check (reel_test.cpp).
# Usage: Tools/ship_detail/run.sh   (SHIP_DETAIL_OUT names the build folder; SHIP_DETAIL_COMPILERS="g++ clang++" by default)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SHIP_DETAIL_OUT:-${TMPDIR:-/tmp}/ship_detail}"
mkdir -p "$OUT"
# (the reference: the branch as it stood before the paint moved to Battle/ShipPaintCore)
git -C "$ROOT" show 55419eb6:unreal/TheaterCommand/Source/TheaterCommand/Campaign/CampaignMiniatures.cpp |
  python3 -I -B "$HERE/extract_paint.py" - "$SRC/Campaign/CampaignMiniatures.cpp" "$OUT"
for CXX in ${SHIP_DETAIL_COMPILERS:-g++ clang++}; do
  NAME="$(basename "$CXX")"
  echo "== ship_paint_test, $NAME"
  $CXX -std=c++20 -O2 -ffp-contract=off -Wall -Wextra -Wshadow -Werror -I"$SRC" -I"$OUT" -I"$HERE" \
    "$HERE/ship_paint_test.cpp" "$SRC/Battle/ShipPaintCore.cpp" "$SRC/Battle/LandJson.cpp" -o "$OUT/ship_paint_test_$NAME"
  "$OUT/ship_paint_test_$NAME" "$SRC" "$ROOT/Content/Models/Ships"
done
if [ -f "$HERE/reel_test.cpp" ]; then
  echo "== reel_test"
  ${SHIP_DETAIL_REEL_CXX:-g++} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/reel_test.cpp" -o "$OUT/reel_test"
  "$OUT/reel_test" "$SRC"
fi
