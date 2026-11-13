#!/bin/bash
# Scorched earth A/B on Version 58: the same wars with the burning on and off.
cd "$(dirname "$0")"; O=out/v58x
G='{"minOdds":1.4,"declare":[[8,"DEN"],[9,"LUX"],[9,"BEL"],[9,"NED"],[20,"YUG"],[22,"SOV"]],"land":true}'
S='{"minOdds":1.5}'
( for s in 7 11 23; do node out58/probe20.mjs GER 1 $s 64 "$G" > $O/scorch-GER-$s-asis.txt 2>&1; done ) &
( for s in 7 11 23; do node out58/probe20.mjs GER 1 $s 64 "$G" SOV=never > $O/scorch-GER-$s-never.txt 2>&1; done ) &
( for s in 7 11 23; do node out58/probe20.mjs SOV 1 $s 64 "$S" - everywhere > $O/scorch-SOV-$s-everywhere.txt 2>&1; done ) &
( for s in 7 11 23; do node out58/probe20.mjs SOV 1 $s 64 "$S" - never > $O/scorch-SOV-$s-never.txt 2>&1; done ) &
wait; echo scorch runs finished
