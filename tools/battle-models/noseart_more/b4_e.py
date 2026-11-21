"""Batch 4, numbers 169 to 175 (painter b4_e): a snapping turtle with its jaws clamped on a propeller blade, a lighthouse
sweeping its beam with the keeper waving from the gallery, a longhorn bull pawing up dust, an airman on a bicycle with
his scarf streaming, a small steam engine coming out of a covered bridge, a ladybug in goggles landing on a daisy and a
soldier on a crate playing his harmonica. Flat shapes with noseart's Drawing API and the figure kit; each name set
clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b2_h import plain_bomber  # noqa: E402
from noseart_more.b3_a import _T, note  # noqa: E402
from noseart_more.b3_b import bicycle  # noqa: E402


# ---- 169: Old Snapper ----------------------------------------------------------------------------------------------------------------
@painting(169)
def p169(d, a):   # a snapping turtle side-on, its hooked jaws clamped on the tip of a propeller blade, not letting go
    ink = d.ink
    K = lambda u, v: (24.0 + u, 4.0 + v)                 # noqa: E731
    # (the blade coming down from above into its jaws: dark, its yellow tip; the hub's edge at the top)
    hub = _T(*K(22.0, 16.0), 64.0)(30.0, 0.0)
    for ang in (244.0, 4.0, 124.0):                        # (three blades round the hub; the first one down into its jaws)
        G = _T(*hub, ang)
        L = 30.0 if ang == 244.0 else 22.0
        d.fill("art_dark", [G(3.0, -2.2), G(L - 6.0, -3.4), G(L, -2.0), G(L, 2.4), G(L - 6.0, 3.2), G(3.0, 2.2)], ink=ink, steps=2)
        d.fill("art_yellow", [G(L - 4.0, -2.8), G(L, -2.0), G(L, 2.4), G(L - 4.0, 2.8)], ink=ink * 0.8, steps=1)
    d.oval("art_lsteel", *hub, 5.0, 5.0, ink=ink)
    d.oval("art_grey", hub[0] + 1.0, hub[1] + 1.0, 1.6, 1.6, ink=0.0)
    # (far legs, tail)
    d.limb("art_khaki", [K(-14.0, 6.0), K(-18.0, 3.0), K(-20.0, 0.6)], [2.6, 2.0, 1.6], ink=ink * 0.8)
    d.limb("art_khaki", [K(10.0, 6.0), K(13.0, 2.6), K(15.0, 0.4)], [2.6, 2.0, 1.6], ink=ink * 0.8)
    d.fill("art_khaki", [K(-18.0, 7.0), K(-28.0, 4.0), K(-29.0, 5.4), K(-18.0, 10.0)], ink=ink, steps=2)
    for u in (-26.0, -23.0, -20.0):
        d.fill("art_dark", [K(u - 1.0, 6.0 + (u + 26) * 0.3), K(u, 8.0 + (u + 26) * 0.3), K(u + 1.0, 6.2 + (u + 26) * 0.3)], ink=0.0, steps=1)
    # (the shell: a high rough dome, its scutes, the jagged rim at the back; the pale underside)
    d.fill("art_brown", [K(-20.0, 6.0), K(-18.0, 14.0), K(-10.0, 21.0), K(0.0, 23.0), K(10.0, 20.0), K(16.0, 13.0), K(17.0, 6.0)], ink=ink, steps=4)
    d.shape("art_tan", [K(-20.0, 6.0), K(-18.0, 4.0), K(16.0, 4.0), K(17.0, 6.0)], ink=ink * 0.8)
    for (u0, v0, u1, v1) in ((-8.0, 7.0, -10.0, 20.0), (0.0, 7.0, 0.0, 22.6), (8.0, 7.0, 9.0, 19.6), (-16.0, 12.0, 14.0, 13.0)):
        d.line("art_dark", [K(u0, v0), K(u1, v1)], 0.6)
    for (u, v) in ((-4.0, 21.6), (4.0, 21.4), (-12.0, 18.4), (11.0, 17.6)):
        d.fill("art_dark", [K(u - 1.6, v), K(u, v + 2.0), K(u + 1.6, v)], ink=0.0, steps=1)
    # (near legs, clawed)
    for (u0, u1) in ((-12.0, -15.0), (12.0, 16.0)):
        d.limb("art_khaki", [K(u0, 6.0), K(u1, 3.0), K(u1 + 1.0, 0.6)], [3.0, 2.4, 2.0], ink=ink)
        for t in (-1.2, 0.0, 1.2):
            d.line("art_ink", [K(u1 + 1.0 + t, 0.6), K(u1 + 1.6 + t * 1.3, -0.6)], 0.4)
    # (the neck and big head stretched forward and up; the hooked beak clamped on the blade; a mean little eye)
    d.limb("art_khaki", [K(14.0, 10.0), K(18.0, 12.0), K(22.0, 14.0)], [3.0, 3.0, 2.8], ink=ink)
    d.fill("art_khaki", [K(18.0, 12.0), K(19.0, 18.0), K(23.0, 19.6), K(27.0, 18.0), K(28.6, 15.4), K(27.0, 12.0), K(22.0, 10.0)], ink=ink, steps=3)
    d.fill("art_khaki", [K(26.0, 17.8), K(29.6, 17.0), K(29.0, 14.4), K(27.4, 15.6)], ink=ink, steps=1)
    d.line("art_ink", [K(21.0, 14.2), K(25.0, 15.0), K(28.6, 15.4)], 0.5)
    d.oval("art_yellow", *K(23.0, 17.0), 0.9, 0.8, ink=0.0)
    d.oval("art_ink", *K(23.2, 17.0), 0.4, 0.5, ink=0.0)
    d.line("art_ink", [K(21.6, 18.4), K(24.4, 17.8)], 0.4)
    for (u, v) in ((31.0, 20.0), (32.0, 13.0)):
        d.line("art_white", [K(u, v), K(u + 2.6, v + (1.6 if v > 15 else -1.6))], 0.6)
    place_name(d, a, lines=["Old", "Snapper"], where="right", room=6.0, at=24.0, caps=21.5)


# ---- 170: Keeper of the Light --------------------------------------------------------------------------------------------------------
@painting(170)
def p170(d, a):   # a lighthouse on its rocks sweeping its beam out to sea, its keeper on the gallery waving
    ink = d.ink
    cx, base, top = 34.0, 8.0, 70.0
    # (the beam, out to the left across the sky: a long pale wedge, fading)
    lx, ly = cx, top + 4.0
    d.shape("art_cream", [(lx, ly + 1.2), (lx - 34.0, ly + 9.0), (lx - 34.0, ly - 3.0), (lx, ly - 1.2)])
    d.shape("art_yellow", [(lx, ly + 0.6), (lx - 22.0, ly + 4.6), (lx - 22.0, ly - 1.4), (lx, ly - 0.6)])
    # (the rocks and the sea at its foot)
    d.fill("art_grey", [(cx - 14.0, 0.0), (cx - 13.0, 5.0), (cx - 8.0, 9.0), (cx - 2.0, 8.4), (cx + 6.0, 9.4), (cx + 12.0, 6.0), (cx + 15.0, 0.0)], ink=ink, steps=3)
    waves(d, cx - 22.0, cx + 22.0, 1.0, 3.0, paint="art_blue", cap="art_white", n=4)
    # (the tower, tapering: white with two red bands; its door, its windows)
    w0, w1 = 7.0, 4.6
    def X(v, su):
        return cx + su * (w0 + (w1 - w0) * (v - base) / (top - base))
    d.shape("art_white", [(X(base, -1), base), (X(top, -1), top), (X(top, 1), top), (X(base, 1), base)], ink=ink)
    for (v0, v1) in ((base + 14.0, base + 24.0), (base + 38.0, base + 48.0)):
        d.shape("art_red", [(X(v0, -1), v0), (X(v1, -1), v1), (X(v1, 1), v1), (X(v0, 1), v0)], ink=ink * 0.7)
    d.fill("art_dark", [(cx - 1.6, base), (cx - 1.6, base + 5.0), (cx, base + 6.0), (cx + 1.6, base + 5.0), (cx + 1.6, base)], ink=0.0, steps=1)
    for v in (base + 30.0, base + 54.0):
        d.shape("art_dark", [(cx - 0.9, v), (cx - 0.9, v + 2.6), (cx + 0.9, v + 2.6), (cx + 0.9, v)])
    # (the gallery and its rail; the lantern room, its lit glass; the cap and its ball)
    d.shape("art_dark", [(cx - 8.0, top), (cx - 8.0, top + 1.2), (cx + 8.0, top + 1.2), (cx + 8.0, top)])
    for u in range(-8, 9, 2):
        d.line("art_dark", [(cx + u, top + 1.2), (cx + u, top + 3.6)], 0.25, taper=False)
    d.line("art_dark", [(cx - 8.0, top + 3.6), (cx + 8.0, top + 3.6)], 0.4, taper=False)
    d.shape("art_yellow", [(cx - 3.4, top + 1.2), (cx - 3.4, top + 7.4), (cx + 3.4, top + 7.4), (cx + 3.4, top + 1.2)], ink=ink * 0.8)
    for u in (-1.2, 1.2):
        d.line("art_dark", [(cx + u, top + 1.2), (cx + u, top + 7.4)], 0.3, taper=False)
    d.fill("art_red", [(cx - 4.6, top + 7.4), (cx, top + 11.6), (cx + 4.6, top + 7.4)], ink=ink * 0.8, steps=1)
    d.oval("art_dark", cx, top + 12.2, 0.8, 0.8, ink=0.0)
    # (the keeper on the gallery, waving: a small man in his cap and jersey)
    F = man(d, cx + 6.0, top + 1.2, 11.0, 1, pose="stand", arms=("wave", "down"), hat="cap",
            wear={"shirt": "art_navy", "sleeves": "long", "trousers": "art_navy", "shoes": "art_black", "cap": "art_navy"}, hair="short")
    place_name(d, a, lines=["Keeper", "of the", "Light"], where="right", room=6.0, at=44.0, gap=1.32, caps=21.5)


# ---- 171: Ornery Otis ----------------------------------------------------------------------------------------------------------------
@painting(171)
def p171(d, a):   # a rangy longhorn bull side-on, head down, horns wide, one fore hoof pawing up dust
    ink = d.ink
    K = lambda u, v: (36.0 + u, 3.0 + v)                 # noqa: E731
    coat, dark = "art_auburn", "art_brown"
    # (far legs)
    d.limb(coat, [K(-16.0, 14.0), K(-17.0, 7.0), K(-16.4, 0.6)], [2.4, 1.6, 1.3], ink=ink * 0.8)
    d.limb(coat, [K(12.0, 14.0), K(13.0, 7.0), K(12.4, 0.6)], [2.4, 1.6, 1.3], ink=ink * 0.8)
    # (the body: long and lean, the hump at the shoulder, the dewlap; patches of white; the tail with its switch)
    d.fill(coat, [K(-20.0, 15.0), K(-20.0, 23.0), K(-12.0, 25.0), K(4.0, 25.0), K(12.0, 28.0), K(17.0, 25.0), K(19.0, 18.0), K(15.0, 12.0), K(4.0, 12.6),
                  K(-14.0, 12.0)], ink=ink, steps=4)
    d.fill("art_white", [K(-8.0, 13.0), K(-10.0, 18.0), K(-4.0, 22.0), K(2.0, 19.0), K(0.0, 13.0)], ink=0.0, steps=3)
    d.fill("art_white", [K(-19.6, 20.0), K(-16.0, 24.4), K(-13.0, 22.0), K(-15.0, 17.0)], ink=0.0, steps=2)
    d.line(coat, [K(-20.0, 22.0), K(-23.0, 16.0), K(-23.4, 9.0)], 0.8)
    d.fill(dark, [K(-24.4, 9.6), K(-23.4, 5.4), K(-22.4, 9.6)], ink=ink * 0.6, steps=1)
    # (near hind leg planted; near fore leg lifted, pawing)
    d.limb(coat, [K(-13.0, 14.0), K(-14.0, 7.0), K(-13.0, 0.6)], [2.8, 1.8, 1.5], ink=ink)
    d.limb(coat, [K(10.0, 14.0), K(14.0, 8.0), K(10.0, 5.0)], [2.8, 1.8, 1.5], ink=ink)
    for (u, v) in ((-12.4, 0.2), (12.4, 0.2), (-16.0, 0.2)):
        d.oval("art_black", *K(u, v), 1.6, 0.8, ink=0.0)
    d.oval("art_black", *K(9.4, 4.4), 1.3, 1.0, turn=40, ink=0.0)
    # (the dust he paws up)
    for (u, v, w, hh, sd) in ((14.0, 1.6, 8.0, 3.6, 1), (18.6, 4.4, 5.0, 3.0, 2)):
        cloud(d, *K(u, v), w, hh, "art_tan", seed=171 + sd, ink=0.0, shade="art_khaki")
    # (the head: low, its long face down to the ground, nostrils snorting; the horns sweeping out wide and up at the tips)
    d.fill(coat, [K(15.0, 24.0), K(19.0, 26.0), K(23.0, 22.0), K(27.0, 14.0), K(28.0, 10.0), K(25.0, 9.0), K(21.0, 13.0), K(17.0, 17.0)], ink=ink, steps=3)
    d.fill("art_white", [K(22.0, 21.0), K(25.0, 16.0), K(26.6, 11.6), K(25.0, 11.0), K(22.6, 16.0), K(20.4, 20.4)], ink=0.0, steps=2)
    d.oval("art_pink", *K(26.6, 10.2), 1.4, 1.2, ink=ink * 0.6)
    d.oval("art_ink", *K(27.2, 10.0), 0.3, 0.4, ink=0.0)
    for (u, v, t) in ((30.0, 9.0, -10.0), (30.6, 11.4, 10.0)):
        G = _T(*K(u, v), t)
        d.line("art_white", [G(0.0, 0.0), G(3.0, 0.0)], 0.5)
    d.oval("art_white", *K(20.6, 20.4), 0.9, 0.7, ink=0.0)
    d.oval("art_red", *K(21.0, 20.4), 0.45, 0.45, ink=0.0)
    d.line("art_ink", [K(19.4, 21.6), K(22.0, 20.8)], 0.4)
    for su in (-1.0, 1.0):
        hb = K(19.0, 24.6)
        pts = [hb, K(19.0 + su * 8.0, 26.6), K(19.0 + su * 16.0, 26.0), K(19.0 + su * 21.0, 29.0)]
        d.limb("art_cream", pts, [1.2, 1.0, 0.7, 0.4], ink=ink * 0.8)
    place_name(d, a, lines=["Ornery", "Otis"], where="below", room=5.0, caps=21.5)


# ---- 172: Bicycle Squadron -----------------------------------------------------------------------------------------------------------
@painting(172)
def p172(d, a):   # an airman in his flying helmet pedalling a bicycle hard, his white scarf streaming behind
    x, y, h = 30.0, 8.0, 56.0
    legs = ([(0.3, 3.8), (1.42, 3.2), (1.3, 1.62)], [(-0.18, 3.8), (0.22, 2.06), (0.5, 0.5)])
    kw = dict(pose=legs, arms=(to(2.5, 5.2, "down"), to(2.15, 5.0, "down")), tilt=14.0, hat="flying",
              wear={"shirt": "art_khaki", "jacket": "art_brown", "trousers": "art_khaki", "shoes": "art_brown", "helmet": "art_brown"}, mouth="grin")
    S = man(Drawing(1), x, y, h, 1, **kw)
    C = lerp(S.PL(1.3, 1.62), S.PL(0.5, 0.5), 0.5)
    r = h * 0.2
    saddle = S.PL(-0.12, 3.54)
    grips = S.hand["aft"][0]
    head = (grips[0] + 0.1 * r, grips[1] - 0.42 * r)
    bicycle(d, C, r, head, grips, saddle, frame="art_blue")
    F = man(d, x, y, h, 1, **kw)
    scarf(d, F, "art_white", length=3.0)
    speed(d, x - 26.0, y + 14.0, 8.0, n=3, gap=3.4, paint="art_white", w=0.9)
    place_name(d, a, lines=["Bicycle", "Squadron"], where="below", room=5.0, caps=21.5)


# ---- 173: Covered Bridge Express -----------------------------------------------------------------------------------------------------
@painting(173)
def p173(d, a):   # a red covered bridge seen end-on, a small steam engine coming out of its dark portal under its own plume
    ink = d.ink
    n0 = len(d.ops)
    cx = 26.0
    # (the stream under it; the stone abutment)
    waves(d, cx - 22.0, cx + 22.0, 3.0, 3.0, paint="art_blue", cap="art_white", n=3)
    d.fill("art_grey", [(cx - 16.0, 0.0), (cx - 16.0, 8.0), (cx + 16.0, 8.0), (cx + 16.0, 0.0)], ink=ink, steps=1)
    for (u, v) in ((-10.0, 4.0), (0.0, 4.0), (10.0, 4.0), (-5.0, 0.6), (5.0, 0.6)):
        d.line("art_dark", [(cx + u - 4.0, v), (cx + u + 4.0, v)], 0.3, taper=False)
    # (the bridge's long side going back over the stream to the far bank, in perspective: its wall, its roof, its windows)
    d.fill("art_brown", [(cx - 15.0, 8.0), (cx - 34.0, 15.0), (cx - 34.0, 27.0), (cx - 15.0, 28.0)], ink=ink, steps=1)
    for t in (0.25, 0.5, 0.75):
        u = cx - 15.0 - 19.0 * t
        d.line("art_dark", [(u, 8.0 + 7.0 * t + 0.4), (u, 28.0 - 1.0 * t - 0.4)], 0.3, taper=False)
    for t in (0.3, 0.62):
        u = cx - 15.0 - 19.0 * t
        d.shape("art_black", [(u - 2.0, 18.0 + 3.0 * t), (u - 2.0, 22.0 + 2.0 * t), (u + 1.6, 22.0 + 2.0 * t), (u + 1.6, 18.0 + 3.0 * t)])
    d.fill("art_dark", [(cx - 18.0, 27.0), (cx - 36.0, 28.0), (cx - 34.0, 33.0), (cx, 40.0)], ink=ink, steps=1)
    d.fill("art_grey", [(cx - 38.0, 12.0), (cx - 38.0, 16.0), (cx - 30.0, 13.4), (cx - 30.0, 10.0)], ink=ink * 0.8, steps=1)
    # (the portal: its red board front, the gable, its eaves; the dark arch going back into it)
    d.fill("art_red", [(cx - 15.0, 8.0), (cx - 15.0, 28.0), (cx, 38.0), (cx + 15.0, 28.0), (cx + 15.0, 8.0)], ink=ink, steps=1)
    for u in range(-13, 14, 3):
        d.line("art_brown", [(cx + u, 8.4), (cx + u, 28.0 + (10.0 * (1 - abs(u) / 15.0)) - 0.6)], 0.25, taper=False)
    d.fill("art_dark", [(cx - 18.0, 27.0), (cx, 40.0), (cx + 18.0, 27.0), (cx + 18.0, 25.4), (cx, 37.6), (cx - 18.0, 25.4)], ink=ink * 0.8, steps=1)
    d.fill("art_black", [(cx - 9.0, 8.0), (cx - 9.0, 20.0), (cx - 6.0, 25.0), (cx, 27.0), (cx + 6.0, 25.0), (cx + 9.0, 20.0), (cx + 9.0, 8.0)], ink=0.0, steps=3)
    # (the engine coming out of it head-on: the cowcatcher, red buffer beam, its cylinders either side, the round
    #  smokebox front with its door, the headlamp lit, the stack; the steam spilling out low either side)
    for su in (-1.0, 1.0):
        puff(d, cx + su * 10.6, 9.6, 2.4, "art_white", seed=int(4 + su), ink=ink * 0.4)
    d.fill("art_grey", [(cx - 7.6, 10.4), (cx + 7.6, 10.4), (cx + 3.0, 8.0), (cx - 3.0, 8.0)], ink=ink, steps=1)
    for u in (-5.0, -2.6, 0.0, 2.6, 5.0):
        d.line("art_dark", [(cx + u, 10.2), (cx + u * 0.42, 8.2)], 0.35, taper=False)
    for su in (-1.0, 1.0):
        d.oval("art_steel", cx + su * 6.8, 12.6, 2.0, 1.9, ink=ink)
        d.oval("art_dark", cx + su * 6.8, 12.6, 0.8, 0.8, ink=0.0)
    d.shape("art_red", [(cx - 8.6, 10.2), (cx - 8.6, 11.8), (cx + 8.6, 11.8), (cx + 8.6, 10.2)], ink=ink)
    d.shape("art_dark", [(cx - 1.0, 9.6), (cx - 1.0, 11.0), (cx + 1.0, 11.0), (cx + 1.0, 9.6)], ink=ink * 0.6)
    d.oval("art_steel", cx, 16.6, 5.2, 5.0, ink=ink)
    d.oval("art_grey", cx, 16.4, 3.8, 3.6, ink=ink * 0.8)
    d.line("art_lsteel", ellipse(cx, 16.4, 3.0, 2.8, 0, 12, 100, 200), 0.35, taper=False)
    d.oval("art_lsteel", cx, 16.4, 0.7, 0.7, ink=ink * 0.6)
    for v in (15.0, 17.8):
        d.line("art_dark", [(cx - 4.6, v), (cx - 1.4, v)], 0.35, taper=False)
    d.fill("art_dark", [(cx - 1.4, 20.6), (cx - 2.4, 25.2), (cx + 2.4, 25.2), (cx + 1.4, 20.6)], ink=ink, steps=1)
    d.line("art_lsteel", [(cx - 2.4, 25.0), (cx + 2.4, 25.0)], 0.4, taper=False)
    d.shape("art_dark", [(cx - 1.8, 20.2), (cx - 1.8, 23.0), (cx + 1.8, 23.0), (cx + 1.8, 20.2)], ink=ink)
    d.oval("art_yellow", cx, 21.6, 1.1, 1.1, ink=0.0)
    for (ang, L) in ((200.0, 3.0), (230.0, 2.4), (310.0, 2.4), (340.0, 3.0), (180.0, 2.0), (0.0, 2.0)):
        c, sn = math.cos(math.radians(ang)), math.sin(math.radians(ang))
        d.line("art_yellow", [(cx + 2.6 * c, 21.6 + 2.6 * sn), (cx + (2.6 + L) * c, 21.6 + (2.6 + L) * sn)], 0.4)
    # (its plume: up off the stack, out under the portal's top and rolling up over the gable, drifting off behind)
    for (u, v, rr, sd) in ((0.0, 26.6, 1.8, 11), (1.4, 29.6, 2.8, 12), (-1.2, 33.4, 3.6, 13), (2.4, 37.6, 4.4, 14), (8.0, 41.6, 5.0, 15),
                           (15.0, 44.0, 5.4, 16)):
        puff(d, cx + u, v, rr, "art_white", seed=sd, ink=ink * 0.5)
    for (u, v, rr, sd) in ((3.0, 36.6, 2.2, 21), (9.6, 40.6, 2.6, 22), (16.4, 43.0, 2.8, 23)):
        puff(d, cx + u, v - 1.4, rr, "art_lgrey", seed=sd, ink=0.0)
    for j in range(n0, len(d.ops)):                    # (the whole of it a size up: the engine must read at a glance)
        op = d.ops[j]
        if op[0] == "polys":
            d.ops[j] = ("polys", op[1], [[(u * 1.3, v * 1.3) for u, v in poly] for poly in op[2]])
    place_name(d, a, lines=["Covered", "Bridge", "Express"], where="right", room=6.0, at=30.0, gap=1.32, caps=21.5)


# ---- 174: Ladybug Landing ------------------------------------------------------------------------------------------------------------
@painting(174)
def p174(d, a):   # a ladybug in flying goggles coming in to land on a daisy, wings out, legs down
    ink = d.ink
    # (the daisy on its stem: white petals, gold eye, two leaves)
    fx, fy = 26.0, 30.0
    d.line("art_green", [(fx, 0.0), (fx - 2.0, 14.0), (fx, fy)], 1.4)
    for (u, v, t) in ((-2.0, 10.0, 150.0), (-1.0, 18.0, 30.0)):
        G = _T(fx + u, v, t)
        d.fill("art_green", [G(0.0, 0.0), G(4.0, 2.0), G(9.0, 0.0), G(4.0, -2.0)], ink=ink * 0.7, steps=2)
    for k in range(14):
        G = _T(fx, fy, k * 360.0 / 14)
        d.fill("art_white", [G(2.0, -1.4), G(8.0, -1.8), G(11.0, 0.0), G(8.0, 1.8), G(2.0, 1.4)], ink=ink * 0.6, steps=2)
    d.oval("art_gold", fx, fy, 4.0, 4.0, ink=ink * 0.8)
    for (u, v) in ((-1.4, 1.0), (1.2, 1.4), (0.0, -1.4), (1.8, -0.6)):
        d.oval("art_orange", fx + u, fy + v, 0.5, 0.5, ink=0.0)
    # (the ladybug above it, coming down: wing cases up and spread, the clear wings out, legs reaching down)
    bx, by = fx + 4.0, fy + 26.0
    for su in (-1.0, 1.0):
        G = _T(bx, by + 2.0, 90.0 + su * 50.0)
        d.fill("art_lgrey", [G(0.0, -1.6), G(14.0, -3.0), G(17.0, 0.0), G(14.0, 3.0), G(0.0, 1.6)], ink=ink * 0.6, steps=2)
        d.line("art_grey", [G(2.0, 0.0), G(15.0, 0.0)], 0.3)
    for (u0, u1) in ((-3.0, -6.0), (0.0, -1.0), (3.0, 5.0)):
        d.line("art_black", [(bx + u0, by - 3.0), (bx + u0 * 1.4, by - 7.0), (bx + u1, by - 9.0)], 0.5)
    d.oval("art_red", bx, by, 7.0, 6.4, ink=ink)
    for su in (-1.0, 1.0):
        G = _T(bx, by, 90.0 + su * 28.0)
        d.fill("art_red", [G(0.0, 0.0), G(2.0, su * -6.0), G(8.0, su * -5.0), G(6.0, su * 1.0)], ink=ink * 0.8, steps=2)
    d.line("art_black", [(bx, by - 6.4), (bx, by + 6.4)], 0.5, taper=False)
    for (u, v) in ((-3.6, 2.0), (3.6, 2.0), (-3.0, -2.6), (3.0, -2.6), (0.0, -4.6)):
        d.oval("art_black", bx + u, by + v, 1.3, 1.2, ink=0.0)
    # (its head below, looking down at the daisy: black, white cheek spots, goggles on, two feelers)
    hx, hy = bx, by - 7.0
    d.oval("art_black", hx, hy, 3.4, 2.6, ink=ink * 0.6)
    for su in (-1.0, 1.0):
        d.oval("art_white", hx + su * 2.0, hy + 0.6, 0.8, 0.6, ink=0.0)
        d.line("art_black", [(hx + su * 1.2, hy - 2.0), (hx + su * 3.0, hy - 4.6), (hx + su * 4.4, hy - 4.6)], 0.35)
    d.line("art_brown", [(hx - 3.4, hy - 0.4), (hx + 3.4, hy - 0.4)], 0.6, taper=False)
    for su in (-1.0, 1.0):
        d.oval("art_lsteel", hx + su * 1.4, hy - 0.6, 1.2, 1.0, ink=ink * 0.7)
        d.oval("art_sky", hx + su * 1.4, hy - 0.6, 0.7, 0.6, ink=0.0)
    # (its landing: a dashed path down to the daisy)
    for i in range(4):
        v = by + 10.0 + i * 3.6
        d.line("art_white", [(bx - 6.0 - i * 2.4, v), (bx - 7.4 - i * 2.4, v + 1.6)], 0.5)
    place_name(d, a, lines=["Ladybug", "Landing"], where="right", room=6.0, at=36.0, caps=21.5)


# ---- 175: Harmonica Hal --------------------------------------------------------------------------------------------------------------
@painting(175)
def p175(d, a):   # a soldier in fatigues sitting on a supply crate, playing his harmonica with both hands, eyes shut
    ink = d.ink
    x, y, h = 24.0, 0.0, 72.0
    kw = dict(pose="sit", arms=("mouth", "mouth"), hat="garrison",
              wear={"shirt": "art_khaki", "sleeves": "rolled", "trousers": "art_khaki", "boots": "art_brown", "cap": "art_khaki"}, hair="short", eyes="shut",
              mouth="none")
    F0 = man(Drawing(1), x, y, h, 1, **kw)
    seat = F0.PL(*MAN_LEGS["sit"][0][0])[1] - F0.r(0.3)
    floor = F0.P(0.0, 0.0)[1]
    # (the crate: boards, stencilled marks as bars only)
    c0, c1 = F0.P(-1.1, 0.0)[0], F0.P(0.8, 0.0)[0]
    d.shape("art_tan", [(c0, floor), (c0, seat), (c1, seat), (c1, floor)], ink=ink)
    for f in (0.33, 0.66):
        v = floor + (seat - floor) * f
        d.line("art_brown", [(c0, v), (c1, v)], 0.4, taper=False)
    d.line("art_brown", [(c0 + 0.6, floor + 0.6), (c1 - 0.6, seat - 0.6)], 0.6, taper=False)
    F = man(d, x, y, h, 1, **kw)
    # (the harmonica at his lips between his hands: its steel cover, its holes)
    mx, my = F.PH(0.3, 6.46)
    d.shape("art_lsteel", [(mx - 1.6, my - 1.0), (mx - 1.6, my + 1.0), (mx + 3.4, my + 1.2), (mx + 3.4, my - 0.8)], ink=ink * 0.8)
    for t in range(5):
        d.oval("art_dark", mx - 0.8 + t * 0.9, my + 0.1, 0.25, 0.3, ink=0.0)
    for side in ("fore", "aft"):
        F.grip(side)
    # (the tune, rising)
    note(d, mx + 8.0, my + 2.0, 7.0, "art_white", turn=-10.0)
    note(d, mx + 14.0, my + 10.0, 8.0, "art_white", turn=8.0)
    note(d, mx + 6.0, my + 16.0, 6.0, "art_white")
    place_name(d, a, lines=["Harmonica", "Hal"], where="below", room=5.0, caps=21.5)
