#!/bin/bash
# The goods signs for the bargain card (release 2): Content/Goods/goods-<sign>.png, 48 x 48, white on clear, from the
# art director's line signs (campaign-map/signs/goods-*.svg: the kit's stroke 1.6 on a 24 box, no fills). The game
# tints them brass-700 on the card, as it does the order signs. Drawn by headless Chrome at twice the box.
# Usage: Tools/bargains/goods_signs.sh <signs folder> [chrome]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "$HERE/../.." && pwd)/Content/Goods"
SIGNS="$(cd "$1" && pwd)"
CHROME="${2:-google-chrome}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"
for svg in "$SIGNS"/goods-*.svg; do
  name="$(basename "$svg" .svg)"
  sed -e 's/<metadata>.*<\/metadata>//' -e 's/#b08d57/#ffffff/g' -e 's/width="24" height="24"/width="48" height="48"/' "$svg" > "$WORK/$name.svg"
  printf '<!doctype html><html><head><style>html,body{margin:0;padding:0;background:transparent}img{display:block;width:48px;height:48px}</style></head><body><img src="%s.svg"></body></html>' "$name" > "$WORK/$name.html"
  "$CHROME" --headless=new --no-sandbox --disable-gpu --hide-scrollbars --force-device-scale-factor=1 \
    --default-background-color=00000000 --window-size=48,48 --screenshot="$OUT/$name.png" "file://$WORK/$name.html" > /dev/null 2>&1
  echo "$name.png"
done
