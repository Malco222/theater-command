#!/bin/bash
# Fetches the open MakeHuman assets the battle soldiers are built on (all released CC0 by the MakeHuman team:
# free for any use, no credit required; we credit them anyway in CREDITS.md). They are kept in the repository, so a
# build needs no network; this script only shows where each file came from and can fetch them again.
set -euo pipefail
cd "$(dirname "$0")"
R=https://raw.githubusercontent.com/makehumancommunity/makehuman/master
curl -fsS -o LICENSE.md "$R/LICENSE.md"
D=$R/makehuman/data
curl -fsS -o base.obj "$D/3dobjs/base.obj"
curl -fsS -o default.mhskel "$D/rigs/default.mhskel"
curl -fsS -o default_weights.mhw "$D/rigs/default_weights.mhw"
curl -fsS -o eyes/high-poly.obj "$D/eyes/high-poly/high-poly.obj"
curl -fsS -o eyes/brown_eye.png "$D/eyes/materials/brown_eye.png"
for t in macrodetails/caucasian-male-young macrodetails/african-male-young macrodetails/asian-male-young \
         macrodetails/universal-male-young-maxmuscle-averageweight macrodetails/universal-male-young-averagemuscle-minweight \
         macrodetails/height/male-young-averagemuscle-averageweight-maxheight macrodetails/height/male-young-averagemuscle-averageweight-minheight \
         macrodetails/proportions/male-young-averagemuscle-averageweight-idealproportions \
         head/head-oval head/head-square head/head-round head/head-triangular head/head-fat-incr head/head-age-incr \
         nose/nose-scale-horiz-incr nose/nose-scale-vert-incr nose/nose-hump-incr nose/nose-width1-incr \
         chin/chin-prominent-incr chin/chin-width-incr mouth/mouth-scale-horiz-incr mouth/mouth-lowerlip-volume-incr \
         eyebrows/eyebrows-trans-down eyebrows/eyebrows-angle-up cheek/l-cheek-bones-incr cheek/r-cheek-bones-incr neck/neck-scale-horiz-incr; do
  curl -fsS -o "targets/$(basename $t).target" "$D/targets/$t.target"
done
echo "MakeHuman CC0 assets fetched."
