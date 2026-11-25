#!/usr/bin/env python3
"""
A top-down picture of a raid on troops as the scene shows it (Brian, 24 November 2026: "if a bomber group is bombing
troops instead of a factory or town and they get intercepted than i want that playable"), from the marks test's dump:

  Tools/air_show/run.sh <recording with marks>            (or air_marks_test <recording> <out-dir>)
  python3 Tools/air_show/marks_picture.py $TMPDIR/air_show/<key>-marks.csv out.png [view]

  view: whole (the column and every bomb, 2 m a pixel), hits (the hits' stretch of the road, 0.5 m a pixel), strafe
  (the strafing pass's lane, 0.5 m a pixel).

The road in tan; each piece its footprint, faced along the road, in its kind's colour (tank olive, lorry brown, gun grey,
light flak purple, men green), outlined by how it ended (black: as it stood; orange: damaged; red: burning; dark red
with a cross: wrecked). Each bomb where it came down: a white dot with a faint ring of its blast; a bomb that carried a
counted hit, a yellow dot ringed in yellow, a line to its piece. The strafing pass: each strafer's track (thin blue,
red where its guns fired) and each round's strike (a dark dot). The column's light flak: a few of its streams, a thin
purple line toward where they were laid.
"""

import csv
import math
import sys

from PIL import Image, ImageDraw

KIND = {'tank': (96, 110, 52), 'lorry': (120, 86, 50), 'gun': (110, 110, 110), 'aa': (128, 70, 150), 'men': (60, 140, 60)}
EDGE = {'ok': (20, 20, 20), 'hit': (240, 140, 20), 'burn': (230, 30, 20), 'out': (110, 0, 0)}


def main():
    src, out = sys.argv[1], sys.argv[2]
    view = sys.argv[3] if len(sys.argv) > 3 else 'whole'
    rows = list(csv.DictReader(open(src)))
    pieces = {int(r['a']): r for r in rows if r['kind'] == 'piece'}
    states = {int(r['a']): r for r in rows if r['kind'] == 'state'}
    road = [(float(r['x']), float(r['y'])) for r in rows if r['kind'] == 'road']
    bombs = [r for r in rows if r['kind'] == 'bomb']
    hits = [r for r in rows if r['kind'] == 'markhit']
    rounds = [r for r in rows if r['kind'] == 'round']
    planes = [r for r in rows if r['kind'] == 'plane']
    flak = [r for r in rows if r['kind'] == 'flak']
    px = [float(r['x']) for r in pieces.values()]
    py = [float(r['y']) for r in pieces.values()]
    mx, my = sum(px) / len(px), sum(py) / len(py)
    if view == 'hits':
        bx = [float(r['x']) for r in bombs if r['c'] == '1' and float(r['air']) > 0]
        by = [float(r['y']) for r in bombs if r['c'] == '1' and float(r['air']) > 0]
        cx, cy = (sum(bx) / len(bx), sum(by) / len(by)) if bx else (mx, my)
        x0, y0, x1, y1, mpp = cx - 500, cy - 350, cx + 500, cy + 350, 0.5
    elif view == 'strafe':
        sh = [r for r in hits if r['b'] == 'strafe']
        sx = [float(r['x']) for r in sh] or [mx]
        sy = [float(r['y']) for r in sh] or [my]
        cx, cy = sum(sx) / len(sx), sum(sy) / len(sy)
        x0, y0, x1, y1, mpp = cx - 600, cy - 400, cx + 600, cy + 400, 0.5
    else:
        allx = px + [float(r['x']) for r in bombs]
        ally = py + [float(r['y']) for r in bombs]
        x0, x1 = min(allx) - 300, max(allx) + 300
        y0, y1 = min(ally) - 300, max(ally) + 300
        mpp = max(1.0, max(x1 - x0, y1 - y0) / 2000.0)
    W, H = int((x1 - x0) / mpp), int((y1 - y0) / mpp)
    img = Image.new('RGB', (W, H), (176, 186, 142))   # (fields)
    d = ImageDraw.Draw(img)

    def to(x, y):
        return ((x - x0) / mpp, (y1 - y) / mpp)

    def ring(x, y, r, colour, width=1):
        u, v = to(x, y)
        rr = r / mpp
        d.ellipse([u - rr, v - rr, u + rr, v + rr], outline=colour, width=width)

    def dot(x, y, r, colour):
        u, v = to(x, y)
        rr = max(1.0, r / mpp)
        d.ellipse([u - rr, v - rr, u + rr, v + rr], fill=colour)

    # The road.
    if len(road) >= 2:
        d.line([to(x, y) for x, y in road], fill=(196, 170, 120), width=max(2, int(7 / mpp)))
    # The column's flak streams (a few).
    for r in flak[::6]:
        d.line([to(float(r['x']), float(r['y'])), to(float(r['a']), float(r['b']))], fill=(150, 90, 170), width=1)
    # The bombs: blast rings first, then the dots.
    for r in bombs:
        x, y, kg = float(r['x']), float(r['y']), float(r['b'])
        blast = max(15.0, 30.0 * (max(50.0, kg) / 250.0) ** (1.0 / 3.0))
        ring(x, y, blast, (150, 150, 140) if r['c'] == '0' else (230, 200, 0), 1 if r['c'] == '0' else 2)
    # The pieces.
    for pid, r in pieces.items():
        x, y, face = float(r['x']), float(r['y']), float(r['c'])
        L, Wd = (float(v) for v in r['d'].split('x'))
        fx, fy = math.sin(face), math.cos(face)
        corners = []
        for a, b in ((0.5, 0.5), (0.5, -0.5), (-0.5, -0.5), (-0.5, 0.5)):
            u, v = a * L, b * Wd
            corners.append(to(x + fx * u + fy * v, y + fy * u - fx * v))
        st = states.get(pid, {}).get('b', 'ok')
        d.polygon(corners, fill=KIND.get(r['b'], (90, 90, 90)), outline=EDGE.get(st, (20, 20, 20)))
        if mpp <= 1.0 and st != 'ok':
            d.polygon(corners, outline=EDGE[st], width=2)
        if st == 'out':
            d.line([corners[0], corners[2]], fill=EDGE['out'], width=2)
            d.line([corners[1], corners[3]], fill=EDGE['out'], width=2)
    for r in bombs:
        x, y = float(r['x']), float(r['y'])
        dot(x, y, 1.6 if mpp <= 1.0 else 3.0, (255, 255, 255) if r['c'] == '0' else (250, 220, 0))
    # Each counted hit: a line from its bomb to its piece, and its word.
    bomb_at = {r['a']: (float(r['x']), float(r['y'])) for r in bombs}
    for r in hits:
        x, y = float(r['x']), float(r['y'])
        if r['b'] == 'bomb' and r['d'] in bomb_at:
            bx, by = bomb_at[r['d']]
            d.line([to(bx, by), to(x, y)], fill=(250, 220, 0), width=1)
        u, v = to(x, y)
        d.text((u + 6, v - 14), '%s %s (#%s)' % (r['b'], r['c'], r['a']), fill=(10, 10, 10))
    # The strafing pass.
    tracks = {}
    for r in planes:
        tracks.setdefault(r['a'], []).append((float(r['x']), float(r['y']), r['c'] == '1'))
    for key, pts in tracks.items():
        for (ax, ay, _), (bx, by, fire) in zip(pts, pts[1:]):
            if math.hypot(bx - ax, by - ay) > 200:
                continue
            d.line([to(ax, ay), to(bx, by)], fill=(220, 20, 20) if fire else (40, 70, 200), width=2 if fire else 1)
    for r in rounds:
        dot(float(r['x']), float(r['y']), 0.6, (40, 30, 20))
    # A scale bar and the key.
    bar = 100.0 if mpp <= 1.0 else 500.0
    d.line([(20, H - 20), (20 + bar / mpp, H - 20)], fill=(0, 0, 0), width=3)
    d.text((20, H - 38), '%d m' % bar, fill=(0, 0, 0))
    d.text((20, 12), 'raid on troops (%s): road tan; pieces by kind, edged by end state (orange hit, red burn, dark red '
                     'wrecked); white bombs with blast rings; yellow: a bomb carrying a counted hit; red track: guns on'
           % view, fill=(0, 0, 0))
    img.save(out, optimize=True)
    print('wrote %s (%d x %d, %.2f m a pixel)' % (out, W, H, mpp))


if __name__ == '__main__':
    main()
