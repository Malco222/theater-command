"""Lays out land_hits_picture's panels as PNGs with their labels (Pillow).
Usage: python3 land_hits_pictures.py <panels folder> <out folder>
  hits_strip_<case>.png: today above, the jolt alone below (variety off: the same fall), at 0, 0.02, 0.033, 0.05, 0.1,
  0.2, 0.4 and 1.0 s of life after the round; the new row carries today's outline in white, so the jolt reads against it.
  hits_wounded.png: three men wounded and one killed beside them, every part on, at 0.5, 2, 5 and 20 s, close and at a
  battle camera's distance (about 50 m, enlarged three times, each pixel kept).
  hits_dense.png: a dense moment of the battle from a battle camera's slant while the last men there still fall, today
  on the left and every part on at the right, each fallen man's clip written by him; a clip a body within 15 m showed
  in the 8 s before at the same rate and start in red.
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

TIMES = ['0 s', '0.02 s', '0.033 s', '0.05 s', '0.1 s', '0.2 s', '0.4 s', '1.0 s']
NT = len(TIMES)
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
    off = [Image.open(os.path.join(src, '%s_off_%d.ppm' % (case, k))).convert('RGB') for k in range(NT)]
    on = [Image.open(os.path.join(src, '%s_on_%d.ppm' % (case, k))).convert('RGB') for k in range(NT)]
    w, h = off[0].size
    sheet = Image.new('RGB', (110 + NT * w, 40 + 2 * h + 20), (40, 42, 44))
    d = ImageDraw.Draw(sheet)
    d.text((10, 10), 'German rifleman, %s: today (top), the jolt alone (below; variety off, so the same fall; today outlined in white). Seen square to the shot; it comes from the right. Seconds of life after the round; the jolt is over by 0.33 s.' % title, fill=(235, 235, 235), font=font)
    for k in range(NT):
        sheet.paste(off[k], (110 + k * w, 40))
        sheet.paste(outline(off[k], on[k]), (110 + k * w, 40 + h))
        d.text((110 + k * w + 8, 40 + 2 * h + 2), TIMES[k], fill=(235, 235, 235), font=font)
    d.text((10, 40 + h // 2), 'today', fill=(235, 235, 235), font=font)
    d.text((10, 40 + h + h // 2), 'jolt alone', fill=(235, 235, 235), font=font)
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
            twin = any(o is not r and o['clip'] == r['clip'] and o['rate'] == r['rate'] and o['skip'] == r['skip'] and 0 <= r['t'] - o['t'] <= 8
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
        d.text((10 + i * (w + 10), 12), ('today' if i == 0 else 'bullet hits on') + ': %d fallen or falling here, %d taking a clip a body within 15 m took in the 8 s before (red)' % (n, same),
               fill=(235, 235, 235), font=font)
    sheet.save(os.path.join(out, 'hits_dense.png'))
if os.path.exists(os.path.join(src, 'wounded_0.ppm')):
    AFTER = ['0.5 s', '2 s', '5 s', '20 s']
    labels = {}
    for line in open(os.path.join(src, 'wounded.txt')):
        k, sx, sy, text = line.rstrip('\n').split(' ', 3)
        labels.setdefault(int(k), []).append((float(sx), float(sy), text))
    near = [Image.open(os.path.join(src, 'wounded_%d.ppm' % k)).convert('RGB') for k in range(4)]
    far = [Image.open(os.path.join(src, 'wounded_far_%d.ppm' % k)).convert('RGB') for k in range(4)]
    far = [f.resize((f.size[0] * 3, f.size[1] * 3), Image.NEAREST) for f in far]
    nw, nh = near[0].size
    fw, fh = far[0].size
    sheet = Image.new('RGB', (90 + nw + 20 + fw + 10, 60 + 4 * (nh + 10)), (40, 42, 44))
    d = ImageDraw.Draw(sheet)
    d.text((10, 10), 'Three men wounded (wd, not carried, no back: they lie for good) and one killed (right), every part of bullet hits on. Seconds of life after the round.', fill=(235, 235, 235), font=font)
    d.text((10, 30), 'Left: close. Right: the same at a battle camera\'s distance (about 50 m: 18 pixels a metre), enlarged three times, each pixel kept.', fill=(235, 235, 235), font=font)
    for k in range(4):
        y = 60 + k * (nh + 10)
        img = near[k].copy()
        dd = ImageDraw.Draw(img)
        for sx, sy, text in labels.get(k, []):
            tw = dd.textlength(text, font=small)
            dd.rectangle((sx - tw / 2 - 3, sy - 2, sx + tw / 2 + 3, sy + 13), fill=(40, 42, 44))
            dd.text((sx - tw / 2, sy), text, fill=(255, 255, 255), font=small)
        sheet.paste(img, (90, y))
        sheet.paste(far[k], (90 + nw + 20, y))
        d.text((10, y + nh // 2), AFTER[k], fill=(235, 235, 235), font=font)
    sheet.save(os.path.join(out, 'hits_wounded.png'))
print('pictures in', out)
