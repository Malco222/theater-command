#!/usr/bin/env python3
"""Drawings of churned ground between dug-in lines over the game's own ground (release 2, the campaign map look builder;
the art director's sheet art-director/fronts-alive/fronts-alive-look.md, piece 1).

Lays a board from the game's own files (the provinces, the land and the heights the 3D game reads) and hands it to
front_churn_test --map, which chains the front with the game's own Campaign/FrontLine and paints the band with the
game's own Campaign/FrontChurn at the sheet's months. The front is drawn along the German-Polish border of the
game's 1939 start, as though both sides had dug in along it: a stand-in, since the cloud has no war of its own. These
are drawings of the ground picture and the fine veil and craters over it; in the game the nations' stain, the fields
and the woods' cloth are drawn over the ground picture as over the ground beside the band.

Usage: draw_churn.py OUTDIR
"""
import json
import os
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
GAME = os.path.abspath(os.path.join(HERE, '..', '..'))
CELL = 1.5
BOX = (640.0, 640.0, 910.0, 900.0)
SIDES = ('GER', 'POL')

GROUND = {  # Campaign/CampaignGround.cpp GroundColor
    'forest': (0x6C, 0x7F, 0x55), 'hills': (0xA3, 0x96, 0x70), 'mountain': (0x8D, 0x86, 0x76), 'urban': (0x9C, 0x94, 0x88),
    'desert': (0xD3, 0xBC, 0x86), 'marsh': (0x7C, 0x8F, 0x78), 'plains': (0xA8, 0xA7, 0x77)}
WOODS = {'forest': 0.26, 'hills': 0.05, 'mountain': 0.10, 'marsh': 0.10, 'urban': -0.14, 'desert': -0.6, 'plains': -0.07}


def smooth_noise(nx, ny, scale, seed):
    rng = np.random.default_rng(seed)
    gx, gy = int(nx / scale) + 3, int(ny / scale) + 3
    grid = rng.random((gy, gx))
    img = Image.fromarray((grid * 65535).astype(np.uint16)).resize((int(gx * scale), int(gy * scale)), Image.BICUBIC)
    return np.clip(np.asarray(img, dtype=np.float64)[:ny, :nx] / 65535.0, 0.0, 1.0)


def ring_of(poly):
    return poly[0] if poly and isinstance(poly[0][0], list) else poly


def inside(ring, x, y):
    hit = False
    n = len(ring)
    for i in range(n):
        x1, y1 = ring[i]
        x2, y2 = ring[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            hit = not hit
    return hit


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else '/tmp/front_churn_draw'
    os.makedirs(out, exist_ok=True)
    x0, y0, x1, y1 = BOX
    nx, ny = int((x1 - x0) / CELL), int((y1 - y0) / CELL)
    to_px = lambda p: ((p[0] - x0) / CELL, (p[1] - y0) / CELL)
    data = json.load(open(os.path.join(ROOT, 'src/generated/campaign-map.json')))
    rivers = json.load(open(os.path.join(ROOT, 'src/generated/campaign-rivers.json')))['rivers']
    regions = data['regions']

    # The ground: each province by its kind, woods in standing masses, the rivers and the sea.
    owner_img = Image.new('I', (nx, ny), 0)
    draw = ImageDraw.Draw(owner_img)
    for k, r in enumerate(regions):
        for poly in r['polys']:
            ring = ring_of(poly)
            if len(ring) >= 3:
                draw.polygon([to_px(p) for p in ring], fill=k + 1)
    region = np.asarray(owner_img, dtype=np.int64) - 1
    land_img = Image.new('L', (nx, ny), 0)
    draw = ImageDraw.Draw(land_img)
    for poly in data['land']:
        rings = poly if isinstance(poly[0][0], list) else [poly]
        draw.polygon([to_px(p) for p in rings[0]], fill=255)
        for hole in rings[1:]:
            draw.polygon([to_px(p) for p in hole], fill=0)
    land = np.asarray(land_img) > 0
    river_img = Image.new('L', (nx, ny), 0)
    draw = ImageDraw.Draw(river_img)
    for river in rivers:
        pts = [to_px(p) for p in river['pts']]
        if len(pts) >= 2:
            draw.line(pts, fill=255, width=1)
    river = np.asarray(river_img) > 0
    terrain = np.array([r['terrain'] for r in regions])
    inside_any = region >= 0
    base = np.zeros((ny, nx, 3))
    lean = np.zeros((ny, nx))
    for name, colour in GROUND.items():
        sel = inside_any & (terrain[np.clip(region, 0, None)] == name)
        base[sel] = colour
        lean[sel] = WOODS[name]
    base[~inside_any] = (0xA4, 0x9C, 0x86)
    wooded = np.clip((smooth_noise(nx, ny, 9.0, 5) - 0.62 + lean) * 5.0, 0.0, 1.0)
    base *= (0.94 + 0.12 * smooth_noise(nx, ny, 2.5, 7))[..., None]
    kind = np.zeros((ny, nx), dtype=np.uint8)
    water = ~land | river
    base[~land] = (78, 100, 116)
    base[river & land] = (86, 110, 124)
    kind[water] = 3
    rgb = np.clip(base, 0, 255).astype(np.uint8)

    # The front: every stretch of border between the two sides' provinces, found as the map finds its shared edges
    # (an edge of one province whose outside, a little way off, lies in the other).
    by_owner = {s: [r for r in regions if r['owner'] == s] for s in SIDES}
    pieces, pairs = [], []
    for a in by_owner[SIDES[0]]:
        for nb in a['nb']:
            b = regions[nb]
            if b['owner'] != SIDES[1]:
                continue
            got = 0
            for poly in a['polys']:
                ring = ring_of(poly)
                area = sum(ring[i][0] * ring[(i + 1) % len(ring)][1] - ring[(i + 1) % len(ring)][0] * ring[i][1] for i in range(len(ring)))
                outward = 1.0 if area >= 0 else -1.0
                for i in range(len(ring)):
                    (ax, ay), (bx, by) = ring[i], ring[(i + 1) % len(ring)]
                    length = ((bx - ax) ** 2 + (by - ay) ** 2) ** 0.5
                    if length < 0.05:
                        continue
                    dx, dy = (bx - ax) / length, (by - ay) / length
                    px, py = (ax + bx) / 2 + dy * outward * 0.35, (ay + by) / 2 - dx * outward * 0.35
                    if any(inside(ring_of(q), px, py) for q in b['polys']):
                        pieces.append((ax, ay, bx, by, a['id'], b['id']))
                        got += 1
            if got:
                pairs.append((a['id'], b['id'], (a['x'] + b['x']) / 2, (a['y'] + b['y']) / 2))
    print('pieces', len(pieces), 'pairs', len(pairs))

    # Where to look: theatre height (the ground picture alone, a point a pixel), working height and close up.
    silesia = next(p for p in pairs if regions[p[0]]['name'] == 'Lower Silesia' and regions[p[1]]['name'] == 'Poznan')
    cx, cy = silesia[2], silesia[3]
    views = [(x0, y0, x1, y1, 1.5, 'theatre'), (cx - 45, cy - 30, cx + 45, cy + 30, 0.25, 'working'), (cx - 14, cy - 9, cx + 14, cy + 9, 0.07, 'close')]

    board = os.path.join(out, 'front.board')
    with open(board, 'wb') as f:
        f.write(struct.pack('<iiddd', nx, ny, CELL, x0, y0))
        f.write(rgb.tobytes())
        f.write((wooded * 255).astype(np.uint8).tobytes())
        f.write(kind.tobytes())
        f.write(struct.pack('<i', len(pieces)))
        for ax, ay, bx, by, a, b in pieces:
            f.write(struct.pack('<ddddii', ax, ay, bx, by, a, b))
        f.write(struct.pack('<i', len(pairs)))
        for a, b, fx, fy in pairs:
            f.write(struct.pack('<iidd', a, b, fx, fy))
        f.write(struct.pack('<i', len(views)))
        for vx0, vy0, vx1, vy1, px, name in views:
            f.write(struct.pack('<ddddd', vx0, vy0, vx1, vy1, px))
            f.write(name.encode().ljust(32, b'\0'))
    subprocess.run([os.path.join(HERE, 'run.sh'), '--map', board, out], check=True)

    font = ImageFont.load_default(size=20) if hasattr(ImageFont, 'load_default') else None

    def sheet(view, moments, cols, scale, name):
        tiles = []
        for key, label in moments:
            img = Image.open(os.path.join(out, f'churn-{view}-{key}.ppm')).convert('RGB')
            if scale != 1:
                img = img.resize((int(img.width * scale), int(img.height * scale)), Image.BILINEAR)
            tiles.append((img, label))
        w, h, pad = tiles[0][0].width, tiles[0][0].height, 30
        rows = (len(tiles) + cols - 1) // cols
        page = Image.new('RGB', (cols * (w + 10), rows * (h + pad + 10)), (28, 26, 22))
        for k, (img, label) in enumerate(tiles):
            px, py = (k % cols) * (w + 10), (k // cols) * (h + pad + 10)
            page.paste(img, (px, py + pad))
            ImageDraw.Draw(page).text((px + 6, py + 4), label, fill=(236, 228, 210), font=font)
        page.save(os.path.join(out, name))
        print('drew', os.path.join(out, name))

    growth = [('m0', 'Month 1: nothing yet'), ('m2', 'Month 2'), ('m4', 'Month 4'), ('m6', 'Month 6: whole')]
    healing = [('m6', 'Whole'), ('heal3', 'Moved on 3 months'), ('heal8', '8 months'), ('heal12', '12 months: gone')]
    seasons = [('m6', 'Whole, dry'), ('m6-mud', 'Whole, mud months'), ('m6-snow', 'Whole, under snow')]
    sheet('working', growth, 2, 1, 'churn-working-growth.png')
    sheet('working', healing, 2, 1, 'churn-working-healing.png')
    sheet('working', seasons, 3, 1, 'churn-working-seasons.png')
    sheet('close', growth[1:] + [('heal3', 'Moved on 3 months')], 2, 1, 'churn-close.png')
    sheet('theatre', [('m0', 'Theatre height, month 1'), ('m6', 'Theatre height, month 6')], 2, 2, 'churn-theatre.png')
    sheet('edge', [('m6', 'An edge at 2x close, month 6'), ('m2', 'The same, month 2')], 1, 1, 'churn-edge-2x.png')
    if os.path.exists(os.path.join(out, 'churn-end-m6.ppm')):
        sheet('end', [('m6', 'A band end, month 6'), ('m2', 'A band end, month 2')], 2, 1, 'churn-end.png')
    if os.path.exists(os.path.join(out, 'churn-corner-m6.ppm')):
        sheet('corner', [('m6', 'A corner, month 6'), ('heal8', 'A corner, 8 months healed')], 2, 1, 'churn-corner.png')


if __name__ == '__main__':
    main()
