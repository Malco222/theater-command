#!/bin/bash
# The government test wars (Version 57 on): every directive a player is given, the cabinet's trust, when it falls,
# and what ministers and decrees do to the budget. About 70 wars in streams side by side.
# usage: BIN=out57 ./gov.sh OUT_DIR [STREAMS]      (UNTIL=76 by default: December 1945)
cd "$(dirname "$0")"; BIN="${BIN:-out}"; O="${1:-out/gov}"; N="${2:-4}"; U="${UNTIL:-76}"; mkdir -p "$O"
GER='{"minOdds":1.4,"declare":[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":true}'
UK='{"minOdds":1.5,"land":true,"buy":["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]}'
FRA='{"minOdds":1.6,"homeOnly":true}'
SOV='{"minOdds":1.5}'
ITA='{"minOdds":1.4,"declare":[[10,"FRA"],[10,"UK"],[14,"GRE"]],"land":true}'
POL='{"minOdds":1.6,"homeOnly":true}'
BEL='{"minOdds":1.6,"homeOnly":true,"path":"allies"}'
FIN='{"minOdds":1.6,"homeOnly":true,"path":"league"}'
TUR='{"minOdds":1.4,"path":"axis"}'
ROM='{"minOdds":1.5,"path":"axis"}'
SWE='{"minOdds":1.5,"path":"league"}'
IDLE='{"idle":true}'
J="$O/jobs.tsv"; : > "$J"
job() { printf '%s\t%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" "$5" >> "$J"; }   # name, country, seed, plan, cabinet
# A. A scripted player who leaves the cabinet alone: which directives come, and can they be done.
for s in 7 11 23; do job play-GER-$s GER $s "$GER" none; job play-UK-$s UK $s "$UK" none; job play-FRA-$s FRA $s "$FRA" none; job play-SOV-$s SOV $s "$SOV" none; job play-ITA-$s ITA $s "$ITA" none; job play-POL-$s POL $s "$POL" none; done
job play-BEL-7 BEL 7 "$BEL" none; job play-FIN-7 FIN 7 "$FIN" none; job play-TUR-7 TUR 7 "$TUR" none; job play-ROM-7 ROM 7 "$ROM" none; job play-SWE-7 SWE 7 "$SWE" none
# B. A player who only ends the month: how long does the cabinet last.
for s in 7 11; do for me in GER UK FRA SOV ITA POL USA BEL SWE TUR; do job idle-$me-$s $me $s "$IDLE" none; done; done
# C. The same wars with a cabinet picked for money, for men, for calm, and with everything.
for s in 7 11; do for c in money men calm most; do job cab-GER-$s-$c GER $s "$GER" $c; job cab-SOV-$s-$c SOV $s "$SOV" $c; job cab-UK-$s-$c UK $s "$UK" $c; done; done
# D. A player who sets every directive aside at once.
job aside-GER-7 GER 7 "$GER" aside; job aside-UK-7 UK 7 "$UK" aside; job aside-SOV-7 SOV 7 "$SOV" aside
stream() { awk -v k="$1" -v n="$N" 'NR % n == k' "$J" | while IFS="$(printf '\t')" read -r name me seed plan cab; do node "$BIN/probe24.mjs" "$me" 1 "$seed" "$U" "$plan" "$cab" > "$O/$name.txt" 2>&1; done; }
k=0; while [ "$k" -lt "$N" ]; do stream "$k" & k=$((k + 1)); done; wait
grep -h '^TOTAL' "$O"/*.txt > "$O/totals.txt"; grep -l '^ERROR' "$O"/*.txt > "$O/errors.txt" 2>/dev/null
echo "government wars finished: $(grep -c '' "$J") asked, $(grep -c '' "$O/totals.txt") ended, $(grep -c '' "$O/errors.txt") with an error"
