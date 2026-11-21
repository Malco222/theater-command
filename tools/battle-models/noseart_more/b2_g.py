"""Batch 2, numbers 67 to 73 (painter b2_g): a woman in factory coveralls punching her card in a time clock, her lunch
pail in her other hand; a pelican with its pouch crammed with bombs; a winged envelope with a heart on its seal; a
steaming tin mug with wings; a bowling ball sending three pins flying; a woman in a raincoat under her umbrella on a
bridge tower in the fog; and a snow-woman in earmuffs and a scarf holding up an icicle. Each is drawn from flat shapes
with noseart's Drawing API; the helpers below (a time clock and its card, a lunch pail, a pelican, a heart, an
envelope, a tin mug and its steam, a bowling ball and its pins, a bridge tower, an umbrella, a snow-woman and her
icicle) are built the same way, for these paintings."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402


# ---- helpers ------------------------------------------------------------------------------------------------------------
def _frame(x, y, k=1.0, s=1, turn=0.0):
    """A frame of one's own on the grid: P(u, v), u toward the side it faces (s), v up, k grid units to one of its own,
    the whole turned `turn` degrees anticlockwise about (x, y)."""
    c, sn = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        u, v = s * u * k, v * k
        return (x + u * c - v * sn, y + u * sn + v * c)
    return P


def _wire(d, paint, pts, w):
    """A wire or a thin rod: a dark line with the paint along it, so that it keeps its edge."""
    d.line("art_ink", pts, w * 1.7, taper=False)
    d.line(paint, pts, w, taper=False)


# ---- 67: a time clock, its card, a lunch pail ---------------------------------------------------------------------------
def time_clock(d, x, y, k=1.0, case="art_lbrown", trim="art_brown", post="art_dark"):
    """A time clock on its pedestal, as a plant gate had one: the middle of its foot at (x, y), 62 k tall. A cast foot
    and a post; on them an oak case with an arched head, a white dial in a brass bezel up in the arch (no figures on it,
    its hands at four, when the swing shift comes on), and under the dial the card slot, a narrow dark slit cut into
    the case's face. Returns its frame (the slot's middle is P(0, 34.0))."""
    P = _frame(x, y, k)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    w0 = d.ink
    d.fill(post, Q([(-8.0, 0.0), (-7.0, 2.0), (-2.4, 3.0), (2.4, 3.0), (7.0, 2.0), (8.0, 0.0)]), steps=3)
    d.shape(post, Q([(-1.8, 2.4), (-1.8, 26.4), (1.8, 26.4), (1.8, 2.4)]))
    d.shape(case, Q([(-10.0, 27.0), (10.0, 27.0)] + ellipse(0.0, 51.0, 10.0, 10.6, 0, 18, 0, 180)))
    d.shape(trim, Q([(-11.0, 25.4), (11.0, 25.4), (11.0, 28.4), (-11.0, 28.4)]))
    d.line("art_cream", Q([(-8.4, 31.0), (-8.6, 44.0), (-7.6, 52.0)]), 0.7 * k)           # (the case's shine, up its edge)
    # (the dial)
    cy = 51.2
    d.oval("art_gold", *P(0.0, cy), 7.2 * k, 7.2 * k)
    d.oval("art_white", *P(0.0, cy), 6.0 * k, 6.0 * k, ink=w0 * 0.6)
    for a_ in (90, 0, 270, 180):
        c_, s_ = math.cos(math.radians(a_)), math.sin(math.radians(a_))
        d.line("art_ink", Q([(4.6 * c_, cy + 4.6 * s_), (5.6 * c_, cy + 5.6 * s_)]), 0.75 * k, taper=False)
    d.line("art_ink", Q([(0.0, cy), (3.1 * math.cos(math.radians(-30)), cy + 3.1 * math.sin(math.radians(-30)))]), 1.0 * k, taper=False)
    d.line("art_ink", Q([(0.0, cy), (0.0, cy + 4.8)]), 0.7 * k, taper=False)
    d.oval("art_ink", *P(0.0, cy), 0.8 * k, 0.8 * k, ink=0.0)
    # (the card slot: a narrow dark slit in the case's face)
    d.shape("art_black", Q([(-4.6, 33.4), (4.6, 33.4), (4.6, 34.6), (-4.6, 34.6)]), ink=w0 * 0.5)
    return P


def time_card(d, P, paint="art_tan"):
    """Her time card going down into the clock's slot (its frame P): its lower half in the slot, its upper half
    standing out of it, plain, its top edge where her fingers push it down."""
    d.shape(paint, [P(-2.7, 34.0), P(2.7, 34.0), P(2.7, 41.0), P(-2.7, 41.0)], ink=d.ink * 0.8)
    d.shape("art_black", [P(-2.75, 33.4), P(2.75, 33.4), P(2.75, 34.0), P(-2.75, 34.0)], ink=0.0)   # (the slot's far lip, in front of the card)


def lunch_pail(d, x, y, paint="art_lsteel", end="art_grey", latch="art_grey", bail="art_lsteel"):
    """A worker's lunch pail hanging from its bail, the bail's grip at (x, y) where the hand holds it, about 16 wide:
    seen from a little to one end, a straight-sided tin box with a tall half-round domed lid (where the vacuum bottle
    rides), nearly as high as the box; its near end, the dome's end, darker; a latch on its front at the lid's seam;
    and the wire bail rising from the dome's two ends to the grip."""
    # (its own frame: the near front corner of its foot at (0, 0); along it toward its far end Lv, across its near end
    #  Dv, the box Hb high, the dome R high over it; the grip at G)
    Lv, Dv, Hb, R = (-9.0, 1.4), (6.6, 1.0), 8.0, 6.6
    G = (-1.0, 22.0)

    def T(u, v):
        return (x + u - G[0], y + v - G[1])

    def Ts(pts):
        return [T(u, v) for u, v in pts]

    def arc(cx, cy, t0, t1, n=10):                       # (the dome's end: a half ellipse on the end's top edge)
        return [(cx + math.cos(t) * Dv[0] / 2, cy + math.cos(t) * Dv[1] / 2 + math.sin(t) * R)
                for t in (t0 + (t1 - t0) * i / n for i in range(n + 1))]
    near_c = (Dv[0] / 2, Hb + Dv[1] / 2)
    far_c = (near_c[0] + Lv[0], near_c[1] + Lv[1])
    piv_near = (near_c[0], near_c[1] + R * 0.3)
    piv_far = (far_c[0], far_c[1] + R * 0.3)
    w = 0.62
    # (the far arm of the bail, from behind the dome)
    _wire(d, bail, Ts([piv_far, (piv_far[0] + 0.6, piv_far[1] + 6.0), (G[0] - 1.4, G[1] - 1.0), (G[0], G[1])]), w)
    # (the box: its long side, its near end)
    d.shape(paint, Ts([(0.0, 0.0), Lv, (Lv[0], Lv[1] + Hb), (0.0, Hb)]))
    d.shape(end, Ts([(0.0, 0.0), Dv, (Dv[0], Dv[1] + Hb), (0.0, Hb)]))
    # (the dome: its long side over the box, then its near end)
    side = arc(*near_c, math.pi, math.pi / 2) + list(reversed(arc(*far_c, math.pi, math.pi / 2)))
    d.fill(paint, Ts(side), steps=2)
    d.fill(end, Ts(arc(*near_c, 0.0, math.pi)), steps=2)
    d.line("art_white", Ts([(near_c[0] - 2.4 + Lv[0] * 0.15, Hb + R * 0.72 + Lv[1] * 0.15), (near_c[0] - 2.4 + Lv[0] * 0.75, Hb + R * 0.76 + Lv[1] * 0.75)]), 0.7)   # (its shine)
    # (the latch on its front, at the lid's seam)
    mx, my = Lv[0] * 0.5, Lv[1] * 0.5 + Hb
    d.shape(latch, Ts([(mx - 0.9, my + 1.2), (mx + 0.9, my + 1.2), (mx + 0.9, my - 2.0), (mx - 0.9, my - 2.0)]), ink=d.ink * 0.7)
    # (the near arm of the bail, over the dome's end, and the pivot it turns on)
    _wire(d, bail, Ts([piv_near, (piv_near[0] - 0.4, piv_near[1] + 5.6), (G[0] + 1.6, G[1] - 1.2), (G[0], G[1])]), w)
    d.oval(bail, *T(*piv_near), 0.75, 0.75, ink=d.ink * 0.6)


# ---- 68: a pelican ----------------------------------------------------------------------------------------------------------
BOMBS_68 = (((37.0, 31.4), (21.39, 23.1)), ((25.6, 35.6), (17.85, 19.71)), ((31.6, 35.0), (19.77, 21.86)))   # ((tail, nose) of each, its own grid)


def pelican(d, x, y, size, body="art_white", wing_="art_lgrey", tips="art_dark", bill="art_orange", pouch="art_yellow",
            feet="art_orange", load=3, gape=20.0):
    """A pelican standing side on, facing right, its feet at (x, y), `size` tall to its crown: its beak open, the upper
    bill swung up `gape` degrees, and its great pouch crammed so full of bombs that their tails stick out of the gap
    between its bills. Returns its frame."""
    k = size / 47.0
    P = _frame(x, y, k)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    w0 = d.ink
    # (its legs and its webbed feet)
    for du in (-3.0, 2.2):
        d.limb(feet, Q([(du, 7.0), (du + 0.3, 3.6), (du + 0.6, 1.0)]), [1.1 * k, 0.9 * k, 0.8 * k], ink=w0 * 0.8)
        d.fill(feet, Q([(du - 1.6, 0.0), (du + 4.8, -0.3), (du + 5.2, 0.9), (du + 1.0, 1.8), (du - 1.4, 1.2)]), ink=w0 * 0.8, steps=3)
    # (its tail, its body, its wing folded along its side: the grey coverts over the dark primaries)
    d.fill(body, Q([(-12.0, 9.0), (-20.4, 5.0), (-21.0, 7.2), (-14.0, 13.0)]), steps=3)
    d.fill(body, Q([(-15.0, 9.4), (-9.0, 4.8), (0.0, 4.2), (7.0, 7.4), (10.4, 13.0), (11.0, 20.0), (8.6, 26.4), (3.0, 30.4),
                    (-4.0, 30.4), (-10.0, 26.0), (-14.4, 18.0)]), steps=5)
    d.fill(tips, Q([(-5.0, 24.0), (-11.6, 20.6), (-16.6, 14.0), (-21.4, 6.8), (-24.0, 3.4), (-19.8, 4.4), (-14.6, 8.0), (-8.6, 12.4),
                    (-2.6, 16.6)]), steps=3)
    d.fill(wing_, Q([(6.0, 25.6), (0.6, 28.8), (-6.6, 27.0), (-11.6, 22.6), (-14.6, 17.0), (-12.0, 14.6), (-10.2, 16.2), (-8.6, 12.8),
                     (-5.8, 14.4), (-3.6, 12.0), (0.2, 15.0), (3.2, 18.8)]), steps=3)
    # (its neck in an S, its head)
    d.limb(body, Q([(2.6, 27.0), (0.0, 33.0), (2.0, 38.6)]), [5.2 * k, 4.4 * k, 4.2 * k])
    d.oval(body, *P(3.4, 41.6), 6.2 * k, 5.4 * k)
    # (its beak open: the upper bill swung up on its hinge, so that the bombs crammed into its pouch stick their tails
    #  out of the gap between the bills, under the upper bill and never behind it; the inside of its mouth, the bombs,
    #  the pouch slung full under the lower bill, the lower bill, then the upper bill)
    hinge = (8.0, 39.0)
    lo_tip = (31.0, 24.4)
    ca, sa = math.cos(math.radians(gape)), math.sin(math.radians(gape))

    def up(u, v):                                        # (a point of the upper bill, swung up on the hinge)
        du, dv = u - hinge[0], v - hinge[1]
        return (hinge[0] + du * ca - dv * sa, hinge[1] + du * sa + dv * ca)
    d.shape("art_red", Q([hinge, up(14.0, 36.0), up(24.0, 32.8), up(33.6, 31.2), (32.6, 26.6), lo_tip, (19.4, 32.2)]), ink=w0 * 0.6)
    for i, (tail, nose) in enumerate(BOMBS_68):
        if i < load:                                     # (their tails out between the bills, fins and all)
            (tu, tv), (nu, nv) = tail, nose
            ln = math.hypot(tu - nu, tv - nv) / 1.04
            bomb(d, *P(nu, nv), ln * k, turn=math.degrees(math.atan2(nv - tv, nu - tu)))
    d.fill(pouch, Q([hinge, (14.0, 35.0), (20.0, 31.0), (26.0, 27.4), lo_tip, (31.6, 20.0), (29.6, 14.6), (24.6, 10.6), (17.4, 9.4),
                     (11.0, 11.6), (6.6, 17.4), (5.2, 25.0), (5.6, 32.6)]), steps=5)
    d.line("art_white", Q([(9.0, 30.0), (9.4, 21.0), (13.0, 14.6)]), 0.9 * k)                     # (its pouch's shine)
    d.limb(bill, Q([hinge, (19.4, 31.8), lo_tip]), [1.0 * k, 0.95 * k, 0.8 * k], ink=w0 * 0.8)
    d.fill(bill, Q([(5.0, 43.8)] + [up(u, v) for u, v in ((14.0, 41.0), (24.0, 37.0), (33.0, 34.2), (35.8, 33.8), (37.0, 31.6), (36.2, 29.0),
                                                           (34.8, 29.6), (33.8, 31.0), (24.0, 32.8), (14.0, 36.0), (8.0, 38.4))]), steps=3)
    d.line("art_ink", Q([up(10.0, 40.4), up(22.0, 36.0), up(32.0, 32.6)]), 0.3 * k)          # (its ridge)
    # (its eye: pleased with itself, the lid half down)
    d.oval("art_cream", *P(4.8, 42.6), 1.5 * k, 1.4 * k, ink=w0 * 0.6)
    d.oval("art_ink", *P(5.3, 42.3), 0.7 * k, 0.75 * k, ink=0.0)
    d.fill(body, Q([(3.0, 42.8), (4.8, 43.4), (6.6, 42.8), (6.6, 44.2), (3.0, 44.2)]), ink=0.0, steps=2)
    d.line("art_ink", Q([(3.1, 42.8), (4.8, 43.3), (6.5, 42.8)]), 0.35 * k, taper=False)
    return P


def piling(d, x, y, w, h, paint="art_lbrown", top="art_tan"):
    """A wharf piling, its top's middle at (x, y), w wide and h tall, its grain up it."""
    d.shape(paint, [(x - w / 2, y - h), (x + w / 2, y - h), (x + w / 2, y), (x - w / 2, y)])
    for u, t in ((-0.22, 0.75), (0.18, 0.5)):
        d.line("art_ink", [(x + u * w, y - h * 0.1), (x + u * w * 1.05, y - h * (0.1 + t * 0.5)), (x + u * w, y - h * (0.1 + t))], w * 0.035)
    d.oval(top, x, y, w / 2, w * 0.14, ink=d.ink * 0.8)


# ---- 69: a heart, an envelope -----------------------------------------------------------------------------------------------
def heart(d, x, y, size, paint="art_red", turn=0.0, ink=None):
    """A heart, its middle at (x, y), `size` across."""
    k = size / 32.0
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    pts = []
    for i in range(48):
        t = 2 * math.pi * i / 48
        u = 16 * math.sin(t) ** 3
        v = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t) + 2.5
        pts.append((x + (u * c - v * s_) * k, y + (u * s_ + v * c) * k))
    return d.shape(paint, pts, ink=ink)


def envelope(d, x, y, w, h, turn=0.0, paint="art_white", flap="art_cream", seal="art_red"):
    """An envelope seen from its back, its middle at (x, y), w by h, turned `turn` degrees: the folds of its bottom and
    side flaps, its top flap closed down over them, and the seal on the flap's point: a red heart. No writing."""
    P = _frame(x, y, 1.0, 1, turn)
    hw, hh = w / 2, h / 2
    d.shape(paint, [P(-hw, -hh), P(hw, -hh), P(hw, hh), P(-hw, hh)])
    mid = (0.0, -hh * 0.12)
    for sx in (-1, 1):
        d.line("art_ink", [P(sx * (hw - 0.8), -hh + 0.8), P(sx * hw * 0.35, -hh * 0.45), P(*mid)], 0.42, taper=False)
    tip = (0.0, -hh * 0.3)
    d.shape(flap, [P(-hw, hh), P(hw, hh), P(*tip)])
    heart(d, *P(tip[0], tip[1] + h * 0.02), w * 0.24, seal, turn=turn)
    d.line("art_lred", [P(-w * 0.07, tip[1] + h * 0.12), P(-w * 0.04, tip[1] + h * 0.16)], w * 0.018)
    return P


# ---- 70: a tin mug --------------------------------------------------------------------------------------------------------
def tin_mug(d, x, y, h, paint="art_steel", rim="art_lsteel", coffee="art_brown"):
    """A tin camp mug, the middle of its foot at (x, y), h tall, its handle to the right: its body tapering a little to
    its foot, its rolled rim, the coffee in it, a dent and its shine. Returns its frame (u, v in h's)."""
    P = _frame(x, y, h)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    w0 = d.ink
    d.limb(paint, Q([(0.38, 0.78), (0.66, 0.74), (0.74, 0.5), (0.62, 0.26), (0.36, 0.22)]), [h * 0.085] * 5)
    d.limb("art_dark", Q([(0.42, 0.68), (0.6, 0.65), (0.64, 0.5), (0.56, 0.34), (0.42, 0.32)]), [h * 0.03] * 5, ink=0.0)
    d.shape(paint, Q([(-0.46, 0.9), (-0.4, 0.0), (0.4, 0.0), (0.46, 0.9)]))                            # (straight tin sides, a little wider at the rim)
    d.oval(rim, *P(0.0, 0.9), 0.48 * h, 0.12 * h)
    d.oval(coffee, *P(0.0, 0.9), 0.4 * h, 0.085 * h, ink=w0 * 0.6)
    d.line("art_lsteel", Q([(-0.33, 0.8), (-0.31, 0.45), (-0.295, 0.14)]), h * 0.05)                     # (its shine)
    d.line("art_ink", Q([(-0.404, 0.065), (0.404, 0.065)]), h * 0.015, taper=False)                     # (its foot's seam, straight)
    return P


def steam(d, x, y, h, n=3, gap=5.0, paint="art_white", lean=0.0):
    """Steam rising from (x, y): n wavy strokes, h tall, `gap` apart, leaning `lean` grid units at their tops."""
    for i in range(n):
        off = (i - (n - 1) / 2) * gap
        hh = h * (1.0 - 0.18 * abs(off) / max(gap * n, 1.0))
        pts = [(x + off + lean * t + 1.3 * math.sin(t * 2 * math.pi * 1.25 + i), y + hh * t) for t in (0.0, 0.2, 0.4, 0.6, 0.8, 1.0)]
        d.line(paint, pts, 1.4)


# ---- 71: a bowling ball, its pins -----------------------------------------------------------------------------------------
PIN = [(0.0, 0.1), (0.04, 0.135), (0.14, 0.185), (0.26, 0.195), (0.38, 0.17), (0.5, 0.115), (0.6, 0.075), (0.68, 0.08), (0.78, 0.105),
       (0.86, 0.112), (0.93, 0.09), (0.98, 0.05), (1.0, 0.0)]


def pin(d, x, y, h, turn=90.0, paint="art_white", stripe="art_red"):
    """A bowling pin, its middle at (x, y), h tall, its head toward `turn` degrees: chubby, with the two red stripes
    round its neck."""
    a = math.radians(turn - 90.0)
    c, s_ = math.cos(a), math.sin(a)

    def P(u, v):
        v -= 0.5
        return (x + (u * c - v * s_) * h, y + (u * s_ + v * c) * h)
    out = [P(w, v) for v, w in PIN] + [P(-w, v) for v, w in reversed(PIN)]
    d.fill(paint, out, steps=4)
    for v in (0.6, 0.67):
        wv = 0.08
        d.line(stripe, [P(-wv, v), P(wv, v)], h * 0.04, taper=False)
    d.line("art_lgrey", [P(0.09, 0.08), P(0.13, 0.24), P(0.09, 0.42)], h * 0.03)


def bowling_ball(d, x, y, r, paint="art_black", turn=0.0, shine="art_lgrey"):
    """A bowling ball, its middle at (x, y), r round: its three holes toward `turn` degrees, and its shine, a bold flat
    crescent painted along its upper-left quarter (a highlight as a sign painter put one: no glow)."""
    d.oval(paint, x, y, r, r)
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    for u, v, rr in ((0.18, 0.42, 0.11), (0.42, 0.3, 0.11), (0.18, -0.02, 0.12)):
        hx, hy = x + (u * c - v * s_) * r, y + (u * s_ + v * c) * r
        d.oval("art_ink", hx, hy, rr * r, rr * r * 1.05, ink=d.ink * 0.7)
        d.line("art_grey", ellipse(hx, hy, rr * r * 0.8, rr * r * 0.8, 0, 8, 200, 340), r * 0.03)
    a0, a1, n = 92.0, 182.0, 16                           # (the crescent: its outer edge along the ball's, fat in its middle, pointed at its ends)
    outer, inner = [], []
    for i in range(n + 1):
        t = i / n
        a = math.radians(a0 + (a1 - a0) * t)
        th = 0.27 * math.sin(math.pi * t) ** 0.8
        outer.append((x + r * 0.86 * math.cos(a), y + r * 0.86 * math.sin(a)))
        inner.append((x + r * (0.86 - th) * math.cos(a), y + r * (0.86 - th) * math.sin(a)))
    d.shape(shine, outer + list(reversed(inner[1:-1])), ink=0.0)


# ---- 72: a bridge tower, an umbrella --------------------------------------------------------------------------------------
TIERS = ((0.0, 10.0, 5.3), (11.0, 19.0, 4.3), (20.0, 27.5, 3.4))    # (each tier of a leg: its foot, its top, its outer edge)


def bridge_tower(d, x, y, k=1.0, paint="art_orange", reach=30.0, drop=23.0, sag=2.2):
    """The top of a great suspension bridge's tower rising out of the fog, the middle of its foot at (x, y) down in the
    fog, 27.5 k tall to its top, about three times as tall as it is wide: two legs stepping in at each tier, the recess
    line up the middle of each tier of each leg, a portal strut across between the legs at each step and one at the
    top, a saddle on each leg's top, and from each saddle the main cable sweeping out and down in a long sagging curve,
    `reach` out and `drop` down, into the fog. Returns its frame (her feet go on P(0, 27.5))."""
    P = _frame(x, y, k)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    inner = 1.2
    for sx in (-1, 1):
        leg = []
        for v0, v1, out in TIERS:
            leg += [(out, v0), (out, v1)]
        leg = [(inner, TIERS[0][0])] + leg + [(inner, TIERS[-1][1])]
        d.shape(paint, Q([(sx * u, v) for u, v in leg]))
        for v0, v1, out in TIERS:                        # (the recess up each tier)
            um = (inner + out) / 2
            d.line("art_ink", Q([(sx * um, v0 + 1.6), (sx * um, v1 - 1.4)]), 0.4 * k, taper=False)
    for v0, top, out in TIERS:                           # (a portal strut at each step, and one at the top)
        d.shape(paint, Q([(-inner - 0.2, top - 2.2), (inner + 0.2, top - 2.2), (inner + 0.2, top), (-inner - 0.2, top)]))
    for sx in (-1, 1):                                   # (the saddles on the legs' tops)
        d.fill(paint, Q([(sx * 1.5, 27.3), (sx * 1.6, 28.6), (sx * 2.5, 29.4), (sx * 3.4, 28.6), (sx * 3.5, 27.3)]), steps=3)
    for sx in (-1, 1):                                   # (the main cables: off each saddle, out and down into the fog)
        pts = []
        for i in range(13):
            t = i / 12.0
            pts.append((sx * (2.9 + reach * t), 28.4 - drop * (1.0 - (1.0 - t) ** sag)))
        d.line("art_ink", Q(pts), 2.5 * k, taper=False)
        d.line(paint, Q(pts), 1.35 * k, taper=False)
    return P


def fog(d, lumps, paint="art_white", shade="art_lgrey"):
    """A bank or a wisp of fog: lumps run together, (x, y, r) or (x, y, rx, ry), a grey line low along it."""
    d.blobs(paint, lumps)
    xs = [c[0] for c in lumps]
    lo = min(c[1] - (c[3] if len(c) > 3 else c[2]) * 0.45 for c in lumps)
    d.line(shade, [(min(xs) + 2.0, lo + 1.2), ((min(xs) + max(xs)) / 2, lo + 0.6), (max(xs) - 2.0, lo + 1.2)], 1.2)


def umbrella(d, hx, hy, r, turn=0.0, paint="art_red", shaft="art_dark"):
    """An open umbrella held at (hx, hy) on its shaft, its canopy r round at the top of the shaft, tipped `turn` degrees
    from upright: a scalloped canopy of four panels and its ribs, the ferrule at its top, and below the hand the crook of
    its handle."""
    a = math.radians(turn)
    c, s_ = math.cos(a), math.sin(a)

    def P(u, v):
        return (hx + u * c - v * s_, hy + u * s_ + v * c)
    L = r * 1.25                                         # (from her hand up to the canopy's rim)
    _wire(d, shaft, [P(0.0, -r * 0.2), P(0.0, L + r * 0.5)], r * 0.07)
    d.line("art_ink", [P(0.0, -r * 0.18), P(0.06 * r, -r * 0.4), P(0.24 * r, -r * 0.48), P(0.38 * r, -r * 0.36)], r * 0.16, taper=False)
    d.line("art_brown", [P(0.0, -r * 0.18), P(0.06 * r, -r * 0.4), P(0.24 * r, -r * 0.48), P(0.38 * r, -r * 0.36)], r * 0.09, taper=False)
    top = [P(r * math.cos(t), L + r * 0.62 * math.sin(t)) for t in (math.pi * i / 16 for i in range(17))]
    tips = [-r, -r / 2, 0.0, r / 2, r]
    low = []
    for i in range(4):
        u0, u1 = tips[i], tips[i + 1]
        for j in range(6):
            t = j / 6.0
            low.append(P(u0 + (u1 - u0) * t, L + 0.13 * r * math.sin(math.pi * t)))
    low.append(P(r, L))
    d.shape(paint, top + low)
    for u in tips[1:-1]:
        d.line("art_ink", [P(0.0, L + r * 0.62), P(u * 0.55, L + r * 0.5), P(u, L)], r * 0.035)
    d.fill(shaft, [P(-r * 0.04, L + r * 0.6), P(0.0, L + r * 0.82), P(r * 0.04, L + r * 0.6)], ink=d.ink * 0.6, steps=1)
    return P


# ---- 73: a snow-woman, her icicle -----------------------------------------------------------------------------------------
def icicle(d, x, y, long, turn=90.0, paint="art_sky"):
    """An icicle held by its thick end at (x, y), `long`, its point toward `turn` degrees: a long cone rippled where it
    froze drop by drop, its top broken off rough, its shine down it."""
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):                                         # u along it, v across
        return (x + u * c - v * s_, y + u * s_ + v * c)
    w = long * 0.12
    side_a, side_b = [], []
    for i in range(13):
        t = i / 12.0
        ww = w * (1.0 - t) ** 0.85 * (1.0 + 0.1 * math.sin(t * math.pi * 5.0))
        side_a.append(P(t * long, ww))
        side_b.append(P(t * long, -ww * (1.0 + 0.06 * math.cos(t * math.pi * 4.0))))
    top = [P(-0.03 * long, -w * 0.6), P(-0.06 * long, -w * 0.1), P(-0.03 * long, w * 0.25), P(-0.07 * long, w * 0.7)]
    d.fill(paint, side_a[:-1] + [side_a[-1]] + list(reversed(side_b[:-1])) + top, steps=3)
    d.line("art_white", [P(0.08 * long, w * 0.42), P(0.36 * long, w * 0.32), P(0.66 * long, w * 0.14)], w * 0.3)
    d.line("art_lblue", [P(0.12 * long, -w * 0.4), P(0.4 * long, -w * 0.3)], w * 0.16)
    return P


def snow_woman(d, x, y, h, scarf="art_red", muffs="art_red", shade="art_lgrey", fore=True):
    """A snow-woman, the middle of her foot at (x, y), h tall to her crown: three balls of snow on a drift, her coal
    eyes with their lashes, her red lips, a short carrot of a nose, three coal buttons, her earmuffs on their band, her
    scarf wound round her neck with one end flying, and her twig arms (`fore` False leaves her raised arm to the
    painting, to draw over what she holds). Returns her frame and where her hands are."""
    k = h / 60.0
    P = _frame(x, y, k)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    w0 = d.ink
    balls = ((0.0, 14.0, 14.0), (0.4, 34.0, 10.4), (0.8, 49.6, 8.2))
    hands = {"fore": P(16.4, 47.6), "aft": P(-14.6, 25.4)}
    # (her twig arms, behind her, the near one raised)
    if fore:
        d.limb("art_brown", Q([(8.0, 37.0), (12.8, 42.0), (16.4, 47.6)]), [0.7 * k, 0.6 * k, 0.5 * k], ink=w0 * 0.7)
    d.limb("art_brown", Q([(-8.0, 33.0), (-13.0, 31.0), (-14.6, 25.4)]), [0.7 * k, 0.6 * k, 0.5 * k], ink=w0 * 0.7)
    d.limb("art_brown", Q([(-12.8, 30.8), (-16.0, 32.6)]), [0.4 * k, 0.3 * k], ink=w0 * 0.6)
    # (the drift, her three balls, each shaded low on its far side)
    d.blobs("art_white", [(P(-12.0, 1.8)[0], P(0, 1.8)[1], 5.0 * k), (P(0.0, 1.0)[0], P(0, 1.0)[1], 6.4 * k), (P(13.0, 1.6)[0], P(0, 1.6)[1], 5.4 * k),
                          (P(0.0, -0.6)[0], P(0, -0.6)[1], 20.0 * k, 3.0 * k)])
    for bx, by, br in balls:
        d.oval("art_white", *P(bx, by), br * k, br * k)
        d.shape(shade, crescent(*P(bx, by), br * k, *P(bx - br * 0.16, by + br * 0.18), br * k * 0.94), ink=0.0)
    for v in (37.6, 33.6, 29.6):
        d.oval("art_ink", *P(0.6, v), 0.9 * k, 0.9 * k, ink=0.0)
    # (her face)
    hx, hy = 0.8, 49.6
    for ex in (-2.6, 2.8):
        d.oval("art_ink", *P(hx + ex, hy + 1.4), 0.95 * k, 1.15 * k, ink=0.0)
        for j, (du, dv) in enumerate(((-0.6, 1.0), (0.2, 1.3), (1.0, 1.1))):
            d.line("art_ink", Q([(hx + ex + du * 0.6, hy + 1.4 + dv * 0.8), (hx + ex + du, hy + 1.4 + dv * 1.6)]), 0.3 * k, taper=False)
    d.oval("art_pink", *P(hx - 4.6, hy - 1.0), 1.4 * k, 0.9 * k, ink=0.0)
    d.oval("art_pink", *P(hx + 4.8, hy - 1.0), 1.4 * k, 0.9 * k, ink=0.0)
    d.fill("art_orange", Q([(hx - 0.2, hy + 0.6), (hx + 4.6, hy - 0.4), (hx - 0.2, hy - 1.0)]), ink=w0 * 0.7, steps=2)
    d.fill("art_red", Q([(hx - 1.6, hy - 3.4), (hx - 0.6, hy - 2.9), (hx, hy - 3.1), (hx + 0.6, hy - 2.9), (hx + 1.6, hy - 3.4), (hx, hy - 4.2)]), ink=0.0, steps=3)
    # (her earmuffs on their band)
    band = [P(hx + 8.6 * math.cos(math.radians(t)), hy + 8.6 * math.sin(math.radians(t))) for t in range(10, 171, 16)]
    d.line("art_ink", band, 1.5 * k, taper=False)
    d.line(muffs, band, 0.8 * k, taper=False)
    for sx in (-1, 1):
        d.oval(muffs, *P(hx + sx * 8.0, hy + 0.6), 2.6 * k, 3.2 * k)
        d.line("art_lred", Q([(hx + sx * 8.0 - 0.8, hy + 2.2), (hx + sx * 8.0 + 0.4, hy + 2.8)]), 0.5 * k)
    # (her scarf: round her neck, one end hanging, one flying)
    d.fill(scarf, Q([(-8.0, 43.8), (-4.0, 42.6), (0.8, 42.2), (5.6, 42.6), (9.4, 43.8), (9.2, 40.8), (5.6, 39.4), (0.8, 39.0), (-4.0, 39.4), (-8.2, 40.8)]), steps=4)
    d.fill(scarf, Q([(-5.4, 41.0), (-3.0, 40.0), (-3.6, 33.0), (-2.6, 30.6), (-6.8, 31.0), (-6.4, 34.0)]), steps=3)
    d.fill(scarf, Q([(-7.6, 42.6), (-13.0, 44.6), (-19.0, 43.6), (-24.0, 46.0), (-22.0, 42.0), (-17.0, 40.4), (-12.0, 40.6), (-7.6, 40.0)]), steps=4)
    return P, hands


# ---- the paintings --------------------------------------------------------------------------------------------------------
@painting(67)
def p67(d, a):   # a woman in factory coveralls with a lunch pail punching a time clock
    wear = {"top": "art_khaki", "trousers": "art_khaki", "sleeves": "rolled", "sleeve": "art_khaki", "collar": "art_khaki",
            "belt": "art_brown", "buckle": "art_gold", "buttons": "art_brown", "boots": "art_brown", "stitch": "art_brown"}
    fx, fy, fh = 44.0, 2.0, 80.0
    probe = lady(Drawing(1), fx, fy, fh, -1, pose="stride", arms=("offer", "down"), wear=wear, hair="rolls", hair_paint="art_auburn")
    (hx, hy), _ = probe.hand["fore"]
    k = (hy - fy + 0.6) / 41.0                           # (the card's top edge just under her fingers)
    C = time_clock(d, hx + 0.4, fy, k)
    time_card(d, C)
    F = lady(d, fx, fy, fh, -1, pose="stride", arms=("offer", "down"), wear=wear, hair="rolls", hair_paint="art_auburn")
    (ax_, ay_), _ = F.hand["aft"]
    lunch_pail(d, ax_, ay_ + 0.2)
    for (px, py), _ in (F.hand["aft"], F.hand["fore"]):
        d.oval("art_skin", px, py, 0.155 * F.k, 0.13 * F.k, ink=d.ink * 0.7)
    names(d, a, (104, 44), 14.5, lines=["Swing", "Shift", "Shirley"], gap=1.36, indent=4)


@painting(68)
def p68(d, a):   # a pelican with a beak full of bombs
    piling(d, 22, 14, 14, 26)
    pelican(d, 22, 14, 60)
    names(d, a, (104, 26.4), 15, lines=split2(a["name"]), indent=8, turn=5)


@painting(69)
def p69(d, a):   # an envelope with wings and a heart on its seal
    wing(d, (28, 41), 98, 40, 12, "art_lgrey", shade="art_grey", spread=-30)
    wing(d, (22, 40), 114, 46, 13, "art_white", spread=-30)
    speed(d, 10, 20, 10, n=3, gap=4.6, paint="art_white", w=1.1)
    envelope(d, 36, 26, 48, 32, turn=8.0)
    names(d, a, (100, 46), 15, lines=["Letter", "from", "Home"], gap=1.36, indent=3)


@painting(70)
def p70(d, a):   # a steaming tin mug with wings
    wing(d, (17, 37), 116, 38, 13, "art_white", spread=-30)
    wing(d, (47, 37), 64, 38, 13, "art_white", spread=30)
    tin_mug(d, 32, 4, 44)
    steam(d, 32, 52, 32, n=3, gap=6.8)
    names(d, a, (102, 42), 15, lines=["Cup", "of", "Joe"], gap=1.36, indent=4)


@painting(71)
def p71(d, a):   # a bowling ball sending three pins flying
    speed(d, 6, 44, 16, n=3, gap=6.0, paint="art_white", w=1.3)
    # (one burst off the ball's front: three pins thrown up and away together, each tumbling its own way)
    pin(d, 61.5, 60, 24, turn=-24)
    pin(d, 61.5, 43.5, 24, turn=204)
    bowling_ball(d, 26, 44, 18, turn=-10)
    pin(d, 50.5, 50, 24, turn=76)
    names(d, a, (46, 11), 14.5)


@painting(72)
def p72(d, a):   # a woman in a raincoat with an umbrella on a bridge tower in the fog
    T = bridge_tower(d, 34, 1, 1.0)
    fog(d, [(4, 3.6, 4.0), (11, 3.2, 4.4), (18, 3.8, 4.6), (25, 4.2, 4.8), (31, 4.6, 4.4), (37.5, 4.4, 4.4), (44, 4.0, 4.8), (51, 3.6, 4.6),
            (58, 3.2, 4.4), (65, 3.6, 4.0), (34, 1.4, 33, 2.0)])
    sx_, sy_ = T(-0.6, 27.5)
    F = lady(d, sx_, sy_, 55, 1, pose="together", arms=("chest", "hip"),
             wear={"top": "art_yellow", "coat": "art_yellow", "belt": "art_yellow", "buckle": "art_gold", "buttons": "art_dark", "sleeves": "long",
                   "shoes": "art_black", "lapel": "art_yellow"},
             hair="rolls", hair_paint="art_black")
    (hx, hy), _ = F.hand["fore"]
    umbrella(d, hx, hy, 10.2, turn=-16, paint="art_orange")
    d.oval("art_skin", hx, hy, 0.155 * F.k, 0.13 * F.k, ink=d.ink * 0.7)
    names(d, a, (110, 46), 15, lines=["Golden", "Gate", "Gertie"], gap=1.36, indent=2)


@painting(73)
def p73(d, a):   # a snow-woman in earmuffs and a scarf holding an icicle
    k = 70 / 60.0
    S = _frame(28, 2, k)                                  # (her frame, as snow_woman() makes it)
    hx, hy = S(16.4, 47.6)
    I = icicle(d, hx + 0.4, hy + 3.0, 34, turn=-88)      # (held up by its broken top, its point hanging as it grew)
    # (her raised twig arm, over the front of the icicle, one piece from her body to her grip: its two twig fingers
    #  grow from its end and curl round the icicle's near edge; every part's dark edge first and then every fill, so
    #  that they join with no line between them)
    grip = I(3.2, -3.4)                                   # (on the icicle's near edge, a little below its broken top)
    parts = [([S(8.0, 37.0), S(10.4, 43.0), (grip[0] - 2.2, grip[1] - 2.3), grip], [0.7 * k, 0.62 * k, 0.56 * k, 0.5 * k]),
             ([grip, (grip[0] + 1.8, grip[1] + 1.4), (grip[0] + 4.0, grip[1] + 1.2), (grip[0] + 5.2, grip[1] - 0.2)], [0.5, 0.46, 0.4, 0.26]),
             ([grip, (grip[0] + 2.0, grip[1] - 0.6), (grip[0] + 4.2, grip[1] - 1.2), (grip[0] + 5.0, grip[1] - 2.6)], [0.5, 0.46, 0.4, 0.26])]
    for pts, rr in parts:
        d.limb(None, pts, rr, ink=d.ink * 0.7 / 0.55)
    for pts, rr in parts:
        d.limb("art_brown", pts, rr, ink=0.0)
    snow_woman(d, 28, 2, 70, fore=False)
    names(d, a, (98, 38), 15, lines=split2(a["name"]), indent=10, turn=5)
