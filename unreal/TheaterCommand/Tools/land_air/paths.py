#!/usr/bin/env python3
"""The ground-attack fighters' picture (Effects, release 2, -TheaterLandAirSupport): land_air_test's three strikes as
the planner flies them, each side-on down its run (height against the way along it) and from above (the field, its
blocks, the strip, the planes' tracks, the ground's guns and their streams, where each lost plane was hit and struck).
Usage: paths.py <air_paths.txt> <out.png>"""
import math
import sys
from collections import defaultdict

from PIL import Image, ImageDraw, ImageFont

SRC, DST = sys.argv[1], sys.argv[2]
planes = defaultdict(list)   # key -> [(t, x, y, z, heading, pitch, bank, firing, how, wreck, ax, ay, model)]
guns = defaultdict(list)     # leg -> [(t, fx, fy, tx, ty, tz)]
hits, crashes, runs, units = {}, {}, [], []
for line in open(SRC):
    w = line.split()
    if not w:
        continue
    if w[0] == 'p':
        k = int(w[1])
        planes[k].append((float(w[2]), float(w[3]), float(w[4]), float(w[5]), float(w[6]), float(w[7]), float(w[8]), w[9] == '1', int(w[10]), w[11] == '1', float(w[12]), float(w[13]), w[14]))
    elif w[0] == 'g':
        guns[int(w[1]) // 8].append((float(w[2]), float(w[3]), float(w[4]), float(w[5]), float(w[6]), float(w[7])))
    elif w[0] == 'hit':
        hits[int(w[1])] = (float(w[2]), float(w[3]), float(w[4]), float(w[5]), int(w[6]))
    elif w[0] == 'crash':
        crashes[int(w[1])] = (float(w[2]), float(w[3]), float(w[4]), float(w[5]), int(w[6]))
    elif w[0] == 'run':
        runs.append((int(w[1]), float(w[2]), float(w[3]), float(w[4]), float(w[5]), w[6], int(w[7]), w[8]))
    elif w[0] == 'u':
        units.append((int(w[1]), int(w[2]), float(w[3]), float(w[4]), float(w[5])))


def font(size):
    for name in ('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', '/usr/share/fonts/truetype/freefont/FreeSans.ttf'):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


F, FS, FB = font(17), font(13), font(20)
SLOT = [(40, 90, 200), (20, 150, 120), (150, 90, 190), (200, 130, 20)]
BG, GROUND, INK, MUTED = (246, 244, 238), (150, 128, 96), (40, 40, 40), (120, 120, 120)
FIRE, TRACER, LOST = (255, 150, 0), (235, 120, 40), (205, 30, 30)

ROWS = [
    ([0], 'USA, July 1944: four Mustangs strafe a German line. The last is hit climbing away and goes down on fire.'),
    ([1], 'Germany, 1942: four Stukas dive on an American line. The last is hit climbing away and spins in.'),
    ([2, 3], 'USSR, 1943: two legs of two Il-2s, low and shallow. One is blown apart in its pass.'),
]
W, RH = 1900, 520
img = Image.new('RGB', (W, RH * len(ROWS) + 100), BG)
d = ImageDraw.Draw(img)
d.text((24, 16), 'Ground-attack fighters as the planner flies them (land_air_test). Left: side-on down the run. Right: from above.', fill=INK, font=FB)


def side_panel(OX, OY, pw, ph, legs):
    sub = Image.new('RGB', (pw + 1, ph + 1), BG)   # its own canvas, so that nothing drawn runs out of the panel
    d = ImageDraw.Draw(sub)
    ox, oy = 0, 0
    leg = legs[0]
    _, x0, y0, x1, y1, nat, year, side = runs[leg]
    L = math.hypot(x1 - x0, y1 - y0)
    fx, fy = (x1 - x0) / L, (y1 - y0) / L
    umin, umax, hmax = -2400.0, 1700.0, 900.0
    sx = lambda u: ox + (u - umin) / (umax - umin) * pw
    sy = lambda h: oy + ph - 46 - h / hmax * (ph - 66)
    d.rectangle((ox, oy, ox + pw, oy + ph), outline=(210, 205, 195))
    d.line((ox, sy(0), ox + pw, sy(0)), fill=GROUND, width=3)
    d.rectangle((sx(0), sy(0) - 3, sx(L), sy(0) + 3), fill=(90, 70, 50))
    d.text((sx(0), sy(0) + 6), 'the strip', fill=MUTED, font=FS)
    for h in (100, 300, 500, 700):
        d.line((ox, sy(h), ox + 8, sy(h)), fill=MUTED)
        d.text((ox + 10, sy(h) - 8), f'{h} m', fill=MUTED, font=FS)
    for u in (-2000, -1000, 0, 1000):
        d.text((sx(u) - 12, sy(0) + 22), f'{u} m', fill=MUTED, font=FS)
    for k in range(leg * 8, leg * 8 + 8):
        pts = planes.get(k)
        if not pts:
            continue
        col = SLOT[k % 8 % 4]
        prev = None
        for (t, x, y, z, hd, pi, bk, firing, how, wreck, ax, ay, model) in pts:
            if wreck:
                continue
            u = (x - x0) * fx + (y - y0) * fy
            c = LOST if how else col
            if prev:
                d.line((sx(prev[0]), sy(prev[1]), sx(u), sy(z)), fill=FIRE if firing else c, width=4 if firing else 2)
            prev = (u, z)
            if firing and int(t * 60) % 20 == 0:
                au = (ax - x0) * fx + (ay - y0) * fy
                d.line((sx(u), sy(z), sx(au), sy(0)), fill=(250, 200, 120), width=1)
        if k in hits:
            t, x, y, z, how = hits[k]
            u = (x - x0) * fx + (y - y0) * fy
            d.line((sx(u) - 9, sy(z) - 9, sx(u) + 9, sy(z) + 9), fill=LOST, width=4)
            d.line((sx(u) - 9, sy(z) + 9, sx(u) + 9, sy(z) - 9), fill=LOST, width=4)
        if k in crashes:
            t, x, y, z, how = crashes[k]
            u = (x - x0) * fx + (y - y0) * fy
            d.ellipse((sx(u) - 9, sy(0) - 9, sx(u) + 9, sy(0) + 9), fill=FIRE, outline=LOST, width=3)
    model = next((p[0][12] for k, p in planes.items() if k // 8 == leg and p), '?')
    d.text((ox + 14, oy + 10), f'{model}: in high down the run, the dive, guns raking the strip (orange), the pull-out and the break', fill=INK, font=FS)
    img.paste(sub, (OX, OY))


def top_panel(OX, OY, pw, ph, legs):
    sub = Image.new('RGB', (pw + 1, ph + 1), BG)
    d = ImageDraw.Draw(sub)
    ox, oy = 0, 0
    xs = [runs[l][1] for l in legs] + [runs[l][3] for l in legs]
    ys = [runs[l][2] for l in legs] + [runs[l][4] for l in legs]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    span = 2400.0
    k_ = min(pw, ph) / span
    mx = lambda x: ox + pw / 2 + (x - cx) * k_
    my = lambda y: oy + ph / 2 - (y - cy) * k_
    d.rectangle((ox, oy, ox + pw, oy + ph), outline=(210, 205, 195))
    for (uid, side, x, y, st) in units:
        if st <= 0.05:
            continue
        r = 16
        col = (70, 110, 190) if side == 0 else (170, 70, 60)
        if ox < mx(x) < ox + pw and oy < my(y) < oy + ph:
            d.rectangle((mx(x) - r, my(y) - r * 0.5, mx(x) + r, my(y) + r * 0.5), outline=col, width=2)
    for l in legs:
        _, x0, y0, x1, y1, nat, year, side = runs[l]
        L = math.hypot(x1 - x0, y1 - y0)
        nx, ny = (y1 - y0) / L * 30, -(x1 - x0) / L * 30
        poly = [(mx(x0 + nx), my(y0 + ny)), (mx(x1 + nx), my(y1 + ny)), (mx(x1 - nx), my(y1 - ny)), (mx(x0 - nx), my(y0 - ny))]
        d.polygon(poly, fill=(225, 210, 185), outline=(150, 120, 90))
        for (t, fx_, fy_, tx, ty, tz) in guns.get(l, [])[::45]:
            d.line((mx(fx_), my(fy_), mx(tx), my(ty)), fill=TRACER, width=1)
        for (t, fx_, fy_, tx, ty, tz) in guns.get(l, []):
            d.rectangle((mx(fx_) - 2, my(fy_) - 2, mx(fx_) + 2, my(fy_) + 2), fill=(120, 60, 30))
        for k in range(l * 8, l * 8 + 8):
            pts = planes.get(k)
            if not pts:
                continue
            col = SLOT[k % 8 % 4]
            prev = None
            for (t, x, y, z, hd, pi, bk, firing, how, wreck, ax, ay, model) in pts:
                if wreck:
                    continue
                c = LOST if how else (FIRE if firing else col)
                if prev:
                    d.line((mx(prev[0]), my(prev[1]), mx(x), my(y)), fill=c, width=3 if (firing or how) else 1)
                prev = (x, y)
            if k in hits:
                t, x, y, z, how = hits[k]
                d.line((mx(x) - 8, my(y) - 8, mx(x) + 8, my(y) + 8), fill=LOST, width=3)
                d.line((mx(x) - 8, my(y) + 8, mx(x) + 8, my(y) - 8), fill=LOST, width=3)
            if k in crashes:
                t, x, y, z, how = crashes[k]
                d.ellipse((mx(x) - 8, my(y) - 8, mx(x) + 8, my(y) + 8), fill=FIRE, outline=LOST, width=3)
    d.line((ox + 14, oy + ph - 16, ox + 14 + 500 * k_, oy + ph - 16), fill=INK, width=2)
    d.text((ox + 14, oy + ph - 36), '500 m', fill=INK, font=FS)
    d.text((ox + 14, oy + 10), 'blue: the attacker\'s blocks, red: the defender\'s; thin orange: the ground\'s guns\' streams', fill=INK, font=FS)
    img.paste(sub, (OX, OY))


for i, (legs, words) in enumerate(ROWS):
    y = 60 + i * RH
    d.text((24, y + 4), words, fill=INK, font=F)
    side_panel(24, y + 32, 1180, RH - 46, legs)
    top_panel(1220, y + 32, 660, RH - 46, legs)
key_y = RH * len(ROWS) + 66
d.text((24, key_y), 'Orange line: guns firing (fine lines to where they rake). Red X: hit. Red path: falling. Orange disc: where it struck and burns.', fill=INK, font=FS)
img.save(DST)
print('wrote', DST)
