"""Batch 4, numbers 197 to 200 (painter b4_i): a piggy bank in a flying helmet with coins dropping into it, a bomber's
pair of wheels reaching down for the runway like feet for their slippers, a hermit crab living in an engine cowling
and a pinto pony galloping under a striped sunset. Flat shapes with noseart's Drawing API and the figure kit; each name
set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T  # noqa: E402


# ---- 197: Piggy Bank -----------------------------------------------------------------------------------------------------------------
def gold_coin(d, x, y, r, turn=0.0, squash=0.45):
    """A coin seen edge-on-ish, tumbling: its face an ellipse, its milled edge showing."""
    ink = d.ink
    d.oval("art_gold", x + 0.3, y - 0.4, r, r * squash, turn=turn, ink=ink * 0.8)
    d.oval("art_yellow", x, y, r, r * squash, turn=turn, ink=ink * 0.8)
    d.oval("art_gold", x, y, r * 0.6, r * squash * 0.6, turn=turn, ink=0.0)


@painting(197)
def p197(d, a):   # a china piggy bank in a flying helmet, its goggles up, coins tumbling down into the slot in its back
    ink = d.ink
    x, y, k = 26.0, 2.0, 1.25
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    # (its far legs, its body, its near legs: stubby, round-ended)
    for u in (-8.0, 7.0):
        d.limb("art_pink", Ps([(u, 8.0), (u + 0.4, 1.6)]), [2.6 * k, 2.4 * k], ink=ink * 0.8)
    d.oval("art_pink", *P(0.0, 15.0), 17.0 * k, 11.6 * k, ink=ink)
    for u in (-11.0, 4.0):
        d.limb("art_pink", Ps([(u, 8.0), (u - 0.2, 1.4)]), [2.9 * k, 2.7 * k], ink=ink)
        d.oval("art_pink", *P(u - 0.2, 1.2), 2.7 * k, 1.0 * k, ink=ink * 0.7)
    # (its curly tail; its snout's flat disc with two nostrils; its ear; its shine)
    d.line("art_pink", Ps([(-16.6, 17.0), (-19.6, 18.0), (-20.6, 20.4), (-19.0, 21.6), (-17.8, 20.0), (-19.0, 18.8), (-21.6, 19.4)]), 0.9 * k)
    d.oval("art_pink", *P(17.2, 13.4), 2.4 * k, 4.0 * k, ink=ink)
    for dv in (-1.4, 1.4):
        d.oval("art_ink", *P(17.8, 13.4 + dv), 0.5 * k, 0.8 * k, ink=0.0)
    d.line("art_white", ellipse(*P(-2.0, 16.0), 12.0 * k, 8.0 * k, 0.0, 24, 110.0, 160.0), 1.0 * k, taper=True)
    d.oval("art_ink", *P(11.0, 17.4), 0.9 * k, 1.1 * k, ink=0.0)
    d.oval("art_white", *P(11.3, 17.8), 0.3 * k, 0.3 * k, ink=0.0)
    d.line("art_ink", Ps([(13.0, 11.0), (14.8, 10.4), (16.0, 11.2)]), 0.35 * k)
    d.oval("art_red", *P(9.0, 12.4), 1.6 * k, 1.0 * k, ink=0.0)
    # (the helmet, on its brow, its strap under its chin; the ear out through the side)
    helmet(d, *P(8.8, 22.4), 13.0 * k, 1, paint="art_brown", view="side", inside=None, turn=-20.0)
    d.fill("art_pink", Ps([(4.6, 24.6), (2.0, 30.0), (6.4, 27.4)]), ink=ink, steps=1)
    # (the slot in its back, a coin half in it, more tumbling down)
    d.shape("art_dark", Ps([(-6.0, 26.2), (0.4, 26.4), (0.4, 27.2), (-6.0, 27.0)]), ink=0.0)
    d.fill("art_yellow", Ps([(-5.0, 26.8), (-4.6, 30.0), (-3.0, 31.4), (-1.4, 30.0), (-1.0, 26.8)]), ink=ink * 0.8, steps=2)
    for (u, v, r, t) in ((-3.0, 37.0, 3.0, 20.0), (-6.0, 45.0, 3.0, -30.0), (0.0, 53.0, 3.0, 50.0)):
        gold_coin(d, *P(u, v), r * k, turn=t)
    for (u, v) in ((-1.0, 33.4), (-4.4, 41.0), (-2.0, 49.4)):
        d.line("art_white", Ps([(u - 1.0, v), (u + 1.0, v + 0.4)]), 0.4 * k, taper=True)
    place_name(d, a, lines=["Piggy", "Bank"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 198: Wheels Down ----------------------------------------------------------------------------------------------------------------
@painting(198)
def p198(d, a):   # a bomber coming in head-on, its two main wheels stretched down for the runway like feet for their slippers
    ink = d.ink
    cx = 36.0
    # (the runway: its edge lines, centre dashes and threshold bars; a pair of slippers waiting on it)
    d.shape("art_dark", [(-6.0, 0.0), (78.0, 0.0), (58.0, 14.0), (14.0, 14.0)], ink=ink)
    for i in range(4):
        t0, t1 = i / 4.0 + 0.04, i / 4.0 + 0.16
        w0, w1 = 1.6 * (1 - t0) + 0.6 * t0, 1.6 * (1 - t1) + 0.6 * t1
        d.shape("art_white", [(cx - w0, 14.0 * t0), (cx + w0, 14.0 * t0), (cx + w1, 14.0 * t1), (cx - w1, 14.0 * t1)], ink=0.0)
    for i in range(5):
        for su in (-1.0, 1.0):
            u = cx + su * (10.0 + i * 5.4)
            d.shape("art_white", [(u - 1.6, 0.8), (u + 1.6, 0.8), (u + 1.2 + su * 0.6, 3.6), (u - 1.2 + su * 0.6, 3.6)], ink=0.0)
    for su in (-1.0, 1.0):                               # (fluffy white slippers, toes toward us, their opening dark)
        sx = cx + su * 13.0
        d.fill("art_white", [(sx - 4.6, 4.6), (sx - 5.0, 9.0), (sx - 3.0, 11.6), (sx + 3.0, 11.6), (sx + 5.0, 9.0), (sx + 4.6, 4.6)], ink=ink, steps=3)
        d.fill("art_dark", [(sx - 3.0, 8.6), (sx - 2.4, 10.6), (sx + 2.4, 10.6), (sx + 3.0, 8.6)], ink=0.0, steps=2)
        for u in (-3.6, -1.2, 1.2, 3.6):
            d.oval("art_white", sx + u, 5.6, 1.6, 1.4, ink=ink * 0.5)
    # (the bomber head-on: its fin over its body, the tailplane behind, the long wing, four engines and their propeller
    #  blurs; its glazed nose)
    wy = 44.0
    d.shape("art_lsteel", [(cx - 1.0, wy + 4.0), (cx + 1.0, wy + 4.0), (cx + 0.8, wy + 17.0), (cx - 0.8, wy + 17.0)], ink=ink)
    d.shape("art_lsteel", [(cx - 14.0, wy + 6.0), (cx + 14.0, wy + 6.0), (cx + 14.0, wy + 7.0), (cx - 14.0, wy + 7.0)], ink=ink * 0.8)
    d.fill("art_lsteel", [(cx - 40.0, wy + 4.2), (cx - 40.0, wy + 5.4), (cx, wy + 2.4), (cx + 40.0, wy + 5.4), (cx + 40.0, wy + 4.2), (cx, wy - 1.2)], ink=ink, steps=1)
    for su in (-1.0, 1.0):
        for e in (12.0, 25.0):
            ex, ey = cx + su * e, wy - 1.0 + e * 0.05
            d.oval("art_lsteel", ex, ey, 3.6, 3.6, ink=ink)
            d.oval("art_dark", ex, ey, 2.4, 2.4, ink=0.0)
            d.oval("art_lgrey", ex, ey, 0.8, 0.8, ink=0.0)
            d.line("art_white", ellipse(ex, ey, 7.0, 7.0, 0.0, 30, 30.0, 150.0), 0.35, taper=True)
            d.line("art_white", ellipse(ex, ey, 7.0, 7.0, 0.0, 30, 210.0, 330.0), 0.35, taper=True)
    d.oval("art_lsteel", cx, wy + 0.6, 5.6, 5.8, ink=ink)
    d.oval("art_sky", cx, wy + 0.2, 3.2, 3.2, ink=ink * 0.6)
    d.line("art_white", [(cx - 2.0, wy + 1.6), (cx - 0.6, wy + 2.6)], 0.4, taper=True)
    # (its two main wheels stretched down from the inner engines, toes pointed for the slippers; their reach)
    for su in (-1.0, 1.0):
        ex = cx + su * 12.0
        wx = cx + su * 13.0
        d.line("art_lsteel", [(ex - 0.8 * su, wy - 4.0), (wx - 0.4 * su, 22.0)], 1.2, taper=False)
        d.line("art_lsteel", [(ex + 1.6 * su, wy - 4.0), (wx + 0.6 * su, 24.0)], 0.7, taper=False)
        d.oval("art_black", wx, 18.6, 2.6, 5.4, ink=ink)
        d.oval("art_lgrey", wx, 19.0, 0.9, 2.2, ink=0.0)
        for dv in (0.0, 3.0):
            d.line("art_white", [(wx + su * 4.4, 22.0 + dv), (wx + su * 4.4, 17.0 + dv)], 0.45, taper=True)
    place_name(d, a, lines=["Wheels", "Down"], where="right", room=6.0, at=32.0, caps=21.5)


# ---- 199: Hardstand Hermit -----------------------------------------------------------------------------------------------------------
@painting(199)
def p199(d, a):   # a hermit crab at home in an old engine cowling on the hardstand, claws over its lip, eyes up on stalks
    ink = d.ink
    x0, y0, r = 32.0, 27.0, 25.0
    d.oval("art_lgrey", x0, 3.0, 40.0, 4.6, ink=0.0)                 # (the hardstand's concrete)
    for (u, v) in ((x0 - 34.0, 3.0), (x0 + 36.0, 4.0)):
        for t in (-30.0, 0.0, 30.0):
            d.line("art_green", [(u, v), (u + 3.0 * math.sin(math.radians(t)), v + 4.0)], 0.6, taper=True)
    # (the cowling: its olive ring, its rim, empty inside but for the hollow dark and a bolt ring at the back: his house)
    d.oval("art_olive", x0, y0, r, r, ink=ink)
    d.oval("art_dark", x0, y0, r * 0.8, r * 0.8, ink=ink * 0.8)
    d.line("art_grey", ellipse(x0, y0, r * 0.5, r * 0.5, 0.0, 36), 0.5, taper=False)
    for i in range(12):
        t = math.radians(i * 30.0)
        d.oval("art_grey", x0 + r * 0.5 * math.cos(t), y0 + r * 0.5 * math.sin(t), 0.6, 0.6, ink=0.0)
    d.line("art_lgrey", ellipse(x0, y0, r * 0.92, r * 0.92, 0.0, 36, 100.0, 170.0), 0.9, taper=True)
    for i in range(16):
        t = math.radians(i * 22.5)
        d.oval("art_steel", x0 + r * 0.9 * math.cos(t), y0 + r * 0.9 * math.sin(t), 0.45, 0.45, ink=0.0)
    # (his walking legs out over the lower lip, planted on the concrete close by)
    lip = y0 - r * 0.8
    for su in (-1.0, 1.0):
        for i, (ku, fu) in enumerate(((9.0, 14.0), (14.0, 21.0))):
            pts = [(x0 + su * (3.0 + i * 3.0), lip + 4.0), (x0 + su * ku, lip + 1.0), (x0 + su * fu, 4.0)]
            d.limb("art_orange", pts, [1.6, 1.3, 0.7], ink=ink)
            d.oval("art_cream", x0 + su * ku, lip + 1.0, 0.7, 0.7, ink=0.0)
    # (his body in the hollow, his eyes up on stalks, his feelers; the small claw on the lip, the big one held across)
    d.oval("art_red", x0, lip + 7.0, 8.0, 5.0, ink=ink)
    for su in (-1.0, 1.0):
        d.line("art_orange", [(x0 + su * 2.0, lip + 10.0), (x0 + su * 4.4, lip + 21.0)], 1.0, taper=False)
        d.oval("art_black", x0 + su * 4.6, lip + 22.4, 2.0, 2.3, ink=ink * 0.6)
        d.oval("art_white", x0 + su * 4.0, lip + 23.2, 0.6, 0.6, ink=0.0)
        d.line("art_orange", [(x0 + su * 1.0, lip + 11.0), (x0 + su * 8.0, lip + 26.0), (x0 + su * 15.0, lip + 33.0)], 0.35, taper=True)
    sm = (x0 + 9.0, lip + 1.0)
    d.limb("art_orange", [(x0 + 4.0, lip + 5.0), (x0 + 7.0, lip + 3.0), sm], [1.6, 1.4, 1.2], ink=ink)
    d.fill("art_orange", [(sm[0] - 1.6, sm[1] - 1.4), (sm[0] + 4.0, sm[1] - 1.8), (sm[0] + 5.0, sm[1] - 0.2), (sm[0] + 2.0, sm[1] + 1.6), (sm[0] - 1.4, sm[1] + 1.4)],
           ink=ink, steps=2)
    C = _T(x0 - 3.0, lip + 2.0, 6.0)                     # (the big claw: its arm, its broad hand, its pincers)
    d.limb("art_orange", [(x0 - 5.0, lip + 6.0), C(-8.0, 1.0), C(-4.0, -1.0)], [2.6, 2.4, 2.2], ink=ink)
    d.fill("art_orange", [C(-6.0, -4.0), C(-7.0, 1.6), C(-2.0, 4.0), C(8.0, 3.4), C(11.0, 1.0), C(9.0, -3.6), C(2.0, -5.0)], ink=ink, steps=3)
    d.fill("art_orange", [C(9.0, 2.6), C(16.0, 3.4), C(17.0, 1.6), C(11.0, 0.8)], ink=ink, steps=2)
    d.fill("art_orange", [C(9.0, -3.0), C(15.6, -0.6), C(16.0, -2.0), C(11.0, -3.8)], ink=ink, steps=2)
    for (u, v) in ((-2.0, 0.6), (2.0, -1.4), (5.0, 1.4), (1.0, 2.0), (6.6, -1.6)):
        d.oval("art_cream", *C(u, v), 0.8, 0.8, ink=0.0)
    place_name(d, a, lines=["Hardstand", "Hermit"], where="below", room=5.0, caps=21.5)


# ---- 200: Painted Desert Pony --------------------------------------------------------------------------------------------------------
def pony(d, x, y, size, coat="art_white", patch="art_black", mane="art_black", hoof="art_dark"):
    """A pinto pony at full gallop to the right, all four feet off the ground: his feet's line at y, his body over x,
    `size` about his height at the withers (40 a pony's); patches on his rump, shoulder and neck, his mane and tail
    streaming."""
    k = size / 40.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    # (his tail streaming back; his far legs, one reaching forward, one thrust back)
    d.fill(mane, Ps([(-13.0, 30.0), (-19.0, 31.6), (-27.0, 30.0), (-31.0, 26.6), (-24.0, 27.6), (-19.0, 26.6), (-14.0, 26.6)]), ink=ink * 0.8, steps=3)
    d.limb("art_lgrey", Ps([(9.0, 22.0), (17.0, 18.0), (22.0, 13.0)]), [2.6 * k, 1.6 * k, 1.3 * k], ink=ink * 0.8)
    d.oval(hoof, *P(23.0, 12.4), 1.6 * k, 1.1 * k, turn=-40.0, ink=ink * 0.6)
    d.limb("art_lgrey", Ps([(-9.0, 22.0), (-16.0, 16.0), (-25.0, 12.0)]), [2.8 * k, 1.7 * k, 1.3 * k], ink=ink * 0.8)
    d.oval(hoof, *P(-26.0, 11.6), 1.6 * k, 1.1 * k, turn=20.0, ink=ink * 0.6)
    # (his body, his neck stretched out, his patches)
    d.fill(coat, Ps([(-14.0, 26.0), (-12.0, 31.0), (0.0, 32.0), (10.0, 31.4), (15.0, 27.0), (13.0, 21.4), (0.0, 20.4), (-11.0, 21.6)]), ink=ink, steps=4)
    d.fill(coat, Ps([(8.0, 30.6), (14.0, 37.0), (19.0, 41.0), (24.4, 39.4), (19.6, 31.0), (14.6, 25.6)]), ink=ink, steps=3)
    for pts in ([(-12.0, 25.0), (-11.0, 29.6), (-5.0, 30.6), (-3.0, 26.0), (-6.0, 22.4), (-10.6, 22.4)], [(3.0, 30.4), (8.0, 30.6), (9.6, 26.0), (5.0, 23.6), (2.0, 26.4)],
                [(14.0, 33.0), (17.4, 37.6), (20.0, 37.0), (18.0, 32.4)]):
        d.fill(patch, Ps(pts), ink=0.0, steps=3)
    # (his mane flying back off his crest)
    for i in range(5):
        t = i / 4.0
        bu, bv = lerp((9.0, 32.0), (19.0, 41.6), t)
        d.fill(mane, Ps([(bu + 0.4, bv + 0.6), (bu - 3.0, bv + 2.4), (bu - 6.0, bv + 1.0), (bu - 2.4, bv - 0.4), (bu - 0.4, bv - 1.2)]), ink=ink * 0.6, steps=2)
    # (his head reaching out: ears pricked, eye, nostril, a white blaze)
    G0 = _T(*P(22.0, 40.4), -32.0)
    G = lambda u, v: G0(u * k, v * k)                    # noqa: E731
    for (eu, lean) in ((-1.6, -6.0), (0.4, 6.0)):
        d.fill(coat, [G(eu - 0.8, 2.2), G(eu - 0.4 + lean * 0.05, 6.4), G(eu + 1.0, 2.6)], ink=ink, steps=2)
    d.fill(coat, [G(-3.0, 1.8), G(-1.4, 3.4), G(2.4, 3.0), G(8.0, 2.0), G(10.4, 0.8), G(10.4, -1.6), G(8.2, -2.6), G(2.6, -3.0), G(-2.4, -2.0)], ink=ink, steps=4)
    d.fill(patch, [G(-2.6, 1.6), G(-1.4, 3.2), G(2.0, 2.8), G(3.6, 0.0), G(1.6, -2.8), G(-2.2, -1.8)], ink=0.0, steps=3)
    d.oval("art_white", *G(1.4, 0.9), 0.9 * k, 0.7 * k, ink=ink * 0.6)
    d.oval("art_black", *G(1.7, 0.9), 0.45 * k, 0.5 * k, ink=0.0)
    d.oval("art_ink", *G(9.6, 0.0), 0.5 * k, 0.35 * k, ink=0.0)
    # (his near legs: the foreleg folded under him, the hind leg reaching forward under his belly)
    d.limb(coat, Ps([(11.0, 22.4), (17.0, 17.6), (13.4, 12.6)]), [2.9 * k, 1.7 * k, 1.4 * k], ink=ink)
    d.oval(hoof, *P(13.0, 11.8), 1.7 * k, 1.2 * k, turn=60.0, ink=ink * 0.6)
    d.limb(coat, Ps([(-6.0, 23.0), (-2.0, 15.4), (4.0, 12.0)]), [3.0 * k, 1.8 * k, 1.4 * k], ink=ink)
    d.oval(hoof, *P(4.6, 11.6), 1.7 * k, 1.2 * k, turn=-20.0, ink=ink * 0.6)
    return P


@painting(200)
def p200(d, a):   # a pinto pony at full gallop across the painted desert under a sunset sky in bands
    ink = d.ink
    cx, cy0, R, yb = 34.0, 22.0, 34.0, 8.0                # (the sky: an arch, its round top over the mesas)

    def half(v):
        return R if v <= cy0 else math.sqrt(max(0.0, R * R - (v - cy0) ** 2))
    bands = ["art_yellow", "art_orange", "art_red", "art_orange", "art_yellow", "art_red", "art_orange"]
    top = cy0 + R
    edges = [yb + (top - yb) * i / len(bands) for i in range(len(bands) + 1)]
    for i, paint in enumerate(bands):
        v0, v1 = edges[i], edges[i + 1]
        vs = [v0 + (v1 - v0) * j / 6 for j in range(7)]
        d.shape(paint, [(cx - half(v), v) for v in vs] + [(cx + half(v), v) for v in reversed(vs)], ink=0.0)
    arch = [(cx - R, yb)] + [(cx + R * math.cos(math.radians(t)), cy0 + R * math.sin(math.radians(t))) for t in range(180, -1, -6)] + [(cx + R, yb)]
    d.line("art_ink", arch, 0.7, taper=False)
    # (the sun going down behind the mesas; the mesas; the desert floor)
    d.oval("art_white", cx + 12.0, yb + 4.0, 9.0, 9.0, ink=0.0)
    d.shape("art_red", [(cx - R - 2.0, 0.0), (cx - R - 2.0, 14.0), (cx - 26.0, 14.0), (cx - 24.0, 22.0), (cx - 14.0, 22.0), (cx - 12.0, 14.0),
                        (cx + 4.0, 14.0), (cx + 6.0, 18.0), (cx + 16.0, 18.0), (cx + 18.0, 12.0), (cx + R + 2.0, 12.0), (cx + R + 2.0, 0.0)], ink=ink)
    for v in (16.0, 19.0):
        d.line("art_yellow", [(cx - 25.0, v), (cx - 13.0, v)], 0.6, taper=False)
    d.shape("art_yellow", [(cx - R - 2.0, -1.0), (cx + R + 2.0, -1.0), (cx + R + 2.0, 6.0), (cx - R - 2.0, 6.0)], ink=ink)
    # (the pony galloping over it, a little dust off his heels)
    pony(d, cx - 2.0, 0.0, 38.0)
    for (u, v, w, h) in ((cx - 30.0, 6.0, 8.0, 4.0), (cx - 36.0, 9.0, 6.0, 3.0)):
        cloud(d, u, v, w, h, "art_white", seed=int(u), flat=True, shade=None)
    place_name(d, a, lines=["Painted", "Desert", "Pony"], where="right", room=6.0, at=32.0, caps=21.5)
