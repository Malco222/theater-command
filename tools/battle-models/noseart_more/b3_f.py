"""Batch 3, numbers 116 to 122 (painter b3_f): a child's wagon with its scrap heap, a winged pass with a train ticket,
a farmer at his telescope, a wolf knocking at a plank door, a woodpecker at a knothole, a drum major high-stepping and
a boy in goggles in his orange-crate racer. Flat shapes with noseart's Drawing API and the figure kit; each name set
clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T  # noqa: E402


def _arc(cx, cy, rx, ry, a0, a1, n=12):
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
            for i in range(n + 1)]


# ---- 116: Scrap Drive Special ------------------------------------------------------------------------------------------------------
def kids_wagon(d, x, y, w, body="art_red", wheel_="art_dark"):
    """A child's wagon side-on, facing right, the middle of its bed's floor at (x, y), `w` long: its box, two wheels,
    its long handle out in front. Returns the bed's top corners."""
    hgt = w * 0.24
    r = w * 0.16
    for wx in (x - w * 0.32, x + w * 0.32):
        wheel(d, wx, y - r * 0.6, r, rim="art_lsteel", spokes=6)
    d.limb("art_dark", [(x + w * 0.46, y + hgt * 0.4), (x + w * 0.72, y + hgt * 0.2), (x + w * 0.9, y - r * 0.4)], [0.8, 0.7, 0.7], ink=d.ink * 0.7)
    d.shape("art_dark", [(x + w * 0.86, y - r * 0.6), (x + w * 0.86, y - r * 0.2), (x + w * 0.96, y - r * 0.2), (x + w * 0.96, y - r * 0.6)])
    d.shape(body, [(x - w / 2, y), (x - w / 2, y + hgt), (x + w / 2, y + hgt), (x + w / 2, y)])
    d.line("art_white", [(x - w * 0.44, y + hgt * 0.5), (x + w * 0.44, y + hgt * 0.5)], hgt * 0.14, taper=False)
    return (x - w / 2, y + hgt), (x + w / 2, y + hgt)


@painting(116)
def p116(d, a):   # a child's wagon with one tall heap of scrap: a bedstead's head standing up out of it, a pot on top
    (bx0, by), (bx1, _) = kids_wagon(d, 30.0, 10.0, 46.0)
    cx = (bx0 + bx1) / 2
    # (the bedstead's head, standing up at the back of the heap: two posts with knobs, the arched top rail, its bars)
    p0, p1, ptop = cx - 13.0, cx + 9.0, by + 42.0
    for px in (p0, p1):
        d.line("art_lsteel", [(px, by + 4.0), (px, ptop)], 1.4, taper=False)
        d.oval("art_gold", px, ptop + 1.4, 1.8, 1.8, ink=d.ink * 0.7)
    rail = _arc((p0 + p1) / 2, ptop - 6.0, (p1 - p0) / 2, 5.0, 180, 0, 14)
    d.line("art_lsteel", rail, 1.2, taper=False)
    for i in range(1, 6):
        bxx = p0 + (p1 - p0) * i / 6
        top = ptop - 6.0 + 5.0 * math.sin(math.pi * i / 6)
        d.line("art_lsteel", [(bxx, by + 10.0), (bxx, top)], 0.7, taper=False)
    # (the heap round its foot: pipes, a tire, cans, a bucket: one mound of scrap)
    d.fill("art_grey", [(bx0 + 1.0, by), (bx0 - 1.0, by + 6.0), (bx0 + 6.0, by + 14.0), (cx - 4.0, by + 18.0), (cx + 8.0, by + 20.0), (bx1 - 2.0, by + 14.0),
                        (bx1 + 2.0, by + 6.0), (bx1 - 1.0, by)], ink=d.ink, steps=4)
    d.oval("art_dark", bx0 + 8.0, by + 9.0, 4.6, 4.6, ink=d.ink * 0.8)
    d.oval("art_grey", bx0 + 8.0, by + 9.0, 2.0, 2.0, ink=d.ink * 0.6)
    d.line("art_lsteel", [(cx + 2.0, by + 6.0), (bx1 + 6.0, by + 16.0)], 1.4, taper=False)
    d.shape("art_lsteel", [(cx + 4.0, by + 10.0), (cx + 5.0, by + 17.0), (cx + 11.0, by + 17.0), (cx + 12.0, by + 10.0)])
    # (the pot on top, tipped, its handle)
    G = _T(cx + 4.0, by + 26.0, -14.0)
    d.fill("art_dark", [G(-6.0, -4.0), G(-6.6, 4.0), G(6.6, 4.0), G(6.0, -4.0)], ink=d.ink, steps=2)
    d.oval("art_grey", *G(0.0, 4.0), 6.6, 1.4, turn=-14.0, ink=d.ink * 0.7)
    d.line("art_dark", [G(6.4, 2.0), G(13.0, 3.0)], 1.2, taper=False)
    place_name(d, a, lines=["Scrap", "Drive", "Special"], where="right", room=6.0, at=34.0, gap=1.32, caps=21.5)


# ---- 117: Three Day Pass ----------------------------------------------------------------------------------------------------------
@painting(117)
def p117(d, a):   # an airman off on his pass: walking away grinning, kit bag on his shoulder, the pass slip in his cap's band
    from noseart_more.b2_f import kit_bag
    ink = d.ink
    x, y, h = 24.0, 2.0, 74.0
    kw = dict(pose="stride", arms=("carry", "down"), hat="peaked",
              wear={"shirt": "art_khaki", "tie": "art_dark", "jacket": "art_brown", "trousers": "art_khaki", "shoes": "art_brown", "hat": "art_khaki",
                    "band": "art_brown"},
              hair="short", mouth="grin")
    # (his kit bag over his shoulder, behind him: its neck in his fist, its round foot down his back)
    F0 = man(Drawing(1), x, y, h, 1, **kw)
    (hx, hy), _ = F0.hand["fore"]
    kit_bag(d, (hx - 0.6, hy + 1.0), F0.P(-1.9, 4.4), 15.0, paint="art_khaki")
    F = man(d, x, y, h, 1, **kw)
    F.grip("fore")
    # (the pass slip tucked in his cap's band at the side, leaning back: cream, its ruled lines, the red stamp)
    G = _T(*F.PH(-0.5, 7.34), 34.0)
    d.fill("art_cream", [G(-1.4, 0.0), G(-1.4, 5.6), G(2.4, 5.6), G(2.4, 0.0)], ink=ink * 0.8, steps=1)
    for v in (4.2, 2.8):
        d.line("art_lgrey", [G(-0.7, v), G(1.8, v)], 0.3, taper=False)
    d.line("art_red", ellipse(*G(1.0, 1.2), 0.8, 0.8, 0, 12), 0.3, taper=False)
    # (the train ticket in his other hand, orange card, held out as he goes)
    (tx, ty), _ = F.hand["aft"]
    T = _T(tx + 1.0, ty - 1.0, -16.0)
    d.shape("art_orange", [T(-1.0, -2.6), T(-1.0, 2.6), T(8.0, 2.6), T(8.0, -2.6)], ink=ink * 0.8)
    for u in (1.6, 4.0, 6.4):
        d.oval("art_dark", *T(u, -1.2), 0.5, 0.5, ink=0.0)
    F.grip("aft")
    speed(d, x - 22.0, y + 24.0, 8.0, n=3, gap=3.2, paint="art_white", w=0.9)
    place_name(d, a, lines=["Three", "Day", "Pass"], where="right", room=6.0, at=36.0, gap=1.32, caps=21.5)


# ---- 118: Show Me Sam ------------------------------------------------------------------------------------------------------------
@painting(118)
def p118(d, a):   # a farmer in a straw hat squinting through a telescope
    x, y, h = 24.0, 2.0, 58.0
    F = man(d, x, y, h, 1, pose="wide", arms=(to(2.5, 6.92, "down"), to(0.6, 6.9, frame="head", elbow=(0.5, -0.4))), hat="straw",
            wear={"shirt": "art_red", "sleeves": "rolled", "overalls": "art_blue", "trousers": "art_blue", "shoes": "art_brown", "hat": "art_tan",
                  "belt": None},
            hair="short", eyes="wink", mouth="flat")
    # (the telescope: from his eye out past his far hand, rising a little, in three drawn sections, brass)
    e = F.PH(0.34, 6.96)
    ang = math.radians(8.0)
    ux, uy = math.cos(ang), math.sin(ang)
    k = F.k
    secs = [(0.0, 1.0, 0.18), (1.0, 2.0, 0.24), (2.0, 3.4, 0.3)]
    for (s0, s1, r) in secs:
        p0 = (e[0] + ux * s0 * k, e[1] + uy * s0 * k)
        p1 = (e[0] + ux * s1 * k, e[1] + uy * s1 * k)
        nx, ny = -uy * r * k, ux * r * k
        d.shape("art_yellow", [(p0[0] + nx, p0[1] + ny), (p1[0] + nx, p1[1] + ny), (p1[0] - nx, p1[1] - ny), (p0[0] - nx, p0[1] - ny)], ink=d.ink)
        d.line("art_gold", [(p1[0] + nx, p1[1] + ny), (p1[0] - nx, p1[1] - ny)], 0.6, taper=False)
    F.grip("fore")
    F.grip("aft")
    for i in range(3):                                    # (what he sees: three dashes out ahead of it)
        p = (e[0] + ux * (3.8 + i * 0.5) * k, e[1] + uy * (3.8 + i * 0.5) * k)
        d.line("art_white", [(p[0], p[1] + (i - 1) * 1.6), (p[0] + 2.4, p[1] + (i - 1) * 2.4)], 0.7)
    place_name(d, a, lines=["Show Me", "Sam"], where="right", room=6.0, at=36.0, caps=21.5)


# ---- 119: Wolf at the Door ------------------------------------------------------------------------------------------------------
def plank_door(d, x0, y0, w, h, wood="art_brown", seam="art_dark", iron="art_dark"):
    """A plank door in its frame, front on, its lower left corner at (x0, y0): five planks, two battens with a brace,
    strap hinges on the left, a latch on the right. Returns its middle."""
    d.shape(wood, [(x0, y0), (x0, y0 + h), (x0 + w, y0 + h), (x0 + w, y0)])
    for i in range(1, 5):
        u = x0 + w * i / 5
        d.line(seam, [(u, y0 + 0.6), (u, y0 + h - 0.6)], 0.5, taper=False)
    for v in (y0 + h * 0.2, y0 + h * 0.78):
        d.line("art_lbrown", [(x0 + 1.0, v), (x0 + w - 1.0, v)], 2.2, taper=False)
        d.line(iron, [(x0 - 1.0, v), (x0 + w * 0.42, v)], 1.0, taper=False)
    d.line("art_lbrown", [(x0 + 1.4, y0 + h * 0.22), (x0 + w - 1.4, y0 + h * 0.76)], 1.8, taper=False)
    d.oval(iron, x0 + w - 3.0, y0 + h * 0.48, 1.0, 1.4, ink=0.0)
    d.line("art_lbrown", [(x0 - 1.4, y0 - 0.6), (x0 - 1.4, y0 + h + 1.4), (x0 + w + 1.4, y0 + h + 1.4), (x0 + w + 1.4, y0 - 0.6)], 2.2, taper=False)
    return (x0 + w / 2, y0 + h / 2)


def wolf(d, x, y, size, coat="art_grey", pale="art_lgrey", dark="art_dark"):
    """A lean grey wolf up on his hind legs, facing right, side on: long legs, a narrow chest, a long snout with a sly
    smile, pointed ears up, a bushy tail; his near forepaw up (knocking: returned), the far one down. Feet at (x, y)."""
    k = size / 40.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    d.fill(coat, Ps([(-3.0, 16.0), (-9.0, 12.0), (-14.0, 4.0), (-12.0, 2.0), (-8.0, 6.0), (-2.0, 12.0)]), ink=ink, steps=4)            # (tail)
    d.limb(coat, Ps([(-1.0, 16.0), (-3.0, 8.0), (-1.0, 0.8)]), [2.6 * k, 1.6 * k, 1.2 * k], ink=ink)                                   # (far leg)
    d.oval(dark, *P(0.4, 0.4), 2.0 * k, 0.9 * k, ink=ink * 0.6)
    d.limb(coat, Ps([(2.0, 26.0), (3.0, 20.0), (4.0, 16.0)]), [1.6 * k, 1.3 * k, 1.1 * k], ink=ink)                                    # (far arm, down)
    d.fill(coat, Ps([(-4.0, 14.0), (-4.4, 22.0), (-2.0, 30.0), (3.0, 31.0), (5.0, 24.0), (3.0, 14.0), (0.0, 12.0)]), ink=ink, steps=4)
    d.fill(pale, Ps([(4.6, 24.0), (3.0, 16.0), (1.0, 14.0), (1.4, 22.0), (3.0, 29.0)]), ink=0.0, steps=3)
    d.limb(coat, Ps([(1.0, 16.0), (4.0, 9.0), (2.0, 0.8)]), [2.8 * k, 1.7 * k, 1.2 * k], ink=ink)                                      # (near leg)
    d.oval(dark, *P(3.4, 0.4), 2.2 * k, 0.9 * k, ink=ink * 0.6)
    # (his head: the long snout out to the right, ears up, a sly eye, a grin)
    hx, hy = 3.0, 33.0
    for (eu, ev) in ((-1.4, 0.0), (0.6, 0.4)):
        d.fill(coat, Ps([(hx + eu - 1.4, hy + 2.0 + ev), (hx + eu - 0.4, hy + 7.4 + ev), (hx + eu + 1.4, hy + 2.6 + ev)]), ink=ink, steps=1)
    d.fill(coat, Ps([(hx - 3.0, hy - 1.0), (hx - 2.0, hy + 3.0), (hx + 2.0, hy + 3.4), (hx + 6.0, hy + 1.6), (hx + 11.0, hy + 0.6), (hx + 11.6, hy - 0.8),
                     (hx + 10.0, hy - 2.0), (hx + 4.0, hy - 3.0), (hx - 1.0, hy - 3.6)]), ink=ink, steps=4)
    d.oval("art_black", *P(hx + 11.2, hy - 0.2), 0.9 * k, 0.7 * k, ink=0.0)
    d.line("art_ink", Ps([(hx + 4.0, hy - 1.6), (hx + 7.0, hy - 1.8), (hx + 10.0, hy - 1.4)]), 0.4 * k)
    d.fill("art_white", Ps([(hx + 5.0, hy - 1.7), (hx + 5.6, hy - 2.6), (hx + 6.2, hy - 1.8)]), ink=0.0, steps=1)
    d.oval("art_yellow", *P(hx + 2.4, hy + 1.2), 0.9 * k, 0.6 * k, ink=ink * 0.5)
    d.line("art_ink", Ps([(hx + 1.4, hy + 2.0), (hx + 3.4, hy + 1.8)]), 0.4 * k)
    # (his near arm, up, knocking)
    paw = P(9.6, 30.0)
    d.limb(coat, Ps([(2.0, 27.0), (6.0, 24.0), (9.0, 29.0)]), [1.7 * k, 1.4 * k, 1.2 * k], ink=ink)
    d.oval(coat, *paw, 1.5 * k, 1.4 * k, ink=ink)
    return paw


@painting(119)
def p119(d, a):   # a lean grey wolf rapping on a plank door with one paw
    plank_door(d, 36.0, 2.0, 28.0, 56.0)
    paw = wolf(d, 18.0, 2.0, 54.0)
    for (dx, dy, L) in ((2.0, 3.0, 3.0), (3.0, 0.0, 3.4), (2.0, -3.0, 3.0)):           # (the raps)
        d.line("art_white", [(paw[0] + dx + 2.0, paw[1] + dy), (paw[0] + dx + 2.0 + L, paw[1] + dy * 1.6)], 0.8)
    place_name(d, a, lines=["Wolf at", "the Door"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 120: Knothole Ned ------------------------------------------------------------------------------------------------------------
@painting(120)
def p120(d, a):   # a woodpecker's head peering out through a knothole in a board fence
    # (the fence: weathered boards, one with the knothole, their nails, the rail behind)
    x0, w, h, n = -2.0, 19.0, 66.0, 3
    for i in range(n):
        bx = x0 + i * w
        top = h + (2.0 if i % 2 else 0.0)
        d.shape("art_tan", [(bx + 0.4, 0.0), (bx + 0.4, top), (bx + w / 2, top + 3.0), (bx + w - 0.4, top), (bx + w - 0.4, 0.0)])
        d.line("art_lbrown", [(bx + 3.0, 4.0), (bx + 3.4, top - 6.0)], 0.5)
        d.line("art_lbrown", [(bx + 8.0, 10.0), (bx + 7.6, top - 12.0)], 0.4)
        for v in (12.0, 54.0):
            d.oval("art_dark", bx + w / 2, v, 0.6, 0.6, ink=0.0)
    hx, hy, hr = x0 + w + w / 2, 36.0, 9.5
    # (the knothole, its rim of grain round it, and in it the bird's head: black and white, a red patch at the back,
    #  one bright eye, his straight beak poking out through it)
    for rr in (hr + 3.0, hr + 1.6):
        d.line("art_lbrown", ellipse(hx, hy, rr, rr * 1.2, 0, 20), 0.5, taper=False)
    d.oval("art_black", hx, hy, hr, hr * 1.15, ink=d.ink)
    m = hr / 4.6
    d.oval("art_white", hx + 0.6 * m, hy - 0.6 * m, hr * 0.8, hr * 0.8, ink=0.0)
    d.fill("art_black", [(hx - 3.0 * m, hy + 2.4 * m), (hx + 3.8 * m, hy + 2.4 * m), (hx + 3.6 * m, hy + 4.0 * m), (hx - 2.6 * m, hy + 4.6 * m)], ink=0.0, steps=2)
    d.fill("art_black", [(hx - 3.6 * m, hy - 0.4 * m), (hx + 3.6 * m, hy + 0.4 * m), (hx + 3.6 * m, hy - 0.8 * m), (hx - 3.6 * m, hy - 1.6 * m)], ink=0.0, steps=1)
    d.oval("art_red", hx - 3.0 * m, hy + 3.0 * m, 1.2 * m, 1.0 * m, ink=0.0)
    d.oval("art_white", hx + 1.4 * m, hy + 0.9 * m, 1.1 * m, 1.1 * m, ink=d.ink * 0.5)
    d.oval("art_black", hx + 1.8 * m, hy + 0.9 * m, 0.6 * m, 0.6 * m, ink=0.0)
    d.fill("art_dark", [(hx + 2.4 * m, hy - 0.2 * m), (hx + 12.0 * m, hy - 1.2 * m), (hx + 2.4 * m, hy - 2.2 * m)], ink=d.ink * 0.7, steps=1)
    # (his tail feathers, stiff, braced against the next board down where he clings on the far side: a hint)
    place_name(d, a, lines=["Knothole", "Ned"], where="right", room=6.0, at=36.0, caps=21.5)


# ---- 121: Drum Major Dan -------------------------------------------------------------------------------------------------------
@painting(121)
def p121(d, a):   # a drum major in his tall hat high-stepping, his baton up
    x, y, h = 28.0, 2.0, 60.0
    F = man(d, x, y, h, 1, pose="step", arms=("raise", "hip"), hat="shako", tilt=-6.0,
            wear={"shirt": "art_white", "jacket": "art_red", "cut": "tunic", "trim": "art_gold", "buttons": "art_gold", "trousers": "art_white",
                  "shoes": "art_black", "hat": "art_navy", "band": "art_gold", "gloves": "art_white", "belt": "art_white", "buckle": "art_gold"},
            hair="short", mouth="smile")
    (hx, hy), _ = F.hand["fore"]
    # (the baton: up from his white glove, a steel shaft, its big ball at the top, its tip below)
    d.line("art_lsteel", [(hx - 1.6, hy - 6.0), (hx + 1.6, hy + 14.0)], 0.9, taper=False)
    d.oval("art_lsteel", hx + 1.9, hy + 15.6, 2.0, 2.0, ink=d.ink * 0.7)
    d.oval("art_white", hx + 1.4, hy + 16.2, 0.6, 0.6, ink=0.0)
    d.oval("art_lsteel", hx - 1.7, hy - 6.6, 0.8, 0.8, ink=d.ink * 0.6)
    F.grip("fore")
    for (u, v) in ((-8.0, 50.0), (-10.0, 38.0)):                                         # (the beat: notes behind him)
        d.oval("art_white", x + u, y + v, 1.6, 1.1, turn=20, ink=d.ink * 0.6)
        d.line("art_white", [(x + u + 1.4, y + v + 0.3), (x + u + 1.4, y + v + 7.0)], 0.6, taper=False)
    place_name(d, a, lines=["Drum", "Major", "Dan"], where="right", room=6.0, at=36.0, gap=1.32, caps=21.5)


# ---- 122: Orange Crate Racer ---------------------------------------------------------------------------------------------------
@painting(122)
def p122(d, a):   # a boy in goggles steering a racer built from an orange crate
    x0, x1, yb = 8.0, 50.0, 10.0
    r = 7.0
    wheel(d, x0 + 6.0, yb - 2.0, r, rim="art_lsteel", spokes=6)
    # (the boy in it: only his body up from the crate, goggles on his brow, his scarf streaming, hands on the wheel)
    F = kid(d, x0 + 18.0, yb - 6.0, 40.0, 1, pose="sit", arms=(to(1.9, 5.0, "down"), to(1.6, 4.8, "down")), hat="goggles",
            wear={"shirt": "art_white", "sleeves": "short", "shorts": "art_blue", "scarf": "art_red"}, hair="tousle", mouth="grin", legs_shown=False)
    # (the crate: slats, its end painted with one big orange and its leaf; the steering wheel, its column)
    d.shape("art_tan", [(x0, yb), (x0, yb + 12.0), (x1, yb + 12.0), (x1, yb)])
    for v in (yb + 4.0, yb + 8.0):
        d.line("art_lbrown", [(x0 + 0.6, v), (x1 - 0.6, v)], 0.6, taper=False)
    for u in (x0 + 0.6, x1 - 0.6):
        d.line("art_lbrown", [(u, yb + 0.4), (u, yb + 11.6)], 1.2, taper=False)
    d.oval("art_orange", x1 - 10.0, yb + 6.0, 4.4, 4.4, ink=d.ink)
    d.fill("art_green", [(x1 - 10.0, yb + 10.2), (x1 - 7.0, yb + 12.4), (x1 - 8.0, yb + 10.0)], ink=d.ink * 0.6, steps=1)
    (hx, hy), _ = F.hand["fore"]
    d.line("art_dark", [(hx + 1.0, hy - 1.0), (x1 - 4.0, yb + 12.0)], 0.9, taper=False)
    d.line("art_dark", ellipse(hx, hy, 0.6, 3.2, 0, 16), 0.8, taper=False)
    F.grip("fore")
    F.grip("aft")
    wheel(d, x1 - 6.0, yb - 2.0, r, rim="art_lsteel", spokes=6)
    speed(d, x0 - 4.0, yb + 4.0, 9.0, n=3, gap=3.6, paint="art_white", w=0.9)
    place_name(d, a, lines=["Orange", "Crate", "Racer"], where="right", room=6.0, at=30.0, gap=1.32, caps=21.5)
