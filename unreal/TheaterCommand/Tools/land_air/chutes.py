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

Its last row: his canopy as the scene draws it (land_air_test's chute_silk.txt, LandAir's own mesh), shaded much as
the pieces' paint is (the sun's light, and on the side turned from the sun the share of its own colour the scene gives
back, its glow): side-on in the sun, from under it 100 m off with the sun behind it with its glow and without, and
fallen in on the field.
Usage: chutes.py <air_paths.txt> <chute_silk.txt> <out.png>"""
import math
import sys
from collections import defaultdict

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
        cur = meshes.setdefault(int(w[1]), {'p': [], 'n': [], 'c': [], 't': [], 'gores': int(w[2])})
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
W, H = 1640, 60 + ROW * len(keys) + 330
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
       "both its faces; shaded here much as the pieces' paint is)" % meshes[0]['gores'], fill=INK)


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


def render(mesh, eye, at, sun, fill, box, scale, title, ground=None):
    x0, y0, x1, y1 = box
    if ground:
        d.rectangle((x0 + 1, y0 + 1, x1 - 1, y1 - 1), fill=ground)
    d.rectangle(box, outline=GREY)
    fwd = norm([a - b for a, b in zip(at, eye)])
    right = norm(cross(fwd, [0.0, 0.0, 1.0]))
    up = cross(right, fwd)
    cxm, cym = (x0 + x1) / 2.0, (y0 + y1) / 2.0 + 10
    L = norm(sun)
    tris = []
    P = mesh['p']
    for t in mesh['t']:
        a, b, c = (P[i] for i in t)
        mid = [(a[k] + b[k] + c[k]) / 3.0 for k in range(3)]
        n = norm([sum(mesh['n'][i][k] for i in t) for k in range(3)])
        if dot(n, [e - m for e, m in zip(eye, mid)]) <= 0.0:
            continue   # (the face of the silk turned from the eye: its other face, on the other side, is drawn)
        col = [sum(mesh['c'][i][k] for i in t) / 3.0 for k in range(3)]
        lit = dot(n, L)
        shade = 0.22 + 0.78 * max(0.0, lit) + (fill * min(1.0, -lit * 1.4) if lit < 0.0 else 0.0)
        rgb = tuple(int(255 * min(1.0, max(0.0, col[k] * shade)) ** (1 / 2.2)) for k in range(3))
        poly = []
        for v in (a, b, c):
            rel = [v[k] - eye[k] for k in range(3)]
            z = dot(rel, fwd)
            poly.append((cxm + dot(rel, right) / z * scale, cym - dot(rel, up) / z * scale))
        tris.append((dot([m - e for m, e in zip(mid, eye)], fwd), poly, rgb))
    tris.sort(key=lambda e: -e[0])
    for _, poly, rgb in tris:
        d.polygon(poly, fill=rgb)
    d.text((x0 + 6, y0 + 4), title, fill=INK)


bw = (W - 40) // 4 - 10
boxes = [(20 + k * (bw + 13), ly + 22, 20 + k * (bw + 13) + bw, ly + 312) for k in range(4)]
SKY, FIELD = (196, 208, 220), (150, 160, 120)
# (1) side-on, the sun high to one side
side_eye, sun_high = [0.0, -40.0, 1.2], [0.5, -0.6, 0.62]
g1 = glow(side_eye, [0.0, 0.0, 1.2], sun_high)
render(meshes[0], side_eye, [0.0, 0.0, 1.2], sun_high, g1, boxes[0], 1400.0, "side-on, the sun high on its near side: glow %.2f" % g1, SKY)
# (2) and (3) from under it 100 m off (60 along, 80 up), the sun just behind it
eye, sun_behind = [3.0, -60.0, -80.0], norm([0.06, 0.62, 0.78])
g2 = glow(eye, [0.0, 0.0, 1.2], sun_behind)
render(meshes[0], eye, [0.0, 0.0, 1.2], sun_behind, g2, boxes[1], 3600.0, "from under it 100 m off, the sun behind it: glow %.2f" % g2, (232, 228, 206))
render(meshes[0], eye, [0.0, 0.0, 1.2], sun_behind, 0.0, boxes[2], 3600.0, "the same with no glow (release 1's plain silk)", (232, 228, 206))
# (4) fallen in on the field, from 12 m off and 6 up
render(meshes[1], [-6.0, -10.0, 6.0], [4.0, 0.0, 0.2], sun_high, 0.0, boxes[3], 520.0, "fallen in on the field, 4 m downwind of him", FIELD)
im.save(DST)
print('wrote', DST)
