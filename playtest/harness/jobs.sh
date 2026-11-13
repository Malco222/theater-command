#!/bin/bash
# A queue of wars that can be picked up again after a stop. usage: jobs.sh JOBS.tsv OUT_DIR [STREAMS]
# Each line of JOBS.tsv is NAME<TAB>COMMAND. The command's output goes to OUT_DIR/NAME.txt; a job that finished leaves
# OUT_DIR/.done/NAME and is skipped the next time, so the same call after a restart plays only what is missing.
cd "$(dirname "$0")"; J="$1"; O="$2"; N="${3:-4}"; mkdir -p "$O/.done"; export O
run_one() { [ -e "$O/.done/$1" ] && exit 0; bash -c "$2" > "$O/$1.txt" 2>&1; touch "$O/.done/$1"; }
export -f run_one
while IFS="$(printf '\t')" read -r name cmd; do [ -n "$name" ] && [ ! -e "$O/.done/$name" ] && printf '%s\0%s\0' "$name" "$cmd"; done < "$J" | xargs -0 -n 2 -P "$N" bash -c 'run_one "$0" "$1"'
echo "queue $(basename "$J"): $(grep -c '' "$J") jobs, $(ls "$O/.done" | wc -l) done at $(date -u +%H:%M)"
