#!/bin/bash
# Compiles the game's C++ for the Unreal editor on this Mac.
#
# While any Unreal editor is open, the build tool gives each new build a numbered file name
# and now and then forgets to record which one is current, so the game would start with stale
# code. This script checks for that and builds once more if it happened, then clears out the
# numbered leftovers.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$HERE/.." && pwd)"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
BIN="$PROJECT_DIR/Binaries/Mac"

build() {
  "$ENGINE/Engine/Build/BatchFiles/Mac/Build.sh" TheaterCommandEditor Mac Development -project="$PROJECT_DIR/TheaterCommand.uproject" -waitmutex
}

current() { sed -n 's/.*"TheaterCommand": "\(.*\)".*/\1/p' "$BIN/UnrealEditor.modules" 2>/dev/null; }
newest() { local f; f="$(ls -t "$BIN"/libUnrealEditor-TheaterCommand*.dylib 2>/dev/null | head -n 1)"; [ -n "$f" ] && basename "$f"; }

build || exit 1
if [ "$(current)" != "$(newest)" ]; then
  echo "The build tool did not record the new build. Building once more..."
  build || exit 1
fi
if [ "$(current)" != "$(newest)" ]; then
  echo "The newest build is $(newest) but the game would load $(current)." >&2
  exit 1
fi

# Remove builds that are no longer the current one.
for f in "$BIN"/libUnrealEditor-TheaterCommand*.dylib; do
  [ "$(basename "$f")" = "$(current)" ] || rm -f "$f"
done
echo "Built: $(current)"
