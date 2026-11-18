"""The explosion test clip: a 105 mm salvo on the battle stage, built to the art director's sheet
(/mnt/project-files/art-director/explosions/explosions-look.md).

    python salvo.py <out folder> [WxHxSPP] [dusk] [only=a-b] [stills=i,j,k] [view=high:close|high:middle] [long=10]

Three shells land near a halted Panzer IV and a rifle squad: far (about 250 m off), middle (80 m) and close (25 m),
at frames 45, 102 and 150 counted from 0 (30 a second): the flashes the sound director's track is cut to
(/mnt/project-files/sound/mix/clips/salvo-105-test.json). Frames go to <out>/salvo-day/0001.png ... (or salvo-dusk) and
the clip to <out>/salvo-day.mp4. `stills` are frame numbers counted from 0; they go to <out>/salvo-day-stills/ (with
`view`, -high-close or -high-middle) under the number plus one, as the film's own frames are named.

The stage is the soldier thread's (battle_set.py), the tank the vehicles thread's (PanzerIV.glb), the men the soldier
thread's (soldier_GER.glb); the burst is ours. Every number of the burst is in `burst()` and `Debris`.
"""
import math
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Quaternion, Vector  # noqa: E402

import kit  # noqa: E402
import stage  # noqa: E402
import battle_set  # noqa: E402
import concrete  # noqa: E402
from scene import _field_paint, ease  # noqa: E402

Z = Vector((0, 0, 1))
FPS = 30
SHADOWS = "noshadow" not in sys.argv                         # (the crown and the cloud shade the ground and each other)
LAND = "/mnt/project-files/battle-models/land/"

# ---- where everything is (the stage's metres: x east, y north) ---------------------------------------------
BEAR = math.radians(105.0)
U = Vector((math.cos(BEAR), math.sin(BEAR), 0.0))            # the way the camera looks, over the ground
R = Vector((U.y, -U.x, 0.0))                                 # its right
FOCUS = Vector((5.0, 0.0, 0.0))                              # the squad and the tank: the frame's middle
UP, DOWN, LENS = 12.0, 15.0, 28.0                            # the battle camera (the art director's reference view)
EYE = FOCUS - U * (UP / math.tan(math.radians(DOWN)))
CLOSE = EYE + U * 22.0 + R * 8.0                             # 22 m in front of the camera, a little to the right
MIDDLE = Vector((-8.0, 80.0, 0.0))
FAR = EYE + Vector((math.cos(math.radians(96.0)), math.sin(math.radians(96.0)), 0.0)) * 290.0   # (clear of the middle one's smoke)
TANK = (1.0, 2.0, 83.0)                                      # on the road, heading up it (x, y, bearing)
F_FAR, F_MIDDLE, F_CLOSE = 45, 102, 150                      # the frames the shells land on (the sound track's)
WIND = U * 1.3 - R * 0.5                                     # a light air, away from the camera and a little left

SOIL = Vector((0.06, 0.045, 0.03))                           # thrown soil: the wet earth from underneath
DUST = Vector((0.6, 0.52, 0.37))                             # the ground's own colour, lightened
CHAR = Vector((0.07, 0.058, 0.046))                          # the cloud: charcoal brown
ASH = Vector((0.21, 0.185, 0.15))                            # ... thinning to brown-grey
FLAME, FLAME_OLD = (0.16, 0.042, 0.0), (0.1, 0.018, 0.0)     # (kept dim: the tone curve bleaches a bright flame to peach)
HAZE = Vector(kit.hex_rgb(battle_set.HAZE)) * 0.62
TANK_PAINT = "4E5459"                                        # Germany's dark grey (nations.json, until 1943-02)
MEN_PAINT = {"tunic": "5F665C", "trousers": "585E57", "boots": "2A2420", "lower": "2C2824", "straps": "2E2620", "helmet": "4F5650",
             "pack": "6E6852", "skin": "BDA08E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420", "leather": "3A2E25",
             "blanket": "564A3E"}                             # (the soldier thread's German: soldier.py's PAINTS)


# ---- the stage ---------------------------------------------------------------------------------------------

def tufts(st, x, y, r, mat, seed):
    """The grass that stands where a shell will land (the stage's own tufts, shown until it does)."""
    rnd = random.Random(seed * 13 + int(x * 5 + y * 3))
    bm = bmesh.new()
    for _ in range(max(3, int(1.3 * math.pi * r * r))):
        a, d = rnd.uniform(0, 2 * math.pi), r * math.sqrt(rnd.random())
        px, py = x + math.cos(a) * d, y + math.sin(a) * d
        z = st.height(px, py)
        for _b in range(5):
            ang = rnd.uniform(0, 2 * math.pi)
            h = rnd.uniform(0.18, 0.45)
            lean = Vector((math.cos(ang), math.sin(ang), 0)) * rnd.uniform(0.04, 0.15)
            side = Vector((-math.sin(ang), math.cos(ang), 0)) * 0.012
            base = Vector((px + rnd.uniform(-0.05, 0.05), py + rnd.uniform(-0.05, 0.05), z - 0.02))
            bm.faces.new((bm.verts.new(base - side), bm.verts.new(base + side), bm.verts.new(base + lean + Vector((0, 0, h)))))
    me = bpy.data.meshes.new("grass.site")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("grass.site", me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(mat)
    return ob


def lay_stage(seed, sites):
    """battle_set.build with our own shell holes: the old ones that stand clear of the squad, and a fresh one at each
    place a shell will land, hidden until it does. Returns the stage and, for each site, (its hole's objects, the grass
    that stood there)."""
    b = battle_set
    mats = {
        "bark": b.haze(kit.material("bark", kit.hex_rgb("4A3F33"), rough=0.95, noise=0.5, noise_scale=8, bump=0.5)),
        "leaf": b.haze(kit.material("leaf", kit.hex_rgb("4F6034"), rough=0.85, noise=0.55, noise_scale=3.0, bump=0.8)),
        "crown": b.haze(kit.material("crown", kit.hex_rgb("33421F"), rough=0.9, noise=0.5, noise_scale=3.0, bump=0.8)),
        "stone": b.haze(kit.material("stone", kit.hex_rgb("8E877A"), rough=0.92, noise=0.5, noise_scale=6, bump=0.6)),
        "plaster": b.haze(kit.material("plaster", kit.hex_rgb("CFC6B0"), rough=0.9, noise=0.35, noise_scale=2.0, bump=0.3)),
        "tile": b.haze(kit.material("tile", kit.hex_rgb("8A5A42"), rough=0.85, noise=0.4, noise_scale=4, bump=0.5)),
        "dark": b.haze(kit.material("dark", kit.hex_rgb("2A2622"), rough=0.9)),
        "earth": b.haze(kit.material("earth", kit.hex_rgb("5E5140"), rough=0.98, noise=0.4, noise_scale=3, bump=0.7)),
        "grass": b.haze(kit.material("grasstuft", kit.hex_rgb("7C8448"), rough=0.8, noise=0.5, noise_scale=1.5)),
    }
    st = b.Stage(b._terrain())
    b._road(st)
    rnd = random.Random(seed)
    for k in range(150):                                   # (the wood's edge, as battle_set.build lays it)
        x = rnd.uniform(-160, -24)
        y = rnd.uniform(18, 170)
        if x > -32 - 0.15 * (y - 18) + rnd.uniform(-8, 8):
            continue
        b._tree(f"tree{k}", Vector((x, y, st.height(x, y))), rnd.uniform(9, 15), seed * 100 + k, mats)
    for k, (x, y) in enumerate(((22, 40), (30, 78), (-20, -20), (48, 120), (-30, 70))):
        b._tree(f"lone{k}", Vector((x, y, st.height(x, y))), rnd.uniform(10, 14), seed * 7 + k, mats, near=True)
    b._stone_wall(st, (8.0, 28.0), (60.0, 41.0), mats, seed)
    b._stone_wall(st, (-6.0, 34.0), (-36.0, 30.0), mats, seed + 1)
    b._farm(st, (34.0, 66.0), math.radians(14), mats)
    old = ((-12, 14, 2.2), (-28, -10, 1.9), (40, 22, 2.4))
    for k, (x, y, r) in enumerate(old):
        b._crater(st, x, y, r, mats, seed, water=k < 2)
    fresh = []
    for x, y, r in sites:
        before = set(bpy.data.objects)
        b._crater(st, x, y, r, mats, seed + 5)
        hole = [o for o in bpy.data.objects if o not in before]
        for o in hole:
            o.hide_render = True
        fresh.append((hole, tufts(st, x, y, 1.3 * r, mats["grass"], seed)))
    b._grass(st, ((-45, -40), (55, 30)), 9000, mats, seed, old + tuple(sites))
    b._clothe_crowns()
    return st, fresh


def daylight(dusk):
    """September daylight, or the last of it: a low red sun off to the left behind the smoke, out of the camera's eye."""
    if not dusk:
        return kit.studio()
    sun = kit.studio(sun_elev=7.0, sun_turn=-122.0, strength=2.1, sky=0.3)
    sun.data.color = (1.0, 0.55, 0.28)
    sun.data.angle = math.radians(2.0)
    nt = bpy.context.scene.world.node_tree
    ramp = next(n for n in nt.nodes if n.type == "VALTORGB")
    ramp.color_ramp.elements[0].color = (0.46, 0.4, 0.42, 1)
    ramp.color_ramp.elements[1].color = (0.86, 0.56, 0.36, 1)
    return sun


def dusk_haze():
    """The haze the stage's materials fade into, at dusk: dimmer and warm."""
    for m in bpy.data.materials:
        if not m.use_nodes:
            continue
        for n in m.node_tree.nodes:
            if n.type == "EMISSION" and abs(n.inputs["Strength"].default_value - 0.62) < 1e-4:
                n.inputs["Color"].default_value = (0.62, 0.42, 0.3, 1)
                n.inputs["Strength"].default_value = 0.36


# ---- the tank and the men: other threads' .glb files, their clips read off the curves -------------------------

class Clip:
    """One clip of an imported model, read straight off its curves, so each man can be at his own moment of it."""

    def __init__(self, action):
        self.f0, self.f1 = action.frame_range
        self.bones = {}
        for fc in kit.fcurves_of(action):
            m = re.match(r'pose\.bones\["(.+?)"\]\.(location|rotation_quaternion|scale)$', fc.data_path)
            if m:
                self.bones.setdefault(m.group(1), {}).setdefault(m.group(2), {})[fc.array_index] = fc

    @property
    def long(self):
        return (self.f1 - self.f0) / FPS

    def pose(self, t, loop=False):
        span = self.f1 - self.f0
        f = t * FPS
        f = self.f0 + (f % span if loop else min(max(f, 0.0), span))
        out = {}
        for bone, ch in self.bones.items():
            loc, rot, scl = ch.get("location"), ch.get("rotation_quaternion"), ch.get("scale")
            out[bone] = (Vector([loc[i].evaluate(f) for i in range(3)]) if loc and len(loc) == 3 else None,
                         Quaternion([rot[i].evaluate(f) for i in range(4)]) if rot and len(rot) == 4 else None,
                         Vector([scl[i].evaluate(f) for i in range(3)]) if scl and len(scl) == 3 else None)
        return out


def blend(a, b, w):
    if w <= 0.0:
        return a
    if w >= 1.0:
        return b
    out = {}
    for bone in set(a) | set(b):
        pa, pb = a.get(bone), b.get(bone)
        if pa is None or pb is None:
            out[bone] = pb or pa
            continue
        out[bone] = tuple((y if x is None else x if y is None else (x.slerp(y, w) if isinstance(x, Quaternion) else x.lerp(y, w)))
                          for x, y in zip(pa, pb))
    return out


def apply(arm, pose):
    for bone, (loc, rot, scl) in pose.items():
        pb = arm.pose.bones.get(bone)
        if pb is None:
            continue
        if loc is not None:
            pb.location = loc
        if rot is not None:
            pb.rotation_quaternion = rot
        if scl is not None:
            pb.scale = scl


def bring(path):
    """Imports a .glb: its rig, its meshes, and its clips by name."""
    before, acts = set(bpy.data.objects), set(bpy.data.actions)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in list(new):
        if o.type == "MESH" and o.parent is None:            # (the importer's shape for the bones)
            new.remove(o)
            bpy.data.objects.remove(o, do_unlink=True)
    arm = next(o for o in new if o.type == "ARMATURE")
    clips = {re.sub(r"\.\d{3}$", "", a.name): Clip(a) for a in bpy.data.actions if a not in acts}
    arm.animation_data_clear()
    arm.rotation_mode = "QUATERNION"
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    for o in new:
        if o.type == "EMPTY":
            o.hide_render = True
    return arm, [o for o in new if o.type == "MESH"], clips


def copy_of(arm, meshes):
    a2 = arm.copy()
    bpy.context.scene.collection.objects.link(a2)
    for m in meshes:
        m2 = m.copy()
        bpy.context.scene.collection.objects.link(m2)
        if m.parent is arm:
            m2.parent = a2
        for mod in m2.modifiers:
            if mod.type == "ARMATURE":
                mod.object = a2
    return a2


def stand(arm, st, x, y, bearing, turn0):
    arm.location = (x, y, st.height(x, y))
    arm.rotation_quaternion = Quaternion(Z, math.radians(bearing)) @ turn0


class Man:
    """One rifleman: he walks up to `end`, facing `bearing`, and then does what `script` says:
    [(when, clip, rate, loop, blend seconds)], in order; the first is his walk."""
    PACE = 1.25                                               # (the walk clip's own speed, m/s)

    def __init__(self, arm, clips, st, end, bearing, script, seed):
        g = random.Random(seed)
        self.arm, self.clips, self.st, self.end, self.bearing, self.script = arm, clips, st, Vector((*end, 0.0)), bearing, script
        self.turn0 = arm.rotation_quaternion.copy()
        self.phase = g.uniform(0.0, 1.2)
        self.way = Vector((math.cos(math.radians(bearing)), math.sin(math.radians(bearing)), 0.0))

    def _pose(self, i, t):
        when, clip, rate, loop, _ = self.script[i]
        return self.clips[clip].pose(((t + self.phase) if i == 0 else (t - when)) * rate, loop)

    def set(self, t):
        i = max(k for k, s in enumerate(self.script) if s[0] <= t or k == 0)
        pose = self._pose(i, t)
        when, _, _, _, mix = self.script[i]
        if i > 0 and t - when < mix:
            pose = blend(self._pose(i - 1, t), pose, ease(t, when, when + mix))
        apply(self.arm, pose)
        # he walks at his clip's pace up to his first halt, and slows to a stand over that halt's blend
        v = self.PACE * self.script[0][2]
        left = 0.0
        if len(self.script) > 1:
            t1, b = self.script[1][0], max(self.script[1][4], 1e-3)
            if t < t1:
                left = v * (t1 + 0.5 * b - t)
            elif t < t1 + b:
                left = v * 0.5 * b * (1.0 - (t - t1) / b) ** 2
        p = self.end - self.way * left
        stand(self.arm, self.st, p.x, p.y, self.bearing, self.turn0)


# ---- the burst -----------------------------------------------------------------------------------------------

def dust_stuff(name, through, rim=False):
    """Dust: a puff that lets the sun through it, so it glows when the sun is behind it. Thick where you look into
    it and thinning to nothing at its edge; with `rim`, a puff laid flat thins to its rim as well, so a low skirt
    seen from above is a soft patch and never a disc."""
    m = bpy.data.materials.new("fx_" + name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    out = nt.nodes["Material Output"]
    info = nt.nodes.new("ShaderNodeObjectInfo")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = 1.6
    nz.inputs["Detail"].default_value = 4.0
    nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
    lw = nt.nodes.new("ShaderNodeLayerWeight")
    lw.inputs["Blend"].default_value = 0.5
    M = stage._math
    face = M(nt, "SUBTRACT", 1.0, lw.outputs["Facing"], clamp=True)
    soft = M(nt, "POWER", face, 1.9)
    soft = M(nt, "MULTIPLY", soft, M(nt, "SUBTRACT", M(nt, "MULTIPLY", nz.outputs[0], 1.9), 0.35, clamp=True), clamp=True)
    if rim:
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        r2 = M(nt, "ADD", M(nt, "MULTIPLY", sep.outputs[0], sep.outputs[0]), M(nt, "MULTIPLY", sep.outputs[1], sep.outputs[1]))
        edge = M(nt, "SUBTRACT", 1.0, M(nt, "ADD", r2, M(nt, "MULTIPLY", M(nt, "SUBTRACT", nz.outputs[0], 0.5), 0.5)), clamp=True)
        soft = M(nt, "MULTIPLY", soft, M(nt, "MULTIPLY", edge, 2.0, clamp=True), clamp=True)
    alpha = M(nt, "MULTIPLY", soft, info.outputs["Alpha"], clamp=True)
    d1, d2 = nt.nodes.new("ShaderNodeBsdfDiffuse"), nt.nodes.new("ShaderNodeBsdfTranslucent")
    nt.links.new(info.outputs["Color"], d1.inputs["Color"])
    nt.links.new(info.outputs["Color"], d2.inputs["Color"])
    body = nt.nodes.new("ShaderNodeMixShader")
    body.inputs[0].default_value = through
    nt.links.new(d1.outputs[0], body.inputs[1])
    nt.links.new(d2.outputs[0], body.inputs[2])
    clear = nt.nodes.new("ShaderNodeBsdfTransparent")
    ms = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(alpha, ms.inputs[0])
    nt.links.new(clear.outputs[0], ms.inputs[1])
    nt.links.new(body.outputs[0], ms.inputs[2])
    nt.links.new(ms.outputs[0], out.inputs["Surface"])
    return m


def kinds(fx):
    """Three more kinds of puff: dust, which lets the sun through it; the low skirt of it that runs along the
    ground; and the cloud, which lets a little through at its edges."""
    for name, through, rim in (("dust", 0.45, False), ("skirt", 0.45, True), ("cloud", 0.22, False)):
        fx.meshes[name] = fx.mesh.copy()
        fx.meshes[name].materials.append(dust_stuff(name, through, rim))
    # who a burst's lamps light: the flash lights the world but not the young smoke round it (lit so close it is a
    # cream bubble); the glow in the crown's foot lights the smoke and earth alone
    world = bpy.data.collections.new("lit.world")
    world.children.link(fx.coll)
    world.collection_children[0].light_linking.link_state = "EXCLUDE"
    puffs = bpy.data.collections.new("lit.puffs")
    puffs.children.link(fx.coll)
    stand = bpy.data.collections.new("lit.stand")             # (the men and the tank: build() puts them in)
    fx.lit = {"world": world, "puffs": puffs, "stand": stand}


def lamp(fx, t0, at, power, long, col, r, to):
    fx.light(t0, at, power, long=long, col=col, r=r)
    fx.lights[-1]["ob"].light_linking.receiver_collection = fx.lit[to]


def along(p, d):
    """Lays a puff's long axis along d."""
    p["ob"].rotation_euler = Vector(d).to_track_quat("X", "Z").to_euler()
    return p


def flat(p, q):
    """Lays a squashed puff level (the stage turns every puff at random, which stands a flat one on edge)."""
    p["ob"].rotation_euler = (0.0, 0.0, q.uniform(0, 6.28))
    return p


def seen_only(p):
    p["ob"].visible_diffuse = False
    p["ob"].visible_glossy = False
    return p


def burst(fx, t0, at, seed=1, size=1.0, detail=1.0, far=0.0, glare=1.0):
    """A high-explosive shell on open field, to the sheet's frame-by-frame table. `size` 1 is a 105 mm (an earth
    crown 8 to 12 m high); `detail` thins the puffs for a burst far off; `far` (0 to 1) greys it into the haze.
    Give t0 a hair before a frame."""
    q = random.Random(seed)
    at = Vector(at)
    n_of = lambda n: max(3, int(round(n * detail)))            # noqa: E731

    def tone(c, k=1.0):
        c = Vector(c) * k
        return tuple(c.lerp(HAZE, far))

    def see(a):
        return a * (1.0 - 0.35 * far)

    # frame 0 to 1: the flash. A star of short ragged tongues, low and wide, their hot hearts together at the ground:
    # a white-yellow core under 1.5 m across, gone by the second frame after.
    for i in range(8):
        a = q.uniform(0, 2 * math.pi)
        w = q.uniform(0.5, 1.5)
        d = Vector((math.cos(a) * w, math.sin(a) * w, q.uniform(0.35, 1.0))).normalized()
        rr = (q.uniform(0.38, 0.6) if i < 3 else q.uniform(0.5, 0.95)) * size
        p = fx.puff(t0, at + Z * 0.08 + d * rr * 0.8 + Vector((q.uniform(-0.15, 0.15), q.uniform(-0.15, 0.15), 0)) * size, vel=d * 5.0,
                    life=q.uniform(0.045, 0.06), r=(rr * 0.85, rr * 1.2), col=FLAME, col2=FLAME_OLD, alpha=see(1.0),
                    kind="tongue" if i < 3 else "lick", drag=6.0, wind=0.0, squash=(1.0, 0.3, 0.3), fade_in=0.004, grow=0.4)
        seen_only(along(p, d))
    heart = (0.357, 0.286, 0.007)                             # (yellow-white on the film: about 238, 224, 169)
    flat(seen_only(fx.puff(t0, at + Z * 0.2 * size, life=0.05, r=(0.26 * size, 0.34 * size), col=heart, alpha=see(1.0), kind="glow", wind=0.0,
                           fade_in=0.004, grow=0.5, squash=(1.0, 1.0, 0.6))), q)
    for i in range(6):                                        # (short spikes of it, low, so it is a star and no ball)
        a = 2 * math.pi * (i + q.uniform(-0.35, 0.35)) / 6
        d = Vector((math.cos(a), math.sin(a), q.uniform(0.1, 0.6))).normalized()
        rr = q.uniform(0.3, 0.44) * size
        p = fx.puff(t0, at + Z * 0.2 * size + d * rr * 0.6, vel=d * 3.0, life=0.05, r=(rr * 0.85, rr), col=heart, alpha=see(1.0), kind="glow", wind=0.0,
                    drag=6.0, fade_in=0.004, grow=0.5, squash=(1.0, 0.3, 0.3))
        seen_only(along(p, d))
    # a real light for the world about it: one frame full, gone by the third (never a white-out). It hangs high, so
    # the ground is lifted evenly out to 30 m and is not burnt white at the hole; a second, low, lights the sides of
    # the men and the tank that face the burst.
    lamp(fx, t0, at + Z * 16.0 * size, 8500.0 * glare * size * size, 0.1, (1.0, 0.78, 0.5), 2.0, "world")
    if far < 0.05:
        lamp(fx, t0, at + Z * 1.5 * size, 2200.0 * glare * size * size, 0.1, (1.0, 0.78, 0.5), 1.0, "stand")
    # frame 1 to 6: an orange glow inside the crown's foot, half hidden by the earth going up round it
    for i in range(n_of(7)):
        a = q.uniform(0, 2 * math.pi)
        d = Vector((math.cos(a) * 0.45, math.sin(a) * 0.45, 1.0)).normalized()
        rr = q.uniform(0.45, 0.85) * size
        p = fx.puff(t0 + q.uniform(0.025, 0.06), at + Z * 0.2 + Vector((math.cos(a), math.sin(a), 0)) * q.uniform(0.0, 0.4) * size + d * rr * 0.7, vel=d * 6.0,
                    life=q.uniform(0.1, 0.17), r=(rr * 0.8, rr * 1.25), col=FLAME, col2=FLAME_OLD, alpha=see(0.95), kind="lick", drag=5.0,
                    wind=0.0, squash=(1.0, 0.34, 0.34), fade_in=0.01, grow=0.4)
        seen_only(along(p, d))
    lamp(fx, t0 + 0.02, at + Z * 0.9 * size, 260.0 * glare * size * size, 0.17, (1.0, 0.42, 0.12), 0.6, "puffs")

    # frame 1 to 4: the shock. A low ring of dust skids out along the ground, knee high, to 15 m by the fourth frame,
    # and lies there thinning; a slower, thicker skirt inside it.
    n = n_of(150)                                             # (each lies along the ring, so they run together into a ragged front)
    for i in range(n):
        a = 2 * math.pi * (i + q.uniform(-0.5, 0.5)) / n
        d = Vector((math.cos(a), math.sin(a), 0.0))
        k = q.uniform(9.0, 11.5)
        c = q.uniform(0.85, 1.1)
        p = fx.puff(t0 + 0.02 + q.uniform(0.0, 0.02), at + d * q.uniform(0.8, 1.6) * size + Z * 0.3, vel=d * q.uniform(10.0, 21.0) * k * size ** 0.5,
                    life=q.uniform(1.8, 3.4), r=(1.0 * size, q.uniform(2.2, 3.2) * size), col=tone(DUST, c), alpha=see(q.uniform(0.4, 0.6)), kind="skirt",
                    drag=k, rise=0.16, fade_in=0.02, grow=0.4, hold=0.15, wind=0.5, squash=(1.0, q.uniform(0.55, 0.85), 0.3))
        p["ob"].rotation_euler = (0.0, 0.0, a + math.pi / 2 + q.uniform(-0.5, 0.5))
    n = n_of(60)
    for i in range(n):
        a = 2 * math.pi * (i + q.uniform(-0.5, 0.5)) / n
        d = Vector((math.cos(a), math.sin(a), 0.0))
        k = q.uniform(5.0, 7.0)
        c = q.uniform(0.75, 1.0)
        p = fx.puff(t0 + 0.03 + q.uniform(0.0, 0.04), at + d * q.uniform(0.5, 1.2) * size + Z * 0.4, vel=d * q.uniform(3.0, 10.0) * k * size ** 0.5,
                    life=q.uniform(3.5, 6.5), r=(0.8 * size, q.uniform(2.0, 3.0) * size), col=tone(DUST.lerp(SOIL, 0.3), c), alpha=see(q.uniform(0.45, 0.65)), kind="skirt",
                    drag=k, rise=0.22, fade_in=0.03, grow=0.45, hold=0.2, wind=0.7, squash=(1.0, q.uniform(0.6, 0.9), 0.5))
        p["ob"].rotation_euler = (0.0, 0.0, a + math.pi / 2 + q.uniform(-0.6, 0.6))

    # frame 2 to 12: the earth crown. Jets of dark soil, each a streak of small puffs drawn out along its own line,
    # the fastest at its tip, so the crown is spiked and no two jets are alike; a dark heart of the same stuff inside.
    for j in range(n_of(28)):
        az = q.uniform(0, 2 * math.pi)
        lean = math.radians(q.triangular(3.0, 60.0, 20.0))
        d = Vector((math.cos(az) * math.sin(lean), math.sin(az) * math.sin(lean), math.cos(lean)))
        reach = size * q.uniform(4.5, 12.0) * (1.0 - 0.3 * lean)
        k = q.uniform(6.5, 9.0)
        born = q.uniform(0.0, 0.06)
        thick = q.uniform(0.6, 1.6)
        dark = q.uniform(0.75, 1.2)
        m = int(6 + reach * 0.9)
        for i in range(m):
            f = (i + q.uniform(0.15, 0.85)) / m
            v = d * reach * k * f ** 0.85 + Vector((q.uniform(-1, 1), q.uniform(-1, 1), q.uniform(-1, 1))) * 1.2
            p = fx.puff(t0 + born + 0.012 * q.random(), at + Z * 0.15 + d * 0.3 * size, vel=v, life=q.uniform(0.6, 1.0) + 0.55 * f,
                        r=((0.09 + 0.13 * (1 - f)) * size * thick, (0.26 + 0.5 * (1 - f)) * size * thick), col=tone(SOIL, dark), col2=tone(SOIL, dark * 2.3),
                        alpha=see(q.uniform(0.82, 0.96)), drag=k, gravity=q.uniform(2.5, 4.5), fade_in=0.012, grow=0.7, hold=0.3, cool=1.0, wind=0.25,
                        squash=(q.uniform(1.6, 2.4), 0.6, 0.6))
            along(p, d)
    for i in range(n_of(70)):
        az = q.uniform(0, 2 * math.pi)
        lean = math.radians(q.triangular(0.0, 42.0, 12.0))
        d = Vector((math.cos(az) * math.sin(lean), math.sin(az) * math.sin(lean), math.cos(lean)))
        reach = size * q.uniform(1.6, 8.0)
        k = q.uniform(6.5, 8.5)
        dark = q.uniform(0.9, 1.4)
        fx.puff(t0 + q.uniform(0.0, 0.07), at + Z * 0.2 + Vector((q.uniform(-0.3, 0.3), q.uniform(-0.3, 0.3), 0)) * size, vel=d * reach * k,
                life=q.uniform(1.1, 2.0), r=(q.uniform(0.2, 0.3) * size, q.uniform(0.7, 1.2) * size), col=tone(SOIL, dark), col2=tone(SOIL.lerp(CHAR, 0.5), dark * 1.8),
                alpha=see(q.uniform(0.78, 0.92)), drag=k, gravity=q.uniform(1.5, 3.0), fade_in=0.015, grow=0.6, hold=0.35, cool=1.0, wind=0.3)

    # frame 6 on: grit rains out of the crown as fine streaks
    for i in range(n_of(64) if detail > 0.5 else 0):
        az, rad = q.uniform(0, 2 * math.pi), q.uniform(0.3, 5.5) * size
        h = q.uniform(2.5, 9.5) * size
        fall = math.sqrt(2 * h / 9.8)
        p = fx.puff(t0 + q.uniform(0.25, 1.3), at + Vector((math.cos(az) * rad, math.sin(az) * rad, h)), vel=Vector((math.cos(az), math.sin(az), 0)) * q.uniform(0.0, 1.5),
                    life=fall, r=(q.uniform(0.035, 0.06) * size, q.uniform(0.035, 0.06) * size), col=tone(SOIL, 0.8), alpha=see(q.uniform(0.55, 0.8)),
                    gravity=9.8, fade_in=0.08, hold=0.9, wind=0.15, squash=(q.uniform(4.0, 7.0), 0.5, 0.5))
        along(p, -Z)

    # frame 10 to 90 and long after: the crown falls in on itself as a rolling charcoal-brown cloud that climbs and
    # leans downwind: many small puffs born all through the crown as its soil thins, warm only at its foot at first
    for i in range(n_of(120)):
        f = q.random()                                         # (how high in the crown it is born)
        az, rad = q.uniform(0, 2 * math.pi), q.uniform(0.0, 1.0 + 1.6 * f) * size
        start = at + Vector((math.cos(az) * rad, math.sin(az) * rad, (0.4 + 7.5 * f ** 1.2) * size))
        v = Vector((math.cos(az) * q.uniform(0.3, 2.0), math.sin(az) * q.uniform(0.3, 2.0), q.uniform(0.2, 1.8)))
        pale = q.uniform(0.0, 0.5) + 0.5 * f
        c0 = CHAR * q.uniform(0.8, 1.25)
        if f < 0.16 and i % 2 == 0:
            c0 = Vector((0.13, 0.062, 0.03))                     # (warm at the foot for its first frames)
        fx.puff(t0 + q.uniform(0.12, 0.8), start, vel=v, life=q.uniform(7.0, 13.0) + 6.0 * pale, r=(q.uniform(0.4, 0.75) * size, (q.uniform(1.25, 2.1) + 0.7 * pale) * size),
                col=tone(c0), col2=tone(CHAR.lerp(ASH, pale), q.uniform(0.9, 1.1)), alpha=see((0.62 - 0.22 * pale) * q.uniform(0.85, 1.1)), kind="cloud",
                drag=1.6, rise=0.1 + 0.5 * f + q.uniform(0.0, 0.2), fade_in=q.uniform(0.2, 0.45), grow=0.55, hold=0.25, cool=4.0 if f < 0.16 else 2.4,
                wind=0.6 + 0.5 * f + q.uniform(-0.1, 0.2))
    # ... and the earth's own dust mixed into its lower half, brown-grey, which lies about the hole and drifts off low
    for i in range(n_of(44)):
        f = q.random() ** 1.5
        az, rad = q.uniform(0, 2 * math.pi), q.uniform(0.3, 3.2) * size
        c = q.uniform(0.8, 1.1)
        fx.puff(t0 + q.uniform(0.15, 1.0), at + Vector((math.cos(az) * rad, math.sin(az) * rad, (0.5 + 5.0 * f) * size)),
                vel=(math.cos(az) * q.uniform(0.3, 1.6), math.sin(az) * q.uniform(0.3, 1.6), q.uniform(-0.2, 0.6)), life=q.uniform(6.0, 12.0),
                r=(q.uniform(0.5, 0.9) * size, q.uniform(1.6, 2.8) * size), col=tone(DUST.lerp(SOIL, 0.55), c), col2=tone(DUST.lerp(SOIL, 0.3), c),
                alpha=see(q.uniform(0.34, 0.5)), kind="dust", drag=1.4, rise=0.05 + 0.2 * f, fade_in=q.uniform(0.25, 0.5), grow=0.5, hold=0.25, wind=0.55 + 0.4 * f)

    # 1 s on: a veil of the ground's own dust drifts down and away
    for i in range(n_of(26)):
        az, rad = q.uniform(0, 2 * math.pi), q.uniform(1.5, 9.0) * size
        fx.puff(t0 + q.uniform(0.25, 1.4), at + Vector((math.cos(az) * rad, math.sin(az) * rad, q.uniform(0.6, 5.0) * size)), vel=(q.uniform(-0.4, 0.4), q.uniform(-0.4, 0.4), 0.0),
                life=q.uniform(6.0, 12.0), r=(q.uniform(1.0, 1.6) * size, q.uniform(3.2, 5.2) * size), col=tone(DUST, q.uniform(0.85, 1.05)), alpha=see(q.uniform(0.16, 0.27)),
                kind="dust", rise=-0.12, fade_in=0.6, grow=0.6, hold=0.2, wind=0.9)

    # the hole smokes faintly for eight seconds: a thin thread, thinning as it goes
    def wisp(t, p0, facing):
        left = max(0.0, 1.0 - (t - t0 - 1.2) / 6.8) ** 0.7
        c = q.uniform(0.15, 0.26)
        fx.puff(t, p0 + Vector((q.uniform(-0.3, 0.3), q.uniform(-0.3, 0.3), 0.15)) * size, vel=(q.uniform(-0.2, 0.2), q.uniform(-0.2, 0.2), q.uniform(0.5, 1.0)),
                life=q.uniform(2.6, 4.2) * (0.5 + 0.5 * left), r=(0.16 * size, q.uniform(0.5, 0.9) * size), col=tone((c, c * 0.95, c * 0.9)),
                alpha=see(q.uniform(0.22, 0.34) * left), kind="cloud", drag=0.6, rise=0.5, fade_in=0.3, grow=0.6, wind=0.8)
    if detail > 0.3:
        fx.stream((at, Z), 0.1, wisp, start=t0 + 1.2, end=t0 + 7.9)


class Debris:
    """The clods a burst throws clear: solid lumps of no shape, many small and a few large, each on a true arc (they
    fall at 9.8 m/s2: one thrown 10 m up comes down 2.8 s later), drawn out along its flight while it flies. The big
    ones bounce once. Each lands with a puff of dust and lies where it stops for the rest of the battle. `lens`
    (the camera's place and its right and forward ways) sends a few across the lower foreground of the picture."""

    def __init__(self, fx, st, t0, at, count=90, seed=1, size=1.0, lens=None):
        g = random.Random(seed)
        self.t0, self.at, self.items = t0, Vector(at), []
        mat = battle_set.haze(kit.material("clod", (0.1, 0.075, 0.05), rough=1.0, spec=0.05, noise=0.4, noise_scale=30.0))
        throws = []
        for i in range(count):
            r = (0.02 + 0.11 * g.random() ** 3.0) * size
            a = g.uniform(0, 2 * math.pi)
            up = math.sqrt(2 * 9.8 * g.uniform(1.0, 11.5) * (g.random() ** 0.6) * size)       # (to where its arc tops out)
            out = g.uniform(0.6, 6.5) * size ** 0.5
            throws.append((r, Vector((math.cos(a) * out, math.sin(a) * out, up))))
        if lens is not None:
            eye, right, fwd = lens
            for i in range(9):
                r = g.uniform(0.035, 0.08) * size
                T = g.uniform(0.75, 1.5)
                P = Vector(eye) + right * g.uniform(-3.0, 3.0) + fwd * g.uniform(2.0, 5.5) - Z * g.uniform(1.0, 3.2)
                throws.append((r, (P - self.at) / T + Z * 4.9 * T))
        for r, v in throws:
            verts, faces = concrete.lump(g, r, g.uniform(0.5, 0.9), 7)
            me = bpy.data.meshes.new("clod")
            me.from_pydata([tuple(p) for p in verts], [], faces)
            me.materials.append(mat)
            ob = bpy.data.objects.new("clod", me)
            bpy.context.scene.collection.objects.link(ob)
            ob.rotation_mode = "QUATERNION"
            ob.scale = (0.0, 0.0, 0.0)
            # where and when it comes down on the stage's own ground
            t, hit = 0.05, None
            while t < 8.0:
                p = self.at + Vector((v.x * t, v.y * t, 0.15 + v.z * t - 4.9 * t * t))
                if v.z - 9.8 * t < 0 and p.z <= st.height(p.x, p.y) + r * 0.3:
                    hit = (t, Vector((p.x, p.y, st.height(p.x, p.y) + r * 0.3)))
                    break
                t += 0.02
            if hit is None:
                hit = (8.0, self.at + Vector((v.x * 8.0, v.y * 8.0, -50.0)))
            hop = None
            if r > 0.05 * size and hit[0] < 8.0:               # (a big one bounces once)
                v2 = Vector((v.x * 0.35, v.y * 0.35, abs(v.z - 9.8 * hit[0]) * 0.22))
                t2 = 2 * v2.z / 9.8
                e = hit[1] + Vector((v2.x * t2, v2.y * t2, 0.0))
                hop = (v2, t2, Vector((e.x, e.y, st.height(e.x, e.y) + r * 0.3)))
            lie = Quaternion((g.gauss(0, 1), g.gauss(0, 1), g.gauss(0, 1), g.gauss(0, 1))).normalized()
            self.items.append((ob, v, r, hit, hop, lie))
            if r > 0.028 * size and hit[0] < 8.0:               # (it lands with a small puff)
                c = g.uniform(0.85, 1.05)
                for _ in range(2):
                    fx.puff(t0 + hit[0], hit[1] + Z * 0.1, vel=(g.uniform(-0.6, 0.6), g.uniform(-0.6, 0.6), g.uniform(0.3, 0.9)), life=g.uniform(0.7, 1.3),
                            r=(0.06 + r, g.uniform(0.3, 0.55) + 3 * r), col=tuple(DUST * c), alpha=g.uniform(0.22, 0.36), kind="dust", drag=2.0, fade_in=0.04, grow=0.5, wind=0.5)

    def set(self, t):
        a = t - self.t0
        for ob, v, r, (down, where), hop, lie in self.items:
            if a < 0.0:
                ob.scale = (0.0, 0.0, 0.0)
                continue
            if a < down:
                ob.location = self.at + Vector((v.x * a, v.y * a, 0.15 + v.z * a - 4.9 * a * a))
                now = Vector((v.x, v.y, v.z - 9.8 * a))
            elif hop is not None and a < down + hop[1]:
                b = a - down
                ob.location = where + Vector((hop[0].x * b, hop[0].y * b, hop[0].z * b - 4.9 * b * b))
                now = Vector((hop[0].x, hop[0].y, hop[0].z - 9.8 * b))
            else:
                ob.location = hop[2] if hop is not None else where
                ob.rotation_quaternion = lie
                ob.scale = (1.0, 1.0, 1.0)
                continue
            ob.rotation_quaternion = now.to_track_quat("X", "Z")
            ob.scale = (1.0 + min(now.length, 14.0) * 0.14, 1.0, 1.0)


# ---- the scene -----------------------------------------------------------------------------------------------

def squad(st, t_heard, t_close):
    """Ten riflemen walking up beside the tank. The far shell's bang reaches them (t_heard) and they throw themselves
    down, each in his own time, their leader shouting them on: all are down 0.7 to 1.1 s after it, before the middle
    shell lands (the sound track has their kit hitting the ground then). Three by the tank only crouch; the close
    shell knocks the nearest of them flat."""
    arm, meshes, clips = bring(LAND + "soldier/soldier_GER.glb")
    for o in meshes:                                          # (the file's paints are plain white: the game tints them by name)
        for mat in o.data.materials:
            hx = MEN_PAINT.get(mat.name.split(".")[0]) if mat else None
            if hx and not mat.get("guest"):
                b = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
                b.inputs["Base Color"].default_value = (*kit.hex_rgb(hx), 1.0)
                b.inputs["Roughness"].default_value = 0.5 if mat.name.startswith("steel") else 0.88
                mat["guest"] = True
    g = random.Random(5)
    places = [(5.5, 6.0), (7.5, 2.0), (5.0, -2.5), (9.5, -1.0), (12.0, 3.5), (8.0, -6.5), (12.5, -5.0), (4.0, -9.0), (10.5, -11.0), (-3.5, -4.0)]
    near = sorted(range(len(places)), key=lambda i: (Vector((*places[i], 0)) - CLOSE).length)
    down = 0.8 * clips["go_prone"].long                          # (how far into the clip his body is on the ground)
    men = []
    for i, end in enumerate(places):
        a = arm if i == 0 else copy_of(arm, meshes)
        script = [(-10.0, "walk", g.uniform(0.94, 1.06), True, 0.0)]
        if i in (near[0], near[-1], near[-2]):
            script.append((t_heard + g.uniform(0.05, 0.3), "cower", 1.0, True, 0.4))
            if i == near[0]:
                script.append((t_close + 0.04, "die_forward", 1.15, False, 0.1))        # (knocked flat by the blast behind him)
        else:
            rate = g.uniform(1.6, 1.9)
            script.append((t_heard + g.uniform(0.7, 1.1) - down / rate, "go_prone", rate, False, 0.25))
        men.append(Man(a, clips, st, end, 83.0 + g.uniform(-6.0, 6.0), script, 20 + i))
    return men


def tank(st, t_close):
    arm, meshes, clips = bring(LAND + "vehicles/PanzerIV.glb")
    for o in meshes:
        for mat in o.data.materials:
            kind = mat.name.split(".")[0].split(":")[-1] if mat else ""
            if kind in ("paint", "upper", "upper2", "shade") and not mat.get("guest"):
                _field_paint(mat, kit.hex_rgb(TANK_PAINT), 0.6 if kind == "shade" else 1.0)
                mat["guest"] = True
    stand(arm, st, TANK[0], TANK[1], TANK[2], arm.rotation_quaternion.copy())

    def pose(t):
        p = clips["idle"].pose(t, True) if "idle" in clips else {}
        a = t - (t_close + 0.05)
        if "hit" in clips and 0.0 <= a < clips["hit"].long:         # (she rocks on her springs)
            p = {**p, **clips["hit"].pose(a)}
        apply(arm, p)
    return pose


def build(dusk=False, view=None):
    kit.fresh(FPS)
    daylight(dusk)
    sites = [(FAR.x, FAR.y, 0.85), (MIDDLE.x, MIDDLE.y, 0.85), (CLOSE.x, CLOSE.y, 0.85)]
    st, fresh = lay_stage(3, sites)
    if dusk:
        dusk_haze()
        global HAZE
        HAZE = Vector((0.62, 0.42, 0.3)) * 0.36
    hits = [(F_FAR, FAR), (F_MIDDLE, MIDDLE), (F_CLOSE, CLOSE)]
    hits = [(f / FPS, Vector((p.x, p.y, st.height(p.x, p.y)))) for f, p in hits]
    t_far, t_mid, t_close = (h[0] for h in hits)
    ear = Vector((FOCUS.x, FOCUS.y, st.height(FOCUS.x, FOCUS.y) + UP))      # (where the sound track listens from)
    men = squad(st, t_far + (hits[0][1] - ear).length / 343.0, t_close)
    tank_pose = tank(st, t_close)

    cam = kit.camera("cam", LENS)
    cam.data.clip_end = 3000.0
    eye = Vector((EYE.x, EYE.y, st.height(EYE.x, EYE.y) + UP))
    aim = Vector((FOCUS.x, FOCUS.y, st.height(EYE.x, EYE.y)))
    if view:                                                 # (the high check view: 30 m up, 40 degrees down)
        c = hits[2][1] if view.endswith("close") else hits[1][1]
        eye, aim = c - U * (30.0 / math.tan(math.radians(40.0))) + Z * 30.0, c
    else:
        cam.data.dof.use_dof = True                          # (what crosses close to the lens is out of focus)
        cam.data.dof.focus_distance = (aim - eye).length
        cam.data.dof.aperture_fstop = 0.7

    fx = stage.Effects(seed=11, wind=tuple(WIND))
    kinds(fx)
    for o in bpy.data.objects:
        if o.type == "MESH" and o.parent is not None and o.parent.type == "ARMATURE":
            fx.lit["stand"].objects.link(o)
    eps = 0.004
    burst(fx, t_far - eps, hits[0][1], seed=31, detail=0.4, far=0.42, glare=3.0)
    burst(fx, t_mid - eps, hits[1][1], seed=32, detail=0.75, far=0.12, glare=1.6)
    mine = len(fx.live)
    burst(fx, t_close - eps, hits[2][1], seed=33)
    if SHADOWS:                                               # (the close one's crown and cloud shade the ground and themselves)
        for p in fx.live[mine:]:
            if p["kind"] in ("smoke", "cloud") and p["life"] > 0.5:
                p["ob"].visible_shadow = True
    fwd = (aim - eye).normalized()
    debris = [Debris(fx, st, t_mid, hits[1][1], count=50, seed=42),
              Debris(fx, st, t_close, hits[2][1], count=130, seed=43, lens=None if view else (eye, R, fwd))]

    if not view:                                              # (dust settles over the view for a second)
        g = random.Random(77)
        for i in range(5):
            p0 = eye + fwd * g.uniform(4.0, 8.0) + R * g.uniform(-4.0, 4.0) - Z * g.uniform(0.5, 2.5) - Vector(WIND) * 1.0
            fx.puff(t_close + g.uniform(0.45, 0.9), p0, vel=(0, 0, -0.3), life=g.uniform(1.4, 2.0), r=(1.4, g.uniform(2.4, 3.4)), col=tuple(DUST * 0.9),
                    alpha=g.uniform(0.07, 0.12), kind="dust", fade_in=0.35, grow=0.6, hold=0.3, wind=1.0)

    def step(t):
        for m in men:
            m.set(t)
        tank_pose(t)
        for d in debris:
            d.set(t)
        for (t0, _), (hole, grass) in zip(hits, fresh):
            for o in hole:
                o.hide_render = t < t0 - 0.01
            grass.hide_render = t >= t0 - 0.01
        kit.look_at(cam, eye, aim)
        a = t - (t_close + 0.06)                              # (the close one jolts the camera: up and down, a breath of roll)
        if a >= 0.0 and not view:
            env = math.exp(-a * 7.5) if a < 0.7 else 0.0
            cam.location.z += 0.1 * env * math.sin(2 * math.pi * 9.0 * a)
            cam.rotation_euler.rotate_axis("X", math.radians(0.35) * env * math.sin(2 * math.pi * 9.0 * a + 1.0))
            cam.rotation_euler.rotate_axis("Z", math.radians(0.3) * env * math.sin(2 * math.pi * 7.0 * a + 0.5))
        fx.at(t)
    return step


def main():
    out = sys.argv[1]
    size = next((a for a in sys.argv[2:] if "x" in a and a[0].isdigit()), "960x540x16")
    w, h, spp = (int(v) for v in size.split("x"))
    dusk = "dusk" in sys.argv[2:]
    only = next((a[5:] for a in sys.argv if a.startswith("only=")), None)
    stills = next((a[7:] for a in sys.argv if a.startswith("stills=")), None)
    view = next((a[5:] for a in sys.argv if a.startswith("view=")), None)
    seconds = next((float(a[5:]) for a in sys.argv if a.startswith("long=")), 10.0)
    name = "salvo-dusk" if dusk else "salvo-day"
    step = build(dusk, view)
    sc = stage.render_setup(w, h, spp)
    sc.cycles.max_bounces = 5
    sc.cycles.diffuse_bounces = 3
    if stills:
        frames = sorted(int(v) for v in stills.split(","))
        folder = os.path.join(out, name + "-stills" + ("-" + view.replace(":", "-") if view else ""))
        stage.film(folder, (frames[-1] + 1) / FPS, FPS, step, only=set(frames))
        return
    sel = None
    if only:
        a, b = (int(v) for v in only.split("-"))
        sel = set(range(a - 1, b))
    stage.film(os.path.join(out, name), seconds, FPS, step, only=sel)
    if not only:
        kit.encode(os.path.join(out, name), os.path.join(out, name), fps=FPS, gif_width=640)


if __name__ == "__main__":
    main()
