"""Batch 4, numbers 162 to 168 (painter b4_d): a circus seal clapping with a bomb balanced on its nose, a farm woman in
overalls holding up one great ear of corn, a black cat arched and hissing, a coiled rattlesnake reared to strike, a
ring doughnut rolling on its edge with a bite out of it, a snowman tipping his hat as he melts and a wild hog charging in a leather
football helmet. Flat shapes with noseart's Drawing API and the figure kit; each name set clear of its picture by
place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T, _lady  # noqa: E402


# ---- 162: Seal of Approval -----------------------------------------------------------------------------------------------------------
@painting(162)
def p162(d, a):   # a circus seal up on its drum, flippers clapping, a bomb balanced level on the tip of its nose
    ink = d.ink
    cx = 22.0
    # (the circus drum: red, a band of stars, its rim)
    d.fill("art_red", [(cx - 12.0, 0.0), (cx - 12.0, 12.0), (cx + 12.0, 12.0), (cx + 12.0, 0.0)], ink=ink, steps=1)
    d.oval("art_white", cx, 12.0, 12.0, 2.6, ink=ink)
    d.shape("art_gold", [(cx - 12.0, 1.0), (cx - 12.0, 3.0), (cx + 12.0, 3.0), (cx + 12.0, 1.0)], ink=0.0)
    for u in (-6.0, 0.0, 6.0):
        star(d, cx + u, 7.4, 2.2, "art_white", ink=0.0)
    # (the seal: up on its tail, body curving up to a head held high, nose to the sky)
    K = lambda u, v: (cx + u, 13.0 + v)                  # noqa: E731
    body = [K(-8.0, 0.0), K(-10.0, 6.0), K(-9.0, 16.0), K(-6.0, 26.0), K(-3.0, 34.0), K(1.0, 40.0), K(5.0, 42.0), K(8.0, 41.0), K(9.0, 38.0),
            K(5.0, 34.0), K(4.0, 26.0), K(6.0, 16.0), K(7.0, 6.0), K(5.0, 0.0)]
    d.fill("art_grey", body, ink=ink, steps=4)
    d.fill("art_lgrey", [K(-2.0, 2.0), K(-1.0, 12.0), K(2.0, 24.0), K(4.6, 32.0), K(5.6, 24.0), K(5.4, 12.0), K(4.0, 2.0)], ink=0.0, steps=3)
    d.fill("art_grey", [K(-8.0, 1.0), K(-15.0, -1.0), K(-14.0, 2.4), K(-8.0, 4.0)], ink=ink * 0.8, steps=2)
    # (its head: nose up, whiskers, a shut happy eye)
    d.oval("art_dark", *K(9.6, 40.6), 1.0, 0.9, ink=0.0)
    d.line("art_ink", [K(3.4, 38.6), K(4.6, 39.4), K(5.8, 38.8)], 0.4)
    for dv in (-0.8, 0.4):
        d.line("art_white", [K(8.0, 39.0 + dv), K(11.6, 38.0 + dv * 2.0)], 0.25)
    # (its flippers clapping in front of its chest, and the clap's strokes)
    for dv in (0.0, 1.8):
        d.fill("art_dark", [K(4.0, 22.0 + dv), K(9.0, 24.0 + dv * 1.6), K(13.0, 26.0 + dv * 2.0), K(12.0, 23.4 + dv), K(8.0, 21.6)], ink=ink * 0.8, steps=2)
    for (u, v, t) in ((15.0, 30.0, 30.0), (16.6, 25.4, 0.0), (15.0, 21.0, -30.0)):
        G = _T(*K(u, v), t)
        d.line("art_white", [G(0.0, 0.0), G(3.0, 0.0)], 0.6)
    # (the bomb, balanced level on the tip of its nose)
    bomb(d, *K(18.0, 45.4), 22.0, turn=0.0, paint="art_dark", band="art_yellow", ink=ink)
    place_name(d, a, lines=["Seal of", "Approval"], where="right", room=6.0, at=32.0, caps=21.5)


# ---- 163: Cornhusker Kate ------------------------------------------------------------------------------------------------------------
@painting(163)
def p163(d, a):   # a farm woman in bib overalls sitting on an upturned bushel basket, husking one great ear of corn
    ink = d.ink
    x, y, h = 24.0, 0.0, 84.0
    kw = dict(pose="sit", arms=("offer", "_b4d_husk"),
              wear={"top": "art_white", "sleeves": "rolled", "collar": "art_white", "trousers": "art_blue", "bib": "art_blue", "shoes": "art_brown"},
              hair="rolls", hair_paint="art_blond", hat="kerchief")
    extra = {"_b4d_husk": ((-0.1, -1.1), (-1.5, -0.7))}
    F0 = _lady(Drawing(1), x, y, h, 1, extra_arms=extra, **kw)
    floor = F0.P(0.0, 1.9)[1]
    seat = F0.P(0.0, 3.5)[1]
    # (the bushel basket upside down under her: slats, hoops)
    b0, b1 = F0.P(-1.0, 0.0)[0], F0.P(0.9, 0.0)[0]
    d.fill("art_tan", [(b0 - 1.0, floor), (b0 + 1.0, seat), (b1 - 1.0, seat), (b1 + 1.0, floor)], ink=ink, steps=1)
    for t in (0.2, 0.4, 0.6, 0.8):
        u0 = b0 - 1.0 + (b1 - b0 + 2.0) * t
        u1 = b0 + 1.0 + (b1 - b0 - 2.0) * t
        d.line("art_brown", [(u0, floor + 0.4), (u1, seat - 0.4)], 0.4, taper=False)
    for f in (0.25, 0.75):
        v = floor + (seat - floor) * f
        d.line("art_brown", [(b0 - 1.0 + 2.0 * f, v), (b1 + 1.0 - 2.0 * f, v)], 1.0, taper=False)
    # (husks on the ground by her boots)
    for (u, r) in ((-12.0, 16.0), (16.0, -20.0), (24.0, 10.0)):
        G = _T(x + u, floor + 0.6, r)
        d.fill("art_lgreen", [G(-5.0, 0.0), G(-1.0, 1.6), G(5.0, 0.4), G(0.0, -0.6)], ink=ink * 0.6, steps=2)
    F = _lady(d, x, y, h, 1, extra_arms=extra, **kw)
    (hx, hy), _ = F.hand["fore"]
    (gx, gy), _ = F.hand["aft"]
    corn(d, hx + 22.0, hy + 3.0, 30.0, turn=8.0)
    for (qx, qy) in ((hx, hy), (gx, gy)):
        d.oval("art_skin", qx, qy, F.r(0.16), F.r(0.14), ink=ink * 0.7)
    place_name(d, a, lines=["Cornhusker", "Kate"], where="below", room=5.0, caps=21.5)


# ---- 164: Spitting Mad ---------------------------------------------------------------------------------------------------------------
def bobcat(d, x, y, size, coat="art_tan", spot="art_brown", white="art_white"):
    """A bobcat side-on, facing right, back arched high, fur on end, ears flat, mouth open spitting: its feet at
    (x, y), `size` the height of its arch. Short black-tipped bobtail, spotted coat, white chin."""
    k = size / 30.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    # (far legs behind)
    for (u0, u1) in ((-10.0, -12.0), (9.0, 11.0)):
        d.limb(coat, Ps([(u0, 16.0), (u1, 8.0), (u1 + 0.6, 1.0)]), [2.6 * k, 2.0 * k, 1.6 * k], ink=ink * 0.8)
    # (the arched body; its fur on end along the arch; its bobtail)
    body = [(-14.0, 14.0), (-14.0, 20.0), (-10.0, 26.0), (-3.0, 29.6), (4.0, 28.6), (10.0, 24.0), (13.0, 19.0), (12.0, 14.0), (6.0, 13.0), (0.0, 15.0),
            (-6.0, 13.6)]
    d.fill(coat, Ps(body), ink=ink, steps=4)
    for i in range(17):
        t = i / 16
        u = -12.0 + 24.0 * t
        v = 26.0 + 3.8 * math.sin(math.pi * t) - (0.0 if t < 0.85 else 4.0 * (t - 0.85) / 0.15)
        d.fill(coat, Ps([(u - 0.7, v - 0.8), (u + 0.2, v + 1.5), (u + 0.8, v - 0.6)]), ink=ink * 0.5, steps=1)
    d.fill(coat, Ps([(-14.0, 20.0), (-18.0, 23.0), (-17.0, 26.0), (-13.0, 23.0)]), ink=ink, steps=2)
    d.oval("art_black", *P(-17.4, 24.6), 1.2 * k, 1.4 * k, ink=0.0)
    for (u, v) in ((-8.0, 22.0), (-3.0, 24.0), (3.0, 23.0), (8.0, 20.0), (-6.0, 17.0), (1.0, 18.0), (6.0, 16.0)):
        d.oval(spot, *P(u, v), 0.9 * k, 0.7 * k, ink=0.0)
    # (near legs, stiff)
    for (u0, u1) in ((-8.0, -9.0), (10.0, 12.0)):
        d.limb(coat, Ps([(u0, 16.0), (u1, 8.0), (u1 + 0.4, 1.0)]), [3.0 * k, 2.2 * k, 1.8 * k], ink=ink)
        d.oval(coat, *P(u1 + 1.0, 0.8), 2.2 * k, 1.0 * k, ink=ink * 0.8)
    # (its head: big and round, low and forward; ears laid flat back; slit eyes; mouth wide on its fangs; spit flying)
    hx, hy, hr = 17.0, 17.0, 6.4
    for dv in (2.6, 4.4):
        d.fill(coat, Ps([(hx - 2.0, hy + dv), (hx - 9.0, hy + dv + 1.6), (hx - 3.0, hy + dv - 2.4)]), ink=ink, steps=1)
    d.oval(coat, *P(hx, hy), hr * k, hr * 0.92 * k, ink=ink)
    for su in (-1.0, 1.0):
        d.fill(coat, Ps([(hx - 4.0, hy - 3.0 + su * 1.0), (hx - 8.4, hy - 4.0 + su * 2.4), (hx - 5.0, hy - 6.0 + su * 1.0)]), ink=ink * 0.7, steps=1)
    d.fill(white, Ps([(hx + 0.6, hy - 1.0), (hx + 7.4, hy - 0.4), (hx + 7.0, hy - 6.4), (hx + 1.6, hy - 5.6)]), ink=0.0, steps=2)
    d.fill("art_red", Ps([(hx + 2.0, hy - 2.0), (hx + 7.6, hy - 1.2), (hx + 7.0, hy - 5.6), (hx + 2.6, hy - 4.6)]), ink=ink * 0.8, steps=2)
    for (u, v, dv) in ((hx + 6.6, hy - 1.4, -2.0), (hx + 3.4, hy - 2.0, -1.6), (hx + 6.4, hy - 5.4, 1.8), (hx + 3.6, hy - 4.6, 1.4)):
        d.fill(white, Ps([(u - 0.5, v), (u, v + dv), (u + 0.5, v)]), ink=0.0, steps=1)
    d.oval("art_pink", *P(hx + 6.6, hy + 0.6), 1.0 * k, 0.7 * k, ink=ink * 0.5)
    d.fill("art_yellow", Ps([(hx + 0.6, hy + 2.2), (hx + 3.6, hy + 2.8), (hx + 3.4, hy + 1.6)]), ink=ink * 0.5, steps=1)
    d.line("art_ink", Ps([(hx + 0.2, hy + 3.4), (hx + 4.0, hy + 2.0)]), 0.6 * k)
    for dv in (-1.0, 0.4):
        d.line(white, Ps([(hx + 5.0, hy - 0.6 + dv), (hx + 10.0, hy + dv * 2.2)]), 0.25 * k)
    for (u, v) in ((hx + 10.0, hy - 3.0), (hx + 12.0, hy - 1.4), (hx + 11.4, hy - 5.6), (hx + 14.0, hy - 3.4)):
        d.oval(white, *P(u, v), 0.6 * k, 0.5 * k, ink=0.0)


def hissing_cat(d, x, y, k=1.0, coat="art_black", eye="art_yellow", mouth="art_red", white="art_white"):
    """The idiom's cat, side-on and facing right, its feet on (x, y): back arched up high like a bridge, stiff straight
    legs on tiptoe, its fur standing on end all along the arch, the tail straight up and bushed out, ears flat back,
    eyes narrowed to slits, mouth wide open on its fangs, hissing and spitting. `k` its scale (about 46 tall at 1)."""
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    # (the tail, straight up and bushed out: fur standing off both its sides)
    spine = [(-12.6, 22.0), (-15.0, 30.0), (-15.6, 38.0), (-14.6, 46.0)]
    for i in range(9):
        t = (i + 0.5) / 9
        u = -13.0 - 2.6 * math.sin(math.pi * t * 0.9)
        v = 25.0 + 20.0 * t
        for su in (-1.0, 1.0):
            d.fill(coat, Ps([(u + su * 1.8, v - 1.2), (u + su * 4.6, v + 0.6 + 0.4 * su), (u + su * 1.8, v + 1.2)]), ink=0.0, steps=1)
    d.limb(coat, Ps(spine), [2.6 * k, 3.4 * k, 3.4 * k, 2.2 * k], ink=0.0)
    # (the legs: stiff and straight, up on its toes)
    for (u0, u1) in ((-12.4, -13.4), (-8.6, -9.0), (8.4, 8.8), (11.6, 12.8)):
        d.limb(coat, Ps([(u0, 16.0), ((u0 + u1) / 2, 8.0), (u1, 1.0)]), [2.2 * k, 1.5 * k, 1.2 * k], ink=0.0)
        d.oval(coat, *P(u1 + 0.6, 0.8), 1.8 * k, 0.9 * k, ink=0.0)
    # (the body arched up like a bridge, its belly drawn up under it; the fur on end all along the arch)
    arch = [(-15.0, 15.0), (-15.4, 21.0), (-12.6, 27.6), (-6.6, 32.0), (0.0, 33.4), (6.0, 31.6), (11.0, 26.6), (13.6, 20.6), (13.4, 15.0)]
    belly = [(9.6, 14.0), (5.0, 19.4), (0.0, 21.0), (-5.0, 19.4), (-10.0, 13.6)]
    d.fill(coat, Ps(arch + belly), ink=0.0, steps=3)
    for i in range(15):
        t = (i + 0.5) / 15
        ang = math.radians(168.0 - 150.0 * t)
        cu, cv = -0.8 + 14.4 * math.cos(ang), 18.6 + 14.4 * math.sin(ang)
        nu, nv = math.cos(ang), math.sin(ang)
        tu, tv = -nv, nu
        L = 3.2 if i % 2 else 2.4
        d.fill(coat, Ps([(cu - tu * 1.0 - nu * 0.6, cv - tv * 1.0 - nv * 0.6), (cu + nu * L + tu * 0.3, cv + nv * L + tv * 0.3),
                         (cu + tu * 1.0 - nu * 0.6, cv + tv * 1.0 - nv * 0.6)]), ink=0.0, steps=1)
    # (the head, low and forward: the ruff standing off its cheek, the ears laid flat back)
    hx, hy = 18.0, 15.6
    for i in range(5):
        ang = math.radians(150.0 + 22.0 * i)
        d.fill(coat, Ps([(hx + 5.4 * math.cos(ang - 0.25), hy + 5.4 * math.sin(ang - 0.25)), (hx + 9.0 * math.cos(ang), hy + 9.0 * math.sin(ang)),
                         (hx + 5.4 * math.cos(ang + 0.25), hy + 5.4 * math.sin(ang + 0.25))]), ink=0.0, steps=1)
    for (b0, b1, tip) in (((13.6, 19.6), (17.6, 21.8), (7.4, 21.6)), ((15.4, 20.8), (19.6, 22.0), (10.0, 23.8))):
        d.fill(coat, Ps([b0, tip, b1]), ink=0.0, steps=1)
    d.fill(mouth, Ps([(14.0, 20.8), (9.4, 22.6), (16.0, 21.6)]), ink=0.0, steps=1)
    d.oval(coat, *P(hx, hy), 6.8 * k, 6.2 * k, ink=0.0)
    d.fill(coat, Ps([(21.0, 18.4), (26.0, 16.6), (26.4, 15.0), (21.6, 14.6)]), ink=0.0, steps=2)
    # (its mouth wide open, hissing: the jaws, the fangs, the lower jaw dropped under it)
    d.fill(mouth, Ps([(19.4, 13.2), (26.0, 15.2), (25.6, 12.0), (24.4, 9.2)]), ink=0.0, steps=2)
    d.fill(coat, Ps([(17.0, 11.6), (24.6, 8.6), (24.0, 7.4), (18.0, 8.6), (15.6, 10.4)]), ink=0.0, steps=2)
    for (u, v, dv) in ((22.4, 14.4, -2.4), (25.0, 15.0, -2.6), (23.2, 9.2, 2.2)):
        d.fill(white, Ps([(u - 0.6, v), (u, v + dv), (u + 0.6, v)]), ink=0.0, steps=1)
    # (its eye narrowed to a slit, slanting down to its nose; its whiskers bristling; the spit flying)
    d.fill(eye, Ps([(17.6, 18.8), (20.0, 19.2), (22.4, 17.6), (20.2, 17.2), (18.0, 17.8)]), ink=0.0, steps=2)
    d.oval(coat, *P(20.2, 18.2), 0.32 * k, 0.9 * k, ink=0.0)
    for (v0, u1, v1) in ((15.8, 33.0, 19.0), (15.4, 33.6, 16.0), (15.0, 32.6, 12.8)):
        d.line(white, Ps([(24.6, v0), ((24.6 + u1) / 2, (v0 + v1) / 2 + 0.3), (u1, v1)]), 0.28 * k)
    for (u, v, r) in ((29.0, 11.0, 0.7), (31.2, 13.4, 0.55), (32.4, 9.4, 0.6), (34.6, 12.0, 0.45), (30.4, 7.6, 0.5)):
        d.oval(white, *P(u, v), r * k, r * 0.8 * k, ink=0.0)


def _rim(d, n0, paint, rim, w):
    """A thin `rim` round the outside of everything painted in `paint` since op n0 (a black cat would sink into the
    dark olive without it): each shape's outline brushed `w` wide underneath them all, its inner half hidden again."""
    n1 = len(d.ops)
    for op in d.ops[n0:n1]:
        if op[0] == "polys" and op[1] == paint:
            for poly in op[2]:
                d.line(rim, poly, w, steps=1, taper=False, closed=True)
    d.ops[n0:] = d.ops[n1:] + d.ops[n0:n1]


@painting(164)
def p164(d, a):   # the idiom's cat: back arched, fur on end, tail straight up and bushed, ears flat, hissing and spitting
    n0 = len(d.ops)
    hissing_cat(d, 20.0, 1.0, 1.2)
    _rim(d, n0, "art_black", "art_white", 1.3)
    place_name(d, a, lines=["Spitting", "Mad"], where="below", room=5.0, caps=21.5)


# ---- 165: Rattler's Reply ------------------------------------------------------------------------------------------------------------
@painting(165)
def p165(d, a):   # a rattlesnake coiled, its neck up in an S, mouth open on its fangs, its rattle raised behind
    ink = d.ink
    cx = 24.0
    # (the coils: three, each smaller, banded with dark diamonds)
    for i, (cy, rx, ry) in enumerate(((5.0, 20.0, 6.0), (11.0, 16.0, 5.0), (16.0, 12.0, 4.4))):
        d.oval("art_khaki", cx, cy, rx, ry, ink=ink)
        for t in range(-3, 4):
            u = cx + t * rx / 4.0
            v = cy - ry * math.sqrt(max(0.0, 1.0 - (t / 4.0) ** 2)) * 0.5
            d.fill("art_brown", [(u - 2.0, v), (u, v + 1.6), (u + 2.0, v), (u, v - 1.6)], ink=0.0, steps=1)
    # (the rattle, raised behind the coils)
    for j in range(5):
        d.oval("art_tan", cx - 16.0 - j * 0.4, 18.0 + j * 2.4, 1.8 - j * 0.15, 1.3, ink=ink * 0.8)
    d.line("art_khaki", [(cx - 12.0, 13.0), (cx - 15.6, 16.0)], 2.6)
    for (u, v) in ((cx - 21.0, 26.0), (cx - 21.6, 22.0)):
        d.line("art_white", [(u, v), (u - 2.0, v + 0.6)], 0.5)
    # (its neck rising in an S from the top coil to the head)
    neck = [(cx + 2.0, 18.0), (cx + 6.0, 24.0), (cx + 4.0, 32.0), (cx - 1.0, 38.0), (cx + 1.0, 44.0), (cx + 6.0, 46.0)]
    d.limb("art_khaki", neck, [3.0, 2.8, 2.6, 2.4, 2.4, 2.4], ink=ink)
    d.line("art_cream", [(cx + 4.6, 23.0), (cx + 2.6, 32.0), (cx - 2.4, 38.0), (cx - 0.6, 43.0)], 0.9)
    for (u, v) in ((cx + 5.6, 27.0), (cx + 1.6, 35.4), (cx - 0.6, 41.0)):
        d.fill("art_brown", [(u - 1.4, v), (u, v + 1.2), (u + 1.4, v), (u, v - 1.2)], ink=0.0, steps=1)
    # (its head: a wide flat wedge, mouth wide, fangs, forked tongue, a slit eye)
    G = _T(cx + 7.0, 46.0, -10.0)
    d.fill("art_khaki", [G(-2.0, -2.6), G(-2.6, 2.4), G(3.0, 4.0), G(8.0, 3.2), G(9.0, 1.4), G(4.0, 0.6)], ink=ink, steps=2)
    d.fill("art_khaki", [G(-2.0, -2.6), G(4.0, -0.6), G(8.0, -3.6), G(3.0, -4.6)], ink=ink, steps=2)
    d.fill("art_red", [G(3.0, 0.4), G(8.6, 1.2), G(7.6, -3.2)], ink=0.0, steps=1)
    for (u, v, dv) in ((7.6, 1.4, -1.6), (6.6, -3.0, 1.4)):
        d.fill("art_white", [G(u - 0.3, v), G(u, v + dv), G(u + 0.3, v)], ink=0.0, steps=1)
    d.line("art_red", [G(5.0, -0.8), G(10.0, -0.6), G(11.6, 0.4)], 0.4)
    d.line("art_red", [G(10.0, -0.6), G(11.6, -1.6)], 0.4)
    d.oval("art_yellow", *G(2.6, 2.4), 0.9, 0.7, ink=0.0)
    d.line("art_ink", [G(2.6, 1.8), G(2.6, 3.0)], 0.35, taper=False)
    place_name(d, a, lines=["Rattler's", "Reply"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 166: Doughnut Run ---------------------------------------------------------------------------------------------------------------
@painting(166)
def p166(d, a):   # a ring doughnut rolling along on its edge, a bite out of it, crumbs flying, speed lines behind
    ink = d.ink
    cx, cy, R, r0 = 34.0, 20.0, 19.0, 6.6
    k = R / 15.0

    def bite(th):                                      # (how deep the bite goes in at angle th: three tooth scallops)
        t = (th - 22.0) / 50.0
        if t <= 0.0 or t >= 1.0:
            return 0.0
        return k * max(0.0, 5.2 * math.sqrt(math.sin(math.pi * t)) - 1.3 * (1.0 - abs(math.sin(3.0 * math.pi * t))))

    def ring(rad, a0=-90.0, a1=270.0, st=2.0):
        out, th = [], a0
        while th <= a1 + 1e-6:
            rr = rad(th)
            out.append((cx + rr * math.cos(math.radians(th)), cy + rr * math.sin(math.radians(th))))
            th += st
        return out

    def annulus(r_out, r_in):                          # (one shape: out round its edge, in through a hair slit at the bottom, back round the hole)
        return ring(r_out) + list(reversed(ring(lambda th: r_in)))

    # (the road; the dust kicked up behind; the speed lines)
    d.line("art_ink", [(-4.0, cy - R), (58.0, cy - R)], 0.5, taper=False)
    for (u, v, rr, sd) in ((cx - 16.0, cy - R + 2.4, 2.6, 1), (cx - 21.6, cy - R + 3.4, 2.0, 2), (cx - 26.0, cy - R + 2.4, 1.4, 3)):
        puff(d, u, v, rr, "art_lgrey", seed=sd, ink=ink * 0.4)
    speed(d, cx - 21.0, cy + 2.0, 16.0, n=3, gap=6.0, paint="art_white", w=1.0)
    # (the doughnut: its golden cake with the bite out of it, the bitten edge showing the cream crumb inside)
    outer = lambda th: R - bite(th)                    # noqa: E731
    d.shape("art_orange", annulus(outer, r0), ink=0.0)
    d.shape("art_cream", ring(outer, 18.0, 76.0, 1.0) + list(reversed(ring(lambda th: outer(th) - (1.8 if bite(th) > 0.05 else 0.0), 18.0, 76.0, 1.0))), ink=0.0)
    # (its pink icing, wavy at the edge, the bite through it too; the sprinkles)
    ice = lambda th: min(R - 3.2 + 1.1 * math.sin(math.radians(th) * 9.0), outer(th) - 1.9)   # noqa: E731
    d.shape("art_pink", annulus(ice, r0 + 2.0), ink=0.0)
    d.line("art_white", ring(lambda th: R - 5.0, 120.0, 165.0, 5.0), 1.0)
    d.line("art_ink", ring(outer, -90.0, 270.0, 2.0), 0.5, taper=False)
    d.line("art_ink", ring(lambda th: r0, -90.0, 270.0, 4.0), 0.45, taper=False)
    d.line("art_yellow", ring(lambda th: R - 1.0, 200.0, 340.0, 3.0), 0.5)
    rng = d.rng
    cols = ("art_yellow", "art_white", "art_lblue", "art_yellow", "art_white", "art_lblue")
    for i in range(16):
        th = 360.0 * (i + rng.uniform(0.2, 0.8)) / 16
        if 18.0 < th < 78.0:
            continue
        rr = r0 + 3.2 + rng.uniform(0.0, 5.0)
        u, v = cx + rr * math.cos(math.radians(th)), cy + rr * math.sin(math.radians(th))
        G = _T(u, v, th + 90.0 + rng.uniform(-50.0, 50.0))
        d.line(cols[i % 6], [G(-1.1, 0.0), G(1.1, 0.0)], 0.85, taper=False)
    # (the crumbs flying off the bite; the roll's curl over its top)
    for (u, v, rr) in ((15.0, 13.0, 0.9), (18.4, 10.4, 0.7), (13.0, 17.0, 0.6), (19.6, 15.0, 0.5), (16.4, 19.0, 0.45)):
        d.oval("art_cream", cx + u * k, cy + v * k, rr * k, rr * 0.8 * k, ink=ink * 0.5)
    d.line("art_white", ring(lambda th: R + 3.0, 110.0, 160.0, 5.0), 0.6)
    d.line("art_white", ring(lambda th: R + 3.0, 200.0, 245.0, 5.0), 0.6)
    place_name(d, a, lines=["Doughnut", "Run"], where="right", room=6.0, at=22.0, caps=21.5)


# ---- 167: January Thaw ---------------------------------------------------------------------------------------------------------------
@painting(167)
def p167(d, a):   # a snowman tipping his top hat with a stick arm as he melts into a puddle, drips running off him
    ink = d.ink
    cx = 22.0
    d.fill("art_lblue", ellipse(cx + 2.0, 2.0, 20.0, 4.0, 0, 30), ink=ink * 0.8, steps=3)
    d.line("art_white", [(cx - 10.0, 3.0), (cx - 4.0, 4.0)], 0.5)
    # (his three balls, slumping, the lowest spread wide)
    d.fill("art_white", [(cx - 15.0, 3.0), (cx - 14.0, 10.0), (cx - 9.0, 16.0), (cx, 18.0), (cx + 9.0, 16.0), (cx + 14.0, 10.0), (cx + 16.0, 3.0)], ink=ink, steps=4)
    d.oval("art_white", cx - 1.0, 25.0, 9.0, 8.0, ink=ink)
    d.oval("art_white", cx - 2.0, 38.0, 6.6, 6.2, ink=ink)
    # (drips down him)
    for (u, v0, v1) in ((-6.0, 31.0, 26.0), (5.0, 22.0, 17.0), (-10.0, 14.0, 8.0), (10.0, 11.0, 6.0)):
        d.line("art_lblue", [(cx + u, v0), (cx + u, v1)], 0.9, taper=False)
        d.oval("art_lblue", cx + u, v1, 0.9, 1.1, ink=0.0)
    # (his face: coal eyes, a coal smile, a carrot nose drooping)
    for su in (-1.0, 1.0):
        d.oval("art_black", cx - 2.0 + su * 2.4, 39.6, 0.9, 0.9, ink=0.0)
    for t in range(5):
        u = -3.0 + t * 1.5
        d.oval("art_black", cx - 2.0 + u, 35.0 - 0.6 * math.cos((t - 2) * 0.7), 0.5, 0.5, ink=0.0)
    d.fill("art_orange", [(cx, 38.6), (cx + 6.0, 35.6), (cx, 37.0)], ink=ink * 0.7, steps=2)
    # (his scarf; his buttons; his other stick arm drooping)
    d.limb("art_red", [(cx - 8.0, 32.0), (cx - 2.0, 31.2), (cx + 4.4, 32.0)], [1.6, 1.6, 1.6], ink=ink * 0.8)
    d.fill("art_red", [(cx - 6.0, 31.0), (cx - 7.6, 24.0), (cx - 4.6, 24.4), (cx - 3.6, 30.6)], ink=ink * 0.8, steps=1)
    for v in (26.0, 21.0, 13.0, 8.0):
        d.oval("art_black", cx + 0.4, v, 0.8, 0.8, ink=0.0)
    d.line("art_brown", [(cx - 9.0, 25.0), (cx - 16.0, 20.0), (cx - 18.0, 16.0)], 0.9)
    d.line("art_brown", [(cx - 16.0, 20.0), (cx - 19.0, 21.4)], 0.6)
    # (his near arm up, tipping his top hat off his head)
    arm = [(cx + 7.0, 27.0), (cx + 13.0, 34.0), (cx + 10.0, 44.0)]
    d.line("art_brown", arm, 0.9)
    d.line("art_brown", [(cx + 12.6, 36.0), (cx + 16.0, 38.0)], 0.6)
    G = _T(cx + 6.0, 47.0, 22.0)
    d.fill("art_black", [G(-5.0, 0.0), G(-5.0, 1.0), G(5.0, 1.0), G(5.0, 0.0)], ink=ink * 0.8, steps=1)
    d.fill("art_black", [G(-3.4, 1.0), G(-3.2, 9.0), G(3.2, 9.0), G(3.4, 1.0)], ink=ink * 0.8, steps=1)
    d.shape("art_red", [G(-3.4, 1.2), G(-3.4, 2.6), G(3.4, 2.6), G(3.4, 1.2)], ink=0.0)
    # (the sun that's doing it, a small one up behind)
    d.oval("art_yellow", cx - 16.0, 46.0, 4.0, 4.0, ink=ink * 0.7)
    for t in range(8):
        a_ = math.radians(t * 45.0)
        d.line("art_yellow", [(cx - 16.0 + 5.4 * math.cos(a_), 46.0 + 5.4 * math.sin(a_)), (cx - 16.0 + 7.6 * math.cos(a_), 46.0 + 7.6 * math.sin(a_))], 0.6)
    place_name(d, a, lines=["January", "Thaw"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 168: Razorback Rush -------------------------------------------------------------------------------------------------------------
@painting(168)
def p168(d, a):   # a wild hog charging in a leather football helmet, bristles up, tusks out, dust flying behind
    ink = d.ink
    K = lambda u, v: (30.0 + u, 4.0 + v)                 # noqa: E731
    coat, dark = "art_brown", "art_dark"
    # (dust behind)
    for (u, v, r) in ((-30.0, 2.0, 4.0), (-36.0, 5.0, 3.0), (-27.0, 7.0, 2.6), (-40.0, 1.0, 2.4)):
        d.oval("art_tan", *K(u, v), r * 1.4, r, ink=ink * 0.5)
    # (far legs: one reaching forward, one kicking back)
    d.limb(coat, [K(14.0, 10.0), K(20.0, 6.0), K(24.0, 4.0)], [3.0, 2.0, 1.5], ink=ink * 0.8)
    d.limb(coat, [K(-14.0, 10.0), K(-20.0, 6.0), K(-25.0, 8.0)], [3.4, 2.2, 1.5], ink=ink * 0.8)
    # (the body: long, a high humped shoulder, the razor ridge of bristles down the spine)
    body = [K(-18.0, 10.0), K(-20.0, 16.0), K(-16.0, 22.0), K(-6.0, 25.0), K(6.0, 28.0), K(14.0, 27.0), K(20.0, 22.0), K(22.0, 15.0), K(18.0, 9.0),
            K(6.0, 7.0), K(-8.0, 7.6)]
    d.fill(coat, body, ink=ink, steps=4)
    for i in range(14):
        t = i / 13
        u = -16.0 + 32.0 * t
        v = 23.0 + 5.0 * math.sin(math.pi * (0.2 + 0.8 * t)) - (2.0 if t < 0.1 else 0.0)
        d.fill(dark, [K(u - 1.0, v), K(u + 0.6, v + 3.6), K(u + 1.4, v)], ink=0.0, steps=1)
    d.fill(coat, [K(-18.0, 18.0), K(-22.0, 21.0), K(-23.0, 18.4), K(-21.0, 20.0)], ink=ink * 0.8, steps=1)
    # (near legs)
    d.limb(coat, [K(12.0, 10.0), K(17.0, 5.0), K(22.0, 0.6)], [3.4, 2.2, 1.7], ink=ink)
    d.limb(coat, [K(-12.0, 10.0), K(-17.0, 4.0), K(-21.0, 1.2)], [3.6, 2.4, 1.7], ink=ink)
    for (u, v) in ((22.6, 0.2), (-21.6, 0.6)):
        d.oval("art_black", *K(u, v), 1.6, 0.9, ink=0.0)
    # (the head: long snout down and forward, tusks up, a mean little eye; the leather helmet strapped on)
    d.fill(coat, [K(16.0, 26.0), K(22.0, 25.0), K(28.0, 18.0), K(34.0, 12.0), K(35.0, 9.0), K(32.0, 8.0), K(26.0, 10.0), K(19.0, 13.0), K(16.0, 18.0)],
           ink=ink, steps=3)
    d.oval("art_pink", *K(34.6, 10.4), 1.4, 2.0, turn=30, ink=ink * 0.7)
    for v in (9.8, 11.0):
        d.oval("art_ink", *K(35.0, v), 0.3, 0.4, ink=0.0)
    d.fill("art_white", [K(30.0, 9.6), K(31.0, 14.0), K(31.4, 9.4)], ink=ink * 0.6, steps=1)
    d.line("art_ink", [K(26.0, 10.6), K(32.0, 8.6)], 0.4)
    d.oval("art_white", *K(24.0, 18.6), 1.0, 0.8, ink=0.0)
    d.oval("art_red", *K(24.4, 18.6), 0.5, 0.5, ink=0.0)
    d.line("art_black", [K(22.4, 20.2), K(25.6, 19.4)], 0.5)
    d.fill("art_tan", [K(14.6, 22.0), K(16.0, 28.0), K(20.0, 30.0), K(25.0, 28.6), K(27.0, 23.0), K(22.6, 21.2), K(18.0, 20.6)], ink=ink, steps=3)
    for (u0, v0, u1, v1) in ((17.0, 28.6, 25.6, 26.0), (16.0, 25.0, 26.6, 23.4)):
        d.line("art_brown", [K(u0, v0), K(u1, v1)], 0.5, taper=False)
    d.line("art_brown", [K(20.8, 30.0), K(21.0, 21.0)], 1.0, taper=False)
    d.line("art_tan", [K(17.0, 21.6), K(19.0, 15.0), K(23.6, 14.6)], 0.6)
    d.fill(coat, [K(14.0, 26.0), K(12.0, 30.4), K(16.6, 28.0)], ink=ink * 0.8, steps=1)
    # (speed strokes)
    for v in (14.0, 19.0, 24.0):
        d.line("art_white", [K(-24.0, v), K(-34.0, v)], 0.6)
    place_name(d, a, lines=["Razorback", "Rush"], where="below", room=5.0, caps=21.5)
