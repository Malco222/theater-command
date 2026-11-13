#!/bin/bash
# Version 58: is the attack card honest? probe29 for a German and a Soviet player on three seeds.
# usage: BIN=out58 ./card.sh OUT_DIR
cd "$(dirname "$0")"; BIN="${BIN:-out}"; O="${1:-out/card}"; mkdir -p "$O"
GER='{"minOdds":1.4,"declare":[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":true}'
SOV='{"minOdds":1.5}'
for s in 7 11 23; do node "$BIN/probe29.mjs" GER 1 $s 76 "$GER" > "$O/card-GER-$s.txt" 2>&1 & node "$BIN/probe29.mjs" SOV 1 $s 76 "$SOV" > "$O/card-SOV-$s.txt" 2>&1 & wait; done
echo "card wars finished: $(grep -l '^TOTAL' "$O"/card-*.txt | wc -l) of 6"
