"""The ground the works are shown on, for the pictures only: late-summer grass worn to earth round each work, holes
opened where a work is dug in, a wood's edge far off, haze to the horizon, the land team's sun.

The game builds its own field (ruled by the art director: field-battle-look.md); this only borrows its look so the
pictures promise nothing else. A dug work says where the ground must be opened in its file (`dig`: outlines in the
work's own plan); the game must do the same on its own ground.

    g = ground.Ground(digs=[outline, ...], worn=[(x, y, r), ...]);  g.finish()
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bmesh  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector, noise  # noqa: E402

import battle_set  # noqa: E402  (the land team's preview field: its haze and its trees)
import kit  # noqa: E402
import stage  # noqa: E402


def inside(poly, x, y):
    n, c = len(poly), False
    j = n - 1
    for i in range(n):
        xi, yi = poly[i][0], poly[i][1]
        xj, yj = poly[j][0], poly[j][1]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-12) + xi:
            c = not c
        j = i
    return c


class Ground:
    def __init__(self, digs=(), worn=(), seed=3, flat=34.0, fine=26.0, cell=0.25, sun_turn=150.0, sun_elev=40.0, trees=True, grass=True, kind="grass",
                 sky_level=0.55, sun=3.2, grass_area=38.0, grass_count=120000, clear=()):
        self.digs = [[(p[0], p[1]) for p in d] for d in digs]
        self.worn = list(worn)                       # (x, y, r[, strength]) circles of trodden earth
        self.clear = [[(p[0], p[1]) for p in d] for d in clear]      # outlines where no grass stands (under a work)
        self.flat, self.seed, self.kind = flat, seed, kind
        stage.sky(sun_elev=sun_elev, sun_turn=sun_turn, strength=sun, level=sky_level)
        self.terrain = self._terrain(fine, cell)
        if trees:
            self._trees()
        if grass:
            self._grass(grass_area, grass_count)

    # ---- heights
    def height(self, x, y):
        r = math.hypot(x, y)
        k = min(max((r - self.flat) / 70.0, 0.0), 1.0)
        k = k * k * (3 - 2 * k)
        if k <= 0.0:
            return 0.0
        h = 2.4 * noise.noise(Vector((x / 150.0, y / 150.0, 0.3 + self.seed))) + 0.6 * noise.noise(Vector((x / 38.0, y / 38.0, 1.7)))
        return k * (h + 0.006 * abs(x))

    def wear(self, x, y):
        w = 0.0
        for c in self.worn:
            d = math.hypot(x - c[0], y - c[1])
            s = c[3] if len(c) > 3 else 1.0
            w = max(w, s * (1.0 - min(max((d - c[2] * 0.55) / (c[2] * 0.45 + 1e-6), 0.0), 1.0)))
        if w > 0.0:
            w = min(1.0, max(0.0, w + 0.45 * noise.noise(Vector((x * 0.9, y * 0.9, 5.0))) * (1.0 - abs(2 * w - 1.0)) * 1.4))
        return w

    def _terrain(self, fine, cell):
        xs = sorted(set([round(v * cell, 4) for v in range(-int(fine / cell), int(fine / cell) + 1)] + list(range(-150, 151, 3)) + list(range(-900, 901, 30))))
        n = len(xs)
        X, Y = np.meshgrid(np.array(xs), np.array(xs), indexing="ij")
        verts = np.zeros((n, n, 3))
        verts[..., 0], verts[..., 1] = X, Y
        for i in range(n):
            for j in range(n):
                if abs(xs[i]) > self.flat * 0.7 or abs(xs[j]) > self.flat * 0.7:
                    verts[i, j, 2] = self.height(xs[i], xs[j])
        boxes = [(min(p[0] for p in d), max(p[0] for p in d), min(p[1] for p in d), max(p[1] for p in d)) for d in self.digs]
        faces = []
        for i in range(n - 1):
            cx = 0.5 * (xs[i] + xs[i + 1])
            for j in range(n - 1):
                cy = 0.5 * (xs[j] + xs[j + 1])
                hole = False
                for d, (x0, x1, y0, y1) in zip(self.digs, boxes):
                    if x0 <= cx <= x1 and y0 <= cy <= y1 and inside(d, cx, cy):
                        hole = True
                        break
                if not hole:
                    faces.append((i * n + j, (i + 1) * n + j, (i + 1) * n + j + 1, i * n + j + 1))
        me = bpy.data.meshes.new("ground")
        me.from_pydata(verts.reshape(-1, 3).tolist(), [], faces)
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
        me.update()
        col = me.color_attributes.new("worn", "FLOAT_COLOR", "POINT")
        vals = np.zeros((n * n, 4))
        vals[:, 3] = 1.0
        if self.worn:
            lim = max(math.hypot(c[0], c[1]) + c[2] for c in self.worn) + 1.0
            for i in range(n):
                if abs(xs[i]) > lim:
                    continue
                for j in range(n):
                    if abs(xs[j]) > lim:
                        continue
                    w = self.wear(xs[i], xs[j])
                    vals[i * n + j, 0:3] = w
        col.data.foreach_set("color", vals.ravel())
        ob = bpy.data.objects.new("ground", me)
        bpy.context.scene.collection.objects.link(ob)
        ob.data.materials.append(self._material())
        return ob

    def _material(self):
        m = bpy.data.materials.new("ground")
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tc = nt.nodes.new("ShaderNodeTexCoord")

        def nz(scale, detail=6.0):
            n = nt.nodes.new("ShaderNodeTexNoise")
            n.inputs["Scale"].default_value = scale
            n.inputs["Detail"].default_value = detail
            nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
            return n

        def ramp(fac, a, b, pa=0.35, pb=0.65):
            r = nt.nodes.new("ShaderNodeValToRGB")
            r.color_ramp.elements[0].position, r.color_ramp.elements[0].color = pa, (*kit.hex_rgb(a), 1)
            r.color_ramp.elements[1].position, r.color_ramp.elements[1].color = pb, (*kit.hex_rgb(b), 1)
            nt.links.new(fac, r.inputs["Fac"])
            return r

        def mix(a, b, fac, blend="MIX"):
            n = nt.nodes.new("ShaderNodeMix")
            n.data_type, n.blend_type = "RGBA", blend
            if isinstance(fac, (int, float)):
                n.inputs[0].default_value = fac
            else:
                nt.links.new(fac, n.inputs[0])
            nt.links.new(a, n.inputs[6])
            nt.links.new(b, n.inputs[7])
            return n.outputs[2]

        big, mid, fine = nz(0.025, 4), nz(0.17, 8), nz(2.6, 10)
        if self.kind == "sand":
            grass = ramp(big.outputs["Fac"], "C9B68A", "BFA878")
            dry = ramp(mid.outputs["Fac"], "CDBB90", "B49C6C", 0.45, 0.7)
            earth = ramp(mid.outputs["Fac"], "A8946A", "BCA67A", 0.3, 0.7)
        else:
            grass = ramp(big.outputs["Fac"], "66753E", "86834C")
            dry = ramp(mid.outputs["Fac"], "66733C", "948A58", 0.42, 0.68)
            earth = ramp(mid.outputs["Fac"], "75624A", "93805C", 0.3, 0.7)
        base = mix(grass.outputs["Color"], dry.outputs["Color"], 0.45)
        worn = nt.nodes.new("ShaderNodeVertexColor")
        worn.layer_name = "worn"
        base = mix(base, earth.outputs["Color"], worn.outputs["Color"])
        fr = ramp(fine.outputs["Fac"], "8A8A8A", "FFFFFF", 0.25, 0.75)
        base = mix(base, fr.outputs["Color"], 0.5, "MULTIPLY")
        nt.links.new(base, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.97
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.45
        bump.inputs["Distance"].default_value = 0.05
        nt.links.new(fine.outputs["Fac"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        return battle_set.haze(m, start=70.0, end=800.0)

    def _trees(self):
        """Woods' edges and lone trees well back from the works: each crown many lumpy clumps of leaf on a trunk and limbs,
        all one mesh, the leaves shaded from deep green in the hollows to sunlit yellow-green."""
        rnd = random.Random(self.seed)
        bm = bmesh.new()
        ball = bmesh.new()
        bmesh.ops.create_icosphere(ball, subdivisions=2, radius=1.0)
        bverts = [v.co.copy() for v in ball.verts]
        bfaces = [[v.index for v in f.verts] for f in ball.faces]
        ball.free()
        leaf_faces, bark_faces = [], []

        def clump(c, r, squash, sd):
            vs = []
            for w in bverts:
                k = 1.0 + 0.34 * noise.noise(w * 1.4 + Vector((sd, sd * 0.37, 0.0))) + 0.14 * noise.noise(w * 4.1 + Vector((0.0, sd, 1.0)))
                vs.append(bm.verts.new(c + Vector((w.x * r * k, w.y * r * k, w.z * r * k * squash))))
            for f in bfaces:
                leaf_faces.append(bm.faces.new([vs[i] for i in f]))

        def limb(a, b_, ra, rb, sides=7):
            ax = (b_ - a).normalized()
            u = ax.cross(Vector((0.3, 0.2, 1.0))).normalized()
            v = ax.cross(u)
            ra_, rb_ = [], []
            for i in range(sides):
                t = 2 * math.pi * i / sides
                d = u * math.cos(t) + v * math.sin(t)
                ra_.append(bm.verts.new(a + d * ra))
                rb_.append(bm.verts.new(b_ + d * rb))
            for i in range(sides):
                j = (i + 1) % sides
                bark_faces.append(bm.faces.new((ra_[i], ra_[j], rb_[j], rb_[i])))

        def tree(at, h, sd):
            g = random.Random(sd)
            trunk = h * g.uniform(0.3, 0.42)
            lean = Vector((g.uniform(-0.05, 0.05), g.uniform(-0.05, 0.05), 1.0)).normalized()
            top = at + lean * trunk
            limb(at - Z_ * 0.3, top, 0.035 * h, 0.022 * h)
            crown_c = at + lean * (trunk + h * 0.3)
            rx, rz = h * g.uniform(0.3, 0.4), h * g.uniform(0.3, 0.36)
            for k in range(g.randint(4, 6)):
                a = g.uniform(0, 2 * math.pi)
                tip = crown_c + Vector((math.cos(a) * rx * 0.6, math.sin(a) * rx * 0.6, g.uniform(-0.3, 0.2) * rz))
                limb(top - lean * g.uniform(0.0, 0.25) * trunk, tip, 0.016 * h, 0.006 * h, 5)
            for k in range(g.randint(18, 26)):
                # (clumps spread through the crown's egg, bigger at its heart, smaller and lower at its skirts)
                a, e = g.uniform(0, 2 * math.pi), math.asin(g.uniform(-0.75, 1.0))
                d = g.random() ** 0.5
                c = crown_c + Vector((math.cos(a) * math.cos(e) * rx * d, math.sin(a) * math.cos(e) * rx * d, math.sin(e) * rz * d))
                clump(c, h * g.uniform(0.1, 0.17) * (1.15 - 0.4 * d), g.uniform(0.7, 0.95), sd * 3.1 + k * 7.7)

        Z_ = Vector((0, 0, 1))
        placed = 0
        for k in range(2600):
            if placed >= 230:
                break
            a = rnd.uniform(0, 2 * math.pi)
            r = rnd.uniform(self.flat + 55.0, self.flat + 250.0)
            x, y = math.cos(a) * r, math.sin(a) * r
            dense = 0.5 + 0.5 * noise.noise(Vector((x / 110.0, y / 110.0, 9.0 + self.seed)))
            if dense < 0.6:
                continue
            tree(Vector((x, y, self.height(x, y))), rnd.uniform(9, 17), self.seed * 100 + k)
            placed += 1
        me = bpy.data.meshes.new("trees")
        leaf_idx = {f.index for f in leaf_faces}
        bm.faces.index_update()
        leaf_idx = {f.index for f in leaf_faces}
        bm.to_mesh(me)
        bm.free()
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
        me.polygons.foreach_set("material_index", [0 if i in leaf_idx else 1 for i in range(len(me.polygons))])
        me.update()
        leaf = bpy.data.materials.new("leaf")
        leaf.use_nodes = True
        nt = leaf.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        n1 = nt.nodes.new("ShaderNodeTexNoise")
        n1.inputs["Scale"].default_value, n1.inputs["Detail"].default_value, n1.inputs["Roughness"].default_value = 1.6, 8.0, 0.7
        nt.links.new(tc.outputs["Object"], n1.inputs["Vector"])
        n2 = nt.nodes.new("ShaderNodeTexNoise")
        n2.inputs["Scale"].default_value, n2.inputs["Detail"].default_value = 0.09, 3.0
        nt.links.new(tc.outputs["Object"], n2.inputs["Vector"])
        r = nt.nodes.new("ShaderNodeValToRGB")
        e = r.color_ramp.elements
        e[0].position, e[0].color = 0.3, (*kit.hex_rgb("2F3D22"), 1)
        e[1].position, e[1].color = 0.72, (*kit.hex_rgb("6F8040"), 1)
        mid = e.new(0.5)
        mid.color = (*kit.hex_rgb("47592D"), 1)
        nt.links.new(n1.outputs[0], r.inputs["Fac"])
        r2 = nt.nodes.new("ShaderNodeValToRGB")
        r2.color_ramp.elements[0].position, r2.color_ramp.elements[0].color = 0.35, (0.82, 0.9, 0.8, 1)
        r2.color_ramp.elements[1].position, r2.color_ramp.elements[1].color = 0.65, (1.12, 1.05, 0.85, 1)
        nt.links.new(n2.outputs[0], r2.inputs["Fac"])
        mx = nt.nodes.new("ShaderNodeMix")
        mx.data_type, mx.blend_type = "RGBA", "MULTIPLY"
        mx.inputs[0].default_value = 1.0
        nt.links.new(r.outputs["Color"], mx.inputs[6])
        nt.links.new(r2.outputs["Color"], mx.inputs[7])
        nt.links.new(mx.outputs[2], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.8
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 1.0
        bump.inputs["Distance"].default_value = 0.5
        nt.links.new(n1.outputs[0], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        bark = kit.material("bark", kit.hex_rgb("4A3F33"), rough=0.95, noise=0.5, noise_scale=8, bump=0.5)
        ob = bpy.data.objects.new("trees", me)
        bpy.context.scene.collection.objects.link(ob)
        ob.data.materials.append(battle_set.haze(leaf, start=70.0, end=800.0))
        ob.data.materials.append(battle_set.haze(bark, start=70.0, end=800.0))
        return ob

    def _grass(self, area, count):
        """Tufts of late-summer grass: each a handful of bent blades, short where it is trodden, long and going to straw
        in patches, every tuft its own shade (carried on the blades as a colour, darker at the root)."""
        rnd = random.Random(self.seed + 5)
        boxes = [(min(p[0] for p in d) - 0.2, max(p[0] for p in d) + 0.2, min(p[1] for p in d) - 0.2, max(p[1] for p in d) + 0.2) for d in self.digs + self.clear]
        polys = self.digs + self.clear
        verts, faces, cols = [], [], []
        green, straw, deep = Vector(kit.hex_rgb("7F8E4A")), Vector(kit.hex_rgb("BDB274")), Vector(kit.hex_rgb("66743C"))
        if self.kind == "sand":
            green, straw, deep = Vector(kit.hex_rgb("8E8A52")), Vector(kit.hex_rgb("B9A870")), Vector(kit.hex_rgb("6F6C42"))
        for _ in range(count):
            # (thicker toward the middle, where the camera looks)
            r = area * rnd.random() ** 0.8
            a = rnd.uniform(0, 2 * math.pi)
            x, y = math.cos(a) * r, math.sin(a) * r
            w = self.wear(x, y)
            if rnd.random() < w * 1.3:
                continue
            skip = False
            for d, (x0, x1, y0, y1) in zip(polys, boxes):
                if x0 <= x <= x1 and y0 <= y <= y1 and inside(d, x, y):
                    skip = True
                    break
            if skip:
                continue
            z = self.height(x, y)
            patchy = 0.5 + 0.5 * noise.noise(Vector((x / 5.0, y / 5.0, 2.0)))
            clump = 0.5 + 0.5 * noise.noise(Vector((x / 1.3, y / 1.3, 7.0)))
            if rnd.random() > 0.35 + 0.9 * clump:
                continue
            tall = (0.4 + 0.75 * patchy * patchy) * (1.0 - 0.6 * w) * (0.7 + 0.5 * clump)
            dry = min(1.0, max(0.0, 0.15 + 0.9 * noise.noise(Vector((x / 9.0, y / 9.0, 4.0))) + rnd.uniform(-0.2, 0.3)))
            shade = rnd.uniform(0.8, 1.15)
            tip = (green.lerp(straw, dry)) * shade
            root = (deep.lerp(green, 0.4)) * shade * 0.92
            for b in range(rnd.randint(5, 8)):
                ang = rnd.uniform(0, 2 * math.pi)
                h = rnd.uniform(0.07, 0.22) * tall
                out = Vector((math.cos(ang), math.sin(ang), 0))
                side = Vector((-math.sin(ang), math.cos(ang), 0)) * rnd.uniform(0.006, 0.012)
                base = Vector((x + rnd.uniform(-0.07, 0.07), y + rnd.uniform(-0.07, 0.07), z - 0.02))
                lean = rnd.uniform(0.15, 0.6) * h
                mid = base + out * lean * 0.35 + Vector((0, 0, h * 0.6))
                top = base + out * lean + Vector((0, 0, h))
                k = len(verts)
                verts += [base - side, base + side, mid + side * 0.7, mid - side * 0.7, top]
                faces += [(k, k + 1, k + 2, k + 3), (k + 3, k + 2, k + 4)]
                mc = root.lerp(tip, 0.7)
                cols += [root, root, mc, mc, tip]
        me = bpy.data.meshes.new("grass")
        me.from_pydata([tuple(v) for v in verts], [], faces)
        me.update()
        ca = me.color_attributes.new("tint", "FLOAT_COLOR", "POINT")
        flat = np.ones((len(verts), 4))
        flat[:, 0:3] = np.array([tuple(c) for c in cols]) if cols else 0.0
        ca.data.foreach_set("color", flat.ravel())
        ob = bpy.data.objects.new("grass", me)
        bpy.context.scene.collection.objects.link(ob)
        mat = bpy.data.materials.new("grasstuft")
        mat.use_nodes = True
        nt = mat.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        at = nt.nodes.new("ShaderNodeVertexColor")
        at.layer_name = "tint"
        nt.links.new(at.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.75
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = 0.2
        # (blades are lit from both sides: face the light's side)
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        up = nt.nodes.new("ShaderNodeVectorMath")
        up.operation = "NORMALIZE"
        mixn = nt.nodes.new("ShaderNodeMix")
        mixn.data_type = "VECTOR"
        mixn.inputs[0].default_value = 0.7
        nt.links.new(geo.outputs["Normal"], mixn.inputs[4])
        mixn.inputs[5].default_value = (0.0, 0.0, 1.0)
        nt.links.new(mixn.outputs[1], up.inputs[0])
        nt.links.new(up.outputs["Vector"], bsdf.inputs["Normal"])
        ob.data.materials.append(battle_set.haze(mat, start=70.0, end=800.0))
        ob.visible_shadow = False
        return ob


def placed(outline, at=(0.0, 0.0), yaw=0.0):
    """A work's own outline (in its plan) where the work stands on the ground."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return [(at[0] + p[0] * c - p[1] * s, at[1] + p[0] * s + p[1] * c) for p in outline]
