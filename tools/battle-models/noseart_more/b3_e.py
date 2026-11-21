"""Batch 3, numbers 109 to 115 (painter b3_e): a soldier at his mountain of potatoes, a catfish fishing with a bomb for
bait, a woman holding her hat on in a gale, a porch lamp and its moths in flying helmets, a pair of patched overalls
walking by themselves, a hawk with a bombsight under his wing, and a bullfrog singing bass on his lily pad. Flat shapes
with noseart's Drawing API and the figure kit; each name set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T, _lady, note  # noqa: E402
from noseart_more.b3_b import straw_hat  # noqa: E402


def _arc(cx, cy, rx, ry, a0, a1, n=12):
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
            for i in range(n + 1)]


# ---- 109: Spud Duty -------------------------------------------------------------------------------------------------------------
def potato(d, x, y, r, turn=0.0, skin="art_lbrown", eye="art_brown"):
    d.oval(skin, x, y, r, r * 0.7, turn=turn, ink=d.ink * 0.8)
    G = _T(x, y, turn)
    for (u, v) in ((-0.4, 0.2), (0.3, -0.1)):
        d.oval(eye, *G(u * r, v * r), r * 0.08, r * 0.06, ink=0.0)


@painting(109)
def p109(d, a):   # a soldier on an upturned bucket peeling a mountain of potatoes
    rng = random.Random(109)
    # (the mountain, behind him and up past his head)
    hx0, hx1, top = 32.0, 70.0, 56.0
    spuds = []
    for i in range(70):
        u = rng.uniform(hx0, hx1)
        t = abs(u - (hx0 + hx1) / 2) / ((hx1 - hx0) / 2)
        ymax = top * (1.0 - t ** 1.5)
        spuds.append((u, rng.uniform(1.0, max(2.0, ymax - 2.0)), rng.uniform(2.4, 3.4), rng.uniform(-40, 40)))
    spuds.sort(key=lambda p: -p[1])
    for (u, v, r, turn) in spuds:
        potato(d, u, v + 1.0, r, turn)
    # (the bucket he sits on, the pot the peeled ones go in, the peelings on the ground)
    d.shape("art_lsteel", [(8.0, 0.0), (6.6, 14.0), (21.4, 14.0), (20.0, 0.0)])
    for v in (3.0, 11.0):
        d.line("art_grey", [(7.4, v), (20.6, v)], 0.6, taper=False)
    d.fill("art_grey", [(26.0, 0.0), (25.0, 9.0), (36.0, 9.0), (35.0, 0.0)], steps=1)
    d.oval("art_dark", 30.5, 9.0, 5.6, 1.2, ink=d.ink * 0.6)
    d.oval("art_cream", 29.0, 9.6, 2.0, 1.0, ink=d.ink * 0.5)
    for (u, v) in ((22.0, 1.0), (24.0, 2.4)):
        d.line("art_lbrown", _arc(u, v, 1.6, 1.0, 0, 300, 10), 0.6)
    F = man(d, 14.0, 0.0, 50.0, 1, pose="sit", arms=(to(1.7, 4.9, "down"), to(1.26, 4.62, "down")), hat="cap",
            wear={"shirt": "art_olive", "sleeves": "rolled", "trousers": "art_olive", "shoes": "art_brown", "cap": "art_olive", "belt": None},
            hair="short", mouth="flat", eyes="open")
    (fx, fy), _ = F.hand["fore"]
    (ax, ay), _ = F.hand["aft"]
    potato(d, ax + 1.0, ay + 0.6, 2.6, 10.0)
    d.line("art_lsteel", [(fx - 0.4, fy + 0.4), (fx - 3.0, fy + 1.4)], 0.6, taper=False)                 # (his paring knife)
    d.line("art_lbrown", [(ax + 1.6, ay - 1.0), (ax + 2.6, ay - 3.4), (ax + 1.6, ay - 5.6), (ax + 2.8, ay - 7.6), (ax + 2.0, ay - 9.4)], 0.6)   # (the peel)
    F.grip("fore")
    F.grip("aft")
    place_name(d, a, lines=["Spud", "Duty"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 110: Catfish Carl -------------------------------------------------------------------------------------------------------------
def catfish(d, x, y, size, coat="art_grey", belly="art_cream", fin="art_dark", braces="art_red"):
    """A catfish sitting up on his tail on a dock, facing right, `size` from his tail to his hat's brim: a broad flat
    head with a wide mouth, small eyes, long whiskers; his pale belly with braces over it; a pectoral fin for a hand.
    (x, y) is where he sits. Returns his fin's tip (his hand) and his head's top."""
    k = size / 40.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    d.fill(fin, Ps([(-6.0, 2.0), (-14.0, -1.0), (-13.0, 5.0), (-6.0, 5.0)]), ink=ink, steps=2)                      # (his tail fin)
    d.fill(coat, Ps([(-8.0, 3.0), (-9.0, 12.0), (-8.0, 22.0), (-4.0, 30.0), (6.0, 31.0), (10.0, 24.0), (9.0, 12.0), (6.0, 1.0), (-2.0, 0.0)]),
           ink=ink, steps=4)
    d.fill(belly, Ps([(8.6, 22.0), (8.4, 12.0), (5.6, 2.0), (0.0, 1.0), (1.0, 12.0), (3.0, 22.0)]), ink=0.0, steps=4)
    for (u0, u1) in ((-2.0, -3.0), (5.4, 6.6)):
        d.line(braces, Ps([(u0, 26.0), (u1 * 0.9 + 0.6, 4.0)]), 1.0 * k, taper=False)
    d.line(braces, Ps([(-6.6, 4.4), (8.0, 4.0)]), 1.0 * k, taper=False)
    d.fill(fin, Ps([(-8.4, 14.0), (-12.6, 18.0), (-11.6, 22.0), (-8.6, 22.0)]), ink=ink, steps=2)                 # (his back fin)
    # (his head: broad and flat, wider than he is, the wide mouth across its front, whiskers drooping long)
    d.fill(coat, Ps([(-6.0, 26.0), (-6.4, 33.0), (0.0, 36.6), (12.0, 36.6), (22.0, 33.0), (24.0, 28.6), (20.0, 25.0), (8.0, 23.6)]), ink=ink, steps=4)
    d.fill(belly, Ps([(23.6, 28.4), (20.0, 25.4), (10.0, 24.2), (12.0, 27.0), (20.0, 28.2)]), ink=0.0, steps=3)
    d.line("art_ink", Ps([(10.0, 28.0), (17.0, 28.4), (23.8, 29.0)]), 0.6 * k)
    d.oval("art_white", *P(14.0, 33.0), 1.4 * k, 1.4 * k, ink=ink * 0.6)
    d.oval("art_black", *P(14.4, 33.0), 0.7 * k, 0.7 * k, ink=0.0)
    for (u, v, du, dv) in ((22.4, 31.6, 12.0, -6.0), (23.4, 29.6, 10.0, -12.0), (20.0, 27.6, 4.0, -12.0), (16.0, 27.4, -1.0, -10.0)):
        d.line(fin, Ps([(u, v), (u + du * 0.5, v + dv * 0.25), (u + du, v + dv)]), 0.7 * k)
    hand = P(14.0, 16.0)
    d.fill(fin, Ps([(4.0, 20.0), (10.0, 18.0), (14.6, 16.6), (12.6, 14.4), (6.0, 16.0)]), ink=ink, steps=3)             # (his fin, out: his hand)
    return hand, P(4.0, 36.4)


@painting(110)
def p110(d, a):   # a catfish in a straw hat and braces, fishing with a bomb for bait
    # (the water and the dock's end he sits on)
    d.fill("art_blue", [(14.0, -6.0), (14.0, 2.0), (80.0, 2.0), (80.0, -6.0)], steps=1)
    d.shape("art_lbrown", [(-6.0, 4.0), (-6.0, 8.0), (26.0, 8.0), (26.0, 4.0)])
    for u in (2.0, 10.0, 18.0):
        d.line("art_brown", [(u, 4.2), (u, 7.8)], 0.4, taper=False)
    d.shape("art_brown", [(20.0, -6.0), (20.0, 4.0), (24.0, 4.0), (24.0, -6.0)])
    hand, top = catfish(d, 8.0, 8.0, 44.0)
    straw_hat(d, top[0] + 2.0, top[1] - 0.8, 20.0, turn=-6.0)
    # (his pole: out over the water; its line down to the bait, a small bomb dangling)
    tip = (64.0, 44.0)
    d.limb("art_tan", [(hand[0] - 4.0, hand[1] - 3.0), hand, lerp(hand, tip, 0.5), tip], [1.4, 1.3, 1.0, 0.6], ink=d.ink * 0.8)
    d.line("art_white", [tip, (tip[0] + 0.6, 26.0), (tip[0] + 0.4, 16.0)], 0.4, taper=False)
    bomb(d, tip[0] + 0.4, 4.0, 12.0, turn=-90)
    d.line("art_white", _arc(tip[0] + 0.4, 2.4, 6.0, 1.4, 190, 350, 8), 0.6)
    place_name(d, a, lines=["Catfish", "Carl"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 111: Windy City Wanda ------------------------------------------------------------------------------------------------------------
@painting(111)
def p111(d, a):   # a woman in a long coat holding her hat on in a gale
    F = _lady(d, 26.0, 2.0, 64.0, 1, extra_arms={"_b3e_hat": ((0.62, 1.1), (-0.66, 1.92))}, pose="stride", arms=("_b3e_hat", "down"),
              wear={"top": "art_tan", "coat": "art_tan", "hem": "long", "collar": "art_tan", "belt": "art_tan", "buttons": "art_brown",
                    "sleeves": "long", "boots": "art_brown"},
              hair="long", hair_paint="art_hair", hat="straw")
    P, Ps, R = F.P, F.Ps, F.r
    (hx, hy), _ = F.hand["fore"]
    d.oval("art_skin", hx, hy, R(0.15), R(0.13), ink=d.ink * 0.7)
    # (the gale: from the right, her coat's hem blown back, streaks past her, three leaves going by)
    d.fill("art_tan", Ps([(-0.62, 1.2), (-1.5, 1.6), (-1.9, 2.6), (-1.2, 2.4), (-0.6, 3.2)]), steps=3)
    for (u, v, L) in ((2.6, 7.0, 3.0), (3.0, 5.0, 3.6), (2.4, 3.0, 2.6), (-2.8, 6.0, 2.4), (-3.0, 4.0, 3.0)):
        x0, y0 = P(u, v)
        d.line("art_white", [(x0 + L * F.k * 0.5, y0 + 0.6), (x0, y0), (x0 - L * F.k * 0.5, y0 - 0.4)], 0.7)
    for (u, v, turn) in ((-2.4, 8.2, 30), (-3.6, 5.2, -20), (-2.0, 2.4, 60)):
        G = _T(*P(u, v), turn)
        d.fill("art_red", [G(-2.0, 0.0), G(0.0, 1.0), G(2.0, 0.0), G(0.0, -1.0)], ink=d.ink * 0.7, steps=2)
    place_name(d, a, lines=["Windy", "City", "Wanda"], where="right", room=6.0, at=38.0, gap=1.32, caps=21.5)


# ---- 112: Porch Light -------------------------------------------------------------------------------------------------------------
def moth(d, x, y, size, turn=0.0, wing_="art_tan", spot="art_brown", body="art_lbrown"):
    """A big moth, front on, flying: four broad wings with eye spots, a furry body, feathered antennae; a flying helmet
    on its head, goggles up. (x, y) its body's middle, `size` its span."""
    G0 = _T(x, y, turn)
    k = size / 30.0
    G = lambda u, v: G0(u * k, v * k)                    # noqa: E731
    ink = d.ink
    for su in (-1, 1):
        d.fill(wing_, [G(su * 1.0, 1.0), G(su * 8.0, 9.0), G(su * 14.0, 8.0), G(su * 15.0, 2.0), G(su * 8.0, -0.6)], ink=ink, steps=4)
        d.fill(wing_, [G(su * 1.0, -1.0), G(su * 9.0, -1.4), G(su * 10.0, -7.0), G(su * 5.0, -9.0), G(su * 1.4, -4.0)], ink=ink, steps=4)
        d.oval(spot, *G(su * 9.6, 4.4), 2.0 * k, 2.0 * k, ink=0.0)
        d.oval("art_cream", *G(su * 9.6, 4.4), 0.9 * k, 0.9 * k, ink=0.0)
        d.oval(spot, *G(su * 6.0, -4.6), 1.2 * k, 1.2 * k, ink=0.0)
    d.fill(body, [G(0.0, 4.0), G(1.6, 2.0), G(1.4, -6.0), G(0.0, -8.0), G(-1.4, -6.0), G(-1.6, 2.0)], ink=ink, steps=3)
    for v in (-2.0, -4.0):
        d.line(spot, [G(-1.2, v), G(1.2, v)], 0.3 * k)
    hx, hy = G(0.0, 5.6)
    d.oval(body, hx, hy, 2.4 * k, 2.2 * k, ink=ink)
    for su in (-1, 1):
        d.line(spot, [G(su * 1.0, 7.4), G(su * 3.0, 11.0), G(su * 5.2, 12.2)], 0.4 * k)
        for t in (0.4, 0.7):
            px, py = lerp(G(su * 1.6, 8.4), G(su * 4.4, 11.8), t)
            d.line(spot, [(px, py), (px + su * 0.8 * k, py - 0.6 * k)], 0.25 * k)
    helmet(d, hx, hy + 0.3 * k, 4.8 * k, 1, paint="art_brown", view="front", inside=None)
    for su in (-1, 1):
        d.oval("art_white", hx + su * 0.9 * k, hy - 0.4 * k, 0.55 * k, 0.65 * k, ink=0.0)
        d.oval("art_black", hx + su * 0.9 * k, hy - 0.4 * k, 0.28 * k, 0.32 * k, ink=0.0)


def porch_lamp(d, x, y, w, frame="art_black", glass="art_yellow", glow="art_cream", wall="art_lbrown"):
    """A porch lantern on its wall plate, front on, its glass box's middle at (x, y), `w` wide: the plate, the arm, the
    cap, the four-paned glass lit from inside, the bulb; the glow round it."""
    for rr, paint in ((w * 1.5, glow), (w * 1.0, "art_white")):
        d.oval(paint, x, y, rr, rr, ink=0.0)
    d.shape(wall, [(x - w * 0.34, y - w * 1.2), (x - w * 0.34, y + w * 0.2), (x + w * 0.34, y + w * 0.2), (x + w * 0.34, y - w * 1.2)])
    hgt = w * 1.3
    d.shape(frame, [(x - w * 0.62, y + hgt * 0.5), (x, y + hgt * 0.5 + w * 0.42), (x + w * 0.62, y + hgt * 0.5)])
    d.oval(frame, x, y + hgt * 0.5 + w * 0.5, w * 0.1, w * 0.1, ink=0.0)
    d.shape(glass, [(x - w * 0.5, y - hgt * 0.5), (x - w * 0.5, y + hgt * 0.5), (x + w * 0.5, y + hgt * 0.5), (x + w * 0.5, y - hgt * 0.5)], ink=0.0)
    d.oval("art_white", x, y, w * 0.18, w * 0.24, ink=0.0)
    for u in (-0.5, 0.0, 0.5):
        d.line(frame, [(x + w * u, y - hgt * 0.5), (x + w * u, y + hgt * 0.5)], w * 0.06, taper=False)
    d.line(frame, [(x - w * 0.5, y + hgt * 0.5), (x + w * 0.5, y + hgt * 0.5)], w * 0.07, taper=False)
    d.shape(frame, [(x - w * 0.42, y - hgt * 0.5), (x - w * 0.3, y - hgt * 0.5 - w * 0.2), (x + w * 0.3, y - hgt * 0.5 - w * 0.2),
                    (x + w * 0.42, y - hgt * 0.5)])


@painting(112)
def p112(d, a):   # a lit porch lamp and three big moths in flying helmets round it
    porch_lamp(d, 30.0, 32.0, 14.0)
    moth(d, 8.0, 52.0, 22.0, turn=14.0)
    moth(d, 54.0, 48.0, 24.0, turn=-12.0)
    moth(d, 52.0, 14.0, 18.0, turn=-30.0)
    place_name(d, a, lines=["Porch", "Light"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 113: Patched Britches -------------------------------------------------------------------------------------------------------
def patch(d, x, y, w, turn=0.0, cloth="art_red", check=None, stitch="art_white"):
    """A square patch sewn on, its middle at (x, y), `w` across: the cloth (with a check of `check` across it), its
    running stitches round it."""
    G = _T(x, y, turn)
    h = w / 2.0
    d.shape(cloth, [G(-h, -h), G(-h, h), G(h, h), G(h, -h)], ink=d.ink * 0.6)
    if check:
        for t in (-0.34, 0.34):
            d.line(check, [G(t * w, -h), G(t * w, h)], w * 0.1, taper=False)
            d.line(check, [G(-h, t * w), G(h, t * w)], w * 0.1, taper=False)
    for side in range(4):
        for i in range(4):
            t = -0.36 + 0.72 * (i + 0.25) / 4
            a0, a1 = t, t + 0.1
            pts = {0: ((a0, -0.42), (a1, -0.42)), 1: ((0.42, a0), (0.42, a1)), 2: ((a0, 0.42), (a1, 0.42)), 3: ((-0.42, a0), (-0.42, a1))}[side]
            d.line(stitch, [G(pts[0][0] * w, pts[0][1] * w), G(pts[1][0] * w, pts[1][1] * w)], w * 0.05, taper=False)


@painting(113)
def p113(d, a):   # a pair of overalls patched in six places, walking along by themselves
    cl, dark = "art_blue", "art_navy"
    x, y = 30.0, 4.0
    # (the far leg, back, in its boot; then the near leg forward)
    for (hip, knee, foot, paint) in (((-3.0, 30.0), (-9.0, 16.0), (-15.0, 4.0), dark), ((3.0, 30.0), (8.0, 16.0), (12.0, 4.0), cl)):
        hx, hy = x + hip[0], y + hip[1]
        kx, ky = x + knee[0], y + knee[1]
        fx, fy = x + foot[0], y + foot[1]
        d.limb(paint, [(hx, hy), (kx, ky), (fx, fy + 2.4)], [5.4, 4.6, 4.4], ink=d.ink)
        d.fill("art_brown", [(fx - 4.0, fy - 2.2), (fx - 3.8, fy + 2.6), (fx + 2.0, fy + 2.6), (fx + 6.0, fy + 0.4), (fx + 6.0, fy - 2.2)], ink=d.ink, steps=2)
        d.line("art_white", [(fx - 4.2, fy + 3.2), (fx + 3.8, fy + 3.6)], 1.6, taper=False)                 # (the turned-up cuff)
    # (the seat and the bib, empty: its straps arched up over nobody, their buckles)
    d.fill(cl, [(x - 8.0, y + 26.0), (x - 8.6, y + 34.0), (x - 6.0, y + 36.0), (x + 6.0, y + 36.0), (x + 8.6, y + 34.0), (x + 8.0, y + 26.0), (x, y + 24.0)],
           ink=d.ink, steps=3)
    d.fill(cl, [(x - 6.0, y + 35.0), (x - 6.2, y + 46.0), (x + 6.2, y + 46.0), (x + 6.0, y + 35.0)], ink=d.ink, steps=1)
    d.shape(dark, [(x - 3.4, y + 40.0), (x - 3.4, y + 44.0), (x + 3.4, y + 44.0), (x + 3.4, y + 40.0)], ink=d.ink * 0.6)
    for su in (-1, 1):
        d.line(cl, [(x + su * 5.0, y + 45.6), (x + su * 6.4, y + 52.0), (x + su * 4.6, y + 56.0), (x + su * 1.0, y + 56.6)], 1.8)
        d.oval("art_lsteel", x + su * 5.0, y + 45.4, 1.2, 1.0, ink=d.ink * 0.6)
    # (six patches)
    for (u, v, w, turn, paint, chk) in ((-4.0, 30.0, 4.6, 8.0, "art_red", "art_white"), (2.4, 42.0, 3.6, -10.0, "art_yellow", None),
                                        (6.6, 18.0, 4.4, -14.0, "art_green", "art_dgreen"), (-8.6, 18.0, 4.0, 12.0, "art_tan", None),
                                        (10.6, 9.0, 3.8, 6.0, "art_red", None), (4.0, 31.0, 3.4, 20.0, "art_lbrown", "art_brown")):
        patch(d, x + u, y + v, w, turn, cloth=paint, check=chk)
    speed(d, x - 16.0, y + 30.0, 8.0, n=3, gap=3.6, paint="art_white", w=0.9)
    place_name(d, a, lines=["Patched", "Britches"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 114: Hawkeye Hank ----------------------------------------------------------------------------------------------------------
def bombsight(d, x, y, w, turn=0.0, body="art_grey", dark="art_dark", glass="art_sky"):
    """A bombsight, side on (a made-up one, of no one maker): a rounded box, its eyepiece tube up at the back with its
    rubber cup, two knobs on its side, its lens window under the front. (x, y) the box's middle, `w` its length."""
    G = _T(x, y, turn)
    h = w * 0.42
    d.fill(body, [G(-w / 2, -h / 2), G(-w / 2, h / 2), G(w * 0.4, h / 2), G(w / 2, h * 0.2), G(w / 2, -h / 2)], ink=d.ink, steps=2)
    d.shape(body, [G(-w * 0.36, h / 2), G(-w * 0.36, h * 1.2), G(-w * 0.14, h * 1.2), G(-w * 0.14, h / 2)], ink=d.ink)
    d.shape(dark, [G(-w * 0.42, h * 1.2), G(-w * 0.42, h * 1.46), G(-w * 0.08, h * 1.46), G(-w * 0.08, h * 1.2)], ink=d.ink * 0.7)
    for u in (-0.1, 0.22):
        d.oval(dark, *G(u * w, 0.0), w * 0.08, w * 0.08, ink=d.ink * 0.6)
        d.oval("art_lsteel", *G(u * w, 0.0), w * 0.035, w * 0.035, ink=0.0)
    d.shape(glass, [G(w * 0.1, -h / 2), G(w * 0.16, -h * 0.72), G(w * 0.42, -h * 0.72), G(w * 0.46, -h / 2)], ink=d.ink * 0.6)


def hawk(d, x, y, size, back="art_brown", chest="art_cream", bar="art_lbrown", beak="art_dark"):
    """A hawk perched, facing right, side on: his fierce head with its hooked beak and hard eye under a low brow, his
    barred cream chest, his folded wing (lifted a little, to tuck a thing under it), his long tail down; feet at (x, y),
    `size` his height. Returns the frame and where under his wing a thing tucks."""
    k = size / 40.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    d.fill(back, Ps([(-4.0, 10.0), (-12.0, -6.0), (-7.0, -7.0), (0.0, 6.0)]), ink=ink, steps=2)
    d.fill(back, Ps([(-6.0, 12.0), (-6.0, 24.0), (0.0, 32.0), (6.0, 32.0), (9.0, 24.0), (8.0, 10.0), (3.0, 2.0), (-3.0, 4.0)]), ink=ink, steps=4)
    d.fill(chest, Ps([(8.8, 24.0), (8.0, 12.0), (3.6, 3.0), (0.0, 4.0), (2.0, 14.0), (4.0, 26.0)]), ink=0.0, steps=4)
    for v in (8.0, 12.0, 16.0, 20.0):
        d.line(bar, Ps([(4.0 + v * 0.06, v), (7.6, v + 0.6)]), 0.5 * k)
    d.oval(back, *P(5.0, 34.0), 6.4 * k, 6.0 * k, ink=ink)
    d.fill(chest, Ps([(7.0, 29.4), (10.0, 31.4), (10.0, 28.6), (8.4, 28.0)]), ink=0.0, steps=2)
    d.fill(beak, Ps([(10.0, 36.0), (14.4, 35.4), (15.2, 32.6), (13.4, 33.0), (10.4, 32.6)]), ink=ink * 0.8, steps=2)
    d.fill("art_dark", Ps([(14.6, 33.6), (15.2, 32.6), (14.0, 31.6)]), ink=0.0, steps=1)
    d.oval("art_yellow", *P(8.0, 35.4), 1.4 * k, 1.3 * k, ink=ink * 0.6)
    d.oval("art_black", *P(8.3, 35.4), 0.7 * k, 0.7 * k, ink=0.0)
    d.line("art_dark", Ps([(5.6, 37.2), (8.0, 37.0), (10.6, 36.2)]), 0.9 * k)                                        # (his brow, low)
    for du in (0.0, 3.0):
        d.line(beak, Ps([(du, 3.0), (du + 0.6, 0.0)]), 0.7 * k, taper=False)
    tuck = P(-1.0, 16.0)
    return P, tuck


@painting(114)
def p114(d, a):   # a hawk with a bombsight tucked under one wing
    d.limb("art_brown", [(-4.0, 3.0), (30.0, 5.0), (56.0, 3.0)], [2.0, 2.2, 1.6], ink=d.ink)                     # (the branch)
    P, tuck = hawk(d, 24.0, 5.0, 60.0)
    bombsight(d, *P(8.0, 13.0), 24.0, turn=-8.0, body="art_lsteel", dark="art_dark")
    k = 60.0 / 40.0
    d.fill("art_brown", [P(-6.0, 26.0), P(-10.0, 14.0), P(-6.0, 8.0), P(2.0, 11.0), P(4.0, 18.0), P(2.0, 25.0)], ink=d.ink, steps=4)   # (his wing, over it)
    for v in (12.0, 16.0, 20.0):
        d.line("art_dark", [P(-7.0, v), P(1.0, v + 1.0)], 0.4 * k)
    place_name(d, a, lines=["Hawkeye", "Hank"], where="right", room=6.0, at=32.0, caps=21.5)


# ---- 115: Bullfrog Basso ----------------------------------------------------------------------------------------------------------
@painting(115)
def p115(d, a):   # a bullfrog in a bow tie singing bass on his lily pad
    x, y = 30.0, 8.0
    # (the water, the lily pad with its notch, one water lily)
    d.fill("art_blue", [(-4.0, 2.0), (-2.0, 10.0), (62.0, 10.0), (64.0, 2.0), (30.0, -2.0)], steps=4)
    d.fill("art_lgreen", [(x, y - 1.0), (x - 30.0, y + 0.0), (x - 34.0, y + 4.0), (x - 20.0, y + 9.0), (x + 20.0, y + 9.0), (x + 34.0, y + 4.0),
                          (x + 30.0, y + 0.0), (x + 5.0, y - 1.0), (x + 3.0, y + 4.0)], ink=d.ink, steps=4)
    for (fu, fv) in ((x + 27.0, y + 5.0),):
        for ang in (60, 90, 120, 75, 105):
            a_ = math.radians(ang)
            d.fill("art_pink", [(fu, fv), (fu + 4.0 * math.cos(a_ - 0.25), fv + 4.0 * math.sin(a_ - 0.25)), (fu + 5.4 * math.cos(a_), fv + 5.4 * math.sin(a_)),
                                (fu + 4.0 * math.cos(a_ + 0.25), fv + 4.0 * math.sin(a_ + 0.25))], ink=d.ink * 0.6, steps=2)
        d.oval("art_yellow", fu, fv + 1.0, 1.0, 0.8, ink=0.0)
    # (the frog: squatting, his big hind legs folded, his round body, his throat blown up white, his mouth open on a
    #  low note, eyes shut; his forelegs spread like a singer's arms; a red bow tie)
    G = lambda u, v: (x + u, y + 6.0 + v)               # noqa: E731
    for su in (-1, 1):
        d.fill("art_green", [G(su * 6.0, 2.0), G(su * 16.0, 4.0), G(su * 18.0, 10.0), G(su * 12.0, 14.0), G(su * 6.0, 10.0)], ink=d.ink, steps=3)
        d.fill("art_green", [G(su * 14.0, 2.0), G(su * 22.0, 1.4), G(su * 22.6, 3.0), G(su * 16.0, 4.0)], ink=d.ink, steps=2)
    d.fill("art_green", [G(-13.0, 8.0), G(-12.0, 20.0), G(-6.0, 28.0), G(6.0, 28.0), G(12.0, 20.0), G(13.0, 8.0), G(0.0, 4.0)], ink=d.ink, steps=4)
    d.oval("art_cream", *G(0.0, 16.0), 9.0, 8.4, ink=d.ink * 0.8)
    d.fill("art_green", [G(-11.6, 24.0), G(-12.0, 30.0), G(-6.0, 34.0), G(6.0, 34.0), G(12.0, 30.0), G(11.6, 24.0)], ink=d.ink, steps=4)
    for su in (-1, 1):
        d.oval("art_green", *G(su * 7.4, 34.0), 4.2, 3.8, ink=d.ink)
        d.line("art_ink", [G(su * 7.4 - 2.4, 34.2), G(su * 7.4, 33.0), G(su * 7.4 + 2.4, 34.2)], 0.6, taper=False)
    d.fill("art_dark", [G(-7.0, 27.0), G(-4.0, 23.6), G(4.0, 23.6), G(7.0, 27.0), G(0.0, 28.0)], ink=d.ink * 0.8, steps=3)
    d.oval("art_red", *G(0.0, 24.6), 2.6, 1.2, ink=0.0)
    for su in (-1, 1):                                    # (his bow tie)
        d.shape("art_red", [G(0.0, 22.2), G(su * 4.6, 24.0), G(su * 4.6, 20.4)], ink=d.ink * 0.7)
    d.oval("art_red", *G(0.0, 22.2), 1.2, 1.2, ink=d.ink * 0.7)
    for su in (-1, 1):                                    # (his forelegs, spread)
        d.limb("art_green", [G(su * 10.0, 20.0), G(su * 15.0, 18.0), G(su * 18.0, 22.0)], [2.2, 1.8, 1.4], ink=d.ink)
        for ang in (40, 70, 100):
            a_ = math.radians(ang if su > 0 else 180 - ang)
            px, py = G(su * 18.0, 22.0)
            d.line("art_green", [(px, py), (px + 2.4 * math.cos(a_), py + 2.4 * math.sin(a_))], 0.8)
    note(d, x + 18.0, y + 40.0, 9.0)
    note(d, x + 26.0, y + 48.0, 7.0, turn=-8)
    place_name(d, a, lines=["Bullfrog", "Basso"], where="right", room=6.0, at=30.0, caps=21.5)
