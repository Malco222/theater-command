#!/bin/bash
# Prepared defences from the player's seat (Version 58 on): probe28 for five countries on three seeds.
# usage: BIN=out58 ./def.sh OUT_DIR [STREAMS]
cd "$(dirname "$0")"; BIN="${BIN:-out}"; O="${1:-out/def}"; N="${2:-4}"; U="${UNTIL:-76}"; mkdir -p "$O"
GER='{"minOdds":1.4,"declare":[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":true}'
UK='{"minOdds":1.5,"land":true,"buy":["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]}'
FRA='{"minOdds":1.6,"homeOnly":true}'
SOV='{"minOdds":1.5}'
ITA='{"minOdds":1.4,"declare":[[10,"FRA"],[10,"UK"],[14,"GRE"]],"land":true}'
J="$O/jobs.tsv"; : > "$J"
for s in 7 11 23; do printf 'def-GER-%s\tGER\t%s\t%s\n' $s $s "$GER" >> "$J"; printf 'def-SOV-%s\tSOV\t%s\t%s\n' $s $s "$SOV" >> "$J"; printf 'def-UK-%s\tUK\t%s\t%s\n' $s $s "$UK" >> "$J"; printf 'def-FRA-%s\tFRA\t%s\t%s\n' $s $s "$FRA" >> "$J"; printf 'def-ITA-%s\tITA\t%s\t%s\n' $s $s "$ITA" >> "$J"; done
stream() { awk -v k="$1" -v n="$N" 'NR % n == k' "$J" | while IFS="$(printf '\t')" read -r name me seed plan; do node "$BIN/probe28.mjs" "$me" 1 "$seed" "$U" "$plan" > "$O/$name.txt" 2>&1; done; }
k=0; while [ "$k" -lt "$N" ]; do stream "$k" & k=$((k + 1)); done; wait
grep -h '^TOTAL' "$O"/def-*.txt > "$O/totals.txt"; echo "defence wars finished: $(grep -c '' "$J") asked, $(grep -c '' "$O/totals.txt") ended"
