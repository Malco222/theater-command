#!/bin/bash
# The men's worn look before and after, for the art director (release 2, tc.Look.Worn): the American rifleman as the
# crowd draws him OFF, ON on Normandy's own soil, ON on the desert's sand, ON in rain, fresh, and the roughness ON writes
# (worn_picture.cpp: Normandy's soil read from its field without the engine). worn.png.
# Usage: Tools/land_show/worn_picture.sh [out folder]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${1:-${TMPDIR:-/tmp}/worn_picture}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
B="$SRC/Battle"
# (the field's cores, no engine, as Tools/land_play builds them: Normandy's ground laid to read its soil)
FIELD="$B/LandJson.cpp $B/LandFeed.cpp $B/LandPlay.cpp $B/LandCrater.cpp $B/LandField.cpp $B/LandHouse.cpp $B/LandSteppe.cpp $B/LandScape.cpp $B/LandDesert.cpp $B/LandMed.cpp $B/LandTropic.cpp $B/LandFarm.cpp $B/LandSnow.cpp $B/LandDry.cpp $B/LandAsia.cpp $B/LandAmerica.cpp $B/LandPlain.cpp $B/LandSight.cpp"
$CXX -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -I"$SRC" -I"$HERE" -pthread "$HERE/worn_picture.cpp" "$B/LandMen.cpp" $FIELD -o "$OUT/worn_picture"
"$OUT/worn_picture" "$ROOT/Content/Models/Soldiers" "$ROOT/Content/Data/LandBattles/armies.json" "$ROOT/Content/Data/LandBattles/normandy-1944.json" "$OUT"
python3 - "$OUT" <<'PY'
import os, sys
from PIL import Image, ImageDraw, ImageFont
out = sys.argv[1]
pic = Image.open(os.path.join(out, 'worn.ppm')).convert('RGB')
notes = open(os.path.join(out, 'worn.txt')).read().splitlines()
cols = [n.split(': ', 1)[1].split(';')[0] for n in notes if n.startswith('column ')]
rows = [n.split(': ', 1)[1] for n in notes if n.startswith('row ')]
soil = [n for n in notes if n.startswith('soil: ')]
path = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
f = ImageFont.truetype(path, 13) if os.path.exists(path) else ImageFont.load_default()
top = 24
cw, ch = pic.width // max(1, len(cols)), pic.height // max(1, len(rows))
lines = soil + ['The ON columns are a man six weeks or more in the line (W 1); fresh is a replacement (W 0.1). The painter is a stand-in and draws no roughness: the last column paints it as grey, 0.3 to 1 over dark to pale (cloth 0.9 pale; skin 0.55, leather 0.45, steel 0.42 darker; dust duller).']
sheet = Image.new('RGB', (pic.width, pic.height + top + 20 * len(lines) + 10), (236, 236, 236))
sheet.paste(pic, (0, top))
d = ImageDraw.Draw(sheet)
for k, c in enumerate(cols):
    d.text((k * cw + 8, 5), c, fill=(40, 40, 40), font=f)
for r, t in enumerate(rows):
    d.text((8, top + r * ch + 6), t, fill=(30, 30, 30), font=f)
for i, t in enumerate(lines):
    d.text((8, top + pic.height + 4 + 20 * i), t, fill=(40, 40, 40), font=f)
p = os.path.join(out, 'worn.png')
sheet.save(p, optimize=True)
print('wrote %s (%dx%d)' % (p, sheet.width, sheet.height))
PY
