"""The aircraft, sculpted wholly by this script to the standard of the tanks.

Run by Blender without its window (see build_plane.sh):

    Blender --background --python Tools/build_plane.py -- --all Content/Models [--only Bf109,Zero] [--look folder]

Each machine is a set of smooth skins drawn over numbers: a fuselage over a row of cross-sections from nose to
tail, a wing over a row of aerofoil sections from root to tip, and the same again for tailplane, fin, hood and
nacelles. Nothing is drawn by hand and nothing is fetched. The skins are kept smooth; the detail is added parts
and scribed lines, never facets: a framed hood over dark glass, a spinner with separate blades, exhaust stubs or
an open cowling with its engine and gills, guns, a pitot, an aerial mast with its wire, the control surfaces and a
few panel lines scribed as thin dark lines, wheel wells and doors scribed underneath, a tailwheel.

What is written out is plain numbers the game reads when it starts: points (metres; x forward, y to the right,
z up, the middle of the fuselage at the origin as the machine flies), which way each faces, a paint for each
(see PAINTS), how worn the paint is there, the triangles, the colours of the paints, and where the markings go.
A second, plainer mesh is written for the table, where an airfield carries several.
"""

import json
import os
import math
import sys

import bmesh
import bpy
from mathutils import Vector, noise

# The paints, by number. The file says what colour each is for this machine.
PAINTS = {"upper": 0, "upper2": 1, "under": 2, "nose": 3, "boss": 4, "glass": 5, "frame": 6, "steel": 7, "tyre": 8,
          "blade": 9, "line": 10, "soot": 11, "engine": 12, "leg": 13, "bomb": 14, "stain": 15, "glare": 16, "glint": 17}
# (the paints that are the same on every machine)
COMMON = {"glass": "2B4254", "glint": "9DB4C2", "frame": "34383A", "steel": "3A3C40", "tyre": "1B1B1A", "blade": "161616", "line": "1E211F",
          "soot": "241F1B", "engine": "141516", "leg": "8A8E90", "bomb": "3A3D38", "glare": "4F5236"}


# ---- Curves and skins ----------------------------------------------------------------------------

def curve(xs, ys):
    """A smooth curve through points that never overshoots them, so a step between two close stations stays a step."""
    n = len(xs)
    h = [xs[i + 1] - xs[i] for i in range(n - 1)]
    d = [(ys[i + 1] - ys[i]) / h[i] if h[i] > 1.0e-9 else 0.0 for i in range(n - 1)]
    m = [0.0] * n
    m[0], m[-1] = d[0], d[-1]
    for i in range(1, n - 1):
        if d[i - 1] * d[i] > 0.0:
            w1, w2 = 2.0 * h[i] + h[i - 1], h[i] + 2.0 * h[i - 1]
            m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])

    def at(x):
        if x <= xs[0]:
            return ys[0]
        if x >= xs[-1]:
            return ys[-1]
        i = 0
        while xs[i + 1] < x:
            i += 1
        t = (x - xs[i]) / h[i]
        return ((2 * t ** 3 - 3 * t ** 2 + 1) * ys[i] + (t ** 3 - 2 * t ** 2 + t) * h[i] * m[i]
                + (-2 * t ** 3 + 3 * t ** 2) * ys[i + 1] + (t ** 3 - t ** 2) * h[i] * m[i + 1])
    return at


class Body:
    """A round body over stations (x, z of its middle, half its width, half its height): a fuselage, a hood, a nacelle."""

    def __init__(self, stations, square=2.0, side=0.0):
        st = sorted(stations, key=lambda r: -r[0])
        t = [-r[0] for r in st]
        self.front, self.back = st[0][0], st[-1][0]
        self.z, self.w, self.h = curve(t, [r[1] for r in st]), curve(t, [r[2] for r in st]), curve(t, [r[3] for r in st])
        self.power, self.side, self.stations = 2.0 / square, side, [r[0] for r in st]

    def at(self, x, a, proud=0.0):
        c, s = math.cos(a), math.sin(a)
        y, z = math.copysign(abs(c) ** self.power, c), math.copysign(abs(s) ** self.power, s)
        return Vector((x, self.side + y * (self.w(-x) + proud), self.z(-x) + z * (self.h(-x) + proud)))

    def out(self, x, a):
        """Which way the skin faces there."""
        e = 0.01
        along = self.at(min(x + e, self.front), a) - self.at(max(x - e, self.back), a)
        round_ = self.at(x, a + e) - self.at(x, a - e)
        n = round_.cross(along)
        if n.length < 1.0e-9:
            return Vector((0.0, math.cos(a), math.sin(a)))
        n.normalize()
        return n if n.dot(Vector((0.0, math.cos(a), math.sin(a)))) >= 0.0 else -n

    def xs(self, step):
        """Where its rings stand: at every station, and close enough between them."""
        out = []
        for a, b in zip(self.stations, self.stations[1:]):
            parts = max(1, int(math.ceil((a - b) / step)))
            out += [a + (b - a) * k / parts for k in range(parts)]
        return out + [self.stations[-1]]


def aerofoil(c):
    """How thick a wing section is at a share of its chord, from 0 at the nose to 0 at the trailing edge; 1 at its thickest."""
    return 10.0 * (0.2969 * math.sqrt(max(c, 0.0)) - 0.126 * c - 0.3516 * c ** 2 + 0.2843 * c ** 3 - 0.1036 * c ** 4)


class Wing:
    """A flying surface over sections (how far out, leading edge, trailing edge, height, thickness). 'hand' is +1 for
    the right-hand one. 'upright' makes a fin: its sections are stacked by height and its thickness is sideways."""

    def __init__(self, sections, hand=1.0, upright=False, side=0.0):
        self.upright, self.hand, self.side = upright, hand, side
        u = [r[3] if upright else r[0] for r in sections]
        self.root, self.tip, self.sections = u[0], u[-1], u
        self.lead, self.trail = curve(u, [r[1] for r in sections]), curve(u, [r[2] for r in sections])
        self.z, self.thick = curve(u, [r[3] for r in sections]), curve(u, [r[4] for r in sections])

    def at(self, u, c, upper=True, proud=0.0):
        x = self.lead(u) + (self.trail(u) - self.lead(u)) * c
        swell = aerofoil(c) * self.thick(u)
        if self.upright:
            return Vector((x, self.side + (1.0 if upper else -1.0) * (0.4 * swell + proud), u))
        return Vector((x, self.hand * u, self.z(u) + (0.5 * swell + proud if upper else -0.36 * swell - proud)))

    def out(self, u, c, upper=True):
        if self.upright:
            return Vector((0.0, 1.0 if upper else -1.0, 0.0))
        e = 0.05
        rise = (self.at(min(u + e, self.tip), c, upper) - self.at(max(u - e, self.root), c, upper))
        chord = self.at(u, min(c + 0.02, 1.0), upper) - self.at(u, max(c - 0.02, 0.0), upper)
        n = rise.cross(chord)
        n.normalize()
        return n if (n.z > 0.0) == upper else -n

    def us(self, step):
        out = []
        for a, b in zip(self.sections, self.sections[1:]):
            parts = max(1, int(math.ceil(abs(b - a) / step)))
            out += [a + (b - a) * k / parts for k in range(parts)]
        return out + [self.sections[-1]]

    def ring(self, u, chords):
        return [self.at(u, c, True) for c in chords] + [self.at(u, c, False) for c in chords[-2:0:-1]]


class Parts:
    """Everything a machine is made of, as Blender objects, each painted or carrying a paint for every face."""

    def __init__(self, fine):
        self.fine, self.objects = fine, []

    def add(self, name, bm, paint=None, detail=False, outward=True):
        if outward:
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        obj["paint"] = -1 if paint is None else PAINTS[paint]
        obj["detail"] = 1 if detail else 0
        self.objects.append(obj)
        return obj

    def skin(self, name, rings, paint_of, caps=(True, True)):
        """A smooth skin over rings of points, each face painted by 'paint_of(middle, facing)'."""
        bm = bmesh.new()
        layer = bm.faces.layers.int.new("paint")
        rows = [[bm.verts.new(p) for p in ring] for ring in rings]
        n = len(rows[0])
        faces = []
        for a, b in zip(rows, rows[1:]):
            for k in range(n):
                faces.append(bm.faces.new((a[k], a[(k + 1) % n], b[(k + 1) % n], b[k])))
        for end, row in ((0, rows[0]), (1, rows[-1])):
            if caps[end]:
                hub = bm.verts.new(sum((v.co for v in row), Vector()) / n)
                for k in range(n):
                    faces.append(bm.faces.new((hub, row[k], row[(k + 1) % n])))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.normal_update()
        # (The coat is laid by point and not by face: the game shades from point to point, so the patches of a
        # camouflage and the line where the under-surface colour begins come out soft, as sprayed paint is, and
        # not in steps. A hard paint, such as a nose band or glass, is laid by face and keeps its edge.)
        by_point = bm.verts.layers.int.new("coat")
        blend = bm.verts.layers.float.new("blend")
        bm.verts.layers.float.new("soot")
        blend_of = getattr(paint_of, "blend", None)
        for v in bm.verts:
            paint = paint_of(v.co, v.normal)
            if blend_of and paint in ("upper", "upper2"):
                # (The patches of a camouflage are not laid point by point as one colour or the other, which made
                # their edges a staircase of square steps: each point says how far it is into the second colour,
                # and the game mixes the two, so the edge between them runs smooth and a little soft.)
                paint = "upper"
                v[blend] = blend_of(v.co)
            v[by_point] = PAINTS[paint]
        for f in bm.faces:
            paint = paint_of(f.calc_center_median(), f.normal)
            f[layer] = -1 if paint in ("upper", "upper2", "under") else PAINTS[paint]
        return self.add(name, bm, None, False, outward=False)

    def strip(self, bm, points, facings, half, lift=0.004, taper=None):
        """A thin ribbon lying on a skin along a row of points: a scribed line, a frame, a stain."""
        if len(points) < 2:
            return
        edge = []
        for k, (p, n) in enumerate(zip(points, facings)):
            along = points[min(k + 1, len(points) - 1)] - points[max(k - 1, 0)]
            side = along.cross(n)
            if side.length < 1.0e-9:
                side = Vector((0.0, 1.0, 0.0))
            side.normalize()
            wide = half * (taper(k / (len(points) - 1)) if taper else 1.0)
            edge.append((bm.verts.new(p + n * lift + side * wide), bm.verts.new(p + n * lift - side * wide)))
        for (a0, b0), (a1, b1), n in zip(edge, edge[1:], facings):
            f = bm.faces.new((a0, b0, b1, a1))
            f.normal_update()
            if f.normal.dot(n) < 0.0:
                f.normal_flip()

    def rod(self, name, a, b, ra, rb=None, paint="steel", sides=8, detail=False):
        a, b = Vector(a), Vector(b)
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=sides, radius1=ra, radius2=ra if rb is None else rb, depth=(b - a).length)
        turn = Vector((0.0, 0.0, 1.0)).rotation_difference((b - a).normalized()).to_matrix().to_4x4()
        from mathutils import Matrix
        bmesh.ops.transform(bm, matrix=Matrix.Translation((a + b) * 0.5) @ turn, verts=bm.verts)
        return self.add(name, bm, paint, detail)

    def ball(self, name, at, radii, paint, u=12, v=8, detail=False):
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=1.0)
        for vert in bm.verts:
            vert.co = Vector((vert.co.x * radii[0] + at[0], vert.co.y * radii[1] + at[1], vert.co.z * radii[2] + at[2]))
        return self.add(name, bm, paint, detail)


# ---- One machine ---------------------------------------------------------------------------------

def coat_of(s):
    """The paint of a point of the skin: under surfaces in one colour, upper surfaces in one or in two in patches."""
    patch = s.get("patch", 0.0)
    under_from = s.get("under_from", -0.3)                 # (a night bomber's black comes well up its sides)

    def field(p):
        return noise.noise(Vector(((p.x + 31.7) / patch, (p.y * 1.3 + 12.3) / patch, 0.0))) - 0.02

    def paint(p, n):
        if n.z < under_from:
            return "under"
        if patch > 0.0 and field(p) > 0.0:
            return "upper2"
        return "upper"

    def blend(p):
        """How far a point of the upper surfaces is into the second colour: 0 in the first, 1 in the second, between at the edge of a patch."""
        if patch <= 0.0:
            return 0.0
        t = min(max((field(p) + 0.045) / 0.09, 0.0), 1.0)
        return t * t * (3.0 - 2.0 * t)
    paint.blend = blend
    return paint


def with_blend(paint, coat):
    """A paint that falls back on the coat carries the coat's soft patches with it."""
    paint.blend = coat.blend
    return paint


def blade(parts, hub, radius, angle, axis_x=1.0):
    """One blade of an airscrew: broad at a third of its length, fined to its tip, and twisted from root to tip."""
    bm = bmesh.new()
    along = Vector((0.0, math.sin(angle), math.cos(angle)))
    across0 = Vector((axis_x, 0.0, 0.0)).cross(along)
    rings = []
    for share, chord, thick, twist in ((0.1, 0.07, 0.05, 62.0), (0.3, 0.115, 0.03, 48.0), (0.6, 0.1, 0.018, 30.0), (0.88, 0.07, 0.01, 20.0), (1.0, 0.03, 0.005, 16.0)):
        t = math.radians(twist)
        flat = across0 * math.cos(t) + Vector((axis_x, 0.0, 0.0)) * math.sin(t)
        up = flat.cross(along)
        mid = Vector(hub) + along * radius * share
        ring = []
        for k in range(10):
            a = 2.0 * math.pi * k / 10.0
            ring.append(mid + flat * (math.cos(a) * chord * radius) + up * (math.sin(a) * thick * radius))
        rings.append([bm.verts.new(p) for p in ring])
    for a, b in zip(rings, rings[1:]):
        for k in range(10):
            bm.faces.new((a[k], a[(k + 1) % 10], b[(k + 1) % 10], b[k]))
    bm.faces.new(rings[0])
    bm.faces.new(rings[-1])
    return parts.add("blade", bm, "blade")


def airscrew(parts, hub, radius, blades, turn=20.0):
    for k in range(blades):
        blade(parts, hub, radius, math.radians(turn) + 2.0 * math.pi * k / blades)


def radial_details(parts, frames, body, mouth, gills):
    """The engine seen in the mouth of its cowling, a ring of cylinder heads round the crankcase; and the cooling
    gills round the back of the cowling, a ring of plates standing a little open."""
    z = body.z(-mouth)
    r = body.w(-(mouth - 0.05)) * 0.62
    if parts.fine:
        for k in range(14):
            a = 2.0 * math.pi * k / 14.0
            parts.rod("cylinder", (mouth - 0.12, body.side + math.cos(a) * r, z + math.sin(a) * r), (mouth + 0.015, body.side + math.cos(a) * r, z + math.sin(a) * r),
                      0.075, paint="steel", sides=8, detail=True)
    for k in range(16):
        a0, a1 = 2.0 * math.pi * (k + 0.08) / 16.0, 2.0 * math.pi * (k + 0.92) / 16.0
        steps = [a0 + (a1 - a0) * j / (4.0 if parts.fine else 2.0) for j in range(5 if parts.fine else 3)]
        parts.strip(frames, [body.at(gills + 0.1, a, 0.0) + (body.at(gills - 0.14, a, 0.03) - body.at(gills + 0.1, a, 0.0)) * 0.5 for a in steps],
                    [body.out(gills, a) for a in steps], 0.12, 0.012)


def stub_details(parts, body, x0, x1, angle, reach=1.5):
    """The exhaust stubs along each side of an engine in line, and the stain they leave behind them on the skin."""
    for hand in (1.0, -1.0):
        a = math.radians(angle) if hand > 0.0 else math.pi - math.radians(angle)
        for k in range(6):
            x = x0 + (x1 - x0) * k / 5.0
            p, n = body.at(x, a), body.out(x, a)
            parts.rod("stub", p - n * 0.02, p + n * 0.05 + Vector((-0.07, 0.0, 0.0)), 0.035, 0.03, paint="soot", sides=7 if parts.fine else 4)
        parts.stains.append((body, x1 - 0.05, reach, a))


def soot_on(parts):
    """The stain of the exhausts, laid into the skin itself: each point of the fuselage or a nacelle says how sooted it
    is, most just behind the last stub and thinning away astern and to each side, so that it has no hard edge."""
    for obj in parts.objects:
        if obj.name.split(".")[0] not in ("hull", "nacelle") or not parts.stains:
            continue
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        soot = bm.verts.layers.float.get("soot")
        for v in bm.verts:
            most = 0.0
            for body, start, reach, angle in parts.stains:
                along = (start - v.co.x) / reach
                if along < -0.08 or along > 1.0 or abs(v.co.y - body.side) > 3.0:
                    continue
                away = (v.co - body.at(v.co.x, angle)).length
                wide = 0.1 + 0.14 * max(along, 0.0)
                most = max(most, max(0.0, 1.0 - away / wide) ** 1.3 * min(1.0, (along + 0.08) * 6.0) * (1.0 - max(along, 0.0)) ** 0.8)
            v[soot] = most
        bm.to_mesh(obj.data)
        bm.free()


def dome(parts, frames, at, radii, ribs=True):
    """A gun turret or a blister: a dome of glass on its ring, framed with a hoop a third of the way up and two ribs over the top."""
    parts.ball("dome", at, radii, "glass", 16, 10)
    if not parts.fine:
        return
    cx, cy, cz = at
    up = 0.35
    out = math.sqrt(1.0 - up * up)
    angles = [2.0 * math.pi * k / 28.0 for k in range(29)]
    pts = [Vector((cx + math.cos(a) * out * radii[0], cy + math.sin(a) * out * radii[1], cz + up * radii[2])) for a in angles]
    parts.strip(frames, pts, [Vector((math.cos(a), math.sin(a), 0.3)).normalized() for a in angles], 0.018, 0.01)
    if ribs:
        for across in (-0.35, 0.35):
            shrink = math.sqrt(1.0 - across * across)
            arc = [math.pi * k / 14.0 for k in range(15)]
            pts = [Vector((cx + math.cos(a) * shrink * radii[0], cy + across * radii[1], cz + math.sin(a) * shrink * radii[2])) for a in arc]
            parts.strip(frames, pts, [Vector((math.cos(a) * shrink, across, math.sin(a) * shrink)).normalized() for a in arc], 0.016, 0.01)


def lines_on_wing(parts, bm, wing, both, flap, aileron, ribs, panel=(0.14, 0.42)):
    """The control surfaces and a few panel lines of a wing or tailplane, scribed: a hinge line, the ends of each
    surface, and (on a wing) two lines along the span and a few ribs. 'both' scribes the under surface too."""
    span = wing.tip - wing.root

    def along(u0, u1, c, upper):
        us = [u0 + (u1 - u0) * k / 14.0 for k in range(15)]
        parts.strip(bm, [wing.at(u, c, upper) for u in us], [wing.out(u, c, upper) for u in us], 0.011)

    def across(u, c0, c1, upper):
        cs = [c0 + (c1 - c0) * k / 8.0 for k in range(9)]
        parts.strip(bm, [wing.at(u, c, upper) for c in cs], [wing.out(u, c, upper) for c in cs], 0.011)

    for upper in ((True, False) if both else (True,)):
        for (u0, u1, c) in (flap, aileron):
            if u1 > u0:
                a, b = wing.root + span * u0, wing.root + span * u1
                along(a, b, c, upper)
                across(a, c, 0.99, upper)
                across(b, c, 0.99, upper)
        for c in panel:
            along(wing.root + span * 0.06, wing.root + span * 0.9, c, upper)
        for share in ribs:
            across(wing.root + span * share, 0.03, 0.7, upper)


def build(s, fine=True):
    """Sculpts one machine from its numbers. 'fine' is the model for the cards and the parade; the other is the table's."""
    parts = Parts(fine)
    coat = coat_of(s)
    hull = Body(s["hull"], s.get("square", 2.0))
    around = 28 if fine else 14
    boss_back = 1.0e9 if s.get("engines") else sorted((r[0] for r in s["hull"]), reverse=True)[s.get("spinner", 2) - 1] - 0.01
    nose_back = s.get("nose_back") if s.get("nose_back") is not None else 1.0e9
    radial = s.get("engine", "inline") == "radial"
    lip = s.get("lip")                                     # (a radial engine: where the open front of its cowling is)
    glazed = s.get("glazed")                               # (a bomber's glass nose: everything ahead of this)

    def hull_paint(p, n):
        if p.x > boss_back:
            return "boss"
        if radial and lip is not None and p.x > lip - 0.02 and n.x > 0.6:
            return "engine"                                # (the open mouth of the cowling)
        if glazed is not None and (p.x > glazed[0] or (p.x > glazed[1] and n.z > -0.55)):
            return "glass"
        for (x0, x1, half_high) in s.get("glass_bands", []):   # (a tail gunner's windows, a row of cabin windows: on the sides only)
            if x1 < p.x < x0 and abs(n.y) > 0.7 and abs(p.z - hull.z(-p.x) - hull.h(-p.x) * 0.3) < half_high:
                return "glass"
        if p.x > nose_back:
            return "nose"
        return coat(p, n)

    hull_paint.blend = coat.blend
    rings = [[hull.at(x, 2.0 * math.pi * k / around) for k in range(around)] for x in hull.xs(0.24 if fine else 0.6)]
    parts.skin("hull", rings, hull_paint)

    # The hood: dark glass under its frames.
    lines = bmesh.new()                                    # every scribed line of the machine
    frames = bmesh.new()
    glints = bmesh.new()
    parts.stains = []                                      # (where exhaust has stained the skin: see soot_on)
    for hood_stations, hoops, spine in s.get("hoods", []):
        hood = Body(hood_stations, 2.3)
        hood_around = 20 if fine else 10
        parts.skin("hood", [[hood.at(x, 2.0 * math.pi * k / hood_around) for k in range(hood_around)] for x in hood.xs(0.2 if fine else 0.5)],
                   lambda p, n: "glass")
        # (the frames are on the table's model too: they read from above)
        steps = 16 if fine else 8
        angles = [math.radians(8.0 + 164.0 * k / steps) for k in range(steps + 1)]
        for x in hoops:
            parts.strip(frames, [hood.at(x, a) for a in angles], [hood.out(x, a) for a in angles], 0.022 if fine else 0.03, 0.008)
        xs = [hood.front - 0.04 - (hood.front - hood.back - 0.08) * k / steps for k in range(steps + 1)]
        for a in ([math.radians(14.0), math.radians(166.0)] + ([math.radians(90.0)] if spine else [])):
            parts.strip(frames, [hood.at(x, a) for x in xs], [hood.out(x, a) for x in xs], 0.02 if fine else 0.028, 0.008)
        # A soft glint along the glass on each shoulder of the hood: the light of the sky on it.
        long = hood.front - hood.back
        gx = [hood.front - long * (0.22 + 0.5 * k / 10.0) for k in range(11)]
        for a in (math.radians(52.0), math.radians(128.0)):
            parts.strip(glints, [hood.at(x, a) for x in gx], [hood.out(x, a) for x in gx], 0.035, 0.004, taper=lambda t: math.sin(math.pi * min(max(t, 0.02), 0.98)))

    # Wings, tailplane and fin.
    chords = [0.5 * (1.0 - math.cos(math.pi * k / (9 if fine else 5))) for k in range((9 if fine else 5) + 1)]
    wing_sections = s["wing"]
    for hand in (1.0, -1.0):
        wing = Wing(wing_sections, hand)
        parts.skin("wing", [wing.ring(u, chords) for u in wing.us(0.42 if fine else 1.2)], coat, caps=(False, True))
        tail = Wing(s["tail"], hand)
        parts.skin("tail", [tail.ring(u, chords) for u in tail.us(0.4 if fine else 1.0)], coat, caps=(False, True))
        if fine:
            lines_on_wing(parts, lines, wing, True, s.get("flap", (0.1, 0.5, 0.76)), s.get("aileron", (0.54, 0.93, 0.74)), (0.3, 0.52, 0.74))
            lines_on_wing(parts, lines, tail, True, (0.08, 0.95, 0.62), (0.0, 0.0, 0.0), (), panel=())
            # The wheel well and its door, scribed under the wing; the gun barrels in its leading edge; the pitot.
            well = s.get("well")
            if well:
                u, c, r = well
                mid = wing.at(u, c, False)
                ringp = [Vector((mid.x + math.cos(a) * r, mid.y + math.sin(a) * r, 0.0)) for a in [2.0 * math.pi * k / 24.0 for k in range(25)]]
                on = [wing.at(abs(p.y), min(max((p.x - wing.lead(abs(p.y))) / (wing.trail(abs(p.y)) - wing.lead(abs(p.y))), 0.02), 0.98), False) for p in ringp]
                parts.strip(lines, on, [Vector((0.0, 0.0, -1.0))] * len(on), 0.012)
                leg = [wing.at(u + (0.2 + 1.2 * k / 6.0) * (-1.0 if s.get("well_in") else 1.0), c, False) for k in range(7)]
                for off in (-0.14, 0.14):
                    parts.strip(lines, [p + Vector((off, 0.0, 0.0)) for p in leg], [Vector((0.0, 0.0, -1.0))] * len(leg), 0.011)
            for (out, back, length, bore) in s.get("guns", []):
                at = wing.at(out, 0.02, True)
                parts.rod("gun", (at.x - back, at.y, at.z - 0.03), (at.x + length, at.y, at.z - 0.03), bore, paint="steel", sides=7, detail=True)
        if s.get("pitot") and hand < 0.0 and fine:
            u = wing.root + (wing.tip - wing.root) * 0.86
            at = wing.at(u, 0.15, False)
            parts.rod("pitot", (at.x, at.y, at.z - 0.1), (at.x + 0.55, at.y, at.z - 0.1), 0.012, paint="steel", sides=5, detail=True)
            parts.rod("pitot_leg", (at.x, at.y, at.z), (at.x + 0.05, at.y, at.z - 0.1), 0.01, paint="steel", sides=5, detail=True)
    fins = s.get("fins") or [(s["fin"], 0.0)]
    for fin_sections, side in fins:
        for hand_side in ((side, -side) if side else (0.0,)):
            fin = Wing(fin_sections, 1.0, True, hand_side)
            parts.skin("fin", [fin.ring(u, chords) for u in fin.us(0.3 if fine else 0.8)], (lambda p, n: "under") if s.get("fins_under") else coat, caps=(True, True) if side else (False, True))
            if fine:
                us = [fin.root + (fin.tip - fin.root) * (0.06 + 0.9 * k / 12.0) for k in range(13)]
                for upper in (True, False):
                    parts.strip(lines, [fin.at(u, 0.6, upper) for u in us], [fin.out(u, 0.6, upper) for u in us], 0.011)

    # The fuselage's own lines: a few frames round it, and a line along each side.
    if fine:
        length = hull.front - hull.back
        hoops = s.get("hull_lines") or [hull.front - length * f for f in (0.2, 0.52, 0.68, 0.84)]
        angles = [2.0 * math.pi * k / 40.0 for k in range(41)]
        for x in hoops:
            parts.strip(lines, [hull.at(x, a) for a in angles], [hull.out(x, a) for a in angles], 0.011)
        xs = [hoops[0] - (hoops[0] - hoops[-1]) * k / 20.0 for k in range(21)]
        for a in (math.radians(12.0), math.radians(168.0)):
            parts.strip(lines, [hull.at(x, a) for x in xs], [hull.out(x, a) for x in xs], 0.011)

    # The engine in the nose.
    hub_x = sorted((r[0] for r in s["hull"]), reverse=True)[s.get("spinner", 2) - 1] + 0.06
    hub_z = hull.z(-hub_x)
    if not s.get("engines"):
        airscrew(parts, (hub_x, 0.0, hub_z), s.get("screw", 1.5), s.get("blades", 3))
        if radial and lip is not None:
            radial_details(parts, frames, hull, lip, nose_back if nose_back < 1.0e8 else lip - 1.0)
        elif s.get("stubs"):
            stub_details(parts, hull, s["stubs"][0], s["stubs"][1], s["stubs"][2])

    # Engines on the wings: each in its nacelle, round and stubby (air-cooled) or long and narrow (in line).
    for index, (out, lead, z, is_radial, back) in enumerate(s.get("engines", [])):
        for hand in (1.0, -1.0):
            side = out * hand
            n_around = 20 if fine else 10
            if is_radial:
                nacelle = Body([(lead + 1.75, z, 0.46, 0.46), (lead + 1.65, z, 0.66, 0.66), (lead + 0.8, z, 0.68, 0.68), (lead - 0.4, z - 0.05, 0.62, 0.58),
                                (lead - back * 0.7, z - 0.1, 0.38, 0.32), (lead - back, z - 0.02, 0.08, 0.07)], 2.0, side)
                parts.skin("nacelle", [[nacelle.at(x, 2.0 * math.pi * k / n_around) for k in range(n_around)] for x in nacelle.xs(0.3 if fine else 0.8)],
                           with_blend(lambda p, n, mouth=lead + 1.7: "engine" if p.x > mouth and n.x > 0.5 else coat(p, n), coat))
                parts.rod("crankcase", (lead + 1.7, side, z), (lead + 2.02, side, z), 0.16, 0.1, paint="leg", sides=10)
                airscrew(parts, (lead + 1.96, side, z), s.get("screw", 1.7), s.get("blades", 3), 20.0 + 27.0 * index)
                radial_details(parts, frames, nacelle, lead + 1.75, lead + 0.75)
            else:
                nacelle = Body([(lead + 2.55, z, 0.04, 0.04), (lead + 2.1, z, 0.34, 0.34), (lead + 2.05, z - 0.04, 0.4, 0.48), (lead + 0.6, z - 0.1, 0.47, 0.6),
                                (lead - 1.2, z - 0.14, 0.44, 0.5), (lead - back, z - 0.04, 0.1, 0.1)], 2.3, side)
                parts.skin("nacelle", [[nacelle.at(x, 2.0 * math.pi * k / n_around) for k in range(n_around)] for x in nacelle.xs(0.3 if fine else 0.8)],
                           with_blend(lambda p, n, boss=lead + 2.09: "boss" if p.x > boss else coat(p, n), coat))
                airscrew(parts, (lead + 2.16, side, z), s.get("screw", 1.7), s.get("blades", 3), 20.0 + 31.0 * index)
                stub_details(parts, nacelle, lead + 1.75, lead + 0.95, 10.0, 1.2)

    # A glass nose is framed: ribs from its tip back, and hoops round it.
    if glazed is not None and fine:
        hoops, ribs = s.get("nose_frames", ((), 8))
        xs = [hull.front - 0.02 - (hull.front - glazed[0] - 0.04) * k / 12.0 for k in range(13)]
        for k in range(ribs):
            a = 2.0 * math.pi * k / ribs
            parts.strip(frames, [hull.at(x, a) for x in xs], [hull.out(x, a) for x in xs], 0.016, 0.008)
        angles = [2.0 * math.pi * k / 36.0 for k in range(37)]
        for x in hoops:
            parts.strip(frames, [hull.at(x, a) for a in angles], [hull.out(x, a) for a in angles], 0.018, 0.008)

    # The panel ahead of the hood that keeps the glare out of the pilot's eyes: a dull strip along the top of the nose.
    if s.get("glare") and fine:
        x0, x1, half = s["glare"]
        glare = bmesh.new()
        xs = [x0 + (x1 - x0) * k / 14.0 for k in range(15)]
        parts.strip(glare, [hull.at(x, math.radians(90.0)) for x in xs], [hull.out(x, math.radians(90.0)) for x in xs], half, 0.003)
        parts.add("glare", glare, "glare", True, outward=False)

    # Radiators and scoops: small bodies under the wings or the belly, each with a dark mouth.
    for stations, side, square in s.get("scoops", []):
        for hand_side in ((side, -side) if side else (0.0,)):
            scoop = Body(stations, square, hand_side)
            n_around = 14 if fine else 8
            parts.skin("scoop", [[scoop.at(x, 2.0 * math.pi * k / n_around) for k in range(n_around)] for x in scoop.xs(0.25 if fine else 0.6)],
                       with_blend(lambda p, n, front=scoop.front, top=stations[1][1] > 0.0: "engine" if p.x > front - 0.06 and n.x > 0.5 else (coat(p, n) if top else "under"), coat))

    # The tailwheel, the aerial mast with its wire, and anything fixed under the machine.
    if s.get("tailwheel"):
        x, z = s["tailwheel"]
        foot = hull.at(x + 0.08, -math.pi / 2.0)           # (the fuselage's own underside there: the leg starts inside it)
        z = min(z, foot.z - 0.2)
        parts.rod("tailwheel_leg", (x + 0.08, 0.0, foot.z + 0.05), (x, 0.0, z), 0.03, paint="leg", sides=6 if fine else 4)
        parts.rod("tailwheel", (x, -0.04, z), (x, 0.04, z), 0.12, paint="tyre", sides=12 if fine else 6)
    if s.get("mast") and fine:
        x, z0, z1 = s["mast"]
        parts.rod("mast", (x, 0.0, z0), (x - 0.12, 0.0, z1), 0.022, 0.012, paint="frame", sides=6, detail=True)
        fin0 = fins[0][0]
        top = (fin0[-2][1] * 0.5 + fin0[-2][2] * 0.5 + 0.1, 0.0, fin0[-2][3])
        parts.rod("wire", (x - 0.12, 0.0, z1), top, 0.006, paint="frame", sides=4, detail=True)
    for kind, values in s.get("extras", []):
        if kind == "rod":
            a, b, ra, rb, paint = values
            for hand in ((1.0, -1.0) if a[1] != 0.0 or b[1] != 0.0 else (1.0,)):
                parts.rod("extra", (a[0], a[1] * hand, a[2]), (b[0], b[1] * hand, b[2]), ra, rb, paint=paint, sides=10)
        elif kind == "ball":
            at, radii, paint = values
            for hand in ((1.0, -1.0) if at[1] != 0.0 else (1.0,)):
                parts.ball("extra", (at[0], at[1] * hand, at[2]), radii, paint)
        elif kind == "dome":
            at, radii = values
            dome(parts, frames, at, radii)
        elif kind == "body":
            stations, side, square, paint = values
            for hand_side in ((side, -side) if side else (0.0,)):
                body = Body(stations, square, hand_side)
                parts.skin("extra", [[body.at(x, 2.0 * math.pi * k / 12.0) for k in range(12)] for x in body.xs(0.3)],
                           (coat if paint == "coat" else (lambda p, n, paint=paint: paint)))
    if fine:
        parts.add("lines", lines, "line", True, outward=False)
    else:
        lines.free()
    parts.add("frames", frames, "frame", False, outward=False)
    parts.add("glints", glints, "glint", False, outward=False)
    soot_on(parts)
    return parts, hull


def gather(parts):
    """Every part's mesh as flat lists the game can read: points, which way each faces, its paint, and the triangles."""
    points, facings, paints, triangles, blends, soots = [], [], [], [], [], []
    seen = {}
    crease = math.cos(math.radians(50.0))
    for obj in parts.objects:
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        layer = bm.faces.layers.int.get("paint")
        by_point = bm.verts.layers.int.get("coat")
        blend_layer, soot_layer = bm.verts.layers.float.get("blend"), bm.verts.layers.float.get("soot")
        own = int(obj["paint"])
        bm.normal_update()
        for face in bm.faces:
            of_face = own if own >= 0 else face[layer]
            loops = list(face.loops)
            corners = []
            for loop in loops:
                paint = of_face if of_face >= 0 else loop.vert[by_point]
                facing = Vector((0.0, 0.0, 0.0))
                for near in loop.vert.link_faces:
                    if near.normal.dot(face.normal) >= crease:
                        facing += near.normal * near.calc_area()
                facing = facing.normalized() if facing.length > 1.0e-9 else face.normal.copy()
                p = loop.vert.co
                key = (round(p.x, 4), round(p.y, 4), round(p.z, 4), round(facing.x, 3), round(facing.y, 3), round(facing.z, 3), paint)
                index = seen.get(key)
                if index is None:
                    index = len(points) // 3
                    seen[key] = index
                    points.extend(key[0:3])
                    facings.extend(key[3:6])
                    paints.append(paint)
                    blends.append(int(round(100.0 * loop.vert[blend_layer])) if blend_layer is not None and of_face < 0 and paint == PAINTS["upper"] else 0)
                    soots.append(int(round(100.0 * loop.vert[soot_layer])) if soot_layer is not None else 0)
                corners.append(index)
            for k in range(1, len(corners) - 1):
                triangles.extend((corners[0], corners[k], corners[k + 1]))
        bm.free()
    return {"points": points, "facings": facings, "paints": paints, "triangles": triangles, "blend": blends, "soot": soots}


def wear_of(mesh, s, hull):
    """How worn the paint is at each point, 0 to 100: chipped where boots and hands go (the wing roots, the cowling),
    and along the leading edges, where the air throws grit."""
    half = Wing(s["wing"]).tip
    side = hull.w(0.0)
    nose_back = s.get("nose_back") if s.get("nose_back") is not None else hull.front - (hull.front - hull.back) * 0.22
    out = []
    for k in range(len(mesh["paints"])):
        x, y, z = mesh["points"][3 * k:3 * k + 3]
        up = mesh["facings"][3 * k + 2]
        w = 0.0
        if abs(y) > side * 0.8 and abs(y) < side + 0.9 and up > 0.5 and abs(z) < 1.0:
            w = max(w, 70.0 * (1.0 - (abs(y) - side * 0.8) / (0.9 + side * 0.2)))        # the walkway at the wing root
        if x > nose_back - 0.25:
            w = max(w, 38.0)                                                               # the cowling panels
        if abs(y) > side and mesh["facings"][3 * k] > 0.75 and abs(y) < half:
            w = max(w, 45.0)                                                               # the leading edges
        out.append(int(w))
    return out


def marks_of(s, hull):
    """Where the markings go, each as a place, a size, the way the skin faces there, and how round the skin is."""
    marks = []
    wing = Wing(s["wing"])
    for (x, radius, tag) in s.get("side_marks", []):
        # (no bigger than the flat of the side will carry: a cross that ran up onto the fuselage's shoulder was pinched by its curve)
        radius = round(min(radius, 0.56 * hull.h(-x)), 3)
        p, n = hull.at(x, 0.0), hull.out(x, 0.0)
        bend = max(hull.h(-x) ** 2 / max(hull.w(-x), 0.05) * (s.get("square", 2.0) / 2.0) ** 2, radius * 0.8)
        # (The art director's ruling of round 22: a cross bent round the fuselage has its upright pinched, seen from
        # the side. The German cross is laid flat on the side, as it is on the wing; a roundel or a star takes the bend.)
        if tag == "GER":
            bend = 0.0
        marks.append({"tag": tag, "at": [round(v, 3) for v in (p.x, p.y + 0.02, p.z)], "radius": radius, "facing": [round(n.x, 3), round(n.y, 3), 0.0], "curve": round(bend, 3), "both": True})
    for (out, x, radius, upper, lower, both_upper, both_lower) in s.get("wing_marks", []):
        c = min(max((x - wing.lead(out)) / (wing.trail(out) - wing.lead(out)), 0.05), 0.95)
        for hand in (1.0, -1.0):
            if upper and (both_upper or hand < 0.0):
                p, n = wing.at(out, c, True, 0.012), wing.out(out, c, True)
                marks.append({"tag": upper, "at": [round(p.x, 3), round(p.y * hand, 3), round(p.z, 3)], "radius": radius, "facing": [round(n.x, 3), round(n.y * hand, 3), round(n.z, 3)], "curve": 0.0})
            if lower and (both_lower or hand > 0.0):
                p, n = wing.at(out, c, False, 0.012), wing.out(out, c, False)
                marks.append({"tag": lower, "at": [round(p.x, 3), round(p.y * hand, 3), round(p.z, 3)], "radius": radius, "facing": [round(n.x, 3), round(n.y * hand, 3), round(n.z, 3)], "curve": 0.0})
    for (x, z, radius, tag) in s.get("fin_marks", []):
        if s.get("fin_marks_at") and s.get("fins"):
            # (twin fins out on the tailplane: the flash is on the outer face of each, not on the middle line where no fin stands)
            fin = Wing(s["fins"][0][0], 1.0, True, s["fin_marks_at"])
            c = min(max((x - fin.lead(z)) / (fin.trail(z) - fin.lead(z)), 0.05), 0.95)
            p = fin.at(z, c, True, 0.015)
            marks.append({"tag": tag, "at": [round(p.x, 3), round(p.y, 3), round(p.z, 3)], "radius": radius, "facing": [0.0, 1.0, 0.0], "curve": 0.0, "both": True})
            continue
        marks.append({"tag": tag, "at": [x, 0.05, z], "radius": radius, "facing": [0.0, 1.0, 0.0], "curve": 0.0, "both": True})
        if s.get("fin_quad"):
            # (a mark that fills a four-cornered place on the rudder: lower leading corner, lower trailing, upper trailing, upper leading)
            marks[-1]["quad"] = s["fin_quad"]
    if s.get("code"):
        x, high = s["code"]
        high = round(min(high, 1.0 * hull.h(-x)), 3)
        p = hull.at(x, 0.0)
        marks.append({"tag": "#", "at": [round(p.x, 3), round(p.y + 0.02, 3), round(p.z, 3)], "radius": high, "facing": [0.0, 1.0, 0.0], "curve": 0.0, "both": True})
    return marks


def colours_of(s):
    """The colour of every paint on this machine. The stain an exhaust leaves is the machine's own upper colour, sooted."""
    colours = dict(COMMON)
    colours.update(s["colours"])
    # (the frames of a hood are painted with the machine: in its own upper colour they stand out on the dark glass)
    # (and no less than a fifth lighter than the glass, or a dark machine's hood is one black lump)
    def light(hexed):
        return sum(w * int(hexed[i:i + 2], 16) for w, i in ((0.2126, 0), (0.7152, 2), (0.0722, 4))) / 255.0
    frame = s["colours"].get("frame", s["colours"]["upper"])
    need = light(colours["glass"]) + 0.24
    if light(frame) < need:
        t = (need - light(frame)) / max(1.0 - light(frame), 0.01)
        frame = "".join("%02X" % int(int(frame[i:i + 2], 16) * (1.0 - t) + 255 * t) for i in (0, 2, 4))
    colours["frame"] = frame
    colours["ink"] = "1A1A1A" if light(s["colours"]["upper"]) > 0.5 else "F2F0E8"   # (what a side number is painted in)
    upper, soot = colours["upper"], COMMON["soot"]
    colours["stain"] = "".join("%02X" % int(int(upper[i:i + 2], 16) * 0.5 + int(soot[i:i + 2], 16) * 0.5) for i in (0, 2, 4))
    return colours


def fresh_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def picture(parts, s, path):
    """A quick picture of the model as Blender sees it, for checking its shape without the game."""
    colours = colours_of(s)
    for index, obj in enumerate(parts.objects):
        layer = None
        own = int(obj["paint"])
        names = {v: k for k, v in PAINTS.items()}
        mesh = obj.data
        by_face = []
        if own < 0:
            face_layer, point_layer = mesh.attributes["paint"], mesh.attributes["coat"]
            by_face = [a.value if a.value >= 0 else point_layer.data[poly.vertices[0]].value for poly, a in zip(mesh.polygons, face_layer.data)]
        used = sorted({own} if own >= 0 else set(by_face))
        for slot, paint in enumerate(used):
            mat = bpy.data.materials.new(f"{obj.name}-{paint}")
            hexed = colours.get(names[paint], "808080")
            mat.diffuse_color = tuple(((int(hexed[i:i + 2], 16) / 255.0) ** 2.2) for i in (0, 2, 4)) + (1.0,)
            mesh.materials.append(mat)
        if own < 0:
            for poly, value in zip(mesh.polygons, by_face):
                poly.material_index = used.index(value)
        for poly in mesh.polygons:
            poly.use_smooth = True
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_cavity = True
    scene.render.resolution_x, scene.render.resolution_y = 1400, 900
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("w")
    scene.world = world
    half = Wing(s["wing"]).tip
    for name, eye in (("above", Vector((half * 1.5, -half * 1.9, half * 1.25))), ("below", Vector((half * 1.3, -half * 1.6, -half * 1.3))),
                      ("front", Vector((half * 2.2, -half * 0.5, half * 0.35)))):
        cam_data = bpy.data.cameras.new("cam")
        cam = bpy.data.objects.new("cam", cam_data)
        scene.collection.objects.link(cam)
        cam.location = eye
        aim = Vector((-0.6, 0.0, 0.0)) - eye
        cam.rotation_euler = aim.to_track_quat("-Z", "Y").to_euler()
        cam_data.lens = 60.0
        scene.camera = cam
        scene.render.filepath = f"{path}-{name}.png"
        bpy.ops.render.render(write_still=True)


def write(name, s, out, look=None):
    fresh_scene()
    parts, hull = build(s, True)
    near = gather(parts)
    near["wear"] = wear_of(near, s, hull)
    marks = marks_of(s, hull)
    if look:
        picture(parts, s, look.rstrip("/") + "/" + name)
    fresh_scene()
    far_parts, _ = build(s, False)
    far = gather(far_parts)
    far["wear"] = wear_of(far, s, hull)
    colours = colours_of(s)
    xs, ys, zs = near["points"][0::3], near["points"][1::3], near["points"][2::3]
    data = {
        "name": s.get("title", name),
        "about": "Made by Tools/build_plane.py. Metres; x forward, y right, z up, the middle of the fuselage at the origin. Paints: " + ", ".join(f"{v} {k}" for k, v in PAINTS.items()),
        "kind": "aircraft",
        "long": round(max(xs) - min(xs), 3), "span": round(max(ys) - min(ys), 3), "high": round(max(zs) - min(zs), 3),
        "colours": [colours.get(k, "808080") for k in PAINTS],
        "nation_paint": bool(s.get("nation_paint")),
        "number_ink": colours["ink"],
        "marks": marks,
        "near": near,
        "far": far,
    }
    with open(out, "w") as file:
        json.dump(data, file, separators=(",", ":"))
    print(f"Theater Command: {name} written to {out}: {len(near['triangles']) // 3} triangles, {len(near['points']) // 3} points "
          f"(far: {len(far['triangles']) // 3} triangles), span {data['span']} m, {data['long']} m long")


# ---- The machines --------------------------------------------------------------------------------
# Each is our own model of a type of the period, from its published measurements: the numbers are the stations of its
# fuselage (x, height of the middle, half-width, half-height), the sections of its wing, tailplane and fin (how far
# out, leading edge, trailing edge, height, thickness), and what it carries.

PLANES = {}
exec(open(__file__.replace("build_plane.py", "plane_numbers.py")).read())
BATTLE_MODELS = {"Spitfire", "Hurricane", "Bf109", "Fw190", "Zero", "Yak", "Warhawk", "Fighter", "Mustang"}


def main():
    rest = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    folder = rest[rest.index("--all") + 1] if "--all" in rest else "."
    only = rest[rest.index("--only") + 1].split(",") if "--only" in rest else None
    look = rest[rest.index("--look") + 1] if "--look" in rest else None
    for name, spec in PLANES.items():
        if only and name not in only:
            continue
        # (the machines whose game file is now the air modeller's battle model, battle-models/air: Brian's yes of
        #  22 November 20:12Z for the fighters, the Mustang's of 21 November. This sculpt goes beside it in Old/, which
        #  the game reads only with -TheaterOldVehicles, to compare and time the two; it never overwrites the new one.)
        kept = "/Old/" if name in BATTLE_MODELS else "/"
        if kept != "/":
            os.makedirs(folder.rstrip("/") + "/Old", exist_ok=True)
        write(name, spec, folder.rstrip("/") + kept + name + ".json", look)


main()
