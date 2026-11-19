"""Scenery for the battles of the Mediterranean and the deserts: what stands on a field of North Africa, the Levant,
Italy, Sicily and Greece. Every shape is our own mesh, made from numbers here; no outside model, photo or texture.

    python scenery.py <out folder> [Name,Name,...|all]      # <Name>.json and <Name>.glb for the game, and index.json

The file is the land vehicles' and warships' format (`near`, `mid`, `far`: `points`, `facings`, `paints`, `triangles`;
`colours`, `paint_names`), `kind: "scenery"`, metres, x east, y to the RIGHT as written, z up from the ground at the
foot, as the Asia thread's SET-FORMAT.md (battle-models/asia/) asks. Added, point by point, `sway`: how far the wind
moves that point (0 at a trunk's foot, 1 at a frond's tip); `foot_m` (the radius of ground it stands on) and `high_m`.
Nothing here stops a man or a tank: the rules' ground decides who passes.

The same builders dress the picture stages (stage_med.py): what Brian sees in a picture is what the game is handed.
"""
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LAND = os.path.join(os.path.dirname(HERE), "land")
sys.path.insert(0, HERE)
sys.path.insert(1, LAND)

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector, noise  # noqa: E402

import kit  # noqa: E402

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))

# The colours, in the campaign map's palette (world-look-brief.md 3.3 and 3.4): the flat-roofed town's mud and pale
# stone, its one dome and its slim tower's grey-green cap; the south's plaster and red tile.
PAINTS = {
    "mud": "CEBA96", "mud_roof": "D6C4A2", "stone_pale": "E0D0AE", "stone_roof": "E6D8BA", "dome": "ECE6D6",
    "cap": "7E8C78", "dark": "2A2622", "wood": "6A5440", "wood_old": "8A7A64",
    "rock": "927A5C", "rock_dark": "6E5A44", "rock_pale": "B09A78", "sand": "D6BC8A", "sand_blown": "BFA27A",
    "thorn": "7C7A52", "thorn_dark": "53523A", "twig": "6E5E48",
    "palm_trunk": "7A6650", "palm_frond": "5E6A3C", "palm_dry": "9A8662", "dates": "8A5A2E",
    "olive_bark": "6E665A", "olive_leaf": "7D8868", "olive_leaf2": "6A7656",
    "cypress": "33402C", "cypress2": "3E4C33", "macchia": "4A5432", "macchia2": "5E6640",
    "plaster": "DCC8A2", "plaster2": "C9A97A", "tile": "AC5E40", "tile2": "94503A", "shutter": "5E6A58",
    "fieldstone": "A8987C", "fieldstone2": "8C7E66", "insulator": "D8D4CA", "iron": "4A4844",
}


def lin(h):
    return kit.hex_rgb(h)


class Prop:
    """A piece of scenery being built: one mesh, a paint on every face, a sway on every point."""

    def __init__(self, name, title, fine=True, seed=1):
        self.name, self.title, self.fine = name, title, fine
        self.rng = random.Random(seed)
        self.bm = bmesh.new()
        self.sway = self.bm.verts.layers.float.new("sway")
        self.paints = []
        self.info = {}

    def n(self, fine, plain):
        return fine if self.fine else plain

    def paint(self, name):
        if name not in self.paints:
            self.paints.append(name)
        return self.paints.index(name)

    def add(self, pts, faces, paint, sway=None, smooth=True):
        """Points and faces (lists of indices into pts); sway: a number, or a function of the point."""
        vs = [self.bm.verts.new(Vector(p)) for p in pts]
        for v in vs:
            v[self.sway] = sway(v.co) if callable(sway) else (sway or 0.0)
        mi = self.paint(paint)
        out = []
        for f in faces:
            try:
                fa = self.bm.faces.new([vs[i] for i in f])
            except ValueError:
                continue
            fa.material_index = mi
            fa.smooth = smooth
            out.append(fa)
        return vs, out

    # ---- shapes -----------------------------------------------------------------------------------------------
    def lathe(self, base, axis, profile, paint, sides=16, sway=None, smooth=True, wobble=0.0, seed=0, close=True):
        """A turned shape: profile is [(along, radius), ...] from one end to the other; `wobble` roughens the radius."""
        base, axis = Vector(base), Vector(axis).normalized()
        u = axis.cross(Z if abs(axis.z) < 0.9 else X).normalized()
        w = axis.cross(u)
        pts, faces = [], []
        for i, (a, r) in enumerate(profile):
            for k in range(sides):
                t = 2 * math.pi * k / sides
                rr = r
                if wobble and r > 0:
                    rr *= 1.0 + wobble * noise.noise(Vector((math.cos(t) * 1.7, math.sin(t) * 1.7, a * 1.3 + seed * 7.1)))
                pts.append(base + axis * a + (u * math.cos(t) + w * math.sin(t)) * rr)
        for i in range(len(profile) - 1):
            for k in range(sides):
                a0, a1 = i * sides + k, i * sides + (k + 1) % sides
                faces.append((a0, a1, a1 + sides, a0 + sides))
        if close:
            n = len(profile)
            if profile[0][1] > 0:
                pts.append(base + axis * profile[0][0])
                c = len(pts) - 1
                faces += [(c, (k + 1) % sides, k) for k in range(sides)]
            if profile[-1][1] > 0:
                pts.append(base + axis * profile[-1][0])
                c = len(pts) - 1
                o = (n - 1) * sides
                faces += [(c, o + k, o + (k + 1) % sides) for k in range(sides)]
        return self.add(pts, faces, paint, sway, smooth)

    def tube(self, path, radii, paint, sides=8, sway=None, smooth=True):
        """A bent tube along `path` (points), its radius at each."""
        path = [Vector(p) for p in path]
        pts, faces = [], []
        prev_u = None
        for i, p in enumerate(path):
            d = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized()
            u = d.cross(Z if abs(d.z) < 0.9 else X).normalized() if prev_u is None else (prev_u - d * prev_u.dot(d)).normalized()
            prev_u = u
            w = d.cross(u)
            for k in range(sides):
                t = 2 * math.pi * k / sides
                pts.append(p + (u * math.cos(t) + w * math.sin(t)) * radii[i])
        for i in range(len(path) - 1):
            for k in range(sides):
                a0, a1 = i * sides + k, i * sides + (k + 1) % sides
                faces.append((a0, a1, a1 + sides, a0 + sides))
        pts.append(path[-1] + (path[-1] - path[-2]).normalized() * radii[-1] * 0.5)
        c = len(pts) - 1
        o = (len(path) - 1) * sides
        faces += [(c, o + k, o + (k + 1) % sides) for k in range(sides)]
        return self.add(pts, faces, paint, sway, smooth)

    def box(self, size, at, paint, turn=None, sway=None, bevel=0.0, smooth=False):
        """A box `size` (x, y, z) standing with its foot's middle at `at` (turned by `turn`, a 3x3)."""
        sx, sy, sz = (s / 2 for s in size[:2]), None, size[2]
        hx, hy = size[0] / 2, size[1] / 2
        b = bevel
        if b <= 0:
            pts = [(x, y, z) for z in (0.0, sz) for y in (-hy, hy) for x in (-hx, hx)]
            faces = [(0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)]
        else:                                           # (a chamfered box: its edges cut back, so it catches the light)
            pts = []
            for z, ins in ((0.0, b), (b, 0.0), (sz - b, 0.0), (sz, b)):
                for x, y in ((-hx + b, -hy), (hx - b, -hy), (hx, -hy + b), (hx, hy - b), (hx - b, hy), (-hx + b, hy), (-hx, hy - b), (-hx, -hy + b)):
                    if ins:
                        x = max(-hx + b, min(hx - b, x))
                        y = max(-hy + b, min(hy - b, y))
                    pts.append((x, y, z))
            faces = [tuple(range(7, -1, -1)), tuple(range(24, 32))]
            for r in range(3):
                for k in range(8):
                    a0, a1 = r * 8 + k, r * 8 + (k + 1) % 8
                    faces.append((a0, a1, a1 + 8, a0 + 8))
        M = turn or Matrix.Identity(3)
        at = Vector(at)
        return self.add([at + M @ Vector(p) for p in pts], faces, paint, sway, smooth)

    def rock(self, at, r, paint, seed, squash=0.6, sharp=0.35, sub=None, sway=None, bury=0.25, yaw=None):
        """An angular stone: a rough ball pushed about by noise and cut flat on its sides, half sunk in the ground."""
        rng = random.Random(seed)
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=sub if sub is not None else self.n(2, 1), radius=1.0)
        yaw = rng.uniform(0, 2 * math.pi) if yaw is None else yaw
        cuts = [Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.3, 1))).normalized() for _ in range(5)]
        pts = []
        for v in bm.verts:
            p = v.co.copy()
            p *= 1.0 + 0.32 * noise.noise(p * 1.3 + Vector((seed * 3.1, seed * 1.7, 0)))
            for c in cuts:                               # (flats where it split along its bedding)
                d = p.dot(c) - (1.0 - sharp * 0.8)
                if d > 0:
                    p -= c * d * 0.85
            p.z *= squash
            p = Matrix.Rotation(yaw, 3, "Z") @ p
            pts.append(Vector(at) + p * r - Z * r * squash * bury)
        faces = [[v.index for v in f.verts] for f in bm.faces]
        bm.free()
        return self.add(pts, faces, paint, sway, smooth=False)

    def blob(self, at, r, paint, seed, squash=1.0, rough=0.35, sub=None, sway=None):
        """A soft lumpy ball: a clump of leaves seen whole."""
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=sub if sub is not None else self.n(2, 1), radius=1.0)
        pts = []
        for v in bm.verts:
            p = v.co.copy()
            p *= 1.0 + rough * noise.noise(p * 2.1 + Vector((seed * 5.3, seed * 2.9, seed)))
            p.z *= squash
            pts.append(Vector(at) + p * r)
        faces = [[v.index for v in f.verts] for f in bm.faces]
        bm.free()
        return self.add(pts, faces, paint, sway, smooth=True)

    def leaf_cards(self, centre, r, count, size, paint, seed, squash=1.0, sway=None, toward_out=True):
        """Small leaf-shaped cards scattered through a crown, each facing out from its middle: up close a crown
        reads as leaves, not as a ball."""
        rng = random.Random(seed)
        c = Vector(centre)
        pts, faces = [], []
        for i in range(count):
            d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))).normalized()
            p = c + Vector((d.x, d.y, d.z * squash)) * r * (0.55 + 0.45 * rng.random() ** 0.5)
            n = (d + Vector((rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 0.3))).normalized()
            u = n.cross(Z if abs(n.z) < 0.9 else X).normalized()
            w = n.cross(u)
            ang = rng.uniform(0, math.pi)
            a = (u * math.cos(ang) + w * math.sin(ang)) * size * rng.uniform(0.7, 1.3)
            b = (u * -math.sin(ang) + w * math.cos(ang)) * size * 0.35
            k = len(pts)
            pts += [p - a, p - b * 0.2, p + a, p + b]
            pts[k + 1] = p + b * -1.0
            faces.append((k, k + 1, k + 2, k + 3))
        return self.add(pts, faces, paint, sway, smooth=False)

    # ---- out -------------------------------------------------------------------------------------------------
    def object(self, look=True):
        """The prop as a Blender object, a material for each paint (for pictures, a little variation in each)."""
        me = bpy.data.meshes.new(self.name)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(me)
        ob = bpy.data.objects.new(self.name, me)
        for p in self.paints:
            ob.data.materials.append(material(p, look))
        return ob


_MATS = {}


def material(paint, look=True):
    """One material per paint, shared. With `look`, a soft mottle so a wall or a rock is never a flat sheet."""
    key = (paint, look)
    try:
        if key in _MATS and _MATS[key].name in bpy.data.materials:
            return _MATS[key]
    except ReferenceError:                                   # (kit.fresh() emptied the scene)
        _MATS.clear()
    m = bpy.data.materials.new(f"med.{paint}")
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    col = lin(PAINTS[paint])
    b.inputs["Roughness"].default_value = 0.92
    leafy = paint.startswith(("thorn", "palm_frond", "olive_leaf", "cypress", "palm_dry"))
    if look:
        tc = nt.nodes.new("ShaderNodeTexCoord")
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 1.6 if not leafy else 6.0
        nz.inputs["Detail"].default_value = 8.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        info = nt.nodes.new("ShaderNodeObjectInfo")
        rmp = nt.nodes.new("ShaderNodeValToRGB")
        lo = tuple(c * (0.80 if not leafy else 0.70) for c in col)
        hi = tuple(min(1.0, c * (1.12 if not leafy else 1.25)) for c in col)
        rmp.color_ramp.elements[0].position, rmp.color_ramp.elements[0].color = 0.32, (*lo, 1)
        rmp.color_ramp.elements[1].position, rmp.color_ramp.elements[1].color = 0.70, (*hi, 1)
        add = nt.nodes.new("ShaderNodeMath")
        add.operation = "ADD"
        nt.links.new(nz.outputs["Fac"], add.inputs[0])
        mul = nt.nodes.new("ShaderNodeMath")
        mul.operation = "MULTIPLY"
        nt.links.new(info.outputs["Random"], mul.inputs[0])
        mul.inputs[1].default_value = 0.22
        nt.links.new(mul.outputs[0], add.inputs[1])
        sub = nt.nodes.new("ShaderNodeMath")
        sub.operation = "SUBTRACT"
        nt.links.new(add.outputs[0], sub.inputs[0])
        sub.inputs[1].default_value = 0.11
        nt.links.new(sub.outputs[0], rmp.inputs["Fac"])
        nt.links.new(rmp.outputs["Color"], b.inputs["Base Color"])
        if leafy:
            b.inputs["Subsurface Weight"].default_value = 0.0
            try:
                b.inputs["Transmission Weight"].default_value = 0.0
            except KeyError:
                pass
    else:
        b.inputs["Base Color"].default_value = (*col, 1)
    if paint == "dark":
        b.inputs["Roughness"].default_value = 1.0
    _MATS[key] = m
    return m


# ---- the models ----------------------------------------------------------------------------------------------
def sway_by_height(top, foot=0.0, power=1.6):
    return lambda p: max(0.0, min(1.0, (p.z - foot) / max(top - foot, 1e-3))) ** power


def stones(fine=True, seed=11):
    """A scatter of loose stones on the gravel plain: five to nine, from a fist to a knee high, half sunk."""
    pr = Prop("Stones", "Loose stones of the stony desert", fine, seed)
    rng = pr.rng
    for i in range(pr.n(9, 5)):
        r = rng.uniform(0.07, 0.26) if i else 0.34
        a, d = rng.uniform(0, 2 * math.pi), rng.uniform(0.0, 1.3) if i else 0.0
        pr.rock((math.cos(a) * d, math.sin(a) * d, 0.0), r, rng.choice(("rock_dark", "rock_dark", "rock", "rock_pale")), seed * 31 + i, squash=rng.uniform(0.45, 0.75), sub=pr.n(2 if r > 0.2 else 1, 0))
    pr.info.update(foot_m=1.6, high_m=0.3)
    return pr


def boulder(fine=True, seed=5):
    """A broken boulder of the escarpments and the rough ground, about 1.6 m across, with a few pieces at its foot."""
    pr = Prop("Boulder", "A boulder and its fallen pieces", fine, seed)
    pr.rock((0, 0, 0), 0.95, "rock", seed, squash=0.7, sharp=0.5, sub=pr.n(3, 1), bury=0.18)
    rng = pr.rng
    for i in range(pr.n(5, 2)):
        a = rng.uniform(0, 2 * math.pi)
        pr.rock((math.cos(a) * 1.15, math.sin(a) * 1.15, 0), rng.uniform(0.12, 0.3), rng.choice(("rock", "rock_dark")), seed * 7 + i, squash=0.6, sub=pr.n(1, 0))
    pr.info.update(foot_m=1.4, high_m=1.3)
    return pr


def camel_thorn(fine=True, seed=3):
    """Camel-thorn scrub: a low rounded clump of thorny twigs and small grey-green leaves, 0.5 to 0.8 m high, a
    little over a metre across, often on a small hummock of blown sand caught round its foot."""
    pr = Prop("CamelThorn", "Camel-thorn scrub on its hummock", fine, seed)
    rng = pr.rng
    hi = 0.7
    sw = sway_by_height(hi + 0.2, 0.05, 1.3)
    pr.lathe((0, 0, -0.05), Z, [(0.0, 0.8), (0.07, 0.62), (0.15, 0.34), (0.18, 0.0)], "sand_blown", pr.n(16, 8), wobble=0.18, seed=seed)       # the hummock
    clumps = pr.n(16, 6)
    for i in range(clumps):
        a, d = rng.uniform(0, 2 * math.pi), rng.uniform(0.0, 0.42)
        h = hi * (1.0 - (d / 0.6) ** 2) * rng.uniform(0.75, 1.05)
        r = rng.uniform(0.13, 0.22)
        pr.blob((math.cos(a) * d, math.sin(a) * d, 0.12 + h * 0.55), r, "thorn" if i % 3 else "thorn_dark", seed * 13 + i, squash=0.75, rough=0.55, sub=pr.n(1, 0), sway=sw)
    if fine:
        pr.leaf_cards((0, 0, 0.42), 0.55, 260, 0.045, "thorn", seed, squash=0.62, sway=sw)
        for i in range(14):                              # bare twigs standing out of the clump
            a = rng.uniform(0, 2 * math.pi)
            d0, d1 = rng.uniform(0.05, 0.25), rng.uniform(0.45, 0.7)
            z1 = rng.uniform(0.35, 0.85)
            pr.tube([(math.cos(a) * d0, math.sin(a) * d0, 0.12), (math.cos(a + 0.2) * d1 * 0.6, math.sin(a + 0.2) * d1 * 0.6, z1 * 0.7), (math.cos(a + 0.3) * d1, math.sin(a + 0.3) * d1, z1)],
                    [0.012, 0.008, 0.003], "twig", 5, sway=sw)
    pr.info.update(foot_m=0.95, high_m=0.85)
    return pr


def date_palm(fine=True, seed=7, high=9.0):
    """A date palm: a rough trunk leaning a little and bowed, its old leaf bases in a close diamond pattern, a crown
    of long arching fronds (each a stalk with stiff leaflets down both sides), the dead ones hanging grey below."""
    pr = Prop("DatePalm", "Date palm", fine, seed)
    rng = pr.rng
    lean = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized() * rng.uniform(0.04, 0.08)
    n = pr.n(14, 6)
    path = []
    for i in range(n + 1):
        t = i / n
        path.append(Vector((0, 0, high * t)) + lean * high * (t ** 1.4) * 1.0 + Vector((0.12 * math.sin(t * 3.0 + seed), 0.1 * math.cos(t * 2.2 + seed), 0)) * t)
    radii = [0.30 - 0.08 * (i / n) + (0.05 if i == 0 else 0.0) for i in range(n + 1)]
    sw_trunk = lambda p: 0.25 * max(0.0, p.z / high) ** 2        # noqa: E731
    pr.tube(path, radii, "palm_trunk", pr.n(14, 6), sway=sw_trunk)
    if fine:                                             # the old leaf bases: knobs set in a close spiral
        for k in range(int(high * 9)):
            t = k / (high * 9)
            i = min(int(t * n), n - 1)
            p = path[i].lerp(path[i + 1], t * n - i)
            ang = k * 2.39996
            d = Vector((math.cos(ang), math.sin(ang), 0))
            r = radii[i] * 0.98
            pr.box((0.10, 0.16, 0.07), p + d * r - Z * 0.03, "palm_trunk", turn=Matrix.Rotation(ang, 3, "Z") @ Matrix.Rotation(-0.5, 3, "Y"), sway=sw_trunk(p))
    top = path[-1]
    sw = lambda p: min(1.0, 0.25 + 0.75 * ((p - top).length / 4.2) ** 1.2)      # noqa: E731
    fronds = pr.n(24, 10)
    for k in range(fronds):
        ang = k * 2.39996 + rng.uniform(-0.1, 0.1)
        dead = k >= fronds - pr.n(7, 3)
        rise = rng.uniform(0.55, 1.15) if not dead else rng.uniform(-1.35, -0.95)
        L = rng.uniform(3.4, 4.4) if not dead else rng.uniform(2.6, 3.4)
        d = Vector((math.cos(ang), math.sin(ang), 0))
        pts = []
        steps = pr.n(10, 4)
        for i in range(steps + 1):
            t = i / steps
            sag = (t ** 2) * (1.9 if not dead else 0.3)
            pts.append(top + d * L * t * math.cos(rise * (1 - t * 0.5)) + Z * (L * t * math.sin(rise) * (1 - t * 0.35) - sag))
        paint = "palm_dry" if dead else "palm_frond"
        pr.tube(pts, [0.035 * (1 - 0.8 * i / steps) + 0.004 for i in range(steps + 1)], paint, pr.n(5, 3), sway=sw)
        side = d.cross(Z).normalized()
        if fine:                                         # the leaflets: stiff, narrow, angled forward and up in a V
            for i in range(2, steps * 4):
                t = i / (steps * 4)
                j = min(int(t * steps), steps - 1)
                p = pts[j].lerp(pts[j + 1], t * steps - j)
                along = (pts[j + 1] - pts[j]).normalized()
                ll = 0.62 * math.sin(math.pi * min(1.0, t * 1.15)) + 0.12
                for s in (1, -1):
                    out = (side * s + along * 0.55 + Z * (0.30 if not dead else -0.5)).normalized()
                    tip = p + out * ll
                    w = along * 0.03
                    vs, _ = pr.add([p - w, p + w, tip + w * 0.2, tip - w * 0.2], [(0, 1, 2, 3)], paint, sway=sw, smooth=False)
        else:                                            # (the plain frond: one strip on each side)
            q = [p + side * 0.35 * math.sin(math.pi * i / steps) for i, p in enumerate(pts)]
            r = [p - side * 0.35 * math.sin(math.pi * i / steps) for i, p in enumerate(pts)]
            m = len(pts)
            pr.add(q + pts + r, [(i, i + 1, m + i + 1, m + i) for i in range(m - 1)] + [(m + i, m + i + 1, 2 * m + i + 1, 2 * m + i) for i in range(m - 1)], paint, sway=sw, smooth=False)
    if fine:                                             # date clusters hanging under the crown
        for k in range(4):
            ang = rng.uniform(0, 2 * math.pi)
            pr.blob(top + Vector((math.cos(ang) * 0.42, math.sin(ang) * 0.42, -0.55)), 0.2, "dates", seed + k, squash=1.6, rough=0.5, sub=1, sway=0.3)
    pr.info.update(foot_m=0.4, high_m=round(high + 2.4, 1))
    return pr


def olive_tree(fine=True, seed=9):
    """An old olive tree: a short gnarled trunk splitting into three or four twisted limbs, a broad rounded crown of
    small silver-grey leaves, 4 to 5 m high. The groves of Italy, Sicily, Greece and the Libyan coast."""
    pr = Prop("OliveTree", "Olive tree", fine, seed)
    rng = pr.rng
    sw_w = lambda p: 0.08 * max(0.0, p.z / 4.5) ** 2        # noqa: E731
    pr.lathe((0, 0, -0.1), Z, [(0.0, 0.42), (0.25, 0.34), (0.9, 0.30), (1.2, 0.34)], "olive_bark", pr.n(12, 6), wobble=0.35, seed=seed, sway=sw_w)
    crowns = []
    for k in range(pr.n(4, 3)):
        ang = k * 2.2 + rng.uniform(-0.3, 0.3)
        d = Vector((math.cos(ang), math.sin(ang), 0))
        a = Vector((0, 0, 1.1))
        b = a + d * rng.uniform(0.5, 0.8) + Z * rng.uniform(0.7, 1.0)
        c = b + d * rng.uniform(0.5, 0.9) + Z * rng.uniform(0.6, 1.0) + d.cross(Z) * rng.uniform(-0.3, 0.3)
        pr.tube([a, a.lerp(b, 0.5) + d.cross(Z) * 0.08, b, c], [0.17, 0.14, 0.11, 0.06], "olive_bark", pr.n(8, 4), sway=sw_w)
        crowns.append(c)
    swc = lambda p: min(1.0, 0.2 + 0.5 * max(0.0, (p.z - 2.0) / 3.0))         # noqa: E731
    # the crown: many small loose clumps, not a few big balls, so light comes through it and the sky shows in gaps
    for i, c in enumerate(crowns):
        for j in range(pr.n(9, 2)):
            off = Vector((rng.gauss(0, 0.55), rng.gauss(0, 0.55), rng.uniform(-0.25, 0.55)))
            r = rng.uniform(0.3, 0.48) if fine else rng.uniform(0.85, 1.1)
            pr.blob(c + off, r, "olive_leaf" if (i + j) % 3 else "olive_leaf2", seed * 17 + i * 11 + j, squash=0.62, rough=0.3, sub=pr.n(2, 1), sway=swc)
        if fine:                                         # and leaves standing out of every clump's skin
            pr.leaf_cards(c + Z * 0.15, 1.05, 760, 0.12, "olive_leaf" if i % 2 else "olive_leaf2", seed * 3 + i, squash=0.62, sway=swc)
    pr.info.update(foot_m=0.6, high_m=4.6)
    return pr


def cypress(fine=True, seed=4, high=12.0):
    """A cypress: a tall dark flame of a tree, the lines of them along a farm's lane and round a cemetery."""
    pr = Prop("Cypress", "Cypress", fine, seed)
    sw = sway_by_height(high, 0.0, 2.0)
    pr.lathe((0, 0, 0), Z, [(0.0, 0.16), (0.6, 0.14)], "olive_bark", 8, sway=0.0)
    prof = []
    steps = pr.n(18, 7)
    for i in range(steps + 1):
        t = i / steps
        prof.append((0.5 + t * (high - 0.5), 0.95 * (math.sin(math.pi * min(1.0, t * 1.05 + 0.02)) ** 0.8) * (1 - t) ** 0.35 + 0.02))
    pr.lathe((0, 0, 0), Z, prof, "cypress", pr.n(18, 8), wobble=0.22, seed=seed, sway=sw)
    if fine:                                             # sprays of needles all over the column: a dark plume, not a pole
        rng = pr.rng
        pts, faces, sws = [], [], []
        for k in range(900):
            t = rng.uniform(0.02, 0.97)
            ang = rng.uniform(0, 2 * math.pi)
            r = 0.95 * (math.sin(math.pi * min(1.0, t * 1.05 + 0.02)) ** 0.8) * (1 - t) ** 0.35 + 0.02
            out = Vector((math.cos(ang), math.sin(ang), 0.0))
            p = out * r * rng.uniform(0.92, 1.12) + Z * (0.5 + t * (high - 0.5))
            n = (out + Z * rng.uniform(0.2, 0.9)).normalized()      # (sprays point up and out, as the tree grows)
            u = out.cross(Z).normalized()
            ln = rng.uniform(0.28, 0.45)
            a, b = n * ln, u * ln * 0.32
            q = p - out * 0.05
            k0 = len(pts)
            pts += [q, q + a * 0.5 - b, q + a, q + a * 0.5 + b]
            faces.append((k0, k0 + 1, k0 + 2, k0 + 3))
        pr.add(pts, faces, "cypress2", sw, smooth=False)
    pr.info.update(foot_m=0.9, high_m=high)
    return pr


def _house_block(pr, at, size, wall, roof, seed, door=None, windows=2, parapet=0.35, spouts=True, lean=0.03):
    """A flat-roofed block of mud brick or rough stone: walls a little out of true and thicker at the foot, a low
    parapet round the roof, small dark windows, a door, wooden spouts to throw the rain off the roof."""
    rng = random.Random(seed)
    x0, y0 = at[0], at[1]
    sx, sy, sz = size
    hx, hy = sx / 2, sy / 2
    # the walls: a box whose top is drawn in a little (batter) and whose edges are rounded by hand
    b = 0.08
    foot = [(-hx, -hy + b), (-hx + b, -hy), (hx - b, -hy), (hx, -hy + b), (hx, hy - b), (hx - b, hy), (-hx + b, hy), (-hx, hy - b)]
    top = [(x * (1 - lean * 2 / sx * 2), y * (1 - lean * 2 / sy * 2)) for x, y in foot]
    pts = [(x0 + x, y0 + y, 0.0) for x, y in foot] + [(x0 + x, y0 + y, sz) for x, y in top]
    for i in range(8, 16):                              # (out of true: the top corners not quite level)
        pts[i] = (pts[i][0], pts[i][1], pts[i][2] + rng.uniform(-0.05, 0.05))
    faces = [tuple(range(7, -1, -1)), tuple(range(8, 16))] + [(k, (k + 1) % 8, (k + 1) % 8 + 8, k + 8) for k in range(8)]
    pr.add(pts, faces, wall, smooth=False)
    # the roof's face and its parapet
    rz = sz + 0.02
    tx, ty = hx * (1 - lean * 4 / sx) - 0.18, hy * (1 - lean * 4 / sy) - 0.18
    pr.box((tx * 2, ty * 2, 0.02), (x0, y0, sz - 0.02), roof)
    if parapet:
        for (cx, cy, lx, ly) in ((0, ty + 0.09, tx * 2 + 0.36, 0.18), (0, -ty - 0.09, tx * 2 + 0.36, 0.18), (tx + 0.09, 0, 0.18, ty * 2), (-tx - 0.09, 0, 0.18, ty * 2)):
            pr.box((lx, ly, parapet), (x0 + cx, y0 + cy, sz - 0.02), wall, bevel=0.04 if pr.fine else 0.0)
    if spouts and pr.fine:
        for k in range(2):
            y = rng.uniform(-ty * 0.8, ty * 0.8)
            pr.box((0.5, 0.09, 0.09), (x0 + hx * (1 - lean * 4 / sx) + 0.15, y0 + y, sz + 0.05), "wood_old")
    # the door and windows, dark, set a little into the wall
    face = rng.choice((1, -1)) if door is None else door
    if face:
        dx = rng.uniform(-hx * 0.5, hx * 0.5)
        pr.box((0.9, 0.06, 1.85), (x0 + dx, y0 - face * (hy + 0.005), 0.0), "dark")
        if pr.fine:
            pr.box((1.1, 0.12, 0.12), (x0 + dx, y0 - face * (hy + 0.02), 1.85), "wood_old")
    if pr.fine:
        for k in range(windows):
            wx = rng.uniform(-hx * 0.7, hx * 0.7)
            side = rng.choice((1, -1))
            pr.box((0.45, 0.06, 0.55), (x0 + wx, y0 + side * (hy + 0.002), sz * 0.55), "dark")
        if rng.random() < 0.6:
            wy = rng.uniform(-hy * 0.5, hy * 0.5)
            pr.box((0.06, 0.45, 0.5), (x0 + hx + 0.002, y0 + wy, sz * 0.55), "dark")


def flat_roof_house(fine=True, seed=21, stone=False, two=False):
    """The flat-roofed town's house (world-look-brief 3.4): mud brick (or pale stone), a block of one storey with a
    smaller one against it, or of two storeys; the roof a shade lighter than the walls; a yard wall."""
    name = ("FlatRoofHouseStone" if stone else "FlatRoofHouse") + ("Two" if two else "")
    title = "Flat-roofed house" + (" of pale stone" if stone else " of mud brick") + (", two storeys" if two else "")
    pr = Prop(name, title, fine, seed)
    rng = pr.rng
    wall, roof = ("stone_pale", "stone_roof") if stone else ("mud", "mud_roof")
    w, d = rng.uniform(6.0, 7.5), rng.uniform(4.6, 5.6)
    h = 3.0 * (2 if two else 1) + rng.uniform(-0.2, 0.3)
    _house_block(pr, (0, 0), (w, d, h), wall, roof, seed, door=1, windows=3 if two else 2)
    w2, d2 = rng.uniform(3.0, 3.8), rng.uniform(3.0, 3.6)
    _house_block(pr, (w / 2 + w2 / 2 - 0.15, d / 2 - d2 / 2 - rng.uniform(0.2, 0.8)), (w2, d2, 2.6), wall, roof, seed + 1, door=0, windows=1, parapet=0.25)
    if not two:                                          # a yard wall round the front
        yw = rng.uniform(5.0, 6.0)
        for (cx, cy, lx, ly) in ((-w / 2 + 0.1, -d / 2 - yw / 2, 0.35, yw), (-w / 2 + 2.6, -d / 2 - yw + 0.17, 5.0, 0.35)):
            pr.box((lx, ly, 1.7), (cx, cy, 0.0), wall, bevel=0.06 if fine else 0.0)
        if fine:                                         # a wooden gate in it, and steps up to the roof
            pr.box((1.4, 0.08, 1.5), (-w / 2 + 5.8, -d / 2 - yw + 0.17, 0.0), "wood")
    if fine:
        for k in range(int(h / 0.25) - 1):               # outside stair along one end
            pr.box((0.85, 0.32, 0.25 * (k + 1)), (-w / 2 - 0.43, d / 2 - 0.3 - k * 0.32, 0.0), wall)
    pr.info.update(foot_m=round(max(w, d) * 0.75, 1), high_m=round(h + 0.4, 1))
    return pr


def dome_house(fine=True, seed=31):
    """The flat-roofed town's one dome: a square house of pale stone carrying a white dome on a low drum."""
    pr = Prop("DomeHouse", "The domed house", fine, seed)
    s, h = 7.0, 4.2
    _house_block(pr, (0, 0), (s, s, h), "stone_pale", "stone_roof", seed, door=1, windows=2, parapet=0.3)
    pr.lathe((0, 0, h - 0.05), Z, [(0.0, 2.25), (0.7, 2.25), (0.75, 2.3), (0.8, 2.3)], "stone_pale", pr.n(40, 16), close=False)
    prof = [(0.8 + 2.3 * math.sin(math.radians(a)) * 1.05, 2.25 * math.cos(math.radians(a))) for a in range(0, 91, pr.n(6, 15))]
    prof[-1] = (prof[-1][0], 0.0)
    pr.lathe((0, 0, h - 0.05), Z, prof, "dome", pr.n(40, 16))
    pr.lathe((0, 0, h + 3.25), Z, [(0.0, 0.08), (0.6, 0.03)], "cap", 8)
    if fine:
        for k in range(8):
            a = 2 * math.pi * (k + 0.5) / 8
            pr.box((0.12, 0.35, 0.45), (math.cos(a) * 2.27, math.sin(a) * 2.27, h + 0.15), "dark", turn=Matrix.Rotation(a, 3, "Z"))
    pr.info.update(foot_m=5.0, high_m=round(h + 3.9, 1))
    return pr


def slim_tower(fine=True, seed=41, high=17.0):
    """The flat-roofed town's slim tower, of our own design: a square foot of pale stone, a round shaft, a gallery
    on brackets near the top, a narrow lantern and a pointed grey-green cap."""
    pr = Prop("SlimTower", "The slim tower", fine, seed)
    sides = pr.n(20, 10)
    pr.box((3.0, 3.0, 4.0), (0, 0, 0), "stone_pale", bevel=0.06 if fine else 0.0)
    pr.box((3.2, 3.2, 0.3), (0, 0, 4.0), "stone_roof")
    pr.lathe((0, 0, 4.3), Z, [(0.0, 1.05), (high - 6.0, 0.9)], "stone_pale", sides, close=False)
    g = high - 1.7
    pr.lathe((0, 0, g), Z, [(-0.5, 0.9), (0.0, 1.45), (0.18, 1.45), (0.18, 0.0)], "stone_roof", sides)
    if fine:
        for k in range(sides):
            a = 2 * math.pi * k / sides
            pr.box((0.08, 0.08, 0.8), (math.cos(a) * 1.38, math.sin(a) * 1.38, g + 0.18), "stone_pale")
        pr.lathe((0, 0, g + 0.98), Z, [(0.0, 1.5), (0.08, 1.5)], "stone_roof", sides)
        for k in range(4):
            a = math.pi / 2 * k
            pr.box((0.35, 0.08, 1.2), (math.cos(a) * 0.62, math.sin(a) * 0.62, high - 6.0 + 1.5), "dark", turn=Matrix.Rotation(a + math.pi / 2, 3, "Z"))
    pr.lathe((0, 0, g + 0.18), Z, [(0.0, 0.6), (1.9, 0.55)], "stone_pale", sides, close=False)
    cap = [(g + 2.08, 0.72), (g + 2.5, 0.68), (g + 3.5, 0.32), (g + 4.1, 0.04)]
    pr.lathe((0, 0, 0), Z, cap, "cap", sides)
    pr.lathe((0, 0, g + 4.0), Z, [(0.0, 0.03), (0.7, 0.02)], "iron", 6)
    pr.info.update(foot_m=2.2, high_m=round(g + 4.7, 1))
    return pr


def farmhouse(fine=True, seed=51):
    """A farmhouse of the Italian south (and Sicily and Greece): two storeys of stone under worn ochre plaster, a low
    roof of red tile, small shuttered windows, an outside stair to the upper floor, a lean-to on one end."""
    pr = Prop("Farmhouse", "Farmhouse of the south", fine, seed)
    rng = pr.rng
    w, d, h = 10.0, 7.0, 6.2
    _house_block(pr, (0, 0), (w, d, h), "plaster", "plaster", seed, door=1, windows=0, parapet=0.0, spouts=False, lean=0.02)
    # the roof: four low slopes of red tile with a little overhang, the ridge along the house
    rz, o, rise = h + 0.02, 0.35, 1.5
    hx, hy = w / 2 + o, d / 2 + o
    ridge = (hx - hy, 0.0)
    pts = [(-hx, -hy, rz), (hx, -hy, rz), (hx, hy, rz), (-hx, hy, rz), (-ridge[0], 0, rz + rise), (ridge[0], 0, rz + rise)]
    faces = [(0, 1, 5, 4), (2, 3, 4, 5), (1, 2, 5), (3, 0, 4)]
    pr.add(pts, faces, "tile", smooth=False)
    pr.add([(x, y, z - 0.08) for x, y, z in pts], [tuple(reversed(f)) for f in faces], "tile2", smooth=False)
    if fine:                                             # the rows of tile, a rib at every course
        for k in range(1, 9):
            t = k / 9
            for s in (1, -1):
                y = s * hy * (1 - t)
                pr.box((2 * (hx - (hx - ridge[0]) * t), 0.06, 0.05), (0, y, rz + rise * t), "tile2")
        pr.box((2 * ridge[0] + 0.3, 0.22, 0.12), (0, 0, rz + rise), "tile2")
        for k in range(4):                               # the windows with their green shutters folded back
            x = -w / 2 + 1.6 + k * 2.3
            for z in (1.3, 4.1):
                if k == 1 and z < 2:
                    continue
                for s in (1, -1):
                    pr.box((0.62, 0.06, 0.9), (x, s * (d / 2 + 0.005), z), "dark")
                    pr.box((0.34, 0.05, 0.92), (x - 0.5, s * (d / 2 + 0.03), z), "shutter")
                    pr.box((0.34, 0.05, 0.92), (x + 0.5, s * (d / 2 + 0.03), z), "shutter")
        for k in range(12):                              # the outside stair up to the upper door
            pr.box((0.42, 1.0, 0.27 * (k + 1)), (w / 2 - 0.4 - k * 0.34, -(d / 2 + 0.55), 0.0), "plaster2")
        pr.box((1.0, 1.4, 0.3), (w / 2 - 4.6, -(d / 2 + 0.55), 3.0), "plaster2")
        pr.box((0.9, 0.06, 1.9), (w / 2 - 4.6, -(d / 2 + 0.005), 3.3), "wood")
        for k in range(9):                               # patches where the plaster has fallen, showing the stone
            pr.box((rng.uniform(0.6, 1.4), 0.03, rng.uniform(0.4, 0.9)), (rng.uniform(-w / 2 + 0.8, w / 2 - 0.8), rng.choice((1, -1)) * (d / 2 + 0.01), rng.uniform(0.3, 5.0)), "fieldstone2")
    # the lean-to on the end: a stone shed under a single slope of tile
    lx = -w / 2 - 2.0
    pr.box((4.0, 5.6, 2.6), (lx, 0.0, 0.0), "fieldstone", bevel=0.05 if fine else 0.0)
    pr.add([(lx - 2.3, -3.1, 2.55), (lx + 2.0, -3.1, 3.4), (lx + 2.0, 3.1, 3.4), (lx - 2.3, 3.1, 2.55)], [(0, 1, 2, 3)], "tile", smooth=False)
    pr.add([(lx - 2.3, -3.1, 2.48), (lx + 2.0, -3.1, 3.33), (lx + 2.0, 3.1, 3.33), (lx - 2.3, 3.1, 2.48)], [(3, 2, 1, 0)], "tile2", smooth=False)
    pr.info.update(foot_m=7.5, high_m=round(h + rise + 0.2, 1))
    return pr


def stone_wall(fine=True, seed=61, long=6.0, high=1.1):
    """A dry-stone wall 6 m long: field stones laid without mortar, wider at the foot, a row of flat stones on top.
    The terraces and field walls of Italy and Greece, and the desert's sangars are built the same way."""
    pr = Prop("StoneWall", "Dry-stone wall", fine, seed)
    rng = pr.rng
    core = [(-long / 2, -0.38, 0.0), (long / 2, -0.38, 0.0), (long / 2, 0.38, 0.0), (-long / 2, 0.38, 0.0),
            (-long / 2, -0.24, high), (long / 2, -0.24, high), (long / 2, 0.24, high), (-long / 2, 0.24, high)]
    pr.add(core, [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (2, 3, 7, 6), (1, 2, 6, 5), (3, 0, 4, 7)], "fieldstone2", smooth=False)
    if fine:
        rows = int(high / 0.22)
        for r in range(rows):
            z = r * 0.22 + 0.02
            wy = 0.38 - (0.14 * z / high)
            x = -long / 2 + rng.uniform(0, 0.2)
            while x < long / 2 - 0.15:
                l = rng.uniform(0.25, 0.5)
                for s in (1, -1):
                    pr.rock((min(x + l / 2, long / 2 - 0.12), s * wy, z + 0.1), l * 0.55, rng.choice(("fieldstone", "fieldstone", "fieldstone2")), rng.randrange(10000), squash=0.55, sharp=0.6, sub=1, bury=0.0)
                x += l * 0.95
        x = -long / 2 + 0.2
        while x < long / 2 - 0.2:                         # the copers
            l = rng.uniform(0.35, 0.6)
            pr.rock((x + l / 2, 0.0, high + 0.05), l * 0.55, "fieldstone", rng.randrange(10000), squash=0.4, sharp=0.6, sub=1, bury=0.0)
            x += l
    pr.info.update(foot_m=round(long / 2, 1), high_m=round(high + 0.2, 1), long_m=long, joins="end to end along x: lay walls 6 m apart on a line")
    return pr


def sangar(fine=True, seed=71):
    """A sangar: where the ground is rock and cannot be dug, a low wall of piled stones in a horseshoe, open at the
    back, for a machine gun's crew or a section (about 4 m across, 0.8 m high), sand blown against its foot. The
    desert war's own fieldwork, laid like the dry-stone wall."""
    pr = Prop("Sangar", "Sangar: a horseshoe of piled stones", fine, seed)
    rng = pr.rng
    r0, high, arc = 1.9, 0.8, math.radians(125)
    seg = pr.n(28, 10)
    # the core: a curved band of rubble, wider at the foot
    ring = []
    for i in range(seg + 1):
        a = -arc + 2 * arc * i / seg
        c, s_ = math.cos(a), math.sin(a)
        h = high * (1 - 0.35 * max(0.0, abs(a) / arc - 0.75) / 0.25)     # (lower at the two ends)
        for rr, z in ((r0 - 0.36, 0.0), (r0 + 0.36, 0.0), (r0 + 0.2, h), (r0 - 0.2, h)):
            ring.append((c * rr, s_ * rr, z))
    faces = []
    for i in range(seg):
        a0, a1 = i * 4, (i + 1) * 4
        for k in range(4):
            faces.append((a0 + k, a1 + k, a1 + (k + 1) % 4, a0 + (k + 1) % 4))
    faces += [(0, 3, 2, 1), (seg * 4, seg * 4 + 1, seg * 4 + 2, seg * 4 + 3)]
    pr.add(ring, faces, "rock_dark", smooth=False)
    if fine:                                             # the faces: stones in courses, inside and out, and a row on top
        for row in range(4):
            z = 0.04 + row * 0.19
            for side in (-1, 1):
                rr = r0 + side * (0.36 - 0.16 * z / high)
                a = -arc + rng.uniform(0, 0.08)
                while a < arc - 0.05:
                    l = rng.uniform(0.24, 0.42)
                    pr.rock((math.cos(a) * rr, math.sin(a) * rr, z + 0.08), l * 0.55, rng.choice(("rock", "rock", "rock_dark", "rock_pale")),
                            rng.randrange(100000), squash=0.55, sharp=0.6, sub=1, bury=0.0)
                    a += l * 0.95 / rr
        a = -arc + 0.05
        while a < arc - 0.05:
            l = rng.uniform(0.3, 0.5)
            pr.rock((math.cos(a) * r0, math.sin(a) * r0, high + 0.02), l * 0.55, rng.choice(("rock", "rock_pale")), rng.randrange(100000), squash=0.45, sharp=0.6, sub=1, bury=0.0)
            a += l / r0
        for k in range(14):                              # a few fallen ones round about
            a = rng.uniform(-arc, arc)
            rr = r0 + rng.choice((-1, 1)) * rng.uniform(0.6, 1.1)
            pr.rock((math.cos(a) * rr, math.sin(a) * rr, 0.0), rng.uniform(0.1, 0.2), "rock", rng.randrange(100000), squash=0.6, sharp=0.5, sub=1)
    # sand drifted against the outside of the wall
    sand = []
    for i in range(seg + 1):
        a = -arc + 2 * arc * i / seg
        c, s_ = math.cos(a), math.sin(a)
        taper = min(1.0, (arc - abs(a)) / 0.5)             # (the drift thins out at the two ends)
        for rr, z in ((r0 + 0.3, 0.06 + 0.22 * taper), (r0 + 0.35 + 0.5 * taper, 0.0)):
            sand.append((c * rr, s_ * rr, z - 0.02))
    pr.add(sand, [(i * 2, i * 2 + 1, i * 2 + 3, i * 2 + 2) for i in range(seg)], "sand", smooth=True)
    pr.info.update(foot_m=3.0, high_m=0.9, opens="to -x: the back of the horseshoe is open; turn it to face the enemy's way")
    return pr


def telegraph_pole(fine=True, seed=81):
    """A telegraph pole of the coast road: a wooden pole 7 m high, one cross-arm, four insulators."""
    pr = Prop("TelegraphPole", "Telegraph pole", fine, seed)
    pr.lathe((0, 0, -0.1), Z, [(0.0, 0.11), (7.1, 0.08)], "wood_old", pr.n(10, 6))
    pr.box((0.12, 1.5, 0.1), (0, 0, 6.6), "wood_old")
    if fine:
        for y in (-0.65, -0.22, 0.22, 0.65):
            pr.lathe((0, y, 6.7), Z, [(0.0, 0.012), (0.08, 0.01)], "iron", 6)
            pr.lathe((0, y, 6.78), Z, [(0.0, 0.035), (0.05, 0.04), (0.11, 0.025), (0.12, 0.0)], "insulator", 10)
    pr.info.update(foot_m=0.2, high_m=7.1, wires="between poles 50 to 60 m apart: four wires at 6.88 m, 0.65 and 0.22 m either side of the pole's middle, sagging about 0.6 m")
    return pr


def macchia(fine=True, seed=13):
    """Macchia: the evergreen scrub of the Mediterranean's dry hills (lentisk, myrtle, broom), a dark rounded clump
    knee to chest high, two metres across. Sicily, the toe of Italy, Greece, the Cyrenaican hills."""
    pr = Prop("Macchia", "Macchia scrub", fine, seed)
    rng = pr.rng
    sw = lambda p: min(1.0, max(0.0, p.z / 1.4)) * 0.5                     # noqa: E731
    for i in range(pr.n(6, 3)):
        a, d = rng.uniform(0, 2 * math.pi), rng.uniform(0.0, 0.7) if i else 0.0
        r = rng.uniform(0.38, 0.62) if fine else rng.uniform(0.5, 0.75)
        pr.blob((math.cos(a) * d, math.sin(a) * d, r * 0.55), r, "macchia" if i % 2 else "macchia2", seed * 7 + i, squash=0.72, rough=0.32, sub=pr.n(2, 1), sway=sw)
    if fine:
        pr.leaf_cards((0, 0, 0.45), 0.85, 360, 0.08, "macchia2", seed * 5, squash=0.55, sway=sw)
    pr.info.update(foot_m=1.1, high_m=1.2)
    return pr


MODELS = {
    "Stones": stones, "Macchia": macchia, "Boulder": boulder, "CamelThorn": camel_thorn, "DatePalm": date_palm, "OliveTree": olive_tree,
    "Cypress": cypress, "FlatRoofHouse": flat_roof_house, "FlatRoofHouseStone": lambda fine=True: flat_roof_house(fine, 23, stone=True),
    "FlatRoofHouseTwo": lambda fine=True: flat_roof_house(fine, 25, two=True), "DomeHouse": dome_house, "SlimTower": slim_tower,
    "Farmhouse": farmhouse, "StoneWall": stone_wall, "Sangar": sangar, "TelegraphPole": telegraph_pole,
}


# ---- the game's files ------------------------------------------------------------------------------------------
def gather(ob, paints):
    """The mesh as the game reads it: points with their facings (split where the shading is flat), paints, sway."""
    dg = bpy.context.evaluated_depsgraph_get()
    eo = ob.evaluated_get(dg)
    me = eo.to_mesh()
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
    sway = np.zeros(len(me.vertices))
    if "sway" in me.attributes:
        me.attributes["sway"].data.foreach_get("value", sway)
    lp = np.zeros(nl, dtype=np.int32)
    fm = np.zeros(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("material_index", fm)
    ls = np.zeros(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("loop_start", ls)
    lt = np.zeros(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("loop_total", lt)
    lp = np.repeat(fm, lt)
    tris = np.zeros(len(me.loop_triangles) * 3, dtype=np.int32)
    me.loop_triangles.foreach_get("loops", tris)
    pos, nor = np.round(co[lv], 3), np.round(cn, 2)
    sw = np.round(sway[lv] * 100)
    key = np.concatenate([pos, nor, lp[:, None], sw[:, None]], axis=1)
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
    eo.to_mesh_clear()

    def flat(a, digits):
        return [round(float(x), digits) if digits else int(x) for x in a.ravel()]
    return {"points": flat(p, 3), "facings": flat(n, 2), "paints": flat(lp[first], 0), "triangles": flat(t, 0), "sway": flat(sw[first] / 100.0, 2)}


def export(name, out, kinds=("json", "glb")):
    """Build one model (near, a mid made from it at about a quarter, and a plain far) and write it out."""
    kit.fresh()
    far_p = MODELS[name](False)
    far_ob = far_p.object(look=False)
    bpy.context.scene.collection.objects.link(far_ob)
    near_p = MODELS[name](True)
    ob = near_p.object(look=False)
    bpy.context.scene.collection.objects.link(ob)
    mid_ob = ob.copy()
    mid_ob.data = ob.data.copy()
    bpy.context.scene.collection.objects.link(mid_ob)
    dec = mid_ob.modifiers.new("mid", "DECIMATE")
    dec.ratio = 0.25
    paints = near_p.paints
    # (the far model's paints are a list of their own: put them in the near model's order)
    far_order = [paints.index(p) if p in paints else len(paints) for p in far_p.paints]
    names = paints + [p for p in far_p.paints if p not in paints]
    near, mid, far = gather(ob, paints), gather(mid_ob, paints), gather(far_ob, far_p.paints)
    far["paints"] = [far_order[i] if far_order[i] < len(paints) else names.index(far_p.paints[i]) for i in far["paints"]]
    data = {
        "name": near_p.title, "kind": "scenery",
        "about": ("Made by tools/battle-models/med/scenery.py. Metres; x east, y right, z up from the ground at the foot. Scenery: nothing moves "
                  "but what the wind moves (`sway`, 0 to 1 point by point: how far the wind moves that point; the game bends by it). "
                  "near: close; mid: about a quarter of near; far: the plain shape for the whole view. Paints: " + ", ".join(f"{i} {k}" for i, k in enumerate(names))),
        "colours": [PAINTS[k] for k in names], "paint_names": names,
        "near": near, "mid": mid, "far": far, **near_p.info,
    }
    xs, ys, zs = near["points"][0::3], near["points"][1::3], near["points"][2::3]
    data["long"], data["wide"], data["high"] = round(max(xs) - min(xs), 2), round(max(ys) - min(ys), 2), round(max(zs) - min(zs), 2)
    os.makedirs(out, exist_ok=True)
    sizes = {}
    if "json" in kinds:
        path = os.path.join(out, name + ".json")
        with open(path, "w") as f:
            json.dump(data, f, separators=(",", ":"))
        sizes["json"] = os.path.getsize(path)
    if "glb" in kinds:
        for o in (far_ob, mid_ob):
            bpy.data.objects.remove(o)
        for i, p in enumerate(paints):                   # (plain colours in the .glb: the game colours by paint name)
            ob.data.materials[i] = material(p, look=False)
        sizes["glb"] = os.path.getsize(kit.export_glb(os.path.join(out, name + ".glb"), [ob]))
    tri = len(near["triangles"]) // 3
    print(f"{name}: {tri} triangles near, {len(mid['triangles']) // 3} mid, {len(far['triangles']) // 3} far; "
          + ", ".join(f"{k} {s / 1e6:.2f} MB" for k, s in sizes.items()), flush=True)
    return {"name": name, "title": near_p.title, "triangles": {"near": tri, "mid": len(mid["triangles"]) // 3, "far": len(far["triangles"]) // 3}, **near_p.info,
            "long": data["long"], "wide": data["wide"], "high": data["high"]}


def main():
    out = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != "all" else ",".join(MODELS)
    kinds = sys.argv[3:] or ["json", "glb"]
    idx_path = os.path.join(out, "index.json")
    index = json.load(open(idx_path)) if os.path.exists(idx_path) else {}
    for name in which.split(","):
        index[name] = export(name, out, kinds)
        with open(idx_path, "w") as f:
            json.dump(index, f, indent=1)


if __name__ == "__main__":
    main()
