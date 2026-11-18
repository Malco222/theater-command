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


def ship_look(paint, mat, nt, bsdf, mix):
    """What the pictures add to a paint that the numbers do not carry: planks on a wooden deck, a sheen on glass."""
    key = paint.split(".")[0]
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
    of the pale churned green under it. From x0 to x1 lengths along her, y1 lengths to each side."""
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
    # the bow wave: a V of feathered crests, gone within a length of the stern; fainter ones from her shoulder and quarter
    tan = math.tan(math.radians(19.5))
    for xo, yo, power, reach in ((bow - 0.02 * L, 0.0, 0.62, 1.9 * L), (bow - 0.27 * L, B * 0.42, 0.4, 1.5 * L), (stern + 0.06 * L, B * 0.45, 0.36, 1.6 * L)):
        d = xo - x
        mid = yo + np.clip(d, 0, None) * tan
        thick = 0.9 + 0.013 * np.clip(d, 0, None)
        feather = 0.55 + 0.9 * fbm(w, h, 40, 5 + abs(int(xo)), stretch=2.0)
        arm = np.exp(-((ay - mid) / thick) ** 2) * power * (1 - ss(0.25 * reach, reach, d)) * ss(0.0, 0.03 * L, d) * feather
        # a second, fainter crest inside the first
        arm2 = np.exp(-((ay - mid * 0.8 + 1.5) / (thick * 0.8)) ** 2) * power * 0.4 * (1 - ss(0.2 * reach, 0.8 * reach, d)) * ss(0.05 * L, 0.2 * L, d) * feather
        foam = np.maximum(foam, np.where(d > 0, np.maximum(arm, arm2), 0.0))
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
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.fromarray(img[::-1].copy()).save(path)        # (a picture's first row is its top; the sea's y runs up)
    return (X0, X1, -Y1, Y1)


class Sea:
    """One sheet of water from under the ship out to the horizon: fine near her, coarse far off, with no seam. It
    carries a real swell (Blender's ocean), whitecaps, the haze of distance, and whatever wakes are laid on it.

    `speed` is how fast the water goes by (metres a second, toward -x): the ship stays where she is and the sea
    passes her, as a film camera keeping station on her would see it."""

    def __init__(self, wave=1.9, choppy=1.25, far=14000.0, cells=560, fine=2.0, tile=420, wind=12.0, heading=205.0, sun_turn=-35.0,
                 clouds=True, foam=-1.8, haze=(2500.0, 16000.0), speed=0.0, sky_too=True, haze_mix=0.75, haze_glow=0.88):
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
        # streaks that go by with the water (long fore and aft, narrow across)
        st = _noise(nt, self.tc.outputs["Object"], (70.0, 10.0), "streaks", 37, 1.4)
        red = _math(nt, "MULTIPLY", sep.outputs[0], power.outputs[0])
        streak = _math(nt, "ADD", _math(nt, "MULTIPLY", st, 1.3), 0.2)
        white = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _math(nt, "MULTIPLY", red, streak), 0.35), 3.5, clamp=True)
        self.mask = _math(nt, "MAXIMUM", self.mask, white)
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


class Clouds:
    """Broken fair-weather cloud seen from above: sheets of soft white, one a little over the other, sliding by.
    (Heaps of puffs looked like cotton wool at this size; a sheet with ragged holes reads as cloud from a height.)"""

    def __init__(self, z, cover=0.36, size=120000.0, patch=3400.0, layers=3, deep=210.0, haze=(3500.0, 24000.0), seed=31):
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
            # (four sizes of the same soft noise, each turned a little, so no row of clouds ever repeats in sight)
            def turned(metres, name, sd, beta, turn):
                mp = nt.nodes.new("ShaderNodeMapping")
                mp.inputs["Rotation"].default_value = (0.0, 0.0, turn)
                nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
                return _noise(nt, mp.outputs["Vector"], metres, name, sd, beta)
            vast = turned(patch * 7.3, "cloud_a", seed, 2.1, 0.61)
            big = turned((patch * 1.25, patch), "cloud_a", seed, 2.1, 0.0)
            mid = turned(patch / 4.3, "cloud_b", seed + 1, 1.7, 1.13)
            small = turned(patch / 17.0, "cloud_c", seed + 2, 1.5, 0.37)
            v = _math(nt, "ADD", _math(nt, "ADD", _math(nt, "MULTIPLY", big, 0.4), _math(nt, "MULTIPLY", vast, 0.26)), _math(nt, "ADD", _math(nt, "MULTIPLY", mid, 0.24), _math(nt, "MULTIPLY", small, 0.1)))
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
            nt.links.new(mh.outputs[0], ms.inputs[2])
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
