"""Several works stood together on one piece of ground, for the pictures and the clips."""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Quaternion, Vector  # noqa: E402

import ground  # noqa: E402
import look  # noqa: E402
import rigid  # noqa: E402
import works  # noqa: E402


# (works whose own file does not say what stands at their sockets)
TAKES = {"FlakPit": {"gun": "HeavyFlak"}, "SandbagRing": {"gun": "LightFlak"}}


class Item:
    def __init__(self, name, m, ob, arm, at, yaw):
        self.name, self.m, self.ob, self.arm, self.at, self.yaw = name, m, ob, arm, at, yaw
        self.pose = {}

    def set(self, *poses):
        """Lays poses over each other and sets the work so (parts not named go back to rest)."""
        self.pose = rigid.merge(*poses) if poses else {}
        rigid.apply_pose(self.arm, self.pose, model=self.m)
        return self.pose

    def clip(self, name, t):
        c = self.m.clips.get(name)
        return rigid.pose_of(self.m, c, max(0.0, min(t, c["long"])) if not c.get("loop") else t) if c else {}

    def aim(self, part, degrees):
        return rigid.aim(self.m, part, degrees)

    def where(self, socket):
        """A socket's place and facing on the ground as the work is posed now."""
        bpy.context.view_layer.update()
        return look.where(self.m, self.arm, self.pose, socket)

    def has(self, socket):
        return any(s["name"] == socket for s in self.m.sockets)


class Set:
    def __init__(self):
        self.items = []

    def put(self, name, at=(0.0, 0.0, 0.0), yaw=0.0, rest="idle", mount=True, **kw):
        at = tuple(at) + (0.0,) * (3 - len(at))
        m = works.BUILDERS[name](True, **kw)
        ob, arm = look.stand(m, at, yaw)
        it = Item(name, m, ob, arm, at, yaw)
        if rest and rest in m.clips:
            it.set(it.clip(rest, 0.0))
        self.items.append(it)
        # what stands at its sockets as a model of its own (a flak pit's gun, a tower's guns)
        it.mounted = {}
        if mount:
            for socket, what in {**TAKES.get(name, {}), **m.info.get("takes", {})}.items():
                if what in works.BUILDERS and it.has(socket):
                    p, d = it.where(socket)
                    it.mounted[socket] = self.put(what, tuple(p), math.degrees(math.atan2(d.y, d.x)), mount=False, **({"nation": kw["nation"]} if "nation" in kw else {}))
        return it

    def guest(self, path, at, yaw=0.0, paint=None, turn=None):
        """Another thread's model (its .glb) stood at a socket for a picture: the vehicles thread's field gun in a pit
        or a casemate, in its owner's `paint` (a hex colour). The game puts the gun there itself; this is for the eye.
        `turn` {bone: (axis letter, degrees)} lays a bone of its rig."""
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=path)
        new = [o for o in bpy.data.objects if o not in before]
        for o in list(new):
            if o.type == "MESH" and o.parent is None:        # (the importer's shape for the bones)
                new.remove(o)
                bpy.data.objects.remove(o, do_unlink=True)
        holder = bpy.data.objects.new("guest", None)
        bpy.context.scene.collection.objects.link(holder)
        holder.location = tuple(at)
        holder.rotation_euler = (0.0, 0.0, math.radians(yaw))
        for o in new:
            if o.parent is None:
                o.parent = holder
            if o.type == "ARMATURE":                         # (at rest: no clip playing)
                o.animation_data_clear()
                for b in o.pose.bones:
                    b.location, b.rotation_quaternion, b.rotation_euler, b.scale = (0, 0, 0), (1, 0, 0, 0), (0, 0, 0), (1, 1, 1)
                for bone, (axis, deg) in (turn or {}).items():
                    b = o.pose.bones[bone]
                    b.rotation_mode = "XYZ"
                    setattr(b.rotation_euler, axis, math.radians(deg))
            if o.type == "EMPTY":
                o.hide_render = True
        if paint:
            for o in new:
                for mat in (o.data.materials if o.type == "MESH" else []):
                    if mat and mat.name.split(".")[0] in ("paint", "shade") and not mat.get("guest"):
                        _field_paint(mat, rigid.hex_rgb(paint), 0.6 if mat.name.startswith("shade") else 1.0)
                        mat["guest"] = True
        return holder

    def lay(self, **kw):
        """The ground under them all: each work's holes, trodden earth and bare patches where it stands."""
        digs, worn, clear, paths, tracks = [], [], [], [], []
        for it in self.items:
            if it.at[2] > 0.5:                       # (what stands up on another work wears no ground)
                continue
            xy = (it.at[0], it.at[1])
            c, s = math.cos(math.radians(it.yaw)), math.sin(math.radians(it.yaw))
            digs += [ground.placed(d, xy, it.yaw) for d in it.m.info.get("dig", [])]
            clear += [ground.placed(d, xy, it.yaw) for d in it.m.info.get("clear", [])]
            for w in list(it.m.info.get("worn", [])) + ground.skirt(it.m):
                worn.append((xy[0] + w[0] * c - w[1] * s, xy[1] + w[0] * s + w[1] * c) + tuple(w[2:]))
            paths += [ground.placed(p, xy, it.yaw) for p in ground.paths_of(it.m, seed=len(paths))]
            tracks += [ground.placed(p, xy, it.yaw) for p in ground.tracks_of(it.m)]
        worn += list(kw.pop("worn", []))
        paths += list(kw.pop("paths", []))
        tracks += list(kw.pop("tracks", []))
        self.ground = ground.Ground(digs=digs, worn=worn, clear=clear, paths=paths, tracks=tracks, **kw)
        return self.ground


def _field_paint(mat, rgb, gain=1.0):
    """A guest's plain paint given the look of ours: mottled, matt, dusty toward the ground."""
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 5.0
    noise.inputs["Detail"].default_value = 6.0
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeMapRange")
    ramp.inputs["From Min"].default_value, ramp.inputs["From Max"].default_value = 0.3, 0.7
    ramp.inputs["To Min"].default_value, ramp.inputs["To Max"].default_value = 0.78 * gain, 1.1 * gain
    nt.links.new(noise.outputs["Fac"], ramp.inputs["Value"])
    tint = nt.nodes.new("ShaderNodeMixRGB")
    tint.blend_type = "MULTIPLY"
    tint.inputs["Fac"].default_value = 1.0
    tint.inputs["Color1"].default_value = tuple(rgb[:3]) + (1.0,)
    nt.links.new(ramp.outputs["Result"], tint.inputs["Color2"])
    # dust: the field's earth on everything near the ground
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["Object"], sep.inputs["Vector"])
    low = nt.nodes.new("ShaderNodeMapRange")
    low.inputs["From Min"].default_value, low.inputs["From Max"].default_value = 0.0, 1.1
    low.inputs["To Min"].default_value, low.inputs["To Max"].default_value = 0.55, 0.0
    nt.links.new(sep.outputs["Z"], low.inputs["Value"])
    dust = nt.nodes.new("ShaderNodeMixRGB")
    nt.links.new(low.outputs["Result"], dust.inputs["Fac"])
    nt.links.new(tint.outputs["Color"], dust.inputs["Color1"])
    dust.inputs["Color2"].default_value = tuple(rigid.hex_rgb("7E6C51")[:3]) + (1.0,)
    nt.links.new(dust.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.68
    bsdf.inputs["Metallic"].default_value = 0.0


def ease(t, t0, t1):
    """0 before t0, 1 after t1, a smooth run between."""
    if t1 <= t0:
        return 1.0 if t >= t1 else 0.0
    k = min(max((t - t0) / (t1 - t0), 0.0), 1.0)
    return k * k * (3 - 2 * k)


def keys(pairs, t):
    """A value along (time, value) pairs, eased from each to the next."""
    if t <= pairs[0][0]:
        return pairs[0][1]
    for (ta, a), (tb, b) in zip(pairs, pairs[1:]):
        if t <= tb:
            return a + (b - a) * ease(t, ta, tb)
    return pairs[-1][1]
