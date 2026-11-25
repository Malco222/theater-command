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

# Both of the project's modules: the game, and the editor's side (Claude's playtest tools, release 2), which the Play
# icon never loads. Each module's files are matched by its own name only: libUnrealEditor-TheaterCommand*.dylib would
# take the editor's library for the game's newest build, and delete it as a leftover.
MODULES=(TheaterCommand TheaterCommandEditor)
current() { sed -n "s/.*\"$1\": \"\(.*\)\".*/\1/p" "$BIN/UnrealEditor.modules" 2>/dev/null; }
libs() { ls -t "$BIN/libUnrealEditor-$1.dylib" "$BIN/libUnrealEditor-$1-"[0-9]*.dylib 2>/dev/null; }
newest() { local f; f="$(libs "$1" | head -n 1)"; [ -n "$f" ] && basename "$f"; }
# True when any module's newest build is not the one recorded.
stale() { local m; for m in "${MODULES[@]}"; do [ "$(current "$m")" = "$(newest "$m")" ] || return 0; done; return 1; }

build || exit 1
if stale; then
  echo "The build tool did not record the new build. Building once more..."
  build || exit 1
fi
if [ "$(current TheaterCommand)" != "$(newest TheaterCommand)" ]; then
  echo "The newest build is $(newest TheaterCommand) but the game would load $(current TheaterCommand)." >&2
  exit 1
fi
# (the editor's side only warns: the Play icon never loads it)
if [ "$(current TheaterCommandEditor)" != "$(newest TheaterCommandEditor)" ]; then
  echo "Warning: the editor would load $(current TheaterCommandEditor), not the newest build of Claude's playtest tools ($(newest TheaterCommandEditor))." >&2
fi

# Remove builds that are no longer the current one.
for m in "${MODULES[@]}"; do
  libs "$m" | while read -r f; do
    [ "$(basename "$f")" = "$(current "$m")" ] || rm -f "$f"
  done
done
echo "Built: $(current TheaterCommand)"
