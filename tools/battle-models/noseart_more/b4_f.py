"""Batch 4, numbers 176 to 182 (painter b4_f): a boy kneeling at a marble ring with his thumb cocked, an otter sliding
belly-down off a cloud bank, a kettle of sap boiling over its fire in the snow, a badger backed against a log, a dog
panting under a blazing sun with a paper fan, a stationmaster waving his cap with the depot small behind him and a pair
of rubber boots stuck fast in the mud. Flat shapes with noseart's Drawing API and the figure kit; each name set clear of
its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_a import sun  # noqa: E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T  # noqa: E402


# ---- 176: Knuckle Down ---------------------------------------------------------------------------------------------------------------
@painting(176)
def p176(d, a):   # a boy kneeling at the edge of a chalk marble ring, knuckles down, thumb cocked to shoot
    ink = d.ink
    x, y, h = 18.0, 2.0, 62.0
    kw = dict(pose="kneel", arms=(to(1.9, 0.9, "down"), "hip"),
              wear={"shirt": "art_red", "sleeves": "rolled", "knickers": "art_brown", "socks": "art_white", "shoes": "art_brown", "cap": "art_blue"},
              hat="ball", mouth="flat", eyes="wink")
    F0 = kid(Drawing(1), x, y, h, 1, **kw)
    (hx, hy), _ = F0.hand["fore"]
    # (the ring chalked on the ground, the marbles in it)
    rx, ry = hx + 18.0, y + 1.0
    d.fill("art_tan", ellipse(rx - 6.0, ry, 30.0, 5.0, 0, 30), ink=0.0, steps=2)
    d.line("art_white", ellipse(rx, ry, 15.0, 3.4, 0, 30), 0.6, taper=False)
    for (u, v, paint) in ((-4.0, 0.6, "art_blue"), (2.0, -1.0, "art_red"), (5.0, 1.2, "art_green"), (-1.0, -1.8, "art_yellow"), (8.0, -0.4, "art_white")):
        d.oval(paint, rx + u, ry + v, 1.3, 1.3, ink=ink * 0.6)
        d.oval("art_white", rx + u - 0.4, ry + v + 0.4, 0.35, 0.35, ink=0.0)
    F = kid(d, x, y, h, 1, **kw)
    (hx, hy), _ = F.hand["fore"]
    # (his shooter on his cocked thumb, and its aim)
    d.oval("art_sky", hx + 1.6, hy + 0.8, 1.4, 1.4, ink=ink * 0.7)
    d.oval("art_white", hx + 1.2, hy + 1.2, 0.4, 0.4, ink=0.0)
    for i in range(3):
        d.line("art_white", [(hx + 4.0 + i * 3.0, hy + 0.6 - i * 0.2), (hx + 5.4 + i * 3.0, hy + 0.5 - i * 0.2)], 0.4, taper=False)
    place_name(d, a, lines=["Knuckle", "Down"], where="below", room=5.0, caps=21.5)


# ---- 177: Otter Slide ----------------------------------------------------------------------------------------------------------------
def otter(d, x, y, size, turn=-24.0, coat="art_brown", belly="art_tan"):
    """An otter sliding head-first on its belly, its nose at (x, y), `size` long nose to tail tip, going `turn` degrees:
    a long sleek body, forepaws tucked back, its thick tapering tail out behind, round little ears, whiskers, a grin."""
    G = _T(x, y, turn + 180.0)
    k = size / 40.0
    Q = lambda u, v: G(u * k, v * k)                     # noqa: E731
    ink = d.ink
    d.limb(coat, [Q(20.0, 1.0), Q(30.0, 1.6), Q(40.0, 2.6)], [3.0 * k, 2.0 * k, 0.6 * k], ink=ink)
    d.fill(coat, [Q(2.0, -1.0), Q(3.0, 3.6), Q(10.0, 5.4), Q(18.0, 5.0), Q(23.0, 3.0), Q(23.0, -1.0), Q(16.0, -3.0), Q(6.0, -3.2)], ink=ink, steps=4)
    d.fill(belly, [Q(4.0, -2.4), Q(12.0, -2.6), Q(20.0, -1.4), Q(12.0, -0.8), Q(5.0, -0.8)], ink=0.0, steps=3)
    d.limb(coat, [Q(19.0, 0.0), Q(23.0, -2.0), Q(27.0, -2.6)], [1.6 * k, 1.3 * k, 1.1 * k], ink=ink * 0.8)
    d.limb(coat, [Q(7.0, 0.0), Q(10.0, -2.4), Q(14.0, -3.0)], [1.6 * k, 1.3 * k, 1.1 * k], ink=ink * 0.8)
    d.oval(coat, *Q(1.0, 1.0), 4.0 * k, 3.4 * k, turn=turn, ink=ink)
    d.oval(belly, *Q(-1.4, 0.0), 2.4 * k, 1.8 * k, turn=turn, ink=0.0)
    d.oval("art_ink", *Q(-3.0, 0.4), 0.8 * k, 0.6 * k, ink=0.0)
    d.oval(coat, *Q(3.6, 4.0), 1.0 * k, 0.9 * k, ink=ink * 0.6)
    d.oval("art_ink", *Q(-0.2, 2.2), 0.5 * k, 0.5 * k, ink=0.0)
    d.oval("art_white", *Q(-0.4, 2.4), 0.18 * k, 0.18 * k, ink=0.0)
    d.line("art_ink", [Q(-2.4, -0.8), Q(-1.0, -1.4), Q(0.4, -0.8)], 0.3 * k)
    for dv in (-0.6, 0.4):
        d.line("art_white", [Q(-2.2, dv), Q(-5.6, dv * 2.6)], 0.15 * k)


@painting(177)
def p177(d, a):   # an otter sliding belly-down and head-first off a sloping cloud bank, a spray of cloud behind
    ink = d.ink
    # (the cloud bank, sloping down to the right: a chain of lumps, each lower than the last)
    for i, (u, v, w, hh) in enumerate(((6.0, 26.0, 26.0, 18.0), (22.0, 18.0, 26.0, 16.0), (38.0, 10.0, 24.0, 13.0), (52.0, 4.0, 20.0, 10.0))):
        cloud(d, u, v, w, hh, "art_white", seed=177 + i, flat=False)
    # (its track down the slope, the cloud spray it throws up)
    otter(d, 50.0, 17.0, 40.0, turn=-30.0)
    for (u, v) in ((18.0, 40.0), (21.0, 43.0), (15.0, 44.0), (24.0, 39.0)):
        d.oval("art_white", u, v, 1.4, 1.2, ink=ink * 0.5)
    place_name(d, a, lines=["Otter", "Slide"], where="right", room=6.0, at=24.0, caps=21.5)


# ---- 178: Sugaring Off ---------------------------------------------------------------------------------------------------------------
@painting(178)
def p178(d, a):   # a black kettle of sap hung from a pole on two forked stakes, boiling over its fire in the snow
    ink = d.ink
    cx = 24.0
    # (the snow)
    d.fill("art_white", [(-4.0, 0.0), (-4.0, 6.0), (10.0, 8.0), (30.0, 7.0), (52.0, 8.0), (52.0, 0.0)], ink=ink, steps=3)
    # (the two forked stakes and the pole across them)
    for u in (-16.0, 16.0):
        d.line("art_brown", [(cx + u, 6.0), (cx + u * 0.9, 34.0)], 1.4, taper=False)
        d.line("art_brown", [(cx + u * 0.9, 34.0), (cx + u * 0.9 - 2.0, 38.0)], 1.0)
        d.line("art_brown", [(cx + u * 0.9, 34.0), (cx + u * 0.9 + 2.0, 38.0)], 1.0)
    d.line("art_brown", [(cx - 19.0, 35.6), (cx + 19.0, 35.6)], 1.4, taper=False)
    # (the fire under: logs crossed, flames up round the kettle's foot)
    for t in (-20.0, 20.0):
        G = _T(cx, 7.0, t)
        d.line("art_brown", [G(-10.0, 0.0), G(10.0, 0.0)], 2.0, taper=False)
    for (u, hh) in ((-9.0, 9.0), (-4.0, 12.0), (2.0, 14.0), (8.0, 10.0)):
        d.fill("art_red", [(cx + u - 4.0, 6.0), (cx + u - 2.4, 6.0 + hh * 0.6), (cx + u, 6.0 + hh), (cx + u + 2.0, 6.0 + hh * 0.5), (cx + u + 4.0, 6.0)],
               ink=ink * 0.6, steps=3)
        d.fill("art_yellow", [(cx + u - 2.2, 6.0), (cx + u - 0.6, 6.0 + hh * 0.5), (cx + u + 0.4, 6.0 + hh * 0.7), (cx + u + 2.2, 6.0)], ink=0.0, steps=2)
    # (the kettle on its chain: black, round-bellied, sap boiling up over its lip, steam)
    d.line("art_dark", [(cx, 35.6), (cx, 27.0)], 0.5, taper=False)
    d.line("art_dark", ellipse(cx, 25.0, 8.0, 5.0, 0, 16, 20, 160), 0.6, taper=False)
    d.fill("art_black", [(cx - 9.0, 24.0), (cx - 10.0, 18.0), (cx - 7.0, 13.0), (cx, 11.6), (cx + 7.0, 13.0), (cx + 10.0, 18.0), (cx + 9.0, 24.0)], ink=ink, steps=4)
    d.oval("art_black", cx, 24.0, 9.0, 1.6, ink=ink)
    d.fill("art_gold", [(cx - 9.0, 24.4), (cx - 6.0, 26.4), (cx - 2.0, 25.4), (cx + 2.0, 27.0), (cx + 6.0, 25.6), (cx + 9.0, 24.4), (cx + 9.4, 21.0), (cx + 8.0, 19.0),
                        (cx + 8.0, 23.0), (cx - 8.0, 23.4), (cx - 9.6, 20.0)], ink=ink * 0.6, steps=3)
    cloud(d, cx - 2.0, 44.0, 16.0, 8.0, "art_white", seed=178, flat=False)
    cloud(d, cx + 6.0, 54.0, 12.0, 6.0, "art_white", seed=179, flat=False)
    place_name(d, a, lines=["Sugaring", "Off"], where="right", room=6.0, at=30.0, caps=21.5, retry=8)   # (his first tries ran its I into the R and N)


# ---- 179: Cornered Badger ------------------------------------------------------------------------------------------------------------
@painting(179)
def p179(d, a):   # a badger backed against a fallen log, facing us, teeth bared, claws out, fur up
    ink = d.ink
    cx = 30.0
    # (the log behind it: bark, a cut end with its rings)
    d.limb("art_brown", [(-4.0, 12.0), (52.0, 16.0)], [8.0, 8.0], ink=ink)
    for u in (4.0, 16.0, 30.0, 42.0):
        d.line("art_dark", [(u, 8.0 + u * 0.07), (u + 5.0, 9.0 + u * 0.07)], 0.4)
    d.oval("art_tan", 52.0, 16.0, 3.4, 8.0, ink=ink * 0.8)
    d.line("art_lbrown", ellipse(52.0, 16.0, 1.8, 4.6, 0, 14), 0.3, taper=False)
    # (the badger: squat grey body low, front legs splayed, claws out; the striped face, mouth open on its teeth)
    d.fill("art_grey", [(cx - 16.0, 2.0), (cx - 18.0, 10.0), (cx - 12.0, 20.0), (cx, 23.0), (cx + 12.0, 20.0), (cx + 18.0, 10.0), (cx + 16.0, 2.0)],
           ink=ink, steps=4)
    for su in (-1.0, 1.0):
        d.limb("art_black", [(cx + su * 9.0, 10.0), (cx + su * 14.0, 4.0), (cx + su * 16.0, 1.0)], [3.0, 2.4, 2.0], ink=ink)
        for t in (-1.6, 0.0, 1.6):
            d.line("art_white", [(cx + su * 16.0 + t, 1.0), (cx + su * 17.4 + t * 1.2, -1.6)], 0.5)
    for i in range(9):
        u = -12.0 + i * 3.0
        v = 19.0 + 3.0 * math.cos(u / 14.0)
        d.fill("art_grey", [(cx + u - 1.2, v), (cx + u, v + 2.6), (cx + u + 1.2, v)], ink=ink * 0.6, steps=1)
    hy = 22.0
    d.oval("art_white", cx, hy, 8.4, 7.4, ink=ink)
    d.fill("art_black", [(cx - 4.6, hy + 7.0), (cx - 2.0, hy + 6.6), (cx - 2.2, hy - 2.0), (cx - 3.2, hy - 5.6), (cx - 6.8, hy - 2.6)], ink=0.0, steps=2)
    d.fill("art_black", [(cx + 4.6, hy + 7.0), (cx + 2.0, hy + 6.6), (cx + 2.2, hy - 2.0), (cx + 3.2, hy - 5.6), (cx + 6.8, hy - 2.6)], ink=0.0, steps=2)
    for su in (-1.0, 1.0):
        d.oval("art_grey", cx + su * 7.6, hy + 5.0, 2.0, 1.6, ink=ink * 0.8)
        d.oval("art_white", cx + su * 3.4, hy + 1.2, 0.8, 0.6, ink=0.0)
        d.oval("art_red", cx + su * 3.4, hy + 1.2, 0.4, 0.4, ink=0.0)
        d.line("art_ink", [(cx + su * 1.6, hy + 3.0), (cx + su * 5.0, hy + 2.0)], 0.5)
    d.oval("art_ink", cx, hy - 1.6, 1.4, 1.0, ink=0.0)
    d.fill("art_red", [(cx - 3.4, hy - 3.4), (cx, hy - 2.6), (cx + 3.4, hy - 3.4), (cx + 2.4, hy - 6.4), (cx - 2.4, hy - 6.4)], ink=ink * 0.8, steps=2)
    for u in (-2.4, -0.8, 0.8, 2.4):
        d.fill("art_white", [(u + cx - 0.5, hy - 3.2), (u + cx, hy - 4.6), (u + cx + 0.5, hy - 3.2)], ink=0.0, steps=1)
    for u in (-2.0, 2.0):
        d.fill("art_white", [(u + cx - 0.5, hy - 6.4), (u + cx, hy - 5.0), (u + cx + 0.5, hy - 6.4)], ink=0.0, steps=1)
    place_name(d, a, lines=["Cornered", "Badger"], where="below", room=5.0, caps=21.5)


# ---- 180: Dog Days -------------------------------------------------------------------------------------------------------------------
@painting(180)
def p180(d, a):   # a hound sitting under a blazing sun, tongue lolling, fanning himself with a folded paper fan
    ink = d.ink
    sun(d, 10.0, 60.0, 8.0, rays=14, long=0.7, short=0.4, paint="art_yellow", start=10.0)
    K = lambda u, v: (30.0 + u, 2.0 + v)                 # noqa: E731
    coat, ear = "art_tan", "art_brown"
    # (sitting: haunch, tail along the ground, front legs straight, chest up)
    d.line(coat, [K(-10.0, 2.0), K(-18.0, 1.6), K(-22.0, 3.0)], 1.6)
    d.fill(coat, [K(-12.0, 0.0), K(-14.0, 8.0), K(-10.0, 18.0), K(-4.0, 28.0), K(2.0, 30.0), K(6.0, 26.0), K(6.0, 14.0), K(4.0, 0.0)], ink=ink, steps=4)
    d.oval(coat, *K(-8.0, 6.0), 7.0, 6.0, ink=ink)
    d.fill("art_white", [K(-1.0, 26.0), K(4.0, 24.0), K(4.0, 12.0), K(0.0, 10.0), K(-2.0, 16.0)], ink=0.0, steps=3)
    d.limb(coat, [K(2.0, 16.0), K(3.0, 8.0), K(3.4, 1.0)], [2.4, 2.0, 1.8], ink=ink)
    d.oval(coat, *K(4.6, 0.8), 2.6, 1.2, ink=ink * 0.8)
    # (the head: long ears down, eyes half shut in the heat, tongue hanging out, sweat drops)
    hx, hy = 4.0, 34.0
    d.fill(ear, [K(hx - 6.0, hy + 3.0), K(hx - 9.0, hy - 4.0), K(hx - 7.0, hy - 9.0), K(hx - 4.0, hy - 4.0)], ink=ink, steps=3)
    d.fill(coat, [K(hx - 6.0, hy + 2.0), K(hx - 4.0, hy + 6.0), K(hx + 2.0, hy + 6.4), K(hx + 6.0, hy + 3.0), K(hx + 12.0, hy + 0.6), K(hx + 12.6, hy - 2.6),
                  K(hx + 6.0, hy - 4.0), K(hx - 2.0, hy - 5.0), K(hx - 6.0, hy - 2.0)], ink=ink, steps=3)
    d.oval("art_ink", *K(hx + 12.0, hy - 0.6), 1.2, 1.0, ink=0.0)
    d.fill("art_pink", [K(hx + 6.0, hy - 3.6), K(hx + 8.0, hy - 9.0), K(hx + 10.0, hy - 9.4), K(hx + 9.0, hy - 3.2)], ink=ink * 0.8, steps=2)
    d.line("art_red", [K(hx + 8.4, hy - 4.0), K(hx + 9.0, hy - 8.0)], 0.3)
    d.line("art_ink", [K(hx + 1.6, hy + 2.4), K(hx + 4.4, hy + 2.0)], 0.5)
    d.line("art_ink", [K(hx + 1.6, hy + 1.6), K(hx + 4.4, hy + 1.8)], 0.3)
    d.fill(ear, [K(hx - 2.0, hy + 4.0), K(hx - 4.0, hy - 2.0), K(hx - 2.0, hy - 8.0), K(hx + 1.0, hy - 2.0)], ink=ink, steps=3)
    for (u, v) in ((hx + 6.0, hy + 9.0), (hx - 8.0, hy + 6.0), (hx + 10.0, hy + 6.0)):
        d.fill("art_sky", [K(u, v + 1.6), K(u - 0.8, v), K(u, v - 0.6), K(u + 0.8, v)], ink=ink * 0.4, steps=2)
    # (the paper fan in his near paw, held up beside his face: pleated, red and white)
    d.limb(coat, [K(-2.0, 22.0), K(6.0, 20.0), K(12.0, 24.0)], [2.2, 1.8, 1.6], ink=ink)
    d.oval(coat, *K(12.6, 24.6), 1.8, 1.6, ink=ink * 0.8)
    fx, fy = K(12.6, 25.0)
    for i in range(7):
        a0, a1 = 40.0 + i * 14.0, 40.0 + (i + 1) * 14.0
        pts = [(fx, fy), (fx + 13.0 * math.cos(math.radians(a0)), fy + 13.0 * math.sin(math.radians(a0))),
               (fx + 13.0 * math.cos(math.radians(a1)), fy + 13.0 * math.sin(math.radians(a1)))]
        d.shape("art_red" if i % 2 == 0 else "art_white", pts, ink=ink * 0.5)
    for (u, v, t) in ((26.0, 36.0, 40.0), (22.0, 40.0, 70.0)):
        G = _T(*K(u, v), t)
        d.line("art_white", [G(0.0, 0.0), G(3.0, 0.0)], 0.5)
    place_name(d, a, lines=["Dog", "Days"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 181: Whistle Stop ---------------------------------------------------------------------------------------------------------------
@painting(181)
def p181(d, a):   # a stationmaster big in front waving his cap high, the little depot behind with its sign board blank
    ink = d.ink
    # (the depot, small behind: its long low roof, its walls, its windows, the platform)
    d.shape("art_lgrey", [(-4.0, 10.0), (-4.0, 12.0), (30.0, 12.0), (30.0, 10.0)], ink=ink * 0.6)
    d.shape("art_tan", [(-2.0, 12.0), (-2.0, 22.0), (26.0, 22.0), (26.0, 12.0)], ink=ink)
    d.fill("art_red", [(-5.0, 21.0), (12.0, 27.0), (29.0, 21.0)], ink=ink, steps=1)
    for u in (2.0, 9.0, 18.0):
        d.shape("art_dark", [(u, 14.0), (u, 18.6), (u + 3.0, 18.6), (u + 3.0, 14.0)])
    d.shape("art_cream", [(6.0, 22.6), (6.0, 24.6), (18.0, 24.6), (18.0, 22.6)], ink=ink * 0.6)
    d.shape("art_dark", [(22.0, 12.0), (22.0, 18.0), (24.4, 18.0), (24.4, 12.0)])
    # (the stationmaster, big in front: frock coat, his cap off and waved high, a watch chain; mouth open in a call)
    F = man(d, 34.0, 0.0, 76.0, 1, pose="wide", arms=("raise", "hip"), hat=None,
            wear={"shirt": "art_white", "tie": "art_dark", "jacket": "art_navy", "lapel": "art_navy", "buttons": "art_gold", "trousers": "art_navy",
                  "shoes": "art_black"},
            hair="short", hair_paint="art_grey", mouth="open", moustache="full")
    (hx, hy), _ = F.hand["fore"]
    G = _T(hx + 1.0, hy + 3.0, 14.0)
    d.fill("art_navy", [G(-4.4, 0.0), G(-4.0, 3.0), G(4.0, 3.4), G(4.6, 0.0)], ink=ink, steps=2)
    d.fill("art_black", [G(-4.4, 0.0), G(-1.0, -1.4), G(5.6, -1.0), G(4.6, 0.0)], ink=ink * 0.8, steps=1)
    d.shape("art_gold", [G(-4.2, 0.4), G(-4.2, 1.0), G(4.4, 1.2), G(4.4, 0.6)], ink=0.0)
    F.grip("fore")
    d.line("art_gold", [F.P(0.2, 4.6), F.P(0.5, 4.3), F.P(0.7, 4.5)], 0.4)
    for (u, v, t) in ((14.0, 70.0, 30.0), (16.0, 64.0, 0.0)):
        G = _T(34.0 + u, v, t)
        d.line("art_white", [G(0.0, 0.0), G(3.4, 0.0)], 0.6)
    place_name(d, a, lines=["Whistle", "Stop"], where="right", room=6.0, at=36.0, caps=21.5)


# ---- 182: Rubber Boots ---------------------------------------------------------------------------------------------------------------
@painting(182)
def p182(d, a):   # a pair of tall rubber boots stuck fast in a puddle of mud, sunk to the ankles, mud splashed up them
    ink = d.ink

    def boot(x, y, lean, paint="art_black", top="art_red"):
        G = _T(x, y, lean)                                # (its heel's foot at (x, y), up its shaft)
        d.fill(paint, [G(-1.0, 0.0), G(-1.0, 30.0), G(9.0, 30.0), G(9.0, 9.0), G(12.0, 6.0), G(17.0, 4.4), G(19.0, 2.0), G(18.0, 0.0)], ink=ink, steps=3)
        d.fill(top, [G(-1.0, 27.0), G(-1.0, 30.6), G(9.0, 30.6), G(9.0, 27.0)], ink=ink * 0.6, steps=1)
        d.oval("art_dark", *G(4.0, 30.6), 5.0, 1.2, turn=lean, ink=ink * 0.6)
        d.line("art_grey", [G(1.0, 5.0), G(1.0, 25.0)], 0.5)
        d.line("art_grey", [G(10.0, 8.0), G(14.0, 5.6)], 0.4)
        for (p, q, r) in ((3.0, 10.0, 1.4), (6.0, 14.0, 1.0), (2.0, 17.0, 0.8), (12.0, 5.0, 1.2)):
            d.oval("art_brown", *G(p, q), r * 1.3, r, ink=0.0)
    boot(16.0, 2.0, -8.0)
    boot(2.0, 1.0, 4.0)
    # (the mud over their feet, its lumps, the splashes)
    d.fill("art_brown", [(-8.0, 0.0), (-10.0, 3.0), (-4.0, 6.6), (6.0, 6.0), (12.0, 7.4), (22.0, 6.4), (32.0, 7.0), (42.0, 4.0), (40.0, 0.0)], ink=ink, steps=4)
    d.line("art_lbrown", [(-4.0, 5.0), (8.0, 4.4), (20.0, 5.0)], 0.5)
    for (u, v, r) in ((-6.0, 12.0, 1.2), (36.0, 12.0, 1.0), (32.0, 18.0, 0.8), (-2.0, 18.0, 0.9)):
        d.oval("art_brown", u, v, r * 1.2, r, ink=ink * 0.5)
    place_name(d, a, lines=["Rubber", "Boots"], where="right", room=6.0, at=18.0, caps=21.5)
