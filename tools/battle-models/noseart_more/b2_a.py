"""Batch 2, numbers 25 to 31 (painter b2_a): a tumbleweed, a bashful cub, a pie on the sill, a dizzy compass, a winged
boxcar, the Sunday sun and a riveter walking home. Each picture is drawn from flat shapes with noseart's Drawing API
and its kit; the helpers below (a tumbleweed, a question mark, a compass, a pie, a boxcar, a sun, a rivet gun...) are
built the same way, for these paintings."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402


# ---- helpers ------------------------------------------------------------------------------------------------------------
def _arc(cx, cy, rx, ry, a0, a1, n=14):
    """Points along an ellipse from a0 to a1 degrees."""
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * k / n)))
            for k in range(n + 1)]


def _lady(d, x, y, h, s=1, extra_arms=None, **kw):
    """lady(), with arms of our own (name: (elbow, hand), as noseart's ARMS gives them) lent to her for this one call."""
    added = [k for k in (extra_arms or {}) if k not in ARMS]
    for k in added:
        ARMS[k] = extra_arms[k]
    try:
        return lady(d, x, y, h, s, **kw)
    finally:
        for k in added:
            ARMS.pop(k, None)


def moon(cx, cy, r, ix, iy, ir, n=120):
    """A crescent: the part of the circle (cx, cy, r) outside the circle (ix, iy, ir), its two arcs joined end to start
    (so that a thin one never crosses itself)."""
    outer = [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    inner = [(ix + ir * math.cos(2 * math.pi * k / n), iy + ir * math.sin(2 * math.pi * k / n)) for k in range(n)]

    def run(pts, keep):
        m = [keep(p) for p in pts]
        start = next(i for i in range(n) if m[i] and not m[i - 1])
        pts, m = pts[start:] + pts[:start], m[start:] + m[:start]
        return [p for p, k in zip(pts, m) if k]

    a_ = run(outer, lambda p: math.hypot(p[0] - ix, p[1] - iy) > ir)
    b_ = run(inner, lambda p: math.hypot(p[0] - cx, p[1] - cy) < r)
    if math.hypot(a_[-1][0] - b_[0][0], a_[-1][1] - b_[0][1]) > math.hypot(a_[-1][0] - b_[-1][0], a_[-1][1] - b_[-1][1]):
        b_ = b_[::-1]
    return a_ + b_


def question(d, x, y, h, turn=0.0, paint="art_white", ink=None):
    """A question mark as a sign painter brushes it (a shape, not a letter): its foot at (x, y), `h` tall, leaning
    `turn` degrees; a fat hook ending in a short stem, and its dot."""
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        return (x + (u * c - v * s_) * h, y + (u * s_ + v * c) * h)

    hook = [(-0.27, 0.66), (-0.22, 0.86), (-0.02, 0.97), (0.22, 0.9), (0.3, 0.72), (0.2, 0.55), (0.03, 0.44), (0.0, 0.28)]
    d.limb(paint, [P(u, v) for u, v in hook], [h * 0.085, h * 0.09, h * 0.095, h * 0.095, h * 0.095, h * 0.09, h * 0.085, h * 0.08], ink=ink)
    d.oval(paint, *P(0.0, 0.07), h * 0.095, h * 0.095, ink=ink)


# ---- 25: the tumbleweed --------------------------------------------------------------------------------------------------
def tumbleweed(d, x, y, r, body="art_tan", twig="art_hair", gap="art_olive"):
    """A tumbleweed: a ball of dry twigs. Its broken twig ends bristle bold all round it, some forked; long twigs wound
    round it like wire at every tilt sweep right across it and run out over its edge among them; and here and there
    the tangle is open, so that her own olive drab shows through (`gap`: art_olive, which a cruder hand leaves bare)."""
    # (the twig ends, all round it, their roots under the ball: about 1.5 thick at the root, 4 to 6 long, one in three forked)
    for k in range(14):
        t = math.radians(9.0 + 360.0 * k / 14 + (4.0, -5.0, 2.0, -3.0, 5.0, -2.0, 3.0)[k % 7])
        ln = (5.4, 4.2, 6.0, 4.6, 5.0, 4.0, 5.8)[k % 7]
        bend = (0.12, -0.1, 0.08, -0.14, 0.1, -0.06, 0.14)[k % 7]
        root = (x + r * 0.82 * math.cos(t), y + r * 0.82 * math.sin(t))
        mid = (x + (r + ln * 0.45) * math.cos(t + bend * 0.5), y + (r + ln * 0.45) * math.sin(t + bend * 0.5))
        tip = (x + (r + ln) * math.cos(t + bend), y + (r + ln) * math.sin(t + bend))
        d.limb(body, [root, mid, tip], [0.78, 0.6, 0.3], ink=d.ink * 0.85)
        if k % 3 == 1:                                       # (a forked one: a spur off it, from about its middle)
            sp = t - bend * 2.6 - 0.32
            fork = (x + (r + ln * 0.95) * math.cos(sp), y + (r + ln * 0.95) * math.sin(sp))
            d.limb(body, [lerp(root, tip, 0.5), fork], [0.55, 0.26], ink=d.ink * 0.8)
    # (the ball, a little uneven)
    pts = [(x + r * (1.0 + 0.025 * math.sin(5 * u + 0.6) + 0.015 * math.sin(9 * u)) * math.cos(u),
            y + r * (1.0 + 0.025 * math.sin(5 * u + 0.6) + 0.015 * math.sin(9 * u)) * math.sin(u)) for u in (2 * math.pi * k / 40 for k in range(40))]
    d.fill(body, pts, steps=3)
    # (where the tangle is open: her olive drab through it)
    for cu, cv, w_, h_, tw in ((-0.5, 0.56, 0.2, 0.11, 28), (0.3, 0.66, 0.17, 0.1, -12), (-0.7, -0.28, 0.11, 0.18, -20),
                               (-0.18, -0.68, 0.2, 0.1, 8), (0.6, -0.66, 0.13, 0.09, 40)):
        cx, cy = x + cu * r, y + cv * r
        d.fill(gap, turned([(cx - w_ * r, cy), (cx - w_ * r * 0.3, cy + h_ * r), (cx + w_ * r * 0.5, cy + h_ * r * 0.8), (cx + w_ * r, cy - h_ * r * 0.1),
                            (cx + w_ * r * 0.2, cy - h_ * r), (cx - w_ * r * 0.6, cy - h_ * r * 0.7)], cx, cy, tw), ink=d.ink * 0.55, steps=3)
    # (long twigs wound round it like wire: great circles at every tilt, each over most of its round, out over its edge)
    for turn_, flat, a0, a1 in ((18, 0.34, 150, 420), (72, 0.58, -40, 220), (128, 0.26, 20, 300), (-28, 0.72, 100, 330),
                                (96, 0.16, 180, 430), (160, 0.62, 200, 440)):
        d.line(twig, ellipse(x, y, r * 1.1, r * 1.1 * flat, turn_, 26, a0, a1), 1.2)
    return pts


def _trail(p0, c, p2, n=14):
    """Points along the quadratic curve from p0 to p2 drawn toward c."""
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * c[0] + t * t * p2[0], (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * c[1] + t * t * p2[1])
            for t in (k / n for k in range(n + 1))]


@painting(25)
def p25(d, a):   # a tumbleweed in goggles bouncing over a falling bomb
    x, y, r = 33, 62, 19.0
    # the bomb under its hop, falling nose down and a little tilted, below and behind it
    bomb(d, 19.5, 5.0, 26, turn=-80, paint="art_black", band="art_yellow")
    # its bounce: one bold trail, rising from behind the bomb, arching over it and running on to the ball
    p0, c, p2 = (-6.0, 4.0), (-2.0, 64.0), (x - 0.62 * r, y - 0.55 * r)
    for off in (-2.7, 0.0, 2.7):
        pts = _trail(p0, c, p2)
        out = []
        for i, (px, py) in enumerate(pts):
            qx, qy = pts[min(i + 1, len(pts) - 1)][0] - pts[max(i - 1, 0)][0], pts[min(i + 1, len(pts) - 1)][1] - pts[max(i - 1, 0)][1]
            ln = math.hypot(qx, qy) or 1.0
            out.append((px - qy / ln * off, py + qx / ln * off))
        d.line("art_white", out, 1.4)
    tumbleweed(d, x, y, r)
    # its goggles: the strap round it to each lens's black rim, the pale lenses, its eyes small behind them; a grin under
    lenses = ((0.14, 0.19, 0.2), (0.62, 0.22, 0.155))         # (middle, pale radius: in r; the far one smaller)
    rim = 0.1
    sw = r * 0.13
    (u0, v0, l0), (u1, v1, l1) = lenses
    d.line("art_black", [(x - 1.0 * r, y + 0.13 * r), (x - 0.6 * r, y + 0.09 * r), (x + (u0 - l0 - rim * 0.5) * r, y + v0 * r)], sw, taper=False)
    d.line("art_black", [(x + (u0 + l0) * r, y + v0 * r), (x + (u1 - l1) * r, y + v1 * r)], sw * 0.8, taper=False)
    d.line("art_black", [(x + (u1 + l1 + rim * 0.5) * r, y + v1 * r), (x + 0.86 * r, y + 0.25 * r), (x + 0.99 * r, y + 0.27 * r)], sw, taper=False)
    for lu, lv, lr in lenses:
        d.oval("art_black", x + lu * r, y + lv * r, (lr + rim) * r * 0.94, (lr + rim) * r, ink=d.ink * 0.7)
        d.oval("art_white", x + lu * r, y + lv * r, lr * r * 0.92, lr * r, ink=d.ink * 0.35)
        d.oval("art_black", x + (lu + 0.05) * r, y + (lv - 0.03) * r, lr * r * 0.24, lr * r * 0.28, ink=0.0)
    d.fill("art_black", [(x + 0.04 * r, y - 0.3 * r), (x + 0.36 * r, y - 0.38 * r), (x + 0.7 * r, y - 0.26 * r), (x + 0.4 * r, y - 0.5 * r)], ink=d.ink * 0.6, steps=3)
    names(d, a, (97, 47), 14.5, lines=["Tulsa", "Tumbleweed"], gap=1.4, indent=-6)


# ---- 26: the bashful cub ---------------------------------------------------------------------------------------------------
@painting(26)
def p26(d, a):   # a bashful bear cub in a flying helmet pushed back, one paw on his blushing cheek
    coat, pale = "art_tan", "art_cream"
    x = 30
    d.fill(coat, [(x - 17, 44), (x - 21, 30), (x - 19, 14), (x - 12, 7), (x, 5.5), (x + 12, 7), (x + 19, 14), (x + 21, 30), (x + 17, 44), (x + 8, 52), (x - 8, 52)])
    d.oval(pale, x, 25, 10.5, 12.5, ink=d.ink * 0.6)            # (his pale belly)
    for sx in (-1, 1):                                       # (his feet, soles out)
        fx = x + sx * 13.0
        d.oval(coat, fx, 9.4, 6.8, 5.8)
        d.oval(pale, fx, 8.2, 3.7, 3.1, ink=d.ink * 0.6)
        for t in (-1, 0, 1):
            d.oval(pale, fx + t * 2.7, 12.4 - abs(t) * 0.6, 1.2, 1.1, ink=d.ink * 0.5)
    # (the aft arm: its paw resting shyly on his belly)
    d.limb(coat, [(x + 18.5, 40.0), (x + 19.5, 31.0), (x + 9.0, 27.0)], [3.6, 3.4, 3.2])
    d.oval(coat, x + 8.6, 27.4, 3.8, 3.4)
    for t in (-1, 0, 1):
        d.line("art_ink", [(x + 6.2, 27.6 + t * 1.5), (x + 5.0, 27.6 + t * 1.6)], 0.45)
    # (his head, under a leather flying helmet pushed back on it, the goggles up)
    d.oval(coat, x, 61, 14.2, 12.6)
    helmet(d, x, 65.0, 28.5, 1, paint="art_brown", view="front", part="cap", inside=None)
    # (his muzzle and nose; a small shy smile)
    d.oval(pale, x, 54.4, 6.4, 4.6)
    d.oval("art_ink", x, 57.0, 2.2, 1.5, ink=0.0)
    d.line("art_ink", [(x, 55.6), (x, 53.6)], 0.5, taper=False)
    d.line("art_ink", [(x - 2.6, 52.8), (x, 51.6), (x + 2.6, 52.8)], 0.55)
    # (his eyes glancing shyly aside and down, brows up in the middle; cheeks blushing deep)
    for sx in (-1, 1):
        ex = x + sx * 5.6
        d.oval("art_ink", ex - 1.2, 60.6, 1.5, 1.9, ink=0.0)
        d.oval("art_white", ex - 0.7, 61.4, 0.48, 0.52, ink=0.0)
        d.line("art_ink", [(ex + sx * 2.4, 63.6), (ex, 64.5), (ex - sx * 2.0, 65.6)], 0.6)
        d.oval("art_red" if not d.crude else "art_pink", x + sx * 9.6, 55.6, 3.4, 2.0, ink=0.0)
    # (the fore arm: up, his paw laid on his blushing cheek)
    d.limb(coat, [(x - 18.5, 40.5), (x - 23.0, 47.0), (x - 13.6, 54.4)], [3.6, 3.4, 3.2])
    d.oval(coat, x - 12.4, 55.0, 4.0, 3.6)
    for t in (-1, 0, 1):
        d.line("art_ink", [(x - 9.8, 55.6 + t * 1.5), (x - 8.6, 55.8 + t * 1.6)], 0.45)
    names(d, a, (96, 44), 15, lines=["Bashful", "Bear"], gap=1.42, indent=8)


# ---- 27: the pie on the sill -------------------------------------------------------------------------------------------------
@painting(27)
def p27(d, a):   # a steaming apple pie cooling on a windowsill
    # the window: a white frame, its panes dark (a room seen from outdoors), and its sill
    d.shape("art_white", [(4, 21), (4, 86), (56, 86), (56, 21)])
    d.shape("art_navy", [(7.5, 24), (7.5, 82.5), (52.5, 82.5), (52.5, 24)], ink=d.ink * 0.8)
    d.shape("art_white", [(28.6, 54), (28.6, 82.5), (31.4, 82.5), (31.4, 54)], ink=d.ink * 0.6)       # (two lights over one)
    d.shape("art_white", [(7.5, 52), (52.5, 52), (52.5, 54.8), (7.5, 54.8)], ink=d.ink * 0.6)
    d.shape("art_white", [(0, 22.5), (60, 22.5), (62.5, 17.5), (-2.5, 17.5)])
    d.shape("art_lgrey", [(-2.5, 17.5), (62.5, 17.5), (62.5, 13.5), (-2.5, 13.5)])
    # its steam, rising over the dark panes
    for sx, ph, n in ((-10, 0.0, 6), (0, 1.6, 3), (10, 3.1, 6)):
        pts = [(30 + sx + 2.2 * math.sin(ph + 1.25 * k), 37 + 6.2 * k) for k in range(n)]
        d.line("art_white", pts, 1.8)
    # the pie: its enamel dish, its domed crust, the crimped edge, three cuts
    d.fill("art_red", _arc(30, 25.6, 24.5, 4.4, 180, 360, 16) + _arc(30, 19.4, 19.5, 2.4, 0, -180, 12), steps=2)
    crust = _arc(30, 25.6, 24.0, 4.2, 182, 358, 16) + [(53.4, 27.5), (51, 31.5), (44, 35.2), (30, 37.0), (16, 35.2), (9, 31.5), (6.6, 27.5)]
    d.fill("art_tan", crust, steps=3)
    for k in range(11):
        t = 192 + 156 * k / 10
        px, py = 30 + 23.4 * math.cos(math.radians(t)), 25.6 + 4.2 * math.sin(math.radians(t))
        d.oval("art_tan", px, py, 2.6, 1.9, ink=d.ink * 0.6)
    for cx, cy, tw in ((21, 30.6, 12), (30, 32.0, 0), (39, 30.6, -12)):          # (its three cuts, along the dome)
        d.fill("art_yellow", turned([(cx - 2.2, cy - 0.3), (cx, cy + 0.6), (cx + 2.2, cy - 0.3), (cx, cy - 0.9)], cx, cy, tw), ink=d.ink * 0.7, steps=3)
    d.line("art_cream", [(11, 30.5), (17, 34.0), (25, 35.6)], 1.0)
    names(d, a, (100, 50), 14, lines=a["name"].split(), gap=1.36)


# ---- 28: the dizzy compass ----------------------------------------------------------------------------------------------------
def _bent_half(x, y, r, start, droop, curl, long=0.8, bend_at=0.64, wide=0.11, step=0.25):
    """One half of a compass needle, bent: a flat lozenge's half, `wide` r across each side of its middle at the hub,
    narrowing to a point; it leaves the hub at `start` degrees, droops `droop` degrees over the first `bend_at` of its
    `long` (in r) and curls `curl` degrees over the rest (negative: clockwise). Returns its outline."""
    L = long * r
    s1 = bend_at * L
    n = max(8, int(L / step))
    th, px, py = math.radians(start), x, y
    mid, ang = [(px, py)], [th]
    for i in range(1, n + 1):
        s = L * i / n
        k = (math.radians(droop) / s1) if s <= s1 else (math.radians(curl) / (L - s1))
        th += k * (L / n)
        px, py = px + math.cos(th) * L / n, py + math.sin(th) * L / n
        mid.append((px, py))
        ang.append(th)
    left, right = [], []
    for i, ((px, py), t) in enumerate(zip(mid, ang)):
        w = wide * r * (1.0 - i / n) ** 0.85
        left.append((px - math.sin(t) * w, py + math.cos(t) * w))
        right.append((px + math.sin(t) * w, py - math.cos(t) * w))
    return left[:-1] + [mid[-1]] + list(reversed(right[:-1]))


def compass(d, x, y, r, case="art_yellow", face="art_white", north="art_red", south="art_black"):
    """A navigator's compass, its middle at (x, y), `r` its case's radius: a slim brass case, a white card with a small
    fine rose, and its needle (a flat lozenge, red north and black south, broad at the hub and sharp at both tips)
    broken and dizzy: kinked at the hub so that its north half droops over sideways, the last third of it curled, and
    bold spin strokes round the hub."""
    d.oval(case, x, y + r * 1.02, r * 0.16, r * 0.13)          # (its lug)
    d.oval(case, x, y, r, r)
    d.oval(face, x, y, r * 0.86, r * 0.86, ink=d.ink * 0.9)
    star(d, x, y, r * 0.48, case, points=4, inner=0.18, turn=45, ink=d.ink * 0.5)
    for a0, a1, r0, r1 in ((36, 152, 0.7, 0.56), (254, 316, 0.7, 0.6)):        # (spinning: two bold strokes round the hub)
        n = 14
        d.line("art_ink", [(x + r * (r0 + (r1 - r0) * k / n) * math.cos(math.radians(a0 + (a1 - a0) * k / n)),
                            y + r * (r0 + (r1 - r0) * k / n) * math.sin(math.radians(a0 + (a1 - a0) * k / n))) for k in range(n + 1)], 1.5)
    # the needle: its south half straight, its north half kinked at the hub, drooping over, its last third curled
    t = math.radians(232)
    w, L = 0.105 * r, 0.74 * r
    d.shape(south, [(x - math.sin(t) * w, y + math.cos(t) * w), (x + math.cos(t) * L, y + math.sin(t) * L), (x + math.sin(t) * w, y - math.cos(t) * w),
                    (x - math.cos(t) * w * 0.8, y - math.sin(t) * w * 0.8)])
    d.shape(north, _bent_half(x, y, r, -12, 0, -140, long=0.84, bend_at=0.68, wide=0.105))
    d.oval(case, x, y, r * 0.075, r * 0.075, ink=d.ink * 0.6)


@painting(28)
def p28(d, a):   # a compass with a bent, dizzy needle and three question marks
    compass(d, 40, 57, 20)
    for qx, qy, qh, qt in ((70, 44, 15, -14), (84, 58, 21, 8), (104, 47, 15, -6)):
        question(d, qx, qy, qh, qt, "art_white")
    names(d, a, (70, 13), 14, lines=["Navigator's", "Nightmare"], gap=1.36)


# ---- 29: the night freight ------------------------------------------------------------------------------------------------------
def boxcar(d, x, y, L, H, body="art_lred", door="art_red", roof="art_black"):
    """A boxcar side on, its body's lower left corner at (x, y), L long and H tall: its roof, its sliding door, its
    ribs, its underframe and its two trucks."""
    d.shape(roof, [(x - 0.8, y + H - 0.3), (x - 0.8, y + H + 1.6), (x + L + 0.8, y + H + 1.6), (x + L + 0.8, y + H - 0.3)])
    d.shape(body, [(x, y), (x, y + H), (x + L, y + H), (x + L, y)])
    for t in (0.1, 0.2, 0.3, 0.7, 0.8, 0.9):
        d.line("art_ink", [(x + L * t, y + 0.6), (x + L * t, y + H - 0.6)], 0.42, taper=False)
    dx0, dx1 = x + L * 0.37, x + L * 0.63
    d.shape(door, [(dx0, y + 0.8), (dx0, y + H - 1.2), (dx1, y + H - 1.2), (dx1, y + 0.8)], ink=d.ink * 0.8)
    d.line("art_black", [(dx0 - 2.0, y + H - 0.7), (dx1 + 2.0, y + H - 0.7)], 0.7, taper=False)
    d.line("art_lsteel", [(dx1 - 1.6, y + H * 0.3), (dx1 - 1.6, y + H * 0.62)], 0.7, taper=False)
    d.shape(roof, [(x + 1.0, y - 2.0), (x + 1.0, y), (x + L - 1.0, y), (x + L - 1.0, y - 2.0)], ink=d.ink * 0.8)
    for tx in (x + L * 0.2, x + L * 0.8):
        d.shape("art_dark", [(tx - 5.0, y - 4.6), (tx - 5.0, y - 1.6), (tx + 5.0, y - 1.6), (tx + 5.0, y - 4.6)], ink=d.ink * 0.7)
        for wx in (tx - 3.0, tx + 3.0):
            d.oval("art_black", wx, y - 4.4, 2.6, 2.6)
            d.oval("art_lsteel", wx, y - 4.4, 0.9, 0.9, ink=d.ink * 0.5)


@painting(29)
def p29(d, a):   # a boxcar with wings under a quarter moon
    d.fill("art_navy", [(4, 44), (6, 74), (18, 86), (36, 88), (52, 82), (60, 64), (60, 34), (54, 14), (36, 6), (18, 8), (6, 20)], ink=d.ink * 1.2, steps=4)
    d.shape("art_yellow", moon(42, 70, 10.5, 47.5, 74.5, 9.6))
    for tx, ty, tr in ((14, 74, 1.9), (54, 50, 1.5), (24, 60, 1.3)):
        twinkle(d, tx, ty, tr)
    wing(d, (34, 44), 122, 26, 10, "art_lgrey", shade="art_grey", spread=-30)
    wing(d, (30, 42), 140, 32, 11.5, "art_white", spread=-30)
    boxcar(d, 8, 26, 44, 16)
    names(d, a, (100, 46), 14.5, lines=["Night", "Freight"], gap=1.4, indent=6)


# ---- 30: the Sunday sun -----------------------------------------------------------------------------------------------------------
def sun(d, x, y, r, rays=16, long=0.5, short=0.26, paint="art_yellow", start=0.0, reach=None):
    """A sun as a sign painter does it: its disc and a full crown of broad pointed rays round it, long and short by
    turns (the first one short, pointing `start` degrees up from forward). `reach`, if given, caps how far a ray may
    stand out sideways from the middle (so that the sun keeps to its width)."""
    for k in range(rays):
        t = math.radians(start) + 2 * math.pi * k / rays
        ln, w = (short, math.radians(10.0)) if k % 2 == 0 else (long, math.radians(12.0))
        far = r * (1.04 + ln)
        if reach is not None and abs(far * math.cos(t)) > reach:
            far = reach / abs(math.cos(t))
        d.shape(paint, [(x + r * 0.9 * math.cos(t - w), y + r * 0.9 * math.sin(t - w)), (x + far * math.cos(t), y + far * math.sin(t)),
                        (x + r * 0.9 * math.cos(t + w), y + r * 0.9 * math.sin(t + w))])
    d.oval(paint, x, y, r, r)


@painting(30)
def p30(d, a):   # a smiling sun in a Sunday hat reading the newspaper
    x, y, r = 28, 46, 19.0
    sun(d, x, y, r, start=-6.0, reach=26.5)                     # (its full crown of rays: they show round its hat and its paper)
    q = r / 17.0
    # its face: eyes lowered to the paper, brows up, a broad smile, warm cheeks
    for sx in (-1, 1):
        ex, ey = x + sx * 5.4 * q, y + 3.4 * q
        d.oval("art_white", ex, ey, 2.5 * q, 3.1 * q)
        d.oval("art_ink", ex + sx * 0.2 * q, ey - 1.3 * q, 1.55 * q, 1.75 * q, ink=0.0)
        d.oval("art_white", ex + (sx * 0.2 + 0.55) * q, ey - 0.75 * q, 0.48 * q, 0.5 * q, ink=0.0)
        d.line("art_ink", [(ex - sx * 2.6 * q, ey + 4.0 * q), (ex, ey + 5.2 * q), (ex + sx * 2.7 * q, ey + 4.4 * q)], 0.62 * q)
        d.oval("art_orange", x + sx * 10.2 * q, y - 1.0 * q, 2.5 * q, 1.7 * q, ink=0.0)
    d.fill("art_ink", _arc(x, y - 0.8 * q, 7.0 * q, 5.2 * q, 194, 346, 14) + _arc(x, y - 2.6 * q, 6.6 * q, 1.9 * q, 338, 202, 8), ink=0.0, steps=2)
    d.fill("art_red", _arc(x, y - 5.0 * q, 3.4 * q, 1.3 * q, 200, 340, 8) + _arc(x, y - 5.0 * q, 3.4 * q, 1.1 * q, 340, 200, 8)[1:-1], ink=0.0, steps=2)
    # its Sunday hat: a navy fedora with a tall pinched crown and a red band, tipped a little back
    hx, hy, tw = x + 1.9, y + r - 1.6, -6                     # (its crown's middle line runs up through the sun's middle)
    brim = [(hx - 15.5, hy + 1.2), (hx - 9.5, hy - 0.6), (hx, hy - 0.9), (hx + 9.5, hy - 0.6), (hx + 15.5, hy + 1.6), (hx + 10.5, hy + 2.8), (hx, hy + 2.6),
            (hx - 10.5, hy + 2.8)]
    crown = [(hx - 8.0, hy + 1.8), (hx - 8.3, hy + 9.5), (hx - 7.4, hy + 15.8), (hx - 5.4, hy + 19.4), (hx - 2.0, hy + 20.2), (hx, hy + 18.6),
             (hx + 2.0, hy + 20.2), (hx + 5.4, hy + 19.4), (hx + 7.4, hy + 15.8), (hx + 8.3, hy + 9.5), (hx + 8.0, hy + 1.8)]
    d.fill("art_navy", turned(crown, hx, hy, tw), steps=4)
    d.shape("art_red", turned([(hx - 8.1, hy + 2.0), (hx - 8.3, hy + 5.4), (hx + 8.3, hy + 5.4), (hx + 8.1, hy + 2.0)], hx, hy, tw), ink=d.ink * 0.7)
    d.fill("art_navy", turned(brim, hx, hy, tw), steps=4)
    d.line("art_ink", turned([(hx - 3.6, hy + 18.2), (hx - 1.1, hy + 16.6), (hx, hy + 14.8)], hx, hy, tw), 0.6)
    d.line("art_ink", turned([(hx + 3.6, hy + 18.2), (hx + 1.1, hy + 16.6), (hx, hy + 14.8)], hx, hy, tw), 0.6)
    # the Sunday paper, a broadsheet held open under its smile: two pages, the fold, the columns' grey and one photograph,
    # and not a word on it
    px0, px1, py0, py1 = x - 23.5, x + 23.5, y - r - 25.0, y - 9.2
    d.fill("art_white", [(px0, py0), (px0 - 0.6, (py0 + py1) / 2), (px0 - 0.4, py1), (x - 12, py1 + 0.9), (x - 1.2, py1 + 0.2), (x, py1 - 0.7),
                         (x + 1.2, py1 + 0.2), (x + 12, py1 + 0.9), (px1 + 0.4, py1), (px1 + 0.6, (py0 + py1) / 2), (px1, py0), (x + 12, py0 + 0.6),
                         (x, py0 - 0.4), (x - 12, py0 + 0.6)], steps=2)
    top, foot = py1 - 3.6, py0 + 3.2
    for side in (-1, 1):                                        # (each page: its columns, set in grey)
        for k, (c0, c1) in enumerate(((2.8, 7.6), (9.4, 14.2), (16.0, 20.8))):
            hi = top - (9.6 if side < 0 and k > 0 else 0.0)     # (the photograph over two columns of the left page)
            d.shape("art_lgrey", [(x + side * c0, foot), (x + side * c0, hi), (x + side * c1, hi), (x + side * c1, foot)], ink=0.0)
    d.shape("art_grey", [(x - 20.8, top - 7.6), (x - 20.8, top), (x - 9.4, top), (x - 9.4, top - 7.6)], ink=d.ink * 0.5)
    d.line("art_grey", [(x, py0 + 0.6), (x, py1 - 1.2)], 0.9, taper=False)
    for sx in (-1, 1):                                          # (its hands, holding the paper up)
        d.oval("art_yellow", x + sx * 23.2, py1 - 2.8, 2.7, 3.4)
        d.oval("art_yellow", x + sx * 21.4, py1 - 1.3, 1.3, 1.7, ink=d.ink * 0.7)
    names(d, a, (101, 44), 14, lines=split2(a["name"]), gap=1.4, indent=8)


# ---- 31: Allegheny Annie ------------------------------------------------------------------------------------------------------------
def rivet_gun(d, F, at, tilt=42.0, rake=-70.0, grip_len=0.27, barrel=2.36, body="art_grey", grip="art_black", steel="art_lsteel"):
    """A pneumatic rivet gun carried on a figure's aft shoulder, its nose back (aft) and `tilt` degrees up: the square
    valve housing at its back, a straight barrel, the beehive retainer's three bold rings at its nose and the set in
    them, and the short pistol grip under the housing's back end, its top at `at` (heads, in her frame), raked `rake`
    degrees and no longer than her hand is wide. Returns the grip's middle (heads), where her hand goes."""
    R = F.r
    c, s_ = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
    ax, ay, nx, ny = -c, s_, s_, c                      # (along it toward its nose; across it, up)

    def G(u, v):
        return F.P(at[0] + u * ax + v * nx, at[1] + u * ay + v * ny)

    turn = math.degrees(math.atan2(ay, ax * F.s))
    gx, gy = math.cos(math.radians(rake)), math.sin(math.radians(rake))
    foot = (at[0] + gx * grip_len, at[1] + gy * grip_len)
    d.limb(grip, F.Ps([(at[0] - gx * 0.08, at[1] - gy * 0.08), lerp(at, foot, 0.5), foot]), [R(0.12), R(0.115), R(0.11)], ink=d.ink * 0.8)
    d.shape(body, [G(0.4, 0.04), G(barrel, 0.04), G(barrel, 0.38), G(0.4, 0.38)])
    d.shape(body, [G(u, v) for u, v in ((-0.1, 0.05), (-0.05, 0.0), (0.39, 0.0), (0.44, 0.05), (0.44, 0.37), (0.39, 0.42), (-0.05, 0.42),
                                        (-0.1, 0.37))])
    d.limb(body, [G(barrel - 0.04, 0.21), G(barrel + 0.44, 0.21)], [R(0.1), R(0.085)], ink=d.ink * 0.6)     # (the set's shank in the coil)
    for i, rr in enumerate((0.27, 0.23, 0.19)):            # (the beehive: a coil narrowing to the set)
        d.oval(steel, *G(barrel + 0.08 + 0.145 * i, 0.21), R(0.09), R(rr), turn=turn, ink=d.ink * 0.75)
    d.limb(steel, [G(barrel + 0.42, 0.21), G(barrel + 0.6, 0.21)], [R(0.075), R(0.07)], ink=d.ink * 0.75)
    d.oval(steel, *G(barrel + 0.61, 0.21), R(0.045), R(0.095), turn=turn, ink=d.ink * 0.7)
    return lerp(at, foot, 0.5)


def rivet_gun_short(d, F, at, tilt=8.0, rake=-100.0, grip_len=0.27, body="art_grey", grip="art_black", steel="art_lsteel", k=1.25):
    """A pneumatic rivet gun held at her chest, its nose forward (the way she faces) and `tilt` degrees up, about her
    forearm's length (1.1 heads) from its back to the set: the square valve housing at its back, a short straight
    barrel, the beehive retainer's three rings at its nose and the set in them; the pistol grip under the housing's
    back end, its top at `at` (heads, in her frame), raked `rake` degrees. Returns the grip's middle and its foot."""
    c, s_ = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))

    def R(v):                                            # (its sizes, scaled by k)
        return F.r(v * k)

    def G(u, v):                                         # (u along it toward its nose, v across it, up)
        u, v = u * k, v * k
        return F.P(at[0] + u * c - v * s_, at[1] + u * s_ + v * c)

    turn = math.degrees(math.atan2(s_, c * F.s))
    gx, gy = math.cos(math.radians(rake)), math.sin(math.radians(rake))
    foot = (at[0] + gx * grip_len, at[1] + gy * grip_len)
    d.limb(grip, F.Ps([(at[0] - gx * 0.06, at[1] - gy * 0.06), lerp(at, foot, 0.5), foot]), [F.r(0.13), F.r(0.125), F.r(0.12)], ink=d.ink * 0.8)
    d.shape(body, [G(u, v) for u, v in ((-0.08, 0.03), (-0.03, -0.02), (0.36, -0.02), (0.4, 0.03), (0.4, 0.36), (0.36, 0.41),
                                        (-0.03, 0.41), (-0.08, 0.36))])
    d.shape(body, [G(0.4, 0.09), G(0.8, 0.09), G(0.8, 0.31), G(0.4, 0.31)])
    for i, rr in enumerate((0.2, 0.17, 0.14)):           # (the beehive: a coil narrowing to the set)
        d.oval(steel, *G(0.84 + 0.1 * i, 0.2), R(0.065), R(rr), turn=turn, ink=d.ink * 0.75)
    d.limb(steel, [G(1.02, 0.2), G(1.14, 0.2)], [R(0.06), R(0.055)], ink=d.ink * 0.75)
    d.oval(steel, *G(1.15, 0.2), R(0.035), R(0.075), turn=turn, ink=d.ink * 0.7)
    return lerp(at, foot, 0.5), foot


@painting(31)
def p31(d, a):   # a woman in steelworker's overalls and a plain bandana walking with a short rivet gun held at her chest
    x, y, h = 26, 4, 70
    # the girder she walks along: an I-beam in red lead, the edges of its flanges, a splice's rivets
    d.shape("art_orange", [(-6, -2.6), (-6, 4), (60, 4), (60, -2.6)])
    for fy in (2.4, -1.0):
        d.line("art_ink", [(-5.8, fy), (59.8, fy)], 0.45, taper=False)
    for rx in (-2.4, 1.6, 50.4, 54.4):
        d.oval("art_orange", rx, 0.7, 0.75, 0.75, ink=d.ink * 0.5)
    # (in heads, in her frame) the gun's grip top at her chest, in front of her; her aft arm folded, the elbow at her
    # side and the forearm across her body to the grip
    at, rake, glen = (0.3, 5.36), -100.0, 0.27
    gx, gy = math.cos(math.radians(rake)), math.sin(math.radians(rake))
    hc = (at[0] + gx * glen / 2, at[1] + gy * glen / 2)
    sh, E = (-SHOULDER[0], SHOULDER[1]), (-0.72, 4.72)
    ln = math.hypot(hc[0] - E[0], hc[1] - E[1])
    ux, uy = (hc[0] - E[0]) / ln, (hc[1] - E[1]) / ln
    Hn = (hc[0] - ux * 0.12, hc[1] - uy * 0.12)
    # (lady()'s one-stroke arm cannot fold this sharply, so she is lent a short straight arm along her upper arm, and
    # the folded arm is painted over it below in two strokes, upper arm and forearm)
    tuck = lerp(sh, E, 0.55)
    carry = {"_b2a_tuck": ((sh[0] - lerp(sh, tuck, 0.5)[0], lerp(sh, tuck, 0.5)[1] - sh[1]), (sh[0] - tuck[0], tuck[1] - sh[1]))}
    F = _lady(d, x, y, h, 1, extra_arms=carry, pose="stride", arms=("down", "_b2a_tuck"),
              wear={"top": "art_white", "trousers": "art_lblue", "bib": "art_lblue", "sleeves": "long", "boots": "art_brown", "stitch": "art_tan",
                    "buttons": "art_lsteel"},
              hair="bob", hair_paint="art_auburn")
    R = F.r
    # her bandana: plain cloth over the top and back of her head, tied at the back, its two ends fluttering behind
    band = "art_yellow"
    d.fill(band, F.Ps([(0.5, 6.95), (0.56, 7.22), (0.47, 7.56), (0.22, 7.75), (-0.12, 7.77), (-0.44, 7.6), (-0.64, 7.3), (-0.72, 6.95),
                       (-0.66, 6.62), (-0.5, 6.66), (-0.44, 6.9), (-0.4, 7.14), (-0.14, 7.3), (0.16, 7.3), (0.42, 7.16)]), ink=d.ink * 0.7, steps=4)
    for tail in ([(-0.72, 6.92), (-1.0, 7.0), (-1.04, 6.88), (-0.74, 6.86)], [(-0.72, 6.86), (-0.96, 6.78), (-0.9, 6.7), (-0.7, 6.8)]):
        d.fill(band, F.Ps(tail), ink=d.ink * 0.6, steps=3)
    d.oval(band, *F.P(-0.7, 6.86), R(0.085), R(0.075), ink=d.ink * 0.6)
    d.line("art_ink", F.Ps([(-0.34, 7.46), (-0.06, 7.6), (0.26, 7.56)]), R(0.03))
    # the gun's air hose, from the grip's foot hanging in a loose loop down past her hip, and on behind her along the girder
    foot = (at[0] + gx * glen, at[1] + gy * glen)
    d.limb("art_black", F.Ps([foot, (foot[0] - 0.04, foot[1] - 0.4), (foot[0] - 0.2, foot[1] - 0.95), (-0.3, 3.35), (-0.9, 3.0), (-1.35, 2.3),
                              (-1.55, 1.4), (-1.75, 0.6), (-2.3, 0.08), (-2.9, 0.02)]), [R(0.09)] * 10, ink=d.ink * 0.8)
    # her aft arm, folded: the elbow down at her side, the forearm across her body (the sleeve as lady() paints one)
    d.limb("art_white", F.Ps([sh, lerp(sh, E, 0.5), E]), [R(0.27), R(0.235), R(0.19)], ink=d.ink)
    d.limb("art_white", F.Ps([E, lerp(E, Hn, 0.45), Hn]), [R(0.19), R(0.18), R(0.15)], ink=d.ink)
    cu = lerp(E, Hn, 0.84)
    d.line("art_ink", F.Ps([(cu[0] - uy * 0.17, cu[1] + ux * 0.17), (cu[0] + uy * 0.17, cu[1] - ux * 0.17)]), R(0.03), taper=False)
    # the rivet gun at her chest, and her hand round its grip (as lady() paints a hand)
    rivet_gun_short(d, F, at, rake=rake, grip_len=glen)
    ang = math.degrees(math.atan2(uy, ux * F.s * F.wide))
    d.oval("art_skin", *F.P(*hc), R(0.155), R(0.13), turn=ang, ink=d.ink * 0.7)
    d.oval("art_skin", *F.P(hc[0] + uy * 0.1, hc[1] - ux * 0.1), R(0.075), R(0.05), turn=ang + 50 * F.s, ink=d.ink * 0.7 * 0.8)
    names(d, a, (96, 44), 14.5, lines=split2(a["name"]), gap=1.42, indent=8)
