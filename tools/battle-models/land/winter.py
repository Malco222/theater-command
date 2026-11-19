"""The Eastern Front's winter for the pictures: snow on the ground, a low grey sky, birch and spruce woods, shell holes
throwing black earth over the snow, and the worn white wash that tanks and guns wore over their summer coat.

    import winter
    winter.install()                      # Show(field="winter") then stands her on snow, washed white
    winter.install(wash=0.0)              # snow under her, her summer coat as it is (a tank fresh to the front)

Nothing here changes a model or what the game's files say. The game lays its own wash over a coat when a battle's
ground is snow (unit-table.json: `winter_wash`); this is how the pictures show it, so the art director and Brian judge
the look the game is asked to match. Our own shapes and shaders throughout.
"""
import math

import bpy
import numpy as np

import ground
import works
from kit import hex_rgb
from works import _math, _mix, _noise, _ramp

#                    snow      wind-packed  earth      road (churned)  rut (slush)  dead stalk  stalk tip   conifer
ground.KINDS["winter"] = ("E4E8EE", "CDD4DD", "5A5046", "9C948A", "4A423A", "7E7458", "A69A76", "1E2A24")
# a road of churned snow throws grey slush, not summer dust, onto what drives on it
SLUSH = "8A8580"

SNOW = (0.80, 0.83, 0.88)               # snow as it lies, linear (sun-lit it reads near white, in shade blue-grey)
WASH = (0.62, 0.62, 0.59)               # lime wash: chalky, a shade off white, never as white as the snow
BARK = (0.72, 0.71, 0.68)               # a birch's bark
TWIG = (0.17, 0.135, 0.13)            # a bare crown's fine twigs, seen together: a purple-brown haze


def overcast(level=0.62):
    """The sky of a winter battle: low grey cloud, palest at the horizon, no blue (the grit direction: an overcast sky
    goes grey-white). Recolours the world stage.sky made."""
    nt = bpy.context.scene.world.node_tree
    ramp = next(n for n in nt.nodes if n.type == "VALTORGB" and len(n.color_ramp.elements) >= 3)
    els = sorted(ramp.color_ramp.elements, key=lambda e: e.position)
    greys = [(0.74, 0.75, 0.77), (0.70, 0.71, 0.73), (0.60, 0.62, 0.65), (0.52, 0.54, 0.58), (0.46, 0.48, 0.52)]
    for e, g in zip(els, greys):
        e.color = (*g, 1.0)
    for n in nt.nodes:                                  # the clouds: a darker grey than the sky between them
        if n.type == "MIX" and n.data_type == "RGBA":
            n.inputs[7].default_value = (0.58, 0.59, 0.61, 1.0)
    nt.nodes["Background"].inputs["Strength"].default_value = level * 1.6


class Winter(ground.Field):
    """A snowfield along a road the armies have churned to slush: woods of spruce and birch along the valley's sides,
    a copse or two nearer, dead grass standing through the snow, low white hills going grey into the haze."""

    def __init__(self, kind="winter", density=5.0, sun_elev=14.0, sun_turn=-38.0, strength=1.7, level=0.62, craters=True, **kw):
        kw.pop("trees", None)
        kw.pop("clouds", None)
        super().__init__(kind=kind, density=density, trees=False, sun_elev=sun_elev, sun_turn=sun_turn, strength=strength, level=level, clouds=True, **kw)
        overcast(level)
        self.sun.data.color = (1.0, 0.93, 0.84)         # a low winter sun through thin cloud: warm, weak
        self._snow_ground()
        self._woods()
        self._hills()
        if craters:
            self._craters()

    # -- the snow: drifts in long soft waves, sparkle, a blue cast where it turns from the sun
    def _snow_ground(self):
        m = self.ground.data.materials[0]
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        bsdf.inputs["Roughness"].default_value = 0.78
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = 0.35
        if "Subsurface Weight" in bsdf.inputs:
            bsdf.inputs["Subsurface Weight"].default_value = 0.12
            bsdf.inputs["Subsurface Radius"].default_value = (0.9, 1.0, 1.2)
        bump = next(n for n in nt.nodes if n.type == "BUMP")
        tc = next(n for n in nt.nodes if n.type == "TEX_COORD")
        pos = tc.outputs["Object"]
        drift = _noise(nt, pos, 0.06, 2.0, 0.5, stretch=(0.5, 1.6, 1.0))        # long drifts lying across the wind
        old = bump.inputs["Height"].links[0].from_socket
        nt.links.new(_math(nt, "ADD", old, _math(nt, "MULTIPLY", drift, 6.0)), bump.inputs["Height"])
        bump.inputs["Distance"].default_value = 0.08

    # -- woods: spruce dark under snow, birch white with a haze of bare twigs
    def _woods(self):
        g = self.rng
        spruce, birch_far, birch_near = [], [], []
        for side in (1.0, -1.0):
            for row, (dist, n) in enumerate(((150.0, 80), (170.0, 90), (196.0, 90), (236.0, 80))):
                for i in range(n):
                    x = -560.0 + 1120.0 * (i + g.uniform(0, 1)) / n
                    y = side * (dist + 28.0 * math.sin(x * 0.011 + side) + 16.0 * math.sin(x * 0.037 + 2.0 * side) + g.uniform(-8, 8))
                    if row == 0 and g.random() < 0.14:
                        continue
                    grove = math.sin(x * 0.019 + side * 1.7) + 0.6 * math.sin(x * 0.051)         # birch where this runs high
                    if grove > 0.55 and row < 2:
                        (birch_near if row == 0 else birch_far).append((x, y, g.uniform(13.0, 19.0)))
                    else:
                        spruce.append((x, y, g.uniform(12.0, 22.0)))
        for cx, cy, n, kind in ((-58.0, 62.0, 9, "birch"), (-95.0, -70.0, 8, "spruce"), (120.0, 85.0, 10, "birch"), (150.0, -75.0, 7, "spruce"),
                                (-30.0, 96.0, 4, "birch"), (60.0, -110.0, 5, "spruce"), (-150.0, 58.0, 6, "spruce")):
            for i in range(n):
                p = (cx + g.uniform(-16, 16), cy + g.uniform(-11, 11), g.uniform(10.0, 18.0))
                (birch_near if kind == "birch" else spruce).append(p)
        self._spruces(spruce)
        self._birches(birch_near, near=True)
        self._birches(birch_far, near=False)

    def _spruce_material(self):
        m = bpy.data.materials.new("spruce")
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        pos = tc.outputs["Object"]
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        ns = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(geo.outputs["Normal"], ns.inputs[0])
        needles = _noise(nt, pos, 3.2, 4.0, 0.7)
        green = nt.nodes.new("ShaderNodeValToRGB")
        c = np.array(hex_rgb(self.cols_hex()[7]))
        green.color_ramp.elements[0].position, green.color_ramp.elements[0].color = 0.3, (*(c * 0.55), 1)
        green.color_ramp.elements[1].position, green.color_ramp.elements[1].color = 0.7, (*(c * 1.25), 1)
        nt.links.new(needles, green.inputs["Fac"])
        # snow lies on the tops of the boughs: where the surface looks up, in clumps, never a smooth white cap
        clumps = _noise(nt, pos, 1.6, 3.0, 0.6)
        up = _ramp(nt, ns.outputs[2], 0.15, 0.55, 0.0, 1.0)
        snowy = _math(nt, "MULTIPLY", up, _ramp(nt, clumps, 0.38, 0.6, 0.0, 1.0))
        _, f, a, b, o = _mix(nt)
        nt.links.new(snowy, f)
        nt.links.new(green.outputs["Color"], a)
        b.default_value = (*SNOW, 1.0)
        col, _ = ground._haze(nt, o, 1500.0)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.88
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.8
        bump.inputs["Distance"].default_value = 0.12
        nt.links.new(needles, bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        return m

    def cols_hex(self):
        return ground.KINDS[self.kind]

    def _spruces(self, spots):
        """Each spruce a stack of drooping skirts of boughs, ragged at every rim, narrowing to a spire."""
        r = self.np
        P, F = [], []
        sides = 11
        for x, y, h in spots:
            tiers = int(r.integers(6, 9))
            base_r = h * r.uniform(0.17, 0.22)
            k0 = sum(len(p) for p in P)
            pts, faces = [], []
            for t in range(tiers):
                f0 = t / tiers
                z_top = h * (0.18 + 0.82 * (f0 + 1.0 / tiers) ** 0.92)
                z_rim = h * (0.10 + 0.82 * f0)
                rr = base_r * (1.0 - f0) ** 0.95 * r.uniform(0.9, 1.08)
                top = len(pts)
                pts.append((x + r.uniform(-0.1, 0.1), y + r.uniform(-0.1, 0.1), z_top))
                ring0 = len(pts)
                a0 = r.uniform(0, 2 * math.pi)
                for i in range(sides):
                    a = a0 + 2 * math.pi * i / sides
                    rj = rr * r.uniform(0.72, 1.1)
                    droop = r.uniform(0.0, 0.35) * h / tiers
                    pts.append((x + rj * math.cos(a), y + rj * math.sin(a), z_rim - droop))
                for i in range(sides):
                    faces.append((top, ring0 + i, ring0 + (i + 1) % sides))
                # the underside of the skirt, back in to the trunk, so a bough has thickness from below
                inner = len(pts)
                pts.append((x, y, z_rim + 0.25 * h / tiers))
                for i in range(sides):
                    faces.append((inner, ring0 + (i + 1) % sides, ring0 + i))
            P.append(np.array(pts))
            F.append(np.array(faces) + k0)
        P, F = np.concatenate(P), np.concatenate(F)
        me = bpy.data.meshes.new("spruces")
        me.from_pydata(P.tolist(), [], F.tolist(), shade_flat=False)
        me.materials.append(self._spruce_material())
        ob = bpy.data.objects.new("spruces", me)
        self.coll.objects.link(ob)
        self.spruces = ob

    def _bark_material(self):
        m = bpy.data.materials.new("birch_bark")
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        pos = tc.outputs["Object"]
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(pos, sep.inputs[0])
        # white bark broken by short dark dashes across the stem, black and fissured at the foot
        dash = _noise(nt, pos, 3.0, 2.0, 0.5, stretch=(1.0, 1.0, 9.0))
        dark = _math(nt, "MAXIMUM", _ramp(nt, dash, 0.62, 0.70, 0.0, 1.0), _ramp(nt, sep.outputs[2], 0.4, 1.6, 0.9, 0.0))
        _, f, a, b, o = _mix(nt)
        nt.links.new(dark, f)
        a.default_value = (*BARK, 1.0)
        b.default_value = (0.05, 0.045, 0.04, 1.0)
        col, _ = ground._haze(nt, o, 1500.0)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.7
        return m

    def _birches(self, spots, near=True):
        """A birch: a slim white stem, leaning a little, its boughs rising and dividing into a fine bare crown. Near
        ones have their boughs and twigs as wood; the far ones a haze of twigs over their stems."""
        r = self.np
        P, F = [], []

        def tube(a, b, ra, rb, sides=5):
            k = sum(len(p) for p in P)
            a, b = np.array(a), np.array(b)
            d = b - a
            d /= max(np.linalg.norm(d), 1e-6)
            u = np.cross(d, [0.0, 0.0, 1.0]) if abs(d[2]) < 0.95 else np.cross(d, [1.0, 0.0, 0.0])
            u /= np.linalg.norm(u)
            w = np.cross(d, u)
            pts = []
            for c, rr in ((a, ra), (b, rb)):
                for i in range(sides):
                    t = 2 * math.pi * i / sides
                    pts.append(c + rr * (math.cos(t) * u + math.sin(t) * w))
            P.append(np.array(pts))
            F.append(np.array([(i, (i + 1) % sides, sides + (i + 1) % sides, sides + i) for i in range(sides)]) + k)

        haze = []
        for x, y, h in spots:
            lean = r.normal(0, 0.05, 2)
            foot, top = np.array([x, y, 0.0]), np.array([x + lean[0] * h, y + lean[1] * h, h])
            r0 = h * r.uniform(0.010, 0.013)
            tube(foot, top, r0, r0 * 0.25, 7 if near else 5)
            if near:
                for j in range(int(r.integers(10, 15))):
                    f0 = r.uniform(0.38, 0.92)
                    at = foot + (top - foot) * f0
                    az = r.uniform(0, 2 * math.pi)
                    up = r.uniform(0.6, 1.1)
                    long = h * r.uniform(0.16, 0.28) * (1.15 - f0)
                    end = at + long * np.array([math.cos(az), math.sin(az), up])
                    tube(at, end, r0 * 0.38 * (1.1 - f0), r0 * 0.08, 4)
                    for k in range(3):
                        g0 = r.uniform(0.4, 0.9)
                        st = at + (end - at) * g0
                        az2 = az + r.uniform(-0.9, 0.9)
                        tw = st + long * 0.45 * np.array([math.cos(az2), math.sin(az2), r.uniform(0.2, 0.9)])
                        tube(st, tw, r0 * 0.08, r0 * 0.03, 3)
            cz = h * 0.68
            w = h * 0.24
            haze.append((top[0] * 0.3 + x * 0.7, top[1] * 0.3 + y * 0.7, cz, w, w, h * 0.3))
            for i in range(3):
                a = r.uniform(0, 2 * math.pi)
                haze.append((x + 0.4 * w * math.cos(a), y + 0.4 * w * math.sin(a), cz + r.uniform(-0.15, 0.2) * h, w * 0.7, w * 0.7, h * 0.22))
        Pn, Fn = np.concatenate(P), np.concatenate(F)
        me = bpy.data.meshes.new("birches")
        me.from_pydata(Pn.tolist(), [], Fn.tolist(), shade_flat=False)
        me.materials.append(self._bark_material())
        ob = bpy.data.objects.new("birches_near" if near else "birches_far", me)
        self.coll.objects.link(ob)
        # the fine twigs no mesh could carry: a thin purple-brown haze through the crown, mostly see-through
        ob = self._blobs("twigs_near" if near else "twigs_far", haze, TWIG, sub=2, clump=0.09 if near else 0.16)
        ob.data.materials[0] = self._twig_material(0.22 if near else 0.32)
        ob.visible_shadow = False

    def _twig_material(self, most):
        """A bare crown's twigs seen together: never solid, thickest at the crown's heart, a dark purple-brown."""
        m = bpy.data.materials.new("twigs")
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        pos = tc.outputs["Object"]
        net = _noise(nt, pos, 9.0, 5.0, 0.75)
        lw = nt.nodes.new("ShaderNodeLayerWeight")
        lw.inputs["Blend"].default_value = 0.5
        face = _math(nt, "SUBTRACT", 1.0, lw.outputs["Facing"], clamp=True)
        alpha = _math(nt, "MULTIPLY", _ramp(nt, net, 0.42, 0.62, 0.0, most), _ramp(nt, face, 0.05, 0.6, 0.15, 1.0))
        col, _ = ground._haze(nt, nt.nodes.new("ShaderNodeRGB").outputs[0], 1500.0)
        rgb = next(n for n in nt.nodes if n.type == "RGB")
        rgb.outputs[0].default_value = (*TWIG, 1.0)
        nt.links.new(col, bsdf.inputs["Base Color"])
        nt.links.new(alpha, bsdf.inputs["Alpha"])
        bsdf.inputs["Roughness"].default_value = 0.9
        return m

    def _hills(self, far=2600.0):
        super()._hills(far)
        em = next(n for n in self.hills.data.materials[0].node_tree.nodes if n.type == "EMISSION")
        em.inputs["Color"].default_value = (0.60, 0.62, 0.66, 1.0)        # white hills gone grey in the haze

    # -- shell holes: black earth thrown over the snow in a ragged star, the hole itself dark and wet
    def _craters(self, spots=((26.0, 11.0, 2.4), (-14.0, -9.0, 3.2), (48.0, -14.0, 2.0), (-40.0, 16.0, 2.8), (8.0, 22.0, 1.8), (70.0, 9.0, 3.6))):
        verts, faces, uvs = [], [], []
        for cx, cy, rr in spots:
            k = len(verts)
            reach = rr * 3.2
            verts += [(cx - reach, cy - reach, 0.012), (cx + reach, cy - reach, 0.012), (cx + reach, cy + reach, 0.012), (cx - reach, cy + reach, 0.012)]
            faces.append((k, k + 1, k + 2, k + 3))
            uvs += [(-1, -1), (1, -1), (1, 1), (-1, 1)]
        me = bpy.data.meshes.new("craters")
        me.from_pydata(verts, [], faces)
        uv = me.uv_layers.new(name="UV")
        for i, loop in enumerate(me.loops):
            uv.data[i].uv = uvs[loop.vertex_index]
        ob = bpy.data.objects.new("craters", me)
        ob.visible_shadow = False
        self.coll.objects.link(ob)
        m = bpy.data.materials.new("craters")
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["UV"], sep.inputs[0])
        ln = nt.nodes.new("ShaderNodeVectorMath")
        ln.operation = "LENGTH"
        cb = nt.nodes.new("ShaderNodeCombineXYZ")
        nt.links.new(sep.outputs[0], cb.inputs[0])
        nt.links.new(sep.outputs[1], cb.inputs[1])
        nt.links.new(cb.outputs[0], ln.inputs[0])
        d = ln.outputs["Value"]                                   # 0 at the hole's middle, 1 at the square's edge
        ang = _math(nt, "ARCTAN2", sep.outputs[1], sep.outputs[0])
        rays = _noise(nt, _comb(nt, _math(nt, "MULTIPLY", ang, 2.6), d), 1.0, 2.0, 0.6, stretch=(1.0, 0.12, 1.0))
        speck = _noise(nt, tc.outputs["Object"], 2.4, 3.0, 0.6)
        star = _math(nt, "SUBTRACT", _math(nt, "ADD", _ramp(nt, d, 0.18, 0.95, 1.0, 0.0), _math(nt, "MULTIPLY", rays, 0.55)), 0.45)
        thrown = _math(nt, "MULTIPLY", _ramp(nt, star, 0.0, 0.25, 0.0, 1.0), _ramp(nt, speck, 0.3, 0.55, 0.45, 1.0))
        hole = _ramp(nt, d, 0.10, 0.20, 1.0, 0.0)
        alpha = _math(nt, "MAXIMUM", thrown, hole, clamp=True)
        _, f, a, b, o = _mix(nt)
        nt.links.new(hole, f)
        a.default_value = (0.055, 0.045, 0.038, 1.0)              # black earth thrown out
        b.default_value = (0.02, 0.018, 0.016, 1.0)               # the wet hole
        nt.links.new(o, bsdf.inputs["Base Color"])
        nt.links.new(alpha, bsdf.inputs["Alpha"])
        bsdf.inputs["Roughness"].default_value = 0.95
        me.materials.append(m)


def _comb(nt, x, y):
    c = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(x, c.inputs[0])
    nt.links.new(y, c.inputs[1])
    return c.outputs[0]


# ---- the worn white wash, and snow lying on her --------------------------------------------------------------

def washed(look, model, wash=0.85, snow=0.8):
    """Wraps a land_look: the coat paints first take a lime wash laid on by brush and mop, thin and streaky, worn back
    to the summer coat at every edge hands and boots touch, round the hatches, low down where the tracks throw slush;
    then, over all her dirt, snow lying on what looks up. The dust, mud and soot of land_look go over the wash."""
    hi = max(v.z for v in model.verts)

    def out(paint, mat, nt, bsdf, mix):
        key = paint.split(".")[0]
        if key in works.COATS and wash > 0:
            src = mix.inputs["A"].links[0].from_socket
            tc = nt.nodes.new("ShaderNodeTexCoord")
            pos = tc.outputs["Object"]
            sep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(pos, sep.inputs[0])
            at = nt.nodes.new("ShaderNodeAttribute")
            at.attribute_name = "Edge"
            ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
            ao.samples = 4
            ao.inputs["Distance"].default_value = 0.1
            strokes = _noise(nt, pos, 1.6, 4.0, 0.7, stretch=(0.6, 0.9, 3.2))        # brush and mop strokes, never even bands
            patches = _noise(nt, pos, 0.8, 2.0, 0.5)
            fine = _noise(nt, pos, 30.0, 3.0, 0.6)
            cover = _math(nt, "ADD", _math(nt, "MULTIPLY", strokes, 0.55), _math(nt, "MULTIPLY", patches, 0.45))
            cover = _ramp(nt, cover, 0.36, 0.56, 0.12, 0.95)                          # thin where the brush ran dry
            worn = _math(nt, "MULTIPLY", at.outputs["Fac"], _ramp(nt, fine, 0.3, 0.6, 0.6, 1.0))
            worn = _math(nt, "MAXIMUM", worn, _ramp(nt, ao.outputs["AO"], 0.55, 0.9, 0.8, 0.0))
            worn = _math(nt, "MAXIMUM", worn, _ramp(nt, sep.outputs[2], 0.35, 1.25, 0.95, 0.0))
            worn = _math(nt, "MAXIMUM", worn, _ramp(nt, _noise(nt, pos, 0.45, 2.0, 0.5), 0.55, 0.72, 0.0, 0.85))   # big scrubbed patches
            fac = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", cover, _math(nt, "SUBTRACT", 1.0, worn, clamp=True)), wash, clamp=True)
            _, f, a, b, o = _mix(nt)
            nt.links.new(fac, f)
            nt.links.new(src, a)
            b.default_value = (*WASH, 1.0)
            nt.links.new(o, mix.inputs["A"])
        look(paint, mat, nt, bsdf, mix)
        if snow > 0 and key not in ("glass", "lamp", "track"):
            last = bsdf.inputs["Base Color"].links[0].from_socket
            geo = nt.nodes.new("ShaderNodeNewGeometry")
            ns = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(geo.outputs["Normal"], ns.inputs[0])
            tc = nt.nodes.new("ShaderNodeTexCoord")
            pos = tc.outputs["Object"]
            sep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(pos, sep.inputs[0])
            lumps = _noise(nt, pos, 3.0, 4.0, 0.65)
            ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
            ao.samples = 4
            ao.inputs["Distance"].default_value = 0.25
            lies = _ramp(nt, ns.outputs[2], 0.72, 0.95, 0.0, 1.0)                        # only on what looks up
            kept = _ramp(nt, ao.outputs["AO"], 0.45, 0.9, 1.0, 0.45)                    # banked in corners, thin on the open
            fac = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", lies, _ramp(nt, lumps, 0.42, 0.58, 0.0, 1.0)), kept)
            fac = _math(nt, "MULTIPLY", fac, _ramp(nt, sep.outputs[2], 0.6, min(1.4, hi * 0.5), 0.3, 1.0))     # gone low down: thrown off
            fac = _math(nt, "MULTIPLY", fac, snow, clamp=True)
            _, f, a, b, o = _mix(nt)
            nt.links.new(fac, f)
            nt.links.new(last, a)
            b.default_value = (*SNOW, 1.0)
            nt.links.new(o, bsdf.inputs["Base Color"])
    return out


_installed = {}


def install(wash=0.85, snow=0.8):
    """Makes field="winter" (and "winter" in landlook's ground=) a Winter, and every vehicle put on the stage after
    this washed white with snow on her. Call before landshow.Show or a pool."""
    if not _installed:
        _installed["Field"] = ground.Field
        _installed["land_look"] = works.land_look
    base_field, base_look = _installed["Field"], _installed["land_look"]

    def field(kind="meadow", **kw):
        if kind != "winter":
            return base_field(kind=kind, **kw)
        kw["density"] = min(kw.get("density", 5.0), 6.0)          # (dead stalks through the snow, never a meadow's grass)
        return Winter(kind=kind, **kw)

    def land_look(model, **kw):
        if kw.get("dust_hex") == ground.KINDS["winter"][3]:
            kw["dust_hex"] = SLUSH
        return washed(base_look(model, **kw), model, wash, snow) if (wash > 0 or snow > 0) else base_look(model, **kw)
    ground.Field = field
    works.land_look = land_look
    # Cycles draws black where a ray meets more see-through faces than it may pass; the kit allows 4, and a row of
    # birches' twig haze stacks up far more than that: let it pass many (a winter picture only)
    import kit
    if "render_setup" not in _installed:
        _installed["render_setup"] = kit.render_setup
    base_setup = _installed["render_setup"]

    def render_setup(*a, **kw):
        sc = base_setup(*a, **kw)
        sc = sc or bpy.context.scene
        sc.cycles.transparent_max_bounces = max(sc.cycles.transparent_max_bounces, 64)
        return sc
    kit.render_setup = render_setup
