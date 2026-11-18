"""Works dug into the ground: a two-man foxhole, lengths of fieldworks (a straight fire bay, a dogleg corner, a bay
with a dugout), and an anti-tank ditch.

Every dug piece models its own hole: walls, floor and the earth lips round it, as one lumpy sheet swept across a
cross-section (`sweep`), and says in `m.info["dig"]` where the ground must be opened for it. A length runs along y
from -5 to +5 with the enemy toward +x; its two ends are flat, open, and carry the same cross-section (the lumps fade
out before the ends), so lengths butt end to end. Our own plain designs throughout.
"""
import math

from mathutils import Quaternion, Vector

import bits
from bits import X, Y, Z, fbm, rnd
from posts import ammo_box, machine_gun, mg_fire_clip, spent, weather
from rigid import smooth

MORE = {"mud": "4B4033"}


# ---- a path in plan, and a frame along it ----------------------------------------------------------------------

def rad(u):
    """The fillet radius a line offset `u` from the path takes round a corner (big enough that nothing folds)."""
    return max(2.0, abs(u) + 0.8)


class Path:
    """A polyline in plan. A point is named by s (distance along the polyline), u (offset to the path's RIGHT: toward
    the enemy for a path running +y) and z. Round a corner each offset line follows its own fillet, so a wide bank
    does not fold where a narrow wall would turn sharply."""

    def __init__(self, pts):
        self.v = [Vector((p[0], p[1], 0.0)) for p in pts]
        self.d = [(b - a).normalized() for a, b in zip(self.v, self.v[1:])]
        self.s = [0.0]
        for a, b in zip(self.v, self.v[1:]):
            self.s.append(self.s[-1] + (b - a).length)
        self.L = self.s[-1]
        self.turn = []
        for j in range(1, len(self.v) - 1):
            a, b = self.d[j - 1], self.d[j]
            self.turn.append(math.atan2(a.x * b.y - a.y * b.x, a.dot(b)))

    def frame(self, s, R=2.0):
        for j in range(1, len(self.v) - 1):
            th = self.turn[j - 1]
            if abs(th) < 1e-6:
                continue
            T = R * math.tan(abs(th) / 2)
            sj = self.s[j]
            if abs(s - sj) < T:
                din = self.d[j - 1]
                A = self.v[j] - din * T
                left = Vector((-din.y, din.x, 0.0))
                O = A + left * R * (1.0 if th > 0 else -1.0)
                q = Quaternion(Z, th * (s - (sj - T)) / (2 * T))
                return O + q @ (A - O), q @ din
        i = 0
        while i < len(self.d) - 1 and s > self.s[i + 1]:
            i += 1
        return self.v[i] + self.d[i] * (s - self.s[i]), self.d[i]

    def at(self, s, u=0.0, z=0.0):
        P, t = self.frame(s, rad(u))
        return P + Vector((t.y, -t.x, 0.0)) * u + Z * z

    def axes(self, s, u=0.0):
        """(along, right) at s: unit vectors in plan."""
        _, t = self.frame(s, rad(u))
        return t.copy(), Vector((t.y, -t.x, 0.0))

    def local(self, p):
        """(s, u) of a place, by the nearest point of the polyline (good enough away from the corners)."""
        best = None
        for i, (a, b) in enumerate(zip(self.v, self.v[1:])):
            ab = b - a
            t = min(max(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / ab.length_squared, 0.0), 1.0)
            q = a + ab * t
            d = math.hypot(p.x - q.x, p.y - q.y)
            if best is None or d < best[0]:
                r = (ab.y / ab.length, -ab.x / ab.length)
                best = (d, self.s[i] + t * ab.length, (p.x - q.x) * r[0] + (p.y - q.y) * r[1])
        return best[1], best[2]


def rows_for(L, step, breaks=()):
    """The s of every row of a sweep: an even grid, plus the edges of every cut (each edge doubled 1 cm apart so a
    cut has a crisp side), with grid rows too close to an edge dropped."""
    edges = []
    for a, b in breaks:
        edges += [a, a + 0.01, b - 0.01, b]
    n = max(1, int(round(L / step)))
    grid = [L * k / n for k in range(n + 1)]
    grid = [g for g in grid if all(abs(g - e) > step * 0.3 for e in edges) or g in (0.0, L)]
    return sorted(set(round(v, 5) for v in grid + edges))


def sweep(m, path, prof, srows, amp, turf=0.2, seed=0.0, push=None, skip=None, endfall=0.8, groove=None):
    """One lumpy sheet of earth swept along a path: `prof` is the cross-section, (u, z, tag) from the rear to the
    front; `amp[tag]` how lumpy each kind of point is; `push(s, k, u, z, tag)` -> (du, dz) cuts niches, recesses and
    sumps; `skip(s, k)` leaves out the quad over a row gap at profile point k (a doorway); `groove(s, u, z, tag)`
    adds a lump of its own (rain gullies). The lumps die away within `endfall` of each end, so the ends are true."""
    n = len(prof)
    nrm = []
    for k in range(n):
        i, j = k - 1, k + 1
        while i > 0 and abs(prof[i][0] - prof[k][0]) + abs(prof[i][1] - prof[k][1]) < 1e-6:
            i -= 1
        while j < n - 1 and abs(prof[j][0] - prof[k][0]) + abs(prof[j][1] - prof[k][1]) < 1e-6:
            j += 1
        a, b = prof[max(i, 0)], prof[min(j, n - 1)]
        du, dz = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(du, dz) or 1.0
        nrm.append((-dz / ln, du / ln))
    s0, s1 = srows[0], srows[-1]
    verts = []
    for s in srows:
        w = smooth(min(s - s0, s1 - s) / endfall) if endfall else 1.0
        for k, (u, z, tag) in enumerate(prof):
            du = dz = 0.0
            if push:
                du, dz = push(s, k, u, z, tag)
            a = amp.get(tag, 0.0) * w
            disp = 0.0
            if a:
                base = path.at(s, u, z)
                disp = a * (0.72 * fbm(base + Vector((seed, 0.0, 0.0)), 0.55) + 0.28 * fbm(base * 1.0 + Vector((0.0, seed + 11.0, 0.0)), 0.14))
            if groove:
                disp += groove(s, u, z, tag) * w
            if tag == "edge":         # (the outer edge of the thrown earth is ragged, not ruled)
                du += math.copysign(0.32, u) * fbm(Vector((s * 0.5, u, seed + 2.0)), 1.0, 3) * w
            verts.append(path.at(s, u + du + nrm[k][0] * disp, z + dz + nrm[k][1] * disp))
    faces, ftags = [], []
    for r in range(len(srows) - 1):
        sm = 0.5 * (srows[r] + srows[r + 1])
        for k in range(n - 1):
            if skip and skip(sm, k):
                continue
            faces.append((r * n + k, r * n + k + 1, (r + 1) * n + k + 1, (r + 1) * n + k))
            ftags.append((prof[k][2], prof[k + 1][2]))
    earth_skin(m, verts, faces, ftags, [prof[i % n][2] for i in range(len(verts))], turf, seed)
    return verts


def _key(p):
    return (round(p.x, 4), round(p.y, 4), round(p.z, 4))


BARE = ("edge", "apron", "bank", "glacis", "spoil", "berm", "lip", "flip", "slope")


def earth_paint(tags, mid):
    """The paint of a face of dug earth, by what kind of earth it is (open ground takes its points' paint instead)."""
    a, b = tags
    t = a if a not in ("edge",) else b
    if t in ("floor", "sump"):
        return "mud"
    if t in BARE:
        return "earth.dry"
    return "mud" if mid.z < -1.85 else "earth"


def earth_coat(tag, p, turf, seed=0.0):
    """The paint of a point of open ground: dry thrown earth, darker damp earth, and turf grown over in soft patches
    (more with `turf`); None for the walls and floors, which keep their face's paint."""
    if tag not in BARE:
        return None
    n = 0.5 + 0.5 * fbm((p.x / 1.5 + seed * 3.1, p.y / 1.5 - seed, 0.37), 1.0, 3) + 0.12 * fbm((p.x * 2.3, p.y * 2.3, 5.0 + seed), 1.0, 2)
    if tag in ("bank", "glacis", "spoil", "apron", "edge") and p.z > 0.04 and n < turf:
        return "turf"
    d = fbm((p.x * 0.8 + 17.0, p.y * 0.8 - seed, 1.3), 1.0, 3)
    if tag in ("lip", "flip", "berm") or d > (0.0 if tag == "slope" else 0.22):
        return "earth"
    return "earth.dry"


def earth_skin(m, verts, faces, ftags, ptags, turf=0.2, seed=0.0):
    """Dug earth into the model: the faces as wound (they already look out of the earth), walls and floors by their
    face's paint, open ground point by point so earth and turf run softly into each other."""
    vs = [Vector(v) for v in verts]
    coat = [earth_coat(t, v, turf, seed) for v, t in zip(vs, ptags)]
    keep, paints, byp = [], [], []
    for f, tg in zip(faces, ftags):
        a, b, c = vs[f[0]], vs[f[1]], vs[f[2]]
        nrm = (b - a).cross(c - a)
        if len(f) > 3:
            nrm = nrm + (c - a).cross(vs[f[3]] - a)
        if nrm.length < 1e-10:
            continue
        mid = sum((vs[i] for i in f), Vector()) / len(f)
        keep.append(tuple(f))
        paints.append(earth_paint(tg, mid))
        byp.append(all(coat[i] is not None for i in f))
    m.raw(vs, keep, paints, m.parts[m.cur].name, coat=coat, by_point=byp, smooth=True)


def dug_weather(m, base, soot_at=(), splash=0.35, rust=0.45):
    """posts.weather, with the dirt splashed up from the floor a thing stands on (the bay's floor, its fire step),
    not from the ground above."""
    weather(m, (), splash, rust)
    f = m.weather

    def g(p, n, paint, part):
        w, _ = f(Vector((p.x, p.y, p.z - base(p))), n, paint, part)
        s = 0.0
        for c, r, k in soot_at:
            s = max(s, k * smooth(1.0 - (p - Vector(c)).length / r))
        return w, s
    m.weather = g


# ---- small things lying about ------------------------------------------------------------------------------------

def crate(m, at, yaw=0.0, size=(0.44, 0.26, 0.2), seed=0):
    """A wooden ammunition box: boards with a batten round each end, rope handles."""
    at = Vector(at)
    q = Quaternion(Z, math.radians(yaw))
    L, W, H = size
    c = at + Z * (H / 2)
    g = rnd(seed)
    m.box(c, (L, W, H), g.choice(["planks", "planks", "timber"]), yaw=yaw)
    for sx in (-1, 1):
        m.box(c + q @ Vector((sx * (L / 2 - 0.035), 0, 0)), (0.045, W + 0.016, H + 0.014), "timber", yaw=yaw)
    if m.fine:
        for sy in (-1, 1):
            for k in (1, 2):
                m.box(c + q @ Vector((0, sy * (W / 2 + 0.0015), -H / 2 + H * k / 3)), (L - 0.1, 0.002, 0.005), "timber.dark", yaw=yaw)
        m.box(c + Z * (H / 2 + 0.0015), (L - 0.1, 0.004, W * 0.0 + 0.004), "timber.dark", yaw=yaw)
        for sx in (-1, 1):
            e = sx * (L / 2 + 0.024)
            bits.tube(m, [c + q @ Vector((e * 0.98, -0.055, 0.03)), c + q @ Vector((e * 1.05, -0.03, -0.01)), c + q @ Vector((e * 1.05, 0.03, -0.01)),
                          c + q @ Vector((e * 0.98, 0.055, 0.03))], 0.008, "canvas", 5)


def helmet(m, at, yaw=0.0, upside=False):
    at = Vector(at)
    if upside:
        m.lathe(at + Z * 0.005, Z, [(0.0, 0.04), (0.03, 0.10), (0.08, 0.135), (0.11, 0.15), (0.115, 0.16)], "upper", 14 if m.fine else 7, (True, False))
    else:
        m.ball(at + Z * 0.012, (0.135, 0.15, 0.115), "upper", 14 if m.fine else 7, 8 if m.fine else 4, top_only=True)
        m.lathe(at, Z, [(0.0, 0.16), (0.012, 0.155)], "upper", 14 if m.fine else 7, (True, True))


def shovel(m, foot, top, paint="steel"):
    """An entrenching spade: blade in the earth at `foot`, handle up to `top`."""
    foot, top = Vector(foot), Vector(top)
    d = (top - foot).normalized()
    m.rod(foot + d * 0.16, top, 0.017, "timber", sides=8)
    m.rod(top - d * 0.02, top + d * 0.03, 0.022, "timber.dark", sides=8)
    side = d.cross(Z)
    if side.length < 1e-4:
        side = X.copy()
    side.normalize()
    bits.plate(m, [(-0.075, 0.0), (0.075, 0.0), (0.08, 0.15), (0.0, 0.2), (-0.08, 0.15)], 0.004, paint, foot + d * 0.17, side, -d)
    m.rod(foot + d * 0.12, foot + d * 0.2, 0.02, paint, sides=8)


def pick(m, at, yaw=0.0):
    """A pick lying on its side."""
    at = Vector(at)
    q = Quaternion(Z, math.radians(yaw))
    m.rod(at + Z * 0.035, at + q @ Vector((0.85, 0, 0.035)), 0.02, "timber", rb=0.017, sides=8)
    head = [at + q @ Vector((0.0, -0.32, 0.03)), at + q @ Vector((0.0, -0.12, 0.05)), at + q @ Vector((0.0, 0.0, 0.055)), at + q @ Vector((0.0, 0.12, 0.05)), at + q @ Vector((0.0, 0.3, 0.03))]
    bits.tube(m, head, 0.022, "steel", 6 if m.fine else 4, radii=[0.006, 0.018, 0.03, 0.018, 0.008])


def bucket(m, at, tip=0.0):
    at = Vector(at)
    m.lathe(at, Z, [(0.0, 0.1), (0.25, 0.13), (0.255, 0.135)], "steel.worn", 14 if m.fine else 7, (True, False))
    m.lathe(at + Z * 0.248, -Z, [(0.0, 0.124), (0.24, 0.096)], "rust", 14 if m.fine else 7, (False, True))
    if m.fine:
        bits.tube(m, [at + Vector((0.13 * math.cos(math.radians(a)), 0.0, 0.25 + 0.12 * math.sin(math.radians(a)) * 0.9)) for a in range(0, 181, 20)], 0.004, "steel", 5)


def tins(m, at, count, seed=0, r=0.4):
    if not m.fine:
        return
    g = rnd(seed)
    at = Vector(at)
    for _ in range(count):
        a, d = g.uniform(0, 6.28), r * g.random()
        p = at + Vector((math.cos(a) * d, math.sin(a) * d, 0.0))
        if g.random() < 0.5:
            m.lathe(p, Z, [(0.0, 0.037), (0.11, 0.037)], g.choice(["steel.worn", "rust"]), 10, (True, False))
        else:
            ax = Vector((math.cos(a + 1), math.sin(a + 1), 0))
            m.lathe(p + Z * 0.037 - ax * 0.055, ax, [(0.0, 0.037), (0.11, 0.037)], g.choice(["steel.worn", "rust"]), 10, (True, False))


def grenade(m, at, yaw=0.0):
    """A plain egg-shaped hand grenade: body, fuse and lever."""
    at = Vector(at)
    q = Quaternion(Z, math.radians(yaw)) @ Quaternion(Y, math.radians(90))
    m.ball(at, (0.03, 0.03, 0.042), "upper2", 10, 6)
    tip = at + q @ Vector((0, 0, 0.042))
    m.rod(tip, at + q @ Vector((0, 0, 0.062)), 0.011, "steel", sides=6)
    m.box(at + q @ Vector((0.022, 0, 0.03)), (0.008, 0.014, 0.06), "steel", yaw=yaw)


def clods(m, boxes, count, seed=0, size=0.07):
    """Lumps of thrown earth lying about: `boxes` are (x0, y0, x1, y1) where they lie, `count` in all."""
    if not m.fine:
        count //= 4
    g = rnd(seed)
    for k in range(count):
        x0, y0, x1, y1 = boxes[k % len(boxes)]
        x, y = g.uniform(min(x0, x1), max(x0, x1)), g.uniform(min(y0, y1), max(y0, y1))
        r = size * g.uniform(0.4, 1.2)
        m.ball((x, y, r * 0.35), (r, r * g.uniform(0.6, 1.0), r * 0.6), g.choice(["earth", "earth.dry", "earth"]), 7 if m.fine else 4, 4 if m.fine else 2)


def roots(m, per, hx, hy, depth, seed=0):
    """Cut roots poking out of a hole's walls."""
    if not m.fine:
        return
    g = rnd(seed)
    for k in range(9):
        p, nn = per[g.randrange(len(per))]
        z = -g.uniform(0.1, 0.5) * depth
        a = p + Vector((0, 0, z)) + nn * 0.01
        tip = a - nn * g.uniform(0.05, 0.14) + Vector((g.uniform(-0.04, 0.04), g.uniform(-0.04, 0.04), -g.uniform(0.02, 0.08)))
        bits.tube(m, [a + nn * 0.04, a, a.lerp(tip, 0.5) + Vector((0, 0, 0.01)), tip], 0.008, "timber.dark", 5, radii=[0.009, 0.008, 0.006, 0.002])


def canteen(m, at, yaw=0.0):
    at = Vector(at)
    q = Quaternion(Z, math.radians(yaw))
    m.ball(at + Z * 0.09, (0.09, 0.045, 0.1), "canvas", 12 if m.fine else 6, 8 if m.fine else 4)
    m.rod(at + Z * 0.18, at + Z * 0.22, 0.015, "steel", sides=8)
    m.lathe(at + Z * 0.215, Z, [(0.0, 0.019), (0.02, 0.019), (0.022, 0.012)], "rubber", 8)


# ---- the foxhole -------------------------------------------------------------------------------------------------

def rr_perimeter(hx, hy, rc, step, breaks_y=()):
    """Points round a rounded rectangle (anticlockwise from the front's right end) with their outward facings; the
    front edge (x = +hx) gets a point at every y in `breaks_y`."""
    pts = []
    yy = hy - rc
    n = max(2, int(2 * yy / step))
    ys = sorted(set([-yy + 2 * yy * k / n for k in range(n + 1)] + list(breaks_y)))
    for y in ys[:-1]:
        pts.append((Vector((hx, y, 0)), X.copy()))
    corners = [((hx - rc, hy - rc), 0), ((-(hx - rc), hy - rc), 90), ((-(hx - rc), -(hy - rc)), 180), ((hx - rc, -(hy - rc)), 270)]
    na = 6
    for q, ((cx, cy), a0) in enumerate(corners):
        for k in range(na):
            a = math.radians(a0 + 90.0 * k / na)
            nn = Vector((math.cos(a), math.sin(a), 0))
            pts.append((Vector((cx, cy, 0)) + nn * rc, nn))
        # the straight edge after this corner (the front edge comes first, so it is left out here)
        if q == 0:
            ex = 2 * (hx - rc)
            ne = max(1, int(ex / step))
            for k in range(ne):
                pts.append((Vector((hx - rc - ex * k / ne, hy, 0)), Y.copy()))
        elif q == 1:
            ey = 2 * (hy - rc)
            ne = max(1, int(ey / step))
            for k in range(ne):
                pts.append((Vector((-hx, hy - rc - ey * k / ne, 0)), -X))
        elif q == 2:
            ex = 2 * (hx - rc)
            ne = max(1, int(ex / step))
            for k in range(ne):
                pts.append((Vector((-(hx - rc) + ex * k / ne, -hy, 0)), -Y))
    return pts


def leafy_branch(m, a, b, seed, leaves=36, paint="garnish"):
    """A cut branch lying where it was thrown: a crooked stem, side twigs, bunches of leaves."""
    g = rnd(seed)
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    d = (b - a).normalized()
    side = d.cross(Z).normalized()
    stem = [a.lerp(b, k / 5) + side * (0.04 * math.sin(k * 1.7 + seed)) + Z * (0.02 * math.sin(k * 2.3)) for k in range(6)]
    bits.tube(m, stem, 0.02, "timber.dark", 6 if m.fine else 3, radii=[0.022, 0.019, 0.016, 0.012, 0.009, 0.005])
    tips = []
    for t in range(4 if m.fine else 2):
        f = g.uniform(0.25, 0.85)
        p = a.lerp(b, f)
        sd = side * g.choice((-1, 1))
        q = p + (d * 0.4 + sd).normalized() * L * g.uniform(0.25, 0.4) + Z * g.uniform(0.0, 0.08)
        m.rod(p, q, 0.009, "timber.dark", rb=0.004, sides=5 if m.fine else 3)
        tips.append((p, q))
    tips.append((a.lerp(b, 0.5), b))
    if not m.fine:
        leaves //= 3
    for k in range(leaves):
        p, q = tips[k % len(tips)]
        c = p.lerp(q, g.uniform(0.3, 1.05)) + Vector((g.uniform(-0.07, 0.07), g.uniform(-0.07, 0.07), g.uniform(-0.01, 0.06)))
        ang = g.uniform(0, 6.28)
        u = Vector((math.cos(ang), math.sin(ang), g.uniform(-0.3, 0.5))).normalized()
        v = u.cross(Vector((g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 1.0))).normalized()
        ln, wd = g.uniform(0.07, 0.12), g.uniform(0.025, 0.04)
        pnt = g.choice([paint, paint, "garnish.3", "garnish.2"])
        m.add([c, c + u * ln * 0.5 + v * wd, c + u * ln, c + u * ln * 0.5 - v * wd], [(0, 1, 2, 3)], pnt, True, lambda mm: Z)


def sod(m, at, yaw, pitch, seed, size=(0.34, 0.27, 0.035)):
    """A cut sod: a ragged slab of earth, the turf uppermost with its grass still standing."""
    g = rnd(seed)
    at = Vector(at)
    L, W, H = size[0] * g.uniform(0.8, 1.15), size[1] * g.uniform(0.8, 1.15), size[2]
    q = Quaternion(Z, math.radians(yaw)) @ Quaternion(Y, math.radians(-pitch)) @ Quaternion(X, math.radians(g.uniform(-5, 5)))
    nn = 9 if m.fine else 5
    plan = []
    for k in range(nn):
        a = 2 * math.pi * k / nn
        c, s_ = math.cos(a), math.sin(a)
        e = 0.35
        rr = g.uniform(0.9, 1.06)
        plan.append((L / 2 * math.copysign(abs(c) ** e, c) * rr, W / 2 * math.copysign(abs(s_) ** e, s_) * rr))
    lo = [at + q @ Vector((x * 0.97, y * 0.97, -H / 2)) for x, y in plan]
    hi = [at + q @ Vector((x, y, H / 2 + 0.01 * g.uniform(-1, 1))) for x, y in plan]
    top = q @ Z
    mid_top = at + q @ Vector((0, 0, H / 2 + 0.012))
    m.add(lo + hi, [(k, (k + 1) % nn, nn + (k + 1) % nn, nn + k) for k in range(nn)], "earth", True, lambda p: p - at)
    m.add(hi + [mid_top], [(k, (k + 1) % nn, nn) for k in range(nn)], "turf", True, lambda p: top)
    if m.fine:
        for k in range(16):
            c = at + q @ Vector((g.uniform(-L / 2, L / 2) * 0.8, g.uniform(-W / 2, W / 2) * 0.8, H / 2 + 0.005))
            ang = g.uniform(0, 6.28)
            sd = Vector((math.cos(ang), math.sin(ang), 0)) * 0.006
            lean = Vector((math.cos(ang + 1.6), math.sin(ang + 1.6), 0)) * g.uniform(0.02, 0.05)
            m.add([c - sd, c + sd, c + lean + top * g.uniform(0.04, 0.09)], [(0, 1, 2)], g.choice(["garnish", "turf", "garnish.2", "garnish.3"]), True, None)


def bank_height(prof, off, along, L, fall=0.8):
    """The height of a bits.bank (without its lumps) at an offset from its line, `along` from its start."""
    h = 0.0
    pr = sorted(prof)
    if pr[0][0] <= off <= pr[-1][0]:
        for (o0, h0), (o1, h1) in zip(pr, pr[1:]):
            if o0 <= off <= o1:
                h = h0 + (h1 - h0) * (off - o0) / max(o1 - o0, 1e-9)
                break
    e = min(along, L - along)
    return h * smooth(e / fall)


def Foxhole(fine=True, nation="_default", name="Foxhole"):
    """A two-man hole, the spoil thrown up in front as a low parapet with an elbow rest left between, the parapet hidden
    under cut sods and branches; a grenade niche in the front wall, an ammunition box to hand."""
    m = bits.model(name, "Foxhole for two men (our own model)", "works", nation, fine, MORE)
    m.part("hole", None, (0, 0, 0), kind="root")
    m.use("hole")
    HX, HY, RC, D = 0.35, 0.9, 0.2, 1.2
    gy0, gy1, gz0, gz1 = -0.16, 0.12, -0.62, -0.42                  # the grenade niche, in the front wall
    per = rr_perimeter(HX, HY, RC, 0.05 if fine else 0.25, (gy0, gy0 + 0.012, gy1 - 0.012, gy1))
    # rings from the outer edge of the trodden apron in to the floor: (offset, z, tag)
    ring = [(1.5, 0.004, "edge"), (1.25, 0.014, "apron"), (1.0, 0.018, "apron"), (0.75, 0.02, "apron"), (0.55, 0.018, "apron"), (0.38, 0.016, "apron"),
            (0.25, 0.014, "lip"), (0.14, 0.01, "lip"), (0.07, -0.005, "lip"), (0.025, -0.04, "wall")]
    for z in (-0.18, -0.3, gz1, gz1, -0.52, gz0, gz0, -0.75, -0.9, -1.05, -1.14):
        ring.append((-0.035 * (-z) / D, z, "wall"))
    ring += [(-0.06, -D + 0.015, "floor"), (-0.1, -D, "floor"), (-0.14, -D, "floor")]
    if not fine:
        ring = [(1.5, 0.004, "edge"), (0.9, 0.018, "apron"), (0.38, 0.016, "apron"), (0.14, 0.01, "lip"), (0.025, -0.04, "wall"),
                (-0.012, -0.42, "wall"), (-0.024, -0.82, "wall"), (-0.035, -1.14, "wall"), (-0.1, -D, "floor")]
    amp = {"apron": 0.018, "lip": 0.02, "wall": 0.03, "floor": 0.012}
    niche_rows = [i for i, r in enumerate(ring) if r[1] in (gz0, gz1)]
    inner_niche = set(range(niche_rows[1], niche_rows[2] + 1)) if len(niche_rows) == 4 else set()
    verts = []
    for i, (o, z, tag) in enumerate(ring):
        for p, nn in per:
            if o > 0.45:      # (the trodden apron's edge is ragged, never a ruled shape)
                a = math.atan2(p.y, p.x * 2.0)
                o0 = ring[i][0]
                k_ = (o0 - 0.45) / 1.05
                o = 0.45 + (o0 - 0.45) * (1.0 + 0.6 * fbm((math.cos(a) * 1.3, math.sin(a) * 1.3, 2.0), 0.5)) * (1.0 + 0.3 * k_ * (nn.x > 0.5))
            else:
                o = ring[i][0]
            q = p + nn * o
            disp = 0.0
            if amp.get(tag):
                disp = amp[tag] * (0.7 * fbm(Vector((q.x, q.y, z)) + Vector((4.0, 0, 0)), 0.45) + 0.3 * fbm(Vector((q.x, q.y, z)) * 1.0 + Vector((0, 9.0, 0)), 0.12))
            v = Vector((q.x, q.y, z))
            if tag == "wall":
                v = v + nn * disp
                if i in inner_niche and abs(nn.x - 1.0) < 1e-6 and gy0 + 0.006 < p.y < gy1 - 0.006:
                    v = v + nn * 0.2                          # the grenade niche, cut 20 cm into the front wall
            else:
                v.z += disp
                if tag in ("apron", "lip") and nn.x > 0.5 and 0.2 < o < 0.45:
                    v.z -= 0.006                              # the elbow rest, smoothed by elbows
            verts.append(v)
    n = len(per)
    faces = []
    for i in range(len(ring) - 1):
        for j in range(n):
            faces.append((i * n + j, i * n + (j + 1) % n, (i + 1) * n + (j + 1) % n, (i + 1) * n + j))
    ftags = [(ring[f[0] // n][2], ring[f[3] // n][2]) for f in faces]
    c = len(verts)
    verts.append(Vector((0.0, 0.0, -D)))
    last = (len(ring) - 1) * n
    faces += [(last + j, last + (j + 1) % n, c) for j in range(n)]
    ftags += [("floor", "floor")] * n
    earth_skin(m, verts, faces, ftags, [ring[i // n][2] for i in range(c)] + ["floor"], 0.0, 2.0)

    # the parapet: the spoil thrown forward, an elbow's width from the hole; lower spoil to the sides
    line = [(1.0, -1.45), (1.08, -0.6), (1.1, 0.0), (1.08, 0.6), (1.0, 1.45)]
    prof = [(0.32, 0.0), (0.2, 0.1), (0.08, 0.22), (-0.08, 0.29), (-0.3, 0.27), (-0.6, 0.16), (-0.9, 0.06), (-1.2, 0.0)]
    bits.bank(m, line, prof, "earth.dry", seed=4, lump=0.025, step=0.12 if fine else 0.4, turf=0.22)
    for sy in (-1, 1):
        bits.bank(m, [(0.55, sy * 1.25), (-0.2, sy * 1.32), (-0.75, sy * 1.15)], [(0.3, 0.0), (0.12, 0.07), (-0.05, 0.1), (-0.3, 0.05), (-0.5, 0.0)] if sy > 0 else
                  [(0.5, 0.0), (0.3, 0.05), (0.05, 0.1), (-0.12, 0.07), (-0.3, 0.0)], "earth.dry", seed=6 + sy, lump=0.02, step=0.15 if fine else 0.45, turf=0.1)
    total = sum((Vector(b + (0,)) - Vector(a + (0,))).length for a, b in zip(line, line[1:]))

    def crest(x, y):
        """the parapet's height at a place (its line runs nearly along y at x about 1.08)"""
        lx = 1.0 + 0.1 * (1.0 - (y / 1.45) ** 2)
        along = (y + 1.45) / 2.9 * total
        return bank_height(prof, lx - x, along, total)

    # a few cut sods slumped over the crest where the spoil showed most, overlapping
    g = rnd(17)
    for k, (x, y) in enumerate([(1.22, -0.55), (1.12, -0.3), (1.3, 0.42), (1.16, 0.62)]):
        xx, yy = x + g.uniform(-0.04, 0.04), y + g.uniform(-0.04, 0.04)
        h = crest(xx, yy)
        slope = (crest(xx + 0.1, yy) - crest(xx - 0.1, yy)) / 0.2
        sod(m, (xx, yy, h - 0.005 + 0.012 * (k % 2)), g.uniform(-35, 35), math.degrees(math.atan(slope)), 100 + k)
    # branches laid over it
    m.part("cover", "hole", (1.25, 0.0, 0.3), kind="free", note="the branches laid over the parapet: a hit throws them off")
    m.use("cover")
    for k, (a, b) in enumerate([((1.7, -1.3), (0.95, -0.35)), ((1.75, 0.2), (1.0, -0.15)), ((1.55, 1.35), (0.9, 0.45)), ((1.9, -0.6), (1.35, 0.55))]):
        A = Vector((a[0], a[1], crest(*a) + 0.06))
        B = Vector((b[0], b[1], crest(*b) + 0.08))
        leafy_branch(m, A, B, 30 + k, 75)
    m.use("hole")
    # sandbags: two on the parapet's flanks to rest a rifle on, one dropped at the rear lip
    for k, (x, y, yaw) in enumerate([(0.78, -0.98, 78), (0.78, 0.96, 102)]):
        bits.sandbag(m, (x, y, crest(x, y) - 0.02), yaw, seed=50 + k, pitch=-6)
    bits.sandbag(m, (-0.72, 0.35, 0.0), 95, seed=53)
    # the grenade niche, with two grenades in it
    for k, y in enumerate((-0.07, 0.04)):
        grenade(m, (HX + 0.1, y, gz0 + 0.032), 90 + 20 * k)
    # an ammunition box open on the elbow rest between the two men, a spade stuck in the side spoil, empties on the floor
    ammo_box(m, (0.55, 0.05, -0.02), yaw=95, open_lid=True)
    shovel(m, (-0.3, -1.42, 0.0), (-0.42, -1.6, 0.72))
    spent(m, (0.0, 0.2, -D), 0.25, 18, seed=6, size=0.05)
    helmet(m, (-0.85, -0.6, 0.012), upside=True)
    clods(m, [(0.6, -1.3, 2.0, 0.9)], 34, seed=8)
    roots(m, per, HX, HY, D, seed=5)
    canteen(m, (0.5, -0.55, 0.0), 30)
    tins(m, (-0.9, 0.2, 0.014), 3, seed=4, r=0.3)

    for k, y in enumerate((0.42, -0.42)):
        bits.crew(m, str(k + 1), (-0.02, y, -D), (1, 0, 0), "stand", "hole", "standing on the floor of the hole, elbows on the elbow rest, firing over the parapet")
    m.socket("hit", (1.2, 0.0, 0.2), (1, 0, 0), part="hole", note="where a shell or grenade strikes the parapet")
    q = Quaternion(Vector((0.3, 1.0, 0.4)).normalized(), math.radians(140))
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Still.", "tracks": [], "events": []}
    m.clips["hit"] = {"long": 1.2, "loop": False, "after": "hold", "about": "A burst on the parapet: the branches are thrown off and land in front.",
                      "tracks": [{"part": "cover", "move": [[0.0, 0, 0, 0], [0.25, 0.5, 0.2, 1.1], [0.6, 1.3, 0.4, 0.9], [1.0, 1.9, 0.5, -0.25]],
                                  "rot": [[0.0, 1, 0, 0, 0], [0.5, *Quaternion().slerp(q, 0.5)], [1.0, q.w, q.x, q.y, q.z]]}],
                      "events": [{"t": 0.0, "fx": "shell_burst", "socket": "hit", "cue": "shell_impact_near"}]}
    dug_weather(m, lambda p: -D if abs(p.x) < HX + 0.1 and abs(p.y) < HY + 0.1 and p.z < -0.2 else 0.0)
    m.info["span"] = 3.4
    m.info["high"] = 0.2
    m.info["aim"] = (0.15, 0.0, -0.25)
    m.info["dig"] = [[(round(p.x + nn.x * 0.22, 3), round(p.y + nn.y * 0.22, 3)) for p, nn in per]]
    m.info["dig_depth"] = D
    m.info["clear"] = [[(round(v.x * 0.94, 3), round(v.y * 0.94, 3)) for v in verts[:len(per)]]]
    m.info["worn"] = [(-0.2, 0.0, 2.2), (0.6, 0.0, 1.6)]
    return m


# ---- fieldworks ----------------------------------------------------------------------------------------------------
# The cross-section every length shares (u toward the enemy, z up): a parados behind, the rear wall, a floor 0.9 m wide
# 2.0 m down, the fire step on the enemy side, the front wall, a berm the parapet's sandbags sit on, a low glacis.

FLOOR, STEP = -2.0, -1.08
U_RL, U_RB = -0.80, -0.65          # the rear wall at the lip, at the floor
U_FL, U_FS = 0.90, 0.82            # the front wall at the lip, at the step
U_R0, U_R1 = 0.25, 0.28            # the step's riser at the floor, at the tread
BERM, BAG_U = 0.05, 1.08           # the sandbags' seat, the line of their middles
FLOOR_U = -0.2                     # the middle of the floor
DOOR_TOP = -0.25
SHELF = -0.255                     # the machine-gun shelf
REC_PUSH = 1.75                    # how far the machine-gun recess is cut forward


def uf(z):
    return U_FS + (U_FL - U_FS) * (z - STEP) / (0.0 - STEP)


def ur(z):
    return U_RL + (U_RB - U_RL) * (-z) / (-FLOOR)


def bay_profile():
    P = [(-3.5, 0.004, "edge"), (-3.1, 0.02, "bank"), (-2.7, 0.08, "bank"), (-2.35, 0.17, "bank"), (-2.05, 0.25, "bank"), (-1.8, 0.29, "bank"),
         (-1.55, 0.26, "bank"), (-1.3, 0.16, "bank"), (-1.12, 0.07, "bank"), (-0.98, 0.03, "lip"), (-0.87, 0.014, "lip")]
    for z, tag in [(-0.04, "wall"), (-0.12, "wall"), (DOOR_TOP, "wall"), (-0.5, "wall"), (-0.75, "wall"), (-1.05, "wall"), (-1.05, "niche"), (-1.3, "niche"),
                   (-1.55, "niche"), (-1.55, "wall"), (-1.75, "wall"), (-1.9, "wall")]:
        P.append((ur(z) - (0.03 if z == -0.04 else 0.0), z, tag))
    P += [(U_RB + 0.035, FLOOR + 0.03, "floor"), (-0.56, FLOOR, "floor"), (-0.47, FLOOR, "floor"), (-0.45, FLOOR, "sump"), (FLOOR_U, FLOOR, "sump"),
          (0.05, FLOOR, "sump"), (0.07, FLOOR, "floor"), (0.18, FLOOR, "floor"), (U_R0 - 0.02, FLOOR + 0.02, "floor")]
    for z in (-1.8, -1.55, -1.3, STEP - 0.03):
        P.append((U_R0 + (U_R1 - U_R0) * (z - FLOOR) / (STEP - FLOOR), z, "riser"))
    P += [(U_R1 + 0.03, STEP, "tread"), (0.45, STEP, "tread"), (0.6, STEP, "tread"), (0.74, STEP, "tread"), (U_FS - 0.02, STEP + 0.02, "tread")]
    for z in (-0.85, -0.6, -0.4):
        P.append((uf(z), z, "fwall"))
    P += [(uf(SHELF), SHELF, "fwall"), (uf(SHELF), SHELF, "shelf"), (uf(-0.13), -0.13, "shelf"), (uf(-0.04), -0.04, "shelf"),
          (U_FL + 0.035, 0.03, "flip"), (0.97, BERM, "berm"), (1.1, BERM, "berm"), (1.24, BERM, "berm"),
          (1.3, 0.13, "glacis"), (1.45, 0.18, "glacis"), (1.75, 0.17, "glacis"), (2.2, 0.12, "glacis"), (2.75, 0.06, "glacis"), (3.3, 0.022, "glacis"),
          (3.9, 0.004, "edge")]
    return P


BAY_AMP = {"bank": 0.06, "lip": 0.012, "wall": 0.035, "niche": 0.035, "floor": 0.018, "sump": 0.018, "riser": 0.006, "tread": 0.012,
           "fwall": 0.01, "shelf": 0.01, "flip": 0.008, "berm": 0.0, "glacis": 0.045}


def swell(s, u, z, tag):
    """Banks rise and fall along their length (thrown by different men on different days)."""
    if tag in ("bank", "glacis"):
        return z * (0.45 * fbm(Vector((s * 0.35, u * 0.2, 4.0)), 1.0, 2) + 0.15 * fbm(Vector((s * 1.3, u * 0.7, 9.0)), 1.0, 2))
    return 0.0


def sample(path, s0, s1, u, z=0.0, step=0.25):
    n = max(1, int(math.ceil((s1 - s0) / step)))
    return [path.at(s0 + (s1 - s0) * k / n, u, z) for k in range(n + 1)]


def bag_run(m, path, s0, s1, u, z, seed, half=False, lean=0.0):
    """One course of sandbags laid along the path between s0 and s1."""
    if s1 - s0 < 0.2:
        return
    pts = [(p.x, p.y) for p in sample(path, s0, s1, u)]
    bits.bag_row(m, pts, z, seed, bits.BAG, half_step=half, lean=lean)


def plank_revet(m, path, s0, s1, seed, top=0.0, posts=True):
    """Front wall held by round posts with sawn planks laid behind them, nailed; each post wired back into the
    parapet."""
    g = rnd(seed)
    n = max(1, int(round((s1 - s0) / 1.1)))
    ps = [s0 + 0.07 + (s1 - s0 - 0.14) * k / n for k in range(n + 1)]
    hs = []
    z = STEP + 0.01
    while z < top - 0.08:
        w = g.uniform(0.17, 0.22) if m.fine else (top - STEP - 0.02)
        hs.append((z + w / 2, w))
        z += w + g.uniform(0.006, 0.02)
    for a, b in zip(ps, ps[1:]):
        for k, (zc, w) in enumerate(hs):
            if g.random() < 0.05:
                continue                              # a plank gone: the earth shows
            tilt = g.uniform(-0.012, 0.012)
            sa, sb = a - 0.06 * (a > s0 + 0.1), b + 0.06 * (b < s1 - 0.1)
            A = path.at(sa, uf(zc) - 0.016, zc + tilt)
            B = path.at(sb, uf(zc) - 0.016, zc - tilt)
            _, r = path.axes((sa + sb) / 2, 0.85)
            bits.plank(m, A, B, w, 0.028, g.choice(["planks", "planks", "timber"]), -r, seed=g.randrange(999))
    for k, sp in enumerate(ps):
        if not posts:
            break
        r_ = 0.05 * g.uniform(0.9, 1.1)
        u0 = uf(STEP) - 0.032 - r_
        bits.log(m, path.at(sp, u0, STEP - 0.03), path.at(sp, uf(top + 0.1) - 0.032 - r_, top + g.uniform(0.06, 0.14)), r_, "timber", seed + k, taper=0.93)
        _, r = path.axes(sp, 0.85)
        for zc, w in hs:
            bits.bolt(m, path.at(sp, uf(zc) - 0.032 - 2 * r_, zc + 0.03), -r, 0.007, 0.006, "iron")
            bits.bolt(m, path.at(sp, uf(zc) - 0.032 - 2 * r_, zc - 0.04), -r, 0.007, 0.006, "iron")
        if m.fine and top > -0.1:
            t = path.at(sp, uf(0.05) - 0.03 - r_, top + 0.03)
            bits.tube(m, [t + Vector((0, 0, 0.0)), path.at(sp, uf(0.05) + 0.02, top + 0.06), path.at(sp, U_FL + 0.06, BERM + 0.015), path.at(sp + 0.02, U_FL + 0.14, BERM - 0.01)],
                      0.004, "wire", 5)


def hurdle_revet(m, path, s0, s1, seed):
    """Front wall held by hurdles: stakes driven at the foot with withies woven between them."""
    g = rnd(seed)
    n = max(2, int(round((s1 - s0) / 0.36)))
    st = [s0 + 0.04 + (s1 - s0 - 0.08) * k / n for k in range(n + 1)]
    for k, sp in enumerate(st):
        tz = g.uniform(0.0, 0.12)
        bits.log(m, path.at(sp, uf(STEP) - 0.06, STEP - 0.03), path.at(sp, uf(tz) - 0.06, tz), 0.026, "timber.dark", seed * 31 + k, sides=7 if m.fine else 4, taper=0.85)
    dz = 0.034 if m.fine else 0.11
    z = STEP + 0.03
    row = 0
    while z < -0.03:
        pts = []
        wob = g.uniform(0, 6.0)
        ext = [st[0] - g.uniform(0.02, 0.08)] + st + [st[-1] + g.uniform(0.02, 0.08)]
        for k, sp in enumerate(ext):
            side = (-1) ** (k + row)
            zz = z + 0.006 * math.sin(k * 1.3 + wob)
            pts.append(path.at(sp, uf(zz) - 0.06 + side * 0.03, zz))
            if k < len(ext) - 1:
                sm = (sp + ext[k + 1]) / 2
                pts.append(path.at(sm, uf(zz) - 0.06, zz + 0.003 * math.sin(k + wob)))
        bits.tube(m, pts, 0.011 if m.fine else 0.03, g.choice(["timber.dark", "timber.dark", "timber", "garnish.2"]), 5 if m.fine else 3)
        z += dz * g.uniform(0.9, 1.1)
        row += 1


def iron_revet(m, path, s0, s1, seed):
    """Front wall held by sheets of corrugated iron behind angle-iron pickets, rusted, one with bullet holes."""
    g = rnd(seed)
    n = max(1, int(round((s1 - s0) / 0.66)))
    w = (s1 - s0) / n
    for k in range(n):
        a, b = s0 + k * w - 0.03 * (k > 0), s0 + (k + 1) * w + 0.03 * (k < n - 1)
        cols = max(4, int((b - a) / (0.019 if m.fine else 0.076)))
        zs = [STEP - 0.02 + (1.1 + 0.03) * j / (5 if m.fine else 2) for j in range((5 if m.fine else 2) + 1)]
        bulge = g.uniform(0.0, 0.035)
        front, back = [], []
        for j, z in enumerate(zs):
            for c in range(cols + 1):
                sc = a + (b - a) * c / cols
                cor = 0.0095 * math.sin(2 * math.pi * (sc - a) / 0.076) if m.fine else 0.0
                f = (z - STEP) / 1.1
                du = -bulge * math.sin(math.pi * f) - (0.03 * smooth((f - 0.9) / 0.1)) + 0.01 * fbm(Vector((sc, z, seed)), 0.15)
                front.append(path.at(sc, uf(z) - 0.02 + cor + du, z))
                back.append(path.at(sc, uf(z) - 0.016 + cor + du, z))
        nc = cols + 1
        fs = [(j * nc + c, (j + 1) * nc + c, (j + 1) * nc + c + 1, j * nc + c + 1) for j in range(len(zs) - 1) for c in range(cols)]
        _, r = path.axes((a + b) / 2, 0.85)
        m.add(front, fs, lambda mid, nn: "rust" if fbm(mid * 3.0 + Vector((seed, 0, 0)), 0.5) > -0.15 else "steel.worn", True, lambda mid: -r)
        m.add(back, fs, "rust", True, lambda mid: r)
        if m.fine and k == 0:
            for h in range(3):
                c = path.at(a + g.uniform(0.15, 0.5), uf(-0.5) - 0.03, g.uniform(-0.75, -0.25))
                m.disc(c, -r, 0.007, "void", 6)
    for k in range(n + 1):
        sp = s0 + k * w + (0.04 if k == 0 else -0.04 if k == n else 0.0)
        A = path.at(sp, uf(STEP) - 0.05, STEP - 0.04)
        B = path.at(sp, uf(0.1) - 0.05, 0.12)
        _, r = path.axes(sp, 0.85)
        bits.angle_iron(m, A, B, 0.045, 0.006, "steel" if m.fine else "rust", -r)
        if m.fine:
            bits.tube(m, [B - Z * 0.03, path.at(sp, U_FL + 0.02, 0.07), path.at(sp, U_FL + 0.12, BERM + 0.012)], 0.004, "wire", 5)


def riser_revet(m, path, s0, s1, seed):
    """The fire step's face boarded, held by pegs, with a plank laid along its edge."""
    g = rnd(seed)
    s = s0
    while s < s1 - 0.1:
        e = min(s1, s + g.uniform(2.0, 2.8))
        for k in range(4 if m.fine else 0):
            zc = FLOOR + 0.12 + k * 0.225
            u = U_R0 + (U_R1 - U_R0) * (zc - FLOOR) / (STEP - FLOOR) - 0.016
            _, r = path.axes((s + e) / 2, 0.3)
            bits.plank(m, path.at(s + 0.01, u, zc + g.uniform(-0.01, 0.01)), path.at(e - 0.01, u, zc), 0.21, 0.026, "planks", -r, seed=g.randrange(999))
        n = max(1, int((e - s) / 1.2))
        for k in range(n + 1 if m.fine else 0):
            sp = s + 0.06 + (e - s - 0.12) * k / n
            bits.log(m, path.at(sp, U_R0 - 0.06, FLOOR - 0.01), path.at(sp, U_R1 - 0.065, STEP - 0.05), 0.03, "timber", g.randrange(999), sides=7 if m.fine else 4)
        bits.plank(m, path.at(s + 0.01, U_R1 + 0.09, STEP + 0.014), path.at(e - 0.01, U_R1 + 0.09, STEP + 0.014), 0.2, 0.028, "planks", Z, seed=g.randrange(999))
        s = e + 0.004


def duckboards(m, path, s0, s1, seed, gaps=()):
    """Duckboards on the floor: slats on two runners, in sections, some askew, a slat gone here and there."""
    g = rnd(seed)
    s = s0
    while s < s1 - 0.3:
        e = min(s1, s + 1.85)
        for a, b in gaps:
            if s < b and e > a:
                if s < a - 0.3:
                    e = a - 0.03
                else:
                    s = b + 0.03
                    e = min(s1, s + 1.85)
        if e - s < 0.3:
            s = e + 0.03
            continue
        lift = FLOOR + 0.02 + g.uniform(0.0, 0.025)
        tilt = g.uniform(-0.02, 0.02)
        du = g.uniform(-0.04, 0.04)
        for side in (-0.22, 0.22):
            bits.plank(m, path.at(s + 0.04, FLOOR_U + side + du, lift + 0.02), path.at(e - 0.04, FLOOR_U + side + du, lift + 0.02 + tilt), 0.05, 0.045, "timber", Z, seed=g.randrange(999))
        if not m.fine:
            bits.plank(m, path.at(s + 0.04, FLOOR_U + du, lift + 0.056), path.at(e - 0.04, FLOOR_U + du, lift + 0.056 + tilt), 0.58, 0.022, "planks", Z)
            s = e + 0.04
            continue
        n = int((e - s) / 0.105)
        for k in range(n):
            sc = s + 0.04 + (e - s - 0.08) * (k + 0.5) / n
            if g.random() < 0.04:
                continue
            zz = lift + 0.045 + 0.011 + tilt * (sc - s) / (e - s)
            sk = g.uniform(-0.015, 0.015)
            bits.plank(m, path.at(sc - sk, FLOOR_U - 0.29 + du, zz), path.at(sc + sk, FLOOR_U + 0.29 + du, zz), 0.07, 0.022, g.choice(["planks", "planks", "timber"]), Z, seed=g.randrange(999))
        s = e + g.uniform(0.02, 0.05)


def ladder(m, path, s, seed=0):
    """A short ladder leaning against the front wall from the fire step to over the parapet."""
    A0, A1 = path.at(s - 0.21, 0.42, STEP), path.at(s + 0.21, 0.42, STEP)
    B0, B1 = path.at(s - 0.21, U_FL + 0.02, 0.42), path.at(s + 0.21, U_FL + 0.02, 0.42)
    _, r = path.axes(s, 0.6)
    for a, b in ((A0, B0), (A1, B1)):
        bits.plank(m, a, b + (b - a).normalized() * 0.15, 0.075, 0.04, "timber", -r, seed=seed)
    n = 5
    for k in range(n):
        f = (k + 0.7) / (n + 0.5)
        m.rod(A0.lerp(B0, f) - Y * 0.0, A1.lerp(B1, f), 0.017, "timber", sides=7)


def niche(m, path, s, seed=0):
    """An ammunition niche cut in the rear wall: boarded shelf, lintel and sides, boxes stacked in it."""
    _, r = path.axes(s, -0.7)
    bits.plank(m, path.at(s - 0.47, ur(-1.55) - 0.2, -1.55 + 0.013), path.at(s + 0.47, ur(-1.55) - 0.2, -1.55 + 0.013), 0.48, 0.025, "planks", Z, seed=seed)
    bits.log(m, path.at(s - 0.7, ur(-1.02) + 0.02, -1.0), path.at(s + 0.7, ur(-1.02) + 0.02, -1.0), 0.06, "timber", seed + 1)
    for sd in (-1, 1):
        bits.plank(m, path.at(s + sd * 0.46, ur(-1.3) - 0.22, -1.55), path.at(s + sd * 0.46, ur(-1.3) - 0.22, -1.05), 0.42, 0.025, "planks", path.axes(s)[0] * -sd, seed=seed + 2)
    crate(m, path.at(s - 0.18, ur(-1.4) - 0.23, -1.537), 0, seed=seed + 3)
    crate(m, path.at(s - 0.15, ur(-1.4) - 0.24, -1.335), 4, seed=seed + 4)
    ammo_box(m, path.at(s + 0.25, ur(-1.4) - 0.22, -1.537), yaw=90, open_lid=False)


def phone_wire(m, path, s0, s1, avoid=(), z=-0.55):
    """A telephone wire hooked along the rear wall on pegs, sagging between them."""
    if not m.fine:
        return
    pegs = []
    s = s0 + 0.3
    while s < s1 - 0.2:
        if not any(a - 0.2 < s < b + 0.2 for a, b in avoid):
            pegs.append(s)
        s += 1.4
    pts = []
    for a, b in zip(pegs, pegs[1:]):
        over = any(a < x0 and b > x1 for x0, x1 in avoid)       # (carried up over a doorway, along the wall's top)
        for k in range(8):
            f = k / 8
            zz = z - 0.07 * math.sin(math.pi * f)
            if over:
                zz = z + (0.0 - 0.04 - z) * smooth(min(f, 1 - f) / 0.3)
            pts.append(path.at(a + (b - a) * f, ur(zz) + 0.05, zz))
    if pegs:
        pts.append(path.at(pegs[-1], ur(z) + 0.05, z))
    if len(pts) > 1:
        bits.tube(m, pts, 0.004, "rubber", 4)
    for sp in pegs:
        m.rod(path.at(sp, ur(z) - 0.05, z + 0.02), path.at(sp, ur(z) + 0.08, z - 0.01), 0.012, "timber", sides=6)


def loophole_plate(m, path, s):
    """A steel loophole plate set in the sandbags, its slot shuttered half open."""
    _, r = path.axes(s, BAG_U)
    c = path.at(s, BAG_U - 0.11, BERM + 0.116 + 0.12)
    al, _ = path.axes(s, BAG_U)
    bits.plate(m, [(-0.27, -0.125), (0.27, -0.125), (0.27, 0.11), (0.2, 0.125), (-0.2, 0.125), (-0.27, 0.11)], 0.012, "steel", c, al, Z)
    m.add([c - r * 0.0075 + al * sx * 0.03 + Z * sz * 0.055 for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))], [(0, 1, 2, 3)], "void", False, lambda p: -r)
    bits.plate(m, [(-0.05, -0.065), (0.05, -0.065), (0.05, 0.065), (-0.05, 0.065)], 0.008, "steel.worn", c - r * 0.012 + al * 0.075, al, Z)
    bits.bolt(m, c - r * 0.018 + al * 0.075 + Z * 0.05, -r, 0.01, 0.01, "steel")
    for sx in (-1, 1):
        for sz in (-1, 1):
            bits.bolt(m, c - r * 0.007 + al * sx * 0.22 + Z * sz * 0.09, -r, 0.012, 0.008, "steel")


def dugout(m, path, s, seed=0):
    """A dugout behind the bay: a timber-framed doorway in the rear wall with a gas curtain rolled over it, steps going
    down a boarded passage, the chamber's roof of logs under a mound of earth a metre high, and its stove pipe."""
    g = rnd(seed)
    t, r = path.axes(s, -1.0)
    hw = 0.4
    # the passage: steps down, boarded sides and roof, frames every half metre, dark beyond
    steps = []
    u = ur(-1.0) + 0.02
    z = FLOOR
    for k in range(6):
        u2 = u - 0.27
        z2 = z - 0.19
        steps.append((u, u2, z2))
        u, z = u2, z2
    for k, (ua, ub, zz) in enumerate(steps):
        lo = path.at(s, ub, zz - 0.4)
        m.box((path.at(s, (ua + ub) / 2, zz - 0.2)), (abs(ua - ub) + 0.01, 2 * hw, 0.4), "earth", yaw=math.degrees(math.atan2(t.y, t.x)) - 90)
        bits.plank(m, path.at(s - hw + 0.02, ua - 0.02, zz + 0.012), path.at(s + hw - 0.02, ua - 0.02, zz + 0.012), 0.05, 0.03, "timber", Z, seed=k)
    u_end = steps[-1][1]

    def ceil(u):
        return DOOR_TOP if u > -1.0 else DOOR_TOP - (-1.0 - u) * 0.7
    def floor_(u):
        return FLOOR if u > steps[0][0] else FLOOR - 0.19 * min(6, (steps[0][0] - u) / 0.27 + 0.5)
    us = [ur(-1.0) + 0.03 - 0.1 * k for k in range(int((ur(-1.0) + 0.03 - u_end) / 0.1) + 1)]
    for sd in (-1, 1):
        wall = []
        for uu in us:
            wall += [path.at(s + sd * hw, uu, floor_(uu) - 0.05), path.at(s + sd * hw, uu, ceil(uu))]
        nn = len(us)
        m.add(wall, [(2 * k, 2 * k + 1, 2 * k + 3, 2 * k + 2) for k in range(nn - 1)], "planks", False, lambda p, sd=sd: t * -sd)
    roof = []
    for uu in us:
        roof += [path.at(s - hw, uu, ceil(uu)), path.at(s + hw, uu, ceil(uu))]
    m.add(roof, [(2 * k, 2 * k + 1, 2 * k + 3, 2 * k + 2) for k in range(len(us) - 1)], "timber.dark", False, lambda p: -Z)
    m.add([path.at(s - hw, u_end, floor_(u_end) - 0.1), path.at(s + hw, u_end, floor_(u_end) - 0.1), path.at(s + hw, u_end, ceil(u_end)), path.at(s - hw, u_end, ceil(u_end))],
          [(0, 1, 2, 3)], "void", False, lambda p: r)
    # a dark shell round the whole passage, so no light comes in where boards meet
    c0 = path.at(s, (u_end + ur(-1.0)) / 2 - 0.05, (FLOOR - 1.4 - 0.02) / 2)
    hx_, hy_, hz_ = (ur(-1.0) - u_end) / 2 + 0.1, hw + 0.15, (1.4 + 2.0 - 0.02) / 2 + 0.05
    shell = [c0 + r * (sx * hx_) + t * (sy * hy_) + Z * (sz * hz_) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
    m.add(shell, [(0, 1, 3, 2), (1, 5, 7, 3), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4)], "void", False, lambda p: c0 - p)
    for k, uu in enumerate(us[2::5]):
        for sd in (-1, 1):
            bits.log(m, path.at(s + sd * (hw - 0.05), uu, floor_(uu) - 0.02), path.at(s + sd * (hw - 0.05), uu, ceil(uu)), 0.05, "timber", seed + k * 2 + sd, sides=8 if m.fine else 4)
        bits.log(m, path.at(s - hw, uu, ceil(uu) - 0.05), path.at(s + hw, uu, ceil(uu) - 0.05), 0.05, "timber", seed + k, sides=8 if m.fine else 4)
    # the doorway's frame: squared posts, a heavy lintel log, a sill; boards over the earth above
    for sd in (-1, 1):
        bits.plank(m, path.at(s + sd * (hw + 0.07), ur(-1.0) + 0.05, FLOOR - 0.02), path.at(s + sd * (hw + 0.07), ur(-0.2) + 0.05, DOOR_TOP + 0.08), 0.14, 0.14, "timber", r, seed=seed + sd)
    bits.log(m, path.at(s - hw - 0.4, ur(DOOR_TOP) + 0.06, DOOR_TOP + 0.08), path.at(s + hw + 0.4, ur(DOOR_TOP) + 0.06, DOOR_TOP + 0.08), 0.085, "timber", seed + 5)
    bits.plank(m, path.at(s - hw - 0.1, ur(-1.9) + 0.06, FLOOR + 0.02), path.at(s + hw + 0.1, ur(-1.9) + 0.06, FLOOR + 0.02), 0.14, 0.05, "timber.dark", Z, seed=seed + 6)
    for k in range(2):
        zc = DOOR_TOP + 0.2 + k * 0.13
        bits.plank(m, path.at(s - hw - 0.3, ur(zc) + 0.012, zc), path.at(s + hw + 0.3, ur(zc) + 0.012, zc), 0.12, 0.025, "planks", r, seed=seed + 7 + k)
    # the gas curtain: a blanket rolled up under the lintel, tied with two cords
    m.rod(path.at(s - hw + 0.02, ur(DOOR_TOP) + 0.15, DOOR_TOP - 0.03), path.at(s + hw - 0.02, ur(DOOR_TOP) + 0.15, DOOR_TOP - 0.03), 0.055, "canvas", sides=12 if m.fine else 5)
    if m.fine:
        for f in (-0.25, 0.25):
            c = path.at(s + f, ur(DOOR_TOP) + 0.15, DOOR_TOP - 0.03)
            m.lathe(c - t * 0.008, t, [(0.0, 0.058), (0.016, 0.058)], "rubber", 12, (False, False))
    # the roof: logs across the chamber, their ends showing at the mound's foot, the mound over them
    def mound_h(ss, uu):
        a = abs((ss - s) / 2.0) ** 4 + abs((uu + 3.25) / 2.3) ** 4
        return 1.05 * max(0.0, 1.0 - a) ** 0.5
    for k in range(12):
        uu = -1.42 - k * 0.25
        e = 2.0 + g.uniform(-0.08, 0.12)
        bits.log(m, path.at(s - e, uu, 0.1), path.at(s + e + g.uniform(-0.1, 0.05), uu + g.uniform(-0.03, 0.03), 0.1), 0.11 * g.uniform(0.85, 1.1), "timber", seed + 20 + k,
                 sides=10 if m.fine else 5)
    ns, nu = (36, 32) if m.fine else (10, 9)
    verts, faces = [], []
    for i in range(ns + 1):
        ss = s - 2.3 + 4.6 * i / ns
        for j in range(nu + 1):
            uu = -0.92 - 4.7 * j / nu
            h = mound_h(ss, uu)
            p = path.at(ss, uu, 0.0)
            h += 0.06 * fbm(p * 1.0 + Vector((5.0, 0, 0)), 0.5) * min(1.0, h * 4)
            verts.append(path.at(ss, uu, max(h, 0.0) + (0.012 if h > 0 else -0.03)))
    for i in range(ns):
        for j in range(nu):
            f = (i * (nu + 1) + j, (i + 1) * (nu + 1) + j, (i + 1) * (nu + 1) + j + 1, i * (nu + 1) + j + 1)
            if max(verts[c].z for c in f) > 0.02:
                faces.append(f)
    bits.turfed(m, verts, faces, amount=0.35, base="earth.dry", low=0.08, seed=3.0)
    # sandbags round the stove pipe's foot, and the pipe: a bend and a hat
    pc = path.at(s + 0.7, -3.0, 0.0)
    for k in range(4):
        a = k * 90 + 20
        q = pc + Vector((math.cos(math.radians(a)) * 0.3, math.sin(math.radians(a)) * 0.3, 0.0))
        sq, uq = path.local(q)
        bits.sandbag(m, q + Z * (mound_h(sq, uq) - 0.035), a + 90, seed=seed + 40 + k, size=(0.44, 0.24, 0.11))
    top = pc + Z * 1.75
    m.lathe(pc + Z * 0.6, Z, [(0.0, 0.065), (1.15, 0.065)], "rust", 14 if m.fine else 6, (False, False))
    m.lathe(pc + Z * 0.8, Z, [(0.0, 0.069), (0.03, 0.069)], "steel", 14 if m.fine else 6, (False, False))
    m.lathe(pc + Z * 1.35, Z, [(0.0, 0.069), (0.03, 0.069)], "steel", 14 if m.fine else 6, (False, False))
    m.lathe(top - Z * 0.02, Z, [(0.0, 0.02), (0.03, 0.15), (0.11, 0.015)], "steel", 14 if m.fine else 6, (True, True))
    for k in range(3):
        a = math.radians(k * 120)
        m.rod(pc + Z * 1.72 + Vector((math.cos(a) * 0.06, math.sin(a) * 0.06, 0)), top + Vector((math.cos(a) * 0.1, math.sin(a) * 0.1, 0.0)), 0.005, "steel", sides=4)
    m.disc(pc + Z * 1.744, Z, 0.06, "void", 12)
    return top


def bay_crew(m, path, root, positions, mg=None):
    for k, (s, note) in enumerate(positions):
        _, r = path.axes(s, 0.6)
        bits.crew(m, "rifleman." + str(k + 1), path.at(s, 0.6, STEP), r, "stand", root, note or "standing on the fire step, firing over the parapet")


def fieldworks(m, path, plan):
    """A length of fire bay along a path. `plan` says where the features stand along it (s values)."""
    fine = m.fine
    L = path.L
    root = m.parts[0].name
    mg, nch, door, lh, sump, lad, spill = (plan.get(k) for k in ("mg", "niche", "door", "loophole", "sump", "ladder", "spill"))
    rec = (mg - 0.75, mg + 0.75) if mg is not None else None
    breaks = []
    if rec:
        breaks.append(rec)
    if nch is not None:
        breaks.append((nch - 0.45, nch + 0.45))
    if door is not None:
        breaks.append((door - 0.4, door + 0.4))
    if sump is not None:
        breaks.append((sump - 0.26, sump + 0.26))
    prof = bay_profile()
    k_shelf = next(i for i, p in enumerate(prof) if p[2] == "shelf")
    k_door0 = next(i for i, p in enumerate(prof) if p[2] == "wall" and abs(p[1] - DOOR_TOP) < 1e-6)
    k_floor = next(i for i, p in enumerate(prof) if p[2] == "floor")
    k_glacis = next(i for i, p in enumerate(prof) if p[2] == "glacis")

    def inside(s, ab):
        return ab is not None and ab[0] + 0.005 < s < ab[1] - 0.005

    def push(s, k, u, z, tag):
        du = dz = 0.0
        if rec and inside(s, rec) and k >= k_shelf and tag not in ("glacis", "edge"):
            du = REC_PUSH
        if rec and inside(s, rec) and tag == "glacis":
            du = max(0.0, 3.05 + 0.08 * (k - k_glacis) - u)       # (the glacis crowds up against the recess's own bags)
        if nch is not None and tag == "niche" and inside(s, (nch - 0.45, nch + 0.45)):
            du = -0.45
        if sump is not None and tag == "sump" and inside(s, (sump - 0.26, sump + 0.26)):
            dz = -0.45
        return du, dz

    def skip(s, k):
        return door is not None and inside(s, (door - 0.4, door + 0.4)) and k_door0 <= k < k_floor

    rows = rows_for(L, 0.1 if fine else 0.5, breaks)
    sweep(m, path, prof, rows, BAY_AMP, 0.25, 1.7, push, skip, groove=swell)
    if sump is not None:
        c = path.at(sump, FLOOR_U, FLOOR - 0.3)
        al, r = path.axes(sump, 0.0)
        ring = [c + al * (0.24 * math.cos(a)) * (1 + 0.05 * math.sin(3 * a)) + r * (0.24 * math.sin(a)) for a in [2 * math.pi * k / 16 for k in range(16)]]
        m.add(ring + [c], [(k, (k + 1) % 16, 16) for k in range(16)], "glass", True, lambda p: Z)
        for sd in (-1, 1):
            bits.plank(m, path.at(sump + sd * 0.29, FLOOR_U - 0.33, FLOOR + 0.02), path.at(sump + sd * 0.29, FLOOR_U + 0.33, FLOOR + 0.02), 0.07, 0.035, "timber.dark", Z, seed=sd)
        bucket(m, path.at(sump + 0.45, -0.52, FLOOR + 0.0))
    # revetments: the front wall in stretches of different stuff, the step's face boarded
    for a, b, kind in plan["revet"]:
        if kind == "planks":
            if rec and a < rec[1] and b > rec[0]:
                plank_revet(m, path, a, rec[0], 11 + int(a * 7))
                plank_revet(m, path, rec[0] - 0.05, rec[1] + 0.05, 12, top=SHELF - 0.01, posts=False)
                plank_revet(m, path, rec[1], b, 13 + int(b * 7))
            else:
                plank_revet(m, path, a, b, 14 + int(a * 7))
        elif kind == "hurdle":
            hurdle_revet(m, path, a, b, 21 + int(a * 7))
        elif kind == "iron":
            iron_revet(m, path, a, b, 31 + int(a * 7))
    riser_revet(m, path, 0.0, L, 41)
    duckboards(m, path, 0.0, L, 51, [(sump - 0.3, sump + 0.3)] if sump is not None else [])
    # the parapet: two courses of sandbags on the berm, a third over the loophole; the machine-gun recess's own
    # wall of three courses across its front
    runs1, runs2 = [(0.0, L)], [(0.0, L)]

    def cut(runs, a, b):
        out = []
        for x0, x1 in runs:
            if b <= x0 or a >= x1:
                out.append((x0, x1))
            else:
                if a > x0:
                    out.append((x0, a))
                if b < x1:
                    out.append((b, x1))
        return out
    if rec:
        runs1 = cut(runs1, *rec)
        runs2 = cut(runs2, *rec)
    if lh is not None:
        runs2 = cut(runs2, lh - 0.27, lh + 0.27)
    if spill:
        runs2 = cut(runs2, *spill)
    g = rnd(61)
    for a, b in runs1:
        bag_run(m, path, a, b, BAG_U, BERM, g.randrange(1 << 30))
    for a, b in runs2:
        bag_run(m, path, a, b, BAG_U, BERM + 0.116, g.randrange(1 << 30), True, -0.012)
    if lh is not None:
        for a, b in ((lh - 0.8, lh - 0.27), (lh - 0.27, lh + 0.27), (lh + 0.27, lh + 0.8)):
            bag_run(m, path, a, b, BAG_U, BERM + 0.232 + (0.125 if abs((a + b) / 2 - lh) < 0.01 else 0.0), g.randrange(1 << 30), False, -0.024)
        loophole_plate(m, path, lh)
        spent(m, path.at(lh, 0.55, STEP), 0.2, 14, seed=9)
    if spill:
        m.part("spill", root, path.at((spill[0] + spill[1]) / 2, BAG_U, BERM + 0.17), kind="free", note="parapet bags a hit throws into the bay")
        m.use("spill")
        bag_run(m, path, spill[0], spill[1], BAG_U, BERM + 0.116, 77, True, -0.012)
        m.use(root)
    names = None
    if rec:
        RU = BAG_U + REC_PUSH
        bag_run(m, path, rec[0] - 0.22, rec[1] + 0.22, RU, BERM, 81)
        bag_run(m, path, rec[0] - 0.22, mg - 0.12, RU, BERM + 0.116, 82, True, -0.01)
        bag_run(m, path, mg + 0.12, rec[1] + 0.22, RU, BERM + 0.116, 83, True, -0.01)
        al, r = path.axes(mg, RU)
        bits.plank(m, path.at(mg - 0.4, RU - 0.02, BERM + 0.245), path.at(mg + 0.4, RU - 0.02, BERM + 0.245), 0.3, 0.035, "timber", Z, seed=4)
        bag_run(m, path, rec[0] - 0.22, rec[1] + 0.22, RU, BERM + 0.265, 84, False, -0.02)
        for k, f in enumerate((-0.5, 0.5)):
            bits.plank(m, path.at(mg + f, uf(SHELF) + 0.05, SHELF + 0.014), path.at(mg + f, uf(SHELF) + REC_PUSH - 0.1, SHELF + 0.014), 0.2, 0.028, "planks", Z, seed=90 + k)
        P = path.at(mg, 1.85, SHELF)
        names = machine_gun(m, P, root)
        m.use(root)
        feed = names["trunnion"] + Vector((-0.085, 0.10, 0.03))
        ammo_box(m, path.at(mg + 0.45, 1.25, SHELF + 0.028), yaw=8, belt_to=feed)
        ammo_box(m, path.at(mg - 0.48, 1.35, SHELF + 0.028), yaw=-5, open_lid=False)
        spent(m, path.at(mg - 0.25, 1.5, SHELF + 0.03), 0.22, 30, seed=3)
        for k, (f, du, zz, yaw) in enumerate(((-0.14, 0.45, STEP, 0), (0.17, 0.45, STEP, 0), (-0.14, 0.67, STEP, 0), (0.17, 0.67, STEP, 0), (0.0, 0.47, STEP + 0.116, 0), (0.02, 0.69, STEP + 0.116, 0))):
            bits.sandbag(m, path.at(mg + f, du, zz), math.degrees(math.atan2(*reversed(tuple(path.axes(mg)[0][:2])))) + 90 * (k < 4) + 0.0, seed=95 + k, size=(0.4, 0.24, 0.116))
        bits.crew(m, "gunner", path.at(mg, 0.56, STEP + 0.232), r, "stand", root, "standing on sandbags on the fire step, leaning into the recess, hands at grip.gunner.l and grip.gunner.r")
        bits.crew(m, "loader", path.at(mg + 0.6, 0.6, STEP), r, "stand", root, "standing on the fire step at the gunner's left, feeding the belt at `feed`")
    if nch is not None:
        niche(m, path, nch, 70)
    stove = None
    if door is not None:
        stove = dugout(m, path, door, 100)
    if lad is not None:
        ladder(m, path, lad, 5)
    phone_wire(m, path, 0.0, L, [b for b in breaks if b != (sump - 0.26, sump + 0.26)] if sump is not None else breaks)
    return names, rec, stove


def bay_info(m, path, rec=None, door=None, extra_clear=()):
    """dig, clear, worn and span for a length of fieldworks."""
    L = path.L
    rows = rows_for(L, 0.25, [rec] if rec else [])
    left = [path.at(s, U_RL - 0.25) for s in rows]
    right = [path.at(s, U_FL + 0.25 + (REC_PUSH if rec and rec[0] + 0.005 < s < rec[1] - 0.005 else 0.0)) for s in rows]
    m.info["dig"] = [[(round(p.x, 3), round(p.y, 3)) for p in left + right[::-1]]]
    m.info["dig_depth"] = -FLOOR
    cl = [path.at(s, -3.3) for s in rows] + [path.at(s, 3.6) for s in rows][::-1]
    m.info["clear"] = [[(round(p.x, 3), round(p.y, 3)) for p in cl]] + list(extra_clear)
    m.info["worn"] = [(p.x, p.y, 1.7) for p in (path.at(s, -1.6) for s in [L * k / 5 for k in range(6)])] + \
                     [(p.x, p.y, 1.5) for p in (path.at(s, 2.0) for s in [L * k / 5 for k in range(6)])]
    m.info["span"] = 10.5
    m.info["high"] = 0.3


def bay_base(path):
    def base(p):
        s, u = path.local(p)
        if U_RL - 0.15 < u < U_FL + 0.1 and p.z < -0.3:
            return FLOOR if u < U_R1 + 0.02 else STEP
        return 0.0
    return base


def hit_clip(spill_mid, land):
    q = Quaternion(Vector((0.3, 1.0, 0.25)).normalized(), math.radians(200))
    return {"long": 1.4, "loop": False, "after": "hold",
            "about": "A shell bursts on the parapet: three sandbags are thrown off it and fall into the bay.",
            "tracks": [{"part": "spill", "move": [[0.0, 0, 0, 0], [0.15, land[0] * 0.25, land[1] * 0.25, 0.6], [0.35, land[0] * 0.62, land[1] * 0.62, 0.55], [0.55, land[0] * 0.9, land[1] * 0.9, -0.6],
                                                  [0.72, land[0], land[1], land[2]], [1.4, land[0], land[1], land[2]]],
                        "rot": [[0.0, 1, 0, 0, 0], [0.36, *Quaternion().slerp(q, 0.5)], [0.72, q.w, q.x, q.y, q.z], [1.4, q.w, q.x, q.y, q.z]]}],
            "events": [{"t": 0.0, "fx": "shell_burst", "socket": "hit", "cue": "shell_impact_near"},
                       {"t": 0.05, "fx": "earth_spray", "socket": "hit", "cue": "explosion_debris"}]}


def joins(far_x=0.0):
    """Where the ends are, in the file's axes (y to the right): a length's start is at file y = +5 (the model's
    y = -5), its far end at file y = -5, the far end's x offset by `far_x`."""
    return {"section": "fieldworks: floor 2.0 m down and 0.9 m wide, fire step 0.92 m high on the enemy side, sandbag parapet on a berm",
            "ends": [{"at": [0.0, 5.0, 0.0], "facing": [0.0, 1.0, 0.0]}, {"at": [far_x, -5.0, 0.0], "facing": [0.0, -1.0, 0.0]}],
            "far_end_dx": far_x}


def Fieldworks(fine=True, nation="_default", name="Fieldworks"):
    """A 10 m length of fire bay: fire step, duckboards, the front wall revetted in stretches (posts and planks,
    woven hurdles, corrugated iron), two courses of sandbags on the parapet with a steel loophole plate, a
    machine-gun recess cut forward with its own loophole, a ladder, an ammunition niche, a sump, a low parados."""
    m = bits.model(name, "Fieldworks, a fire bay 10 m long (our own model)", "works", nation, fine, MORE)
    m.part("fieldworks", None, (0, 0, 0), kind="root")
    m.use("fieldworks")
    path = Path([(0.0, -5.0), (0.0, 5.0)])
    plan = {"mg": 2.0, "niche": 4.0, "loophole": 6.0, "sump": 7.6, "ladder": 9.15, "spill": (8.15, 8.85),
            "revet": [(0.0, 3.4, "planks"), (3.4, 6.6, "hurdle"), (6.6, 7.95, "iron"), (7.95, 10.0, "planks")]}
    names, rec, _ = fieldworks(m, path, plan)
    bay_crew(m, path, "fieldworks", [(3.3, None), (4.7, None), (6.0, "standing on the fire step, firing through the loophole plate"), (7.3, None), (8.5, None)])
    # what lies about: a helmet on the step, a pick and a shovel against the rear wall, tins, a rolled groundsheet
    helmet(m, path.at(5.3, 0.62, STEP), upside=False)
    pick(m, path.at(1.1, -0.45, FLOOR + 0.07), 80)
    shovel(m, path.at(5.2, -0.55, FLOOR + 0.06), path.at(5.0, -0.78, -1.15))
    tins(m, path.at(6.9, 0.5, STEP), 3, seed=3, r=0.15)
    m.rod(path.at(3.8, 0.55, STEP + 0.06), path.at(4.5, 0.6, STEP + 0.06), 0.06, "canvas", sides=12 if fine else 5)
    clods(m, [(-2.6, -4.5, -1.0, 4.5), (1.4, -4.5, 3.0, 4.5)], 60, seed=12, size=0.06)
    m.socket("hit", path.at(8.5, BAG_U + 0.3, 0.3), (1, 0, 0), part="fieldworks", note="where a shell strikes the parapet (the `hit` clip throws the bags there into the bay)")
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Manned and still.", "tracks": [], "events": []}
    m.clips["fire"] = mg_fire_clip(names)
    m.clips["hit"] = hit_clip(None, (-1.25, 0.0, FLOOR - BERM - 0.06))
    dug_weather(m, bay_base(path), [(names["trunnion"] + X * 0.7, 0.45, 0.7)])
    bay_info(m, path, rec)
    m.info["aim"] = (0.3, 0.0, -0.4)
    m.info["file"] = {"joins": joins(0.0)}
    return m



def FieldworksCorner(fine=True, nation="_default", name="FieldworksCorner", forward=False):
    """A dogleg joining two lengths: the bay turns 45 degrees, runs back 3 m and turns again (a Z in plan), so a shell
    bursting in one bay does not sweep the next. Its ends carry the straight length's cross-section at y = -5 and
    y = +5; the far end stands 3 m back (or forward)."""
    dx = 3.0 if forward else -3.0
    m = bits.model(name, "Fieldworks, a dogleg corner (our own model)", "works", nation, fine, MORE)
    m.part("fieldworks", None, (0, 0, 0), kind="root")
    m.use("fieldworks")
    path = Path([(0.0, -5.0), (0.0, -1.0 if forward else -2.0), (dx, 2.0 if forward else 1.0), (dx, 5.0)])
    L = path.L
    sp = (6.6, 7.3)
    plan = {"niche": L - 1.6 if not forward else 1.4, "sump": 5.3, "ladder": None, "spill": sp,
            "revet": [(0.0, 2.2, "planks"), (2.2, 7.6, "hurdle"), (7.6, 8.9, "iron"), (8.9, L, "planks")] if not forward else
                     [(0.0, 2.6, "planks"), (2.6, 3.9, "iron"), (3.9, 8.6, "hurdle"), (8.6, L, "planks")]}
    fieldworks(m, path, plan)
    bay_crew(m, path, "fieldworks", [(1.3, None), (3.9, None), (8.2, None), (9.9, None)])
    helmet(m, path.at(5.0, 0.6, STEP), upside=True)
    pick(m, path.at(L - 3.1, -0.45, FLOOR + 0.07), 80 + (45 if not forward else -45) * 0)
    tins(m, path.at(2.6, 0.5, STEP), 3, seed=5, r=0.15)
    g = rnd(4)
    clods(m, [(p.x - 0.8, p.y - 0.8, p.x + 0.8, p.y + 0.8) for p in (path.at(L * k / 7, -2.0 + 4.0 * (k % 2)) for k in range(8))], 60, seed=12, size=0.06)
    hs = (sp[0] + sp[1]) / 2
    m.socket("hit", path.at(hs, BAG_U + 0.3, 0.3), path.axes(hs, 1.0)[1], part="fieldworks", note="where a shell strikes the parapet")
    _, r = path.axes(hs, BAG_U)
    land = -r * 1.25
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Manned and still.", "tracks": [], "events": []}
    m.clips["hit"] = hit_clip(None, (land.x, land.y, FLOOR - BERM - 0.06))
    dug_weather(m, bay_base(path))
    bay_info(m, path)
    m.info["span"] = 11.5
    m.info["aim"] = (dx * 0.5 + 0.3, 0.0, -0.4)
    m.info["file"] = {"joins": joins(dx)}
    return m


def FieldworksDugout(fine=True, nation="_default", name="FieldworksDugout"):
    """A 10 m length of fire bay like the straight one, with a dugout entered from its rear wall: a timber-framed
    doorway, steps going down, and behind the bay its roof of logs under a mound of earth a metre high, with a stove
    pipe."""
    m = bits.model(name, "Fieldworks with a dugout, 10 m long (our own model)", "works", nation, fine, MORE)
    m.part("fieldworks", None, (0, 0, 0), kind="root")
    m.use("fieldworks")
    path = Path([(0.0, -5.0), (0.0, 5.0)])
    door = 4.6
    plan = {"door": door, "loophole": 7.6, "sump": 2.2, "ladder": 0.9, "spill": (5.6, 6.3),
            "revet": [(0.0, 2.8, "planks"), (2.8, 4.2, "iron"), (4.2, 7.0, "hurdle"), (7.0, 10.0, "planks")]}
    _, _, stove = fieldworks(m, path, plan)
    bay_crew(m, path, "fieldworks", [(1.8, None), (3.2, None), (5.0, None), (7.6, "standing on the fire step, firing through the loophole plate"), (8.9, None)])
    bits.crew(m, "sentry", path.at(door, -0.45, FLOOR + 0.05), (-1, 0, 0), "stand", "fieldworks", "standing in the bay at the dugout's door")
    helmet(m, path.at(2.9, 0.62, STEP), upside=False)
    shovel(m, path.at(6.3, -0.55, FLOOR + 0.06), path.at(6.1, -0.78, -1.15))
    tins(m, path.at(door + 0.9, -0.45, FLOOR + 0.07), 3, seed=8, r=0.18)
    m.box(path.at(door - 0.8, -0.45, FLOOR + 0.2), (0.32, 0.22, 0.34), "upper2")            # a water can by the door
    m.rod(path.at(door - 0.8, -0.5, FLOOR + 0.37), path.at(door - 0.8, -0.4, FLOOR + 0.37), 0.012, "steel", sides=6)
    clods(m, [(-2.6, -4.5, -1.0, -2.6), (-2.6, 2.6, -1.0, 4.5), (1.4, -4.5, 3.0, 4.5)], 50, seed=12, size=0.06)
    m.socket("hit", path.at(5.95, BAG_U + 0.3, 0.3), (1, 0, 0), part="fieldworks", note="where a shell strikes the parapet")
    m.socket("smoke", stove + Z * 0.05, (0, 0, 1), part="fieldworks", note="the stove pipe's smoke")
    m.socket("door", path.at(door, ur(-1.0), FLOOR), (-1, 0, 0), part="fieldworks", note="the dugout's doorway: men go in and out here")
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Manned and still.", "tracks": [], "events": []}
    m.clips["hit"] = hit_clip(None, (-1.25, 0.0, FLOOR - BERM - 0.06))
    dug_weather(m, bay_base(path), [(stove, 0.3, 0.8)])
    mound = [path.at(door + 2.0 * math.cos(a), -3.25 + 2.3 * math.sin(a)) for a in [2 * math.pi * k / 24 for k in range(24)]]
    bay_info(m, path, None, extra_clear=[[(round(p.x, 3), round(p.y, 3)) for p in mound]])
    m.info["aim"] = (-0.6, 0.0, -0.3)
    m.info["span"] = 11.0
    m.info["file"] = {"joins": joins(0.0)}
    return m


# ---- the anti-tank ditch -------------------------------------------------------------------------------------------

AT_DEPTH, AT_LIP_D, AT_LIP_E = 2.5, -2.0, 2.5


def at_wall(z):
    """The defender's wall, nearly upright, at height z."""
    return AT_LIP_D + 0.22 * (-z / AT_DEPTH)


def ditch_profile():
    P = [(-7.3, 0.004, "edge"), (-6.8, 0.04, "bank"), (-6.2, 0.2, "bank"), (-5.6, 0.5, "bank"), (-5.0, 0.8, "bank"), (-4.5, 0.97, "bank"), (-4.0, 1.02, "bank"),
         (-3.55, 0.9, "bank"), (-3.15, 0.6, "bank"), (-2.85, 0.3, "bank"), (-2.65, 0.1, "bank"), (-2.45, 0.03, "berm"), (-2.2, 0.02, "berm"), (-2.06, 0.012, "lip")]
    for z in (-0.05, -0.3, -0.6, -0.9, -1.2, -1.5, -1.8, -2.1, -2.38):
        P.append((at_wall(z), z, "wall"))
    P += [(-1.74, -2.5, "floor"), (-1.6, -2.56, "floor"), (-1.42, -2.58, "floor"), (-1.25, -2.55, "floor")]
    u0, z0 = AT_LIP_E - AT_DEPTH / math.tan(math.radians(35.0)), -AT_DEPTH
    for k in range(13):
        f = k / 12
        P.append((u0 + (AT_LIP_E - u0) * f, z0 * (1 - f) - 0.02 * math.sin(math.pi * f), "slope"))
    P += [(AT_LIP_E + 0.1, 0.012, "lip"), (2.85, 0.016, "apron"), (3.3, 0.014, "apron"), (3.9, 0.004, "edge")]
    return P


def AntiTankDitch(fine=True, nation="_default", name="AntiTankDitch"):
    """A 10 m length of anti-tank ditch: 4.5 m across the top, 2.5 m deep, the enemy's side sloped at 35 degrees so a
    tank runs down it and cannot climb out, the defender's side nearly upright and revetted with driven poles held by
    wales and wired back, the spoil thrown up in a bank a metre high on the defender's side."""
    m = bits.model(name, "Anti-tank ditch, a length (our own model)", "works", nation, fine, MORE)
    m.part("ditch", None, (0, 0, 0), kind="root")
    m.use("ditch")
    path = Path([(0.0, -5.0), (0.0, 5.0)])
    L = path.L
    amp = {"bank": 0.08, "berm": 0.0, "lip": 0.01, "wall": 0.02, "floor": 0.035, "slope": 0.03, "apron": 0.015}

    def gully(s, u, z, tag):
        if tag == "slope":           # rain has cut runnels down the enemy's slope
            f = (-z) / AT_DEPTH
            return (-0.16 * max(0.0, fbm(Vector((s * 1.6, 0.3, 7.0)), 1.0, 2) - 0.05) - 0.05 * max(0.0, fbm(Vector((s * 4.0, f * 2.0, 3.0)), 1.0, 2))) \
                * math.sin(math.pi * min(1.0, f * 1.1)) + 0.06 * fbm(Vector((s * 0.5, f * 3.0, 1.0)), 1.0, 2) * math.sin(math.pi * f)
        return swell(s, u, z, "bank" if tag == "bank" else "")
    sweep(m, path, ditch_profile(), rows_for(L, 0.1 if fine else 0.5), amp, 0.18, 3.3, groove=gully)
    g = rnd(7)
    # water standing along the bottom
    pts = []
    n = 40 if fine else 10
    for k in range(n + 1):
        sc = 0.05 + (L - 0.1) * k / n
        wdt = 0.13 + 0.09 * fbm(Vector((sc * 0.8, 1.0, 2.0)), 1.0, 2)
        pts.append((path.at(sc, -1.42 - wdt, -2.555), path.at(sc, -1.42 + wdt, -2.555)))
    m.add([p for pr in pts for p in pr], [(2 * k, 2 * k + 1, 2 * k + 3, 2 * k + 2) for k in range(n)], "glass", True, lambda p: Z)
    # the revetment: poles driven side by side against the defender's wall
    s = 0.07
    k = 0
    while s < L - 0.05:
        r = g.uniform(0.052, 0.068)
        top = g.uniform(0.03, 0.2)
        bits.log(m, path.at(s, at_wall(-2.5) + r + 0.005, -2.62), path.at(s, at_wall(top) + r + 0.005, top), r, "timber", 300 + k, sides=8 if fine else 4, taper=0.9)
        s += 2 * r + g.uniform(0.004, 0.03) if fine else 0.32
        k += 1
    # wales across the poles, in lengths, wired back to pickets on the berm
    for z in (-0.55, -1.95):
        a = 0.0
        while a < L - 0.1:
            b = min(L, a + g.uniform(2.4, 3.2))
            uu = at_wall(z) + 0.135 + 0.07
            bits.log(m, path.at(a + 0.02, uu, z + g.uniform(-0.02, 0.02)), path.at(b - 0.02, uu, z + g.uniform(-0.02, 0.02)), 0.07, "timber", int(a * 10 + z * 3), sides=10 if fine else 5)
            a = b + 0.01
    for k in range(9):
        sc = 0.6 + k * (L - 1.2) / 8
        bits.log(m, path.at(sc, -2.6, -0.3), path.at(sc + 0.03, -2.62, 0.22), 0.035, "timber.dark", 400 + k, sides=7 if fine else 4)
        if fine:
            for z in (-0.55,):
                uu = at_wall(z) + 0.135 + 0.07
                ring = [path.at(sc, uu + 0.075 * math.cos(a), z + 0.075 * math.sin(a)) for a in [math.radians(t) for t in range(-90, 181, 30)]]
                bits.tube(m, ring + [path.at(sc, at_wall(z + 0.05), z + 0.05)], 0.004, "wire", 5)
            bits.tube(m, [path.at(sc, at_wall(0.0) + 0.02, 0.02), path.at(sc + 0.01, -2.3, 0.05), path.at(sc + 0.02, -2.58, 0.17)], 0.0045, "wire", 5)
    # what the engineers left: spare poles and a shovel on the berm, clods along the bank
    for k in range(3):
        bits.log(m, path.at(1.4 + k * 0.17, -2.9 - k * 0.05, 0.2 + 0.11 * (k == 2)), path.at(4.2 + k * 0.12, -2.95 - k * 0.05, 0.22 + 0.11 * (k == 2)), 0.06, "timber", 500 + k, sides=8 if fine else 4)
    shovel(m, path.at(7.2, -2.5, 0.0), path.at(7.0, -2.75, 0.72))
    clods(m, [(-6.8, -4.6, -2.6, 4.6), (2.6, -4.6, 3.4, 4.6)], 70, seed=21, size=0.08)
    m.socket("hit", path.at(5.0, -4.1, 1.05), (1, 0, 0), part="ditch", note="where a shell strikes the spoil bank")
    m.socket("bridge", path.at(5.0, (AT_LIP_D + AT_LIP_E) / 2, 0.0), (1, 0, 0), part="ditch",
             note="the middle of a crossing laid by engineers: lip to lip 4.5 m, from x = -2.0 (defender) to x = +2.5 (enemy), along x")
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Still.", "tracks": [], "events": []}
    m.clips["hit"] = {"long": 1.0, "loop": False, "after": "hold", "about": "A shell bursts on the spoil bank.", "tracks": [],
                      "events": [{"t": 0.0, "fx": "shell_burst", "socket": "hit", "cue": "shell_impact_near"}]}
    dug_weather(m, lambda p: -AT_DEPTH if AT_LIP_D - 0.1 < p.x < AT_LIP_E and p.z < -0.3 else 0.0)
    rows = rows_for(L, 0.25)
    m.info["dig"] = [[(round(p.x, 3), round(p.y, 3)) for p in [path.at(sc, AT_LIP_D - 0.25) for sc in rows] + [path.at(sc, AT_LIP_E + 0.25) for sc in rows][::-1]]]
    m.info["dig_depth"] = AT_DEPTH
    m.info["clear"] = [[(round(p.x, 3), round(p.y, 3)) for p in [path.at(sc, -7.1) for sc in rows] + [path.at(sc, 3.6) for sc in rows][::-1]]]
    m.info["worn"] = [(-2.5, y, 1.5) for y in (-4, -2, 0, 2, 4)]
    m.info["span"] = 11.5
    m.info["high"] = 0.5
    m.info["aim"] = (-0.8, 0.0, -0.5)
    m.info["file"] = {"joins": {"section": "anti-tank ditch: 4.5 m across the top, 2.5 m deep, the enemy's side at 35 degrees",
                                "ends": [{"at": [0.0, 5.0, 0.0], "facing": [0.0, 1.0, 0.0]}, {"at": [0.0, -5.0, 0.0], "facing": [0.0, -1.0, 0.0]}], "far_end_dx": 0.0}}
    return m


BUILDERS = {
    "Foxhole": Foxhole,
    "Fieldworks": Fieldworks,
    "FieldworksCorner": FieldworksCorner,
    "FieldworksCorner_forward": lambda fine=True: FieldworksCorner(fine, forward=True, name="FieldworksCorner_forward"),
    "FieldworksDugout": FieldworksDugout,
    "AntiTankDitch": AntiTankDitch,
}
