"""Volume puffs: smoke and dust as soft volumes in Cycles, not shells with a painted edge.

A surface puff (stage.Effects) is a ball whose skin fades at its rim. It can never quite lose the ball: seen edge on,
or many together, it shows rims, beads and cotton wool. A volume puff is a cloud of density with no skin at all. It
shades itself (lit on top, dark underneath), lets light through at its thin edges, and glows when the sun is behind it.
The API is the same as stage.Effects.puff, so a burst can be laid in either.
"""
import math

import bpy
from mathutils import Vector


def _m(nt, op, a, b=None, clamp=False):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation = op
    n.use_clamp = clamp
    for i, v in enumerate((a, b)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[i].default_value = v
        else:
            nt.links.new(v, n.inputs[i])
    return n.outputs[0]


def volume_material(name="vol_smoke", lumpy=1.6, edge=0.45, scatter=0.5, aniso=0.35, dens=3.0, hot=False):
    """`lumpy` is how many billows across one puff, `edge` how much of the puff is torn away by them,
    `scatter` how much light the stuff throws back (dust high, soot low), `aniso` how strongly it glows toward
    the sun behind it, `dens` how thick a puff of alpha 1 is (per metre at radius 1; scaled by the object)."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    out = nt.nodes["Material Output"]
    info = nt.nodes.new("ShaderNodeObjectInfo")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    vm = nt.nodes.new("ShaderNodeVectorMath")
    vm.operation = "LENGTH"
    nt.links.new(tc.outputs["Object"], vm.inputs[0])
    r = vm.outputs["Value"]
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.noise_dimensions = "4D"
    nz.inputs["Scale"].default_value = lumpy
    nz.inputs["Detail"].default_value = 3.0
    nz.inputs["Roughness"].default_value = 0.58
    nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
    nt.links.new(_m(nt, "MULTIPLY", info.outputs["Random"], 53.0), nz.inputs["W"])
    n = nz.outputs["Fac"]
    # thick in the middle, gone at the rim, and the billows eat into it from outside in
    core = _m(nt, "SUBTRACT", 1.0, r, clamp=True)
    shape = _m(nt, "ADD", _m(nt, "MULTIPLY", core, 1.0 + edge), _m(nt, "MULTIPLY", _m(nt, "SUBTRACT", n, 0.5), 2.2 * edge))
    shape = _m(nt, "MULTIPLY", _m(nt, "SUBTRACT", shape, 0.18), 2.0, clamp=True)
    shape = _m(nt, "POWER", shape, 1.4)
    d = _m(nt, "MULTIPLY", _m(nt, "MULTIPLY", shape, info.outputs["Alpha"]), dens)
    pv = nt.nodes.new("ShaderNodeVolumePrincipled")
    nt.links.new(info.outputs["Color"], pv.inputs["Color"])
    nt.links.new(d, pv.inputs["Density"])
    pv.inputs["Anisotropy"].default_value = aniso
    # soot drinks light; dust throws it back. Absorption is the colour's complement scaled by how dark the stuff is.
    pv.inputs["Absorption Color"].default_value = (1.0 - scatter, 1.0 - scatter, 1.0 - scatter, 1.0)
    if hot:
        nt.links.new(info.outputs["Color"], pv.inputs["Emission Color"])
        nt.links.new(_m(nt, "MULTIPLY", shape, 6.0), pv.inputs["Emission Strength"])
    nt.links.new(pv.outputs["Volume"], out.inputs["Volume"])
    return m


class Volumes:
    """Puffs as volumes, laid with the same arguments as stage.Effects.puff (the surface kinds are ignored)."""

    KINDS = {
        "smoke": dict(lumpy=1.5, edge=0.5, scatter=0.35, aniso=0.3, dens=2.2),
        "dust": dict(lumpy=1.8, edge=0.55, scatter=0.75, aniso=0.45, dens=1.6),
        "earth": dict(lumpy=2.4, edge=0.7, scatter=0.25, aniso=0.2, dens=5.0),
    }

    def __init__(self, fx):
        self.fx = fx
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1.0)
        ball = bpy.context.object
        self.mesh = ball.data
        bpy.data.objects.remove(ball)
        self.meshes = {}
        for k, kw in self.KINDS.items():
            me = self.mesh.copy()
            me.materials.append(volume_material("vol_" + k, **kw))
            self.meshes[k] = me
            fx.meshes["v_" + k] = me

    def puff(self, t0, at, kind="smoke", **kw):
        p = self.fx.puff(t0, at, kind="v_" + kind, **kw)
        p["ob"].visible_shadow = True     # (a volume casts its own shade: that is the point of it)
        return p
