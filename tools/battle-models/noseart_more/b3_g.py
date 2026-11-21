"""Batch 3, numbers 123 to 129 (painter b3_g): a moose bursting through a cloud with a propeller tangled in his antlers,
a red caboose with its marker lamp, a soda fountain girl with a tall soda, a soldier asleep on his cot under the
newspaper, a boy fishing from a keystone with a bent pin, one lit window and one star, and a beaver carpenter with his
hammer and his plans. Flat shapes with noseart's Drawing API and the figure kit; each name set clear of its picture by
place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T  # noqa: E402


def blade(d, hub, tip, paint="art_dark", tip_paint="art_yellow", w=1.0):
    """One propeller blade from `hub` to `tip`: broad near its root, round at its end, its tip painted (the ground crews'
    warning band)."""
    (x0, y0), (x1, y1) = hub, tip
    at = lambda t: (x0 + (x1 - x0) * t, y0 + (y1 - y0) * t)          # noqa: E731
    d.limb(paint, [at(0.0), at(0.35), at(0.75), at(1.0)], [1.0 * w, 1.7 * w, 1.5 * w, 1.1 * w], ink=d.ink)
    d.limb(tip_paint, [at(0.84), at(1.0)], [1.42 * w, 1.1 * w], ink=d.ink * 0.6)


# ---- 123: Moose on the Loose -------------------------------------------------------------------------------------------------------
@painting(123)
def p123(d, a):   # a moose bursting through a cloud, a propeller caught in his antlers
    ink = d.ink
    # (his far antler, behind his head: a broad palm reaching up and forward, its tines along its rim)
    d.shape("art_tan", [(34.0, 44.0), (36.0, 50.0), (40.0, 55.0), (44.0, 56.6), (45.4, 61.0), (47.6, 57.0), (50.8, 59.4), (51.0, 55.0), (54.8, 56.4),
                        (53.4, 51.4), (57.4, 51.6), (52.6, 47.4), (45.0, 47.2), (39.0, 44.0)])
    # (his neck and shoulders, rising out of the cloud: the hump of his withers, a dark mane along it)
    d.fill("art_brown", [(4.0, 12.0), (6.0, 30.0), (12.0, 40.0), (21.0, 45.0), (30.0, 43.0), (35.0, 34.0), (34.0, 20.0), (27.0, 12.0)], ink=ink, steps=4)
    d.limb("art_hair", [(6.6, 28.0), (11.0, 37.0), (17.0, 42.4), (23.0, 44.6)], [0.6, 1.4, 1.2, 0.5])
    # (the propeller tangled in the antlers: two blades out either side of the near palm, its hub hidden in it)
    hub = (24.0, 58.0)
    blade(d, hub, (3.0, 65.0), w=1.5)
    blade(d, hub, (45.0, 51.6), w=1.5)
    # (his long head, its heavy overhanging nose down and forward, the bell under his chin)
    d.fill("art_brown", [(34.0, 28.0), (32.4, 21.0), (34.6, 14.6), (36.8, 19.0), (38.6, 26.0)], ink=ink, steps=3)
    d.fill("art_brown", [(26.0, 40.0), (29.0, 45.6), (34.0, 46.6), (40.0, 42.6), (46.0, 36.4), (51.0, 31.4), (54.0, 27.6), (53.4, 23.0),
                         (49.0, 21.0), (45.0, 22.0), (40.0, 25.0), (34.0, 28.0), (28.0, 32.0)], ink=ink, steps=4)
    d.oval("art_brown", 50.8, 25.4, 4.4, 3.7, turn=-20.0, ink=ink)
    d.oval("art_dark", 53.4, 25.2, 1.0, 0.65, turn=-30.0, ink=0.0)
    d.line("art_dark", [(46.4, 22.6), (49.6, 22.0), (52.6, 22.4)], 0.45)
    d.oval("art_white", 38.2, 37.2, 1.7, 1.3, ink=ink * 0.6)
    d.oval("art_black", 38.9, 37.1, 0.8, 0.9, ink=0.0)
    d.line("art_dark", [(35.8, 39.4), (38.2, 40.2), (40.6, 39.4)], 0.55)
    # (his ear, laid back)
    d.fill("art_brown", [(29.0, 42.0), (22.0, 46.0), (19.6, 49.2), (25.0, 48.8), (31.0, 45.0)], ink=ink, steps=3)
    d.fill("art_tan", [(28.0, 43.0), (23.0, 46.6), (25.0, 47.6), (29.6, 44.6)], ink=0.0, steps=2)
    # (his near antler, over the blades' roots: the palm sweeping back, tines along its rim)
    d.shape("art_tan", [(31.0, 44.4), (27.0, 49.0), (21.0, 53.0), (15.0, 54.0), (12.6, 58.0), (16.8, 57.6), (16.6, 61.8), (20.0, 58.8), (21.8, 63.2),
                        (24.6, 59.0), (27.6, 62.8), (28.8, 57.2), (31.8, 60.2), (32.4, 54.2), (34.6, 57.0), (34.8, 50.0), (33.6, 46.0)])
    d.line("art_lbrown", [(31.0, 46.0), (26.0, 52.0), (20.0, 55.2)], 0.4)
    # (the cloud he bursts from, in front of his shoulders; its torn puffs flung ahead of him)
    cloud(d, 20.0, 12.0, 46.0, 18.0, paint="art_white", seed=4)
    for (u, v, r) in ((49.0, 12.0, 2.6), (55.0, 16.0, 1.8), (52.0, 6.0, 1.6), (58.0, 10.0, 1.1)):
        d.oval("art_white", u, v, r, r * 0.8, ink=ink * 0.6)
    place_name(d, a, lines=["Moose", "on the", "Loose"], where="right", room=6.0, at=34.0, gap=1.32, caps=21.5)


# ---- 124: The Red Caboose ----------------------------------------------------------------------------------------------------------
@painting(124)
def p124(d, a):   # a red caboose side-on on its rail, its marker lamp lit on the rear platform's railing
    ink = d.ink
    # (the track: ties, one rail)
    for tx in range(0, 62, 6):
        d.shape("art_brown", [(tx - 1.8, 0.0), (tx - 1.8, 1.8), (tx + 1.8, 1.8), (tx + 1.8, 0.0)])
    d.line("art_lsteel", [(-3.0, 2.5), (61.0, 2.5)], 1.0, taper=False)
    # (her trucks: four wheels, the side frames over them)
    for wx in (10.0, 17.0, 41.0, 48.0):
        wheel(d, wx, 6.0, 3.1, rim="art_lsteel", spokes=0)
    for (u0, u1) in ((8.0, 19.0), (39.0, 50.0)):
        d.shape("art_dark", [(u0, 6.4), (u0, 8.4), (u1, 8.4), (u1, 6.4)])
    # (the underframe, the platforms at either end, their steps and railings, the couplers)
    d.shape("art_dark", [(2.0, 8.6), (2.0, 11.2), (56.0, 11.2), (56.0, 8.6)])
    for (cx, sx) in ((0.0, -1), (58.0, 1)):
        d.shape("art_dark", [(cx - 0.6 * sx - 1.0, 8.4), (cx - 0.6 * sx - 1.0, 10.2), (cx + 1.0, 10.2), (cx + 1.0, 8.4)])
    for (p0, p1) in ((0.6, 5.0), (53.0, 57.4)):
        for px in (p0, p1):
            d.line("art_dark", [(px, 11.0), (px, 21.4)], 0.6, taper=False)
        d.line("art_dark", [(p0, 21.0), (p1, 21.0)], 0.6, taper=False)
        d.line("art_dark", [(p0, 16.2), (p1, 16.2)], 0.4, taper=False)
    # (her body: the red boards, a white stripe, two lit windows, the roof's overhang)
    d.shape("art_red", [(5.0, 11.2), (5.0, 30.0), (53.0, 30.0), (53.0, 11.2)])
    for bx in range(8, 53, 4):
        d.line("art_lred", [(bx, 12.0), (bx, 29.2)], 0.25, taper=False)
    d.line("art_white", [(5.4, 14.2), (52.6, 14.2)], 0.7, taper=False)
    for wx in (10.0, 43.0):
        d.shape("art_yellow", [(wx, 19.0), (wx, 25.4), (wx + 5.0, 25.4), (wx + 5.0, 19.0)])
        d.line("art_dark", [(wx + 2.5, 19.0), (wx + 2.5, 25.4)], 0.5, taper=False)
    d.shape("art_dark", [(3.6, 30.0), (3.6, 32.2), (54.4, 32.2), (54.4, 30.0)])
    # (the cupola on her roof: its windows lit, its own roof; the stovepipe and its smoke)
    d.shape("art_red", [(22.0, 32.2), (22.0, 41.0), (34.0, 41.0), (34.0, 32.2)])
    for wx in (23.6, 28.8):
        d.shape("art_yellow", [(wx, 34.6), (wx, 39.0), (wx + 3.6, 39.0), (wx + 3.6, 34.6)])
    d.shape("art_dark", [(20.8, 41.0), (20.8, 42.8), (35.2, 42.8), (35.2, 41.0)])
    d.line("art_dark", [(44.0, 32.2), (44.0, 37.6)], 1.3, taper=False)
    d.shape("art_dark", [(42.6, 37.4), (42.6, 38.6), (45.4, 38.6), (45.4, 37.4)])
    for (u, v, r) in ((46.0, 41.4, 1.5), (48.8, 44.6, 2.0), (52.6, 47.4, 2.6)):
        d.oval("art_lgrey", u, v, r, r * 0.85, ink=ink * 0.5)
    # (the marker lamp on the rear railing: its body, its red lens lit, its glow)
    d.shape("art_dark", [(55.6, 21.0), (55.6, 26.4), (58.6, 26.4), (58.6, 21.0)])
    d.shape("art_dark", [(55.2, 26.4), (57.1, 28.0), (59.0, 26.4)])
    d.oval("art_red", 59.2, 23.7, 1.1, 1.5, ink=ink * 0.6)
    d.oval("art_yellow", 59.5, 24.0, 0.4, 0.5, ink=0.0)
    for ang in (-30.0, 0.0, 30.0):
        c, s = math.cos(math.radians(ang)), math.sin(math.radians(ang))
        d.line("art_red", [(60.8 + 0.6 * c, 23.7 + 1.0 * s), (60.8 + 3.0 * c, 23.7 + 3.4 * s)], 0.5, taper=False)
    place_name(d, a, lines=["The Red", "Caboose"], where="right", room=6.0, at=24.0, caps=21.5)


# ---- 125: Soda Fountain Stella -----------------------------------------------------------------------------------------------------
@painting(125)
def p125(d, a):   # a soda fountain girl in her white uniform and paper cap, carrying one tall ice cream soda up high
    F = lady(d, 24.0, 2.0, 64.0, 1, pose="stand", arms=("wave", "hip"),
             wear={"top": "art_white", "sleeves": "short", "collar": "art_red", "tie": "art_red", "buttons": "art_red", "belt": "art_red",
                   "hem": "knee", "shoes": "art_white", "cap": "art_white"},
             hair="rolls", hair_paint="art_auburn", hat="garrison")
    ink = d.ink
    (hx, hy), _ = F.hand["fore"]
    gx, gy = hx + 0.3, hy + 1.0
    # (the soda: its foot on her palm, the short stem, the tall flared glass of pink soda, a scoop on top, the cream, the
    #  cherry, a straw)
    d.oval("art_lsteel", gx, gy, 2.6, 0.7, ink=ink * 0.7)
    d.line("art_lsteel", [(gx, gy), (gx, gy + 2.4)], 0.9, taper=False)
    d.shape("art_pink", [(gx - 1.2, gy + 2.4), (gx - 3.0, gy + 15.0), (gx + 3.0, gy + 15.0), (gx + 1.2, gy + 2.4)])
    d.line("art_white", [(gx + 1.6, gy + 4.6), (gx + 2.4, gy + 13.6)], 0.4, taper=False)
    for (u, v) in ((-0.8, 6.0), (0.4, 9.0), (-1.2, 11.6), (0.9, 12.8)):
        d.oval("art_white", gx + u, gy + v, 0.35, 0.35, ink=0.0)
    d.line("art_red", [(gx - 1.6, gy + 13.0), (gx - 4.0, gy + 23.6)], 0.7, taper=False)
    d.oval("art_white", gx, gy + 16.2, 3.6, 2.4, ink=ink * 0.8)
    d.fill("art_white", [(gx - 2.6, gy + 17.0), (gx - 1.0, gy + 20.4), (gx + 0.2, gy + 21.8), (gx + 1.4, gy + 20.2), (gx + 2.8, gy + 17.0)],
           ink=ink * 0.8, steps=3)
    d.line("art_dark", [(gx + 0.6, gy + 22.6), (gx + 1.8, gy + 25.0)], 0.3)
    d.oval("art_red", gx + 0.5, gy + 22.6, 1.3, 1.2, ink=ink * 0.7)
    d.oval("art_white", gx + 0.1, gy + 23.0, 0.35, 0.3, ink=0.0)
    place_name(d, a, lines=["Soda", "Fountain", "Stella"], where="right", room=6.0, at=36.0, gap=1.32, caps=21.5)


# ---- 126: Bunk Fatigue -------------------------------------------------------------------------------------------------------------
@painting(126)
def p126(d, a):   # a soldier asleep on his cot under his blanket, the newspaper tented over his face, his helmet on the post
    ink = d.ink
    # (the cot: its folding legs, its side rail, the canvas sagging under him)
    for (u0, u1) in ((5.0, 13.0), (45.0, 53.0)):
        d.line("art_tan", [(u0, 0.0), (u1, 13.0)], 1.0, taper=False)
        d.line("art_tan", [(u1, 0.0), (u0, 13.0)], 1.0, taper=False)
    d.fill("art_khaki", [(1.0, 12.4), (1.0, 15.6), (28.0, 14.8), (57.0, 15.6), (57.0, 12.4)], ink=ink, steps=2)
    d.line("art_tan", [(0.0, 13.0), (58.0, 13.0)], 1.0, taper=False)
    # (his helmet on the floor under the cot, its chin strap trailing)
    d.line("art_khaki", [(23.4, 0.6), (21.0, 1.4), (19.0, 0.6)], 0.4)
    d.fill("art_olive", ellipse(29.0, 0.6, 6.0, 5.6, 0, 18, 0, 180), ink=ink, steps=2)
    d.fill("art_olive", [(22.0, 0.0), (22.4, 1.4), (29.0, 1.0), (35.6, 1.4), (36.0, 0.0)], ink=ink, steps=2)
    d.line("art_dgreen", ellipse(29.0, 1.4, 5.2, 4.6, 0, 12, 20, 160), 0.35, taper=False)
    # (the pillow, his head on it; his boots standing up at the foot of the blanket)
    d.oval("art_white", 11.0, 18.2, 6.0, 2.8, ink=ink * 0.8)
    d.oval("art_skin", 12.0, 21.0, 3.0, 3.2, ink=ink)
    d.fill("art_hair", [(9.2, 21.4), (9.4, 23.6), (11.6, 24.4), (13.0, 23.6), (10.6, 22.6)], ink=ink * 0.5, steps=2)
    for (bx, by) in ((52.6, 22.0), (55.2, 20.6)):
        d.fill("art_brown", [(bx - 1.6, 15.0), (bx - 1.8, by + 2.0), (bx, by + 4.4), (bx + 1.8, by + 3.8), (bx + 1.6, 15.0)], ink=ink, steps=2)
        d.line("art_dark", [(bx + 1.0, by + 0.6), (bx + 1.6, by + 3.6)], 0.9, taper=False)
    # (the blanket over him: his hips and knees under it, the sheet's turned-down band at his chest)
    d.fill("art_olive", [(14.0, 15.0), (13.6, 21.6), (19.0, 25.4), (30.0, 26.4), (38.0, 28.0), (45.0, 25.4), (51.0, 21.0), (53.6, 16.0), (54.0, 15.0)],
           ink=ink, steps=4)
    d.line("art_white", [(14.2, 20.6), (18.6, 24.8)], 1.6, taper=False)
    # (one arm out over the blanket, his hand on his chest)
    d.limb("art_khaki", [(16.6, 23.4), (21.6, 25.8), (26.4, 25.4)], [1.5, 1.3, 1.1], ink=ink)
    d.oval("art_skin", 27.6, 25.2, 1.4, 1.1, ink=ink * 0.7)
    # (the newspaper tented over his face, its columns and fold, and his snores going up)
    d.shape("art_white", [(6.6, 19.4), (12.0, 29.4), (17.6, 20.0)])
    d.shape("art_lgrey", [(12.0, 29.4), (17.6, 20.0), (14.4, 19.8)])
    for t in (0.3, 0.5, 0.7):
        d.line("art_grey", [(6.6 + 5.4 * t + 0.8, 19.4 + 10.0 * t - 0.6), (6.6 + 5.4 * t + 3.4, 19.4 + 10.0 * t - 5.8)], 0.25, taper=False)
    for (u, v, k) in ((14.0, 31.6, 1.0), (16.0, 35.0, 1.3), (18.6, 38.8, 1.6)):
        d.line("art_white", [(u, v), (u + 0.8 * k, v + 0.8 * k), (u + 1.6 * k, v), (u + 2.4 * k, v + 0.8 * k)], 0.45 * k)
    place_name(d, a, lines=["Bunk", "Fatigue"], where="right", room=6.0, at=18.0, caps=21.5)


# ---- 127: Keystone Kid ---------------------------------------------------------------------------------------------------------------
@painting(127)
def p127(d, a):   # a boy in a newsboy cap sitting on a tall keystone, fishing with a bent pin on a string
    ink = d.ink
    # (the creek at the stone's foot, where his line goes in)
    d.fill("art_blue", [(28.0, 0.0), (28.0, 3.6), (33.0, 4.8), (38.0, 4.0), (43.0, 5.0), (48.0, 4.2), (52.0, 4.8), (54.0, 3.4), (54.0, 0.0)], ink=ink, steps=3)
    # (the keystone, tall: its face wider at the top, its top corners stepped, its top, its side in shadow)
    top = 40.0
    d.shape("art_lgrey", [(9.0, 0.0), (6.0, top - 3.4), (4.0, top - 3.4), (4.0, top), (32.0, top), (32.0, top - 3.4), (30.0, top - 3.4), (27.0, 0.0)])
    d.shape("art_grey", [(27.0, 0.0), (30.0, top - 3.4), (32.0, top - 3.4), (32.0, top), (35.0, top + 2.4), (30.4, 2.0)])
    d.shape("art_white", [(4.0, top), (7.0, top + 2.4), (35.0, top + 2.4), (32.0, top)])
    d.line("art_grey", [(10.0, 4.0), (7.6, 30.0)], 0.3, taper=False)
    d.line("art_grey", [(15.0, 12.0), (19.0, 18.0), (17.0, 25.0)], 0.3)
    # (the boy on it, his legs over its edge; both hands on his pole)
    h = 50.0
    F0 = kid(Drawing(1), 0.0, 0.0, h, 1, pose="sit")
    seat = F0.PL(*MAN_LEGS["sit"][0][0])[1] - F0.r(0.3)
    F = kid(d, 19.0, top + 1.4 - seat, h, 1, pose="sit", arms=(to(1.8, 4.9, "down"), to(1.4, 4.6, "down")), hat="newsboy",
            wear={"shirt": "art_white", "sleeves": "long", "knickers": "art_tan", "socks": "art_white", "shoes": "art_brown", "hat": "art_grey",
                  "braces": "art_red"}, hair="tousle", mouth="smile")
    (fx, fy), _ = F.hand["fore"]
    (ax, ay), _ = F.hand["aft"]
    tip = (46.0, 70.0)
    dx, dy = tip[0] - ax, tip[1] - ay
    ln = math.hypot(dx, dy)
    butt = (ax - dx / ln * 3.0, ay - dy / ln * 3.0)
    d.limb("art_brown", [butt, (fx, fy), tip], [0.7, 0.55, 0.3], ink=ink * 0.7)
    F.grip("aft")
    F.grip("fore")
    # (the line down to the water, its bent pin hanging just above it)
    d.line("art_white", [tip, (46.4, 8.4)], 0.22, taper=False)
    d.line("art_lsteel", [(46.4, 8.6), (46.4, 6.2)] + ellipse(45.6, 6.2, 0.8, 0.8, 0, 8, 0, -180)[1:], 0.35, taper=False)
    place_name(d, a, lines=["Keystone", "Kid"], where="right", room=6.0, at=40.0, caps=21.5)


# ---- 128: First Light ----------------------------------------------------------------------------------------------------------------
@painting(128)
def p128(d, a):   # one warm lit window in a dark house on a deep blue square of night, and one star (the ruling: no more)
    ink = d.ink
    d.shape("art_navy", [(0.0, 0.0), (0.0, 54.0), (54.0, 54.0), (54.0, 0.0)])
    # (the house, black against it: its gable and chimney; the ground it stands on)
    d.shape("art_black", [(0.0, 0.0), (0.0, 4.0), (8.0, 4.4), (8.0, 22.0), (27.0, 36.0), (46.0, 22.0), (46.0, 4.6), (54.0, 5.0), (54.0, 0.0)])
    d.shape("art_black", [(36.0, 28.4), (36.0, 37.4), (40.6, 37.4), (40.6, 25.0)])
    # (the window: warm, four panes, its sill)
    d.shape("art_yellow", [(21.6, 10.0), (21.6, 20.6), (32.4, 20.6), (32.4, 10.0)])
    d.line("art_black", [(27.0, 10.0), (27.0, 20.6)], 0.9, taper=False)
    d.line("art_black", [(21.6, 15.3), (32.4, 15.3)], 0.9, taper=False)
    # (the star)
    star(d, 14.0, 44.0, 3.6, "art_yellow", ink=ink * 0.6)
    place_name(d, a, lines=["First", "Light"], where="right", room=6.0, at=27.0, caps=21.5)


# ---- 129: Beaver Lodge ---------------------------------------------------------------------------------------------------------------
def beaver(d, x, y, size, s=1, coat="art_brown", tail="art_hair", muzzle="art_tan", apron="art_tan"):
    """A beaver standing up on his hind feet, `size` tall, his feet's middle at (x, y), facing s: his flat scaly tail
    on the ground behind him, a carpenter's apron with its pocket, his big front teeth. Returns his frame (P) and his
    paws' anchors: fore (his shoulder's, for an arm of the painter's own) and aft."""
    k = size / 54.0
    P = lambda u, v: (x + s * (u - 24.0) * k, y + (v - 2.0) * k)    # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]                         # noqa: E731
    ink = d.ink
    # (his tail, flat on the ground behind him, its scales crossed)
    d.fill(tail, Ps([(16.0, 4.0), (6.0, 2.0), (-1.0, 3.6), (-2.6, 8.4), (2.0, 11.4), (10.0, 10.6), (17.0, 8.0)]), ink=ink, steps=3)
    for i in range(4):
        d.line("art_ink", Ps([(1.0 + 3.4 * i, 3.0), (4.0 + 3.4 * i, 10.6)]), 0.25 * k, taper=False)
        d.line("art_ink", Ps([(-1.0 + 3.4 * i, 9.6), (5.0 + 3.4 * i, 3.0)]), 0.25 * k, taper=False)
    # (his hind feet)
    for u in (18.0, 31.0):
        d.oval(tail, *P(u, 2.6), 4.4 * k, 1.8 * k, ink=ink * 0.8)
    # (his body, a pear; the apron over it: its bib, its strings, its pocket)
    d.fill(coat, Ps([(14.0, 6.0), (12.0, 18.0), (13.0, 30.0), (17.0, 38.0), (24.0, 41.0), (31.0, 38.0), (35.0, 30.0), (36.0, 18.0), (33.0, 6.0),
                     (24.0, 3.6)]), ink=ink, steps=4)
    d.fill(apron, Ps([(17.0, 6.0), (15.6, 20.0), (19.0, 30.0), (19.0, 36.0), (29.0, 36.0), (29.0, 30.0), (32.4, 20.0), (31.0, 6.0)]), ink=ink, steps=2)
    d.line(apron, Ps([(19.4, 35.6), (21.6, 39.6)]), 0.7 * k, taper=False)
    d.line(apron, Ps([(28.6, 35.6), (27.0, 39.6)]), 0.7 * k, taper=False)
    d.line("art_ink", Ps([(19.6, 13.0), (19.6, 20.0), (28.0, 20.0), (28.0, 13.0)]), 0.3 * k, taper=False)
    d.line("art_red", Ps([(26.0, 19.4), (27.2, 23.6)]), 0.6 * k, taper=False)
    # (his head: round, his tan muzzle and black nose, the two big front teeth, an eye, small round ears, whiskers)
    d.oval(coat, *P(20.6, 52.4), 1.9 * k, 1.9 * k, ink=ink)
    d.oval(coat, *P(25.6, 54.0), 1.7 * k, 1.7 * k, ink=ink)
    d.fill(coat, Ps([(17.0, 42.0), (17.0, 49.0), (21.0, 53.0), (28.0, 54.0), (33.0, 51.0), (37.0, 47.0), (38.6, 43.0), (35.0, 39.6), (28.0, 38.6),
                     (21.0, 39.4)]), ink=ink, steps=4)
    d.oval(muzzle, *P(34.2, 43.0), 4.2 * k, 3.0 * k, ink=ink * 0.6)
    d.shape("art_white", Ps([(32.8, 40.8), (32.8, 37.0), (36.0, 37.0), (36.0, 40.8)]))
    d.line("art_ink", Ps([(34.4, 40.6), (34.4, 37.2)]), 0.25 * k, taper=False)
    d.oval("art_black", *P(38.0, 45.0), 1.5 * k, 1.1 * k, ink=0.0)
    d.oval("art_white", *P(30.4, 48.0), 1.3 * k, 1.3 * k, ink=ink * 0.6)
    d.oval("art_black", *P(31.0, 48.0), 0.75 * k, 0.8 * k, ink=0.0)
    d.line("art_ink", Ps([(28.4, 50.2), (30.6, 50.8), (32.6, 50.0)]), 0.35 * k)
    for (dv, du) in ((0.8, 4.6), (-0.4, 4.4)):
        d.line("art_ink", Ps([(36.4, 43.4 + dv), (36.4 + du, 43.8 + dv * 2.0)]), 0.2 * k)
    return P


@painting(129)
def p129(d, a):   # a beaver in his carpenter's apron, his hammer up in one paw, the lodge's plans held up in the other
    x, y, size = 28.0, 2.0, 56.0
    k = size / 54.0
    ink = d.ink
    P = lambda u, v: (x + (u - 24.0) * k, y + (v - 2.0) * k)        # noqa: E731
    # (the plans, a big blue sheet held up behind him, a lodge drawn on it in white lines)
    sheet = [P(-2.0, 26.0), P(-0.6, 50.0), P(16.0, 51.0), P(14.6, 27.0)]
    d.shape("art_blue", sheet)
    d.line("art_white", [P(2.0, 31.0), P(7.0, 42.0), P(12.0, 31.6), P(2.0, 31.0)], 0.5 * k, taper=False)
    d.line("art_white", [P(5.4, 31.2), P(6.0, 35.4), P(8.6, 35.6), P(8.4, 31.4)], 0.4 * k, taper=False)
    d.line("art_white", [P(1.0, 29.0), P(13.6, 29.6)], 0.3 * k, taper=False)
    d.line("art_white", [P(1.4, 46.0), P(14.6, 46.8)], 0.3 * k, taper=False)
    beaver(d, x, y, size, 1)
    # (his aft arm up to the sheet's corner; his fore arm raising the hammer)
    d.limb("art_brown", [P(17.0, 35.0), P(14.0, 38.0), P(14.4, 42.0)], [2.2 * k, 1.9 * k, 1.6 * k], ink=ink)
    d.oval("art_hair", *P(14.6, 43.2), 1.8 * k, 1.7 * k, ink=ink * 0.8)
    G = _T(*P(41.4, 46.6), 75.0 - 90.0)
    d.line("art_tan", [P(38.0, 34.0), P(41.4, 46.4)], 1.0 * k, taper=False)
    d.shape("art_dark", [G(-4.2 * k, -1.2 * k), G(-4.2 * k, 1.3 * k), G(3.4 * k, 1.3 * k), G(3.6 * k, -1.2 * k)])
    d.limb("art_brown", [P(31.0, 36.0), P(36.6, 31.6), P(39.2, 36.6)], [2.4 * k, 2.0 * k, 1.8 * k], ink=ink)
    d.oval("art_hair", *P(39.0, 37.8), 1.9 * k, 1.8 * k, ink=ink * 0.8)
    place_name(d, a, lines=["Beaver", "Lodge"], where="right", room=6.0, at=30.0, caps=21.5)
