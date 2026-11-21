"""Batch 2, numbers 60 to 66 (painter b2_f): a gunner waist deep in spent shell cases, a blue crab in a sailor cap with
a wrench in each claw, a prize pumpkin bigger than its wagon, an opossum asleep hanging by its tail from a bomb, a
Dalmatian in a fire helmet climbing a ladder into the clouds, a bighorn ram with a kit bag over his shoulder, and a cook
in her apron holding up a wishbone. Each picture is drawn from flat shapes with noseart's Drawing API and the shared
figure kit; the helpers below (a big spent case and the brass mound, a crab, a sailor's cap, an open wrench, a pumpkin, a farm
wagon, a prize rosette, an opossum and its tail, a ladder, a fire helmet, a Dalmatian, a kit bag, a ram's horn, a bighorn
ram, a wishbone) are built the same way, for these paintings."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402


# ---- helpers ------------------------------------------------------------------------------------------------------------
def _frame(x, y, k=1.0, s=1, turn=0.0, flip=False):
    """A frame of one's own on the grid: P(u, v), u toward the side it faces (s), v up (down if `flip`), k grid units to
    one of its own, the whole turned `turn` degrees anticlockwise about (x, y)."""
    c, sn = math.cos(math.radians(turn)), math.sin(math.radians(turn))

    def P(u, v):
        u, v = s * u * k, (-v if flip else v) * k
        return (x + u * c - v * sn, y + u * sn + v * c)
    return P


# ---- 60: spent shell cases ------------------------------------------------------------------------------------------------
def brass_case(d, base, ang, L, r, tilt=0.0, paint="art_yellow", mouth="art_ink", ink=None, shine="art_white", sunk=0.0, heap="art_gold"):
    """One spent .50 calibre case, big and plain: the middle of its base at `base`, lying toward `ang` degrees on the
    grid, L long and r round (a straight tube, no neck), tipped `tilt` degrees toward the viewer so that its open mouth
    shows as an ellipse (0: side on; 90: end on, a round ring); its base's rim a little wider, a groove above it. With
    `sunk`, that share of it (from its base) is buried in the heap (painted over in the heap's paint)."""
    a = math.radians(ang)
    ux, uy = math.cos(a), math.sin(a)
    nx, ny = -uy, ux
    tp = math.radians(tilt)
    Lp = L * math.cos(tp)                                # (its length as seen)
    m = r * max(math.sin(tp), 0.3)                       # (its mouth's ellipse, along its length)
    x0, y0 = base

    def P(u, v):                                         # (u along it from its base, v across it)
        return (x0 + ux * u + nx * v, y0 + uy * u + ny * v)
    rim = min(0.12 * L, 0.3 * Lp)
    back = [P(m * math.cos(math.radians(t)), r * 1.1 * math.sin(math.radians(t))) for t in range(90, 271, 15)]
    d.shape(paint, [P(Lp, -r), P(Lp, r), P(rim, r), P(rim, r * 1.1)] + back + [P(rim, -r * 1.1), P(rim, -r)], ink=ink)
    d.line("art_ink", [P(rim, r * 1.0), P(rim, 0.0), P(rim, -r * 1.0)], r * 0.16, taper=False)
    if Lp - rim > r * 1.2:
        d.line(shine, [P(rim + (Lp - rim) * 0.2, r * 0.5), P(rim + (Lp - rim) * 0.5, r * 0.56), P(Lp - m * 1.3, r * 0.5)], r * 0.3)
    if sunk:
        cut = Lp * sunk
        d.fill(heap, [P(-m - r, -r * 1.6), P(-m - r, r * 1.6), P(cut - r * 0.3, r * 1.6), P(cut + r * 0.1, r * 0.6), P(cut + r * 0.2, 0.0),
                      P(cut + r * 0.1, -r * 0.6), P(cut - r * 0.3, -r * 1.6)], ink=0.0, steps=4)
    d.oval(paint, *P(Lp, 0.0), m, r, turn=ang, ink=ink)
    d.oval(mouth, *P(Lp + m * 0.06, 0.0), m * 0.64, r * 0.66, turn=ang, ink=0.0)
    return P


def brass_mound(d, cx, base, top, hw, paint="art_gold"):
    """The heap of spent brass as one bold mound: its foot `hw` either side of cx on `base`, its rounded top at `top`."""
    H = top - base
    prof = [(1.0, 0.0), (0.97, 0.16), (0.88, 0.38), (0.72, 0.62), (0.5, 0.84), (0.24, 0.97), (0.0, 1.0)]
    pts = [(cx - hw * u, base + H * v) for u, v in prof] + [(cx + hw * u, base + H * v) for u, v in reversed(prof[:-1])]
    pts += [(cx + hw * 0.6, base - 0.6), (cx - hw * 0.6, base - 0.6)]
    d.fill(paint, pts, steps=5)


# ---- 61: a blue crab and a sailor's cap -------------------------------------------------------------------------------------
def sailor_cap(d, x, y, w, turn=0.0, paint="art_white", inside="art_lgrey"):
    """A sailor's white gob cap, its brim's foot's middle at (x, y), w across its brim's top edge: a tall brim turned up
    all round and flaring out to its rolled top edge (inked clear), the shade inside its far side, and its low rounded
    crown rising inside it."""
    P = _frame(x, y, w / 16.0, 1, turn)
    k = w / 16.0
    rim = [P(8.0 * math.cos(math.radians(t)), 5.4 + 1.3 * math.sin(math.radians(t))) for t in range(0, 360, 15)]
    d.fill(inside, rim, steps=3)                                                       # (the inside of its far side)
    d.fill(paint, [P(-4.9, 4.2), P(-4.5, 6.6), P(-2.6, 8.1), P(0.0, 8.5), P(2.6, 8.1), P(4.5, 6.6), P(4.9, 4.2)], steps=4)   # (its crown)
    front = [P(-8.0, 5.4), P(-6.6, 2.6), P(-5.4, 0.1), P(-2.8, -0.35), P(0.0, -0.45), P(2.8, -0.35), P(5.4, 0.1), P(6.6, 2.6), P(8.0, 5.4)]
    front += [P(8.0 * math.cos(math.radians(t)), 5.4 - 1.3 * math.sin(math.radians(t))) for t in range(15, 180, 15)]
    d.shape(paint, front)                                                              # (its brim, turned up before it)
    d.line("art_ink", [P(8.0 * math.cos(math.radians(t)), 5.4 - 1.3 * math.sin(math.radians(t))) for t in range(0, 181, 15)], k * 0.5, taper=False)
    d.line("art_lgrey", [P(-6.2, 2.4), P(-3.0, 1.75), P(0.0, 1.6), P(3.0, 1.75), P(6.2, 2.4)], k * 0.3)   # (a row of stitching)
    d.line("art_lgrey", [P(-2.2, 7.6), P(0.0, 7.9), P(1.8, 7.7)], k * 0.35)


def open_wrench(d, x0, y0, x1, y1, w, paint="art_lsteel", grip="art_red"):
    """An open-ended wrench from the end of its handle (x0, y0) to the middle of its head (x1, y1), w wide: a flat steel
    handle, its lower half dipped red, and a round head with its jaw open straight on along it."""
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    nx, ny = -uy, ux
    R, h = 1.55 * w, 0.5 * w
    th = math.atan2(uy, ux)
    al = math.asin(h / R)
    head = [(x1 + R * math.cos(th + al + (2 * math.pi - 2 * al) * i / 24), y1 + R * math.sin(th + al + (2 * math.pi - 2 * al) * i / 24)) for i in range(25)]
    head += [(x1 + ux * R * 0.05 - nx * h, y1 + uy * R * 0.05 - ny * h), (x1 + ux * R * 0.05 + nx * h, y1 + uy * R * 0.05 + ny * h)]
    hw = w * 0.5
    d.shape(paint, [(x0 + nx * hw, y0 + ny * hw), (x1 - ux * R * 0.7 + nx * hw * 1.1, y1 - uy * R * 0.7 + ny * hw * 1.1),
                    (x1 - ux * R * 0.7 - nx * hw * 1.1, y1 - uy * R * 0.7 - ny * hw * 1.1), (x0 - nx * hw, y0 - ny * hw)])
    d.shape(paint, head)
    d.limb(grip, [(x0 + ux * w * 0.4, y0 + uy * w * 0.4), (x0 + dx * 0.45, y0 + dy * 0.45)], [w * 0.62, w * 0.6])
    d.line("art_white", [(x1 - ux * R * 0.8 + nx * hw * 0.4, y1 - uy * R * 0.8 + ny * hw * 0.4), (x0 + dx * 0.5 + nx * hw * 0.4, y0 + dy * 0.5 + ny * hw * 0.4)], w * 0.18)


def crab(d, x, y, k=1.0, shell="art_lblue", claws="art_sky", belly="art_cream", cap="art_white"):
    """A blue crab seen from before, its shell's middle at (x, y), about 54 k across its spines: its legs splayed, its
    claws raised to its sides, its eyes up on their stalks, a sailor's cap on its shell. Returns its frame and where
    each claw grips (the fore claw, on the right, first)."""
    P = _frame(x, y, k)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    w0 = d.ink
    # (its walking legs, three a side, splayed out and down from under its shell)
    for sx in (-1, 1):
        for i in range(3):
            hip, knee, foot = (sx * (8 + 3.4 * i), -3.0 - 0.6 * i), (sx * (19.5 + 3.8 * i), 1.6 - 2.0 * i), (sx * (22.5 + 5.0 * i), -11.0 - 1.6 * i)
            d.limb(shell, Q([hip, knee, foot]), [1.7 * k, 1.5 * k, 0.45 * k])
    # (its underside, pale, below the shell's front)
    d.fill(belly, Q([(-14.0, -2.0), (-12.0, -7.2), (-6.0, -9.6), (0.0, -10.2), (6.0, -9.6), (12.0, -7.2), (14.0, -2.0)]), steps=4)
    # (its arms, out from under the shell's front to its claws)
    grips = []
    for sx in (-1, 1):
        d.limb(claws, Q([(sx * 12.0, -2.4), (sx * 19.0, 1.6), (sx * 24.0, 7.0)]), [2.3 * k, 2.2 * k, 2.4 * k])
    # (its shell: the spines first, then the shell over their roots, a row of small teeth along its front)
    for sx in (-1, 1):
        d.shape(shell, Q([(sx * 15.0, 4.6), (sx * 27.5, 2.4), (sx * 15.0, -0.8)]))
        for t in (0.38, 0.55, 0.72):
            a = math.pi / 2 - sx * (math.pi / 2) * t
            ex, ey = 19.0 * math.cos(a), 1.6 + 8.8 * math.sin(a)
            nx, ny = math.cos(a) / 19.0, math.sin(a) / 8.8
            ln = math.hypot(nx, ny)
            nx, ny = nx / ln, ny / ln
            tx, ty = -ny, nx
            d.shape(shell, Q([(ex - tx * 1.4, ey - ty * 1.4), (ex + nx * 2.0, ey + ny * 2.0), (ex + tx * 1.4, ey + ty * 1.4)]), ink=w0 * 0.8)
    d.fill(shell, Q([(-19.0, 1.6), (-16.4, 6.6), (-9.4, 9.8), (0.0, 10.6), (9.4, 9.8), (16.4, 6.6), (19.0, 1.6), (15.4, -3.0),
                     (8.0, -5.2), (0.0, -5.8), (-8.0, -5.2), (-15.4, -3.0)]), steps=5)
    # (its grin)
    d.line("art_ink", Q([(-6.0, 0.4), (-3.0, -2.0), (0.0, -2.6), (3.0, -2.0), (6.0, 0.4)]), 0.9 * k)
    # (its cap, square on the top of its shell, cocked; its eyes, black beads up on their stalks, either side of it)
    sailor_cap(d, *P(-2.2, 10.0), 25.0 * k, turn=-20.0, paint=cap)
    for sx in (-1, 1):
        d.limb(shell, Q([(sx * 7.0, 7.0), (sx * 10.6, 11.2), (sx * 13.6, 14.4)]), [1.2 * k, 1.05 * k, 1.0 * k], ink=w0 * 0.8)
        d.oval("art_black", *P(sx * 14.6, 15.8), 2.1 * k, 2.3 * k, ink=w0 * 0.8)
        d.oval("art_white", *P(sx * 14.6 - 0.7, 16.6), 0.55 * k, 0.6 * k, ink=0.0)
    # (its claws: a swollen hand, a fixed finger and a moving one, open round the thing it grips)
    for sx in (-1, 1):
        d.fill(claws, Q([(sx * 21.6, 5.0), (sx * 21.0, 9.6), (sx * 23.4, 13.6), (sx * 27.0, 14.6), (sx * 29.4, 12.4), (sx * 29.2, 8.0),
                         (sx * 26.6, 4.6)]), steps=4)
        d.fill(claws, Q([(sx * 24.4, 13.0), (sx * 25.2, 17.4), (sx * 27.2, 19.8), (sx * 27.6, 18.0), (sx * 26.8, 13.8)]), ink=w0 * 0.9, steps=3)
        d.fill(claws, Q([(sx * 28.4, 12.0), (sx * 30.6, 15.4), (sx * 31.6, 17.6), (sx * 30.4, 17.4), (sx * 28.0, 14.0)]), ink=w0 * 0.9, steps=3)
        grips.append(P(sx * 28.2, 16.2))
    return P, grips[::-1]


def crab_finger(d, P, sx, claws, k):
    """A claw's moving finger again, over the thing it grips."""
    d.fill(claws, [P(sx * 24.4, 13.0), P(sx * 25.2, 17.4), P(sx * 27.2, 19.8), P(sx * 27.6, 18.0), P(sx * 26.8, 13.8)], ink=d.ink * 0.9, steps=3)


# ---- 62: a pumpkin, a farm wagon, a prize rosette ---------------------------------------------------------------------------
def pumpkin(d, x, y, rx, ry, paint="art_orange", stem="art_green", leaf="art_lgreen", shine="art_yellow"):
    """A great pumpkin, its middle at (x, y), rx by ry: five lobes (the outer ones first, each overlapping the last),
    a short thick stem, one leaf and a curl of vine."""
    for sx in (-1, 1):
        d.oval(paint, x + sx * rx * 0.62, y - ry * 0.06, rx * 0.4, ry * 0.76)
    for sx in (-1, 1):
        d.oval(paint, x + sx * rx * 0.33, y - ry * 0.01, rx * 0.43, ry * 0.94)
    d.oval(paint, x, y + ry * 0.01, rx * 0.4, ry)
    d.line(shine, [(x - rx * 0.24, y + ry * 0.5), (x - rx * 0.18, y + ry * 0.66), (x - rx * 0.08, y + ry * 0.76)], ry * 0.08)
    d.limb(stem, [(x + rx * 0.01, y + ry * 0.86), (x + rx * 0.02, y + ry * 1.08), (x + rx * 0.12, y + ry * 1.24)], [ry * 0.13, ry * 0.11, ry * 0.1])
    d.fill(leaf, [(x + rx * 0.06, y + ry * 1.0), (x - rx * 0.12, y + ry * 1.2), (x - rx * 0.34, y + ry * 1.18), (x - rx * 0.44, y + ry * 1.04),
                  (x - rx * 0.3, y + ry * 0.98), (x - rx * 0.12, y + ry * 0.94)], steps=4)
    d.line("art_ink", [(x - rx * 0.36, y + ry * 1.07), (x - rx * 0.16, y + ry * 1.08), (x + rx * 0.02, y + ry * 1.0)], ry * 0.03)
    d.line(leaf, [(x + rx * 0.14, y + ry * 1.2), (x + rx * 0.26, y + ry * 1.32), (x + rx * 0.34, y + ry * 1.22), (x + rx * 0.28, y + ry * 1.14),
                  (x + rx * 0.22, y + ry * 1.2)], ry * 0.05, taper=False)


def farm_wagon(d, x, y, long, box="art_red", wheels="art_yellow", tongue="art_cream", s=-1):
    """A small farm wagon, side on, its wheels on y, its middle at x, its box `long` (plank sides, a stake at each
    corner), the front wheels smaller, and its tongue turned up at its front toward s as a parked wagon's stands, a
    crossbar at its top. Returns the top of its box (y)."""
    L = long
    top = y + L * 0.5
    for u, r in ((-0.3 * s, 0.215), (0.3 * s, 0.17)):        # (the far wheels, peeping under the box)
        d.oval("art_dark", x + u * L + s * L * 0.05, y + L * r, L * r * 0.95, L * r * 0.95, ink=d.ink * 0.7)
    d.shape(box, [(x - L * 0.5, y + L * 0.28), (x + L * 0.5, y + L * 0.28), (x + L * 0.52, top), (x - L * 0.52, top)])
    for t in (1 / 3.0, 2 / 3.0):
        v = y + L * (0.28 + 0.22 * t)
        d.line("art_ink", [(x - L * 0.5, v), (x + L * 0.5, v)], L * 0.012, taper=False)
    for u in (-0.5, 0.5):
        d.shape(box, [(x + u * L - L * 0.035, y + L * 0.26), (x + u * L + L * 0.035, y + L * 0.26), (x + u * L * 1.04 + L * 0.035, top + L * 0.05),
                      (x + u * L * 1.04 - L * 0.035, top + L * 0.05)], ink=d.ink * 0.8)
    for u, r in ((-0.3 * s, 0.215), (0.3 * s, 0.17)):
        wheel(d, x + u * L, y + L * r, L * r, rim=wheels, spokes=8)
    fx, fy = x + s * L * 0.3, y + L * 0.17                   # (the front axle: the tongue's pin)
    tx, ty = x + s * L * 0.5, y + L * 1.12                   # (its top, standing up)
    d.limb(tongue, [(fx, fy), ((fx + tx) / 2 + s * L * 0.01, (fy + ty) / 2), (tx, ty)], [L * 0.03, L * 0.028, L * 0.026])
    d.limb(tongue, [(tx - L * 0.1, ty + L * 0.02), (tx + L * 0.1, ty - L * 0.01)], [L * 0.028, L * 0.028])
    d.oval("art_dark", fx, fy, L * 0.035, L * 0.035, ink=d.ink * 0.6)
    return top


def rosette(d, x, y, r, paint="art_blue", middle="art_yellow"):
    """A prize rosette, its middle at (x, y), r across its pleats: two tails hanging, the pleated ring, a plain button."""
    d.shape(paint, [(x - r * 0.2, y - r * 0.2), (x - r * 0.62, y - r * 2.1), (x - r * 0.38, y - r * 1.86), (x - r * 0.18, y - r * 2.16), (x + r * 0.16, y - r * 0.34)])
    d.shape(paint, [(x + r * 0.12, y - r * 0.3), (x + r * 0.36, y - r * 2.2), (x + r * 0.58, y - r * 1.9), (x + r * 0.84, y - r * 2.08), (x + r * 0.5, y - r * 0.2)])
    n = 22
    d.shape(paint, [(x + (r if i % 2 == 0 else r * 0.84) * math.cos(2 * math.pi * i / n), y + (r if i % 2 == 0 else r * 0.84) * math.sin(2 * math.pi * i / n)) for i in range(n)])
    d.oval(middle, x, y, r * 0.5, r * 0.5, ink=d.ink * 0.8)


# ---- 63: an opossum asleep, hanging by its tail -----------------------------------------------------------------------------
def _along(pts, n):
    """n + 1 points spaced evenly along the smooth open curve through `pts`, with the unit normal to its left at each."""
    c = smooth(pts, False, 8)
    seg = [math.hypot(c[i + 1][0] - c[i][0], c[i + 1][1] - c[i][1]) for i in range(len(c) - 1)]
    tot = sum(seg)
    out, j, run = [], 0, 0.0
    for i in range(n + 1):
        want = tot * i / n
        while j < len(seg) - 1 and run + seg[j] < want:
            run += seg[j]
            j += 1
        f = (want - run) / max(seg[j], 1e-9)
        p = lerp(c[j], c[j + 1], min(max(f, 0.0), 1.0))
        tx, ty = c[j + 1][0] - c[j][0], c[j + 1][1] - c[j][1]
        ln = math.hypot(tx, ty) or 1.0
        out.append((p, (-ty / ln, tx / ln)))
    return out


def possum(d, x, y, size, s=1, fur="art_grey", grizzle="art_lgrey", face="art_white", legs="art_black", pink="art_pink",
           ears="art_black", tips="art_lgrey"):
    """A plain opossum from life, asleep, hanging head down: the root of its tail at (x, y), `size` from there to the
    crown of its head, its belly and face toward s. Its grey coat with a pale band of long guard hairs down its back
    breaking out in big shaggy tufts, its white face with the long pointed snout hanging down and the pink nose, thin
    leaf-like black ears with pale tips, its eye shut; its forelegs dangling limp below its head, small pink hands with
    spread fingers; its hind leg tucked up against its rump by its tail. Returns its frame (u toward its belly, v down
    from its rump)."""
    k = size / 40.0
    P = _frame(x, y, k, s, flip=True)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    w0 = d.ink

    def hand(cx, cy, r, spread=(-50, -20, 10, 40), down=90.0):
        """A small pink hand, its fingers spread toward `down` degrees in the frame."""
        for f in spread:
            t = math.radians(down + f)
            d.limb(pink, Q([(cx, cy), (cx + 1.9 * r * math.cos(t), cy + 1.9 * r * math.sin(t))]), [0.42 * r * k, 0.3 * r * k], ink=w0 * 0.6)
        d.oval(pink, *P(cx, cy), r * k, r * 0.9 * k, ink=w0 * 0.7)

    # (its far ear and far foreleg, behind it)
    d.fill(ears, Q([(-5.6, 29.2), (-8.6, 28.6), (-11.4, 29.6), (-12.2, 31.0), (-10.6, 32.0), (-7.4, 31.8)]), ink=w0 * 0.8, steps=4)
    d.fill(tips, Q([(-10.0, 29.4), (-11.4, 29.6), (-12.2, 31.0), (-10.8, 31.8), (-9.8, 31.0)]), ink=w0 * 0.5, steps=3)
    d.limb(legs, Q([(8.4, 21.6), (12.4, 26.6), (13.2, 33.6), (12.8, 40.2)]), [1.5 * k, 1.2 * k, 1.0 * k, 0.9 * k], ink=w0 * 0.8)
    hand(12.8, 41.2, 1.1)
    # (its head, white, hanging under its shoulders: its long snout pointing down)
    d.fill(face, Q([(-5.6, 25.4), (-6.6, 28.8), (-7.0, 31.6), (-6.0, 34.4), (-4.2, 36.4), (-1.8, 38.2), (1.8, 40.3), (4.6, 42.0), (6.4, 42.9),
                    (7.6, 42.2), (7.7, 39.6), (7.1, 35.0), (6.9, 30.4), (6.2, 26.0), (2.0, 24.8), (-2.0, 24.8)]), steps=5)
    # (its body, hanging: the rump up by its tail, the shoulders down by its head)
    body = [(-1.0, -1.4), (2.2, 0.4), (5.0, 3.6), (7.4, 8.2), (9.0, 13.4), (9.4, 18.4), (8.2, 23.0), (5.6, 26.6),
            (1.6, 28.6), (-3.0, 28.4), (-6.8, 26.0), (-9.0, 21.6), (-9.6, 16.0), (-8.6, 10.0), (-6.4, 4.6), (-3.6, 0.8)]
    d.fill(fur, Q(body), steps=5)
    # (its grizzle: one pale band of long guard hairs down its back, its outer edge breaking out in shaggy tufts, its
    #  inner edge in long locks, the hair lying back toward the tail)
    N = 40
    back = _along([(0.6, -1.3), (-3.6, 0.8), (-6.4, 4.6), (-8.6, 10.0), (-9.6, 16.0), (-9.0, 21.6), (-7.0, 25.6)], N)

    def off(i, o, slide=0.0):
        (px, py), (nx, ny) = back[min(max(i, 0), N)]
        (qx, qy), _ = back[min(max(i + 1, 0), N)]
        return (px + nx * o + (qx - px) * slide, py + ny * o + (qy - py) * slide)   # (o out from its back)
    outer = [off(0, 0.2), off(1, 0.6)]
    for i0, big in ((2, 1.0), (11, 1.15), (20, 1.05), (29, 0.9)):    # (a shaggy tuft: steep up to its point, lying toward the tail, then long back down)
        outer += [off(i0 + 1, 1.3 * big), off(i0 + 1, 2.8 * big, 0.7), off(i0 + 3, 2.6 * big), off(i0 + 5, 1.9 * big), off(i0 + 7, 1.0 * big), off(i0 + 8, 0.5)]
    outer += [off(N - 1, 0.5), off(N, 0.2)]
    inner = [off(N, -0.5), off(N - 4, -2.2), off(N - 10, -3.0), off(N - 17, -3.7), off(N - 24, -3.4), off(N - 31, -3.9), off(N - 36, -2.6), off(1, -0.8)]
    d.fill(grizzle, Q(outer + inner), ink=w0 * 0.8, steps=4)
    # (its hind leg, tucked up against its rump by its tail, its small foot gripping there)
    d.limb(legs, Q([(2.6, 8.4), (5.0, 6.4), (4.6, 3.2), (3.0, 1.4)]), [2.3 * k, 1.9 * k, 1.4 * k, 1.1 * k], ink=w0 * 0.9)
    hand(2.3, 0.7, 0.85, spread=(-45, -10, 25), down=205.0)
    # (its pink nose, its mouth)
    d.oval(pink, *P(6.9, 42.5), 1.55 * k, 1.2 * k, turn=-40 * s, ink=w0 * 0.7)
    d.line("art_ink", Q([(6.8, 40.8), (5.9, 38.8), (6.1, 36.8)]), 0.45 * k)                       # (its mouth, content)
    # (its eye, shut: a long lid with three lashes, the snout's way along it)
    lid = [(-0.2, 31.6), (1.5, 33.6), (2.4, 35.9)]
    d.line("art_ink", Q(lid), 0.62 * k, taper=False)
    for (u0, v0), ang in (((0.5, 32.4), 0.0), ((1.6, 33.7), 20.0), ((2.2, 35.0), 40.0)):
        t = math.radians(-20.0 + ang)
        d.line("art_ink", Q([(u0, v0), (u0 + 1.5 * math.cos(t), v0 + 1.5 * math.sin(t))]), 0.36 * k, taper=False)
    # (its near ear: thin, rounded, a leaf standing off its crown, black with a pale tip)
    d.fill(ears, Q([(-4.6, 32.6), (-7.0, 33.8), (-10.6, 35.8), (-12.0, 37.4), (-10.6, 38.4), (-7.4, 37.4), (-5.0, 35.4)]), ink=w0 * 0.8, steps=4)
    d.fill(tips, Q([(-9.4, 35.8), (-10.6, 35.8), (-12.0, 37.4), (-10.6, 38.4), (-9.0, 37.6)]), ink=w0 * 0.5, steps=3)
    # (its near foreleg, dangling limp below its head)
    d.limb(legs, Q([(6.4, 21.0), (9.4, 26.4), (9.8, 34.6), (9.2, 42.6)]), [1.6 * k, 1.25 * k, 1.05 * k, 0.95 * k], ink=w0 * 0.9)
    hand(9.2, 43.7, 1.2)
    return P


def possum_tail(d, root, path, rise, base="art_black", skin="art_pink", r0=2.0, r1=1.3):
    """Its tail: black at its root (up from `root` for `rise`), then bare and pink along `path`."""
    (x0, y0) = root
    d.limb(base, [(x0, y0 - 1.0), (x0, y0 + rise * 0.5), path[0]], [r0, r0 * 0.96, r0 * 0.92])
    n = len(path)
    d.limb(skin, path, [r0 * 0.92 + (r1 - r0 * 0.92) * i / (n - 1) for i in range(n)])


# ---- 64: a ladder, a Dalmatian, a fire helmet -------------------------------------------------------------------------------
def ladder(d, base, top, wide, rungs, paint="art_tan", w=1.8):
    """A wooden ladder from `base` to `top` (the middles of its ends), `wide` between its rails; its rungs at the
    given distances up it from its base."""
    (x0, y0), (x1, y1) = base, top
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    nx, ny = -uy, ux
    for t in rungs:
        cx, cy = x0 + ux * t, y0 + uy * t
        a, b = (cx - nx * wide / 2, cy - ny * wide / 2), (cx + nx * wide / 2, cy + ny * wide / 2)
        h = w * 0.36
        d.shape(paint, [(a[0] - ux * h, a[1] - uy * h), (b[0] - ux * h, b[1] - uy * h), (b[0] + ux * h, b[1] + uy * h), (a[0] + ux * h, a[1] + uy * h)], ink=d.ink * 0.8)
    for sg in (-1, 1):
        ax, ay = x0 + nx * sg * wide / 2, y0 + ny * sg * wide / 2
        bx, by = x1 + nx * sg * wide / 2, y1 + ny * sg * wide / 2
        h = w / 2
        d.shape(paint, [(ax - nx * h, ay - ny * h), (ax + nx * h, ay + ny * h), (bx + nx * h, by + ny * h), (bx - nx * h, by - ny * h)])


def fire_helmet(d, P, k, paint="art_lred", shield="art_yellow"):
    """A fireman's leather helmet on a head drawn in frame P (its dome's middle at (0, 0) of the frame, facing +u): the
    high dome, the long brim swept down behind, the plain shield standing up before."""
    w0 = d.ink
    d.fill(paint, [P(-10.0, -2.6), P(-7.6, -0.6), P(-3.0, 0.6), P(4.0, 0.6), P(7.0, 0.2), P(7.4, -0.8), P(4.0, -1.0), P(-3.0, -1.0),
                   P(-7.0, -2.2), P(-9.6, -4.0)], steps=4)
    d.fill(paint, [P(-5.2, 0.0), P(-4.8, 3.8), P(-2.4, 6.6), P(1.0, 7.2), P(4.0, 5.6), P(5.4, 2.4), P(5.4, 0.0)], steps=5)
    for u in (-2.6, 0.2, 2.8):
        d.line("art_ink", [P(u * 0.9, 6.6 - abs(u) * 0.5), P(u * 1.15, 3.4), P(u * 1.25, 0.6)], w0 * 0.8)
    # (the shield: a flat plate standing straight up from the brim's front, a border round its face, no number on it)
    plate = [(4.6, 0.2), (4.4, 4.8), (4.7, 7.0), (5.6, 8.3), (6.6, 8.8), (7.6, 8.3), (8.5, 7.0), (8.8, 4.8), (8.6, 0.2), (6.6, -0.4)]
    d.fill(shield, [P(u, v) for u, v in plate], ink=w0 * 0.9, steps=3)
    mid = (6.6, 4.2)
    d.line("art_ink", smooth([P(mid[0] + (u - mid[0]) * 0.68, mid[1] + (v - mid[1]) * 0.74) for u, v in plate], True, 3), w0 * 0.55, taper=False, steps=1)
    d.line(paint, [P(4.0, 0.0), P(6.6, -0.5), P(9.0, 0.1)], w0 * 1.6, taper=False)          # (the brim's lip under it)


def dalmatian(d, x, y, size, s=-1, turn=0.0, coat="art_white", spots="art_black", helmet="art_lred", head=1.0):
    """A Dalmatian climbing, reared up on its hind legs, its near hind paw on a rung at (x, y), `size` from there to the
    top of its head, facing s, the whole turned `turn` degrees (as its ladder leans): its forepaws on the rung five up
    (40 of its own units: rungs every 8), a fire helmet on its head. Returns its frame."""
    k = size / 50.0
    P = _frame(x, y, k, s, turn)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    w0 = d.ink
    far = "art_lgrey"
    # (its far legs, behind it: a forepaw on the rung, a hind paw)
    d.limb(far, Q([(0.0, 31.0), (6.0, 36.4), (10.6, 40.6)]), [2.4 * k, 2.0 * k, 1.8 * k])
    d.oval(far, *P(11.4, 41.0), 2.6 * k, 1.6 * k, turn=turn + 20 * s, ink=w0 * 0.8)
    # (its tail, out behind, up)
    d.limb(coat, Q([(-6.0, 17.0), (-10.6, 15.8), (-14.2, 17.4), (-15.0, 20.6), (-13.2, 22.8)]), [2.2 * k, 1.8 * k, 1.3 * k, 0.9 * k, 0.5 * k])
    # (its near hind leg: the thigh, the shank, the paw on the rung)
    d.limb(coat, Q([(-2.6, 5.6), (-2.2, 2.6), (1.4, 0.8)]), [1.9 * k, 1.7 * k, 1.6 * k])
    d.oval(coat, *P(2.0, 0.9), 3.0 * k, 1.6 * k, turn=turn, ink=w0 * 0.8)
    # (its body)
    d.fill(coat, Q([(-7.6, 15.0), (-8.2, 21.6), (-6.8, 28.6), (-3.8, 34.6), (1.0, 37.8), (5.2, 36.0), (6.6, 30.0), (5.6, 23.0),
                    (3.2, 16.8), (-0.6, 13.0), (-5.0, 12.8)]), steps=5)
    d.fill(coat, Q([(-6.8, 18.6), (-7.4, 13.4), (-5.6, 8.6), (-2.6, 5.0), (0.6, 5.6), (3.0, 9.6), (2.4, 14.6), (-1.6, 18.6)]), steps=4)
    # (its near foreleg up to the rung)
    d.limb(coat, Q([(0.6, 32.0), (4.8, 36.6), (7.8, 40.4)]), [2.6 * k, 2.1 * k, 1.9 * k])
    d.oval(coat, *P(8.6, 40.8), 2.8 * k, 1.7 * k, turn=turn + 20 * s, ink=w0 * 0.8)
    # (its head, `head` times its own size about its neck: the skull, the long muzzle, the black nose, the floppy black ear)
    def H(u, v):
        return P(2.0 + (u - 2.0) * head, 40.0 + (v - 40.0) * head)
    kh = k * head
    d.fill(coat, [H(-2.2, 41.0), H(-2.8, 45.2), H(-1.0, 48.6), H(2.6, 50.0), H(5.6, 48.8), H(10.4, 48.0), H(13.0, 46.6), H(12.8, 44.0),
                  H(9.8, 42.8), H(5.4, 42.0), H(2.0, 39.8)], steps=5)
    d.oval("art_ink", *H(12.8, 46.2), 1.25 * kh, 1.05 * kh, ink=0.0)
    d.line("art_ink", [H(12.2, 43.6), H(9.6, 43.0), H(7.2, 43.6)], 0.55 * kh)
    d.oval("art_ink", *H(6.2, 46.4), 0.85 * kh, 1.0 * kh, ink=0.0)
    d.oval("art_white", *H(6.5, 46.8), 0.28 * kh, 0.3 * kh, ink=0.0)
    d.fill(spots, [H(1.8, 47.4), H(-0.6, 47.8), H(-2.6, 45.6), H(-2.6, 41.8), H(-1.2, 40.0), H(0.8, 41.6), H(1.8, 44.4)], ink=w0 * 0.7, steps=4)
    # (its spots: few and bold)
    for (u, v, r) in ((-4.6, 24.0, 1.5), (-1.4, 30.4, 1.1), (2.8, 26.0, 1.0), (-3.6, 11.6, 1.3), (0.6, 8.2, 0.9), (4.2, 37.0, 0.8),
                      (-13.4, 16.2, 0.8), (-0.4, 37.4, 0.9)):
        d.oval(spots, *P(u, v), r * k, r * 1.15 * k, turn=turn, ink=0.0)
    # (its fire helmet)
    kt = k * (1.0 + (head - 1.0) * 0.5)
    fire_helmet(d, _frame(*H(1.8, 48.6), kt, s, turn), kt, paint=helmet)
    return P


# ---- 65: a bighorn ram and its kit bag ---------------------------------------------------------------------------------------
def kit_bag(d, top, bottom, wide, paint="art_khaki", cord="art_white", seam="art_ink"):
    """A canvas kit bag, its gathered neck at `top` (where a hand holds it) and its rounded foot at `bottom`, `wide`
    across: the drawstring neck pulled tight under a puckered top, the cord's loop hanging from it, folds running down
    from the gather, one seam down its length and its round foot."""
    (x0, y0), (x1, y1) = top, bottom
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    nx, ny = -uy, ux
    h = wide / 2

    def B(a, b):                                          # (a along it from its neck, in its lengths; b across, in half widths)
        return (x0 + ux * L * a + nx * h * b, y0 + uy * L * a + ny * h * b)
    w0 = d.ink
    d.fill(paint, [B(0.07, -0.34), B(0.16, -0.62), B(0.3, -0.98), B(0.6, -1.02), B(0.88, -0.98), B(0.99, -0.72), B(1.03, 0.0),
                   B(0.99, 0.72), B(0.88, 0.98), B(0.6, 1.02), B(0.3, 0.98), B(0.16, 0.62), B(0.07, 0.34)], steps=4)
    d.line(seam, [B(0.3, 0.5), B(0.6, 0.56), B(0.93, 0.48)], wide * 0.045)                       # (its seam)
    d.line(seam, [B(0.96, -0.62), B(1.0, 0.0), B(0.96, 0.62)], wide * 0.04)                     # (its round foot's rim)
    for b0, b1 in ((-0.2, -0.62), (0.05, 0.02), (0.22, 0.6)):                                   # (the folds down from its gather)
        d.line(seam, [B(0.1, b0), B(0.2, (b0 + b1) * 0.6), B(0.33, b1)], wide * 0.035)
    d.fill(paint, [B(0.06, -0.36), B(-0.03, -0.5), B(-0.09, -0.62), B(-0.12, -0.36), B(-0.09, -0.16), B(-0.13, 0.04), B(-0.09, 0.22),
                   B(-0.12, 0.46), B(-0.08, 0.64), B(-0.02, 0.5), B(0.06, 0.36)], ink=w0 * 0.85, steps=3)   # (its puckered top)
    for b0 in (-0.36, 0.0, 0.34):
        d.line(seam, [B(0.04, b0 * 0.7), B(-0.07, b0 * 1.15)], wide * 0.03)
    d.line(cord, [B(0.06, -0.42), B(0.06, 0.0), B(0.06, 0.42)], wide * 0.07, taper=False)       # (the drawstring, pulled tight)
    loop = [B(0.06, -0.4), B(0.13, -0.9), B(0.24, -1.12), B(0.31, -0.98), B(0.24, -0.72), B(0.08, -0.44)]
    d.line(cord, loop, wide * 0.055, taper=False)                                                 # (its loop, lying over its shoulder)
    return B


def horn_curl(d, P, k, c, r0, r1, th0, sweep, w0, w1, paint="art_cream", ridge="art_ink", ridges=(0.16, 0.32, 0.48, 0.64)):
    """A ram's horn in frame P: an open spiral about c, from radius r0 at th0 degrees round `sweep` degrees
    (anticlockwise in the frame) in to radius r1, w0 thick at its root and w1 at its tip; ink ridges across it."""
    n = 13

    def at(t):
        th = math.radians(th0 + sweep * t)
        r = r0 + (r1 - r0) * t
        return th, r, (c[0] + r * math.cos(th), c[1] + r * math.sin(th))
    pts, rad = [], []
    for i in range(n):
        t = i / (n - 1)
        pts.append(P(*at(t)[2]))
        rad.append((w0 + (w1 - w0) * t) * k)
    d.limb(paint, pts, rad)
    for t in ridges:
        th, r, _ = at(t)
        w = (w0 + (w1 - w0) * t) * 0.82
        a0 = (c[0] + (r - w) * math.cos(th), c[1] + (r - w) * math.sin(th))
        a1 = (c[0] + (r + w) * math.cos(th + 0.05), c[1] + (r + w) * math.sin(th + 0.05))
        d.line(ridge, [P(*a0), P(*a1)], d.ink * 1.1, taper=True)


def bighorn(d, x, y, size, s=1, coat="art_lbrown", pale="art_white", horn="art_cream", hoof="art_dark", bag="art_khaki"):
    """A bighorn ram walking upright, sturdy, his near hoof's tip at (x, y), `size` from there to the top of his horn,
    facing s, a kit bag slung over his near shoulder (its gathered neck in his near fore hoof, the bag down his back):
    his great open curled horn rooted on his crown above his eye, his pale muzzle. Returns his frame."""
    k = size / 67.0
    P = _frame(x, y, k, s)

    def Q(pts):
        return [P(u, v) for u, v in pts]

    def R(pts, up=4.0):                    # (his head, up on his neck)
        return [P(u, v + up) for u, v in pts]
    w0 = d.ink
    # (his far arm swinging forward, and his far leg back in his stride: both behind him)
    d.limb(coat, Q([(3.0, 41.0), (7.4, 33.4), (12.4, 30.0)]), [3.3 * k, 2.9 * k, 2.5 * k])
    d.fill(hoof, Q([(11.6, 32.6), (15.4, 31.8), (15.6, 28.2), (11.8, 27.8)]), ink=w0 * 0.8, steps=3)
    d.limb(coat, Q([(-1.8, 25.0), (-4.8, 14.0), (-8.4, 5.0)]), [4.8 * k, 3.4 * k, 2.6 * k])
    d.fill(hoof, Q([(-11.4, 5.8), (-6.0, 5.2), (-6.6, 0.4), (-12.2, 1.0)]), ink=w0 * 0.8, steps=3)
    # (his near leg, forward in his stride)
    d.limb(coat, Q([(3.6, 25.0), (6.6, 14.2), (4.6, 4.4)]), [5.4 * k, 3.7 * k, 2.8 * k])
    d.fill(hoof, Q([(1.4, 5.4), (7.6, 5.4), (8.4, 0.0), (0.8, 0.0)]), ink=w0 * 0.8, steps=3)
    # (his body, a deep barrel, and his thick neck)
    d.fill(coat, Q([(-8.2, 22.6), (-10.4, 30.0), (-10.0, 38.0), (-7.2, 44.6), (-1.2, 48.8), (5.6, 48.4), (10.6, 44.0), (12.0, 36.0),
                    (11.2, 28.0), (7.2, 21.8), (-0.8, 20.2)]), steps=5)
    d.fill(coat, Q([(0.2, 44.0), (2.4, 53.4), (5.4, 58.6), (10.6, 59.2), (14.0, 52.8), (11.0, 41.4)]), steps=4)
    # (the kit bag, its neck at his near hoof before his shoulder, its body down his back)
    kit_bag(d, P(11.6, 46.1), P(-16.4, 27.0), 12.4 * k, paint=bag)
    # (his head: a long face, a Roman nose, the pale muzzle; his eye under its heavy brow)
    d.fill(coat, R([(5.0, 51.0), (6.0, 56.4), (9.4, 59.0), (14.0, 58.4), (18.4, 55.2), (21.2, 51.4), (20.8, 48.6), (17.6, 47.4),
                    (12.4, 47.8), (8.0, 47.6)]), steps=5)
    d.fill(pale, R([(16.0, 54.8), (19.4, 53.4), (21.2, 51.4), (20.8, 48.6), (17.6, 47.4), (15.0, 48.6), (14.6, 51.8)]), ink=0.0, steps=4)
    d.line("art_ink", R([(19.4, 51.8), (20.2, 51.2)]), 0.5 * k, taper=False)
    d.line("art_ink", R([(17.0, 48.8), (18.8, 48.6), (20.2, 49.4)]), 0.45 * k)
    d.oval("art_white", *R([(13.4, 54.6)])[0], 1.5 * k, 1.25 * k, ink=w0 * 0.7)
    d.oval("art_ink", *R([(13.9, 54.4)])[0], 0.7 * k, 0.8 * k, ink=0.0)
    d.line("art_ink", R([(11.6, 56.4), (13.4, 57.0), (15.2, 56.4)]), 0.5 * k)
    # (his great horn: rooted thick on his crown above his eye, up and back, down round behind his cheek and forward,
    #  its tip curling in inside the outer coil with a gap)
    d.oval(coat, *P(5.0, 58.4), 5.3 * k, 5.3 * k, ink=0.0)                                   # (the side of his head, seen through the curl)
    horn_curl(d, P, k, (5.0, 58.4), 8.4, 2.4, 38.0, 328.0, 3.4, 0.75, paint=horn)
    # (his near arm: the upper arm down and forward to his elbow, the forearm up to his hoof on the bag's neck)
    d.limb(coat, Q([(1.0, 44.0), (5.6, 39.6), (10.0, 36.0)]), [3.5 * k, 3.1 * k, 2.8 * k])
    d.limb(coat, Q([(10.4, 35.8), (11.2, 39.6), (10.4, 43.0)]), [2.8 * k, 2.6 * k, 2.4 * k])
    d.fill(hoof, Q([(7.8, 42.4), (8.0, 46.6), (12.6, 46.6), (12.8, 42.2)]), ink=w0 * 0.8, steps=3)
    return P


# ---- 66: a wishbone -------------------------------------------------------------------------------------------------------
def wishbone(d, x, y, size, turn=0.0, paint="art_cream"):
    """A wishbone held up by its joint: the little flat knob under the joint at (x, y), its two thin arms bowed up and
    out from the joint into a wide V `size` tall, their ends a little flared."""
    P = _frame(x, y, size / 22.0, 1, turn)
    k = size / 22.0
    for sx in (-1, 1):
        d.limb(paint, [P(sx * 0.2, 2.0), P(sx * 2.4, 4.4), P(sx * 5.0, 9.0), P(sx * 7.4, 14.2), P(sx * 9.0, 18.8), P(sx * 9.6, 21.4)],
               [1.3 * k, 1.05 * k, 0.95 * k, 0.95 * k, 1.05 * k, 1.4 * k])
    d.fill(paint, [P(-1.6, 2.4), P(-1.2, 0.6), P(0.0, -0.4), P(1.2, 0.6), P(1.6, 2.4), P(0.0, 3.2)], ink=d.ink * 0.8, steps=3)
    d.line("art_white", [P(-2.4, 5.6), P(-4.6, 9.8), P(-6.6, 14.4)], 0.5 * k)


# ---- the paintings --------------------------------------------------------------------------------------------------------
@painting(60)
def p60(d, a):   # a gunner standing waist deep in spent shell cases
    F = man(d, 26, -20, 78, 1, pose="stand", arms=("up_out", "hip"),
            wear={"shirt": "art_khaki", "jacket": "art_lbrown", "cut": "flight", "fur": "art_cream", "knit": "art_brown",
                  "trousers": "art_olive", "helmet": "art_brown"},
            hat="flying", legs_shown=False, mouth="grin")
    for base, ang, tilt in (((16.0, 18.0), 148.0, 12.0), ((36.0, 18.0), 30.0, 16.0)):     # (behind its top edge, poking past it)
        brass_case(d, base, ang, 22.0, 2.95, tilt)
    brass_mound(d, 26, 0, 29, 29)
    brass_case(d, (37.0, 21.5), 192.0, 22.0, 2.95, 14.0)                                     # (lying across it)
    for base, ang in (((10.0, 16.0), 256.0), ((27.0, 14.2), 276.0), ((43.0, 17.0), 298.0)):   # (pointing out at us)
        brass_case(d, base, ang, 21.0, 3.1, 74.0)
    brass_case(d, (-5.4, 1.0), 4.0, 22.0, 2.95, 10.0)                                        # (loose at its foot)
    brass_case(d, (40.6, 2.0), 352.0, 21.0, 2.95, 44.0)
    (hx, hy), _ = F.hand["fore"]
    brass_case(d, (hx - 1.3, hy - 4.6), 68.0, 12.5, 2.1, tilt=25.0)
    F.grip("fore")
    names(d, a, (96, 34), 14, lines=["Waist", "Deep in", "Brass"], gap=1.38)


@painting(61)
def p61(d, a):   # a blue crab in a sailor cap with a wrench in each claw
    k = 1.04
    P, grips = crab(d, 50, 58, k)
    for sx, (gx, gy) in zip((1, -1), grips):
        ux, uy = sx * math.cos(math.radians(30)), math.sin(math.radians(30))
        open_wrench(d, gx - ux * 7.0, gy - uy * 7.0, gx + ux * 17.0, gy + uy * 17.0, 3.0)
        crab_finger(d, P, sx, "art_sky", k)
    names(d, a, (50, 20), 14, lines=split2(a["name"]))


@painting(62)
def p62(d, a):   # a prize pumpkin bigger than the wagon that carries it
    pumpkin(d, 30, 43.0, 27.5, 28.0)
    farm_wagon(d, 30, 0, 34, s=-1)
    rosette(d, 45, 42.0, 5.8)
    names(d, a, (103.5, 42), 15, lines=split2(a["name"]))


def possum_curled(d, x, y, size, fur="art_grey", grizzle="art_lgrey", face="art_white", pink="art_pink", ears="art_black",
                  tips="art_lgrey"):
    """A plain opossum from life, curled up asleep lying on top of something, its belly at (x, y) on it, `size` long
    from rump to nose, its head at the front (+x): one round grey mound with a pale shaggy band of guard hairs along its
    back, its white head tucked down on its small pink hands, the long snout and pink nose, a thin black leaf of an
    ear with a pale tip, its eye shut. Returns where its tail leaves its rump."""
    k = size / 40.0

    def P(u, v):
        return (x + u * k, y + v * k)

    def Q(pts):
        return [P(u, v) for u, v in pts]
    w0 = d.ink
    # (its hind foot, behind, gripping)
    d.oval(pink, *P(-14.0, 1.2), 2.2 * k, 1.4 * k, ink=w0 * 0.7)
    # (its body: one round mound, the rump high at the back, the shoulders lower at the front)
    body = [(-19.0, 1.0), (-20.4, 6.0), (-19.2, 11.6), (-15.4, 16.2), (-9.6, 19.0), (-3.0, 19.6), (3.4, 18.0), (8.6, 14.4),
            (11.6, 9.6), (12.4, 4.6), (11.0, 0.6), (2.0, -0.6), (-9.0, -0.6)]
    d.fill(fur, Q(body), steps=5)
    # (the grizzle: one pale band of long guard hairs along its back, its top edge breaking out in shaggy tufts)
    top = [(-18.6, 9.0), (-17.6, 13.6), (-19.4, 15.4), (-14.6, 16.2), (-12.4, 19.2), (-9.4, 18.8), (-6.0, 21.6), (-3.0, 20.2),
           (0.6, 21.4), (3.0, 18.6), (6.8, 18.6), (8.4, 14.6), (7.2, 13.0), (3.0, 15.0), (-2.4, 16.0), (-8.0, 15.4), (-13.0, 12.8), (-16.2, 9.4)]
    d.fill(grizzle, Q(top), ink=w0 * 0.8, steps=3)
    # (its far ear, behind the head)
    d.fill(ears, Q([(10.6, 12.6), (9.6, 16.2), (10.4, 18.2), (12.4, 17.0), (13.2, 13.6)]), ink=w0 * 0.8, steps=4)
    # (its head, white, tucked down at the front on its hands: the long snout down to the pink nose)
    d.fill(face, Q([(7.0, 10.6), (8.6, 13.4), (11.6, 14.4), (14.6, 13.0), (17.6, 9.8), (20.6, 6.0), (22.6, 3.4), (22.4, 1.8),
                    (19.6, 1.6), (15.4, 2.2), (10.6, 3.2), (7.4, 5.6)]), steps=5)
    d.oval(pink, *P(22.6, 2.6), 1.6 * k, 1.25 * k, turn=-30, ink=w0 * 0.7)
    d.line("art_ink", Q([(21.0, 1.9), (18.6, 2.6), (16.6, 3.2)]), 0.4 * k)         # (its mouth, content)
    # (its eye, shut: a long lid with three short lashes down)
    lid = [(12.0, 9.2), (13.8, 8.6), (15.6, 9.2)]
    d.line("art_ink", Q(lid), 0.6 * k, taper=False)
    for u0 in (12.6, 13.8, 15.0):
        d.line("art_ink", Q([(u0, 8.75 if u0 != 13.8 else 8.55), (u0 - 0.3, 7.5)]), 0.34 * k, taper=False)
    # (its near ear: thin, rounded, a leaf lying back off its crown, black with a pale tip)
    d.fill(ears, Q([(9.2, 12.4), (6.6, 15.0), (4.4, 17.8), (5.6, 19.0), (8.6, 17.8), (10.6, 14.4)]), ink=w0 * 0.8, steps=4)
    d.fill(tips, Q([(5.0, 17.0), (4.4, 17.8), (5.6, 19.0), (6.8, 18.6), (6.0, 17.6)]), ink=w0 * 0.5, steps=3)
    # (its small pink hands under its chin, the fingers spread on what it lies on)
    for cx in (12.6, 17.0):
        for f in (-40, -10, 20):
            t = math.radians(-90 + f)
            d.limb(pink, Q([(cx, 0.9), (cx + 1.8 * math.cos(t), 0.9 + 1.8 * math.sin(t))]), [0.42 * k, 0.3 * k], ink=w0 * 0.6)
        d.oval(pink, *P(cx, 1.0), 1.1 * k, 0.9 * k, ink=w0 * 0.7)
    return P(-19.6, 4.0)


def snooze(d, x, y, h, paint="art_white"):
    """One small sleeper's z, brushed as a shape (not a letter): top bar, the diagonal back, the foot bar."""
    d.line(paint, [(x - 0.45 * h, y + 0.5 * h), (x + 0.45 * h, y + 0.5 * h), (x - 0.45 * h, y - 0.5 * h), (x + 0.5 * h, y - 0.5 * h)], 0.24 * h,
           steps=1, taper=False)


@painting(63)
def p63(d, a):   # an opossum curled up asleep riding on top of a bomb, its tail wrapped round it, one small z
    long, by = 58.0, 26.0
    r = long * 0.17
    # (the tail's run under the bomb, behind it)
    d.limb("art_pink", [(12.0, by - r * 0.6), (14.0, by - r - 1.2), (21.0, by - r - 1.4), (27.0, by - r + 1.0)], [1.5, 1.45, 1.4, 1.3])
    B = bomb(d, 62, by, long, turn=0)
    # the possum curled on top of it, its belly on the bomb's back
    tail = possum_curled(d, 34, by + r - 0.8, 48)
    # (its tail: black at the root, then bare and pink, down over the bomb's near side and under it, wrapped round)
    d.limb("art_black", [tail, (tail[0] - 2.4, tail[1] - 2.2), (tail[0] - 3.0, tail[1] - 5.4)], [2.0, 1.95, 1.9])
    d.limb("art_pink", [(tail[0] - 3.0, tail[1] - 5.2), (tail[0] - 2.6, by + 2.0), (tail[0] - 0.4, by - 4.2), (tail[0] + 3.4, by - r + 0.6),
                        (tail[0] + 7.6, by - r - 0.6)], [1.85, 1.75, 1.6, 1.45, 1.3])
    snooze(d, 52.0, 63.0, 6.0)
    names(d, a, (110, 40), 14, lines=split2(a["name"]))


@painting(64)
def p64(d, a):   # a Dalmatian in a fire helmet climbing a ladder into the clouds
    lean, size = 16.0, 54.0
    k = size / 50.0
    ux, uy = -math.sin(math.radians(lean)), math.cos(math.radians(lean))
    base, sp, L = (48.0, -2.0), 8.0 * k, 80.0
    ladder(d, base, (base[0] + ux * L, base[1] + uy * L), 14.0 * k, [sp * (i + 0.5) for i in range(int(L / sp))], w=1.9 * k)
    t0 = sp * 1.5
    paw = (base[0] + ux * t0 + uy * 3.0 * k, base[1] + uy * t0 - ux * 3.0 * k)
    dalmatian(d, paw[0], paw[1], size, s=-1, turn=lean, head=1.2)
    cloud(d, 22, 80, 36, 17, seed=64, flat=False, shade=None)
    names(d, a, (106, 41), 14, lines=split2(a["name"]), indent=6)


@painting(65)
def p65(d, a):   # a bighorn ram with a kit bag over its shoulder
    bighorn(d, 22, 2, 84, 1)
    names(d, a, (90.5, 44), 14, lines=a["name"].split(), gap=1.36)


@painting(66)
def p66(d, a):   # a woman in a cook's apron holding up a wishbone
    F = lady(d, 22, 2, 72, 1, pose="stand", arms=("wave", "hip"),
             wear={"top": "art_lred", "skirt": "art_lred", "apron": "art_white", "sleeves": "puff", "shoes": "art_red", "flare": 0.3},
             hair="rolls", hair_paint="art_blond")
    (hx, hy), _ = F.hand["fore"]
    wishbone(d, hx + 0.2, hy + 0.6, 23, turn=-6)
    d.oval("art_skin", hx, hy, 0.155 * F.k, 0.13 * F.k, ink=d.ink * 0.7)
    twinkle(d, hx + 15.4, hy + 25.0, 4.2, "art_yellow")
    names(d, a, (98, 38), 14, lines=split2(a["name"]))
