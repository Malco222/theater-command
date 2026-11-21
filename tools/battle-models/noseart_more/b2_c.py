"""Batch 2, numbers 39 to 45 (painter b2_c): a baby alligator hatching in a flying helmet, a rooster bombing with eggs,
a Dakota sunrise, a milkmaid riding a milk can on wheels, a pickle barrel target, a pair of eyes in the tail gunner's
window and a frog in a trench coat blowing a foghorn in thick fog. Each picture is drawn from flat shapes with
noseart's Drawing API; the helpers below (an eggshell's rim, a gator and its flying helmet, an egg, a basket, an ear of
wheat, a sunrise, a cart wheel, a milk can, a rail truck, a handkerchief, a fog bank, a frog, a trench coat, a
foghorn...) are built the same way, for these paintings."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more._kit import _band  # noqa: E402  (the kit's strap: even, edged, its end cut along the edge it goes round)


# ---- helpers ------------------------------------------------------------------------------------------------------------
def _frame(x, y, turn=0.0, k=1.0):
    """A local frame: (u, v) in `k` grid units, turned `turn` degrees about (x, y). P.k is its scale."""
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        return (x + (u * c - v * s_) * k, y + (u * s_ + v * c) * k)
    P.k = k
    return P


def _arc(cx, cy, rx, ry, a0, a1, n=16):
    """Points along an ellipse from a0 to a1 degrees."""
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
            for i in range(n + 1)]


def _leave(cx, cy, rx, ry, p, q):
    """Where the ray from p (inside the ellipse) toward q leaves it."""
    (px, py), (qx, qy) = p, q
    dx, dy = qx - px, qy - py
    ux, uy, vx, vy = (px - cx) / rx, (py - cy) / ry, dx / rx, dy / ry
    a, b, c = vx * vx + vy * vy, 2 * (ux * vx + uy * vy), ux * ux + uy * uy - 1.0
    t = (-b + math.sqrt(max(b * b - 4 * a * c, 0.0))) / (2 * a)
    return (px + dx * t, py + dy * t)


def passed_hand(d, *seeds):
    """Its name in the very hand the art director passed at the review: each line's seed as it was first drawn. A mend
    to the picture draws the painter's dice afresh before the name, and the lettering was not part of the mend."""
    for op, seed in zip([op for op in d.ops if op[0] == "text"], seeds):
        op[1]["seed"] = seed


# ---- 39: the hatchling ---------------------------------------------------------------------------------------------------
def shell_rim(cx, cy, rx, ry, rim, teeth=7, depth=3.4, lift=0.0, phase=0):
    """A broken eggshell's zigzag rim along y = rim (+ lift), from the egg's right side to its left."""
    dy = (rim - cy) / ry
    half = rx * math.sqrt(max(0.0, 1.0 - dy * dy))
    xr, xl = cx + half, cx - half
    pts = []
    for i in range(teeth * 2 + 1):
        t = i / (teeth * 2)
        up = (i + phase) % 2 == 1
        wob = 0.5 * math.sin(i * 2.3 + phase)
        pts.append((xr + (xl - xr) * t, rim + lift + (depth + wob if up else wob * 0.4)))
    pts[0] = (xr, rim + lift)
    pts[-1] = (xl, rim + lift)
    return pts


GATOR_HEAD = [(-8.4, -0.4), (-7.6, 4.4), (-4.6, 7.8), (-0.4, 9.0), (3.8, 8.4), (7.6, 6.6), (10.4, 6.0), (12.8, 5.8), (14.6, 6.1),
              (15.8, 6.5), (17.2, 6.2), (18.4, 5.0), (19.0, 2.6), (18.7, 0.1), (17.5, -1.6), (14.4, -2.9), (10.4, -4.2), (6.0, -5.8),
              (1.0, -7.4), (-3.0, -7.5), (-6.6, -5.0)]


def gator(d, x, y, turn=18.0, k=1.0, skin="art_lgreen", jaw="art_cream"):
    """A baby alligator's head in profile, from life: a big round baby's skull, a short broad snout with a round end,
    its snout up toward `turn` degrees; (x, y) the middle of its skull, `k` its size. Its eye is painted by
    gator_eye(), last. Returns its frame and its outline on the grid."""
    P = _frame(x, y, turn, k)
    head = d.fill(skin, [P(u, v) for u, v in GATOR_HEAD], steps=4)
    # (its pale lower jaw, its smile, a small tooth and a nostril)
    d.fill(jaw, [P(u, v) for u, v in [(18.3, -0.8), (15.0, -1.9), (10.4, -2.9), (6.4, -4.1), (2.6, -6.0), (-0.4, -7.3), (1.0, -7.4),
                                       (6.0, -5.8), (10.4, -4.2), (14.4, -2.9), (17.5, -1.6)]], ink=0.0, steps=3)
    d.shape("art_white", [P(12.4, -0.3), P(13.9, -0.45), P(13.1, -1.95)], ink=0.3)
    d.line("art_ink", [P(u, v) for u, v in [(18.9, 1.1), (15.4, 0.3), (11.4, -0.3), (8.4, -0.6), (6.6, -0.1), (5.7, 1.1)]], 0.62 * k)
    d.oval("art_ink", *P(16.0, 5.6), 0.62 * k, 0.46 * k, turn=turn, ink=0.0)
    return P, head


def gator_eye(d, P, turn, skin="art_lgreen", iris="art_yellow"):
    """Its eye high on its head, as a gator's is: a bump, a yellow eye, a dark upright pupil, a glint."""
    k = P.k
    d.oval(skin, *P(6.4, 7.6), 3.9 * k, 3.4 * k, turn=turn)
    d.oval(iris, *P(6.8, 7.9), 2.6 * k, 2.45 * k, turn=turn, ink=0.35)
    d.oval("art_ink", *P(7.4, 7.9), 0.9 * k, 1.95 * k, turn=turn, ink=0.0)
    d.oval("art_white", *P(6.1, 9.0), 0.62 * k, 0.56 * k, ink=0.0)


GATOR_CAP = [(4.2, 10.2), (1.6, 10.7), (-1.6, 10.7), (-4.8, 9.8), (-7.6, 7.7), (-9.3, 4.6), (-9.7, 1.2), (-9.4, -1.6),   # (over its crown)
             (-6.6, -2.0), (-3.7, -1.3),                                                                                      # (its nape)
             (-3.9, -3.2), (-2.8, -4.9), (-0.2, -5.4), (2.2, -4.7), (3.1, -2.8), (3.0, -0.4),                                  # (the ear flap)
             (3.1, 2.2), (3.6, 4.4), (4.0, 5.4)]                                                                                # (its brow edge)


def gator_cap(d, P, turn, paint="art_lbrown", cup="art_brown", band="art_black", rim="art_lsteel", lens="art_sky",
              lenses=((-0.4, 8.6, 1.85), (-3.2, 8.0, 2.2)), strap=((-5.2, 7.7), (-6.0, 7.5), (-6.8, 7.2)), head=None):
    """A close leather flying helmet on the gator's skull (its frame P), as the owl's in 12: the cap over its whole
    crown and the back of its head, its brow edge just behind the eye's bump (the bump, painted after, laps over it); an
    ear flap hanging over the jaw's hinge with the round cup of its earphone on it; a short chin strap from the flap under
    the throat; and its goggles pushed up on the crown, behind the eye, their band running back round the crown to the
    leather's edge and on out of sight."""
    k, w0 = P.k, d.ink
    if head:                                                  # (the chin strap: from the flap's foot round under the throat)
        _band(d, paint, [P(0.2, -4.4), P(0.6, -5.6), P(1.0, -6.6)], 0.62 * k, w0 * 0.55, within=head, reach=(False, True))
    cap = d.fill(paint, [P(u, v) for u, v in GATOR_CAP], steps=4)
    d.oval(cup, *P(-0.4, -2.5), 1.9 * k, 2.0 * k, turn=turn, ink=w0 * 0.75)                     # (the earphone's cup)
    d.line(paint, [P(-1.5, -1.2), P(-0.5, -0.9), P(0.5, -1.3)], 0.5 * k)                        # (the light on its rim)
    if strap:                                                 # (the goggles' band, back round the crown, cut at the cap's edge)
        _band(d, band, [P(u, v) for u, v in strap], 0.45 * k, w0 * 0.5, within=cap, reach=(False, True))
    for lu, lv, r in lenses:                                  # (the far lens first, then the near one over it)
        d.oval(rim, *P(lu, lv), r * k, r * 0.92 * k, turn=turn, ink=w0 * 0.9)
        d.oval(lens, *P(lu, lv), r * 0.64 * k, r * 0.58 * k, turn=turn, ink=w0 * 0.45)
        d.line("art_white", [P(lu - r * 0.35, lv + r * 0.15), P(lu - r * 0.05, lv + r * 0.36)], 0.42 * k)
    return cap


@painting(39)
def p39(d, a):   # a baby alligator in a flying helmet hatching from its egg
    cx, cy, rx, ry, rim = 24.0, 22.0, 19.5, 23.0, 31.0
    tilt = 18.0
    # (the shell's inside, in shadow, behind the rim: the far half of the broken egg)
    back = shell_rim(cx, cy, rx, ry, rim, teeth=6, depth=2.8, lift=1.6, phase=1)
    d.shape("art_lgrey", list(reversed(back)) + [(back[0][0], rim - 5), (back[-1][0], rim - 5)])
    # (the hatchling's neck, rising out of it, and its arms reaching for the rim)
    d.limb("art_lgreen", [(21.0, 22.0), (21.4, 33.0), (22.0, 41.0)], [7.4, 7.2, 6.8])
    d.limb("art_lgreen", [(32.5, 22.0), (33.4, 28.0), (34.6, 31.4)], [2.1, 2.0, 1.9])
    d.limb("art_lgreen", [(12.5, 22.0), (11.2, 28.0), (11.4, 31.4)], [2.1, 2.0, 1.9])
    P, head = gator(d, 23.0, 45.0, turn=tilt, k=1.12)
    gator_cap(d, P, tilt, head=head)
    gator_eye(d, P, tilt)
    # (the shell's near half, its rim broken in a zigzag, two short cracks down from it)
    front = shell_rim(cx, cy, rx, ry, rim, teeth=7, depth=3.6)
    th = math.degrees(math.asin((rim - cy) / ry))
    d.shape("art_white", front + _arc(cx, cy, rx, ry, 180.0 - th, 360.0 + th, 30)[1:-1])
    d.line("art_lgrey", _arc(cx, cy, rx * 0.86, ry * 0.86, 292.0, 338.0, 8), 2.0)          # (its shade)
    for i, sgn in ((4, 1.0), (11, -1.0)):
        p = front[i]
        d.line("art_ink", [p, (p[0] + 1.0 * sgn, p[1] - 2.6), (p[0] - 0.6 * sgn, p[1] - 4.4), (p[0] + 0.8 * sgn, p[1] - 6.6)], 0.5, taper=False)
    # (its hands over the rim, little claws)
    for hx, hy in ((34.8, 32.0), (11.4, 32.0)):
        d.oval("art_lgreen", hx, hy, 2.7, 2.0)
        for k in (-1, 0, 1):
            d.limb("art_lgreen", [(hx + k * 1.6, hy - 0.4), (hx + k * 1.8, hy - 2.5)], [0.8, 0.62], ink=d.ink * 0.7)
            d.oval("art_cream", hx + k * 1.8, hy - 2.9, 0.42, 0.42, ink=0.0)
    names(d, a, (84, 34), 14, lines=["Bayou", "Baby"], gap=1.42)
    passed_hand(d, 231911, 942449)


# ---- 40: the barnyard bomber ---------------------------------------------------------------------------------------------
def egg(d, x, y, r, turn=0.0, paint="art_cream"):
    """A hen's egg, `r` across its middle, its long axis turned `turn` degrees from upright; a gleam on it."""
    d.oval(paint, x, y, r * 1.3, r, turn=turn + 90.0)
    a = math.radians(turn + 90.0)
    d.line("art_white", [(x - r * 0.5 * math.cos(a) - r * 0.35 * math.sin(a), y - r * 0.5 * math.sin(a) + r * 0.35 * math.cos(a)),
                         (x + r * 0.1 * math.cos(a) - r * 0.5 * math.sin(a), y + r * 0.1 * math.sin(a) + r * 0.5 * math.cos(a))], r * 0.3)


def basket(d, x, y, turn=0.0, k=1.0, paint="art_tan", rim="art_lbrown", inside="art_brown", eggs_in=()):
    """A woven egg basket with its handle, its middle at (x, y), tipped `turn` degrees from upright: its mouth an
    ellipse dark inside, with `eggs_in` ((u, v, r, turn) in its frame) in it. Returns its frame (the handle's top at
    (0, 15.2))."""
    B = _frame(x, y, turn, k)
    d.limb(paint, [B(-8.8, 4.8), B(-6.6, 11.8), B(0.0, 15.2), B(6.6, 11.8), B(8.8, 4.8)], [0.95 * k] * 5)
    d.fill(paint, [B(-9.2, 4.6), B(9.2, 4.6), B(8.4, -1.4), B(6.4, -5.4), B(3.0, -7.2), B(-3.0, -7.2), B(-6.4, -5.4), B(-8.4, -1.4)], steps=3)
    for v in (0.4, -3.6):                                    # (its weave)
        d.line("art_ink", [B(-8.5, v), B(0.0, v - 0.7), B(8.5, v)], 0.36 * k, taper=False)
    for u in (-4.4, 0.0, 4.4):
        d.line("art_ink", [B(u * 1.08, 3.6), B(u, -1.4), B(u * 0.68, -6.6)], 0.32 * k)
    d.oval(rim, *B(0.0, 4.8), 9.8 * k, 3.2 * k, turn=turn)
    d.oval(inside, *B(0.0, 5.0), 8.2 * k, 2.2 * k, turn=turn, ink=d.ink * 0.5)
    for (u, v, r, t) in eggs_in:
        egg(d, *B(u, v), r * k, turn=turn + t)
    return B


@painting(40)
def p40(d, a):   # a rooster in goggles dropping eggs from a basket
    R = _frame(30.0, 46.0)
    wing(d, R(6.0, 7.0), 74, 22, 9, "art_lgrey", shade="art_grey", spread=-30)             # (its far wing, up)
    for pts, rr, col in (([(-10, 3), (-17, 8), (-24, 9), (-29, 5.5), (-30.5, 0.5)], [3.3, 3.1, 2.6, 1.9, 0.7], "art_white"),
                         ([(-11, 0), (-17.5, 2.5), (-24, 1.0), (-27.5, -4.0)], [3.0, 2.8, 2.1, 0.7], "art_lgrey"),
                         ([(-11, -3), (-16, -5.0), (-20, -9.5), (-20.5, -13.5)], [2.7, 2.3, 1.7, 0.7], "art_white")):
        d.limb(col, [R(u, v) for u, v in pts], rr)                                        # (its tail's sickle feathers)
    # (the basket in its feet, tipped forward: an egg tumbling over its lip, two falling)
    B = basket(d, *R(0.0, -25.0), turn=-48.0, eggs_in=((3.6, 7.0, 2.4, -20),))
    grip = B(0.0, 15.2)
    egg(d, *R(14.5, -27.5), 2.5, turn=-30)
    egg(d, *R(17.0, -38.5), 2.5, turn=20)
    for u0, du in ((1.0, -1.4), (5.0, 1.2)):                                              # (its legs, down to the handle)
        top = R(u0, -7.4)
        g = (grip[0] + du, grip[1] + 0.8)
        d.limb("art_yellow", [top, ((top[0] + g[0]) / 2 + 0.6, (top[1] + g[1]) / 2), g], [1.0, 0.85, 0.75], ink=d.ink * 0.8)
        for t in (-1, 0, 1):
            d.limb("art_yellow", [g, (g[0] + t * 1.3 + 0.8, g[1] - 0.6), (g[0] + t * 1.1 + 0.6, g[1] - 2.0)], [0.55, 0.5, 0.4], ink=d.ink * 0.6)
    body = [(13, 3), (13.4, -2), (10.5, -6.5), (4, -9), (-4, -9), (-10, -6), (-13, -1), (-12, 4), (-7, 7.5), (0, 9), (7, 8.5), (11, 6.5)]
    d.fill("art_white", [R(u, v) for u, v in body])
    d.line("art_lgrey", [R(-8, -5.6), R(0, -7.4), R(8, -6.0)], 1.4)
    d.fill("art_white", [R(u, v) for u, v in [(5.0, 6.0), (6.6, 12.6), (10.0, 18.0), (14.6, 20.2), (18.6, 18.6), (19.2, 13.8), (17.4, 8.6),
                                               (14.6, 3.6), (10.6, 1.4)]], steps=4)                       # (its neck)
    d.line("art_lgrey", [R(9.6, 4.6), R(12.6, 9.6), R(14.0, 14.6)], 0.9)
    d.fill("art_red", [R(u, v) for u, v in [(11.0, 23.4), (10.6, 27.8), (12.8, 26.6), (13.8, 30.8), (15.8, 27.6), (17.8, 30.2), (18.6, 26.4),
                                             (20.6, 26.6), (19.6, 23.0)]], steps=3)                       # (its comb)
    d.oval("art_white", *R(15.6, 20.8), 4.9, 4.7)
    d.fill("art_red", [R(u, v) for u, v in [(18.6, 18.4), (20.8, 17.8), (21.0, 14.2), (19.4, 13.0), (17.8, 14.8), (17.6, 17.4)]], steps=3)
    d.shape("art_yellow", [R(19.6, 21.8), R(25.2, 20.0), R(19.8, 18.0)])                  # (its beak)
    d.line("art_ink", [R(19.8, 19.9), R(23.8, 19.9)], 0.35)
    # (its goggles, down over its eyes for the run)
    d.line("art_black", [R(11.0, 22.8), R(14.2, 23.4), R(17.2, 23.2)], 1.3, taper=False)
    for lu, lv, r in ((20.6, 22.6, 2.0), (16.6, 21.8, 2.8)):
        d.oval("art_lsteel", *R(lu, lv), r, r * 0.92, ink=d.ink * 0.9)
        d.oval("art_sky", *R(lu, lv), r * 0.66, r * 0.6, ink=d.ink * 0.45)
        d.line("art_white", [R(lu - r * 0.35, lv + r * 0.15), R(lu - r * 0.05, lv + r * 0.38)], 0.45)
    wing(d, R(-1.0, 6.0), 100, 28, 11.5, "art_white", spread=-30)                         # (its near wing, up)
    names(d, a, (98, 41), 15, lines=["Barnyard", "Bomber"], gap=1.4)


# ---- 41: Dakota dawn -------------------------------------------------------------------------------------------------------
def grain(d, paint, x, y, a, b, turn, ink):
    """One grain of wheat: a plump almond `a` long and `b` wide (half lengths), its point toward `turn` degrees."""
    P = _frame(x, y, turn)
    pts = []
    for i in range(28):
        t = 2 * math.pi * i / 28
        pts.append(P(a * math.cos(t), b * math.sin(t) * (0.5 + 0.5 * ((1 - math.cos(t)) / 2) ** 0.6)))
    d.shape(paint, pts, ink=ink)


def wheat(d, base, tip, a=4.4, b=2.4, paint="art_cream", stalk="art_brown", awn_w=0.9, pairs=3, lean=-3.0):
    """One bold ear of wheat on its stalk, from `base` to `tip`: an edged stalk; its grains, each `a` long and `b` wide
    (half lengths), in pairs up the head and one at its top, each pointing up and out; and its long beard, five
    whiskers running up past it nearly side by side, each `awn_w` across (the sun's rays are a third of that), leaning
    `lean` degrees off the stalk's line."""
    bx, by = base
    tx, ty = tip
    L = math.hypot(tx - bx, ty - by)
    ux, uy = (tx - bx) / L, (ty - by) / L
    nx, ny = -uy, ux
    step = a * 0.95                                          # (each pair of grains a little up the head from the last)
    hl = step * pairs + a * 1.2                              # (the head's length, to its top grain's tip)
    hb = (tx - ux * hl, ty - uy * hl)                        # (where the head starts on the stalk)
    d.limb(stalk, [base, lerp(base, hb, 0.55), (hb[0] + ux * step, hb[1] + uy * step)], [b * 0.42, b * 0.4, b * 0.36])
    top = (tx - ux * a * 0.75, ty - uy * a * 0.75)
    ang0 = math.atan2(uy, ux)
    for o in (-2, -1, 0, 1, 2):                              # (its beard, behind its grains: from the top grain and the pair below)
        an = ang0 + math.radians(4.0 * o + lean)
        back = a * 0.6 * abs(o) ** 0.5
        p0 = (top[0] + nx * o * b * 0.4 - ux * back, top[1] + ny * o * b * 0.4 - uy * back)
        ln = a * (2.9 - 0.2 * abs(o)) + back
        p1 = (p0[0] + math.cos(an) * ln, p0[1] + math.sin(an) * ln)
        d.line(paint, [p0, lerp(p0, p1, 0.6), p1], awn_w, taper=False)
    for i in range(pairs):                                   # (its grains, a pair at each step, leaning out and up)
        c = (hb[0] + ux * step * (i + 0.5), hb[1] + uy * step * (i + 0.5))
        sz = 1.0 - 0.07 * i
        for side in (1, -1):
            an = ang0 + math.radians(28.0 * side)
            g = (c[0] + nx * side * b * 0.9 * sz + ux * a * 0.25, c[1] + ny * side * b * 0.9 * sz + uy * a * 0.25)
            grain(d, paint, g[0], g[1], a * sz, b * sz, math.degrees(an), d.ink * 0.85)
    grain(d, paint, *top, a * 0.9, b * 0.95, math.degrees(ang0), d.ink * 0.85)


def sunrise(d, cx, cy, rx, ry, horizon=0.36, sky="art_navy", sun="art_cream", rays="art_yellow", field="art_gold",
            rows="art_brown", ray_w=2.0):
    """An upright oval sign of a prairie sunrise, its middle at (cx, cy), rx by ry: a deep blue morning sky, a pale sun
    half up over the horizon low in it, its short rays, the wheat field below with its rows running off toward the sun,
    each row ending on the rim under the sign's edge. Returns the horizon's height and the sun's radius."""
    hy = cy - ry + 2 * ry * horizon
    rim = ellipse(cx, cy, rx, ry, 0, 72)
    d.shape(sky, rim, ink=0.0)                               # (its edge goes on last, over the rows' ends)
    sr = rx * 0.42
    for i in range(7):                                       # (its rays: short strokes, never out to the edge)
        an = math.radians(20 + 140 * i / 6)
        r0, r1 = sr + 2.4, sr + (8.4 if i % 2 == 0 else 5.8)
        d.line(rays, [(cx + r0 * math.cos(an), hy + r0 * math.sin(an)), (cx + r1 * math.cos(an), hy + r1 * math.sin(an))], ray_w)
    d.oval(sun, cx, hy, sr, sr)
    half = rx * math.sqrt(max(0.0, 1.0 - ((hy - cy) / ry) ** 2))
    a_ = math.degrees(math.atan2((hy - cy) / ry, half / rx))
    top = [(cx + half * (1 - 2 * t / 10), hy) for t in range(11)]
    d.shape(field, top + _arc(cx, cy, rx, ry, 180 - a_, 360 + a_, 40)[1:-1], ink=0.0)
    inset = 0.12
    for f in (-0.82, -0.3, 0.3, 0.82):                       # (its rows, from the sun's foot to the rim)
        apex = (cx + f * 1.4, hy - 0.5)
        aim = (cx + f * rx * 1.25, cy - ry)
        e = _leave(cx, cy, rx - inset, ry - inset, apex, aim)
        ln = math.hypot(e[0] - apex[0], e[1] - apex[1])
        nx, ny = -(e[1] - apex[1]) / ln, (e[0] - apex[0]) / ln
        w = 1.05
        e0 = _leave(cx, cy, rx - inset, ry - inset, apex, (e[0] + nx * w, e[1] + ny * w))
        e1 = _leave(cx, cy, rx - inset, ry - inset, apex, (e[0] - nx * w, e[1] - ny * w))
        d.shape(rows, [(apex[0] + nx * 0.14, apex[1] + ny * 0.14), e0, e1, (apex[0] - nx * 0.14, apex[1] - ny * 0.14)], ink=0.0)
    d.line("art_ink", [(cx - half - 0.3, hy), (cx, hy + 0.15), (cx + half + 0.3, hy)], d.ink * 0.9, taper=False)   # (the horizon)
    d.shape(None, rim, ink=d.ink * 1.1)                      # (the sign's edge)
    return hy, sr


@painting(41)
def p41(d, a):   # a sunrise over a wheat field with one small bomber high in the sky
    cx, cy, rx, ry = 30.0, 40.0, 28.0, 40.0
    hy, sr = sunrise(d, cx, cy, rx, ry)
    small_bomber(d, cx + 9.0, cy + 25.0, 12.0, paint="art_white", turn=8.0)
    wheat(d, (18.0, 0.0), (7.0, 28.0))                       # (one ear of it close by, across the sign's lower left)
    names(d, a, (101.5, cy), 14.5, lines=["Dakota", "Dawn"], gap=1.42)
    passed_hand(d, 612538, 172503)


# ---- 42: Milk Train Millie (a cruder hand) ---------------------------------------------------------------------------------
def spoked(d, x, y, r, rim="art_red", tire="art_black", spokes=6):
    """A cart wheel: a black tyre, a painted rim and spokes, a black hub."""
    d.oval(tire, x, y, r, r)
    d.oval(rim, x, y, r * 0.78, r * 0.78, ink=d.ink * 0.6)
    d.oval(tire, x, y, r * 0.6, r * 0.6, ink=0.0)
    for i in range(spokes):
        an = math.pi * 2 * i / spokes + 0.3
        d.line(rim, [(x, y), (x + r * 0.62 * math.cos(an), y + r * 0.62 * math.sin(an))], r * 0.14, taper=False)
    d.oval(tire, x, y, r * 0.2, r * 0.2, ink=d.ink * 0.6)


def churn(d, cx, y0, R=7.5, body=22.0, shoulder=3.0, rn=2.9, neck=5.4, rc=6.2, cap=3.0, paint="art_lsteel", shine="art_white"):
    """A dairy's ten-gallon milk can standing up, from life: its foot ring, its straight upright body, taller than it
    is wide, one rolled rib round it; a short sloped shoulder with a handle each side; a narrow neck; and the wide
    mushroom cap of its lid. Returns the lid's top and the shoulder's foot."""
    yS = y0 + body
    yN, yL = yS + shoulder, yS + shoulder + neck
    top = yL + cap
    for sx in (-1, 1):                                       # (its handles, out from the body's top, under the shoulder)
        d.limb(paint, [(cx + sx * (R - 0.8), yS - 4.6), (cx + sx * (R + 2.0), yS - 3.9), (cx + sx * (R + 2.2), yS - 1.6),
                       (cx + sx * (R - 0.6), yS - 0.6)], [0.85] * 4)
    d.shape(paint, [(cx - R, y0 + 1.0), (cx - R, yS), (cx - rn, yN), (cx - rn, yL), (cx + rn, yL), (cx + rn, yN), (cx + R, yS),
                    (cx + R, y0 + 1.0)])
    d.shape(paint, [(cx - R - 0.5, y0), (cx + R + 0.5, y0), (cx + R + 0.5, y0 + 2.0), (cx - R - 0.5, y0 + 2.0)])      # (its foot)
    yr = y0 + 0.4 * body                                     # (its one rolled rib, standing a little proud of it)
    d.limb(paint, [(cx - R + 0.4, yr), (cx, yr - 0.15), (cx + R - 0.4, yr)], [0.62, 0.62, 0.62], ink=d.ink * 0.8)
    d.line(shine, [(cx - R * 0.55, y0 + 3.4), (cx - R * 0.55, yr - 2.0)], R * 0.2, taper=False)
    d.line(shine, [(cx - R * 0.55, yr + 2.0), (cx - R * 0.55, yS - 1.2), (cx - R * 0.42, yS + 0.8)], R * 0.2)
    d.line(shine, [(cx - rn * 0.45, yN + 0.8), (cx - rn * 0.45, yL - 0.8)], rn * 0.3, taper=False)
    lid = [(rn * 0.9, yL - 0.4), (rc * 0.8, yL - 0.75), (rc * 0.98, yL - 0.45), (rc * 1.04, yL + 0.3), (rc * 0.96, yL + 1.1),
           (rc * 0.74, yL + 1.8), (rc * 0.4, top - 0.15)]                   # (its lid: a rounded rim, a low dome over it)
    d.fill(paint, [(cx + u, v) for u, v in lid] + [(cx, top)] + [(cx - u, v) for u, v in reversed(lid)], steps=3)
    return top, yS


def railcar(d, cx, y, half=12.0, r=4.4, frame="art_black"):
    """A little rail truck for it to ride on: its frame, its two wheels. Returns its deck's height."""
    d.shape(frame, [(cx - half, y + r + 0.4), (cx + half, y + r + 0.4), (cx + half - 0.5, y + r + 2.4), (cx - half + 0.5, y + r + 2.4)])
    for wx in (cx - half * 0.6, cx + half * 0.6):
        spoked(d, wx, y + r, r)
    return y + r + 2.4


def hanky(d, hand, paint="art_white"):
    """A handkerchief waved from her hand (lady()'s F.hand entry), streaming back in the wind."""
    (hx, hy), _ = hand
    d.fill(paint, [(hx + 0.3, hy + 0.5), (hx - 0.4, hy + 2.6), (hx - 2.2, hy + 4.4), (hx - 4.4, hy + 4.6), (hx - 3.2, hy + 3.0),
                   (hx - 4.6, hy + 1.6), (hx - 2.0, hy + 1.2), (hx - 0.6, hy - 0.2)], ink=d.ink * 0.8, steps=3)


@painting(42)
def p42(d, a):   # a milkmaid in an apron riding a milk can on wheels
    cx, h = 22.0, 40.0
    deck = railcar(d, cx, 0.0, half=10.6)
    for dy, ln_ in ((5.0, 9.0), (-1.0, 13.0), (-7.0, 8.0)):  # (her speed, behind)
        y = deck + 12.0 + dy
        d.line("art_white", [(cx - 11.5, y), (cx - 11.5 - ln_ * 0.5, y), (cx - 11.5 - ln_, y)], 1.2)
    lid, _ = churn(d, cx, deck)
    k = h / HEADS
    F = lady(d, cx - 4.4 - 0.35 * k, lid - 3.36 * k, h, 1, pose="sit", arms=("lift", "down"),
             wear={"top": "art_red", "skirt": "art_red", "sleeves": "puff", "tall": "art_black", "hat": "art_white"},
             hair="bob", hair_paint="art_blond", hat="kerchief")
    # (her apron over her lap, inside her skirt's edge, and its bib: a sitting figure's own is left off by lady())
    d.fill("art_white", F.Ps([(-0.42, 4.56), (0.45, 4.56), (0.7, 4.1), (1.4, 4.0), (1.82, 3.9), (1.9, 3.62), (1.4, 3.56), (0.5, 3.66),
                              (-0.3, 3.96)]), ink=d.ink * 0.7, steps=3)
    d.fill("art_white", F.Ps([(-0.4, 4.54), (-0.42, 5.3), (0.42, 5.3), (0.4, 4.54)]), ink=d.ink * 0.7, steps=2)
    for sx in (-1, 1):
        d.limb("art_white", F.Ps([(sx * 0.38, 5.26), (sx * 0.3, 5.8), (sx * 0.22, 6.1)]), [F.r(0.05)] * 3, ink=d.ink * 0.56)
    for sx in (-1, 1):                                       # (her cheeks plain: a crude hand has no pink)
        d.oval("art_skin", *F.P(0.07 + sx * 0.245, 6.66), F.r(0.08), F.r(0.05), ink=0.0)
    hanky(d, F.hand["fore"])                                 # (waving her handkerchief from the milk train)
    names(d, a, (72, 32), 14, lines=a["name"].split(), gap=1.36)
    passed_hand(d, 523179, 13604, 495320)


# ---- 43: the pickle barrel -------------------------------------------------------------------------------------------------
@painting(43)
def p43(d, a):   # a pickle barrel with a bullseye on its lid and a bomb dropping in
    cx, y0, y1, rx, ry, bulge = 28.0, 2.0, 36.0, 14.0, 7.0, 3.4

    def W(t):
        return rx + bulge * math.sin(math.pi * t)

    def S(f, t):                                             # (a point on its near side: f across, -1 to 1; t up, 0 to 1)
        return (cx + f * W(t), y0 + (y1 - y0) * t - ry * math.sqrt(max(0.0, 1 - f * f)))
    body = [(cx - rx, y1)] + [S(-1.0, t / 12) for t in range(12, -1, -1)][1:] + \
           [S(f / 10, 0.0) for f in range(-10, 11)][1:-1] + [S(1.0, t / 12) for t in range(13)]
    d.shape("art_tan", body + _arc(cx, y1, rx, ry, 0.0, 180.0, 20)[1:-1])
    for f in (-0.62, -0.22, 0.2, 0.6):                       # (its staves)
        d.line("art_ink", [S(f, t / 10) for t in range(11)], 0.4, taper=False)
    dt = 1.1 / (y1 - y0)
    for t in (0.1, 0.32, 0.68, 0.9):                         # (its iron hoops, round it and no wider)
        d.shape("art_dark", [S(f / 10, t - dt) for f in range(-10, 11)] + [S(f / 10, t + dt) for f in range(10, -11, -1)], ink=d.ink * 0.8)
    # (its lid, a bullseye painted on it)
    d.oval("art_tan", cx, y1, rx, ry)
    d.oval("art_white", cx, y1, rx * 0.84, ry * 0.8, ink=0.4)
    d.oval("art_red", cx, y1, rx * 0.62, ry * 0.58, ink=0.0)
    d.oval("art_white", cx, y1, rx * 0.4, ry * 0.37, ink=0.0)
    d.oval("art_red", cx, y1, rx * 0.18, ry * 0.17, ink=0.0)
    # (the bomb, dropping in, nose first, just over the bull)
    B = bomb(d, cx + 1.2, y1 + 10.5, 22.0, turn=-85.0)
    for v, u0, u1 in ((0.32, -0.5, -0.95), (0.42, -0.62, -0.92), (-0.32, -0.5, -0.95), (-0.42, -0.62, -0.92)):
        d.line("art_white", [B(u0, v), B(u1, v)], 0.8)       # (its rush through the air, streaming past it)
    names(d, a, (90, 34), 14, lines=a["name"].split(), gap=1.36)


# ---- 44: Looking Back (a cruder hand) --------------------------------------------------------------------------------------
def _rounded(x0, y0, x1, y1, r, n=5):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, 270), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        pts += _arc(cx, cy, r, r, a0, a0 + 90, n)
    return pts


@painting(44)
def p44(d, a):   # a pair of eyes peering backward through a tail gunner's little window
    x0, y0, x1, y1, t = 4.0, 0.0, 56.0, 80.0, 5.0            # (an upright window, as the tail gunner's are)
    d.shape("art_lsteel", _rounded(x0, y0, x1, y1, 9.0))
    d.shape("art_black", _rounded(x0 + t, y0 + t, x1 - t, y1 - t, 5.0), ink=d.ink * 0.8)
    mx, my = (x0 + x1) / 2, (y0 + y1) / 2
    for (rx_, ry_) in ((x0 + t / 2, my - 15.0), (x0 + t / 2, my + 15.0), (x1 - t / 2, my - 15.0), (x1 - t / 2, my + 15.0),
                       (mx - 10.0, y0 + t / 2), (mx + 10.0, y0 + t / 2), (mx - 10.0, y1 - t / 2), (mx + 10.0, y1 - t / 2)):
        d.oval("art_ink", rx_, ry_, 0.7, 0.7, ink=0.0)        # (its frame's rivets)
    ey = y0 + 0.4 * (y1 - y0)                                 # (the eyes, wide, low in it, looking aft, their lids half down)
    for ex in (mx - 9.8, mx + 9.8):
        d.oval("art_white", ex, ey, 8.0, 9.9, ink=0.0)
        d.shape("art_black", [(ex - 8.8, ey + 3.2), (ex + 8.8, ey + 4.1), (ex + 8.8, ey + 11.6), (ex - 8.8, ey + 11.6)], ink=0.0)
        d.oval("art_black", ex + 4.0, ey - 1.3, 3.5, 4.2, ink=0.0)
        d.oval("art_white", ex + 5.1, ey + 0.2, 1.0, 1.0, ink=0.0)
    d.line("art_white", [(x0 + t + 3.0, y1 - t - 9.0), (x0 + t + 9.0, y1 - t - 3.0)], 1.4)   # (a gleam on the glass)
    d.line("art_white", [(x0 + t + 3.0, y1 - t - 14.0), (x0 + t + 13.0, y1 - t - 4.0)], 0.9)
    names(d, a, (103, my), 15, lines=["Looking", "Back"], gap=1.4)
    passed_hand(d, 183809, 983914)


# ---- 45: Fogbound Freddie --------------------------------------------------------------------------------------------------
def fog(d, lumps, paint="art_white", shade="art_lgrey", under=None):
    """A bank of thick fog: big rolling lumps run together, the painter's edge round them all, a little shade along
    the foot of the bank (`under`: the points of its stroke)."""
    d.blobs(paint, lumps)
    if under:
        d.line(shade, under, 1.6)


def frog_head(d, ox, oy):
    """A frog's wide head, from life, its eyes up on top, golden with a level pupil: (ox, oy) moves it. Returns the
    corner of its mouth."""
    def T(pts):
        return [(x + ox, y + oy) for x, y in pts]
    d.fill("art_green", T([(10.0, 44.0), (11.0, 50.0), (16.0, 53.4), (24.0, 54.0), (31.0, 52.6), (35.4, 48.4), (34.6, 43.4), (28.0, 40.6), (18.0, 40.4)]))
    for ex, ey, r in ((17.6, 54.6, 4.4), (28.6, 55.2, 4.0)):
        d.oval("art_green", ex + ox, ey + oy, r, r * 0.92)
        d.oval("art_yellow", ex + 0.6 + ox, ey + 0.2 + oy, r * 0.66, r * 0.6, ink=0.4)
        d.oval("art_ink", ex + 1.2 + ox, ey + 0.2 + oy, r * 0.42, r * 0.22, ink=0.0)
        d.oval("art_white", ex + 0.2 + ox, ey + 1.4 + oy, r * 0.16, r * 0.16, ink=0.0)
    d.line("art_ink", T([(12.0, 46.0), (20.0, 44.6), (28.0, 45.2), (32.4, 46.8)]), 0.6)
    return (32.4 + ox, 46.4 + oy)


def trench_coat(d, ox, oy, paint="art_tan", belt="art_brown"):
    """His trench coat, from life: sloping shoulders, double-breasted, belted, its skirt flaring to his knees, the far
    sleeve down at his side with its hand in his pocket; below its hem a frog's legs and webbed feet."""
    def T(pts):
        return [(x + ox, y + oy) for x, y in pts]
    for fx, s_ in ((16.4, -1), (26.6, 1)):
        d.limb("art_green", T([(fx, 7.0), (fx + s_ * 0.5, 4.0), (fx + s_ * 0.7, 2.2)]), [2.0, 1.8, 1.5])
        ank = (fx + s_ * 0.7, 1.6)
        tips = [(ank[0] + s_ * 1.2 + t * 2.6, -0.4 - abs(t) * 0.5) for t in (-1, 0, 1)]
        web = [ank]
        for j, tp in enumerate(tips):
            web.append(tp)
            if j < 2:
                web.append(((tp[0] + tips[j + 1][0]) / 2, (tp[1] + tips[j + 1][1]) / 2 + 0.9))
        d.fill("art_green", T(web), ink=d.ink * 0.8, steps=2)
        for tp in T(tips):
            d.oval("art_green", *tp, 0.75, 0.6, ink=d.ink * 0.6)
    d.limb(paint, T([(11.6, 35.0), (9.6, 28.0), (9.4, 21.0)]), [2.7, 2.5, 2.4])                     # (his far sleeve, down)
    d.fill(paint, T([(14.0, 38.6), (11.0, 36.0), (11.2, 30.0), (12.6, 26.0), (9.0, 6.4), (16.0, 5.4), (22.0, 5.0), (28.0, 5.4), (35.0, 6.4),
                     (31.4, 26.0), (32.8, 30.0), (33.0, 36.0), (30.0, 38.6), (22.0, 39.4)]), steps=3)
    d.line("art_ink", T([(24.4, 37.6), (25.0, 26.0), (26.4, 5.6)]), 0.5)                            # (its front edge)
    d.line("art_ink", T([(22.0, 18.0), (21.6, 11.0), (21.2, 5.4)]), 0.35)                            # (a fold)
    d.fill(belt, T([(12.4, 27.8), (31.6, 27.8), (31.8, 25.0), (12.2, 25.0)]), ink=d.ink * 0.8, steps=2)
    d.shape("art_gold", T([(22.6, 28.4), (26.6, 28.4), (26.6, 24.4), (22.6, 24.4)]), ink=d.ink * 0.7)
    for bx, by in ((19.6, 34.0), (28.0, 34.0), (19.4, 30.4), (28.2, 30.4)):
        d.oval(belt, bx + ox, by + oy, 0.85, 0.85, ink=d.ink * 0.5)
    for pts in ([(14.0, 39.2), (12.4, 43.6), (18.2, 40.4)], [(29.6, 39.2), (33.4, 42.6), (27.0, 40.4)]):    # (his collar up)
        d.shape(paint, T(pts))


def foghorn(d, at, turn, L=22.0, bell=5.6):
    """A hand foghorn of brass: a tube flaring to its bell, from his mouth at `at`, up `turn` degrees. Returns its frame."""
    H = _frame(at[0], at[1], turn)
    d.fill("art_yellow", [H(u, v) for u, v in [(0.0, 0.85), (L * 0.64, 1.3), (L * 0.86, 2.6), (L, bell), (L, -bell), (L * 0.86, -2.6),
                                                  (L * 0.64, -1.3), (0.0, -0.85)]], steps=3)
    d.line("art_gold", [H(1.5, -0.35), H(L * 0.64, -0.75), H(L * 0.86, -2.0)], 0.8)
    d.oval("art_dark", *H(L, 0.0), 1.5, bell, turn=turn, ink=d.ink * 0.8)
    return H


@painting(45)
def p45(d, a):   # a frog in a trench coat blowing a foghorn in thick fog
    ox, oy, turn, L = -6.0, -2.0, 56.0, 21.0
    # (the fog bank behind him, rolling up to his belt)
    fog(d, [(-1.6, 8.4, 4.8), (1.4, 15.0, 7.0), (10.0, 17.0, 8.0), (19.0, 17.4, 8.4), (28.8, 15.6, 7.6), (34.2, 8.6, 5.4)])
    trench_coat(d, ox, oy)
    mouth = frog_head(d, ox, oy)
    H = foghorn(d, (mouth[0] - 1.2, mouth[1] - 0.4), turn, L=L)
    for k in (1, 2, 3):                                      # (its blast)
        d.line("art_ink", [H(L + 2.0 + k * 2.6, 6.4 + k * 1.4), H(L + 3.2 + k * 3.0, 0.0), H(L + 2.0 + k * 2.6, -6.4 - k * 1.4)], 0.6)
    # (his near arm up to it: the upper arm out and down from his shoulder, the elbow out to the side, the forearm up to
    #  the horn, a cuff at his wrist)
    sh, el = (25.2, 34.0), (33.6, 29.0)
    wr = H(6.2, -1.6)
    d.limb("art_tan", [sh, lerp(sh, el, 0.5), el], [2.6, 2.45, 2.3])
    d.limb("art_tan", [el, lerp(el, wr, 0.5), wr], [2.3, 2.15, 2.0])
    cuff_a = lerp(el, wr, 0.8)
    d.limb("art_tan", [cuff_a, wr], [2.45, 2.4], ink=d.ink * 0.85)
    d.oval("art_green", *H(8.0, -0.2), 2.0, 1.8)
    # (and the bank in front of him, round his legs from the coat's hem down, his feet lost in it)
    fog(d, [(-1.8, 1.2, 4.8), (4.4, 3.0, 6.2), (12.2, 2.4, 6.8), (20.0, 3.0, 6.6), (27.4, 2.2, 6.4), (33.4, 0.8, 5.0), (16.0, -2.4, 19.6, 3.6)],
        under=[(-1.0, -4.0), (16.0, -4.6), (33.0, -3.8)])
    names(d, a, (96, 16), 16, lines=["Fogbound", "Freddie"], gap=1.38)
    passed_hand(d, 702988, 521618)
