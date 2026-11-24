"""
Two signs for the town card's tiles (release 2, the UI director, 24 November 2026), in the UI kit's own hand as
Tools/city_signs/civic_signs.py draws them: white outline strokes about 6 px wide on a clear 96 px ground, round ends,
to be tinted by the panel. The coin (what a province pays a month: the kit's coin, two rings and a bar) and the prize
pennant (what the province is worth in the war). Drawn at four times the size and brought down.

    python3 unreal/TheaterCommand/Tools/town_card/town_signs.py
"""
import os
from PIL import Image, ImageDraw

S = 4
N = 96 * S
W = 6 * S
OUT = os.path.join(os.path.dirname(__file__), '..', '..', 'Content', 'TownCard')


def pen():
    im = Image.new('L', (N, N), 0)
    return im, ImageDraw.Draw(im)


def line(d, pts, w=W):
    d.line([(x * S, y * S) for x, y in pts], fill=255, width=w, joint='curve')
    for x, y in (pts[0], pts[-1]):
        r = w / 2
        d.ellipse((x * S - r, y * S - r, x * S + r, y * S + r), fill=255)


def ring(d, cx, cy, r, w=W):
    d.ellipse(((cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S), outline=255, width=w)


def save(im, name):
    small = im.resize((96, 96), Image.LANCZOS)
    out = Image.new('RGBA', (96, 96), (255, 255, 255, 0))
    out.putalpha(small)
    os.makedirs(OUT, exist_ok=True)
    out.save(os.path.join(OUT, name))


im, d = pen()
ring(d, 48, 48, 34)
ring(d, 48, 48, 20)
line(d, [(48, 39), (48, 57)])
save(im, 'sign-coin.png')

im, d = pen()
line(d, [(30, 14), (30, 84)])
line(d, [(22, 84), (40, 84)])
line(d, [(30, 16), (74, 30), (30, 44)])
save(im, 'sign-worth.png')
print('signs in', os.path.abspath(OUT))
