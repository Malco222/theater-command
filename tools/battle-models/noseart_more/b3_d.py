"""Batch 3, numbers 102 to 108 (painter b3_d): a robin flying home with her suitcase, a bulldog in an eyeshade dealing
cards, a woman at a jukebox, an old man kinged at checkers, a chickadee on watch in his flying helmet, one thunderbolt
out of a blue disc, and two bare fists up in the old stance. Flat shapes with noseart's Drawing API and the figure kit;
each name set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T, _lady  # noqa: E402


def _arc(cx, cy, rx, ry, a0, a1, n=12):
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
            for i in range(n + 1)]


# ---- 102: Robin's Return -----------------------------------------------------------------------------------------------------------
def suitcase(d, x, y, w, turn=0.0, leather="art_tan", strap="art_brown"):
    """A small suitcase, its handle's top at (x, y), `w` wide, hanging: the case, its two straps, its handle."""
    G = _T(x, y, turn)
    hgt = w * 0.66
    d.line(strap, [G(-w * 0.14, -w * 0.1), G(-w * 0.12, w * 0.02), G(w * 0.12, w * 0.02), G(w * 0.14, -w * 0.1)], w * 0.06, taper=False)
    d.fill(leather, [G(-w / 2, -w * 0.1), G(-w / 2, -w * 0.1 - hgt), G(w / 2, -w * 0.1 - hgt), G(w / 2, -w * 0.1)], steps=1)
    for u in (-w * 0.28, w * 0.28):
        d.shape(strap, [G(u - w * 0.05, -w * 0.1), G(u - w * 0.05, -w * 0.1 - hgt), G(u + w * 0.05, -w * 0.1 - hgt), G(u + w * 0.05, -w * 0.1)], ink=0.0)
    d.shape(None, [G(-w / 2, -w * 0.1), G(-w / 2, -w * 0.1 - hgt), G(w / 2, -w * 0.1 - hgt), G(w / 2, -w * 0.1)])
    d.oval("art_gold", *G(0.0, -w * 0.16), w * 0.05, w * 0.04, ink=0.0)


def robin(d, x, y, size, tilt=-6.0, back="art_grey", breast="art_orange", head="art_dark", beak="art_yellow"):
    """A robin flying right, `size` from tail to beak, her body's middle at (x, y), tilted `tilt`: grey-brown back and
    wings, her orange breast, a dark head with a white eye-ring and a yellow beak, her far wing up behind, her near wing
    raised, her tail fanned. Returns her feet (where she holds her suitcase)."""
    G0 = _T(x, y, tilt)
    k = size / 40.0
    G = lambda u, v: G0(u * k, v * k)                    # noqa: E731
    ink = d.ink
    wing(d, G(-1.0, 4.0), 112.0 + tilt, 18.0 * k, 7.0 * k, back, shade="art_dark", spread=-22)
    d.fill(back, [G(-10.0, 1.6), G(-19.0, 4.6), G(-21.0, 1.0), G(-20.0, -2.6), G(-10.0, -1.6)], ink=ink, steps=3)    # (her tail)
    for v in (2.4, 0.4, -1.6):
        d.line("art_dark", [G(-12.0, v * 0.5), G(-19.4, v)], 0.3 * k)
    d.fill(back, [G(-12.0, 0.4), G(-6.0, 5.0), G(4.0, 6.0), G(10.0, 3.6), G(11.0, -2.0), G(4.0, -6.0), G(-6.0, -4.0)], ink=ink, steps=4)
    d.fill(breast, [G(10.6, -0.4), G(9.6, -4.0), G(4.0, -6.2), G(-3.6, -4.4), G(-1.0, -1.4), G(5.0, 0.2)], ink=0.0, steps=4)
    d.oval(head, *G(11.0, 3.0), 5.0 * k, 4.6 * k, ink=ink)
    d.fill(beak, [G(15.2, 3.6), G(19.4, 2.2), G(15.4, 1.6)], ink=ink * 0.8, steps=1)
    d.oval("art_white", *G(12.8, 4.2), 1.3 * k, 1.3 * k, ink=0.0)
    d.oval("art_black", *G(13.1, 4.2), 0.7 * k, 0.75 * k, ink=0.0)
    wing(d, G(2.0, 4.6), 76.0 + tilt, 20.0 * k, 8.0 * k, back, shade="art_dark", spread=28)
    feet = G(2.0, -6.4)
    for du in (-0.8, 0.8):
        d.line("art_brown", [G(2.0 + du * 0.4, -5.6), G(2.0 + du, -7.4)], 0.5 * k, taper=False)
    return feet


def birdhouse(d, x, y, w, wood="art_red", roof="art_brown", post="art_brown"):
    """A birdhouse on its post, its floor's middle at (x, y), `w` wide: the box, its pitched roof, the round door and
    the perch under it; the post down from it. No word on it."""
    hgt = w * 1.0
    d.shape(post, [(x - w * 0.1, y - w * 1.6), (x - w * 0.1, y), (x + w * 0.1, y), (x + w * 0.1, y - w * 1.6)])
    d.shape(wood, [(x - w / 2, y), (x - w / 2, y + hgt), (x, y + hgt + w * 0.42), (x + w / 2, y + hgt), (x + w / 2, y)])
    d.shape(roof, [(x - w * 0.66, y + hgt - w * 0.1), (x, y + hgt + w * 0.56), (x + w * 0.66, y + hgt - w * 0.1), (x + w * 0.58, y + hgt - w * 0.22),
                   (x, y + hgt + w * 0.38), (x - w * 0.58, y + hgt - w * 0.22)])
    d.oval("art_black", x, y + hgt * 0.6, w * 0.18, w * 0.18, ink=d.ink * 0.6)
    d.line(post, [(x, y + hgt * 0.32), (x + w * 0.34, y + hgt * 0.32)], w * 0.06, taper=False)


@painting(102)
def p102(d, a):   # a robin with her suitcase flying toward her birdhouse
    birdhouse(d, 66.0, 26.0, 16.0)
    feet = robin(d, 28.0, 40.0, 46.0, tilt=-8.0)
    suitcase(d, feet[0] + 0.4, feet[1] - 0.6, 11.0, turn=-6.0)
    speed(d, 0.0, 44.0, 8.0, n=3, gap=3.4, paint="art_white", turn=-8.0, w=0.9)
    place_name(d, a, lines=["Robin's", "Return"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 103: Card Table Chester -----------------------------------------------------------------------------------------------------
def card(d, x, y, w, turn=0.0, face=True, suit="art_red"):
    """A playing card, its middle at (x, y), `w` wide, turned: face up a white card with one pip of its suit painted
    big in its middle (no letter, no number); face down its red back with a white border."""
    G = _T(x, y, turn)
    hgt = w * 1.4
    pts = [G(-w / 2, -hgt / 2), G(-w / 2, hgt / 2), G(w / 2, hgt / 2), G(w / 2, -hgt / 2)]
    if face:
        d.shape("art_white", pts, ink=d.ink * 0.7)
        cx, cy = G(0.0, 0.0)
        r = w * 0.22
        d.oval(suit, cx - r * 0.5, cy + r * 0.3, r * 0.6, r * 0.6, ink=0.0)
        d.oval(suit, cx + r * 0.5, cy + r * 0.3, r * 0.6, r * 0.6, ink=0.0)
        d.shape(suit, [G(-r * 1.04, r * 0.2), G(r * 1.04, r * 0.2), G(0.0, -r * 1.2)], ink=0.0)
    else:
        d.shape("art_white", pts, ink=d.ink * 0.7)
        d.shape(suit, [G(-w * 0.38, -hgt * 0.42), G(-w * 0.38, hgt * 0.42), G(w * 0.38, hgt * 0.42), G(w * 0.38, -hgt * 0.42)], ink=0.0)


def bulldog(d, x, y, size, coat="art_white", patch="art_tan", jowl="art_lgrey", shade_="art_green"):
    """A bulldog's head and shoulders, front on, (x, y) the middle of his face, `size` his head's width: the broad skull
    under a green eyeshade, a brown patch over one eye, small rose ears, a black nose, heavy jowls, his underbite with
    two teeth up; a cigar? none. Returns his face's frame."""
    k = size / 30.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    d.fill(coat, Ps([(-20.0, -22.0), (-18.0, -10.0), (-10.0, -6.0), (10.0, -6.0), (18.0, -10.0), (20.0, -22.0)]), ink=ink, steps=3)   # (his shoulders)
    d.fill(coat, Ps([(-15.0, 2.0), (-14.0, 10.0), (-8.0, 14.0), (8.0, 14.0), (14.0, 10.0), (15.0, 2.0), (13.0, -6.0), (6.0, -10.0), (-6.0, -10.0),
                     (-13.0, -6.0)]), ink=ink, steps=4)
    for su in (-1, 1):
        d.fill(patch, Ps([(su * 11.0, 12.0), (su * 17.0, 15.6), (su * 17.4, 11.0), (su * 14.0, 9.0)]), ink=ink, steps=2)             # (his ears)
    d.fill(patch, Ps([(-11.0, 2.0), (-11.0, 9.0), (-6.0, 11.0), (-2.4, 8.0), (-3.0, 2.6), (-7.0, 0.6)]), ink=0.0, steps=3)
    for su in (-1, 1):
        d.oval("art_white", *P(su * 5.6, 5.4), 2.4 * k, 2.0 * k, ink=ink * 0.7)
        d.oval("art_black", *P(su * 5.6 - 0.6, 5.0), 1.2 * k, 1.2 * k, ink=0.0)
        d.line("art_ink", Ps([(su * 3.4, 7.8), (su * 5.8, 8.8), (su * 8.0, 7.8)]), 0.6 * k)            # (his lids, heavy: a card player's)
    d.fill(jowl, Ps([(-13.0, -1.0), (-14.0, -6.0), (-8.0, -10.6), (0.0, -9.0), (8.0, -10.6), (14.0, -6.0), (13.0, -1.0), (6.0, 1.0), (0.0, 0.0),
                     (-6.0, 1.0)]), ink=ink, steps=4)
    d.oval("art_black", *P(0.0, 1.6), 3.4 * k, 2.2 * k, ink=ink * 0.6)
    d.line("art_ink", Ps([(0.0, -0.6), (0.0, -3.0)]), 0.6 * k)
    d.line("art_ink", Ps([(-8.0, -4.0), (-3.0, -3.0), (0.0, -3.6), (3.0, -3.0), (8.0, -4.0)]), 0.7 * k)
    for su in (-1, 1):
        d.fill("art_white", Ps([(su * 3.6, -4.0), (su * 4.0, -1.8), (su * 5.2, -3.8)]), ink=ink * 0.6, steps=1)
    # (his eyeshade: a green visor out over his brow, its band round his head)
    d.fill(shade_, Ps([(-16.0, 9.0), (-12.0, 13.4), (0.0, 15.0), (12.0, 13.4), (16.0, 9.0), (8.0, 11.4), (0.0, 12.0), (-8.0, 11.4)]), ink=ink, steps=4)
    d.line("art_dark", Ps([(-14.0, 13.6), (0.0, 15.8), (14.0, 13.6)]), 0.9 * k)
    return P


@painting(103)
def p103(d, a):   # a bulldog in a green eyeshade dealing cards
    x, y = 32.0, 40.0
    # (the card table's green top, seen across; its edge)
    d.shape("art_green", [(-4.0, 10.0), (4.0, 22.0), (60.0, 22.0), (68.0, 10.0)])
    d.line("art_brown", [(-4.0, 10.0), (68.0, 10.0)], 2.0, taper=False)
    P = bulldog(d, x, y, 28.0)
    # (his paws on the table, the deck in one, a card going off the other; the dealt cards before him)
    d.oval("art_white", *P(-12.0, -22.0), 4.0, 3.0, ink=d.ink)
    d.oval("art_white", *P(12.0, -21.0), 4.0, 3.0, ink=d.ink)
    for i in range(4):
        card(d, P(-12.6, -19.6)[0] + i * 0.3, P(-12.6, -19.6)[1] + i * 0.5, 6.0, turn=12.0, face=False)
    d.oval("art_white", *P(-12.0, -21.0), 3.4, 2.4, ink=d.ink * 0.8)
    for (u, v, turn) in ((18.0, -12.0, -30.0), (26.0, -4.0, -54.0)):
        card(d, *P(u, v), 6.0, turn=turn, face=False)
    for (u, v, turn, suit) in ((-4.0, -27.0, 8.0, "art_red"), (5.0, -27.4, -6.0, "art_black")):
        card(d, *P(u, v), 6.0, turn=turn, face=True, suit=suit)
    speed(d, P(24.0, -2.0)[0] + 3.0, P(24.0, -2.0)[1] + 3.0, 6.0, n=2, gap=2.4, paint="art_white", turn=-140, w=0.8)
    place_name(d, a, lines=["Card", "Table", "Chester"], where="right", room=6.0, at=34.0, gap=1.32, caps=21.5)


# ---- 104: Juke Box Judy -------------------------------------------------------------------------------------------------------------
def jukebox(d, x, y, w, body="art_brown", glow="art_yellow", band="art_red", grille="art_dark"):
    """A jukebox of the forties, front on, its foot's middle at (x, y), `w` wide: the cabinet with its round top,
    bands of light curving up its sides and over its top, the record window, the grille, the coin slot. Returns the
    slot."""
    hgt = w * 1.7
    top = _arc(x, y + hgt - w * 0.5, w * 0.5, w * 0.5, 0, 180, 14)
    d.fill(body, [(x + w * 0.5, y)] + top + [(x - w * 0.5, y)], ink=d.ink, steps=1)
    for f, paint in ((0.42, band), (0.34, glow)):
        arc_ = _arc(x, y + hgt - w * 0.5, w * f, w * f, 0, 180, 14)
        d.line(paint, [(x + w * f, y + w * 0.3)] + arc_ + [(x - w * f, y + w * 0.3)], w * 0.08, taper=False)
    d.fill("art_cream", _arc(x, y + hgt - w * 0.5, w * 0.24, w * 0.24, 0, 180, 10) + [(x - w * 0.24, y + hgt - w * 0.74), (x + w * 0.24, y + hgt - w * 0.74)],
           ink=d.ink * 0.6, steps=1)
    d.oval("art_black", x, y + hgt - w * 0.62, w * 0.14, w * 0.05, ink=0.0)
    d.shape(grille, [(x - w * 0.26, y + w * 0.22), (x - w * 0.26, y + w * 0.62), (x + w * 0.26, y + w * 0.62), (x + w * 0.26, y + w * 0.22)])
    for i in range(1, 5):
        xx = x - w * 0.26 + w * 0.52 * i / 5
        d.line(glow, [(xx, y + w * 0.24), (xx, y + w * 0.6)], w * 0.02, taper=False)
    slot = (x + w * 0.16, y + w * 0.78)
    d.shape("art_lsteel", [(slot[0] - w * 0.06, slot[1] - w * 0.05), (slot[0] - w * 0.06, slot[1] + w * 0.05), (slot[0] + w * 0.06, slot[1] + w * 0.05),
                           (slot[0] + w * 0.06, slot[1] - w * 0.05)])
    d.line("art_black", [(slot[0], slot[1] - w * 0.03), (slot[0], slot[1] + w * 0.03)], w * 0.015, taper=False)
    return slot


@painting(104)
def p104(d, a):   # a woman in a long skirt dropping a nickel into a jukebox
    slot = jukebox(d, 46.0, 2.0, 30.0)
    F = _lady(d, 22.0, 2.0, 56.0, 1, extra_arms={"_b3d_coin": ((0.7, -0.5), (1.4, -0.1))}, pose="together", arms=("_b3d_coin", "hip"),
              wear={"top": "art_white", "skirt": "art_red", "hem": "long", "sleeves": "puff", "belt": "art_red", "shoes": "art_red"},
              hair="rolls", hair_paint="art_hair")
    (hx, hy), _ = F.hand["fore"]
    d.oval("art_skin", hx, hy, F.r(0.15), F.r(0.13), ink=d.ink * 0.7)
    d.oval("art_lsteel", hx + 1.8, hy + 1.0, 1.1, 1.1, ink=d.ink * 0.6)       # (the nickel)
    for (dx, dy) in ((1.8, 3.4), (3.6, 2.6)):
        d.line("art_white", [(hx + dx, hy + dy), (hx + dx + 0.6, hy + dy + 1.2)], 0.4)
    for (nx_, ny_, hh, turn) in ((60.0, 52.0, 6.0, 0.0), (66.0, 44.0, 5.0, -10.0)):
        d.oval("art_white", nx_, ny_, hh * 0.2, hh * 0.14, turn=20 + turn, ink=d.ink * 0.7)
        d.line("art_white", [(nx_ + hh * 0.17, ny_ + hh * 0.04), (nx_ + hh * 0.17, ny_ + hh)], hh * 0.08, taper=False)
    place_name(d, a, lines=["Juke", "Box", "Judy"], where="right", room=6.0, at=34.0, gap=1.32, caps=21.5)


# ---- 105: King Me ------------------------------------------------------------------------------------------------------------------
def barrel(d, x, y, w, wood="art_brown", hoop="art_dark"):
    """A barrel standing on end, its foot's middle at (x, y), `w` wide: the bulging staves, three hoops."""
    hgt = w * 1.3
    d.fill(wood, [(x - w * 0.42, y), (x - w * 0.5, y + hgt * 0.5), (x - w * 0.42, y + hgt), (x + w * 0.42, y + hgt), (x + w * 0.5, y + hgt * 0.5),
                  (x + w * 0.42, y)], steps=4)
    for i in range(1, 5):
        u = -0.42 + 0.84 * i / 5
        d.line("art_lbrown", [(x + w * u, y + 0.6), (x + w * u * 1.12, y + hgt * 0.5), (x + w * u, y + hgt - 0.6)], 0.35, taper=False)
    for t, bulge in ((0.12, 0.44), (0.5, 0.5), (0.88, 0.44)):
        d.line(hoop, [(x - w * bulge, y + hgt * t), (x + w * bulge, y + hgt * t)], w * 0.05, taper=False)
    return y + hgt


def checkerboard(d, x, y, w, depth=0.36, dark="art_black", light="art_red", rim="art_tan"):
    """A checkerboard lying flat on a top at height y, its near edge's middle at (x, y), `w` wide, seen from a little
    above (`depth` how deep it looks): eight by eight squares."""
    D = w * depth
    def Q(u, v):                                          # (u across 0..1, v back 0..1)
        s = 1.0 - 0.18 * v
        return (x + (u - 0.5) * w * s, y + v * D)
    d.shape(rim, [(x - w * 0.54, y - 1.0), (x - w * 0.54 * 0.8, y + D + 0.8), (x + w * 0.54 * 0.8, y + D + 0.8), (x + w * 0.54, y - 1.0)])
    for i in range(8):
        for j in range(8):
            paint = dark if (i + j) % 2 == 0 else light
            d.shape(paint, [Q(i / 8, j / 8), Q(i / 8, (j + 1) / 8), Q((i + 1) / 8, (j + 1) / 8), Q((i + 1) / 8, j / 8)], ink=0.0)
    return Q


@painting(105)
def p105(d, a):   # an old man in suspenders at a checkerboard on a barrel, his checker crowned
    bx = 44.0
    top = barrel(d, bx, 2.0, 22.0)
    Q = checkerboard(d, bx, top, 28.0)
    # (the pieces: a few of each, and his crowned king, two stacked, in his hand going down)
    D = 28.0 * 0.36
    for (i, j, paint) in ((1, 6, "art_white"), (3, 6, "art_white"), (5, 7, "art_white"), (2, 1, "art_red"), (4, 1, "art_red"), (6, 2, "art_red")):
        cx, cy = Q((i + 0.5) / 8, (j + 0.5) / 8)
        d.oval(paint, cx, cy + 0.4, 1.4, 0.7, ink=d.ink * 0.6)
    F = man(d, 18.0, 2.0, 50.0, 1, pose="sit", arms=(to(2.9, 4.62, "down"), "rest"),
            wear={"shirt": "art_white", "sleeves": "rolled", "trousers": "art_grey", "braces": "art_red", "shoes": "art_brown", "belt": None},
            hair="bald", moustache="full", mouth="grin")
    (hx, hy), _ = F.hand["fore"]
    for dv in (0.0, 1.3):
        d.oval("art_red", hx + 1.4, hy - 0.4 + dv, 1.6, 0.8, ink=d.ink * 0.7)
    d.oval("art_skin", hx, hy, F.r(0.15) * 1.0, F.r(0.13), ink=d.ink * 0.7)
    for (u, v) in ((-0.18, 6.86), (0.2, 6.86)):                 # (his spectacles)
        d.line("art_ink", ellipse(*F.PH(u, v), F.rh(0.12), F.rh(0.1), 0, 20), F.rh(0.03), taper=False)
    # (the crown of light over it: three short rays)
    for ang in (60, 90, 120):
        r0, r1 = 3.4, 5.6
        d.line("art_yellow", [(hx + 1.4 + r0 * math.cos(math.radians(ang)), hy + 1.0 + r0 * math.sin(math.radians(ang))),
                              (hx + 1.4 + r1 * math.cos(math.radians(ang)), hy + 1.0 + r1 * math.sin(math.radians(ang)))], 0.7)
    place_name(d, a, lines=["King", "Me"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 106: Pine Tree Patrol -----------------------------------------------------------------------------------------------------------
def pine_bough(d, x0, y0, x1, y1, wood="art_brown", needles="art_dgreen", tips="art_green"):
    """A pine bough from (x0, y0) to (x1, y1): the branch, tufts of needles along it, and one cone hanging."""
    d.limb(wood, [(x0, y0), lerp((x0, y0), (x1, y1), 0.5), (x1, y1)], [2.0, 1.6, 1.0], ink=d.ink)
    for i in range(9):
        t = 0.08 + 0.92 * i / 8
        px, py = lerp((x0, y0), (x1, y1), t)
        for (ang, L) in ((70 + i * 3, 7.0), (110 - i * 3, 6.4), (-60 - i * 2, 6.0), (-120, 5.4)):
            a_ = math.radians(ang)
            d.line(needles if ang < 0 else tips, [(px, py), (px + L * math.cos(a_), py + L * math.sin(a_))], 1.1)
    cx, cy = lerp((x0, y0), (x1, y1), 0.7)
    d.fill("art_lbrown", [(cx - 2.2, cy - 2.0), (cx - 2.6, cy - 6.0), (cx, cy - 10.0), (cx + 2.6, cy - 6.0), (cx + 2.2, cy - 2.0)], ink=d.ink, steps=3)
    for v in (-4.0, -6.4, -8.4):
        d.line("art_brown", [(cx - 2.0, cy + v), (cx, cy + v - 0.8), (cx + 2.0, cy + v)], 0.4)


def chickadee(d, x, y, size, back="art_grey", cap="art_black", cheek="art_white", side="art_tan", belly="art_cream"):
    """A chickadee perched, facing right, side on: a round body, his black cap and bib, white cheek, grey back and
    wing, buff sides, his tail down behind; feet at (x, y), `size` his height. Returns his head's middle and width."""
    k = size / 30.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    d.fill(back, Ps([(-6.0, 6.0), (-14.0, -4.0), (-12.0, -5.4), (-4.0, 3.0)]), ink=ink, steps=2)                    # (his tail)
    d.fill(back, Ps([(-8.0, 8.0), (-6.0, 15.0), (0.0, 18.0), (7.0, 15.0), (9.0, 8.0), (6.0, 2.0), (0.0, 0.8), (-6.0, 2.6)]), ink=ink, steps=4)
    d.fill(belly, Ps([(8.8, 10.0), (8.6, 6.0), (5.0, 1.6), (0.0, 1.2), (2.0, 6.0), (5.0, 10.0)]), ink=0.0, steps=3)
    d.fill(side, Ps([(-3.0, 3.0), (0.0, 1.2), (3.0, 3.6), (1.0, 6.4), (-3.0, 6.0)]), ink=0.0, steps=3)
    d.fill(back, Ps([(-7.0, 12.0), (-1.0, 14.0), (5.0, 11.0), (2.0, 6.0), (-6.0, 5.0)]), ink=ink * 0.8, steps=3)           # (his wing)
    for v in (7.4, 9.2):
        d.line("art_white", Ps([(-5.0, v), (2.0, v + 1.0)]), 0.4 * k)
    hx, hy, hr = 5.0, 20.0, 7.0
    d.oval(cheek, *P(hx, hy), hr * k, hr * 0.92 * k, ink=ink)
    d.fill(cap, Ps([(hx - hr, hy + 0.4), (hx - hr * 0.7, hy + 4.8), (hx, hy + hr * 0.92), (hx + hr * 0.8, hy + 4.0), (hx + hr, hy + 1.0),
                    (hx + 2.0, hy + 1.6), (hx - 3.0, hy + 1.2)]), ink=0.0, steps=4)
    d.fill(cap, Ps([(hx + 1.0, hy - 3.2), (hx + 5.0, hy - 3.4), (hx + 3.4, hy - 7.0), (hx + 0.4, hy - 6.0)]), ink=0.0, steps=3)    # (his bib)
    d.oval("art_black", *P(hx + 3.6, hy + 0.2), 1.0 * k, 1.1 * k, ink=0.0)
    d.oval("art_white", *P(hx + 3.9, hy + 0.6), 0.3 * k, 0.3 * k, ink=0.0)
    d.fill("art_dark", Ps([(hx + hr - 0.4, hy - 0.4), (hx + hr + 3.0, hy - 1.0), (hx + hr - 0.4, hy - 1.8)]), ink=ink * 0.6, steps=1)
    for du in (-1.0, 1.4):
        d.line("art_dark", Ps([(du, 1.4), (du + 0.4, -0.6)]), 0.5 * k, taper=False)
    return P(hx, hy), 2 * hr * k


@painting(106)
def p106(d, a):   # a chickadee in a flying helmet keeping watch from a pine bough
    pine_bough(d, 0.0, 6.0, 56.0, 14.0)
    (hx, hy), hw = chickadee(d, 26.0, 10.8, 54.0)
    helmet(d, hx - 0.4, hy + 0.8, hw * 0.9, 1, paint="art_brown", view="three", inside=None)
    place_name(d, a, lines=["Pine", "Tree", "Patrol"], where="right", room=6.0, at=34.0, gap=1.32, caps=21.5)


# ---- 107: Bolt from the Blue -------------------------------------------------------------------------------------------------------
@painting(107)
def p107(d, a):   # one yellow thunderbolt striking down out of a small blue disc
    cx, cy, r = 14.0, 60.0, 12.0
    d.oval("art_blue", cx, cy, r, r, ink=d.ink)
    d.line("art_white", _arc(cx, cy, r * 0.72, r * 0.72, 110, 170, 6), 0.9)
    # (the bolt: the sign painter's zigzag, out of the disc's lower side and down to the right)
    G = _T(cx + 4.0, cy - 6.0, 28.0)
    k = 2.1
    bolt = [(0.0, 0.0), (7.0, 0.0), (3.0, -9.0), (8.0, -9.0), (-3.0, -26.0), (0.0, -13.0), (-5.0, -13.0)]
    d.shape("art_yellow", [G(u * k, v * k) for u, v in bolt])
    tip = G(-3.0 * k, -26.0 * k)
    for ang in (150, 200, 250, 300, 350, 40):            # (where it strikes)
        a_ = math.radians(ang)
        d.line("art_white", [(tip[0] + 2.6 * math.cos(a_), tip[1] + 2.6 * math.sin(a_)), (tip[0] + 6.4 * math.cos(a_), tip[1] + 6.4 * math.sin(a_))], 1.0)
    place_name(d, a, lines=["Bolt", "from the", "Blue"], where="right", room=6.0, at=32.0, gap=1.32, caps=21.5)


# ---- 108: Bare Knuckles ----------------------------------------------------------------------------------------------------------
def fist(d, x, y, w, s=1, skin="art_skin", turn=0.0):
    """A clenched fist seen from the front, its knuckles up, its middle at (x, y), `w` wide, its thumb across to side s:
    the four curled fingers side by side, their knuckles' bumps along its top, the thumb folded over their front."""
    G = _T(x, y, turn)
    ink = d.ink
    hgt = w * 0.9
    d.fill(skin, [G(-w * 0.5, -hgt * 0.5), G(-w * 0.54, hgt * 0.2), G(-w * 0.4, hgt * 0.5), G(w * 0.4, hgt * 0.54), G(w * 0.54, hgt * 0.2),
                  G(w * 0.5, -hgt * 0.5)], ink=ink, steps=3)
    for i in range(1, 4):
        u = -w * 0.5 + w * i / 4
        d.line("art_ink", [G(u, hgt * 0.5), G(u, -hgt * 0.04)], w * 0.03)
    for i in range(4):
        u = -w * 0.5 + w * (i + 0.5) / 4
        d.line("art_ink", [G(u - w * 0.08, hgt * 0.12), G(u, hgt * 0.08), G(u + w * 0.08, hgt * 0.12)], w * 0.025)
    d.fill(skin, [G(-s * w * 0.56, -hgt * 0.1), G(-s * w * 0.3, hgt * 0.02), G(s * w * 0.14, -hgt * 0.06), G(s * w * 0.2, -hgt * 0.22),
                  G(-s * w * 0.1, -hgt * 0.3), G(-s * w * 0.54, -hgt * 0.36)], ink=ink * 0.8, steps=3)
    d.line("art_ink", [G(s * w * 0.06, -hgt * 0.14), G(s * w * 0.12, -hgt * 0.16)], w * 0.03)


@painting(108)
def p108(d, a):   # a pair of bare fists up in the old prize-fighter's stance, sleeves rolled
    for (x, y, w, s_, lean) in ((22.0, 42.0, 25.0, 1, 4.0), (50.0, 35.0, 27.0, -1, -3.0)):
        ex = x - lean                                     # (his elbow, low; the forearm up from it to the fist)
        d.limb("art_white", [(ex - lean * 0.6, -1.0), (ex, 10.0)], [w * 0.42, w * 0.4], ink=d.ink)         # (the shirt sleeve)
        d.limb("art_skin", [(ex, 12.0), lerp((ex, 12.0), (x, y - w * 0.4), 0.5), (x, y - w * 0.4)], [w * 0.3, w * 0.27, w * 0.25], ink=d.ink)
        d.fill("art_white", [(ex - w * 0.46, 8.0), (ex - w * 0.48, 13.6), (ex + w * 0.48, 14.2), (ex + w * 0.46, 8.6)], ink=d.ink, steps=2)   # (its roll)
        d.line("art_lgrey", [(ex - w * 0.4, 11.0), (ex + w * 0.4, 11.4)], 0.6, taper=False)
        fist(d, x, y, w, s_, turn=-lean * 0.6)
    for (u, v, ang) in ((6.0, 64.0, 140), (64.0, 56.0, 40), (68.0, 44.0, 10)):
        a_ = math.radians(ang)
        d.line("art_white", [(u, v), (u + 4.0 * math.cos(a_), v + 4.0 * math.sin(a_))], 0.9)
    place_name(d, a, lines=["Bare", "Knuckles"], where="right", room=6.0, at=30.0, caps=21.5)
