"""What the pictures add to the works' paints that the numbers do not carry (board marks in cast concrete, the weave of
sacking, grain, rubbed edges), and the helpers that stand works on the ground for a picture.

Picture-only: the game paints by its own materials; the files carry plain paints, wear and soot.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Euler, Matrix, Quaternion, Vector  # noqa: E402

import bits  # noqa: E402
import kit  # noqa: E402
import rigid  # noqa: E402
from stage import _math, _mix  # noqa: E402


def _noise(nt, tc, scale, detail=5.0, rough=0.55, stretch=None, coords="Object"):
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = scale
    nz.inputs["Detail"].default_value = detail
    nz.inputs["Roughness"].default_value = rough
    if stretch:
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = stretch
        nt.links.new(tc.outputs[coords], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
    else:
        nt.links.new(tc.outputs[coords], nz.inputs["Vector"])
    return nz.outputs[0]


def _ramp(nt, fac, lo, hi, p0=0.0, p1=1.0):
    r = nt.nodes.new("ShaderNodeValToRGB")
    r.color_ramp.elements[0].position, r.color_ramp.elements[0].color = p0, (lo, lo, lo, 1)
    r.color_ramp.elements[1].position, r.color_ramp.elements[1].color = p1, (hi, hi, hi, 1)
    nt.links.new(fac, r.inputs["Fac"])
    return r.outputs["Color"]


def _times(nt, col, by, fac=1.0):
    _, _, a, b, o = _mix(nt, "MULTIPLY", fac)
    nt.links.new(col, a)
    nt.links.new(by, b)
    return o


def _toward(nt, col, to, fac):
    """Mix a colour toward another by a factor that may be a socket."""
    _, f, a, b, o = _mix(nt)
    if isinstance(fac, (int, float)):
        f.default_value = fac
    else:
        nt.links.new(fac, f)
    nt.links.new(col, a)
    if isinstance(to, tuple):
        b.default_value = (*to, 1.0)
    else:
        nt.links.new(to, b)
    return o


def _band(nt, v, lo, hi, a=0.0, b=1.0):
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.interpolation_type = "SMOOTHSTEP"
    mr.inputs["From Min"].default_value, mr.inputs["From Max"].default_value = lo, hi
    mr.inputs["To Min"].default_value, mr.inputs["To Max"].default_value = a, b
    nt.links.new(v, mr.inputs["Value"])
    return mr.outputs["Result"]


def _bump(nt, bsdf, height, strength=0.3, dist=0.01):
    b = nt.nodes.new("ShaderNodeBump")
    b.inputs["Strength"].default_value = strength
    b.inputs["Distance"].default_value = dist
    nt.links.new(height, b.inputs["Height"])
    nt.links.new(b.outputs["Normal"], bsdf.inputs["Normal"])


def _edges(nt, col, light=(0.75, 0.75, 0.72), much=0.5, sharp=(0.02, 0.22), radius=0.02):
    """Rubbed edges: where the skin turns sharply (found by the Bevel node: its rounded facing differs from the true
    one only near an edge), mix toward a paler colour, broken up so the rubbing is not a ruled line."""
    bev = nt.nodes.new("ShaderNodeBevel")
    bev.samples = 3
    bev.inputs["Radius"].default_value = radius
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    dot = nt.nodes.new("ShaderNodeVectorMath")
    dot.operation = "DOT_PRODUCT"
    nt.links.new(bev.outputs["Normal"], dot.inputs[0])
    nt.links.new(geo.outputs["Normal"], dot.inputs[1])
    edge = _math(nt, "SUBTRACT", 1.0, dot.outputs["Value"], clamp=True)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nz = _noise(nt, tc, 22.0, 4.0)
    edge = _math(nt, "MULTIPLY", edge, _math(nt, "MULTIPLY", nz, 2.2))
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["From Min"].default_value, mr.inputs["From Max"].default_value = sharp
    mr.inputs["To Min"].default_value, mr.inputs["To Max"].default_value = 0.0, much
    nt.links.new(edge, mr.inputs["Value"])
    _, f, a, b, o = _mix(nt)
    nt.links.new(mr.outputs["Result"], f)
    nt.links.new(col, a)
    b.default_value = (*light, 1.0)
    return o


def works_look(paint, mat, nt, bsdf, mix):
    key = paint.split(".")[0]
    col = mix.outputs[2]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    if key == "concrete":
        # the marks of the shuttering boards (level lines about 0.18 m apart, each board a shade of its own), blotches,
        # rain streaks down the face, damp and green at the foot
        mass = paint == "concrete.mass"            # a great work: shuttered in panels half a metre high, poured two and a half metres a day
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (0.0, 0.0, 0.628 if mass else 1.745)
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        wave = nt.nodes.new("ShaderNodeTexWave")
        wave.wave_type, wave.bands_direction = "BANDS", "Z"
        wave.inputs["Scale"].default_value = 1.0
        wave.inputs["Distortion"].default_value = 0.25
        wave.inputs["Detail"].default_value = 1.0
        wave.inputs["Detail Scale"].default_value = 0.6
        nt.links.new(mp.outputs["Vector"], wave.inputs["Vector"])
        boards = _ramp(nt, wave.outputs[1], 0.62, 1.0, 0.0, 0.05)
        # (each board its own shade: a noise that only changes with height, in board-high steps)
        each = _ramp(nt, _noise(nt, tc, 1.0, 0.0, 0.5, (0.0, 0.0, 2.0 if mass else 5.6)), 0.86, 1.1, 0.3, 0.7)
        blot = _ramp(nt, _noise(nt, tc, 0.9, 6.0), 0.66, 1.16, 0.3, 0.7)
        streak = _ramp(nt, _noise(nt, tc, 1.0, 5.0, 0.62, (3.2, 3.2, 0.15)), 0.66, 1.08, 0.36, 0.68)
        pit = _ramp(nt, _noise(nt, tc, 38.0, 2.0, 0.5), 0.55, 1.0, 0.24, 0.36)
        grit = _noise(nt, tc, 55.0, 3.0)
        quiet = paint == "concrete.in"
        # (no two stretches of wall alike: the board marks come and go, the whole face runs lighter and darker)
        here = _ramp(nt, _noise(nt, tc, 0.5, 3.0), 0.15, 1.0, 0.35, 0.65)
        col = _times(nt, col, _toward(nt, boards, (1.0, 1.0, 1.0), _math(nt, "SUBTRACT", 1.0, here)), 0.2 if quiet else (0.3 if mass else 0.55))
        col = _times(nt, col, each, 0.3 if quiet else 0.7)
        col = _times(nt, col, _ramp(nt, _noise(nt, tc, 0.23, 3.0), 0.8, 1.14, 0.3, 0.7), 1.0)
        col = _times(nt, col, blot, 0.9)
        col = _times(nt, col, streak, 0.9)
        col = _times(nt, col, pit, 0.5)
        if not quiet:
            # the joints between one day's pour and the next: a darker, wavering line every metre and a third
            mp2 = nt.nodes.new("ShaderNodeMapping")
            mp2.inputs["Scale"].default_value = (0.0, 0.0, 0.1208 if mass else 0.2417)
            mp2.inputs["Location"].default_value = (0.0, 0.0, 0.11)
            nt.links.new(tc.outputs["Object"], mp2.inputs["Vector"])
            lift = nt.nodes.new("ShaderNodeTexWave")
            lift.wave_type, lift.bands_direction = "BANDS", "Z"
            lift.inputs["Scale"].default_value = 1.0
            nt.links.new(mp2.outputs["Vector"], lift.inputs["Vector"])
            wob = _math(nt, "ADD", _math(nt, "MULTIPLY", _noise(nt, tc, 2.2, 3.0), 0.08), -0.04)
            col = _times(nt, col, _ramp(nt, _math(nt, "ADD", lift.outputs[1], wob), 0.62, 1.0, 0.0, 0.025 if mass else 0.05), 0.6 if mass else 0.8)
            # rain run-off: dark where something overhangs (a roof's edge, an embrasure's hood: found by looking up from
            # the wall), dragged down the face in streaks
            ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
            ao.samples = 6
            ao.only_local = True
            ao.inputs["Distance"].default_value = 0.9
            ao.inputs["Normal"].default_value = (0.0, 0.0, 1.0)
            under = _band(nt, ao.outputs["AO"], 0.47, 0.16)
            drip = _ramp(nt, _noise(nt, tc, 1.0, 4.0, 0.6, (4.5, 4.5, 0.1)), 0.0, 1.0, 0.38, 0.62)
            run = _math(nt, "MULTIPLY", under, _math(nt, "ADD", _math(nt, "MULTIPLY", drip, 0.6), 0.4), clamp=True)
            # (and long drips from the top of a wall, in places)
            long_ = _math(nt, "MULTIPLY", _ramp(nt, _noise(nt, tc, 1.0, 3.0, 0.5, (6.0, 6.0, 0.05)), 0.0, 1.0, 0.56, 0.66), _ramp(nt, _noise(nt, tc, 0.7, 2.0), 0.0, 1.0, 0.45, 0.6))
            col = _toward(nt, col, (0.045, 0.043, 0.038), _math(nt, "MULTIPLY", _math(nt, "MAXIMUM", run, _math(nt, "MULTIPLY", long_, 0.62)), 0.84))
            # damp and moss creeping up from the ground, broken by the blotches; earth splashed up the first foot
            sep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(tc.outputs["Object"], sep.inputs[0])
            damp = _math(nt, "MULTIPLY", _band(nt, sep.outputs["Z"], 0.02, 0.95, 1.0, 0.0), _ramp(nt, _noise(nt, tc, 1.7, 5.0), 0.25, 1.0, 0.28, 0.62), clamp=True)
            col = _toward(nt, col, (0.085, 0.1, 0.055), _math(nt, "MULTIPLY", damp, 0.78))
            splash = _math(nt, "MULTIPLY", _band(nt, sep.outputs["Z"], 0.0, 0.42, 1.0, 0.0), _ramp(nt, _noise(nt, tc, 4.0, 5.0), 0.2, 1.0, 0.3, 0.7), clamp=True)
            col = _toward(nt, col, (0.15, 0.115, 0.075), _math(nt, "MULTIPLY", splash, 0.6))
        col = _edges(nt, col, (0.62, 0.62, 0.58), 0.45, radius=0.03)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.93
        h = _math(nt, "ADD", _math(nt, "MULTIPLY", grit, 0.25), _math(nt, "MULTIPLY", wave.outputs[1], -0.2))
        h = _math(nt, "ADD", h, _math(nt, "MULTIPLY", pit, 0.5))
        _bump(nt, bsdf, h, 0.55, 0.012)
    elif key == "sandbag":
        # sacking: a coarse weave, each bag a little different, darker where it is damp underneath
        weave = nt.nodes.new("ShaderNodeTexWave")
        weave.wave_type, weave.bands_direction = "BANDS", "DIAGONAL"
        weave.inputs["Scale"].default_value = 210.0
        weave.inputs["Distortion"].default_value = 1.2
        nt.links.new(tc.outputs["Object"], weave.inputs["Vector"])
        mott = _ramp(nt, _noise(nt, tc, 5.0, 6.0), 0.7, 1.18, 0.25, 0.75)
        col = _times(nt, col, mott, 0.85)
        col = _times(nt, col, _ramp(nt, weave.outputs[1], 0.86, 1.05), 0.6)
        # (greyed and browned with dirt in the lowest courses, where rain splashes the earth up)
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        foot = _math(nt, "MULTIPLY", _band(nt, sep.outputs["Z"], 0.0, 0.34, 1.0, 0.0), _ramp(nt, _noise(nt, tc, 3.0, 5.0), 0.35, 1.0, 0.3, 0.7), clamp=True)
        col = _toward(nt, col, (0.12, 0.095, 0.065), _math(nt, "MULTIPLY", foot, 0.6))
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.97
        if "Sheen Weight" in bsdf.inputs:
            bsdf.inputs["Sheen Weight"].default_value = 0.25
        h = _math(nt, "ADD", _math(nt, "MULTIPLY", weave.outputs[1], 0.5), _math(nt, "MULTIPLY", _noise(nt, tc, 9.0, 5.0), 1.2))
        _bump(nt, bsdf, h, 0.35, 0.006)
    elif key in ("earth", "turf"):
        # Thrown earth, and the turf that has grown over it. The turf is the field's own grass (the stage's recipe:
        # two greens of late summer, dry patches, fine mottling), so a bank runs into the field with no seam. Where a
        # face is earth turfed point by point (the coat reaches the picture as a tint), the two do not fade evenly into
        # each other: the grass breaks up into the earth in ragged patches.
        mott = _ramp(nt, _noise(nt, tc, 2.4, 8.0), 0.62, 1.25, 0.25, 0.75)
        clod = _noise(nt, tc, 14.0, 6.0, 0.7)

        def tone(fac, a, b, pa, pb):
            r = nt.nodes.new("ShaderNodeValToRGB")
            r.color_ramp.elements[0].position, r.color_ramp.elements[0].color = pa, (*kit.hex_rgb(a), 1)
            r.color_ramp.elements[1].position, r.color_ramp.elements[1].color = pb, (*kit.hex_rgb(b), 1)
            nt.links.new(fac, r.inputs["Fac"])
            return r.outputs["Color"]

        grass = _toward(nt, tone(_noise(nt, tc, 0.025, 4.0), "6F7B42", "8D8650", 0.35, 0.65), tone(_noise(nt, tc, 0.15, 8.0), "7A8048", "9C8C5E", 0.45, 0.7), 0.45)
        grass = _times(nt, grass, _ramp(nt, _noise(nt, tc, 2.5, 10.0), 0.54, 1.0, 0.25, 0.75), 0.5)
        if key == "turf":
            col = grass
        else:
            if paint == "earth":
                # (the field's own bare earth, as the stage has it where the grass is trodden away)
                earth = _times(nt, tone(_noise(nt, tc, 0.15, 8.0), "70604A", "8C7958", 0.3, 0.7), _ramp(nt, _noise(nt, tc, 2.5, 10.0), 0.54, 1.0, 0.25, 0.75), 0.5)
            else:
                base = nt.nodes.new("ShaderNodeRGB")
                base.outputs[0].default_value = (*kit.hex_rgb(bits.PAINTS.get(paint, bits.PAINTS["earth"])), 1.0)
                earth = _times(nt, base.outputs[0], mott, 0.9)
            # how far the turf has grown over each point (`stand` lays it on the points, spread wide and soft); the
            # edge itself is the grass's own, broken and wandering, never the mesh's
            att = nt.nodes.new("ShaderNodeAttribute")
            att.attribute_name = "grown"
            stray = _math(nt, "ADD", _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _noise(nt, tc, 2.2, 9.0, 0.65), 0.5), 1.25),
                          _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", _noise(nt, tc, 0.6, 4.0), 0.5), 0.7))
            grown = _band(nt, _math(nt, "ADD", att.outputs["Fac"], stray), 0.42, 0.6)
            col = _toward(nt, earth, grass, grown)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.98
        _bump(nt, bsdf, clod, 0.7, 0.03)
    elif key == "grass":
        # blades of grass: lit as if they looked mostly upward, letting light through, as the stage's own are
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.75
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        nrm = nt.nodes.new("ShaderNodeVectorMath")
        nrm.operation = "NORMALIZE"
        mixn = nt.nodes.new("ShaderNodeMix")
        mixn.data_type = "VECTOR"
        mixn.inputs[0].default_value = 0.7
        nt.links.new(geo.outputs["Normal"], mixn.inputs[4])
        mixn.inputs[5].default_value = (0.0, 0.0, 1.0)
        nt.links.new(mixn.outputs[1], nrm.inputs[0])
        nt.links.new(nrm.outputs["Vector"], bsdf.inputs["Normal"])
        out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
        if out.inputs["Surface"].links and out.inputs["Surface"].links[0].from_node == bsdf:
            tr = nt.nodes.new("ShaderNodeBsdfTranslucent")
            nt.links.new(col, tr.inputs["Color"])
            both = nt.nodes.new("ShaderNodeMixShader")
            both.inputs[0].default_value = 0.45
            nt.links.new(bsdf.outputs[0], both.inputs[1])
            nt.links.new(tr.outputs[0], both.inputs[2])
            nt.links.new(both.outputs[0], out.inputs["Surface"])
    elif key in ("timber", "planks"):
        grain = _noise(nt, tc, 9.0, 6.0, 0.6, (1.0, 14.0, 14.0))
        mott = _ramp(nt, _noise(nt, tc, 3.0, 4.0), 0.75, 1.15, 0.3, 0.7)
        col = _times(nt, col, _ramp(nt, grain, 0.7, 1.12, 0.3, 0.7), 0.8)
        col = _times(nt, col, mott, 0.7)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.88
        _bump(nt, bsdf, grain, 0.25, 0.004)
    elif key in ("upper", "upper2", "wood"):
        # field paint on steel: faded unevenly, rubbed to metal on the edges
        mott = _ramp(nt, _noise(nt, tc, 3.5, 6.0), 0.8, 1.16, 0.3, 0.7)
        col = _times(nt, col, mott, 0.75)
        col = _edges(nt, col, (0.36, 0.36, 0.36), 0.7)
        nt.links.new(col, bsdf.inputs["Base Color"])
        r = nt.nodes.new("ShaderNodeMapRange")
        r.inputs["To Min"].default_value, r.inputs["To Max"].default_value = 0.5, 0.75
        nt.links.new(_noise(nt, tc, 6.0, 4.0), r.inputs["Value"])
        nt.links.new(r.outputs["Result"], bsdf.inputs["Roughness"])
    elif key in ("steel", "iron"):
        col = _edges(nt, col, (0.5, 0.5, 0.5), 0.6, radius=0.012)
        nt.links.new(col, bsdf.inputs["Base Color"])
        # (dull: a broad flat of weathered steel must not mirror the sky)
        r = nt.nodes.new("ShaderNodeMapRange")
        r.inputs["To Min"].default_value, r.inputs["To Max"].default_value = 0.5, 0.8
        nt.links.new(_noise(nt, tc, 7.0, 5.0), r.inputs["Value"])
        nt.links.new(r.outputs["Result"], bsdf.inputs["Roughness"])
        bsdf.inputs["Metallic"].default_value = 0.5
    elif key in ("rust", "wire"):
        mott = _ramp(nt, _noise(nt, tc, 8.0, 6.0), 0.6, 1.35, 0.25, 0.75)
        col = _times(nt, col, mott, 0.9)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.8
        bsdf.inputs["Metallic"].default_value = 0.35
    elif key in ("canvas", "net", "garnish", "leather"):
        mott = _ramp(nt, _noise(nt, tc, 6.0, 6.0), 0.72, 1.2, 0.25, 0.75)
        col = _times(nt, col, mott, 0.8)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.95
    elif key == "brass":
        bsdf.inputs["Roughness"].default_value = 0.3
        bsdf.inputs["Metallic"].default_value = 1.0
    elif key == "glass":
        bsdf.inputs["Roughness"].default_value = 0.06
    elif key == "void":
        bsdf.inputs["Roughness"].default_value = 1.0
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = 0.0


def grown(model, me, passes=4):
    """How far turf has grown over each point of the mesh, 0 to 1, as the attribute `grown` for the earth's look. The
    model says turf or earth point by point; here that is spread from point to point a few times, so it runs from
    turf to earth over a metre or so and the look can break the edge up as grass does."""
    import numpy as np
    names = list(model.paints)
    turf = {i for i, n in enumerate(names) if n.split(".")[0] == "turf"}
    nv = len(me.vertices)
    g = np.array([1.0 if c in turf else 0.0 for c in model.vcoat[:nv]] + [0.0] * max(0, nv - len(model.vcoat)))
    if g.any():
        e = np.zeros(len(me.edges) * 2, dtype=np.int32)
        me.edges.foreach_get("vertices", e)
        e = e.reshape(-1, 2)
        n = np.zeros(nv)
        np.add.at(n, e[:, 0], 1.0)
        np.add.at(n, e[:, 1], 1.0)
        for _ in range(passes):
            t = np.zeros(nv)
            np.add.at(t, e[:, 0], g[e[:, 1]])
            np.add.at(t, e[:, 1], g[e[:, 0]])
            g = np.where(n > 0, 0.4 * g + 0.6 * t / np.maximum(n, 1.0), g)
    a = me.attributes.new("grown", "FLOAT", "POINT")
    a.data.foreach_set("value", g)


def stand(model, at=(0.0, 0.0, 0.0), yaw=0.0, crease=50.0):
    """The model as a Blender mesh on its armature, stood on the ground: returns (mesh object, armature)."""
    # (the library keeps one material for each model name and paint: two armies' builds of one gun in one picture
    # must not share theirs)
    keep, wears = model.name, model.info.get("wears", "_default")
    if wears != "_default":
        model.name = keep + "@" + wears
    try:
        ob, arm = rigid.to_blender(model, works_look, crease=crease)
    finally:
        model.name = keep
    grown(model, ob.data)
    arm.location = Vector(at)
    arm.rotation_euler = Euler((0.0, 0.0, math.radians(yaw)), "XYZ")
    bpy.context.view_layer.update()
    return ob, arm


def where(model, arm, pose, socket):
    """A socket's place and facing on the ground under a pose."""
    at, facing = rigid.place(model, pose, socket)
    mw = arm.matrix_world
    return mw @ at, (mw.to_3x3() @ facing).normalized()


def render_setup(w=1280, h=720, samples=40):
    sc = kit.render_setup(w, h, samples=samples)
    sc.cycles.transparent_max_bounces = 32
    sc.cycles.max_bounces = 5
    sc.cycles.diffuse_bounces = 3
    return sc


def cam(lens=50.0):
    c = kit.camera("cam", lens)
    c.data.clip_end = 6000.0
    return c
