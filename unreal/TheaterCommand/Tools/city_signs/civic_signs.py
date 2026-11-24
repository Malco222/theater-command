"""
The town's buildings as signs for the city panel's AT A GLANCE page (release 2; Brian, 23 November 2026: "the
buildings that you can build have very poor graphics of what the buildings are"). One sign a line of building, in the
UI kit's own hand (Content/CityGlance/glance-factory.png): white outline strokes about 6 px wide on a clear 96 px
ground, round ends, a ground line under each building, to be tinted by the panel. Drawn at four times the size and
brought down, so the edges are smooth. A draft for the art director: redraw any of them and keep the file names.

    python3 unreal/TheaterCommand/Tools/city_signs/civic_signs.py
"""
import math
import os
from PIL import Image, ImageDraw

S = 4  # drawn at four times the size
N = 96 * S
W = 6 * S  # the stroke
OUT = os.path.join(os.path.dirname(__file__), '..', '..', 'Content', 'CityGlance')


def pen():
    im = Image.new('L', (N, N), 0)
    return im, ImageDraw.Draw(im)


def line(d, pts, w=W):
    d.line([(x * S, y * S) for x, y in pts], fill=255, width=w, joint='curve')
    for x, y in (pts[0], pts[-1]):
        r = w / 2
        d.ellipse((x * S - r, y * S - r, x * S + r, y * S + r), fill=255)


def dot(d, x, y, r):
    d.ellipse(((x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S), fill=255)


def ground(d):
    line(d, [(10, 82), (86, 82)])


def market(d):
    # A market hall's stall: a scalloped awning on two posts, and the counter.
    line(d, [(16, 34), (24, 18), (72, 18), (80, 34)])
    for i in range(4):
        x0 = 16 + i * 16
        d.arc(((x0) * S, (26) * S, (x0 + 16) * S, (42) * S), 0, 180, fill=255, width=W)
    line(d, [(22, 40), (22, 82)])
    line(d, [(74, 40), (74, 82)])
    line(d, [(22, 60), (74, 60)])
    ground(d)


def farm(d):
    # A sheaf of wheat, tied at the waist: three stalks fanning out, each with its ear of grain.
    for ang in (-24, 0, 24):
        a = math.radians(ang)
        bx, by = 48, 60  # the tie
        tx, ty = bx + math.sin(a) * 40, by - math.cos(a) * 40
        line(d, [(48 - math.sin(a) * 14, 82), (bx, by), (tx, ty)])
        # The ear: grains as short strokes on each side of the stalk's top, pointing up it.
        for k in range(3):
            f = 0.55 + k * 0.15
            cx, cy = bx + (tx - bx) * f, by + (ty - by) * f
            for side in (-1, 1):
                ga = a + side * math.radians(38)
                line(d, [(cx, cy), (cx + math.sin(ga) * 9, cy - math.cos(ga) * 9)], w=int(W * 0.8))
    line(d, [(40, 64), (56, 64)])
    ground(d)


def school(d):
    # An open book on a stand.
    line(d, [(48, 30), (48, 70)])
    line(d, [(48, 30), (34, 24), (14, 26), (14, 66), (34, 64), (48, 70)])
    line(d, [(48, 30), (62, 24), (82, 26), (82, 66), (62, 64), (48, 70)])
    line(d, [(22, 36), (38, 34)])
    line(d, [(22, 46), (38, 44)])
    line(d, [(58, 34), (74, 36)])
    line(d, [(58, 44), (74, 46)])
    ground(d)


def camp(d):
    # A barracks hut with its flag over the drill ground.
    line(d, [(14, 82), (14, 52), (50, 40), (86, 52), (86, 82)])
    line(d, [(42, 82), (42, 64), (58, 64), (58, 82)])
    line(d, [(24, 30), (24, 12)])
    d.polygon([(24 * S, 12 * S), (42 * S, 17 * S), (24 * S, 22 * S)], fill=255)
    line(d, [(24, 30), (24, 46)])
    ground(d)


def hospital(d):
    # A ward block with a cross over its door.
    line(d, [(16, 82), (16, 30), (80, 30), (80, 82)])
    line(d, [(48, 38), (48, 58)])
    line(d, [(38, 48), (58, 48)])
    line(d, [(40, 82), (40, 68), (56, 68), (56, 82)])
    line(d, [(24, 40), (28, 40)])
    line(d, [(68, 40), (72, 40)])
    ground(d)


def hall(d):
    # A town hall: a pediment over four columns on a step.
    line(d, [(12, 34), (48, 14), (84, 34), (12, 34)])
    for x in (22, 38, 58, 74):
        line(d, [(x, 42), (x, 72)])
    line(d, [(14, 76), (82, 76)])
    ground(d)


def save(name, draw):
    a, d = pen()
    draw(d)
    a = a.resize((96, 96), Image.LANCZOS)
    im = Image.new('RGBA', (96, 96), (255, 255, 255, 0))
    im.putalpha(a)
    im.save(os.path.join(OUT, f'glance-civic-{name}.png'))


if __name__ == '__main__':
    for n, f in (('market', market), ('farm', farm), ('school', school), ('camp', camp), ('hospital', hospital), ('hall', hall)):
        save(n, f)
    print('six signs drawn in', os.path.normpath(OUT))
