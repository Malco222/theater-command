"""Nose art, batch 2, numbers 32 to 38 (painter's group b2_b): a jackalope at a crossing post, a winged coin, a bandsman
with his trombone, a sandlot batter, a stork with its bundle, a short-order cook and a pup in a flying helmet.

Each painting is drawn as batch 1's are (noseart.py's module docstring): flat shapes on a grid about 100 wide, y up,
the painter's edge and wear added by the house hand, the name lettered by names() in its listed lettering. The things
the house kit lacks (a crossing post, a jackalope, a coin with an airman's head, a trombone, a stork, a skillet, a pup)
are drawn here in the same hand.
"""
from noseart import *  # noqa: F401,F403
from noseart_more._kit import *  # noqa: F401,F403
from noseart_more._kit import _HELMET  # noqa: E402  (the helmet's own strap and earphone, read for 38's lining and plain cup)

import math  # noqa: E402


def _rot(pts, cx, cy, deg):
    c, s_ = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return [(cx + (x - cx) * c - (y - cy) * s_, cy + (x - cx) * s_ + (y - cy) * c) for x, y in pts]


def _leaf(base, tip, w, bulge=0.45, blunt=0.5):
    """A leaf's outline (an ear, a feather) from `base` to `tip`, `w` its half width at `bulge` of its length; `blunt`
    how round its tip is."""
    (bx, by), (tx, ty) = base, tip
    L = math.hypot(tx - bx, ty - by) or 1.0
    ux, uy = (tx - bx) / L, (ty - by) / L
    nx, ny = -uy, ux
    prof = [(0.0, 0.72), (0.2, 0.92), (bulge, 1.0), (0.72, 0.86), (0.88, 0.58 * blunt + 0.3), (0.97, 0.3 * blunt + 0.08)]
    left = [(bx + ux * L * t + nx * w * f, by + uy * L * t + ny * w * f) for t, f in prof]
    right = [(bx + ux * L * t - nx * w * f, by + uy * L * t - ny * w * f) for t, f in reversed(prof)]
    return left + [(tx, ty)] + right


# ---- 32: the jackalope and the crossing post ----------------------------------------------------------------------------
def crossbuck(d, x, y, h, span=24.0, board=3.8, post="art_lbrown", paint="art_white", slope=36.0):
    """A plain railroad crossing post: a wooden post `h` tall from (x, y), its two crossed boards at its top, no
    lettering on them."""
    top = y + h
    cy = top - span * 0.5 * math.sin(math.radians(slope)) - board * 0.6
    d.shape(post, [(x - 1.5, y), (x + 1.5, y), (x + 1.3, top), (x - 1.3, top)])
    for sg in (-1, 1):
        a = math.radians(slope * sg)
        ux, uy = math.cos(a), math.sin(a)
        nx, ny = -uy, ux
        L = span / 2
        d.shape(paint, [(x - ux * L + nx * board / 2, cy - uy * L + ny * board / 2), (x + ux * L + nx * board / 2, cy + uy * L + ny * board / 2),
                        (x + ux * L - nx * board / 2, cy + uy * L - ny * board / 2), (x - ux * L - nx * board / 2, cy - uy * L - ny * board / 2)])
    d.oval("art_dark", x, cy, 0.9, 0.9, ink=d.ink * 0.5)
    return cy


def jackalope(d, x, y, size, s=1, coat="art_tan", belly="art_cream", horn="art_cream"):
    """A jackrabbit with a pair of small antlers, sitting on its haunches, its forelegs straight down, its long ears
    up: the postcard jackalope, side on, waiting; its hind foot's middle at (x, y), `size` tall to its ear tips,
    facing s. Returns its frame."""
    k = size / 64.0

    def P(u, v):
        return (x + s * u * k, y + v * k)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    fine = d.ink * 0.75
    # (the far ear, the far antler and the far foreleg, behind it)
    d.fill(coat, Q(_leaf((3.2, 41.0), (-3.4, 62.4), 2.7, 0.42)), ink=fine)
    d.fill("art_black", Q(_leaf((-2.0, 58.6), (-3.4, 62.4), 2.0, 0.15)), ink=0.0)
    d.limb(horn, Q([(8.6, 42.0), (8.2, 48.0), (7.4, 54.0), (7.8, 60.2)]), [1.35 * k, 1.15 * k, 0.9 * k, 0.6 * k], ink=fine)
    d.limb(horn, Q([(8.0, 49.4), (5.0, 52.4), (4.4, 55.0)]), [0.85 * k, 0.65 * k, 0.45 * k], ink=fine)
    d.limb(horn, Q([(7.5, 55.2), (9.8, 58.4)]), [0.7 * k, 0.45 * k], ink=fine)
    d.limb(coat, Q([(4.2, 17.0), (4.6, 8.0), (5.0, 1.4)]), [1.9 * k, 1.5 * k, 1.3 * k], ink=fine)
    d.oval(coat, *P(6.2, 1.0), 2.3 * k, 1.2 * k, ink=fine)
    # (its body: a round back over its haunch, its chest up to its neck)
    body = [(6.0, 5.0), (7.6, 12.0), (8.4, 19.0), (10.0, 25.6), (11.6, 31.0), (7.0, 36.6), (2.0, 32.4), (-3.4, 28.6), (-9.4, 27.8),
            (-15.4, 25.6), (-21.0, 21.2), (-23.8, 15.0), (-23.6, 8.0), (-20.6, 3.0), (-14.0, 1.2), (-4.0, 1.8), (2.0, 3.2)]
    d.fill(coat, Q(body))
    d.fill(belly, Q([(8.2, 16.0), (9.4, 22.6), (11.0, 28.6), (12.6, 31.4), (9.6, 31.6), (7.0, 25.0), (6.4, 18.0)]), ink=0.0)
    # (its haunch, its long hind foot flat under it, its white tail: the haunch kept well inside the rump, so that the
    #  rump shows round it as a clear band, not a sliver between two edges)
    d.oval(coat, *P(-11.6, 11.6), 10.4 * k, 9.6 * k, turn=-12 * s)
    d.fill(coat, Q([(-23.0, 3.4), (-17.0, 4.8), (-6.0, 4.4), (1.4, 3.6), (4.4, 1.8), (3.6, 0.0), (-21.0, 0.0), (-23.6, 1.4)]))
    d.line("art_ink", Q([(-0.4, 2.8), (1.6, 1.0)]), 0.3 * k)
    d.oval("art_white", *P(-23.4, 16.6), 3.1 * k, 2.9 * k)
    # (its near foreleg, straight down to the ground)
    d.limb(coat, Q([(7.4, 18.0), (8.0, 9.0), (8.6, 1.6)]), [2.0 * k, 1.6 * k, 1.4 * k], ink=fine)
    d.oval(coat, *P(9.8, 1.1), 2.4 * k, 1.25 * k, ink=fine)
    # (its head: long, its muzzle forward, its eye looking out down the line)
    head = [(4.4, 34.0), (5.2, 39.0), (8.2, 42.2), (12.6, 43.2), (16.6, 41.8), (19.8, 38.8), (21.4, 35.6), (20.6, 33.2), (17.4, 31.6),
            (12.2, 30.4), (7.4, 30.4)]
    d.fill(coat, Q(head))
    d.oval(belly, *P(17.4, 33.6), 3.2 * k, 2.3 * k, turn=-8 * s, ink=0.0)
    d.oval("art_pink", *P(20.9, 36.0), 1.0 * k, 0.85 * k, ink=fine * 0.6)
    d.line("art_ink", Q([(20.6, 34.4), (19.6, 33.2), (18.0, 33.4)]), 0.34 * k, taper=False)
    d.oval("art_white", *P(14.4, 38.0), 1.75 * k, 1.5 * k, ink=fine * 0.7)
    d.oval("art_ink", *P(15.2, 37.9), 0.95 * k, 1.1 * k, ink=0.0)
    d.oval("art_white", *P(15.5, 38.4), 0.3 * k, 0.3 * k, ink=0.0)
    d.line("art_ink", Q([(12.4, 40.6), (14.4, 41.2), (16.6, 40.6)]), 0.4 * k)
    # (the near ear: long, black-tipped, pink inside)
    d.fill(coat, Q(_leaf((5.6, 40.6), (0.8, 64.0), 3.0, 0.45)))
    d.fill("art_pink", Q(_leaf((5.0, 42.6), (1.3, 60.2), 1.4, 0.45)), ink=0.0)
    d.fill("art_black", Q(_leaf((1.6, 60.0), (0.8, 64.0), 2.2, 0.15)), ink=0.0)
    # (the near antler: a short beam, its tines forward)
    d.limb(horn, Q([(10.8, 42.2), (11.6, 47.6), (13.2, 53.2), (14.0, 59.6)]), [1.45 * k, 1.25 * k, 1.0 * k, 0.62 * k], ink=fine)
    d.limb(horn, Q([(11.6, 46.6), (15.0, 48.8), (17.6, 51.8)]), [1.0 * k, 0.8 * k, 0.55 * k], ink=fine)
    d.limb(horn, Q([(13.0, 52.6), (16.6, 55.4), (18.2, 58.2)]), [0.85 * k, 0.65 * k, 0.45 * k], ink=fine)
    d.limb(horn, Q([(13.9, 58.0), (12.0, 61.6)]), [0.6 * k, 0.4 * k], ink=fine)
    return P


@painting(32)
def p32(d, a):   # a jackrabbit with antlers waiting beside a plain crossing post
    crossbuck(d, 9.0, 0.0, 90.0, span=23.0, board=3.9)
    jackalope(d, 38.5, 0.0, 67.0, -1)
    d.line("art_lsteel", [(-2.0, 0.6), (30.0, 0.3), (63.0, 0.6)], 1.3, taper=False)
    names(d, a, (100.0, 74.0), 15, lines=split2(a["name"]), gap=1.45, indent=8.0)    # (half a capital clear of the ears and the boards)


# ---- 33: the coin, heads up, spinning, with small wings ----------------------------------------------------------------
def small_wing(d, root, side=1, size=1.0, turn=0.0, paint="art_white", rise=0.0):
    """A small bird's wing, as an emblem carries one: four pointed feathers fanned up and out from `root` (out toward
    `side`), the top one longest, their roots under a rounded shoulder; `rise` sweeps them up (degrees)."""
    rx, ry = root
    for ang, ln in ((10, 8.0), (29, 10.6), (48, 13.2), (67, 15.6)):
        a_ = math.radians(ang + rise + turn * side)
        ux, uy = side * math.cos(a_), math.sin(a_)
        tip = (rx + ux * ln * size, ry + uy * ln * size)
        base = (rx + ux * 1.5 * size, ry + uy * 1.5 * size)
        d.fill(paint, _leaf(base, tip, 2.0 * size, 0.42, blunt=0.25), ink=d.ink * 0.85, steps=4)
    a_ = math.radians(40 + rise + turn * side)
    d.oval(paint, rx + side * math.cos(a_) * 2.6 * size, ry + math.sin(a_) * 2.6 * size, 4.6 * size, 3.4 * size, turn=(40 + rise + turn * side) * side,
           ink=d.ink * 0.85)


_BUST = [(-0.22, -0.76), (-0.2, -0.46), (-0.32, -0.34), (-0.46, -0.16), (-0.53, 0.06), (-0.5, 0.3), (-0.38, 0.48), (-0.18, 0.6),
         (0.06, 0.62), (0.24, 0.55), (0.33, 0.45), (0.4, 0.43), (0.47, 0.35), (0.47, 0.25), (0.39, 0.2), (0.37, 0.16), (0.405, 0.11),
         (0.385, 0.06), (0.41, 0.02), (0.53, -0.1), (0.45, -0.14), (0.465, -0.18), (0.435, -0.205), (0.46, -0.23), (0.42, -0.27),
         (0.45, -0.33), (0.38, -0.39), (0.25, -0.42), (0.22, -0.54), (0.32, -0.63), (0.26, -0.76)]


def coin(d, x, y, r, squash=0.8, turn=-14.0, thick=0.12, face="art_yellow", edge="art_gold", relief="art_cream"):
    """A coin flipped and spinning, heads up: its face (an airman's head in a flying helmet, its goggles up, in profile,
    raised), its rim, its milled edge's thickness showing below; (x, y) its middle, `r` its face's radius, tilted by
    `squash` and turned `turn` degrees."""
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v, dz=0.0):
        uu, vv = u * r, v * r * squash - dz * r
        return (x + uu * c - vv * s_, y + uu * s_ + vv * c)

    top = [P(math.cos(math.radians(t)), math.sin(math.radians(t))) for t in range(0, 181, 10)]
    low = [P(math.cos(math.radians(t)), math.sin(math.radians(t)), thick) for t in range(180, 361, 10)]
    d.shape(edge, top + low)
    for t in range(200, 345, 18):                        # (its milled edge)
        a0 = math.radians(t)
        d.line("art_ink", [P(math.cos(a0), math.sin(a0), 0.02), P(math.cos(a0), math.sin(a0), thick - 0.02)], r * 0.035, taper=False)
    d.shape(face, [P(math.cos(math.radians(t)), math.sin(math.radians(t))) for t in range(0, 360, 6)])
    d.shape(None, [P(0.86 * math.cos(math.radians(t)), 0.86 * math.sin(math.radians(t))) for t in range(0, 360, 6)], ink=d.ink * 0.6)
    # (the head, raised: one pale shape, its lines cut in)
    B = lambda u, v, dz=0.0: P(u * 1.1, (v + 0.07) * 1.1, dz)     # noqa: E731  (the head a little bigger, in the middle)
    d.fill(relief, [B(u, v) for u, v in _BUST], ink=d.ink * 0.85, steps=3)
    fine = d.ink * 0.55
    d.shape(relief, [B(0.36, 0.36), B(0.0, 0.42), B(-0.3, 0.38), B(-0.5, 0.27), B(-0.505, 0.2), B(-0.3, 0.3), B(0.0, 0.34), B(0.36, 0.28)],
            ink=fine)                                                                                                # (the goggles' band)
    d.oval(relief, *B(0.41, 0.31), r * 0.094, r * 0.132 * squash, turn=turn - 12, ink=fine * 1.3)                       # (a lens, up on his brow)
    d.oval(face, *B(0.415, 0.31), r * 0.05, r * 0.083 * squash, turn=turn - 12, ink=fine * 0.8)
    d.line("art_ink", [B(0.37, 0.165), B(0.27, 0.12), B(0.2, 0.0), B(0.18, -0.16), B(0.21, -0.3), B(0.3, -0.38), B(0.385, -0.395)],
           r * 0.035)                                                                                                # (the helmet round his face, its strap)
    d.oval(relief, *B(-0.14, -0.06), r * 0.12, r * 0.155 * squash, turn=turn, ink=fine)                                # (the earphone's cup)
    d.line("art_ink", [B(0.33, 0.07), B(0.375, 0.055)], r * 0.032, taper=False)                                       # (his eye)
    d.line("art_ink", [B(0.405, -0.205), B(0.44, -0.205)], r * 0.022, taper=False)                                    # (his mouth)
    d.line("art_ink", [B(0.22, -0.54), B(0.0, -0.58), B(-0.2, -0.52)], r * 0.035)                                     # (his collar)
    d.line("art_white", [P(-0.62, 0.62), P(-0.3, 0.83), P(0.05, 0.9)], r * 0.06)                                      # (its shine)
    return P


@painting(33)
def p33(d, a):   # a spinning coin, heads up, with small wings
    cx, cy, r = 36.0, 46.0, 32.0
    tilt = -12.0
    c, s_ = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
    for side in (-1, 1):                                 # (its small wings, at the ends of the axis it spins on: spread out, not swept up behind it)
        rx, ry = cx + side * r * 0.97 * c, cy + side * r * 0.97 * s_ + 1.0
        small_wing(d, (rx, ry), side, 1.42, turn=tilt * side, rise=30.0)
    coin(d, cx, cy, r, squash=0.82, turn=tilt)
    for a0, a1, rr in ((62, 118, 1.3), (242, 298, 1.32)):   # (it spins over and over)
        pts = [(cx + r * rr * math.cos(math.radians(t)) * c - r * rr * 0.82 * math.sin(math.radians(t)) * s_,
                cy + r * rr * math.cos(math.radians(t)) * s_ + r * rr * 0.82 * math.sin(math.radians(t)) * c) for t in range(a0, a1 + 1, 8)]
        d.line("art_white", pts, 1.3)
    names(d, a, (123.0, 46.0), 15, lines=["Heads", "We", "Win"], gap=1.36, indent=0)   # (three lines, as p13's: a taller block beside a bigger coin)


# ---- 34: the bandsman and his trombone -------------------------------------------------------------------------------------
class _Horn:
    """Where a trombone lies, side on, between a player's lips (`M`) and his slide hand (`H`), facing s: u along it
    from his lips, v up across it. The slide's two tubes run at v = 0 (into his lips) and v = -g, its stay in his hand
    on the line between them; `beyond` how far the slide runs on past his hand."""

    def __init__(self, M, H, s=1, g=3.4, beyond=10.0):
        self.M, self.s, self.g = M, s, g
        Dx, Dy = H[0] - M[0], H[1] - M[1]
        L = math.hypot(Dx, Dy) or 1.0
        ang = math.atan2(Dy, Dx) + s * math.asin(min(0.9, g / 2 / L))
        self.f = (math.cos(ang), math.sin(ang))
        self.n = (-self.f[1], self.f[0]) if s > 0 else (self.f[1], -self.f[0])
        self.hand = math.sqrt(max(L * L - (g / 2) ** 2, 0.0))
        self.long = self.hand + beyond

    def A(self, u, v):
        (mx, my), (fx, fy), (nx, ny) = self.M, self.f, self.n
        return (mx + fx * u + nx * v, my + fy * u + ny * v)


def horn_bell(d, T, at=24.0, b=4.6, R=5.8, flare=8.5, w=0.85, paint="art_yellow", shade="art_gold"):
    """The trombone's bell section (painted before its player, so that his head hides where it passes him): its tube
    from behind his head forward at v = b, its flare, its bell's mouth at `at`, and the brace down to the slide."""
    A = T.A
    d.limb(paint, [A(-1.5, b), A(at * 0.4, b), A(at - flare + 0.6, b)], [w, w, w])
    d.line(paint, [A(7.0, b - 0.4), A(7.0, 0.4)], w * 1.1, taper=False)            # (the brace to the slide)
    top, bot = [], []
    for i in range(11):
        t = i / 10.0
        rr = w + (R - w) * t ** 2.4
        top.append(A(at - flare + flare * t, b + rr))
        bot.append(A(at - flare + flare * t, b - rr))
    d.shape(paint, top + list(reversed(bot)))
    ang = math.degrees(math.atan2(T.f[1], T.f[0]))
    cx, cy = A(at, b)
    d.oval(paint, cx, cy, R * 0.34, R, turn=ang)
    ix, iy = A(at + R * 0.05, b)
    d.oval(shade, ix, iy, R * 0.22, R * 0.78, turn=ang, ink=d.ink * 0.55)
    d.line("art_white", [A(at - flare * 0.55, b + w * 0.2 + (R - w) * 0.12), A(at - flare * 0.15, b + R * 0.62), A(at - 0.6, b + R * 0.86)], 0.55)


def horn_slide(d, T, w=0.85, paint="art_yellow"):
    """The trombone's slide (painted after its player): its upper tube from his lips, its lower tube and their crook
    out past his hand, the stay his hand grips, the mouthpiece at his lips."""
    A, g, Ls = T.A, T.g, T.long
    a0 = math.atan2(T.n[1], T.n[0])
    c0 = A(Ls, -g / 2)
    R1, R0 = g / 2 + w, g / 2 - w
    sg = -1 if T.s > 0 else 1                              # (round the crook from the top tube to the bottom one)
    out = [A(-0.4, w), A(Ls, w)]
    out += [(c0[0] + R1 * math.cos(a0 + sg * math.pi * j / 12), c0[1] + R1 * math.sin(a0 + sg * math.pi * j / 12)) for j in range(1, 12)]
    out += [A(Ls, -g - w), A(6.0, -g - w), A(6.0, -g + w), A(Ls, -g + w)]
    out += [(c0[0] + R0 * math.cos(a0 + sg * math.pi * (12 - j) / 12), c0[1] + R0 * math.sin(a0 + sg * math.pi * (12 - j) / 12)) for j in range(1, 12)]
    out += [A(Ls, -w), A(-0.4, -w)]
    d.shape(paint, out)
    for u in (6.0, T.hand):                                # (the slide's two stays: by his lips, and in his hand)
        d.shape(paint, [A(u - 0.7, -0.2), A(u + 0.7, -0.2), A(u + 0.7, -g + 0.2), A(u - 0.7, -g + 0.2)], ink=d.ink * 0.7)
    d.line("art_white", [A(9.0, w * 0.3), A(Ls - 2.0, w * 0.3)], w * 0.55, taper=False)
    d.oval(paint, *A(-0.2, 0.0), 1.0, 1.25, ink=d.ink * 0.8)       # (the mouthpiece's cup at his lips)


@painting(34)
def p34(d, a):   # a bandsman in a bandstand jacket playing a trombone
    x0, h = 21.0, 74.0
    arms = (to(2.6, 6.45, "down"), "slide")
    wear = {"shirt": "art_white", "jacket": "art_red", "cut": "suit", "stripes": "art_cream", "bowtie": "art_navy", "trousers": "art_cream",
            "shoes": "art_brown", "hat": "art_tan", "band": "art_navy"}
    kw = dict(pose="stride", arms=arms, wear=wear, hat="boater", mouth="none", eyes="shut", wide=0.9, tilt=-4.0)
    G = man(Drawing(1), x0, 2, h, 1, **kw)                 # (where his lips and his slide hand will be)
    T = _Horn(G.mouth, G.hand["fore"][0], 1, g=3.4, beyond=11.0)
    horn_bell(d, T, at=T.hand - 2.0)
    F = man(d, x0, 2, h, 1, **kw)
    horn_slide(d, T)
    F.grip("fore")
    F.grip("aft")
    names(d, a, (78, 36), 14, lines=split2(a["name"]), gap=1.4, indent=8)


# ---- 35: the sandlot batter --------------------------------------------------------------------------------------------
def baseball(d, x, y, r, trail=(1, 0), long=9.0):
    """A pitched ball, its seams, three short lines trailing behind it."""
    tx, ty = trail
    for o in (-0.55, 0.0, 0.55):
        d.line("art_white", [(x + tx * r * 1.4 - ty * o * r * 1.3, y + ty * r * 1.4 + tx * o * r * 1.3),
                             (x + tx * (r * 1.4 + long * (1 - abs(o) * 0.4)) - ty * o * r * 1.3, y + ty * (r * 1.4 + long * (1 - abs(o) * 0.4)) + tx * o * r * 1.3)], r * 0.32)
    d.oval("art_white", x, y, r, r)
    d.line("art_red", ellipse(x - r * 1.25, y, r * 0.75, r * 0.82, 0, 12, -50, 50), r * 0.12, taper=False)
    d.line("art_red", ellipse(x + r * 1.25, y, r * 0.75, r * 0.82, 0, 12, 130, 230), r * 0.12, taper=False)


@painting(35)
def p35(d, a):   # a kid in a ball cap swinging at a pitch
    # (his hands out before him on his fore side, so that his lead arm hangs from its own shoulder and only his aft arm
    #  crosses his chest; the kit's "swing" holds them too close in)
    F = kid(d, 37, 2, 80, -1, pose="bat", arms=(to(1.7, 4.85, "down"), to(1.5, 4.7)),
            wear={"shirt": "art_white", "stripes": "art_red", "knickers": "art_khaki", "socks": "art_red", "shoes": "art_black", "cap": "art_red"},
            hat="ball", hair="short", mouth="grin")
    (hx, hy), (dx, dy) = F.hand["fore"]
    (ax, ay), _ = F.hand["aft"]
    bx, by = hx - 22.0, hy + 5.0                          # (a full-length bat, its barrel tipped up a little)
    d.limb("art_tan", [(ax + 1.6, ay - 0.8), ((ax + bx) / 2, (ay + by) / 2), (bx, by)], [0.9, 1.25, 1.85])
    F.grip("aft")
    F.grip("fore")
    baseball(d, bx + 0.5, by + 6.0, 2.9, trail=(-1, 0.35), long=4.0)   # (the pitch coming in just over the barrel's end)
    names(d, a, (107.0, 40.0), 15, lines=split2(a["name"]), gap=1.4, indent=6)


# ---- 36: the stubborn stork and its bundle (a cruder hand) ---------------------------------------------------------------------
def stork(d, x, y, size=1.0, s=1, tilt=-5.0, body="art_white", beak="art_red", dark="art_black"):
    """A cartoon white stork flying, side on, facing s: a plump body, its neck out, a long red beak clamped shut, its
    eye half shut under a low, set brow (stubborn), its near wing raised with black flight feathers, its red legs
    trailing; (x, y) its body's middle, `size` its scale (1: some 60 long, beak to toes), `tilt` its nose down (degrees).
    Returns the point its beak holds a thing by (a bundle's knot) and its frame."""
    c, s_ = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))

    def P(u, v):
        uu, vv = u * c + v * s_, -u * s_ + v * c
        return (x + s * uu * size, y + vv * size)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    k = size
    fine = d.ink * 0.85
    # (the far wing, up behind the near one: its black feathers only)
    d.fill(dark, Q([(-3.0, 6.0), (-6.0, 16.0), (-11.0, 24.0), (-17.0, 28.6), (-19.4, 27.0), (-18.6, 24.6), (-21.0, 23.2), (-19.6, 20.0),
                    (-21.4, 17.6), (-18.6, 14.6), (-17.0, 7.0)]), ink=fine, steps=3)
    # (its legs trailing straight back from under its tail, its feet)
    for dv in (0.0, -1.8):
        d.limb(beak, Q([(-7.0, -2.4 + dv), (-14.0, -1.8 + dv), (-20.0, -0.6 + dv)]), [0.95 * k, 0.8 * k, 0.75 * k], ink=fine)
        for tv in (1.3, -0.2, -1.5):
            d.limb(beak, Q([(-20.0, -0.6 + dv), (-23.2, -0.6 + dv + tv)]), [0.6 * k, 0.4 * k], ink=fine * 0.9)
    # (its neck, its tail, its body over their roots)
    d.limb(body, Q([(8.0, 1.8), (14.6, 5.0), (20.0, 7.6)]), [3.5 * k, 3.0 * k, 2.8 * k])
    d.fill(body, Q([(-9.0, 3.6), (-17.6, 5.6), (-20.4, 2.6), (-19.2, -0.8), (-10.0, -3.4)]), ink=fine, steps=3)
    d.fill(body, Q([(13.0, 1.6), (11.4, 5.4), (6.0, 7.8), (-2.0, 8.0), (-9.0, 5.8), (-13.4, 1.6), (-12.0, -3.4), (-6.0, -6.4), (2.0, -7.0),
                    (9.0, -4.8), (12.6, -1.6)]))
    # (its head)
    d.oval(body, *P(22.6, 8.6), 5.2 * k, 4.6 * k, turn=-tilt * s)
    # (its beak: long, red, shut tight)
    d.fill(beak, Q([(25.4, 11.6), (31.0, 9.6), (38.0, 6.4), (43.4, 3.4), (44.2, 2.4), (43.0, 2.0), (37.0, 3.4), (30.4, 5.0), (25.8, 6.0)]),
           ink=fine, steps=3)
    d.line("art_ink", Q([(26.6, 8.0), (32.0, 6.8), (38.0, 4.6), (43.6, 2.5)]), 0.36 * k, taper=False)
    # (its eye, half shut under a low set brow: it will deliver, come what may)
    ex, ey = 23.4, 10.0
    d.oval("art_white", *P(ex, ey), 2.0 * k, 1.8 * k, ink=fine * 0.8)
    d.oval("art_ink", *P(ex + 0.7, ey - 0.4), 1.05 * k, 1.15 * k, ink=0.0)
    d.oval("art_white", *P(ex + 1.0, ey - 0.1), 0.32 * k, 0.32 * k, ink=0.0)
    d.fill(body, Q([(ex - 2.6, ey + 0.4), (ex - 1.0, ey + 2.6), (ex + 2.6, ey + 2.6), (ex + 2.6, ey - 0.2), (ex + 0.4, ey + 0.5)]), ink=0.0)
    d.line("art_ink", Q([(ex - 2.1, ey + 0.55), (ex + 0.4, ey + 0.35), (ex + 2.3, ey - 0.3)]), 0.55 * k, taper=False)
    d.line("art_ink", Q([(ex - 2.4, ey + 2.6), (ex + 0.4, ey + 2.2), (ex + 2.8, ey + 1.0)]), 0.8 * k, taper=False)
    # (the near wing, raised: white over the shoulder, black flight feathers fanned at its tip and trailing edge)
    d.fill(dark, Q([(-1.0, 8.0), (-4.0, 17.0), (-7.4, 25.4), (-10.6, 31.4), (-12.6, 32.8), (-13.6, 31.0), (-15.6, 31.8), (-16.4, 29.6),
                    (-18.6, 29.6), (-18.6, 27.0), (-20.6, 26.2), (-19.6, 23.4), (-21.2, 21.6), (-18.2, 18.0), (-16.2, 11.0), (-12.0, 5.6)]),
           ink=fine, steps=3)
    d.fill(body, Q([(3.0, 5.6), (1.8, 12.0), (-1.4, 18.4), (-5.0, 23.6), (-8.2, 21.6), (-10.6, 14.4), (-11.0, 8.6), (-8.0, 4.4)]), ink=fine, steps=3)
    return P(42.4, 2.6), P


def bundle(d, knot, s=1, drop=17.0, long=22.0, cloth="art_white", bomb_paint="art_dark", band="art_yellow"):
    """A bomb slung in a cloth bundle, its corners knotted at `knot` (in a beak): its nose out of the bundle's front,
    its fins out of the back, the bundle hanging `drop` below the knot."""
    kx, ky = knot
    cy = ky - drop
    bomb(d, kx + s * long * 0.5, cy, long, turn=0 if s > 0 else 180, paint=bomb_paint, band=band)
    w, hb = long * 0.3, long * 0.36
    bag = [(kx - 1.0, ky - 1.4), (kx - w * 0.55, cy + hb * 0.55), (kx - w, cy + hb * 0.05), (kx - w * 0.92, cy - hb * 0.45), (kx - w * 0.5, cy - hb * 0.72),
           (kx, cy - hb * 0.78), (kx + w * 0.5, cy - hb * 0.72), (kx + w * 0.92, cy - hb * 0.45), (kx + w, cy + hb * 0.05), (kx + w * 0.55, cy + hb * 0.55),
           (kx + 1.0, ky - 1.4)]
    d.fill(cloth, bag, steps=4)
    d.line("art_ink", [(kx - 0.6, ky - 3.4), (kx - w * 0.3, cy), (kx - w * 0.36, cy - hb * 0.5)], 0.4)
    d.line("art_ink", [(kx + 0.7, ky - 4.0), (kx + w * 0.3, cy + 0.6), (kx + w * 0.4, cy - hb * 0.4)], 0.4)
    for sg in (-1, 1):                                    # (the knot's two ears)
        d.fill(cloth, [(kx, ky - 0.2), (kx + sg * 2.4, ky + 2.8), (kx + sg * 3.6, ky + 1.6), (kx + sg * 1.4, ky - 1.0)], ink=d.ink * 0.85, steps=3)
    d.oval(cloth, kx, ky - 0.4, 1.6, 1.4, ink=d.ink * 0.85)


@painting(36)
def p36(d, a):   # a stork carrying a bomb in a bundle sling
    knot, P = stork(d, 50.0, 52.0, 1.0, -1, tilt=-2.0)
    bundle(d, knot, -1, drop=18.0, long=25.0)
    names(d, a, (71.0, 19.5), 14, lines=split2(a["name"]), gap=1.4, indent=12)


# ---- 37: the short-order cook and his flapjack --------------------------------------------------------------------------
def skillet(d, hand, s=1, w=16.0, handle=9.0, rise=0.08):
    """A black cast-iron skillet held out level by its handle (gripped at `hand`), seen a little from above: its handle,
    its pan `w` across, the dark inside of its rim. Returns the middle of its rim."""
    hx, hy = hand

    def H(u, v):                                          # (u out along the handle from his grip, v up)
        return (hx + s * u, hy + u * rise + v)

    d.shape("art_black", [H(-1.6, 0.9), H(handle, 1.0), H(handle, -0.9), H(-1.6, -0.8)], ink=d.ink * 0.9)
    cx, cy = H(handle + w / 2 - 0.6, 1.4)
    d.shape("art_black", [(cx - s * w / 2, cy), (cx + s * w / 2, cy), (cx + s * w * 0.4, cy - 3.6), (cx - s * w * 0.4, cy - 3.6)])
    d.oval("art_black", cx, cy, w / 2, 1.9)
    d.oval("art_dark", cx, cy + 0.2, w / 2 - 1.2, 1.15, ink=0.0)
    d.line("art_lgrey", [(cx - s * w * 0.3, cy - 1.6), (cx + s * w * 0.28, cy - 1.6)], 0.45, taper=False)
    return cx, cy


def flapjack(d, x, y, r, turn=0.0):
    """A flapjack in the air, mid-flip: its browned edge, its golden top, a pat of butter on it."""
    d.oval("art_lbrown", x, y - r * 0.1, r, r * 0.46, turn=turn)
    d.oval("art_tan", x, y + r * 0.04, r * 0.9, r * 0.36, turn=turn, ink=d.ink * 0.5)
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    bx, by = x + r * 0.08 * c, y + r * 0.08 * s_ + r * 0.06
    d.shape("art_yellow", _rot([(bx - r * 0.2, by - r * 0.08), (bx + r * 0.2, by - r * 0.08), (bx + r * 0.22, by + r * 0.16), (bx - r * 0.18, by + r * 0.16)],
                              bx, by, turn), ink=d.ink * 0.6)


@painting(37)
def p37(d, a):   # a short-order cook in a paper hat flipping a pancake
    F = man(d, 22, 2, 74, 1, pose="stand", arms=("flip", "hip"),
            wear={"shirt": "art_sky", "sleeves": "rolled", "neck": "crew", "trousers": "art_grey", "apron": "art_white", "shoes": "art_black"},
            hat="paper", hair="short", wide=1.14, mouth="grin")
    (hx, hy), _ = F.hand["fore"]
    px, py = skillet(d, (hx, hy), 1)
    F.grip("fore")
    fx, fy = px + 1.0, py + 17.0
    p0, p1, p2 = (px + 8.5, py + 2.6), (px + 14.5, py + 9.5), (fx + 9.0, fy - 1.0)
    for t0 in (0.08, 0.38, 0.68):                         # (its flip: a dashed arc up from the pan's rim)
        pts = [tuple((1 - t) ** 2 * p0[j] + 2 * (1 - t) * t * p1[j] + t * t * p2[j] for j in (0, 1)) for t in (t0, t0 + 0.1, t0 + 0.2)]
        d.line("art_white", pts, 0.75)
    flapjack(d, fx, fy, 8.0, turn=-14)
    names(d, a, (80.0, 22.0), 14, lines=split2(a["name"]), gap=1.4, indent=8)    # (down and right, clear of the pan and his forearm)


# ---- 38: one big flying helmet, a pup peeking out of it -------------------------------------------------------------------
def pup(d, x, y, r, s=1, coat="art_white", ears="art_brown", patch="art_tan"):
    """A pup's head, three-quarter on, facing s: long floppy ears, a patch over one eye, a black nose, its tongue out;
    (x, y) the middle of its head, `r` its head's half width."""
    def P(u, v):
        return (x + s * u * r, y + v * r)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    fine = d.ink * 0.85
    d.fill(coat, Q([(-0.96, 0.1), (-0.9, 0.62), (-0.5, 0.94), (0.1, 1.0), (0.66, 0.84), (0.98, 0.4), (0.96, -0.2), (0.64, -0.62), (0.1, -0.8),
                    (-0.5, -0.66), (-0.9, -0.3)]))
    d.fill(patch, Q([(0.08, 0.86), (0.6, 0.78), (0.82, 0.42), (0.7, 0.12), (0.36, 0.08), (0.12, 0.34)]), ink=0.0)
    d.fill(coat, Q([(-0.1, -0.08), (0.34, -0.02), (0.76, -0.14), (0.9, -0.42), (0.66, -0.72), (0.16, -0.76), (-0.18, -0.5)]), ink=fine)
    d.fill("art_pink", Q([(0.2, -0.6), (0.44, -0.62), (0.5, -0.96), (0.36, -1.1), (0.2, -1.0), (0.16, -0.74)]), ink=fine)
    d.line("art_ink", Q([(0.33, -0.66), (0.34, -0.92)]), r * 0.04)
    d.line("art_ink", Q([(-0.02, -0.46), (0.18, -0.58), (0.36, -0.52), (0.56, -0.58), (0.74, -0.46)]), r * 0.06, taper=False)
    d.oval("art_black", *P(0.42, -0.2), r * 0.24, r * 0.17, ink=fine)
    d.oval("art_white", *P(0.36, -0.14), r * 0.07, r * 0.045, ink=0.0)
    for (u, v, big) in ((-0.22, 0.32, 0.9), (0.5, 0.36, 1.0)):
        d.oval("art_ink", *P(u, v), r * 0.13 * big, r * 0.16 * big, ink=0.0)
        d.oval("art_white", *P(u + 0.05, v + 0.06), r * 0.045, r * 0.05, ink=0.0)
    for (u, v, sg) in ((-0.88, 0.52, -1), (0.86, 0.5, 1)):     # (its ears, hanging)
        e = [(u - sg * 0.1, v + 0.16), (u + sg * 0.3, v + 0.08), (u + sg * 0.42, v - 0.4), (u + sg * 0.36, v - 1.02), (u + sg * 0.12, v - 1.18),
             (u - sg * 0.12, v - 0.9), (u - sg * 0.2, v - 0.3)]
        d.fill(ears, Q(e), ink=fine)
    return P


@painting(38)
def p38(d, a):   # one big flying helmet with a pup peeking out of it
    hx, hy, size = 30.0, 44.0, 56.0
    helmet(d, hx, hy, size, 1, paint="art_lbrown", part="inside")

    def H(u, v):
        return (hx + u * size, hy + v * size)

    d.fill("art_dark", [H(-0.15, -0.12), H(-0.15, 0.2), H(-0.06, 0.37), H(0.13, 0.43), H(0.36, 0.37), H(0.34, 0.08), H(0.1, -0.02)], ink=0.0)
    # (the lining carried on down to the chin strap's middle line, its sides under both flaps, so that no olive shows
    #  between the lining, the strap and the left flap's foot: the bottom one clean edge. The only edge of it that shows
    #  runs from the strap's buckle up to the far flap's foot; it is inked there, and the strap and flaps cover the rest)
    strap = smooth([H(u, v) for u, v in _HELMET["three"]["strap"]], False, 5)
    d.shape("art_dark", [H(-0.10, -0.30), H(-0.09, -0.46)] + strap + [H(0.43, -0.47), H(0.47, -0.30)], ink=0.0)
    side = [lerp(strap[-1], H(0.43, -0.47), t / 5.0) for t in range(6)]
    d._put("art_ink", d._edge(side, d.ink, closed=False)[:len(side) - 1])
    cx, cy = H(0.2, -0.36)
    d.oval("art_white", cx, cy, 10.5, 9.0, ink=d.ink * 0.85)       # (its chest, down to the strap)
    P = pup(d, *H(0.21, -0.06), 15.5, 1)
    helmet(d, hx, hy, size, 1, paint="art_lbrown", part="cap")
    cu, cv = _HELMET["three"]["cups"][0]                  # (the earphone's dark middle left off, the cup plain leather: at
    d.oval("art_lbrown", *H(cu, cv), size * 0.045, size * 0.062, ink=0.0)   # this size the dot read as a hole or an eye,
    #                                                       and a ringed button as a gunsight's rings where her seams cross it)
    for u in (0.1, 0.33):                                 # (its forepaws over the chin strap, holding on)
        px, py = H(u, -0.6)
        d.oval("art_white", px, py, 3.6, 2.7)
        for o in (-1.1, 0.0, 1.1):
            d.line("art_ink", [(px + o, py - 2.4), (px + o * 1.05, py - 0.7)], 0.32)
    names(d, a, (107.0, 42.0), 15, lines=split2(a["name"]), gap=1.4, indent=8)    # (half a capital clear of the far flap, its T's swash too)
