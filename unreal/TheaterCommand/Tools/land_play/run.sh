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
# measured, held whole, and that standing in forest is the field's word 'woods' at the block's middle and nothing else; which kinds go on foot, on wheels, on tracks and as guns, and that a team is a gun block; whose kind sets the
# distance at which a hidden block is seen, and by whom; that tracks alone are heard; who holds his fire in hiding, and
# that guns, mortar teams, tanks and blocks sent to attack or told to fire on a target do not, the last at their whole
# reach; that none fires on a block itself hidden; that each kind which holds reaches as far as its hold (cavalry and
# engineers ten metres short, as LandSight.h says); that a block which has
# fired is struck softer until located, and located once an enemy is within its sight; that smoke hides): the scene has
# no word from the rules for any of them, so they are checked here and the run fails where they differ, or where the
# rules are not there to check against. The ring's own reach (LandSight.h, FindsWithin) is held to the same quarter.
# (Everything below reads CODE ALONE: every file is first copied with its comments struck, a /* */ block's text and a
# // comment's to its line's end, and only the comment: the code beside one stays where it stood, each line on its own
# line, trailing blanks dropped. So an old line or a whole old function kept in a comment cannot stand in for the
# rule, a comment set beside a changed rule cannot make it read as the old one, and a comment's own words can neither
# trip a pin nor pass one. A quote's text, '...', "..." or `...`, is left as it is to the end of its line. Not
# known: a quote that runs over lines, and a regex that holds a quote or a comment's opening. None stands in the
# files read today; one would leave comments unstruck after it on its line or strike code as a comment.)
code_of() {
  tr -d '\r' < "$1" | awk '
    BEGIN { st = 0; q = "" }
    {
      out = ""; n = length($0); i = 1
      while (i <= n) {
        c = substr($0, i, 1); d = substr($0, i, 2)
        if (st == 1) { if (d == "*/") { st = 0; i += 2 } else i++; continue }
        if (st == 2) {
          out = out c
          if (c == "\\") { out = out substr($0, i + 1, 1); i += 2; continue }
          if (c == q) st = 0
          i++; continue
        }
        if (d == "/*") { st = 1; i += 2; continue }
        if (d == "//") break
        if (c == "\"" || c == "\047" || c == "`") { st = 2; q = c }
        out = out c; i++
      }
      if (st == 2) st = 0
      sub(/[ \t]+$/, "", out)
      print out
    }'
}
CODE="$OUT/code_alone"
mkdir -p "$CODE"
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
  code_of "$RULES" > "$CODE/rules.ts"
  code_of "$SIM" > "$CODE/sim.ts"
  code_of "$UNITS" > "$CODE/units.ts"
  RULES="$CODE/rules.ts"
  SIM="$CODE/sim.ts"
  UNITS="$CODE/units.ts"
  code_of "$SRC/Battle/LandSight.h" > "$CODE/LandSight.h"
  RULE_SIGHT="$(sed -n 's/^ *sight: { foot: \([0-9]*\), wheels: \([0-9]*\), tracks: \([0-9]*\), guns: \([0-9]*\),.*/\1 \4 \2 \3/p' "$RULES" | head -n 1)"
  RULE_HEARD="$(sed -n 's/^ *heard: {.*tracks: \([0-9]*\),.*/\1/p' "$RULES" | head -n 1)"
  RULE_AMBUSH="$(sed -n 's/^ *ambush: { foot: \([0-9]*\), wheels: \([0-9]*\), tracks: 0, guns: 0, fixed: 0, mg: \([0-9]*\) }.*/\1 \2 \3/p' "$RULES" | head -n 1)"
  RULE_FOREST="$(sed -n 's/^ *forest: \([0-9]*\),.*/\1/p' "$RULES" | head -n 1)"
  CORE_SIGHT="$(sed -n 's/^.*double Foot = \([0-9]*\)\.0, Guns = \([0-9]*\)\.0, Vehicles = \([0-9]*\)\.0;.*/\1 \2 \3 \3/p' "$CODE/LandSight.h" | head -n 1)"
  CORE_HEARD="$(sed -n 's/^.*double Heard = \([0-9]*\)\.0;.*/\1/p' "$CODE/LandSight.h" | head -n 1)"
  CORE_AMBUSH="$(sed -n 's/^.*double Ambush = \([0-9]*\)\.0, AmbushMg = \([0-9]*\)\.0, AmbushForest = \([0-9]*\)\.0;.*/\1 \1 \2/p' "$CODE/LandSight.h" | head -n 1)"
  CORE_FOREST="$(sed -n 's/^.*double Ambush = \([0-9]*\)\.0, AmbushMg = \([0-9]*\)\.0, AmbushForest = \([0-9]*\)\.0;.*/\3/p' "$CODE/LandSight.h" | head -n 1)"
  if [ -z "$RULE_SIGHT" ] || [ -z "$RULE_HEARD" ] || [ "$RULE_SIGHT" != "$CORE_SIGHT" ] || [ "$RULE_HEARD" != "$CORE_HEARD" ]; then
    echo "FAULT: the spotter's mark's distances (LandSight.h Finds: foot guns wheels tracks '$CORE_SIGHT', heard '$CORE_HEARD') are not the rules' (rules.ts LAND.hide: '$RULE_SIGHT', heard '$RULE_HEARD')"
    STATUS=1
  elif [ -z "$RULE_AMBUSH" ] || [ -z "$RULE_FOREST" ] || [ "$RULE_AMBUSH" != "$CORE_AMBUSH" ] || [ "$RULE_FOREST" != "$CORE_FOREST" ]; then
    echo "FAULT: the spotter's mark's ambush distances (LandSight.h Finds: foot wheels mg '$CORE_AMBUSH', forest '$CORE_FOREST') are not the rules' (rules.ts LAND.hide.ambush: '$RULE_AMBUSH', forest '$RULE_FOREST'; tracks, guns and fixed must stay 0)"
    STATUS=1
  else
    echo "spotter's mark: its distances are the rules' (seen: foot guns wheels tracks $RULE_SIGHT m; a moving tank heard at $RULE_HEARD m; men in hiding hold their fire to foot wheels mg $RULE_AMBUSH m, $RULE_FOREST m in forest)"
  fi
  grep -qF "inf: 'foot', mtn: 'foot', cav: 'foot', eng: 'foot', mot: 'wheels', arm: 'tracks', art: 'guns'" "$RULES" || { echo "FAULT: rules.ts MOTION no longer sorts the kinds as the spotter's rings name them (on foot; guns and teams; tanks and motorised)"; STATUS=1; }
  grep -qF "this.gap(u, e) <= sight))) delete u.unloc" "$SIM" || { echo "FAULT: sim.ts spotHidden() no longer locates a block found by its fire once an enemy is within its sight (the spotter's mark's line on one that has fired)"; STATUS=1; }
  grep -qF "if (vague) p *= LAND.hide.locate.fire;" "$SIM" || { echo "FAULT: sim.ts no longer strikes a block found by its fire, not yet located, at the locate share (the spotter's mark's line on one that has fired)"; STATUS=1; }
  grep -qF "const sight = H.sight[MOTION[u.kind]];" "$SIM" || { echo "FAULT: sim.ts spotHidden() no longer takes the sight distance from the hidden block's own kind (the spotter's rings' words)"; STATUS=1; }
  grep -qF "fighting(e) && this.gap(u, e) <= Math.max(sight, heard) && (this.gap(u, e) <= heard || !veiled(e))" "$SIM" || { echo "FAULT: sim.ts spotHidden() no longer sees a hidden block by a fighting enemy within its sight, or heard, and not through smoke (the spotter's rings, and its smoke line)"; STATUS=1; }
  grep -qE "heard: \{ foot: 0, wheels: 0, tracks: [0-9]+, guns: 0, fixed: 0 \}" "$RULES" || { echo "FAULT: rules.ts LAND.hide.heard no longer hears tracks alone (LandSight.h says a lorried block is not heard)"; STATUS=1; }
  # (The lines of sim.ts that say who holds his fire in hiding are held WHOLE, inside their own function and in their
  # order: a search for the words alone would pass a hold added on the same line or the same words in another
  # function. A function's own lines of code, indents and blank lines dropped; the function itself is found by its
  # head and its closing brace at the class's own two spaces, so a re-indent of the file reads here as a changed
  # rule.)
  body_of() { sed -n "/^  $1/,/^  }/p" "$SIM" | sed -e 's/^[[:space:]]*//' | grep -v '^$' || true; }
  # (ambushAt, all of it: by how the block moves, the machine gun team alone of the teams with a hold of its own, no
  # hold at all where the table gives none (guns, mortar teams, tanks: Infinity), and in true forest the least)
  AMBUSH_WANT="private ambushAt(u: LandUnit): number {
const H = LAND.hide, a = u.team === 'mg' ? H.ambush.mg : H.ambush[MOTION[u.kind]];
if (a <= 0) return Infinity;
return !this.onHedge(u.x, u.y) && this.inWoods(u) ? Math.min(a, H.forest) : a;
}"
  [ "$(body_of 'private ambushAt(u: LandUnit): number {')" = "$AMBUSH_WANT" ] || { echo "FAULT: sim.ts ambushAt() is no longer, line for line, the hold the spotter's mark gives (by how the block moves, machine gun teams apart, none for guns, mortar teams and tanks, forest the least), or the file's indent has changed. If the rule itself is unchanged bring this pin up to date; if it has changed, the mark's legend (LandBattleScreen.cpp) and LandSight.h must change with it"; STATUS=1; }
  # (and the two helpers that ambushAt and targetOf measure with: gap(), whole, the metres between two blocks' edges
  # that every hold, reach and ring is measured in; and inWoods()'s one line, that "in forest" is the field's word
  # 'woods' where the block's middle stands. A factor on gap() for a hidden block, or inWoods() asking for more
  # than woods, would change the distances the mark gives and leave ambushAt's and targetOf's own lines as they
  # are. NOT held: which ground the field calls 'woods'. That is field.ts, coverAt() and the field's laying, which
  # this guard does not read: fieldworks dug in a wood count as woods there today, and a change there that made
  # more ground woods would pass here.)
  GAP_WANT="gap(a: LandUnit, b: LandUnit): number {
return Math.max(0, dist(a.x, a.y, b.x, b.y) - (Math.max(a.w, a.d) + Math.max(b.w, b.d)) / 4);
}"
  [ "$(body_of 'gap(a: LandUnit, b: LandUnit): number {')" = "$GAP_WANT" ] || { echo "FAULT: sim.ts gap() is no longer, line for line, the metres between two blocks' edges less a quarter of each one's longer side (every hold, reach and ring of the spotter's mark is measured in it; LandSight.h FindsWithin), or the file's indent has changed. If the rule itself is unchanged bring this pin up to date; if it has changed, LandSight.h and the mark's legend must change with it"; STATUS=1; }
  WOODS_WANT="private inWoods(u: LandUnit): boolean { return coverAt(this.field, u.x, u.y) === 'woods'; }"
  [ "$(sed -e 's/^[[:space:]]*//' "$SIM" | grep -cxF "$WOODS_WANT" || true)" = "1" ] || { echo "FAULT: sim.ts inWoods() is no longer standing in woods and nothing else (the spotter's mark's IN FOREST, where the hold is the least). If the rule itself is unchanged bring this pin up to date; if it has changed, the mark's legend (LandBattleScreen.cpp) and LandSight.h must change with it"; STATUS=1; }
  # (targetOf, all of it, line for line as ambushAt is held: the block's whole reach; straight after it a given target
  # tested against that reach; only then a hidden block's hold, unless it is sent to attack; none struck that is
  # itself hidden; and the two lines that put the held reach to use, in the search for a target and in firing on
  # cover. So a hidden block sent to attack or given a target fires at its whole reach, and no other hold can stand
  # in targetOf beside the one the mark's words give. What is held is these functions' own lines, gap(),
  # inWoods()'s line and (below) reachOf(), and no more. What those lines call is not held: live(), onHedge() and
  # this.unit() in sim.ts, dist() in core/geometry.ts, coverAt() in field.ts. A hold written into one of them, or
  # into whatever calls targetOf, a factor in dist(), or more ground called woods by coverAt(), would not be seen
  # here.)
  TARGET_WANT="targetOf(u: LandUnit): LandUnit | null {
const marching = u.order && u.order.kind !== 'attack' && u.path.length > 0;
if (u.kind === 'art' && marching) return null;
if (u.order?.kind === 'withdraw') return null;
let reach = reachOf(u);
if (u.aim !== undefined) { const a = this.unit(u.aim); if (a && live(a) && a.side !== u.side && !a.hid && this.gap(u, a) <= reach) return a; }
if (u.hid && u.order?.kind !== 'attack') reach = Math.min(reach, this.ambushAt(u));
let best: LandUnit | null = null, score = Infinity;
for (const e of this.units) {
if (e.side === u.side || !live(e) || e.hid) continue;
const d = this.gap(u, e);
if (d > reach) continue;
if (u.order?.kind === 'attack' && u.order.target === e.id) return e;
const off = Math.abs(turn(u.face, bearing(u.x, u.y, e.x, e.y)));
const s = d * (1 + off) * (e.state === 'routing' ? 2 : 1);
if (s < score) { score = s; best = e; }
}
if (!best && u.search && this.hide) {
const [sx, sy] = u.search, R = u.searchR ?? LAND.hide.search.reach;
for (const e of this.units) {
if (e.side === u.side || !live(e) || !e.hid || dist(e.x, e.y, sx, sy) > R + Math.max(e.w, e.d) / 2) continue;
const d = this.gap(u, e);
if (d <= reach && d < score) { score = d; best = e; }
}
}
return best;
}"
  # (Two forms of targetOf are held, each whole. The second is the ground thread's since 25 November (support teams
  # in battle, behind `teams`, off): two lines more before the search and six in it, by which a machine gun or
  # mortar team takes, of the enemies ALREADY within its reach, the one its weapon harms most, where every other
  # block takes the nearest. The reach, the given target, the hidden block's hold and the hidden never struck are
  # the same lines in both, in the same places, so the mark's words stand on either. A tree has one or the other:
  # the first goes from here once no tree to be merged still has it.)
  TARGET_WANT_TEAMS="targetOf(u: LandUnit): LandUnit | null {
const marching = u.order && u.order.kind !== 'attack' && u.path.length > 0;
if (u.kind === 'art' && marching) return null;
if (u.order?.kind === 'withdraw') return null;
let reach = reachOf(u);
if (u.aim !== undefined) { const a = this.unit(u.aim); if (a && live(a) && a.side !== u.side && !a.hid && this.gap(u, a) <= reach) return a; }
if (u.hid && u.order?.kind !== 'attack') reach = Math.min(reach, this.ambushAt(u));
let best: LandUnit | null = null, score = Infinity;
const T = this.teamsRule && ownArm(u) ? LAND.teams[u.team] : null;
let harm = 0;
for (const e of this.units) {
if (e.side === u.side || !live(e) || e.hid) continue;
const d = this.gap(u, e);
if (d > reach) continue;
if (u.order?.kind === 'attack' && u.order.target === e.id) return e;
const off = Math.abs(turn(u.face, bearing(u.x, u.y, e.x, e.y)));
const s = d * (1 + off) * (e.state === 'routing' ? 2 : 1);
if (T) {
const h = (e.kind === 'arm' ? T.armour : 1) * (d < T.closeAt ? T.close : 1);
if (h < harm - 1e-9 || (h <= harm + 1e-9 && s >= score)) continue;
harm = h; score = s; best = e;
continue;
}
if (s < score) { score = s; best = e; }
}
if (!best && u.search && this.hide) {
const [sx, sy] = u.search, R = u.searchR ?? LAND.hide.search.reach;
for (const e of this.units) {
if (e.side === u.side || !live(e) || !e.hid || dist(e.x, e.y, sx, sy) > R + Math.max(e.w, e.d) / 2) continue;
const d = this.gap(u, e);
if (d <= reach && d < score) { score = d; best = e; }
}
}
return best;
}"
  TARGET_IS="$(body_of 'targetOf(u: LandUnit): LandUnit | null {')"
  [ "$TARGET_IS" = "$TARGET_WANT" ] || [ "$TARGET_IS" = "$TARGET_WANT_TEAMS" ] || { echo "FAULT: sim.ts targetOf() is no longer, line for line, the rule the spotter's mark's words stand on (the block's whole reach; its given target tested against that reach; only then a hidden block's hold unless it is sent to attack; none struck that is itself hidden; the held reach put to use in the search and in firing on cover: MEN LYING IN WAIT, TROOPS IN PLAIN SIGHT, FULL RANGE), or the file's indent has changed. If the rule itself is unchanged bring this pin up to date; if it has changed, the mark's legend (LandBattleScreen.cpp) and LandSight.h must change with it"; STATUS=1; }
  REACH_WANT="export const reachOf = (u: { kind: Kind; team?: Team }): number => (u.team ? LAND.teams[u.team].reach : LAND.reach[u.kind]);"
  [ "$(grep -cxF "$REACH_WANT" "$RULES" || true)" = "1" ] || { echo "FAULT: rules.ts reachOf() is no longer a block's whole reach by its kind and team alone (the spotter's mark's FULL RANGE)"; STATUS=1; }
  # (a hold is only the distance the mark's words give where the block's own reach is no less: men on foot and
  # motorised, and the machine gun team, against their holds. Cavalry and engineers reach ten metres short of
  # theirs as the rules stand, 140 m against 150, so hidden they open fire at 140: LandSight.h says so, and no
  # more than those ten metres is let by here.)
  # (each read from its own table and nowhere else: LAND.reach's row by its "as Record<Kind, number>" end, the
  # machine gun team's inside LAND.teams; and exactly one of each, so that no other table that reads alike is
  # measured in its place)
  REACH_ROW="$(sed -n 's/^ *reach: { inf: \([0-9]*\), mtn: \([0-9]*\), cav: \([0-9]*\), eng: \([0-9]*\), mot: \([0-9]*\), arm: [0-9]*, art: [0-9]*, fort: [0-9]* } as Record<Kind, number>,.*/\1 \2 \3 \4 \5/p' "$RULES")"
  REACH_MG="$(sed -n '/^  teams: {/,/^  }/s/^ *mg: { reach: \([0-9]*\),.*/\1/p' "$RULES")"
  REACH_ROWS="$(printf '%s\n' "$REACH_ROW" | grep -c . || true)"
  REACH_MGS="$(printf '%s\n' "$REACH_MG" | grep -c . || true)"
  REACH_FORM=1
  REACH_LEFT="whether each kind that holds its fire in hiding still reaches as far as its hold is NOT checked. Read the holds against the reaches again and bring this pin up to date"
  [ "$REACH_ROWS" = "1" ] || { echo "FAULT: rules.ts LAND.reach's row was read $REACH_ROWS times, not once (read: '$REACH_ROW'). The form this check reads, comments apart: one line, indented with spaces, 'reach: { inf: N, mtn: N, cav: N, eng: N, mot: N, arm: N, art: N, fort: N } as Record<Kind, number>,' with one space after each colon and comma, plain digits, and that ending. It is no longer so, or another table reads alike: $REACH_LEFT"; STATUS=1; REACH_FORM=0; }
  [ "$REACH_MGS" = "1" ] || { echo "FAULT: rules.ts LAND.teams' machine gun row was read $REACH_MGS times, not once (read: '$REACH_MG'). The form this check reads, comments apart: inside the block that opens with a line of exactly two spaces and 'teams: {', one line, indented with spaces, beginning 'mg: { reach: N,' in plain digits. It is no longer so, or there are two: $REACH_LEFT"; STATUS=1; REACH_FORM=0; }
  [ -n "$RULE_AMBUSH" ] || { echo "FAULT: rules.ts LAND.hide.ambush could not be read (its form: 'ambush: { foot: N, wheels: N, tracks: 0, guns: 0, fixed: 0, mg: N }' on one line): $REACH_LEFT"; STATUS=1; REACH_FORM=0; }
  if [ "$REACH_FORM" = "1" ]; then
    REACH_OK=0
    read -r R_INF R_MTN R_CAV R_ENG R_MOT <<<"$REACH_ROW"
    read -r A_FOOT A_WHEELS A_MG <<<"$RULE_AMBUSH"
    if [ "$R_INF" -ge "$A_FOOT" ] && [ "$R_MTN" -ge "$A_FOOT" ] && [ "$R_MOT" -ge "$A_WHEELS" ] && [ "$REACH_MG" -ge "$A_MG" ] \
      && [ "$R_CAV" -ge "$((A_FOOT - 10))" ] && [ "$R_ENG" -ge "$((A_FOOT - 10))" ]; then REACH_OK=1; fi
    [ "$REACH_OK" = "1" ] || { echo "FAULT: a kind that holds its fire in hiding now reaches less far than its hold (rules.ts LAND.reach foot and motorised kinds '$REACH_ROW', the machine gun team '$REACH_MG', against LAND.hide.ambush '$RULE_AMBUSH'; cavalry and engineers are let ten metres short), so the spotter's mark's distances are no longer where it opens fire: the mark's legend (LandBattleScreen.cpp) and LandSight.h must change with it"; STATUS=1; }
  fi
  [ "$(sed -n '/^export function makeBlocks(/,/^}/p' "$UNITS" | grep -cF "const k = (team ? 'art' :" || true)" -ge 1 ] || { echo "FAULT: units.ts makeBlocks(), which makes each side's blocks, no longer makes a machine gun or mortar team a gun block (the spotter's ring GUNS AND TEAMS)"; STATUS=1; }
  grep -qF "return Sight + 0.25 * (W > D ? W : D);" "$CODE/LandSight.h" || { echo "FAULT: LandSight.h FindsWithin no longer adds a quarter of the block's longer side, as sim.ts gap() takes it (the rings' reach)"; STATUS=1; }
fi
# (The camera's dark rule reads the drawn picture through the scene, which is not built here. WHEN a frame is asked
# for and which frames are read is LandEye::DarkReader's, plain code that the test below builds and drives as the
# scene does (its film passes and its bench); what no run here can tell is whether the scene makes the reader's calls
# as the test makes them. So those lines of LandBattle.cpp are held, in the file's code alone:
# in ReadDark(), each once and in this order, the reader's clock handed the screen's clock, the wall's, whether the
# reading is wanted and the director's three answers; every frame come back that carries its ask handed on with the
# time that ask carried (the loop over the frames left only by the one 'continue;' for a frame with no ask, and the
# payload, FDarkAsk, keeping the time it is made with); the ask, made only when the reader says so and stamped with
# the same screen's clock; and the line that says when
# a reading is wanted (not while a look aside, the stills' walk, a card, the general's own view or the night's light
# is on the picture, nor in a shot the rule does not judge). That the reader's Tick, Back, Ask, Resized, Begin and
# Built are called nowhere else in the file (and the reader named no more often than its calls and its read
# answers come to, its type's name 'LandEye::DarkReader' apart, so none is made through a pointer or a reference to
# it, however the member is spelt out), and the grabber asked nowhere else; that it is never stopped in
# ReadDark() (stopped between asks it does not start again: it is simply not asked) and started only where it is
# bound; that the rule is given the reader's last reading; that the reading stays off unless asked for; and in the
# tick, that ReadDark() is called once, after the line that updates the film's eye with the same screen's clock.
# And that LandBattle.h, which holds the reader by value, takes LandEye.h itself: the campaign's files take that
# header with no other of the battle's, and without it they do not compile (26 November's review; no run here
# builds them).
# The test's own film pass is held to the same call. Not held: the rest of these functions, the frame grabber, the
# share of a frame that is dark; and what the engine's preprocessor makes of the lines, which are read here as text.
# Through release 1.5.1 the scene kept the reading's clock itself, measured it against a frame's length, asked for a
# single frame as the battle began and the rule saw nothing all battle: the clock is the reader's now, and tested.)
code_of "$SRC/Battle/LandBattle.cpp" > "$CODE/LandBattle.cpp"
code_of "$ROOT/Tools/land_play/land_play_test.cpp" > "$CODE/land_play_test.cpp"
flat() { sed -e 's/^[[:space:]]*//' | grep -v '^$' || true; }
DARK_BODY="$(sed -n '/^void ALandBattle::ReadDark()/,/^}/p' "$CODE/LandBattle.cpp" | flat)"
DARK_CALLS="$(printf '%s\n' "$DARK_BODY" | grep -E '(^|[^[:alnum:]_])DarkReader\.(Tick|Back|Ask|Resized|Begin|Built)\(|CaptureThisFrame\(|GetCapturedFrames\(|GetPayload<' || true)"
DARK_CALLS_WANT="if (DarkReader.Resized(Size.X, Size.Y))
DarkReader.Tick(S.Real, LandParts::Seconds(), bWanted, S.Eye.ShotBegan(), S.Eye.DarkCanLeave(), S.Eye.DarkRun() > 0);
TArray<FCapturedFrameData> Frames = DarkGrab->GetCapturedFrames();
const FDarkAsk* Asked = Frame.Payload.IsValid() ? Frame.GetPayload<FDarkAsk>() : nullptr;
DarkReader.Back(Asked->At, Share);
if (DarkGrab->IsCapturingFrames() && DarkReader.Ask())
DarkGrab->CaptureThisFrame(MakeShared<FDarkAsk, ESPMode::ThreadSafe>(S.Real));"
[ "$DARK_CALLS" = "$DARK_CALLS_WANT" ] \
  || { echo "FAULT: LandBattle.cpp ReadDark() no longer makes the dark reader's calls as they were held, each once and in this order (the size asked; 'DarkReader.Tick(S.Real, LandParts::Seconds(), bWanted, S.Eye.ShotBegan(), S.Eye.DarkCanLeave(), S.Eye.DarkRun() > 0);'; the frames taken; each one's ask read from its payload; 'DarkReader.Back(Asked->At, Share);'; 'if (DarkGrab->IsCapturingFrames() && DarkReader.Ask())'; the ask stamped 'MakeShared<FDarkAsk, ESPMode::ThreadSafe>(S.Real)'. Read: '$(printf '%s' "$DARK_CALLS" | tr '\n' ' ')'): in another order, on another clock or with another stamp the reader reads the last shot's picture as this one's, or nothing, and no run here can tell. If the lines have only been written anew bring this pin up to date, and the test's film pass with them"; STATUS=1; }
DARK_ASK_WANT="struct FDarkAsk : IFramePayload
{
double At = 0.0;
explicit FDarkAsk(double InAt) : At(InAt) {}
};"
[ "$(sed -n '/^[[:space:]]*struct FDarkAsk : IFramePayload$/,/^[[:space:]]*};$/p' "$CODE/LandBattle.cpp" | flat)" = "$DARK_ASK_WANT" ] \
  && [ "$(tr -c '[:alnum:]_' '\n' < "$CODE/LandBattle.cpp" | grep -cx 'FDarkAsk' || true)" = "5" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -cxF 'for (FCapturedFrameData& Frame : Frames)' || true)" = "1" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -xF -A 1 'TArray<FCapturedFrameData> Frames = DarkGrab->GetCapturedFrames();' | tail -n 1 || true)" = "for (FCapturedFrameData& Frame : Frames)" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | tr -c '[:alnum:]_' '\n' | grep -cx 'Frames' || true)" = "2" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -cE '(^|[^[:alnum:]_])continue([^[:alnum:]_]|$)' || true)" = "1" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -cE '(^|[^[:alnum:]_])(break|goto)([^[:alnum:]_]|$)' || true)" = "0" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -cE '(^|[^[:alnum:]_])return([^[:alnum:]_]|$)' || true)" = "2" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -xF -B 1 'DarkReader.Back(Asked->At, Share);' | head -n 1 || true)" = "}" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -xF -A 3 'if (!Asked)' | tr '\n' ' ' || true)" = "if (!Asked) { continue; } " ] \
  || { echo "FAULT: LandBattle.cpp no longer carries an ask's time to its frame and back as it was held (the payload 'struct FDarkAsk : IFramePayload { double At = 0.0; explicit FDarkAsk(double InAt) : At(InAt) {} };', named five times in the file; in ReadDark() one loop 'for (FCapturedFrameData& Frame : Frames)' straight after the frames are taken and over all of them, left only by the one 'continue;' for a frame that carries no ask, no 'break', and 'return;' only in the two places before the reader's Tick; 'DarkReader.Back(Asked->At, Share);' standing after the share's own block, not inside it): a time altered on the way, or a frame not handed on, and the reader reads another shot's picture, or none, with every run here passing. If the lines have only been written anew bring this pin up to date"; STATUS=1; }
[ "$(grep -cxF '#include "Battle/LandEye.h"' "$SRC/Battle/LandBattle.h" || true)" = "1" ] \
  && [ "$(grep -nxF '#include "Battle/LandEye.h"' "$SRC/Battle/LandBattle.h" | cut -d: -f1 || true)" -lt "$(grep -nxF '#include "LandBattle.generated.h"' "$SRC/Battle/LandBattle.h" | cut -d: -f1 || true)" ] \
  && [ "$(grep -cE '^[[:space:]]*LandEye::DarkReader DarkReader;$' "$SRC/Battle/LandBattle.h" || true)" = "1" ] \
  || { echo "FAULT: LandBattle.h no longer takes LandEye.h itself, before its generated header ('#include \"Battle/LandEye.h\"', once), or no longer holds the dark reader by value ('LandEye::DarkReader DarkReader;'): the header names LandEye::DarkReader and LandEye::Settings, and the campaign's files (CampaignGameMode.cpp, CampaignHUD.cpp, CampaignPlayerController.cpp) and the generated code take it without LandBattleState.h, so without the line they do not compile on the Mac, and nothing here builds them. If the reader is held another way now bring this pin up to date"; STATUS=1; }
DARK_WANTED_WANT="const bool bWanted = FMath::Abs(LookAside.X) <= 1.0 && FMath::Abs(LookAside.Y) <= 1.0 && !bWalkPanOn && !S.bCard && !(S.bGeneralEye && !S.General.bFilm) && !S.NightNow.bNight && S.NightNow.Dark <= 0.0 && S.NightNow.Black <= 0.0 && S.Eye.DarkJudged(); "
[ "$(printf '%s\n' "$DARK_BODY" | grep -F -A 2 "const bool bWanted" | tr '\n' ' ' || true)" = "$DARK_WANTED_WANT" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -cE '(^|[^[:alnum:]_])bWanted([^[:alnum:]_]|$)' || true)" = "2" ] \
  || { echo "FAULT: LandBattle.cpp ReadDark() no longer says when a dark reading is wanted as it was held (no look aside over a degree, not the stills' walk, no card, not the general's own view, not the night or its light, and a shot the rule judges; set once and handed to the reader's Tick alone): read while his own view or a card is the picture, the film leaves shots for a darkness that is not theirs. If the line has only been written anew bring this pin up to date"; STATUS=1; }
dark_count() { grep -cE "$1" "$CODE/LandBattle.cpp" || true; }
[ "$(dark_count '(^|[^[:alnum:]_])DarkReader\.Tick\(')" = "1" ] && [ "$(dark_count '(^|[^[:alnum:]_])DarkReader\.Back\(')" = "1" ] && [ "$(dark_count '(^|[^[:alnum:]_])DarkReader\.Ask\(')" = "1" ] \
  && [ "$(dark_count '(^|[^[:alnum:]_])DarkReader\.Resized\(')" = "1" ] && [ "$(dark_count '(^|[^[:alnum:]_])DarkReader\.Begin\(')" = "1" ] && [ "$(dark_count '(^|[^[:alnum:]_])DarkReader\.Built\(')" = "1" ] \
  && [ "$(dark_count 'CaptureThisFrame\(')" = "1" ] && [ "$(dark_count 'StartCapturingFrames\(')" = "1" ] && [ "$(dark_count 'StopCapturingFrames\(')" = "2" ] \
  && [ "$(sed -e 's/LandEye::DarkReader//g' "$CODE/LandBattle.cpp" | tr -c '[:alnum:]_' '\n' | grep -cx 'DarkReader' || true)" = "18" ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -cE 'StopCapturingFrames\(|StartCapturingFrames\(|Shutdown\(' || true)" = "0" ] \
  && [ "$(sed -n '/^void ALandBattle::StartDarkRead(const LandEye::Settings& Rule)/,/^}/p' "$CODE/LandBattle.cpp" | flat | grep -cxF "DarkReader.Begin(Pace);" || true)" = "1" ] \
  && [ "$(sed -n '/^void ALandBattle::BindDark()/,/^}/p' "$CODE/LandBattle.cpp" | flat | grep -cE '^(DarkGrab->StartCapturingFrames\(\);|DarkReader\.Built\(Size\.X, Size\.Y\);|DarkGrab->StopCapturingFrames\(\);)$' || true)" = "3" ] \
  && [ "$(sed -n '/^void ALandBattle::StopDarkRead()/,/^}/p' "$CODE/LandBattle.cpp" | flat | grep -cxF "DarkGrab->StopCapturingFrames();" || true)" = "1" ] \
  || { echo "FAULT: LandBattle.cpp calls the dark reader, or the frame grabber, somewhere else or otherwise than it was held (the reader's Tick, Back, Ask and Resized once each, in ReadDark(); its Begin once, in StartDarkRead(); its Built once, in BindDark(), where alone the grabber is started; the grabber asked once, in ReadDark(), and stopped only in BindDark() and StopDarkRead(), never in ReadDark(); the reader named 18 times in the file apart from its type's name 'LandEye::DarkReader', these calls and its answers read for the rule and the logs, so that none is made through another name): a second Tick or Ask in a frame runs the reader's clock twice, and a grabber stopped between asks does not start again. If the lines have only been written anew bring this pin up to date"; STATUS=1; }
[ "$(grep -cF "EyeWorld.DarkLast = [Self]() { return Self.IsValid() ? Self->DarkReader.Last() : LandEye::DarkReading(); };" "$CODE/LandBattle.cpp" || true)" = "1" ] \
  && [ "$(grep -cE '(^|[^[:alnum:]_])DarkLast([^[:alnum:]_]|$)' "$CODE/LandBattle.cpp" || true)" = "1" ] \
  && [ "$(sed -n '/^void ALandBattle::StartDarkRead(const LandEye::Settings& Rule)/,/^}/p' "$CODE/LandBattle.cpp" | flat | grep -cxF 'if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterLandDarkRead")) || FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoDarkRead")))' || true)" = "1" ] \
  || { echo "FAULT: LandBattle.cpp no longer gives the camera's dark rule the reader's last reading ('EyeWorld.DarkLast = [Self]() { return Self.IsValid() ? Self->DarkReader.Last() : LandEye::DarkReading(); };', once), or StartDarkRead() no longer leaves the reading off unless '-TheaterLandDarkRead' asks for it (release 1.5.2 and release 2 ship it off until the Mac has timed it). If the lines have only been written anew bring this pin up to date"; STATUS=1; }
DARK_TICK="$(sed -n '/^void ALandBattle::Tick(float DeltaSeconds)/,/^}/p' "$CODE/LandBattle.cpp" | flat)"
DARK_EYE_LINE="$(printf '%s\n' "$DARK_TICK" | grep -nxF "S.Pose = bNight ? S.NightNow.Pose : S.Eye.Update(S.Battle, S.Real, Real);" | cut -d: -f1 || true)"
DARK_READ_LINE="$(printf '%s\n' "$DARK_TICK" | grep -nxF "ReadDark();" | cut -d: -f1 || true)"
[ "$(printf '%s\n' "$DARK_EYE_LINE" | grep -c . || true)" = "1" ] && [ "$(printf '%s\n' "$DARK_READ_LINE" | grep -c . || true)" = "1" ] \
  && [ "${DARK_EYE_LINE:-0}" -lt "${DARK_READ_LINE:-0}" ] \
  && [ "$(printf '%s\n' "$DARK_TICK" | grep -cE '(^|[^[:alnum:]_])S\.Eye\.Update\(' || true)" = "1" ] \
  && [ "$(grep -cE '(^|[^[:alnum:]_:])ReadDark\(' "$CODE/LandBattle.cpp" || true)" = "1" ] \
  || { echo "FAULT: LandBattle.cpp's tick no longer calls ReadDark() once, after the film's eye is updated with the screen's clock (in Tick: 'S.Pose = bNight ? S.NightNow.Pose : S.Eye.Update(S.Battle, S.Real, Real);' the only update, then 'ReadDark();' the only call in the file): called before it, the reader is told of the last frame's shot, and a cut's first frame is read as the old shot's. If the tick's lines have only been written anew bring this pin up to date"; STATUS=1; }
[ "$(grep -cF "Reader.Tick(R, R, F.bWanted, Dl.ShotBegan(), Dl.DarkCanLeave(), Dl.DarkRun() > 0);" "$CODE/land_play_test.cpp" || true)" = "1" ] \
  || { echo "FAULT: land_play_test.cpp's film pass no longer drives the dark reader with the director's three answers as the scene does ('Reader.Tick(R, R, F.bWanted, Dl.ShotBegan(), Dl.DarkCanLeave(), Dl.DarkRun() > 0);' once): the test would pass a reader the scene does not run"; STATUS=1; }
# (A vehicle's wireless aerial near the lens is put away by the scene, which is not built here: held, in the file's
# code alone, only that DrawArmour() still asks the camera's rule for it, LandEye::AerialShown, which this test plays.
# Not held: what the scene does with the answer.)
[ "$(sed -n '/^void ALandBattle::DrawArmour(const FVector& EyeWorld)/,/^}/p' "$CODE/LandBattle.cpp" | grep -c 'LandEye::AerialShown(' || true)" -ge 1 ] \
  || { echo "FAULT: LandBattle.cpp DrawArmour() no longer asks LandEye::AerialShown how much of a vehicle's wireless aerial to draw: an aerial 2 to 4 m before the lens stands through the whole picture again, a thin dark line from its bottom to its top (Brian's picture of 25 November). If the function has only been named anew bring this pin up to date"; STATUS=1; }
for F in "$@"; do
  case "$F" in *armies.json) continue ;; esac
  "$OUT/land_play_test" "$F" "$ROOT/Content/Data/Sound/mix.json" "$OUT" || STATUS=1
done
echo "logs in $OUT"
exit $STATUS
