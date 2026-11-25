#!/bin/bash
# Tries the house-rules skill in a throwaway copy of the game, never the real one.
#
#   check_on_spare_copy.sh [project folder] [skill branch]
#
# 1. Steps aside if any Unreal editor or build is running.
# 2. Makes a copy-on-write clone of the project (no extra disk until written to).
# 3. Puts this plugin into the clone and switches it on there, with the editor's
#    Claude link (MCP) on port 8001, so it never meets 8000 or 8765.
# 4. Opens the clone's editor without a window, runs check_skill.py against it,
#    closes it and deletes the clone. The real project folder is only read.
set -uo pipefail

REPO="${1:-$HOME/Total War WW2}"
BRANCH="${2:-claude/project-thread-2qcpzp}"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
EDITOR="$ENGINE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
PORT=8001
WORK="$(mktemp -d "${TMPDIR:-/tmp}/tc-skill-check.XXXXXX")"
PROJECT="$WORK/TheaterCommand"
LOG="$WORK/editor.log"
PY=/usr/bin/python3
[ -x "$PY" ] || PY="$ENGINE/Engine/Binaries/ThirdParty/Python3/Mac/bin/python3"

say() { echo "[skill-check $(date +%H:%M:%S)] $*"; }
finish() {
  if [ -n "${EPID:-}" ] && kill -0 "$EPID" 2>/dev/null; then
    kill -TERM "$EPID" 2>/dev/null
    for _ in $(seq 1 60); do kill -0 "$EPID" 2>/dev/null || break; sleep 1; done
    kill -KILL "$EPID" 2>/dev/null
  fi
  [ -f "$LOG" ] && { say "editor log, link and skill lines:"; grep -iE "ModelContextProtocol|AgentSkill|theater_command_skills|Python.*(Error|Traceback)|LogPython: Error" "$LOG" | tail -25; }
  rm -rf "$WORK"
  say "clone removed"
}
trap finish EXIT

# 1. Step aside.
if pgrep -x UnrealEditor >/dev/null || pgrep -f "UnrealBuildTool|Build.sh|UnrealEditor-Cmd" >/dev/null; then
  say "STEP ASIDE: an Unreal editor or build is running:"; pgrep -lf "UnrealEditor|UnrealBuildTool|Build.sh"; exit 3
fi
if lsof -nP -iTCP:$PORT -sTCP:LISTEN >/dev/null 2>&1; then
  say "STEP ASIDE: something already listens on port $PORT"; exit 3
fi

# 2. Clone.
say "cloning $REPO/unreal/TheaterCommand"
cp -c -R "$REPO/unreal/TheaterCommand" "$PROJECT" || { say "FAIL clone"; exit 2; }
rm -rf "$PROJECT/Saved" "$PROJECT/Plugins/TheaterCommandSkills"

# 3. The plugin, from the skill branch, into the clone only.
git -C "$REPO" fetch -q origin "$BRANCH" || { say "FAIL fetch $BRANCH"; exit 2; }
git -C "$REPO" archive FETCH_HEAD unreal/TheaterCommand/Plugins/TheaterCommandSkills \
  | tar -x -C "$PROJECT" --strip-components=2 || { say "FAIL plugin files"; exit 2; }
"$PY" - "$PROJECT/TheaterCommand.uproject" <<'EOF'
import json, sys
path = sys.argv[1]
project = json.load(open(path))
plugins = project.setdefault("Plugins", [])
for name in ("ModelContextProtocol", "TheaterCommandSkills"):
    entry = next((p for p in plugins if p.get("Name") == name), None)
    if entry is None:
        plugins.append({"Name": name, "Enabled": True, "TargetAllowList": ["Editor"]})
    else:
        entry["Enabled"] = True
json.dump(project, open(path, "w"), indent="\t")
print("clone's plugins:", ", ".join(p["Name"] for p in plugins if p.get("Enabled")))
EOF

# 4. Open the clone's editor with the link, check, close.
say "opening the clone's editor with the link on port $PORT (WINDOWED=1 for a window)"
START=$(date +%s)
RHI="-nullrhi"; [ "${WINDOWED:-0}" = 1 ] && RHI="-windowed"
"$EDITOR" "$PROJECT/TheaterCommand.uproject" $RHI -unattended -nosplash -nosound \
  -ModelContextProtocolStartServer -ModelContextProtocolPort=$PORT -abslog="$LOG" >/dev/null 2>&1 &
EPID=$!
UP=0
for _ in $(seq 1 180); do
  kill -0 "$EPID" 2>/dev/null || { say "FAIL the editor quit during start-up"; tail -40 "$LOG"; exit 1; }
  if curl -s -o /dev/null -m 3 -X POST -H 'Content-Type: application/json' -d '{}' "http://127.0.0.1:$PORT/mcp"; then UP=1; break; fi
  sleep 5
done
[ "$UP" = 1 ] || { say "FAIL the link did not answer within 15 minutes"; tail -40 "$LOG"; exit 1; }
say "link answered after $(( $(date +%s) - START )) s"
"$PY" "$PROJECT/Plugins/TheaterCommandSkills/check_skill.py" $PORT
RESULT=$?
exit $RESULT
