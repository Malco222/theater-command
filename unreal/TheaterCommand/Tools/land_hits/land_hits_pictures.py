"""Lays out land_hits_picture's panels as PNGs with their labels (Pillow).
Usage: python3 land_hits_pictures.py <panels folder> <out folder>
  hits_strip_<case>.png: today above, the jolt below, at 0, 0.05, 0.1, 0.2, 0.4 and 1.0 s of life after the round;
  the new row carries today's outline in white, so the jolt reads against it.
  hits_dense.png: a dense moment of the battle, today on the left and every part on at the right, each fallen man's
  clip written by him; a clip a body within 15 m showed in the 5 s before at the same rate and start in red.
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
font = ImageFont.load_default()
try:
    font = ImageFont.truetype('DejaVuSans.ttf', 15)
    small = ImageFont.truetype('DejaVuSans.ttf', 11)
except OSError:
    small = font

TIMES = ['0 s', '0.05 s', '0.1 s', '0.2 s', '0.4 s', '1.0 s']
CASES = {'front': 'shot from the front', 'left': 'shot from his left', 'running': 'running, shot from the front'}


def outline(img_off, img_on):
    """Today's man (where today's panel differs from the empty ground) drawn as a white edge over the new panel."""
    a = img_off.load()
    w, h = img_off.size
    base = img_on.copy()
    b = base.load()
    # (the man: pixels that are not ground, read as not grey-green checker: his paints differ from the ground's)
    def man(px):
        r, g, bl = px
        return not (abs(r - 196) < 3 and abs(g - 206) < 3 and abs(bl - 214) < 3) and not (g > r and g > bl and bl < 0.8 * g)
    mask = [[man(a[x, y]) for x in range(w)] for y in range(h)]
    for y in range(1, h - 1):
        for x in range(1, w - 1):
            if mask[y][x] and not (mask[y - 1][x] and mask[y + 1][x] and mask[y][x - 1] and mask[y][x + 1]):
                b[x, y] = (255, 255, 255)
    return base


for case, title in CASES.items():
    off = [Image.open(os.path.join(src, '%s_off_%d.ppm' % (case, k))).convert('RGB') for k in range(6)]
    on = [Image.open(os.path.join(src, '%s_on_%d.ppm' % (case, k))).convert('RGB') for k in range(6)]
    w, h = off[0].size
    sheet = Image.new('RGB', (110 + 6 * w, 40 + 2 * h + 20), (40, 42, 44))
    d = ImageDraw.Draw(sheet)
    d.text((10, 10), 'German rifleman, %s: today (top), the jolt (below, today outlined in white). Seen square to the shot; it comes from the right.' % title, fill=(235, 235, 235), font=font)
    for k in range(6):
        sheet.paste(off[k], (110 + k * w, 40))
        sheet.paste(outline(off[k], on[k]), (110 + k * w, 40 + h))
        d.text((110 + k * w + 8, 40 + 2 * h + 2), TIMES[k], fill=(235, 235, 235), font=font)
    d.text((10, 40 + h // 2), 'today', fill=(235, 235, 235), font=font)
    d.text((10, 40 + h + h // 2), 'jolt', fill=(235, 235, 235), font=font)
    sheet.save(os.path.join(out, 'hits_strip_%s.png' % case))

if os.path.exists(os.path.join(src, 'dense_off.ppm')):
    panels = []
    for side in ('off', 'on'):
        img = Image.open(os.path.join(src, 'dense_%s.ppm' % side)).convert('RGB')
        rows = []
        for line in open(os.path.join(src, 'dense_%s.txt' % side)):
            p = line.split()
            rows.append(dict(clip=p[0], rate=float(p[1]), skip=float(p[2]), sx=float(p[3]), sy=float(p[4]), t=float(p[5]), x=float(p[6]), y=float(p[7])))
        d = ImageDraw.Draw(img)
        same = 0
        for r in rows:
            twin = any(o is not r and o['clip'] == r['clip'] and o['rate'] == r['rate'] and o['skip'] == r['skip'] and 0 <= r['t'] - o['t'] <= 5
                       and math.hypot(o['x'] - r['x'], o['y'] - r['y']) <= 15 for o in rows)
            same += twin
            label = r['clip'].replace('die_', '') + ('' if r['rate'] == 1 and r['skip'] == 0 else ' x%.2f' % r['rate'])
            d.text((r['sx'] + 6, r['sy'] - 6), label, fill=(255, 70, 60) if twin else (255, 255, 255), font=small)
        panels.append((img, same, len(rows)))
    w, h = panels[0][0].size
    sheet = Image.new('RGB', (2 * w + 30, h + 50), (40, 42, 44))
    d = ImageDraw.Draw(sheet)
    for i, (img, same, n) in enumerate(panels):
        sheet.paste(img, (10 + i * (w + 10), 40))
        d.text((10 + i * (w + 10), 12), ('today' if i == 0 else 'bullet hits on') + ': %d fallen here, %d taking a clip a body within 15 m took in the 5 s before (red)' % (n, same),
               fill=(235, 235, 235), font=font)
    sheet.save(os.path.join(out, 'hits_dense.png'))
print('pictures in', out)
