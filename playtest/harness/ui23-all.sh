#!/bin/bash
# The Version 62 screens on every save worth a look. usage: ui23-all.sh
cd "$(dirname "$0")"; mkdir -p out/v62ui
run() { timeout 400 node ui23.cjs "out/v62saves/$1.json" "88-v62-$2" "$3" > "out/v62ui/$2.txt" 2>&1; echo "$2 done"; }
run SOVd-11-directive sov-job ""
run SOVd-11-thinning sov-men "early,hold"
run SOVd-11-doubt sov-doubt "secure,off"
run GERp-11-head ger-head "off"
run GERh-7-hard ger-take ""
run GERh-7-vow ger-vow "off"
run GERh-7-hold ger-hold "secure"
run GERh-7-short ger-men "early,hold"
