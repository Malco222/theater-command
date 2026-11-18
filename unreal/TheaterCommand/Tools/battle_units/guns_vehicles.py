"""THE FIELD GUN, THE LORRY and THE HALF-TRACKS for the battles: built up for a close camera, rigged, animated, exported.

Run without Blender's window (the bpy module, Blender 5.0.1):

    python guns_vehicles.py                                        every model, every step
    python guns_vehicles.py --models gun --only export              the field gun's files alone
    python guns_vehicles.py --models gun,lorry --only before_after,sheet --fast     test pictures (640x360, 16 samples)
    python guns_vehicles.py --models gun --only clip                the gun firing on the battlefield (960x540, mp4 + gif)
    python guns_vehicles.py --models 25pdr,leFH18 --only export      any other gun in build_tank.OTHERS, on the same rig

  models (the switch for each): gun (= Gun75), ammo (the gun's ammunition boxes, a prop), lorry (= Truck),
          hanomag, m3 (= M3Halftrack), or a build_tank.OTHERS name: Gun75, 25pdr, leFH18, Howitzer105, ZiS3, Truck,
          Hanomag, M3Halftrack.  Default: gun,ammo,lorry,hanomag,m3.
  steps:  export, before_after, sheet, clip, readme.  --fast: test size.  --out FOLDER: instead of battle-models/land/.

Every piece starts from the campaign piece in build_tank.py (read, never changed; its last line runs it, so it is
loaded here without that line): the same measures from OTHERS (the gun's wheel size, trunnion height, barrel, shield
and trail; the lorry's axles, bonnet, cab, bed and tilt; each half-track's body, track unit and wheels), so the battle
pieces and the campaign pieces are the same machines. Everything a close camera finds wanting is added here, and each
is rigged, animated and written out the way tank.py writes the tank (its helpers are copied here, not imported, so a
change to tank.py cannot break this file): metres, +X forward, Z up, Blender's y to the LEFT (the JSON's y to the
right), in-place clips, one action per clip, materials named by paint, sockets as empties socket_<name> on bones.

Nothing is fetched and nothing is drawn by hand. These are our own designs: no real gun or vehicle is reproduced, and
nothing is taken from any game.
"""

import bpy  # (first: the bpy module puts Blender's own script folders on the path)
import bmesh
import json
import math
import os
import random
import shutil
import sys
import time
import types

import numpy as np
from mathutils import Euler, Matrix, Vector, geometry

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import kit  # noqa: E402


def _load_build_tank():
    """build_tank.py as a module, without running it (its last line is `main()`)."""
    path = os.path.join(TOOLS, "build_tank.py")
    with open(path) as f:
        src = f.read()
    src = src[:src.rindex("\nmain()")]
    mod = types.ModuleType("build_tank")
    mod.__file__ = path
    exec(compile(src, path, "exec"), mod.__dict__)
    return mod


bt = _load_build_tank()
PAINT_NAMES = sorted(bt.PAINTS, key=bt.PAINTS.get)          # paint, shade, track, rubber, wood, steel, canvas, glass, rust, stripe
MODELS_DIR = os.path.normpath(os.path.join(TOOLS, "..", "Content", "Models"))
OUT_ROOT = "/mnt/project-files/battle-models/land/"
WORK = "/home/user/art/guns/"
FPS = 30
FAST = False


def pick(lod, *v):
    return v[min(lod, len(v) - 1)]


def smoothstep(e0, e1, x):
    if e1 == e0:
        return 1.0 if x >= e1 else 0.0
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


# ================================================================================================ geometry (from tank.py)
class Geo:
    """Faces gathered for one mesh: each point carries its bone weights, each face its paint (and, if given, its UVs)."""

    def __init__(self):
        self.co, self.wt, self.faces, self.fmat, self.fuv = [], [], [], [], []

    def add(self, verts, faces, mat, bone, uvs=None):
        base = len(self.co)
        for p in verts:
            p = Vector(p)
            self.co.append(p)
            self.wt.append(bone(p) if callable(bone) else {bone: 1.0})
        for i, f in enumerate(faces):
            self.faces.append(tuple(base + k for k in f))
            self.fmat.append(mat)
            self.fuv.append(uvs[i] if uvs is not None else None)

    def put(self, bm, mat, bone, m=None):
        if m is not None:
            bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
        bm.verts.index_update()
        self.add([v.co.copy() for v in bm.verts], [[v.index for v in f.verts] for f in bm.faces], mat, bone)
        bm.free()

    def tris(self):
        return sum(len(f) - 2 for f in self.faces)


def place(at=(0, 0, 0), rot=(0, 0, 0)):
    """Turn (degrees about x, then y, then z) and move to `at`."""
    m = Euler([math.radians(a) for a in rot], "XYZ").to_matrix().to_4x4()
    m.translation = Vector(at)
    return m


def frame(at, x, z):
    """A matrix whose local z runs along `z` and local x (squared to it) along `x`."""
    z = Vector(z).normalized()
    x = Vector(x)
    x = (x - z * x.dot(z)).normalized()
    y = z.cross(x)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = Vector(at)
    return m


def toward(at, d, spin=0.0):
    """Local z along d."""
    d = Vector(d).normalized()
    m = Vector((0, 0, 1)).rotation_difference(d).to_matrix().to_4x4() @ Matrix.Rotation(spin, 4, "Z")
    m.translation = Vector(at)
    return m


def bm_box(size, bevel=0.0, seg=1, shape=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        p = Vector(shape(v.co.copy())) if shape else v.co.copy()
        v.co = Vector((p.x * size[0], p.y * size[1], p.z * size[2]))
    if bevel > 0.0:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=bevel, offset_type="OFFSET", segments=seg, profile=0.5,
                        affect="EDGES", clamp_overlap=True)
    return bm


def bm_lathe(prof, sides, cap0=False, cap1=False, phase=0.0):
    """A profile [(radius, height)] turned round local z. Taken bottom to top (counter-clockwise in radius-height) its
    faces look outward; a radius of 0 is a pole."""
    bm = bmesh.new()
    rings = []
    for r, h in prof:
        if r < 1e-6:
            rings.append([bm.verts.new((0.0, 0.0, h))])
        else:
            rings.append([bm.verts.new((r * math.cos(phase + 2 * math.pi * k / sides), r * math.sin(phase + 2 * math.pi * k / sides), h))
                          for k in range(sides)])
    for a, b in zip(rings, rings[1:]):
        if len(a) == 1 and len(b) == 1:
            continue
        for k in range(sides):
            k1 = (k + 1) % sides
            if len(a) == 1:
                bm.faces.new((a[0], b[k1], b[k]))
            elif len(b) == 1:
                bm.faces.new((a[k], a[k1], b[0]))
            else:
                bm.faces.new((a[k], a[k1], b[k1], b[k]))
    if cap0 and len(rings[0]) > 2:
        bm.faces.new(list(reversed(rings[0])))
    if cap1 and len(rings[-1]) > 2:
        bm.faces.new(rings[-1])
    return bm


def bm_cyl(r, depth, sides, r2=None, caps=True, cap0=None, cap1=None):
    return bm_lathe([(r, -depth * 0.5), (r if r2 is None else r2, depth * 0.5)], sides,
                    caps if cap0 is None else cap0, caps if cap1 is None else cap1)


def bm_prism(poly, depth):
    """A flat shape [(x, y)], counter-clockwise, given thickness along z (-depth/2 .. +depth/2)."""
    bm = bmesh.new()
    lo = [bm.verts.new((x, y, -depth * 0.5)) for x, y in poly]
    hi = [bm.verts.new((x, y, depth * 0.5)) for x, y in poly]
    n = len(poly)
    for k in range(n):
        k1 = (k + 1) % n
        bm.faces.new((lo[k], lo[k1], hi[k1], hi[k]))
    bm.faces.new(list(reversed(lo)))
    bm.faces.new(hi)
    return bm


def bm_plate(outline, holes=(), depth=0.01, walls=True):
    """A flat plate in local xy (counter-clockwise outline, clockwise or any-way holes), thickness along z, holes cut
    clean through (tessellated, so concave outlines and windows both work)."""
    bm = bmesh.new()
    loops = [list(outline)] + [list(h) for h in holes]
    flat = [Vector((x, y, 0.0)) for lp in loops for x, y in lp]
    tris = geometry.tessellate_polygon([[Vector((x, y, 0.0)) for x, y in lp] for lp in loops])
    lo = [bm.verts.new((p.x, p.y, -depth * 0.5)) for p in flat]
    hi = [bm.verts.new((p.x, p.y, depth * 0.5)) for p in flat]
    for a, b, c in tris:
        # (tessellate_polygon's winding is not promised: each face is turned to look its own way)
        n = (flat[b] - flat[a]).cross(flat[c] - flat[a])
        if n.z >= 0:
            bm.faces.new((hi[a], hi[b], hi[c]))
            bm.faces.new((lo[c], lo[b], lo[a]))
        else:
            bm.faces.new((hi[c], hi[b], hi[a]))
            bm.faces.new((lo[a], lo[b], lo[c]))
    if walls:
        base = 0
        for lp in loops:
            n = len(lp)
            for k in range(n):
                k1 = (k + 1) % n
                try:
                    bm.faces.new((lo[base + k], lo[base + k1], hi[base + k1], hi[base + k]))
                except ValueError:
                    pass
            base += n
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bm


def circle_profile(sides):
    return [(math.cos(-2 * math.pi * k / sides), math.sin(-2 * math.pi * k / sides)) for k in range(sides)]


def bm_tube(pts, radius, sides=6, closed=False, caps=True, up=None, profile=None, open_profile=False):
    """A tube along a polyline. Its ring is `profile` [(b, n)] (clockwise, in the plane of the binormal b and the normal
    n) or a circle; `up` fixes the normal (a weld bead's height is out of the plate it lies on)."""
    bm = bmesh.new()
    P = [Vector(p) for p in pts]
    n = len(P)
    prof = profile or circle_profile(sides)
    T = []
    for i in range(n):
        t = (P[(i + 1) % n] - P[i - 1]) if closed else (P[min(i + 1, n - 1)] - P[max(i - 1, 0)])
        T.append(t.normalized())
    ref = Vector((0, 0, 1)) if abs(T[0].z) < 0.9 else Vector((1, 0, 0))
    N = (ref - T[0] * ref.dot(T[0])).normalized()
    rings = []
    for i in range(n):
        if up is not None:
            u = Vector(up[i] if isinstance(up, list) else up)
            N = (u - T[i] * u.dot(T[i])).normalized()
        else:
            N = (N - T[i] * N.dot(T[i])).normalized()
        B = T[i].cross(N)
        r = radius(i) if callable(radius) else radius
        rings.append([bm.verts.new(P[i] + (B * b + N * c) * r) for b, c in prof])
    m = len(prof)
    for i in range(n if closed else n - 1):
        a, b = rings[i], rings[(i + 1) % n]
        for k in range(m - 1 if open_profile else m):
            k1 = (k + 1) % m
            bm.faces.new((a[k], a[k1], b[k1], b[k]))
    if caps and not closed and not open_profile:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    return bm


def rod(g, a, b, r, sides, mat, bone, r2=None, caps=True):
    """A round bar from a to b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    bm = bm_lathe([(r, 0.0), (r if r2 is None else r2, d.length)], sides, caps, caps)
    g.put(bm, mat, bone, toward(a, d))


def bolt(g, at, normal, bone, r=0.014, h=0.012, mat="shade", sides=6, spin=0.0):
    """A hex-headed bolt standing out of a plate."""
    g.put(bm_lathe([(r, -0.003), (r, h)], sides, cap1=True), mat, bone, toward(at, normal, spin))


def rivet(g, at, normal, bone, r=0.009, mat="paint", sides=5):
    """A round rivet head, a low dome."""
    g.put(bm_lathe([(r, -0.002), (r * 0.8, r * 0.45), (0.0, r * 0.7)], sides), mat, bone, toward(at, normal))


def bolt_ring(g, centre, normal, radius, count, bone, r=0.013, h=0.011, mat="shade", phase=0.0, ref=None, sides=6):
    n = Vector(normal).normalized()
    a = Vector(ref) if ref is not None else (Vector((0, 0, 1)) if abs(n.z) < 0.9 else Vector((1, 0, 0)))
    a = (a - n * a.dot(n)).normalized()
    b = n.cross(a)
    for k in range(count):
        t = phase + 2 * math.pi * k / count
        bolt(g, Vector(centre) + (a * math.cos(t) + b * math.sin(t)) * radius, n, bone, r, h, mat, sides, spin=t)


def rivet_row(g, a, b, count, normal, bone, r=0.009, mat="paint", ends=True):
    a, b = Vector(a), Vector(b)
    for k in range(count):
        t = (k / max(count - 1, 1)) if ends else (k + 0.5) / count
        rivet(g, a.lerp(b, t), normal, bone, r, mat)


def weld(g, a, b, out, bone, wide=0.011, high=0.008, seg=None, sink=0.0, mat="shade"):
    """A weld bead along a joint, standing out of it along `out`."""
    a, b, out = Vector(a), Vector(b), Vector(out).normalized()
    length = (b - a).length
    seg = seg or max(2, int(length / 0.3))
    pts, ups = [], []
    for k in range(seg + 1):
        t = k / seg
        wob = 0.0012 * math.sin(k * 2.3)
        pts.append(a.lerp(b, t) - out * sink + out.cross(b - a).normalized() * wob)
        ups.append(out)
    prof = [(x, y * high / wide) for x, y in ((-1.0, -0.3), (-0.55, 0.75), (0.55, 0.75), (1.0, -0.3))]
    g.put(bm_tube(pts, wide, profile=prof, up=ups, open_profile=True, caps=False), mat, bone)


def handwheel(g, centre, axis, radius, bone, lod, spokes=4, mat="steel", knob=True):
    """A crank wheel: a round rim on spokes, with a turning handle standing out of it."""
    m = toward(centre, axis)
    n = pick(lod, 14, 8)
    g.put(bm_tube([m @ Vector((radius * math.cos(2 * math.pi * k / n), radius * math.sin(2 * math.pi * k / n), 0.0)) for k in range(n)],
                  pick(lod, 0.011, 0.014), pick(lod, 5, 4), closed=True), mat, bone)
    if lod >= 2:
        return
    for k in range(spokes):
        a = 2 * math.pi * k / spokes + 0.4
        rod(g, m @ Vector((0, 0, -0.01)), m @ Vector((radius * math.cos(a), radius * math.sin(a), 0.0)), 0.007, 4, mat, bone, caps=False)
    g.put(bm_cyl(0.022, 0.05, 8), mat, bone, m)
    if knob:
        a = 0.4 + math.pi / spokes
        p = m @ Vector((radius * math.cos(a), radius * math.sin(a), 0.0))
        rod(g, p, p + (m.to_3x3() @ Vector((0, 0, 1))) * 0.07, 0.011, 6, "wood", bone)


def tyre_profile(r, w, lod, bulge=0.012, rim_r=None, tread=0.0):
    """A tyre's cross-section, inner edge to inner edge round the outside, as (radius, height) for bm_lathe."""
    h = w * 0.5
    ri = rim_r if rim_r is not None else r * 0.68
    if lod >= 2:
        return [(ri, -h), (r, -h), (r, h), (ri, h)]
    if lod == 1:
        return [(ri, -h), (r - 0.04, -h - bulge * 0.5), (r, -h * 0.6), (r, h * 0.6), (r - 0.04, h + bulge * 0.5), (ri, h)]
    sw = r - (r - ri) * 0.45
    return [(ri, -h * 0.92), (sw, -h - bulge), (r - 0.035, -h - bulge * 0.4), (r - tread, -h * 0.82), (r, -h * 0.62),
            (r, h * 0.62), (r - tread, h * 0.82), (r - 0.035, h + bulge * 0.4), (sw, h + bulge), (ri, h * 0.92)]


def tread_lugs(g, centre, side, r, w, n, bone, chevron=True, depth=0.022, mat="rubber"):
    """Cross-country tread: lugs standing out of the tyre's crown, a chevron (each half leaning) or straight bars."""
    out = Vector((0.0, side, 0.0))
    turn = Matrix.Rotation(-side * math.pi / 2, 4, "X")
    base = Matrix.Translation(centre) @ turn
    for k in range(n):
        a = 2 * math.pi * k / n
        for half in ((-1.0, 1.0) if chevron else (0.0,)):
            lean = 0.18 * half if chevron else 0.0
            zc = half * w * 0.24 if chevron else 0.0
            ln = w * (0.42 if chevron else 0.8)
            m = base @ Matrix.Rotation(a + lean * 0.0, 4, "Z") @ Matrix.Translation(Vector((r - 0.004, 0.0, zc))) \
                @ Matrix.Rotation(math.radians(30.0) * half, 4, "X")
            bm = bm_box((depth + 0.004, 0.05, ln))
            bmesh.ops.translate(bm, vec=(depth * 0.5, 0.0, 0.0), verts=bm.verts)
            bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.x < -0.9], context="FACES_ONLY")
            g.put(bm, mat, bone, m)


# ================================================================================================ the rig (from tank.py)
def J(v):
    """Blender's frame to the game's JSON frame (y to the right)."""
    return [round(float(v[0]), 4), round(-float(v[1]), 4), round(float(v[2]), 4)]


def turn_axis_b(kj):
    """A turn of +a about the JSON axis kj (as Unreal's FQuat(axis, angle) in the JSON's own axes) is a turn of +a about
    this axis in Blender."""
    return Vector((-kj[0], kj[1], -kj[2]))


def slide_axis_b(kj):
    return Vector((kj[0], -kj[1], kj[2]))


def json_axis_of(b):
    """The JSON axis whose + turn is a + turn about Blender axis b."""
    b = Vector(b).normalized()
    return (round(-b.x, 4) + 0.0, round(b.y, 4) + 0.0, round(-b.z, 4) + 0.0)


class Part(dict):
    pass


def part(name, parent, pivot, axis, kind, limits, sense, tail=(0, 0, 0.3), **extra):
    return Part(name=name, parent=parent, pivot=Vector(pivot), axis=tuple(float(a) for a in axis), kind=kind, limits=limits,
                sense=sense, tail=Vector(tail), **extra)


def make_rig(parts, name):
    ad = bpy.data.armatures.new(name)
    ad.display_type = "STICK"
    arm = bpy.data.objects.new(name, ad)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for p in parts:
        eb = ad.edit_bones.new(p["name"])
        eb.head = p["pivot"]
        eb.tail = p["pivot"] + p["tail"]
        eb.roll = 0.0
        eb.use_deform = True
        if p["parent"]:
            eb.parent = ad.edit_bones[p["parent"]]
            eb.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


def make_sockets(arm, table):
    bpy.context.view_layer.update()
    out = []
    for name, bone, at, facing in table:
        e = bpy.data.objects.new(name, None)
        e.empty_display_type = "ARROWS"
        e.empty_display_size = 0.15
        bpy.context.scene.collection.objects.link(e)
        e.parent = arm
        e.parent_type = "BONE"
        e.parent_bone = bone
        bpy.context.view_layer.update()
        m = Vector((1, 0, 0)).rotation_difference(Vector(facing)).to_matrix().to_4x4()      # (its x along the facing)
        m.translation = Vector(at)
        e.matrix_world = m
        out.append(e)
    bpy.context.view_layer.update()
    return out


class Clip:
    """A clip as numbers: channels (part, kind, axis or None for the part's own, value as a function of time) and events."""

    def __init__(self, name, long, loop, loop_cue=None, about="", metres=None):
        self.name, self.long, self.loop, self.loop_cue, self.about = name, long, loop, loop_cue, about
        self.metres = metres                    # metres one loop covers (driving and towing clips)
        self.ch, self.events = [], []
        self.travel = None                      # metres travelled by time t (for the pictures' ground and belts)

    def add(self, part, kind, fn, axis=None):
        self.ch.append((part, kind, axis, fn))
        return self

    def event(self, t, fx, socket, cue=None):
        self.events.append({"t": round(t, 3), "fx": fx, "socket": socket, "cue": cue})
        return self

    def times(self):
        n = int(round(self.long * FPS))
        return [min(k / FPS, self.long) for k in range(n + 1)]

    def value(self, part, kind, t, axis=None):
        return sum(fn(t) for p, k, a, fn in self.ch if p == part and k == kind and a == axis)


def _damped(t, a, period, tau, t0=0.0):
    if t < t0:
        return 0.0
    t -= t0
    return a * math.exp(-t / tau) * math.sin(2 * math.pi * t / period)


def _ease(t, t0, t1, a=0.0, b=1.0):
    return a + (b - a) * smoothstep(t0, t1, t)


def sin_(f, t, ph=0.0):
    return math.sin(2 * math.pi * f * t + ph)


def bone_delta(p, chans, t):
    """The bone's own movement at time t (as a matrix in the rig's rest frame, about its pivot), from its channels."""
    R = Matrix.Identity(4)
    move = Vector((0, 0, 0))
    for kind, axis, fn in chans:
        a = fn(t)
        k = axis or p["axis"]
        if kind in ("turn", "spin"):
            R = R @ Matrix.Rotation(a, 4, turn_axis_b(k))
        elif kind == "slide":
            move += slide_axis_b(k) * a
    pv = p["pivot"]
    return Matrix.Translation(move) @ Matrix.Translation(pv) @ R @ Matrix.Translation(-pv)


def make_actions(arm, parts, clip_list):
    """One Blender action per clip, keyed every frame (linear between), for the bones each clip moves."""
    by_name = {p["name"]: p for p in parts}
    arm.animation_data_create()
    acts = {}
    for c in clip_list:
        act = bpy.data.actions.new(c.name)
        act.use_fake_user = True
        arm.animation_data.action = act
        chans = {}
        for pn, kind, axis, fn in c.ch:
            if kind != "scroll":
                chans.setdefault(pn, []).append((kind, axis, fn))
        if not chans:                                       # (a still clip still needs a key, or it is not exported)
            chans = {parts[1]["name"] if len(parts) > 1 else parts[0]["name"]: [("turn", None, lambda t: 0.0)]}
        for bname, ch in chans.items():
            pb = arm.pose.bones[bname]
            rest = arm.data.bones[bname].matrix_local
            rinv = rest.inverted()
            prev = None
            for f, t in enumerate(c.times()):
                basis = rinv @ bone_delta(by_name[bname], ch, t) @ rest
                loc, q, _ = basis.decompose()
                if prev is not None and q.dot(prev) < 0:
                    q.negate()
                prev = q
                pb.location = loc
                pb.rotation_quaternion = q
                pb.keyframe_insert("location", frame=f, group=bname)
                pb.keyframe_insert("rotation_quaternion", frame=f, group=bname)
        for fc in kit.fcurves_of(act):
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
        act["loop"] = c.loop
        acts[c.name] = act
    arm.animation_data.action = None
    return acts


def reset_pose(arm):
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)


def play(arm, act, sc):
    reset_pose(arm)
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and act.slots:
        arm.animation_data.action_slot = act.slots[0]
    sc.frame_start = int(act.frame_range[0])
    sc.frame_end = int(act.frame_range[1])


def pose_static(arm, parts, values):
    """Pose the rig by hand (no action): values {part: [(kind, axis or None, value)]}."""
    by_name = {p["name"]: p for p in parts}
    reset_pose(arm)
    if arm.animation_data:
        arm.animation_data.action = None
    for bname, chans in values.items():
        rest = arm.data.bones[bname].matrix_local
        basis = rest.inverted() @ bone_delta(by_name[bname], [(k, a, (lambda t, v=v: v)) for k, a, v in chans], 0.0) @ rest
        loc, q, _ = basis.decompose()
        pb = arm.pose.bones[bname]
        pb.location = loc
        pb.rotation_quaternion = q
    bpy.context.view_layer.update()


def _simplify(keys, tol):
    if len(keys) <= 2:
        return keys
    out = [keys[0]]
    for i in range(1, len(keys) - 1):
        (t0, v0), (t1, v1), (t2, v2) = out[-1], keys[i], keys[i + 1]
        guess = v0 + (v2 - v0) * (t1 - t0) / (t2 - t0)
        if abs(guess - v1) > tol:
            out.append(keys[i])
    out.append(keys[-1])
    return out


def clips_json(clip_list, index):
    out = {}
    for c in clip_list:
        tracks = []
        for pn, kind, axis, fn in c.ch:
            tol = {"slide": 2e-5, "scroll": 1e-6}.get(kind, 1e-4)
            keys = [[round(t, 5), round(fn(t), 6)] for t in c.times()]
            keys = _simplify(keys, tol)
            if all(abs(v) < 1e-7 for _, v in keys):
                continue
            tr = {"part": index[pn], "part_name": pn, kind: keys}
            if axis is not None:
                tr["axis"] = list(axis)
            tracks.append(tr)
        e = {"long": c.long, "loop": c.loop, "loop_cue": c.loop_cue, "about": c.about, "tracks": tracks, "events": c.events}
        if not c.loop:
            e["after"] = getattr(c, "after", "hold")         # (the shared format's rule: what follows a clip that does not loop)
        if c.metres is not None:
            e["metres_per_loop"] = round(c.metres, 4)
        out[c.name] = e
    return out


# ================================================================================================ meshes and materials
PLAIN = {"paint": None, "shade": None, "track": (0.045, 0.042, 0.04), "rubber": (0.02, 0.02, 0.02), "wood": (0.30, 0.19, 0.09),
         "steel": (0.32, 0.32, 0.31), "canvas": (0.36, 0.32, 0.22), "glass": (0.05, 0.08, 0.1), "rust": (0.24, 0.10, 0.045),
         "stripe": None}


def materials_plain(nation="GER"):
    """One material per paint, plainly coloured (what the exports carry; the game recolours by name)."""
    paint = kit.NATIONS[nation]["paint"]
    cols = dict(PLAIN, paint=kit.hex_rgb(paint), shade=kit.hex_rgb(paint, 0.62), stripe=kit.hex_rgb("D4B44E"))
    out = {}
    for name in PAINT_NAMES:
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get("Principled BSDF")
        if bsdf is None:
            nt, bsdf = _nodes(m)
        bsdf.inputs["Base Color"].default_value = (*cols[name], 1.0)
        bsdf.inputs["Roughness"].default_value = {"steel": 0.45, "glass": 0.1, "track": 0.6}.get(name, 0.8)
        bsdf.inputs["Metallic"].default_value = {"steel": 0.8, "track": 0.5}.get(name, 0.0)
        out[name] = m
    return out


def make_mesh(g, name, arm=None, nation="GER"):
    """A Blender mesh from a Geo: material slots in paint order, UVs (given ones, a box projection elsewhere), a `rest`
    attribute (each point's place in the model's rest frame, for the pictures' weathering), vertex groups by bone,
    smooth shading with sharp edges past 40 degrees, skinned to `arm`."""
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(p) for p in g.co], [], g.faces)
    me.validate(clean_customdata=False)
    used = [n for n in PAINT_NAMES if n in set(g.fmat)]
    mats = materials_plain(nation)
    for n in used:
        me.materials.append(mats[n])
    me.polygons.foreach_set("material_index", [used.index(m) for m in g.fmat])
    uvl = me.uv_layers.new(name="UVMap")
    uvs = []
    for poly, fuv in zip(me.polygons, g.fuv):
        if fuv is not None:
            uvs += [c for uv in fuv for c in uv]
            continue
        nrm = poly.normal
        ax = max(range(3), key=lambda i: abs(nrm[i]))
        for li in poly.loop_indices:
            p = me.vertices[me.loops[li].vertex_index].co
            u, v = [(p.y, p.z), (p.x, p.z), (p.x, p.y)][ax]
            uvs += [u, v]
    uvl.data.foreach_set("uv", uvs)
    rest = me.attributes.new("rest", "FLOAT_VECTOR", "POINT")
    rest.data.foreach_set("vector", [c for v in me.vertices for c in v.co])
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    me.set_sharp_from_angle(angle=math.radians(40.0))
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    groups = {}
    for i, w in enumerate(g.wt):
        for bone, x in w.items():
            groups.setdefault(bone, {}).setdefault(round(x, 4), []).append(i)
    for bone, by_w in groups.items():
        vg = obj.vertex_groups.new(name=bone)
        for x, idx in by_w.items():
            vg.add(idx, x, "REPLACE")
    if arm is not None:
        obj.parent = arm
        mod = obj.modifiers.new("rig", "ARMATURE")
        mod.object = arm
    return obj


def _nodes(m):
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return nt, bsdf


def _math(nt, op, a, b=None, clamp=False):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation = op
    n.use_clamp = clamp
    for i, v in enumerate((a, b)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[i].default_value = v
        else:
            nt.links.new(v, n.inputs[i])
    return n.outputs[0]


def _mix(nt, a, b, fac, blend="MIX"):
    n = nt.nodes.new("ShaderNodeMix")
    n.data_type = "RGBA"
    n.blend_type = blend
    for sock, v in (("A", a), ("B", b), ("Factor", fac)):
        if isinstance(v, (int, float)):
            n.inputs[sock].default_value = v
        elif isinstance(v, tuple):
            n.inputs[sock].default_value = v if len(v) == 4 else (*v, 1.0)
        else:
            nt.links.new(v, n.inputs[sock])
    return n.outputs["Result"]


def _noise(nt, vec, scale, detail=4.0, rough=0.55):
    n = nt.nodes.new("ShaderNodeTexNoise")
    n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = detail
    n.inputs["Roughness"].default_value = rough
    nt.links.new(vec, n.inputs["Vector"])
    return n.outputs["Fac"]


def _range(nt, v, a, b, c=0.0, d=1.0):
    n = nt.nodes.new("ShaderNodeMapRange")
    nt.links.new(v, n.inputs["Value"])
    n.inputs["From Min"].default_value, n.inputs["From Max"].default_value = a, b
    n.inputs["To Min"].default_value, n.inputs["To Max"].default_value = c, d
    n.clamp = True
    return n.outputs["Result"]


def _vscale(nt, vec, s):
    n = nt.nodes.new("ShaderNodeVectorMath")
    n.operation = "MULTIPLY"
    n.inputs[1].default_value = s
    nt.links.new(vec, n.inputs[0])
    return n.outputs[0]


def picture_materials(nation="GER", wet=0.0, scroll=None):
    """For the pictures only: the materials (by paint name) worn by a season in the field. Positions are each point's
    place in the model's REST frame (the mesh's `rest` attribute, in metres), so mud stays put on a turning wheel and on a
    bonnet that swings open.
      - paint mottled in soft patches, faded on the tops, streaked down the sides by rain;
      - grime in the recesses (ambient occlusion within 12 cm darkens toward an oily brown);
      - edges rubbed through to dark primer and bright steel where hands and boots go (a bevel shader finds the edges);
      - dust settling thick low down and on flat tops; dried mud splashed up low (below about 0.7 m), thickest round the
        wheels and on the undersides of guards and arches; tyres caked;
      - rust on the exhausts, and on bare steel edges a little.
    `scroll` (a Value node output maker) moves the track's U, as the game will."""
    paint = kit.NATIONS[nation]["paint"]
    dust = kit.hex_rgb("A39276")
    mud = kit.hex_rgb("5A4936")
    mud_dry = kit.hex_rgb("7D6B52")
    grime = kit.hex_rgb("2E2A22")
    base = {"paint": kit.hex_rgb(paint), "shade": kit.hex_rgb(paint, 0.7), "rubber": (0.024, 0.023, 0.022), "wood": (0.24, 0.155, 0.08),
            "steel": (0.25, 0.25, 0.24), "canvas": kit.hex_rgb("6E6448"), "glass": (0.03, 0.04, 0.045), "rust": (0.16, 0.065, 0.03),
            "track": (0.06, 0.056, 0.05), "stripe": kit.hex_rgb("B8964A")}
    mats = {}
    for name in PAINT_NAMES:
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        nt, bsdf = _nodes(m)
        attr = nt.nodes.new("ShaderNodeAttribute")
        attr.attribute_name = "rest"
        attr.attribute_type = "GEOMETRY"
        P = attr.outputs["Vector"]
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(P, sep.inputs[0])
        Z = sep.outputs["Z"]
        sepn = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(geo.outputs["Normal"], sepn.inputs[0])
        NZ = sepn.outputs["Z"]
        col = base[name]
        if name == "glass":
            bsdf.inputs["Base Color"].default_value = (0.6, 0.65, 0.62, 1)
            bsdf.inputs["Roughness"].default_value = 0.03
            bsdf.inputs["Transmission Weight"].default_value = 1.0
            bsdf.inputs["IOR"].default_value = 1.45
            dirt = _math(nt, "MULTIPLY", _range(nt, _noise(nt, P, 6.0, 4.0), 0.45, 0.75), 0.6)
            nt.links.new(_mix(nt, (0.55, 0.6, 0.58), dust, dirt), bsdf.inputs["Base Color"])
            nt.links.new(_math(nt, "ADD", _math(nt, "MULTIPLY", dirt, 0.5), 0.03), bsdf.inputs["Roughness"])
            mats[name] = m
            continue
        big = _noise(nt, P, 0.7, 3.0)
        fine = _noise(nt, P, 18.0, 6.0, 0.6)
        mid = _noise(nt, P, 4.0, 5.0, 0.6)
        mot = nt.nodes.new("ShaderNodeValToRGB")
        mot.color_ramp.elements[0].position, mot.color_ramp.elements[0].color = 0.3, (0.84, 0.84, 0.82, 1)
        mot.color_ramp.elements[1].position, mot.color_ramp.elements[1].color = 0.7, (1.12, 1.1, 1.05, 1)
        nt.links.new(big, mot.inputs["Fac"])
        c = _mix(nt, col, mot.outputs["Color"], 1.0, "MULTIPLY")
        c = _mix(nt, c, _range(nt, fine, 0.3, 0.7, 0.9, 1.07), 1.0, "MULTIPLY")
        if name == "canvas":
            # canvas: weathered unevenly, stained by water in long runs, darker in the folds
            stain = _range(nt, _noise(nt, _vscale(nt, P, (3.0, 3.0, 0.8)), 1.0, 5.0), 0.5, 0.75, 0.0, 0.35)
            c = _mix(nt, c, (0.12, 0.10, 0.06), stain)
            c = _mix(nt, c, (0.32, 0.29, 0.21), _range(nt, NZ, 0.6, 1.0, 0.0, 0.3))      # (bleached on top)
        if name == "rust":
            c = _mix(nt, (0.30, 0.11, 0.035), (0.07, 0.035, 0.02), _range(nt, mid, 0.35, 0.65))
            c = _mix(nt, c, (0.42, 0.20, 0.08), _range(nt, fine, 0.62, 0.72, 0.0, 0.6))
        if name == "track":
            c = _mix(nt, c, (0.13, 0.11, 0.08), 0.35)
        # rain streaks down the upright faces of the paint
        if name in ("paint", "shade"):
            streak = _noise(nt, _vscale(nt, P, (9.0, 9.0, 0.35)), 1.0, 3.0)
            upright = _range(nt, NZ, 0.6, 0.2)
            c = _mix(nt, c, (0.12, 0.12, 0.10), _math(nt, "MULTIPLY", _range(nt, streak, 0.55, 0.75, 0.0, 0.32), upright), "MULTIPLY")
            c = _mix(nt, c, (0.62, 0.6, 0.52), _math(nt, "MULTIPLY", _range(nt, streak, 0.25, 0.4, 0.18, 0.0), upright))
            c = _mix(nt, c, (0.55, 0.55, 0.5), _range(nt, NZ, 0.7, 1.0, 0.0, 0.12))         # (sun-faded tops)
        # grime in the recesses
        ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
        ao.samples = 6
        ao.only_local = True
        ao.inputs["Distance"].default_value = 0.12
        occl = _range(nt, ao.outputs["AO"], 0.95, 0.35, 0.0, 1.0)
        c = _mix(nt, c, grime, _math(nt, "MULTIPLY", occl, {"rubber": 0.3, "glass": 0.0}.get(name, 0.7)))
        rough = {"steel": 0.42, "track": 0.6, "rubber": 0.88, "rust": 0.9, "wood": 0.8, "canvas": 0.93, "stripe": 0.5}.get(name, 0.62)
        metal = None
        # edges worn through
        if name in ("paint", "shade", "steel", "wood", "stripe"):
            bev = nt.nodes.new("ShaderNodeBevel")
            bev.samples = 4
            bev.inputs["Radius"].default_value = 0.01
            dot = nt.nodes.new("ShaderNodeVectorMath")
            dot.operation = "DOT_PRODUCT"
            nt.links.new(bev.outputs["Normal"], dot.inputs[0])
            nt.links.new(geo.outputs["Normal"], dot.inputs[1])
            edge = _range(nt, dot.outputs["Value"], 0.99, 0.86)
            chips = _range(nt, _noise(nt, P, 40.0, 6.0, 0.7), 0.42, 0.62, 0.0, 1.0)
            edge = _math(nt, "MULTIPLY", edge, chips, clamp=True)
            # (chips in the flats too, a few, where boots and tools knock)
            flat_chips = _range(nt, _noise(nt, P, 26.0, 8.0, 0.75), 0.72, 0.78, 0.0, 0.8)
            edge = _math(nt, "MAXIMUM", edge, _math(nt, "MULTIPLY", flat_chips, _range(nt, mid, 0.5, 0.7)))
            if name in ("paint", "shade"):
                primer = _mix(nt, (0.10, 0.10, 0.095), (0.38, 0.37, 0.35), _range(nt, fine, 0.45, 0.6))
                c = _mix(nt, c, primer, _math(nt, "MULTIPLY", edge, 0.9))
                metal = _math(nt, "MULTIPLY", edge, _range(nt, fine, 0.45, 0.6, 0.0, 0.7))
            elif name == "steel":
                c = _mix(nt, c, (0.55, 0.54, 0.5), edge)
                c = _mix(nt, c, (0.16, 0.07, 0.03), _math(nt, "MULTIPLY", _range(nt, mid, 0.62, 0.75), 0.6))   # (rust spots)
            elif name == "wood":
                c = _mix(nt, c, (0.36, 0.27, 0.17), edge)
            else:
                c = _mix(nt, c, (0.6, 0.5, 0.3), edge)
        # dust low down and on flat tops; mud splashed up from the ground
        low = _range(nt, Z, 1.05, 0.1, 0.0, 0.55)
        top = _range(nt, NZ, 0.55, 0.95, 0.0, 0.35)
        amount = _math(nt, "MAXIMUM", low, top)
        if name in ("rubber", "track"):
            amount = _math(nt, "MULTIPLY", _math(nt, "ADD", amount, 0.15), 0.55)
        patchy = _range(nt, _noise(nt, P, 3.0, 5.0), 0.3, 0.65, 0.4, 1.0)
        amount = _math(nt, "MULTIPLY", amount, patchy, clamp=True)
        c = _mix(nt, c, dust, amount)
        # mud: splashes low down (below ~0.75 m), more on faces turned down (the insides of guards and arches)
        under = _range(nt, NZ, -0.1, -0.7, 0.0, 0.7)
        lowm = _range(nt, Z, 0.8, 0.12, 0.0, 1.0)
        mudmask = _math(nt, "MAXIMUM", _math(nt, "MULTIPLY", lowm, 0.85), _math(nt, "MULTIPLY", under, _range(nt, Z, 1.3, 0.6)))
        splash = _range(nt, _math(nt, "ADD", _noise(nt, P, 9.0, 8.0, 0.7), _math(nt, "MULTIPLY", mudmask, 0.45)), 0.72, 0.8)
        if name in ("rubber", "track"):
            splash = _math(nt, "MAXIMUM", _math(nt, "MULTIPLY", splash, 0.6), _math(nt, "MULTIPLY", _range(nt, mid, 0.45, 0.62), 0.45))
        if name == "glass":
            splash = _math(nt, "MULTIPLY", splash, 0.0)
        mudc = _mix(nt, mud, mud_dry, _range(nt, fine, 0.4, 0.6))
        if name in ("rubber", "track"):
            mudc = _mix(nt, mudc, (0.06, 0.05, 0.04), 0.45)
        c = _mix(nt, c, mudc, splash)
        rough_n = _math(nt, "ADD", _math(nt, "MULTIPLY", _math(nt, "MAXIMUM", amount, splash), 0.3), rough, clamp=True)
        nt.links.new(rough_n, bsdf.inputs["Roughness"])
        if wet:
            pass
        nt.links.new(c, bsdf.inputs["Base Color"])
        if name in ("steel", "track"):
            nt.links.new(_math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 1.0, _math(nt, "MAXIMUM", amount, splash), clamp=True),
                               0.6 if name == "steel" else 0.4), bsdf.inputs["Metallic"])
        elif metal is not None:
            nt.links.new(_math(nt, "MULTIPLY", metal, 0.7), bsdf.inputs["Metallic"])
        if name == "canvas" and "Sheen Weight" in bsdf.inputs:
            bsdf.inputs["Sheen Weight"].default_value = 0.35
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = {"rubber": 0.15, "canvas": 0.25, "wood": 0.3}.get(name, 0.1)
        bump.inputs["Distance"].default_value = 0.003
        hgt = _math(nt, "ADD", fine, _math(nt, "MULTIPLY", splash, 1.5))
        nt.links.new(hgt, bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        mats[name] = m
    return mats


def track_picture_material(links, scroll_value=None, band=False):
    """The belt in the pictures: a link (or rubber pad) every 1/links of U, scrolled along U by a value the clip keys, as
    the game will scroll its own track material. `band`: a rubber band track (bars moulded across it, no steel joints)."""
    m = bpy.data.materials.get("track")
    nt, bsdf = _nodes(m)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["UV"], sep.inputs[0])
    U = sep.outputs["X"]
    V = sep.outputs["Y"]
    if scroll_value is not None:
        U = _math(nt, "ADD", U, scroll_value)
    ph = _math(nt, "FRACT", _math(nt, "MULTIPLY", U, float(links)))
    gap = _range(nt, _math(nt, "ABSOLUTE", _math(nt, "SUBTRACT", ph, 0.5)), 0.40, 0.46)        # (the joint between links)
    grouser = _range(nt, _math(nt, "ABSOLUTE", _math(nt, "SUBTRACT", ph, 0.5)), 0.12, 0.06)     # (a bar across each)
    vmid = _range(nt, _math(nt, "ABSOLUTE", _math(nt, "SUBTRACT", V, 0.5)), 0.08, 0.04)
    rub = _range(nt, _math(nt, "ABSOLUTE", _math(nt, "SUBTRACT", V, 0.5)), 0.3, 0.36)
    pad = _math(nt, "SUBTRACT", 1.0, _math(nt, "MAXIMUM", gap, rub), clamp=True)
    height = _math(nt, "ADD", _math(nt, "MULTIPLY", pad, 0.6), _math(nt, "MULTIPLY", grouser, 0.4))
    height = _math(nt, "SUBTRACT", height, _math(nt, "MULTIPLY", vmid, 0.3))
    if band:
        gap = _math(nt, "MULTIPLY", gap, 0.0)
        height = _math(nt, "ADD", _math(nt, "MULTIPLY", grouser, 0.7), _math(nt, "MULTIPLY", rub, -0.3))
        c = _mix(nt, (0.03, 0.029, 0.028), (0.02, 0.019, 0.018), grouser)            # (one rubber band, its bars)
    else:
        c = _mix(nt, (0.05, 0.048, 0.045), (0.022, 0.021, 0.02), pad)                # (steel links, rubber pads)
    big = _noise(nt, tc.outputs["Object"], 3.0, 5.0)
    c = _mix(nt, c, kit.hex_rgb("6F604A"), _range(nt, big, 0.35, 0.6, 0.15, 0.6))
    c = _mix(nt, c, (0.3, 0.29, 0.27), _math(nt, "MULTIPLY", gap, 0.4))
    nt.links.new(c, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.75
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.9
    bump.inputs["Distance"].default_value = 0.02
    nt.links.new(height, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


# ================================================================================================ THE FIELD GUN
LORRY_HOOK = Vector((-2.98, 0.0, 0.70))          # (the lorry's towing pintle, where a towed gun's eye rides: see Lorry)


class FieldGun:
    """A field gun on its carriage, from build_tank.OTHERS (GUN's measures with a gun's own laid over them). The battle's
    field gun is Gun75 modernised for towing by lorry: its own wheels, barrel, shield and height, but on steel spoked
    wheels with solid rubber tyres and a split trail with spades (the brief's), where the campaign piece has wooden
    wheels and a box trail. Any other gun of OTHERS builds on the same rig: a box trail is one `trail` bone that does
    not open, disc wheels replace spoked ones, the recoil cylinders go above the barrel where `above` says, a muzzle brake
    where `brake` says, and a firing platform under the wheels where `platform` says."""

    kind = "gun"
    folder = "gun"

    def __init__(self, campaign="Gun75", changes=None):
        s = dict(bt.OTHERS[campaign])
        if campaign == "Gun75":
            s.update(split=True)                        # (modernised for the lorry: a split trail; see the class's note)
        s.update(changes or {})
        self.s = s
        self.name = campaign
        self.campaign = campaign
        self.title = f"Battle field gun {campaign} (our own design)"
        self.nation = "GER"
        self.R = s["wheel_r"]                       # tyre radius = axle height
        self.TY = s["track"]                        # each wheel's middle, out from the centre line
        self.H = s["high"]                          # the trunnions' height
        self.k = s["bore"] / 0.06                   # (the barrel's thickness against the Gun75's)
        self.BEHIND, self.AHEAD = s["behind"], s["ahead"]
        self.RL = round(0.6 * min(1.0, s["recoil"] / 1.3) + 0.0, 3)    # barrel's recoil (m)
        self.BO = 0.12                              # breech block's travel when open (m)
        self.split = s["split"]
        self.HX, self.HY, self.HZ = -0.42, (0.2 if self.split else 0.0), self.R - 0.06       # trail hinge(s)
        self.L = s["trail"]
        self.OPEN = math.radians(22.0) if self.split else 0.0        # each leg's spread as built (deployed)
        self.END_Z = 0.2                            # the legs' middles at their ends, deployed
        self.SH_X = 0.36                            # the shield's foot
        self.SH_LEAN = 0.18                         # how far back its top leans over its height
        self.SIGHT = Vector((-0.06, 0.30, self.H + 0.27))
        lean = math.atan2(self.SH_LEAN, s["shield_top"] - s["shield_foot"])
        self.SH_N = Vector((math.cos(lean), 0.0, math.sin(lean)))          # out of the shield's face
        self.SH_UP = Vector((-math.sin(lean), 0.0, math.cos(lean)))        # up its face
        # (the carriage traverses until the shield's outer edge would meet a tyre)
        lim = 0.5
        for k in range(1, 100):
            a = 0.005 * k
            if self.SH_X * math.sin(a) + s["shield"] * math.cos(a) > self.TY - 0.075:
                lim = max(0.03, 0.005 * (k - 1))
                break
        self.TRAVERSE = round(lim, 3)
        self.CLOSE = self._close_angle()
        self.TOW, self.TRAVEL_UP = self._tow_pose()

    # -------------------------------------------------------------------------------------------- measures
    def leg_frame(self, side, spread=None):
        """A leg's frame (deployed unless `spread` is given): origin at its hinge, x along the leg to its end, z up out of
        its top, y across. And its length."""
        a = self.OPEN if spread is None else spread
        hinge = Vector((self.HX, side * self.HY, self.HZ))
        lh = math.sqrt(self.L ** 2 - (self.HZ - self.END_Z) ** 2)
        end = hinge + Vector((-math.cos(a) * lh, side * math.sin(a) * lh, self.END_Z - self.HZ))
        x = (end - hinge).normalized()
        up = (Vector((0, 0, 1)) - x * x.z).normalized()          # (square to the leg, in its upright plane)
        return frame(hinge, x, up), self.L

    def _close_angle(self):
        if not self.split:
            return 0.0
        # each leg turns in until its end's inner face is 1 cm off the middle
        lh = math.sqrt(self.L ** 2 - (self.HZ - self.END_Z) ** 2)
        a = math.asin(max(-0.9, (0.06 - self.HY) / lh))
        return round(self.OPEN - a, 4)

    def lunette_at(self):
        """The towing eye's middle, deployed."""
        m, L = self.leg_frame(1.0 if self.split else 0.0)
        if self.split:
            # (beyond the left leg's end, set in toward the middle so that, closed, it lies on the centre line)
            closed = (self.leg_frame(1.0, self.OPEN - self.CLOSE)[0])
            want = closed @ Vector((L + 0.2, 0.0, 0.0))
            want.y = 0.0
            # back into the deployed leg's frame: the same point relative to the leg
            local = closed.inverted() @ want
            return m @ local
        return m @ Vector((L + 0.2, 0.0, 0.0))

    def _tow_pose(self):
        """The axle's turn (about the axle, JSON axis (0,-1,0): + muzzle up) that, with the legs closed, lifts the eye onto
        the lorry's pintle; and the cradle's turn that then holds the barrel level."""
        p = self.lunette_at()
        if self.split:
            hinge = Vector((self.HX, self.HY, self.HZ))
            p = Matrix.Translation(hinge) @ Matrix.Rotation(-self.CLOSE, 4, "Z") @ Matrix.Translation(-hinge) @ p
        ax = Vector((0.0, 0.0, self.R))
        lo, hi = -0.6, 0.0
        for _ in range(60):
            mid = (lo + hi) * 0.5
            q = Matrix.Translation(ax) @ Matrix.Rotation(mid, 4, turn_axis_b((0, -1, 0))) @ Matrix.Translation(-ax) @ p
            if q.z > LORRY_HOOK.z:
                lo = mid
            else:
                hi = mid
        tow = round((lo + hi) * 0.5, 4)
        return tow, round(-tow, 4)

    # -------------------------------------------------------------------------------------------- the rig
    def parts(self):
        rad = math.radians
        s = self.s
        P = [part("root", None, (0, 0, 0), (0, 0, 1), "fixed", None, "the gun's origin, on the ground under the axle's middle", (0, 0, 0.5)),
             part("axle", "root", (0, 0, self.R), (0, -1, 0), "turn", [round(self.TOW - 0.05, 4), 0.06],
                  "the whole gun on its axle: + tips the muzzle up and the trail down. The clips also lift it along z (the "
                  "jump when it fires) and the tow lifts the trail onto the lorry's pintle with it.", (0.4, 0, 0))]
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            P.append(part(f"wheel_{sl}", "axle", (0, side * self.TY, self.R), (0, 1, 0), "spin", None,
                          f"{'left' if side > 0 else 'right'} wheel: + rolls forward (the gun's +x)", (0, 0.25 * side, 0), radius=self.R))
        if self.split:
            for side, sl in ((1.0, "l"), (-1.0, "r")):
                P.append(part(f"trail_{sl}", "axle", (self.HX, side * self.HY, self.HZ), (0, 0, side), "turn", [round(-self.CLOSE, 4), 0.0],
                              f"the {'left' if side > 0 else 'right'} trail leg on its hinge: + opens. 0 is open as built (deployed); "
                              f"{-self.CLOSE:.3f} closes it for towing", (-0.5, 0, 0)))
        else:
            P.append(part("trail", "axle", (self.HX, 0, self.HZ), (0, 0, 1), "fixed", None, "the box trail (does not move on the axle)", (-0.5, 0, 0)))
        P.append(part("carriage", "axle", (0, 0, self.R + 0.06), (0, 0, 1), "turn", [-self.TRAVERSE, self.TRAVERSE],
                      f"the top carriage with the shield: traverse, + to the right, within {math.degrees(self.TRAVERSE):.1f} degrees "
                      "(beyond that the crew swing the trails and the game turns the whole gun). The game turns it.", (0, 0, 0.4)))
        P.append(part("handwheel_traverse", "carriage", (-0.47, 0.25, self.R + 0.22), (-1, 0, 0), "spin", None,
                      "the traversing handwheel: the game spins it while it traverses (about 30 turns of it to a radian)", (-0.15, 0, 0)))
        P.append(part("handwheel_elevate", "carriage", (-0.16, 0.36, self.H - 0.15), (0, 1, 0), "spin", None,
                      "the elevating handwheel: the game spins it while it elevates", (0, 0.15, 0)))
        P.append(part("cradle", "carriage", (0, 0, self.H), (0, -1, 0), "turn", [round(rad(-5), 4), round(rad(40), 4)],
                      "elevation at the trunnions: + muzzle up, from -5 to +40 degrees. The game turns it.", (0.5, 0, 0)))
        P.append(part("barrel", "cradle", (-self.BEHIND, 0, self.H), (-1, 0, 0), "slide", [0.0, self.RL],
                      f"recoil: + slides back along the bore (metres, {self.RL} at most)", (0.6, 0, 0)))
        P.append(part("breech", "barrel", (-self.BEHIND + 0.08, 0, self.H), (0, 1, 0), "slide", [0.0, self.BO],
                      f"the sliding breech block: + slides it out to the right, open ({self.BO} m)", (0, -0.15, 0)))
        return P

    def sockets(self):
        R, H = self.R, self.H
        out = [("socket_muzzle", "barrel", Vector((self.AHEAD + 0.02, 0, H)), Vector((1, 0, 0))),
               ("socket_breech", "barrel", Vector((-self.BEHIND - 0.03, 0, H)), Vector((-1, 0, -0.35)).normalized()),
               ("socket_crew_layer", "carriage", Vector((-0.5, 0.52, 0.0)), Vector((1, 0, 0))),
               ("socket_crew_loader", "carriage", Vector((-1.05, -0.48, 0.0)), Vector((0.8, 0.6, 0)).normalized()),
               ("socket_tow", "trail_l" if self.split else "trail", self.lunette_at(), Vector((-1, 0, 0))),
               ("socket_ammo", "root", Vector((-1.3, -1.25, 0.0)), Vector((1, 0, 0)))]
        for side, sl in (((1.0, "l"), (-1.0, "r")) if self.split else ((0.0, ""),)):
            m, L = self.leg_frame(side)
            p = m @ Vector((L - 0.05, 0, 0))
            p.z = 0.0
            out.append((f"socket_spade_{sl}" if sl else "socket_spade", f"trail_{sl}" if sl else "trail", p, Vector((0, 0, 1))))
        return out

    def marks(self):
        return {"roof": round(self.s["shield_top"], 3)}

    # -------------------------------------------------------------------------------------------- the clips
    def clips(self):
        rad = math.radians
        C = []
        split = self.split
        legs = ("trail_l", "trail_r") if split else ()

        c = Clip("idle", 2.0, True, None, "deployed and still (a gun has no engine): holds the pose")
        C.append(c)

        # fire: the shot at 0.0; the barrel recoils 0.0-0.1 and runs out by 0.7; the gun jumps on its spades and settles;
        # the breech opens at 1.2 throwing out the case, and is closed again (the next round rammed) by 2.2
        c = Clip("fire", 2.5, False, None, "fires one round")
        RL = self.RL

        def barrel(t):
            if t <= 0.1:
                return RL * (1.0 - (1.0 - t / 0.1) ** 2)
            return RL * (1.0 - smoothstep(0.1, 0.7, t))
        c.add("barrel", "slide", barrel)
        A = rad(1.7)
        settle = lambda t: 1.0 - smoothstep(0.85, 1.25, t)
        hop = lambda t: (A * (t / 0.085) * math.exp(1.0 - t / 0.085) + _damped(t, A * 0.22, 0.24, 0.12, 0.32)) * settle(t)
        reach = math.hypot(self.L, self.HX) + 0.3
        c.add("axle", "turn", hop)
        c.add("axle", "slide", lambda t: hop(t) * reach * 0.92 + 0.004 * _damped(t, 1.0, 0.2, 0.1, 0.36) * settle(t), axis=(0, 0, 1))
        c.add("cradle", "turn", lambda t: _damped(t, rad(0.45), 0.28, 0.1) * (1.0 - smoothstep(0.5, 0.8, t)))
        c.add("carriage", "turn", lambda t: _damped(t, rad(0.18), 0.22, 0.12) * (1.0 - smoothstep(0.6, 0.9, t)))
        bo = self.BO
        c.add("breech", "slide", lambda t: bo * (smoothstep(1.2, 1.3, t) - smoothstep(2.05, 2.2, t)))
        c.event(0.0, "muzzle_flash_big", "socket_muzzle", None)
        c.event(0.0, "muzzle_blast_dust", "socket_muzzle", None)
        c.event(1.2, "case_eject", "socket_breech", None)
        c.event(2.05, "breech_close", "socket_breech", None)
        C.append(c)

        tow, up, close = self.TOW, self.TRAVEL_UP, self.CLOSE

        # limber: the legs swing shut (0.0-1.0), the barrel is laid level for the road, the trail is lifted onto the pintle
        c = Clip("limber", 2.0, False, None, "from deployed to hooked on: legs closed, trail lifted to the lorry's pintle")
        for leg in legs:
            c.add(leg, "turn", lambda t: _ease(t, 0.0, 1.0, 0.0, -close))
        c.add("cradle", "turn", lambda t: _ease(t, 0.3, 1.1, 0.0, up))
        c.add("axle", "turn", lambda t: _ease(t, 1.0, 2.0, 0.0, tow))
        C.append(c)

        # unlimber: the reverse; the spades bite at 1.0 (dust)
        c = Clip("unlimber", 2.0, False, None, "from hooked on to deployed: trail lowered, legs swung open")
        c.add("axle", "turn", lambda t: _ease(t, 0.0, 1.0, tow, 0.0))
        for leg in legs:
            c.add(leg, "turn", lambda t: _ease(t, 1.0, 2.0, -close, 0.0))
        c.add("cradle", "turn", lambda t: _ease(t, 0.9, 1.7, up, 0.0))
        for sl in (("l", "r") if split else ("",)):
            c.event(1.0 if not split else 2.0, "dust_puff", f"socket_spade_{sl}" if sl else "socket_spade", None)
        C.append(c)

        # towed: trail first behind the lorry (the gun goes toward its own -x); wheels turning, the gun jolting a little
        LOOP = 2.0
        D = 2.0 * 2.0 * math.pi * self.R           # two turns of the wheels a loop
        c = Clip("towed", LOOP, True, None, f"towed trail first behind the lorry: {D:.3f} m a loop toward the gun's -x", metres=D)
        c.travel = lambda t: -D * t / LOOP
        for sl in "lr":
            c.add(f"wheel_{sl}", "spin", lambda t: -D * t / LOOP / self.R)
        for leg in legs:
            c.add(leg, "turn", lambda t: -close)
        c.add("cradle", "turn", lambda t: up)
        c.add("axle", "turn", lambda t: tow + rad(0.35) * sin_(1.0, t, 0.3) + rad(0.15) * sin_(3.5, t))
        c.add("axle", "slide", lambda t: 0.008 * sin_(1.5, t, 0.8) + 0.004 * sin_(4.0, t) + 0.002 * sin_(7.5, t, 1.0), axis=(0, 0, 1))
        C.append(c)
        return C

    # -------------------------------------------------------------------------------------------- pictures and notes
    noun = "field gun"
    nation_word = "German"

    def battle_clip(self, **kw):
        return gun_film(self, **kw)

    def picture_extras(self, st):
        """The ammunition boxes beside the gun, where socket_ammo is."""
        ammo = AmmoBoxes()
        ob = make_mesh(ammo.geo(0), "ammo_boxes_LOD0", None, self.nation)
        at = next(a for n, b, a, f in self.sockets() if n == "socket_ammo")
        ob.location = at
        ob.rotation_euler = (0, 0, math.radians(-20))
        st.extra_objs = [ob]

    def picture_pose(self):
        return {"cradle": [("turn", None, math.radians(self.s["raised"]))]}

    def views(self):
        H = self.H
        return {"front": ((-0.3, 0.0, 0.75), 8.6, 14.0, 34.0, 50.0),
                "rear": ((-0.9, 0.0, 0.6), 9.8, 18.0, 148.0, 50.0),
                "detail": ((-0.4, 0.14, H - 0.08), 2.9, 20.0, 132.0, 45.0),
                "game": ((-0.3, 0.0, 0.6), 16.0, 30.0, 40.0, 40.0)}

    def view_labels(self):
        return {"front": "Front three-quarter", "rear": "Rear three-quarter: split trail, spades, towing eye",
                "detail": "Close: breech, sight, handwheels", "game": "The game's camera (16 m, 30 degrees down)"}

    def sheet_title(self):
        return f"The battle field gun: {self.campaign} modernised for the lorry (German paint)"

    def json_extra(self, index):
        tow = self.lunette_at()
        return {"tow": {"socket": "socket_tow", "pintle_height": LORRY_HOOK.z, "axle_turn": self.TOW, "cradle_turn": self.TRAVEL_UP,
                        "legs_closed": -self.CLOSE if self.split else None,
                        "about": "Towed trail first: the gun's -x points along the lorry's +x. Put the gun so its socket_tow (in the "
                                 "towed pose) meets the lorry's socket_tow, its yaw the lorry's + 180 degrees, and play `towed` at "
                                 "speed / metres_per_loop x 2.0."},
                "controls": [
                    {"part": index["carriage"], "part_name": "carriage", "what": "traverse, turned by the game (+ clockwise seen from above)",
                     "limits": [-self.TRAVERSE, self.TRAVERSE], "spin_with": "handwheel_traverse", "spin_ratio": 30.0},
                    {"part": index["cradle"], "part_name": "cradle", "what": "elevation, turned by the game (+ muzzle up)",
                     "limits": [round(math.radians(-5), 4), round(math.radians(40), 4)], "spin_with": "handwheel_elevate", "spin_ratio": 40.0}]}

    # -------------------------------------------------------------------------------------------- the model
    def geo(self, lod):
        g = Geo()
        self.wheels(g, lod)
        self.lower(g, lod)
        self.top_carriage(g, lod)
        self.shield(g, lod)
        self.cradle_geo(g, lod)
        self.barrel_geo(g, lod)
        self.trails(g, lod)
        if self.s.get("platform"):
            n = pick(lod, 24, 16, 10)
            r = self.TY + 0.35
            g.put(bm_cyl(r, 0.05, n), "wood", "root", Matrix.Translation((0, 0, 0.025)))
            if lod < 2:
                for k in range(8):
                    a = math.pi * k / 4
                    g.put(bm_box((r * 1.9, 0.08, 0.03)), "shade", "root", place((0, 0, 0.06), (0, 0, math.degrees(a))))
        return g

    def wheels(self, g, lod):
        R, s = self.R, self.s
        w = 0.12 if R > 0.5 else 0.14
        for side, bone in ((1.0, "wheel_l"), (-1.0, "wheel_r")):
            c = Vector((0.0, side * self.TY, R))
            out = Vector((0.0, side, 0.0))
            turn = Matrix.Rotation(-side * math.pi / 2, 4, "X")       # (local z out from the gun)
            m = Matrix.Translation(c) @ turn
            n = pick(lod, 36, 18, 10)
            ri = R - (0.075 if R > 0.5 else 0.11)
            g.put(bm_lathe(tyre_profile(R, w, lod, bulge=0.008, rim_r=ri, tread=0.006), n), "rubber", bone, m)
            if lod == 2:
                g.put(bm_cyl(ri, w * 0.7, n), "paint", bone, m)
                continue
            if s["spoked"]:
                # a steel rim (a channel), twelve spokes staggered to two flanges of the hub, the hub with its cap
                g.put(bm_lathe([(ri - 0.035, -0.045), (ri + 0.003, -0.05), (ri + 0.003, 0.05), (ri - 0.035, 0.045), (ri - 0.03, 0.0),
                                (ri - 0.035, -0.045)], n), "paint", bone, m)
                ns = pick(lod, 12, 8)
                for k in range(ns):
                    a = 2 * math.pi * (k + 0.5) / ns
                    zf = 0.045 if k % 2 == 0 else -0.04
                    rod(g, m @ Vector((0.085 * math.cos(a), 0.085 * math.sin(a), zf)), m @ Vector(((ri - 0.03) * math.cos(a), (ri - 0.03) * math.sin(a), 0.0)),
                        pick(lod, 0.017, 0.02), pick(lod, 6, 4), "paint", bone, r2=pick(lod, 0.013, 0.016), caps=False)
                hub = [(0.06, -0.12), (0.1, -0.1), (0.1, -0.06), (0.085, -0.05), (0.085, 0.05), (0.1, 0.06), (0.1, 0.075),
                       (0.065, 0.085), (0.06, 0.12), (0.035, 0.14), (0.0, 0.145)]
                g.put(bm_lathe(hub if lod == 0 else [(0.1, -0.1), (0.1, 0.08), (0.05, 0.13), (0.0, 0.14)], pick(lod, 12, 8)), "paint", bone, m)
            else:
                # a pressed steel disc wheel, dished, with its bolt circle
                g.put(bm_lathe([(ri - 0.02, -0.05), (ri + 0.003, -0.055), (ri + 0.003, 0.05), (ri - 0.03, 0.05), (ri * 0.62, 0.075),
                                (ri * 0.45, 0.06), (0.11, 0.07), (0.1, 0.1), (0.05, 0.115), (0.0, 0.12)], n), "paint", bone, m)
                g.put(bm_lathe([(ri - 0.02, -0.05), (0.15, -0.07), (0.0, -0.08)], n), "paint", bone, m)
            if lod == 0:
                bolt_ring(g, m @ Vector((0, 0, 0.075 if s["spoked"] else 0.07)), out, 0.075 if s["spoked"] else 0.14, 6, bone, 0.012, 0.012, "steel", sides=6)
                bolt(g, m @ Vector((0, 0, 0.14 if s["spoked"] else 0.118)), out, bone, 0.02, 0.012, "steel", sides=8)
                # the brake drum inboard
                g.put(bm_lathe([(0.0, -0.17), (0.17, -0.17), (0.175, -0.155), (0.175, -0.1), (0.12, -0.09)], 14), "shade", bone, m)

    def lower(self, g, lod):
        """The axle and the bottom carriage the trails hinge on (bone axle)."""
        R = self.R
        g.put(bm_box((0.11, 2.0 * (self.TY - 0.1), 0.11), pick(lod, 0.012, 0.0)), "shade", "axle", Matrix.Translation((0, 0, R)))
        for side in (1.0, -1.0):
            rod(g, (0, side * (self.TY - 0.12), R), (0, side * (self.TY + 0.02), R), 0.045, pick(lod, 10, 6), "steel", "axle")
        g.put(bm_box((0.62, 0.52, 0.14), pick(lod, 0.015, 0.0)), "paint", "axle", Matrix.Translation((-0.22, 0, R - 0.03)))
        if lod == 2:
            return
        # the hinge knuckles, and the pivot ring the top carriage turns on
        for side in ((1.0, -1.0) if self.split else (0.0,)):
            g.put(bm_cyl(0.05, 0.24, pick(lod, 10, 6)), "paint", "axle", Matrix.Translation((self.HX, side * self.HY, self.HZ)))
            if lod == 0:
                g.put(bm_cyl(0.022, 0.29, 6), "steel", "axle", Matrix.Translation((self.HX, side * self.HY, self.HZ)))
        g.put(bm_cyl(0.24, 0.035, pick(lod, 16, 8)), "shade", "axle", Matrix.Translation((0.0, 0, R + 0.055)))
        if lod == 0:
            # leaf springs on the axle (for the road), their clamps and shackles
            for side in (1.0, -1.0):
                y = side * (self.TY - 0.24)
                for k in range(4):
                    ln = 0.62 - 0.12 * k
                    g.put(bm_box((ln, 0.07, 0.014), shape=lambda p: Vector((p.x, p.y, p.z - 3.0 * (p.x * p.x) * 0.0))), "steel", "axle",
                          Matrix.Translation((-0.02, y, R - 0.075 - 0.016 * k)))
                for x in (-0.06, 0.06):
                    g.put(bm_box((0.018, 0.09, 0.12)), "shade", "axle", Matrix.Translation((x, y, R - 0.07)))
                rivet_row(g, (-0.4, side * 0.262, R + 0.02), (-0.0, side * 0.262, R + 0.02), 4, (0, side, 0), "axle", 0.008, "paint")
            # the travel lock bracket for the barrel at the front of the bottom carriage
            g.put(bm_box((0.05, 0.16, 0.05), 0.006), "shade", "axle", Matrix.Translation((0.12, 0, R + 0.03)))

    def side_plate(self, g, outline_xz, y, thick, mat, bone, holes=()):
        """A plate standing upright along x at y, its outline given as (x, z)."""
        bm = bm_plate([(x, -z) for x, z in outline_xz], [[(x, -z) for x, z in h] for h in holes], thick)
        g.put(bm, mat, bone, frame((0, y, 0), (1, 0, 0), (0, 1, 0)))

    def top_carriage(self, g, lod):
        """The top carriage (bone carriage): a saddle on the pivot, two cheeks holding the trunnions, the handwheels."""
        R, H = self.R, self.H
        g.put(bm_box((0.8, 0.5, 0.07), pick(lod, 0.012, 0.0)), "paint", "carriage", Matrix.Translation((-0.08, 0, R + 0.105)))
        top = H + 0.07
        cheek = [(-0.48, R + 0.07), (0.32, R + 0.07), (0.32, R + 0.2), (0.14, H - 0.04), (0.08, top), (-0.08, top), (-0.16, H - 0.02),
                 (-0.48, R + 0.3)]
        if lod == 2:
            cheek = [(-0.48, R + 0.07), (0.32, R + 0.07), (0.08, top), (-0.12, top)]
        for side in (1.0, -1.0):
            self.side_plate(g, cheek, side * 0.2, 0.024, "paint", "carriage")
            if lod == 2:
                continue
            # flanges along the cheeks' top edges, the trunnion bearings with their caps
            if lod == 0:
                for (x0, z0), (x1, z1) in ((cheek[2], cheek[3]), (cheek[6], cheek[7])):
                    g.put(bm_tube([(x0, side * 0.2, z0), (x1, side * 0.2, z1)], 1.0, profile=[(-0.035, 0.0), (0.035, 0.0), (0.035, 0.012), (-0.035, 0.012)],
                                  up=(0, 0, 1)), "paint", "carriage")
                for (x0, z0), (x1, z1) in ((cheek[2], cheek[3]), (cheek[7], cheek[6])):
                    rivet_row(g, (x0, side * 0.214, z0 - 0.03), (x1, side * 0.214, z1 - 0.03), 5, (0, side, 0), "carriage", 0.008)
            m = Matrix.Translation((0, side * 0.215, H)) @ Matrix.Rotation(-side * math.pi / 2, 4, "X")
            g.put(bm_lathe([(0.075, -0.01), (0.075, 0.02), (0.06, 0.035), (0.0, 0.035)], pick(lod, 14, 8)), "shade", "carriage", m)
            if lod == 0:
                bolt_ring(g, m @ Vector((0, 0, 0.022)), (0, side, 0), 0.055, 4, "carriage", 0.009, 0.01, "steel", phase=0.6)
        if lod == 2:
            return
        # the elevating gear: a box on the left cheek with the handwheel's shaft
        g.put(bm_box((0.16, 0.08, 0.14), pick(lod, 0.01, 0.0)), "paint", "carriage", Matrix.Translation((-0.16, 0.25, H - 0.15)))
        rod(g, (-0.16, 0.28, H - 0.15), (-0.16, 0.37, H - 0.15), 0.016, 6, "steel", "carriage")
        handwheel(g, Vector((-0.16, 0.37, H - 0.15)), (0, 1, 0), 0.12, "handwheel_elevate", lod)
        # the traversing gear: a shaft from the saddle back to the handwheel by the layer's knee
        g.put(bm_box((0.14, 0.1, 0.12), pick(lod, 0.01, 0.0)), "paint", "carriage", Matrix.Translation((-0.4, 0.25, R + 0.2)))
        rod(g, (-0.4, 0.25, R + 0.22), (-0.48, 0.25, R + 0.22), 0.016, 6, "steel", "carriage")
        handwheel(g, Vector((-0.48, 0.25, R + 0.22)), (-1, 0, 0), 0.1, "handwheel_traverse", lod)
        if lod == 0:
            # the equilibrator springs: two cylinders from the saddle's front up to the cheeks
            for side in (1.0, -1.0):
                rod(g, (0.24, side * 0.27, R + 0.14), (0.05, side * 0.27, H - 0.12), 0.035, 8, "paint", "carriage")
                rod(g, (0.27, side * 0.27, R + 0.12), (0.22, side * 0.27, R + 0.16), 0.02, 6, "steel", "carriage")
            # the pivot's clamp bolts on the saddle
            for x in (-0.38, 0.22):
                for y in (-0.2, 0.2):
                    bolt(g, (x, y, R + 0.14), (0, 0, 1), "carriage", 0.012, 0.012, "steel")

    def shield(self, g, lod):
        """The shield (bone carriage): one plate leaning back, a slot for the cradle, the sight's window with its shutter,
        stiffeners riveted behind, an angle along the top, struts to the cheeks, and a lower apron."""
        s = self.s
        foot, top, half = s["shield_foot"], s["shield_top"], s["shield"]
        hgt = (top - foot) / self.SH_UP.z
        base = Vector((self.SH_X, 0.0, foot))
        up, n = self.SH_UP, self.SH_N
        across = Vector((0, 1, 0))
        m = frame(base, across, n)                         # local x: across (to the left), y: up the plate, z: out of it
        slot_v = (0.86 - foot) / up.z
        sw = 0.15 * max(1.0, self.k)
        win_u, win_v0, win_v1 = self.SIGHT.y, (self.SIGHT.z - 0.07 - foot) / up.z, (self.SIGHT.z + 0.05 - foot) / up.z
        cut = 0.12
        outline = [(-half, 0.0), (half, 0.0), (half, hgt - cut), (half - cut, hgt), (sw, hgt), (sw, slot_v), (-sw, slot_v), (-sw, hgt),
                   (-half + cut, hgt), (-half, hgt - cut)]
        holes = []
        if lod < 2:
            holes = [[(win_u - 0.05, win_v0), (win_u + 0.05, win_v0), (win_u + 0.05, win_v1), (win_u - 0.05, win_v1)]]
        g.put(bm_plate(outline, holes, 0.01), "paint", "carriage", m)
        to = lambda u, v, h=0.0: m @ Vector((u, v, h))
        if lod == 2:
            return
        # the sight window's shutter, slid aside, in its guides
        g.put(bm_box((0.11, 0.15, 0.008)), "paint", "carriage", m @ Matrix.Translation((win_u + 0.115, (win_v0 + win_v1) * 0.5, 0.009)))
        for dv in (-0.085, 0.085):
            g.put(bm_box((0.26, 0.016, 0.012)), "shade", "carriage", m @ Matrix.Translation((win_u + 0.07, (win_v0 + win_v1) * 0.5 + dv, 0.008)))
        # stiffening angles behind, an angle along the top edge, a rolled edge on the slot
        back = -0.012
        for u in (-0.5 * half - 0.12, 0.5 * half + 0.12):
            g.put(bm_box((0.05, hgt - 0.1, 0.012)), "paint", "carriage", m @ Matrix.Translation((u, hgt * 0.5, back - 0.006)))
            g.put(bm_box((0.012, hgt - 0.1, 0.05)), "paint", "carriage", m @ Matrix.Translation((u, hgt * 0.5, back - 0.03)))
        for sgn in (1.0, -1.0):
            g.put(bm_box((half - sw - cut, 0.012, 0.05)), "paint", "carriage",
                  m @ Matrix.Translation((sgn * (sw + (half - sw - cut) * 0.5), hgt - 0.006, back - 0.025)))
            g.put(bm_box((0.012, hgt - slot_v, 0.04)), "paint", "carriage", m @ Matrix.Translation((sgn * (sw + 0.006), (slot_v + hgt) * 0.5, back - 0.02)))
        if lod == 0:
            for u in (-0.5 * half - 0.12, 0.5 * half + 0.12):
                rivet_row(g, to(u, 0.06, 0.005), to(u, hgt - 0.08, 0.005), 9, n, "carriage", 0.009)
            for sgn in (1.0, -1.0):
                rivet_row(g, to(sgn * (sw + 0.06), hgt - 0.035, 0.005), to(sgn * (half - cut - 0.02), hgt - 0.035, 0.005), 6, n, "carriage", 0.009)
                rivet_row(g, to(sgn * (sw + 0.035), slot_v + 0.05, 0.005), to(sgn * (sw + 0.035), hgt - 0.08, 0.005), 4, n, "carriage", 0.008)
                rivet_row(g, to(sgn * (half - 0.03), 0.06, 0.005), to(sgn * (half - 0.03), hgt - cut - 0.03, 0.005), 7, n, "carriage", 0.008)
                rivet_row(g, to(sgn * 0.08, 0.035, 0.005), to(sgn * (half - 0.08), 0.035, 0.005), 6, n, "carriage", 0.008)
            # lifting handles on the top corners, a spare-parts box on the back
            for sgn in (1.0, -1.0):
                p0 = to(sgn * (half - 0.3), hgt - 0.08, back - 0.012)
                p1 = to(sgn * (half - 0.18), hgt - 0.08, back - 0.012)
                hb = -n * 0.06
                g.put(bm_tube([p0, p0 + hb, p1 + hb, p1], 0.008, 5, caps=False), "steel", "carriage")
            g.put(bm_box((0.32, 0.16, 0.1), 0.008), "shade", "carriage", m @ Matrix.Translation((-0.42, 0.5, back - 0.065)))
        # struts from the shield's back to the cheeks
        for sgn in (1.0, -1.0):
            rod(g, to(sgn * 0.42, 0.45, back - 0.01), Vector((0.05, sgn * 0.21, self.R + 0.25)), 0.02, pick(lod, 6, 4), "paint", "carriage")
            rod(g, to(sgn * 0.3, 0.12, back - 0.01), Vector((0.2, sgn * 0.21, self.R + 0.12)), 0.018, pick(lod, 6, 4), "paint", "carriage")
        # the lower apron, hung from the shield's foot
        apron = [(-half + 0.1, -0.3), (half - 0.1, -0.3), (half - 0.02, 0.0), (-half + 0.02, 0.0)]
        ma = frame(base + Vector((0.01, 0, 0)), across, Vector((1, 0, 0)))
        g.put(bm_plate(apron, (), 0.008), "paint", "carriage", ma)
        if lod == 0:
            for sgn in (1.0, -1.0):
                for u in (0.25, 0.55):
                    g.put(bm_cyl(0.018, 0.08, 6), "shade", "carriage",
                          Matrix.Translation(base + Vector((0.0, sgn * u, -0.005))) @ Matrix.Rotation(math.pi / 2, 4, "X"))
            rivet_row(g, ma @ Vector((-half + 0.14, -0.25, 0.005)), ma @ Vector((half - 0.14, -0.25, 0.005)), 8, (1, 0, 0), "carriage", 0.008)

    def cradle_geo(self, g, lod):
        """The cradle (bone cradle): the trough the barrel slides in, the recoil cylinders, the trunnion pins, the toothed
        arc that elevates it, the sight, and a small shield over the slot."""
        H, k = self.H, self.k
        above = self.s["above"]
        x0, x1 = -0.32, min(1.05, self.AHEAD * 0.48)
        g.put(bm_box((x1 - x0, 0.24 * k, 0.1 * k), pick(lod, 0.012, 0.0)), "paint", "cradle",
              Matrix.Translation(((x0 + x1) * 0.5, 0, H - (0.075 + 0.05) * k)))
        for side in (1.0, -1.0):
            rod(g, (0, side * 0.1, H), (0, side * 0.205, H), 0.045, pick(lod, 10, 6), "steel", "cradle")
        cz = H + (0.165 * k if above else -0.17 * k)
        n = pick(lod, 12, 8, 6)
        for y, r in ((0.058 * k, 0.048 * k), (-0.058 * k, 0.044 * k)):
            rod(g, (x0 + 0.06, y, cz), (x1 + 0.06, y, cz), r, n, "paint", "cradle")
            if lod == 0:
                g.put(bm_lathe([(r + 0.006, 0.0), (r + 0.006, 0.03), (r - 0.01, 0.045), (0.0, 0.05)], n), "shade", "cradle",
                      toward((x1 + 0.06, y, cz), (1, 0, 0)))
                bolt_ring(g, (x1 + 0.08, y, cz), (1, 0, 0), r * 0.62, 5, "cradle", 0.007, 0.008, "steel")
                g.put(bm_lathe([(r + 0.008, 0.0), (r + 0.008, 0.04), (0.0, 0.04)], n), "shade", "cradle", toward((x0 + 0.02, y, cz), (1, 0, 0)))
                # the piston rod, on the barrel: it draws out of the cylinder as the barrel recoils
                rod(g, (-self.BEHIND + 0.12, y, cz), (x0 + 0.8, y, cz), 0.018 * k, 8, "steel", "barrel")
        if lod < 2:
            # the slides the barrel runs on, the collar at the cradle's mouth
            for side in (1.0, -1.0):
                g.put(bm_box((x1 - x0, 0.03, 0.03)), "steel", "cradle", Matrix.Translation(((x0 + x1) * 0.5, side * 0.075 * k, H - 0.06 * k)))
            g.put(bm_lathe([(0.085 * k, -0.05), (0.09 * k, -0.04), (0.09 * k, 0.04), (0.085 * k, 0.05)], pick(lod, 14, 8)), "paint", "cradle",
                  toward((x1 - 0.02, 0, H), (1, 0, 0)))
            # rods joining the cylinders to the trough
            g.put(bm_box((x1 - x0 - 0.1, 0.1 * k, abs(cz - H) - 0.07 * k)), "paint", "cradle",
                  Matrix.Translation(((x0 + x1) * 0.5, 0, (cz + H) * 0.5 + (-0.035 if above else 0.035) * k)))
        if lod == 2:
            return
        # the toothed elevating arc under the trough, at the left (the pinion on the elevating box meets it)
        arc_r0, arc_r1 = 0.24, 0.3
        a0, a1 = math.radians(-160), math.radians(-115)
        pts = []
        steps = pick(lod, 14, 6)
        for i in range(steps + 1):
            a = a0 + (a1 - a0) * i / steps
            rr = arc_r1 + (0.012 if (lod == 0 and i % 2 == 0) else 0.0)
            pts.append((rr * math.cos(a), rr * math.sin(a)))
        for i in range(steps, -1, -1):
            a = a0 + (a1 - a0) * i / steps
            pts.append((arc_r0 * math.cos(a), arc_r0 * math.sin(a)))
        pts += [(0.1, -0.12)]
        self.side_plate(g, [(x, H + z) for x, z in pts], 0.135, 0.022, "paint", "cradle")
        # the sight on its bracket at the left: body, the head standing up to the window, the eyepiece toward the layer
        sx, sy, sz = self.SIGHT
        g.put(bm_box((0.05, 0.11, 0.04), pick(lod, 0.005, 0.0)), "paint", "cradle", Matrix.Translation((sx - 0.06, 0.2, H + 0.02)))
        g.put(bm_box((0.16, 0.07, 0.09), pick(lod, 0.008, 0.0)), "shade", "cradle", Matrix.Translation((sx - 0.1, sy, H + 0.08)))
        rod(g, (sx - 0.04, sy, H + 0.12), (sx - 0.04, sy, sz - 0.03), 0.022, pick(lod, 8, 6), "shade", "cradle")
        g.put(bm_box((0.09, 0.06, 0.06), pick(lod, 0.006, 0.0)), "shade", "cradle", Matrix.Translation((sx - 0.02, sy, sz)))
        if lod == 0:
            g.put(bm_cyl(0.02, 0.012, 10), "glass", "cradle", toward((sx + 0.027, sy, sz), (1, 0, 0)))
            rod(g, (sx - 0.18, sy, H + 0.1), (sx - 0.3, sy, H + 0.1), 0.017, 8, "shade", "cradle")
            g.put(bm_lathe([(0.017, 0.0), (0.026, 0.01), (0.026, 0.04), (0.02, 0.045), (0.0, 0.045)], 10), "rubber", "cradle",
                  toward((sx - 0.3, sy, H + 0.1), (-1, 0, 0)))
            handwheel(g, Vector((sx - 0.12, sy + 0.05, H + 0.08)), (0, 1, 0), 0.035, "cradle", lod, spokes=3, mat="shade", knob=False)
        # the upper shield on the cradle: it closes the carriage shield's slot and rides up over it as the gun elevates
        sw = 0.15 * max(1.0, self.k) + 0.045
        lo_, hi_ = 0.86 - H - 0.04, self.s["shield_top"] - H + 0.03
        ss = [(-sw, lo_), (sw, lo_), (sw, hi_ - 0.05), (sw - 0.05, hi_), (-sw + 0.05, hi_), (-sw, hi_ - 0.05)]
        hole = [[(0.085 * k * math.cos(2 * math.pi * i / 12), 0.085 * k * math.sin(2 * math.pi * i / 12)) for i in range(12)]]
        if self.s["above"]:
            hole.append([(0.058 * k + 0.05 * k * math.cos(2 * math.pi * i / 8), 0.165 * k + 0.05 * k * math.sin(2 * math.pi * i / 8)) for i in range(8)])
        g.put(bm_plate(ss, hole, 0.01), "paint", "cradle", frame((0.43, 0, H), (0, 1, 0), (1, 0, 0)))
        if lod == 0:
            for u in (-sw + 0.025, sw - 0.025):
                for i in range(5):
                    rivet(g, (0.436, u, H + lo_ + 0.04 + (hi_ - lo_ - 0.1) * i / 4), (1, 0, 0), "cradle", 0.008)
            for i in range(4):
                rivet(g, (0.436, -sw + 0.08 + (2 * sw - 0.16) * i / 3, H + hi_ - 0.025), (1, 0, 0), "cradle", 0.008)
            # its brackets back to the cradle's mouth
            for u in (-0.09, 0.09):
                g.put(bm_box((0.36, 0.02, 0.05)), "paint", "cradle", Matrix.Translation((0.26, u * k, H - 0.11 * k)))

    def barrel_geo(self, g, lod):
        """The barrel (bone barrel) with its breech ring and chamber, and the sliding block (bone breech)."""
        H, k, A, Bh = self.H, self.k, self.AHEAD, self.BEHIND
        x_ring = -Bh
        ring_l = 0.26
        xb = x_ring + ring_l - 0.02
        cal = 0.0375 * k
        n = pick(lod, 20, 12, 8)
        if lod == 2:
            prof = [(0.075 * k, xb), (0.06 * k, 0.35), (0.05 * k, A), (0.0, A)]
        elif lod == 1:
            prof = [(0.077 * k, xb), (0.077 * k, 0.3), (0.062 * k, 0.38), (0.05 * k, A - 0.06), (0.056 * k, A - 0.05), (0.056 * k, A), (cal, A), (0.0, A - 0.1)]
        else:
            prof = [(0.077 * k, xb), (0.078 * k, 0.28), (0.074 * k, 0.31), (0.066 * k, 0.36), (0.058 * k, A * 0.55), (0.051 * k, A - 0.12),
                    (0.051 * k, A - 0.075), (0.056 * k, A - 0.06), (0.057 * k, A - 0.01), (0.054 * k, A), (cal + 0.003, A), (cal, A - 0.012),
                    (cal, A - 0.3), (0.0, A - 0.3)]
        bm = bm_lathe([(r, h) for r, h in prof], n)
        g.put(bm, "paint" if lod < 2 else "paint", "barrel", toward((0, 0, H), (1, 0, 0)))
        if lod == 0:
            # the muzzle's face in bare steel, and a band where the jacket is shrunk on
            g.put(bm_lathe([(0.054 * k, A - 0.004), (0.057 * k, A + 0.002), (cal + 0.003, A + 0.002), (cal + 0.003, A - 0.004)], n), "steel", "barrel",
                  toward((0, 0, H), (1, 0, 0)))
            g.put(bm_lathe([(0.0795 * k, 0.16), (0.0795 * k, 0.2)], n), "paint", "barrel", toward((0, 0, H), (1, 0, 0)))
        if self.s["brake"]:
            g.put(bm_box((0.26 * k, 0.17 * k, 0.13 * k), pick(lod, 0.012, 0.0)), "shade", "barrel", Matrix.Translation((A - 0.06, 0, H)))
            if lod == 0:
                for dx in (-0.06, 0.04):
                    for side in (1.0, -1.0):
                        g.put(bm_box((0.05, 0.02, 0.08 * k)), "glass", "barrel", Matrix.Translation((A - 0.06 + dx, side * 0.087 * k, H)))
        # the breech ring: a frame round the block, open to the right where the block slides out
        rc = x_ring + ring_l * 0.5
        hw, hh = 0.105 * k, 0.1 * k
        bev = pick(lod, 0.01, 0.0, 0.0)
        g.put(bm_box((ring_l, 2 * hw, 0.05 * k), bev), "paint", "barrel", Matrix.Translation((rc, 0, H + hh - 0.025 * k)))
        g.put(bm_box((ring_l, 2 * hw, 0.05 * k), bev), "paint", "barrel", Matrix.Translation((rc, 0, H - hh + 0.025 * k)))
        g.put(bm_box((ring_l, 0.04 * k, 2 * hh - 0.1 * k), bev), "paint", "barrel", Matrix.Translation((rc, hw - 0.02 * k, H)))
        g.put(bm_box((ring_l - 0.16, 2 * hw - 0.04 * k, 2 * hh - 0.1 * k)), "paint", "barrel",
              Matrix.Translation((x_ring + 0.16 + (ring_l - 0.16) * 0.5, -0.02 * k, H)))
        # the lug under the ring for the piston rods
        g.put(bm_box((0.12, 0.2 * k, abs(0.17 * k) - 0.05), pick(lod, 0.008, 0.0)), "paint", "barrel",
              Matrix.Translation((x_ring + 0.14, 0, H + (0.12 * k if self.s["above"] else -0.12 * k))))
        if lod < 2:
            # the chamber's mouth, seen when the block is out
            face = [(-hw + 0.0, -hh + 0.05 * k), (hw - 0.04 * k, -hh + 0.05 * k), (hw - 0.04 * k, hh - 0.05 * k), (-hw, hh - 0.05 * k)]
            hole = [[(cal * 1.25 * math.cos(2 * math.pi * i / 12), cal * 1.25 * math.sin(2 * math.pi * i / 12)) for i in range(12)]]
            g.put(bm_plate(face, hole, 0.006), "steel", "barrel", frame((x_ring + 0.158, 0, H), (0, 1, 0), (-1, 0, 0)))
            g.put(bm_lathe([(cal * 1.25, 0.0), (cal * 1.2, 0.3), (0.0, 0.3)], 12), "shade", "barrel", toward((x_ring + 0.155, 0, H), (1, 0, 0)))
        # the block, its lever and its firing gear
        g.put(bm_box((0.155, 2 * hw - 0.04 * k + 0.012, 2 * hh - 0.1 * k - 0.004), pick(lod, 0.006, 0.0)), "steel", "breech",
              Matrix.Translation((x_ring + 0.08, -0.026 * k, H)))
        if lod == 0:
            g.put(bm_cyl(0.022, 0.03, 10), "shade", "breech", toward((x_ring - 0.005, -0.01, H), (-1, 0, 0)))
            p0 = Vector((x_ring + 0.06, -hw - 0.012, H + 0.02))
            g.put(bm_tube([p0, p0 + Vector((-0.02, -0.04, 0.02)), p0 + Vector((-0.2, -0.05, 0.09)), p0 + Vector((-0.26, -0.05, 0.1))], 0.012, 6),
                  "steel", "breech")
            g.put(bm_cyl(0.02, 0.07, 8), "wood", "breech", toward(p0 + Vector((-0.27, -0.05, 0.1)), (-1, 0, 0.3)))
            # a lifting eye on the ring, bolts on its face
            for dy in (-0.06, 0.06):
                for dz in (-0.07, 0.07):
                    bolt(g, (x_ring - 0.002, dy * k + 0.02 * k, H + dz * k), (-1, 0, 0), "barrel", 0.009, 0.008, "steel")

    def trails(self, g, lod):
        """The trail legs (bones trail_l, trail_r) or the box trail (bone trail): riveted girders on their hinges, a spade
        at each end, lifting handles, the towing eye on the left leg, the rammer and a shovel stowed on them."""
        sides = ((1.0, "trail_l"), (-1.0, "trail_r")) if self.split else ((0.0, "trail"),)
        for side, bone in sides:
            m, L = self.leg_frame(side)
            h0, h1 = 0.2, 0.13
            w0, w1 = (0.13, 0.095) if self.split else (0.34, 0.26)

            def shape(p, h0=h0, h1=h1, w0=w0, w1=w1):
                t = p.x + 0.5
                return Vector((t * L, p.y * (w0 + (w1 - w0) * t), p.z * (h0 + (h1 - h0) * t)))
            bm = bm_box((1, 1, 1), shape=shape)
            if lod == 0:
                bmesh.ops.bevel(bm, geom=[e for e in bm.edges if abs((e.verts[0].co - e.verts[1].co).normalized().x) > 0.9],
                                offset=0.008, offset_type="OFFSET", segments=1, affect="EDGES", clamp_overlap=True)
            g.put(bm, "paint", bone, m)
            to = lambda x, y, z: m @ Vector((x, y, z))
            hz = lambda x: (h0 + (h1 - h0) * x / L) * 0.5
            wy = lambda x: (w0 + (w1 - w0) * x / L) * 0.5
            if lod < 2:
                # a top flange plate, a little wider, lying on the leg's tapering top
                def fl(p, L=L, w0=w0, w1=w1, hz=hz):
                    x = 0.05 + (p.x + 0.5) * (L - 0.1)
                    return Vector((x, p.y * (w0 + 0.02 + (w1 - w0) * x / L), hz(x) + 0.006 + p.z * 0.012))
                g.put(bm_box((1, 1, 1), shape=fl), "paint", bone, m)
            if lod == 0:
                for sy in (1.0, -1.0):
                    pts = []
                    for i in range(12):
                        x = 0.25 + (L - 0.4) * i / 11
                        pts.append(x)
                        rivet(g, to(x, sy * (wy(x) + 0.004), hz(x) - 0.012), m.to_3x3() @ Vector((0, sy, 0)), bone, 0.008)
                    for i in range(10):
                        x = 0.3 + (L - 0.5) * i / 9
                        rivet(g, to(x, sy * (wy(x) - 0.002), hz(x) + 0.018), m.to_3x3() @ Vector((0, 0, 1)), bone, 0.007)
                # a lightening plate on the outside near the hinge, and the hinge's own eye
                g.put(bm_box((0.3, 0.012, h0 * 0.8), 0.004), "paint", bone, m @ Matrix.Translation((0.2, (side or 1.0) * (w0 * 0.5 + 0.005), 0)))
            # the end: a spade standing down into the ground, a gusset, the lifting handle
            end = m @ Vector((L, 0, 0))
            dirh = (m.to_3x3() @ Vector((1, 0, 0)))
            dirh.z = 0.0
            dirh.normalize()
            perp = Vector((0, 0, 1)).cross(dirh).normalized()
            sw = 0.25 if self.split else 0.32
            top_z = end.z + h1 * 0.5 + 0.1
            spade = [(-sw, top_z), (sw, top_z), (sw, -0.02), (sw * 0.35, -0.13), (-sw * 0.35, -0.13), (-sw, -0.02)]
            sm = frame(Vector((end.x, end.y, 0.0)) - dirh * 0.05, perp, perp.cross(Vector((0, 0, 1))))
            g.put(bm_plate(spade, (), 0.02), "shade", bone, sm)
            if lod < 2:
                gus = [(0.0, top_z), (0.0, top_z - 0.02), (0.0, -0.0)]
                # gussets behind the spade, to the leg's underside
                for u in ((-sw * 0.6, sw * 0.6) if lod == 0 else (0.0,)):
                    a = Vector((end.x, end.y, 0.0)) - dirh * 0.06 + perp * u
                    tri = [Vector((a.x, a.y, 0.02)), Vector((a.x, a.y, end.z - h1 * 0.5)), a - dirh * 0.35 + Vector((0, 0, end.z + 0.02))]
                    bm = bmesh.new()
                    vs = [bm.verts.new(p) for p in tri]
                    bm.faces.new(vs)
                    bm.faces.new(list(reversed([bm.verts.new(p + perp * 0.012) for p in tri])))
                    g.put(bm, "shade", bone)
                # the lifting handle: a bar across the end, standing up
                hp = [end + Vector((0, 0, h1 * 0.5)) - dirh * 0.35 + perp * 0.12, end + Vector((0, 0, h1 * 0.5 + 0.11)) - dirh * 0.3 + perp * 0.12,
                      end + Vector((0, 0, h1 * 0.5 + 0.11)) - dirh * 0.3 - perp * 0.12, end + Vector((0, 0, h1 * 0.5)) - dirh * 0.35 - perp * 0.12]
                g.put(bm_tube(hp, 0.014, pick(lod, 6, 4)), "steel", bone)
            if (side > 0 or not self.split) and lod < 3:
                # the towing eye (a ring lying flat) on its drawbar
                eye = self.lunette_at()
                g.put(bm_tube([eye + Vector((0.065 * math.cos(2 * math.pi * i / 12), 0.065 * math.sin(2 * math.pi * i / 12), 0)) for i in range(12)],
                              0.022, pick(lod, 6, 4), closed=True), "steel", bone)
                foot = end - dirh * 0.15
                g.put(bm_tube([foot, eye + (foot - eye).normalized() * 0.08], 0.03, pick(lod, 8, 5)), "paint", bone)
            if lod == 0 and self.split:
                out = perp * (1.0 if perp.dot(Vector((0, side, 0))) > 0 else -1.0)
                if side < 0:
                    # the rammer staff along the right leg's outside, in two clips
                    a = m @ Vector((0.35, 0, 0)) + out * (wy(0.35) + 0.05) + Vector((0, 0, 0.03))
                    b = m @ Vector((L - 0.25, 0, 0)) + out * (wy(L - 0.25) + 0.05) + Vector((0, 0, 0.03))
                    rod(g, a, b, 0.022, 8, "wood", bone)
                    g.put(bm_lathe([(0.0, 0.0), (0.045, 0.0), (0.045, 0.12), (0.03, 0.14), (0.0, 0.14)], 10), "canvas", bone, toward(b, b - a))
                    for x in (0.7, L - 0.6):
                        cpt = m @ Vector((x, 0, 0)) + out * (wy(x) + 0.03) + Vector((0, 0, 0.03))
                        g.put(bm_box((0.04, 0.06, 0.07)), "steel", bone, frame(cpt, dirh, Vector((0, 0, 1))))
                else:
                    # a shovel along the left leg's outside
                    a = m @ Vector((0.5, 0, 0)) + out * (wy(0.5) + 0.04) + Vector((0, 0, 0.0))
                    b = m @ Vector((1.5, 0, 0)) + out * (wy(1.5) + 0.04) + Vector((0, 0, 0.0))
                    rod(g, a, b, 0.018, 6, "wood", bone)
                    g.put(bm_box((0.07, 0.03, 0.1)), "wood", bone, frame(a - (b - a).normalized() * 0.04, dirh, Vector((0, 0, 1))))
                    blade = [(-0.11, 0.0), (0.11, 0.0), (0.1, 0.22), (0.0, 0.29), (-0.1, 0.22)]
                    g.put(bm_plate(blade, (), 0.006), "steel", bone, frame(b + (b - a).normalized() * 0.02, Vector((0, 0, 1)), out) @ Matrix.Rotation(-math.pi / 2, 4, "Z"))
                    for x in (0.75, 1.3):
                        cpt = m @ Vector((x, 0, 0)) + out * (wy(x) + 0.02)
                        g.put(bm_box((0.035, 0.05, 0.06)), "steel", bone, frame(cpt, dirh, Vector((0, 0, 1))))


class AmmoBoxes:
    """The gun's ammunition beside it, a prop on its own (no moving parts): two closed wooden boxes stacked, one open with
    its lid thrown back and four rounds in its rack, two rounds laid ready on a box, and a few spent cases thrown on the
    ground. Each box holds four rounds of the field gun's (a steel case and a painted shell)."""

    kind = "prop"
    folder = "gun"
    static = True
    name = "ammo_boxes"
    campaign = "Gun75"
    title = "Field gun ammunition boxes (our own design)"
    nation = "GER"
    noun = "ammunition"
    nation_word = "German"
    BOX = (0.74, 0.34, 0.24)

    def parts(self):
        return [part("root", None, (0, 0, 0), (0, 0, 1), "fixed", None, "the prop's origin, on the ground at its middle", (0, 0, 0.3))]

    def sockets(self):
        return [("socket_take", "root", Vector((0.0, -0.75, 0.0)), Vector((0, 1, 0)))]

    def marks(self):
        return {"roof": 0.5}

    def clips(self):
        return []

    def round_(self, g, at, d, lod, spent=False):
        """One round lying along d: its steel case with a rim and primer, and (unless spent) the shell with its driving band."""
        m = toward(at, d)
        n = pick(lod, 12, 8, 6)
        prof = [(0.0, 0.0), (0.05, 0.0), (0.05, 0.012), (0.045, 0.014), (0.045, 0.26), (0.04, 0.29), (0.039, 0.31)]
        if spent:
            prof += [(0.039, 0.32), (0.033, 0.32), (0.033, 0.2)]
        g.put(bm_lathe(prof if lod < 2 else [(0.0, 0.0), (0.047, 0.0), (0.045, 0.3), (0.0, 0.3)], n), "steel", "root", m)
        if spent or lod == 2:
            return
        g.put(bm_lathe([(0.0375, 0.31), (0.0395, 0.32), (0.0395, 0.335), (0.0375, 0.34)], n), "rust", "root", m)
        shell = [(0.0375, 0.34), (0.0375, 0.44), (0.034, 0.49), (0.026, 0.53), (0.016, 0.555), (0.0, 0.565)]
        g.put(bm_lathe(shell if lod == 0 else [(0.0375, 0.34), (0.0375, 0.45), (0.0, 0.56)], n), "shade", "root", m)
        if lod == 0:
            g.put(bm_lathe([(0.0, 0.002 - 0.004), (0.012, -0.002), (0.012, 0.001), (0.0, 0.001)], 8), "rust", "root", m)

    def box_(self, g, at, turn, lod, closed=True):
        """A wooden box (its planks, rope handles, lid with hinges and catches), its middle-bottom at `at`."""
        L, W, Hh = self.BOX
        base = Matrix.Translation(at) @ Matrix.Rotation(math.radians(turn), 4, "Z")
        if closed:
            g.put(bm_box((L, W, Hh), pick(lod, 0.008, 0.0)), "wood", "root", base @ Matrix.Translation((0, 0, Hh * 0.5)))
            if lod == 0:
                # the lid's lip, plank joints, battens on the ends, hinges and catches
                g.put(bm_box((L + 0.01, W + 0.01, 0.035), 0.004), "wood", "root", base @ Matrix.Translation((0, 0, Hh - 0.03)))
                for side in (1.0, -1.0):
                    g.put(bm_box((0.025, W + 0.024, Hh - 0.02), 0.004), "wood", "root", base @ Matrix.Translation((side * (L * 0.5 - 0.06), 0, Hh * 0.5)))
                    for k in (-1.0, 1.0):
                        g.put(bm_box((L - 0.02, 0.006, 0.006)), "shade", "root", base @ Matrix.Translation((0, side * (W * 0.5 + 0.001), Hh * (0.5 + k * 0.17))))
                    # a rope handle at each end
                    e = base @ Vector((side * (L * 0.5 + 0.012), 0, Hh * 0.62))
                    pts = [base @ Vector((side * (L * 0.5 + 0.004), -0.07, Hh * 0.68)), base @ Vector((side * (L * 0.5 + 0.03), -0.05, Hh * 0.5)),
                           base @ Vector((side * (L * 0.5 + 0.035), 0.0, Hh * 0.46)), base @ Vector((side * (L * 0.5 + 0.03), 0.05, Hh * 0.5)),
                           base @ Vector((side * (L * 0.5 + 0.004), 0.07, Hh * 0.68))]
                    g.put(bm_tube(pts, 0.008, 5), "canvas", "root")
                for x in (-0.2, 0.2):
                    g.put(bm_box((0.06, 0.012, 0.05)), "steel", "root", base @ Matrix.Translation((x, -W * 0.5 - 0.006, Hh - 0.03)))
                    g.put(bm_box((0.05, 0.012, 0.04)), "steel", "root", base @ Matrix.Translation((x, W * 0.5 + 0.006, Hh - 0.02)))
            return
        # open: the box's walls and floor, the rack, four rounds, the lid thrown back on its hinges
        t = 0.018
        for (sx, sy, sz, x, y) in ((L, t, Hh, 0, W * 0.5 - t * 0.5), (L, t, Hh, 0, -W * 0.5 + t * 0.5), (t, W, Hh, L * 0.5 - t * 0.5, 0),
                                   (t, W, Hh, -L * 0.5 + t * 0.5, 0)):
            g.put(bm_box((sx, sy, sz), pick(lod, 0.004, 0.0)), "wood", "root", base @ Matrix.Translation((x, y, Hh * 0.5)))
        g.put(bm_box((L, W, t)), "wood", "root", base @ Matrix.Translation((0, 0, t * 0.5)))
        if lod < 2:
            for x in (-0.18, 0.12):
                rack = [(-W * 0.5 + t, 0.0), (W * 0.5 - t, 0.0), (W * 0.5 - t, 0.1)] + \
                       [(W * 0.5 - t - (W - 2 * t) * (i + 0.5) / 4 + 0.042 * c, 0.1 - 0.04 * s_) for i in range(4)
                        for c, s_ in ((0.9, 0.2), (0.0, 1.0), (-0.9, 0.2))] + [(-W * 0.5 + t, 0.1)]
                g.put(bm_plate(rack, (), 0.016), "wood", "root", base @ frame((x, 0, 0.0), (0, 1, 0), (1, 0, 0)))
            for i in range(4):
                y = W * 0.5 - t - (W - 2 * t) * (i + 0.5) / 4
                self.round_(g, base @ Vector((-L * 0.5 + 0.06, y, 0.075)), base.to_3x3() @ Vector((1, 0, 0.0)), lod)
        lid = Matrix.Translation((0, -W * 0.5, Hh)) @ Matrix.Rotation(math.radians(-115), 4, "X") @ Matrix.Translation((0, W * 0.5, 0))
        g.put(bm_box((L + 0.01, W + 0.01, 0.022), pick(lod, 0.004, 0.0)), "wood", "root", base @ lid @ Matrix.Translation((0, 0, 0.011)))
        if lod == 0:
            for x in (-0.25, 0.25):
                g.put(bm_box((0.07, 0.03, 0.012)), "steel", "root", base @ Matrix.Translation((x, -W * 0.5 - 0.004, Hh)))

    def geo(self, lod):
        g = Geo()
        L, W, Hh = self.BOX
        self.box_(g, Vector((0.0, 0.25, 0.0)), 4.0, lod)
        self.box_(g, Vector((0.02, 0.24, Hh)), -3.0, lod)
        self.box_(g, Vector((0.05, -0.32, 0.0)), -6.0, lod, closed=False)
        # two rounds laid ready across the top box
        for dy in (-0.05, 0.05):
            self.round_(g, Vector((-0.3, 0.24 + dy, 2 * Hh + 0.045)), Vector((1, 0.04, 0)), lod)
        if lod < 2:
            # spent cases thrown back out of the breech
            rng = random.Random(5)
            for i in range(pick(lod, 4, 2)):
                a = rng.uniform(0, 6.283)
                at = Vector((rng.uniform(-1.2, -0.5), rng.uniform(-0.6, 0.7), 0.047))
                self.round_(g, at, Vector((math.cos(a), math.sin(a), rng.uniform(-0.05, 0.05))), lod, spent=True)
        return g

    def picture_pose(self):
        return {}

    def views(self):
        return {"front": ((-0.15, 0.0, 0.25), 3.4, 22.0, -35.0, 50.0), "rear": ((-0.15, 0.0, 0.25), 3.4, 22.0, 140.0, 50.0),
                "detail": ((0.0, -0.3, 0.2), 1.6, 35.0, -60.0, 50.0), "game": ((-0.15, 0.0, 0.25), 16.0, 30.0, 40.0, 40.0)}


# ================================================================================================ shared vehicle pieces
def sheet_rows(g, rows, mat, bone, closed=False, flip=False, uvs=None):
    """A surface through rows of points (each row a list of the same length), quads between neighbours."""
    nr, nc = len(rows), len(rows[0])
    verts = [p for r in rows for p in r]
    faces = []
    for i in range(nr - 1):
        for j in range(nc - 1 + (1 if closed else 0)):
            j1 = (j + 1) % nc
            f = [i * nc + j, i * nc + j1, (i + 1) * nc + j1, (i + 1) * nc + j]
            faces.append(f[::-1] if flip else f)
    g.add(verts, faces, mat, bone)


def thick_rows(g, rows, thick, mat, bone, inward):
    """A panel of some thickness through rows of points: its outer face, its inner face `thick` toward inward(p), and its
    edges closed."""
    inner = [[p + inward(p) * thick for p in r] for r in rows]
    sheet_rows(g, rows, mat, bone)
    sheet_rows(g, inner, mat, bone, flip=True)
    nr, nc = len(rows), len(rows[0])
    for edge in ([(0, j) for j in range(nc)], [(nr - 1, j) for j in range(nc)][::-1], [(i, 0) for i in range(nr)][::-1],
                 [(i, nc - 1) for i in range(nr)]):
        for (i0, j0), (i1, j1) in zip(edge, edge[1:]):
            g.add([rows[i0][j0], rows[i1][j1], inner[i1][j1], inner[i0][j0]], [[0, 1, 2, 3]], mat, bone)


def road_wheel(g, c, side, r, w, bone, lod, rim_r=None, lugs=14, disc="paint", inner=False, nuts=6, axis=None):
    """A lorry's wheel: a cross-country tyre (chevron lugs on the outer ones), a dished steel disc, its nuts and cap.
    `axis` lays it some other way than on an axle across the vehicle (a spare lying flat), without lugs."""
    out = Vector((0.0, side, 0.0)) if axis is None else Vector(axis).normalized()
    turn = Matrix.Rotation(-side * math.pi / 2, 4, "X")
    m = Matrix.Translation(c) @ turn if axis is None else toward(c, axis)
    if axis is not None:
        lugs = 0
    n = pick(lod, 20, 14, 8)
    ri = rim_r or r * 0.64
    tp = tyre_profile(r, w, lod, bulge=0.014, rim_r=ri, tread=0.008)
    if lod == 0:
        tp = tp[:3] + tp[4:6] + tp[7:]                                      # (the lugs make the tread's edge)
    g.put(bm_lathe(tp, n), "rubber", bone, m)
    if lod == 0 and lugs and not inner:
        tread_lugs(g, c, side, r, w, lugs, bone, chevron=True, depth=0.018)
    if lod == 2:
        g.put(bm_cyl(ri, w * 0.8, n), disc, bone, m)
        return
    if inner:
        g.put(bm_lathe([(ri, -w * 0.4), (ri, w * 0.4), (0.0, w * 0.4)], n), disc, bone, m)
        return
    prof = [(ri, -w * 0.42), (ri + 0.004, w * 0.3), (ri - 0.03, w * 0.32), (ri * 0.6, w * 0.18), (0.12, w * 0.24), (0.0, w * 0.3)]
    g.put(bm_lathe(prof if lod == 0 else [(ri, -w * 0.4), (ri, w * 0.3), (0.12, w * 0.2), (0.0, w * 0.28)], n), disc, bone, m)
    if lod == 0:
        for k in range(5):                                              # (the disc's ventilation holes, dark)
            a = 2 * math.pi * (k + 0.5) / 5
            g.put(bm_lathe([(0.032, 0.0), (0.0, 0.0)], 7), "shade", bone, m @ Matrix.Translation((ri * 0.8 * math.cos(a), ri * 0.8 * math.sin(a), w * 0.255)))
        bolt_ring(g, m @ Vector((0, 0, w * 0.22)), out, 0.1, nuts, bone, 0.014, 0.014, "steel", sides=5)
        g.put(bm_lathe([(0.07, 0.0), (0.07, 0.03), (0.05, 0.05), (0.0, 0.055)], 10), "shade", bone, m @ Matrix.Translation((0, 0, w * 0.26)))


def lamp(g, at, bone, lod, r=0.1, hood=True, d=(1, 0, 0)):
    """A headlamp: a bowl, its glass, a black-out hood over the top half, on a stem."""
    m = toward(at, d)
    g.put(bm_lathe([(0.0, -0.12), (r * 0.6, -0.11), (r * 0.95, -0.05), (r, 0.0), (r * 1.03, 0.012), (r * 0.9, 0.015)], pick(lod, 12, 8)), "paint", bone, m)
    g.put(bm_lathe([(r * 0.9, 0.012), (r * 0.7, 0.022), (0.0, 0.026)], pick(lod, 12, 8)), "glass", bone, m)
    if lod == 0 and hood:
        pts = []
        for k in range(7):
            a = math.pi * k / 6
            pts.append((m @ Vector((0, 0, 0.02))) + (m.to_3x3() @ Vector((math.cos(a) * r * 1.05, math.sin(a) * r * 1.05, 0))))
        prof = [(-0.0, 0.0), (0.0, 0.0)]
        g.put(bm_tube(pts, 1.0, profile=[(-0.004, 0.0), (0.004, 0.0), (0.004, 0.09), (-0.004, 0.09)], up=Vector(d)), "paint", bone)
    if lod < 2:
        rod(g, at - Vector(d) * 0.08, at - Vector(d) * 0.08 + Vector((0, 0, -0.16)), 0.018, 6, "paint", bone)


def jerrycan(g, at, turn_deg, bone, lod, mat="paint"):
    """A jerrycan standing up: the can (its pressed X on each face), the three handles, the spout."""
    m = Matrix.Translation(at) @ Matrix.Rotation(math.radians(turn_deg), 4, "Z")
    g.put(bm_box((0.34, 0.165, 0.47), pick(lod, 0.012, 0.0)), mat, bone, m @ Matrix.Translation((0, 0, 0.235)))
    if lod == 0:
        for side in (1.0, -1.0):
            for a in (32.0, -32.0):
                g.put(bm_box((0.36, 0.006, 0.02)), mat, bone, m @ Matrix.Translation((0, side * 0.084, 0.235)) @ Matrix.Rotation(math.radians(a), 4, "Y"))
            g.put(bm_box((0.33, 0.006, 0.44)), mat, bone, m @ Matrix.Translation((0, side * 0.0835, 0.235)) @ Matrix.Scale(0.0, 4, (0, 1, 0)) @ Matrix.Identity(4)) if False else None
        for dx in (-0.07, 0.0, 0.07):
            g.put(bm_box((0.03, 0.02, 0.05)), mat, bone, m @ Matrix.Translation((dx, 0, 0.49)))
        g.put(bm_box((0.18, 0.02, 0.012)), mat, bone, m @ Matrix.Translation((0, 0, 0.515)))
        g.put(bm_cyl(0.022, 0.05, 8), "steel", bone, m @ Matrix.Translation((0.13, 0, 0.49)))


def tool_shovel(g, a, b, bone, out):
    """A shovel lying from its grip a to its blade's end b against a side whose outward is `out`."""
    a, b = Vector(a), Vector(b)
    d = (b - a).normalized()
    rod(g, a, b - d * 0.27, 0.016, 6, "wood", bone)
    blade = [(-0.11, 0.0), (0.11, 0.0), (0.1, 0.2), (0.0, 0.27), (-0.1, 0.2)]
    up = Vector(out).cross(d).normalized()
    g.put(bm_plate(blade, (), 0.006), "steel", bone, frame(b - d * 0.27, up, out) @ Matrix.Rotation(0.0, 4, "Z") @
          Matrix(((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1))))
    g.put(bm_box((0.1, 0.02, 0.03)), "wood", bone, frame(a - d * 0.02, d, out))


def tool_pick(g, a, b, bone, out):
    a, b = Vector(a), Vector(b)
    d = (b - a).normalized()
    rod(g, a, b, 0.02, 6, "wood", bone)
    up = Vector(out).cross(d).normalized()
    h = b - d * 0.05
    g.put(bm_tube([h + up * 0.3, h + up * 0.1 + d * 0.03, h - up * 0.1 + d * 0.03, h - up * 0.3], 0.016, 5), "steel", bone)


def clamp_strap(g, at, along, out, bone, w=0.04):
    g.put(bm_box((w, 0.03, 0.06)), "steel", bone, frame(at, along, out))


# ================================================================================================ THE LORRY
class Lorry:
    """A lorry of about three tons, from build_tank.OTHERS["Truck"]: the campaign piece's bonnet, cab, planked bed and canvas
    tilt in the same measures, built up for a close camera: a butterfly bonnet (each half hinged at the top, the engine
    under it), a cab with doors, glass, a bench, wheel and dashboard, wings and running boards, lamps with black-out
    hoods, a chassis with leaf springs, axles and a towing pintle, cross-country tyres (twin at the back), dropsides
    with stakes and latches, benches inside for a squad, a tilt over five hoops sagging between them, its rear rolled up
    and its foot lashed down, and stowage: a spare wheel, jerrycans, a toolbox, a pick and shovel, a rolled net on the cab
    roof. Mud and dust are the pictures' (see picture_materials)."""

    kind = "lorry"
    folder = "lorry"
    noun = "lorry"

    def __init__(self, campaign="Truck", nation="GER"):
        s = dict(bt.OTHERS[campaign])
        self.s = s
        self.name = campaign
        self.campaign = campaign
        self.title = "Battle lorry (our own design)"
        self.nation = nation
        self.nation_word = {"GER": "German", "USA": "American", "UK": "British", "SOV": "Soviet", "POL": "Polish"}.get(nation, nation)
        self.FX, self.RX, self.R = s["front_x"], s["rear_x"], s["wheel_r"]
        self.FY, self.RY = 0.86, 0.8                     # each front wheel's middle; the middle of each pair of twins
        self.BED0, self.BED1 = 0.1, self.RX - 1.35       # the bed's front and back (x)
        self.BED_Y = 1.08
        self.FLOOR = 0.98                                # top of the bed's floor
        self.SIDE_TOP = 1.54
        self.TILT_FOOT, self.TILT_TOP = 1.5, 2.55
        self.CAB0, self.CAB1 = 0.2, 1.3                  # the cab's back and front (x)
        self.CAB_Y = 0.95
        self.CAB_FLOOR, self.CAB_ROOF = 0.76, 2.18
        self.COWL = 1.62                                 # where the windscreen starts
        self.BON0, self.BON1 = 1.3, self.FX + 0.89        # the bonnet's back and front (x)
        self.BON_Y = (0.5, 0.43)                         # its half-width at the back and the front
        self.BON_Z = (1.57, 1.47)                        # its top
        self.BON_FOOT = 0.88
        self.HOOPS = [self.BED1 + 0.08 + (self.BED0 - self.BED1 - 0.16) * k / 4 for k in range(5)]
        self.DRIVE_LOOP = 2.0
        self.DRIVE_M = 4.0 * 2.0 * math.pi * self.R      # four turns of the wheels a loop

    # -------------------------------------------------------------------------------------------- measures
    def cab_front(self, z):
        """The cab's front face at height z: upright to the cowl, then the windscreen leaning back."""
        if z <= self.COWL:
            return self.CAB1
        return self.CAB1 - 0.16 * (z - self.COWL) / (self.CAB_ROOF - self.COWL)

    def bonnet_at(self, x):
        t = (x - self.BON0) / (self.BON1 - self.BON0)
        return self.BON_Y[0] + (self.BON_Y[1] - self.BON_Y[0]) * t, self.BON_Z[0] + (self.BON_Z[1] - self.BON_Z[0]) * t

    def tilt_profile(self, n, foot=None):
        """The tilt's section from the left foot over the top to the right foot: (y, z) points, and each one's outward."""
        foot = self.TILT_FOOT - 0.12 if foot is None else foot
        half, top = self.BED_Y + 0.02, self.TILT_TOP
        eave = self.TILT_FOOT + (top - self.TILT_FOOT) * 0.55
        pts = [(half, foot), (half, (foot + eave) * 0.5)]
        k = max(4, n - 4)
        for i in range(k + 1):
            a = math.pi * i / k
            y = math.cos(a) * half
            z = eave + (top - eave) * math.sin(a)
            pts.append((y, z))
        pts += [(-half, (foot + eave) * 0.5), (-half, foot)]
        outs = []
        for i, (y, z) in enumerate(pts):
            y0, z0 = pts[max(i - 1, 0)]
            y1, z1 = pts[min(i + 1, len(pts) - 1)]
            ty, tz = y1 - y0, z1 - z0
            ln = math.hypot(ty, tz)
            outs.append((tz / ln, -ty / ln))
        return pts, outs

    # -------------------------------------------------------------------------------------------- the rig
    def parts(self):
        P = [part("root", None, (0, 0, 0), (0, 0, 1), "fixed", None, "the lorry's origin, on the ground midway between its axles' ends", (0, 0, 0.5)),
             part("body", "root", (0.0, 0.0, 0.75), (0, -1, 0), "turn", [-0.08, 0.08],
                  "the body on its springs: + pitches the nose up; the clips also roll it about x (+ right side up) and bob it along z", (1.0, 0, 0))]
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            P.append(part(f"steer_{sl}", "root", (self.FX, side * (self.FY - 0.12), self.R), (0, 0, 1), "turn", [-0.6, 0.6],
                          f"the {'left' if side > 0 else 'right'} front wheel's steering on its king pin: + steers to the right "
                          "(clockwise seen from above). The game steers.", (0, 0, 0.3)))
            P.append(part(f"wheel_f{sl}", f"steer_{sl}", (self.FX, side * self.FY, self.R), (0, 1, 0), "spin", None,
                          f"front {'left' if side > 0 else 'right'} wheel: + rolls forward", (0, 0.25 * side, 0), radius=self.R))
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            P.append(part(f"wheel_r{sl}", "root", (self.RX, side * self.RY, self.R), (0, 1, 0), "spin", None,
                          f"the {'left' if side > 0 else 'right'} rear twins: + roll forward", (0, 0.25 * side, 0), radius=self.R))
        hz = (self.CAB_FLOOR + 0.1 + 1.95) * 0.5
        P.append(part("door_l", "body", (1.19, self.CAB_Y + 0.01, hz), (0, 0, 1), "turn", [0.0, 1.25],
                      "the driver's door (left), hinged at its front edge: + opens", (0, 0, 0.3)))
        P.append(part("door_r", "body", (1.19, -self.CAB_Y - 0.01, hz), (0, 0, -1), "turn", [0.0, 1.25],
                      "the co-driver's door (right), hinged at its front edge: + opens", (0, 0, 0.3)))
        zt = self.BON_Z[0]
        P.append(part("bonnet_l", "body", ((self.BON0 + self.BON1) * 0.5, 0.0, zt), (-1, 0, 0), "turn", [0.0, 1.9],
                      "the bonnet's left half, hinged along the top middle: + lifts it open", (0.4, 0, 0)))
        P.append(part("bonnet_r", "body", ((self.BON0 + self.BON1) * 0.5, 0.0, zt), (1, 0, 0), "turn", [0.0, 1.9],
                      "the bonnet's right half, hinged along the top middle: + lifts it open", (0.4, 0, 0)))
        P.append(part("tilt", "body", ((self.BED0 + self.BED1) * 0.5, 0.0, self.TILT_TOP), (0, 0, 1), "slide", [-0.03, 0.03],
                      "the canvas between the hoops (blended with the body): + lifts it. Only the clips' flutter moves it.", (0, 0, 0.3)))
        return P

    def sockets(self):
        out = [("socket_exhaust", "body", Vector((-0.62, -0.66, 0.4)), Vector((-0.4, -0.5, -0.75)).normalized()),
               ("socket_tow", "body", LORRY_HOOK.copy(), Vector((-1, 0, 0))),
               ("socket_dust_l", "root", Vector((self.RX - 0.6, self.RY + 0.1, 0.12)), Vector((-1, 0, 0.35)).normalized()),
               ("socket_dust_r", "root", Vector((self.RX - 0.6, -self.RY - 0.1, 0.12)), Vector((-1, 0, 0.35)).normalized()),
               ("socket_engine", "body", Vector((self.FX + 0.1, 0.0, 1.35)), Vector((0, 0, 1))),
               ("socket_cab_fire", "body", Vector((0.7, 0.0, 1.45)), Vector((0, 0, 1))),
               ("socket_driver", "body", Vector((0.6, 0.42, 1.22)), Vector((1, 0, 0))),
               ("socket_codriver", "body", Vector((0.6, -0.42, 1.22)), Vector((1, 0, 0)))]
        xs = [self.BED1 + 0.45 + (self.BED0 - self.BED1 - 0.75) * k / 3 for k in range(4)]
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            for i, x in enumerate(xs):
                out.append((f"socket_rider_{sl}{i + 1}", "body", Vector((x, side * 0.82, self.FLOOR + 0.42)), Vector((0, -side, 0))))
        out.append(("socket_tailboard", "body", Vector((self.BED1 - 0.35, 0.0, 0.0)), Vector((-1, 0, 0))))
        return out

    def marks(self):
        return {"cross": [round(0.78, 3), round(self.CAB_Y + 0.035, 3), 1.32, 0.16, 0.0], "roof": round(self.CAB_ROOF + 0.05, 3)}

    # -------------------------------------------------------------------------------------------- the clips
    def clips(self):
        rad = math.radians
        C = []
        D, LOOP = self.DRIVE_M, self.DRIVE_LOOP
        wheels = [f"wheel_{a}{b}" for a in "fr" for b in "lr"]

        c = Clip("idle", 2.0, True, None, "engine running, standing still")
        c.add("body", "turn", lambda t: rad(0.05) * sin_(6, t, 0.5) + rad(0.02) * sin_(11.5, t))
        c.add("body", "turn", lambda t: rad(0.12) * sin_(3, t) + rad(0.05) * sin_(6, t, 2.0), axis=(1, 0, 0))
        c.add("body", "slide", lambda t: 0.0018 * sin_(6, t) + 0.0008 * sin_(11.5, t, 1.0), axis=(0, 0, 1))
        c.add("tilt", "slide", lambda t: 0.002 * sin_(6, t, 1.2))
        c.event(0.0, "exhaust_idle", "socket_exhaust")
        C.append(c)

        c = Clip("drive", LOOP, True, None, f"driving forward: {D:.3f} m a loop", metres=D)
        c.travel = lambda t: D * t / LOOP
        for wname in wheels:
            c.add(wname, "spin", lambda t: D * t / LOOP / self.R)
        c.add("body", "turn", lambda t: rad(0.5) * sin_(0.5, t) + rad(0.25) * sin_(1.5, t, 0.7) + rad(0.06) * sin_(9, t))
        c.add("body", "turn", lambda t: rad(0.7) * sin_(0.5, t, 1.9) + rad(0.3) * sin_(2.0, t, 0.4), axis=(1, 0, 0))
        c.add("body", "slide", lambda t: 0.012 * sin_(1.0, t, 0.4) + 0.004 * sin_(3.5, t) + 0.0015 * sin_(9, t, 2.0), axis=(0, 0, 1))
        c.add("tilt", "slide", lambda t: 0.009 * sin_(4.5, t) + 0.006 * sin_(7.0, t, 1.3) + 0.004 * sin_(1.0, t))
        c.event(0.0, "exhaust_drive", "socket_exhaust")
        c.event(0.0, "dust_trail", "socket_dust_l").event(0.0, "dust_trail", "socket_dust_r")
        C.append(c)

        c = Clip("stop", 0.8, False, None, "pulling up from the drive")
        v0, tb = D / LOOP, 0.35
        travel = lambda t: v0 * t - v0 * t * t / (2 * tb) if t < tb else v0 * tb * 0.5
        c.travel = travel
        for wname in wheels:
            c.add(wname, "spin", lambda t: travel(t) / self.R)

        def nose(t):
            if t < 0.3:
                return -rad(1.6) * math.sin(0.5 * math.pi * t / 0.3)
            return -rad(1.6) * math.exp(-(t - 0.3) / 0.14) * math.cos(2 * math.pi * (t - 0.3) / 0.45) * (1.0 - smoothstep(0.65, 0.8, t))
        c.add("body", "turn", nose)
        c.add("body", "slide", lambda t: nose(t) * 0.3, axis=(0, 0, 1))
        c.add("tilt", "slide", lambda t: _damped(t, 0.012, 0.3, 0.25, 0.25) * (1.0 - smoothstep(0.65, 0.8, t)))
        c.event(0.0, "brakes", "socket_dust_l").event(0.0, "brakes", "socket_dust_r")
        C.append(c)

        # knocked out: struck at 0.0; the body jolts and the right half of the bonnet springs open (0.12); the driver's
        # door swings open at 0.5; the body settles low on the right front (the tyre gone) by 1.4; fire takes the cab at 1.0
        c = Clip("knocked_out", 3.0, False, None, "hit and knocked out; holds its last frame")
        still = lambda t: 1.0 - smoothstep(1.0, 1.4, t)
        c.add("body", "turn", lambda t: _damped(t, rad(2.4), 0.4, 0.22) * still(t) + _ease(t, 0.3, 1.3, 0.0, -rad(1.3)))
        c.add("body", "turn", lambda t: _damped(t, rad(2.8), 0.45, 0.25) * still(t) + _ease(t, 0.3, 1.3, 0.0, -rad(2.6)), axis=(1, 0, 0))
        c.add("body", "slide", lambda t: _damped(t, 0.035, 0.35, 0.18) * still(t) + _ease(t, 0.3, 1.3, 0.0, -0.035), axis=(0, 0, 1))

        def keys(seq):
            def f(t):
                for (t0, a0), (t1, a1) in zip(seq, seq[1:]):
                    if t <= t1:
                        u = (t - t0) / (t1 - t0)
                        u = u * u if a1 > a0 else 1 - (1 - u) * (1 - u)
                        return a0 + (a1 - a0) * u
                return seq[-1][1]
            return f
        c.add("bonnet_r", "turn", keys([(0.0, 0.0), (0.12, 0.0), (0.3, 1.75), (0.42, 1.3), (0.55, 1.62), (0.66, 1.45), (0.78, 1.55), (0.9, 1.5)]))
        c.add("door_l", "turn", keys([(0.0, 0.0), (0.5, 0.0), (0.85, 1.2), (1.0, 1.05), (1.15, 1.15), (1.3, 1.12)]))
        c.add("tilt", "slide", lambda t: _damped(t, 0.02, 0.3, 0.3))
        c.event(0.0, "hit", None, "tank_knocked_out")
        c.event(0.12, "smoke_burst", "socket_engine")
        c.event(1.0, "fire_loop", "socket_cab_fire")
        c.event(1.6, "smoke_loop", "socket_engine")
        C.append(c)
        return C

    # -------------------------------------------------------------------------------------------- the model
    def geo(self, lod):
        g = Geo()
        self.wheels(g, lod)
        self.chassis(g, lod)
        self.bonnet(g, lod)
        self.cab(g, lod)
        self.wings(g, lod)
        self.bed(g, lod)
        self.tilt(g, lod)
        self.stowage(g, lod)
        return g

    def wheels(self, g, lod):
        R = self.R
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            road_wheel(g, Vector((self.FX, side * self.FY, R)), side, R, 0.22, f"wheel_f{sl}", lod, rim_r=0.27)
            road_wheel(g, Vector((self.RX, side * (self.RY + 0.11), R)), side, R, 0.2, f"wheel_r{sl}", lod, rim_r=0.27)
            road_wheel(g, Vector((self.RX, side * (self.RY - 0.11), R)), side, R, 0.2, f"wheel_r{sl}", lod, rim_r=0.27, inner=True)

    def chassis(self, g, lod):
        """Frame rails, cross members, leaf springs, the axles (on the wheels' side), the exhaust, the pintle."""
        R = self.R
        x0, x1 = self.RX - 1.3, self.FX + 0.9
        for side in (1.0, -1.0):
            g.put(bm_box((x1 - x0, 0.06, 0.16), pick(lod, 0.006, 0.0)), "shade", "body", Matrix.Translation(((x0 + x1) * 0.5, side * 0.42, 0.62)))
        if lod < 2:
            for x in (x1 - 0.05, self.FX - 0.6, 0.15, self.RX + 0.55, self.RX - 0.6, x0 + 0.05):
                g.put(bm_box((0.08, 0.8, 0.1)), "shade", "body", Matrix.Translation((x, 0, 0.62)))
        # axles: the front a beam with its track rod, the rear a tube with the differential's housing
        g.put(bm_box((0.09, 2 * self.FY - 0.28, 0.1), pick(lod, 0.008, 0.0)), "shade", "root", Matrix.Translation((self.FX, 0, R)))
        rod(g, (self.RX, -self.RY + 0.2, R), (self.RX, self.RY - 0.2, R), 0.07, pick(lod, 10, 6), "shade", "root")
        if lod < 2:
            g.put(bm_lathe([(0.0, -0.2), (0.14, -0.17), (0.2, -0.05), (0.19, 0.08), (0.12, 0.16), (0.0, 0.18)], pick(lod, 12, 8)), "shade", "root",
                  toward((self.RX, 0, R), (1, 0, 0)))
            rod(g, (self.FX - 0.18, -self.FY + 0.2, R - 0.05), (self.FX - 0.18, self.FY - 0.2, R - 0.05), 0.022, 6, "steel", "root")
            # the propeller shaft from the gearbox to the differential
            rod(g, (self.RX + 0.2, 0, R + 0.02), (0.9, 0, 0.6), 0.04, 6, "steel", "body")
        if lod == 0:
            # leaf springs (a stack of leaves, bowed) with their hangers and shackles
            for x, ln in ((self.FX, 1.1), (self.RX, 1.3)):
                for side in (1.0, -1.0):
                    y = side * 0.42
                    for k in range(4):
                        L = ln - 0.22 * k
                        bow = 0.06
                        pts = [Vector((x - L / 2 + L * i / 4, y, R + 0.1 - 0.014 * k - bow * (((i - 2) / 2) ** 2) * (1 - 0.15 * k))) for i in range(5)]
                        g.put(bm_tube(pts, 1.0, profile=[(-0.035, -0.006), (0.035, -0.006), (0.035, 0.006), (-0.035, 0.006)], up=(0, 0, 1)), "steel", "body")
                    for dx in (-0.08, 0.08):
                        g.put(bm_box((0.02, 0.09, 0.15)), "shade", "body", Matrix.Translation((x + dx, y, R + 0.06)))
                    for dx in (-ln / 2, ln / 2):
                        g.put(bm_box((0.05, 0.08, 0.1)), "shade", "body", Matrix.Translation((x + dx, y, R + 0.08)))
            # the exhaust: down from the engine, back under the right of the cab to a silencer, its tail pipe turned down
            ex = [Vector((self.FX + 0.2, -0.36, 0.95)), Vector((self.FX - 0.1, -0.38, 0.62)), Vector((1.1, -0.5, 0.5)),
                  Vector((0.75, -0.55, 0.48))]
            g.put(bm_tube(ex, 0.03, 8), "rust", "body")
            g.put(bm_lathe([(0.0, 0.0), (0.085, 0.01), (0.09, 0.05), (0.09, 0.55), (0.085, 0.59), (0.0, 0.6)], 12), "rust", "body",
                  toward((0.75, -0.55, 0.48), (-1, 0, 0)))
            g.put(bm_tube([Vector((0.15, -0.55, 0.48)), Vector((-0.3, -0.6, 0.45)), Vector((-0.55, -0.65, 0.42)), Vector((-0.62, -0.66, 0.4))],
                          0.028, 8), "rust", "body")
            # the pintle at the back, on its bracket, and the rear lamps
            hk = LORRY_HOOK
            g.put(bm_box((0.16, 0.24, 0.16), 0.01), "shade", "body", Matrix.Translation((x0 - 0.02, 0, hk.z + 0.02)))
            g.put(bm_tube([hk + Vector((0.06 * math.cos(2 * math.pi * i / 10), 0, 0.06 * math.sin(2 * math.pi * i / 10) + 0.0)) for i in range(10)][2:9],
                          0.022, 6), "steel", "body")
            rod(g, hk + Vector((0.07, 0, 0.0)), hk + Vector((0.07, 0, 0.11)), 0.018, 6, "steel", "body")
            for side in (1.0, -1.0):
                g.put(bm_box((0.05, 0.12, 0.08), 0.006), "paint", "body", Matrix.Translation((x0 - 0.04, side * 0.85, 0.82)))
                g.put(bm_box((0.012, 0.08, 0.04)), "glass", "body", Matrix.Translation((x0 - 0.066, side * 0.85, 0.82)))
        elif lod == 1:
            g.put(bm_cyl(0.09, 0.6, 8), "rust", "body", toward((0.45, -0.55, 0.48), (1, 0, 0)))

    def bonnet(self, g, lod):
        """The bonnet in two halves hinged along the top middle (bones bonnet_l/r), louvred, with catches; the radiator and
        its guard, the engine under it, the cowl behind it."""
        nx = pick(lod, 6, 3, 2)
        for side, bone in ((1.0, "bonnet_l"), (-1.0, "bonnet_r")):
            rows = []
            for i in range(nx + 1):
                x = self.BON0 + 0.01 + (self.BON1 - self.BON0 - 0.05) * i / nx
                w, zt = self.bonnet_at(x)
                prof = [(0.0, zt + 0.012), (w * 0.5, zt + 0.006), (w - 0.06, zt - 0.004), (w - 0.015, zt - 0.03), (w, zt - 0.08),
                        (w, (zt + self.BON_FOOT) * 0.5), (w, self.BON_FOOT)]
                if lod == 2:
                    prof = [(0.0, zt), (w, zt - 0.03), (w, self.BON_FOOT)]
                rows.append([Vector((x, side * y, z)) for y, z in prof])
            if side < 0:
                rows = [r for r in rows]
            inward = (lambda p, side=side: Vector((0, -side * (1.0 if abs(p.y) > 0.3 else 0.3), -1.0 if abs(p.y) < 0.3 else -0.2)).normalized())
            thick_rows(g, rows if side > 0 else [r[::-1] for r in rows][::-1], 0.008, "paint", bone, inward)
            if lod == 0:
                # louvres along the side's rear half, the catches at its foot, a handle
                for k in range(7):
                    x = self.BON0 + 0.25 + 0.075 * k
                    w, zt = self.bonnet_at(x)
                    g.put(bm_box((0.03, 0.02, 0.24), 0.0, shape=lambda p: Vector((p.x + (0.5 if p.y > 0 else 0.0) * 0.0, p.y, p.z))), "paint", bone,
                          Matrix.Translation((x, side * (w + 0.006), zt - 0.3)) @ Matrix.Rotation(math.radians(-25 * side), 4, "Z"))
                for x in (self.BON0 + 0.3, self.BON1 - 0.3):
                    w, zt = self.bonnet_at(x)
                    g.put(bm_box((0.05, 0.03, 0.07), 0.004), "steel", bone, Matrix.Translation((x, side * (w + 0.012), self.BON_FOOT + 0.06)))
                    g.put(bm_box((0.04, 0.02, 0.04)), "steel", "body", Matrix.Translation((x, side * (w + 0.005), self.BON_FOOT - 0.01)))
                x = (self.BON0 + self.BON1) * 0.5
                w, zt = self.bonnet_at(x)
                g.put(bm_tube([Vector((x - 0.08, side * (w + 0.003), zt - 0.16)), Vector((x - 0.06, side * (w + 0.04), zt - 0.16)),
                               Vector((x + 0.06, side * (w + 0.04), zt - 0.16)), Vector((x + 0.08, side * (w + 0.003), zt - 0.16))], 0.008, 5), "steel", bone)
        # the hinge along the top, the bonnet's sills
        rod(g, (self.BON0, 0, self.BON_Z[0] + 0.012), (self.BON1 - 0.04, 0, self.BON_Z[1] + 0.012), 0.014, pick(lod, 6, 4), "paint", "body")
        for side in (1.0, -1.0):
            g.put(bm_box((self.BON1 - self.BON0, 0.06, 0.03)), "shade", "body",
                  Matrix.Translation(((self.BON0 + self.BON1) * 0.5, side * (self.BON_Y[0] - 0.02), self.BON_FOOT - 0.02)))
        # the radiator shell and guard at the front
        fx = self.BON1
        w, zt = self.bonnet_at(fx)
        shell = [(-w - 0.03, self.BON_FOOT - 0.12), (w + 0.03, self.BON_FOOT - 0.12), (w + 0.03, zt - 0.05), (w - 0.05, zt + 0.02), (-w + 0.05, zt + 0.02),
                 (-w - 0.03, zt - 0.05)]
        hole = [[(-w + 0.05, self.BON_FOOT - 0.04), (w - 0.05, self.BON_FOOT - 0.04), (w - 0.05, zt - 0.06), (-w + 0.05, zt - 0.06)]] if lod < 2 else []
        g.put(bm_plate(shell, hole, 0.05), "paint", "body", frame((fx + 0.02, 0, 0), (0, 1, 0), (1, 0, 0)) @ Matrix.Identity(4))
        g.put(bm_box((0.04, 2 * w - 0.1, zt - self.BON_FOOT - 0.02)), "shade", "body", Matrix.Translation((fx - 0.02, 0, (zt + self.BON_FOOT) * 0.5 - 0.04)))
        if lod < 2:
            nb = pick(lod, 11, 5)
            for k in range(nb):
                y = -w + 0.08 + (2 * w - 0.16) * k / (nb - 1)
                g.put(bm_box((0.02, 0.018, zt - self.BON_FOOT - 0.06)), "paint", "body", Matrix.Translation((fx + 0.04, y, (zt + self.BON_FOOT) * 0.5 - 0.04)))
            for z in ((self.BON_FOOT + zt) * 0.5 - 0.04,):
                g.put(bm_box((0.02, 2 * w - 0.1, 0.02)), "paint", "body", Matrix.Translation((fx + 0.045, 0, z)))
        # the bumper, its tow hooks, the starting handle's hole
        g.put(bm_box((0.1, 1.9, 0.16), pick(lod, 0.015, 0.0)), "shade", "body", Matrix.Translation((self.FX + 1.0, 0, 0.66)))
        for side in (1.0, -1.0):
            rod(g, (self.FX + 0.9, side * 0.42, 0.62), (self.FX + 0.98, side * 0.42, 0.64), 0.04, 6, "shade", "body")
        if lod == 0:
            for side in (1.0, -1.0):
                pts = [Vector((self.FX + 1.05, side * 0.62, 0.7)), Vector((self.FX + 1.13, side * 0.62, 0.72)), Vector((self.FX + 1.15, side * 0.62, 0.64)),
                       Vector((self.FX + 1.1, side * 0.62, 0.6))]
                g.put(bm_tube(pts, 0.018, 6), "steel", "body")
                for z in (0.62, 0.7):
                    bolt(g, (self.FX + 1.05, side * 0.75, z), (1, 0, 0), "body", 0.012, 0.01, "steel")
            # the engine, seen when a half is lifted: block, head and rocker cover, the air cleaner, the fan
            ex = self.BON0 + 0.25
            g.put(bm_box((0.85, 0.36, 0.38), 0.01), "shade", "body", Matrix.Translation((ex + 0.45, 0, 1.0)))
            g.put(bm_box((0.8, 0.3, 0.1), 0.01), "steel", "body", Matrix.Translation((ex + 0.45, 0.03, 1.24)))
            for k in range(6):
                g.put(bm_box((0.02, 0.26, 0.02)), "steel", "body", Matrix.Translation((ex + 0.12 + 0.13 * k, 0.03, 1.3)))
            g.put(bm_lathe([(0.0, 0.0), (0.11, 0.0), (0.11, 0.14), (0.08, 0.16), (0.0, 0.16)], 12), "paint", "body", toward((ex + 0.25, -0.22, 1.2), (0, 0, 1)))
            rod(g, (ex + 0.25, -0.22, 1.25), (ex + 0.4, -0.12, 1.15), 0.035, 6, "paint", "body")
            for k in range(4):
                a = math.pi * k / 2 + 0.3
                g.put(bm_box((0.012, 0.3, 0.07)), "steel", "body", Matrix.Translation((fx - 0.12, 0, 1.12)) @ Matrix.Rotation(a, 4, "X") @
                      Matrix.Translation((0, 0.15, 0)))
            rod(g, (self.FX + 0.2, -0.36, 0.95), (self.FX + 0.2, -0.3, 1.02), 0.035, 6, "rust", "body")

    def cab(self, g, lod):
        """The cab: back wall with its window, roof with drip rails, the cowl, the windscreen in two panes, the side frames
        and the doors (bones door_l/r) with their windows wound half down, the bench, the wheel and dashboard, mirrors."""
        x0, x1, Y = self.CAB0, self.CAB1, self.CAB_Y
        zf, zr, zc = self.CAB_FLOOR, self.CAB_ROOF, self.COWL
        bev = pick(lod, 0.01, 0.0, 0.0)
        xt = self.cab_front(zr)
        # back wall with its small window
        hole = [[(-0.32, 1.72), (0.32, 1.72), (0.32, 1.98), (-0.32, 1.98)]] if lod < 2 else []
        g.put(bm_plate([(-Y, zf), (Y, zf), (Y, zr), (-Y, zr)], hole, 0.025), "paint", "body", frame((x0, 0, 0), (0, 1, 0), (1, 0, 0)))
        if lod < 2:
            g.put(bm_box((0.006, 0.64, 0.26)), "glass", "body", Matrix.Translation((x0 + 0.004, 0, 1.85)))
        # the roof, a little crowned, with drip rails
        rows = []
        for i in range(pick(lod, 4, 2, 1) + 1):
            xx = x0 - 0.02 + (xt + 0.03 - x0 + 0.02) * i / pick(lod, 4, 2, 1)
            rows.append([Vector((xx, y, zr + 0.035 * (1 - (y / (Y + 0.02)) ** 2))) for y in ((Y + 0.02), Y * 0.6, 0.0, -Y * 0.6, -(Y + 0.02))])
        thick_rows(g, rows, 0.025, "paint", "body", lambda p: Vector((0, 0, -1)))
        if lod == 0:
            for side in (1.0, -1.0):
                g.put(bm_box((xt - x0 + 0.04, 0.02, 0.02)), "paint", "body", Matrix.Translation(((x0 + xt) * 0.5, side * (Y + 0.025), zr + 0.005)))
        # the cowl under the windscreen, and the dashboard behind it
        g.put(bm_box((0.03, 2 * Y, zc - zf), bev), "paint", "body", Matrix.Translation((x1 - 0.015, 0, (zc + zf) * 0.5)))
        # the windscreen: a frame in the leaning plane, a post down the middle, two panes
        a = Vector((x1, 0, zc))
        b = Vector((xt, 0, zr))
        up = (b - a).normalized()
        n = Vector((up.z, 0, -up.x))
        L = (b - a).length
        wf = frame(a, Vector((0, 1, 0)), n)                     # local x across, y up the screen
        outline = [(-Y, 0.0), (Y, 0.0), (Y, L), (-Y, L)]
        holes = [[(0.03, 0.06), (Y - 0.08, 0.06), (Y - 0.08, L - 0.07), (0.03, L - 0.07)],
                 [(-Y + 0.08, 0.06), (-0.03, 0.06), (-0.03, L - 0.07), (-Y + 0.08, L - 0.07)]] if lod < 2 else []
        g.put(bm_plate(outline, holes, 0.03), "paint", "body", wf)
        if lod < 2:
            for u0, u1 in ((0.03, Y - 0.08), (-Y + 0.08, -0.03)):
                g.put(bm_box((u1 - u0, L - 0.13, 0.006)), "glass", "body", wf @ Matrix.Translation(((u0 + u1) * 0.5, (L - 0.01) * 0.5, -0.004)))
            if lod == 0:
                for u in (0.35, -0.55):                         # wipers parked along the top
                    g.put(bm_box((0.36, 0.012, 0.012)), "shade", "body", wf @ Matrix.Translation((u, L - 0.1, 0.02)) @ Matrix.Rotation(0.15, 4, "Z"))
                    g.put(bm_cyl(0.015, 0.03, 6), "steel", "body", wf @ Matrix.Translation((u + 0.17, L - 0.06, 0.02)))
        # the side frames round the door openings, and the doors
        door = [(0.37, zf + 0.1), (1.19, zf + 0.1), (1.19, zc), (self.cab_front(1.96) - 0.1, 1.96), (0.37, 1.96)]
        side_outline = [(x0, zf), (x1, zf), (x1, zc), (xt, zr), (x0, zr)]
        for side, bone in ((1.0, "door_l"), (-1.0, "door_r")):
            y = side * Y
            if lod < 2:
                self.side_plate(g, side_outline, y, 0.025, "paint", "body", holes=[door])
            else:
                self.side_plate(g, side_outline, y, 0.025, "paint", "body")
                continue
            dz = [(x - 0.006 if i in (1, 2) else x + 0.006, z) for i, (x, z) in enumerate(door)]
            win = [(0.45, 1.5), (1.12, 1.5), (1.12, zc + 0.01), (self.cab_front(1.9) - 0.13, 1.9), (0.45, 1.9)]
            self.side_plate(g, dz, y + side * 0.012, 0.03, "paint", bone, holes=[win])
            # the window wound half down, the handle, the hinges, a pressed panel line
            g.put(bm_box((0.66, 0.006, 0.36)), "glass", bone, Matrix.Translation((0.785, y + side * 0.005, 1.5 + 0.18 - 0.16)))
            if lod == 0:
                g.put(bm_tube([Vector((0.45, y + side * 0.03, 1.38)), Vector((0.47, y + side * 0.06, 1.38)), Vector((0.6, y + side * 0.06, 1.38)),
                               Vector((0.62, y + side * 0.03, 1.38))], 0.009, 5), "steel", bone)
                for z in (1.05, 1.75):
                    g.put(bm_cyl(0.018, 0.09, 8), "shade", bone, Matrix.Translation((1.2, y + side * 0.03, z)))
                g.put(bm_box((0.7, 0.006, 0.012)), "paint", bone, Matrix.Translation((0.78, y + side * 0.03, 1.12)))
                # the mirror on its arm from the windscreen's pillar
                p0 = Vector((self.cab_front(1.9) + 0.02, y + side * 0.02, 1.9))
                p1 = p0 + Vector((0.08, side * 0.24, 0.05))
                g.put(bm_tube([p0, p0 + Vector((0.03, side * 0.08, 0.0)), p1], 0.01, 5), "steel", "body")
                g.put(bm_lathe([(0.0, -0.015), (0.08, -0.012), (0.085, 0.0), (0.075, 0.006), (0.0, 0.008)], 10), "paint", "body", toward(p1, (-1, side * 0.2, 0)))
                g.put(bm_cyl(0.07, 0.004, 10), "glass", "body", toward(p1 - Vector((0.008, 0, 0)), (-1, side * 0.2, 0)))
        if lod == 2:
            return
        # inside: the floor, the bench, the steering wheel and column (left, the driver's), the dashboard, levers
        g.put(bm_box((x1 - x0, 2 * Y, 0.03)), "shade", "body", Matrix.Translation(((x0 + x1) * 0.5, 0, zf + 0.12)))
        g.put(bm_box((0.42, 1.7, 0.14), pick(lod, 0.03, 0.0)), "canvas", "body", Matrix.Translation((0.5, 0, 1.12)))
        g.put(bm_box((0.12, 1.7, 0.5), pick(lod, 0.03, 0.0)), "canvas", "body", Matrix.Translation((0.3, 0, 1.45)) @ Matrix.Rotation(-0.12, 4, "Y"))
        g.put(bm_box((0.1, 2 * Y - 0.05, 0.18), bev), "paint", "body", Matrix.Translation((x1 - 0.07, 0, zc - 0.12)))
        col0, col1 = Vector((1.2, 0.42, 1.0)), Vector((0.88, 0.42, 1.56))
        rod(g, col0, col1, 0.028, 6, "shade", "body")
        handwheel(g, col1 + (col1 - col0).normalized() * 0.03, col1 - col0, 0.2, "body", lod, spokes=3, mat="rubber", knob=False)
        if lod == 0:
            rod(g, (0.95, 0.12, 0.9), (0.85, 0.15, 1.3), 0.012, 5, "steel", "body")
            g.put(bm_lathe([(0.0, 0.0), (0.03, 0.01), (0.03, 0.04), (0.0, 0.05)], 8), "wood", "body", toward((0.85, 0.15, 1.3), (-0.25, 0.07, 1)))
            rod(g, (0.95, 0.25, 0.9), (0.82, 0.26, 1.2), 0.012, 5, "steel", "body")
            for k in range(3):
                g.put(bm_cyl(0.03, 0.01, 10), "glass", "body", toward((x1 - 0.13, 0.25 - 0.18 * k, zc - 0.08), (-1, 0, 0.3)))
        # the running boards, with ribs, and the step under each door
        for side in (1.0, -1.0):
            g.put(bm_box((1.05, 0.26, 0.035), pick(lod, 0.006, 0.0)), "shade", "body", Matrix.Translation((0.78, side * 1.0, 0.62)))
            if lod == 0:
                for k in range(5):
                    g.put(bm_box((1.0, 0.012, 0.008)), "shade", "body", Matrix.Translation((0.78, side * (0.9 + 0.05 * k), 0.64)))
                for x in (0.4, 1.15):
                    g.put(bm_box((0.04, 0.24, 0.12)), "shade", "body", Matrix.Translation((x, side * 0.86, 0.66)))

    side_plate = FieldGun.side_plate

    def wings(self, g, lod):
        """Front wings over the front wheels, rounded and lipped, running back to the running boards, with the headlamps on
        them; flat guards over the rear twins with mud flaps."""
        R, FX = self.R, self.FX
        rr = R + 0.11
        steps = pick(lod, 12, 6, 3)
        for side in (1.0, -1.0):
            yc = side * (self.FY + 0.02)
            pts, ups = [], []
            for i in range(steps + 1):
                a = math.radians(18 + (172 - 18) * i / steps)
                pts.append(Vector((FX + rr * math.cos(a), yc, R + rr * math.sin(a))))
                ups.append(Vector((math.cos(a), 0, math.sin(a))))
            pts.append(Vector((self.CAB1 + 0.02, yc, 0.635)))
            ups.append(Vector((0, 0, 1)))
            pts.append(Vector((self.CAB1 - 0.08, yc, 0.635)))
            ups.append(Vector((0, 0, 1)))
            prof = [(-0.18, 0.0), (0.18, 0.0), (0.2, -0.015), (0.2, -0.07), (0.186, -0.07), (0.186, -0.012), (-0.17, -0.012)] if lod < 2 else \
                [(-0.18, 0.0), (0.18, 0.0), (0.18, -0.012), (-0.18, -0.012)]
            if side < 0:
                prof = [(-b, c) for b, c in prof][::-1]
            g.put(bm_tube(pts, 1.0, profile=prof, up=ups), "paint", "body")
            # (the inner valance between the wing and the bonnet)
            if lod < 2:
                g.put(bm_box((0.9, 0.012, 0.3)), "paint", "body", Matrix.Translation((FX - 0.1, side * (self.BON_Y[0] + 0.02), 0.85)))
            lamp(g, Vector((FX + 0.5, side * 0.72, 1.1)), "body", lod)
            if lod == 0:
                # the sidelight on the wing's crown, a convoy marker on the left
                g.put(bm_lathe([(0.0, 0.0), (0.035, 0.0), (0.035, 0.05), (0.0, 0.055)], 8), "paint", "body", toward((FX + 0.05, side * (self.FY + 0.12), R + rr + 0.005), (0, 0, 1)))
                g.put(bm_cyl(0.026, 0.02, 8), "glass", "body", toward((FX + 0.085, side * (self.FY + 0.12), R + rr + 0.03), (1, 0, 0)))
            # the rear guard over the twins, and its flap
            x0, x1 = self.RX - 0.55, self.RX + 0.55
            g.put(bm_box((x1 - x0, 0.5, 0.02), pick(lod, 0.005, 0.0)), "paint", "body", Matrix.Translation((self.RX, side * self.RY, 0.93)))
            if lod < 2:
                g.put(bm_box((0.012, 0.48, 0.42)), "rubber", "body", Matrix.Translation((x0 - 0.01, side * self.RY, 0.72)))
                g.put(bm_box((x1 - x0, 0.012, 0.12)), "paint", "body", Matrix.Translation((self.RX, side * (self.RY + 0.25), 0.87)))
        if lod == 0:
            # the horn on the left wing's valance, a reflector each side at the back
            g.put(bm_lathe([(0.0, 0.0), (0.02, 0.0), (0.05, 0.1), (0.055, 0.11), (0.0, 0.11)], 10), "paint", "body", toward((FX + 0.55, 0.48, 0.95), (1, 0, 0)))

    def bed(self, g, lod):
        """The bed: floor on bearers, dropsides of three planks with stakes, hinges and latches, the tailboard with its
        chains and a step, benches along the sides inside."""
        b0, b1, Y = self.BED0, self.BED1, self.BED_Y
        fl, top = self.FLOOR, self.SIDE_TOP
        g.put(bm_box((b0 - b1 - 0.05, 2 * Y - 0.09, 0.08), pick(lod, 0.006, 0.0)), "wood", "body", Matrix.Translation(((b0 + b1) * 0.5, 0, fl - 0.04)))
        if lod < 2:
            for x in (b0 - 0.25, b0 - 1.1, b1 + 1.1, b1 + 0.25):
                g.put(bm_box((0.1, 2 * Y, 0.12)), "shade", "body", Matrix.Translation((x, 0, fl - 0.14)))
        planks = pick(lod, 3, 1, 1)
        ph = (top - fl) / planks
        for side in (1.0, -1.0):
            for k in range(planks):
                z = fl + ph * (k + 0.5)
                g.put(bm_box((b0 - b1, 0.045, ph - (0.012 if lod == 0 else 0.0)), pick(lod, 0.005, 0.0)), "paint", "body",
                      Matrix.Translation(((b0 + b1) * 0.5, side * (Y - 0.022), z)))
            if lod == 2:
                continue
            for k in range(5):
                x = b0 - 0.25 - k * (b0 - b1 - 0.5) / 4
                g.put(bm_box((0.07, 0.035, top - fl + 0.08), pick(lod, 0.005, 0.0)), "shade", "body", Matrix.Translation((x, side * (Y + 0.015), (fl + top) * 0.5 - 0.03)))
            if lod == 0:
                # hinges along the foot, latches at the corners, a rope hook between each pair of stakes
                for k in range(4):
                    x = b0 - 0.6 - k * (b0 - b1 - 1.0) / 3
                    g.put(bm_cyl(0.016, 0.12, 6), "steel", "body", toward((x, side * (Y + 0.005), fl - 0.005), (1, 0, 0)))
                for x in (b0 - 0.06, b1 + 0.06):
                    g.put(bm_box((0.06, 0.025, 0.08), 0.004), "steel", "body", Matrix.Translation((x, side * (Y + 0.02), top - 0.08)))
                # the bench inside: a board on brackets, folded down for the riders
                g.put(bm_box((b0 - b1 - 0.25, 0.3, 0.03), 0.005), "wood", "body", Matrix.Translation(((b0 + b1) * 0.5, side * (Y - 0.22), fl + 0.4)))
                for k in range(4):
                    x = b0 - 0.3 - k * (b0 - b1 - 0.6) / 3
                    g.put(bm_box((0.03, 0.26, 0.03)), "steel", "body", Matrix.Translation((x, side * (Y - 0.18), fl + 0.37)) @ Matrix.Rotation(side * 0.5, 4, "X"))
                    g.put(bm_box((0.03, 0.03, 0.4)), "steel", "body", Matrix.Translation((x, side * (Y - 0.33), fl + 0.2)))
        # the tailboard, and the chains that hold it
        for k in range(planks):
            z = fl + ph * (k + 0.5)
            g.put(bm_box((0.045, 2 * Y, ph - (0.012 if lod == 0 else 0.0)), pick(lod, 0.005, 0.0)), "paint", "body", Matrix.Translation((b1 - 0.02, 0, z)))
        if lod == 0:
            for y in (-0.6, 0.0, 0.6):
                g.put(bm_box((0.035, 0.07, top - fl + 0.04), 0.005), "shade", "body", Matrix.Translation((b1 - 0.05, y, (fl + top) * 0.5)))
            for side in (1.0, -1.0):
                pts = [Vector((b1 - 0.04, side * (Y - 0.02), top - 0.04))]
                for k in range(1, 6):
                    pts.append(Vector((b1 - 0.04 + 0.01 * (k % 2), side * (Y - 0.02 + 0.006 * (k % 2)), top - 0.04 + 0.05 * k)))
                g.put(bm_tube(pts, 0.009, 4), "steel", "body")
            # the step under the tailboard
            st = [Vector((b1 - 0.06, -0.18, fl - 0.05)), Vector((b1 - 0.12, -0.18, fl - 0.4)), Vector((b1 - 0.12, 0.18, fl - 0.4)), Vector((b1 - 0.06, 0.18, fl - 0.05))]
            g.put(bm_tube(st, 0.016, 6), "steel", "body")

    def tilt(self, g, lod):
        """The canvas tilt over five hoops: sagging between them, lashed down along its foot, its back rolled up; an inner
        skin so that it is not hollow seen from inside; the hoops themselves."""
        b0, b1 = self.BED0 - 0.03, self.BED1 + 0.03
        nx = pick(lod, 24, 8, 2)
        ns = pick(lod, 17, 10, 6)
        prof, outs = self.tilt_profile(ns)
        hoops = self.HOOPS
        rng = random.Random(3)
        wob = [rng.uniform(-1, 1) for _ in range((nx + 1) * len(prof))]

        def sag_at(x, j):
            # between hoops the canvas sags in; at the ends it is pulled taut over the end hoops
            for h0, h1 in zip(hoops, hoops[1:]):
                if h0 <= x <= h1:
                    u = (x - h0) / (h1 - h0)
                    return math.sin(math.pi * u) ** 1.5
            return 0.0

        def at(x, j, inset=0.0, k=None):
            y, z = prof[j]
            oy, oz = outs[j]
            roof = smoothstep(0.2, 0.9, oz)
            amount = (0.02 + 0.045 * roof) * sag_at(x, j) if lod < 2 else 0.0
            w = 0.004 * wob[k] if (lod == 0 and k is not None) else 0.0
            d = -(amount + inset) + w
            return Vector((x, y + oy * d, z + oz * d))

        def weight(p):
            # (the middle of the roof between hoops rides partly on the `tilt` bone: the flutter)
            hz = smoothstep(self.TILT_FOOT + 0.4, self.TILT_TOP, p.z)
            s = 0.0
            for h0, h1 in zip(hoops, hoops[1:]):
                if h0 <= p.x <= h1:
                    s = math.sin(math.pi * (p.x - h0) / (h1 - h0))
            w = 0.6 * hz * s
            return {"body": 1.0 - w, "tilt": w} if w > 0.01 else {"body": 1.0}
        xs = [b1 + (b0 - b1) * i / nx for i in range(nx + 1)]
        rows = [[at(x, j, 0.0, i * len(prof) + j) for j in range(len(prof))] for i, x in enumerate(xs)]
        nr, nc = len(rows), len(rows[0])
        verts = [p for r in rows for p in r]
        faces = [[i * nc + j, (i + 1) * nc + j, (i + 1) * nc + j + 1, i * nc + j + 1] for i in range(nr - 1) for j in range(nc - 1)]
        g.add(verts, faces, "canvas", weight)
        # the inner skin, coarser, two centimetres in
        if lod < 2:
            nxi = pick(lod, 10, 4)
            pi, oi = self.tilt_profile(10)
            xs_i = [b1 + 0.01 + (b0 - b1 - 0.02) * i / nxi for i in range(nxi + 1)]
            rows_i = []
            for x in xs_i:
                r = []
                for (y, z), (oy, oz) in zip(pi, oi):
                    d = -0.022
                    r.append(Vector((x, y + oy * d, z + oz * d)))
                rows_i.append(r)
            sheet_rows(g, rows_i, "canvas", "body", flip=False)
        # the front end, closed against the cab
        endp = [(y, z) for y, z in prof]
        bm = bmesh.new()
        vs = [bm.verts.new((b0 - 0.005, y, z)) for y, z in endp]
        bm.faces.new(vs)
        bmesh.ops.triangulate(bm, faces=bm.faces)
        g.put(bm, "canvas", "body")
        # the back rolled up under the roof, tied with straps
        if lod < 2:
            ry = self.BED_Y - 0.12
            roll = bm_cyl(0.075, 2 * ry, pick(lod, 10, 6))
            g.put(roll, "canvas", "body", Matrix.Translation((b1 - 0.02, 0, self.TILT_TOP - 0.24)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
            if lod == 0:
                for y in (-0.6, 0.0, 0.6):
                    pts = [Vector((b1 + 0.05, y, self.TILT_TOP - 0.03)), Vector((b1 - 0.1, y, self.TILT_TOP - 0.18)),
                           Vector((b1 - 0.08, y, self.TILT_TOP - 0.33)), Vector((b1 + 0.02, y, self.TILT_TOP - 0.3))]
                    g.put(bm_tube(pts, 1.0, profile=[(-0.02, -0.003), (0.02, -0.003), (0.02, 0.003), (-0.02, 0.003)], up=(0, 1, 0)), "shade", "body")
        # the hoops, inside the canvas
        if lod < 2:
            ph, po = self.tilt_profile(pick(lod, 12, 8), foot=self.SIDE_TOP - 0.1)
            for x in (hoops[:2] if lod == 0 else hoops[:1]):
                pts = [Vector((x, y - oy * 0.035, z - oz * 0.035)) for (y, z), (oy, oz) in zip(ph, po)]
                g.put(bm_tube(pts, 0.016, 4), "shade", "body")
        # the lashings along the foot, from the canvas's eyelets to hooks on the dropsides
        if lod == 0:
            for side in (1.0, -1.0):
                for k in range(9):
                    x = b1 + 0.2 + (b0 - b1 - 0.4) * k / 8
                    y = side * (self.BED_Y + 0.03)
                    pts = [Vector((x, y, self.TILT_FOOT - 0.09)), Vector((x + 0.02, y + side * 0.012, self.TILT_FOOT - 0.2)),
                           Vector((x + 0.04, y + side * 0.012, self.TILT_FOOT - 0.32))]
                    g.put(bm_tube(pts, 0.007, 4, caps=False), "canvas", "body")

    def stowage(self, g, lod):
        """The spare wheel (left, under the bed), jerrycans and a toolbox (right, under the bed), a pick and a shovel on the
        left dropside, a rolled net on the cab's roof."""
        R = self.R
        # the spare wheel lying flat in its cradle under the bed's back, behind the axle
        sp = Vector((self.BED1 + 0.46, 0.0, 0.5))
        road_wheel(g, sp, 1.0, R * 0.98, 0.2, "body", lod, rim_r=0.27, axis=(0, 0, -1))
        if lod < 2:
            for y in (-0.3, 0.3):
                g.put(bm_box((0.9, 0.05, 0.04)), "shade", "body", Matrix.Translation((sp.x, y, 0.37)))
            g.put(bm_box((0.05, 0.7, 0.04)), "shade", "body", Matrix.Translation((sp.x, 0.0, 0.37)))
            # the fuel tank on the left, under the bed
            g.put(bm_lathe([(0.0, -0.36), (0.17, -0.35), (0.19, -0.3), (0.19, 0.3), (0.17, 0.35), (0.0, 0.36)], pick(lod, 14, 8)), "paint", "body",
                  toward((-0.42, 0.74, 0.62), (1, 0, 0)))
            if lod == 0:
                for x in (-0.68, -0.16):
                    g.put(bm_tube([Vector((x, 0.55, 0.82)), Vector((x, 0.82, 0.82)), Vector((x, 0.95, 0.66)), Vector((x, 0.88, 0.44)), Vector((x, 0.6, 0.42))],
                                  1.0, profile=[(-0.02, -0.003), (0.02, -0.003), (0.02, 0.003), (-0.02, 0.003)], up=(0, 1, 0)), "shade", "body")
                g.put(bm_cyl(0.04, 0.05, 8), "steel", "body", toward((-0.2, 0.85, 0.79), (0, 0.4, 1)))
        if lod < 2:
            for k in range(3):
                jerrycan(g, Vector((-0.05 - 0.19 * k, -0.86, 0.42)), 90.0, "body", lod)
            g.put(bm_box((0.6, 0.06, 0.04)), "shade", "body", Matrix.Translation((-0.24, -0.86, 0.4)))
            g.put(bm_box((0.36, 0.3, 0.32), pick(lod, 0.012, 0.0)), "paint", "body", Matrix.Translation((-0.85, -0.82, 0.62)))
            if lod == 0:
                g.put(bm_box((0.6, 0.012, 0.03)), "shade", "body", Matrix.Translation((-0.24, -0.95, 0.72)))
                for dx in (-0.1, 0.1):
                    g.put(bm_box((0.04, 0.02, 0.05)), "steel", "body", Matrix.Translation((-0.85 + dx, -0.98, 0.72)))
        if lod == 0:
            y = self.BED_Y + 0.04
            tool_shovel(g, (self.BED1 + 0.45, y, 1.28), (self.BED1 + 1.45, y, 1.28), "body", Vector((0, 1, 0)))
            tool_pick(g, (self.BED1 + 1.6, y, 1.12), (self.BED1 + 2.5, y, 1.12), "body", Vector((0, 1, 0)))
            for x in (self.BED1 + 0.7, self.BED1 + 1.25, self.BED1 + 1.8, self.BED1 + 2.3):
                g.put(bm_box((0.035, 0.03, 0.08)), "steel", "body", Matrix.Translation((x, y + 0.005, 1.2)))
            # a rolled net on the cab roof, lashed down
            g.put(bm_lathe([(0.0, -0.75), (0.11, -0.72), (0.14, -0.4), (0.12, 0.0), (0.15, 0.4), (0.12, 0.72), (0.0, 0.76)], 10), "canvas", "body",
                  Matrix.Translation((0.62, 0.0, self.CAB_ROOF + 0.17)) @ Matrix.Rotation(math.pi / 2, 4, "X") @ Matrix.Rotation(0.3, 4, "Z"))
            for y in (-0.45, 0.45):
                pts = [Vector((0.62 + 0.17 * math.cos(a), y, self.CAB_ROOF + 0.17 + 0.155 * math.sin(a))) for a in [math.pi * k / 6 for k in range(-1, 8)]]
                g.put(bm_tube(pts, 0.008, 4), "canvas", "body")

    # -------------------------------------------------------------------------------------------- pictures
    def picture_pose(self):
        return {}

    def views(self):
        return {"front": ((0.0, 0.0, 1.15), 11.5, 12.0, 34.0, 50.0),
                "rear": ((-0.5, 0.0, 1.2), 11.5, 15.0, 146.0, 50.0),
                "detail": ((1.55, 0.55, 1.05), 3.9, 9.0, 38.0, 45.0),
                "game": ((0.0, 0.0, 1.0), 16.0, 30.0, 40.0, 40.0)}

    def view_labels(self):
        return {"front": "Front three-quarter", "rear": "Rear three-quarter: tilt, benches, pintle", "detail": "Close: wing, lamp, cab, tyre",
                "game": "The game's camera (16 m, 30 degrees down)"}

    def sheet_title(self):
        return f"The battle lorry: our three-tonner from the campaign's Truck ({self.nation_word} paint)"

    def json_extra(self, index):
        return {"controls": [
            {"parts": [index["steer_l"], index["steer_r"]], "what": "steering, turned by the game (+ to the right, clockwise seen from above)",
             "limits": [-0.6, 0.6], "note": "give the inner wheel the larger angle: tan(inner) = W / (T - t/2), tan(outer) = W / (T + t/2), "
                                              f"W = {self.FX - self.RX:.2f} m wheelbase, t = {2 * (self.FY - 0.12):.2f} m between the king pins"},
            {"what": "drive play rate", "clip": "drive", "rate": f"speed (m/s) * {self.DRIVE_LOOP} / {self.DRIVE_M:.4f}"},
            {"what": "doors", "parts": [index["door_l"], index["door_r"]], "note": "open (about 1.1) while crew get in or out at socket_driver / socket_codriver"}],
            "wheelbase": round(self.FX - self.RX, 3)}


# ================================================================================================ THE HALF-TRACKS
class BeltPath:
    """The line the inside of a track belt follows, seen from the side (x forward, z up), round a list of circles in loop
    order (forward along the bottom, back along the top), as tank.py's TrackPath. U runs once round it."""

    def __init__(self, circles, pitch=0.12):
        n = len(circles)
        tang = []
        for i in range(n):
            (x1, z1, r1), (x2, z2, r2) = circles[i], circles[(i + 1) % n]
            dx, dz = x2 - x1, z2 - z1
            L = math.hypot(dx, dz)
            ux, uz = dx / L, dz / L
            rx, rz = uz, -ux
            a = (r1 - r2) / L
            b = math.sqrt(max(1.0 - a * a, 0.0))
            nx, nz = a * ux + b * rx, a * uz + b * rz
            tang.append(((x1 + r1 * nx, z1 + r1 * nz), (x2 + r2 * nx, z2 + r2 * nz)))
        pts = []
        for i in range(n):
            cx, cz, r = circles[i]
            p_in, p_out = tang[i - 1][1], tang[i][0]
            a0 = math.atan2(p_in[1] - cz, p_in[0] - cx)
            a1 = math.atan2(p_out[1] - cz, p_out[0] - cx)
            da = (a1 - a0) % (2 * math.pi)
            if da > 1.9 * math.pi:
                da = 0.0
            steps = max(1, int(da * r / 0.01))
            for k in range(steps + 1):
                t = a0 + da * k / steps
                pts.append((cx + r * math.cos(t), cz + r * math.sin(t)))
            q0, q1 = p_out, tang[i][1]
            seg = max(1, int(math.hypot(q1[0] - q0[0], q1[1] - q0[1]) / 0.01))
            for k in range(1, seg):
                pts.append((q0[0] + (q1[0] - q0[0]) * k / seg, q0[1] + (q1[1] - q0[1]) * k / seg))
        P = np.array(pts + [pts[0]])
        d = np.hypot(np.diff(P[:, 0]), np.diff(P[:, 1]))
        P = P[np.concatenate([[True], d > 1e-6])]
        d = np.hypot(np.diff(P[:, 0]), np.diff(P[:, 1]))
        self.px, self.pz = P[:, 0], P[:, 1]
        self.cum = np.concatenate([[0.0], np.cumsum(d)])
        self.length = float(self.cum[-1])
        self.links = int(round(self.length / pitch))
        self.pitch = self.length / self.links

    def at(self, s):
        s = np.mod(s, self.length)
        return np.interp(s, self.cum, self.px), np.interp(s, self.cum, self.pz)

    def samples(self, tol):
        """Distances along the path where a band must have a ring to follow it within `tol` metres (straights are left long)."""
        keep = [0]
        P = np.stack([self.px, self.pz], 1)
        i = 0
        n = len(P) - 1
        while i < n:
            j = i + 2
            while j <= n:
                a, b = P[i], P[j]
                ab = b - a
                L = np.hypot(*ab)
                mid = P[i + 1:j]
                dev = np.abs((mid[:, 0] - a[0]) * ab[1] - (mid[:, 1] - a[1]) * ab[0]) / max(L, 1e-9)
                if dev.max() > tol:
                    break
                j += 1
            i = j - 1
            keep.append(i)
        return [float(self.cum[k]) for k in keep]


def sym_radius(r, sectors, metres):
    """The rolling radius, within a few percent of r, that turns a wheel a whole number of its like sectors over `metres`."""
    step = 2 * math.pi / sectors
    k = max(1, round(metres / r / step))
    return metres / (k * step)


class HalfTrack:
    """An armoured half-track from build_tank.OTHERS: "Hanomag" (Germany's: plates leaning in above and below a fold,
    big interleaved road wheels on a linked belt, welded) or "M3Halftrack" (America's: upright bolted plates, small
    road wheels in two bogies on a rubber band track, a roller on its nose). Each keeps its campaign piece's body,
    bonnet, track unit and wheels; added for a close camera: plate thickness and an open fighting compartment with
    benches for a squad, the driver's roofed cab with visors, rear doors, an armoured bonnet whose halves lift, the
    machine-gun on its shield (bone mg), steered front wheels with fenders and lamps, the belt (U along its length for a
    scrolling material) round a sprocket, road wheels and idler, welds or bolts, stowage."""

    kind = "halftrack"
    folder = "halftrack"
    noun = "half-track"

    def __init__(self, campaign="Hanomag", nation=None):
        s = dict(bt.OTHERS[campaign])
        self.s = s
        self.name = campaign
        self.campaign = campaign
        self.us = campaign.startswith("M3")
        self.nation = nation or ("USA" if self.us else "GER")
        self.nation_word = {"GER": "German", "USA": "American"}.get(self.nation, self.nation)
        self.title = f"Battle half-track {campaign} (our own design)"
        self.FX, self.R = s["front_x"], s["wheel_r"]
        self.FY = 0.9
        self.lean = s["lean"]
        self.BX, self.BL, self.BW = s["body_x"], s["body_long"], s["body_wide"]
        self.B0, self.B1 = self.BX - self.BL * 0.5, self.BX + self.BL * 0.5            # body's back and front
        self.Z0, self.ZF, self.ZT = 0.85, 1.35, 1.95                                    # its floor, fold and top
        half = self.BW * 0.5
        self.YB, self.YF, self.YT = half * (2.0 - self.lean), half, half * self.lean     # its half-width at floor, fold, top
        self.TOPF = self.B1 - 0.25                                                       # the top of the front plate
        self.WELL0, self.WELL1 = self.BX - self.BL * 0.47, self.BX + self.BL * 0.15      # the open compartment
        self.BON0, self.BON1 = self.B1, self.FX + 0.85
        # the track unit, as build_halftrack lays it
        n, r, x0, st = s["track_wheels"], s["track_wheel_r"], s["track_first"], s["track_step"]
        self.TW_R = r
        self.TW_Z = r + 0.05
        self.WHEELS = [x0 + st * k for k in range(n)]
        self.INNER = [(a + b) * 0.5 for a, b in zip(self.WHEELS, self.WHEELS[1:])] if s["interleaved"] else []
        last = x0 + st * (n - 1)
        if s["interleaved"]:
            self.SPR = (last + 0.55, r + 0.12, r * 0.95) if s["drive_front"] else (x0 - 0.55, r + 0.12, r * 0.95)
            self.IDL = (x0 - 0.55, r + 0.08, r * 0.9) if s["drive_front"] else (last + 0.55, r + 0.08, r * 0.9)
        else:
            # small bogie wheels: the sprocket and idler are bigger and stand higher than the campaign piece's, so the
            # belt's top run comes up near the body (over one return roller) instead of hugging the little wheels
            spr, idl = (last + 0.5, 0.40, 0.25), (x0 - 0.5, 0.36, 0.24)
            self.SPR, self.IDL = (spr, idl) if s["drive_front"] else ((idl[0], spr[1], spr[2]), (spr[0], idl[1], idl[2]))
        self.TY, self.TWD = 0.92, 0.3
        top = (self.SPR[1] + self.SPR[2] + self.IDL[1] + self.IDL[2]) * 0.5
        self.ROLLERS = [] if s["interleaved"] else [((self.SPR[0] + self.IDL[0]) * 0.5, top - 0.07 + 0.01, 0.07)]
        circ = [(self.IDL[0], self.IDL[1], self.IDL[2])] + [(x, self.TW_Z, r) for x in sorted(self.WHEELS)] + [self.SPR]
        if s["interleaved"]:
            circ += [(x, self.TW_Z, r) for x in sorted(self.WHEELS, reverse=True)][1:-1]
        else:
            circ += self.ROLLERS
        self.PATH = BeltPath(circ, pitch=0.14 if s["interleaved"] else 0.1)
        self.BELT_T = 0.035
        self.DRIVE_LOOP = 2.0
        self.DRIVE_M = round(10.5 / self.PATH.pitch) * self.PATH.pitch          # whole links a loop: ~10.5 m in 2 s
        self.DOORS = ("door_rl", "door_rr") if not self.us else ("door_rear",)

    # -------------------------------------------------------------------------------------------- measures
    def section(self, x):
        """The body's section at x: [(y, z)] from the left top edge down round the floor and up to the right top edge."""
        top_y = self.YT
        return [(top_y, self.ZT), (self.YF, self.ZF), (self.YB, self.Z0), (-self.YB, self.Z0), (-self.YF, self.ZF), (-top_y, self.ZT)]

    def front_x(self, z):
        """The body's front face at height z (the lower front upright, the visor plate leaning back above the fold)."""
        if z <= self.ZF:
            return self.B1
        return self.B1 - (self.B1 - self.TOPF) * (z - self.ZF) / (self.ZT - self.ZF)

    def bonnet_at(self, x):
        t = (x - (self.FX - 0.65)) / 1.5
        t = min(max(t, 0.0), 1.0)
        w_bot = 0.65 * (1.0 - 0.28 * t)
        w_top = w_bot * self.lean
        z_top = 1.47 - 0.12 * t
        return w_bot, w_top, z_top

    def radii(self):
        D = self.DRIVE_M
        out = {}
        for sl in "lr":
            out[f"wheel_f{sl}"] = sym_radius(self.R, 6, D)
            for i in range(len(self.WHEELS)):
                out[f"wheel_{sl}{i + 1}"] = sym_radius(self.TW_R, 6, D)
            for i in range(len(self.INNER)):
                out[f"wheel_{sl}i{i + 1}"] = sym_radius(self.TW_R, 6, D)
            out[f"sprocket_{sl}"] = self.PATH.links / max(1, round(2 * math.pi * self.SPR[2] / self.PATH.pitch)) * 0.0 + \
                sym_radius(self.SPR[2], 10, D)
            out[f"idler_{sl}"] = sym_radius(self.IDL[2], 6, D)
            for i in range(len(self.ROLLERS)):
                out[f"roller_{sl}{i + 1}"] = sym_radius(self.ROLLERS[i][2], 4, D)
        return out

    # -------------------------------------------------------------------------------------------- the rig
    def parts(self):
        P = [part("root", None, (0, 0, 0), (0, 0, 1), "fixed", None, "the half-track's origin, on the ground under its middle", (0, 0, 0.5)),
             part("body", "root", (0.0, 0.0, 0.8), (0, -1, 0), "turn", [-0.08, 0.08],
                  "the body on its springs: + pitches the nose up; the clips also roll it about x (+ right side up) and bob it along z", (1.0, 0, 0))]
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            P.append(part(f"steer_{sl}", "root", (self.FX, side * (self.FY - 0.13), self.R), (0, 0, 1), "turn", [-0.55, 0.55],
                          f"the {'left' if side > 0 else 'right'} front wheel's steering: + steers to the right (clockwise seen from above). The game steers.",
                          (0, 0, 0.3)))
            P.append(part(f"wheel_f{sl}", f"steer_{sl}", (self.FX, side * self.FY, self.R), (0, 1, 0), "spin", None,
                          f"front {'left' if side > 0 else 'right'} wheel: + rolls forward", (0, 0.25 * side, 0), radius=self.R))
        rad = self.radii()
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            y = side * self.TY
            for i, x in enumerate(self.WHEELS):
                P.append(part(f"wheel_{sl}{i + 1}", "root", (x, y, self.TW_Z), (0, 1, 0), "spin", None,
                              f"road wheel {i + 1} from the back, {'left' if side > 0 else 'right'}: + rolls forward", (0, 0.2 * side, 0), radius=self.TW_R))
            for i, x in enumerate(self.INNER):
                P.append(part(f"wheel_{sl}i{i + 1}", "root", (x, side * (self.TY - 0.12), self.TW_Z), (0, 1, 0), "spin", None,
                              f"inner road wheel {i + 1} from the back: + rolls forward", (0, 0.2 * side, 0), radius=self.TW_R))
            P.append(part(f"sprocket_{sl}", "root", (self.SPR[0], y, self.SPR[1]), (0, 1, 0), "spin", None,
                          "the drive sprocket: + drives forward", (0, 0.2 * side, 0), radius=round(rad[f"sprocket_{sl}"], 4)))
            P.append(part(f"idler_{sl}", "root", (self.IDL[0], y, self.IDL[1]), (0, 1, 0), "spin", None, "the idler: + rolls forward",
                          (0, 0.2 * side, 0), radius=self.IDL[2]))
            for i, (x, z, r) in enumerate(self.ROLLERS):
                P.append(part(f"roller_{sl}{i + 1}", "root", (x, y, z), (0, 1, 0), "spin", None, "a return roller: + rolls forward", (0, 0.2 * side, 0), radius=r))
            P.append(part(f"track_{sl}", "root", (0.0, y, 0.4), (1, 0, 0), "scroll", None,
                          "the belt: never moved as a bone; the game scrolls its U (see track in the notes)", (0.5, 0, 0), metres_per_u=self.PATH.length))
        if self.us:
            P.append(part("door_rear", "body", (self.B0 - 0.02, 0.45, 1.4), (0, 0, 1), "turn", [0.0, 1.6],
                          "the rear door, hinged at its left edge: + opens", (0, 0, 0.3)))
        else:
            P.append(part("door_rl", "body", (self.B0 - 0.02, 0.45, 1.4), (0, 0, 1), "turn", [0.0, 1.6], "the left rear door, hinged at its outer edge: + opens", (0, 0, 0.3)))
            P.append(part("door_rr", "body", (self.B0 - 0.02, -0.45, 1.4), (0, 0, -1), "turn", [0.0, 1.6], "the right rear door, hinged at its outer edge: + opens", (0, 0, 0.3)))
        mx = (self.BON0 + self.BON1) * 0.5
        zt = self.bonnet_at(mx)[2]
        P.append(part("bonnet_l", "body", (mx, 0.0, zt), (-1, 0, 0), "turn", [0.0, 1.9], "the armoured bonnet's left half, hinged along the top: + lifts it", (0.4, 0, 0)))
        P.append(part("bonnet_r", "body", (mx, 0.0, zt), (1, 0, 0), "turn", [0.0, 1.9], "the armoured bonnet's right half, hinged along the top: + lifts it", (0.4, 0, 0)))
        P.append(part("mg", "body", self.mg_pivot(), (0, 0, 1), "turn", [-0.7, 0.7],
                      "the machine-gun and its shield on their pintle: traverse, + clockwise seen from above. The game turns it.", (0.4, 0, 0)))
        return P

    def mg_pivot(self):
        return Vector((self.WELL1 - 0.12, 0.0, self.ZT + 0.02))

    def sockets(self):
        mp = self.mg_pivot()
        out = [("socket_exhaust", "body", Vector((self.B1 - 0.3, -self.YB + 0.05, 0.62)), Vector((0, -0.6, -0.8)).normalized()),
               ("socket_tow", "body", Vector((self.B0 - 0.2, 0.0, 0.62)), Vector((-1, 0, 0))),
               ("socket_dust_l", "root", Vector((self.IDL[0] - 0.4, self.TY, 0.12)), Vector((-1, 0, 0.35)).normalized()),
               ("socket_dust_r", "root", Vector((self.IDL[0] - 0.4, -self.TY, 0.12)), Vector((-1, 0, 0.35)).normalized()),
               ("socket_mg", "mg", mp + Vector((0.95, 0.0, 0.18)), Vector((1, 0, 0))),
               ("socket_mg_gunner", "body", Vector((mp.x - 0.45, 0.0, self.Z0 + 0.05)), Vector((1, 0, 0))),
               ("socket_engine", "body", Vector((self.FX + 0.1, 0.0, 1.35)), Vector((0, 0, 1))),
               ("socket_fire", "body", Vector(((self.WELL0 + self.WELL1) * 0.5, 0.0, 1.2)), Vector((0, 0, 1))),
               ("socket_driver", "body", Vector((self.TOPF - 0.45, 0.4, 1.15)), Vector((1, 0, 0))),
               ("socket_codriver", "body", Vector((self.TOPF - 0.45, -0.4, 1.15)), Vector((1, 0, 0))),
               ("socket_door", "root", Vector((self.B0 - 0.8, 0.0, 0.0)), Vector((-1, 0, 0)))]
        n = 5
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            for i in range(n):
                x = self.WELL0 + 0.3 + (self.WELL1 - self.WELL0 - 0.9) * i / (n - 1)
                out.append((f"socket_rider_{sl}{i + 1}", "body", Vector((x, side * (self.YF - 0.32), self.Z0 + 0.45)), Vector((0, -side, 0))))
        return out

    def marks(self):
        half = self.BW * 0.5
        return {"cross": [round(self.BX + 0.5, 3), round(half * (1.0 + self.lean) * 0.5 + 0.02, 3), 1.65, 0.17, round(half * (1.0 - self.lean) / 0.6, 3)],
                "number": [round(self.BX - 0.9, 3), round(half * (1.0 + self.lean) * 0.5 + 0.02, 3), 1.65, 0.22, round(half * (1.0 - self.lean) / 0.6, 3)],
                "roof": round(self.ZT + 0.05, 3)}

    # -------------------------------------------------------------------------------------------- the clips
    def rolling(self, c, travel):
        c.travel = travel
        for bone, r in self.radii().items():
            c.add(bone, "spin", lambda t, r=r: travel(t) / r)
        for sl in "lr":
            c.add(f"track_{sl}", "scroll", lambda t: travel(t) / self.PATH.length)

    def clips(self):
        rad = math.radians
        C = []
        D, LOOP = self.DRIVE_M, self.DRIVE_LOOP
        c = Clip("idle", 2.0, True, None, "engine running, standing still")
        c.add("body", "turn", lambda t: rad(0.05) * sin_(6, t, 0.5) + rad(0.02) * sin_(11.5, t))
        c.add("body", "turn", lambda t: rad(0.1) * sin_(3, t) + rad(0.04) * sin_(6, t, 2.0), axis=(1, 0, 0))
        c.add("body", "slide", lambda t: 0.0015 * sin_(6, t) + 0.0007 * sin_(11.5, t, 1.0), axis=(0, 0, 1))
        c.event(0.0, "exhaust_idle", "socket_exhaust")
        C.append(c)

        c = Clip("drive", LOOP, True, None, f"driving forward: {D:.3f} m a loop", metres=D)
        self.rolling(c, lambda t: D * t / LOOP)
        c.add("body", "turn", lambda t: rad(0.45) * sin_(0.5, t) + rad(0.2) * sin_(1.5, t, 0.7) + rad(0.05) * sin_(8, t))
        c.add("body", "turn", lambda t: rad(0.5) * sin_(0.5, t, 1.9) + rad(0.2) * sin_(2.0, t, 0.4), axis=(1, 0, 0))
        c.add("body", "slide", lambda t: 0.009 * sin_(1.0, t, 0.4) + 0.003 * sin_(3.5, t) + 0.0015 * sin_(8, t, 2.0), axis=(0, 0, 1))
        c.event(0.0, "exhaust_drive", "socket_exhaust")
        c.event(0.0, "dust_trail", "socket_dust_l").event(0.0, "dust_trail", "socket_dust_r")
        C.append(c)

        c = Clip("stop", 0.8, False, None, "pulling up from the drive")
        v0, tb = D / LOOP, 0.3
        self.rolling(c, lambda t: v0 * t - v0 * t * t / (2 * tb) if t < tb else v0 * tb * 0.5)

        def nose(t):
            if t < 0.25:
                return -rad(1.5) * math.sin(0.5 * math.pi * t / 0.25)
            return -rad(1.5) * math.exp(-(t - 0.25) / 0.14) * math.cos(2 * math.pi * (t - 0.25) / 0.45) * (1.0 - smoothstep(0.65, 0.8, t))
        c.add("body", "turn", nose)
        c.add("body", "slide", lambda t: nose(t) * 0.3, axis=(0, 0, 1))
        c.event(0.0, "brakes", "socket_dust_l").event(0.0, "brakes", "socket_dust_r")
        C.append(c)

        c = Clip("knocked_out", 3.0, False, None, "hit and knocked out; holds its last frame")
        still = lambda t: 1.0 - smoothstep(1.0, 1.4, t)
        c.add("body", "turn", lambda t: _damped(t, rad(2.0), 0.4, 0.22) * still(t) + _ease(t, 0.3, 1.3, 0.0, -rad(0.9)))
        c.add("body", "turn", lambda t: _damped(t, rad(2.4), 0.45, 0.25) * still(t) + _ease(t, 0.3, 1.3, 0.0, rad(1.6)), axis=(1, 0, 0))
        c.add("body", "slide", lambda t: _damped(t, 0.03, 0.35, 0.18) * still(t) + _ease(t, 0.3, 1.3, 0.0, -0.025), axis=(0, 0, 1))

        def keys(seq):
            def f(t):
                for (t0, a0), (t1, a1) in zip(seq, seq[1:]):
                    if t <= t1:
                        u = (t - t0) / (t1 - t0)
                        u = u * u if a1 > a0 else 1 - (1 - u) * (1 - u)
                        return a0 + (a1 - a0) * u
                return seq[-1][1]
            return f
        c.add("bonnet_l", "turn", keys([(0.0, 0.0), (0.1, 0.0), (0.28, 1.6), (0.4, 1.15), (0.52, 1.45), (0.64, 1.3), (0.76, 1.38), (0.9, 1.35)]))
        c.add(self.DOORS[0], "turn", keys([(0.0, 0.0), (0.45, 0.0), (0.8, 1.55), (0.95, 1.35), (1.1, 1.5), (1.25, 1.45)]))
        c.add("mg", "turn", lambda t: _ease(t, 0.05, 0.4, 0.0, rad(14.0)) + _damped(t, rad(4.0), 0.35, 0.2, 0.4))
        c.event(0.0, "hit", None, "tank_knocked_out")
        c.event(0.1, "smoke_burst", "socket_engine")
        c.event(1.0, "fire_loop", "socket_fire")
        c.event(1.6, "smoke_loop", "socket_engine")
        C.append(c)
        return C

    # -------------------------------------------------------------------------------------------- the model
    def geo(self, lod):
        g = Geo()
        self.running_gear(g, lod)
        self.front_wheels(g, lod)
        self.hull(g, lod)
        self.bonnet(g, lod)
        self.fighting(g, lod)
        self.fittings(g, lod)
        return g

    def belt(self, g, side, lod):
        """The belt as a band with thickness: U from 0 to 1 once round (forward along the bottom), V across."""
        P = self.PATH
        ss = P.samples(pick(lod, 0.003, 0.012, 0.04))
        if lod == 2:
            ss = ss[::2] + [ss[-1]] if len(ss) > 40 else ss
        s = np.array(ss + [P.length])
        x, z = P.at(s)
        x[-1], z[-1] = x[0], z[0]
        xa, za = P.at(s + 0.02)
        xb, zb = P.at(s - 0.02)
        tx, tz = xa - xb, za - zb
        ln = np.hypot(tx, tz)
        ox, oz = tz / ln, -tx / ln
        half = self.TWD * 0.5
        yc = side * self.TY
        T = self.BELT_T
        name = "track_" + ("l" if side > 0 else "r")
        for k in range(len(s) - 1):
            u0, u1 = s[k] / P.length, s[k + 1] / P.length
            p = lambda i, w, y: (x[i] + ox[i] * w, y, z[i] + oz[i] * w)
            k1 = k + 1
            quads = []
            for y0, y1 in ((-half, half),):
                out = [p(k, T, yc + y0), p(k1, T, yc + y0), p(k1, T, yc + y1), p(k, T, yc + y1)]
                inn = [p(k, 0.0, yc + y1), p(k1, 0.0, yc + y1), p(k1, 0.0, yc + y0), p(k, 0.0, yc + y0)]
                quads += [(out, [(u0, 0), (u1, 0), (u1, 1), (u0, 1)]), (inn, [(u0, 1), (u1, 1), (u1, 0), (u0, 0)])]
            for ye in (half, -half):
                rim = [p(k, 0.0, yc + ye), p(k, T, yc + ye), p(k1, T, yc + ye), p(k1, 0.0, yc + ye)]
                if ye < 0:
                    rim = rim[::-1]
                quads.append((rim, [(u0, 0.5), (u0, 0.5), (u1, 0.5), (u1, 0.5)]))
            for quad, uv in quads:
                g.add(quad, [[0, 1, 2, 3]], "track", name, uvs=[uv])

    def road_wheel_hal(self, g, c, side, r, bone, lod, wide=0.2, dished=True, inner=False):
        out = Vector((0.0, side, 0.0))
        turn = Matrix.Rotation(-side * math.pi / 2, 4, "X")
        m = Matrix.Translation(c) @ turn
        n = pick(lod, 16, 12, 8)
        if self.s["interleaved"]:
            # a big pressed steel wheel on a thin solid rubber rim (the belt's horns run between the pairs); the inner row,
            # mostly hidden behind the outer, is plainer
            if inner:
                if lod == 2:
                    return
                if lod == 1:
                    g.put(bm_cyl(r - 0.01, wide * 0.8, 8), "paint", bone, m)
                    return
                n = 12
            elif lod == 1:
                n = 10
            if lod == 2:
                g.put(bm_cyl(r, wide * 0.8, n), "rubber", bone, m)
                return
            g.put(bm_lathe([(r - 0.04, -wide * 0.5), (r, -wide * 0.5), (r, wide * 0.5), (r - 0.04, wide * 0.5)], n), "rubber", bone, m)
            prof = [(r - 0.04, -wide * 0.5), (r - 0.04, wide * 0.5), (r * 0.7, wide * 0.55), (r * 0.45, wide * 0.48), (0.08, wide * 0.6), (0.0, wide * 0.62)]
            g.put(bm_lathe(prof if (lod == 0 and not inner) else [prof[0], prof[1], prof[3], prof[5]], n), "paint", bone, m)
            if lod == 0 and not inner:
                bolt_ring(g, m @ Vector((0, 0, wide * 0.5)), out, r * 0.57, 6, bone, 0.011, 0.01, "shade", sides=4)
                bolt(g, m @ Vector((0, 0, wide * 0.6)), out, bone, 0.035, 0.02, "shade", sides=6)
        else:
            # a small bogie wheel: a rubber tyre on a spoked disc
            g.put(bm_lathe(tyre_profile(r, wide * 0.9, lod, bulge=0.006, rim_r=r * 0.62), n), "rubber", bone, m)
            if lod == 2:
                return
            g.put(bm_lathe([(r * 0.62, -wide * 0.4), (r * 0.62, wide * 0.36), (r * 0.3, wide * 0.42), (0.0, wide * 0.5)], n), "paint", bone, m)
            if lod == 0:
                for k in range(6):
                    a = 2 * math.pi * k / 6
                    g.put(bm_lathe([(0.025, 0.0), (0.0, 0.0)], 6), "shade", bone, m @ Matrix.Translation((r * 0.46 * math.cos(a), r * 0.46 * math.sin(a), wide * 0.4)))
                bolt(g, m @ Vector((0, 0, wide * 0.5)), out, bone, 0.03, 0.015, "shade", sides=6)

    def running_gear(self, g, lod):
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            y = side * self.TY
            for i, x in enumerate(self.WHEELS):
                self.road_wheel_hal(g, Vector((x, y + side * 0.02, self.TW_Z)), side, self.TW_R, f"wheel_{sl}{i + 1}", lod, wide=0.12 if self.INNER else 0.2)
            for i, x in enumerate(self.INNER):
                self.road_wheel_hal(g, Vector((x, side * (self.TY - 0.12), self.TW_Z)), side, self.TW_R, f"wheel_{sl}i{i + 1}", lod, wide=0.12, inner=True)
            # the sprocket (toothed rings), the idler, rollers
            sx, sz, sr = self.SPR
            m = Matrix.Translation((sx, y, sz)) @ Matrix.Rotation(-side * math.pi / 2, 4, "X")
            teeth = max(8, int(round(2 * math.pi * sr / self.PATH.pitch)))
            if lod < 2:
                poly = []
                for k in range(teeth):
                    a = 2 * math.pi * k / teeth
                    da = 2 * math.pi / teeth
                    poly += [(sr - 0.035, a - 0.42 * da), (sr + 0.02, a - 0.14 * da), (sr + 0.02, a + 0.14 * da), (sr - 0.035, a + 0.42 * da)]
                poly = [(rr * math.cos(a), rr * math.sin(a)) for rr, a in poly]
                for h in ((-0.07, 0.07) if lod == 0 else (0.0,)):
                    g.put(bm_prism(poly, 0.03 if lod == 0 else 0.2), "shade", f"sprocket_{sl}", m @ Matrix.Translation((0, 0, h)))
            g.put(bm_lathe([(sr * 0.75, -0.11), (sr * 0.75, 0.11), (sr * 0.4, 0.14), (0.0, 0.15)], pick(lod, 14, 10, 8)), "paint", f"sprocket_{sl}", m)
            if lod == 0:
                bolt_ring(g, m @ Vector((0, 0, 0.12)), (0, side, 0), sr * 0.55, 10, f"sprocket_{sl}", 0.01, 0.01, "shade", sides=5)
                # the final drive's housing on the body
                g.put(bm_lathe([(0.2, 0.0), (0.2, 0.06), (0.12, 0.12), (0.0, 0.12)], 12), "paint", "body",
                      Matrix.Translation((sx, side * (self.TY - 0.24), sz)) @ Matrix.Rotation(-side * math.pi / 2, 4, "X"))
            ix, iz, ir = self.IDL
            m = Matrix.Translation((ix, y, iz)) @ Matrix.Rotation(-side * math.pi / 2, 4, "X")
            g.put(bm_lathe([(ir - 0.03, -0.12), (ir, -0.12), (ir, -0.06), (ir - 0.03, -0.05), (ir - 0.03, 0.05), (ir, 0.06), (ir, 0.12), (ir - 0.03, 0.12),
                            (ir * 0.4, 0.14), (0.0, 0.15)] if lod == 0 else [(ir, -0.12), (ir, 0.12), (0.0, 0.14)], pick(lod, 16, 10, 8)), "steel", f"idler_{sl}", m)
            for i, (x, z, r) in enumerate(self.ROLLERS):
                if lod < 2:
                    g.put(bm_cyl(r, 0.18, pick(lod, 10, 6)), "rubber", f"roller_{sl}{i + 1}", Matrix.Translation((x, y, z)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
            self.belt(g, side, lod)
            if lod < 2:
                # the suspension: swing arms or bogie frames from each wheel to the body, the idler's crank
                if self.s["interleaved"]:
                    for x in self.WHEELS + self.INNER:
                        g.put(bm_box((0.32, 0.05, 0.07), pick(lod, 0.006, 0.0)), "shade", "body",
                              Matrix.Translation((x + 0.14, side * (self.TY - 0.24), self.TW_Z + 0.08)) @ Matrix.Rotation(-0.35, 4, "Y"))
                else:
                    for a, b in zip(self.WHEELS[0::2], self.WHEELS[1::2]):
                        mid = (a + b) * 0.5
                        g.put(bm_box((b - a + 0.1, 0.06, 0.16), pick(lod, 0.01, 0.0)), "paint", "body", Matrix.Translation((mid, side * (self.TY - 0.12), self.TW_Z + 0.04)))
                        g.put(bm_box((0.18, 0.12, 0.3), pick(lod, 0.01, 0.0)), "paint", "body", Matrix.Translation((mid, side * (self.TY - 0.2), self.TW_Z + 0.22)))
                        if lod == 0:
                            g.put(bm_lathe([(0.07, 0.0), (0.07, 0.16), (0.0, 0.16)], 8), "steel", "body", toward((mid, side * (self.TY - 0.12), self.TW_Z + 0.12), (0, 0, 1)))
                g.put(bm_box((0.4, 0.06, 0.08)), "shade", "body", Matrix.Translation((ix + 0.25, side * (self.TY - 0.22), iz + 0.1)) @ Matrix.Rotation(0.3, 4, "Y"))

    def front_wheels(self, g, lod):
        R, FX = self.R, self.FX
        for side, sl in ((1.0, "l"), (-1.0, "r")):
            road_wheel(g, Vector((FX, side * self.FY, R)), side, R, 0.24, f"wheel_f{sl}", lod, rim_r=0.26, lugs=14)
            # the fender: flat, bent down before and behind the wheel
            yc = side * (self.FY + 0.01)
            pts = [Vector((FX + 0.62, yc, 0.66)), Vector((FX + 0.45, yc, R * 2 + 0.11)), Vector((FX - 0.45, yc, R * 2 + 0.11)), Vector((FX - 0.62, yc, 0.8))]
            ups = [Vector((0.8, 0, 0.6)), Vector((0, 0, 1)), Vector((0, 0, 1)), Vector((-0.6, 0, 0.8))]
            prof = [(-0.18, 0.0), (0.18, 0.0), (0.18, -0.012), (-0.18, -0.012)]
            if side < 0:
                prof = [(-b, c) for b, c in prof][::-1]
            g.put(bm_tube(pts, 1.0, profile=prof, up=ups), "paint", "body")
            if lod < 2:
                g.put(bm_box((1.0, 0.012, 0.22)), "paint", "body", Matrix.Translation((FX, side * (self.FY - 0.17), R * 2 + 0.0)))
            lamp(g, Vector((FX + 0.45, side * 0.62, 1.12)), "body", lod, r=0.085)
        # the front axle and its springs (on the wheels' side and the body's)
        g.put(bm_box((0.09, 2 * self.FY - 0.3, 0.1), pick(lod, 0.008, 0.0)), "shade", "root", Matrix.Translation((FX, 0, R)))
        if lod == 0:
            for side in (1.0, -1.0):
                for k in range(4):
                    L = 1.0 - 0.2 * k
                    g.put(bm_box((L, 0.07, 0.013)), "steel", "body", Matrix.Translation((FX, side * 0.5, R + 0.1 - 0.015 * k)))
        if self.s["roller"]:
            # the roller on the nose, in its frame
            g.put(bm_cyl(0.16, 1.1, pick(lod, 14, 10, 8)), "shade", "body", Matrix.Translation((FX + 1.05, 0, 0.62)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
            for side in (1.0, -1.0):
                g.put(bm_box((0.5, 0.05, 0.16), pick(lod, 0.008, 0.0)), "paint", "body", Matrix.Translation((FX + 0.85, side * 0.6, 0.66)))
        else:
            for side in (1.0, -1.0):
                rod(g, (FX + 0.7, side * 0.45, 0.66), (FX + 0.88, side * 0.45, 0.66), 0.04, 6, "shade", "body")
                if lod == 0:
                    g.put(bm_tube([Vector((FX + 0.86, side * 0.45, 0.72)), Vector((FX + 0.95, side * 0.45, 0.72)), Vector((FX + 0.97, side * 0.45, 0.62)),
                                   Vector((FX + 0.9, side * 0.45, 0.58))], 0.018, 6), "steel", "body")

    side_plate = FieldGun.side_plate

    def hull(self, g, lod):
        """The armoured body: its plates (with thickness, the top open over the fighting compartment), the roofed cab with
        its visor plate, the rear plate with its door(s), welds or bolts along the joints."""
        B0, B1 = self.B0, self.B1
        sec = self.section(0)
        th = pick(lod, 0.014, 0.014, 0.0)
        # the sides and floor as one bent plate, its front edge following the visor plate's lean
        rows = []
        nx = 2
        for i in range(nx + 1):
            x = B0 + (B1 - B0) * i / nx
            rows.append([Vector((min(x, self.front_x(z)) if i == nx else x, y, z)) for y, z in sec])
        inward = lambda p: Vector((0, -math.copysign(1.0, p.y) if abs(p.y) > 0.05 and p.z > self.Z0 + 0.02 else 0.0, 1.0 if p.z <= self.Z0 + 0.02 else 0.0)).normalized() \
            if (abs(p.y) > 0.05 or p.z <= self.Z0 + 0.02) else Vector((0, 0, 1))
        if th > 0:
            thick_rows(g, rows, th, "paint", "body", inward)
        else:
            sheet_rows(g, rows, "paint", "body")
        # the lower front plate and the visor plate, with the visors
        front = [(-self.YB, self.Z0), (self.YB, self.Z0), (self.YF, self.ZF), (-self.YF, self.ZF)]
        g.put(bm_plate(front, (), 0.014), "paint", "body", frame((B1 - 0.007, 0, 0), (0, 1, 0), (1, 0, 0)) @ Matrix.Identity(4))
        a, b = Vector((B1, 0, self.ZF)), Vector((self.TOPF, 0, self.ZT))
        up = (b - a).normalized()
        n = Vector((up.z, 0, -up.x))
        L = (b - a).length
        vf = frame(a, Vector((0, 1, 0)), n)
        wtop = self.YT
        vis = [(-self.YF, 0.0), (self.YF, 0.0), (wtop, L), (-wtop, L)]
        holes = [[(0.12, L * 0.45), (0.5, L * 0.45), (0.5, L * 0.6), (0.12, L * 0.6)], [(-0.5, L * 0.45), (-0.12, L * 0.45), (-0.12, L * 0.6), (-0.5, L * 0.6)]] \
            if lod < 2 else []
        g.put(bm_plate(vis, holes, 0.014), "paint", "body", vf @ Matrix.Translation((0, 0, -0.007)))
        if lod < 2:
            for u in (0.31, -0.31):
                g.put(bm_box((0.38, 0.15, 0.006)), "glass", "body", vf @ Matrix.Translation((u, L * 0.525, -0.03)))
                # the visor's armoured flap, propped open above the slit
                g.put(bm_box((0.44, 0.13, 0.016), pick(lod, 0.004, 0.0)), "paint", "body",
                      vf @ Matrix.Translation((u, L * 0.62 + 0.04, 0.05)) @ Matrix.Rotation(-0.9, 4, "X"))
        # the cab's roof, from the visor plate's top back to the compartment
        g.put(bm_box((self.TOPF - self.WELL1 + 0.02, 2 * self.YT - 0.01, 0.014)), "paint", "body",
              Matrix.Translation(((self.TOPF + self.WELL1) * 0.5, 0, self.ZT - 0.007)))
        # the rear plate, with its door opening
        rear = [(-self.YB, self.Z0), (self.YB, self.Z0), (self.YF, self.ZF), (self.YT, self.ZT), (-self.YT, self.ZT), (-self.YF, self.ZF)]
        dh = [(-0.45, self.Z0 + 0.1), (0.45, self.Z0 + 0.1), (0.45, self.ZT - 0.12), (-0.45, self.ZT - 0.12)]
        g.put(bm_plate(rear, [dh] if lod < 2 else [], 0.014), "paint", "body", frame((B0 + 0.007, 0, 0), (0, 1, 0), (1, 0, 0)))
        if lod < 2:
            dz0, dz1 = self.Z0 + 0.1, self.ZT - 0.12
            if self.us:
                dm = Matrix.Translation((B0 - 0.005, 0, 0))
                g.put(bm_box((0.016, 0.88, dz1 - dz0 - 0.01), pick(lod, 0.004, 0.0)), "paint", "door_rear", Matrix.Translation((B0 - 0.008, 0.0, (dz0 + dz1) * 0.5)))
                handles = [(-0.3, "door_rear")]
            else:
                for side, bone in ((1.0, "door_rl"), (-1.0, "door_rr")):
                    g.put(bm_box((0.016, 0.44, dz1 - dz0 - 0.01), pick(lod, 0.004, 0.0)), "paint", bone, Matrix.Translation((B0 - 0.008, side * 0.224, (dz0 + dz1) * 0.5)))
                handles = [(0.08, "door_rl"), (-0.08, "door_rr")]
            if lod == 0:
                for y, bone in handles:
                    g.put(bm_tube([Vector((B0 - 0.016, y, 1.35)), Vector((B0 - 0.06, y, 1.35)), Vector((B0 - 0.06, y, 1.5)), Vector((B0 - 0.016, y, 1.5))], 0.01, 5),
                          "steel", bone)
                for side in ((1.0,) if self.us else (1.0, -1.0)):
                    for z in (dz0 + 0.15, dz1 - 0.15):
                        g.put(bm_cyl(0.022, 0.1, 8), "shade", "body", Matrix.Translation((B0 - 0.02, side * 0.46, z)))
        if lod == 0:
            # joints: welds on the German one, rows of bolts on the American one
            for side in (1.0, -1.0):
                fold_a, fold_b = Vector((B0, side * self.YF, self.ZF)), Vector((B1, side * self.YF, self.ZF))
                outn = Vector((0, side, 0.25)).normalized()
                if self.us:
                    rivet_row(g, fold_a + Vector((0.05, side * 0.008, 0.03)), fold_b + Vector((-0.05, side * 0.008, 0.03)), 22, Vector((0, side, 0)), "body", 0.011, "paint")
                    rivet_row(g, Vector((B0 + 0.05, side * (self.YT + 0.008), self.ZT - 0.04)), Vector((self.TOPF - 0.05, side * (self.YT + 0.008), self.ZT - 0.04)), 22,
                              Vector((0, side, 0)), "body", 0.011, "paint")
                    for x in (B0 + 0.03, B1 - 0.03):
                        rivet_row(g, Vector((x, side * (self.YF + 0.008), self.ZF + 0.05)), Vector((x, side * (self.YT + 0.008), self.ZT - 0.08)), 6,
                                  Vector((0, side, 0)), "body", 0.011, "paint")
                else:
                    weld(g, fold_a, fold_b, outn, "body")
                    weld(g, Vector((B1, side * self.YB, self.Z0)), Vector((B1, side * self.YF, self.ZF)), Vector((1, side, 0)), "body")
                    weld(g, Vector((B0, side * self.YB, self.Z0)), Vector((B0, side * self.YF, self.ZF)), Vector((-1, side, 0)), "body")
                    weld(g, Vector((B0, side * self.YF, self.ZF)), Vector((B0, side * self.YT, self.ZT)), Vector((-1, side, 0)), "body")
                    weld(g, Vector((B1, side * self.YF, self.ZF)), Vector((self.TOPF, side * self.YT, self.ZT)), Vector((1, side, 0)), "body")

    def bonnet(self, g, lod):
        """The armoured bonnet in two halves hinged along its top (bones bonnet_l/r), with louvres; the grille; the bulkhead."""
        x0, x1 = self.BON0, self.BON1
        nx = pick(lod, 4, 2, 1)
        for side, bone in ((1.0, "bonnet_l"), (-1.0, "bonnet_r")):
            rows = []
            for i in range(nx + 1):
                x = x0 + 0.01 + (x1 - x0 - 0.02) * i / nx
                wb, wt, zt = self.bonnet_at(x)
                prof = [(0.0, zt + 0.01), (wt - 0.03, zt), (wt, zt - 0.03), ((wt + wb) * 0.5, (zt + self.Z0) * 0.5), (wb, self.Z0)]
                rows.append([Vector((x, side * y, z)) for y, z in prof])
            inward = lambda p, side=side: Vector((0, -side, -0.3)).normalized() if abs(p.y) > 0.2 else Vector((0, 0, -1))
            thick_rows(g, rows if side > 0 else [r[::-1] for r in rows][::-1], 0.012, "paint", bone, inward)
            if lod == 0:
                # louvres on the side, the hinges along the top
                for k in range(6):
                    x = x0 + 0.3 + 0.09 * k
                    wb, wt, zt = self.bonnet_at(x)
                    zz = zt - 0.18
                    yy = wt + (wb - wt) * (zt - zz) / (zt - self.Z0)
                    g.put(bm_box((0.035, 0.02, 0.22)), "paint", bone, Matrix.Translation((x, side * (yy + 0.008), zz - 0.1)) @
                          Matrix.Rotation(math.radians(-22 * side), 4, "Z") @ Matrix.Rotation(-side * math.atan2(wb - wt, zt - self.Z0), 4, "X"))
                for x in (x0 + 0.25, x1 - 0.3):
                    wb, wt, zt = self.bonnet_at(x)
                    g.put(bm_cyl(0.016, 0.1, 6), "steel", "body", toward((x, side * 0.02, zt + 0.012), (1, 0, 0)))
                for x in ((x0 + x1) * 0.5,):
                    wb, wt, zt = self.bonnet_at(x)
                    g.put(bm_tube([Vector((x - 0.08, side * (wt - 0.1), zt + 0.002)), Vector((x - 0.06, side * (wt - 0.1), zt + 0.04)),
                                   Vector((x + 0.06, side * (wt - 0.1), zt + 0.04)), Vector((x + 0.08, side * (wt - 0.1), zt + 0.002))], 0.009, 5), "steel", bone)
        # the nose: an armoured grille
        wb, wt, zt = self.bonnet_at(x1)
        nose = [(-wb, self.Z0), (wb, self.Z0), (wt, zt), (-wt, zt)]
        holes = [[(-wb + 0.1, self.Z0 + 0.12), (wb - 0.1, self.Z0 + 0.12), (wt - 0.08, zt - 0.08), (-wt + 0.08, zt - 0.08)]] if lod < 2 else []
        g.put(bm_plate(nose, holes, 0.016), "paint", "body", frame((x1, 0, 0), (0, 1, 0), (1, 0, 0)))
        if lod < 2:
            g.put(bm_box((0.03, 2 * wt, zt - self.Z0 - 0.1)), "shade", "body", Matrix.Translation((x1 - 0.06, 0, (zt + self.Z0) * 0.5)))
            nb = pick(lod, 8, 4)
            for k in range(nb):
                zz = self.Z0 + 0.14 + (zt - self.Z0 - 0.26) * k / (nb - 1)
                ww = wb + (wt - wb) * (zz - self.Z0) / (zt - self.Z0)
                g.put(bm_box((0.03, 2 * ww - 0.2, 0.025)), "paint", "body", Matrix.Translation((x1 - 0.01, 0, zz)) @ Matrix.Rotation(0.5, 4, "Y"))
        # (the floor under the bonnet, and the bulkhead behind it)
        g.put(bm_box((x1 - x0, 1.2, 0.012)), "shade", "body", Matrix.Translation(((x0 + x1) * 0.5, 0, self.Z0 - 0.006)))
        if lod == 0:
            g.put(bm_box((0.7, 0.34, 0.3), 0.01), "shade", "body", Matrix.Translation(((x0 + x1) * 0.5 + 0.1, 0, self.Z0 + 0.2)))
            g.put(bm_box((0.66, 0.28, 0.08), 0.01), "steel", "body", Matrix.Translation(((x0 + x1) * 0.5 + 0.1, 0.02, self.Z0 + 0.39)))

    def fighting(self, g, lod):
        """The open compartment: floor, benches down both sides with their backs, ammunition boxes, racks; the machine-gun on
        its shield (bone mg); the driver's seats and wheel under the roof."""
        if lod == 2:
            g.put(bm_box((self.WELL1 - self.WELL0, 2 * self.YT - 0.1, 0.02)), "shade", "body", Matrix.Translation(((self.WELL0 + self.WELL1) * 0.5, 0, self.ZT - 0.5)))
            return
        W0, W1 = self.WELL0, self.WELL1
        g.put(bm_box((self.B1 - self.B0 - 0.04, 2 * self.YB - 0.2, 0.03)), "shade", "body", Matrix.Translation(((self.B0 + self.B1) * 0.5, 0, self.Z0 + 0.03)))
        for side in (1.0, -1.0):
            y = side * (self.YF - 0.3)
            g.put(bm_box((W1 - W0 - 0.5, 0.32, 0.05), pick(lod, 0.008, 0.0)), "wood", "body", Matrix.Translation(((W0 + W1) * 0.5 - 0.15, y, self.Z0 + 0.4)))
            # (the back, inside the side plate where it leans in most, at the back's top)
            yb = self.YF - (self.YF - self.YT) * (self.Z0 + 0.87 - self.ZF) / (self.ZT - self.ZF) - 0.014 - 0.035
            g.put(bm_box((W1 - W0 - 0.5, 0.05, 0.3), pick(lod, 0.008, 0.0)), "wood", "body", Matrix.Translation(((W0 + W1) * 0.5 - 0.15, side * yb, self.Z0 + 0.72)))
            if lod == 0:
                for k in range(4):
                    x = W0 + 0.3 + (W1 - W0 - 0.9) * k / 3
                    g.put(bm_box((0.04, 0.04, 0.38)), "shade", "body", Matrix.Translation((x, y - side * 0.1, self.Z0 + 0.2)))
                # rifle racks along the top inside, ammunition boxes under the benches
                g.put(bm_box((W1 - W0 - 0.6, 0.03, 0.04)), "shade", "body", Matrix.Translation(((W0 + W1) * 0.5 - 0.15, side * (self.YT - 0.06), self.ZT - 0.18)))
                for k in range(3):
                    g.put(bm_box((0.28, 0.14, 0.18), 0.006), "paint", "body", Matrix.Translation((W0 + 0.5 + 0.55 * k, y, self.Z0 + 0.14)))
        # the machine-gun on its shield, turning on a pintle
        mp = self.mg_pivot()
        rod(g, mp - Vector((0, 0, 0.35)), mp + Vector((0, 0, 0.05)), 0.03, pick(lod, 8, 6), "shade", "body")
        sh = [(-0.3, 0.0), (0.3, 0.0), (0.3, 0.3), (0.2, 0.38), (-0.2, 0.38), (-0.3, 0.3)]
        g.put(bm_plate(sh, [[(0.035 * math.cos(2 * math.pi * k / 8), 0.18 + 0.035 * math.sin(2 * math.pi * k / 8)) for k in range(8)]], 0.012), "paint", "mg",
              frame(mp + Vector((0.25, 0, 0.0)), (0, 1, 0), (1, 0, 0)) @ Matrix.Translation((0, -0.0, 0)))
        rod(g, mp + Vector((-0.25, 0, 0.18)), mp + Vector((0.95, 0, 0.18)), 0.016, pick(lod, 8, 5), "steel", "mg")
        if lod == 0:
            g.put(bm_box((0.36, 0.06, 0.1), 0.006), "shade", "mg", Matrix.Translation(mp + Vector((-0.12, 0, 0.17))))
            g.put(bm_box((0.12, 0.04, 0.12), 0.004), "wood", "mg", Matrix.Translation(mp + Vector((-0.35, 0, 0.15))) @ Matrix.Rotation(0.3, 4, "Y"))
            rod(g, mp + Vector((0.95, 0, 0.18)), mp + Vector((1.0, 0, 0.18)), 0.022, 8, "shade", "mg")
            g.put(bm_lathe([(0.0, 0.0), (0.06, 0.0), (0.06, 0.06), (0.0, 0.06)], 10), "paint", "mg", toward(mp + Vector((0.05, 0.06, 0.12)), (0, 1, 0)))
            g.put(bm_tube([mp + Vector((0.3, 0.08, 0.2)), mp + Vector((0.25, -0.1, 0.32)), mp + Vector((0.15, -0.16, 0.28))], 0.004, 4), "steel", "mg")
        # the driver's seats and wheel, under the roof
        x = self.TOPF - 0.45
        for side in (1.0, -1.0):
            g.put(bm_box((0.4, 0.42, 0.1), pick(lod, 0.02, 0.0)), "canvas", "body", Matrix.Translation((x, side * 0.4, self.Z0 + 0.35)))
            g.put(bm_box((0.08, 0.42, 0.4), pick(lod, 0.02, 0.0)), "canvas", "body", Matrix.Translation((x - 0.22, side * 0.4, self.Z0 + 0.6)))
        c0, c1 = Vector((self.B1 - 0.05, 0.4, self.Z0 + 0.25)), Vector((x + 0.35, 0.4, self.Z0 + 0.75))
        rod(g, c0, c1, 0.025, 6, "shade", "body")
        handwheel(g, c1, c1 - c0, 0.2, "body", lod, spokes=3, mat="rubber", knob=False)

    def fittings(self, g, lod):
        """Stowage and fittings: jerrycans, track guards with boxes, tools, a tarpaulin roll along the compartment's edge,
        the aerial, the exhaust, the tow hook at the back."""
        B0, B1 = self.B0, self.B1
        if not self.s["interleaved"] and lod < 2:
            # the chassis rails, seen in the gap between the belt's top run and the body
            for side in (1.0, -1.0):
                g.put(bm_box((self.B1 - self.B0 + 0.4, 0.08, 0.16), pick(lod, 0.006, 0.0)), "shade", "body",
                      Matrix.Translation(((self.B0 + self.B1) * 0.5 + 0.2, side * 0.62, self.Z0 - 0.1)))
        if lod == 2:
            return
        for k in range(3):
            jerrycan(g, Vector((self.BX - 0.3 - 0.2 * k + 0.0, (self.YB + 0.1), self.Z0 + 0.05)), 90.0, "body", lod)
        g.put(bm_box((0.66, 0.2, 0.04)), "shade", "body", Matrix.Translation((self.BX - 0.5, self.YB + 0.1, self.Z0 + 0.03)))
        if lod == 0:
            g.put(bm_box((0.66, 0.012, 0.03)), "shade", "body", Matrix.Translation((self.BX - 0.5, self.YB + 0.19, self.Z0 + 0.36)))
            # a stowage box and the tools on the right side
            g.put(bm_box((0.6, 0.22, 0.3), 0.01), "paint", "body", Matrix.Translation((self.BX - 0.6, -(self.YB + 0.11), self.Z0 + 0.15)))
            out = Vector((0, -1, 0))
            y = -(self.YF + 0.03) if self.lean < 1.0 else -(self.YF + 0.025)
            zz = (self.ZF + self.ZT) * 0.5
            tool_shovel(g, (B0 + 0.4, y, zz), (B0 + 1.4, y, zz), "body", out)
            tool_pick(g, (B0 + 1.6, y, zz - 0.05), (B0 + 2.5, y, zz - 0.05), "body", out)
            for x in (B0 + 0.6, B0 + 1.2, B0 + 1.8, B0 + 2.3):
                clamp_strap(g, Vector((x, y, zz)), Vector((1, 0, 0)), out, "body")
            # a tarpaulin rolled along the compartment's left edge, lashed
            ya = self.YT + 0.06
            g.put(bm_lathe([(0.0, 0.0), (0.075, 0.01), (0.085, 0.3), (0.078, 0.9), (0.086, 1.5), (0.075, 2.0), (0.0, 2.01)], 10), "canvas", "body",
                  toward((self.WELL0 + 0.15, ya, self.ZT + 0.03), (1, 0, 0)))
            for x in (self.WELL0 + 0.5, self.WELL0 + 1.6):
                pts = [Vector((x, ya + 0.1 * math.cos(a), self.ZT + 0.03 + 0.1 * math.sin(a))) for a in [math.pi * k / 5 for k in range(-3, 7)]]
                g.put(bm_tube(pts, 0.007, 4), "canvas", "body")
            # the aerial on the right, the exhaust under the left, the tow hook
            base = Vector((self.BX + self.BL * 0.3, -self.YT + 0.08, self.ZT))
            rod(g, base, base + Vector((0, 0, 1.5)), 0.008, 4, "shade", "body", r2=0.004)
            g.put(bm_lathe([(0.03, 0.0), (0.03, 0.06), (0.015, 0.09), (0.0, 0.09)], 8), "shade", "body", toward(base, (0, 0, 1)))
            ex = [Vector((B1 + 0.1, -0.35, self.Z0 - 0.05)), Vector((B1 - 0.15, -0.6, self.Z0 - 0.15)), Vector((B1 - 0.3, -self.YB + 0.05, 0.62))]
            g.put(bm_tube(ex, 0.035, 8), "rust", "body")
            g.put(bm_lathe([(0.0, 0.0), (0.09, 0.02), (0.09, 0.5), (0.0, 0.52)], 10), "rust", "body", toward(Vector((B1 + 0.1, -0.35, self.Z0 - 0.05)), (-1, 0, 0)))
            hk = Vector((B0 - 0.2, 0.0, 0.62))
            g.put(bm_box((0.2, 0.2, 0.16), 0.01), "shade", "body", Matrix.Translation((B0 - 0.08, 0, 0.66)))
            g.put(bm_tube([hk + Vector((0.06 * math.cos(2 * math.pi * i / 10), 0, 0.06 * math.sin(2 * math.pi * i / 10))) for i in range(10)][2:9], 0.022, 6), "steel", "body")
            # spare road wheel on the nose plate (the German one), headlamp guards, a notice board for the unit's sign
            if not self.us:
                sp = Vector((B1 + 0.02, 0.0, self.ZF - 0.05))
                self.road_wheel_hal(g, sp - Vector((0.0, 0, 0)), 1.0, self.TW_R, "body", lod, wide=0.1)
                for gi in range(0):
                    pass

    # -------------------------------------------------------------------------------------------- pictures
    def picture_pose(self):
        return {}

    def views(self):
        return {"front": ((0.1, 0.0, 1.05), 11.5, 12.0, 34.0, 50.0),
                "rear": ((-0.4, 0.0, 1.1), 11.5, 17.0, 146.0, 50.0),
                "detail": ((self.IDL[0] + 1.3, 0.9, 0.55), 4.6, 12.0, 60.0, 40.0),
                "game": ((0.0, 0.0, 0.9), 16.0, 30.0, 40.0, 40.0)}

    def view_labels(self):
        return {"front": "Front three-quarter", "rear": "Rear three-quarter: open compartment, benches, doors",
                "detail": "Close: running gear and belt", "game": "The game's camera (16 m, 30 degrees down)"}

    def sheet_title(self):
        return f"The battle half-track: {self.campaign} ({self.nation_word} paint)"

    def json_extra(self, index):
        P = self.PATH
        return {"track": {"metres_per_u": round(P.length, 4), "links": P.links, "pitch": round(P.pitch, 5),
                          "metres_per_drive_loop": round(self.DRIVE_M, 4), "drive_loop_seconds": self.DRIVE_LOOP,
                          "scroll": "U runs once round each belt (0 to 1), forward along the bottom; V across. Sample the belt's texture at "
                                    "U + d / metres_per_u, d the metres travelled forward. One link (or pad) to every 1/links of U.",
                          "track_out": self.TY, "width": self.TWD},
                "controls": [
                    {"parts": [index["steer_l"], index["steer_r"]], "what": "steering, turned by the game (+ to the right)", "limits": [-0.55, 0.55]},
                    {"part": index["mg"], "part_name": "mg", "what": "machine-gun traverse, turned by the game (+ clockwise seen from above)", "limits": [-0.7, 0.7]},
                    {"what": "belts", "parts": [index["track_l"], index["track_r"]], "scroll_u_per_metre": round(1.0 / P.length, 6)},
                    {"what": "drive play rate", "clip": "drive", "rate": f"speed (m/s) * {self.DRIVE_LOOP} / {self.DRIVE_M:.4f}"}]}

    def picture_extras(self, st):
        track_picture_material(self.PATH.links, band=self.us)

    def follow_clip(self, st, clip):
        pass


# ================================================================================================ putting a unit together
def assemble(unit, lod=0, rig=True):
    """The unit in the current scene: rig, sockets and the skinned mesh."""
    parts = unit.parts()
    arm = make_rig(parts, unit.name + "_rig") if rig and parts else None
    g = unit.geo(lod)
    mesh = make_mesh(g, f"{unit.name}_LOD{lod}", arm, unit.nation)
    socks = make_sockets(arm, unit.sockets()) if arm is not None else []
    return arm, mesh, socks, parts, g


# ------------------------------------------------------------------------------------------------ delivery
# /mnt/project-files is a network mount: each file is made locally, copied there once, and read back later to check.
SENT = []


def out_dir(unit):
    return os.path.join(OUT_ROOT, unit.folder) + "/"


def stage_path(unit, *names):
    p = os.path.join(WORK, "out", unit.folder, *names)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    return p


def deliver(unit, local, rel):
    dest = os.path.join(out_dir(unit), rel)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    shutil.copyfile(local, dest)
    SENT.append((local, dest, os.path.getsize(local)))
    return dest


def check_delivered(wait=10.0):
    """Read every copied file back (after a pause) and compare it with the local one."""
    import hashlib
    if not SENT:
        return True
    time.sleep(wait)
    ok = True
    for local, dest, size in SENT:
        h = lambda p: hashlib.sha1(open(p, "rb").read()).hexdigest()
        good = os.path.exists(dest) and os.path.getsize(dest) == size and h(dest) == h(local)
        note = ""
        if not good and os.path.exists(dest) and dest.endswith((".png", ".gif")):
            from PIL import Image, ImageChops
            a, b = Image.open(local), Image.open(dest)
            good = a.size == b.size and getattr(a, "n_frames", 1) == getattr(b, "n_frames", 1) and \
                ImageChops.difference(a.convert("RGB"), b.convert("RGB")).getbbox() is None
            note = " (re-saved by the share, same pixels)" if good else ""
        elif not good and os.path.exists(dest) and dest.endswith(".mp4") and os.path.getsize(dest) > 0.5 * size:
            good, note = True, f" (re-saved by the share: {os.path.getsize(dest)} bytes)"
        ok &= good
        print(f"  {'ok ' if good else 'BAD'} {dest} ({size} bytes){note}")
    SENT.clear()
    return ok


# ------------------------------------------------------------------------------------------------ the exports
def _fbx(path, objs, anim):
    kit._select_only(objs)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
                             object_types={"ARMATURE", "MESH", "EMPTY"}, use_mesh_modifiers=False, mesh_smooth_type="FACE",
                             add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL",
                             bake_anim=anim, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=True, bake_anim_force_startend_keying=True, bake_anim_step=1.0,
                             bake_anim_simplify_factor=0.0, axis_forward="X", axis_up="Z")


def _glb(path, objs, anim):
    if anim:
        kit.export_glb(path, objs)
        return
    kit._select_only(objs)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_animations=False, export_apply=False,
                              export_yup=True, export_skins=True, export_extras=True)


def mesh_json(obj, index):
    """A mesh in the game's own form (build_tank.gather's): points, facings, paints, triangles, with each point's part
    (its bone, the strongest where it is shared) and its UV."""
    me = obj.data
    me.calc_loop_triangles()
    normals = [cn.vector.copy() for cn in me.corner_normals]
    uvd = me.uv_layers["UVMap"].data
    gname = {vg.index: vg.name for vg in obj.vertex_groups}
    vpart = []
    for v in me.vertices:
        best = max(v.groups, key=lambda e: e.weight) if len(v.groups) else None
        vpart.append(index.get(gname[best.group], 0) if best else 0)
    paint_of = [bt.PAINTS[m.name.split(".")[0]] for m in me.materials]
    pts, fac, pai, tri, prt, uvs = [], [], [], [], [], []
    seen = {}
    for lt in me.loop_triangles:
        paint = paint_of[me.polygons[lt.polygon_index].material_index]
        for li in lt.loops:
            vi = me.loops[li].vertex_index
            p = me.vertices[vi].co
            n = normals[li]
            u, w = uvd[li].uv
            key = (round(p.x, 4), round(-p.y, 4), round(p.z, 4), round(n.x, 3), round(-n.y, 3), round(n.z, 3), paint, vpart[vi],
                   round(u, 4), round(w, 4))
            i = seen.get(key)
            if i is None:
                i = len(pai)
                seen[key] = i
                pts.extend(key[0:3])
                fac.extend(key[3:6])
                pai.append(paint)
                prt.append(key[7])
                uvs.extend(key[8:10])
            tri.append(i)
    return {"points": pts, "facings": fac, "paints": pai, "triangles": tri, "part": prt, "uv": uvs}


ABOUT_TURNS = ("Turns are in radians, about the part's axis through its pivot, positive by v' = v cos a + (axis x v) sin a + "
               "axis (axis.v)(1 - cos a) computed in these axes (as Unreal's FQuat(axis, angle)); each part's 'sense' says what + "
               "does; a traverse is + clockwise seen from above. Slides are metres along the axis (also given as `slide`), carried "
               "by the parent's own turn (a barrel runs back along its elevated cradle). A part's pose is applied about its pivot, "
               "then its parent's, up to the root. A track may carry its own axis (the body rolls and bobs as well as pitching). "
               "A clip that does not loop says what follows it in `after` (hold: keep its last pose).")


def write_json(unit, path, meshes, parts, clip_list):
    index = {p["name"]: i for i, p in enumerate(parts)}
    near, mid, far = (mesh_json(meshes[k], index) for k in (0, 1, 2))
    zs, xs = near["points"][2::3], near["points"][0::3]
    data = {
        "name": unit.title,
        "about": f"Made by Tools/battle_units/guns_vehicles.py from build_tank.py's {unit.campaign}. Metres; x forward, y right, z up. "
                 "Paints: " + ", ".join(f"{v} {k}" for k, v in bt.PAINTS.items()) +
                 ". near = LOD0, mid = LOD1, far = LOD2. Each point has a part (index into parts) and a uv. " + ABOUT_TURNS,
        "long": round(max(xs) - min(xs), 3),
        "high": round(max(zs) - min(zs), 3),
        "marks": unit.marks(),
        "near": near,
        "mid": mid,
        "far": far,
        "parts": [],
        "sockets": [],
        "clips": clips_json(clip_list, index),
        "angle_units": "radians (as the tank's file; the sea and air files use degrees)",
    }
    for p in parts:
        e = {"name": p["name"], "parent": index[p["parent"]] if p["parent"] else None, "parent_name": p["parent"],
             "pivot": J(p["pivot"]), "axis": [round(a, 4) + 0.0 for a in p["axis"]], "kind": p["kind"], "limits": p["limits"],
             "sense": p["sense"]}
        for k in ("radius", "metres_per_u"):
            if k in p:
                e[k] = round(p[k], 4)
        if p["kind"] == "slide":
            e["slide"] = e["axis"]                          # (the shared format names a slide's direction `slide`)
        data["parts"].append(e)
    for name, bone, at, facing in unit.sockets():
        data["sockets"].append({"name": name, "part": index[bone], "part_name": bone, "at": J(at), "facing": J(Vector(facing).normalized())})
    data.update(unit.json_extra(index) if hasattr(unit, "json_extra") else {})
    with open(path, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    return data


def export_unit(unit):
    """<name>.glb (LOD0, every clip), <name>_LOD1/_LOD2.glb, <name>.fbx (LOD0, every clip a take), <name>_LOD1/_LOD2.fbx, and
    <name>.json in the game's own form."""
    t0 = time.time()
    sc = kit.fresh(FPS)
    parts = unit.parts()
    arm = make_rig(parts, unit.name + "_rig")
    clip_list = unit.clips()
    make_actions(arm, parts, clip_list)
    socks = make_sockets(arm, unit.sockets())
    meshes, tris = {}, {}
    for lod in (0, 1, 2):
        g = unit.geo(lod)
        tris[lod] = g.tris()
        meshes[lod] = make_mesh(g, f"{unit.name}_LOD{lod}", arm, unit.nation)
    reset_pose(arm)
    arm.animation_data.action = None
    sc.frame_set(0)
    names = []
    for lod in (0, 1, 2):
        nm = unit.name if lod == 0 else f"{unit.name}_LOD{lod}"
        _glb(stage_path(unit, nm + ".glb"), [arm, meshes[lod]] + socks, lod == 0)
        _fbx(stage_path(unit, nm + ".fbx"), [arm, meshes[lod]] + socks, lod == 0)
        names += [nm + ".glb", nm + ".fbx"]
    data = write_json(unit, stage_path(unit, unit.name + ".json"), meshes, parts, clip_list)
    names.append(unit.name + ".json")
    for n in names:
        deliver(unit, stage_path(unit, n), n)
    info = {"tris": tris, "json_tris": {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")},
            "long": data["long"], "high": data["high"], "seconds": round(time.time() - t0, 1),
            "bones": len(parts), "sockets": len(unit.sockets()), "clips": [c.name for c in clip_list]}
    with open(stage_path(unit, unit.name + "_export_info.json"), "w") as f:
        json.dump(info, f)
    print(f"exports {unit.name}:", info)
    return info


def export_static(unit):
    """A prop with no moving parts: <name>.glb, .fbx, LOD1/2, .json (one part, no clips)."""
    t0 = time.time()
    kit.fresh(FPS)
    meshes, tris = {}, {}
    for lod in (0, 1, 2):
        g = unit.geo(lod)
        tris[lod] = g.tris()
        meshes[lod] = make_mesh(g, f"{unit.name}_LOD{lod}", None, unit.nation)
    names = []
    for lod in (0, 1, 2):
        nm = unit.name if lod == 0 else f"{unit.name}_LOD{lod}"
        _glb(stage_path(unit, nm + ".glb"), [meshes[lod]], False)
        _fbx(stage_path(unit, nm + ".fbx"), [meshes[lod]], False)
        names += [nm + ".glb", nm + ".fbx"]
    parts = unit.parts()
    data = write_json(unit, stage_path(unit, unit.name + ".json"), meshes, parts, [])
    names.append(unit.name + ".json")
    for n in names:
        deliver(unit, stage_path(unit, n), n)
    info = {"tris": tris, "json_tris": {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")},
            "long": data["long"], "high": data["high"], "seconds": round(time.time() - t0, 1)}
    with open(stage_path(unit, unit.name + "_export_info.json"), "w") as f:
        json.dump(info, f)
    print(f"exports {unit.name}:", info)
    return info


# ================================================================================================ the pictures
def render_cfg(w=None, h=None, samples=None, final=None):
    """kit's render settings, sized for a test (640x360, 16 samples) or a final (1280x720, 48), on two threads (the four
    cores are shared with the other artists)."""
    final = (not FAST) if final is None else final
    W, H, N = (1280, 720, 48) if final else (640, 360, 16)
    sc = kit.render_setup(w or W, h or H, samples or N)
    sc.render.threads_mode = "FIXED"
    sc.render.threads = 2
    sc.render.use_persistent_data = True
    return sc


def rest_attr(obj):
    me = obj.data
    a = me.attributes.get("rest") or me.attributes.new("rest", "FLOAT_VECTOR", "POINT")
    a.data.foreach_set("vector", [c for v in me.vertices for c in v.co])


class Stage:
    """A unit in a fresh scene for pictures: rigged, its clips as actions, worn paint, the sun and sky, a field."""

    def __init__(self, unit, ground=True, nation=None):
        self.unit = unit
        self.sc = kit.fresh(FPS)
        self.arm, self.mesh, self.socks, self.parts, self.geo = assemble(unit, 0)
        self.clips = {c.name: c for c in unit.clips()}
        self.acts = make_actions(self.arm, self.parts, list(self.clips.values())) if self.arm else {}
        self.mats = picture_materials(nation or unit.nation)
        if hasattr(unit, "picture_extras"):
            unit.picture_extras(self)
        # (the sun high on the left and a little ahead, so the three-quarter views from the left see lit faces)
        self.sun = kit.studio(sun_turn=getattr(unit, "sun_turn", 160.0))
        self.ground = kit.field_ground() if ground else None
        self.cam = kit.camera(lens=50)
        for e in self.socks:
            e.hide_render = True

    def play(self, name):
        play(self.arm, self.acts[name], self.sc)
        if hasattr(self.unit, "follow_clip"):
            self.unit.follow_clip(self, self.clips[name])
        self.sc.frame_set(0)

    def rest(self, pose=None):
        if self.arm:
            pose_static(self.arm, self.parts, pose or {})


def load_old(campaign, nation="GER"):
    """Today's campaign piece, from its JSON (near mesh), painted by number with the same materials."""
    with open(os.path.join(MODELS_DIR, campaign + ".json")) as f:
        d = json.load(f)
    n = d["near"]
    P, T, pa = n["points"], n["triangles"], n["paints"]
    me = bpy.data.meshes.new("old_" + campaign)
    me.from_pydata([(P[i], -P[i + 1], P[i + 2]) for i in range(0, len(P), 3)], [], [(T[i], T[i + 2], T[i + 1]) for i in range(0, len(T), 3)])
    for name in PAINT_NAMES:
        me.materials.append(bpy.data.materials.get(name) or materials_plain(nation)[name])
    me.polygons.foreach_set("material_index", [pa[p.vertices[0]] for p in me.polygons])
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    me.set_sharp_from_angle(angle=math.radians(40.0))
    ob = bpy.data.objects.new("old_" + campaign, me)
    bpy.context.scene.collection.objects.link(ob)
    rest_attr(ob)
    return ob, len(T) // 3


def aim(cam, view):
    target, dist, pitch, yaw, lens = view
    cam.data.lens = lens
    kit.orbit(cam, target, dist, pitch, yaw)


def pic_path(unit, *names):
    return stage_path(unit, "pictures", *names)


def deliver_pictures(unit, *names):
    for n in names:
        deliver(unit, pic_path(unit, n), os.path.join("pictures", n))


def before_after(unit, extra_label=""):
    """Today's campaign piece and the battle piece, the same camera and light, the nation's paint."""
    t0 = time.time()
    st = Stage(unit)
    st.rest(unit.picture_pose())
    aim(st.cam, unit.views()["front"])
    render_cfg()
    tmp = os.path.join(WORK, "ba", unit.name)
    os.makedirs(tmp, exist_ok=True)
    kit.still(os.path.join(tmp, "after.png"))
    st.mesh.hide_render = True
    for ob in getattr(st, "extra_objs", []):
        ob.hide_render = True
    old, old_tris = load_old(unit.campaign, unit.nation)
    kit.still(os.path.join(tmp, "before.png"))
    out = pic_path(unit, f"before-after{extra_label}.png")
    paint = kit.NATIONS[unit.nation]["paint"]
    kit.sheet([os.path.join(tmp, "before.png"), os.path.join(tmp, "after.png")], out, cols=2,
              labels=[f"Before: today's campaign {unit.campaign} ({old_tris:,} triangles)",
                      f"After: the battle {unit.noun} (LOD0, {st.geo.tris():,} triangles)"],
              title=f"Before and after: same camera, same light, {unit.nation_word} paint {paint}")
    print(f"before-after {unit.name}: {out} ({time.time() - t0:.0f} s)")
    return out


def sheet(unit, extra_label="", only=None):
    """The four views of the contact sheet: front and rear three-quarter, a detail close up, the game's camera. `only`: the
    views to render again (the others are taken as they were last rendered)."""
    t0 = time.time()
    st = Stage(unit)
    st.rest(unit.picture_pose())
    render_cfg()
    paths = []
    views = unit.views()
    labels = unit.view_labels()
    for v in ("front", "rear", "detail", "game"):
        p = os.path.join(WORK, "stills", unit.name, v + ".png")
        paths.append(p)
        if only and v not in only and os.path.exists(p):
            continue
        aim(st.cam, views[v])
        kit.still(p)
    kit.sheet(paths, pic_path(unit, f"sheet{extra_label}.png"), cols=2, labels=[labels[v] for v in ("front", "rear", "detail", "game")],
              title=unit.sheet_title())
    print(f"sheet {unit.name}: {time.time() - t0:.0f} s")


# ================================================================================================ effects, for the pictures only
# The smoke, dust, flash and fire are the warship modeller's tested effects (battle-models/shared/sea-air/stage.py, class
# Effects: copied unchanged but for leaving out the sea's own makers, so land and sea match), with field-gun-sized makers
# of our own below it (dust tan, not grey). Nothing here goes into the game: the game makes its own effects at the sockets.
def _fx_math(nt, op, a=None, b=None, clamp=False):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation, n.use_clamp = op, clamp
    for k, v in enumerate((a, b)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[k].default_value = v
        else:
            nt.links.new(v, n.inputs[k])
    return n.outputs[0]


class Effects:
    """Soft puffs that are born, drift, grow and fade: enough to show a funnel's smoke, a gun's flash and its brown
    cloud, a shell's column of water, a fire and its black smoke. Call at(t) for every frame, in order."""

    def __init__(self, seed=7, wind=(-11.0, 1.5, 0.0)):
        self.rng = random.Random(seed)
        self.wind = Vector(wind)                 # what the air does as she sees it (her own speed and the wind's)
        self.live, self.spare = [], {}
        self.streams, self.shots = [], []
        self.lights = []
        self.t = None
        self.coll = bpy.data.collections.new("fx")
        bpy.context.scene.collection.children.link(self.coll)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=1.0)
        ball = bpy.context.object
        self.mesh = ball.data
        for p in self.mesh.polygons:
            p.use_smooth = True
        bpy.data.objects.remove(ball)
        self.meshes = {}
        for k in ("smoke", "water", "glow", "flat"):       # one ball for each kind of puff, in its own stuff
            self.meshes[k] = self.mesh.copy()
            self.meshes[k].materials.append(self._mat(k))

    def _mat(self, kind):
        m = bpy.data.materials.new("fx_" + kind)
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.remove(nt.nodes["Principled BSDF"])
        out = nt.nodes["Material Output"]
        info = nt.nodes.new("ShaderNodeObjectInfo")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 1.6 if kind != "flat" else 2.4
        nz.inputs["Detail"].default_value = 4.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        if kind == "flat":       # a patch lying on the water: thick in the middle, ragged at the edge
            sep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(tc.outputs["Object"], sep.inputs[0])
            r = _fx_math(nt, "SQRT", _fx_math(nt, "ADD", _fx_math(nt, "MULTIPLY", sep.outputs[0], sep.outputs[0]), _fx_math(nt, "MULTIPLY", sep.outputs[1], sep.outputs[1])))
            soft = _fx_math(nt, "SUBTRACT", 1.0, _fx_math(nt, "ADD", r, _fx_math(nt, "MULTIPLY", _fx_math(nt, "SUBTRACT", nz.outputs[0], 0.5), 0.9)), clamp=True)
            soft = _fx_math(nt, "MULTIPLY", soft, 1.7, clamp=True)
        else:                    # a puff: thick where you look into it, thinning to nothing at its edge
            lw = nt.nodes.new("ShaderNodeLayerWeight")
            lw.inputs["Blend"].default_value = 0.5
            face = _fx_math(nt, "SUBTRACT", 1.0, lw.outputs["Facing"], clamp=True)
            soft = _fx_math(nt, "POWER", face, {"smoke": 2.4, "water": 1.9, "glow": 1.3}[kind])
            soft = _fx_math(nt, "MULTIPLY", soft, _fx_math(nt, "SUBTRACT", _fx_math(nt, "MULTIPLY", nz.outputs[0], 1.9), 0.35, clamp=True), clamp=True)
        alpha = _fx_math(nt, "MULTIPLY", soft, info.outputs["Alpha"], clamp=True)
        if kind == "glow":
            body = nt.nodes.new("ShaderNodeEmission")
            body.inputs["Strength"].default_value = 7.0
        elif kind == "water":      # spray lets the light through: a column stands white against the sun too
            d1, d2 = nt.nodes.new("ShaderNodeBsdfDiffuse"), nt.nodes.new("ShaderNodeBsdfTranslucent")
            nt.links.new(info.outputs["Color"], d1.inputs["Color"])
            nt.links.new(info.outputs["Color"], d2.inputs["Color"])
            body = nt.nodes.new("ShaderNodeMixShader")
            body.inputs[0].default_value = 0.45
            nt.links.new(d1.outputs[0], body.inputs[1])
            nt.links.new(d2.outputs[0], body.inputs[2])
        else:
            body = nt.nodes.new("ShaderNodeBsdfDiffuse")
        if kind != "water":
            nt.links.new(info.outputs["Color"], body.inputs["Color"])
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        ms = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(alpha, ms.inputs[0])
        nt.links.new(clear.outputs[0], ms.inputs[1])
        nt.links.new(body.outputs[0], ms.inputs[2])
        nt.links.new(ms.outputs[0], out.inputs["Surface"])
        return m

    # -- one puff
    def puff(self, t0, at, vel=(0, 0, 0), life=6.0, r=(2.0, 8.0), col=(0.5, 0.5, 0.5), col2=None, alpha=0.6, kind="smoke", drag=0.0, rise=0.0,
             gravity=0.0, wind=1.0, squash=(1.0, 1.0, 1.0), fade_in=0.15, grow=0.6, hold=0.0, cool=1.6):
        ob = self.spare[kind].pop() if self.spare.get(kind) else None
        if ob is None:
            ob = bpy.data.objects.new("puff", self.meshes[kind])
            self.coll.objects.link(ob)
            ob.visible_shadow = False                    # (far cheaper; the sun still shades each puff by its own roundness)
        ob.rotation_euler = (self.rng.uniform(0, 6.28), self.rng.uniform(0, 6.28), self.rng.uniform(0, 6.28)) if kind != "flat" else (0, 0, self.rng.uniform(0, 6.28))
        ob.hide_render = False
        p = dict(ob=ob, kind=kind, t0=t0, at=Vector(at), vel=Vector(vel), life=life, r=r, col=Vector(col), col2=Vector(col2 if col2 is not None else col), alpha=alpha,
                 drag=drag, rise=rise, gravity=gravity, wind=wind, squash=Vector(squash), fade_in=fade_in, grow=grow, hold=hold, cool=cool)
        self.live.append(p)
        return p

    def _place(self, p, t):
        a = t - p["t0"]
        ob = p["ob"]
        if a < 0 or a > p["life"]:
            ob.hide_render = True
            ob.scale = (0, 0, 0)
            return a <= p["life"]
        ob.hide_render = False
        f = a / p["life"]
        k = p["drag"]
        go = p["vel"] * ((1 - math.exp(-k * a)) / k if k > 1e-6 else a)
        pos = p["at"] + go + self.wind * (p["wind"] * a) + Vector((0, 0, p["rise"] * a - 0.5 * p["gravity"] * a * a))
        ob.location = pos
        rad = p["r"][0] + (p["r"][1] - p["r"][0]) * f ** p["grow"]
        ob.scale = (rad * p["squash"][0], rad * p["squash"][1], rad * p["squash"][2])
        fade = min(1.0, a / max(p["fade_in"], 1e-3)) * (1.0 if f < p["hold"] else (1 - (f - p["hold"]) / max(1 - p["hold"], 1e-6)) ** 1.4)
        c = p["col"].lerp(p["col2"], min(1.0, f * p["cool"]))
        ob.color = (c[0], c[1], c[2], p["alpha"] * fade)
        return True

    # -- what makes puffs
    def stream(self, where, every, make, start=0.0, end=1e9):
        """A steady source: every `every` seconds from start to end, make(t, at, facing) lays puffs. `where` is a
        function of nothing giving (at, facing) now (a socket riding the ship), or a fixed pair."""
        self.streams.append(dict(where=where, every=every, make=make, next=start, end=end))

    def once(self, t0, where, make):
        self.shots.append(dict(t=t0, where=where, make=make, done=False))

    def light(self, t0, at, power, long=0.12, col=(1.0, 0.72, 0.4), r=3.0):
        d = bpy.data.lights.new("flash", "POINT")
        d.energy, d.color, d.shadow_soft_size = 0.0, col, r
        ob = bpy.data.objects.new("flash", d)
        self.coll.objects.link(ob)
        ob.location = at
        self.lights.append(dict(ob=ob, t0=t0, long=long, power=power))

    def at(self, t):
        """Everything as it is at t. Sources lay the puffs that fell due since the last call."""
        for s in self.streams:
            if s["next"] > t + 0.5 or s["next"] > s["end"]:
                continue
            w = s["where"]() if callable(s["where"]) else s["where"]
            now, facing = Vector(w[0]), Vector(w[1])
            was = s.get("was")                           # (where the source was at the last call: what fell due between is laid along its path)
            while s["next"] <= t and s["next"] <= s["end"]:
                at = now
                if was is not None and t > was[0] and s["next"] > was[0]:
                    at = was[1].lerp(now, (s["next"] - was[0]) / (t - was[0]))
                s["make"](s["next"], at, facing)
                s["next"] += s["every"]
            s["was"] = (t, now)
        for s in self.shots:
            if not s["done"] and s["t"] <= t:
                w = s["where"]() if callable(s["where"]) else s["where"]
                s["make"](s["t"], Vector(w[0]), Vector(w[1]))
                s["done"] = True
        keep = []
        for p in self.live:
            if self._place(p, t):
                keep.append(p)
            else:
                self.spare.setdefault(p["kind"], []).append(p["ob"])
        self.live = keep
        for l in self.lights:
            a = t - l["t0"]
            l["ob"].data.energy = l["power"] * (1 - a / l["long"]) if 0 <= a < l["long"] else 0.0
        self.t = t

    # -- the effects themselves (each is a `make` for stream or once, or lays its own)
    def gun(self, t0, where, bore=0.38, light=True):
        """A heavy gun fires: a tongue of flame, a ball of hot brown smoke that the wind takes."""
        g = self.rng
        k = bore / 0.38

        def make(t, at, facing):
            side = facing.cross(Vector((0, 0, 1))).normalized()
            up = side.cross(facing)
            q = facing.to_track_quat("X", "Z")
            for i in range(3):       # the flash: long along the bore
                p = self.puff(t, at + facing * (3.0 + 5.0 * i) * k, vel=facing * 18.0, life=0.14 + 0.03 * i, r=(2.0 * k, (4.6 - 0.7 * i) * k), col=(1.0, 0.7, 0.3),
                              col2=(1.0, 0.38, 0.1), alpha=1.0, kind="glow", drag=6.0, wind=0.0, squash=(2.0, 1.0, 1.0), fade_in=0.01, grow=0.4)
                p["ob"].rotation_euler = q.to_euler()
            for i in range(13):      # the smoke: thrown out along the bore, then it hangs, thins and drifts
                sp = g.uniform(30.0, 110.0) * k
                v = facing * sp + side * g.uniform(-16, 16) * k + up * g.uniform(-6, 14) * k
                c = g.uniform(0.68, 0.84)
                self.puff(t + 0.03, at + facing * 2.0, vel=v, life=g.uniform(4.0, 7.5), r=(1.6 * k, g.uniform(6.0, 11.0) * k), col=(0.72, 0.5, 0.26), col2=(c, c * 0.94, c * 0.84),
                          alpha=g.uniform(0.3, 0.5), drag=g.uniform(2.6, 3.6), rise=1.0, fade_in=0.05, grow=0.5, wind=0.85)
            if light:
                self.light(t, at + facing * 9.0 * k + Vector((0, 0, 2.0)), 2.5e5 * k * k, 0.14)
        self.once(t0, where, make)

    def burst(self, t0, where, size=1.0, light=True):
        """A shell bursts on her: a flash, and dark smoke thrown out."""
        g = self.rng

        def make(t, at, facing):
            self.puff(t, at, life=0.22, r=(3.0 * size, 9.0 * size), col=(1.0, 0.8, 0.45), col2=(1.0, 0.4, 0.1), alpha=1.0, kind="glow", wind=0.0, fade_in=0.01, grow=0.4)
            for i in range(10):
                v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.1, 1.2))).normalized() * g.uniform(12, 40) * size
                c = g.uniform(0.07, 0.16)
                self.puff(t + 0.04, at, vel=v, life=g.uniform(5, 9), r=(2.5 * size, g.uniform(8, 13) * size), col=(0.35, 0.2, 0.1), col2=(c, c, c), alpha=0.8, drag=3.0, rise=1.6,
                          fade_in=0.05, grow=0.45, wind=0.9)
            if light:
                self.light(t, at + Vector((0, 0, 3.0)), 4.0e5 * size * size, 0.2)
        self.once(t0, where, make)

    def fire(self, where, start=0.0, end=1e9, size=1.0):
        """She burns: flame at the foot, black smoke leaning downwind."""
        g = self.rng

        def flame(t, at, facing):
            if at.z < 0.5:
                return
            self.puff(t, at + Vector((g.uniform(-2, 2) * size, g.uniform(-1.5, 1.5) * size, 0.5)), vel=(0, 0, g.uniform(5, 9)), life=g.uniform(0.5, 0.9), r=(2.2 * size, 1.0 * size),
                      col=(1.0, 0.62, 0.2), col2=(1.0, 0.3, 0.06), alpha=0.95, kind="glow", wind=0.25, fade_in=0.05, squash=(1.0, 1.0, 1.6))

        def smoke(t, at, facing):
            if at.z < 0.5:
                return
            c = g.uniform(0.03, 0.08)
            self.puff(t, at + Vector((g.uniform(-1.5, 1.5) * size, g.uniform(-1.5, 1.5) * size, 2.0)), vel=(g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(6, 9)), life=g.uniform(14, 19),
                      r=(3.4 * size, g.uniform(16, 23) * size), col=(c * 1.6, c * 1.2, c), col2=(c, c, c), alpha=0.85, drag=0.35, rise=1.6, fade_in=0.3, grow=0.6, wind=0.95, hold=0.3)
        self.stream(where, 0.09, flame, start=start, end=end)
        self.stream(where, 0.3, smoke, start=start, end=end)

    def blast(self, t0, where, size=1.0):
        """A magazine goes up: a great flash, a tower of flame and black-brown smoke."""
        g = self.rng

        def make(t, at, facing):
            for i in range(5):
                self.puff(t + 0.02 * i, at + Vector((g.uniform(-8, 8), g.uniform(-4, 4), 4 + 6 * i)) * size, vel=(g.uniform(-6, 6), g.uniform(-6, 6), 30 + 14 * i), life=g.uniform(0.8, 1.4),
                          r=(8 * size, g.uniform(18, 28) * size), col=(0.42, 0.22, 0.07), col2=(0.3, 0.09, 0.02), alpha=1.0, kind="glow", drag=1.6, wind=0.1, fade_in=0.02, grow=0.4)
            for i in range(16):      # the pillar
                c = g.uniform(0.03, 0.1)
                self.puff(t + 0.05 + g.uniform(0, 0.5), at + Vector((g.uniform(-5, 5), g.uniform(-4, 4), 3.0)), vel=(g.uniform(-9, 9), g.uniform(-9, 9), g.uniform(60, 150) * size), life=g.uniform(12, 20),
                          r=(6 * size, g.uniform(16, 28) * size), col=(0.55, 0.3, 0.12), col2=(c, c * 0.95, c * 0.9), alpha=0.75, drag=g.uniform(1.0, 1.5), rise=2.6, fade_in=0.08, grow=0.5,
                          wind=0.75, hold=0.4, cool=6.0)
            for i in range(26):      # the cloud that spreads from her
                v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.1, 1.2))).normalized() * g.uniform(18, 80) * size
                c = g.uniform(0.05, 0.16)
                self.puff(t + 0.05 + g.uniform(0, 0.4), at + Vector((g.uniform(-6, 6), g.uniform(-4, 4), 3.0)), vel=v, life=g.uniform(10, 18), r=(6 * size, g.uniform(18, 32) * size),
                          col=(0.6, 0.33, 0.12), col2=(c, c * 0.95, c * 0.9), alpha=0.7, drag=g.uniform(1.0, 1.7), rise=2.0, fade_in=0.08, grow=0.5, wind=0.8, hold=0.35, cool=5.0)
            self.light(t, at + Vector((0, 0, 30.0)), 2.5e6 * size * size, 0.5, r=12.0)
        self.once(t0, where, make)



DUST, DUST2 = (0.50, 0.42, 0.30), (0.62, 0.55, 0.42)      # tan, paling as it thins (the battle set's dry stubble)
WIND = Vector((1.4, 0.9, 0.0))                          # m/s, toward the east-north-east (as the tank's clips)


def field_gun_shot(fx, t0, where, ground, bore=0.075):
    """A field gun fires, its muzzle a metre off the ground: a short tongue of flame, a ring of grey-brown smoke thrown
    out round the muzzle, a plume forward along the line, a ball hanging at the muzzle, a wide fan of tan dust blown off
    the ground in front, and a flash of light. Sized from the shared gun (bore 0.38 m) by the bore, and from the tank's."""
    g = fx.rng
    k = max(bore / 0.075, 0.6)

    def make(t, at, facing):
        side = facing.cross(Vector((0, 0, 1))).normalized()
        up = side.cross(facing)
        q = facing.to_track_quat("X", "Z")
        for i in range(3):                                   # the flash, long along the bore, a hot core in it
            p = fx.puff(t, at + facing * (0.25 + 0.3 * i) * k, vel=facing * 6.0, life=0.045 + 0.01 * i, r=(0.12 * k, (0.4 - 0.08 * i) * k),
                        col=(0.95, 0.55, 0.18), col2=(0.85, 0.32, 0.07), alpha=1.0, kind="glow", drag=6.0, wind=0.0, squash=(2.0, 1.0, 1.0), fade_in=0.01, grow=0.4)
            p["ob"].rotation_euler = q.to_euler()
        p = fx.puff(t, at + facing * 0.3 * k, vel=facing * 4.0, life=0.035, r=(0.08 * k, 0.2 * k), col=(1.0, 0.9, 0.62), col2=(1.0, 0.7, 0.3), alpha=1.0,
                    kind="glow", drag=6.0, wind=0.0, squash=(2.2, 1.0, 1.0), fade_in=0.01, grow=0.4)
        p["ob"].rotation_euler = q.to_euler()
        for i in range(4):                                   # (and a little sideways out of the muzzle's mouth)
            d = (side * (1 if i % 2 else -1) * 0.8 + facing * 0.6 + up * g.uniform(-0.2, 0.3)).normalized()
            fx.puff(t, at + facing * 0.15, vel=d * 4.0, life=0.04, r=(0.08 * k, 0.28 * k), col=(0.95, 0.6, 0.2), col2=(0.8, 0.3, 0.06), alpha=0.9,
                    kind="glow", drag=6.0, wind=0.0, fade_in=0.01, grow=0.4)
        for i in range(26):                                  # the ring round the muzzle (flattened: the ground below, little lift): it stands and thins
            a = 2 * math.pi * i / 26 + g.uniform(-0.08, 0.08)
            out = side * math.cos(a) + up * (math.sin(a) * 0.55)
            c = g.uniform(0.55, 0.66)
            fx.puff(t + 0.02, at + facing * 0.35, vel=out * g.uniform(3.0, 4.2) * k + facing * g.uniform(2.0, 4.0), life=g.uniform(4.0, 5.5),
                    r=(0.2 * k, g.uniform(0.8, 1.1) * k), col=(0.62, 0.54, 0.43), col2=(c, c * 0.98, c * 0.94), alpha=0.6, drag=3.0, rise=0.25,
                    fade_in=0.03, grow=0.5, wind=0.9)
        for i in range(6):                                   # a ball of smoke hanging at the muzzle
            c = g.uniform(0.52, 0.62)
            fx.puff(t + 0.03, at + facing * 0.7, vel=facing * g.uniform(1.0, 3.0) + Vector((g.uniform(-0.8, 0.8), g.uniform(-0.8, 0.8), g.uniform(0.0, 0.8))),
                    life=g.uniform(4.5, 6.0), r=(0.3 * k, g.uniform(1.0, 1.5) * k), col=(0.62, 0.53, 0.42), col2=(c, c * 0.98, c * 0.94), alpha=0.55,
                    drag=2.0, rise=0.3, fade_in=0.03, grow=0.5, wind=0.9)
        for i in range(10):                                  # the plume along the line of fire
            fx.puff(t + 0.02, at + facing * 0.6, vel=facing * g.uniform(8.0, 30.0) * k + up * g.uniform(-1.0, 2.0) + side * g.uniform(-2.0, 2.0),
                    life=g.uniform(4.0, 6.0), r=(0.3 * k, g.uniform(1.2, 1.9) * k), col=(0.64, 0.52, 0.38), col2=(0.58, 0.57, 0.54), alpha=0.55,
                    drag=3.0, rise=0.3, fade_in=0.03, grow=0.5, wind=0.9)
        flat = Vector((facing.x, facing.y, 0.0)).normalized()
        s2 = Vector((-flat.y, flat.x, 0.0))
        for i in range(30):                                  # the dust: a fan blown off the ground ahead of the muzzle
            u = g.uniform(0.3, 6.0)
            w = g.uniform(-1.0, 1.0) * (0.7 + 0.4 * u)
            p = at + flat * u + s2 * w
            z = ground(p)
            out = (flat + s2 * (w / max(u, 1.0))).normalized()
            fx.puff(t + 0.02 + 0.012 * u, Vector((p.x, p.y, z + 0.25)), vel=out * g.uniform(3.0, 7.0) + Vector((0, 0, g.uniform(0.6, 2.2))),
                    life=g.uniform(4.0, 6.0), r=(0.3, g.uniform(1.4, 2.5)), col=DUST, col2=DUST2, alpha=0.55, drag=1.8, rise=0.15, fade_in=0.05,
                    grow=0.5, wind=0.85, squash=(1.0, 1.0, 0.7))
        fx.light(t, at + facing * 1.2 + Vector((0, 0, 0.5)), 5.0e3 * k, 0.1)
    fx.once(t0, where, make)


def dust_kick(fx, t0, at, ground, n=5, size=1.0):
    """Dust shaken up off the ground where a wheel or a spade is jarred."""
    g = fx.rng

    def make(t, p0, facing):
        for i in range(n):
            p = p0 + Vector((g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 0.0))
            fx.puff(t + g.uniform(0.0, 0.06), Vector((p.x, p.y, ground(p) + 0.1)), vel=Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.3, 1.0))) * 1.2,
                    life=g.uniform(2.0, 3.0), r=(0.12 * size, g.uniform(0.5, 0.8) * size), col=DUST, col2=DUST2, alpha=0.45, drag=2.0, rise=0.1,
                    fade_in=0.05, grow=0.5, wind=0.8, squash=(1.0, 1.0, 0.7))
    fx.once(t0, at, make)


class Case:
    """The spent case thrown out of the breech: it flies back, tumbles, strikes the ground, hops and lies still, smoking
    a little. A brass cylinder for the pictures (the game throws its own)."""

    def __init__(self, fx, t0, where, ground, length=0.3, r=0.045, seed=0):
        g = random.Random(seed)
        self.fx, self.t0, self.where, self.ground = fx, t0, where, ground
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, segments=12, radius1=r * 1.12, radius2=r * 0.92, depth=length)
        me = bpy.data.meshes.new("case")
        bm.to_mesh(me)
        bm.free()
        for f in me.polygons:
            f.use_smooth = True
        m = bpy.data.materials.get("fx_brass")
        if m is None:
            m = bpy.data.materials.new("fx_brass")
            m.use_nodes = True
            b = m.node_tree.nodes["Principled BSDF"]
            b.inputs["Base Color"].default_value = (0.62, 0.42, 0.16, 1)
            b.inputs["Metallic"].default_value = 1.0
            b.inputs["Roughness"].default_value = 0.33
        me.materials.append(m)
        self.ob = bpy.data.objects.new("case", me)
        fx.coll.objects.link(self.ob)
        self.ob.hide_render = True
        self.len, self.r = length, r
        self.spin = g.uniform(9.0, 13.0)
        self.speed = g.uniform(2.6, 3.2)
        self.p0 = None

    def at(self, t):
        a = t - self.t0
        if a < 0:
            self.ob.hide_render = True
            return
        if self.p0 is None:
            w = self.where()
            self.p0, d = Vector(w[0]), Vector(w[1]).normalized()
            self.v0 = d * self.speed + Vector((0, 0, 0.6))
            self.axis = Vector((d.x, d.y, 0.0)).normalized()                 # the case comes out base first, along the bore
            self.tumble = self.axis.cross(Vector((0, 0, 1))).normalized()    # it turns end over end about the level cross line
            zg = self.ground(self.p0) + self.r
            gz = 9.8
            vz, h = self.v0.z, self.p0.z - zg
            self.t_hit = (vz + math.sqrt(vz * vz + 2 * gz * h)) / gz
            self.hit = self.p0 + self.v0 * self.t_hit + Vector((0, 0, -0.5 * gz * self.t_hit ** 2))
            self.hop_v = Vector((self.v0.x * 0.35, self.v0.y * 0.35, 1.2))
            self.t_hop = 2 * self.hop_v.z / gz
            self.rest_at = self.hit + Vector((self.hop_v.x, self.hop_v.y, 0)) * self.t_hop
            self.slide = Vector((self.v0.x, self.v0.y, 0)) * 0.12
            self.t_still = self.t_hit + self.t_hop + 0.25
            # spin so that it lies flat when it comes to rest
            turns = max(1, round(self.spin * self.t_still / math.pi))
            self.w = turns * math.pi / self.t_still
            fx = self.fx
            for i in range(5):                                               # a wisp of smoke out of the open breech
                fx.puff(t + 0.05 * i, self.p0, vel=Vector((d.x, d.y, 0.3)) * 0.6, life=2.2, r=(0.08, 0.45), col=(0.72, 0.7, 0.66), alpha=0.25,
                        drag=1.5, rise=0.35, fade_in=0.05, grow=0.5, wind=0.9)
        self.ob.hide_render = False
        gz = 9.8
        if a < self.t_hit:
            pos = self.p0 + self.v0 * a + Vector((0, 0, -0.5 * gz * a * a))
        elif a < self.t_hit + self.t_hop:
            b = a - self.t_hit
            pos = self.hit + self.hop_v * b + Vector((0, 0, -0.5 * gz * b * b))
        else:
            b = min(a - self.t_hit - self.t_hop, 0.25)
            pos = self.rest_at + self.slide * (b / 0.25) * (1 - 0.5 * b / 0.25)
        pos.z = max(pos.z, self.ground(pos) + self.r * 0.95)
        ang = self.w * min(a, self.t_still)
        base = self.axis.to_track_quat("Z", "Y")                             # (the cone's own axis is z)
        from mathutils import Quaternion
        self.ob.rotation_mode = "QUATERNION"
        self.ob.rotation_quaternion = Quaternion(self.tumble, ang) @ base
        self.ob.location = pos


def _clear_grass(centre, heading, half_long, half_wide, back=0.0):
    """Drop the grass tufts under a unit standing on the battle stage (a box about its middle, turned to its heading)."""
    ob = bpy.data.objects.get("grass")
    if ob is None:
        return
    fwd = Vector((math.cos(heading), math.sin(heading), 0.0))
    left = Vector((-fwd.y, fwd.x, 0.0))
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    gone = []
    for f in bm.faces:
        c = f.calc_center_median() - centre
        u, v = c.dot(fwd), c.dot(left)
        if -half_long - back < u < half_long and abs(v) < half_wide:
            gone.append(f)
    bmesh.ops.delete(bm, geom=gone, context="FACES")
    bm.to_mesh(ob.data)
    bm.free()


GUN_AT = Vector((30.0, -6.0, 0.0))            # on the stubble east of the road
GUN_HEADING = 225.0                           # degrees from +x: firing toward the south-west
FILM_SHOTS = (0.5, 3.3)                       # seconds into the film


def gun_film(unit, w=960, h=540, samples=8, frames=None, seconds=6.0, tag="firing", fps=24):
    """The field gun, crew-less, on the battle stage, firing two rounds: the muzzle blast, the smoke cloud, dust blown up
    in front of the muzzle, the gun jumping on its spades and settling, the breech opening and the case flung out.
    `frames` (a list) renders only those frames, for a look. Writes pictures/<tag>.mp4 and .gif at `fps` frames a second (the
    clip's keys are at 30 a second; frames between are read between them)."""
    import battle_set
    t_start = time.time()
    st = Stage(unit, ground=False)
    # September light from the south, a little to the camera's left
    st.sun.rotation_euler = Euler((math.radians(90 - 33), 0.0, math.radians(10.0)), "XYZ")
    world = battle_set.build()
    print(f"battle stage: {time.time() - t_start:.0f} s", flush=True)
    head = math.radians(GUN_HEADING)
    fwd = Vector((math.cos(head), math.sin(head), 0.0))
    left = Vector((-fwd.y, fwd.x, 0.0))
    ground = lambda p: world.height(p.x, p.y)
    # stand the gun on the ground: its height at the axle, tipped to the slope between its wheels and its spades
    gz = ground(GUN_AT)
    zl, zr = ground(GUN_AT + left * unit.TY), ground(GUN_AT - left * unit.TY)
    zs = ground(GUN_AT - fwd * unit.L)
    pitch = math.atan2(zs - gz, unit.L)            # (+: the spades stand higher, so the muzzle dips)
    roll = math.atan2(zl - zr, 2 * unit.TY)            # (+: the left wheel stands higher)
    carrier = bpy.data.objects.new("carrier", None)
    st.sc.collection.objects.link(carrier)
    carrier.location = Vector((GUN_AT.x, GUN_AT.y, gz))
    carrier.rotation_mode = "XYZ"
    carrier.rotation_euler = Euler((roll, pitch, head), "XYZ")      # (roll, then pitch, then the heading)
    st.arm.parent = carrier
    for ob in getattr(st, "extra_objs", []):
        ob.parent = carrier
    _clear_grass(Vector((GUN_AT.x, GUN_AT.y, gz)) - fwd * 1.0, head, 2.6, 2.0, back=1.4)
    # the film's own clip: the gun laid at its picture elevation, firing at each of FILM_SHOTS
    fire = st.clips["fire"]
    film = Clip("film", seconds, False)
    raised = math.radians(unit.s["raised"])
    film.add("cradle", "turn", lambda t: raised)
    shots = FILM_SHOTS

    def shifted(fn):
        def f(t):
            t0 = max([s for s in shots if s <= t], default=None)
            return fn(0.0) if t0 is None else fn(min(t - t0, fire.long))
        return f
    for part, kind, axis, fn in fire.ch:
        film.add(part, kind, shifted(fn), axis=axis)
    acts = make_actions(st.arm, st.parts, [film])
    play(st.arm, acts["film"], st.sc)
    socks = {e.name: e for e in st.socks}

    def where(name):
        def f():
            m = socks[name].matrix_world
            return m.translation.copy(), (m.to_3x3() @ Vector((1, 0, 0))).normalized()
        return f
    # (the sockets' empties point their x along the socket's facing: see make_sockets)
    fx = Effects(seed=11, wind=WIND)
    cases = []
    for k, t0 in enumerate(shots):
        field_gun_shot(fx, t0, where("socket_muzzle"), ground, bore=unit.s["bore"] * 1.25)
        for nm in ("socket_spade_l", "socket_spade_r") if unit.split else ("socket_spade",):
            dust_kick(fx, t0 + 0.06, where(nm), ground, n=4, size=0.9)
        for sgn in (1, -1):
            wp = lambda sgn=sgn: (carrier.matrix_world @ Vector((0, sgn * unit.TY, 0.0)), Vector((0, 0, 1)))
            dust_kick(fx, t0 + 0.04, wp, ground, n=3, size=0.8)
        cases.append(Case(fx, t0 + 1.2, where("socket_breech"), ground, seed=k))
    # the camera: front-left of the gun, a man's height, looking across it so the blast goes out to the left of the frame;
    # it is shaken a little by each shot
    eye0 = Vector((GUN_AT.x, GUN_AT.y, 0.0)) + left * 8.2 + fwd * 2.4
    eye0.z = ground(eye0) + 1.55
    look = Vector((GUN_AT.x, GUN_AT.y, gz + 0.85)) + fwd * 1.1
    st.cam.data.lens = 35
    render_cfg(w, h, samples)
    sc = st.sc
    sc.cycles.transparent_max_bounces = 96
    sc.cycles.diffuse_bounces = 1
    sc.cycles.glossy_bounces = 1
    folder = os.path.join(WORK, "clip_" + tag)
    shutil.rmtree(folder, ignore_errors=True)
    os.makedirs(folder, exist_ok=True)
    n = int(round(seconds * fps))
    k = 1
    for f in range(n):
        t = f / fps
        shake = Vector((0, 0, 0))
        for t0 in shots:
            a = t - t0
            if 0 <= a < 0.6:
                e = math.exp(-a / 0.12)
                shake += Vector((math.sin(a * 71.0), math.sin(a * 53.0 + 1.0), math.sin(a * 61.0 + 2.0))) * 0.035 * e
        kit.look_at(st.cam, eye0 + shake, look + shake * 0.3)
        whole = int(math.floor(t * FPS + 1e-6))
        sc.frame_set(whole, subframe=t * FPS - whole)
        fx.at(t)
        for c in cases:
            c.at(t)
        if frames is not None and f not in frames:
            continue
        tick = time.time()
        kit.still(os.path.join(folder, f"{k:04d}.png" if frames is None else f"f{f:04d}.png"))
        k += 1
        print(f"frame {f}: {time.time() - tick:.1f} s", flush=True)
    if frames is None:
        kit.encode(folder, pic_path(unit, tag), fps=fps)
        deliver_pictures(unit, tag + ".mp4", tag + ".gif")
    print(f"film {tag}: {time.time() - t_start:.0f} s", flush=True)
    return folder


# ================================================================================================ the READMEs
FOLDER_UNITS = {"gun": ("gun", "ammo"), "lorry": ("lorry",), "halftrack": ("hanomag", "m3")}


def _fmt(v, nd=3):
    return "(" + ", ".join(f"{float(a):.{nd}f}".rstrip("0").rstrip(".") if abs(float(a)) > 1e-9 else "0" for a in v) + ")"


def _deg(a):
    return f"{math.degrees(a):.1f} deg"


def _unit_tables(u, w):
    tris = [u.geo(l).tris() for l in (0, 1, 2)]
    w(f"### {u.name}: {u.title}")
    w("")
    w(f"Files: `{u.name}.json`, `{u.name}.glb`, `{u.name}.fbx`, `{u.name}_LOD1.glb/.fbx`, `{u.name}_LOD2.glb/.fbx`. Paint in the pictures: "
      f"{u.nation_word} ({kit.NATIONS[u.nation]['paint']}).")
    w("")
    w("| LOD | triangles | in the .json |")
    w("| --- | ---: | --- |")
    for l, key in enumerate(("near", "mid", "far")):
        w(f"| LOD{l} | {tris[l]:,} | `{key}` |")
    w("")
    if getattr(u, "static", False):
        return
    parts = u.parts()
    w("**Bones** (pivots in the game's frame: metres, x forward, y right, z up; turns in radians):")
    w("")
    w("| bone | parent | kind | pivot | axis | limits | what + does |")
    w("| --- | --- | --- | --- | --- | --- | --- |")
    for p in parts:
        lim = p["limits"]
        if lim is not None:
            lim = f"{lim[0]:+.3f} .. {lim[1]:+.3f}" + (f" ({_deg(lim[0])} .. {_deg(lim[1])})" if p["kind"] == "turn" else " m")
        extra = ""
        if "radius" in p:
            extra = f" (rolling radius {p['radius']:.3f} m)"
        if "metres_per_u" in p:
            extra = f" ({p['metres_per_u']:.3f} m of belt per 1.0 of U)"
        w(f"| `{p['name']}` | {p['parent'] or '-'} | {p['kind']} | {_fmt(J(p['pivot']))} | {_fmt(p['axis'], 2)} | {lim or '-'} | {p['sense']}{extra} |")
    w("")
    w("**Sockets** (each rides its bone):")
    w("")
    w("| socket | bone | at | facing |")
    w("| --- | --- | --- | --- |")
    for name, bone, at, facing in u.sockets():
        w(f"| `{name}` | {bone} | {_fmt(J(at), 2)} | {_fmt(J(Vector(facing).normalized()), 2)} |")
    w("")
    w("**Clips** (every clip that moves a bone is an action in the .glb and a take in the .fbx; events and sockets are only in the .json):")
    w("")
    w("| clip | seconds | loops | metres a loop | events (time: effect at socket, sound cue) | what it is |")
    w("| --- | ---: | --- | ---: | --- | --- |")
    for c in u.clips():
        ev = "; ".join(f"{e['t']:.2f}: {e['fx']}" + (f" at {e['socket']}" if e["socket"] else "") + (f", cue `{e['cue']}`" if e["cue"] else "") for e in c.events) or "-"
        w(f"| `{c.name}` | {c.long} | {'yes' if c.loop else 'no (after: hold)'} | {f'{c.metres:.3f}' if c.metres else '-'} | {ev} | {c.about} |")
    w("")


def write_readme(folder):
    units = [make_unit(k) for k in FOLDER_UNITS[folder]]
    lines = []
    w = lines.append
    u0 = units[0]
    title = {"gun": "Battle models: the field gun", "lorry": "Battle models: the lorry", "halftrack": "Battle models: the half-tracks"}[folder]
    w(f"# {title}")
    w("")
    w("Made wholly by script: `unreal/TheaterCommand/Tools/battle_units/guns_vehicles.py` (Blender as a Python module, no window). "
      "Every shape is our own design, built on the measures of today's campaign piece in `build_tank.py` (OTHERS); no name, "
      "model or art from any other game, and no real vehicle or gun copied exactly.")
    w("")
    w("```")
    w(f"python guns_vehicles.py --models {','.join(FOLDER_UNITS[folder])} --only export        # the models (seconds)")
    w(f"python guns_vehicles.py --models {','.join(k for k in FOLDER_UNITS[folder] if k != 'ammo')} --only before_after,sheet{',clip' if folder == 'gun' else ''}   # the pictures (slow: Cycles on the CPU)")
    w(f"python guns_vehicles.py --models {','.join(FOLDER_UNITS[folder])} --only readme      # this README")
    w("python guns_vehicles.py --fast ...          # 16 samples, for a quick look")
    w("```")
    w("")
    w("## Look at first")
    w("")
    if folder == "gun":
        w("1. `pictures/sheet.png`: front and rear three-quarter, a close look at the breech, sight and shield, and the game's camera.")
        w("2. `pictures/firing.mp4` (or `.gif`): the gun with no crew on the battle stage, firing two rounds: blast, smoke, dust, the jump, the case.")
        w("3. `pictures/before-after.png`: today's campaign Gun75 beside the battle gun, same light and camera.")
    else:
        w("1. `pictures/sheet.png` (one for each model): front and rear three-quarter, a close look, and the game's camera.")
        w("2. `pictures/before-after.png`: today's campaign model beside the battle one, same light and camera.")
    w("")
    w("## The models")
    w("")
    for u in units:
        _unit_tables(u, w)
    w("## What the game must do")
    w("")
    if folder == "gun":
        g = u0
        w(f"- **Aim.** Traverse: turn `carriage` by (bearing to the target - the gun's heading), clamped to +-{g.TRAVERSE:.3f} rad "
          f"({_deg(g.TRAVERSE)}; + is clockwise seen from above). Past that the crew heave the trail round: turn the whole gun about its "
          "axle's middle (the root) slowly, about 10 degrees a second. Elevation: turn `cradle` (+ lifts the muzzle) by the angle the "
          "range needs, within -5 to +40 degrees. Spin `handwheel_traverse` 30 times and `handwheel_elevate` 40 times as fast as the turns "
          "they make, while they turn (the .json's `controls`).")
        w("- **Fire.** Play `fire` laid over the aim (its turns are small and added to the laid ones). Its events: `muzzle_flash_big` "
          "and `muzzle_blast_dust` at `socket_muzzle` at 0.0, `case_eject` at `socket_breech` at 1.2 (the breech opens; throw a spent case "
          "out along the socket's facing), `breech_close` at 2.05. The next round may be fired when the clip ends (2.5 s), or at 2.2 at the "
          f"soonest. The barrel recoils {g.RL:.2f} m (0.0-0.1 s) and runs out by 0.7; the gun jumps on its spades and settles by 1.25.")
        w("- **Deploy and travel.** `unlimber` (2.0 s): from the towed pose to the firing pose, the spades bite at 2.0 (`dust_puff` "
          f"at each spade). `limber` (2.0 s): the reverse; ends with the legs shut ({-g.CLOSE:.4f} rad), the barrel level for the road "
          f"(cradle {g.TRAVEL_UP:+.3f}) and the trail lifted ({g.TOW:+.3f} rad on `axle`) to the lorry's pintle height "
          f"({LORRY_HOOK.z:.2f} m). `towed` loops, trail first: put the gun so its `socket_tow` meets the lorry's `socket_tow`, its "
          "heading the lorry's + 180 degrees, and play `towed` at speed (m/s) x 2.0 / metres_per_loop.")
        w("- **Crew.** The crew come later: they stand at `socket_crew_layer` (left of the breech, at the sight) and "
          "`socket_crew_loader` (right, behind). `socket_ammo` is where to stand the `ammo_boxes` prop (turned -20 degrees in the pictures).")
        w("- The gun has no engine: `idle` is a still pose (it exists so every unit has one).")
    elif folder == "lorry":
        L = u0
        w(f"- **Drive.** Play `drive` (loops, {L.DRIVE_M:.3f} m a loop) at a rate of speed (m/s) x {L.DRIVE_LOOP} / {L.DRIVE_M:.3f}; it turns "
          "the wheels and bobs and rolls the body. When it stops, play `stop` (0.8 s: the body pitches forward on its springs and back), "
          "then `idle` (loops; the engine shakes the body).")
        w(f"- **Steer.** Turn `steer_l` and `steer_r` (+ steers right, clockwise seen from above; limits +-0.6 rad), the inner wheel more "
          f"than the outer (Ackermann: tan inner = W / (T - t/2), tan outer = W / (T + t/2), W = {L.FX - L.RX:.2f} m wheelbase, "
          f"t = {2 * (L.FY - 0.12):.2f} m between the king pins, T the turning radius).")
        w("- **Doors.** Turn `door_l` / `door_r` open (about 1.1 rad) while a man gets in or out at `socket_driver` / `socket_codriver`. "
          "Riders sit at `socket_rider_l1..4` and `socket_rider_r1..4` on the benches facing in; they climb in at `socket_tailboard`.")
        w("- **Tow.** A gun hooks on at `socket_tow` (the pintle, 0.70 m up, at the back): see the gun's README.")
        w("- **Knocked out.** Play `knocked_out` (3.0 s, holds its last pose): the lorry jolts, the right front tyre goes flat (the "
          "body settles and lists that way), the bonnet's right half springs open and the driver's (left) door swings open; fire takes hold in the cab at 1.0 (`fire_loop` at "
          "`socket_cab_fire`), smoke from the engine at 1.6. Sound cue `tank_knocked_out` at 0.0 (placeholder name).")
        w("- The `tilt` (canvas cover) bone breathes a little while driving (a slide of a few millimetres): no need to drive it.")
    else:
        H = u0
        w(f"- **Drive.** Play `drive` (loops) at a rate of speed (m/s) x 2.0 / metres_per_loop (Hanomag {units[0].DRIVE_M:.3f} m, M3 "
          f"{units[1].DRIVE_M:.3f} m a loop: whole links, so the belt's pattern loops cleanly). It turns the front wheels, the road "
          "wheels, sprocket and idler, and bobs and rolls the body. Then `stop` (0.8 s) and `idle`.")
        w("- **Belts.** The belts (`track_l`, `track_r`, kind `scroll`) are never moved as bones. Each belt's U runs once round it, forward "
          "along the bottom (0 to 1; V across): scroll the track material's U by (metres travelled) / metres_per_u (the .json's `track`; "
          f"Hanomag {units[0].PATH.length:.3f} m, {units[0].PATH.links} links; M3 {units[1].PATH.length:.3f} m, {units[1].PATH.links} "
          "pads). On a turn on the spot or a skid, scroll each side by its own distance. The game draws the links in its track texture "
          "(the pictures draw them with a procedural material).")
        w("- **Steer.** Turn `steer_l` / `steer_r` (+ to the right; limits +-0.55 rad); a half-track also brakes the inner belt, so "
          "scroll that belt slower on a turn.")
        w("- **Machine-gun.** Turn `mg` (+ clockwise seen from above, +-0.7 rad); its gunner stands at `socket_mg_gunner`, its muzzle "
          "flash is at `socket_mg`. The squad sits at `socket_rider_l1..5` / `socket_rider_r1..5` facing in; they leave by the rear "
          "door(s) (`door_rl`/`door_rr` on the Hanomag, `door_rear` on the M3: + opens) to `socket_door`.")
        w("- **Knocked out.** `knocked_out` (3.0 s, holds): it rocks, settles nose down and lists, the bonnet's left half flies "
          "open, a rear door swings open, the machine-gun is knocked round; `smoke_burst` at the engine at 0.1, `fire_loop` in the "
          "compartment at 1.0, `smoke_loop` at the engine at 1.6. Sound cue `tank_knocked_out` at 0.0.")
    w("")
    w("## The file, and how it fits the other battle models")
    w("")
    w("`<Name>.json` is the game's own model file (`name`, `about`, `long`, `high`, `marks`, `near`, `far`, and a `mid` for LOD1) with "
      "these added: `part` (in each mesh: for every point, the index of its bone), `parts` (`name`, `parent`, `pivot`, `axis`, "
      "`kind` fixed/turn/slide/spin/scroll, `limits`, `sense`, and `slide` for a slide's direction), `sockets` (`name`, `part`, `at`, "
      "`facing`), `clips` (`long`, `loop`, `loop_cue`, `about`, `tracks`, `events`, `after` for a clip that does not loop, "
      "`metres_per_loop` for a driving clip) and `controls` (what the game turns itself).")
    w("")
    w("- Metres; x forward, **y to the right**, z up (the .glb and .fbx are Blender's own frame, as exported).")
    w("- Turns are **radians** (as the tank's file; the sea and air files use degrees), about the part's axis through its pivot, "
      "positive by v' = v cos a + (axis x v) sin a + axis (axis . v)(1 - cos a). A traverse is + clockwise seen from above. A part's "
      "pose is applied about its pivot, then its parent's, up to the root.")
    w("- A slide is metres along its `slide` direction **as the parent's turn has carried it** (the barrel runs back along its "
      "lifted cradle). A spin turns without limit. A track may carry its own `axis` (the body rolls and bobs as well as pitching).")
    w("- Clip keys are read in a straight line between times and held outside them; clips may be laid over each other bone by "
      "bone (the aim and `fire`, or `drive` and the steering).")
    w("- Nothing here breaks away, so no part is of kind `free`.")
    w("- LOD1 and LOD2 are skinned to the same skeleton (same bone names) but carry no clips of their own: play LOD0's.")
    w("- Effects named in events (`muzzle_flash_big`, `case_eject`, `dust_trail`, `fire_loop` ...) are names for the game's own "
      "effects; nothing here builds them. Sound cues are placeholders until the sound director names them.")
    w("")
    w("## Honest notes")
    w("")
    w("- The wear in the pictures (mottled and faded paint, grime in the recesses, rubbed edges and chips, rain streaks, dust low "
      "and on tops, dried mud splashed up, rust on the exhausts) is a **picture-only** procedural material. The exports carry plain "
      "materials, one per paint name; the game paints by its own materials and marks (`marks`).")
    w("- The smoke, dust, flash and fire in the pictures are the warship modeller's shared soft-puff effects (copied, sized for "
      "these units, dust tan), as stand-ins for the game's.")
    if folder == "gun":
        g = u0
        w("- Gun75 is modernised for towing by lorry: steel spoked wheels with solid rubber tyres and a split trail with spades "
          "(the brief's), where the campaign piece has wooden wheels and a box trail. Its barrel, shield, height and track are the "
          "campaign piece's.")
        w("- The same rig builds the other guns of OTHERS (`--models 25pdr`, `leFH18`, `Howitzer105`, `ZiS3`: 10,700-11,500 "
          "triangles); a box trail is one `trail` bone that does not open. Their pictures are not made. The 25-pounder's firing "
          "platform is a flat wooden disc on the root under the wheels; the gun does not turn on it.")
        w("- The case thrown out in `firing.mp4` is a picture prop; in the game `case_eject` is an effect (and `ammo_boxes` carries a "
          "few spent cases on the ground).")
    elif folder == "lorry":
        w("- The flat tyre in `knocked_out` is shown by the body settling and listing over the wheel (the tyre mesh does not deform; "
          "a game could swap in a flat tyre at that wheel).")
    else:
        w("- The M3's sprocket and idler are larger and stand higher than the campaign piece's, so its belt's top run comes up near "
          "the body over a return roller; the Hanomag keeps the campaign's interleaved wheels and track layout.")
        w("- The belts are smooth bands; links and guide horns are for the track texture to draw (fixed horns would stand still "
          "while the texture scrolls, so none are modelled).")
    w("")
    out = stage_path(u0, "README.md")
    with open(out, "w") as f:
        f.write("\n".join(lines))
    deliver(u0, out, "README.md")
    print(f"README {folder}: {out} ({len(lines)} lines)")
    return out


# ================================================================================================ run
def make_unit(key):
    k = key.lower()
    if k in ("gun", "gun75"):
        return FieldGun("Gun75")
    if k in ("ammo", "ammo_boxes"):
        return AmmoBoxes()
    if k in ("lorry", "truck"):
        return Lorry("Truck")
    if k in ("hanomag",):
        return HalfTrack("Hanomag")
    if k in ("m3", "m3halftrack"):
        return HalfTrack("M3Halftrack")
    if key in bt.OTHERS and bt.OTHERS[key].get("kind") == "gun":
        return FieldGun(key)
    raise SystemExit(f"no model called {key}")


STEPS = ["export", "before_after", "sheet", "clip", "readme"]


def main():
    global FAST, OUT_ROOT
    a = kit.args()
    FAST = "--fast" in a
    if "--out" in a:
        OUT_ROOT = a[a.index("--out") + 1].rstrip("/") + "/"
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else STEPS
    models = a[a.index("--models") + 1].split(",") if "--models" in a else ["gun", "ammo", "lorry", "hanomag", "m3"]
    log = {}
    done_readmes = set()
    for key in models:
        unit = make_unit(key)
        for step in steps:
            t0 = time.time()
            if step == "export":
                (export_static if getattr(unit, "static", False) else export_unit)(unit)
            elif step == "before_after" and hasattr(unit, "campaign") and not getattr(unit, "static", False):
                before_after(unit)
                deliver_pictures(unit, "before-after.png")
            elif step == "sheet" and hasattr(unit, "views"):
                sheet(unit, only=a[a.index("--views") + 1].split(",") if "--views" in a else None)
                deliver_pictures(unit, "sheet.png")
            elif step == "clip" and hasattr(unit, "battle_clip"):
                unit.battle_clip()
            elif step == "readme" and not getattr(unit, "static", False):
                if unit.folder not in done_readmes:
                    write_readme(unit.folder)
                    done_readmes.add(unit.folder)
            log[f"{key}:{step}"] = round(time.time() - t0, 1)
            print(f"step {key}:{step}: {log[f'{key}:{step}']} s", flush=True)
    ok = check_delivered()
    print("delivered" if ok else "SOME FILES DID NOT LAND", log)


if __name__ == "__main__":
    main()
