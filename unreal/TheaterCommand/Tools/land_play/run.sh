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
  [ "$(body_of 'targetOf(u: LandUnit): LandUnit | null {')" = "$TARGET_WANT" ] || { echo "FAULT: sim.ts targetOf() is no longer, line for line, the rule the spotter's mark's words stand on (the block's whole reach; its given target tested against that reach; only then a hidden block's hold unless it is sent to attack; none struck that is itself hidden; the held reach put to use in the search and in firing on cover: MEN LYING IN WAIT, TROOPS IN PLAIN SIGHT, FULL RANGE), or the file's indent has changed. If the rule itself is unchanged bring this pin up to date; if it has changed, the mark's legend (LandBattleScreen.cpp) and LandSight.h must change with it"; STATUS=1; }
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
# (The camera's dark rule reads the drawn picture through the scene, which is not built here: the lines that give the
# reading its own clock are held, whole and in their order. Measured against a frame's length instead, as it was
# through release 1.5.1, the reading asked for a single frame as the battle began and the rule saw nothing all battle;
# no run here could tell. Held, in the file's code alone: every line of ReadDark() that names the frame's length, the
# clock, the next ask's time or the last reading's time, so the ask and the staleness are measured on the clock that
# is advanced by the frame's length; that the clock, the next ask's time and the last reading's time are written
# nowhere else in the file but in StartDarkRead(), where a battle's reading begins, each set to nought there, once
# (a plain write or a step on one line: one through a pointer or a reference, or one whose sign stands on the next
# line, is not seen; which function a write stands in is held too: the three noughts put in the tick instead, the
# clock begins again every frame and the rule is blind as it was); that a stale reading is told as none, and that
# only a frame that was read keeps the reading fresh; and in the tick, that it hands ReadDark the frame's length and
# that this is the frame's length. Not held: the rest of ReadDark, and the frame grabber.)
code_of "$SRC/Battle/LandBattle.cpp" > "$CODE/LandBattle.cpp"
DARK_BODY="$(sed -n '/^void ALandBattle::ReadDark(double Real)/,/^}/p' "$CODE/LandBattle.cpp" | sed -e 's/^[[:space:]]*//' | grep -v '^$' || true)"
DARK_WANT="void ALandBattle::ReadDark(double Real)
DarkClock += Real;
const double Now = DarkClock;
if (Now >= DarkAskAt && DarkGrab->IsCapturingFrames())
DarkAskAt = Now + 0.25;
if (DarkShare >= 0.0 && Now - DarkGotAt > 0.6)
DarkGotAt = Now;"
[ "$(printf '%s\n' "$DARK_BODY" | grep -E '(^|[^[:alnum:]_])(Real|Now|DarkClock|DarkAskAt|DarkGotAt)([^[:alnum:]_]|$)' || true)" = "$DARK_WANT" ] \
  || { echo "FAULT: LandBattle.cpp ReadDark() no longer keeps the dark reading's clock as it was held (DarkClock += Real, once; Now that clock; the next ask a quarter second on by it, and the reading stale after six tenths of a second by it, under the rule's own three quarters; the frame's length used for nothing else): measured against the frame's length, or on two clocks, the camera's dark rule is blind after its first frame. If the lines have only been written anew bring this pin up to date"; STATUS=1; }
dark_writes() { sed -e 's/^[[:space:]]*//' | grep -E '(^|[^[:alnum:]_])(DarkClock|DarkAskAt|DarkGotAt)[[:space:]]*((<<|>>|[-+*/%&|^])?=([^=]|$)|\+\+|--)|(\+\+|--)[[:space:]]*(DarkClock|DarkAskAt|DarkGotAt)([^[:alnum:]_]|$)' || true; }
DARK_START_WANT="DarkClock = 0.0;
DarkAskAt = 0.0;
DarkGotAt = 0.0;"
DARK_READ_WANT="DarkClock += Real;
DarkAskAt = Now + 0.25;
DarkGotAt = Now;"
DARK_START="$(sed -n '/^void ALandBattle::StartDarkRead()/,/^}/p' "$CODE/LandBattle.cpp" | dark_writes)"
DARK_READ="$(printf '%s\n' "$DARK_BODY" | dark_writes)"
DARK_WRITES="$(dark_writes < "$CODE/LandBattle.cpp")"
[ "$DARK_START" = "$DARK_START_WANT" ] && [ "$DARK_READ" = "$DARK_READ_WANT" ] && [ "$(printf '%s\n' "$DARK_WRITES" | grep -c . || true)" = "6" ] \
  || { echo "FAULT: LandBattle.cpp writes the dark reading's clock, its next ask's time or its last reading's time somewhere else, or otherwise, than it was held (in StartDarkRead(), where a battle's reading begins, the three set to 0.0, once each; in ReadDark() 'DarkClock += Real;', 'DarkAskAt = Now + 0.25;' and 'DarkGotAt = Now;', once each; and nowhere else in the file. Read: in StartDarkRead '$(printf '%s' "$DARK_START" | tr '\n' ' ')'; in ReadDark '$(printf '%s' "$DARK_READ" | tr '\n' ' ')'; in the file '$(printf '%s' "$DARK_WRITES" | tr '\n' ' ')'): a second step of the clock runs it fast, a second stamp keeps a stale reading fresh, and the clock set to nought in the tick begins again every frame, the rule blind as it was through release 1.5.1. If the lines have only been written anew bring this pin up to date"; STATUS=1; }
[ "$(printf '%s\n' "$DARK_BODY" | grep -xF -A 2 "if (DarkShare >= 0.0 && Now - DarkGotAt > 0.6)" | tr '\n' ' ' || true)" = "if (DarkShare >= 0.0 && Now - DarkGotAt > 0.6) { DarkShare = -1.0; " ] \
  && [ "$(printf '%s\n' "$DARK_BODY" | grep -xF -B 1 "DarkGotAt = Now;" | head -n 1 || true)" = "DarkShare = static_cast<double>(Dark) / static_cast<double>((Ht - Skip) * Wd);" ] \
  || { echo "FAULT: LandBattle.cpp ReadDark() no longer tells the rule there is no reading once the last is stale (the line after the stale test's brace 'DarkShare = -1.0;'), or no longer stamps the reading's time straight after the reading itself is set ('DarkGotAt = Now;' under 'DarkShare = static_cast<double>(Dark) / static_cast<double>((Ht - Skip) * Wd);'): a frame that was not read would keep an old reading fresh. If the lines have only been written anew bring this pin up to date"; STATUS=1; }
DARK_TICK="$(sed -n '/^void ALandBattle::Tick(float DeltaSeconds)/,/^}/p' "$CODE/LandBattle.cpp" | sed -e 's/^[[:space:]]*//' || true)"
[ "$(printf '%s\n' "$DARK_TICK" | grep -cxF "const double Real = FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1);" || true)" = "1" ] \
  && [ "$(printf '%s\n' "$DARK_TICK" | grep -cE '^const double Real([^[:alnum:]_]|$)' || true)" = "1" ] \
  && [ "$(printf '%s\n' "$DARK_TICK" | grep -cxF "ReadDark(Real);" || true)" = "1" ] \
  && [ "$(grep -cE '(^|[^[:alnum:]_:])ReadDark\(' "$CODE/LandBattle.cpp" || true)" = "1" ] \
  || { echo "FAULT: LandBattle.cpp's tick no longer hands ReadDark() the frame's length, once (in Tick: 'const double Real = FMath::Clamp(static_cast<double>(DeltaSeconds), 0.0, 0.1);' the only Real, and 'ReadDark(Real);' the only call in the file): handed a running clock, or not called, the camera's dark rule reads wrongly or not at all. If the tick's lines have only been written anew bring this pin up to date"; STATUS=1; }
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
