#!/bin/bash
# Film critic: runs the whole morning chain on a synthetic night, end to end, and screenshots the report.
#   bash tests/run_chain.sh [WORK_DIR]
# Makes the night (tests/make_synthetic_night.py), checks it (check_night.py), fakes the two judges and last night's
# history (tests/make_fake_verdicts.py), builds the page (report.py), checks what came out (tests/check_outputs.py),
# and takes screenshots at 1440 and 390 wide (tests/screenshot.mjs, Playwright's Chromium).
set -euo pipefail
KIT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:-$(mktemp -d -t film-critic-e2e-XXXX)}"
rm -rf "$WORK/nights" "$WORK/work" "$WORK/judges" "$WORK/scenes" "$WORK/report"
mkdir -p "$WORK"
PY="python3 -I -B"  # -B: no __pycache__ left in the kit

$PY "$KIT/tests/make_synthetic_night.py" "$WORK"
NIGHT="$WORK/nights/2026-11-26"
$PY "$KIT/check_night.py" "$NIGHT" --rules "$KIT/tests/rules-test.yaml" --shots "$WORK/scenes/shots.yaml" \
  --out "$WORK/work/night-check.json"
$PY "$KIT/tests/make_fake_verdicts.py" "$WORK/work/night-check.json" "$WORK/judges"
$PY "$KIT/report.py" "$WORK/work/night-check.json" --verdicts "$WORK/judges/judge.json" \
  --verify "$WORK/judges/verify.json" --history "$WORK/judges/history.json" --out-dir "$WORK/report"
$PY "$KIT/tests/check_outputs.py" "$WORK"
if command -v node > /dev/null && [ -d /opt/node-tools/node_modules/playwright ]; then
  NODE_PATH=/opt/node-tools/node_modules node "$KIT/tests/screenshot.mjs" "$WORK/report/index.html" "$WORK/report"
else
  echo "no Playwright here: screenshots skipped"
fi
echo "report: $WORK/report/index.html"
