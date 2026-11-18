"""The ground the works are shown on, for the pictures only: a late-summer field in the field battle's own colours,
worn to earth round each work in a ragged, wandering edge, paths trodden to the doors, wheel ruts to the gun pits,
holes opened where a work is dug in, hedgerows and a wood's edge behind, haze to the horizon, the land team's sun.

The game builds its own field (ruled by the art director: field-battle-look.md); this only borrows its look so the
pictures promise nothing else. A dug work says where the ground must be opened in its file (`dig`: outlines in the
work's own plan) and where it is worn (`worn`: circles); the game must do the same on its own ground.

    g = ground.Ground(digs=[outline, ...], worn=[(x, y, r), ...], paths=[[(x, y), ...]], tracks=[[(x, y), ...]])
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

import battle_set  # noqa: E402  (the land team's preview field: its haze and its colours)
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


def inside_many(poly, X, Y):
    """`inside` for arrays of points."""
    c = np.zeros(np.shape(X), bool)
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i][0], poly[i][1]
        xj, yj = poly[j][0], poly[j][1]
        c ^= ((yi > Y) != (yj > Y)) & (X < (xj - xi) * (Y - yi) / (yj - yi + 1e-12) + xi)
        j = i
    return c


class Field:
    """A smooth random field (-1 to 1) read at any points: the wander of a worn edge, where the grass grows long."""

    def __init__(self, period, seed, size=96):
        self.v = np.random.default_rng(seed).uniform(-1.0, 1.0, (size, size))
        self.p, self.n = period, size

    def __call__(self, x, y):
        u, v = np.asarray(x, float) / self.p, np.asarray(y, float) / self.p
        i, j = np.floor(u).astype(int), np.floor(v).astype(int)
        fu, fv = u - i, v - j
        fu, fv = fu * fu * (3 - 2 * fu), fv * fv * (3 - 2 * fv)
        n, a = self.n, self.v
        return (a[i % n, j % n] * (1 - fu) + a[(i + 1) % n, j % n] * fu) * (1 - fv) + (a[i % n, (j + 1) % n] * (1 - fu) + a[(i + 1) % n, (j + 1) % n] * fu) * fv


def smooth(v, a, b):
    k = np.clip((np.asarray(v, float) - a) / (b - a), 0.0, 1.0)
    return k * k * (3 - 2 * k)


def walk(line, step=0.3):
    """Points every `step` along a line of points, with how far along each is (0 to 1)."""
    pts = [Vector((p[0], p[1])) for p in line]
    total = sum((b - a).length for a, b in zip(pts, pts[1:])) or 1.0
    out, done = [], 0.0
    for a, b in zip(pts, pts[1:]):
        L = (b - a).length
        for k in range(max(1, int(L / step))):
            t = k / max(1, int(L / step))
            out.append((a.lerp(b, t), (done + L * t) / total, (b - a).normalized()))
        done += L
    out.append((pts[-1], 1.0, (pts[-1] - pts[-2]).normalized()))
    return out


def soft(line, n=3):
    """A line of points with its corners rounded."""
    pts = [Vector((p[0], p[1])) for p in line]
    for _ in range(n):
        new = [pts[0]]
        for a, b in zip(pts, pts[1:]):
            new += [a.lerp(b, 0.25), a.lerp(b, 0.75)]
        new.append(pts[-1])
        pts = new
    return pts


def mesh_of(name, verts, quads=None, tris=None):
    """A mesh straight from arrays."""
    verts = np.asarray(verts, np.float32).reshape(-1, 3)
    quads = np.zeros((0, 4), np.int32) if quads is None else np.asarray(quads, np.int32).reshape(-1, 4)
    tris = np.zeros((0, 3), np.int32) if tris is None else np.asarray(tris, np.int32).reshape(-1, 3)
    me = bpy.data.meshes.new(name)
    me.vertices.add(len(verts))
    me.vertices.foreach_set("co", verts.ravel())
    me.loops.add(len(quads) * 4 + len(tris) * 3)
    me.loops.foreach_set("vertex_index", np.concatenate([quads.ravel(), tris.ravel()]))
    me.polygons.add(len(quads) + len(tris))
    me.polygons.foreach_set("loop_start", np.concatenate([np.arange(len(quads)) * 4, len(quads) * 4 + np.arange(len(tris)) * 3]).astype(np.int32))
    me.update(calc_edges=True)
    return me


def tinted(me, name, cols):
    ca = me.color_attributes.new(name, "FLOAT_COLOR", "POINT")
    flat = np.ones((len(me.vertices), 4), np.float32)
    flat[:, 0:3] = cols
    ca.data.foreach_set("color", flat.ravel())


def tint_material(name, rough=0.75, up=0.7, sheen=0.2, through=0.45):
    """A material that takes its colour from the points' own tint; its faces shaded as if they looked mostly upward, and
    letting light through as a blade or a leaf does, so the side away from the sun is not black."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    at = nt.nodes.new("ShaderNodeVertexColor")
    at.layer_name = "tint"
    nt.links.new(at.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = rough
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = sheen
    if up > 0.0:
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        nrm = nt.nodes.new("ShaderNodeVectorMath")
        nrm.operation = "NORMALIZE"
        mixn = nt.nodes.new("ShaderNodeMix")
        mixn.data_type = "VECTOR"
        mixn.inputs[0].default_value = up
        nt.links.new(geo.outputs["Normal"], mixn.inputs[4])
        mixn.inputs[5].default_value = (0.0, 0.0, 1.0)
        nt.links.new(mixn.outputs[1], nrm.inputs[0])
        nt.links.new(nrm.outputs["Vector"], bsdf.inputs["Normal"])
    if through > 0.0:
        tr = nt.nodes.new("ShaderNodeBsdfTranslucent")
        nt.links.new(at.outputs["Color"], tr.inputs["Color"])
        both = nt.nodes.new("ShaderNodeMixShader")
        both.inputs[0].default_value = through
        out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
        nt.links.new(bsdf.outputs[0], both.inputs[1])
        nt.links.new(tr.outputs[0], both.inputs[2])
        nt.links.new(both.outputs[0], out.inputs["Surface"])
    return battle_set.haze(mat)


class Ground:
    def __init__(self, digs=(), worn=(), seed=3, flat=34.0, fine=26.0, cell=0.25, sun_turn=150.0, sun_elev=40.0, trees=True, grass=True, kind="grass",
                 sky_level=0.55, sun=3.2, grass_area=38.0, grass_count=120000, clear=(), paths=(), tracks=(), churn=(), centre=(0.0, 0.0), hedges=True, dig_depth=3.4):
        self.digs = [[(p[0], p[1]) for p in d] for d in digs]
        self.clear = [[(p[0], p[1]) for p in d] for d in clear]      # outlines where no grass stands (under a work)
        self.cx, self.cy = centre
        self.flat, self.seed, self.kind, self.fine, self.cell = flat, seed, kind, 3.0 * math.ceil(fine / 3.0), cell      # (the fine ground's edge on the coarse lattice)
        s = seed * 13
        self.f0, self.f1, self.f2, self.f3 = Field(5.5, s), Field(2.3, s + 1), Field(0.62, s + 2), Field(0.2, s + 3)
        self.long_, self.clump, self.bare, self.tone = Field(6.5, s + 4), Field(1.15, s + 5), Field(4.2, s + 6), Field(11.0, s + 7)
        self.h1, self.h2 = Field(150.0, s + 8), Field(38.0, s + 9)
        stage.sky(sun_elev=sun_elev, sun_turn=sun_turn, strength=sun, level=sky_level)
        self._raster(list(worn), list(churn), list(paths), list(tracks))
        if trees:
            self._trees(hedges)
        self.terrain = self._terrain()
        self._holes(dig_depth)
        if grass:
            self._grass(grass_area, grass_count)
        self._clods()

    # ---- heights and wear
    def height(self, x, y):
        r = np.hypot(np.asarray(x, float) - self.cx, np.asarray(y, float) - self.cy)
        k = smooth(r, self.flat, self.flat + 70.0)
        h = 2.6 * self.h1(x, y) + 0.7 * self.h2(x, y) + 0.004 * r
        out = k * h
        return float(out) if np.ndim(out) == 0 else out

    def _read(self, A, x, y):
        n = A.shape[0]
        u, v = (np.asarray(x, float) - self.gx[0]) / self.cell, (np.asarray(y, float) - self.gy[0]) / self.cell
        i, j = np.clip(np.rint(u).astype(int), 0, n - 1), np.clip(np.rint(v).astype(int), 0, n - 1)
        return np.where((u < 0) | (u > n - 1) | (v < 0) | (v > n - 1), 0.0, A[i, j])

    def wear(self, x, y):
        out = self._read(self.W, x, y)
        return float(out) if np.ndim(out) == 0 else out

    def _raster(self, worn, churn, paths, tracks):
        """Where the grass is trodden to earth (W), where the earth is freshly turned and dark (C), and where wheels have
        sunk ruts (R): each a picture of the middle of the ground, a point every `cell`. A worn circle's edge wanders
        (by a third of its radius, in long and short waves), so no two sides of a work wear alike."""
        n = int(round(2 * self.fine / self.cell)) + 1
        self.gx = self.cx - self.fine + self.cell * np.arange(n)
        self.gy = self.cy - self.fine + self.cell * np.arange(n)
        X, Y = np.meshgrid(self.gx, self.gy, indexing="ij")
        wander = 0.34 * self.f0(X, Y) + 0.3 * self.f1(X, Y) + 0.16 * self.f2(X, Y)
        self.W, self.C, self.R = np.zeros((n, n)), np.zeros((n, n)), np.zeros((n, n))

        def stamp(A, x, y, r, s=1.0, edge=0.6, wild=1.0):
            reach = r * 1.6 + self.cell
            i0, i1 = max(0, int((x - reach - self.gx[0]) / self.cell)), min(n, int((x + reach - self.gx[0]) / self.cell) + 2)
            j0, j1 = max(0, int((y - reach - self.gy[0]) / self.cell)), min(n, int((y + reach - self.gy[0]) / self.cell) + 2)
            if i0 >= i1 or j0 >= j1:
                return
            d = np.hypot(X[i0:i1, j0:j1] - x, Y[i0:i1, j0:j1] - y)
            re = r * (1.0 + wild * wander[i0:i1, j0:j1])
            w = s * (1.0 - smooth(d, re * (1.0 - edge), re))
            np.maximum(A[i0:i1, j0:j1], w, out=A[i0:i1, j0:j1])

        for c in worn:
            s = c[3] if len(c) > 3 else 1.0
            stamp(self.W, c[0], c[1], c[2], 0.95 * s, 0.7)
        for c in churn:
            stamp(self.C, c[0], c[1], c[2], c[3] if len(c) > 3 else 1.0, 0.7)
            stamp(self.W, c[0], c[1], c[2] * 1.15, 0.9)
        for line in paths:
            for p, t, d in walk(soft(line), 0.3):
                side = Vector((-d.y, d.x))
                q = p + side * 0.3 * float(self.f1(p.x * 1.7, p.y * 1.7))
                fade = 1.0 - smooth(t, 0.6, 1.0)                          # a path peters out into the grass
                stamp(self.W, q.x, q.y, 0.5 + 0.15 * float(self.f2(p.x, p.y)), (0.5 + 0.35 * fade) * (0.85 + 0.15 * float(self.f1(p.y, p.x))), 0.75, 0.6)
        for line in tracks:
            for p, t, d in walk(soft(line), 0.25):
                side = Vector((-d.y, d.x))
                fade = 1.0 - smooth(t, 0.7, 1.0)
                stamp(self.W, p.x, p.y, 1.5, 0.42 * fade, 0.8)
                for k in (-0.82, 0.82):
                    q = p + side * (k + 0.06 * float(self.f2(p.x * 2.0, p.y * 2.0)))
                    stamp(self.W, q.x, q.y, 0.34, 0.95 * fade, 0.6, 0.5)
                    stamp(self.C, q.x, q.y, 0.3, 0.8 * fade, 0.7, 0.5)
                    stamp(self.R, q.x, q.y, 0.3, 1.0 * fade, 0.8, 0.3)

    def _terrain(self):
        def axis(c):
            fine = c - self.fine + self.cell * np.arange(len(self.gx))
            return np.unique(np.round(np.concatenate([fine, c + np.arange(-150.0, 151.0, 3.0), c + np.arange(-900.0, 901.0, 30.0)]), 4))

        xs, ys = axis(self.cx), axis(self.cy)
        nx, ny = len(xs), len(ys)
        X, Y = np.meshgrid(xs, ys, indexing="ij")
        Z = self.height(X, Y)
        col = np.zeros((nx, ny, 4), np.float32)
        col[..., 3] = 1.0
        i0, j0 = int(np.searchsorted(xs, self.gx[0] - 1e-4)), int(np.searchsorted(ys, self.gy[0] - 1e-4))
        n = len(self.gx)
        if np.allclose(xs[i0:i0 + n], self.gx, atol=1e-3) and np.allclose(ys[j0:j0 + n], self.gy, atol=1e-3):
            col[i0:i0 + n, j0:j0 + n, 0] = self.W
            col[i0:i0 + n, j0:j0 + n, 1] = self.C
            Z[i0:i0 + n, j0:j0 + n] -= 0.05 * self.R
        # (the ground is left open where a work is dug in)
        mx, my = 0.5 * (xs[:-1] + xs[1:]), 0.5 * (ys[:-1] + ys[1:])
        hole = np.zeros((nx - 1, ny - 1), bool)
        for d in self.digs:
            a0, a1 = np.searchsorted(mx, min(p[0] for p in d)), np.searchsorted(mx, max(p[0] for p in d))
            b0, b1 = np.searchsorted(my, min(p[1] for p in d)), np.searchsorted(my, max(p[1] for p in d))
            if a1 > a0 and b1 > b0:
                MX, MY = np.meshgrid(mx[a0:a1], my[b0:b1], indexing="ij")
                hole[a0:a1, b0:b1] |= inside_many(d, MX, MY)
        i, j = np.nonzero(~hole)
        v = i * ny + j
        me = mesh_of("ground", np.stack([X, Y, Z], -1).reshape(-1, 3), np.stack([v, v + ny, v + ny + 1, v + 1], 1))
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
        ca = me.color_attributes.new("worn", "FLOAT_COLOR", "POINT")
        ca.data.foreach_set("color", col.reshape(-1))
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

        def nz(scale, detail=6.0, rough=0.5):
            n = nt.nodes.new("ShaderNodeTexNoise")
            n.inputs["Scale"].default_value = scale
            n.inputs["Detail"].default_value = detail
            n.inputs["Roughness"].default_value = rough
            nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
            return n.outputs["Fac"]

        def ramp(fac, a, b, pa=0.35, pb=0.65):
            r = nt.nodes.new("ShaderNodeValToRGB")
            r.color_ramp.elements[0].position, r.color_ramp.elements[0].color = pa, (*kit.hex_rgb(a), 1)
            r.color_ramp.elements[1].position, r.color_ramp.elements[1].color = pb, (*kit.hex_rgb(b), 1)
            nt.links.new(fac, r.inputs["Fac"])
            return r.outputs["Color"]

        def mix(a, b, fac, blend="MIX"):
            n = nt.nodes.new("ShaderNodeMix")
            n.data_type, n.blend_type = "RGBA", blend
            if isinstance(fac, (int, float)):
                n.inputs[0].default_value = fac
            else:
                nt.links.new(fac, n.inputs[0])
            for k, v in ((6, a), (7, b)):
                if isinstance(v, str):
                    n.inputs[k].default_value = (*kit.hex_rgb(v), 1)
                else:
                    nt.links.new(v, n.inputs[k])
            return n.outputs[2]

        def calc(op, a, b=None, c=None, clamp=False):
            n = nt.nodes.new("ShaderNodeMath")
            n.operation, n.use_clamp = op, clamp
            for k, v in enumerate((a, b, c)):
                if v is None:
                    continue
                if isinstance(v, (int, float)):
                    n.inputs[k].default_value = v
                else:
                    nt.links.new(v, n.inputs[k])
            return n.outputs[0]

        def band(v, lo, hi):
            n = nt.nodes.new("ShaderNodeMapRange")
            n.interpolation_type = "SMOOTHSTEP"
            n.inputs["From Min"].default_value, n.inputs["From Max"].default_value = lo, hi
            nt.links.new(v, n.inputs["Value"])
            return n.outputs["Result"]

        big, mid, fine, grit = nz(0.025, 4), nz(0.15, 8), nz(2.5, 10), nz(7.0, 8, 0.7)
        if self.kind == "sand":
            grass = ramp(big, "C9B68A", "BFA878")
            dry = ramp(mid, "CDBB90", "B49C6C", 0.45, 0.7)
            earth = ramp(mid, "A8946A", "BCA67A", 0.3, 0.7)
            turned = ramp(grit, "8E7B58", "A8946A", 0.3, 0.7)
            base = mix(grass, dry, 0.45)
        else:
            # the field battle's own grass: two greens of late summer, dry patches, a fifth gone to straw, weeds
            grass = ramp(big, "6F7B42", "8D8650")
            dry = ramp(mid, "7A8048", "9C8C5E", 0.45, 0.7)
            base = mix(grass, dry, 0.45)
            straw = ramp(mid, "A8945F", "97875A", 0.35, 0.7)
            base = mix(base, straw, calc("MULTIPLY", band(nz(0.11, 5), 0.55, 0.68), 0.6))
            base = mix(base, "6B7640", calc("MULTIPLY", band(nz(0.6, 6), 0.56, 0.66), 0.55))
            earth = ramp(mid, "70604A", "8C7958", 0.3, 0.7)
            turned = ramp(grit, "5A4A3A", "7A6850", 0.3, 0.72)
        worn = nt.nodes.new("ShaderNodeVertexColor")
        worn.layer_name = "worn"
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(worn.outputs["Color"], sep.inputs[0])
        # (the worn edge breaks up: earth shows through in spots before it is all earth)
        ragged = calc("ADD", calc("MULTIPLY_ADD", nz(1.7, 9, 0.65), 0.8, 0.25), calc("MULTIPLY", nz(0.45, 4), 0.75))
        bare = band(calc("MULTIPLY", sep.outputs[0], ragged), 0.3, 0.55)
        fresh = band(calc("MULTIPLY", sep.outputs[1], calc("ADD", calc("MULTIPLY", grit, 0.7), calc("MULTIPLY_ADD", nz(1.1, 6), 0.9, 0.2))), 0.3, 0.75)
        base = mix(base, earth, bare)
        base = mix(base, turned, fresh)
        fr = ramp(fine, "8A8A8A", "FFFFFF", 0.25, 0.75)
        base = mix(base, fr, 0.5, "MULTIPLY")
        nt.links.new(base, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.97
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.4
        bump.inputs["Distance"].default_value = 0.05
        nt.links.new(fine, bump.inputs["Height"])
        rough = nt.nodes.new("ShaderNodeBump")                           # turned earth is lumpy
        rough.inputs["Distance"].default_value = 0.11
        nt.links.new(calc("MULTIPLY", calc("ADD", fresh, calc("MULTIPLY", bare, 0.35)), 0.9), rough.inputs["Strength"])
        nt.links.new(grit, rough.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], rough.inputs["Normal"])
        nt.links.new(rough.outputs["Normal"], bsdf.inputs["Normal"])
        return battle_set.haze(m)

    def _holes(self, depth):
        """Earth under every hole cut in the ground: its sides and a floor well below any work's own, so that where a
        work leaves its hole open (the end of a length of fieldworks, made to join the next) there is a face of earth,
        never the sky seen through the ground."""
        if not self.digs:
            return None
        verts, faces = [], []
        for d in self.digs:
            n, b = len(d), len(verts)
            verts += [(x, y, float(self.height(x, y)) + 0.01) for x, y in d] + [(x, y, -depth) for x, y in d]
            faces += [(b + k, b + (k + 1) % n, b + n + (k + 1) % n, b + n + k) for k in range(n)]
            faces.append(tuple(b + n + k for k in range(n)))
        me = bpy.data.meshes.new("holes")
        me.from_pydata(verts, [], faces)
        me.update()
        mat = bpy.data.materials.new("hole-earth")
        mat.use_nodes = True
        nt = mat.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value, nz.inputs["Detail"].default_value = 3.0, 8.0
        r = nt.nodes.new("ShaderNodeValToRGB")
        r.color_ramp.elements[0].position, r.color_ramp.elements[0].color = 0.3, (*kit.hex_rgb("5A4A3A"), 1)
        r.color_ramp.elements[1].position, r.color_ramp.elements[1].color = 0.72, (*kit.hex_rgb("7A6850"), 1)
        nt.links.new(nz.outputs["Fac"], r.inputs["Fac"])
        nt.links.new(r.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 1.0
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value, bump.inputs["Distance"].default_value = 0.6, 0.06
        nt.links.new(nz.outputs["Fac"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        me.materials.append(mat)
        ob = bpy.data.objects.new("holes", me)
        bpy.context.scene.collection.objects.link(ob)
        return ob

    # ---- what grows
    def _trees(self, hedges):
        """A wood's edge far off, hedgerows with their trees across the middle distance: each crown many lumpy clumps
        on a trunk and limbs, clothed in thousands of single leaves (broad cards far off, small leaves nearer), so
        a tree has a ragged outline and light in it, and is nowhere a green ball on a stick."""
        rnd = random.Random(self.seed)
        rng = np.random.default_rng(self.seed + 11)
        bm = bmesh.new()
        ball = bmesh.new()
        bmesh.ops.create_icosphere(ball, subdivisions=2, radius=1.0)
        bverts = [v.co.copy() for v in ball.verts]
        bfaces = [[v.index for v in f.verts] for f in ball.faces]
        ball.free()
        heart_faces = []
        clumps = []                                  # (centre, r, squash, leaves a square metre, (size lo, hi), tone)
        Z_ = Vector((0, 0, 1))

        def clump(c, r, squash, sd, dens, size, tone):
            vs = []
            for w in bverts:
                k = 0.8 * (1.0 + 0.3 * noise.noise(w * 1.4 + Vector((sd, sd * 0.37, 0.0))))
                vs.append(bm.verts.new(c + Vector((w.x * r * k, w.y * r * k, w.z * r * k * squash))))
            for f in bfaces:
                heart_faces.append(bm.faces.new([vs[i] for i in f]))
            clumps.append((c, r, squash, dens, size, tone))

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
                bm.faces.new((ra_[i], ra_[j], rb_[j], rb_[i]))

        def tree(at, h, sd, dens, size):
            g = random.Random(sd)
            trunk = h * g.uniform(0.2, 0.36)
            lean = Vector((g.uniform(-0.07, 0.07), g.uniform(-0.07, 0.07), 1.0)).normalized()
            top = at + lean * trunk
            limb(at - Z_ * 0.3, top, 0.036 * h, 0.022 * h)
            broad = g.uniform(0.85, 1.35)
            rx, rz = h * g.uniform(0.3, 0.38) * broad, h * g.uniform(0.3, 0.38)
            crown_c = at + lean * (trunk + rz * 0.62)
            off = Vector((g.uniform(-0.2, 0.2) * rx, g.uniform(-0.2, 0.2) * rx, 0.0))      # no crown sits square on its trunk
            tone = g.uniform(-0.2, 0.2)
            for k in range(g.randint(4, 6)):
                a = g.uniform(0, 2 * math.pi)
                tip = crown_c + off + Vector((math.cos(a) * rx * 0.6, math.sin(a) * rx * 0.6, g.uniform(-0.3, 0.2) * rz))
                limb(top - lean * g.uniform(0.0, 0.25) * trunk, tip, 0.016 * h, 0.006 * h, 5)
            for k in range(g.randint(10, 15)):
                # (clumps through the crown's egg: bigger at its heart, smaller at its skirts, some hanging low)
                a, e = g.uniform(0, 2 * math.pi), math.asin(g.uniform(-0.85, 1.0))
                d = 0.82 * g.random() ** 0.5
                c = crown_c + off * (1.0 - d) + Vector((math.cos(a) * math.cos(e) * rx * d, math.sin(a) * math.cos(e) * rx * d, math.sin(e) * rz * d))
                clump(c, min(rx, rz) * g.uniform(0.36, 0.52) * (1.15 - 0.4 * d), g.uniform(0.65, 0.95), sd * 3.1 + k * 7.7, dens, size, tone)

        def bush(at, h, sd):
            g = random.Random(sd)
            tone = g.uniform(-0.25, 0.1)
            for k in range(g.randint(2, 4)):
                r = h * g.uniform(0.4, 0.62)
                c = at + Vector((g.uniform(-0.6, 0.6), g.uniform(-0.6, 0.6), r * g.uniform(0.55, 1.0)))
                clump(c, r, g.uniform(0.7, 1.0), sd * 1.7 + k * 3.3, 30.0, (0.13, 0.27), tone)

        def ground_at(x, y):
            return Vector((x, y, self.height(x, y)))

        # the wood
        placed = 0
        for k in range(2600):
            if placed >= 190:
                break
            a = rnd.uniform(0, 2 * math.pi)
            r = rnd.uniform(self.flat + 75.0, self.flat + 260.0)
            x, y = self.cx + math.cos(a) * r, self.cy + math.sin(a) * r
            if 0.5 + 0.5 * noise.noise(Vector((x / 110.0, y / 110.0, 9.0 + self.seed))) < 0.58:
                continue
            tree(ground_at(x, y), rnd.uniform(9, 18), self.seed * 100 + k, 3.0, (0.5, 1.0))
            placed += 1
        if hedges:
            # field hedges: straight runs with gaps for gates, thick and thin, a tree standing in them here and there
            for k in range(5):
                a = math.radians(k * 72.0 + rnd.uniform(-22, 22))
                mid = Vector((self.cx + math.cos(a) * (self.flat + rnd.uniform(26, 52)), self.cy + math.sin(a) * (self.flat + rnd.uniform(26, 52))))
                run = math.radians(rnd.uniform(-28, 28)) + a + math.pi / 2
                along = Vector((math.cos(run), math.sin(run)))
                L = rnd.uniform(110, 210)
                s = -L / 2
                gate = rnd.uniform(-0.3, 0.3) * L
                while s < L / 2:
                    p = mid + along * s
                    thick = 0.5 + 0.5 * noise.noise(Vector((p.x / 14.0, p.y / 14.0, 3.0 + k)))
                    if abs(s - gate) > 2.4 and thick > 0.24:
                        bush(ground_at(p.x + rnd.uniform(-0.5, 0.5), p.y + rnd.uniform(-0.5, 0.5)), rnd.uniform(0.9, 2.4) * (0.6 + 0.9 * thick), self.seed * 50 + k * 1000 + int(s * 10))
                        if rnd.random() < 0.045:
                            tree(ground_at(p.x, p.y), rnd.uniform(7, 12), self.seed * 70 + k * 1000 + int(s * 10), 13.0, (0.2, 0.42))
                    s += rnd.uniform(1.0, 1.7)
        me = bpy.data.meshes.new("trees")
        bm.faces.index_update()
        heart = {f.index for f in heart_faces}
        bm.to_mesh(me)
        bm.free()
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
        me.polygons.foreach_set("material_index", [0 if i in heart else 1 for i in range(len(me.polygons))])
        me.update()
        ob = bpy.data.objects.new("trees", me)
        bpy.context.scene.collection.objects.link(ob)
        ob.data.materials.append(battle_set.haze(kit.material("crown", kit.hex_rgb("33421F"), rough=0.9, noise=0.5, noise_scale=3.0, bump=0.8)))
        ob.data.materials.append(battle_set.haze(kit.material("bark", kit.hex_rgb("4A3F33"), rough=0.95, noise=0.5, noise_scale=8, bump=0.5)))

        # the leaves: one diamond each, standing off the clump's skin, turned mostly outward and to the sky
        shades = [np.array(kit.hex_rgb(h)) for h in ("3C4E26", "51632F", "6A7A3A")]
        V, C = [], []
        for c, r, squash, dens, size, tone in clumps:
            k = max(8, int(4 * math.pi * r * r * (0.5 + 0.5 * squash) * dens))
            d = rng.normal(size=(k, 3))
            d /= np.linalg.norm(d, axis=1, keepdims=True)
            d = d[d[:, 2] > -0.55 - 0.3 * rng.random(len(d))]                      # few leaves under a clump
            k = len(d)
            p = np.array(c) + d * (r * (0.86 + 0.3 * rng.random((k, 1)))) * np.array([1.0, 1.0, squash])
            nrm = d * 0.7 + rng.normal(size=(k, 3)) * 0.45 + np.array([0.0, 0.0, 0.3])
            nrm /= np.linalg.norm(nrm, axis=1, keepdims=True)
            t = np.cross(nrm, rng.normal(size=(k, 3)))
            t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-9
            b = np.cross(nrm, t)
            a = rng.uniform(size[0], size[1], (k, 1)) * 0.5
            V.append(np.stack([p - t * a, p + b * a * 0.62, p + t * a * 1.1, p - b * a * 0.62], 1))
            f = np.clip(rng.random((k, 1)) * 0.9 + tone + 0.25 * d[:, 2:3], 0.0, 1.0) * 2.0
            col = np.where(f < 1.0, shades[0] * (1 - f) + shades[1] * f, shades[1] * (2 - f) + shades[2] * (f - 1))
            C.append(np.repeat(col[:, None, :], 4, 1))
        if V:
            V, C = np.concatenate(V).reshape(-1, 3), np.concatenate(C).reshape(-1, 3)
            lm = mesh_of("leaves", V, np.arange(len(V)).reshape(-1, 4))
            tinted(lm, "tint", C)
            lo = bpy.data.objects.new("leaves", lm)
            bpy.context.scene.collection.objects.link(lo)
            lo.data.materials.append(tint_material("leaf", rough=0.7, up=0.35, sheen=0.3))
        return ob

    def _blades(self, P, per, high, lean, wide, root, tip, rng, base_w=1.0, mid_w=0.65, mid_at=0.4, mid_h=0.75, droop=0.0, spread=0.07, flat_len=None):
        """Blades for the tufts at P (one row each): `per` blades a tuft, each a bent strip of two faces, leaning out
        `lean` times its height (or lying out `flat_len` metres, for a weed's leaf), drooping at the tip."""
        N = len(P)
        if not N:
            return np.zeros((0, 5, 3)), np.zeros((0, 5, 3))
        idx = np.repeat(np.arange(N), per)
        M = len(idx)
        ang = rng.uniform(0, 2 * math.pi, M)
        out = np.stack([np.cos(ang), np.sin(ang), np.zeros(M)], 1)
        side = np.stack([-np.sin(ang), np.cos(ang), np.zeros(M)], 1) * (wide * rng.uniform(0.7, 1.3, M))[:, None]
        h = (high[idx] * rng.uniform(0.55, 1.0, M))[:, None]
        ln = (h * rng.uniform(lean[0], lean[1], M)[:, None]) if flat_len is None else rng.uniform(flat_len[0], flat_len[1], M)[:, None]
        up = np.array([0.0, 0.0, 1.0])
        base = P[idx] + np.stack([rng.uniform(-spread, spread, M), rng.uniform(-spread, spread, M), np.full(M, -0.02)], 1)
        mid = base + out * ln * mid_at + up * h * mid_h
        top = base + out * ln + up * h * (1.0 - droop * rng.random(M)[:, None])
        V = np.stack([base - side * base_w, base + side * base_w, mid + side * mid_w, mid - side * mid_w, top], 1)
        r, t = root[idx], tip[idx]
        m = r * 0.3 + t * 0.7
        return V, np.stack([r, r, m, m, t], 1)

    def _grass(self, area, count):
        """A field, not a lawn: low splayed tufts that thin to nothing in bare patches and bunch in others, stands of
        long grass gone to straw with drooping heads, broad-leaved weeds; all of it giving way raggedly where the ground
        is worn. Every tuft has its own shade (carried on the blades as a colour, darker at the root)."""
        rng = np.random.default_rng(self.seed + 5)
        r = area * rng.random(count) ** 0.8                                # thicker toward the middle, where the camera looks
        a = rng.uniform(0, 2 * math.pi, count)
        x, y = self.cx + np.cos(a) * r, self.cy + np.sin(a) * r
        w, c = self._read(self.W, x, y), self._read(self.C, x, y)
        cl = 0.5 + 0.5 * self.clump(x, y)
        bare = smooth(self.bare(x, y) + 0.3 * self.f2(x, y), -0.55, -0.1)   # 0 in the thin patches
        keep = rng.random(count) < (0.1 + 1.05 * cl * cl) * (0.25 + 0.75 * bare)
        keep &= rng.random(count) > np.clip((w - 0.12) / 0.5, 0.0, 1.0) ** 0.7
        keep &= c < 0.3 + 0.2 * rng.random(count)
        for d in self.digs + self.clear:
            x0, x1, y0, y1 = min(p[0] for p in d) - 0.2, max(p[0] for p in d) + 0.2, min(p[1] for p in d) - 0.2, max(p[1] for p in d) + 0.2
            box = keep & (x > x0) & (x < x1) & (y > y0) & (y < y1)
            if box.any():
                keep[box] &= ~inside_many(d, x[box], y[box])
        x, y, w, cl, bare = x[keep], y[keep], w[keep], cl[keep], bare[keep]
        n = len(x)
        P = np.stack([x, y, self.height(x, y) - 0.05 * self._read(self.R, x, y)], 1)
        longness = smooth(self.long_(x, y) + 0.3 * self.f2(x, y), 0.1, 0.5) * (1.0 - smooth(w, 0.05, 0.3))
        pick = rng.random(n)
        is_long = pick < 0.42 * longness
        is_weed = (~is_long) & (pick > 0.984)
        is_short = ~(is_long | is_weed)
        if self.kind == "sand":
            g1, g2, straw, deep, weed = (np.array(kit.hex_rgb(h)) for h in ("8E8A52", "A39A62", "B9A870", "6F6C42", "77754A"))
        else:
            g1, g2, straw, deep, weed = (np.array(kit.hex_rgb(h)) for h in ("6F7B42", "8D8650", "B3A167", "5C6A38", "5E6E3E"))
        tone = (0.5 + 0.5 * self.tone(x, y))[:, None]
        shade = rng.uniform(0.82, 1.14, (n, 1))
        dry = np.clip(0.1 + 0.5 * self.long_(y, x)[:, None] + rng.uniform(-0.2, 0.3, (n, 1)), 0.0, 1.0)
        green = (g1 * (1 - tone) + g2 * tone)
        tip = (green * (1 - dry * 0.6) + straw * dry * 0.6) * shade
        root = (deep * 0.6 + green * 0.4) * shade * 0.9
        V, C = [], []
        s = is_short
        high = (0.045 + 0.075 * cl[s]) * (0.55 + 0.6 * bare[s]) * (1.0 - 0.55 * w[s])
        v, col = self._blades(P[s], 7, high, (0.7, 1.5), 0.0075, root[s], tip[s], rng)
        V.append(v), C.append(col)
        s = is_long
        high = (0.17 + 0.25 * rng.random(int(s.sum()))) * (0.7 + 0.5 * longness[s])
        heads = (straw * rng.uniform(0.9, 1.12, (int(s.sum()), 1)))
        v, col = self._blades(P[s], 10, high, (0.3, 1.05), 0.0075, root[s], heads * 0.5 + tip[s] * 0.5, rng, mid_at=0.3, mid_h=0.78, droop=0.4, spread=0.1)
        V.append(v), C.append(col)
        s = is_weed
        k = int(s.sum())
        wcol = weed * rng.uniform(0.8, 1.2, (k, 1))
        v, col = self._blades(P[s], 7, np.full(k, 0.07), None, 0.03, wcol * 0.8, wcol, rng, base_w=0.3, mid_w=1.0, mid_at=0.5, mid_h=1.0, droop=0.9, spread=0.02, flat_len=(0.09, 0.2))
        V.append(v), C.append(col)
        V, C = np.concatenate(V), np.concatenate(C)
        M = len(V)
        k = np.arange(M)[:, None] * 5
        me = mesh_of("grass", V.reshape(-1, 3), k + np.array([0, 1, 2, 3]), k + np.array([3, 2, 4]))
        tinted(me, "tint", C.reshape(-1, 3))
        ob = bpy.data.objects.new("grass", me)
        bpy.context.scene.collection.objects.link(ob)
        ob.data.materials.append(tint_material("grasstuft"))
        ob.visible_shadow = False
        return ob

    def _clods(self):
        """Lumps of earth and a few stones lying where the ground is turned or worn bare."""
        rng = np.random.default_rng(self.seed + 21)
        n = 26 * int((2 * self.fine) ** 2 / 10)
        x, y = self.cx + rng.uniform(-self.fine, self.fine, n), self.cy + rng.uniform(-self.fine, self.fine, n)
        w, c = self._read(self.W, x, y), self._read(self.C, x, y)
        keep = rng.random(n) < np.clip(c * 0.8 + np.clip(w - 0.55, 0.0, 1.0) * 0.35, 0.0, 1.0)
        for d in self.digs + self.clear:
            keep &= ~inside_many(d, x, y)
        x, y = x[keep][:12000], y[keep][:12000]
        k = len(x)
        if not k:
            return None
        ball = bmesh.new()
        bmesh.ops.create_icosphere(ball, subdivisions=1, radius=1.0)
        bv = np.array([v.co[:] for v in ball.verts])
        bf = np.array([[v.index for v in f.verts] for f in ball.faces])
        ball.free()
        r = (0.012 + 0.05 * rng.random(k) ** 2.2)[:, None, None]
        lump = 1.0 + 0.3 * rng.uniform(-1, 1, (k, len(bv), 1))
        V = bv[None] * r * lump * np.array([1.0, 1.0, 0.6])
        V[..., 0] += x[:, None]
        V[..., 1] += y[:, None]
        V[..., 2] += (self.height(x, y) - 0.05 * self._read(self.R, x, y))[:, None] + r[:, 0] * 0.15
        F = bf[None] + (np.arange(k) * len(bv))[:, None, None]
        me = mesh_of("clods", V.reshape(-1, 3), None, F.reshape(-1, 3))
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
        stone = rng.random(k) < 0.12
        tone = rng.uniform(0.75, 1.2, (k, 1))
        col = np.where(stone[:, None], np.array(kit.hex_rgb("8E877A")), np.array(kit.hex_rgb("6A5945"))) * tone
        tinted(me, "tint", np.repeat(col, len(bv), 0))
        ob = bpy.data.objects.new("clods", me)
        bpy.context.scene.collection.objects.link(ob)
        ob.data.materials.append(tint_material("clod", rough=0.97, up=0.0, sheen=0.1))
        return ob


def placed(outline, at=(0.0, 0.0), yaw=0.0):
    """A work's own outline (in its plan) where the work stands on the ground."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return [(at[0] + p[0] * c - p[1] * s, at[1] + p[0] * s + p[1] * c) for p in outline]


def approach(start, facing=(-1.0, 0.0), back=(-1.0, 0.0), long=11.0, seed=0):
    """The path men have trodden to a work: out of its door the way the door looks, then bending away to the rear
    (`back`: the way home, on the ground), wandering as a path does."""
    g = random.Random(seed)
    p = Vector((start[0], start[1]))
    f, b = Vector((facing[0], facing[1])).normalized(), Vector((back[0], back[1])).normalized()
    side = Vector((-b.y, b.x)) * g.choice((-1.0, 1.0))
    pts = [p, p + f * 1.5]
    q = pts[-1]
    for k, frac in enumerate((0.3, 0.6, 1.0)):
        pts.append(q + b * long * frac + side * g.uniform(0.4, 1.6) * (1 if k % 2 == 0 else -0.6))
    return [tuple(v) for v in pts]


def skirt(m, cell=0.7, reach=0.9):
    """Worn circles under and just beyond a work's own thrown earth (its faces painted earth or turf that lie near the
    ground), in its plan. The ground is bare there, so a bank runs into bare earth and the earth runs raggedly into
    grass: no work stands on a mat."""
    names = list(m.paints)
    want = {i for i, n in enumerate(names) if n.split(".")[0] == "earth"}
    turf = {i for i, n in enumerate(names) if n.split(".")[0] == "turf"}
    seen = {}
    for f, p in zip(m.faces, m.fpaint):
        if p in want:
            for i in f:
                v = m.verts[i]
                # (where the foot of a bank is turfed, the field's grass runs straight up onto it: no bare ring there)
                if -0.6 < v.z < 0.3 and m.vcoat[i] not in turf:
                    seen[(round(v.x / cell), round(v.y / cell))] = True
    return [(kx * cell, ky * cell, reach, 0.5) for kx, ky in seen]


def paths_of(m, seed=0):
    """The paths to a work, in its own plan: its own (`m.info["paths"]`), or else one from each door, or from its rear
    if men serve it."""
    if "paths" in m.info:
        return [list(p) for p in m.info["paths"]]
    out = []
    doors = [s for s in m.sockets if s["name"].split(".")[0] in ("door", "entrance")]
    for k, s in enumerate(doors):
        if s["at"].z > 1.5:
            continue
        f = s["facing"]
        out.append(approach((s["at"].x, s["at"].y), (f.x, f.y) if abs(f.x) + abs(f.y) > 0.2 else (-1.0, 0.0), seed=seed + k + len(m.name)))
    if not out and any(s["name"].startswith("crew") or s["name"].startswith("rifleman") for s in m.sockets):
        worn = m.info.get("worn", [])
        rear = min(worn, key=lambda c: c[0] - c[2]) if worn else (0.0, 0.0, m.info.get("span", 4.0) * 0.3)
        out.append(approach((rear[0] - rear[2] * 0.6, rear[1]), seed=seed + len(m.name)))
    return out


def tracks_of(m):
    """Wheel ruts to a work, in its own plan: its own (`m.info["tracks"]`), or else to the rear of a pit a gun was towed into."""
    if "tracks" in m.info:
        return [list(p) for p in m.info["tracks"]]
    return []
