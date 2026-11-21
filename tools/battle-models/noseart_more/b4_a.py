"""Batch 4, numbers 141 to 147 (painter b4_a): a bird dog on point against a hunter's moon, a little red devil in a
flying helmet with his pitchfork, a woman navigator with her dividers and chart, a winged plow horse in harness, a
lumberjack girl balancing on a floating log, a welder at lunch on a girder and a battered farm pickup bouncing down
the road with milk cans in its bed. Flat
shapes with noseart's Drawing API and the figure kit; each name set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import _lean, place_name  # noqa: E402
from noseart_more.b3_a import _T, _lady  # noqa: E402


def _seat(make, h, pose="sit"):
    """Where a seated figure's seat is above its feet (a scratch figure, never painted)."""
    F0 = make(Drawing(1), 0.0, 0.0, h, 1, pose=pose)
    return F0.PL(*MAN_LEGS[pose][0][0])[1] - F0.r(0.3)


# ---- 141: Hunter's Moon --------------------------------------------------------------------------------------------------------------
@painting(141)
def p141(d, a):   # a bird dog on point and a bare tree, black against a big orange moon
    ink = d.ink
    d.oval("art_orange", 32.0, 36.0, 26.0, 26.0, ink=ink)
    for (u, v, r) in ((40.0, 48.0, 4.0), (22.0, 44.0, 2.6), (34.0, 30.0, 3.0), (46.0, 38.0, 1.8)):
        d.oval("art_yellow", u, v, r, r * 0.8, ink=0.0)
    # (the ground, the tree: its trunk and its bare limbs)
    d.fill("art_black", [(0.0, 0.0), (0.0, 6.4), (20.0, 7.6), (40.0, 6.8), (64.0, 7.4), (64.0, 0.0)], steps=3)
    d.limb("art_black", [(10.0, 6.0), (11.0, 20.0), (12.0, 32.0), (11.4, 44.0), (10.0, 56.0)], [2.6, 2.0, 1.6, 1.0, 0.4])
    for pts, rr in ((((11.6, 24.0), (6.0, 34.0), (2.0, 44.0)), (1.2, 0.8, 0.3)), (((12.0, 30.0), (19.0, 42.0), (26.0, 52.0)), (1.1, 0.7, 0.25)),
                    (((11.6, 40.0), (16.0, 50.0), (18.0, 58.0)), (0.8, 0.5, 0.2)), (((11.0, 46.0), (6.0, 54.0)), (0.6, 0.2)),
                    (((19.0, 42.0), (24.0, 44.0)), (0.5, 0.2)), (((6.0, 34.0), (2.0, 36.0)), (0.5, 0.2))):
        d.limb("art_black", list(pts), list(rr))
    # (the dog on point: his tail straight out behind, one forepaw lifted, his nose out toward the bird)
    d.limb("art_black", [(32.0, 24.0), (26.0, 25.6), (20.0, 26.8)], [1.3, 0.8, 0.3])
    for pts in (((34.0, 19.6), (32.6, 13.4), (33.6, 7.0)), ((36.4, 19.6), (37.4, 13.4), (36.4, 7.0)), ((46.4, 19.6), (47.0, 13.4), (46.4, 7.0))):
        d.limb("art_black", list(pts), [1.8, 1.2, 1.0])
    d.limb("art_black", [(48.6, 20.0), (51.4, 16.0), (49.6, 12.8)], [1.8, 1.2, 1.0])
    d.fill("art_black", [(31.0, 21.0), (31.6, 25.0), (40.0, 26.0), (48.0, 25.4), (50.6, 22.4), (48.0, 18.6), (40.0, 18.0), (32.6, 18.4)], steps=3)
    d.fill("art_black", [(47.0, 22.0), (50.0, 27.6), (54.0, 29.4), (58.6, 28.6), (61.4, 27.0), (61.0, 25.6), (57.0, 25.2), (52.6, 24.0), (51.0, 20.8)], steps=3)
    d.oval("art_orange", 55.6, 27.6, 0.4, 0.4, ink=0.0)
    place_name(d, a, lines=["Hunter's", "Moon"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 142: Brimstone Bill -------------------------------------------------------------------------------------------------------------
@painting(142)
def p142(d, a):   # a little red devil in a flying helmet, his pitchfork stood upright in his fist, grinning
    ink = d.ink
    x, y, h = 20.0, 4.0, 72.0
    # (the brimstone he stands in: three licks of flame round his boots, kept close so that he stands tall and narrow)
    for (u, hh, lean) in ((-7.0, 9.0, -1.6), (8.0, 10.0, 1.6), (0.6, 6.0, 0.8)):
        d.fill("art_orange", [(x + u - 2.8, y - 3.0), (x + u - 2.2, y + hh * 0.5), (x + u + lean, y + hh), (x + u + 2.2, y + hh * 0.4), (x + u + 2.8, y - 3.0)],
               ink=ink * 0.7, steps=3)
        d.fill("art_yellow", [(x + u - 1.3, y - 3.0), (x + u - 0.8, y + hh * 0.3), (x + u + lean * 0.6, y + hh * 0.6), (x + u + 1.0, y + hh * 0.2), (x + u + 1.3, y - 3.0)],
               ink=0.0, steps=2)
    F = kid(d, x, y, h, 1, pose="stand", arms=(to(0.95, 4.3, "down"), "hip"), hat="flying", skin="art_red",
            wear={"shirt": "art_red", "sleeves": "short", "shorts": "art_black", "socks": "art_red", "shoes": "art_black", "helmet": "art_brown"},
            hair="none", mouth="grin", eyes="wink")
    # (his horns through the helmet; his tail curling up behind him, its spade)
    for su in (-1.0, 1.0):
        bx, by = F.PH(su * 0.34, 7.34)
        tx, ty = F.PH(su * 0.6, 7.86)
        d.fill("art_cream", [(bx - 1.0, by), (tx, ty), (bx + 1.0, by + 0.4)], ink=ink * 0.8, steps=1)
    tail = [F.P(-0.5, 3.9), F.P(-1.2, 3.5), F.P(-1.6, 4.2), F.P(-1.4, 5.0)]
    d.limb("art_red", tail, [F.r(0.08), F.r(0.07), F.r(0.06), F.r(0.05)], ink=ink * 0.8)
    G = _T(*tail[-1], 10.0)
    d.fill("art_red", [G(0.0, -0.6), G(1.8, 0.0), G(0.0, 2.6), G(-1.8, 0.0)], ink=ink * 0.8, steps=1)
    # (his pitchfork stood upright: its butt on the ground by his boot, its shaft through his fist, its tines over his head)
    (hx, hy), _ = F.hand["fore"]
    top = (hx, F.PH(0.0, 8.3)[1] + 2.0)
    d.line("art_tan", [(hx, y - 1.0), top], 1.1, taper=False)
    d.line("art_lsteel", [(top[0] - 3.6, top[1]), (top[0] + 3.6, top[1])], 1.4, taper=False)
    for t in (-3.6, 0.0, 3.6):
        d.line("art_lsteel", [(top[0] + t, top[1]), (top[0] + t, top[1] + 8.0)], 1.3)
    F.grip("fore")
    place_name(d, a, lines=["Brimstone", "Bill"], where="right", room=6.0, at=40.0, caps=21.5)


# ---- 143: Dead Reckoning Dot ---------------------------------------------------------------------------------------------------------
@painting(143)
def p143(d, a):   # a woman navigator in flying gear, her dividers held up, her chart rolled under her arm
    x, y, h = 24.0, 2.0, 62.0
    F = lady(d, x, y, h, 1, pose="stand", arms=("wave", "hold"),
             wear={"top": "art_khaki", "jacket": "art_brown", "lapel": "art_brown", "collar": "art_cream", "trousers": "art_khaki",
                   "tall": "art_brown", "sleeves": "long", "belt": "art_brown", "buckle": "art_gold"},
             hair="bob", hair_paint="art_hair")
    P, R = F.P, F.r
    ink = d.ink
    helmet(d, *P(-0.02, 6.98), R(1.0), 1, paint="art_brown", view="front", inside=None)
    scarf(d, F, "art_white", length=2.0)
    # (her dividers, held up by their head, legs open)
    (hx, hy), _ = F.hand["fore"]
    d.oval("art_gold", hx + 0.4, hy + 2.2, 1.0, 1.0, ink=ink * 0.7)
    for lean in (-0.42, 0.42):
        d.line("art_lsteel", [(hx + 0.4, hy + 2.2), (hx + 0.4 + 9.0 * math.sin(lean), hy + 2.2 + 9.0 * math.cos(lean))], 0.6)
    d.oval("art_skin", hx, hy, R(0.15), R(0.13), ink=ink * 0.7)
    # (her chart, rolled under her other arm: the roll, its ends showing their turns, a corner of it unrolled)
    (ax, ay), _ = F.hand["aft"]
    p0, p1 = (ax - 9.0, ay + 4.0), (ax + 7.0, ay + 1.0)
    d.limb("art_cream", [p0, p1], [1.6, 1.6], ink=ink)
    for (ex, ey) in (p0, p1):
        d.oval("art_cream", ex, ey, 0.7, 1.6, ink=ink * 0.7)
        d.line("art_sky", ellipse(ex, ey, 0.35, 0.9, 0, 8), 0.2, taper=False)
    d.fill("art_cream", [(ax + 3.0, ay + 1.6), (ax + 4.6, ay - 4.6), (ax + 8.6, ay - 3.4), (ax + 6.8, ay + 1.0)], ink=ink * 0.7, steps=1)
    d.line("art_sky", [(ax + 4.4, ay - 1.0), (ax + 6.4, ay - 2.4), (ax + 7.6, ay - 1.2)], 0.3)
    d.oval("art_skin", ax, ay, R(0.15), R(0.13), ink=ink * 0.7)
    place_name(d, a, lines=["Dead", "Reckoning", "Dot"], where="right", room=6.0, at=36.0, gap=1.32, caps=21.5)


# ---- 144: The Plow Horse -------------------------------------------------------------------------------------------------------------
def draft_horse(d, x, y, size, coat="art_auburn", mane="art_hair", feather="art_white", hoof="art_dark"):
    """A heavy farm horse standing side-on to the right, his head low: broad body, thick legs feathered white at the
    hoof, a white blaze. (x, y) where his body stands, `size` his length (60 nose to tail). Returns a frame P(u, v)."""
    k = size / 60.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    d.limb(mane, Ps([(-15.6, 31.0), (-19.0, 26.0), (-19.6, 17.0)]), [2.0 * k, 2.6 * k, 1.4 * k], ink=ink)
    legs_far = (((10.0, 22.0), (11.0, 12.0), (11.4, 3.0)), ((-11.0, 22.0), (-13.0, 12.0), (-12.0, 3.0)))
    legs_near = (((13.4, 22.0), (14.6, 12.0), (14.2, 3.0)), ((-8.0, 23.0), (-10.0, 12.0), (-9.0, 3.0)))
    for pts in legs_far:
        d.limb(coat, Ps(pts), [3.6 * k, 2.4 * k, 2.2 * k], ink=ink)
        d.oval(feather, *P(pts[2][0], 3.6), 3.0 * k, 2.6 * k, ink=ink * 0.6)
        d.oval(hoof, *P(pts[2][0] + 0.4, 1.0), 2.8 * k, 1.0 * k, ink=ink * 0.6)
    d.fill(coat, Ps([(-16.0, 24.0), (-15.0, 34.0), (-6.0, 36.0), (6.0, 35.6), (14.0, 33.0), (17.0, 27.0), (14.0, 20.6), (0.0, 19.4), (-12.0, 20.6)]), ink=ink, steps=4)
    d.fill(coat, Ps([(10.0, 33.0), (14.0, 40.0), (19.0, 43.0), (24.0, 41.0), (21.0, 33.0), (16.0, 25.0)]), ink=ink, steps=3)
    d.fill(coat, Ps([(19.0, 42.0), (21.0, 45.2), (26.0, 44.4), (32.0, 38.6), (34.0, 35.8), (32.0, 33.6), (28.0, 34.4), (23.0, 37.6), (19.6, 38.6)]), ink=ink, steps=3)
    d.oval(coat, *P(32.2, 35.4), 2.6 * k, 2.4 * k, ink=ink * 0.8)
    d.fill(feather, Ps([(23.6, 43.6), (29.6, 38.6), (32.0, 36.0), (31.0, 35.4), (28.4, 37.6), (22.8, 42.6)]), ink=0.0, steps=2)
    d.oval("art_ink", *P(33.2, 35.4), 0.5 * k, 0.4 * k, ink=0.0)
    d.fill(coat, Ps([(20.4, 44.0), (19.6, 48.4), (22.6, 45.6)]), ink=ink, steps=1)
    d.oval("art_white", *P(24.4, 41.0), 0.8 * k, 0.6 * k, ink=ink * 0.6)
    d.oval("art_black", *P(24.7, 41.0), 0.4 * k, 0.45 * k, ink=0.0)
    d.limb(mane, Ps([(10.6, 34.0), (15.0, 40.6), (19.6, 44.2)]), [1.2 * k, 1.6 * k, 1.0 * k], ink=ink * 0.6)
    for pts in legs_near:
        d.limb(coat, Ps(pts), [4.0 * k, 2.6 * k, 2.4 * k], ink=ink)
        d.oval(feather, *P(pts[2][0], 3.6), 3.2 * k, 2.8 * k, ink=ink * 0.6)
        d.oval(hoof, *P(pts[2][0] + 0.4, 1.0), 3.0 * k, 1.0 * k, ink=ink * 0.6)
    return P


@painting(144)
def p144(d, a):   # a big farm horse in his harness, a pair of wings growing from his collar
    ink = d.ink
    x, y, size = 26.0, 2.0, 64.0
    k = size / 60.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    wing(d, P(12.0, 39.0), 116.0, 30.0 * k, 11.0 * k, "art_lgrey", shade="art_grey", spread=-30.0)
    draft_horse(d, x, y, size)
    # (his harness: the back pad and its band, the traces back along his side; the collar round his neck, its brass hames)
    d.shape("art_dark", [P(-2.0, 35.8), P(-2.0, 21.0), P(2.6, 21.0), P(2.6, 35.8)])
    d.line("art_dark", [P(17.0, 28.0), P(4.0, 27.0), P(-16.0, 27.4)], 1.0, taper=False)
    d.limb("art_brown", [P(12.0, 37.6), P(14.6, 31.0), P(18.6, 25.0)], [2.6 * k, 2.8 * k, 2.4 * k], ink=ink)
    d.line("art_gold", [P(11.4, 39.6), P(13.8, 31.0), P(17.6, 24.0)], 0.6, taper=False)
    for (u, v) in ((11.4, 39.6), (17.6, 24.0)):
        d.oval("art_gold", *P(u, v), 0.9, 0.9, ink=ink * 0.6)
    wing(d, P(13.0, 38.0), 104.0, 32.0 * k, 12.0 * k, "art_white", spread=-30.0)
    place_name(d, a, lines=["The", "Plow", "Horse"], where="right", room=6.0, at=34.0, gap=1.32, caps=21.5)


# ---- 145: Evergreen Evie -------------------------------------------------------------------------------------------------------------
@painting(145)
def p145(d, a):   # a lumberjack girl in a check shirt and boots balancing on a floating log, arms out
    ink = d.ink
    # (the river and the log: a short length of it under her boots, so that she stands tall, not lost in a wide scene)
    d.fill("art_blue", [(10.0, 0.0), (10.0, 9.0), (16.0, 10.2), (22.0, 9.2), (28.0, 10.4), (34.0, 9.4), (40.0, 10.4), (46.0, 9.2), (50.0, 10.0),
                        (50.0, 0.0)], ink=ink, steps=4)
    d.limb("art_brown", [(13.0, 11.6), (47.0, 11.6)], [3.4, 3.4], ink=ink)
    d.oval("art_tan", 47.0, 11.6, 1.6, 3.4, ink=ink * 0.8)
    d.line("art_lbrown", ellipse(47.0, 11.6, 0.8, 1.8, 0, 10), 0.3, taper=False)
    for u in (17.0, 27.0, 37.0):
        d.line("art_dark", [(u, 13.6), (u + 5.0, 13.2)], 0.3)
    for (u, v) in ((12.0, 9.6), (49.0, 9.6), (30.0, 9.8)):
        d.line("art_white", [(u - 2.0, v), (u, v + 1.4), (u + 2.0, v)], 0.5)
    F = lady(d, 30.0, 14.6, 76.0, 1, pose="stride", arms=("up_out", "up_out"),
             wear={"top": "art_red", "sleeves": "rolled", "collar": "art_red", "buttons": "art_dark", "trousers": "art_blue", "tall": "art_brown",
                   "belt": "art_brown"},
             hair="pony", hair_paint="art_auburn")
    # (the check of her shirt: black squares on the red)
    for i, v in enumerate((4.3, 4.65, 5.0, 5.35)):
        for j, u in enumerate((-0.48, -0.16, 0.16, 0.48)):
            if (i + j) % 2 == 0:
                d.shape("art_black", F.Ps([(u - 0.09, v - 0.09), (u - 0.09, v + 0.09), (u + 0.09, v + 0.09), (u + 0.09, v - 0.09)]))
    place_name(d, a, lines=["Evergreen", "Evie"], where="right", room=6.0, at=46.0, caps=21.5)


# ---- 146: Lunch Pail Larry -----------------------------------------------------------------------------------------------------------
@painting(146)
def p146(d, a):   # a welder sitting on a girder high up, his mask tipped up, eating his sandwich, his lunch pail by him
    ink = d.ink
    gy0, gy1 = 22.0, 28.0
    # (the girder: red lead, its flanges, its rivets)
    d.shape("art_orange", [(-4.0, gy0), (-4.0, gy1), (62.0, gy1), (62.0, gy0)])
    for v in (gy0 + 0.8, gy1 - 0.8):
        d.line("art_red", [(-4.0, v), (62.0, v)], 0.5, taper=False)
    for u in range(-2, 62, 4):
        for v in (gy0 + 0.4, gy1 - 0.4):
            d.oval("art_dark", u, v, 0.35, 0.35, ink=0.0)
    h = 58.0
    seat = _seat(man, h)
    x = 22.0
    F = man(d, x, gy1 - seat, h, 1, pose="sit", arms=("mouth", "down"),
            wear={"shirt": "art_khaki", "jacket": "art_brown", "cut": "short", "trousers": "art_blue", "boots": "art_brown", "gloves": "art_tan"},
            hair="short", mouth="open")
    # (his welding mask tipped up on his head, its dark window)
    d.fill("art_dark", F.PHs([(-0.52, 7.06), (-0.66, 7.56), (-0.1, 8.06), (0.56, 7.86), (0.6, 7.34), (0.3, 7.2)]), ink=ink, steps=2)
    d.shape("art_black", F.PHs([(-0.1, 7.6), (0.02, 7.86), (0.36, 7.76), (0.26, 7.5)]))
    # (the sandwich at his mouth: two slices, the lettuce between)
    (hx, hy), _ = F.hand["fore"]
    G = _T(hx + 1.6, hy + 0.8, 20.0)
    d.fill("art_tan", [G(-2.4, -1.6), G(-2.4, 1.6), G(2.6, 1.8), G(2.6, -1.4)], ink=ink * 0.8, steps=2)
    d.line("art_green", [G(-2.6, 0.0), G(-1.0, 0.4), G(0.6, -0.2), G(2.8, 0.4)], 0.6)
    F.grip("fore")
    # (his lunch pail on the girder beside him, its domed lid, its handle)
    px = x - 13.0
    d.shape("art_black", [(px - 4.0, gy1), (px - 4.0, gy1 + 5.0), (px + 4.0, gy1 + 5.0), (px + 4.0, gy1)])
    d.fill("art_black", ellipse(px, gy1 + 5.0, 4.0, 2.4, 0, 12, 0, 180), ink=ink, steps=2)
    d.line("art_lsteel", ellipse(px, gy1 + 6.4, 3.0, 3.4, 0, 12, 0, 180), 0.4, taper=False)
    d.line("art_lsteel", [(px - 4.0, gy1 + 2.4), (px + 4.0, gy1 + 2.4)], 0.3, taper=False)
    place_name(d, a, lines=["Lunch", "Pail", "Larry"], where="right", room=6.0, at=38.0, gap=1.32, caps=21.5)


# ---- 147: Pop's Pickup ---------------------------------------------------------------------------------------------------------------
def _milk_can(d, x, y, h, turn=0.0, paint="art_white"):
    """A farm milk can standing on (x, y), `h` tall: its body, shoulder and neck, the lid's dark cap, a handle each side."""
    ink = d.ink
    k = h / 10.0
    G = _T(x, y, turn, k)
    d.fill(paint, [G(-2.6, 0.0), G(-2.6, 6.4), G(-1.3, 7.8), G(-1.3, 8.8), G(1.3, 8.8), G(1.3, 7.8), G(2.6, 6.4), G(2.6, 0.0)], ink=ink, steps=1)
    d.line("art_ink", [G(-2.6, 1.0), G(2.6, 1.0)], 0.3, taper=False)
    d.line("art_ink", [G(-2.6, 5.2), G(2.6, 5.2)], 0.3, taper=False)
    d.fill("art_black", [G(-1.8, 8.8), G(-1.8, 9.6), G(-1.0, 10.0), G(1.0, 10.0), G(1.8, 9.6), G(1.8, 8.8)], ink=ink * 0.6, steps=1)
    for su in (-1.0, 1.0):
        d.line("art_ink", [G(su * 2.2, 6.6), G(su * 3.0, 7.4), G(su * 2.6, 8.2)], 0.4, taper=False)


@painting(147)
def p147(d, a):   # a battered old farm pickup bouncing down the road, milk cans jolting in its bed, a dust trail behind it
    ink = d.ink
    body = "art_red"
    n0 = len(d.ops)
    # (the cans in the bed, one jolted up off the floor and tipping; the bed's stake sides in front of them)
    _milk_can(d, 6.6, 15.0, 10.4)
    _milk_can(d, 20.4, 15.0, 10.4)
    _milk_can(d, 13.6, 18.6, 10.4, turn=-14.0)
    for u in (3.0, 9.0, 15.0, 21.0, 25.4):
        d.line("art_ink", [(u, 16.0), (u, 21.0)], 0.6, taper=False)
    d.line("art_ink", [(3.0, 21.0), (25.4, 21.0)], 0.6, taper=False)
    d.shape(body, [(2.0, 8.0), (2.0, 17.0), (26.0, 17.0), (26.0, 8.0)], ink=ink)
    # (the cab, Pop at the wheel in his cap; the hood, its grille and lamp; the fenders and the running board)
    d.fill(body, [(26.0, 8.0), (26.0, 26.0), (28.0, 28.0), (37.0, 28.0), (38.4, 24.0), (38.4, 8.0)], ink=ink, steps=1)
    d.shape("art_white", [(28.6, 18.6), (28.6, 25.6), (35.6, 25.6), (37.0, 18.6)])
    d.oval("art_skin", 32.6, 21.4, 2.3, 2.5, ink=ink * 0.7)
    d.fill("art_black", [(30.0, 22.6), (30.4, 24.6), (33.0, 25.2), (35.2, 24.2), (36.0, 23.0)], ink=0.0, steps=2)
    d.fill("art_white", [(32.6, 20.6), (35.2, 20.0), (34.8, 19.2), (33.0, 19.8)], ink=ink * 0.5, steps=1)
    d.oval("art_ink", 33.6, 22.0, 0.35, 0.35, ink=0.0)
    d.line("art_ink", [(28.6, 18.6), (28.6, 25.6), (35.6, 25.6), (37.0, 18.6), (28.6, 18.6)], 0.4, taper=False)
    d.fill(body, [(38.4, 8.0), (38.4, 20.0), (50.0, 18.4), (54.0, 16.0), (54.0, 8.0)], ink=ink, steps=1)
    d.shape("art_black", [(53.0, 8.6), (53.0, 15.4), (55.0, 15.0), (55.0, 9.0)])
    for v in (10.0, 11.6, 13.2):
        d.line("art_white", [(53.4, v), (54.8, v)], 0.3, taper=False)
    d.oval("art_yellow", 50.0, 18.6, 1.6, 1.6, ink=ink * 0.7)
    for (cx, w) in ((12.0, 8.0), (45.0, 8.0)):
        d.fill(body, [(cx - w, 6.0), (cx - w + 1.0, 11.0), (cx - 2.0, 13.4), (cx + 2.0, 13.4), (cx + w - 1.0, 11.0), (cx + w, 6.0)], ink=ink, steps=3)
        wheel(d, cx, 5.6, 5.4, rim="art_white", spokes=6)
    d.shape("art_black", [(20.0, 7.0), (20.0, 8.4), (37.0, 8.4), (37.0, 7.0)])
    # (its dents)
    for (u, v) in ((8.0, 12.0), (44.0, 14.0), (31.0, 12.0)):
        d.line("art_ink", ellipse(u, v, 1.6, 1.0, 0, 8, 200, 340), 0.3, taper=False)
    _lean(d, n0, 12.0, 0.2, 7.0)                       # (the whole truck tipped back on its rear wheel: its front wheel up off the road)
    # (the road; the jolt under the front wheel; the dust boiling up behind)
    d.line("art_ink", [(-20.0, 0.0), (60.0, 0.0)], 0.5, taper=False)
    for (u0, u1) in ((40.6, 39.6), (44.2, 44.2), (47.8, 48.8)):
        d.line("art_white", [(u0, 0.9), (u1, 2.9)], 0.45, taper=False)
    for (u, v, w, h, sd) in ((-15.0, 6.6, 12.0, 9.0, 3), (-5.0, 4.2, 9.0, 7.0, 5), (2.0, 2.6, 6.0, 4.6, 7)):
        cloud(d, u, v, w, h, "art_white", seed=sd, flat=True, ink=ink * 0.5, shade=None)
    for (u, v) in ((26.0, 31.0), (14.0, 31.4), (4.0, 28.8)):
        d.line("art_white", ellipse(u, v - 3.0, 2.0, 1.6, 0, 8, 30, 150), 0.4, taper=False)
    place_name(d, a, lines=["Pop's", "Pickup"], where="right", room=6.0, at=22.0, caps=21.5)
