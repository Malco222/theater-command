"""Batch 3, numbers 88 to 94 (painter b3_b): a cane pole by a fishing hole, a tabby dancing on a tin roof under her
parasol, a paperboy on his bicycle, a fisherwoman in oilskins with her codfish, a strongman with his barbell, a
grandmother knitting in a winged rocking chair and a clerk buried in his forms. Each picture is drawn from flat shapes
with noseart's Drawing API and the figure kit; each name is set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T, _lady, umbrella  # noqa: E402


def _arc(cx, cy, rx, ry, a0, a1, n=12):
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
            for i in range(n + 1)]


# ---- 88: the fishing hole ---------------------------------------------------------------------------------------------------------
def straw_hat(d, x, y, w, turn=0.0, straw="art_tan", band="art_red"):
    """A farmer's straw hat lying on the grass, its brim's middle at (x, y), `w` across: the brim, the crown, its band."""
    G = _T(x, y, turn)
    d.fill(straw, [G(-w * 0.5, 0.0), G(-w * 0.32, w * 0.07), G(0.0, w * 0.1), G(w * 0.32, w * 0.07), G(w * 0.5, 0.0), G(w * 0.32, -w * 0.08),
                   G(0.0, -w * 0.1), G(-w * 0.32, -w * 0.08)], steps=4)
    d.fill(straw, [G(-w * 0.26, w * 0.02), G(-w * 0.24, w * 0.3), G(-w * 0.1, w * 0.38), G(w * 0.1, w * 0.38), G(w * 0.24, w * 0.3), G(w * 0.26, w * 0.02)],
           steps=4)
    d.shape(band, [G(-w * 0.26, w * 0.03), G(-w * 0.255, w * 0.11), G(w * 0.255, w * 0.11), G(w * 0.26, w * 0.03)], ink=d.ink * 0.6)


@painting(88)
def p88(d, a):   # a cane pole and its bobber by a pond, a straw hat left on the bank
    # (the pond, its far shore a little darker)
    d.fill("art_blue", [(14.0, 4.0), (30.0, 12.5), (58.0, 14.0), (80.0, 11.0), (86.0, 4.0), (76.0, -3.0), (48.0, -5.0), (22.0, -3.0)], steps=5)
    # (the bank it lies on, at the left, with tufts of grass)
    d.fill("art_green", [(-4.0, -4.0), (-4.0, 16.0), (6.0, 20.0), (16.0, 18.0), (26.0, 10.0), (28.0, 2.0), (20.0, -4.0)], steps=4)
    for gx, gy in ((2.0, 18.4), (12.0, 18.6), (22.0, 12.6)):
        d.line("art_green", [(gx - 1.6, gy - 1.0), (gx - 2.4, gy + 2.8)], 0.9)
        d.line("art_green", [(gx, gy - 1.0), (gx + 0.4, gy + 3.6)], 0.9)
        d.line("art_green", [(gx + 1.6, gy - 1.0), (gx + 2.8, gy + 2.6)], 0.9)
    # (the forked stick it rests on)
    d.line("art_brown", [(12.0, 14.0), (13.0, 26.0)], 1.4, taper=False)
    d.line("art_brown", [(13.0, 24.0), (10.4, 29.0)], 1.1)
    d.line("art_brown", [(13.0, 24.0), (16.2, 28.6)], 1.1)
    # (the cane pole: butt on the bank, over the fork, bending out over the water; its joints)
    pole = [(-2.0, 12.0), (13.2, 26.0), (30.0, 40.0), (46.0, 51.0), (60.0, 57.0), (70.0, 59.0)]
    d.limb("art_tan", pole, [1.9, 1.75, 1.5, 1.2, 0.9, 0.7], ink=d.ink * 0.8)
    for t in (0.18, 0.36, 0.55, 0.72, 0.86):
        i = int(t * (len(pole) - 1))
        f = t * (len(pole) - 1) - i
        px, py = lerp(pole[i], pole[i + 1], f)
        dx, dy = pole[i + 1][0] - pole[i][0], pole[i + 1][1] - pole[i][1]
        ln = math.hypot(dx, dy)
        nx, ny = -dy / ln * 1.8 * (1.1 - t * 0.6), dx / ln * 1.8 * (1.1 - t * 0.6)
        d.line("art_brown", [(px + nx, py + ny), (px - nx, py - ny)], 0.6, taper=False)
    # (the line, down from its tip to the bobber, and the bobber riding the water in its rings)
    bx, by = 66.0, 6.0
    d.line("art_white", [(70.0, 59.0), (69.4, 40.0), (67.8, 20.0), (bx, by + 3.4)], 0.45, taper=False)
    for rr in (5.0, 8.4):
        d.line("art_white", _arc(bx, by - 0.4, rr, rr * 0.3, 190, 350), 0.6)
    d.oval("art_white", bx, by, 2.6, 2.6, ink=d.ink * 0.8)
    d.fill("art_red", _arc(bx, by, 2.6, 2.6, 0, 180, 10), ink=d.ink * 0.6, steps=1)
    d.line("art_white", [(bx, by + 2.4), (bx, by + 4.4)], 0.6, taper=False)
    # (the hat, left on the bank)
    straw_hat(d, 6.0, 4.0, 16.0, turn=6.0)
    place_name(d, a, lines=["Fishing", "Hole"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 89: the tabby on the tin roof ------------------------------------------------------------------------------------------------
def tin_roof(d, x0, x1, y, h, tin="art_grey", rib="art_lsteel", rust="art_auburn"):
    """A corrugated tin roof's near slope seen from a little below its ridge: the ridge along y + h from x0 to x1, the
    slope coming down toward us to its eave at y, the eave scalloped by the corrugations; ribs down the slope, a rust
    streak or two, and the ridge cap along the top."""
    n = int((x1 - x0) / 3.6)
    pitch = (x1 - x0) / n
    eave = []
    for i in range(n + 1):
        x = x0 - 2.0 + (x1 - x0 + 4.0) * i / n
        eave += [(x, y), (x + pitch * 0.5, y - 0.9)] if i < n else [(x, y)]
    d.shape(tin, [(x0, y + h)] + eave + [(x1, y + h)])
    for i in range(1, n + 1):
        xt = x0 + (x1 - x0) * (i - 0.5) / n
        xb = x0 - 2.0 + (x1 - x0 + 4.0) * (i - 0.5) / n
        d.line(rib, [(xt, y + h - 1.0), (xb, y + 0.6)], 0.7, taper=False)
    for (u, l) in ((0.22, 0.5), (0.7, 0.36)):
        xt = x0 + (x1 - x0) * u
        d.line(rust, [(xt, y + h * 0.55), (xt - 0.6, y + h * (0.55 - l))], 1.1)
    d.fill("art_lsteel", [(x0 - 1.0, y + h - 1.2), (x0 - 1.0, y + h + 1.4), (x1 + 1.0, y + h + 1.4), (x1 + 1.0, y + h - 1.2)], steps=1)


def tabby(d, x, y, size, kick=1.0, coat="art_orange", stripe="art_brown", white="art_white"):
    """A tabby cat up on her hind legs, dancing, facing right: her feet at (x, y), `size` her height to her ears. Her
    near foot planted, the far one kicked up behind; her near paw up (for a parasol's handle: returned), the far one out
    for balance; her tail up in an S. Returns (near paw, far paw)."""
    k = size / 40.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    # (her tail, behind her: up from her rump in an S, ringed, its tip dark)
    tail = Ps([(-3.0, 14.0), (-8.0, 15.0), (-11.5, 20.0), (-10.0, 26.0), (-12.0, 31.0), (-15.0, 33.0)])
    d.limb(coat, tail, [1.6 * k, 1.5 * k, 1.45 * k, 1.4 * k, 1.35 * k, 1.2 * k], ink=ink)
    for t in (0.25, 0.45, 0.65):
        i = int(t * 5)
        p = lerp(tail[i], tail[i + 1], t * 5 - i)
        d.oval(stripe, p[0], p[1], 1.2 * k, 1.5 * k, turn=30, ink=0.0)
    d.oval(stripe, *tail[-1], 1.4 * k, 1.3 * k, ink=ink * 0.7)
    # (her far leg, kicked up behind her, and her far arm out for balance)
    d.limb(coat, Ps([(-1.0, 12.0), (-4.0, 7.0), (-8.0, 6.0 + 2.0 * kick)]), [2.4 * k, 1.8 * k, 1.3 * k], ink=ink)
    d.oval(white, *P(-9.2, 6.4 + 2.0 * kick), 1.8 * k, 1.3 * k, turn=20, ink=ink * 0.8)
    far = P(-10.0, 23.0)
    d.limb(coat, Ps([(-2.0, 21.0), (-6.0, 23.0), (-10.0, 23.0)]), [1.7 * k, 1.4 * k, 1.2 * k], ink=ink)
    d.oval(white, *far, 1.5 * k, 1.4 * k, ink=ink * 0.8)
    # (her body: a pear, a little leaning; her white chest; her near leg planted)
    d.fill(coat, Ps([(-4.0, 10.0), (-5.0, 15.0), (-3.4, 21.0), (0.0, 24.0), (3.6, 21.0), (4.4, 15.0), (3.0, 10.0), (0.0, 8.6)]), ink=ink, steps=4)
    d.fill(white, Ps([(0.6, 21.6), (2.6, 18.0), (2.6, 13.0), (1.2, 10.2), (-0.6, 13.4), (-0.6, 18.0)]), ink=0.0, steps=4)
    for (u, v, r) in ((-3.0, 18.4, 14), (-3.6, 15.2, 6), (-3.0, 12.0, -4)):
        d.line(stripe, Ps([(u, v), (u + 1.6, v + 0.3), (u + 2.6, v + 0.2)]), 1.0 * k)
    d.limb(coat, Ps([(1.0, 11.0), (2.0, 6.0), (1.6, 1.4)]), [2.5 * k, 1.9 * k, 1.5 * k], ink=ink)
    d.oval(white, *P(2.8, 0.9), 2.4 * k, 1.2 * k, ink=ink * 0.8)
    # (her head: round, its cheeks full; ears up; a white muzzle; her eyes shut in delight; whiskers)
    hx, hy, hr = 1.4, 30.0, 6.4
    for su in (-1, 1):
        ex = hx + su * 3.6
        d.fill(coat, Ps([(ex - 2.4, hy + 3.2), (ex + su * 0.6, hy + 9.6), (ex + 2.4, hy + 3.2)]), ink=ink, steps=2)
        d.fill("art_pink", Ps([(ex - 1.2, hy + 4.2), (ex + su * 0.4, hy + 7.8), (ex + 1.2, hy + 4.2)]), ink=0.0, steps=2)
    d.fill(coat, Ps([(hx - hr, hy + 0.6), (hx - hr * 0.86, hy + 4.4), (hx, hy + hr), (hx + hr * 0.86, hy + 4.4), (hx + hr, hy + 0.6),
                     (hx + hr * 1.08, hy - 2.6), (hx + 2.0, hy - hr + 0.4), (hx - 2.0, hy - hr + 0.4), (hx - hr * 1.08, hy - 2.6)]), ink=ink, steps=4)
    for su in (-1.6, 0.0, 1.6):
        d.line(stripe, Ps([(hx + su, hy + hr - 0.6), (hx + su * 1.1, hy + hr - 2.6)]), 0.9 * k)
    d.oval(white, *P(hx + 0.6, hy - 2.6), 3.6 * k, 2.4 * k, ink=0.0)
    d.oval("art_pink", *P(hx + 0.6, hy - 1.2), 0.9 * k, 0.6 * k, ink=ink * 0.5)
    d.line("art_ink", Ps([(hx - 1.4, hy - 3.0), (hx - 0.4, hy - 3.9), (hx + 0.6, hy - 3.2), (hx + 1.6, hy - 3.9), (hx + 2.6, hy - 3.0)]), 0.35 * k)
    for su in (-1, 1):
        ex = hx + 0.6 + su * 2.6
        d.line("art_ink", Ps([(ex - 1.2, hy + 1.0), (ex, hy + 1.8), (ex + 1.2, hy + 1.0)]), 0.45 * k, taper=False)
        for dv in (-0.6, 0.4):
            d.line("art_ink", Ps([(hx + 0.6 + su * 2.8, hy - 2.4 + dv * 0.6), (hx + 0.6 + su * 7.6, hy - 2.0 + dv * 1.8)]), 0.25 * k)
    # (her near arm, up to the parasol's handle)
    near = P(7.0, 27.0)
    d.limb(coat, Ps([(2.0, 21.0), (5.6, 22.0), (7.0, 27.0)]), [1.8 * k, 1.5 * k, 1.3 * k], ink=ink)
    return near, far


@painting(89)
def p89(d, a):   # a tabby cat dancing on a tin roof under her parasol
    tin_roof(d, -4.0, 62.0, -8.0, 14.0)
    near, far = tabby(d, 26.0, 7.0, 56.0, kick=1.4)
    umbrella(d, near[0] + 3.0, near[1] + 24.0, 20.0, near, cloth="art_pink", rib="art_ink", shaft="art_lsteel", ribs=7)
    d.oval("art_white", *near, 1.6, 1.5, ink=d.ink * 0.8)                    # (her paw round the handle)
    place_name(d, a, lines=["Tin Roof", "Tabby"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 90: the paperboy -----------------------------------------------------------------------------------------------------------
def bicycle(d, C, r, head, grips, saddle, frame="art_red", tyre_rim="art_lsteel"):
    """A boy's bicycle side-on, facing right: its crank at C, its wheels `r`, the top of its head tube at `head`, its
    grips at `grips`, its saddle's top at `saddle`; a basket on its front. Returns the front axle."""
    ax_y = C[1] + 0.12 * r
    rear, front = (C[0] - 1.36 * r, ax_y), (C[0] + 1.78 * r, ax_y)
    for (wx, wy) in (rear, front):
        wheel(d, wx, wy, r, rim=tyre_rim, spokes=12)
    w = r * 0.15
    seat_top = (saddle[0] + 0.1 * r, saddle[1] - 0.18 * r)
    head_low = lerp(head, front, 0.28)
    for a_, b_ in ((C, seat_top), (seat_top, head), (C, head_low), (C, rear), (seat_top, rear), (head, front)):
        d.line(frame, [a_, b_], w, taper=False)
    d.line(frame, [head, lerp(head, front, 0.32)], w * 1.4, taper=False)
    # (the bars, up and back from the head tube to the grips; the grips)
    d.line("art_lsteel", [head, (head[0], head[1] + 0.22 * r), grips], w * 0.75, taper=False)
    d.oval("art_black", grips[0], grips[1], 0.13 * r, 0.09 * r, ink=d.ink * 0.5)
    # (the saddle, the chain ring and its crank)
    d.fill("art_black", [(saddle[0] - 0.3 * r, saddle[1]), (saddle[0] + 0.32 * r, saddle[1] - 0.02 * r), (saddle[0] + 0.2 * r, saddle[1] - 0.12 * r),
                         (saddle[0] - 0.26 * r, saddle[1] - 0.12 * r)], ink=d.ink * 0.8, steps=2)
    d.oval("art_lsteel", C[0], C[1], 0.22 * r, 0.22 * r, ink=d.ink * 0.7)
    d.line("art_dark", [(C[0], C[1] + 0.22 * r), (rear[0], rear[1] + 0.08 * r)], w * 0.35, taper=False)
    d.line("art_dark", [(C[0], C[1] - 0.22 * r), (rear[0], rear[1] - 0.08 * r)], w * 0.35, taper=False)
    return front


@painting(90)
def p90(d, a):   # a paperboy on his bicycle, throwing a rolled newspaper
    x, y, h = 30.0, 8.0, 46.0
    legs = ([(0.3, 3.8), (1.42, 3.2), (1.3, 1.62)], [(-0.18, 3.8), (0.22, 2.06), (0.5, 0.5)])
    kw = dict(pose=legs, arms=(to(2.6, 6.6, "down"), to(2.15, 5.0, "down")), tilt=10.0, hat="newsboy",
              wear={"shirt": "art_lblue", "sleeves": "rolled", "knickers": "art_khaki", "socks": "art_white", "shoes": "art_brown", "cap": "art_grey"})
    S = kid(Drawing(1), x, y, h, 1, **kw)                # (a first, unseen try: where his feet, seat and hands fall)
    C = lerp(S.PL(1.3, 1.62), S.PL(0.5, 0.5), 0.5)
    r = h * 0.24
    saddle = S.PL(-0.12, 3.54)
    grips = S.hand["aft"][0]
    head = (grips[0] + 0.1 * r, grips[1] - 0.42 * r)
    front = bicycle(d, C, r, head, grips, saddle)
    # (his basket over the front wheel, the day's papers rolled in it)
    bxy = (front[0] + 0.05 * r, head[1] - 0.05 * r)
    for i, (u, v) in enumerate(((-0.34, 0.5), (-0.08, 0.6), (0.18, 0.52))):
        d.oval("art_white", bxy[0] + u * r, bxy[1] + v * r, 0.14 * r, 0.3 * r, turn=12 - i * 12, ink=d.ink * 0.7)
    d.shape("art_tan", [(bxy[0] - 0.55 * r, bxy[1] + 0.48 * r), (bxy[0] + 0.45 * r, bxy[1] + 0.48 * r), (bxy[0] + 0.38 * r, bxy[1] - 0.12 * r),
                        (bxy[0] - 0.48 * r, bxy[1] - 0.12 * r)])
    for t in (0.25, 0.5, 0.75):
        d.line("art_brown", [(bxy[0] - 0.55 * r + t * r, bxy[1] + 0.46 * r), (bxy[0] - 0.5 * r + t * 0.86 * r, bxy[1] - 0.1 * r)], 0.5, taper=False)
    F = kid(d, x, y, h, 1, **kw)
    # (the paper he has just let go: rolled, tied, flying on ahead, the rush of it behind)
    hx, hy = F.hand["fore"][0]
    px, py = hx + 8.0, hy + 3.0
    d.oval("art_white", px, py, 4.6, 1.9, turn=24, ink=d.ink)
    d.line("art_red", [(px - 0.8, py - 1.9), (px - 0.0, py + 1.6)], 0.6, taper=False)
    d.line("art_ink", [(px + 1.4, py - 0.2), (px + 3.0, py + 0.6)], 0.25)
    speed(d, px - 5.6, py - 2.2, 6.0, n=2, gap=2.4, paint="art_white", turn=24, w=0.8)
    place_name(d, a, lines=["Morning", "Edition"], where="right", room=6.0, at=32.0, caps=21.5)


# ---- 91: Bay State Bess ------------------------------------------------------------------------------------------------------------
def codfish(d, x, y, L, turn=0.0, back="art_khaki", belly="art_cream", spot="art_brown", line="art_white"):
    """A big codfish side-on, its middle at (x, y), `L` long, its head to the right: the long body, three fins on its
    back and two under, a square tail, the pale line along its side, its spots, its chin's barbel, its round eye."""
    G = _T(x, y, turn)
    h = L * 0.13
    e = L * 0.5
    for (u0, u1, up) in ((-0.30, -0.12, 1), (-0.04, 0.12, 1), (0.16, 0.30, 1), (-0.30, -0.1, -1), (-0.04, 0.12, -1)):
        if up < 0 and u1 > 0.12:
            continue
        uu0, uu1 = u0 * L, u1 * L
        v0 = h * 0.86 * up
        d.fill(back, [G(uu0, v0 * 0.9), G(uu0 + (uu1 - uu0) * 0.2, v0 + h * 0.62 * up), G(uu1 - (uu1 - uu0) * 0.2, v0 + h * 0.5 * up), G(uu1, v0 * 0.9)],
               ink=d.ink * 0.7, steps=2)
    d.fill(back, [G(-e + L * 0.02, 0.0), G(-e - L * 0.08, h * 0.9), G(-e - L * 0.1, h * 0.7), G(-e - L * 0.1, -h * 0.7), G(-e - L * 0.08, -h * 0.9)],
           ink=d.ink * 0.8, steps=2)
    body = [G(-e, -h * 0.28), G(-e, h * 0.28), G(-L * 0.3, h * 0.82), G(0.0, h), G(L * 0.3, h * 0.92), G(e - L * 0.05, h * 0.5), G(e, 0.0),
            G(e - L * 0.04, -h * 0.4), G(L * 0.3, -h * 0.86), G(0.0, -h * 0.92), G(-L * 0.3, -h * 0.7)]
    d.fill(back, body, ink=d.ink, steps=4)
    d.fill(belly, [G(-L * 0.36, -h * 0.4), G(-L * 0.1, -h * 0.5), G(L * 0.24, -h * 0.5), G(e - L * 0.08, -h * 0.3), G(L * 0.3, -h * 0.82),
                   G(0.0, -h * 0.88), G(-L * 0.3, -h * 0.66)], ink=0.0, steps=4)
    d.line(line, [G(-e + L * 0.04, 0.0), G(-L * 0.2, h * 0.18), G(L * 0.12, h * 0.22), G(L * 0.3, h * 0.14)], h * 0.12)
    for (u, v) in ((-0.32, 0.4), (-0.22, 0.56), (-0.12, 0.42), (0.0, 0.62), (0.08, 0.44), (0.2, 0.6), (0.3, 0.42), (-0.06, 0.78)):
        d.oval(spot, *G(u * L, v * h), h * 0.09, h * 0.07, ink=0.0)
    # (its head: the gill's curve, the eye, the mouth, the barbel under its chin)
    d.line("art_ink", [G(L * 0.33, h * 0.78), G(L * 0.3, h * 0.2), G(L * 0.33, -h * 0.66)], h * 0.07)
    d.oval("art_white", *G(L * 0.42, h * 0.38), h * 0.17, h * 0.17, ink=d.ink * 0.7)
    d.oval("art_black", *G(L * 0.43, h * 0.38), h * 0.08, h * 0.08, ink=0.0)
    d.line("art_ink", [G(e, -h * 0.04), G(e - L * 0.07, -h * 0.12)], h * 0.07)
    d.line(back, [G(e - L * 0.06, -h * 0.4), G(e - L * 0.08, -h * 0.75)], h * 0.07)


@painting(91)
def p91(d, a):   # a woman in oilskins and a sou'wester with a big codfish under her arm
    x, y, h = 30.0, 4.0, 58.0
    F = lady(d, x, y, h, 1, pose="stand", arms=("hold", "down"),
             wear={"top": "art_yellow", "coat": "art_yellow", "collar": "art_yellow", "sleeves": "long", "buttons": "art_dark", "belt": None,
                   "boots": "art_black"},
             hair="rolls", hair_paint="art_auburn")
    P, Ps, R = F.P, F.Ps, F.r
    # (her sou'wester: oilskin, a brim all round, long and down over her nape; its tie under her chin)
    col = "art_yellow"
    d.fill(col, Ps([(0.78, 7.26), (0.72, 7.16), (0.3, 7.2), (-0.1, 7.2), (-0.5, 7.1), (-0.78, 6.84), (-0.92, 6.5), (-0.76, 6.38), (-0.52, 6.48),
                    (-0.49, 6.9), (-0.38, 7.3), (0.52, 7.36)]), steps=4)
    d.fill(col, Ps([(-0.48, 7.28), (-0.52, 7.6), (-0.32, 7.86), (0.0, 7.92), (0.32, 7.86), (0.52, 7.6), (0.5, 7.3), (0.0, 7.34)]), steps=5)
    d.line("art_ink", Ps([(-0.34, 7.8), (-0.12, 7.54), (-0.08, 7.34)]), R(0.025))
    d.line("art_ink", Ps([(0.3, 7.82), (0.22, 7.56), (0.2, 7.34)]), R(0.025))
    d.line("art_dark", Ps([(-0.4, 6.9), (-0.3, 6.5), (0.0, 6.28), (0.3, 6.5), (0.42, 6.9)]), R(0.04))
    # (her codfish, under her arm: its head forward, its tail behind her)
    codfish(d, *P(0.5, 4.62), 34.0, turn=-6.0)
    # (her near arm again, over it, its hand round its belly)
    sh = (SHOULDER[0], SHOULDER[1])
    el, hd = ARMS["hold"]
    E, Hn = (sh[0] + el[0], sh[1] + el[1]), (sh[0] + hd[0] + 0.62, sh[1] + hd[1] + 0.12)
    d.limb(col, Ps([sh, lerp(sh, E, 0.5), E, lerp(E, Hn, 0.45), Hn]), [R(0.27), R(0.235), R(0.19), R(0.18), R(0.15)], ink=d.ink)
    d.oval("art_skin", *P(Hn[0] - 0.08, Hn[1] - 0.04), R(0.15), R(0.13), ink=d.ink * 0.7)
    place_name(d, a, lines=["Bay State", "Bess"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 92: Barbell Bruno -------------------------------------------------------------------------------------------------------------
@painting(92)
def p92(d, a):   # a strongman in a striped bathing suit, a barbell held high over his head
    x, y, h = 34.0, 4.0, 56.0
    F = man(d, x, y, h, 1, pose="wide", arms=(to(1.62, 9.0, "out"), to(-1.62, 9.0, "out")),
            wear={"shirt": "art_red", "sleeves": "bare", "stripes": "art_white", "stripe_dir": "h", "shorts": "art_red", "belt": "art_black",
                  "buckle": "art_gold", "shoes": "art_black"},
            hair="slick", moustache="full", mouth="grin", eyes="shut")
    (fx, fy), _ = F.hand["fore"]
    (ax, ay), _ = F.hand["aft"]
    # (the bar across his hands, a great black ball at each end)
    ext = 9.0
    vx, vy = fx - ax, fy - ay
    ln = math.hypot(vx, vy)
    ux, uy = vx / ln, vy / ln
    e0, e1 = (ax - ux * ext, ay - uy * ext), (fx + ux * ext, fy + uy * ext)
    d.line("art_lsteel", [e0, e1], 1.5, taper=False)
    for (bx, by) in (e0, e1):
        d.oval("art_black", bx, by, 7.6, 7.6, ink=d.ink)
        d.oval("art_grey", bx - 2.4, by + 2.6, 2.2, 1.6, turn=30, ink=0.0)
    F.grip("fore")
    F.grip("aft")
    for su in (-1, 1):                                    # (the strain: short lines out from his arms)
        bx, by = (e0 if su < 0 else e1)
        d.line("art_white", [(bx + su * 9.4, by - 1.0), (bx + su * 12.4, by - 2.0)], 0.8)
        d.line("art_white", [(bx + su * 9.0, by + 3.4), (bx + su * 12.0, by + 4.6)], 0.8)
    place_name(d, a, lines=["Barbell", "Bruno"], where="right", room=6.0, at=30.0, caps=21.5)


# ---- 93: Granny's Rocker ---------------------------------------------------------------------------------------------------------
def rocker(d, F, wood="art_brown", seat="art_lbrown"):
    """A rocking chair under a seated figure's frame F (heads, facing her way): its rockers, legs, seat, arms, spindled
    tall back. Returns the top of its back (for the wings)."""
    R = F.r
    w = R(0.16)
    d.fill(wood, F.Ps([(-2.4, 1.7), (-1.2, 1.18), (0.4, 1.06), (1.8, 1.24), (2.7, 1.62), (2.66, 1.4), (1.8, 0.98), (0.4, 0.8), (-1.2, 0.92),
                       (-2.5, 1.48)]), steps=4)
    for (u0, u1) in ((-0.7, -0.8), (1.0, 1.1)):
        d.line(wood, F.Ps([(u1, 1.1), (u0, 3.4)]), w, taper=False)
    d.line(wood, F.Ps([(-0.8, 1.9), (1.06, 1.9)]), w * 0.6, taper=False)
    # (the back: two stiles leaning back, a top rail, spindles)
    top = (-1.5, 8.3)
    d.line(wood, F.Ps([(-0.86, 3.4), top]), w, taper=False)
    d.line(wood, F.Ps([(-0.52, 3.4), (-1.16, 8.3)]), w, taper=False)
    d.fill(wood, F.Ps([(-1.76, 8.0), (-1.7, 8.6), (-0.92, 8.7), (-0.86, 8.1)]), steps=2)
    for t in (0.33, 0.66):
        d.line(wood, F.Ps([lerp((-0.86, 3.6), top, 0.0) if False else (-0.82 + t * 0.2, 3.6), (-1.6 + t * 0.4, 8.0)]), w * 0.6, taper=False)
    # (the seat, and the arm, its front post)
    d.fill(seat, F.Ps([(-0.9, 3.36), (-0.9, 3.66), (1.2, 3.6), (1.22, 3.3)]), steps=1)
    d.line(wood, F.Ps([(1.1, 3.5), (1.16, 5.0)]), w * 0.8, taper=False)
    d.line(wood, F.Ps([(-0.9, 5.3), (0.4, 5.12), (1.36, 5.08)]), w * 0.9, taper=False)
    return F.P(*top)


@painting(93)
def p93(d, a):   # a grandmother knitting in a winged rocking chair
    x, y, h = 30.0, 2.0, 54.0
    Fr = Fig(d, x, y, h, 1)
    top = rocker(d, Fr)
    wing(d, Fr.P(-1.0, 7.0), 178.0, 22.0, 9.0, "art_lgrey", shade="art_grey", spread=-24)
    wing(d, Fr.P(-1.3, 7.6), 132.0, 28.0, 10.5, "art_white", spread=26)
    F = _lady(d, x, y, h, 1, extra_arms={"_b3b_knit_f": ((0.3, -1.1), (0.42, -1.32)), "_b3b_knit_a": ((0.0, -1.15), (-1.56, -1.24))},
              pose="sit", arms=("_b3b_knit_f", "_b3b_knit_a"),
             wear={"top": "art_navy", "hem": "long", "sleeves": "long", "shoes": "art_black", "collar": "art_white"},
             hair="bun", hair_paint="art_lgrey")
    P, Ps, R = F.P, F.Ps, F.r
    # (her shawl round her shoulders, its point down her back)
    d.fill("art_red", Ps([(-0.78, 6.1), (-0.9, 5.4), (-0.6, 4.9), (0.0, 5.1), (0.7, 5.3), (0.86, 5.9), (0.5, 6.3), (0.0, 6.2), (-0.4, 6.34)]), steps=3)
    # (her spectacles)
    for ex in (-0.1, 0.25):
        d.line("art_ink", ellipse(*P(ex, 6.9), R(0.12), R(0.1), 0, 24), R(0.03), taper=False)
    d.line("art_ink", Ps([(0.02, 6.92), (0.13, 6.92)]), R(0.03), taper=False)
    # (the knitting: two needles crossed at her hands, the work hanging from them, the yarn down to its ball)
    (h1, _), (h2, _) = F.hand["fore"], F.hand["aft"]
    mx, my = (h1[0] + h2[0]) / 2, (h1[1] + h2[1]) / 2
    d.line("art_lsteel", [(mx - 3.0, my - 2.4), (mx + 5.4, my + 5.0)], 0.7, taper=False)
    d.line("art_lsteel", [(mx + 3.0, my - 2.4), (mx - 3.6, my + 5.6)], 0.7, taper=False)
    knit = [(mx - 3.0, my - 0.6), (mx + 3.4, my - 0.6), (mx + 3.0, my - 7.0), (mx - 2.6, my - 7.4)]
    d.shape("art_red", knit)
    for i in range(1, 5):
        yy = my - 0.6 - i * 1.4
        d.line("art_white", [(mx - 2.8, yy), (mx + 3.1, yy + 0.1)], 0.3, taper=False)
    bx, by = x + 24.0, y + 9.0
    d.line("art_red", [(mx + 2.6, my - 7.0), (mx + 6.0, by + 8.0), (bx - 2.0, by + 5.0), (bx, by + 3.6)], 0.5)
    d.oval("art_red", bx, by, 3.6, 3.4, ink=d.ink * 0.8)
    for t in (-30, 10, 50):
        d.line("art_white", _arc(bx, by, 3.0, 2.8, t, t + 110, 6), 0.3, taper=False)
    place_name(d, a, lines=["Granny's", "Rocker"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 94: In Triplicate -------------------------------------------------------------------------------------------------------------
@painting(94)
def p94(d, a):   # a clerk buried to his cap in forms
    rng = random.Random(94)
    cx, cy = 32.0, 0.0
    # (his head, peeking out of the top of the heap; his cap)
    d.oval("art_skin", cx, 43.0, 7.0, 7.6, ink=d.ink)
    for su in (-1, 1):
        d.oval("art_white", cx + su * 2.8, 44.0, 2.2, 2.6, ink=d.ink * 0.7)
        d.oval("art_black", cx + su * 2.8 + 0.9, 43.6, 1.0, 1.2, ink=0.0)
    d.line("art_ink", [(cx - 4.8, 47.6), (cx - 1.4, 47.2)], 0.5)
    d.line("art_ink", [(cx + 1.4, 47.2), (cx + 4.8, 47.8)], 0.5)
    d.fill("art_khaki", [(cx - 7.8, 47.4), (cx - 7.0, 50.4), (cx - 2.0, 52.0), (cx + 3.0, 52.2), (cx + 7.6, 50.8), (cx + 8.0, 47.4), (cx, 48.4)],
           steps=3)
    d.line("art_ink", [(cx - 6.0, 49.8), (cx + 1.0, 51.2)], 0.4)
    # (the heap of forms: sheet on sheet, each ruled, some stamped)
    sheets = []
    for i in range(40):
        t = i / 39.0
        u = rng.uniform(-30.0, 30.0)
        top = 40.0 * (1.0 - (abs(u) / 34.0) ** 1.6)
        v = rng.uniform(0.0, max(1.0, top - 6.0)) * (0.4 + 0.6 * t)
        sheets.append((u, v, rng.uniform(-40, 40)))
    sheets.sort(key=lambda s: -s[1])
    for (u, v, turn) in sheets:
        G = _T(cx + u, cy + v + 5.0, turn)
        d.shape("art_white", [G(-6.0, -4.4), G(-6.0, 4.4), G(6.0, 4.4), G(6.0, -4.4)])
        for k in range(3):
            d.line("art_lgrey", [G(-4.6, 2.4 - k * 1.8), G(4.4, 2.4 - k * 1.8)], 0.35, taper=False)
    # (his hand up out of it, his rubber stamp in it)
    d.limb("art_khaki", [(cx + 14.0, 30.0), (cx + 16.0, 38.0), (cx + 16.6, 44.0)], [2.2, 2.0, 1.8], ink=d.ink)
    d.oval("art_skin", cx + 16.6, 45.6, 2.2, 2.0, ink=d.ink * 0.8)
    d.shape("art_brown", [(cx + 15.4, 46.6), (cx + 15.4, 51.0), (cx + 17.8, 51.0), (cx + 17.8, 46.6)])
    d.oval("art_brown", cx + 16.6, 52.6, 2.0, 1.8, ink=d.ink * 0.8)
    d.shape("art_black", [(cx + 13.6, 42.0), (cx + 13.6, 44.0), (cx + 19.6, 44.0), (cx + 19.6, 42.0)])
    # (and two forms sliding off the heap)
    for (u, v, turn) in ((-34.0, 8.0, 64.0), (36.0, 14.0, -58.0)):
        G = _T(cx + u, v, turn)
        d.shape("art_white", [G(-6.0, -4.4), G(-6.0, 4.4), G(6.0, 4.4), G(6.0, -4.4)])
    place_name(d, a, lines=["In", "Triplicate"], where="right", room=6.0, at=28.0, caps=21.5)
