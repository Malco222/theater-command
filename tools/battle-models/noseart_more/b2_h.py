"""Batch 2, numbers 74 to 80 (painter b2_h): a wire-haired terrier in a flight jacket holding up his map, a girl in a
wide sun hat and a long dress holding up a magnolia bloom, a carnival barker pointing his cane, a tin washtub with its
suds, a toy boat and a scrubbing brush, a B-17 close up with her main wheel on its toes at the square end of a short
runway, a crew chief polishing his bomber's nose and a prickly pear tipping its cowboy hat. Each picture is drawn from
flat shapes with noseart's Drawing API and the shared figure kit; the helpers below (a terrier, a folded chart, a
magnolia, a sun hat, a cane, a washtub's rim, a toy boat, a bath brush, a tiptoe tyre, a B-17's nose close up in an
oval and in a round frame, a prickly pear's pads and spines, a cowboy hat) are built the same way, for these
paintings."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402


# ---- helpers ------------------------------------------------------------------------------------------------------------
def _frame(x, y, k=1.0, s=1, turn=0.0):
    """A frame of one's own on the grid: P(u, v), u toward the side it faces (s), v up, k grid units to one of its own,
    the whole turned `turn` degrees anticlockwise about (x, y)."""
    c, sn = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        u, v = s * u * k, v * k
        return (x + u * c - v * sn, y + u * sn + v * c)
    return P


def _tufts(pts, depth, n):
    """A wiry edge along the open polyline `pts`: n tufts, every other point pushed `depth` to the right of the way it
    runs (outward, along a counter-clockwise outline)."""
    acc = [0.0]
    for i in range(1, len(pts)):
        acc.append(acc[-1] + math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]))
    total = acc[-1] or 1.0
    out = []
    m = 2 * n
    for j in range(m + 1):
        t = total * j / m
        i = next((q for q in range(1, len(pts)) if acc[q] >= t - 1e-9), len(pts) - 1)
        f = (t - acc[i - 1]) / ((acc[i] - acc[i - 1]) or 1.0)
        (ax, ay), (bx, by) = pts[i - 1], pts[i]
        px, py = ax + (bx - ax) * f, ay + (by - ay) * f
        tl = math.hypot(bx - ax, by - ay) or 1.0
        tx, ty = (bx - ax) / tl, (by - ay) / tl
        if j % 2:
            px, py = px + ty * depth, py - tx * depth
        out.append((px, py))
    return out


def _run(p, steps=4):
    return smooth(p, False, steps) if len(p) > 2 else [tuple(q) for q in p]


def _edge(parts):
    """An outline (in a frame of one's own) put together from runs, each starting where the last ended: ("s", pts) a
    smooth run, ("w", pts, depth, n) a wiry one (n tufts `depth` deep, outward whichever way round the outline goes)."""
    raw = []
    for p in parts:
        raw += _run(p[1])
    sign = 1.0 if area(raw) > 0 else -1.0
    out = []
    for p in parts:
        seg = _run(p[1]) if p[0] == "s" else _tufts(_run(p[1], 3), p[2] * sign, p[3])
        if out and math.hypot(seg[0][0] - out[-1][0], seg[0][1] - out[-1][1]) < 1e-6:
            seg = seg[1:]
        out += seg
    if len(out) > 2 and math.hypot(out[0][0] - out[-1][0], out[0][1] - out[-1][1]) < 1e-6:
        out.pop()
    return out


def _lady(d, x, y, h, s=1, extra_arms=None, **kw):
    """lady(), with arms of our own (name: (elbow, hand), as noseart's ARMS gives them) lent to her for this one call."""
    added = [k for k in (extra_arms or {}) if k not in ARMS]
    for k in added:
        ARMS[k] = extra_arms[k]
    try:
        return lady(d, x, y, h, s, **kw)
    finally:
        for k in added:
            ARMS.pop(k, None)


def _clip_disc(poly, cx, cy, r, step=0.4):
    """The polygon with every point outside the circle (cx, cy, r) pulled in onto it: a shape cut by the circle's edge
    (for a shape that crosses it once, as a fuselage runs out of a round frame)."""
    out = []
    for i in range(len(poly)):
        (ax, ay), (bx, by) = poly[i], poly[(i + 1) % len(poly)]
        m = max(1, int(math.hypot(bx - ax, by - ay) / step))
        for j in range(m):
            px, py = ax + (bx - ax) * j / m, ay + (by - ay) * j / m
            dd = math.hypot(px - cx, py - cy)
            if dd > r:
                px, py = cx + (px - cx) * r / dd, cy + (py - cy) * r / dd
            out.append((px, py))
    return out


# ---- 74: a wire-haired terrier in a flight jacket, and his map ----------------------------------------------------------
def chart(d, x, y, w, h, turn=0.0, paper="art_white", fold="art_lbrown", route="art_red", panels=3):
    """A folded paper chart, its middle at (x, y), w by h, turned `turn` degrees: folded as a concertina in `panels`
    panels, every other one in its fold's shade; one bold route across it, ending in a round dot (the place he is
    bound for). Returns its frame, P(u, v) from its middle."""
    c, sn = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        return (x + u * c - v * sn, y + u * sn + v * c)
    fine = d.ink * 0.7
    b = h * 0.08
    xs = [-w / 2 + w * i / panels for i in range(panels + 1)]
    lift = [b if i % 2 else 0.0 for i in range(panels + 1)]
    for i in range(panels):
        quad = [(xs[i], -h / 2 + lift[i]), (xs[i + 1], -h / 2 + lift[i + 1]), (xs[i + 1], h / 2 + lift[i + 1]), (xs[i], h / 2 + lift[i])]
        d.shape(paper if i % 2 == 0 else fold, [P(*q) for q in quad], ink=fine)
    d.line(route, [P(-w * 0.36, -h * 0.02), P(-w * 0.2, h * 0.26), P(w * 0.0, h * 0.12), P(w * 0.18, h * 0.3)], h * 0.1)
    d.oval(route, *P(w * 0.3, h * 0.28), h * 0.12, h * 0.12, ink=fine)
    return P


def terrier(d, x, y, size, coat="art_white", far="art_white", ear="art_auburn", jacket="art_lbrown", knit="art_hair",
            nose="art_black", paper="art_white", fold="art_lbrown", route="art_red", nod=-12.0, lean=0.92):
    """A wire-haired terrier standing up on his hind legs, `size` from his feet at (x, y) to the top of his head, facing
    right: his long flat head with its square muzzle and wiry beard, a bushy brow, a folded ear, bowed `nod` degrees
    toward his chart; in a pilot's leather jacket (its knit cuffs and waistband, its collar, its zip), his far arm
    out of sight behind him, his near arm holding his folded chart up before his chest, his paw on its middle panel.
    `lean` is his build across (below 1, a wiry terrier's narrow one)."""
    k = size / 50.0

    def P(u, v):
        return (x + u * k * lean, y + v * k)
    pv = (0.6, 34.4)                                                   # (his neck, which his head bows about)
    cn, sn = math.cos(math.radians(nod)), math.sin(math.radians(nod))

    def Ph(u, v):
        du, dv = u - pv[0], v - pv[1]
        return P(pv[0] + du * cn - dv * sn, pv[1] + du * sn + dv * cn)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    def QH(pts):
        return [Ph(u, v) for u, v in pts]
    w0, fine = d.ink, d.ink * 0.7
    # (his docked tail, cocked up behind him from under his jacket)
    d.shape(coat, Q(_edge([("s", [(-5.2, 17.6), (-7.0, 19.6), (-8.4, 21.8)]),
                           ("w", [(-8.4, 21.8), (-9.6, 23.4), (-11.4, 22.4), (-10.8, 19.4)], 0.55, 4),
                           ("s", [(-10.8, 19.4), (-8.6, 15.8), (-5.8, 14.8)])])), ink=w0)
    # (his legs, apart; wiry down their backs; big paws)
    for dx, paint in ((-8.0, far), (0.0, coat)):
        leg = _edge([("s", [(1.0 + dx, 2.4), (4.6 + dx, 2.4)]), ("s", [(4.6 + dx, 2.4), (4.8 + dx, 8.0), (4.6 + dx, 14.6)]),
                     ("s", [(4.6 + dx, 14.6), (-0.8 + dx, 14.6)]), ("w", [(-0.8 + dx, 14.6), (-0.2 + dx, 8.0), (1.0 + dx, 2.4)], 0.6, 4)])
        d.shape(paint, Q(leg), ink=w0)
        d.fill(paint, Q([(0.4 + dx, 2.6), (3.6 + dx, 3.3), (6.6 + dx, 2.6), (7.6 + dx, 1.0), (6.4 + dx, 0.0), (1.0 + dx, 0.0), (0.0 + dx, 1.0)]), ink=fine, steps=3)
        for tx in (5.0, 6.3):
            d.line("art_ink", Q([(tx + dx, 1.9), (tx + 0.25 + dx, 0.5)]), 0.32 * k)
    # (his jacket: its body, waistband, zip, a pocket's flap; his back one clean line, his far arm out of sight)
    body = _edge([("s", [(-6.4, 12.4), (7.0, 12.4)]),
                  ("s", [(7.0, 12.4), (8.2, 16.8), (8.6, 22.6), (8.0, 28.2), (6.2, 32.3), (3.2, 34.1), (-1.0, 34.5), (-4.6, 33.6),
                         (-6.8, 30.4), (-7.3, 24.0), (-7.1, 17.0), (-6.4, 12.4)])])
    d.shape(jacket, Q(body), ink=w0)
    d.shape(knit, Q([(-6.7, 11.4), (7.3, 11.4), (7.7, 14.4), (-7.1, 14.4)]), ink=fine)
    d.line("art_ink", Q([(5.0, 32.6), (5.5, 24.0), (5.3, 14.6)]), 0.34 * k)
    d.shape(jacket, Q([(-3.4, 22.8), (1.2, 23.0), (1.0, 21.0), (-1.2, 20.3), (-3.2, 21.0)]), ink=fine)
    d.shape(jacket, Q([(-5.6, 32.6), (-5.0, 35.8), (0.8, 36.2), (1.8, 33.6)]), ink=fine)
    # (his head, side on, bowed to his chart: a long flat skull, a long square muzzle, a wiry beard; his folded ear,
    #  his brow, eye and nose)
    head = _edge([("s", [(-4.2, 33.0), (1.2, 33.0)]),
                  ("s", [(1.2, 33.0), (1.6, 35.4), (3.0, 37.8), (7.6, 39.2)]),
                  ("w", [(7.6, 39.2), (8.6, 36.6), (12.6, 36.0), (15.6, 36.8), (16.6, 39.4), (16.9, 42.2)], 0.75, 7),
                  ("s", [(16.9, 42.2), (17.3, 44.0), (17.0, 45.8), (16.2, 46.7), (13.0, 47.4), (8.0, 48.2), (2.0, 48.8), (-2.6, 48.6)]),
                  ("w", [(-2.6, 48.6), (-5.4, 47.6), (-6.4, 44.0), (-6.3, 40.0)], 0.6, 4),
                  ("s", [(-6.3, 40.0), (-5.0, 34.6), (-4.2, 33.0)])])
    d.shape(coat, QH(head), ink=w0)
    d.fill(ear, QH([(-2.0, 46.6), (0.4, 47.6), (3.0, 47.6), (4.6, 46.4), (5.6, 43.8), (5.4, 41.6), (4.2, 42.0), (2.0, 43.6),
                    (-0.4, 45.0)]), ink=fine, steps=3)
    d.line("art_ink", QH([(0.6, 46.6), (2.8, 46.7), (4.0, 45.8)]), 0.3 * k)
    d.shape(coat, QH(_edge([("s", [(5.8, 45.5), (8.0, 45.8), (10.6, 45.7), (13.0, 45.1)]),
                            ("w", [(13.0, 45.1), (12.0, 47.0), (8.8, 47.6), (6.2, 46.9), (5.8, 45.5)], 0.5, 4)])), ink=fine)
    d.oval("art_ink", *Ph(9.0, 44.3), 0.85 * k, 1.05 * k, turn=nod, ink=0.0)
    d.oval("art_white", *Ph(9.25, 44.7), 0.3 * k, 0.3 * k, ink=0.0)
    d.oval(nose, *Ph(16.3, 45.3), 1.55 * k, 1.3 * k, turn=nod, ink=fine)
    d.line("art_ink", QH([(16.7, 41.9), (14.6, 41.4), (12.6, 41.9)]), 0.4 * k)
    d.shape(jacket, Q([(-0.4, 35.4), (6.4, 33.3), (5.0, 29.8), (2.0, 31.1)]), ink=fine)          # (his collar, before his neck)
    # (his chart, held up before his chest; his near arm before it, his paw on its middle panel)
    chart(d, *P(14.4, 24.6), 13.5 * k * lean, 10.0 * k, turn=5.0, paper=paper, fold=fold, route=route)
    S, E, H = (3.6, 30.4), (6.4, 20.6), (12.4, 21.3)
    d.limb(jacket, Q([S, lerp(S, E, 0.5), E, lerp(E, H, 0.5), H]), [2.8 * k, 2.6 * k, 2.4 * k, 2.3 * k, 2.2 * k], ink=w0)
    d.limb(knit, Q([lerp(E, H, 0.72), H]), [2.4 * k, 2.35 * k], ink=fine)
    ang = math.degrees(math.atan2(H[1] - E[1], H[0] - E[0]))
    hx, hy = H[0] + (H[0] - E[0]) * 0.34, H[1] + (H[1] - E[1]) * 0.34
    d.oval(coat, *P(hx, hy), 1.95 * k, 1.75 * k, turn=ang, ink=fine)
    d.oval(coat, *P(hx + 0.4, hy + 1.3), 0.95 * k, 0.75 * k, turn=ang + 50, ink=fine * 0.8)
    return P


# ---- 75: a magnolia, and a wide sun hat ---------------------------------------------------------------------------------
def _petal(P, ang, ln, wd, lift=0.0):
    """A magnolia petal's outline (in frame P): from the bloom's middle out `ln` toward `ang` degrees, `wd` across at its
    widest (near its round tip)."""
    c, s_ = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    prof = [(0.0, -0.16), (0.3, -0.42), (0.68, -0.5), (0.92, -0.32), (1.0, 0.0), (0.92, 0.32), (0.68, 0.5), (0.3, 0.42), (0.0, 0.16)]
    return [P(u * ln * c - v * wd * s_, u * ln * s_ + v * wd * c + lift * u) for u, v in prof]


def magnolia(d, x, y, r, turn=0.0, petal="art_white", back="art_cream", heart="art_yellow", leaf="art_green", rib="art_lgreen"):
    """A southern magnolia's bloom seen a little from the side, its middle at (x, y), `r` its petals' reach: two glossy
    leaves behind it, three open petals behind, two cupped before, its yellow heart between."""
    P = _frame(x, y, r / 10.0, 1, turn)
    k = r / 10.0
    fine = d.ink * 0.7
    for ang, ln in ((206.0, 15.5), (334.0, 14.5)):                  # (two leaves, long and pointed, out behind it)
        c, s_ = math.cos(math.radians(ang)), math.sin(math.radians(ang))
        prof = [(0.0, 0.0), (0.25, -0.2), (0.6, -0.24), (0.88, -0.12), (1.0, 0.0), (0.88, 0.12), (0.6, 0.24), (0.25, 0.2)]
        d.fill(leaf, [P(u * ln * c - v * ln * s_, u * ln * s_ + v * ln * c) for u, v in prof], ink=fine, steps=4)
        d.line(rib, [P(1.5 * c, 1.5 * s_), P(ln * 0.55 * c, ln * 0.55 * s_), P(ln * 0.92 * c, ln * 0.92 * s_)], 0.35 * k)
    for ang in (160.0, 92.0, 22.0):                                 # (three petals open behind)
        d.fill(back, _petal(P, ang, 10.0, 7.0), ink=fine, steps=4)
    d.oval(heart, *P(0.0, 2.6), 1.6 * k, 2.4 * k, ink=fine)          # (its heart, a cone)
    d.line("art_lgreen", [P(-0.6, 4.2), P(0.0, 4.9), P(0.6, 4.2)], 0.5 * k)
    for ang, sx in ((212.0, -1), (328.0, 1)):                       # (two petals cupped before it)
        pts = _petal(P, ang + 0.0, 9.5, 8.2)
        d.fill(petal, pts, ink=fine, steps=4)
        d.line(back, [P(sx * 2.2, -1.6), P(sx * 4.6, -3.4), P(sx * 6.6, -4.2)], 0.4 * k)


def sun_hat(d, F, paint="art_yellow", band="art_blue", tilt=7.0, brim=1.62):
    """A wide straw sun hat on lady()'s head (F her frame), its brim `brim` heads to either side, tilted `tilt` degrees up
    on the side she faces; a ribbon round its crown, tied in a little bow behind."""
    c = F.P(0.03, 7.5)
    H = _frame(c[0], c[1], F.k, F.s, tilt * F.s)

    def Q(pts):
        return [H(u, v) for u, v in pts]
    fine = d.ink * 0.7
    d.shape(paint, Q(ellipse(0.0, 0.0, brim, 0.27, 0, 40)), ink=d.ink)
    d.line("art_gold", Q(ellipse(0.0, 0.02, brim * 0.8, 0.17, 0, 18, 200, 340)), F.r(0.035))
    d.fill(paint, Q([(-0.52, 0.04), (-0.54, 0.38), (-0.36, 0.6), (0.0, 0.66), (0.36, 0.6), (0.54, 0.38), (0.53, 0.04)]), ink=fine, steps=4)
    d.shape(band, Q([(-0.535, 0.03), (0.535, 0.03), (0.54, 0.2), (-0.545, 0.2)]), ink=fine * 0.8)
    kx, ky = -0.54, 0.115                                                   # (a little bow on its band, behind)
    for pts in ([(kx, ky), (kx - 0.16, ky + 0.25), (kx - 0.33, ky + 0.2), (kx - 0.3, ky + 0.04)],
                [(kx, ky), (kx - 0.3, ky - 0.02), (kx - 0.35, ky - 0.17), (kx - 0.17, ky - 0.2)]):
        d.fill(band, Q(pts), ink=fine * 0.8, steps=3)
    d.oval(band, *H(kx, ky), F.r(0.055), F.r(0.055), ink=fine * 0.8)
    return H


# ---- 76: a barker's cane ----------------------------------------------------------------------------------------------
def cane(d, hand, ang, back=4.0, long=20.0, w=1.3, paint="art_black", tip="art_white", hook=2.2):
    """A walking cane held at `hand`, its shaft pointing `ang` degrees: `back` behind the hand to its tipped end, `long`
    before it to its crook, which curls over toward the side it points."""
    hx, hy = hand
    ux, uy = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    nx, ny = uy, -ux                                                  # (to its right: the crook curls over that way)
    a = (hx - ux * back, hy - uy * back)
    t = (hx + ux * long, hy + uy * long)
    cx, cy = t[0] + nx * hook, t[1] + ny * hook
    a0 = math.degrees(math.atan2(t[1] - cy, t[0] - cx))
    crook = [(cx + hook * math.cos(math.radians(a0 - 200.0 * j / 10)), cy + hook * math.sin(math.radians(a0 - 200.0 * j / 10))) for j in range(11)]
    d.line(paint, [a, lerp(a, t, 0.5), t] + crook[1:], w, taper=False, steps=3)
    d.line(tip, [a, (a[0] + ux * w * 1.2, a[1] + uy * w * 1.2)], w * 1.05, taper=False)


# ---- 77: a washtub, a toy boat, a bath brush ----------------------------------------------------------------------------
def _ring_part(cx, cy, rx, ry, t, a0, a1, n=16):
    """A piece of an elliptical ring `t` thick (inward), from a0 to a1 degrees."""
    return ellipse(cx, cy, rx, ry, 0, n, a0, a1) + list(reversed(ellipse(cx, cy, rx - t, ry - t * ry / rx, 0, n, a0, a1)))


def toy_boat(d, x, y, long, hull="art_white", sail="art_white", mast="art_brown", flag="art_red", stripe="art_red"):
    """A toy sailboat afloat, its hull's middle at the waterline (x, y), `long` from bow to stern: a mainsail and a jib,
    a flag at its masthead, a stripe round its hull."""
    k = long / 12.0
    fine = d.ink * 0.7
    mx = x - 0.6 * k
    d.line(mast, [(mx, y + 2.6 * k), (mx, y + 15.6 * k)], 0.6 * k, taper=False)
    d.shape(sail, [(mx + 0.35 * k, y + 15.0 * k), (mx + 0.35 * k, y + 4.0 * k), (mx + 7.0 * k, y + 4.2 * k)], ink=d.ink)
    d.shape(sail, [(mx - 0.35 * k, y + 13.6 * k), (mx - 0.35 * k, y + 4.6 * k), (mx - 4.6 * k, y + 4.8 * k)], ink=d.ink)
    d.shape(flag, [(mx, y + 15.8 * k), (mx + 3.0 * k, y + 15.0 * k), (mx, y + 14.2 * k)], ink=fine)
    d.shape(hull, [(x - 6.4 * k, y + 3.0 * k), (x + 6.6 * k, y + 3.0 * k), (x + 4.6 * k, y - 0.4 * k), (x - 4.8 * k, y - 0.4 * k)], ink=d.ink)
    d.line(stripe, [(x - 5.5 * k, y + 2.0 * k), (x + 5.7 * k, y + 2.0 * k)], 0.7 * k, taper=False)


def bath_brush(d, foot, top, wide=6.0, wood="art_lbrown", bristle="art_cream", grain="art_tan"):
    """A long-handled bath brush from its handle's `foot` to its head's `top`: a stout round-ended handle, a big oval
    wooden head (half as wide as it is long) and a thick fringe of bristles standing out along its fore side."""
    (x0, y0), (x1, y1) = foot, top
    L = math.hypot(x1 - x0, y1 - y0)
    ux, uy = (x1 - x0) / L, (y1 - y0) / L
    nx, ny = uy, -ux                                                  # (its bristles' side: to the right of the way up it)
    hl = wide * 2.0                                                   # (the head's length)
    hb = (x1 - ux * hl, y1 - uy * hl)
    d.limb(wood, [foot, lerp(foot, hb, 0.5), (hb[0] + ux * 1.2, hb[1] + uy * 1.2)], [wide * 0.2, wide * 0.19, wide * 0.24])
    d.oval(wood, foot[0] - ux * 0.2, foot[1] - uy * 0.2, wide * 0.27, wide * 0.27)

    def at(a, b):
        return (hb[0] + ux * a + nx * b, hb[1] + uy * a + ny * b)
    br = _edge([("s", [(hl * 0.08, wide * 0.25), (hl * 0.92, wide * 0.25)]),
                ("w", [(hl * 0.92, wide * 0.25), (hl * 0.95, wide * 0.6), (hl * 0.86, wide * 0.92), (hl * 0.14, wide * 0.92),
                       (hl * 0.05, wide * 0.6), (hl * 0.08, wide * 0.25)], wide * 0.11, 9)])
    d.shape(bristle, [at(a, b) for a, b in br], ink=d.ink * 0.8)
    head = [(0.0, -wide * 0.06), (hl * 0.1, -wide * 0.34), (hl * 0.5, -wide * 0.42), (hl * 0.9, -wide * 0.32), (hl, 0.0),
            (hl * 0.92, wide * 0.36), (hl * 0.5, wide * 0.42), (hl * 0.08, wide * 0.36)]
    d.fill(wood, [at(a, b) for a, b in head], steps=4)
    d.line(grain, [at(hl * 0.2, -wide * 0.12), at(hl * 0.55, -wide * 0.18), at(hl * 0.84, -wide * 0.1)], wide * 0.09)


# ---- 78: a B-17 side on, and its runway's end ------------------------------------------------------------------------------
B17 = {   # (a B-17's side, in feet: x back from its nose, y up from its centre line; its main wheel a size up, for the cartoon)
    "hull": [(0.0, 0.3), (0.5, 1.5), (1.8, 2.5), (4.0, 3.1), (8.0, 3.4), (12.0, 3.6), (14.4, 3.8), (16.2, 5.0), (18.0, 5.5),
             (21.0, 5.6), (30.0, 5.4), (42.0, 5.0), (49.0, 4.9), (54.0, 5.6), (58.0, 7.4), (61.5, 10.8), (64.5, 14.4),
             (66.6, 16.0), (68.8, 16.5), (70.8, 15.9), (71.9, 13.8), (72.6, 9.4), (73.3, 5.0), (74.3, 2.6), (74.3, -0.2),
             (72.8, -1.0), (66.0, -1.9), (58.0, -2.9), (50.0, -3.8), (40.0, -4.4), (28.0, -4.6), (18.0, -4.4), (10.0, -3.8),
             (5.0, -3.0), (2.0, -2.0), (0.5, -1.0)],
    "glass": [(0.0, 0.3), (0.5, 1.5), (1.8, 2.5), (4.0, 3.1), (5.8, 3.25), (6.2, -2.85), (5.0, -3.0), (2.0, -2.0), (0.5, -1.0)],
    "wind": [(14.7, 3.85), (16.2, 5.0), (18.0, 5.45), (20.6, 5.5), (20.6, 3.95)],
    "tailplane": [(58.5, 0.5), (61.5, 1.3), (67.0, 1.6), (73.8, 1.0), (73.6, 0.2), (67.0, -0.3), (61.5, -0.2)],
    "wing": [(21.0, -2.0), (22.4, -0.9), (26.0, -0.4), (32.0, -0.3), (38.0, -0.6), (41.4, -1.2), (38.0, -2.0), (32.0, -2.6),
             (26.0, -3.1), (22.4, -3.0)],
    "nacelle": [(12.2, -0.5), (13.6, -0.2), (20.0, -0.1), (27.0, -0.4), (32.0, -1.2), (33.5, -2.2), (31.0, -3.4), (25.0, -4.5),
                (17.0, -5.0), (13.4, -4.9), (12.2, -4.6)],
    "axle": (23.0, -7.8), "tyre": 2.8, "ground": -10.6,
}


def _glass():
    """A B-17's glazed nose (in its own feet): round in front, cut straight behind."""
    return smooth([(5.0, -3.0), (2.0, -2.0), (0.5, -1.0), (0.0, 0.3), (0.5, 1.5), (1.8, 2.5), (4.0, 3.1), (5.8, 3.25)], False, 4) + [(6.2, -2.85)]


def _bomber_frame(X, Y, k, s=1, tilt=0.0):
    """A bomber's own frame onto the grid: (x feet back from its nose, y feet up from its centre line), its nose at
    (X, Y), facing s, `k` grid units to the foot, its nose turned `tilt` degrees up."""
    c, sn = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))

    def B(xf, yf):
        u, v = -xf * k, yf * k
        return (X + s * (u * c - v * sn), Y + u * sn + v * c)
    return B


def _clip_oval(poly, cx, cy, rx, ry, step=0.3):
    """The polygon with every point outside the ellipse (cx, cy, rx, ry) pulled in onto it: a shape cut by the frame's
    edge (for a shape that crosses it once, as a fuselage runs out of an oval frame)."""
    out = []
    for i in range(len(poly)):
        (ax, ay), (bx, by) = poly[i], poly[(i + 1) % len(poly)]
        m = max(1, int(math.hypot(bx - ax, by - ay) / step))
        for j in range(m):
            px, py = ax + (bx - ax) * j / m, ay + (by - ay) * j / m
            dd = math.hypot((px - cx) / rx, (py - cy) / ry)
            if dd > 1.0:
                px, py = cx + (px - cx) / dd, cy + (py - cy) / dd
            out.append((px, py))
    return out


def _clip_oval_run(pts, cx, cy, rx, ry, step=0.3):
    """The part of the open line `pts` inside the ellipse (cx, cy, rx, ry), as one run."""
    out = []
    for i in range(len(pts) - 1):
        (ax, ay), (bx, by) = pts[i], pts[i + 1]
        m = max(1, int(math.hypot(bx - ax, by - ay) / step))
        for j in range(m + (1 if i == len(pts) - 2 else 0)):
            px, py = ax + (bx - ax) * j / m, ay + (by - ay) * j / m
            if math.hypot((px - cx) / rx, (py - cy) / ry) <= 1.0:
                out.append((px, py))
    return out


def tiptoe_tyre(x, y, r, lean=34.0, flat=0.8, n=48):
    """A tyre's outline (its hub at (x, y), `r` its radius) standing on the front of its tread as a foot stands on its
    toes: its heel (the tread behind) cut away up and back by a flat `flat` r from its hub, whose face is turned
    `lean` degrees back from straight down, so that daylight shows under it. Returns the outline and its toe (the
    lowest point: where it stands, a little before its hub)."""
    a = math.radians(lean)
    dx, dy = -math.sin(a), -math.cos(a)                                  # (down and back: the heel's way)
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        px, py = r * math.cos(t), r * math.sin(t)
        over = px * dx + py * dy - flat * r
        if over > 0:
            px, py = px - dx * over, py - dy * over
        pts.append((x + px, y + py))
    toe = min(pts, key=lambda q: q[1])
    return pts, toe


def b17_close(d, oval, X, Y, k, ground, skin="art_olive", glass="art_lgrey", tyre="art_black", leg="art_lgrey",
              prop="art_lgrey", tyre_r=4.2, axle_x=23.0, lean=34.0, flat=0.8):
    """A B-17 close up, side on and level, facing right, running out of the oval frame `oval` (cx, cy, rx, ry) behind
    her nose: her nose at (X, Y) on the grid, k grid units to the foot. Her fuselage runs out of the frame; before it
    her inboard engine with its propeller's two blades, her glazed nose and its frames, a cheek window, her windscreen
    and top turret; under the engine her main wheel, big, standing on the front of its tyre (tiptoe_tyre()) with its
    toe on the line `ground` (grid y). Returns the toe (grid)."""
    cx, cy, rx, ry = oval
    B = _bomber_frame(X, Y, k, 1, 0.0)
    w0, fine = d.ink, d.ink * 0.7

    def Q(pts):
        return _clip_oval([B(x, y) for x, y in pts], cx, cy, rx * 0.995, ry * 0.995)

    def L(pts, w, paint="art_ink"):
        seg = _clip_oval_run([B(x, y) for x, y in smooth(pts, False, 4)] if len(pts) > 2 else [B(x, y) for x, y in pts], cx, cy, rx * 0.985, ry * 0.985)
        if len(seg) > 1:
            d.line(paint, seg, w, taper=False)
    d.shape(skin, Q(smooth(B17["hull"], True, 3)), ink=w0)                                              # (her fuselage)
    d.shape(skin, Q(smooth(B17["nacelle"], True, 3)), ink=w0)                                           # (her inboard engine)
    L([(14.6, -0.3), (14.6, -4.9)], 0.2 * k)
    # (her main wheel under it: its leg, its tyre big and standing on its toe, its hub)
    r = tyre_r * k
    hub_x = B(axle_x, 0.0)[0]
    pts, toe = tiptoe_tyre(0.0, 0.0, r, lean, flat)
    hub_y = ground - toe[1]
    d.limb(leg, [B(axle_x + 0.4, -3.6), (hub_x, hub_y)], [0.7 * k, 0.6 * k], ink=w0)
    d.shape(tyre, [(hub_x + u, hub_y + v) for u, v in pts], ink=w0)
    d.oval(leg, hub_x, hub_y, r * 0.42, r * 0.42, ink=fine)
    d.oval("art_ink", hub_x, hub_y, r * 0.12, r * 0.12, ink=0.0)
    hx, hy = 11.7, -2.55                                                                                 # (its propeller)
    for y1 in (5.4, -5.4):
        d.limb(prop, [B(hx, hy), B(hx, hy + y1 * 0.5), B(hx, hy + y1)], [0.55 * k, 0.62 * k, 0.42 * k], ink=w0)
    d.oval(prop, *B(hx - 0.3, hy), 0.95 * k, 0.9 * k, ink=fine)
    d.shape(glass, Q(_glass()), ink=w0)                                                                  # (her glazed nose)
    L([(0.5, 0.45), (3.2, 0.6), (6.0, 0.5)], 0.2 * k)
    L([(3.0, 2.95), (3.25, 0.2), (3.15, -2.5)], 0.2 * k)
    d.shape(glass, Q([(8.6, 1.0), (10.8, 1.0), (10.8, 2.5), (8.6, 2.5)]), ink=fine)                   # (a cheek window)
    d.shape(glass, Q(B17["wind"]), ink=fine)                                                              # (her windscreen)
    L([(18.0, 5.4), (18.0, 3.95)], 0.16 * k)
    d.shape(glass, Q(ellipse(24.0, 5.45, 2.0, 1.5, 0, 12, 0, 180)), ink=fine)                         # (her top turret)
    return (hub_x + toe[0], ground)


# ---- 79: a B-17's nose, big, in a round frame ------------------------------------------------------------------------------
def _clip_run(pts, cx, cy, r, step=0.3):
    """The part of the open line `pts` inside the circle (cx, cy, r), as one run (for a line that leaves it once)."""
    out = []
    for i in range(len(pts) - 1):
        (ax, ay), (bx, by) = pts[i], pts[i + 1]
        m = max(1, int(math.hypot(bx - ax, by - ay) / step))
        for j in range(m + (1 if i == len(pts) - 2 else 0)):
            px, py = ax + (bx - ax) * j / m, ay + (by - ay) * j / m
            if math.hypot(px - cx, py - cy) <= r:
                out.append((px, py))
    return out


def b17_nose(d, disc, X, Y, k, s=-1, tilt=0.0, skin="art_lsteel", belly="art_steel", glass="art_lblue",
             prop="art_grey", hub="art_dark", tips="art_yellow"):
    """A B-17's nose big and side on, running out of the round frame `disc` (cx, cy, r): its polished metal skin (a
    shade darker along its belly), its glazed nose with its frames, a cheek window, its windscreen; before it all its
    near inboard engine and its propeller (light blades, so that they show on a dark ground; their tips yellow).
    Returns its frame B (feet back from its nose, feet up: the grid)."""
    cx, cy, r = disc
    B = _bomber_frame(X, Y, k, s, tilt)
    w0, fine = d.ink, d.ink * 0.7

    def Q(pts):
        return _clip_disc([B(x, y) for x, y in pts], cx, cy, r * 0.995)

    def L(pts):
        return _clip_run([B(x, y) for x, y in smooth(pts, False, 4)], cx, cy, r * 0.985)
    top = smooth(B17["hull"][:10], False, 4)
    under = [(24.0, -4.55), (18.0, -4.4), (10.0, -3.8), (5.0, -3.0), (2.0, -2.0), (0.5, -1.0), (0.0, 0.3)]
    bottom = smooth(under, False, 4)[:-1]
    d.shape(skin, Q(top + [(26.0, 5.65), (26.0, -4.6)] + bottom), ink=w0)
    d.shape(belly, Q([(5.0, -1.5), (26.0, -1.9), (26.0, -4.6)] + smooth(under[:4], False, 4)), ink=0.0)   # (its belly, in shade)
    seg = L([(5.0, -1.5), (15.0, -1.75), (26.0, -1.9)])
    if len(seg) > 1:
        d.line("art_ink", seg, 0.12 * k, taper=False)
    d.shape(glass, Q(_glass()), ink=w0)
    for pts in ([(0.5, 0.45), (3.2, 0.6), (6.0, 0.5)], [(3.0, 2.95), (3.25, 0.2), (3.15, -2.5)], [(1.4, 2.1), (1.6, -1.6)]):
        seg = L(pts)
        if len(seg) > 1:
            d.line("art_ink", seg, 0.2 * k, taper=False)
    d.shape(glass, Q([(8.6, 1.0), (10.8, 1.0), (10.8, 2.5), (8.6, 2.5)]), ink=fine)                   # (a cheek window)
    d.shape(glass, Q(B17["wind"]), ink=fine)                                                              # (its windscreen)
    seg = L([(18.0, 5.4), (18.0, 3.95)])
    if len(seg) > 1:
        d.line("art_ink", seg, 0.16 * k, taper=False)
    d.shape(skin, Q(smooth(B17["nacelle"], True, 3)), ink=w0)                                            # (its inboard engine)
    d.shape(belly, Q([(12.4, -2.6), (26.0, -2.8), (26.0, -4.9), (17.0, -5.0), (13.4, -4.9), (12.4, -4.6)]), ink=0.0)
    seg = L([(14.6, -0.3), (14.6, -4.9)])
    if len(seg) > 1:
        d.line("art_ink", seg, 0.14 * k, taper=False)
    hx, hy = 11.7, -2.55                                                                                  # (its propeller)
    for y0, y1 in ((0.0, 5.6), (0.0, -5.6)):
        seg = L([(hx, hy + y0), (hx, hy + (y0 + y1) / 2), (hx, hy + y1)])
        if len(seg) > 1:
            d.line(prop, seg, 0.7 * k)
        seg = L([(hx, hy + y1 * 0.8), (hx, hy + y1 * 0.97)])
        if len(seg) > 1:
            d.line(tips, seg, 0.5 * k, taper=False)
    d.shape(hub, Q(ellipse(hx - 0.3, hy, 0.9, 0.85, 0, 14)), ink=fine)
    return B


# ---- 80: a prickly pear, a cowboy hat ---------------------------------------------------------------------------------------
def _paddle(base, tip, wide):
    """A prickly pear's pad from its `base` (where it grows from another) to its `tip`: narrow at its base, widest two
    thirds out, round at its end."""
    (x0, y0), (x1, y1) = base, tip
    L = math.hypot(x1 - x0, y1 - y0)
    ux, uy = (x1 - x0) / L, (y1 - y0) / L
    nx, ny = -uy, ux
    prof = [(0.0, 0.16), (0.2, 0.33), (0.45, 0.45), (0.68, 0.5), (0.86, 0.44), (0.97, 0.25), (1.0, 0.0),
            (0.97, -0.25), (0.86, -0.44), (0.68, -0.5), (0.45, -0.45), (0.2, -0.33), (0.0, -0.16)]
    return [(x0 + ux * L * t + nx * wide * c, y0 + uy * L * t + ny * wide * c) for t, c in prof]


def spines(d, places, long=5.8, base=2.1, paint="art_cream", fan=26.0):
    """A prickly pear's spines, standing out from its pads' edges: at each (x, y, angle) a pair of stout pale spines
    fanning `fan` degrees apart about `angle` (outward from the pad), each a thorn `long` from a little inside the edge
    and `base` across its root, edged as the painter edges everything. Painted before the pads, so that each pad hides
    their roots."""
    for x, y, ang in places:
        for da in (-fan / 2, fan / 2):
            t = math.radians(ang + da)
            ux, uy = math.cos(t), math.sin(t)
            nx, ny = -uy, ux
            rx_, ry_ = x - 1.4 * ux, y - 1.4 * uy
            d.shape(paint, [(rx_ + nx * base / 2, ry_ + ny * base / 2), (x + ux * long, y + uy * long), (rx_ - nx * base / 2, ry_ - ny * base / 2)],
                    ink=d.ink * 0.7)


def stetson(d, x, y, w, turn=0.0, paint="art_white", band="art_brown"):
    """A cowboy hat, its brim's middle at (x, y), `w` across its brim, turned `turn` degrees: its brim curled up at
    the sides, its crown creased down the middle, its band."""
    P = _frame(x, y, w / 21.0, 1, turn)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    fine = d.ink * 0.8
    d.fill(paint, Q([(-5.1, 0.4), (-5.6, 5.4), (-4.2, 8.0), (-1.4, 8.4), (0.0, 7.3), (1.4, 8.4), (4.2, 8.0), (5.6, 5.4), (5.1, 0.4)]), steps=4)
    d.line("art_ink", Q([(0.0, 7.2), (-0.2, 5.6), (0.1, 4.2)]), 0.3 * w / 21.0)
    d.shape(band, Q([(-5.15, 0.6), (5.15, 0.6), (5.3, 2.3), (-5.3, 2.3)]), ink=fine)
    d.fill(paint, Q([(-10.6, 2.6), (-9.4, 0.6), (-6.0, -0.5), (0.0, -0.9), (6.0, -0.5), (9.4, 0.6), (10.6, 2.6), (8.6, 1.6), (5.2, 0.9),
                     (0.0, 0.7), (-5.2, 0.9), (-8.6, 1.6)]), steps=4)
    return P


# ---- the paintings --------------------------------------------------------------------------------------------------------
@painting(74)
def p74(d, a):   # a wire-haired terrier in a flight jacket holding a map
    terrier(d, 18, 2, 79)
    names(d, a, (94.2, 42), 14.5, lines=split2(a["name"]), indent=0, turn=2)


@painting(75)
def p75(d, a):   # a woman in a wide sun hat and long dress holding a magnolia bloom
    bloom_arm = {"_b2h_bloom": ((0.58, -0.98), (1.02, -0.18))}
    F = _lady(d, 20, 2, 70, 1, extra_arms=bloom_arm, pose="together", arms=("_b2h_bloom", "hip"),
              wear={"top": "art_pink", "skirt": "art_pink", "hem": "long", "flare": 0.36, "sleeves": "puff", "sleeve": "art_white",
                    "neck": "square", "belt": "art_white", "buckle": "art_white", "shoes": "art_white"},
              hair="rolls", hair_paint="art_hair")
    sun_hat(d, F, "art_yellow", "art_blue")
    (hx, hy), _ = F.hand["fore"]
    d.line("art_green", [(hx - 0.4, hy - 2.2), (hx + 0.2, hy + 1.0), (hx + 0.8, hy + 3.4)], 0.9, taper=False)
    magnolia(d, hx + 1.4, hy + 6.4, 8.4, turn=-8)
    d.oval("art_skin", hx, hy, 0.155 * F.k, 0.13 * F.k, ink=d.ink * 0.7)
    names(d, a, (97, 33), 14.5, lines=split2(a["name"]), indent=10, turn=4)


@painting(76)
def p76(d, a):   # a carnival barker in a straw boater and striped jacket with a cane
    F = man(d, 21, 1, 75, 1, pose="wide", arms=(to(2.5, 6.1, "down"), "hip"), tilt=3,
            wear={"shirt": "art_white", "jacket": "art_red", "cut": "suit", "stripes": "art_white", "trousers": "art_black", "bowtie": "art_black",
                  "shoes": "art_black", "hat": "art_yellow", "band": "art_black"},
            hat="boater", mouth="open", moustache="trim", cheeks=False)
    (hx, hy), _ = F.hand["fore"]
    cane(d, (hx, hy), 196.0, back=21.0, long=2.6, w=2.0, hook=2.0)
    F.grip("fore")
    names(d, a, (100, 38), 15, lines=["Step", "Right", "Up"], gap=1.36, indent=6)


@painting(77)
def p77(d, a):   # a tin washtub with a toy boat and a scrubbing brush
    cx, cy, rx, ry, bot = 0.0, 26.0, 29.0, 7.4, 5.2
    d.oval("art_grey", cx, cy, rx - 1.5, ry - 1.2, ink=0.0)                                   # (its inside)
    d.oval("art_lblue", cx, cy - 1.0, rx - 2.6, ry - 2.1, ink=0.0)                            # (the water)
    d.shape("art_steel", _ring_part(cx, cy, rx, ry, 1.9, 0, 180))                            # (its rim, behind)
    bath_brush(d, (-5.0, 27.6), (-40.0, 37.2), wide=7.2)                                       # (laid across its rim)
    d.blobs("art_white", [(-16.4, 29.0, 4.3), (-10.2, 31.2, 5.5), (-4.0, 29.4, 4.1), (-22.0, 27.8, 3.1), (-10.6, 27.4, 11.0, 2.4)])
    toy_boat(d, 12.5, 25.6, 15.0)
    body = ellipse(cx, cy, rx, ry, 0, 24, 180, 360) + ellipse(cx, bot, rx * 0.86, ry * 0.86, 0, 24, 360, 180)
    d.shape("art_steel", body)
    d.shape("art_steel", _ring_part(cx, cy, rx, ry, 1.9, 180, 360))                          # (its rim, before)
    for yy, f in ((18.8, 0.95), (11.0, 0.9)):                                                 # (its two ribs)
        d.line("art_lsteel", ellipse(cx, yy, rx * f, ry * f, 0, 16, 196, 344), 1.2)
        d.line("art_ink", ellipse(cx, yy - 1.2, rx * f, ry * f, 0, 16, 200, 340), 0.45)
    d.line("art_lsteel", ellipse(cx, cy - 0.5, rx - 1.0, ry - 0.7, 0, 12, 214, 252), 0.8)
    for sx in (-1, 1):                                                                         # (its handles)
        hx = cx + sx * (rx + 0.4)
        d.shape("art_steel", ellipse(hx, cy - 1.6, 3.8, 3.0, 0, 10, 90 if sx < 0 else -90, 270 if sx < 0 else 90) +
                list(reversed(ellipse(hx, cy - 1.6, 2.0, 1.4, 0, 10, 90 if sx < 0 else -90, 270 if sx < 0 else 90))))
    names(d, a, (0, -26.6), 15.5, lines=["Saturday", "Night Bath"], gap=1.2)


def plain_bomber(d, x, y, L, tilt=0.0, skin="art_olive", glass="art_lgrey", tyre="art_black", strut="art_lgrey"):
    """A plain four-engined bomber in side view, nose toward +x, its tail's end at (x, y), `L` long, its nose up `tilt`
    degrees; wheels down. No markings and no one aircraft's lines: a long body, a tall rounded fin, the near wing's root
    with its two engines in a row and their propellers' blurs, a glazed nose, the cockpit's windows, the main wheel under
    the near inner engine and the small tail wheel. Returns its frame P(u, v) (u along it from the tail's end, v up, in L)."""
    c, s_ = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))

    def P(u, v):
        return (x + (u * c - v * s_) * L, y + (u * s_ + v * c) * L)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    w0 = d.ink
    # (the far tailplane, and the main gear's far wheel, behind)
    d.oval(tyre, *P(0.585, -0.125), 0.032 * L, 0.036 * L, ink=w0 * 0.7)
    # (the fin: tall and rounded, its leading edge running far forward along the back)
    d.fill(skin, Q([(0.02, 0.06), (0.03, 0.2), (0.07, 0.27), (0.12, 0.27), (0.16, 0.22), (0.2, 0.15), (0.3, 0.1), (0.3, 0.07)]), steps=4)
    d.line("art_ink", Q([(0.06, 0.08), (0.08, 0.24)]), 0.006 * L)                # (the rudder's hinge)
    # (the body: long, deep in the middle, the tail narrowing to its end)
    body = [(0.0, 0.05), (0.08, 0.07), (0.3, 0.09), (0.6, 0.105), (0.8, 0.105), (0.88, 0.095), (0.95, 0.07), (1.0, 0.03), (1.0, 0.0),
            (0.97, -0.03), (0.9, -0.05), (0.7, -0.055), (0.45, -0.05), (0.2, -0.02), (0.05, 0.02), (0.0, 0.035)]
    d.fill(skin, Q(body), steps=4)
    # (the tailplane, near side, a thin slab)
    d.fill(skin, Q([(0.0, 0.045), (0.02, 0.06), (0.17, 0.058), (0.2, 0.05), (0.17, 0.042), (0.02, 0.04)]), ink=w0 * 0.8, steps=2)
    # (the glazed nose, and the cockpit's windows high behind it)
    d.fill(glass, Q([(0.94, 0.072), (0.985, 0.045), (1.0, 0.0), (0.975, -0.032), (0.94, -0.04), (0.93, 0.02)]), ink=w0 * 0.8, steps=3)
    d.shape(glass, Q([(0.8, 0.098), (0.86, 0.093), (0.885, 0.08), (0.8, 0.08)]), ink=w0 * 0.7)
    # (the near wing's root, a thin slab, and its two engines in a row under it, each with its propeller's blur)
    d.fill(skin, Q([(0.47, 0.012), (0.63, 0.018), (0.65, 0.0), (0.47, -0.006)]), ink=w0 * 0.8, steps=2)
    for eu, ev, sc in ((0.66, -0.005, 0.9), (0.7, -0.02, 1.0)):
        d.fill(skin, Q([(eu - 0.13 * sc, ev + 0.01), (eu - 0.02, ev + 0.03 * sc), (eu + 0.02, ev + 0.02 * sc), (eu + 0.02, ev - 0.02 * sc),
                        (eu - 0.02, ev - 0.03 * sc), (eu - 0.13 * sc, ev - 0.006)]), ink=w0 * 0.8, steps=3)
        d.oval(glass, *P(eu + 0.032, ev), 0.006 * L, 0.075 * L * sc, turn=tilt, ink=w0 * 0.5)
    # (the main gear: a strut down from the near inner engine to its wheel; the small tail wheel)
    d.limb(strut, Q([(0.6, -0.03), (0.6, -0.11)]), [0.009 * L, 0.008 * L], ink=w0 * 0.6)
    d.oval(tyre, *P(0.6, -0.125), 0.036 * L, 0.04 * L, ink=w0 * 0.8)
    d.oval(strut, *P(0.6, -0.125), 0.013 * L, 0.014 * L, ink=w0 * 0.5)
    d.limb(strut, Q([(0.1, 0.03), (0.1, -0.005)]), [0.006 * L, 0.005 * L], ink=w0 * 0.5)
    d.oval(tyre, *P(0.1, -0.012), 0.016 * L, 0.018 * L, ink=w0 * 0.6)
    return P


def rail_fence(d, x0, x1, y, h, posts=3, paint="art_white"):
    """A post-and-rail fence across the end of a strip, from x0 to x1 standing on y, `h` tall: square posts, two rails."""
    for i in range(posts):
        px = x0 + (x1 - x0) * i / max(posts - 1, 1)
        d.shape(paint, [(px - 0.9, y), (px + 0.9, y), (px + 0.9, y + h), (px - 0.9, y + h)], ink=d.ink * 0.8)
    for ry in (0.42, 0.82):
        d.shape(paint, [(x0 - 1.6, y + h * ry - 0.7), (x1 + 1.6, y + h * ry - 0.7), (x1 + 1.6, y + h * ry + 0.7), (x0 - 1.6, y + h * ry + 0.7)],
                ink=d.ink * 0.8)


@painting(78)
def p78(d, a):   # a plain bomber, wheels down, just clearing the fence at the end of a short strip
    cx, cy, rx, ry = 0.0, 0.0, 38.0, 25.0
    oval = (cx, cy, rx, ry)
    frame = ellipse(cx, cy, rx, ry, 0, 72)
    d.shape("art_lblue", frame, ink=0.0)                                                     # (the sky in its frame)
    ground = -15.0                                                                           # (the strip's top)
    end = -2.0                                                                               # (the strip's end, and its fence)
    d.shape("art_lgrey", _clip_oval([(-rx - 2, ground), (end, ground), (end, -ry - 2), (-rx - 2, -ry - 2)], cx, cy, rx, ry), ink=d.ink * 0.9)
    d.shape("art_lgrey", _clip_oval([(end, ground - 1.2), (rx + 2, ground - 1.2), (rx + 2, -ry - 2), (end, -ry - 2)], cx, cy, rx, ry), ink=d.ink * 0.9)
    rail_fence(d, end + 1.5, end + 11.0, ground - 1.2, 6.5, posts=3)
    # the bomber climbing out over it, its main wheel a hand's breadth over the top rail
    plain_bomber(d, -33.0, -4.6, 56.0, tilt=9.0)
    d.shape(None, frame, ink=d.ink * 1.2)                                                    # (the frame's edge, over all)
    names(d, a, (0, -38), 13, lines=[a["name"]])


@painting(79)
def p79(d, a):   # a mechanic in overalls polishing a bomber's nose with a rag
    disc = (34.0, 40.0, 36.0)
    d.oval("art_navy", disc[0], disc[1], disc[2], disc[2], ink=d.ink * 1.2)
    X, Y = 25.0, 43.5
    B = b17_nose(d, disc, X, Y, 2.65, s=-1, tilt=5.0)
    rx_, ry_ = B(2.6, -0.1)                                                                  # (where he rubs: the middle of the glass)
    rub = dict(MAN_ARMS["polish"])                                                           # (the kit's polish, its hand sent there)
    rub["fore"] = to((rx_ - 15) / (56 / HEADS) - 0.1, (ry_ - 9.5) / (56 / HEADS))
    F = man(d, 15, 9.5, 56, 1, pose="stand", arms=(rub, "hip"),
            wear={"shirt": "art_white", "sleeves": "rolled", "overalls": "art_lblue", "shoes": "art_brown", "cap": "art_khaki"},
            hat="cap", hair="short", mouth="smile")
    (hx, hy), _ = F.hand["fore"]
    rag = [(-3.0, 2.2), (-0.5, 3.0), (2.4, 2.5), (3.2, 0.3), (2.5, -2.1), (0.6, -2.6), (-0.6, -4.4), (-1.9, -4.3), (-2.1, -2.4), (-3.3, -0.8)]
    d.fill("art_red", [(hx + 0.5 + u, hy + 0.4 + v) for u, v in rag], steps=3)               # (his rag, bunched under his palm, a corner hanging)
    for f in ([(-2.3, 1.4), (-1.0, 0.5), (-0.5, -1.1)], [(1.4, 2.2), (2.2, 1.0)], [(-1.3, -2.7), (-1.25, -3.8)]):
        d.line("art_ink", [(hx + 0.5 + u, hy + 0.4 + v) for u, v in f], 0.3)
    F.grip("fore")
    gx, gy = B(4.55, 1.95)                                                                   # (its gleam, on the glass he has rubbed)
    twinkle(d, gx, gy, 3.9)
    names(d, a, (100, 40), 14, lines=["Crew", "Chief's", "Pride"], gap=1.38, indent=6)


@painting(80)
def p80(d, a):   # a prickly pear cactus in a cowboy hat, tipping it politely
    f = 1.25                                                                                  # (grid units to one of its own)

    def S(pts):
        return [(u * f, v * f) for u, v in pts]
    d.fill("art_cream", S([(-17.0, 1.2), (-11.0, 4.8), (0.0, 6.2), (11.0, 4.8), (17.0, 1.2), (0.0, 0.0)]), steps=4)   # (its sand)
    arm_out = ((-9.0, 23.0), (-20.6, 42.0), 12.0)
    arm_up = ((12.0, 28.0), (18.4, 55.6), 10.5)
    spines(d, [(u * f, v * f, ang) for u, v, ang in (
        (-15.0, 41.8, 84.0),                                                                      # (its arm out, at its tip, by its fruit)
        (21.0, 52.8, 56.0),                                                                       # (its arm at its brim, at its tip)
        (-9.8, 47.4, 112.0),                                                                      # (its body's top)
        (-14.4, 21.0, 192.0), (14.0, 17.0, -14.0))])                                              # (its body's sides)
    d.fill("art_lgreen", S(_paddle(*arm_out)))                                                   # (its arm, out)
    d.fill("art_lgreen", S(_paddle((0.0, 3.0), (0.0, 50.0), 30.0)))                              # (its body)
    for (fx, fy, ft) in ((-21.4, 44.6, 25.0), (-18.0, 46.0, -5.0)):                              # (its fruit, on its arm's tip)
        d.oval("art_red", fx * f, fy * f, 1.8 * f, 2.5 * f, turn=ft, ink=d.ink * 0.8)
    for ex in (-5.0, 4.2):                                                                        # (its face)
        d.oval("art_ink", ex * f, 37.4 * f, 1.25 * f, 1.7 * f, ink=0.0)
        d.oval("art_white", (ex + 0.4) * f, 38.0 * f, 0.42 * f, 0.42 * f, ink=0.0)
        d.line("art_ink", S([(ex - 1.8, 40.6), (ex, 41.6), (ex + 1.8, 41.0)]), 0.55 * f)
    d.line("art_ink", S([(-4.8, 31.6), (-2.4, 29.6), (0.6, 29.4), (4.0, 31.6)]), 0.7 * f)
    for ex in (-8.0, 7.0):
        d.oval("art_pink", ex * f, 33.0 * f, 1.8 * f, 1.0 * f, ink=0.0)
    stetson(d, 2.0 * f, 51.4 * f, 26.0 * f, turn=10.0)
    d.fill("art_lgreen", S(_paddle(*arm_up)))                                                    # (its arm at its brim)
    names(d, a, (62.4, 32), 13.6, lines=["Prickly", "Pear", "Pete"], gap=1.38, indent=-4)
