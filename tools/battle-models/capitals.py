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
from rigid import PORT, STBD, X, Y, Z, Model, lerp, smooth
from shipyard import Hull, Yard, livery, weather_for


# ---- a hull laid to a measured plan --------------------------------------------------------------------------

class PlanHull(Hull):
    """A hull whose deck edge follows a table measured off a plan: (x from her middle, half width as a share of half
    her beam). Everything else (the run of her bottom, her flare, her stem and stern) is the yard's hull's."""

    def __init__(self, table, **kw):
        super().__init__(**kw)
        self.table = sorted(table)

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


def us_funnel(yard, x, z0, top, long, wide, slope=1.6, name=None, lights=True, platform=None):
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
            pts.append((x + a_ * c * r, b_ * s * r))
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
    inner = [Vector((x + (p.x - x) * 0.86, p.y * 0.84, p.z)) for p in cap1]
    m.add(cap1 + inner, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "black", False, lambda q: Z)
    deep = [p - Z * 1.8 for p in inner]
    m.skin([deep, inner], "black", (True, False), inside=True, cap_paint="black")
    if fine:
        for j in range(-4, 5):                           # the grating over its mouth
            px = x + j * long * 0.09
            m.box((px, 0, zt(px, 0.0) + 0.03), (0.09, wide * 0.8, 0.06), "steel")
        for off, r in ((-1.0, 0.16), (0.0, 0.12), (1.0, 0.14)):    # steam pipes up its after face
            px = x - long * 0.5 - r - 0.08
            m.rod((px, off, z0), (px, off, zt(x - long * 0.5, 0.0) + 0.6), r, "upper", sides=6)
            m.lathe((px, off, zt(x - long * 0.5, 0.0) + 0.58), Z, [(0.0, r), (0.1, r * 1.4), (0.25, r * 1.4)], "black", 6)
        for k in range(4):                                # its plates' seams
            zz = lerp(z0, zt(x, 1.4), (k + 1) / 5.0)
            ring = [Vector((px * 1.0, py, zz)) for px, py in plan((zz - z0) / max(zt(x, 1.4) - z0, 1e-6))]
            ring = [Vector((x + (p.x - x) * 1.004, p.y * 1.004, p.z)) for p in ring]
            m.strip(ring + [ring[0]], [Vector((p.x - x, p.y, 0)).normalized() for p in ring + [ring[0]]], 0.012, "seam", 0.004)
    if platform is not None and lights:
        zp = platform
        from ships import railing
        for hand in (PORT, STBD):
            yy = hand * (wide * 0.5 + 0.9)
            m.box((x + 0.5, yy, zp), (3.2, 2.0, 0.12), "upper")
            for dx in (-1.2, 1.2):
                m.rod((x + 0.5 + dx, hand * wide * 0.42, zp - 1.1), (x + 0.5 + dx, yy + hand * 0.8, zp - 0.06), 0.06, "upper", sides=4)
            if fine:
                railing(m, [(x + 2.1, yy + hand * 0.95, zp), (x - 1.1, yy + hand * 0.95, zp)], every=1.0)
            yard.searchlight(x + 0.5, yy, zp + 0.06, 0.62, (0.6, hand * 0.8, 0.1))
    k = sum(1 for s in m.sockets if s["name"].startswith("funnel.")) + 1
    m.socket(name or f"funnel.{k}", (x, 0, top + 0.4), (-0.25, 0, 1), yard.half(x), "the funnel's mouth: her smoke rises here, more when she works up speed")
    yard.soot_at.append((x, 0, top, long))
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


def kingfisher(yard, x, y, z, bearing=0.0):
    """The battleship's scout floatplane on her catapult: a slim two-seat monoplane on one big float with a small float
    under each wingtip, two men under a long glazed hood (our own small model)."""
    m = yard.on(x)
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


def catapult_us(yard, x, y, z, long, bearing, plane=True):
    """A powder catapult on a turntable: a lattice girder track, its car at the after end with the floatplane on it."""
    m = yard.on(x)
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
        kingfisher(yard, c.x - f.x * long * 0.18, c.y - f.y * long * 0.18, c.z + 0.2, bearing)


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


def aa_clip(m):
    """Every light mounting that trains: they swing onto a bearing and fire in short bursts (the game aims each by its
    own part; this clip only shows them working)."""
    tracks, events = [], []
    t_ = 0.0
    for p in m.parts:
        if p.name.endswith(".guns") and (p.name.startswith("bofors.") or p.name.startswith("aa25.")):
            ph = (hash(p.name) % 7) * 0.05
            tracks.append({"part": p.name, "turn": [[0.0, 0.0], [0.6, 28.0], [1.0 + ph, 30.0], [1.15 + ph, 29.2], [1.3 + ph, 30.0], [1.45 + ph, 29.2], [1.6 + ph, 30.0], [3.0, 30.0]]})
    for s in m.sockets:
        if s["name"].startswith("aa."):
            events.append({"t": round(1.0 + (len(events) % 5) * 0.07, 2), "fx": "aa_tracer", "socket": s["name"], "cue": "ship_guns" if len(events) % 6 == 0 else ""})
    m.clips["fire.aa"] = {"long": 3.0, "loop": False, "about": "her light guns lift onto an aircraft and fire bursts of tracer from every `aa.N` socket", "tracks": tracks, "events": events}


BUILDERS = {
    "Iowa": lambda fine: iowa(fine),
}
