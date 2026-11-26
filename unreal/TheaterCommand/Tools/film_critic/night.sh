#!/bin/bash
# Film critic: the night run on Brian's personal Mac. A development tool, not part of the game.
#
# Started by the LaunchAgent every half hour through the night (see install.sh). Each time it first asks whether
# it may run at all, and leaves at once, saying why in the night's attempts.log, unless ALL of these hold:
#   - tonight's run is not done yet, and it is between 9 PM and 6 AM on this Mac's clock;
#   - the Mac is on mains power and its lid is open (a locked screen is fine: the game draws the same behind it);
#   - nobody has touched the keyboard or mouse for 20 minutes;
#   - no game is open that is not a picture run (that is Brian's game, from any folder, his backup icons too),
#     no other picture run (TheaterShots=) and no Unreal editor is open, and none of the Mac builder's scripts
#     (TC-mac-work/...sh) is running;
#   - the Mac builder's lock ~/TC-mac-work/joblock can be taken (mkdir). It is given back (rmdir) on the way out.
#   - there are 3 GB free on the disk.
# Then it plays each scene in scenes/scenes.txt with the Play folder's own build, in picture mode only
# (-TheaterShots= with -TheaterSteps=): no git, no build, nothing in Content is touched. Between scenes it looks again
# and stops at once if Brian has come back (a key or the mouse in the last minute, or his game opened).
# Around every game run it keeps his settings: Saved/Config is copied aside first and put back after (a picture
# run rewrites the window and option settings), and the picture run's own files are deleted after
# (Saved/Campaign/campaign-pictures.json, options-pictures.json, Saves-pictures). His campaign.json and Saves are
# never touched.
# Output: Saved/FilmCritic/nights/<evening's date>/<scene>/ (pictures, steps.jsonl, the game's log), one zip per
# scene for the morning check in the cloud, and night.json saying what ran. Two nights are kept.
#
# FILM_CRITIC_FORCE=1 skips the clock, idle and "done tonight" checks (for a trial run someone has asked for);
# every other check still holds. FILM_CRITIC_ONLY="scene,scene" plays only those scenes.
set -uo pipefail
export LC_ALL=C

KIT="$(cd "$(dirname "$0")" && pwd)"
PROJ="${FILM_CRITIC_PROJECT:-$HOME/Total War WW2/unreal/TheaterCommand}"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
GAME_BIN="$ENGINE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
LOCK="$HOME/TC-mac-work/joblock"
ROOT="$PROJ/Saved/FilmCritic/nights"
GAMELOG="$HOME/Library/Logs/TheaterCommand/TheaterCommand.log"
# The Play folder's path has spaces; the game is handed paths through a link that has none.
LINK="/tmp/tc-film-critic"
FORCE="${FILM_CRITIC_FORCE:-0}"
ONLY="${FILM_CRITIC_ONLY:-}"

hour=$(date +%H); hour=$((10#$hour))
# The night belongs to the evening it began on: 1 AM on the 27th is the night of the 26th.
if [ "$hour" -lt 12 ]; then NIGHT=$(date -v-1d +%Y-%m-%d); else NIGHT=$(date +%Y-%m-%d); fi
OUT="$ROOT/$NIGHT"
mkdir -p "$OUT"
say() { echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$OUT/attempts.log"; }
leave() { say "not run: $*"; exit 0; }

# ---- may it run at all? ---------------------------------------------------------------------------------------
[ -f "$OUT/DONE" ] && [ "$FORCE" != 1 ] && exit 0
if [ "$FORCE" != 1 ] && [ "$hour" -lt 21 ] && [ "$hour" -ge 6 ]; then exit 0; fi

# Keeping the Mac awake (Brian's choice, a file "keep-awake" beside this script): from the first try of the evening,
# while the Mac is plugged in, a caffeinate keeps the display from sleeping (so it does not lock) until the run is done
# or 6 AM. The run itself still waits for 20 idle minutes, and stops the moment a key or the mouse is used.
KEEPER="$ROOT/.keep-awake.pid"
keep_awake() {
  [ -f "$KIT/keep-awake" ] || return 0
  [ "$hour" -ge 21 ] || [ "$hour" -lt 6 ] || return 0
  pmset -g batt | head -1 | grep -q "AC Power" || return 0
  [ -f "$KEEPER" ] && kill -0 "$(cat "$KEEPER")" 2> /dev/null && return 0
  local left=$(( ( (hour >= 21 ? 30 - hour : 6 - hour) * 3600 ) - 10#$(date +%M) * 60 ))
  [ "$left" -gt 0 ] || return 0
  nohup caffeinate -d -i -t "$left" > /dev/null 2>&1 &
  echo $! > "$KEEPER"
}
let_sleep() { [ -f "$KEEPER" ] && kill "$(cat "$KEEPER")" 2> /dev/null; rm -f "$KEEPER"; }
# After the run the screen may sleep and lock again, but the Mac itself is held awake (never on battery) until the
# morning check in the cloud has copied the night (it leaves a file PULLED in the night's folder) or 8 AM, whichever
# comes first, so the pictures can be fetched while Brian sleeps.
hold_for_pull() {
  local h; h=$(date +%H); h=$((10#$h))
  local left=$(( ( (h >= 8 ? 32 - h : 8 - h) * 3600 ) - 10#$(date +%M) * 60 ))
  [ "$left" -gt 0 ] && [ "$left" -le 43200 ] || return 0
  local end=$(( $(date +%s) + left ))
  # (caffeinate ignores -t when it runs a command: the loop keeps its own clock)
  nohup caffeinate -i -s /bin/sh -c "while [ ! -f \"$OUT/PULLED\" ] && [ \$(date +%s) -lt $end ]; do sleep 60; done" > /dev/null 2>&1 &
}

idle_seconds() { ioreg -c IOHIDSystem | awk '/HIDIdleTime/ {print int($NF/1000000000); exit}'; }
lid_closed() { ioreg -r -k AppleClamshellState -d 4 | grep -q '"AppleClamshellState" = Yes'; }
screen_locked() { ioreg -n Root -d1 | grep -q '"CGSSessionScreenIsLocked"=Yes'; }
on_battery() { pmset -g batt | head -1 | grep -q "Battery Power"; }
# Brian's game: any game run (from any folder) without TheaterShots= and without this kit's own mark (-FilmCritic, on
# every run it starts, battle scenes included). Any full editor counts as someone at work too.
# (the engine's own binary only: Epic's UnrealEditorServices helper runs all the time and is not a game)
brians_game() { ps -axo args= | grep -E "MacOS/UnrealEditor( |$)" | grep -v "TheaterShots=" | grep -v -- "-FilmCritic" | grep -q .; }
other_picture_run() { pgrep -fl "TheaterShots=" | grep -q .; }
builder_busy() { pgrep -f "TC-mac-work/.*\.sh" > /dev/null; }
free_gb() { df -g "$PROJ" | awk 'NR==2 {print $4}'; }

keep_awake
lid_closed && leave "the lid is closed"
# (a locked screen does not stop it: the Mac builder measured the same frame rate locked as unlocked, 26 Nov;
# each scene's result says whether the screen was locked, so the morning check can tell if that ever changes)
on_battery && leave "on battery power"
if [ "$FORCE" != 1 ]; then
  idle=$(idle_seconds); [ "${idle:-0}" -lt 1200 ] && leave "the Mac was in use ${idle}s ago"
fi
brians_game && leave "a game or the editor is open (Brian's, or a build)"
other_picture_run && leave "another picture run is going"
builder_busy && leave "a Mac builder script is running"
[ "$(free_gb)" -lt 3 ] && leave "under 3 GB free on the disk"
[ -x "$GAME_BIN" ] || leave "no game engine at $GAME_BIN"
[ -f "$PROJ/TheaterCommand.uproject" ] || leave "no project at $PROJ"
mkdir -p "$(dirname "$LOCK")"
mkdir "$LOCK" 2> /dev/null || leave "the Mac builder holds the lock"

# ---- it runs: hold the lock, keep the Mac awake, keep Brian's settings ------------------------------------------
CONFIG_KEEP="$OUT/.config-kept"
restore_config() {
  if [ -d "$CONFIG_KEEP/Config" ]; then
    rm -rf "$PROJ/Saved/Config" && cp -Rp "$CONFIG_KEEP/Config" "$PROJ/Saved/Config"
  fi
  rm -f "$PROJ/Saved/Campaign/campaign-pictures.json" "$PROJ/Saved/Campaign/options-pictures.json"
  rm -rf "$PROJ/Saved/Campaign/Saves-pictures"
}
GAME=""
finish() {
  [ -n "$GAME" ] && kill -TERM "$GAME" 2> /dev/null
  restore_config
  rm -rf "$CONFIG_KEEP"
  rmdir "$LOCK" 2> /dev/null
  [ -n "${AWAKE:-}" ] && kill "$AWAKE" 2> /dev/null
}
trap finish EXIT
trap 'say "stopped by a signal"; exit 1' INT TERM HUP

rm -rf "$CONFIG_KEEP"; mkdir -p "$CONFIG_KEEP"
ln -sfn "$PROJ/Saved/FilmCritic" "$LINK"
[ -d "$PROJ/Saved/Config" ] && cp -Rp "$PROJ/Saved/Config" "$CONFIG_KEEP/Config"
# Keep the Mac and its display awake for the run (never -u for the whole run: a standing "the user is active"
# claim would hide Brian's own return). One short claim first wakes a display that has gone dark.
caffeinate -u -t 2 > /dev/null 2>&1
caffeinate -dims -w $$ &
AWAKE=$!
WOKE=$(date +%s)

BUILD=$(git -C "$PROJ" rev-parse --short=9 HEAD 2> /dev/null || echo unknown)
say "run begins on build $BUILD"
START=$(date +%s)
RESULTS=()

came_back() {
  local idle; idle=$(idle_seconds)
  # (the display was woken by a claim of activity at the start: the idle clock is read from two minutes after it)
  [ "$FORCE" != 1 ] && [ $(( $(date +%s) - WOKE )) -gt 120 ] && [ "${idle:-0}" -lt 60 ] && { echo "a key or the mouse was used"; return 0; }
  lid_closed && { echo "the lid was closed"; return 0; }
  brians_game && { echo "a game was opened"; return 0; }
  return 1
}

play_scene() { # name minutes flags (with {OUT} for the scene's folder and {KIT} for this kit, both without spaces)
  local name="$1" minutes="$2" flags="$3"
  local dir="$OUT/$name" began code=0 why=""
  rm -rf "$dir"; mkdir -p "$dir"
  flags="${flags//\{OUT\}/$LINK/nights/$NIGHT/$name}"
  flags="${flags//\{KIT\}/$LINK/kit}"
  echo "$flags" > "$dir/flags.txt"
  began=$(date +%s)
  # shellcheck disable=SC2086
  "$GAME_BIN" "$PROJ/TheaterCommand.uproject" -game -windowed -TheaterWindow=1600x1000 -unattended -nosplash -nosound \
    -FilmCritic $flags < /dev/null > /dev/null 2>&1 &
  GAME=$!
  local limit=$((minutes * 60))
  while kill -0 "$GAME" 2> /dev/null; do
    sleep 5
    if [ $(( $(date +%s) - began )) -ge "$limit" ]; then why="timed out after $minutes minutes"; break; fi
    if b=$(came_back); then why="stopped: $b"; break; fi
  done
  if kill -0 "$GAME" 2> /dev/null; then
    kill -TERM "$GAME" 2> /dev/null; sleep 10; kill -KILL "$GAME" 2> /dev/null
    code=124
  else
    wait "$GAME"; code=$?
  fi
  local pid="$GAME"; GAME=""
  # A crashed run leaves a crash reporter behind; give it twenty seconds, then stop it (this run's own only, by its
  # number, as Tools/take_pictures.sh does).
  for _ in 1 2 3 4 5 6 7 8 9 10; do
    pgrep -f "CrashReportClient .*TheaterCommand-pid-$pid-" > /dev/null || break
    sleep 2
  done
  pkill -f "CrashReportClient .*TheaterCommand-pid-$pid-" 2> /dev/null && why="${why:+$why; }crashed"
  [ -f "$GAMELOG" ] && cp "$GAMELOG" "$dir/game.log" && gzip -f "$dir/game.log"
  restore_config
  local secs=$(( $(date +%s) - began )) pics
  pics=$(ls "$dir" | grep -c -i '\.jpg$\|\.png$')
  say "scene $name: ${secs}s, exit $code, $pics pictures ${why:+($why)}"
  if [[ "$why" == stopped:* ]]; then
    # Brian came back mid-scene: throw the half scene away; a later start tonight plays it again from the top.
    rm -rf "$dir" "$OUT/$name.zip"
    return 1
  fi
  local locked=false; screen_locked && locked=true
  echo "{\"scene\":\"$name\",\"seconds\":$secs,\"exit\":$code,\"pictures\":$pics,\"locked\":$locked,\"note\":\"$why\"}" > "$dir/result.json"
  RESULTS+=("$name")
  # Stills go from PNG to JPEG (about a sixth of the size; the checker finds step-001.jpg under its PNG name).
  if command -v sips > /dev/null; then
    find "$dir" -type f -name '*.png' | while read -r png; do
      sips -s format jpeg -s formatOptions 88 "$png" --out "${png%.png}.jpg" > /dev/null 2>&1 && rm -f "$png"
    done
  fi
  rm -f "$OUT/$name.zip"
  # The zip is what the morning check fetches: once it is made, only result.json stays loose (the Mac's disk is
  # nearly full, and the camera and men facts squeeze about ten to one in a zip).
  if (cd "$OUT" && zip -qr "$name.zip" "$name"); then
    find "$dir" -mindepth 1 ! -name result.json -delete 2> /dev/null
  fi
  return 0
}

# The morning check reads the night at 5:43 AM: no scene starts after 5:15 AM (a scene runs 25 minutes at most).
night_over() {
  [ "$FORCE" = 1 ] && return 1
  local h m; h=$((10#$(date +%H))); m=$((10#$(date +%M)))
  [ "$h" -ge 6 ] && [ "$h" -lt 21 ] && return 0
  [ "$h" -eq 5 ] && [ "$m" -ge 15 ] && return 0
  return 1
}

STOPPED=""; LATE=""; WANTED=0; MISSING=0
# scenes.txt: one scene a line, "name | minutes | the game's flags", {OUT} and {KIT} as above; # starts a note.
while IFS='|' read -r name minutes flags; do
  case "$name" in ''|\#*) continue ;; esac
  name=$(echo "$name" | xargs); minutes=$(echo "${minutes:-15}" | xargs); flags=$(echo "$flags" | xargs)
  if [ -n "$ONLY" ] && [[ ",$ONLY," != *",$name,"* ]]; then continue; fi
  WANTED=$((WANTED + 1))
  # A scene already played in full tonight (an earlier start that Brian interrupted later) is not played again.
  [ "$FORCE" != 1 ] && [ -f "$OUT/$name/result.json" ] && continue
  [ -n "$STOPPED" ] && { MISSING=$((MISSING + 1)); continue; }
  if night_over; then STOPPED="the night ran out (no scene starts after 5:15 AM)"; LATE=1; MISSING=$((MISSING + 1)); continue; fi
  if b=$(came_back); then STOPPED="$b"; say "stopping before $name: $b"; MISSING=$((MISSING + 1)); continue; fi
  play_scene "$name" "$minutes" "$flags" || { STOPPED="Brian came back during $name"; MISSING=$((MISSING + 1)); }
done < "$KIT/scenes/scenes.txt"

END=$(date +%s)
# night.json covers every scene played in full tonight, from this start and any earlier one.
{
  echo "{\"night\":\"$NIGHT\",\"build\":\"$BUILD\",\"began\":$START,\"ended\":$END,\"stopped\":\"$STOPPED\",\"wanted\":$WANTED,\"missing\":$MISSING,\"scenes\":["
  first=1
  for r in "$OUT"/*/result.json; do
    [ -f "$r" ] || continue
    [ $first = 1 ] || echo ","
    cat "$r"; first=0
  done
  echo "]}"
} > "$OUT/night.json"
played=$(ls "$OUT"/*/result.json 2> /dev/null | wc -l | xargs)
if [ "$FORCE" = 1 ] || [ -n "$ONLY" ]; then
  :   # a trial someone asked for: tonight's own run still happens (it skips the scenes the trial already played)
elif [ "$MISSING" = 0 ] && [ "$played" -gt 0 ]; then
  touch "$OUT/DONE"
  let_sleep
  hold_for_pull
elif [ -n "$LATE" ] && [ "$played" -gt 0 ]; then
  let_sleep
  hold_for_pull   # part of the night was played: keep the Mac reachable for the morning check
fi
say "run ends after $(( (END - START) / 60 )) minutes ${STOPPED:+(stopped: $STOPPED)}"

# Keep two nights at most, and none the morning check has already fetched (PULLED) other than tonight's.
ls -1d "$ROOT"/20??-??-?? 2> /dev/null | sort -r | tail -n +3 | while read -r old; do rm -rf "$old"; done
for old in "$ROOT"/20??-??-??; do
  [ -d "$old" ] && [ "$old" != "$OUT" ] && [ -f "$old/PULLED" ] && rm -rf "$old"
done
exit 0
