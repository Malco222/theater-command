#!/usr/bin/env python3
"""The downed pilots' picture (Effects, release 2, -TheaterLandChutes): land_air_test's two pilots as LandAir brings
them down, side-on (height against the ground covered from where each left his plane): falling free (red), his canopy
blossoming (amber), full (green), each dot a quarter of a battle second; then from above on the ground: where he
landed, his canopy's heap downwind, his run from the enemy (blue) and where he went to ground.
Usage: chutes.py <air_paths.txt> <out.png>"""
import math
import sys
from collections import defaultdict

from PIL import Image, ImageDraw

SRC, DST = sys.argv[1], sys.argv[2]
chutes = defaultdict(list)
for line in open(SRC):
    w = line.split()
    if w and w[0] == 'c':
        chutes[int(w[1])].append((float(w[2]), float(w[3]), float(w[4]), float(w[5]), float(w[6]), float(w[7]), w[8] == '1', w[9], float(w[10]), float(w[11]), float(w[12])))
W, H = 1400, 420 * max(1, len(chutes))
im = Image.new('RGB', (W, H), (246, 244, 238))
d = ImageDraw.Draw(im)
for row, key in enumerate(sorted(chutes)):
    pts = chutes[key]
    oy = row * 420
    x0, y0 = pts[0][1], pts[0][2]
    air = [p for p in pts if not p[6]]
    ground = [p for p in pts if p[6]]
    far = max(1.0, max(math.hypot(p[1] - x0, p[2] - y0) for p in air))
    top = max(p[3] for p in air)
    sx = min(800.0 / far, 360.0 / max(1.0, top))
    d.text((20, oy + 8), f"pilot of plane {key}: out at {top:.0f} m at most, landed {math.hypot(air[-1][1]-x0, air[-1][2]-y0):.0f} m from where he left her", fill=(20, 20, 20))
    d.line((40, oy + 400, 880, oy + 400), fill=(90, 110, 60), width=2)
    for p in air:
        u = 40 + math.hypot(p[1] - x0, p[2] - y0) * sx
        v = oy + 400 - p[3] * sx
        c = (200, 40, 30) if p[4] <= 0.0 else (220, 150, 20) if p[4] < 1.0 else (40, 140, 60)
        d.ellipse((u - 2.5, v - 2.5, u + 2.5, v + 2.5), fill=c)
        if p[4] >= 1.0 and abs(p[0] * 4 - round(p[0] * 4)) < 1e-6 and int(round(p[0] * 4)) % 8 == 0:
            # (his canopy over him, swung by his swing)
            a = math.radians(p[5])
            cx, cy = u + math.sin(a) * 4.1 * sx, v - math.cos(a) * 4.1 * sx
            d.chord((cx - 3.5 * sx, cy - 2.0 * sx, cx + 3.5 * sx, cy + 2.0 * sx), 180, 360, fill=(235, 232, 220), outline=(120, 120, 110))
            d.line((u, v, cx - 3.2 * sx, cy), fill=(120, 120, 110))
            d.line((u, v, cx + 3.2 * sx, cy), fill=(120, 120, 110))
    # On the ground, from above (north up), 4 px a metre about where he landed.
    gx, gy = 1130, oy + 210
    if ground:
        lx, ly = ground[0][1], ground[0][2]
        d.rectangle((900, oy + 20, 1380, oy + 400), outline=(160, 160, 150))
        d.text((910, oy + 26), "down, from above (north up, 4 px a metre)", fill=(20, 20, 20))
        hx, hy = ground[0][8], ground[0][9]
        d.ellipse((gx + (hx - lx) * 4 - 14, gy - (hy - ly) * 4 - 14, gx + (hx - lx) * 4 + 14, gy - (hy - ly) * 4 + 14), fill=(235, 232, 220), outline=(120, 120, 110))
        last = None
        for p in ground:
            u, v = gx + (p[1] - lx) * 4, gy - (p[2] - ly) * 4
            c = (40, 90, 200) if p[7] == 'run' else (60, 60, 60)
            d.ellipse((u - 2, v - 2, u + 2, v + 2), fill=c)
            if p[7] != last:
                d.text((u + 6, v - 6), p[7], fill=(20, 20, 20))
                last = p[7]
im.save(DST)
print('wrote', DST)
