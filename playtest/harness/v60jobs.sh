#!/bin/bash
# Writes the job list for the Version 60 test (read by jobs.sh). usage: BIN=out60 TC_PAGE=file:///home/user/tc60/dist/play.html ./v60jobs.sh > v60.tsv
#   gov-*    the government wars (probe24): A a player who leaves the cabinet alone, B an idle player, C four picked cabinets, D sets all aside,
#            E an obedient player, H a hawk table, V a dove table, P promises (safe, fair, bold, any), K heads (sack, stand by)
#   men-*    men (probe32): as before, and holding back new corps under half and under a quarter of the reserve
#   off-*    the cabinet switched off against the same war with it on (probe33); two wars switch it on again in March 1942
#   battery  the standing battery, one job a file (play, late, level, sweeps, idle, falls, monkeys), the scorched earth test (probe20)
#   berlin-* the German player who follows history on seeds 1 to 24 (probe28);  watch-* computer-only wars, seeds 1 to 36, to January 1947
B="${BIN:-out60}"; PG="${TC_PAGE:-file:///home/user/tc60/dist/play.html}"
GER='"minOdds":1.4,"declare":[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":true'
UK='"minOdds":1.5,"land":true,"buy":["corps:arm","corps:inf","air:ftr","ship:dd","work:factory"]'
FRA='"minOdds":1.6,"homeOnly":true'
SOV='"minOdds":1.5'
SOVF='"minOdds":1.4,"declare":[[10,"GER"]]'
ITA='"minOdds":1.4,"declare":[[10,"FRA"],[10,"UK"],[14,"GRE"]],"land":true'
POL='"minOdds":1.6,"homeOnly":true'
BEL='"minOdds":1.6,"homeOnly":true,"path":"allies"'
FIN='"minOdds":1.6,"homeOnly":true,"path":"league"'
TUR='"minOdds":1.4,"path":"axis"'
ROM='"minOdds":1.5,"path":"axis"'
SWE='"minOdds":1.5,"path":"league"'
job() { printf '%s\t%s\n' "$1" "$2"; }
gov() { job "gov-$1" "node $B/probe24.mjs $2 1 $3 76 '{$4}' $5"; }   # name, country, seed, plan, cabinet
# P, K, H, V: what Version 60 added
for s in 7 11; do for w in GER SOV UK; do eval "p=\$$w"
  for v in safe fair bold any; do gov vow-$w-$s-$v $w $s "$p,\"vow\":\"$v\"" none; done
  gov head-$w-$s-sack $w $s "$p,\"head\":\"sack\"" none; gov head-$w-$s-stand $w $s "$p,\"head\":\"stand\"" none; done; done
for s in 7 11; do for w in GER SOV UK ITA FRA; do eval "p=\$$w"; gov hawks-$w-$s $w $s "$p,\"chase\":true" hawks; gov doves-$w-$s $w $s "$p" doves; done; done
# M: men
for s in 7 11 23; do for w in SOV SOVF GER; do eval "p=\$$w"; me=$w; [ "$w" = SOVF ] && me=SOV
  job men-$w-$s-asis "node $B/probe32.mjs $me 1 $s 76 '{$p}'"; job men-$w-$s-half "node $B/probe32.mjs $me 1 $s 76 '{$p,\"menKeep\":0.5}'"; job men-$w-$s-quarter "node $B/probe32.mjs $me 1 $s 76 '{$p,\"menKeep\":0.25}'"; done; done
# O: the cabinet off
for s in 7 11; do for w in GER UK FRA SOV ITA; do eval "p=\$$w"
  job off-$w-$s-off "node $B/probe33.mjs $w 1 $s 76 '{$p,\"noCabinet\":true}'"; job off-$w-$s-on "node $B/probe33.mjs $w 1 $s 76 '{$p}'"; done; done
job off-GER-7-again "node $B/probe33.mjs GER 1 7 76 '{$GER,\"noCabinet\":true}' 31"; job off-SOV-7-again "node $B/probe33.mjs SOV 1 7 76 '{$SOV,\"noCabinet\":true}' 31"
# A, B, E, C, D: the government wars as on Version 57
for s in 7 11 23; do for w in GER UK FRA SOV ITA POL; do eval "p=\$$w"; gov play-$w-$s $w $s "$p" none; done; done
for w in BEL FIN TUR ROM SWE; do eval "p=\$$w"; gov play-$w-7 $w 7 "$p" none; done
for s in 7 11; do for w in GER UK FRA SOV ITA POL BEL SWE TUR; do gov idle-$w-$s $w $s '"idle":true' none; done; done
for s in 7 11 23; do for w in GER UK FRA SOV ITA; do eval "p=\$$w"; gov chase-$w-$s $w $s "\"chase\":true,$p" none; done; done
for c in money men calm most; do gov cab-GER-7-$c GER 7 "$GER" $c; gov cab-UK-7-$c UK 7 "$UK" $c; done
gov aside-GER-7 GER 7 "$GER" aside; gov aside-UK-7 UK 7 "$UK" aside; gov aside-SOV-7 SOV 7 "$SOV" aside
# The battery
for p in GER GER_WEST GER_SEALION UK FRA FRA_ATTACK SOV SOV_FIRST ITA ITA_BALKAN POL BEL FIN TUR ROM SWE ESP; do job play-$p "node $B/play.mjs $p 7,11,23 1 76 colonel"; done
for t in 5 17 29; do job late-$t "node $B/play.mjs GER,UK,SOV,ITA,FRA 7 $t 76 colonel"; done
job level-marshal "node $B/play.mjs GER,UK,FRA,SOV,ITA 7 1 76 marshal"; job level-cadet "node $B/play.mjs GER,UK,FRA,SOV,ITA 7 1 76 cadet"
job minor-sweep "node $B/play.mjs BEL,FIN 1,2,3 1 76 colonel"; job pol-sweep "node $B/play.mjs POL 1,2,3,4,5,6,8,9,10,12 1 76 colonel"
job idle-7 "node $B/idle.mjs GER,UK,FRA,SOV,ITA,POL 7 1 76"; job idle-GER-90 "node $B/idle.mjs GER 7 1 90"; job idle-ITA-88 "node $B/probe10.mjs ITA 7 88"
job play-GER_STARVE "node $B/play.mjs GER_STARVE 7,11 1 40 colonel"
job falls-a "node $B/probe11.mjs 1,2,3,4,5,6 88"; job falls-b "node $B/probe11.mjs 7,8,9,10,11,12 88"
n=0; for a in "GER 31 2500 1" "SOV 32 2200 1" "FRA 33 1500 1 marshal" "POL 34 1200 1" "UK 35 2200 1" "ITA 36 2200 5" "GER 37 2200 29 cadet" "BEL 38 1200 1"; do n=$((n + 1)); job monkey-$n "TC_PAGE=$PG node monkey.cjs $a"; done
for s in 7 11 23; do job scorch-GER-$s-asis "node $B/probe20.mjs GER 1 $s 64 '{$GER}'"; job scorch-GER-$s-never "node $B/probe20.mjs GER 1 $s 64 '{$GER}' SOV=never"
  job scorch-SOV-$s-everywhere "node $B/probe20.mjs SOV 1 $s 64 '{$SOV}' - everywhere"; job scorch-SOV-$s-never "node $B/probe20.mjs SOV 1 $s 64 '{$SOV}' - never"; done
# The builders' claims
for s in $(seq 1 24); do job berlin-GER-$s "node $B/probe28.mjs GER 1 $s 76 '{$GER}'"; done
for k in 1,2,3,4,5,6 7,8,9,10,11,12 13,14,15,16,17,18 19,20,21,22,23,24 25,26,27,28,29,30 31,32,33,34,35,36; do job watch-$(echo $k | cut -d, -f1) "node $B/watch.mjs $k 89"; done
