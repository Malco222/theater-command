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
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (0.0, 0.0, 1.745)
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
        each = _ramp(nt, _noise(nt, tc, 1.0, 0.0, 0.5, (0.0, 0.0, 5.6)), 0.86, 1.1, 0.3, 0.7)
        blot = _ramp(nt, _noise(nt, tc, 0.9, 6.0), 0.66, 1.16, 0.3, 0.7)
        streak = _ramp(nt, _noise(nt, tc, 1.0, 5.0, 0.62, (3.2, 3.2, 0.15)), 0.66, 1.08, 0.36, 0.68)
        pit = _ramp(nt, _noise(nt, tc, 38.0, 2.0, 0.5), 0.55, 1.0, 0.24, 0.36)
        grit = _noise(nt, tc, 55.0, 3.0)
        quiet = paint == "concrete.in"
        col = _times(nt, col, boards, 0.2 if quiet else 0.6)
        col = _times(nt, col, each, 0.3 if quiet else 0.8)
        col = _times(nt, col, blot, 0.9)
        col = _times(nt, col, streak, 0.9)
        col = _times(nt, col, pit, 0.5)
        if not quiet:
            # damp and moss creeping up from the ground, broken by the blotches
            sep = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(tc.outputs["Object"], sep.inputs[0])
            foot = nt.nodes.new("ShaderNodeMapRange")
            foot.inputs["From Min"].default_value, foot.inputs["From Max"].default_value = 0.05, 0.75
            foot.inputs["To Min"].default_value, foot.inputs["To Max"].default_value = 1.0, 0.0
            nt.links.new(sep.outputs["Z"], foot.inputs["Value"])
            damp = _math(nt, "MULTIPLY", foot.outputs["Result"], _ramp(nt, _noise(nt, tc, 1.7, 5.0), 0.2, 1.0, 0.3, 0.65), clamp=True)
            _, f, a, b2, o = _mix(nt)
            nt.links.new(_math(nt, "MULTIPLY", damp, 0.62), f)
            nt.links.new(col, a)
            b2.default_value = (0.105, 0.115, 0.07, 1.0)
            col = o
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
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.97
        if "Sheen Weight" in bsdf.inputs:
            bsdf.inputs["Sheen Weight"].default_value = 0.25
        h = _math(nt, "ADD", _math(nt, "MULTIPLY", weave.outputs[1], 0.5), _math(nt, "MULTIPLY", _noise(nt, tc, 9.0, 5.0), 1.2))
        _bump(nt, bsdf, h, 0.35, 0.006)
    elif key in ("earth", "turf"):
        mott = _ramp(nt, _noise(nt, tc, 2.4, 8.0), 0.62, 1.25, 0.25, 0.75)
        clod = _noise(nt, tc, 14.0, 6.0, 0.7)
        col = _times(nt, col, mott, 0.9)
        nt.links.new(col, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.98
        _bump(nt, bsdf, clod, 0.7, 0.03)
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
        bsdf.inputs["Roughness"].default_value = 0.42
        bsdf.inputs["Metallic"].default_value = 0.75
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


def stand(model, at=(0.0, 0.0, 0.0), yaw=0.0, crease=50.0):
    """The model as a Blender mesh on its armature, stood on the ground: returns (mesh object, armature)."""
    ob, arm = rigid.to_blender(model, works_look, crease=crease)
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
