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
CORES="$SRC/Battle/LandJson.cpp $SRC/Battle/LandFeed.cpp $SRC/Battle/LandPlay.cpp $SRC/Battle/LandSound.cpp $SRC/Battle/LandEye.cpp $SRC/Battle/LandGeneralEye.cpp $SRC/Battle/LandDesk.cpp $SRC/Battle/LandCrater.cpp $SRC/Battle/LandField.cpp $SRC/Battle/LandHouse.cpp $SRC/Battle/LandSteppe.cpp $SRC/Battle/LandScape.cpp $SRC/Battle/LandDesert.cpp $SRC/Battle/LandMed.cpp $SRC/Battle/LandTropic.cpp $SRC/Battle/LandFarm.cpp $SRC/Battle/LandSnow.cpp $SRC/Battle/LandDry.cpp $SRC/Battle/LandAsia.cpp $SRC/Battle/LandAmerica.cpp $SRC/Battle/LandPlain.cpp $SRC/Battle/LandSight.cpp $SRC/Battle/LandWalkPan.cpp $SRC/Battle/LandNight.cpp"
CXX="${CXX:-g++}"
$CXX ${LAND_PLAY_FLAGS:-} -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" "$HERE/land_play_test.cpp" $CORES -o "$OUT/land_play_test"
# The Mac builds with clang and its -Wunreachable-code-aggressive: build with it too where it is here.
CLANG="$(command -v clang++ || ls /usr/bin/clang++-[0-9]* 2>/dev/null | sort -V | tail -n 1 || true)"
if [ -n "$CLANG" ]; then
  "$CLANG" -std=c++20 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-aggressive -Werror -I"$SRC" "$HERE/land_play_test.cpp" $CORES -o "$OUT/land_play_test_clang"
fi
if [ "$#" -eq 0 ]; then set -- "$ROOT"/Content/Data/LandBattles/*.json; fi
STATUS=0
# The spotter's mark's distances (Battle/LandSight.h, Finds) are the rules' own (src/battle/land/rules.ts, LAND.hide's sight,
# heard, ambush and forest), and its rings and words stand on the rules' own lines (how the gap between two blocks is
# measured; which kinds go on foot, on wheels, on tracks and as guns, and that a team is a gun block; whose kind sets the
# distance at which a hidden block is seen, and by whom; that tracks alone are heard; who holds his fire in hiding, and
# that guns, mortar teams, tanks and blocks sent to attack or told to fire on a target do not, the last at their whole
# reach; that none fires on a block itself hidden; that a block which has
# fired is struck softer until located, and located once an enemy is within its sight; that smoke hides): the scene has
# no word from the rules for any of them, so they are checked here and the run fails where they differ, or where the
# rules are not there to check against. The ring's own reach (LandSight.h, FindsWithin) is held to the same quarter.
RULES="$ROOT/../../src/battle/land/rules.ts"
SIM="$ROOT/../../src/battle/land/sim.ts"
UNITS="$ROOT/../../src/battle/land/units.ts"
if [ ! -f "$RULES" ] || [ ! -f "$SIM" ] || [ ! -f "$UNITS" ]; then
  # (a tree without the rules beside it: said out loud, and a fault unless LAND_PLAY_NO_RULES=1 says it is meant)
  if [ -n "${LAND_PLAY_NO_RULES:-}" ]; then
    echo "spotter's mark: its distances are NOT checked here (LAND_PLAY_NO_RULES: $RULES, $SIM or $UNITS is not in this tree)"
  else
    echo "FAULT: the spotter's mark's distances are NOT checked: $RULES, $SIM or $UNITS is not there (LAND_PLAY_NO_RULES=1 where that is meant)"
    STATUS=1
  fi
else
  RULE_SIGHT="$(sed -n 's/^ *sight: { foot: \([0-9]*\), wheels: \([0-9]*\), tracks: \([0-9]*\), guns: \([0-9]*\),.*/\1 \4 \2 \3/p' "$RULES" | head -n 1)"
  RULE_HEARD="$(sed -n 's/^ *heard: {.*tracks: \([0-9]*\),.*/\1/p' "$RULES" | head -n 1)"
  RULE_AMBUSH="$(sed -n 's/^ *ambush: { foot: \([0-9]*\), wheels: \([0-9]*\), tracks: 0, guns: 0, fixed: 0, mg: \([0-9]*\) }.*/\1 \2 \3/p' "$RULES" | head -n 1)"
  RULE_FOREST="$(sed -n 's/^ *forest: \([0-9]*\),.*/\1/p' "$RULES" | head -n 1)"
  CORE_SIGHT="$(sed -n 's/^.*double Foot = \([0-9]*\)\.0, Guns = \([0-9]*\)\.0, Vehicles = \([0-9]*\)\.0;.*/\1 \2 \3 \3/p' "$SRC/Battle/LandSight.h" | head -n 1)"
  CORE_HEARD="$(sed -n 's/^.*double Heard = \([0-9]*\)\.0;.*/\1/p' "$SRC/Battle/LandSight.h" | head -n 1)"
  CORE_AMBUSH="$(sed -n 's/^.*double Ambush = \([0-9]*\)\.0, AmbushMg = \([0-9]*\)\.0, AmbushForest = \([0-9]*\)\.0;.*/\1 \1 \2/p' "$SRC/Battle/LandSight.h" | head -n 1)"
  CORE_FOREST="$(sed -n 's/^.*double Ambush = \([0-9]*\)\.0, AmbushMg = \([0-9]*\)\.0, AmbushForest = \([0-9]*\)\.0;.*/\3/p' "$SRC/Battle/LandSight.h" | head -n 1)"
  if [ -z "$RULE_SIGHT" ] || [ -z "$RULE_HEARD" ] || [ "$RULE_SIGHT" != "$CORE_SIGHT" ] || [ "$RULE_HEARD" != "$CORE_HEARD" ]; then
    echo "FAULT: the spotter's mark's distances (LandSight.h Finds: foot guns wheels tracks '$CORE_SIGHT', heard '$CORE_HEARD') are not the rules' (rules.ts LAND.hide: '$RULE_SIGHT', heard '$RULE_HEARD')"
    STATUS=1
  elif [ -z "$RULE_AMBUSH" ] || [ -z "$RULE_FOREST" ] || [ "$RULE_AMBUSH" != "$CORE_AMBUSH" ] || [ "$RULE_FOREST" != "$CORE_FOREST" ]; then
    echo "FAULT: the spotter's mark's ambush distances (LandSight.h Finds: foot wheels mg '$CORE_AMBUSH', forest '$CORE_FOREST') are not the rules' (rules.ts LAND.hide.ambush: '$RULE_AMBUSH', forest '$RULE_FOREST'; tracks, guns and fixed must stay 0)"
    STATUS=1
  else
    echo "spotter's mark: its distances are the rules' (seen: foot guns wheels tracks $RULE_SIGHT m; a moving tank heard at $RULE_HEARD m; men in hiding hold their fire to foot wheels mg $RULE_AMBUSH m, $RULE_FOREST m in forest)"
  fi
  grep -qF "(Math.max(a.w, a.d) + Math.max(b.w, b.d)) / 4" "$SIM" || { echo "FAULT: sim.ts gap() no longer takes a quarter of each block's longer side (LandSight.h FindsWithin, the rings' reach)"; STATUS=1; }
  grep -qF "inf: 'foot', mtn: 'foot', cav: 'foot', eng: 'foot', mot: 'wheels', arm: 'tracks', art: 'guns'" "$RULES" || { echo "FAULT: rules.ts MOTION no longer sorts the kinds as the spotter's rings name them (on foot; guns and teams; tanks and motorised)"; STATUS=1; }
  grep -qF "u.team === 'mg' ? H.ambush.mg : H.ambush[MOTION[u.kind]]" "$SIM" && grep -qF "Math.min(a, H.forest)" "$SIM" || { echo "FAULT: sim.ts ambushAt() no longer holds hidden men's fire as the spotter's mark says (by motion, machine gun teams apart, forest the least)"; STATUS=1; }
  grep -qF "this.gap(u, e) <= sight))) delete u.unloc" "$SIM" || { echo "FAULT: sim.ts spotHidden() no longer locates a block found by its fire once an enemy is within its sight (the spotter's mark's line on one that has fired)"; STATUS=1; }
  grep -qF "if (vague) p *= LAND.hide.locate.fire;" "$SIM" || { echo "FAULT: sim.ts no longer strikes a block found by its fire, not yet located, at the locate share (the spotter's mark's line on one that has fired)"; STATUS=1; }
  grep -qF "const sight = H.sight[MOTION[u.kind]];" "$SIM" || { echo "FAULT: sim.ts spotHidden() no longer takes the sight distance from the hidden block's own kind (the spotter's rings' words)"; STATUS=1; }
  grep -qF "fighting(e) && this.gap(u, e) <= Math.max(sight, heard) && (this.gap(u, e) <= heard || !veiled(e))" "$SIM" || { echo "FAULT: sim.ts spotHidden() no longer sees a hidden block by a fighting enemy within its sight, or heard, and not through smoke (the spotter's rings, and its smoke line)"; STATUS=1; }
  grep -qE "heard: \{ foot: 0, wheels: 0, tracks: [0-9]+, guns: 0, fixed: 0 \}" "$RULES" || { echo "FAULT: rules.ts LAND.hide.heard no longer hears tracks alone (LandSight.h says a lorried block is not heard)"; STATUS=1; }
  grep -qF "if (u.hid && u.order?.kind !== 'attack') reach = Math.min(reach, this.ambushAt(u));" "$SIM" || { echo "FAULT: sim.ts targetOf() no longer holds a hidden block's fire to its ambush distance unless it is sent to attack (the spotter's mark's lines on men lying in wait and on any sent to attack)"; STATUS=1; }
  # (and the other way a hidden block does not wait: one told to fire on a block strikes it at full reach. The line
  # must also stand BEFORE the hold above, or the mark's words on a block given a target would not be true)
  AIM_AT="$(grep -nF "if (u.aim !== undefined) { const a = this.unit(u.aim); if (a && live(a) && a.side !== u.side && !a.hid && this.gap(u, a) <= reach) return a; }" "$SIM" | head -n 1 | cut -d: -f1 || true)"
  HOLD_AT="$(grep -nF "if (u.hid && u.order?.kind !== 'attack') reach = Math.min(reach, this.ambushAt(u));" "$SIM" | head -n 1 | cut -d: -f1 || true)"
  [ -n "$AIM_AT" ] && [ -n "$HOLD_AT" ] && [ "$AIM_AT" -lt "$HOLD_AT" ] || { echo "FAULT: sim.ts targetOf() no longer lets a hidden block told to fire on one block strike it at full reach, before its hold (the spotter's mark's line on any given a target)"; STATUS=1; }
  # (and FULL reach it must be there: the reach is declared whole just above the aim's line, with nothing but a
  # comment between the two, so no hold in other words can have been put in ahead of it)
  REACH_AT="$(grep -nF "let reach = reachOf(u);" "$SIM" | cut -d: -f1 | awk -v a="${AIM_AT:-0}" '$1 < a { r = $1 } END { print r }' || true)"
  BETWEEN=1
  if [ -n "$REACH_AT" ] && [ -n "$AIM_AT" ]; then
    if [ "$((AIM_AT - REACH_AT))" -eq 1 ]; then BETWEEN=0; else BETWEEN="$(sed -n "$((REACH_AT + 1)),$((AIM_AT - 1))p" "$SIM" | grep -cvE '^[[:space:]]*(//.*)?$' || true)"; fi
  fi
  [ "$BETWEEN" = "0" ] || { echo "FAULT: sim.ts targetOf() no longer tests a block's given target against its whole reach (let reach = reachOf(u); straight before the aim's line: the spotter's mark's FULL RANGE for any given a target)"; STATUS=1; }
  grep -qF "if (a <= 0) return Infinity;" "$SIM" || { echo "FAULT: sim.ts ambushAt() no longer lets hidden guns, mortar teams and tanks fire at full reach, in forest too (the spotter's mark's line on guns, mortars and tanks; a mortar team is a gun block with no hold of its own)"; STATUS=1; }
  grep -qF "if (e.side === u.side || !live(e) || e.hid) continue;" "$SIM" || { echo "FAULT: sim.ts targetOf() no longer spares a block that is itself hidden (the spotter's mark: troops in plain sight)"; STATUS=1; }
  [ "$(sed -n '/^export function makeBlocks(/,/^}/p' "$UNITS" | grep -cF "const k = (team ? 'art' :" || true)" -ge 1 ] || { echo "FAULT: units.ts makeBlocks(), which makes each side's blocks, no longer makes a machine gun or mortar team a gun block (the spotter's ring GUNS AND TEAMS)"; STATUS=1; }
  grep -qF "return Sight + 0.25 * (W > D ? W : D);" "$SRC/Battle/LandSight.h" || { echo "FAULT: LandSight.h FindsWithin no longer adds a quarter of the block's longer side, as sim.ts gap() takes it (the rings' reach)"; STATUS=1; }
fi
for F in "$@"; do
  case "$F" in *armies.json) continue ;; esac
  "$OUT/land_play_test" "$F" "$ROOT/Content/Data/Sound/mix.json" "$OUT" || STATUS=1
done
echo "logs in $OUT"
exit $STATUS
