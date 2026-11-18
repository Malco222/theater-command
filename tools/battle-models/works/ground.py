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
                 sky_level=0.55, sun=3.2, grass_area=42.0, grass_count=52000, clear=()):
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
            grass = ramp(big.outputs["Fac"], "6F7B42", "8D8650")
            dry = ramp(mid.outputs["Fac"], "7A8048", "9C8C5E", 0.45, 0.7)
            earth = ramp(mid.outputs["Fac"], "6A5843", "86724F", 0.3, 0.7)
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
        rnd = random.Random(self.seed)
        mats = {
            "bark": battle_set.haze(kit.material("bark", kit.hex_rgb("4A3F33"), rough=0.95, noise=0.5, noise_scale=8, bump=0.5)),
            "leaf": battle_set.haze(kit.material("leaf", kit.hex_rgb("4F6034"), rough=0.85, noise=0.55, noise_scale=3.0, bump=0.8)),
        }
        # a wood's edge behind the works (the defender's side) and a few lone trees
        placed = 0
        for k in range(400):
            if placed >= 70:
                break
            a = rnd.uniform(0, 2 * math.pi)
            r = rnd.uniform(95.0, 240.0)
            x, y = math.cos(a) * r, math.sin(a) * r
            dense = 0.5 + 0.5 * noise.noise(Vector((x / 90.0, y / 90.0, 9.0 + self.seed)))
            if dense < 0.52:
                continue
            battle_set._tree(f"tree{k}", Vector((x, y, self.height(x, y))), rnd.uniform(9, 16), self.seed * 100 + k, mats)
            placed += 1

    def _grass(self, area, count):
        rnd = random.Random(self.seed + 5)
        bm = bmesh.new()
        boxes = [(min(p[0] for p in d) - 0.2, max(p[0] for p in d) + 0.2, min(p[1] for p in d) - 0.2, max(p[1] for p in d) + 0.2) for d in self.digs + self.clear]
        polys = self.digs + self.clear
        for _ in range(count):
            # (thicker toward the middle, where the camera looks)
            r = area * rnd.random() ** 0.75
            a = rnd.uniform(0, 2 * math.pi)
            x, y = math.cos(a) * r, math.sin(a) * r
            if rnd.random() < self.wear(x, y) * 1.25:
                continue
            skip = False
            for d, (x0, x1, y0, y1) in zip(polys, boxes):
                if x0 <= x <= x1 and y0 <= y <= y1 and inside(d, x, y):
                    skip = True
                    break
            if skip:
                continue
            z = self.height(x, y)
            tall = 0.6 + 0.8 * (0.5 + 0.5 * noise.noise(Vector((x / 6.0, y / 6.0, 2.0))))
            for b in range(4):
                ang = rnd.uniform(0, 2 * math.pi)
                h = rnd.uniform(0.12, 0.34) * tall
                lean = Vector((math.cos(ang), math.sin(ang), 0)) * rnd.uniform(0.03, 0.13)
                side = Vector((-math.sin(ang), math.cos(ang), 0)) * 0.011
                base = Vector((x + rnd.uniform(-0.05, 0.05), y + rnd.uniform(-0.05, 0.05), z - 0.02))
                bm.faces.new((bm.verts.new(base - side), bm.verts.new(base + side), bm.verts.new(base + lean + Vector((0, 0, h)))))
        me = bpy.data.meshes.new("grass")
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new("grass", me)
        bpy.context.scene.collection.objects.link(ob)
        mat = kit.material("grasstuft", kit.hex_rgb("7C8448" if self.kind != "sand" else "A89C62"), rough=0.8, noise=0.5, noise_scale=1.5)
        ob.data.materials.append(battle_set.haze(mat, start=70.0, end=800.0))
        ob.visible_shadow = False
        return ob


def placed(outline, at=(0.0, 0.0), yaw=0.0):
    """A work's own outline (in its plan) where the work stands on the ground."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return [(at[0] + p[0] * c - p[1] * s, at[1] + p[0] * s + p[1] * c) for p in outline]
