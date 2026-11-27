#!/bin/bash
# lane_lift.sh FXPREVIEW RECIPE OUT : the oil lane seen low (the look reviews of 27 November): a ship's lane 900 s old
# (a straight one: the previewer's "deck" lane runs straight unless FX_TURN bends it) drawn from another ship's bridge
# 760 m off and 20 m up, and 2 km off and 30 m up, at 60 and 20 degree lenses, and from straight above; then the same
# low views on a night sea (FX_NIGHT=1: the scene's seed-mode night, no calm band) and at a dusk (FX_DIM=0.35: field
# mode's, the band toward the dimmed sky at that share); each beside the same scene with the strip unpainted
# (FX_NO_LANE=1), graded with game_curve.py show; then lane_lift.py holds what the lane adds. RECIPE is flatten.py's
# output of fx.json.
set -u
X="${1:?fxpreview}"; R="${2:?recipe}"; O="${3:?out}"
HERE="$(cd "$(dirname "$0")" && pwd)"; G="$HERE/../game_curve.py"; S=800x450
mkdir -p "$O"; export FX_FLOAT=1 FX_SINCE=900 FX_MPS=8
for NL in 0 1; do
  if [ "$NL" = 1 ]; then export FX_NO_LANE=1; P=lane-nl; else unset FX_NO_LANE; P=lane; fi
  FX_EYE=-1255,-1246,20 FX_AT=-1455,-516,0 FX_LENS=60 "$X" "$R" "$O" deck "$P-760" 0 $S > /dev/null
  FX_EYE=-1255,-1246,20 FX_AT=-1455,-516,0 FX_LENS=20 "$X" "$R" "$O" deck "$P-760t" 0 $S > /dev/null
  FX_EYE=-927,-2444,30 FX_AT=-1455,-516,0 FX_LENS=60 "$X" "$R" "$O" deck "$P-2k" 0 $S > /dev/null
  FX_EYE=-927,-2444,30 FX_AT=-1455,-516,0 FX_LENS=20 "$X" "$R" "$O" deck "$P-2kt" 0 $S > /dev/null
  FX_EYE=-1400,-700,2500 FX_AT=-1400,-650,0 FX_LENS=60 "$X" "$R" "$O" deck "$P-top" 0 $S > /dev/null
  FX_NIGHT=1 FX_EYE=-1255,-1246,20 FX_AT=-1455,-516,0 FX_LENS=60 "$X" "$R" "$O" deck "$P-n760" 0 $S > /dev/null
  FX_NIGHT=1 FX_EYE=-927,-2444,30 FX_AT=-1455,-516,0 FX_LENS=60 "$X" "$R" "$O" deck "$P-n2k" 0 $S > /dev/null
  FX_DIM=0.35 FX_EYE=-1255,-1246,20 FX_AT=-1455,-516,0 FX_LENS=60 "$X" "$R" "$O" deck "$P-d760" 0 $S > /dev/null
done
for F in "$O"/*.ppm.f32; do [ -e "$F" ] || continue; python3 "$G" show "$F" 800 450 "${F%.ppm.f32}.png" > /dev/null; rm -f -- "$F" "${F%.f32}"; done
rm -f -- "$O"/*.ppm
python3 "$HERE/lane_lift.py" "$O"
