"""Kaga: the Imperial Navy's carrier on the hull of a battleship, as she was at Pearl Harbor (December 1941) and Midway
(June 1942), after her rebuilding at Sasebo in 1933-35.

Our own model, made from numbers: her published dimensions after the rebuild (247.65 m overall, 240.3 m on the
waterline, 32.5 m over the second pair of bulges, 9.48 m draught; one flight deck of 248.6 m by 30.5 over two hangars;
three lifts, 11.5 by 12 m forward, 15.9 by 10.7 amidships (its 15.9 m fore and aft), 12.8 by 9.9 aft), and the layout of
the ship as photographs of her from 1936 to 1941 show it, measured off them by a ruler laid along her flight deck: the
small island well forward on her STARBOARD side (about a third of her length from the bow), her one great funnel led
out through her starboard side amidships and turned down toward the sea (the long trunked funnels of 1928 were gone),
the flight deck narrowing over her open forecastle on two pairs of posts and overhanging her lengthened stern (a round
cruiser counter) on tall braced posts, her flight deck some 21 m over the water. Her guns: ten 20 cm/50 in casemates in
her after hull, five a side (the three of 1928 low down, the two from her old turrets a deck higher and further forward:
`sec.P1-5`, `sec.S1-5`), eight twin 12.7 cm/40 high-angle mountings in their shields, one forward and three aft on each
side, on sponsons raised in the rebuilding to the flight deck's level so that the guns stand above it and could fire
across it (`sec.P6-9`, `sec.S6-9`), their two Type 91 directors likewise (`director.sec.port`, `director.sec.starboard`),
eleven twin 25 mm (`aa25.1-11`) with the six Type 95 directors that laid them (`director.aa.1-6`), six 6.5 mm machine
guns at the galleries' rails, all of them manned. Amidships each side two boats hang on davits in recesses in the lower
hangar's wall. Nothing is copied from any drawing, plan or model; where the photographs could not settle a place (the
light guns' and their directors' stations, the searchlights, the radio masts, the boat recesses) it is our own
reckoning.
"""

import math
import random

from mathutils import Matrix, Vector
from mathutils.geometry import tessellate_polygon

import capitals as cap
import fittings as fit
from rigid import PORT, STBD, X, Y, Z, Model, lerp, smooth
from shipyard import Yard, livery, weather_for


# ---- her hull: the battleship's, lengthened aft, with the second pair of bulges of 1934-35 ------------------------

# the hull's deck edge (half widths as shares of her old 29.6 m beam): a fine battleship's bow, a long parallel body,
# a full round stern
KAGA_PLAN = [(-123.8, 0.0), (-123.65, 0.1), (-123.3, 0.19), (-122.7, 0.28), (-121.8, 0.38), (-120.4, 0.48), (-118.4, 0.58), (-115.6, 0.68),
             (-112.0, 0.77), (-107.5, 0.845), (-100.0, 0.92), (-88.0, 0.98), (-75.0, 1.0), (45.0, 1.0), (58.0, 0.975), (70.0, 0.93), (82.0, 0.85), (92.0, 0.76), (101.0, 0.65), (108.0, 0.54), (114.0, 0.42),
             (118.5, 0.31), (121.5, 0.2), (123.2, 0.1), (123.8, 0.0)]
# her upper deck's line: low and nearly level, rising to the bow
KAGA_DECK = [(-123.8, 9.1), (-90.0, 9.15), (0.0, 9.3), (50.0, 9.45), (85.0, 9.8), (105.0, 10.4), (118.0, 11.1), (123.8, 11.5)]

# the flight deck's edges, aft to fore: 248.5 m, 30.5 wide over most of her, narrowing to 16 m over the forecastle and
# squared off there; its after end broad, its corners well rounded
KAGA_PORT = [(-125.5, 5.0), (-125.3, 8.6), (-124.6, 11.4), (-123.3, 13.4), (-121.4, 14.6), (-118.5, 15.15), (-114.0, 15.25), (52.0, 15.25),
             (62.0, 14.75), (72.0, 13.9), (84.0, 12.5), (96.0, 11.0), (107.0, 9.7), (116.0, 8.6), (121.4, 8.0), (122.6, 7.3), (123.0, 5.8)]
KAGA_STAR = [(x, -y) for x, y in KAGA_PORT]
DECK_FORE, DECK_AFT = 123.0, -125.5

FD, IZ = 20.1, 20.8                # the flight deck's underside and its top
ZG = 18.3                          # the galleries along her sides under the deck's edge
ZH = IZ - 0.55                     # the 12.7 cm guns' sponsons, raised to the flight deck's level in 1934-35
HX0, HX1 = 99.0, -89.0             # the hangars' fore and after walls
BX = -16.0                         # where her back breaks

# Imperial Navy grey; her flight deck of plain wood
KAGA_PAINT = {"planks": "B59C72", "flight": "B59C72", "plane": "3E4A3C"}


class KagaHull(cap.PlanHull):
    """Her hull laid to her plan, with the bulges of 1934-35 swelling her waterline from the 29.6 m of her side above
    to 32.5 m; the bulge rounds in to her upright side within the lower part of her freeboard."""

    bulge = 2.03

    def swell(self, u):
        return smooth((u + 0.86) / 0.24) * smooth((0.74 - u) / 0.3)

    def half_water(self, u):
        return super().half_water(u) + self.bulge * self.swell(u)

    def point(self, u, where, hand=PORT):
        p = super().point(u, where, hand)
        if where > 0.0:
            s = self.swell(u)
            if s > 0.0:
                # (the parent's side runs from the bulged waterline straight up to the deck; take the bulge back off
                #  it above, so that her side stands upright over the bulge's shoulder)
                base = cap.PlanHull.half_water(self, u)
                hd = max(self.half_deck(u), 0.12)
                plain = base + (hd - base) * where
                k = 1.0 - smooth(where / 0.42)
                p = Vector((p.x, hand * (plain + self.bulge * s * k), p.z))
        return p


class Closer:
    """A hull's frames drawn closer together toward her stern: the yard lays its frames evenly along u, and so few of
    them fall on the round of her counter that it shows as a box. This stands in for her hull with u bent, so that
    the frames crowd in over her last 24 m (a third of the spacing at the very end) and run evenly from there forward.
    Everything that takes a place along her (deck heights, her side at x) is her hull's own; everything that takes u
    is read through the bend, so that a frame and its place along her still agree."""

    T0, A = 0.1, 0.86            # (the bent stretch, as a share of her length from the stern; how hard it is bent)

    def __init__(self, inner):
        self.inner = inner

    def __getattr__(self, k):
        return getattr(self.inner, k)

    def bend(self, u):
        t = (u + 1.0) * 0.5
        if t >= self.T0:
            return u
        s = t / self.T0
        return -1.0 + 2.0 * self.T0 * (s - self.A * s * (1.0 - s) ** 2)

    def unbend(self, ul):
        if (ul + 1.0) * 0.5 >= self.T0:
            return ul
        lo, hi = -1.0, -1.0 + 2.0 * self.T0
        for _ in range(40):
            mid = (lo + hi) * 0.5
            lo, hi = (mid, hi) if self.bend(mid) < ul else (lo, mid)
        return (lo + hi) * 0.5

    def frame_x(self, u):
        return self.inner.frame_x(self.bend(u))

    def u_of(self, x):
        return self.unbend(self.inner.u_of(x))

    def lean(self, u):
        return self.inner.lean(self.bend(u))

    def beam(self, u):
        return self.inner.beam(self.bend(u))

    def half_deck(self, u):
        return self.inner.half_deck(self.bend(u))

    def half_water(self, u):
        return self.inner.half_water(self.bend(u))

    def keel(self, u):
        return self.inner.keel(self.bend(u))

    def point(self, u, where, hand=PORT):
        p = self.inner.point(self.bend(u), where, hand)
        if u <= -1.0 + 1e-9:
            # (her hull's last frame is her sternpost: a line, not the narrow strip the yard gives a frame of no width,
            #  which it closes with a plate that shows as a dark seam down her stern)
            p.y *= 0.05
        return p


# ---- small helpers -------------------------------------------------------------------------------------------------

def ccw(plan):
    """A closed plan turned to go round anticlockwise seen from above."""
    a = sum(plan[i][0] * plan[(i + 1) % len(plan)][1] - plan[(i + 1) % len(plan)][0] * plan[i][1] for i in range(len(plan)))
    return plan if a > 0 else list(reversed(plan))


def outboard_rect(xf, xa, y_in, y_out, r, steps=3):
    """A plan built out from a straight edge at y_in to y_out: its outer corners rounded, its inner ones square."""
    s = 1.0 if y_out > y_in else -1.0
    pts = [(xf, y_in)]
    for k in range(steps + 1):                      # (the fore outer corner)
        a = math.pi * 0.5 * k / steps
        pts.append((xf - r + math.cos(a) * r, y_out - s * r + s * math.sin(a) * r))
    for k in range(steps + 1):                      # (the after outer corner)
        a = math.pi * 0.5 + math.pi * 0.5 * k / steps
        pts.append((xa + r + math.cos(a) * r, y_out - s * r + s * math.sin(a) * r))
    pts.append((xa, y_in))
    out = []
    for p in pts:
        if not out or abs(out[-1][0] - p[0]) > 1e-6 or abs(out[-1][1] - p[1]) > 1e-6:
            out.append(p)
    return ccw(out)


def round_front(xf, xa, y0, y1, r_back=0.6, steps=6):
    """A plan whose fore end is a half round across its whole width and whose after corners are rounded a little."""
    hw, yc = (y1 - y0) * 0.5, (y0 + y1) * 0.5
    cx = xf - hw
    pts = [(cx + math.cos(a) * hw, yc + math.sin(a) * hw) for a in [-math.pi / 2 + math.pi * k / steps for k in range(steps + 1)]]
    for q, (ccx, ccy) in enumerate(((xa + r_back, y1 - r_back), (xa + r_back, y0 + r_back))):
        for k in range(3):
            a = math.pi / 2 + (q + k / 2.0) * math.pi / 2
            pts.append((ccx + math.cos(a) * r_back, ccy + math.sin(a) * r_back))
    out = []
    for p in pts:
        if not out or abs(out[-1][0] - p[0]) > 1e-6 or abs(out[-1][1] - p[1]) > 1e-6:
            out.append(p)
    return ccw(out)


def plate_top(m, plan, z, paint, down=False):
    """A flat plate over a closed plan at height z (cut into triangles that follow its bays)."""
    pts = [Vector((px, py, z)) for px, py in plan]
    tris = tessellate_polygon([pts])
    m.add(pts, [tuple(t) for t in tris], paint, False, (lambda q: -Z) if down else (lambda q: Z))


def railing_arc(m, c, r, a0, a1, z, every=1.0):
    from ships import railing
    n = max(2, int(abs(a1 - a0) * r / every) + 1)
    railing(m, [Vector((c.x + math.cos(lerp(a0, a1, i / (n - 1))) * r, c.y + math.sin(lerp(a0, a1, i / (n - 1))) * r, z)) for i in range(n)], high=1.0, every=every)


def gun_sponson(yard, x, y, z, r, wall_y, paint="upper", rail=True):
    """A round platform built out from her side at height z on knees, with a lip round its edge and (fine) a rail round
    its outer half: where a gun or a searchlight stands below the flight deck's edge."""
    m = yard.on(x)
    fine = m.fine
    hand = 1.0 if y >= wall_y else -1.0
    # (its inner half reaches back into her side, so that it is built out from the wall and not hung off it)
    m.lathe((x, y, z - 0.25), Z, [(0.0, r), (0.25, r)], paint, 24 if fine else 8)
    m.lathe((x, y, z - 0.25), Z, [(0.0, r * 0.97), (0.0, r * 0.2)], paint, 24 if fine else 8, caps=(False, False))
    if fine:
        m.lathe((x, y, z), Z, [(0.0, r + 0.02), (0.12, r + 0.02)], paint, 24, caps=(False, False))       # its lip
        for dx in (-r * 0.62, 0.0, r * 0.62):                                                           # the knees under it
            yo = y + hand * (math.sqrt(max(r * r - dx * dx, 0.0)) - 0.35)
            m.rod((x + dx, wall_y, z - 2.1), (x + dx, yo, z - 0.22), 0.09, paint, sides=4)
        if rail:
            c = Vector((x, y, z))
            a_mid = math.pi * 0.5 if hand > 0 else -math.pi * 0.5
            railing_arc(m, c, r - 0.08, a_mid - math.radians(70), a_mid + math.radians(70), z, every=1.1)


# ---- her guns ------------------------------------------------------------------------------------------------------

def casemate20(yard, name, x, z, hand, limits=(-55, 55)):
    """A 20 cm/50 of the 3rd Year Type in its casemate in her after hull: an armoured frame standing proud of her
    plating round a dark port, the curved shield on the mounting closing the port, the long barrel out through it with
    its muzzle swell. A `sec.*` turret (its bearing clockwise: -90 to port): the shield and gun train across the port,
    the gun lifts and runs back."""
    m, h = yard.m, yard.h
    fine = m.fine
    bearing = -90.0 if hand == PORT else 90.0
    half = yard.half(x)
    m.use(half)
    fx, lo, hi = 1.95, z - 1.15, z + 1.2
    # (the plating bulges and rounds in over the port's height: the frame stands out from the furthest of it)
    far = max(abs(h.side(x + dx, zz, hand)[0].y) for dx in (-fx - 0.4, 0.0, fx + 0.4) for zz in (lo - 0.35, z, hi + 0.35))
    near = min(abs(h.side(x + dx, zz, hand)[0].y) for dx in (-fx - 0.4, 0.0, fx + 0.4) for zz in (lo - 0.35, z, hi + 0.35))
    y_black = far + 0.03
    y_face = y_black + 0.34
    y_root = near - 0.25
    out = Vector((0.0, hand, 0.0))
    # the port: a dark opening behind the frame's face
    m.add([Vector((x - fx, hand * y_black, lo)), Vector((x + fx, hand * y_black, lo)), Vector((x + fx, hand * y_black, hi)), Vector((x - fx, hand * y_black, hi))],
          [(0, 1, 2, 3)], "black", False, lambda q, o=out: o)
    # the armoured frame round it, standing out from her side
    deep = y_face - y_root
    yc = hand * (y_root + deep * 0.5)
    m.box((x, yc, hi + 0.2), (fx * 2 + 0.8, deep, 0.4), "hull")
    m.box((x, yc, lo - 0.2), (fx * 2 + 0.8, deep, 0.4), "hull")
    for s in (-1, 1):
        m.box((x + s * (fx + 0.2), yc, z + 0.025), (0.4, deep, hi - lo), "hull")
    if fine:
        # an eyebrow over it to throw the water clear, and the plates' seams on the frame
        m.box((x, hand * (y_face + 0.18), hi + 0.48), (fx * 2 + 1.4, 0.55, 0.08), "hull")
        for s in (-1, 1):
            m.box((x + s * (fx + 0.42), hand * (y_face + 0.003), z + 0.025), (0.012, 0.01, hi - lo + 0.8), "seam")
    piv = Vector((x, hand * (y_face - 1.25), z))
    tn = m.part(name, half, tuple(piv), axis=(0, 0, -1), limits=list(limits), kind="turn",
                note=f"a 20 cm casemate gun: trains across its port (plus clockwise seen from above); rest bearing {int(bearing)}; about 6 degrees a second")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 6.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, lat = rot @ X, rot @ Y
    rows = []
    for t in (-1.0, -0.66, -0.33, 0.0, 0.33, 0.66, 1.0) if fine else (-1.0, 0.0, 1.0):
        c = piv + f * (1.17 - 0.2 * t * t) + lat * (t * 1.7)
        rows.append([c - Z * 1.05, c + Z * 1.12])
    m.sheet(rows, "hull", f, smooth=True)
    m.sheet([[p_ - f * 0.07 for p_ in r] for r in rows], "hull", -f, smooth=True)
    if fine:
        m.sheet([[r[1], r[1] - f * 0.95] for r in rows], "hull", Z, smooth=True)          # the shield's roof
        m.box(piv + f * 1.06 + lat * 1.05 + Z * 0.78, (0.12, 0.34, 0.11), "black", yaw=-bearing)   # the layer's sighting port
        m.box(piv + f * 1.14 + Z * 0.05, (0.08, 0.62, 0.62), "black", yaw=-bearing)        # where the gun goes through
        for t in (-0.66, 0.66):                                                            # the shield's plates' seams
            c = piv + f * (1.17 - 0.2 * t * t + 0.004) + lat * (t * 1.7)
            m.rod(c - Z * 1.0, c + Z * 1.08, 0.012, "seam", sides=3, smooth=False)
    gp = piv + f * 0.45 + Z * 0.05
    gn = m.part(f"{name}.gun.1", tn, tuple(gp), axis=tuple(-lat), limits=[-5, 25], slide=tuple(-f), slide_limits=[0.0, 0.9], kind="turn",
                note="the gun: plus lifts the muzzle; runs back when it fires")
    m.use(gn)
    e = math.radians(2.0)
    g = (f * math.cos(e) + Z * math.sin(e)).normalized()
    m.lathe(gp - g * 1.3, g, [(0.0, 0.36), (1.7, 0.35), (2.1, 0.28), (3.6, 0.25), (6.4, 0.2), (8.9, 0.165), (9.1, 0.185), (9.35, 0.18)],
            "hull", 14 if fine else 6, caps=(True, False))
    tip = gp + g * 8.05
    m.disc(tip + g * 0.004, g, 0.1, "black", 10 if fine else 6)
    m.socket(f"muzzle.{name}.1", tip, g, gn, "flash and smoke leave here")
    m.use(half)
    return tn, [gn]


def twin127(yard, name, x, y, z, bearing, limits, crew=True):
    """The twin 12.7 cm/40 high-angle mounting of the Type 89 in its shield: a fixed ring on the sponson, the turning
    floor, the shield (a rounded front with the guns' slots, flat sides, a roof, open behind for the loaders), the two
    guns in one cradle with their sleeves, recoil cylinders and breeches; the layer and trainer seated either side at
    their handwheels, two loaders at the breeches, a man bringing up rounds. A `sec.*` turret (bearing clockwise, -90
    to port): parts, sockets and clip as the yard's."""
    from ships import bulwark
    m = yard.m
    half = yard.half(x)
    fine = m.fine
    m.use(half)
    o = Vector((x, y, z))
    m.lathe(o, Z, [(0.0, 1.85), (0.3, 1.8)], "upper", 20 if fine else 8)
    piv0 = o + Z * 0.3
    tn = m.part(name, half, tuple(piv0), axis=(0, 0, -1), limits=list(limits), kind="turn",
                note=f"a twin 12.7 cm high-angle mounting: plus trains it clockwise seen from above; rest bearing {int(bearing)}; about 16 degrees a second")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 16.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y

    def W(px, py, pz):
        return piv0 + f * px + l * py + Z * pz
    m.lathe(W(0, 0, 0), Z, [(0.0, 2.1), (0.1, 2.1)], "deck", 24 if fine else 8)            # the turning floor
    m.lathe(W(0.25, 0, 0.1), Z, [(0.0, 0.55), (0.6, 0.45), (1.0, 0.4)], "upper", 12 if fine else 6)   # the pedestal
    # the shield
    line = [(-1.15, 1.72), (0.5, 1.72), (1.3, 1.38), (1.72, 0.7), (1.85, 0.0), (1.72, -0.7), (1.3, -1.38), (0.5, -1.72), (-1.15, -1.72)]
    wpts = [W(px, py, 0) for px, py in line]
    zt = piv0.z + 2.5
    bulwark(m, [(p.x, p.y) for p in wpts], piv0.z + 0.1, zt, 0.05, "upper")
    roof = ccw([(p.x, p.y) for p in wpts])
    plate_top(m, roof, zt + 0.06, "roof")
    plate_top(m, roof, zt, "upper", down=True)
    if fine:
        # the roof's edge, its sighting hoods, the slots for the guns in its face, a ventilator
        n_ = len(wpts)
        for i in range(n_ - 1):
            a, b = wpts[i], wpts[i + 1]
            d = b - a
            on = Vector((d.y, -d.x, 0)).normalized()
            if on.dot(a - piv0) < 0:
                on = -on
            q = [Vector((a.x, a.y, zt)), Vector((b.x, b.y, zt)), Vector((b.x, b.y, zt + 0.06)), Vector((a.x, a.y, zt + 0.06))]
            m.add(q, [(0, 1, 2, 3)], "upper", False, lambda _, w=on: w)
        for s in (1, -1):
            m.box(W(0.9, s * 1.15, 2.55 + 0.17), (0.6, 0.38, 0.3), "upper", yaw=-bearing)
            m.disc(W(1.205, s * 1.15, 2.74), f, 0.08, "glass", 8)
            # the slots in the face where the guns pass: a dark recess in its front
            yl, yh = s * 0.27, s * 0.63
            xl, xh = 1.85 - 0.13 * abs(yl) / 0.7 + 0.012, 1.85 - 0.13 * abs(yh) / 0.7 + 0.012
            pts = [W(xl, yl, 0.95), W(xh, yh, 0.95), W(xh, yh, 2.35), W(xl, yl, 2.35)]
            m.add(pts, [(0, 1, 2, 3)], "black", False, lambda _, w=f: w)
        m.lathe(W(-0.6, 0.0, 2.56), Z, [(0.0, 0.2), (0.28, 0.17), (0.34, 0.24)], "upper", 8)
    # the guns
    guns = []
    for k, s in enumerate((1, -1)):
        gp = W(0.3, s * 0.45, 1.5)
        gn = m.part(f"{name}.gun.{k + 1}", tn, tuple(gp), axis=tuple(-l), limits=[-8, 90], slide=tuple(-f), slide_limits=[0.0, 0.45], kind="turn",
                    note="a gun: plus lifts the muzzle; slides back along its own length when it fires")
        m.use(gn)
        e = math.radians(10.0)
        g = (f * math.cos(e) + Z * math.sin(e)).normalized()
        upg = g.cross(l).normalized()
        m.rod(gp - g * 0.7, gp + g * 1.45, 0.18, "upper", sides=12 if fine else 6)                     # the cradle's sleeve
        m.lathe(gp + g * 1.45, g, [(0.0, 0.12), (1.3, 0.105), (3.6, 0.085), (3.72, 0.095), (3.85, 0.09)], "upper", 10 if fine else 6, caps=(False, True))
        tip = gp + g * 5.3
        m.disc(tip + g * 0.004, g, 0.05, "black", 8)
        m.box(gp - g * 0.95, (0.55, 0.36, 0.42), "upper", yaw=-bearing, pitch=10.0)                   # the breech
        if fine:
            m.rod(gp + upg * 0.26 - g * 0.6, gp + upg * 0.26 + g * 1.2, 0.07, "upper", sides=8)       # the recoil cylinder
            m.box(gp - g * 1.32 - upg * 0.14, (0.6, 0.2, 0.06), "steel", yaw=-bearing, pitch=10.0)     # the loading tray
        m.socket(f"muzzle.{name}.{k + 1}", tip, g, gn, "flash, smoke and the shell leave here")
        guns.append(gn)
    m.use(tn)
    for s in (1, -1):                       # the layer's and trainer's seats, sights and handwheels, out each side
        seat = W(-0.15, s * 1.18, 0.62)
        m.box(seat, (0.36, 0.36, 0.06), "gunmetal", yaw=-bearing)
        m.rod(W(-0.15, s * 1.18, 0.1), seat, 0.04, "upper", sides=5)
        m.rod(W(0.25, s * 1.0, 1.25), W(0.55, s * 1.18, 1.42), 0.04, "upper", sides=4)
        m.rod(W(0.4, s * 1.18, 1.45), W(0.9, s * 1.18, 1.45), 0.065, "gunmetal", sides=6)          # his sight, to a hood in the roof
        if fine:
            for hw in (0.13, -0.13):
                m.lathe(W(0.25, s * 1.18 + hw, 1.0), f, [(0.0, 0.15), (0.03, 0.15)], "gunmetal", 10, caps=(False, False))
            m.rod(W(0.05, s * 0.9, 1.0), W(0.25, s * 1.18, 1.0), 0.035, "upper", sides=4)
    if fine:
        m.box(W(-1.55, -1.3, 0.55), (0.55, 0.45, 0.9), "upper", yaw=-bearing)                   # the fuze setter, behind the shield
        m.box(W(-1.55, -1.3, 1.02), (0.4, 0.3, 0.06), "gunmetal", yaw=-bearing)
    if crew and fine:
        for s in (1, -1):
            fit.sailor(m, W(-0.12, s * 1.18, 0.1), f, "sit", seat=0.54, hands=(W(0.22, s * 1.18 + 0.14, 1.0), W(0.22, s * 1.18 - 0.14, 1.0)),
                       navy="JAP", lean=0.1)
            fit.sailor(m, W(-1.2, s * 0.45, 0.1), f, "load", navy="JAP")
        fit.sailor(m, W(-1.75, 0.75, 0.1), f - l * 0.4, "carry", navy="JAP")
    m.use(half)
    return tn, guns


def twin25(yard, n, x, y, z, bearing, tub=2.0, crew=True, parent=None, limits=(-100, 100)):
    """Japan's twin 25 mm mounting (Type 96) as fitted in 1934-41, open, in a splinter tub: two guns side by side in one
    cradle on a pedestal, the long flash hider on each and the box magazine standing on top, the layer and trainer
    seated either side, a loader and a man with a magazine behind, the mount's captain in the tub. Parts `aa25.N`
    (trains: plus clockwise seen from above) and `aa25.N.guns` (plus lifts them); socket `aa.K` at the muzzles."""
    m = yard.m
    half = parent or yard.half(x)
    m.use(half)
    fine = m.fine
    o = Vector((x, y, z))
    if tub:
        m.tub(o, tub, 1.05, "upper", wall=0.05, sides=22 if fine else 8, floor="deck")
        if fine:   # ready-use lockers for the magazines round the inside of the tub, behind the gun, and a lip on its rim
            for i in range(5):
                a = math.radians(-bearing + 180.0 + (i - 2) * 26.0)
                c = o + Vector((math.cos(a), math.sin(a), 0)) * (tub - 0.27)
                m.box(c + Z * 0.38, (0.36, 0.5, 0.72), "upper", yaw=math.degrees(a) + 90.0)
            m.lathe(o + Z * 1.03, Z, [(0.0, tub + 0.03), (0.05, tub + 0.03)], "upper", 22, caps=(False, False))
    tn = m.part(f"aa25.{n}", half, (x, y, z + 0.2), axis=(0, 0, -1), limits=list(limits), kind="turn",
                note=f"a twin 25 mm mounting: plus trains it clockwise seen from above; rest bearing {int(bearing)}; its arc is outboard, "
                     f"{int(limits[0])} to {int(limits[1])} from rest (inboard the flight deck's edge is in the way); about 30 degrees a second")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 30.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y

    def W(px, py, pz):
        return o + f * px + l * py + Z * pz
    m.lathe(o, Z, [(0.0, 0.4), (0.12, 0.34), (0.95, 0.24)], "upper", 12 if fine else 6)
    m.lathe(o + Z * 0.2, Z, [(0.0, 0.98), (0.06, 0.98)], "deck", 18 if fine else 6)
    for s in (1, -1):
        m.box(W(0.0, s * 0.36, 0.98), (0.55, 0.08, 0.45), "upper", yaw=-bearing)
    piv = W(0.0, 0.0, 1.18)
    gn = m.part(f"aa25.{n}.guns", tn, tuple(piv), axis=tuple(-l), limits=[-10, 85], kind="turn", note="the two guns: plus lifts them")
    m.use(gn)
    e = math.radians(15.0)
    g = (f * math.cos(e) + Z * math.sin(e)).normalized()
    upg = g.cross(l).normalized()
    if upg.z < 0:
        upg = -upg
    for s in (1, -1):
        q = piv + l * s * 0.17
        m.box(q - g * 0.2, (1.05, 0.15, 0.24), "gunmetal", yaw=-bearing, pitch=15.0)            # the receiver
        m.rod(q + g * 0.32, q + g * 1.7, 0.03, "gunmetal", sides=6 if fine else 4)              # the barrel
        m.lathe(q + g * 1.7, g, [(0.0, 0.045), (0.48, 0.05), (0.53, 0.036)], "gunmetal", 8 if fine else 4)   # the flash hider
        if fine:
            m.box(q - g * 0.18 + upg * 0.27, (0.34, 0.1, 0.3), "gunmetal", yaw=-bearing, pitch=15.0)     # the magazine
            m.rod(q - g * 0.62, q - g * 0.85 - upg * 0.16, 0.02, "gunmetal", sides=4)                  # the shoulder stock
            m.rod(q + g * 0.32, q + g * 0.6, 0.05, "gunmetal", sides=6)                                # the barrel's jacket
    m.box(piv - g * 0.05, (0.35, 0.5, 0.12), "gunmetal", yaw=-bearing, pitch=15.0)
    k = sum(1 for s in m.sockets if s["name"].startswith("aa.")) + 1
    m.socket(f"aa.{k}", piv + g * 2.25, g, gn, "a twin 25 mm mounting's muzzles: two streams of tracer start here")
    m.use(tn)
    for s in (1, -1):
        s0 = W(-0.32, s * 0.78, 0.2)
        m.box(s0 + Z * 0.4, (0.32, 0.32, 0.06), "gunmetal", yaw=-bearing)
        m.rod(s0, s0 + Z * 0.38, 0.035, "upper", sides=5)
        m.box(W(0.2, s * 0.62, 1.32), (0.18, 0.1, 0.18), "gunmetal", yaw=-bearing)                  # his sight, on its arm
        m.rod(W(0.16, s * 0.6, 1.24), W(0.05, s * 0.38, 1.12), 0.025, "gunmetal", sides=4)
        if fine:
            m.lathe(W(0.0, s * 0.62, 0.95), l * s, [(0.0, 0.12), (0.025, 0.12)], "gunmetal", 8, caps=(False, False))
    if crew and fine:
        for s in (1, -1):
            fit.sailor(m, W(-0.3, s * 0.78, 0.2), f, "sit", seat=0.44, hands=(W(0.05, s * 0.66 + 0.1, 0.98), W(0.05, s * 0.66 - 0.1, 0.98)), navy="JAP", lean=0.08)
        fit.sailor(m, W(-1.0, 0.3, 0.2), f, "load", navy="JAP")
        fit.sailor(m, W(-1.2, -0.35, 0.2), f, "carry", navy="JAP")
    m.use(half)
    if crew and fine and tub:
        a = math.radians(-bearing + 180.0 + 42.0)
        fit.sailor(m, o + Vector((math.cos(a), math.sin(a), 0)) * (tub - 0.32) + Z * 0.02, f, "stand", navy="JAP")
    return tn


def director91(yard, name, x, y, z, bearing=0.0):
    """The Type 91 high-angle director: a squat round house turning on a pedestal, the rangefinder's arms out each side
    with a hood at each end, the layers' window across its face, a hatch on top. A turn part (bearing clockwise)."""
    m = yard.m
    half = yard.half(x)
    dn = m.part(name, half, (x, y, z), axis=(0, 0, -1), kind="turn", note=f"a high-angle director: turns with the 12.7 cm guns it serves; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, lat = rot @ X, rot @ Y
    o = Vector((x, y, z))
    m.lathe(o, Z, [(0.0, 0.7), (0.5, 0.6)], "upper", 12 if m.fine else 6)
    m.lathe(o + Z * 0.5, Z, [(0.0, 1.35), (1.45, 1.3), (1.75, 0.95)], "upper", 18 if m.fine else 8)
    c = o + Z * 1.35
    m.lathe(c - lat * 2.2, lat, [(0.0, 0.2), (0.25, 0.25), (4.15, 0.25), (4.4, 0.2)], "upper", 10 if m.fine else 5)
    for s in (1, -1):
        m.box(c + lat * s * 2.0 + f * 0.15, (0.55, 0.45, 0.55), "upper", yaw=-bearing)
        m.disc(c + lat * s * 2.0 + f * 0.43, f, 0.12, "glass", 8)
    if m.fine:
        m.box(o + Z * 1.55 + f * 1.24, (0.06, 1.5, 0.3), "glass", yaw=-bearing)
        m.lathe(o + Z * 2.25, Z, [(0.0, 0.42), (0.12, 0.38)], "upper", 10)
    m.use(half)
    return dn


def director95(yard, name, x, y, z, bearing, crew=True):
    """The Type 95 director for the 25 mm guns: a short pedestal, a turning head with its sighting telescopes in front
    and a short rangefinder across it with a hood at each end, two handles at the back, the layer standing at them.
    A turn part (bearing clockwise), its arc outboard."""
    m = yard.m
    half = yard.half(x)
    fine = m.fine
    m.use(half)
    o = Vector((x, y, z))
    m.lathe(o, Z, [(0.0, 0.3), (0.12, 0.2), (0.95, 0.15)], "upper", 10 if fine else 5)
    dn = m.part(name, half, (x, y, z + 0.95), axis=(0, 0, -1), limits=[-110, 110], kind="turn",
                note=f"a Type 95 director for the 25 mm guns: plus turns it clockwise seen from above; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, lat = rot @ X, rot @ Y
    yaw = -bearing

    def W(px, py, pz):
        return o + f * px + lat * py + Z * pz
    m.lathe(W(0, 0, 0.95), Z, [(0.0, 0.2), (0.1, 0.2), (0.16, 0.14)], "upper", 10 if fine else 5)
    m.box(W(0.02, 0, 1.33), (0.55, 0.4, 0.42), "upper", yaw=yaw)                               # its body
    m.lathe(W(0.05, -0.85, 1.43), lat, [(0.0, 0.07), (0.1, 0.085), (1.6, 0.085), (1.7, 0.07)], "upper", 8 if fine else 4)   # the rangefinder
    for s in (1, -1):
        m.box(W(0.1, s * 0.82, 1.43), (0.24, 0.16, 0.22), "upper", yaw=yaw)                   # its end hoods
        if fine:
            m.disc(W(0.225, s * 0.82, 1.43), f, 0.06, "glass", 6)
            m.lathe(W(0.25, s * 0.11, 1.38), f, [(0.0, 0.065), (0.32, 0.06), (0.36, 0.07)], "gunmetal", 8)   # the telescopes
            m.disc(W(0.615, s * 0.11, 1.38), f, 0.06, "glass", 6)
            m.rod(W(-0.25, s * 0.16, 1.2), W(-0.42, s * 0.24, 1.18), 0.025, "gunmetal", sides=4)          # his handles
            m.rod(W(-0.42, s * 0.24, 1.1), W(-0.42, s * 0.24, 1.26), 0.03, "gunmetal", sides=5)
    if fine:
        m.box(W(-0.3, 0.0, 1.5), (0.1, 0.24, 0.12), "black", yaw=yaw)                           # the eyepieces
    if crew and fine:
        fit.sailor(m, W(-0.82, 0.0, 0.0), f, "aim", hands=(W(-0.42, 0.24, 1.2), W(-0.42, -0.24, 1.2)), navy="JAP", lean=0.12)
    m.use(half)
    return dn


def mg65(yard, x, y, z, facing, crew=True):
    """A 6.5 mm light machine gun on a pedestal at a gallery's rail, its gunner standing at it: a fixed light gun, with
    an `aa.K` socket at its muzzle."""
    m = yard.on(x)
    f = Vector((math.cos(math.radians(facing)), math.sin(math.radians(facing)), 0.0))
    lat = Z.cross(f)
    o = Vector((x, y, z))
    m.lathe(o, Z, [(0.0, 0.16), (0.08, 0.06), (1.0, 0.05)], "upper", 6)
    e = math.radians(18.0)
    g = (f * math.cos(e) + Z * math.sin(e)).normalized()
    q = o + Z * 1.12
    m.box(q - g * 0.08, (0.6, 0.09, 0.14), "gunmetal", yaw=facing, pitch=18.0)                       # the receiver
    m.rod(q + g * 0.22, q + g * 0.82, 0.018, "gunmetal", sides=5)                                   # the barrel
    if m.fine:
        m.rod(q + g * 0.22, q + g * 0.5, 0.032, "gunmetal", sides=6)                                # its cooling fins
        m.box(q + lat * 0.09 + Z * 0.02, (0.2, 0.1, 0.1), "gunmetal", yaw=facing, pitch=18.0)       # the hopper
        m.rod(q - g * 0.38, q - g * 0.58 - Z * 0.1, 0.03, "boat", sides=4)                           # the butt
    k = sum(1 for s in m.sockets if s["name"].startswith("aa.")) + 1
    m.socket(f"aa.{k}", q + g * 0.84, g, yard.half(x), "a 6.5 mm machine gun's muzzle")
    if crew and m.fine:
        fit.sailor(m, o - f * 0.75 + lat * 0.05, f, "aim", hands=(q - g * 0.32 + lat * 0.1, q - g * 0.32 - lat * 0.1), navy="JAP", lean=0.1)


def step_platform(yard, x, hand, w0, out, half_len, z):
    """A small platform built out square from her side a step above the gallery, its outer corners rounded, a rail
    round its outer edge, knees under it: where a director stands."""
    from ships import railing
    m = yard.on(x)
    plan = outboard_rect(x + half_len, x - half_len, hand * (w0 - 0.1), hand * (w0 + out), 0.7, 3 if m.fine else 1)
    cap.slab(m, plan, z - 0.22, z, paint="upper", top="deck", bottom=True)
    if m.fine:
        edge = [Vector((px, py, z)) for px, py in plan if abs(py) > w0 + 0.05]
        edge.sort(key=lambda p: p.x)
        inset = [Vector((p.x - math.copysign(0.07, p.x - x), p.y - hand * 0.07, z)) for p in edge]
        railing(m, inset, high=1.0, every=0.9)
        for dx in (-half_len * 0.6, half_len * 0.6):
            m.rod((x + dx, hand * w0, z - 1.7), (x + dx, hand * (w0 + out - 0.3), z - 0.2), 0.07, "upper", sides=4)


def sponson_stair(yard, x0, x1, y, z0, z1):
    """An inclined ladder from a gallery up to a sponson raised above it: from (x0, y) at z0 to (x1, y) at z1."""
    if not yard.m.fine:
        return
    m = yard.on(x1)
    fit.stair(m, Vector((x0, y, z0)), Vector((x1, y, z1)), wide=0.7)
    for s in (-0.35, 0.35):                               # (stanchions for its hand rails, at its foot and its head)
        for p_ in (Vector((x0, y + s, z0)), Vector((x1, y + s, z1))):
            m.rod(p_, p_ + Z * 0.92, 0.025, "upper", sides=4)


# ---- her funnel ----------------------------------------------------------------------------------------------------

def great_funnel(yard, x, wall_y, z0, rx=5.6, rz=2.9, out=1.4, bend=4.0, down=62.0, run=1.6):
    """Her one great funnel (the uptakes of all eight boilers brought together), led out through her starboard side
    just under the flight deck's edge and turned down toward the sea, its mouth facing down and out: a broad trunk of
    rounded oblong section, ribbed with its plates' seams, in a tapered casing where it leaves her side (wide under
    the deck's edge, narrower below), the spray pipe that cooled the gases standing off round it near the mouth on
    clips, bars across the mouth and the hinged cover over its upper lip (closed if she heeled far enough to put the
    mouth in the sea), a hood of curved plate on its hinge. Returns its mouth's middle."""
    m = yard.on(x)
    fine = m.fine
    n = 28 if fine else 10
    hand = -1.0 if wall_y < 0 else 1.0
    path, tans = [], []
    a_end = math.radians(down)
    steps = (3, 9, 3) if fine else (1, 3, 1)
    for i in range(steps[0] + 1):                         # straight out from her side
        t = i / steps[0]
        path.append(Vector((x, wall_y + hand * out * t, z0)))
        tans.append(Vector((0, hand, 0)))
    for i in range(1, steps[1] + 1):                      # round the bend
        th = a_end * i / steps[1]
        path.append(Vector((x, wall_y + hand * (out + math.sin(th) * bend), z0 - (1.0 - math.cos(th)) * bend)))
        tans.append(Vector((0, hand * math.cos(th), -math.sin(th))))
    last = path[-1]
    for i in range(1, steps[2] + 1):                      # and on to the mouth
        t = run * i / steps[2]
        path.append(last + Vector((0, hand * math.cos(a_end), -math.sin(a_end))) * t)
        tans.append(Vector((0, hand * math.cos(a_end), -math.sin(a_end))))

    def up_of(tan):
        u = X.cross(tan).normalized()
        return -u if u.z < 0 else u

    def oblong(p, tan, grow, k=n):
        # (a rounded oblong rather than an oval: flatter on top, where it runs up under the flight deck's edge)
        up = up_of(tan)
        ring = []
        for j in range(k):
            a = 2 * math.pi * j / k
            ca, sa = math.cos(a), math.sin(a)
            ring.append(p + X * math.copysign(abs(ca) ** 0.7, ca) * rx * grow + up * math.copysign(abs(sa) ** 0.8, sa) * rz * grow)
        return ring

    def normals(ring, tan):
        out_ = []
        cen = sum(ring, Vector()) / len(ring)
        for j, q in enumerate(ring):
            t = ring[(j + 1) % len(ring)] - ring[j - 1]
            nn = t.cross(tan).normalized()
            out_.append(nn if nn.dot(q - cen) > 0 else -nn)
        return out_
    grows = [lerp(1.0, 1.1, i / (len(path) - 1)) for i in range(len(path))]
    rings = [oblong(p, tan, g) for p, tan, g in zip(path, tans, grows)]
    root = [q - Vector((0, hand * 0.8, 0)) for q in rings[0]]
    m.skin([root] + rings, "upper", (False, False))
    end, tan = path[-1], tans[-1]
    up = up_of(tan)
    lip = [end + (q - end) * 1.05 for q in rings[-1]]
    m.skin([rings[-1], lip, [q + tan * 0.3 for q in lip]], "upper", (False, False))
    inner = [end + (q - end) * 0.9 + tan * 0.3 for q in rings[-1]]
    m.skin([[q + tan * 0.3 for q in lip], inner], "upper", (False, False))
    deep = [end + (q - end) * 0.9 - tan * 1.6 for q in rings[-1]]
    m.skin([deep, inner], "black", (True, False), inside=True, cap_paint="black")

    # the casing where it leaves her side: wide under the deck's edge, drawn in below, its face standing out a little
    # past where the trunk starts to bend
    zt, zb = FD - 0.04, z0 - rz - 0.9
    face = out + 0.25

    def casing_ring(yy, k):
        wt, wb = rx + 1.25 * k, rx + 0.45 * k
        pts = [(-wt, zt), (-wb - 0.35, zb + 0.6), (-wb, zb), (wb, zb), (wb + 0.35, zb + 0.6), (wt, zt)]
        return [Vector((x + px, yy, pz)) for px, pz in pts]
    cr = [casing_ring(wall_y - hand * 0.3, 1.0), casing_ring(wall_y + hand * face * 0.6, 0.97), casing_ring(wall_y + hand * face, 0.9)]
    m.skin(cr, "hull", (False, True), smooth=False, closed=True)
    if fine:
        for i in (2, 5, 8, 11, 14):                       # its plates' seams, round the trunk
            if i < len(rings):
                cen = path[i]
                ring = [cen + (q - cen) * 1.008 for q in rings[i]] + [cen + (rings[i][0] - cen) * 1.008]
                m.strip(ring, [(q - cen).normalized() for q in ring], 0.02, "seam", 0.006)
        for j in range(-4, 5):                            # bars across its mouth
            c = end + tan * 0.15 + X * (j * rx * 0.2)
            # (to the trunk's inside at that place across the mouth: the rounded oblong's half height there)
            ca_ = min((abs(j) * 0.2 / grows[-1]) ** (1.0 / 0.7), 0.999)
            w = rz * grows[-1] * math.sqrt(1.0 - ca_ * ca_) ** 0.8 * 0.99
            m.rod(c - up * w, c + up * w, 0.05, "steel", sides=4, smooth=False)
        # the spray pipe round the trunk near its mouth, standing off it on clips
        c0 = end - tan * 0.7
        g0 = lerp(grows[-2], grows[-1], 0.5)
        skin_ring = oblong(c0, tan, g0, 24)
        nrm = normals(skin_ring, tan)
        pipe = [q + w_ * 0.32 for q, w_ in zip(skin_ring, nrm)]
        for a_, b_ in zip(pipe, pipe[1:] + pipe[:1]):
            m.rod(a_, b_, 0.075, "steel", sides=6, smooth=False)
        for j in range(0, 24, 3):
            m.rod(skin_ring[j] - nrm[j] * 0.03, pipe[j], 0.04, "steel", sides=4)
            m.box(skin_ring[j] + nrm[j] * 0.02, (0.16, 0.16, 0.16), "steel")
        # the hinged cover over its upper lip: a hood of curved plate on a hinge as broad as the mouth's flat top, its
        # brackets down onto the trunk
        top_lip = [q for q in lip if (q - end).dot(up) > rz * 0.55]
        top_lip.sort(key=lambda q: q.x)
        rows_o, rows_i = [], []
        for q in top_lip:
            w_ = (q - end).dot(up) / (rz * 1.155)
            o_ = (q - end - tan * (q - end).dot(tan)).normalized()
            a_ = q + tan * 0.28 + o_ * 0.05
            b_ = a_ + tan * 1.0 + o_ * (0.22 * w_)
            rows_o.append([a_, b_])
            rows_i.append([a_ - o_ * 0.06, b_ - o_ * 0.06])
        m.sheet(rows_o, "upper", up, smooth=True)
        m.sheet(rows_i, "upper", -up, smooth=True)
        m.strip([r[1] - (r[1] - r[0]).normalized() * 0.02 for r in rows_o], [tan] * len(rows_o), 0.035, "upper", 0.0)
        hx = min(abs(q.x - x) for q in (top_lip[0], top_lip[-1])) - 0.15
        hinge = end + up * (rz * 1.155 + 0.14) + tan * 0.28
        m.rod(hinge - X * hx, hinge + X * hx, 0.09, "steel", sides=8)
        for s in (-1, 1):
            hb = hinge + X * s * (hx - 0.3)
            m.box(hb - up * 0.17 - tan * 0.2, (0.18, 0.5, 0.42), "steel")
            m.box(hb - up * 0.04 + tan * 0.18, (0.2, 0.16, 0.2), "steel")
        # rungs up its flank, fore and aft, from the casing to the spray pipe
        for k in range(4, len(path) - 2, 1):
            p = path[k]
            u_k = up_of(tans[k])
            for sx_ in (1, -1):
                side = p + X * sx_ * (rx * grows[k] + 0.08)
                m.rod(side - u_k * 0.2, side + u_k * 0.2, 0.02, "steel", sides=3, smooth=False)
    k = sum(1 for s in m.sockets if s["name"].startswith("funnel.")) + 1
    m.socket(f"funnel.{k}", end + tan * 0.8, tan, yard.half(x), "her great funnel's mouth, turned down over her starboard side: her smoke rolls out low over the water and aft")
    yard.soot_at.append((end.x, end.y, end.z + 2.0, 8.0))
    return end


# ---- her island ----------------------------------------------------------------------------------------------------

def binoculars(m, foot, look):
    """A pair of the Navy's big 12 cm binoculars on a pedestal at a bridge's screen: a post, a fork, the two long
    barrels and their eyepieces bent down to a man's eye. `look` is the way they face."""
    f = Vector((look.x, look.y, 0.0)).normalized()
    side = Z.cross(f)
    m.lathe(foot, Z, [(0.0, 0.14), (0.06, 0.07), (1.12, 0.06)], "upper", 6)
    top = foot + Z * 1.2
    m.box(top, (0.12, 0.42, 0.08), "gunmetal", yaw=math.degrees(math.atan2(f.y, f.x)))
    for s in (1, -1):
        m.rod(top + side * s * 0.19, top + side * s * 0.19 + Z * 0.16, 0.025, "gunmetal", sides=4)
        c = top + side * s * 0.12 + Z * 0.2
        m.lathe(c - f * 0.32, f, [(0.0, 0.055), (0.12, 0.07), (0.62, 0.075), (0.7, 0.085)], "gunmetal", 8, caps=(True, True))
        m.disc(c + f * 0.385, f, 0.07, "glass", 8)
        m.rod(c - f * 0.32, c - f * 0.42 - Z * 0.1, 0.03, "black", sides=5)


def island(yard, xf, xa, y_in, y_out, crew=True):
    """The small island on her starboard side, on a sponson of its own built out from the flight deck's edge: a
    deckhouse, a second house with the searchlight on its fore end, the navigating bridge with its windows round the
    front, the open compass bridge on top behind its screen, the rangefinder, and the tripod mast at its after end with
    its yard and the gaff her ensign flies from. Returns the gaff's end and the mast's head."""
    from ships import bulwark, ladder, railing
    m = yard.m
    fine = m.fine
    xi = (xf + xa) * 0.5
    m.use(yard.half(xi))
    # the sponson it stands on, from the gallery up to the deck's level, on knees from her side
    spon = outboard_rect(xf, xa, y_in, y_out, 1.4, 3 if fine else 1)
    cap.slab(m, spon, ZG - 0.4, IZ, paint="hull", top="deck")
    plate_top(m, spon, ZG - 0.4, "hull", down=True)
    w0 = yard.h.half_at(xi) * 0.99
    for dx in (-7.0, -2.5, 2.5, 7.0):
        m.rod((xi + dx, -w0, ZG - 4.6), (xi + dx, y_out + 0.6, ZG - 0.5), 0.2, "upper", sides=6 if fine else 4)
    if fine:
        railing(m, [Vector((xf - 0.5, y_out + 0.15, IZ)), Vector((xa + 0.5, y_out + 0.15, IZ))], every=1.5)
    yi = (y_in + y_out) * 0.5 - 0.35                       # (the houses stand toward its outer edge)
    tz = IZ
    levels = ((xf - 1.3, xa + 1.3, 2.35, 2.4, "house"), (xf - 2.6, xa + 3.6, 2.1, 2.2, "house2"), (xf - 4.4, xa + 6.4, 1.75, 2.4, "bridge"))
    tops = []
    for k, (hf, ha, hw, hh, what) in enumerate(levels):
        pl = round_front(hf, ha, yi - hw, yi + hw, 0.6, 8 if fine else 3) if what == "bridge" else Model.round_rect(hf, ha, yi - hw, yi + hw, 0.8, 3 if fine else 1)
        cap.slab(m, pl, tz, tz + hh, paint="upper", top="deck")
        if fine:
            n_ = len(pl)
            if what == "bridge":                          # the bridge's windows round its front and down its sides, an eyebrow over them
                panes = []                                # (each pane: its two ends on the wall and the way it looks)
                for i in range(n_):
                    a, b = Vector((*pl[i], 0)), Vector((*pl[(i + 1) % n_], 0))
                    d = b - a
                    if d.length < 0.2:
                        continue
                    o_ = Vector((d.y, -d.x, 0)).normalized()
                    if min(a.x, b.x) >= hf - 2.6:
                        panes.append((a, b, o_))
                    elif abs(o_.y) > 0.9 and max(a.x, b.x) >= hf - hw - 0.05:
                        # (down each side from the round of its front: two panes, the foremost at the round's end)
                        fx_ = hf - hw
                        for k_ in range(2):
                            p0, p1 = Vector((fx_ - 0.08 - k_ * 1.05, a.y, 0)), Vector((fx_ - 1.03 - k_ * 1.05, a.y, 0))
                            panes.append((p0, p1, o_) if (p1 - p0).dot(d) > 0 else (p1, p0, o_))
                for a, b, o_ in panes:
                    d = b - a
                    c = (a + b) * 0.5 + o_ * 0.02
                    u = d.normalized() * (d.length * 0.5 - 0.05)
                    m.add([c - u + Z * (tz + 1.1), c + u + Z * (tz + 1.1), c + u + Z * (tz + 2.0), c - u + Z * (tz + 2.0)], [(0, 1, 2, 3)], "glass", False,
                          lambda q, o=o_: o)
                    # the frames between the panes, and the sill under them
                    for e_ in (a, b):
                        m.box(e_ + o_ * 0.035 + Z * (tz + 1.55), (0.07, 0.08, 0.98), "upper", yaw=math.degrees(math.atan2(o_.y, o_.x)))
                    m.box(c + o_ * 0.05 + Z * (tz + 1.06), (0.1, d.length + 0.06, 0.07), "upper", yaw=math.degrees(math.atan2(o_.y, o_.x)))
                eb = [p for p in pl if p[0] > hf - 2.8] + [(hf - hw - 2.3, yi - hw), (hf - hw - 2.3, yi + hw)]
                eb.sort(key=lambda p: math.atan2(p[1] - yi, p[0] - (hf - hw)))
                m.strip([Vector((px, py, tz + 2.12)) for px, py in eb], [Z] * len(eb), 0.22, "upper", 0.0)
            else:                                         # scuttles along its sides and a door in its after end
                for i in range(n_):
                    a, b = Vector((*pl[i], 0)), Vector((*pl[(i + 1) % n_], 0))
                    d = b - a
                    if d.length < 0.9:
                        continue
                    o_ = Vector((d.y, -d.x, 0)).normalized()
                    c = (a + b) * 0.5 + o_ * 0.02
                    if abs(o_.y) > 0.9 and d.length > 3.0:
                        for j in range(-2, 3) if what == "house" else (-1, 0, 1):
                            if what == "house" and j == 0:
                                m.add([c - Vector((0.4, 0, 0)) + Z * (tz + 0.1), c + Vector((0.4, 0, 0)) + Z * (tz + 0.1), c + Vector((0.4, 0, 0)) + Z * (tz + 1.95),
                                       c - Vector((0.4, 0, 0)) + Z * (tz + 1.95)], [(0, 1, 2, 3)], "door", False, lambda q, o=o_: o)
                                continue
                            m.disc(c + d.normalized() * j * d.length * 0.2 + Z * (tz + 1.6), o_, 0.17, "glass", 8)
                    elif o_.x < -0.9:
                        m.add([c - Y * 0.4 + Z * (tz + 0.1), c + Y * 0.4 + Z * (tz + 0.1), c + Y * 0.4 + Z * (tz + 1.95), c - Y * 0.4 + Z * (tz + 1.95)],
                              [(0, 1, 2, 3)], "door", False, lambda q, o=o_: o)
                railing(m, [Vector((px, py, tz + hh)) for px, py in pl] + [Vector((*pl[0], tz + hh))], every=1.3)
        tops.append((tz + hh, hf, ha, hw))
        tz += hh
    z_bridge_roof = tz
    # the open compass bridge on the bridge's roof: a screen round its front and down its sides with a wind deflector
    # turned out along its top, open behind
    bf, ba, bw = levels[2][0], levels[2][1], levels[2][2]
    pl = round_front(bf, ba, yi - bw, yi + bw, 0.6, 8 if fine else 3)
    sx = ba + 2.5                                         # (where the sides end: clear of the rangefinder's sweep)
    arc = [(sx, yi - bw)] + sorted([p for p in pl if p[1] < yi and p[0] > ba + 0.9], key=lambda p: p[0]) + \
        sorted([p for p in pl if p[1] >= yi and p[0] > ba + 0.9], key=lambda p: -p[0]) + [(sx, yi + bw)]
    bulwark(m, arc, tz, tz + 1.15, 0.06, "upper", lip=0.22 if fine else 0.0)
    if fine:
        front = arc[1:-1]
        for p in front[1:-1:2]:                           # voice pipes up the inside of the screen
            c = Vector((lerp(p[0], bf - 2.0, 0.12), lerp(p[1], yi, 0.12), tz))
            m.rod(c, c + Z * 0.95, 0.04, "steel", sides=5)
            m.lathe(c + Z * 0.95, Z, [(0.0, 0.06), (0.1, 0.09)], "brass", 6, caps=(False, True))
        for p in (front[1], front[3], front[5], front[7]) if len(front) > 7 else front[1:-1]:
            binoculars(m, Vector((lerp(p[0], bf - 2.0, 0.28), lerp(p[1], yi, 0.28), tz)), Vector((p[0] - (bf - bw), p[1] - yi, 0.0)))
        m.box((bf - 2.4, yi, tz + 0.55), (0.6, 0.6, 1.1), "upper")                 # the compass binnacle
        m.ball((bf - 2.4, yi, tz + 1.15), (0.25, 0.25, 0.2), "brass", 8, 4, top_only=True)
        for s_ in (1, -1):
            # a signal lamp at the screen's after end each side, on its pedestal, and the flag lockers forward of it
            lp = Vector((sx + 0.25, yi + s_ * (bw - 0.32), tz))
            m.lathe(lp, Z, [(0.0, 0.16), (0.1, 0.07), (1.05, 0.06)], "upper", 6)
            m.rod(lp + Z * 1.12 - X * 0.05, lp + Z * 1.12 + X * 0.05, 0.03, "steel", sides=4)
            m.lathe(lp + Z * 1.32 - X * 0.3, X, [(0.0, 0.17), (0.12, 0.22), (0.5, 0.22), (0.56, 0.2)], "upper", 10, caps=(True, False))
            m.disc(lp + Z * 1.32 + X * 0.265, X, 0.2, "glass", 10)
            m.box(lp + Z * 1.32 + X * 0.3, (0.04, 0.46, 0.04), "steel")             # its shutter's slats
            m.box(lp + Z * 1.32 + X * 0.3 + Z * 0.1, (0.04, 0.46, 0.03), "steel")
            m.box(lp + Z * 1.32 + X * 0.3 - Z * 0.1, (0.04, 0.46, 0.03), "steel")
            lk = Vector((sx + 1.35, yi + s_ * (bw - 0.32), tz))
            m.box(lk + Z * 0.45, (1.2, 0.42, 0.9), "upper")
            for j in range(3):
                m.box(lk + Z * (0.18 + j * 0.27) + Vector((0, -s_ * 0.215, 0)), (1.1, 0.012, 0.012), "seam")
            m.box(lk + Z * 0.93, (1.26, 0.48, 0.06), "upper")
    # the rangefinder on its platform at the compass bridge's after end
    fit.rangefinder(yard, "director.fore", ba + 1.0, yi, tz + 0.3, 1.9, bearing=0.0, house=0.85, parent=yard.half(xi))
    m.use(yard.half(xi))
    m.lathe((ba + 1.0, yi, tz), Z, [(0.0, 0.9), (0.3, 0.9)], "upper", 14 if fine else 6)
    # the searchlight on the second house's fore end, on a round platform
    z2, f2 = tops[1][0], tops[1][1]
    c = Vector((f2 - 0.2, yi, z2))
    m.lathe(c - Z * 0.2, Z, [(0.0, 1.25), (0.2, 1.25)], "upper", 18 if fine else 8)
    if fine:
        railing_arc(m, c, 1.18, -math.pi * 0.55, math.pi * 0.55, c.z, every=0.8)
    yard.searchlight(c.x + 0.2, c.y, c.z, 0.55, (1, 0, 0.05))
    # the tripod mast at the island's after end, its yard and signal halyards, a topmast, the gaff
    # (its pole stands on the second house's roof, a little in from its after end; its two legs splay aft and out to
    #  the first house's roof behind it, clear of the rounded corners)
    z1 = tops[0][0]
    mx = levels[1][1] + 0.6
    foot = Vector((mx, yi, tops[1][0]))
    head = Vector((mx - 0.6, yi, IZ + 16.5))
    m.lathe(foot, head - foot, [(0.0, 0.24), ((head - foot).length * 0.6, 0.19), ((head - foot).length, 0.08)], "upper", 8 if fine else 5)
    m.lathe(foot, Z, [(0.0, 0.42), (0.12, 0.38), (0.2, 0.27)], "upper", 10 if fine else 5)          # its step on the roof
    for s in (1, -1):
        lf = Vector((mx - 1.2, yi + s * 1.55, z1))
        m.rod(lf, (mx - 0.3, yi, IZ + 10.0), 0.12, "upper", sides=6 if fine else 4)
        m.lathe(lf, Z, [(0.0, 0.26), (0.1, 0.2)], "upper", 8 if fine else 4)
    zy = IZ + 12.0
    m.rod((mx - 0.45, yi - 3.2, zy), (mx - 0.45, yi + 3.2, zy), 0.07, "upper", rb=0.07, sides=4)
    m.lathe((mx - 0.35, yi, IZ + 9.4), Z, [(0.0, 0.75), (0.12, 0.75)], "upper", 12 if fine else 6)        # a small top for the lookouts
    gaff0, gaff1 = Vector((mx - 0.4, yi, IZ + 10.6)), Vector((mx - 3.8, yi, IZ + 12.2))
    m.rod(gaff0, gaff1, 0.06, "upper", sides=4)
    if fine:
        for s in (1, -1):                                 # the halyards from the yard's arms down to the compass bridge
            yard.wire((mx - 0.45, yi + s * 3.0, zy), (bf - 1.5, yi + s * 1.4, tz + 1.1), 0.015)
        m.lathe(head + Z * 0.05, Z, [(0.0, 0.05), (1.4, 0.035)], "upper", 4)
        m.rod((mx - 0.6, yi - 1.0, IZ + 15.2), (mx - 0.6, yi + 1.0, IZ + 15.2), 0.04, "upper", sides=4)
        # ladders up its after face, life rings on its sides, a float rack
        ladder(m, Vector((xa + 1.3 - 0.0, yi + 1.0, IZ)), z1, (-1, 0, 0))
        ladder(m, Vector((xa + 3.6, yi - 0.9, z1)), tops[1][0], (-1, 0, 0))
        ladder(m, Vector((xa + 6.4, yi + 0.6, tops[1][0])), z_bridge_roof, (-1, 0, 0))
        for dx, dz in ((-1.0, 1.0), (2.4, 1.0), (-0.4, 3.4)):
            p = Vector((xi + dx, yi + 2.35 + 0.03 if dz < 2.0 else yi + 2.1 + 0.03, IZ + dz))
            m.lathe(p, Vector((0, 1, 0)), [(0.0, 0.32), (0.12, 0.32)], "cork", 10, caps=(False, False))
        # (a pair of rafts on its inboard face, between the scuttles and below them, clear of the sponson's edge rail)
        fit.float_rack(yard, xi + 4.5, yi + levels[0][2] + 0.02, IZ + 0.75, (0, 1, 0), n=2, long=2.0, wide=0.9)
    if crew and fine:
        rg = random.Random(1935)
        for (dx, dy, face) in ((-2.0, 0.65, (1, 0)), (-2.9, -0.85, (1, -0.4)), (-3.7, 0.55, (0.4, 1)), (-1.4, -0.5, (1, 0.2))):
            fit.sailor(m, Vector((bf + dx, yi + dy, tz)), Vector((*face, 0)), "stand", navy="JAP")
        fit.sailor(m, Vector((c.x - 0.6, c.y - 0.7, c.z)), Vector((1, 0.2, 0)), "stand", navy="JAP")
        for (dx, dy) in ((xf - 0.8, y_in - 0.7), (xa + 0.9, y_in - 0.6)):
            fit.sailor(m, Vector((dx, dy, IZ)), Vector((rg.uniform(-1, 1), 1, 0)), "stand", navy="JAP")
    return gaff1, head


# ---- her deck's markings --------------------------------------------------------------------------------------------

def bomb_cart(m, at, yaw):
    """A low bomb trolley on four small wheels with a 250 kg bomb on its cradle and a towing bar."""
    if not m.fine:
        return
    r = math.radians(yaw)
    f, l = Vector((math.cos(r), math.sin(r), 0)), Vector((-math.sin(r), math.cos(r), 0))
    m.box(at + Z * 0.3, (1.9, 0.75, 0.08), "upper", yaw=yaw)
    for s1 in (-0.7, 0.7):
        for s2 in (-0.42, 0.42):
            m.lathe(at + f * s1 + l * s2 + Z * 0.16 - l * 0.05, l, [(0.0, 0.16), (0.1, 0.16)], "black", 8)
    for s1 in (-0.45, 0.45):
        m.box(at + f * s1 + Z * 0.42, (0.12, 0.6, 0.16), "upper", yaw=yaw)
    c = at + Z * 0.68
    m.lathe(c - f * 0.95, f, [(0.0, 0.02), (0.25, 0.17), (1.35, 0.17), (1.75, 0.08), (1.9, 0.02)], "gunmetal", 10)
    for s in (1, -1):
        m.box(c - f * 0.95 + l * s * 0.12, (0.25, 0.02, 0.3), "gunmetal", yaw=yaw)
    m.rod(at + f * 0.95 + Z * 0.3, at + f * 1.9 + Z * 0.08, 0.025, "steel", sides=4)


def fuel_cart(m, at, yaw):
    """A small fuel cart: a tank on a two-wheeled frame, its hose coiled on top, the handle down on the deck."""
    if not m.fine:
        return
    r = math.radians(yaw)
    f, l = Vector((math.cos(r), math.sin(r), 0)), Vector((-math.sin(r), math.cos(r), 0))
    for s in (1, -1):
        m.lathe(at + l * s * 0.55 + Z * 0.3 - l * s * 0.06, l * s, [(0.0, 0.3), (0.12, 0.3)], "black", 10)
    m.lathe(at + Z * 0.62 - f * 0.65, f, [(0.0, 0.2), (0.08, 0.36), (1.22, 0.36), (1.3, 0.2)], "upper", 12)
    m.lathe(at + Z * 1.02, Z, [(0.0, 0.25), (0.1, 0.25)], "black", 10, caps=(False, False))
    m.rod(at - f * 0.65 + Z * 0.5, at - f * 1.6 + Z * 0.05, 0.03, "steel", sides=4)
    m.rod(at + f * 0.5 + Z * 0.3, at + f * 0.5 + Z * 0.02, 0.03, "steel", sides=4)


def kana_ka(m, xc, yc, z, size):
    """Her kana, カ (KA), in white on her after flight deck, upright to a pilot coming in over the stern: a stroke along
    and down to a hook, and a stroke falling to the left through it."""
    def P(u, v):                 # (u across to his right: her starboard; v up his page: forward)
        return Vector((xc + (v - 0.5) * size, yc - (u - 0.5) * size, z))
    w = size * 0.055
    strokes = [
        [(0.1, 0.7), (0.4, 0.7), (0.72, 0.7), (0.785, 0.692), (0.824, 0.668), (0.838, 0.62), (0.832, 0.52), (0.805, 0.36), (0.775, 0.23), (0.74, 0.135),
         (0.695, 0.078), (0.64, 0.065), (0.585, 0.088), (0.535, 0.125)],
        [(0.46, 0.98), (0.46, 0.8), (0.445, 0.66), (0.43, 0.6), (0.37, 0.4), (0.27, 0.2), (0.13, 0.03)],
    ]
    def spline(pts, k=4):
        # (a Catmull-Rom curve through the points, k to each span: the brush's stroke, not a chain of straight cuts)
        out = []
        n = len(pts)
        for i in range(n - 1):
            p0, p1, p2, p3 = pts[max(i - 1, 0)], pts[i], pts[i + 1], pts[min(i + 2, n - 1)]
            for j in range(k):
                t = j / k
                t2, t3 = t * t, t * t * t
                out.append(0.5 * ((2.0 * p1) + (p2 - p0) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 + (3.0 * p1 - p0 - 3.0 * p2 + p3) * t3))
        out.append(pts[-1])
        return out

    def brush(t):
        # (the brush comes down full, and lifts off over the last of each stroke)
        return lerp(0.85, 1.0, min(t / 0.05, 1.0)) if t < 0.82 else lerp(1.0, 0.3, (t - 0.82) / 0.18)
    for i, s in enumerate(strokes):
        pts = spline([P(u, v) for u, v in s], 4 if m.fine else 1)
        m.strip(pts, [Z] * len(pts), w, "white", 0.026 + 0.006 * i, taper=brush)


# ---- the ship ------------------------------------------------------------------------------------------------------

def kaga(fine=True):
    """Kaga as she was in 1941-42 (see the module's head for what she is built from)."""
    from ships import railing
    paints = livery("JAP", "carrier")
    paints.update(KAGA_PAINT)
    paints = fit.crew_paints(paints, "JAP")
    m = Model("Kaga", "Aircraft carrier Kaga, as rebuilt 1933-35, 1941-42 (our own model)", "ship", paints, fine)
    h = Closer(KagaHull(KAGA_PLAN, decks=KAGA_DECK, L=247.65, B=29.62, D=9.3, T=9.48, rake=5.0, counter=3.5, flare=0.25, fine=1.6, full=0.72, wl_fore=0.56,
                        bilge=5.5))
    bx = BX
    yard = Yard(m, h, break_x=bx)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("deck", above=[0.08, 0.17, 0.27, 0.38, 0.5, 0.66, 0.83, 1.0] if fine else [0.3, 0.6, 1.0])
    D = h.deck_z
    iz, fd, zg = IZ, FD, ZG
    hx0, hx1 = HX0, HX1

    def w_at(x):                                          # her hangars' side, carried straight up from her deck's edge
        return h.half_at(x) * 0.99

    # ---- where things stand along her sides (x, half length): the galleries give way to them
    lifts = [(62.0, 0.0, 11.5, 12.0, "planks"), (33.0, 0.0, 15.9, 10.7, "planks"), (-69.0, 0.0, 12.8, 9.9, "planks")]
    h127 = {PORT: [70.0, -7.0, -22.0, -40.0], STBD: [70.0, -26.0, -44.0, -60.0]}
    a25 = {PORT: [88.0, 52.0, 30.0, -54.0, -70.0, -84.0], STBD: [88.0, 52.0, -12.0, -76.0, -86.0]}
    lights = {PORT: [94.5], STBD: [94.5]}
    dirs = {PORT: [41.0], STBD: [-35.0]}
    d95 = {PORT: [81.0, 35.0, -47.0], STBD: [81.0, 46.0, -81.0]}
    mgs = {PORT: [63.0, -14.0, -34.0], STBD: [60.0, 10.0, -20.0]}
    masts = {PORT: [-30.0, -62.0, -77.5], STBD: [-52.0, -68.0]}
    xf_, xi_f, xi_a = -1.0, 36.5, 17.5                    # the funnel, the island's sponson
    stops = {PORT: [], STBD: [(xf_ - 7.6, xf_ + 7.6), (xi_a - 0.5, xi_f + 0.5)]}
    for hand in (PORT, STBD):
        stops[hand] += [(x - 3.3, x + 3.3) for x in h127[hand]] + [(x - 2.6, x + 2.6) for x in a25[hand]] + \
            [(x - 2.0, x + 2.0) for x in lights[hand]] + [(x - 2.6, x + 2.6) for x in dirs[hand]] + [(x - 1.55, x + 1.55) for x in d95[hand]]

    # ---- the hangars' walls: her side carried up from her upper deck to the flight deck, two hangars high; amidships
    # each side two deep recesses in the lower hangar's wall, each with a boat on its davits
    recesses = {PORT: [3.0, 15.0], STBD: [12.0, -25.0]}
    RL, RD, RZ0, RZ1 = 11.0, 2.2, 10.0, 13.6

    def in_recess(hand, x, margin=0.0):
        return any(abs(x - r_) < RL * 0.5 + margin for r_ in recesses[hand])
    for x0, x1, part in ((bx, hx0, "fore"), (hx1, bx, "aft")):
        lo_x, hi_x = min(x0, x1), max(x0, x1)
        xs = [lerp(x0, x1, k / 26.0) for k in range(27)]
        cuts = {hand: [(r_ - RL * 0.5, r_ + RL * 0.5) for r_ in recesses[hand] if lo_x < r_ - RL * 0.5 and r_ + RL * 0.5 < hi_x] for hand in (PORT, STBD)}
        xs_all = sorted(set([round(x, 6) for x in xs] + [round(e, 6) for hand in cuts for c in cuts[hand] for e in c]), reverse=True)

        def side_of(hand, notched):
            pts = []
            for x in xs_all:                               # (fore to aft)
                w = w_at(x)
                if notched and any(abs(x - c1) < 1e-6 for c0, c1 in cuts[hand]):
                    pts += [(x, w), (x, w - RD)]
                elif notched and any(abs(x - c0) < 1e-6 for c0, c1 in cuts[hand]):
                    pts += [(x, w - RD), (x, w)]
                elif notched and any(c0 < x < c1 for c0, c1 in cuts[hand]):
                    pts.append((x, w - RD))
                else:
                    pts.append((x, w))
            return pts

        def plan_of(notched):
            port = side_of(PORT, notched)
            star = [(x, -y) for x, y in reversed(side_of(STBD, notched))]
            return ccw([(xs_all[0], 0.0)] + port + [(xs_all[-1], 0.0)] + star)
        m.use(part)
        z_lo = D(x0 if part == "fore" else x1) - 0.5
        if fine and (cuts[PORT] or cuts[STBD]):
            cap.slab(m, plan_of(False), z_lo, RZ0, paint="hull", top=None)
            cap.slab(m, plan_of(True), RZ0, RZ1, paint="hull", top=None)
            cap.slab(m, plan_of(False), RZ1, fd, paint="hull", top=None)
            for hand in (PORT, STBD):
                for c0, c1 in cuts[hand]:
                    wa, wb = w_at(c0), w_at(c1)
                    for zz, way in ((RZ1, -Z), (RZ0, Z)):           # the recess's head and its floor
                        m.add([Vector((c0, hand * (wa - RD), zz)), Vector((c1, hand * (wb - RD), zz)), Vector((c1, hand * wb, zz)), Vector((c0, hand * wa, zz))],
                              [(0, 1, 2, 3)], "hull" if way.z < 0 else "deck", False, lambda q, w_=way: w_)
        else:
            cap.slab(m, plan_of(False), z_lo, fd, paint="hull", top=None)
    if fine:
        # the boats in their recesses: on chocks, held by a pair of davits from the recess's back, their falls rove
        for hand in (PORT, STBD):
            for r_ in recesses[hand]:
                w = w_at(r_)
                by = hand * (w - RD + 1.45)
                yard.boat(r_, by, RZ0 + 0.45, 8.6)
                yard.on(r_)
                for dx in (-2.4, 2.4):
                    m.box((r_ + dx, by, RZ0 + 0.25), (0.3, 1.7, 0.5), "upper")
                for dx in (-3.9, 3.9):
                    foot = Vector((r_ + dx, hand * (w - RD + 0.14), RZ0))
                    knee = foot + Z * (RZ1 - RZ0 - 1.0)
                    head = Vector((r_ + dx, by, RZ1 - 0.4))
                    m.rod(foot, knee, 0.1, "upper", sides=6)
                    m.rod(knee, knee.lerp(head, 0.5) + Z * 0.25, 0.09, "upper", sides=6)
                    m.rod(knee.lerp(head, 0.5) + Z * 0.25, head, 0.08, "upper", rb=0.07, sides=6)
                    m.lathe(head - Z * 0.12, Z, [(0.0, 0.12), (0.12, 0.12)], "steel", 6)
                    yard.wire(head - Z * 0.12, Vector((r_ + dx * 0.92, by, RZ0 + 0.45 + 1.05)), 0.025)
                    m.box((r_ + dx, hand * (w - RD + 0.06), RZ0 + 0.3), (0.5, 0.12, 0.6), "upper")
                m.add([Vector((r_ - 0.45, hand * (w - RD + 0.02), RZ0 + 0.1)), Vector((r_ + 0.45, hand * (w - RD + 0.02), RZ0 + 0.1)),
                       Vector((r_ + 0.45, hand * (w - RD + 0.02), RZ0 + 2.0)), Vector((r_ - 0.45, hand * (w - RD + 0.02), RZ0 + 2.0))],
                      [(0, 1, 2, 3)], "door", False, lambda q, s_=hand: Vector((0, s_, 0)))
        # along the walls, bay by bay: shuttered openings in the upper hangar's side, a few open; below them in the
        # lower hangar big framed openings by turns with scuttles, a door at the upper deck's level here and there;
        # frames up the plating between the bays; the strakes where her upper deck and upper hangar's deck run
        x, k = hx0 - 6.0, 0
        while x > hx1 + 5.0:
            for hand in (PORT, STBD):
                if hand == STBD and xf_ - 8.2 < x < xf_ + 8.2:
                    continue
                m_ = yard.on(x)
                w = w_at(x)
                yy = hand * (w + 0.03)
                open_bay = (k + (2 if hand == STBD else 0)) % 6 == 3
                m_.box((x, yy, 16.5), (5.6, 0.06, 2.1), "black" if open_bay else "door")
                if not open_bay:
                    for j in range(1, 5):
                        m_.box((x, yy + hand * 0.02, 15.45 + j * 0.42), (5.5, 0.03, 0.04), "seam")
                below = not in_recess(hand, x, 2.8)
                if below and (k + (1 if hand == STBD else 0)) % 2 == 1:
                    # a big opening in the lower hangar, dark behind its frame, three lights to it
                    m_.box((x, hand * (w + 0.02), 12.2), (4.4, 0.05, 2.2), "black")
                    for zz in (13.42, 10.98):
                        m_.box((x, hand * (w + 0.1), zz), (4.9, 0.2, 0.24), "hull")
                    for s_ in (-1, 1):
                        m_.box((x + s_ * 2.33, hand * (w + 0.1), 12.2), (0.26, 0.2, 2.68), "hull")
                        m_.box((x + s_ * 0.73, hand * (w + 0.06), 12.2), (0.09, 0.12, 2.2), "hull")
                    m_.box((x, hand * (w + 0.22), 13.62), (5.3, 0.4, 0.07), "hull")      # its eyebrow
                elif below:
                    for dx in (-3.0, 0.0, 3.0):
                        yard.on(x + dx).disc((x + dx, hand * (w_at(x + dx) + 0.01), 12.4), (0, hand, 0), 0.18, "glass", 8)
                    if k % 4 == 0:
                        dz = D(x)
                        m_.add([Vector((x - 0.45, hand * (w + 0.02), dz + 0.15)), Vector((x + 0.45, hand * (w + 0.02), dz + 0.15)),
                                Vector((x + 0.45, hand * (w + 0.02), dz + 1.95)), Vector((x - 0.45, hand * (w + 0.02), dz + 1.95))],
                               [(0, 1, 2, 3)], "door", False, lambda q, s_=hand: Vector((0, s_, 0)))
                        m_.box((x + 0.3, hand * (w + 0.06), dz + 1.05), (0.08, 0.08, 0.08), "steel")
                for dx in (-3.05, 3.05, -4.6):                     # the frames up her plating, between the bays
                    xx = x + dx
                    if in_recess(hand, xx, 0.3) or (hand == STBD and xf_ - 8.0 < xx < xf_ + 8.0) or not hx1 + 1.0 < xx < hx0 - 1.0:
                        continue
                    yard.on(xx).box((xx, hand * (w_at(xx) + 0.035), (D(xx) + 0.75 + zg - 0.12) * 0.5), (0.2, 0.07, zg - 0.12 - D(xx) - 0.75), "hull")
            x -= 9.2
            k += 1
        for hand in (PORT, STBD):
            for a_, b_ in ((hx1 + 0.4, bx), (bx, hx0 - 0.4)):
                xs = [lerp(a_, b_, k_ / 12.0) for k_ in range(13)]
                m.use(yard.half((a_ + b_) * 0.5))
                for zz in (14.6, D(0.0) + 0.6):
                    m.strip([Vector((x, hand * (w_at(x) + 0.012), zz)) for x in xs], [Vector((0, hand, 0))] * 13, 0.05, "seam", 0.006)
        # the boat booms, swung in and stowed along her side under the upper deck's edge, and her accommodation
        # ladders, stowed on edge against her side with the platform at their head by a door
        for hand, (b0, b1), (l0, l1) in ((PORT, (21.0, 33.5), (44.0, 52.5)), (STBD, (40.0, 52.5), (55.0, 63.5))):
            zb_ = D(b0) - 0.9
            pa, na = h.side(b0, zb_, hand)
            pb, nb = h.side(b1, zb_, hand)
            yard.on(b0)
            m.rod(pa + na * 0.32, pb + nb * 0.32, 0.14, "upper", rb=0.09, sides=8)
            m.box(pa + na * 0.16, (0.5, 0.32, 0.4), "steel")                       # its gooseneck
            for t in (0.45, 0.9):
                p_, n_ = h.side(lerp(b0, b1, t), zb_, hand)
                m.rod(p_ + n_ * 0.02 - Z * 0.2, p_ + n_ * 0.32 - Z * 0.05, 0.05, "steel", sides=4)
            zl = D(l0) - 1.25
            p0, n0 = h.side(l0, zl, hand)
            p1, n1 = h.side(l1, zl, hand)
            o0, o1 = p0 + n0 * 0.3, p1 + n1 * 0.3
            yard.on(l0)
            for dz in (-0.38, 0.38):                                                 # its two strings, on edge
                m.box((o0 + o1) * 0.5 + Z * dz, ((o1 - o0).length, 0.08, 0.22), "upper", yaw=math.degrees(math.atan2(o1.y - o0.y, o1.x - o0.x)))
            for j in range(1, 30):
                q = o0.lerp(o1, j / 30.0)
                m.rod(q - Z * 0.36, q + Z * 0.36, 0.025, "steel", sides=3, smooth=False)
            for q, n_ in ((p0, n0), (p1, n1)):                                       # the brackets it hangs in
                m.box(q + n_ * 0.17, (0.22, 0.34, 1.0), "steel", yaw=math.degrees(math.atan2(n_.y, n_.x)) - 90.0)
            # the platform at its head, folded up against her side, and the door it serves
            dz = D(l1)
            p2, n2 = h.side(l1 + 1.2, dz - 0.4, hand)
            m.box(p2 + n2 * 0.12 + Z * 0.1, (1.4, 0.12, 1.1), "upper")
            wl = w_at(l1 + 1.2)
            m.add([Vector((l1 + 0.75, hand * (wl + 0.02), dz + 0.15)), Vector((l1 + 1.65, hand * (wl + 0.02), dz + 0.15)),
                   Vector((l1 + 1.65, hand * (wl + 0.02), dz + 1.95)), Vector((l1 + 0.75, hand * (wl + 0.02), dz + 1.95))],
                  [(0, 1, 2, 3)], "door", False, lambda q, s_=hand: Vector((0, s_, 0)))
        # the hangars' fore and after walls: doors and scuttles
        for xw, way in ((hx0, 1.0), (hx1, -1.0)):
            for yy in (-6.0, 0.0, 6.0):
                yard.on(xw).box((xw + way * 0.03, yy, D(xw) + 1.0), (0.06, 1.0, 1.9), "door")
            for yy in (-8.5, -3.0, 3.0, 8.5):
                for zz in (12.6, 16.8):
                    yard.on(xw).disc((xw + way * 0.012, yy, zz), (way, 0, 0), 0.18, "glass", 8)

    # ---- the galleries along her sides under the deck's edge, on knees, railed; where sponsons stand they give way
    for hand in (PORT, STBD):
        segs, a = [], hx1 + 1.0
        for s0, s1 in sorted(stops[hand]):
            if s1 <= a:
                continue
            segs.append((a, s0))
            a = max(a, s1)
        segs.append((a, hx0 - 2.0))
        cut = []
        for a, b in segs:
            if a < bx < b:
                cut += [(a, bx), (bx, b)]
            else:
                cut.append((a, b))
        for a, b in cut:
            if b - a < 1.0:
                continue
            k_ = max(2, int((b - a) / 4.0))
            xs = [lerp(a, b, i / k_) for i in range(k_ + 1)]
            m.use(yard.half((a + b) * 0.5))
            strip = [Vector((x, hand * (w_at(x) + 0.66), zg)) for x in xs]
            m.strip(strip, [Z] * len(strip), 0.66, "deck", 0.0)
            m.strip([p - Z * 0.1 for p in strip], [-Z] * len(strip), 0.66, "upper", 0.0)
            edge = [Vector((x, hand * (w_at(x) + 1.32), zg)) for x in xs]
            m.strip([p - Z * 0.05 for p in edge], [Vector((0, hand, 0))] * len(edge), 0.05, "upper", 0.0)
            if fine:
                railing(m, [Vector((x, hand * (w_at(x) + 1.28), zg)) for x in xs], high=1.0, every=2.2)
                for x in xs[::2]:
                    m.rod((x, hand * w_at(x), zg - 1.6), (x, hand * (w_at(x) + 1.2), zg - 0.1), 0.06, "upper", sides=4)
    if fine:                                              # knees under the deck's overhang
        x = 94.0
        while x > hx1 + 2.0:
            for hand in (PORT, STBD):
                e = abs(cap.edge_at(KAGA_PORT, x))
                w0 = w_at(x)
                if e - w0 > 0.4 and not any(s0 - 0.5 < x < s1 + 0.5 for s0, s1 in stops[hand]):
                    yard.on(x).rod((x, hand * w0, fd - 1.5), (x, hand * (e - 0.2), fd - 0.04), 0.08, "upper", sides=4)
            x -= 6.0

    # ---- the flight deck: planked, with its edge plating and underside; its beams where it is open beneath
    for x0, x1, part in ((bx, DECK_FORE, "fore"), (DECK_AFT, bx, "aft")):
        m.use(part)
        cap.slab(m, cap.cut_deck(KAGA_PORT, KAGA_STAR, x0, x1, every=4.0 if fine else 12.0), fd, iz, paint="upper", top="planks", bottom=True)
    if fine:
        for x in [hx0 + 1.5 + 3.0 * k for k in range(8)] + [hx1 - 1.5 - 3.6 * k for k in range(10)]:
            if DECK_AFT + 1.0 < x < DECK_FORE - 1.0:
                e = min(abs(cap.edge_at(KAGA_PORT, x)), abs(cap.edge_at(KAGA_STAR, x)))
                yard.on(x).box((x, 0.0, fd - 0.25), (0.3, 2.0 * e - 0.8, 0.5), "upper")
        for hand in (PORT, STBD):                          # the edge girders under it there
            for a, b in ((hx0, DECK_FORE - 1.0), (DECK_AFT + 2.0, hx1)):
                xs = [lerp(a, b, i / 6.0) for i in range(7)]
                for p, q in zip(xs, xs[1:]):
                    e0, e1 = abs(cap.edge_at(KAGA_PORT, p)) - 0.5, abs(cap.edge_at(KAGA_PORT, q)) - 0.5
                    yard.on((p + q) * 0.5).add([Vector((p, hand * e0, fd)), Vector((q, hand * e1, fd)), Vector((q, hand * e1, fd - 0.8)), Vector((p, hand * e0, fd - 0.8))],
                                               [(0, 1, 2, 3)], "upper", False, lambda _, s=hand: Vector((0, s, 0)))

    # ---- its posts over the open forecastle, two pairs braced together and a slender pair at the bow
    for px in (104.0, 111.0):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.82, 7.2)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.08, fd - 0.05), 0.36, "upper", rb=0.3, sides=10 if fine else 5)
    if fine:
        for hand in (PORT, STBD):
            y0, y1 = hand * min(h.half_at(104.0) * 0.82, 7.2), hand * min(h.half_at(111.0) * 0.82, 7.2)
            for (pa, ya, za), (pb, yb, zb) in (((104.0, y0, D(104.0) + 0.6), (111.0, y1 * 1.08, fd - 1.0)), ((111.0, y1, D(111.0) + 0.6), (104.0, y0 * 1.08, fd - 1.0))):
                yard.on(107.5).rod((pa, ya, za), (pb, yb, zb), 0.13, "upper", sides=6)
            yard.on(107.5).rod((104.0, y0 * 1.04, (D(104.0) + fd) * 0.5), (111.0, y1 * 1.04, (D(111.0) + fd) * 0.5), 0.1, "upper", sides=5)
    for hand in (PORT, STBD):
        px = 119.5
        yy = hand * min(h.half_at(px) * 0.7, 3.0)
        yard.on(px).rod((px, yy, D(px)), (px + 0.6, yy * 1.6, fd - 0.05), 0.22, "upper", sides=8 if fine else 5)
    # ---- and over the quarterdeck: tall posts in frames across her, braced, a raking strut under the overhang
    frames = (-95.0, -103.0, -111.0)
    for px in frames:
        for hand in (PORT, STBD):
            yy = hand * h.half_at(px) * 0.9
            yard.on(px).box((px, yy, (D(px) + fd) * 0.5), (0.9, 0.7, fd - D(px)), "hull")
        if fine:
            yard.on(px).box((px, 0.0, fd - 0.39), (0.6, 2.0 * h.half_at(px) * 0.9, 0.8), "upper")
    for hand in (PORT, STBD):
        y0, y1 = hand * h.half_at(-103.0) * 0.9, hand * h.half_at(-111.0) * 0.9
        m.use("aft")
        for (pa, ya, za), (pb, yb, zb) in (((-103.0, y0, D(-103.0) + 1.2), (-111.0, y1, fd - 1.4)), ((-111.0, y1, D(-111.0) + 1.2), (-103.0, y0, fd - 1.4))):
            m.rod((pa, ya, za), (pb, yb, zb), 0.22, "upper", sides=6 if fine else 4)
        ys = hand * h.half_at(-115.0) * 0.8
        m.rod((-115.0, ys, D(-115.0)), (-123.0, hand * 9.0, fd - 0.05), 0.32, "upper", sides=8 if fine else 5)
        m.rod((-115.0, ys * 0.4, D(-115.0)), (-123.5, hand * 3.5, fd - 0.05), 0.28, "upper", sides=8 if fine else 5)
        # the bulwark round the quarterdeck and the fascia under the deck's edge between the posts
        xs = [lerp(hx1, -121.0, k / 10.0) for k in range(11)]
        for i in range(len(xs) - 1):
            a, b = xs[i], xs[i + 1]
            ya_, yb_ = hand * h.half_at(a) * 0.985, hand * h.half_at(b) * 0.985
            q = [Vector((a, ya_, D(a))), Vector((b, yb_, D(b))), Vector((b, yb_, D(b) + 1.1)), Vector((a, ya_, D(a) + 1.1))]
            m.add(q, [(0, 1, 2, 3)], "hull", False, lambda _, s=hand: Vector((0, s, 0)))
            m.add(q, [(0, 1, 2, 3)], "hull", False, lambda _, s=hand: Vector((0, -s, 0)))
            ea, eb = abs(cap.edge_at(KAGA_PORT, a)) - 0.05, abs(cap.edge_at(KAGA_PORT, b)) - 0.05
            q = [Vector((a, hand * ea, fd - 1.3)), Vector((b, hand * eb, fd - 1.3)), Vector((b, hand * eb, fd)), Vector((a, hand * ea, fd))]
            m.add(q, [(0, 1, 2, 3)], "hull", False, lambda _, s=hand: Vector((0, s, 0)))
            m.add(q, [(0, 1, 2, 3)], "hull", False, lambda _, s=hand: Vector((0, -s, 0)))

    # ---- the lifts' places, the lines, the wires and barriers, the wind screens, the round-down's stripes, her kana
    lift_gap = [(lx - ll * 0.5 - 1.0, lx + ll * 0.5 + 1.0) for lx, ly, ll, lw, _ in lifts]

    def along(a, b, y, half_w, paint, lift=0.025):
        for p0, p1 in ((a, min(b, bx)), (max(a, bx), b)):
            if p1 - p0 < 0.2:
                continue
            k = max(1, int((p1 - p0) / 8.0))
            pts = [Vector((lerp(p0, p1, i / k), y, iz)) for i in range(k + 1)]
            m.use(yard.half((p0 + p1) * 0.5))
            m.strip(pts, [Z] * len(pts), half_w, paint, lift)
    x = DECK_FORE - 4.0
    while x > -104.0:                                     # the dashed centre line
        if not any(g0 - 2.6 < x < g1 + 2.6 for g0, g1 in lift_gap):
            along(x - 2.5, x + 2.5, 0.0, 0.2, "line")
        x -= 8.0
    for hand in (PORT, STBD):                             # the two side lines, whole, broken at the lifts
        pieces, last = [], -104.0
        for g0, g1 in sorted(lift_gap):
            pieces.append((last, g0))
            last = g1
        pieces.append((last, 110.0))
        for a, b in pieces:
            if b - a > 0.5:
                along(a, b, hand * 7.5, 0.15, "line")
    for lx, ly, ll, lw, _ in lifts:                       # a white line across her before and abaft each lift
        for s in (-1, 1):
            xx = lx + s * (ll * 0.5 + 0.6)
            m.use(yard.half(xx))
            m.strip([Vector((xx, -9.0, iz)), Vector((xx, 9.0, iz))], [Z, Z], 0.12, "line", 0.025)
    m.use("aft")
    wires = (-44.0, -50.0, -56.0, -80.0, -86.0, -92.0, -98.0, -104.0)
    for xw in wires:                                      # the arresting wires on their sheaves
        w_ = min(abs(cap.edge_at(KAGA_PORT, xw)), abs(cap.edge_at(KAGA_STAR, xw))) - 2.0
        m.use(yard.half(xw))
        m.rod((xw, -w_, iz + 0.12), (xw, w_, iz + 0.12), 0.035, "steel", sides=3, smooth=False)
        if fine:
            for hand in (PORT, STBD):
                m.lathe((xw, hand * w_, iz), Z, [(0.0, 0.22), (0.18, 0.18)], "steel", 8)
                m.box((xw, hand * (w_ + 0.6), iz + 0.06), (0.4, 0.8, 0.12), "steel")
    m.use("fore")
    for xb in (12.0, 17.0):                               # the crash barriers, lowered
        m.rod((xb, -11.3, iz + 0.06), (xb, 11.3, iz + 0.06), 0.05, "steel", sides=4, smooth=False)
        for hand in (PORT, STBD):
            m.box((xb, hand * 11.4, iz + 0.25), (0.4, 0.4, 0.5), "steel")
    if fine:                                              # the wind screens before the forward lift, lying flat
        for j in range(-8, 9):
            m.box((70.2, j * 1.05, iz + 0.04), (0.9, 0.95, 0.06), "steel")
        m.rod((69.75, -9.0, iz + 0.07), (69.75, 9.0, iz + 0.07), 0.05, "steel", sides=4)
        # the steel strips across her planking that her aircraft were lashed to, broken at the lifts; chocks, a bomb
        # trolley and a fuel cart by the island
        xt = DECK_FORE - 6.0
        while xt > -119.0:
            if not any(abs(xt - lx) < ll * 0.5 + 1.0 for lx, ly, ll, lw, _ in lifts) and not 66.5 < xt < 71.0:
                e = min(abs(cap.edge_at(KAGA_PORT, xt)), abs(cap.edge_at(KAGA_STAR, xt))) - 0.6
                m.use(yard.half(xt))
                m.strip([Vector((xt, -e, iz)), Vector((xt, e, iz))], [Z, Z], 0.05, "seam", 0.018)
            xt -= 3.6
        for cx_, cy_, a_ in ((22.5, -11.6, 15.0), (24.2, -12.3, -30.0), (38.6, -12.0, 70.0)):
            m.use(yard.half(cx_))
            ca, sa = math.cos(math.radians(a_)), math.sin(math.radians(a_))
            for s_ in (-0.3, 0.3):
                m.box((cx_ - sa * s_, cy_ + ca * s_, iz + 0.11), (0.3, 0.18, 0.2), "lino", yaw=a_)
            m.rod((cx_ - sa * 0.3, cy_ + ca * 0.3, iz + 0.12), (cx_ - sa * 0.3 - ca * 0.9, cy_ + ca * 0.3 - sa * 0.9, iz + 0.04), 0.012, "rope", sides=3)
        bomb_cart(m.use(yard.half(26.5)), Vector((26.5, -11.2, iz)), 20.0)
        fuel_cart(m.use(yard.half(19.8)), Vector((19.8, -12.6, iz)), -10.0)
    m.use("aft")
    aft_edge = [(y, x) for x, y in KAGA_PORT[:7]]          # (the after edge: x where the deck ends, by y)
    for k in range(11):                                   # the red and white stripes on her round-down, fore and aft
        yy = (k - 5) * 2.4
        x0 = cap.edge_at(aft_edge, abs(yy) + 1.2) + 0.25
        if x0 < -121.0:
            m.box(((x0 - 121.0) * 0.5, yy, iz + 0.025), (-121.0 - x0, 2.4 if k % 2 == 0 else 2.38, 0.03), "red" if k % 2 == 0 else "white")
    kana_ka(m, -113.0, 0.0, iz, 8.0)

    # ---- the island, well forward on her starboard side
    gaff, mast_head = island(yard, xi_f, xi_a, -15.25, -21.0, crew=fine)

    # ---- the great funnel to starboard amidships
    great_funnel(yard, xf_, -w_at(xf_) + 0.05, FD - 0.22 - 2.9, rx=5.6, rz=2.9, out=1.4, bend=4.0, down=62.0, run=1.6)

    # ---- ten 20 cm in casemates in her after hull: the three of 1928 low down, the two from her turrets a deck higher
    n_p = n_s = 0
    for cx, cz in ((-38.0, 7.7), (-50.0, 7.7), (-66.0, 5.4), (-78.0, 5.4), (-90.0, 5.4)):
        for hand in (PORT, STBD):
            if hand == PORT:
                n_p += 1
                nm = f"sec.P{n_p}"
            else:
                n_s += 1
                nm = f"sec.S{n_s}"
            casemate20(yard, nm, cx, cz, hand)

    # ---- eight twin 12.7 cm high-angle, one forward and three aft a side, on sponsons raised (in 1934-35) to the flight
    # deck's level, so that the guns stand clear above it and could fire across it; a ladder up from the gallery to each
    def gallery_free(hand, a, b, own):
        a, b = min(a, b), max(a, b)
        return hx1 + 1.0 < a and b < hx0 - 2.0 and not any(s0 < b and a < s1 for s0, s1 in stops[hand] if not s0 < own < s1)

    def stair_up(hand, gx, r, w0, z_top):
        for s_ in ((1, -1) if gx < 0 else (-1, 1)):
            far, near = gx + s_ * (r + 1.7), gx + s_ * (r - 1.15)
            if gallery_free(hand, gx + s_ * (r + 0.1), far + s_ * 0.3, gx):
                sponson_stair(yard, far, near, hand * (w_at(gx) + 0.85), zg, z_top)
                return
    for hand in (PORT, STBD):
        for gx in h127[hand]:
            w0 = w_at(gx)
            yy = hand * (w0 + 2.95)
            gun_sponson(yard, gx, yy, ZH, 3.2, hand * w0)
            stair_up(hand, gx, 3.2, w0, ZH)
            if hand == PORT:
                n_p += 1
                nm = f"sec.P{n_p}"
                rest = -60.0 if gx > 0 else -120.0
                lim = (-140, 100) if gx > 0 else (-80, 160)
            else:
                n_s += 1
                nm = f"sec.S{n_s}"
                rest = 60.0 if gx > 0 else 120.0
                lim = (-100, 140) if gx > 0 else (-160, 80)
            twin127(yard, nm, gx, yy, ZH, rest, lim, crew=True)
    # their two high-angle directors, on sponsons of their own at the same height, to see over the deck
    for hand, name in ((PORT, "director.sec.port"), (STBD, "director.sec.starboard")):
        for gx in dirs[hand]:
            w0 = w_at(gx)
            yy = hand * (w0 + 2.3)
            gun_sponson(yard, gx, yy, ZH, 2.4, hand * w0)
            stair_up(hand, gx, 2.4, w0, ZH)
            director91(yard, name, gx, yy, ZH, bearing=hand * -90.0)

    # ---- eleven twin 25 mm in tubs on sponsons along her galleries, each resting toward its own end of her and
    # training only outboard; the six Type 95 directors that lay them, three a side, a step above the gallery
    q = 0
    for hand in (PORT, STBD):
        for gx in a25[hand]:
            q += 1
            w0 = w_at(gx)
            yy = hand * (w0 + 2.45)
            gun_sponson(yard, gx, yy, zg - 0.1, 2.45, hand * w0, rail=False)
            lean_ = (30.0 if gx > 0 else -30.0)
            twin25(yard, q, gx, yy, zg - 0.1, bearing=hand * (-90.0) + (lean_ if hand == PORT else -lean_), tub=2.0, limits=(-100, 100))
    q = 0
    for hand in (PORT, STBD):
        for gx in d95[hand]:
            q += 1
            w0 = w_at(gx)
            step_platform(yard, gx, hand, w0, 2.75, 1.5, zg + 0.35)
            director95(yard, f"director.aa.{q}", gx, hand * (w0 + 1.75), zg + 0.35, hand * -90.0 + (20.0 if gx > 0 else -20.0) * hand, crew=fine)

    # ---- six 6.5 mm machine guns at the galleries' rails, three a side
    for hand in (PORT, STBD):
        for gx in mgs[hand]:
            w0 = w_at(gx)
            mg65(yard, gx, hand * (w0 + 1.0), zg, hand * 90.0 + (25.0 if gx < 0 else -25.0) * hand, crew=fine)

    # ---- searchlights on platforms at the galleries' fore ends, and one each side under the overhang aft
    for hand in (PORT, STBD):
        for gx in lights[hand]:
            w0 = w_at(gx)
            yy = hand * (w0 + 1.6)
            gun_sponson(yard, gx, yy, zg, 1.7, hand * w0)
            yard.searchlight(gx, yy, zg, 0.55, (0.6, hand * 0.8, 0.05))
        gx = -95.0                                        # (built out from the foremost post of the stern's frames)
        wy = h.half_at(gx) * 0.9 + 0.35
        yy = hand * (wy + 1.4)
        gun_sponson(yard, gx, yy, zg - 1.0, 1.5, hand * wy)
        yard.searchlight(gx, yy, zg - 1.0, 0.55, (-0.5, hand * 0.85, 0.05))

    # ---- the radio masts, lowered outboard for flying, on their hinges at the galleries
    for hand in (PORT, STBD):
        for mxx in masts[hand]:
            w0 = w_at(mxx)
            foot = Vector((mxx, hand * (w0 + 1.4), zg + 0.25))
            tip = foot + Vector((0.0, hand * 12.5, 1.2))
            m_ = yard.on(mxx)
            m_.box(foot - Vector((0, hand * 0.25, 0.05)), (1.0, 0.5, 0.5), "steel")
            for dx in (-0.35, 0.35):
                m_.rod(foot + X * dx, tip + X * dx * 0.3, 0.07, "upper", sides=4)
            if fine:
                for k in range(1, 12):
                    a = foot.lerp(tip, k / 12.0)
                    m_.rod(a - X * 0.33 * lerp(1.0, 0.3, k / 12.0), a + X * 0.33 * lerp(1.0, 0.3, k / 12.0), 0.025, "upper", sides=3, smooth=False)
                yard.wire(tip, tip + Vector((0, hand * 1.5, -0.4)), 0.02)

    # ---- boats on chocks on the quarterdeck under the overhang
    for bx_, by_ in ((-100.0, 6.0), (-108.5, 6.2)):
        for hand in (PORT, STBD):
            zb = D(bx_) + 0.55
            yard.boat(bx_, hand * by_, zb, 8.6 if bx_ > -105.0 else 7.4)
            if fine:
                for t in (-0.3, 0.3):
                    yard.on(bx_).box((bx_ + t * 7.5, hand * by_, D(bx_) + 0.3), (0.35, 1.6, 0.6), "upper")

    # ---- her flight deck's crew: handlers by the range aft and forward, officers and runners by the island
    if fine:
        rg = random.Random(1941)

        def on_lift(p):
            return any(abs(p.x - lx) < ll * 0.5 + 0.6 and abs(p.y - ly) < lw * 0.5 + 0.6 for lx, ly, ll, lw, _ in lifts)
        for cx, cy, k_ in ((-48.0, 0.0, 6), (-84.0, 9.0, 3), (45.0, 9.0, 3), (24.0, -11.5, 4), (75.0, -4.0, 2), (5.0, 6.0, 2)):
            placed = 0
            while placed < k_:
                a = rg.uniform(0, 2 * math.pi)
                foot = Vector((cx + rg.uniform(-5.0, 5.0), cy + rg.uniform(-3.0, 3.0), iz + 0.035))
                if on_lift(foot) or abs(foot.y) > 13.5:
                    continue
                fit.sailor(m.use(yard.half(foot.x)), foot, Vector((math.cos(a), math.sin(a), 0)), "stand", navy="JAP")
                placed += 1

    # ---- forecastle and quarterdeck: anchor gear, bollards, rails, scuttles; staffs; screws and rudder
    n_rust = len(yard.rust_at)
    yard.anchor_gear(116.0, 106.5, spread=2.4)
    del yard.rust_at[n_rust:]      # (a weep down the flare under each hawse shows as a hard light block on so few stations)
    for bx_ in (118.5, 113.5, -116.0, -120.5):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.8), D(bx_))
    yard.rails(hx0 + 1.5, h.L * 0.488)
    yard.rails(-h.L * 0.492, -121.5)
    skip = [(cx - 3.2, cx + 3.2) for cx in (-38.0, -50.0, -66.0, -78.0, -90.0)]
    yard.portholes(-h.L * 0.4, h.L * 0.43, 2.2, every=3.4, skip=skip)
    yard.portholes(-h.L * 0.38, h.L * 0.42, 4.6, every=3.4, skip=skip)
    m.use("aft")
    sx = -h.L * 0.497
    m.rod((sx + 0.6, 0, D(sx + 0.6)), (sx - 0.4, 0, D(sx + 0.6) + 5.0), 0.07, "upper", sides=4)
    m.use("fore")
    jx = h.L * 0.493
    m.rod((jx, 0, D(jx)), (jx + 0.4, 0, D(jx) + 3.0), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.4, 0, D(jx) + 3.0), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=4, r=2.2, rudders=1)
    # her sternpost: the line her two sides close on, faired over with a round bar from her forefoot aft to her deck
    m.use("aft")
    post = [h.point(-1.0, w, PORT) for w in ([-0.6, -0.3, -0.12, -0.04, 0.0, 0.05, 0.12, 0.3, 0.5, 0.7, 0.85, 0.97] if fine else [-0.3, 0.0, 0.5, 0.97])]
    for pa, pb in zip(post, post[1:]):
        mid_z = (pa.z + pb.z) * 0.5
        m.rod(pa, pb, 0.16, "antifoul" if mid_z < -0.27 else ("boot" if mid_z < 0.64 else "hull"), sides=8 if fine else 4)

    hz = iz - D(0.0) - 0.4
    cap.carrier_finish(yard, lifts, DECK_FORE, DECK_AFT, iz, hz, [{"model": "DeckPark_Zero"}], 110.0, -115.0, gaff, "fore")
    yard.fire_points += [(52.0, 0.0, iz), (-20.0, 0.0, iz), (-75.0, 0.0, iz), (27.0, -17.5, iz + 2.0), (xf_, -16.0, zg), (-60.0, 13.0, 12.0)]
    from ships import sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    cap.carrier_clips(m, yard, len(lifts), hz)
    m.info.setdefault("file", {}).update({"class": "Kaga", "navy": "JAP", "year": 1941, "stands_for": "Carrier_JAP",
                                          "paint": "Imperial Navy grey all over; a flight deck of plain wood with white lines, red and white stripes on its after end and her kana KA"})
    return m, yard


BUILDERS = {"Kaga": lambda fine: kaga(fine)}
