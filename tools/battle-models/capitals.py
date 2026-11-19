"""The Pacific's capital ships, each a named class laid out against a measured drawing of the real ship.

    Iowa      America's fast battleship, as she was in 1944-45 (Measure 22: navy blue hull, haze grey above)
    Yamato    Japan's great battleship, as she was in April 1945
    Essex     America's fleet carrier, 1944
    Shokaku   Japan's fleet carrier, 1942

These are the hero ships of the Pacific battles, held to the bar of the B-17 (Brian, 2026-11-19: "use this 3d model
is our bench mark"): laid out against measured drawings, every gun position the real ship had there and manned,
and the fittings a close camera finds. The drawings were measured, not traced, and nothing of any drawing or model
is used: every shape is our own, made from numbers. Where the drawings disagree on a height, the US Navy's own
recognition figures win (ONI 54-B for the Iowa: bow 36 ft, stern 22 ft, bridge 65 ft, stack 98 ft, mast 134 ft).

Each file has the parts, sockets and clips every ship has (see the README's "Ships"), so a battle can use one where it
used the plain ship of her navy: `Iowa` for `Battleship_USA`, `Yamato` for `Battleship_JAP`, `Essex` for
`Carrier_USA`, `Shokaku` for `Carrier_JAP`. What they add: their light guns are mountings that train
(`bofors.N`, `aa25.N`, each with `.guns`), their crews stand at every gun, and their directors and radars turn.
"""

import math

from mathutils import Matrix, Vector
from mathutils.geometry import tessellate_polygon

import fittings as fit
from rigid import PORT, STBD, X, Y, Z, Model, lerp
from shipyard import Hull, Yard, livery, weather_for


# ---- a hull laid to a measured plan --------------------------------------------------------------------------

class PlanHull(Hull):
    """A hull whose deck edge follows a table measured off a plan: (x from her middle, half width as a share of half
    her beam). Everything else (the run of her bottom, her flare, her stem and stern) is the yard's hull's."""

    def __init__(self, table, decks=None, **kw):
        super().__init__(**kw)
        self.table = sorted(table)
        self.decks = sorted(decks) if decks else None      # (x, deck height): her deck's line off a profile, where it waves

    def deck_z(self, x):
        if not self.decks:
            return super().deck_z(x)
        t = self.decks
        if x <= t[0][0]:
            return t[0][1]
        if not hasattr(self, "_dz"):
            # (a monotone cubic through the table's points: no overshoot, and no flat step at each point)
            d = [(z1 - z0) / (x1 - x0) for (x0, z0), (x1, z1) in zip(t, t[1:])]
            dz = [d[0]] + [0.0 if a * b <= 0 else 2.0 / (1.0 / a + 1.0 / b) for a, b in zip(d, d[1:])] + [d[-1]]
            self._dz = dz
        for i, ((x0, z0), (x1, z1)) in enumerate(zip(t, t[1:])):
            if x <= x1:
                hh = x1 - x0
                u = (x - x0) / hh
                h00, h10, h01, h11 = 2 * u ** 3 - 3 * u ** 2 + 1, u ** 3 - 2 * u ** 2 + u, -2 * u ** 3 + 3 * u ** 2, u ** 3 - u ** 2
                return h00 * z0 + h10 * hh * self._dz[i] + h01 * z1 + h11 * hh * self._dz[i + 1]
        return t[-1][1]

    def share(self, x):
        t = self.table
        if x <= t[0][0]:
            return t[0][1]
        for (x0, s0), (x1, s1) in zip(t, t[1:]):
            if x <= x1:
                return lerp(s0, s1, (x - x0) / (x1 - x0))
        return t[-1][1]

    def beam(self, u):
        x = self.frame_x(u) + self.lean(u)
        return self.B * 0.5 * max(0.0, self.share(x))


# ---- houses built to a plan ----------------------------------------------------------------------------------

def sym(port):
    """A closed plan from its port half, bow to stern: [(x, y), ...] with y >= 0. Points on the middle line are not
    doubled. Comes back going round anticlockwise seen from above."""
    star = [(x, -y) for x, y in reversed(port) if y > 1e-6]
    pts = [(x, y) for x, y in port] + star
    out = []
    for p in pts:
        if not out or (abs(out[-1][0] - p[0]) > 1e-6 or abs(out[-1][1] - p[1]) > 1e-6):
            out.append(p)
    a = sum(out[i][0] * out[(i + 1) % len(out)][1] - out[(i + 1) % len(out)][0] * out[i][1] for i in range(len(out)))
    return out if a > 0 else list(reversed(out))


def slab(m, plan, z0, z1, paint="upper", top="deck", bottom=False):
    """A plan raised from z0 to z1 with upright walls, each facing out, and a flat top laid over it (a plan may have
    bays and bulges: its top is cut into triangles that follow them)."""
    # (points in a straight line with their neighbours are dropped: the top's triangles would leave them out)
    keep = []
    for k, p in enumerate(plan):
        a_, c_ = plan[k - 1], plan[(k + 1) % len(plan)]
        cross = (p[0] - a_[0]) * (c_[1] - p[1]) - (p[1] - a_[1]) * (c_[0] - p[0])
        if abs(cross) > 1e-6 * max(1.0, math.hypot(c_[0] - a_[0], c_[1] - a_[1])):
            keep.append(p)
    plan = keep
    n = len(plan)
    lo = [Vector((p[0], p[1], z0)) for p in plan]
    hi = [Vector((p[0], p[1], z1)) for p in plan]
    for k in range(n):
        a, b = plan[k], plan[(k + 1) % n]
        out = Vector((b[1] - a[1], -(b[0] - a[0]), 0.0))
        if out.length < 1e-9:
            continue
        m.add([lo[k], lo[(k + 1) % n], hi[(k + 1) % n], hi[k]], [(0, 1, 2, 3)], paint, False, lambda q, o=out: o)
    tris = tessellate_polygon([hi])
    if top:
        m.add(hi, [tuple(t) for t in tris], top, False, lambda q: Z)
    if bottom:
        m.add(lo, [tuple(t) for t in tris], paint, False, lambda q: -Z)


def walls_detail(yard, plan, z0, z1, doors=True, scuttles=True, rail=True, skip=()):
    """What a deckhouse's walls show close to: doors, a row of scuttles, the rail round its top."""
    m = yard.m
    if not m.fine:
        return
    n = len(plan)
    from ships import railing
    for k in range(n):
        a, b = Vector((*plan[k], 0.0)), Vector((*plan[(k + 1) % n], 0.0))
        d = b - a
        L = d.length
        if L < 1.0:
            continue
        out = Vector((d.y, -d.x, 0.0)).normalized()
        along = d.normalized()
        if rail:
            railing(m, [a + Z * z1 - out * 0.12, b + Z * z1 - out * 0.12], high=1.0, every=1.9)
        if z1 - z0 < 2.0 or L < 3.0:
            continue
        steps = int((L - 1.2) / 2.3)
        for i in range(steps):
            p = a + along * (0.6 + (i + 0.5) * (L - 1.2) / steps)
            if any(lo_ <= p.x <= hi_ for lo_, hi_ in skip):
                continue
            yard.on(p.x)
            if doors and i % 5 == 2 and z1 - z0 >= 2.3:
                m.add([p - along * 0.38 + Z * (z0 + 0.15) + out * 0.03, p + along * 0.38 + Z * (z0 + 0.15) + out * 0.03,
                       p + along * 0.38 + Z * (z0 + 1.95) + out * 0.03, p - along * 0.38 + Z * (z0 + 1.95) + out * 0.03], [(0, 1, 2, 3)], "door", False, lambda q, o=out: o)
                m.box(p + along * 0.24 + Z * (z0 + 1.05) + out * 0.06, (0.08, 0.08, 0.08), "steel")
            elif scuttles:
                m.disc(p + Z * (z0 + min(z1 - z0 - 0.6, 1.7)) + out * 0.015, out, 0.17, "glass", 8)


def sponson(yard, x, y, z, r, wall_y, paint="upper"):
    """A round platform jutting from a wall at height z, on brackets: room for a gun tub beside a narrow house."""
    m = yard.on(x)
    m.lathe((x, y, z - 0.18), Z, [(0.0, r), (0.18, r)], paint, 20 if m.fine else 8)
    if m.fine:
        hand = 1.0 if y >= wall_y else -1.0
        for dx in (-r * 0.6, 0.0, r * 0.6):
            m.rod((x + dx, wall_y, z - 1.6), (x + dx, y - hand * r * 0.2, z - 0.2), 0.07, paint, sides=4)


# ---- American main battery and funnels --------------------------------------------------------------------------

def us_turret(yard, name, x, deck, bearing, raised=0.0, barrels=3, floats=True, rangefinder=True, parent=None):
    """An American 16-inch triple turret: a barbette; a gunhouse with its thick face sloped back and its corners cut,
    sides that widen to the rear and a squared back that overhangs the barbette; the rangefinder's hoods out each side
    of its rear; sighting hoods on the roof; three guns in canvas bloomers. Parts `turret.<L>` and its guns, as the
    yard's turrets, with the same sockets."""
    m = yard.m
    half = parent or yard.half(x)
    fine = m.fine
    base = deck + raised
    m.use(half)
    m.lathe((x, 0, deck - 0.6), Z, [(0.0, 5.9), (raised + 1.05, 5.9)], "upper", 32 if fine else 12, caps=(False, True))
    piv = Vector((x, 0, base + 1.0))
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")

    def W(px, py, pz):
        return piv + rot @ Vector((px, py, pz))
    tname = m.part(name, half, tuple(piv), axis=(0, 0, -1), limits=[-150, 150], kind="turn",
                   note=f"trains: plus turns it clockwise seen from above, so its bearing is its rest bearing plus the turn; rest bearing {int(bearing)}; about 4 degrees a second")
    m.parts[m.part_at[tname]].rest = bearing
    m.parts[m.part_at[tname]].rate = 4.0
    m.use(tname)
    H = 3.45
    lo = [(8.3, 3.9), (7.6, 5.4), (4.2, 6.35), (-5.2, 6.35), (-6.6, 5.4), (-6.8, 0.0)]
    hi = [(6.4, 3.7), (5.8, 5.0), (3.4, 6.05), (-5.0, 6.05), (-6.35, 5.2), (-6.55, 0.0)]
    plan_lo = lo[:-1] + [(-6.8, 0.0)] + [(px, -py) for px, py in reversed(lo[:-1])]
    plan_hi = hi[:-1] + [(-6.55, 0.0)] + [(px, -py) for px, py in reversed(hi[:-1])]
    n = len(plan_lo)
    vl = [W(px, py, 0.0) for px, py in plan_lo]
    vh = [W(px, py, H) for px, py in plan_hi]
    mid = W(0.5, 0, H * 0.5)
    m.add(vl + vh, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "upper", False, lambda q: q - mid)
    tris = tessellate_polygon([[Vector((p[0], p[1], 0)) for p in plan_hi]])
    m.add(vh, [tuple(t) for t in tris], "roof", False, lambda q: Z)
    m.add(vl, [tuple(t) for t in tris], "steel", False, lambda q: -Z)
    if fine:
        # the roof's plates, the sighting hoods, the hatches, the rangefinder's hoods, rafts on her sides
        for k in range(1, 6):
            px = 5.8 - k * 2.05
            m.box(W(px, 0, H + 0.005), (0.03, 11.6, 0.012), "seam", yaw=bearing)
        for sy in (-3.6, 0.0, 3.6):
            m.box(W(4.6, sy, H + 0.33), (1.4, 0.95, 0.66), "roof", yaw=bearing)
            m.box(W(5.31, sy, H + 0.36), (0.02, 0.6, 0.18), "glass", yaw=bearing)
        for sy in (-2.0, 2.0):
            m.lathe(W(-4.2, sy, H), Z, [(0.0, 0.45), (0.12, 0.45), (0.18, 0.38)], "roof", 10)
        m.box(W(-6.0, 0, H + 0.25), (0.8, 2.4, 0.5), "roof", yaw=bearing)
        if rangefinder:
            for side in (1, -1):
                m.box(W(-3.9, side * 6.55, H * 0.66), (1.6, 0.9, 1.05), "upper", yaw=bearing)
                m.disc(W(-3.08, side * 6.8, H * 0.66), rot @ X, 0.18, "glass", 8)
        if floats:
            for side in (1, -1):
                for dx in (-1.6, 1.4):
                    c = W(dx, side * 6.36, H * 0.5)
                    yard.float_(c.x, c.y, c.z, rot @ Vector((0, side, 0)), long=2.6, wide=1.15)
        # ladders up her back and rungs on her sides
        from ships import ladder
        back = W(-6.75, 1.2, 0.0)
        ladder(m, back, back.z + H, rot @ Vector((-1, 0, 0)))
    # the guns: lift on their trunnions, run back along their own length
    guns = []
    fwd, lat = rot @ X, rot @ Y
    gap = 3.05
    face_x = 7.2
    for k in range(barrels):
        off = (k - (barrels - 1) / 2.0) * gap
        p = W(5.0, off, 1.7)
        gname = m.part(f"{name}.gun.{k + 1}", tname, tuple(p), axis=tuple(-lat), limits=[-5, 45], slide=tuple(-fwd), slide_limits=[0.0, 1.2], kind="turn",
                       note="a gun: plus lifts the muzzle; slides back along its own length when it fires")
        m.use(gname)
        e = math.radians(2.0)
        ax = (fwd * math.cos(e) + Z * math.sin(e)).normalized()
        reach = face_x - 5.0
        long = 17.8
        prof = [(0.0, 0.62), (reach + 0.4, 0.6), (reach + 2.6, 0.55), (reach + 3.0, 0.5), (reach + long * 0.55, 0.42), (reach + long * 0.93, 0.35),
                (reach + long * 0.95, 0.37), (reach + long, 0.36)]
        m.lathe(p, ax, prof, "upper", 16 if fine else 8, caps=(True, False))
        tip = p + ax * (reach + long)
        m.disc(tip - ax * 0.02, ax, 0.355, "upper", 16 if fine else 8)
        m.disc(tip + ax * 0.005, ax, 0.205, "black", 12 if fine else 6)
        m.lathe(p + ax * (reach - 0.6), ax, [(0.0, 1.05), (0.5, 1.02), (1.3, 0.86), (2.8, 0.62)], "bag", 14 if fine else 6, caps=(False, False))
        m.socket(f"muzzle.{name}.{k + 1}", tip, ax, gname, "flash, smoke and the shell leave here")
        guns.append(gname)
    m.use(half)
    return tname, guns, W, H


def us_funnel(yard, x, z0, top, long, wide, slope=1.6, name=None, lights=True, platform=None, y=0.0):
    """An American fast battleship's funnel: oval in plan with flatter sides, tall, its top cut on a slope (higher
    forward) under a deep cap with a grating over the mouth; steam pipes up its after face, a railed platform with a
    searchlight each side."""
    m = yard.on(x)
    fine = m.fine
    n = 28 if fine else 12

    def plan(t):
        a_, b_ = long * 0.5 * lerp(1.0, 0.94, t), wide * 0.5 * lerp(1.0, 0.9, t)
        pts = []
        for k in range(n):
            th = 2 * math.pi * k / n
            c, s = math.cos(th), math.sin(th)
            r = 1.0 / ((abs(c) ** 3.0 + abs(s) ** 3.0) ** (1.0 / 3.0))      # (a superellipse: flatter sides, rounded ends)
            pts.append((x + a_ * c * r, y + b_ * s * r))
        return pts

    def zt(px, cut):
        return top - slope * (x + long * 0.5 - px) / long - cut
    shares = (0.0, 0.3, 0.55, 0.75, 0.88, 1.0) if fine else (0.0, 1.0)
    rings = []
    for t in shares:
        pts = plan(t)
        rings.append([Vector((px, py, lerp(z0, zt(px, 1.4), t))) for px, py in pts])
    m.skin(rings, "upper", (False, False))
    cap0 = rings[-1]
    cap1 = [Vector((p.x, p.y, zt(p.x, 0.0))) for p in cap0]
    m.skin([cap0, cap1], "black", (False, False))
    inner = [Vector((x + (p.x - x) * 0.86, y + (p.y - y) * 0.84, p.z)) for p in cap1]
    m.add(cap1 + inner, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "black", False, lambda q: Z)
    deep = [p - Z * 1.8 for p in inner]
    m.skin([deep, inner], "black", (True, False), inside=True, cap_paint="black")
    if fine:
        for j in range(-4, 5):                           # the grating over its mouth
            px = x + j * long * 0.09
            m.box((px, y, zt(px, 0.0) + 0.03), (0.09, wide * 0.8, 0.06), "steel")
        for off, r in ((-1.0, 0.16), (0.0, 0.12), (1.0, 0.14)):    # steam pipes up its after face
            px = x - long * 0.5 - r - 0.08
            m.rod((px, y + off, z0), (px, y + off, zt(x - long * 0.5, 0.0) + 0.6), r, "upper", sides=6)
            m.lathe((px, y + off, zt(x - long * 0.5, 0.0) + 0.58), Z, [(0.0, r), (0.1, r * 1.4), (0.25, r * 1.4)], "black", 6)
        for k in range(4):                                # its plates' seams
            zz = lerp(z0, zt(x, 1.4), (k + 1) / 5.0)
            ring = [Vector((px * 1.0, py, zz)) for px, py in plan((zz - z0) / max(zt(x, 1.4) - z0, 1e-6))]
            ring = [Vector((x + (p.x - x) * 1.004, y + (p.y - y) * 1.004, p.z)) for p in ring]
            m.strip(ring + [ring[0]], [Vector((p.x - x, p.y - y, 0)).normalized() for p in ring + [ring[0]]], 0.012, "seam", 0.004)
    if platform is not None and lights:
        zp = platform
        from ships import railing
        for hand in (PORT, STBD):
            yy = y + hand * (wide * 0.5 + 0.9)
            m.box((x + 0.5, yy, zp), (3.2, 2.0, 0.12), "upper")
            for dx in (-1.2, 1.2):
                m.rod((x + 0.5 + dx, y + hand * wide * 0.42, zp - 1.1), (x + 0.5 + dx, yy + hand * 0.8, zp - 0.06), 0.06, "upper", sides=4)
            if fine:
                railing(m, [(x + 2.1, yy + hand * 0.95, zp), (x - 1.1, yy + hand * 0.95, zp)], every=1.0)
            yard.searchlight(x + 0.5, yy, zp + 0.06, 0.62, (0.6, hand * 0.8, 0.1))
    k = sum(1 for s in m.sockets if s["name"].startswith("funnel.")) + 1
    m.socket(name or f"funnel.{k}", (x, y, top + 0.4), (-0.25, 0, 1), yard.half(x), "the funnel's mouth: her smoke rises here, more when she works up speed")
    yard.soot_at.append((x, y, top, long))
    return top


def lattice_crane(yard, x, y, z, high, reach, bearing=0.0, lift=38.0):
    """A big aircraft crane: a round post and a lattice jib raised over the catapults, its fall hanging from the head."""
    m = yard.on(x)
    m.lathe((x, y, z), Z, [(0.0, 0.85), (high * 0.15, 0.7), (high, 0.55)], "upper", 12 if m.fine else 6)
    a = math.radians(bearing)
    f = Vector((math.cos(a), math.sin(a), 0))
    s = Z.cross(f)
    foot = Vector((x, y, z + high * 0.55))
    e = math.radians(lift)
    head = foot + (f * math.cos(e) + Z * math.sin(e)) * reach
    w0, w1 = 0.55, 0.22
    chords = []
    for sy in (-1, 1):
        for sz in (-1, 1):
            o0 = s * sy * w0 + Z * sz * w0 * 0.8
            o1 = s * sy * w1 + Z * sz * w1 * 0.8
            m.rod(foot + o0, head + o1, 0.07, "upper", sides=4)
            chords.append((o0, o1))
    if m.fine:
        steps = 12
        for i in range(steps):
            t0, t1 = i / steps, (i + 1) / steps
            for sy in (-1, 1):
                p0 = foot + (head - foot) * t0 + s * sy * lerp(w0, w1, t0) + Z * lerp(w0, w1, t0) * 0.8 * (1 if i % 2 else -1)
                p1 = foot + (head - foot) * t1 + s * sy * lerp(w0, w1, t1) + Z * lerp(w0, w1, t1) * 0.8 * (-1 if i % 2 else 1)
                m.rod(p0, p1, 0.035, "upper", sides=3, smooth=False)
    yard.wire(Vector((x, y, z + high)), head, 0.035)
    yard.wire(head, head - Z * (head.z - z - 3.0), 0.03)
    m.box(head - Z * (head.z - z - 3.2), (0.3, 0.3, 0.4), "steel")


def kingfisher(yard, x, y, z, bearing=0.0, parent=None):
    """The battleship's scout floatplane on her catapult: a slim two-seat monoplane on one big float with a small float
    under each wingtip, two men under a long glazed hood (our own small model)."""
    m = yard.on(x) if parent is None else yard.m.use(parent)
    a = math.radians(bearing)
    f = Vector((math.cos(a), math.sin(a), 0))
    s = Vector((-math.sin(a), math.cos(a), 0))
    o = Vector((x, y, z))

    def P(fx, sy, uz):
        return o + f * fx + s * sy + Z * uz
    L, span = 10.3, 10.9
    rings = []
    for t, w, hh, zc in ((0.5, 0.55, 0.55, 2.55), (0.46, 0.62, 0.62, 2.55), (0.3, 0.55, 0.62, 2.6), (0.05, 0.5, 0.6, 2.62), (-0.2, 0.36, 0.42, 2.68), (-0.45, 0.12, 0.2, 2.8)):
        rings.append([P(t * L, math.cos(2 * math.pi * k / 10) * w, zc + math.sin(2 * math.pi * k / 10) * hh) for k in range(10)])
    m.skin(rings, "plane", (True, True))
    m.lathe(P(0.5 * L, 0, 2.55), f, [(0.0, 0.62), (0.18, 0.5), (0.3, 0.2)], "black", 10)        # the cowling's mouth, the airscrew
    m.box(P(0.5 * L + 0.32, 0, 2.55), (0.06, 2.6, 0.16), "black", yaw=bearing)
    m.ball(P(0.12 * L, 0, 3.08), (2.2, 0.36, 0.42), "glass", 10, 6, top_only=True)
    for hand in (1, -1):
        m.add([P(0.28 * L, hand * 0.5, 2.4), P(0.08 * L, hand * 0.5, 2.4), P(0.12 * L, hand * span / 2, 2.62), P(0.22 * L, hand * span / 2, 2.62)],
              [(0, 1, 2, 3), (3, 2, 1, 0)], "plane", False, None)
        m.add([P(-0.36 * L, 0, 2.75), P(-0.47 * L, 0, 2.75), P(-0.47 * L, hand * 2.3, 2.75), P(-0.39 * L, hand * 2.3, 2.75)], [(0, 1, 2, 3), (3, 2, 1, 0)], "plane", False, None)
        m.lathe(P(0.24 * L, hand * span * 0.42, 1.15), -f, [(0.0, 0.04), (0.3, 0.2), (2.2, 0.16), (2.6, 0.03)], "plane", 6)
        m.rod(P(0.18 * L, hand * span * 0.42, 1.25), P(0.18 * L, hand * span * 0.42, 2.58), 0.04, "steel", sides=4)
    m.add([P(-0.38 * L, 0.03, 2.75), P(-0.48 * L, 0.03, 2.75), P(-0.48 * L, 0.03, 4.1), P(-0.42 * L, 0.03, 4.1)], [(0, 1, 2, 3), (3, 2, 1, 0)], "plane", False, None)
    m.lathe(P(0.48 * L, 0, 0.85), -f, [(0.0, 0.08), (0.6, 0.48), (L * 0.6, 0.42), (L * 0.78, 0.06)], "plane", 10)
    for t in (0.32, 0.1):
        for hand in (1, -1):
            m.rod(P(t * L, hand * 0.3, 1.2), P(t * L, hand * 0.4, 2.2), 0.05, "steel", sides=4)
    for c in ((0.18 * L, 0.0), (-0.02 * L, 0.0)):
        fit.sailor(m, P(c[0], c[1], 2.32), f, "sit", seat=0.1, hands=(P(c[0] + 0.4, 0.15, 2.95), P(c[0] + 0.4, -0.15, 2.95)), k=0.92)


def catapult_us(yard, x, y, z, long, bearing, plane=True, parent=None):
    """A powder catapult on a turntable: a lattice girder track, its car at the after end with the floatplane on it.
    `parent`: the part it rides (a turret's roof), else her half."""
    m = yard.on(x) if parent is None else yard.m.use(parent)
    a = math.radians(bearing)
    f = Vector((math.cos(a), math.sin(a), 0))
    s = Z.cross(f)
    piv = Vector((x, y, z)) - f * long * 0.32
    m.lathe(piv, Z, [(0.0, 1.9), (0.4, 1.9), (0.5, 1.4), (0.95, 1.2)], "upper", 16 if m.fine else 8)
    c = Vector((x, y, z + 1.35))
    for sy in (-0.6, 0.6):
        m.box(c + s * sy, (long, 0.12, 0.7), "upper", yaw=bearing)
        m.box(c + s * sy + Z * 0.4, (long, 0.3, 0.1), "steel", yaw=bearing)
    if m.fine:
        for i in range(int(long / 1.1)):
            p = c - f * (long * 0.5) + f * (0.55 + i * 1.1)
            m.box(p, (0.08, 1.2, 0.08), "upper", yaw=bearing)
            m.rod(p - s * 0.6 - Z * 0.3, p + s * 0.6 + Z * 0.3, 0.025, "upper", sides=3, smooth=False)
    m.box(c - f * long * 0.32 + Z * 0.55, (2.2, 1.4, 0.4), "steel", yaw=bearing)
    if plane:
        kingfisher(yard, c.x - f.x * long * 0.18, c.y - f.y * long * 0.18, c.z + 0.2, bearing, parent=parent)


# ---- the Iowa ---------------------------------------------------------------------------------------------------

# The deck's edge from the US Navy's plan of the class (public domain), measured every 10 m: (x, half width as a share
# of half her beam). Her long fine bow is nearly straight-sided from abreast her first turret to the stem.
IOWA_PLAN = [(-135.2, 0.0), (-134.0, 0.3), (-132.0, 0.42), (-128.0, 0.5), (-125.2, 0.52), (-115.2, 0.69), (-105.2, 0.81), (-95.2, 0.89),
             (-85.2, 0.94), (-75.2, 0.975), (-65.2, 0.99), (-45.0, 1.0), (-5.0, 1.0), (4.8, 0.99), (14.8, 0.975), (24.8, 0.95), (34.8, 0.905),
             (44.8, 0.845), (54.8, 0.765), (64.8, 0.66), (74.8, 0.55), (84.8, 0.455), (94.8, 0.385), (104.8, 0.33), (114.8, 0.275),
             (124.8, 0.215), (131.0, 0.15), (134.6, 0.06), (135.2, 0.0)]

IOWA_PAINT = {"hull": "3E4A5C", "upper": "7E878F", "deck": "3B4757", "planks": "455262", "roof": "3B4757", "flight": "455262", "plane": "6E7A86",
              "door": "6A737B", "seam": "6F777E"}


def iowa(fine=True):
    """USS Iowa's class as she was in 1944-45: 270.4 m by 33.0, nine 16-inch guns in three turrets, ten twin 5-inch
    mountings, twenty quadruple 40 mm and about fifty 20 mm guns, two catapults and a crane on her fantail. Measure 22:
    navy blue hull up to the lowest point of her sheer, haze grey above, deck blue on her decks."""
    paints = livery("USA", "battleship")
    paints.update(IOWA_PAINT)
    paints = fit.crew_paints(paints, "USA")
    m = Model("Iowa", "Fast battleship, Iowa class, 1944-45 (our own model)", "ship", paints, fine)
    # freeboard by the recognition figures: 6.3 m amidships, 11.0 at the stem, 6.7 at the stern
    h = PlanHull(IOWA_PLAN, L=270.4, B=33.0, D=6.3, T=10.8, sheer=4.7, sheer_aft=0.4, rake=9.0, counter=5.5, flare=0.85, fine=1.5, full=0.75,
                 wl_fore=0.55, bilge=6.0)
    yard = Yard(m, h, break_x=-14.0)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("planks", line=6.25)
    D = h.deck_z
    z01, z02, z03, z04, z05 = 8.9, 11.5, 14.1, 16.7, 19.3

    # ---- the deckhouses, level by level (plans measured off the Navy's plan of the class)
    p01 = sym([(28.0, 0.0), (24.0, 3.5), (17.0, 10.4), (8.5, 12.6), (4.2, 13.8), (3.6, 13.8), (3.6, 15.6), (-3.6, 15.6), (-3.6, 13.8), (-21.2, 13.8),
               (-21.2, 15.6), (-28.8, 15.6), (-28.8, 13.8), (-43.5, 12.8), (-51.5, 6.5), (-57.0, 0.0)])
    p02 = sym([(24.5, 0.0), (20.0, 5.0), (12.5, 8.0), (12.2, 8.0), (12.2, 11.8), (5.8, 11.8), (5.8, 8.0), (-5.6, 8.0), (-5.6, 11.8), (-12.2, 11.8), (-12.2, 8.0),
               (-31.2, 8.0), (-31.2, 11.6), (-38.0, 11.6), (-38.0, 8.0), (-44.0, 6.5), (-49.5, 0.0)])
    p03 = sym([(23.0, 0.0), (19.5, 4.6), (13.0, 6.6), (-12.0, 6.6), (-14.0, 4.6), (-14.0, 0.0)])
    p03a = sym([(-15.5, 0.0), (-15.5, 4.8), (-23.0, 4.8), (-23.0, 0.0)])                     # under the after funnel's foot
    p03b = sym([(-36.5, 0.0), (-36.5, 4.6), (-40.5, 5.2), (-50.0, 4.0), (-55.0, 0.0)])        # the after tower's foot
    for plan, z0, z1 in ((p01, D(0.0) - 0.4, z01), (p02, z01, z02), (p03, z02, z03), (p03a, z02, z03), (p03b, z02, z03)):
        halves = ([(plan, None)])
        for pl, _ in halves:
            m.use("fore" if sum(p[0] for p in pl) / len(pl) > yard.break_x else "aft")
            slab(m, pl, z0, z1)
            walls_detail(yard, pl, z0 if z0 > 7 else 6.5, z1)

    # ---- the main battery
    mains = []
    for name, tx, raised, bearing in (("turret.A", 57.8, 0.0, 0.0), ("turret.B", 36.0, 3.7, 0.0), ("turret.Y", -63.8, 0.6, 180.0)):
        tn, gs, W, H = us_turret(yard, name, tx, D(tx), bearing, raised=raised)
        mains.append((tn, gs, tx, raised > 1.0))
        yard.fire_points.append((tx, 0.0, D(tx) + raised + 2.5))

    # ---- the forward tower: the armoured conning tower, the pilot house round it, the tower's levels, the director
    m.use("fore")
    ct = [(22.4 + math.cos(2 * math.pi * k / 20) * 3.3, math.sin(2 * math.pi * k / 20) * 2.9) for k in range(20)]
    slab(m, ct, z02, z05 + 1.4, top="roof")
    if fine:
        ring = [Vector((px, py, z05 + 0.75)) for px, py in ct]
        for k in range(20):                                          # the vision slits round its top
            a, b = ring[k], ring[(k + 1) % 20]
            o_ = Vector(((a.x + b.x) * 0.5 - 22.4, (a.y + b.y) * 0.5, 0)).normalized()
            if o_.x > -0.3:
                m.add([a + o_ * 0.02, b + o_ * 0.02, b + o_ * 0.02 + Z * 0.22, a + o_ * 0.02 + Z * 0.22], [(0, 1, 2, 3)], "black", False, lambda q, o=o_: o)
    pilot = sym([(25.0, 0.0), (24.0, 4.2), (20.5, 7.4), (11.0, 7.4), (11.0, 0.0)])
    slab(m, pilot, z03, z04)
    walls_detail(yard, pilot, z03, z04, doors=False, scuttles=False)
    bridge = sym([(24.6, 0.0), (23.6, 4.0), (20.0, 7.0), (10.8, 7.0), (10.8, 0.0)])
    slab(m, bridge, z04, z05)
    if fine:   # the navigating bridge's windows round its front and sides
        n_ = len(bridge)
        for k in range(n_):
            a, b = Vector((*bridge[k], 0)), Vector((*bridge[(k + 1) % n_], 0))
            if min(a.x, b.x) < 13.0:
                continue
            d = b - a
            o_ = Vector((d.y, -d.x, 0)).normalized()
            panes = max(1, int(d.length / 1.0))
            for i in range(panes):
                c = a + d * ((i + 0.5) / panes) + o_ * 0.03
                m.add([c - d.normalized() * (d.length / panes * 0.5 - 0.08) + Z * (z04 + 1.15), c + d.normalized() * (d.length / panes * 0.5 - 0.08) + Z * (z04 + 1.15),
                       c + d.normalized() * (d.length / panes * 0.5 - 0.08) + Z * (z04 + 2.05), c - d.normalized() * (d.length / panes * 0.5 - 0.08) + Z * (z04 + 2.05)],
                      [(0, 1, 2, 3)], "glass", False, lambda q, o=o_: o)
    walls_detail(yard, bridge, z04, z05, doors=False, scuttles=False)
    # the bridge wings
    for hand in (PORT, STBD):
        m.box((16.0, hand * 9.0, z05 - 0.08), (4.0, 3.6, 0.16), "deck")
        from ships import railing
        if fine:
            railing(m, [(18.0, hand * 7.0, z05), (18.0, hand * 10.8, z05), (14.0, hand * 10.8, z05), (14.0, hand * 7.0, z05)], every=1.3)
        for dx in (-1.6, 1.6):
            m.rod((16.0 + dx, hand * 7.2, z04), (16.0 + dx, hand * 10.4, z05 - 0.15), 0.07, "upper", sides=4)
    # the tower proper, its levels stepping in as it rises
    tz = z05
    levels = ((16.5, 2.4, 5.4, 2.6), (13.8, 2.4, 4.6, 2.6), (12.0, 2.8, 4.0, 2.8), (11.0, 3.2, 3.6, 2.6), (10.6, 1.5, 4.4, 2.6))
    for k, (xf, xa, hw, hh) in enumerate(levels):
        pl = Model.round_rect(xf, xa, -hw, hw, 1.2 if k < 4 else 0.6, 3 if fine else 1)
        slab(m, pl, tz, tz + hh)
        walls_detail(yard, pl, tz, tz + hh, doors=k < 2, scuttles=True)
        if k in (0, 4) and fine:
            yard.windows(xf, hw - 0.6, tz + hh - 1.5, 0.9, back=2.2)
        tz += hh
    top_z = tz                                                        # 32.4
    fit.mk38(yard, "director.fore", 6.2, 0.0, top_z, 0.0, arm=4.0, parent="fore")
    # the Mark 37s each side of the tower, on platforms of their own
    for hand, nm in ((PORT, "director.sec.port"), (STBD, "director.sec.starboard")):
        zp = z05 + 2.6 * 2
        m.box((6.6, hand * 5.9, zp - 0.1), (6.0, 4.4, 0.2), "deck")
        for dx in (-2.2, 2.2):
            m.rod((6.6 + dx, hand * 3.7, zp - 2.2), (6.6 + dx, hand * 7.5, zp - 0.2), 0.08, "upper", sides=4)
        fit.mk37(yard, nm, 6.6, hand * 6.0, zp, 0.0, parent="fore")
    # searchlights and the small platforms on the tower
    for hand in (PORT, STBD):
        m.box((3.6, hand * 4.6, z05 + 7.8 + 2.6), (2.2, 1.8, 0.12), "deck")
        yard.searchlight(3.6, hand * 4.8, z05 + 7.8 + 2.66, 0.55, (0.2, hand, 0))

    # ---- the funnels, the foremast and its radars, the mainmast
    us_funnel(yard, -4.3, z03, 29.9, 10.8, 8.4, slope=1.4, platform=22.8)
    us_funnel(yard, -29.6, z03, 29.2, 10.6, 8.2, slope=1.4, platform=21.6)
    m.use("fore")
    slab(m, Model.round_rect(1.5, -10.5, -5.2, 5.2, 1.8, 3 if fine else 1), z03, z03 + 4.4)          # the funnel's casing up to the tower
    mast_x = 0.4
    m.lathe((mast_x, 0, 24.0), Z, [(0.0, 0.42), (9.0, 0.32), (16.8, 0.16)], "upper", 8 if fine else 5)
    for hand in (PORT, STBD):
        m.rod((mast_x - 4.5, hand * 2.6, 27.5), (mast_x, 0.0, 34.0), 0.18, "upper", sides=6)
    m.box((mast_x + 1.6, 0, 34.0), (4.8, 3.4, 0.16), "deck")
    m.rod((mast_x - 2.6, -3.6, 37.4), (mast_x - 2.6, 3.6, 37.4), 0.08, "upper", sides=4)
    fit.sk_radar(yard, "radar.air", mast_x + 2.2, 0.0, 34.1, size=5.2, parent="fore")
    fit.sg_radar(yard, "radar.surface", mast_x, 0.0, 40.6, wide=1.7, parent="fore")
    head_fore = Vector((mast_x, 0, 40.8))
    m.use("aft")
    main_x = -36.4
    m.lathe((main_x, 0, z03), Z, [(0.0, 0.42), (12.0, 0.3), (24.2, 0.14)], "upper", 8 if fine else 5)
    for hand in (PORT, STBD):
        m.rod((main_x - 3.0, hand * 2.4, z03), (main_x, 0.0, z03 + 13.0), 0.16, "upper", sides=6)
    m.rod((main_x, -3.4, z03 + 20.0), (main_x, 3.4, z03 + 20.0), 0.08, "upper", sides=4)
    fit.sg_radar(yard, "radar.surface.aft", main_x, 0.0, z03 + 21.6, wide=1.5, parent="aft")
    head_main = Vector((main_x, 0, z03 + 24.2))
    yard.wire(head_fore, head_main, 0.025, sag=1.2)
    yard.wire(head_main, (-h.L * 0.49, 0, D(-h.L * 0.49) + 7.0), 0.025)
    m.socket("ensign", (main_x - 1.5, 0, z03 + 18.0), (-1, 0, 0), "aft", "the battle ensign flies from the mainmast's gaff")

    # ---- the after tower and its directors
    m.use("aft")
    tz = z03
    for xf, xa, hw, hh in ((-38.5, -49.5, 3.9, 2.6), (-40.0, -48.5, 3.4, 2.6), (-41.5, -47.5, 3.0, 2.4)):
        pl = Model.round_rect(xf, xa, -hw, hw, 1.0, 3 if fine else 1)
        slab(m, pl, tz, tz + hh)
        walls_detail(yard, pl, tz, tz + hh)
        tz += hh
    fit.mk38(yard, "director.aft", -44.6, 0.0, tz, 180.0, arm=3.6, parent="aft")
    m.box((-52.5, 0, z03 - 0.1), (5.0, 5.2, 0.2), "deck")
    fit.mk37(yard, "director.sec.aft", -52.6, 0.0, z03, 180.0, parent="aft")

    # ---- the ten twin 5-inch mountings: outboard pairs on the first deck up, inboard ones a deck higher
    sec = []
    n_p = n_s = 0
    for sx, sy, zz, bearing in ((9.0, 9.4, z02, 0.0), (0.0, 13.2, z01, 0.0), (-8.9, 9.4, z02, 0.0), (-25.0, 13.2, z01, 180.0), (-34.6, 9.4, z02, 180.0)):
        for hand in (PORT, STBD):
            if hand == PORT:
                n_p += 1
                nm = f"sec.P{n_p}"
            else:
                n_s += 1
                nm = f"sec.S{n_s}"
            ahead = bearing == 0.0
            lim = (-150, 30) if (hand == PORT) == ahead else (-30, 150)
            fit.twin_5in(yard, nm, sx, hand * sy, zz, bearing, limits=lim)
            sec.append(nm)

    # ---- twenty quadruple 40 mm mountings, manned
    q = 0
    quads = [(84.0, 6.0, None, 0.0), (20.7, 12.3, z01, 0.0), (15.6, 8.6, z03, 0.0), (-17.0, 6.6, z03, 0.0), (-22.0, 6.4, z03, 0.0), (-39.5, 6.6, z03, 180.0),
             (-44.8, 6.9, z03, 180.0), (-49.0, 13.0, None, 180.0), (-127.0, 4.2, None, 180.0)]
    for qx, qy, qz, bearing in quads:
        for hand in (PORT, STBD):
            q += 1
            zz = qz if qz is not None else D(qx)
            if qz is not None and (qx in (15.6, -17.0, -22.0, -39.5, -44.8)):
                wall = {15.6: 6.6, -17.0: 4.4, -22.0: 4.8, -39.5: 5.0, -44.8: 3.9}[qx]
                sponson(yard, qx, hand * qy, zz, 3.05, hand * wall)
            fit.bofors_quad(yard, q, qx, hand * qy, zz, bearing=bearing + (hand * -60.0 if bearing == 0.0 else hand * 60.0), tub=2.75 if qx != -127.0 else 2.4)
    for tn, gs, tx, raised in mains:      # one on the roof of the second and the third turret, riding the turret
        if tn in ("turret.B", "turret.Y"):
            q += 1
            p = m.parts[m.part_at[tn]]
            zr = p.pivot.z + 3.45
            rest = m.parts[m.part_at[tn]].rest
            ox = -3.2 if rest == 0.0 else 3.2
            fit.bofors_quad(yard, q, tx + ox, 0.0, zr, bearing=rest, tub=2.6, parent=tn)

    # ---- the 20 mm guns: along the forecastle, round the first deck up, on the quarterdeck
    for gx in (44.0, 47.5, 51.0, 54.5, 66.0, 69.5):
        for hand in (PORT, STBD):
            yy = hand * (h.half_at(gx) - 1.4)
            fit.oerlikon(yard, gx, yy, D(gx), facing=hand * 70.0)
    for gx in (12.0, -9.0, -12.5, -16.0, -19.0, -32.0, -36.0, -40.0):
        for hand in (PORT, STBD):
            hy = 13.0 if gx > -42.0 else 11.6
            if 2.5 > gx > -5.0:
                continue
            fit.oerlikon(yard, gx, hand * hy, z01, facing=hand * 80.0, loader=(int(gx) % 2 == 0))
    for gx in (-84.0, -88.0, -92.0, -134.0 + 6.0):
        for hand in (PORT, STBD):
            yy = hand * (h.half_at(gx) - 1.3)
            fit.oerlikon(yard, gx, yy if gx > -120 else hand * 1.6, D(gx), facing=hand * 75.0 if gx > -120 else 180.0 - hand * 20.0, loader=False)
    for gx in (5.0, -1.0):                 # and two on the funnel's casing
        for hand in (PORT, STBD):
            fit.oerlikon(yard, gx, hand * 4.6, z03 + 4.4, facing=hand * 80.0, loader=False)

    # ---- the fantail: two catapults with their Kingfishers, the crane, boats
    for hand in (PORT, STBD):
        catapult_us(yard, -112.5, hand * 11.0, D(-112.5), 20.5, hand * 9.0)
    lattice_crane(yard, -131.0, 0.0, D(-131.0), 9.0, 17.0, bearing=0.0, lift=34.0)
    for hand in (PORT, STBD):
        yard.boat(-97.0, hand * 6.2, D(-97.0) + 0.6, 10.0)
        if fine:
            for dx in (-3.0, 3.0):
                m.box((-97.0 + dx, hand * 6.2, D(-97.0) + 0.3), (0.4, 2.0, 0.6), "upper")

    # ---- forecastle and quarterdeck: anchors, breakwater, bollards, rails, scuttles, staffs; rafts on the houses
    yard.anchor_gear(h.L * 0.47, 101.0, spread=2.6, breakwater=(73.4, 9.6, 1.3))
    for bx in (118.0, 96.0, 30.0, -60.0, -100.0, -122.0):
        for hand in (PORT, STBD):
            yard.bollards(bx, hand * (h.half_at(bx) - 1.0), D(bx))
    yard.rails(-h.L * 0.49, h.L * 0.485)
    yard.portholes(-h.L * 0.42, h.L * 0.44, 2.2, every=3.6, skip=[(-62, 56)])
    yard.portholes(-h.L * 0.3, h.L * 0.42, 4.6, every=3.6, skip=[(-62, 62)])
    if fine:
        for fx, fy in ((-15.0, 13.85), (-6.5, 13.85), (-41.0, 13.0), (-46.0, 12.0)):
            for hand in (PORT, STBD):
                fit.float_rack(yard, fx, hand * fy, 6.9 + 0.6, (0, hand, 0), n=2)
        for vx in (40.0, 26.0, -54.0, -70.0, -105.0):
            for hand in (PORT, STBD):
                yard.vent(vx, hand * h.half_at(vx) * 0.45, D(vx), 0.45, 1.3)
    m.use("aft")
    sx = -h.L * 0.497
    m.rod((sx + 0.6, 0, D(sx + 0.6)), (sx - 0.3, 0, D(sx + 0.6) + 6.0), 0.07, "upper", sides=4)
    m.use("fore")
    jx = h.L * 0.494
    m.rod((jx, 0, D(jx)), (jx + 0.4, 0, D(jx) + 3.2), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.4, 0, D(jx) + 3.2), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=4, r=2.6, rudders=2)

    # ---- where she burns
    yard.fire_points += [(16.0, 0.0, z05), (-4.3, 0.0, z03 + 4.0), (-29.6, 0.0, z03 + 2.0), (-45.0, 0.0, z03), (80.0, 0.0, D(80.0)), (-100.0, 0.0, D(-100.0))]
    from ships import clips_for_ship, sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, mains, "battleship")
    aa_clip(m)
    m.info.setdefault("file", {}).update({"class": "Iowa", "navy": "USA", "year": 1943, "stands_for": "Battleship_USA",
                                          "paint": "Measure 22: navy blue 5-N to the lowest point of the sheer, haze grey 5-H above, deck blue 20-B"})
    return m, yard


# ---- Japanese main battery, funnel, hull fittings ------------------------------------------------------------------

def jp_turret46(yard, name, x, deck, bearing, raised=0.0, parent=None):
    """Japan's 46 cm triple turret, the largest afloat: a barbette as wide as a destroyer is long; a long gunhouse with
    its face sloped steeply back and its sides angled in to it, a flat roof, a rounded rear; the 15 m rangefinder
    across its back with a hood out each side; three guns in canvas bloomers. Parts and sockets as the yard's."""
    m = yard.m
    half = parent or yard.half(x)
    fine = m.fine
    base = deck + raised
    m.use(half)
    m.lathe((x, 0, deck - 0.6), Z, [(0.0, 7.1), (raised + 1.0, 7.1)], "upper", 36 if fine else 14, caps=(False, True))
    if fine:
        m.lathe((x, 0, base + 0.38), Z, [(0.0, 7.16), (0.06, 7.16)], "seam", 36, caps=(False, False))
    piv = Vector((x, 0, base + 1.0))
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")

    def W(px, py, pz):
        return piv + rot @ Vector((px, py, pz))
    tname = m.part(name, half, tuple(piv), axis=(0, 0, -1), limits=[-150, 150], kind="turn",
                   note=f"trains: plus turns it clockwise seen from above, so its bearing is its rest bearing plus the turn; rest bearing {int(bearing)}; about 2 degrees a second")
    m.parts[m.part_at[tname]].rest = bearing
    m.parts[m.part_at[tname]].rate = 2.0
    m.use(tname)
    H = 3.9
    lo = [(10.4, 3.7), (9.0, 6.3), (2.0, 7.3), (-6.4, 7.3), (-8.4, 6.0), (-8.9, 3.0), (-9.0, 0.0)]
    hi = [(7.0, 3.5), (6.0, 5.7), (1.6, 6.9), (-6.2, 6.9), (-8.1, 5.7), (-8.6, 2.9), (-8.7, 0.0)]
    plan_lo = lo[:-1] + [lo[-1]] + [(px, -py) for px, py in reversed(lo[:-1])]
    plan_hi = hi[:-1] + [hi[-1]] + [(px, -py) for px, py in reversed(hi[:-1])]
    n = len(plan_lo)
    vl = [W(px, py, 0.0) for px, py in plan_lo]
    vh = [W(px, py, H) for px, py in plan_hi]
    mid = W(0.5, 0, H * 0.5)
    m.add(vl + vh, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "upper", False, lambda q: q - mid)
    tris = tessellate_polygon([[Vector((p[0], p[1], 0)) for p in plan_hi]])
    m.add(vh, [tuple(t) for t in tris], "roof", False, lambda q: Z)
    m.add(vl, [tuple(t) for t in tris], "steel", False, lambda q: -Z)
    if fine:
        for k in range(1, 7):                                   # the roof's plates
            px = 6.0 - k * 2.0
            m.box(W(px, 0, H + 0.005), (0.03, 13.4, 0.012), "seam", yaw=bearing)
        for sy in (-3.4, 3.4):                                  # the gunlayers' periscope hoods
            m.box(W(4.4, sy, H + 0.28), (1.2, 0.8, 0.56), "roof", yaw=bearing)
            m.box(W(5.01, sy, H + 0.32), (0.02, 0.5, 0.14), "glass", yaw=bearing)
        for sy in (-1.6, 1.6):                                  # hatches
            m.lathe(W(-5.5, sy, H), Z, [(0.0, 0.5), (0.12, 0.5), (0.18, 0.42)], "roof", 12)
        m.box(W(-2.0, 0, H + 0.35), (2.6, 2.2, 0.7), "roof", yaw=bearing)        # the turret officer's hood
        for side in (1, -1):                                    # the rangefinder's ends out of her sides, hooded
            m.box(W(-5.4, side * 7.25, H * 0.62), (2.2, 1.1, 1.5), "upper", yaw=bearing)
            m.disc(W(-4.28, side * 7.55, H * 0.62), rot @ X, 0.2, "glass", 8)
            m.box(W(-5.4, side * 7.83, H * 0.62), (1.6, 0.06, 1.1), "seam", yaw=bearing)
        from ships import ladder
        back = W(-8.8, 1.6, 0.0)
        ladder(m, back, back.z + H, rot @ Vector((-1, 0, 0)))
        for side in (1, -1):                                    # hand rails round the roof's edge
            for a_, b_ in zip(hi[1:5], hi[2:6]):
                p0, p1 = W(a_[0], side * a_[1] - side * 0.15, H), W(b_[0], side * b_[1] - side * 0.15, H)
                m.rod(p0 + Z * 0.5, p1 + Z * 0.5, 0.025, "upper", sides=3, smooth=False)
    guns = []
    fwd, lat = rot @ X, rot @ Y
    gap = 3.05
    for k in range(3):
        off = (k - 1) * gap
        p = W(6.0, off, 1.8)
        gname = m.part(f"{name}.gun.{k + 1}", tname, tuple(p), axis=tuple(-lat), limits=[-5, 45], slide=tuple(-fwd), slide_limits=[0.0, 1.4], kind="turn",
                       note="a gun: plus lifts the muzzle; slides back along its own length when it fires")
        m.use(gname)
        e = math.radians(2.0)
        ax = (fwd * math.cos(e) + Z * math.sin(e)).normalized()
        reach = 2.9
        long = 12.6
        prof = [(0.0, 0.7), (reach + 0.4, 0.66), (reach + 2.6, 0.6), (reach + 3.0, 0.55), (reach + long * 0.55, 0.47), (reach + long * 0.93, 0.4),
                (reach + long * 0.95, 0.43), (reach + long, 0.42)]
        m.lathe(p, ax, prof, "upper", 18 if fine else 8, caps=(True, False))
        tip = p + ax * (reach + long)
        m.disc(tip - ax * 0.02, ax, 0.415, "upper", 18 if fine else 8)
        m.disc(tip + ax * 0.005, ax, 0.235, "black", 12 if fine else 6)
        m.lathe(p + ax * (reach - 0.6), ax, [(0.0, 1.15), (0.5, 1.12), (1.4, 0.95), (3.0, 0.7)], "bag", 16 if fine else 6, caps=(False, False))
        m.socket(f"muzzle.{name}.{k + 1}", tip, ax, gname, "flash, smoke and the shell leave here")
        guns.append(gname)
    m.use(half)
    return tname, guns, W, H


def jp_funnel(yard, x0, z0, x1, z1, long0, wide0, long1, wide1, name=None):
    """A great raked funnel: an oval trunk leaning aft from (x0, z0) to (x1, z1), narrowing as it rises; a deep lip
    round its mouth, a cage of bars over it (the late-war splinter cap), steam pipes up its after face."""
    m = yard.on(x0)
    fine = m.fine
    n = 28 if fine else 12
    shares = (0.0, 0.25, 0.5, 0.75, 0.92, 1.0) if fine else (0.0, 1.0)

    def ring(t, grow=1.0):
        cx, cz = lerp(x0, x1, t), lerp(z0, z1, t)
        a_, b_ = lerp(long0, long1, t) * 0.5 * grow, lerp(wide0, wide1, t) * 0.5 * grow
        pts = []
        for k in range(n):
            th = 2 * math.pi * k / n
            c, s = math.cos(th), math.sin(th)
            r = 1.0 / ((abs(c) ** 2.6 + abs(s) ** 2.6) ** (1.0 / 2.6))
            pts.append(Vector((cx + a_ * c * r, b_ * s * r, cz)))
        return pts
    rings = [ring(t) for t in shares]
    m.skin(rings, "upper", (False, False))
    top = rings[-1]
    lip = ring(1.0, 1.06)
    lip_top = [p + Z * 0.7 for p in lip]
    m.skin([top, lip, lip_top], "upper", (False, False))
    inner = [Vector((x1 + (p.x - x1) * 0.9, p.y * 0.88, p.z)) for p in lip_top]
    m.add(lip_top + inner, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "black", False, lambda q: Z)
    deep = [p - Z * 2.2 for p in inner]
    m.skin([deep, inner], "black", (True, False), inside=True, cap_paint="black")
    if fine:
        for j in range(-5, 6):                                  # the cage of bars over its mouth
            px = x1 + j * long1 * 0.085
            m.rod((px, -wide1 * 0.46, z1 + 0.7), (px, wide1 * 0.46, z1 + 0.7), 0.05, "steel", sides=4)
        m.rod((x1 - long1 * 0.5, 0, z1 + 0.72), (x1 + long1 * 0.5, 0, z1 + 0.72), 0.06, "steel", sides=4)
        for off in (-1.2, 0.0, 1.2):                            # steam pipes up its after face
            a = Vector((x0 - long0 * 0.5 - 0.25, off, z0))
            b = Vector((x1 - long1 * 0.5 - 0.25, off, z1 + 1.3))
            m.rod(a, b, 0.17, "upper", sides=6)
            m.lathe(b, Z, [(0.0, 0.17), (0.1, 0.24), (0.3, 0.24)], "black", 6)
        for k in range(1, 5):                                   # its plates' seams
            rr = [Vector((p.x, p.y * 1.004, p.z)) for p in ring(k / 5.0, 1.004)]
            cx = lerp(x0, x1, k / 5.0)
            m.strip(rr + [rr[0]], [Vector((p.x - cx, p.y, 0)).normalized() for p in rr + [rr[0]]], 0.012, "seam", 0.004)
    k = sum(1 for s in m.sockets if s["name"].startswith("funnel.")) + 1
    m.socket(name or f"funnel.{k}", (x1, 0, z1 + 1.0), (-0.35, 0, 1), yard.half(x1), "the funnel's mouth: her smoke rises here, more when she works up speed")
    yard.soot_at.append((x1, 0, z1, long1))


def hull_panel(yard, x0, x1, z0, z1, hand, paint="black", off=0.03):
    """A shape laid on her side (a recess's shadow, an opening): an oblong between x0..x1 and z0..z1 that follows the
    skin, a little off it."""
    m, h = yard.m, yard.h
    if not m.fine:
        return
    cols, rows_ = 14, 5
    grid = []
    for i in range(cols + 1):
        x = lerp(x0, x1, i / cols)
        row = []
        for j in range(rows_ + 1):
            z = lerp(z0, z1, j / rows_)
            p, nn = h.side(x, z, hand)
            row.append(p + nn * off)
        grid.append(row)
    yard.on((x0 + x1) / 2)
    m.sheet(grid, paint, Vector((0, hand, 0)), smooth=False)


YAMATO_PLAN = [(-131.5, 0.0), (-131.0, 0.14), (-129.5, 0.22), (-128.0, 0.28), (-124.0, 0.42), (-118.0, 0.56), (-112.0, 0.68), (-106.0, 0.79), (-100.0, 0.87),
               (-90.0, 0.9), (-80.0, 0.92), (-75.0, 0.955), (-70.0, 0.985), (-60.0, 1.0), (20.0, 1.0), (30.0, 0.975), (40.0, 0.92), (50.0, 0.878),
               (60.0, 0.82), (70.0, 0.737), (80.0, 0.653), (90.0, 0.563), (100.0, 0.48), (110.0, 0.408), (115.0, 0.376), (120.0, 0.348), (124.0, 0.325),
               (127.0, 0.306), (129.0, 0.283), (130.5, 0.229), (131.2, 0.14), (131.5, 0.0)]
# her deck's wave, off the 1945 profile: low aft, up to her upper deck amidships, down to its lowest abreast her first
# turret, then the long climb to her flared bow
YAMATO_DECK = [(-131.5, 6.1), (-110.0, 5.9), (-90.0, 5.9), (-80.0, 6.1), (-72.0, 7.1), (-64.0, 7.9), (-55.0, 8.1), (15.0, 8.1), (25.0, 7.6), (35.0, 7.0),
               (45.0, 6.6), (55.0, 6.4), (75.0, 6.35), (90.0, 6.5), (100.0, 6.75), (110.0, 7.15), (118.0, 7.55), (124.0, 8.0), (128.0, 8.4), (131.5, 8.8)]
YAMATO_PAINT = {"planks": "B8A27A", "deck": "5E6468", "flight": "5E6468"}


def yamato(fine=True):
    """Yamato as she sailed on 7 April 1945: 263 m by 38.9, nine 46 cm guns in three turrets, two 15.5 cm triple
    turrets on her centre line, twelve twin 12.7 cm high-angle mountings, some fifty triple 25 mm mountings ringed
    round her superstructure and single guns along her decks. Kure grey, a pale wooden deck, steel fore and aft."""
    paints = livery("JAP", "battleship")
    paints.update(YAMATO_PAINT)
    paints = fit.crew_paints(paints, "JAP")
    m = Model("Yamato", "Battleship, Yamato class, 1945 (our own model)", "ship", paints, fine)
    h = PlanHull(YAMATO_PLAN, decks=YAMATO_DECK, L=263.0, B=38.9, D=6.3, T=10.4, rake=5.5, counter=1.5, flare=1.0, fine=1.7, full=0.7, wl_fore=0.42, bilge=7.0)
    yard = Yard(m, h, break_x=-18.0)
    yard.soot_at, yard.rust_at = [], []
    nf = len(m.faces)
    yard.hull("planks")
    # (steel decks fore and aft of the wood: her forecastle past the anchor gear, her quarterdeck where the aircraft were worked)
    pk, dk = m.paint_at["planks"], m.paint_at["deck"]
    for i in range(nf, len(m.faces)):
        if m.fpaint[i] == pk:
            xs = [m.verts[v][0] for v in m.faces[i]]
            if max(xs) <= -69.5 or min(xs) >= 101.0:
                m.fpaint[i] = dk
    D = h.deck_z
    zm = 8.1                       # her upper deck amidships
    zA, zB, zC = 10.7, 13.3, 15.9

    # ---- the superstructure's three lower levels (off the plan: a long rounded block ringed with gun positions)
    pA = sym([(9.0, 0.0), (8.0, 5.5), (5.0, 10.0), (0.0, 13.2), (-3.0, 16.2), (-36.0, 16.2), (-39.0, 13.5), (-44.0, 10.0), (-49.0, 5.5), (-51.0, 0.0)])
    pB = sym([(4.0, 0.0), (3.0, 5.0), (0.0, 8.0), (-3.5, 11.2), (-36.5, 11.2), (-40.0, 8.5), (-43.0, 4.5), (-44.5, 0.0)])
    pC = sym([(2.5, 0.0), (1.5, 4.3), (-2.0, 5.0), (-38.0, 5.0), (-39.5, 3.5), (-40.0, 0.0)])
    for plan, z0, z1 in ((pA, zm - 0.4, zA), (pB, zA, zB), (pC, zB, zC)):
        m.use("fore" if sum(p[0] for p in plan) / len(plan) > yard.break_x else "aft")
        slab(m, plan, z0, z1)
        walls_detail(yard, plan, max(z0, zm), z1)

    # ---- the main battery
    mains = []
    for name, tx, base, bearing in (("turret.A", 49.6, 7.4, 0.0), ("turret.B", 26.0, 10.4, 0.0), ("turret.Y", -60.4, 8.6, 180.0)):
        tn, gs, W, H = jp_turret46(yard, name, tx, D(tx), bearing, raised=base - D(tx))
        mains.append((tn, gs, tx, base - D(tx) > 1.5))
        yard.fire_points.append((tx, 0.0, base + 2.5))

    # ---- the two 15.5 cm triple turrets on her centre line, one before the tower and one aft
    sec = []
    for nm, sx, z_, bearing, lim in (("sec.C1", 11.5, D(11.5), 0.0, (-150, 150)), ("sec.C2", -47.2, zA, 180.0, (-150, 150))):
        raised = (14.4 if bearing == 0.0 else 12.7) - z_
        tn, gs = yard.turret(nm, sx, z_, 8.2, 3, 9.6, bearing, 0.0775, raised=raised, style="medium", limits=lim, elev=(-7, 55), rate=6.0, rest_elev=2.0,
                             floats=False)
        sec.append(nm)

    # ---- the tower: its foot on the third level, the bridge with its wings, the levels stepping in, the 15 m
    # rangefinder and its radar on top, a pole above
    m.use("fore")
    tz = zC
    levels = ((3.0, -12.5, 5.4, 3.5, "base"), (2.2, -11.2, 4.8, 3.2, "bridge"), (1.4, -9.8, 3.9, 3.4, ""), (1.0, -9.3, 3.6, 3.4, "deck"),
              (0.6, -8.8, 3.3, 3.4, "radar"), (0.2, -8.3, 3.0, 2.6, "top"))
    tower_top = None
    from ships import railing
    for k, (xf, xa, hw, hh, what) in enumerate(levels):
        pl = Model.round_rect(xf, xa, -hw, hw, 1.6 if k < 3 else 1.1, 3 if fine else 1)
        slab(m, pl, tz, tz + hh)
        walls_detail(yard, pl, tz, tz + hh, doors=k < 2, scuttles=what not in ("bridge", "top"))
        if what in ("bridge", "top") and fine:
            yard.windows(xf, hw - 0.5, tz + hh - 1.45, 0.95, back=3.0)
        if what == "bridge":                       # the wings each side, open, railed, on brackets
            for hand in (PORT, STBD):
                m.box((-3.0, hand * (hw + 2.0), tz + hh - 0.08), (6.0, 4.0, 0.16), "deck")
                if fine:
                    railing(m, [(0.0, hand * (hw + 0.1), tz + hh), (0.0, hand * (hw + 4.0), tz + hh), (-6.0, hand * (hw + 4.0), tz + hh), (-6.0, hand * (hw + 0.1), tz + hh)],
                            every=1.3)
                for dx in (-5.0, -1.0):
                    m.rod((dx, hand * hw, tz + 0.6), (dx, hand * (hw + 3.6), tz + hh - 0.16), 0.08, "upper", sides=4)
        if what == "deck":                         # a platform round her with a searchlight each side
            m.use("fore")
            for hand in (PORT, STBD):
                m.box((-6.0, hand * (hw + 1.2), tz + 0.1), (3.0, 2.4, 0.14), "deck")
                yard.searchlight(-6.0, hand * (hw + 1.4), tz + 0.17, 0.6, (0.3, hand, 0.05))
        if what == "radar":                        # the Type 22 surface radar's two horns each side, like a snout
            for hand in (PORT, STBD):
                c = Vector((-3.4, hand * (hw + 0.9), tz + hh * 0.5))
                m.box(c - Vector((0, hand * 0.45, 0)), (1.4, 0.9, 2.6), "upper")
                for dz_ in (-0.7, 0.7):
                    m.lathe(c + Vector((0.2, 0, dz_)), Vector((0, hand, 0)), [(0.0, 0.18), (0.9, 0.42), (1.0, 0.44)], "upper", 10 if fine else 5, caps=(True, False))
                    m.disc(c + Vector((0.2, hand * 1.0, dz_)), Vector((0, hand, 0)), 0.4, "black", 10 if fine else 5)
        tz += hh
        tower_top = tz
    # the rangefinder house (it turns) with the 15 m rangefinder through it and the Type 21 aerials on its sides
    rf = fit.rangefinder(yard, "director.fore", -4.2, 0.0, tower_top, 7.5, bearing=0.0, house=2.6, parent="fore")
    m.use(rf)
    for hand in (PORT, STBD):              # the Type 21 air radar: a mattress of dipoles each side of the house
        c = Vector((-4.9, hand * 3.4, tower_top + 2.9))
        m.box(c, (0.12, 2.6, 1.8), "upper", yaw=0.0)
        if fine:
            for j in range(-3, 4):
                m.rod(c + Vector((0.12, j * 0.36, -0.85)), c + Vector((0.12, j * 0.36, 0.85)), 0.025, "steel", sides=3, smooth=False)
            for i in range(-2, 3):
                m.rod(c + Vector((0.12, -1.25, i * 0.38)), c + Vector((0.12, 1.25, i * 0.38)), 0.02, "steel", sides=3, smooth=False)
    m.use("fore")
    m.lathe((-6.8, 0, tower_top), Z, [(0.0, 0.3), (3.6, 0.26), (8.6, 0.12)], "upper", 8 if fine else 5)
    m.rod((-6.8, -3.0, tower_top + 6.0), (-6.8, 3.0, tower_top + 6.0), 0.07, "upper", sides=4)
    head_fore = Vector((-6.8, 0, tower_top + 8.6))
    for hand in (PORT, STBD):
        m.rod((-6.0, hand * 2.4, tower_top), (-6.8, 0, tower_top + 4.4), 0.1, "upper", sides=5)

    # ---- the funnel, raked aft, and the searchlight towers round it
    m.use("aft")
    casing = Model.round_rect(-15.5, -32.5, -5.2, 5.2, 2.0, 3 if fine else 1)            # the casing round her funnel's foot
    slab(m, casing, zC, 19.2)
    walls_detail(yard, casing, zC, 19.2, doors=True, scuttles=False)
    jp_funnel(yard, -23.6, 19.0, -28.6, 31.0, 12.4, 8.8, 10.2, 7.6)
    for (sx, sz) in ((-17.2, 21.0), (-33.6, 22.6)):
        for hand in (PORT, STBD):
            m.use("aft" if sx < yard.break_x else "fore")
            post = Vector((sx, hand * 5.6, zC))
            m.lathe(post, Z, [(0.0, 0.5), (sz - zC - 0.3, 0.42)], "upper", 10 if fine else 5)
            m.box((sx, hand * 6.0, sz - 0.1), (3.0, 3.0, 0.16), "deck")
            if fine:
                railing(m, [(sx + 1.5, hand * 4.5, sz), (sx + 1.5, hand * 7.5, sz), (sx - 1.5, hand * 7.5, sz), (sx - 1.5, hand * 4.5, sz)], every=1.0)
            yard.searchlight(sx, hand * 6.2, sz - 0.02, 0.75, (0.5, hand, 0.05))

    # ---- the mainmast: a raked pole on a tripod, the Type 13 air-warning ladder on it; the after control tower
    m.use("aft")
    mx = -36.8
    head_main = Vector((mx - 4.2, 0, 41.0))
    m.lathe((mx, 0, zC), (head_main - Vector((mx, 0, zC))).normalized(), [(0.0, 0.55), (12.0, 0.42), ((head_main - Vector((mx, 0, zC))).length, 0.2)],
            "upper", 10 if fine else 5)
    for hand in (PORT, STBD):
        m.rod((mx + 2.6, hand * 2.6, zC), (mx - 1.4, 0, 26.0), 0.22, "upper", sides=6)
    m.rod((mx - 3.2, -3.6, 37.0), (mx - 3.2, 3.6, 37.0), 0.08, "upper", sides=4)
    for hand in (PORT, STBD):              # the Type 13: a tall narrow ladder of dipoles on the yard's end
        a = Vector((mx - 3.2, hand * 3.2, 34.6))
        m.rod(a, a + Z * 3.4, 0.03, "steel", sides=3, smooth=False)
        m.rod(a + Vector((0, hand * 0.6, 0)), a + Vector((0, hand * 0.6, 3.4)), 0.03, "steel", sides=3, smooth=False)
        if fine:
            for j in range(8):
                m.rod(a + Z * (0.2 + j * 0.42), a + Vector((0, hand * 0.6, 0.2 + j * 0.42)), 0.02, "steel", sides=3, smooth=False)
    yard.wire(head_fore, head_main, 0.025, sag=1.4)
    yard.wire(head_main, (-h.L * 0.49, 0, D(-h.L * 0.49) + 6.0), 0.025)
    yard.wire(head_fore, (h.L * 0.49, 0, D(h.L * 0.49) + 1.5), 0.025)
    m.socket("ensign", (mx - 3.6, 0, 36.2), (-1, 0, 0), "aft", "the naval ensign flies from the mainmast's gaff")
    # the after control tower: a round tower with a rangefinder on top
    m.lathe((-41.8, 0, zB), Z, [(0.0, 2.6), (5.0, 2.3), (8.2, 2.2)], "upper", 20 if fine else 8, caps=(False, True))
    if fine:
        for k in range(10):
            a = 2 * math.pi * k / 10
            m.disc((-41.8 + math.cos(a) * 2.32, math.sin(a) * 2.32, zB + 6.4), (math.cos(a), math.sin(a), 0), 0.16, "glass", 8)
    fit.rangefinder(yard, "director.aft", -41.8, 0.0, zB + 8.2, 5.0, bearing=180.0, house=1.8, parent="aft")

    # ---- twelve twin 12.7 cm high-angle mountings in their shields, six each side, in two rows
    n_p = n_s = 0
    for sx, sy, zz in ((-28.6, 13.4, zA), (-18.4, 13.4, zA), (-8.5, 13.4, zA), (-33.0, 8.2, zB), (-24.0, 8.2, zB), (-13.0, 8.2, zB)):
        for hand in (PORT, STBD):
            if hand == PORT:
                n_p += 1
                nm = f"sec.P{n_p}"
            else:
                n_s += 1
                nm = f"sec.S{n_s}"
            bearing = 0.0 if sx > -20.0 else 180.0
            ahead = bearing == 0.0
            lim = (-165, 20) if (hand == PORT) == ahead else (-20, 165)
            fit.jp_twin127(yard, nm, sx, hand * sy, zz, bearing, shield=True, limits=lim)

    # ---- the triple 25 mm mountings: the ring along her deck edge, the corners of the levels, her decks fore and aft
    q = 0
    trip = []
    for x_ in (-30.0, -24.5, -19.0, -13.5, -8.0):                       # the ring on sponsons along her side
        trip.append((x_, 18.3, D(x_), True, 1.9))
    trip += [(-36.8, 14.3, zA, True, 1.9), (-41.6, 10.4, zA, True, 1.9), (-46.0, 6.4, zA, True, 1.8), (-3.2, 15.0, zA, True, 1.9), (1.4, 11.3, zA, True, 1.9),
             (4.6, 6.8, zA, True, 1.8), (-38.0, 8.6, zB, True, 1.7), (-0.5, 6.6, zB, True, 1.7), (-28.0, 4.4, 26.0, False, 1.6)]
    trip += [(5.6, 10.6, D(5.6), False, 1.7), (17.6, 10.6, D(17.6), False, 1.7), (-53.0, 11.0, D(-53.0), False, 1.7), (-67.5, 12.0, D(-67.5), False, 1.7),
             (-82.0, 10.6, D(-82.0), True, 1.8), (-96.6, 15.6, D(-96.6), False, 1.8), (-103.0, 15.0, D(-103.0), False, 1.8), (-128.6, 3.4, D(-128.6), True, 1.4)]
    for x_, y_, z_, shield, tub in trip:
        for hand in (PORT, STBD):
            q += 1
            if abs(y_) > 15.0 and z_ < zA - 1.0:
                sponson(yard, x_, hand * y_, z_ + 0.0, tub + 0.4, hand * (h.half_at(x_) - 0.3))
            out = math.degrees(math.atan2(hand * y_, x_ + 20.0))
            fit.jp_triple25(yard, q, x_, hand * y_, z_, bearing=-out if abs(y_) > 5.0 else (0.0 if x_ > -20 else 180.0), shield=shield, tub=tub)
    for tn, gs, tx, raised in mains:                                     # one on the roof of the second turret and the third, riding it
        if tn in ("turret.B", "turret.Y"):
            q += 1
            p = m.parts[m.part_at[tn]]
            rest = p.rest
            ox = -6.2 if rest == 0.0 else 6.2
            fit.jp_triple25(yard, q, tx + ox, 0.0, p.pivot.z + 2.9, bearing=rest, shield=False, tub=0.0, parent=tn)

    # ---- single 25 mm guns along her forecastle, round the tower, on the quarterdeck
    for gx in (62.0, 70.0, 78.0, 86.0, 94.0):
        for hand in (PORT, STBD):
            fit.jp_single25(yard, gx, hand * (h.half_at(gx) - 1.4), D(gx), facing=hand * 75.0)
    for gx in (-74.0, -88.0, -110.0):
        for hand in (PORT, STBD):
            fit.jp_single25(yard, gx, hand * (h.half_at(gx) - 1.5), D(gx), facing=hand * 100.0)
    for hand in (PORT, STBD):
        fit.jp_single25(yard, -1.0, hand * 5.4, zC + 3.5 + 0.02, facing=hand * 60.0)

    # ---- the stern: two catapults, the crane lying along her centre line, the trolley rails
    for hand, b in ((PORT, 175.0), (STBD, 185.0)):
        catapult_us(yard, -120.5, hand * 10.3, D(-120.5) + 0.3, 19.5, b, plane=False)
    lattice_crane(yard, -128.0, 0.0, D(-128.0), 3.6, 19.0, bearing=0.0, lift=6.0)
    if fine:
        for hand in (PORT, STBD):                                       # the trolley rails up the quarterdeck, painted
            pts = [Vector((lerp(-110.0, -71.5, t), hand * lerp(10.0, 14.0, t), 0)) for t in [k / 20.0 for k in range(21)]]
            pts = [Vector((p.x, p.y, D(p.x) + 0.26)) for p in pts]
            m.strip(pts, [Z] * len(pts), 1.2, "lino", 0.01)
            for dy in (-0.55, 0.55):
                rail = [p + Vector((0, dy, 0.02)) for p in pts]
                m.strip(rail, [Z] * len(rail), 0.1, "steel", 0.04)
    # the boats' recesses in her sides aft
    for hand in (PORT, STBD):
        hull_panel(yard, -99.5, -74.0, 2.6, 5.8, hand, "steel", off=0.04)

    # ---- forecastle and quarterdeck: anchor gear, bollards, rails, scuttles, staffs, vents, rafts
    yard.anchor_gear(h.L * 0.455, 106.0, spread=3.0)
    for bx in (124.0, 112.0, 96.0, 40.0, -64.0, -92.0, -121.0):
        for hand in (PORT, STBD):
            yard.bollards(bx, hand * (h.half_at(bx) - 1.0), D(bx))
    yard.rails(-h.L * 0.49, h.L * 0.488)
    yard.portholes(-h.L * 0.36, h.L * 0.44, 1.6, every=3.4, skip=[(-100, -73)])
    yard.portholes(-h.L * 0.2, h.L * 0.38, 3.6, every=3.4, skip=[(-100, -73)])
    if fine:
        for vx in (60.0, 42.0, -56.0, -70.0):
            for hand in (PORT, STBD):
                yard.vent(vx, hand * h.half_at(vx) * 0.5, D(vx), 0.5, 1.4)
        for fx, fy in ((-12.0, 16.25), (-26.0, 16.25), (-40.0, 13.6)):
            for hand in (PORT, STBD):
                fit.float_rack(yard, fx, hand * fy, zm + 0.6, (0, hand, 0), n=2)
    m.use("aft")
    sx = -h.L * 0.497
    m.rod((sx + 0.6, 0, D(sx + 0.6)), (sx - 0.4, 0, D(sx + 0.6) + 6.5), 0.08, "upper", sides=4)
    m.use("fore")
    jx = h.L * 0.493
    m.rod((jx, 0, D(jx)), (jx + 0.4, 0, D(jx) + 3.4), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.4, 0, D(jx) + 3.4), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=4, r=3.0, rudders=2)

    # ---- where she burns
    yard.fire_points += [(-3.0, 0.0, zC + 4.0), (-24.0, 0.0, zC + 1.0), (-41.0, 0.0, zB), (11.5, 0.0, 14.4), (-20.0, 14.0, zA), (-20.0, -14.0, zA),
                         (80.0, 0.0, D(80.0)), (-100.0, 0.0, D(-100.0))]
    from ships import clips_for_ship, sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, mains, "battleship")
    aa_clip(m)
    m.info.setdefault("file", {}).update({"class": "Yamato", "navy": "JAP", "year": 1945, "stands_for": "Battleship_JAP",
                                          "paint": "Kure arsenal grey; a pale wooden upper deck, steel forecastle and quarterdeck"})
    return m, yard


# ---- carriers' own pieces ------------------------------------------------------------------------------------------

def deck_outline(port, star):
    """A flight deck's outline from its port edge and its starboard edge, each [(x, y), ...] from aft to fore (y as
    it is, starboard's negative): comes back anticlockwise seen from above, the ends squared across between them."""
    pts = [(x, y) for x, y in port] + [(x, y) for x, y in reversed(star)]
    a = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))
    return pts if a > 0 else list(reversed(pts))


def edge_at(edge, x):
    """An edge's y at x (the edge a list of (x, y) from aft to fore)."""
    if x <= edge[0][0]:
        return edge[0][1]
    for (x0, y0), (x1, y1) in zip(edge, edge[1:]):
        if x <= x1:
            return lerp(y0, y1, (x - x0) / max(x1 - x0, 1e-9))
    return edge[-1][1]


def cut_deck(port, star, x0, x1, every=4.0):
    """The piece of a flight deck between x0 and x1 (aft to fore) as a closed plan, anticlockwise."""
    xs = sorted(set([x0, x1] + [x for x, _ in port + star if x0 < x < x1]))
    fine_xs = []
    for a, b in zip(xs, xs[1:]):
        k = max(1, int((b - a) / every))
        fine_xs += [lerp(a, b, i / k) for i in range(k)]
    fine_xs.append(xs[-1])
    left = [(x, edge_at(port, x)) for x in fine_xs]
    right = [(x, edge_at(star, x)) for x in fine_xs]
    return deck_outline(left, right)


def bent_funnel(yard, x, wall_y, z0, r, out=4.6, drop=3.6, name=None):
    """A funnel led out through her starboard side and turned down toward the water (the Japanese carriers'): an oval
    trunk from the hangar's wall out past the gallery, bending down, its mouth facing down and out."""
    m = yard.on(x)
    fine = m.fine
    n = 18 if fine else 8
    path = []
    steps = 9 if fine else 4
    for i in range(steps + 1):
        t = i / steps
        # (out level from the wall, then round a bend of radius `drop` downward)
        if t < 0.45:
            s = t / 0.45
            path.append(Vector((x - 0.4 * s, wall_y - (out - drop) * s, z0 + 0.25 * s)))
        else:
            s = (t - 0.45) / 0.55
            th = s * math.pi * 0.42
            path.append(Vector((x - 0.4 - 0.8 * s, wall_y - (out - drop) - math.sin(th) * drop, z0 + 0.25 - (1.0 - math.cos(th)) * drop)))
    rings = []
    for i, p in enumerate(path):
        tan = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized()
        side = X.cross(tan)
        if side.length < 1e-6:
            side = Z.copy()
        side.normalize()
        up = tan.cross(side).normalized()
        rr = r * lerp(1.0, 1.08, i / len(path))
        rings.append([p + side * math.cos(2 * math.pi * k / n) * rr * 1.18 + up * math.sin(2 * math.pi * k / n) * rr for k in range(n)])
    m.skin(rings, "upper", (False, False))
    end, tan = path[-1], (path[-1] - path[-2]).normalized()
    lip = [end + (q - end) * 1.08 for q in rings[-1]]
    m.skin([rings[-1], lip, [q + tan * 0.35 for q in lip]], "upper", (False, False))
    m.disc(end + tan * 0.05, tan, r * 0.95, "black", n)
    if fine:                                    # a cage of bars across its mouth, and its plates' seams
        side = X.cross(tan).normalized()
        for j in range(-3, 4):
            c = end + tan * 0.3 + X * (j * r * 0.27)
            m.rod(c - side * r * 1.1, c + side * r * 1.1, 0.04, "steel", sides=3, smooth=False)
        for i in (2, 5):
            if i < len(rings):
                ring = rings[i] + [rings[i][0]]
                cen = path[i]
                m.strip([cen + (q - cen) * 1.01 for q in ring], [(q - cen).normalized() for q in ring], 0.012, "seam", 0.004)
    k = sum(1 for s in m.sockets if s["name"].startswith("funnel.")) + 1
    m.socket(name or f"funnel.{k}", end + tan * 0.6, tan, yard.half(x), "a funnel's mouth, turned down over the side: her smoke rolls out and aft along the water")
    yard.soot_at.append((end.x, end.y, end.z + 2.0, 6.0))


def carrier_finish(yard, lifts, x_fore, x_aft, iz, hz, deck_park, launch_x, land_x, ensign_at, ensign_part):
    """What every carrier's file has: the lifts as parts with their sockets and clips, the deck park's part, the
    launch and landing sockets, the ensign."""
    m = yard.m
    for i, (lx, ly, ll, lw, paint) in enumerate(lifts):
        ln = m.part(f"lift.{i + 1}", yard.half(lx), (lx, ly, iz), slide=(0, 0, -1), slide_limits=[0.0, round(hz, 2)], kind="slide",
                    note="an aircraft lift: slides down to the hangar deck and back; the dark well under it is part of the deck")
        m.use(yard.half(lx))
        plan = Model.round_rect(lx + ll * 0.5, lx - ll * 0.5, ly - lw * 0.5, ly + lw * 0.5, 0.5, 2)
        if abs(ly) < 5.0:                                # (a lift out at the deck's edge has no well under it)
            m.add([Vector((px, py, iz + 0.012)) for px, py in plan], [tuple(range(len(plan)))], "black", False, lambda q: Z)
        m.use(ln)
        m.prism(plan, iz - 0.3, iz + 0.035, "upper", top_paint=paint, bottom=True)
        if abs(ly) >= 5.0:                               # (a deck-edge lift hangs on girders off its guides)
            for k in range(5):
                gx = lx + (k - 2) * (ll - 1.0) / 4.0
                m.box((gx, ly, iz - 0.75), (0.35, lw - 0.4, 0.9), "upper")
            m.box((lx, ly - math.copysign(lw * 0.5 - 0.3, ly), iz - 0.75), (ll - 0.4, 0.4, 0.9), "upper")
        if m.fine:
            for dx in (-0.5, 0.5):
                m.box((lx + dx * (ll - 0.2), ly, iz + 0.045), (0.14, lw - 0.6, 0.02), "seam")
            for dy in (-0.5, 0.5):
                m.box((lx, ly + dy * (lw - 0.2), iz + 0.045), (ll - 0.6, 0.14, 0.02), "seam")
        m.socket(f"lift.{i + 1}", (lx, ly, iz + 0.1), (0, 0, 1), ln, "an aircraft comes up here")
    m.part("deckpark", "aft", (yard.h.L * -0.26, 0, iz), kind="fixed",
           note="where her aircraft are ranged: the range named in `deck_park` stands at this pivot (its x forward) and rides with this part")
    m.info.setdefault("file", {})["deck_park"] = deck_park
    m.socket("launch", (launch_x, 0, iz + 1.5), (1, 0, 0), "fore", "an aircraft leaves the deck here, climbing ahead")
    m.socket("land", (land_x, 0, iz + 2.5), (-1, 0, 0.1), "aft", "an aircraft comes in over the stern here")
    m.socket("ensign", tuple(ensign_at), (-1, 0, 0), ensign_part, "the battle ensign flies from the island's mast")


def carrier_clips(m, yard, n_lifts, hz):
    from ships import clips_for_ship
    clips_for_ship(m, yard, [], "carrier")
    for i in range(n_lifts):
        down = round(hz, 2)
        m.clips[f"lift.{i + 1}"] = {"long": 16.0, "loop": False, "about": "a lift goes down to the hangar, waits for an aircraft, and comes up with it",
                                   "tracks": [{"part": f"lift.{i + 1}", "slide": [[0.0, 0.0], [5.0, down], [10.0, down], [15.0, 0.0], [16.0, 0.0]]}],
                                   "events": [{"t": 0.0, "fx": "", "socket": f"lift.{i + 1}", "cue": "lift_motor"}]}
    aa_clip(m)


# the hull's deck edge under the flight deck, off the 1942 plan (half widths as shares of 13 m)
SHOKAKU_PLAN = [(-128.75, 0.0), (-128.3, 0.3), (-127.0, 0.5), (-124.0, 0.66), (-118.0, 0.8), (-110.0, 0.89), (-100.0, 0.95), (-85.0, 0.99), (-70.0, 1.0),
                (40.0, 1.0), (55.0, 0.98), (70.0, 0.94), (85.0, 0.86), (95.0, 0.79), (105.0, 0.72), (112.0, 0.66), (116.0, 0.62), (120.0, 0.52), (124.0, 0.38),
                (127.0, 0.24), (128.5, 0.1), (128.75, 0.0)]
# the flight deck's edges, aft to fore: 242 m, wider to port forward, cut in to starboard beside the island
SHOKAKU_PORT = [(-125.2, 3.5), (-124.0, 7.8), (-122.0, 10.6), (-119.0, 13.0), (-110.0, 14.1), (-100.0, 14.6), (-60.0, 14.7), (35.0, 14.7), (45.0, 14.0),
                (52.0, 13.0), (65.0, 12.2), (80.0, 11.4), (95.0, 10.4), (108.0, 9.6), (114.6, 9.1), (115.8, 7.0), (116.0, 3.0)]
SHOKAKU_STAR = [(-125.2, -3.5), (-124.0, -7.8), (-122.0, -10.6), (-119.0, -13.0), (-110.0, -14.1), (-100.0, -14.6), (-60.0, -14.7), (24.0, -14.7),
                (30.0, -12.4), (50.0, -12.0), (60.0, -11.4), (80.0, -10.4), (95.0, -9.7), (108.0, -9.2), (114.6, -8.8), (115.8, -6.8), (116.0, -3.0)]


def shokaku(fine=True):
    """Shokaku as she was in 1942 (her sister Zuikaku the same): 257.5 m by 26, a 242 m wooden flight deck over two
    hangars, three lifts, a small island well forward to starboard, two funnels turned down over the starboard side,
    eight twin 12.7 cm high-angle mountings and the triple 25 mm on sponsons along her sides, all manned; the Type 21
    radar on her island, as fitted that autumn."""
    paints = livery("JAP", "carrier")
    paints.update({"flight": "B79E74", "planks": "B79E74", "plane": "3E4A3C"})
    paints = fit.crew_paints(paints, "JAP")
    m = Model("Shokaku", "Aircraft carrier, Shokaku class, 1941-42 (our own model)", "ship", paints, fine)
    h = PlanHull(SHOKAKU_PLAN, L=257.5, B=26.0, D=9.9, T=8.6, sheer=1.3, rake=8.0, counter=3.0, flare=0.75, fine=1.65, full=0.7, wl_fore=0.5, bilge=6.0,
                 step_aft_at=-104.5, step_aft=2.5)
    bx = -2.0
    yard = Yard(m, h, break_x=bx)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("deck")
    D = h.deck_z
    fd, iz = 13.7, 14.4                       # the flight deck's underside and its top
    zg = 12.2                                 # the galleries along her sides, where her guns stand

    # ---- the hangar's walls from her deck to the flight deck, following her sides, in two halves at her break
    hx0, hx1 = 106.0, -104.5
    for x0, x1, part in ((bx, hx0, "fore"), (hx1, bx, "aft")):
        xs = [lerp(x0, x1, k / 24.0) for k in range(25)]
        plan = sym([(x, 0.0) for x in [x1]] + [(x, h.half_at(x) * 0.97) for x in reversed(xs)] + [(x0, 0.0)])
        m.use(part)
        slab(m, plan, D(x0 if part == "fore" else x1) - 0.5, fd, paint="hull", top=None)
    if fine:            # the openings in her upper hangar's walls, shutters drawn
        x = hx0 - 8.0
        while x > hx1 + 6.0:
            for hand in (PORT, STBD):
                if not (hand == STBD and (-12.0 < x < 8.0 or 26.0 < x < 50.0)):
                    yy = hand * (h.half_at(x) * 0.97 + 0.03)
                    yard.on(x).box((x, yy, D(x) + 1.9), (6.2, 0.06, 2.0), "door")
            x -= 9.5
    # the galleries: a walkway along each side under the flight deck's edge, on brackets, railed
    for hand in (PORT, STBD):
        for a, b in ((hx1 + 2.0, bx), (bx, hx0 - 6.0)):
            pts = [lerp(a, b, k / 12.0) for k in range(13)]
            m.use(yard.half((a + b) * 0.5))
            strip = [Vector((x, hand * (h.half_at(x) * 0.97 + 0.65), zg)) for x in pts]
            m.strip(strip, [Z] * len(strip), 1.3, "deck", 0.1)
            if fine:
                from ships import railing
                railing(m, [Vector((x, hand * (h.half_at(x) * 0.97 + 1.27), zg)) for x in pts], high=1.0, every=2.4)
                for x in pts[::2]:
                    m.rod((x, hand * h.half_at(x) * 0.97, zg - 1.5), (x, hand * (h.half_at(x) * 0.97 + 1.2), zg - 0.08), 0.06, "upper", sides=4)

    # ---- the flight deck, its supports over the open bow and stern, its lines, wires, the stripes on its after end
    for x0, x1, part in ((bx, 116.0, "fore"), (-125.2, bx, "aft")):
        m.use(part)
        slab(m, cut_deck(SHOKAKU_PORT, SHOKAKU_STAR, x0, x1), fd, iz, paint="upper", top="flight", bottom=True)
    for px in (114.0, 109.5):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.6, 4.4)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.25, fd), 0.32, "upper", sides=8 if fine else 5)
    for px in (-110.0, -118.0, -123.0):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.6, 5.0)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.3, fd), 0.32, "upper", sides=8 if fine else 5)
    if fine:            # the overhang's brackets
        x = 100.0
        while x > -100.0:
            for hand in (PORT, STBD):
                e = edge_at(SHOKAKU_PORT if hand == PORT else SHOKAKU_STAR, x)
                w0 = h.half_at(x) * 0.97
                if abs(e) - w0 > 0.6 and not (hand == STBD and 24.0 < x < 50.0):
                    yard.on(x).rod((x, hand * w0, fd - 2.4), (x, e - hand * 0.3, fd - 0.05), 0.1, "upper", sides=4)
            x -= 7.0
    lifts = [(40.9, 0.2, 12.6, 15.5, "flight"), (-20.9, 0.0, 13.2, 12.0, "flight"), (-85.6, 0.0, 12.7, 12.0, "flight")]
    x = 112.0
    while x > -118.0:   # the centre line, dashed; two side lines whole
        if not any(abs(x - lx) < ll * 0.5 + 3.4 for lx, _, ll, _, _ in lifts):
            yard.on(x).box((x, 0, iz + 0.02), (5.0, 0.4, 0.03), "line")
        x -= 9.0
    for hand in (PORT, STBD):
        for a, b in ((-118.0, bx), (bx, 92.0)):
            pieces, last = [], a
            for lx, _, ll, _, _ in sorted(lifts):
                if a < lx < b:
                    pieces.append((last, lx - ll * 0.5 - 0.6))
                    last = lx + ll * 0.5 + 0.6
            pieces.append((last, b))
            for p0, p1 in pieces:
                if p1 - p0 > 1.0:
                    yard.on((p0 + p1) * 0.5).box(((p0 + p1) * 0.5, hand * 3.8, iz + 0.02), (p1 - p0, 0.3, 0.03), "line")
    m.use("aft")
    for xw in (-46.0, -52.0, -58.0, -64.0, -70.0, -76.0, -97.0, -103.0, -109.0):   # the arrester wires, on their sheaves
        w_ = min(edge_at(SHOKAKU_PORT, xw), -edge_at(SHOKAKU_STAR, xw)) - 1.8
        m.rod((xw, -w_, iz + 0.12), (xw, w_, iz + 0.12), 0.035, "steel", sides=3, smooth=False)
        if fine:
            for hand in (PORT, STBD):
                m.lathe((xw, hand * w_, iz), Z, [(0.0, 0.22), (0.18, 0.18)], "steel", 8)
    m.use("fore")
    for xb in (12.0, 18.0):                                                # the crash barriers, lowered
        m.rod((xb, -11.0, iz + 0.18), (xb, 11.0, iz + 0.18), 0.05, "steel", sides=4, smooth=False)
        for hand in (PORT, STBD):
            m.box((xb, hand * 11.4, iz + 0.25), (0.4, 0.4, 0.5), "steel")
    m.use("aft")
    aft_edge = [(y, x) for x, y in SHOKAKU_PORT[:5]]                      # (the after edge: x where the deck ends, by y)
    for k in range(9):                                                     # the red and white stripes on her round-down, fore and aft
        yy = (k - 4) * 2.4
        x0 = edge_at(aft_edge, abs(yy) + 1.2) + 0.25
        if x0 < -120.8:
            m.box(((x0 - 120.4) * 0.5, yy, iz + 0.025), (-120.4 - x0, 2.4 if k % 2 == 0 else 2.38, 0.03), "red" if k % 2 == 0 else "white")

    # ---- the island: a sponson built out from her starboard side, three small levels, the rangefinder and the
    # Type 21 aerial on top; the tripod mast just aft of it
    m.use("fore")
    spon = Model.round_rect(48.0, 31.0, -19.0, -12.0, 1.2, 3 if fine else 1)
    slab(m, spon, zg - 0.3, iz, paint="hull", top="deck")
    for dx in (-6.0, 0.0, 6.0):
        m.rod((39.5 + dx, -13.0, zg - 3.5), (39.5 + dx, -18.6, zg - 0.4), 0.18, "upper", sides=5)
    tz = iz
    for k, (xf, xa, y0, y1, hh) in enumerate(((47.0, 33.0, -18.6, -13.0, 2.9), (46.0, 36.5, -18.4, -13.4, 2.7), (44.6, 38.6, -18.0, -14.0, 2.3))):
        pl = Model.round_rect(xf, xa, y0, y1, 1.2 if k < 2 else 0.9, 3 if fine else 1)
        slab(m, pl, tz, tz + hh)
        walls_detail(yard, pl, tz, tz + hh, doors=k == 0, scuttles=k == 0)
        if k >= 1 and fine:
            yard.windows(xf, (y1 - y0) * 0.5 - 0.3, tz + hh - 1.35, 0.85, y=(y0 + y1) * 0.5, back=2.6)
        tz += hh
    fit.rangefinder(yard, "director.fore", 41.4, -16.0, tz, 2.3, bearing=0.0, house=1.1, parent="fore")
    m.use("fore")
    c = Vector((39.0, -16.0, tz + 1.2))
    m.rod((39.0, -16.0, tz), c + Z * 0.3, 0.12, "upper", sides=5)
    m.box(c + Z * 1.2, (0.12, 2.8, 1.8), "upper")
    if fine:
        for j in range(-3, 4):
            m.rod(c + Vector((0.12, j * 0.38, 0.35)), c + Vector((0.12, j * 0.38, 2.05)), 0.025, "steel", sides=3, smooth=False)
    mx, my = 30.0, -15.6
    head = Vector((mx - 0.8, my, 29.8))
    m.rod((mx + 0.2, my, iz), head, 0.24, "upper", rb=0.12, sides=7 if fine else 5)
    for hand in (PORT, STBD):
        m.rod((mx - 2.2, my + hand * 1.6, iz), (mx - 0.5, my, 24.0), 0.14, "upper", sides=5)
    m.rod((mx - 0.6, my - 2.6, 26.6), (mx - 0.6, my + 2.6, 26.6), 0.07, "upper", sides=4)
    yard.searchlight(43.0, -19.4, zg + 0.1, 0.6, (0.3, -1, 0.05))

    # ---- the two funnels turned down over the starboard side, the forward one the larger
    for fx, r in ((2.0, 2.25), (-7.6, 2.0)):
        bent_funnel(yard, fx, -h.half_at(fx) * 0.97 + 0.4, 11.6, r, out=6.6, drop=4.8)

    # ---- eight twin 12.7 cm high-angle mountings on sponsons, two pairs each side, fore and aft
    n_p = n_s = 0
    for sx in (72.0, 57.0, -57.0, -72.0):
        for hand in (PORT, STBD):
            w0 = h.half_at(sx) * 0.97
            yy = hand * (w0 + 2.7)
            sponson(yard, sx, yy, zg, 3.0, hand * w0)
            if hand == PORT:
                n_p += 1
                nm = f"sec.P{n_p}"
            else:
                n_s += 1
                nm = f"sec.S{n_s}"
            bearing = 0.0 if sx > 0 else 180.0
            lim = (-165, 20) if (hand == PORT) == (sx > 0) else (-20, 165)
            fit.jp_twin127(yard, nm, sx, yy, zg, bearing, shield=True, limits=lim)

    # ---- the triple 25 mm mountings in tubs on sponsons along her sides, a pair at her bow and her stern
    q = 0
    spots = [(PORT, x_) for x_ in (-97.0, -46.0, -34.0, -22.0, -10.0, 2.0, 22.0, 88.0)]
    spots += [(STBD, x_) for x_ in (-97.0, -46.0, -34.0, 16.0, 88.0)]
    for hand, x_ in spots:
        q += 1
        w0 = h.half_at(x_) * 0.97
        yy = hand * (w0 + 2.3)
        if any(abs(x_ - sx) < 5.5 for sx in (72.0, 57.0, -57.0, -72.0)):
            continue
        sponson(yard, x_, yy, zg, 2.3, hand * w0)
        fit.jp_triple25(yard, q, x_, yy, zg, bearing=(90.0 if hand == STBD else -90.0) + (-25.0 if x_ > 0 else 25.0) * (1 if hand == STBD else -1), shield=False, tub=1.9)
    for x_ in (120.5, -126.6):
        for hand in (PORT, STBD):
            q += 1
            fit.jp_triple25(yard, q, x_, hand * 3.2, D(x_), bearing=0.0 if x_ > 0 else 180.0, shield=False, tub=1.7)

    # ---- the radio masts each side, lowered outboard for flying, on their hinges at the galleries
    for mxx in (-48.0, -79.0):
        for hand in (PORT, STBD):
            foot = Vector((mxx, hand * (h.half_at(mxx) * 0.97 + 1.3), zg + 0.2))
            tip = foot + Vector((0.0, hand * 12.0, 1.4))
            m_ = yard.on(mxx)
            for dx in (-0.35, 0.35):
                m_.rod(foot + X * dx, tip + X * dx * 0.3, 0.07, "upper", sides=4)
            if fine:
                for k in range(1, 12):
                    a = foot.lerp(tip, k / 12.0)
                    m_.rod(a - X * 0.33 * lerp(1.0, 0.3, k / 12.0), a + X * 0.33 * lerp(1.0, 0.3, k / 12.0), 0.025, "upper", sides=3, smooth=False)

    # ---- her flight deck's crew: handlers by the aircraft ranged aft and at the lifts, officers by the island
    if fine:
        rg = __import__("random").Random(1942)
        for cx, cy, k_ in ((-60.0, 0.0, 7), (-21.0, 7.5, 3), (36.0, -10.6, 4), (50.0, 4.0, 2)):
            for i in range(k_):
                a = rg.uniform(0, 2 * math.pi)
                foot = Vector((cx + rg.uniform(-5.0, 5.0), cy + rg.uniform(-4.0, 4.0), iz + 0.035))
                fit.sailor(m.use(yard.half(foot.x)), foot, Vector((math.cos(a), math.sin(a), 0)), "stand", navy="JAP")

    # ---- boats, the forecastle's anchor gear, her quarterdeck, rails, scuttles
    for hand in (PORT, STBD):
        yard.boat(-114.5, hand * 4.6, D(-114.5) + 0.5, 8.5)
    yard.anchor_gear(h.L * 0.468, 121.5, spread=2.2)
    for bx_ in (124.0, 117.0, -121.0, -127.0):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.8), D(bx_))
    yard.rails(hx0 + 1.5, h.L * 0.488)
    yard.rails(-h.L * 0.492, hx1 - 1.0)
    yard.portholes(-h.L * 0.38, h.L * 0.42, 2.0, every=3.4)
    yard.portholes(-h.L * 0.36, h.L * 0.4, 5.2, every=3.4)
    m.use("aft")
    sx = -h.L * 0.497
    m.rod((sx + 0.6, 0, D(sx + 0.6)), (sx - 0.4, 0, D(sx + 0.6) + 5.0), 0.07, "upper", sides=4)
    m.use("fore")
    jx = h.L * 0.493
    m.rod((jx, 0, D(jx)), (jx + 0.4, 0, D(jx) + 3.0), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.4, 0, D(jx) + 3.0), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=4, r=2.1, rudders=1)

    carrier_finish(yard, lifts, 116.0, -125.2, iz, iz - D(0.0) - 0.4, [{"model": "DeckPark_Zero"}], 108.0, -112.0, head - Vector((0.8, 0, 3.0)), "fore")
    yard.fire_points += [(52.0, 0.0, iz), (-20.0, 0.0, iz), (-70.0, 0.0, iz), (40.0, -16.0, iz + 2.0), (-3.0, -15.0, zg)]
    from ships import sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    carrier_clips(m, yard, len(lifts), iz - D(0.0) - 0.4)
    m.info.setdefault("file", {}).update({"class": "Shokaku", "navy": "JAP", "year": 1941, "stands_for": "Carrier_JAP",
                                          "paint": "IJN grey; an unpainted wooden flight deck with white lines, red and white stripes on its after end"})
    return m, yard


# the flight deck's edges, aft to fore: 262.7 m, 29.3 wide, tapering forward to its square end; to starboard it is
# narrowed a little beside the island, which stands out over her side on a sponson of its own
ESSEX_PORT = [(-130.4, 6.5), (-129.6, 10.6), (-127.6, 13.2), (-124.0, 14.4), (-118.0, 14.65), (62.0, 14.65), (80.0, 14.2), (95.0, 13.4), (108.0, 12.2),
              (118.0, 11.0), (123.6, 10.2), (124.6, 8.6), (124.9, 4.0)]
ESSEX_STAR = [(-130.4, -6.5), (-129.6, -10.6), (-127.6, -13.2), (-124.0, -14.4), (-118.0, -14.65), (-40.0, -14.65), (-36.0, -13.4), (34.0, -13.4),
              (38.0, -14.65), (62.0, -14.65), (80.0, -14.2), (95.0, -13.4), (108.0, -12.2), (118.0, -11.0), (123.6, -10.2), (124.6, -8.6), (124.9, -4.0)]
# her hull's deck edge at the hangar deck (half widths as shares of 14.15 m): long and full amidships, a broad
# rounded stern under the flight deck's overhang, the bow fine at the water and flared out above it
ESSEX_PLAN = [(-132.9, 0.0), (-132.6, 0.3), (-131.8, 0.46), (-130.0, 0.6), (-126.0, 0.74), (-120.0, 0.84), (-112.0, 0.92), (-100.0, 0.975),
              (-86.0, 1.0), (30.0, 1.0), (48.0, 0.97), (64.0, 0.91), (80.0, 0.8), (92.0, 0.68), (104.0, 0.53), (112.0, 0.42), (118.0, 0.33),
              (123.0, 0.24), (127.0, 0.16), (130.0, 0.09), (132.9, 0.01)]
ESSEX_PAINT = {"hull": "3E4A5C", "upper": "3E4A5C", "deck": "3B4757", "planks": "3B4757", "flight": "47525F", "roof": "36414F",
               "door": "4A5566", "seam": "35404D", "plane": "3E4A5C"}


def open_5in(yard, name, x, y, z, bearing, limits, long=5.6, shield=True, rest_elev=8.0):
    """A single 5-inch/38 in an open mounting on the gallery: the gun on its pedestal behind a low shield, its loaders
    and pointers standing round it. A `sec.*` turret. (`long` 3.6 and no shield: the short 5-inch/25 of 1941.)"""
    m = yard.m
    tn, guns = yard.turret(name, x, z, 3.4, 1, long, bearing, 0.07, y=y, style="open", rate=15.0, elev=(-15, 85), rest_elev=rest_elev, limits=limits, floats=False)
    if m.fine:
        p = m.parts[m.part_at[tn]]
        m.use(tn)
        rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
        o = Vector(p.pivot)

        def W(px, py, pz):
            return o + rot @ Vector((px, py, pz))
        f = rot @ X
        for side in (1, -1):
            if shield:
                m.box(W(0.7, side * 0.9, 0.95), (0.08, 1.0, 1.3), "upper", yaw=bearing + side * 18.0)    # the low shield either side of the gun
            fit.sailor(m, W(-0.3, side * 1.15, 0.0), f, "aim", hands=(W(0.1, side * 0.75, 1.2), W(0.2, side * 0.75, 1.0)), navy="USA", lean=0.06)
        fit.sailor(m, W(-1.7, 0.4, 0.0), f, "load", navy="USA")
        fit.sailor(m, W(-2.4, -0.6, 0.0), f, "carry", navy="USA")
    return tn, guns


def essex(fine=True):
    """Essex, first of her class, as she was in 1944: 265.8 m by 28.3, a 262.7 m flight deck over an open hangar with
    its roller curtains, two lifts on the centre line and one on the port deck edge, the tall island to starboard with
    its funnel, tripod and radars, four twin 5-inch gun houses on the flight deck either end of it and four single
    5-inch on the port gallery, the quadruple 40 mm and the 20 mm round her galleries, all manned. Measure 21: navy blue
    everywhere, her flight deck stained dark blue."""
    paints = livery("USA", "carrier")
    paints.update(ESSEX_PAINT)
    paints = fit.crew_paints(paints, "USA")
    m = Model("Essex", "Aircraft carrier, Essex class, 1944 (our own model)", "ship", paints, fine)
    h = PlanHull(ESSEX_PLAN, L=265.8, B=28.3, D=8.1, T=8.7, sheer=2.2, sheer_aft=0.3, rake=10.0, counter=4.6, flare=0.95, fine=1.6, full=0.72,
                 wl_fore=0.5, bilge=6.0)
    bx = -2.0
    yard = Yard(m, h, break_x=bx)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("deck")
    D = h.deck_z
    fd, iz = 16.1, 16.8                       # the flight deck's underside and its top
    zg = fd - 1.3                             # the galleries round her, just under the flight deck's edge
    hz = iz - D(0.0)                          # how far a lift goes down to the hangar deck

    # ---- the hangar: open sides closed by roller curtains, a few bays open; the hangar's walls follow her sides
    hx0, hx1 = 112.0, -118.0
    for x0, x1, part in ((bx, hx0, "fore"), (hx1, bx, "aft")):
        xs = [lerp(x0, x1, k / 26.0) for k in range(27)]
        plan = sym([(x1, 0.0)] + [(x, h.half_at(x) * 0.97) for x in reversed(xs)] + [(x0, 0.0)])
        m.use(part)
        slab(m, plan, D(x0 if part == "fore" else x1) - 0.5, fd, paint="hull", top=None)
    if fine:
        x, k = hx0 - 9.0, 0
        while x > hx1 + 6.0:
            for hand in (PORT, STBD):
                if hand == STBD and -40.0 < x < 36.0:
                    continue                                    # (the island's sponson and the boiler uptakes there)
                if hand == PORT and -8.0 < x < 14.0:
                    continue                                    # (the deck-edge lift's opening)
                yy = hand * (h.half_at(x) * 0.97 + 0.03)
                open_bay = (k + (1 if hand == STBD else 0)) % 5 == 2
                yard.on(x).box((x, yy, D(x) + 3.0), (11.0, 0.06, 5.2), "black" if open_bay else "door")
                if not open_bay:                               # the curtain's slats
                    for j in range(1, 7):
                        yard.on(x).box((x, yy + hand * 0.02, D(x) + 0.4 + j * 0.74), (10.8, 0.03, 0.05), "seam")
            x -= 12.5
            k += 1
    # the galleries round her under the flight deck's edge, on brackets, railed
    for hand in (PORT, STBD):
        for a, b in (((-118.0, -7.4), (13.4, 100.0)) if hand == PORT else ((-118.0, bx), (bx, 100.0))):
            pts = [lerp(a, b, k / 14.0) for k in range(15)]
            m.use(yard.half((a + b) * 0.5))
            edge = ESSEX_PORT if hand == PORT else ESSEX_STAR
            strip = [Vector((x, hand * (abs(edge_at(edge, x)) - 0.2), zg)) for x in pts]
            m.strip(strip, [Z] * len(strip), 1.4, "deck", 0.1)
            if fine:
                from ships import railing
                railing(m, [Vector((x, hand * (abs(edge_at(edge, x)) + 0.45), zg)) for x in pts], high=1.0, every=2.4)
                for x in pts[::2]:
                    w0 = h.half_at(x) * 0.97
                    m.rod((x, hand * w0, zg - 2.0), (x, hand * (abs(edge_at(edge, x)) + 0.3), zg - 0.08), 0.07, "upper", sides=4)

    # ---- the flight deck, its pillars over the open bow and fantail, its lines, catapults, wires, barriers
    for x0, x1, part in ((bx, 124.9, "fore"), (-130.4, bx, "aft")):
        m.use(part)
        slab(m, cut_deck(ESSEX_PORT, ESSEX_STAR, x0, x1), fd, iz, paint="upper", top="flight", bottom=True)
    for px in (121.0, 116.0):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.6, 4.2)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.4, fd), 0.3, "upper", sides=8 if fine else 5)
    for px in (-121.0, -127.0):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.6, 5.5)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.3, fd), 0.32, "upper", sides=8 if fine else 5)
    lifts = [(64.0, 0.0, 13.4, 14.6, "flight"), (3.0, 14.65 + 5.0, 18.3, 10.4, "flight"), (-58.0, 0.0, 13.4, 14.6, "flight")]
    x = 120.0
    while x > -124.0:                     # the dashed centre line
        if not any(abs(x - lx) < ll * 0.5 + 3.2 and abs(ly) < 1.0 for lx, ly, ll, _, _ in lifts):
            yard.on(x).box((x, 0, iz + 0.02), (4.5, 0.35, 0.03), "line")
        x -= 9.0
    for hand in (PORT, STBD):             # the deck-edge lines
        edge = ESSEX_PORT if hand == PORT else ESSEX_STAR
        for a, b in ((-122.0, bx), (bx, 118.0)):
            xs = [lerp(a, b, k / 10.0) for k in range(11)]
            line = [Vector((x, hand * (abs(edge_at(edge, x)) - 1.0), iz + 0.02)) for x in xs]
            m.use(yard.half((a + b) * 0.5))
            m.strip(line, [Z] * len(line), 0.3, "line", 0.01)
    m.use("fore")
    for cy in (3.6, -3.6):                # the two hydraulic catapults' tracks forward
        m.box((101.0, cy, iz + 0.03), (44.0, 0.5, 0.05), "steel")
        m.box((122.0, cy, iz + 0.05), (1.6, 1.1, 0.12), "steel")
    for xb in (24.0, 30.0):              # the crash barriers, lowered
        m.rod((xb, -11.0, iz + 0.18), (xb, 11.0, iz + 0.18), 0.05, "steel", sides=4, smooth=False)
        for hand in (PORT, STBD):
            m.box((xb, hand * 11.4, iz + 0.3), (0.4, 0.4, 0.6), "steel")
    m.use("aft")
    for xw in (-72.0, -78.0, -84.0, -90.0, -96.0, -102.0, -108.0, -114.0):
        w_ = min(edge_at(ESSEX_PORT, xw), -edge_at(ESSEX_STAR, xw)) - 2.0
        m.rod((xw, -w_, iz + 0.12), (xw, w_, iz + 0.12), 0.035, "steel", sides=3, smooth=False)
        if fine:
            for hand in (PORT, STBD):
                m.lathe((xw, hand * w_, iz), Z, [(0.0, 0.22), (0.18, 0.18)], "steel", 8)
    # the deck-edge lift's guides down her side
    for dx in (-7.6, 7.6):
        yard.on(3.0 + dx).box((3.0 + dx, h.half_at(3.0) * 0.97 + 0.35, D(3.0) + hz * 0.5), (0.5, 0.5, hz), "upper")

    # ---- the island: its sponson out over her starboard side, four levels, the Mk 37s fore and aft, the tripod with
    # the SK and SG, the big funnel over its after half, two quadruple 40 mm at its ends
    m.use("fore")
    yi = -16.6                                # (the middle of its foot)
    for pl, part in ((Model.round_rect(15.0, bx, -20.0, -13.0, 1.4, 3 if fine else 1), "fore"), (Model.round_rect(bx, -20.0, -20.0, -13.0, 1.4, 3 if fine else 1), "aft")):
        m.use(part)
        slab(m, pl, zg - 0.3, iz, paint="hull", top="deck")
    for dx in (-15.0, -7.0, 1.0, 9.0):
        yard.on(dx).rod((dx, -h.half_at(dx) * 0.97, zg - 4.0), (dx, -19.4, zg - 0.4), 0.2, "upper", sides=5)
    m.use("fore")
    tz = iz
    levels = ((14.0, bx + 0.01, 2.6, 3.0, "base"), (10.6, -2.8, 2.9, 2.7, "flag"), (9.8, -2.2, 2.9, 2.6, "nav"), (8.4, -1.4, 2.6, 2.5, "pilot"))
    for k, (xf, xa, hw, hh, what) in enumerate(levels):
        yc = yi - (0.5 if k else 0.0)
        pl = Model.round_rect(xf, xa, yc - hw, yc + hw, 1.0, 3 if fine else 1)
        slab(m, pl, tz, tz + hh)
        walls_detail(yard, pl, tz, tz + hh, doors=k == 0, scuttles=k == 0)
        if what in ("nav", "pilot", "flag") and fine:
            yard.windows(xf, hw - 0.3, tz + hh - 1.4, 0.9, y=yc, back=3.4)
        tz += hh
    m.use("aft")
    after = Model.round_rect(bx - 0.01, -20.0, yi - 2.6, yi + 2.6, 1.0, 3 if fine else 1)
    slab(m, after, iz, iz + 3.0)
    walls_detail(yard, after, iz, iz + 3.0, doors=True, scuttles=True)
    house_aft = Model.round_rect(-14.5, -19.6, yi - 2.2, yi + 2.2, 0.9, 3 if fine else 1)
    slab(m, house_aft, iz + 3.0, iz + 6.4)
    walls_detail(yard, house_aft, iz + 3.0, iz + 6.4, doors=False, scuttles=True)
    fit.mk37(yard, "director.fore", 4.0, yi - 0.5, tz, 0.0, parent="fore")
    fit.mk37(yard, "director.aft", -17.0, yi, iz + 6.4, 180.0, parent="aft")
    # the funnel over the island's after half
    us_funnel(yard, -8.6, iz + 3.0, iz + 15.6, 12.4, 5.6, slope=1.2, platform=None, y=yi)
    # the tripod on the pilot house's roof, between the bridge and the funnel
    m.use("fore")
    mx = 0.6
    head = Vector((mx, yi, iz + 27.0))
    m.lathe((mx, yi, tz), Z, [(0.0, 0.34), (iz + 14.0 - tz, 0.28), (iz + 24.0 - tz, 0.13)], "upper", 8 if fine else 5)
    for hand in (PORT, STBD):
        m.rod((mx - 1.6, yi + hand * 2.0, tz), (mx, yi, iz + 15.0), 0.16, "upper", sides=6)
    m.box((mx + 1.2, yi, iz + 18.0), (4.6, 3.8, 0.16), "deck")
    m.rod((mx + 0.0, yi, iz + 15.0), (mx + 3.0, yi, iz + 17.9), 0.1, "upper", sides=5)
    fit.sk_radar(yard, "radar.air", mx + 2.0, yi, iz + 18.1, size=5.0, parent="fore")
    fit.sg_radar(yard, "radar.surface", mx, yi, iz + 24.6, wide=1.5, parent="fore")
    m.rod((mx - 0.2, yi - 3.0, iz + 22.6), (mx - 0.2, yi + 3.0, iz + 22.6), 0.07, "upper", sides=4)
    fit.bofors_quad(yard, 1, 12.6, yi + 0.4, iz + 3.0, bearing=0.0, tub=2.6, parent="fore")
    fit.bofors_quad(yard, 2, -21.8, yi, iz + 0.0, bearing=180.0, tub=2.6, parent="aft")

    # ---- four twin 5-inch on the flight deck, the inner of each pair firing over the outer
    for nm, gx, base, bearing in (("sec.S1", 29.5, 0.0, 0.0), ("sec.S2", 21.6, 2.7, 0.0), ("sec.S3", -27.4, 2.7, 180.0), ("sec.S4", -35.4, 0.0, 180.0)):
        lim = (-30, 150) if bearing == 0.0 else (-150, 30)
        m.use(yard.half(gx))
        slab(m, Model.round_rect(gx + 3.4, gx - 3.4, yi - 2.9, yi + 2.9, 1.0, 3 if fine else 1), zg - 0.3, iz, paint="hull", top="deck")
        fit.twin_5in(yard, nm, gx, yi, iz, bearing, limits=lim, base=base)
    # ---- four single 5-inch open mountings on the port gallery, two forward, two aft
    for k, gx in enumerate((44.0, 34.0, -38.0, -48.0)):
        w0 = h.half_at(gx) * 0.97
        yy = edge_at(ESSEX_PORT, gx) + 2.4
        sponson(yard, gx, yy, zg - 0.1, 3.2, w0)
        bearing = 0.0 if gx > 0 else 180.0
        open_5in(yard, f"sec.P{k + 1}", gx, yy, zg - 0.1, bearing, (-165, 20) if gx > 0 else (-20, 165))

    # ---- the quadruple 40 mm round her: forward corners, galleries, fantail
    q = 2
    quads = [(111.0, PORT), (111.0, STBD), (88.0, PORT), (78.0, STBD), (-20.0, PORT), (-70.0, PORT), (-86.0, STBD), (-102.0, PORT), (-108.0, STBD)]
    for gx, hand in quads:
        q += 1
        edge = ESSEX_PORT if hand == PORT else ESSEX_STAR
        yy = hand * (abs(edge_at(edge, gx)) + 2.2)
        sponson(yard, gx, yy, zg - 0.6, 3.1, hand * h.half_at(gx) * 0.97)
        fit.bofors_quad(yard, q, gx, yy, zg - 0.6, bearing=hand * (-80.0 if gx > 0 else -100.0), tub=2.7)
    for hand in (PORT, STBD):              # two on the fantail under the flight deck's overhang
        q += 1
        fit.bofors_quad(yard, q, -125.0, hand * 6.2, D(-125.0), bearing=180.0 + hand * 30.0, tub=2.5)

    # ---- the 20 mm along her galleries
    for hand in (PORT, STBD):
        edge = ESSEX_PORT if hand == PORT else ESSEX_STAR
        for gx in (94.0, 70.0, 58.0, 52.0, 24.0, 18.0, -12.0, -28.0, -56.0, -62.0, -78.0, -94.0, -108.0):
            if hand == STBD and -40.0 < gx < 36.0:
                continue
            if hand == PORT and -10.0 < gx < 16.0:
                continue
            if any(abs(gx - qx) < 5.0 and qh == hand for qx, qh in quads) or (hand == PORT and any(abs(gx - sx) < 5.0 for sx in (44.0, 34.0, -38.0, -48.0))):
                continue
            yy = hand * (abs(edge_at(edge, gx)) - 0.25)
            fit.oerlikon(yard, gx, yy, zg, facing=hand * 80.0, loader=(int(gx) % 3 == 0))
    for gx in (12.0, 6.0, -6.0):            # and along the island's sponson
        fit.oerlikon(yard, gx, -19.2, iz, facing=-80.0, loader=False)

    # ---- her flight deck's crew: handlers by the aircraft, at the lifts, by the island
    if fine:
        rg = __import__("random").Random(1944)
        for cx, cy, k_ in ((-82.0, 0.0, 7), (64.0, 10.8, 3), (8.0, -10.0, 5), (3.0, 10.0, 3)):
            for i in range(k_):
                a = rg.uniform(0, 2 * math.pi)
                foot = Vector((cx + rg.uniform(-5.0, 5.0), cy + rg.uniform(-3.0, 3.0), iz + 0.035))
                fit.sailor(m.use(yard.half(foot.x)), foot, Vector((math.cos(a), math.sin(a), 0)), "stand", navy="USA")

    # ---- forecastle and fantail: anchor gear, bollards, rails, scuttles; staffs
    yard.anchor_gear(h.L * 0.47, 118.5, spread=2.2)
    for bx_ in (126.0, 119.0, -122.0, -128.0):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.8), D(bx_))
    yard.rails(hx0 + 1.5, h.L * 0.488)
    yard.rails(-h.L * 0.49, hx1 - 1.0)
    yard.portholes(-h.L * 0.4, h.L * 0.43, 2.0, every=3.4)
    yard.portholes(-h.L * 0.38, h.L * 0.41, 4.6, every=3.4)
    m.use("aft")
    sx = -h.L * 0.497
    m.rod((sx + 0.6, 0, D(sx + 0.6)), (sx - 0.4, 0, D(sx + 0.6) + 5.0), 0.07, "upper", sides=4)
    m.use("fore")
    jx = h.L * 0.493
    m.rod((jx, 0, D(jx)), (jx + 0.4, 0, D(jx) + 3.0), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.4, 0, D(jx) + 3.0), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=4, r=2.3, rudders=1)

    carrier_finish(yard, lifts, 124.9, -130.4, iz, hz, [{"model": "DeckPark_CarrierFighter"}, {"from": "1943-01", "model": "DeckPark_Hellcat"}],
                   112.0, -118.0, head - Vector((1.0, 0, 3.0)), "fore")
    yard.fire_points += [(50.0, 0.0, iz), (-20.0, 0.0, iz), (-80.0, 0.0, iz), (4.0, yi, iz + 6.0), (-8.0, yi, iz + 3.0)]
    from ships import sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    carrier_clips(m, yard, len(lifts), hz)
    m.info.setdefault("file", {}).update({"class": "Essex", "navy": "USA", "year": 1943, "stands_for": "Carrier_USA",
                                          "paint": "Measure 21: navy blue 5-N on every side, deck blue 20-B decks, the flight deck stained dark blue"})
    return m, yard


# ---- the standard-type battleship of 1941 ----------------------------------------------------------------------

def us_turret14(yard, name, x, deck, bearing, raised=0.0, parent=None, limits=(-150, 150), depress=-5):
    """An American 14-inch triple turret of the standard type: a plain gunhouse, its face a single plate sloped back,
    straight sides, a rounded back over the barbette, the rangefinder's ears out each side at the rear, three sighting
    hoods on the roof, the guns close together in their bloomers. Parts and sockets as the yard's turrets. Her three
    guns lay in one common slide: each is a part of its own (it runs back on its own), but they lift together, so a
    battle gives all three the same elevation."""
    m = yard.m
    half = parent or yard.half(x)
    fine = m.fine
    base = deck + raised
    m.use(half)
    m.lathe((x, 0, deck - 0.6), Z, [(0.0, 4.75), (raised + 1.05, 4.75)], "upper", 28 if fine else 12, caps=(False, True))
    piv = Vector((x, 0, base + 1.0))
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")

    def W(px, py, pz):
        return piv + rot @ Vector((px, py, pz))
    tname = m.part(name, half, tuple(piv), axis=(0, 0, -1), limits=list(limits), kind="turn",
                   note=f"trains: plus turns it clockwise seen from above, so its bearing is its rest bearing plus the turn; rest bearing {int(bearing)}; about 4 degrees a second; "
                        "her three guns lay in one slide: lift all three to the same elevation")
    m.parts[m.part_at[tname]].rest = bearing
    m.parts[m.part_at[tname]].rate = 4.0
    m.use(tname)
    H = 3.0
    lo = [(6.1, 3.2), (5.9, 4.7), (-4.5, 4.7), (-5.2, 4.1), (-5.35, 0.0)]
    hi = [(4.7, 3.1), (4.55, 4.55), (-4.4, 4.55), (-5.05, 4.0), (-5.2, 0.0)]
    plan_lo = lo[:-1] + [lo[-1]] + [(px, -py) for px, py in reversed(lo[:-1])]
    plan_hi = hi[:-1] + [hi[-1]] + [(px, -py) for px, py in reversed(hi[:-1])]
    n = len(plan_lo)
    vl = [W(px, py, 0.0) for px, py in plan_lo]
    vh = [W(px, py, H) for px, py in plan_hi]
    mid = W(0.5, 0, H * 0.5)
    m.add(vl + vh, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "upper", False, lambda q: q - mid)
    tris = tessellate_polygon([[Vector((p[0], p[1], 0)) for p in plan_hi]])
    m.add(vh, [tuple(t) for t in tris], "roof", False, lambda q: Z)
    m.add(vl, [tuple(t) for t in tris], "steel", False, lambda q: -Z)
    if fine:
        for k in range(1, 5):                                   # the roof's plates
            m.box(W(4.2 - k * 1.8, 0, H + 0.005), (0.03, 9.0, 0.012), "seam", yaw=bearing)
        for sy in (-2.6, 0.0, 2.6):                             # the sighting hoods
            m.box(W(3.6, sy, H + 0.28), (1.2, 0.85, 0.56), "roof", yaw=bearing)
            m.box(W(4.21, sy, H + 0.3), (0.02, 0.55, 0.16), "glass", yaw=bearing)
        for sy in (-1.6, 1.6):                                  # the hatches
            m.lathe(W(-3.4, sy, H), Z, [(0.0, 0.4), (0.1, 0.4), (0.16, 0.33)], "roof", 10)
        for side in (1, -1):                                    # the rangefinder's ears
            m.box(W(-3.3, side * 5.0, H * 0.6), (1.5, 0.8, 0.95), "upper", yaw=bearing)
            m.disc(W(-2.54, side * 5.2, H * 0.6), rot @ X, 0.16, "glass", 8)
        from ships import ladder
        back = W(-5.3, 1.0, 0.0)
        ladder(m, back, back.z + H, rot @ Vector((-1, 0, 0)))
    guns = []
    fwd, lat = rot @ X, rot @ Y
    gap = 2.35
    for k in range(3):
        off = (k - 1) * gap
        p = W(4.1, off, 1.45)
        gname = m.part(f"{name}.gun.{k + 1}", tname, tuple(p), axis=tuple(-lat), limits=[depress, 30], slide=tuple(-fwd), slide_limits=[0.0, 1.0], kind="turn",
                       note="a gun: plus lifts the muzzle (the turret's three lift together, in one slide); slides back along its own length when it fires")
        m.use(gname)
        e = math.radians(2.0)
        ax = (fwd * math.cos(e) + Z * math.sin(e)).normalized()
        reach, long = 1.3, 13.6
        prof = [(0.0, 0.52), (reach + 0.3, 0.5), (reach + 2.0, 0.46), (reach + 2.4, 0.42), (reach + long * 0.55, 0.35), (reach + long * 0.93, 0.29),
                (reach + long * 0.95, 0.31), (reach + long, 0.3)]
        m.lathe(p, ax, prof, "upper", 14 if fine else 8, caps=(True, False))
        tip = p + ax * (reach + long)
        m.disc(tip - ax * 0.02, ax, 0.3, "upper", 14 if fine else 8)
        m.disc(tip + ax * 0.005, ax, 0.178, "black", 12 if fine else 6)
        m.lathe(p + ax * (reach - 0.5), ax, [(0.0, 0.85), (0.4, 0.82), (1.1, 0.7), (2.3, 0.5)], "bag", 12 if fine else 6, caps=(False, False))
        m.socket(f"muzzle.{name}.{k + 1}", tip, ax, gname, "flash, smoke and the shell leave here")
        guns.append(gname)
    m.use(half)
    return tname, guns, W, H


# a casemate's port in her superstructure's side: half its width, its sill and its head from the gun's axis, its depth
CASEMATE_PORT = (1.5, 0.85, 0.85, 0.9)


def ported_slab(m, plan, z0, z1, ports, paint="upper", top="deck"):
    """`slab` with a hole in its side wall for each casemate port: `ports` [(x, y of the wall, height of the gun's
    axis)], each hole CASEMATE_PORT's size (`casemate_5in` lines it)."""
    w2, lo, hi, _ = CASEMATE_PORT
    keep = []
    for k, p in enumerate(plan):
        a_, c_ = plan[k - 1], plan[(k + 1) % len(plan)]
        cross = (p[0] - a_[0]) * (c_[1] - p[1]) - (p[1] - a_[1]) * (c_[0] - p[0])
        if abs(cross) > 1e-6 * max(1.0, math.hypot(c_[0] - a_[0], c_[1] - a_[1])):
            keep.append(p)
    plan = keep
    n = len(plan)
    for k in range(n):
        a, b = plan[k], plan[(k + 1) % n]
        out = Vector((b[1] - a[1], -(b[0] - a[0]), 0.0))
        if out.length < 1e-9:
            continue
        holes = sorted((px - w2, px + w2, pz - lo, pz + hi) for px, py, pz in ports
                       if abs(a[1] - py) < 1e-6 and abs(b[1] - py) < 1e-6 and min(a[0], b[0]) + w2 < px < max(a[0], b[0]) - w2)
        y = a[1]

        def quad(xa, xb, za, zb):
            if xb - xa > 1e-6 and zb - za > 1e-6:
                m.add([Vector((xa, y, za)), Vector((xb, y, za)), Vector((xb, y, zb)), Vector((xa, y, zb))], [(0, 1, 2, 3)], paint, False, lambda q, o=out: o)
        if not holes:
            m.add([Vector((a[0], a[1], z0)), Vector((b[0], b[1], z0)), Vector((b[0], b[1], z1)), Vector((a[0], a[1], z1))], [(0, 1, 2, 3)], paint, False, lambda q, o=out: o)
            continue
        cur = min(a[0], b[0])
        for h0, h1, hz0, hz1 in holes:
            quad(cur, h0, z0, z1)
            quad(h0, h1, z0, hz0)
            quad(h0, h1, hz1, z1)
            cur = h1
        quad(cur, max(a[0], b[0]), z0, z1)
    hi_ = [Vector((p[0], p[1], z1)) for p in plan]
    tris = tessellate_polygon([hi_])
    if top:
        m.add(hi_, [tuple(t) for t in tris], top, False, lambda q: Z)


def casemate_5in(yard, name, x, y, z, bearing, limits=(-55, 55)):
    """A 5-inch/51 of her broadside battery in its casemate: an embrasure let into the wall (`ported_slab` leaves the
    hole, this lines it: a dark recess with a sill under it and a hood over it), the gun on its pivot 0.6 m inside the
    wall's line, a curved shield closing the port flush with the wall, the barrel out through the shield's middle. A
    `sec.*` turret (its bearing clockwise: -90 to port): the shield and gun train across the port, the gun lifts and
    runs back."""
    m = yard.m
    half = yard.half(x)
    hand = 1.0 if y > 0 else -1.0
    out = Vector((0, hand, 0))
    w2, lo, hi, deep = CASEMATE_PORT
    z0, z1 = z - lo, z + hi
    yb = y - hand * deep
    m.use(half)

    def quad(pts, paint, face):
        m.add([Vector(p) for p in pts], [(0, 1, 2, 3)], paint, False, lambda q, o=Vector(face): o)
    quad([(x - w2, yb, z0), (x + w2, yb, z0), (x + w2, yb, z1), (x - w2, yb, z1)], "recess", out)          # its back,
    quad([(x - w2, y, z0), (x + w2, y, z0), (x + w2, yb, z0), (x - w2, yb, z0)], "recess", Z)               # its floor,
    quad([(x - w2, y, z1), (x + w2, y, z1), (x + w2, yb, z1), (x - w2, yb, z1)], "recess", -Z)              # its head
    for s in (-1, 1):                                                                                     # and its jambs
        quad([(x + s * w2, y, z0), (x + s * w2, yb, z0), (x + s * w2, yb, z1), (x + s * w2, y, z1)], "recess", (-s, 0, 0))
    m.box((x, y + hand * 0.09, z0 - 0.07), (2 * w2 + 0.3, 0.2, 0.14), "upper")                              # the sill
    hood = [Vector((x - w2 - 0.25, y, z1 + 0.34)), Vector((x + w2 + 0.25, y, z1 + 0.34)),
            Vector((x + w2 + 0.25, y + hand * 0.46, z1 + 0.14)), Vector((x - w2 - 0.25, y + hand * 0.46, z1 + 0.14))]
    t_ = Vector((0, 0, -0.06))
    hv = hood + [p + t_ for p in hood]
    hc = sum(hv, Vector()) / 8
    m.add(hv, [(0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)], "upper", False, lambda q, c=hc: q - c)
    if m.fine:
        for s in (-1, 1):                                                                                 # the hood's cheeks
            m.add([Vector((x + s * (w2 + 0.22), y, z1 + 0.02)), Vector((x + s * (w2 + 0.22), y, z1 + 0.3)), Vector((x + s * (w2 + 0.22), y + hand * 0.42, z1 + 0.12))],
                  [(0, 1, 2), (2, 1, 0)], "upper", False, None)
    piv = Vector((x, y - hand * 0.6, z))
    tn = m.part(name, half, tuple(piv), axis=(0, 0, -1), limits=list(limits), kind="turn",
                note=f"a casemate gun: trains across its port (plus clockwise); rest bearing {int(bearing)}")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 10.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, lat = rot @ X, rot @ Y
    ts = (-1.0, -0.75, -0.5, -0.25, 0.0, 0.25, 0.5, 0.75, 1.0) if m.fine else (-1.0, 0.0, 1.0)
    rows = []
    for t in ts:                                            # the shield: a shallow curve, its face on the wall's line
        c = piv + f * (0.6 - 0.16 * t * t) + lat * (t * 1.32)
        rows.append([c - Z * 0.74, c + Z * 0.76])
    m.sheet(rows, "upper", f, smooth=True)
    back = [[p_ - f * 0.06 for p_ in r] for r in rows]
    m.sheet(back, "upper", -f, smooth=True)
    for r, b in ((rows[0], back[0]), (rows[-1], back[-1])):      # its edges
        m.add([r[0], r[1], b[1], b[0]], [(0, 1, 2, 3)], "upper", False, lambda q, c=piv: q - c)
    for j in (0, 1):
        m.add([p_ for r in rows for p_ in (r[j],)] + [p_ for r in back for p_ in (r[j],)],
              [(i, i + 1, len(rows) + i + 1, len(rows) + i) for i in range(len(rows) - 1)], "upper", False, lambda q, s=(1 if j else -1): Z * s)
    gp = piv + f * 0.2 + Z * 0.02
    m.lathe(gp + f * 0.33, f, [(0.0, 0.34), (0.1, 0.34), (0.18, 0.26), (0.24, 0.2)], "upper", 14 if m.fine else 6)   # the collar round the gun
    gn = m.part(f"{name}.gun.1", tn, tuple(gp), axis=tuple(-lat), limits=[-10, 20], slide=tuple(-f), slide_limits=[0.0, 0.5], kind="turn",
                note="the gun: plus lifts the muzzle; runs back when it fires")
    m.use(gn)
    m.lathe(gp - f * 0.9, f, [(0.0, 0.2), (1.2, 0.17), (1.5, 0.14), (5.6, 0.11), (5.8, 0.12), (6.0, 0.115)], "upper", 12 if m.fine else 6, caps=(True, False))
    tip = gp + f * 5.1
    m.disc(tip + f * 0.005, f, 0.064, "black", 8)
    m.socket(f"muzzle.{name}.1", tip, f, gn, "flash and smoke leave here")
    m.use(half)
    return tn, [gn]


def az_5in25(yard, name, x, y, z, bearing, limits, tub=0.0, parent="aft"):
    """One of her eight 5-inch/25: the short gun on its pedestal, open, its crew close round it (layer and trainer at
    the handwheels, a loader with a round over the breech, a man bringing up the next); `tub` a radius: the low
    splinter shield round it (the four end guns, whose tubs still lie on the wreck). A `sec.*` turret, on `parent`
    (her after half: her superstructure goes with it when her back breaks)."""
    m = yard.m
    tn, guns = yard.turret(name, x, z, 3.4, 1, 3.6, bearing, 0.07, y=y, style="open", rate=15.0, elev=(-15, 85), rest_elev=12.0, limits=limits, floats=False,
                           parent=parent)
    if tub:
        m.use(parent).tub((x, y, z), tub, 0.95, "upper", sides=28 if m.fine else 12)
    if m.fine:
        p = m.parts[m.part_at[tn]]
        m.use(tn)
        rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
        o = Vector(p.pivot)

        def W(px, py, pz):
            return o + rot @ Vector((px, py, pz))
        f = rot @ X
        for side in (1, -1):        # (the handwheels are 0.85 out from the gun, 0.5 behind its trunnions)
            fit.sailor(m, W(-0.9, side * 1.15, -0.45), f, "aim", hands=(W(-0.5, side * 0.9, 0.95), W(-0.62, side * 0.9, 0.85)), navy="USA", lean=0.06)
        fit.sailor(m, W(-1.75, 0.45, -0.45), f, "load", navy="USA")
        fit.sailor(m, W(-1.7, -0.5, -0.45), f, "carry", navy="USA")
    m.use(parent)
    return tn, guns


def az_walls_detail(yard, plan, z0, z1, doors=True, scuttles=True, rail=True, skip=(), part="aft", rail_cut=(), no_rail=None):
    """`walls_detail` for her superstructure: everything on `part` (her houses go with her after half when her back
    breaks, wherever they stand); `rail_cut` [(x, y, r)]: circles the rail round the top stops at (a tub's own rim is
    the guard there); `no_rail(a, b)`: true for a wall whose top has no rail at all."""
    m = yard.m
    if not m.fine:
        return
    from ships import railing
    m.use(part)
    n = len(plan)
    for k in range(n):
        a, b = Vector((*plan[k], 0.0)), Vector((*plan[(k + 1) % n], 0.0))
        d = b - a
        L = d.length
        if L < 1.0:
            continue
        out = Vector((d.y, -d.x, 0.0)).normalized()
        along = d.normalized()
        if rail and not (no_rail and no_rail(a, b)):
            ra, rb = a - out * 0.12, b - out * 0.12
            # (the stretches of the rail outside every cut circle, as shares of the wall's length)
            cuts = []
            for cx, cy, r in rail_cut:
                c = Vector((cx, cy, 0.0))
                t0 = (c - ra).dot(along)
                off = ((ra + along * t0) - c).length
                if off < r:
                    hw = math.sqrt(r * r - off * off)
                    cuts.append(((t0 - hw) / L, (t0 + hw) / L))
            runs, t = [], 0.0
            for c0, c1 in sorted(cuts):
                if c0 > t:
                    runs.append((t, min(c0, 1.0)))
                t = max(t, c1)
            if t < 1.0:
                runs.append((t, 1.0))
            for t0, t1 in runs:
                if (t1 - t0) * L > 0.4:
                    railing(m, [ra.lerp(rb, t0) + Z * z1, ra.lerp(rb, t1) + Z * z1], high=1.0, every=1.9)
        if z1 - z0 < 2.0 or L < 3.0:
            continue
        steps = int((L - 1.2) / 2.3)
        for i in range(steps):
            p = a + along * (0.6 + (i + 0.5) * (L - 1.2) / steps)
            if any(lo_ <= p.x <= hi_ for lo_, hi_ in skip):
                continue
            if doors and i % 5 == 2 and z1 - z0 >= 2.3:
                m.add([p - along * 0.38 + Z * (z0 + 0.15) + out * 0.03, p + along * 0.38 + Z * (z0 + 0.15) + out * 0.03,
                       p + along * 0.38 + Z * (z0 + 1.95) + out * 0.03, p - along * 0.38 + Z * (z0 + 1.95) + out * 0.03], [(0, 1, 2, 3)], "door", False, lambda q, o=out: o)
                m.box(p + along * 0.24 + Z * (z0 + 1.05) + out * 0.06, (0.08, 0.08, 0.08), "steel")
            elif scuttles:
                m.disc(p + Z * (z0 + min(z1 - z0 - 0.6, 1.7)) + out * 0.015, out, 0.17, "glass", 8)


def az_rangefinder(yard, name, x, y, z, arm, bearing=0.0, house=1.4, parent=None):
    """`fit.rangefinder` with its hoods set 3 cm back from the tube's ends (so the end caps and the hoods' faces are
    not one plane)."""
    m = yard.m
    dn = m.part(name, parent or yard.half(x), (x, y, z), axis=(0, 0, -1), kind="turn", note=f"a rangefinder: turns with the guns; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
    lat = rot @ Y
    m.lathe((x, y, z), Z, [(0.0, house), (house * 0.9, house * 0.95), (house * 1.05, house * 0.7)], "upper", 16 if m.fine else 8)
    c = Vector((x, y, z + house * 0.62))
    m.lathe(c - lat * arm, lat, [(0.0, 0.3), (0.35, 0.36), (arm * 2 - 0.35, 0.36), (arm * 2, 0.3)], "upper", 12 if m.fine else 5)
    for side in (1, -1):
        m.box(c + lat * side * (arm - 0.33) + rot @ Vector((0.25, 0, 0.0)), (0.8, 0.6, 0.85), "upper", yaw=bearing)
        m.disc(c + lat * side * (arm - 0.33) + rot @ Vector((0.66, 0, 0.0)), rot @ X, 0.16, "glass", 8)
    return dn


def az_crane(yard, x, y, z, head, reach, bearing=0.0, house_z=15.4):
    """One of her two boat cranes: a tall round post on its pedestal, a railed platform round it, the machinery house
    high on the post with its platform, the jib heeled on the post under the house and laid out at a low angle over
    the boats, its topping lift from the post's head and the fall from its tip."""
    m = yard.m
    from ships import railing
    m.use("aft")
    fine = m.fine
    m.lathe((x, y, z), Z, [(0.0, 0.62), (0.9, 0.62), (1.05, 0.5), (head - z, 0.36)], "upper", 16 if fine else 7, caps=(False, True))
    m.lathe((x, y, z + 1.6), Z, [(0.0, 1.35), (0.14, 1.35)], "upper", 20 if fine else 8)        # the platform round its foot
    if fine:
        railing(m, [Vector((x + math.cos(2 * math.pi * k / 10) * 1.28, y + math.sin(2 * math.pi * k / 10) * 1.28, z + 1.74)) for k in range(11)], high=0.9, every=1.0)
    a = math.radians(bearing)
    f = Vector((math.cos(a), math.sin(a), 0.0))
    s = Z.cross(f)
    yaw = bearing
    hz = house_z
    m.lathe((x, y, hz - 0.14), Z, [(0.0, 1.5), (0.14, 1.5)], "upper", 20 if fine else 8)          # the machinery platform
    m.box(Vector((x, y, hz + 1.2)) - f * 0.5, (2.4, 2.0, 2.4), "upper", yaw=yaw)                     # the machinery house
    m.box(Vector((x, y, hz + 2.44)) - f * 0.5, (2.6, 2.2, 0.08), "roof", yaw=yaw)
    if fine:
        railing(m, [Vector((x + math.cos(2 * math.pi * k / 12) * 1.42, y + math.sin(2 * math.pi * k / 12) * 1.42, hz)) for k in range(13)], high=0.9, every=1.0)
        for side in (1, -1):
            m.disc(Vector((x, y, hz + 1.5)) - f * 0.5 + s * side * 1.005, s * side, 0.22, "glass", 8)
    heel = Vector((x, y, hz - 0.5)) + f * 0.45
    tip = Vector((x, y, 0.0)) + f * reach
    tip.z = head - 1.4
    for side in (1, -1):                                                       # the jib: two booms meeting at its tip
        m.rod(heel + s * side * 0.45, tip + s * side * 0.12, 0.17, "upper", rb=0.1, sides=6 if fine else 4)
    m.box(tip, (0.5, 0.5, 0.4), "upper", yaw=yaw)
    top = Vector((x, y, head))
    m.box(top + Z * 0.15, (0.6, 0.6, 0.3), "upper", yaw=yaw)
    if fine:
        for side in (1, -1):
            yard.wire(top + Z * 0.15 + s * side * 0.2, tip + s * side * 0.1 + Z * 0.15, 0.035)
        for k in range(1, 6):                                                   # the booms' lacing
            p = heel.lerp(tip, k / 6.0)
            m.rod(p + s * lerp(0.45, 0.12, k / 6.0), p - s * lerp(0.45, 0.12, k / 6.0), 0.04, "upper", sides=4)
    hook = tip - Z * 5.0
    yard.wire(tip - Z * 0.2, hook, 0.025)
    m.use("aft")
    m.box(hook - Z * 0.2, (0.3, 0.3, 0.4), "steel")
    return tip


def az_break(yard, run=2.2):
    """The break of her forecastle, where the yard's hull steps down a deck to her quarterdeck: a steel bulkhead
    closes the step across her (the yard lays its deck, planks and all, down the step), and each side comes down to
    the quarterdeck in a curved fairing `run` metres long, steep at its top and easing out at its foot."""
    m, h = yard.m, yard.h
    fine = m.fine
    xb = h.step_at
    du = 0.0012 * h.Lwl * 0.5                       # (the yard's two frames either side of the step, as metres)
    lo, hi = h.deck_z(xb - 1.0), h.deck_z(xb + 1.0)
    m.use("aft")
    # the bulkhead, 3 cm abaft the step's foot: its top follows the forecastle deck's camber
    xp = xb - du - 0.03
    e = h.half_at(xb + du + 0.05)
    half = max(e - 0.35, e * 0.5)
    across = 7
    top = [Vector((xp, e, hi + 0.02))] + [Vector((xp, half * (1.0 - 2.0 * j / (across - 1)), hi + 0.02 + 0.22 * (1.0 - (1.0 - 2.0 * j / (across - 1)) ** 2) * min(1.0, half / 4.0)))
                                    for j in range(across)] + [Vector((xp, -e, hi + 0.02))]
    pts = top + [Vector((xp, -e, lo - 0.06)), Vector((xp, e, lo - 0.06))]
    tris = tessellate_polygon([[Vector((p.y, p.z, 0.0)) for p in pts]])
    m.add(pts, [tuple(t) for t in tris], "upper", False, lambda q: -X)
    # the fairings: a plate up each side over the quarterdeck's edge, in the forecastle's own side
    xs_, xf_ = xb + du + 0.05, xb - du - run
    n = 12 if fine else 4
    for hand in (PORT, STBD):
        yq = h.half_at(xb - 1.0)

        def y_at(z, g):
            yf = h.side(xb + 0.6, z, hand)[0].y * hand
            yl = h.side(xb + 0.6, lo, hand)[0].y * hand
            return yf + 0.008 - (yl - yq) * (1.0 - (z - lo) / (hi - lo)) * g
        outer, inner = [], []
        for k in range(n + 1):
            s = 1.0 - math.cos(k / n * math.pi * 0.5)                    # (closer together at its steep top)
            x = lerp(xs_, xf_, s)
            zt = hi - (hi - lo) * math.sqrt(max(0.0, 1.0 - (1.0 - s) ** 2))
            zt = max(zt, lo + 0.06)
            g = min(1.0, s * 1.6)
            col_o, col_i = [], []
            for j in range(4):
                z = lerp(lo - 0.04, zt, j / 3.0)
                y = y_at(min(max(z, lo), hi), g)
                col_o.append(Vector((x, hand * y, z)))
                col_i.append(Vector((x, hand * (y - 0.04), z)))
            outer.append(col_o)
            inner.append(col_i)
        out_n, in_n = Vector((0, hand, 0)), Vector((0, -hand, 0))
        m.sheet(outer, "hull", out_n, smooth=False)
        m.sheet(inner, "hull", in_n, smooth=False)
        m.sheet([[a[-1], b[-1]] for a, b in zip(outer, inner)], "hull", Z, smooth=False)      # its top edge
        m.sheet([[outer[0][j], inner[0][j]] for j in range(4)], "hull", X, smooth=False)        # (its fore edge, against the step)
    return xp


def az_drum_director(yard, name, x, z, r, high, arm, bearing=180.0, parent="aft"):
    """The main battery's after director in the top of her mainmast: a round windowed house that turns, the
    rangefinder through it, its hooded ends standing out each side. A turn part (its pole runs up its middle)."""
    m = yard.m
    fine = m.fine
    dn = m.part(name, parent, (x, 0.0, z), axis=(0, 0, -1), kind="turn", note=f"the after main battery director: turns with the guns; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
    lat, fwd = rot @ Y, rot @ X
    n = 24 if fine else 10
    m.lathe((x, 0.0, z), Z, [(0.0, r), (high, r), (high + 0.06, r * 0.96)], "light", n)
    if fine:
        for k in range(n):                                  # the window band round it, but where the rangefinder runs through
            a0, a1 = 2 * math.pi * (k + 0.12) / n, 2 * math.pi * (k + 0.88) / n
            am = (a0 + a1) * 0.5
            o_ = Vector((math.cos(am), math.sin(am), 0.0))
            if abs((rot.transposed() @ o_).y) > 0.86:
                continue
            p0 = Vector((x + math.cos(a0) * (r + 0.012), math.sin(a0) * (r + 0.012), 0.0))
            p1 = Vector((x + math.cos(a1) * (r + 0.012), math.sin(a1) * (r + 0.012), 0.0))
            m.add([p0 + Z * (z + high - 1.0), p1 + Z * (z + high - 1.0), p1 + Z * (z + high - 0.45), p0 + Z * (z + high - 0.45)], [(0, 1, 2, 3)], "glass", False,
                  lambda q, o=o_: o)
    c = Vector((x, 0.0, z + high * 0.48))
    m.lathe(c - lat * arm, lat, [(0.0, 0.26), (0.3, 0.32), (arm * 2 - 0.3, 0.32), (arm * 2, 0.26)], "light", 12 if fine else 5)
    for side in (1, -1):
        m.box(c + lat * side * (arm - 0.36) + fwd * 0.15, (0.8, 0.6, 0.8), "light", yaw=bearing)
        if fine:
            m.disc(c + lat * side * (arm - 0.36) + fwd * 0.555, fwd, 0.15, "glass", 8)
    m.use(parent)
    return dn


def az_raft(yard, x, y, z, facing, n=2, long=2.8, wide=1.2):
    """Life rafts on edge against a wall, as `fit.float_rack`, rounder (her rafts are seen close)."""
    if not yard.m.fine:
        return
    m = yard.m
    f = Vector(facing).normalized()
    side = f.cross(Z)
    for i in range(n):
        mid = Vector((x, y, z + i * 0.12)) + f * (i * 0.24)
        k_ = 20
        ring = [mid + side * (math.cos(2 * math.pi * k / k_) * long / 2) + Z * (math.sin(2 * math.pi * k / k_) * wide / 2) for k in range(k_)]
        verts, faces = [], []
        for sc, off in ((1.0, 0.0), (1.0, 0.22), (0.66, 0.22), (0.66, 0.0)):
            verts += [mid + (p - mid) * sc + f * off for p in ring]
        for layer in range(3):
            faces += [(layer * k_ + k, layer * k_ + (k + 1) % k_, (layer + 1) * k_ + (k + 1) % k_, (layer + 1) * k_ + k) for k in range(k_)]
        m.add(verts, faces, "cork", True, lambda q, c=mid: (q - c) + f * 0.3)
        for t in (-0.3, 0.0, 0.3):                                                          # its grating
            m.box(mid + side * (t * long * 0.5) + f * 0.12, (0.05, 0.05, wide * 0.62), "steel", yaw=math.degrees(math.atan2(side.y, side.x)))


def az_windows(m, pl, z_lo, z_hi, pane=1.1, min_len=1.2, front_of=None):
    """Window panes right round a plan (or, with `front_of`, only on the walls forward of that x)."""
    for k in range(len(pl)):
        a, b = Vector((*pl[k], 0)), Vector((*pl[(k + 1) % len(pl)], 0))
        d = b - a
        if d.length < min_len or (front_of is not None and min(a.x, b.x) < front_of):
            continue
        o_ = Vector((d.y, -d.x, 0)).normalized()
        panes = max(1, int(d.length / pane))
        for i in range(panes):
            c = a + d * ((i + 0.5) / panes) + o_ * 0.03
            u_ = d.normalized() * (d.length / panes * 0.5 - 0.1)
            m.add([c - u_ + Z * z_lo, c + u_ + Z * z_lo, c + u_ + Z * z_hi, c - u_ + Z * z_hi], [(0, 1, 2, 3)], "glass", False, lambda q, o=o_: o)


def az_tiers(yard, tiers, z, paint="light"):
    """A spotting top of stacked houses, each [x fore, x aft, half width, height, windows (False, True or "slits")],
    from height z up; returns the height of its roof."""
    m = yard.m
    from ships import railing
    for xf, xa, hw, hh, win in tiers:
        pl = Model.round_rect(xf, xa, -hw, hw, 1.0, 3 if m.fine else 1)
        slab(m, pl, z, z + hh, paint=paint, top="roof")
        if m.fine and win:
            if win == "slits":
                az_windows(m, pl, z + hh - 0.75, z + hh - 0.45, pane=0.8)
            else:
                az_windows(m, pl, z + hh - 1.35, z + hh - 0.5)
            m.strip([Vector((px_, py_, z + hh - 0.2)) for px_, py_ in pl] + [Vector((*pl[0], z + hh - 0.2))],
                    [Vector((px_ - (xf + xa) * 0.5, py_, 0)).normalized() for px_, py_ in pl + [pl[0]]], 0.05, paint, 0.03)   # a drip strip
        z += hh
    if m.fine:
        pl = Model.round_rect(*tiers[-1][:2], -tiers[-1][2], tiers[-1][2], 1.0, 3)
        railing(m, [Vector((px_, py_, z)) for px_, py_ in pl] + [Vector((*pl[0], z))], every=1.5, paint=paint)
    return z


def blister_ledge(yard, x0, x1, z, proud=0.3, drop=0.65, step=1.5):
    """The top of her torpedo bulges: a ledge along each side at the waterline, its face sloping back into the hull
    under it, tapering out at its ends; inspection plates bolted over the hull above it."""
    m, h = yard.m, yard.h
    for hand in (PORT, STBD):
        xj = yard.break_x + yard.jag_at(min(max(z / max(h.deck_z(yard.break_x), 0.1), 0.0), 1.0), hand)
        xs = []
        x = x0
        while x < x1:
            xs.append(x)
            x += step
        xs = sorted(set([round(v, 4) for v in xs + [x1, xj - 0.01, xj + 0.01]]))
        rows = []
        for x in xs:
            t = min(1.0, (x - x0) / 8.0, (x1 - x) / 8.0)
            t = t * t * (3 - 2 * t)
            p, n = h.side(x, z, hand)
            q, _ = h.side(x, z - drop, hand)
            nh = Vector((n.x, n.y, 0.0)).normalized()
            rows.append((x, p - nh * 0.06, p + nh * (0.06 + proud * t), q - nh * 0.06, nh))
        for (xa, ia, oa, la, na), (xb, ib, ob, lb, nb) in zip(rows, rows[1:]):
            m.use("fore" if (xa + xb) * 0.5 >= xj else "aft")
            m.add([ia, oa, ob, ib], [(0, 1, 2, 3)], "hull", False, lambda q_: Z)
            sl = (na - Z * 0.6)
            m.add([oa, la, lb, ob], [(0, 1, 2, 3)], "hull", False, lambda q_, s=sl: s)
        if m.fine:
            x = x0 + 9.0
            while x < x1 - 8.0:
                if abs(x - xj) > 1.0:
                    p, n = h.side(x, z + 0.75, hand)
                    m.use("fore" if x >= xj else "aft")
                    yaw = math.degrees(math.atan2(n.y, n.x))
                    m.box(p + n * 0.02, (0.04, 0.8, 0.6), "hull", yaw=yaw)
                    for dx in (-0.32, 0.32):
                        for dz in (-0.22, 0.22):
                            m.box(p + n * 0.05 + Vector((-n.y, n.x, 0)) * dx + Z * dz, (0.03, 0.05, 0.05), "steel", yaw=yaw)
                x += 7.5


# her hull's deck edge, after the class's plan (half widths as shares of 16.2 m): full and long, a round stern under a
# short counter, a fine straight-sided bow
ARIZONA_PLAN = [(-92.7, 0.0), (-92.4, 0.25), (-91.5, 0.4), (-89.0, 0.53), (-84.0, 0.66), (-76.0, 0.78), (-66.0, 0.88), (-56.0, 0.95),
                (-44.0, 0.99), (-34.0, 1.0), (22.0, 1.0), (32.0, 0.98), (42.0, 0.93), (52.0, 0.85), (62.0, 0.73), (70.0, 0.61), (78.0, 0.46),
                (84.0, 0.33), (88.0, 0.22), (91.0, 0.11), (92.7, 0.0)]
# Measure 1 of 1941: dark grey 5-D on every side up to the funnel's top (22.5 m), light grey 5-L above it and on her
# tops; her boats, their covers and her guns' bloomers grey as the ship; her wooden decks weathered teak (no stain:
# she was lost on 7 December 1941, before the Pacific Fleet's decks were stained blue)
ARIZONA_PAINT = {"hull": "4E565E", "upper": "4E565E", "light": "879098", "deck": "5A6066", "planks": "9C8B6C", "roof": "5A6066",
                 "door": "545C64", "seam": "474E55", "plane": "6E7A86", "bag": "6A7076", "boat": "6C747C", "canvas": "7E8178",
                 "cork": "727A80", "recess": "24282C"}


def arizona(fine=True):
    """USS Arizona, of the Pennsylvania class, as she lay at Pearl Harbor in December 1941: 185.4 m by 32.4 over her
    bulges, twelve 14-inch guns in four triple turrets, two tripod masts each under a three-tiered spotting top, one
    funnel, ten 5-inch/51 in casemates (the two that stood on her superstructure were landed shortly before the
    attack), eight 5-inch/25 (six on her boat deck, the after pair on the houses abaft the break of her forecastle),
    eight .50 calibre machine guns (four in the box at her mainmast's head, two on her foretop, two on the funnel's
    platforms); a catapult on her third turret and one on her fantail, each with its Kingfisher. Laid out against
    the measured plan and profiles of her wreck (HAER HI-13, sheets 3 and 4: her barbettes, the break of her
    forecastle, the stump of her mainmast's tripod, her crane posts, the 5-inch tubs, the fantail catapult's base)
    and her 1941 photographs. Stands in for `Battleship_USA` in 1941-42."""
    paints = livery("USA", "battleship")
    paints.update(ARIZONA_PAINT)
    paints = fit.crew_paints(paints, "USA")
    m = Model("Arizona", "Battleship, Pennsylvania class, 1941 (our own model)", "ship", paints, fine)
    # the forecastle deck runs from her stem to just forward of her mainmast, where her side comes down a deck to the
    # quarterdeck in a curved fairing (HAER sheet 4's profiles; her 1941 photographs); her bulges swell her sides
    # below it (106 ft 3 in over them, 97 ft at the deck)
    h = PlanHull(ARIZONA_PLAN, L=185.4, B=32.4, D=4.6, T=8.8, sheer=2.3, sheer_aft=0.3, rake=2.5, counter=3.5, flare=0.3, fine=1.6, full=0.72,
                 wl_fore=0.62, bilge=5.0, step_at=-19.8, step=2.5, tumble=0.915)
    yard = Yard(m, h, break_x=21.5)              # (where her forward magazines blew: just abaft her second turret)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("planks")
    D = h.deck_z
    z0 = D(0.0)
    z01, z02, z03, z04, z05 = z0 + 2.6, z0 + 5.2, z0 + 7.8, z0 + 10.4, z0 + 13.0
    zM1 = 22.5                                   # (Measure 1: dark 5-D on every upright surface up to the funnel's top)
    from ships import railing
    blister_ledge(yard, -70.0, 70.0, 1.3, proud=0.35, drop=0.7)
    xp = az_break(yard)                          # the step's bulkhead, abaft the forecastle deck's last frame
    zq = D(-21.0)                                # (the quarterdeck there)
    zA = D(-19.0) + 0.25                         # (the houses abaft the break: their roofs level with the forecastle deck)

    # ---- the superstructure deck (her casemate battery in its sides) and the boat deck over it. Abaft the boat
    # deck the superstructure deck's sides run on to the break; between them, a well on the forecastle deck where the
    # mainmast's legs come down
    cas_x = (15.5, 10.0, -7.2, -12.6, -18.0)
    zc = z0 + 1.3
    xe = xp + 0.02
    p01 = sym([(27.0, 0.0), (26.4, 6.5), (24.6, 12.9), (xe, 12.9), (xe, 6.6), (-17.6, 6.6), (-17.6, 0.0)])
    p02 = sym([(22.0, 0.0), (22.5, 4.6), (24.0, 5.6), (24.0, 10.6), (-17.4, 10.6), (-17.4, 0.0)])
    m.use("aft")
    ported_slab(m, p01, z0 - 0.4, z01, [(cx, hand * 12.9, zc) for cx in cas_x for hand in (PORT, STBD)])
    az_walls_detail(yard, p01, z0, z01, scuttles=False, skip=[(cx - 2.2, cx + 2.2) for cx in cas_x])
    slab(m, p02, z01, z02)
    az_walls_detail(yard, p02, z01, z02, scuttles=True, skip=[(-4.8, 2.8)])
    if fine:   # the rail across the well's open after edge, where the deck steps down
        railing(m, [(-19.66, -6.5, D(-19.5) + 0.17), (-19.66, -5.75, D(-19.5) + 0.17)], every=1.0)
        railing(m, [(-19.66, -4.0, D(-19.5) + 0.22), (-19.66, 4.0, D(-19.5) + 0.22)], every=1.6)
        railing(m, [(-19.66, 5.75, D(-19.5) + 0.17), (-19.66, 6.5, D(-19.5) + 0.17)], every=1.0)
    # the houses each side abaft the break, on the quarterdeck: the boat cranes' pedestals and the after 5-inch/25s
    # stand on them
    for hand in (PORT, STBD):
        ah = Model.round_rect(xp + 0.04, -30.0, hand * 6.2, hand * 12.9, 0.8, 3 if fine else 1)
        m.prism(ah, zq - 0.1, zA, "upper", top_paint="deck")
        az_walls_detail(yard, ah, zq, zA, doors=True, scuttles=True, rail_cut=[(-20.6, hand * 7.4, 1.45), (-30.0, hand * 9.0, 0.6)],
                        no_rail=lambda a, b: min(a.x, b.x) > xp - 0.5)
        if fine:     # a ladder up its after end from the quarterdeck
            fit.stair(m, (-31.6, hand * 9.0, zq + 0.16), (-29.95, hand * 9.0, zA), wide=0.7)

    # ---- the four triple turrets, each held off the superstructure on its after bearings
    mains = []
    # (training limits found by sweeping each turret's guns through their whole lift: beyond them they foul the
    # turret ahead or astern, the superstructure or the 5-inch/25s; the superfiring pair cannot depress their guns
    # into the roofs of the turrets they fire over, nor the fourth turret's into the fantail catapult)
    for name, tx, raised, bearing, lim, dep in (("turret.A", 44.4, 0.0, 0.0, 135, -5), ("turret.B", 29.4, 2.6, 0.0, 90, 0),
                                                ("turret.X", -41.0, 3.0, 180.0, 145, 0), ("turret.Y", -55.0, 0.3, 180.0, 130, -2)):
        tn, gs, W, H = us_turret14(yard, name, tx, D(tx), bearing, raised=raised, limits=(-lim, lim), depress=dep)
        mains.append((tn, gs, tx, raised > 1.0))
        yard.fire_points.append((tx, 0.0, D(tx) + raised + 2.5))
    # the third turret's catapult and its Kingfisher ride its roof
    px = m.parts[m.part_at["turret.X"]]
    catapult_us(yard, -41.0, 0.0, px.pivot.z + 3.0 - 0.4, 14.0, 180.0, parent="turret.X")

    # ---- the conning tower; over it the navigating bridge, the flag bridge and the chart house, stepped back, with
    # open wings; the two AA directors in their tubs either side
    m.use("aft")
    ct = [(19.8 + math.cos(2 * math.pi * k / 20) * 3.0, math.sin(2 * math.pi * k / 20) * 2.6) for k in range(20)]
    slab(m, ct, z01, z03 + 0.6, top="roof")
    if fine:
        ring = [Vector((px_, py_, z03 + 0.1)) for px_, py_ in ct]
        for k in range(20):                                          # the vision slits round its top
            a, b = ring[k], ring[(k + 1) % 20]
            o_ = Vector(((a.x + b.x) * 0.5 - 19.8, (a.y + b.y) * 0.5, 0)).normalized()
            if o_.x > -0.3:
                m.add([a + o_ * 0.02, b + o_ * 0.02, b + o_ * 0.02 + Z * 0.2, a + o_ * 0.02 + Z * 0.2], [(0, 1, 2, 3)], "black", False, lambda q, o=o_: o)
    house = sym([(17.5, 0.0), (17.0, 4.4), (15.5, 5.8), (12.5, 5.8), (12.5, 0.0)])
    slab(m, house, z02, z03)
    az_walls_detail(yard, house, z02, z03)
    nav = sym([(24.8, 0.0), (24.0, 4.2), (21.5, 6.8), (14.0, 6.8), (14.0, 0.0)])
    slab(m, nav, z03, z04)
    if fine:   # the navigating bridge's windows round its front and sides
        az_windows(m, nav, z03 + 1.15, z03 + 2.05, pane=1.0, min_len=0.5, front_of=16.5)
    az_walls_detail(yard, nav, z03, z04, doors=False, scuttles=False)
    for hand in (PORT, STBD):          # its wings
        m.box((18.5, hand * 8.9, z04 - 0.08), (3.6, 4.2, 0.16), "deck")
        if fine:
            railing(m, [(20.3, hand * 6.8, z04), (20.3, hand * 11.0, z04), (16.7, hand * 11.0, z04), (16.7, hand * 6.8, z04)], every=1.3)
        for dx in (-1.4, 1.4):
            m.rod((18.5 + dx, hand * 6.9, z03), (18.5 + dx, hand * 10.6, z04 - 0.15), 0.07, "upper", sides=4)
    flag = sym([(23.4, 0.0), (22.8, 3.4), (20.8, 5.6), (14.2, 5.6), (14.2, 0.0)])
    slab(m, flag, z04, z05)
    if fine:
        az_windows(m, flag, z04 + 1.0, z04 + 1.9, pane=1.0, min_len=0.5, front_of=17.0)
    # (its rail stops at the directors' tubs, whose rims guard their own edges)
    az_walls_detail(yard, flag, z04, z05, doors=True, scuttles=False, rail_cut=[(15.6, 6.7, 1.62), (15.6, -6.7, 1.62)])
    for hand in (PORT, STBD):          # the flag bridge's open wings, and the AA directors' tubs abaft them
        m.box((19.6, hand * 7.3, z05 - 0.08), (2.8, 3.4, 0.16), "deck")
        if fine:
            railing(m, [(21.0, hand * 5.6, z05), (21.0, hand * 9.0, z05), (18.2, hand * 9.0, z05), (18.2, hand * 5.6, z05)], every=1.2)
        for dx in (-1.0, 1.0):
            m.rod((19.6 + dx, hand * 5.65, z04 + 0.2), (19.6 + dx, hand * 8.7, z05 - 0.15), 0.06, "upper", sides=4)
        dy = hand * 6.7
        m.lathe((15.6, dy, z05 - 0.24), Z, [(0.0, 1.55), (0.2, 1.55)], "upper", 24 if fine else 8)
        if fine:
            for dx in (-0.8, 0.0, 0.8):
                m.rod((15.6 + dx, hand * 5.62, z05 - 1.7), (15.6 + dx, dy + hand * 0.9, z05 - 0.25), 0.06, "upper", sides=4)
        m.tub((15.6, dy, z05 - 0.04), 1.55, 0.85, "upper", sides=24 if fine else 8, floor="deck")
        m.lathe((15.6, dy, z05), Z, [(0.0, 0.32), (0.6, 0.26)], "upper", 10 if fine else 6)
        az_rangefinder(yard, "director.ha." + ("1" if hand == PORT else "2"), 15.6, dy, z05 + 0.6, 0.9, bearing=0.0, house=0.62, parent="aft")
        m.use("aft")
    chart = Model.round_rect(20.6, 14.8, -4.0, 4.0, 1.0, 3 if fine else 1)
    slab(m, chart, z05, z05 + 2.4)
    az_walls_detail(yard, chart, z05, z05 + 2.4, doors=True, scuttles=True)
    if fine:
        yard.windows(20.6, 3.4, z05 + 1.0, 0.8, back=2.0)
        for k, (ox, oy, zz, face) in enumerate(((19.4, 9.6, z04, 0.4), (17.6, -10.2, z04, -0.6), (20.4, 8.0, z05, 0.2), (19.0, -7.6, z05, -0.3),
                                                (16.4, 0.8, z05 + 2.4, 0.0), (19.6, -2.0, z05 + 2.4, 1.0))):
            fit.sailor(m.use("aft"), Vector((ox, oy, zz)), Vector((math.cos(face), math.sin(face), 0)), "stand", navy="USA")
    m.use("aft")

    # ---- the foremast: a tripod, its searchlight platform, the gallery with two of her .50s, the three-tiered spotting
    # top, the main battery's director on its roof, a short topmast
    fx = 13.4
    r_f = 0.62 - 0.12 * (zM1 - z02) / 18.0
    m.lathe((fx, 0, z02), Z, [(0.0, 0.62), (zM1 - z02, r_f)], "upper", 18 if fine else 5, caps=(True, False))
    for hand in (PORT, STBD):
        a, b = Vector((fx - 5.0, hand * 4.6, z02)), Vector((fx, 0.0, 31.2))
        c = a.lerp(b, (zM1 - a.z) / (b.z - a.z))
        m.rod(a, c, 0.42, "upper", sides=14 if fine else 5, caps=(True, False))
        m.rod(c, b, 0.42, "light", sides=14 if fine else 5, caps=(False, True))
    zs = 26.6
    m.box((fx - 0.4, 0, zs - 0.08), (3.6, 8.6, 0.16), "light")
    if fine:
        railing(m, [(fx + 1.4, 4.3, zs), (fx + 1.4, -4.3, zs)], every=1.4, paint="light")
    for hand in (PORT, STBD):
        yard.searchlight(fx - 0.4, hand * 3.3, zs, 0.6, (1.0, hand * 0.4, 0))
    m.use("aft")
    zm = 30.0
    plat = Model.round_rect(19.6, 7.6, -6.2, 6.2, 1.4, 3 if fine else 1)
    m.prism(plat, zm - 0.18, zm, "light", top_paint="deck", bottom=True)
    if fine:
        railing(m, [Vector((px_, py_, zm)) for px_, py_ in plat] + [Vector((*plat[0], zm))], every=1.6, paint="light")
    # (.50s: two on each mast and four on the funnel, says the memorial's own record (ussarizona.org); the 1940-41
    # refit put four on a platform at the very top of the mainmast (her refit record, and her action report's ".50
    # caliber machine guns in maintop"): so four at the main's head, two here, two on the funnel's platforms)
    for gx_, gy_, face in ((18.9, 5.4, 35.0), (18.9, -5.4, -35.0)):
        fit.mg50(yard, gx_, gy_, zm, facing=face, part="aft")
    tz = az_tiers(yard, ((17.2, 9.6, 4.3, 2.4, True), (16.6, 10.2, 3.6, 2.3, True), (15.9, 10.9, 2.7, 2.0, "slits")), zm)
    m.lathe((fx, 0, zM1), Z, [(0.0, r_f), (tz + 0.4 - zM1, 0.36)], "light", 18 if fine else 5, caps=(False, True))
    az_rangefinder(yard, "director.fore", fx + 0.6, 0.0, tz, 2.8, bearing=0.0, house=1.1, parent="aft")
    m.use("aft")
    # (her topmast stood only some 6 m over the top in January 1941; the photographs show no radar's foundation)
    m.lathe((fx, 0, tz + 0.4), Z, [(0.0, 0.2), (6.1, 0.08)], "light", 10 if fine else 4)
    m.rod((fx, -3.2, tz + 4.4), (fx, 3.2, tz + 4.4), 0.07, "light", sides=4)
    head_fore = Vector((fx, 0, tz + 6.4))

    # ---- the funnel (its top under the foremast's searchlights, as the 1941 photographs show it) and its searchlight
    # platforms, with a .50 abaft each light
    fnx, zf, zfp = 2.3, zM1, 17.0
    us_funnel(yard, fnx, z02, zf, 8.2, 5.6, slope=0.4, platform=zfp)
    m.use("aft")
    slab(m, Model.round_rect(fnx + 5.2, fnx - 5.2, -4.1, 4.1, 1.4, 3 if fine else 1), z02, z03)
    for hand in (PORT, STBD):
        yy = hand * 3.7
        m.box((fnx - 2.0, hand * 3.45, zfp), (1.8, 2.5, 0.12), "upper")
        for dx in (-2.5, -1.4):
            m.rod((fnx + dx, hand * 2.0, zfp - 1.1), (fnx + dx, hand * 4.5, zfp - 0.06), 0.06, "upper", sides=4)
        if fine:
            railing(m, [(fnx - 1.1, yy + hand * 0.95, zfp + 0.06), (fnx - 2.9, yy + hand * 0.95, zfp + 0.06), (fnx - 2.9, hand * 2.3, zfp + 0.06)], every=1.0)
        fit.mg50(yard, fnx - 2.2, hand * 3.9, zfp + 0.06, facing=hand * 140.0, part="aft")

    # ---- the boats: two 10 m and the 12 m between the funnel and the mainmast, the two 7 m beside the funnel; the
    # two boat cranes on their pedestals just abaft the break (their posts' stumps on the wreck beside the tripod's
    # legs), their jibs laid forward over the boats
    m.use("aft")
    for bx_, by_, long_, bz in ((-11.6, 4.8, 10.0, 0.55), (2.6, 6.7, 7.0, 0.5), (-10.6, 0.0, 12.0, 0.6)):
        for hand in ((PORT, STBD) if by_ else (PORT,)):
            yard.boat(bx_, hand * by_, z02 + bz, long_)
            m.use("aft")
            for t in (-0.28, 0.28):
                m.box((bx_ + t * long_, hand * by_, z02 + bz * 0.5), (0.35, long_ * 0.2, bz), "upper")
    for hand in (PORT, STBD):
        az_crane(yard, -20.6, hand * 7.4, zA, 20.6, 13.5, bearing=-hand * 8.0)

    # ---- the mainmast: its pole on the quarterdeck, its legs on the forecastle deck forward of it; the lower
    # platform, the searchlight platform, the gallery, the two-tiered top, the after director's round house over it,
    # and on that the open box of four .50s; topmast and gaff
    mx, mz = -26.8, 28.0
    leg = Vector((-19.2, 5.0, D(-19.2)))
    zt = mz + 2.6 + 2.6                                # (the roof of the top's second tier)
    zd = zt + 2.2                                      # (the director's roof)
    zn = zd + 1.3                                      # (the box's floor: its rim is at 37.9 m, as in her 1941 photograph)
    r_m = lerp(0.55, 0.36, (zM1 - D(mx)) / (zn - D(mx)))
    m.lathe((mx, 0, D(mx)), Z, [(0.0, 0.55), (zM1 - D(mx), r_m)], "upper", 18 if fine else 5, caps=(True, False))
    m.lathe((mx, 0, zM1), Z, [(0.0, r_m), (zn - zM1, 0.36)], "light", 18 if fine else 5, caps=(False, True))
    for hand in (PORT, STBD):
        a, b = Vector((leg.x, hand * leg.y, leg.z)), Vector((mx, 0.0, mz - 0.1))
        c = a.lerp(b, (zM1 - a.z) / (b.z - a.z))
        m.rod(a, c, 0.36, "upper", sides=14 if fine else 5, caps=(True, False))
        m.rod(c, b, 0.36, "light", sides=14 if fine else 5, caps=(False, True))
    zlo = 17.0                                         # the lower platform, across the legs, its lookouts' house at its after end
    lo_p = Model.round_rect(mx + 5.0, mx - 2.8, -3.6, 3.6, 0.8, 3 if fine else 1)
    m.prism(lo_p, zlo - 0.16, zlo, "upper", top_paint="deck", bottom=True)
    m.box((mx - 1.85, 0.0, zlo + 1.0), (1.5, 2.6, 2.0), "upper")
    m.box((mx - 1.85, 0.0, zlo + 2.04), (1.66, 2.76, 0.08), "roof")
    if fine:
        railing(m, [Vector((px_, py_, zlo)) for px_, py_ in lo_p] + [Vector((*lo_p[0], zlo))], every=1.5)
        for sy in (-0.65, 0.65):
            m.add([Vector((mx - 1.85 + 0.76, sy - 0.4, zlo + 1.35)), Vector((mx - 1.85 + 0.76, sy + 0.4, zlo + 1.35)),
                   Vector((mx - 1.85 + 0.76, sy + 0.4, zlo + 1.75)), Vector((mx - 1.85 + 0.76, sy - 0.4, zlo + 1.75))], [(0, 1, 2, 3)], "glass", False, lambda q: X)
        fit.sailor(m.use("aft"), Vector((mx + 2.6, 2.4, zlo)), Vector((0.2, 1.0, 0)), "stand", navy="USA")
    m.use("aft")
    zl = 21.5                                          # the searchlight platform: four lights
    lp = Model.round_rect(mx + 5.4, mx - 1.6, -4.3, 4.3, 1.0, 3 if fine else 1)
    m.prism(lp, zl - 0.16, zl, "upper", top_paint="deck", bottom=True)
    if fine:
        railing(m, [Vector((px_, py_, zl)) for px_, py_ in lp] + [Vector((*lp[0], zl))], every=1.5)
    for hand in (PORT, STBD):
        for lx in (mx + 3.7, mx + 0.4):
            yard.searchlight(lx, hand * 3.15, zl, 0.6, (0.3, hand * 1.0, 0.05))
        m.use("aft")
        for (xa, ya), (xb_, yb) in (((mx + 4.4, 3.3), (mx + 4.6, 4.0)), ((mx - 2.2, 3.1), (mx - 1.0, 3.8))):       # posts from the lower platform
            m.rod((xa, hand * ya, zlo), (xb_, hand * yb, zl - 0.16), 0.07, "upper", sides=4)
    if fine:
        fit.sailor(m.use("aft"), Vector((mx + 2.2, 3.4, zl)), Vector((0.3, 1.0, 0)), "stand", navy="USA")
        fit.sailor(m.use("aft"), Vector((mx + 2.2, -3.4, zl)), Vector((0.3, -1.0, 0)), "stand", navy="USA")
    m.use("aft")
    gal = Model.round_rect(mx + 4.4, mx - 4.4, -4.8, 4.8, 1.2, 3 if fine else 1)
    m.prism(gal, mz - 0.18, mz, "light", top_paint="deck", bottom=True)
    if fine:
        railing(m, [Vector((px_, py_, mz)) for px_, py_ in gal] + [Vector((*gal[0], mz))], every=1.6, paint="light")
    az_tiers(yard, ((mx + 3.3, mx - 3.3, 3.7, 2.6, True), (mx + 2.8, mx - 2.8, 3.1, 2.6, True)), mz)
    az_drum_director(yard, "director.aft", mx, zt, 2.0, zd - zt, 2.3, bearing=180.0)
    m.use("aft")
    # the .50s' box: wider than the director under it and set forward of the pole, on a flared skirt (her 1941
    # photograph: a squarish open box at the very top, the pole at its after edge)
    bx0, bx1, bh = mx + 4.1, mx - 0.9, 2.4
    zs0, zs1, rim = zd + 0.03, zn - 0.1, zn + 1.2
    skirt_lo = Model.round_rect(mx + 1.9, mx - 1.9, -1.9, 1.9, 0.9, 3 if fine else 1)
    skirt_hi = Model.round_rect(bx0, bx1, -bh, bh, 0.12, 3 if fine else 1)
    if len(skirt_lo) != len(skirt_hi):
        skirt_lo = skirt_hi
    m.prism(skirt_lo, zs0, zs1, "light", top=skirt_hi, top_paint="light", bottom=True)
    t_ = 0.08
    for (cx_, cy_, lx_, ly_) in (((bx0 - t_ / 2, 0.0, t_, 2 * bh)), ((bx1 + t_ / 2, 0.0, t_, 2 * bh)), (((bx0 + bx1) / 2, bh - t_ / 2, bx0 - bx1 - 2 * t_, t_)),
                                 (((bx0 + bx1) / 2, -bh + t_ / 2, bx0 - bx1 - 2 * t_, t_))):
        m.box((cx_, cy_, (zs1 + rim) * 0.5), (lx_, ly_, rim - zs1), "light")
    m.prism([(bx0 - t_, bh - t_), (bx1 + t_, bh - t_), (bx1 + t_, -bh + t_), (bx0 - t_, -bh + t_)], zs1, zn, "light", top_paint="deck")
    for gx_, gy_ in ((bx0 - 0.95, 1.4), (bx0 - 0.95, -1.4), (bx1 + 1.05, 1.5), (bx1 + 1.05, -1.5)):
        face = math.degrees(math.atan2(gy_, gx_ - (bx0 + bx1) / 2))
        fit.mg50(yard, gx_, gy_, zn + 0.02, facing=face, part="aft")
    m.use("aft")
    m.lathe((mx, 0, zn), Z, [(0.0, 0.22), (6.4, 0.07)], "light", 10 if fine else 4)
    m.rod((mx, -2.8, rim + 2.6), (mx, 2.8, rim + 2.6), 0.06, "light", sides=4)
    gaff0, gaff1 = Vector((mx - 0.15, 0, rim + 0.9)), Vector((mx - 3.8, 0, rim + 2.1))
    m.rod(gaff0, gaff1, 0.07, "light", sides=4)
    head_main = Vector((mx, 0, zn + 6.3))
    yard.wire(head_fore, head_main, 0.025, sag=1.0)
    sx = -h.L * 0.497
    ens = Vector((sx - 0.3, 0, D(sx + 0.6) + 5.5))
    yard.wire(head_main, ens, 0.025)                                  # the backstay, to the ensign staff's head
    jx = h.L * 0.494
    yard.wire(head_fore, (jx + 0.3, 0, D(jx) + 3.2), 0.025)            # the forestay, to the jackstaff's head
    m.socket("ensign", tuple(gaff1), (-1, 0, 0), "aft", "the ensign flies from the mainmast's gaff")

    # ---- ten 5-inch/51 in her casemates; eight 5-inch/25, the four at the ends in splinter tubs, each with its
    # ready-use lockers
    n_p = n_s = 0
    for cx in cas_x:
        for hand in (PORT, STBD):
            if hand == PORT:
                n_p += 1
                nm = f"sec.P{n_p}"
            else:
                n_s += 1
                nm = f"sec.S{n_s}"
            casemate_5in(yard, nm, cx, hand * 12.9, zc, -90.0 * hand)
    placed = []
    for ax_, ay_, gz, bearing, tub, deck_x in ((21.5, 8.1, z02, 0.0, 2.3, (-16.6, 23.3)), (14.5, 8.4, z02, 0.0, 0.0, (-16.6, 23.3)),
                                                (-12.5, 8.6, z02, 180.0, 0.0, (-16.6, 23.3)), (-26.0, 10.3, zA, 180.0, 2.2, (-29.2, -21.0))):
        for hand in (PORT, STBD):
            if hand == PORT:
                n_p += 1
                nm = f"sec.P{n_p}"
                lim = (-165, 20) if bearing == 0.0 else (-20, 165)
            else:
                n_s += 1
                nm = f"sec.S{n_s}"
                lim = (-20, 165) if bearing == 0.0 else (-165, 20)
            az_5in25(yard, nm, ax_, hand * ay_, gz, bearing, lim, tub=tub, parent="aft")
            m.use("aft")
            for dx in (-3.0, 3.0):
                lx, ly = ax_ + dx, hand * (ay_ + 1.4)
                if any(abs(lx - qx) < 1.7 and abs(ly - qy) < 1.0 for qx, qy in placed):
                    lx += -0.8 if dx > 0 else 0.8           # (drawn in toward its own gun, clear of the next gun's locker)
                if not (deck_x[0] < lx < deck_x[1]) or any(abs(lx - qx) < 1.7 and abs(ly - qy) < 1.0 for qx, qy in placed):
                    continue
                placed.append((lx, ly))
                m.box((lx, ly, gz + 0.55), (1.4, 0.8, 1.1), "upper")
                if fine:
                    m.box((lx, ly, gz + 1.12), (1.5, 0.9, 0.06), "upper")
                    for sx_ in (-0.4, 0.4):
                        m.box((lx + sx_, ly + hand * 0.42, gz + 0.8), (0.12, 0.04, 0.08), "steel")

    # ---- the fantail: the catapult on its turntable (whose base lies on the wreck) with its Kingfisher, the aircraft
    # crane at her stern, its jib raised clear of the plane and swung to port, its post and jib clear of the backstay
    # (her Kingfisher well aft on it, its tail clear of the fourth turret's muzzles)
    catapult_us(yard, -81.3, 0.0, D(-81.3), 15.0, 180.0, plane=False)
    kingfisher(yard, -80.4, 0.0, D(-81.3) + 1.55, 180.0)
    lattice_crane(yard, -89.6, 0.0, D(-89.6), 5.8, 11.0, bearing=35.0, lift=65.0)

    # ---- forecastle and quarterdeck: anchors, bollards, rails, scuttles, vents, rafts, staffs; boat booms and
    # accommodation ladders; sailors about her decks
    yard.anchor_gear(h.L * 0.47, 70.0, spread=2.4)
    for bx_ in (80.0, 60.0, -62.0, -74.0, -88.0):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 1.0), D(bx_))
    yard.rails(-h.L * 0.49, -21.6)
    yard.rails(-19.5, h.L * 0.485)
    yard.portholes(-h.L * 0.44, h.L * 0.45, 1.9, every=3.4, skip=[(-22.6, -19.0)])
    yard.portholes(-18.6, h.L * 0.43, 4.3, every=3.4)
    for hand in (PORT, STBD):
        # a boat boom stowed along each side under the forecastle's edge, its heel on a gooseneck
        za, zb = D(40.0) - 0.9, D(28.0) - 0.9
        pa, na = h.side(40.0, za, hand)
        pb, nb = h.side(28.0, zb, hand)
        a, b = pa + na * 0.3, pb + nb * 0.3
        m.use("fore")
        m.rod(a, b, 0.11, "upper", rb=0.08, sides=8 if fine else 4)
        m.box(pa + na * 0.15, (0.4, 0.4, 0.3), "steel", yaw=math.degrees(math.atan2(na.y, na.x)))
        for t in (0.45, 0.9):
            q = a.lerp(b, t)
            pq, nq = h.side(q.x, q.z, hand)
            m.rod(pq, q - Z * 0.14, 0.05, "upper", sides=4)
        # an accommodation ladder rigged down her side abaft the break of the forecastle, a platform at its foot
        m.use("aft")
        xt, xf_ = -34.4, -42.0
        top = Vector((xt, 0, D(xt) + 0.02))
        pt, nt = h.side(xt, D(xt) - 0.3, hand)
        pf, nf = h.side(xf_, 1.6, hand)
        yoff = max(abs(pt.y), abs(pf.y)) + 0.9
        top.y = hand * yoff
        foot = Vector((xf_, hand * yoff, 1.6))
        if fine:
            fit.stair(m, foot, top, wide=0.8)
            m.box((xt + 0.5, hand * (yoff + abs(pt.y)) * 0.5, D(xt) + 0.02), (1.4, yoff - abs(pt.y) + 0.5, 0.08), "deck")
            m.box((xf_ - 0.7, hand * yoff, 1.54), (1.6, 1.3, 0.12), "deck")
            m.rod((xf_ - 0.7, hand * (abs(pf.y) - 0.05), 1.4), (xf_ - 0.7, hand * (yoff - 0.5), 1.48), 0.06, "upper", sides=4)
    if fine:
        for fx_ in (3.0, -3.6):
            for hand in (PORT, STBD):
                az_raft(yard, fx_, hand * 12.9, z01 - 1.6, (0, hand, 0), n=2)
        for fx_ in (1.0, -3.0):
            for hand in (PORT, STBD):
                az_raft(yard, fx_, hand * 10.6, z01 + 1.2, (0, hand, 0), n=2)
        for vx in (58.0, 52.0, -60.0, -70.0):
            for hand in (PORT, STBD):
                yard.vent(vx, hand * h.half_at(vx) * 0.5, D(vx), 0.45, 1.3)
        for vx, vy in ((74.0, 4.6), (-46.5, 10.2), (-66.0, 6.2), (-79.0, 7.0)):
            for hand in (PORT, STBD):
                yard.vent(vx, hand * vy, D(vx), 0.36, 1.0)
        for hx, hy in ((53.5, 5.5), (37.5, 9.5), (-48.5, 9.0), (-64.0, 4.0)):       # hatches, their lids shut
            for hand in (PORT, STBD):
                if hand == STBD and hx == 37.5:
                    continue
                yard.on(hx).box((hx, hand * hy, D(hx) + 0.2), (1.6, 1.2, 0.4), "upper")
                m.box((hx, hand * hy, D(hx) + 0.43), (1.7, 1.3, 0.06), "deck")
                for dy in (-0.35, 0.35):
                    m.box((hx - 0.86, hand * hy + dy, D(hx) + 0.38), (0.06, 0.16, 0.1), "steel")
        # the awning stanchions along her quarterdeck (the wreck keeps their sockets), the awnings furled; low enough
        # for the fourth turret's guns to pass over them
        for hand in (PORT, STBD):
            pts = []
            x = -31.0
            while x > -86.0:
                pts.append(Vector((x, hand * (h.half_at(x) - 0.75), D(x))))
                x -= 3.2
            m.use("aft")
            for q in pts:
                m.rod(q, q + Z * 2.1, 0.04, "upper", sides=5, caps=(False, True))
            for qa, qb in zip(pts, pts[1:]):
                yard.wire(qa + Z * 2.1, qb + Z * 2.1, 0.015)
        for hand in (PORT, STBD):                    # the signal lamps at her bridge wings' ends
            yard.searchlight(19.9, hand * 10.5, z04, 0.3, (0.6, hand * 1.0, 0.0))
        for ox, oy, face in ((-32.8, 13.0, 1.6), (-33.4, 12.2, -0.4), (-32.6, -13.2, -1.6), (75.0, 6.5, 3.0), (73.6, -6.8, 2.8)):
            foot = Vector((ox, oy, D(ox) + 0.02))      # the officer of the deck's watch at the ladders; hands on the forecastle
            fit.sailor(m.use(yard.half(ox)), foot, Vector((math.cos(face), math.sin(face), 0)), "stand", navy="USA")
        rg = __import__("random").Random(1941)
        for cx, cy, k_ in ((-66.0, 0.0, 6), (67.0, 0.0, 3), (-33.8, 9.0, 2), (35.0, -9.0, 2)):
            for i in range(k_):
                a = rg.uniform(0, 2 * math.pi)
                foot = Vector((cx + rg.uniform(-2.5, 2.5), cy + rg.uniform(-2.5, 2.5), 0.0))
                foot.z = D(foot.x) + 0.02
                fit.sailor(m.use(yard.half(foot.x)), foot, Vector((math.cos(a), math.sin(a), 0)), "stand", navy="USA")
        for bx_, by_, face in ((-5.2, 7.4, -2.0), (-15.6, -7.6, 1.2), (-15.4, 7.7, -0.6), (-5.0, -7.3, 2.6)):     # hands by the boats
            fit.sailor(m.use("aft"), Vector((bx_, by_, z02 + 0.02)), Vector((math.cos(face), math.sin(face), 0)), "stand", navy="USA")
        for ox, oy, face in ((-18.6, 2.0, 3.0), (-23.0, 8.4, -1.2)):      # by the mainmast's legs; on the crane's house
            fit.sailor(m.use("aft"), Vector((ox, oy, D(ox) + 0.24 if abs(oy) < 6.0 else zA)), Vector((math.cos(face), math.sin(face), 0)), "stand", navy="USA")
    m.use("aft")
    m.rod((sx + 0.6, 0, D(sx + 0.6)), ens, 0.07, "upper", sides=4)
    m.use("fore")
    m.rod((jx, 0, D(jx)), (jx + 0.3, 0, D(jx) + 3.2), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.3, 0, D(jx) + 3.2), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=4, r=2.0, rudders=1)

    # ---- where she burns: her bridge, the forward magazines, the funnel, the boat deck, the quarterdeck
    yard.fire_points += [(18.0, 0.0, z04), (36.0, 0.0, D(36.0)), (fnx, 0.0, z03), (-12.0, 0.0, z02), (-62.0, 0.0, D(-62.0)), (60.0, 0.0, D(60.0))]
    from ships import clips_for_ship, sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, mains, "battleship")
    aa_clip(m)
    m.info.setdefault("file", {}).update({"class": "Pennsylvania", "navy": "USA", "year": 1941, "stands_for": "Battleship_USA",
                                          "paint": f"Measure 1: dark grey 5-D up to the funnel's top ({zM1:g} m), light grey 5-L on her masts and tops above it; "
                                                   "wooden decks weathered teak (lost before the fleet's decks were stained blue)"})
    return m, yard


# the light mountings that train and lift as parts: `<kind>.N` turns, `<kind>.N.guns` lifts (40 mm, 25 mm, 1.1-inch)
AA_TRAINS = ("bofors.", "aa25.", "aa28.")


def aa_clip(m):
    """Every light mounting that trains: they swing onto a bearing and fire in short bursts (the game aims each by its
    own part; this clip only shows them working)."""
    tracks, events = [], []
    for p in m.parts:
        if p.name.endswith(".guns") and p.name.startswith(AA_TRAINS):
            ph = (sum(map(ord, p.name)) % 7) * 0.05     # (a steady hash: Python's own changes every run)
            tracks.append({"part": p.name, "turn": [[0.0, 0.0], [0.6, 28.0], [1.0 + ph, 30.0], [1.15 + ph, 29.2], [1.3 + ph, 30.0], [1.45 + ph, 29.2], [1.6 + ph, 30.0], [3.0, 30.0]]})
    for s in m.sockets:
        if s["name"].startswith("aa."):
            events.append({"t": round(1.0 + (len(events) % 5) * 0.07, 2), "fx": "aa_tracer", "socket": s["name"], "cue": "ship_guns" if len(events) % 6 == 0 else ""})
    m.clips["fire.aa"] = {"long": 3.0, "loop": False, "about": "her light guns lift onto an aircraft and fire bursts of tracer from every `aa.N` socket", "tracks": tracks, "events": events}


BUILDERS = {
    "Arizona": lambda fine: arizona(fine),
    "Essex": lambda fine: essex(fine),
    "Iowa": lambda fine: iowa(fine),
    "Yamato": lambda fine: yamato(fine),
    "Shokaku": lambda fine: shokaku(fine),
}


# ---- named ships built in modules of their own (one file each, so they can be made side by side) ----------------
NAMED_MODULES = ("yorktown", "akagi", "kaga", "soryu")


def _load_named():
    import importlib
    for mod in NAMED_MODULES:
        try:
            BUILDERS.update(importlib.import_module(mod).BUILDERS)
        except ModuleNotFoundError as e:
            if e.name != mod:          # (a module that is there but fails to load is an error, not an absence)
                raise


_load_named()
