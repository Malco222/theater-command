"""The motor pool: the pieces a land vehicle is built from, each made from numbers.

A vehicle is a `rigid.Model` (the ships' and aircraft's own kind of model: parts that hang from one another, sockets,
clips), built here from armour plate with rounded edges, castings with no flat on them, wheels turned on a lathe, and a
track laid link by link round its wheels. Everything is in metres, to the measurements of the real type; x forward,
z up from the ground, y to the vehicle's LEFT while modelling (the file is written with y to the right: see rigid.py).

Two things make a machine look made rather than drawn, and both are done for every plate:
  * no edge is a knife: every plate edge is cut back (a bevel) and the mesh is given weighted normals (see `finish`),
    so a flat stays flat and its edge catches the light as rolled or cast steel does;
  * nothing round shows its facets: wheels, barrels and castings are turned with enough sides for the closest camera.

With `fine=False` the same code makes the plain mesh for the far camera: fewer sides, no bevels, no small fittings, a
band for each track.
"""

import math
import random
import zlib

import bpy  # (first: the bpy module puts Blender's own modules, bmesh among them, on the path)
import bmesh
from mathutils import Matrix, Quaternion, Vector

import rigid
from rigid import X, Y, Z, Model, lerp, smooth

LEFT, RIGHT = 1.0, -1.0
SIDES = (("l", LEFT), ("r", RIGHT))

# Paints every vehicle has. `upper` is the coat: the game gives it the nation's paint (unit-table.json). `upper2` is a
# second camouflage colour (how far a point is into it is the file's `blend`).
COMMON = {
    "upper": "60664A", "upper2": "7A6A48", "shade": "3C402E", "track": "37322E", "rubber": "202022", "steel": "3A3C3E",
    "bright": "8C8E90", "wood": "7C5E3C", "canvas": "8C8468", "glass": "1B2A33", "black": "101112", "rust": "4A3226",
    "lamp": "D8D4C0", "white": "E6E2D8", "red": "A82020", "yellow": "C9A227", "blue": "2F4A7A", "leather": "4A3626",
    "torn": "241D1A", "rope": "8A7A5A", "cloth": "807A56", "skin": "C49A7A", "brass": "9A8448",
}

# The art director's vehicle paints (unit-table.json, "as built"), and a second colour where the army camouflaged.
NATION = {
    "GER": ("4E5459", None), "POL": ("66684A", "7A6A48"), "UK": ("65704E", None), "USA": ("565C3A", None),
    "SOV": ("4F5E3E", None), "FRA": ("5E6A48", "7A5C40"), "ITA": ("8A7F5E", "5E6A48"), "JAP": ("988256", "5E6442"),
    "_": ("60664A", None),
}


def dark(hx, k):
    return "".join("%02X" % max(0, min(255, int(int(hx[i:i + 2], 16) * k))) for i in (0, 2, 4))


def livery(tag, paint=None):
    coat, second = NATION.get(tag, NATION["_"])
    coat = paint or coat
    p = dict(COMMON)
    p.update({"upper": coat, "upper2": second or dark(coat, 0.8), "shade": dark(coat, 0.62)})
    return p


def rot(yaw=0.0, pitch=0.0, roll=0.0):
    """Turned `yaw` to the left, nose lifted `pitch`, rolled `roll` (left side up), in degrees."""
    return Matrix.Rotation(math.radians(yaw), 3, "Z") @ Matrix.Rotation(math.radians(-pitch), 3, "Y") @ Matrix.Rotation(math.radians(roll), 3, "X")


def frame(axis):
    """Two unit vectors square to an axis and to each other."""
    axis = Vector(axis).normalized()
    side = axis.cross(Z)
    if side.length < 1e-6:
        side = axis.cross(X)
    side.normalize()
    return axis, side, side.cross(axis)


def sym_plan(left):
    """A plan from its left half (x, y points from the front going back): the full outline, counter-clockwise."""
    left = [tuple(p) for p in left]
    return left + [(x, -y) for x, y in reversed(left)]


def inset_plan(plan, insets):
    """A plan (x, y going counter-clockwise) with every edge moved inward by its own distance (edge i runs from point
    i to point i + 1). The walls between the two plans are then flat plates, each leaning by its own amount."""
    n = len(plan)
    if isinstance(insets, (int, float)):
        insets = [insets] * n
    lines = []
    for i in range(n):
        a, b = Vector(plan[i][:2]), Vector(plan[(i + 1) % n][:2])
        d = (b - a).normalized()
        lines.append((a + Vector((-d.y, d.x)) * insets[i], d))
    out = []
    for i in range(n):
        (p, d), (q, e) = lines[i - 1], lines[i]
        den = d.x * e.y - d.y * e.x
        if abs(den) < 1e-9:
            out.append(tuple(q))
            continue
        t = ((q.x - p.x) * e.y - (q.y - p.y) * e.x) / den
        out.append(tuple(p + d * t))
    return out


# ---- solids in bmesh ---------------------------------------------------------------------------------

def bm_box(size, at=(0, 0, 0), turn=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    if turn is not None:
        bmesh.ops.rotate(bm, cent=(0, 0, 0), matrix=turn, verts=bm.verts)
    bmesh.ops.translate(bm, vec=Vector(at), verts=bm.verts)
    return bm


def bm_prism(plan, z0, z1, top=None, shift=(0.0, 0.0)):
    """A plan (points x, y) raised from z0 to z1; `top` another plan for the upper edge; `shift` moves the upper plan."""
    bm = bmesh.new()
    tp = top or plan
    lo = [bm.verts.new((p[0], p[1], z0)) for p in plan]
    hi = [bm.verts.new((p[0] + shift[0], p[1] + shift[1], z1)) for p in tp]
    n = len(plan)
    bm.faces.new(lo[::-1])
    bm.faces.new(hi)
    for k in range(n):
        bm.faces.new((lo[k], lo[(k + 1) % n], hi[(k + 1) % n], hi[k]))
    return bm


def bm_extrude(profile, y0, y1):
    """A side view (points x, z going round) carried across from y0 to y1."""
    bm = bmesh.new()
    a = [bm.verts.new((p[0], y0, p[1])) for p in profile]
    b = [bm.verts.new((p[0], y1, p[1])) for p in profile]
    n = len(profile)
    bm.faces.new(a)
    bm.faces.new(b[::-1])
    for k in range(n):
        bm.faces.new((a[k], b[k], b[(k + 1) % n], a[(k + 1) % n]))
    return bm


def bm_hull(points, both=False):
    """The convex solid round a set of points; `both` adds each point's mirror across the centre line."""
    bm = bmesh.new()
    seen = set()
    for p in points:
        for s in ((1.0, -1.0) if both else (1.0,)):
            q = (round(p[0], 5), round(p[1] * s, 5), round(p[2], 5))
            if q not in seen:
                seen.add(q)
                bm.verts.new(q)
    out = bmesh.ops.convex_hull(bm, input=bm.verts[:], use_existing_faces=False)
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(0.6), verts=bm.verts[:], edges=bm.edges[:], delimit=set())
    return bm


def bm_move(bm, at=None, turn=None, scale=None, about=(0, 0, 0)):
    if scale is not None:
        bmesh.ops.scale(bm, vec=Vector(scale), verts=bm.verts, space=Matrix.Translation(-Vector(about)))
    if turn is not None:
        bmesh.ops.rotate(bm, cent=Vector(about), matrix=turn, verts=bm.verts)
    if at is not None:
        bmesh.ops.translate(bm, vec=Vector(at), verts=bm.verts)
    return bm


def bm_bevel(bm, width, segs=1, angle=24.0, profile=0.5):
    """Cuts back every edge sharper than `angle` degrees. Returns the new faces: the cut-back edges themselves."""
    lim = math.radians(angle)
    edges = [e for e in bm.edges if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > lim]
    if not edges:
        return set()
    res = bmesh.ops.bevel(bm, geom=edges, offset=width, offset_type="OFFSET", segments=segs, profile=profile, affect="EDGES", clamp_overlap=True)
    return set(res["faces"])


def outline(xc, af, ar, b, n=2.4, sides=40, yc=0.0, bulge=0.0):
    """An egg-shaped plan: `af` long ahead of its middle and `ar` behind, `b` half-wide; `n` is how square it is
    (2 an ellipse, 4 nearly a box with round corners). Counter-clockwise from the front."""
    pts = []
    for k in range(sides):
        t = 2.0 * math.pi * k / sides
        c, s = math.cos(t), math.sin(t)
        a = af if c >= 0 else ar
        x = a * math.copysign(abs(c) ** (2.0 / n), c)
        y = b * math.copysign(abs(s) ** (2.0 / n), s)
        if bulge:
            y *= 1.0 + bulge * (x / max(af, ar))
        pts.append((xc + x, yc + y))
    return pts


def round_plan(pts, r, steps=4):
    """A plan (x, y points going round counter-clockwise) with every corner rounded to radius r (or to the r given as a
    third number with a point)."""
    out = []
    n = len(pts)
    for i in range(n):
        p = Vector(pts[i][:2])
        rr = pts[i][2] if len(pts[i]) > 2 else r
        a, b = Vector(pts[i - 1][:2]), Vector(pts[(i + 1) % n][:2])
        da, db = (a - p), (b - p)
        if rr <= 1e-6 or da.length < 1e-6 or db.length < 1e-6:
            out.append((p.x, p.y))
            continue
        ang = da.angle(db)
        if abs(math.pi - ang) < 1e-3:
            out.append((p.x, p.y))
            continue
        t = min(rr / math.tan(ang / 2.0), da.length * 0.49, db.length * 0.49)
        rr = t * math.tan(ang / 2.0)
        pa, pb = p + da.normalized() * t, p + db.normalized() * t
        c = p + (da.normalized() + db.normalized()).normalized() * (rr / math.sin(ang / 2.0))
        a0 = math.atan2(pa.y - c.y, pa.x - c.x)
        a1 = math.atan2(pb.y - c.y, pb.x - c.x)
        d = (a1 - a0 + math.pi) % (2 * math.pi) - math.pi
        for k in range(steps + 1):
            t2 = a0 + d * k / steps
            out.append((c.x + math.cos(t2) * rr, c.y + math.sin(t2) * rr))
    return out


# ---- a track's path ----------------------------------------------------------------------------------

class Belt:
    """The path of a track round its wheels, in the vehicle's side view (x forward, z up).

    `wheels` are (x, z, r) in the order the track meets them going round: along the ground from the rear to the front,
    up over the front wheel, back along the top (return rollers, or the tops of big road wheels), down round the rear
    wheel. The path is the track's inner face, where the wheels run. s is metres along it; a link that drives the
    vehicle forward travels with s falling (rearward along the ground)."""

    def __init__(self, wheels, sag=0.03, step=0.012):
        n = len(wheels)
        tans = []
        for i in range(n):
            a, b = wheels[i], wheels[(i + 1) % n]
            d = Vector((b[0] - a[0], b[1] - a[1]))
            L = d.length
            u = d / L
            c = max(-1.0, min(1.0, (a[2] - b[2]) / L))
            right = Vector((u.y, -u.x))
            nrm = u * c + right * math.sqrt(max(0.0, 1.0 - c * c))
            tans.append((Vector((a[0], a[1])) + nrm * a[2], Vector((b[0], b[1])) + nrm * b[2]))
        pts = []
        for i in range(n):
            w = wheels[i]
            pin, pout = tans[i - 1][1], tans[i][0]
            a0 = math.atan2(pin.y - w[1], pin.x - w[0])
            a1 = math.atan2(pout.y - w[1], pout.x - w[0])
            d = (a1 - a0) % (2 * math.pi)
            if d > 2 * math.pi - 0.6:          # (the track only brushes this wheel)
                d = 0.0
            k = max(1, int(d * w[2] / step))
            for j in range(k + 1 if d > 1e-9 else 1):
                t = a0 + d * j / k
                pts.append(Vector((w[0] + math.cos(t) * w[2], w[1] + math.sin(t) * w[2])))
            # the straight to the next wheel; a top run hangs a little between its supports
            p0, p1 = tans[i]
            span = (p1 - p0).length
            k = max(1, int(span / (step * 2.0)))
            dip = min(sag, 0.06 * span) if (p1.x < p0.x - 0.05 and span > 0.25) else 0.0
            for j in range(0, k + 1):
                t = j / k
                p = p0.lerp(p1, t)
                p.y -= dip * 4.0 * t * (1.0 - t)
                pts.append(p)
        # drop doubled points, measure
        clean = [pts[0]]
        for p in pts[1:]:
            if (p - clean[-1]).length > 1e-5:
                clean.append(p)
        if (clean[0] - clean[-1]).length < 1e-5:
            clean.pop()
        self.pts = clean
        self.acc = [0.0]
        for i in range(len(clean)):
            self.acc.append(self.acc[-1] + (clean[(i + 1) % len(clean)] - clean[i]).length)
        self.long = self.acc[-1]

    def at(self, s):
        """(point, way it runs, outward) at s metres along; all 2D (x, z)."""
        s %= self.long
        lo, hi = 0, len(self.pts)
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if self.acc[mid] <= s:
                lo = mid
            else:
                hi = mid
        a, b = self.pts[lo], self.pts[(lo + 1) % len(self.pts)]
        seg = self.acc[lo + 1] - self.acc[lo]
        t = (s - self.acc[lo]) / seg if seg > 1e-9 else 0.0
        # the way it runs, taken over a short reach so links do not twitch at the joins of arc and straight
        h = 0.02
        p0, p1 = self._pt(s - h), self._pt(s + h)
        way = (p1 - p0).normalized()
        return a.lerp(b, t), way, Vector((way.y, -way.x))

    def _pt(self, s):
        s %= self.long
        lo, hi = 0, len(self.pts)
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if self.acc[mid] <= s:
                lo = mid
            else:
                hi = mid
        a, b = self.pts[lo], self.pts[(lo + 1) % len(self.pts)]
        seg = self.acc[lo + 1] - self.acc[lo]
        return a.lerp(b, (s - self.acc[lo]) / seg if seg > 1e-9 else 0.0)


# ---- the workshop ------------------------------------------------------------------------------------

class Shop:
    """Builds one vehicle. Holds its model `m`; every maker adds faces to the part in use (`use`)."""

    def __init__(self, name, title, kind, tag="GER", fine=True, paint=None, paints=None):
        p = livery(tag, paint)
        p.update(paints or {})
        self.m = Model(name, title, kind, p, fine)
        self.fine, self.tag, self.name = fine, tag, name
        self.rng = random.Random(zlib.crc32(name.encode()))
        self.m.info["tag"] = tag
        self.m.info["rust"] = "8E897C"        # (what `wear` goes toward: on a vehicle, paint rubbed thin over worn metal)
        self.wheels = []          # (part, radius): what the game turns as she rolls
        self.fedge = []           # for every face of the model: is it a plate's cut-back edge (where paint wears first)
        self.m.fedge = self.fedge
        self.belts = {}           # side letter -> what the track clip needs
        self.marks = []

    # -- small conveniences
    def n(self, sides, least=6):
        """How many sides a round thing has: as asked on the fine mesh, about a third on the plain one."""
        return sides if self.fine else max(least, int(round(sides / 3.0)))

    def part(self, name, parent, pivot=(0, 0, 0), **kw):
        self.m.part(name, parent, pivot, **kw)
        self.m.use(name)
        return name

    def use(self, name):
        self.m.use(name)
        return self

    def socket(self, name, at, facing=(1, 0, 0), part=None, note=""):
        self.m.socket(name, at, facing, part, note)

    def flag(self, before, flags):
        """Marks the faces added since the model had `before` of them: how far each is a rubbed edge or a polished face."""
        self.fedge.extend([0.0] * (before - len(self.fedge)))
        added = len(self.m.faces) - before
        self.fedge.extend(list(flags) if (flags is not None and added == len(flags)) else [0.0] * added)

    # -- faces from a bmesh solid
    def put(self, bm, paint, bevel=0.0, segs=1, angle=24.0, recalc=True, smooth_faces=True, part=None):
        if recalc:
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        cut = bm_bevel(bm, bevel, segs, angle) if (bevel > 0.0 and self.fine) else set()
        bm.verts.index_update()
        verts = [v.co.copy() for v in bm.verts]
        faces = [tuple(v.index for v in f.verts) for f in bm.faces]
        flags = [1.0 if f in cut else 0.0 for f in bm.faces]
        before = len(self.m.faces)
        self.fedge.extend([0.0] * (before - len(self.fedge)))
        self.m.add(verts, faces, paint, smooth_faces, None, part)
        added = len(self.m.faces) - before
        self.fedge.extend(flags if added == len(flags) else [0.0] * added)
        bm.free()

    def box(self, size, at, paint, bevel=0.0, turn=None, segs=1, part=None):
        self.put(bm_box(size, at, turn), paint, bevel, segs, part=part)

    def slab(self, a, b, paint, bevel=0.0, segs=1, part=None):
        """A box by two opposite corners."""
        a, b = Vector(a), Vector(b)
        self.put(bm_box([abs(b[i] - a[i]) for i in range(3)], (a + b) / 2.0), paint, bevel, segs, part=part)

    def hull(self, points, paint, bevel=0.012, segs=2, both=True, part=None, angle=14.0):
        """Plate armour: the convex solid round these points (and their mirrors), its edges cut back."""
        self.put(bm_hull(points, both), paint, bevel, segs, angle, part=part)

    def prism(self, plan, z0, z1, paint, top=None, bevel=0.0, segs=1, shift=(0.0, 0.0), part=None, angle=24.0):
        self.put(bm_prism(plan, z0, z1, top, shift), paint, bevel, segs, angle, part=part)

    def extrude(self, profile, y0, y1, paint, bevel=0.0, segs=1, part=None, angle=24.0):
        self.put(bm_extrude(profile, y0, y1), paint, bevel, segs, angle, part=part)

    def plate(self, corners, thick, paint, bevel=0.0, part=None):
        """A flat plate given by its corners (going round), `thick` deep behind its face."""
        cs = [Vector(c) for c in corners]
        nrm = (cs[1] - cs[0]).cross(cs[2] - cs[0]).normalized()
        bm = bmesh.new()
        a = [bm.verts.new(c) for c in cs]
        b = [bm.verts.new(c - nrm * thick) for c in cs]
        k = len(cs)
        bm.faces.new(a)
        bm.faces.new(b[::-1])
        for i in range(k):
            bm.faces.new((a[i], b[i], b[(i + 1) % k], a[(i + 1) % k]))
        self.put(bm, paint, bevel, part=part)

    # -- turned on a lathe
    def lathe(self, base, axis, profile, paint, sides=24, smooth_faces=True, part=None):
        """Turned about `axis` from `base`. The profile is (distance along the axis, radius) points going round the
        solid's section COUNTER-CLOCKWISE when the axis is drawn to the right and the radius upward: up the far face,
        back along the rim, down the near face. So the faces know which way is out without guessing."""
        axis, side, up = frame(axis)
        base = Vector(base)
        prof = [(float(d), max(float(r), 0.0)) for d, r in profile]
        verts, rings = [], []
        for d, r in prof:
            if r < 1e-6:
                rings.append([len(verts)])
                verts.append(base + axis * d)
            else:
                rings.append(list(range(len(verts), len(verts) + sides)))
                for k in range(sides):
                    a = 2 * math.pi * k / sides
                    verts.append(base + axis * d + (side * math.cos(a) + up * math.sin(a)) * r)
        faces = []
        for i in range(len(prof) - 1):
            (d0, r0), (d1, r1) = prof[i], prof[i + 1]
            if abs(d1 - d0) < 1e-9 and abs(r1 - r0) < 1e-9:
                continue
            A, B = rings[i], rings[i + 1]
            if len(A) == 1 and len(B) == 1:
                continue
            want = axis * (r1 - r0) - side * (d1 - d0)          # which way is out, at the first spoke
            for k in range(sides):
                k1 = (k + 1) % sides
                if len(A) == 1:
                    f = (A[0], B[k], B[k1])
                elif len(B) == 1:
                    f = (A[k], B[0], A[k1])
                else:
                    f = (A[k], B[k], B[k1], A[k1])
                if k == 0:
                    p = [verts[j] for j in f]
                    nrm = (p[1] - p[0]).cross(p[2] - p[0])
                    if len(f) == 4:
                        nrm += (p[2] - p[0]).cross(p[3] - p[0])
                    turn_round = nrm.dot(want) < 0.0
                faces.append(tuple(reversed(f)) if turn_round else f)
        self.m.add(verts, faces, paint, smooth_faces, None, part)

    def rod(self, a, b, r, paint, sides=10, r2=None, caps=True, part=None):
        a, b = Vector(a), Vector(b)
        L = (b - a).length
        if L < 1e-6:
            return
        r2 = r if r2 is None else r2
        prof = [(L, 0.0), (L, r2), (0.0, r), (0.0, 0.0)] if caps else [(L, r2), (0.0, r)]
        self.lathe(a, b - a, prof, paint, self.n(sides, 5), part=part)

    def tube(self, points, r, paint, sides=8, part=None, closed=False):
        """A bent rod along a row of points (a grab handle, a tow cable, a conduit)."""
        pts = [Vector(p) for p in points]
        if len(pts) < 2:
            return
        sides = self.n(sides, 4)
        rings = []
        prev_side = None
        k_n = len(pts)
        for i, p in enumerate(pts):
            if closed:
                along = pts[(i + 1) % k_n] - pts[i - 1]
            else:
                along = pts[min(i + 1, k_n - 1)] - pts[max(i - 1, 0)]
            along.normalize()
            if prev_side is None:
                _, side, up = frame(along)
            else:
                side = (prev_side - along * prev_side.dot(along))
                if side.length < 1e-6:
                    _, side, up = frame(along)
                side.normalize()
                up = along.cross(side)
            prev_side = side
            # a mitre: the ring is stretched at a bend so the tube keeps its thickness
            if 0 < i < k_n - 1 or closed:
                din = (pts[i] - pts[i - 1]).normalized()
                c = max(0.35, math.sqrt(max(0.0, (1.0 + din.dot((pts[(i + 1) % k_n] - pts[i]).normalized())) / 2.0)))
            else:
                c = 1.0
            rings.append([p + (side * math.cos(2 * math.pi * j / sides) + up * math.sin(2 * math.pi * j / sides)) * (r / c ** 0.5) for j in range(sides)])
        if closed:
            rings.append(rings[0])
        self.m.skin(rings, paint, (not closed, not closed), True)

    def ball(self, at, r, paint, u=12, v=8, part=None, dome=False, squash=1.0):
        """A ball, or with `dome` its upper half on a flat foot; `squash` flattens it."""
        u, v = self.n(u, 6), self.n(v, 4)
        prof = []
        for j in range(v + 1):
            a = math.pi / 2 - (math.pi / 2 if dome else math.pi) * j / v
            prof.append((math.sin(a) * r * squash, max(0.0, math.cos(a) * r)))
        self.lathe(at, Z, prof, paint, u, part=part)

    # -- a casting: a skin over level plans
    def cast(self, keys, paint, sides=44, top=0.05, steps=3, between=3, top_paint=None, bottom=True, part=None, n_top=None):
        """A cast shape (a turret, a hull nose) as a skin over egg-shaped level plans.

        keys: dicts z, xc (middle), af, ar (lengths ahead and behind), b (half-width), n (squareness), yc. Between keys
        the plan is carried over smoothly; the upper edge is rounded over to radius `top` and closed with a roof."""
        sides = self.n(sides, 10)
        between = between if self.fine else 1
        steps = steps if self.fine else 1

        def at(u):
            # a smooth curve through the keys (Catmull-Rom), u counted in keys
            i = min(int(u), len(keys) - 2)
            t = u - i
            k0, k1, k2, k3 = keys[max(i - 1, 0)], keys[i], keys[i + 1], keys[min(i + 2, len(keys) - 1)]
            out = {}
            for f in ("z", "xc", "af", "ar", "b", "n", "yc"):
                p0, p1, p2, p3 = (k.get(f, 0.0 if f in ("xc", "yc") else 2.4 if f == "n" else None) for k in (k0, k1, k2, k3))
                out[f] = 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3)
            return out
        rings = []
        total = (len(keys) - 1) * between
        for j in range(total + 1):
            k = at(j / between)
            rings.append([Vector((x, y, k["z"])) for x, y in outline(k["xc"], k["af"], k["ar"], k["b"], k["n"], sides, k["yc"])])
        last = at(len(keys) - 1.0)
        mid = Vector((last["xc"] + (last["af"] - last["ar"]) * 0.5, last["yc"], last["z"]))
        if top > 0:
            for s in range(1, steps + 1):
                a = math.pi / 2 * s / steps
                inset, rise = top * (1 - math.cos(a)), top * math.sin(a)
                ring = []
                for x, y in outline(last["xc"], last["af"] - inset, last["ar"] - inset, last["b"] - inset, last["n"] if n_top is None else lerp(last["n"], n_top, s / steps), sides, last["yc"]):
                    ring.append(Vector((x, y, last["z"] + rise)))
                rings.append(ring)
            mid.z += top
        if part:
            self.m.use(part)
        self.m.skin(rings, paint, (False, False), True)
        n = len(rings[-1])
        self.m.add(rings[-1] + [mid], [(k, (k + 1) % n, n) for k in range(n)], top_paint or paint, True, lambda p: Z)
        if bottom:
            lowm = sum(rings[0], Vector()) / n
            self.m.add(rings[0] + [lowm], [(k, (k + 1) % n, n) for k in range(n)], "shade", False, lambda p: -Z)
        return rings

    # -- small fittings ------------------------------------------------------------------------------
    def bolt(self, at, facing, r=0.014, high=0.008, paint="upper", sides=6, part=None):
        if not self.fine:
            return
        self.lathe(at, facing, [(high, 0.0), (high, r * 0.85), (0.0, r), (0.0, 0.0)], paint, sides, False, part)

    def rivet(self, at, facing, r=0.011, paint="upper", part=None):
        if not self.fine:
            return
        self.lathe(at, facing, [(r * 0.62, 0.0), (r * 0.5, r * 0.55), (r * 0.2, r * 0.92), (0.0, r)], paint, 6, True, part)

    def rivets(self, a, b, count, facing, r=0.011, paint="upper", ends=True, part=None):
        """A row of rivets from a to b."""
        if not self.fine or count < 1:
            return
        a, b = Vector(a), Vector(b)
        for k in range(count):
            t = (k / (count - 1) if count > 1 else 0.5) if ends else (k + 0.5) / count
            self.rivet(a.lerp(b, t), facing, r, paint, part)

    def bolts(self, at, facing, ring_r, count, r=0.014, high=0.008, paint="upper", start=0.0, part=None):
        """A ring of bolt heads."""
        if not self.fine:
            return
        axis, side, up = frame(facing)
        for k in range(count):
            a = start + 2 * math.pi * k / count
            self.bolt(Vector(at) + (side * math.cos(a) + up * math.sin(a)) * ring_r, axis, r, high, paint, part=part)

    def handle(self, a, b, out, r=0.008, reach=0.045, paint="upper", part=None):
        """A grab handle between two feet, standing `reach` off the plate."""
        if not self.fine:
            return
        a, b, o = Vector(a), Vector(b), Vector(out).normalized() * reach
        k = 0.25
        self.tube([a, a + o * 0.8, a.lerp(b, k * 0.3) + o, a.lerp(b, 1 - k * 0.3) + o, b + o * 0.8, b], r, paint, 6, part)

    def hinge(self, a, b, r=0.014, paint="upper", part=None):
        if not self.fine:
            return
        self.rod(a, b, r, paint, 8, part=part)

    def weld(self, a, b, facing, r=0.007, paint="upper", part=None):
        """A weld bead along a join: a thin ragged rod."""
        if not self.fine:
            return
        a, b = Vector(a), Vector(b)
        k = max(2, int((b - a).length / 0.05))
        g = self.rng
        f = Vector(facing).normalized()
        pts = [a.lerp(b, i / k) + f * g.uniform(-0.0015, 0.0015) for i in range(k + 1)]
        self.tube(pts, r, paint, 5, part)
