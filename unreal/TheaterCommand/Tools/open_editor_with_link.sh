#!/bin/bash
# Opens the Unreal editor on this project with Unreal's built-in Claude link (its MCP server) awake.
#
# Brian's OK, 25 November 2026: the link is switched on in TheaterCommand.uproject (editor only) but
# stays ASLEEP. Only this script wakes it, and only Claude's own Mac jobs run this script. The Play
# icon never wakes it, so the game plays exactly as before.
#
# The link listens only inside this Mac, at http://127.0.0.1:$PORT/mcp (nothing outside can reach it).
# Ports: 8000 is this project's link; 8001 is the skill check's spare clone; 8765 is the scratch
# editor's. Never use 8765 here.
#
# Steps aside when an Unreal editor or the game is already open: Brian may be playing.
# Usage: Tools/open_editor_with_link.sh   (waits until the link answers, up to 15 minutes)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$HERE/.." && pwd)"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
PORT="${TC_MCP_PORT:-8000}"
WAIT="${TC_MCP_WAIT:-900}"
URL="http://127.0.0.1:$PORT/mcp"

if [ "$PORT" = "8765" ]; then
  echo "Port 8765 belongs to the scratch editor. Use another." >&2
  exit 2
fi
if pgrep -x UnrealEditor > /dev/null; then
  echo "An Unreal editor or the game is already open, so a second one is not opened." >&2
  exit 2
fi

echo "Bringing the game's code up to date..."
"$HERE/build.sh" > /tmp/theater-command-build.log 2>&1 || { tail -n 30 /tmp/theater-command-build.log; exit 1; }

LOGS="$HOME/Library/Logs/TheaterCommand/EditorLink"
mkdir -p "$LOGS"
LOG="$LOGS/editor-link-$(date +%Y-%m-%d-%H%M%S).log"
echo "Opening the editor with the link awake (log: $LOG)..."
# Same game switches as the Play icon (minus -game/-windowed), so Play-In-Editor sees the same game.
# The log goes to EditorLink, not the Sessions folder, so Brian's own play logs stay his.
"$ENGINE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJECT_DIR/TheaterCommand.uproject" \
  -TheaterWorld -TheaterSpeedLog -ModelContextProtocolStartServer -ModelContextProtocolPort="$PORT" -abslog="$LOG" \
  > /dev/null 2>&1 &
disown

for ((i = 0; i < WAIT; i += 5)); do
  code="$(curl -s -o /dev/null -w '%{http_code}' -X POST "$URL" -H 'Content-Type: application/json' \
    -H 'Accept: application/json, text/event-stream' \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"tc-link-check","version":"1"}}}' 2> /dev/null)"
  if [ -n "$code" ] && [ "$code" != "000" ]; then
    echo "The link is awake at $URL (answered $code after ${i}s)."
    exit 0
  fi
  if ! pgrep -x UnrealEditor > /dev/null && [ "$i" -gt 30 ]; then
    echo "The editor closed before the link woke. See $LOG" >&2
    exit 1
  fi
  sleep 5
done
echo "The link did not answer within ${WAIT}s. See $LOG (look for ModelContextProtocol / 'Failed to listen on port')." >&2
exit 1
