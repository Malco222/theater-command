"""The field a land vehicle is filmed on, and what she does to the air.

Field: a meadow with a dirt road along x (two ruts where tracks have run), grass standing by the road, bushes, a far
treeline and low hills going blue with distance, under the warships' sky and sun.

Air: the warships' soft puffs (stage.Effects), sized for a tank: dust from the tracks, exhaust, the gun's flash and its
grey cloud and the dust it lifts, a strike, fire in tongues with black smoke, and the blast when her ammunition goes.
Smoke and flame are many small soft puffs and ragged tongues, never solid balls.
"""
import math
import random

import bpy
import numpy as np
from mathutils import Quaternion, Vector

import stage
from kit import hex_rgb
from works import _math, _mix, _noise, _ramp

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
HAZE = (0.74, 0.77, 0.80)
DUST = (0.60, 0.52, 0.40)

KINDS = {
    #          grass      dry grass   earth      road       rut        blade      blade tip   leaf
    "meadow": ("4E6030", "6F7440", "6A573C", "8C7A5A", "5C4E3C", "4F6A2A", "8E9A4A", "36481F"),
    "steppe": ("7C7A40", "A39656", "7A6444", "9A8660", "6A5A44", "6E7436", "B0A45C", "44502A"),
    "desert": ("B49A6A", "C2AA78", "A88C5E", "C4AE80", "9C845C", "8A8048", "B8A868", "5A5C34"),
}


def _smooth(nt, v, lo, hi, a=0.0, b=1.0):
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.interpolation_type = "SMOOTHSTEP"
    mr.inputs["From Min"].default_value, mr.inputs["From Max"].default_value = lo, hi
    mr.inputs["To Min"].default_value, mr.inputs["To Max"].default_value = a, b
    nt.links.new(v, mr.inputs["Value"])
    return mr.outputs["Result"]


def _haze(nt, col, far=1500.0, haze=HAZE):
    """A colour gone toward the sky's foot with distance from the camera."""
    cd = nt.nodes.new("ShaderNodeCameraData")
    f = _math(nt, "SUBTRACT", 1.0, _math(nt, "POWER", 2.718, _math(nt, "DIVIDE", cd.outputs["View Distance"], -far)), clamp=True)
    _, fac, a, b, o = _mix(nt)
    nt.links.new(f, fac)
    nt.links.new(col, a)
    b.default_value = (*haze, 1.0)
    return o, f


class Field:
    def __init__(self, kind="meadow", rut=1.225, rut_wide=0.44, road=2.7, grass=(-34.0, 46.0, -30.0, 30.0), density=16.0, trees=True, seed=3,
                 sun_elev=34.0, sun_turn=-38.0, strength=3.4, level=0.6, size=9000.0, clouds=True):
        self.kind, self.rut, self.road = kind, rut, road
        self.rng = random.Random(seed)
        self.np = np.random.default_rng(seed)
        self.cols = [hex_rgb(h) for h in KINDS[kind]]
        self.sun = stage.sky(sun_elev=sun_elev, sun_turn=sun_turn, strength=strength, level=level, clouds=clouds)
        self.coll = bpy.data.collections.new("field")
        bpy.context.scene.collection.children.link(self.coll)
        self._ground(size, rut, rut_wide, road)
        if density > 0:
            self._grass(grass, density, road)
        if trees:
            self._trees()
            self._hills()

    # -- the ground itself
    def _ground(self, size, rut, rut_wide, road):
        me = bpy.data.meshes.new("ground")
        h = size / 2
        me.from_pydata([(-h, -h, 0), (h, -h, 0), (h, h, 0), (-h, h, 0)], [], [(0, 1, 2, 3)])
        ob = bpy.data.objects.new("ground", me)
        self.coll.objects.link(ob)
        m = bpy.data.materials.new("ground")
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        pos = tc.outputs["Object"]
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(pos, sep.inputs[0])
        g, dry, earth, rd, rt = [(*c, 1.0) for c in self.cols[:5]]
        patch = _noise(nt, pos, 0.045, 3.0, 0.5)
        patch2 = _noise(nt, pos, 0.11, 4.0, 0.55, stretch=(1.0, 1.0, 1.0))
        fine = _noise(nt, pos, 1.7, 5.0, 0.6)
        grit = _noise(nt, pos, 14.0, 4.0, 0.6)
        # the meadow: green going dry in wide patches, bare earth showing here and there
        _, f1, a1, b1, o1 = _mix(nt)
        nt.links.new(_smooth(nt, patch, 0.38, 0.66), f1)
        a1.default_value, b1.default_value = g, dry
        _, f2, a2, b2, o2 = _mix(nt)
        nt.links.new(_smooth(nt, _math(nt, "ADD", patch2, _math(nt, "MULTIPLY", fine, 0.25)), 0.74, 0.86), f2)
        nt.links.new(o1, a2)
        b2.default_value = earth
        # the road: a ragged-edged strip of dry earth, two ruts where the tracks run, grass between them in places
        ay = _math(nt, "ABSOLUTE", sep.outputs[1])
        edge = _math(nt, "ADD", ay, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _noise(nt, pos, 0.55, 3.0), 0.5), 1.5))
        on_road = _smooth(nt, edge, road - 0.5, road + 0.5, 1.0, 0.0)
        _, f3, a3, b3, o3 = _mix(nt)
        nt.links.new(on_road, f3)
        nt.links.new(o2, a3)
        b3.default_value = rd
        in_rut = _smooth(nt, _math(nt, "ABSOLUTE", _math(nt, "SUBTRACT", ay, rut)), rut_wide * 0.42, rut_wide * 0.42 + 0.14, 1.0, 0.0)
        in_rut = _math(nt, "MULTIPLY", in_rut, _ramp(nt, _noise(nt, pos, 0.3, 2.0), 0.25, 0.6, 0.55, 1.0))
        _, f4, a4, b4, o4 = _mix(nt)
        nt.links.new(in_rut, f4)
        nt.links.new(o3, a4)
        b4.default_value = rt
        crown = _math(nt, "MULTIPLY", _smooth(nt, ay, 0.25, 0.75, 1.0, 0.0), _smooth(nt, _noise(nt, pos, 0.4, 3.0), 0.45, 0.6))
        _, f5, a5, b5, o5 = _mix(nt)
        nt.links.new(_math(nt, "MULTIPLY", crown, 0.8), f5)
        nt.links.new(o4, a5)
        b5.default_value = g
        # never one flat colour
        _, _, a6, b6, o6 = _mix(nt, "MULTIPLY", 1.0)
        nt.links.new(o5, a6)
        tone = nt.nodes.new("ShaderNodeValToRGB")
        tone.color_ramp.elements[0].position, tone.color_ramp.elements[0].color = 0.25, (0.78, 0.78, 0.76, 1)
        tone.color_ramp.elements[1].position, tone.color_ramp.elements[1].color = 0.75, (1.16, 1.15, 1.12, 1)
        nt.links.new(_math(nt, "ADD", _math(nt, "MULTIPLY", fine, 0.6), _math(nt, "MULTIPLY", grit, 0.4)), tone.inputs["Fac"])
        nt.links.new(tone.outputs["Color"], b6)
        col, far = _haze(nt, o6)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.96
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = 0.15
        # the lie of it: clods and grit everywhere, the prints of track links pressed into the ruts
        wave = nt.nodes.new("ShaderNodeTexWave")
        wave.wave_type, wave.bands_direction = "BANDS", "X"
        wave.inputs["Scale"].default_value = 0.31416 / 0.12       # (a band every 120 mm: a link's print)
        wave.inputs["Distortion"].default_value = 0.4
        nt.links.new(pos, wave.inputs["Vector"])
        height = _math(nt, "ADD", _math(nt, "MULTIPLY", grit, 0.5), _math(nt, "MULTIPLY", fine, 0.8))
        height = _math(nt, "SUBTRACT", height, _math(nt, "MULTIPLY", in_rut, _math(nt, "ADD", 0.9, _math(nt, "MULTIPLY", wave.outputs["Fac"], 0.5))))
        near = _math(nt, "SUBTRACT", 1.0, far, clamp=True)
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Distance"].default_value = 0.05
        nt.links.new(_math(nt, "MULTIPLY", near, 0.9), bump.inputs["Strength"])
        nt.links.new(height, bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        me.materials.append(m)
        self.ground = ob

    # -- grass that stands
    def _grass(self, box, density, road, blades=5):
        x0, x1, y0, y1 = box
        r = self.np
        n = int((x1 - x0) * (y1 - y0) * density)
        x, y = r.uniform(x0, x1, n), r.uniform(y0, y1, n)
        # none on the road (its edge wanders), thinner where the ground is dry
        wob = 0.9 * np.sin(x * 0.37) + 0.5 * np.sin(x * 1.3 + 1.0)
        keep = np.abs(y) > road + 0.15 + 0.4 * wob + r.uniform(0.0, 0.9, n)
        thin = 0.5 + 0.5 * np.sin(x * 0.11 + 0.7) * np.cos(y * 0.13)
        keep &= r.uniform(0, 1, n) < 0.55 + 0.45 * thin
        x, y = x[keep], y[keep]
        n = len(x)
        high = r.uniform(0.12, 0.40, n) ** 1.0 * (0.75 + 0.5 * np.sin(x * 0.21) * np.sin(y * 0.17 + 2.0)) * (0.7 + 0.6 * r.uniform(0, 1, n) ** 3)
        tone = r.uniform(0.0, 1.0, n)
        P, C, F = [], [], []
        base_col, tip_col = np.array(self.cols[5]), np.array(self.cols[6])
        for b in range(blades):
            a = r.uniform(0, 2 * np.pi, n)
            lean = r.uniform(0.15, 0.6, n) * high
            h = high * r.uniform(0.7, 1.15, n)
            w = r.uniform(0.010, 0.019, n)
            bx, by = x + r.uniform(-0.05, 0.05, n), y + r.uniform(-0.05, 0.05, n)
            ca, sa = np.cos(a), np.sin(a)
            # across the blade is at right angles to the way it leans
            px, py = -sa * w, ca * w
            v0 = np.stack([bx - px, by - py, np.zeros(n)], 1)
            v1 = np.stack([bx + px, by + py, np.zeros(n)], 1)
            v2 = np.stack([bx - px * 0.7 + ca * lean * 0.35, by - py * 0.7 + sa * lean * 0.35, h * 0.55], 1)
            v3 = np.stack([bx + px * 0.7 + ca * lean * 0.35, by + py * 0.7 + sa * lean * 0.35, h * 0.55], 1)
            v4 = np.stack([bx + ca * lean, by + sa * lean, h], 1)
            k = len(P) * 0 + (b * n * 5)
            P.append(np.stack([v0, v1, v2, v3, v4], 1).reshape(-1, 3))
            mix = (0.25 + 0.75 * tone)[:, None]
            cb = base_col[None, :] * (0.55 + 0.25 * tone[:, None])
            ct = base_col[None, :] * (1 - mix) + tip_col[None, :] * mix
            cm = (cb + ct) / 2
            C.append(np.stack([cb, cb, cm, cm, ct], 1).reshape(-1, 3))
            i = k + np.arange(n) * 5
            F.append(np.stack([i, i + 1, i + 3, i, i + 3, i + 2, i + 2, i + 3, i + 4], 1).reshape(-1, 3))
        P, C, F = np.concatenate(P), np.concatenate(C), np.concatenate(F)
        me = bpy.data.meshes.new("grass")
        me.vertices.add(len(P))
        me.vertices.foreach_set("co", P.ravel())
        me.loops.add(F.size)
        me.loops.foreach_set("vertex_index", F.ravel().astype(np.int32))
        me.polygons.add(len(F))
        me.polygons.foreach_set("loop_start", np.arange(len(F), dtype=np.int32) * 3)
        me.polygons.foreach_set("loop_total", np.full(len(F), 3, dtype=np.int32))
        me.update()
        me.validate()
        att = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
        att.data.foreach_set("color", np.concatenate([C, np.ones((len(C), 1))], 1).ravel())
        m = bpy.data.materials.new("grass")
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        at = nt.nodes.new("ShaderNodeAttribute")
        at.attribute_name = "Col"
        nt.links.new(at.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.7
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = 0.2
        # (a blade is lit from both sides: give it the ground's facing, so a field of them shades as one soft surface)
        nrm = nt.nodes.new("ShaderNodeNormal")
        nrm.outputs[0].default_value = (0.0, 0.0, 1.0)
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        mixn = nt.nodes.new("ShaderNodeMix")
        mixn.data_type = "VECTOR"
        mixn.inputs[0].default_value = 0.7
        nt.links.new(geo.outputs["Normal"], mixn.inputs[4])
        mixn.inputs[5].default_value = (0.0, 0.0, 1.0)
        nt.links.new(mixn.outputs[1], bsdf.inputs["Normal"])
        me.materials.append(m)
        ob = bpy.data.objects.new("grass", me)
        ob.visible_shadow = False
        self.coll.objects.link(ob)
        self.grass = ob
        self.blades = len(F) // 3

    # -- what stands far off
    def _leaves(self, name, colour, far, clump):
        """Leaves: lumpy, never one green, and ragged at the edge of every mass (the outline is eaten away in clumps
        of leaves, so a crown has a broken edge against the sky and not a ball's)."""
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        out = nt.nodes["Material Output"]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        pos = tc.outputs["Object"]
        n1 = _noise(nt, pos, 0.16, 3.0, 0.55)
        leaf = _noise(nt, pos, 1.0 / clump, 3.0, 0.7)
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        c = np.array(colour)
        ramp.color_ramp.elements[0].position, ramp.color_ramp.elements[0].color = 0.3, (*(c * 0.6), 1)
        ramp.color_ramp.elements[1].position, ramp.color_ramp.elements[1].color = 0.72, (*(c * 1.3 + np.array([0.02, 0.02, 0.0])), 1)
        nt.links.new(_math(nt, "ADD", _math(nt, "MULTIPLY", n1, 0.6), _math(nt, "MULTIPLY", leaf, 0.4)), ramp.inputs["Fac"])
        col, _ = _haze(nt, ramp.outputs["Color"], far)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.85
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = 0.12
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.9
        bump.inputs["Distance"].default_value = clump * 0.6
        nt.links.new(leaf, bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        lw = nt.nodes.new("ShaderNodeLayerWeight")
        lw.inputs["Blend"].default_value = 0.5
        face = _math(nt, "SUBTRACT", 1.0, lw.outputs["Facing"], clamp=True)          # 1 looking into it, 0 at its edge
        need = _ramp(nt, face, 0.0, 0.62, 0.66, 0.22)                                # (how much leaf it takes to be solid here)
        solid = _math(nt, "GREATER_THAN", leaf, need)
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        ms = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(solid, ms.inputs[0])
        nt.links.new(clear.outputs[0], ms.inputs[1])
        nt.links.new(bsdf.outputs[0], ms.inputs[2])
        nt.links.new(ms.outputs[0], out.inputs["Surface"])
        return m

    def _blobs(self, name, spots, colour, far=1500.0, sub=2, clump=0.35):
        """Round leafy masses (the lobes of a tree's crown, a bush): spots = [(x, y, z, rx, ry, rz)]."""
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=sub, radius=1.0)
        ball = bpy.context.object
        bv = np.array([v.co[:] for v in ball.data.vertices])
        bf = np.array([p.vertices[:] for p in ball.data.polygons])
        bpy.data.objects.remove(ball)
        r = self.np
        P, F = [], []
        for k, (x, y, z, rx, ry, rz) in enumerate(spots):
            lump = 1.0 + 0.16 * np.sin(bv[:, 0] * r.uniform(2, 4) + r.uniform(0, 6)) * np.sin(bv[:, 1] * r.uniform(2, 4) + r.uniform(0, 6)) + 0.1 * np.sin(bv[:, 2] * r.uniform(3, 6) + r.uniform(0, 6))
            v = bv * lump[:, None] * np.array([rx, ry, rz])[None, :]
            a = r.uniform(0, 6.28)
            ca, sa = math.cos(a), math.sin(a)
            v = np.stack([v[:, 0] * ca - v[:, 1] * sa, v[:, 0] * sa + v[:, 1] * ca, v[:, 2]], 1) + np.array([x, y, z])[None, :]
            P.append(v)
            F.append(bf + k * len(bv))
        P, F = np.concatenate(P), np.concatenate(F)
        me = bpy.data.meshes.new(name)
        me.from_pydata(P.tolist(), [], F.tolist(), shade_flat=False)
        me.materials.append(self._leaves(name, colour, far, clump))
        ob = bpy.data.objects.new(name, me)
        self.coll.objects.link(ob)
        return ob

    def _tree(self, x, y, h, w, lobes):
        """A broad-leaved tree: its crown is a cluster of lobes round a middle. Returns (lobes, trunk)."""
        g = self.rng
        cz = h * 0.64
        out = [(x, y, cz, w * 0.62, w * 0.62, h * 0.3)]
        for i in range(lobes):
            a = g.uniform(0, 6.283)
            d = g.uniform(0.35, 0.62) * w
            rr = g.uniform(0.4, 0.62) * w
            out.append((x + d * math.cos(a), y + d * math.sin(a), cz + g.uniform(-0.2, 0.26) * h, rr, rr * g.uniform(0.85, 1.15), rr * g.uniform(0.7, 0.95)))
        return out, (x, y, h * 0.5, g.uniform(0.018, 0.028) * h)

    def _trunks(self, trunks):
        verts, faces = [], []
        for x, y, h, r in trunks:
            k = len(verts)
            for z, rr in ((0.0, r * 1.3), (h, r * 0.7)):
                verts += [(x + rr * math.cos(i * math.pi / 3), y + rr * math.sin(i * math.pi / 3), z) for i in range(6)]
            faces += [(k + i, k + (i + 1) % 6, k + 6 + (i + 1) % 6, k + 6 + i) for i in range(6)]
        me = bpy.data.meshes.new("trunks")
        me.from_pydata(verts, [], faces, shade_flat=False)
        m = bpy.data.materials.new("trunks")
        m.use_nodes = True
        m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.09, 0.07, 0.055, 1.0)
        m.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.95
        me.materials.append(m)
        ob = bpy.data.objects.new("trunks", me)
        self.coll.objects.link(ob)
        return ob

    def _trees(self):
        g = self.rng
        leaf = self.cols[7]
        far_spots, near_spots, trunks = [], [], []
        # a wood along each side of the valley, its edge wandering; thicker behind
        for side in (1.0, -1.0):
            for row, (dist, n) in enumerate(((150.0, 70), (172.0, 80), (200.0, 80), (240.0, 70))):
                for i in range(n):
                    x = -520.0 + 1040.0 * (i + g.uniform(0, 1)) / n
                    y = side * (dist + 28.0 * math.sin(x * 0.011 + side) + 16.0 * math.sin(x * 0.037 + 2.0 * side) + g.uniform(-8, 8))
                    if g.random() < 0.12 and row == 0:
                        continue
                    lobes, trunk = self._tree(x, y, g.uniform(11.0, 19.0), g.uniform(4.5, 7.5), 4)
                    far_spots += lobes
                    if row == 0:
                        trunks.append(trunk)
        # a copse or two nearer, and trees standing alone
        for cx, cy, n in ((-58.0, 62.0, 7), (-95.0, -70.0, 9), (120.0, 85.0, 8), (150.0, -75.0, 6), (-30.0, 96.0, 3), (60.0, -110.0, 4)):
            for i in range(n):
                lobes, trunk = self._tree(cx + g.uniform(-16, 16), cy + g.uniform(-11, 11), g.uniform(10.0, 17.0), g.uniform(4.0, 6.5), 8)
                near_spots += lobes
                trunks.append(trunk)
        self.trees = self._blobs("trees", far_spots, leaf, sub=2, clump=0.9)
        self.copses = self._blobs("copses", near_spots, leaf, sub=3, clump=0.42)
        self._trunks(trunks)
        # a broken hedge along each side, and bushes here and there
        spots = []
        for i in range(120):
            x = g.uniform(-170, 220)
            side = g.choice((1.0, -1.0))
            hedge = i < 80
            if hedge and (int((x + 400) / 26.0) % 4 == 0):
                continue
            y = side * (46.0 + 4.0 * math.sin(x * 0.045) + g.uniform(-0.8, 0.8)) if hedge else side * g.uniform(38, 130)
            w = g.uniform(0.8, 1.5)
            for k in range(3):
                spots.append((x + g.uniform(-0.8, 0.8), y + g.uniform(-0.5, 0.5), w * g.uniform(0.5, 0.9), w * g.uniform(0.8, 1.2), w * g.uniform(0.8, 1.1), w * g.uniform(0.7, 1.0)))
        self.bushes = self._blobs("bushes", spots, tuple(c * 1.12 for c in leaf), sub=3, clump=0.22)

    def _hills(self, far=2600.0):
        g = self.rng
        k = 240
        a0 = [g.uniform(0, 6.28) for _ in range(4)]
        verts, faces = [], []
        for i in range(k):
            a = 2 * math.pi * i / k
            h = 70.0 + 46.0 * math.sin(3 * a + a0[0]) + 30.0 * math.sin(7 * a + a0[1]) + 14.0 * math.sin(17 * a + a0[2]) + 6.0 * math.sin(41 * a + a0[3])
            verts += [(far * math.cos(a), far * math.sin(a), -5.0), (far * math.cos(a), far * math.sin(a), max(h, 12.0))]
        for i in range(k):
            j = (i + 1) % k
            faces.append((2 * i, 2 * i + 1, 2 * j + 1, 2 * j))
        me = bpy.data.meshes.new("hills")
        me.from_pydata(verts, [], faces)
        m = bpy.data.materials.new("hills")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.remove(nt.nodes["Principled BSDF"])
        em = nt.nodes.new("ShaderNodeEmission")
        c = np.array(self.cols[7]) * 0.9
        hz = np.array(HAZE)
        em.inputs["Color"].default_value = (*(c * 0.22 + hz * 0.78), 1.0)
        em.inputs["Strength"].default_value = 1.0
        nt.links.new(em.outputs[0], nt.nodes["Material Output"].inputs["Surface"])
        me.materials.append(m)
        ob = bpy.data.objects.new("hills", me)
        ob.visible_shadow = False
        self.coll.objects.link(ob)
        self.hills = ob


# ---- the air round a vehicle -------------------------------------------------------------------------

class Battle:
    """The field battle's own ground and trees, so a vehicle is judged where she will stand: the soldier thread's
    preview stage (battle_set.py, copied here unchanged but for the name of the kit it imports), under this folder's sky.

    Every vehicle here stands at the origin, heading +x, with the ground at z = 0. So the stage is laid as it is, and then
    turned and moved until a stretch of its dirt road runs along +x through the origin. Her tracks and wheels are rigid,
    so the ground round that stretch is levelled (`flat`: level out to the first distance, the stage's own rolling ground
    beyond the second); two ruts with the prints of track links are laid along it, which the stage does not have.

    stand: where on the stage she halts (the road's point at that y); toward: -1 heading south down the road, +1 north."""

    def __init__(self, rut=1.225, rut_wide=0.44, stand=18.0, toward=-1.0, seed=3, sun_elev=34.0, sun_turn=-38.0, strength=3.4, level=0.6,
                 clouds=True, flat=(30.0, 66.0), behind=48.0, ahead=18.0, ruts=True, pitch=0.12, **_):
        import battle_set
        self.kind, self.rut = "battle", rut
        self.dust_hex = "9A8A6A"                       # (the stage's road)
        self.sun = stage.sky(sun_elev=sun_elev, sun_turn=sun_turn, strength=strength, level=level, clouds=clouds)
        road = battle_set.ROAD
        (ax, ay), (bx, by) = next(((a, b) for a, b in zip(road, road[1:]) if a[1] <= stand <= b[1]), (road[0], road[1]))
        f = (stand - ay) / (by - ay)
        sx, sy = ax + (bx - ax) * f, stand
        u = Vector((bx - ax, by - ay)).normalized() * (1.0 if toward > 0 else -1.0)
        plain = battle_set._ground_height
        h0 = plain(sx, sy, [])

        def levelled(x, y, rd):
            h = plain(x, y, rd)
            t = min(max((x - sx) * u.x + (y - sy) * u.y, -behind), ahead)
            d = math.hypot(x - (sx + u.x * t), y - (sy + u.y * t))
            k = min(max((d - flat[0]) / (flat[1] - flat[0]), 0.0), 1.0)
            return h0 + (h - h0) * k * k * (3.0 - 2.0 * k)
        had = {o.name for o in bpy.data.objects}
        battle_set._ground_height = levelled
        try:
            self.stage = battle_set.build(seed)
        finally:
            battle_set._ground_height = plain
        root = bpy.data.objects.new("battle_stage", None)
        bpy.context.scene.collection.objects.link(root)
        for o in bpy.data.objects:
            if o.name not in had and o.parent is None and o is not root:
                o.parent = root
        ang = math.atan2(u.y, u.x)
        turn = Quaternion(Z, -ang)
        root.rotation_euler = (0.0, 0.0, -ang)
        root.location = -(turn @ Vector((sx, sy, h0 + 0.03)))        # (the road lies 3 cm proud of the ground)
        self.root, self._turn, self._at = root, turn, Vector((sx, sy, h0 + 0.03))
        if ruts:
            self._ruts(rut, rut_wide, -behind, ahead, pitch)

    def here(self, x, y, z=0.0):
        """A place on the stage (its own x, y, and a height above its ground there) in the picture's axes."""
        return self._turn @ (Vector((x, y, self.stage.height(x, y) + z)) - self._at)

    def _ruts(self, rut, wide, x0, x1, pitch):
        me = bpy.data.meshes.new("ruts")
        w = wide * 0.75
        vs, fs = [], []
        for sgn in (1.0, -1.0):
            k = len(vs)
            vs += [(x0, sgn * rut - w, 0.006), (x1, sgn * rut - w, 0.006), (x1, sgn * rut + w, 0.006), (x0, sgn * rut + w, 0.006)]
            fs.append((k, k + 1, k + 2, k + 3))
        me.from_pydata(vs, [], fs)
        ob = bpy.data.objects.new("ruts", me)
        bpy.context.scene.collection.objects.link(ob)
        ob.visible_shadow = False
        m = bpy.data.materials.new("ruts")
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        pos = tc.outputs["Object"]
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(pos, sep.inputs[0])
        wave = nt.nodes.new("ShaderNodeTexWave")
        wave.wave_type, wave.bands_direction = "BANDS", "X"
        wave.inputs["Scale"].default_value = 0.31416 / pitch
        wave.inputs["Distortion"].default_value = 0.6
        wave.inputs["Detail"].default_value = 1.0
        nt.links.new(pos, wave.inputs["Vector"])
        prints = _smooth(nt, wave.outputs["Fac"], 0.35, 0.75)
        across = _math(nt, "ABSOLUTE", _math(nt, "SUBTRACT", _math(nt, "ABSOLUTE", sep.outputs[1]), rut))
        ragged = _math(nt, "ADD", across, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _noise(nt, pos, 2.2, 3.0, 0.6), 0.5), wide * 0.5))
        inside = _smooth(nt, ragged, wide * 0.3, wide * 0.6, 1.0, 0.0)
        ends = _math(nt, "MULTIPLY", _smooth(nt, sep.outputs[0], x0, x0 + 8.0), _smooth(nt, sep.outputs[0], x1 - 6.0, x1, 1.0, 0.0))
        patch = _smooth(nt, _noise(nt, pos, 0.5, 3.0, 0.5), 0.3, 0.7, 0.55, 1.0)
        a = _math(nt, "MULTIPLY", _math(nt, "ADD", 0.3, _math(nt, "MULTIPLY", prints, 0.45)), _math(nt, "MULTIPLY", inside, _math(nt, "MULTIPLY", ends, patch)))
        nt.links.new(a, bsdf.inputs["Alpha"])
        r, g, b = hex_rgb(self.dust_hex)
        bsdf.inputs["Base Color"].default_value = (r * 0.42, g * 0.4, b * 0.38, 1.0)
        bsdf.inputs["Roughness"].default_value = 1.0
        ob.data.materials.append(m)


# The 75 mm's flash as lamps (Blender's watts: the sun here gives the ground about 3.4 a square metre, and a lamp of P
# watts gives P / (39.5 d d) at d metres). The art director: a fifth brighter at most on the faces that see the muzzle,
# a warm pool on the ground under it, never the whole field.
FLASH_TOUCH = 230.0     # the lamp at the muzzle
FLASH_POOL = 500.0      # the lamp that throws the pool straight down


class Air(stage.Effects):
    """stage.Effects with a land vehicle's effects. Places and facings are in the world's axes."""

    def __init__(self, seed=7, wind=(-1.4, 1.9, 0.0), dust=DUST):
        super().__init__(seed=seed, wind=wind)
        self.dust_col = Vector(dust)

    def _dust_puff(self, t, at, vel, big=1.0, thick=1.0, life=None):
        g = self.rng
        c = g.uniform(0.9, 1.1)
        return self.puff(t, at, vel=vel, life=life or g.uniform(2.6, 4.6), r=(0.22 * big, g.uniform(1.0, 1.8) * big), col=self.dust_col * c, col2=self.dust_col * (c * 1.06),
                         alpha=g.uniform(0.2, 0.34) * thick, drag=1.8, rise=0.22, fade_in=0.12, grow=0.6, wind=1.0, hold=0.15)

    def dust(self, where, speed, start=0.0, end=1e9, thick=1.0):
        """What her tracks or wheels lift: more the faster she goes. `speed` is m/s, or a function of time."""
        g = self.rng

        def make(t, at, facing):
            v = abs(speed(t) if callable(speed) else speed)
            if v < 0.4:
                return
            k = min(1.0, v / 7.0)
            if g.random() > 0.35 + 0.65 * k:
                return
            side = facing.cross(Z)
            self._dust_puff(t, at + side * g.uniform(-0.2, 0.2) + Z * g.uniform(0.05, 0.3), facing * v * g.uniform(0.1, 0.35) + Z * g.uniform(0.2, 0.9) * k + side * g.uniform(-0.4, 0.4),
                            big=0.6 + 0.6 * k, thick=thick * (0.5 + 0.5 * k))
        self.stream(where, 0.05, make, start=start, end=end)

    def exhaust(self, where, rev=1.0, start=0.0, end=1e9):
        """Her engine's breath: thin and blue-grey, thicker and darker when she is opened up. `rev` 0..1, or a function of time."""
        g = self.rng

        def make(t, at, facing):
            k = rev(t) if callable(rev) else rev
            if k <= 0.0:
                return
            c = g.uniform(0.34, 0.44) - 0.2 * max(0.0, k - 0.6)
            self.puff(t, at, vel=facing * g.uniform(2.0, 4.0) * (0.5 + k) + Z * g.uniform(0.2, 0.8), life=g.uniform(0.8, 1.5), r=(0.05, g.uniform(0.3, 0.5) * (0.6 + 0.6 * k)),
                      col=(c * 0.95, c * 0.98, c * 1.05), alpha=g.uniform(0.1, 0.2) * (0.5 + k), drag=3.0, rise=0.5, fade_in=0.05, grow=0.6, wind=1.0)
        self.stream(where, 0.045, make, start=start, end=end)

    def cough(self, t0, where):
        """She is started, or opened up from rest: a dark burst from the exhaust."""
        g = self.rng

        def make(t, at, facing):
            for i in range(7):
                c = g.uniform(0.1, 0.2)
                self.puff(t + 0.03 * i, at, vel=facing * g.uniform(3.0, 7.0) + Z * g.uniform(0.3, 1.5) + Vector((g.uniform(-0.6, 0.6), g.uniform(-0.6, 0.6), 0)), life=g.uniform(1.6, 2.8),
                          r=(0.08, g.uniform(0.55, 0.95)), col=(c, c, c * 1.05), alpha=g.uniform(0.35, 0.5), drag=3.0, rise=0.6, fade_in=0.04, grow=0.55, wind=1.0)
        self.once(t0, where, make)

    def spot(self, t0, at, power, long=0.1, col=(1.0, 0.6, 0.28), cone=110.0, r=0.3):
        """A flash's light thrown straight down in a cone: a warm pool on the ground under it and nothing beyond."""
        d = bpy.data.lights.new("flash_pool", "SPOT")
        d.energy, d.color, d.shadow_soft_size = 0.0, col, r
        d.spot_size, d.spot_blend = math.radians(cone), 1.0
        ob = bpy.data.objects.new("flash_pool", d)
        self.coll.objects.link(ob)
        ob.location = at                                  # (a lamp looks down its own -z: as made, straight down)
        self.lights.append(dict(ob=ob, t0=t0, long=long, power=power))

    def gun(self, t0, where, bore=0.075, brake=True, light=True, ground=True):
        """Her main gun fires. Through a muzzle brake the flash is thrown out sideways: two short yellow-white jets left
        and right of the brake and only a small tongue ahead. A plain muzzle gives a short cone ahead. Orange at the
        edges, bright for an instant and gone in a tenth of a second; then the grey-brown cloud along the bore, and the
        dust the blast lifts from the ground under it. Its light is small and warm: a pool on the ground under the
        muzzle, a touch on the faces that see it, never the whole field."""
        g = self.rng
        k = (bore / 0.075) ** 0.8

        def make(t, at, facing):
            side = facing.cross(Z).normalized()
            up = side.cross(facing)

            def tongues(n, way, spread, long0, long1, root, thick=1.0):
                # (flame in ragged tongues, each its own length and shade: never one solid shape)
                for i in range(n):
                    long = g.uniform(long0, long1) * k
                    d = (way + side * g.uniform(-spread, spread) + up * g.uniform(-spread * 0.8, spread)).normalized()
                    kk = g.uniform(0.85, 1.2)
                    wide = (0.1 * long + 0.035 * k) * thick
                    p = self.puff(t, root + d * (0.5 * long + 0.03), vel=d * g.uniform(3.0, 8.0), life=g.uniform(0.1, 0.15), r=(0.8, 1.0), col=(0.27 * kk, 0.078 * kk, 0.009),
                                  col2=(0.2, 0.035, 0.005), alpha=0.95, kind="tongue" if i % 4 else "lick", wind=0.0,
                                  squash=(0.5 * long, wide, wide), fade_in=0.002, grow=0.5, hold=0.3, cool=0.9)
                    p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion(X, g.uniform(0, 6.283))).to_euler()

            def heart(root, way, long, r):
                # (the yellow-white heart of a jet: small, and gone before the orange round it)
                p = self.puff(t, root + way * long * 0.4, vel=way * 2.0, life=0.075, r=(r, r * 1.5), col=(0.75, 0.56, 0.26), col2=(0.7, 0.3, 0.07), alpha=0.8, kind="glow", wind=0.0,
                              squash=(long / r * 0.5, 1.0, 1.0), fade_in=0.002, grow=0.4, hold=0.3)
                p["ob"].rotation_euler = way.to_track_quat("X", "Z").to_euler()

            if brake:                # out of each side of the brake, and a small tongue ahead
                vent = at - facing * 0.14 * k
                for hand in (1.0, -1.0):
                    d = (side * hand + facing * 0.22 + up * 0.05).normalized()
                    tongues(5, d, 0.15, 0.65, 1.3, vent + d * 0.05)
                    heart(vent + d * 0.06, d, 0.5 * k, 0.085 * k)
                tongues(2, facing, 0.1, 0.25, 0.5, at, thick=0.8)
                heart(at, facing, 0.18 * k, 0.05 * k)
            else:                    # a plain muzzle: a short cone ahead
                tongues(8, facing, 0.24, 0.4, 1.05, at, thick=1.15)
                heart(at, facing, 0.42 * k, 0.1 * k)
            if brake:                # the gas that follows the flash out of the brake, to each side
                for hand in (1.0, -1.0):
                    d = (side * hand + facing * 0.35 + up * 0.08).normalized()
                    for i in range(5):
                        c = g.uniform(0.62, 0.78)
                        self.puff(t + 0.02, at, vel=d * g.uniform(8, 22) * k + up * g.uniform(-1, 3), life=g.uniform(1.8, 3.2), r=(0.16 * k, g.uniform(0.8, 1.4) * k), col=(0.6, 0.5, 0.36),
                                  col2=(c, c * 0.97, c * 0.92), alpha=g.uniform(0.2, 0.32), drag=g.uniform(4.5, 6.5), rise=0.35, fade_in=0.03, grow=0.5, wind=1.0)
            for i in range(11):      # the smoke: thrown out along the bore, then it hangs, thins and drifts
                sp = g.uniform(8.0, 34.0) * k
                v = facing * sp + side * g.uniform(-4, 4) * k + up * g.uniform(-1.5, 3.5) * k
                c = g.uniform(0.8, 0.95)
                self.puff(t + 0.02, at + facing * 0.3, vel=v, life=g.uniform(2.6, 4.6), r=(0.2 * k, g.uniform(1.0, 1.9) * k), col=(0.8, 0.64, 0.42), col2=(c, c * 0.98, c * 0.95),
                          alpha=g.uniform(0.34, 0.5), drag=g.uniform(3.6, 5.2), rise=0.4, fade_in=0.03, grow=0.5, wind=1.0)
            if ground:               # the blast beats the ground: dust jumps up under the muzzle and runs out ahead
                foot = Vector((at.x, at.y, 0.0))
                for i in range(22):
                    out = facing * g.uniform(-1.0, 6.5) * k + side * g.uniform(-3.2, 3.2) * k
                    out.z = 0.0
                    away = (out - facing * 0.5)
                    away = away.normalized() if away.length > 0.05 else facing
                    self._dust_puff(t + 0.03 + 0.012 * out.length, foot + out + Z * 0.12, away * g.uniform(1.5, 6.0) + Z * g.uniform(0.4, 2.2), big=g.uniform(0.7, 1.25) * k, thick=1.25,
                                    life=g.uniform(2.6, 4.8))
            if light:
                # (one frame, warm, short of reach: faces that see the muzzle brighten by a fifth at most, the ground
                # under it gets a warm pool)
                self.light(t, at + facing * 0.25 * k + Z * 0.1, FLASH_TOUCH * k * k, 0.09, col=(1.0, 0.6, 0.28), r=0.35)
                self.spot(t, at + Z * 0.05, FLASH_POOL * k * k, 0.09)
        self.once(t0, where, make)

    def shaken(self, t0, spots, thick=1.0):
        """The shock of her own gun (or of a strike) shakes the dust off her: little puffs along her sides."""
        g = self.rng

        def make(t, at, facing):
            for where in spots:
                w = where() if callable(where) else where
                p = Vector(w[0])
                for i in range(3):
                    self._dust_puff(t + g.uniform(0.0, 0.08), p + Vector((g.uniform(-1.2, 1.2), g.uniform(-0.2, 0.2), g.uniform(0.0, 0.5))), Vector((g.uniform(-0.6, 0.6), g.uniform(-0.8, 0.8), g.uniform(0.2, 0.9))),
                                    big=0.55, thick=0.8 * thick, life=g.uniform(1.6, 2.6))
        self.once(t0, (Vector(), X), make)

    def mg(self, t0, where, rounds=10, every=0.075):
        """A machine gun's burst: a flicker at the muzzle, a wisp of smoke."""
        g = self.rng

        def one(t, at, facing):
            p = self.puff(t, at + facing * 0.12, vel=facing * 3.0, life=0.045, r=(0.05, g.uniform(0.12, 0.2)), col=(1.0, 0.78, 0.4), col2=(1.0, 0.45, 0.12), alpha=1.0, kind="glow", wind=0.0,
                          squash=(2.0, 1.0, 1.0), fade_in=0.005, grow=0.4)
            p["ob"].rotation_euler = facing.to_track_quat("X", "Z").to_euler()
            if g.random() < 0.5:
                c = g.uniform(0.6, 0.75)
                self.puff(t, at + facing * 0.15, vel=facing * g.uniform(1.5, 4.0), life=g.uniform(0.7, 1.2), r=(0.04, g.uniform(0.2, 0.34)), col=(c, c, c), alpha=0.14, drag=4.0, rise=0.3, wind=1.0)
        for i in range(rounds):
            self.once(t0 + i * every, where, one)

    def strike(self, t0, where, size=1.0, light=True):
        """A shot strikes her armour: a white-hot flash, sparks, a knot of grey-brown smoke."""
        g = self.rng

        def make(t, at, facing):
            self.puff(t, at + facing * 0.15, life=0.1, r=(0.2 * size, 0.85 * size), col=(1.0, 0.86, 0.6), col2=(1.0, 0.5, 0.15), alpha=1.0, kind="glow", wind=0.0, fade_in=0.006, grow=0.4)
            for i in range(26):      # sparks: flung out from the plate, falling
                v = (facing * g.uniform(0.3, 1.0) + Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-0.3, 1)))).normalized() * g.uniform(6, 22) * size
                self.puff(t, at + facing * 0.1, vel=v, life=g.uniform(0.35, 0.9), r=(0.035, 0.012), col=(1.0, 0.8, 0.4), col2=(0.9, 0.3, 0.05), alpha=1.0, kind="glow", drag=1.2, gravity=9.8,
                          wind=0.0, fade_in=0.004)
            for i in range(9):
                v = (facing * g.uniform(0.4, 1.0) + Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0, 1)))).normalized() * g.uniform(3, 12) * size
                c = g.uniform(0.2, 0.34)
                self.puff(t + 0.02, at + facing * 0.1, vel=v, life=g.uniform(2.2, 3.8), r=(0.2 * size, g.uniform(0.9, 1.6) * size), col=(0.42, 0.3, 0.18), col2=(c, c, c), alpha=g.uniform(0.35, 0.55),
                          drag=3.4, rise=0.5, fade_in=0.03, grow=0.5, wind=1.0)
            if light:
                self.light(t, at + facing * 0.5, 1.2e4 * size * size, 0.12, r=0.5)
        self.once(t0, where, make)

    def smoke_from(self, where, start=0.0, end=1e9, size=1.0, thick=1.0, dark=0.06, every=0.1):
        """Smoke coming out of her: a hatch, the engine deck. Dark and oily; the wind lays it over."""
        g = self.rng

        def make(t, at, facing):
            c = dark * g.uniform(0.6, 1.5)
            big = g.uniform(0.7, 1.25)
            self.puff(t, at + Vector((g.uniform(-0.25, 0.25), g.uniform(-0.25, 0.25), g.uniform(0.1, 0.4))) * size, vel=(g.uniform(-0.5, 0.5), g.uniform(-0.5, 0.5), g.uniform(2.2, 4.2)),
                      life=g.uniform(8.0, 12.0), r=(0.3 * size, g.uniform(2.6, 4.2) * size * big), col=(c * 1.7, c * 1.25, c), col2=(c * 1.5, c * 1.5, c * 1.55), alpha=g.uniform(0.5, 0.75) * thick,
                      drag=0.3, rise=1.1, fade_in=0.2, grow=0.62, hold=0.3, wind=1.0)
        self.stream(where, every, make, start=start, end=end)

    def flames(self, where, start=0.0, end=1e9, size=1.0, lean=None):
        """Fire coming out of her, drawn as tongues: ragged streaks rooted where she burns, alive for a frame or two,
        a yellow heart at the root, scraps torn off above going red into the smoke."""
        g = self.rng
        blow = (lean if lean is not None else self.wind * 0.12)

        def across(flow, far):
            side = flow.cross(X)
            side = side.normalized() if side.length > 1e-6 else Y.copy()
            up = side.cross(flow)
            a = g.uniform(0, 6.283)
            return (side * math.cos(a) + up * math.sin(a)) * far

        def tongue(t, root, flow, long, kind, col, col2, life, alpha=0.92, splay=0.2, rise=1.5):
            d = (flow + across(flow, g.uniform(0.0, splay))).normalized()
            wide = long * g.uniform(0.22, 0.34)
            p = self.puff(t, root + d * (0.5 * long), vel=d * rise, life=life, r=(0.72, 1.0), col=col, col2=col2, alpha=alpha, kind=kind, wind=0.1, squash=(0.5 * long, 0.5 * wide, 0.5 * wide),
                          fade_in=0.006, grow=0.5, hold=0.55, cool=1.0)
            p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion(X, g.uniform(0, 6.283))).to_euler()

        def make(t, at, facing):
            flow = (Z + blow + Vector((0.12 * math.sin(9.0 * t), 0.12 * math.sin(7.0 * t + 1.0), 0.0))).normalized()
            gust = 0.7 + 0.55 * abs(math.sin(17.0 * t) * math.sin(5.3 * t + 1.0))
            k = g.uniform(0.85, 1.15)
            tongue(t, at + across(flow, g.uniform(0.0, 0.28) * size), flow, g.uniform(0.7, 1.5) * size * gust, "tongue", (0.30 * k, 0.082 * k, 0.009), (0.24, 0.05, 0.006), g.uniform(0.07, 0.14))
            if g.random() < 0.55:
                tongue(t, at + flow * g.uniform(0.7, 1.7) * size * gust + across(flow, g.uniform(0.0, 0.3) * size), flow, g.uniform(0.45, 1.0) * size, "lick", (0.25 * k, 0.062 * k, 0.007),
                       (0.13, 0.02, 0.004), g.uniform(0.09, 0.2), alpha=0.85, splay=0.3, rise=3.0)
            if g.random() < 0.2:
                tongue(t, at + flow * g.uniform(1.6, 2.6) * size + across(flow, g.uniform(0.0, 0.4) * size), flow, g.uniform(0.3, 0.6) * size, "lick", (0.15, 0.032, 0.005), (0.035, 0.012, 0.006),
                       g.uniform(0.2, 0.4), alpha=0.6, splay=0.35, rise=3.5)
            if g.random() < 0.35:
                p = self.puff(t, at + flow * g.uniform(0.05, 0.25) * size, life=g.uniform(0.05, 0.1), r=(0.1 * size, g.uniform(0.2, 0.32) * size * gust), col=(0.62, 0.34, 0.07), col2=(0.4, 0.15, 0.02),
                              alpha=0.95, kind="glow", wind=0.0, squash=(1.0, 1.0, 1.5), fade_in=0.006, grow=0.6)
        self.stream(where, 0.022, make, start=start, end=end)

    def fire_light(self, where, start, end, power=900.0):
        """The glow a fire throws on her own plates and the ground (it flickers)."""
        d = bpy.data.lights.new("fire", "POINT")
        d.energy, d.color, d.shadow_soft_size = 0.0, (1.0, 0.5, 0.18), 0.5
        ob = bpy.data.objects.new("fire", d)
        self.coll.objects.link(ob)
        self.fires = getattr(self, "fires", [])
        self.fires.append(dict(ob=ob, where=where, start=start, end=end, power=power))

    def at(self, t):
        super().at(t)
        for f in getattr(self, "fires", []):
            on = f["start"] <= t <= f["end"]
            f["ob"].data.energy = f["power"] * (0.7 + 0.3 * math.sin(31.0 * t) * math.sin(11.0 * t + 0.5)) * min(1.0, (t - f["start"]) / 0.6) if on else 0.0
            if on:
                w = f["where"]() if callable(f["where"]) else f["where"]
                f["ob"].location = Vector(w[0]) + Z * 0.7

    def brew_up(self, t0, where, size=1.0):
        """Her ammunition goes: a blinding flash, a jet of flame straight up out of her, burning scraps thrown wide, and
        a rolling ball of fire that turns to a black pillar."""
        g = self.rng

        def make(t, at, facing):
            for i in range(5):       # the flash: a moment only, and never one round ball
                self.puff(t, at + Vector((g.uniform(-0.7, 0.7), g.uniform(-0.7, 0.7), g.uniform(0.2, 1.4))) * size, life=g.uniform(0.07, 0.12), r=(0.5 * size, g.uniform(1.1, 1.7) * size),
                          col=(0.9, 0.62, 0.3), col2=(0.7, 0.3, 0.08), alpha=0.7, kind="glow", wind=0.0, fade_in=0.006, grow=0.4)
            for i in range(16):      # the jet: flame standing straight up out of the turret ring, in ragged tongues
                long = g.uniform(1.6, 3.6) * size
                d = (Z + Vector((g.uniform(-0.16, 0.16), g.uniform(-0.16, 0.16), 0.0))).normalized()
                kk = g.uniform(0.85, 1.2)
                p = self.puff(t + 0.02 * i, at + Vector((g.uniform(-0.35, 0.35), g.uniform(-0.35, 0.35), 0.1)) * size + d * (0.5 * long), vel=d * g.uniform(5.0, 12.0), life=g.uniform(0.25, 0.5), r=(0.72, 1.0),
                              col=(0.34 * kk, 0.1 * kk, 0.012), col2=(0.2, 0.04, 0.005), alpha=0.95, kind="tongue" if i % 2 == 0 else "lick", wind=0.1,
                              squash=(0.5 * long, 0.16 * long, 0.16 * long), fade_in=0.006, grow=0.5, hold=0.5, cool=1.0)
                p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion(X, g.uniform(0, 6.283))).to_euler()
            for i in range(22):      # the ball of fire rolling up, going dark: many small ones, each its own shade
                kk = g.uniform(0.5, 1.2)
                self.puff(t + 0.02 * i, at + Vector((g.uniform(-0.9, 0.9), g.uniform(-0.9, 0.9), g.uniform(0.4, 2.6))) * size, vel=Vector((g.uniform(-5, 5), g.uniform(-5, 5), g.uniform(5, 17))) * size,
                          life=g.uniform(0.5, 1.1), r=(0.4 * size, g.uniform(0.9, 1.7) * size), col=(0.36 * kk, 0.1 * kk, 0.012 * kk), col2=(0.08, 0.01, 0.002), alpha=g.uniform(0.6, 0.85), kind="glow",
                          drag=2.4, wind=0.7, fade_in=0.01, grow=0.45)
            for i in range(40):      # burning scraps and sparks: flung far, falling
                v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.2, 1.6))).normalized() * g.uniform(8, 30) * size
                self.puff(t, at + Z * 0.5, vel=v, life=g.uniform(0.8, 2.0), r=(0.06, 0.02), col=(1.0, 0.75, 0.35), col2=(0.8, 0.22, 0.04), alpha=1.0, kind="glow", drag=0.7, gravity=9.8, wind=0.0,
                          fade_in=0.004)
            for i in range(30):      # the black-brown cloud and the pillar
                up = i < 12
                v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.2, 1.3))).normalized() * g.uniform(5, 20) * size
                if up:
                    v = Vector((g.uniform(-2, 2), g.uniform(-2, 2), g.uniform(16, 42) * size))
                c = g.uniform(0.04, 0.12)
                self.puff(t + 0.05 + g.uniform(0, 0.35), at + Vector((g.uniform(-0.8, 0.8), g.uniform(-0.8, 0.8), 0.8)) * size, vel=v, life=g.uniform(7, 12), r=(0.8 * size, g.uniform(3.2, 5.8) * size),
                          col=(0.3, 0.16, 0.07), col2=(c, c * 0.96, c * 0.92), alpha=g.uniform(0.6, 0.8), drag=g.uniform(1.4, 2.2), rise=1.5, fade_in=0.3, grow=0.5, wind=0.9, hold=0.35, cool=5.0)
            foot = Vector((at.x, at.y, 0.0))
            for i in range(26):      # the ground all round her jumps
                a = g.uniform(0, 6.283)
                out = Vector((math.cos(a), math.sin(a), 0.0))
                self._dust_puff(t + 0.04, foot + out * g.uniform(1.8, 4.5) + Z * 0.15, out * g.uniform(3.0, 9.0) + Z * g.uniform(0.5, 2.0), big=g.uniform(0.9, 1.5), thick=1.3, life=g.uniform(3.0, 5.5))
            self.light(t, at + Z * 3.0, 6.0e4 * size * size, 0.3, r=1.5)
        self.once(t0, where, make)
