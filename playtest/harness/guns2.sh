#!/bin/bash
# Version 58: the same wars as guns.sh, with a player who also brings the guns into the attack (plan option useGuns:
# up to two artillery corps take the place of the weakest corps beside them where the card then reads better).
# usage: BIN=out58 ./guns2.sh OUT_DIR [STREAMS]     (every battle is printed as a row: ROWS=1)
cd "$(dirname "$0")"; BIN="${BIN:-out}"; O="${1:-out/guns2}"; N="${2:-4}"; U="${UNTIL:-76}"; mkdir -p "$O"
GER='"minOdds":1.4,"declare":[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":true'
SOV='"minOdds":1.5'
SOVF='"minOdds":1.4,"declare":[[10,"GER"]]'
J="$O/jobs.tsv"; : > "$J"
for s in 7 11 23; do for w in GER SOV SOVF; do
  eval "p=\$$w"; me=$w; [ "$w" = SOVF ] && me=SOV
  printf 'def-%s_gunsup-%s\t%s\t%s\t{%s,"guns":1.5,"useGuns":true}\n' $w $s $me $s "$p" >> "$J"
  printf 'def-%s_bothup-%s\t%s\t%s\t{%s,"guns":1.5,"useGuns":true,"minChance":0.6}\n' $w $s $me $s "$p" >> "$J"
done; done
stream() { awk -v k="$1" -v n="$N" 'NR % n == k' "$J" | while IFS="$(printf '\t')" read -r name me seed plan; do ROWS=1 node "$BIN/probe28.mjs" "$me" 1 "$seed" "$U" "$plan" > "$O/$name.txt" 2>&1; done; }
k=0; while [ "$k" -lt "$N" ]; do stream "$k" & k=$((k + 1)); done; wait
for f in "$O"/def-*.txt; do echo "$(basename "$f" .txt) $(grep -h '^TOTAL' "$f")"; done > "$O/totals.txt"; echo "gun wars finished: $(grep -c '' "$J") asked, $(grep -c 'TOTAL' "$O/totals.txt") ended"
