"""Batch 4, numbers 148 to 154 (painter b4_b): a ferry pilot in her flying suit and parachute harness with her logbook,
a stonecutter chiselling a bomb out of a granite block, a scarecrow in a flying helmet with a crow on each arm, a
raccoon tiptoeing off with a pie, a rainbow ending in a kit bag, a prospector woman panning one big silver star and
three horseshoes in a row, the third one gold. Flat shapes with noseart's Drawing API and the figure kit; each name set
clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b2_f import kit_bag  # noqa: E402
from noseart_more.b3_a import _T, _lady  # noqa: E402


# ---- 148: Ferry Pilot Flora ----------------------------------------------------------------------------------------------------------
@painting(148)
def p148(d, a):   # a woman ferry pilot in her flying suit and parachute harness, her logbook under her arm
    ink = d.ink
    x, y, h = 24.0, 2.0, 74.0
    kw = dict(pose="stand", arms=("wave", "down"),
              wear={"top": "art_khaki", "trousers": "art_khaki", "sleeves": "long", "collar": "art_brown", "belt": "art_khaki",
                    "tall": "art_brown"},
              hair="rolls", hair_paint="art_auburn")
    F0 = lady(Drawing(1), x, y, h, 1, **kw)
    # (her parachute's seat pack, slung behind her hips: drawn first, so that she stands in front of it)
    d.fill("art_brown", F0.Ps([(-0.7, 3.0), (-1.16, 3.4), (-1.22, 4.2), (-0.96, 4.7), (-0.2, 4.7), (-0.1, 3.0)]), ink=ink, steps=3)
    d.line("art_ink", F0.Ps([(-1.1, 3.86), (-0.3, 3.86)]), 0.4, taper=False)
    F = lady(d, x, y, h, 1, **kw)
    P, R = F.P, F.r
    # (her suit's zip, down its front)
    d.line("art_brown", F.Ps([(0.16, 5.56), (0.12, 4.3), (0.1, 3.6)]), 0.35, taper=False)
    # (the harness: two straps over her shoulders down to her legs, the chest strap, the buckle; the leg loops)
    for su in (-1.0, 1.0):
        d.line("art_brown", F.Ps([(su * 0.34, 5.66), (su * 0.3, 4.9), (su * 0.26, 4.2), (su * 0.22, 3.5)]), 1.3, taper=False)
    d.line("art_brown", F.Ps([(-0.42, 5.02), (0.46, 5.02)]), 1.2, taper=False)
    d.line("art_brown", F.Ps([(-0.46, 3.5), (-0.1, 3.36), (0.26, 3.4), (0.5, 3.56)]), 1.2, taper=False)
    d.oval("art_lsteel", *P(0.06, 5.02), R(0.08), R(0.07), ink=ink * 0.6)
    d.oval("art_lsteel", *P(0.02, 4.2), R(0.1), R(0.09), ink=ink * 0.6)
    # (her logbook in her hand at her side: dark covers, its pages' edge)
    (hx, hy), _ = F.hand["aft"]
    d.fill("art_brown", [(hx - 3.6, hy - 6.4), (hx - 3.6, hy + 3.4), (hx + 3.0, hy + 3.6), (hx + 3.0, hy - 6.2)], ink=ink, steps=1)
    d.line("art_khaki", [(hx + 3.4, hy - 5.8), (hx + 3.4, hy + 3.2)], 0.8, taper=False)
    d.oval("art_skin", hx, hy, R(0.15), R(0.13), ink=ink * 0.7)
    place_name(d, a, lines=["Ferry", "Pilot", "Flora"], where="right", room=6.0, at=40.0, gap=1.32, caps=21.5)


# ---- 149: Granite State Gil ----------------------------------------------------------------------------------------------------------
@painting(149)
def p149(d, a):   # a stonecutter with his mallet up, his chisel at a bomb half cut out of a block of granite
    ink = d.ink
    x, y, h = 14.0, 2.0, 70.0
    hd = h / 7.42                                        # (one of his heads, in the drawing's units)
    bx0, bx1, btop = x + 13.6, x + 40.0, y + 4.2 * hd
    # (the block: grey granite, its flecks, its cut face rough at the top where the bomb comes out of it)
    d.fill("art_grey", [(bx0, y - 1.0), (bx0, btop), (bx0 + 4.0, btop + 1.4), (bx0 + 9.0, btop + 0.4), (bx1 - 8.0, btop + 1.2), (bx1 - 3.0, btop),
                        (bx1, btop + 0.8), (bx1, y - 1.0)], ink=ink, steps=3)
    rng = d.rng
    for _ in range(26):
        fx, fy = bx0 + 1.5 + rng.random() * (bx1 - bx0 - 3.0), y + 1.0 + rng.random() * (btop - y - 3.0)
        d.oval(rng.choice(("art_lgrey", "art_dark")), fx, fy, 0.45, 0.35, ink=0.0)
    d.line("art_lgrey", [(bx0 + 0.6, y), (bx0 + 0.6, btop - 0.6)], 0.5, taper=False)
    # (the bomb rising out of it, its nose up, the stone still round its tail)
    mx = (bx0 + bx1) / 2 + 1.0
    bomb(d, mx, btop + 22.0, 28.0, turn=90.0, paint="art_lgrey", band="art_white", fins=False, ink=ink)
    d.fill("art_grey", [(mx - 7.0, btop - 0.4), (mx - 6.2, btop + 3.0), (mx - 3.0, btop + 1.6), (mx, btop + 3.4), (mx + 3.4, btop + 1.4), (mx + 6.6, btop + 2.8),
                        (mx + 7.2, btop - 0.4)], ink=ink * 0.8, steps=2)
    # (Gil: cap, rolled sleeves, leather apron, his mallet raised in his aft hand, his chisel held to the stone)
    F = man(d, x, y, h, 1, pose="wide", arms=(to(1.42, 4.6, "down"), "raise"),
            wear={"shirt": "art_blue", "sleeves": "rolled", "trousers": "art_brown", "boots": "art_brown", "apron": "art_tan", "apron_cut": "bib"},
            hat="cap", hair="short", mouth="flat")
    (cx, cy), _ = F.hand["fore"]
    d.line("art_lsteel", [(cx - 1.0, cy - 0.6), (bx0 + 1.2, btop + 3.0)], 1.0, taper=False)
    F.grip("fore")
    (mx_, my_), _ = F.hand["aft"]
    d.line("art_tan", [(mx_ - 1.4, my_ - 3.0), (mx_ + 1.0, my_ + 8.0)], 1.0, taper=False)
    G = _T(mx_ + 1.4, my_ + 9.6, 12.0)
    d.fill("art_brown", [G(-3.4, -2.0), G(-3.4, 2.0), G(3.4, 2.0), G(3.4, -2.0)], ink=ink, steps=1)
    F.grip("aft")
    # (chips flying off the stone)
    for (u, v, r) in ((3.0, 3.6, 0.7), (5.4, 1.2, 0.5), (4.0, -1.6, 0.55)):
        d.fill("art_lgrey", [(bx0 + u - r, btop + v), (bx0 + u, btop + v + r * 1.2), (bx0 + u + r, btop + v - r * 0.4)], ink=ink * 0.6, steps=1)
    place_name(d, a, lines=["Granite", "State", "Gil"], where="right", room=6.0, at=38.0, gap=1.32, caps=21.5)


# ---- 150: Scarecrow Sentry -----------------------------------------------------------------------------------------------------------
def crow(d, x, y, size, s=1, paint="art_black", beak="art_lgrey", open_beak=False):
    """A crow perched, its feet at (x, y), `size` tall, facing s: a glossy body, a wedge of tail, a round head with a
    stout pale beak and one bright eye, its toes round the perch."""
    k = size / 12.0
    P = lambda u, v: (x + s * u * k, y + v * k)          # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    d.fill(paint, Ps([(-2.0, 3.6), (-6.4, 1.0), (-7.0, 2.0), (-3.0, 5.4)]), ink=ink * 0.8, steps=1)
    d.fill(paint, Ps([(-3.6, 4.6), (-2.6, 7.6), (0.4, 9.0), (3.0, 8.2), (3.4, 5.6), (1.4, 2.6), (-1.6, 2.4)]), ink=ink * 0.8, steps=3)
    d.line("art_grey", Ps([(-2.4, 6.4), (-0.4, 5.0), (1.6, 4.6)]), 0.35 * k)
    d.oval(paint, *P(2.6, 9.6), 2.2 * k, 2.0 * k, ink=ink * 0.8)
    if open_beak:
        d.fill(beak, Ps([(4.2, 10.4), (7.4, 11.2), (4.6, 9.6)]), ink=ink * 0.6, steps=1)
        d.fill(beak, Ps([(4.2, 9.4), (6.8, 8.4), (4.4, 9.0)]), ink=ink * 0.6, steps=1)
    else:
        d.fill(beak, Ps([(4.2, 10.4), (7.2, 9.6), (4.2, 8.8)]), ink=ink * 0.6, steps=1)
    d.oval("art_white", *P(3.0, 10.2), 0.55 * k, 0.55 * k, ink=0.0)
    d.oval("art_ink", *P(3.2, 10.2), 0.28 * k, 0.3 * k, ink=0.0)
    for u in (-0.6, 0.8):
        d.line("art_lgrey", Ps([(u, 2.6), (u + 0.2, 0.4)]), 0.35 * k)
        d.line("art_lgrey", Ps([(u - 0.6, 0.0), (u + 0.8, 0.2)]), 0.3 * k)


@painting(150)
def p150(d, a):   # a scarecrow in a flying helmet on his pole, his sleeves drooping, a crow perched on each arm
    ink = d.ink
    cx, bar = 24.0, 60.0
    # (the pole and the short crossbar under his coat)
    d.shape("art_brown", [(cx - 1.4, 9.0), (cx - 1.4, bar + 6.0), (cx + 1.4, bar + 6.0), (cx + 1.4, 9.0)])
    d.fill("art_brown", [(cx - 17.0, bar - 4.0), (cx - 17.0, bar - 1.6), (cx, bar + 1.2), (cx + 17.0, bar - 1.6), (cx + 17.0, bar - 4.0), (cx, bar - 1.2)], ink=0.0, steps=1)
    # (his trousers hanging under the coat, straw at the ankles; a patch on one knee)
    for su in (-1.0, 1.0):
        d.fill("art_brown", [(cx + su * 0.4, 38.0), (cx + su * 7.2, 38.0), (cx + su * 6.6, 18.0), (cx + su * 2.4, 17.4)], ink=ink, steps=2)
        for t in (-1.6, 0.0, 1.6):
            ax = cx + su * 4.6 + t
            d.line("art_yellow", [(ax, 18.0), (ax + t * 0.5, 13.0)], 0.7)
    d.shape("art_tan", [(cx + 2.8, 28.0), (cx + 6.0, 28.0), (cx + 5.8, 24.4), (cx + 3.0, 24.6)])
    d.line("art_ink", [(cx + 3.0, 26.2), (cx + 5.8, 26.2)], 0.25)
    # (his coat on the crossbar: its sleeves drooping along it, straw at the cuffs; a patch; a rope belt)
    for su in (-1.0, 1.0):
        d.fill("art_blue", [(cx + su * 5.0, bar + 3.6), (cx + su * 16.6, bar - 1.2), (cx + su * 16.0, bar - 7.4), (cx + su * 6.4, bar - 4.6)], ink=ink, steps=2)
        for t in (-2.0, 0.0, 2.0):
            d.line("art_yellow", [(cx + su * 16.4, bar - 4.2 + t), (cx + su * 19.8, bar - 6.2 + t * 1.4)], 0.7)
    d.fill("art_blue", [(cx - 8.0, bar + 4.0), (cx + 8.0, bar + 4.0), (cx + 9.0, 36.0), (cx + 4.0, 35.0), (cx, 36.6), (cx - 4.4, 35.0), (cx - 9.0, 36.4)],
           ink=ink, steps=3)
    d.shape("art_red", [(cx - 6.2, 49.0), (cx - 6.2, 53.4), (cx - 2.2, 53.6), (cx - 2.0, 49.2)])
    for u in (-5.8, -2.4):
        d.line("art_ink", [(cx + u, 49.6), (cx + u, 52.8)], 0.2)
    d.line("art_tan", [(cx - 8.6, 41.0), (cx, 40.2), (cx + 8.6, 41.0)], 0.9, taper=False)
    for t in (-1.2, 0.0, 1.2):
        d.line("art_yellow", [(cx + t * 2.0, bar + 4.4), (cx + t * 3.0, bar + 6.4)], 0.6)
    # (his head: a stuffed sack, stitched mouth, button eyes; his flying helmet)
    hy = bar + 12.0
    d.oval("art_tan", cx, hy, 7.0, 7.6, ink=ink)
    for su in (-1.0, 1.0):
        d.oval("art_black", cx + su * 2.6, hy + 0.6, 1.2, 1.2, ink=0.0)
        d.oval("art_lgrey", cx + su * 2.4, hy + 0.9, 0.35, 0.35, ink=0.0)
    d.line("art_ink", [(cx - 3.4, hy - 3.6), (cx, hy - 4.4), (cx + 3.4, hy - 3.6)], 0.4)
    for u in (-2.4, -0.8, 0.8, 2.4):
        dv = 0.4 if abs(u) < 1 else 0.0
        d.line("art_ink", [(cx + u, hy - 3.2 - dv), (cx + u, hy - 4.6 - dv)], 0.3, taper=False)
    helmet(d, cx, hy + 1.6, 7.4, 1, paint="art_brown", view="front", inside=None)
    # (the two crows: one on each drooping sleeve, facing out, the fore one cawing)
    crow(d, cx - 12.6, bar - 1.4, 15.0, -1)
    crow(d, cx + 12.6, bar - 1.4, 15.0, 1, open_beak=True)
    place_name(d, a, lines=["Scarecrow", "Sentry"], where="below", room=4.0, caps=21.5)


# ---- 151: Midnight Raccoon -----------------------------------------------------------------------------------------------------------
def raccoon(d, x, y, size, coat="art_grey", mask="art_black", white="art_white"):
    """A raccoon up on his hind legs tiptoeing to the right, a pie held up in his front paws: his feet at (x, y),
    `size` his height to his ears. Ringed tail up behind, black mask over his eyes, white brows and muzzle, small round
    ears. Returns the middle of the pie he carries."""
    k = size / 40.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    # (his tail: up behind him, ringed black)
    tail = Ps([(-3.0, 11.0), (-8.0, 11.0), (-12.0, 14.0), (-14.0, 19.0), (-13.0, 23.0)])
    d.limb(coat, tail, [2.6 * k, 2.8 * k, 3.0 * k, 2.8 * k, 2.2 * k], ink=ink)
    for t in (0.2, 0.42, 0.64, 0.86):
        i = int(t * 4)
        p = lerp(tail[i], tail[i + 1], t * 4 - i)
        d.oval(mask, p[0], p[1], 1.4 * k, 2.6 * k, turn=-40 + 60 * t, ink=0.0)
    d.oval(mask, *tail[-1], 1.8 * k, 1.6 * k, ink=ink * 0.7)
    # (his far leg, stepping back on its toes; his body, a pear leaning forward; his near leg on tiptoe)
    d.limb(coat, Ps([(-1.4, 12.0), (-4.0, 6.4), (-6.6, 2.0)]), [3.0 * k, 2.0 * k, 1.4 * k], ink=ink)
    d.oval(mask, *P(-6.0, 0.9), 2.0 * k, 0.9 * k, turn=-30, ink=ink * 0.6)
    d.fill(coat, Ps([(-5.0, 11.0), (-5.6, 16.0), (-3.0, 22.6), (1.0, 25.4), (5.0, 22.6), (5.4, 15.4), (3.6, 10.0), (-0.6, 8.6)]), ink=ink, steps=4)
    d.fill(white, Ps([(1.0, 22.6), (3.6, 19.0), (3.4, 13.6), (1.6, 10.6), (-0.4, 14.0), (-0.6, 19.0)]), ink=0.0, steps=4)
    d.limb(coat, Ps([(1.6, 11.0), (2.8, 5.6), (3.6, 1.6)]), [3.0 * k, 2.2 * k, 1.5 * k], ink=ink)
    d.oval(mask, *P(4.4, 0.9), 2.0 * k, 0.9 * k, turn=10, ink=ink * 0.6)
    # (the pie, held up before him: its dish, its golden crust domed, its lattice, steam rising)
    px, py = P(9.4, 20.6)
    d.fill("art_lsteel", ellipse(px, py, 7.0 * k, 1.8 * k, 0, 16, 180, 360), ink=ink * 0.8, steps=2)
    d.fill("art_gold", ellipse(px, py, 6.4 * k, 3.2 * k, 0, 16, 0, 180) + [(px - 6.4 * k, py)], ink=ink * 0.8, steps=2)
    for t in (-3.6, -1.2, 1.2, 3.6):
        d.line("art_lbrown", [(px + t * k - 0.8 * k, py + 0.2 * k), (px + t * k + 0.8 * k, py + 2.8 * k - abs(t) * 0.35 * k)], 0.4 * k)
    for t in (-2.6, 0.0, 2.6):
        d.line("art_white", [(px + t * k, py + 4.0 * k), (px + t * k + 0.8 * k, py + 6.0 * k), (px + t * k - 0.4 * k, py + 8.0 * k), (px + t * k + 0.4 * k, py + 9.6 * k)],
               0.35 * k)
    # (his arms: his paws under the dish)
    for (u0, v0, u1, v1) in ((2.0, 21.0, 5.8, 19.0), (3.4, 20.0, 7.6, 18.6)):
        d.limb(coat, Ps([(u0, v0), (u1 - 1.6, v1 - 0.6), (u1 + 1.2, v1 + 0.6)]), [1.6 * k, 1.3 * k, 1.1 * k], ink=ink)
        d.oval(mask, *P(u1 + 1.6, v1 + 0.9), 1.2 * k, 0.9 * k, ink=ink * 0.6)
    # (his head: turned to us over the pie, round ears, mask, white brows and muzzle, black nose; a sly look)
    hx, hy, hr = 2.2, 29.6, 6.0
    for su in (-1, 1):
        d.oval(coat, *P(hx + su * 4.0, hy + 4.4), 2.0 * k, 2.2 * k, ink=ink)
        d.oval(white, *P(hx + su * 4.0, hy + 4.4), 1.0 * k, 1.1 * k, ink=0.0)
    d.fill(coat, Ps([(hx - hr, hy + 0.4), (hx - hr * 0.8, hy + 4.0), (hx, hy + hr * 0.92), (hx + hr * 0.8, hy + 4.0), (hx + hr, hy + 0.4), (hx + hr * 0.7, hy - 3.4),
                     (hx + 1.6, hy - hr + 0.6), (hx - 1.6, hy - hr + 0.6), (hx - hr * 0.7, hy - 3.4)]), ink=ink, steps=4)
    d.fill(white, Ps([(hx - 4.4, hy + 1.6), (hx - 2.6, hy + 3.4), (hx - 0.6, hy + 2.4), (hx + 0.6, hy + 2.4), (hx + 2.6, hy + 3.4), (hx + 4.4, hy + 1.6),
                      (hx + 2.6, hy + 2.0), (hx, hy + 1.4), (hx - 2.6, hy + 2.0)]), ink=0.0, steps=2)
    d.fill(mask, Ps([(hx - 5.6, hy + 0.4), (hx - 4.0, hy + 1.6), (hx - 1.0, hy + 0.8), (hx, hy - 0.2), (hx + 1.0, hy + 0.8), (hx + 4.0, hy + 1.6), (hx + 5.6, hy + 0.4),
                     (hx + 3.6, hy - 1.8), (hx + 1.0, hy - 1.2), (hx, hy - 1.6), (hx - 1.0, hy - 1.2), (hx - 3.6, hy - 1.8)]), ink=0.0, steps=3)
    d.oval(white, *P(hx, hy - 3.4), 2.8 * k, 1.8 * k, ink=0.0)
    d.oval("art_ink", *P(hx, hy - 2.6), 1.0 * k, 0.7 * k, ink=0.0)
    d.line("art_ink", Ps([(hx - 1.4, hy - 4.2), (hx, hy - 4.6), (hx + 1.6, hy - 4.0)]), 0.35 * k)
    for su in (-1, 1):
        ex = hx + su * 2.4
        d.oval("art_white", *P(ex, hy - 0.1), 0.9 * k, 0.6 * k, ink=0.0)
        d.oval("art_ink", *P(ex + 0.4, hy - 0.1), 0.45 * k, 0.5 * k, ink=0.0)
    return px, py


@painting(151)
def p151(d, a):   # a raccoon tiptoeing off into the night with a pie, a crescent moon and a few stars behind him
    from noseart_more.b3_a import moon_shape
    d.shape("art_cream", moon_shape(10.0, 66.0, 9.0))
    for (u, v, r) in ((26.0, 70.0, 1.8), (36.0, 62.0, 1.3), (4.0, 50.0, 1.4)):
        twinkle(d, u, v, r, "art_white")
    raccoon(d, 22.0, 2.0, 58.0)
    place_name(d, a, lines=["Midnight", "Raccoon"], where="right", room=6.0, at=38.0, caps=21.5)


# ---- 152: Rainbow's End --------------------------------------------------------------------------------------------------------------
@painting(152)
def p152(d, a):   # a rainbow arching out of a cloud and ending in a kit bag, sparkling at its neck, where the gold should be
    ink = d.ink
    cx, cy, R = 34.0, 4.0, 30.0
    bands = ("art_red", "art_orange", "art_yellow", "art_green", "art_blue")
    w = 2.6
    for i, paint in enumerate(bands):
        ro, ri = R - i * w, R - (i + 1) * w
        outer = ellipse(cx, cy, ro, ro * 0.96, 0, 36, 8, 178)
        inner = list(reversed(ellipse(cx, cy, ri, ri * 0.96, 0, 36, 8, 178)))
        d.shape(paint, outer + inner)
    d.line("art_ink", ellipse(cx, cy, R, R * 0.96, 0, 36, 8, 178), 0.5, taper=False)
    d.line("art_ink", ellipse(cx, cy, R - 5 * w, (R - 5 * w) * 0.96, 0, 36, 8, 178), 0.4, taper=False)
    cloud(d, cx - R + 6.0, cy + 5.0, 22.0, 11.0, "art_white", seed=152)
    # (the kit bag where the rainbow comes down, its neck open, the bands running into it; sparkles at its mouth)
    bx = cx + R - 6.0
    kit_bag(d, (bx, cy + 12.0), (bx + 1.0, cy - 6.0), 16.0, paint="art_khaki")
    for (u, v, r) in ((-6.0, 17.0, 1.8), (5.0, 18.0, 2.2), (8.6, 12.0, 1.4), (-1.0, 21.0, 1.4)):
        twinkle(d, bx + u, cy + v, r, "art_yellow")
    place_name(d, a, lines=["Rainbow's", "End"], where="below", room=5.0, caps=21.5)


# ---- 153: Silver State Sadie ---------------------------------------------------------------------------------------------------------
@painting(153)
def p153(d, a):   # a prospector woman standing in a stream in her boots, holding up her pan with one big silver star in it
    ink = d.ink
    x, y, h = 24.0, 6.0, 72.0
    F = _lady(d, x, y, h, 1, extra_arms={"_b4b_pan": ((-0.2, -1.12), (-1.7, -0.96))}, pose="stride", arms=("offer", "_b4b_pan"),
             wear={"top": "art_red", "sleeves": "rolled", "collar": "art_red", "buttons": "art_dark", "trousers": "art_blue", "tall": "art_brown",
                   "belt": "art_brown", "buckle": "art_gold"},
             hair="rolls", hair_paint="art_black", hat="cowgirl")
    # (the stream about her boots: its water, the white of its ripples)
    waves(d, 2.0, 50.0, 10.0, 6.0, paint="art_blue", cap="art_white", n=4)
    # (her pan, tipped toward us: its rim, its dark inside, one big silver star lying in it, a glint)
    (hx, hy), _ = F.hand["fore"]
    (gx, gy), _ = F.hand["aft"]
    px, py = (hx + gx) / 2, (hy + gy) / 2 + 1.0
    d.fill("art_lsteel", ellipse(px, py, 9.0, 4.2, -8, 24), ink=ink, steps=2)
    d.fill("art_dark", ellipse(px, py + 0.2, 7.4, 3.2, -8, 24), ink=0.0, steps=2)
    star(d, px, py + 0.6, 4.6, "art_white", ink=ink * 0.8)
    d.line("art_lgrey", [(px - 1.0, py + 1.6), (px + 0.4, py + 2.4)], 0.4)
    twinkle(d, px + 6.0, py + 6.0, 1.8, "art_white")
    for side in ("fore", "aft"):
        (qx, qy), _ = F.hand[side]
        d.oval("art_skin", qx, qy, F.r(0.15), F.r(0.13), ink=ink * 0.7)
    place_name(d, a, lines=["Silver", "State", "Sadie"], where="right", room=6.0, at=40.0, gap=1.32, caps=21.5)


# ---- 154: Third Time Lucky -----------------------------------------------------------------------------------------------------------
@painting(154)
def p154(d, a):   # three horseshoes in a rising row, the first two plain iron, the third one gold and shining
    ink = d.ink
    for (u, v, r, paint, turn) in ((9.0, 9.0, 8.4, "art_lsteel", -10.0), (22.0, 28.0, 9.4, "art_lsteel", -2.0), (38.0, 50.0, 11.6, "art_gold", 8.0)):
        horseshoe(d, u, v, r, paint, turn)
        d.line("art_ink", ellipse(u, v, r, r * 1.08, turn, 20, -40, 220), 0.4, taper=False)
    for t in range(8):
        a_ = math.radians(t * 45.0 + 22.5)
        d.line("art_yellow", [(38.0 + 14.6 * math.cos(a_), 50.0 + 15.4 * math.sin(a_)), (38.0 + 18.4 * math.cos(a_), 50.0 + 19.4 * math.sin(a_))], 1.2)
    place_name(d, a, lines=["Third", "Time", "Lucky"], where="right", room=6.0, at=34.0, gap=1.32, caps=21.5)
