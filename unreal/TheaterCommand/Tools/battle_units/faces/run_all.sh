#!/bin/bash
# face_mesh then face_detail on every soldier file, from the originals (before any face work) into a stage folder
ORIG=${1:?originals folder (the files before any face work)}
STAGE=${2:?stage folder}
HERE=$(cd "$(dirname "$0")" && pwd); PY=${PY:-/home/user/bpyenv/bin/python}; mkdir -p $STAGE $STAGE/logs
cd "$HERE"
for f in $(ls $ORIG | grep -E '^soldier_.*\.json$'); do
  rm -f $STAGE$f.mesh.json
  $PY face_mesh.py $ORIG$f $STAGE$f.mesh.json > $STAGE/logs/$f.log 2>&1 || { echo "$f MESH FAIL"; continue; }
  $PY face_detail.py $STAGE$f.mesh.json $STAGE$f >> $STAGE/logs/$f.log 2>&1 || { echo "$f DETAIL FAIL"; continue; }
  rm -f $STAGE$f.mesh.json
  echo "$f $(grep -o '"triangles": \[[0-9]*, [0-9]*\]' $STAGE/logs/$f.log) $(grep -o '"folds": {[^}]*}' $STAGE/logs/$f.log)"
done
