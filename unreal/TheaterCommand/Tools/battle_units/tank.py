"""THE TANK for the battles: the game's generic medium tank, built up for a close camera, rigged and animated.

Run without Blender's window (the bpy module, Blender 5.0.1):

    python tank.py                         everything: model, rig, clips, exports, pictures, notes
    python tank.py --only export           the exports alone (tank.glb, tank_LOD1/2.glb, tank.fbx, tank_LOD1/2.fbx, tank.json)
    python tank.py --only stills,turntable,clips --fast      test pictures (640x360, 16 samples)
    (steps: export, before_after, stills, turntable, clips, readme; --out FOLDER changes where they go)

It starts from the campaign piece: build_tank.py's SPEC with VARIANTS["MediumTank"] laid over it, and build_tank.build()
itself makes the hull, the waist, the nose, the six-sided turret, the mantlet and the grilles, so the battle tank and
the campaign piece are the same tank in the same measures. (build_tank.py is read, never changed: its own last line
runs it, so it is loaded here without that line.) Everything that a close camera finds wanting is then added or
remade here: dual road wheels on rubber tyres with hub caps and bolt circles, a toothed sprocket and its final-drive
housing, the idler and return rollers, track links with grousers and guide horns (one link made and laid round the
track's path), weld seams, bolted and hinged hatches, periscopes and vision blocks, a bow machine-gun, headlamps with
guards, mudguards, the silencer with two pipes, a tow cable, tools, spare links, a stowage bin with a tarpaulin, and a
whip aerial in three bones.

Three meshes are made, LOD0 (about 25,000 triangles), LOD1 (about 6,000) and LOD2 (about 1,500), each skinned rigidly to
one armature (each point to one bone; the belts and the aerial blend). Materials are named by paint (build_tank.PAINTS),
so the game colours each nation's tank from its own table. Conventions are the kit's: metres, +X forward, Z up, Blender's
y to the LEFT (the JSON's y to the right), in-place clips, one action per clip, sockets as empties named socket_<name>.
"""

import bpy  # (first: the bpy module puts Blender's own script folders on the path)
import bmesh
import json
import math
import os
import shutil
import sys
import time
import types

import numpy as np
from mathutils import Euler, Matrix, Quaternion, Vector

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
S = dict(bt.SPEC)
S.update(bt.VARIANTS["MediumTank"])
PAINT_NAMES = sorted(bt.PAINTS, key=bt.PAINTS.get)          # paint, shade, track, rubber, wood, steel, canvas, glass, rust, stripe

OUT = "/mnt/project-files/battle-models/land/tank/"
WORK = "/home/user/art/tank/"
OLD_JSON = os.path.normpath(os.path.join(TOOLS, "..", "Content", "Models", "MediumTank.json"))
FPS = 30
NATION = "GER"

# ------------------------------------------------------------------------------------------------ the measures
# All from the campaign piece (S), named here once. Blender's frame: x forward, y LEFT, z up, metres.
DECK = S["hull_deck"]                                   # 1.0: the hull's deck, where the sponsons start
WAIST_TOP = DECK + S["waist"]                           # 1.3: top of the upright band, foot of the glacis
ROOF = DECK + S["upper_high"]                           # 1.86: the hull roof
UPPER_X = -0.12                                         # (build_tank places the upper hull so)
FRONT = UPPER_X + S["upper_long"] * 0.5                 # 2.53: the waist's front face
REAR = UPPER_X - S["upper_long"] * 0.5                  # -2.77: its rear face
GLACIS_TOP = FRONT - S["glacis"]                        # 1.43: where the glacis meets the roof
TAIL_TOP = REAR + S["tail"]                             # -2.22: where the tail plate meets the roof
SIDE = S["upper_wide"] * 0.5                            # 1.35: the sponson sides
TRACK_Y = S["track_out"]                                # 1.15: the middle of each track
TRACK_W = S["track_wide"]                               # 0.40
TZ = ROOF + 0.02                                        # 1.88: the turret's foot
THIGH = S["turret_high"]                                # 0.6
TROOF = TZ + THIGH                                      # 2.48: the turret's roof
TW, TL = S["turret_wide"] / 1.9, S["turret_long"] / 2.25
GX = S["turret_x"] + S["turret_long"] * 0.5 * 0.92 - 0.16      # 1.09: the gun's root, as build_tank has it
GZ = TZ + THIGH * 0.52                                  # 2.192: the bore's height
CAL = S["gun_calibre"]
GUN_END = GX + 0.24 + S["gun_long"]                     # 3.13: the muzzle
WHEEL_Z = S["wheel_r"] + 0.05                           # 0.41: road wheels' axles
WHEEL_XS = sorted(bt.wheel_places(S), reverse=True)     # front first
ROLLER_XS = sorted(S["rollers"], reverse=True)
G_LEN = math.hypot(S["glacis"], S["waist"] - S["upper_high"])
G_ALONG = Vector((-S["glacis"], 0.0, S["upper_high"] - S["waist"])).normalized()   # up the glacis
G_OUT = Vector((G_ALONG.z, 0.0, -G_ALONG.x))                                       # out of the glacis (forward and up)
T_ALONG = Vector((S["tail"], 0.0, S["upper_high"] - S["waist"])).normalized()      # up the tail plate
T_OUT = Vector((-T_ALONG.z, 0.0, T_ALONG.x))

# The turret's six corners (build_tank's "hex"), at its foot and at its roof: (x, y) in plan.
HEX_FOOT = [(1.443, 0.0), (0.722, 0.866), (-0.722, 0.866), (-1.443, 0.0), (-0.722, -0.866), (0.722, -0.866)]
HEX_ROOF = [(x * S["turret_taper"], y * S["turret_taper"]) for x, y in HEX_FOOT]

# The track: link pitch is chosen so a whole number of links goes round; the drive clip moves a whole number of links.
LINK_PITCH = 0.14
DRIVE_LOOP = 2.0                                        # seconds
DRIVE_LINKS = 21                                        # links the belt moves in one drive loop (21 x 0.1407 = 2.96 m: 1.48 m/s)


def glacis(d, y, h=0.0):
    """A point d metres up the glacis from its foot, y to the left, h out of the plate."""
    return Vector((FRONT, y, WAIST_TOP)) + G_ALONG * d + G_OUT * h


def pick(lod, *v):
    return v[min(lod, len(v) - 1)]


# ------------------------------------------------------------------------------------------------ geometry
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


def bolt(g, at, normal, bone, r=0.014, h=0.012, mat="shade", sides=6, spin=0.0):
    """A hex-headed bolt standing out of a plate."""
    g.put(bm_lathe([(r, -0.003), (r, h)], sides, cap1=True), mat, bone, toward(at, normal, spin))


def bolt_ring(g, centre, normal, radius, count, bone, r=0.013, h=0.011, mat="shade", phase=0.0, ref=None, sides=6):
    n = Vector(normal).normalized()
    a = Vector(ref) if ref is not None else (Vector((0, 0, 1)) if abs(n.z) < 0.9 else Vector((1, 0, 0)))
    a = (a - n * a.dot(n)).normalized()
    b = n.cross(a)
    for k in range(count):
        t = phase + 2 * math.pi * k / count
        bolt(g, Vector(centre) + (a * math.cos(t) + b * math.sin(t)) * radius, n, bone, r, h, mat, sides, spin=t)


def bolt_row(g, a, b, count, normal, bone, r=0.012, h=0.01, mat="shade", sides=6):
    a, b = Vector(a), Vector(b)
    for k in range(count):
        bolt(g, a.lerp(b, (k + 0.5) / count), normal, bone, r, h, mat, sides)


def weld(g, a, b, out, bone, wide=0.011, high=0.008, seg=None, sink=0.0):
    """A weld bead along a joint, standing out of it along `out`."""
    a, b, out = Vector(a), Vector(b), Vector(out).normalized()
    length = (b - a).length
    seg = seg or max(2, int(length / 0.3))
    pts, ups = [], []
    for k in range(seg + 1):
        t = k / seg
        wob = 0.0012 * math.sin(k * 2.3)                  # (a hand weld is never quite straight)
        pts.append(a.lerp(b, t) - out * sink + out.cross(b - a).normalized() * wob)
        ups.append(out)
    prof = [(x, y * high / wide) for x, y in ((-1.0, -0.3), (-0.55, 0.75), (0.55, 0.75), (1.0, -0.3))]
    g.put(bm_tube(pts, wide, profile=prof, up=ups, open_profile=True, caps=False), "shade", bone)


def smoothstep(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


# ------------------------------------------------------------------------------------------------ the track's path
class TrackPath:
    """The line the inside of the track follows, seen from the side (x forward, z up): under the road wheels, up round
    the sprocket in front, back over the return rollers and down round the idler. Taken counter-clockwise (forward
    along the bottom), as the U of the belt runs."""

    def __init__(self):
        circles = [(S["idler_x"], S["idler_z"], S["idler_r"])]
        circles += [(x, WHEEL_Z, S["wheel_r"]) for x in sorted(WHEEL_XS)]
        circles += [(S["sprocket_x"], S["sprocket_z"], S["sprocket_r"])]
        circles += [(x, S["roller_z"], S["roller_r"]) for x in ROLLER_XS]
        n = len(circles)
        tang = []                                           # (leave circle i at p1, reach circle i+1 at p2)
        for i in range(n):
            (x1, z1, r1), (x2, z2, r2) = circles[i], circles[(i + 1) % n]
            dx, dz = x2 - x1, z2 - z1
            L = math.hypot(dx, dz)
            ux, uz = dx / L, dz / L
            rx, rz = uz, -ux                                # (to the right of the way of travel: outside the loop)
            a = (r1 - r2) / L
            b = math.sqrt(max(1.0 - a * a, 0.0))
            nx, nz = a * ux + b * rx, a * uz + b * rz
            tang.append(((x1 + r1 * nx, z1 + r1 * nz), (x2 + r2 * nx, z2 + r2 * nz)))
        pts = []
        for i in range(n):
            cx, cz, r = circles[i]
            p_in = tang[i - 1][1]
            p_out = tang[i][0]
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
        keep = np.concatenate([[True], d > 1e-6])
        P = P[keep]
        d = np.hypot(np.diff(P[:, 0]), np.diff(P[:, 1]))
        self.px, self.pz = P[:, 0], P[:, 1]
        self.cum = np.concatenate([[0.0], np.cumsum(d)])
        self.length = float(self.cum[-1])
        self.links = int(round(self.length / LINK_PITCH))
        self.pitch = self.length / self.links
        self.drive_links = DRIVE_LINKS
        self.drive_metres = self.drive_links * self.pitch     # metres one drive loop covers (whole links)
        self.teeth = int(round(2 * math.pi * S["sprocket_r"] / self.pitch))
        self.sprocket_pitch_r = self.teeth * self.pitch / (2 * math.pi)

    def at(self, s):
        s = np.mod(s, self.length)
        return np.interp(s, self.cum, self.px), np.interp(s, self.cum, self.pz)


PATH = TrackPath()


def link_template(lod=0):
    """One track link in its own frame: x along the track (the pitch), y out of the loop, z across the track. A shoe with
    a grouser across its middle, a guide horn standing in toward the wheels, and the pin's end-connector on each side."""
    p = PATH.pitch
    g = 0.008
    half = TRACK_W * 0.5
    parts = []
    shoe = [(-p / 2 + g, 0.0), (p / 2 - g, 0.0), (p / 2 - g, 0.032), (0.026, 0.035), (0.019, 0.05), (-0.019, 0.05),
            (-0.026, 0.035), (-p / 2 + g, 0.032)]
    parts.append((bm_prism(shoe, TRACK_W), "track"))
    if lod == 0:
        horn = bm_box((0.07, 0.085, 0.05), shape=lambda q: Vector((q.x * (0.55 if q.y < 0 else 1.0), q.y, q.z * (0.7 if q.y < 0 else 1.0))))
        bmesh.ops.translate(horn, vec=(0.0, -0.0425 + 0.002, 0.0), verts=horn.verts)
        bmesh.ops.delete(horn, geom=[f for f in horn.faces if f.normal.y > 0.9], context="FACES_ONLY")   # (its foot is in the shoe)
        parts.append((horn, "track"))
        # (the pin's end on the outer edge only: the inner one faces the hull and is never seen)
        pin = bm_box((0.034, 0.03, 0.022))
        bmesh.ops.translate(pin, vec=(p / 2 - 0.012, 0.016, half + 0.008), verts=pin.verts)
        bmesh.ops.delete(pin, geom=[f for f in pin.faces if f.normal.z < -0.9], context="FACES_ONLY")
        parts.append((pin, "track"))
    verts, faces, mats = [], [], []
    for bm, mat in parts:
        base = len(verts)
        bm.verts.index_update()
        verts += [tuple(v.co) for v in bm.verts]
        for f in bm.faces:
            faces.append([base + v.index for v in f.verts])
            mats.append(mat)
        bm.free()
    return np.array(verts), faces, mats


def link_frames(offset=0.0):
    """Where each link lies for a belt moved `offset` metres (forward drive moves the bottom run back, so links run
    toward -s): centre (x, z), along (x, z), out (x, z), and the path distance of its middle at rest."""
    k = np.arange(PATH.links)
    s0 = k * PATH.pitch - offset
    ax, az = PATH.at(s0)
    bx, bz = PATH.at(s0 + PATH.pitch)
    tx, tz = bx - ax, bz - az
    ln = np.hypot(tx, tz)
    tx, tz = tx / ln, tz / ln
    ox, oz = tz, -tx                                        # out of the loop
    return (ax + bx) * 0.5, (az + bz) * 0.5, tx, tz, ox, oz, k * PATH.pitch + PATH.pitch * 0.5


def hull_share(x, z):
    """How much of a belt point rides with the hull (the sprocket, idler and rollers are on it) rather than staying on
    the ground with the road wheels."""
    return max(smoothstep(0.32, 0.8, z), smoothstep(2.0, 2.45, abs(x)))


def belt_links(g, side, lod=0, cull=True):
    """The belt as links (LOD0): UVs run U from 0 to 1 once round (METRES_PER_U = PATH.length), V across the track."""
    tv, tf, tm = link_template(lod)
    cx, cz, tx, tz, ox, oz, smid = link_frames(0.0)
    yc = side * TRACK_Y
    name = "track_" + ("l" if side > 0 else "r")
    kept = 0
    for k in range(PATH.links):
        X = cx[k] + tv[:, 0] * tx[k] + tv[:, 1] * ox[k]
        Z = cz[k] + tv[:, 0] * tz[k] + tv[:, 1] * oz[k]
        Y = yc + tv[:, 2]
        if cull and np.all(Z > DECK + 0.005) and np.all((X > REAR + 0.04) & (X < FRONT - 0.04)):
            continue                                        # (inside the sponson: never seen)
        Y = tuck(X, Y, Z)
        kept += 1
        verts = list(zip(X, Y, Z))
        uvs = []
        for f in tf:
            uvs.append([((smid[k] + tv[i, 0]) / PATH.length, 0.5 + tv[i, 2] / TRACK_W) for i in f])
        g.add(verts, tf, "track", lambda p, n=name: _belt_w(p, n), uvs=uvs)
    return kept


def tuck(X, Y, Z):
    """The track's outer edge is flush with the sponson's side (track_out + half its width = half the upper hull's width):
    where the belt runs inside the sponson its edge is drawn in a little, so it does not show through the side plate."""
    inside = (Z > DECK - 0.005) & (X > REAR + 0.01) & (X < FRONT - 0.01)
    lim = SIDE - 0.014
    return np.where(inside & (np.abs(Y) > lim), np.sign(Y) * lim, Y)


def _belt_w(p, name):
    w = hull_share(p.x, p.z)
    return {name: 1.0 - w, "hull": w} if w > 0.001 else {name: 1.0}


def belt_band(g, side, segs):
    """The belt as a plain band for LOD1 and LOD2: outside, inside and outer edge, with the same U along it."""
    yc = side * TRACK_Y
    name = "track_" + ("l" if side > 0 else "r")
    s = np.linspace(0.0, PATH.length, segs + 1)
    x, z = PATH.at(s)
    x[-1], z[-1] = x[0], z[0]
    # (outward normal by the neighbours)
    xa, za = PATH.at(s + 0.05)
    xb, zb = PATH.at(s - 0.05)
    tx, tz = xa - xb, za - zb
    ln = np.hypot(tx, tz)
    ox, oz = tz / ln, -tx / ln
    half = TRACK_W * 0.5
    edge_y = yc + side * half
    in_y = yc - side * half
    for k in range(segs):
        u0, u1 = s[k] / PATH.length, s[k + 1] / PATH.length
        p = lambda i, w, y: (x[i] + ox[i] * w, y, z[i] + oz[i] * w)
        k1 = k + 1
        out = [p(k, 0.05, in_y), p(k1, 0.05, in_y), p(k1, 0.05, edge_y), p(k, 0.05, edge_y)]
        inn = [p(k, 0.0, edge_y), p(k1, 0.0, edge_y), p(k1, 0.0, in_y), p(k, 0.0, in_y)]
        rim = [p(k, 0.0, edge_y), p(k, 0.05, edge_y), p(k1, 0.05, edge_y), p(k1, 0.0, edge_y)]
        for quad, uv in ((out, [(u0, 0), (u1, 0), (u1, 1), (u0, 1)]), (inn, [(u0, 1), (u1, 1), (u1, 0), (u0, 0)]),
                         (rim, [(u0, 1), (u0, 1), (u1, 1), (u1, 1)])):
            if side > 0:                                    # (mirrored: turn the face about so it still looks out)
                quad, uv = quad[::-1], uv[::-1]
            g.add(quad, [[0, 1, 2, 3]], "track", lambda q, n=name: _belt_w(q, n), uvs=[uv])


# ------------------------------------------------------------------------------------------------ the model
# Where the moving parts turn (Blender's frame). The rig and the model both read these.
HULL_PIVOT = Vector((0.0, 0.0, 0.9))                    # the hull rocks on its springs about here
TURRET_PIVOT = Vector((S["turret_x"] - 0.1, S["turret_y"], ROOF))
GUN_PIVOT = Vector((0.98, 0.0, GZ))                     # the trunnions, just behind the mantlet
BARREL_ROOT = Vector((1.20, 0.0, GZ))                   # the barrel's breech end (inside the sleeve)
CMDR = Vector((-0.35, -0.36, TROOF))                    # the commander's round hatch (right of the gun)
CMDR_HINGE = CMDR + Vector((-0.265, 0.0, 0.05))
LOADER = Vector((-0.20, 0.36, TROOF))                   # the loader's square hatch (left)
LOADER_HINGE = LOADER + Vector((0.0, 0.175, 0.03))
DRV_D, DRV_Y = 0.55, -0.55                              # the driver's flap on the glacis (right, as the campaign piece has it)
DRV_HINGE = glacis(DRV_D + 0.27, DRV_Y, 0.025)
MG_D, MG_Y = 0.45, 0.55                                 # the bow machine-gun's ball (left)
AERIAL_FOOT = Vector((-0.72, 0.26, TROOF + 0.08))
AERIAL_LEN = 1.6
SILENCER = Vector((-2.90, 0.0, 1.20))
_NOSE_X = S["upper_long"] * 0.5 + UPPER_X + 0.16                        # (build_tank's nose block, its front face sloping back)
NOSE_FOOT = Vector((_NOSE_X + 0.31, 0.0, DECK - 0.19))
NOSE_TOP = Vector((_NOSE_X + 0.31 - 0.217, 0.0, DECK + 0.15))

KEEP = {"hull": "hull", "waist": "hull", "upper": "hull", "nose": "hull", "turret": "turret", "mantlet": "gun", "sleeve": "gun",
        "eye": "turret", "grille": "hull", "louvre": "hull", "tail_plate": "hull", "silencer_bracket": "hull"}


def campaign_parts(g, lod):
    """The campaign piece's own plates, made by build_tank.build() from the same numbers."""
    made = bt.build(S)
    kept = []
    for obj in made:
        base = obj.name.split(".")[0]
        bone = KEEP.get(base)
        if bone is None or (lod == 0 and base == "sleeve") or (lod == 2 and base in ("eye", "louvre", "silencer_bracket")):
            continue
        for mod in obj.modifiers:
            mod.show_viewport = lod == 0                    # (bevelled edges near; plain plates farther off)
        kept.append((obj, bone))
    dg = bpy.context.evaluated_depsgraph_get()
    dg.update()
    for obj, bone in kept:
        bm = bmesh.new()
        bm.from_object(obj, dg)
        bmesh.ops.transform(bm, matrix=obj.matrix_world, verts=bm.verts)
        g.put(bm, PAINT_NAMES[int(obj["paint"])], bone)
    for obj in made:
        me = obj.data
        bpy.data.objects.remove(obj)
        bpy.data.meshes.remove(me)


def road_wheel(g, c, side, bone, lod):
    out = Vector((0.0, side, 0.0))
    turn = Matrix.Rotation(-side * math.pi / 2, 4, "X")     # local z out from the hull
    r = S["wheel_r"]
    if lod == 2:
        g.put(bm_cyl(r, 0.32, 8), "rubber", bone, Matrix.Translation(c) @ turn)
        return
    n = pick(lod, 16, 12)
    for half, k in ((1.0, 0), (-1.0, 1)):                    # the outer wheel, then the inner one of the pair
        m = Matrix.Translation(c + out * (0.10 * half)) @ turn
        if lod == 0:
            tyre = [(r - 0.06, -0.06), (r, -0.05), (r, 0.035), (r - 0.01, 0.06), (r - 0.06, 0.06)]
            if k == 1:
                tyre = [(r - 0.06, -0.06), (r, -0.06), (r, 0.06), (r - 0.06, 0.06)]
        else:
            tyre = [(r - 0.06, -0.06), (r, -0.06), (r, 0.06), (r - 0.06, 0.06)]
        g.put(bm_lathe(tyre, n), "rubber", bone, m)
        if k == 0:
            if lod == 0:
                g.put(bm_lathe([(r - 0.06, 0.06), (r - 0.09, 0.062), (0.17, 0.035), (0.11, 0.04)], n), "paint", bone, m)
                g.put(bm_lathe([(0.11, 0.04), (0.10, 0.075), (0.065, 0.095), (0.0, 0.10)], 10), "paint", bone, m)
                for b in range(8):
                    a = 2 * math.pi * b / 8 + math.pi / 8
                    bolt(g, m @ Vector((0.14 * math.cos(a), 0.14 * math.sin(a), 0.036)), out, bone, 0.013, 0.012, "paint", sides=4, spin=a)   # (square nuts)
                bolt(g, m @ Vector((0.0, 0.0, 0.098)), out, bone, 0.024, 0.014, "shade", sides=8)
            else:
                g.put(bm_lathe([(r - 0.06, 0.06), (0.12, 0.04), (0.0, 0.07)], n), "paint", bone, m)
        else:
            g.put(bm_lathe([(r - 0.06, 0.06), (0.0, 0.06)], n), "paint", bone, m)


def sprocket(g, side, bone, lod):
    c = Vector((S["sprocket_x"], side * TRACK_Y, S["sprocket_z"]))
    out = Vector((0.0, side, 0.0))
    turn = Matrix.Rotation(-side * math.pi / 2, 4, "X")
    teeth = PATH.teeth
    if lod == 2:
        g.put(bm_cyl(0.31, 0.3, 8), "shade", bone, Matrix.Translation(c) @ turn)
        return
    tip, root = 0.322, 0.268
    poly = []
    for k in range(teeth):
        a = 2 * math.pi * k / teeth
        da = 2 * math.pi / teeth
        if lod == 0:
            poly += [(root, a - 0.42 * da), (tip, a - 0.16 * da), (tip, a + 0.16 * da), (root, a + 0.42 * da)]
        else:
            poly += [(root, a - 0.4 * da), (tip, a)]
    poly = [(rr * math.cos(a), rr * math.sin(a)) for rr, a in poly]
    simple = [(rr * math.cos(a), rr * math.sin(a)) for k in range(teeth)
              for rr, a in ((root, 2 * math.pi * (k - 0.4) / teeth), (tip, 2 * math.pi * k / teeth))]
    for half in ((1.0, -1.0) if lod == 0 else (0.0,)):
        m = Matrix.Translation(c + out * (0.10 * half)) @ turn
        # (the inner ring hides behind the outer one: plain pointed teeth do for it)
        g.put(bm_prism(poly if half > 0 else simple, 0.035 if lod == 0 else 0.25), "shade", bone, m)
    m = Matrix.Translation(c) @ turn
    g.put(bm_cyl(0.2, 0.2, pick(lod, 16, 10), caps=False), "paint", bone, m)
    if lod == 0:
        g.put(bm_lathe([(0.2, 0.08), (0.2, 0.125), (0.155, 0.125), (0.14, 0.16), (0.08, 0.185), (0.0, 0.19)], 16), "paint", bone, m)
        # (one bolt to a tooth, and a round boss: a drive loop turns the sprocket by whole teeth, so it loops unseen)
        bolt_ring(g, m @ Vector((0, 0, 0.125)), out, 0.178, teeth, bone, 0.012, 0.011, "shade", sides=5)
        g.put(bm_lathe([(0.03, -0.003), (0.03, 0.008), (0.018, 0.016), (0.0, 0.018)], 12), "shade", bone, m @ Matrix.Translation((0, 0, 0.186)))
        # the final drive's housing on the hull side, its flange bolted round
        hm = Matrix.Translation(Vector((c.x, side * (S["hull_wide"] * 0.5), c.z))) @ turn
        g.put(bm_lathe([(0.27, 0.0), (0.27, 0.025), (0.22, 0.03), (0.21, 0.09), (0.12, 0.11), (0.0, 0.11)], 12), "paint", "hull", hm)
        bolt_ring(g, hm @ Vector((0, 0, 0.025)), out, 0.245, 10, "hull", 0.012, 0.01, "shade", sides=5)
    else:
        g.put(bm_cyl(0.16, 0.36, 8), "paint", bone, m)


def idler(g, side, bone, lod):
    c = Vector((S["idler_x"], side * TRACK_Y, S["idler_z"]))
    out = Vector((0.0, side, 0.0))
    turn = Matrix.Rotation(-side * math.pi / 2, 4, "X")
    r = S["idler_r"]
    if lod == 2:
        g.put(bm_cyl(r, 0.3, 8), "shade", bone, Matrix.Translation(c) @ turn)
        return
    n = pick(lod, 16, 10)
    for half in (1.0, -1.0):
        m = Matrix.Translation(c + out * (0.10 * half)) @ turn
        g.put(bm_lathe([(r - 0.03, -0.055), (r, -0.055), (r, 0.055), (r - 0.03, 0.055)], n), "steel", bone, m)
        if half > 0:
            if lod == 0:
                g.put(bm_lathe([(r - 0.03, 0.05), (0.2, 0.03), (0.09, 0.04)], n), "paint", bone, m)
                g.put(bm_lathe([(0.09, 0.04), (0.075, 0.08), (0.0, 0.085)], 10), "paint", bone, m)
                for b in range(6):
                    a = 2 * math.pi * b / 6
                    at = Vector((0.155 * math.cos(a), 0.155 * math.sin(a), 0.036))
                    g.put(bm_lathe([(0.042, 0.0), (0.0, 0.0)], 10), "shade", bone, m @ Matrix.Translation(at))   # (a lightening hole, dark)
                    bolt(g, m @ Vector((0.06 * math.cos(a + 0.5), 0.06 * math.sin(a + 0.5), 0.07)), out, bone, 0.01, 0.01, "shade", 5)
            else:
                g.put(bm_lathe([(r - 0.03, 0.055), (0.0, 0.07)], n), "paint", bone, m)
        else:
            g.put(bm_lathe([(r - 0.03, 0.055), (0.0, 0.055)], n), "paint", bone, m)
    if lod == 0:
        # the track-adjusting crank from the hull to the idler's axle
        g.put(bm_box((0.36, 0.06, 0.08), 0.008), "shade", "hull",
              place((c.x + 0.16, side * (S["hull_wide"] * 0.5 + 0.03), c.z + 0.05), (0, -25, 0)))


def roller(g, x, side, bone, lod):
    if lod == 2:
        return
    c = Vector((x, side * TRACK_Y, S["roller_z"]))
    turn = Matrix.Rotation(-side * math.pi / 2, 4, "X")
    m = Matrix.Translation(c) @ turn
    r = S["roller_r"]
    g.put(bm_lathe([(r, -0.15), (r, 0.15), (0.06, 0.15)], pick(lod, 12, 8)), "rubber", bone, m)
    if lod == 0:
        g.put(bm_lathe([(0.06, 0.15), (0.05, 0.19), (0.0, 0.19)], 8), "paint", bone, m)


def running_gear(g, lod, belts=True):
    for side in (1.0, -1.0):
        sl = "l" if side > 0 else "r"
        y = side * TRACK_Y
        for i, x in enumerate(WHEEL_XS):
            bone = f"wheel_{sl}{i + 1}"
            road_wheel(g, Vector((x, y, WHEEL_Z)), side, bone, lod)
            if lod < 2:
                # the swing arm between the hull and the wheel's axle, and the bump stop above it
                g.put(bm_box((0.4, 0.07, 0.1), 0.01 if lod == 0 else 0.0), "shade", "root",
                      place((x - 0.17, side * (S["hull_wide"] * 0.5 + 0.05), WHEEL_Z + 0.08), (0, -24, 0)))
                if lod == 0:
                    g.put(bm_cyl(0.055, 0.12, 10), "shade", "root", Matrix.Translation((x, side * 0.99, WHEEL_Z)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
                    g.put(bm_box((0.12, 0.07, 0.08), 0.008), "shade", "hull", place((x - 0.4, side * (S["hull_wide"] * 0.5 + 0.035), 0.78)))
        sprocket(g, side, f"sprocket_{sl}", lod)
        idler(g, side, f"idler_{sl}", lod)
        for i, x in enumerate(ROLLER_XS):
            roller(g, x, side, f"roller_{sl}{i + 1}", lod)
        if not belts:
            continue
        if lod == 0:
            belt_links(g, side, 0, cull=True)
        else:
            belt_band(g, side, pick(lod, 0, PATH.links // 2, 30))


def driver_hatch(g, lod):
    f = frame(glacis(DRV_D, DRV_Y, 0.0), G_ALONG, G_OUT)
    b = "hatch_driver"
    g.put(bm_box((0.5, 0.56, 0.04), pick(lod, 0.008, 0.0)), "paint", b, f @ Matrix.Translation((0, 0, 0.02)))
    if lod == 2:
        return
    g.put(bm_box((0.09, 0.36, 0.05), pick(lod, 0.008, 0.0)), "paint", b, f @ Matrix.Translation((0.13, 0, 0.06)))
    g.put(bm_box((0.024, 0.27, 0.012)), "glass", b, f @ Matrix.Translation((0.11, 0, 0.087)))
    if lod == 0:
        for (u, v) in ((-0.21, -0.24), (-0.21, 0.0), (-0.21, 0.24), (0.0, -0.24), (0.0, 0.24), (0.2, -0.24), (0.2, 0.24)):
            bolt(g, f @ Vector((u, v, 0.04)), G_OUT, b, 0.012, 0.01, "shade")
        g.put(bm_tube([(-0.17, -0.08, 0.038), (-0.17, -0.08, 0.075), (-0.17, 0.08, 0.075), (-0.17, 0.08, 0.038)], 0.008, 4),
              "steel", b, f)
        for v in (-0.18, 0.18):                              # the hinge's knuckles: on the flap, and on the plate between them
            g.put(bm_cyl(0.022, 0.09, 8), "shade", b, f @ Matrix.Translation((0.27, v, 0.025)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
        g.put(bm_cyl(0.022, 0.24, 8), "shade", "hull", f @ Matrix.Translation((0.27, 0.0, 0.025)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
        for k, v in enumerate((-0.13, 0.13)):               # two periscopes above it
            at = glacis(1.0, DRV_Y + v, 0.0)
            g.put(bm_box((0.11, 0.15, 0.1), 0.006), "paint", "hull", Matrix.Translation(at + Vector((0.0, 0, 0.04))))
            g.put(bm_box((0.012, 0.12, 0.045)), "glass", "hull", Matrix.Translation(at + Vector((0.056, 0, 0.05))))
            g.put(bm_box((0.14, 0.17, 0.02), 0.004), "shade", "hull", Matrix.Translation(at + Vector((0.01, 0, 0.1))))


def bow_gun(g, lod):
    at = glacis(MG_D, MG_Y, 0.0)
    f = frame(at, G_ALONG, G_OUT)
    if lod == 2:
        return
    if lod == 0:
        g.put(bm_lathe([(0.19, -0.01), (0.19, 0.03), (0.15, 0.05), (0.14, 0.05)], 16), "paint", "hull", f)
        sph = [(0.125 * math.sin(math.radians(a)), -0.125 * math.cos(math.radians(a))) for a in range(0, 181, 30)]
        sph[0], sph[-1] = (0.0, -0.125), (0.0, 0.125)
        g.put(bm_lathe(sph, 12), "paint", "hull", toward(at + G_OUT * 0.01, (1, 0, 0)))
    else:
        g.put(bm_cyl(0.13, 0.12, 8), "paint", "hull", toward(at, (1, 0, 0)))
    x0 = at.x + 0.1
    g.put(bm_cyl(0.028, 0.22, pick(lod, 8, 6)), "shade", "hull", toward((x0 + 0.11, at.y, at.z + 0.01), (1, 0, 0)))
    g.put(bm_cyl(0.011, 0.12, 6), "steel", "hull", toward((x0 + 0.28, at.y, at.z + 0.01), (1, 0, 0)))


def lamps(g, lod):
    if lod == 2:
        return
    for side in (1.0, -1.0):
        c = Vector((FRONT + 0.12, side * 1.04, 1.19))
        m = toward(c, (1, 0, 0))
        g.put(bm_box((0.1, 0.045, 0.045)), "shade", "hull", Matrix.Translation((FRONT + 0.04, c.y, c.z - 0.04)))
        if lod == 0:
            g.put(bm_lathe([(0.0, -0.07), (0.05, -0.065), (0.078, -0.03), (0.085, 0.04), (0.085, 0.05), (0.07, 0.05)], 12), "paint", "hull", m)
            g.put(bm_lathe([(0.07, 0.045), (0.0, 0.056)], 12), "glass", "hull", m)
            g.put(bm_tube([c + Vector((-0.04, -0.1, -0.07)), c + Vector((0.08, -0.1, 0.0)), c + Vector((0.11, 0.0, 0.03)),
                           c + Vector((0.08, 0.1, 0.0)), c + Vector((-0.04, 0.1, -0.07))], 0.006, 4), "steel", "hull")
            g.put(bm_tube([c + Vector((0.11, 0.0, 0.03)), c + Vector((0.1, 0.0, -0.09)), c + Vector((-0.02, 0.0, -0.1))], 0.006, 4), "steel", "hull")
        else:
            g.put(bm_cyl(0.08, 0.12, 6), "paint", "hull", m)
            g.put(bm_cyl(0.065, 0.01, 6), "glass", "hull", toward(c + Vector((0.065, 0, 0)), (1, 0, 0)))


def mudguards(g, lod):
    if lod == 2:
        return
    bev = pick(lod, 0.004, 0.0)
    for side in (1.0, -1.0):
        y = side * TRACK_Y
        w = TRACK_W + 0.12
        # in front: flat under the waist's lip, then sloping down over the sprocket
        g.put(bm_box((0.22, w, 0.012), bev), "paint", "hull", Matrix.Translation((FRONT + 0.06, y, 1.035)))
        g.put(bm_box((0.34, w, 0.012), bev), "paint", "hull", place((FRONT + 0.335, y, 1.0), (0, 13, 0)))
        if lod == 0:
            g.put(bm_box((0.34, 0.012, 0.05), bev), "paint", "hull", place((FRONT + 0.335, y + side * w * 0.5, 0.98), (0, 13, 0)))
        # behind: a short guard and a rubber flap hanging from it
        g.put(bm_box((0.3, w, 0.012), bev), "paint", "hull", place((REAR - 0.1, y, 0.99), (0, 8, 0)))
        g.put(bm_box((0.012, w - 0.04, 0.24), 0.0), "rubber", "hull", Matrix.Translation((REAR - 0.245, y, 0.86)))
        if lod == 0:
            for dy in (-0.14, 0.0, 0.14):
                bolt(g, (REAR - 0.252, y + dy, 0.95), (-1, 0, 0), "hull", 0.01, 0.008, "shade")


def spare_links(g, lod):
    """Four spare links hung across the nose plate under a bar (painted 'rust': they are not part of the belt, and the
    game must not scroll them)."""
    if lod == 2:
        return
    lo, hi = NOSE_FOOT, NOSE_TOP
    e = (hi - lo).normalized()
    n = Vector((e.z, 0.0, -e.x))
    if n.x < 0:
        n = -n
    tv, tf, tm = link_template(0 if lod == 0 else 1)
    for y in (-0.645, -0.215, 0.215, 0.645):
        c = lo + e * 0.2 + Vector((0, y, 0))
        pts = [tuple(c + e * u + n * (w + 0.002) + Vector((0, v, 0))) for u, w, v in tv]
        g.add(pts, tf, "rust", "hull")
    if lod == 0:
        bar = lo + e * 0.2 + n * 0.065
        g.put(bm_tube([bar + Vector((0, -0.86, 0)), bar + Vector((0, 0.86, 0))], 0.012, 6), "steel", "hull")
        for y in (-0.86, -0.43, 0.0, 0.43, 0.86):
            g.put(bm_box((0.035, 0.03, 0.08)), "shade", "hull", frame(lo + e * 0.2 + n * 0.035 + Vector((0, y, 0)), e, n))


def shackles(g, lod):
    if lod != 0:
        return
    for x, z, d in ((NOSE_FOOT.x - 0.02, NOSE_FOOT.z - 0.05, (1, 0, -0.5)), (REAR - 0.04, 0.93, (-1, 0, 0))):
        for y in (-0.62, 0.62):
            g.put(bm_box((0.05, 0.1, 0.12)), "paint", "hull", Matrix.Translation((x, y, z)))
            ring = [Vector((x + d[0] * (0.07 + 0.045 * math.cos(t)), y, z + 0.045 * math.sin(t))) for t in np.linspace(0, 2 * math.pi, 9)[:-1]]
            g.put(bm_tube(ring, 0.013, 4, closed=True), "steel", "hull")


def exhaust(g, lod):
    c = SILENCER
    m = Matrix.Translation(c) @ Matrix.Rotation(-math.pi / 2, 4, "X")     # local z along +y
    if lod == 2:
        g.put(bm_cyl(0.13, 1.5, 6), "rust", "hull", m)
        return
    n = pick(lod, 14, 8)
    g.put(bm_lathe([(0.0, -0.75), (0.1, -0.75), (0.13, -0.71), (0.13, 0.71), (0.1, 0.75), (0.0, 0.75)], n), "rust", "hull", m)
    if lod == 0:
        arc = [(math.cos(math.radians(a)), math.sin(math.radians(a))) for a in (200, 180, 150, 120, 90, 60, 30)]
        g.put(bm_tube([c + Vector((0, -0.62, 0)), c + Vector((0, 0.62, 0))], 0.165, profile=arc, open_profile=True, caps=False),
              "shade", "hull")
        for y in (-0.45, -0.15, 0.15, 0.45):                 # the shield's straps
            g.put(bm_tube([c + Vector((0, y, 0)), c + Vector((0, y + 0.03, 0))], 0.172, profile=arc, open_profile=True, caps=False), "shade", "hull")
    for side in (1.0, -1.0):
        pts = [c + Vector((0.0, side * 0.7, 0.0)), c + Vector((0.0, side * 0.86, 0.0)), c + Vector((-0.06, side * 0.93, -0.05)),
               c + Vector((-0.12, side * 0.96, -0.13)), c + Vector((-0.15, side * 0.97, -0.2))]
        g.put(bm_tube(pts, 0.045, pick(lod, 8, 5)), "rust", "hull")
        if lod == 0:
            g.put(bm_lathe([(0.05, 0.0), (0.056, 0.03), (0.044, 0.03)], 8), "rust", "hull", toward(pts[-1], pts[-1] - pts[-2]))


def exhaust_end(side):
    c = SILENCER
    a = c + Vector((-0.12, side * 0.96, -0.13))
    b = c + Vector((-0.15, side * 0.97, -0.2))
    return b + (b - a).normalized() * 0.03, (b - a).normalized()


def engine_deck(g, lod):
    if lod == 2:
        return
    deck_x = -S["upper_long"] * 0.5 + UPPER_X + S["tail"] + 0.6
    g.put(bm_box((0.95, 0.26, 0.016), pick(lod, 0.004, 0.0)), "paint", "hull", Matrix.Translation((deck_x, 0.0, ROOF + 0.008)))
    if lod == 0:
        for y in (-0.1, 0.1):
            bolt_row(g, (deck_x - 0.45, y, ROOF + 0.016), (deck_x + 0.45, y, ROOF + 0.016), 6, (0, 0, 1), "hull", 0.011, 0.009, sides=5)
        for y in (-1.0, 1.0):                                # fuel fillers
            g.put(bm_lathe([(0.075, -0.005), (0.075, 0.03), (0.06, 0.04), (0.0, 0.04)], 12), "paint", "hull", Matrix.Translation((-0.9, y, ROOF)))
            g.put(bm_box((0.1, 0.02, 0.02)), "shade", "hull", Matrix.Translation((-0.9, y, ROOF + 0.05)))
        for x, y in ((1.3, 1.2), (1.3, -1.2), (-2.08, 1.2), (-2.08, -1.2)):   # lifting eyes
            ring = [Vector((x + 0.04 * math.cos(t), y, ROOF + 0.045 + 0.04 * math.sin(t))) for t in np.linspace(0, 2 * math.pi, 7)[:-1]]
            g.put(bm_tube(ring, 0.011, 4, closed=True), "shade", "hull")
            g.put(bm_box((0.1, 0.04, 0.02)), "shade", "hull", Matrix.Translation((x, y, ROOF + 0.008)))


def tools_and_cable(g, lod):
    """Shovel, axe and crowbar clamped on the right sponson; the tow cable along the left one. (The game paints its
    number on both sides at x -1.4 .. -0.5, z 1.3 .. 1.56: kept clear.)"""
    if lod == 2:
        return
    ys = -SIDE - 0.03
    if lod == 1:
        g.put(bm_box((1.15, 0.03, 0.04)), "wood", "hull", Matrix.Translation((0.5, ys, 1.62)))
        g.put(bm_box((0.75, 0.03, 0.04)), "wood", "hull", Matrix.Translation((0.45, ys, 1.43)))
        g.put(bm_box((1.3, 0.03, 0.03)), "steel", "hull", Matrix.Translation((-1.4, ys, 1.76)))
        g.put(bm_box((3.2, 0.03, 0.03)), "steel", "hull", Matrix.Translation((-0.4, SIDE + 0.03, 1.72)))
        return
    X = lambda x, z: toward((x, ys, z), (1, 0, 0))
    # shovel: handle, D-grip, blade
    g.put(bm_cyl(0.017, 0.82, 6), "wood", "hull", X(0.44, 1.62))
    g.put(bm_tube([(0.03, ys, 1.62), (-0.04, ys, 1.66), (-0.1, ys, 1.62), (-0.04, ys, 1.58), (0.03, ys, 1.62)], 0.008, 4), "steel", "hull")
    g.put(bm_box((0.3, 0.012, 0.2), 0.0, shape=lambda p: Vector((p.x, p.y, p.z * (0.8 if p.x < 0 else 1.0)))), "steel", "hull",
          Matrix.Translation((1.0, ys, 1.62)))
    # axe: handle and head
    g.put(bm_cyl(0.017, 0.72, 6), "wood", "hull", X(0.47, 1.43))
    g.put(bm_box((0.06, 0.03, 0.17), 0.004, shape=lambda p: Vector((p.x * (0.4 if p.z > 0.2 else 1.0), p.y, p.z))), "steel", "hull",
          Matrix.Translation((0.11, ys, 1.46)))
    # crowbar, above the number
    g.put(bm_cyl(0.016, 1.3, 6), "steel", "hull", X(-1.4, 1.76))
    g.put(bm_tube([(-0.75, ys, 1.76), (-0.68, ys, 1.77), (-0.64, ys, 1.8)], 0.016, 6), "steel", "hull")
    for x, z in ((0.15, 1.62), (0.75, 1.62), (0.28, 1.43), (0.68, 1.43), (-1.85, 1.76), (-0.95, 1.76)):
        g.put(bm_box((0.04, 0.05, 0.055), 0.004), "shade", "hull", Matrix.Translation((x, ys + 0.005, z)))
    # the tow cable: a long loop with an eye at each end, held in three clamps
    yc = SIDE + 0.03
    top, low = 1.76, 1.67
    pts = [Vector((1.18, yc, top))]
    pts += [Vector((x, yc, top + 0.006 * math.sin(x * 9))) for x in np.linspace(1.05, -1.95, 12)]
    pts += [Vector((-2.06, yc, (top + low) * 0.5 + 0.03)), Vector((-2.08, yc, (top + low) * 0.5)), Vector((-2.06, yc, (top + low) * 0.5 - 0.03))]
    pts += [Vector((x, yc, low + 0.006 * math.sin(x * 7))) for x in np.linspace(-1.95, 1.05, 12)]
    pts += [Vector((1.18, yc, low))]
    g.put(bm_tube(pts, 0.016, 5), "steel", "hull")
    for z in (top, low):
        eye = [Vector((1.26 + 0.07 * math.cos(t), yc, z + 0.04 * math.sin(t))) for t in np.linspace(0, 2 * math.pi, 9)[:-1]]
        g.put(bm_tube(eye, 0.016, 5, closed=True), "steel", "hull")
    g.put(bm_cyl(0.02, 0.08, 6), "shade", "hull", toward((1.3, yc - 0.01, (top + low) * 0.5), (0, 1, 0)))
    for x in (0.4, -0.6, -1.6):
        g.put(bm_box((0.05, 0.04, 0.14), 0.004), "shade", "hull", Matrix.Translation((x, yc + 0.005, (top + low) * 0.5)))


def engine_door(g, lod):
    """The engine's access door on the sloping tail plate: hinged along its top, bolted round, with two grab handles."""
    if lod == 2:
        return
    f = frame(Vector((REAR, 0.0, WAIST_TOP)) + T_ALONG * 0.36, T_ALONG, T_OUT)
    g.put(bm_box((0.4, 1.0, 0.03), pick(lod, 0.006, 0.0)), "paint", "hull", f @ Matrix.Translation((0, 0, 0.015)))
    if lod == 0:
        for v in (-0.36, 0.36):
            g.put(bm_cyl(0.022, 0.14, 8), "shade", "hull", f @ Matrix.Translation((0.2, v, 0.02)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
        for u, v in ((-0.16, -0.44), (-0.16, 0.0), (-0.16, 0.44), (0.0, -0.44), (0.0, 0.44), (0.14, -0.2), (0.14, 0.2)):
            bolt(g, f @ Vector((u, v, 0.03)), T_OUT, "hull", 0.011, 0.009, "shade", 5)
        for v in (-0.25, 0.25):
            g.put(bm_tube([(-0.13, v - 0.07, 0.03), (-0.13, v - 0.07, 0.065), (-0.13, v + 0.07, 0.065), (-0.13, v + 0.07, 0.03)], 0.008, 4),
                  "steel", "hull", f)


def stowage(g, lod):
    """The crew's kit on the back of the engine deck, outside the circle the turret's bin sweeps (so the turret turns
    clear of it): three jerrycans upright in a rack on the left, an ammunition crate on the right, a bucket on the right
    grille. (A rolled tarpaulin rides on the turret's bin; spare links on the nose.)"""
    if lod == 2:
        g.put(bm_box((0.52, 0.36, 0.45)), "shade", "hull", Matrix.Translation((-1.77, 1.125, ROOF + 0.225)))
        g.put(bm_box((0.45, 0.32, 0.26)), "wood", "hull", Matrix.Translation((-1.8, -1.11, ROOF + 0.13)))
        return
    z0 = ROOF + 0.012
    yc = 1.125                                               # the rack's middle across (left side)
    xs = (-1.94, -1.77, -1.60)
    for k, x in enumerate(xs):
        if lod == 1:
            g.put(bm_box((0.16, 0.345, 0.47)), "shade", "hull", Matrix.Translation((x, yc, z0 + 0.235)))
            continue
        g.put(bm_box((0.16, 0.345, 0.47), 0.012), "shade", "hull", Matrix.Translation((x, yc, z0 + 0.235)))
        g.put(bm_box((0.168, 0.353, 0.014)), "shade", "hull", Matrix.Translation((x, yc, z0 + 0.25)))     # the seam
        top = z0 + 0.47
        g.put(bm_tube([(x, yc - 0.12, top - 0.005), (x, yc - 0.09, top + 0.04), (x, yc + 0.04, top + 0.04), (x, yc + 0.07, top - 0.005)],
                      0.011, 4), "shade", "hull")                                                        # the handles
        g.put(bm_cyl(0.026, 0.05, 6), "steel", "hull", Matrix.Translation((x, yc + 0.13, top + 0.02)))  # the spout's cap
    # the rack: a tray, end plates, a strap over the tops
    g.put(bm_box((0.56, 0.37, 0.012)), "paint", "hull", Matrix.Translation((-1.77, yc, ROOF + 0.006)))
    for x in (-2.035, -1.505):
        g.put(bm_box((0.012, 0.37, 0.3)), "paint", "hull", Matrix.Translation((x, yc, z0 + 0.15)))
    if lod == 0:
        g.put(bm_box((0.56, 0.035, 0.008)), "steel", "hull", Matrix.Translation((-1.77, yc - 0.17, z0 + 0.36)))
        g.put(bm_box((0.56, 0.035, 0.008)), "steel", "hull", Matrix.Translation((-1.77, yc + 0.17, z0 + 0.36)))
    # the crate: planked lid, rope handles at its ends
    cx, cy, ch = -1.8, -1.11, 0.26
    g.put(bm_box((0.45, 0.32, ch), pick(lod, 0.008, 0.0)), "wood", "hull", Matrix.Translation((cx, cy, ROOF + ch * 0.5)))
    if lod == 0:
        for x in (cx - 0.15, cx + 0.15):
            g.put(bm_box((0.03, 0.33, 0.015)), "wood", "hull", Matrix.Translation((x, cy, ROOF + ch + 0.004)))
        for x in (cx - 0.235, cx + 0.235):
            d = 1.0 if x > cx else -1.0
            g.put(bm_tube([(x - d * 0.01, cy - 0.07, ROOF + 0.17), (x + d * 0.02, cy - 0.06, ROOF + 0.13), (x + d * 0.02, cy + 0.06, ROOF + 0.13),
                           (x - d * 0.01, cy + 0.07, ROOF + 0.17)], 0.009, 4), "canvas", "hull")
    # the bucket, on the right-hand grille
    bx, by = -2.0, -0.72
    if lod == 0:
        g.put(bm_lathe([(0.0, 0.0), (0.105, 0.0), (0.135, 0.24), (0.142, 0.25), (0.13, 0.25), (0.098, 0.012)], 10), "steel", "hull",
              Matrix.Translation((bx, by, ROOF + 0.02)))
        g.put(bm_tube([(bx, by - 0.14, ROOF + 0.24), (bx - 0.05, by - 0.1, ROOF + 0.3), (bx - 0.07, by, ROOF + 0.32), (bx - 0.05, by + 0.1, ROOF + 0.3),
                       (bx, by + 0.14, ROOF + 0.24)], 0.006, 4), "steel", "hull")
    else:
        g.put(bm_cyl(0.13, 0.25, 6, r2=0.14), "steel", "hull", Matrix.Translation((bx, by, ROOF + 0.145)))


def hull_welds(g, lod):
    if lod != 0:
        return
    for s_ in (1.0, -1.0):
        Y = Vector((0, s_, 0))
        weld(g, (FRONT, s_ * SIDE, WAIST_TOP), (GLACIS_TOP, s_ * SIDE, ROOF), Y + G_OUT, "hull", sink=0.018)
        weld(g, (REAR, s_ * SIDE, WAIST_TOP), (TAIL_TOP, s_ * SIDE, ROOF), Y + T_OUT, "hull", sink=0.018)
        weld(g, (FRONT - 0.03, s_ * SIDE, WAIST_TOP), (REAR + 0.03, s_ * SIDE, WAIST_TOP), Y, "hull", sink=0.012)
        weld(g, (GLACIS_TOP - 0.02, s_ * SIDE, ROOF), (TAIL_TOP + 0.02, s_ * SIDE, ROOF), Y + Vector((0, 0, 1)), "hull", sink=0.018)
    weld(g, (FRONT, -SIDE + 0.02, WAIST_TOP), (FRONT, SIDE - 0.02, WAIST_TOP), Vector((1, 0, 0)) + G_OUT, "hull", sink=0.02)
    weld(g, (REAR, -SIDE + 0.02, WAIST_TOP), (REAR, SIDE - 0.02, WAIST_TOP), Vector((-1, 0, 0)) + T_OUT, "hull", sink=0.02)
    weld(g, (GLACIS_TOP, -SIDE + 0.02, ROOF), (GLACIS_TOP, SIDE - 0.02, ROOF), G_OUT + Vector((0, 0, 1)), "hull", sink=0.012)
    weld(g, (TAIL_TOP, -SIDE + 0.02, ROOF), (TAIL_TOP, SIDE - 0.02, ROOF), T_OUT + Vector((0, 0, 1)), "hull", sink=0.012)


def turret_face(i, h):
    """The middle of the turret's i-th face (between corners i and i+1) at height h above its foot, and its outward normal."""
    a0, b0 = Vector(HEX_FOOT[i] + (0.0,)), Vector(HEX_FOOT[(i + 1) % 6] + (0.0,))
    a1, b1 = Vector(HEX_ROOF[i] + (THIGH,)), Vector(HEX_ROOF[(i + 1) % 6] + (THIGH,))
    t = h / THIGH
    a, b = a0.lerp(a1, t), b0.lerp(b1, t)
    n = (b0 - a0).cross(a1 - a0).normalized()
    if n.dot((a0 + b0) * 0.5) < 0:
        n = -n
    return TURRET_PIVOT + Vector((0, 0, 0.02)) + (a + b) * 0.5, n, (b - a).normalized()


def turret_fittings(g, lod):
    tb = "turret"
    # the race ring under the turret's overhang
    g.put(bm_lathe([(0.84, -0.01), (0.84, 0.025)], pick(lod, 24, 16, 8)), "shade", tb, Matrix.Translation(TURRET_PIVOT))
    # commander's hatch: coaming ring on the roof, the round lid on its hinge, a periscope ahead of it
    hb = "hatch_commander"
    n = pick(lod, 20, 12, 8)
    if lod == 2:
        g.put(bm_cyl(0.25, 0.05, 8), "paint", hb, Matrix.Translation(CMDR + Vector((0, 0, 0.03))))
    else:
        g.put(bm_lathe([(0.275, -0.005), (0.275, 0.035), (0.248, 0.035)], n), "paint", tb, Matrix.Translation(CMDR))
        g.put(bm_lathe([(0.0, 0.03), (0.256, 0.03), (0.256, 0.055), (0.232, 0.07), (0.0, 0.076)], n), "paint", hb, Matrix.Translation(CMDR))
        if lod == 0:
            bolt_ring(g, CMDR + Vector((0, 0, 0.068)), (0, 0, 1), 0.19, 8, hb, 0.011, 0.009, "shade")
            g.put(bm_tube([CMDR + Vector((0.08, -0.07, 0.072)), CMDR + Vector((0.08, -0.07, 0.11)), CMDR + Vector((0.08, 0.07, 0.11)),
                           CMDR + Vector((0.08, 0.07, 0.072))], 0.008, 4), "steel", hb)
            g.put(bm_cyl(0.024, 0.16, 8), "shade", hb, toward(CMDR_HINGE, (0, 1, 0)))
            g.put(bm_box((0.07, 0.2, 0.045), 0.006), "shade", tb, Matrix.Translation(CMDR_HINGE + Vector((-0.02, 0, -0.025))))
        g.put(bm_box((0.12, 0.18, 0.1), pick(lod, 0.008, 0.0)), "paint", tb, Matrix.Translation(CMDR + Vector((0.38, 0, 0.05))))
        g.put(bm_box((0.012, 0.14, 0.05)), "glass", tb, Matrix.Translation(CMDR + Vector((0.445, 0, 0.055))))
        if lod == 0:
            g.put(bm_box((0.15, 0.2, 0.02), 0.004), "shade", tb, Matrix.Translation(CMDR + Vector((0.39, 0, 0.11))))
    # loader's hatch: a square lid hinged on its outer edge, a periscope ahead
    lb = "hatch_loader"
    g.put(bm_box((0.44, 0.34, 0.035), pick(lod, 0.008, 0.0)), "paint", lb, Matrix.Translation(LOADER + Vector((0, 0, 0.0225))))
    if lod < 2:
        g.put(bm_box((0.12, 0.18, 0.1), pick(lod, 0.008, 0.0)), "paint", tb, Matrix.Translation(LOADER + Vector((0.42, 0.02, 0.05))))
        g.put(bm_box((0.012, 0.14, 0.05)), "glass", tb, Matrix.Translation(LOADER + Vector((0.485, 0.02, 0.055))))
    if lod == 0:
        for u, v in ((-0.17, -0.12), (-0.17, 0.12), (0.17, -0.12), (0.17, 0.12), (0.0, -0.12), (0.0, 0.12)):
            bolt(g, LOADER + Vector((u, v, 0.04)), (0, 0, 1), lb, 0.011, 0.009, "shade")
        for u in (-0.14, 0.14):
            g.put(bm_cyl(0.02, 0.1, 8), "shade", lb, toward(LOADER_HINGE + Vector((u, 0, 0)), (1, 0, 0)))
        g.put(bm_tube([LOADER + Vector((-0.08, -0.1, 0.04)), LOADER + Vector((-0.08, -0.1, 0.075)), LOADER + Vector((0.08, -0.1, 0.075)),
                       LOADER + Vector((0.08, -0.1, 0.04))], 0.008, 4), "steel", lb)
    # the mushroom vent
    vx = Vector((0.45, 0.0, TROOF))
    if lod < 2:
        g.put(bm_cyl(0.07, 0.1, pick(lod, 12, 8)), "paint", tb, Matrix.Translation(vx + Vector((0, 0, 0.03))))
        g.put(bm_lathe([(0.0, 0.07), (0.15, 0.07), (0.15, 0.09), (0.12, 0.11), (0.0, 0.115)], pick(lod, 14, 8)), "paint", tb, Matrix.Translation(vx))
    # vision blocks on the flat sides, a stowage bin on the back with a rolled tarpaulin
    for i in (1, 4):
        at, nrm, along = turret_face(i, 0.42)
        f = frame(at, along, nrm)
        if lod < 2:
            g.put(bm_box((0.26, 0.13, 0.06), pick(lod, 0.008, 0.0)), "paint", tb, f @ Matrix.Translation((0.32 if i == 4 else -0.32, 0, 0.02)))
            g.put(bm_box((0.18, 0.026, 0.012)), "glass", tb, f @ Matrix.Translation((0.32 if i == 4 else -0.32, 0.0, 0.052)))
    if lod < 2:
        bz = 2.04
        poly = [(-1.68, -0.42), (-0.9, -0.42), (-1.25, 0.0), (-0.9, 0.42), (-1.68, 0.42)]
        # (its front follows the turret's back plates, which lean in as they rise)
        lean = (HEX_FOOT[0][0] - HEX_ROOF[0][0]) / THIGH
        bin_ = bm_prism(poly, 0.3)
        for v in bin_.verts:
            if v.co.x > -1.4:
                v.co.x += lean * v.co.z
        g.put(bin_, "paint", tb, Matrix.Translation((0, 0, bz + 0.15)))
        if lod == 0:
            lid = [(-1.70, -0.44), (-0.92, -0.44), (-1.26, 0.0), (-0.92, 0.44), (-1.70, 0.44)]
            g.put(bm_prism([(x + (lean * 0.16 if x > -1.4 else 0.0), y) for x, y in lid], 0.02), "shade", tb, Matrix.Translation((0, 0, bz + 0.31)))
            g.put(bm_cyl(0.095, 0.82, 10), "canvas", tb, toward((-1.45, 0.0, bz + 0.41), (0, 1, 0)))
            for y in (-0.28, 0.28):
                g.put(bm_cyl(0.1, 0.035, 10), "shade", tb, toward((-1.45, y, bz + 0.41), (0, 1, 0)))
            for y in (-0.3, 0.3):
                g.put(bm_box((0.06, 0.04, 0.14), 0.004), "shade", tb, Matrix.Translation((-1.0, y * 1.3, bz + 0.07)))
    # the aerial's foot
    if lod < 2:
        g.put(bm_lathe([(0.05, -0.01), (0.05, 0.02), (0.035, 0.05), (0.02, 0.08)], 8), "shade", tb, Matrix.Translation(AERIAL_FOOT - Vector((0, 0, 0.08))))
    if lod == 0:
        for i in range(6):                                   # weld seams at the turret's corners and round its roof
            a0 = TURRET_PIVOT + Vector((HEX_FOOT[i][0], HEX_FOOT[i][1], 0.02))
            a1 = TURRET_PIVOT + Vector((HEX_ROOF[i][0], HEX_ROOF[i][1], 0.02 + THIGH))
            out = Vector((HEX_FOOT[i][0], HEX_FOOT[i][1], 0.25)).normalized()
            weld(g, a0 + Vector((0, 0, 0.03)), a1 - Vector((0, 0, 0.03)), out, tb, sink=0.012)
            _, nrm, _ = turret_face(i, THIGH)
            b1 = TURRET_PIVOT + Vector((HEX_ROOF[(i + 1) % 6][0], HEX_ROOF[(i + 1) % 6][1], 0.02 + THIGH))
            weld(g, a1.lerp(b1, 0.04), a1.lerp(b1, 0.96), nrm + Vector((0, 0, 1)), tb, sink=0.016)


def aerial(g, lod):
    """The whip: a thin tapering rod in rings, each ring shared between the three aerial bones so it bends."""
    seg = AERIAL_LEN / 3.0
    rings = pick(lod, 13, 4, 4)
    sides = pick(lod, 5, 4, 3)
    pts = [AERIAL_FOOT + Vector((0, 0, AERIAL_LEN * k / (rings - 1))) for k in range(rings)]
    rad = lambda i: 0.011 - 0.005 * i / (rings - 1)

    def w(p):
        b = (p.z - AERIAL_FOOT.z) / seg
        ws = {}
        for i in range(3):
            ws[f"aerial_{i + 1}"] = max(0.0, 1.0 - abs(min(max(b, 0.5), 2.5) - (i + 0.5)))
        tot = sum(ws.values())
        return {k: v / tot for k, v in ws.items() if v > 1e-4}
    g.put(bm_tube(pts, rad, sides), "shade", w)


def gun(g, lod):
    gb, bb = "gun", "barrel"
    n = pick(lod, 16, 8, 6)
    X = lambda x: toward((x, 0.0, GZ), (1, 0, 0))
    if lod == 0:
        g.put(bm_lathe([(0.138, 1.27), (0.138, 1.34), (0.126, 1.36), (0.116, 1.80), (0.106, 1.89), (0.088, 1.89)], n), "paint", gb, X(0.0))
        g.put(bm_lathe([(0.122, 1.56), (0.128, 1.57), (0.128, 1.62), (0.121, 1.63)], n), "paint", gb, X(0.0))
    for prof, mat in (([(0.077, BARREL_ROOT.x), (0.065, GUN_END - 0.15)], "paint"),
                      ([(0.065, GUN_END - 0.15), (0.075, GUN_END - 0.14), (0.078, GUN_END - 0.12), (0.078, GUN_END), (0.072, GUN_END + 0.005),
                        (0.045, GUN_END + 0.005)], "steel"),
                      ([(0.045, GUN_END + 0.005), (0.045, GUN_END - 0.1), (0.0, GUN_END - 0.1)], "shade")):
        if lod > 0:
            prof = [prof[0], prof[-1]] if mat != "shade" else prof[1:]
        g.put(bm_lathe(prof, n), mat, bb, X(0.0))
    if lod == 2:
        return
    # the coaxial machine-gun's port and barrel (left of the gun), the gunner's sight (right)
    cy, cz = 0.25, GZ + 0.02
    g.put(bm_lathe([(0.045, -0.01), (0.04, 0.035), (0.02, 0.04)], pick(lod, 10, 6)), "paint", gb, toward((1.30, cy, cz), (1, 0, 0)))
    g.put(bm_cyl(0.012, 0.22, 6), "steel", gb, toward((1.43, cy, cz), (1, 0, 0)))
    g.put(bm_box((0.04, 0.11, 0.08), pick(lod, 0.006, 0.0)), "shade", gb, Matrix.Translation((1.31, -0.2, GZ + 0.11)))
    g.put(bm_box((0.012, 0.08, 0.045)), "glass", gb, Matrix.Translation((1.332, -0.2, GZ + 0.11)))
    if lod == 0:
        for z in (GZ - 0.205, GZ + 0.205):
            bolt_row(g, (1.302, -0.3, z), (1.302, 0.3, z), 6, (1, 0, 0), gb, 0.012, 0.01)


COAX_TIP = Vector((1.55, 0.25, GZ + 0.02))
MG_TIP = None    # (set once the glacis point is known: see sockets())


def build_geo(lod, belts=True):
    """The whole tank at one level of detail, as one Geo."""
    g = Geo()
    campaign_parts(g, lod)
    running_gear(g, lod, belts)
    driver_hatch(g, lod)
    bow_gun(g, lod)
    lamps(g, lod)
    mudguards(g, lod)
    spare_links(g, lod)
    shackles(g, lod)
    exhaust(g, lod)
    engine_deck(g, lod)
    tools_and_cable(g, lod)
    engine_door(g, lod)
    stowage(g, lod)
    hull_welds(g, lod)
    turret_fittings(g, lod)
    aerial(g, lod)
    gun(g, lod)
    return g


def materials_plain(nation=NATION):
    """One material per paint, plainly coloured (what the exports carry; the game recolours by name)."""
    paint = kit.NATIONS[nation]["paint"]
    cols = {"paint": kit.hex_rgb(paint), "shade": kit.hex_rgb(paint, 0.62), "track": (0.045, 0.042, 0.04), "rubber": (0.02, 0.02, 0.02),
            "wood": (0.30, 0.19, 0.09), "steel": (0.32, 0.32, 0.31), "canvas": (0.36, 0.32, 0.22), "glass": (0.05, 0.08, 0.1),
            "rust": (0.24, 0.10, 0.045), "stripe": kit.hex_rgb("D4B44E")}
    out = {}
    for name in PAINT_NAMES:
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = (*cols[name], 1.0)
        bsdf.inputs["Roughness"].default_value = {"steel": 0.45, "glass": 0.1, "track": 0.6}.get(name, 0.8)
        bsdf.inputs["Metallic"].default_value = {"steel": 0.8, "track": 0.5}.get(name, 0.0)
        out[name] = m
    return out


def make_mesh(g, name, arm=None):
    """A Blender mesh from a Geo: material slots in paint order, UVs (the belt's own, a box projection elsewhere),
    vertex groups by bone, smooth shading with sharp edges past 40 degrees, skinned to `arm`."""
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(p) for p in g.co], [], g.faces)
    me.validate(clean_customdata=False)
    used = [n for n in PAINT_NAMES if n in set(g.fmat)]
    mats = materials_plain()
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


def quick_look(path, lod=0, w=640, h=360, samples=16, yaw=35.0, pitch=22.0, dist=11.0, target=(0, 0, 1.1)):
    """A test still of the bare model (no rig), for checking shapes."""
    sc = kit.fresh(FPS)
    g = build_geo(lod)
    print(f"LOD{lod}: {g.tris()} triangles")
    make_mesh(g, f"tank_LOD{lod}")
    kit.studio()
    kit.field_ground()
    cam = kit.camera(lens=50)
    kit.orbit(cam, target, dist, pitch, yaw)
    kit.render_setup(w, h, samples)
    sc.render.threads_mode = "FIXED"
    sc.render.threads = 2
    kit.still(path)


# ------------------------------------------------------------------------------------------------ the rig
def J(v):
    """Blender's frame to the game's JSON frame (y to the right)."""
    return [round(float(v[0]), 4), round(-float(v[1]), 4), round(float(v[2]), 4)]


def turn_axis_b(kj):
    """A turn of +a about the JSON axis kj (by v' = v cos a + (k x v) sin a + k (k.v)(1 - cos a), computed in the JSON's
    components, as Unreal's FQuat(axis, angle) does) is a turn of +a about this axis in Blender."""
    return Vector((-kj[0], kj[1], -kj[2]))


def slide_axis_b(kj):
    return Vector((kj[0], -kj[1], kj[2]))


def opening_axis(hinge, centre, outward):
    """The JSON axis about which + swings a lid (its middle at `centre`) out along `outward`."""
    a = (Vector(centre) - Vector(hinge)).cross(Vector(outward)).normalized()
    return (round(-a.x, 4), round(a.y, 4), round(-a.z, 4))


def parts_table():
    rad = math.radians
    P = []

    def add(name, parent, pivot, axis, kind, limits, sense, tail=(0, 0, 0.3), **extra):
        P.append(dict(name=name, parent=parent, pivot=Vector(pivot), axis=tuple(float(a) for a in axis), kind=kind,
                      limits=limits, sense=sense, tail=Vector(tail), **extra))
    add("root", None, (0, 0, 0), (0, 0, 1), "fixed", None, "the tank's origin, on the ground under its middle", (0, 0, 0.5))
    add("hull", "root", HULL_PIVOT, (0, -1, 0), "turn", [-0.07, 0.07],
        "rocks on its springs. Its axis is the pitch: + nose up. The clips also roll it about x (+ right side up) and bob it along z (+ up).",
        (1.0, 0, 0))
    add("turret", "hull", TURRET_PIVOT, (0, 0, 1), "turn", None,
        "traverse, free all round (no limits): + turns to the right, clockwise seen from above. The game turns it.", (0, 0, 0.6))
    add("gun", "turret", GUN_PIVOT, (0, -1, 0), "turn", [round(rad(-8), 4), round(rad(20), 4)],
        "elevation at the trunnions: + muzzle up, from -8 to +20 degrees. The game turns it.", (0.6, 0, 0))
    add("barrel", "gun", BARREL_ROOT, (-1, 0, 0), "slide", [0.0, 0.3], "recoil: + slides back along the bore (metres)", (1.0, 0, 0))
    drv_mid = glacis(DRV_D, DRV_Y, 0.025)
    add("hatch_driver", "hull", DRV_HINGE, opening_axis(DRV_HINGE, drv_mid, G_OUT), "turn", [0.0, round(rad(100), 4)],
        "the driver's flap, hinged at its top edge: + opens", (0, -0.25, 0))
    add("hatch_commander", "turret", CMDR_HINGE, opening_axis(CMDR_HINGE, CMDR + Vector((0, 0, 0.05)), (0, 0, 1)), "turn",
        [0.0, round(rad(115), 4)], "the commander's round hatch, hinged at its back: + opens", (0, -0.25, 0))
    add("hatch_loader", "turret", LOADER_HINGE, opening_axis(LOADER_HINGE, LOADER + Vector((0, 0, 0.03)), (0, 0, 1)), "turn",
        [0.0, round(rad(110), 4)], "the loader's square hatch, hinged at its outer edge: + opens", (0.25, 0, 0))
    seg = AERIAL_LEN / 3.0
    for i in range(3):
        add(f"aerial_{i + 1}", "turret" if i == 0 else f"aerial_{i}", AERIAL_FOOT + Vector((0, 0, seg * i)), (0, 1, 0), "turn",
            [-0.35, 0.35], "the whip aerial, three bones: + leans the tip forward (about y); the clips also sway it about x (+ tip to the left)",
            (0, 0, seg))
    for side in (1.0, -1.0):
        sl = "l" if side > 0 else "r"
        y = side * TRACK_Y
        for i, x in enumerate(WHEEL_XS):
            add(f"wheel_{sl}{i + 1}", "root", (x, y, WHEEL_Z), (0, 1, 0), "spin", None,
                f"road wheel {i + 1} from the front, {'left' if side > 0 else 'right'}: + rolls forward", (0, 0.25, 0), radius=S["wheel_r"])
        add(f"sprocket_{sl}", "hull", (S["sprocket_x"], y, S["sprocket_z"]), (0, 1, 0), "spin", None,
            "the drive sprocket (front): + drives forward", (0, 0.25, 0), radius=round(PATH.sprocket_pitch_r, 4))
        add(f"idler_{sl}", "hull", (S["idler_x"], y, S["idler_z"]), (0, 1, 0), "spin", None, "the idler (rear): + rolls forward",
            (0, 0.25, 0), radius=S["idler_r"])
        for i, x in enumerate(ROLLER_XS):
            add(f"roller_{sl}{i + 1}", "hull", (x, y, S["roller_z"]), (0, 1, 0), "spin", None, "a return roller: + rolls forward",
                (0, 0.2, 0), radius=S["roller_r"])
        add(f"track_{sl}", "root", (0.0, y, 0.5), (1, 0, 0), "scroll", None,
            "the belt: never moved as a bone; the game scrolls its U (see track in the notes). Near the sprocket and idler it is skinned "
            "partly to the hull so it follows the hull's rocking.", (0.5, 0, 0), metres_per_u=round(PATH.length, 4))
    return P


def sockets_table():
    mg_at = glacis(MG_D, MG_Y, 0.0)
    deck_x = -S["upper_long"] * 0.5 + UPPER_X + S["tail"] + 0.6
    el, dl = exhaust_end(1.0)
    er, dr = exhaust_end(-1.0)
    return [
        ("socket_muzzle", "barrel", Vector((GUN_END + 0.01, 0.0, GZ)), Vector((1, 0, 0))),
        ("socket_coax", "gun", COAX_TIP, Vector((1, 0, 0))),
        ("socket_hull_mg", "hull", Vector((mg_at.x + 0.44, mg_at.y, mg_at.z + 0.01)), Vector((1, 0, 0))),
        ("socket_exhaust_l", "hull", el, dl),
        ("socket_exhaust_r", "hull", er, dr),
        ("socket_commander", "turret", CMDR + Vector((0, 0, 0.05)), Vector((0, 0, 1))),
        ("socket_engine", "hull", Vector((deck_x, 0.0, ROOF + 0.03)), Vector((0, 0, 1))),
        ("socket_dust_l", "root", Vector((REAR - 0.35, TRACK_Y, 0.12)), Vector((-1, 0, 0.35)).normalized()),
        ("socket_dust_r", "root", Vector((REAR - 0.35, -TRACK_Y, 0.12)), Vector((-1, 0, 0.35)).normalized()),
    ]


def make_rig(parts):
    ad = bpy.data.armatures.new("tank_rig")
    ad.display_type = "STICK"
    arm = bpy.data.objects.new("tank_rig", ad)
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


def make_sockets(arm):
    bpy.context.view_layer.update()
    out = []
    for name, bone, at, facing in sockets_table():
        e = bpy.data.objects.new(name, None)
        e.empty_display_type = "ARROWS"
        e.empty_display_size = 0.15
        bpy.context.scene.collection.objects.link(e)
        e.parent = arm
        e.parent_type = "BONE"
        e.parent_bone = bone
        bpy.context.view_layer.update()
        m = Vector((1, 0, 0)).rotation_difference(facing).to_matrix().to_4x4()      # (its x along the facing)
        m.translation = at
        e.matrix_world = m
        out.append(e)
    bpy.context.view_layer.update()
    return out


# ------------------------------------------------------------------------------------------------ the clips
def sym_radius(r, sectors, metres):
    """The rolling radius, within a few percent of r, that turns a wheel a whole number of its like sectors over `metres`, so a
    loop's last frame matches its first."""
    step = 2 * math.pi / sectors
    n = max(1, round(metres / r / step))
    return metres / (n * step)


def wheel_radii():
    D = PATH.drive_metres
    out = {}
    for side in "lr":
        for i in range(len(WHEEL_XS)):
            out[f"wheel_{side}{i + 1}"] = sym_radius(S["wheel_r"], 8, D)
        out[f"sprocket_{side}"] = PATH.sprocket_pitch_r
        out[f"idler_{side}"] = sym_radius(S["idler_r"], 6, D)
        for i in range(len(ROLLER_XS)):
            out[f"roller_{side}{i + 1}"] = sym_radius(S["roller_r"], 4, D)
    return out


class Clip:
    """A clip as numbers: channels (part, kind, axis or None for the part's own, value as a function of time) and events."""

    def __init__(self, name, long, loop, loop_cue=None, about=""):
        self.name, self.long, self.loop, self.loop_cue, self.about = name, long, loop, loop_cue, about
        self.ch, self.events = [], []
        self.travel = None                      # metres the belt has moved by time t (for the pictures' links)

    def add(self, part, kind, fn, axis=None):
        self.ch.append((part, kind, axis, fn))
        return self

    def event(self, t, fx, socket, cue=None):
        self.events.append({"t": round(t, 3), "fx": fx, "socket": socket, "cue": cue})
        return self

    def rolling(self, travel):
        """Wheels, sprockets, idlers and rollers turned, and the belts scrolled, by the metres travelled."""
        self.travel = travel
        for bone, r in wheel_radii().items():
            self.add(bone, "spin", lambda t, r=r: travel(t) / r)
        for sl in "lr":
            self.add(f"track_{sl}", "scroll", lambda t: travel(t) / PATH.length)
        return self

    def times(self):
        n = int(round(self.long * FPS))
        return [min(k / FPS, self.long) for k in range(n + 1)]


def _damped(t, a, period, tau, t0=0.0):
    if t < t0:
        return 0.0
    t -= t0
    return a * math.exp(-t / tau) * math.sin(2 * math.pi * t / period)


def _ease(t, t0, t1, a=0.0, b=1.0):
    return a + (b - a) * smoothstep(t0, t1, t)


def clips():
    rad = math.radians
    sin = lambda f, t, ph=0.0: math.sin(2 * math.pi * f * t + ph)
    D = PATH.drive_metres
    C = []

    def aerial_sway(c, fore, side, extra=None):
        for i in range(3):
            k = i + 1
            c.add(f"aerial_{k}", "turn", lambda t, k=k: fore(t, k))
            c.add(f"aerial_{k}", "turn", lambda t, k=k: side(t, k), axis=(1, 0, 0))

    # idle: the engine's shudder; the aerial sways
    c = Clip("idle", 2.0, True, "tank_idle_loop", "engine running, standing still")
    c.add("hull", "turn", lambda t: rad(0.07) * sin(6, t, 0.5) + rad(0.03) * sin(9.5, t))
    c.add("hull", "turn", lambda t: rad(0.09) * sin(3, t) + rad(0.04) * sin(6, t, 2.0), axis=(1, 0, 0))
    c.add("hull", "slide", lambda t: 0.0022 * sin(6, t) + 0.001 * sin(9.5, t, 1.0), axis=(0, 0, 1))
    aerial_sway(c, lambda t, k: rad(0.5 * k) * sin(1.0, t, -0.6 * k), lambda t, k: rad(0.35 * k) * sin(1.5, t, 0.3 - 0.5 * k))
    c.event(0.0, "exhaust_idle", "socket_exhaust_l").event(0.0, "exhaust_idle", "socket_exhaust_r")
    C.append(c)

    # start: the engine catches; the hull lurches on its springs and shudders up to the idle, then the idle clip follows
    c = Clip("start", 1.5, False, None, "the engine starting; play idle after it")
    ramp = lambda t: smoothstep(0.0, 0.5, t) * (1.0 - smoothstep(1.2, 1.5, t))
    c.add("hull", "turn", lambda t: _damped(t, rad(0.6), 0.55, 0.3, 0.08) + ramp(t) * (rad(0.12) * sin(8, t) + rad(0.05) * sin(13, t)))
    c.add("hull", "turn", lambda t: _damped(t, rad(0.35), 0.6, 0.3, 0.1) + ramp(t) * rad(0.1) * sin(5, t, 1.0), axis=(1, 0, 0))
    c.add("hull", "slide", lambda t: ramp(t) * 0.003 * sin(8, t, 0.5), axis=(0, 0, 1))
    aerial_sway(c, lambda t, k: _damped(t, rad(1.5 * k), 0.55, 0.4, 0.08) * (1.0 - smoothstep(1.2, 1.5, t)),
                lambda t, k: _damped(t, rad(1.0 * k), 0.6, 0.4, 0.1) * (1.0 - smoothstep(1.2, 1.5, t)))
    c.event(0.0, "exhaust_start", "socket_exhaust_l", "tank_start").event(0.0, "exhaust_start", "socket_exhaust_r")
    C.append(c)

    # drive: wheels and belts turning, the hull pitching and bobbing a little over the ground
    c = Clip("drive", DRIVE_LOOP, True, "tank_loop", f"driving forward: {D:.3f} m a loop")
    c.rolling(lambda t: D * t / DRIVE_LOOP)
    c.add("hull", "turn", lambda t: rad(0.45) * sin(0.5, t) + rad(0.15) * sin(1.5, t, 0.7) + rad(0.04) * sin(8, t))
    c.add("hull", "turn", lambda t: rad(0.3) * sin(0.5, t, 1.9) + rad(0.1) * sin(1.5, t), axis=(1, 0, 0))
    c.add("hull", "slide", lambda t: 0.008 * sin(1.0, t, 0.4) + 0.0015 * sin(7, t), axis=(0, 0, 1))
    aerial_sway(c, lambda t, k: rad(-1.0 * k) + rad(0.9 * k) * sin(1.0, t, -0.7 * k), lambda t, k: rad(0.5 * k) * sin(0.5, t, 1.0 - 0.6 * k))
    for s_ in ("l", "r"):
        c.event(0.0, "dust_trail", f"socket_dust_{s_}")
        c.event(0.0, "exhaust_drive", f"socket_exhaust_{s_}")
    C.append(c)

    # fire: the main gun at 0.0; the barrel recoils by 0.12 and runs out by 0.6; the hull rocks back and settles by 1.5
    c = Clip("fire", 2.5, False, None, "the main gun fires")
    recoil = 0.3

    def barrel(t):
        if t <= 0.12:
            return recoil * (1.0 - (1.0 - t / 0.12) ** 2)
        return recoil * (1.0 - smoothstep(0.12, 0.6, t))
    c.add("barrel", "slide", barrel)
    win = lambda t: 1.0 - smoothstep(1.2, 1.5, t)
    c.add("hull", "turn", lambda t: _damped(t, rad(1.5), 0.9, 0.33) * win(t))
    c.add("hull", "slide", lambda t: _damped(t, -0.01, 0.9, 0.3) * win(t), axis=(0, 0, 1))
    c.add("gun", "turn", lambda t: _damped(t, rad(0.6), 0.5, 0.15) * (1.0 - smoothstep(0.5, 0.8, t)))
    aerial_sway(c, lambda t, k: _damped(t, rad(-2.5 * k), 0.55, 0.45) * (1.0 - smoothstep(2.0, 2.5, t)),
                lambda t, k: _damped(t, rad(0.6 * k), 0.7, 0.4) * (1.0 - smoothstep(2.0, 2.5, t)))
    c.event(0.0, "muzzle_flash_big", "socket_muzzle", "tank_gun_01")
    c.event(0.0, "muzzle_blast_dust", "socket_muzzle")
    c.event(1.2, "case_clang", "socket_commander", "tank_shell_case")
    C.append(c)

    # mg_burst: the coaxial gun fires ten rounds; no big motion
    c = Clip("mg_burst", 1.5, False, None, "a burst from the coaxial machine-gun")
    shots = [round(k * 0.075, 3) for k in range(10)]
    c.add("gun", "turn", lambda t: rad(0.08) * sin(13.3, t) * (1.0 - smoothstep(0.7, 0.85, t)))
    c.add("hull", "slide", lambda t: 0.0006 * sin(13.3, t, 1.0) * (1.0 - smoothstep(0.7, 0.85, t)), axis=(0, 0, 1))
    for k, t in enumerate(shots):
        c.event(t, "muzzle_flash", "socket_coax", "tank_mg_burst" if k == 0 else None)
    C.append(c)

    # stop: from driving to standing; the hull pitches forward (most at 0.1) and settles
    c = Clip("stop", 0.8, False, None, "pulling up from the drive")
    v0, tb = D / DRIVE_LOOP, 0.12
    c.rolling(lambda t: v0 * t - v0 * t * t / (2 * tb) if t < tb else v0 * tb * 0.5)

    def nose(t):
        if t < 0.1:
            return -rad(1.8) * math.sin(0.5 * math.pi * t / 0.1)
        return -rad(1.8) * math.exp(-(t - 0.1) / 0.16) * math.cos(2 * math.pi * (t - 0.1) / 0.5) * (1.0 - smoothstep(0.6, 0.8, t))
    c.add("hull", "turn", nose)
    c.add("hull", "slide", lambda t: nose(t) * 0.25, axis=(0, 0, 1))
    aerial_sway(c, lambda t, k: _damped(t, rad(3.0 * k), 0.45, 0.35) * (1.0 - smoothstep(0.65, 0.8, t)),
                lambda t, k: 0.0)
    c.event(0.0, "brakes", "socket_dust_l").event(0.0, "brakes", "socket_dust_r")
    C.append(c)

    # knocked out: hit at 0.0; the hull jolts, the turret is jarred round, the commander's hatch is blown open at 0.4,
    # the gun droops, and then all is still
    c = Clip("knocked_out", 3.0, False, None, "hit and knocked out; holds its last frame")
    still = lambda t: 1.0 - smoothstep(1.1, 1.5, t)
    c.add("hull", "turn", lambda t: _damped(t, rad(2.2), 0.45, 0.25) * still(t))
    c.add("hull", "turn", lambda t: _damped(t, rad(1.6), 0.5, 0.3) * still(t), axis=(1, 0, 0))
    c.add("hull", "slide", lambda t: _damped(t, -0.015, 0.4, 0.2) * still(t), axis=(0, 0, 1))
    c.add("turret", "turn", lambda t: _ease(t, 0.02, 0.16, 0.0, rad(3.6)) - _ease(t, 0.16, 0.45, 0.0, rad(0.6)))

    def lid(t):
        keys = [(0.0, 0.0), (0.4, 0.0), (0.5, 2.0), (0.6, 1.78), (0.7, 2.0), (0.77, 1.92), (0.84, 2.0)]
        for (t0, a0), (t1, a1) in zip(keys, keys[1:]):
            if t <= t1:
                u = (t - t0) / (t1 - t0)
                u = u * u if a1 > a0 else 1 - (1 - u) * (1 - u)        # (falls fast, rises slowing)
                return a0 + (a1 - a0) * u
        return 2.0
    c.add("hatch_commander", "turn", lid)
    c.add("gun", "turn", lambda t: _damped(t, rad(1.2), 0.3, 0.12) + _ease(t, 0.6, 2.2, 0.0, rad(-7.0)))
    aerial_sway(c, lambda t, k: _damped(t, rad(-4.0 * k), 0.5, 0.6) * (1.0 - smoothstep(2.4, 3.0, t)),
                lambda t, k: _damped(t, rad(2.5 * k), 0.62, 0.55) * (1.0 - smoothstep(2.4, 3.0, t)))
    c.event(0.0, "hit", None, "tank_knocked_out")
    c.event(0.4, "hatch_blown", "socket_commander")
    c.event(1.0, "fire_loop", "socket_engine", "fire_loop")             # (the cue loops until the game stops it)
    C.append(c)
    return C


def bone_delta(part, chans, t):
    """The bone's own movement at time t (as a matrix in the rig's rest frame, about its pivot), from its channels."""
    R = Matrix.Identity(4)
    move = Vector((0, 0, 0))
    for kind, axis, fn in chans:
        a = fn(t)
        k = axis or part["axis"]
        if kind in ("turn", "spin"):
            R = R @ Matrix.Rotation(a, 4, turn_axis_b(k))
        elif kind == "slide":
            move += slide_axis_b(k) * a
    pv = part["pivot"]
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
        for part, kind, axis, fn in c.ch:
            if kind != "scroll":
                chans.setdefault(part, []).append((kind, axis, fn))
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
    """Put a clip on the rig and the scene's frame range on the clip."""
    reset_pose(arm)
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and act.slots:
        arm.animation_data.action_slot = act.slots[0]
    sc.frame_start = int(act.frame_range[0])
    sc.frame_end = int(act.frame_range[1])


def _simplify(keys, tol):
    """Drop keys that a straight line between their neighbours gives within tol."""
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
        for part, kind, axis, fn in c.ch:
            tol = {"slide": 2e-5, "scroll": 1e-6}.get(kind, 1e-4)
            keys = [[round(t, 5), round(fn(t), 6)] for t in c.times()]
            keys = _simplify(keys, tol)
            if all(abs(v) < 1e-7 for _, v in keys):
                continue
            tr = {"part": index[part], "part_name": part, kind: keys}
            if axis is not None:
                tr["axis"] = list(axis)
            tracks.append(tr)
        out[c.name] = {"long": c.long, "loop": c.loop, "loop_cue": c.loop_cue, "about": c.about, "tracks": tracks,
                       "events": c.events}
        if not c.loop:
            out[c.name]["after"] = "hold"                 # (the shared format's rule: what follows a clip that does not loop)
    return out


# ------------------------------------------------------------------------------------------------ the tank, put together
def assemble(lod=0, rig=True, belts=True):
    """The tank in the current scene: rig, sockets and the skinned mesh. belts=False leaves the belts out of the mesh,
    for the pictures to lay moving links instead."""
    parts = parts_table()
    arm = make_rig(parts) if rig else None
    g = build_geo(lod, belts)
    mesh = make_mesh(g, f"tank_LOD{lod}", arm)
    socks = make_sockets(arm) if rig else []
    return arm, mesh, socks, parts, g


# ------------------------------------------------------------------------------------------------ the pictures
FAST = False


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


MUD_DRY, MUD_WET, GRIME = kit.hex_rgb("7A6B52"), kit.hex_rgb("3E3528"), (0.022, 0.02, 0.016)


def weather(nt, name, c, P, Z, geo):
    """More wear for the pictures: dried mud caked on the running gear and the hull's lower parts and splashed higher,
    grime in the recesses (an occlusion shader finds them), rain streaks down the upright plates, soot round the
    exhaust's outlets. Returns the new colour."""
    if name == "glass":
        return c
    if Z is not None:
        cake = _math(nt, "MULTIPLY", _range(nt, Z, 1.2, 0.45), _range(nt, _noise(nt, P, 4.5, 6.0), 0.38, 0.52), clamp=True)
        spl = _math(nt, "MULTIPLY", _range(nt, Z, 1.65, 0.95), _range(nt, _noise(nt, P, 20.0, 2.0), 0.56, 0.62), clamp=True)
        mud = _math(nt, "MAXIMUM", cake, spl)
        mud_col = _mix(nt, MUD_WET, MUD_DRY, _range(nt, Z, 0.12, 0.55))      # (wetter and darker lowest down)
    else:                                                                    # (the belts: by their UVs)
        mud = _range(nt, _noise(nt, P, 7.0, 5.0), 0.45, 0.62, 0.0, 0.8)
        mud_col = _mix(nt, MUD_WET, MUD_DRY, 0.5)
    c = _mix(nt, c, mud_col, _math(nt, "MULTIPLY", mud, 0.92 if name not in ("canvas", "wood") else 0.5))
    # grime where light hardly reaches
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
    ao.samples = 6
    ao.only_local = True
    ao.inputs["Distance"].default_value = 0.18
    c = _mix(nt, c, GRIME, _range(nt, ao.outputs["AO"], 0.95, 0.35, 0.0, 0.7))
    if Z is None:
        return c
    sepn = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Normal"], sepn.inputs[0])
    upright = _range(nt, _math(nt, "ABSOLUTE", sepn.outputs["Z"]), 0.6, 0.25)
    if name in ("paint", "shade"):
        sv = nt.nodes.new("ShaderNodeVectorMath")
        sv.operation = "MULTIPLY"
        nt.links.new(P, sv.inputs[0])
        sv.inputs[1].default_value = (7.0, 7.0, 0.45)
        streak = _math(nt, "MULTIPLY", _range(nt, _noise(nt, sv.outputs[0], 1.0, 3.0), 0.5, 0.72, 0.0, 0.45), upright)
        c = _mix(nt, c, (0.05, 0.047, 0.04), streak)
    if name in ("paint", "shade", "rust", "steel"):
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(P, sep.inputs[0])
        q = nt.nodes.new("ShaderNodeCombineXYZ")
        nt.links.new(sep.outputs["X"], q.inputs[0])
        nt.links.new(_math(nt, "ABSOLUTE", sep.outputs["Y"]), q.inputs[1])
        nt.links.new(sep.outputs["Z"], q.inputs[2])
        d = nt.nodes.new("ShaderNodeVectorMath")
        d.operation = "DISTANCE"
        nt.links.new(q.outputs[0], d.inputs[0])
        d.inputs[1].default_value = (-3.07, 0.97, 0.98)
        soot = _math(nt, "MULTIPLY", _range(nt, d.outputs["Value"], 0.5, 0.08, 0.0, 0.9),
                     _range(nt, _noise(nt, P, 6.0, 4.0), 0.3, 0.6, 0.55, 1.0), clamp=True)
        c = _mix(nt, c, (0.012, 0.011, 0.01), soot)
    return c


def picture_materials(bounds, nation=NATION):
    """For the pictures only: the same materials (by paint name) given some wear. Paint mottled in large soft patches,
    dust settling on the lower hull and the running gear and on the flat tops, edges rubbed lighter (a bevel shader
    tells the edges), tracks dark steel polished on the grousers. Positions are taken in the tank's rest frame (from the
    mesh's undeformed coordinates), so the dust stays put on a turning wheel. `bounds` = (min, size) of the rest mesh."""
    mn, size = bounds
    paint = kit.NATIONS[nation]["paint"]
    dust = kit.hex_rgb("9A8B6A")
    base = {"paint": kit.hex_rgb(paint), "shade": kit.hex_rgb(paint, 0.66), "rubber": (0.022, 0.021, 0.02), "wood": (0.25, 0.15, 0.07),
            "steel": (0.30, 0.30, 0.29), "canvas": (0.17, 0.15, 0.09), "glass": (0.03, 0.05, 0.06), "rust": (0.11, 0.05, 0.028),
            "track": (0.05, 0.047, 0.044), "stripe": kit.hex_rgb("D4B44E")}
    for name in PAINT_NAMES:
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        nt, bsdf = _nodes(m)
        tc = nt.nodes.new("ShaderNodeTexCoord")
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        if name == "track":
            P = tc.outputs["UV"]
            Z = None
        else:
            mp = nt.nodes.new("ShaderNodeMapping")
            mp.inputs["Location"].default_value = mn
            mp.inputs["Scale"].default_value = size
            nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
            P = mp.outputs["Vector"]
            sep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(P, sep.inputs[0])
            Z = sep.outputs["Z"]
        big = _noise(nt, P, 0.9, 3.0)
        fine = _noise(nt, P, 14.0, 6.0, 0.6)
        col = base[name] if name != "rust" else _mix(nt, (0.075, 0.034, 0.02), (0.30, 0.11, 0.035), _range(nt, _noise(nt, P, 9.0, 6.0), 0.4, 0.7))
        # mottling: soft patches lighter and darker
        mot = nt.nodes.new("ShaderNodeValToRGB")
        mot.color_ramp.elements[0].position, mot.color_ramp.elements[0].color = 0.3, (0.76, 0.76, 0.74, 1)
        mot.color_ramp.elements[1].position, mot.color_ramp.elements[1].color = 0.7, (1.16, 1.13, 1.08, 1)
        nt.links.new(big, mot.inputs["Fac"])
        c = _mix(nt, col, mot.outputs["Color"], 1.0 if name not in ("glass",) else 0.0, "MULTIPLY")
        c = _mix(nt, c, _range(nt, fine, 0.3, 0.7, 0.92, 1.06), 1.0, "MULTIPLY") if name not in ("glass",) else c
        # wear on the edges
        rough = 0.72
        metal_edge = 0.0
        if name in ("paint", "shade", "track", "steel", "rust", "wood"):
            bev = nt.nodes.new("ShaderNodeBevel")
            bev.samples = 4
            bev.inputs["Radius"].default_value = 0.012
            dot = nt.nodes.new("ShaderNodeVectorMath")
            dot.operation = "DOT_PRODUCT"
            nt.links.new(bev.outputs["Normal"], dot.inputs[0])
            nt.links.new(geo.outputs["Normal"], dot.inputs[1])
            edge = _range(nt, dot.outputs["Value"], 0.985, 0.85)
            edge = _math(nt, "MULTIPLY", edge, _range(nt, fine, 0.35, 0.65, 0.2, 1.0), clamp=True)
            worn = {"paint": (0.30, 0.31, 0.30), "shade": (0.22, 0.23, 0.22), "track": (0.32, 0.31, 0.29), "steel": (0.5, 0.5, 0.48),
                    "rust": (0.2, 0.11, 0.06), "wood": (0.36, 0.25, 0.14)}[name]
            c = _mix(nt, c, worn, _math(nt, "MULTIPLY", edge, 0.85 if name != "track" else 1.0))
            metal_edge = edge
        # dust: thick low down, thin on the flat tops
        if name not in ("glass",):
            if Z is not None:
                low = _range(nt, Z, 1.45, 0.15, 0.0, 0.72)
            else:
                low = None
            top = _range(nt, geo.outputs["Normal"], 0.0, 1.0)       # (the vector's first part; replaced below)
            sepn = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(geo.outputs["Normal"], sepn.inputs[0])
            top = _range(nt, sepn.outputs["Z"], 0.55, 0.95, 0.0, 0.28)
            amount = top if low is None else _math(nt, "MAXIMUM", low, top)
            if name in ("track", "rubber"):
                amount = _math(nt, "ADD", amount, 0.3 if name == "track" else 0.12)
            patchy = _range(nt, _noise(nt, P, 3.2, 5.0), 0.35, 0.65, 0.35, 1.0)
            amount = _math(nt, "MULTIPLY", amount, patchy, clamp=True)
            c = _mix(nt, c, dust, amount)
            c = weather(nt, name, c, P, Z, geo)
            rough_n = _math(nt, "ADD", _math(nt, "MULTIPLY", amount, 0.25), {"steel": 0.42, "track": 0.55, "rubber": 0.85, "glass": 0.05,
                                                                              "rust": 0.85, "wood": 0.75, "canvas": 0.9}.get(name, 0.68), clamp=True)
            nt.links.new(rough_n, bsdf.inputs["Roughness"])
        else:
            bsdf.inputs["Roughness"].default_value = 0.04
            bsdf.inputs["Specular IOR Level"].default_value = 0.8
        nt.links.new(c, bsdf.inputs["Base Color"])
        if name in ("steel", "track"):
            bsdf.inputs["Metallic"].default_value = 0.65 if name == "steel" else 0.45
        elif metal_edge:
            nt.links.new(_math(nt, "MULTIPLY", metal_edge, 0.6), bsdf.inputs["Metallic"])
        if name == "canvas" and "Sheen Weight" in bsdf.inputs:
            bsdf.inputs["Sheen Weight"].default_value = 0.3
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.08 if name not in ("glass", "rubber") else 0.03
        bump.inputs["Distance"].default_value = 0.002
        nt.links.new(fine, bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])


class MovingBelts:
    """For the pictures: the belts as every link, laid round the path each frame with the clip's travel and the hull's
    rocking (the export's belt is fixed and the game scrolls its U instead)."""

    def __init__(self, parts):
        self.tv, self.tf, _ = link_template(0)
        self.hull = {p["name"]: p for p in parts}["hull"]
        self.objs = []
        nv = len(self.tv)
        _, _, _, _, _, _, smid = link_frames(0.0)
        mat = bpy.data.materials.get("track") or materials_plain()["track"]
        for side in (1.0, -1.0):
            faces, uvs = [], []
            for k in range(PATH.links):
                for f in self.tf:
                    faces.append([k * nv + i for i in f])
                    uvs += [c for i in f for c in ((k * 0.37 + self.tv[i, 0]) * 1.0, 0.5 + self.tv[i, 2] / TRACK_W)]
            me = bpy.data.meshes.new("belt_" + ("l" if side > 0 else "r"))
            me.from_pydata([(0.0, 0.0, 0.0)] * (PATH.links * nv), [], faces)
            me.materials.append(mat)
            me.uv_layers.new(name="UVMap").data.foreach_set("uv", uvs)
            me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
            me.set_sharp_from_angle(angle=math.radians(40.0))
            ob = bpy.data.objects.new(me.name, me)
            bpy.context.scene.collection.objects.link(ob)
            self.objs.append((ob, side))
        self.clip = None
        self.place(0.0, Matrix.Identity(4))

    def place(self, travel, H):
        cx, cz, tx, tz, ox, oz, _ = link_frames(travel)
        tv = self.tv
        Hn = np.array(H)
        for ob, side in self.objs:
            X = cx[:, None] + tv[None, :, 0] * tx[:, None] + tv[None, :, 1] * ox[:, None]
            Z = cz[:, None] + tv[None, :, 0] * tz[:, None] + tv[None, :, 1] * oz[:, None]
            Y = tuck(X, np.broadcast_to(side * TRACK_Y + tv[None, :, 2], X.shape), Z)
            P = np.stack([X.ravel(), Y.ravel(), Z.ravel(), np.ones(X.size)])
            ss = lambda e0, e1, x: np.clip((x - e0) / (e1 - e0), 0, 1) ** 2 * (3 - 2 * np.clip((x - e0) / (e1 - e0), 0, 1))
            w = np.maximum(ss(0.32, 0.8, P[2]), ss(2.0, 2.45, np.abs(P[0])))
            Ph = Hn @ P
            Q = P * (1 - w) + Ph * w
            ob.data.vertices.foreach_set("co", Q[:3].T.ravel().astype(np.float32))
            ob.data.update()

    def at_time(self, t):
        if self.clip is None:
            self.place(0.0, Matrix.Identity(4))
            return
        travel = self.clip.travel(t) if self.clip.travel else 0.0
        chans = [(kind, axis, fn) for part, kind, axis, fn in self.clip.ch if part == "hull"]
        H = bone_delta(self.hull, chans, t) if chans else Matrix.Identity(4)
        self.place(travel, H)

    def follow(self, clip):
        """Lay the links by `clip` on every frame change (frame 1 is time 0)."""
        self.clip = clip
        for h in list(bpy.app.handlers.frame_change_post):
            if getattr(h, "__name__", "") == "_tank_belts":
                bpy.app.handlers.frame_change_post.remove(h)

        def _tank_belts(scene, depsgraph=None):
            self.at_time(scene.frame_current / FPS)
        bpy.app.handlers.frame_change_post.append(_tank_belts)


def rest_bounds(obj):
    vs = np.array([v.co[:] for v in obj.data.vertices])
    mn, mx = vs.min(0), vs.max(0)
    return tuple(mn), tuple(np.maximum(mx - mn, 1e-6))


MARK_NUMBER = "214"


def _mark_images():
    """The pictures' markings as images: the nation's plain cross (white-edged, for German paint; other nations a plain
    white disc for now) and the turret number in white edged dark, as the game paints them."""
    from PIL import Image, ImageDraw, ImageFont
    d = os.path.join(WORK, "marks")
    os.makedirs(d, exist_ok=True)
    N = 256
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    dr = ImageDraw.Draw(im)
    if NATION == "GER":
        w, b = 0.36 * N, 0.07 * N                   # an arm's width; the white edge
        for (x0, y0, x1, y1), col in (((0, (N - w) / 2, N, (N + w) / 2), (235, 232, 222, 255)), (((N - w) / 2, 0, (N + w) / 2, N), (235, 232, 222, 255)),
                                      ((b, (N - w) / 2 + b, N - b, (N + w) / 2 - b), (18, 18, 18, 255)),
                                      (((N - w) / 2 + b, b, (N + w) / 2 - b, N - b), (18, 18, 18, 255))):
            dr.rectangle((x0, y0, x1, y1), fill=col)
    else:
        dr.ellipse((8, 8, N - 8, N - 8), fill=(235, 232, 222, 255))
    cross = os.path.join(d, "cross.png")
    im.save(cross)
    font = None
    for fp in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if os.path.exists(fp):
            font = ImageFont.truetype(fp, 200)
            break
    font = font or ImageFont.load_default()
    tw = int(ImageDraw.Draw(im).textlength(MARK_NUMBER, font=font)) + 40
    im = Image.new("RGBA", (tw, 256), (0, 0, 0, 0))
    ImageDraw.Draw(im).text((20, 18), MARK_NUMBER, font=font, fill=(232, 228, 216, 255), stroke_width=9, stroke_fill=(20, 20, 20, 255))
    num = os.path.join(d, "number.png")
    im.save(num)
    return cross, num, tw / 256.0


def _decal_material(name, path):
    m = bpy.data.materials.new(name)
    nt, bsdf = _nodes(m)
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(path)
    tex.extension = "CLIP"
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nt.links.new(tc.outputs["UV"], tex.inputs["Vector"])
    wear = _noise(nt, tc.outputs["Object"], 30.0, 4.0)
    chips = _range(nt, wear, 0.3, 0.36)                      # (small chips gone from the paint)
    nt.links.new(_mix(nt, tex.outputs["Color"], _range(nt, _noise(nt, tc.outputs["Object"], 6.0), 0.3, 0.7, 0.72, 1.0), 1.0, "MULTIPLY"),
                 bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.75
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(_math(nt, "MULTIPLY", tex.outputs["Alpha"], chips), mix.inputs["Fac"])
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(bsdf.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m


def paint_marks(arm, mesh):
    """For the pictures: the cross on the turret's sides and the number on the hull's, where build_tank.marks_of() puts
    them (found on our own plates by casting in from the side), parented to the turret and hull bones."""
    from mathutils.bvhtree import BVHTree
    mk = bt.marks_of(S)
    cross_png, num_png, aspect = _mark_images()
    mats = {"cross": _decal_material("mark_cross", cross_png), "number": _decal_material("mark_number", num_png)}
    reset_pose(arm)
    bpy.context.view_layer.update()
    bvh = BVHTree.FromObject(mesh, bpy.context.evaluated_depsgraph_get())
    out = []
    for key, bone in (("cross", "turret"), ("number", "hull")):
        x, _, z, size, _ = mk[key]
        for side in (1.0, -1.0):                             # (Blender's +y is the left)
            hit, nrm, _, _ = bvh.ray_cast(Vector((x, side * 3.0, z)), Vector((0, -side, 0)))
            if hit is None:
                continue
            nrm = nrm.normalized()
            right = Vector((0, -side, 0)).cross(Vector((0, 0, 1)))       # (the viewer's right, looking in at this side)
            up = nrm.cross(right).normalized()
            right = up.cross(nrm).normalized()
            hw, hh = (size * 0.5, size * 0.5) if key == "cross" else (size * aspect * 0.5, size * 0.5)
            c = hit + nrm * 0.004
            pts = [c - right * hw - up * hh, c + right * hw - up * hh, c + right * hw + up * hh, c - right * hw + up * hh]
            me = bpy.data.meshes.new(f"mark_{key}")
            me.from_pydata([tuple(p) for p in pts], [], [(0, 1, 2, 3)])
            me.uv_layers.new(name="UVMap").data.foreach_set("uv", [0, 0, 1, 0, 1, 1, 0, 1])
            me.materials.append(mats[key])
            ob = bpy.data.objects.new(f"mark_{key}_{'l' if side > 0 else 'r'}", me)
            bpy.context.scene.collection.objects.link(ob)
            ob.visible_shadow = False
            ob.parent = arm
            ob.parent_type = "BONE"
            ob.parent_bone = bone
            bpy.context.view_layer.update()
            ob.matrix_world = Matrix.Identity(4)
            out.append(ob)
    return out


class Stage:
    """The tank in a fresh scene for pictures: rigged, its clips as actions, moving belts, worn paint, the sun and sky."""

    def __init__(self, ground=True, nation=NATION):
        self.sc = kit.fresh(FPS)
        self.arm, self.mesh, self.socks, self.parts, self.geo = assemble(0, belts=False)
        self.clips = {c.name: c for c in clips()}
        self.acts = make_actions(self.arm, self.parts, list(self.clips.values()))
        self.belts = MovingBelts(self.parts)
        self.bounds = rest_bounds(self.mesh)
        picture_materials(self.bounds, nation)
        self.decals = paint_marks(self.arm, self.mesh)
        self.sun = kit.studio()
        self.ground = kit.field_ground() if ground else None
        self.cam = kit.camera(lens=50)
        for e in self.socks:
            e.hide_render = True

    def play(self, name):
        play(self.arm, self.acts[name], self.sc)
        self.belts.follow(self.clips[name])
        self.sc.frame_set(0)

    def rest(self):
        reset_pose(self.arm)
        self.arm.animation_data.action = None
        self.belts.follow(Clip("rest", 0.1, False))
        self.belts.clip = None
        self.belts.at_time(0.0)


def load_old_tank(path=OLD_JSON):
    """Today's campaign piece, from its JSON (near mesh), painted by number with the same materials."""
    with open(path) as f:
        d = json.load(f)
    n = d["near"]
    P, T, pa = n["points"], n["triangles"], n["paints"]
    me = bpy.data.meshes.new("old_tank")
    me.from_pydata([(P[i], -P[i + 1], P[i + 2]) for i in range(0, len(P), 3)], [], [(T[i], T[i + 2], T[i + 1]) for i in range(0, len(T), 3)])
    for name in PAINT_NAMES:
        me.materials.append(bpy.data.materials.get(name) or materials_plain()[name])
    me.polygons.foreach_set("material_index", [pa[p.vertices[0]] for p in me.polygons])
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    me.set_sharp_from_angle(angle=math.radians(40.0))
    ob = bpy.data.objects.new("old_tank", me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


VIEWS = {
    # name: (target, distance, pitch, yaw, lens)
    "front": ((0.3, 0.0, 1.05), 12.5, 16.0, 38.0, 50.0),
    "rear": ((-0.2, 0.0, 1.1), 12.5, 18.0, 148.0, 50.0),
    "gear": ((1.45, 1.2, 0.55), 4.6, 10.0, 62.0, 40.0),
    "game": ((0.0, 0.0, 0.9), 16.0, 30.0, 40.0, 40.0),
}


def aim(cam, view):
    target, dist, pitch, yaw, lens = VIEWS[view] if isinstance(view, str) else view
    cam.data.lens = lens
    kit.orbit(cam, target, dist, pitch, yaw)


def pic_path(*names):
    return os.path.join(OUT, "pictures", *names)


def stamp(path, text):
    """A small caption burnt into the corner of a still."""
    from PIL import Image, ImageDraw
    im = Image.open(path).convert("RGB")
    d = ImageDraw.Draw(im)
    f = kit._font(max(14, im.height // 34))
    d.rectangle((0, im.height - f.size - 14, im.width, im.height), fill=(232, 226, 214))
    d.text((10, im.height - f.size - 9), text, fill=(40, 34, 28), font=f)
    im.save(path)


def before_after():
    """Today's MediumTank and the battle tank, the same camera and light, German paint."""
    t0 = time.time()
    st = Stage()
    st.rest()
    aim(st.cam, "front")
    render_cfg()
    tmp = os.path.join(WORK, "ba")
    os.makedirs(tmp, exist_ok=True)
    kit.still(os.path.join(tmp, "after.png"))
    st.mesh.hide_render = True
    for ob in [o for o, _ in st.belts.objs] + st.decals:
        ob.hide_render = True
    load_old_tank()
    kit.still(os.path.join(tmp, "before.png"))
    out = pic_path("before-after.png")
    kit.sheet([os.path.join(tmp, "before.png"), os.path.join(tmp, "after.png")], out, cols=2,
              labels=[f"Before: today's campaign MediumTank ({_old_tris()} triangles)", f"After: the battle tank (LOD0, {st.geo.tris() + belt_tris_static()} triangles)"],
              title="Before and after: same camera, same light, German paint 4E5459")
    print(f"before-after: {out} ({time.time() - t0:.0f} s)")
    return out


def _old_tris():
    with open(OLD_JSON) as f:
        return len(json.load(f)["near"]["triangles"]) // 3


def belt_tris_static():
    g = Geo()
    belt_links(g, 1.0, 0, cull=True)
    belt_links(g, -1.0, 0, cull=True)
    return g.tris()


def stills():
    """The four views of the contact sheet, and each alone."""
    t0 = time.time()
    st = Stage()
    st.rest()
    render_cfg()
    paths = []
    labels = {"front": "Front three-quarter", "rear": "Rear three-quarter", "gear": "Running gear, close", "game": "The game's camera (16 m, 30 degrees down)"}
    for v in ("front", "rear", "gear", "game"):
        aim(st.cam, v)
        p = os.path.join(WORK, "stills", v + ".png")
        kit.still(p)
        paths.append(p)
    kit.sheet(paths, pic_path("sheet.png"), cols=2, labels=[labels[v] for v in ("front", "rear", "gear", "game")],
              title="The battle tank: our generic medium tank, rigged and animated (German paint)")
    print(f"stills: {time.time() - t0:.0f} s")


def turntable(frames=90, fps=15, w=None, h=None, samples=None):
    t0 = time.time()
    st = Stage()
    st.rest()
    st.cam.data.lens = 50
    kit.turntable(st.cam, (0.0, 0.0, 1.0), 13.0, 17.0, frames, start_yaw=30.0, frame0=1)
    render_cfg(w, h, samples)
    folder = os.path.join(WORK, "turntable")
    shutil.rmtree(folder, ignore_errors=True)
    kit.frames(folder, 1, frames)
    kit.encode(folder, pic_path("turntable"), fps=fps)
    print(f"turntable: {frames} frames, {time.time() - t0:.0f} s")


CLIP_VIEWS = {
    "drive": ((0.2, 0.0, 0.8), 14.0, 24.0, 52.0, 45.0),
    "fire": ((0.6, 0.0, 1.2), 13.0, 22.0, 62.0, 45.0),
    "knocked_out": ((0.0, 0.0, 1.2), 14.0, 28.0, 32.0, 45.0),
    "idle": ((0.0, 0.0, 1.0), 14.0, 25.0, 40.0, 45.0),
    "stop": ((0.2, 0.0, 0.9), 14.0, 22.0, 70.0, 45.0),
    "mg_burst": ((0.6, 0.0, 1.2), 13.0, 22.0, 40.0, 45.0),
}


def picture_fx(st, name):
    """Picture-only effects so a clip reads (the game draws its own from the events): a muzzle flash for `fire`, the
    hit and a fire's glow in the hatch for `knocked_out`."""
    def flash(at, size, frames_on, colour=(1.0, 0.75, 0.4), power=60.0, parent=None, bone=None):
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=2, radius=size)
        me = bpy.data.meshes.new("flash")
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new("flash", me)
        st.sc.collection.objects.link(ob)
        m = bpy.data.materials.new("flash")
        nt, bsdf = _nodes(m)
        bsdf.inputs["Emission Color"].default_value = (*colour, 1)
        bsdf.inputs["Emission Strength"].default_value = power
        bsdf.inputs["Alpha"].default_value = 0.85
        me.materials.append(m)
        if parent is not None:
            ob.parent = parent
            ob.parent_type = "BONE" if bone else "OBJECT"
            if bone:
                ob.parent_bone = bone
            bpy.context.view_layer.update()
        ob.matrix_world = Matrix.Translation(at)
        for f in range(0, int(st.sc.frame_end) + 1):
            ob.hide_render = f not in frames_on
            ob.keyframe_insert("hide_render", frame=f)
        return ob
    if name == "fire":
        tip = Vector((GUN_END + 0.35, 0.0, GZ))
        flash(tip, 0.35, (0, 1), power=7.0, parent=st.arm, bone="barrel").scale = (1.7, 1.0, 1.0)
        flash(tip + Vector((0.55, 0, 0)), 0.22, (0,), colour=(1.0, 0.55, 0.2), power=5.0, parent=st.arm, bone="barrel")
    if name == "knocked_out":
        flash(Vector((1.2, -0.95, 2.05)), 0.3, (0, 1), colour=(1.0, 0.75, 0.45), power=9.0)
        flash(CMDR + Vector((0, 0, 0.02)), 0.2, tuple(range(13, 200)), colour=(1.0, 0.4, 0.1), power=4.0, parent=st.arm,
              bone="turret").scale = (1.0, 1.0, 0.3)
        lamp = bpy.data.lights.new("fire", "POINT")
        lamp.energy = 0.0
        lamp.color = (1.0, 0.5, 0.2)
        lo = bpy.data.objects.new("fire", lamp)
        st.sc.collection.objects.link(lo)
        lo.location = CMDR + Vector((0, 0, 0.45))
        for f in range(0, int(st.sc.frame_end) + 1):
            lamp.energy = 0.0 if f < 12 else 45.0 * (0.75 + 0.25 * math.sin(f * 1.7) * math.sin(f * 0.6))
            lamp.keyframe_insert("energy", frame=f)


def ground_motion(st, c):
    """For a clip that travels: slide the field back under the tank at the clip's speed, with stones on it and the
    belts' marks pressed into it behind the tank, so the movement reads. For a loop the field repeats itself every
    loop's travel (its pattern laid round a circle), so the movie loops with no jump."""
    import random
    if not c.travel or st.ground is None:
        return
    g = st.ground
    for f in range(int(st.sc.frame_start), int(st.sc.frame_end) + 1):
        g.location.x = -c.travel(f / FPS)
        g.keyframe_insert("location", index=0, frame=f)
    kit._linear(g)
    period = c.travel(c.long) if c.loop else None
    nt = g.data.materials[0].node_tree
    if period:
        tc = next(n for n in nt.nodes if n.type == "TEX_COORD")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        ang = _math(nt, "MULTIPLY", sep.outputs[0], 2.0 * math.pi / period)
        R = period / (2.0 * math.pi)
        comb = nt.nodes.new("ShaderNodeCombineXYZ")
        nt.links.new(_math(nt, "MULTIPLY", _math(nt, "COSINE", ang), R), comb.inputs[0])
        nt.links.new(_math(nt, "MULTIPLY", _math(nt, "SINE", ang), R), comb.inputs[1])
        nt.links.new(sep.outputs[1], comb.inputs[2])
        for n in [n for n in nt.nodes if n.type == "TEX_NOISE"]:
            nt.links.new(comb.outputs[0], n.inputs["Vector"])
    # stones (repeating every `period` for a loop)
    rng = random.Random(7)
    tile = period or 80.0
    reps = range(-int(36 / tile) - 1, int(40 / tile) + 2) if period else [0]
    stones = []
    for _ in range(int(26 * tile) if period else 2000):
        y = rng.uniform(1.65, 24.0) * rng.choice((-1.0, 1.0))
        big = rng.random() < 0.12
        stones.append((rng.uniform(0.0, tile) - (0 if period else 40.0), y,
                       rng.uniform(0.07, 0.17) if big else rng.uniform(0.025, 0.06), rng.uniform(0.0, 6.283),
                       rng.uniform(0.35, 0.65), rng.randrange(1 << 30)))
    bm = bmesh.new()
    for x0, y, sz, a, flat, seed in stones:
        r2 = random.Random(seed)
        jit = [r2.uniform(0.75, 1.2) for _ in range(42)]
        for rep in reps:
            vs = bmesh.ops.create_icosphere(bm, subdivisions=1, radius=sz)["verts"]
            ca, sa = math.cos(a), math.sin(a)
            for k, v in enumerate(vs):
                px, py, pz = v.co * jit[k % 42]
                v.co = Vector((x0 + rep * tile + ca * px - sa * py * 0.8, y + sa * px + ca * py * 0.8, pz * flat + sz * flat * 0.35))
    me = bpy.data.meshes.new("stones")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(kit.material("stones", (0.30, 0.27, 0.22), rough=0.9))
    ob = bpy.data.objects.new("stones", me)
    st.sc.collection.objects.link(ob)
    ob.parent = g
    # the belts' marks: dark pressed earth under each belt from the front road wheel back, with a grouser print every
    # link that moves with the field (so the belt's bottom run sits still on its own prints)
    m = bpy.data.materials.new("marks")
    mt, bsdf = _nodes(m)
    tc = mt.nodes.new("ShaderNodeTexCoord")
    tc.object = g
    sep = mt.nodes.new("ShaderNodeSeparateXYZ")
    mt.links.new(tc.outputs["Object"], sep.inputs[0])
    ph = _math(mt, "FRACT", _math(mt, "DIVIDE", sep.outputs[0], PATH.pitch))
    groove = _range(mt, _math(mt, "ABSOLUTE", _math(mt, "SUBTRACT", ph, 0.5)), 0.30, 0.42)
    mt.links.new(_mix(mt, (0.075, 0.062, 0.042), (0.13, 0.11, 0.075), groove), bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.9
    bump = mt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.6
    mt.links.new(groove, bump.inputs["Height"])
    mt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    front = WHEEL_XS[0]
    for side in (1.0, -1.0):
        bm = bmesh.new()
        w = TRACK_W * 0.5 + 0.02
        vs = [bm.verts.new((x, side * TRACK_Y + d, 0.004)) for x, d in ((-60.0, -w), (front, -w), (front, w), (-60.0, w))]
        bm.faces.new(vs)
        me = bpy.data.meshes.new("marks")
        bm.to_mesh(me)
        bm.free()
        me.materials.append(m)
        mo = bpy.data.objects.new("marks", me)
        st.sc.collection.objects.link(mo)


def clip_movie(name, w=None, h=None, samples=None, repeat=1):
    """One clip from a low, close, game-like camera. The ground slides back under a driving tank at its own speed (the
    clips are in place), so you can see the bottom of the belt keep still on the ground as it should."""
    t0 = time.time()
    st = Stage()
    st.play(name)
    aim(st.cam, CLIP_VIEWS[name])
    picture_fx(st, name)
    c = st.clips[name]
    ground_motion(st, c)
    render_cfg(w, h, samples)
    folder = os.path.join(WORK, "clip_" + name)
    shutil.rmtree(folder, ignore_errors=True)
    end = int(st.sc.frame_end) - (1 if c.loop else 0)          # (a loop's last frame is its first)
    kit.frames(folder, 0, end)
    kit.encode(folder, pic_path(name), fps=FPS)                 # (the gif loops by itself)
    if repeat > 1:                                              # (the mp4 plays the loop `repeat` times)
        files = sorted(os.listdir(folder))
        n = len(files)
        for r in range(1, repeat):
            for i, fn in enumerate(files):
                shutil.copy(os.path.join(folder, fn), os.path.join(folder, f"{r * n + i + 1:04d}.png"))
        kit.encode(folder, pic_path(name), fps=FPS, gif=False)
    print(f"clip {name}: {end} frames, {time.time() - t0:.0f} s")


# ------------------------------------------------------------------------------------------------ the battle pictures
# Brian's hero clip and the knocked-out clip on the battle stage (battle_set.py): the tank driven over the ground in the
# world (its clips are in place; here a carrier moves it), with smoke, dust and flame as soft puffs.
WIND = Vector((1.4, 0.9, 0.0))                          # m/s, toward the east-north-east
HERO_START = Vector((46.0, -2.0, 0.0))                  # on the stubble field east of the road
HERO_HEADING = 204.8                                    # degrees from +x (toward the west-south-west)
HERO_CAM = Vector((27.5, -19.9, 0.0))                   # front-left of where the tank stops
HERO_LONG, HERO_V, HERO_V2 = 8.0, 3.2, 2.6              # seconds; speed in and out (m/s)
T_BRAKE, T_STOP, T_FIRE, T_GO, T_RUN = 2.6, 3.4, 4.0, 6.2, 7.4
TURRET_TO, GUN_TO = 55.0, 1.5                           # where the gun is laid (degrees: right, up)


# The smoke, dust, flash and fire are the warship modeller's tested effects (battle-models/shared/sea-air/stage.py,
# class Effects, copied unchanged so land and sea match), with tank-sized makers of our own below it.
import random


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
            r = _math(nt, "SQRT", _math(nt, "ADD", _math(nt, "MULTIPLY", sep.outputs[0], sep.outputs[0]), _math(nt, "MULTIPLY", sep.outputs[1], sep.outputs[1])))
            soft = _math(nt, "SUBTRACT", 1.0, _math(nt, "ADD", r, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", nz.outputs[0], 0.5), 0.9)), clamp=True)
            soft = _math(nt, "MULTIPLY", soft, 1.7, clamp=True)
        else:                    # a puff: thick where you look into it, thinning to nothing at its edge
            lw = nt.nodes.new("ShaderNodeLayerWeight")
            lw.inputs["Blend"].default_value = 0.5
            face = _math(nt, "SUBTRACT", 1.0, lw.outputs["Facing"], clamp=True)
            soft = _math(nt, "POWER", face, {"smoke": 2.4, "water": 1.9, "glow": 1.3}[kind])
            soft = _math(nt, "MULTIPLY", soft, _math(nt, "SUBTRACT", _math(nt, "MULTIPLY", nz.outputs[0], 1.9), 0.35, clamp=True), clamp=True)
        alpha = _math(nt, "MULTIPLY", soft, info.outputs["Alpha"], clamp=True)
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
    def funnel_smoke(self, where, thick=0.5, r=2.4, start=-16.0, dark=0.3, end=1e9):
        g = self.rng

        def make(t, at, facing):
            shade = dark + g.uniform(-0.04, 0.04)
            self.puff(t, at + Vector((g.uniform(-0.6, 0.6), g.uniform(-0.6, 0.6), 0)), vel=facing * 7.0 + Vector((g.uniform(-0.6, 0.6), g.uniform(-0.8, 0.8), g.uniform(0, 1.5))),
                      life=g.uniform(12.0, 16.0), r=(r, r * 4.2), col=(shade, shade * 0.97, shade * 0.93), alpha=thick, drag=0.6, rise=0.9, fade_in=0.15, grow=0.7)
        self.stream(where, 0.3, make, start=start, end=end)

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

    def splash(self, t0, at, high=50.0, r=5.0):
        """A heavy shell falls in the sea: a tall white column that stands a moment and falls back, and a ring of foam."""
        g = self.rng
        at = Vector(at)

        def make(t, p0, facing):
            n = 20
            for i in range(n):
                f = (i + 0.5) / n
                v0 = math.sqrt(2 * 9.8 * high * f) * g.uniform(0.92, 1.05)
                self.puff(t + 0.006 * i, p0 + Vector((g.uniform(-1, 1), g.uniform(-1, 1), 0.5)), vel=(g.uniform(-2.5, 2.5) * (1 - f), g.uniform(-2.5, 2.5) * (1 - f), v0), life=g.uniform(3.4, 4.6),
                          r=(r * 0.4, r * g.uniform(0.7, 1.15) * (1.3 - 0.6 * f)), col=(0.95, 0.97, 0.97), col2=(0.88, 0.92, 0.92), alpha=0.75, kind="water", gravity=9.8, wind=0.06,
                          squash=(1.0, 1.0, 2.3), fade_in=0.04, grow=0.3, hold=0.4)
            self.puff(t, p0 + Vector((0, 0, 0.35)), life=7.0, r=(r * 1.4, r * 4.5), col=(0.92, 0.95, 0.95), alpha=0.75, kind="flat", wind=0.0, squash=(1.0, 1.0, 0.04), fade_in=0.2, grow=0.5, hold=0.3)
        self.once(t0, (at, Vector((0, 0, 1))), make)

    def bow_spray(self, where, start=-2.0, end=1e9):
        """The water she throws from her stem."""
        g = self.rng

        def make(t, at, facing):
            for hand in (1.0, -1.0):
                self.puff(t, at + Vector((-g.uniform(0, 7), hand * g.uniform(0.6, 2.6), 0.4)), vel=(g.uniform(1, 4), hand * g.uniform(3, 7), g.uniform(4.5, 8.5)), life=g.uniform(1.0, 1.6),
                          r=(0.8, g.uniform(1.8, 2.8)), col=(0.95, 0.97, 0.97), alpha=0.4, kind="water", gravity=9.8, wind=0.9, fade_in=0.08, grow=0.5)
        self.stream(where, 0.12, make, start=start, end=end)

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

    def froth(self, where, start, end, r=9.0):
        """Where she goes under: the water boils white."""
        g = self.rng

        def make(t, at, facing):
            self.puff(t, Vector((at[0] + g.uniform(-r, r) * 1.5, at[1] + g.uniform(-r, r), 0.3)), life=g.uniform(3.5, 5.5), r=(r * 0.5, r * g.uniform(1.1, 1.8)), col=(0.9, 0.93, 0.93), alpha=0.8,
                      kind="flat", wind=0.0, squash=(1.0, 1.0, 0.05), fade_in=0.4, grow=0.6, hold=0.3)
        self.stream(where, 0.3, make, start=start, end=end)

    def slick(self, t0, at, r=40.0):
        """What she leaves: a small ragged dark slick of oil."""
        g = self.rng

        def make(t, p0, facing):
            for i in range(5):
                self.puff(t + i * 0.4, Vector((p0[0] + g.uniform(-r, r) * 0.5, p0[1] + g.uniform(-r, r) * 0.3, 0.25)), life=60.0, r=(r * 0.25, r * g.uniform(0.5, 0.9)), col=(0.05, 0.06, 0.07),
                          alpha=0.55, kind="flat", wind=0.0, squash=(1.5, 1.0, 0.03), fade_in=3.0, grow=0.5, hold=0.9)
        self.once(t0, (Vector(at), Vector((0, 0, 1))), make)


DUST, DUST2 = (0.47, 0.39, 0.27), (0.58, 0.51, 0.38)      # tan, paling as it thins


def tank_gun(fx, t0, where, ground):
    """The tank's gun fires: a short tongue of flame along the bore, a ring of grey smoke thrown out round the muzzle,
    a plume forward along the line, and dust blown up off the ground in front of the gun (and a flash of light)."""
    g = fx.rng

    def make(t, at, facing):
        side = facing.cross(Vector((0, 0, 1))).normalized()
        up = side.cross(facing)
        q = facing.to_track_quat("X", "Z")
        for i in range(3):
            p = fx.puff(t, at + facing * (0.25 + 0.3 * i), vel=facing * 5.0, life=0.04 + 0.01 * i, r=(0.12, 0.42 - 0.08 * i), col=(0.9, 0.5, 0.15),
                        col2=(0.8, 0.3, 0.06), alpha=1.0, kind="glow", drag=6.0, wind=0.0, squash=(2.0, 1.0, 1.0), fade_in=0.01, grow=0.4)
            p["ob"].rotation_euler = q.to_euler()
        for i in range(24):                                  # the ring: thrown out round the muzzle, it stands and thins
            a = 2 * math.pi * i / 24 + g.uniform(-0.08, 0.08)
            out = side * math.cos(a) + up * math.sin(a)
            c = g.uniform(0.52, 0.62)
            fx.puff(t + 0.02, at + facing * 0.3, vel=out * g.uniform(4.5, 6.0) + facing * g.uniform(2.0, 3.5), life=g.uniform(4.0, 5.5),
                    r=(0.2, g.uniform(0.75, 1.0)), col=(0.6, 0.53, 0.43), col2=(c, c, c * 0.97), alpha=0.6, drag=3.0, rise=0.25, fade_in=0.03,
                    grow=0.5, wind=0.9)
        for i in range(5):                                   # a ball of smoke hanging at the muzzle
            c = g.uniform(0.5, 0.6)
            fx.puff(t + 0.03, at + facing * 0.6, vel=facing * g.uniform(1.0, 3.0) + Vector((g.uniform(-0.8, 0.8), g.uniform(-0.8, 0.8), g.uniform(0.0, 0.8))),
                    life=g.uniform(4.0, 5.5), r=(0.3, g.uniform(0.9, 1.3)), col=(0.6, 0.52, 0.42), col2=(c, c, c * 0.97), alpha=0.5, drag=2.0,
                    rise=0.3, fade_in=0.03, grow=0.5, wind=0.9)
        for i in range(8):                                   # the plume along the line of fire
            fx.puff(t + 0.02, at + facing * 0.5, vel=facing * g.uniform(8.0, 26.0) + up * g.uniform(-1.0, 2.0) + side * g.uniform(-2.0, 2.0),
                    life=g.uniform(4.0, 5.5), r=(0.3, g.uniform(1.1, 1.7)), col=(0.62, 0.5, 0.36), col2=(0.56, 0.56, 0.54), alpha=0.55, drag=3.0,
                    rise=0.3, fade_in=0.03, grow=0.5, wind=0.9)
        flat = Vector((facing.x, facing.y, 0.0)).normalized()
        s2 = Vector((-flat.y, flat.x, 0.0))
        for i in range(18):
            u = g.uniform(0.5, 5.0)
            w = g.uniform(-1.0, 1.0) * (0.6 + 0.35 * u)
            p = at + flat * u + s2 * w
            z = ground(p)
            out = (flat + s2 * (w / max(u, 1.0))).normalized()
            fx.puff(t + 0.02 + 0.012 * u, Vector((p.x, p.y, z + 0.25)), vel=out * g.uniform(3.0, 6.0) + Vector((0, 0, g.uniform(0.6, 1.8))),
                    life=g.uniform(3.5, 5.0), r=(0.3, g.uniform(1.4, 2.3)), col=DUST, col2=DUST2, alpha=0.55, drag=1.8, rise=0.15, fade_in=0.05,
                    grow=0.5, wind=0.85, squash=(1.0, 1.0, 0.7))
        fx.light(t, at + facing * 1.2 + Vector((0, 0, 0.6)), 3.5e3, 0.1)
    fx.once(t0, where, make)


def tank_fire(fx, where, start=0.0, end=1e9):
    """A tank burns: low flame licking up off the engine deck, and thick black smoke that rises, spreads and leans away
    on the wind (the shared fire's look, sized for a tank)."""
    g = fx.rng

    def flame(t, at, facing):
        fx.puff(t, at + Vector((g.uniform(-0.35, 0.35), g.uniform(-0.3, 0.3), 0.1)), vel=(0, 0, g.uniform(1.5, 2.6)), life=g.uniform(0.35, 0.6),
                r=(g.uniform(0.3, 0.42), 0.12), col=(0.6, 0.24, 0.05), col2=(0.45, 0.12, 0.02), alpha=0.9, kind="glow", wind=0.35, fade_in=0.04,
                squash=(1.0, 1.0, 1.5), grow=0.7)

    def smoke(t, at, facing):
        c = g.uniform(0.03, 0.07)
        fx.puff(t, at + Vector((g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 0.6)), vel=(g.uniform(-0.4, 0.4), g.uniform(-0.4, 0.4), g.uniform(2.5, 3.8)),
                life=g.uniform(6.0, 8.0), r=(0.6, g.uniform(3.0, 4.2)), col=(c * 1.6, c * 1.2, c), col2=(c, c, c), alpha=0.8, drag=0.45, rise=1.1,
                fade_in=0.25, grow=0.6, wind=0.95, hold=0.3)
    fx.stream(where, 0.07, flame, start=start, end=end)
    fx.stream(where, 0.16, smoke, start=start, end=end)


def tank_sparks(fx, t0, at, n=14):
    """A shell strikes: sparks thrown out, falling."""
    g = fx.rng

    def make(t, p0, facing):
        for i in range(n):
            d = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-0.2, 1.0))).normalized()
            fx.puff(t, p0, vel=d * g.uniform(6.0, 13.0), life=g.uniform(0.25, 0.6), r=(0.05, 0.03), col=(1.0, 0.85, 0.5), col2=(1.0, 0.45, 0.1),
                    alpha=1.0, kind="glow", gravity=9.8, wind=0.0, fade_in=0.01, grow=1.0)
    fx.once(t0, (Vector(at), Vector((0, 0, 1))), make)


class Course:
    """The tank's way over the stage: along a straight line from `start` at `heading`, s(t) metres along it by time t
    from the speed v(t), standing on the ground and pitched and rolled to it."""

    def __init__(self, stage, start, heading, speed, long, dt=1.0 / 600.0):
        self.stage, self.start, self.long = stage, Vector(start), long
        a = math.radians(heading)
        self.yaw = a
        self.dir = Vector((math.cos(a), math.sin(a), 0.0))
        self.left = Vector((-self.dir.y, self.dir.x, 0.0))
        self.T = np.arange(0.0, long + 2 * dt, dt)
        self.V = np.array([speed(t) for t in self.T])
        self.S = np.concatenate([[0.0], np.cumsum((self.V[1:] + self.V[:-1]) * 0.5 * dt)])
        self.A = np.gradient(self.V, dt)
        self._m = {}

    def s(self, t):
        return float(np.interp(t, self.T, self.S))

    def v(self, t):
        return float(np.interp(t, self.T, self.V))

    def a(self, t):
        return float(np.interp(t, self.T, self.A))

    def point(self, s, lat=0.0):
        return self.start + self.dir * s + self.left * lat

    def matrix(self, t):
        """The carrier's world matrix at time t (the tank's rest frame placed on the ground)."""
        key = round(t * FPS * 4)
        if key in self._m:
            return self._m[key]
        s = self.s(t)
        h = lambda p: self.stage.height(p.x, p.y)
        p = self.point(s)
        hf, hr = h(p + self.dir * 1.9), h(p - self.dir * 1.9)
        hl, hrt = h(p + self.left * TRACK_Y), h(p - self.left * TRACK_Y)
        pitch = math.atan2(hf - hr, 3.8)
        roll = math.atan2(hl - hrt, 2.0 * TRACK_Y)
        z = (hf + hr + hl + hrt) * 0.25
        m = Matrix.Translation((p.x, p.y, z)) @ Matrix.Rotation(self.yaw, 4, "Z") @ Matrix.Rotation(-pitch, 4, "Y") @ Matrix.Rotation(roll, 4, "X")
        self._m[key] = m
        return m


def _spring(T, target, period=0.6, zeta=0.35):
    """A suspension's answer to a wanted angle over time: a damped spring, stepped along the grid T."""
    w = 2 * math.pi / period
    x, v = 0.0, 0.0
    out = np.zeros(len(T))
    for i in range(1, len(T)):
        dt = T[i] - T[i - 1]
        acc = w * w * (target[i] - x) - 2 * zeta * w * v
        v += acc * dt
        x += v * dt
        out[i] = x
    return out


def hero_speed(t):
    if t < T_BRAKE:
        return HERO_V
    if t < T_STOP:
        return HERO_V * (1.0 - (t - T_BRAKE) / (T_STOP - T_BRAKE))
    if t < T_GO:
        return 0.0
    return HERO_V2 * smoothstep(T_GO, T_RUN, t)


def hero_clip(course):
    """The hero clip as numbers: rolling by the course's metres; the hull pitching to the braking and the pulling away,
    bobbing while it drives, rocked by the shot along the gun's line; the turret laid onto the target and the gun up;
    the fire clip's recoil at T_FIRE; the aerial swaying and whipping."""
    rad = math.radians
    sin = lambda f, t, ph=0.0: math.sin(2 * math.pi * f * t + ph)
    c = Clip("hero", HERO_LONG, False, None, "drives in, stops, lays the gun, fires, drives off")
    c.rolling(course.s)
    T = course.T
    lean = _spring(T, np.array([rad(0.5) * a for a in course.A]))       # (+ nose up: braking dips it, pulling away lifts it)
    k = lambda t: course.v(t) / HERO_V
    fire = {x.name: x for x in clips()}["fire"]
    f_ch = {(p, kind, ax): fn for p, kind, ax, fn in fire.ch}
    rock = lambda t: f_ch[("hull", "turn", None)](t - T_FIRE) if t >= T_FIRE else 0.0
    th = rad(TURRET_TO)
    c.add("hull", "turn", lambda t: float(np.interp(t, T, lean)) + k(t) * (rad(0.4) * sin(0.7, t) + rad(0.12) * sin(2.1, t, 0.7) + rad(0.04) * sin(9, t))
          + rock(t) * math.cos(th))
    c.add("hull", "turn", lambda t: k(t) * (rad(0.3) * sin(0.6, t, 1.9) + rad(0.1) * sin(1.7, t)) + rock(t) * math.sin(th), axis=(1, 0, 0))
    c.add("hull", "slide", lambda t: k(t) * (0.008 * sin(1.3, t, 0.4) + 0.0015 * sin(8, t))
          + (f_ch[("hull", "slide", (0, 0, 1))](t - T_FIRE) if t >= T_FIRE else 0.0), axis=(0, 0, 1))
    c.add("turret", "turn", lambda t: _ease(t, 1.2, 3.6, 0.0, rad(TURRET_TO)))
    c.add("gun", "turn", lambda t: _ease(t, 2.9, 3.8, 0.0, rad(GUN_TO)) + (f_ch[("gun", "turn", None)](t - T_FIRE) if t >= T_FIRE else 0.0))
    c.add("barrel", "slide", lambda t: f_ch[("barrel", "slide", None)](t - T_FIRE) if t >= T_FIRE else 0.0)
    for i in range(3):
        kk = i + 1
        c.add(f"aerial_{kk}", "turn", lambda t, kk=kk: k(t) * (rad(-1.2 * kk) + rad(1.0 * kk) * sin(1.0, t, -0.7 * kk))
              + _damped(t, rad(4.0 * kk), 0.5, 0.4, T_BRAKE + 0.15) + _damped(t, rad(-3.0 * kk), 0.55, 0.45, T_FIRE)
              - _damped(t, rad(3.0 * kk), 0.5, 0.4, T_GO + 0.1))
        c.add(f"aerial_{kk}", "turn", lambda t, kk=kk: k(t) * rad(0.5 * kk) * sin(0.6, t, 1.0 - 0.6 * kk)
              + _damped(t, rad(1.5 * kk), 0.6, 0.4, T_FIRE), axis=(1, 0, 0))
    return c


def ko_clip(course, hold=6.0):
    """The knocked_out clip with the turret where the hero left it (laid onto the target), held to `hold` seconds."""
    rad = math.radians
    ko = {x.name: x for x in clips()}["knocked_out"]
    c = Clip("ko_stage", hold, False, None, "hit and knocked out, burning")
    c.rolling(lambda t: 0.0)
    end = ko.long
    for part, kind, axis, fn in ko.ch:
        if kind == "scroll":
            continue
        g = (lambda t, fn=fn: fn(min(t, end)))
        if part == "turret":
            g = (lambda t, fn=fn: rad(TURRET_TO) + fn(min(t, end)))
        if part == "gun":
            g = (lambda t, fn=fn: rad(GUN_TO) + fn(min(t, end)))
        c.add(part, kind, g, axis)
    if not any(p == "turret" for p, *_ in c.ch):
        c.add("turret", "turn", lambda t: rad(TURRET_TO))
    return c


def chain_matrix(clip, by_name, name, t):
    """A part's movement at time t, its parents' included, in the tank's rest frame."""
    chain, p = [], by_name[name]
    while p is not None:
        chain.append(p)
        p = by_name.get(p["parent"]) if p["parent"] else None
    M = Matrix.Identity(4)
    for p in reversed(chain):
        ch = [(k, ax, fn) for part, k, ax, fn in clip.ch if part == p["name"] and k != "scroll"]
        if ch:
            M = M @ bone_delta(p, ch, t)
    return M


def track_marks(world, course, front_of):
    """The belts' prints pressed into the ground along the course (from well before the clip's start), shown only
    where the tank has already been: a grouser's print every link."""
    import battle_set
    m = bpy.data.materials.new("track_marks")
    nt, bsdf = _nodes(m)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["UV"], sep.inputs[0])
    ph = _math(nt, "FRACT", _math(nt, "DIVIDE", sep.outputs[0], PATH.pitch))
    groove = _range(nt, _math(nt, "ABSOLUTE", _math(nt, "SUBTRACT", ph, 0.5)), 0.28, 0.4)
    edge = _range(nt, _math(nt, "ABSOLUTE", sep.outputs[1]), 0.17, 0.21)       # (soft at the sides)
    nt.links.new(_mix(nt, kit.hex_rgb("6A5E46"), kit.hex_rgb("857659"), groove), bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.95
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.7
    nt.links.new(groove, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    front = nt.nodes.new("ShaderNodeValue")
    front.name = "front"
    shown = _math(nt, "LESS_THAN", sep.outputs[0], front.outputs[0])
    alpha = _math(nt, "MULTIPLY", shown, _math(nt, "SUBTRACT", 1.0, edge))
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(_math(nt, "MULTIPLY", alpha, 0.6), mix.inputs["Fac"])
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(bsdf.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    battle_set.haze(m)
    sc = bpy.context.scene
    for f in range(int(sc.frame_start), int(sc.frame_end) + 1):
        front.outputs[0].default_value = front_of(f / FPS)
        front.outputs[0].keyframe_insert("default_value", frame=f)
    s1 = course.s(course.long) + 3.0
    for side in (1.0, -1.0):
        bm = bmesh.new()
        uv = bm.loops.layers.uv.new("UVMap")
        rows = []
        for s in np.arange(-40.0, s1, 0.25):
            row = []
            for d in (-0.22, 0.22):
                p = course.point(s, side * TRACK_Y + d)
                row.append((bm.verts.new((p.x, p.y, world.height(p.x, p.y) + 0.012)), (s, d)))
            rows.append(row)
        for a, b in zip(rows, rows[1:]):
            f = bm.faces.new((a[0][0], b[0][0], b[1][0], a[1][0]))
            for loop, (_, st) in zip(f.loops, (a[0], b[0], b[1], a[1])):
                loop[uv].uv = st
        me = bpy.data.meshes.new("marks")
        bm.to_mesh(me)
        bm.free()
        me.materials.append(m)
        ob = bpy.data.objects.new("track_marks", me)
        sc.collection.objects.link(ob)
        ob.visible_shadow = False


def clear_corridor(course, half=1.8, s0=-45.0, s1=40.0):
    """The tank flattens the grass in its way: drop the tufts along the course."""
    ob = bpy.data.objects.get("grass")
    if ob is None:
        return
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    gone = []
    for f in bm.faces:
        c = f.calc_center_median() - course.start
        s, lat = c.dot(course.dir), c.dot(course.left)
        if s0 < s < s1 and abs(lat) < half:
            gone.append(f)
    bmesh.ops.delete(bm, geom=gone, context="FACES")
    bm.to_mesh(ob.data)
    bm.free()


class BattleScene:
    """The tank on the battle stage, driven along a course by a carrier, with its belts, its markings and its wear."""

    def __init__(self, clip_of, course_args, cam_at, cam_height, lens):
        import battle_set
        self.st = Stage(ground=False)
        st = self.st
        # September light from the camera's right (the sun low in the south)
        st.sun.rotation_euler = Euler((math.radians(90 - 33), 0.0, math.radians(-92)), "XYZ")
        t0 = time.time()
        self.world = battle_set.build()
        print(f"battle stage: {time.time() - t0:.0f} s")
        self.course = Course(self.world, *course_args)
        self.clip = clip_of(self.course)
        acts = make_actions(st.arm, st.parts, [self.clip])
        play(st.arm, acts[self.clip.name], st.sc)
        st.sc.frame_end = int(round(self.clip.long * FPS))
        st.belts.follow(self.clip)
        self.carrier = bpy.data.objects.new("carrier", None)
        st.sc.collection.objects.link(self.carrier)
        st.arm.parent = self.carrier
        for ob, _ in st.belts.objs:
            ob.parent = st.arm
        for f in range(0, st.sc.frame_end + 1):
            self.carrier.matrix_world = self.course.matrix(f / FPS)
            self.carrier.keyframe_insert("location", frame=f)
            self.carrier.keyframe_insert("rotation_euler", frame=f)
        kit._linear(self.carrier)
        clear_corridor(self.course)
        self.by_name = {p["name"]: p for p in st.parts}
        self.fx = Effects(seed=7, wind=WIND)
        self.now = 0.0
        self.cam_at, self.cam_height = Vector(cam_at), cam_height
        st.cam.data.lens = lens
        table = sockets_table()
        self.sock_rest = {name: (Vector(at), Vector(facing).normalized()) for name, bone, at, facing in table}
        self.sock_part = {name: bone for name, bone, at, facing in table}
        st.sc.frame_set(0)

    def socket(self, name, t):
        """A socket's world place and facing at time t."""
        at, facing = self.sock_rest[name]
        part = self.sock_part[name]
        M = self.course.matrix(t) @ chain_matrix(self.clip, self.by_name, part, t)
        return M @ at, (M.to_3x3() @ facing).normalized()

    def ground(self, p):
        return self.world.height(p.x, p.y)

    def aim_camera(self, look_of):
        cam = self.st.cam
        for f in range(0, self.st.sc.frame_end + 1):
            t = f / FPS
            eye = Vector((self.cam_at.x, self.cam_at.y, self.ground(self.cam_at) + self.cam_height))
            kit.look_at(cam, eye, look_of(t))
            cam.keyframe_insert("location", frame=f)
            cam.keyframe_insert("rotation_euler", frame=f)
        kit._linear(cam)

    def where(self, name):
        """A socket as the effects want it: a function giving its (place, facing) at the scene's present time."""
        return lambda: self.socket(name, self.now)

    def dust_trail(self, t0, t1, every=1.0 / 5.0):
        """Tan dust thrown up behind each belt while the tank moves, thicker the faster it goes."""
        fx, g = self.fx, self.fx.rng

        def make(t, at, facing):
            v = self.course.v(t)
            if v < 0.3:
                return
            k = min(1.0, v / HERO_V)
            z = self.ground(at)
            fx.puff(t, Vector((at.x + g.uniform(-0.2, 0.2), at.y + g.uniform(-0.2, 0.2), z + 0.35)),
                    vel=facing * (0.7 * v) + Vector((g.uniform(-0.4, 0.4), g.uniform(-0.4, 0.4), 0.5)), life=g.uniform(3.0, 4.0),
                    r=(0.6, g.uniform(2.0, 2.8)), col=DUST, col2=DUST2, alpha=0.42 * k, drag=1.5, rise=0.18, fade_in=0.12, grow=0.5, wind=0.8,
                    squash=(1.0, 1.0, 0.7))
        for side in ("l", "r"):
            fx.stream(self.where(f"socket_dust_{side}"), every, make, start=t0, end=t1)

    def exhaust(self, t0, t1, every=0.15, burst=None):
        """A faint haze from the silencer's two pipes; a darker puff when the engine is opened up."""
        fx, g = self.fx, self.fx.rng

        def make(t, at, facing):
            b = 2.2 if burst and burst[0] <= t < burst[1] else 1.0
            c = 0.42 if b == 1.0 else 0.24
            fx.puff(t, at, vel=facing * 1.5 + Vector((g.uniform(-0.2, 0.2), g.uniform(-0.2, 0.2), 0.0)), life=g.uniform(1.4, 2.0),
                    r=(0.08, g.uniform(0.5, 0.8) * (1.0 + 0.25 * (b - 1.0))), col=(c, c * 1.02, c * 1.08), col2=(0.55, 0.57, 0.6), alpha=0.18 * b,
                    drag=2.5, rise=0.5, fade_in=0.05, grow=0.5, wind=1.0)
        for side in ("l", "r"):
            fx.stream(self.where(f"socket_exhaust_{side}"), every, make, start=t0, end=t1)

    def render(self, out_name, w, h, samples, frames=None):
        """Every frame in order (the effects remember what they laid), rendered, or only `frames` for a test."""
        st = self.st
        render_cfg(w, h, samples)
        st.sc.cycles.transparent_max_bounces = 96
        st.sc.cycles.diffuse_bounces = 1
        st.sc.cycles.glossy_bounces = 1
        folder = os.path.join(WORK, "clip_" + out_name)
        done = set()
        if frames is None and os.path.isdir(folder):
            # (resuming after an interruption: keep the frames already made at this size, whole; the scene is still
            # stepped through them in order, since the smoke remembers)
            from PIL import Image
            for fn in sorted(os.listdir(folder)):
                path = os.path.join(folder, fn)
                try:
                    with Image.open(path) as im:
                        im.load()
                        good = fn[:4].isdigit() and im.size == (st.sc.render.resolution_x, st.sc.render.resolution_y)
                except Exception:
                    good = False
                if good:
                    done.add(fn)
                else:
                    os.remove(path)
            print(f"resuming: {len(done)} frames already made", flush=True)
        else:
            shutil.rmtree(folder, ignore_errors=True)
        os.makedirs(folder, exist_ok=True)
        n = 1
        for f in range(0, st.sc.frame_end + 1):
            self.now = f / FPS
            st.sc.frame_set(f)
            self.fx.at(self.now)
            if frames is not None and f not in frames:
                continue
            name = f"{n:04d}.png" if frames is None else f"f{f:04d}.png"
            n += 1
            if name in done:
                continue
            t = time.time()
            kit.still(os.path.join(folder, name))
            print(f"frame {f}: {time.time() - t:.1f} s", flush=True)
        if frames is None:
            kit.encode(folder, pic_path(out_name), fps=FPS)
        return folder


def hero(w=None, h=None, samples=None, frames=None):
    """Brian's hero clip: the tank drives in across the stubble, the turret lays onto the wood's edge as it brakes to a
    stop, it fires (flash, a ring of grey smoke, dust blown up off the ground, the hull rocking), then drives off."""
    t0 = time.time()
    b = BattleScene(hero_clip, (HERO_START, HERO_HEADING, hero_speed, HERO_LONG), HERO_CAM, 5.0, 38.0)
    c = b.course

    def look(t):
        s = c.s(t) * 0.85 + c.s(min(t + 0.6, HERO_LONG)) * 0.15         # (a little ahead of the tank)
        p = c.point(s)
        return Vector((p.x, p.y, b.ground(p) + 1.3))
    b.aim_camera(look)
    track_marks(b.world, c, lambda t: c.s(t) + WHEEL_XS[0] + 0.25)
    b.dust_trail(0.0, HERO_LONG)
    b.exhaust(0.0, HERO_LONG, burst=(T_GO - 0.1, T_GO + 0.5))
    tank_gun(b.fx, T_FIRE, b.where("socket_muzzle"), b.ground)
    folder = b.render("hero", w, h, samples, frames)
    print(f"hero: {time.time() - t0:.0f} s")
    return folder


def ko_stage(w=None, h=None, samples=None, frames=None, hold=6.0):
    """The knocked_out clip on the stage, the tank stopped where the hero clip had it fire: the hit's sparks and flash,
    the hatch blown with a gout of flame, then fire on the engine deck and a black column of smoke leaning away on the
    wind (the shared effects' fire, sized for a tank)."""
    t0 = time.time()
    probe = Course(None, HERO_START, HERO_HEADING, hero_speed, HERO_LONG)
    start = probe.point(probe.s(T_FIRE))
    b = BattleScene(lambda course: ko_clip(course, hold), (start, HERO_HEADING, lambda t: 0.0, hold), HERO_CAM + Vector((-2.0, -1.0, 0.0)), 3.0, 35.0)
    p = b.course.point(0.0)
    look = Vector((p.x, p.y, b.ground(p) + 3.6))
    b.aim_camera(lambda t: look)
    track_marks(b.world, b.course, lambda t: 2.3)
    fx, g = b.fx, b.fx.rng
    M = b.course.matrix(0.0)
    hit = M @ Vector((1.25, -1.0, 1.55))
    tank_sparks(fx, 0.0, hit)
    fx.burst(0.0, (hit, Vector((0, 0, 1))), size=0.12)

    def gout(t, at, facing):                                 # the hatch blown: flame out of it, then smoke from it
        for i in range(4):
            fx.puff(t + 0.02 * i, at + Vector((0, 0, 0.2 + 0.3 * i)), vel=(g.uniform(-0.5, 0.5), g.uniform(-0.5, 0.5), g.uniform(5.0, 8.0)),
                    life=g.uniform(0.45, 0.7), r=(0.25, 0.7), col=(0.7, 0.3, 0.06), col2=(0.5, 0.15, 0.03), alpha=0.95, kind="glow", drag=3.0,
                    wind=0.3, fade_in=0.02, squash=(1.0, 1.0, 1.5))
        fx.light(t, at + Vector((0, 0, 0.8)), 6.0e3, 0.25)
    fx.once(0.4, b.where("socket_commander"), gout)

    def hatch_smoke(t, at, facing):
        c = g.uniform(0.05, 0.1)
        fx.puff(t, at + Vector((0, 0, 0.15)), vel=(g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), g.uniform(1.5, 2.5)), life=g.uniform(5.0, 7.0),
                r=(0.3, g.uniform(1.4, 2.0)), col=(c * 1.5, c * 1.2, c), col2=(c, c, c), alpha=0.6, drag=0.6, rise=1.2, fade_in=0.2, grow=0.6, wind=0.95)
    fx.stream(b.where("socket_commander"), 0.25, hatch_smoke, start=0.6)
    for y in (-0.52, 0.52):                                  # the engine deck burns over both grilles
        deck = (M @ Vector((-1.62, y, ROOF + 0.1)), Vector((0, 0, 1)))
        tank_fire(fx, deck, start=1.0 + (0.15 if y > 0 else 0.0))
    lamp = bpy.data.lights.new("deck_fire", "POINT")
    lamp.color = (1.0, 0.5, 0.2)
    lamp.shadow_soft_size = 0.6
    lo = bpy.data.objects.new("deck_fire", lamp)
    b.st.sc.collection.objects.link(lo)
    lo.location = M @ Vector((-1.62, 0.0, ROOF + 0.7))
    for f in range(0, b.st.sc.frame_end + 1):
        tt = f / FPS
        lamp.energy = 0.0 if tt < 1.0 else 2500.0 * smoothstep(1.0, 1.8, tt) * (0.75 + 0.25 * math.sin(f * 1.7) * math.sin(f * 0.6))
        lamp.keyframe_insert("energy", frame=f)
    folder = b.render("knocked_out_stage", w, h, samples, frames)
    print(f"ko_stage: {time.time() - t0:.0f} s")
    return folder


# ------------------------------------------------------------------------------------------------ delivery
# /mnt/project-files is a network mount: each file is made locally, copied there once, and read back later to check.
SENT = []


def stage_path(*names):
    """Where a deliverable is made before it is copied out."""
    p = os.path.join(WORK, "out", *names)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    return p


def deliver(local, rel):
    dest = os.path.join(OUT, rel)
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
            # (the shared folder re-saves pictures: the same pixels in other bytes count as landed)
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


def pic_path(*names):                                       # (pictures are made here, then delivered)
    return stage_path("pictures", *names)


def deliver_pictures(*names):
    for n in names:
        deliver(pic_path(n), os.path.join("pictures", n))


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
    (its bone, the strongest where it is shared) and its UV (U along the belt; a box projection elsewhere)."""
    me = obj.data
    try:
        me.calc_loop_triangles()
    except Exception:
        pass
    normals = [cn.vector.copy() for cn in me.corner_normals]
    uvd = me.uv_layers["UVMap"].data
    gname = {vg.index: vg.name for vg in obj.vertex_groups}
    vpart = []
    for v in me.vertices:
        best = max(v.groups, key=lambda e: e.weight) if len(v.groups) else None
        vpart.append(index[gname[best.group]] if best else 0)
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


def write_json(path, meshes, parts, clip_list):
    index = {p["name"]: i for i, p in enumerate(parts)}
    near, mid, far = (mesh_json(meshes[k], index) for k in (0, 1, 2))
    zs, xs = near["points"][2::3], near["points"][0::3]
    data = {
        "name": "Battle medium tank (our own design)",
        "about": "Made by Tools/battle_units/tank.py from build_tank.py's MediumTank. Metres; x forward, y right, z up. Paints: "
                 + ", ".join(f"{v} {k}" for k, v in bt.PAINTS.items())
                 + ". near = LOD0, mid = LOD1, far = LOD2. Each point has a part (index into parts) and a uv. Turns are in radians, "
                   "about the part's axis through its pivot, positive by v' = v cos a + (axis x v) sin a + axis (axis.v)(1 - cos a) "
                   "computed in these axes (as Unreal's FQuat(axis, angle)); each part's 'sense' says what + does. A child turns with "
                   "its parent. A track may carry its own axis (the hull rolls and bobs as well as pitching). Angles here are in "
                   "RADIANS (the sea and air files use degrees): see units.",
        "units": {"length": "metres", "angle": "radians", "time": "seconds"},
        "long": round(max(xs) - min(xs), 3),
        "high": round(max(zs) - min(zs), 3),
        "marks": bt.marks_of(S),
        "near": near,
        "mid": mid,
        "far": far,
        "parts": [],
        "sockets": [],
        "clips": clips_json(clip_list, index),
        "track": {
            "metres_per_u": round(PATH.length, 4),
            "links": PATH.links,
            "pitch": round(PATH.pitch, 5),
            "metres_per_drive_loop": round(PATH.drive_metres, 4),
            "drive_loop_seconds": DRIVE_LOOP,
            "scroll": "U runs once round each belt (0 to 1), forward along the bottom; one link to every 1/links of U. Sample the belt's "
                      "texture at U + d / metres_per_u, d the metres the tank has gone forward (less when reversing). The clips' 'scroll' "
                      "keys give d / metres_per_u.",
            "path": [[round(float(x), 4), round(float(z), 4)] for x, z in zip(*PATH.at(np.arange(PATH.links) * PATH.pitch))],
            "path_about": "the inside of the belt at each link's pin, seen from the side (x forward, z up), at y = +-track_out; for laying "
                          "link instances along it if the game wants links that travel up close",
            "track_out": TRACK_Y, "width": TRACK_W,
        },
        "controls": [
            {"part": index["turret"], "part_name": "turret", "what": "traverse, turned by the game", "limits": None,
             "loop_cue": None, "fx": "turret_traverse", "note": "no recording yet"},
            {"part": index["gun"], "part_name": "gun", "what": "elevation, turned by the game", "limits": [round(math.radians(-8), 4), round(math.radians(20), 4)]},
            {"what": "belts", "parts": [index["track_l"], index["track_r"]], "scroll_u_per_metre": round(1.0 / PATH.length, 6)},
            {"what": "drive play rate", "clip": "drive", "rate": "speed (m/s) * 2.0 / " + f"{PATH.drive_metres:.4f}"},
        ],
    }
    for p in parts:
        e = {"name": p["name"], "parent": index[p["parent"]] if p["parent"] else None, "parent_name": p["parent"],
             "pivot": J(p["pivot"]), "axis": [round(a, 4) for a in p["axis"]], "kind": p["kind"], "limits": p["limits"], "sense": p["sense"]}
        for k in ("radius", "metres_per_u"):
            if k in p:
                e[k] = p[k]
        data["parts"].append(e)
    for name, bone, at, facing in sockets_table():
        data["sockets"].append({"name": name, "part": index[bone], "part_name": bone, "at": J(at), "facing": J(facing.normalized())})
    with open(path, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    return data


def export_all():
    """tank.glb (LOD0, all clips), tank_LOD1.glb, tank_LOD2.glb, tank.fbx (LOD0, all clips as takes), tank_LOD1.fbx,
    tank_LOD2.fbx, and tank.json in the game's own form."""
    t0 = time.time()
    sc = kit.fresh(FPS)
    parts = parts_table()
    arm = make_rig(parts)
    clip_list = clips()
    make_actions(arm, parts, clip_list)
    socks = make_sockets(arm)
    meshes, tris = {}, {}
    for lod in (0, 1, 2):
        g = build_geo(lod)
        tris[lod] = g.tris()
        meshes[lod] = make_mesh(g, f"tank_LOD{lod}", arm)
    reset_pose(arm)
    arm.animation_data.action = None
    sc.frame_set(0)
    for lod in (0, 1, 2):
        name = "tank" if lod == 0 else f"tank_LOD{lod}"
        _glb(stage_path(name + ".glb"), [arm, meshes[lod]] + socks, lod == 0)
        _fbx(stage_path(name + ".fbx"), [arm, meshes[lod]] + socks, lod == 0)
    data = write_json(stage_path("tank.json"), meshes, parts, clip_list)
    for n in ("tank.glb", "tank_LOD1.glb", "tank_LOD2.glb", "tank.fbx", "tank_LOD1.fbx", "tank_LOD2.fbx", "tank.json"):
        deliver(stage_path(n), n)
    info = {"tris": tris, "json_tris": {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")},
            "long": data["long"], "high": data["high"], "seconds": round(time.time() - t0, 1)}
    with open(stage_path("export_info.json"), "w") as f:
        json.dump(info, f)
    print("exports:", info)
    return info



# ------------------------------------------------------------------------------------------------ the notes
def write_readme():
    """README.md beside the files: what was built, the counts, the rig, the sockets, the clips, and what the game does."""
    info_p = stage_path("export_info.json")
    if os.path.exists(info_p):
        with open(info_p) as f:
            info = json.load(f)
        tris = {int(k): v for k, v in info["tris"].items()}
    else:
        kit.fresh(FPS)
        tris = {lod: build_geo(lod).tris() for lod in (0, 1, 2)}
        info = {"long": None, "high": None}
    parts = parts_table()
    clip_list = clips()
    D, L = PATH.drive_metres, PATH.length
    deg = lambda r: f"{math.degrees(r):.0f}"
    L_ = []
    w = L_.append
    w("# The battle tank\n")
    w("Our own generic medium tank (build_tank.py's `MediumTank`, the campaign piece) built up for the battles' close camera, "
      "rigged and animated. Made entirely by `Tools/battle_units/tank.py` (run it with the `bpy` module, Blender 5.0.1: "
      "`python tank.py`; `--only export` remakes just the files). It is not a copy of any real vehicle or any game's model.\n")
    w("## What was built\n")
    w("- **The campaign piece's own plates**: `build_tank.build()` is called with SPEC + VARIANTS[\"MediumTank\"] and its hull, waist, "
      "nose, six-sided turret, mantlet, grilles and louvres are kept as they are (bevelled at LOD0), so the battle tank and the "
      "campaign piece are the same tank in the same measures (6.25 m long over the guards, 2.48 m to the turret roof).")
    w("- **Added for the close camera**: dual road wheels on rubber tyres with dished discs, hub caps and bolt circles; a 13-tooth "
      "sprocket (two rings) with a bolted hub and the final-drive housing bolted to the hull; a dual idler with a steel rim, "
      "lightening holes and a track-adjusting crank; return rollers; swing arms and bump stops; track links with a grouser, a "
      "guide horn and a pin end, one link made and laid round the track's path; weld seams along the plate joints and the "
      "turret's corners; the driver's flap (bolted, hinged, with its visor and two periscopes); the commander's round hatch "
      "(bolt ring, handle, hinge) and the loader's square hatch; turret periscopes and side vision blocks; a mushroom vent; "
      "a bow machine-gun in a ball mount; headlamps with wire guards; front and rear mudguards with rubber flaps; four "
      "spare links on the nose under a bar; tow shackles front and rear; the silencer with a heat shield and two pipes; a bolted, "
      "hinged engine door in the tail plate; an engine-deck access plate, fuel fillers and lifting eyes; a shovel, an axe and a crowbar in clamps on the right "
      "sponson; the tow cable with its eyes on the left; a stowage bin on the turret's back with a rolled tarpaulin; "
      "three jerrycans upright in a rack, an ammunition crate with rope handles and a bucket on the back of the engine deck "
      "(outside the circle the turret's bin sweeps, so the turret turns clear); a whip aerial in three bones. The sides are kept clear where the game paints its number (x -1.4 to -0.5 m, z 1.3 to 1.56 m).\n")
    w("## Triangles\n")
    w("| LOD | triangles | file | meant for |")
    w("|---|---|---|---|")
    w(f"| LOD0 | {tris[0]:,} | tank.glb, tank.fbx, tank.json `near` | close camera |")
    w(f"| LOD1 | {tris[1]:,} | tank_LOD1.glb, tank_LOD1.fbx, tank.json `mid` | middle distance (wheels plain, belts a band, no bolts or welds) |")
    w(f"| LOD2 | {tris[2]:,} | tank_LOD2.glb, tank_LOD2.fbx, tank.json `far` | far (boxes, eight-sided wheels, a coarse band) |")
    w("\nAll three share one skeleton and one set of material names. LOD0's belts carry only the links that can be seen: those "
      "running inside the sponsons are left out.\n")
    w("## Materials (one per paint)\n")
    w("`paint, shade, track, rubber, wood, steel, canvas, glass, rust` (the names and numbers of build_tank.PAINTS; `stripe` is not "
      "used). The game colours `paint` and `shade` with the nation's colours (unit table). `track` is the belts alone (the "
      "spare links are `rust`, so a panner on `track` moves only the belts). The exports carry plain colours (German 4E5459); the "
      "pictures' wear (mottling, dust low down, rubbed edges) is a studio material only and is not exported.\n")
    w("## Axes and units\n")
    w("Metres; +X forward, Z up, the ground at z = 0, the tank's middle at x = y = 0. In Blender and the .glb/.fbx the y axis "
      "runs to the tank's LEFT; in tank.json y runs to the RIGHT (as build_tank.py writes it). The parts' pivots below are in "
      "the JSON's axes (y right). Angles in radians. A turn of +a about an axis is `v' = v cos a + (axis x v) sin a + "
      "axis (axis . v)(1 - cos a)` computed in the JSON's components, the same as Unreal's `FQuat(Axis, Angle)` in Unreal's own "
      "axes (x forward, y right, z up); the *sense* column says what + does.\n")
    w("## Bones (parts)\n")
    w("| # | bone | parent | pivot (x, y right, z) | axis | kind | limits | sense |")
    w("|---|---|---|---|---|---|---|---|")
    for i, p in enumerate(parts):
        lim = "free" if p["limits"] is None else ", ".join(f"{v:g}" for v in p["limits"])
        if p["kind"] == "turn" and p["limits"]:
            lim += f" ({deg(p['limits'][0])} to {deg(p['limits'][1])} deg)"
        if p["kind"] == "scroll":
            lim = f"{p['metres_per_u']:.4f} m per U"
        piv = ", ".join(f"{v + 0.0:g}" for v in J(p["pivot"]))           # (+ 0.0: no "-0")
        ax = ", ".join(f"{v + 0.0:g}" for v in p["axis"])
        w(f"| {i} | `{p['name']}` | {p['parent'] or '-'} | {piv} | {ax} | {p['kind']} | {lim} | {p['sense']} |")
    w("\nThe road wheels stay on the ground with the root; the sprocket, idler and return rollers ride on the hull, and the "
      "belts are skinned partly to the hull near them so they follow its rocking. Every point of the meshes belongs to one bone "
      "(weight 1), except the belts (track_l/r with hull) and the aerial (blended along its three bones so it bends).\n")
    w("## Sockets\n")
    w("Empties named `socket_<name>`, parented to bones; each one's +X axis points along its facing.\n")
    w("| socket | bone | at (x, y right, z) | facing |")
    w("|---|---|---|---|")
    for name, bone, at, facing in sockets_table():
        w(f"| `{name}` | {bone} | {', '.join(f'{v + 0.0:g}' for v in J(at))} | "
          f"{', '.join(f'{v + 0.0:g}' for v in J(facing.normalized()))} |")
    w("\n## Clips\n")
    w("Every clip that does not loop carries `\"after\": \"hold\"` (hold the last pose), as the sea and air files do. "
      "Unlike them, this file's angles are in radians (`units` in tank.json); the turret's + is clockwise from above "
      "as theirs is, and the barrel's recoil slide is a child of the gun's elevation, so it runs back along the lifted bore.\n")
    w("In place (no root motion), 30 frames a second, one action per clip (glb: separate animations; fbx: separate takes; "
      "json: `clips`). Loops end on the pose they start on. Event times are in seconds from the clip's start.\n")
    w("| clip | seconds | loops | what | loop cue | events (t: fx @ socket, cue) |")
    w("|---|---|---|---|---|---|")
    for c in clip_list:
        ev = "; ".join(f"{e['t']:g}: {e['fx']} @ {e['socket'] or '(where the shell struck)'}" + (f", **{e['cue']}**" if e['cue'] else "")
                       for e in c.events)
        if c.name == "mg_burst":
            ev = "0.0: muzzle_flash @ socket_coax, **tank_mg_burst**; then a muzzle_flash every 0.075 s to 0.675 (ten rounds, cue null)"
        w(f"| `{c.name}` | {c.long:g} | {'yes' if c.loop else 'no'} | {c.about} | {c.loop_cue or '-'} | {ev} |")
    w("\n- **fire**: the barrel slides back 0.30 m from 0.0 to 0.12, runs out by 0.6; the hull rocks back (nose up 1.5 degrees "
      "at most) and is still by 1.5; the gun kicks up 0.6 degrees and settles.")
    w("- **stop**: the hull pitches forward (1.8 degrees, most at 0.1) and settles by 0.8; the wheels and belts come to rest "
      "by 0.12.")
    w("- **knocked_out**: hit at 0.0; the hull jolts (pitch and roll), the turret is jarred round 3 degrees (to the right), the "
      "commander's hatch is blown open at 0.4 (bangs against its stop and settles at 115 degrees), the gun droops to -7 "
      "degrees between 0.6 and 2.2, the aerial whips and is still by 3.0. Hold the last frame.")
    w("- The sound director's cues: `tank_idle_loop` and `tank_loop` (idle and drive, clip-level loop cues), `tank_gun_01` "
      "(fire, 0.0), `tank_shell_case` (fire, the spent case at 1.2), `tank_mg_burst` (mg_burst, 0.0), `tank_knocked_out` "
      "(knocked_out, 0.0) and `fire_loop` (knocked_out, burning from 1.0 at socket_engine: a loop the game keeps playing "
      "while the wreck burns). Still null: the brakes (stop) and the turret's traverse loop (not a clip: see below). "
      "`tank_start` is the `start` clip's (0.0, at the exhaust).\n")
    w("## Driving: metres per loop, and the belts\n")
    w(f"- **One `drive` loop covers {D:.4f} m in {DRIVE_LOOP:g} s** ({D / DRIVE_LOOP:.2f} m/s as made). Play rate = speed (m/s) x "
      f"{DRIVE_LOOP:g} / {D:.4f}. At that rate the road wheels and sprocket turn at the speed of the ground, so nothing slides.")
    w(f"- The belt is {L:.4f} m round: **{L:.4f} metres per U** (U runs 0 to 1 once round, forward along the bottom; V across). "
      f"It has {PATH.links} links of {PATH.pitch:.4f} m, one link to every 1/{PATH.links} of U. The game scrolls the belt's material: "
      f"sample its texture at (U + d / {L:.4f}, V), d = metres travelled forward; that is {1.0 / L:.5f} U per metre. "
      "The drive and stop clips also carry this as `scroll` keys in tank.json.")
    w(f"- A drive loop moves the belt exactly {PATH.drive_links} links, turns the sprocket exactly {PATH.drive_links} teeth and each "
      "wheel a whole number of its bolt spacings, so the loop has no seam.")
    w("- LOD0's belt is modelled link by link (grousers, horns) but does not move as geometry: a texture scrolled on it gives the "
      "movement (a link texture tiled once per link). For links that visibly travel up close (as in our clips), `track.path` in "
      "tank.json gives the belt's path at every link's pin (x, z at y = +-track_out), to lay link instances along with an offset "
      "of d mod pitch.\n")
    w("## What the game does itself\n")
    w("1. **Turret traverse**: turn bone `turret` about its pivot, axis z, + to the right, free all round. Apply it after the clip "
      "(add to the clip's own value: knocked_out jars the turret 3 degrees). Its sound loop has no recording yet (cue null).")
    w("2. **Gun elevation**: turn bone `gun` about the trunnions, + muzzle up, limits -8 to +20 degrees; again added after the "
      "clip (fire kicks it, knocked_out droops it).")
    w("3. **Belts**: scroll the `track` material's U from the ground speed (above), each side by its own speed when turning.")
    w("4. **Play rate**: set the drive clip's play rate from the speed (above); blend drive -> stop -> idle.")
    w("5. **Effects and sounds**: from the clip events at the sockets (muzzle flashes, dust, exhaust, the hatch fire).")
    w("6. **Hatches**: `hatch_commander` opened (to about 1.6 rad) when a commander figure stands at `socket_commander`.\n")
    w("## How to import\n")
    w("- **tank.glb** (glTF 2.0, LOD0, skinned, six animations, sockets as child nodes of their bones): Unreal 5's Interchange glTF "
      "importer, as a Skeletal Mesh with animations. Convert to Unreal axes on import as usual (glTF is y-up; the importer does it).")
    w("- **tank.fbx** (LOD0, skinned, six takes `tank_rig|tank_rig|<clip>`): Import as Skeletal Mesh, Import Animations on, "
      "*Use T0 As Reference Pose* off, import scale 1.0: the file carries its units (written with FBX_SCALE_UNITS), so 1 m arrives "
      "as 100 Unreal units; X forward, Z up, bones' primary axis Y. The socket "
      "empties arrive as small extra bones named `socket_*`: add Unreal sockets of the same names on them (or attach effects "
      "to those bones directly).")
    w("- **LODs**: open the imported Skeletal Mesh, LOD Settings, *Import LOD* level 1 from tank_LOD1.fbx (or .glb) and level 2 "
      "from tank_LOD2; they share the skeleton. Suggested screen sizes 1.0 / 0.35 / 0.12.")
    w("- **tank.json**: the game's own reader loads `near` and `far` as today (the added keys `mid`, `part`, `uv`, `parts`, "
      "`sockets`, `clips`, `track`, `controls` are new and ignored by the old reader).\n")
    w("## The pictures\n")
    w("- `pictures/before-after.png`: today's MediumTank (its JSON, painted by number) beside the battle tank, same camera, same sun, "
      "German paint.")
    w("- `pictures/sheet.png` (1280x720 each, 48 samples): front and rear three-quarter, the running gear, the game's camera.")
    w("- `pictures/turntable.mp4/.gif`: 6 s, 90 frames at 15 a second, 960x540, 12 samples (denoised).")
    w("- `pictures/drive.mp4/.gif` (30 frames a second, 960x540, 8 samples, denoised) from a low game-like camera 14 m away: "
      "the drive loop exactly as the game plays it, in place. The ground (with stones, and the belts' prints behind the tank) slides back "
      "under the tank at its own speed, since the clip is in place: the bottom of the belt keeps still on its prints while the "
      "top run travels forward. The drive mp4 plays the 2 s loop three times; the gif loops by itself.")
    w("- Every link of the belts in the pictures is laid round the track's path each frame (the export's belt is fixed and "
      "the game scrolls its texture instead, see above).")
    times_p = stage_path("times.json")
    if os.path.exists(times_p):
        with open(times_p) as f:
            tm = json.load(f)
        w("- `pictures/hero.mp4/.gif` (8 s, 30 frames a second, 960x540, 8 samples, denoised): on the battle stage (battle_set.py), the "
      "tank drives in across the stubble field, lays its turret onto the wood's edge as it brakes, stops, fires (flash, a ring "
      "of grey smoke, a plume, dust blown off the ground ahead of the gun, the hull rocking along the gun's line), then drives "
      "off; exhaust haze, a tan dust trail, and the belts' prints left in the ground. The tank is carried over the ground by "
      "the picture (in the game the unit moves it); its wheels and belts turn by the ground it covers, the hull dips as it "
      "brakes (as `stop` does) and the recoil and rock are the `fire` clip's, laid along the gun's line.")
    w("- `pictures/knocked_out_stage.mp4/.gif` (6 s, 30 frames a second, 960x540, 8 samples): the same tank, stopped where it fired, is hit: sparks and a flash, "
      "the commander's hatch blown open with a gout of flame, then fire on the engine deck and a black column of smoke "
      "leaning away on the wind (the knocked_out clip, held after 3.0 s).")
    w("- The smoke, dust, flash and fire are the warship modeller's shared effects (`battle-models/shared/sea-air/stage.py`, "
      "class Effects, copied unchanged into tank.py so land and sea match), with tank-sized makers. Picture-only; the game "
      "draws its own from the events.")
    w("- The pictures' paint wear: mottled paint, rubbed edges, dried mud caked on the running gear and lower hull and "
      "splashed higher, dust low down and on the flat tops, grime in the recesses, rain streaks on the upright plates, rust "
      "and soot on the exhaust, and the markings painted where `build_tank.marks_of()` puts them (a plain white-edged cross "
      "on the turret's sides, the number 214 on the hull's). None of it is in the exports: the game paints by material name "
      "and draws its own marks.")
    w("\nRender times (Cycles on two CPU threads): " + "; ".join(f"{k} {v}" for k, v in tm.items()) + ".")
    path = stage_path("README.md")
    with open(path, "w") as f:
        f.write("\n".join(L_) + "\n")
    deliver(path, "README.md")
    return path


# ------------------------------------------------------------------------------------------------ run
STEPS = ["export", "hero", "ko_stage", "before_after", "stills", "turntable", "clips", "readme"]


def main():
    global FAST, OUT
    a = kit.args()
    FAST = "--fast" in a
    if "--out" in a:
        OUT = a[a.index("--out") + 1].rstrip("/") + "/"
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else STEPS
    movies = a[a.index("--clips") + 1].split(",") if "--clips" in a else ["drive", "fire", "knocked_out"]
    log = {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            export_all()
        elif step == "before_after":
            before_after()
            deliver_pictures("before-after.png")
        elif step == "stills":
            stills()
            deliver_pictures("sheet.png")
        elif step == "turntable":
            if FAST:
                turntable(frames=24, fps=8)
            else:
                turntable(frames=90, fps=15, w=960, h=540, samples=12)
            deliver_pictures("turntable.mp4", "turntable.gif")
        elif step == "clips":
            for name in movies:
                if FAST:
                    clip_movie(name)
                else:
                    clip_movie(name, w=960, h=540, samples=8, repeat=3 if name == "drive" else 1)
                deliver_pictures(name + ".mp4", name + ".gif")
        elif step == "hero":
            if FAST:
                hero()
            else:
                hero(w=960, h=540, samples=8)
            deliver_pictures("hero.mp4", "hero.gif")
        elif step == "ko_stage":
            if FAST:
                ko_stage()
            else:
                ko_stage(w=960, h=540, samples=8)
            deliver_pictures("knocked_out_stage.mp4", "knocked_out_stage.gif")
        elif step == "readme":
            write_readme()
        log[step] = round(time.time() - t0, 1)
        print(f"step {step}: {log[step]} s")
        if step not in ("export", "readme"):
            tp = stage_path("times.json")
            tm = json.load(open(tp)) if os.path.exists(tp) else {}
            tm[step + (" (test size)" if FAST else "")] = f"{log[step] / 60:.1f} min"
            with open(tp, "w") as f:
                json.dump(tm, f)
    ok = check_delivered()
    print("delivered" if ok else "SOME FILES DID NOT LAND", log)


if __name__ == "__main__":
    main()
