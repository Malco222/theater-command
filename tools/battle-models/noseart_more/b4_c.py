"""Batch 4, numbers 155 to 161 (painter b4_c): a jug and a washboard with music rising, a pigeon reading a road map
with its message capsule on its leg, a kid sister in pigtails and overalls with her slingshot, a bomber stitched
together from mismatched parts, a woman roller skating in a calf-length skirt, the sun rising between two blue ridges
and a telephone operator in her headset plugging in cords. Flat shapes with noseart's Drawing API and the figure kit;
each name set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_a import sun  # noqa: E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b2_h import plain_bomber  # noqa: E402
from noseart_more.b3_a import _T, _lady, note  # noqa: E402
from noseart_more.b3_i import girl_hair  # noqa: E402


# ---- 155: Jug Band Jake --------------------------------------------------------------------------------------------------------------
@painting(155)
def p155(d, a):   # a stoneware jug and a washboard leaning on it, a few notes rising; no figure
    ink = d.ink
    # (the washboard, leaning behind the jug: its wooden frame, its ribbed tin, its feet)
    G = _T(36.0, 0.0, -8.0)
    d.fill("art_tan", [G(-9.0, 0.0), G(-9.0, 50.0), G(9.0, 50.0), G(9.0, 0.0)], ink=ink, steps=1)
    d.fill("art_lsteel", [G(-6.4, 10.0), G(-6.4, 40.0), G(6.4, 40.0), G(6.4, 10.0)], ink=ink * 0.8, steps=1)
    for i in range(14):
        v = 11.6 + i * 2.0
        d.line("art_grey", [G(-6.0, v), G(6.0, v)], 0.6, taper=False)
    d.fill("art_tan", [G(-9.0, 42.0), G(-9.0, 50.0), G(9.0, 50.0), G(9.0, 42.0)], ink=ink * 0.8, steps=1)
    d.oval("art_dark", *G(0.0, 46.0), 1.6, 1.6, ink=0.0)
    for su in (-1.0, 1.0):
        d.shape("art_tan", [G(su * 9.0, 0.0), G(su * 9.0, -2.4), G(su * 6.4, -2.4), G(su * 6.4, 0.0)], ink=ink * 0.8)
    # (the jug: cream stoneware, its top dipped brown, its handle, its neck and cork)
    jx = 18.0
    body = ellipse(jx, 14.0, 13.0, 14.0, 0, 28, -90, 270)
    d.fill("art_cream", body, ink=ink, steps=3)
    d.fill("art_brown", ellipse(jx, 14.0, 13.0, 14.0, 0, 18, 20, 160) + [(jx - 6.0, 30.0), (jx + 6.0, 30.0)], ink=0.0, steps=2)
    d.fill("art_brown", [(jx - 5.0, 26.0), (jx - 4.0, 34.0), (jx + 4.0, 34.0), (jx + 5.0, 26.0)], ink=ink, steps=1)
    d.fill("art_tan", [(jx - 3.4, 34.0), (jx - 3.0, 38.0), (jx + 3.0, 38.0), (jx + 3.4, 34.0)], ink=ink * 0.8, steps=1)
    d.line("art_brown", [(jx + 4.6, 31.0), (jx + 10.0, 31.0), (jx + 11.6, 26.0), (jx + 10.6, 22.0)], 2.2)
    d.line("art_white", [(jx - 8.0, 6.0), (jx - 10.0, 12.0), (jx - 9.0, 18.0)], 0.9)
    d.oval("art_ink", jx, 0.4, 11.0, 1.2, ink=0.0)
    # (the music: notes rising from the jug's mouth)
    note(d, jx - 6.0, 44.0, 8.0, "art_white", turn=-10.0)
    note(d, jx + 4.0, 52.0, 9.0, "art_white", turn=6.0)
    note(d, jx - 2.0, 62.0, 7.0, "art_white", turn=-4.0)
    place_name(d, a, lines=["Jug", "Band", "Jake"], where="right", room=6.0, at=30.0, gap=1.32, caps=21.5)


# ---- 156: Pigeon Post ----------------------------------------------------------------------------------------------------------------
def pigeon(d, x, y, size, coat="art_grey", neck="art_green", white="art_white"):
    """A homing pigeon standing, facing right, its feet at (x, y), `size` tall to its crown: plump grey body, two dark
    bars on its wing, green sheen at its neck, small head with a pale cere, pink feet; a red message capsule on its near
    leg. Returns its near wing's tip (where it holds a thing)."""
    k = size / 30.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    d.fill(coat, Ps([(-6.0, 9.0), (-14.0, 6.0), (-15.0, 8.6), (-8.0, 13.0)]), ink=ink, steps=1)
    d.fill(coat, Ps([(-9.0, 10.0), (-7.0, 17.0), (-1.0, 21.0), (5.0, 21.4), (8.0, 17.0), (7.4, 10.6), (2.0, 6.4), (-4.0, 6.6)]), ink=ink, steps=4)
    d.fill(neck, Ps([(1.6, 18.4), (3.0, 22.6), (6.6, 22.6), (7.8, 17.8), (5.4, 16.4)]), ink=0.0, steps=3)
    d.oval(coat, *P(5.4, 25.6), 3.4 * k, 3.2 * k, ink=ink)
    d.fill("art_dark", Ps([(8.4, 26.4), (11.6, 25.4), (8.4, 24.6)]), ink=ink * 0.6, steps=1)
    d.oval(white, *P(8.6, 26.2), 0.8 * k, 0.5 * k, ink=0.0)
    d.oval("art_orange", *P(6.2, 26.4), 0.9 * k, 0.9 * k, ink=0.0)
    d.oval("art_ink", *P(6.4, 26.4), 0.45 * k, 0.45 * k, ink=0.0)
    for su in (-1.0, 0.6):
        x0 = 0.6 + su * 1.2
        d.line("art_pink", Ps([(x0, 7.0), (x0 + 0.4, 1.2)]), 0.7 * k)
        d.line("art_pink", Ps([(x0 - 1.6, 0.4), (x0 + 0.4, 1.2), (x0 + 2.4, 0.4)]), 0.5 * k)
    d.shape("art_red", Ps([(1.6, 3.0), (1.6, 6.0), (3.4, 6.0), (3.4, 3.0)]), ink=ink * 0.8)
    d.line("art_lsteel", Ps([(1.4, 4.5), (3.6, 4.5)]), 0.3 * k, taper=False)
    # (its near wing, folded, with its two dark bars; its tip brought forward)
    d.fill("art_lgrey", Ps([(-5.0, 17.0), (3.0, 18.6), (9.0, 14.0), (12.0, 12.0), (4.0, 10.0), (-6.0, 11.0)]), ink=ink, steps=3)
    for v in (13.0, 15.0):
        d.line("art_dark", Ps([(-3.0, v), (2.0, v + 0.4)]), 0.7 * k)
    return P(12.0, 12.0)


@painting(156)
def p156(d, a):   # a homing pigeon with its message capsule on its leg, puzzling over a road map held open in its wing
    ink = d.ink
    tip = pigeon(d, 16.0, 2.0, 56.0)
    # (the road map, open: its folds, its roads, a dotted route)
    mx, my = tip[0] + 2.0, tip[1] - 4.0
    G = _T(mx, my, 8.0)
    d.fill("art_cream", [G(0.0, 0.0), G(0.0, 20.0), G(18.0, 22.0), G(18.0, 2.0)], ink=ink, steps=1)
    for u in (6.0, 12.0):
        d.line("art_tan", [G(u, 0.6), G(u, 20.6)], 0.4, taper=False)
    d.line("art_red", [G(1.0, 4.0), G(6.0, 9.0), G(11.0, 7.0), G(17.0, 16.0)], 0.6)
    d.line("art_blue", [G(2.0, 17.0), G(8.0, 14.0), G(10.0, 18.0), G(16.0, 20.0)], 0.5)
    for t in range(6):
        d.oval("art_dark", *G(3.0 + t * 2.6, 12.0 + math.sin(t) * 2.0), 0.4, 0.4, ink=0.0)
    d.oval("art_lgrey", *tip, 2.4, 2.0, ink=ink * 0.7)
    # (its puzzlement: a little question of dashes over its head)
    for (u, v) in ((26.0, 60.0), (30.0, 64.0)):
        d.line("art_white", [(u, v), (u + 1.6, v + 2.4)], 0.7)
    place_name(d, a, lines=["Pigeon", "Post"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 157: Kid Sister -----------------------------------------------------------------------------------------------------------------
@painting(157)
def p157(d, a):   # a little girl in pigtails and overalls drawing back her slingshot, one eye shut
    ink = d.ink
    x, y, h = 20.0, 2.0, 76.0
    F = kid(d, x, y, h, 1, pose="wide", arms=(to(1.86, 5.4, "down"), to(0.52, 5.6, "out")),
            wear={"shirt": "art_yellow", "sleeves": "short", "overalls": "art_blue", "shoes": "art_brown", "socks": "art_white"},
            hair="none", mouth="flat", eyes="wink")
    girl_hair(d, F, paint="art_auburn", ribbon="art_red")
    # (the slingshot: its forked stick in her outstretched fist, the band drawn back to her other hand, the stone in it)
    (fx, fy), _ = F.hand["fore"]
    (bx, by), _ = F.hand["aft"]
    d.line("art_brown", [(fx, fy - 6.0), (fx, fy + 1.4)], 2.0, taper=False)
    tips = [(fx - 3.2, fy + 9.0), (fx + 3.4, fy + 9.0)]
    for t in tips:
        d.line("art_brown", [(fx, fy + 1.4), t], 1.8, taper=False)
    F.grip("fore")
    for t in tips:
        d.line("art_tan", [t, (bx + 1.0, by + 0.6)], 0.9, taper=False)
    d.oval("art_lgrey", bx + 1.6, by + 0.6, 1.6, 1.4, ink=ink * 0.8)
    F.grip("aft")
    place_name(d, a, lines=["Kid", "Sister"], where="right", room=6.0, at=36.0, caps=21.5)


# ---- 158: Bits and Pieces ------------------------------------------------------------------------------------------------------------
_TOP = [(0.0, 0.05), (0.08, 0.07), (0.3, 0.09), (0.6, 0.105), (0.8, 0.105), (0.88, 0.095), (0.95, 0.07), (1.0, 0.03)]
_BOT = [(0.0, 0.035), (0.05, 0.02), (0.2, -0.02), (0.45, -0.05), (0.7, -0.055), (0.9, -0.05), (0.97, -0.03), (1.0, 0.0)]


def _edge(pts, u):
    for (u0, v0), (u1, v1) in zip(pts, pts[1:]):
        if u0 <= u <= u1:
            return v0 + (v1 - v0) * (u - u0) / (u1 - u0)
    return pts[-1][1]


@painting(158)
def p158(d, a):   # a bomber stitched together from mismatched parts: panels of every colour, sewn seams, a patch
    ink = d.ink
    L = 74.0
    P = plain_bomber(d, 0.0, 6.0, L, tilt=4.0)
    # (its body in sections from other aircraft: each section's panel laid over the body between two stations)
    secs = ((0.02, 0.24, "art_lsteel"), (0.24, 0.46, "art_khaki"), (0.62, 0.78, "art_tan"), (0.9, 0.94, "art_red"))
    for (u0, u1, paint) in secs:
        us = [u0 + (u1 - u0) * i / 8 for i in range(9)]
        top = [P(u, _edge(_TOP, u) - 0.004) for u in us]
        bot = [P(u, _edge(_BOT, u) + 0.004) for u in reversed(us)]
        d.shape(paint, top + bot, ink=ink * 0.8)
    # (the fin from yet another one; a square patch on the body)
    d.fill("art_red", [P(u, v) for (u, v) in ((0.03, 0.2), (0.07, 0.27), (0.12, 0.27), (0.16, 0.22), (0.12, 0.12), (0.04, 0.11))], ink=ink * 0.8, steps=3)
    d.shape("art_lgrey", [P(0.52, 0.06), P(0.58, 0.062), P(0.58, 0.0), P(0.52, -0.002)], ink=ink * 0.8)
    # (the seams: big cross stitches across each join, and round the patch)
    for u in (0.24, 0.46, 0.62, 0.78, 0.9):
        v0, v1 = _edge(_BOT, u) + 0.008, _edge(_TOP, u) - 0.008
        n = max(3, int((v1 - v0) / 0.022))
        for i in range(n):
            v = v0 + (v1 - v0) * (i + 0.5) / n
            d.line("art_white", [P(u - 0.012, v - 0.008), P(u + 0.012, v + 0.008)], 0.6)
            d.line("art_white", [P(u - 0.012, v + 0.008), P(u + 0.012, v - 0.008)], 0.6)
    for (u0, v0, u1, v1) in ((0.52, 0.06, 0.58, 0.062), (0.58, 0.062, 0.58, 0.0), (0.58, 0.0, 0.52, -0.002), (0.52, -0.002, 0.52, 0.06)):
        for t in (0.25, 0.75):
            cx, cy = u0 + (u1 - u0) * t, v0 + (v1 - v0) * t
            d.line("art_white", [P(cx - 0.008, cy - 0.006), P(cx + 0.008, cy + 0.006)], 0.5)
    place_name(d, a, lines=["Bits and", "Pieces"], where="below", room=5.0, caps=21.5)


# ---- 159: Roller Rink Rhoda ----------------------------------------------------------------------------------------------------------
@painting(159)
def p159(d, a):   # a woman in a calf-length skirt rolling along on roller skates, arms out, her skirt swinging
    ink = d.ink
    x, y, h = 26.0, 4.0, 72.0
    F = lady(d, x, y, h, 1, pose="stride", arms=("lift", "up_out"),
             wear={"top": "art_white", "sleeves": "puff", "collar": "art_red", "skirt": "art_red", "hem": "calf", "flare": 0.6, "shoes": "art_white",
                   "belt": "art_red"},
             hair="rolls", hair_paint="art_blond")
    R = F.r
    # (her skates: a plate under each boot, two wheels showing, a toe stop)
    for (au, av) in ((1.18, 0.44), (-1.1, 0.62)):
        fx, fy = F.P(au, av)
        plate_y = fy - R(0.42)
        d.line("art_lsteel", [(fx - R(0.28), plate_y), (fx + R(0.5), plate_y)], R(0.09), taper=False)
        for du in (-0.16, 0.4):
            d.oval("art_red", fx + R(du), plate_y - R(0.14), R(0.13), R(0.13), ink=ink * 0.8)
            d.oval("art_lsteel", fx + R(du), plate_y - R(0.14), R(0.05), R(0.05), ink=0.0)
        d.oval("art_dark", fx + R(0.58), plate_y - R(0.06), R(0.08), R(0.07), ink=0.0)
    # (speed: three strokes behind her)
    for v in (2.0, 3.0, 4.0):
        p0 = F.P(-1.6, v)
        d.line("art_white", [p0, (p0[0] - 9.0, p0[1])], 0.6)
    place_name(d, a, lines=["Roller", "Rink", "Rhoda"], where="right", room=6.0, at=36.0, gap=1.32, caps=21.5)


# ---- 160: Shenandoah Sunrise ---------------------------------------------------------------------------------------------------------
@painting(160)
def p160(d, a):   # the sun coming up in the gap between two long blue ridges, one behind the other
    ink = d.ink
    sun(d, 44.0, 13.0, 9.0, rays=14, long=0.7, short=0.4, paint="art_yellow", start=10.0)

    def ridge(x0, x1, peaks, base=0.0):
        pts = [(x0, base)]
        n = 40
        for i in range(n + 1):
            u = x0 + (x1 - x0) * i / n
            v = base
            for (px, ph, pw) in peaks:
                v = max(v, base + ph * math.exp(-((u - px) / pw) ** 2))
            v += 0.5 * math.sin(u * 0.9) + 0.3 * math.sin(u * 2.3)
            pts.append((u, max(base, v)))
        pts.append((x1, base))
        return pts
    d.shape("art_blue", ridge(-4.0, 50.0, ((14.0, 21.0, 16.0), (32.0, 16.0, 10.0))), ink=ink)
    d.shape("art_navy", ridge(40.0, 94.0, ((58.0, 15.0, 9.0), (76.0, 22.0, 15.0))), ink=ink)
    d.shape("art_navy", ridge(-4.0, 94.0, ((10.0, 5.0, 14.0), (46.0, 4.0, 18.0), (84.0, 6.0, 14.0)), base=-3.0), ink=ink * 0.8)
    place_name(d, a, lines=["Shenandoah", "Sunrise"], where="below", room=5.0, caps=21.5)


# ---- 161: Switchboard Selma ----------------------------------------------------------------------------------------------------------
@painting(161)
def p161(d, a):   # a telephone operator on her stool at the board, headset on, plugging in a cord
    ink = d.ink
    x, y, h = 18.0, 0.0, 68.0
    kw = dict(pose="sit", arms=("wave", "down"),
              wear={"top": "art_lblue", "sleeves": "long", "collar": "art_white", "skirt": "art_navy", "hem": "knee", "shoes": "art_black"},
              hair="rolls", hair_paint="art_hair")
    F0 = lady(Drawing(1), x, y, h, 1, **kw)
    floor = F0.P(0.0, 1.9)[1]
    # (her stool)
    s0, s1 = F0.P(-0.7, 3.5), F0.P(0.5, 3.5)
    d.shape("art_brown", [(s0[0], s0[1] - 1.6), (s0[0], s0[1]), (s1[0], s1[1]), (s1[0], s1[1] - 1.6)], ink=ink)
    for u in (-0.6, 0.4):
        p = F0.P(u, 3.4)
        d.line("art_brown", [p, (p[0] + (u + 0.1) * 2.0, floor)], 1.0, taper=False)
    # (the board: its wooden case, its dark panel of jacks in rows, its shelf of plugs, the cords)
    b0, b1 = F0.P(2.3, 0.0)[0], F0.P(4.6, 0.0)[0]
    top = F0.P(0.0, 8.6)[1]
    shelf = F0.P(0.0, 4.1)[1]
    d.shape("art_brown", [(b0, floor), (b0, top), (b1, top), (b1, floor)], ink=ink)
    d.shape("art_dark", [(b0 + 2.0, shelf + 3.0), (b0 + 2.0, top - 2.0), (b1 - 2.0, top - 2.0), (b1 - 2.0, shelf + 3.0)], ink=ink * 0.6)
    cols = 5
    rows = 7
    for i in range(rows):
        for j in range(cols):
            jx = b0 + 4.0 + (b1 - b0 - 8.0) * j / (cols - 1)
            jy = shelf + 5.4 + (top - shelf - 9.0) * i / (rows - 1)
            d.oval("art_lsteel", jx, jy, 0.8, 0.8, ink=0.0)
            d.oval("art_black", jx, jy, 0.4, 0.4, ink=0.0)
    d.shape("art_tan", [(b0 - 4.0, shelf), (b0 - 4.0, shelf + 2.0), (b1, shelf + 2.0), (b1, shelf)], ink=ink * 0.8)
    for t, paint in ((0.2, "art_red"), (0.45, "art_black"), (0.7, "art_red")):
        px = b0 + (b1 - b0) * t
        jy = shelf + 5.4 + (top - shelf - 9.0) * (0.3 + t * 0.6)
        jx = b0 + 4.0 + (b1 - b0 - 8.0) * ((t * 3.0) % 1.0)
        d.line(paint, [(px, shelf + 2.0), (px + 1.0, shelf - 2.0), (jx - 1.0, shelf - 1.0), (jx, jy)], 0.5)
    F = lady(d, x, y, h, 1, **kw)
    # (the cord in her hand, its plug going into a jack)
    (hx, hy), _ = F.hand["fore"]
    d.line("art_red", [(b0 - 2.0, shelf + 2.0), (hx - 1.0, hy - 6.0), (hx, hy)], 0.5)
    d.line("art_lsteel", [(hx, hy), (b0 + 3.4, hy + 0.4)], 0.8, taper=False)
    d.oval("art_skin", hx, hy, F.r(0.15), F.r(0.13), ink=ink * 0.7)
    # (her headset: the band over her hair, the earphone; the horn of her mouthpiece on her breastplate)
    hc = F.P(0.0, 6.9)
    headset(d, hc[0], hc[1], 10.4)
    mp = F.P(0.3, 5.36)
    d.line("art_black", [F.P(-0.1, 5.7), mp], 0.4, taper=False)
    d.fill("art_black", [(mp[0], mp[1] - 1.2), (mp[0] + 3.0, mp[1] + 0.4), (mp[0] + 3.0, mp[1] + 2.4), (mp[0], mp[1] + 1.2)], ink=0.0, steps=1)
    place_name(d, a, lines=["Switchboard", "Selma"], where="below", room=5.0, caps=21.5)
