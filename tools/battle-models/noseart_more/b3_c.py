"""Batch 3, numbers 95 to 101 (painter b3_c): a colt blowing a bugle, a firefly lighting her way through the clouds,
a flyer skywriting one long loop, a mechanic on a winged tire, a woman in curlers pulling down the blackout shade, a
soldier with his mess tray piled high and one signpost with its one long arrow. Flat shapes with noseart's Drawing API
and the figure kit; each name set clear of its picture by place_name (b2_e)."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402
from noseart_more.b2_e import place_name  # noqa: E402
from noseart_more.b3_a import _T, _lady, note  # noqa: E402


def _arc(cx, cy, rx, ry, a0, a1, n=12):
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
            for i in range(n + 1)]


# ---- 95: the bugling colt --------------------------------------------------------------------------------------------------------
def bugle(d, G0, at, brass="art_yellow", shade="art_gold", w=1.0, k=1.0):
    """A bugle in frame G (u along the way it points), its mouthpiece at u = at: a straight tube forward, a bend down and
    back, a bend forward again, and the bell flaring out ahead; k its scale. Returns the bell mouth's middle."""
    G = lambda u, v: G0(at + (u - at) * k, v * k)        # noqa: E731
    path = [(at, 0.2), (at + 7.4, 0.4), (at + 8.8, -0.6), (at + 8.6, -1.8), (at + 7.4, -2.4), (at + 3.0, -2.4), (at + 1.6, -3.2), (at + 1.8, -4.4),
            (at + 3.0, -4.9), (at + 8.0, -4.9)]
    d.line(brass, [G(u, v) for u, v in path], w, taper=False)
    m = at + 8.0
    d.fill(brass, [G(m, -4.9 + w * 0.5), G(m + 3.6, -4.6 + 0.6), G(m + 5.6, -4.9 + 3.4), G(m + 6.0, -4.9 + 3.0), G(m + 6.0, -4.9 - 3.0),
                   G(m + 5.6, -4.9 - 3.4), G(m + 3.6, -4.9 - 0.6 - 0.2), G(m, -4.9 - w * 0.5)], steps=3)
    d.oval(shade, *G(m + 5.8, -4.9), 0.9 * k, 3.1 * k, turn=0.0, ink=d.ink * 0.6)
    d.oval(brass, *G(at - 0.3, 0.2), 0.7 * k, 0.8 * k, ink=d.ink * 0.6)
    return G(m + 6.0, -4.9)


def colt(d, x, y, size, coat="art_auburn", mane="art_brown", sock="art_white", hoof="art_dark", nod=28.0):
    """A colt standing side-on facing right, all legs and a short bristly mane, his head thrown up `nod` degrees: his
    hooves at y, his chest at x + 12 of size 40. Returns his head's frame (u along his muzzle) and his muzzle's tip u."""
    k = size / 40.0
    P = lambda u, v: (x + u * k, y + v * k)              # noqa: E731
    Ps = lambda pts: [P(u, v) for u, v in pts]           # noqa: E731
    ink = d.ink
    # (his far legs, behind)
    for top, knee, foot in (((8.0, 22.0), (9.0, 12.0), (9.6, 1.6)), ((-8.0, 22.0), (-10.6, 12.0), (-9.0, 1.6))):
        d.limb(coat, Ps([top, knee, foot]), [2.2 * k, 1.3 * k, 1.1 * k], ink=ink)
        d.oval(hoof, *P(foot[0] + 0.4, foot[1] - 0.6), 1.5 * k, 0.9 * k, ink=ink * 0.7)
    # (his tail: short, a brush)
    d.limb(mane, Ps([(-12.0, 27.0), (-16.0, 24.0), (-17.4, 18.0)]), [1.6 * k, 2.0 * k, 1.4 * k], ink=ink)
    # (his body)
    d.fill(coat, Ps([(-13.0, 26.0), (-11.0, 30.6), (0.0, 31.6), (10.0, 31.0), (14.0, 27.0), (12.0, 21.6), (0.0, 20.4), (-10.0, 21.6)]), ink=ink, steps=4)
    # (his neck, up from his shoulders to his head; the mane along its crest)
    d.fill(coat, Ps([(6.0, 30.4), (11.0, 39.0), (14.0, 44.6), (19.4, 44.0), (16.8, 36.0), (14.4, 27.0)]), ink=ink, steps=3)
    for i in range(6):
        t = i / 5.0
        bx, by = lerp((7.0, 31.4), (13.4, 45.0), t)
        d.fill(mane, Ps([(bx - 1.4, by - 0.6), (bx - 2.8, by + 1.6), (bx - 0.6, by + 1.0), (bx + 0.6, by + 1.2)]), ink=ink * 0.6, steps=1)
    # (his head, thrown up: long, a flat forehead, a soft muzzle; ears up; a white blaze; his eye shut, blowing)
    hx, hy = 15.6, 45.0
    G0 = _T(*P(hx, hy), nod)
    G = lambda u, v: G0(u * k, v * k)                    # noqa: E731
    for (eu, lean) in ((-1.4, -10.0), (0.6, 8.0)):
        d.fill(coat, [G(eu - 0.9, 2.2), G(eu - 0.6 + lean * 0.05, 7.4), G(eu + 1.1, 2.6)], ink=ink, steps=2)
    d.fill(coat, [G(-3.0, 1.6), G(-1.6, 3.4), G(2.4, 3.0), G(8.0, 2.0), G(10.4, 1.0), G(10.6, -1.4), G(8.4, -2.6), G(2.6, -3.2), G(-2.4, -2.0)], ink=ink, steps=4)
    d.fill(sock, [G(1.0, 2.8), G(5.0, 2.4), G(8.4, 1.6), G(8.2, 1.0), G(5.0, 1.6), G(1.4, 2.0)], ink=0.0, steps=2)
    d.line("art_ink", [G(0.2, 0.8), G(1.4, 1.3), G(2.6, 0.9)], 0.35 * k, taper=False)
    d.oval("art_ink", *G(9.6, 0.6), 0.5 * k, 0.35 * k, ink=0.0)
    d.oval(coat, *G(8.4, -1.6), 1.8 * k, 1.3 * k, ink=ink * 0.6)                  # (his cheek, puffed out)
    # (his near legs: a white sock on each)
    for top, knee, foot in (((11.0, 22.4), (12.6, 12.0), (12.8, 1.6)), ((-6.0, 23.0), (-8.6, 12.6), (-6.4, 1.6))):
        d.limb(coat, Ps([top, knee, foot]), [2.6 * k, 1.5 * k, 1.25 * k], ink=ink)
        d.limb(sock, Ps([lerp(knee, foot, 0.55), foot]), [1.3 * k, 1.25 * k], ink=ink * 0.6)
        d.oval(hoof, *P(foot[0] + 0.4, foot[1] - 0.6), 1.7 * k, 1.0 * k, ink=ink * 0.7)
    return G, 10.6 * k, G0


@painting(95)
def p95(d, a):   # a colt blowing a bugle
    x, y = 30.0, 2.0
    # (the bluegrass he stands in)
    for gx in (12.0, 22.0, 34.0, 46.0):
        for (dx, dh, lean) in ((-2.0, 4.0, -1.6), (-0.6, 6.0, -0.4), (0.8, 5.0, 0.8), (2.0, 3.6, 1.8)):
            d.line("art_lblue", [(gx + dx, y), (gx + dx + lean, y + dh)], 0.9)
    G, tip, G0 = colt(d, x, y, 54.0)
    bell = bugle(d, G0, tip + 0.2, w=1.4, k=1.35)
    note(d, bell[0] + 3.0, bell[1] + 2.0, 7.0)
    note(d, bell[0] + 8.0, bell[1] + 9.0, 5.6, turn=-8)
    place_name(d, a, lines=["Bluegrass", "Bugler"], where="right", room=6.0, at=36.0, caps=21.5)


# ---- 96: Firefly Flossie ---------------------------------------------------------------------------------------------------------
def firefly(d, x, y, size, tilt=-8.0, shell="art_brown", collar="art_red", head="art_dark", lamp="art_yellow", glow="art_cream"):
    """A firefly flying right, her body tilted `tilt`: a round dark head with big eyes and lashes, a red collar, wing cases
    spread up behind, lacy wings, six thin legs, and her tail's lamp lit, its glow round it. (x, y) the middle of her
    body, `size` from her lamp to her nose. Returns the lamp's middle."""
    G0 = _T(x, y, tilt)
    k = size / 40.0
    G = lambda u, v: G0(u * k, v * k)                    # noqa: E731
    ink = d.ink
    lamp_at = G(-15.0, -1.0)
    for rr, paint in ((11.0, glow), (7.4, "art_white")):
        d.oval(paint, *lamp_at, rr * k, rr * k * 0.9, ink=0.0)
    # (the lacy hind wings, then the wing cases, spread up behind her)
    for (ang, L, paint) in ((112.0, 17.0, "art_white"), (138.0, 15.0, "art_lgrey")):
        a_ = math.radians(ang + tilt)
        rx, ry = G(-1.0, 3.0)
        tx, ty = rx + L * k * math.cos(a_), ry + L * k * math.sin(a_)
        nx, ny = -math.sin(a_) * 4.2 * k, math.cos(a_) * 4.2 * k
        d.fill(paint, [(rx, ry), (lerp((rx, ry), (tx, ty), 0.5)[0] + nx, lerp((rx, ry), (tx, ty), 0.5)[1] + ny), (tx, ty),
                       (lerp((rx, ry), (tx, ty), 0.6)[0] - nx * 0.5, lerp((rx, ry), (tx, ty), 0.6)[1] - ny * 0.5)], ink=ink * 0.7, steps=4)
        d.line("art_lgrey", [(rx, ry), lerp((rx, ry), (tx, ty), 0.85)], 0.3 * k)
    # (her legs, three a side, under her)
    for i, u in enumerate((3.0, 0.0, -3.0)):
        d.line(head, [G(u, -2.6), G(u + 1.4 - i * 0.4, -6.4), G(u + 3.0 - i * 0.8, -8.0)], 0.5 * k)
    # (her abdomen, ringed, its last two rings the lamp)
    d.fill(shell, [G(-2.0, 3.4), G(-8.0, 3.2), G(-13.0, 2.2), G(-15.6, 0.0), G(-14.4, -3.2), G(-8.0, -3.6), G(-2.0, -3.0)], ink=ink, steps=4)
    for u in (-5.0, -8.0):
        d.line("art_ink", [G(u, 3.2), G(u + 0.6, -3.4)], 0.3 * k)
    d.fill(lamp, [G(-10.4, 2.8), G(-13.2, 2.2), G(-15.8, 0.0), G(-14.6, -3.2), G(-10.8, -3.6)], ink=ink * 0.7, steps=4)
    # (her wing cases, closed over her back's front)
    d.fill(shell, [G(-1.0, 3.6), G(-6.0, 6.8), G(-11.0, 6.0), G(-6.0, 3.2)], ink=ink, steps=3)
    # (her collar, her head: big eyes looking ahead, long lashes, a smile; antennae)
    d.fill(collar, [G(-2.6, 3.8), G(2.6, 4.6), G(4.8, 0.0), G(2.6, -4.0), G(-2.6, -3.4)], ink=ink, steps=4)
    hx, hy = G(8.0, 0.6)
    d.oval(head, hx, hy, 5.4 * k, 5.0 * k, ink=ink)
    for (eu, ev) in ((1.6, 1.2), (4.0, 0.8)):
        ex, ey = G(8.0 + eu, 0.6 + ev)
        d.oval("art_white", ex, ey, 1.5 * k, 1.9 * k, ink=ink * 0.6)
        d.oval("art_black", ex + 0.5 * k, ey, 0.75 * k, 1.0 * k, ink=0.0)
        d.line("art_white", [G(8.0 + eu - 0.6, 0.6 + ev + 1.8), G(8.0 + eu - 0.4, 0.6 + ev + 3.0)], 0.35 * k)
        d.line("art_white", [G(8.0 + eu + 0.4, 0.6 + ev + 1.9), G(8.0 + eu + 0.9, 0.6 + ev + 3.0)], 0.35 * k)
    d.line("art_pink", [G(9.0, -2.4), G(10.6, -3.2), G(12.2, -2.4)], 0.5 * k)
    for (u0, du) in ((6.6, -1.0), (8.6, 1.4)):
        d.line(head, [G(u0, 5.0), G(u0 + du, 9.0), G(u0 + du * 2.4, 10.6)], 0.45 * k)
        d.oval(head, *G(u0 + du * 2.4, 10.6), 0.8 * k, 0.8 * k, ink=0.0)
    return lamp_at


@painting(96)
def p96(d, a):   # a firefly lighting the way through the clouds with her tail
    cloud(d, 4.0, 10.0, 34.0, 13.0, seed=96)
    cloud(d, 66.0, 56.0, 28.0, 10.0, seed=196)
    lamp = firefly(d, 36.0, 30.0, 52.0, tilt=10.0)
    for (ang, L) in ((-150, 12.0), (-178, 15.0), (154, 12.0), (-122, 9.0)):          # (her light, out across the clouds)
        a_ = math.radians(ang)
        d.line("art_cream", [(lamp[0] + 9.0 * math.cos(a_), lamp[1] + 8.0 * math.sin(a_)),
                             (lamp[0] + (9.0 + L) * math.cos(a_), lamp[1] + (8.0 + L) * math.sin(a_))], 1.0)
    place_name(d, a, lines=["Firefly", "Flossie"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 97: Vapor Trail Connie --------------------------------------------------------------------------------------------------------
@painting(97)
def p97(d, a):   # a woman in flying gear writing one long loop across the sky with her finger
    x, y, h = 22.0, 2.0, 62.0
    F = lady(d, x, y, h, 1, pose="stride", arms=("up_out", "hip"),
             wear={"top": "art_khaki", "jacket": "art_brown", "lapel": "art_brown", "collar": "art_cream", "trousers": "art_khaki",
                   "tall": "art_brown", "sleeves": "long", "belt": "art_brown", "buckle": "art_gold"},
             hair="bob", hair_paint="art_auburn")
    P, Ps, R = F.P, F.Ps, F.r
    helmet(d, *P(-0.02, 6.98), R(1.0), 1, paint="art_brown", view="front", inside=None)
    scarf(d, F, "art_white", length=2.4)
    # (her line: up from her fingertip, round one great loop over her, and away behind her, widening as it goes)
    (hx, hy), _ = F.hand["fore"]
    k = F.k
    trail = [(hx + 0.2, hy + 0.8), (hx + 0.8 * k, hy + 2.2 * k), (hx + 0.6 * k, hy + 3.8 * k), (hx - 0.6 * k, hy + 4.8 * k),
             (hx - 2.2 * k, hy + 4.6 * k), (hx - 3.0 * k, hy + 3.4 * k), (hx - 2.4 * k, hy + 2.2 * k), (hx - 1.0 * k, hy + 2.0 * k),
             (hx + 0.4 * k, hy + 3.0 * k), (hx + 2.4 * k, hy + 3.8 * k), (hx + 4.6 * k, hy + 3.6 * k), (hx + 6.6 * k, hy + 3.0 * k)]
    d.limb("art_white", trail, [0.3, 0.6, 0.9, 1.1, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0], ink=d.ink * 0.5)
    d.oval("art_skin", hx, hy, R(0.15), R(0.13), ink=d.ink * 0.7)
    place_name(d, a, lines=["Vapor", "Trail", "Connie"], where="right", room=6.0, at=40.0, gap=1.32, caps=21.5)


# ---- 98: Motor City Molly --------------------------------------------------------------------------------------------------------
def tire(d, x, y, r, rubber="art_black", wall="art_dark", hub="art_lsteel"):
    """A tire standing on its tread, side-on: its middle at (x, y), `r` its outer radius: the tread's blocks round its
    rim, the sidewall, the steel wheel and its hub."""
    d.oval(rubber, x, y, r, r, ink=d.ink)
    for i in range(24):
        a_ = math.pi * 2 * i / 24
        d.line(wall, [(x + r * 0.9 * math.cos(a_), y + r * 0.9 * math.sin(a_)), (x + r * 0.99 * math.cos(a_), y + r * 0.99 * math.sin(a_))],
               r * 0.06, taper=False)
    d.oval(wall, x, y, r * 0.78, r * 0.78, ink=d.ink * 0.5)
    d.line("art_grey", _arc(x, y, r * 0.7, r * 0.7, 100, 170, 8), r * 0.04)
    d.oval(hub, x, y, r * 0.5, r * 0.5, ink=d.ink * 0.7)
    d.oval("art_grey", x, y, r * 0.2, r * 0.2, ink=d.ink * 0.6)
    for i in range(5):
        a_ = math.pi * 2 * i / 5 + 0.3
        d.oval("art_dark", x + r * 0.33 * math.cos(a_), y + r * 0.33 * math.sin(a_), r * 0.045, r * 0.045, ink=0.0)


@painting(98)
def p98(d, a):   # a woman in a mechanic's coveralls sitting on a winged tire, a wrench in her hand
    tx, ty, tr = 30.0, 16.0, 15.0
    wing(d, (tx - tr * 0.7, ty + tr * 0.4), 150.0, 24.0, 9.5, "art_lgrey", shade="art_grey", spread=-26)
    tire(d, tx, ty, tr)
    wing(d, (tx + tr * 0.6, ty + tr * 0.3), 34.0, 26.0, 10.0, "art_white", spread=28)
    F = lady(d, tx - 4.0, ty + tr - 25.0, 54.0, 1, pose="sit", arms=("offer", "hip"),
             wear={"top": "art_blue", "trousers": "art_blue", "bib": "art_blue", "sleeves": "rolled", "shoes": "art_brown", "stitch": "art_white",
                   "hat": "art_red", "belt": None},
             hair="rolls", hair_paint="art_hair", hat="kerchief")
    (hx, hy), _ = F.hand["fore"]
    wrench(d, hx - 3.0, hy - 1.0, hx + 9.0, hy + 2.6, 2.2)
    d.oval("art_skin", hx, hy, F.r(0.16), F.r(0.14), ink=d.ink * 0.7)
    place_name(d, a, lines=["Motor City", "Molly"], where="right", room=6.0, at=34.0, caps=21.5)


# ---- 99: Blackout Betty ---------------------------------------------------------------------------------------------------------
@painting(99)
def p99(d, a):   # a woman in a long housecoat and curlers pulling down the blackout shade
    kw = dict(pose="together", arms=("offer", "hip"),
              wear={"top": "art_pink", "coat": "art_pink", "hem": "long", "collar": "art_white", "belt": "art_pink", "sleeves": "long",
                    "shoes": "art_lblue"},
              hair="rolls", hair_paint="art_auburn")
    x, y, h = 20.0, 2.0, 58.0
    (hx, hy), _ = lady(Drawing(1), x, y, h, 1, **kw).hand["fore"]          # (where her hand will be)
    wx0, wx1, wy0, wy1 = hx - 3.0, hx + 27.0, 14.0, 66.0
    # (the window: its night, one star; the shade pulled down to her hand; the frame and the sill)
    d.shape("art_navy", [(wx0, wy0), (wx0, wy1), (wx1, wy1), (wx1, wy0)])
    star(d, wx0 + 19.0, wy0 + 12.0, 2.8, "art_white")
    down = hy + 5.0
    d.shape("art_black", [(wx0 - 0.6, down), (wx0 - 0.6, wy1), (wx1 + 0.6, wy1), (wx1 + 0.6, down)])
    d.line("art_tan", [(wx0 - 0.6, down), (wx1 + 0.6, down)], 1.2, taper=False)
    for (u0, v0, u1, v1) in ((wx0, wy0, wx0, wy1), (wx1, wy0, wx1, wy1), (wx0, wy1, wx1, wy1)):
        d.line("art_lbrown", [(u0, v0), (u1, v1)], 2.2, taper=False)
    d.line("art_lbrown", [((wx0 + wx1) / 2, wy0), ((wx0 + wx1) / 2, down)], 1.2, taper=False)
    d.line("art_lbrown", [(wx0, (wy0 + down) / 2), (wx1, (wy0 + down) / 2)], 1.2, taper=False)
    d.shape("art_lbrown", [(wx0 - 3.0, wy0 - 2.4), (wx0 - 3.0, wy0 + 0.4), (wx1 + 3.0, wy0 + 0.4), (wx1 + 3.0, wy0 - 2.4)])
    d.line("art_tan", [(hx, down), (hx, hy + 2.2)], 0.45, taper=False)                 # (its cord, down to the ring in her hand)
    # (her: in her housecoat, her hair rolled up in curlers)
    F = lady(d, x, y, h, 1, **kw)
    P, R = F.P, F.r
    for (u, v) in ((-0.5, 7.28), (-0.2, 7.46), (0.12, 7.48), (0.42, 7.3), (-0.66, 7.0), (-0.34, 7.16), (0.0, 7.24)):
        d.oval("art_lsteel", *P(u, v), R(0.13), R(0.075), turn=0.0, ink=d.ink * 0.6)
    d.line("art_ink", ellipse(hx, hy + 1.1, 1.2, 1.2, 0, 16), 0.45, taper=False)
    d.oval("art_skin", hx, hy, R(0.15), R(0.13), ink=d.ink * 0.7)
    place_name(d, a, lines=["Blackout", "Betty"], where="right", room=6.0, at=36.0, caps=21.5)


# ---- 100: Chow Line Champ ----------------------------------------------------------------------------------------------------------
def heaped_tray(d, x, y, w, tray="art_lsteel"):
    """A mess tray seen edge-on, its middle at (x, y), `w` wide, piled a foot high: potatoes, a slab of meat, beans,
    a stack of flapjacks, bread, a drumstick, and a pat of butter on top. Returns the pile's top."""
    hw = w / 2.0
    d.shape(tray, [(x - hw, y - 0.8), (x - hw - 0.6, y + 0.4), (x + hw + 0.6, y + 0.4), (x + hw, y - 0.8)])
    layers = [("art_cream", 0.96, 3.2), ("art_brown", 0.9, 2.6), ("art_tan", 0.8, 2.4), ("art_tan", 0.76, 2.2), ("art_tan", 0.72, 2.2),
              ("art_green", 0.66, 2.0), ("art_white", 0.56, 2.6), ("art_lbrown", 0.46, 2.4)]
    v = y + 0.4
    for i, (paint, frac, th) in enumerate(layers):
        lw = hw * frac
        sx = (i % 3 - 1) * 0.8
        d.fill(paint, [(x - lw + sx, v), (x - lw + sx - 0.4, v + th * 0.6), (x - lw * 0.6 + sx, v + th), (x + lw * 0.6 + sx, v + th),
                       (x + lw + sx + 0.4, v + th * 0.6), (x + lw + sx, v)], ink=d.ink * 0.8, steps=3)
        v += th - 0.3
    d.limb("art_lbrown", [(x + hw * 0.2, v - 1.0), (x + hw * 0.7, v + 3.6)], [1.8, 1.2], ink=d.ink * 0.8)       # (the drumstick)
    d.oval("art_white", x + hw * 0.78, v + 4.2, 1.1, 0.9, ink=d.ink * 0.7)
    d.shape("art_yellow", [(x - 1.8, v - 0.4), (x - 1.8, v + 1.4), (x + 1.4, v + 1.4), (x + 1.4, v - 0.4)])
    for (sx, sy) in ((-3.4, 2.6), (-1.0, 3.6), (1.6, 2.8)):                       # (the steam off it)
        d.line("art_white", [(x + sx, v + sy), (x + sx - 0.8, v + sy + 2.0), (x + sx + 0.4, v + sy + 3.6)], 0.5)
    return (x, v)


@painting(100)
def p100(d, a):   # a soldier with his mess tray piled a foot high, grinning
    x, y, h = 30.0, 2.0, 56.0
    F = man(d, x, y, h, 1, pose="easy", arms=(to(1.5, 5.12, "down"), to(1.3, 4.98, "down")), hat="garrison",
            wear={"shirt": "art_khaki", "sleeves": "rolled", "trousers": "art_khaki", "tie": "art_brown", "neck": "tie", "belt": "art_olive",
                  "buckle": "art_gold", "shoes": "art_brown", "hat": "art_khaki"},
            hair="short", mouth="grin")
    (fx, fy), _ = F.hand["fore"]
    (ax, ay), _ = F.hand["aft"]
    heaped_tray(d, (fx + ax) / 2 + 2.0, max(fy, ay) + 0.6, 20.0)
    F.grip("fore")
    F.grip("aft")
    place_name(d, a, lines=["Chow", "Line", "Champ"], where="right", room=6.0, at=36.0, gap=1.32, caps=21.5)


# ---- 101: Long Way Home ---------------------------------------------------------------------------------------------------------
@painting(101)
def p101(d, a):   # one signpost with one long arrow, its tip bent toward home
    # (the knoll it stands on)
    d.fill("art_green", [(-4.0, 0.0), (1.0, 5.0), (10.0, 6.6), (17.0, 4.4), (21.0, 0.0)], steps=4)
    # (the post, with its grain)
    px = 10.0
    d.shape("art_brown", [(px - 1.8, 4.0), (px - 1.8, 52.0), (px - 0.6, 53.4), (px + 0.6, 53.4), (px + 1.8, 52.0), (px + 1.8, 4.0)])
    d.line("art_ink", [(px - 0.4, 10.0), (px - 0.6, 30.0)], 0.3)
    d.line("art_ink", [(px + 0.6, 34.0), (px + 0.5, 48.0)], 0.3)
    # (the one arrow: a long board out from the post, bent down near its end toward home, its head there; two nails)
    y0, th = 45.0, 3.2
    run = 74.0
    bend = (px + run, y0)
    end = (px + run + 9.0, y0 - 10.0)
    d.limb("art_cream", [(px - 3.0, y0), lerp((px - 3.0, y0), bend, 0.5), bend, end], [th, th, th, th], ink=d.ink)
    ux, uy = end[0] - bend[0], end[1] - bend[1]
    ln = math.hypot(ux, uy)
    ux, uy = ux / ln, uy / ln
    nx, ny = -uy, ux
    d.shape("art_cream", [(end[0] + nx * th * 2.2, end[1] + ny * th * 2.2), (end[0] + ux * th * 3.4, end[1] + uy * th * 3.4),
                          (end[0] - nx * th * 2.2, end[1] - ny * th * 2.2)])
    d.line("art_tan", [(px + 2.0, y0), (px + run - 4.0, y0)], 0.4, taper=False)
    for nx_ in (px - 0.6, px + 0.6):
        d.oval("art_dark", nx_, y0 + (nx_ - px) * 1.6, 0.45, 0.45, ink=0.0)
    place_name(d, a, lines=["Long", "Way", "Home"], where="right", edge=px + 6.0, room=5.0, at=18.0, gap=1.3, caps=21.5)
