#!/bin/bash
# Fetch the Freesound public preview files listed in urls.tsv (id, item, url).
# Plain client, one file at a time, 2 s pause. Writes audio/<id>.ogg (or .mp3) and fetch-log.tsv.
set -u
cd "$(dirname "$0")"
mkdir -p audio
free_gb=$(df -g "$HOME" | awk 'NR==2{print $4}')
if [ "${free_gb:-0}" -lt 3 ]; then echo "STOP: only ${free_gb} GB free"; exit 1; fi
ua="sound-research-client/1.0"
: > fetch-log.tsv
ok=0; bad=0
while IFS=$'\t' read -r id item url; do
  [ -z "${id:-}" ] && continue
  if [ -s "audio/$id.ogg" ] || [ -s "audio/$id.mp3" ]; then ok=$((ok+1)); continue; fi
  code=$(curl -sS -A "$ua" --max-time 180 -o "audio/$id.part" -w "%{http_code}" "$url")
  if [ "$code" = "200" ]; then mv "audio/$id.part" "audio/$id.ogg"; ext=ogg
  else
    rm -f "audio/$id.part"; sleep 2
    url2="${url%-hq.ogg}-hq.mp3"
    code=$(curl -sS -A "$ua" --max-time 180 -o "audio/$id.part" -w "%{http_code}" "$url2")
    if [ "$code" = "200" ]; then mv "audio/$id.part" "audio/$id.mp3"; ext=mp3; else rm -f "audio/$id.part"; ext=none; fi
  fi
  if [ "$ext" = none ]; then bad=$((bad+1)); else ok=$((ok+1)); fi
  printf '%s\t%s\t%s\t%s\n' "$id" "$item" "$code" "$ext" >> fetch-log.tsv
  sleep 2
done < urls.tsv
size=$(du -sh audio | cut -f1)
echo "DONE: $ok fetched, $bad failed, total $size"
