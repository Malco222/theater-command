#!/bin/bash
# Men out of their machines without their packs, for the art director (Effects, R2): a downed pilot and a tank's crewman
# bailing out, Russian and British (no tankers' file), as the crowd draws them (pack_picture.cpp). pack_off.png.
# Usage: Tools/land_show/pack_picture.sh [soldiers folder with pack_tris] [out folder]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/Source/TheaterCommand"
OUT="${2:-${TMPDIR:-/tmp}/pack_picture}"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
$CXX -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -I"$SRC" -I"$HERE" -pthread "$HERE/pack_picture.cpp" "$SRC/Battle/LandMen.cpp" -o "$OUT/pack_picture"
"$OUT/pack_picture" "${1:-$ROOT/Content/Models/Soldiers}" "$ROOT/Content/Data/LandBattles/armies.json" "$OUT"
python3 - "$OUT" <<'PY'
import os, sys
from PIL import Image, ImageDraw, ImageFont
out = sys.argv[1]
pic = Image.open(os.path.join(out, 'pack_off.ppm')).convert('RGB')
notes = open(os.path.join(out, 'pack_off.txt')).read().splitlines()
rows = [n.split(': ', 1)[1] for n in notes if n.startswith('row ')]
cols = ['near, front', 'near, behind', 'mid, behind (running)', 'near, behind: as before (pack on)']
path = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
f = ImageFont.truetype(path, 13) if os.path.exists(path) else ImageFont.load_default()
top, side = 24, 0
cw, ch = pic.width // len(cols), pic.height // max(1, len(rows))
sheet = Image.new('RGB', (pic.width, pic.height + top + 20 * len(rows) + 10), (236, 236, 236))
sheet.paste(pic, (0, top))
d = ImageDraw.Draw(sheet)
for k, c in enumerate(cols):
    d.text((k * cw + 8, 5), c, fill=(40, 40, 40), font=f)
for r, t in enumerate(rows):
    d.text((8, top + r * ch + 6), 'row %d' % (r + 1), fill=(30, 30, 30), font=f)
    d.text((8, top + pic.height + 4 + 20 * r), 'row %d: %s' % (r + 1, t), fill=(40, 40, 40), font=f)
p = os.path.join(out, 'pack_off.png')
sheet.save(p, optimize=True)
print('wrote %s (%dx%d)' % (p, sheet.width, sheet.height))
PY
