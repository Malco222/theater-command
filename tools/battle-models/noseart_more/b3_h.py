"""Batch 3, numbers 130 to 136 (painter b3_h): a tortoise in a flying helmet with a bomb strapped to his shell, one
pair of long johns kicking on a short line, a navigator's chart folded into a paper hat, a farm girl laughing with one
boot stuck in tar, a carousel horse leaping off its pole, a track worker leaning on his lining bar and a crow in a
sailor's cap with his spyglass up in the crow's nest. Flat shapes with noseart's Drawing API and the figure kit; each
name set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T  # noqa: E402
from noseart_more.b3_d import barrel  # noqa: E402


# ---- 130: Sooner or Later -----------------------------------------------------------------------------------------------------------
def tortoise(d, x, y, size, shell="art_green", hide="art_khaki", scute="art_ink"):
    """A tortoise walking to the right, his feet at y, his shell's middle at x, `size` long from his tail to his nose:
    a high domed shell with its scutes drawn in, four stumpy legs, his neck out, a sleepy eye. Returns his head's middle,
    its width, and the top of his shell."""
    k = size / 54.0
    P = lambda u, v: (x + (u - 24.0) * k, y + v * k)       # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]             # noqa: E731
    ink = d.ink
    # (his far legs, behind, and his tail)
    for pts in (((34.0, 8.0), (35.6, 4.0), (35.0, 1.0)), ((13.0, 8.0), (11.4, 4.0), (12.0, 1.0))):
        d.limb(hide, Ps(pts), [3.0 * k, 2.6 * k, 2.4 * k], ink=ink)
    d.fill(hide, Ps([(7.0, 8.4), (2.0, 6.0), (6.4, 6.2)]), ink=ink, steps=1)
    # (the shell: a high dome over a flat rim, its scutes)
    dome = [P(u, v) for (u, v) in ellipse(24.0, 7.0, 19.0, 18.0, 0, 24, 0, 180)]
    d.fill(shell, dome, ink=ink, steps=2)
    d.line(scute, Ps([(5.6, 9.4), (12.0, 9.8), (24.0, 10.0), (36.0, 9.8), (42.4, 9.4)]), 0.5 * k, taper=False)
    hexes = [(24.0, 16.0, 6.0, 5.0), (12.6, 14.6, 4.6, 4.2), (35.4, 14.6, 4.6, 4.2), (24.0, 22.4, 4.4, 2.4)]
    for (cu, cv, ru, rv) in hexes:
        d.line(scute, [P(cu + ru * math.cos(math.radians(a)), cv + rv * math.sin(math.radians(a))) for a in range(0, 361, 60)], 0.45 * k, taper=False)
    for (u0, v0, u1, v1) in ((18.0, 11.0, 15.0, 10.0), (30.0, 11.0, 33.0, 10.0), (8.0, 12.6, 6.0, 10.0), (40.0, 12.6, 42.0, 10.0)):
        d.line(scute, Ps([(u0, v0), (u1, v1)]), 0.4 * k, taper=False)
    # (his near legs, in front of the shell's rim; his neck and head out ahead)
    for pts in (((37.0, 9.0), (39.0, 4.4), (38.0, 1.0)), ((11.0, 9.0), (9.0, 4.4), (10.0, 1.0))):
        d.limb(hide, Ps(pts), [3.4 * k, 3.0 * k, 2.8 * k], ink=ink)
        for t in (-1.6, 0.0, 1.6):
            d.oval("art_lgrey", *P(pts[2][0] + 1.4 + t, 0.6), 0.5 * k, 0.4 * k, ink=0.0)
    d.limb(hide, Ps([(40.0, 9.4), (45.0, 13.6), (48.0, 15.6)]), [3.2 * k, 2.8 * k, 2.7 * k], ink=ink)
    hx, hy = P(50.4, 17.2)
    d.oval(hide, hx, hy, 4.8 * k, 3.7 * k, ink=ink)
    d.oval("art_white", hx + 1.6 * k, hy + 0.9 * k, 1.3 * k, 1.1 * k, ink=ink * 0.6)
    d.oval("art_black", hx + 2.0 * k, hy + 0.8 * k, 0.6 * k, 0.65 * k, ink=0.0)
    d.fill(hide, [(hx + 0.2 * k, hy + 1.2 * k), (hx + 1.6 * k, hy + 2.3 * k), (hx + 3.0 * k, hy + 1.2 * k)], ink=0.0, steps=2)
    d.line("art_ink", [(hx + 0.3 * k, hy + 1.2 * k), (hx + 1.6 * k, hy + 1.4 * k), (hx + 3.0 * k, hy + 1.2 * k)], 0.35 * k, taper=False)
    d.line("art_ink", [(hx + 1.4 * k, hy - 1.6 * k), (hx + 3.2 * k, hy - 1.2 * k), (hx + 4.4 * k, hy - 0.4 * k)], 0.35 * k)
    return (hx, hy), 9.6 * k, P(24.0, 25.0)


@painting(130)
def p130(d, a):   # a tortoise plodding along in a flying helmet, a bomb strapped on top of his shell
    (hx, hy), hw, (tx, ty) = tortoise(d, 28.0, 2.0, 58.0)
    helmet(d, hx, hy + 0.4, hw, 1, "art_brown", view="side", inside=None)
    # (the bomb on top of his shell, nose forward, strapped down by two belts round the shell)
    bomb(d, tx + 15.0, ty + 3.6, 30.0, turn=0.0, paint="art_dark", band="art_yellow")
    for u in (-6.0, 6.0):
        d.line("art_brown", [(tx + u - 0.4, ty - 6.0), (tx + u, ty + 1.0), (tx + u + 0.2, ty + 7.4)], 1.5, taper=False)
        d.shape("art_gold", [(tx + u - 1.2, ty - 1.0), (tx + u - 1.2, ty + 1.2), (tx + u + 1.2, ty + 1.2), (tx + u + 1.2, ty - 1.0)])
    place_name(d, a, lines=["Sooner", "or Later"], where="right", room=6.0, at=22.0, caps=21.5)


# ---- 131: Clothesline Parade --------------------------------------------------------------------------------------------------------
@painting(131)
def p131(d, a):   # one pair of red long johns pegged on a short line, blown out sideways, its legs kicking (the ruling: one pair)
    ink = d.ink
    # (the two posts and the line between them)
    for px in (0.0, 66.0):
        d.line("art_brown", [(px, 0.0), (px, 52.0)], 2.0, taper=False)
        d.line("art_brown", [(px - 3.4, 50.0), (px + 3.4, 50.0)], 1.4, taper=False)
    d.line("art_white", [(0.0, 49.0), (16.0, 46.4), (33.0, 45.8), (50.0, 46.4), (66.0, 49.0)], 0.5, taper=False)
    # (the union suit, hung by its shoulders and blown out to the right: its body, its sleeves, its legs kicking)
    cx, top, lean, m = 20.0, 46.0, 0.36, 1.62
    S = lambda u, v: (cx + m * (u - lean * v), top + m * v)       # noqa: E731
    Ss = lambda pts: [S(u, v) for u, v in pts]                     # noqa: E731
    suit = "art_red"
    d.limb(suit, Ss([(-7.6, -1.6), (-9.6, -8.0), (-8.6, -14.0)]), [2.7 * m, 2.4 * m, 2.1 * m], ink=ink)
    d.limb(suit, Ss([(-4.0, -19.0), (1.0, -27.0), (9.0, -27.4)]), [3.6 * m, 3.0 * m, 2.6 * m], ink=ink)
    d.fill(suit, Ss([(-8.0, 0.0), (-3.0, 0.0), (0.0, -2.2), (3.0, 0.0), (8.0, 0.0), (8.6, -8.0), (7.8, -18.0), (5.6, -21.4), (-5.6, -21.4),
                     (-7.8, -18.0), (-8.6, -8.0)]), ink=ink, steps=3)
    d.limb(suit, Ss([(4.0, -19.4), (12.0, -25.0), (20.0, -24.6)]), [3.6 * m, 3.0 * m, 2.6 * m], ink=ink)
    d.limb(suit, Ss([(7.6, -1.6), (13.0, -5.0), (19.0, -6.0)]), [2.7 * m, 2.4 * m, 2.1 * m], ink=ink)
    # (the ribbed cuffs, the button front)
    for (p0, p1) in (((-8.7, -12.6), (-8.5, -14.6)), ((8.0, -27.4), (9.6, -27.4)), ((18.6, -24.6), (20.4, -24.6)), ((17.6, -5.8), (19.4, -6.1))):
        for t in (0.0, 0.5, 1.0):
            u, v = p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t
            d.line("art_ink", [S(u, v - 1.6), S(u, v + 1.6)] if abs(p1[0] - p0[0]) > abs(p1[1] - p0[1]) else [S(u - 1.6, v), S(u + 1.6, v)], 0.35, taper=False)
    for v in (-4.0, -8.0, -12.0, -16.0):
        d.oval("art_white", *S(0.0, v), 0.9, 0.9, ink=ink * 0.5)
    # (the two clothespins at its shoulders)
    for u in (-6.0, 6.0):
        X, Y = S(u, 0.0)
        d.shape("art_tan", [(X - 0.9, Y - 4.4), (X - 0.9, Y + 2.0), (X + 0.9, Y + 2.0), (X + 0.9, Y - 4.4)])
    # (the wind that does it)
    for (u, v, L) in ((4.0, 34.0, 8.0), (3.0, 26.0, 7.0), (4.0, 18.0, 8.0)):
        d.line("art_white", [(u, v), (u + L * 0.5, v + 1.2), (u + L, v)], 0.6)
    place_name(d, a, lines=["Clothesline", "Parade"], where="below", room=5.0, caps=21.5)


# ---- 132: Lost and Found ------------------------------------------------------------------------------------------------------------
def _tilt(d, n0, cx, cy, deg):
    """Turn every shape painted since op n0 by `deg` degrees about (cx, cy)."""
    c, s_ = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    for op in d.ops[n0:]:
        if op[0] == "polys":
            for poly in op[2]:
                poly[:] = [(cx + (u - cx) * c - (v - cy) * s_, cy + (u - cx) * s_ + (v - cy) * c) for u, v in poly]


@painting(132)
def p132(d, a):   # a navigator's chart folded into a paper hat, his dividers standing by it
    ink = d.ink
    x0, x1, yb, peak = 4.0, 48.0, 12.0, 46.0
    cx = (x0 + x1) / 2.0
    n0 = len(d.ops)
    # (the hat: the chart's sea in pale blue, a coast in tan, the grid, the course pencilled across it, its fixes)
    d.shape("art_sky", [(x0, yb), (cx, peak), (x1, yb)])
    d.fill("art_tan", [(x0 + 0.4, yb), (x0 + 5.0, yb + 7.6), (x0 + 9.0, yb + 10.0), (x0 + 12.0, yb + 16.0), (x0 + 14.0, yb + 13.0), (x0 + 13.0, yb + 6.0),
                       (x0 + 16.0, yb)], ink=ink * 0.6, steps=3)
    half = (x1 - x0) / 2.0
    for v in (yb + 8.0, yb + 16.0, yb + 24.0):
        w = half * (1.0 - (v - yb) / (peak - yb))
        d.line("art_lblue", [(cx - w + 0.4, v), (cx + w - 0.4, v)], 0.25, taper=False)
    for u in (cx - 12.0, cx, cx + 12.0):
        v1 = yb + (peak - yb) * (1.0 - abs(u - cx) / half)
        d.line("art_lblue", [(u, yb), (u, v1 - 0.4)], 0.25, taper=False)
    course = [(x0 + 12.0, yb + 4.6), (cx - 2.0, yb + 14.0), (cx + 10.0, yb + 8.0)]
    for (p, q) in zip(course, course[1:]):
        for i in range(0, 10, 2):
            d.line("art_red", [lerp(p, q, i / 10.0), lerp(p, q, (i + 1) / 10.0)], 0.4, taper=False)
    for (u, v) in course:
        d.line("art_red", ellipse(u, v, 1.1, 1.1, 0, 12), 0.3, taper=False)
    star(d, cx + 4.0, yb + 20.0, 2.6, "art_navy", points=4, ink=0.0, inner=0.3)
    # (the band folded up round its foot, its ends turned in, the chart's grid going on over it)
    d.shape("art_white", [(x0 - 3.0, yb - 8.0), (x0 - 3.0, yb + 0.6), (x1 + 3.0, yb + 0.6), (x1 + 3.0, yb - 8.0)])
    for u in range(int(x0), int(x1) + 1, 8):
        d.line("art_lblue", [(u, yb - 7.6), (u, yb + 0.2)], 0.25, taper=False)
    d.line("art_lblue", [(x0 - 2.6, yb - 4.0), (x1 + 2.6, yb - 4.0)], 0.25, taper=False)
    for (u, s) in ((x0 - 3.0, 1), (x1 + 3.0, -1)):
        d.shape("art_lgrey", [(u, yb - 8.0), (u, yb + 0.6), (u + s * 3.4, yb - 3.6)])
    # (the hat sits at a jaunty tilt, as on a head, not square on the ground)
    _tilt(d, n0, cx, yb - 4.0, 10.0)
    # (his plotter lying in front of it: a clear ruler, its ruled edge, its round protractor; his dividers standing by)
    G = _T(x0 - 2.0, yb - 13.0, 4.0)
    d.shape("art_lsky" if "art_lsky" in PAINTS else "art_lblue", [G(0.0, 0.0), G(0.0, 5.0), G(44.0, 5.0), G(44.0, 0.0)], ink=ink * 0.8)
    for i in range(23):
        d.line("art_ink", [G(1.6 + i * 1.86, 5.0), G(1.6 + i * 1.86, 5.0 - (2.0 if i % 5 == 0 else 1.0))], 0.25, taper=False)
    d.line("art_ink", ellipse(*G(22.0, 0.0), 4.0, 4.0, 0, 14, 0, 180), 0.3, taper=False)
    hx, hy = 58.0, 36.0
    for fx in (52.0, 63.0):
        d.line("art_lsteel", [(hx, hy), (fx, yb - 12.0)], 1.4)
    d.oval("art_gold", hx, hy, 2.0, 2.0, ink=ink * 0.7)
    d.line("art_gold", [(hx, hy + 1.6), (hx, hy + 4.6)], 0.9, taper=False)
    place_name(d, a, lines=["Lost and", "Found"], where="right", room=6.0, at=24.0, caps=21.5)


# ---- 133: Tarheel Tess --------------------------------------------------------------------------------------------------------------
@painting(133)
def p133(d, a):   # a farm girl laughing, one boot stuck fast in a puddle of tar
    F = lady(d, 30.0, 4.0, 64.0, 1, pose="stride", arms=("wave", "hip"),
             wear={"top": "art_lblue", "dots": "art_white", "sleeves": "puff", "collar": "art_white", "apron": "art_white", "hem": "calf",
                   "boots": "art_brown", "hat": "art_red"},
             hair="pony", hair_paint="art_auburn", hat="kerchief")
    ink = d.ink
    fx = 0.07
    # (her laugh: her mouth open, her teeth)
    d.fill("art_dark", F.Ps([(fx - 0.15, 6.58), (fx - 0.06, 6.595), (fx + 0.07, 6.595), (fx + 0.15, 6.58), (fx + 0.1, 6.47), (fx, 6.43), (fx - 0.1, 6.47)]),
           ink=ink * 0.6, steps=2)
    d.fill("art_white", F.Ps([(fx - 0.11, 6.575), (fx + 0.11, 6.575), (fx + 0.1, 6.54), (fx - 0.1, 6.54)]), ink=0.0, steps=1)
    d.oval("art_lred", *F.P(fx, 6.47), F.r(0.06), F.r(0.03), ink=0.0)
    # (the tar: a black puddle round her aft boot, over its sole, strings of it stretching up her boot, its shine)
    ax, ay = F.P(-1.18, 0.0)
    d.fill("art_black", ellipse(ax + 1.0, ay + 0.2, 12.0, 2.6, 0, 20), ink=ink, steps=2)
    d.oval("art_black", ax + 0.6, ay + 1.2, 3.8, 1.9, ink=0.0)
    for (u, h) in ((-2.6, 3.6), (-0.6, 4.6), (1.8, 3.2), (3.4, 2.4)):
        d.limb("art_black", [(ax + u, ay + 0.6), (ax + u + 0.2, ay + h * 0.6), (ax + u * 0.8, ay + h)], [0.9, 0.35, 0.5], ink=0.0)
    for (u, v, r) in ((-6.0, 0.8, 1.6), (6.0, 0.2, 1.2), (8.6, 0.8, 0.6)):
        d.oval("art_lgrey", ax + u, ay + v, r, r * 0.3, ink=0.0)
    place_name(d, a, lines=["Tarheel", "Tess"], where="right", room=6.0, at=36.0, caps=21.5)


# ---- 134: Carousel Runaway ----------------------------------------------------------------------------------------------------------
def carousel_horse(d, x, y, size, tilt=14.0, coat="art_white", gilt="art_gold", trap="art_red", nod=-18.0):
    """A carved carousel horse in mid-leap to the right, forelegs tucked, hind legs stretched back, his body pitched up
    `tilt` degrees: gilt mane, tail and hooves, a red saddle and breast collar with jewels, his bridle. (x, y) is where
    his body would stand (as colt() is drawn, size 40); returns his saddle's top (where his pole went through)."""
    k = size / 40.0
    c, s = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))

    def P(u, v):
        dv = v - 26.0
        return (x + (u * c - dv * s) * k, y + (26.0 + u * s + dv * c) * k)

    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    # (his tail, gilt and flowing; his far legs)
    d.limb(gilt, Ps([(-12.0, 27.0), (-17.0, 26.0), (-21.0, 22.0), (-21.6, 16.0)]), [1.8 * k, 2.6 * k, 2.4 * k, 1.0 * k], ink=ink)
    for pts in (((8.0, 22.0), (13.0, 16.6), (10.0, 12.6)), ((-8.0, 22.0), (-14.0, 16.4), (-21.0, 13.0))):
        d.limb(coat, Ps(pts), [2.2 * k, 1.3 * k, 1.1 * k], ink=ink)
        d.oval(gilt, *P(*pts[2]), 1.4 * k, 1.0 * k, ink=ink * 0.7)
    # (his body, his neck arched up to his head)
    d.fill(coat, Ps([(-13.0, 26.0), (-11.0, 30.6), (0.0, 31.6), (10.0, 31.0), (14.0, 27.0), (12.0, 21.6), (0.0, 20.4), (-10.0, 21.6)]), ink=ink, steps=4)
    d.fill(coat, Ps([(6.0, 30.4), (10.0, 38.0), (14.0, 44.0), (19.4, 43.4), (17.6, 35.0), (14.4, 27.0)]), ink=ink, steps=3)
    # (the saddle cloth and saddle, the breast collar with its jewels)
    d.fill(trap, Ps([(-7.0, 31.4), (-7.6, 25.0), (-5.0, 23.6), (5.0, 23.6), (6.4, 25.4), (5.6, 31.6)]), ink=ink, steps=2)
    d.line(gilt, Ps([(-7.4, 25.4), (-5.0, 23.8), (5.0, 23.8), (6.2, 25.6)]), 0.6 * k, taper=False)
    d.fill(trap, Ps([(-5.0, 31.4), (-5.6, 34.0), (-2.6, 32.6), (2.6, 32.6), (4.8, 34.2), (4.4, 31.4)]), ink=ink, steps=2)
    d.limb(trap, Ps([(8.4, 31.0), (12.6, 27.6), (14.0, 23.4)]), [1.3 * k, 1.3 * k, 1.2 * k], ink=ink)
    for t in (0.2, 0.55, 0.9):
        u, v = lerp((8.4, 31.0), (14.0, 23.4), t)
        d.oval(gilt, *P(u + 0.4, v), 0.7 * k, 0.7 * k, ink=ink * 0.5)
    # (his carved mane: gilt flames falling back along his crest)
    for i in range(6):
        t = i / 5.0
        bu, bv = lerp((7.0, 31.8), (14.2, 45.0), t)
        d.fill(gilt, Ps([(bu + 0.6, bv + 0.8), (bu - 1.8, bv + 1.6), (bu - 4.4, bv - 0.6), (bu - 2.0, bv - 0.2), (bu - 0.2, bv - 1.4)]), ink=ink * 0.7, steps=2)
    # (his head, bowed a little: ears, an open eye, the nostril; the gilt bridle and the rein back to the saddle)
    G0 = _T(*P(15.6, 45.0), nod)
    G = lambda u, v: G0(u * k, v * k)                    # noqa: E731
    for (eu, lean) in ((-1.4, -10.0), (0.6, 8.0)):
        d.fill(coat, [G(eu - 0.9, 2.2), G(eu - 0.6 + lean * 0.05, 7.0), G(eu + 1.1, 2.6)], ink=ink, steps=2)
    d.fill(coat, [G(-3.0, 1.6), G(-1.6, 3.4), G(2.4, 3.0), G(8.0, 2.0), G(10.4, 1.0), G(10.6, -1.4), G(8.4, -2.6), G(2.6, -3.2), G(-2.4, -2.0)], ink=ink, steps=4)
    d.fill(gilt, [G(-1.0, 3.2), G(-0.4, 5.0), G(1.4, 3.0)], ink=ink * 0.6, steps=1)
    d.oval("art_white", *G(1.6, 0.9), 0.9 * k, 0.7 * k, ink=ink * 0.6)
    d.oval("art_black", *G(1.9, 0.9), 0.45 * k, 0.5 * k, ink=0.0)
    d.oval("art_ink", *G(9.6, 0.4), 0.5 * k, 0.35 * k, ink=0.0)
    d.line(gilt, [G(6.8, 2.4), G(7.2, -2.8)], 0.5 * k, taper=False)
    d.line(gilt, [G(1.0, 2.8), G(2.0, -3.0)], 0.5 * k, taper=False)
    d.line(gilt, [G(10.0, -1.6), G(4.0, -3.4)] + Ps([(10.0, 33.0), (5.0, 31.6)]), 0.4 * k, taper=False)
    # (his near legs: the foreleg tucked up under him, the hind leg stretched out back)
    for pts in (((11.0, 22.4), (16.4, 16.6), (12.6, 11.8)), ((-6.0, 23.0), (-13.4, 16.4), (-21.6, 12.0))):
        d.limb(coat, Ps(pts), [2.6 * k, 1.5 * k, 1.25 * k], ink=ink)
        d.oval(gilt, *P(*pts[2]), 1.6 * k, 1.1 * k, ink=ink * 0.7)
    return P(-0.2, 33.4)


@painting(134)
def p134(d, a):   # a carousel horse leaping off its pole, the pole left standing behind him
    ink = d.ink
    # (the brass pole he has left: twisted, its foot and its finial)
    px = 6.0
    d.line("art_gold", [(px, 0.0), (px, 56.0)], 2.2, taper=False)
    for v in range(2, 56, 4):
        d.line("art_lsteel", [(px - 1.0, v), (px + 1.0, v + 2.0)], 0.5, taper=False)
    d.oval("art_gold", px, 57.6, 2.0, 2.0, ink=ink * 0.7)
    d.shape("art_gold", [(px - 3.0, 0.0), (px - 2.2, 2.0), (px + 2.2, 2.0), (px + 3.0, 0.0)])
    # (the horse, in mid-air, away from it; the arcs of his leap)
    top = carousel_horse(d, 38.0, 2.0, 60.0)
    d.oval("art_dark", top[0], top[1], 1.0, 0.5, ink=ink * 0.5)
    for (u, v, r) in ((13.0, 26.0, 9.0), (13.0, 36.0, 7.0)):
        d.line("art_white", ellipse(u, v, r, r * 0.6, 0, 10, 150, 40), 0.7, taper=True)
    place_name(d, a, lines=["Carousel", "Runaway"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 135: Gandy Dancer --------------------------------------------------------------------------------------------------------------
@painting(135)
def p135(d, a):   # a track worker in overalls leaning on his lining bar, its foot under the rail
    ink = d.ink
    # (the track side-on: the ballast, a tie's end, the rail on it)
    d.fill("art_grey", [(0.0, 0.0), (0.0, 3.4), (20.0, 4.2), (40.0, 4.0), (60.0, 3.6), (60.0, 0.0)], ink=ink, steps=3)
    for (u, v) in ((4.0, 1.6), (11.0, 2.4), (19.0, 1.4), (27.0, 2.6), (35.0, 1.8), (44.0, 2.4), (52.0, 1.4), (57.0, 2.6)):
        d.oval("art_lgrey", u, v, 1.0, 0.6, ink=ink * 0.4)
    for u in (6.0, 22.0, 38.0, 54.0):
        d.shape("art_brown", [(u - 3.4, 3.2), (u - 3.4, 5.6), (u + 3.4, 5.6), (u + 3.4, 3.2)])
    d.shape("art_steel", [(0.0, 5.6), (0.0, 8.0), (60.0, 8.0), (60.0, 5.6)])
    d.line("art_lsteel", [(0.0, 7.7), (60.0, 7.7)], 0.5, taper=False)
    # (the man behind it, leaning on his bar, both hands on its top; the bar's foot under the rail)
    x, y, h = 24.0, 3.0, 64.0
    F = man(d, x, y, h, 1, pose="easy", tilt=8.0, arms=(to(1.5, 4.7, "down"), to(1.2, 4.9, "down")), hat="fedora",
            wear={"shirt": "art_red", "sleeves": "rolled", "overalls": "art_blue", "shoes": "art_brown", "hat": "art_brown", "band": "art_dark"},
            hair="short", mouth="smile")
    (fx, fy), _ = F.hand["fore"]
    (ax, ay), _ = F.hand["aft"]
    foot = (x + 17.0, 6.2)
    tx, ty = (fx + ax) / 2.0, max(fy, ay) + 2.4
    d.line("art_steel", [foot, (tx, ty)], 1.4, taper=False)
    d.line("art_lsteel", [(foot[0] - 0.3, foot[1] + 1.0), (tx - 0.3, ty)], 0.4, taper=False)
    d.shape("art_steel", [(foot[0] - 0.6, foot[1] + 0.8), (foot[0] + 2.8, foot[1] - 0.4), (foot[0] + 0.6, foot[1] - 1.0)])
    F.grip("aft")
    F.grip("fore")
    d.shape("art_steel", [(foot[0] - 2.0, 5.6), (foot[0] - 2.0, 8.0), (foot[0] + 4.0, 8.0), (foot[0] + 4.0, 5.6)])
    d.line("art_lsteel", [(foot[0] - 2.0, 7.7), (foot[0] + 4.0, 7.7)], 0.5, taper=False)
    place_name(d, a, lines=["Gandy", "Dancer"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 136: Crow's Nest Clem ----------------------------------------------------------------------------------------------------------
@painting(136)
def p136(d, a):   # a crow in a sailor's cap up in the crow's nest, his spyglass to his eye
    ink = d.ink
    mx = 26.0
    # (the shrouds, the mast, its pennant)
    for fx in (6.0, 46.0):
        d.line("art_tan", [(mx, 60.0), (fx, 0.0)], 0.4, taper=False)
    d.line("art_brown", [(mx, 0.0), (mx, 66.0)], 2.2, taper=False)
    # (the crow's nest: a barrel on the mast)
    w = 24.0
    top = barrel(d, mx, 34.0, w)
    # (the crow standing in it: his body, his head and beak, his eye; his sailor's cap)
    d.fill("art_black", [(mx - 8.0, top - 4.0), (mx - 9.0, top + 4.0), (mx - 5.0, top + 9.0), (mx + 2.0, top + 9.4), (mx + 5.6, top + 5.0), (mx + 5.0, top - 4.0)],
           ink=ink, steps=3)
    hx, hy = mx + 1.6, top + 13.0
    d.oval("art_black", hx, hy, 6.0, 5.4, ink=ink)
    d.fill("art_grey", [(hx + 4.4, hy - 0.6), (hx + 13.0, hy - 3.6), (hx + 4.2, hy - 4.6)], ink=ink * 0.8, steps=1)
    d.line("art_ink", [(hx + 4.6, hy - 2.6), (hx + 11.6, hy - 3.6)], 0.3, taper=False)
    d.fill("art_white", [(hx - 5.0, hy + 3.6), (hx - 4.6, hy + 7.6), (hx, hy + 8.4), (hx + 4.6, hy + 7.6), (hx + 5.0, hy + 3.6)], ink=ink * 0.8, steps=3)
    d.oval("art_white", hx, hy + 3.8, 5.8, 1.4, ink=ink * 0.7)
    # (the spyglass at his eye, three brass draws, and his wing holding it up)
    ex, ey = hx + 3.0, hy + 1.2
    d.oval("art_white", ex - 0.6, ey, 1.0, 1.0, ink=ink * 0.6)
    secs = [((ex, ey), (ex + 6.0, ey + 1.0), 1.5), ((ex + 6.0, ey + 1.0), (ex + 11.0, ey + 1.8), 1.25), ((ex + 11.0, ey + 1.8), (ex + 16.0, ey + 2.6), 1.05)]
    for (p, q, r) in secs:
        d.limb("art_gold", [p, q], [r, r], ink=ink * 0.8)
        d.line("art_dark", [(q[0] - 0.2, q[1] - r), (q[0] - 0.2, q[1] + r)], 0.4, taper=False)
    d.limb("art_black", [(mx + 3.0, top + 2.0), (hx + 9.0, top + 2.4), (ex + 12.0, ey - 2.2)], [3.0, 2.2, 1.4], ink=ink)
    for t in (-1.0, 0.0, 1.0):
        d.oval("art_black", ex + 12.6 + t * 1.2, ey + 0.4 + t * 0.4, 0.7, 1.6, turn=-20.0, ink=ink * 0.6)
    # (the barrel's rim in front of him)
    d.shape("art_brown", [(mx - w * 0.43, top - 2.6), (mx - w * 0.43, top), (mx + w * 0.43, top), (mx + w * 0.43, top - 2.6)])
    d.line("art_dark", [(mx - w * 0.44, top - 1.3), (mx + w * 0.44, top - 1.3)], w * 0.05, taper=False)
    place_name(d, a, lines=["Crow's", "Nest", "Clem"], where="right", room=6.0, at=50.0, gap=1.32, caps=21.5)
