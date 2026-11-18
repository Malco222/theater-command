"""The soldier's body: the open MakeHuman base man, shaped, rigged and weighted in Blender.

Why: Brian asked (2026-11-18) for the soldiers to look "a lot more realistic". A face and a body sculpted by numbers
hit a ceiling, so the body comes from MakeHuman's base mesh, which its makers released CC0 (free for any use; see
makehuman_cc0/LICENSE.md and CREDITS.md). It is not from any game. Everything he wears and carries is our own.

    import human
    man = human.build(seed=3, nation="GER")
    man["armature"], man["body"], man["eyes"], man["tights"], man["skirt"], man["joints"]

* The man faces +X, z up, metres, feet on z = 0, 1.75 m tall by default, in MakeHuman's relaxed A pose.
* The armature uses the battle soldier's bone names (below), with MakeHuman's own CC0 skin weights merged onto them,
  so every vertex of the body, the eyes and the two clothing shells is weighted.
* `tights` is MakeHuman's body-tight helper suit and `skirt` its skirt helper, both already fitted to this man's shape:
  the starting shells for the tunic and the trousers. `scalp` is a vertex group on the body for short hair.
* `seed` varies the face (head shape, nose, chin, mouth, cheeks, brows) and the build a little, so no two men in a
  squad share a face. `look` overrides any target weight by name.
"""

import json
import math
import os
import random

import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree

HERE = os.path.dirname(os.path.abspath(__file__))
MH = os.path.join(HERE, "makehuman_cc0")

# The battle soldier's skeleton: (bone, parent, the MakeHuman bones merged into it). Any MakeHuman bone not named here
# (the face, the eyes, the tongue, the toes' joints) goes to its nearest named ancestor.
SKELETON = [
    ("pelvis", "root", ["root", "pelvis.L", "pelvis.R"]),
    ("spine_01", "pelvis", ["spine05", "spine04"]),
    ("spine_02", "spine_01", ["spine03"]),
    ("chest", "spine_02", ["spine02", "spine01", "breast.L", "breast.R"]),
    ("neck", "chest", ["neck01", "neck02", "neck03"]),
    ("head", "neck", ["head"]),
]
for _s, _S in (("l", "L"), ("r", "R")):
    SKELETON += [
        (f"clavicle_{_s}", "chest", [f"clavicle.{_S}"]),
        (f"upperarm_{_s}", f"clavicle_{_s}", [f"shoulder01.{_S}", f"upperarm01.{_S}", f"upperarm02.{_S}"]),
        (f"lowerarm_{_s}", f"upperarm_{_s}", [f"lowerarm01.{_S}", f"lowerarm02.{_S}"]),
        (f"hand_{_s}", f"lowerarm_{_s}", [f"wrist.{_S}"] + [f"metacarpal{k}.{_S}" for k in range(1, 5)]),
        (f"thumb_{_s}", f"hand_{_s}", [f"finger1-{k}.{_S}" for k in range(1, 4)]),
        (f"fingers_{_s}", f"hand_{_s}", [f"finger{f}-{k}.{_S}" for f in range(2, 6) for k in range(1, 4)]),
        (f"thigh_{_s}", "pelvis", [f"upperleg01.{_S}", f"upperleg02.{_S}"]),
        (f"calf_{_s}", f"thigh_{_s}", [f"lowerleg01.{_S}", f"lowerleg02.{_S}"]),
        (f"foot_{_s}", f"calf_{_s}", [f"foot.{_S}"]),
        (f"ball_{_s}", f"foot_{_s}", [f"toe{t}-{k}.{_S}" for t in range(1, 6) for k in range(1, 4)]),
    ]

# Faces by army: the mix of MakeHuman's three regional base shapes (a plain average, not a portrait of anyone).
NATION_MIX = {"JAP": (0.1, 0.05, 0.85), "CHN": (0.1, 0.05, 0.85), "THA": (0.1, 0.05, 0.85), "IND": (0.6, 0.2, 0.2), "_default": (0.75, 0.12, 0.13)}

_OBJ = None
_TARGETS = {}


def _load_obj():
    global _OBJ
    if _OBJ is None:
        V, UV, groups, cur = [], [], {}, None
        for line in open(os.path.join(MH, "base.obj")):
            if line.startswith("v "):
                V.append(Vector(tuple(map(float, line.split()[1:4]))))
            elif line.startswith("vt "):
                UV.append(tuple(map(float, line.split()[1:3])))
            elif line.startswith("g "):
                cur = line.split()[1]
            elif line.startswith("f "):
                f = [t.split("/") for t in line.split()[1:]]
                groups.setdefault(cur, []).append(([int(a[0]) - 1 for a in f], [int(a[1]) - 1 if len(a) > 1 and a[1] else -1 for a in f]))
        _OBJ = (V, UV, groups)
    return _OBJ


def _target(name):
    if name not in _TARGETS:
        d = {}
        for line in open(os.path.join(MH, "targets", name + ".target")):
            if line[0] in "#\n":
                continue
            p = line.split()
            d[int(p[0])] = Vector(tuple(map(float, p[1:4])))
        _TARGETS[name] = d
    return _TARGETS[name]


def look_for(seed=0, nation="_default"):
    """The target weights for one man: a young soldier of average build, his face varied by `seed`."""
    rnd = random.Random(seed * 7919 + 17)
    c, a, s = NATION_MIX.get(nation, NATION_MIX["_default"])
    j = [rnd.uniform(-0.12, 0.12) for _ in range(3)]          # (no two men quite the same mix)
    mix = [max(0.0, c + j[0]), max(0.0, a + j[1]), max(0.0, s + j[2])]
    tot = sum(mix)
    w = {
        "caucasian-male-young": mix[0] / tot,
        "african-male-young": mix[1] / tot,
        "asian-male-young": mix[2] / tot,
        "male-young-averagemuscle-averageweight-idealproportions": 0.65,
        "universal-male-young-maxmuscle-averageweight": rnd.uniform(0.15, 0.45),     # fit young men
        "universal-male-young-averagemuscle-minweight": rnd.uniform(0.1, 0.45),      # and lean: army rations
    }
    w[rnd.choice(["head-oval", "head-square", "head-round", "head-triangular"])] = rnd.uniform(0.2, 0.7)
    for t, lo, hi in (("nose-scale-horiz-incr", -0.4, 0.5), ("nose-scale-vert-incr", -0.3, 0.5), ("nose-hump-incr", -0.2, 0.7),
                      ("nose-width1-incr", -0.4, 0.5), ("chin-prominent-incr", -0.3, 0.7), ("chin-width-incr", -0.3, 0.6),
                      ("mouth-scale-horiz-incr", -0.3, 0.4), ("mouth-lowerlip-volume-incr", -0.3, 0.5), ("eyebrows-trans-down", 0.0, 0.6),
                      ("eyebrows-angle-up", -0.4, 0.3), ("head-fat-incr", -0.4, 0.2), ("neck-scale-horiz-incr", 0.0, 0.6),
                      ("head-age-incr", 0.0, 0.25)):
        w[t] = rnd.uniform(lo, hi)
    cheek = rnd.uniform(-0.2, 0.7)
    w["l-cheek-bones-incr"] = w["r-cheek-bones-incr"] = cheek
    return w


def _shaped(look):
    V, _, _ = _load_obj()
    out = [v.copy() for v in V]
    for name, k in look.items():
        if abs(k) < 1e-6:
            continue
        for i, dv in _target(name).items():
            out[i] += dv * k
    return out


def _bl(v, k):
    """MakeHuman (x to his left, y up, z to his front; decimetres) -> ours (x front, y left, z up; metres)."""
    return Vector((v.z, v.x, v.y)) * (0.1 * k)


def _mesh(name, verts, faces_uv, UV, keep=None):
    used = sorted({i for f, _ in faces_uv for i in f})
    remap = {o: n for n, o in enumerate(used)}
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(verts[i]) for i in used], [], [[remap[i] for i in f] for f, _ in faces_uv])
    if UV:
        uvl = me.uv_layers.new(name="UVMap")
        k = 0
        for f, t in faces_uv:
            for ti in t:
                uvl.data[k].uv = UV[ti] if ti >= 0 else (0.0, 0.0)
                k += 1
    me.update()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob, used


def build(seed=0, nation="_default", height=1.75, look=None, name="man"):
    """Builds one man in the current scene. Returns a dict of his objects and joint places (see the module note)."""
    V, UV, groups = _load_obj()
    w = look_for(seed, nation)
    if look:
        w.update(look)
    S = _shaped(w)
    body_faces = groups["body"]
    feet = min(S[i].y for f, _ in body_faces for i in f)
    crown = max(S[i].y for f, _ in body_faces for i in f)
    k = height / ((crown - feet) * 0.1)
    lift = Vector((0, 0, -feet * 0.1 * k))
    P = [_bl(v, k) + lift for v in S]

    # ---- the skeleton's joints, where MakeHuman says they are on this man
    skel = json.load(open(os.path.join(MH, "default.mhskel")))
    joints = {n: sum((P[i] for i in idx), Vector()) / len(idx) for n, idx in skel["joints"].items()}
    mh = skel["bones"]
    owner = {}
    for b, _, members in SKELETON:
        for m in members:
            owner[m] = b

    def owner_of(m):
        while m not in owner:
            m = mh[m]["parent"]
        return owner[m]

    # ---- the armature
    arm_data = bpy.data.armatures.new(name + "_rig")
    arm = bpy.data.objects.new(name + "_rig", arm_data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm_data.edit_bones
    root = eb.new("root")
    root.head, root.tail = Vector((0, 0, 0)), Vector((0.25, 0, 0))
    heads = {b: joints[members[0] + "____head"] for b, _, members in SKELETON}
    children = {}
    for b, p, _ in SKELETON:
        children.setdefault(p, []).append(b)
    for b, p, members in SKELETON:
        bone = eb.new(b)
        bone.head = heads[b]
        chain_child = [c for c in children.get(b, []) if not c.startswith(("clavicle", "thigh", "thumb"))]
        if b == "pelvis":
            tail = heads["spine_01"]
        elif b == "hand_" + b[-1]:
            tail = heads["fingers_" + b[-1]]
        elif b.startswith("fingers_"):
            tail = joints["finger3-3." + b[-1].upper() + "____tail"]
        elif chain_child:
            tail = heads[chain_child[0]]
        else:
            tail = joints[members[-1] + "____tail"]
        if (tail - bone.head).length < 0.02:
            tail = bone.head + Vector((0, 0, 0.05))
        bone.tail = tail
    for b, p, _ in SKELETON:
        eb[b].parent = eb[p]
        eb[b].use_connect = False
    for bone in eb:
        d = (bone.tail - bone.head).normalized()
        bone.align_roll(Vector((0, 0, 1)) if abs(d.x) > 0.7 else Vector((1, 0, 0)))
    bpy.ops.object.mode_set(mode="OBJECT")

    # ---- the meshes: the body, two eyes, and the clothing shells
    body, used = _mesh(name + "_body", P, body_faces, UV)
    shells = {}
    for g, label in (("helper-tights", "tights"), ("helper-skirt", "skirt")):
        shells[label] = _mesh(name + "_" + label, P, groups[g], UV)
    eyes = []
    for side in ("l", "r"):
        g = groups[f"helper-{side}-eye"]
        idx = sorted({i for f, _ in g for i in f})
        c = sum((P[i] for i in idx), Vector()) / len(idx)
        r = max((P[i] - c).length for i in idx) * 0.98
        bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=10, radius=r, location=c)
        e = bpy.context.object
        e.name = f"{name}_eye_{side}"
        e.rotation_euler = (0, math.pi / 2, 0)                  # the sphere's pole looks forward, for the iris
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.object.shade_smooth()
        eyes.append(e)

    # ---- the weights: MakeHuman's own, merged onto our bones
    mhw = json.load(open(os.path.join(MH, "default_weights.mhw")))["weights"]
    per_vertex = {}
    for mb, pairs in mhw.items():
        b = owner_of(mb) if mb in mh else "head"
        for vi, wt in pairs:
            per_vertex.setdefault(vi, {}).setdefault(b, 0.0)
            per_vertex[vi][b] += wt
    kd = KDTree(len(used))
    for n, vi in enumerate(used):
        kd.insert(P[vi], n)
    kd.balance()

    def weigh(ob, idx_list):
        for b in [s[0] for s in SKELETON]:
            ob.vertex_groups.new(name=b)
        for n, vi in enumerate(idx_list):
            ws = per_vertex.get(vi)
            if not ws:                                        # (a shell's point MakeHuman left unweighted: take the body's nearest)
                _, near, _ = kd.find(P[vi])
                ws = per_vertex.get(used[near], {"pelvis": 1.0})
            tot = sum(ws.values()) or 1.0
            for b, wt in ws.items():
                if wt / tot > 0.002:
                    ob.vertex_groups[b].add([n], wt / tot, "REPLACE")
        mod = ob.modifiers.new("rig", "ARMATURE")
        mod.object = arm
        ob.parent = arm

    weigh(body, used)
    for label, (ob, idx) in shells.items():
        weigh(ob, idx)
    for e in eyes:
        e.vertex_groups.new(name="head").add(list(range(len(e.data.vertices))), 1.0, "REPLACE")
        m = e.modifiers.new("rig", "ARMATURE")
        m.object = arm
        e.parent = arm

    # ---- the scalp, for short hair under the helmet
    head_j = joints["head____head"]
    brow = (joints["eye.L____head"] + joints["eye.R____head"]) / 2
    sc = body.vertex_groups.new(name="scalp")
    mid_y = (joints["eye.L____head"].y + joints["eye.R____head"].y) / 2

    def ramp(v, a, b):                                         # 0 below a, 1 above b, smooth between
        t = max(0.0, min(1.0, (v - a) / (b - a)))
        return t * t * (3 - 2 * t)

    for n, vi in enumerate(used):
        p = P[vi]
        if p.z < head_j.z - 0.02:
            continue
        side = abs(p.y - mid_y)
        back = brow.x - 0.03 - p.x                             # how far behind the brow
        line = brow.z + 0.07 - 0.55 * max(0.0, back) - 0.25 * max(0.0, back - 0.06)   # the hairline, falling toward the nape
        wgt = ramp(p.z, line - 0.006, line + 0.012)
        if side > 0.068 and p.z < brow.z + 0.045 and back < 0.11:   # the ears stay skin
            wgt *= ramp(p.z, brow.z + 0.03, brow.z + 0.045)
        if wgt > 0.01:
            sc.add([n], wgt, "REPLACE")
    return {"armature": arm, "body": body, "eyes": eyes, "tights": shells["tights"][0], "skirt": shells["skirt"][0],
            "joints": {b: heads[b].copy() for b in heads}, "scale": k, "look": w}


def skin_material(name="skin", tone="C59A7C", seed=0):
    """Skin with a little subsurface scatter, ruddy cheeks and nose, a shade of stubble and short dark hair on the
    scalp group (the material reads the `scalp` vertex group through an attribute)."""
    import kit
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = kit.material(name, kit.hex_rgb(tone), rough=0.5, noise=0.18, noise_scale=40, bump=0.08)
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Subsurface Weight"].default_value = 0.2
    bsdf.inputs["Subsurface Radius"].default_value = (0.012, 0.005, 0.003)
    nt = m.node_tree
    att = nt.nodes.new("ShaderNodeAttribute")
    att.attribute_name = "scalp"
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    nt.links.new(att.outputs["Fac"], mix.inputs["Factor"])
    src = bsdf.inputs["Base Color"].links[0].from_socket if bsdf.inputs["Base Color"].links else None
    if src:
        nt.links.new(src, mix.inputs["A"])
    else:
        mix.inputs["A"].default_value = (*kit.hex_rgb(tone), 1)
    mix.inputs["B"].default_value = (*kit.hex_rgb("3A2E25"), 1)
    nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    return m


def eye_material(name="eye"):
    """White of the eye, a brown iris and a dark pupil, painted by the sphere's own facing (its +X pole looks out)."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.15
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["Normal"], sep.inputs[0])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    e = ramp.color_ramp.elements
    e[0].position, e[0].color = 0.0, (0.82, 0.8, 0.76, 1)
    e[1].position, e[1].color = 0.86, (0.82, 0.8, 0.76, 1)
    a = e.new(0.88)
    a.color = (0.18, 0.1, 0.05, 1)
    b = e.new(0.965)
    b.color = (0.02, 0.015, 0.01, 1)
    ramp.color_ramp.interpolation = "CONSTANT"
    nt.links.new(sep.outputs["X"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    return m
