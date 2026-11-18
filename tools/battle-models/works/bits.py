"""The shapes every fieldwork is made of: sandbags, cast concrete, earth, timber, wire, plate and bolts.

Everything is built into a `rigid.Model` (one mesh of parts, see ../rigid.py), in Blender's axes while modelling:
metres, x toward the enemy, z up, y to the piece's LEFT (the file is written with y to the right, as the game has it).
Every shape is our own; nothing is fetched.

Concrete is cast the way it is poured: solid blocks joined and cut by Blender's own tools (`cast`), every edge
chamfered, so an embrasure is a real stepped opening into a real room and nothing is a painted patch.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Quaternion, Vector, noise  # noqa: E402

import rigid  # noqa: E402
from rigid import X, Y, Z, Model  # noqa: E402

# The paints of the works (the game gives `upper` and `upper2` each nation's own field paint; the rest are what they are).
PAINTS = {
    "concrete": "8F8D83", "concrete.in": "55554F", "concrete.old": "7E7B6E", "concrete.mass": "88877E",
    "earth": "7E6C51", "earth.dry": "86724F", "turf": "7A7B4A",
    "grass": "6F7B42", "grass.2": "8D8650", "grass.3": "B0A066",
    "sandbag": "7C6A46", "sandbag.2": "6C5C3E", "sandbag.3": "5F553A",
    "timber": "77604A", "timber.end": "A68C66", "timber.dark": "4B3E33", "planks": "8A7254",
    "steel": "3B3D3F", "steel.worn": "7C7E7E", "rust": "6B4631", "wire": "4B4037", "iron": "2B2B2C",
    "upper": "4E5459", "upper2": "6C6A4E", "rubber": "1D1D1D", "brass": "B38F4B", "canvas": "777050",
    "net": "4C4735", "garnish": "5B693B", "garnish.2": "7B6B46", "garnish.3": "3F4A2C",
    "glass": "1E2629", "sign": "A5362A", "sign.2": "D6C169", "sign.3": "D9D5C9", "void": "0B0B0B",
    "leather": "5A3E2A", "wood.paint": "5E6A48",
}

# Each army's field paint. The first colour comes from the art director's unit table (ruling 78(f): every gun takes its
# owner's paint from that one table); this list is what stands in when the table cannot be read, and holds the second
# colour of an army's camouflage where it used one (the table gives the first only).
NATIONS = {
    "GER": ("4E5459", "3E4347"), "UK": ("65704E", "4A4436"), "USA": ("565C3A", "3F4430"), "SOV": ("4F5E3E", "6B6146"),
    "FRA": ("5E6A48", "7A6844"), "ITA": ("8A7F5E", "5E6442"), "JAP": ("988256", "566240"), "POL": ("66684A", "7B6A48"),
    "_default": ("60664A", "7A6E4E"),
}
# Paint that changed during the war: (from month, first colour, second colour, what it is). Germany's guns are dark grey
# until early 1943 and dark yellow from then on, the same two coats and the same month as its tanks (land kit, PanzerIV).
DATED = {"GER": [(None, "4E5459", "3E4347", "dark grey"), ("1943-02", "A3925E", "82744B", "dark yellow")]}
TABLE = os.environ.get("UNIT_TABLE", "/mnt/project-files/art-director/generic-units/unit-table.json")
_armies = None


def _darker(hexa, k=0.8):
    return "".join(f"{int(int(hexa[i:i + 2], 16) * k):02X}" for i in (0, 2, 4))


def armies():
    """{tag: [(from month or None, first colour, second colour, what it is), ...]} for every country in the unit table
    (and `_default`): its field paint through the war, earliest first."""
    global _armies
    if _armies is None:
        firsts = {k: v[0] for k, v in NATIONS.items()}
        try:
            import json
            table = json.load(open(TABLE))
            firsts.update({k: c["paint"].upper() for k, c in table["countries"].items() if c.get("paint")})
            if table.get("_default", {}).get("paint"):
                firsts["_default"] = table["_default"]["paint"].upper()
        except (OSError, ValueError, KeyError) as e:
            print(f"bits: unit table not read ({e}); using the paints kept here", file=sys.stderr)
        _armies = {}
        for k, first in firsts.items():
            second = NATIONS[k][1] if k in NATIONS and NATIONS[k][0] == first else _darker(first)
            _armies[k] = [(None, first, second, "field paint")]
            if k in DATED:
                _armies[k] = [(when, a if when else first, b, what) for when, a, b, what in DATED[k]]
    return _armies


def paint_of(nation="_default"):
    """An army's two colours. `nation` is its tag, or tag:YYYY-MM for its paint at that month ("GER:1943-06")."""
    tag, _, when = nation.partition(":")
    rows = armies().get(tag) or armies()["_default"]
    row = rows[0]
    for r in rows[1:]:
        if when and when >= r[0]:
            row = r
    return row[1], row[2]


def model(name, title, kind="works", nation="_default", fine=True, more=None):
    paints = dict(PAINTS)
    paints["upper"], paints["upper2"] = paint_of(nation)
    if more:
        paints.update(more)
    m = Model(name, title, kind, paints, fine)
    m.info["nation"] = nation.partition(":")[0]
    m.info["wears"] = nation                      # (tag, or tag:month: whose paint this build wears in a picture)
    m.info["rust"] = "6B4631"
    m.info["soot"] = "1C1A19"
    return m


def rnd(seed):
    return random.Random(seed)


def fbm(p, scale=1.0, octaves=3):
    """Smooth noise about -1..1 at a place."""
    v, a, f = 0.0, 1.0, 1.0 / scale
    tot = 0.0
    for _ in range(octaves):
        v += a * noise.noise(Vector(p) * f)
        tot += a
        a *= 0.5
        f *= 2.1
    return v / tot


# ---- cast concrete (and anything else best made by cutting solids) ---------------------------------------

def _link(me, name="tmp"):
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def s_mesh(verts, faces):
    me = bpy.data.meshes.new("tmp")
    me.from_pydata([tuple(v) for v in verts], [], [tuple(f) for f in faces])
    me.update()
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    return _link(me)


def s_box(lo, hi):
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    return s_prism([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], z0, z1)


def s_prism(plan, z0, z1, top=None, shift=(0.0, 0.0)):
    """A plan (x, y pairs, going round either way) raised from z0 to z1; `top` another plan of the same count."""
    top = top or plan
    n = len(plan)
    verts = [(p[0], p[1], z0) for p in plan] + [(p[0] + shift[0], p[1] + shift[1], z1) for p in top]
    faces = [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)] + [tuple(range(n - 1, -1, -1)), tuple(range(n, 2 * n))]
    return s_mesh(verts, faces)


def s_lathe(base, axis, profile, sides=24):
    """Turned about an axis: profile is (distance along the axis, radius) pairs; closed at both ends."""
    base, axis = Vector(base), Vector(axis).normalized()
    side = axis.cross(Z)
    if side.length < 1e-6:
        side = axis.cross(X)
    side.normalize()
    up = side.cross(axis)
    verts, faces = [], []
    for d, r in profile:
        for k in range(sides):
            a = 2 * math.pi * k / sides
            verts.append(base + axis * d + (side * math.cos(a) + up * math.sin(a)) * max(r, 1e-4))
    for j in range(len(profile) - 1):
        for k in range(sides):
            k1 = (k + 1) % sides
            faces.append((j * sides + k, j * sides + k1, (j + 1) * sides + k1, (j + 1) * sides + k))
    faces.append(tuple(range(sides)))
    faces.append(tuple(range((len(profile) - 1) * sides, len(profile) * sides)))
    return s_mesh(verts, faces)


def s_cyl(base, axis, r, depth, sides=24, r2=None):
    return s_lathe(base, axis, [(0.0, r), (depth, r if r2 is None else r2)], sides)


def s_wedge(pts_lo, pts_hi):
    """A solid between two polygons of the same count (any place, any slope): a splayed embrasure, a ramp."""
    n = len(pts_lo)
    verts = [tuple(p) for p in pts_lo] + [tuple(p) for p in pts_hi]
    faces = [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)] + [tuple(range(n - 1, -1, -1)), tuple(range(n, 2 * n))]
    return s_mesh(verts, faces)


def s_move(ob, at=(0, 0, 0), yaw=0.0, pitch=0.0, roll=0.0):
    """Turn (degrees) and then move a solid's points."""
    rot = Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Rotation(math.radians(-pitch), 4, "Y") @ Matrix.Rotation(math.radians(roll), 4, "X")
    ob.data.transform(Matrix.Translation(Vector(at)) @ rot)
    ob.data.update()
    return ob


def _apply(ob, mod):
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    old = ob.data
    ob.modifiers.remove(mod)
    ob.data = me
    bpy.data.meshes.remove(old)
    return ob


def _bool(a, others, op):
    for b in others:
        mod = a.modifiers.new("b", "BOOLEAN")
        mod.operation = op
        mod.solver = "EXACT"
        mod.object = b
        _apply(a, mod)
        me = b.data
        bpy.data.objects.remove(b)
        bpy.data.meshes.remove(me)
    return a


def s_cut(a, *others):
    return _bool(a, others, "DIFFERENCE")


def s_join(a, *others):
    return _bool(a, others, "UNION")


def s_both(a, *others):
    return _bool(a, others, "INTERSECT")


def s_bevel(ob, width=0.03, seg=2, angle=28.0):
    """Chamfer every edge sharper than `angle` (cast concrete has no knife edges; nor has anything else)."""
    mod = ob.modifiers.new("bev", "BEVEL")
    mod.width, mod.segments, mod.limit_method, mod.angle_limit = width, seg, "ANGLE", math.radians(angle)
    mod.offset_type = "OFFSET"
    mod.use_clamp_overlap = True
    mod.miter_outer = "MITER_ARC"
    return _apply(ob, mod)


def s_noise(ob, amount=0.01, scale=0.4, seed=0.0, only=None):
    """Push the points about a little (a cast face is never dead flat; a blown one is far from it)."""
    for v in ob.data.vertices:
        if only is not None and not only(v.co):
            continue
        p = Vector(v.co)
        v.co = p + Vector((fbm(p + Vector((seed, 0, 0)), scale), fbm(p + Vector((0, seed + 7.3, 0)), scale), fbm(p + Vector((0, 0, seed + 3.1)), scale))) * amount
    ob.data.update()
    return ob


def s_cut_fine(ob, size):
    """Cut the faces small enough that noise or a blast can bend them."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    for _ in range(6):
        long = [e for e in bm.edges if e.calc_length() > size]
        if not long:
            break
        bmesh.ops.subdivide_edges(bm, edges=long, cuts=1)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def take(m, ob, paint, smooth=True, part=None, keep=False):
    """A solid's skin into the model (as triangles), painted by a name or by f(middle, facing) -> name."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.verts.ensure_lookup_table()
    verts = [Vector(v.co) for v in bm.verts]
    faces = [tuple(v.index for v in f.verts) for f in bm.faces]
    bm.free()
    m.add(verts, faces, paint, smooth, None, part)
    if not keep:
        me = ob.data
        bpy.data.objects.remove(ob)
        bpy.data.meshes.remove(me)


def cast(m, ob, paint="concrete", chamfer=0.035, part=None, inside=None, rough=0.0, seed=0.0):
    """Finish a solid as cast concrete and take it into the model. `inside(middle)` says a face is inside the work
    (dark, unweathered)."""
    if chamfer:
        s_bevel(ob, chamfer, 2)
    if rough:
        s_noise(ob, rough, 0.5, seed)
    if inside is None:
        take(m, ob, paint, True, part)
    else:
        take(m, ob, lambda mid, n: "concrete.in" if inside(mid) else paint, True, part)


# ---- sandbags ---------------------------------------------------------------------------------------------

BAG = (0.56, 0.28, 0.115)          # a filled bag as it lies: long, wide, high


def sandbag(m, at, yaw=0.0, size=BAG, seed=0, paint=None, droop=0.032, pitch=0.0, roll=0.0, tie=True):
    """One filled sandbag lying flat: square-shouldered, sagging at its ends, the neck tied off at one end."""
    g = rnd(seed)
    L, W, H = size[0] * g.uniform(0.94, 1.05), size[1] * g.uniform(0.95, 1.05), size[2] * g.uniform(0.92, 1.08)
    paint = paint or g.choice(["sandbag", "sandbag", "sandbag.2", "sandbag.2", "sandbag.3"])
    nu, nv = (9, 12) if m.fine else (4, 6)
    rot = Matrix.Rotation(math.radians(yaw + g.uniform(-3, 3)), 3, "Z") @ Matrix.Rotation(math.radians(-pitch), 3, "Y") @ Matrix.Rotation(math.radians(roll + g.uniform(-2, 2)), 3, "X")
    at = Vector(at)
    lump = [g.uniform(-1, 1) for _ in range(6)]
    rings = []
    for i in range(nu):
        u = math.sin((-1.0 + 2.0 * i / (nu - 1)) * math.pi / 2) if nu > 4 else (-0.94 + 1.88 * i / (nu - 1))   # (more rings at the ends, where it turns)
        # how full the bag is along its length: square shoulders, ends drawn in to a seam
        # (beaten flat into its course: a slab with rounded shoulders, the ends drawn in only a little)
        full = (1.0 - abs(u) ** 7.0) ** 0.33 if abs(u) < 1.0 else 0.0
        full = max(full, 0.42)
        wide = W / 2 * (0.74 + 0.26 * full) * (1.0 + 0.04 * lump[0] * math.sin(2.2 * u + lump[1] * 3))
        high = H * (0.52 + 0.48 * full) * (1.0 + 0.05 * lump[2] * math.cos(1.7 * u + lump[3] * 3))
        sag = -droop * (abs(u) ** 2.2) + 0.006 * lump[4] * math.sin(3.0 * u)
        ring = []
        for k in range(nv):
            a = 2 * math.pi * k / nv
            c, s = math.cos(a), math.sin(a)
            # a squarish section, flat underneath, bulging on top
            e = 0.4
            y = wide * math.copysign(abs(c) ** e, c)
            zz = math.copysign(abs(s) ** e, s)
            # (pressed flat on top by the bag above, bedded flat beneath, the fill slumped to one side)
            z = (H / 2 + sag) + (high / 2) * zz * (0.86 if zz > 0 else 0.92) + (H / 2 - high / 2) * (-0.6) + 0.012 * lump[5] * c * full
            ring.append(at + rot @ Vector((u * L / 2, y, z)))
        rings.append(ring)
    m.skin(rings, paint, (True, True), True)
    if tie and m.fine:
        # the tied neck: a little twist of sacking standing off one end
        end = at + rot @ Vector((L / 2 * 0.98, 0.0, H * 0.5 - droop))
        d = rot @ Vector((1.0, g.uniform(-0.5, 0.5), g.uniform(0.1, 0.6))).normalized()
        m.lathe(end, d, [(0.0, 0.03), (0.035, 0.016), (0.05, 0.03), (0.085, 0.036)], paint, 6, (False, True))
    return L


def bag_row(m, pts, z, seed=0, size=BAG, half_step=False, lean=0.0, skip=None, paint=None):
    """A course of bags laid end to end along a line of points (a polyline on the ground), at height z."""
    g = rnd(seed)
    segs, total = [], 0.0
    for a, b in zip(pts, pts[1:]):
        a, b = Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0))
        segs.append((a, b, total, (b - a).length))
        total += (b - a).length
    n = max(1, int(round(total / (size[0] * 0.93))))
    step = total / n
    for i in range(n + (1 if half_step else 0)):
        s = (i + (0.0 if half_step else 0.5)) * step
        if half_step and (i == 0 or i == n):
            # (a half bag at each end of the odd courses would stick out: lay a short one inside the line)
            s = min(max(s, step * 0.28), total - step * 0.28)
            sz = (size[0] * 0.52, size[1], size[2])
        else:
            sz = (step / 0.93 * 0.96, size[1], size[2])
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
        sandbag(m, (p.x, p.y, z + g.uniform(-0.004, 0.004)), math.degrees(math.atan2(d.y, d.x)), sz, g.randrange(1 << 30), paint)


def arc(cx, cy, r, a0, a1, n=None):
    """Points on an arc, degrees, anticlockwise from +x."""
    n = n or max(2, int(abs(a1 - a0) / 6.0) + 1)
    return [(cx + math.cos(math.radians(a0 + (a1 - a0) * k / (n - 1))) * r, cy + math.sin(math.radians(a0 + (a1 - a0) * k / (n - 1))) * r) for k in range(n)]


def bag_wall(m, pts, courses, z0=0.0, seed=0, batter=0.012, thick=1, size=BAG, top=None, inward=1.0, closed=False):
    """A wall of sandbags along a line of points: courses breaking joint, leaning in a little as it rises. `thick` 2
    lays a second row behind; `top(f)` gives the number of courses at f along the wall (0..1), for a notch to fire through.
    `inward` is +1 if the wall's own side (where the second row and the lean go) is to the left of the line."""
    g = rnd(seed)
    for c in range(courses):
        z = z0 + c * size[2] * 0.88
        skip = (lambda f, zz, c=c: c >= top(f)) if top else None
        for t in range(thick):
            lean = inward * (c * batter + t * size[1] * 0.92)
            bag_row(m, pts, z, g.randrange(1 << 30), size, half_step=(c + t) % 2 == 1, lean=lean, skip=skip)
    return z0 + courses * size[2] * 0.88


# ---- earth ------------------------------------------------------------------------------------------------

def bank(m, pts, profile, paint="earth", seed=0, lump=0.05, closed=False, step=0.35, paint_fn=None, ends=True, turf=None):
    """Thrown-up earth along a line of points: `profile` is (offset to the line's left, height) pairs across it.
    Lumpy, never a smooth extrusion."""
    path = []
    pts = [Vector((p[0], p[1], 0)) for p in pts]
    if closed:
        pts = pts + [pts[0]]
    for a, b in zip(pts, pts[1:]):
        n = max(1, int((b - a).length / step))
        for k in range(n):
            path.append(a.lerp(b, k / n))
    if not closed:
        path.append(pts[-1])
    rows = []
    n = len(path)
    for i, p in enumerate(path):
        if closed:
            d = path[(i + 1) % n] - path[i - 1]
        else:
            d = path[min(i + 1, n - 1)] - path[max(i - 1, 0)]
        d.normalize()
        side = Vector((-d.y, d.x, 0))
        fall = 1.0
        if ends and not closed:
            e = min(i, n - 1 - i) * step
            fall = rigid.smooth(e / 0.8)
        row = []
        mid_off = 0.5 * (profile[0][0] + profile[-1][0])
        wide = abs(profile[-1][0] - profile[0][0])
        for j, (off, h) in enumerate(profile):
            edge = j in (0, len(profile) - 1)
            toe = edge and h <= 0.02
            if toe:
                # (a toe is never a ruled line: it runs out further here, less there, and ends just under the ground)
                q0 = p + side * off
                out = 1.0 if off > mid_off else -1.0
                off = off + out * min(0.6, 0.3 * wide) * (0.35 + 0.9 * fbm((q0.x * 0.45 + seed * 1.7, q0.y * 0.45, 2.0), 1.0, 2) + 0.3 * fbm((q0.x * 1.9, q0.y * 1.9, seed + 4.0), 1.0, 2))
            q = p + side * off
            w = 0.0 if toe else lump
            dz = fbm((q.x * 1.0 + seed, q.y * 1.0, h * 2.0), 0.55) * w * 1.6
            dxy = fbm((q.x + 31.0 + seed, q.y - 17.0, 0.3), 0.7) * w
            row.append(Vector((q.x + side.x * dxy, q.y + side.y * dxy, -0.03 if toe else max(h * fall + (dz if h > 0.02 else 0.0), -9.0 if h < 0 else 0.004))))
        rows.append(row)
    if closed:
        rows.append(rows[0])
    if turf is not None:
        k = len(rows[0])
        turfed(m, [q for row in rows for q in row], [(r * k + j, r * k + j + 1, (r + 1) * k + j + 1, (r + 1) * k + j) for r in range(len(rows) - 1) for j in range(k - 1)],
               turf, paint, seed=seed)
        return
    m.sheet(rows, paint_fn or paint, Z, True)


def turfed(m, verts, faces, amount=0.5, base="earth", over="turf", seed=0.0, part=None, low=0.05, scale=1.5, smooth=True, grass=5.0):
    """Earth grown over with turf in soft-edged patches, as thrown earth is after a season: every POINT is earth or turf by
    a broad noise (more turf with `amount`; none below `low` metres, where boots keep it bare), and the paint runs
    smoothly from point to point, so there is never a hard-edged square of green. Faces are turned to look upward.
    Wants points no more than about 0.3 m apart where the patches' edges should look natural."""
    vs = [Vector(v) for v in verts]
    coat = []
    for q in vs:
        n = 0.5 + 0.5 * fbm((q.x / scale + seed * 3.1, q.y / scale - seed, 0.37), 1.0, 3)
        fine = 0.12 * fbm((q.x * 2.3, q.y * 2.3, 5.0 + seed), 1.0, 2)
        # (turf where the noise says, and along the toe, where the field's grass has grown up onto the earth)
        coat.append(over if ((q.z > low and n + fine < amount) or (q.z < 0.11 and n + 3 * fine < 0.8)) else None)
    fs = []
    for f in faces:
        a, b, c = vs[f[0]], vs[f[1]], vs[f[2]]
        fs.append(tuple(f) if (b - a).cross(c - a).z >= 0.0 else tuple(reversed(f)))
    name = part if part is not None else m.parts[m.cur].name
    m.raw(vs, fs, [base] * len(fs), name, coat=coat, by_point=[True] * len(fs), smooth=smooth)
    if grass > 0.0 and m.fine and over == "turf":
        # (the field's grass grows on the turf: tufts by the square metre, where every corner of a face is turfed)
        g = rnd(int(seed * 7) + 11)
        pts = []
        for f in fs:
            if all(coat[i] is not None for i in f):
                a, b, c = vs[f[0]], vs[f[1]], vs[f[2]]
                area = (b - a).cross(c - a).length * (0.5 if len(f) == 3 else 1.0)
                k = int(area * grass + g.random())
                for _ in range(k):
                    u, v = g.random(), g.random()
                    if len(f) == 4:
                        pts.append((a.lerp(b, u)).lerp(vs[f[3]].lerp(vs[f[2]], u), v))
                    else:
                        if u + v > 1.0:
                            u, v = 1.0 - u, 1.0 - v
                        pts.append(a + (b - a) * u + (c - a) * v)
        if pts:
            tufts(m, pts, seed=int(seed * 3) + 5, part=name, long=0.12)


def patch(m, cx, cy, r, paint="earth", seed=0, n=28, lift=0.012, ragged=0.22, oval=1.0, yaw=0.0):
    """Trodden or dug-over ground round a work. It is no longer a disc of earth laid on the grass (that read as a
    display pad): it only records a circle of wear in m.info["worn"], which the stage (and the game, from the file's
    `ground.worn`) wears into the ground itself with a ragged, fading edge."""
    ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    worn = m.info.setdefault("worn", [])
    if abs(oval - 1.0) < 0.2:
        worn.append((cx, cy, r * 1.05, 0.9))
        return
    # a long patch: a chain of circles along its length
    long_, wide = (r * oval, r) if oval > 1.0 else (r, r * oval)
    along = (ca, sa) if oval > 1.0 else (-sa, ca)
    k = max(2, int(2 * long_ / max(wide, 0.3)))
    for i in range(k + 1):
        t = -1.0 + 2.0 * i / k
        worn.append((cx + along[0] * t * (long_ - wide), cy + along[1] * t * (long_ - wide), wide * 1.05, 0.9))


def tufts(m, pts, seed=0, high=(0.06, 0.2), paints=("grass", "grass", "grass.2", "grass.3"), blades=(5, 8), part=None, long=0.2):
    """Grass and weeds growing on a work (a turfed roof, between dragon's teeth): the field's own grass, each tuft a
    handful of bent blades splayed outward, one in five (`long`) a taller stand gone to straw. Paints `grass*` are for
    blades only (the pictures let light through them)."""
    g = rnd(seed)
    if not m.fine:
        pts = pts[::4]
    for p in pts:
        p = Vector(p)
        tall = g.random() < long
        paint = "grass.3" if tall and g.random() < 0.7 else paints[int(g.random() * len(paints)) % len(paints)]
        if paint.split(".")[0] != "grass":
            paint = "grass"
        for _ in range(g.randint(*blades) if m.fine else 2):
            ang = g.uniform(0, 2 * math.pi)
            h = g.uniform(*high) * (1.9 if tall else 0.6)
            out = Vector((math.cos(ang), math.sin(ang), 0))
            side = Vector((-math.sin(ang), math.cos(ang), 0)) * g.uniform(0.006, 0.01)
            base = p + Vector((g.uniform(-0.07, 0.07), g.uniform(-0.07, 0.07), -0.02))
            lean = g.uniform(0.2, 0.7) * h if tall else g.uniform(0.7, 1.5) * h
            mid = base + out * lean * 0.35 + Z * h * 0.75
            top = base + out * lean + Z * h * (g.uniform(0.75, 1.0) if tall else 1.0)
            if part is not None:
                m.add([base - side, base + side, mid + side * 0.65, mid - side * 0.65, top], [(0, 1, 2, 3), (3, 2, 4)], paint, False, None, part)
            else:
                m.add([base - side, base + side, mid + side * 0.65, mid - side * 0.65, top], [(0, 1, 2, 3), (3, 2, 4)], paint, False, None)


def mound(m, ring, high=0.3, seed=0, rings=6, amount=0.75, base="earth", over="turf", lump=0.05, part=None, grass=0.0):
    """Earth heaped inside a closed line of points (lying where its edge lies): it rises unevenly toward the middle,
    higher on one side than the other, lumpy, turfed in soft patches. `grass` tufts a square metre grow on it."""
    ring = [Vector(p) for p in ring]
    n = len(ring)
    c = sum(ring, Vector()) / n
    verts = []
    for j in range(rings):
        t = j / rings
        for p in ring:
            q = p.lerp(c, t)
            rise = 1.0 - (1.0 - t) ** 2.2
            uneven = 0.7 + 0.55 * fbm((q.x * 0.6 + seed, q.y * 0.6, 1.3), 1.0, 2)
            verts.append(Vector((q.x, q.y, p.z + (c.z - p.z) * t + high * rise * uneven + lump * fbm((q.x * 2.6, q.y * 2.6, seed + 7.0), 1.0, 2) * min(1.0, t * 4.0))))
    top = Vector((c.x, c.y, c.z + high * (0.7 + 0.55 * fbm((c.x * 0.6 + seed, c.y * 0.6, 1.3), 1.0, 2))))
    verts.append(top)
    faces = [(j * n + k, j * n + (k + 1) % n, (j + 1) * n + (k + 1) % n, (j + 1) * n + k) for j in range(rings - 1) for k in range(n)]
    faces += [((rings - 1) * n + k, (rings - 1) * n + (k + 1) % n, rings * n) for k in range(n)]
    turfed(m, verts, faces, amount, base, over, seed=seed, part=part, low=-9.0, grass=0.0)
    if grass > 0.0 and m.fine:
        g = rnd(seed + 3)
        area = abs(sum(ring[k].x * ring[(k + 1) % n].y - ring[(k + 1) % n].x * ring[k].y for k in range(n))) * 0.5
        pts = []
        for _ in range(int(area * grass)):
            j, k, u = g.randrange(rings - 1), g.randrange(n), g.random()
            a, b = verts[j * n + k].lerp(verts[j * n + (k + 1) % n], u), verts[(j + 1) * n + k].lerp(verts[(j + 1) * n + (k + 1) % n], u)
            pts.append(a.lerp(b, g.random()) + Z * 0.01)
        tufts(m, pts, seed=seed + 5, part=part)
    return verts


# ---- timber -----------------------------------------------------------------------------------------------

def log(m, a, b, r, paint="timber", seed=0, sides=None, taper=0.9, ends="timber.end"):
    """A round log from a to b: a little taper, a little wander, the sawn ends pale."""
    a, b = Vector(a), Vector(b)
    sides = sides or (12 if m.fine else 6)
    L = (b - a).length
    n = 5 if m.fine else 2
    axis = (b - a) / L
    side = axis.cross(Z)
    if side.length < 1e-6:
        side = axis.cross(X)
    side.normalize()
    up = side.cross(axis)
    g = rnd(seed)
    ph = g.uniform(0, 6.28)
    rings = []
    for i in range(n):
        f = i / (n - 1)
        rr = r * (1.0 - (1.0 - taper) * f) * (1.0 + 0.04 * math.sin(7 * f + ph))
        mid = a + axis * (L * f) + side * (0.012 * math.sin(3.1 * f + ph)) * (L > 1.0)
        rings.append([mid + (side * math.cos(2 * math.pi * k / sides) + up * math.sin(2 * math.pi * k / sides)) * rr * (1.0 + 0.05 * math.sin(3 * k + ph)) for k in range(sides)])
    m.skin(rings, paint, (True, True), True, cap_paint=ends)


def plank(m, a, b, wide, thick, paint="planks", up=Z, seed=0):
    """A sawn plank from a to b (its middle line), `wide` across and `thick` through, lying with `up` its face."""
    a, b = Vector(a), Vector(b)
    d = (b - a)
    L = d.length
    d.normalize()
    up = Vector(up)
    side = d.cross(up)
    if side.length < 1e-6:
        side = d.cross(X)
    side.normalize()
    face = side.cross(d).normalized()
    g = rnd(seed)
    w0, w1 = wide / 2 * g.uniform(0.96, 1.0), wide / 2 * g.uniform(0.96, 1.0)
    vs = [a + side * sy * (w0) + face * sz * thick / 2 for sy in (-1, 1) for sz in (-1, 1)] + [b + side * sy * (w1) + face * sz * thick / 2 for sy in (-1, 1) for sz in (-1, 1)]
    mid = (a + b) / 2
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    m.add(vs, faces, paint, False, lambda p: p - mid)


def beam(m, a, b, wide, high, paint="timber", yaw_up=Z):
    plank(m, a, b, wide, high, paint, yaw_up)


# ---- steel ------------------------------------------------------------------------------------------------

def tube(m, pts, r, paint, sides=None, closed=False, caps=True, smooth=True, radii=None):
    """A round bar or pipe bent through a row of points."""
    pts = [Vector(p) for p in pts]
    sides = sides or (8 if m.fine else 4)
    n = len(pts)
    rings = []
    prev_side = None
    for i, p in enumerate(pts):
        if closed:
            d = pts[(i + 1) % n] - pts[i - 1]
        else:
            d = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
        if d.length < 1e-9:
            d = X.copy()
        d.normalize()
        if prev_side is None:
            side = d.cross(Z)
            if side.length < 1e-4:
                side = d.cross(X)
        else:
            side = prev_side - d * prev_side.dot(d)
            if side.length < 1e-6:
                side = d.cross(Z)
        side.normalize()
        prev_side = side
        up = d.cross(side)
        rr = radii[i] if radii else r
        rings.append([p + (side * math.cos(2 * math.pi * k / sides) + up * math.sin(2 * math.pi * k / sides)) * rr for k in range(sides)])
    if closed:
        rings.append(rings[0])
    m.skin(rings, paint, (caps and not closed, caps and not closed), smooth)


def bolt(m, at, facing, r=0.016, h=0.012, paint="steel", sides=6):
    """A bolt head or a rivet standing on a plate."""
    at, facing = Vector(at), Vector(facing).normalized()
    if not m.fine:
        return
    m.lathe(at, facing, [(0.0, r), (h * 0.7, r), (h, r * 0.6)], paint, sides, (False, True), False)


def bolts(m, a, b, n, facing, r=0.016, h=0.012, paint="steel", ends=True):
    a, b = Vector(a), Vector(b)
    for k in range(n):
        t = (k + (0.0 if ends else 0.5)) / ((n - 1) if ends and n > 1 else n)
        bolt(m, a.lerp(b, t), facing, r, h, paint)


def bolt_ring(m, centre, facing, radius, n, r=0.016, h=0.012, paint="steel", phase=0.0):
    centre, facing = Vector(centre), Vector(facing).normalized()
    side = facing.cross(Z)
    if side.length < 1e-6:
        side = facing.cross(X)
    side.normalize()
    up = side.cross(facing)
    for k in range(n):
        a = 2 * math.pi * (k + phase) / n
        bolt(m, centre + (side * math.cos(a) + up * math.sin(a)) * radius, facing, r, h, paint)


def plate(m, outline, thick, paint, at, u=X, v=Z, centred=True):
    """A flat plate cut to an outline: (a, b) pairs measured from `at` along `u` and along `v` (so u = X, v = Z is a
    plate standing fore and aft). `thick` through, half each side of `at` (or all behind its front face, the side
    u x v looks to, when not `centred`)."""
    at, u, v = Vector(at), Vector(u).normalized(), Vector(v).normalized()
    n = u.cross(v).normalized()
    k = len(outline)
    off = n * (thick / 2 if centred else 0.0)
    front = [at + u * p[0] + v * p[1] + off for p in outline]
    back = [p - n * thick for p in front]
    c = sum(front, Vector()) / k
    m.add(front + [c], [(i, (i + 1) % k, k) for i in range(k)], paint, False, lambda p: n)
    m.add(back + [c - n * thick], [(i, (i + 1) % k, k) for i in range(k)], paint, False, lambda p: -n)
    mid = c - n * thick / 2
    m.add(front + back, [(i, (i + 1) % k, k + (i + 1) % k, k + i) for i in range(k)], paint, False, lambda p: p - mid)


def angle_iron(m, a, b, leg, thick, paint, open_to):
    """A length of angle iron (an L in section) from a to b; `open_to` is the way the L opens (between its legs)."""
    a, b = Vector(a), Vector(b)
    d = (b - a).normalized()
    o = Vector(open_to) - d * Vector(open_to).dot(d)
    o.normalize()
    p = d.cross(o)
    l1, l2 = (o + p).normalized(), (o - p).normalized()          # the two legs, 90 degrees apart
    sec = [Vector(), l1 * leg, l1 * leg + l2 * thick, (l1 + l2) * thick, l2 * leg + l1 * thick, l2 * leg]
    k = len(sec)
    lo = [a + s for s in sec]
    hi = [b + s for s in sec]
    mid = (a + b) / 2 + (l1 + l2) * leg * 0.3
    m.add(lo + hi, [(i, (i + 1) % k, k + (i + 1) % k, k + i) for i in range(k)], paint, False, None)
    # (the ends: an L is not convex, so two quads each)
    for ring, way in ((lo, -d), (hi, d)):
        m.add(ring, [(0, 1, 2, 3), (0, 3, 4, 5)], paint, False, lambda q, w=way: w)
    return l1, l2


def handwheel(m, centre, axis, radius, paint="steel", spokes=4, rim=0.011, knob=True):
    centre, axis = Vector(centre), Vector(axis).normalized()
    side = axis.cross(Z)
    if side.length < 1e-6:
        side = axis.cross(X)
    side.normalize()
    up = side.cross(axis)
    n = 20 if m.fine else 8
    tube(m, [centre + (side * math.cos(2 * math.pi * k / n) + up * math.sin(2 * math.pi * k / n)) * radius for k in range(n)], rim, paint, 6 if m.fine else 3, closed=True)
    for s in range(spokes):
        a = 2 * math.pi * s / spokes
        m.rod(centre, centre + (side * math.cos(a) + up * math.sin(a)) * radius, rim * 0.75, paint, sides=5)
    m.lathe(centre - axis * 0.02, axis, [(0.0, radius * 0.2), (0.04, radius * 0.2)], paint, 8)
    if knob:
        k = centre + side * radius
        m.rod(k, k + axis * 0.07, rim * 1.3, "timber.dark", sides=6)


# ---- wire -------------------------------------------------------------------------------------------------

def barbs(m, pts, every, size=0.03, paint="wire", seed=0):
    """Barbs along a wire: each a tiny pair of crossed points."""
    if not m.fine:
        return
    g = rnd(seed)
    run = 0.0
    nxt = g.uniform(0, every)
    for a, b in zip(pts, pts[1:]):
        a, b = Vector(a), Vector(b)
        ln = (b - a).length
        while nxt <= run + ln:
            p = a.lerp(b, (nxt - run) / max(ln, 1e-9))
            d = (b - a).normalized()
            s = d.cross(Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-1, 1))))
            if s.length < 1e-3:
                s = d.cross(Z)
            s.normalize()
            t = d.cross(s)
            for u in (s, t):
                m.add([p - u * size, p + d * size * 0.22, p + u * size, p - d * size * 0.22], [(0, 1, 2, 3)], paint, False, None)
            nxt += every * g.uniform(0.8, 1.2)
        run += ln


# ---- crews ------------------------------------------------------------------------------------------------

def crew(m, role, at, facing=(1, 0, 0), stance="stand", part=None, note="", seat_h=None, grips=None):
    """Where a man of the crew is: socket `crew.<role>` on the part he rides, with a `stance` the soldier modeller can
    pose to (stand, kneel, sit or prone), the seat's height where he sits, and `grip.<role>.l/r` where his hands go."""
    m.socket("crew." + role, at, facing, part=part, note=note)
    keys = {"stance": stance}
    if seat_h is not None:
        keys["seat_h"] = round(seat_h, 3)
    m.info.setdefault("socket_keys", {})["crew." + role] = keys
    for hand, g in (grips or {}).items():
        m.socket("grip." + role + "." + hand, g[0], g[1] if len(g) > 2 or isinstance(g[1], (tuple, list, Vector)) else (1, 0, 0), part=g[2] if len(g) > 2 else part,
                 note=("left" if hand == "l" else "right") + " hand of the " + role)
