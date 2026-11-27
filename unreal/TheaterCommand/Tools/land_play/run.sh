#!/bin/bash
# The ground battle's cloud test: builds the replay, sound, camera, desk and shell-hole cores (the game's own
# Battle/LandFeed, LandPlay, LandSound, LandEye, LandDesk, LandCrater, and the field LandField with LandSight, no engine) and plays a recorded ground battle through them at 60 frames a
# second. Usage: Tools/land_play/run.sh [recording.json ...]  (default: every recording)
# Release 2's small arms off armour (the game's -TheaterLandRicochet, on trial) is off here as in the game unless asked
# for: LAND_PLAY_RICOCHET=1 Tools/land_play/run.sh runs the same recordings with it on and its checks (what is armour,
# the carriers the rules name, every burst at armour off her plate, following her, never inside her, the rattle). Run it
# both ways before a release-2 change to the ricochets: off, every output must be as before but for timings.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_PLAY_OUT:-${TMPDIR:-/tmp}/land_play}"
mkdir -p "$OUT"
CORES="$SRC/Battle/LandJson.cpp $SRC/Battle/LandFeed.cpp $SRC/Battle/LandPlay.cpp $SRC/Battle/LandSound.cpp $SRC/Battle/LandEye.cpp $SRC/Battle/LandGeneralEye.cpp $SRC/Battle/LandDesk.cpp $SRC/Battle/LandCrater.cpp $SRC/Battle/LandField.cpp $SRC/Battle/LandSteppe.cpp $SRC/Battle/LandScape.cpp $SRC/Battle/LandDesert.cpp $SRC/Battle/LandMed.cpp $SRC/Battle/LandTropic.cpp $SRC/Battle/LandFarm.cpp $SRC/Battle/LandSnow.cpp $SRC/Battle/LandDry.cpp $SRC/Battle/LandAsia.cpp $SRC/Battle/LandAmerica.cpp $SRC/Battle/LandPlain.cpp $SRC/Battle/LandSight.cpp $SRC/Battle/LandWalkPan.cpp $SRC/Battle/LandNight.cpp"
CXX="${CXX:-g++}"
$CXX ${LAND_PLAY_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_play_test.cpp" $CORES -o "$OUT/land_play_test"
# The Mac builds with clang and its -Wunreachable-code-aggressive: build with it too where it is here.
CLANG="$(command -v clang++ || ls /usr/bin/clang++-[0-9]* 2>/dev/null | sort -V | tail -n 1 || true)"
if [ -n "$CLANG" ]; then
  "$CLANG" -std=c++20 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -I"$SRC" "$HERE/land_play_test.cpp" $CORES -o "$OUT/land_play_test_clang"
fi
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/LandBattles/*.json; fi
STATUS=0
for F in "$@"; do
  case "$F" in *armies.json) continue ;; esac
  "$OUT/land_play_test" "$F" "$ROOT/Content/Data/Sound/mix.json" "$OUT" || STATUS=1
done
echo "logs in $OUT"
exit $STATUS
