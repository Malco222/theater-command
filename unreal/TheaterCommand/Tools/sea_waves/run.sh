#!/bin/bash
# The release 2 sea's cloud test (the release 2 ocean plan, section 6; step 0: measure first, and the guard rails). No
# engine: the C++ with g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror, the Python with python3 -I -B. Run beside the two
# existing runners (Tools/sea_show/run.sh, Tools/convoy_looks/run.sh), which keep their baseline counts (baseline.txt).
#   - measure_test: the frames by thread and the "Sea look:" line (Battle/SeaFrames.h, Battle/SeaLookSwitches.h), and
#     the game's lines that feed them;
#   - graph_pin: the 15 materials Tools/build_content.py builds today, graph for graph, against graphs.sha, through the
#     stand-in Unreal module (fake_unreal.py);
#   - pearl_pin: Pearl Harbor's and Midway's sea, the battle look, the convoy looks' core and the byte copies, against
#     pearl.sha; and bUseUnity, and the campaign's three that take no new line, against 10f85ffb;
#   - room_scan: every lamp, sky light, fog and air setter the sea's code calls, given back when the scene ends;
#   - switches_scan: the plan's switch table, against the "Sea look:" line's and every switch the game reads;
#   - bench_table's self-test: the Mac bench's reader (Tools/sea_bench.sh) held to the game's own lines;
#   - bench_dry_run: Tools/sea_bench.sh itself, with a stand-in in the engine's place (stand_in_game.py): its order, its
#     replays of 22 November's picture runs, its columns, its window, sound and span, its stop of a hung run, and its
#     table (about a minute and a half: a hung run is given 30 s to stop before it is killed);
#   - marks_pool_test (step 1(a), pay first: DrawMarks' meshes kept while their shape holds, -TheaterSeaMarksPool=1): the
#     game's own DrawMarks, cut out of Battle/SeaBattle.cpp as it stands (marks_extract.py) and built against the engine's
#     stand-ins (marks_shim.h), played today's way and kept over every recorded action and stand-in, frame by frame, with
#     the wake ink and without: kept draws today's, never sends more points or makes its proxy afresh more often, and
#     every kind of mark is found in today's own meshes; the recordings MARKS_JOBS at once (default 4: about 8 minutes on
#     the cloud's four cores alone, half an hour beside other runs), each one's log in OUT/marks, then their sum.
# Usage: Tools/sea_waves/run.sh   (SEA_WAVES_OUT: where the test writes; default $TMPDIR/sea_waves)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${SEA_WAVES_OUT:-${TMPDIR:-/tmp}/sea_waves}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
cd "$ROOT" || exit 1
STATUS=0
FAILED=()
step() {
  local NAME="$1"
  shift
  echo "== $NAME"
  if "$@"; then
    echo "== $NAME: passed"
  else
    echo "== $NAME: FAILED"
    STATUS=1
    FAILED+=("$NAME")
  fi
}

rm -f "$OUT/measure_test" "$OUT/measure_lines.txt" # (nothing of an earlier run read as this one's)
step "measure_test (build)" $CXX ${SEA_WAVES_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/measure_test.cpp" -o "$OUT/measure_test"
if [ -x "$OUT/measure_test" ]; then
  step "measure_test" "$OUT/measure_test" "$SRC" "$OUT"
fi
step "graph_pin" python3 -I -B "$HERE/graph_pin.py" --out "$OUT"
step "pearl_pin" bash "$HERE/pearl_pin.sh"
step "room_scan (self-test)" python3 -I -B "$HERE/room_scan.py" --self-test
step "room_scan" python3 -I -B "$HERE/room_scan.py"
step "switches_scan (self-test)" python3 -I -B "$HERE/switches_scan.py" --self-test
step "switches_scan" python3 -I -B "$HERE/switches_scan.py"
step "bench_table (self-test)" python3 -I -B "$HERE/bench_table.py" --self-test "$OUT/measure_lines.txt"
step "sea_bench.sh (reads as bash)" bash -n "$ROOT/Tools/sea_bench.sh"
step "bench_dry_run" python3 -I -B "$HERE/bench_dry_run.py" "$OUT/measure_lines.txt" "$OUT"

# Step 1(a), pay first: DrawMarks' meshes kept (the release 2 ocean plan; Battle/SeaMarksPool.h)
MARKS="$OUT/marks"
rm -rf "$MARKS" "$OUT/marks_pool_test"
mkdir -p "$MARKS"
step "marks_extract (DrawMarks cut out of the game's source)" python3 -I -B "$HERE/marks_extract.py" "$SRC" "$MARKS"
if [ -f "$MARKS/marks_bodies.inc" ]; then
  step "marks_pool_test (build)" $CXX ${SEA_WAVES_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" -I"$MARKS" -I"$HERE" "$HERE/marks_pool_test.cpp" \
    "$SRC/Battle/SeaShow.cpp" "$SRC/Battle/ShipGuns.cpp" "$SRC/Battle/SeaFeed.cpp" "$SRC/Battle/AirFeed.cpp" "$SRC/Battle/AirShow.cpp" "$SRC/Battle/AirSky.cpp" \
    "$SRC/Effects/TheaterFxCore.cpp" -o "$OUT/marks_pool_test"
fi
if [ -x "$OUT/marks_pool_test" ]; then
  # (every recorded action and stand-in the sea show's runner plays, as it finds them)
  shopt -s nullglob
  PLAYED=("$ROOT"/Content/Data/SeaBattles/*.json "$ROOT"/Tools/sea_show/stand-ins/*.json "$ROOT"/Tools/sea_show/recordings/*.json)
  shopt -u nullglob
  KINDS=()
  for F in "${PLAYED[@]}"; do KINDS+=("$MARKS/$(basename "$F" .json).kinds"); done
  marks_every() {
    local OK=0
    printf '%s\n' "${PLAYED[@]}" | xargs -P "${MARKS_JOBS:-4}" -I{} bash -c 'N="$(basename "$1" .json)"; "$2" "$3" --one "$4/$N.kinds" "$1" > "$4/$N.log" 2>&1' _ {} \
      "$OUT/marks_pool_test" "$SRC" "$MARKS" || OK=1
    for F in "${PLAYED[@]}"; do cat "$MARKS/$(basename "$F" .json).log"; done
    return $OK
  }
  step "marks_pool_test (every recording and stand-in, ${MARKS_JOBS:-4} at once)" marks_every
  step "marks_pool_test" "$OUT/marks_pool_test" "$SRC" --sum "${KINDS[@]}"
fi
echo "written in $OUT"
if [ "$STATUS" -eq 0 ]; then
  echo "SEA WAVES: ALL PASSED"
else
  echo "SEA WAVES: FAILED: ${FAILED[*]}"
fi
exit $STATUS
