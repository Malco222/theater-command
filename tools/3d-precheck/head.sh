#!/bin/bash
# Checks one pushed head of the 3D game:  head.sh <branch> <sha> [base-sha]
#   1. fetches the branch and checks the head out beside the others (never touches a helper's branch)
#   2. C++: every .cpp compile-checked against Unreal 5.8's headers; only what is new against the base is printed
#   3. rules side, when the bridge, the rules or the data changed: bundle rebuilt and compared, bridge type-checked
# The base defaults to a30710a, the last head the Mac built before the pre-check began.
set -uo pipefail
BRANCH="$1"; SHA="$2"; BASE="${3:-67e36d03}"
REPO=/home/user/wt/base
PRE=/home/user/precheck
git -C "$REPO" fetch -q origin "$BRANCH" || { echo "fetch failed"; exit 2; }
FULL=$(git -C "$REPO" rev-parse --verify -q "$SHA^{commit}") || { echo "$SHA is not on $BRANCH yet"; exit 2; }
SHORT=${FULL:0:7}
WT=/home/user/wt/h-$SHORT
[ -d "$WT" ] || git -C "$REPO" worktree add -f "$WT" "$FULL" > /dev/null 2>&1
[ -e "$WT/node_modules" ] || ln -s /home/user/wt/base/node_modules "$WT/node_modules"
echo "== $SHORT on $BRANCH: $(git -C "$REPO" log -1 --format=%s "$FULL")"
git -C "$REPO" merge-base --is-ancestor origin/unreal "$FULL" 2>/dev/null && echo "contains origin/unreal ($(git -C "$REPO" rev-parse --short origin/unreal)): fast-forwards" || echo "NOTE: does not contain origin/unreal $(git -C "$REPO" rev-parse --short origin/unreal): needs a merge first"
echo "-- changed in unreal/ since $BASE:"
git -C "$REPO" diff --stat "$BASE" "$FULL" -- unreal | tail -n 12
git -C "$REPO" diff --name-only "$BASE" "$FULL" -- 'unreal/TheaterCommand/Source/*.cs' 'unreal/TheaterCommand/Source/TheaterCommand/*.cs' unreal/TheaterCommand/TheaterCommand.uproject | sed 's/^/NOTE: build settings changed (the include paths here are the Mac snapshot'"'"'s): /'
echo "-- C++ compile check"
[ -f "$PRE/out/base-$BASE/summary.json" ] || { BW=/home/user/wt/b-$BASE; [ "$BASE" = 67e36d03 ] && BW=/home/user/wt/base; [ -d "$BW" ] || git -C "$REPO" worktree add -f "$BW" "$BASE" > /dev/null 2>&1; python3 "$PRE/check.py" "base-$BASE" "$BW/unreal/TheaterCommand" --link > /dev/null; }
python3 "$PRE/check.py" "h-$SHORT" "$WT/unreal/TheaterCommand" --link | grep -v '^ok \|^link step\|^  UNDEFINED'
python3 "$PRE/compare.py" "base-$BASE" "h-$SHORT"
# Unreal's build tool glues a game module's files into combined units once it has 32 of them; file-local helpers with
# the same name in two files then clash on the Mac although every file compiles alone.
NCPP=$(find "$WT/unreal/TheaterCommand/Source/TheaterCommand" -name '*.cpp' | wc -l)
if grep -q 'bUseUnity *= *false' "$WT/unreal/TheaterCommand/Source/TheaterCommand/TheaterCommand.Build.cs"; then echo "source files: $NCPP (combined builds are switched off in Build.cs)"
elif [ "$NCPP" -ge 32 ]; then echo "UNITY TRAP: $NCPP source files (32 or more): the Mac now builds them glued together and same-named file-local helpers clash. Add bUseUnity = false; to TheaterCommand.Build.cs"
else echo "source files: $NCPP (at 32 the Mac glues them together and 13 same-named helpers clash; $((32 - NCPP)) to go)"; fi
# data files the game reads at start: each changed .json must still parse
git -C "$REPO" diff --name-only --diff-filter=AM "$BASE" "$FULL" -- 'unreal/TheaterCommand/Content/*.json' | while read -r J; do
  python3 -I -c 'import json,sys; json.load(open(sys.argv[1], encoding="utf-8"))' "$WT/$J" 2>/tmp/json-err.txt && echo "data file parses: ${J#unreal/TheaterCommand/}" || echo "DATA FILE BROKEN: $J: $(tail -n 1 /tmp/json-err.txt)"
done
if ! git -C "$REPO" diff --quiet "$BASE" "$FULL" -- unreal/TheaterCommand/Rules unreal/TheaterCommand/Content/Rules src data; then
  echo "-- rules side"
  ( cd "$WT/unreal/TheaterCommand" && cp Content/Rules/campaign-rules.js /tmp/committed-$SHORT.js && bash Tools/build_rules.sh 2>&1 | tail -n 1
    if cmp -s Content/Rules/campaign-rules.js /tmp/committed-$SHORT.js; then echo "bundle: the committed file equals a fresh build"; else echo "BUNDLE: the committed file is NOT what the sources build"; cp /tmp/committed-$SHORT.js Content/Rules/campaign-rules.js; fi )
  cat > "$WT/tsconfig.bridge.json" <<'EOF'
{ "extends": "./tsconfig.json", "compilerOptions": { "noEmit": true, "types": [], "skipLibCheck": true }, "include": [], "files": ["unreal/TheaterCommand/Rules/campaign-bridge.ts"] }
EOF
  TB="$PRE/out/tsc-base-$BASE.txt"
  if [ ! -f "$TB" ]; then BW=/home/user/wt/b-$BASE; [ "$BASE" = 67e36d03 ] && BW=/home/user/wt/base; [ -d "$BW" ] || git -C "$REPO" worktree add -f "$BW" "$BASE" > /dev/null 2>&1; [ -e "$BW/node_modules" ] || ln -s /home/user/wt/base/node_modules "$BW/node_modules"
    cp "$WT/tsconfig.bridge.json" "$BW/tsconfig.bridge.json"; ( cd "$BW" && ./node_modules/.bin/tsc -p tsconfig.bridge.json 2>&1 | sed -E 's/\(([0-9]+),([0-9]+)\)/(L)/' | sort > "$TB"; rm -f tsconfig.bridge.json ); fi
  ( cd "$WT" && ./node_modules/.bin/tsc -p tsconfig.bridge.json 2>&1 | sed -E 's/\(([0-9]+),([0-9]+)\)/(L)/' | sort > /tmp/tsc-$SHORT.txt
    NEW=$(comm -13 "$TB" /tmp/tsc-$SHORT.txt)
    if [ -z "$NEW" ]; then echo "bridge type-check: nothing new ($(grep -c 'error TS' /tmp/tsc-$SHORT.txt) old errors, as on the base)"; else echo "BRIDGE type-check, new errors:"; echo "$NEW"; fi
    rm -f tsconfig.bridge.json )
fi
