#!/bin/bash
# pearl_pin's own test: the campaign files' check in its two modes (the sea scene's ruling, 25 November 22:25Z), on a
# small repository of its own, and the Pearl and Midway pins gating in report mode on this tree.
#  - by default a changed campaign file fails, whoever changed it;
#  - PEARL_CAMPAIGN=report passes on a change by another lane and names the file and each commit (sha and subject);
#  - report mode still fails on a sea lane's commit ("Sea ..." or "Storms and night at sea ..."), on a change not yet
#    committed, and on any Pearl or Midway pin changed; an unknown PEARL_CAMPAIGN fails.
# Usage: Tools/sea_waves/pearl_pin_test.sh
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PIN="$HERE/pearl_pin.sh"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/pearl_pin_test.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
CHECKS=0
FAILED=0
check() {
  CHECKS=$((CHECKS + 1))
  if [ "$1" = "1" ]; then echo "  ok   $2"; else echo "  FAIL $2"; FAILED=$((FAILED + 1)); fi
}
# (the test's repository: the three campaign files, committed by a test author of its own)
REPO="$WORK/repo"
G() { git -C "$REPO" -c user.name="pearl_pin test" -c user.email="pearl-pin-test@example.invalid" -c commit.gpgsign=false "$@"; }
mkdir -p "$REPO/Source/TheaterCommand/Campaign"
git init -q "$REPO"
for F in CampaignMapActor CampaignHUD CampaignPlayerController; do
  printf '// %s\n' "$F" > "$REPO/Source/TheaterCommand/Campaign/$F.h"
  printf '// %s\nvoid %s() {}\n' "$F" "$F" > "$REPO/Source/TheaterCommand/Campaign/$F.cpp"
done
G add -A && G commit -q -m "Base"
BASE="$(G rev-parse HEAD)"
run() { # mode, then the output in OUT and the exit in CODE
  OUT="$(PEARL_ROOT="$REPO" PEARL_BASE="$BASE" PEARL_CAMPAIGN="$1" bash "$PIN" --campaign-only 2>&1)"
  CODE=$?
}
runs_default() {
  OUT="$(PEARL_ROOT="$REPO" PEARL_BASE="$BASE" bash "$PIN" --campaign-only 2>&1)"
  CODE=$?
}

runs_default
check "$([ $CODE -eq 0 ] && echo 1)" "nothing changed: the default passes"
run report
check "$([ $CODE -eq 0 ] && echo 1)" "nothing changed: report mode passes"

# (another lane's change: the map's HUD gains a row)
printf 'void Row() {}\n' >> "$REPO/Source/TheaterCommand/Campaign/CampaignHUD.cpp"
G commit -qam "Map: the month's row on the HUD"
MAPSHA="$(G rev-parse --short HEAD)"
runs_default
check "$([ $CODE -ne 0 ] && printf '%s' "$OUT" | grep -q "FAIL Campaign/CampaignHUD" && echo 1)" "the default fails on a changed campaign file, whoever changed it"
run report
check "$([ $CODE -eq 0 ] && echo 1)" "report mode passes on another lane's change"
check "$(printf '%s' "$OUT" | grep -q "note Campaign/CampaignHUD.{h,cpp} changed since" && printf '%s' "$OUT" | grep -q "$MAPSHA Map: the month's row on the HUD" && echo 1)" \
  "report mode names the changed file and its commit, sha and subject"
check "$(printf '%s' "$OUT" | grep -q "ok   Campaign/CampaignMapActor" && echo 1)" "report mode still says the unchanged files are as at the base"

# (a sea lane's commit on a side branch, merged in: followed through the merge)
G checkout -q -b sea "$BASE"
printf '// one more\n' >> "$REPO/Source/TheaterCommand/Campaign/CampaignMapActor.h"
G commit -qam "Sea look: a hook on the map"
SEASHA="$(G rev-parse --short HEAD)"
G checkout -q -
G merge -q --no-edit sea
run report
check "$([ $CODE -ne 0 ] && printf '%s' "$OUT" | grep -q "FAIL Campaign/CampaignMapActor.{h,cpp} was touched by a sea lane" && printf '%s' "$OUT" | grep -q "$SEASHA Sea look: a hook on the map" && echo 1)" \
  "report mode fails on a sea lane's commit (\"Sea ...\"), merged in from a side branch, and names it"
G reset -q --hard HEAD~1
printf 'void Storm() {}\n' >> "$REPO/Source/TheaterCommand/Campaign/CampaignPlayerController.cpp"
G commit -qam "Storms and night at sea: a key on the map"
run report
check "$([ $CODE -ne 0 ] && printf '%s' "$OUT" | grep -q "FAIL Campaign/CampaignPlayerController.{h,cpp} was touched by a sea lane" && echo 1)" \
  "report mode fails on the storms lane's commit (\"Storms and night at sea ...\")"
G reset -q --hard HEAD~1

# (a change not committed yet: no commit can be named for it)
printf '// loose\n' >> "$REPO/Source/TheaterCommand/Campaign/CampaignMapActor.cpp"
run report
check "$([ $CODE -ne 0 ] && printf '%s' "$OUT" | grep -q "FAIL Campaign/CampaignMapActor.{h,cpp} has a change not yet committed" && echo 1)" \
  "report mode fails on a change not yet committed"
G checkout -q -- .

run everything
check "$([ $CODE -ne 0 ] && printf '%s' "$OUT" | grep -q "FAIL PEARL_CAMPAIGN=everything" && echo 1)" "an unknown PEARL_CAMPAIGN fails"

# (the Pearl and Midway pins on this tree, in report mode: as they stand they pass; one pin changed fails)
OUT="$(PEARL_CAMPAIGN=report bash "$PIN" 2>&1)"
CODE=$?
check "$([ $CODE -eq 0 ] && printf '%s' "$OUT" | grep -q "pins, 0 changed" && echo 1)" "report mode on this tree: every Pearl and Midway pin as pinned"
# (the first pin's digest, its first character turned: 0 to 1, anything else to 0)
awk '!done && /^[0-9a-f]{64}  / { $0 = (substr($0, 1, 1) == "0" ? "1" : "0") substr($0, 2); done = 1 } { print }' "$HERE/pearl.sha" > "$WORK/pearl.sha"
OUT="$(PEARL_CAMPAIGN=report PEARL_PINS="$WORK/pearl.sha" bash "$PIN" 2>&1)"
CODE=$?
check "$([ $CODE -ne 0 ] && printf '%s' "$OUT" | grep -q "pins, 1 changed" && echo 1)" "report mode still fails on one Pearl or Midway pin changed"
cmp -s "$HERE/pearl.sha" "$WORK/pearl.sha"
check "$([ $? -ne 0 ] && echo 1)" "(the test changed its own copy of the pins, never pearl.sha)"

echo "pearl_pin_test: $CHECKS checks, $FAILED failed"
[ "$FAILED" -eq 0 ]
