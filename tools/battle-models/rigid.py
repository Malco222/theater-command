"""A battle model made of rigid parts: the ships and aircraft of the real-time battles.

A model is one mesh whose every face belongs to one PART (the hull, a turret, a gun, an airscrew, an outer wing panel).
Parts hang from one another and each has a pivot; a CLIP moves them (a turret trains, a gun runs back, a ship rolls
over and goes down, a wing comes off). Nothing here decides anything: the battle's numbers say what happened and the
clip acts it out.

Three things come out, all from the same numbers:
  * the game's own model file (Content/Models/<Name>.json), with the keys it has today (near, far: points, facings,
    paints, triangles, blend, soot, wear; colours) and four ADDED keys: `part` for every point, `parts`, `sockets`,
    `clips`. Today's reader ignores what it does not know, so the file still loads as a plain model.
  * a Blender mesh on an armature (one bone a part), for the pictures and for the .glb and .fbx beside the file.
  * the clips as Blender actions.

AXES. Everything is modelled as Blender has it: metres, x forward, z up, and y to the model's LEFT (port). The game's
file has y to the RIGHT, so the writer mirrors y (and turns the triangles round, and mirrors every axis and turn with
it). PORT and STBD below say which way is which while modelling.

TURNS. A part that turns has an `axis`; a turn of +t degrees about it carries a point p to
    p cos t + (axis x p) sin t + axis (axis . p)(1 - cos t)
about the part's pivot, the cross product by the usual formula. Axes are chosen so that plus is the natural way:
a turret trains to starboard, a gun lifts, an airscrew turns as the pilot sees it.
"""

import json
import math
import os

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

PORT, STBD = 1.0, -1.0
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def V(*a):
    return Vector(a[0]) if len(a) == 1 else Vector(a)


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def hex_rgb(h):
    """'5F665C' -> linear RGB."""
    def lin(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return tuple(lin(int(h[i:i + 2], 16)) for i in (0, 2, 4))


def mix_hex(a, b, t):
    return "".join("%02X" % int(round(int(a[i:i + 2], 16) * (1 - t) + int(b[i:i + 2], 16) * t)) for i in (0, 2, 4))


class Part:
    def __init__(self, name, parent, pivot, axis=None, limits=None, slide=None, slide_limits=None, kind="fixed", note=""):
        self.name, self.parent, self.pivot = name, parent, Vector(pivot)
        self.axis = Vector(axis).normalized() if axis is not None else None
        self.limits = limits
        self.slide = Vector(slide).normalized() if slide is not None else None
        self.slide_limits = slide_limits
        self.kind, self.note = kind, note


def ring_cut(verts, faces, which, hub, axis, r_cut):
    """Cuts the faces numbered in `which` where they cross a radius from an axis, so that each piece lies wholly
    inside it or wholly outside. `verts` and `faces` are changed in place (new points and faces go on their ends).
    Returns (inside, outside, born): the numbers of the pieces inside and outside, and for every face added the
    face it was cut from, so that the caller can give it the same paint, part and so on."""
    hub, axis = Vector(hub), Vector(axis).normalized()

    def radius(i):
        d = verts[i] - hub
        return (d - axis * d.dot(axis)).length

    made, inside, outside, born = {}, [], [], []

    def between(a, b):
        key = (min(a, b), max(a, b))
        if key not in made:
            ra, rb = radius(a), radius(b)
            verts.append(verts[a].lerp(verts[b], (r_cut - ra) / (rb - ra)))
            made[key] = len(verts) - 1
        return made[key]

    def put(ring, where, like, at=None):
        fans = [tuple(ring)] if len(ring) <= 4 else [(ring[0], ring[j], ring[j + 1]) for j in range(1, len(ring) - 1)]
        for n, f in enumerate(fans):
            if at is not None and n == 0:
                faces[at] = f
                where.append(at)
                continue
            faces.append(f)
            born.append((len(faces) - 1, like))
            where.append(len(faces) - 1)

    for k in list(which):
        f = faces[k]
        out = [radius(i) > r_cut + 1e-6 for i in f]
        if all(out):
            outside.append(k)
            continue
        if not any(out):
            inside.append(k)
            continue
        a_in, a_out = [], []
        for j, a in enumerate(f):
            (a_out if out[j] else a_in).append(a)
            if out[j] != out[(j + 1) % len(f)]:
                c = between(a, f[(j + 1) % len(f)])
                a_in.append(c)
                a_out.append(c)
        put(a_in, inside, k, at=k)
        put(a_out, outside, k)
    return inside, outside, born


class Model:
    """Collects the faces of a model, part by part, and what moves them."""

    def __init__(self, name, title, kind, paints, fine=True):
        self.name, self.title, self.kind, self.fine = name, title, kind, fine
        self.paints = dict(paints)                       # paint name -> hex, in order
        self.paint_at = {k: i for i, k in enumerate(self.paints)}
        self.parts, self.part_at = [], {}
        self.verts, self.faces, self.fpaint, self.fpart, self.fsmooth = [], [], [], [], []
        # (a sprayed coat: a face marked in fcoat takes its paint corner by corner from its points (vcoat), and a point
        #  says how far it is into the second upper colour (vblend) and how sooted it is (vsoot))
        self.fcoat, self.vcoat, self.vblend, self.vsoot = [], [], [], []
        self.sockets, self.clips, self.marks = [], {}, []
        self.info = {}
        self.cur = 0
        self.weather = None                              # f(point, facing, paint name, part name) -> (wear, soot) or (wear, soot, salt), each 0..1
        self.lying = {}                                  # the points that lie on a skin (a ribbon's), and how far off it: see `lay_on_skin`
        self.uv = {}                                     # a point's place on a picture laid on it (a decal's u, v; 0..1), where it has one

    ON = ".on"                                           # (what a ribbon's paint is called: the skin's own, with this after it)

    def on_paint(self, paint):
        """The paint of what lies on a skin of `paint`: the same colour under its own name, so that the pictures (and
        the game) can treat what lies on a skin differently from the skin: it stops no light on its way to it."""
        name = paint + Model.ON
        if name not in self.paints:
            self.paints[name] = self.paints[paint]
            self.paint_at[name] = len(self.paints) - 1
        return name

    # ---- parts -------------------------------------------------------------------------------------
    def part(self, name, parent=None, pivot=(0, 0, 0), **kw):
        if name in self.part_at:
            raise ValueError("part twice: " + name)
        p = Part(name, self.part_at[parent] if parent is not None else -1, pivot, **kw)
        self.part_at[name] = len(self.parts)
        self.parts.append(p)
        return name

    def use(self, name):
        """Faces added from now on belong to this part."""
        self.cur = self.part_at[name]
        return self

    def socket(self, name, at, facing=(1, 0, 0), part=None, note=""):
        self.sockets.append({"name": name, "part": self.part_at[part] if part else self.cur, "at": Vector(at), "facing": Vector(facing).normalized(), "note": note})

    # ---- faces -------------------------------------------------------------------------------------
    def add(self, verts, faces, paint, smooth=True, out=None, part=None, uv=None):
        """verts: points; faces: tuples of indices into them; paint: a name, or f(middle, facing) -> name.
        out: f(middle) -> the way that face should look; faces looking the other way are turned round.
        uv: each point's (u, v) on a picture laid on these faces (a decal), if they carry one."""
        base = len(self.verts)
        vs = [Vector(v) for v in verts]
        self.verts.extend(vs)
        if uv is not None:
            for i, q in enumerate(uv):
                self.uv[base + i] = (float(q[0]), float(q[1]))
        self.vcoat.extend([-1] * len(vs))
        self.vblend.extend([0.0] * len(vs))
        self.vsoot.extend([0.0] * len(vs))
        pi = self.part_at[part] if part is not None else self.cur
        for f in faces:
            if len(set(f)) < 3:
                continue
            a, b, c = vs[f[0]], vs[f[1]], vs[f[2]]
            n = (b - a).cross(c - a)
            if len(f) > 3:
                n = n + (vs[f[2]] - a).cross(vs[f[3]] - a)
            if n.length < 1e-12:
                continue
            mid = sum((vs[i] for i in f), Vector()) / len(f)
            if out is not None and n.dot(out(mid)) < 0.0:
                f = tuple(reversed(f))
                n = -n
            name = paint(mid, n.normalized()) if callable(paint) else paint
            self.faces.append(tuple(base + i for i in f))
            self.fpaint.append(self.paint_at[name])
            self.fpart.append(pi)
            self.fsmooth.append(smooth)
            self.fcoat.append(False)

    def cut_ring(self, which, hub, axis, r_cut):
        """Cuts the faces numbered in `which` at a radius from an axis (see `ring_cut`); each new face is painted and
        placed as the one it was cut from. Returns (inside, outside): the numbers of the pieces."""
        n = len(self.verts)
        inside, outside, born = ring_cut(self.verts, self.faces, which, hub, axis, r_cut)
        more = len(self.verts) - n
        self.vcoat.extend([-1] * more)
        self.vblend.extend([0.0] * more)
        self.vsoot.extend([0.0] * more)
        for _, like in born:
            self.fpaint.append(self.fpaint[like])
            self.fpart.append(self.fpart[like])
            self.fsmooth.append(self.fsmooth[like])
            self.fcoat.append(self.fcoat[like])
        return inside, outside

    def raw(self, verts, faces, paints, part, coat=None, blend=None, soot=None, by_point=None, smooth=True):
        """Points and faces taken as they are (nothing turned round): `paints` a name for every face; `by_point` says,
        face by face, whether its corners take the paint of their points instead (`coat`, a name or None per point)."""
        base = len(self.verts)
        self.verts.extend(Vector(v) for v in verts)
        n = len(verts)
        self.vcoat.extend([self.paint_at[c] if c is not None else -1 for c in (coat or [None] * n)])
        self.vblend.extend(list(blend) if blend is not None else [0.0] * n)
        self.vsoot.extend(list(soot) if soot is not None else [0.0] * n)
        pi = self.part_at[part]
        for k, f in enumerate(faces):
            self.faces.append(tuple(base + i for i in f))
            self.fpaint.append(self.paint_at[paints[k]])
            self.fpart.append(pi)
            self.fsmooth.append(smooth)
            self.fcoat.append(bool(by_point[k]) if by_point is not None else False)
        return base

    def skin(self, rings, paint, caps=(False, False), smooth=True, closed=True, cap_paint=None, inside=False):
        """A skin over rings of points (each ring the same count, going round the same way)."""
        n = len(rings[0])
        verts = [p for ring in rings for p in ring]
        faces = []
        mids = [sum((Vector(p) for p in ring), Vector()) / n for ring in rings]
        last = n if closed else n - 1
        for r in range(len(rings) - 1):
            for k in range(last):
                k1 = (k + 1) % n
                faces.append((r * n + k, r * n + k1, (r + 1) * n + k1, (r + 1) * n + k))

        def out(mid):
            # (the nearest point of the line through the rings' middles)
            best, way = 1e18, Z
            for a, b in zip(mids, mids[1:]):
                ab = b - a
                t = min(max((mid - a).dot(ab) / max(ab.length_squared, 1e-12), 0.0), 1.0)
                d = mid - (a + ab * t)
                if d.length < best:
                    best, way = d.length, d
            return -way if inside else way
        self.add(verts, faces, paint, smooth, out)
        for end, on in ((0, caps[0]), (-1, caps[1])):
            if on:
                ring = [Vector(p) for p in rings[end]]
                way = (mids[0] - mids[1]) if end == 0 else (mids[-1] - mids[-2])
                if way.length < 1e-9:
                    way = Z
                self.add(ring + [mids[end]], [(k, (k + 1) % n, n) for k in range(n)], cap_paint or paint, False, lambda m, w=way: w)

    def sheet(self, rows, paint, up=Z, smooth=True):
        """An open sheet over rows of points (a deck, an awning)."""
        n = len(rows[0])
        verts = [p for row in rows for p in row]
        faces = [(r * n + k, r * n + k + 1, (r + 1) * n + k + 1, (r + 1) * n + k) for r in range(len(rows) - 1) for k in range(n - 1)]
        self.add(verts, faces, paint, smooth, lambda m: up)

    def prism(self, plan, z0, z1, paint, top=None, top_paint=None, bottom=False, smooth=False, shift=(0.0, 0.0)):
        """A plan (points x, y going round) raised from z0 to z1; `top` is another plan for the upper edge (sloped walls);
        `shift` moves the upper plan (a leaning funnel)."""
        top = top or plan
        n = len(plan)
        lo = [Vector((p[0], p[1], z0)) for p in plan]
        hi = [Vector((p[0] + shift[0], p[1] + shift[1], z1)) for p in top]
        c = sum(lo, Vector()) / n
        ch = sum(hi, Vector()) / n
        verts = lo + hi
        faces = [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)]
        axis_a, axis_b = c, ch

        def out(mid):
            t = min(max((mid.z - z0) / max(z1 - z0, 1e-9), 0.0), 1.0)
            return mid - (axis_a + (axis_b - axis_a) * t)
        self.add(verts, faces, paint, smooth, out)
        self.add(hi + [ch], [(k, (k + 1) % n, n) for k in range(n)], top_paint or paint, False, lambda m: Z)
        if bottom:
            self.add(lo + [c], [(k, (k + 1) % n, n) for k in range(n)], paint, False, lambda m: -Z)

    @staticmethod
    def round_rect(x0, x1, y0, y1, r, steps=3):
        fore, rear = max(x0, x1), min(x0, x1)
        left, right = max(y0, y1), min(y0, y1)
        r = min(r, (fore - rear) * 0.49, (left - right) * 0.49)
        if r <= 1e-6:
            return [(fore, left), (rear, left), (rear, right), (fore, right)]
        plan = []
        for q, (cx, cy) in enumerate(((fore - r, left - r), (rear + r, left - r), (rear + r, right + r), (fore - r, right + r))):
            for k in range(steps + 1):
                a = (q + k / steps) * math.pi / 2.0
                plan.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
        return plan

    @staticmethod
    def oval(cx, cy, a, b, n=16):
        return [(cx + math.cos(2 * math.pi * k / n) * a, cy + math.sin(2 * math.pi * k / n) * b) for k in range(n)]

    def house(self, x0, x1, half, z, high, paint, r=0.6, y=0.0, top_paint=None, steps=3, taper=0.0, square=(False, False), smooth=False):
        """`square`: leave the corners of the fore end, the after end, unrounded (a house built in two lengths end to end)."""
        plan = self.round_rect(x0, x1, y - half, y + half, r, steps)
        if (square[0] or square[1]) and len(plan) > 4:
            n = steps + 1
            fore, rear = max(x0, x1), min(x0, x1)
            c = [plan[i * n:(i + 1) * n] for i in range(4)]          # fore-left, rear-left, rear-right, fore-right
            if square[0]:
                c[0], c[3] = [(fore, y + half)], [(fore, y - half)]
            if square[1]:
                c[1], c[2] = [(rear, y + half)], [(rear, y - half)]
            plan = [q for corner in c for q in corner]
        top = None
        if taper:
            top = self.round_rect(x0 - math.copysign(taper, x0 - x1), x1 + math.copysign(taper, x0 - x1), y - half + taper, y + half - taper, max(r - taper, 0.05), steps)
        self.prism(plan, z, z + high, paint, top=top, top_paint=top_paint, smooth=smooth)

    def box(self, mid, size, paint, yaw=0.0, pitch=0.0, roll=0.0):
        hx, hy, hz = size[0] / 2.0, size[1] / 2.0, size[2] / 2.0
        rot = Matrix.Rotation(math.radians(yaw), 3, "Z") @ Matrix.Rotation(math.radians(-pitch), 3, "Y") @ Matrix.Rotation(math.radians(roll), 3, "X")
        m = Vector(mid)
        vs = [m + rot @ Vector((sx * hx, sy * hy, sz * hz)) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
        faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        self.add(vs, faces, paint, False, lambda p: p - m)

    def lathe(self, base, axis, profile, paint, sides=10, caps=(True, True), smooth=True):
        """Turned on a lathe: profile is (distance along the axis, radius) pairs from the base."""
        base, axis = Vector(base), Vector(axis).normalized()
        side = axis.cross(Z)
        if side.length < 1e-6:
            side = axis.cross(X)
        side.normalize()
        up = side.cross(axis)
        rings = [[base + axis * d + (side * math.cos(2 * math.pi * k / sides) + up * math.sin(2 * math.pi * k / sides)) * max(r, 1e-4) for k in range(sides)]
                 for d, r in profile]
        self.skin(rings, paint, caps, smooth)

    def rod(self, a, b, ra, paint, rb=None, sides=8, caps=(True, True), smooth=True):
        a, b = Vector(a), Vector(b)
        if (b - a).length < 1e-6:
            return
        self.lathe(a, b - a, [(0.0, ra), ((b - a).length, ra if rb is None else rb)], paint, sides, caps, smooth)

    def ball(self, at, radii, paint, u=12, v=8, top_only=False):
        at = Vector(at)
        if isinstance(radii, (int, float)):
            radii = (radii, radii, radii)
        lo = 0 if top_only else -v // 2
        rings = []
        for j in range(lo, v // 2 + 1):
            a = math.pi * j / v
            if abs(j) == v // 2:
                continue
            rings.append([at + Vector((math.cos(a) * math.cos(2 * math.pi * k / u) * radii[0], math.cos(a) * math.sin(2 * math.pi * k / u) * radii[1], math.sin(a) * radii[2])) for k in range(u)])
        self.skin(rings, paint, (False, False), True)
        n = u
        top = at + Vector((0, 0, radii[2]))
        self.add(rings[-1] + [top], [(k, (k + 1) % n, n) for k in range(n)], paint, True, lambda m: Z)
        if top_only:
            self.add(rings[0] + [at], [(k, (k + 1) % n, n) for k in range(n)], paint, False, lambda m: -Z)
        else:
            bot = at - Vector((0, 0, radii[2]))
            self.add(rings[0] + [bot], [(k, (k + 1) % n, n) for k in range(n)], paint, True, lambda m: -Z)

    def disc(self, at, facing, r, paint, sides=10, oval=1.0):
        at, facing = Vector(at), Vector(facing).normalized()
        side = facing.cross(Z)
        if side.length < 1e-6:
            side = facing.cross(X)
        side.normalize()
        up = side.cross(facing)
        ring = [at + (side * math.cos(2 * math.pi * k / sides) * oval + up * math.sin(2 * math.pi * k / sides)) * r for k in range(sides)]
        self.add(ring + [at], [(k, (k + 1) % sides, sides) for k in range(sides)], paint, False, lambda m: facing)

    def strip(self, points, facings, half, paint, lift=0.01, taper=None):
        """A thin ribbon lying on a skin along a row of points: a scribed line, a painted stripe."""
        pts = [Vector(p) for p in points]
        if len(pts) < 2:
            return
        verts, faces = [], []
        for k, (p, n) in enumerate(zip(pts, facings)):
            n = Vector(n).normalized()
            along = pts[min(k + 1, len(pts) - 1)] - pts[max(k - 1, 0)]
            side = along.cross(n)
            if side.length < 1e-9:
                side = Y.copy()
            side.normalize()
            w = half * (taper(k / (len(pts) - 1)) if taper else 1.0)
            verts += [p + n * lift + side * w, p + n * lift - side * w]
        for k in range(len(pts) - 1):
            faces.append((2 * k, 2 * k + 1, 2 * k + 3, 2 * k + 2))
        fn = [Vector(n).normalized() for n in facings]
        self.add(verts, faces, paint, True, lambda m: fn[min(range(len(pts)), key=lambda i: (pts[i] - m).length_squared)])

    def band(self, left, mid, right, facings, paint, lift=0.004):
        """A ribbon lying on a skin, three points across (edge, middle, edge) at every place along it, so that what its
        middle points carry (rust, dried salt: see `weather`) fades away to its edges. Its points are `lying`: when the
        mesh is made they are brought down onto the skin under them and take its shading (`lay_on_skin`), so nothing
        tells the ribbon from the skin but what it carries. `paint` is the skin's; the ribbon's own is that with ".on"
        after it (`on_paint`). Returns its middle points, in order along it."""
        return self.bands([left, mid, right], facings, paint, lift)[1]

    def bands(self, lines, facings, paint, lift=0.004):
        """As `band`, with any number of points across (five give a streak a core and a softer edge). Returns each
        line's points as they were laid, in order along it."""
        fn = [Vector(n).normalized() for n in facings]
        lines = [[Vector(q) for q in line] for line in lines]
        n, w = len(lines[0]), len(lines)
        if n < 2:
            return [[] for _ in lines]
        verts, faces = [], []
        for k in range(n):
            verts += [line[k] + fn[k] * lift for line in lines]
        for k in range(n - 1):
            for j in range(w - 1):
                faces.append((w * k + j, w * k + j + 1, w * (k + 1) + j + 1, w * (k + 1) + j))
        mid = lines[w // 2]
        base = len(self.verts)
        self.add(verts, faces, self.on_paint(paint), True, lambda m: fn[min(range(n), key=lambda i: (mid[i] - m).length_squared)])
        for i in range(len(verts)):
            self.lying[base + i] = lift
        return [[verts[w * k + j] for k in range(n)] for j in range(w)]

    def ribbon(self, points, facings, half, paint, lift=0.004, taper=None, core=None):
        """A `band` along a row of points, so far to either side of them (`taper`: f(0..1 along it) -> a share of that).
        With `core` (a share of its half width) it is five points across: edge, shoulder, middle, shoulder, edge; and
        what comes back is all five lines (`bands`) rather than the middle one."""
        pts = [Vector(p) for p in points]
        if len(pts) < 2:
            return [] if core is None else [[] for _ in range(5)]
        left, right, l2, r2 = [], [], [], []
        for k, (p, n) in enumerate(zip(pts, facings)):
            n = Vector(n).normalized()
            along = pts[min(k + 1, len(pts) - 1)] - pts[max(k - 1, 0)]
            side = along.cross(n)
            if side.length < 1e-9:
                side = Y.copy()
            side.normalize()
            w = half * (taper(k / (len(pts) - 1)) if taper else 1.0)
            left.append(p + side * w)
            right.append(p - side * w)
            if core is not None:
                l2.append(p + side * w * core)
                r2.append(p - side * w * core)
        if core is not None:
            return self.bands([left, l2, pts, r2, right], facings, paint, lift)
        return self.band(left, pts, right, facings, paint, lift)

    def tub(self, at, r, high, paint, wall=0.08, sides=14, floor=None):
        """A gun tub: a low round wall, open at the top."""
        at = Vector(at)
        self.lathe(at, Z, [(0.0, r), (high, r)], paint, sides, (False, False), True)
        inner = [[at + Vector((math.cos(2 * math.pi * k / sides) * (r - wall), math.sin(2 * math.pi * k / sides) * (r - wall), h)) for k in range(sides)] for h in (0.0, high)]
        self.skin(inner, paint, (False, False), True, inside=True)
        ring_o = [at + Vector((math.cos(2 * math.pi * k / sides) * r, math.sin(2 * math.pi * k / sides) * r, high)) for k in range(sides)]
        self.add(ring_o + inner[1], [(k, (k + 1) % sides, sides + (k + 1) % sides, sides + k) for k in range(sides)], paint, False, lambda m: Z)
        if floor:
            self.disc(at + Vector((0, 0, 0.02)), Z, r - wall, floor, sides)

    # ---- counting ----------------------------------------------------------------------------------
    def triangles(self):
        return sum(len(f) - 2 for f in self.faces)


# ---- to Blender -------------------------------------------------------------------------------------

ROUGH = {"glass": 0.08, "planks": 0.85, "sea": 0.1, "canvas": 0.95, "rope": 0.95, "brass": 0.35, "steel": 0.45, "blade": 0.4}
METAL = {"brass": 1.0, "steel": 0.6, "bare": 0.9}


def material(model, paint, look=None):
    """The Blender material of a paint: its colour, weathered by what each point carries (rust and soot)."""
    name = model.name + ":" + paint
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    rgb = hex_rgb(model.paints[paint])
    key = paint.split(".")[0]                             # (so "hull.on", what lies on the hull, is as rough as the hull)
    bsdf.inputs["Roughness"].default_value = ROUGH.get(key, 0.62)
    bsdf.inputs["Metallic"].default_value = METAL.get(key, 0.0)
    col = nt.nodes.new("ShaderNodeRGB")
    col.outputs[0].default_value = (*rgb, 1.0)
    att = nt.nodes.new("ShaderNodeVertexColor")
    att.layer_name = "Col"
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    nt.links.new(col.outputs[0], mix.inputs["A"])
    nt.links.new(att.outputs["Color"], mix.inputs["B"])
    nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    if look:
        look(paint, m, nt, bsdf, mix)
    m.diffuse_color = (*rgb, 1.0)
    return m


def tint_of(model, wear, soot, paint_idx, face_idx=None, blend=None, salt=None):
    """What wear, soot, dried salt and a sprayed coat do to a paint, as a colour to multiply the face's own paint by (the
    .glb carries it as COLOR_0). `paint_idx` is the paint of each corner, `face_idx` the paint of the face it is a corner of."""
    names = list(model.paints)
    rust, black = np.array(hex_rgb(model.info.get("rust", "6E4A34"))), np.array(hex_rgb(model.info.get("soot", "1C1A19")))
    show = model.info.get("wear_show", 1.0)
    cols = np.array([hex_rgb(model.paints[n]) for n in names])
    base = cols[paint_idx if face_idx is None else face_idx]
    shown = cols[paint_idx]
    if blend is not None and "upper2" in model.paint_at:
        into = np.where(paint_idx == model.paint_at["upper"], blend, 0.0)[:, None]
        shown = shown * (1.0 - into) + cols[model.paint_at["upper2"]][None, :] * into
    wear = wear * show
    worn = shown * (1.0 - wear[:, None]) + rust[None, :] * wear[:, None]
    if salt is not None:      # (dried salt bleaches what it lies on, rust and all, toward a pale grey)
        pale = np.array(hex_rgb(model.info.get("salt", "D2D5CF")))
        worn = worn * (1.0 - salt[:, None]) + pale[None, :] * salt[:, None]
    worn = worn * (1.0 - soot[:, None]) + black[None, :] * soot[:, None]
    return np.clip(worn / np.maximum(base, 1e-4), 0.0, 8.0)


def lay_on_skin(model, me):
    """The points that lie on a skin (`Model.lying`: a ribbon's) are brought down onto the nearest face of that skin
    (the skin of the paint they are named for), stood off it by their lift, and given its shading there. (Left to
    itself a ribbon faces a degree or two away from the skin under it, which a low sun shows as a panel, and it hovers
    wherever the skin runs flat between its own points, which shows as a line of shadow.) The skin is taken as the
    triangles it is drawn with, for a four-sided face that is not flat is two faces. A ribbon should not lie across
    a ridge of the skin deeper than its lift: the ridge would stand through it."""
    from mathutils.bvhtree import BVHTree
    from mathutils.interpolate import poly_3d_calc
    lying = model.lying
    nl = len(me.loops)
    cn = np.zeros(nl * 3)
    me.corner_normals.foreach_get("vector", cn)
    cn = cn.reshape(-1, 3)
    names = list(model.paints)
    skin_of, own = {}, {}
    for i, f in enumerate(model.faces):
        name = names[model.fpaint[i]]
        if any(v in lying for v in f):
            for v in f:
                own[v] = name[:-len(Model.ON)] if name.endswith(Model.ON) else name
        else:
            skin_of[i] = name
    me.calc_loop_triangles()
    skins = {}
    for tri in me.loop_triangles:
        name = skin_of.get(tri.polygon_index)
        if name is not None:
            skins.setdefault(name, []).append((tuple(tri.vertices), tuple(tri.loops)))
    points = [tuple(v) for v in model.verts]
    trees, facing = {}, {}
    for v, lift in lying.items():
        pa = own.get(v)
        if pa not in skins:
            continue
        if pa not in trees:
            trees[pa] = BVHTree.FromPolygons(points, [t[0] for t in skins[pa]])
        # (the nearest face that looks the way the ribbon does: by a deck's edge the deck itself may be as near as the side)
        own_way = me.vertices[v].normal
        near = [hit for hit in trees[pa].find_nearest_range(model.verts[v], lift + 0.05) if hit[1].dot(own_way) > 0.5]
        loc, _, idx, dist = min(near, key=lambda hit: hit[3]) if near else trees[pa].find_nearest(model.verts[v])
        if loc is None or dist > 0.25:               # (nothing of its own paint under it: it stays as it was made)
            continue
        corners, loops = skins[pa][idx]
        w = poly_3d_calc([model.verts[j] for j in corners], loc)
        n = Vector()
        for l, share in zip(loops, w):
            n += Vector(cn[l]) * share
        if n.length < 1e-9:
            continue
        n.normalize()
        me.vertices[v].co = loc + n * lift
        facing[v] = n
    if facing:
        lv = np.zeros(nl, dtype=np.int32)
        me.loops.foreach_get("vertex_index", lv)
        for l in range(nl):
            n = facing.get(int(lv[l]))
            if n is not None:
                cn[l] = n
        me.normals_split_custom_set([tuple(n) for n in cn])
        me.update()


def to_blender(model, look=None, crease=38.0, collection=None):
    """The model as one mesh on an armature, a bone for every part. Returns (mesh object, armature object)."""
    me = bpy.data.meshes.new(model.name)
    me.from_pydata([tuple(v) for v in model.verts], [], model.faces, shade_flat=False)
    names = list(model.paints)
    used = sorted(set(model.fpaint))
    slot = {p: i for i, p in enumerate(used)}
    for p in used:
        me.materials.append(material(model, names[p], look))
    me.polygons.foreach_set("material_index", [slot[p] for p in model.fpaint])
    me.polygons.foreach_set("use_smooth", model.fsmooth)
    me.update()
    me.set_sharp_from_angle(angle=math.radians(crease))
    # which part every point belongs to (a point is never shared between parts: each piece brings its own)
    nv = len(model.verts)
    vpart = np.zeros(nv, dtype=np.int32)
    vpaint = np.zeros(nv, dtype=np.int32)
    for f, pt, pa in zip(model.faces, model.fpart, model.fpaint):
        for i in f:
            vpart[i] = pt
            vpaint[i] = pa
    wear = np.zeros(nv)
    soot = np.array(model.vsoot, dtype=float) if len(model.vsoot) == nv else np.zeros(nv)
    salt = np.zeros(nv)
    if model.weather:
        normals = np.zeros(nv * 3)
        me.vertices.foreach_get("normal", normals)
        normals = normals.reshape(-1, 3)
        for i, v in enumerate(model.verts):
            got = model.weather(v, normals[i], names[vpaint[i]], model.parts[vpart[i]].name)
            w, s = got[0], got[1]
            wear[i], soot[i] = min(max(w, 0.0), 1.0), min(max(max(s, soot[i]), 0.0), 1.0)
            if len(got) > 2:
                salt[i] = min(max(got[2], 0.0), 1.0)
    if model.lying:
        lay_on_skin(model, me)
    # corner by corner: the paint (the face's own, or the point's where the coat is sprayed), and the tint that shows it
    nl = len(me.loops)
    lv = np.zeros(nl, dtype=np.int32)
    me.loops.foreach_get("vertex_index", lv)
    total = np.zeros(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("loop_total", total)
    lface = np.repeat(np.array(model.fpaint, dtype=np.int32), total)
    lpaint = lface.copy()
    blend = np.array(model.vblend, dtype=float) if len(model.vblend) == nv else np.zeros(nv)
    if any(model.fcoat):
        sprayed = np.repeat(np.array(model.fcoat, dtype=bool), total)
        coat = np.array(model.vcoat, dtype=np.int32)[lv]
        lpaint = np.where(sprayed & (coat >= 0), coat, lpaint)
    lblend = np.where(lpaint == model.paint_at.get("upper", -2), blend[lv], 0.0)
    model._vpart, model._vpaint, model._wear, model._soot, model._lpaint, model._lblend = vpart, vpaint, wear, soot, lpaint, lblend
    model._salt = salt
    tint = tint_of(model, wear[lv], soot[lv], lpaint, lface, lblend, salt[lv] if salt.any() else None)
    col = me.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    col.data.foreach_set("color", np.concatenate([tint, np.ones((len(tint), 1))], axis=1).ravel())
    if getattr(model, "uv", None):                       # (a decal's place on its picture, corner by corner: TEXCOORD_0 in the .glb)
        vu = np.zeros((nv, 2))
        for i, q in model.uv.items():
            if i < nv:
                vu[i] = q
        me.uv_layers.new(name="UVMap").data.foreach_set("uv", vu[lv].ravel())
    ob = bpy.data.objects.new(model.name, me)
    (collection or bpy.context.scene.collection).objects.link(ob)
    for k, p in enumerate(model.parts):
        g = ob.vertex_groups.new(name=p.name)
        idx = np.nonzero(vpart == k)[0]
        if len(idx):
            g.add([int(i) for i in idx], 1.0, "REPLACE")
    # the armature: every bone lies along +Y with no roll, so a bone's own axes are the model's axes
    arm_data = bpy.data.armatures.new(model.name + "_rig")
    arm = bpy.data.objects.new(model.name + "_rig", arm_data)
    (collection or bpy.context.scene.collection).objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    size = max((max(abs(c) for c in v) for v in model.verts), default=1.0)
    bones = []
    for p in model.parts:
        b = arm_data.edit_bones.new(p.name)
        b.head = p.pivot
        b.tail = p.pivot + Vector((0.0, max(size * 0.02, 0.05), 0.0))
        b.roll = 0.0
        if p.parent >= 0:
            b.parent = bones[p.parent]
        b.use_deform = True
        bones.append(b)
    bpy.ops.object.mode_set(mode="OBJECT")
    ob.parent = arm
    mod = ob.modifiers.new("rig", "ARMATURE")
    mod.object = arm
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    arm_data.display_type = "STICK"
    return ob, arm


# ---- clips -------------------------------------------------------------------------------------------

def keys_at(keys, t):
    """Straight-line reading of [[t, v...], ...] at t (held outside its ends)."""
    if t <= keys[0][0]:
        return list(keys[0][1:])
    if t >= keys[-1][0]:
        return list(keys[-1][1:])
    for a, b in zip(keys, keys[1:]):
        if a[0] <= t <= b[0]:
            s = (t - a[0]) / max(b[0] - a[0], 1e-9)
            return [lerp(x, y, s) for x, y in zip(a[1:], b[1:])]
    return list(keys[-1][1:])


def quat_at(keys, t):
    if t <= keys[0][0]:
        return Quaternion(keys[0][1:])
    if t >= keys[-1][0]:
        return Quaternion(keys[-1][1:])
    for a, b in zip(keys, keys[1:]):
        if a[0] <= t <= b[0]:
            s = (t - a[0]) / max(b[0] - a[0], 1e-9)
            return Quaternion(a[1:]).slerp(Quaternion(b[1:]), s)
    return Quaternion(keys[-1][1:])


def pose_of(model, clip, t):
    """Where a clip puts each part it moves at time t: {part name: (quaternion, move, scale)}."""
    out = {}
    if clip.get("loop") and clip["long"] > 0:
        t = t % clip["long"]
    for tr in clip["tracks"]:
        part = model.parts[model.part_at[tr["part"]]]
        q, mv, sc = Quaternion(), Vector(), Vector((1, 1, 1))
        if "turn" in tr:
            q = Quaternion(part.axis, math.radians(keys_at(tr["turn"], t)[0]))
        if "rot" in tr:
            q = quat_at(tr["rot"], t) @ q
        if "slide" in tr:
            mv = part.slide * keys_at(tr["slide"], t)[0]
        if "move" in tr:
            mv = mv + Vector(keys_at(tr["move"], t))
        if "scale" in tr:
            s = keys_at(tr["scale"], t)
            sc = Vector((s[0], s[0], s[0])) if len(s) == 1 else Vector(s)
        out[tr["part"]] = (q, mv, sc)
    return out


def combine(a, b):
    """Two poses of the same part, one after the other."""
    return (a[0] @ b[0], a[1] + b[1], Vector((a[2][0] * b[2][0], a[2][1] * b[2][1], a[2][2] * b[2][2])))


def apply_pose(arm, pose, frame=None, model=None):
    """Sets the armature's bones (and keys them at a frame, when one is given). Parts not named go back to rest.
    A part that both turns and slides (a gun) slides along its own length as its turn has carried it."""
    slides = {p.name for p in model.parts if p.slide is not None and p.axis is not None} if model else ()
    for pb in arm.pose.bones:
        q, mv, sc = pose.get(pb.name, (Quaternion(), Vector(), Vector((1, 1, 1))))
        pb.rotation_quaternion, pb.location, pb.scale = q, (q @ mv if pb.name in slides else mv), sc
        if frame is not None:
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            pb.keyframe_insert("location", frame=frame)
            pb.keyframe_insert("scale", frame=frame)


def merge(*poses):
    """Several poses laid over each other (a ship riding the swell, her turrets trained, a salvo fired)."""
    out = {}
    for pose in poses:
        for k, v in pose.items():
            out[k] = combine(out[k], v) if k in out else v
    return out


def aim(model, part, degrees):
    """The pose of one turning part turned so many degrees."""
    return {part: (Quaternion(model.parts[model.part_at[part]].axis, math.radians(degrees)), Vector(), Vector((1, 1, 1)))}


def place(model, pose, name):
    """Where a socket is, and which way it faces, under a pose: (at, facing) in the model's own axes."""
    s = next(x for x in model.sockets if x["name"] == name)
    p, d, k = Vector(s["at"]), Vector(s["facing"]), s["part"]
    while k >= 0:
        P = model.parts[k]
        q, mv, sc = pose.get(P.name, (Quaternion(), Vector(), Vector((1, 1, 1))))
        if P.slide is not None and P.axis is not None:
            mv = q @ mv
        rel = p - P.pivot
        p = P.pivot + mv + q @ Vector((rel.x * sc[0], rel.y * sc[1], rel.z * sc[2]))
        d = q @ d
        k = P.parent
    return p, d


def carried(model, pose, part, point):
    """Where a point of a part (the part by name; the point as it lies at rest) is under a pose, in the model's own axes."""
    p, k = Vector(point), model.part_at[part]
    while k >= 0:
        P = model.parts[k]
        q, mv, sc = pose.get(P.name, (Quaternion(), Vector(), Vector((1, 1, 1))))
        if P.slide is not None and P.axis is not None:
            mv = q @ mv
        rel = p - P.pivot
        p = P.pivot + mv + q @ Vector((rel.x * sc[0], rel.y * sc[1], rel.z * sc[2]))
        k = P.parent
    return p


def bake_actions(model, arm, fps=30):
    """Every clip as a Blender action on the armature (what the .glb and .fbx carry)."""
    arm.animation_data_create()
    made = []
    for name, clip in model.clips.items():
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        arm.animation_data.action = act
        moved = {tr["part"] for tr in clip["tracks"]}
        n = max(1, int(round(clip["long"] * fps)))
        for f in range(n + 1):
            pose = pose_of(model, clip, min(f / fps, clip["long"] - (1e-6 if clip.get("loop") and f == n else 0.0)) if not (clip.get("loop") and f == n) else 0.0)
            for part in moved:
                pb = arm.pose.bones[part]
                q, mv, sc = pose[part]
                pb.rotation_quaternion, pb.location, pb.scale = q, mv, sc
                pb.keyframe_insert("rotation_quaternion", frame=f + 1)
                pb.keyframe_insert("location", frame=f + 1)
                pb.keyframe_insert("scale", frame=f + 1)
        made.append(act)
    for pb in arm.pose.bones:
        pb.rotation_quaternion, pb.location, pb.scale = Quaternion(), Vector(), Vector((1, 1, 1))
    arm.animation_data.action = None
    return made


# ---- the game's file ---------------------------------------------------------------------------------

def gather(model, ob):
    """The mesh as the game reads it: points, facings, paints, triangles, and the part of every point. y is mirrored
    (the file has y to the right) and the triangles turned round with it."""
    me = ob.data
    me.calc_loop_triangles()
    nl = len(me.loops)
    lv = np.zeros(nl, dtype=np.int32)
    me.loops.foreach_get("vertex_index", lv)
    co = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    cn = np.zeros(nl * 3)
    me.corner_normals.foreach_get("vector", cn)
    cn = cn.reshape(-1, 3)
    tris = np.zeros(len(me.loop_triangles) * 3, dtype=np.int32)
    me.loop_triangles.foreach_get("loops", tris)
    pos = np.round(co[lv], 3)
    nor = np.round(cn, 2)
    part, paint = model._vpart[lv], model._lpaint
    wear, soot, blend = np.round(model._wear[lv] * 100), np.round(model._soot[lv] * 100), np.round(model._lblend * 100)
    salt = np.round(getattr(model, "_salt", np.zeros(len(me.vertices)))[lv] * 100)
    uvl = None
    if getattr(model, "uv", None) and me.uv_layers.get("UVMap") is not None:   # (a decal's place on its picture: kept apart where it differs)
        uvl = np.zeros(nl * 2)
        me.uv_layers["UVMap"].data.foreach_get("uv", uvl)
        uvl = np.round(uvl.reshape(-1, 2), 4)
    key = np.concatenate([pos, nor, part[:, None], paint[:, None], wear[:, None], soot[:, None], blend[:, None], salt[:, None]]
                         + ([uvl] if uvl is not None else []), axis=1)
    _, first, inverse = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inverse = inverse.ravel()
    order = np.argsort(first)
    rank = np.empty_like(order)
    rank[order] = np.arange(len(order))
    first = first[order]
    index = rank[inverse]
    p, n = pos[first].copy(), nor[first].copy()
    p[:, 1] *= -1.0
    n[:, 1] *= -1.0
    t = index[tris].reshape(-1, 3)[:, [0, 2, 1]]

    def flat(a, digits):
        return [round(float(v), digits) if digits else int(v) for v in a.ravel()]
    out = {"points": flat(p, 3), "facings": flat(n, 2), "paints": flat(paint[first], 0), "triangles": flat(t, 0),
           "blend": flat(blend[first], 0), "soot": flat(soot[first], 0), "wear": flat(wear[first], 0), "part": flat(part[first], 0)}
    if salt.any():            # (only a model that carries dried salt has the list: a submarine)
        out["salt"] = flat(salt[first], 0)
    if uvl is not None:       # (only a model with a picture laid on it has the list: u, v for every point, 0 where it has none)
        out["uv"] = flat(uvl[first], 4)
    return out


def mirror(v):
    """A place or a facing, from the model's axes to the file's."""
    return [round(v[0], 4), round(-v[1], 4), round(v[2], 4)]


def mirror_axis(v):
    """An axis of turning, from the model's axes to the file's (a mirror turns a turn the other way)."""
    return [round(-v[0], 4) + 0.0, round(v[1], 4) + 0.0, round(-v[2], 4) + 0.0]


def clip_out(model, clip):
    out = {"long": round(clip["long"], 3), "loop": bool(clip.get("loop")), "about": clip.get("about", ""), "tracks": [], "events": clip.get("events", [])}
    for k in ("stride", "after"):
        if k in clip:
            out[k] = clip[k]
    for tr in clip["tracks"]:
        o = {"part": model.part_at[tr["part"]], "name": tr["part"]}
        if "turn" in tr:
            o["turn"] = [[round(k[0], 3), round(k[1], 2)] for k in tr["turn"]]
        if "slide" in tr:
            o["slide"] = [[round(k[0], 3), round(k[1], 3)] for k in tr["slide"]]
        if "rot" in tr:
            o["rot"] = [[round(k[0], 3), round(k[1], 5), round(-k[2], 5), round(k[3], 5), round(-k[4], 5)] for k in tr["rot"]]
        if "move" in tr:
            o["move"] = [[round(k[0], 3), round(k[1], 3), round(-k[2], 3), round(k[3], 3)] for k in tr["move"]]
        if "scale" in tr:
            o["scale"] = [[round(v, 3) for v in k] for k in tr["scale"]]
        out["tracks"].append(o)
    return out


def write_json(model, near, far, path, extra=None):
    names = list(model.paints)
    data = {
        "name": model.title,
        "about": "Made by tools/battle-models (" + model.kind + "). Metres; x forward, y right, z up. A battle model: `part` says which part "
                 "each point belongs to; `parts` are the parts (parent, pivot, axis of turning, way of sliding); `sockets` are named "
                 "places for flash, smoke, fire and wake; `clips` move the parts. Turns: +t degrees about an axis carries p to "
                 "p cos t + (axis x p) sin t + axis (axis . p)(1 - cos t), about the part's pivot. Paints: " + ", ".join(f"{i} {k}" for i, k in enumerate(names)),
        "kind": model.kind,
        "colours": [model.paints[k] for k in names],
        "paint_names": names,
        "near": near, "far": far,
        "parts": [{"name": p.name, "parent": p.parent, "pivot": mirror(p.pivot), "kind": p.kind,
                   **({"axis": mirror_axis(p.axis)} if p.axis is not None else {}),
                   **({"limits": p.limits} if p.limits is not None else {}),
                   **({"slide": mirror(p.slide)} if p.slide is not None else {}),
                   **({"slide_limits": p.slide_limits} if p.slide_limits is not None else {}),
                   **({"note": p.note} if p.note else {})} for p in model.parts],
        "sockets": [{"name": s["name"], "part": s["part"], "at": mirror(s["at"]), "facing": mirror(s["facing"]), **({"note": s["note"]} if s["note"] else {})} for s in model.sockets],
        "clips": {k: clip_out(model, c) for k, c in model.clips.items()},
    }
    if model.marks:
        data["marks"] = model.marks
    data.update(model.info.get("file", {}))
    if extra:
        data.update(extra)
    xs, ys, zs = near["points"][0::3], near["points"][1::3], near["points"][2::3]
    data["long"], data["wide"], data["high"] = round(max(xs) - min(xs), 2), round(max(ys) - min(ys), 2), round(max(zs) - min(zs), 2)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    return data
