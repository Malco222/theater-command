#!/bin/bash
# usage: clickq.sh LIST   (lines: PREFIX STEPFILE PAGE-URL); plays each list in turn with click.cjs, skipping those already done
HERE="$(cd "$(dirname "$0")" && pwd)"; cd "$HERE"
while read -r PRE STEPS PAGEURL; do
  [ -z "$PRE" ] && continue
  [ -s "out/click/$PRE.summary.txt" ] && [ -e "out/click/.done-$PRE" ] && continue
  TC_PAGE="$PAGEURL" timeout 2700 node click.cjs "$STEPS" "$PRE" > "out/click/$PRE.out" 2>&1 && touch "out/click/.done-$PRE"
  echo "$PRE: $(sed -n 2p out/click/$PRE.summary.txt 2>/dev/null)"
done < "$1"
