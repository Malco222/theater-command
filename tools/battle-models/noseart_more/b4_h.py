"""Batch 4, numbers 190 to 196 (painter b4_h): a wooden nickel stamped with a bomber, a woman in a long gown spinning
her upright bass, a picture house's marquee lit up over two small bombers, a trolley car climbing a steep hill with its
bell going, a tin wind-up bomber with its key in its back, a child in pyjamas at the window wishing on one bright star
and a sign painter up his ladder, brush raised beside the aircraft's own name. Flat shapes with noseart's Drawing API
and the figure kit; each name set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b2_f import ladder  # noqa: E402
from noseart_more.b2_h import plain_bomber  # noqa: E402
from noseart_more.b3_a import _T  # noqa: E402


# ---- 190: Wooden Nickel --------------------------------------------------------------------------------------------------------------
@painting(190)
def p190(d, a):   # a wooden nickel, its grain and a knot showing, a bomber stamped in its middle, a ring of beads round its rim
    ink = d.ink
    x, y, r = 28.0, 30.0, 27.0
    d.oval("art_lbrown", x + 1.2, y - 2.6, r, r * 0.94, turn=-8.0, ink=ink)          # (its edge, its thickness)
    d.oval("art_tan", x, y, r, r * 0.94, turn=-8.0, ink=ink)
    d.line("art_lbrown", ellipse(x, y, r * 0.86, r * 0.81, -8.0, 40), 0.9, taper=False)
    for i, (dv, bend) in enumerate(((-18.0, 2.0), (-11.0, -1.6), (-4.0, 1.2), (6.0, -2.0), (13.0, 1.4), (19.0, -1.0))):
        half = math.sqrt(max(0.0, (r * 0.8) ** 2 - dv ** 2))
        d.line("art_lbrown", [(x - half, y + dv), (x - half * 0.3, y + dv + bend), (x + half * 0.4, y + dv - bend * 0.6), (x + half, y + dv + bend * 0.4)], 0.35, taper=True)
    d.oval("art_lbrown", x - 13.0, y - 12.0, 2.6, 1.6, turn=-20.0, ink=ink * 0.6)                  # (a knot in it)
    d.oval("art_brown", x - 13.0, y - 12.0, 1.1, 0.7, turn=-20.0, ink=0.0)
    for i in range(30):                                   # (the beads stamped round its rim)
        t = math.radians(i * 12.0)
        bx, by = r * 0.93 * math.cos(t), r * 0.93 * 0.94 * math.sin(t)
        c, s_ = math.cos(math.radians(-8.0)), math.sin(math.radians(-8.0))
        d.oval("art_brown", x + bx * c - by * s_, y + bx * s_ + by * c, 0.7, 0.7, ink=0.0)
    # (the bomber stamped in its middle, nosing up and away; a star either side)
    small_bomber(d, x + 10.0, y + 9.0, 30.0, paint="art_brown", turn=42.0)
    star(d, x - 14.0, y + 8.0, 3.0, "art_brown", ink=0.0)
    star(d, x + 13.0, y - 11.0, 3.0, "art_brown", ink=0.0)
    d.line("art_white", ellipse(x - 6.0, y + 9.0, r * 0.7, r * 0.6, -8.0, 30, 110.0, 150.0), 0.6, taper=True)
    place_name(d, a, lines=["Wooden", "Nickel"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 191: Bass Fiddle Fay ------------------------------------------------------------------------------------------------------------
def upright_bass(d, x, y, h, turn=0.0, wood="art_auburn", board="art_black", string="art_white"):
    """An upright bass standing on its endpin at (x, y), `h` tall to its scroll, leaned `turn` degrees: its round
    lower bout, its waist, its sloping shoulders, its f-holes, bridge and tailpiece, its long black fingerboard and four
    strings, its scroll and pegs."""
    G0 = _T(x, y, turn)
    G = lambda u, v: G0(u * h, v * h)                    # noqa: E731
    Gs = lambda pts: [G(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    side = [(0.02, 0.03), (0.13, 0.06), (0.2, 0.14), (0.2, 0.24), (0.14, 0.32), (0.15, 0.39), (0.15, 0.46), (0.1, 0.53), (0.035, 0.585)]
    body = side + [(-u, v) for (u, v) in reversed(side)]
    d.line("art_lsteel", Gs([(0.0, -0.03), (0.0, 0.04)]), 0.012 * h, taper=False)
    d.fill(wood, Gs(body), ink=ink, steps=4)
    d.fill(board, Gs([(-0.022, 0.25), (0.022, 0.25), (0.028, 0.92), (-0.028, 0.92)]), ink=ink * 0.6, steps=1)
    d.fill(board, Gs([(-0.04, 0.06), (0.04, 0.06), (0.025, 0.17), (-0.025, 0.17)]), ink=ink * 0.6, steps=1)     # (the tailpiece)
    for su in (-1.0, 1.0):                                 # (the f-holes)
        d.line(board, Gs([(su * 0.075, 0.37), (su * 0.06, 0.34), (su * 0.07, 0.3), (su * 0.08, 0.25), (su * 0.065, 0.22)]), 0.008 * h)
    d.fill("art_tan", Gs([(-0.05, 0.215), (0.05, 0.215), (0.04, 0.24), (-0.04, 0.24)]), ink=ink * 0.5, steps=1)    # (the bridge)
    for i in range(4):
        u0 = -0.015 + 0.01 * i
        d.line(string, Gs([(u0, 0.16), (u0 * 0.9, 0.9)]), 0.0025 * h, taper=False)
    d.oval(wood, *G(0.012, 0.95), 0.032 * h, 0.036 * h, ink=ink)                             # (the scroll and its pegs)
    d.line(board, Gs([(0.01, 0.95), (0.02, 0.96), (0.016, 0.94)]), 0.006 * h)
    for v in (0.885, 0.915):
        for su in (-1.0, 1.0):
            d.oval("art_lsteel", *G(su * 0.05, v), 0.012 * h, 0.008 * h, ink=ink * 0.5)
    return G


@painting(191)
def p191(d, a):   # a woman in a long gown swinging her upright bass round on its endpin, the bass spinning, her free arm flung out
    ink = d.ink
    x, y, h = 12.0, 2.0, 60.0
    kw = dict(pose="stand", arms=("lift", "wave"), wear={"top": "art_red", "hem": "long", "sleeves": "short", "shoes": "art_red"},
              hair="rolls", hair_paint="art_blond")
    F0 = lady(Drawing(1), x, y, h, 1, **kw)
    (hx, hy), _ = F0.hand["fore"]
    # (the bass beside her, leaning in to her hand, spinning: its arcs round it)
    bh, turn = 66.0, 9.0
    bx = hx + 0.8 * bh * math.sin(math.radians(turn)) - 0.5
    F = lady(d, x, y, h, 1, **kw)
    G = upright_bass(d, bx, y, bh, turn=turn)
    d.oval("art_skin", hx, hy, 1.3, 1.5, ink=ink * 0.7)
    for (v, rx, a0, a1) in ((0.18, 0.3, 200.0, 345.0), (0.36, 0.25, 195.0, 340.0)):
        cx, cy = G(0.0, v)
        d.line("art_white", ellipse(cx, cy, rx * bh, 0.05 * bh, turn, 30, a0, a1), 0.7, taper=True)
    for (u, v) in ((-0.3, 0.26), (0.3, 0.3), (-0.26, 0.44)):
        cx, cy = G(u, v)
        d.line("art_white", [(cx, cy), (cx + (2.0 if u > 0 else -2.0), cy - 0.8)], 0.5, taper=True)
    place_name(d, a, lines=["Bass", "Fiddle", "Fay"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 192: Double Feature -------------------------------------------------------------------------------------------------------------
@painting(192)
def p192(d, a):   # a picture house's marquee, all its bulbs lit, two small bombers up on its board where the titles go
    ink = d.ink
    W = 96.0                                           # (the marquee's width: as wide as the name under it)
    # (the house front under it: its doors either side, its ticket booth)
    d.shape("art_cream", [(8.0, 0.0), (W - 8.0, 0.0), (W - 8.0, 22.0), (8.0, 22.0)], ink=ink)
    for u in (12.0, 24.0, W - 32.0, W - 20.0):
        d.shape("art_red", [(u, 0.0), (u + 8.0, 0.0), (u + 8.0, 15.0), (u, 15.0)], ink=ink * 0.8)
        d.line("art_gold", [(u + 4.0, 0.6), (u + 4.0, 14.4)], 0.4, taper=False)
    cx = W / 2
    d.fill("art_gold", [(cx - 6.0, 0.0), (cx + 6.0, 0.0), (cx + 6.0, 14.0), (cx + 4.0, 18.0), (cx, 19.4), (cx - 4.0, 18.0), (cx - 6.0, 14.0)], ink=ink, steps=3)
    d.fill("art_yellow", [(cx - 3.4, 9.0), (cx + 3.4, 9.0), (cx + 3.4, 14.0), (cx, 16.6), (cx - 3.4, 14.0)], ink=ink * 0.6, steps=2)
    # (the marquee: its board, lit, the two bombers on it; its crown; bulbs all round)
    d.shape("art_red", [(0.0, 22.0), (W, 22.0), (W, 44.0), (0.0, 44.0)], ink=ink)
    d.shape("art_cream", [(4.0, 25.0), (W - 4.0, 25.0), (W - 4.0, 41.0), (4.0, 41.0)], ink=ink * 0.7)
    for u in (9.0, W / 2 + 5.0):
        plain_bomber(d, u, 29.0, 34.0, tilt=4.0, skin="art_dark", glass="art_lgrey", tyre="art_dark", strut="art_dark")
    d.fill("art_red", [(10.0, 44.0), (W - 10.0, 44.0), (W - 20.0, 54.0), (20.0, 54.0)], ink=ink, steps=1)
    star(d, cx, 49.0, 3.6, "art_yellow", ink=ink * 0.6)
    n = int(W / 2.9)
    bulbs = [(1.8 + (W - 3.6) * i / (n - 1), 23.6) for i in range(n)] + [(1.8 + (W - 3.6) * i / (n - 1), 42.4) for i in range(n)] + \
            [(1.8, 23.6 + 3.1 * j) for j in range(1, 6)] + [(W - 1.8, 23.6 + 3.1 * j) for j in range(1, 6)] + \
            [(21.0 + (W - 42.0) * i / 15, 52.4) for i in range(16)]
    for (u, v) in bulbs:
        d.oval("art_yellow", u, v, 0.9, 0.9, ink=0.0)
        d.oval("art_white", u - 0.2, v + 0.2, 0.3, 0.3, ink=0.0)
    for (u, v, a0) in ((-1.0, 33.0, 180.0), (W + 1.0, 33.0, 0.0), (cx, 56.0, 90.0)):            # (their glow)
        for k in range(3):
            ang = math.radians(a0 + (k - 1) * 30.0)
            d.line("art_yellow", [(u + math.cos(ang) * 1.0, v + math.sin(ang) * 1.0), (u + math.cos(ang) * 3.6, v + math.sin(ang) * 3.6)], 0.55, taper=True)
    place_name(d, a, lines=["Double", "Feature"], where="below", room=5.0, caps=21.5)


# ---- 193: Hilltop Trolley ------------------------------------------------------------------------------------------------------------
@painting(193)
def p193(d, a):   # a trolley car grinding up a steep street, its pole on the wire, its bell ringing
    ink = d.ink
    ang, run = 24.0, 96.0
    sl = math.tan(math.radians(ang))
    top = 2.0 + run * sl
    d.shape("art_green", [(0.0, -1.0), (run, -1.0), (run, top), (0.0, 2.0)], ink=ink)
    d.shape("art_lgrey", [(0.0, 2.0), (run, top), (run, top - 3.4), (0.0, -1.4)], ink=0.0)
    G0 = _T(0.0, 2.0, ang)
    L = run / math.cos(math.radians(ang))
    d.line("art_dark", [G0(0.0, -0.5), G0(L, -0.5)], 0.7, taper=False)
    # (a little house up top of the hill, to say it is one)
    hx, hy = run - 12.0, top
    d.shape("art_white", [(hx - 6.0, hy - 0.6), (hx + 6.0, hy - 0.6), (hx + 6.0, hy + 8.0), (hx - 6.0, hy + 8.0)], ink=ink)
    d.shape("art_red", [(hx - 7.6, hy + 8.0), (hx + 7.6, hy + 8.0), (hx, hy + 14.0)], ink=ink)
    d.shape("art_yellow", [(hx - 2.0, hy + 2.6), (hx + 2.0, hy + 2.6), (hx + 2.0, hy + 5.8), (hx - 2.0, hy + 5.8)], ink=ink * 0.7)
    # (the car, big on the grade: on its trucks, red below and cream above, its windows, its headlamp; its pole up on a
    #  length of wire; the bell)
    K, c0 = 1.45, 30.0
    Q = lambda u, v: G0(c0 + u * K, v * K)               # noqa: E731
    Qs = lambda pts: [Q(u, v) for u, v in pts]           # noqa: E731
    for u in (-9.0, 9.0):
        d.shape("art_dark", Qs([(u - 4.0, 1.4), (u + 4.0, 1.4), (u + 3.4, 4.0), (u - 3.4, 4.0)]), ink=ink * 0.7)
        for du in (-2.2, 2.2):
            d.oval("art_black", *Q(u + du, 1.6), 1.6 * K, 1.6 * K, ink=ink * 0.6)
            d.oval("art_lsteel", *Q(u + du, 1.6), 0.5 * K, 0.5 * K, ink=0.0)
    d.shape("art_red", Qs([(-16.0, 4.0), (16.0, 4.0), (17.6, 6.0), (17.6, 11.0), (-17.6, 11.0), (-17.6, 6.0)]), ink=ink)
    d.shape("art_cream", Qs([(-17.6, 11.0), (17.6, 11.0), (17.0, 18.6), (-17.0, 18.6)]), ink=ink)
    d.fill("art_dark", Qs([(-17.6, 18.6), (17.6, 18.6), (16.0, 20.4), (10.0, 21.2), (-10.0, 21.2), (-16.0, 20.4)]), ink=ink, steps=2)
    for i in range(6):
        u = -15.0 + i * 5.4
        d.shape("art_sky", Qs([(u, 12.4), (u + 4.0, 12.4), (u + 4.0, 17.4), (u, 17.4)]), ink=ink * 0.6)
    d.line("art_gold", Qs([(-17.0, 8.4), (17.0, 8.4)]), 0.6, taper=False)
    d.oval("art_yellow", *Q(17.8, 8.0), 1.3 * K, 1.4 * K, ink=ink * 0.7)
    d.line("art_dark", [Q(-9.0, 21.0), Q(-16.0, 30.0)], 0.7, taper=False)
    d.line("art_lsteel", [Q(-30.0, 30.4), Q(-2.0, 30.4)], 0.45, taper=True)
    d.oval("art_dark", *Q(-16.2, 30.2), 0.9, 0.9, ink=0.0)
    bx, by = Q(10.0, 23.0)
    d.line("art_dark", [Q(10.0, 21.0), Q(10.0, 22.0)], 0.6, taper=False)
    d.fill("art_gold", [(bx - 2.6, by - 1.6), (bx + 2.6, by - 1.6), (bx + 1.6, by + 1.8), (bx, by + 2.9), (bx - 1.6, by + 1.8)], ink=ink * 0.8, steps=2)
    for (rr, a0, a1) in ((5.0, 10.0, 80.0), (7.6, 15.0, 75.0), (5.0, 100.0, 170.0), (7.6, 105.0, 165.0)):
        d.line("art_yellow", ellipse(bx, by + 0.6, rr, rr * 0.8, 0.0, 24, a0, a1), 0.55, taper=True)
    for u in (-22.0, -26.0):                             # (the grind of its wheels up the grade)
        d.line("art_white", [Q(u, 3.0), Q(u + 3.0, 3.6)], 0.6, taper=True)
    place_name(d, a, lines=["Hilltop", "Trolley"], where="below", room=5.0, caps=21.5)


# ---- 194: Wind-Up Wonder -------------------------------------------------------------------------------------------------------------
@painting(194)
def p194(d, a):   # a tin wind-up toy bomber on its four little wheels, printed windows and seams, its big key in its back
    ink = d.ink
    # (its wheels under it, on their axles)
    for u in (12.0, 44.0):
        d.line("art_dark", [(u, 5.0), (u, 10.0)], 0.8, taper=False)
        d.oval("art_black", u, 4.6, 4.2, 4.2, ink=ink)
        d.oval("art_lsteel", u, 4.6, 1.6, 1.6, ink=ink * 0.5)
    # (its fin and tailplane, its body, the stripe and seam printed on it, its nose)
    d.fill("art_yellow", [(2.0, 18.0), (0.0, 34.0), (5.0, 36.0), (11.0, 22.0)], ink=ink, steps=2)
    body = [(0.0, 18.0), (4.0, 24.0), (20.0, 27.0), (46.0, 27.0), (56.0, 24.0), (60.0, 18.0), (57.0, 12.0), (46.0, 9.4), (14.0, 10.0), (4.0, 13.0)]
    d.fill("art_red", body, ink=ink, steps=3)
    d.fill("art_white", [(3.0, 17.0), (20.0, 18.4), (50.0, 18.4), (59.6, 17.0), (59.4, 15.4), (50.0, 16.0), (20.0, 16.0), (3.6, 15.4)], ink=0.0, steps=2)
    for i in range(12):                                   # (the tabs along its seam)
        u = 8.0 + i * 4.2
        d.shape("art_dark", [(u, 24.6), (u + 1.4, 24.6), (u + 1.4, 25.4), (u, 25.4)], ink=0.0)
    d.fill("art_sky", [(50.0, 20.0), (55.0, 21.6), (58.0, 19.0), (52.0, 18.8)], ink=ink * 0.7, steps=2)
    for i in range(4):
        u = 22.0 + i * 5.0
        d.shape("art_sky", [(u, 20.0), (u + 3.0, 20.0), (u + 3.0, 22.6), (u, 22.6)], ink=ink * 0.7)
    # (its wing, through it, the two motors and their props)
    d.fill("art_yellow", [(22.0, 14.0), (40.0, 14.6), (44.0, 12.0), (24.0, 10.0)], ink=ink, steps=2)
    for (u, v) in ((38.0, 12.6), (30.0, 12.0)):
        d.fill("art_lsteel", [(u - 3.0, v - 1.6), (u + 3.0, v - 1.6), (u + 4.4, v), (u + 3.0, v + 1.6), (u - 3.0, v + 1.6)], ink=ink * 0.8, steps=2)
    d.oval("art_lgrey", 61.0, 18.0, 1.2, 7.0, ink=ink * 0.6)
    d.oval("art_dark", 60.6, 18.0, 1.0, 1.0, ink=0.0)
    star(d, 14.0, 20.4, 2.8, "art_white", ink=ink * 0.6)
    # (the key in its back, mid-turn: its stem and its two lobes)
    kx, ky = 28.0, 26.6
    d.line("art_lsteel", [(kx, ky), (kx, ky + 6.0)], 1.4, taper=False)
    K = _T(kx, ky + 6.0, 0.0)
    d.fill("art_lsteel", [K(-0.8, 0.0), K(-7.0, 2.4), K(-8.4, 7.0), K(-5.0, 9.0), K(-0.8, 4.0), K(0.8, 4.0), K(5.0, 9.0), K(8.4, 7.0), K(7.0, 2.4), K(0.8, 0.0)],
           ink=ink, steps=4)
    for su in (-1.0, 1.0):
        d.oval("art_dark", *K(su * 5.4, 5.4), 1.2, 1.4, ink=0.0)
    for (rr, a0, a1) in ((11.0, 20.0, 70.0), (11.0, 110.0, 160.0)):
        d.line("art_white", ellipse(kx, ky + 9.0, rr, rr * 0.35, 0.0, 24, a0, a1), 0.5, taper=True)
    # (whirring off: its speed lines behind)
    speed(d, -2.0, 16.0, 8.0, n=3, gap=3.0, paint="art_white", turn=180.0, w=0.7)
    place_name(d, a, lines=["Wind-Up", "Wonder"], where="below", room=5.0, caps=21.5)


# ---- 195: Wishing Star ---------------------------------------------------------------------------------------------------------------
@painting(195)
def p195(d, a):   # a child in striped pyjamas kneeling up at the window, chin on his hands, wishing on one bright star
    ink = d.ink
    # (the window: the night in its panes, the one big star; its frame and bars; the curtains tied back)
    d.shape("art_navy", [(10.0, 16.0), (58.0, 16.0), (58.0, 64.0), (10.0, 64.0)], ink=0.0)
    star(d, 44.0, 50.0, 6.0, "art_white", ink=ink * 0.6)          # (white: a crude hand's few tins would turn a yellow one red)
    for (u, v, r) in ((44.0, 50.0, 10.0),):
        for k in range(8):
            t = math.radians(k * 45.0 + 22.5)
            d.line("art_white", [(u + math.cos(t) * 7.0, v + math.sin(t) * 7.0), (u + math.cos(t) * r, v + math.sin(t) * r)], 0.5, taper=True)
    for (u, v) in ((22.0, 56.0), (16.0, 44.0), (52.0, 32.0), (30.0, 60.0)):
        twinkle(d, u, v, 0.9, "art_white")
    for pts in ([(8.0, 14.0), (60.0, 14.0), (60.0, 17.4), (8.0, 17.4)], [(8.0, 62.6), (60.0, 62.6), (60.0, 66.0), (8.0, 66.0)],
                [(8.0, 14.0), (11.4, 14.0), (11.4, 66.0), (8.0, 66.0)], [(56.6, 14.0), (60.0, 14.0), (60.0, 66.0), (56.6, 66.0)],
                [(33.0, 16.0), (35.0, 16.0), (35.0, 64.0), (33.0, 64.0)], [(10.0, 39.0), (58.0, 39.0), (58.0, 41.0), (10.0, 41.0)]):
        d.shape("art_white", pts, ink=ink * 0.8)
    d.shape("art_white", [(4.0, 12.0), (64.0, 12.0), (64.0, 15.0), (4.0, 15.0)], ink=ink)        # (the sill)
    for su, u0 in ((1.0, 6.0), (-1.0, 62.0)):
        d.fill("art_red", [(u0, 68.0), (u0 + su * 9.0, 68.0), (u0 + su * 6.0, 46.0), (u0 + su * 3.0, 36.0), (u0 + su * 6.0, 22.0), (u0 + su * 5.0, 15.0),
                           (u0, 15.0)], ink=ink, steps=3)
        d.line("art_white", [(u0 + su * 1.0, 36.0), (u0 + su * 4.6, 36.0)], 0.9, taper=False)
    # (the child kneeling up to the sill, elbows on it, hands together under his chin, looking up at it)
    x, y, h = 14.0, -6.0, 50.0
    kw = dict(pose="kneel", arms=(to(1.2, 5.6, "down"), to(1.0, 5.4, "down")),
              wear={"shirt": "art_red", "stripes": "art_white", "stripe_dir": "h", "trousers": "art_red", "shoes": "art_skin", "sleeves": "long"},
              hair="tousle", hair_paint="art_blond", mouth="smile", eyes="shut")
    kid(d, x, y, h, 1, **kw)
    # (his bed's quilt heaped up before his knees, patched)
    quilt = [(-2.0, -6.0), (42.0, -6.0), (42.0, 6.0), (36.0, 11.0), (24.0, 13.0), (10.0, 12.6), (0.0, 10.0), (-2.0, 6.0)]
    d.fill("art_red", quilt, ink=ink, steps=3)
    for (u, v) in ((4.0, 2.0), (16.0, 6.0), (28.0, 1.0), (34.0, 7.0), (10.0, -3.0), (22.0, -4.0)):
        d.shape("art_white", [(u - 2.6, v - 2.2), (u + 2.6, v - 2.2), (u + 2.6, v + 2.2), (u - 2.6, v + 2.2)], ink=ink * 0.5)
    place_name(d, a, lines=["Wishing", "Star"], where="right", room=6.0, at=40.0, caps=21.5)


# ---- 196: Sign Painter's Holiday -----------------------------------------------------------------------------------------------------
@painting(196)
def p196(d, a):   # a sign painter up his ladder, his lettering brush raised toward the aircraft's own name, his pot dripping
    ink = d.ink
    base, tip = (6.0, 0.0), (20.0, 62.0)
    ladder(d, base, tip, 8.0, [6.0, 13.0, 20.0, 27.0, 34.0, 41.0, 48.0, 55.0], paint="art_tan", w=1.6)
    L = math.hypot(tip[0] - base[0], tip[1] - base[1])
    rung = 20.0
    fx, fy = base[0] + (tip[0] - base[0]) * rung / L, base[1] + (tip[1] - base[1]) * rung / L
    kw = dict(pose="stand", arms=(to(1.9, 6.6, "down"), to(1.5, 4.8, "down")),
              wear={"shirt": "art_blue", "sleeves": "rolled", "overalls": "art_white", "shoes": "art_brown", "cap": "art_white"},
              hat="cap", mouth="grin")
    F0 = man(Drawing(1), fx + 1.0, fy + 0.6, 52.0, 1, **kw)
    (bx, by), _ = F0.hand["fore"]
    (mx, my), _ = F0.hand["aft"]
    # (his paint pot hung on the ladder by his knee, yellow running down it and dripping)
    lo = 10.0
    px, py = base[0] + (tip[0] - base[0]) * lo / L + 5.6, base[1] + (tip[1] - base[1]) * lo / L - 4.0
    d.line("art_lsteel", ellipse(px, py + 5.4, 3.4, 3.0, 0.0, 20, 20.0, 160.0), 0.4, taper=False)
    d.shape("art_lsteel", [(px - 3.4, py - 3.6), (px + 3.4, py - 3.6), (px + 3.6, py + 3.4), (px - 3.6, py + 3.4)], ink=ink)
    d.fill("art_yellow", [(px - 3.6, py + 3.4), (px + 3.6, py + 3.4), (px + 3.4, py + 1.0), (px + 2.0, py - 1.6), (px + 1.2, py + 1.4), (px - 0.6, py - 3.0),
                          (px - 1.6, py + 1.0), (px - 3.4, py + 0.4)], ink=ink * 0.6, steps=2)
    for (u, v, r) in ((0.6, -6.0, 0.8), (2.0, -9.4, 0.6), (-0.4, -13.0, 0.7)):
        d.fill("art_yellow", [(px + u - r, py + v), (px + u, py + v + r * 2.4), (px + u + r, py + v), (px + u, py + v - r)], ink=ink * 0.5, steps=2)
    F = man(d, fx + 1.0, fy + 0.6, 52.0, 1, **kw)
    # (his mahl stick from his aft hand out to the skin, his lettering brush up in the other, its tip wet with yellow)
    d.line("art_tan", [(mx - 1.0, my - 0.4), (mx + 20.0, my + 4.0)], 0.6, taper=False)
    d.oval("art_white", mx + 20.6, my + 4.1, 1.3, 1.3, ink=ink * 0.6)
    F.grip("aft")
    G = _T(bx, by, 28.0)
    d.line("art_tan", [G(-3.0, 0.0), G(5.0, 0.0)], 0.6, taper=False)
    d.shape("art_lsteel", [G(5.0, -0.45), G(7.0, -0.45), G(7.0, 0.45), G(5.0, 0.45)], ink=ink * 0.6)
    d.fill("art_yellow", [G(7.0, -0.55), G(10.4, 0.0), G(7.0, 0.55)], ink=ink * 0.6, steps=1)
    F.grip("fore")
    place_name(d, a, lines=["Sign", "Painter's", "Holiday"], where="right", room=4.0, at=G(10.4, 0.0)[1] - 6.0, caps=21.5)
