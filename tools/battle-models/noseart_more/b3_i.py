"""Batch 3, numbers 137 to 140 (painter b3_i): a girl in a pinafore dancing on a cloud, a farm woman in calico swinging
a scarecrow for her partner, a boy flying a kite made in a bomber's shape, and a woman in gingham with a sunflower for
a parasol. Flat shapes with noseart's Drawing API and the figure kit; each name set clear of its picture by place_name
(b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T, _lady, note  # noqa: E402


def girl_hair(d, F, paint="art_blond", ribbon="art_red"):
    """A girl's hair on a kid()'s head frame (PH): a fringe over her brow and two short plaits out at the sides, each
    tied with a bow; a bow on top."""
    ink = d.ink * 0.7
    R = F.rh
    for su in (-1.0, 1.0):
        d.limb(paint, F.PHs([(su * 0.42, 6.86), (su * 0.62, 6.62), (su * 0.74, 6.3)]), [R(0.12), R(0.1), R(0.08)], ink=ink)
        bx, by = F.PH(su * 0.72, 6.36)
        d.fill(ribbon, [(bx, by), (bx - R(0.14), by + R(0.1)), (bx - R(0.14), by - R(0.1))], ink=ink, steps=1)
        d.fill(ribbon, [(bx, by), (bx + R(0.14), by + R(0.1)), (bx + R(0.14), by - R(0.1))], ink=ink, steps=1)
    d.fill(paint, F.PHs([(-0.47, 6.96), (-0.42, 7.3), (0.0, 7.48), (0.42, 7.3), (0.48, 7.02), (0.32, 7.2), (0.1, 7.16), (-0.12, 7.22), (-0.32, 7.16)]),
           ink=ink, steps=3)
    tx, ty = F.PH(-0.1, 7.44)
    for s in (-1.0, 1.0):
        d.fill(ribbon, [(tx, ty), (tx + s * R(0.22), ty + R(0.16)), (tx + s * R(0.22), ty - R(0.1))], ink=ink, steps=1)
    d.oval(ribbon, tx, ty, R(0.06), R(0.06), ink=ink)


# ---- 137: Tuesday's Child -----------------------------------------------------------------------------------------------------------
@painting(137)
def p137(d, a):   # a little girl in a pinafore dancing on a cloud, one hand up, music about her
    cloud(d, 22.0, 8.0, 40.0, 16.0, paint="art_white", seed=7)
    x, y, h = 22.0, 12.0, 74.0
    F = kid(d, x, y, h, 1, pose="step", arms=("up_out", "wave"),
            wear={"shirt": "art_pink", "sleeves": "short", "shorts": "art_pink", "apron": "art_white", "apron_cut": "bib", "socks": "art_white",
                  "shoes": "art_black", "belt": None},
            hair="none", mouth="smile")
    ink = d.ink
    # (her skirt, swinging out as she turns, the pinafore's skirt over it, its frill)
    d.fill("art_pink", F.Ps([(-0.62, 4.2), (-1.56, 2.5), (-1.1, 2.2), (-0.2, 2.08), (0.7, 2.14), (1.68, 2.38), (0.7, 4.2)]), ink=ink, steps=3)
    d.fill("art_white", F.Ps([(-0.44, 4.2), (-1.04, 2.62), (-0.2, 2.42), (0.7, 2.46), (1.16, 2.6), (0.5, 4.2)]), ink=ink * 0.8, steps=3)
    for t in range(6):
        u = -1.36 + 2.8 * t / 5.0
        d.oval("art_white", *F.P(u, 2.24 + 0.06 * math.sin(t)), F.r(0.14), F.r(0.08), ink=ink * 0.5)
    girl_hair(d, F)
    # (the music she dances to)
    note(d, x + 17.0, y + 34.0, 8.0, "art_white", turn=-10.0)
    note(d, x - 20.0, y + 24.0, 7.0, "art_white", turn=10.0)
    star(d, x + 20.0, y + 52.0, 2.4, "art_yellow", ink=ink * 0.5)
    place_name(d, a, lines=["Tuesday's", "Child"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 138: Do-Si-Do Dora -------------------------------------------------------------------------------------------------------------
@painting(138)
def p138(d, a):   # a farm woman in a calico dress swinging a scarecrow round for her square-dance partner
    ink = d.ink
    lx, sx, y, h = 16.0, 46.0, 3.0, 60.0
    F = lady(d, lx, y, h, 1, pose="stride", arms=("offer", "hip"),
             wear={"top": "art_lred", "dots": "art_white", "sleeves": "puff", "collar": "art_white", "hem": "calf", "flare": 0.56, "shoes": "art_brown"},
             hair="rolls", hair_paint="art_blond")
    (hx, hy), _ = F.hand["fore"]
    # (the scarecrow, facing her, his near hand in hers: straw hat, patched overalls, straw at his cuffs and collar)
    k = h / HEADS
    u, v = (sx - hx) / k + 0.1, (hy - y) / k
    S = man(d, sx, y, h * 0.98, -1, pose="step", arms=(to(u, v, "down"), "up_out"), hat="straw",
            wear={"shirt": "art_khaki", "sleeves": "long", "overalls": "art_blue", "shoes": "art_brown", "hat": "art_yellow", "band": "art_red"},
            hair="none", mouth="grin", eyes="open")
    straw = "art_yellow"
    for side in ("fore", "aft"):
        (wx, wy), _ = S.hand[side]
        for t in (-30.0, 0.0, 30.0):
            c, s = math.cos(math.radians(t + (90.0 if side == "aft" else 180.0))), math.sin(math.radians(t + (90.0 if side == "aft" else 180.0)))
            d.line(straw, [(wx, wy), (wx + 3.0 * c, wy + 3.0 * s)], 0.5)
    for (uu, vv) in ((-0.3, 0.3), (0.0, 0.2), (0.3, 0.3)):
        X, Y = S.PL(uu, vv)
        d.line(straw, [(X, Y + 1.6), (X - 0.6, Y - 1.0)], 0.5)
    for (pu, pv, pw) in ((0.1, 2.8, 0.3), (-0.2, 1.6, 0.26)):
        X, Y = S.PL(pu, pv)
        d.shape("art_tan", [(X - pw * k, Y - pw * k), (X - pw * k, Y + pw * k), (X + pw * k, Y + pw * k), (X + pw * k, Y - pw * k)])
        d.line("art_ink", [(X - pw * k, Y - pw * k), (X + pw * k, Y + pw * k)], 0.2, taper=False)
    F.grip("fore") if hasattr(F, "grip") else None
    S.grip("fore")
    place_name(d, a, lines=["Do-Si-Do", "Dora"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 139: Kite Season ---------------------------------------------------------------------------------------------------------------
def bomber_kite(d, x, y, span, cloth="art_orange", stick="art_brown", trim="art_white", turn=25.0):
    """A boy's kite made in a bomber's shape, seen from below against the sky, its nose up and turned `turn` degrees:
    a long body with a round nose, broad tapered wings with four engines out ahead of them, a tailplane with twin
    fins; cloth over its cross of sticks, its middle at (x, y), `span` wide. Returns where its string ties on."""
    k = span / 40.0
    G0 = _T(x, y, turn)
    P = lambda u, v: G0(u * k, v * k)                    # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    # (the tail of rag bows first, hanging below it)
    tail = Ps([(0.0, -15.0)]) + [(x + dx, y + dy) for (dx, dy) in ((-2.0 * k, -20.0 * k), (1.0 * k, -25.0 * k), (-1.0 * k, -30.0 * k), (2.0 * k, -35.0 * k))]
    d.line("art_white", tail, 0.25 * k, taper=False)
    for (tx, ty) in tail[1:]:
        d.fill(trim, [(tx, ty), (tx - 1.4 * k, ty + 0.8 * k), (tx - 1.4 * k, ty - 0.8 * k)], ink=ink * 0.5, steps=1)
        d.fill(trim, [(tx, ty), (tx + 1.4 * k, ty + 0.8 * k), (tx + 1.4 * k, ty - 0.8 * k)], ink=ink * 0.5, steps=1)
    # (the tailplane and its twin fins, the wings, the engines out ahead, the body over all)
    d.fill(cloth, Ps([(-8.0, -11.0), (-8.6, -14.4), (8.6, -14.4), (8.0, -11.0)]), ink=ink, steps=1)
    for su in (-8.4, 8.4):
        d.fill(cloth, Ps([(su - 0.8, -10.4), (su - 0.8, -15.6), (su + 0.8, -15.6), (su + 0.8, -10.4)]), ink=ink, steps=1)
    d.fill(cloth, Ps([(-20.0, -0.6), (-20.0, 2.4), (-3.0, 5.0), (3.0, 5.0), (20.0, 2.4), (20.0, -0.6), (3.0, -2.0), (-3.0, -2.0)]), ink=ink, steps=1)
    for eu in (-13.4, -7.4, 7.4, 13.4):
        d.oval(cloth, *P(eu, 4.8), 1.5 * k, 3.2 * k, turn=turn, ink=ink)
        d.line(trim, Ps([(eu - 2.0, 8.0), (eu + 2.0, 8.0)]), 0.3 * k, taper=False)
    d.fill(cloth, Ps([(-2.2, -15.0), (-2.4, 8.0), (-1.4, 11.0), (0.0, 12.0), (1.4, 11.0), (2.4, 8.0), (2.2, -15.0)]), ink=ink, steps=2)
    # (its sticks showing through the cloth: the spar, the spine, the crossed bracing)
    d.line(stick, Ps([(-19.4, 0.8), (19.4, 0.8)]), 0.35 * k, taper=False)
    d.line(stick, Ps([(0.0, 11.0), (0.0, -14.6)]), 0.35 * k, taper=False)
    d.line(trim, Ps([(-17.0, 1.6), (-4.0, 3.4)]), 0.3 * k, taper=False)
    d.line(trim, Ps([(17.0, 1.6), (4.0, 3.4)]), 0.3 * k, taper=False)
    return P(0.0, 0.8)


@painting(139)
def p139(d, a):   # a boy flying a kite made in the shape of a bomber, the string taut, his cap blown back
    x, y, h = 14.0, 2.0, 42.0
    F = kid(d, x, y, h, 1, pose="stride", arms=("lift", "down"), hat="ball",
            wear={"shirt": "art_white", "sleeves": "short", "shorts": "art_blue", "hat": "art_red", "socks": "art_white", "shoes": "art_brown"},
            hair="tousle", mouth="grin")
    (hx, hy), _ = F.hand["fore"]
    kx, ky = 50.0, 54.0
    bridle = bomber_kite(d, kx, ky, 32.0, cloth="art_orange", stick="art_brown", trim="art_white")
    d.line("art_white", [(hx, hy), ((hx + bridle[0]) / 2.0, (hy + bridle[1]) / 2.0 - 2.0), bridle], 0.25, taper=False)
    F.grip("fore")
    for (u, v, L) in ((30.0, 20.0, 8.0), (34.0, 26.0, 6.0)):
        d.line("art_white", [(u, v), (u + L * 0.5, v + 1.0), (u + L, v)], 0.5)
    place_name(d, a, lines=["Kite", "Season"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 140: Sunflower Sophie ----------------------------------------------------------------------------------------------------------
def sunflower(d, x, y, r, petal="art_yellow", heart="art_brown", seed="art_dark", turn=0.0):
    """A big sunflower face on: a ring of long petals round its broad brown heart, the seeds in it."""
    ink = d.ink
    for i in range(22):
        a = math.radians(turn + i * 360.0 / 22)
        cx, cy = x + math.cos(a) * r * 0.72, y + math.sin(a) * r * 0.72
        d.oval(petal, cx, cy, r * 0.34, r * 0.11, turn=math.degrees(a), ink=ink * 0.6)
    d.oval(heart, x, y, r * 0.46, r * 0.46, ink=ink)
    for i in range(14):
        a = i * 2.4
        rr = r * 0.36 * math.sqrt((i + 0.5) / 14.0)
        d.oval(seed, x + math.cos(a) * rr, y + math.sin(a) * rr, r * 0.04, r * 0.04, ink=0.0)


@painting(140)
def p140(d, a):   # a woman in a gingham dress holding a sunflower over her shoulder as a parasol
    F = _lady(d, 30.0, 2.0, 62.0, 1, extra_arms={"_b3i_parasol": ((0.62, -0.62), (0.66, 0.5))}, pose="stand", arms=("hip", "_b3i_parasol"),
             wear={"top": "art_lblue", "dots": "art_white", "sleeves": "puff", "collar": "art_white", "belt": "art_white", "hem": "knee",
                   "shoes": "art_white"},
             hair="rolls", hair_paint="art_hair")
    (hx, hy), _ = F.hand["aft"]
    # (the sunflower: its stem up from her hand at her shoulder, leaning back, a leaf on it, its great face over her head)
    fx, fy = F.P(-2.0, 9.4)
    d.limb("art_green", [(hx + 0.6, hy - 2.4), (hx, hy + 2.0), ((hx + fx) / 2.0, (hy + fy) / 2.0), (fx, fy)], [0.7, 0.7, 0.7, 0.6], ink=d.ink * 0.7)
    lx, ly = (hx + fx) / 2.0 - 0.6, (hy + fy) / 2.0 - 1.0
    d.fill("art_green", [(lx, ly), (lx - 4.0, ly + 2.4), (lx - 6.0, ly + 0.6), (lx - 3.0, ly - 0.6)], ink=d.ink * 0.7, steps=2)
    sunflower(d, fx, fy, 13.0, turn=6.0)
    d.oval("art_skin", hx, hy, F.r(0.15), F.r(0.13), ink=d.ink * 0.7)
    place_name(d, a, lines=["Sunflower", "Sophie"], where="right", room=6.0, at=38.0, caps=21.5)
