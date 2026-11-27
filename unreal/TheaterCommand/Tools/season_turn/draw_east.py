#!/usr/bin/env python3
"""Drawings of the season turn's mud over the game's own east (release 2, the campaign map look builder).

Lays a board from the game's own files (the provinces and who holds them at the start of 1939, the land, the rivers
and the heights the 3D game reads) and hands it to season_turn_test --map, which paints the mud with the game's own
Campaign/SeasonTurn.cpp at four moments of the turn. These are drawings of the ground layer the mud is painted into:
the bare country's colour by its kind (as Campaign/CampaignGround.cpp has it), woods as a darker green, and the mud.
In the game the nations' stain, the fields, the woods' cloth and the borders are drawn over it.

Usage: draw_east.py OUTDIR [x0 y0 x1 y1]   (map units; the default box runs from East Prussia to Moscow and Kharkov)
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
CELL = 1.5  # the game's own grid
EAST = {'POL', 'SOV', 'LIT', 'LAT', 'EST', 'ROM', 'FIN'}  # the rules' mud ground: what these hold (campaign-bridge.ts)

GROUND = {  # Campaign/CampaignGround.cpp GroundColor
    'forest': (0x6C, 0x7F, 0x55), 'hills': (0xA3, 0x96, 0x70), 'mountain': (0x8D, 0x86, 0x76), 'urban': (0x9C, 0x94, 0x88),
    'desert': (0xD3, 0xBC, 0x86), 'marsh': (0x7C, 0x8F, 0x78), 'plains': (0xA8, 0xA7, 0x77)}
WOODS = {'forest': 0.26, 'hills': 0.05, 'mountain': 0.10, 'marsh': 0.10, 'urban': -0.14, 'desert': -0.6, 'plains': -0.07}
RELIEF = {'mountain': 7.0, 'hills': 2.1, 'forest': 0.55, 'desert': 0.5, 'urban': 0.25, 'marsh': 0.1, 'plains': 0.3}


def smooth_noise(nx, ny, scale, seed):
    """Value noise changing over about `scale` cells, 0 to 1."""
    rng = np.random.default_rng(seed)
    gx, gy = int(nx / scale) + 3, int(ny / scale) + 3
    grid = rng.random((gy, gx))
    img = Image.fromarray((grid * 65535).astype(np.uint16)).resize((int(gx * scale), int(gy * scale)), Image.BICUBIC)
    return np.clip(np.asarray(img, dtype=np.float64)[:ny, :nx] / 65535.0, 0.0, 1.0)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else '/tmp/season_turn_east'
    x0, y0, x1, y1 = (float(v) for v in sys.argv[2:6]) if len(sys.argv) >= 6 else (760.0, 560.0, 1300.0, 960.0)
    os.makedirs(out, exist_ok=True)
    nx, ny = int((x1 - x0) / CELL), int((y1 - y0) / CELL)
    to_px = lambda p: ((p[0] - x0) / CELL, (p[1] - y0) / CELL)

    data = json.load(open(os.path.join(ROOT, 'src/generated/campaign-map.json')))
    rivers = json.load(open(os.path.join(ROOT, 'src/generated/campaign-rivers.json')))['rivers']

    # The provinces, each its own number on a picture of the box; the land; the rivers.
    owner_img = Image.new('I', (nx, ny), 0)
    draw = ImageDraw.Draw(owner_img)
    regions = data['regions']
    for k, r in enumerate(regions):
        for poly in r['polys']:
            ring = poly[0] if poly and isinstance(poly[0][0], list) else poly
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

    # The heights, as the game lifts them (Campaign/CampaignTerrain.cpp): the shelf, and 5.6 (m / 2500)^0.78 over it.
    raw = open(os.path.join(GAME, 'Content/Data/Campaign/elevation.bin'), 'rb').read()
    ew, eh = struct.unpack('<II', raw[4:12])
    metres = np.frombuffer(raw[12:12 + ew * eh * 2], dtype='<i2').reshape(eh, ew).astype(np.float64)
    xs = x0 + np.arange(nx) * CELL
    ys = y0 + np.arange(ny) * CELL
    xi = np.clip(xs.astype(int), 0, ew - 1)
    yi = np.clip(ys.astype(int), 0, eh - 1)
    m = np.maximum(metres[np.ix_(yi, xi)], 0.0)
    height = 0.55 + 5.6 * np.power(m / 2500.0, 0.78)

    terrain = np.array([r['terrain'] for r in regions])
    owners = np.array([r['owner'] for r in regions])
    inside = region >= 0
    kind = np.zeros((ny, nx), dtype=np.uint8)
    kind[inside & np.isin(owners[np.clip(region, 0, None)], list(EAST))] = 1
    kind[~land | river] = 2
    kind[~inside & land & ~river] = 0

    rough = np.zeros((ny, nx))
    base = np.zeros((ny, nx, 3))
    woods_lean = np.zeros((ny, nx))
    for name, colour in GROUND.items():
        sel = inside & (terrain[np.clip(region, 0, None)] == name)
        base[sel] = colour
        rough[sel] = RELIEF[name]
        woods_lean[sel] = WOODS[name]
    base[~inside] = (0xA4, 0x9C, 0x86)
    # Woods in standing masses, as the game lays them: a noise with each kind's lean added.
    wooded = np.clip((smooth_noise(nx, ny, 9.0, 5) - 0.62 + woods_lean) * 5.0, 0.0, 1.0)
    grain = 0.94 + 0.12 * smooth_noise(nx, ny, 2.5, 7)
    rgb = base * (1.0 - 0.55 * wooded[..., None]) + np.array([58, 74, 46]) * 0.55 * wooded[..., None]
    rgb *= grain[..., None]
    rgb[kind == 2] = (78, 100, 116)
    rgb[river & land] = (86, 110, 124)
    rgb = np.clip(rgb, 0, 255).astype(np.uint8)

    board = os.path.join(out, 'east.board')
    with open(board, 'wb') as f:
        f.write(struct.pack('<iid', nx, ny, CELL))
        f.write(rgb.tobytes())
        f.write(kind.tobytes())
        f.write(height.astype('<f4').tobytes())
        f.write(rough.astype('<f4').tobytes())
        f.write((wooded * 255).astype(np.uint8).tobytes())

    subprocess.run([os.path.join(HERE, 'run.sh'), '--map', board, out], check=True)

    # The four moments side by side, each twice the game's grid (smoothed between points as the table's picture is).
    names = [('0-before', 'Before the turn'), ('1-one-second', 'One second in'), ('2-two-seconds', 'Two seconds in'), ('3-settled', 'Settled')]
    frames = []
    for key, label in names:
        img = Image.open(os.path.join(out, f'mud-{key}.ppm')).convert('RGB')
        img = img.resize((nx * 2, ny * 2), Image.BILINEAR)
        img.save(os.path.join(out, f'mud-{key}.png'))
        frames.append((img, label))
    font = ImageFont.load_default(size=22) if hasattr(ImageFont, 'load_default') else None
    pad = 34
    strip = Image.new('RGB', (nx * 2 * 2 + 12, (ny * 2 + pad) * 2 + 12), (28, 26, 22))
    for k, (img, label) in enumerate(frames):
        cx, cy = (k % 2) * (nx * 2 + 12), (k // 2) * (ny * 2 + pad + 12)
        strip.paste(img, (cx, cy + pad))
        ImageDraw.Draw(strip).text((cx + 8, cy + 6), label, fill=(236, 228, 210), font=font)
    strip.save(os.path.join(out, 'mud-turn-east.png'))
    print('drew', os.path.join(out, 'mud-turn-east.png'))


if __name__ == '__main__':
    main()
