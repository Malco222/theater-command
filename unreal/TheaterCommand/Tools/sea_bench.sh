#!/bin/bash
# The sea bench (the release 2 ocean plan, step 0: measure first), for the Mac: plays the fleet's sea battles again and
# again, each sea look's tier interleaved with release 1.5's (tier 0), and tables what each costs, the game thread, the
# render thread, the RHI thread, the graphics chip, the most of the three (game, render, chip) and the whole frame, with
# each tier held to the tier 0 runs of its own rounds (Tools/sea_waves/bench_table.py). A difference counts only when the
# 99% interval on the mean of the paired differences leaves 0 out (|mean d| > t(0.995, n - 1) x their spread / root n:
# about twice their spread at five pairs, the plan's rule, and narrower with every round added).
#
#   Tools/sea_bench.sh [OUT]          (default: Saved/SeaBench/<date-time>)
#
# - First a warm-up run, not counted (samar, 5 minutes): the fanless Air is warmed before anything is timed.
# - Then release 1.5's 22 November picture runs, replayed as they were made, the three in turn each round (5 rounds
#   unless told, SEA_BENCH_REPLAY): two-lines and light-ships as water sheets ("-TheaterSeaWaterSheet
#   -TheaterSeaBattleShots=", 24 pictures each), samar as its own shots ("-TheaterSeaBattleShots=", 10 pictures), into a
#   scratch folder emptied after each run, in the game's own window whatever THEATER_WINDOW says, without
#   SEA_BENCH_ARGS, each run quitting itself when its pictures are taken. 22 November's lines (9.5/11.4/13.7,
#   9.2/11.3/15.4, 14.2/22.8/31.0 ms) are of such runs, not of whole battles: the camera on the planned shots, skipping
#   ahead between them, 35, 34 and 167 s of frames. The table reads them only beside these replays (the plan's check 4:
#   tier 0 against the 22 November baseline). No command line of 22 November was kept, so its sound is not known; the
#   replays take the bench's (SEA_BENCH_SOUND).
# - Then, for each tier asked (1 unless told), a block of rounds, every battle a pair a round, tier 0 and that tier one
#   after the other, the order turned about every round (0, T in odd rounds; T, 0 in even), so the warming within a
#   round and the last run's load fall on each side alike; five rounds unless told. Tier 0 asked is a block of tier 0
#   against tier 0 with the held runs' own switches (SEA_BENCH_HELD_ARGS): an own switch's A/B, as step 1(a)'s
#   (SEA_BENCH_TIERS=0 SEA_BENCH_HELD_ARGS=-TheaterSeaMarksPool=1 SEA_BENCH_BATTLES="two-lines samar"), each held run
#   labelled by its "Sea look:" line and held to the plain tier 0 run of its own round; with none, an A/A block.
# - Each run counts its frames only after its first 60 s of play, while the machine settles, and plays to the battle's
#   own end ("-TheaterSeaBench=60,end"): the fight is counted, not only the steaming in (the fighting starts at about
#   210-260 s, and the sinkings, fires and men in the water come after 300). It draws nothing differently.
# - The game's own window by default (98 by 95 hundredths of the desktop: 2881x1786 on Brian's Mac, the size of 22
#   November's picture runs), and its sound played, with the Mac's output muted for the bench and given back after.
# - A run is labelled by its "Sea look:" line, by what was drawn: until step 15 builds the tier, a run asked for tier 1
#   says tier 0, asked and not built (an A/A run, the bench's own noise).
# - Tier 0 is asked for by asking nothing: release 1.5's sea as made.
# - Each run's start and the machine's heat (the system's thermal pressure level, 0 nominal to 4) before it, at most
#   during it and after it go in runs.tsv: the drift across the bench, heat or load. (22 November's other samar line,
#   17.2/31.2/45.8, was samar-warm-grade: a picture run under another grade, -TheaterSeaGrade, not a second run of the
#   same; its grade's values were not kept, so it is not replayed.)
# - Every battle's recording is checked before the first run; a run that overruns is stopped, and killed if it will not
#   stop, and the bench goes on.
#
# Told by the environment:
#   SEA_BENCH_BATTLES  the battles, a recording's key or a path under the project (default: two-lines light-ships samar
#                      Tools/sea_show/stand-ins/convoy-night.json Tools/sea_show/stand-ins/wolf-pack-surrender.json)
#   SEA_BENCH_TIERS    the tiers held to tier 0, a block each, in turn (default: 1; until step 15 builds the tier every
#                      block is one more A/A block); 0: tier 0 held to tier 0, the held runs with SEA_BENCH_HELD_ARGS
#   SEA_BENCH_RUNS     the rounds a block, 5 at the least for the table to count (default 5)
#   SEA_BENCH_DROP     seconds of each run's play left out (default 60)
#   SEA_BENCH_SPAN     seconds counted after them, or "end" for the battle's end (default end)
#   SEA_BENCH_FROM     where in the battle a run starts, "battle=seconds ..." (e.g. "two-lines=190 light-ships=160
#                      samar=150": shorter runs of the fight alone; not the replays); unless told, the battle's start
#   SEA_BENCH_WARM     seconds of the warm-up run, 0 for none (default 300)
#   SEA_BENCH_REPLAY   the rounds of 22 November's picture runs replayed, 0 for none (default 5, the least the table
#                      holds 22 November to)
#   SEA_BENCH_SOUND    muted (default: played, the Mac's output muted), on (played aloud) or off (-nosound: no sound
#                      at all, as Tools/take_pictures.sh runs unless told), the replays' too
#   SEA_BENCH_ARGS     more for the game's command line, the same for every bench run (e.g. -dpcvars=...); never the
#                      replays', made as 22 November's were
#   SEA_BENCH_HELD_ARGS  more for the held runs' command line alone (each block's tier T runs, never its plain tier 0
#                      runs or the replays): an own switch held to today's, e.g. -TheaterSeaMarksPool=1 (step 1(a)) or
#                      -TheaterSeaFarEdge=10 (step 1(b), once built)
#   SEA_BENCH_SLACK    seconds a run may take past its battle (its load, its shaders, slow frames) before it is stopped
#                      (default 300)
#   THEATER_WINDOW     the window, e.g. 1600x1000 (default: the game's own); never the replays', always the game's own
#   UE_ROOT            the engine (default /Users/Shared/Epic Games/UE_5.8)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
PROJECT="$ROOT/TheaterCommand.uproject"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
GAME_BIN="$ENGINE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
LOG="$HOME/Library/Logs/TheaterCommand/TheaterCommand.log"
OUT="${1:-$ROOT/Saved/SeaBench/$(date +%Y%m%d-%H%M%S)}"
BATTLES=(${SEA_BENCH_BATTLES:-two-lines light-ships samar Tools/sea_show/stand-ins/convoy-night.json Tools/sea_show/stand-ins/wolf-pack-surrender.json})
TIERS=(${SEA_BENCH_TIERS:-1})
RUNS="${SEA_BENCH_RUNS:-5}"
DROP="${SEA_BENCH_DROP:-60}"
SPAN="${SEA_BENCH_SPAN:-end}"
WARM="${SEA_BENCH_WARM:-300}"
REPLAY="${SEA_BENCH_REPLAY:-5}"
# (22 November's picture runs, as they were made: the battle and its shots, a water sheet or its own)
REPLAYED=(two-lines light-ships samar)
SOUND="${SEA_BENCH_SOUND:-muted}"
EXTRA=(${SEA_BENCH_ARGS:-})
HELD_EXTRA=(${SEA_BENCH_HELD_ARGS:-})
SLACK="${SEA_BENCH_SLACK:-300}"
if [ ! -x "$GAME_BIN" ]; then
  echo "sea_bench: no engine at $GAME_BIN (UE_ROOT): this runs on the Mac."
  exit 2
fi
if [ "$RUNS" -lt 5 ]; then
  echo "sea_bench: $RUNS rounds a block; the plan asks 5 at the least (a table with fewer is a look, not a measure)."
fi
case "$SPAN" in
  end) ;;
  ''|*[!0-9]*) echo "sea_bench: SEA_BENCH_SPAN is a whole number of seconds or end, not $SPAN"; exit 2 ;;
esac
case "$REPLAY" in
  ''|*[!0-9]*) echo "sea_bench: SEA_BENCH_REPLAY is a whole number of rounds, not $REPLAY"; exit 2 ;;
esac
for T in "${TIERS[@]}"; do
  case "$T" in
    ''|*[!0-9]*) echo "sea_bench: SEA_BENCH_TIERS lists tiers 0 to 3, not $T"; exit 2 ;;
  esac
  if [ "$T" -gt 3 ]; then
    echo "sea_bench: SEA_BENCH_TIERS lists tiers 0 to 3, not $T"
    exit 2
  fi
  if [ "$T" -eq 0 ] && [ ${#HELD_EXTRA[@]} -eq 0 ]; then
    echo "sea_bench: block 0 with no SEA_BENCH_HELD_ARGS: tier 0 against tier 0 as made, an A/A block (the bench's own noise)."
  fi
done
if [ "$REPLAY" -gt 0 ] && [ "$REPLAY" -lt 5 ]; then
  echo "sea_bench: $REPLAY rounds of 22 November's picture runs; the table holds 22 November to 5 at the least (fewer is a look)."
fi
[ "$REPLAY" -gt 0 ] || REPLAYED=()
WINDOW_ARGS=()
WINDOW_SAID="the game's own (98 by 95 hundredths of the desktop: 2881x1786 on Brian's Mac, the size of 22 November's picture runs)"
if [ -n "${THEATER_WINDOW:-}" ]; then
  WINDOW_ARGS=("-TheaterWindow=$THEATER_WINDOW")
  WINDOW_SAID="$THEATER_WINDOW (THEATER_WINDOW)"
fi
SOUND_ARGS=()
case "$SOUND" in
  muted) SOUND_SAID="muted (played with sound, the Mac's output muted for the bench)" ;;
  on) SOUND_SAID="on (played aloud)" ;;
  off) SOUND_ARGS=(-nosound); SOUND_SAID="off (-nosound: no audio device, its work not in the frames)" ;;
  *) echo "sea_bench: SEA_BENCH_SOUND is muted, on or off, not $SOUND"; exit 2 ;;
esac

# A battle's recording, and its length in seconds of the battle's clock (nothing when it cannot be read).
recording_of() {
  case "$1" in
    *.json) echo "$ROOT/$1" ;;
    *) echo "$ROOT/Content/Data/SeaBattles/$1.json" ;;
  esac
}
length_of() {
  python3 -I -c 'import json, sys
views = json.load(open(sys.argv[1], encoding="utf-8"))["views"]
print(int(float(views[-1]["t"]) - float(views[0]["t"]) + 0.999))' "$(recording_of "$1")" 2> /dev/null
}
name_of() {
  basename "$1" .json
}
# Where in its battle a run starts (SEA_BENCH_FROM), 0 for its start.
from_of() {
  local PAIR
  for PAIR in ${SEA_BENCH_FROM:-}; do
    if [ "${PAIR%%=*}" = "$(name_of "$1")" ]; then
      echo "${PAIR#*=}"
      return
    fi
  done
  echo 0
}
# The seconds a run plays: from its start to the battle's end and the scene's 12 s after it, or its drop and span.
played_of() {
  local BATTLE="$1" FROM="$2" FOR="$3" LEN START
  LEN="$(length_of "$BATTLE")"
  START="$(from_of "$BATTLE")"
  local TO_END=$(( LEN - START + 12 ))
  if [ "$FOR" = "end" ] || [ $(( FROM + FOR )) -gt "$TO_END" ]; then
    echo "$TO_END"
  else
    echo $(( FROM + FOR ))
  fi
}
# The system's own word on its heat (the level TheaterSpeed's ReadHeat reads): 0 nominal, 1 moderate, 2 heavy,
# 3 trapping, 4 sleeping; -1 where it is not told.
heat_now() {
  local LEVEL
  LEVEL="$(notifyutil -g com.apple.system.thermalpressurelevel 2> /dev/null | awk '{print $NF}')"
  case "$LEVEL" in
    ''|*[!0-9]*) echo -1 ;;
    *) echo "$LEVEL" ;;
  esac
}

# Every battle's recording read before the first run, the replays' too: one that cannot be read would spoil a run of
# every round.
for B in ${REPLAYED[@]+"${REPLAYED[@]}"}; do
  if [ -z "$(length_of "$B")" ]; then
    echo "sea_bench: $B (replayed): no recording to be read at $(recording_of "$B"); nothing run."
    exit 2
  fi
done
TOTAL=0
for B in "${BATTLES[@]}"; do
  LEN="$(length_of "$B")"
  if [ -z "$LEN" ]; then
    echo "sea_bench: $B: no recording to be read at $(recording_of "$B"); nothing run."
    exit 2
  fi
  START="$(from_of "$B")"
  case "$START" in
    ''|*[!0-9]*) echo "sea_bench: SEA_BENCH_FROM gives $B a start of $START, not a whole number of seconds"; exit 2 ;;
  esac
  if [ "$START" -ge "$LEN" ]; then
    echo "sea_bench: SEA_BENCH_FROM starts $B at $START s, after its end ($LEN s); nothing run."
    exit 2
  fi
  TOTAL=$(( TOTAL + $(played_of "$B" "$DROP" "$SPAN") + 45 ))
done
mkdir -p "$OUT/logs"
COUNT=$(( ${#TIERS[@]} * RUNS * ${#BATTLES[@]} * 2 + REPLAY * ${#REPLAYED[@]} ))
# (a replay round: 22 November's 35, 34 and 167 s of frames, the skips ahead and three loads, about 7 minutes)
echo "sea_bench: $COUNT runs, about $(( (TOTAL * 2 * RUNS * ${#TIERS[@]} + WARM + REPLAY * 420 * (${#REPLAYED[@]} > 0)) / 60 )) minutes, into $OUT"
printf '# order\tbattle\tasked tier\tlog\tstatus\tblock\tround\trole\tfirst\tstarted\theat before\theat most\theat after\n' > "$OUT/runs.tsv"
{
  echo "sea_bench started $(date); tiers ${TIERS[*]} (a block each), $RUNS rounds, drop $DROP s, span $SPAN, warm-up $WARM s, $REPLAY replay rounds"
  if [ ${#EXTRA[@]} -gt 0 ]; then
    echo "every bench run also: ${EXTRA[*]} (SEA_BENCH_ARGS)"
  fi
  if [ ${#HELD_EXTRA[@]} -gt 0 ]; then
    echo "the held runs alone also: ${HELD_EXTRA[*]} (SEA_BENCH_HELD_ARGS; never the plain tier 0 runs or the replays)"
  fi
  echo "battles: ${BATTLES[*]}"
  for B in "${BATTLES[@]}"; do
    echo "  $(name_of "$B"): $(length_of "$B") s of battle, played from $(from_of "$B") s"
  done
  if [ "$REPLAY" -gt 0 ]; then
    echo "replays: $REPLAY rounds of release 1.5's 22 November picture runs (two-lines and light-ships water sheets, samar its own shots), the game's own window, no SEA_BENCH_ARGS; 22 November's sound not known"
  fi
  echo "window: $WINDOW_SAID"
  echo "sound: $SOUND_SAID"
  echo "heat: the system's thermal pressure level, 0 nominal to 4 (now $(heat_now); -1: not told)"
  echo "machine: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || uname -m); $(sw_vers -productVersion 2>/dev/null || uname -sr)"
  echo "tree: $(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null) $(git -C "$ROOT" status --porcelain 2>/dev/null | wc -l | tr -d ' ') files changed"
} > "$OUT/about.txt"

# The Mac's output muted for the bench (SEA_BENCH_SOUND=muted), and given back as it was however the bench ends.
WAS_MUTED=""
restore_sound() {
  if [ "$WAS_MUTED" = "false" ]; then
    osascript -e 'set volume without output muted' > /dev/null 2>&1
    WAS_MUTED=""
  fi
}
if [ "$SOUND" = "muted" ]; then
  WAS_MUTED="$(osascript -e 'output muted of (get volume settings)' 2> /dev/null)"
  if osascript -e 'set volume with output muted' > /dev/null 2>&1; then
    trap restore_sound EXIT
    trap 'restore_sound; exit 130' INT TERM
  else
    WAS_MUTED=""
    echo "sea_bench: the Mac's output could not be muted: the runs play aloud."
    echo "  (the Mac's output could not be muted: the runs played aloud)" >> "$OUT/about.txt"
  fi
fi

# One run: plays a battle, waits for it (stopped if it overruns its battle by five minutes, and killed if it will not
# stop), stops its crash reporter if it left one (17 November: one stuck and spoiled every timing beside it), and keeps
# its log with its place in the bench and the machine's heat. ROLE warm: the warm-up, not counted; replay: 22
# November's picture run of the battle, as it was made, its pictures not kept; base or held: a bench run.
ORDER=0
run_one() {
  local BATTLE="$1" TIER="$2" FROM="$3" FOR="$4" BLOCK="$5" ROUND="$6" ROLE="$7" FIRST="$8"
  local LOOK=() START_AT=() SHOTS="" LIMIT REPLAY_ARGS=() HELD_ARGS=()
  [ "$TIER" -gt 0 ] && LOOK=("-TheaterSeaLook=$TIER")
  # (the held runs' own switches, SEA_BENCH_HELD_ARGS: never a plain tier 0 run's, a replay's or the warm-up's)
  [ "$ROLE" = "held" ] && HELD_ARGS=(${HELD_EXTRA[@]+"${HELD_EXTRA[@]}"})
  local START
  START="$(from_of "$BATTLE")"
  [ "$START" -gt 0 ] && START_AT=("-TheaterSeaBattleFrom=$START")
  if [ "$ROLE" = "replay" ]; then
    # (22 November's picture run of the battle as it was made: two-lines and light-ships water sheets, samar its own
    # shots, into a scratch folder; the game quits when its pictures are taken)
    SHOTS="$(printf '%s/shots/%03d' "$OUT" "$ORDER")"
    REPLAY_ARGS=(-TheaterSeaBattle="$BATTLE")
    [ "$BATTLE" = samar ] || REPLAY_ARGS+=(-TheaterSeaWaterSheet)
    REPLAY_ARGS+=(-TheaterSeaBattleShots="$SHOTS")
    LIMIT=$(( $(length_of "$BATTLE") + 12 + SLACK ))
  else
    LIMIT=$(( $(played_of "$BATTLE" "$FROM" "$FOR") + SLACK ))
  fi
  local STARTED HEAT_BEFORE HEAT_MOST HEAT_AFTER
  STARTED="$(date +%Y-%m-%dT%H:%M:%S)"
  HEAT_BEFORE="$(heat_now)"
  HEAT_MOST="$HEAT_BEFORE"
  rm -f "$LOG"
  if [ "$ROLE" = "replay" ]; then
    # (as 22 November's were made: the game's own window, no -TheaterSeaBench and no SEA_BENCH_ARGS; it quits itself)
    mkdir -p "$SHOTS"
    "$GAME_BIN" "$PROJECT" -game -windowed -unattended -nosplash ${SOUND_ARGS[@]+"${SOUND_ARGS[@]}"} "${REPLAY_ARGS[@]}" > /dev/null 2>&1 &
  else
    "$GAME_BIN" "$PROJECT" -game -windowed ${WINDOW_ARGS[@]+"${WINDOW_ARGS[@]}"} -unattended -nosplash ${SOUND_ARGS[@]+"${SOUND_ARGS[@]}"} \
      -TheaterSeaBattle="$BATTLE" -TheaterSeaBattleSpeed=1 ${START_AT[@]+"${START_AT[@]}"} -TheaterSeaBench="$FROM,$FOR" \
      ${LOOK[@]+"${LOOK[@]}"} ${EXTRA[@]+"${EXTRA[@]}"} ${HELD_ARGS[@]+"${HELD_ARGS[@]}"} > /dev/null 2>&1 &
  fi
  local GAME=$! WAITED=0 STATUS=done HEAT
  while kill -0 "$GAME" 2> /dev/null; do
    sleep 1
    WAITED=$(( WAITED + 1 ))
    if [ $(( WAITED % 5 )) -eq 0 ]; then
      HEAT="$(heat_now)"
      [ "$HEAT" -gt "$HEAT_MOST" ] && HEAT_MOST="$HEAT"
    fi
    if [ "$WAITED" -gt "$LIMIT" ] && kill -0 "$GAME" 2> /dev/null; then
      # (asked to quit first; a game thread that hangs never answers that, and one hung run must not stop the bench)
      kill "$GAME" 2> /dev/null
      STATUS=overran
      for _ in $(seq 1 30); do
        kill -0 "$GAME" 2> /dev/null || break
        sleep 1
      done
      if kill -0 "$GAME" 2> /dev/null; then
        kill -9 "$GAME" 2> /dev/null
        STATUS="overran, killed"
      fi
      break
    fi
  done
  wait "$GAME" 2> /dev/null
  for _ in 1 2 3 4 5 6 7 8 9 10; do
    pgrep -f "CrashReportClient .*TheaterCommand-pid-$GAME-" > /dev/null || break
    sleep 2
  done
  if pgrep -f "CrashReportClient .*TheaterCommand-pid-$GAME-" > /dev/null; then
    pkill -f "CrashReportClient .*TheaterCommand-pid-$GAME-"
    STATUS=crashed
  fi
  HEAT_AFTER="$(heat_now)"
  local NAME
  NAME="$(name_of "$BATTLE")"
  if [ "$ROLE" = "replay" ]; then
    # (its pictures only for its timing: at 2881x1786 five rounds of them would fill gigabytes of the Air's disk)
    local FILE PICTURES
    PICTURES="$(find "$SHOTS" -name '*.png' 2> /dev/null | wc -l | tr -d ' ')"
    rm -rf "$SHOTS"
    FILE="$(printf 'logs/%03d-%s-t0-replay.log' "$ORDER" "$NAME")"
    cp "$LOG" "$OUT/$FILE" 2> /dev/null || STATUS="$STATUS, no log"
    printf '%d\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$ORDER" "$NAME" 0 "$FILE" "$STATUS" "" "$ROUND" replay "" \
      "$STARTED" "$HEAT_BEFORE" "$HEAT_MOST" "$HEAT_AFTER" >> "$OUT/runs.tsv"
    echo "  $(date +%H:%M:%S) run $ORDER: $NAME, 22 November's picture run replayed (round $ROUND): $STATUS; heat $HEAT_BEFORE/$HEAT_MOST/$HEAT_AFTER; $PICTURES pictures written, not kept; $(grep -h "Sea battle: frame times over" "$OUT/$FILE" 2> /dev/null | tail -1 | sed -e 's/^.*Sea battle: frame times over /frame times over /' | cut -c1-160)"
    ORDER=$(( ORDER + 1 ))
  elif [ "$ROLE" != "warm" ]; then
    local FILE
    FILE="$(printf 'logs/%03d-%s-t%d.log' "$ORDER" "$NAME" "$TIER")"
    cp "$LOG" "$OUT/$FILE" 2> /dev/null || STATUS="$STATUS, no log"
    printf '%d\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$ORDER" "$NAME" "$TIER" "$FILE" "$STATUS" "$BLOCK" "$ROUND" "$ROLE" "$FIRST" \
      "$STARTED" "$HEAT_BEFORE" "$HEAT_MOST" "$HEAT_AFTER" >> "$OUT/runs.tsv"
    echo "  $(date +%H:%M:%S) run $ORDER: $NAME, tier $TIER asked${HELD_ARGS[@]+ with ${HELD_ARGS[*]}} (block $BLOCK, round $ROUND): $STATUS; heat $HEAT_BEFORE/$HEAT_MOST/$HEAT_AFTER; $(grep -h "Sea look:" "$OUT/$FILE" 2> /dev/null | tail -1 | sed -e 's/^.*Sea look: /Sea look: /' -e 's/; none of the release 2 sea.s [0-9]* switches is built yet, so every one is at today.s value//' | cut -c1-160)"
    ORDER=$(( ORDER + 1 ))
  else
    cp "$LOG" "$OUT/logs/warm-up.log" 2> /dev/null
    echo "  $(date +%H:%M:%S) warm-up: $STATUS (not counted); heat $HEAT_BEFORE/$HEAT_MOST/$HEAT_AFTER"
  fi
}

if [ "$WARM" -gt 0 ]; then
  run_one samar 0 1 "$WARM" "" "" warm ""
fi
if [ "$REPLAY" -gt 0 ]; then
  echo "sea_bench: release 1.5's 22 November picture runs, replayed, $REPLAY rounds"
  for (( R = 1; R <= REPLAY; R++ )); do
    for B in "${REPLAYED[@]}"; do
      run_one "$B" 0 1 end "" "$R" replay ""
    done
  done
  rmdir "$OUT/shots" 2> /dev/null
fi
for T in "${TIERS[@]}"; do
  echo "sea_bench: block $T, tier 0 against tier $T${HELD_EXTRA[@]+ with ${HELD_EXTRA[*]}}, $RUNS rounds"
  for (( R = 1; R <= RUNS; R++ )); do
    for B in "${BATTLES[@]}"; do
      if [ $(( R % 2 )) -eq 1 ]; then
        run_one "$B" 0 "$DROP" "$SPAN" "$T" "$R" base base
        run_one "$B" "$T" "$DROP" "$SPAN" "$T" "$R" held base
      else
        run_one "$B" "$T" "$DROP" "$SPAN" "$T" "$R" held held
        run_one "$B" 0 "$DROP" "$SPAN" "$T" "$R" base held
      fi
    done
  done
done
echo "sea_bench finished $(date)" >> "$OUT/about.txt"
restore_sound
python3 -I "$HERE/sea_waves/bench_table.py" "$OUT"
