#!/usr/bin/env python3
"""The downed pilots' picture (Effects, release 2, -TheaterLandChutes), redrawn for the art director's notes of 24
November (02:26Z): land_air_test's four pilots as LandAir brings them down, two on a field with a light wind and two
on one with a wind over 4 m/s (the first of those out of an Il-2 under 60 m).

Each pilot's row: his first seconds out, how far under where he left her against time (falling free red, his canopy
filling amber, full green), with half g t squared dashed (her first note: he is never above it, nor above where he
left her); his way through the air, the wind's taken off, dying away (her second: the wind is no push from rest, and
he is with it within a few seconds of his canopy filling); and down, his seconds on the field as his clips play them
(her third, fourth and fifth: over flat in his chute's mesh, swapped for the crowd's man, dragged, up onto his knees,
his harness, up, running or limping, down).

Its last rows: his canopy as the scene draws it (land_air_test's chute_silk.txt, LandAir's own mesh), shaded much as
the pieces' paint is (the sun's light, and on the side turned from the sun the share of its own colour the scene gives
back, its glow): side-on in the sun through a long lens (the art director's verdict of 24 November 14:34Z, (c): its hem
pulled in toward its lines), from under it 100 m off with the sun behind it with its glow and without, and falling in
a quarter at a time across the wind; and lying on the field (her verdict, (b)): from 40 m at a man's eye height, and
from above, his and a second pilot's (her note of 24 November 17:40Z: its folds across it, its uneven outline and
its hem; each pixel of the silk lit by its facing there, as the scene lights it). Each down view draws him lying (a
dark bar) and his lines back to him.
It measures in the picture what she asked for, from the pixels it drew, and prints it (and writes it under each view).
Usage: chutes.py <air_paths.txt> <chute_silk.txt> <out.png>"""
import math
import sys
from collections import defaultdict

import numpy as np
from PIL import Image, ImageDraw

SRC, SILK, DST = sys.argv[1], sys.argv[2], sys.argv[3]
G = 9.81

chutes = defaultdict(list)
winds = {}
for line in open(SRC):
    w = line.split()
    if not w:
        continue
    if w[0] == 'w':
        winds[int(w[1])] = (float(w[2]), float(w[3]), float(w[4]))
    elif w[0] == 'c' and len(w) >= 22:
        chutes[int(w[1])].append(dict(T=float(w[2]), x=float(w[3]), y=float(w[4]), z=float(w[5]), open=float(w[6]), swing=float(w[7]),
                                      down=w[8] == '1', clip=w[9], hx=float(w[10]), hy=float(w[11]), slump=float(w[12]), inmesh=w[13] == '1',
                                      fallen=float(w[14]), rate=float(w[15]), hard=w[16] == '1', dragged=float(w[17]), time=float(w[18]),
                                      vx=float(w[19]), vy=float(w[20]), vz=float(w[21])))

meshes = {}
cur = None
for line in open(SILK):
    w = line.split()
    if not w:
        continue
    if w[0] == 'mesh':
        cur = meshes.setdefault(int(w[1]), {'p': [], 'n': [], 'c': [], 't': [], 'gores': int(w[2]), 's': [], 'h': None})
    elif w[0] == 'h':
        cur['h'] = [float(a) for a in w[1:5]]
    elif w[0] == 's':
        cur['s'].append(int(w[1]))
        cur.setdefault('k', []).append(int(w[2]) if len(w) > 2 else len(cur['s']) - 1)
    elif w[0] == 'l':
        v = [float(a) for a in w[1:]]
        cur.setdefault('l', []).append([v[k:k + 3] for k in range(0, len(v), 3)])
    elif w[0] == 'p':
        v = [float(a) for a in w[1:]]
        cur['p'].append(v[0:3])
        cur['n'].append(v[3:6])
        cur['c'].append(v[6:9])
    elif w[0] == 't':
        cur['t'].append([int(a) for a in w[1:4]])

PAPER, INK, GREY = (246, 244, 238), (20, 20, 20), (150, 150, 140)
RED, AMBER, GREEN, BLUE = (200, 40, 30), (220, 150, 20), (40, 140, 60), (40, 90, 200)
PHASE_FILL = {'over': (170, 170, 160), 'lies': (110, 110, 100), 'knees': (220, 150, 20), 'harness': (200, 40, 30),
              'stands': (40, 140, 60), 'runs': (40, 90, 200), 'limps': (120, 60, 160), 'down': (60, 60, 60)}
ROW = 250
keys = sorted(chutes)
W, H = 1640, 60 + ROW * len(keys) + 360 + 400
im = Image.new('RGB', (W, H), PAPER)
d = ImageDraw.Draw(im)
d.text((20, 12), "Downed pilots, release 2 (-TheaterLandChutes with -TheaterLandAirSupport): the art director's notes of 24 November, 02:26Z, "
       "as LandAir brings land_air_test's pilots down", fill=INK)
d.text((20, 30), "Each dot a twentieth of a battle second (a tenth of a real one). Seconds are real seconds.", fill=INK)


def smooth(x):
    x = min(1.0, max(0.0, x))
    return x * x * (3.0 - 2.0 * x)


def dashed(pts, fill):
    for k in range(0, len(pts) - 1, 2):
        d.line((pts[k][0], pts[k][1], pts[k + 1][0], pts[k + 1][1]), fill=fill)


def phase_of(p, ran):
    if p['inmesh']:
        return 'over'
    if p['clip'] == 'go_prone' and p['rate'] == 0.0 and not ran and p['time'] > 1.89:
        return 'lies'
    if p['clip'] == 'go_prone' and p['rate'] < 0.0:
        return 'knees'
    if p['clip'] == 'kneel_down':
        return 'harness'
    if p['clip'] == 'stand_up':
        return 'stands'
    if p['clip'] == 'run':
        return 'limps' if p['hard'] else 'runs'
    return 'down'


for row, key in enumerate(keys):
    pts = chutes[key]
    tag = 100 if key >= 100 else 0
    wx, wy, life = winds.get(tag, (0.0, 0.0, 2.0))
    oy = 60 + row * ROW
    first = pts[0]
    air = [p for p in pts if not p['down']]
    ground = [p for p in pts if p['down']]
    exit_h = first['z']
    t0 = first['T']

    def sec(p):
        return (p['T'] - t0) * life

    full = next((p for p in air if p['open'] >= 1.0), None)
    pull = next((p for p in air if p['open'] > 0.0), None)
    landed = ground[0] if ground else air[-1]
    field = ('the windy field (%.1f m/s)' if tag else 'a light wind (%.1f m/s)') % math.hypot(wx, wy)
    sink = [-p['vz'] for p in air if p['open'] >= 1.0 and p['z'] > 3.0 and sec(p) > (sec(full) + 2.0 if full else 1e9)]
    head = "pilot of plane %d, %s: out %.0f m up%s; %s; down at %s; landed %s, %.0f s after he left" % (
        key % 100, field, exit_h, ' (under 60 m)' if exit_h < 60 else '',
        ('his canopy full %.1f s out, %.0f m under where he left her' % (sec(full), exit_h - full['z'])) if full
        else 'his canopy still filling as he lands (%.0f%% open)' % (landed['open'] * 100),
        ('%.1f to %.1f m/s' % (min(sink), max(sink))) if sink else 'his canopy never full', 'hard' if landed['hard'] else 'soft', sec(landed))
    d.text((20, oy), head, fill=INK)

    # (A) his first seconds: how far under where he left her
    ax, ay, aw, ah = 40, oy + 22, 520, 200
    span_t = 10.0
    span_z = max(90.0, min(exit_h + 5.0, 120.0))
    d.rectangle((ax, ay, ax + aw, ay + ah), outline=GREY)
    title = "his first %d s: metres under where he left her" % span_t
    d.text((ax + aw - 6 - d.textlength(title), ay + 4), title, fill=INK)

    def X(t):
        return ax + t / span_t * aw

    def Y(z):
        return ay + z / span_z * ah

    for m in range(0, int(span_z) + 1, 20):
        d.line((ax - 4, Y(m), ax, Y(m)), fill=GREY)
        d.text((ax - 30, Y(m) - 6), '%d' % m, fill=GREY)
    for s in range(0, int(span_t) + 1, 2):
        d.text((X(s) - 4, ay + ah + 2), '%d' % s, fill=GREY)
    curve = []
    for k in range(0, 201):
        t = span_t * k / 200.0
        if 0.5 * G * t * t > span_z:
            break
        curve.append((X(t), Y(0.5 * G * t * t)))
    dashed(curve, (120, 120, 110))
    d.text((X(2.3) + 6, Y(0.5 * G * 2.3 * 2.3)), "1/2 g t^2", fill=(120, 120, 110))
    if exit_h < span_z:
        d.line((ax, Y(exit_h), ax + aw, Y(exit_h)), fill=(90, 110, 60), width=2)
        d.text((ax + aw - 60, Y(exit_h) - 14), "the field", fill=(90, 110, 60))
    for p in air:
        t = sec(p)
        if t > span_t:
            break
        c = RED if p['open'] <= 0.0 else AMBER if p['open'] < 1.0 else GREEN
        u, v = X(t), Y(exit_h - p['z'])
        d.ellipse((u - 2, v - 2, u + 2, v + 2), fill=c)
    if pull:
        d.text((X(sec(pull)) + 10, Y(exit_h - pull['z']) + 2), "pulls at %.1f s" % sec(pull), fill=AMBER)
    if full and sec(full) < span_t:
        d.text((X(sec(full)) + 8, Y(exit_h - full['z']) - 4), "full, %.0f m down" % (exit_h - full['z']), fill=GREEN)

    # (B) his way through the air, the wind's taken off
    bx, by, bw, bh = 600, oy + 22, 380, 200
    d.rectangle((bx, by, bx + bw, by + bh), outline=GREY)
    title = "his way through the air (m/s, the wind's taken off)"
    d.text((bx + bw - 6 - d.textlength(title), by + 4), title, fill=INK)
    rels = [(sec(p), math.hypot(p['vx'] - wx, p['vy'] - wy)) for p in air if sec(p) <= span_t]
    top = max(10.0, max(r for _, r in rels) if rels else 10.0)

    def BX(t):
        return bx + t / span_t * bw

    def BY(r):
        return by + bh - r / top * (bh - 50)

    d.line((bx, BY(1.0), bx + bw, BY(1.0)), fill=(200, 200, 190))
    d.text((bx + bw - 44, BY(1.0) - 14), "1 m/s", fill=GREY)
    for s in range(0, int(span_t) + 1, 2):
        d.text((BX(s) - 4, by + bh + 2), '%d' % s, fill=GREY)
    for t, r in rels:
        d.ellipse((BX(t) - 2, BY(r) - 2, BX(t) + 2, BY(r) + 2), fill=BLUE)
    d.text((bx + bw - 6 - d.textlength("out at 000 m/s against the air"), by + 18), "out at %.0f m/s against the air" % (rels[0][1] if rels else 0.0), fill=BLUE)
    with_wind = next((p for p in air if math.hypot(p['vx'] - wx, p['vy'] - wy) < 1.0), None)
    if with_wind is not None:
        words = "with the wind (within 1 m/s of it) %.1f s out, %.0f m down" % (sec(with_wind), exit_h - with_wind['z'])
        d.text((bx + bw - 6 - d.textlength(words), by + 32), words, fill=GREEN)
    elif ground:
        words = "lands %.1f m/s through the air, his canopy %.0f%% open" % (math.hypot(air[-1]['vx'] - wx, air[-1]['vy'] - wy), ground[0]['open'] * 100)
        d.text((bx + bw - 6 - d.textlength(words), by + 32), words, fill=RED)
    if full and sec(full) < span_t:
        d.line((BX(sec(full)), by + 44, BX(sec(full)), by + bh), fill=GREEN)
        d.text((BX(sec(full)) + 4, by + 46), "his canopy full", fill=GREEN)

    # (C) down: his seconds on the field
    cx, cy, cw, ch = 1020, oy + 22, 600, 200
    d.rectangle((cx, cy, cx + cw, cy + ch), outline=GREY)
    d.text((cx + 4, cy + 2), "down: his seconds on the field from his landing (%s landing)" % ('a hard' if landed['hard'] else 'a soft'), fill=INK)
    if ground:
        tl = ground[0]['T']

        def gsec(p):
            return (p['T'] - tl) * life

        ran = False
        segs = []
        for p in ground:
            ph = phase_of(p, ran)
            ran = ran or ph in ('runs', 'limps')
            if not segs or segs[-1][0] != ph:
                segs.append([ph, gsec(p), gsec(p)])
            segs[-1][2] = gsec(p) + 0.1
            if gsec(p) > 30.0:
                break
        span = max(10.0, math.ceil(min(30.0, segs[-1][2]) / 5.0) * 5.0)

        def CX(t):
            return cx + 10 + min(t, span) / span * (cw - 20)

        for k, (ph, a, b) in enumerate(segs):
            d.rectangle((CX(a), cy + 40, CX(b), cy + 70), fill=PHASE_FILL[ph])
            words = ph if ph in ('runs', 'limps', 'down') else "%s %.1f s" % (ph, b - a)
            d.text((CX(a) + 2, cy + 74 + (k % 4) * 13), words, fill=PHASE_FILL[ph])
        for s in range(0, int(span) + 1, 5):
            d.text((CX(s) - 4, cy + 26), '%d' % s, fill=GREY)
        drags = [(gsec(p), p['dragged']) for p in ground if gsec(p) < span]
        most = max(r for _, r in drags)
        if most > 0.0:
            d.line([(CX(t), cy + 160 - r * 20) for t, r in drags], fill=(150, 90, 40), width=2)
            d.text((cx + 10, cy + 164), "dragged by his canopy %.1f m downwind (the line, 20 px a metre)" % most, fill=(150, 90, 40))
        else:
            d.text((cx + 10, cy + 164), "a light wind: not dragged", fill=GREY)
        runs = [p for p in ground if p['clip'] == 'run']
        swap = next((gsec(p) for p in ground if not p['inmesh']), 0.0)
        if runs:
            dist = math.hypot(runs[-1]['x'] - runs[0]['x'], runs[-1]['y'] - runs[0]['y'])
            dt = max(1e-3, gsec(runs[-1]) - gsec(runs[0]))
            d.text((cx + 10, cy + 180), "%s %.0f m at %.1f m/s, then to ground; the crowd's man from %.1f s" % (
                'limps' if runs[0]['hard'] else 'runs', dist, dist / dt, swap), fill=INK)

# The canopy's look: LandAir's own silk, shaded much as the pieces' paint shades it.
ly = 60 + ROW * len(keys)
d.text((20, ly), "his canopy as the scene draws it (LandAir's mesh: %d gores, its seams and hem a little darker in its points' colours; "
       "both its faces; shaded here much as the pieces' paint is). Measured in the picture, from the pixels drawn." % meshes[0]['gores'], fill=INK)


def norm(v):
    n = math.sqrt(sum(a * a for a in v)) or 1.0
    return [a / n for a in v]


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def glow(eye, at, sun):
    """LandAir::Glow, as the scene gives it to the silk's ink (its FigureFill)."""
    way, s = norm([a - b for a, b in zip(at, eye)]), norm(sun)
    if s[2] <= 0.0:
        return 0.0
    day = min(1.0, s[2] / 0.1)
    under = min(1.0, max(0.0, way[2] * 2.0))
    return day * (0.15 * under + 0.75 * smooth((dot(way, s) - 0.3) / 0.65))


def srgb(lin):
    return tuple(int(255 * min(1.0, max(0.0, c)) ** (1 / 2.2)) for c in lin)


class View:
    """A camera: eye, looking at at, scale pixels a radian, centred in box (hint: which way is up in it)."""

    def __init__(self, eye, at, scale, box, hint=(0.0, 0.0, 1.0)):
        self.eye, self.scale, self.box = eye, scale, box
        self.fwd = norm([a - b for a, b in zip(at, eye)])
        self.right = norm(cross(self.fwd, list(hint)))
        self.up = cross(self.right, self.fwd)
        x0, y0, x1, y1 = box
        self.cx, self.cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0 + 10

    def at(self, v):
        rel = [v[k] - self.eye[k] for k in range(3)]
        z = dot(rel, self.fwd)
        return (self.cx + dot(rel, self.right) / z * self.scale, self.cy - dot(rel, self.up) / z * self.scale)

    def depth(self, v):
        return dot([v[k] - self.eye[k] for k in range(3)], self.fwd)


def silk_polys(mesh, view, sun, fill):
    """Each triangle of the silk's face toward the eye, projected, far first, with each corner's facing and colour: the
    scene lights each pixel by its points' facings across the triangle, not one light a triangle (the art director's
    note of 24 November 17:40Z: a fold across the silk is a darker band and a lighter one between two rings)."""
    L = norm(sun)
    tris = []
    P, N, C = mesh['p'], mesh['n'], mesh['c']
    for t in mesh['t']:
        a, b, c = (P[i] for i in t)
        mid = [(a[k] + b[k] + c[k]) / 3.0 for k in range(3)]
        n = norm([sum(N[i][k] for i in t) for k in range(3)])
        if dot(n, [e - m for e, m in zip(view.eye, mid)]) <= 0.0:
            continue   # (the face of the silk turned from the eye: its other face, on the other side, is drawn)
        col = [sum(C[i][k] for i in t) / 3.0 for k in range(3)]
        lit = dot(n, L)
        shade = 0.22 + 0.78 * max(0.0, lit) + (fill * min(1.0, -lit * 1.4) if lit < 0.0 else 0.0)
        tris.append((view.depth(mid), [view.at(v) for v in (a, b, c)], srgb([col[k] * shade for k in range(3)]), [norm(N[i]) for i in t], [C[i] for i in t],
                     [view.depth(v) for v in (a, b, c)]))
    tris.sort(key=lambda e: -e[0])
    return tris, L, fill


def panel(box, title, ground=None):
    x0, y0, x1, y1 = box
    # (no silk drawn in it yet: a panel's lines are tested against its own silk only, not a panel's drawn before it)
    DEPTH[max(0, y0):min(H, y1 + 1), max(0, x0):min(W, x1 + 1)] = np.inf
    if ground:
        d.rectangle((x0 + 1, y0 + 1, x1 - 1, y1 - 1), fill=ground)
    d.rectangle(box, outline=GREY)
    d.text((x0 + 6, y0 + 4), title, fill=INK)


def paint(drawn, box, onto=None, depth=True):
    """Draws the triangles far first, clipped to the box (onto: an image of their own, for a layer), lit much as the
    pieces' paint is: the sun's light, and on the side turned from the sun the share of its own colour the scene gives
    back, its glow (fill). Each is drawn whole in one light (its corners' facings' mean, as before), and then each pixel
    inside it lit by its facing there, its corners' between them, as the scene lights it (the art director's note of 24
    November 17:40Z: a fold across the silk is a darker band and a lighter one between two rings, which one light a
    triangle had shown as one shade). Each pixel's depth from the eye is kept (DEPTH: onto a layer too, unless depth is
    False), so that his lines are drawn only where the silk does not hide them, as the scene's depth test draws them.
    Returns the silk's mask (the whole triangles')."""
    tris, sun, fill = drawn
    x0, y0, x1, y1 = box
    target = onto if onto is not None else im
    mask = Image.new('L', (W, H), 0)
    md = ImageDraw.Draw(mask)
    dd = ImageDraw.Draw(target)
    for _, poly, rgb, _, _, _ in tris:
        md.polygon(poly, fill=255)
        dd.polygon(poly, fill=rgb + ((255,) if onto is not None else ()))
    clip = Image.new('L', (W, H), 0)
    ImageDraw.Draw(clip).rectangle((x0 + 1, y0 + 22, x1 - 1, y1 - 1), fill=255)
    cx0, cy0, cx1, cy1 = x0 + 1, y0 + 22, x1 - 1, y1 - 1
    pix = np.array(target)
    L = np.array(sun)
    for _, poly, _, facings, colours, depths in tris:
        (ax, ay), (bx, by), (qx, qy) = poly
        lx, hx = max(cx0, int(math.floor(min(ax, bx, qx)))), min(cx1, int(math.ceil(max(ax, bx, qx))))
        ly, hy = max(cy0, int(math.floor(min(ay, by, qy)))), min(cy1, int(math.ceil(max(ay, by, qy))))
        den = (by - qy) * (ax - qx) + (qx - bx) * (ay - qy)
        if lx > hx or ly > hy or abs(den) < 1e-12:
            continue
        X, Y = np.meshgrid(np.arange(lx, hx + 1, dtype=float), np.arange(ly, hy + 1, dtype=float))
        w0 = ((by - qy) * (X - qx) + (qx - bx) * (Y - qy)) / den
        w1 = ((qy - ay) * (X - qx) + (ax - qx) * (Y - qy)) / den
        w2 = 1.0 - w0 - w1
        inside = (w0 >= -1e-7) & (w1 >= -1e-7) & (w2 >= -1e-7)
        if not inside.any():
            continue
        w = np.stack([w0[inside], w1[inside], w2[inside]], axis=1)
        n = w @ np.array(facings)
        n /= np.maximum(1e-12, np.linalg.norm(n, axis=1))[:, None]
        lit = n @ L
        shade = 0.22 + 0.78 * np.maximum(0.0, lit) + np.where(lit < 0.0, fill * np.minimum(1.0, -lit * 1.4), 0.0)
        rgb = (255.0 * np.clip((w @ np.array(colours)) * shade[:, None], 0.0, 1.0) ** (1 / 2.2)).astype(np.uint8)
        ys, xs = np.nonzero(inside)
        pix[ys + ly, xs + lx, 0:3] = rgb
        if depth:
            DEPTH[ys + ly, xs + lx] = w @ np.array(depths)
    target.paste(Image.fromarray(pix))
    return Image.composite(mask, Image.new('L', (W, H), 0), clip)


def extent(mask):
    """The mask's columns and rows with silk in them: per row (left, right), per column (top, bottom)."""
    px = mask.load()
    bbox = mask.getbbox()
    rows, cols = {}, {}
    if not bbox:
        return rows, cols, bbox
    for y in range(bbox[1], bbox[3]):
        xs = [x for x in range(bbox[0], bbox[2]) if px[x, y]]
        if xs:
            rows[y] = (xs[0], xs[-1])
    for x in range(bbox[0], bbox[2]):
        ys = [y for y in range(bbox[1], bbox[3]) if px[x, y]]
        if ys:
            cols[x] = (ys[0], ys[-1])
    return rows, cols, bbox


SUIT, CORD = srgb((0.30, 0.26, 0.19)), srgb((0.13, 0.12, 0.07))
# (each pixel's depth from the eye of the silk last drawn on the picture: paint's)
DEPTH = np.full((H, W), np.inf)


def cord(dd, view, a, b):
    """A length of line from a to b, a pixel wide, drawn where the silk does not hide it: the scene draws it as a rod
    12 mm round, so its near side is 12 mm nearer the eye than its middle (a review of 24 November 17:40Z, after a
    second look: drawn over everything, lines under the silk had shown through it). Within its view's panel only, as
    paint clips the silk."""
    x0, y0, x1, y1 = view.box
    u, v = view.at(a), view.at(b)
    steps = max(1, int(math.ceil(max(abs(v[0] - u[0]), abs(v[1] - u[1])) * 2.0)))
    for k in range(steps + 1):
        f = k / steps
        p = [a[i] + (b[i] - a[i]) * f for i in range(3)]
        x, y = view.at(p)
        xi, yi = int(round(x)), int(round(y))
        if x0 + 1 <= xi <= x1 - 1 and y0 + 22 <= yi <= y1 - 1 and 0 <= xi < W and 0 <= yi < H and view.depth(p) - 0.012 <= DEPTH[yi, xi]:
            dd.point((xi, yi), fill=CORD)


def man_and_lines(mesh, view, dd=None):
    """Him lying (his feet at the mesh's origin, the way he faces), and his lines from his harness to the seams, a little
    slack as the scene draws them lying (each in two lengths, its middle let down toward the lower end's height and bowed
    to one side by 0.06 of its length, every other line to the other)."""
    dd = dd or d
    hx, hy, hz, turn = mesh['h']
    t = math.radians(turn)
    fwd, side = (math.cos(t), math.sin(t)), (-math.sin(t), math.cos(t))
    feet, head = (0.0, 0.0, 0.14), (1.7 * fwd[0], 1.7 * fwd[1], 0.16)
    a, b = view.at(feet), view.at(head)
    wide = max(2, int(0.34 * view.scale / max(1.0, view.depth([0.85 * fwd[0], 0.85 * fwd[1], 0.15]))))
    dd.line((a[0], a[1], b[0], b[1]), fill=SUIT, width=wide)
    if mesh.get('l'):
        # (as the scene draws them: LandAir's LineLies points, a rod from each to the next)
        for pts in mesh['l']:
            for a, b in zip(pts[:-1], pts[1:]):
                cord(dd, view, a, b)
        return
    for s, k in zip(mesh['s'], mesh.get('k', range(len(mesh['s'])))):
        p = mesh['p'][s]
        way = [p[0] - hx, p[1] - hy, p[2] - hz]
        size = math.sqrt(sum(a * a for a in way))
        flat = math.hypot(way[0], way[1]) or 1.0
        bow = (1.0 if k % 2 == 0 else -1.0) * 0.06 * size
        mid = [0.5 * (hx + p[0]) - way[1] / flat * bow, 0.5 * (hy + p[1]) + way[0] / flat * bow, 0.5 * (hz + p[2]) - 0.5 * abs(p[2] - hz)]
        cord(dd, view, [hx, hy, hz], mid)
        cord(dd, view, mid, p)


def luma(rgb):
    return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]


printed = []


def say(box, row, words):
    d.text((box[0] + 6, box[3] - 16 - 13 * row), words, fill=INK)
    printed.append(words)


bw = (W - 40) // 4 - 10
r1 = [(20 + k * (bw + 13), ly + 22, 20 + k * (bw + 13) + bw, ly + 352) for k in range(4)]
SKY, FIELD, PALE = (196, 208, 220), (150, 160, 120), (232, 228, 206)
sun_high = [0.5, -0.6, 0.62]

# (1) side-on through a long lens (200 m off: its sides as they are, no perspective to speak of), the sun high on its
# near side; measured: its widest and its hem across, where its widest is down it
panel(r1[0], "side-on, full (200 m off, long lens), his lines to him: glow %.2f" % glow([0.0, -200.0, -1.0], [0.0, 0.0, -1.0], sun_high), SKY)
v1 = View([0.0, -200.0, -1.0], [0.0, 0.0, -1.0], 6500.0, r1[0])
m1 = paint(silk_polys(meshes[0], v1, sun_high, glow(v1.eye, [0.0, 0.0, -1.0], sun_high)), r1[0])
rows, cols, bb = extent(m1)
mpp = 200.0 / v1.scale
widths = {y: r - l + 1 for y, (l, r) in rows.items()}
most = max(widths.values())
wide_rows = [y for y, w in widths.items() if w >= most - 1]
wide_row = sum(wide_rows) / len(wide_rows)
top, bottom = min(rows), max(rows)
# (its hem seen nearly edge on: its width two rows up from the silhouette's lowest, which is only its nearest point)
hem_row = bottom - 2
hem = widths[hem_row]
# (his lines from each seam to his risers 3.3 m under its skirt, and him 4.1 m under it, his feet 1.1 m under that)
seams0 = [k for k in range(0, 4 * meshes[0]['gores'], 4)]
riser = v1.at([0.0, 0.0, -3.3])
for k in seams0:
    q = v1.at(meshes[0]['p'][k])
    d.line((q[0], q[1], riser[0], riser[1]), fill=CORD, width=1)
a, b = v1.at([0.0, 0.0, -3.4]), v1.at([0.1, 0.0, -5.2])
d.line((a[0], a[1], b[0], b[1]), fill=SUIT, width=6)
for y in (wide_row, hem_row):
    l, r = rows[int(round(y))]
    d.line((l - 12, y, l - 3, y), fill=RED)
    d.line((r + 3, y, r + 12, y), fill=RED)
side_words = ["widest %.2f m across, %.2f of its %.2f m depth down from its crown" % (most * mpp, (wide_row - top) / (bottom - top), (bottom - top + 1) * mpp),
              "its hem %.2f m across: %.2f of its widest (her 0.8 to 0.9)" % (hem * mpp, hem / most)]


# (her verdict's (c), and a review of 24 November: under its widest it had dropped straight, 0.41 to 0.66 of its depth,
# under a flat-topped crown. Its silhouette's width a share of the way down its depth from its crown, against its widest)
def width_at(share):
    return widths[int(round(top + share * (bottom - top)))] / most


side_words.append("%.3f of its widest 0.5 of its depth down, %.3f at 0.6" % (width_at(0.5), width_at(0.6)))
side_words.append("its crown %.2f of its widest 3%% down, %.2f 11%% down" % (width_at(0.03), width_at(0.11)))
say(r1[0], 3, side_words[0])
say(r1[0], 2, side_words[2])
say(r1[0], 1, side_words[3])
say(r1[0], 0, side_words[1])

# (2) and (3) from under it 100 m off (60 along, 80 up), the sun just behind it
eye, sun_behind = [3.0, -60.0, -80.0], norm([0.06, 0.62, 0.78])
g2 = glow(eye, [0.0, 0.0, 1.2], sun_behind)
panel(r1[1], "from under it 100 m off, the sun behind it: glow %.2f" % g2, PALE)
v2 = View(eye, [0.0, 0.0, 1.2], 3600.0, r1[1])
paint(silk_polys(meshes[0], v2, sun_behind, g2), r1[1])
panel(r1[2], "the same with no glow (release 1's plain silk)", PALE)
v3 = View(eye, [0.0, 0.0, 1.2], 3600.0, r1[2])
paint(silk_polys(meshes[0], v3, sun_behind, 0.0), r1[2])

# (4) falling in, across the wind 30 m off: as he landed, and a quarter, a half and three quarters fallen in (paler
# the earlier), and lying; his lines (lying) drawn where its silk lying does not hide them, not tested against the
# paler stages before it, which the scene never draws with it
panel(r1[3], "falling in, across the wind 30 m off (paler earlier)", FIELD)
v4 = View([5.5, -30.0, 2.0], [5.5, 0.0, 3.5], 1000.0, r1[3])
x0, y0, x1, y1 = r1[3]
gy = v4.at([5.5, 0.0, 0.0])[1]
d.rectangle((x0 + 1, y0 + 22, x1 - 1, int(gy)), fill=SKY)
layers = Image.new('RGBA', (W, H), (0, 0, 0, 0))
mids = []
for k, which in enumerate((3, 4, 5, 6, 1)):
    layer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    paint(silk_polys(meshes[which], v4, sun_high, 0.0), r1[3], onto=layer, depth=which == 1)
    alpha = layer.getchannel('A').point(lambda a, k=k: int(a * (0.35 + 0.65 * k / 4.0)))
    layer.putalpha(alpha)
    layers = Image.alpha_composite(layers, layer)
    pts = meshes[which]['p']
    mids.append(sum(p[0] for p in pts) / len(pts))
clipped = Image.new('RGBA', (W, H), (0, 0, 0, 0))
clipped.paste(layers.crop((x0 + 1, y0 + 22, x1 - 1, y1 - 1)), (x0 + 1, y0 + 22))
im.paste(clipped, (0, 0), clipped)
d = ImageDraw.Draw(im)
man_and_lines(meshes[1], v4)
say(r1[3], 0, "its middle %s m downwind of his feet" % ', '.join('%.1f' % m for m in mids))

# Lying on the field (her verdict, (b)).
ly2 = ly + 372
wide_box = (20, ly2, 20 + 2 * bw + 13, ly2 + 360)
r2 = [wide_box, (20 + 2 * (bw + 13), ly2, 20 + 2 * (bw + 13) + bw, ly2 + 360), (20 + 3 * (bw + 13), ly2, 20 + 3 * (bw + 13) + bw, ly2 + 360)]
lie = meshes[1]
P = lie['p']
half = len(P) // 2
xs, ys = [p[0] for p in P[:half]], [p[1] for p in P[:half]]
mid_x, mid_y = (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0

# (5) from 40 m at a man's eye height (1.7 m), across the wind, the sun high on the near side
panel(wide_box, "lying, from 40 m at a man's eye height (1.7 m), across the wind (its far end to the right)", FIELD)
look_x = (min(xs) + max(xs) + min(0.0, min(xs))) / 2.0 - 0.6
v5 = View([look_x, mid_y - 40.0, 1.7], [look_x, mid_y, 0.3], 2900.0, wide_box)
x0, y0, x1, y1 = wide_box
horizon = v5.cy - math.tan(math.atan2(1.4, 40.0)) * v5.scale
d.rectangle((x0 + 1, y0 + 22, x1 - 1, max(y0 + 22, int(horizon))), fill=SKY)
tris5 = silk_polys(lie, v5, sun_high, 0.0)
m5 = paint(tris5, wide_box)
bare5 = im.copy()   # (its silk's own pixels, before his lines are drawn over it)
man_and_lines(lie, v5)
rows, cols, bb = extent(m5)
left, right = min(cols), max(cols)
span = right - left
ground_row = v5.at([look_x, mid_y, 0.0])[1]
far_cols = [x for x in cols if x >= left + 0.75 * span]
near_cols = [x for x in cols if x <= left + 0.25 * span]
far_up = (ground_row - min(cols[x][0] for x in far_cols)) * 40.0 / v5.scale
near_up = (ground_row - min(cols[x][0] for x in near_cols)) * 40.0 / v5.scale
long5 = span * 40.0 / v5.scale
pix = bare5.load()
silk_px = [pix[x, y] for x in range(bb[0], bb[2]) for y in range(bb[1], bb[3]) if m5.getpixel((x, y))]
lum = sorted(luma(c) for c in silk_px)
say(wide_box, 3, "%.1f m long along the wind; its far quarter stands %.2f m up at most (her 0.3 to 0.8), its near quarter %.2f "
    "(heights at 40 m, within 4%% for its depth)" % (long5, far_up, near_up))


# (a review of 24 November: its far end had come to a beak 0.23 m off the ground. Its top over its last twentieth, at
# its far end: how high at 90, 95 and 98 hundredths of its length and at its tip, and how much it falls over the first
# and second halves of its last twentieth: falling faster toward its tip, it comes round and down)
def top_at(share):
    x = min(cols, key=lambda c: abs(c - (left + share * span)))
    return (ground_row - cols[x][0]) * 40.0 / v5.scale


ends = [top_at(f) for f in (0.90, 0.95, 0.975, 0.98, 1.0)]
say(wide_box, 2, "its far end's top %.2f m up 0.9 along it, %.2f at 0.95, %.2f at 0.98 and %.2f at its tip: over its last twentieth "
    "falling %.2f m, then %.2f" % (ends[0], ends[1], ends[3], ends[4], ends[1] - ends[2], ends[2] - ends[4]))
say(wide_box, 1, "its silk here sRGB luma %.0f to %.0f (a tenth and nine tenths), median %.0f; his lines and him at its near end (left)" % (
    lum[len(lum) // 10], lum[9 * len(lum) // 10], lum[len(lum) // 2]))


# (6) and (7) from above (150 m straight up, the wind to the right), his and the second pilot's
def from_above(mesh, box, who):
    P = mesh['p']
    half = len(P) // 2
    xs, ys = [p[0] for p in P[:half]], [p[1] for p in P[:half]]
    cx, cy = (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0
    panel(box, "lying, from above, %s (wind to the right)" % who, FIELD)
    v = View([cx, cy, 150.0], [cx, cy, 0.0], 5000.0, box, hint=(0.0, 1.0, 0.0))
    m = paint(silk_polys(mesh, v, sun_high, 0.0), box)
    bare = im.copy()   # (its silk's own pixels, before his lines are drawn over it: a review of 24 November 17:40Z)
    man_and_lines(mesh, v)
    rows, cols, bb = extent(m)
    mpp = 150.0 / v.scale
    along = (max(cols) - min(cols) + 1) * mpp
    across = (max(rows) - min(rows) + 1) * mpp
    pix = bare.load()
    # (its folds: luma across it at its middle, from one edge to the other: ridges lit, troughs and their far sides darker)
    mx = int((min(cols) + max(cols)) / 2)
    raw = [luma(pix[mx, y]) for y in range(cols[mx][0] + 2, cols[mx][1] - 1)]
    line = [sum(raw[max(0, k - 2):k + 3]) / len(raw[max(0, k - 2):k + 3]) for k in range(len(raw))]
    # (its light ridges: rises of 10 or more of luma over the darker on either side, smoothed over 5 pixels)
    peaks = []
    for k in range(1, len(line) - 1):
        if line[k] < line[k - 1] or line[k] <= line[k + 1]:
            continue
        lo_l = min(line[:k]) if k else line[k]
        lo_r = min(line[k + 1:])
        j = k
        while j > 0 and line[j - 1] <= line[j]:
            j -= 1
        lo_l = line[j]
        j = k
        while j + 1 < len(line) and line[j + 1] <= line[j]:
            j += 1
        lo_r = line[j]
        if line[k] - max(lo_l, lo_r) >= 10.0:
            peaks.append(k)
    say(box, 2, "%.1f m along the wind by %.1f across: %.2f to 1 (her 2 to 3)" % (along, across, along / across))
    say(box, 1, "across its middle sRGB luma %.0f to %.0f, %d light ridges" % (min(line), max(line), len(peaks)))
    return along / across


panel_ratio = [from_above(meshes[1], r2[1], "his"), from_above(meshes[2], r2[2], "the second pilot's")]
im.save(DST)
for words in printed:
    print('measured:', words)
print('wrote', DST)
