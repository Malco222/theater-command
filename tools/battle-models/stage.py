"""The sea and the sky the ships and aircraft are shown on, the wake a ship leaves, and the smoke, flash, splash and
fire of the pictures.

The light is the land team's (shared/kit.py: one warm sun at 38 degrees, AgX), so the pictures of all four kinds of
unit sit together. The sea is in the campaign map's own pale grey-blue, not a dark navy.

Nothing here goes into the game: the game makes its own water and its own effects at the sockets the models carry.
These are the pictures' stand-ins for them, built to the art director's words (soft smoke, never solid balls; a wake
that is whitest at the stern and breaks into streaks; a bow wave as faint feathered crests).
"""
import math
import os
import random

import bpy
import numpy as np
from mathutils import Euler, Quaternion, Vector

import kit
from rigid import hex_rgb

HAZE = (0.74, 0.76, 0.75)          # the colour the far sea and the foot of the sky fade into


def _mix(nt, blend="MIX", fac=1.0):
    """A colour mix node, with its sockets by number (the node has three of each name)."""
    n = nt.nodes.new("ShaderNodeMix")
    n.data_type, n.blend_type = "RGBA", blend
    n.inputs[0].default_value = fac
    return n, n.inputs[0], n.inputs[6], n.inputs[7], n.outputs[2]


def _face(nt, ball=True):
    """How squarely the camera looks into a puff at this point of it: 1 through its middle, 0 at its outline.
    (The layer weight's "facing" was used for this, and drew a faint dark ring round every puff: it takes the
    smoothed normal and forgets its sign, so at the outline, where that normal tips away from the camera, it rises
    again. Black flak puffs against the sky showed it as frogspawn; flame tongues as a crisp brushed edge.) For a
    ball it is worked out from where the sight line passes its middle, in the ball's own space, so squashing does
    not matter and the facets do not show; for another shape, from the normal with its sign kept."""
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    if not ball:
        d = nt.nodes.new("ShaderNodeVectorMath")
        d.operation = "DOT_PRODUCT"
        nt.links.new(geo.outputs["Incoming"], d.inputs[0])
        nt.links.new(geo.outputs["Normal"], d.inputs[1])
        return _math(nt, "MULTIPLY", d.outputs["Value"], 1.0, clamp=True)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    vt = nt.nodes.new("ShaderNodeVectorTransform")
    vt.vector_type, vt.convert_from, vt.convert_to = "VECTOR", "WORLD", "OBJECT"
    nt.links.new(geo.outputs["Incoming"], vt.inputs["Vector"])        # (by name: it is not this node's first socket)
    nm = nt.nodes.new("ShaderNodeVectorMath")
    nm.operation = "NORMALIZE"
    nt.links.new(vt.outputs[0], nm.inputs[0])
    pd = nt.nodes.new("ShaderNodeVectorMath")
    pd.operation = "DOT_PRODUCT"
    nt.links.new(tc.outputs["Object"], pd.inputs[0])
    nt.links.new(nm.outputs[0], pd.inputs[1])
    pp = nt.nodes.new("ShaderNodeVectorMath")
    pp.operation = "DOT_PRODUCT"
    nt.links.new(tc.outputs["Object"], pp.inputs[0])
    nt.links.new(tc.outputs["Object"], pp.inputs[1])
    off2 = _math(nt, "SUBTRACT", pp.outputs["Value"], _math(nt, "MULTIPLY", pd.outputs["Value"], pd.outputs["Value"]))     # (how far off its middle the sight line passes, squared)
    return _math(nt, "SQRT", _math(nt, "SUBTRACT", 1.0, off2, clamp=True))


def _math(nt, op, a=None, b=None, clamp=False):
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


PICTURES = True          # (export.py clears it: the .glb and .fbx take their paints plain)


def ship_look(paint, mat, nt, bsdf, mix):
    """What the pictures add to a paint that the numbers do not carry: planks on a wooden deck, a sheen on glass."""
    key = paint.split(".")[0]
    if paint.endswith(".on") and PICTURES:
        # What lies on a skin (a streak of rust or salt: rigid.Model.band) is seen by the camera and by nothing else:
        # it stands a few millimetres off the skin, and if it stopped any light its edge would draw a hairline of
        # shadow beside it, and the skin seen under its near edge would be dark.
        out = nt.nodes["Material Output"]
        path = nt.nodes.new("ShaderNodeLightPath")
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        ms = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(_math(nt, "SUBTRACT", 1.0, path.outputs["Is Camera Ray"]), ms.inputs[0])
        nt.links.new(bsdf.outputs[0], ms.inputs[1])
        nt.links.new(clear.outputs[0], ms.inputs[2])
        nt.links.new(ms.outputs[0], out.inputs["Surface"])
    if key == "planks":
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (0.35, 1.75, 1.0)      # planks run fore and aft, about 0.18 m wide
        wave = nt.nodes.new("ShaderNodeTexWave")
        wave.wave_type = "BANDS"
        wave.bands_direction = "Y"
        wave.inputs["Scale"].default_value = 1.0
        wave.inputs["Distortion"].default_value = 0.0
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 1.2
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], wave.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position, ramp.color_ramp.elements[0].color = 0.0, (0.55, 0.55, 0.55, 1)
        ramp.color_ramp.elements[1].position, ramp.color_ramp.elements[1].color = 0.12, (1, 1, 1, 1)
        nt.links.new(wave.outputs[1], ramp.inputs["Fac"])
        _, _, a2, b2, o2 = _mix(nt, "MULTIPLY", 0.7)
        nt.links.new(mix.outputs[2], a2)
        nt.links.new(ramp.outputs["Color"], b2)
        _, _, a3, b3, o3 = _mix(nt, "MULTIPLY", 0.3)
        r2 = nt.nodes.new("ShaderNodeValToRGB")
        r2.color_ramp.elements[0].color, r2.color_ramp.elements[1].color = (0.7, 0.7, 0.7, 1), (1.15, 1.15, 1.15, 1)
        nt.links.new(nz.outputs[0], r2.inputs["Fac"])
        nt.links.new(o2, a3)
        nt.links.new(r2.outputs["Color"], b3)
        nt.links.new(o3, bsdf.inputs["Base Color"])
    if key == "glass":
        bsdf.inputs["Roughness"].default_value = 0.06
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = 0.9
    if key in ("hull", "upper", "roof", "boot", "antifoul", "deck", "flight", "steel"):
        # paint on steel plate: a little unevenness in how it takes the light
        tc = nt.nodes.new("ShaderNodeTexCoord")
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 0.35
        nz.inputs["Detail"].default_value = 5.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        mr = nt.nodes.new("ShaderNodeMapRange")
        mr.inputs["To Min"].default_value, mr.inputs["To Max"].default_value = 0.5, 0.72
        nt.links.new(nz.outputs[0], mr.inputs["Value"])
        nt.links.new(mr.outputs["Result"], bsdf.inputs["Roughness"])


def sky(sun_elev=38.0, sun_turn=-35.0, strength=3.2, level=0.55, clouds=True, high=False):
    """The land team's sun, and a sky that is pale at the horizon and blue overhead (by the way a ray looks, so the
    sea mirrors it rightly). `high` is the sky of the upper air, for aircraft: deeper blue, the haze below."""
    sc = bpy.context.scene
    world = bpy.data.worlds.new("sky")
    sc.world = world
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes["Background"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["Generated"], sep.inputs[0])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    e = ramp.color_ramp.elements
    e[0].position, e[0].color = 0.0, (*HAZE, 1)
    e[1].position, e[1].color = 0.55, ((0.13, 0.26, 0.56, 1) if high else (0.36, 0.5, 0.72, 1))
    mid = ramp.color_ramp.elements.new(0.03 if high else 0.045)
    mid.color = (0.6, 0.72, 0.87, 1) if high else (0.68, 0.77, 0.86, 1)
    low = ramp.color_ramp.elements.new(0.13 if high else 0.2)
    low.color = (0.27, 0.45, 0.76, 1) if high else (0.42, 0.6, 0.84, 1)
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["From Min"].default_value, mr.inputs["From Max"].default_value = -0.02, 1.0
    nt.links.new(sep.outputs["Z"], mr.inputs["Value"])
    nt.links.new(mr.outputs["Result"], ramp.inputs["Fac"])
    out = ramp.outputs["Color"]
    if clouds:
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 2.2
        nz.inputs["Detail"].default_value = 6.0
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (1.0, 1.0, 3.5)
        nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        cr = nt.nodes.new("ShaderNodeValToRGB")
        cr.color_ramp.elements[0].position, cr.color_ramp.elements[0].color = 0.52, (0, 0, 0, 1)
        cr.color_ramp.elements[1].position, cr.color_ramp.elements[1].color = 0.72, (1, 1, 1, 1)
        nt.links.new(nz.outputs[0], cr.inputs["Fac"])
        _, f, a, b, o = _mix(nt)
        b.default_value = (0.92, 0.9, 0.86, 1)
        nt.links.new(cr.outputs["Color"], f)
        nt.links.new(out, a)
        out = o
    nt.links.new(out, bg.inputs["Color"])
    bg.inputs["Strength"].default_value = level * 1.6
    sun_data = bpy.data.lights.new("sun", "SUN")
    sun_data.energy = strength
    sun_data.angle = math.radians(3.0)
    sun_data.color = (1.0, 0.93, 0.82)
    sun = bpy.data.objects.new("sun", sun_data)
    sc.collection.objects.link(sun)
    sun.rotation_euler = Euler((math.radians(90 - sun_elev), 0.0, math.radians(sun_turn)), "XYZ")
    return sun


# ---- the sea -----------------------------------------------------------------------------------------

def fbm(w, h, cells=6, seed=1, octaves=4, stretch=1.0):
    """Soft noise as a picture w by h, 0..1 (several sizes of blur laid over each other)."""
    from PIL import Image
    rng = np.random.default_rng(seed)
    out = np.zeros((h, w), dtype=np.float32)
    amp, total = 1.0, 0.0
    for k in range(octaves):
        cw, ch = max(2, int(cells * 2 ** k * stretch)), max(2, int(cells * 2 ** k * h / w))
        g = (rng.random((ch, cw)) * 255).astype(np.uint8)
        out += amp * np.asarray(Image.fromarray(g).resize((w, h), Image.BICUBIC), dtype=np.float32) / 255.0
        total += amp
        amp *= 0.5
    return np.clip(out / total, 0.0, 1.0)


def noise_image(name="noise", size=1024, beta=1.6, seed=11):
    """A picture of soft noise that tiles without a seam, its values spread evenly from 0 to 1 (far cheaper for the
    renderer to look up than noise worked out at every ray)."""
    im = bpy.data.images.get(name)
    if im:
        return im
    rng = np.random.default_rng(seed)
    f = np.fft.fftfreq(size)
    fx, fy = np.meshgrid(f, f)
    k = np.sqrt(fx ** 2 + fy ** 2)
    k[0, 0] = 1.0
    spec = (rng.normal(size=(size, size)) + 1j * rng.normal(size=(size, size))) / k ** beta
    spec[0, 0] = 0.0
    g = np.real(np.fft.ifft2(spec)).ravel()
    rank = np.empty(len(g), dtype=np.float32)
    rank[np.argsort(g)] = np.linspace(0.0, 1.0, len(g), dtype=np.float32)
    px = np.ones((size * size, 4), dtype=np.float32)
    px[:, 0] = px[:, 1] = px[:, 2] = rank
    im = bpy.data.images.new(name, size, size, alpha=False, float_buffer=False, is_data=True)
    im.pixels.foreach_set(px.ravel())
    im.pack()
    return im


def _noise(nt, coords, metres, name="noise", seed=11, beta=1.6):
    """The noise picture laid over the sea so one tile of it covers `metres` (a pair: along, across). Returns its value."""
    mp = nt.nodes.new("ShaderNodeMapping")
    mx, my = metres if isinstance(metres, (tuple, list)) else (metres, metres)
    mp.inputs["Scale"].default_value = (1.0 / mx, 1.0 / my, 1.0)
    nt.links.new(coords, mp.inputs["Vector"])
    im = nt.nodes.new("ShaderNodeTexImage")
    im.image = noise_image(name, seed=seed, beta=beta)
    im.extension = "REPEAT"
    nt.links.new(mp.outputs["Vector"], im.inputs["Vector"])
    return im.outputs["Color"]


def paint_wake(path, L, B, half_of=None, bow=None, stern=None, x0=-4.0, x1=0.62, y1=0.95, w=2304):
    """The wake as a picture seen from above, in the ship's own measures: red is how much white water, green how much
    of the pale churned green under it, blue the crests of her bow wave. From x0 to x1 lengths along her, y1 lengths
    to each side."""
    from PIL import Image
    X0, X1, Y1 = x0 * L, x1 * L, y1 * L
    h = int(round(w * 2 * Y1 / (X1 - X0)))
    x = np.linspace(X0, X1, w, dtype=np.float32)[None, :]
    y = np.linspace(-Y1, Y1, h, dtype=np.float32)[:, None]
    ay = np.abs(y)

    def ss(a, b, v):
        t = np.clip((v - a) / (b - a), 0.0, 1.0)
        return t * t * (3 - 2 * t)
    bow, stern = (L / 2 if bow is None else bow), (-L / 2 if stern is None else stern)
    if half_of is None:
        def half_of(xx):
            u = min(max((xx - stern) / (bow - stern), 0.0), 1.0)
            return (B / 2) * min(max(min(u / 0.12, (1 - u) / 0.42), 0.0), 1.0) ** 0.6
    hb = np.array([float(half_of(float(v))) for v in x[0]], dtype=np.float32)[None, :]
    foam = np.zeros((h, w), dtype=np.float32)
    green = np.zeros((h, w), dtype=np.float32)
    rough = fbm(w, h, 10, 3, stretch=3.0)
    # the ribbon astern: hull wide, twice the beam three lengths back, whitest at the stern
    s = stern - x                                                   # how far astern
    aft = s > -0.04 * L
    wide = (B * 0.5) * (0.82 + np.clip(s, 0, None) / (3.0 * L) * 1.25)
    along = (0.24 + 0.76 * np.exp(-np.clip(s, 0, None) / (0.4 * L))) * (1 - ss(2.6 * L, 3.9 * L, s))
    edge = 1 - ss(wide * 0.72, wide * 1.12, ay)
    rim = np.exp(-((ay - wide * 0.86) / (1.2 + 0.006 * np.clip(s, 0, None))) ** 2) * 0.35 * ss(0.15 * L, 0.8 * L, s)
    foam = np.maximum(foam, np.where(aft, along * edge * (0.75 + 0.5 * rough) + rim * along, 0.0))
    green = np.maximum(green, np.where(aft, (0.45 + 0.55 * np.exp(-np.clip(s, 0, None) / (1.6 * L))) * (1 - ss(3.0 * L, 4.0 * L, s)) * (1 - ss(wide * 0.8, wide * 1.5, ay)), 0.0))
    # the bow wave: a V of faint crests, gone within a length of the stern; fainter ones from her shoulder and quarter.
    # They go in the picture's blue, apart from the white water: laid on the sea as they are, never broken up by the
    # streaks that go by with the water. (The art director, 2026-11-18, of a carrier's clip: each arm, cut across by
    # those streaks, was "a row of bright white dashes of equal size and spacing, like road paint". An arm's crest
    # comes and goes in lengths of every size from twenty metres to a hundred, wanders a little, swells and thins,
    # and each length dies away softly at its ends; no two arms alike, port and starboard neither.)
    crest = np.zeros((h, w), dtype=np.float32)
    tan = math.tan(math.radians(19.5))
    # (Dimmed by half and thinned at the art director's word, 10:45 UTC: as broad and nearly as bright as the foam
    # astern they read as "brush marks on the sea". The brightest white is for the curl at her stem and the first few
    # lengths astern only.)
    for k, (xo, yo, power, reach) in enumerate(((bow - 0.02 * L, 0.0, 0.31, 1.9 * L), (bow - 0.27 * L, B * 0.42, 0.2, 1.5 * L), (stern + 0.06 * L, B * 0.45, 0.18, 1.6 * L))):
        d = xo - x
        cells = max(8, int(round((X1 - X0) / 55.0)))
        lengths = ss(0.45, 0.62, fbm(w, h, cells, 61 + 7 * k, octaves=3))
        wander = (fbm(w, h, cells * 2, 83 + 7 * k, octaves=2) - 0.5) * 5.0
        mid = yo + np.clip(d, 0, None) * tan + wander
        thick = (0.55 + 0.008 * np.clip(d, 0, None)) * (0.65 + 0.9 * fbm(w, h, cells * 2, 97 + 7 * k, octaves=2))
        arm = np.exp(-((ay - mid) / thick) ** 2) * power * (1 - ss(0.25 * reach, reach, d)) * ss(0.0, 0.03 * L, d) * lengths
        # a second, fainter crest inside the first, with lengths of its own
        lengths2 = ss(0.5, 0.66, fbm(w, h, cells, 113 + 7 * k, octaves=3))
        arm2 = np.exp(-((ay - mid * 0.8 + 1.5) / (thick * 0.8)) ** 2) * power * 0.4 * (1 - ss(0.2 * reach, 0.8 * reach, d)) * ss(0.05 * L, 0.2 * L, d) * lengths2
        crest = np.maximum(crest, np.where(d > 0, np.maximum(arm, arm2), 0.0))
    # the curl at the stem and the broken water down her sides
    inside = (x < bow) & (x > stern)
    out_of_hull = ay - hb
    d = bow - x
    curl = np.exp(-(np.clip(out_of_hull, 0, None) / (1.6 + 0.02 * d)) ** 2) * (1.0 * np.exp(-d / (0.1 * L)) + 0.36) * (0.7 + 0.6 * rough)
    foam = np.maximum(foam, np.where(inside, curl * ss(-1.0, 0.3, out_of_hull + 1.0), 0.0))
    green = np.maximum(green, np.where(inside, 0.5 * np.exp(-(np.clip(out_of_hull, 0, None) / 5.0) ** 2), 0.0))
    img = np.zeros((h, w, 3), dtype=np.uint8)
    img[..., 0] = np.clip(foam, 0, 1) * 255
    img[..., 1] = np.clip(green, 0, 1) * 255
    img[..., 2] = np.clip(crest, 0, 1) * 255
    os.makedirs(os.path.dirname(path), exist_ok=True)
    # (a picture's first row is its top; the sea's y runs up. It is written beside its place and moved in whole:
    # several renders may share one folder, and one that read this picture while another was half way through
    # writing it got a sea that was white water from end to end)
    part = f"{path}.{os.getpid()}.part.png"
    Image.fromarray(img[::-1].copy()).save(part)
    os.replace(part, path)
    return (X0, X1, -Y1, Y1)


class Sea:
    """One sheet of water from under the ship out to the horizon: fine near her, coarse far off, with no seam. It
    carries a real swell (Blender's ocean), whitecaps, the haze of distance, and whatever wakes are laid on it.

    `speed` is how fast the water goes by (metres a second, toward -x): the ship stays where she is and the sea
    passes her, as a film camera keeping station on her would see it."""

    def __init__(self, wave=1.9, choppy=1.25, far=14000.0, cells=560, fine=2.0, tile=420, wind=12.0, heading=205.0, sun_turn=-35.0,
                 clouds=True, foam=-1.8, haze=(2500.0, 16000.0), speed=0.0, sky_too=True, haze_mix=0.75, haze_glow=0.88, back_light=1.0):
        sc = bpy.context.scene
        if sky_too:
            sky(sun_turn=sun_turn, clouds=clouds)
        self.speed = speed
        n = cells
        half = fine * n / 2
        k = 5.0
        for _ in range(60):
            k = math.asinh(far / half * k)
        c = half / k
        xs = c * np.sinh(k * np.linspace(-1, 1, n + 1))
        X, Y = np.meshgrid(xs, xs, indexing="ij")
        co = np.stack([X, Y, np.zeros_like(X)], axis=-1).reshape(-1, 3)
        i, j = np.meshgrid(np.arange(n), np.arange(n), indexing="ij")
        a = (i * (n + 1) + j).ravel()
        quads = np.stack([a, a + (n + 1), a + (n + 1) + 1, a + 1], axis=1)
        me = bpy.data.meshes.new("sea")
        me.vertices.add(len(co))
        me.vertices.foreach_set("co", co.ravel())
        me.loops.add(quads.size)
        me.loops.foreach_set("vertex_index", quads.ravel().astype(np.int32))
        me.polygons.add(len(quads))
        me.polygons.foreach_set("loop_start", np.arange(0, quads.size, 4, dtype=np.int32))
        me.polygons.foreach_set("use_smooth", np.ones(len(quads), dtype=bool))
        me.update(calc_edges=True)
        o = bpy.data.objects.new("sea", me)
        sc.collection.objects.link(o)
        self.flow = o.modifiers.new("flow", "DISPLACE")           # the water going by: the sheet stays, its pattern slides
        self.flow.direction, self.flow.space, self.flow.mid_level, self.flow.strength = "X", "LOCAL", 0.0, 0.0
        mod = o.modifiers.new("ocean", "OCEAN")
        mod.geometry_mode = "DISPLACE"
        mod.spatial_size = int(tile)
        mod.size = 1.0
        mod.resolution = mod.viewport_resolution = 14
        mod.wave_scale = wave
        mod.choppiness = choppy
        mod.wind_velocity = wind
        mod.wave_scale_min = 0.5
        mod.wave_alignment = 0.55
        mod.wave_direction = math.radians(heading)
        mod.depth = 200.0
        mod.use_foam = True
        mod.foam_coverage = foam
        mod.foam_layer_name = "foam"
        mod.time = 0.0
        for m_ in (self.flow, mod):
            m_.show_viewport = False                              # (only a render works the sea out, not every pose)
        self.ob, self.mod = o, mod
        self.haze_mix, self.haze_glow = haze_mix, haze_glow
        self.back_light = back_light                              # (how much of the haze's glow lights what is over it)
        self.mat = self._material(haze)
        me.materials.append(self.mat)
        self.wakes = []

    def _material(self, haze):
        m = bpy.data.materials.new("sea")
        m.use_nodes = True
        nt = m.node_tree
        self.nt = nt
        out = nt.nodes["Material Output"]
        water = nt.nodes["Principled BSDF"]
        water.inputs["Roughness"].default_value = 0.1
        if "Specular IOR Level" in water.inputs:
            water.inputs["Specular IOR Level"].default_value = 0.5
        water.inputs["IOR"].default_value = 1.33
        tc = nt.nodes.new("ShaderNodeTexCoord")
        self.tc = tc
        # the water's colour, paler and greener where a wake has churned air into it
        _, f, a, b, col = _mix(nt)
        a.default_value = (*hex_rgb("3F5E6B"), 1)
        b.default_value = (0.3, 0.52, 0.5, 1)
        f.default_value = 0.0
        self.green_fac = f
        nt.links.new(col, water.inputs["Base Color"])
        # small ripples the sheet does not carry
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.45
        bump.inputs["Distance"].default_value = 0.7
        nt.links.new(_noise(nt, tc.outputs["Object"], 46.0, "ripples", 11, 1.3), bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], water.inputs["Normal"])
        # whitecaps: where the swell breaks, broken up by a fleck of noise
        at = nt.nodes.new("ShaderNodeAttribute")
        at.attribute_name = "foam"
        fleck = _noise(nt, tc.outputs["Object"], (34.0, 12.0), "fleck", 23, 1.4)
        cap = _math(nt, "MULTIPLY", at.outputs["Fac"], fleck)
        cap = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", cap, 0.5), 3.0, clamp=True)
        self.mask = cap                                           # (wakes join it with a "whichever is more")
        white = nt.nodes.new("ShaderNodeBsdfDiffuse")
        white.inputs["Color"].default_value = (0.86, 0.9, 0.9, 1)
        ms = nt.nodes.new("ShaderNodeMixShader")
        self.foam_mix = ms
        nt.links.new(cap, ms.inputs[0])
        nt.links.new(water.outputs[0], ms.inputs[1])
        nt.links.new(white.outputs[0], ms.inputs[2])
        # the haze of distance: the far sea goes into the foot of the sky
        cd = nt.nodes.new("ShaderNodeCameraData")
        mr = nt.nodes.new("ShaderNodeMapRange")
        mr.interpolation_type = "SMOOTHSTEP"
        mr.inputs["From Min"].default_value, mr.inputs["From Max"].default_value = haze
        nt.links.new(cd.outputs["View Distance"], mr.inputs["Value"])
        em = nt.nodes.new("ShaderNodeEmission")
        em.inputs["Color"].default_value = (*HAZE, 1)
        em.inputs["Strength"].default_value = self.haze_glow
        if self.back_light < 1.0:
            # (the haze is as bright as the foot of the sky to look at, but the light that comes up off a sea is far
            #  less than the light that comes down: or a white belly is as bright as the sky behind it and a wing seen
            #  from just below vanishes into it)
            lp = nt.nodes.new("ShaderNodeLightPath")
            seen = _math(nt, "ADD", _math(nt, "MULTIPLY", lp.outputs["Is Camera Ray"], 1.0 - self.back_light), self.back_light)
            nt.links.new(_math(nt, "MULTIPLY", seen, self.haze_glow), em.inputs["Strength"])
        hz = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(_math(nt, "MULTIPLY", mr.outputs["Result"], self.haze_mix), hz.inputs[0])
        nt.links.new(ms.outputs[0], hz.inputs[1])
        nt.links.new(em.outputs[0], hz.inputs[2])
        nt.links.new(hz.outputs[0], out.inputs["Surface"])
        return m

    def wake(self, L, B, folder, half_of=None, bow=None, stern=None, at=(0.0, 0.0, 0.0), turn=0.0, name="wake", strength=1.0):
        """Lays a ship's wake on the water: painted once to her measures, it keeps station with her while the streaks
        in it go by with the water. Returns the strength socket (set .default_value to fade it: a sinking ship has none)."""
        nt = self.nt
        path = os.path.join(folder, f"{name}.png")
        X0, X1, Y0, Y1 = paint_wake(path, L, B, half_of, bow, stern)
        anchor = bpy.data.objects.new(name, None)
        bpy.context.scene.collection.objects.link(anchor)
        anchor.location = at
        anchor.rotation_euler = (0, 0, math.radians(turn))
        tc = nt.nodes.new("ShaderNodeTexCoord")
        tc.object = anchor
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (1.0 / (X1 - X0), 1.0 / (Y1 - Y0), 1.0)
        mp.inputs["Location"].default_value = (-X0 / (X1 - X0), -Y0 / (Y1 - Y0), 0.0)
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        im = nt.nodes.new("ShaderNodeTexImage")
        im.image = bpy.data.images.load(path)
        im.image.colorspace_settings.name = "Non-Color"
        im.extension = "CLIP"
        nt.links.new(mp.outputs["Vector"], im.inputs["Vector"])
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(im.outputs["Color"], sep.inputs[0])
        power = nt.nodes.new("ShaderNodeValue")
        power.outputs[0].default_value = strength
        # The white water astern: one pale streak without a break, whitest at her stern, that widens and fades in
        # uneven patches, with streaks in it that go by with the water (long fore and aft, narrow across). (It was
        # cut hard into white streaks and nothing: the art director, 2026-11-18, "foam astern is a continuous pale
        # streak that widens, greens and fades in uneven patches".)
        st = _noise(nt, self.tc.outputs["Object"], (70.0, 10.0), "streaks", 37, 1.4)
        patch = _noise(nt, self.tc.outputs["Object"], (170.0, 95.0), "patches", 41, 1.9)
        red = _math(nt, "MULTIPLY", sep.outputs[0], power.outputs[0])
        streak = _math(nt, "ADD", _math(nt, "MULTIPLY", st, 0.75), 0.55)      # (the noise pictures are spread evenly from 0 to 1)
        body = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", red, streak), _math(nt, "ADD", _math(nt, "MULTIPLY", patch, 0.6), 0.7))
        white = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", body, 0.06), 1.2, clamp=True)
        # the crests of her bow wave: faint, as painted, with only a shimmer from the water going by
        shim = _noise(nt, self.tc.outputs["Object"], (23.0, 19.0), "shimmer", 53, 1.2)
        crest = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", sep.outputs[2], power.outputs[0]), _math(nt, "ADD", _math(nt, "MULTIPLY", shim, 0.4), 0.45), clamp=True)
        self.mask = _math(nt, "MAXIMUM", self.mask, _math(nt, "MAXIMUM", white, crest))
        nt.links.new(self.mask, self.foam_mix.inputs[0])
        green = _math(nt, "MULTIPLY", sep.outputs[1], power.outputs[0])
        if self.green_fac.is_linked:
            green = _math(nt, "MAXIMUM", self.green_fac.links[0].from_socket, green)
        nt.links.new(green, self.green_fac)
        self.wakes.append((anchor, power.outputs[0]))
        return power.outputs[0]

    def at(self, t, way=None):
        """The sea at t seconds; `way` is how far the water has gone by (speed times t, unless she is losing way)."""
        way = self.speed * t if way is None else way
        self.mod.time = t
        self.flow.strength = way
        self.ob.location.x = -way


def sea_and_sky(calm=False, **kw):
    kw.setdefault("wave", 1.0 if calm else 1.9)
    return Sea(**kw)


# ---- smoke, flash, splash, fire ----------------------------------------------------------------------

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
        for k in ("smoke", "water", "glow", "flat", "foam"):       # one ball for each kind of puff, in its own stuff
            self.meshes[k] = self.mesh.copy()
            self.meshes[k].materials.append(self._mat(k))
        # a tongue of flame: the ball drawn out along x, blunt at its root (x = -1) and tapering to a point (x = +1),
        # with a bend in it. "tongue" is hot at its root; "lick" is a scrap of flame torn off downstream.
        lick = self.mesh.copy()
        for v in lick.vertices:
            x, y, z = v.co
            u = min(max(0.5 * (x + 1.0), 0.0), 1.0)
            was = math.hypot(y, z)
            now = 1.9 * u ** 0.45 * (1.0 - u) ** 0.9
            k = now / was if was > 1e-6 else 0.0
            v.co = (x, y * k + 0.2 * u * math.sin(2.6 * u), z * k + 0.12 * u * u)
        for k in ("tongue", "lick"):
            self.meshes[k] = lick.copy()
            self.meshes[k].materials.append(self._flame_mat(k))
        for k in ("jet", "ring"):                # (made last, so that nothing made before them is named differently)
            self.meshes[k] = self.mesh.copy()
            self.meshes[k].materials.append(self._jet_mat() if k == "jet" else self._ring_mat())
        # "heart": the yellow heart of a fire, a short solid tongue. "bit": a scrap of wreckage, a dark chip. (Made after
        # everything else, for the same reason.)
        self.meshes["heart"] = lick.copy()
        self.meshes["heart"].materials.append(self._flame_mat("heart"))
        self.meshes["bit"] = self.mesh.copy()
        self.meshes["bit"].materials.append(self._bit_mat())
        self.meshes["tracer"] = self.mesh.copy()           # (a tracer round's streak of light)
        self.meshes["tracer"].materials.append(self._tracer_mat())

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
        col_out = info.outputs["Color"]
        if kind == "foam":
            # Churned water lying on the sea (the art director, 2026-11-18: the foam a splash left was "a smooth white
            # ellipse with a crisp rim, like a plate laid on the sea"). Broken foam: holes and streaks all through it,
            # thick and greenish-white at its heart, thinning to nothing at its edge, no two patches alike; and as it
            # dies its holes open and it breaks up, rather than only growing paler.
            sep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(tc.outputs["Object"], sep.inputs[0])
            r = _math(nt, "SQRT", _math(nt, "ADD", _math(nt, "MULTIPLY", sep.outputs[0], sep.outputs[0]), _math(nt, "MULTIPLY", sep.outputs[1], sep.outputs[1])))
            own = _math(nt, "MULTIPLY", info.outputs["Random"], 61.0)
            big = nt.nodes.new("ShaderNodeTexNoise")              # (the patch's broad shape: bays and tongues)
            big.noise_dimensions = "4D"
            big.inputs["Scale"].default_value = 2.3
            big.inputs["Detail"].default_value = 2.0
            nt.links.new(tc.outputs["Object"], big.inputs["Vector"])
            nt.links.new(own, big.inputs["W"])
            mp = nt.nodes.new("ShaderNodeMapping")                 # (the lace: drawn out one way, so it lies in streaks)
            mp.inputs["Scale"].default_value = (1.0, 2.7, 1.0)
            nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
            lace = nt.nodes.new("ShaderNodeTexNoise")
            lace.noise_dimensions = "4D"
            lace.inputs["Scale"].default_value = 4.5
            lace.inputs["Detail"].default_value = 6.0
            lace.inputs["Roughness"].default_value = 0.72
            lace.inputs["Distortion"].default_value = 0.8
            nt.links.new(mp.outputs["Vector"], lace.inputs["Vector"])
            nt.links.new(own, lace.inputs["W"])
            live = _math(nt, "MULTIPLY", info.outputs["Alpha"], 1.3, clamp=True)            # 1 while it is fresh, falling as it dies
            thick = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 1.0, r, clamp=True), 1.2)
            thick = _math(nt, "ADD", thick, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", big.outputs[0], 0.5), 2.2))
            thick = _math(nt, "ADD", thick, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", lace.outputs[0], 0.5), 1.6))
            thick = _math(nt, "SUBTRACT", thick, _math(nt, "ADD", 0.16, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 1.0, live), 0.75)))
            thick = _math(nt, "MULTIPLY", thick, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 1.0, r, clamp=True), 5.0, clamp=True))     # (nothing at all at its rim)
            soft = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", thick, 1.5, clamp=True), _math(nt, "MULTIPLY", live, 1.8, clamp=True))
            _, g_fac, g_a, g_b, col_out = _mix(nt, "MULTIPLY")
            nt.links.new(info.outputs["Color"], g_a)
            g_b.default_value = (0.74, 0.95, 0.87, 1.0)
            nt.links.new(_math(nt, "SUBTRACT", _math(nt, "MULTIPLY", thick, 1.4), 0.15, clamp=True), g_fac)
        elif kind == "flat":     # a patch lying on the water: thick in the middle, torn into tongues at the edge
            # (the art director, 2026-11-18, of the oil at a sinking: its near edge was "a smooth curve". How far the
            # patch reaches differs all the way round it, by a noise read on the way out from its middle, its own for
            # each patch: so its rim is bays and tongues, and a dozen of them one on another have no arc anywhere.)
            sep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(tc.outputs["Object"], sep.inputs[0])
            r = _math(nt, "SQRT", _math(nt, "ADD", _math(nt, "MULTIPLY", sep.outputs[0], sep.outputs[0]), _math(nt, "MULTIPLY", sep.outputs[1], sep.outputs[1])))
            flat_ = nt.nodes.new("ShaderNodeCombineXYZ")
            nt.links.new(sep.outputs[0], flat_.inputs[0])
            nt.links.new(sep.outputs[1], flat_.inputs[1])
            way = nt.nodes.new("ShaderNodeVectorMath")
            way.operation = "NORMALIZE"
            nt.links.new(flat_.outputs[0], way.inputs[0])
            ring = nt.nodes.new("ShaderNodeTexNoise")
            ring.noise_dimensions = "4D"
            ring.inputs["Scale"].default_value = 1.7
            ring.inputs["Detail"].default_value = 3.0
            ring.inputs["Roughness"].default_value = 0.65
            nt.links.new(way.outputs[0], ring.inputs["Vector"])
            nt.links.new(_math(nt, "MULTIPLY", info.outputs["Random"], 61.0), ring.inputs["W"])
            reach = _math(nt, "MINIMUM", _math(nt, "ADD", 0.08, _math(nt, "MULTIPLY", ring.outputs[0], 1.3)), 0.97)
            reach = _math(nt, "MAXIMUM", reach, 0.2)
            soft = _math(nt, "SUBTRACT", 1.0, _math(nt, "ADD", _math(nt, "DIVIDE", r, reach), _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", nz.outputs[0], 0.5), 0.9)), clamp=True)
            soft = _math(nt, "MULTIPLY", soft, 1.7, clamp=True)
        else:                    # a puff: thick where you look into it, thinning to nothing at its edge
            face = _face(nt, ball=True)
            soft = _math(nt, "POWER", face, {"smoke": 2.4, "water": 1.9, "glow": 1.3}[kind])
            soft = _math(nt, "MULTIPLY", soft, _math(nt, "SUBTRACT", _math(nt, "MULTIPLY", nz.outputs[0], 1.9), 0.35, clamp=True), clamp=True)
        alpha = _math(nt, "MULTIPLY", soft, 0.85 if kind == "foam" else info.outputs["Alpha"], clamp=True)
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
        elif kind == "flat":       # oil lying on the water: dark and smooth, with a dull shine where the sky lies in it
            d1, d2 = nt.nodes.new("ShaderNodeBsdfDiffuse"), nt.nodes.new("ShaderNodeBsdfGlossy")
            nt.links.new(info.outputs["Color"], d1.inputs["Color"])
            d2.inputs["Roughness"].default_value = 0.3
            body = nt.nodes.new("ShaderNodeMixShader")
            body.inputs[0].default_value = 0.1
            nt.links.new(d1.outputs[0], body.inputs[1])
            nt.links.new(d2.outputs[0], body.inputs[2])
        else:
            body = nt.nodes.new("ShaderNodeBsdfDiffuse")
        if kind not in ("water", "flat"):
            nt.links.new(col_out, body.inputs["Color"])
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        ms = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(alpha, ms.inputs[0])
        nt.links.new(clear.outputs[0], ms.inputs[1])
        nt.links.new(body.outputs[0], ms.inputs[2])
        nt.links.new(ms.outputs[0], out.inputs["Surface"])
        return m

    def _jet_mat(self):
        """Solid water thrown up by a shell: dense and white at its heart, its edge torn into streaks that run
        along it (the ball is drawn out tall and stood upright), no two alike."""
        m = bpy.data.materials.new("fx_jet")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.remove(nt.nodes["Principled BSDF"])
        out = nt.nodes["Material Output"]
        info = nt.nodes.new("ShaderNodeObjectInfo")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (4.6, 4.6, 0.6)                                   # (streaks lie along the column)
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.noise_dimensions = "4D"
        nz.inputs["Scale"].default_value = 1.0
        nz.inputs["Detail"].default_value = 5.0
        nz.inputs["Roughness"].default_value = 0.65
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        nt.links.new(_math(nt, "MULTIPLY", info.outputs["Random"], 61.0), nz.inputs["W"])
        face = _face(nt, ball=True)                  # 1 looking into it, 0 at its edge
        heart = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", face, 0.1), 2.4)                  # (over 1 in its heart: opaque there; the outer quarter of it can tear)
        torn = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", nz.outputs[0], 0.5), 2.8)
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        head = _math(nt, "MULTIPLY", _math(nt, "MAXIMUM", sep.outputs[2], 0.0), 0.9)          # (and its upper end tears most: a feathered head, never a rounded one)
        soft = _math(nt, "SUBTRACT", _math(nt, "ADD", heart, torn), head, clamp=True)
        alpha = _math(nt, "MULTIPLY", soft, info.outputs["Alpha"], clamp=True)
        d1, d2 = nt.nodes.new("ShaderNodeBsdfDiffuse"), nt.nodes.new("ShaderNodeBsdfTranslucent")
        nt.links.new(info.outputs["Color"], d1.inputs["Color"])
        nt.links.new(info.outputs["Color"], d2.inputs["Color"])
        body = nt.nodes.new("ShaderNodeMixShader")
        body.inputs[0].default_value = 0.4
        nt.links.new(d1.outputs[0], body.inputs[1])
        nt.links.new(d2.outputs[0], body.inputs[2])
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        ms = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(alpha, ms.inputs[0])
        nt.links.new(clear.outputs[0], ms.inputs[1])
        nt.links.new(body.outputs[0], ms.inputs[2])
        nt.links.new(ms.outputs[0], out.inputs["Surface"])
        return m

    def _ring_mat(self):
        """A ring of disturbed water lying on the sea round the foot of a column: dark, ragged, nothing in its middle."""
        m = bpy.data.materials.new("fx_ring")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.remove(nt.nodes["Principled BSDF"])
        out = nt.nodes["Material Output"]
        info = nt.nodes.new("ShaderNodeObjectInfo")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        r = _math(nt, "SQRT", _math(nt, "ADD", _math(nt, "MULTIPLY", sep.outputs[0], sep.outputs[0]), _math(nt, "MULTIPLY", sep.outputs[1], sep.outputs[1])))
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.noise_dimensions = "4D"
        nz.inputs["Scale"].default_value = 3.0
        nz.inputs["Detail"].default_value = 4.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        nt.links.new(_math(nt, "MULTIPLY", info.outputs["Random"], 61.0), nz.inputs["W"])
        band = _math(nt, "SUBTRACT", 1.0, _math(nt, "DIVIDE", _math(nt, "ABSOLUTE", _math(nt, "SUBTRACT", r, 0.68)), 0.3), clamp=True)
        soft = _math(nt, "MULTIPLY", _math(nt, "ADD", band, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", nz.outputs[0], 0.5), 1.1)), 1.5, clamp=True)
        soft = _math(nt, "MULTIPLY", soft, _math(nt, "MULTIPLY", band, 4.0, clamp=True), clamp=True)                # (nothing where the band is not)
        alpha = _math(nt, "MULTIPLY", soft, info.outputs["Alpha"], clamp=True)
        body = nt.nodes.new("ShaderNodeBsdfDiffuse")
        nt.links.new(info.outputs["Color"], body.inputs["Color"])
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        ms = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(alpha, ms.inputs[0])
        nt.links.new(clear.outputs[0], ms.inputs[1])
        nt.links.new(body.outputs[0], ms.inputs[2])
        nt.links.new(ms.outputs[0], out.inputs["Surface"])
        return m

    def _flame_mat(self, kind):
        """Flame as tongues: fat near the root and drawn out to a thin soft tip, ragged (eaten away by noise, more
        toward the tip), streaked along its length, each one different. Light and dark in every one of them: deep
        orange in its body (the puff's own colour), dark red-brown toward its tip and along its edge, going into
        smoke at its very end; "tongue" has a yellow root as well, "lick" (a scrap torn off downstream) has none, and
        "heart" is the yellow heart of a fire: short, solid, orange only at its end. A tongue's edge is soft and seen
        through for a few pixels; a heart's is not (a thin film of yellow over a pale skin or the sky is peach).
        (The art director, 2026-11-18: as plain stretched balls they were "lens-shaped ovals with a line down the
        middle" and read as orange leaves; then, one flat tone with a crisp edge, as "a paint splat".)"""
        hot, solid = kind == "tongue", kind == "heart"
        m = bpy.data.materials.new("fx_" + kind)
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.remove(nt.nodes["Principled BSDF"])
        out = nt.nodes["Material Output"]
        info = nt.nodes.new("ShaderNodeObjectInfo")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        u = _math(nt, "MULTIPLY", _math(nt, "ADD", sep.outputs[0], 1.0), 0.5, clamp=True)      # 0 at the root, 1 at the tip
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (0.7, 3.0, 3.0)                                    # (streaks lie along the tongue)
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.noise_dimensions = "4D"
        nz.inputs["Scale"].default_value = 1.0
        nz.inputs["Detail"].default_value = 3.5
        nz.inputs["Roughness"].default_value = 0.55
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        nt.links.new(_math(nt, "MULTIPLY", info.outputs["Random"], 61.0), nz.inputs["W"])     # (no two alike)
        n = nz.outputs[0]
        face = _face(nt, ball=False)                   # 1 looking into it, 0 at its edge
        # (a tongue dies by being eaten away, never by going thin all over: thin orange over a bright sky is peach.
        #  So as it dies the holes in it open, from the tip back, and what is left of it is as solid as it was.)
        gone = _math(nt, "SUBTRACT", 1.0, info.outputs["Alpha"], clamp=True)
        e0, eu = (0.62, 0.85) if hot else (0.3, 0.6) if solid else (0.8, 0.85)
        bar = _math(nt, "ADD", _math(nt, "ADD", e0, _math(nt, "MULTIPLY", u, eu)), _math(nt, "MULTIPLY", gone, 1.0))
        eaten = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _math(nt, "MULTIPLY", n, 2.5), bar), 3.2 if solid else 2.0, clamp=True)
        # its outline: only the middle of the ball is kept as it nears the tip, so it tapers, and the streaks tear its
        # edge.
        taper = _math(nt, "MULTIPLY", _math(nt, "POWER", u, 1.3), 0.75 if solid else 0.92)
        mp2 = nt.nodes.new("ShaderNodeMapping")
        mp2.inputs["Scale"].default_value = (0.55, 1.5, 1.5)                                  # (broad notches: a tongue forks and licks)
        nt.links.new(tc.outputs["Object"], mp2.inputs["Vector"])
        nz2 = nt.nodes.new("ShaderNodeTexNoise")
        nz2.noise_dimensions = "4D"
        nz2.inputs["Scale"].default_value = 1.0
        nz2.inputs["Detail"].default_value = 2.0
        nt.links.new(mp2.outputs["Vector"], nz2.inputs["Vector"])
        nt.links.new(_math(nt, "MULTIPLY", info.outputs["Random"], 37.0), nz2.inputs["W"])
        torn = _math(nt, "ADD", _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", n, 0.5), 1.0), _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", nz2.outputs[0], 0.5), 1.1))
        rim = _math(nt, "ADD", _math(nt, "SUBTRACT", _math(nt, "POWER", face, 0.9), taper), torn)
        if not solid:                                # (dying, it draws in toward its middle: what is left of it is shreds in its heart, never a hollow outline)
            rim = _math(nt, "SUBTRACT", rim, _math(nt, "MULTIPLY", gone, 0.45))
        edge = _math(nt, "MULTIPLY", rim, 2.5 if solid else 1.15, clamp=True)                 # (1 well inside it, falling to 0 at its outline)
        alpha = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", edge, eaten), 4.0 if solid else 1.5, clamp=True)
        # colour. Its body is the puff's own colour, streaked lighter and darker along it.
        _, s_fac, s_a, s_b, col = _mix(nt, "MULTIPLY")
        s_fac.default_value = 1.0
        nt.links.new(info.outputs["Color"], s_a)
        shade = _math(nt, "ADD", 0.5, _math(nt, "MULTIPLY", n, 0.7))
        grey = nt.nodes.new("ShaderNodeCombineXYZ")
        for i in range(3):
            nt.links.new(shade, grey.inputs[i])
        nt.links.new(grey.outputs[0], s_b)
        far = _math(nt, "ADD", u, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 0.5, n), 0.6))   # (how far along, the streaks running ahead and behind)
        if solid:
            # a heart: yellow all through, deep orange only at its end
            _, t_fac, t_a, t_b, tip = _mix(nt)
            t_b.default_value = (0.14, 0.026, 0.004, 1.0)
            nt.links.new(col, t_a)
            nt.links.new(_math(nt, "MULTIPLY", _math(nt, "SUBTRACT", far, 0.62), 2.2, clamp=True), t_fac)
            col = tip
        else:
            # dark red-brown toward the tip, along the edge, and as it dies
            _, t_fac, t_a, t_b, tip = _mix(nt)
            t_b.default_value = (0.032, 0.006, 0.0015, 1.0)
            nt.links.new(col, t_a)
            dark = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", far, 0.2), 2.3)
            # (and wherever it is seen through, at its soft edge and where it is being eaten: thin orange over the sky
            #  is peach, thin red-brown is the smoke coming into it)
            dark = _math(nt, "ADD", dark, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 1.0, _math(nt, "MULTIPLY", edge, eaten)), 1.3))
            dark = _math(nt, "ADD", dark, _math(nt, "MULTIPLY", gone, 0.9), clamp=True)
            nt.links.new(dark, t_fac)
            # and smoke at its very end
            _, k_fac, k_a, k_b, soot = _mix(nt)
            k_b.default_value = (0.006, 0.0045, 0.0035, 1.0)
            nt.links.new(tip, k_a)
            nt.links.new(_math(nt, "ADD", _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", far, 0.66), 3.0), _math(nt, "MULTIPLY", gone, 0.35), clamp=True), k_fac)
            col = soot
            if hot:
                # a yellow root (by how far along it is and by its streaks, not by how it faces the camera: that drew
                # a pale line down its middle)
                _, h_fac, h_a, h_b, heart = _mix(nt)
                h_a.default_value = (0.175, 0.088, 0.0, 1.0)
                nt.links.new(col, h_b)
                nt.links.new(_math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _math(nt, "ADD", _math(nt, "MULTIPLY", u, 2.2), _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 0.5, n), 0.9)), 0.3), 2.0, clamp=True), h_fac)
                col = heart
        body = nt.nodes.new("ShaderNodeEmission")
        body.inputs["Strength"].default_value = 7.0
        nt.links.new(col, body.inputs["Color"])
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        ms = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(alpha, ms.inputs[0])
        nt.links.new(clear.outputs[0], ms.inputs[1])
        nt.links.new(body.outputs[0], ms.inputs[2])
        nt.links.new(ms.outputs[0], out.inputs["Surface"])
        return m

    def _tracer_mat(self):
        """A tracer round's streak (the ball drawn out long along x, its head at +x): a solid core of light with a
        faint soft glow round it, paler down its middle and thinning toward its tail. No noise in it: a mottled streak
        a few pixels wide is half sky, and yellow mixed with a pale sky is straw."""
        m = bpy.data.materials.new("fx_tracer")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.remove(nt.nodes["Principled BSDF"])
        out = nt.nodes["Material Output"]
        info = nt.nodes.new("ShaderNodeObjectInfo")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        u = _math(nt, "ADD", _math(nt, "MULTIPLY", sep.outputs[0], 0.5), 0.5, clamp=True)       # 0 at its tail, 1 at its head
        face = _face(nt, ball=True)
        # a solid core down the middle half of its width, and a soft glow out to its edge (the art director,
        # 2026-11-18: "a short bright dash with a soft glow"). The glow is kept faint: yellow thin over a pale sky is cream.
        core = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", face, 0.82), 12.0, clamp=True)
        glow = _math(nt, "MULTIPLY", _math(nt, "POWER", face, 1.3), 0.42)
        tail = _math(nt, "ADD", _math(nt, "MULTIPLY", u, 1.6), 0.15, clamp=True)
        alpha = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", _math(nt, "MAXIMUM", core, glow), tail), info.outputs["Alpha"], clamp=True)
        hot = _math(nt, "MULTIPLY", _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", face, 0.93), 14.0, clamp=True), u)
        body = nt.nodes.new("ShaderNodeEmission")
        nt.links.new(info.outputs["Color"], body.inputs["Color"])
        nt.links.new(_math(nt, "MULTIPLY", _math(nt, "ADD", 1.0, _math(nt, "MULTIPLY", hot, 0.5)), 7.0), body.inputs["Strength"])
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        ms = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(alpha, ms.inputs[0])
        nt.links.new(clear.outputs[0], ms.inputs[1])
        nt.links.new(body.outputs[0], ms.inputs[2])
        nt.links.new(ms.outputs[0], out.inputs["Surface"])
        return m

    def _bit_mat(self):
        """A scrap of wreckage: a small dark chip with a crisp ragged outline, no two alike (a soft round puff, however
        small, reads as a ball of fluff)."""
        m = bpy.data.materials.new("fx_bit")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.remove(nt.nodes["Principled BSDF"])
        out = nt.nodes["Material Output"]
        info = nt.nodes.new("ShaderNodeObjectInfo")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.noise_dimensions = "4D"
        nz.inputs["Scale"].default_value = 1.7
        nz.inputs["Detail"].default_value = 2.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        nt.links.new(_math(nt, "MULTIPLY", info.outputs["Random"], 61.0), nz.inputs["W"])
        face = _face(nt, ball=True)
        chip = _math(nt, "MULTIPLY", _math(nt, "ADD", _math(nt, "SUBTRACT", face, 0.55), _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", nz.outputs[0], 0.5), 2.6)), 8.0, clamp=True)
        alpha = _math(nt, "MULTIPLY", chip, info.outputs["Alpha"], clamp=True)
        body = nt.nodes.new("ShaderNodeBsdfDiffuse")
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
             gravity=0.0, wind=1.0, squash=(1.0, 1.0, 1.0), fade_in=0.15, grow=0.6, hold=0.0, cool=1.6, name="puff", rng=None, least=0.0):
        """`least`: never thinner on the picture than this many pixels from its middle to its edge, however far off (a
        tracer's light is seen a long way, though the round is small). `name`, `rng`: its object's name and the dice it is turned by. Something added to a clip that is already
        rendered takes its own of both, so that every other puff keeps the name and the turn it had (a puff's
        name decides the pattern of its stuff), and the frames it is not in come out as they were."""
        ob = self.spare[kind].pop() if self.spare.get(kind) else None
        if ob is None:
            ob = bpy.data.objects.new(name, self.meshes[kind])
            self.coll.objects.link(ob)
            ob.visible_shadow = False                    # (far cheaper; the sun still shades each puff by its own roundness)
        dice = rng or self.rng
        ob.rotation_euler = (dice.uniform(0, 6.28), dice.uniform(0, 6.28), dice.uniform(0, 6.28)) if kind not in ("flat", "foam", "ring") else (0, 0, dice.uniform(0, 6.28))
        ob.hide_render = False
        p = dict(ob=ob, kind=kind, t0=t0, at=Vector(at), vel=Vector(vel), life=life, r=r, col=Vector(col), col2=Vector(col2 if col2 is not None else col), alpha=alpha,
                 drag=drag, rise=rise, gravity=gravity, wind=wind, squash=Vector(squash), fade_in=fade_in, grow=grow, hold=hold, cool=cool, least=least)
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
        rad = p["r"][0] + (p["r"][1] - p["r"][0]) * f ** p["grow"]
        wide, tall, thin = 1.0, 1.0, 1.0
        q = p.get("settle")
        if q is not None and t > q[0]:           # (it collapses, spreads and is gone between two moments)
            s = min(1.0, (t - q[0]) / max(q[1] - q[0], 1e-3))
            pos.z *= 1.0 - 0.75 * s * s
            wide, tall, thin = 1.0 + 0.9 * s, 1.0 - 0.6 * s, (1.0 - s) ** 1.3
        ob.location = pos
        cam = bpy.context.scene.camera
        sy, sz = rad * p["squash"][1] * wide, rad * p["squash"][2] * tall
        if p["least"] and cam is not None:       # (the camera must be set for this frame before at() is called, for these)
            far = (pos - cam.location).length
            need = p["least"] * far * cam.data.sensor_width / (cam.data.lens * bpy.context.scene.render.resolution_x)
            if far > 300.0:                      # (beyond 300 m it is let dwindle a little: a sky full of far tracers all as fat as near ones was confetti)
                need *= math.sqrt(300.0 / far)
            sy, sz = max(sy, need), max(sz, need)
        ob.scale = (rad * p["squash"][0] * wide, sy, sz)
        fade = thin * min(1.0, a / max(p["fade_in"], 1e-3)) * (1.0 if f < p["hold"] else (1 - (f - p["hold"]) / max(1 - p["hold"], 1e-6)) ** 1.4)
        if cam is not None and p["kind"] in ("smoke", "water"):
            # (a puff the camera is in, or nearly in, shows its rim "like a flat glass plate": it thins away as the
            #  camera comes up to it)
            reach = rad * max(p["squash"])
            fade *= min(1.0, max(0.0, ((pos - cam.location).length - 1.1 * reach) / (1.4 * reach)))
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

    def light(self, t0, at, power, long=0.14, col=(1.0, 0.72, 0.4), r=3.0, shine=0.05):
        """The light a flash throws, for a frame or two. (The art director, 2026-11-18: a flash that turned the whole
        ship near-white and laid a glittering road down the sea read as a low sun. So it is weak enough that only
        what stands within a few lengths of a gun is lit, to about half as bright again as the day, and it hardly
        shines in the water.)"""
        d = bpy.data.lights.new("flash", "POINT")
        d.energy, d.color, d.shadow_soft_size = 0.0, col, r
        d.specular_factor = shine
        ob = bpy.data.objects.new("flash", d)
        self.coll.objects.link(ob)
        ob.location = at
        self.lights.append(dict(ob=ob, t0=t0, long=long, power=power))

    def at(self, t):
        """Everything as it is at t. Sources lay the puffs that fell due since the last call."""
        self.now = t                                 # (for a source that works out where it is: `t` is still the last call's)
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
            l["ob"].data.energy = l["power"] * min(1.0, (1 - a / l["long"]) / 0.4) if 0 <= a < l["long"] else 0.0     # (full, then quickly out)
        self.t = t

    def flames(self, t, at, n, long, toward=None, spread=1.0, life=(0.1, 0.17), speed=(8.0, 22.0), wind=0.3, heart=0.0):
        """A ragged ball of flame: `n` tongues thrown out from one place at one moment, each between long[0] and
        long[1] metres, deep orange and no two alike, gone in a frame or two; `toward` leans the ball one way (along
        a gun's bore, up from a deck) and `heart` is the size of the hot yellow heart at its root. (The art director,
        2026-11-18: a gun's flash drawn as smooth glowing balls in a row was "a long smooth cream jet, like a
        rocket's exhaust". Flame is ragged, and it breaks at once into smoke.)"""
        g = self.rng
        for i in range(n):
            d = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-1, 1)))
            if d.length < 0.05:
                continue
            d.normalize()
            if toward is not None:
                d = (d * spread + Vector(toward)).normalized()
            ln = g.uniform(*long) * (1.0 if i % 3 else 1.4)
            wide = ln * g.uniform(0.34, 0.5) * 0.58        # (long and thin; fat ones were petals)
            k = g.uniform(0.8, 1.2)
            p = self.puff(t, at + d * ln * 0.32, vel=d * g.uniform(*speed), life=g.uniform(*life), r=(0.6, 1.0), col=(0.14 * k, 0.026 * k, 0.004), col2=(0.1, 0.014, 0.002),
                          alpha=0.92, kind="tongue" if i % 2 else "lick", drag=3.0, wind=wind, squash=(0.5 * ln, 0.5 * wide, 0.5 * wide), fade_in=0.008, grow=0.5, hold=0.4, cool=1.0)
            p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion((1.0, 0.0, 0.0), g.uniform(0, 6.283))).to_euler()
        if heart > 0.0:
            # its heart: a few short yellow tongues at the root, never a round glowing ball ("a round salmon dome").
            # (Dice of their own; the old ball's throws are still made, so everything after it is as it was.)
            life = g.uniform(0.07, 0.1)
            g.uniform(0, 6.28), g.uniform(0, 6.28), g.uniform(0, 6.28)     # (the three throws that turned the old ball)
            q = random.Random(int(at.x * 53.0 + at.y * 7.0 + at.z * 3.0) + 389)
            for j in range(5):
                d = Vector((q.uniform(-1, 1), q.uniform(-1, 1), q.uniform(-1, 1)))
                d = d.normalized() if d.length > 0.05 else Vector((0.0, 0.0, 1.0))
                if toward is not None:
                    d = (d * spread + Vector(toward)).normalized()
                ln = heart * q.uniform(1.3, 2.2)
                wide = ln * q.uniform(0.22, 0.34)
                p = self.puff(t, at + d * ln * 0.3, vel=d * q.uniform(3.0, 8.0), life=life * q.uniform(1.2, 2.0), r=(0.7, 1.0), col=(0.193, 0.111, 0.0), col2=(0.16, 0.05, 0.002), alpha=0.95,
                              kind="heart", drag=3.0, wind=wind, squash=(0.5 * ln, 0.5 * wide, 0.5 * wide), fade_in=0.008, grow=0.5, hold=0.5, cool=1.0, name="heart", rng=q)
                p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion((1.0, 0.0, 0.0), q.uniform(0, 6.283))).to_euler()

    # -- the effects themselves (each is a `make` for stream or once, or lays its own)
    def funnel_smoke(self, where, thick=0.5, r=2.4, start=-16.0, dark=0.3, end=1e9):
        g = self.rng

        def make(t, at, facing):
            shade = dark + g.uniform(-0.04, 0.04)
            self.puff(t, at + Vector((g.uniform(-0.6, 0.6), g.uniform(-0.6, 0.6), 0)), vel=facing * 7.0 + Vector((g.uniform(-0.6, 0.6), g.uniform(-0.8, 0.8), g.uniform(0, 1.5))),
                      life=g.uniform(12.0, 16.0), r=(r, r * 4.2), col=(shade, shade * 0.97, shade * 0.93), alpha=thick, drag=0.6, rise=0.9, fade_in=0.15, grow=0.7)
        self.stream(where, 0.3, make, start=start, end=end)

    def gun(self, t0, where, bore=0.38, light=True):
        """A heavy gun fires: a ragged ball of orange flame, bigger than her turret for a moment, that breaks at once
        into hot brown smoke the wind takes."""
        g = self.rng
        k = bore / 0.38

        def make(t, at, facing):
            side = facing.cross(Vector((0, 0, 1))).normalized()
            up = side.cross(facing)
            self.flames(t, at + facing * 3.0 * k, 13, (4.5 * k, 8.0 * k), toward=facing * 0.8, spread=0.9, life=(0.1, 0.17), speed=(10.0 * k, 26.0 * k), wind=0.2, heart=1.8 * k)
            for i in range(13):      # the smoke: thrown out along the bore, then it hangs, thins and drifts
                sp = g.uniform(30.0, 110.0) * k
                v = facing * sp + side * g.uniform(-16, 16) * k + up * g.uniform(-6, 14) * k
                c = g.uniform(0.68, 0.84)
                self.puff(t + 0.03, at + facing * 2.0, vel=v, life=g.uniform(4.0, 7.5), r=(1.6 * k, g.uniform(6.0, 11.0) * k), col=(0.72, 0.5, 0.26), col2=(c, c * 0.94, c * 0.84),
                          alpha=g.uniform(0.3, 0.5), drag=g.uniform(2.6, 3.6), rise=1.0, fade_in=0.05, grow=0.5, wind=0.85)
            if light:
                self.light(t, at + facing * 4.0 * k + Vector((0, 0, 1.5)), 1.0e4 * k * k, 0.14)
        self.once(t0, where, make)

    def foam(self, t, at, r, life=8.0, n=14):
        """Churned water where something fell or went under: a patch at its heart and a scatter of smaller ones about
        it, drawn out into streaks, each ragged and holed, spreading and drifting a little apart, each dying in its
        own time. (One broad patch alone reads as a plate laid on the sea, however ragged its stuff.)"""
        g = self.rng
        at = Vector((at[0], at[1], 0.0))
        col = (0.84, 0.9, 0.88)
        self.puff(t, at + Vector((0, 0, 0.3)), life=life * g.uniform(0.85, 1.0), r=(r * 1.0, r * 3.0), col=col, alpha=0.7, kind="foam", wind=0.03,
                  squash=(1.0, g.uniform(0.75, 0.95), 0.03), fade_in=0.3, grow=0.5, hold=0.15)
        for i in range(n):
            a = g.uniform(0, 2 * math.pi)
            d = r * g.uniform(0.8, 3.3)
            way = Vector((math.cos(a), math.sin(a), 0.0))
            self.puff(t + g.uniform(0.1, 0.8), at + way * d + Vector((0, 0, 0.31 + 0.012 * i)), vel=way * g.uniform(0.5, 1.6), drag=0.5, life=life * g.uniform(0.5, 1.0),
                      r=(r * 0.4, r * g.uniform(0.9, 1.9)), col=col, alpha=g.uniform(0.45, 0.75), kind="foam", wind=0.03,
                      squash=(g.uniform(1.0, 1.8), g.uniform(0.5, 0.9), 0.03), fade_in=0.35, grow=0.5, hold=0.12)

    def splash(self, t0, at, high=50.0, r=5.0, settle=None):
        """A heavy shell falls in the sea: a tall white column that stands a moment and falls back, and the churned
        water it leaves.

        It goes up fast and narrow. (The art director, 11:31 UTC on 18 November 2026: built of soft puffs thrown up
        under gravity alone, its first five or six frames were "a soft white cotton-wool mound swelling at the
        waterline, like steam from a kettle". A heavy shell's splash is at most of its height in two or three frames,
        its core dense and opaque, its edges streaked with jets and falling drops, a dark ring of disturbed water at
        its foot; the soft mist belongs at its base after it peaks and in the curtain that falls.) So: jets of solid
        water, upright, each thrown to its own height against a strong drag, which sag and are gone in a second and
        a third; thin jets and drops about them; the ring; and the soft puffs as before but coming in slowly, the
        mist the column leaves. The column has dice of its own and its puffs names of their own (see `puff`), so
        a clip rendered before it was built can have it put in: the frames after it is gone come out as they were."""
        g = self.rng
        at = Vector(at)
        q = random.Random(int(at.x * 131.0 + at.y * 17.0) + 4099)
        k = 9.0                                  # (the drag on a jet: it is at four fifths of its height in a sixth of a second)

        def jet(t, foot, top, rad, tall, life, lean=(0.0, 0.0), alpha=1.0, fall=7.0):
            p = self.puff(t, foot, vel=(lean[0] * k, lean[1] * k, top * k), life=life, r=(rad * 0.75, rad), col=(0.97, 0.98, 0.98), col2=(0.9, 0.93, 0.93), alpha=alpha,
                          kind="jet", drag=k, gravity=fall, wind=0.05, squash=(1.0, 1.0, tall), fade_in=0.03, grow=0.25, hold=0.5, name="jet", rng=q)
            # (it leans the way it is thrown: the spin about its own length first, then the lean)
            away = Vector((lean[0], lean[1], 0.0))
            tilt = Quaternion((-away.y, away.x, 0.0), min(0.3, math.atan2(away.length, max(top, 1.0)) * 1.4)) if away.length > 1e-4 else Quaternion()
            wob = Quaternion((1.0, 0.0, 0.0), q.uniform(-0.05, 0.05)) @ Quaternion((0.0, 1.0, 0.0), q.uniform(-0.05, 0.05))
            p["ob"].rotation_euler = (tilt @ wob @ Quaternion((0.0, 0.0, 1.0), q.uniform(0, 6.28))).to_euler()

        def column(t, p0):
            H = high * 0.72
            for j in range(7):                   # its core, from the foot up, narrowing
                c = (j + 0.5) / 7.0
                jet(t + 0.008 * j, p0 + Vector((q.uniform(-0.9, 0.9), q.uniform(-0.9, 0.9), 0.3)), H * (0.1 + 0.8 * c), r * (0.62 - 0.3 * c) * q.uniform(0.8, 1.15), q.uniform(2.6, 3.6), q.uniform(1.15, 1.3))
            for j in range(13):                  # thin jets on its edge, splayed: the streaks down its sides
                c = q.uniform(0.2, 0.92)
                a = q.uniform(0, 2 * math.pi)
                out = Vector((math.cos(a), math.sin(a), 0.0))
                far = q.uniform(0.6, 2.4) * (0.5 + 1.2 * c)
                jet(t + q.uniform(0.0, 0.07), p0 + out * (r * q.uniform(0.35, 0.7)) + Vector((0, 0, 0.3)), H * c, r * q.uniform(0.05, 0.11), q.uniform(9.0, 18.0), q.uniform(0.9, 1.25),
                    lean=(out.x * far, out.y * far), alpha=0.85)
            for j in range(5):                   # and out of its top, a moment after it: a ragged crown, each its own way
                c = (0.9, 1.0, 1.06, 1.12, 1.17)[j] + q.uniform(-0.03, 0.03)
                a = 2 * math.pi * (j * 0.4 + q.uniform(-0.12, 0.12))
                out = Vector((math.cos(a), math.sin(a), 0.0))
                far = q.uniform(0.5, 3.2)
                jet(t + q.uniform(0.03, 0.11), p0 + out * (r * q.uniform(0.05, 0.3)) + Vector((0, 0, 0.3)), H * c, r * q.uniform(0.05, 0.12), q.uniform(8.0, 16.0), q.uniform(0.85, 1.2),
                    lean=(out.x * far, out.y * far), alpha=0.85)
            for j in range(12):                  # drops falling out of it
                a = q.uniform(0, 2 * math.pi)
                out = Vector((math.cos(a), math.sin(a), 0.0))
                p = self.puff(t + q.uniform(0.2, 0.5), p0 + out * (r * q.uniform(0.3, 0.7)) + Vector((0, 0, H * q.uniform(0.3, 1.0))), vel=out * q.uniform(1.0, 4.0) + Vector((0, 0, q.uniform(-2.0, 4.0))),
                              life=q.uniform(0.65, 0.85), r=(r * 0.07, r * q.uniform(0.09, 0.15)), col=(0.97, 0.98, 0.98), alpha=0.9, kind="jet", gravity=9.8, wind=0.1, squash=(1.0, 1.0, 1.7),
                              fade_in=0.03, grow=0.4, hold=0.6, name="jet", rng=q)
                p["ob"].rotation_euler = (0.0, 0.0, q.uniform(0, 6.28))
            # the ring of disturbed water at its foot, spreading
            self.puff(t, Vector((p0.x, p0.y, 0.26)), life=1.3, r=(r * 1.5, r * 3.4), col=(0.03, 0.075, 0.095), alpha=0.6, kind="ring", wind=0.03, squash=(1.0, 1.0, 0.004),
                      fade_in=0.08, grow=0.6, hold=0.4, name="jet", rng=q)

        def make(t, p0, facing):
            column(t, p0)
            n = 20
            for i in range(n):
                f = (i + 0.5) / n
                v0 = math.sqrt(2 * 9.8 * high * f) * g.uniform(0.92, 1.05)
                p = self.puff(t + 0.006 * i, p0 + Vector((g.uniform(-1, 1), g.uniform(-1, 1), 0.5)), vel=(g.uniform(-2.5, 2.5) * (1 - f), g.uniform(-2.5, 2.5) * (1 - f), v0), life=g.uniform(3.4, 4.6),
                          r=(r * 0.4, r * g.uniform(0.7, 1.15) * (1.3 - 0.6 * f)), col=(0.95, 0.97, 0.97), col2=(0.88, 0.92, 0.92), alpha=0.75, kind="water", gravity=9.8, wind=0.06,
                          squash=(1.0, 1.0, 2.3), fade_in=0.8, grow=0.3, hold=0.4)
                if settle is not None:           # (`settle`: two ages of the splash between which its curtain comes down and is gone;
                    p["settle"] = (t + settle[0], t + settle[1])                               #  left alone it hangs four seconds)
            self.foam(t, p0, r)
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
            self.flames(t, at + Vector((0, 0, 1.0)), 11, (3.0 * size, 6.0 * size), toward=(0, 0, 0.5), life=(0.1, 0.2), speed=(8.0 * size, 20.0 * size), wind=0.3, heart=1.5 * size)
            for i in range(10):
                v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.1, 1.2))).normalized() * g.uniform(12, 40) * size
                c = g.uniform(0.07, 0.16)
                self.puff(t + 0.04, at, vel=v, life=g.uniform(5, 9), r=(2.5 * size, g.uniform(8, 13) * size), col=(0.35, 0.2, 0.1), col2=(c, c, c), alpha=0.8, drag=3.0, rise=1.6,
                          fade_in=0.05, grow=0.45, wind=0.9)
            if light:
                self.light(t, at + Vector((0, 0, 3.0)), 1.0e4 * size * size, 0.14)
        self.once(t0, where, make)

    def fire(self, where, start=0.0, end=1e9, size=1.0):
        """She burns: flame at the foot, black smoke leaning downwind."""
        g = self.rng

        def flame(t, at, facing):
            if at.z < 0.5:
                return
            # (ragged tongues that stand up from her and lean with the wind; round glowing balls read as lamps)
            d = (Vector((g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 1.0)) + self.wind * 0.035).normalized()
            ln = g.uniform(3.0, 6.0) * size
            wide = ln * g.uniform(0.3, 0.45)
            k = g.uniform(0.85, 1.15)
            p = self.puff(t, at + Vector((g.uniform(-2, 2) * size, g.uniform(-1.5, 1.5) * size, 0.3)) + d * ln * 0.42, vel=d * g.uniform(3, 7), life=g.uniform(0.2, 0.36), r=(0.7, 1.0),
                          col=(0.135 * k, 0.025 * k, 0.004), col2=(0.1, 0.014, 0.002), alpha=0.92, kind="tongue" if g.random() < 0.65 else "lick", wind=0.25,
                          squash=(0.5 * ln, 0.5 * wide, 0.5 * wide), fade_in=0.012, grow=0.5, hold=0.5, cool=1.0)
            p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion((1.0, 0.0, 0.0), g.uniform(0, 6.283))).to_euler()

        def smoke(t, at, facing):
            if at.z < 0.5:
                return
            c = g.uniform(0.03, 0.08)
            self.puff(t, at + Vector((g.uniform(-1.5, 1.5) * size, g.uniform(-1.5, 1.5) * size, 2.0)), vel=(g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(6, 9)), life=g.uniform(14, 19),
                      r=(3.4 * size, g.uniform(16, 23) * size), col=(c * 1.6, c * 1.2, c), col2=(c, c, c), alpha=0.85, drag=0.35, rise=1.6, fade_in=0.3, grow=0.6, wind=0.95, hold=0.3)
        self.stream(where, 0.05, flame, start=start, end=end)
        self.stream(where, 0.3, smoke, start=start, end=end)

    def blast(self, t0, where, size=1.0):
        """A magazine goes up: a great flash, a tower of flame and black-brown smoke."""
        g = self.rng

        def make(t, at, facing):
            # the tower of flame: three bursts of great ragged tongues, each higher than the last, gone inside a second
            for i, (dt, high, n) in enumerate(((0.0, 5.0, 16), (0.1, 15.0, 14), (0.24, 27.0, 12))):
                self.flames(t + dt, at + Vector((g.uniform(-4, 4), g.uniform(-3, 3), high)) * size, n, (11.0 * size, 22.0 * size), toward=(0, 0, 1.1), spread=0.95, life=(0.3, 0.75),
                            speed=(18.0 * size, 55.0 * size), wind=0.15, heart=(7.0 * size if i == 0 else 0.0))
            # Its smoke is coloured by how high it is thrown: fire light only reaches a cloud's foot, so it is warm in
            # its lower third while the fire burns, grey-brown in the middle and charcoal at the top, and the warm
            # foot is gone in a second. (All tan to its top it read as "a dust cloud beside the black fire smoke".
            # The dice are thrown in the order they always were, so the rest of a clip is as it was.)
            def shade(h, c):
                warm, mid, top = Vector((0.4, 0.23, 0.1)), Vector((0.17, 0.13, 0.1)), Vector((c * 0.8, c * 0.76, c * 0.72))
                return warm.lerp(mid, min(1.0, h / 0.4)) if h < 0.4 else mid.lerp(top, min(1.0, (h - 0.4) / 0.35))
            for i in range(16):      # the pillar
                c = g.uniform(0.03, 0.1)
                dt, ax, ay = g.uniform(0, 0.5), g.uniform(-5, 5), g.uniform(-4, 4)
                vx, vy, vz = g.uniform(-9, 9), g.uniform(-9, 9), g.uniform(60, 150)
                life, rr, dr = g.uniform(12, 20), g.uniform(16, 28), g.uniform(1.0, 1.5)
                self.puff(t + 0.05 + dt, at + Vector((ax, ay, 3.0)), vel=(vx, vy, vz * size), life=life, r=(6 * size, rr * size), col=shade(0.25 + 0.75 * (vz - 60.0) / 90.0, c),
                          col2=(c, c * 0.95, c * 0.9), alpha=0.75, drag=dr, rise=2.6, fade_in=0.3, grow=0.5, wind=0.75, hold=0.4, cool=life / 1.0)   # (it shows once it is up among the tongues, not at the deck over the heart)
            for i in range(26):      # the cloud that spreads from her
                v = Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.1, 1.2))).normalized() * g.uniform(18, 80) * size
                c = g.uniform(0.05, 0.16)
                dt, ax, ay = g.uniform(0, 0.4), g.uniform(-6, 6), g.uniform(-4, 4)
                life, rr, dr = g.uniform(10, 18), g.uniform(18, 32), g.uniform(1.0, 1.7)
                h = min(1.0, max(0.0, v.z / size) / 70.0)
                # (the low smoke comes in slowly, or it hides the heart from the first frame)
                self.puff(t + 0.05 + dt, at + Vector((ax, ay, 3.0)), vel=v, life=life, r=(6 * size, rr * size), col=shade(h, c),
                          col2=(c, c * 0.95, c * 0.9), alpha=0.7, drag=dr, rise=2.0, fade_in=0.5 if h < 0.4 else 0.28, grow=0.5, wind=0.8, hold=0.35, cool=life / 1.0)
            # a small yellow heart low at her deck while the magazine burns: short fat tongues, yellow where the
            # tower above them is orange (dice and names of their own, as a splash's column has)
            q = random.Random(int(at.x * 53.0 + at.y * 7.0) + 911)
            for i in range(13):
                d = Vector((q.uniform(-0.7, 0.7), q.uniform(-0.7, 0.7), 1.0)).normalized()
                ln = q.uniform(6.0, 11.0) * size
                wide = ln * q.uniform(0.45, 0.6)
                p = self.puff(t + q.uniform(0.0, 0.6), at + Vector((q.uniform(-5, 5), q.uniform(-3, 3), 2.0)) * size + d * ln * 0.4, vel=d * q.uniform(4, 10), life=q.uniform(0.28, 0.45), r=(0.7, 1.0),
                              col=(0.193, 0.111, 0.0), col2=(0.16, 0.05, 0.002), alpha=0.95, kind="heart", wind=0.15, squash=(0.5 * ln, 0.5 * wide, 0.5 * wide), fade_in=0.01, grow=0.5, hold=0.5,
                              cool=1.0, name="heart", rng=q)
                p["ob"].rotation_euler = (d.to_track_quat("X", "Z") @ Quaternion((1.0, 0.0, 0.0), q.uniform(0, 6.283))).to_euler()
            self.light(t, at + Vector((0, 0, 18.0)), 6.0e4 * size * size, 0.17, r=10.0)
        self.once(t0, where, make)

    def froth(self, ring, start, end, r=2.4, every=0.07):
        """Where a hull is going under: white water all along the line where the sea is climbing her side. `ring` is a
        function of nothing giving those places now, each (a place on the sea, the way outward from her there); nothing
        is laid while it gives none. (Foam laid under her, or inside the line of her side, is hidden by her: at first
        this was laid about the middle of her deck and nothing of it showed.)"""
        g = self.rng

        def make(t, at, facing):
            places = ring()
            for _ in range(min(3, 1 + len(places) // 4) if places else 0):
                p, out = places[g.randrange(len(places))]
                out = Vector((out[0], out[1], 0.0))
                side = Vector((-out[1], out[0], 0.0))
                q = Vector((p[0], p[1], 0.0)) + out * g.uniform(0.3, 2.6) + side * g.uniform(-2.5, 2.5)
                self.foam(t, q, r * g.uniform(0.7, 1.2), life=g.uniform(2.2, 3.6), n=2)
                if g.random() < 0.7:         # (and it leaps up against her side)
                    self.puff(t, q + Vector((0, 0, 0.4)), vel=(out[0] * g.uniform(0, 2), out[1] * g.uniform(0, 2), g.uniform(4.5, 9.0)), life=g.uniform(0.9, 1.5),
                              r=(r * 0.6, r * g.uniform(1.2, 2.2)), col=(0.95, 0.97, 0.97), col2=(0.88, 0.92, 0.92), alpha=0.66, kind="water", gravity=9.8, wind=0.05,
                              squash=(1.0, 1.0, 1.3), fade_in=0.05, grow=0.4, hold=0.35)
        self.stream((Vector(), Vector((0, 0, 1))), every, make, start=start, end=end)

    def boil(self, at, start, end, r=(30.0, 18.0), rise=2.0):
        """Where she has gone under: the sea boils. Air bursts up from her in white domes, and the water is churned
        white over a patch that spreads to `r` (along her, across her) in `rise` seconds and keeps boiling to `end`.
        The heaviest of the white water at a sinking: each hull's own line (`froth`) is lighter beside it."""
        g = self.rng
        at = Vector((at[0], at[1], 0.0))

        def how(t):
            return smooth_step((t - start) / rise)

        def foam(t, p0, facing):
            k = how(t)
            for _ in range(1 + int(2.0 * k + g.random())):
                a, d = g.uniform(0, 2 * math.pi), math.sqrt(g.random())
                q = at + Vector((math.cos(a) * d * r[0] * (0.3 + 0.7 * k), math.sin(a) * d * r[1] * (0.4 + 0.6 * k), 0.0))
                self.foam(t, q, g.uniform(3.2, 5.6), life=g.uniform(2.8, 4.6), n=3)

        def burst(t, p0, facing):
            k = how(t)
            if g.random() > 0.35 + 0.65 * k:
                return
            a, d = g.uniform(0, 2 * math.pi), math.sqrt(g.random()) * 0.8
            q = at + Vector((math.cos(a) * d * r[0] * (0.3 + 0.7 * k), math.sin(a) * d * r[1] * (0.4 + 0.6 * k), 0.3))
            big = g.uniform(0.8, 1.9)
            self.puff(t, q, vel=(g.uniform(-1.5, 1.5), g.uniform(-1.5, 1.5), g.uniform(5.5, 10.5) * big), life=g.uniform(1.0, 1.8), r=(1.4 * big, g.uniform(3.0, 5.2) * big),
                      col=(0.95, 0.97, 0.97), col2=(0.86, 0.91, 0.9), alpha=0.72, kind="water", gravity=9.8, wind=0.05, squash=(1.0, 1.0, 1.35), fade_in=0.05, grow=0.4, hold=0.35)

        def mound(t, p0, facing):      # (the spray that hangs low over the boil: seen from near the sea's level this is most of its white)
            k = how(t)
            a, d = g.uniform(0, 2 * math.pi), math.sqrt(g.random()) * 0.85
            q = at + Vector((math.cos(a) * d * r[0] * (0.3 + 0.7 * k), math.sin(a) * d * r[1] * (0.4 + 0.6 * k), g.uniform(0.5, 1.6)))
            self.puff(t, q, vel=(g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(0.6, 1.8)), life=g.uniform(1.6, 2.8), r=(2.2, g.uniform(4.5, 7.5)), col=(0.93, 0.96, 0.96), col2=(0.86, 0.91, 0.9),
                      alpha=0.3 + 0.22 * k, kind="water", wind=0.06, squash=(1.3, 1.3, 0.55), fade_in=0.25, grow=0.6, hold=0.3)
        self.stream((at, Vector((0, 0, 1))), 0.15, foam, start=start, end=end)
        self.stream((at, Vector((0, 0, 1))), 0.045, burst, start=start, end=end)
        self.stream((at, Vector((0, 0, 1))), 0.09, mound, start=start, end=end)

    def steam(self, where, start, end, size=1.0):
        """Steam and smoke pouring from a breach in her (her back broken, her boiler rooms open to the sky): white steam
        under pressure and grey-brown smoke with it, until the place goes under."""
        g = self.rng

        def make(t, at, facing):
            if at.z < -1.5:
                return
            white = g.random() < 0.55
            c = g.uniform(0.82, 0.93) if white else g.uniform(0.08, 0.18)
            self.puff(t, at + Vector((g.uniform(-2, 2) * size, g.uniform(-9, 9) * size, g.uniform(0, 2))), vel=(g.uniform(-3, 3), g.uniform(-4, 4), g.uniform(9, 20)), life=g.uniform(4.0, 7.0),
                      r=(2.6 * size, g.uniform(9, 15) * size), col=(c, c, c) if white else (c * 1.5, c * 1.15, c), col2=(c, c, c) if white else (c, c, c), alpha=0.72 if white else 0.85,
                      drag=0.8, rise=2.4, fade_in=0.08, grow=0.55, wind=0.9, hold=0.3)
        self.stream(where, 0.035, make, start=start, end=end)

    def slick(self, t0, at, r=40.0):
        """What she leaves: a dark sheen of oil that wells up where she broke and spreads out from there, ragged, quickly
        at first and then slowly, over the seconds that follow. It lies under the foam, never over it. (From a camera
        low over the sea a patch on the water is seen edge on, so it has to spread well clear of her side to be seen.)"""
        g = self.rng

        def make(t, p0, facing):                              # (a patch's torn rim reaches its full size only here and there: hence sizes a third over `r`)
            for i in range(13):
                a = g.uniform(0, 2 * math.pi)
                way = Vector((math.cos(a), math.sin(a), 0.0))
                self.puff(t + i * 0.25, Vector((p0[0] + way.x * r * 0.08, p0[1] + way.y * r * 0.08, 0.22 - 0.003 * i)), vel=way * g.uniform(3.0, 8.0), drag=0.25, life=13.0,
                          r=(r * 0.16, r * g.uniform(0.75, 1.3)), col=(0.03, 0.034, 0.04), alpha=0.72, kind="flat", wind=0.0, squash=(1.4, 1.0, 0.004), fade_in=1.0, grow=0.45, hold=0.9)
        self.once(t0, (Vector(at), Vector((0, 0, 1))), make)


def smooth_step(v):
    v = min(max(v, 0.0), 1.0)
    return v * v * (3.0 - 2.0 * v)


class Clouds:
    """Broken fair-weather cloud seen from above: sheets of soft white, one a little over the other, sliding by.
    (Heaps of puffs looked like cotton wool at this size; a sheet with ragged holes reads as cloud from a height.)"""

    def __init__(self, z, cover=0.36, size=120000.0, patch=3400.0, layers=3, deep=210.0, haze=(3500.0, 24000.0), seed=31, back_light=1.0):
        self.obs = []
        for k in range(layers):
            me = bpy.data.meshes.new("cloud")
            h = size / 2
            me.from_pydata([(-h, -h, 0), (h, -h, 0), (h, h, 0), (-h, h, 0)], [], [(0, 1, 2, 3)])
            o = bpy.data.objects.new("cloud", me)
            bpy.context.scene.collection.objects.link(o)
            o.location = (0.0, 0.0, z + deep * k / max(layers - 1, 1))
            m = bpy.data.materials.new("cloud")
            m.use_nodes = True
            nt = m.node_tree
            nt.nodes.remove(nt.nodes["Principled BSDF"])
            out = nt.nodes["Material Output"]
            tc = nt.nodes.new("ShaderNodeTexCoord")
            # (five sizes of soft noise, each turned a different way, the middle ones pushed about by two more: big
            #  uneven heaps with ragged holes, and no row of clouds that repeats in sight. Clumps of one size on one
            #  tile lay in even rows when seen low and far off, like a ploughed field.)
            def turned(metres, name, sd, beta, turn, coords=None):
                mp = nt.nodes.new("ShaderNodeMapping")
                mp.inputs["Rotation"].default_value = (0.0, 0.0, turn)
                nt.links.new(coords or tc.outputs["Object"], mp.inputs["Vector"])
                return _noise(nt, mp.outputs["Vector"], metres, name, sd, beta)
            push = nt.nodes.new("ShaderNodeCombineXYZ")
            nt.links.new(_math(nt, "MULTIPLY", _math(nt, "SUBTRACT", turned(patch * 5.3, "cloud_b", seed + 1, 1.7, 2.2), 0.5), patch * 0.7), push.inputs[0])
            nt.links.new(_math(nt, "MULTIPLY", _math(nt, "SUBTRACT", turned(patch * 4.1, "cloud_c", seed + 2, 1.5, 0.9), 0.5), patch * 0.7), push.inputs[1])
            moved = nt.nodes.new("ShaderNodeVectorMath")
            moved.operation = "ADD"
            nt.links.new(tc.outputs["Object"], moved.inputs[0])
            nt.links.new(push.outputs[0], moved.inputs[1])
            vast = turned(patch * 7.3, "cloud_a", seed, 2.1, 0.61)
            big = turned((patch * 3.1, patch * 2.6), "cloud_a", seed, 2.1, 0.23, moved.outputs[0])
            mid = turned(patch * 0.74, "cloud_b", seed + 1, 1.7, 1.13, moved.outputs[0])
            small = turned(patch / 4.3, "cloud_c", seed + 2, 1.5, 0.37)
            fine = turned(patch / 17.0, "cloud_c", seed + 2, 1.5, 1.9)
            v = _math(nt, "ADD", _math(nt, "ADD", _math(nt, "MULTIPLY", big, 0.34), _math(nt, "MULTIPLY", vast, 0.3)),
                      _math(nt, "ADD", _math(nt, "MULTIPLY", mid, 0.2), _math(nt, "ADD", _math(nt, "MULTIPLY", small, 0.11), _math(nt, "MULTIPLY", fine, 0.05))))
            edge = (1.0 - cover) * 0.62 + 0.19 + 0.035 * k          # (each sheet above is a little smaller: the tops dome)
            alpha = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", v, edge), 7.0, clamp=True)
            alpha = _math(nt, "MULTIPLY", _math(nt, "POWER", alpha, 0.8), 0.96 if k == 0 else 0.8)
            cam = nt.nodes.new("ShaderNodeCameraData")
            mr = nt.nodes.new("ShaderNodeMapRange")
            mr.inputs["From Min"].default_value, mr.inputs["From Max"].default_value = haze
            nt.links.new(cam.outputs["View Distance"], mr.inputs["Value"])
            far = _math(nt, "MULTIPLY", mr.outputs["Result"], 0.92)
            white = nt.nodes.new("ShaderNodeBsdfDiffuse")
            white.inputs["Color"].default_value = (0.94, 0.94, 0.95, 1.0)
            lit = nt.nodes.new("ShaderNodeEmission")                # (a cloud is lit through: its shaded side is never dark)
            lit.inputs["Color"].default_value = (0.8, 0.84, 0.9, 1.0)
            lit.inputs["Strength"].default_value = 0.42
            body = nt.nodes.new("ShaderNodeAddShader")
            nt.links.new(white.outputs[0], body.inputs[0])
            nt.links.new(lit.outputs[0], body.inputs[1])
            mist = nt.nodes.new("ShaderNodeEmission")
            mist.inputs["Color"].default_value = (*HAZE, 1.0)
            mist.inputs["Strength"].default_value = 1.0
            mh = nt.nodes.new("ShaderNodeMixShader")
            nt.links.new(far, mh.inputs[0])
            nt.links.new(body.outputs[0], mh.inputs[1])
            nt.links.new(mist.outputs[0], mh.inputs[2])
            clear = nt.nodes.new("ShaderNodeBsdfTransparent")
            ms = nt.nodes.new("ShaderNodeMixShader")
            nt.links.new(_math(nt, "MULTIPLY", alpha, _math(nt, "SUBTRACT", 1.0, _math(nt, "POWER", mr.outputs["Result"], 2.0))), ms.inputs[0])
            nt.links.new(clear.outputs[0], ms.inputs[1])
            seen = mh.outputs[0]
            if back_light < 1.0:
                # (a sunlit sheet of cloud is as bright to look at as it is, but it lights what flies over it less:
                #  with all of that light a white belly is as bright as the sky behind it, and a pale wing seen from
                #  just below is lost against it)
                lp = nt.nodes.new("ShaderNodeLightPath")
                none = nt.nodes.new("ShaderNodeEmission")
                none.inputs["Strength"].default_value = 0.0
                dim = nt.nodes.new("ShaderNodeMixShader")
                nt.links.new(_math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 1.0, lp.outputs["Is Camera Ray"]), 1.0 - back_light), dim.inputs[0])
                nt.links.new(mh.outputs[0], dim.inputs[1])
                nt.links.new(none.outputs[0], dim.inputs[2])
                seen = dim.outputs[0]
            nt.links.new(seen, ms.inputs[2])
            nt.links.new(ms.outputs[0], out.inputs["Surface"])
            me.materials.append(m)
            self.obs.append(o)

    def at(self, t, speed):
        for o in self.obs:
            o.location.x = -speed * t


# ---- the film ----------------------------------------------------------------------------------------

def render_setup(w=960, h=540, samples=16):
    sc = kit.render_setup(w, h, samples=samples)
    sc.render.use_persistent_data = True
    sc.cycles.transparent_max_bounces = 96
    return sc


def film(folder, seconds, fps, step, t0=0.0, only=None):
    """Renders a clip frame by frame: step(t) sets the scene for each. Frames already on disk are passed over (the
    scene is still stepped through them, since smoke remembers)."""
    import time
    os.makedirs(folder, exist_ok=True)
    sc = bpy.context.scene
    n = int(round(seconds * fps))
    for i in range(n):
        t = t0 + i / fps
        step(t)
        path = os.path.join(folder, f"{i + 1:04d}.png")
        if (only is not None and i not in only) or os.path.exists(path):
            continue
        tick = time.time()
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print(f"  frame {i + 1}/{n}: {time.time() - tick:.1f}s", flush=True)
    return folder
