"""Batch 2, numbers 53 to 59 (painter b2_e): a chuck-wagon cook at his dinner triangle, a gunner snug in a snow globe,
a lady of the 1890s at a riverboat's rail, a porcupine bristling with gun barrels, a cowgirl roping a cloud, a flyer
with the wind at her back and a porch swing on a summer evening. Each picture is drawn from flat shapes with noseart's
Drawing API and the shared figure kit; the helpers below (a bent rod, a dinner triangle, a snow globe, a paddle box, a
bulwark, a porcupine, gun barrels, chaps, a guitar, a pitcher, a porch swing...) are built the same way, for these paintings."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402
import random  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402


# ---- helpers ------------------------------------------------------------------------------------------------------------
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


def _arm_of(name, side, extra=None):
    """Her shoulder, elbow and hand (heads, her frame) for one of lady()'s arms."""
    el, hd = (extra or {}).get(name) or ARMS[name]
    sx = 1.0 if side == "fore" else -1.0
    sh = (0.88 * sx, 5.92)
    return sh, (sh[0] + el[0] * sx, sh[1] + el[1]), (sh[0] + hd[0] * sx, sh[1] + hd[1])


def _lean(d, n0, cx, cy, deg):
    """Every shape painted since op n0 turned `deg` degrees about (cx, cy): a whole figure leaned at once."""
    for i in range(n0, len(d.ops)):
        op = d.ops[i]
        if op[0] == "polys":
            d.ops[i] = ("polys", op[1], [turned(p, cx, cy, deg) for p in op[2]])


def _no_blush(d, n0=0):
    """Her cheeks' blush taken off again (shapes painted since op n0): a cruder hand, keeping only three colours of its
    own, would paint them in the nearest of those (a yellow, a tan)."""
    d.ops[n0:] = [op for op in d.ops[n0:] if not (op[0] == "polys" and op[1] == "art_pink")]


def _sway(d):
    """A cruder hand's sway, as noseart's cruder() will give it to every shape of this painting (seeded alike): broad
    waves, and over them a small shake."""
    r = random.Random(d.seed * 3 + 1)
    waves = [(r.uniform(0.05, 0.11), r.uniform(0.05, 0.11), r.uniform(0.0, 6.28)) for _ in range(4)]
    shake = [(r.uniform(0.6, 1.2), r.uniform(0.0, 6.28)) for _ in range(4)]
    amp, amp2 = 0.55, 0.2

    def sway(u, v):
        du = amp * (math.sin(waves[0][0] * v + waves[0][2]) + 0.6 * math.sin(waves[1][1] * (u + v) + waves[1][2])) + \
            amp2 * (math.sin(shake[0][0] * v + shake[0][1]) + 0.5 * math.sin(shake[1][0] * (u + v) + shake[1][1]))
        dv = amp * (math.sin(waves[2][0] * u + waves[2][2]) + 0.6 * math.sin(waves[3][1] * (u - v) + waves[3][2])) + \
            amp2 * (math.sin(shake[2][0] * u + shake[2][1]) + 0.5 * math.sin(shake[3][0] * (u - v) + shake[3][1]))
        return u + du, v + dv
    return sway


def _steady(d, n0, c, r):
    """Under a cruder hand, a face's small features (every shape painted since op n0 lying wholly within r of c: its
    eyes, brows, nose, mouth) kept true to one another, for the hand's small shake would tilt a brow into a scowl. Each
    point is set where the sway will carry it home; the features still move as the face's middle moves."""
    if not getattr(d, "crude", False):
        return
    sway = _sway(d)
    su, sv = sway(*c)
    ou, ov = su - c[0], sv - c[1]

    def dense(poly, step=0.5):
        out = []
        for i, (u0, v0) in enumerate(poly):
            u1, v1 = poly[(i + 1) % len(poly)]
            k = max(1, int(math.hypot(u1 - u0, v1 - v0) / step))
            out += [(u0 + (u1 - u0) * j / k, v0 + (v1 - v0) * j / k) for j in range(k)]
        return out

    def home(u, v):
        tu, tv = u + ou, v + ov
        gu, gv = tu, tv
        for _ in range(24):
            wu, wv = sway(gu, gv)
            gu, gv = gu + (tu - wu), gv + (tv - wv)
        return gu, gv

    for i in range(n0, len(d.ops)):
        op = d.ops[i]
        if op[0] != "polys" or not op[2] or any(math.hypot(u - c[0], v - c[1]) > r for p in op[2] for u, v in p):
            continue
        d.ops[i] = ("polys", op[1], [[home(u, v) for u, v in dense(p)] for p in op[2]])


def _extent(d):
    """The painting so far: (x0, y0, x1, y1) of all its shapes."""
    xs = [p[0] for op in d.ops if op[0] == "polys" for poly in op[2] for p in poly]
    ys = [p[1] for op in d.ops if op[0] == "polys" for poly in op[2] for p in poly]
    return min(xs), min(ys), max(xs), max(ys)


def _caps(d):
    """How tall the name's capitals will stand on her skin (cm), the painting fitted into its decal as raster() fits it."""
    xs, ys, sizes = [], [], []
    for op in d.ops:
        if op[0] == "polys":
            for poly in op[2]:
                xs += [p[0] for p in poly]
                ys += [p[1] for p in poly]
        else:
            x0, y0, x1, y1 = text_bounds(op[1])
            xs += [x0, x1]
            ys += [y0, y1]
            sizes.append(op[1]["size"])
    k = min(PX[0] * 0.93 / (max(xs) - min(xs)), PX[1] * 0.93 / (max(ys) - min(ys)))
    return min(sizes) * k * 100.0 / PX_PER_M


def place_name(d, a, lines=None, where="right", room=6.0, at=None, edge=None, gap=1.4, indent=0.0, turn=0.0, caps=20.9, size=14.0,
               retry=0):
    """The name set by the picture, never over it: where="right", its lines' left edge `room` clear of the picture's
    right side (or of x = edge) and their middle at height `at`; where="below", its top `room` under the picture's foot
    (or under y = edge) and its middle at x = `at` (the picture's middle when `at` is None). Its size is found so that
    its capitals stand `caps` cm tall on her skin (the art director asks 20 to 30). `retry` lets the sign painter letter
    it again that many times over (a letter of his first try that would not read: an e closed up into a c)."""
    x0, y0, x1, y1 = _extent(d)
    n0 = len(d.ops)
    for _ in range(retry):
        d.rng.random()
    state = d.rng.getstate()
    for _ in range(16):
        del d.ops[n0:]
        d.rng.setstate(state)
        names(d, a, (0.0, 0.0), size, lines=lines or split2(a["name"]), gap=gap, indent=indent * size / 14.0, turn=turn)
        bs = [text_bounds(op[1]) for op in d.ops[n0:]]
        bx0, by0, bx1, by1 = min(b[0] for b in bs), min(b[1] for b in bs), max(b[2] for b in bs), max(b[3] for b in bs)
        if where == "right":
            dx = (x1 if edge is None else edge) + room - bx0
            dy = ((y0 + y1) / 2 if at is None else at) - (by0 + by1) / 2
        else:
            dx = ((x0 + x1) / 2 if at is None else at) - (bx0 + bx1) / 2
            dy = (y0 if edge is None else edge) - room - by1
        for op in d.ops[n0:]:
            op[1]["at"] = (op[1]["at"][0] + dx, op[1]["at"][1] + dy)
        c = _caps(d)
        if abs(c - caps) < 0.1 or size > 40.0:
            break
        size *= max(0.8, min(1.25, caps / c))
    return size


def _rod(pts, r):
    """The outline of a bent rod of even width 2r along the polyline `pts`: its bends mitred, its two ends round."""
    n = len(pts)

    def unit(ax, ay):
        ln = math.hypot(ax, ay) or 1.0
        return ax / ln, ay / ln

    left, right = [], []
    for i in range(n):
        px, py = pts[i]
        if i in (0, n - 1):
            j0, j1 = (0, 1) if i == 0 else (n - 2, n - 1)
            tx, ty = unit(pts[j1][0] - pts[j0][0], pts[j1][1] - pts[j0][1])
            mx, my, ml = -ty, tx, r
        else:
            ax, ay = unit(px - pts[i - 1][0], py - pts[i - 1][1])
            bx, by = unit(pts[i + 1][0] - px, pts[i + 1][1] - py)
            mx, my = unit(-ay - by, ax + bx)
            ml = r / max(mx * -ay + my * ax, 0.3)
        left.append((px + mx * ml, py + my * ml))
        right.append((px - mx * ml, py - my * ml))

    def cap(c, a_from):
        return [(c[0] + r * math.cos(a_from - math.pi * k / 8), c[1] + r * math.sin(a_from - math.pi * k / 8)) for k in range(1, 8)]
    tx1, ty1 = unit(pts[-1][0] - pts[-2][0], pts[-1][1] - pts[-2][1])
    tx0, ty0 = unit(pts[1][0] - pts[0][0], pts[1][1] - pts[0][1])
    return left + cap(pts[-1], math.atan2(tx1, -ty1)) + list(reversed(right)) + cap(pts[0], math.atan2(-tx0, ty0))


def _arc(cx, cy, rx, ry, a0, a1, n=14, turn=0.0):
    """Points along an ellipse from a0 to a1 degrees (turned `turn` degrees about its middle)."""
    pts = [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * k / n)))
           for k in range(n + 1)]
    return turned(pts, cx, cy, turn) if turn else pts


# ---- 53: the chuck-wagon cook -------------------------------------------------------------------------------------------
def triangle(d, top, side, paint="art_white", r=0.8, swing=0.0):
    """A dinner triangle hung by its top corner at `top`: one rod bent twice, its two ends not quite meeting at its
    lower left corner; `side` long, `r` its rod's half width, swung `swing` degrees about its top (just struck: its
    foot swings away). Returns its corners (top, left, right)."""
    tx, ty = top
    h = side * 0.866
    T, L, R = turned([(tx, ty), (tx - side / 2, ty - h), (tx + side / 2, ty - h)], tx, ty, swing)
    d.shape(paint, _rod([lerp(L, T, 0.09), T, R, lerp(R, L, 0.91)], r))
    return T, L, R


@painting(53)
def p53(d, a):   # a chuck-wagon cook ringing a dinner triangle
    n0 = len(d.ops)
    F = man(d, 20, 2, 74, 1, pose="wide", arms=(to(1.65, 7.8, elbow=(0.0, 1.24)), to(0.75, 5.95, "down")),
            wear={"shirt": "art_red", "sleeves": "long", "trousers": "art_tan", "apron": "art_white", "apron_cut": "bib",
                  "boots": "art_black", "hat": "art_tan", "band": "art_black", "belt": None},
            hat="cowboy", mouth="open", moustache="full", hair_paint="art_hair", wide=1.1, cheeks=False)
    _steady(d, n0, F.face, F.rh(0.52))                                                          # (calling them in, not scowling)
    (hx, hy), _ = F.hand["fore"]
    (ax, ay), _ = F.hand["aft"]
    sw, thong, side = 20.0, 7.0, 17.0                          # (just struck: swinging out on its thong, away from his arm)
    top = (hx + thong * math.sin(math.radians(sw)), hy - thong * math.cos(math.radians(sw)))
    d.line("art_black", [(hx, hy), top], 0.9, taper=False)                                     # (its thong)
    T, L, R = triangle(d, top, side, "art_white", r=1.2, swing=sw)
    tip = lerp(lerp(T, L, 0.5), R, 0.45)                                                        # (the iron he beats it with,
    ux, uy = tip[0] - ax, tip[1] - ay                                                           # in its open middle)
    ln = math.hypot(ux, uy)
    d.shape("art_black", _rod([(ax - ux / ln * 2.4, ay - uy / ln * 2.4), tip], 0.62))
    F.grip("aft")
    F.grip("fore")
    place_name(d, a, lines=["Biscuits", "and", "Gravy"], room=5.0)


# ---- 54: the ball turret buddy --------------------------------------------------------------------------------------------
def globe_base(d, cx, y0, w, h, wood="art_brown", band="art_tan"):
    """A snow globe's turned wooden base, its foot at y0: a collar under the glass, a waist, a wide foot, a band."""
    d.fill(wood, [(cx - w * 0.5, y0), (cx - w * 0.5, y0 + h * 0.2), (cx - w * 0.38, y0 + h * 0.46), (cx - w * 0.4, y0 + h * 0.8),
                  (cx - w * 0.34, y0 + h), (cx + w * 0.34, y0 + h), (cx + w * 0.4, y0 + h * 0.8), (cx + w * 0.38, y0 + h * 0.46),
                  (cx + w * 0.5, y0 + h * 0.2), (cx + w * 0.5, y0)], steps=3)
    d.shape(band, [(cx - w * 0.39, y0 + h * 0.5), (cx + w * 0.39, y0 + h * 0.5), (cx + w * 0.4, y0 + h * 0.72), (cx - w * 0.4, y0 + h * 0.72)],
            ink=d.ink * 0.7)


@painting(54)
def p54(d, a):   # a small man curled up snug in a glass ball like a snow globe
    cx, cy, r = 28.0, 46.0, 27.0
    d.oval("art_sky", cx, cy, r, r)
    floor = cy - r * 0.52                                                                       # (the drift's top, where he sits)
    drift = _arc(cx, cy, r, r, 200, 340, 18) + [(cx + r * 0.86, floor + 1.0), (cx + r * 0.45, floor - 0.6), (cx, floor + 0.4),
                                               (cx - r * 0.45, floor - 0.4), (cx - r * 0.86, floor + 1.4)]
    d.fill("art_white", drift, ink=d.ink * 0.8, steps=3)
    k = 50 / 7.4
    man(d, cx - 1.0, floor - 0.62 * k, 50, 1, pose="curl", arms=("hug", "hug"),
        wear={"shirt": "art_white", "jacket": "art_lbrown", "cut": "flight", "fur": "art_cream", "trousers": "art_dark", "boots": "art_black"},
        hat="flying", eyes="shut")
    d.fill("art_white", [(cx - 12.0, floor - 3.0), (cx - 11.0, floor + 0.6), (cx - 6.0, floor + 1.4), (cx + 2.0, floor + 0.4), (cx + 9.0, floor + 1.2),
                         (cx + 13.0, floor - 0.2), (cx + 13.0, floor - 3.0)], ink=d.ink * 0.8, steps=3)  # (the snow before him)
    for (fx, fy, fr) in ((cx - 16.0, cy + 9.0, 1.3), (cx + 15.0, cy + 13.0, 1.2), (cx - 7.0, cy + 19.0, 1.1), (cx + 18.0, cy - 1.0, 1.0)):
        d.oval("art_white", fx, fy, fr, fr, ink=d.ink * 0.5)
    d.line("art_white", _arc(cx, cy, r * 0.8, r * 0.8, 112, 158, 10), 1.8)
    globe_base(d, cx, cy - r - 8.0, 40.0, 12.0, wood="art_lbrown", band="art_brown")
    place_name(d, a, lines=["Ball", "Turret", "Buddy"], room=6.0, retry=3)       # (his first tries closed the e of Turret)


# ---- 55: Riverboat Ruby -----------------------------------------------------------------------------------------------------
def paddle_box(d, x, y, r, paint="art_yellow", rays="art_red", n=5):
    """A paddle steamer's wheel housing, side on: a solid half round standing on her deck at (x, y), r high, with a
    few painted rays fanning from a hub at its foot and a band round its rim, as the boats wore them."""
    d.shape(paint, _arc(x, y, r, r, 0.0, 180.0, 36))
    d.line(rays, _arc(x, y, r * 0.86, r * 0.86, 3.0, 177.0, 36), r * 0.08, taper=False)
    for i in range(n):
        a_ = math.radians(180.0 * (i + 0.5) / n)
        c, s_ = math.cos(a_), math.sin(a_)
        nx, ny = -s_, c
        r0, r1, w0, w1 = r * 0.3, r * 0.7, r * 0.03, r * 0.075
        d.shape(rays, [(x + r0 * c + w0 * nx, y + r0 * s_ + w0 * ny), (x + r1 * c + w1 * nx, y + r1 * s_ + w1 * ny),
                       (x + r1 * c - w1 * nx, y + r1 * s_ - w1 * ny), (x + r0 * c - w0 * nx, y + r0 * s_ - w0 * ny)], ink=d.ink * 0.6)
    d.shape(rays, _arc(x, y, r * 0.2, r * 0.2, 0.0, 180.0, 14), ink=d.ink * 0.8)


def bulwark(d, x0, x1, y0, y1, paint="art_white", lip=1.6, rail=3.4):
    """A steamer's deck rail seen from the river, one plain shape: a solid bulwark from y0 up, its top rail (`rail`
    deep) standing a little proud of it at both ends, its top at y1."""
    pts = [(x0, y0), (x1, y0), (x1, y1 - rail), (x1 + lip, y1 - rail), (x1 + lip, y1), (x0 - lip, y1), (x0 - lip, y1 - rail), (x0, y1 - rail)]
    d.shape(paint, pts)
    d.line("art_ink", [(x0, y1 - rail), (x1, y1 - rail)], d.ink * 0.6, taper=False)


def pompadour(d, F, paint="art_auburn"):
    """Her hair as the 1890s wore it, swept up off her brow and puffed out over her temples, its edge on her forehead
    well above her brows (a cruder hand's hair over her brows would turn her lids into a frown)."""
    outer = [(-0.5, 6.72), (-0.6, 7.05), (-0.55, 7.4), (-0.3, 7.62), (0.1, 7.68), (0.45, 7.52), (0.58, 7.2), (0.54, 6.8)]
    inner = [(0.45, 6.78), (0.44, 7.04), (0.36, 7.27), (0.1, 7.35), (-0.2, 7.33), (-0.38, 7.2), (-0.45, 6.95), (-0.44, 6.72)]
    d.fill(paint, F.Ps(outer + inner), ink=d.ink * 0.7, steps=3)
    for pts in ([(-0.3, 7.3), (-0.42, 7.12), (-0.5, 6.9)], [(0.3, 7.36), (0.46, 7.2), (0.5, 7.0)]):
        d.line("art_ink", F.Ps(pts), F.r(0.03))


def boater(d, F, paint="art_yellow", band="art_red", tilt=-7.0):
    """A straw boater perched on her upswept hair, tipped forward a little."""
    cx, cy = F.P(0.04, 7.5)
    brim = ellipse(cx, cy, F.r(0.8), F.r(0.12), tilt * F.s, 24)
    crown = turned([F.P(-0.42, 7.5), F.P(-0.4, 7.86), F.P(0.44, 7.86), F.P(0.46, 7.5)], cx, cy, tilt * F.s)
    bandp = turned([F.P(-0.42, 7.53), F.P(-0.41, 7.68), F.P(0.45, 7.68), F.P(0.46, 7.53)], cx, cy, tilt * F.s)
    d.fill(paint, brim, steps=2)
    d.shape(paint, crown)
    d.shape(band, bandp, ink=d.ink * 0.6)


def _clip_below(d, n0, y0):
    """Every shape painted since op n0 cut off at y = y0 (the rest of her, out of sight below the bulwark)."""
    def clip(poly):
        out = []
        for i in range(len(poly)):
            (pu, pv), (qu, qv) = poly[i - 1], poly[i]
            if (pv >= y0) != (qv >= y0):
                t = (y0 - pv) / (qv - pv)
                out.append((pu + (qu - pu) * t, y0))
            if qv >= y0:
                out.append((qu, qv))
        return out
    for i in range(n0, len(d.ops)):
        op = d.ops[i]
        if op[0] == "polys":
            d.ops[i] = ("polys", op[1], [c for c in (clip(p) for p in op[2]) if len(c) >= 3])
    d.ops[n0:] = [op for op in d.ops[n0:] if op[0] != "polys" or op[2]]        # (her shoes, wholly out of sight)


def _ruby_name(d, a, ruby_left, room=4.0, caps=20.9, size=18.0):
    """Riverboat Ruby's name, stepped up from the paddle box: RUBY from x = ruby_left, its foot `room` clear of the
    picture under it; RIVERBOAT above, its right end over RUBY's and its foot `room` clear of RUBY and of the picture
    under it; sized, as place_name() sizes a name, so that its capitals stand `caps` cm tall on her skin."""
    lines = split2(a["name"])
    n0 = len(d.ops)
    pic = [p for op in d.ops if op[0] == "polys" for poly in op[2] for p in poly]

    def foot(u0, u1, extra=()):
        return max([v for u, v in pic if u0 - room < u < u1 + room] + list(extra)) + room

    state = d.rng.getstate()
    for _ in range(16):
        del d.ops[n0:]
        d.rng.setstate(state)
        for t in lines:
            d.name(t, a["lettering"], (0.0, 0.0), size)
        top, low = d.ops[n0][1], d.ops[n0 + 1][1]
        x0, y0, x1, y1 = text_bounds(low)
        low["at"] = (ruby_left - x0, foot(ruby_left, ruby_left + x1 - x0) - y0)
        lx0, ly0, lx1, ly1 = text_bounds(low)
        x0, y0, x1, y1 = text_bounds(top)
        top["at"] = (lx1 - x1, foot(lx1 - (x1 - x0), lx1, [ly1]) - y0)
        c = _caps(d)
        if abs(c - caps) < 0.1:
            break
        size *= max(0.8, min(1.25, caps / c))
    return size


def riverboat_ruby(d, a, x=37.0, rail=12.0, head=19.0, box_r=35.0):
    """Ruby at the rail of a paddle steamer, painted from the rail up, waving her handkerchief to the shore: her
    shirtwaist's great leg-of-mutton sleeves, her boater, her pompadour; before her the white bulwark, one plain shape,
    and beside her the steamer's paddle box, a bold half round with its painted rays."""
    k = head
    box_x = x + 1.2 * k + box_r                             # (its foot just behind her hand on the rail)
    paddle_box(d, box_x, rail, box_r)
    extra = {"rail": ((0.8, -0.95), (0.4, -1.67))}          # (her hand on the rail's top)
    arms = ("rail", "raise")
    n0 = len(d.ops)
    F = _lady(d, x, rail - 4.2 * k, HEADS * k, 1, pose="stand", arms=arms, extra_arms=extra,
              wear={"top": "art_white", "skirt": "art_red", "hem": "long", "flare": 0.5, "sleeves": "long",
                    "belt": "art_red", "buckle": "art_red", "tie": "art_red", "shoes": "art_black"},
              hair="none")
    _clip_below(d, n0, 1.0)
    _no_blush(d, n0)
    for side, nm in (("aft", arms[1]), ("fore", arms[0])):     # (the 1890s sleeve: a great puff from shoulder to elbow)
        sh, el, _ = _arm_of(nm, side, extra)
        c = lerp(sh, el, 0.42)
        ang = math.degrees(math.atan2(el[1] - sh[1], (el[0] - sh[0]) * F.s))
        d.oval("art_white", *F.P(*c), F.r(0.64), F.r(0.42), turn=ang)
    collar = F.Ps([(-0.25, 5.98), (-0.21, 6.3), (0.21, 6.3), (0.25, 5.98)])       # (her high collar, of a piece with her blouse)
    d.shape("art_white", collar, ink=0.0)
    d.line("art_ink", collar, F.r(0.035), taper=False)
    pompadour(d, F)
    boater(d, F)
    _steady(d, n0, F.P(0.07, 6.86), F.r(0.47))
    (hx, hy), _ = F.hand["aft"]                                 # (a handkerchief, waved to the shore)
    q = k / 10.0
    d.fill("art_white", [(hx + 0.2 * q, hy + 0.6 * q), (hx - 2.2 * q, hy + 4.4 * q), (hx - 6.4 * q, hy + 6.6 * q), (hx - 8.8 * q, hy + 5.0 * q),
                         (hx - 6.8 * q, hy + 2.2 * q), (hx - 8.6 * q, hy - 1.4 * q), (hx - 4.0 * q, hy - 0.6 * q), (hx - 1.0 * q, hy - 0.8 * q)], steps=3)
    d.oval("art_skin", hx, hy, F.r(0.155), F.r(0.13), ink=d.ink * 0.7)
    bulwark(d, x - 1.3 * k, box_x + box_r + 2.0, 0.0, rail)
    (fx, fy), _ = F.hand["fore"]
    d.oval("art_skin", fx, fy, F.r(0.17), F.r(0.12), ink=d.ink * 0.7)                # (her hand on the rail)
    _ruby_name(d, a, box_x + box_r * 0.55)


@painting(55)
def p55(d, a):   # a woman in an 1890s travelling dress at the rail of a paddle steamer
    riverboat_ruby(d, a)


# ---- 56: Porcupine Express ---------------------------------------------------------------------------------------------------
def gun_barrel(d, root, ang, L, k, jacket="art_lsteel", barrel="art_steel", ring="art_steel", jw=2.0, bw=0.9, holes=3):
    """One quill: a machine gun's barrel in its perforated cooling jacket, from `root` (hidden under the fur) out along
    `ang` degrees, `L` long (the porcupine's units, k grid units each): the fat jacket over its first 0.6, its holes in a
    row, the bare barrel on past it, and at its end a square-cut muzzle with a short ring a little wider than the barrel."""
    ux, uy = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    nx, ny = -uy, ux

    def at(t, off):                                             # (along it t units from its root, off across it)
        return ((root[0] + ux * t + nx * off) * k, (root[1] + uy * t + ny * off) * k)

    J, R0 = L * 0.6, L - 1.5                                    # (the jacket's end; where the muzzle's ring begins)
    d.shape(barrel, [at(J - 1.0, bw), at(L, bw), at(L, -bw), at(J - 1.0, -bw)], ink=d.ink * 0.8)
    d.shape(ring, [at(R0, bw * 1.3), at(L, bw * 1.3), at(L, -bw * 1.3), at(R0, -bw * 1.3)], ink=d.ink * 0.8)
    d.limb(jacket, [at(0.0, 0.0), at(J * 0.5, 0.0), at(J, 0.0)], [jw * k] * 3, ink=d.ink * 0.9)
    for i in range(holes):
        d.oval("art_dark", *at(J * (0.52 + 0.17 * i), 0.0), 0.5 * k, 0.5 * k, ink=0.0)


def porcupine(d, x, y, size, s=1, fur="art_brown", face="art_tan", jacket="art_lsteel", barrel="art_steel", ring="art_steel"):
    """A cartoon porcupine running, side on, its paws' soles at y, its middle at x, `size` long, facing s; its quills
    are six machine-gun barrels in their cooling jackets, rooted all along its back from its rump to just behind its
    ear, every one swept back."""
    k = size / 40.0

    def P(u, v):
        return (x + s * u * k, y + v * k)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    w0 = d.ink
    # (its far legs)
    d.limb(fur, Q([(5.0, 5.0), (8.6, 2.6), (11.0, 1.8)]), [1.9 * k, 1.5 * k, 1.2 * k], ink=w0 * 0.8)
    d.limb(fur, Q([(-9.0, 5.0), (-11.6, 2.6), (-13.0, 1.2)]), [2.0 * k, 1.6 * k, 1.2 * k], ink=w0 * 0.8)
    # (its quills: gun barrels coming out all along its back, from its rump to just behind its ear, each swept back from
    #  the back's own slope (more at the front, where the back slopes forward); spaced so that no two jackets touch)
    body = [(-14.0, 4.0), (-16.6, 8.0), (-15.6, 13.0), (-11.0, 17.4), (-4.0, 19.4), (3.0, 18.6), (9.0, 15.6), (12.6, 11.0), (12.0, 5.0),
            (6.0, 3.0), (-6.0, 2.8)]
    back = sorted([p for p in smooth(body, True, 12) if p[1] > 8.0 and p[0] < 7.6], key=lambda p: p[0])
    arc = [0.0]
    for i in range(1, len(back)):
        arc.append(arc[-1] + math.hypot(back[i][0] - back[i - 1][0], back[i][1] - back[i - 1][1]))
    for f, sweep, L in ((0.07, 16.0, 15.0), (0.255, 28.0, 18.0), (0.44, 37.0, 19.0), (0.625, 43.0, 19.0), (0.81, 48.0, 17.5), (0.995, 52.0, 15.5)):
        i = min(range(len(back)), key=lambda q: abs(arc[q] - f * arc[-1]))
        (pu, pv), (qu, qv) = back[max(i - 1, 0)], back[min(i + 1, len(back) - 1)]
        tl = math.hypot(qu - pu, qv - pv) or 1.0
        ang = math.degrees(math.atan2((qu - pu) / tl, -(qv - pv) / tl)) + sweep     # (its back's outward slope, swept back)
        ux, uy = math.cos(math.radians(ang)), math.sin(math.radians(ang))
        root = (back[i][0] - ux * 2.5, back[i][1] - uy * 2.5)                       # (2.5 of it hidden in the fur)
        gun_barrel(d, (x / k + s * root[0], y / k + root[1]), ang if s > 0 else 180.0 - ang, L, k, jacket, barrel, ring, jw=1.85)
    # (its body: a round hump of fur over their roots)
    d.fill(fur, Q(body))
    for (u, v) in ((-8.0, 13.0), (-1.0, 15.0), (-11.0, 8.0), (4.0, 12.0)):
        d.line("art_ink", Q([(u - 1.2, v + 0.8), (u, v), (u + 1.0, v + 1.0)]), 0.3 * k)
    # (its face: a pale snout, a black nose, an eye under a set brow, a little grin)
    d.fill(face, Q([(8.6, 15.4), (13.0, 14.6), (17.6, 12.2), (20.6, 9.6), (20.2, 7.6), (16.0, 6.4), (11.6, 5.6), (9.0, 8.0), (8.4, 12.0)]),
           ink=w0 * 0.9)
    d.oval("art_black", *P(20.4, 9.2), 1.25 * k, 1.05 * k, ink=w0 * 0.6)
    d.oval("art_white", *P(14.2, 11.6), 1.35 * k, 1.5 * k, ink=w0 * 0.7)
    d.oval("art_ink", *P(14.8, 11.4), 0.7 * k, 0.9 * k, ink=0.0)
    d.line("art_ink", Q([(12.4, 13.6), (14.6, 13.9), (16.4, 13.0)]), 0.42 * k, taper=False)
    d.line("art_ink", Q([(15.0, 7.4), (17.2, 6.9), (19.0, 7.6)]), 0.35 * k)
    d.oval(fur, *P(9.6, 15.6), 1.4 * k, 1.3 * k, ink=w0 * 0.7)
    d.oval("art_pink", *P(9.7, 15.5), 0.65 * k, 0.6 * k, ink=0.0)
    # (its near legs, running: one reaching forward, one pushing off)
    d.limb(fur, Q([(7.0, 5.0), (10.6, 2.4), (14.0, 1.2)]), [2.1 * k, 1.7 * k, 1.3 * k])
    d.fill("art_black", Q([(12.6, 2.6), (15.6, 2.4), (16.4, 0.6), (12.4, 0.2)]), ink=w0 * 0.6, steps=3)
    d.limb(fur, Q([(-8.0, 5.0), (-12.4, 2.6), (-16.4, 1.0)]), [2.2 * k, 1.7 * k, 1.3 * k])
    d.fill("art_black", Q([(-15.4, 2.4), (-18.6, 1.6), (-18.8, -0.2), (-15.0, 0.0)]), ink=w0 * 0.6, steps=3)
    return P


@painting(56)
def p56(d, a):   # a porcupine with gun barrels for quills
    x, y, size = 36.0, 16.0, 66.0
    k = size / 40.0
    for (v, u0, ln) in ((4.0, -30.0, 20.0), (9.5, -32.5, 26.0), (15.0, -31.0, 18.0)):     # (the express: going like the wind)
        x0, y0 = x + u0 * k, y + v * k
        d.line("art_white", [(x0, y0), (x0 - ln * 0.5, y0 + 0.3), (x0 - ln, y0)], 1.2)
    porcupine(d, x, y, size, 1, fur="art_lbrown", face="art_cream")
    place_name(d, a, where="below", room=5.0)


# ---- 57: Sagebrush Sara -------------------------------------------------------------------------------------------------------
def chaps(d, F, pose, paint="art_lbrown", fringe="art_tan"):
    """Leather chaps over her trouser legs (lady()'s legs for `pose`): each a legging from her hip to her boot, a
    fringe cut along its outer seam."""
    for i, pts in enumerate(LEGS[pose]):
        if not pts:
            continue
        hip, knee, ank = pts
        sx = 1.0 if i == 0 else -1.0
        chain = [hip, lerp(hip, knee, 0.5), knee, lerp(knee, ank, 0.42)]
        radii = [0.5, 0.48, 0.4, 0.37]
        G = smooth(chain, False, 4)
        teeth = []
        for j in range(len(G)):
            t = j / (len(G) - 1)
            rr = radii[0] * (1 - t) + radii[-1] * t
            (pu, pv), (qu, qv) = G[max(j - 1, 0)], G[min(j + 1, len(G) - 1)]
            ln = math.hypot(qu - pu, qv - pv) or 1.0
            nu, nv = (qv - pv) / ln * sx, -(qu - pu) / ln * sx         # (outward, away from her other leg)
            if nu * sx < 0:
                nu, nv = -nu, -nv
            o = rr + (0.2 if j % 2 else 0.04)
            teeth.append((G[j][0] + nu * o, G[j][1] + nv * o))
        inner = [(u - sx * 0.05, v) for u, v in reversed(G)]
        d.fill(fringe, F.Ps(teeth + inner), ink=d.ink * 0.6, steps=1)
        d.limb(paint, F.Ps(chain), [F.r(r) for r in radii])


@painting(57)
def p57(d, a):   # a cowgirl in chaps and hat spinning a lasso around a cloud
    n0 = len(d.ops)
    lumps = cloud(d, 58.0, 73.0, 28, 22, seed=57, flat=False, shade=None)
    n1 = len(d.ops)
    x0, x1 = min(u - r for u, v, r in lumps), max(u + r for u, v, r in lumps)
    y0, y1 = min(v - r for u, v, r in lumps), max(v + r for u, v, r in lumps)
    loop = _arc((x0 + x1) / 2, y0 + (y1 - y0) * 0.42, (x1 - x0) / 2 + 4.5, 4.8, 0, 360, 60, turn=-6.0)
    back = loop[1:30]                                           # (the loop's far side, behind the cloud)
    d.limb("art_tan", back, [0.6] * len(back), ink=d.ink * 0.6)
    d.ops[n0:] = d.ops[n1:] + d.ops[n0:n1]
    F = _lady(d, 20, 2, 64, 1, pose="stride", arms=("up_out", "hip"),
              wear={"top": "art_sky", "trousers": "art_blue", "boots": "art_brown", "stitch": "art_tan", "sleeves": "long", "collar": "art_white",
                    "hat": "art_white", "band": "art_red", "belt": "art_brown", "buckle": "art_gold"},
              hair="rolls", hair_paint="art_blond", hat="cowgirl")
    chaps(d, F, "stride")
    (hx, hy), _ = F.hand["fore"]
    front = loop[29:] + loop[1:2]                               # (its near side, over the cloud)
    d.limb("art_tan", front, [0.65] * len(front), ink=d.ink * 0.6)
    near = loop[33]
    d.limb("art_tan", [(hx, hy), lerp((hx, hy), near, 0.5), near], [0.55, 0.6, 0.65], ink=d.ink * 0.6)
    d.oval("art_skin", hx, hy, F.r(0.155), F.r(0.13), ink=d.ink * 0.7)
    place_name(d, a, room=4.0, at=30.0, edge=48.0)


# ---- 58: Tailwind Tilly --------------------------------------------------------------------------------------------------------
@painting(58)
def p58(d, a):   # a woman in a flying suit and long scarf with the wind at her back
    x, y, h = 40.0, 2.0, 72.0
    n0 = len(d.ops)
    F = lady(d, x, y, h, 1, pose="stride", arms=("hip", "hip"),
             wear={"top": "art_khaki", "trousers": "art_khaki", "belt": "art_brown", "buckle": "art_gold", "sleeves": "long",
                   "boots": "art_brown", "collar": "art_khaki"},
             hair="rolls", hair_paint="art_auburn")
    for sx in (-1, 1):                                          # (her flying suit's breast pockets)
        d.shape("art_khaki", F.Ps([(sx * 0.14, 5.62), (sx * 0.56, 5.62), (sx * 0.54, 5.2), (sx * 0.16, 5.2)]), ink=d.ink * 0.6)
    d.line("art_ink", F.Ps([(0.0, 5.9), (0.0, 4.7)]), F.r(0.035), taper=False)
    for pts, w in (([(0.56, 6.5), (0.86, 6.54), (1.1, 6.72), (1.2, 6.98)], 0.15),     # (her hair's ends lifted forward by the wind)
                   ([(0.58, 6.66), (0.8, 6.8), (0.94, 7.0), (0.96, 7.18)], 0.11)):
        d.limb("art_auburn", F.Ps(pts), [F.r(w), F.r(w * 0.8), F.r(w * 0.5), F.r(0.02)], ink=d.ink * 0.7)
    cxh, cyh = F.P(0.02, 6.9)
    helmet(d, cxh, cyh, F.r(0.94), F.s, paint="art_tan", view="front", part="cap", inside=None)   # (tan leather, lighter than her hair)
    ns = len(d.ops)
    scarf(d, F, "art_white", tail="ahead", length=2.0, rise=0.9)  # (blown out before her: the wind is at her back)
    ne = len(d.ops)
    _lean(d, n0, x, y, -5.0)                                     # (and she leans into her stride with it)
    for (v, ln) in ((7.0, 4.5), (4.2, 5.0)):                    # (the wind behind her: two short gusts at her back)
        yy = y + v * h / HEADS
        xs = min(p[0] for op in d.ops[n0:] if op[0] == "polys" for poly in op[2] for p in poly if abs(p[1] - yy) < 2.5) - 3.6
        d.line("art_white", [(xs - ln, yy + 1.2), (xs - ln * 0.5, yy + 0.3), (xs, yy - 0.8), (xs + 2.2, yy + 1.2), (xs + 0.6, yy + 2.8)], 1.2)
    body_x1 = max(p[0] for i, op in enumerate(d.ops) if op[0] == "polys" and not ns <= i < ne for poly in op[2] for p in poly)
    place_name(d, a, room=5.0, edge=body_x1)                     # (beside her, under her scarf's end)


# ---- 59: Front Porch Swing --------------------------------------------------------------------------------------------------------
def guitar(d, x, y, long, turn=90.0, body="art_orange", neck="art_tan"):
    """A plain flat-top guitar, the foot of its body at (x, y), `long` from there to its headstock, pointing `turn`."""
    c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        return (x + (u * c - v * s_) * long, y + (u * s_ + v * c) * long)

    d.limb(neck, [P(0.42, 0.0), P(0.66, 0.0), P(0.86, 0.0)], [long * 0.032, long * 0.03, long * 0.027])
    d.fill("art_black", [P(0.84, -0.045), P(1.0, -0.055), P(1.03, 0.0), P(1.0, 0.055), P(0.84, 0.045)], steps=2)
    d.blobs(body, [P(0.18, 0.0) + (long * 0.18, long * 0.185), P(0.42, 0.0) + (long * 0.135, long * 0.14)])
    d.oval("art_black", *P(0.38, 0.0), long * 0.06, long * 0.06, ink=d.ink * 0.6)
    d.shape("art_black", [P(0.12, -0.075), P(0.12, 0.075), P(0.155, 0.075), P(0.155, -0.075)], ink=0.0)
    for v in (-0.014, 0.014):
        d.line("art_cream", [P(0.14, v), P(0.98, v)], long * 0.006, taper=False)
    return P


def lemon_slice(d, x, y, r):
    """A round of lemon: its rind, its pale pith, its segments."""
    d.oval("art_yellow", x, y, r, r, ink=d.ink * 0.6)
    d.oval("art_cream", x, y, r * 0.78, r * 0.78, ink=0.0)
    for i in range(6):
        a_ = math.radians(30 + 60 * i)
        d.line("art_yellow", [(x, y), (x + r * 0.7 * math.cos(a_), y + r * 0.7 * math.sin(a_))], r * 0.14, taper=False)


def pitcher(d, x, y, h, glass="art_sky", drink="art_cream"):
    """A glass pitcher of lemonade standing at (x, y), `h` tall: its spout to the right, its handle to the left, two
    rounds of lemon in it (`glass` its tint: a pressed-glass pitcher of the thirties, green or blue)."""
    k = h / 10.0

    def P(u, v):
        return (x + u * k, y + v * k)

    d.limb(glass, [P(-2.6, 8.4), P(-5.0, 7.6), P(-5.0, 3.6), P(-3.0, 2.2)], [0.6 * k] * 4)
    outline = [P(-3.0, 0.0), P(-3.6, 2.6), P(-3.5, 6.2), P(-2.6, 8.6), P(-2.7, 10.0), P(2.2, 10.0), P(4.0, 10.8), P(3.0, 8.6),
               P(3.5, 6.2), P(3.6, 2.6), P(3.0, 0.0)]
    d.fill(glass, outline, steps=3)
    d.fill(drink, [P(-2.9, 0.4), P(-3.3, 2.6), P(-3.2, 6.2), P(-2.7, 7.6), P(2.8, 7.6), P(3.2, 6.2), P(3.3, 2.6), P(2.9, 0.4)], ink=0.0, steps=3)
    d.line("art_ink", [P(-2.7, 7.6), P(0.0, 7.75), P(2.8, 7.6)], 0.22 * k, taper=False)
    lemon_slice(d, *P(-0.8, 4.6), 1.5 * k)
    lemon_slice(d, *P(1.6, 2.4), 1.3 * k)
    d.line("art_white", [P(-2.4, 1.4), P(-2.7, 4.6), P(-2.2, 8.4)], 0.4 * k)


def porch_swing(d, x, y, w, paint="art_white", chain="art_lgrey", beam="art_tan", top=None):
    """A porch swing hanging on its chains from a beam: its seat's front at y, its middle at x, `w` wide."""
    hb = w * 0.36                                              # (its back's height)
    arm_y = y + w * 0.2
    top = top if top is not None else y + w * 0.95
    for sx in (-1, 1):                                          # (its chains, from each arm's front up to the beam)
        d.limb(chain, [(x + sx * (w / 2 + 0.6), arm_y + 1.0), (x + sx * (w / 2 + 0.2), top)], [1.0, 1.0], ink=d.ink * 0.7)
    d.shape(beam, [(x - w * 0.62, top - 1.5), (x - w * 0.62, top + 3.5), (x + w * 0.62, top + 3.5), (x + w * 0.62, top - 1.5)])
    d.shape("art_lgrey", [(x - w / 2 + 1.0, y + 0.5), (x - w / 2 + 2.0, y + 3.4), (x + w / 2 - 2.0, y + 3.4), (x + w / 2 - 1.0, y + 0.5)])
    for i in range(3):                                         # (its back: three wide boards)
        y0 = y + 3.4 + i * hb / 3
        d.shape(paint, [(x - w / 2 + 2.0, y0), (x - w / 2 + 2.0, y0 + hb / 3 - 1.2), (x + w / 2 - 2.0, y0 + hb / 3 - 1.2), (x + w / 2 - 2.0, y0)])
    d.shape(paint, [(x - w / 2 - 1.0, y - 3.4), (x - w / 2 - 1.0, y + 0.8), (x + w / 2 + 1.0, y + 0.8), (x + w / 2 + 1.0, y - 3.4)])
    for sx in (-1, 1):                                         # (its arms: a post, an arm rest)
        d.shape(paint, [(x + sx * (w / 2 - 0.6), y + 0.6), (x + sx * (w / 2 - 0.6), arm_y), (x + sx * (w / 2 + 1.6), arm_y), (x + sx * (w / 2 + 1.6), y + 0.6)])
        d.shape(paint, [(x + sx * (w / 2 - 1.6), arm_y), (x + sx * (w / 2 - 1.6), arm_y + 2.4), (x + sx * (w / 2 + 2.6), arm_y + 2.4),
                        (x + sx * (w / 2 + 2.6), arm_y)])
    return top


@painting(59)
def p59(d, a):   # a porch swing with a guitar and a pitcher of lemonade
    x, y, w = 30.0, 16.0, 44.0
    porch_swing(d, x, y, w, top=82.0)                         # (long chains, so that the picture stands the decal's height)
    guitar(d, x - 13.0, y + 0.8, 33, turn=74)
    pitcher(d, x + 11.0, y + 1.0, 15, glass="art_green", drink="art_yellow")               # (green glass, so that it stands off the white swing)
    place_name(d, a, lines=["Front", "Porch", "Swing"], room=6.0)
