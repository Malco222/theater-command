"""The picture stages of the Mediterranean war: the ground the sets in sets/ describe, dressed with scenery.py's models
and the land vehicles standing on it, for Brian and the art director to judge before the game is handed the files.

    python stage_med.py desert <out.png> [WxH] [samples] [units=desert|none] [view=wide|low|game]
    python stage_med.py drysouth <out.png> [WxH] [samples] [units=italy|none] [view=wide|low|game]

desert: the art director's default desert field (19 Nov): a stony plain of gravel (170,140,106) with patches of fine
sand and camel-thorn scrub, a wadi winding off a low escarpment, a village of flat-roofed houses with a dome and a
slim tower at the escarpment's foot among date palms, a telegraph line along a braided track, a sangar. Dunes rare
(none here). A bleached, hazy horizon.
drysouth: Sicily and the south of Italy: ochre ground with olive scrub, olive groves in rows behind dry-stone walls,
a farmhouse with a line of cypress up its lane, terraces going up to bare hills.

Our own shapes and colours throughout; no outside model, photo or texture.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LAND = os.path.join(os.path.dirname(HERE), "land")
sys.path.insert(0, HERE)
sys.path.insert(1, LAND)

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector, noise  # noqa: E402

import ground  # noqa: E402
import kit  # noqa: E402
import landmarks  # noqa: E402
import rigid  # noqa: E402
import scenery  # noqa: E402
import stage  # noqa: E402
import vehicles  # noqa: E402
import works  # noqa: E402
from ground import _smooth  # noqa: E402
from works import _math, _mix, _noise, _ramp  # noqa: E402

LOOKS = {
    # (the colours as seen, from the art director's world-look brief 3.3 and her desert call of 19 Nov)
    "desert": {"gravel": "AA8C6A", "gravel_dark": "7E6A50", "stone_dark": "4E4034", "gravel_pale": "BEA582", "sand": "D6BC8A", "wadi": "CDB58C",
               "rock": "9A8262", "rock_band": "AE9672", "rock_dark": "806A50", "haze": (0.84, 0.79, 0.70), "sight_m": 3400.0,
               "dust": "B89C72", "track": "C6AE86"},
    "drysouth": {"gravel": "A8905E", "gravel_dark": "7A6A46", "stone_dark": "5E5444", "gravel_pale": "BCA470", "sand": "B8A06A", "wadi": "9C8A5C",
                 "rock": "A09070", "rock_band": "B4A484", "rock_dark": "6E644E", "haze": (0.70, 0.75, 0.80), "sight_m": 2400.0,
                 "dust": "A8946A", "scrub": "6E7448", "track": "8A7A58",
                 "fields": ("B89A5E", "8A6E4E", "9C8C58", "7E7E4A", "C4A86C", "6E6A44", "A88A58")},
}


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def vnoise(x, y, scale, seed=0.0):
    """Perlin noise at many points (mathutils', -1..1), x and y in metres, `scale` the size of its features."""
    return np.array([noise.noise(Vector((a / scale + seed * 13.1, b / scale - seed * 7.3, seed))) for a, b in zip(x.ravel(), y.ravel())]).reshape(x.shape)


class Land:
    """A ground mesh fine under the camera and coarse at the horizon, its height from a function, its paint from
    masks worked out with the height (sand, wadi, track) carried on its points."""

    def __init__(self, look, height, masks, track=None, n=560, extent=7000.0, k=6.4, centre=(0.0, 0.0)):
        self.look = LOOKS[look]
        self.track = track
        u = np.linspace(-1, 1, n)
        s = np.sign(u) * extent * (np.exp(k * np.abs(u)) - 1) / (np.exp(k) - 1)
        X, Y = np.meshgrid(s + centre[0], s + centre[1], indexing="xy")
        self.X, self.Y = X, Y
        H = height(X, Y)
        self.H = H
        self.height = height
        verts = np.stack([X.ravel(), Y.ravel(), H.ravel()], axis=1)
        idx = np.arange(n * n).reshape(n, n)
        quads = np.stack([idx[:-1, :-1].ravel(), idx[:-1, 1:].ravel(), idx[1:, 1:].ravel(), idx[1:, :-1].ravel()], axis=1)
        me = bpy.data.meshes.new("land")
        me.vertices.add(len(verts))
        me.vertices.foreach_set("co", verts.astype(np.float32).ravel())
        me.loops.add(quads.size)
        me.loops.foreach_set("vertex_index", quads.astype(np.int32).ravel())
        me.polygons.add(len(quads))
        me.polygons.foreach_set("loop_start", (np.arange(len(quads)) * 4).astype(np.int32))
        me.polygons.foreach_set("loop_total", np.full(len(quads), 4, dtype=np.int32))
        me.update(calc_edges=True)
        me.polygons.foreach_set("use_smooth", np.ones(len(quads), dtype=bool))
        for name, fn in masks.items():
            a = me.attributes.new(name, "FLOAT", "POINT")
            a.data.foreach_set("value", fn(X, Y).astype(np.float32).ravel())
        self.ob = bpy.data.objects.new("land", me)
        bpy.context.scene.collection.objects.link(self.ob)
        me.materials.append(self._paint(masks))

    def z(self, x, y):
        return float(self.height(np.array([[x]]), np.array([[y]]))[0, 0])

    def _paint(self, masks):
        lk = self.look
        m = bpy.data.materials.new("land")
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        pos = tc.outputs["Object"]

        def attr(name):
            a = nt.nodes.new("ShaderNodeAttribute")
            a.attribute_name = name
            return a.outputs["Fac"]

        def col(h):
            return (*kit.hex_rgb(h), 1.0)
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        sepn = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(geo.outputs["Normal"], sepn.inputs[0])
        seppos = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(pos, seppos.inputs[0])
        # the gravel: a stony crust, never flat: broad tone, then the stones themselves (Voronoi cells, each its own shade)
        broad = _noise(nt, pos, 0.025, 4.0, 0.55)
        mid = _noise(nt, pos, 0.25, 5.0, 0.6)
        # (the stones' places pushed about by noise, so they lie as they fell and never in a pattern)
        wob = nt.nodes.new("ShaderNodeTexNoise")
        wob.inputs["Scale"].default_value = 3.0
        wob.inputs["Detail"].default_value = 3.0
        nt.links.new(pos, wob.inputs["Vector"])
        shove = nt.nodes.new("ShaderNodeVectorMath")
        shove.operation = "MULTIPLY_ADD"
        nt.links.new(wob.outputs["Color"], shove.inputs[0])
        shove.inputs[1].default_value = (0.09, 0.09, 0.0)
        nt.links.new(pos, shove.inputs[2])
        vor = nt.nodes.new("ShaderNodeTexVoronoi")
        vor.inputs["Scale"].default_value = 13.0                      # (stones about 4 to 10 cm across)
        vor.inputs["Randomness"].default_value = 1.0
        nt.links.new(shove.outputs[0], vor.inputs["Vector"])
        vor2 = nt.nodes.new("ShaderNodeTexVoronoi")
        vor2.inputs["Scale"].default_value = 2.2                      # (and fist-sized ones among them)
        nt.links.new(shove.outputs[0], vor2.inputs["Vector"])
        _, f0, a0, b0, o0 = _mix(nt)
        nt.links.new(_smooth(nt, broad, 0.35, 0.65), f0)
        a0.default_value, b0.default_value = col(lk["gravel"]), col(lk["gravel_pale"])
        stones = nt.nodes.new("ShaderNodeValToRGB")
        e = stones.color_ramp.elements
        e[0].position, e[0].color = 0.0, col(lk["stone_dark"])
        e[1].position, e[1].color = 1.0, col(lk["gravel_pale"])
        mide = stones.color_ramp.elements.new(0.6)
        mide.color = col(lk["gravel_dark"])
        nt.links.new(vor.outputs["Color"], sepc := nt.nodes.new("ShaderNodeSeparateColor").inputs[0])
        nt.links.new(sepc.node.outputs[0], stones.inputs["Fac"])
        # (a stone is a cell's middle; between the stones the finer grit and sand they lie in)
        sep2 = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(vor2.outputs["Color"], sep2.inputs[0])
        big = _math(nt, "MULTIPLY", _smooth(nt, vor2.outputs["Distance"], 0.28, 0.14), _smooth(nt, sep2.outputs[0], 0.62, 0.72))    # (one cell in three)
        self.big = big
        # (thick where the wind has blown the fines away, sparse where it has laid them down)
        thick = _ramp(nt, _noise(nt, pos, 0.6, 4.0, 0.6), 0.3, 0.7, 0.12, 0.42)
        pebble = _math(nt, "MAXIMUM", _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", thick, vor.outputs["Distance"]), 8.0, clamp=True), big)
        _, f1, a1, b1, o1 = _mix(nt)
        nt.links.new(_math(nt, "MULTIPLY", pebble, _ramp(nt, mid, 0.3, 0.7, 0.55, 0.95)), f1)
        nt.links.new(o0, a1)
        nt.links.new(stones.outputs["Color"], b1)
        # fine sand: in patches and drifts, and on the wadi's floor; smoother, paler, with a faint ripple
        sand = _math(nt, "MAXIMUM", attr("sand") if "sand" in masks else 0.0, _math(nt, "MULTIPLY", attr("wadi") if "wadi" in masks else 0.0, 0.85))
        sand = _math(nt, "MULTIPLY", sand, _ramp(nt, _noise(nt, pos, 0.9, 4.0, 0.6), 0.25, 0.55, 0.65, 1.0))
        _, f2, a2, b2, o2 = _mix(nt)
        nt.links.new(_smooth(nt, sand, 0.3, 0.7), f2)
        nt.links.new(o1, a2)
        _, fs, as_, bs, os_ = _mix(nt)
        nt.links.new(attr("wadi") if "wadi" in masks else _math(nt, "ADD", 0.0, 0.0), fs)
        as_.default_value, bs.default_value = col(lk["sand"]), col(lk["wadi"])
        nt.links.new(os_, b2)
        out = o2
        # farmland (drysouth): fields in patches, each its own tone (stubble, ploughland, fallow), their edges grown over
        if "fields" in lk:
            fv = nt.nodes.new("ShaderNodeTexVoronoi")
            fv.inputs["Scale"].default_value = 1.0 / 45.0
            fv.inputs["Randomness"].default_value = 0.85
            nt.links.new(pos, fv.inputs["Vector"])
            fe = nt.nodes.new("ShaderNodeTexVoronoi")
            fe.feature = "DISTANCE_TO_EDGE"
            fe.inputs["Scale"].default_value = 1.0 / 45.0
            fe.inputs["Randomness"].default_value = 0.85
            nt.links.new(pos, fe.inputs["Vector"])
            fs_ = nt.nodes.new("ShaderNodeSeparateColor")
            nt.links.new(fv.outputs["Color"], fs_.inputs[0])
            fr = nt.nodes.new("ShaderNodeValToRGB")
            fr.color_ramp.interpolation = "CONSTANT"
            cols = lk["fields"]
            fr.color_ramp.elements[0].position, fr.color_ramp.elements[0].color = 0.0, col(cols[0])
            fr.color_ramp.elements[1].position, fr.color_ramp.elements[1].color = 0.999, col(cols[-1])
            for i, c in enumerate(cols[1:-1]):
                el = fr.color_ramp.elements.new((i + 1) / (len(cols) - 1))
                el.color = col(c)
            nt.links.new(fs_.outputs[0], fr.inputs["Fac"])
            furrow = nt.nodes.new("ShaderNodeTexWave")
            furrow.inputs["Scale"].default_value = 0.9
            furrow.inputs["Distortion"].default_value = 0.6
            nt.links.new(pos, furrow.inputs["Vector"])
            _, ff, fa, fb, fo = _mix(nt, "MULTIPLY", 1.0)
            nt.links.new(fr.outputs["Color"], fa)
            ft = nt.nodes.new("ShaderNodeValToRGB")
            ft.color_ramp.elements[0].color, ft.color_ramp.elements[1].color = (0.88, 0.88, 0.88, 1), (1.05, 1.05, 1.05, 1)
            nt.links.new(furrow.outputs["Fac"], ft.inputs["Fac"])
            nt.links.new(ft.outputs["Color"], fb)
            _, f7, a7, b7, o7 = _mix(nt)
            farm = attr("farm") if "farm" in masks else 1.0
            nt.links.new(_math(nt, "MULTIPLY", farm, 0.85), f7)
            nt.links.new(out, a7)
            nt.links.new(fo, b7)
            _, f8, a8, b8, o8 = _mix(nt)
            edge = _smooth(nt, _math(nt, "ADD", fe.outputs["Distance"], _math(nt, "MULTIPLY", _noise(nt, pos, 0.4, 3.0), 0.01)), 0.016, 0.006)
            nt.links.new(_math(nt, "MULTIPLY", edge, farm), f8)
            nt.links.new(o7, a8)
            b8.default_value = col(lk["scrub"])
            out = o8
        # scrub ground (drysouth): olive-brown patches of low growth between the stones
        if "scrub" in masks:
            _, f3, a3, b3, o3 = _mix(nt)
            nt.links.new(_math(nt, "MULTIPLY", attr("scrub"), _ramp(nt, _noise(nt, pos, 1.6, 5.0, 0.6), 0.35, 0.6)), f3)
            nt.links.new(out, a3)
            b3.default_value = col(lk["scrub"])
            out = o3
        # the track: darker, beaten ruts where lorries and tanks have run, braided as desert tracks are (worked out
        # in the paint itself, so a rut's edge is as sharp up close as the eye wants it)
        tr = self.track
        if tr:
            x, y = seppos.outputs["X"], seppos.outputs["Y"]
            f = _math(nt, "ADD", _math(nt, "ADD", tr["a"], _math(nt, "MULTIPLY", x, tr["b"])),
                      _math(nt, "MULTIPLY", _math(nt, "SINE", _math(nt, "DIVIDE", x, tr["wave"])), tr["amp"]))
            d = _math(nt, "SUBTRACT", y, f)
            d = _math(nt, "ADD", d, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _noise(nt, pos, 0.08, 2.0), 0.5), 1.2))   # (no rut runs ruler-true)
            rut = None
            for off, w in tr["ruts"]:
                one = _smooth(nt, _math(nt, "ABSOLUTE", _math(nt, "SUBTRACT", d, off)), w, w * 0.45, 0.0, 1.0)
                rut = one if rut is None else _math(nt, "MAXIMUM", rut, one)
            rut = _math(nt, "MULTIPLY", rut, _smooth(nt, x, tr["x_end"], tr["x_end"] - 30.0))
            rut = _math(nt, "MULTIPLY", rut, _ramp(nt, _noise(nt, pos, 0.5, 3.0), 0.3, 0.6, 0.35, 0.7))
            self.rut = rut
            _, f4, a4, b4, o4 = _mix(nt)
            nt.links.new(rut, f4)
            nt.links.new(out, a4)
            b4.default_value = col(lk["track"])
            out = o4
        # bare rock where the ground is steep: the escarpment's face, banded where its beds lie
        wave = nt.nodes.new("ShaderNodeTexWave")
        wave.wave_type, wave.bands_direction = "BANDS", "Z"
        wave.inputs["Scale"].default_value = 0.22
        wave.inputs["Distortion"].default_value = 7.0
        wave.inputs["Detail"].default_value = 6.0
        wave.inputs["Detail Scale"].default_value = 2.5
        nt.links.new(pos, wave.inputs["Vector"])
        rk = nt.nodes.new("ShaderNodeValToRGB")
        rk.color_ramp.elements[0].position, rk.color_ramp.elements[0].color = 0.3, col(lk["rock_dark"])
        rk.color_ramp.elements[1].position, rk.color_ramp.elements[1].color = 0.75, col(lk["rock_band"])
        r_mid = rk.color_ramp.elements.new(0.5)
        r_mid.color = col(lk["rock"])
        nt.links.new(wave.outputs["Fac"], rk.inputs["Fac"])
        steep = _smooth(nt, sepn.outputs["Z"], 0.9, 0.72)
        _, f5, a5, b5, o5 = _mix(nt)
        nt.links.new(_math(nt, "MULTIPLY", steep, _ramp(nt, mid, 0.2, 0.5, 0.6, 1.0)), f5)
        nt.links.new(out, a5)
        nt.links.new(rk.outputs["Color"], b5)
        out = o5
        # never one flat colour
        _, _, a6, b6, o6 = _mix(nt, "MULTIPLY", 1.0)
        nt.links.new(out, a6)
        tone = nt.nodes.new("ShaderNodeValToRGB")
        tone.color_ramp.elements[0].position, tone.color_ramp.elements[0].color = 0.25, (0.84, 0.83, 0.81, 1)
        tone.color_ramp.elements[1].position, tone.color_ramp.elements[1].color = 0.75, (1.1, 1.09, 1.07, 1)
        nt.links.new(_noise(nt, pos, 0.07, 6.0, 0.6), tone.inputs["Fac"])
        nt.links.new(tone.outputs["Color"], b6)
        colr, far = ground._haze(nt, o6, far=lk["sight_m"], haze=lk["haze"])
        nt.links.new(colr, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.95
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = 0.12
        # the lie of it: every stone a little proud of the crust, the sand smooth with a faint ripple
        ripple = nt.nodes.new("ShaderNodeTexWave")
        ripple.inputs["Scale"].default_value = 1.4
        ripple.inputs["Distortion"].default_value = 6.0
        ripple.inputs["Detail Scale"].default_value = 1.5
        nt.links.new(pos, ripple.inputs["Vector"])
        pebbles = _math(nt, "ADD", _math(nt, "MULTIPLY", pebble, 0.35),
                        _math(nt, "MULTIPLY", self.big, 0.6))
        if tr:
            pebbles = _math(nt, "SUBTRACT", pebbles, _math(nt, "MULTIPLY", self.rut, 0.8))
        sandy = _smooth(nt, sand, 0.3, 0.7)
        h = _math(nt, "ADD", _math(nt, "MULTIPLY", pebbles, _math(nt, "SUBTRACT", 1.0, sandy)), _math(nt, "MULTIPLY", ripple.outputs["Fac"], _math(nt, "MULTIPLY", sandy, 0.25)))
        near = _math(nt, "SUBTRACT", 1.0, far, clamp=True)
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Distance"].default_value = 0.03
        nt.links.new(_math(nt, "MULTIPLY", near, 0.5), bump.inputs["Strength"])
        nt.links.new(h, bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        return m


# ---- placing the models ---------------------------------------------------------------------------------------
class Props:
    """One near and one far mesh per model, shared by every copy (linked duplicates: a thousand stones cost one)."""

    def __init__(self, land):
        self.land = land
        self.near, self.far = {}, {}
        self.coll = bpy.data.collections.new("props")
        bpy.context.scene.collection.children.link(self.coll)
        self.count = {}

    def _mesh(self, name, fine):
        store = self.near if fine else self.far
        if name not in store:
            ob = scenery.MODELS[name](fine).object(look=True)
            store[name] = ob.data
        return store[name]

    def put(self, name, x, y, turn=None, scale=1.0, sink=0.0, far_at=260.0, eye=(0.0, 0.0), rng=random):
        fine = math.hypot(x - eye[0], y - eye[1]) < far_at
        ob = bpy.data.objects.new(name, self._mesh(name, fine))
        ob.location = (x, y, self.land.z(x, y) - sink)
        ob.rotation_euler[2] = rng.uniform(0, 2 * math.pi) if turn is None else turn
        ob.scale = (scale, scale, scale)
        self.coll.objects.link(ob)
        self.count[name] = self.count.get(name, 0) + 1
        return ob

    def scatter(self, name, n, where, ok, scale=(0.85, 1.2), gap=1.0, seed=1, eye=(0.0, 0.0), far_at=260.0, sink=0.0, tries=30):
        """`n` of a model at points `where` draws, kept where `ok(x, y)` holds and `gap` metres from each other."""
        rng = random.Random(seed)
        placed = []
        for _ in range(n * tries):
            if len(placed) >= n:
                break
            x, y = where(rng)
            if not ok(x, y):
                continue
            if any((x - a) ** 2 + (y - b) ** 2 < gap * gap for a, b in placed[-400:]):
                continue
            placed.append((x, y))
            self.put(name, x, y, scale=rng.uniform(*scale), eye=eye, far_at=far_at, sink=sink, rng=rng)
        return placed


def wires(points, sag=0.6, high=6.5, offsets=(-0.42, 0.42), r=0.006):
    """The telegraph wires, hung from pole to pole with a sag between."""
    for off in offsets:
        cd = bpy.data.curves.new("wire", "CURVE")
        cd.dimensions = "3D"
        cd.bevel_depth = r
        cd.bevel_resolution = 1
        sp = cd.splines.new("POLY")
        pts = []
        for (x0, y0, z0, t0), (x1, y1, z1, t1) in zip(points[:-1], points[1:]):
            for k in range(8):
                f = k / 8
                x, y = x0 + (x1 - x0) * f, y0 + (y1 - y0) * f
                z = z0 + (z1 - z0) * f + high - sag * 4 * f * (1 - f)
                ox, oy = -math.sin(t0) * off, math.cos(t0) * off
                pts.append((x + ox, y + oy, z))
        x1, y1, z1, t1 = points[-1]
        pts.append((x1 - math.sin(t1) * off, y1 + math.cos(t1) * off, z1 + high))
        sp.points.add(len(pts) - 1)
        for p, q in zip(sp.points, pts):
            p.co = (*q, 1.0)
        ob = bpy.data.objects.new("wire", cd)
        ob.data.materials.append(kit.material("wire", kit.hex_rgb("3A3632"), rough=0.6, metal=0.5))
        bpy.context.scene.collection.objects.link(ob)


# ---- the desert -------------------------------------------------------------------------------------------------
def wadi_y(x):
    return 34.0 + 16.0 * np.sin(x / 52.0) + 0.12 * x


def track_y(x):
    return -30.0 + 0.06 * x + 4.0 * np.sin(x / 90.0)


ESC_X = 175.0          # the escarpment's foot runs north and south about here, its top 16 m up


def esc_line(y):
    return ESC_X + 38.0 * np.sin(y / 140.0) + 16.0 * np.sin(y / 47.0 + 1.3)


def desert_height(X, Y):
    r = np.hypot(X, Y)
    h = 0.55 * vnoise(X, Y, 45.0, 1.0) + 0.18 * vnoise(X, Y, 11.0, 2.0)
    h *= 0.35 + 0.65 * smoothstep(18.0, 60.0, np.hypot(X - 15.0, Y))         # (the ground where the tanks stand lies almost flat)
    e = X - esc_line(Y)
    e = e + 4.0 * vnoise(X, Y, 14.0, 3.0)
    h += 10.0 * smoothstep(-30.0, 0.0, e) + 7.0 * smoothstep(-1.0, 4.5, e) + 1.2 * smoothstep(-30.0, 0.0, e) * smoothstep(4.0, -4.0, e) * vnoise(X, Y, 4.0, 3.5) + 2.0 * smoothstep(0.0, 160.0, e)
    dw = np.abs(Y - wadi_y(X))
    wad = np.exp(-(dw / 8.0) ** 2) * smoothstep(ESC_X + 12, ESC_X - 30, X) * smoothstep(-220.0, -80.0, X)
    h -= 1.5 * wad
    h += 85.0 * smoothstep(1500.0, 4200.0, r) * (0.55 + 0.45 * vnoise(X, Y, 700.0, 4.0)) + 25.0 * smoothstep(900.0, 2500.0, r) * vnoise(X, Y, 260.0, 5.0)
    return h


def desert_masks():
    def wadi(X, Y):
        dw = np.abs(Y - wadi_y(X))
        return np.exp(-(dw / 7.0) ** 2) * smoothstep(ESC_X + 12, ESC_X - 30, X) * smoothstep(-220.0, -80.0, X)

    def sand(X, Y):
        e = X - esc_line(Y)
        drift = np.exp(-((e + 14.0) / 12.0) ** 2)                                # (blown against the escarpment's foot)
        return np.clip(smoothstep(0.32, 0.55, vnoise(X, Y, 38.0, 6.0) + 0.25 * vnoise(X, Y, 9.0, 7.0)) + drift * 0.8, 0.0, 1.0)
    return {"wadi": wadi, "sand": sand}


DESERT_TRACK = {"a": -30.0, "b": 0.06, "amp": 4.0, "wave": 90.0, "x_end": ESC_X - 5.0,
                "ruts": ((-3.2, 0.26), (-1.4, 0.3), (1.0, 0.28), (2.8, 0.26), (6.5, 0.3), (8.3, 0.28))}


def desert(units="desert", seed=5):
    stage.HAZE = LOOKS["desert"]["haze"]
    sun = stage.sky(sun_elev=34.0, sun_turn=-58.0, strength=3.5, level=0.5, clouds=False)
    masks = desert_masks()
    land = Land("desert", desert_height, masks, track=DESERT_TRACK)
    eye = (-30.0, -22.0)
    P = Props(land)
    on_track = lambda x, y: abs(y - float(track_y(x)) - 2.5) < 7.5                                  # noqa: E731
    in_wadi = lambda x, y: masks["wadi"](np.array([[x]]), np.array([[y]]))[0, 0]                    # noqa: E731
    clear = lambda x, y: math.hypot(x - 15, y - 1) > 26                                              # (the tanks' own ground)
    near = lambda r: (lambda g: (g.uniform(-60, 200), g.uniform(-110, 120)))                          # noqa: E731
    # stones everywhere near, thicker toward the escarpment
    P.scatter("Stones", 380, near(0), lambda x, y: clear(x, y) and not on_track(x, y), scale=(0.7, 1.4), gap=3.0, seed=seed, eye=eye)
    P.scatter("Boulder", 70, lambda g: (esc_line(g.uniform(-200, 200)) + g.uniform(-40, 6), g.uniform(-200, 200)), lambda x, y: True,
              scale=(0.8, 2.6), gap=5.0, seed=seed + 1, eye=eye, sink=0.2)
    P.scatter("Boulder", 10, near(0), lambda x, y: clear(x, y) and not on_track(x, y), scale=(0.6, 1.2), gap=12.0, seed=seed + 2, eye=eye, sink=0.15)
    # camel thorn: sparse on the plain, thick along the wadi where the water runs when it rains
    P.scatter("CamelThorn", 160, near(0), lambda x, y: clear(x, y) and not on_track(x, y), scale=(0.7, 1.5), gap=4.0, seed=seed + 3, eye=eye)
    P.scatter("CamelThorn", 160, lambda g: ((x := g.uniform(-80, ESC_X - 20)), float(wadi_y(x)) + g.gauss(0, 7)),
              lambda x, y: in_wadi(x, y) > 0.15 and clear(x, y), scale=(0.9, 1.7), gap=2.5, seed=seed + 4, eye=eye)
    P.scatter("CamelThorn", 500, lambda g: (g.uniform(200, 1400), g.uniform(-900, 900)), lambda x, y: True, scale=(0.9, 1.6), gap=8.0, seed=seed + 5, eye=eye)
    # the village under the escarpment, where the wadi comes down: houses, its dome and its slim tower, palms round the well
    vx, vy = float(esc_line(95.0)) - 34.0, 95.0
    rng = random.Random(seed + 6)
    lots = [(-14, -18, "FlatRoofHouse"), (2, -22, "FlatRoofHouseStone"), (18, -14, "FlatRoofHouseTwo"), (-20, 2, "FlatRoofHouseTwo"),
            (-2, 4, "DomeHouse"), (12, 8, "FlatRoofHouse"), (26, 4, "FlatRoofHouseStone"), (-8, 22, "FlatRoofHouseStone"), (8, 26, "FlatRoofHouse"),
            (22, 24, "FlatRoofHouseTwo"), (-24, 20, "FlatRoofHouse"), (6, -6, "SlimTower")]
    for dx, dy, name in lots:
        P.put(name, vx + dx, vy + dy, turn=math.radians(rng.choice((0, 90, 180, 270)) + rng.uniform(-4, 4)), eye=eye, rng=rng)
    P.scatter("DatePalm", 34, lambda g: (vx + g.uniform(-30, 30), vy + g.choice((-1, 1)) * g.uniform(34, 70)), lambda x, y: True,
              scale=(0.8, 1.25), gap=5.0, seed=seed + 7, eye=eye)
    P.scatter("DatePalm", 7, lambda g: ((x := g.uniform(ESC_X - 110, ESC_X - 50)), float(wadi_y(x)) + g.gauss(0, 5)), lambda x, y: True,
              scale=(0.8, 1.2), gap=6.0, seed=seed + 8, eye=eye)
    # the telegraph line along the track, a pole every 55 m
    poles = []
    for k in range(-6, 30):
        x = -300.0 + k * 55.0
        if x > ESC_X - 10:
            break
        y = float(track_y(x)) - 14.0
        tx = math.atan2(float(track_y(x + 1) - track_y(x)), 1.0)
        P.put("TelegraphPole", x, y, turn=tx, eye=eye)
        poles.append((x, y, land.z(x, y), tx))
    wires(poles, high=6.62)
    # a sangar, its open back to the camera, on the rise beyond the tanks
    P.put("Sangar", 58.0, 14.0, turn=math.radians(10), eye=eye)
    P.put("Sangar", 74.0, -26.0, turn=math.radians(-15), eye=eye)
    if units != "none":
        stand_units(land, [("M13", 8.0, 4.0, 200.0, {}), ("Semovente", 24.0, 12.0, 192.0, {}), ("L3", -4.0, -7.0, 214.0, {}),
                           ("PanzerIV", 30.0, -9.0, 186.0, {"gun": "short", "paint": "desert41"})], LOOKS["desert"]["dust"])
    return land, P, eye


def stand_units(land, units, dust_hex, dust=1.5):
    for name, x, y, heading, pick in units:
        v = vehicles.build(name, True, **pick)
        m = v.m
        ob, arm = works.to_scene(m, dust=dust, dust_hex=dust_hex)
        landmarks.put(m, ob, arm)
        arm.location = (x, y, land.z(x, y) + 0.02)
        arm.rotation_euler[2] = math.radians(heading)


# ---- the south of Italy -----------------------------------------------------------------------------------------
def lane_y(x):
    return 6.0 + 0.18 * x


def south_height(X, Y):
    r = np.hypot(X, Y)
    h = 0.6 * vnoise(X, Y, 50.0, 1.0) + 0.2 * vnoise(X, Y, 14.0, 2.0)
    h *= 0.35 + 0.65 * smoothstep(18.0, 60.0, np.hypot(X - 15.0, Y))
    h += 14.0 * smoothstep(120.0, 420.0, X) * (0.7 + 0.3 * vnoise(X, Y, 120.0, 3.0))          # (the ground climbs toward the hills)
    h += 160.0 * smoothstep(700.0, 2600.0, r) * (0.5 + 0.5 * vnoise(X, Y, 600.0, 4.0)) + 40.0 * smoothstep(500.0, 1500.0, r) * vnoise(X, Y, 200.0, 5.0)
    return h


def south_masks():
    def scrub(X, Y):
        return smoothstep(0.0, 0.35, vnoise(X, Y, 30.0, 8.0) + 0.3 * vnoise(X, Y, 7.0, 9.0)) * (1 - 0.8 * farm(X, Y))

    def farm(X, Y):                                                                                # (fields on the low ground, scrub on the rise)
        return smoothstep(0.15, -0.1, vnoise(X, Y, 260.0, 10.0)) * smoothstep(380.0, 200.0, X)

    def sand(X, Y):                                                                                # (pale dusty earth along the lane)
        return np.exp(-((Y - lane_y(X)) / 2.6) ** 2) * smoothstep(-200.0, -150.0, X)
    return {"scrub": scrub, "sand": sand, "farm": farm}


SOUTH_TRACK = {"a": 6.0, "b": 0.18, "amp": 0.0, "wave": 50.0, "x_end": 125.0, "ruts": ((-0.85, 0.22), (0.85, 0.22))}


def drysouth(units="italy", seed=9):
    stage.HAZE = LOOKS["drysouth"]["haze"]
    sun = stage.sky(sun_elev=34.0, sun_turn=-48.0, strength=3.6, level=0.5, clouds=True)
    masks = south_masks()
    land = Land("drysouth", south_height, masks, track=SOUTH_TRACK)
    eye = (-30.0, -22.0)
    P = Props(land)
    clear = lambda x, y: math.hypot(x - 15, y - 1) > 24                                              # noqa: E731
    off_lane = lambda x, y: abs(y - lane_y(x)) > 3.5                                                 # noqa: E731
    # olive groves in rows behind dry-stone walls, on either side of the lane
    walls = []
    for (x0, y0, nx, ny, turn) in ((40, 26, 9, 6, 0.0), (60, -48, 8, 5, 0.06), (150, 50, 10, 7, 0.1), (180, -60, 9, 7, -0.05)):
        rng = random.Random(seed + x0)
        for i in range(nx):
            for j in range(ny):
                x = x0 + i * 7.5 + rng.uniform(-0.6, 0.6)
                y = y0 + j * 7.5 + rng.uniform(-0.6, 0.6)
                if off_lane(x, y) and clear(x, y) and rng.random() > 0.06:
                    P.put("OliveTree", x, y, scale=rng.uniform(0.8, 1.15), eye=eye, rng=rng)
        walls.append((x0 - 5, y0 - 5, x0 + nx * 7.5 - 2, y0 - 5))
    for (xa, ya, xb, yb) in walls:
        L = math.hypot(xb - xa, yb - ya)
        t = math.atan2(yb - ya, xb - xa)
        for k in range(int(L / 6.0)):
            f = (k + 0.5) * 6.0 / L
            P.put("StoneWall", xa + (xb - xa) * f, ya + (yb - ya) * f, turn=t, eye=eye)
    # a farmhouse at the lane's head, a line of cypress up to it
    fx = 130.0
    P.put("Farmhouse", fx, float(lane_y(fx)) + 14.0, turn=math.radians(-80), eye=eye)
    for k in range(8):
        x = 52.0 + k * 10.0
        for s in (-1, 1):
            if s == 1:
                P.put("Cypress", x, float(lane_y(x)) + s * 4.2, scale=random.Random(k * 7 + s).uniform(0.85, 1.1), eye=eye)
    P.scatter("Stones", 160, lambda g: (g.uniform(-60, 200), g.uniform(-110, 120)), lambda x, y: clear(x, y) and off_lane(x, y), scale=(0.6, 1.2), gap=3.0, seed=seed, eye=eye)
    farm = masks["farm"]
    wild = lambda x, y: clear(x, y) and off_lane(x, y) and farm(np.array([[x]]), np.array([[y]]))[0, 0] < 0.3    # noqa: E731
    P.scatter("Macchia", 260, lambda g: (g.uniform(-60, 320), g.uniform(-160, 160)), wild, scale=(0.7, 1.5), gap=3.0, seed=seed + 1, eye=eye)
    P.scatter("Macchia", 700, lambda g: (g.uniform(300, 1400), g.uniform(-900, 900)), lambda x, y: True, scale=(1.0, 1.8), gap=10.0, seed=seed + 5, eye=eye)
    P.scatter("OliveTree", 260, lambda g: (g.uniform(300, 1300), g.uniform(-900, 900)), lambda x, y: True, scale=(0.9, 1.2), gap=14.0, seed=seed + 2, eye=eye)
    P.scatter("Cypress", 30, lambda g: (g.uniform(300, 1300), g.uniform(-900, 900)), lambda x, y: True, scale=(0.9, 1.2), gap=20.0, seed=seed + 3, eye=eye)
    P.scatter("Farmhouse", 6, lambda g: (g.uniform(350, 1200), g.uniform(-700, 700)), lambda x, y: True, scale=(1.0, 1.0), gap=150.0, seed=seed + 4, eye=eye)
    if units != "none":
        stand_units(land, [("M13", 8.0, 0.0, 196.0, {}), ("Semovente", 24.0, -10.0, 190.0, {}), ("PanzerIV", 28.0, 10.0, 184.0, {"gun": "long", "paint": "yellow"})],
                    LOOKS["drysouth"]["dust"], dust=1.1)
    return land, P, eye


VIEWS = {
    "wide": ((-32.0, -26.0, 3.1), (40.0, 6.0, 0.6), 32.0),
    "low": ((-12.0, -14.0, 1.25), (18.0, 4.0, 1.6), 30.0),
    "game": ((-46.0, -46.0, 30.0), (24.0, 4.0, 0.0), 35.0),
    "far": ((-30.0, -40.0, 14.0), (220.0, 60.0, 10.0), 40.0),
}


def main():
    look, out = sys.argv[1], sys.argv[2]
    w, h = (int(a) for a in (sys.argv[3] if len(sys.argv) > 3 else "1600x900").split("x"))
    spp = int(sys.argv[4]) if len(sys.argv) > 4 else 32
    opt = dict(a.split("=", 1) for a in sys.argv[5:] if "=" in a)
    kit.fresh()
    if look == "desert":
        land, P, eye = desert(units=opt.get("units", "desert"))
    else:
        land, P, eye = drysouth(units=opt.get("units", "italy"))
    print("props:", P.count, flush=True)
    kit.render_setup(w, h, samples=spp)
    bpy.context.scene.cycles.use_adaptive_sampling = True
    bpy.context.scene.view_settings.exposure = float(opt.get("exposure", -0.35))
    e, t, lens = VIEWS[opt.get("view", "wide")]
    cam = kit.camera("cam", lens)
    cam.data.clip_end = 12000.0
    ex, ey, ez = e
    kit.look_at(cam, (ex, ey, land.z(ex, ey) + ez), (t[0], t[1], land.z(t[0], t[1]) + t[2]))
    kit.still(out)


if __name__ == "__main__":
    main()
