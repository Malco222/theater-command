"""Pits and emplacements for guns, and the works round them: the heavy flak pit, a sandbag ring, a gun pit for a
towed gun, the mortar and its pit, a camouflage net on poles, and a timber observation post.

Our own plain designs (a plain medium mortar of about 81 mm, a plain timber watchtower), no army's pattern, no emblem
and no lettering. The pits hold no gun of their own except the mortar pit: the game stands its gun model on socket
`gun`.
"""
import math

from mathutils import Matrix, Quaternion, Vector

import bits
import guns
from bits import X, Y, Z, rnd
from posts import weather
from rigid import smooth

MORE = {"rope": "8C7A58"}


# ---- helpers ------------------------------------------------------------------------------------------------

def V2(a, r=1.0):
    """A level unit vector at an angle (degrees) times r."""
    return Vector((math.cos(math.radians(a)) * r, math.sin(math.radians(a)) * r, 0.0))


def bag(m, at, yaw=0.0, size=bits.BAG, seed=0, paint=None, droop=0.03, pitch=0.0, roll=0.0, q=(7, 10), tie=0.35):
    """One filled sandbag (the kit's shape) at a chosen fineness `q` (rings along, points round), the neck tied off on
    some of them. The kit's own bag is finer; a wall of four hundred of them would be too many triangles."""
    g = rnd(seed)
    L, W, H = size[0] * g.uniform(0.94, 1.05), size[1] * g.uniform(0.95, 1.05), size[2] * g.uniform(0.92, 1.08)
    paint = paint or g.choice(["sandbag", "sandbag", "sandbag.2", "sandbag.2", "sandbag.3"])
    nu, nv = q if m.fine else (3, 4)
    rot = Matrix.Rotation(math.radians(yaw + g.uniform(-3, 3)), 3, "Z") @ Matrix.Rotation(math.radians(-pitch), 3, "Y") @ Matrix.Rotation(math.radians(roll + g.uniform(-2, 2)), 3, "X")
    at = Vector(at)
    lump = [g.uniform(-1, 1) for _ in range(6)]
    rings = []
    for i in range(nu):
        u = math.sin((-1.0 + 2.0 * i / (nu - 1)) * math.pi / 2) if nu > 4 else (-0.94 + 1.88 * i / (nu - 1))   # (more rings at the ends, where it turns)
        # (beaten flat into its course: a slab with rounded shoulders, the ends drawn in only a little)
        full = (1.0 - abs(u) ** 7.0) ** 0.33 if abs(u) < 1.0 else 0.0
        full = max(full, 0.42)
        wide = W / 2 * (0.74 + 0.26 * full) * (1.0 + 0.04 * lump[0] * math.sin(2.2 * u + lump[1] * 3))
        high = H * (0.52 + 0.48 * full) * (1.0 + 0.05 * lump[2] * math.cos(1.7 * u + lump[3] * 3))
        sag = -droop * (abs(u) ** 2.2) + 0.006 * lump[4] * math.sin(3.0 * u)
        ring = []
        for k in range(nv):
            a = 2 * math.pi * (k + (0.5 if nv < 8 else 0.0)) / nv
            c, s = math.cos(a), math.sin(a)
            y = wide * math.copysign(abs(c) ** 0.4, c)
            zz = math.copysign(abs(s) ** 0.4, s)
            z = (H / 2 + sag) + (high / 2) * zz * (1.0 if zz > 0 else 0.92) + (H / 2 - high / 2) * (-0.6)
            ring.append(at + rot @ Vector((u * L / 2, y, z)))
        rings.append(ring)
    m.skin(rings, paint, (True, True), True)
    if m.fine and g.random() < tie:
        end = at + rot @ Vector((L / 2 * 0.98, 0.0, H * 0.5 - droop))
        d = rot @ Vector((1.0, g.uniform(-0.5, 0.5), g.uniform(0.1, 0.6))).normalized()
        m.lathe(end, d, [(0.0, 0.03), (0.035, 0.016), (0.05, 0.03), (0.085, 0.036)], paint, 5, (False, True))
    return L


def bag_row(m, pts, z, seed=0, size=bits.BAG, half_step=False, lean=0.0, skip=None, q=(7, 10), tie=0.35, closed=False):
    """A course of bags end to end along a polyline (the kit's `bag_row` with this module's bag)."""
    g = rnd(seed)
    pts = [Vector((p[0], p[1], 0)) for p in pts]
    if closed:
        pts = pts + [pts[0]]
    segs, total = [], 0.0
    for a, b in zip(pts, pts[1:]):
        segs.append((a, b, total, (b - a).length))
        total += (b - a).length
    n = max(1, int(round(total / (size[0] * 0.93))))
    step = total / n
    extra = 1 if (half_step and not closed) else 0
    for i in range(n + extra):
        s = (i + (0.0 if half_step else 0.5)) * step
        sz = (step / 0.93 * 0.96, size[1], size[2])
        if half_step and not closed and (i == 0 or i == n):
            s = min(max(s, step * 0.28), total - step * 0.28)
            sz = (size[0] * 0.52, size[1], size[2])
        if skip and skip(s / total, z):
            continue
        for a, b, s0, ln in segs:
            if s <= s0 + ln + 1e-6:
                t = (s - s0) / max(ln, 1e-9)
                p = a.lerp(b, t)
                d = (b - a).normalized()
                break
        side = Vector((-d.y, d.x, 0))
        p = p + side * (lean + g.uniform(-0.012, 0.012))
        bag(m, (p.x, p.y, z + g.uniform(-0.004, 0.004)), math.degrees(math.atan2(d.y, d.x)), sz, g.randrange(1 << 30), None, q=q, tie=tie)


def bag_wall(m, pts, courses, z0=0.0, seed=0, batter=0.012, thick=1, size=bits.BAG, top=None, inward=1.0, q=(7, 10), hide=None, closed=False, tie=0.35, keep=None):
    """A wall of bags breaking joint (the kit's `bag_wall`); `hide(row, course)` leaves out a buried row's bags,
    `keep(row, course, f)` says whether the bag at f along the wall (0..1) is laid at all, `q` is the fineness, or a
    function of (row, course) giving it."""
    g = rnd(seed)
    for c in range(courses):
        z = z0 + c * size[2] * 0.93
        for t in range(thick):
            if keep:
                skip = (lambda f, zz, c=c, t=t: not keep(t, c, f) or (top is not None and c >= top(f)))
            else:
                skip = (lambda f, zz, c=c: c >= top(f)) if top else None
            lean = inward * (c * batter + t * size[1] * 0.92)
            sd = g.randrange(1 << 30)
            if hide and hide(t, c):
                continue
            qq = q(t, c) if callable(q) else q
            bag_row(m, pts, z, sd, size, half_step=(c + t) % 2 == 1, lean=lean, skip=skip, q=qq, tie=tie, closed=closed)
    return z0 + courses * size[2] * 0.93


def ring_bank(m, r0, profile, seed=0, lump=0.05, step=1.25, cut=None, paint="earth", a0=0.0, a1=360.0, centre=(0.0, 0.0), across=0.2, turf=0.0):
    """Thrown earth all round a pit (or part way round): `profile` is (distance outward from r0, height) pairs;
    `cut(x, y)` says how much of the bank stands there (0 where an entrance goes through it). Lumpy, never a smooth
    lathe. The profile is filled in to a point every `across` metres so the lumps and the turf have room."""
    if not m.fine:
        step, across = step * 2.5, 9.0
    dense = [profile[0]]
    for (o0, h0), (o1, h1) in zip(profile, profile[1:]):
        k = max(1, int(math.ceil((o1 - o0) / across)))
        for i in range(1, k + 1):
            t = i / k
            dense.append((o0 + (o1 - o0) * t, h0 + (h1 - h0) * (t * t * (3 - 2 * t) * 0.5 + t * 0.5)))
    profile = dense
    full = abs(a1 - a0) >= 359.9
    n = max(2, int(round(abs(a1 - a0) / step)))
    rows = n if full else n + 1
    c0 = Vector((centre[0], centre[1], 0.0))
    verts = []
    for i in range(rows):
        a = a0 + (a1 - a0) * i / n
        d = V2(a)
        for j, (off, h) in enumerate(profile):
            q = c0 + d * (r0 + off)
            f = cut(q.x, q.y) if cut else 1.0
            edge = j in (0, len(profile) - 1)
            toe = edge and h <= 0.02
            if toe and j > 0:
                # (the outer toe is never a ruled circle: it runs out further here, less there, and ends just under the ground)
                wide = abs(profile[-1][0] - profile[0][0])
                q = q + d * min(0.6, 0.3 * wide) * (0.35 + 0.9 * bits.fbm((q.x * 0.45 + seed * 1.7, q.y * 0.45, 2.0), 1.0, 2) + 0.3 * bits.fbm((q.x * 1.9, q.y * 1.9, seed + 4.0), 1.0, 2))
            w = 0.0 if toe else lump
            dz = bits.fbm((q.x + seed, q.y, h * 2.0), 0.55) * w * 1.6
            dr = bits.fbm((q.x + 31.0 + seed, q.y - 17.0, 0.3), 0.7) * w
            z = -0.03 if toe else max((h + (dz if h > 0.02 else 0.0)) * f, 0.006)
            verts.append(q + d * dr + Vector((0, 0, z)))
    k = len(profile)
    faces = []
    for i in range(rows if full else rows - 1):
        i1 = (i + 1) % rows
        for j in range(k - 1):
            faces.append((i * k + j, i * k + j + 1, i1 * k + j + 1, i1 * k + j))
    if turf > 0.0:
        bits.turfed(m, verts, faces, amount=turf, base=paint, seed=seed * 0.37, low=0.06)
    else:
        m.add(verts, faces, paint, True, lambda p: Z)


def duckboard(m, a, b, width=0.56, z=0.0, seed=0, every=0.12):
    """A duckboard: two bearers and slats nailed across them, a little out of true."""
    g = rnd(seed)
    a, b = Vector((a[0], a[1], z)), Vector((b[0], b[1], z))
    d = b - a
    L = d.length
    d.normalize()
    s = Vector((-d.y, d.x, 0))
    for side in (-1, 1):
        bits.plank(m, a + s * side * width * 0.36 + Z * 0.03, b + s * side * width * 0.36 + Z * 0.03, 0.06, 0.055, "timber", Z, g.randrange(999))
    n = max(2, int(L / (every if m.fine else every * 2.2)))
    for k in range(n):
        t = (k + 0.5) / n
        c = a + d * (L * t) + Z * (0.069 + g.uniform(-0.003, 0.003))
        tw = s.copy()
        tw.rotate(Quaternion(Z, math.radians(g.uniform(-3, 3))))
        w = width * g.uniform(0.94, 1.02) / 2
        bits.plank(m, c - tw * w + d * g.uniform(-0.01, 0.01), c + tw * w, 0.085 * g.uniform(0.9, 1.05), 0.022, g.choice(["planks", "planks", "timber"]), Z, g.randrange(999))
        if m.fine:
            for side in (-1, 1):
                for dd in (-0.02, 0.02):
                    m.disc(c + s * side * width * 0.36 + d * dd + Z * 0.0115, Z, 0.006, "iron", 4)


def ammo_box(m, at, yaw=0.0, size=(0.98, 0.36, 0.17), seed=0, lid=False, rounds=0, paint=None, pitch=0.0, cal=0.09, long=0.86):
    """A wooden ammunition box: battens on the lid and the ends, a rope handle at each end, a hasp and two hinges.
    `lid` open shows `rounds` lying in it."""
    g = rnd(seed)
    paint = paint or g.choice(["wood.paint", "wood.paint", "planks"])
    L, W, H = size
    at = Vector(at)
    rot = Matrix.Rotation(math.radians(yaw), 3, "Z") @ Matrix.Rotation(math.radians(-pitch), 3, "Y")

    def P(x, y, z):
        return at + rot @ Vector((x, y, z))
    u, v = rot @ X, rot @ Y
    if lid:
        # an open box: four sides and a floor, the rounds in it, the lid leaning back on its hinges
        th = 0.018
        m.box(P(0, 0, th / 2), (L, W, th), paint, yaw, pitch)
        for sy in (-1, 1):
            m.box(P(0, sy * (W / 2 - th / 2), H / 2), (L, th, H), paint, yaw, pitch)
        for sx in (-1, 1):
            m.box(P(sx * (L / 2 - th / 2), 0, H / 2), (th, W - 2 * th, H), paint, yaw, pitch)
        for k in range(rounds):
            yy = (k - (rounds - 1) / 2) * (W - 0.05) / max(rounds, 1)
            guns.shell(m, P(-L / 2 + 0.04, yy, 0.02 + cal * 0.64), u, cal, min(long, L - 0.08))
        m.box(P(0, -W / 2 - 0.02, H + W / 2 - 0.03), (L, W, 0.02), paint, yaw, pitch, -75.0)
    else:
        m.box(P(0, 0, H / 2), (L, W, H), paint, yaw, pitch)
        for x in (-L * 0.34, L * 0.34):
            m.box(P(x, 0, H + 0.006), (0.06, W + 0.012, 0.014), paint, yaw, pitch)
        if m.fine:
            m.box(P(0, W / 2 + 0.004, H - 0.03), (0.05, 0.006, 0.05), "steel", yaw, pitch)          # the hasp
            for x in (-L * 0.25, L * 0.25):
                m.box(P(x, -W / 2 - 0.003, H - 0.025), (0.07, 0.005, 0.04), "steel", yaw, pitch)    # the hinges
    for sx in (-1, 1):
        m.box(P(sx * (L / 2 + 0.008), 0, H * 0.55), (0.016, W * 0.8, 0.06), paint, yaw, pitch)      # the end cleat
        if m.fine:
            sag = g.uniform(0.03, 0.06)
            pts = [P(sx * (L / 2 + 0.012), -0.075, H * 0.62), P(sx * (L / 2 + 0.04), -0.05, H * 0.6 - sag), P(sx * (L / 2 + 0.045), 0.0, H * 0.6 - sag * 1.3),
                   P(sx * (L / 2 + 0.04), 0.05, H * 0.6 - sag), P(sx * (L / 2 + 0.012), 0.075, H * 0.62)]
            bits.tube(m, pts, 0.009, "rope", 5)


def case(m, at, axis, r=0.0576, long=0.57, sides=10, dark=True):
    """An empty brass case: the rim, the body, the neck, the mouth dark."""
    at, axis = Vector(at), Vector(axis).normalized()
    m.lathe(at, axis, [(0.0, r), (0.014, r * 0.92), (long * 0.8, r * 0.87), (long, r * 0.72)], "brass", sides, (True, False))
    if dark and m.fine:
        m.disc(at + axis * (long - 0.004), axis, r * 0.66, "void", sides)


def bucket(m, at, r=0.14, h=0.27, paint="steel.worn", tipped=False, seed=0):
    at = Vector(at)
    axis = Vector((0.9, 0.3, 0.25)).normalized() if tipped else Z
    base = at + (Z * r * 0.95 if tipped else Vector())
    m.lathe(base, axis, [(0.0, r * 0.78), (h, r), (h + 0.01, r * 1.04)], paint, 16 if m.fine else 8, (True, False))
    m.lathe(base + axis * h, -axis, [(0.0, r * 0.98), (h - 0.01, r * 0.76)], "rust", 16 if m.fine else 8, (False, True))
    side = axis.cross(Y if abs(axis.y) < 0.9 else X).normalized()
    lean = 0.35 if not tipped else 0.0
    up = (axis * math.cos(lean) + axis.cross(side) * math.sin(lean)).normalized()
    bits.tube(m, [base + axis * (h + 0.005) + side * (r * 1.04 * math.cos(math.pi * k / 10)) + up * (r * 0.75 * math.sin(math.pi * k / 10)) for k in range(11)], 0.005, "steel", 4)


def spade(m, foot, lean=(-0.12, 0.05, 1.0), paint="timber"):
    """A short digging spade stuck in the earth: the blade half in, the haft leaning."""
    foot, d = Vector(foot), Vector(lean).normalized()
    m.rod(foot + d * 0.1, foot + d * 0.85, 0.016, paint, sides=8)
    m.rod(foot + d * 0.85, foot + d * 0.95, 0.02, "timber.dark", sides=8)
    s = d.cross(Z if abs(d.z) < 0.99 else X).normalized()
    if s.length < 0.5:
        s = Y
    bits.plate(m, [(-0.08, 0.0), (0.08, 0.0), (0.085, 0.17), (0.0, 0.22), (-0.085, 0.17)], 0.004, "steel", foot + d * 0.15, s, -d)


def stakes(m, a, b, n, h=0.5, r=0.03, seed=0, paint="timber"):
    a, b = Vector(a), Vector(b)
    g = rnd(seed)
    for k in range(n):
        p = a.lerp(b, (k + 0.5) / n)
        bits.log(m, p - Z * 0.1, p + Vector((g.uniform(-0.03, 0.03), g.uniform(-0.03, 0.03), h * g.uniform(0.9, 1.1))), r, paint, g.randrange(999), sides=6, taper=0.8)


def ray_poly(poly, a):
    """How far from the origin a ray at angle a (degrees) meets a closed outline (x, y points)."""
    d = V2(a)
    best = 0.0
    n = len(poly)
    for i in range(n):
        p, q = Vector((poly[i][0], poly[i][1], 0)), Vector((poly[(i + 1) % n][0], poly[(i + 1) % n][1], 0))
        e = q - p
        den = d.x * e.y - d.y * e.x
        if abs(den) < 1e-12:
            continue
        t = (p.x * e.y - p.y * e.x) / den
        u = (p.x * d.y - p.y * d.x) / den
        if t > 0 and -1e-9 <= u <= 1 + 1e-9:
            best = max(best, t)
    return best


def dug_bowl(m, outline_r, floor_z, batter, step=3.0, seed=0, lump=0.03, wall="earth", floor="earth.dry", flare=0.15, trodden=False):
    """A hole dug in level ground, star-shaped about the origin: `outline_r(a)` is its edge at ground level (a in
    degrees), `floor_z(x, y)` the floor's height, `batter(a)` how far in the wall's foot stands from its top. Walls
    lumpy, a lip of loose earth round the edge lying over the ground. Returns the plan outline for `m.info["dig"]`,
    drawn `flare` outside the hole's edge so the ground the stage leaves never overhangs the hole. `trodden`: the
    floor's lumps only go down from `floor_z`, never up (a kneeling man's shin lies on the floor: a lump 1 cm high
    under it reads to the crew check as his leg in the ground)."""
    if not m.fine:
        step *= 2.0
    n = int(round(360.0 / step))
    verts, cols = [], None
    dig = []
    ft = (0.0, 0.3, 0.55, 0.75, 0.88, 0.96, 1.0) if m.fine else (0.0, 0.6, 1.0)
    for i in range(n):
        a = 360.0 * i / n
        d = V2(a)
        Ro = outline_r(a)
        Rf = Ro - batter(a)
        row = []
        for t in ft:
            p = d * (Rf * t)
            up = bits.fbm((p.x * 2 + seed, p.y * 2, 1.0), 0.5) * 0.012 if t > 0 else 0.0
            row.append(p + Z * (floor_z(p.x, p.y) + (min(up, 0.0) if trodden else up)))
        zf = floor_z((d * Rf).x, (d * Rf).y)
        for t in ((0.3, 0.65) if m.fine else (0.5,)):
            r = Rf + (Ro - Rf) * t
            p = d * r
            dr = bits.fbm((p.x * 1.6 + seed, p.y * 1.6, zf * (1 - t) * 3), 0.45) * lump * 1.5
            row.append(d * (r + dr) + Z * (zf * (1 - t)))
        for r, z in ((0.0, 0.03), (0.12, 0.045), (0.4, 0.03), (0.65, 0.016), (0.85, 0.0)) if m.fine else ((0.0, 0.03), (0.4, 0.03), (0.85, 0.0)):
            p = d * (Ro + r)
            row.append(p + Z * (z + (bits.fbm((p.x * 2 + seed, p.y * 2, 5.0), 0.5) * 0.02 if 0 < r < 0.8 else 0.0)))
        verts += row
        cols = len(row)
        dig.append(tuple((d * (Ro + flare)).to_2d()))
    faces = []
    for i in range(n):
        i1 = (i + 1) % n
        for j in range(cols - 1):
            faces.append((i * cols + j, i * cols + j + 1, i1 * cols + j + 1, i1 * cols + j))

    def paint(mid, nn):
        return floor if (nn.z > 0.85 and mid.z < -0.02) else ("earth" if mid.z < 0.01 else "earth.dry")
    m.add(verts, faces, paint, True, lambda p: Z)
    return dig


def slit(m, a, b, w=0.75, depth=1.1, seed=0, cover=0.6, flare=0.15):
    """A slit dug for men to shelter in: straight walls a little battered, the floor duckboarded, boards on stakes
    along the top of each long wall, the far end roofed with logs and bags, the spoil heaped on the side away from
    `a`'s left. Returns its plan outline (for `m.info["dig"]`)."""
    g = rnd(seed)
    a, b = Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0))
    d = (b - a)
    L = d.length
    d.normalize()
    s = Vector((-d.y, d.x, 0))
    bt = 0.08
    nl = 6 if m.fine else 2
    nd = 3 if m.fine else 1
    # walls: four sides, each a grid from the top edge down to the floor, lumpy
    def wall(p0, p1, inward):
        rows = []
        for i in range(nl + 1):
            t = i / nl
            top = p0.lerp(p1, t)
            row = []
            for k in range(nd + 1):
                f = k / nd
                q = top + inward * (bt * f) + Z * (-depth * f)
                if 0 < i < nl and 0 < k < nd:
                    q = q - inward * bits.fbm((q.x * 2.5 + seed, q.y * 2.5, q.z * 2.5), 0.4) * 0.035
                row.append(q)
            rows.append(row)
        vs = [p for r in rows for p in r]
        fs = [(i * (nd + 1) + k, i * (nd + 1) + k + 1, (i + 1) * (nd + 1) + k + 1, (i + 1) * (nd + 1) + k) for i in range(nl) for k in range(nd)]
        m.add(vs, fs, "earth", True, lambda p, inward=inward: inward)
    hw = w / 2
    c = [a - s * hw, b - s * hw, b + s * hw, a + s * hw]
    wall(c[0], c[1], s)
    wall(c[2], c[3], -s)
    wall(c[1], c[2], -d)
    wall(c[3], c[0], d)
    fl = [c[0] + s * bt + d * bt, c[1] + s * bt - d * bt, c[2] - s * bt - d * bt, c[3] - s * bt + d * bt]
    m.add([p - Z * depth for p in fl], [(0, 1, 2, 3)], "earth.dry", False, lambda p: Z)
    # the lip round the top
    out = [c[0] - s * 0.0 - d * 0.0, c[1], c[2], c[3]]
    dirs = [(-s - d), (-s + d), (s + d), (s - d)]
    lipv = []
    for k in range(4):
        for r, z in ((0.0, 0.03), (0.35, 0.025), (0.8, 0.0)):
            lipv.append(out[k] + dirs[k] * r * 0.7071 + Z * z)
    faces = []
    for k in range(4):
        k1 = (k + 1) % 4
        for j in range(2):
            faces.append((k * 3 + j, k * 3 + j + 1, k1 * 3 + j + 1, k1 * 3 + j))
    m.add(lipv, faces, "earth.dry", True, lambda p: Z)
    # the floor's duckboard, boards on stakes along the top of the long walls
    duckboard(m, a + d * 0.15, b - d * 0.15, w - 2 * bt - 0.06, -depth, g.randrange(999), 0.13)
    for side in (-1, 1):
        base = (a + b) / 2 + s * side * (hw - 0.01)
        for k in range(2 if m.fine else 1):
            z = -0.12 - 0.15 * k
            bits.plank(m, a + d * 0.1 + s * side * (hw - 0.025) + Z * z, b - d * 0.1 + s * side * (hw - 0.025) + Z * z, 0.14, 0.03, "planks", -s * side, g.randrange(999))
        for t in (0.15, 0.5, 0.85):
            p = a.lerp(b, t) + s * side * (hw - 0.06)
            bits.log(m, p - Z * 0.5, p + Z * 0.06, 0.035, "timber", g.randrange(999), sides=6)
    # the far end roofed: logs across, bags on them
    n = int(L * cover / 0.14)
    for k in range(n):
        p = b - d * (0.08 + k * 0.14)
        bits.log(m, p - s * (hw + 0.3) + Z * 0.08, p + s * (hw + 0.3) + Z * 0.09, 0.07, "timber", g.randrange(999))
    for k in range(int(L * cover / 0.5)):
        p = b - d * (0.3 + k * 0.5)
        bag(m, p + s * 0.12 + Z * 0.15, math.degrees(math.atan2(d.y, d.x)) + 90, seed=g.randrange(999), q=(6, 9))
        bag(m, p - s * 0.22 + Z * 0.15, math.degrees(math.atan2(d.y, d.x)) + 90 + g.uniform(-8, 8), seed=g.randrange(999), q=(6, 9))
    return [tuple((p + dd * flare * 0.7071).to_2d()) for p, dd in zip(c, dirs)]


# ---- 1. the heavy flak pit -------------------------------------------------------------------------------------

def FlakPit(fine=True, nation="_default", name="FlakPit"):
    """The pit a heavy anti-aircraft gun stands in: a ring of thrown earth, its inside face revetted with planks on
    posts and topped with sandbags, two entrances at the rear quarters behind baffle walls, four lockers of rounds let
    into the wall. No gun: the game stands one on `gun`."""
    m = bits.model(name, "Heavy flak pit (our own model)", "works", nation, fine, MORE)
    m.part("pit", None, (0, 0, 0), kind="root")
    m.use("pit")
    g = rnd(41)
    R = 3.6                      # the revetment's face: the posts' inner faces
    RP, RPL, RBK = 3.67, 3.76, 3.82     # the posts' middles, the planks', the earth behind them
    RB = 3.80                    # the bags' middle line
    TOP = 0.76                   # the planking's top, where the bags start
    ENT = (135.0, -135.0)
    GH = 0.55                    # half an entrance's width
    REC = (55.0, -55.0, 100.0, -100.0)
    RH = 0.60                    # half a locker's width
    gh = math.degrees(math.asin(GH / RP))
    rh = math.degrees(math.asin(RH / RP))

    def adiff(a, b):
        return (a - b + 180.0) % 360.0 - 180.0

    def in_gap(a):
        return any(abs(adiff(a, e)) < gh for e in ENT)

    def in_rec(a):
        return any(abs(adiff(a, r)) < rh for r in REC)

    # -- the posts: every 15 degrees, and one at each side of every opening
    angs = [a for a in range(-180, 180, 15) if all(abs(adiff(a, e)) > gh + 4 for e in ENT) and all(abs(adiff(a, r)) > rh + 4 for r in REC)]
    angs += [e + s * gh for e in ENT for s in (-1, 1)] + [r + s * rh for r in REC for s in (-1, 1)]
    angs = sorted(((a + 180.0) % 360.0) - 180.0 for a in angs)
    for k, a in enumerate(angs):
        c = V2(a)
        bits.log(m, c * RP - Z * 0.12, c * RP + Z * (TOP + 0.01), 0.068, "timber", 100 + k, taper=0.95)
    # -- the planks between them, five courses, and the earth behind
    for k, a in enumerate(angs):
        b = angs[(k + 1) % len(angs)]
        span = (b - a) % 360.0
        mid = a + span / 2
        if in_gap(mid) or in_rec(mid):
            continue
        ca, cb = V2(a), V2(a + span)
        n = V2(mid)
        for c in range(5 if fine else 1):
            z = 0.075 + c * 0.152 + g.uniform(-0.006, 0.006) if fine else TOP / 2
            pa = ca * RPL + Z * z - (cb - ca).normalized() * 0.04
            pb = cb * RPL + Z * (z + g.uniform(-0.012, 0.012)) + (cb - ca).normalized() * 0.04
            bits.plank(m, pa, pb, 0.145 * g.uniform(0.95, 1.0) if fine else TOP, 0.04, g.choice(["planks", "planks", "timber"]), -n, g.randrange(999))
            if fine:
                for p, cc in ((a, ca), (a + span, cb)):
                    m.disc(cc * (RP - 0.066) + Z * (z + 0.02), -cc, 0.008, "iron", 4)
        # the earth behind the planks, seen through the gaps between them
        steps = max(2, int(span / 3))
        ring = [[V2(a + span * i / steps, RBK + 0.02 * bits.fbm((a + i, z0, 0.0), 0.5)) + Z * z0 for i in range(steps + 1)] for z0 in (0.0, TOP + 0.02)]
        m.add(ring[0] + ring[1], [(i, i + 1, steps + 2 + i, steps + 1 + i) for i in range(steps)], "earth", True, lambda p: -Vector((p.x, p.y, 0)))
    # -- three courses of bags on top, all round but for the entrances
    for a0, a1, sd in ((ENT[1] + gh, ENT[0] - gh, 3), (ENT[0] + gh, 360.0 + ENT[1] - gh, 4)):
        line = bits.arc(0, 0, RB, a0 + 0.8, a1 - 0.8)
        bag_wall(m, line, 3, TOP, seed=sd, batter=0.02, thick=1, inward=-1.0, q=(6, 10))

    # -- the bank: about 1.1 m high and 2.3 m thick, the entrances cut through it
    def cut(x, y):
        f = 1.0
        for e in ENT:
            d = V2(e)
            along = x * d.x + y * d.y
            perp = abs(-x * d.y + y * d.x)
            if along > 3.0:
                f = min(f, (0.92 + 0.08 * smooth((perp - 0.74) / 0.3)) if perp > 0.73 else 0.0)
        return f
    prof = [(0.0, 0.92), (0.08, 0.98), (0.28, 1.06), (0.63, 1.12), (1.08, 1.10), (1.53, 0.86), (1.93, 0.48), (2.28, 0.16), (2.53, 0.03), (2.68, 0.0)]
    ring_bank(m, 3.9, prof, seed=5, lump=0.06, step=1.6, cut=cut, turf=0.52)
    bits.patch(m, 0.0, 0.0, 3.62, "earth.dry", seed=9, n=64, lift=0.008, ragged=0.01)

    # -- the entrances: planked like the pit's face, a baffle wall behind, a duckboard path in
    def hb(r):
        o = r - 3.9
        for (o0, h0), (o1, h1) in zip(prof, prof[1:]):
            if o0 <= o <= o1:
                return h0 + (h1 - h0) * (o - o0) / (o1 - o0)
        return prof[0][1] if o < 0 else 0.0
    for i, e in enumerate(ENT):
        d = V2(e)
        p = Vector((-d.y, d.x, 0))
        for side in (-1, 1):
            rs = [3.93, 4.62, 5.3, 5.95, 6.45]
            for k, r in enumerate(rs[:-1]):
                bits.log(m, d * r + p * side * (GH + 0.07) - Z * 0.1, d * r + p * side * (GH + 0.07) + Z * (hb(r) + 0.03), 0.06, "timber", 200 + 10 * i + k + side, taper=0.95)
                r1 = rs[k + 1]
                for c in range(6):
                    z = 0.075 + c * 0.152 + g.uniform(-0.006, 0.006)
                    if z + 0.05 > hb(r if c < 2 else (r + r1) / 2) or (k == len(rs) - 2 and z > 0.3):
                        break
                    a0 = d * (r - 0.05) + p * side * (GH + 0.16) + Z * z
                    a1 = d * (r1 + 0.05) + p * side * (GH + 0.16) + Z * (z + g.uniform(-0.01, 0.01))
                    bits.plank(m, a0, a1, 0.145 * g.uniform(0.95, 1.0), 0.04, g.choice(["planks", "planks", "timber"]), -p * side, g.randrange(999))
                    if fine:
                        m.disc(d * r + p * side * (GH + 0.007) + Z * (z + 0.02), -p * side, 0.008, "iron", 4)
        base = d * 7.15
        bag_wall(m, [(base - p * 1.2).to_2d(), (base + p * 1.2).to_2d()], 5, 0.0, seed=30 + i, batter=0.01, thick=2, inward=1.0, q=lambda t, c: (6, 9) if t == 0 else (5, 8))
        duckboard(m, d * 6.6, d * 2.35, 0.56, 0.0, seed=50 + i)
        bits.patch(m, *(d * 6.9).to_2d(), 1.6, "earth.dry", seed=11 + i, oval=1.4, yaw=e, lift=0.007)

    # -- the four lockers let into the wall
    for i, a in enumerate(REC):
        locker(m, a, RPL, TOP, RH, seed=60 + i)
        c = V2(a)
        m.socket(f"ammo.{i + 1}", c * (R - 0.25), c, note=f"the mouth of locker {i + 1}: a carrier stands here, facing it, and takes a round from its shelf")

    # -- the empties, thrown out over the rear wall in a heap
    heap = Vector((-7.2, 0.15, 0.0))
    if fine:
        for k in range(72):
            layer = 0 if k < 38 else (1 if k < 60 else 2)
            rr = (1.05, 0.75, 0.45)[layer] * math.sqrt(g.random())
            a = g.uniform(0, 6.283)
            pos = heap + Vector((math.cos(a) * rr * 1.25, math.sin(a) * rr, 0.06 + layer * 0.085 + 0.03 * (1 - rr)))
            yaw = g.uniform(0, 6.283)
            ax = Vector((math.cos(yaw), math.sin(yaw), g.uniform(-0.25, 0.25)))
            case(m, pos - ax * 0.28, ax, sides=8)
        for k in range(9):
            a = g.uniform(0, 6.283)
            rr = g.uniform(1.4, 3.0)
            pos = heap + Vector((math.cos(a) * rr, math.sin(a) * rr * 1.2, 0.058))
            yaw = g.uniform(0, 6.283)
            ax = Vector((math.cos(yaw), math.sin(yaw), 0))
            case(m, pos - ax * 0.28, ax, sides=8)
    else:
        m.ball(heap + Z * 0.05, (1.2, 1.0, 0.22), "brass", 10, 4, top_only=True)
    bits.patch(m, heap.x, heap.y, 1.6, "earth", seed=12, oval=1.3, lift=0.006)

    # -- what lies about: a telephone on the rear wall and its cable out, a bucket, a spade, the cleaning staff
    tel = V2(180.0, R + 0.02) + Z * 0.95
    m.box(tel + Vector((0.12, 0, 0)), (0.12, 0.26, 0.2), "wood.paint")
    bits.log(m, tel + Vector((0.02, 0, -1.05)), tel + Vector((0.02, 0, 0.12)), 0.05, "timber", 77, taper=0.9)
    bits.plank(m, tel + Vector((0.06, 0, -0.12)), tel + Vector((0.2, 0, -0.12)), 0.3, 0.025, "planks", Z, 3)
    m.box(tel + Vector((0.12, 0, 0.11)), (0.13, 0.27, 0.02), "wood.paint")
    m.rod(tel + Vector((0.2, -0.08, 0.06)), tel + Vector((0.2, 0.08, 0.06)), 0.018, "iron", sides=8)
    if fine:
        cable = [tel + Vector((0.15, 0.1, -0.1))] + [V2(180 - 4 * k, R - 0.05) + Z * (0.02 if k > 1 else 0.5 - 0.24 * k) for k in range(1, 12)]
        cable += [V2(ENT[0]) * r + V2(ENT[0] + 90) * 0.42 + Z * 0.015 for r in (3.7, 4.6, 5.6, 6.6, 7.4)]
        bits.tube(m, cable, 0.005, "rubber", 4)
    bucket(m, V2(76.0, 3.3), seed=1)
    bucket(m, V2(-78.0, 3.32), seed=2, tipped=True)
    spade(m, V2(170.0, 4.6) + Z * 1.06, (0.15, 0.08, 1.0))
    staff0, staff1 = V2(160.0, 3.95) + Z * 1.18, V2(195.0, 3.95) + Z * 1.13
    m.rod(staff0, staff1, 0.022, "timber", sides=8)
    m.lathe(staff1, staff1 - staff0, [(0.0, 0.05), (0.32, 0.05), (0.34, 0.03)], "canvas", 10)
    # a range board on the rear wall, a sheet of figures pinned to it
    bits.plate(m, [(-0.22, 0.06), (0.22, 0.06), (0.22, 0.34), (-0.22, 0.34)], 0.004, "sign.3", V2(200.0, R - 0.072) + Z * 0.95, -V2(110.0), Vector((0.1 * math.cos(math.radians(200)), 0.1 * math.sin(math.radians(200)), 1)))
    bits.plate(m, [(-0.3, 0.0), (0.3, 0.0), (0.3, 0.4), (-0.3, 0.4)], 0.02, "planks", V2(200.0, R - 0.05) + Z * 0.95, -V2(110.0), Vector((0.1 * math.cos(math.radians(200)), 0.1 * math.sin(math.radians(200)), 1)))

    # -- the gun's place, the crew, a hit
    m.socket("gun", (0.0, 0.0, 0.0), (1, 0, 0), note="the heavy flak gun stands here facing the enemy (build it from its own model). Floor clear to 3.3 m radius: "
             "nothing on it but the duckboards and the floor itself, so the gun's platform and crew can turn all round")
    bits.crew(m, "carrier.1", V2(REC[0], R - 0.3), V2(REC[0]), "stand", "pit", "an ammunition carrier at locker 1, taking a round off its shelf (he carries it to the gun's loader)")
    bits.crew(m, "carrier.2", V2(REC[3], R - 0.3), V2(REC[3]), "stand", "pit", "an ammunition carrier at locker 4, taking a round off its shelf")
    bits.crew(m, "commander", V2(180.0, 3.35), (1, 0, 0), "stand", "pit", "the gun commander at the rear wall, field glasses up, giving the orders; the telephone is at his back")
    m.socket("hit", (4.9, 0.0, 1.05), (1, 0, 0), note="where a shell strikes the bank's front")
    m.socket("dust", (0.0, 0.0, 0.1), (0, 0, 1), note="where the gun's blast lifts dust off the floor")
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing empty.", "tracks": [], "events": []}
    weather(m, splash=0.35, rust=0.4)
    m.info["span"] = 15.0
    m.info["high"] = 0.7
    m.info["worn"] = [(0.0, 0.0, 4.4)] + [tuple(V2(e, 6.6).to_2d()) + (1.8,) for e in ENT] + [(-7.2, 0.0, 2.0)]
    m.info["clear"] = [[tuple(V2(a, 6.6).to_2d()) for a in range(0, 360, 15)]]
    return m


def locker(m, a, rface, top, half, seed=0):
    """A locker let into the wall: timber sides, back, floor and roof under a lintel, a shelf of rounds stacked nose
    in, boxes of rounds on the floor below."""
    g = rnd(seed)
    c, t = V2(a), V2(a + 90)
    D = 0.95
    def P(r, s, z):
        return c * r + t * s + Z * z
    r0, r1 = rface - 0.02, rface + D
    # lintel and its sill
    bits.plank(m, P(r0 - 0.04, -half - 0.12, top - 0.05), P(r0 - 0.04, half + 0.12, top - 0.05), 0.10, 0.12, "timber", -c, g.randrange(999))
    # sides and back: level boards
    for k in range(5):
        z = 0.075 + k * 0.145
        for s in (-1, 1):
            bits.plank(m, P(r0, s * (half + 0.01), z), P(r1 + 0.02, s * (half + 0.01), z), 0.14, 0.03, "planks", t * -s, g.randrange(999))
        bits.plank(m, P(r1, -half - 0.02, z), P(r1, half + 0.02, z), 0.14, 0.03, "planks", -c, g.randrange(999))
    # roof boards, radial, under the bags
    for k in range(8):
        s = -half + (k + 0.5) * (2 * half / 8)
        bits.plank(m, P(r0 - 0.06, s, top - 0.002), P(r1 + 0.03, s, top - 0.002), 2 * half / 8 * 0.95, 0.03, "planks", Z, g.randrange(999))
    # floor boards on two bearers
    for s in (-half * 0.6, half * 0.6):
        bits.plank(m, P(r0, s, 0.025), P(r1, s, 0.025), 0.06, 0.05, "timber", Z, g.randrange(999))
    for k in range(6):
        s = -half + (k + 0.5) * (2 * half / 6)
        bits.plank(m, P(r0, s, 0.06), P(r1, s, 0.06), 2 * half / 6 * 0.93, 0.022, "planks", Z, g.randrange(999))
    # the shelf on cleats, two battens across it to bed the rounds
    zs = 0.40
    for s in (-1, 1):
        bits.plank(m, P(r0 + 0.04, s * (half - 0.03), zs - 0.04), P(r1 - 0.02, s * (half - 0.03), zs - 0.04), 0.05, 0.04, "timber", Z, g.randrange(999))
    for k in range(3):
        s = -half + (k + 0.5) * (2 * half / 3)
        bits.plank(m, P(r0 + 0.02, s, zs), P(r1 - 0.02, s, zs), 2 * half / 3 * 0.96, 0.025, "planks", Z, g.randrange(999))
    for rr in (r0 + 0.2, r0 + 0.65):
        bits.plank(m, P(rr, -half + 0.03, zs + 0.025), P(rr, half - 0.03, zs + 0.025), 0.04, 0.03, "timber", Z, g.randrange(999))
    # the rounds: four on the shelf, three nested on them, bases out to the pit, noses in
    rr = 0.0576
    for k in range(4):
        s = (k - 1.5) * 0.135
        guns.shell(m, P(r0 + 0.03 + g.uniform(-0.02, 0.02), s, zs + 0.04 + rr), c, 0.09, 0.86)
    for k in range(3 if m.fine else 0):
        s = (k - 1.0) * 0.135
        guns.shell(m, P(r0 + 0.03 + g.uniform(-0.02, 0.02), s, zs + 0.04 + rr * 2.7), c, 0.09, 0.86)
    # boxes below
    for k, s in enumerate((-0.25, 0.25)):
        ammo_box(m, P(r0 + 0.48, s, 0.075), a, (0.88, 0.38, 0.15), g.randrange(999))
        if g.random() < 0.6:
            ammo_box(m, P(r0 + 0.48, s + g.uniform(-0.02, 0.02), 0.075 + 0.165), a + g.uniform(-3, 3), (0.88, 0.38, 0.15), g.randrange(999))


# ---- 2. the sandbag ring ---------------------------------------------------------------------------------------

def SandbagRing(fine=True, nation="_default", name="SandbagRing"):
    """A round sandbag emplacement for a light flak gun, a mortar or a searchlight: a wall two bags thick and seven
    courses high round a trodden floor, one entrance at the rear behind a short blast wall, earth banked low outside."""
    m = bits.model(name, "Sandbag ring (our own model)", "works", nation, fine, MORE)
    m.part("ring", None, (0, 0, 0), kind="root")
    m.use("ring")
    g = rnd(17)
    R = 2.1
    RB = R + 0.135
    GH = 0.45
    gh = math.degrees(math.asin(GH / RB))
    line = bits.arc(0, 0, RB, -180 + gh, 180 - gh, 60)

    def keep(t, c, f):
        # the outer row's lowest courses lie under the bank, but for a bag or two at each end of the wall
        return t == 0 or c >= 3 or f < 0.05 or f > 0.95
    bag_wall(m, line, 7, 0.0, seed=4, batter=0.012, thick=2, inward=-1.0, keep=keep, q=lambda t, c: (6, 10) if t == 0 else (5, 8), tie=0.3)
    # the wall's two ends, finished with bags laid across them
    for sgn in (-1, 1):
        a = 180 - gh + 2.0 if sgn > 0 else -180 + gh - 2.0
        for c in range(7):
            if c % 2 == 0:
                p = V2(a, RB + 0.13)
                bag(m, p + Z * (c * 0.116), a + g.uniform(-4, 4), seed=g.randrange(999), q=(6, 9), tie=0.0)
    # the bank, cut away at the entrance
    def cut(x, y):
        perp = abs(y)
        if x < -1.5:
            return smooth((perp - 0.62) / 0.5)
        return 1.0
    ring_bank(m, R + 0.5, [(0.0, 0.38), (0.1, 0.40), (0.25, 0.42), (0.55, 0.33), (0.9, 0.17), (1.2, 0.05), (1.4, 0.0)], seed=3, lump=0.04, step=2.0, cut=cut, turf=0.45)
    # the blast wall behind the entrance
    bx = -RB - 1.35
    bag_wall(m, [(bx, -1.05), (bx, 1.05)], 6, 0.0, seed=8, batter=0.012, thick=2, inward=1.0, q=lambda t, c: (6, 10) if t == 0 else (5, 8))
    bits.bank(m, [(bx - 0.5, -1.0), (bx - 0.5, 1.0)], [(-0.12, 0.0), (-0.05, 0.34), (0.25, 0.22), (0.55, 0.05), (0.7, 0.0)], "earth", seed=6, lump=0.03)
    # the floor: trodden earth, a few duckboards
    bits.patch(m, 0.0, 0.0, R - 0.02, "earth.dry", seed=2, n=48, lift=0.008, ragged=0.01)
    bits.patch(m, bx + 0.6, 0.0, 1.1, "earth.dry", seed=3, oval=1.3, lift=0.006)
    duckboard(m, (-RB - 0.9, 0.05), (-1.25, 0.0), 0.56, 0.0, seed=1)
    duckboard(m, V2(62, 1.65), V2(100, 1.65), 0.5, 0.0, seed=2)
    duckboard(m, V2(-62, 1.65), V2(-98, 1.65), 0.5, 0.0, seed=3)
    # what lies about: boxes by the wall, a bucket, a spade in the bank, a folded groundsheet
    ammo_box(m, V2(128, 1.72), 128 + 90, (0.62, 0.32, 0.2), 5, paint="upper")
    ammo_box(m, V2(132, 1.72) + Z * 0.205, 128 + 93, (0.62, 0.32, 0.2), 6, paint="upper")
    ammo_box(m, V2(-130, 1.7), -130 + 90, (0.62, 0.32, 0.2), 7, paint="upper", lid=True)
    bucket(m, V2(-150, 1.75), seed=4)
    spade(m, V2(150, R + 1.0) + Z * 0.28, (0.2, 0.1, 1.0))
    m.add([V2(40, 1.55) + Z * 0.012 + v for v in (Vector((-0.3, -0.2, 0)), Vector((0.3, -0.22, 0)), Vector((0.32, 0.2, 0.0)), Vector((-0.28, 0.22, 0)))], [(0, 1, 2, 3)], "canvas", True, lambda p: Z)
    m.box(V2(40, 1.55) + Z * 0.06, (0.5, 0.36, 0.09), "canvas", yaw=40)
    m.socket("gun", (0, 0, 0), (1, 0, 0), note="the gun, mortar or searchlight stands here facing the enemy. Floor 4.2 m across inside the wall, clear to 1.5 m radius; the wall stands 0.82 m high")
    m.socket("hit", (R + 0.5, 0.0, 0.5), (1, 0, 0), note="where a shell strikes the wall")
    bits.crew(m, "1", V2(70, 1.5), -V2(70), "kneel", "ring", "kneeling at the wall, left: the gunner's mate or the light's operator")
    bits.crew(m, "2", V2(-70, 1.5), -V2(-70), "kneel", "ring", "kneeling at the wall, right: ammunition, the boxes at his back")
    bits.crew(m, "3", V2(170, 1.55), (1, 0, 0), "stand", "ring", "standing by the entrance: the commander, looking over the gun")
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing empty.", "tracks": [], "events": []}
    weather(m)
    m.info["span"] = 6.4
    m.info["high"] = 0.45
    m.info["worn"] = [(0.0, 0.0, 2.6), (-3.6, 0.0, 1.4)]
    m.info["clear"] = [[tuple(V2(a, R + 1.8).to_2d()) for a in range(0, 360, 20)]]
    return m


def bough(m, pts, side, seed=0, wilt=0.0, rise=(-0.05, 0.3)):
    """A cut branch lying along `pts` (butt first): a woody stem, twigs to left and right of it in the spray's own
    plane (`side` lies in that plane, across the stem), sprigs off the twigs, and leaves of one shape along them all.
    `wilt` 0..1 is how far the leaves have dulled and drooped: cut boughs wilt in a day. `rise` is how far the twigs
    stand up out of the spray's plane (a bough lying down a slope keeps them low)."""
    g = rnd(seed)
    pts = [Vector(q) for q in pts]
    spans = [(b - a).length for a, b in zip(pts, pts[1:])]
    L = sum(spans)
    n = len(pts)
    r0 = 0.009 + 0.004 * L
    bits.tube(m, pts, r0, "timber.dark", sides=5 if m.fine else 3, radii=[r0 * (1.0 - 0.72 * i / (n - 1)) for i in range(n)])
    side = Vector(side)

    def at(f):
        d = f * L
        for (a, b), sp in zip(zip(pts, pts[1:]), spans):
            if d <= sp:
                return a.lerp(b, d / sp), (b - a).normalized()
            d -= sp
        return pts[-1].copy(), (pts[-1] - pts[-2]).normalized()

    def tone():
        w = wilt * g.uniform(0.6, 1.4)
        if w > 0.75:
            return g.choice(("leaf.dull", "leaf.dull", "leaf.dry"))
        if w > 0.4:
            return g.choice(("leaf.dull", "leaf", "leaf.dull"))
        return g.choice(("leaf", "leaf", "leaf", "leaf.2"))

    def leaf(c, u, up, scale=1.0):
        # one leaf, its stalk at c, lying along u, its upper face toward `up`: a pointed oval creased along its midrib
        hang = wilt * g.uniform(0.45, 0.95)
        u = (u * (1.0 - hang) - Z * (0.12 + hang)).normalized()
        up = up + Vector((g.uniform(-0.35, 0.35), g.uniform(-0.35, 0.35), g.uniform(-0.1, 0.2)))
        up = up - u * up.dot(u)
        if up.length < 1e-4:
            up = Z - u * Z.dot(u)
        up.normalize()
        v = u.cross(up)
        long = g.uniform(0.095, 0.118) * scale * (1.0 - 0.12 * wilt)
        w = long * 0.31 * (1.0 - 0.45 * hang)
        fold = w * (0.28 + 0.9 * hang)
        c = c + u * 0.012
        m.add([c, c + u * long * 0.32 + v * w + up * fold, c + u * long * 0.7 + v * w * 0.72 + up * fold * 0.8, c + u * long - up * w * 0.15,
               c + u * long * 0.7 - v * w * 0.72 + up * fold * 0.8, c + u * long * 0.32 - v * w + up * fold],
              [(0, 1, 2, 3), (0, 3, 4, 5)], tone(), False, lambda q, up=up: up)

    def clothe(a, b, up, start=0.18):
        # leaves by turns to left and right along a twig from a to b, and one at its end
        way = (b - a).normalized()
        across = up.cross(way).normalized()
        span = (b - a).length
        k = max(1, int(span * (1.0 - start) / 0.056))
        for j in range(k):
            t = start + (1.0 - start) * (j + 0.5) / k
            sg = 1.0 if j % 2 == 0 else -1.0
            leaf(a.lerp(b, t), (way * 0.62 + across * sg * 0.78).normalized(), up)
        leaf(b, way, up)

    twigs = max(4, int(L / 0.17)) if m.fine else max(3, int(L / 0.5))
    for i in range(twigs):
        f = 0.2 + 0.76 * (i + g.uniform(0.2, 0.8)) / twigs
        p, d = at(f)
        s = side - d * side.dot(d)
        s.normalize()
        up = s.cross(d)
        if up.z < 0.0:
            up = -up
        sg = 1.0 if i % 2 == 0 else -1.0
        ang = math.radians(g.uniform(34.0, 56.0))
        long = g.uniform(0.3, 0.52) * (1.12 - 0.62 * f) * min(1.25, 0.55 + L * 0.33)
        way = (d * math.cos(ang) + s * sg * math.sin(ang) + up * g.uniform(*rise)).normalized()
        sag = wilt * g.uniform(0.25, 0.6)
        tip = p + way * long - Z * long * (0.1 + sag)
        mid = p + way * long * 0.5 + Z * long * 0.04
        if not m.fine:
            # from far off: the twig's leaves as one blade
            v = up.cross(way).normalized() * long * 0.3
            m.add([p, mid + v, tip, mid - v], [(0, 1, 2, 3)], tone(), False, lambda q, up=up: up)
            continue
        bits.tube(m, [p, mid, tip], 0.0045, "timber.dark", sides=4, radii=[0.005, 0.0036, 0.002], caps=False)
        clothe(mid.lerp(p, 0.55), mid, up, 0.0)
        clothe(mid, tip, up, 0.0)
        if long > 0.26:
            for k, t in enumerate((0.38, 0.66) if long < 0.45 else (0.3, 0.5, 0.72)):
                q0 = p.lerp(mid, t * 2.0) if t < 0.5 else mid.lerp(tip, t * 2.0 - 1.0)
                sd = up.cross(way).normalized() * (1.0 if (k + i) % 2 == 0 else -1.0)
                q1 = q0 + (way * 0.7 + sd * 0.7 + up * g.uniform(-0.1, 0.25)).normalized() * g.uniform(0.12, 0.2) - Z * 0.02 * (1.0 + 3.0 * sag)
                bits.tube(m, [q0, q1], 0.003, "timber.dark", sides=3, radii=[0.003, 0.0018], caps=False)
                clothe(q0, q1, up, 0.25)
    # the leader: the stem's own last third carries leaves too
    if m.fine:
        p0, d0 = at(0.7)
        up = (side - d0 * side.dot(d0)).cross(d0)
        if up.z < 0.0:
            up = -up
        up.normalize()
        for k in range(3):
            a, _ = at(0.7 + 0.1 * k)
            b, _ = at(0.8 + 0.1 * k)
            clothe(a, b, up, 0.0)


# ---- 3. the gun pit ---------------------------------------------------------------------------------------------

def GunPit(fine=True, nation="_default", name="GunPit", low_front=False):
    """A pit for a towed field gun: a horseshoe sunk 0.4 m, the spoil thrown up as a parapet in front and at the
    sides, its inside face revetted with bags; a ramp out at the back, logs for the trail spades, a slit for the crew
    at each rear corner, an ammunition niche, the ground in front cleared and scorched. `low_front` makes it an
    anti-tank gun's: such a gun fires flat, its bore under a metre up, so the parapet is cut down across its front
    and the bags stepped down to it, and cut boughs are laid over the parapet to hide the position."""
    m = bits.model(name, "Anti-tank gun pit (our own model)" if low_front else "Gun pit (our own model)", "works", nation, fine, MORE)
    m.part("pit", None, (0, 0, 0), kind="root")
    m.use("pit")
    g = rnd(23)
    D = 0.4
    R = 2.4                       # inside the bags
    RO = 2.76                     # the hole's edge at ground level
    ARM = 118.0
    poly = [tuple(V2(a, RO).to_2d()) for a in range(-int(ARM), int(ARM) + 1, 4)] + [(-3.0, 2.15), (-4.35, 1.85), (-4.35, -1.85), (-3.0, -2.15)]

    def floor_z(x, y):
        return -D * (1.0 - smooth((-2.85 - x) / 1.45))

    def batter(a):
        aa = abs((a + 180) % 360 - 180)
        return 0.08 + 0.22 * smooth((aa - ARM) / 10.0)
    dig = [dug_bowl(m, lambda a: ray_poly(poly, a), floor_z, batter, 3.0, seed=4)]
    # the bags up the inside face, with a niche for the rounds on the left
    NA, NH = 92.0, 0.5
    nh = math.degrees(math.asin(NH / (R + 0.135)))

    NOTCH = 22.0                  # (half the width of an anti-tank gun's low front, in degrees)
    COURSE = bits.BAG[2] * 0.93
    STEP = math.degrees(bits.BAG[0] / (R + 0.135))      # (a bag's length round the wall)

    def courses_at(a):
        return 8 if not low_front else min(8, 5 + max(0, int(math.ceil((abs(a) - NOTCH) / STEP))))

    def keep(t, c, f):
        a = -ARM + 2 * ARM * f
        if low_front and c >= courses_at(a):
            return False             # (five courses across the front; at its shoulders a bag's length to each step up)
        return not (abs(a - NA) < nh + 1.5 and c < 4)
    bag_wall(m, bits.arc(0, 0, R + 0.135, -ARM, ARM, 50), 8, -D, seed=7, batter=0.016, thick=1, inward=-1.0, keep=keep, q=(6, 10), tie=0.3)
    # the parapet: the spoil, in front and at the sides
    def taper(x, y):
        a = abs(math.degrees(math.atan2(y, x)))
        k = smooth((ARM + 10 - a) / 16.0)
        if low_front:
            # (the earth stands as high as the bags in front of it, a little ahead of each step: no slot behind a step)
            k *= 0.3 + 0.7 * smooth((a - NOTCH + 6.0) / (2.6 * STEP))
        return k
    PROF = [(0.0, 0.47), (0.2, 0.53), (0.5, 0.5), (0.9, 0.33), (1.3, 0.13), (1.6, 0.03), (1.8, 0.0)]
    # (behind a low wall the earth's inner edge would show the gap it leaves behind the bags: there it runs in under them)
    ring_bank(m, RO - 0.02, ([(-0.12, 0.40)] if low_front else []) + PROF, seed=9, lump=0.04, step=2.0, cut=taper, turf=0.4, a0=-ARM - 12, a1=ARM + 12)
    niche(m, NA, R + 0.01, -D, 0.07, NH, 0.85, seed=3)
    # two logs at the back for the trail spades, each pinned by stakes
    for sy in (-1, 1):
        a, b = Vector((-2.75, sy * 0.35, -D + 0.07)), Vector((-2.8, sy * 1.35, -D + 0.07))
        bits.log(m, a, b, 0.12, "timber", 5 + sy)
        for t in (0.15, 0.85):
            p = a.lerp(b, t) + Vector((-0.17, 0, -0.1))
            bits.log(m, p, p + Vector((0.02, 0, 0.32)), 0.035, "timber", 9 + sy, sides=6)
        bits.patch(m, -2.6, sy * 0.85, 0.55, "earth", seed=7 + sy, oval=0.7, lift=-D + 0.01)
    # a slit for the crew at each rear corner
    for sy in (-1, 1):
        y0, y1 = (2.75, 3.5) if sy > 0 else (-3.5, -2.75)
        dig.append(slit(m, (-2.75, (y0 + y1) / 2), (-4.75, (y0 + y1) / 2), y1 - y0, 1.1, seed=20 + sy, cover=0.45))
        bits.bank(m, [(-2.6, sy * 3.95), (-4.9, sy * 3.95)], [(-0.55, 0.0), (-0.3, 0.2), (0.0, 0.3), (0.3, 0.15), (0.55, 0.0)], "earth", seed=12 + sy, lump=0.04)
        m.socket(f"shelter.{1 if sy > 0 else 2}", (-3.75, (y0 + y1) / 2, -1.1), (1, 0, 0), note="the floor of a crew slit: two men crouch here under fire")
    # in front: brush cut down and the ground scorched by the muzzle blast
    bits.patch(m, 5.6, 0.0, 2.9, "earth.dry", seed=21, oval=1.25, lift=0.006, ragged=0.3)
    bits.patch(m, 4.6, 0.0, 1.5, "earth", seed=22, oval=1.6, lift=0.009, ragged=0.35)
    for k in range(9 if fine else 4):
        p = Vector((g.uniform(4.0, 8.5), g.uniform(-3.2, 3.2), 0.0))
        bits.log(m, p - Z * 0.05, p + Z * g.uniform(0.06, 0.16), g.uniform(0.03, 0.06), "timber", g.randrange(999), sides=7)
    for k in range(5 if fine else 2):
        p = Vector((g.uniform(3.8, 7.5), g.uniform(-3.4, 3.4), 0.02))
        ya = g.uniform(0, 6.28)
        q = p + Vector((math.cos(ya), math.sin(ya), 0)) * g.uniform(0.5, 1.0)
        bits.log(m, p, q + Z * 0.02, 0.018, "timber.dark", g.randrange(999), sides=5, taper=0.5)
    # empties and a box by the niche, a cleaning staff on the parapet
    if fine:
        for k in range(7):
            p = Vector((g.uniform(-1.6, -0.6), g.uniform(-1.6, -0.6), -D + 0.04))
            ya = g.uniform(0, 6.28)
            case(m, p, Vector((math.cos(ya), math.sin(ya), 0.0)), 0.044, 0.42, 8)
    ammo_box(m, V2(115, 1.95) + Z * -D, 115 + 90, (0.72, 0.3, 0.16), 4, lid=True, rounds=3, cal=0.075, long=0.6)
    m.rod(V2(-60, 3.1) + Z * 0.5, V2(-25, 3.2) + Z * 0.55, 0.02, "timber", sides=8)
    if low_front:
        # cut boughs hide the gun: laid over the low front, butts back and tips toward the enemy, a few over the
        # parapet's shoulders, and (a part of their own) leaned from the gun's shield down to the low front
        gb = rnd(61)

        def top_at(x, y):
            r, a = math.hypot(x, y), math.degrees(math.atan2(y, x))
            if r < R:
                return floor_z(x, y)
            if r < RO - 0.02:
                return -D + courses_at(a) * COURSE + 0.02
            off = r - (RO - 0.02)
            for (o0, h0), (o1, h1) in zip(PROF, PROF[1:]):
                if off <= o1:
                    return (h0 + (h1 - h0) * (off - o0) / (o1 - o0)) * taper(x, y)
            return 0.0

        def lay(butt, tip, lift0, lift1, n=7, give=0.015):
            # a stiff branch resting on what lies under it: it bridges the hollows, it does not follow them (`give` is
            # how far it bends over a crest between one point and the next)
            pts = []
            for i in range(n):
                t = i / (n - 1)
                q = butt.lerp(tip, t)
                pts.append(Vector((q.x, q.y, top_at(q.x, q.y) + lift0 + (lift1 - lift0) * t)))
            for _ in range(6):
                for i in range(1, n - 1):
                    pts[i].z = max(pts[i].z, (pts[i - 1].z + pts[i + 1].z) / 2 - give)
            return pts

        def laid(a, r, a2, long, roll, wilt, lift=(0.035, 0.2), give=0.015, rise=(-0.05, 0.3)):
            butt = V2(a, r)
            way = V2(a2)
            pts = lay(butt, butt + way * long, lift[0], lift[1], give=give)
            side = Vector((-way.y, way.x, 0.0)) * math.cos(math.radians(roll)) + Z * math.sin(math.radians(roll))
            bough(m, pts, side, gb.randrange(1 << 30), wilt, rise)

        def wilted():
            return gb.choice((0.0, 0.0, 0.0, 0.15, 0.55, 0.85, 1.0))
        if fine:
            for k in range(14):                       # over the low front
                a = -NOTCH - 3.0 + (2 * NOTCH + 6.0) * (k + gb.uniform(0.15, 0.85)) / 14
                flat = abs(a) < 8.0                    # (under the barrel they lie flat)
                laid(a, gb.uniform(2.44, 2.62), a + gb.uniform(-26, 26), gb.uniform(1.35, 2.0), gb.uniform(-8, 8) if flat else gb.uniform(-26, 26), wilted(),
                     (0.035, gb.uniform(0.08, 0.16) if flat else gb.uniform(0.12, 0.32)))
            for sy in (-1, 1):                        # over the shoulders, where the wall steps up: bent over the crest
                for k in range(4):                    # and lying down the outer slope, their twigs low (stood up, they
                    a = sy * (NOTCH + 6.0 + 44.0 * (k + gb.uniform(0.1, 0.9)) / 4)          # read as saplings planted there)
                    laid(a, gb.uniform(2.66, 2.8), a * gb.uniform(0.4, 0.8), gb.uniform(1.5, 2.1), gb.uniform(-7, 7), wilted(), (0.04, gb.uniform(0.05, 0.1)), 0.06, (-0.12, 0.05))
        # leaned on the gun: butts hooked over the shield's top edge, tips down on the low front either side of the barrel
        SX, SZ = 0.05, 0.8 + 0.015                    # (the Pak 40's shield top, 1.2 m above the pit floor)
        m.part("boughs", "pit", (SX, 0.0, SZ), kind="free", note="cut boughs leaned from the gun's shield down to the low front, and one laid across her barrel: loose, the game takes them off when the pit stands without its gun, and may let them fall at her first shots. Fitted to the Pak40 (shield top 1.20 m above the pit floor, 0.05 m ahead of the socket); for the AntiTankGun move the part 0.15 m forward and 0.06 m down")
        m.use("boughs")
        for k, (yb, yt, xt) in enumerate(((-0.52, -1.25, 2.5), (-0.37, -0.82, 2.75), (-0.21, -0.42, 2.9), (0.2, 0.47, 2.85), (0.35, 0.86, 2.7), (0.5, 1.3, 2.45))):
            rest = Vector((SX, yb, SZ))
            tip = Vector((xt + gb.uniform(-0.1, 0.1), yt + gb.uniform(-0.08, 0.08), 0.0))
            tip.z = top_at(tip.x, tip.y) + gb.uniform(0.2, 0.3)
            way = (tip - rest).normalized()
            # (the butt is tucked down behind the shield's top edge: a row of butts standing proud of it is a crown of sticks)
            pts = [rest + Vector((-0.05, 0.0, -0.07)), rest, rest.lerp(tip, 0.35) - Z * 0.015, rest.lerp(tip, 0.7) - Z * 0.03, tip]
            roll = math.radians(gb.uniform(-16, 16))
            side = Vector((-way.y, way.x, 0.0)).normalized() * math.cos(roll) + Z * math.sin(roll)
            bough(m, pts, side, gb.randrange(1 << 30), (0.0, 0.7, 0.1, 0.0, 0.9, 0.3)[k])
        # (one across the barrel: it rests on the shield and on the barrel's top, 1.04 m above the floor)
        rest, over = Vector((SX, -0.3, SZ + 0.01)), Vector((1.25, 0.0, 1.04 - D + 0.012))
        way = (over - rest).normalized()
        pts = [rest + Vector((-0.05, 0.0, -0.07)), rest, over, over + way * 0.7 - Z * 0.03, over + way * 1.3 - Z * 0.1]
        bough(m, pts, Vector((-way.y, way.x, 0.0)).normalized(), gb.randrange(1 << 30), 0.2)
        # (two sprigs hung over the shield's top edge, their leaves down its face: they break its straight line)
        for yb, lean in ((-0.4, 0.12), (0.31, -0.1)):
            top = Vector((SX, yb, SZ + 0.004))
            pts = [top + Vector((-0.16, -lean * 0.4, -0.12)), top, top + Vector((0.13, lean * 0.5, -0.17)), top + Vector((0.27, lean, -0.4)), top + Vector((0.36, lean * 1.3, -0.56))]
            bough(m, pts, Y, gb.randrange(1 << 30), 0.25 if yb < 0 else 0.0, (-0.02, 0.22))
        m.use("pit")
    if low_front:
        m.socket("gun", (0.0, 0.0, -D), (1, 0, 0), note="the anti-tank gun stands on the pit floor's middle facing the enemy. Room: 4.8 m across inside the bags; across the front 44 degrees the bags stand five courses (0.56 m above the floor) and the earth beyond is lower still, so a bore 0.8 m up or more fires flat over it and traverses 20 degrees either way; at the sides the parapet top is 0.9 m above the floor. Two logs lie 2.8 m behind, 0.7 m apart: split trails (up to 4.0 m wide, 3.1 m back) pass outside their ends into the open rear. Of the vehicle modeller's guns: Pak40 (bore 0.96 m up) and AntiTankGun (0.86 m) both have room")
    else:
        m.socket("gun", (0.0, 0.0, -D), (1, 0, 0), note="the gun stands on the pit floor's middle facing the enemy. Room: 4.8 m across inside the bags, the parapet top 0.9 m above the floor; two logs lie 2.8 m behind, 0.7 m apart: a single trail's spade (Gun75, 25pdr) beds between them, a split trail's legs (ZiS3, leFH18, Howitzer105, up to 3.9 m wide and 3.55 m back) pass outside their ends into the open rear. All five of the vehicle modeller's towed field guns have room. An anti-tank gun fires flat with its bore at the height of this parapet (Pak40 0.96 m, AntiTankGun 0.86 m): stand those in the AntiTankPit, whose front is cut down for them")
    m.socket("hit", (3.3, 0.0, 0.4), (1, 0, 0), note="where a shell strikes the parapet")
    m.socket("ground", (4.0, 0.0, 0.05), (1, 0, 0), note="where the muzzle blast lifts dust off the scorched ground")
    bits.crew(m, "ammo", V2(NA, R - 0.4) + Z * -D, V2(NA), "kneel", "pit", "kneeling at the ammunition niche, passing rounds to the loader")
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing empty.", "tracks": [], "events": []}
    weather(m, soot_at=[((4.6, 0.0, 0.0), 2.4, 0.55), ((2.9, 0.0, 0.5), 1.0, 0.35)])
    m.info["span"] = 9.5
    m.info["high"] = 0.2
    m.info["dig"] = dig
    m.info["dig_depth"] = D
    m.info["worn"] = [(-0.5, 0.0, 4.0), (5.6, 0.0, 3.0, 0.7)]
    m.info["clear"] = [[tuple(V2(a, 4.2).to_2d()) for a in range(0, 360, 20)]]
    return m


def niche(m, a, rface, z0, top, half, depth, seed=0, fill=None):
    """A niche let into a wall at floor level for rounds: timber lined, a lintel, boxes inside."""
    g = rnd(seed)
    c, t = V2(a), V2(a + 90)

    def P(r, s, z):
        return c * r + t * s + Z * z
    r0, r1 = rface, rface + depth
    H = top - z0
    bits.plank(m, P(r0 + 0.05, -half - 0.15, top + 0.05), P(r0 + 0.05, half + 0.15, top + 0.05), 0.1, 0.1, "timber", -c, g.randrange(999))
    for k in range(max(1, int(H / 0.145))):
        z = z0 + 0.075 + k * 0.145
        for s in (-1, 1):
            bits.plank(m, P(r0 + 0.1, s * (half + 0.01), z), P(r1, s * (half + 0.01), z), 0.14, 0.03, "planks", t * -s, g.randrange(999))
        bits.plank(m, P(r1, -half, z), P(r1, half, z), 0.14, 0.03, "planks", -c, g.randrange(999))
    for k in range(5):
        s = -half + (k + 0.5) * (2 * half / 5)
        bits.plank(m, P(r0 + 0.1, s, top - 0.002), P(r1 + 0.02, s, top - 0.002), 2 * half / 5 * 0.95, 0.03, "planks", Z, g.randrange(999))
        bits.plank(m, P(r0 + 0.1, s, z0 + 0.02), P(r1, s, z0 + 0.02), 2 * half / 5 * 0.93, 0.022, "planks", Z, g.randrange(999))
    if fill:
        fill(P, g)
        return
    for k, s in enumerate((-0.2, 0.2)):
        ammo_box(m, P(r0 + 0.5, s, z0 + 0.035), a, (0.72, 0.3, 0.15), g.randrange(999))
        ammo_box(m, P(r0 + 0.5, s, z0 + 0.19), a + g.uniform(-4, 4), (0.72, 0.3, 0.15), g.randrange(999))


# ---- 4. the mortar and its pit ---------------------------------------------------------------------------------

ELEV = 62.0


def bomb(m, base, axis=Z, paint="upper2", seed=0):
    """A finned teardrop mortar bomb of about 81 mm, its tail at `base`: the boom with its fins and a ring of
    charges, the body, a copper band, the fuze."""
    base, axis = Vector(base), Vector(axis).normalized()
    sides = 14 if m.fine else 6
    side = axis.cross(Z if abs(axis.z) < 0.9 else X).normalized()
    up = axis.cross(side)
    m.lathe(base, axis, [(0.0, 0.011), (0.17, 0.012)], "steel", 8 if m.fine else 4, (True, False))
    for k in range(6 if m.fine else 3):
        a = 2 * math.pi * k / (6 if m.fine else 3)
        u = side * math.cos(a) + up * math.sin(a)
        bits.plate(m, [(0.008, 0.0), (0.038, 0.0), (0.038, 0.075), (0.012, 0.11)], 0.003, "steel", base, u, axis)
    if m.fine:
        bits.tube(m, [base + (side * math.cos(2 * math.pi * k / 12) + up * math.sin(2 * math.pi * k / 12)) * 0.038 + axis * 0.004 for k in range(12)], 0.004, "steel", 4, closed=True)
        m.lathe(base + axis * 0.115, axis, [(0.0, 0.022), (0.04, 0.022)], "canvas", 10, (True, True))         # the charges
    m.lathe(base + axis * 0.16, axis, [(0.0, 0.013), (0.03, 0.03), (0.08, 0.0395), (0.14, 0.0405), (0.20, 0.037), (0.25, 0.029), (0.285, 0.019)], paint, sides, (True, False))
    m.lathe(base + axis * 0.295, axis, [(0.0, 0.0412), (0.012, 0.0412)], "brass", sides, (False, False))
    m.lathe(base + axis * 0.445, axis, [(0.0, 0.019), (0.012, 0.019), (0.035, 0.011), (0.045, 0.004)], "brass", 10 if m.fine else 5, (True, True))


def bomb_box(m, at, yaw=0.0, n=6, seed=0, lid=True):
    """An open wooden box with bombs standing in it nose up between partitions."""
    g = rnd(seed)
    at = Vector(at)
    rot = Matrix.Rotation(math.radians(yaw), 3, "Z")

    def P(x, y, z):
        return at + rot @ Vector((x, y, z))
    L, W, H, th = 0.42, 0.29, 0.28, 0.016
    m.box(P(0, 0, th / 2), (L, W, th), "wood.paint", yaw)
    for sy in (-1, 1):
        m.box(P(0, sy * (W / 2 - th / 2), H / 2), (L, th, H), "wood.paint", yaw)
    for sx in (-1, 1):
        m.box(P(sx * (L / 2 - th / 2), 0, H / 2), (th, W - 2 * th, H), "wood.paint", yaw)
        if m.fine:
            bits.tube(m, [P(sx * (L / 2 + 0.004), -0.05, H * 0.75), P(sx * (L / 2 + 0.03), -0.03, H * 0.62), P(sx * (L / 2 + 0.03), 0.03, H * 0.62), P(sx * (L / 2 + 0.004), 0.05, H * 0.75)], 0.007, "rope", 5)
    m.box(P(0, 0, H * 0.55), (L - 0.03, 0.008, H * 0.9), "planks", yaw)
    for x in (-0.068, 0.068):
        m.box(P(x, 0, H * 0.55), (0.008, W - 0.03, H * 0.9), "planks", yaw)
    # (the row on the box's -y side is the last to be emptied, and the other empties from its +x end: the bomb handler
    # kneels on the +y side and lifts from that end, so his forearm passes over empty places, not standing fuzes)
    for ix, iy in ((-1, -1), (0, -1), (1, -1), (-1, 1), (0, 1), (1, 1))[:n]:
        bomb(m, P(ix * 0.135, iy * 0.07, th) + Vector((g.uniform(-0.004, 0.004), g.uniform(-0.004, 0.004), 0)), Vector((g.uniform(-0.03, 0.03), g.uniform(-0.03, 0.03), 1)))
    if lid:
        m.box(P(0, -W / 2 - 0.06, H * 0.5), (L, 0.016, W), "wood.paint", yaw, 0.0, -68.0)


def mortar(m, parent, at=(0.0, 0.0, 0.0)):
    """The mortar: a ribbed baseplate with its spade, the smooth tube of 1.2 m laid at 62 degrees toward +x, the
    bipod in front with its elevating and traversing screws, a cross-level and the sight on the left. Parts:
    `baseplate` (fixed) and `tube` (with the bipod: slides down the bore for the jolt)."""
    fine = m.fine
    O = Vector(at)
    el = math.radians(ELEV)
    d = Vector((math.cos(el), 0.0, math.sin(el)))
    nrm = Vector((math.sin(el), 0.0, -math.cos(el)))
    B = O + Vector((0.0, 0.0, 0.085))
    sides = 20 if fine else 8
    # ---- the baseplate
    m.part("baseplate", parent, O, kind="fixed")
    m.use("baseplate")
    plan = [(O.x + x, O.y + y) for x, y in m.round_rect(-0.31, 0.21, -0.25, 0.25, 0.11, 4 if fine else 2)]
    m.prism(plan, O.z - 0.004, O.z + 0.022, "upper", bottom=True)
    bits.tube(m, [Vector((x, y, O.z + 0.022)) for x, y in plan], 0.009, "upper", 6 if fine else 3, closed=True)
    rel = [(x - O.x, y - O.y) for x, y in plan]
    for k in range(10):
        a = 360.0 * k / 10 + 18
        u = V2(a)
        r = ray_poly(rel, a) - 0.02
        bits.plate(m, [(0.075, 0.0), (r, 0.0), (r, 0.012), (0.075, 0.055)], 0.013, "upper", O + Z * 0.022, u, Z)
    m.lathe(O + Z * 0.02, Z, [(0.0, 0.085), (0.03, 0.085), (0.05, 0.07), (0.075, 0.062), (0.08, 0.05)], "upper", sides, (False, True))   # the socket
    m.rod(O + Vector((0.0, -0.07, 0.07)), O + Vector((-0.02, -0.16, 0.10)), 0.008, "steel", sides=6)                                          # its locking lever
    m.ball(O + Vector((-0.02, -0.165, 0.102)), 0.014, "steel", 8, 6)
    for sy in (-1, 1):       # the carrying handles
        bits.tube(m, [O + Vector((-0.18, sy * 0.22, 0.022)), O + Vector((-0.15, sy * 0.24, 0.06)), O + Vector((-0.03, sy * 0.24, 0.06)), O + Vector((0.0, sy * 0.22, 0.022))], 0.009, "steel", 6 if fine else 3)
    # the spade: a blade under the plate's front edge, bitten into the ground
    bits.plate(m, [(-0.2, 0.0), (0.2, 0.0), (0.15, -0.13), (-0.15, -0.13)], 0.012, "upper", O + Vector((0.12, 0, 0.0)), Y, Vector((0.25, 0, 1)))
    if fine:
        bits.bolt_ring(m, O + Z * 0.025, Z, 0.11, 8, 0.008, 0.006, "steel", 0.5)
    # ---- the tube and the bipod
    m.part("tube", "baseplate", B, slide=(-d.x, -d.y, -d.z), slide_limits=[0.0, 0.04], kind="slide",
           note="laid at 62 degrees (the game may turn the whole mortar for bearing); plus jolts the tube and bipod back down the bore")
    m.use("tube")
    m.ball(B, 0.05, "upper", 14 if fine else 6, 8 if fine else 4)
    m.lathe(B, d, [(0.0, 0.05), (0.06, 0.052), (0.085, 0.047), (1.165, 0.047), (1.175, 0.052), (1.2, 0.052), (1.2, 0.041)], "upper", sides, (False, False))
    m.lathe(B + d * 1.2, -d, [(0.0, 0.041), (0.25, 0.041)], "void", 16 if fine else 6, (False, True))
    C = B + d * 0.80
    m.lathe(C - d * 0.04, d, [(0.0, 0.058), (0.08, 0.058)], "upper", 16 if fine else 6)                    # the clamp collar
    m.rod(C + Y * 0.06 + d * 0.0, C + Y * 0.06 - nrm * 0.07, 0.008, "steel", sides=6)
    m.ball(C + Y * 0.06 - nrm * 0.075, 0.013, "timber.dark", 8, 6)
    Yc = C + nrm * 0.10
    for sy in (-1, 1):       # the two recoil cylinders from the collar to the yoke
        a0 = C + Y * sy * 0.075 + nrm * 0.03 - d * 0.10
        m.lathe(a0, d, [(0.0, 0.018), (0.02, 0.023), (0.16, 0.023), (0.18, 0.016)], "upper", 12 if fine else 5)
        m.rod(a0 + d * 0.06, Yc + Y * sy * 0.075, 0.012, "upper", sides=6)
    m.rod(Yc - Y * 0.14, Yc + Y * 0.14, 0.022, "upper", sides=12 if fine else 5)                          # the traversing screw's housing
    m.rod(Yc - Y * 0.18, Yc - Y * 0.14, 0.009, "steel.worn", sides=6)
    bits.handwheel(m, Yc - Y * 0.185, -Y, 0.045, "steel", 3, 0.006, True)
    J = O + Vector((0.53, 0.0, 0.34))
    m.rod(J + Z * 0.02, Yc - nrm * 0.0 + Vector((0, 0, -0.02)), 0.026, "upper", sides=12 if fine else 5)  # the elevating screw's housing
    m.lathe(J - Z * 0.03, Z, [(0.0, 0.032), (0.07, 0.032)], "upper", 12 if fine else 5)
    m.rod(J + Vector((0, -0.03, -0.0)), J + Vector((0, -0.12, 0.0)), 0.008, "steel", sides=6)              # its crank
    m.rod(J + Vector((0, -0.12, 0.0)), J + Vector((0.0, -0.12, -0.07)), 0.008, "steel", sides=6)
    m.rod(J + Vector((0, -0.12, -0.07)), J + Vector((0.0, -0.17, -0.07)), 0.012, "timber.dark", sides=6)
    feet = []
    for sy in (-1, 1):
        F = O + Vector((0.70, sy * 0.38, 0.02))
        m.rod(J + Y * sy * 0.03, F + Z * 0.02, 0.017, "upper", 0.014, sides=10 if fine else 4)
        m.lathe(F - Z * 0.02, Z, [(0.0, 0.004), (0.03, 0.03), (0.045, 0.03), (0.05, 0.016)], "steel", 8)    # the foot
        feet.append(F)
    # the cross-level between the legs, an adjusting sleeve and its level
    la, lb = J.lerp(feet[1] + Z * 0.02, 0.42), J.lerp(feet[0] + Z * 0.02, 0.42)
    m.rod(la, lb, 0.009, "steel", sides=6)
    m.lathe(la.lerp(lb, 0.3), lb - la, [(0.0, 0.016), (0.09, 0.016)], "upper", 10)
    m.lathe(la.lerp(lb, 0.3) + Z * 0.02 + (lb - la).normalized() * 0.02, lb - la, [(0.0, 0.008), (0.05, 0.008)], "glass", 8)
    # the sight, on the left of the yoke
    S = Yc + Y * 0.16 + Z * 0.03
    m.rod(Yc + Y * 0.14, S, 0.012, "upper", sides=8)
    m.box(S + Z * 0.05, (0.07, 0.05, 0.09), "upper")
    m.lathe(S + Vector((-0.02, 0.0, 0.10)), Z, [(0.0, 0.017), (0.09, 0.017)], "upper", 12 if fine else 5)
    m.lathe(S + Vector((-0.02, 0.0, 0.18)), -X, [(0.0, 0.018), (0.07, 0.018), (0.08, 0.022), (0.10, 0.022)], "upper", 12 if fine else 5, (True, False))
    m.disc(S + Vector((-0.121, 0.0, 0.18)), -X, 0.017, "glass", 10)
    m.disc(S + Vector((0.008, 0.0, 0.18)), X, 0.014, "glass", 10)
    m.lathe(S + Vector((0.03, 0.015, 0.098)), X, [(0.0, 0.006), (0.04, 0.006)], "glass", 6)
    m.lathe(S + Vector((-0.01, 0.026, 0.07)), Y, [(0.0, 0.006), (0.03, 0.006)], "glass", 6)
    m.socket("muzzle", B + d * 1.22, d, part="tube", note="where the bomb leaves")
    m.socket("sight", S + Vector((-0.13, 0.0, 0.18)), X, part="tube", note="the sight's eyepiece")
    return {"B": B, "d": d, "muzzle": B + d * 1.2, "sight": S}


def mortar_fire_clip():
    return {"long": 0.8, "loop": False, "after": "hold",
            "about": "One bomb: the tube and bipod jolt 2 cm down the bore and come back, the baseplate presses 1 cm into the ground and springs back.",
            "tracks": [{"part": "tube", "slide": [[0.0, 0.0], [0.03, 0.02], [0.12, 0.006], [0.3, 0.0]]},
                       {"part": "baseplate", "move": [[0.0, 0, 0, 0], [0.035, 0, 0, -0.01], [0.25, 0, 0, -0.004], [0.6, 0, 0, 0.0]]}],
            "events": [{"t": 0.0, "fx": "muzzle_flash", "socket": "muzzle", "cue": "mortar_fire"},
                       {"t": 0.02, "fx": "muzzle_smoke", "socket": "muzzle", "cue": ""}]}


def Mortar(fine=True, nation="_default", name="Mortar"):
    m = bits.model(name, "Medium mortar of about 81 mm (our own design)", "works", nation, fine, MORE)
    m.part("mortar", None, (0, 0, 0), kind="root")
    mt = mortar(m, "mortar", (0, 0, 0))
    m.use("mortar")
    # (the box stands 3 cm further to number three's right than it first did, and the two places nearest his hands
    # are the empty ones: lifting the next bomb out, his right forearm came down 1.9 cm onto the nose of a bomb
    # standing in the near row's middle place)
    bomb_box(m, (-0.544, -0.579, 0.0), 20.0, 4, seed=3)
    bomb(m, Vector((-0.2, -0.75, 0.042)), Vector((1.0, 0.25, 0.0)))
    bits.crew(m, "number1", (0.05, 0.62, 0.0), (0.35, -1, 0), "kneel", "mortar", "number one kneeling on the left, his eye at the sight (socket `sight`)")
    bits.crew(m, "number2", (0.55, -0.55, 0.0), (-0.25, 1, 0), "kneel", "mortar", "number two kneeling on the right front, his hands at the muzzle to drop the bomb",
              grips={"l": (mt["muzzle"] + Vector((-0.02, -0.06, 0.05)), (0, 1, 0), "tube"), "r": (mt["muzzle"] + Vector((0.03, -0.06, -0.04)), (0, 1, 0), "tube")})
    bits.crew(m, "number3", (-0.75, -0.3, 0.0), (1, 0.2, 0), "kneel", "mortar", "number three kneeling behind at the bomb box, passing bombs")
    m.socket("hit", (0.2, 0.0, 0.3), (1, 0, 0), note="where a hit strikes")
    m.clips["fire"] = mortar_fire_clip()
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Laid and still.", "tracks": [], "events": []}
    weather(m, [(mt["muzzle"], 0.12, 0.8)], splash=0.15, rust=0.3)
    m.info["span"] = 1.9
    m.info["high"] = 0.45
    m.info["worn"] = [(0.0, 0.0, 1.2)]
    return m


def MortarPit(fine=True, nation="_default", name="MortarPit"):
    """A round pit 2.6 m across sunk 0.6 m, its wall revetted with poles on stakes, a collar of bags round the rim, steps
    in at the back, a niche of bombs; two aiming posts out in front; the mortar in it. (5.5 cm wider all round than it
    was first dug: number two, kneeling at the right front, sits back against the wall as he ducks from the muzzle,
    and his seat and the toe of his right boot were 2.4 and 1.6 cm into its poles.)"""
    m = bits.model(name, "Mortar pit (our own model)", "works", nation, fine, MORE)
    m.part("pit", None, (0, 0, 0), kind="root")
    m.use("pit")
    g = rnd(31)
    D = 0.6
    RO, RF = 1.385, 1.325
    STEP_HALF = 0.42
    NA = -118.0

    def adiff(a, b):
        return abs((a - b + 180.0) % 360.0 - 180.0)

    def outline(a):
        return RO + 0.85 * smooth((22.0 - adiff(a, 180.0)) / 7.0)

    def floor_z(x, y):
        if x < -0.95:
            return -D * (1.0 - min(1.0, (-0.95 - x) / 1.25))
        return -D

    dig = [dug_bowl(m, outline, floor_z, lambda a: RO - RF + 0.25 * smooth((22.0 - adiff(a, 180.0)) / 7.0), 3.0, seed=6, trodden=True)]
    # the wall's revetment: stakes and level poles, broken at the steps and at the niche
    gaps = [(180.0, 21.0), (NA, 16.0)]
    spans = []
    a = -180.0 + 21.0
    # (no stake stands from bearing -66 to -42, where number two sits back against the wall and his right boot's toe
    # lies: the stakes either side are set a little wider, at -70 and -38)
    stake_angles = ([x for x in range(-180, 180, 24) if all(adiff(x, c) > h + 3 for c, h in gaps) and not -90 < x < -30] + [-70, -38]
                    + [c + s * h for c, h in gaps for s in (-1, 1)])
    for k, sa in enumerate(stake_angles):
        p = V2(sa, RF - 0.045)
        bits.log(m, p - Z * (D + 0.1), p + Z * 0.05, 0.03, "timber", 300 + k, sides=8, taper=0.9)
    for a0, a1 in ((-180 + 21, NA - 16), (NA + 16, 180 - 21)):
        for k in range(5 if fine else 2):
            z = -D + 0.05 + k * (0.13 if fine else 0.3)
            pts = [V2(a0 + (a1 - a0) * i / 24, RF - 0.005 + 0.004 * math.sin(i * 1.7 + k)) + Z * (z + 0.006 * math.sin(i * 0.9 + k * 2)) for i in range(25)]
            bits.tube(m, pts, 0.032, "timber", 7 if fine else 4)
    # the steps: two timber-edged treads in the ramp
    for x in (-1.25, -1.6):
        z = floor_z(x, 0)
        bits.plank(m, (x, -STEP_HALF, z + 0.02), (x, STEP_HALF, z + 0.02), 0.08, 0.06, "timber", Vector((0.4, 0, 1)), g.randrange(999))
        for sy in (-1, 1):
            bits.log(m, (x - 0.06, sy * (STEP_HALF - 0.02), z - 0.2), (x - 0.06, sy * (STEP_HALF - 0.02), z + 0.08), 0.025, "timber", g.randrange(999), sides=6)
    # the niche for bombs
    def fill(P, gg):
        for k, (s, z) in enumerate(((-0.12, 0.0), (0.12, 0.0), (0.0, 0.155))):
            ammo_box(m, P(RF + 0.27, s, -D + 0.035 + z), NA, (0.5, 0.15, 0.15), gg.randrange(999), paint="upper")
        bomb(m, P(RF + 0.08, -0.2, -D + 0.075), -V2(NA) + Vector((0, 0, 0)))
    niche(m, NA, RF - 0.01, -D, -0.17, 0.32, 0.6, seed=5, fill=fill)
    # the collar: three courses two bags thick on the rim, a gap at the steps; earth banked low outside
    gh = math.degrees(math.asin(0.45 / 1.555))
    # (the sandbag wall's own bag, slumped and tied at the neck: the pits' lighter bag read as cushions here, where
    # the collar is seen from above and close)
    top = bits.bag_wall(m, bits.arc(0, 0, 1.555, -180 + gh, 180 - gh, 40), 2, 0.025, seed=9, batter=0.015, thick=2, inward=-1.0)
    # (the top course is headers: bags laid across both rows, each sagging over the course beneath, its tied neck
    # outward. Seen from above, a course of stretchers pressed flat read as a ring of cushions)
    n = 31
    for k in range(n):
        # (laid by tired men: none quite square to the rim, some pushed in, some out)
        a = -180 + gh + 1.5 + (k + 0.5 + g.uniform(-0.16, 0.16)) / n * (360 - 2 * gh - 3.0)
        at = V2(a, 1.69 + g.uniform(-0.06, 0.06)) + Z * (top - 0.012 + g.uniform(-0.004, 0.008))
        size = (bits.BAG[0] * g.uniform(0.9, 1.04), bits.BAG[1] * g.uniform(0.94, 1.06), bits.BAG[2])
        bits.sandbag(m, at, a + g.uniform(-11, 11), size=size, seed=g.randrange(1 << 30), droop=g.uniform(0.04, 0.062), pitch=g.uniform(-2.5, 2.5), roll=g.uniform(-4.0, 4.0))

    def cut(x, y):
        return smooth((abs(y) - 0.65) / 0.4) if x < -1.0 else 1.0
    # (the bank lies low against the collar, so the bags' staggered courses show above it)
    ring_bank(m, 1.895, [(0.0, 0.1), (0.14, 0.13), (0.39, 0.13), (0.74, 0.07), (1.04, 0.02), (1.19, 0.0)], seed=4, lump=0.03, step=2.5, cut=cut, turf=0.5)
    # the mortar, its bombs to hand, empties of the charges' containers
    O = Vector((-0.22, 0.0, -D))
    mt = mortar(m, "pit", O)
    m.use("pit")
    bomb_box(m, O + Vector((-0.55, -0.62, 0.0)), 35.0, 4, seed=5, lid=False)
    for k in range(3):
        bomb(m, O + Vector((-0.75 + 0.09 * k, -0.15, 0.042)), Vector((0.2, -1.0, 0.0)) + Vector((g.uniform(-0.1, 0.1), 0, 0)))
    if fine:
        # (thrown to the left front, along the wall's foot, where nobody kneels: number one's boots are on the floor
        # from x -0.61 to 0.19 on the left, number two's on the right)
        for k in range(5):
            ba = math.radians(g.uniform(25.0, 65.0))
            p = Vector((math.cos(ba), math.sin(ba), 0.0)) * g.uniform(0.78, 1.0) + Z * (-D + 0.035)
            ya = ba + math.pi / 2 + g.uniform(-0.7, 0.7)
            m.lathe(p, Vector((math.cos(ya), math.sin(ya), 0)), [(0.0, 0.032), (0.22, 0.032)], "rubber", 10, (True, False))
            m.disc(p + Vector((math.cos(ya), math.sin(ya), 0)) * 0.219, Vector((math.cos(ya), math.sin(ya), 0)), 0.028, "void", 10)
    m.rod(V2(150, RF - 0.1) + Z * -D, V2(150, RF - 0.05) + Z * 0.35, 0.013, "timber", sides=6)        # the cleaning rod
    # two aiming posts out in front, banded so the layer can see them
    for k, (x, y) in enumerate(((4.5, 0.9), (7.5, 1.45))):
        base = Vector((x, y, -0.25))
        for j in range(6):
            m.lathe(base + Z * (j * 0.25), Z, [(0.0, 0.017), (0.25, 0.017)], "sign" if j % 2 else "sign.3", 10 if fine else 4, (j == 0, j == 5))
        m.lathe(base + Z * 1.5, Z, [(0.0, 0.017), (0.04, 0.004)], "steel", 8)
        bits.patch(m, x, y, 0.2, "earth", seed=40 + k, lift=0.006)
    m.socket("hit", (1.6, 0.0, 0.25), (1, 0, 0), note="where a bomb strikes the collar")
    bits.crew(m, "number1", O + Vector((0.08, 0.6, 0.0)), (0.35, -1, 0), "kneel", "pit", "number one kneeling on the left, his eye at the sight (socket `sight`)")
    bits.crew(m, "number2", O + Vector((0.55, -0.55, 0.0)), (-0.25, 1, 0), "kneel", "pit", "number two kneeling on the right front, his hands at the muzzle to drop the bomb",
              grips={"l": (mt["muzzle"] + Vector((-0.02, -0.06, 0.05)), (0, 1, 0), "tube"), "r": (mt["muzzle"] + Vector((0.03, -0.06, -0.04)), (0, 1, 0), "tube")})
    bits.crew(m, "number3", O + Vector((-0.65, -0.35, 0.0)), (0.6, 0.8, 0), "kneel", "pit", "number three kneeling at the back by the niche, passing bombs to number two")
    m.clips["fire"] = mortar_fire_clip()
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Laid and still.", "tracks": [], "events": []}
    weather(m, [(mt["muzzle"], 0.3, 0.7)], splash=0.3, rust=0.3)
    m.info["span"] = 5.0
    m.info["aim"] = (1.0, 0.0, 0.1)
    m.info["dig"] = dig
    m.info["dig_depth"] = D
    m.info["worn"] = [(-0.3, 0.0, 3.0)]
    m.info["clear"] = [[tuple(V2(a, 2.9).to_2d()) for a in range(0, 360, 20)]]
    return m


# ---- 5. the camouflage net -------------------------------------------------------------------------------------

def CamoNet(fine=True, nation="_default", name="CamoNet"):
    """A camouflage net about 7 m square on six poles, guyed out to pegs, sagging between the poles: a fine mesh of
    cord with scraps of scrim tied on over about three fifths of it, and a few leafy branches thrown over."""
    m = bits.model(name, "Camouflage net on poles (our own model)", "works", nation, fine, MORE)
    m.part("poles", None, (0, 0, 0), kind="root")
    g = rnd(77)
    HALF = 3.5
    POLES = [(-3.0, 3.0, 2.35), (0.0, 3.1, 2.55), (3.0, 3.0, 2.35), (-3.0, -3.0, 2.35), (0.0, -3.1, 2.55), (3.0, -3.0, 2.35)]

    def surf(x, y):
        """The net's height: lifted at each pole, sagging between, pulled down at its edges by the guys."""
        w_sum, h_sum, dmin = 0.0, 0.0, 9.0
        for px, py, ph in POLES:
            d2 = (x - px) ** 2 + (y - py) ** 2
            w = 1.0 / (d2 + 0.05) ** 1.2
            w_sum += w
            h_sum += w * ph
            dmin = min(dmin, math.sqrt(d2))
        z = h_sum / w_sum - 0.55 * (1.0 - math.exp(-dmin * dmin / 2.2))
        z += 0.07 * math.exp(-dmin * dmin / 0.03)
        edge = max(abs(x), abs(y))
        z -= 0.42 * smooth((edge - 2.95) / 0.75) + 0.2 * smooth((abs(x) + abs(y) - 5.6) / 1.4)
        z += 0.03 * bits.fbm((x * 0.8, y * 0.8, 2.0), 0.6)
        return z

    # ---- the poles, the guys and pegs
    m.use("poles")
    tops = []
    for k, (px, py, ph) in enumerate(POLES):
        top = Vector((px, py, surf(px, py) - 0.06))
        bits.log(m, (px, py, -0.15), top, 0.045, "timber", 400 + k, taper=0.8)
        bits.plank(m, top + Vector((-0.2, 0, 0.0)), top + Vector((0.2, 0, 0.0)), 0.07, 0.03, "timber", Z, k)       # the spreader under the net
        bits.patch(m, px, py, 0.25, "earth", seed=k, lift=0.005)
        tops.append(top)
        # its guy: out and away to a peg
        out = Vector((px * 0.35, py * 0.45, 0.0)).normalized() if abs(px) > 0.1 else Vector((0.0, math.copysign(1, py), 0.0))
        peg = Vector((px, py, 0.0)) + out * 2.0
        guy(m, top + Z * 0.02, peg, g.randrange(999))
    # the edges pulled down to pegs between the poles
    for x, y in ((-HALF, 0.0), (HALF, 0.0), (-1.5, HALF), (1.5, HALF), (-1.5, -HALF), (1.5, -HALF), (-HALF, -HALF), (HALF, HALF), (-HALF, HALF), (HALF, -HALF)):
        e = Vector((x, y, surf(x, y)))
        out = Vector((x, y, 0.0))
        out = Vector((math.copysign(1, x) if abs(x) > 3.4 else 0.0, math.copysign(1, y) if abs(y) > 3.4 else 0.0, 0.0)).normalized()
        guy(m, e, Vector((x, y, 0.0)) + out * (1.0 + 0.4 * (abs(out.x) * abs(out.y) > 0.1)), g.randrange(999))

    # ---- the net
    m.part("net", "poles", (0.0, 0.0, 2.2), kind="free", note="stirs in the wind (clip sway)")
    m.use("net")
    CELL = 0.35
    n = int(round(2 * HALF / CELL))
    grid = [[Vector((-HALF + i * CELL, -HALF + j * CELL, 0.0)) for j in range(n + 1)] for i in range(n + 1)]
    for row in grid:
        for p in row:
            p.z = surf(p.x, p.y)
    sides = 3 if fine else 3
    if fine:
        for i in range(n + 1):
            bits.tube(m, [grid[i][j] for j in range(n + 1)], 0.007 if 0 < i < n else 0.014, "net" if 0 < i < n else "rope", sides, caps=False)
        for j in range(n + 1):
            bits.tube(m, [grid[i][j] for i in range(n + 1)], 0.007 if 0 < j < n else 0.014, "net" if 0 < j < n else "rope", sides, caps=False)
    else:
        # the far mesh: the net as one sheet, both faces
        rows = [[p + Z * 0.004 for p in row] for row in grid]
        m.sheet(rows, "net", Z, True)
        m.sheet([[p - Z * 0.004 for p in row] for row in grid], "net", -Z, True)

    # the scrim: scraps tied on in drifts, about three fifths covered, three colours in broad patches
    def scrap(c, ang, L, W, paint):
        d = Vector((math.cos(ang), math.sin(ang), 0.0))
        s = Vector((-d.y, d.x, 0.0))
        k = 4
        pts = []
        for i in range(k):
            t = i / (k - 1) - 0.5
            w = W * (0.55 + 0.45 * math.sin(math.pi * (t + 0.5))) * g.uniform(0.8, 1.15)
            q = c + d * (t * L)
            for sg in (-1, 1):
                r = q + s * (sg * w / 2)
                pts.append(Vector((r.x, r.y, surf(r.x, r.y) + 0.012 + g.uniform(-0.006, 0.01))))
        faces = [(2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2) for i in range(k - 1)]
        m.add(pts, faces, paint, True, lambda p: Z)
        m.add([p - Z * 0.004 for p in pts], faces, paint, True, lambda p: -Z)

    count = 0
    step = 0.165 if fine else 0.5
    nx = int(2 * HALF / step)
    for i in range(nx):
        for j in range(nx):
            x = -HALF + (i + g.uniform(0.1, 0.9)) * step
            y = -HALF + (j + g.uniform(0.1, 0.9)) * step
            if max(abs(x), abs(y)) > HALF - 0.08:
                continue
            dense = bits.fbm((x * 0.7 + 3.0, y * 0.7, 0.5), 1.0, 2)
            if g.random() > 0.62 + 0.55 * dense:
                continue
            tone = bits.fbm((x * 0.45, y * 0.45 + 9.0, 1.5), 1.0, 2)
            paint = "garnish" if tone > 0.12 else ("garnish.2" if tone > -0.15 else "garnish.3")
            if g.random() < 0.12:
                paint = g.choice(["garnish", "garnish.2", "garnish.3"])
            if fine:
                scrap(Vector((x, y, 0)), g.uniform(0, math.pi), g.uniform(0.22, 0.42), g.uniform(0.06, 0.11), paint)
            else:
                scrap(Vector((x, y, 0)), g.uniform(0, math.pi), 0.8, 0.4, paint)
            count += 1
    # a few leafy branches thrown over it
    for k in range(5 if fine else 2):
        c = Vector((g.uniform(-2.6, 2.6), g.uniform(-2.6, 2.6), 0.0))
        ang = g.uniform(0, 6.28)
        d = Vector((math.cos(ang), math.sin(ang), 0.0))
        L = g.uniform(1.0, 1.6)
        pts = []
        for i in range(5):
            q = c + d * (L * (i / 4 - 0.5))
            pts.append(Vector((q.x, q.y, surf(q.x, q.y) + 0.035)))
        bits.tube(m, pts, 0.02, "timber.dark", 5 if fine else 3, radii=[0.022, 0.018, 0.014, 0.01, 0.006])
        for i in range(36 if fine else 8):
            t = g.uniform(0.15, 1.0)
            q = c + d * (L * (t - 0.5)) + Vector((-d.y, d.x, 0)) * g.uniform(-0.25, 0.25) * t
            z = surf(q.x, q.y) + 0.04 + g.uniform(0.0, 0.08)
            la = g.uniform(0, 6.28)
            u = Vector((math.cos(la), math.sin(la), g.uniform(-0.3, 0.3))) * g.uniform(0.07, 0.11)
            v = Vector((-u.y, u.x, 0.0)).normalized() * 0.035
            p0 = Vector((q.x, q.y, z))
            m.add([p0, p0 + u * 0.5 + v, p0 + u, p0 + u * 0.5 - v], [(0, 1, 2, 3)], g.choice(["garnish.3", "garnish", "garnish.3"]), True, lambda p: Z)
    m.use("poles")
    m.socket("hit", (0.0, 0.0, 1.9), (0, 0, 1), part="net", note="where a shell bursts on it; the net is torn away (the game removes it)")
    m.socket("under", (0.0, 0.0, 0.0), (1, 0, 0), note="the middle under the net: a GunPit or a SandbagRing stands here (clear inside the poles: 6 m by 6 m, 1.7 m high at the middle)")
    q1 = Quaternion(Vector((1, 0.3, 0)).normalized(), math.radians(0.5))
    q2 = Quaternion(Vector((-0.4, 1, 0)).normalized(), math.radians(0.45))
    m.clips["sway"] = {"long": 4.0, "loop": True, "about": "The net stirring a little in the wind.",
                       "tracks": [{"part": "net", "rot": [[0.0, 1, 0, 0, 0], [1.0, q1.w, q1.x, q1.y, q1.z], [2.0, 1, 0, 0, 0], [3.0, q2.w, q2.x, q2.y, q2.z], [4.0, 1, 0, 0, 0]],
                                   "move": [[0.0, 0, 0, 0], [1.0, 0.0, 0.0, 0.02], [2.0, 0, 0, -0.005], [3.0, 0.0, 0.0, 0.015], [4.0, 0, 0, 0]]}], "events": []}
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Still.", "tracks": [], "events": []}
    weather(m, splash=0.2, rust=0.2)
    m.info["span"] = 9.0
    m.info["high"] = 1.3
    m.info["scraps"] = count
    m.info["worn"] = [(0.0, 0.0, 3.5, 0.5)]
    return m


def guy(m, a, b, seed=0):
    """A guy rope from a to a peg driven at b, sagging a little."""
    g = rnd(seed)
    a, b = Vector(a), Vector(b)
    pts = [a.lerp(b, t) - Z * (0.06 * math.sin(math.pi * t)) for t in (0.0, 0.25, 0.5, 0.75, 1.0)]
    pts[-1] = b + Z * 0.12
    bits.tube(m, pts, 0.007, "rope", 4 if m.fine else 3)
    out = (Vector((b.x - a.x, b.y - a.y, 0))).normalized()
    bits.log(m, b - out * 0.04 - Z * 0.12, b + out * 0.03 + Z * 0.2, 0.022, "timber", g.randrange(999), sides=6, taper=0.85)


# ---- 6. the observation post ------------------------------------------------------------------------------------

def ObservationPost(fine=True, nation="_default", name="ObservationPost"):
    """A timber watchtower: four splayed legs braced across, a platform 6 m up with a waist-high planked parapet and
    bags round its floor, a pitched plank roof, a ladder up through a hatch, a field telephone, an observer at his
    glasses."""
    m = bits.model(name, "Observation post, timber tower (our own design)", "works", nation, fine, MORE)
    g = rnd(55)
    FOOT, TOPW, H = 1.6, 1.05, 6.0
    PIV = Vector((-FOOT - 0.12, 0.0, 0.12))
    m.part("footings", None, (0, 0, 0), kind="root")
    m.use("footings")
    feet = [Vector((sx * FOOT, sy * FOOT, 0.0)) for sx in (-1, 1) for sy in (-1, 1)]
    for k, f in enumerate(feet):
        m.box(f + Z * 0.05, (0.7, 0.7, 0.16), "timber.dark", yaw=g.uniform(-4, 4))                                 # the sill
        m.box(f + Z * 0.14, (0.42, 0.42, 0.05), "timber", yaw=g.uniform(-4, 4))
        bits.patch(m, f.x, f.y, 0.65, "earth", seed=k, lift=0.006)
        for sx, sy in ((1, 1), (-1, 1), (1, -1), (-1, -1)):
            bits.log(m, f + Vector((sx * 0.38, sy * 0.38, -0.1)), f + Vector((sx * 0.38, sy * 0.38, 0.16)), 0.03, "timber", g.randrange(999), sides=6)
    if fine:
        cable = [Vector((-FOOT + 0.1, FOOT - 0.1, 0.02)), Vector((-2.4, 1.9, 0.015)), Vector((-3.4, 2.1, 0.015)), Vector((-4.5, 1.8, 0.015))]
        bits.tube(m, cable, 0.006, "rubber", 4)

    m.part("tower", "footings", PIV, kind="free", note="everything above the feet: it leans and falls over in clip collapse")
    m.use("tower")
    tops = [Vector((math.copysign(TOPW, f.x), math.copysign(TOPW, f.y), H)) for f in feet]
    for k, (f, t) in enumerate(zip(feet, tops)):
        bits.log(m, f + Z * 0.16, t + Z * 0.02, 0.11, "timber", 500 + k, taper=0.85)
    # the corner posts up from the platform to the roof
    EAVE = H + 2.05
    for t in tops:
        bits.log(m, t, t + Z * (EAVE - H), 0.075, "timber", g.randrange(999), taper=0.95)

    def leg(k, z):
        f, t = feet[k], tops[k]
        u = (z - 0.16) / (H - 0.16)
        return f.lerp(t, u) + Z * 0.0 * u
    faces = [(0, 1), (1, 3), (3, 2), (2, 0)]
    levels = [0.5, 2.6, 4.6]
    for a, b in faces:
        for z in levels[1:]:
            pa, pb = leg(a, z), leg(b, z)
            o = ((pa + pb) / 2).to_2d().normalized().to_3d() * 0.11
            bits.log(m, pa + o, pb + o, 0.06, "timber", g.randrange(999), taper=0.95)
            if fine:
                bits.bolt(m, pa + o * 1.6, o, 0.025, 0.02, "iron", 6)
                bits.bolt(m, pb + o * 1.6, o, 0.025, 0.02, "iron", 6)
        for z0, z1 in ((levels[0], levels[1]), (levels[1], levels[2]), (levels[2], H - 0.15)):
            for x0, x1 in ((a, b), (b, a)):
                pa, pb = leg(x0, z0), leg(x1, z1)
                o = ((pa + pb) / 2).to_2d().normalized().to_3d() * 0.18
                bits.plank(m, pa + o, pb + o, 0.15, 0.05, "planks", o, g.randrange(999))
                if fine:
                    bits.bolt(m, pa + o * 1.15, o, 0.022, 0.02, "iron", 6)
                    bits.bolt(m, pb + o * 1.15, o, 0.022, 0.02, "iron", 6)
    # the platform: bearers, joists, a floor of planks with the hatch at the left rear
    P = 1.35
    for sy in (-1, 1):
        bits.plank(m, (-P - 0.1, sy * TOPW, H - 0.08), (P + 0.1, sy * TOPW, H - 0.08), 0.12, 0.16, "timber", Z, g.randrange(999))
    for k in range(7):
        x = -P + 0.05 + k * (2 * P - 0.1) / 6
        bits.plank(m, (x, -P, H + 0.04), (x, P, H + 0.04), 0.08, 0.08, "timber", Z, g.randrange(999))
    HX0, HX1, HY0, HY1 = -1.15, -0.55, 0.35, 0.95
    nb = 15
    for k in range(nb):
        y = -P + (k + 0.5) * (2 * P / nb)
        w = 2 * P / nb * 0.94
        if HY0 - 0.02 < y < HY1 + 0.02:
            bits.plank(m, (-P, y, H + 0.095), (HX0, y, H + 0.095), w, 0.03, "planks", Z, g.randrange(999))
            bits.plank(m, (HX1, y, H + 0.095), (P, y, H + 0.095), w, 0.03, "planks", Z, g.randrange(999))
        else:
            bits.plank(m, (-P + g.uniform(0, 0.03), y, H + 0.095), (P - g.uniform(0, 0.03), y, H + 0.095), w, 0.03, "planks", Z, g.randrange(999))
    # the hatch's lid, thrown back open on its hinges
    hl = [Vector((HX0, HY0, H + 0.11)), Vector((HX1, HY0, H + 0.11))]
    bits.plate(m, [(0.0, 0.0), (HX1 - HX0, 0.0), (HX1 - HX0, HY1 - HY0), (0.0, HY1 - HY0)], 0.035, "planks", Vector((HX0, HY0 - 0.02, H + 0.12)), X, Vector((0, -0.25, 1)))
    # the parapet: posts, level planks to waist height, a rail on top
    TOPZ = H + 1.05
    corners = [Vector((P, P, 0)), Vector((-P, P, 0)), Vector((-P, -P, 0)), Vector((P, -P, 0))]
    for k in range(4):
        a, b = corners[k], corners[(k + 1) % 4]
        d = (b - a).normalized()
        o = Vector((d.y, -d.x, 0))
        for t in (0.0, 0.5):
            p = a.lerp(b, t)
            bits.plank(m, p + Z * (H + 0.08), p + Z * TOPZ, 0.1, 0.1, "timber", o, g.randrange(999))
        for c in range(6):
            z = H + 0.2 + c * 0.155
            bits.plank(m, a + o * 0.06 + Z * z - d * 0.06, b + o * 0.06 + Z * z + d * 0.06, 0.15, 0.03, g.choice(["planks", "planks", "timber"]), o, g.randrange(999))
        bits.plank(m, a + Z * (TOPZ + 0.03) - d * 0.08, b + Z * (TOPZ + 0.03) + d * 0.08, 0.14, 0.05, "timber", Z, g.randrange(999))
        # bags round the floor inside the parapet
        bag_wall(m, [(a - o * 0.15 + d * 0.15).to_2d(), (b - o * 0.15 - d * 0.15).to_2d()], 2, H + 0.11, seed=60 + k, batter=0.0, thick=1, q=(6, 9), tie=0.25,
                 keep=(lambda t, c, f, k=k: not (k == 1 and 0.45 < f < 0.8)))
    # the roof: rafters to a ridge, boards down each slope
    RIDGE = EAVE + 0.75
    RO = P + 0.35
    for x in (-P, 0.0, P):
        for sy in (-1, 1):
            bits.plank(m, (x, sy * RO, EAVE - 0.08), (x, 0.0, RIDGE), 0.08, 0.12, "timber", Vector((0, sy * 0.75, RO)), g.randrange(999))
    for sy in (-1, 1):
        bits.plank(m, (-P - 0.05, sy * TOPW, EAVE - 0.02), (P + 0.05, sy * TOPW, EAVE - 0.02), 0.12, 0.12, "timber", Z, g.randrange(999))
        n = 16
        for k in range(n):
            x = -P - 0.25 + (k + 0.5) * (2 * P + 0.5) / n
            up = Vector((0, sy * 0.75, RO)).normalized()
            bits.plank(m, Vector((x, sy * (RO + 0.05), EAVE - 0.0)) + up * 0.07, Vector((x, 0.0, RIDGE)) + up * 0.07, (2 * P + 0.5) / n * 0.97, 0.025, g.choice(["planks", "planks", "timber"]), up, g.randrange(999))
    bits.plank(m, (-P - 0.3, 0, RIDGE + 0.1), (P + 0.3, 0, RIDGE + 0.1), 0.16, 0.04, "timber", Z, g.randrange(999))
    # the ladder up through the hatch
    lb, lt = Vector((-0.65, 0.65, 0.0)), Vector((-0.85, 0.65, H + 0.95))
    for sy in (-1, 1):
        bits.log(m, lb + Y * sy * 0.22 + Z * 0.16, lt + Y * sy * 0.22, 0.032, "timber", g.randrange(999), sides=8 if fine else 4)
    for k in range(int((H + 0.9) / 0.32)):
        t = (0.3 + k * 0.32) / (H + 0.95)
        p = lb.lerp(lt, t)
        m.rod(p - Y * 0.24, p + Y * 0.24, 0.018, "timber", sides=7 if fine else 4)
    # the telephone box on the left parapet, its cable down a leg
    tel = Vector((0.3, P - 0.12, H + 0.75))
    m.box(tel, (0.26, 0.12, 0.2), "wood.paint")
    m.box(tel + Vector((0, -0.065, 0.02)), (0.2, 0.01, 0.12), "wood.paint")
    m.rod(tel + Vector((-0.08, -0.08, 0.12)), tel + Vector((0.08, -0.08, 0.12)), 0.016, "iron", sides=8)
    m.rod(tel + Vector((0.13, -0.02, 0.0)), tel + Vector((0.18, -0.02, 0.0)), 0.006, "steel", sides=5)
    m.rod(tel + Vector((0.18, -0.02, 0.0)), tel + Vector((0.18, -0.06, -0.04)), 0.006, "steel", sides=5)
    if fine:
        cpts = [tel + Vector((-0.13, 0.0, -0.05)), Vector((-0.6, P - 0.05, H + 0.6)), Vector((-P + 0.02, P + 0.02, H + 0.1))]
        cpts += [leg(1, z) + Vector((0.0, 0.12, 0.0)) for z in (H - 0.2, 4.0, 2.0, 0.3)]
        bits.tube(m, cpts, 0.006, "rubber", 4)
    # the observer's glasses on their stand at the front parapet
    st = Vector((P - 0.25, 0.0, H + 0.11))
    m.rod(st, st + Z * 1.2, 0.022, "steel", sides=8)
    for a in (0, 120, 240):
        m.rod(st + Z * 0.25, st + V2(a, 0.22) + Z * 0.01, 0.012, "steel", sides=6)
    gl = st + Vector((-0.04, 0.0, 1.3))
    m.box(gl, (0.08, 0.06, 0.06), "upper")
    for sy in (-1, 1):
        m.lathe(gl + Vector((-0.09, sy * 0.055, 0.0)), X, [(0.0, 0.03), (0.04, 0.03), (0.06, 0.036), (0.2, 0.036), (0.21, 0.032)], "upper", 12 if fine else 5)
        m.disc(gl + Vector((0.121, sy * 0.055, 0.0)), X, 0.03, "glass", 10)
        m.disc(gl + Vector((-0.091, sy * 0.055, 0.0)), -X, 0.024, "glass", 10)
    m.box(st + Vector((0.0, -0.4, 0.94)), (0.22, 0.3, 0.02), "planks")                     # a shelf for the map board
    bits.plate(m, [(-0.12, -0.14), (0.12, -0.14), (0.12, 0.14), (-0.12, 0.14)], 0.004, "sign.3", st + Vector((-0.02, -0.4, 0.955)), X, Y)
    bits.crew(m, "observer", Vector((P - 0.75, 0.0, H + 0.11)), (1, 0, 0), "stand", "tower", "the observer standing at his glasses on their stand, eyes at the eyepieces",
              grips={"l": (gl + Vector((-0.1, 0.11, 0.0)), (1, 0, 0), "tower"), "r": (gl + Vector((-0.1, -0.11, 0.0)), (1, 0, 0), "tower")})
    m.socket("glasses", gl + Vector((-0.1, 0, 0)), (1, 0, 0), part="tower", note="the eyepieces of the glasses on their stand")
    m.socket("hit", (0.0, 0.0, 3.4), (1, 0, 0), part="tower", note="where the shell strikes the legs")
    # ---- how far over it goes before it lies on the ground
    axis = Vector((0.1, 1.0, 0.0)).normalized()
    ti = m.part_at["tower"]
    pts = [m.verts[i] for f, pt in zip(m.faces, m.fpart) if pt == ti for i in f]
    pts = [p for p in pts[::5] if p.z > 2.0]
    final = 85.0
    for deg in range(20, 120, 1):
        q = Quaternion(axis, math.radians(-deg))
        if min((PIV + q @ (p - PIV)).z for p in pts) < 0.02:
            final = deg - 1
            break
    qf = Quaternion(axis, math.radians(-final))
    land = PIV + qf @ (Vector((0, 0, H)) - PIV)
    m.use("footings")
    m.socket("landing", (land.x, land.y, 0.0), (0, 0, 1), part="footings", note="where the platform and roof come down: dust and splinters")
    keys, moves = [], []
    for t, a in [(0.0, 0.0), (0.2, 1.5), (0.5, 4.0)] + [(0.5 + 1.5 * f, 4.0 + (final - 4.0) * f * f) for f in (0.2, 0.35, 0.5, 0.62, 0.74, 0.84, 0.92, 1.0)] + [(2.15, final - 4.0), (2.32, final - 0.5), (2.5, final)]:
        q = Quaternion(axis, math.radians(-a))
        keys.append([round(t, 3), q.w, q.x, q.y, q.z])
    moves = [[0.0, 0, 0, 0], [1.6, -0.08, 0, 0], [2.0, -0.35, 0.05, -0.02], [2.5, -0.42, 0.06, -0.03]]
    m.clips["collapse"] = {"long": 2.5, "loop": False, "after": "hold",
                           "about": "A hit on the legs: the tower leans back off its feet and falls over away from the enemy, lands about 2 s in, settles and lies there.",
                           "tracks": [{"part": "tower", "rot": keys, "move": moves}],
                           "events": [{"t": 0.0, "fx": "shell_burst", "socket": "hit", "cue": "building_collapse"},
                                      {"t": 2.0, "fx": "dust_cloud", "socket": "landing", "cue": ""}]}
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing.", "tracks": [], "events": []}
    weather(m, splash=0.5, rust=0.3)
    m.info["span"] = 11.5
    m.info["high"] = 4.6
    m.info["worn"] = [(0.0, 0.0, 2.6)]
    m.info["clear"] = [[(x * 2.1, y * 2.1) for x, y in ((1, 1), (-1, 1), (-1, -1), (1, -1))]]
    return m


BUILDERS = {
    "FlakPit": FlakPit,
    "SandbagRing": SandbagRing,
    "GunPit": GunPit,
    "AntiTankPit": lambda fine=True, nation="_default": GunPit(fine, nation, "AntiTankPit", low_front=True),
    "Mortar": Mortar,
    "MortarPit": MortarPit,
    "CamoNet": CamoNet,
    "ObservationPost": ObservationPost,
}
