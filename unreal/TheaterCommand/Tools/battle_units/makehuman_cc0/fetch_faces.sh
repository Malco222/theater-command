#!/bin/bash
# Fetches the MakeHuman face shapes (CC0, as fetch.sh's) that variety.py mixes into each army's eight faces
# (Brian, 20 November 2026: "Make different faces for the soldiers"). Kept in the repository like the rest; this
# script shows where each came from and can fetch them again.
set -euo pipefail
cd "$(dirname "$0")"
D=https://raw.githubusercontent.com/makehumancommunity/makehuman/master/makehuman/data
for t in chin/chin-bones-incr chin/chin-cleft-incr chin/chin-height-incr chin/chin-jaw-drop-incr \
         chin/chin-prognathism-incr eyebrows/eyebrows-trans-forward eyebrows/eyebrows-trans-up \
         forehead/forehead-scale-vert-incr forehead/forehead-temple-incr forehead/forehead-trans-forward \
         head/head-diamond head/head-invertedtriangular head/head-rectangular head/head-scale-horiz-incr \
         head/head-scale-vert-incr cheek/l-cheek-inner-incr cheek/l-cheek-trans-down cheek/l-cheek-volume-incr \
         ears/l-ear-flap-incr ears/l-ear-lobe-incr ears/l-ear-rot-backward ears/l-ear-scale-incr \
         ears/l-ear-scale-vert-incr ears/l-ear-wing-incr eyes/l-eye-bag-incr eyes/l-eye-epicanthus-in \
         eyes/l-eye-height2-incr eyes/l-eye-push1-in eyes/l-eye-scale-incr eyes/l-eye-trans-up mouth/mouth-angles-up \
         mouth/mouth-laugh-lines-out mouth/mouth-philtrum-volume-incr mouth/mouth-scale-depth-incr \
         mouth/mouth-scale-vert-incr mouth/mouth-trans-forward mouth/mouth-upperlip-volume-incr nose/nose-base-up \
         nose/nose-curve-convex nose/nose-flaring-incr nose/nose-greek-incr nose/nose-nostrils-width-incr \
         nose/nose-point-up nose/nose-point-width-incr nose/nose-scale-depth-incr nose/nose-trans-forward \
         nose/nose-trans-up nose/nose-volume-incr nose/nose-width2-incr nose/nose-width3-incr \
         cheek/r-cheek-inner-incr cheek/r-cheek-trans-down cheek/r-cheek-volume-incr ears/r-ear-flap-incr \
         ears/r-ear-lobe-incr ears/r-ear-rot-backward ears/r-ear-scale-incr ears/r-ear-scale-vert-incr \
         ears/r-ear-wing-incr eyes/r-eye-bag-incr eyes/r-eye-epicanthus-in eyes/r-eye-height2-incr \
         eyes/r-eye-push1-in eyes/r-eye-scale-incr eyes/r-eye-trans-up; do
  curl -fsS -o "targets/$(basename $t).target" "$D/targets/$t.target"
done
echo "MakeHuman face shapes fetched."
