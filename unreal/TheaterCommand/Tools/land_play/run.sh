#!/bin/bash
# The ground battle's cloud test: builds the replay, sound, camera, desk and shell-hole cores (the game's own
# Battle/LandFeed, LandPlay, LandSound, LandEye, LandDesk, LandCrater, and the field LandField with LandSight, no engine) and plays a recorded ground battle through them at 60 frames a
# second. Usage: Tools/land_play/run.sh [recording.json ...]  (default: every recording)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${LAND_PLAY_OUT:-${TMPDIR:-/tmp}/land_play}"
mkdir -p "$OUT"
CORES="$SRC/Battle/LandJson.cpp $SRC/Battle/LandFeed.cpp $SRC/Battle/LandPlay.cpp $SRC/Battle/LandSound.cpp $SRC/Battle/LandEye.cpp $SRC/Battle/LandGeneralEye.cpp $SRC/Battle/LandDesk.cpp $SRC/Battle/LandCrater.cpp $SRC/Battle/LandField.cpp $SRC/Battle/LandHouse.cpp $SRC/Battle/LandSteppe.cpp $SRC/Battle/LandScape.cpp $SRC/Battle/LandDesert.cpp $SRC/Battle/LandMed.cpp $SRC/Battle/LandTropic.cpp $SRC/Battle/LandFarm.cpp $SRC/Battle/LandSnow.cpp $SRC/Battle/LandDry.cpp $SRC/Battle/LandAsia.cpp $SRC/Battle/LandAmerica.cpp $SRC/Battle/LandPlain.cpp $SRC/Battle/LandCity.cpp $SRC/Battle/LandSight.cpp $SRC/Battle/LandWalkPan.cpp $SRC/Battle/LandNight.cpp"
CXX="${CXX:-g++}"
$CXX ${LAND_PLAY_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_play_test.cpp" $CORES -o "$OUT/land_play_test"
# The Mac builds with clang and its -Wunreachable-code-aggressive: build with it too where it is here.
CLANG="$(command -v clang++ || ls /usr/bin/clang++-[0-9]* 2>/dev/null | sort -V | tail -n 1 || true)"
if [ -n "$CLANG" ]; then
  "$CLANG" -std=c++20 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -I"$SRC" "$HERE/land_play_test.cpp" $CORES -o "$OUT/land_play_test_clang"
fi
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/LandBattles/*.json; fi
STATUS=0
# The spotter's mark's distances (Battle/LandSight.h, Finds) are the rules' own (src/battle/land/rules.ts, LAND.hide's sight
# and heard): the scene has no word from the rules for them, so they are checked here and the run fails where they differ.
RULES="$ROOT/../../src/battle/land/rules.ts"
if [ -f "$RULES" ]; then
  RULE_SIGHT="$(sed -n 's/^ *sight: { foot: \([0-9]*\), wheels: \([0-9]*\), tracks: \([0-9]*\), guns: \([0-9]*\),.*/\1 \4 \2 \3/p' "$RULES" | head -n 1)"
  RULE_HEARD="$(sed -n 's/^ *heard: {.*tracks: \([0-9]*\),.*/\1/p' "$RULES" | head -n 1)"
  CORE_SIGHT="$(sed -n 's/^.*double Foot = \([0-9]*\)\.0, Guns = \([0-9]*\)\.0, Vehicles = \([0-9]*\)\.0;.*/\1 \2 \3 \3/p' "$SRC/Battle/LandSight.h" | head -n 1)"
  CORE_HEARD="$(sed -n 's/^.*double Heard = \([0-9]*\)\.0;.*/\1/p' "$SRC/Battle/LandSight.h" | head -n 1)"
  if [ -z "$RULE_SIGHT" ] || [ "$RULE_SIGHT" != "$CORE_SIGHT" ] || [ "$RULE_HEARD" != "$CORE_HEARD" ]; then
    echo "FAULT: the spotter's mark's distances (LandSight.h Finds: foot guns wheels tracks '$CORE_SIGHT', heard '$CORE_HEARD') are not the rules' (rules.ts LAND.hide: '$RULE_SIGHT', heard '$RULE_HEARD')"
    STATUS=1
  else
    echo "spotter's mark: its distances are the rules' (foot guns wheels tracks $RULE_SIGHT m, a moving tank heard at $RULE_HEARD m)"
  fi
fi
for F in "$@"; do
  case "$F" in *armies.json) continue ;; esac
  "$OUT/land_play_test" "$F" "$ROOT/Content/Data/Sound/mix.json" "$OUT" || STATUS=1
done
echo "logs in $OUT"
exit $STATUS
