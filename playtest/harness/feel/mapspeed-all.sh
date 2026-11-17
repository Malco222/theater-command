#!/bin/bash
# The map's speed on one built version, both saves, both windows.  mapspeed-all.sh <worktree> <tag>
SP=/tmp/claude-0/-home-user/e1c93ea1-094d-53b5-a3e2-a01a878cea53/scratchpad/pt
W=$1; T=$2; cd $SP
for z in 1600x1000 1440x900; do
  TC_SIZE=$z TC_PAGE=file://$W/dist/play.html timeout 130 node mapspeed.cjs out/click/us42d.save.txt > out/mapspeed-$T-us42-$z.txt 2>&1
  TC_SIZE=$z TC_PAGE=file://$W/dist/play.html timeout 130 node mapspeed.cjs /home/user/tc-harness/playtest/harness/feel/germany-december-1939.save.txt > out/mapspeed-$T-ger-$z.txt 2>&1
done
echo done > out/mapspeed-$T.done
