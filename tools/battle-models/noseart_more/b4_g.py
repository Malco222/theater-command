"""Batch 4, numbers 183 to 189 (painter b4_g): a village smith hammering a propeller blade on his anvil, a lobsterman
in his slicker hauling up a trap with a bomb in it, a polar bear in a striped bathing suit diving off an iceberg, a
woman on a picking ladder with her basket of oranges, a front door open on a lamplit room over its doormat, a tin
hut with a crooked stovepipe puffing smoke rings and a calf in a flying helmet far too big for him. Flat shapes with
noseart's Drawing API and the figure kit; each name set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b2_f import ladder  # noqa: E402
from noseart_more.b3_a import _T  # noqa: E402


# ---- 183: Village Smithy -------------------------------------------------------------------------------------------------------------
def anvil(d, x, top, L, ground, paint="art_dark", shine="art_lsteel", stump="art_lbrown"):
    """A smith's anvil side-on, its horn to the right: the middle of its face at x, its face's top at `top`, `L` from
    heel to horn tip; on a wooden stump down to `ground`."""
    k = L / 24.0
    P = lambda u, v: (x + u * k, top + v * k)            # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    gb = (ground - top) / k
    d.fill(stump, Ps([(-6.6, -9.0), (-7.2, gb + 0.6), (-3.0, gb), (3.0, gb + 0.4), (6.6, gb + 0.2), (6.0, -9.0)]), ink=ink, steps=2)
    for (u0, u1) in ((-4.0, -4.4), (0.6, 0.8), (3.6, 4.2)):
        d.line("art_brown", Ps([(u0, -11.0), (u1, gb + 1.6)]), 0.35 * k, taper=True)
    body = [(-11.0, 0.0), (5.0, 0.0), (9.0, -0.6), (13.0, -1.8), (9.0, -2.8), (5.0, -4.0), (3.0, -5.0), (2.4, -7.4), (5.2, -9.4), (5.6, -10.6),
            (-6.6, -10.6), (-6.2, -9.4), (-3.6, -7.4), (-4.0, -5.0), (-6.4, -3.6), (-11.0, -2.8)]
    d.shape(paint, Ps(body), ink=ink)
    d.shape(shine, Ps([(-10.6, -0.3), (4.8, -0.3), (8.4, -0.8), (8.4, -1.3), (4.8, -1.0), (-10.6, -1.0)]), ink=0.0)
    d.line(shine, Ps([(-1.0, -7.0), (-1.4, -9.8)]), 0.4 * k, taper=False)
    return P


@painting(183)
def p183(d, a):   # a smith in his leather apron, hammer up, a propeller blade held in his tongs across the anvil, glowing where struck
    ink = d.ink
    x, y, h = 12.0, 2.0, 64.0
    kw = dict(pose="wide", arms=(to(0.5, 8.1, "up"), to(1.7, 3.9, "down")),
              wear={"shirt": "art_white", "sleeves": "rolled", "apron": "art_brown", "apron_cut": "bib", "trousers": "art_dark", "boots": "art_black"},
              hair="short", hair_paint="art_hair", moustache="full", mouth="grin")
    F0 = man(Drawing(1), x, y, h, 1, **kw)
    (fx, fy), _ = F0.hand["fore"]
    (tx, ty), _ = F0.hand["aft"]
    # (the anvil before him, its face at his hips)
    ax, top = 41.0, 29.0
    P = anvil(d, ax, top, 26.0, y)
    k = 26.0 / 24.0
    # (the blade: its root in his tongs at the heel, its yellow tip out past the horn; red-hot where the hammer falls)
    root = (tx + 7.0, top + 1.6)
    tip = (P(19.0, 0.0)[0], top + 2.4)
    L = tip[0] - root[0]
    G = _T(root[0], root[1], math.degrees(math.atan2(tip[1] - root[1], L)))
    blade = [(0.0, -0.7), (6.0, -0.8), (9.0, -1.6), (L - 5.0, -1.8), (L - 1.0, -1.3), (L, 0.0), (L - 1.0, 1.4), (L - 5.0, 1.9), (9.0, 1.7), (6.0, 0.8), (0.0, 0.7)]
    d.fill("art_dark", [G(u, v) for u, v in blade], ink=ink, steps=3)
    d.fill("art_yellow", [G(u, v) for u, v in [(L - 4.6, -1.8), (L - 1.0, -1.3), (L, 0.0), (L - 1.0, 1.4), (L - 4.6, 1.9)]], ink=ink * 0.8, steps=3)
    hot = (ax + 1.0, top + 2.2)
    d.oval("art_orange", *hot, 4.6, 1.8, ink=0.0)
    d.oval("art_yellow", hot[0] + 0.4, hot[1], 2.4, 1.0, ink=0.0)
    d.line("art_white", [G(10.0, 0.9), G(L - 6.0, 1.2)], 0.3, taper=True)
    # (the tongs: two long jaws from his aft hand to the blade's root)
    for dv in (-0.6, 0.6):
        d.line("art_steel", [(tx + 0.6, ty + dv * 0.6), (root[0] - 2.0, root[1] + dv * 1.2), (root[0] + 1.4, root[1] + dv * 0.9)], 0.75, taper=False)
    F = man(d, x, y, h, 1, **kw)
    F.grip("aft")
    # (his hammer up over his head, its head swung back, ready to fall)
    hd = (fx - 4.0, fy + 9.0)
    d.line("art_tan", [(fx + 0.6, fy - 1.6), hd], 1.0, taper=False)
    Hm = _T(*hd, math.degrees(math.atan2(hd[1] - fy, hd[0] - fx)) - 90.0)
    d.shape("art_dark", [Hm(-4.2, -1.4), Hm(3.0, -1.4), Hm(3.6, -1.0), Hm(3.6, 1.0), Hm(3.0, 1.4), Hm(-4.2, 1.4)], ink=ink)
    d.shape("art_lsteel", [Hm(-4.0, 0.5), Hm(3.4, 0.5), Hm(3.4, 1.1), Hm(-4.0, 1.1)], ink=0.0)
    F.grip("fore")
    # (the sparks off the last blow)
    for (u, v, r) in ((-3.0, 6.0, 1.3), (3.0, 7.6, 1.6), (6.0, 4.6, 1.1), (-6.0, 4.0, 1.0), (9.0, 6.8, 0.9), (0.0, 9.6, 1.0)):
        star(d, hot[0] + u, hot[1] + v, r, "art_yellow", points=4, ink=0.0)
    for (u, v) in ((-5.0, 8.0), (5.0, 10.0), (10.0, 4.0), (-8.0, 6.4)):
        d.line("art_orange", [(hot[0] + u * 0.5, hot[1] + v * 0.4), (hot[0] + u, hot[1] + v)], 0.4, taper=True)
    place_name(d, a, lines=["Village", "Smithy"], where="right", room=6.0, at=38.0, caps=21.5)


# ---- 184: Lobster Pot Lou ------------------------------------------------------------------------------------------------------------
def trap(d, x, y, w, h, slat="art_white", rail="art_lbrown", inside=None):
    """A lath lobster trap side-on: its foot's middle at (x, y), `w` long, its box `h` tall under a round top; slats up
    its sides and over its bow, `inside` (a callable painting what is caught) painted first, seen between them."""
    ink = d.ink
    r = h * 0.55
    if inside:
        inside()
    n = 7
    for i in range(n):
        u = x - w / 2 + w * (i + 0.5) / n
        t = (u - x) / (w / 2)
        tp = y + h + r * math.sqrt(max(0.0, 1.0 - t * t)) * 0.8
        d.shape(slat, [(u - 0.9, y + 1.2), (u + 0.9, y + 1.2), (u + 0.9, tp), (u - 0.9, tp)], ink=ink * 0.7)
    arch = [(x - w / 2 + w * j / 20, y + h + r * 0.8 * math.sqrt(max(0.0, 1.0 - ((j / 10) - 1.0) ** 2))) for j in range(21)]
    d.line(rail, arch, 1.3, taper=False)
    for v in (y + 0.6, y + h):
        d.shape(rail, [(x - w / 2 - 0.6, v - 0.8), (x + w / 2 + 0.6, v - 0.8), (x + w / 2 + 0.6, v + 0.8), (x - w / 2 - 0.6, v + 0.8)], ink=ink * 0.8)
    for u in (x - w / 2, x + w / 2):
        d.shape(rail, [(u - 0.8, y - 0.2), (u + 0.8, y - 0.2), (u + 0.8, y + h + 0.4), (u - 0.8, y + h + 0.4)], ink=ink * 0.8)
    return (x, y + h + r * 0.8)


@painting(184)
def p184(d, a):   # a lobsterman in a yellow slicker and sou'wester leaning back on his line, a trap with a bomb in it coming up
    ink = d.ink
    x, y, h = 13.0, 6.0, 60.0
    kw = dict(pose="wide", arms=(to(1.6, 4.6, "down"), to(1.3, 4.1, "down")), tilt=-9.0,
              wear={"coat": "art_yellow", "trousers": "art_yellow", "boots": "art_black", "shirt": "art_white", "hat": "art_yellow"},
              hat="souwester", moustache="full", mouth="grin")
    F0 = man(Drawing(1), x, y, h, 1, **kw)
    (fx, fy), _ = F0.hand["fore"]
    # (the sea, under his dory and the trap)
    waves(d, -2.0, 64.0, 6.0, 5.0, "art_blue", "art_white", n=5)
    # (the trap coming up out of the water, the bomb caught in it; the drips off it)
    tx, ty = 48.0, 9.0

    def caught():
        bomb(d, tx + 8.0, ty + 7.2, 15.0, 0.0, paint="art_black", band="art_yellow")
    top = trap(d, tx, ty, 20.0, 11.0, inside=caught)
    for (u, v) in ((-6.0, -2.0), (-1.0, -3.6), (4.0, -2.4), (8.0, -4.0)):
        d.oval("art_white", tx + u, ty + v, 0.6, 0.9, ink=0.0)
    # (his line from his hands down to the trap's bridle)
    d.line("art_white", [top, (top[0] - 1.0, top[1] + 2.4)], 0.5, taper=False)
    d.line("art_white", [(fx + 1.0, fy - 0.6), (top[0] - 1.0, top[1] + 2.4)], 0.55, taper=False)
    man(d, x, y, h, 1, **kw)
    # (his dory's side before his knees, its red stripe)
    hull = [(-4.0, 17.0), (6.0, 15.4), (18.0, 15.0), (31.0, 16.6), (28.6, 8.0), (22.0, 4.4), (4.0, 4.0), (-1.6, 7.0)]
    d.fill("art_white", hull, ink=ink, steps=2)
    d.line("art_blue", [(-3.6, 13.6), (6.0, 12.2), (18.0, 11.8), (30.4, 13.2)], 1.6, taper=False)
    for u in (6.0, 12.0, 18.0, 24.0):
        d.line("art_black", [(u, 13.0), (u - 0.6, 5.0)], 0.3, taper=False)
    waves(d, -3.0, 32.0, 4.0, 4.0, "art_blue", "art_white", n=3)
    place_name(d, a, lines=["Lobster", "Pot Lou"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 185: Polar Bear Plunge ----------------------------------------------------------------------------------------------------------
def diving_bear(d, x, y, long, turn=-40.0, coat="art_white", shade="art_lgrey", suit=("art_red", "art_white")):
    """A polar bear diving head first: its body's middle at (x, y), `long` hind feet to fore paws, going `turn` degrees;
    its forelegs out together ahead of its head, its head up between its shoulders, grinning, ears up; its hind legs
    trailing; a period striped bathing suit round its middle, its stripes round the body, straps over the shoulders."""
    k = long / 46.0
    G0 = _T(x, y, turn)
    G = lambda u, v: G0(u * k, v * k)                    # noqa: E731
    Gs = lambda pts: [G(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    rx, ry = 12.5, 7.4
    # (the far legs: a hind one trailing, a fore one reaching out ahead)
    d.limb(shade, Gs([(-8.0, 3.4), (-14.0, 5.4), (-19.0, 6.0)]), [3.6 * k, 3.0 * k, 2.6 * k], ink=ink * 0.8)
    d.oval(shade, *G(-19.6, 6.0), 1.6 * k, 2.8 * k, turn=turn, ink=ink * 0.7)
    d.limb(shade, Gs([(7.0, -2.0), (15.0, -4.6), (25.0, -3.6)]), [3.4 * k, 2.8 * k, 2.4 * k], ink=ink * 0.8)
    d.oval(shade, *G(25.6, -3.6), 2.4 * k, 1.8 * k, turn=turn, ink=ink * 0.7)
    # (its body, a stub of a tail; the suit)
    d.oval(coat, *G(-12.6, 1.6), 1.6 * k, 1.4 * k, turn=turn, ink=ink * 0.8)
    d.oval(coat, *G(0.0, 0.0), rx * k, ry * k, turn=turn, ink=ink)
    for i in range(6):
        u0, u1 = -8.4 + 2.5 * i, -8.4 + 2.5 * (i + 1)
        hw = lambda u: ry * math.sqrt(max(0.0, 1.0 - (u / rx) ** 2)) * 0.97   # noqa: E731
        d.shape(suit[i % 2], Gs([(u0, -hw(u0)), (u1, -hw(u1)), (u1, hw(u1)), (u0, hw(u0))]), ink=ink * 0.5)
    d.line(suit[0], Gs([(6.4, 5.4), (9.6, 3.4)]), 1.0 * k, taper=False)
    # (the near hind leg)
    d.limb(coat, Gs([(-8.0, -3.0), (-14.0, -4.0), (-19.6, -4.0)]), [4.0 * k, 3.2 * k, 2.8 * k], ink=ink)
    d.oval(coat, *G(-20.2, -4.0), 1.8 * k, 3.0 * k, turn=turn, ink=ink * 0.8)
    d.oval("art_dark", *G(-21.0, -4.0), 0.7 * k, 2.0 * k, turn=turn, ink=0.0)
    # (its head up between its shoulders: round ears, a long muzzle, a black nose, an eye wide with fun, a grin)
    hx, hy = 15.6, 2.4
    d.limb(coat, Gs([(7.0, 1.0), (11.0, 1.8), (hx, hy)]), [6.0 * k, 5.4 * k, 4.8 * k], ink=ink)
    for (u, v) in ((hx - 2.4, hy + 4.6), (hx + 0.8, hy + 5.0)):
        d.oval(coat, *G(u, v), 1.7 * k, 1.5 * k, turn=turn, ink=ink * 0.8)
        d.oval(shade, *G(u, v - 0.2), 0.8 * k, 0.7 * k, turn=turn, ink=0.0)
    d.oval(coat, *G(hx, hy), 5.4 * k, 4.8 * k, turn=turn, ink=ink)
    d.oval(coat, *G(hx + 5.0, hy - 1.4), 3.4 * k, 2.4 * k, turn=turn, ink=ink)
    d.oval("art_black", *G(hx + 8.0, hy - 0.8), 1.1 * k, 0.9 * k, turn=turn, ink=0.0)
    d.oval("art_white", *G(hx + 8.1, hy - 0.5), 0.3 * k, 0.25 * k, ink=0.0)
    d.oval("art_black", *G(hx + 2.0, hy + 1.6), 0.8 * k, 0.8 * k, ink=0.0)
    d.oval("art_white", *G(hx + 2.2, hy + 1.9), 0.25 * k, 0.25 * k, ink=0.0)
    d.line("art_ink", Gs([(hx + 1.0, hy + 2.9), (hx + 2.4, hy + 3.2)]), 0.3 * k)
    d.line("art_ink", Gs([(hx + 3.6, hy - 3.0), (hx + 5.4, hy - 3.6), (hx + 7.2, hy - 2.6)]), 0.35 * k)
    # (the near foreleg out ahead under its chin, paws together for the water)
    d.limb(coat, Gs([(6.0, -4.0), (15.0, -6.8), (26.0, -5.8)]), [3.8 * k, 3.0 * k, 2.6 * k], ink=ink)
    d.oval(coat, *G(26.8, -5.8), 2.6 * k, 2.0 * k, turn=turn, ink=ink * 0.8)
    return G


@painting(185)
def p185(d, a):   # a polar bear in a striped bathing suit diving off an iceberg, head first for the sea
    ink = d.ink
    # (the iceberg, peaked, its shadowed faces; the sea at its foot)
    berg = [(0.0, 4.0), (1.0, 24.0), (5.0, 38.0), (9.0, 36.0), (14.0, 52.0), (19.0, 44.0), (23.0, 47.0), (28.0, 30.0), (31.0, 4.0)]
    d.shape("art_white", berg, ink=ink)
    for face in ([(14.0, 52.0), (19.0, 44.0), (17.0, 30.0), (12.0, 20.0)], [(23.0, 47.0), (28.0, 30.0), (31.0, 4.0), (24.0, 4.0), (22.0, 24.0)],
                 [(5.0, 38.0), (9.0, 36.0), (7.0, 22.0), (3.0, 16.0)]):
        d.shape("art_lblue", face, ink=0.0)
    # (its leap: the arc it took off the peak)
    for (u, v) in ((19.0, 56.0), (24.0, 57.0), (29.0, 55.6)):
        d.line("art_white", [(u, v), (u + 3.0, v - 0.4)], 0.8, taper=True)
    diving_bear(d, 40.0, 32.0, 46.0, turn=-40.0)
    # (the sea, and the splash ring waiting for it)
    waves(d, -2.0, 66.0, 5.0, 5.0, "art_blue", "art_white", n=6)
    d.line("art_white", ellipse(56.0, 6.6, 7.0, 1.6, 0, 30), 0.6, taper=False)
    place_name(d, a, lines=["Polar", "Bear", "Plunge"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 186: Orange Grove Olive ---------------------------------------------------------------------------------------------------------
@painting(186)
def p186(d, a):   # a woman in a sun dress up a picking ladder in an orange tree, reaching for one, her basket of oranges on her arm
    ink = d.ink
    # (the tree: its trunk and its round dark-green head, oranges all through it)
    d.limb("art_brown", [(48.0, 0.0), (47.0, 8.0), (45.0, 16.0)], [3.6, 3.0, 2.6], ink=ink)
    cloud(d, 46.0, 36.0, 42.0, 42.0, "art_green", seed=186, flat=False, shade="art_dgreen")
    for (u, v) in ((34.0, 46.0), (42.0, 54.0), (52.0, 50.0), (60.0, 40.0), (40.0, 34.0), (50.0, 32.0), (58.0, 26.0), (45.0, 22.0), (33.0, 26.0), (63.0, 48.0),
                   (54.0, 18.0), (38.0, 16.0)):
        d.oval("art_tangerine", u, v, 2.3, 2.2, ink=ink * 0.6)
        d.oval("art_yellow", u - 0.8, v + 0.7, 0.6, 0.5, ink=0.0)
        d.fill("art_dgreen", [(u + 0.2, v + 1.8), (u + 1.6, v + 3.2), (u + 1.0, v + 1.6)], ink=0.0, steps=1)
    # (the picking ladder, leaned into the tree)
    base, tip = (16.0, 0.0), (30.0, 54.0)
    ladder(d, base, tip, 7.0, [6.0, 14.0, 22.0, 30.0, 38.0, 46.0], paint="art_tan", w=1.6)
    L = math.hypot(tip[0] - base[0], tip[1] - base[1])
    fx, fy = base[0] + (tip[0] - base[0]) * 22.0 / L, base[1] + (tip[1] - base[1]) * 22.0 / L
    # (Olive on her rung, reaching up for an orange; her basket on her other arm)
    F = lady(d, fx, fy + 0.6, 46.0, 1, pose="stand", arms=("up_out", "hold"),
             wear={"top": "art_sky", "dots": "art_white", "sleeves": "puff", "hem": "knee", "shoes": "art_white", "belt": "art_white"},
             hair="rolls", hair_paint="art_auburn")
    (hx, hy), _ = F.hand["aft"]
    bx, by = hx + 1.0, hy - 4.0
    d.fill("art_tan", [(bx - 6.0, by + 3.0), (bx + 6.0, by + 3.0), (bx + 4.6, by - 3.6), (bx - 4.6, by - 3.6)], ink=ink, steps=2)
    for v in (by + 1.0, by - 1.2):
        d.line("art_lbrown", [(bx - 5.4, v), (bx + 5.4, v)], 0.35, taper=False)
    for (u, v) in ((-3.4, 3.6), (0.0, 4.2), (3.4, 3.6), (-1.6, 5.4), (1.8, 5.6)):
        d.oval("art_tangerine", bx + u, by + v, 1.8, 1.7, ink=ink * 0.6)
        d.oval("art_yellow", bx + u - 0.6, by + v + 0.6, 0.45, 0.4, ink=0.0)
    d.line("art_tan", ellipse(bx, by + 3.0, 6.0, 7.0, 0, 30, 0.0, 180.0), 0.7, taper=False)
    (rx_, ry_), _ = F.hand["fore"]
    d.oval("art_tangerine", rx_ + 1.0, ry_ + 1.6, 2.2, 2.1, ink=ink * 0.6)
    d.oval("art_yellow", rx_ + 0.2, ry_ + 2.3, 0.55, 0.5, ink=0.0)
    place_name(d, a, lines=["Orange", "Grove", "Olive"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 187: Welcome Mat ----------------------------------------------------------------------------------------------------------------
def flying_boot(d, x, y, size, turn=0.0, suede="art_brown", fur="art_cream", sole="art_black", mud="art_lbrown"):
    """A sheepskin flying boot side-on, toe to the right: its heel at (x, y), `size` tall to its fur cuff's top, turned
    `turn` degrees (slumped over); the fleece rolled over at the top, a zip down its front, a strap at the ankle, mud up
    its foot."""
    k = size / 26.0
    G0 = _T(x, y, turn)
    G = lambda u, v: G0(u * k, v * k)                    # noqa: E731
    Gs = lambda pts: [G(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    shaft = [(-0.4, 1.0), (-1.4, 8.0), (-1.6, 16.0), (-1.2, 21.0), (9.4, 21.4), (9.6, 15.0), (9.4, 10.0), (11.6, 7.0), (15.0, 5.0), (16.4, 3.0), (16.0, 1.0)]
    d.fill(suede, Gs(shaft), ink=ink, steps=3)
    d.fill(fur, Gs([(-2.2, 20.0), (-2.6, 24.0), (0.0, 26.0), (4.0, 25.4), (8.0, 26.2), (10.6, 24.4), (10.2, 20.2), (4.0, 19.4)]), ink=ink, steps=3)
    for (u, v) in ((0.6, 23.4), (3.6, 22.6), (6.8, 23.6), (8.6, 21.6)):
        d.line("art_lgrey", Gs([(u, v), (u + 0.8, v + 1.4)]), 0.3 * k)
    d.line("art_lsteel", Gs([(8.4, 19.6), (8.6, 12.0), (10.6, 8.0)]), 0.45 * k, taper=False)
    d.shape("art_dark", Gs([(-1.0, 8.6), (10.0, 9.6), (10.4, 7.8), (-0.8, 6.8)]), ink=ink * 0.6)
    d.shape("art_lsteel", Gs([(3.4, 7.6), (5.0, 7.8), (5.0, 9.2), (3.4, 9.0)]), ink=ink * 0.5)
    d.fill(sole, Gs([(-0.6, 0.0), (16.6, 0.0), (16.8, 1.6), (-0.6, 1.6)]), ink=ink * 0.7, steps=1)
    for (u, v, r) in ((12.0, 3.6, 1.6), (5.0, 3.0, 1.8), (9.0, 2.6, 1.4), (14.6, 2.4, 1.0), (1.0, 4.4, 1.2), (7.0, 11.0, 0.9), (2.0, 13.0, 0.8)):
        d.oval(mud, *G(u, v), r * k, r * 0.7 * k, ink=0.0)
    return G


@painting(187)
def p187(d, a):   # the front door standing open on a lamplit room, its light falling out across the doormat
    ink = d.ink
    # (the clapboard wall; the red trim round the doorway; the room inside, all lamplight)
    d.shape("art_white", [(6.0, 20.0), (6.0, 58.0), (56.0, 58.0), (56.0, 20.0)], ink=ink)
    for v in range(23, 57, 3):
        for (u0, u1) in ((6.4, 17.6), (44.4, 55.6)):
            d.line("art_ink", [(u0, v), (u1, v + 0.3)], 0.3, taper=False)
    for v in range(55, 57, 3):
        d.line("art_ink", [(17.6, v), (44.4, v + 0.3)], 0.3, taper=False)
    d.shape("art_red", [(17.6, 20.0), (17.6, 54.4), (44.4, 54.4), (44.4, 20.0)], ink=ink)
    d.shape("art_yellow", [(20.4, 20.0), (20.4, 51.6), (41.6, 51.6), (41.6, 20.0)], ink=ink)
    d.line("art_ink", [(20.4, 25.0), (41.6, 25.0)], 0.35, taper=False)
    # (a lamp on a little table inside, black against the light)
    d.shape("art_black", [(31.0, 33.0), (31.0, 34.2), (39.6, 34.2), (39.6, 33.0)])
    for u in (31.8, 38.8):
        d.shape("art_black", [(u - 0.45, 25.4), (u - 0.45, 33.0), (u + 0.45, 33.0), (u + 0.45, 25.4)])
    d.shape("art_black", [(34.0, 34.2), (34.0, 35.0), (36.6, 35.0), (36.6, 34.2)])
    d.shape("art_black", [(35.0, 35.0), (35.0, 40.0), (35.6, 40.0), (35.6, 35.0)])
    d.fill("art_black", [(32.4, 40.0), (33.6, 45.0), (37.0, 45.0), (38.2, 40.0)], ink=0.0, steps=1)
    # (the door, swung wide open into the room: its panels, its knob)
    d.shape("art_red", [(20.4, 20.0), (20.4, 51.6), (26.6, 49.4), (26.6, 22.2)], ink=ink)
    for (v0, v1) in ((24.0, 33.6), (37.0, 47.6)):
        d.line("art_white", [(21.6, v0 + 0.4), (21.6, v1), (25.4, v1 - 1.0), (25.4, v0 + 1.4), (21.6, v0 + 0.4)], 0.35, taper=False)
    d.oval("art_yellow", 25.4, 35.4, 0.7, 0.8, ink=ink * 0.7)
    # (the porch floor's edge; the lamplight falling out of the doorway and spreading over the boards; in it the mat,
    #  seen from a little above: its border, its bristle, the light catching it)
    d.line("art_ink", [(-2.0, -1.0), (64.0, -1.0)], 0.5, taper=False)
    d.shape("art_yellow", [(20.4, 20.0), (41.6, 20.0), (61.0, -0.6), (1.0, -0.6)], ink=0.0)
    mat = [(4.0, 2.0), (58.0, 2.0), (52.0, 16.0), (10.0, 16.0)]
    d.shape("art_red", mat, ink=ink)
    d.line("art_white", [(8.6, 4.4), (53.4, 4.4), (49.4, 13.8), (12.6, 13.8), (8.6, 4.4)], 0.8, taper=False)
    for i in range(18):
        u = 12.0 + i * 2.2
        d.line("art_black", [(u, 6.4 + (i % 3) * 2.2), (u + 0.4, 7.6 + (i % 3) * 2.2)], 0.35)
    for (u0, u1) in ((14.0, 26.0), (36.0, 48.0)):
        d.line("art_yellow", [(u0, 15.4), (u1, 15.4)], 0.5, taper=False)
    for u in (24.0, 30.0, 36.0):
        d.line("art_white", [(u, 17.4), (u + (u - 30.0) * 0.12, 18.8)], 0.3, taper=False)
    place_name(d, a, lines=["Welcome", "Mat"], where="below", room=5.0, caps=21.5)


# ---- 188: Stovepipe Lodge ------------------------------------------------------------------------------------------------------------
@painting(188)
def p188(d, a):   # a round-roofed tin hut, its stovepipe gone crooked, puffing smoke rings off on the wind
    ink = d.ink
    K = 1.35                                           # (the hut's scale: big, the rings drifting off level beside it)
    Q = lambda u, v: (u * K, v * K)                    # noqa: E731
    Qs = lambda pts: [Q(u, v) for u, v in pts]         # noqa: E731
    d.fill("art_tan", Qs([(-2.0, 1.0), (14.0, -1.0), (52.0, 1.0), (64.0, 6.0), (40.0, 10.0), (4.0, 8.0)]), ink=0.0, steps=3)
    # (the hut: its long round roof of corrugated tin, its near end with door and windows)
    cx0, cy0, r0 = 16.0, 4.0, 16.0                     # (the near end's arch)
    cx1, cy1, r1 = 46.0, 11.0, 13.0                    # (the far end's)
    arc = lambda cx, cy, r, t0, t1, st: [Q(cx + r * math.cos(math.radians(t)), cy + r * math.sin(math.radians(t))) for t in range(t0, t1, st)]  # noqa: E731
    roof = [Q(cx0 + r0, cy0), Q(cx1 + r1, cy1)] + arc(cx1, cy1, r1, 10, 100, 10) + arc(cx0, cy0, r0, 100, -1, -10)
    d.shape("art_lsteel", roof, ink=ink)
    for j in range(1, 9):
        t = j / 9
        d.line("art_steel", arc(cx0 + (cx1 - cx0) * t, cy0 + (cy1 - cy0) * t, r0 + (r1 - r0) * t, 0, 100, 10), 0.45, taper=False)
    d.shape("art_steel", arc(cx0, cy0, r0, 0, 181, 10), ink=ink)
    d.shape("art_dark", Qs([(cx0 - 4.0, cy0), (cx0 + 4.0, cy0), (cx0 + 4.0, cy0 + 12.0), (cx0 - 4.0, cy0 + 12.0)]), ink=ink * 0.8)
    d.oval("art_lsteel", *Q(cx0 + 2.4, cy0 + 6.0), 0.6, 0.6, ink=0.0)
    for su in (-1.0, 1.0):
        wx = cx0 + su * 9.6
        d.shape("art_yellow", Qs([(wx - 2.4, cy0 + 5.0), (wx + 2.4, cy0 + 5.0), (wx + 2.4, cy0 + 9.0), (wx - 2.4, cy0 + 9.0)]), ink=ink * 0.8)
        d.line("art_dark", Qs([(wx, cy0 + 5.0), (wx, cy0 + 9.0)]), 0.5, taper=False)
    # (the stovepipe up through the roof, kinked, its little hat; the rings it puffs, carried off on the wind)
    t = 0.62
    px, py = cx0 + (cx1 - cx0) * t, cy0 + (cy1 - cy0) * t + (r0 + (r1 - r0) * t) - 0.6
    pipe = Qs([(px, py), (px, py + 3.6), (px + 2.4, py + 6.6), (px + 2.2, py + 9.4)])
    d.line("art_dark", pipe, 2.6, taper=False)
    (qx, qy) = pipe[-1]
    d.shape("art_dark", [(qx - 4.2, qy + 0.6), (qx + 4.2, qy + 0.6), (qx, qy + 4.0)], ink=ink * 0.8)
    d.line("art_lsteel", [Q(px - 0.5, py + 0.6), Q(px - 0.5, py + 3.2)], 0.5, taper=False)
    ox, oy = qx + 1.0, qy + 5.0
    for i, (dx, dy, r) in enumerate(((3.0, 1.6, 2.2), (9.6, 3.6, 2.8), (17.6, 5.2, 3.4), (27.0, 6.4, 4.0), (38.0, 7.0, 4.6))):
        d.line("art_lgrey", ellipse(ox + dx, oy + dy, r, r * 0.55, -12.0, 30), 1.2 + 0.15 * i, taper=False)
        d.line("art_white", ellipse(ox + dx, oy + dy + 0.4, r * 0.9, r * 0.4, -12.0, 30, 160.0, 20.0), 0.45, taper=False)
    place_name(d, a, lines=["Stovepipe", "Lodge"], where="below", room=5.0, caps=21.5)


# ---- 189: Greenhorn ------------------------------------------------------------------------------------------------------------------
def calf(d, x, y, size, coat="art_auburn", face="art_white", hoof="art_black"):
    """A gangly calf side-on, facing right: his feet at (x, y), `size` tall to his poll; a big head, knobbly knees, a
    white face, a tuft on his tail. Returns his frame and his head's middle and width."""
    k = size / 40.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    for (u0, u1) in ((-9.0, -10.4), (8.0, 9.4)):          # (his far legs)
        d.limb("art_brown", Ps([(u0, 18.0), (u1, 10.0), (u1 - 0.2, 1.4)]), [2.2 * k, 1.5 * k, 1.2 * k], ink=ink * 0.8)
        d.oval(hoof, *P(u1, 0.8), 1.4 * k, 0.9 * k, ink=ink * 0.6)
    d.limb(coat, Ps([(-13.0, 23.0), (-16.0, 20.0), (-16.6, 13.0)]), [0.8 * k, 0.7 * k, 0.6 * k], ink=ink * 0.8)
    d.oval("art_brown", *P(-16.6, 12.0), 1.0 * k, 1.6 * k, ink=ink * 0.6)
    d.fill(coat, Ps([(-14.0, 22.0), (-13.0, 26.6), (-6.0, 28.0), (4.0, 27.6), (10.0, 28.6), (13.0, 25.0), (12.0, 18.0), (4.0, 16.6), (-6.0, 17.0),
                     (-13.0, 18.0)]), ink=ink, steps=4)
    d.fill(face, Ps([(-2.0, 17.4), (4.0, 16.8), (8.0, 17.6), (4.0, 19.6), (-1.0, 19.4)]), ink=0.0, steps=2)
    for (u0, u1) in ((-7.0, -6.4), (10.0, 11.6)):         # (his near legs, knobbly kneed)
        d.limb(coat, Ps([(u0, 19.0), (u1, 10.0), (u1 + 0.2, 1.4)]), [2.4 * k, 1.6 * k, 1.3 * k], ink=ink)
        d.oval(coat, *P(u1, 10.0), 1.8 * k, 1.6 * k, ink=ink * 0.7)
        d.oval(hoof, *P(u1 + 0.4, 0.8), 1.5 * k, 1.0 * k, ink=ink * 0.6)
    # (his neck up to a big head, white-faced, his muzzle pink)
    d.limb(coat, Ps([(10.0, 25.0), (14.0, 29.0), (17.0, 31.0)]), [4.6 * k, 4.2 * k, 3.8 * k], ink=ink)
    hx, hy = 20.0, 31.0
    d.fill(face, Ps([(hx - 5.0, hy + 4.0), (hx + 1.0, hy + 6.4), (hx + 5.0, hy + 3.0), (hx + 8.4, hy - 3.0), (hx + 8.6, hy - 6.4), (hx + 5.0, hy - 8.0),
                     (hx + 1.0, hy - 5.0), (hx - 4.4, hy - 2.0)]), ink=ink, steps=4)
    d.oval("art_pink", *P(hx + 6.8, hy - 6.0), 2.6 * k, 2.0 * k, turn=-30.0, ink=ink * 0.7)
    d.oval("art_ink", *P(hx + 7.6, hy - 5.4), 0.4 * k, 0.5 * k, ink=0.0)
    d.line("art_ink", Ps([(hx + 4.6, hy - 7.6), (hx + 6.4, hy - 8.0)]), 0.3 * k)
    d.fill(coat, Ps([(hx - 2.0, hy + 1.6), (hx - 9.0, hy - 0.4), (hx - 7.6, hy - 2.4), (hx - 2.6, hy - 0.6)]), ink=ink, steps=2)    # (his ear)
    return P, P(hx + 1.6, hy + 1.6), 12.0 * k


@painting(189)
def p189(d, a):   # a calf with tiny horns poking through a flying helmet far too big for him, the goggles slipped over his eyes
    ink = d.ink
    P, (cx, cy), w = calf(d, 10.0, 2.0, 46.0)
    k = 46.0 / 40.0
    # (the helmet, a man's, down over his brow and ears to his eyes; its strap dangling loose; the goggles askew)
    G = helmet(d, cx - 0.6, cy - 0.4, w * 1.7, 1, paint="art_brown", view="side", inside=None, turn=-16.0)
    # (his little horns poking through it, and his eye peering out from under the goggles)
    for (u, v, t) in ((-1.6, 7.6, 14.0), (2.6, 7.4, -10.0)):
        hx, hy = cx + u * k, cy + v * k
        d.fill("art_cream", [(hx - 1.0 * k, hy - 0.6 * k), (hx + t * 0.04 * k, hy + 2.2 * k), (hx + 1.0 * k, hy - 0.6 * k)], ink=ink * 0.7, steps=1)
    ex, ey = cx + 3.6 * k, cy - 3.6 * k
    d.oval("art_white", ex, ey, 1.4 * k, 1.1 * k, ink=ink * 0.7)
    d.oval("art_ink", ex + 0.3 * k, ey - 0.1 * k, 0.6 * k, 0.6 * k, ink=0.0)
    place_name(d, a, lines=["Greenhorn"], where="below", room=5.0, caps=21.5)
