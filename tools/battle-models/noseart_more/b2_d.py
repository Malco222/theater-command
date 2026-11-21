"""Batch 2, numbers 46 to 52 (painter b2_d): a hound asleep on a cloud, a hoop snake rolling downhill, a goat on stilts
chewing a flight plan, a woman under a four-leaf clover, a jitterbug in saddle shoes, a city kid bouncing a ball on his
stoop and a skater with a muff on a frozen river. Each picture is drawn from flat shapes with noseart's Drawing API and
the shared figure kit; the helpers below (a hound, a hoop snake, a goat on stilts, a clover, a saddle shoe, a skate, a
stoop, an open hand) are built the same way, for these paintings."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402


# ---- helpers ------------------------------------------------------------------------------------------------------------
def _rot(p, c, deg):
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)
    return (c[0] + (p[0] - c[0]) * ca - (p[1] - c[1]) * sa, c[1] + (p[0] - c[0]) * sa + (p[1] - c[1]) * ca)


def _tilt(d, i0, c, deg):
    """Every shape painted since op i0 turned `deg` degrees about c (a figure leaning into a dance or a glide)."""
    for i in range(i0, len(d.ops)):
        op = d.ops[i]
        if op[0] == "polys":
            d.ops[i] = ("polys", op[1], [[_rot(p, c, deg) for p in poly] for poly in op[2]])


def _sharp(runs, steps=5):
    """A closed outline from open runs of points, each run smoothed on its own: where two runs meet, a sharp corner."""
    out = []
    for run in runs:
        pts = smooth(run, False, steps) if len(run) > 2 else [tuple(p) for p in run]
        if out and math.hypot(out[-1][0] - pts[0][0], out[-1][1] - pts[0][1]) < 1e-6:
            pts = pts[1:]
        out += pts
    if len(out) > 2 and math.hypot(out[0][0] - out[-1][0], out[0][1] - out[-1][1]) < 1e-6:
        out.pop()
    return out


def _dashes(d, paint, pts, w, n=4):
    """A dashed line along the run pts, smoothed: n dashes, the gaps between them as long as they are."""
    path = smooth(pts, False, 8) if len(pts) > 2 else [tuple(p) for p in pts]
    run = [0.0]
    for i in range(1, len(path)):
        run.append(run[-1] + math.hypot(path[i][0] - path[i - 1][0], path[i][1] - path[i - 1][1]))
    seg = run[-1] / (2 * n - 1)

    def at(s_):
        for i in range(1, len(path)):
            if run[i] >= s_:
                return lerp(path[i - 1], path[i], (s_ - run[i - 1]) / max(run[i] - run[i - 1], 1e-9))
        return path[-1]

    for j in range(n):
        s0 = 2 * j * seg
        d.line(paint, [at(s0 + seg * q / 3.0) for q in range(4)], w, taper=False)


def _local(org, ang, pts, scale=1.0):
    """Points given about an origin, turned `ang` degrees and grown `scale`, onto the grid."""
    c, s_ = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    return [(org[0] + (u * c - v * s_) * scale, org[1] + (u * s_ + v * c) * scale) for u, v in pts]


# ---- 46: the hound ----------------------------------------------------------------------------------------------------
def hound(d, x, y, size, s=1, coat="art_tan", ear="art_brown", inside="art_lbrown"):
    """A hound lying asleep side on, his belly on y, his middle at x, `size` from his tail's tip to his nose, facing s:
    his tail lying limp behind him, his chin on his forepaw, and one long ear thrown forward from the back of his skull
    over his brow, its pale inside up and its tip hanging on the top of his muzzle, in front of his eyes."""
    k = size / 64.0

    def P(u, v):
        return (x + s * u * k, y + v * k)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    fine = d.ink * 0.75
    # (his tail, lying limp along behind him and draped down over the edge he lies on)
    d.limb(coat, Q([(-16.0, 3.0), (-21.0, 1.6), (-24.8, -0.4), (-27.0, -3.6), (-27.4, -7.2)]), [1.9 * k, 1.55 * k, 1.3 * k, 1.05 * k, 0.7 * k])
    # (his body, long and deep: his rump high and round, his back, his shoulders, his chest)
    d.fill(coat, Q([(-18.0, 1.0), (-19.6, 6.0), (-18.0, 11.8), (-12.8, 15.2), (-6.4, 14.4), (-0.6, 13.8), (5.0, 14.8), (9.6, 14.4),
                    (12.6, 11.0), (13.6, 6.0), (12.6, 1.0), (5.0, -0.6), (-8.0, -0.8), (-15.0, -0.4)]))
    d.line("art_ink", Q([(-5.4, 0.8), (-7.2, 6.6), (-11.8, 10.8), (-16.4, 10.6)]), 0.42 * k)          # (his haunch)
    d.fill(coat, Q([(-6.6, 0.0), (-6.8, 2.4), (-4.0, 3.2), (-0.6, 3.0), (0.6, 1.6), (-0.4, -0.2), (-3.6, -0.6)]), ink=fine)  # (his hind foot, tucked)
    for u in (-1.2, -2.6):
        d.line("art_ink", Q([(u, 2.6), (u - 0.3, 1.0)]), 0.28 * k, taper=False)
    d.line("art_ink", Q([(10.4, 1.4), (9.4, 6.6), (6.6, 10.4)]), 0.36 * k)                              # (his shoulder)
    # (his foreleg stretched out before him over the edge he lies on, its paw hanging down it)
    d.limb(coat, Q([(8.0, 4.0), (15.0, 2.4), (20.0, 2.2), (23.4, 0.6), (25.0, -2.4)]), [2.9 * k, 2.2 * k, 1.95 * k, 1.85 * k, 1.8 * k], ink=fine)
    d.oval(coat, *P(25.6, -4.0), 2.0 * k, 3.0 * k, turn=-12 * s, ink=fine)
    for u in (24.8, 26.4):
        d.line("art_ink", Q([(u + 0.9, -6.2), (u + 0.3, -5.2)]), 0.28 * k, taper=False)
    # (his head, big, resting on it: the round skull, the long muzzle, the flews hanging)
    hs = 1.15

    def Hd(pts):
        return Q([(12.0 + (u - 12.0) * hs, 3.0 + (v - 3.0) * hs) for u, v in pts])

    d.fill(coat, Hd([(9.0, 9.0), (9.4, 13.0), (12.0, 15.8), (15.6, 16.6), (19.0, 15.6), (21.6, 13.4), (25.2, 12.2), (28.8, 11.0),
                     (30.6, 9.2), (30.4, 6.6), (29.0, 4.8), (26.4, 3.4), (21.0, 2.8), (15.0, 3.0), (11.0, 4.6), (9.2, 6.4)]))
    d.fill("art_ink", Hd(ellipse(30.0, 8.8, 1.8, 1.4, -15.0, 24)), ink=0.0)                           # (his nose)
    d.line("art_white", Hd([(29.0, 9.6), (30.1, 9.8)]), 0.45 * k * hs)
    d.line("art_ink", Hd([(29.8, 6.0), (28.0, 4.9), (25.8, 4.6)]), 0.45 * k * hs)                     # (his mouth, content)
    # (his ear: from the back of his skull, thrown forward over his crown and his brow, lying on his head with its pale
    #  inside up, its tip hanging down on his muzzle in front of his eyes; dark where it turns over at its root)
    d.fill(ear, Hd([(12.8, 12.4), (12.4, 14.8), (13.6, 17.0), (16.2, 18.6), (19.6, 18.2), (22.4, 16.2), (24.4, 13.2), (25.4, 10.0),
                    (25.4, 7.4), (24.6, 5.8), (23.2, 5.6), (22.4, 7.0), (21.8, 9.0), (20.0, 10.8), (17.4, 12.0), (14.8, 12.4)]))
    d.fill(inside, Hd([(15.6, 17.2), (17.2, 17.6), (19.8, 17.2), (22.0, 15.4), (23.8, 12.6), (24.5, 9.8), (24.4, 7.4), (23.8, 6.5),
                       (23.2, 6.7), (22.8, 8.4), (21.8, 10.0), (19.8, 11.6), (17.4, 12.8), (15.4, 13.2), (14.8, 15.4)]), ink=0.0)
    d.line("art_ink", Hd([(14.0, 12.6), (14.2, 15.2), (15.0, 17.6)]), 0.34 * k * hs)                 # (where it turns over)
    return P


@painting(46)
def p46(d, a):   # a hound dog asleep on a cloud, one ear over his eyes
    # (his bed: a cloud of five big uneven lumps, lumpy underneath too, his length or a little less)
    d.blobs("art_white", [(17.0, 16.5, 8.5), (29.0, 14.0, 11.0), (45.0, 12.5, 12.5), (62.0, 13.0, 12.0), (78.0, 15.0, 10.0)])
    hound(d, 50, 21.2, 100, 1)
    names(d, a, (54, -14), 15)


# ---- 47: the hoop snake -------------------------------------------------------------------------------------------------
def hoop_snake(d, cx, cy, R, r, body="art_lgreen", belly="art_yellow", eye="art_yellow", head_at=96.0, aim=14.0, way=1):
    """A hoop snake: a snake with the tip of its tail in its jaws, rolling as a hoop. Its middle at (cx, cy), R the
    hoop's radius to the middle of its body, r its body's half-thickness; its head at `head_at` degrees round the hoop,
    facing anticlockwise round it (way=1) or clockwise (way=-1: at the top of the hoop, facing right, as a hoop rolling
    to the right turns) and aimed `aim` degrees in, its jaws on the tip of its tail."""
    g = r / 3.4
    sg = 1.0 if way >= 0 else -1.0
    ph = math.radians(head_at)
    hx, hy = cx + R * math.cos(ph), cy + R * math.sin(ph)
    pa = math.radians(head_at + sg * aim)
    tx, ty = -sg * math.sin(pa), sg * math.cos(pa)               # (the way it faces)
    nx, ny = math.cos(pa), math.sin(pa)                          # (the top of its head)

    def H(a_, b_):
        return (hx + (a_ * tx + b_ * nx) * g, hy + (a_ * ty + b_ * ny) * g)

    # (its body round the hoop, from under its head back round the way it came to the tip of its tail, in its jaws)
    mx, my = H(3.4, -0.2)
    psi = math.degrees(math.atan2(my - cy, mx - cx))
    rho = math.hypot(mx - cx, my - cy)
    neck = head_at - sg * 26.0
    tip = psi - sg * 360.0
    while sg * (tip - (neck - sg * 300.0)) > 0.0:
        tip -= sg * 360.0
    while sg * (tip - (neck - sg * 420.0)) < 0.0:
        tip += sg * 360.0
    n = 30
    pts, rad = [], []
    for i in range(n):
        t = i / (n - 1)
        phi = neck + (tip - neck) * t
        left = sg * (phi - tip)                                  # (degrees still to go to the tip)
        blend = max(0.0, (40.0 - left) / 40.0) if left < 40.0 else 0.0
        rr = R + (rho - R) * min(1.0, blend)
        pts.append((cx + rr * math.cos(math.radians(phi)), cy + rr * math.sin(math.radians(phi))))
        rad.append(r * (min(1.0, 0.9 + 0.5 * t) if t < 0.2 else max(0.1, 1.0 - 1.35 * (t - 0.36)) if t > 0.36 else 1.0))
    d.limb(body, pts, rad)
    inner = []
    for (px, py), rr_ in zip(pts, rad):                          # (its pale belly, along the inside of the hoop)
        ux, uy = px - cx, py - cy
        ln = math.hypot(ux, uy) or 1.0
        inner.append((px - ux / ln * rr_ * 0.52, py - uy / ln * rr_ * 0.52))
    d.limb(belly, inner[:-3], [rr_ * 0.4 for rr_ in rad[:-3]], ink=0.0)
    # (its head: long and flat, wide at the jaw, the jaws open on its tail)
    head = [(-9.0, 3.4), (-5.0, 4.5), (-0.5, 4.6), (4.0, 4.2), (7.6, 3.4), (9.6, 2.4), (9.0, 1.6), (6.0, 0.9), (2.6, -0.3),
            (5.2, -1.6), (7.6, -2.6), (7.0, -3.4), (3.2, -4.2), (-1.6, -4.7), (-5.4, -4.6), (-9.0, -3.4)]
    d.fill(body, [H(*p) for p in head], steps=4)
    d.fill(belly, [H(*p) for p in [(-7.6, -2.0), (-2.0, -2.5), (3.0, -2.4), (6.4, -2.8), (6.2, -3.4), (3.0, -3.9), (-1.6, -4.3), (-5.6, -4.1)]], ink=0.0, steps=4)
    d.line("art_ink", [H(2.6, -0.3), H(0.0, -1.0), H(-2.4, -0.9)], 0.45 * g)       # (its grin, back along its jaw)
    d.oval(eye, *H(0.6, 2.2), 1.35 * g, 1.25 * g, ink=d.ink * 0.7)
    d.oval("art_ink", *H(0.9, 2.2), 0.42 * g, 0.95 * g, turn=math.degrees(pa), ink=0.0)
    d.line("art_ink", [H(-1.6, 3.5), H(0.6, 4.0), H(2.8, 3.4)], 0.5 * g)
    d.oval("art_ink", *H(7.4, 2.4), 0.36 * g, 0.3 * g, ink=0.0)
    return H


@painting(47)
def p47(d, a):   # a snake with its tail in its mouth rolling downhill like a hoop
    # (the hill: low and long, its round summit up at the left, its slope falling away under the hoop and on down past
    #  it, flattening to a round foot at the right)
    crest = [(-3.4, -0.4), (-4.2, 4.0), (-3.0, 9.0), (-0.2, 13.8), (3.6, 17.6), (8.0, 19.6), (12.6, 19.4), (17.6, 17.6), (24.0, 14.6),
             (32.0, 11.6), (41.0, 8.8), (50.0, 6.6), (59.0, 5.0), (66.0, 4.2), (71.4, 3.4), (73.8, 1.6)]
    base = [(73.4, -0.4), (70.0, -0.8), (40.0, -0.8), (0.0, -0.8)]
    hill = d.fill("art_green", crest + base, steps=4)
    # (the hoop, sitting on the slope below the summit, the lines of its rolling behind it)
    R, r = 19.0, 5.4
    cx = 38.0
    cy = max(py + math.sqrt(max((R + r) ** 2 - (px - cx) ** 2, 0.0)) for px, py in hill if abs(px - cx) < R + r) + 0.2
    for rr, a0, a1 in ((R + r + 3.6, 114, 182), (R + r + 7.2, 124, 172)):
        d.line("art_yellow", ellipse(cx, cy, rr, rr, 0, 16, a0, a1), 1.6)
    hoop_snake(d, cx, cy, R, r, head_at=88.0, way=-1)
    names(d, a, (106, 46), 15, lines=["Hoop", "Snake", "Hollow"], gap=1.36)


# ---- 48: the goat on stilts -------------------------------------------------------------------------------------------------
def goat_on_stilts(d, x, y, size, coat="art_tan", pale="art_cream", horn="art_cream", stilt="art_yellow", paper="art_white"):
    """A goat up on stilts, on its hind legs on their footrests and its forefeet on their tops, its knees knocking;
    its head turned to the right, a flight plan in its jaws, a bite out of its corner. Its stilts' feet at y, their middle
    at x, `size` tall to its horns' tips."""
    k = size / 80.0

    def P(u, v):
        return (x + u * k, y + v * k)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    fine = d.ink * 0.75
    # (its stilts, a little splayed, and their footrests)
    for (u0, u1, side) in ((-12.0, -9.0, 1), (12.0, 9.0, -1)):
        d.limb(stilt, Q([(u0, 0.0), ((u0 + u1) / 2, 23.0), (u1, 46.0)]), [1.35 * k, 1.3 * k, 1.25 * k])
        fu = u0 + (u1 - u0) * 22.0 / 46.0
        d.shape(stilt, Q([(fu, 21.0), (fu + side * 5.4, 21.6), (fu + side * 5.4, 23.4), (fu, 23.6)]))
    # (its hind legs, knock-kneed, down to its hooves on the footrests)
    for sx in (-1, 1):
        d.limb(coat, Q([(sx * 4.8, 38.0), (sx * 3.4, 33.0), (sx * 2.4, 29.6), (sx * 4.4, 26.6), (sx * 6.2, 24.6)]),
               [3.2 * k, 2.6 * k, 1.5 * k, 1.25 * k, 1.1 * k])
        d.fill("art_dark", Q([(sx * 4.6, 25.2), (sx * 7.8, 25.2), (sx * 8.2, 23.4), (sx * 4.2, 23.4)]), ink=fine, steps=2)
    # (its body, upright, its pale belly)
    d.fill(coat, Q([(-7.6, 40.0), (-8.6, 46.0), (-7.4, 53.0), (-3.6, 56.6), (3.6, 56.6), (7.4, 53.0), (8.6, 46.0), (7.6, 40.0),
                    (3.0, 36.4), (-3.0, 36.4)]))
    d.fill(pale, Q([(-3.8, 40.0), (-4.4, 45.0), (-3.0, 51.0), (0.0, 52.4), (3.0, 51.0), (4.4, 45.0), (3.8, 40.0), (0.0, 38.4)]), ink=fine)
    # (its forelegs down to the stilts' tops)
    for sx in (-1, 1):
        u1 = -9.0 if sx < 0 else 9.0
        d.limb(coat, Q([(sx * 6.0, 53.6), (sx * 8.6, 50.0), (u1 + sx * 0.4, 47.2)]), [1.9 * k, 1.5 * k, 1.25 * k], ink=fine)
        d.fill("art_dark", Q([(u1 - 1.8, 46.0), (u1 + 1.8, 46.0), (u1 + 1.6, 48.4), (u1 - 1.6, 48.4)]), ink=fine, steps=2)
    # (its neck and head, turned to the right)
    d.limb(coat, Q([(0.4, 54.0), (2.0, 58.6), (3.6, 62.0)]), [3.6 * k, 3.3 * k, 3.1 * k])
    for (pts, col) in (([(1.8, 69.5), (-0.6, 74.4), (-4.4, 76.6), (-7.4, 75.0)], "art_lgrey"),
                       ([(3.4, 70.0), (2.4, 75.2), (-1.0, 78.6), (-4.4, 78.2)], horn)):
        d.limb(col, Q(pts), [1.45 * k, 1.25 * k, 0.9 * k, 0.4 * k], ink=fine)
    d.fill(coat, Q([(-2.4, 69.4), (-6.6, 67.6), (-8.4, 66.6), (-6.2, 65.6), (-2.6, 66.2)]), ink=fine)
    d.fill(coat, Q([(-1.2, 65.6), (-1.0, 69.4), (1.6, 71.4), (5.4, 70.6), (9.4, 66.2), (12.8, 63.2), (13.4, 61.0), (12.2, 59.2),
                    (8.8, 58.6), (4.8, 59.0), (1.6, 60.4)]))
    d.fill("art_brown", Q([(7.0, 59.2), (10.2, 59.0), (9.4, 55.4), (8.0, 52.0), (6.8, 55.6)]), ink=fine)        # (its beard)
    d.oval("art_white", *P(4.6, 66.6), 2.0 * k, 2.4 * k, ink=fine)
    d.oval("art_ink", *P(5.0, 66.2), 0.7 * k, 0.8 * k, ink=0.0)
    d.line("art_ink", Q([(2.4, 69.4), (4.6, 70.4), (7.0, 69.4)]), 0.5 * k)
    d.oval("art_ink", *P(12.2, 61.8), 0.4 * k, 0.35 * k, ink=0.0)
    # (the flight plan in its jaws, a bite out of its corner: on it only its route, a few bold dashes from the field it
    #  leaves to the one it makes for; no words)
    sheet = [(13.2, 60.4), (14.4, 61.4), (15.2, 60.2), (16.4, 61.0), (26.0, 60.4), (25.4, 45.0), (12.8, 46.0), (12.6, 57.0), (14.2, 58.4)]
    d.shape(paper, Q(sheet))
    _dashes(d, "art_navy", Q([(18.0, 49.4), (21.0, 50.6), (23.0, 52.8), (23.7, 55.0)]), 1.2 * k, n=3)
    d.oval("art_navy", *P(16.0, 48.8), 1.3 * k, 1.3 * k, ink=0.0)
    d.oval("art_red", *P(23.9, 57.3), 1.45 * k, 1.45 * k, ink=fine * 0.6)
    # (nervous: two drops of sweat flying up off its brow, in front of its horns)
    for (u, v, ang) in ((8.4, 73.6, -47.0), (11.4, 70.8, -39.0)):
        d.fill("art_sky", Q(_local((u, v), ang, [(0.0, -2.1), (1.05, 0.0), (0.0, 1.05), (-1.05, 0.0)])), ink=fine, steps=4)
    return P


@painting(48)
def p48(d, a):   # a goat on stilts chewing a flight plan
    goat_on_stilts(d, 26, 2, 82)
    names(d, a, (103, 42), 14, lines=["The", "Nervous", "Goat"], gap=1.36)


# ---- 49: the clover umbrella ----------------------------------------------------------------------------------------------
def heart_leaf(L):
    """One leaf of a clover, its point at (0, 0) and its notch at L along +x."""
    up = [(0.0, 0.0), (0.24 * L, 0.2 * L), (0.5 * L, 0.42 * L), (0.78 * L, 0.52 * L), (0.98 * L, 0.42 * L), (1.06 * L, 0.24 * L), (1.0 * L, 0.08 * L), (0.86 * L, 0.0)]
    dn = [(u, -v) for u, v in reversed(up)]
    return up, dn


def clover(d, x, y, L, squash=0.56, turn=0.0, paint="art_lgreen", rib="art_green"):
    """A four-leaf clover held up like an umbrella: its four heart leaves round its middle (x, y), each L long, seen a
    little from below and the side (`squash`), turned `turn` degrees."""
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def T(p):
        u, v = p[0], p[1] * squash
        return (x + u * c - v * s_, y + u * s_ + v * c)

    order = (225.0, 315.0, 135.0, 45.0)                         # (the far leaves first, the near ones over them)
    order = (45.0, 135.0, 315.0, 225.0)
    for ang in order:
        up, dn = heart_leaf(L)
        pts = _sharp([_local((0.0, 0.0), ang, up), _local((0.0, 0.0), ang, dn)])
        d.shape(paint, [T(p) for p in pts])
        d.line(rib, [T(p) for p in _local((0.0, 0.0), ang, [(0.12 * L, 0.0), (0.45 * L, 0.0), (0.8 * L, 0.0)])], L * 0.07)
    return T


@painting(49)
def p49(d, a):   # a woman in a raincoat holding a four-leaf clover like an umbrella
    x, y, h = 30.0, 2.0, 54.0
    F = lady(d, x, y, h, 1, pose="stand", arms=("wave", "hip"),
             wear={"top": "art_yellow", "coat": "art_yellow", "belt": "art_yellow", "buckle": "art_brown", "buttons": "art_brown",
                   "sleeves": "long", "boots": "art_red", "stitch": "art_white", "lapel": "art_yellow"},
             hair="rolls", hair_paint="art_auburn")
    (hx, hy), _ = F.hand["fore"]
    c = F.P(0.95, 9.3)                                           # (the clover's middle, over her head)
    d.line("art_green", [(hx + 0.5, hy - 3.2), (hx, hy), lerp((hx, hy), c, 0.55), c], 1.5, taper=False)
    d.oval("art_skin", hx, hy + 0.3, 0.17 * F.k, 0.15 * F.k, ink=d.ink * 0.7)
    clover(d, c[0], c[1], 18.0, squash=0.5, turn=10.0)
    for (u, v) in ((8.0, 64.0), (3.0, 44.0), (61.0, 50.0)):   # (the rain beside her, none under it)
        d.fill("art_sky", [(u, v + 4.2), (u - 1.9, v), (u, v - 1.9), (u + 1.9, v)], steps=4)
    names(d, a, (93, 42), 15, lines=["Four", "Leaf", "Flo"], gap=1.36)


# ---- 50: the jitterbug ----------------------------------------------------------------------------------------------------
SADDLE = [(-0.2, 0.08), (0.1, 0.1), (0.36, -0.08), (0.56, -0.2), (0.62, -0.31), (0.52, -0.39), (-0.18, -0.39), (-0.27, -0.27), (-0.26, -0.04)]
SADDLE_BAND = [(-0.02, 0.095), (0.23, 0.0), (0.3, -0.385), (-0.06, -0.385)]


def saddle_leg(d, F, hip, knee, ank, ang=0.0, skin="art_skin", shoe="art_white", band="art_black", sock="art_white", cuff=0.74):
    """A leg in a bobby sock and a saddle shoe (white, its saddle dark), the shoe turned `ang` degrees (heads); `cuff`
    is where the sock's turned-down cuff begins, down the shin from the knee (1.0 at the ankle)."""
    fine = d.ink * 0.7
    R = F.r
    d.limb(skin, F.Ps([hip, lerp(hip, knee, 0.5), knee, lerp(knee, ank, 0.3), ank]), [R(r) for r in (0.4, 0.31, 0.185, 0.225, 0.11)], ink=fine)
    d.limb(sock, F.Ps([lerp(knee, ank, cuff + 0.04), lerp(knee, ank, (cuff + 1.0) / 2 + 0.05), ank]), [R(0.17), R(0.16), R(0.15)], ink=fine)
    d.limb(sock, F.Ps([lerp(knee, ank, cuff), lerp(knee, ank, cuff + 0.1)]), [R(0.2), R(0.2)], ink=fine)
    d.fill(shoe, F.Ps(_local(ank, ang, SADDLE)), ink=fine, steps=3)
    d.shape(band, F.Ps(_local(ank, ang, SADDLE_BAND)), ink=fine * 0.8)


@painting(50)
def p50(d, a):   # a woman in saddle shoes and a full skirt in the middle of a swing dance
    x, y, h = 30.0, 4.0, 70.0
    F = Fig(d, x, y, h, 1)
    i0 = len(d.ops)
    hair, top, skirt = "art_blond", "art_white", "art_lred"
    # (her ponytail, flying out behind her head)
    d.limb(hair, F.Ps([(-0.3, 7.32), (-0.9, 7.5), (-1.45, 7.36), (-1.86, 6.98)]), [F.r(0.2), F.r(0.23), F.r(0.17), F.r(0.06)], ink=d.ink * 0.7)
    # (her legs: she stands on her aft foot and kicks with her fore, knee up)
    saddle_leg(d, F, (-0.34, 3.8), (-0.4, 2.06), (-0.48, 0.42))
    saddle_leg(d, F, (0.34, 3.8), (1.42, 3.3), (2.5, 2.62), ang=-18.0, cuff=0.84)     # (a little shin between her hem and her sock)
    # (her full skirt, flying: lifted over her knee, swinging out behind)
    hem = [(-2.2, 2.4), (-1.66, 2.06), (-1.1, 2.3), (-0.5, 2.06), (0.12, 2.36), (0.72, 2.32), (1.26, 2.76), (1.74, 2.82), (2.2, 3.3)]
    d.fill("art_white", F.Ps([(-0.5, 4.6), (-2.0, 2.6)] + [(u, v - 0.16) for u, v in hem[1:-1]] + [(2.04, 3.3), (0.5, 4.6)]), ink=d.ink * 0.7, steps=3)
    d.fill(skirt, F.Ps([(-0.52, 4.7), (-0.92, 4.2)] + hem + [(0.98, 4.2), (0.52, 4.7)]), steps=4)
    for (u0, u1, v1) in ((-0.3, -1.1, 2.4), (0.0, -0.3, 2.3), (0.3, 0.74, 2.5)):
        d.line("art_ink", F.Ps([(u0, 4.4), ((u0 + u1) / 2, (4.4 + v1) / 2), (u1, v1)]), F.r(0.035))
    for run in ([(-1.7, 3.9), (-2.16, 3.3), (-2.5, 2.66)], [(-2.3, 3.7), (-2.66, 3.18), (-2.88, 2.74)]):   # (its swing behind it)
        d.line("art_white", F.Ps(run), F.r(0.06))
    G = lady(d, x, y, h, 1, pose="stand", arms=("up_out", "lean"), legs_shown=False,
         wear={"top": top, "skirt": None, "sleeves": "short", "belt": "art_red", "buckle": "art_red", "collar": "art_white"},
         hair="none", hair_paint=hair)
    (hx, hy), (ux, uy) = G.hand["fore"]                          # (her raised hand, painted again bigger over lady()'s)
    ang = math.degrees(math.atan2(uy, ux))
    d.oval("art_skin", hx, hy, F.r(0.23), F.r(0.185), turn=ang, ink=d.ink * 0.7)
    d.oval("art_skin", hx - uy * F.r(0.14), hy + ux * F.r(0.14), F.r(0.1), F.r(0.065), turn=ang + 50.0, ink=d.ink * 0.56)
    # (her hair over her head, a ribbon on her ponytail)
    d.fill(hair, F.Ps([(-0.47, 6.72), (-0.52, 7.2), (-0.32, 7.48), (0.05, 7.52), (0.38, 7.44), (0.52, 7.15), (0.48, 6.8),
                       (0.34, 7.1), (0.0, 7.2), (-0.32, 7.08)]), ink=d.ink * 0.7)
    d.line("art_ink", F.Ps([(0.1, 7.48), (-0.05, 7.33), (-0.3, 7.24)]), F.r(0.03))
    d.oval("art_red", *F.P(-0.42, 7.36), F.r(0.12), F.r(0.09), ink=d.ink * 0.6)
    _tilt(d, i0, F.P(-0.48, 0.0), 6.0)
    names(d, a, (100, 44), 15, lines=["Jitterbug", "Jenny"], gap=1.4, indent=6)


# ---- 51: the stoop -----------------------------------------------------------------------------------------------------------
def stoop(d, x0, y0, rise, tread, n=4, top=None, stone="art_lbrown", lip="art_tan"):
    """Brownstone steps side on, rising to the left from (x0, y0) (the foot of the lowest riser): n steps `rise` high
    and `tread` deep, the top one the landing (`top` deep); one shape, each tread's stone nosing jutting over its riser."""
    top = top or tread * 1.3
    pts = [(x0, y0)]
    for i in range(n):
        xx = x0 - i * tread
        pts += [(xx, y0 + (i + 1) * rise), (xx - (tread if i < n - 1 else top), y0 + (i + 1) * rise)]
    pts += [(pts[-1][0], y0)]
    d.shape(stone, pts)
    fine = d.ink * 0.75
    for i in range(n):
        xx = x0 - i * tread
        yy = y0 + (i + 1) * rise
        back = xx - (tread if i < n - 1 else top)
        d.shape(lip, [(xx + 1.2, yy + 0.2), (xx + 1.2, yy - 2.2), (back + 0.2, yy - 2.2), (back + 0.2, yy + 0.2)], ink=fine)
    return pts


def open_hand(d, wrist, ang, size=1.0, skin="art_skin", ink=None):
    """A hand held open, palm down, seen from its back: its four fingers spread in a fan and its thumb out to the
    side, one outline round it all. From `wrist`, pointing `ang` degrees (-90 straight down), `size` its scale (1.0
    about five units long); its thumb on the right of the way it points. Returns the tip of its middle finger."""
    fine = d.ink * 0.7 if ink is None else ink

    def edge(b, th, L, w):
        u = (math.sin(math.radians(th)), math.cos(math.radians(th)))
        n = (u[1], -u[0])
        return b, u, n, L, w

    def at(b, u, n, a_, b_):
        return (b[0] + u[0] * a_ + n[0] * b_, b[1] + u[1] * a_ + n[1] * b_)

    def cross(p0, d0, p1, d1):                                   # (where two finger edges, run back, meet: the crotch between them)
        den = d0[0] * d1[1] - d0[1] * d1[0]
        if abs(den) < 1e-9:
            return lerp(p0, p1, 0.5)
        t = ((p1[0] - p0[0]) * d1[1] - (p1[1] - p0[1]) * d1[0]) / den
        return (p0[0] + d0[0] * t, p0[1] + d0[1] * t)

    thumb = edge((1.05, 0.9), 64.0, 1.75, 0.42)
    fingers = [edge((1.0, 2.0), 20.0, 1.95, 0.42), edge((0.34, 2.1), 6.0, 2.25, 0.42), edge((-0.32, 2.05), -8.0, 2.1, 0.41),
               edge((-0.95, 1.9), -22.0, 1.6, 0.37)]
    pts = [(0.98, -0.5), (1.1, 0.3)]
    b, u, n, L, w = thumb
    pts += [at(b, u, n, 0.0, w), at(b, u, n, L, w)]
    pts += [at(b, u, n, L + w * math.sin(f), w * math.cos(f)) for f in (math.pi * i / 8 for i in range(1, 8))]
    pts += [at(b, u, n, L, -w), at(b, u, n, 0.35, -w)]
    for i, (b, u, n, L, w) in enumerate(fingers):
        if i == 0:
            pts.append(at(b, u, n, -0.15, w))
        else:
            pb, pu, pn, _, pw = fingers[i - 1]
            pts.append(cross(at(pb, pu, pn, 0.0, -pw), pu, at(b, u, n, 0.0, w), u))
        pts += [at(b, u, n, L, w)]
        pts += [at(b, u, n, L + w * math.sin(f), w * math.cos(f)) for f in (math.pi * i / 8 for i in range(1, 8))]
        pts += [at(b, u, n, L, -w)]
    b, u, n, L, w = fingers[-1]
    pts += [at(b, u, n, -0.1, -w), (-1.38, 1.0), (-1.18, 0.2), (-0.98, -0.5)]
    d.shape(skin, _local(wrist, ang - 90.0, pts, size), ink=fine)
    b, u, n, L, w = fingers[1]
    return _local(wrist, ang - 90.0, [at(b, u, n, L + w, 0.0)], size)[0]


@painting(51)
def p51(d, a):   # a city kid on brownstone steps bouncing a ball
    rise, tread, x0 = 12.0, 13.5, 46.0
    stoop(d, x0, 0.0, rise, tread, n=4, top=12.0)
    h = 60.0
    legs = ([(0.42, 3.8), (0.38, 2.08), (0.3, 0.42)], [(-0.42, 3.8), (-0.38, 2.1), (-0.32, 0.44)])   # (his feet close: both on the tread)
    F = kid(d, 39.3, rise, h, 1, pose=legs, arms=(to(2.5, 4.5, hand=False), "pocket"), tilt=8.0,
            wear={"shirt": "art_white", "stripes": "art_red", "knickers": "art_navy", "socks": "art_lgrey", "shoes": "art_dark", "cap": "art_grey"},
            hat="newsboy", hair="short", mouth="grin")
    tx, ty = open_hand(d, F.P(2.5, 4.5), -80.0, 1.2)             # (his hand open over the ball, palm down, fingers spread)
    bx, by = tx + 0.2, ty - 6.2                                  # (the ball, on its way back up off the pavement to his hand)
    for dx in (-1.6, 1.6):
        d.line("art_white", [(bx + dx, by - 5.0), (bx + dx, by - 9.0)], 0.7)
    d.oval("art_pink", bx, by, 3.7, 3.7)
    d.line("art_white", [(bx - 2.1, by + 1.1), (bx - 0.9, by + 2.3)], 0.7)
    names(d, a, (100, 44), 15, lines=["Stoop", "Ball", "Champ"], gap=1.38)


# ---- 52: the skater -----------------------------------------------------------------------------------------------------------
SKATE = [(-0.2, 0.1), (0.12, 0.12), (0.34, -0.06), (0.52, -0.16), (0.58, -0.3), (0.46, -0.38), (-0.18, -0.38), (-0.26, -0.26), (-0.25, 0.0)]


def skate_leg(d, F, hip, knee, ank, ang=0.0, back=False, skin="art_skin", boot="art_white", blade="art_lsteel"):
    """A leg in a skating boot, its blade under it with its toe curled up, the boot turned `ang` degrees (heads);
    back=True points its toe behind her (a free leg stretched out behind, its foot pointed), its blade still under it."""
    fine = d.ink * 0.7
    R = F.r
    m = -1.0 if back else 1.0

    def B(pts):
        return F.Ps(_local(ank, ang, [(m * u, v) for u, v in pts]))

    d.limb(skin, F.Ps([hip, lerp(hip, knee, 0.5), knee, lerp(knee, ank, 0.3), ank]), [R(r) for r in (0.4, 0.31, 0.185, 0.225, 0.11)], ink=fine)
    d.limb(boot, F.Ps([lerp(knee, ank, 0.5), lerp(knee, ank, 0.8), ank]), [R(0.22), R(0.2), R(0.18)], ink=fine)
    d.fill(boot, B(SKATE), ink=fine, steps=3)
    for u in (-0.1, 0.38):
        d.line("art_ink", B([(u, -0.38), (u, -0.52)]), R(0.05), taper=False)
    d.limb(blade, B([(-0.3, -0.54), (0.3, -0.55), (0.7, -0.52), (0.82, -0.42)]), [R(0.05)] * 4, ink=fine * 0.8)


@painting(52)
def p52(d, a):   # a woman in a winter coat and muff skating on a frozen river
    # (the river, frozen: a long band of ice, the lines her blades have cut in it; the far bank's drift of snow ahead of
    #  her, ending short of her, so that her white boot stands against the ice and the olive drab, not the snow)
    d.fill("art_white", [(42.6, 6.0), (44.6, 9.6), (50.0, 11.4), (57.0, 12.4), (63.0, 10.0), (66.0, 6.0), (62.0, 3.0), (44.0, 3.4)], steps=4)
    d.fill("art_sky", [(-2.0, 2.4), (0.4, 6.6), (34.0, 7.6), (64.0, 6.9), (69.0, 3.0), (66.0, -1.8), (34.0, -3.2), (1.0, -2.8), (-1.6, -0.6)], steps=4)
    d.line("art_white", [(62.0, 3.2), (50.0, 4.0), (38.0, 2.4), (34.0, 0.6), (42.0, 0.2), (50.0, 1.6)], 0.9)
    x, y, h = 30.0, 5.0, 66.0
    F = Fig(d, x, y, h, 1)
    i0 = len(d.ops)
    fine = d.ink * 0.7
    coat = "art_lred"
    skate_leg(d, F, (-0.36, 3.8), (-1.3, 3.22), (-2.3, 2.94), ang=12.0, back=True)
    skate_leg(d, F, (0.36, 3.8), (0.5, 2.1), (0.42, 0.58))
    # (her coat's skirt, swung by her lifted leg: its back hem flared up over her thigh in one tented fold, her leg coming
    #  out from under it; to her calf in front)
    d.fill(coat, F.Ps([(-0.52, 4.6), (-0.8, 4.24), (-1.0, 3.96), (-1.14, 3.66), (-1.2, 3.36), (-1.12, 3.0), (-0.98, 2.56), (-0.76, 2.1),
                       (-0.4, 1.8), (0.1, 1.7), (0.6, 1.72), (1.03, 1.71), (0.9, 2.6), (0.86, 3.98), (0.52, 4.6)]), steps=4)
    d.line("art_ink", F.Ps([(-0.92, 3.66), (-0.82, 2.92), (-0.62, 2.12)]), F.r(0.04))
    d.line("art_ink", F.Ps([(0.14, 4.6), (0.2, 3.0), (0.26, 1.74)]), F.r(0.04))
    G = lady(d, x, y, h, 1, pose="stand", arms=("hold", "hold"), legs_shown=False,
             wear={"top": coat, "skirt": None, "neck": "v", "sleeves": "long", "hat": "art_white", "belt": coat, "buckle": coat},
             hair="rolls", hair_paint="art_hair", hat="beret")
    for sx in (-1, 1):                                           # (her coat's lapels and its buttons, as lady()'s coat has them)
        d.shape(coat, F.Ps([(sx * 0.21, 6.14), (sx * 0.52, 6.1), (sx * 0.44, 5.72), (sx * 0.31, 5.68), (sx * 0.03, 5.12)]), ink=fine)
    for (u, v) in ((0.15, 5.42), (0.15, 5.06), (0.19, 4.24), (0.19, 3.88)):
        d.oval("art_white", *F.P(u, v), F.r(0.055), F.r(0.055), ink=fine * 0.6)
    # (her fur collar, round her neck under her chin)
    d.blobs("art_white", [(*F.P(u, v), F.r(rr)) for u, v, rr in ((-0.62, 5.9, 0.24), (-0.3, 6.04, 0.25), (0.06, 6.02, 0.24), (0.42, 6.0, 0.25), (0.7, 5.86, 0.22))])
    # (her muff, a fur barrel, her hands in it)
    (hx, hy), _ = G.hand["fore"]
    (ax, ay), _ = G.hand["aft"]
    cx, cy = (hx + ax) / 2, (hy + ay) / 2
    d.blobs("art_white", [(cx - 2.9, cy, 2.5), (cx - 1.0, cy + 0.4, 2.9), (cx + 1.0, cy + 0.4, 2.9), (cx + 2.9, cy, 2.5)])
    for u in (-1.9, 0.0, 1.9):
        d.line("art_lgrey", [(cx + u - 0.3, cy + 1.6), (cx + u + 0.2, cy), (cx + u - 0.2, cy - 1.6)], 0.45)
    _tilt(d, i0, F.P(0.42, 0.0), -8.0)
    names(d, a, (93, 46), 15, lines=["Mohawk", "Valley", "Mae"], gap=1.36)
