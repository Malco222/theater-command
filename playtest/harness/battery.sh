#!/bin/bash
# The whole playtest battery in four streams: about 115 wars and 8 random-click sessions.
# usage: [BIN=out56 TC_PAGE=file:///path/dist/play.html] battery.sh OUT_DIR   (BIN is the folder of built programs, default out; build first: for n in play idle watch probe10 probe11; do ./build.sh $n; done; the page needs npm run build in the repo)
HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="${1:?out dir}"; mkdir -p "$OUT"; cd "$HERE"
BIN="${BIN:-out}"
play() { node $BIN/play.mjs "$@" 2>&1; }
( for p in GER GER_WEST GER_SEALION UK FRA FRA_ATTACK; do play $p 7,11,23 1 76 colonel > "$OUT/play-$p.txt"; done
  for a in "GER 31 2500 1" "SOV 32 2200 1" "FRA 33 1500 1 marshal" "POL 34 1200 1"; do node monkey.cjs $a >> "$OUT/monkey.txt" 2>&1; done; echo done > "$OUT/a.done" ) &
( for p in SOV SOV_FIRST ITA ITA_BALKAN POL BEL; do play $p 7,11,23 1 76 colonel > "$OUT/play-$p.txt"; done
  node $BIN/probe11.mjs 1,2,3,4,5,6,7,8,9,10,11,12 88 > "$OUT/falls-and-assaults.txt" 2>&1; echo done > "$OUT/b.done" ) &
( for p in FIN TUR ROM SWE ESP; do play $p 7,11,23 1 76 colonel > "$OUT/play-$p.txt"; done
  play GER,UK,SOV,ITA,FRA 7 5 76 colonel > "$OUT/late-5.txt"; play GER,UK,SOV,ITA,FRA 7 17 76 colonel > "$OUT/late-17.txt"; play GER,UK,SOV,ITA,FRA 7 29 76 colonel > "$OUT/late-29.txt"
  for a in "UK 35 2200 1" "ITA 36 2200 5" "GER 37 2200 29 cadet" "BEL 38 1200 1"; do node monkey.cjs $a >> "$OUT/monkey-b.txt" 2>&1; done; echo done > "$OUT/c.done" ) &
( node $BIN/watch.mjs 1,2,3,4,5,6,7,8,9,10,11,12 88 > "$OUT/watch.txt" 2>&1
  play GER,UK,FRA,SOV,ITA 7 1 76 marshal > "$OUT/level-marshal.txt"; play GER,UK,FRA,SOV,ITA 7 1 76 cadet > "$OUT/level-cadet.txt"
  play BEL,FIN 1,2,3 1 76 colonel > "$OUT/minor-sweep.txt"; play POL 1,2,3,4,5,6,8,9,10,12 1 76 colonel > "$OUT/pol-sweep.txt"
  node $BIN/idle.mjs GER,UK,FRA,SOV,ITA,POL 7 1 76 > "$OUT/idle-7.txt" 2>&1; node $BIN/idle.mjs GER 7 1 90 > "$OUT/idle-GER-90.txt" 2>&1; node $BIN/probe10.mjs ITA 7 88 > "$OUT/idle-ITA-88.txt" 2>&1
  play GER_STARVE 7,11 1 40 colonel > "$OUT/play-GER_STARVE.txt"; echo done > "$OUT/d.done" ) &
wait
echo "battery finished $(date -u +%H:%M)"
