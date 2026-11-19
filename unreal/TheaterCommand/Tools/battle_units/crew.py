"""The crew pack: the soldier (soldier.py) seated, standing and working at the vehicles' and the fortifications' crew
places, as loops and one-shot clips on the soldier's own rig, and a posing function that puts a man at any crew socket.

Run without Blender's window (Python 3.11 with the `bpy` module, Blender 5), from Tools/battle_units/:

    python crew.py                          everything: measure, bake, export, check, pictures, readme
    python crew.py --only measure           the vehicles' seats ray-cast under their crew sockets (printed, kept in crew.json)
    python crew.py --only export,check      crew.glb and crew.json (every clip an action on the soldier rig), the clearances
    python crew.py --only pictures --fast   the proof pictures, small (640x360, 16 samples)
    python crew.py --clips sit_bench,stand_mg --only strips      frames of some clips, for looking at

    crew.pose(man, "sit", 0.44, grip_l, grip_r, facing, at=...)   from another script: a man posed at any crew socket

Where a clip's root goes (the anchor): every clip is posed about the rig's root bone, which the game puts on a socket.
A SEATED clip's root is the SEAT POINT: the top of the seat or bench under the middle of his seat (the vehicles'
`driver`, `rider.cab` and `rider.N` sockets are exactly that: the ray-cast lands on the seat's top at `at`). Every
other clip's root is on the FLOOR AT HIS FEET (standing, kneeling; lying, under his hips). Where a socket is not that
point (the tank commander's, in the middle of the hatch ring; the M3's gunner, stood too close to his grips), crew.json
says where the root goes from the socket.

Everything is our own: the man is the soldier of soldier.py, the motion is posed here (two-bone IK to the grips and the
floor), the props (the flak round, the mortar bomb, the binoculars, the belt and its box) are made here.

Conventions as soldier.py's: metres, the man faces +X, Z up; Blender's y is the man's LEFT. The game's JSON (the
vehicles', the fortifications' and ours) has y to the RIGHT; this script flips it on reading and on writing.
"""

import json
import math
import os
import pickle
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

import kit  # noqa: E402
import soldier as S  # noqa: E402
from soldier import E, M4, R_axis, _n, add3, bump, frame2, osc, ramp, sm  # noqa: E402
from sculpt import frame_from  # noqa: E402

OUT = "/mnt/project-files/battle-models/land/soldier/crew/"
WORK = os.environ.get("CREW_WORK", "/home/user/art/soldier/crew/")
VEH = "/mnt/project-files/battle-models/land/vehicles/"
FORT = "/mnt/project-files/battle-models/fortifications/"
FPS = S.FPS


def g2b(v):
    """The game's axes (y right) to Blender's (y left), and back: the same flip."""
    return np.array((float(v[0]), -float(v[1]), float(v[2])))


def Rz(deg):
    return R_axis((0, 0, 1), deg)


def yaw_of(f):
    return math.degrees(math.atan2(f[1], f[0]))


# ================================================================================================ the places
class Place:
    """Where a man's root goes and which way he faces (Blender axes), with the turns between that frame and the
    piece's."""

    def __init__(self, at, facing):
        self.at = np.asarray(at, float)
        f = np.asarray(facing, float)
        self.yaw = yaw_of(f)
        self.R = Rz(self.yaw)

    def local(self, p):
        return self.R.T @ (np.asarray(p, float) - self.at)

    def world(self, p):
        return self.at + self.R @ np.asarray(p, float)

    def ldir(self, d):
        return self.R.T @ np.asarray(d, float)

    def matrix(self):
        return M4(self.R, self.at)


def piece_json(kind, name):
    return json.load(open((VEH if kind == "vehicle" else FORT) + name + ".json"))


def sockets_of(kind, name):
    """A piece's sockets in Blender's axes: {name: dict(at, facing, stance, seat_h, part, note)}."""
    out = {}
    for s in piece_json(kind, name)["sockets"]:
        d = dict(s)
        d["at"], d["facing"] = g2b(s["at"]), g2b(s.get("facing", (1, 0, 0)))
        out[s["name"]] = d
    return out


def load_piece(kind, name, at=(0.0, 0.0, 0.0), yaw=0.0):
    """A vehicle's or a fortification's .glb in the scene (its own materials), and its armature; the stray helper
    sphere some exports carry is dropped."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=(VEH if kind == "vehicle" else FORT) + name + ".glb")
    objs = [o for o in bpy.data.objects if o not in before]
    for o in list(objs):
        if o.name.startswith("Icosphere"):
            objs.remove(o)
            bpy.data.objects.remove(o)
    for o in objs:
        if o.animation_data:                  # (the import leaves a clip on the rig: the pieces stand at rest here)
            o.animation_data.action = None
        if o.type == "ARMATURE":
            for pb in o.pose.bones:
                pb.location, pb.rotation_quaternion, pb.scale = (0, 0, 0), (1, 0, 0, 0), (1, 1, 1)
                pb.rotation_mode = "QUATERNION"
    top = [o for o in objs if o.parent is None]
    for o in top:
        o.matrix_world = Matrix(M4(Rz(yaw), at).tolist()) @ o.matrix_world
    bpy.context.view_layer.update()
    return objs


def paint_piece(objs, kind, name):
    """The fortifications' .glb colour themselves by vertex colour over white: for the pictures each material gets its
    paint's colour from the piece's JSON (`colours` by `paint_names`), times the vertex colour."""
    d = piece_json(kind, name)
    cols = dict(zip(d.get("paint_names", []), d.get("colours", [])))
    done = set()
    for o in objs:
        if o.type != "MESH":
            continue
        for m in o.data.materials:
            if m is None or m.name in done:
                continue
            done.add(m.name)
            paint = m.name.split(":", 1)[-1]
            hexc = cols.get(paint) or cols.get(paint.split(".")[0])
            if not hexc or not m.node_tree:
                continue
            nt = m.node_tree
            b = nt.nodes.get("Principled BSDF")
            if b is None:
                continue
            rgb = kit.hex_rgb(hexc)
            src = b.inputs["Base Color"].links[0].from_socket if b.inputs["Base Color"].links else None
            mul = nt.nodes.new("ShaderNodeMix")
            mul.data_type = "RGBA"
            mul.blend_type = "MULTIPLY"
            mul.inputs["Factor"].default_value = 1.0
            mul.inputs[7].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
            if src is not None:
                nt.links.new(src, mul.inputs[6])
            else:
                mul.inputs[6].default_value = (1, 1, 1, 1)
            nt.links.new(mul.outputs[2], b.inputs["Base Color"])
            b.inputs["Roughness"].default_value = 0.8


def pose_part(objs, part, deg, axis_game, pivot_game):
    """Turn one part of a piece (its bone in the .glb) by `deg` about its JSON axis through its pivot, as the JSON's
    rule says (+t about an axis in the game's axes)."""
    arm = [o for o in objs if o.type == "ARMATURE"][0]
    pb = arm.pose.bones[part]
    # (the flipped axis, the same turn: checked on the tank's hatch, which opens upward)
    Rw = R_axis(g2b(axis_game), deg)
    piv = g2b(pivot_game)
    Mw = M4(Rw, piv - Rw @ piv)
    rest = np.array(arm.matrix_world @ pb.bone.matrix_local)
    pb.matrix = Matrix((np.linalg.inv(np.array(arm.matrix_world)) @ Mw @ rest).tolist())
    bpy.context.view_layer.update()


def env_tree(objs, drop=None):
    """A BVH of a piece's meshes as they stand (posed), in the world (and its bmesh). `drop(centre, material name)`
    leaves faces out (the PanzerIV's painted discs over its open hatch)."""
    dg = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    for o in objs:
        if o.type != "MESH":
            continue
        me = o.evaluated_get(dg).to_mesh()
        t = bmesh.new()
        t.from_mesh(me)
        t.transform(o.matrix_world)
        if drop is not None:
            mats = [mm.name if mm else "" for mm in o.data.materials]
            gone = [f for f in t.faces if drop(np.array(f.calc_center_median()), mats[f.material_index] if f.material_index < len(mats) else "")]
            bmesh.ops.delete(t, geom=gone, context="FACES")
        tm = bpy.data.meshes.new("_t")
        t.to_mesh(tm)
        bm.from_mesh(tm)
        t.free()
        bpy.data.meshes.remove(tm)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    return BVHTree.FromBMesh(bm), bm


# ================================================================================================ the man
def load_man(nation="GER", weapon="rifle", fresh=False):
    """The soldier, dressed, armed and rigged (soldier.assemble), kept in a .blend after the first time so that the
    poses can be tried again in seconds. Returns soldier.assemble's dict (arm, rig, body, kit, helmet, weapon...)."""
    os.makedirs(WORK, exist_ok=True)
    blend = os.path.join(WORK, f"man_{nation}_{weapon}.blend")
    meta = blend[:-6] + ".pkl"
    if fresh or not (os.path.exists(blend) and os.path.exists(meta)):
        m = S.assemble(nation, weapon)
        names = {"arm": m["arm"].name, "body": m["body"].name, "kit": [o.name for o in m["kit"]], "helmet": m["helmet"].name,
                 "weapon": m["weapon"].name, "weapon_part": m["weapon_part"].name, "sockets": [o.name for o in m["sockets"]]}
        objs = [m["arm"], m["body"], m["helmet"], m["weapon"], m["weapon_part"]] + m["kit"] + m["sockets"]
        bpy.data.libraries.write(blend, set(objs), fake_user=True)
        pickle.dump({"names": names, "J": m["J"], "nation": nation, "weapon_kind": weapon, "seed": m["seed"]}, open(meta, "wb"))
        m["rig"].fit = (1.0, 1.0)
        return m
    d = pickle.load(open(meta, "rb"))
    nm = d["names"]
    want = [nm["arm"], nm["body"], nm["helmet"], nm["weapon"], nm["weapon_part"]] + nm["kit"] + nm["sockets"]
    with bpy.data.libraries.load(blend, link=False) as (src, dst):
        dst.objects = list(want)
    got = dict(zip(want, dst.objects))
    for o in dst.objects:
        bpy.context.scene.collection.objects.link(o)
    J = d["J"]
    S.fit_body(J)
    arm = got[nm["arm"]]
    rig = S.Rig(arm, J)
    rig.fit = (1.0, 1.0)                      # (the crew's poses are set out on this man: nothing is scaled)
    rig.weapon_kind = weapon
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return {"nation": nation, "arm": arm, "rig": rig, "body": got[nm["body"]], "kit": [got[n] for n in nm["kit"]],
            "helmet": got[nm["helmet"]], "weapon": got[nm["weapon"]], "weapon_part": got[nm["weapon_part"]],
            "sockets": [got[n] for n in nm["sockets"]], "J": J, "weapon_kind": weapon, "seed": d["seed"]}


def clone(m, with_weapon=True):
    """Another man from the same meshes, on an armature of his own, with a Rig that keys it."""
    import copy
    arm = m["arm"].copy()
    arm.data = m["arm"].data
    bpy.context.scene.collection.objects.link(arm)
    arm.animation_data_clear()
    pieces = []
    for ob in [m["body"]] + m["kit"] + [m["helmet"]] + ([m["weapon"], m["weapon_part"]] if with_weapon else []):
        c = ob.copy()
        bpy.context.scene.collection.objects.link(c)
        c.parent = arm
        c.matrix_parent_inverse = Matrix.Identity(4)
        for md in c.modifiers:
            if md.type == "ARMATURE":
                md.object = arm
        pieces.append(c)
    rig = copy.copy(m["rig"])
    rig.arm, rig.prev_q = arm, {}
    return {"arm": arm, "rig": rig, "pieces": pieces, "weapon": pieces[-2] if with_weapon else None,
            "weapon_part": pieces[-1] if with_weapon else None}


class Skin:
    """The man's meshes skinned here (linear blend, as the armature modifier does it) from a solved pose, so that a
    pose can be measured against a seat or a hull without Blender evaluating anything."""

    def __init__(self, m, objs=None):
        arm = m["arm"]
        self.bones = [b.name for b in arm.data.bones]
        bi = {n: i for i, n in enumerate(self.bones)}
        self.M0 = {b.name: np.array(b.matrix_local) for b in arm.data.bones}
        self.M0i = np.stack([np.linalg.inv(self.M0[n]) for n in self.bones])
        self.parts = []
        for ob in objs or ([m["body"]] + m["kit"] + [m["helmet"]]):
            me = ob.data
            n = len(me.vertices)
            co = np.empty(n * 3)
            me.vertices.foreach_get("co", co)
            co = co.reshape(-1, 3)
            Mo = np.array(arm.matrix_world.inverted() @ ob.matrix_world)
            co = co @ Mo[:3, :3].T + Mo[:3, 3]
            W = np.zeros((n, len(self.bones)))
            gname = {g.index: g.name for g in ob.vertex_groups}
            for v in me.vertices:
                for g in v.groups:
                    k = bi.get(gname.get(g.group))
                    if k is not None:
                        W[v.index, k] += g.weight
            W /= np.maximum(W.sum(1, keepdims=True), 1e-9)
            main = np.array([self.bones[i] for i in W.argmax(1)])
            self.parts.append((ob.name, co, W, main))

    def posed(self, st):
        """[(name, points in the root's frame, main bone of each point)]."""
        Sk = np.stack([M4(st["D"][b] @ self.M0[b][:3, :3], st["H"][b]) @ self.M0i[i] for i, b in enumerate(self.bones)])
        out = []
        for name, co, W, main in self.parts:
            P = np.einsum("vb,bij,vj->vi", W, Sk[:, :3, :3], co) + W @ Sk[:, :3, 3]
            out.append((name, P, main))
        return out

    def points(self, st):
        ps = self.posed(st)
        return np.concatenate([p for _, p, _ in ps]), np.concatenate([b for _, _, b in ps])


def clearance(points, bones, tree, near=0.12):
    """How far the man's points go into a piece's solids: [(depth, bone)] for each point inside, deepest first. A
    point is inside where the nearest surface faces away from it (the meshes are closed and wound outward)."""
    out = []
    for p, b in zip(points, bones):
        loc, nrm, idx, dist = tree.find_nearest(Vector(p), near)
        if loc is None:
            continue
        if (Vector(p) - loc).dot(nrm) < 0.0:
            out.append((dist, str(b)))
    out.sort(reverse=True)
    return out


# ================================================================================================ posing
# Measures of the man taken from his joints (set by measure_man): the hips, the shoulders, the arm's reach.
MAN = {}


def measure_man(rig):
    J = rig.J
    H0 = rig.H0
    MAN["pelvis"] = H0["pelvis"].copy()
    MAN["hip"] = {s: H0["thigh_" + s] - H0["pelvis"] for s in ("l", "r")}
    MAN["thigh"] = float(np.linalg.norm(H0["calf_l"] - H0["thigh_l"]))
    MAN["shin"] = float(np.linalg.norm(H0["foot_l"] - H0["calf_l"]))
    MAN["upper"] = float(np.linalg.norm(H0["lowerarm_l"] - H0["upperarm_l"]))
    MAN["fore"] = float(np.linalg.norm(H0["hand_l"] - H0["lowerarm_l"]))
    MAN["grip"] = {s: float(np.linalg.norm(S.grip_point(J, s) - H0["hand_" + s])) for s in ("l", "r")}
    MAN["ankle"] = float(H0["foot_l"][2])
    MAN["eye"] = rig.eye0["r"].copy()
    return MAN


def hand_axes(s, a, n):
    """(a, n) made square: a from the wrist to the knuckles, n the way the palm faces."""
    a = _n(a)
    n = np.asarray(n, float)
    n = _n(n - a * (a @ n))
    return a, n


def thumb_way(s, a, n):
    """Which way the thumb side lies for a hand held with (a, n) (the left hand's axes are mirrored)."""
    return np.cross(n, a) if s == "l" else np.cross(a, n)


def hand_on(at, a, n, pole, fingers=None):
    a, n = hand_axes("l", a, n)
    h = {"at": tuple(np.asarray(at, float)), "a": tuple(a), "n": tuple(n), "pole_world": tuple(np.asarray(pole, float))}
    return h


def stowed():
    """The rifle put away: the weapon bone hidden inside his chest and scaled to nothing (crew.json: weapon none)."""
    def place(rig, st):
        C = rig.frame(st, "chest")
        return C @ M4(np.eye(3), (0.05, 0.0, 0.05))
    return place


def seated_legs(seat_h, knee_out=0.11, feet_ahead=0.46, feet_out=0.13, toe=0.0, yaw=(8.0, -8.0)):
    """Feet flat on the floor `seat_h` below the seat, ahead of it; the knees led up and forward."""
    fz = -seat_h + MAN["ankle"]
    return {"foot_l": {"at": (feet_ahead, feet_out, fz), "yaw": yaw[0], "pole": (1.0, 0.25 * knee_out / 0.11, 0.55), "toe": toe},
            "foot_r": {"at": (feet_ahead - 0.02, -feet_out, fz), "yaw": yaw[1], "pole": (1.0, -0.25 * knee_out / 0.11, 0.55), "toe": toe}}


def sit_body(seat_h, slouch=0.0, lean=0.0, turn=0.0, hip_x=-0.07, hip_z=0.115):
    """Sitting on a seat whose top is the root: the pelvis tipped back a little, the back upright over it (slouch
    rounds it, lean bends him forward from the hips, turn twists the chest)."""
    pz = hip_z
    P = {"pelvis_at": (hip_x, 0.0, pz - MAN["pelvis"][2]), "pelvis": (-12.0 + 0.8 * lean, 0.0, 0.0),
         "spine_01": (6.0 + 4.0 * slouch + 0.1 * lean, 0.0, 0.2 * turn), "spine_02": (5.0 + 5.0 * slouch + 0.1 * lean, 0.0, 0.3 * turn),
         "chest": (4.0 + 4.0 * slouch, 0.0, 0.3 * turn), "neck": (-3.0 - 4.0 * slouch, 0.0, 0.1 * turn), "head": (-2.0 - 3.0 * slouch, 0.0, 0.1 * turn),
         "clavicle_l": (0.0, 4.0), "clavicle_r": (0.0, 4.0), "fingers_l": 40.0, "fingers_r": 40.0, "thumb_l": 25.0, "thumb_r": 25.0}
    P.update(seated_legs(seat_h))
    return P


def settle_on_seat(rig, skin, P, seat_top=0.0, sink=0.004, region=(-0.25, 0.2), lift_knees=4):
    """Raise or lower the pelvis so his seat rests on the seat's top (the cloth pressed `sink` into it): the lowest
    point of the body over the seat, measured on the skinned mesh. (lift_knees 0: the rig's rule that keeps a knee off
    the ground at the root's height is not wanted on a seat, where the floor is seat_h lower.)"""
    for _ in range(3):
        st = rig.solve(P, lift_knees=lift_knees)
        pts, bones = skin.points(st)
        sel = (pts[:, 0] > region[0]) & (pts[:, 0] < region[1]) & np.isin(bones, ("pelvis", "thigh_l", "thigh_r", "spine_01"))
        low = float(pts[sel, 2].min())
        dz = (seat_top - sink) - low
        if abs(dz) < 0.0005:
            break
        pa = list(P["pelvis_at"])
        pa[2] += dz
        P = dict(P, pelvis_at=tuple(pa))
    return P


SIT_FEET = {"ahead": 0.46, "thigh_max": 30.0}   # (seated: his feet this far ahead of the seat point; on a high seat
                                                #  the shins hang under the knees and the thighs slope down, at most
                                                #  30 degrees, his seat on the seat's front edge)
POSE_LIMITS = {"sit": (0.15, 0.60), "ground": 0.12}   # (seat_h that crew.pose's seated legs reach the floor from; below
                                                      #  `ground` he sits on the floor itself, knees up)


def high_seat(seat_h):
    """0 on a seat up to 0.45 over the floor, 1 from 0.55: how far the high seat's way of sitting is taken."""
    return float(np.clip((seat_h - 0.45) / 0.1, 0.0, 1.0))


def sit_feet_ahead(seat_h):
    """How far ahead of the seat point a seated man's feet go: SIT_FEET's 0.46, or on a high seat under his knees,
    the thighs sloped down as far as the floor asks (to SIT_FEET's limit)."""
    hip = (-0.07, 0.09)
    s = (seat_h + hip[1] - MAN["ankle"] - MAN["shin"]) / MAN["thigh"]
    th = math.asin(float(np.clip(s, 0.0, math.sin(math.radians(SIT_FEET["thigh_max"])))))
    under = hip[0] + MAN["thigh"] * math.cos(th) + 0.03
    w = high_seat(seat_h)
    return float((1.0 - w) * SIT_FEET["ahead"] + w * under)


def pose(man, stance, seat_h=0.0, grip_l=None, grip_r=None, facing=(1.0, 0.0, 0.0), at=(0.0, 0.0, 0.0), axes="game",
         look=None, key=True, frame=0, place=True, feet=None, root="feet"):
    """A man posed at a crew socket: `stance` stand / kneel / sit / prone; `seat_h` from his feet down from the seat
    (sit); his hands on the grips (points, or None to rest them); `facing` and `at` the socket's (the piece's axes:
    the game's, y to the right, unless axes="blender"); `look` a point to look at; `feet` {"l": point, "r": point}
    where each boot stands (the floor under his ankle; either may be left out), else where the stance puts them.
    The root goes on the seat point (sit) or the floor at his feet (the rest); root="hips" puts his hips over `at`
    instead (a socket that is the floor under his hips: the kneel's root is 0.21 ahead of them). The pose is keyed on
    his armature at `frame` and the armature put at the socket. Returns (P, solved state); pose_report(P, st) says
    how far each hand and foot fell short of its target."""
    rig = man["rig"]
    if not MAN:
        measure_man(rig)
    cv = g2b if axes == "game" else (lambda v: np.asarray(v, float))
    seated = stance == "sit" and seat_h > POSE_LIMITS["ground"]
    lift = 0 if seated else 4
    at_b = cv(at)
    if root == "hips":
        st0 = rig.solve(body_for(rig, stance, seat_h, None, None, None), lift_knees=lift)
        hip = np.array((st0["H"]["pelvis"][0], st0["H"]["pelvis"][1], 0.0))
        at_b = at_b - Place(at_b, cv(facing)).R @ hip
    pl = Place(at_b, cv(facing))
    gl = None if grip_l is None else pl.local(cv(grip_l))
    gr = None if grip_r is None else pl.local(cv(grip_r))
    lk = None if look is None else pl.local(cv(look))
    P = body_for(rig, stance, seat_h, gl, gr, lk)
    if feet:
        P = feet_on(rig, P, stance, seat_h, {s: pl.local(cv(f)) for s, f in feet.items() if f is not None})
    if seated:
        P = settle_on_seat(rig, man.get("skin") or Skin(man), P, region=(-0.25, 0.2 - 0.15 * high_seat(seat_h)), lift_knees=0)
    st = rig.solve(P, lift_knees=lift)
    if key:
        S.pose_once(man["arm"], rig, st, frame)
        man["arm"].scale = (1.0, 1.0, 1.0)
        pb = man["arm"].pose.bones["weapon"]
        pb.scale = (0.0, 0.0, 0.0) if P.get("weapon_hidden") else (1.0, 1.0, 1.0)
        pb.keyframe_insert("scale", frame=frame, group="weapon")
    if place:
        man["arm"].matrix_world = Matrix(pl.matrix().tolist())
    P["_root"] = pl.matrix()                   # (where the root went: the Blender world matrix; the rig ignores it)
    P["_lift_knees"] = lift
    return P, st


def feet_on(rig, P, stance, seat_h, feet):
    """The boots put where asked (points in his root's frame, on the floor under each ankle). Standing, the hips come
    down by as much as the lower foot is below his root (so its leg still reaches); a foot higher up (a step) just bends
    its knee. Kneeling, the left foot is the one forward (its knee up), the right the kneeling one's toes."""
    P = dict(P)
    for s, f in feet.items():
        key = "foot_" + s
        old = dict(P[key])
        f = np.asarray(f, float)
        if stance == "kneel" and s == "r":
            old["at"] = (float(f[0]), float(f[1]), float(f[2]) + old["at"][2])
        else:
            old["at"] = (float(f[0]), float(f[1]), float(f[2]) + MAN["ankle"])
        P[key] = old
    if stance == "stand":
        low = min(float(np.asarray(f, float)[2]) for f in feet.values())
        if low < 0.0:
            pa = P["pelvis_at"]
            P["pelvis_at"] = (pa[0], pa[1], pa[2] + low)
    return P


def pose_report(P, st):
    """How far each hand fell short of its grip and each foot of where it was put (m), for a pose from pose()."""
    rig = MODEL["rig"] if MODEL else None
    out = {"hand_gap": {}, "leg_short": {s: round(float(st["short"].get("leg_" + s, 0.0)), 4) for s in ("l", "r")}}
    if rig is not None:
        out["hand_gap"] = {s: round(v, 4) for s, v in grip_gaps(rig, st, P).items()}
    return out


def reach_of(s):
    return MAN["upper"] + MAN["fore"] + MAN["grip"][s]


def default_hand(s, shoulder, g, palm="down"):
    """A hand on a handle with no way given: the knuckles on along the reach, the palm down and a little in."""
    side = 1.0 if s == "l" else -1.0
    d = np.asarray(g, float) - shoulder
    h = np.array((d[0], d[1], 0.0))
    if np.linalg.norm(h) < 1e-3:
        h = np.array((1.0, 0.0, 0.0))
    a = _n(_n(h) + np.array((0.0, 0.0, -0.45)))
    n = np.array((0.0, -0.35 * side, -1.0)) if palm == "down" else np.array((0.0, -side, 0.0))
    return a, n


def body_for(rig, stance, seat_h, gl, gr, look):
    """The plain pose for a stance with the hands on the grips (the general case of pose)."""
    # 1. the body for the stance
    if stance == "sit":
        if seat_h > POSE_LIMITS["ground"]:
            P = sit_body(seat_h)
            P.update(seated_legs(seat_h, feet_ahead=sit_feet_ahead(seat_h)))
        else:                                                  # on the ground, knees up
            P = sit_body(0.0)
            P.update({"pelvis_at": (-0.04, 0.0, 0.11 - MAN["pelvis"][2]), "pelvis": (-22.0, 0.0, 0.0),
                      "foot_l": {"at": (0.52, 0.17, MAN["ankle"]), "yaw": 14.0, "pole": (0.6, 0.3, 1.0)},
                      "foot_r": {"at": (0.5, -0.17, MAN["ankle"]), "yaw": -14.0, "pole": (0.6, -0.3, 1.0)}})
    elif stance == "kneel":
        P = kneel_grounded(S.kneel_base(aimed=False))
    elif stance == "prone":
        P = S.prone_base()
    else:
        P = S.base_stand()
        P["pelvis"] = (2.0, 0.0, 0.0)
        P["foot_l"] = {"at": (0.04, 0.12, MAN["ankle"]), "yaw": 8.0}
        P["foot_r"] = {"at": (-0.04, -0.12, MAN["ankle"]), "yaw": -8.0}
    P["weapon"] = stowed()
    P["weapon_hidden"] = True
    grips = {"l": gl, "r": gr}
    if gl is None and gr is None:
        if stance == "sit":
            P["hand_l"] = {"frame": "chest", "at": (0.3, 0.16, -0.42), "a": (0.6, 0.1, -0.8), "n": (0.0, 0.0, -1.0), "pole": (-0.3, 1.0, -0.6)}
            P["hand_r"] = {"frame": "chest", "at": (0.3, -0.16, -0.42), "a": (0.6, -0.1, -0.8), "n": (0.0, 0.0, -1.0), "pole": (-0.3, -1.0, -0.6)}
        else:
            P["hand_l"] = {"frame": "chest", "at": (0.02, 0.24, -0.55), "a": (0.0, 0.1, -1.0), "n": (0.0, -1.0, 0.0), "pole": (-1.0, 0.3, 0.0)}
            P["hand_r"] = {"frame": "chest", "at": (0.02, -0.24, -0.55), "a": (0.0, -0.1, -1.0), "n": (0.0, 1.0, 0.0), "pole": (-1.0, -0.3, 0.0)}
        if look is not None:
            aim_head(P, rig, look)
        return P
    # 2. lean and turn toward the grips until both are in easy reach; where a hand still falls short of its grip, lean
    #    further, turn and bend to the side more until it is on it (the least lean that does)
    mid = np.mean([g for g in (gl, gr) if g is not None], axis=0)
    turn = float(np.clip(math.degrees(math.atan2(mid[1], max(mid[0], 0.05))), -40.0, 40.0))
    lift = 0 if (stance == "sit" and seat_h > POSE_LIMITS["ground"]) else 4
    base = dict(P)

    def leaned(lean, tw, side=0.0):
        Q = dict(base)
        Q["spine_01"] = add3(base.get("spine_01", (0, 0, 0)), (0.3 * lean, 0.3 * side, 0.25 * tw))
        Q["spine_02"] = add3(base.get("spine_02", (0, 0, 0)), (0.35 * lean, 0.35 * side, 0.35 * tw))
        Q["chest"] = add3(base.get("chest", (0, 0, 0)), (0.35 * lean, 0.35 * side, 0.3 * tw))
        Q["neck"] = add3(base.get("neck", (0, 0, 0)), (-0.4 * lean, -0.3 * side, 0.0))
        Q["head"] = add3(base.get("head", (0, 0, 0)), (-0.3 * lean, -0.3 * side, 0.0))
        Q["hand_l"] = {"frame": "chest", "at": (0.3, 0.16, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)}
        Q["hand_r"] = {"frame": "chest", "at": (0.3, -0.16, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)}
        return Q

    def handed(Q):
        st = rig.solve(Q, lift_knees=lift)
        Q = dict(Q)
        for s, g in grips.items():
            side = 1.0 if s == "l" else -1.0
            if g is None:
                Q["hand_" + s] = {"frame": "chest", "at": (0.25, 0.18 * side, -0.4), "a": (0.6, 0.2 * side, -0.8), "n": (0.0, 0.0, -1.0),
                                  "pole": (-0.3, side, -0.6)}
                continue
            a, n = default_hand(s, st["H"]["upperarm_" + s], g)
            Q["hand_" + s] = hand_on(g, a, n, np.array((-0.25, 0.9 * side, -1.0)))
            Q["fingers_" + s], Q["thumb_" + s] = 62.0, 40.0
        st = rig.solve(Q, lift_knees=lift)
        return Q, max([0.0] + list(grip_gaps(rig, st, Q).values()))

    lean0 = 47.5
    for lean in np.arange(0.0, 50.0, 2.5):
        st = rig.solve(leaned(lean, turn), lift_knees=lift)
        worst = max(np.linalg.norm(g - st["H"]["upperarm_" + s]) / reach_of(s) for s, g in grips.items() if g is not None)
        if worst < 0.9:
            lean0 = lean
            break
    P, gap = handed(leaned(lean0, turn))
    if gap > 0.004:
        # (the side bend toward the lower grip's side: + to his left)
        low = min((g for g in grips.values() if g is not None), key=lambda g: g[2])
        bend = 1.0 if low[1] > 0 else -1.0
        best = (gap, 0.0, P)
        for lean in np.arange(lean0, 62.5, 2.5):
            for dt in (0.0, 10.0, -10.0, 20.0, -20.0):
                for sd in (0.0, 10.0, 20.0):
                    Q, g = handed(leaned(lean, float(np.clip(turn + dt, -60.0, 60.0)), bend * sd))
                    if g < best[0] - 1e-4:
                        best = (g, lean, Q)
            if best[0] <= 0.004:
                break
        P = best[2]
    if look is not None:
        aim_head(P, rig, look)
    return P


def aim_head(P, rig, look):
    """Turn the neck and head so his eyes look at a point (the root's frame)."""
    st = rig.solve(P)
    eye = 0.5 * (rig.eye(st, "l") + rig.eye(st, "r"))
    d = np.asarray(look, float) - eye
    C = st["D"]["chest"]
    dl = C.T @ d                          # in the chest's frame
    yaw = math.degrees(math.atan2(dl[1], dl[0]))
    pitch = math.degrees(math.atan2(-dl[2], math.hypot(dl[0], dl[1])))
    yaw = float(np.clip(yaw, -75.0, 75.0))
    pitch = float(np.clip(pitch, -40.0, 50.0))
    P["neck"] = (0.45 * pitch, 0.0, 0.45 * yaw)
    P["head"] = (0.55 * pitch, 0.0, 0.55 * yaw)
    return P


# ================================================================================================ the clips
CREW = {}


def crew_clip(name, long, loop=False, anchor="feet", weapon="rifle", place=None, events=(), about="", fps=FPS):
    """A clip: `fn(t)` gives the pose (soldier.py's plain terms) at t seconds. anchor: seat | feet (where the root is);
    weapon: rifle (in his hands) | lmg (the gunner's own gun: play it on soldier_GER_lmg) | none (put away);
    place: [(kind, piece, socket, root offset from the socket)] where it is meant to be played."""
    def deco(fn):
        CREW[name] = dict(name=name, fn=fn, long=long, loop=loop, anchor=anchor, weapon=weapon, place=list(place or []),
                          events=list(events), about=about, props={})
        return fn
    return deco


def looped(keys, long):
    """Key poses [(t, P)] passed through smoothly round a loop (Catmull-Rom with the ends joined: the first key is
    the last one again at t = long)."""
    times = [k[0] for k in keys]
    poses = [k[1] for k in keys]
    n = len(keys)

    def at(t):
        t = t % long
        i = max(j for j in range(n) if times[j] <= t)
        t1 = times[i + 1] if i + 1 < n else long
        u = (t - times[i]) / (t1 - times[i])
        p0, p1, p2, p3 = poses[(i - 1) % n], poses[i], poses[(i + 1) % n], poses[(i + 2) % n]
        w = ((-u ** 3 + 2 * u * u - u) * 0.5, (3 * u ** 3 - 5 * u * u + 2) * 0.5, (-3 * u ** 3 + 4 * u * u + u) * 0.5, (u ** 3 - u * u) * 0.5)
        Ps, ws = [], []
        for P, wi in zip((p0, p1, p2, p3), w):
            if abs(wi) > 1e-9:
                Ps.append(P)
                ws.append(wi)
        return S.mix(Ps, ws)
    return at


def wave(t, long, k, phase=0.0):
    """A sine that loops `k` times in a clip `long` seconds long."""
    return math.sin(2 * math.pi * (k * t / long + phase))


RIFLE = {}


def rifle_measures(m):
    """The rifle's own measures (its frame: origin at the right hand's grip, x to the muzzle, z up): the butt's end
    and its lowest point standing on it, the barrel's line."""
    me = m["weapon"].data
    co = np.array([tuple(v.co) for v in me.vertices]) - np.array(m["arm"].data.bones["weapon"].head_local)
    RIFLE["pts"] = co
    RIFLE["butt_x"] = float(co[:, 0].min())
    RIFLE["muzzle_x"] = float(co[:, 0].max())
    return RIFLE


def rifle_upright(butt_at, lean_back=8.0, roll=0.0, side=0.0, turn=0.0):
    """The rifle stood on its butt at `butt_at` (the floor point under the butt), its muzzle up and leaned back toward
    the man by `lean_back` degrees (side: leaned to his left), its sights toward him."""
    x = np.array((0.0, 0.0, 1.0))
    R = Rz(turn) @ R_axis((0, 1, 0), -lean_back) @ R_axis((1, 0, 0), -side)
    xw = R @ x
    zw = R @ np.array((-1.0, 0.0, 0.0))
    Rw = np.stack([xw, np.cross(zw, xw), zw], axis=1)
    Rw = Rw @ R_axis((1, 0, 0), roll)
    pts = RIFLE["pts"] @ Rw.T
    low = pts[np.argmin(pts[:, 2])]
    o = np.asarray(butt_at, float) - low
    return M4(Rw, o)


def on_rifle(W, s, x_w, a_hint, up=1.0, pole=None, z_w=0.012):
    """A hand round the rifle at x_w along it (its barrel or fore-end), the thumb up the rifle (up=-1: down it),
    the knuckles on along `a_hint` (the way the forearm comes in), the palm round the wood."""
    side = 1.0 if s == "l" else -1.0
    R, o = W[:3, :3], W[:3, 3]
    g = o + R @ np.array((x_w, 0.0, z_w))
    t = R[:, 0] * up                                        # (the thumb's way)
    a = np.asarray(a_hint, float)
    a = _n(a - t * (t @ a))
    n = np.cross(a, t) if s == "l" else np.cross(t, a)     # (so that the thumb lies along t)
    if pole is None:
        pole = (-0.3, 0.8 * side, -1.0)
    return {"at": tuple(g), "a": tuple(a), "n": tuple(n), "pole_world": tuple(pole)}


SEAT_FIT = {}


def seated(name, P, seat_top=0.0):
    """The pelvis height that seats this clip's man on the seat's top (measured once per clip on its first pose)."""
    if name not in SEAT_FIT:
        Q = settle_on_seat(MODEL["rig"], MODEL["skin"], dict(P), seat_top)
        SEAT_FIT[name] = Q["pelvis_at"][2] - P["pelvis_at"][2]
    pa = list(P["pelvis_at"])
    pa[2] += SEAT_FIT[name]
    P["pelvis_at"] = tuple(pa)
    return P


GROUND_FIT = {}


def grounded(name, P, floor=0.0, give=0.004):
    """A man lying down with his knees and boot toes on the ground, not in it: the feet drawn back (the legs
    straightened) and lifted until nothing of the legs goes more than `give` below the floor (found once per clip on
    its first pose, then the same shift on every frame)."""
    if name not in GROUND_FIT:
        rig, skin = MODEL["rig"], MODEL["skin"]
        Q0 = dict(P)
        for h, y in (("hand_l", 0.1), ("hand_r", -0.1)):
            Q0.setdefault(h, {"frame": "chest", "at": (0.3, y, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)})
        z0 = float(rig.solve(Q0)["H"]["pelvis"][2])                  # (where the solver's own knee lift left his hips)
        dpz = z0 - float(rig.H0["pelvis"][2] + Q0.get("pelvis_at", (0, 0, 0))[2])
        dx = dz = 0.0
        legs = ("thigh_l", "thigh_r", "calf_l", "calf_r")
        toes = ("foot_l", "foot_r", "ball_l", "ball_r")
        for _ in range(30):
            Q = _shift_feet(Q0, dx, dz, dpz)
            pts, bones = skin.points(rig.solve(Q))
            kl = pts[np.isin(bones, legs), 2].min()
            tl = pts[np.isin(bones, toes), 2].min()
            if kl >= floor - give and tl >= floor - give:
                break
            if kl < floor - give:
                dx -= 0.01
            if tl < floor - give:
                dz += 0.005
        GROUND_FIT[name] = (dx, dz, dpz)
    return _shift_feet(P, *GROUND_FIT[name])


def _shift_feet(P, dx, dz, dpz=0.0):
    Q = dict(P)
    pa = np.asarray(P.get("pelvis_at", (0, 0, 0)), float)
    Q["pelvis_at"] = (pa[0], pa[1], pa[2] + dpz)
    for s in ("l", "r"):
        f = dict(P["foot_" + s])
        f["at"] = tuple(np.asarray(f["at"], float) + np.array((dx, 0.0, dz)))
        Q["foot_" + s] = f
    return Q


MODEL = {}          # the pattern man the clips are measured on (rig, skin)

# ---- the vehicles: measured off the M3 (crew.json `measure` has every vehicle's)
BENCH_H = 0.44      # the M3's bench top over its floor (the Truck's is 0.455)
CAB_H = 0.40        # the M3's front seats over the floor in front of them
WHEEL = {"centre": (0.44, 0.0, 0.16), "up": (-0.595, 0.0, 0.804), "r": 0.20}    # the M3's wheel from the driver's seat point


def bench_body(t, long, seed=0.0, slouch=0.6, hip_x=0.065):
    """A rider on the bench: the vehicle's sway taken in the hips and the back, the head steadying itself."""
    roll = 2.2 * wave(t, long, 3, seed) + 0.8 * wave(t, long, 5, 0.3 + seed)
    pitch = 1.6 * wave(t, long, 2, 0.15 + seed) + 0.6 * wave(t, long, 7, seed)
    br = wave(t, long, 2, 0.6 + seed)
    P = sit_body(BENCH_H, slouch=slouch, hip_x=hip_x)
    P["pelvis"] = add3(P["pelvis"], (0.4 * pitch, roll, 0.0))
    P["spine_01"] = add3(P["spine_01"], (0.4 * pitch, -0.3 * roll, 0.0))
    P["spine_02"] = add3(P["spine_02"], (0.3 * pitch - 0.4 * br, -0.3 * roll, 0.0))
    P["chest"] = add3(P["chest"], (-0.8 * br, -0.2 * roll, 0.0))
    P["neck"] = add3(P["neck"], (-0.5 * pitch, -0.3 * roll, 0.0))
    P["head"] = add3(P["head"], (-0.4 * pitch, -0.3 * roll, 0.0))
    P["clavicle_l"] = add3(P["clavicle_l"], (1.0 * br, 0.0))[:2]
    P["clavicle_r"] = add3(P["clavicle_r"], (1.0 * br, 0.0))[:2]
    return P, roll, pitch


@crew_clip("sit_bench", 6.0, loop=True, anchor="seat", weapon="rifle",
           place=[("vehicle", v, "rider.%d" % i, (0, 0, 0)) for v in ("M3Halftrack", "Truck", "Hanomag") for i in range(1, 11)],
           about="A rider on a bench facing inboard: the rifle upright between his knees, both hands on it, the vehicle's "
                 "sway in his hips and back, his head looking about now and then.")
def c_sit_bench(t):
    long = 6.0
    P, roll, pitch = bench_body(t, long)
    look = bump(t, 1.2, 1.9, 3.0) - 0.7 * bump(t, 3.6, 4.2, 5.2)
    P["neck"] = add3(P["neck"], (2.0 * bump(t, 3.6, 4.2, 5.2), 0.0, 22.0 * look))
    P["head"] = add3(P["head"], (-3.0 * bump(t, 1.2, 1.9, 3.0), 0.0, 25.0 * look))
    W = rifle_upright((0.4, 0.0, -BENCH_H), lean_back=9.0 + 0.5 * pitch, side=-0.5 * roll)
    P["weapon"] = W
    P["hand_l"] = on_rifle(W, "l", 0.6, (0.75, -0.65, -0.1), pole=(-0.2, 1.0, -0.8))
    P["hand_r"] = on_rifle(W, "r", 0.49, (0.75, 0.65, -0.1), pole=(-0.2, -1.0, -0.8))
    for s in ("l", "r"):                  # (the elbows held to their poles: else the arm hops between two easy places)
        P["hand_" + s]["pole_weight"] = 0.03
    P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 62.0, 62.0, 45.0, 45.0
    return seated("sit_bench", P)


def on_rim(s, centre, up, r, clock, a_lean=0.0, roll=90.0):
    """A hand round a steering wheel's rim at a clock angle (degrees from the top, + to his left), the thumb along
    the rim toward the top, the knuckles over the rim away from him; the hand free to turn up to `roll` degrees round
    the rim and to take the rim the other way (flip), where his wrist and forearm sit easiest (soldier.comfortable_arm)."""
    c = np.asarray(centre, float)
    u = _n(up)
    v = np.array((0.0, 1.0, 0.0))
    v = _n(v - u * (u @ v))
    face = np.cross(v, u)                                        # (the wheel's face away from him: forward and up)
    ph = math.radians(clock)
    g = c + r * (math.cos(ph) * u + math.sin(ph) * v)
    tan_top = math.sin(ph) * u - math.cos(ph) * v                # (along the rim toward the top)
    if abs(clock) < 1e-6:
        tan_top = -v if s == "l" else v
    elif clock < 0.0:
        tan_top = -tan_top                                       # (on his right of the top the rim's way up turns)
    t = _n(tan_top)
    radial = _n(g - c)
    a = _n(face + 0.7 * radial + a_lean * np.array((0.0, 0.0, -1.0)))
    a = _n(a - t * (t @ a))
    n = np.cross(a, t) if s == "l" else np.cross(t, a)
    side = 1.0 if s == "l" else -1.0
    return {"at": tuple(g), "a": tuple(a), "n": tuple(n), "pole_world": (-0.3, 1.0 * side, -1.2), "roll": roll,
            "flip": True}


@crew_clip("sit_drive", 6.0, loop=True, anchor="seat", weapon="none",
           place=[("vehicle", v, "driver", (0, 0, 0)) for v in ("M3Halftrack", "Hanomag", "FlakHalftrack", "M16")],
           about="The driver: both hands on the wheel at half past ten and half past one, feet on the pedals, the body taking the ride, his eyes "
                 "on the road with a glance to each side. Fitted to the M3's wheel (its centre 0.44 ahead of the seat point "
                 "and 0.16 up, tilted 36 degrees back).")
def c_sit_drive(t):
    long = 6.0
    roll = 1.6 * wave(t, long, 3, 0.1) + 0.6 * wave(t, long, 5, 0.4)
    pitch = 1.2 * wave(t, long, 2, 0.3)
    br = wave(t, long, 2, 0.5)
    P = sit_body(CAB_H, slouch=0.2, lean=8.0, hip_x=0.06)
    P["pelvis"] = add3(P["pelvis"], (0.4 * pitch, roll, 0.0))
    P["spine_02"] = add3(P["spine_02"], (0.3 * pitch - 0.4 * br, -0.4 * roll, 0.0))
    P["chest"] = add3(P["chest"], (-0.8 * br, -0.3 * roll, 0.0))
    glance = bump(t, 1.6, 2.0, 2.7) - bump(t, 4.0, 4.4, 5.0)
    P["neck"] = add3(P["neck"], (-2.0 - 0.4 * pitch, -0.3 * roll, 26.0 * glance))
    P["head"] = add3(P["head"], (-2.0 - 0.3 * pitch, -0.4 * roll, 30.0 * glance))
    P["foot_l"] = {"at": (0.5, 0.27, -CAB_H + MAN["ankle"] + 0.02), "yaw": 14.0, "pitch": -12.0, "pole": (0.8, 1.0, 0.5)}
    P["foot_r"] = {"at": (0.56, -0.26, -CAB_H + MAN["ankle"] + 0.03), "yaw": -12.0, "pitch": -16.0, "pole": (0.8, -1.0, 0.5)}
    P["weapon"], P["weapon_hidden"] = stowed(), True
    P["hand_l"] = on_rim("l", WHEEL["centre"], WHEEL["up"], WHEEL["r"], 45.0, a_lean=0.8)
    P["hand_r"] = on_rim("r", WHEEL["centre"], WHEEL["up"], WHEEL["r"], -45.0, a_lean=0.8)
    P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 64.0, 64.0, 30.0, 30.0
    return seated("sit_drive", P)


@crew_clip("sit_cab", 6.0, loop=True, anchor="seat", weapon="rifle",
           place=[("vehicle", v, "rider.cab", (0, 0, 0)) for v in ("M3Halftrack", "Hanomag", "FlakHalftrack", "M16")],
           about="The man beside the driver: sat back, his rifle stood between his knees and leaned against his left "
                 "shoulder under his right hand, his left hand on his thigh; watching the road, and out of his side now and then.")
def c_sit_cab(t):
    long = 6.0
    roll = 1.6 * wave(t, long, 3, 0.35) + 0.6 * wave(t, long, 5, 0.1)
    pitch = 1.2 * wave(t, long, 2, 0.55)
    br = wave(t, long, 2, 0.2)
    P = sit_body(CAB_H, slouch=0.8, hip_x=0.065)
    P["pelvis"] = add3(P["pelvis"], (0.4 * pitch - 3.0, roll, 0.0))
    P["spine_02"] = add3(P["spine_02"], (0.3 * pitch - 0.4 * br, -0.4 * roll, 0.0))
    P["chest"] = add3(P["chest"], (-0.8 * br, -0.3 * roll, 0.0))
    look = bump(t, 2.2, 2.9, 4.0)
    P["spine_02"] = add3(P["spine_02"], (0.0, 0.0, -5.0 * look))
    P["neck"] = add3(P["neck"], (0.0, -0.3 * roll, -30.0 * look))
    P["head"] = add3(P["head"], (-2.0 * look, -0.3 * roll, -34.0 * look))
    P["foot_l"] = {"at": (0.44, 0.15, -CAB_H + MAN["ankle"]), "yaw": 14.0, "pole": (1.0, 0.35, 0.55)}
    P["foot_r"] = {"at": (0.48, -0.13, -CAB_H + MAN["ankle"]), "yaw": -10.0, "pole": (1.0, -0.25, 0.55)}
    W = rifle_upright((0.36, 0.03, -CAB_H), lean_back=17.0 + 0.4 * pitch, side=7.0 - 0.4 * roll)
    P["weapon"] = W
    P["hand_r"] = on_rifle(W, "r", 0.46, (0.3, 0.9, -0.3), pole=(-0.2, -1.0, -0.8))     # (high enough that the wrist lies straight)
    P["hand_l"] = {"at": (0.28, 0.13, 0.19), "a": (0.75, -0.25, -0.6), "n": (0.0, 0.0, -1.0), "pole_world": (-0.4, 1.0, -0.6)}
    P["fingers_l"], P["thumb_l"] = 22.0, 15.0
    P["fingers_r"], P["thumb_r"] = 62.0, 45.0
    return seated("sit_cab", P)


# ---- the tank commander, up in his hatch
CUPOLA = {"rim": 2.625, "inner": 0.285, "outer": 0.34, "socket_z": 2.375}      # the PanzerIV's cupola (measured, game z)
RIM_Z = 1.17                                                                   # the rim over his feet
BINO = {"half": 0.058}                                                         # half the spacing of the binoculars' barrels


def elbow_pole(rig, st, s, hand_at, elbow_at):
    """The pole that lays an arm's elbow at (or nearest) a wanted place, for the IK to the hand."""
    sh = st["H"]["upperarm_" + s]
    mid = 0.5 * (sh + np.asarray(hand_at, float))
    return np.asarray(elbow_at, float) - mid


@crew_clip("commander_hatch", 10.0, loop=True, anchor="feet", weapon="none",
           place=[("vehicle", "PanzerIV", "commander", (0.0, 0.0, CUPOLA["rim"] - RIM_Z - CUPOLA["socket_z"])),
                  ("vehicle", "ArmouredCar", "commander", (0.0, 0.0, -0.92)),
                  ("vehicle", "AssaultGun", "commander", (0.0, 0.0, -0.92))],
           about="The tank commander stood up in his hatch, chest out of it and forearms on the rim, the binoculars in his "
                 "hands; he looks about, and twice raises the glasses to his eyes. His feet are inside the turret: root "
                 "0.92 m below the PanzerIV's `commander` socket (the rim 1.17 m over his feet), and the ArmouredCar's "
                 "and the AssaultGun's.")
def c_commander(t):
    long = 10.0
    br = wave(t, long, 3, 0.2)
    sway = wave(t, long, 2, 0.1)
    up1 = ramp(t, 2.1, 2.9) * (1 - ramp(t, 4.6, 5.25))           # (raised over 0.8 s: his wrists turn under them)
    up2 = ramp(t, 6.85, 7.65) * (1 - ramp(t, 8.6, 9.25))
    raised = max(up1, up2)
    yaw = 30.0 * bump(t, 0.3, 1.3, 2.3) + 16.0 * up1 - 34.0 * bump(t, 5.2, 6.0, 6.9) - 12.0 * up2
    body_yaw = 0.35 * yaw
    P = {"pelvis_at": (0.0, 0.0, 0.0), "pelvis": (3.0, 0.5 * sway, 0.3 * body_yaw), "spine_01": (3.0, 0.0, 0.25 * body_yaw),
         "spine_02": (4.0 - 0.4 * br, -0.3 * sway, 0.25 * body_yaw), "chest": (5.0 - 0.8 * br - 4.0 * raised, -0.2 * sway, 0.2 * body_yaw),
         "clavicle_l": (7.0 + 5.0 * raised + 1.0 * br, 3.0), "clavicle_r": (7.0 + 5.0 * raised + 1.0 * br, 3.0),
         "foot_l": {"at": (0.03, 0.12, MAN["ankle"]), "yaw": 8.0}, "foot_r": {"at": (-0.04, -0.12, MAN["ankle"]), "yaw": -8.0},
         "weapon": stowed(), "weapon_hidden": True, "fingers_l": 52.0, "fingers_r": 52.0, "thumb_l": 30.0, "thumb_r": 30.0}
    look = yaw - body_yaw
    P["neck"] = (-3.0 + 2.0 * raised, 0.0, 0.45 * look)
    P["head"] = (-2.0 + 1.0 * raised, 0.0, 0.55 * look)
    rig = MODEL["rig"]
    st = rig.solve(dict(P, hand_l={"frame": "chest", "at": (0.3, 0.1, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)},
                        hand_r={"frame": "chest", "at": (0.3, -0.1, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)}))
    eye = 0.5 * (rig.eye(st, "l") + rig.eye(st, "r"))
    # the binoculars: lowered, lying across the front of the rim under his hands; raised, at his eyes
    Rd = Rz(0.4 * yaw)                   # (lying under his hands, turned not much more than his chest)
    B_low = M4(Rd @ R_axis((0, 1, 0), -6.0), Rd @ np.array((0.3, 0.0, RIM_Z + 0.070)))
    Rh = Rz(yaw) @ R_axis((0, 1, 0), 3.0)
    B_up = M4(Rh, eye + Rh @ np.array((0.022, 0.0, -0.004)))
    w = sm(raised)
    qa, qb = Matrix(B_low[:3, :3].tolist()).to_quaternion(), Matrix(B_up[:3, :3].tolist()).to_quaternion()
    o = (1 - w) * B_low[:3, 3] + w * B_up[:3, 3] + np.array((0.0, 0.0, 0.06)) * 4 * w * (1 - w)
    B = M4(np.array(qa.slerp(qb, w).to_matrix()), o)
    R = B[:3, :3]
    for s, side in (("l", 1.0), ("r", -1.0)):
        g = o + R @ np.array((0.085, side * BINO["half"], 0.0))
        tdir = -R[:, 0]                  # (the thumbs back toward him under the barrels, the fingers round over them)
        a = _n((1 - w) * (R @ np.array((0.0, -0.45 * side, -0.9))) + w * (R @ np.array((0.0, -0.35 * side, 0.94))))
        a = _n(a - tdir * (tdir @ a))
        n = np.cross(a, tdir) if s == "l" else np.cross(tdir, a)
        if w < 0.5:
            pass
        e_low = Rz(0.3 * yaw) @ np.array((0.1, side * 0.33, RIM_Z + 0.050))
        e_up = eye + Rh @ np.array((-0.02, side * 0.24, -0.2))
        e = (1 - w) * e_low + w * e_up
        pole = elbow_pole(rig, st, s, g, e)
        P["hand_" + s] = {"at": tuple(g), "a": tuple(a), "n": tuple(n), "pole_world": tuple(pole), "roll": 35.0}
    P["props"] = {"binoculars": B}
    return P


# ---- the half-track's gunner at the pedestal machine gun
MG_GRIPS = {"x": 0.40, "y": 0.047, "z": 1.08}        # the M3's spade grips from his feet (0.27 behind its `gunner` socket)


def mg_hands(P, jolt=0.0, k=0.0):
    """Both hands on the spade grips (rods running back from the gun): an overhand grip, the rod across each palm
    from the heel of the hand to the first finger, the thumbs back toward him over the butterfly trigger, the elbows
    a little out."""
    for s, side in (("l", 1.0), ("r", -1.0)):
        g = np.array((MG_GRIPS["x"] - 0.004 * jolt, side * MG_GRIPS["y"], MG_GRIPS["z"] + 0.002 * jolt * k))
        tdir = _n(np.array((-0.9, -0.35 * side, 0.0)))
        a = _n(np.array((0.45, -0.75 * side, -0.5)))
        a = _n(a - tdir * (tdir @ a))
        n = np.cross(a, tdir) if s == "l" else np.cross(tdir, a)
        P["hand_" + s] = {"at": tuple(g), "a": tuple(a), "n": tuple(n), "pole_world": (-0.7, 1.0 * side, -0.7)}
        P["fingers_" + s], P["thumb_" + s] = 66.0, 25.0
    return P


def mg_body(t, long):
    w = wave(t, long, 1, 0.1)
    br = wave(t, long, 2, 0.4)
    P = {"pelvis_at": (0.0, 0.012 * w, -0.03 - 0.004 * abs(w)), "pelvis": (8.0, -1.5 * w, -2.0), "spine_01": (4.0, 0.8 * w, 0.0),
         "spine_02": (5.0 - 0.4 * br, 0.0, 0.5), "chest": (6.0 - 0.8 * br, 0.4 * w, 1.0), "neck": (-4.0, 0.0, 0.0), "head": (-2.0, 0.0, 0.0),
         "clavicle_l": (2.0 + br, 6.0), "clavicle_r": (2.0 + br, 6.0),
         "foot_l": {"at": (0.12, 0.15, MAN["ankle"]), "yaw": 6.0}, "foot_r": {"at": (-0.12, -0.15, MAN["ankle"]), "yaw": -14.0},
         "weapon": stowed(), "weapon_hidden": True}
    return P


@crew_clip("stand_mg", 4.0, loop=True, anchor="feet", weapon="none",
           place=[("vehicle", "M3Halftrack", "gunner", (0.0, 0.0, 0.0)), ("vehicle", "M7", "crew.mg", (0.0, 0.0, 0.0))],
           about="The half-track's gunner stood at the pedestal gun, both hands on its spade grips, thumbs at the trigger, "
                 "looking along the gun and about. Fitted to the M3 (grips 0.40 ahead of his feet and 1.08 up); "
                 "attach him to the part `mg_mount` so he turns with the gun.")
def c_stand_mg(t):
    long = 4.0
    P = mg_body(t, long)
    look = bump(t, 0.8, 1.4, 2.2) - 0.8 * bump(t, 2.5, 3.0, 3.7)
    P["neck"] = add3(P["neck"], (0.0, 0.0, 16.0 * look))
    P["head"] = add3(P["head"], (0.0, 0.0, 18.0 * look))
    return mg_hands(P)


def burst_jolt(t, times, decay=0.03):
    j = 0.0
    for t0 in times:
        if t >= t0:
            u = t - t0
            j += (1 - math.exp(-u / 0.008)) * math.exp(-u / decay)
    return j


MG_ROUNDS = [round(0.075 * k, 3) for k in range(10)]    # the M3's own mg_burst: ten rounds, 0.075 s apart


@crew_clip("stand_mg_burst", 1.0, loop=True, anchor="feet", weapon="none",
           place=[("vehicle", "M3Halftrack", "gunner", (0.0, 0.0, 0.0)), ("vehicle", "M7", "crew.mg", (0.0, 0.0, 0.0))],
           events=[{"t": t0, "fx": "muzzle_flash", "socket": "mg (the vehicle's)", "cue": ""} for t0 in MG_ROUNDS],
           about="The gunner firing: each of the ten rounds of the M3's own `mg_burst` (play them together) jolts his "
                 "shoulders and head, his hands staying on the grips; he leans into the gun.")
def c_stand_mg_burst(t):
    P = mg_body(0.0, 4.0)
    j = burst_jolt(t, MG_ROUNDS)
    on = ramp(t, 0.0, 0.05) * (1 - ramp(t, 0.72, 0.95))
    P["chest"] = add3(P["chest"], (3.0 * on - 2.0 * j, 0.0, 0.4 * j * osc(t, 0.13)))
    P["spine_02"] = add3(P["spine_02"], (2.0 * on - 0.8 * j, 0.0, 0.0))
    P["neck"] = add3(P["neck"], (2.0 * on + 1.5 * j, 0.0, 0.0))
    P["head"] = add3(P["head"], (2.0 * on - 2.5 * j, 0.6 * j * osc(t, 0.11), 0.0))
    P["clavicle_l"] = add3(P["clavicle_l"], (1.2 * j, -2.0 * j))[:2]
    P["clavicle_r"] = add3(P["clavicle_r"], (1.2 * j, -2.0 * j))[:2]
    P["pelvis_at"] = add3(P["pelvis_at"], (-0.004 * j, 0.0, 0.0))
    return mg_hands(P, j, 1.0)


# ================================================================================================ the props
PROP_PAINT = {"black": "2B2A27", "brass": "A7843F", "steel": "54575A", "olive": "4F5338", "fieldgrey": "5C6152", "wood": "7A5634",
              "canvas": "8C8467", "red": "6E2A22"}


def prop_material(name):
    m = bpy.data.materials.get("prop_" + name)
    if m is None:
        rough = 0.35 if name in ("brass", "steel") else 0.7
        metal = 0.8 if name in ("brass", "steel") else 0.0
        m = kit.material("prop_" + name, kit.hex_rgb(PROP_PAINT[name]), rough=rough, metal=metal)
    return m


def _geo_object(g, name, paints=None):
    """A soldier.Geo's pieces as one object, coloured by our prop paints, flat where it is sharp."""
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in g.V], [], [tuple(f) for f in g.F])
    me.validate()
    used = []
    for p in g.P:
        if p not in used:
            used.append(p)
    for p in used:
        me.materials.append(prop_material(p))
    me.polygons.foreach_set("material_index", np.array([used.index(p) for p in g.P], np.int32))
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    S.smooth_shade(ob, 35.0)
    return ob


def build_prop(name):
    """Our own props, each with its origin where crew.json's tracks put it: x along its length, z up."""
    g = S.Geo()
    if name == "binoculars":
        # (origin between the eyepieces; the barrels run forward along x)
        for side in (1.0, -1.0):
            y = side * BINO["half"]
            g.tube((-0.005, y, 0.0), (0.05, y, 0.0), 0.017, 0.019, "black", sides=14)
            g.tube((0.05, y, 0.0), (0.135, y, 0.0), 0.027, 0.029, "black", sides=16)
            g.tube((0.135, y, 0.0), (0.155, y, 0.0), 0.031, 0.031, "black", sides=16)
        g.box((0.03, 0.0, 0.004), (0.02, BINO["half"], 0.008), "black")
        return _geo_object(g, "prop_binoculars", ["black"])
    if name == "flak_round":
        # (origin at the case's base; the round points along +x: case 0.57 m, shell 0.36 m, 88 mm)
        r = 0.044
        g.tube((0.0, 0.0, 0.0), (0.012, 0.0, 0.0), r + 0.007, r + 0.007, "brass", sides=20)
        g.tube((0.012, 0.0, 0.0), (0.5, 0.0, 0.0), r + 0.004, r + 0.002, "brass", sides=20)
        g.tube((0.5, 0.0, 0.0), (0.57, 0.0, 0.0), r + 0.002, r - 0.002, "brass", sides=20)
        g.tube((0.57, 0.0, 0.0), (0.64, 0.0, 0.0), r, r, "steel", sides=20)
        g.tube((0.64, 0.0, 0.0), (0.82, 0.0, 0.0), r, r * 0.62, "olive", sides=20)
        g.tube((0.82, 0.0, 0.0), (0.93, 0.0, 0.0), r * 0.62, 0.008, "red", sides=20)
        return _geo_object(g, "prop_flak_round", ["brass", "steel", "olive", "red"])
    if name == "mortar_bomb":
        # (origin at the tail; 81 mm, 0.33 m long, the fins at the tail)
        r = 0.04
        g.tube((0.0, 0.0, 0.0), (0.09, 0.0, 0.0), 0.012, 0.012, "olive", sides=10)
        for k in range(4):
            a = math.radians(45 + 90 * k)
            g.box((0.04, 0.03 * math.cos(a), 0.03 * math.sin(a)), (0.035, 0.018, 0.002), "olive",
                  R=R_axis((1, 0, 0), math.degrees(a)))
        g.tube((0.09, 0.0, 0.0), (0.13, 0.0, 0.0), 0.014, r, "olive", sides=18)
        g.tube((0.13, 0.0, 0.0), (0.24, 0.0, 0.0), r, r, "olive", sides=18)
        g.tube((0.24, 0.0, 0.0), (0.32, 0.0, 0.0), r, 0.012, "olive", sides=18)
        g.tube((0.32, 0.0, 0.0), (0.335, 0.0, 0.0), 0.012, 0.006, "steel", sides=10)
        return _geo_object(g, "prop_mortar_bomb", ["olive", "steel"])
    if name == "belt":
        # (in the gun's frame: rounds across the belt, bullets toward the muzzle, a steel link round each)
        B = belt_local()
        pitch = 0.0155
        for s in np.arange(0.012, B["length"] - 0.03, pitch):
            k = int(np.clip(np.searchsorted(B["s"], s) - 1, 0, len(B["s"]) - 2))
            u = (s - B["s"][k]) / max(B["s"][k + 1] - B["s"][k], 1e-9)
            p = (1 - u) * B["pts"][k] + u * B["pts"][k + 1]
            d = _n(B["pts"][k + 1] - B["pts"][k])
            x = np.array((1.0, 0.0, 0.0))
            x = _n(x - d * (d @ x))
            g.tube(p - 0.034 * x, p + 0.022 * x, 0.0058, 0.0055, "brass", sides=8)
            g.tube(p + 0.022 * x, p + 0.046 * x, 0.0042, 0.0012, "brass", sides=8)
            g.tube(p - 0.014 * x, p - 0.004 * x, 0.0068, 0.0068, "steel", sides=8, caps=False)
        return _geo_object(g, "prop_belt", ["brass", "steel"])
    if name == "ammo_box":
        # (origin on the ground under its middle; long along x; the lid thrown open back over -y)
        hx, hy, hz = BOX_HALF
        t = 0.004
        g.box((0.0, 0.0, 0.004), (hx, hy, 0.004), "fieldgrey")
        for sx in (1.0, -1.0):
            g.box((sx * (hx - t), 0.0, hz), (t, hy, hz), "fieldgrey")
        for sy in (1.0, -1.0):
            g.box((0.0, sy * (hy - t), hz), (hx, t, hz), "fieldgrey")
        g.box((0.0, hy + 0.012, 2 * hz - 0.03), (hx * 0.9, 0.012, 0.03), "fieldgrey")          # the lid's catch side
        lid = R_axis((1, 0, 0), -100.0)
        g.box(np.array((0.0, -hy, 2 * hz)) + lid @ np.array((0.0, hy, 0.006)), (hx + 0.004, hy + 0.004, 0.006), "fieldgrey", R=lid)
        g.tube((-0.06, 0.0, 2 * hz + 0.01), (0.06, 0.0, 2 * hz + 0.01), 0.006, 0.006, "steel", sides=6)    # handle
        for k in range(7):                                                                       # the belt lying folded in it
            g.box((-hx + 0.03 + 0.034 * k, 0.0, 2 * hz - 0.04), (0.012, hy - 0.012, 0.03), "brass")
        return _geo_object(g, "prop_ammo_box", ["fieldgrey", "steel", "brass"])
    if name == "flak_crate":
        # (origin on the ground under its middle; long along x; open, four rounds lying in it)
        hx, hy, hz = 0.5, 0.2, 0.12
        t = 0.012
        g.box((0.0, 0.0, 0.01), (hx, hy, 0.01), "wood")
        for sx in (1.0, -1.0):
            g.box((sx * (hx - t), 0.0, hz), (t, hy, hz), "wood")
        for sy in (1.0, -1.0):
            g.box((0.0, sy * (hy - t), hz), (hx, t, hz), "wood")
        for k in range(4):
            y = -0.135 + 0.09 * k
            g.tube((-0.465, y, 0.07), (0.105, y, 0.07), 0.05, 0.046, "brass", sides=14)
            g.tube((0.105, y, 0.07), (0.355, y, 0.07), 0.044, 0.04, "olive", sides=14)
            g.tube((0.355, y, 0.07), (0.465, y, 0.07), 0.04, 0.008, "red", sides=14)
        return _geo_object(g, "prop_flak_crate", ["wood", "brass", "olive", "red"])
    raise KeyError(name)


# ================================================================================================ staging and checking
def setup(nation="GER", weapon="rifle"):
    """The pattern man the clips are measured on (kept out of sight below the ground)."""
    m = load_man(nation, weapon)
    measure_man(m["rig"])
    rifle_measures(m)
    m["skin"] = Skin(m)
    MODEL.update(rig=m["rig"], skin=m["skin"], man=m)
    return m


def place_matrix(kind, piece, socket, offset, socks=None):
    """Where a clip's root goes for a socket: the socket's point and facing, moved by the clip's offset (forward, right,
    up of the socket's facing, the game's way)."""
    s = (socks or sockets_of(kind, piece))[socket]
    pl = Place(s["at"], s["facing"])
    off = g2b(offset)
    return M4(pl.R, pl.at + pl.R @ off)


def man_at(m, clip, t, Mw, props=True):
    """A clone of the man in a clip's pose at time t, his root at the world matrix Mw; his props with him."""
    c = CREW[clip]
    cm = clone(m, with_weapon=c["weapon"] != "none")
    P = c["fn"](t)
    st = cm["rig"].solve(P)
    cm["rig"].key(st, 0)
    cm["arm"].matrix_world = Matrix(np.asarray(Mw).tolist())
    cm["P"], cm["st"], cm["props"] = P, st, []
    if props:
        for name, v in P.get("props", {}).items():
            M, shown = v if isinstance(v, tuple) else (v, True)
            if not shown:
                continue
            ob = build_prop(name)
            ob.matrix_world = Matrix((np.asarray(Mw) @ M).tolist())
            cm["props"].append(ob)
    return cm


def grip_errors(rig, st, P):
    """How far each hand's grip point is from where the pose asked for it (m)."""
    out = {}
    for s in ("l", "r"):
        h = P.get("hand_" + s, {})
        if "at" in h and "frame" not in h:
            out[s] = float(np.linalg.norm(rig.at(st, "hand_" + s, S.grip_point(rig.J, s)) - np.asarray(h["at"], float)))
    return out


RAYS = [Vector(v).normalized() for v in ((0.31, 0.17, 0.93), (-0.62, 0.71, -0.33), (0.45, -0.83, 0.12))]


def inside(tree, p, far=6.0):
    """A point inside a closed surface: an odd count of crossings along most of three rays (thin parts whose
    windings are turned do not fool it, as the nearest face's facing would)."""
    votes = 0
    for d in RAYS:
        q, k = Vector(p), 0
        for _ in range(40):
            hit = tree.ray_cast(q, d, far)
            if hit[0] is None:
                break
            k += 1
            q = hit[0] + d * 1e-4
        votes += k % 2
    return votes >= 2


def hatch_discs(centre=(-0.38, 0.0), r=0.3):
    """The PanzerIV's open hatch is drawn as painted discs at the rim (black over shade): not solid, left out."""
    def drop(c, mat):
        return mat.endswith((":black", ":shade")) and math.hypot(c[0] - centre[0], c[1] - centre[1]) < r and c[2] > 2.5
    return drop


def cupola_gaps(wp, centre=(-0.38, 0.0), rim=2.625, inner=((2.29, 2.58, 0.345), (2.58, 2.63, 0.282)), outer=0.345):
    """How far a man's points (world) go into the PanzerIV's cupola: through its inner wall below the rim, or down
    into the rim's top. Returns (deepest, its point)."""
    r = np.hypot(wp[:, 0] - centre[0], wp[:, 1] - centre[1])
    z = wp[:, 2]
    worst, at = 0.0, None
    for z0, z1, ri in inner:
        k = (z >= z0) & (z < z1) & (r > ri) & (r < outer)
        if k.any():
            d = np.minimum(np.minimum(r[k] - ri, outer - r[k]), rim - z[k])
            i = int(np.argmax(d))
            if d[i] > worst:
                worst, at = float(d[i]), wp[k][i]
    return worst, at


def check(m, clip, Mw, tree, bm=None, ignore=None, times=None, tol=0.012, keep_point=None):
    """A clip played at a place, measured against the piece: the deepest the man goes into it (m, and which bone) and
    the worst gap between a hand and its grip, over the clip's frames (every 6th)."""
    c = CREW[clip]
    rig, skin = MODEL["rig"], MODEL["skin"]
    N = int(round(c["long"] * FPS))
    times = times if times is not None else [f / FPS for f in range(0, N + 1, 6)]
    Mw = np.asarray(Mw)
    worst, worst_at, gworst, count = 0.0, None, 0.0, 0
    mats = None
    for t in times:
        P = c["fn"](t)
        st = rig.solve(P)
        pts, bones = skin.points(st)
        wp = pts @ Mw[:3, :3].T + Mw[:3, 3]
        if keep_point is not None:
            k = keep_point(wp)
            wp, bones = wp[k], bones[k]
        for p, b in zip(wp, bones):
            loc, nrm, idx, dist = tree.find_nearest(Vector(p), 0.1)
            if loc is None or (Vector(p) - loc).dot(nrm) >= 0.0 and dist > 0.004:
                continue
            if ignore is not None and ignore(np.array(loc), idx):
                continue
            if not inside(tree, p):
                continue
            if dist > tol:
                count += 1
            if dist > worst:
                worst, worst_at = dist, (round(t, 3), str(b), tuple(np.round(np.linalg.inv(Mw) @ np.append(p, 1.0), 3)[:3]))
        ge = grip_errors(rig, st, P)
        gworst = max([gworst] + list(ge.values()))
    return {"deepest": round(worst, 4), "where": worst_at, "points_past_tol": count, "grip_gap": round(gworst, 4)}


# ================================================================================================ the clips at the works
def yawed(P, yaw, at):
    """A pose said in a man's own frame moved to another place in the clip's: turned by `yaw` degrees about his root
    and set down at `at` (the pair clips share one root)."""
    R = Rz(yaw)
    at = np.asarray(at, float)
    Q = dict(P)
    pa = np.asarray(P.get("pelvis_at", (0, 0, 0)), float)
    p0 = MAN["pelvis"]
    Q["pelvis_at"] = tuple(R @ (p0 + pa) + at - p0)
    pv = P.get("pelvis", (0, 0, 0))
    Q["pelvis"] = (pv[0], pv[1], pv[2] + yaw)
    for s in ("l", "r"):
        f = dict(P["foot_" + s])
        f["at"] = tuple(R @ np.asarray(f["at"], float) + at)
        f["yaw"] = f.get("yaw", 0.0) + yaw
        if "pole" in f:
            f["pole"] = tuple(R @ np.asarray(f["pole"], float))
        Q["foot_" + s] = f
        h = P.get("hand_" + s)
        if h is not None:
            Q["hand_" + s] = _moved_hand(h, R, at)
    W = P.get("weapon")
    if W is not None and not callable(W):
        Q["weapon"] = M4(R, at) @ W
    return Q


def _moved_hand(h, R, at):
    """A hand's target turned by R and moved by `at` with the man's root (one held on his own body only its pole)."""
    if "blend" in h:
        return {"blend": [(w, _moved_hand(hh, R, at)) for w, hh in h["blend"]]}
    h = dict(h)
    if "frame" not in h and "grip" not in h:
        h["at"] = tuple(R @ np.asarray(h["at"], float) + at)
        h["a"], h["n"] = tuple(R @ np.asarray(h["a"], float)), tuple(R @ np.asarray(h["n"], float))
    if "pole_world" in h:
        h["pole_world"] = tuple(R @ np.asarray(h["pole_world"], float))
    return h


def reframed(P, A, B):
    """A pose made in a man's frame A (a Place in the piece) said in frame B (the socket the game attaches him at):
    the same moves in the piece, his root now at B. His props go with him."""
    yaw, at = A.yaw - B.yaw, B.local(A.at)
    Q = yawed(P, yaw, at)
    if "props" in P:
        T = M4(Rz(yaw), at)
        Q["props"] = {k: ((T @ v[0], v[1]) if isinstance(v, tuple) else T @ v) for k, v in P["props"].items()}
    return Q


def lmg_place(t):
    """The gunner's gun (its frame in the pair's root frame) at t, from his own pose."""
    P = CREW["mg_pair_gunner"]["fn"](t)
    st = MODEL["rig"].solve(P)
    return st["W"]


MG_PAIR_ROUNDS = [round(0.1 + 0.1 * k, 2) for k in range(8)]


@crew_clip("mg_pair_gunner", 2.4, loop=True, anchor="feet", weapon="lmg",
           events=[{"t": t0, "fx": "muzzle_flash", "socket": "socket_muzzle", "cue": "lmg_burst" if k == 0 else ""}
                   for k, t0 in enumerate(MG_PAIR_ROUNDS)],
           about="The machine-gunner lying behind his gun on its bipod: a burst of eight, then he holds his aim and glances "
                 "to his number two. Root on the ground under his hips (the pair's root: play mg_pair_feeder on the same "
                 "root). Play it on soldier_GER_lmg.")
def c_mg_pair_gunner(t):
    if t < 1.2:
        P = S.c_lmg_burst(t)
    else:
        P = S.c_lmg_burst(1.15)
        u = t - 1.2
        g = bump(u, 0.15, 0.45, 0.85)
        P["neck"] = add3(P["neck"], (2.0 * g, 0.0, 12.0 * g))
        P["head"] = add3(P["head"], (4.0 * g, 3.0 * g, 16.0 * g))
        P = S.breathe(P, t, 1.2, 0.3)
    P["weapon_kind"] = "lmg"
    # (his left hand as soldier.c_lmg_burst has it now: under the butt's small, pulling it into the shoulder, not on
    # its top, where the left arm turned past its range; the elbow held to its pole, else the arm hops between two
    # easy places as the gun shakes)
    P["hand_l"] = dict(P["hand_l"], pole_weight=0.03)
    return grounded("mg_pair_gunner", P)


BELT = {}                                                # the belt's line in the gun's own frame (made once)
BOX_AT, BOX_YAW = (0.98, 0.50, 0.0), -25.0                # the ammunition box on the ground (the pair's frame)
BOX_HALF = (0.13, 0.05, 0.085)                           # its half-sizes (long, wide, high)


def belt_local():
    """The belt from the gun's feed (the end of the box on its left side) sagging down into the ammunition box, as
    points in the gun's frame at rest (the belt is a rigid prop on the gun: it shivers with it as it fires)."""
    if "pts" not in BELT:
        W = lmg_place(0.0)
        feed = W[:3, 3] + W[:3, :3] @ np.array((0.064, 0.158, 0.034))
        Rb = Rz(BOX_YAW)
        mouth = np.asarray(BOX_AT, float) + Rb @ np.array((0.0, 0.0, 2 * BOX_HALF[2] + 0.004))
        into = mouth + np.array((0.0, 0.0, -0.07))
        side = feed + W[:3, :3] @ np.array((0.0, 0.07, -0.01))           # (out of the feed sideways first)
        ctrl = [feed, side, 0.5 * (side + mouth) + np.array((0.0, 0.0, -0.05)), mouth + np.array((0.0, 0.0, 0.07)), mouth, into]
        dense = []
        for i in range(len(ctrl) - 1):                                     # (a Catmull-Rom line through them)
            p0, p1, p2, p3 = ctrl[max(i - 1, 0)], ctrl[i], ctrl[i + 1], ctrl[min(i + 2, len(ctrl) - 1)]
            for u in np.linspace(0.0, 1.0, 24, endpoint=False):
                dense.append(0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (-p0 + 3 * p1 - 3 * p2 + p3) * u ** 3))
        dense.append(ctrl[-1])
        dense = np.array(dense)
        seg = np.linalg.norm(np.diff(dense, axis=0), axis=1)
        s_at = np.concatenate([[0.0], np.cumsum(seg)])
        Wi = np.linalg.inv(W)
        BELT["pts"] = dense @ Wi[:3, :3].T + Wi[:3, 3]
        BELT["s"] = s_at
        BELT["length"] = float(s_at[-1])
    return BELT


def belt_at(W, s):
    """A point of the belt `s` metres from the feed, and the belt's way there (the pair's frame, the gun at W)."""
    B = belt_local()
    s = float(np.clip(s, 0.0, B["length"]))
    k = int(np.clip(np.searchsorted(B["s"], s) - 1, 0, len(B["s"]) - 2))
    u = (s - B["s"][k]) / max(B["s"][k + 1] - B["s"][k], 1e-9)
    p = (1 - u) * B["pts"][k] + u * B["pts"][k + 1]
    d = B["pts"][k + 1] - B["pts"][k]
    return W[:3, :3] @ p + W[:3, 3], _n(W[:3, :3] @ d)


@crew_clip("mg_pair_feeder", 2.4, loop=True, anchor="feet", weapon="rifle",
           about="The number two lying at the gunner's left, turned in toward the gun, up on his left elbow: his right hand "
                 "lifts the belt into the gun's left side while it fires, his left hand draws it up from the box; between "
                 "bursts he watches the fall of shot. Same root as mg_pair_gunner (his body 0.6 m to the gunner's left); "
                 "his rifle lies by his left side; the belt and its box are props (crew.json).")
def c_mg_pair_feeder(t):
    on = ramp(t, 0.05, 0.12) * (1 - ramp(t, 0.8, 0.95))
    jig = 0.004 * on * osc(t, 0.1)
    look = bump(t, 1.1, 1.5, 2.2)
    P = {"pelvis_at": (0.0, 0.0, S.PRONE_Z + 0.04), "pelvis": (82.0, 0.0, 0.0), "spine_01": (-6.0, -4.0, -3.0),
         "spine_02": (-10.0, -5.0, -5.0), "chest": (-12.0, -6.0, -6.0), "neck": (-28.0 - 5.0 * look, -4.0, -6.0 + 14.0 * look),
         "head": (-18.0 - 4.0 * look, -10.0, -8.0 + 18.0 * look), "clavicle_l": (10.0, 10.0), "clavicle_r": (6.0, 16.0),
         "foot_l": {"at": (-0.82, 0.32, 0.125), "yaw": 26.0, "pitch": 132.0, "roll": -12.0, "toe": 12.0, "pole": (-0.1, 0.3, -1.0)},
         "foot_r": {"at": (-0.8, -0.22, 0.125), "yaw": -20.0, "pitch": 132.0, "roll": 12.0, "toe": 12.0, "pole": (-0.1, -0.3, -1.0)},
         "fingers_l": 50.0, "fingers_r": 55.0, "thumb_l": 35.0, "thumb_r": 35.0}
    yaw, at = -25.0, np.array((0.28, 0.82, 0.0))
    W = lmg_place(t)
    Rw = Rz(yaw)
    # (the hands said in the pair's frame: the right under the belt near the gun, lifting it level with the feed;
    # the left on it nearer the box, drawing it up)
    hands = {}
    # (each hand where the belt runs across its fingers with the wrist straight: the right 0.15 m along it from the
    # feed, the left 0.54 m, nearer the box: the gun sits higher in the gunner's shoulder since the aim's head fix)
    for s, along, lift in (("r", 0.15, 0.004 * on * osc(t, 0.1)), ("l", 0.54, 0.012 * on * osc(t, 0.2, 0.3))):
        g, d = belt_at(W, along)
        side = 1.0 if s == "l" else -1.0
        tdir = -d if s == "r" else d                                     # (the thumbs along the belt, outward: palms up)
        n = np.array((0.0, 0.0, 1.0))                                    # (the palm up under it)
        a = _n(np.cross(n, tdir) * (-side))
        n = _n(n - a * (a @ n) - tdir * (tdir @ n))
        a = _n(np.cross(tdir, n)) if s == "l" else _n(np.cross(n, tdir))
        g = g + np.array((0.0, 0.0, -0.024 + lift))
        pole = Rw @ (np.array((-0.4, -0.6, -1.0)) if s == "r" else np.array((-0.6, 0.5, -1.0)))
        hands[s] = {"at": tuple(g), "a": tuple(a), "n": tuple(n), "pole_world": tuple(pole)}
    hand_r, hand_l = hands["r"], hands["l"]
    rifle = M4(R_axis((0, 0, 1), 8.0) @ R_axis((1, 0, 0), 90.0), (-0.1, 0.36, 0.02))
    P["weapon"] = rifle
    Q = yawed(grounded("mg_pair_feeder", P), yaw, at)
    Q["hand_r"], Q["hand_l"] = hand_r, hand_l
    Q["props"] = {"ammo_box": M4(Rz(BOX_YAW), BOX_AT), "belt": W}
    return Q


# ---- the mortar's crew: number one at the sight, number two dropping the bomb, number three passing bombs
FORT_SOCKS = {}


def fsock(piece, name):
    if piece not in FORT_SOCKS:
        FORT_SOCKS[piece] = sockets_of("fort", piece)
    return FORT_SOCKS[piece][name]


def crew_place(piece, crew):
    s = fsock(piece, crew)
    return Place(s["at"], s["facing"])


MORTAR = {}
MORTAR_OFF = {"number3": (0.0, -0.1, 0.0)}             # (number three moved 0.1 to his left: his socket puts his thigh in the box)
MORTAR_T = {"release": 1.30, "fire": 1.62, "hand_over": 0.25, "take": 2.45, "long": 3.2}


def mortar():
    """The Mortar's measures in its own frame (Blender's axes): the bore, the muzzle, the sight, number two's grips,
    the hand-over point between number three and number two, a free place in the bomb box; and the crew's places."""
    if not MORTAR:
        g = lambda n: np.asarray(fsock("Mortar", n)["at"], float)
        MORTAR["muzzle"] = g("muzzle")
        MORTAR["bore"] = _n(np.asarray(fsock("Mortar", "muzzle")["facing"], float))
        MORTAR["sight"] = g("sight")
        MORTAR["grip_l"], MORTAR["grip_r"] = g("grip.number2.l"), g("grip.number2.r")
        MORTAR["hand_over"] = g2b((-0.17, 0.44, 0.60))
        MORTAR["slot"] = g2b((-0.36, 0.405, 0.176))                     # (a free place in the box, the bombs' tails' height)
        MORTAR["shaft_l"] = g2b((0.464, -0.172, 0.752))                  # (the left end of the traverse shaft)
        for k in ("number1", "number2", "number3"):
            pl = crew_place("Mortar", "crew." + k)
            MORTAR[k] = Place(pl.at + pl.R @ g2b(MORTAR_OFF.get(k, (0, 0, 0))), pl.R[:, 0])
    return MORTAR


def bomb_frame(origin, axis, up=(0.0, 0.0, 1.0)):
    x = _n(axis)
    hint = np.asarray(up, float)
    if abs(x @ _n(hint)) > 0.95:
        hint = np.array((1.0, 0.0, 0.0))
    return M4(frame_from(x, hint), np.asarray(origin, float))


def slerp_frames(keys, t):
    """A frame passed smoothly through keyed frames [(t, M)]: the origin by Catmull-Rom, the turn by slerp (eased)."""
    times = [k[0] for k in keys]
    if t <= times[0]:
        return keys[0][1]
    if t >= times[-1]:
        return keys[-1][1]
    i = max(j for j in range(len(times) - 1) if times[j] <= t)
    u = (t - times[i]) / (times[i + 1] - times[i])
    P = [k[1][:3, 3] for k in keys]
    p0, p1, p2, p3 = P[max(i - 1, 0)], P[i], P[i + 1], P[min(i + 2, len(P) - 1)]
    pos = 0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (-p0 + 3 * p1 - 3 * p2 + p3) * u ** 3)
    qa = Matrix(keys[i][1][:3, :3].tolist()).to_quaternion()
    qb = Matrix(keys[i + 1][1][:3, :3].tolist()).to_quaternion()
    if qa.dot(qb) < 0:
        qb.negate()
    R = np.array(qa.slerp(qb, sm(u)).to_matrix())
    return M4(R, pos)


def pl2_toward(d):
    """`d` from the mortar's bore toward number two (the bomb is brought in from his side)."""
    M = mortar()
    v = M["number2"].at - M["muzzle"]
    v = v - M["bore"] * (M["bore"] @ v)
    v[2] = 0.0
    return d * _n(v)


def mortar_bomb2(t):
    """Number two's bomb (the Mortar's frame) and whether it shows: taken at the hand-over, swung round to the muzzle,
    its tail let into the muzzle and held, let go at `release`, sliding down the bore."""
    M = mortar()
    T = MORTAR_T
    mz, bore = M["muzzle"], M["bore"]
    across = _n(np.cross((0.0, 0.0, 1.0), M["hand_over"] - M["number2"].at))
    at_hand = bomb_frame(M["hand_over"] - 0.167 * across, across)
    front = M["number2"].world((0.30, -0.02, 0.86))
    k = [(0.0, at_hand), (T["hand_over"], at_hand),
         (0.62, bomb_frame(front, M["number2"].world((0.25, -0.1, 0.0)) - M["number2"].at + np.array((0.0, 0.0, 0.9)))),
         (1.03, bomb_frame(mz - 0.01 * bore + pl2_toward(0.012), bore)),
         (1.14, bomb_frame(mz - 0.09 * bore, bore)), (T["release"], bomb_frame(mz - 0.09 * bore, bore))]
    if t < T["release"]:
        return slerp_frames(k, t), t >= T["hand_over"]
    if t - T["release"] > 0.22:                                   # (gone down the bore: the next one is at the hand-over)
        return at_hand, False
    d = 0.5 * 18.6 * (t - T["release"]) ** 2
    return bomb_frame(mz - (0.09 + d) * bore, bore), True


def mortar_bomb3(t):
    """Number three's bomb: lifted from the box, swung up crosswise to the hand-over, held there for number two."""
    M = mortar()
    T = MORTAR_T
    across = _n(np.cross((0.0, 0.0, 1.0), M["hand_over"] - M["number2"].at))
    at_hand = bomb_frame(M["hand_over"] - 0.167 * across, across)
    in_box = bomb_frame(M["slot"], (0.0, 0.0, 1.0), up=(1.0, 0.0, 0.0))
    lifted = bomb_frame(M["slot"] + np.array((0.04, 0.0, 0.36)), (0.25, 0.0, 1.0), up=(1.0, 0.0, 0.0))
    k = [(T["take"], in_box), (2.62, lifted), (2.95, at_hand), (T["long"], at_hand)]
    if t >= T["take"]:
        return slerp_frames(k, t), True
    return at_hand, t < T["hand_over"] + 0.02


def on_rod(s, M, x, r, toward, thumb=1.0, pole=None):
    """A hand round a rod (a bomb, a tube: its frame M, x along it) at x, from the side toward `toward`: the palm on
    the rod, the thumb along it (thumb=-1: the other way), the grip point r out from the rod's line."""
    ax = M[:3, 0]
    p = M[:3, 3] + ax * x
    rad = np.asarray(toward, float) - p
    rad = _n(rad - ax * (ax @ rad))
    g = p + rad * r
    n = -rad
    tdir = ax * thumb
    a = np.cross(tdir, n) if s == "l" else np.cross(n, tdir)
    h = {"at": tuple(g), "a": tuple(_n(a)), "n": tuple(n), "rod_r": r}
    if pole is not None:
        h["pole_world"] = tuple(pole)
    return h


def to_local(pl, h):
    """A hand said in the piece's frame, said in a man's (his place pl)."""
    out = dict(h)
    out["at"] = tuple(pl.local(h["at"]))
    for k in ("a", "n", "pole_world"):
        if k in h:
            out[k] = tuple(pl.ldir(h[k]))
    return out


KNEEL_SINK = 0.04       # (m) the hips let down so the right knee rests on the ground


def kneel_grounded(P):
    """soldier.kneel_base leaves the right boot's toes about 10 cm in the ground and the knee 4 cm over it: the foot
    stood up on its tucked toes (ankle 0.19 m up, the toes bent flat) and the hips let down KNEEL_SINK, so the boot
    is on the ground and the knee about 1 cm off it."""
    P = dict(P)
    f = P["foot_r"]
    P["foot_r"] = dict(f, at=(f["at"][0], f["at"][1], 0.19), pitch=76.0, toe=80.0)
    pa = P["pelvis_at"]
    P["pelvis_at"] = (pa[0] - 0.25 * KNEEL_SINK, pa[1], pa[2] - KNEEL_SINK)
    return P


def kneel_body(bend=0.0, turn=0.0, side=0.0, drop=0.0, neck=(0.0, 0.0), sway=0.0, fwd=0.0):
    """Kneeling on the right knee, the left foot forward (soldier.kneel_base, his rifle slung): bend him forward from
    the hips, turn his shoulders (+ to his left), lean him to his left, sit him `drop` lower; neck (pitch down, turn
    left)."""
    P = kneel_grounded(S.kneel_base(aimed=False))
    P["weapon"], P["weapon_hidden"] = stowed(), True
    pa = P["pelvis_at"]
    P["pelvis_at"] = (pa[0] - 0.25 * drop + fwd, pa[1], pa[2] - drop)
    P["pelvis"] = add3(P["pelvis"], (0.3 * bend, 0.2 * side, 0.25 * turn))
    P["spine_01"] = add3(P.get("spine_01", (0, 0, 0)), (0.25 * bend, 0.25 * side, 0.25 * turn))
    P["spine_02"] = add3(P.get("spine_02", (0, 0, 0)), (0.25 * bend, 0.3 * side, 0.25 * turn))
    P["chest"] = add3(P.get("chest", (0, 0, 0)), (0.2 * bend + 0.6 * sway, 0.25 * side, 0.25 * turn))
    P["neck"] = (-0.3 * bend + 0.45 * neck[0], 0.0, 0.45 * neck[1])
    P["head"] = (-0.2 * bend + 0.55 * neck[0], 0.0, 0.55 * neck[1])
    P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 55.0, 55.0, 35.0, 35.0
    return P


def on_knees(rig, P):
    """His hands resting: the left on the left knee (the one up), the right on the right thigh."""
    Q = dict(P, hand_l={"frame": "chest", "at": (0.3, 0.15, -0.35), "a": (1, 0, 0), "n": (0, 0, -1)},
             hand_r={"frame": "chest", "at": (0.3, -0.15, -0.35), "a": (1, 0, 0), "n": (0, 0, -1)})
    st = rig.solve(Q)
    kl = st["H"]["calf_l"]
    tl = _n(st["H"]["calf_l"] - st["H"]["thigh_l"])
    hip_r, kr = st["H"]["thigh_r"], st["H"]["calf_r"]
    th = 0.45 * hip_r + 0.55 * kr
    hl = {"at": tuple(kl - 0.06 * tl + np.array((0.0, 0.0, 0.085))), "a": tuple(_n(tl + np.array((0.0, 0.0, -0.6)))),
          "n": (0.0, 0.0, -1.0), "pole_world": (-0.2, 1.0, -0.4)}
    hr = {"at": tuple(th + np.array((0.0, -0.01, 0.085))), "a": tuple(_n(kr - hip_r + np.array((0.0, 0.0, -0.4)))),
          "n": (0.0, 0.0, -1.0), "pole_world": (-0.2, -1.0, -0.4)}
    return hl, hr


def blend_hand(pairs):
    pairs = [(w, h) for w, h in pairs if w > 1e-4]
    if len(pairs) == 1:
        return pairs[0][1]
    return {"blend": pairs}


@crew_clip("mortar_number2", 3.2, loop=True, anchor="feet", weapon="none",
           place=[("fort", "Mortar", "crew.number2", (0, 0, 0)), ("fort", "MortarPit", "crew.number2", (0, 0, 0))],
           events=[{"t": 0.25, "fx": "", "cue": "bomb_taken"}, {"t": 1.30, "fx": "", "cue": "mortar_drop"},
                   {"t": 1.62, "fx": "piece_clip", "clip": "fire", "cue": "mortar_fire (play the Mortar's own `fire` clip)"}],
           about="Number two, kneeling at the mortar's right front: takes a bomb from number three, swings it round and "
                 "lets its tail into the muzzle, his hands at grip.number2.l/r; lets it go, ducks away from the muzzle "
                 "and the blast, and turns back for the next. The bomb is a prop (crew.json); the Mortar's own fire "
                 "clip goes at 1.62. Play mortar_number1/2/3 together, each at its own socket.")
def c_mortar_number2(t):
    M = mortar()
    T = MORTAR_T
    pl = M["number2"]
    rig = MODEL["rig"]
    free = (1, 1, 1, 1, 1, 0)

    def dress(t, P):
        B, shown = mortar_bomb2(t)
        me = pl.world((0.0, 0.0, 1.0))
        hold = {"l": on_rod("l", B, 0.16, 0.062, me, pole=pl.R @ np.array((-0.3, 1.0, -1.0))),
                "r": on_rod("r", B, 0.08, 0.06, me, pole=pl.R @ np.array((-0.3, -1.0, -1.0)))}
        tube = bomb_frame(M["muzzle"], M["bore"])
        Bl = mortar_bomb2(T["release"])[0] if t > T["release"] else B       # (let go at the muzzle: not down the bore with it)
        at_muzzle = {"l": on_rod("l", Bl, 0.105 + 0.0, 0.062, M["grip_l"] + 0.5 * (M["grip_l"] - me), pole=pl.R @ np.array((-0.3, 1.0, -0.8))),
                     "r": on_rod("r", tube, -0.041, 0.06, M["grip_r"] + 0.5 * (M["grip_r"] - me), pole=pl.R @ np.array((-0.3, -1.0, -0.8)))}
        hl_k, hr_k = on_knees(rig, P)
        rest = {"l": hl_k, "r": hr_k}
        down = ramp(t, T["release"] + 0.02, T["release"] + 0.45)          # (let go: hands from the muzzle to his knees)
        w_muzzle = ramp(t, 0.93, 1.22) * (1 - down)
        w_rest = down * (1 - ramp(t, 2.6, 3.15))
        w_hold = max(0.0, 1.0 - w_muzzle - w_rest)
        for s in ("l", "r"):
            hb = to_local(pl, hold[s])
            hm = to_local(pl, at_muzzle[s])
            P["hand_" + s] = blend_hand([(w_hold, hb), (w_muzzle, hm), (w_rest, rest[s])])
            P["fingers_" + s] = 55.0 + 10.0 * (w_hold + w_muzzle) - 25.0 * ramp(t, T["release"], T["release"] + 0.08) * (1 - w_rest)
        P["props"] = {"mortar_bomb": (np.linalg.inv(pl.matrix()) @ B, shown)}
        return P

    reach = kneel_body(bend=38.0, turn=64.0, side=14.0, fwd=0.05, drop=0.0, neck=(2.0, 18.0))
    carry = kneel_body(bend=10.0, turn=8.0, fwd=0.05, neck=(18.0, -6.0))
    load = kneel_body(bend=32.0, turn=6.0, side=-6.0, fwd=0.24, drop=-0.08, neck=(-6.0, -20.0))
    duck = kneel_body(bend=26.0, turn=55.0, side=14.0, drop=0.08, fwd=-0.05, neck=(30.0, 40.0))
    keys = [(0.0, reach), (T["hand_over"], reach), (0.6, carry), (0.86, load), (T["release"], load), (1.62, duck), (2.2, duck),
            (2.7, kneel_body(bend=18.0, turn=48.0, side=6.0, neck=(6.0, 26.0)))]
    keys = reached_keys("mortar_number2", keys, dress, free, long=T["long"])
    P = looped(keys, T["long"])(t)
    P = S.breathe(P, t, 1.6, 0.4)
    return reach_fix(dress(t, P), free=free)


KNEEL_PARAMS = ("bend", "turn", "side", "fwd", "drop", "np", "ny")
KNEEL_SCALE = np.array((10.0, 10.0, 10.0, 0.1, 0.1, 10.0, 10.0))
KNEEL_LO = np.array((-10.0, -80.0, -25.0, -0.1, -0.12, -40.0, -80.0))
KNEEL_HI = np.array((60.0, 80.0, 25.0, 0.3, 0.25, 50.0, 80.0))


def kneel_from(x):
    return kneel_body(bend=x[0], turn=x[1], side=x[2], fwd=x[3], drop=x[4], neck=(x[5], x[6]))


def fit_eye(eye_at, gaze, x0, eye="r", iters=12):
    """Kneeling with an eye at a point and looking along `gaze` (his frame): the body's bend, turn and lean, the
    pelvis's height and the neck fitted by damped least squares."""
    rig = MODEL["rig"]
    gaze = _n(gaze)

    def res(x):
        P = kneel_from(x)
        P["hand_l"] = {"frame": "chest", "at": (0.3, 0.1, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)}
        P["hand_r"] = {"frame": "chest", "at": (0.3, -0.1, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)}
        st = rig.solve(P)
        e = rig.eye(st, eye)
        f = st["D"]["head"] @ np.array((1.0, 0.0, 0.0))
        return np.concatenate([(e - eye_at) * 10.0, (f - gaze) * 0.6, (x - x0) / KNEEL_SCALE * 0.02])
    x = np.asarray(x0, float).copy()
    for _ in range(iters):
        r = res(x)
        J = np.zeros((len(r), len(x)))
        for i in range(len(x)):
            dx = np.zeros(len(x))
            dx[i] = KNEEL_SCALE[i] * 0.02
            J[:, i] = (res(x + dx) - r) / dx[i]
        step = np.linalg.solve(J.T @ J + 1e-3 * np.diag(1.0 / KNEEL_SCALE ** 2), -J.T @ r)
        x = np.clip(x + step, KNEEL_LO, KNEEL_HI)
    return x, float(np.linalg.norm(res(x)[:3]) / 10.0)


def knob_hand(s, rig, st, g, pole_side=1.0):
    sh = st["H"]["upperarm_" + s]
    a, n = default_hand(s, sh, g)
    side = 1.0 if s == "l" else -1.0
    return hand_on(g, a, n, (-0.3, 0.9 * side, -1.0))


@crew_clip("mortar_number1", 3.2, loop=True, anchor="feet", weapon="none",
           place=[("fort", "Mortar", "crew.number1", (0, 0, 0)), ("fort", "MortarPit", "crew.number1", (-0.029, 0.022, 0.0))],
           about="Number one, kneeling on the mortar's left: his right eye at the sight (socket `sight`), his left hand on "
                 "the traverse shaft's end and his right on the sight's knob, laying it; as number two lets the bomb go he "
                 "pulls his head back, ducks away from the muzzle with his hands to his knees, then puts his eye back to "
                 "the sight. Play with mortar_number2 (and 3) from the same moment.")
def c_mortar_number1(t):
    M = mortar()
    T = MORTAR_T
    pl = M["number1"]
    rig = MODEL["rig"]
    if "n1_sight" not in M:
        eye_at = pl.local(M["sight"] - np.array((0.035, 0.0, 0.0)))
        x, err = fit_eye(eye_at, pl.ldir((1.0, 0.0, -0.05)), (30.0, 35.0, 0.0, 0.05, 0.0, 10.0, 35.0))
        M["n1_sight"], M["n1_err"] = x, err
    xs = M["n1_sight"]
    sight = kneel_from(xs)
    back = kneel_from(xs + np.array((-8.0, -6.0, 0.0, -0.03, 0.0, -6.0, -10.0)))
    duck = kneel_body(bend=30.0, turn=-50.0, side=-12.0, drop=0.03, fwd=-0.03, neck=(40.0, -50.0))
    keys = [(0.0, sight), (1.2, sight), (1.4, back), (1.62, duck), (2.2, duck), (2.62, back), (2.86, sight)]
    st0 = rig.solve(dict(sight, hand_l={"frame": "chest", "at": (0.3, 0.1, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)},
                         hand_r={"frame": "chest", "at": (0.3, -0.1, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)}))
    knob = pl.local(g2b((0.309, -0.06, 0.827)))
    shaft = pl.local(M["shaft_l"]) + np.array((0.0, 0.0, 0.0))
    work = {"l": knob_hand("l", rig, st0, shaft), "r": knob_hand("r", rig, st0, knob)}
    wt = lambda t: ramp(t, 1.15, 1.56) * (1 - ramp(t, 2.45, 2.8))

    def dress(t, P):
        hl, hr = on_knees(rig, P)
        rest = {"l": hl, "r": hr}
        w = wt(t)
        adj = 0.004 * osc(t, 0.9) * (1 - w)                            # (laying: the knob turned a touch)
        for s in ("l", "r"):
            h = dict(work[s])
            if s == "r":
                h["at"] = tuple(np.asarray(h["at"]) + np.array((0.0, adj, 0.0)))
            P["hand_" + s] = blend_hand([(1 - w, h), (w, rest[s])])
            P["fingers_" + s] = 58.0
        return P
    # (off the sight, from 1.2 to 2.8, his body carries his hands to his knees and back (comfy_body); at the sight
    # it is where fit_eye put it)
    if "n1_carry" not in M:
        tks = np.arange(0.0, T["long"] - 1e-6, 0.1)
        X = comfy_body("mortar_number1", [dress(tk, S.keyed(keys)(tk)) for tk in tks], (1, 1, 1, 1, 1, 0),
                       fixed=[i for i, tk in enumerate(tks) if wt(tk) < 0.02])
        M["n1_carry"] = (tks, X)
    tks, X = M["n1_carry"]
    x = np.array([np.interp(t, np.append(tks, T["long"]), np.append(X[:, j], X[0, j])) for j in range(6)])
    return dress(t, _body_delta(S.keyed(keys)(t), x))


@crew_clip("mortar_number3", 3.2, loop=True, anchor="feet", weapon="none",
           place=[("fort", "Mortar", "crew.number3", MORTAR_OFF["number3"])],
           about="Number three, kneeling behind at the bomb box: holds a bomb out crosswise for number two, who takes it at "
                 "0.25; ducks with the others at the shot, then lifts the next bomb out of the box and holds it out again. "
                 "The bomb is a prop (crew.json), shown while it is in his hands.")
def c_mortar_number3(t):
    M = mortar()
    T = MORTAR_T
    pl = M["number3"]
    rig = MODEL["rig"]
    free = (1, 1, 1, 1, 1, 0)

    def dress(t, P):
        B, shown = mortar_bomb3(t)
        # (once he has let the bomb go his hands go next to the one in the box, not back to the hand-over)
        Bh = mortar_bomb3(T["take"])[0] if 1.0 <= t < T["take"] else B
        me = pl.world((0.0, 0.0, 1.0))
        # (which end of the bomb is to his left: the tail as he holds it out; in the box both hands near the nose)
        u = ramp(t, T["take"] + 0.08, 2.85) if t >= T["take"] - 0.3 else 1.0
        xl = 0.21 + (0.07 - 0.21) * u
        xr = 0.31 + (0.25 - 0.31) * u
        hold = {"l": on_rod("l", Bh, xl, 0.062, me, pole=pl.R @ np.array((-0.3, 1.0, -1.0))),
                "r": on_rod("r", Bh, xr, 0.062, me, pole=pl.R @ np.array((-0.3, -1.0, -0.3)))}     # (elbow out over the box)
        hl, hr = on_knees(rig, P)
        rest = {"l": hl, "r": hr}
        w_hold = 1.0 - ramp(t, T["hand_over"] + 0.02, 0.6) if t < 1.0 else ramp(t, 2.12, T["take"])
        for s in ("l", "r"):
            P["hand_" + s] = blend_hand([(w_hold, to_local(pl, hold[s])), (1 - w_hold, rest[s])])
            P["fingers_" + s] = 50.0 + 12.0 * w_hold
        P["props"] = {"mortar_bomb": (np.linalg.inv(pl.matrix()) @ B, shown)}
        return P

    give = kneel_body(bend=26.0, turn=-14.0, side=-4.0, fwd=0.08, drop=0.04, neck=(22.0, -8.0))
    sit = kneel_body(bend=8.0, turn=-6.0, neck=(6.0, -16.0))
    duck = kneel_body(bend=34.0, turn=20.0, side=8.0, drop=0.08, fwd=-0.03, neck=(30.0, 30.0))
    dig = kneel_body(bend=46.0, turn=-30.0, side=-10.0, fwd=0.06, drop=-0.03, neck=(30.0, -20.0))
    keys = [(0.0, give), (T["hand_over"], give), (0.75, sit), (1.3, sit), (1.62, duck), (2.0, duck), (2.4, dig), (2.55, dig),
            (2.95, give)]
    keys = reached_keys("mortar_number3", keys, dress, free, long=T["long"])
    P = looped(keys, T["long"])(t)
    P = S.breathe(P, t, 1.6, 0.4)
    return reach_fix(dress(t, P), free=free)


# ---- the heavy flak's crew: the layers at their handwheels, the loader, two men passing rounds up
FLAK = {}
FLAK_T = {"long": 3.2, "case": 0.8, "close": 2.45, "home": 2.2, "take": 2.6, "pass": 2.0, "up": 2.85}
LOADER_FIT = (-1.25, 0.20, 0.66)    # (game axes: the frame stand_load_breech is made in, crew.loader before 09:41Z, facing the
                                    # gun's line; since then crew.loader stands 0.30 to the right of it, turned 8.5 degrees
                                    # left: the clip is said there, the same moves in the gun's frame)


def flak():
    """The HeavyFlak's measures (its frame, Blender's axes; the gun laid level): the handwheels, the breech, the
    ready rack, the crew's places, the points where rounds change hands."""
    if not FLAK:
        g = lambda n: np.asarray(fsock("HeavyFlak", n)["at"], float)
        parts = {p["name"]: p for p in piece_json("fort", "HeavyFlak")["parts"]}
        FLAK["breech"] = g("breech")
        FLAK["wheel"] = {k: g2b(parts["handwheel." + k]["pivot"]) for k in ("traverse", "elevate")}     # (the hubs)
        # (on the handwheels' handles, 3 cm nearer the layer than the rim; parented to the wheels, as they stand at rest)
        FLAK["grips"] = {k: (g("grip.layer.%s.l" % k), g("grip.layer.%s.r" % k)) for k in ("traverse", "elevate")}
        FLAK["rack"] = g2b(parts["round.ready"]["pivot"])          # (the ready rack's nearest round, its own part, on its base)
        FLAK["pass"] = g2b((-2.74, -0.12, 1.02))                      # (where number two of the ammunition hands to number one)
        FLAK["up"] = g2b((-1.98, -0.78, 1.86))                        # (where he hands it up over the rail)
        for k in ("layer.traverse", "layer.elevate", "loader", "ammo.1", "ammo.2"):
            FLAK[k] = crew_place("HeavyFlak", "crew." + k)
        # (stand_load_breech's moves are made in this frame, square to the gun's line, and said at crew.loader after)
        FLAK["loader.fit"] = Place(g2b(LOADER_FIT), -np.asarray(fsock("HeavyFlak", "breech")["facing"], float))
    return FLAK


def stand_body(bend=0.0, turn=0.0, side=0.0, shift=(0.0, 0.0), drop=0.0, neck=(0.0, 0.0), feet=None):
    """Standing, his rifle slung: bent forward from the hips, the shoulders turned (+ left), leaned to his left, the
    hips moved by `shift` (forward, left) and lowered by `drop` (the knees bend), the neck (down, left); feet
    {"l": (x, y, yaw), "r": ...} on the floor."""
    P = S.base_stand()
    P["weapon"], P["weapon_hidden"] = stowed(), True
    P.pop("hand_l", None)
    P.pop("hand_r", None)
    pa = P["pelvis_at"]
    P["pelvis_at"] = (pa[0] + shift[0], pa[1] + shift[1], pa[2] - drop)
    P["pelvis"] = add3(P["pelvis"], (0.35 * bend, 0.2 * side, 0.3 * turn))
    P["spine_01"] = add3(P["spine_01"], (0.25 * bend, 0.25 * side, 0.25 * turn))
    P["spine_02"] = add3(P["spine_02"], (0.2 * bend, 0.3 * side, 0.25 * turn))
    P["chest"] = add3(P["chest"], (0.2 * bend, 0.25 * side, 0.2 * turn))
    P["neck"] = (-0.3 * bend + 0.45 * neck[0], 0.0, 0.45 * neck[1])
    P["head"] = (-0.2 * bend + 0.55 * neck[0], 0.0, 0.55 * neck[1])
    f = feet or {"l": (0.03, 0.13, 8.0), "r": (-0.03, -0.13, -10.0)}
    P["foot_l"] = {"at": (f["l"][0], f["l"][1], MAN["ankle"]), "yaw": f["l"][2]}
    P["foot_r"] = {"at": (f["r"][0], f["r"][1], MAN["ankle"]), "yaw": f["r"][2]}
    P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 40.0, 40.0, 30.0, 30.0
    return P


REACH_SCALE = np.array((10.0, 10.0, 10.0, 0.1, 0.1, 0.1))


def _body_delta(P, x):
    """A pose moved by (bend, turn, side degrees; forward, left, down metres) spread as stand_body spreads them."""
    Q = dict(P)
    bend, turn, side, fx, fy, dz = x
    pa = Q.get("pelvis_at", (0, 0, 0))
    Q["pelvis_at"] = (pa[0] + fx, pa[1] + fy, pa[2] - dz)
    Q["pelvis"] = add3(Q.get("pelvis", (0, 0, 0)), (0.35 * bend, 0.2 * side, 0.3 * turn))
    Q["spine_01"] = add3(Q.get("spine_01", (0, 0, 0)), (0.25 * bend, 0.25 * side, 0.25 * turn))
    Q["spine_02"] = add3(Q.get("spine_02", (0, 0, 0)), (0.2 * bend, 0.3 * side, 0.25 * turn))
    Q["chest"] = add3(Q.get("chest", (0, 0, 0)), (0.2 * bend, 0.25 * side, 0.2 * turn))
    return Q


def reach_fix(P, iters=6, reg=0.004, free=(1, 1, 1, 1, 1, 1)):
    """Where a hand falls short of its grip, the body is bent, turned, leaned and moved (the least it takes,
    damped least squares) until both hands reach. Smooth in time: the same sum from the same pose each frame."""
    rig = MODEL["rig"]
    free = np.asarray(free, float)

    def res(x):
        st = rig.solve(_body_delta(P, x))
        sh = [max(0.0, st["short"].get("arm_l", 0.0)), max(0.0, st["short"].get("arm_r", 0.0))]
        return np.concatenate([np.array(sh) * 10.0, x / REACH_SCALE * reg * 10.0])
    x = np.zeros(6)
    r = res(x)
    if r[:2].max() < 0.002 * 10.0:
        return P
    best = (float(r @ r), x)
    for _ in range(iters):
        J = np.zeros((len(r), 6))
        for i in range(6):
            if not free[i]:
                continue
            d = np.zeros(6)
            d[i] = REACH_SCALE[i] * 0.05
            J[:, i] = (res(x + d) - r) / d[i]
        step = np.linalg.solve(J.T @ J + 1e-4 * np.diag(1.0 / REACH_SCALE ** 2), -J.T @ r) * free
        for k in (1.0, 0.5, 0.25):                  # (a step that makes it worse is halved)
            rn = res(x + k * step)
            if float(rn @ rn) < best[0]:
                break
        x, r = x + k * step, rn
        if float(r @ r) < best[0]:
            best = (float(r @ r), x)
        if r[:2].max() < 0.001 * 10.0:
            break
    return _body_delta(P, best[1])


KEY_FIT = {}
# (the body carrying the hands, for the clips whose arms are tracked (ARM_HOLD): at each of reached_keys' samples
# the body's bend, turn, lean and the hips' place are searched (a pattern search from the last sample's) for where
# both arms are easiest, as the arm tracker reckons them on a coarser grid, each change of body paid for (reg, per
# step of `scale`) and each change from the last sample too (tie): turned 20-40 degrees and stepped in where that
# takes the arms off their limits. lim: the most of each (bend, turn, side degrees; forward, left, down metres).)
COMFY = {"grid": (30.0, 30.0, 15.0), "reg": 4.0, "tie": 12.0, "scale": (10.0, 10.0, 10.0, 0.1, 0.1, 0.1),
         "step": (8.0, 8.0, 6.0, 0.06, 0.06, 0.05), "lim_lo": (-25.0, -40.0, -20.0, -0.15, -0.15, -0.10),
         "lim_hi": (35.0, 40.0, 20.0, 0.15, 0.15, 0.25), "evals": 90}


def _comfy_arms(P, holds):
    """Both arms' least cost on the coarse grid (_arm_costs) for a dressed pose P."""
    import rom
    rig = MODEL["rig"]
    body = rom.Body.of_rig(rig)
    st = rig.solve(dict(P, comfort=False))
    tot = 0.0
    for s, side in (("l", 1.0), ("r", -1.0)):
        h = P["hand_" + s]
        g, a1, n1, pole = rig.resolve_hand(st, P, h, side)
        tot += float(_arm_costs(rig, body, st, s, g, a1, n1, _rod_r(h), holds[s], steps=COMFY["grid"])[0].min())
    return tot


COMFY_STAND = {"lim_lo": (-25.0, -40.0, -20.0, -0.25, -0.25, -0.10), "lim_hi": (35.0, 40.0, 20.0, 0.25, 0.25, 0.25)}
# (clips whose body may step only a little: the mortar men in their pit, whose back and sides are close)
COMFY_CLIP = {k: {"lim_lo": (-25.0, -40.0, -20.0, -0.04, -0.04, -0.10), "lim_hi": (35.0, 40.0, 20.0, 0.04, 0.04, 0.25)}
              for k in ("mortar_number1", "mortar_number3")}
# (the loaders' lunge is placed with care between the recoil, the rail and the rack: the body's own step kept small)
for _k in ("stand_load_breech", "stand_load_breech_twin", "stand_load_breech_twin_l"):
    COMFY_CLIP[_k] = {"lim_lo": (-25.0, -40.0, -20.0, -0.04, -0.06, -0.05), "lim_hi": (35.0, 40.0, 20.0, 0.04, 0.06, 0.10)}
COMFY_WIDE = ("stand_load_breech", "stand_load_breech_twin", "stand_load_breech_twin_l", "stand_ammo_give",
              "stand_ammo_crate_hand_up")


def comfy_body(name, dressed, free, fixed=()):
    """The body's moves (bend, turn, side, forward, left, down) for each of a looped clip's dressed samples, that let
    its body carry the hands (see COMFY): (n, 6); none at the samples `fixed` (where the body is placed for itself).
    Kept on disk under the samples' own key."""
    import hashlib
    rig = MODEL["rig"]
    _TORSO_M[0] = TRACK_CLIP.get(name, {}).get("torso_m", 0.01)
    holds = ARM_HOLD[name]
    free = np.asarray(free, float)
    wide = name in COMFY_WIDE
    if wide:
        free = np.ones(6)
    sc = np.asarray(COMFY["scale"])
    lim = COMFY_CLIP.get(name, COMFY_STAND if wide else COMFY)
    lo, hi = np.asarray(lim["lim_lo"]), np.asarray(lim["lim_hi"])
    feats = []
    for P in dressed:
        st = rig.solve(dict(P, comfort=False))
        row = [st["H"]["pelvis"], st["H"]["chest"]]
        for s, side in (("l", 1.0), ("r", -1.0)):
            g, a1, n1, pole = rig.resolve_hand(st, P, P["hand_" + s], side)
            row += [g, a1, n1]
        feats.append(np.concatenate(row))
    key = hashlib.sha1(json.dumps([name, COMFY, TRACK, holds, list(free), sorted(int(i) for i in fixed)] +
                                  ([COMFY_STAND] if wide else []) + ([COMFY_CLIP[name]] if name in COMFY_CLIP else []),
                                  sort_keys=True).encode() +
                       np.round(np.array(feats), 4).tobytes()).hexdigest()
    path = os.path.join(WORK, "arm_track", name + "_body.pkl")
    if os.path.exists(path):
        old = pickle.load(open(path, "rb"))
        if old.get("key") == key:
            return old["x"]
    n = len(dressed)

    def f(i, x, nb):
        c = _comfy_arms(_body_delta(dressed[i], x), holds) + COMFY["reg"] * float(np.sum((x / sc) ** 2))
        for xn in nb:
            c += COMFY["tie"] * float(np.sum(((x - xn) / sc) ** 2))
        return c

    def search(i, x0, nb):
        x = np.clip(np.asarray(x0, float), lo, hi) * free
        fx = f(i, x, nb)
        step = np.asarray(COMFY["step"]) * free
        evals = 1
        while evals < COMFY["evals"] and step.max() > 0:
            best = None
            for j in np.nonzero(free)[0]:
                for sg in (1.0, -1.0):
                    y = x.copy()
                    y[j] = np.clip(y[j] + sg * step[j], lo[j], hi[j])
                    if np.allclose(y, x):
                        continue
                    fy = f(i, y, nb)
                    evals += 1
                    if fy < fx - 1e-6 and (best is None or fy < best[0]):
                        best = (fy, y)
            if best is None:
                step = step * 0.5
                if np.all(step <= np.asarray(COMFY["step"]) * free / 8.0 + 1e-12):
                    break
            else:
                fx, x = best
        return x, fx
    X = np.zeros((n, 6))
    fixed = set(int(i) for i in fixed)
    for i in range(n):                                       # (once round from rest, each from the last)
        if i not in fixed:
            X[i], _ = search(i, X[i - 1] if i else np.zeros(6), [X[i - 1]] if i else [])
    for i in range(n):                                       # (and again, tied to both neighbours round the loop)
        if i not in fixed:
            X[i], fx = search(i, X[i], [X[i - 1], X[(i + 1) % n]])
    os.makedirs(os.path.dirname(path) + "/", exist_ok=True)
    pickle.dump({"key": key, "x": X}, open(path, "wb"))
    print("comfy_body %s: turn %s" % (name, np.round(X[:, 1], 0).tolist()), flush=True)
    return X


def reached_keys(name, keys, dress, free=(1, 1, 1, 1, 1, 1), long=None, dt=0.1):
    """A looped clip's body made to reach what its hands hold: the keyed body sampled every `dt` seconds, each
    sample dressed with that moment's hands (dress(t, P)) and bent and moved until they reach (reach_fix), found
    once per clip; these are the clip's keys then. The body is keyed through poses that reach and the per-frame
    reach_fix only takes up the little left between them: no lurch where a grip moves faster than the body."""
    if name not in KEY_FIT:
        base = looped(keys, long)
        out = []
        tks = np.arange(0.0, long - 1e-6, dt)
        carried = comfy_body(name, [dress(tk, dict(base(tk))) for tk in tks], free) if name in ARM_HOLD else None
        for i, tk in enumerate(tks):
            Pk = base(tk)
            if carried is not None:
                Pk = _body_delta(Pk, carried[i])
            Q = dict(reach_fix(dress(tk, dict(Pk)), free=free))
            for k in ("hand_l", "hand_r", "props", "fingers_l", "fingers_r"):
                if k in Pk:
                    Q[k] = Pk[k]
                else:
                    Q.pop(k, None)
            out.append((float(tk), Q))
        KEY_FIT[name] = out
    return KEY_FIT[name]


def hands_rest_pinned(rig, P):
    """hands_rest, but where they hang for this pose, held there in the clip's frame: blended toward a grip, the hand
    then goes where the blend says and does not drag the resting end along as reach_fix bends the body (with the
    chest's frame, any blend toward a low grip asks for the whole stoop at once)."""
    rest = hands_rest(rig, P)
    Q = dict(P, hand_l=rest["l"], hand_r=rest["r"])
    st = rig.solve(Q)
    out = {}
    for s, side in (("l", 1.0), ("r", -1.0)):
        g, a, n, pole = rig.resolve_hand(st, Q, rest[s], side)
        out[s] = {"at": tuple(g), "a": tuple(a), "n": tuple(n), "pole_world": tuple(pole)}
    return out


def stepped(P, s, t, plan):
    """Foot s where a plan of steps puts it at t: plan = [(t0, t1, A, B, h, first)] in time order, each step carrying
    the foot from where pose A has it to where pose B has it, lifted h on the way (first "x": forward first, "y":
    across first, as round a corner; None: straight); between steps it stands where the last step left it."""
    def put(Q):
        P["foot_" + s] = dict(Q["foot_" + s])
        return P
    for i, (t0, t1, A, B, h, first) in enumerate(plan):
        if t < t0:
            return put(plan[i - 1][3] if i else plan[-1][3])
        if t <= t1:
            u = (t - t0) / (t1 - t0)
            early, late = sm(min(1.0, u / 0.65)), sm(max(0.0, (u - 0.35) / 0.65))
            ux, uy = {"x": (early, late), "y": (late, early)}.get(first, (sm(u), sm(u)))
            fa, fb = A["foot_" + s], B["foot_" + s]
            a, b = np.asarray(fa["at"], float), np.asarray(fb["at"], float)
            f = dict(fb)
            f["at"] = (a[0] + ux * (b[0] - a[0]), a[1] + uy * (b[1] - a[1]), a[2] + sm(u) * (b[2] - a[2]))
            f["yaw"] = fa.get("yaw", 0.0) + sm(u) * (fb.get("yaw", 0.0) - fa.get("yaw", 0.0))
            P["foot_" + s] = f
            return lifted(P, s, t, t0, t1, h)
    return put(plan[-1][3])


def lifted(P, s, t, t0, t1, h=0.07):
    """A foot lifted off the floor between t0 and t1 (as it is moved)."""
    k = bump(t, t0, 0.5 * (t0 + t1), t1)
    if k > 0:
        f = dict(P["foot_" + s])
        a = np.asarray(f["at"], float)
        f["at"] = (a[0], a[1], a[2] + h * k)
        f["pitch"] = f.get("pitch", 0.0) + 8.0 * k
        P["foot_" + s] = f
    return P


def hands_rest(rig, P, where="sides"):
    """Hands resting: hanging at his sides."""
    out = {}
    for s, side in (("l", 1.0), ("r", -1.0)):
        out[s] = {"frame": "chest", "at": (0.04, 0.24 * side, -0.47), "a": (0.15, 0.1 * side, -1.0), "n": (0.0, -side, 0.0),
                  "pole": (-1.0, 0.3 * side, 0.0)}
    return out


def wheel_turn(t, long=1.0):
    """How far a layer's handwheel has turned at t (degrees about Blender's +x) as the game turns it plus, one turn in
    `long` seconds: the game's plus about its x brings the right-hand (game +y) handle up (so the piece's own follow
    clip turns it), which is minus about Blender's x."""
    return -360.0 * t / long


# (each fist round the middle of its handle, on the grip socket (since the 10:19Z export the sockets are the fists'
# middles, the handles 9.5 cm long), the thumb toward him: so it stays clear of the rim, the spokes and the wheel's face
# all the way round; "back" would move it along the handle toward him)
LAYER_FIST = {"back": 0.0, "dir": -1.0, "fingers": 66.0, "thumb": 40.0}


def flak_wheel_hands(kind, t, long):
    """The layer's fists on his handwheel's two handles, on grip.layer.<kind>.l/r (the sockets sit on the handles at
    9 and 3 o'clock, 3 cm nearer him than the rim, and turn with the wheel), going round with the wheel one turn in
    `long` seconds the game's plus way; each fist closed round its handle (the handles stand out of the rim toward
    him, along the wheel's axis)."""
    F = flak()
    hub = F["wheel"][kind]
    R = R_axis((1.0, 0.0, 0.0), wheel_turn(t, long))
    out = {}
    for s, g in zip(("l", "r"), F["grips"][kind]):
        p = hub + R @ (g - hub)
        knob = M4(frame_from(np.array((1.0, 0.0, 0.0)), (0.0, 0.0, 1.0)), p)
        shoulder = F["layer." + kind].world((-0.02, 0.18 if s == "l" else -0.18, 0.58))
        # (the hand comes onto the handle from his shoulder's side of the wheel, the same way all round the turn:
        # the handle turns in his fist rather than his wrist flicking round as it passes his shoulder's line)
        h = on_rod(s, knob, -LAYER_FIST["back"], 0.0, p + (shoulder - hub), thumb=LAYER_FIST["dir"])
        side = 1.0 if s == "l" else -1.0
        h["pole_world"] = tuple(F["layer." + kind].R @ np.array((-0.3, 1.0 * side, -1.0)))
        out[s] = h
    return out


def _flak_layer(kind, t):
    F = flak()
    pl = F["layer." + kind]
    long = 1.0
    c = pl.local(F["wheel"][kind])
    toward = math.degrees(math.atan2(c[1], c[0]))
    th = math.radians(wheel_turn(t, long))
    far = 0.5 * (1 - math.cos(th))                      # (the inboard handle at the wheel's far side: he reaches after it)
    P = sit_body(0.44, slouch=0.3, lean=18.0 + 8.0 * far, turn=0.6 * toward + 26.0 * far * math.copysign(1.0, toward), hip_x=-0.02)
    roll = 1.6 * math.sin(th)
    P["spine_02"] = add3(P["spine_02"], (0.8 * math.cos(th), roll, 0.0))
    P["chest"] = add3(P["chest"], (0.6 * math.cos(th), 0.8 * roll, 1.5 * math.sin(th)))
    P["neck"] = add3(P["neck"], (6.0, 0.0, 0.25 * toward))
    P["head"] = add3(P["head"], (4.0, 0.0, 0.2 * toward))
    hands = flak_wheel_hands(kind, t, long)
    for s in ("l", "r"):
        P["hand_" + s] = to_local(pl, hands[s])
        P["fingers_" + s], P["thumb_" + s] = LAYER_FIST["fingers"], LAYER_FIST["thumb"]
    P["weapon"], P["weapon_hidden"] = stowed(), True
    return seated("sit_handwheel_" + kind, P)


@crew_clip("sit_handwheel_traverse", 1.0, loop=True, anchor="seat", weapon="none",
           place=[("fort", "HeavyFlak", "crew.layer.traverse", (0, 0, 0)), ("fort", "HeavyFlakTwin", "crew.layer.traverse", (0, 0, 0))],
           about="The traverse layer on his seat (attach to the part `mount`), his fists on the handwheel's two handles "
                 "(each fist on grip.layer.traverse.l/r, the handle's middle, the thumb toward him), "
                 "turning it one turn a second the game's plus way (about the wheel's axis, the game's x: the right-hand "
                 "handle rises), from the handles' rest (9 and 3 o'clock) at t = 0: play it at the wheel's own speed (the "
                 "game spins handwheel.traverse 60 degrees for each degree the mount turns: one turn a second at 6 degrees "
                 "a second), backwards while the wheel turns minus, and hold it when the wheel stops.")
def c_sit_handwheel_traverse(t):
    return _flak_layer("traverse", t)


@crew_clip("sit_handwheel_elevate", 1.0, loop=True, anchor="seat", weapon="none",
           place=[("fort", "HeavyFlak", "crew.layer.elevate", (0, 0, 0)), ("fort", "HeavyFlakTwin", "crew.layer.elevate", (0, 0, 0))],
           about="The elevation layer, as sit_handwheel_traverse on the other side, at handwheel.elevate.")
def c_sit_handwheel_elevate(t):
    return _flak_layer("elevate", t)


LOADER_READY_1 = ((0.05, -0.49, 1.01), (0.8, 0.55, 0.13))   # (the single gun's loader: back from the breech, which recoils)
LOADER_READY = ((0.22, -0.40, 1.10), (0.85, 0.5, 0.12))    # (the round at the ready: on both palms before his chest,
#                                                             the elbows down and ahead of him, its nose clear of the recoil)
LOADER = {"grab": 2.72, "up": 2.95, "ready": 0.3, "let_go": 1.88}      # (both hands on the rack's round; lifted out; at the ready)


def flak_round_loader(t):
    """The loader's round (the HeavyFlak's frame) and whether it shows: held ready while the gun fires, swung up
    and its nose put into the open breech, rammed home by 2.2, gone; the next lifted from the ready rack."""
    F = flak()
    T = FLAK_T
    pl = F["loader.fit"]
    br = F["breech"]
    ready = bomb_frame(pl.world(LOADER_READY_1[0]), pl.R @ _n(np.array(LOADER_READY_1[1])))
    aligned = bomb_frame(br + np.array((-0.74, 0.0, -0.03)), (1.0, 0.0, 0.0))
    home = bomb_frame(br + np.array((0.02, 0.0, 0.0)), (1.0, 0.0, 0.0))
    rack = bomb_frame(F["rack"], (0.0, 0.0, 1.0), up=(1.0, 0.0, 0.0))
    up = bomb_frame(F["rack"] + np.array((0.04, 0.0, 0.16)), (0.15, 0.0, 1.0), up=(1.0, 0.0, 0.0))
    W = LOADER["ready"]
    if W <= t < T["home"] + 0.02:
        k = [(W, ready), (1.0, ready), (1.45, bomb_frame(br + np.array((-1.1, -0.12, -0.3)), (0.95, 0.05, 0.25))), (1.75, aligned),
             (T["home"], home)]
        return slerp_frames(k, t), True
    if W <= t < T["take"]:
        return home, False
    tt = t + T["long"] if t < W else t                   # (the next round comes up to the ready over the loop's end)
    if tt < LOADER["grab"]:
        return rack, True                                # (standing in the rack, where round.ready stood, till he lifts it)
    k = [(T["take"], rack), (LOADER["grab"], rack), (LOADER["up"], up), (T["long"] + W, ready)]
    return slerp_frames(k, tt), True


@crew_clip("stand_load_breech", 3.2, loop=True, anchor="feet", weapon="none",
           place=[("fort", "HeavyFlak", "crew.loader", (0, 0, 0))],
           events=[{"t": 0.0, "fx": "piece_clip", "clip": "fire", "cue": "play the HeavyFlak's own `fire` clip with it"},
                   {"t": 1.75, "fx": "", "cue": "round_in"}, {"t": 2.2, "fx": "", "cue": "round_home"}],
           about="The loader on the platform behind the breech (attach to the part `mount`; his root at crew.loader as it "
                 "stands, no offset), in step with the gun's own fire clip (play both from the same moment, t = 0 the "
                 "shot): he stands at crew.loader, clear of the recoil, with the next round in his arms while the barrel "
                 "runs back and the case flies out (0.8); steps in to his left, puts the round's nose into the open breech, "
                 "rams it home with the flat of his right hand (2.2) and draws it clear before the breech closes (2.45); turns to the ready rack, his "
                 "hands on its nearest round at 2.6 (the fire clip hides round.ready then; the prop round shows in its "
                 "place, standing as it stood), closes them on it and lifts it out from 2.72, up to the ready by 0.3 into "
                 "the next cycle, stepping back to crew.loader. The gun laid level (the breech at its rest).")
def c_stand_load_breech(t):
    F = flak()
    T = FLAK_T
    pl = F["loader.fit"]                 # (made square to the gun's line; said at crew.loader at the end)
    rig = MODEL["rig"]
    free = (1, 1, 1, 1, 0, 1)

    def dress(t, P):
        B, shown = flak_round_loader(t)
        # (once the round is home his hands go to the next one, standing in the rack, not to the one in the breech)
        Bh = flak_round_loader(T["take"])[0] if T["home"] + 0.02 <= t < T["take"] else B
        me = pl.world((0.0, 0.0, 1.2))
        under = me + np.array((0.0, 0.0, -0.6))
        tt = t + T["long"] if t < LOADER["ready"] else t
        u = ramp(tt, LOADER["grab"] + 0.03, T["long"] + 0.15) if tt >= T["take"] - 0.3 else 1.0   # (from its top to carrying it)
        xl, xr = 0.75 + (0.6 - 0.75) * u, 0.55 + (0.28 - 0.55) * u      # (carried on both palms before him)
        on = {"l": on_rod("l", Bh, xl, 0.05, under, thumb=1.0, pole=pl.R @ np.array((-0.2, 1.0, -1.0))),
              "r": on_rod("r", Bh, xr, 0.055, under, thumb=1.0, pole=pl.R @ np.array((-0.4, -1.0, -0.6)))}
        # (ramming: the right palm flat on the case's base, pushing along the bore, the fingers up)
        Bf = flak_round_loader(T["home"])[0] if t >= T["home"] else B          # (the round rammed home: his hand leaves it there)
        base = Bf[:3, 3]
        fist = {"at": tuple(base - Bf[:3, 0] * 0.03), "a": tuple(_n(np.array((0.0, 0.0, 1.0)) - Bf[:3, 0] * Bf[2, 0])), "n": tuple(Bf[:3, 0]),
                "pole_world": tuple(pl.R @ np.array((-0.3, -1.0, -0.8)))}
        # (the ramming hand drawn back out of the breech as it closes (2.45) and on, as he turns, to the rack's round, on
        # it at `grab`; the left hand lets go of the round at 1.88 as the right drives it home, and goes to the rack's round
        # with the turn of his body: no hand flung to his side and back)
        rack_B = flak_round_loader(T["take"])[0]
        on_rack = {"l": on_rod("l", rack_B, 0.75, 0.05, under, thumb=1.0, pole=pl.R @ np.array((-0.2, 1.0, -1.0))),
                   "r": on_rod("r", rack_B, 0.55, 0.055, under, thumb=1.0, pole=pl.R @ np.array((-0.4, -1.0, -0.6)))}
        let_go = on_rod("l", flak_round_loader(LOADER["let_go"])[0], 0.6, 0.05, under, thumb=1.0, pole=pl.R @ np.array((-0.2, 1.0, -1.0)))
        if T["home"] + 0.02 <= t < LOADER["grab"]:
            w = ramp(t, T["home"] + 0.02, LOADER["grab"])
            hr = blend_hand([(1 - w, to_local(pl, fist)), (w, to_local(pl, on_rack["r"]))])
            w_ram = 1 - w
        else:
            w_ram = ramp(t, 1.6, 1.9) if 1.0 <= t < T["home"] + 0.02 else 0.0
            hr = blend_hand([(1 - w_ram, to_local(pl, on["r"])), (w_ram, to_local(pl, fist))])
        if LOADER["let_go"] <= t < LOADER["grab"]:
            w = ramp(t, LOADER["let_go"], LOADER["grab"])
            hl = blend_hand([(1 - w, to_local(pl, let_go)), (w, to_local(pl, on_rack["l"]))])
        else:
            hl = to_local(pl, on["l"])
        P["hand_l"], P["hand_r"] = hl, hr
        P["fingers_l"], P["fingers_r"] = 62.0, 62.0 - 30.0 * w_ram
        P["props"] = {"flak_round": (np.linalg.inv(pl.matrix()) @ B, shown)}
        return P

    # (clear of the recoil at crew.loader, 0.3 to the right of the gun's line here; the right foot in front of the
    # ready rack, not in its cups)
    clear = stand_body(bend=8.0, turn=10.0, side=-10.0, shift=(0.06, -0.27), drop=0.03, neck=(6.0, 18.0),
                       feet={"l": (0.10, -0.10, 6.0), "r": (0.05, -0.44, -14.0)})
    lift = stand_body(bend=4.0, turn=-12.0, side=4.0, shift=(-0.02, -0.08), drop=0.05, neck=(-8.0, 10.0),
                      feet={"l": (0.12, 0.08, 6.0), "r": (-0.16, -0.24, -16.0)})
    ram = stand_body(bend=18.0, turn=-24.0, side=2.0, shift=(0.15, -0.08), drop=0.08, neck=(-4.0, 16.0),
                     feet={"l": (0.22, 0.08, 4.0), "r": (-0.16, -0.24, -16.0)})
    back = stand_body(bend=14.0, turn=-30.0, side=0.0, shift=(0.06, -0.1), drop=0.08, neck=(0.0, 0.0),        # (clear of the breech's guard)
                      feet={"l": (0.16, 0.08, 4.0), "r": (-0.16, -0.24, -16.0)})
    rack = stand_body(bend=44.0, turn=-80.0, side=-6.0, shift=(0.1, -0.02), drop=0.12, neck=(26.0, -30.0),
                      feet={"l": (0.06, 0.06, 8.0), "r": (-0.12, -0.28, -30.0)})
    rise = stand_body(bend=28.0, turn=-62.0, side=-4.0, shift=(0.06, -0.06), drop=0.07, neck=(16.0, -24.0),
                      feet={"l": (0.06, 0.06, 8.0), "r": (-0.12, -0.28, -30.0)})
    keys = [(0.0, S.mix([rise, clear], [0.35, 0.65])), (LOADER["ready"], clear), (0.9, clear), (1.35, lift), (1.75, lift),
            (T["home"], ram), (2.35, back), (T["take"], rack), (LOADER["grab"] + 0.02, rack), (3.0, rise)]
    keys = reached_keys("stand_load_breech", keys, dress, free, long=T["long"])
    P = looped(keys, T["long"])(t)
    # (his feet, step by step: in toward the gun the left foot first, the right one across and then back, round the
    # ready rack's cups; up to the breech; back to the rack; out again with the round, the right foot forward first)
    tt = t + T["long"] if t < LOADER["ready"] else t
    P = stepped(P, "l", tt, [(0.9, 1.15, clear, lift, 0.05, None), (1.8, 2.15, lift, ram, 0.05, None),
                             (2.25, 2.45, ram, rack, 0.04, None), (3.2, 3.45, rack, clear, 0.05, None)])
    P = stepped(P, "r", tt, [(1.1, 1.4, clear, lift, 0.07, "y"), (2.45, 2.65, back, rack, 0.03, None),
                             (2.98, 3.22, rack, clear, 0.07, "x")])
    return reframed(reach_fix(dress(t, P), free=free), pl, F["loader"])


def flak_round_ammo(t):
    """The round passed along on the ground (the HeavyFlak's frame): out of the crate at number two's (ammo.2's)
    right, carried round and held out crosswise between the two (1.55-2.0), taken by number one (ammo.1), carried and
    lifted over his head toward the rail, where it is taken (2.85)."""
    F = flak()
    T = FLAK_T
    p2, p1 = F["ammo.2"], F["ammo.1"]
    in_crate = bomb_frame(p2.world((-0.465, -0.575, 0.07)), p2.R @ np.array((1.0, 0.0, 0.0)))
    lifted_ = bomb_frame(p2.world((-0.4, -0.5, 0.5)), p2.R @ _n(np.array((1.0, 0.2, 0.15))))
    carried = bomb_frame(p2.world((-0.1, -0.35, 0.95)), p2.R @ _n(np.array((0.9, 0.45, 0.1))))
    mid = 0.5 * (p2.at + p1.at) + np.array((0.0, 0.0, 1.0))
    across = _n(np.cross((0.0, 0.0, 1.0), p1.at - p2.at))
    at_pass = bomb_frame(mid - 0.465 * across, across)
    held1 = bomb_frame(p1.world((0.3, 0.45, 1.0)), p1.R @ _n(np.array((0.0, -1.0, 0.1))))
    over = bomb_frame(p1.world((0.42, 0.42, 1.72)), p1.R @ _n(np.array((0.15, -1.0, 0.15))))
    return [(0.45, in_crate), (0.7, lifted_), (1.1, carried), (1.55, at_pass), (T["pass"], at_pass), (2.4, held1), (T["up"], over)]


def _flak_ammo_round(t, who):
    T = FLAK_T
    k = flak_round_ammo(t)
    if who == 2:
        return slerp_frames(k[:5], t), 0.45 <= t <= T["pass"] + 0.02
    return slerp_frames(k[4:], t), T["pass"] <= t <= T["up"] + 0.03


def round_hands(pl, B, xs=(0.3, 0.62), below=0.6, ref=None, flip=False):
    """Both hands on a round from a man's side (under it), each at the end on its own side. Which end is on which
    side is read off the round as it is at `ref` (a frame) if given, for the whole of the hold: never swapped while
    he holds it (a round lying along his line of sight has no end to either side, and each hand would snap round)."""
    under = pl.world((0.0, 0.0, 0.3))
    R_ = B if ref is None else ref
    ends = [R_[:3, 3] + R_[:3, 0] * x for x in xs]
    left_first = pl.local(ends[0])[1] > pl.local(ends[1])[1]
    xl, xr = (xs[0], xs[1]) if left_first else (xs[1], xs[0])
    th = 1.0 if left_first else -1.0
    fl = ("l", "r") if flip is True else (flip or ())                 # (flip: those hands' thumbs the other way along it)
    return {"l": on_rod("l", B, xl, 0.05, under, thumb=-th if "l" in fl else th, pole=pl.R @ np.array((-0.3, 1.0, -1.0))),
            "r": on_rod("r", B, xr, 0.055, under, thumb=th if "r" in fl else -th, pole=pl.R @ np.array((-0.3, -1.0, -1.0)))}


GIVE_FLIP = ("r",)    # (his right thumb on the round: so turned that his hand goes to the crate from his side with no snap)


@crew_clip("stand_ammo_give", 3.2, loop=True, anchor="feet", weapon="none",
           place=[("fort", "HeavyFlak", "crew.ammo.2", (0, 0, 0))],
           about="Number two of the ammunition, on the ground behind the gun: turns to the crate behind his right "
                 "shoulder (a prop, crew.json), lifts a round out, steps to his left and holds it out to ammo.1, who "
                 "takes it at 2.0; steps back. In step with stand_ammo_hand_up and the loader (3.2 s, the gun's own cycle).")
def c_stand_ammo_give(t):
    F = flak()
    T = FLAK_T
    pl = F["ammo.2"]
    rig = MODEL["rig"]
    free = (1, 1, 1, 1, 1, 1)

    def dress(t, P):
        B, shown = _flak_ammo_round(t, 2)
        # (reaching for the next round: the hands go to it in the crate, not to where the last was handed over)
        hold = round_hands(pl, _flak_ammo_round(0.45, 2)[0] if t >= 2.5 else B, ref=_flak_ammo_round(1.1, 2)[0], flip=GIVE_FLIP)
        rest = hands_rest_pinned(rig, P)
        # (he reaches for the crate from 2.95, round the loop, to have the round at 0.45)
        w = ramp(t + T["long"] if t < 1.0 else t, 2.95, T["long"] + 0.45) if (t < 1.0 or t >= 2.95) else 1 - ramp(t, T["pass"] + 0.03, 2.5)
        for s in ("l", "r"):
            P["hand_" + s] = blend_hand([(w, to_local(pl, hold[s])), (1 - w, rest[s])])
            P["fingers_" + s] = 40.0 + 25.0 * w
        P["props"] = {"flak_round": (np.linalg.inv(pl.matrix()) @ B, shown), "flak_crate": (M4(np.eye(3), (-0.05, -0.62, 0.0)), True)}
        return P

    stand = stand_body(bend=4.0, turn=-10.0, neck=(4.0, 10.0))
    crate = stand_body(bend=52.0, turn=-60.0, side=-8.0, shift=(-0.1, -0.12), drop=0.2, neck=(20.0, -20.0),
                       feet={"l": (0.08, 0.14, -10.0), "r": (-0.1, -0.2, -45.0)})
    carry = stand_body(bend=6.0, turn=-10.0, drop=0.03, neck=(8.0, 20.0), feet={"l": (0.05, 0.13, 10.0), "r": (-0.06, -0.14, -20.0)})
    give = stand_body(bend=18.0, turn=66.0, side=10.0, shift=(0.02, 0.26), drop=0.06, neck=(12.0, 24.0),
                      feet={"l": (0.06, 0.46, 50.0), "r": (-0.04, -0.1, 20.0)})
    keys = [(0.0, stand), (0.45, crate), (0.62, crate), (1.05, carry), (1.55, give), (T["pass"], give), (2.6, stand)]
    keys = reached_keys("stand_ammo_give", keys, dress, free, long=T["long"])
    P = looped(keys, T["long"])(t)
    P = lifted(P, "l", t, 1.1, 1.5, 0.08)
    P = lifted(P, "l", t, 2.08, 2.5, 0.06)
    return reach_fix(dress(t, P), free=free)


@crew_clip("stand_ammo_hand_up", 3.2, loop=True, anchor="feet", weapon="none",
           place=[("fort", "HeavyFlak", "crew.ammo.1", (0, 0, 0))],
           about="Number one of the ammunition, on the ground by the platform's edge: turns to his right and takes the "
                 "round from ammo.2 at 2.0, swings it up and hands it over the rail (at 2.85 it is taken: hidden), "
                 "waits with his eyes on the gun. In step with stand_ammo_give and the loader.")
def c_stand_ammo_hand_up(t):
    F = flak()
    T = FLAK_T
    pl = F["ammo.1"]
    rig = MODEL["rig"]
    free = (1, 1, 1, 1, 1, 1)

    def dress(t, P):
        B, shown = _flak_ammo_round(t, 1)
        # (his arms coming down after the hand-up: from where he let it go, over his head)
        hold = round_hands(pl, _flak_ammo_round(T["up"], 1)[0] if (t < 1.0 or t > T["up"]) else B, ref=_flak_ammo_round(2.4, 1)[0])
        rest = hands_rest_pinned(rig, P)
        w = 1 - ramp(t + T["long"], T["up"] + 0.03, 3.55) if t < 0.4 else ramp(t, 1.6, T["pass"]) * (1 - ramp(t, T["up"] + 0.03, 3.55))
        for s in ("l", "r"):
            P["hand_" + s] = blend_hand([(w, to_local(pl, hold[s])), (1 - w, rest[s])])
            P["fingers_" + s] = 40.0 + 25.0 * w
        P["props"] = {"flak_round": (np.linalg.inv(pl.matrix()) @ B, shown)}
        return P

    wait = stand_body(bend=2.0, turn=-6.0, neck=(-14.0, 4.0))
    take = stand_body(bend=18.0, turn=-66.0, side=-10.0, shift=(0.02, -0.26), drop=0.06, neck=(12.0, -24.0),
                      feet={"l": (0.04, 0.1, -20.0), "r": (0.06, -0.46, -50.0)})
    carry = stand_body(bend=4.0, turn=6.0, drop=0.06, neck=(-6.0, 0.0), feet={"l": (0.04, 0.12, -6.0), "r": (-0.04, -0.16, -14.0)})
    hand_up = stand_body(bend=-8.0, turn=10.0, shift=(0.1, 0.02), neck=(-26.0, 0.0), feet={"l": (0.26, 0.1, 0.0), "r": (-0.08, -0.16, -12.0)})
    keys = [(0.0, wait), (1.1, wait), (1.55, take), (T["pass"], take), (2.4, carry), (T["up"], hand_up), (3.05, wait)]
    keys = reached_keys("stand_ammo_hand_up", keys, dress, free, long=T["long"])
    P = looped(keys, T["long"])(t)
    P = lifted(P, "r", t, 1.1, 1.5, 0.07)
    P = lifted(P, "r", t, 2.02, 2.38, 0.06)
    P = lifted(P, "l", t, 2.42, 2.8, 0.07)
    P = lifted(P, "l", t, 2.9, 3.18, 0.05)
    return reach_fix(dress(t, P), free=free)



# ---- the twin heavy flak's loaders: loader.r from its ready rack's round.ready, loader.l (no ready round of his own)
# from ammunition number one's hands over the rail, and number one himself
TWIN = {}
TWIN_HAND = (-2.25, -0.75, 1.90)     # (game axes: the round's middle as ammo.1 holds it up over the rail to loader.l)
# (each loader's times: breech.r's runs 0.1 behind breech.l's in the twin's fire clip (3.3 s); loader.l takes his round
# from ammo.1 at `grab`, loader.r lifts his out of the rack at `grab`)
TWIN_T = {"r": {"dt": 0.1, "long": 3.3, "home": 2.3, "take": 2.7, "grab": 2.82, "up": 3.05, "ready": 0.4},
          "l": {"dt": 0.0, "long": 3.3, "home": 2.2, "take": 3.0, "grab": 3.08, "carry": 3.5, "ready": 0.6}}
# (loader.l walks to the rail after ramming, takes his round there at `grab` and walks back with it through the shot,
# well clear of the recoil; ammo.1's clip is made for a hand-over at AMMO1_GIVE and played AMMO1_SHIFT later)
AMMO1_GIVE = 2.75
AMMO1_SHIFT = TWIN_T["l"]["grab"] - AMMO1_GIVE
# (loader.l's walk to the rail behind him and back, canonical (the breech to his left: he works inboard of breech.l, on
# the far side of it from the fuze setter's machine): turning, taking the round from ammo.1, carrying it back; the spots
# where he takes and carries it are where they were when he worked outboard)
TWIN_L = {"turn1": dict(bend=10.0, turn=20.0, drop=0.05, neck=(0.0, 10.0), feet={"l": (0.04, 0.10, 0.0), "r": (-0.04, -0.12, 0.0)},
                        yaw=80.0, at=(-0.68, 0.30)),
          "take": dict(bend=8.0, turn=0.0, drop=0.05, neck=(-12.0, 0.0), feet={"l": (0.04, 0.10, 4.0), "r": (-0.06, -0.10, -6.0)},
                       yaw=178.0, at=(-0.60, 0.60)),
          "carry": dict(bend=8.0, turn=-10.0, drop=0.05, neck=(6.0, 0.0), feet={"l": (0.04, 0.10, 0.0), "r": (-0.04, -0.12, 0.0)},
                        yaw=90.0, at=(-0.62, 0.35)),
          # (back from the rail he stands on the breech's line behind the recoil while the guns fire, the round
          # crosswise before his chest, its nose to his right as ammo.1 gave it him; then the lunge with his left fist
          # on the base and his right foot leading, the brace's low end at his left)
          "clear": dict(bend=6.0, turn=-6.0, lean=0.0, shift=(-0.62, 0.10), drop=0.03, neck=(8.0, -6.0),
                        feet={"r": (-0.52, -0.04, -4.0), "l": (-0.76, 0.22, 10.0)}),
          "line": dict(bend=6.0, turn=-4.0, lean=0.0, shift=(-0.68, 0.06), drop=0.03, neck=(-2.0, -4.0),
                       feet={"r": (-0.50, -0.10, -4.0), "l": (-0.88, 0.16, 14.0)}),
          "set": dict(bend=10.0, turn=-4.0, lean=0.0, shift=(-0.56, 0.06), drop=0.06, neck=(-4.0, -4.0),
                      feet={"r": (-0.50, -0.10, -4.0), "l": (-0.66, 0.18, 14.0)}),
          "lunge": dict(bend=22.0, turn=-8.0, lean=0.0, shift=(-0.12, 0.06), drop=0.14, neck=(-10.0, 0.0),
                        feet={"r": (-0.04, -0.06, -2.0), "l": (-0.66, 0.18, 14.0)}),
          "back": dict(bend=14.0, turn=20.0, lean=0.0, shift=(-0.18, 0.04), drop=0.10, neck=(0.0, 0.0),
                       feet={"r": (-0.04, -0.06, -2.0), "l": (-0.66, 0.18, 14.0)}),
          "ready": ((-0.32, 0.28, 1.16), (0.35, -0.94, 0.05))}
# (the twin's loaders' lunge, canonical (his fit frame: the breech's face 0.57 ahead of him, 0.20 to his left, 1.59 up;
# it recoils 0.75 back): while the guns fire he stands behind the recoil and to its right, the round crosswise on both
# palms before his chest, its nose to his left; after the run-out he steps back half a pace onto the breech's line,
# square to it, the round raised along it with its nose at the breech (`raised`, from the breech) and its base ahead of
# his chest; sets, the nose in (`aligned`, the base's distance from the breech); lunges in on his left foot, trunk and
# legs driving and both arms reaching, his right fist following the base home, drawn back clear as the breech closes)
LUNGE = {"ready": ((-0.10, -0.56, 1.16), (0.25, 0.97, 0.06)),
         "raised": ((-0.97, 0.0, -0.19), (0.975, 0.0, 0.221)),
         "aligned": -0.72}
LUNGE_POSE = {
    "clear": dict(bend=6.0, turn=8.0, lean=0.0, shift=(-0.40, -0.38), drop=0.03, neck=(10.0, 24.0),
                  feet={"l": (-0.30, -0.24, 6.0), "r": (-0.48, -0.54, -12.0)}),
    "line": dict(bend=6.0, turn=4.0, lean=0.0, shift=(-0.68, 0.06), drop=0.03, neck=(-2.0, 10.0),
                 feet={"l": (-0.50, -0.10, 4.0), "r": (-0.90, -0.16, -14.0)}),
    "set": dict(bend=10.0, turn=0.0, lean=0.0, shift=(-0.56, 0.06), drop=0.06, neck=(-4.0, 8.0),
                feet={"l": (-0.50, -0.10, 4.0), "r": (-0.66, -0.16, -14.0)}),
    "lunge": dict(bend=22.0, turn=-4.0, lean=0.0, shift=(-0.12, 0.06), drop=0.14, neck=(-10.0, 6.0),
                  feet={"l": (0.0, -0.06, 2.0), "r": (-0.66, -0.16, -14.0)}),
    "back": dict(bend=14.0, turn=-20.0, lean=0.0, shift=(-0.18, 0.0), drop=0.10, neck=(0.0, 0.0),
                 feet={"l": (0.0, -0.06, 2.0), "r": (-0.66, -0.16, -14.0)})}


def twin():
    """The HeavyFlakTwin's measures (its frame, Blender's axes): per loader his side (+1 the breech to his left),
    his breech, the frame his clip is made in (square to the gun, 0.20 m to his side of the breech's line, as the
    single gun's loader's), his socket; round.ready; the hand-over over the rail; ammo.1's place."""
    if not TWIN:
        js = piece_json("fort", "HeavyFlakTwin")
        parts = {p["name"]: p for p in js["parts"]}
        g = lambda n: np.asarray(fsock("HeavyFlakTwin", n)["at"], float)
        TWIN["rack"] = g2b(parts["round.ready"]["pivot"])
        TWIN["ammo.1"] = crew_place("HeavyFlakTwin", "crew.ammo.1")
        pl1 = TWIN["ammo.1"]
        across = pl1.R @ _n(np.array((0.0, 1.0, 0.08)))       # (crosswise to number one, its nose to his left)
        TWIN["hand"] = bomb_frame(g2b(TWIN_HAND) - across * 0.465, across)
        for k in ("r", "l"):
            # (both made with the breech to their left: loader.l works inboard of breech.l, on the far side of it from
            # the fuze setter's machine, his work the mirror of what it was)
            side = 1.0
            br = g("breech." + k)
            fwd = -np.asarray(fsock("HeavyFlakTwin", "breech." + k)["facing"], float)
            fit = Place(Place(br, fwd).world((-0.57, -0.20 * side, -1.59)), fwd)
            TWIN["loader." + k] = {"side": side, "breech": br, "fit": fit,
                                   "socket": crew_place("HeavyFlakTwin", "crew.loader." + k)}
    return TWIN


def _canon(k):
    """For loader k: (S4, to_canon(world frame), from_canon(canonical frame)): his canonical frame is his fit frame,
    mirrored across his own x-z plane for loader.l, so that both are made as the single gun's (the breech to his
    left)."""
    L = twin()["loader." + k]
    S4 = M4(np.diag((1.0, L["side"], 1.0)), (0.0, 0.0, 0.0))
    F = L["fit"].matrix()
    return S4, (lambda M: S4 @ np.linalg.inv(F) @ M @ S4), (lambda Mc: S4 @ Mc @ S4)


def twin_round(t, k):
    """Loader k's round in his canonical frame (a 4x4) and whether it shows: held ready while the guns fire, swung up
    and its nose put into the open breech, rammed home, gone; then the next: for loader.r out of the rack (round.ready),
    for loader.l from ammo.1's hands over the rail, carried round to the ready."""
    T = TWIN_T[k]
    dt = T["dt"]
    S4, canon, _ = _canon(k)
    L = twin()["loader." + k]
    br = (S4 @ np.append(L["fit"].local(L["breech"]), 1.0))[:3]
    rd = LUNGE["ready"] if k == "r" else TWIN_L["ready"]
    ready = bomb_frame(np.array(rd[0]), _n(np.array(rd[1])))
    raised = bomb_frame(br + np.array(LUNGE["raised"][0]), _n(np.array(LUNGE["raised"][1])))
    aligned = bomb_frame(br + np.array((LUNGE["aligned"], 0.0, -0.03)), (1.0, 0.0, 0.0))
    home = bomb_frame(br + np.array((0.02, 0.0, 0.0)), (1.0, 0.0, 0.0))
    W = T["ready"]
    if W <= t < T["home"] + 0.02:
        keys = [(W, ready), (0.95 + dt, ready), (1.45 + dt, raised), (1.75 + dt, aligned), (T["home"], home)]
        return slerp_frames(keys, t), True
    if W <= t < T["take"]:
        return home, False
    tt = t + T["long"] if t < W else t
    if k == "r":
        rack_at = (S4 @ np.append(L["fit"].local(twin()["rack"]), 1.0))[:3]
        rack = bomb_frame(rack_at, (0.0, 0.0, 1.0), up=(1.0, 0.0, 0.0))
        up = bomb_frame(rack_at + np.array((0.04, 0.0, 0.16)), (0.15, 0.0, 1.0), up=(1.0, 0.0, 0.0))
        if tt < T["grab"]:
            return rack, True                        # (standing in the rack, where round.ready stood, till he lifts it)
        keys = [(T["take"], rack), (T["grab"], rack), (T["up"], up), (T["long"] + W, ready)]
        return slerp_frames(keys, tt), True
    H = canon(twin()["hand"])
    if tt < T["grab"]:
        return H, False                              # (ammo.1 holds it up till he has it)
    ax = _n(np.array((-0.99, -0.09, 0.15)))
    (cx, cy), cyaw = TWIN_L["carry"]["at"], TWIN_L["carry"]["yaw"]
    fw = np.array((math.cos(math.radians(cyaw)), math.sin(math.radians(cyaw)), 0.0))
    ax = _n(np.array((fw[1], -fw[0], 0.15)))           # (crosswise before his chest, its nose to his right)
    c = np.array((cx, cy, 1.0)) + 0.28 * fw
    carry = bomb_frame(c - 0.465 * ax, ax)
    keys = [(T["take"], H), (T["grab"], H), (T["carry"], carry), (T["long"] + W, ready)]
    return slerp_frames(keys, tt), True


def _twin_yaw(k, t):
    """Loader k's body's turn (canonical degrees) at t: loader.l turns round to the rail and back."""
    if k == "r":
        return 0.0
    T = TWIN_T["l"]
    tt = t + T["long"] if t < T["ready"] else t
    return float(np.interp(tt, [2.35, 2.62, T["take"], T["grab"] + 0.04, T["carry"], T["long"] + T["ready"]],
                           [0.0, TWIN_L["turn1"]["yaw"], TWIN_L["take"]["yaw"], TWIN_L["take"]["yaw"], TWIN_L["carry"]["yaw"], 0.0]))


def _mirror_hand(h, side):
    """A hand's target made in the canonical frame, said in his own (y mirrored for loader.l: the hand is then the
    other one)."""
    if side > 0:
        return h
    if "blend" in h:
        return {"blend": [(w, _mirror_hand(hh, side)) for w, hh in h["blend"]]}
    m = lambda v: (v[0], -v[1], v[2])
    out = dict(h)
    for key in ("at", "a", "n", "pole_world"):
        if key in h:
            out[key] = m(h[key])
    return out


def _sbm(side, bend=0.0, turn=0.0, lean=0.0, shift=(0.0, 0.0), drop=0.0, neck=(0.0, 0.0), feet=None, yaw=0.0, at=(0.0, 0.0)):
    """stand_body said canonically (the breech to his left), mirrored for loader.l; turned by `yaw` about his root
    and set down at `at` (canonical)."""
    if side < 0:
        turn, lean, shift, neck = -turn, -lean, (shift[0], -shift[1]), (neck[0], -neck[1])
        if feet:
            feet = {"l": (feet["r"][0], -feet["r"][1], -feet["r"][2]), "r": (feet["l"][0], -feet["l"][1], -feet["l"][2])}
        yaw, at = -yaw, (at[0], -at[1])
    P = stand_body(bend=bend, turn=turn, side=lean, shift=shift, drop=drop, neck=neck, feet=feet)
    if yaw or at[0] or at[1]:
        P = yawed(P, yaw, (at[0], at[1], 0.0))
    return P


def _twin_loader(t, k, name):
    """stand_load_breech_twin (k "r") and stand_load_breech_twin_l (k "l"): the lunge (LUNGE) at the twin's breech,
    both made with the breech to their left; loader.r's next round from the ready rack, loader.l's from ammo.1's
    hands over the rail behind him."""
    L = twin()["loader." + k]
    T = TWIN_T[k]
    dt, side = T["dt"], L["side"]
    rig = MODEL["rig"]
    free = (1, 1, 1, 1, 0, 1)
    S4, canon, uncanon = _canon(k)
    near, far = ("r", "l") if side > 0 else ("l", "r")          # (the real hands holding the canonical right and left)

    ram, sup = ("r", "l") if k == "r" else ("l", "r")    # (the hand that rams, and the one under the round's front)
    pole_on = {"l": (-0.2, 1.0, -1.0), "r": (-0.4, -1.0, -0.6)}

    def dress(t, P):
        Bc, shown = twin_round(t, k)
        Bh = twin_round(T["take"] if k == "r" else T["grab"], k)[0] if T["home"] + 0.02 <= t < T["take"] + (0.0 if k == "r" else 0.1) else Bc
        under = np.array((0.0, 0.0, 0.6))
        Ry = Rz(_twin_yaw(k, t))                                 # (his elbows' poles turn with him)
        tt = t + T["long"] if t < T["ready"] else t
        u = ramp(tt, T["grab"] + 0.03, T["long"] + 0.15) if tt >= T["take"] - 0.3 else 1.0
        # (along the round from its base: the ramming hand nearer the base, the other nearer the nose)
        if k == "l":
            x_ram, x_sup = 0.1 + (0.28 - 0.1) * u, 0.76 + (0.6 - 0.76) * u      # (outside ammo.1's hands, then to carry)
        else:
            x_ram, x_sup = 0.55 + (0.28 - 0.55) * u, 0.75 + (0.6 - 0.75) * u
        # (as he raises it onto the breech's line: the ramming palm near the base, the other under its middle, sliding
        # back along it as its nose goes into the breech, and letting go of it short of the face)
        if 0.9 + dt <= t < T["home"] + 0.02:
            g = ramp(t, 0.95 + dt, 1.4 + dt)
            x_sup, x_ram = x_sup + (0.42 - x_sup) * g, x_ram + (0.12 - x_ram) * g
            x_sup -= 0.2 * ramp(t, 1.6 + dt, 1.85 + dt)
        on = {h: on_rod(h, Bh, x_ram if h == ram else x_sup, 0.05 if h == "l" else 0.055, under, thumb=1.0,
                        pole=Ry @ np.array(pole_on[h])) for h in ("l", "r")}
        Bf = twin_round(T["home"], k)[0] if t >= T["home"] else Bc
        base, ax = Bf[:3, 3], Bf[:3, 0]
        # (the fist on the case's base, its palm pushing along the bore; drawn straight back off it and down as the
        # breech closes, turning palm down on the way (so that it goes on to his side without the wrist rolling over))
        pull = 0.3 * ramp(t, T["home"] + 0.02, T["home"] + 0.18) if t >= T["home"] else 0.0
        q = math.radians(90.0) * pull / 0.3
        a0, n0 = _n(np.array((0.0, 0.0, 1.0)) - ax * ax[2]), ax
        fist = {"at": tuple(base - ax * (0.03 + pull) - np.array((0.0, 0.0, 0.3 * pull))),
                "a": tuple(math.cos(q) * a0 + math.sin(q) * n0), "n": tuple(math.cos(q) * n0 - math.sin(q) * a0),
                "pole_world": (-0.3, -1.0 if ram == "r" else 1.0, -0.8)}
        w_in = ramp(t, 1.55 + dt, 1.8 + dt) if 1.0 <= t < T["home"] + 0.02 else 0.0
        rest = hands_rest_pinned(rig, P)
        lg = LOADER["let_go"] + dt
        let_go = on_rod(sup, twin_round(lg, k)[0], 0.22, 0.05 if sup == "l" else 0.055, under, thumb=1.0, pole=Ry @ np.array(pole_on[sup]))
        # (on the next round by T_end: loader.r's in the rack, loader.l's in ammo.1's hands; `on` holds it then)
        T_end = T["grab"] if k == "r" else T["take"]
        lead = 0.35 if k == "r" else 0.4
        if T["home"] + 0.02 <= t < T_end + 0.05:
            # (one hand's target blended with one other at a time: fist to his side, his side to the next round)
            w_a = ramp(t, T["home"] + 0.15, T["home"] + 0.3)
            w_b = ramp(t, max(T_end - lead, T["home"] + 0.32), T_end)
            w_ram = (1.0 - w_a) * (1.0 - w_b)
            hn = blend_hand([(w_ram, fist), (w_a * (1.0 - w_b), rest[ram]), (w_b, on[ram])])
        else:
            w_ram = w_in
            hn = blend_hand([(1.0 - w_ram, on[ram]), (w_ram, fist)])
        if lg <= t < T_end + 0.05:
            w_a = ramp(t, lg, lg + 0.45)
            w_b = ramp(t, max(T_end - lead, lg + 0.47), T_end)
            hf = blend_hand([((1.0 - w_a) * (1.0 - w_b), let_go), (w_a * (1.0 - w_b), rest[sup]), (w_b, on[sup])])
        else:
            hf = on[sup]
        P["hand_" + ram], P["hand_" + sup] = hn, hf
        P["fingers_" + sup], P["fingers_" + ram] = 62.0, 62.0 - 30.0 * w_ram
        P["props"] = {"flak_round": (uncanon(Bc), shown)}
        return P

    sb = lambda **kw: _sbm(side, **kw)
    poses = LUNGE_POSE if k == "r" else TWIN_L
    clear, line, setp, lunge, back = (sb(**poses[n]) for n in ("clear", "line", "set", "lunge", "back"))
    if k == "r":
        # (round.ready stands 0.31 further out than the single gun's: he stands out to it, in front of the rack)
        rack = sb(bend=46.0, turn=-80.0, lean=-6.0, shift=(-0.02, -0.36), drop=0.13, neck=(26.0, -30.0),
                  feet={"l": (0.08, -0.20, 8.0), "r": (-0.28, -0.50, -34.0)})
        rise = sb(bend=28.0, turn=-62.0, lean=-4.0, shift=(-0.04, -0.38), drop=0.07, neck=(16.0, -24.0),
                  feet={"l": (0.08, -0.20, 8.0), "r": (-0.28, -0.50, -34.0)})
        keys = [(0.0 + dt, S.mix([rise, clear], [0.35, 0.65])), (T["ready"], clear), (0.95 + dt, clear), (1.4 + dt, line),
                (1.5 + dt, line), (1.75 + dt, setp), (T["home"], lunge), (2.35 + dt, back), (T["take"], rack),
                (T["grab"] + 0.02, rack), (3.0 + dt, rise)]
        plan_c = {"l": [(0.9 + dt, 1.15 + dt, clear, line, 0.05, None), (1.8 + dt, 2.05 + dt, setp, lunge, 0.06, None),
                        (2.35 + dt, 2.55 + dt, back, rack, 0.05, "y"), (3.2 + dt, 3.45 + dt, rack, clear, 0.05, None)],
                  "r": [(1.0 + dt, 1.3 + dt, clear, line, 0.07, None), (1.5 + dt, 1.7 + dt, line, setp, 0.05, None),
                        (2.45 + dt, 2.65 + dt, back, rack, 0.05, "x"), (2.98 + dt, 3.22 + dt, rack, clear, 0.07, None)]}
    else:
        # (he turns round to his right to the rail behind him, takes the round from ammo.1's hands at `grab`, and comes
        # back with it behind the recoil, to stand clear by `ready`)
        turn1, take, carry = (sb(**TWIN_L[n]) for n in ("turn1", "take", "carry"))
        L_ = T["long"]
        keys = [(T["carry"] - L_, carry), (T["ready"], clear), (0.95, clear), (1.4, line), (1.5, line), (1.75, setp),
                (T["home"], lunge), (2.35, back), (2.62, turn1), (T["take"], take), (T["grab"] + 0.04, take)]
        plan_c = {"r": [(0.9, 1.15, clear, line, 0.05, None), (1.8, 2.05, setp, lunge, 0.06, None),
                        (2.36, 2.52, back, turn1, 0.06, None), (2.70, 2.84, turn1, take, 0.10, None),
                        (3.20, 3.38, take, carry, 0.12, None), (3.58, 3.74, carry, clear, 0.06, None)],
                  "l": [(1.0, 1.3, clear, line, 0.07, None), (1.5, 1.7, line, setp, 0.05, None),
                        (2.45, 2.6, back, turn1, 0.06, None), (2.86, 3.0, turn1, take, 0.12, "y"),
                        (3.40, 3.56, take, carry, 0.14, None), (3.75, 3.9, carry, clear, 0.05, None)]}
    if keys[0][0] > 0.0:
        # (looped wants a key at 0: the pose the loop passes through there)
        t0k = keys[0][0]
        lp = looped([(tk - t0k, Pk) for tk, Pk in keys], T["long"])
        keys = [(0.0, lp((-t0k) % T["long"]))] + keys
    keys = reached_keys(name, keys, dress, free, long=T["long"])
    P = looped(keys, T["long"])(t)
    tt = t + T["long"] if t < T["ready"] else t
    for s_c, plan in plan_c.items():
        P = stepped(P, s_c if side > 0 else {"l": "r", "r": "l"}[s_c], tt, plan)
    return reframed(reach_fix(dress(t, P), free=free), L["fit"], L["socket"])


@crew_clip("stand_load_breech_twin", 3.3, loop=True, anchor="feet", weapon="none",
           place=[("fort", "HeavyFlakTwin", "crew.loader.r", (0, 0, 0))],
           events=[{"t": 0.0, "fx": "piece_clip", "clip": "fire", "cue": "play the HeavyFlakTwin's own `fire` clip with it"},
                   {"t": 1.85, "fx": "", "cue": "round_in"}, {"t": 2.3, "fx": "", "cue": "round_home"},
                   {"t": 2.7, "fx": "hide_part", "part": "round.ready", "cue": ""},
                   {"t": 0.7, "fx": "show_part", "part": "round.ready", "cue": ""}],
           about="The twin's right-hand loader (crew.loader.r; attach to the part `mount`, no offset), stand_load_breech's "
                 "work at breech.r, 0.1 s behind it as breech.r runs 0.1 behind breech.l in the twin's fire clip (3.3 s; play "
                 "both from the same moment): at his socket, clear of the recoil, the next round in his arms while the right "
                 "barrel runs back (0.1) and its case flies out (0.9); steps in to his left, rams the round home (2.3) and "
                 "draws his hand clear before breech.r closes (2.55); steps out to the ready rack, his hands on round.ready "
                 "at 2.7 (hide the part then: the twin's fire clip does not; the prop shows in its place, standing as it "
                 "stood), lifts it out from 2.82, up to the ready by 0.4. Show round.ready again at 0.7 (the rack refilled). "
                 "The guns laid level.")
def c_stand_load_breech_twin(t):
    return _twin_loader(t, "r", "stand_load_breech_twin")


@crew_clip("stand_load_breech_twin_l", 3.3, loop=True, anchor="feet", weapon="none",
           place=[("fort", "HeavyFlakTwin", "crew.loader.l", (0, 0, 0))],
           events=[{"t": 0.0, "fx": "piece_clip", "clip": "fire", "cue": "play the HeavyFlakTwin's own `fire` clip with it"},
                   {"t": 1.75, "fx": "", "cue": "round_in"}, {"t": 2.2, "fx": "", "cue": "round_home"},
                   {"t": 3.08, "fx": "", "cue": "round_taken"}],
           about="The twin's left-hand loader (crew.loader.l; attach to the part `mount`, no offset): "
                 "stand_load_breech_twin's mirror at breech.l, in step with it (the twin's fire clip from the same moment): "
                 "his round held upright before him at his socket while the guns fire and run out (to 0.75), swung up "
                 "and rammed home with the flat of his right hand (2.2), clear before breech.l closes (2.35). He has no ready "
                 "round: he walks back to the rail behind him (inboard of the fuze setter's machine, stepping over the "
                 "low strut), takes the next round from ammo.1's hands over it (stand_ammo_crate_hand_up) at 3.08, and "
                 "walks back with it through the next shot, well behind the recoil, at his socket again by 0.55. The "
                 "guns laid level.")
def c_stand_load_breech_twin_l(t):
    return _twin_loader(t, "l", "stand_load_breech_twin_l")


def twin_ammo_round(t):
    """ammo.1's round on the twin (world): out of the crate at his right (0.6), carried, held up over the rail in both
    hands (2.75) and taken from them by loader.l at 2.9."""
    tw = twin()
    pl = tw["ammo.1"]
    in_crate = bomb_frame(pl.world((-0.465, -0.575, 0.07)), pl.R @ np.array((1.0, 0.0, 0.0)))
    lifted_ = bomb_frame(pl.world((-0.4, -0.5, 0.5)), pl.R @ _n(np.array((1.0, 0.2, 0.15))))
    carried = bomb_frame(pl.world((-0.05, -0.38, 0.95)), pl.R @ _n(np.array((0.8, 0.6, 0.1))))
    across = pl.R @ _n(np.array((0.0, 1.0, 0.08)))
    raised = bomb_frame(pl.world((0.62, 0.0, 1.45)) - across * 0.465, across)
    keys = [(0.6, in_crate), (0.85, lifted_), (1.3, carried), (1.9, carried), (2.3, raised), (2.6, tw["hand"]), (2.75, tw["hand"])]
    return slerp_frames(keys, t), 0.6 <= t < AMMO1_GIVE


@crew_clip("stand_ammo_crate_hand_up", 3.3, loop=True, anchor="feet", weapon="none",
           place=[("fort", "HeavyFlakTwin", "crew.ammo.1", (0, 0, 0))],
           about="An ammunition man on the ground below a gun's platform, alone: squats to the crate at his right (a "
                 "prop, crew.json), lifts a round out (0.93), carries it two steps forward and lifts it up over the "
                 "rail, holding it there crosswise till the loader takes it (3.08: hidden), and goes straight back down "
                 "to the crate for the next. On the twin at crew.ammo.1, in step with stand_load_breech_twin_l and the "
                 "fire clip (3.3 s, from the same moment).")
def c_stand_ammo_crate_hand_up(t):
    return _ammo_crate_hand_up((t - AMMO1_SHIFT) % TWIN_T["l"]["long"])


def _ammo_crate_hand_up(t):
    """stand_ammo_crate_hand_up at its own times (the hand-over at AMMO1_GIVE)."""
    tw = twin()
    pl = tw["ammo.1"]
    rig = MODEL["rig"]
    free = (1, 1, 1, 1, 1, 1)
    grab = AMMO1_GIVE
    long = TWIN_T["l"]["long"]

    def dress(t, P):
        B, shown = twin_ammo_round(t)
        # (in the crate till 0.6; after the loader has it, the hands leave from where they gave it, drawn back off the
        # rail before they come down)
        # (and straight down to the crate for the next, his hands there by 0.55)
        if 0.6 <= t < grab:
            Bh = B
        else:
            gave = twin_ammo_round(grab)[0].copy()
            gave[:3, 3] += pl.R @ np.array((-0.28, 0.0, 0.02))
            if t >= grab and t < grab + 0.15:
                Bh = twin_ammo_round(grab)[0].copy()
                Bh[:3, 3] += pl.R @ np.array((-0.28, 0.0, 0.02)) * ramp(t, grab, grab + 0.15)
            else:
                tt = t + long if t < 0.6 else t
                low = gave.copy()
                low[:3, 3] = pl.world((0.5, -0.15, 0.95))
                Bh = slerp_frames([(grab + 0.15, gave), (grab + 0.45, low), (long + 0.55, twin_ammo_round(0.6)[0])], tt)
        hold = round_hands(pl, Bh, xs=(0.3, 0.6), ref=twin_ammo_round(2.3)[0])
        w = 1.0
        for s in ("l", "r"):
            P["hand_" + s] = to_local(pl, hold[s])
            P["fingers_" + s] = 40.0 + 25.0 * w
        P["props"] = {"flak_round": (np.linalg.inv(pl.matrix()) @ B, shown), "flak_crate": (M4(np.eye(3), (-0.05, -0.62, 0.0)), True)}
        return P

    stand = stand_body(bend=2.0, turn=-6.0, neck=(-10.0, 4.0))
    # (squatting to the round lying in the crate on the ground)
    crate = stand_body(bend=50.0, turn=-60.0, side=-8.0, shift=(-0.14, -0.12), drop=0.52, neck=(20.0, -20.0),
                       feet={"l": (0.08, 0.14, -10.0), "r": (-0.1, -0.2, -45.0)})
    carry = stand_body(bend=6.0, turn=-8.0, drop=0.03, neck=(8.0, 10.0), feet={"l": (0.05, 0.13, 6.0), "r": (-0.06, -0.14, -14.0)})
    fwd = 0.46
    carry2 = stand_body(bend=4.0, turn=0.0, shift=(fwd, 0.0), drop=0.03, neck=(-10.0, 0.0),
                        feet={"l": (fwd + 0.05, 0.13, 2.0), "r": (fwd - 0.06, -0.14, -8.0)})
    hand_up = stand_body(bend=-8.0, turn=4.0, shift=(fwd + 0.12, 0.02), neck=(-26.0, 0.0),
                         feet={"l": (fwd + 0.24, 0.12, 0.0), "r": (fwd - 0.06, -0.15, -10.0)})
    keys = [(0.0, S.mix([stand, crate], [0.55, 0.45])), (0.42, crate), (0.68, crate), (1.2, carry), (1.85, carry2), (2.45, hand_up),
            (grab + 0.04, hand_up), (3.05, stand)]
    keys = reached_keys("stand_ammo_crate_hand_up", keys, dress, free, long=long)
    P = looped(keys, long)(t)
    tt = t + long if t < 0.1 else t
    P = stepped(P, "r", tt, [(0.15, 0.38, stand, crate, 0.06, None), (0.75, 1.0, crate, carry, 0.06, None),
                             (1.45, 1.7, carry, carry2, 0.07, None), (2.82, 3.05, carry2, stand, 0.06, None)])
    P = stepped(P, "l", tt, [(1.18, 1.45, stand, carry2, 0.07, None), (2.05, 2.3, carry2, hand_up, 0.06, None),
                             (3.05, 3.3, hand_up, stand, 0.06, None)])
    return reach_fix(dress(t, P), free=free)


# ---- throw_flat: the dive to the ground under fire
THROW = {"ahead": 1.1}                # (where his hips come down: 1.1 m ahead of where his feet were)


def _moved(P, dx):
    """A pose moved forward by dx (the hips, the feet; the hands that ride on the rifle or the chest go with them)."""
    Q = dict(P)
    pa = Q.get("pelvis_at", (0, 0, 0))
    Q["pelvis_at"] = (pa[0] + dx, pa[1], pa[2])
    for s in ("l", "r"):
        f = dict(Q["foot_" + s])
        a = np.asarray(f["at"], float)
        f["at"] = (a[0] + dx, a[1], a[2])
        Q["foot_" + s] = f
    return Q


@crew_clip("throw_flat", 1.0, loop=False, anchor="feet", weapon="rifle",
           events=[{"t": 0.12, "fx": "", "cue": "kit_rattle"}, {"t": 0.5, "fx": "dust_puff", "socket": "chest", "cue": "body_fall"}],
           about="Throwing himself flat under fire: from standing with his rifle, one long step and a dive forward, the "
                 "rifle brought up from the port into his shoulder through the dive, down on his chest by 0.5 and into the firing position "
                 "lying (soldier.py's prone_base) by 0.85. His root stays where his feet were: at the end his hips are "
                 "1.1 m ahead of it (crew.json end_offset), where the prone clips' root goes.")
def c_throw_flat(t):
    D = THROW["ahead"]
    stand = S.base_stand()
    lunge = dict(S.base_stand(), pelvis_at=(0.18, 0.0, -0.2), pelvis=(26.0, 0.0, -4.0), spine_01=(8.0, 0, 0), spine_02=(8.0, 0, 0),
                 chest=(6.0, 0, 0), neck=(-14.0, 0, 0), head=(-10.0, 0, 0), weapon=S.port_arms(lift=0.04, out=0.06),
                 foot_l={"at": (0.5, 0.13, MAN["ankle"]), "yaw": 4.0, "pole": (1.0, 0.2, 0.2)},
                 foot_r={"at": (-0.12, -0.13, 0.155), "yaw": -10.0, "pitch": 30.0, "toe": 30.0, "pole": (1.0, -0.2, 0.2)})
    launch = dict(S.base_stand(), pelvis_at=(0.5, 0.0, -0.3), pelvis=(58.0, 0.0, -2.0), spine_01=(6.0, 0, 0), spine_02=(4.0, 0, 0),
                  chest=(2.0, 0, 0), neck=(-28.0, 0, 0), head=(-18.0, 0, 0), weapon=S.port_arms(lift=0.08, out=0.1),
                  clavicle_l=(10.0, 16.0), clavicle_r=(10.0, 16.0),
                  foot_l={"at": (0.5, 0.14, 0.16), "yaw": 4.0, "pitch": 40.0, "toe": 40.0, "pole": (1.0, 0.2, 0.0)},
                  foot_r={"at": (-0.2, -0.14, 0.32), "yaw": -10.0, "pitch": 70.0, "toe": 20.0, "pole": (1.0, -0.2, -0.4)})
    fly = dict(S.base_stand(), pelvis_at=(0.82, 0.0, -0.5), pelvis=(78.0, 0.0, -2.0), spine_01=(-4.0, 0, 0), spine_02=(-6.0, 0, 0),
               chest=(-6.0, 0, 0), neck=(-32.0, 0, 0), head=(-20.0, 0, 0), weapon=S.port_arms(lift=0.1, out=0.14),
               clavicle_l=(12.0, 18.0), clavicle_r=(12.0, 18.0),
               foot_l={"at": (0.05, 0.16, 0.3), "yaw": 6.0, "pitch": 110.0, "toe": 10.0, "pole": (0.0, 0.3, -1.0)},
               foot_r={"at": (-0.05, -0.16, 0.42), "yaw": -10.0, "pitch": 115.0, "toe": 10.0, "pole": (0.0, -0.3, -1.0)})
    hit = S.prone_base(arch=0.0)
    hit.update({"pelvis_at": (0.0, 0.03, S.PRONE_Z + 0.006), "neck": (-26.0, 0, 2.0), "head": (-16.0, -4.0, 2.0),
                "weapon": S.aim(pitch=4.0), "sight": 0.6})      # (just down: his head comes onto the sights as he settles)
    hit = _moved(grounded("throw_flat_hit", hit), D - 0.05)
    down = _moved(grounded("throw_flat", S.prone_base()), D)
    # (the rifle brought from the port up into the shoulder through the lunge, the launch and the flight, so the arms
    # carry it out ahead as he goes down: not swung from the port to the aim in the tenth of a second of the landing)
    lunge, launch, fly = (to_shoulder(lunge, 0.22, 0.0), to_shoulder(launch, 0.58, -15.0), to_shoulder(fly, 1.0, -5.0))
    keys = [(0.0, stand), (0.12, lunge), (0.26, launch), (0.4, fly), (0.5, hit), (0.85, down), (1.0, down)]
    P = S.keyed(keys)(t)
    if t <= 0.9:
        P = toes_up(P)
    if t >= 0.7:
        P = S.breathe(P, t, 1.4, 0.3)
    return P


def to_shoulder(P, w, pitch=0.0):
    """A pose with its rifle `w` of the way from where it is (the port) to the aim (soldier.aim: the butt in the
    shoulder), the hands going with it (from their grips at the port to those at the aim, as soldier.prone_base has
    them), so the arms move the rifle rather than the rifle jumping between two holds."""
    P = dict(P)
    aim_l = dict(S.HOLD_AIM["l"], slide=(-0.16, 0.0, 0.0), pole_world=(0.0, 0.3, -1.0))
    aim_r = dict(S.HOLD_AIM["r"], pole_world=(-0.1, -1.0, -1.3))
    P["weapon"] = S.blend_places([(1.0 - w, P["weapon"]), (w, S.aim(pitch=pitch))])
    P["hand_l"] = blend_hand([(1.0 - w, P["hand_l"]), (w, aim_l)])
    P["hand_r"] = blend_hand([(1.0 - w, P["hand_r"]), (w, aim_r)])
    return P


def toes_up(P, floor=0.0, give=0.003):
    """Between two grounded keys the boots can swing through the ground: lifted (the feet only) by what goes in."""
    rig, skin = MODEL["rig"], MODEL["skin"]
    pts, bones = skin.points(rig.solve(P))
    low = float(pts[np.isin(bones, ("foot_l", "foot_r", "ball_l", "ball_r")), 2].min())
    if low < floor - give:
        P = _shift_feet(P, 0.0, floor - give - low)
    return P


# ================================================================================================ the arms, tracked
# The working clips ask each hand for a grip point and a hand frame. A real man holds a round, a bomb or a handle
# with some play in it: his palm may turn on the round's side or under it, his fist roll round a handle. ARM_HOLD
# gives each hand's play (degrees about the palm's normal, `spin`, and about the line across the knuckles, `roll`).
# Every frame (60 a second) the hand frame within that play and the elbow's swing round the shoulder-wrist line are
# chosen where his shoulder, elbow, forearm and wrist are easiest (soldier.arm_cost: rom.py's limits, the elbow kept
# out of his torso and the hand on its grip), and the choice is tracked through the clip: the cheapest path that
# moves at most a step a frame, then eased. So an arm never jumps from one way of reaching to another, and the hand
# keeps its grip point.
ROUND = {"spin": 180.0, "roll": 180.0, "top": True}   # (a palm on a round: turned on it and round it, dear on top of it)
ROD = {"spin": 45.0, "roll": 180.0}        # (a fist round a handle: rolled round it, the handle aslant in it)
ROD_WHEEL = {"spin": 45.0, "roll": 90.0, "behind": 0.04}    # (a fist on a handwheel's handle: rolled less, the forearm off the housing)
ARM_HOLD = {
    "mortar_number1": {"l": ROD, "r": ROD},
    "mortar_number2": {"l": ROUND, "r": ROUND},
    "mortar_number3": {"l": ROUND, "r": ROUND},
    "sit_handwheel_traverse": {"l": ROD_WHEEL, "r": ROD_WHEEL},
    "sit_handwheel_elevate": {"l": ROD_WHEEL, "r": ROD_WHEEL},
    "stand_load_breech": {"l": ROUND, "r": ROUND},
    "stand_load_breech_twin": {"l": ROUND, "r": ROUND},
    "stand_load_breech_twin_l": {"l": ROUND, "r": ROUND},
    "stand_ammo_give": {"l": ROUND, "r": ROUND},
    "stand_ammo_hand_up": {"l": ROUND, "r": ROUND},
    "stand_ammo_crate_hand_up": {"l": ROUND, "r": ROUND},
}
# (grid steps (degrees) of spin, roll and the elbow's swing; the cost of a degree squared of change from one frame to
# the next; the path eased over `ease` frames; the dearness of the elbow in the torso (cm squared), of a hand short of
# its grip (cm squared) and of a degree squared of play taken)
TRACK = {"steps": (15.0, 15.0, 7.5), "smooth": 0.01, "ease": 2.0, "torso": 100.0, "short": 400.0, "prior": 0.002,
         "fps": 60.0, "top": 0.5, "top_free": 15.0, "move": 0.05, "quick": 5.0, "quick_free": 9.0, "el_up": 27.0}
# (move: the dearness of a degree squared of any of the arm's angles changed from one frame to the next; quick: of a
# degree squared of a joint's turn in a frame past quick_free (rom.py's 900 deg/s is 15 a frame); el_up: rom.py's
# elbow-up limit, kept clear)
# (top: the dearness of a degree squared of a palm on a lying round facing down past top_free: on top of it, not under
# it or at its side)
TRACKED = {}
TRACK_V = 3                  # (the tracker's cost as rom.py's limits are kept clear: a new one re-tracks every clip)
TRACK_EASE = 3.0             # (the tracked path eased over this many frames (a Gaussian's sigma): its steps smoothed)
TRACK_QUICK = 7.0            # (a joint's turn in a frame past this is dear: TRACK's quick)
# (clips tracked with a lighter hand: eased less, a joint's turn freer; their version, ease and quick)
TRACK_CLIP = {k: dict(v=2, ease=2.0, quick=9.0) for k in ("sit_handwheel_traverse", "sit_handwheel_elevate", "mortar_number1",
                                                           "mortar_number2", "stand_ammo_hand_up")}
TRACK_CLIP["stand_ammo_crate_hand_up"] = dict(torso_m=0.03)     # (his elbows kept 3 cm off his flanks, not 1)
_TORSO_M = [0.01]


def _track_set(name):
    d = TRACK_CLIP.get(name, {})
    return d.get("v", TRACK_V), d.get("ease", TRACK_EASE), d.get("quick", TRACK_QUICK)


def _torso_cost(body, st, K):
    """Each candidate elbow K (N, 3) into his torso past its flesh, or behind his back, as rom.py tests it: cm squared
    (with a cm to spare)."""
    import rom
    c = np.zeros(len(K))
    for b, z0, z1, xb, xf, hw in body.torso:
        q = (K - st["H"][b]) @ st["D"][b] + body.H0[b]
        inz = (q[:, 2] >= z0) & (q[:, 2] < z1)
        xc, ax = 0.5 * (xb + xf), 0.5 * (xf - xb)
        dx, dy = q[:, 0] - xc, q[:, 1]
        e = np.sqrt((dx / ax) ** 2 + (dy / hw) ** 2)
        dist = np.hypot(dx, dy) * (1.0 - 1.0 / np.maximum(e, 1e-9))
        pen = (rom.ELBOW_R + _TORSO_M[0] - dist) * 100.0
        bk = np.where(np.abs(q[:, 1]) < hw * 0.9, (xb + 0.01 - q[:, 0]) * 100.0, -1.0)
        c = c + np.where(inz, np.maximum(pen, 0.0) ** 2 + np.maximum(bk, 0.0) ** 2, 0.0)
    return c


def _grid(lim, step):
    """A play's grid: -lim to lim, or (lim 180) the whole way round."""
    if lim >= 180.0:
        return np.arange(-180.0, 180.0, step)
    return np.arange(-lim, lim + 1e-6, step)


def _arm_grid(hold, steps=None):
    sp, sr, sw = steps or TRACK["steps"]
    return _grid(hold["spin"], sp), _grid(hold["roll"], sr), np.arange(-180.0, 180.0, sw)


def _rod_r(h):
    """How far a hand's grip point is out from the line it holds round (a round's or a handle's axis): on_rod's r."""
    if "blend" in h:
        ws = [(w, _rod_r(hh)) for w, hh in h["blend"]]
        return sum(w * r for w, r in ws) / max(sum(w for w, _ in ws), 1e-9)
    return float(h.get("rod_r", 0.0))


def _arm_build(rig, st, s, g, a1, n1, rr, psi, rho, phi):
    """For (spin, roll, swing) arrays (degrees, broadcast): the hand spun about its palm's normal at the grip and rolled
    round the held line (rr out from the grip, along the thumb), and the elbow swung: (Dh (N,3,3), grip (N,3), elbow K,
    wrist T, pole V, reach short (m))."""
    side = 1.0 if s == "l" else -1.0
    R0 = rig.arm0[s]
    a0, t0, n0 = (np.asarray(v, float) for v in rig.hand0[s])
    a1 = _n(np.asarray(a1, float))
    n1 = _n(np.asarray(n1, float) - a1 * (a1 @ np.asarray(n1, float)))
    t1 = np.cross(n1, a1)
    D0 = frame2(a1, n1) @ frame2(a0, n0).T
    g = np.asarray(g, float)
    c = g + n1 * rr
    psi, rho, phi = np.broadcast_arrays(*(np.atleast_1d(np.asarray(x, float)) for x in (psi, rho, phi)))
    up, ip = np.unique(psi, return_inverse=True)
    ur, ir = np.unique(rho, return_inverse=True)
    RP = np.stack([R_axis(n1, float(p)) @ D0 for p in up])
    RR = np.stack([R_axis(t1, float(r)) for r in ur])
    Dh = np.einsum("nij,njk->nik", RR[ir.ravel()], RP[ip.ravel()])
    G = c + (RR @ (g - c))[ir.ravel()]
    g_rest = S.grip_point(rig.J, s)
    Wr = G - np.einsum("rij,j->ri", Dh, g_rest - rig.H0["hand_" + s])
    A = st["H"]["upperarm_" + s]
    ref = st["D"]["chest"] @ np.array((-0.3, 0.6 * side, -1.0))
    l1, l2 = R0["l1"], R0["l2"]
    d = Wr - A
    dist = np.linalg.norm(d, axis=-1)
    dc = np.clip(dist, abs(l1 - l2) + 1e-4, (l1 + l2) * 0.9995)
    ul = d / dist[:, None]
    e1 = S._nv(ref - ul * (ul @ ref)[:, None])
    e2 = np.cross(ul, e1)
    ca = (l1 * l1 + dc * dc - l2 * l2) / (2 * l1 * dc)
    sa = np.sqrt(np.maximum(1 - ca * ca, 0.0))
    ph = np.radians(phi)
    V = np.cos(ph)[:, None] * e1 + np.sin(ph)[:, None] * e2
    K = A + l1 * (ul * ca[:, None] + V * sa[:, None])
    T = A + ul * dc[:, None]
    return Dh, G, K, T, V, dist - dc


def _wrap(x):
    return (np.asarray(x, float) + 180.0) % 360.0 - 180.0


def _arm_costs(rig, body, st, s, g, a1, n1, rr, hold, sig=(0.0, 0.0), steps=None):
    """Every (spin, roll, swing) on the grid: the arm's cost, (n_spin, n_roll, n_swing). sig: how far the ask itself
    has spun and rolled since the clip began (the grid's spin and roll then the hand's own, not the ask's)."""
    R0 = rig.arm0[s]
    ps, rs, fs = _arm_grid(hold, steps)
    P_, R_, F_ = np.meshgrid(_wrap(ps - sig[0]), _wrap(rs - sig[1]), fs, indexing="ij")
    Dh, G, K, T, V, short = _arm_build(rig, st, s, g, a1, n1, rr, P_.ravel(), R_.ravel(), F_.ravel())
    A = st["H"]["upperarm_" + s]
    a = S._nv(K - A)
    nn = S._nv(V - a * np.sum(V * a, -1, keepdims=True))
    Dup = np.stack((a, nn, np.cross(a, nn)), axis=-1) @ R0["Fu"].T
    Ms = np.diag((1.0, -1.0 if s == "r" else 1.0, 1.0)) @ st["D"]["chest"].T
    an = S.arm_angles_batch(Ms, A, K, T, Dup, Dh, R0["flex"], R0["a"], R0["t"], R0["n"])
    c = S.arm_cost(an) + TRACK["torso"] * _torso_cost(body, st, K) + TRACK["short"] * (100.0 * short) ** 2 + \
        TRACK["prior"] * (P_.ravel() ** 2 + R_.ravel() ** 2)
    if hold.get("behind"):
        # (a handwheel's fist: the elbow kept `behind` back of the handle along the wheel's axis (his own x), the
        # forearm coming onto the handle from his side of the wheel, not lying across its rim)
        c = c + TRACK["torso"] * (100.0 * np.maximum(K[:, 0] - (g[0] - hold["behind"]), 0.0)) ** 2
    # (rom.py's own limits, kept 3 degrees clear: a degree squared past that a hundred times dearer)
    m3 = 3.0
    c = c + 100.0 * (np.maximum(an["wrist_flex"] - (75.0 - m3), 0) ** 2 + np.maximum(-an["wrist_flex"] - (65.0 - m3), 0) ** 2 +
                     np.maximum(an["wrist_dev"] - (20.0 - m3), 0) ** 2 + np.maximum(-an["wrist_dev"] - (30.0 - m3), 0) ** 2 +
                     np.maximum(an["pronation"] - (80.0 - m3), 0) ** 2 + np.maximum(-an["pronation"] - (85.0 - m3), 0) ** 2 +
                     np.maximum(an["elbow"] - (145.0 - m3), 0) ** 2 + np.maximum(an["rotation"] - (70.0 - m3), 0) ** 2 +
                     np.maximum(-an["rotation"] - (90.0 - m3), 0) ** 2 + np.maximum(an["medial"] - (40.0 - m3), 0) ** 2 +
                     np.clip((an["elevation"] - 90.0) / 5.0, 0, 1) * (an["plane"] < 45.0) * np.maximum(an["rotation"] + m3, 0) ** 2)
    # (rom.py's elbow pointing up: the elbow's apex over the level, the arm bent)
    u = S._nv((K - A) @ Ms.T)
    lo = S._nv((T - K) @ Ms.T)
    apex = S._nv(u - lo)
    el_up = np.degrees(np.arcsin(np.clip(apex[:, 2], -1.0, 1.0)))
    c = c + 10.0 * (an["elbow"] > 25.0) * np.maximum(el_up - TRACK["el_up"], 0.0) ** 2
    if hold.get("top"):
        # (no more on top of it than the ask has the palm)
        a0, t0, n0 = (np.asarray(v, float) for v in rig.hand0[s])
        down = np.degrees(np.arcsin(np.clip(-(Dh @ n0)[:, 2], -1.0, 1.0)))
        asked = max(math.degrees(math.asin(float(np.clip(-_n(np.asarray(n1, float))[2], -1.0, 1.0)))), 0.0)
        c = c + TRACK["top"] * np.maximum(down - asked - TRACK["top_free"], 0.0) ** 2
    # (what the arm's speed from frame to frame is reckoned from: the upper arm's way (degrees), its turn, the elbow,
    # the forearm's turn and the wrist)
    feats = np.concatenate((np.degrees(u), np.stack([an[k] for k in ("rotation", "elbow", "pronation", "wrist_flex",
                                                                      "wrist_dev")], -1)), -1)
    return c.reshape(P_.shape), feats.reshape(P_.shape + (8,)).astype(np.float32)


def _rotvec(C):
    import mocap
    q = mocap.mat_to_quat(C)
    v = q[1:]
    sn = float(np.linalg.norm(v))
    if sn < 1e-12:
        return np.zeros(3)
    return v / sn * 2.0 * math.atan2(sn, float(q[0]))


def _ask_twist(ask, loop, cyc):
    """How far the asked hand has spun about its palm's normal and rolled about the held line since the clip began
    (degrees, (F, 2)), on the axes that go the whole way round (cyc); over a loop, the spin and roll it ends with
    spread back through it, so that its last frame meets its first."""
    Fr = [frame2(a, n) for g, a, n, rr in ask]
    sig = np.zeros((len(Fr) + 1, 2))
    for k in range(1, len(Fr) + 1):
        rv = _rotvec(Fr[k - 1].T @ Fr[k % len(Fr)])
        sig[k] = sig[k - 1] + np.degrees((rv[1], -rv[2]))
    if loop:
        tot = sig[-1]
        sig = sig - np.outer(np.arange(len(sig)) / len(Fr), tot - 360.0 * np.round(tot / 360.0))
    sig = sig[:len(Fr)]
    return sig * np.array([1.0 if cyc[0] else 0.0, 1.0 if cyc[1] else 0.0])


def _track_path(U, cyclic=(False, False, True), Fe=None, quick=None):
    """The cheapest path through per-frame costs U (F, a, b, c) moving at most a step a frame in a and b and two in c
    (an axis marked cyclic goes round), each move paid for by how far it moves on the grid and (Fe (F, a, b, c, 8):
    each state's arm, _arm_costs) how fast it turns the arm's joints: the indices (F, 3)."""
    F, na, nb, nc = U.shape
    sp, sr, sw = TRACK["steps"]
    lam = TRACK["smooth"]
    moves = [(da, db, dc) for da in (-1, 0, 1) for db in (-1, 0, 1) for dc in (-2, -1, 0, 1, 2)]
    pen = [lam * ((da * sp) ** 2 + (db * sr) ** 2 + (dc * sw) ** 2) for da, db, dc in moves]

    def shifted(x, d, axis, n, cyc, fill=np.inf):
        if d == 0:
            return x
        if cyc:
            return np.roll(x, d, axis=axis)
        out = np.full(x.shape, fill, dtype=x.dtype)
        sl_dst = [slice(None)] * x.ndim
        sl_src = [slice(None)] * x.ndim
        sl_dst[axis] = slice(max(d, 0), n + min(d, 0))
        sl_src[axis] = slice(max(-d, 0), n - max(d, 0))
        out[tuple(sl_dst)] = x[tuple(sl_src)]
        return out

    def shift3(x, m, fill=np.inf):
        da, db, dc = moves[m]
        x = shifted(x, dc, 2, nc, cyclic[2], fill)
        x = shifted(x, db, 1, nb, cyclic[1], fill)
        return shifted(x, da, 0, na, cyclic[0], fill)

    def speed_cost(f0, f1):
        d = f1 - f0
        d[..., 3] = (d[..., 3] + 180.0) % 360.0 - 180.0
        d[..., 5] = (d[..., 5] + 180.0) % 360.0 - 180.0
        q = TRACK_QUICK if quick is None else quick
        up = np.sqrt(np.sum(d[..., :4] ** 2, -1))
        hand = np.sqrt(np.sum(d[..., 5:] ** 2, -1))
        return TRACK["move"] * np.sum(d * d, -1) + TRACK["quick"] * (np.maximum(up - q, 0.0) ** 2 +
                                                                     np.maximum(np.abs(d[..., 4]) - q, 0.0) ** 2 +
                                                                     np.maximum(hand - q, 0.0) ** 2)
    cost = U[0].copy()
    back = np.zeros(U.shape, np.int16)
    for k in range(1, F):
        best = np.full(cost.shape, np.inf)
        arg = np.zeros(cost.shape, np.int16)
        for m in range(len(moves)):
            c = shift3(cost, m) + pen[m]
            if Fe is not None:
                c = c + np.nan_to_num(speed_cost(shift3(Fe[k - 1], m, 0.0), Fe[k]), nan=1e9)
            better = c < best
            best = np.where(better, c, best)
            arg = np.where(better, m, arg)
        cost = best + U[k]
        back[k] = arg
    idx = np.zeros((F, 3), int)
    idx[-1] = np.unravel_index(int(np.argmin(cost)), cost.shape)
    for k in range(F - 1, 0, -1):
        da, db, dc = moves[back[k][tuple(idx[k])]]
        idx[k - 1] = ((idx[k][0] - da) % na, (idx[k][1] - db) % nb, (idx[k][2] - dc) % nc)
    return idx


def _ease(x, loop, sigma):
    """A path (F,) eased by a Gaussian of sigma frames (round the loop if it loops)."""
    if sigma <= 0:
        return x
    r = int(math.ceil(3 * sigma))
    w = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    w /= w.sum()
    if loop:
        xp = np.concatenate((x[-r:], x, x[:r]))
    else:
        xp = np.concatenate((np.full(r, x[0]), x, np.full(r, x[-1])))
    return np.convolve(xp, w, mode="valid")


def _ease_angle(deg, loop, sigma):
    r = np.radians(deg)
    return np.degrees(np.arctan2(_ease(np.sin(r), loop, sigma), _ease(np.cos(r), loop, sigma)))


def track_arms(name, fn):
    """The clip's arms tracked (see ARM_HOLD): {side: (spin, roll, swing) (F, 3), degrees} at 60 fps over one pass of
    the clip (F frames; a loop's last is its first)."""
    import hashlib
    import rom
    rig = MODEL["rig"]
    body = rom.Body.of_rig(rig)
    c = CREW[name]
    fps = TRACK["fps"]
    N = int(round(c["long"] * fps)) + 1
    loop = c["loop"]
    M = N - 1 if loop else N
    asks, sts = [], []
    for k in range(M):
        P = fn(k / fps)
        st = rig.solve(dict(P, comfort=False))
        ask = {}
        for s, side in (("l", 1.0), ("r", -1.0)):
            h = P["hand_" + s]
            g, a1, n1, pole = rig.resolve_hand(st, P, h, side)
            ask[s] = (np.asarray(g, float), np.asarray(a1, float), np.asarray(n1, float), _rod_r(h))
        asks.append(ask)
        sts.append(st)
    tv, tease, tquick = _track_set(name)
    _TORSO_M[0] = TRACK_CLIP.get(name, {}).get("torso_m", 0.01)
    key = hashlib.sha1(json.dumps([name, TRACK, ARM_HOLD[name], tv] + ([TRACK_CLIP[name]] if name in TRACK_CLIP and tv != 2 else []),
                                  sort_keys=True).encode() + np.round(np.array(
        [np.concatenate([np.concatenate(ask[s][:3] + (np.array([ask[s][3]]),)) for s in ("l", "r")] +
                        [st["H"]["upperarm_l"], st["H"]["upperarm_r"], st["D"]["chest"].ravel()])
         for ask, st in zip(asks, sts)]), 4).tobytes()).hexdigest()
    path = os.path.join(WORK, "arm_track", name + ".pkl")
    if os.path.exists(path):
        old = pickle.load(open(path, "rb"))
        if old.get("key") == key:
            return old["out"]
    out = {}
    for s in ("l", "r"):
        hold = ARM_HOLD[name][s]
        cyc = (hold["spin"] >= 180.0, hold["roll"] >= 180.0, True)
        sig = _ask_twist([ask[s] for ask in asks], loop, cyc)
        UF = [_arm_costs(rig, body, st, s, *ask[s], hold, sig[k]) for k, (ask, st) in enumerate(zip(asks, sts))]
        U = np.stack([u for u, f in UF])
        Fe = np.stack([f for u, f in UF])
        del UF
        if loop:
            idx = _track_path(np.concatenate((U, U, U)), cyc, np.concatenate((Fe, Fe, Fe)), tquick)
            seam = idx[2 * M] - idx[M]
            idx = idx[M:2 * M]
        else:
            idx = _track_path(U, cyc, Fe, tquick)
            seam = np.zeros(3, int)
        ps, rs, fs = _arm_grid(hold)
        ang = []
        for x, grid, cy in ((idx[:, 0], ps, cyc[0]), (idx[:, 1], rs, cyc[1]), (idx[:, 2], fs, True)):
            v = grid[x]
            ang.append(_ease_angle(v, loop, tease) if cy else _ease(v, loop, tease))
        ang = np.stack(ang, axis=1)
        if loop:
            ang = np.concatenate((ang, ang[:1]))
            sig = np.concatenate((sig, [sig[0] + 360.0 * np.round((sig[-1] - sig[0]) / 360.0)]))
        out[s] = (ang, sig)
        print("track_arms %s %s: cost along the path max %.0f mean %.0f; seam off %s" % (
            name, s, max(U[k][tuple(idx[k])] for k in range(M)), np.mean([U[k][tuple(idx[k])] for k in range(M)]),
            list(seam)), flush=True)
    os.makedirs(os.path.dirname(path) + "/", exist_ok=True)
    pickle.dump({"key": key, "out": out}, open(path, "wb"))
    return out


def _lerp_angle(x0, x1, u):
    d = (x1 - x0 + 180.0) % 360.0 - 180.0
    return x0 + u * d


def tracked(name, fn):
    """The clip's pose function with its arms tracked (track_arms): each hand on its grip, spun and rolled on what it
    holds and its elbow swung as tracked (interpolated between the 60ths), the solve told to take them as they are."""
    def run(t):
        P = fn(t)
        if name not in TRACKED:
            TRACKED[name] = track_arms(name, fn)
        tr = TRACKED[name]
        c = CREW[name]
        fps = TRACK["fps"]
        F = len(tr["l"][0])
        x = ((t % c["long"]) if c["loop"] else min(max(t, 0.0), c["long"])) * fps
        k0 = min(int(math.floor(x)), F - 1)
        k1 = min(k0 + 1, F - 1)
        u = x - k0
        rig = MODEL["rig"]
        st = rig.solve(dict(P, comfort=False))
        Q = dict(P, comfort=False)
        a0s = {}
        for s, side in (("l", 1.0), ("r", -1.0)):
            h = P["hand_" + s]
            g, a1, n1, pole = rig.resolve_hand(st, P, h, side)
            ang, sig = tr[s]
            psi, rho, phi = _lerp_angle(ang[k0], ang[k1], u)
            sg = (1 - u) * sig[k0] + u * sig[k1]
            Dh, G, K, T, V, short = _arm_build(rig, st, s, g, a1, n1, _rod_r(h), psi - sg[0], rho - sg[1], phi)
            a0, t0, n0 = (np.asarray(v, float) for v in rig.hand0[s])
            Q["hand_" + s] = {"at": tuple(float(v) for v in G[0]), "a": tuple(float(v) for v in Dh[0] @ a0),
                              "n": tuple(float(v) for v in Dh[0] @ n0), "pole_world": tuple(float(v) for v in V[0])}
        return Q
    return run


for _name in ARM_HOLD:
    CREW[_name]["fn_asked"] = CREW[_name]["fn"]          # (the hands as the clip asks them, untracked: for comparing)
    CREW[_name]["fn"] = tracked(_name, CREW[_name]["fn"])


# ================================================================================================ the vehicles' seats
def slide_part(objs, part, move_game):
    """Slide one part of a piece (its bone) by a vector in the game's axes (the HeavyFlak's breech, opened)."""
    arm = [o for o in objs if o.type == "ARMATURE"][0]
    pb = arm.pose.bones[part]
    Mw = M4(np.eye(3), g2b(move_game))
    rest = np.array(arm.matrix_world @ pb.bone.matrix_local)
    pb.matrix = Matrix((np.linalg.inv(np.array(arm.matrix_world)) @ Mw @ rest).tolist())
    bpy.context.view_layer.update()


MEASURED = ("M3Halftrack", "Truck", "Hanomag", "FlakHalftrack", "M16", "FlakTruck", "M7", "Sexton", "Hummel", "SU76", "M10",
            "M8", "SdKfz222", "ArmouredCar", "PanzerIV", "SelfPropelledGun", "AssaultGun", "25pdr", "Pak40", "ZiS3", "leFH18",
            "Howitzer105", "Gun75", "AntiTankGun")


def measure_seats():
    """What is under each crew socket of the vehicles: ray-cast straight down from 0.25 m above `at` to the first
    surface (the seat's top, or the floor), and on down from 0.45 m ahead of it to the floor in front of the seat."""
    out = {}
    for v in MEASURED:
        kit.fresh(FPS)
        objs = load_piece("vehicle", v)
        tree, bm = env_tree(objs)
        res = {}
        for name, s in sockets_of("vehicle", v).items():
            if not (name in ("driver", "rider.cab", "gunner", "commander") or name.startswith(("rider.", "crew."))):
                continue
            at = np.asarray(s["at"], float)
            hit = tree.ray_cast(Vector(at + np.array((0.0, 0.0, 0.25))), Vector((0.0, 0.0, -1.0)), 2.0)
            top = None if hit[0] is None else float(hit[0][2])
            f = _n(np.asarray(s["facing"], float) * np.array((1.0, 1.0, 0.0)))
            ahead = at + 0.45 * f
            fl = tree.ray_cast(Vector(ahead + np.array((0.0, 0.0, -0.05))), Vector((0.0, 0.0, -1.0)), 2.0)
            floor = None if fl[0] is None else float(fl[0][2])
            res[name] = {"at_z": round(float(at[2]), 3), "surface_under": None if top is None else round(top, 3),
                         "at_minus_surface": None if top is None else round(float(at[2]) - top, 3),
                         "floor_ahead": None if floor is None else round(floor, 3),
                         "seat_over_floor": None if (top is None or floor is None) else round(top - floor, 3)}
        out[v] = res
        bm.free()
    return out


# ================================================================================================ baking and the json
FIT_N = 2          # (prop tracks: every 2nd frame, 30 a second)


def game_matrix(M):
    """A frame in Blender's axes (y left) as the game's (y right): position and a quaternion (w, x, y, z)."""
    M = np.asarray(M, float)
    p = g2b(M[:3, 3])
    q = Matrix(M[:3, :3].tolist()).to_quaternion()
    return [round(float(p[0]), 4), round(float(p[1]), 4), round(float(p[2]), 4),
            round(q.w, 5), round(-q.x, 5), round(q.y, 5), round(-q.z, 5)]


def bake(m, names):
    """Every clip as its own action on the man's armature (as soldier.bake_clips: the pose solved every 60th of a
    second, plain keys, linear between; a loop's last frame is its first). The weapon bone is keyed to nothing in the
    clips that put the rifle away. Returns {clip: {short, props: {name: track}}}."""
    arm, rig = m["arm"], m["rig"]
    arm.animation_data_create()
    out = {}
    for name in names:
        t0 = time.time()
        c = CREW[name]
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        arm.animation_data.action = act
        rig.prev_q = {}
        N = int(round(c["long"] * FPS))
        worst, tracks = 0.0, {}
        for f in range(N + 1):
            t = 0.0 if (c["loop"] and f == N) else f / FPS
            P = c["fn"](t)
            st = rig.solve(P)
            worst = max([worst] + [abs(v) for k, v in st["short"].items() if k.startswith("arm")])
            rig.key(st, f)
            pb = arm.pose.bones["weapon"]
            pb.scale = (0.0, 0.0, 0.0) if c["weapon"] == "none" else (1.0, 1.0, 1.0)
            if f in (0, N):
                pb.keyframe_insert("scale", frame=f, group="weapon")
            if f % FIT_N == 0:
                for pn, v in P.get("props", {}).items():
                    M, shown = v if isinstance(v, tuple) else (v, True)
                    tracks.setdefault(pn, []).append([round(f / FPS, 4)] + game_matrix(M) + [1 if shown else 0])
        for fc in kit.fcurves_of(act):
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
        act.use_frame_range = True
        act.frame_start, act.frame_end = 0, N
        act["loop"] = c["loop"]
        act["anchor"] = c["anchor"]
        act["weapon"] = c["weapon"]
        act["events"] = json.dumps(c["events"], separators=(",", ":"))
        out[name] = {"short": round(worst, 4), "props": tracks}
        print(f"  baked {name}: {N + 1} frames in {time.time() - t0:.0f} s (hands short at most {worst * 100:.1f} cm)")
    arm.animation_data.action = None
    S.reset_pose(arm)
    return out


PROP_ABOUT = {
    "binoculars": "field glasses; origin between the eyepieces, the barrels along +x",
    "mortar_bomb": "81 mm bomb; origin at the tail's end, the nose along +x (0.335 m)",
    "flak_round": "88 mm round; origin at the case's base, the nose along +x (0.93 m)",
    "belt": "the machine gun's belt, rigid on the gun: its track is the gun's own frame (the weapon bone)",
    "ammo_box": "the belt's ammunition box, lid open; origin on the ground under it",
    "flak_crate": "a wooden crate of four 88 mm rounds; origin on the ground under it",
}


SEAT_H = {"sit_bench": BENCH_H, "sit_drive": CAB_H, "sit_cab": CAB_H, "sit_handwheel_traverse": 0.44, "sit_handwheel_elevate": 0.44}

# Where each clip fits beyond the pieces it was fitted at (the names are the job, not the vehicle).
FITS = {
    "sit_bench": "any rider on a bench facing inboard whose top is about 0.44 over the floor his boots are on (the M3's "
                 "the Truck's and the Hanomag's rider.N). Other heights: crew.pose(stance 'sit', seat_h).",
    "sit_drive": "a driver at a wheel 0.44 ahead of his seat point and 0.16 over it, tilted 36 degrees back, the seat 0.40 "
                 "over the floor (the M3's, the M16's, the Hanomag's and the FlakHalftrack's driver: in the two Sd.Kfz. 251 "
                 "his left boot goes 1.5 cm into the footwell's outer wall, see the issues). Other wheels: crew.pose with the "
                 "wheel's two grips.",
    "sit_cab": "the man beside a driver, his seat 0.40 over the floor (the M3's, the M16's, the Hanomag's and the "
               "FlakHalftrack's rider.cab: in the two Sd.Kfz. 251 his right calf goes 1.5 cm into the footwell's outer wall).",
    "commander_hatch": "a commander stood in a hatch, his forearms on its rim: the root (his feet) 1.17 m under that rim. "
                       "PanzerIV, ArmouredCar and AssaultGun commander: root_offset (0, 0, -0.92), his feet 0.92 under the "
                       "socket (the ArmouredCar's and the AssaultGun's lids open by their open_up clips; at the ArmouredCar "
                       "his chest 1.7 cm into the rim's front, see the issues). "
                       "Not the M8's and SdKfz222's open turrets (their commanders' hatch sockets, tried with root_offset "
                       "(0, 0, -0.92): 2.5-2.8 cm into the turret, see the issues), nor the open fighting compartments (the "
                       "M10, M16, FlakHalftrack, FlakTruck, M7, Sexton, Hummel, SU76 and SelfPropelledGun): their walls are "
                       "0.65-1.06 m over a floor he stands on, so their commanders stand there by crew.pose (crew.json `posed`).",
    "stand_mg": "a man stood at a pedestal or ring-mounted machine gun with spade grips 0.40 ahead of his feet and 1.08 up "
                "(the M3's gunner and the M7's crew.mg on its raised pulpit, both with no root_offset; at the M7 his hips "
                "go 1.8 cm into the pulpit's wall behind him, see the issues). Attach him to the gun's turning part.",
    "stand_mg_burst": "as stand_mg, firing.",
    "mg_pair_gunner": "a prone machine-gun team's gunner, anywhere on the ground.",
    "mg_pair_feeder": "the prone team's number two, on the gunner's root.",
    "mortar_number1": "an 8 cm mortar's number one (the Mortar and MortarPit fortifications).",
    "mortar_number2": "an 8 cm mortar's number two.",
    "mortar_number3": "an 8 cm mortar's number three, at the bomb box.",
    "sit_handwheel_traverse": "a gunner seated on a turning mount, turning a handwheel whose hub is 0.365 ahead of his seat "
                              "point, 0.26 to his right and 0.46 up, by two handles at 9 and 3 o'clock, 0.17 out (the "
                              "HeavyFlak's and HeavyFlakTwin's traverse layer, seat 0.44 over the footrest). Attach him to the mount. Other seats and wheels (the FlakHalftrack's, "
                              "FlakTruck's, M16's, SU76's and SelfPropelledGun's crew.gunner, who have a hand on each of "
                              "two wheels by the right knee): crew.pose(stance 'sit', seat_h, the grips), "
                              "crew.json `posed`.",
    "sit_handwheel_elevate": "as sit_handwheel_traverse, the hub 0.26 to his left (the HeavyFlak's elevation layer).",
    "stand_load_breech": "a loader stood beside a breech's path, the breech 0.59 ahead of his feet, 0.41 to his left and "
                         "1.59 up, stepping in to ram the round home with the flat of his hand and taking the next from a ready rack "
                         "0.23 behind him and 0.09 to his right (the HeavyFlak's crew.loader, which faces 8.5 degrees left "
                         "of the gun's line; these are square to his facing). The self-propelled guns' and the towed "
                         "guns' loaders (their breeches lower, and no ready rack where his clip has it): crew.pose "
                         "standing, looking at the breech (crew.json `posed`).",
    "stand_load_breech_twin": "a loader stood beside a breech's path, square to it, the breech 0.57 ahead of his feet, 0.30 "
                              "to his left and 1.59 up, the round in his ready rack 0.17 behind him and 0.63 to his right "
                              "(the HeavyFlakTwin's crew.loader.r; he works 0.1 s behind breech.l, as breech.r fires).",
    "stand_load_breech_twin_l": "its mirror, the breech 0.30 to his right, with no rack: he takes each round from "
                                "stand_ammo_crate_hand_up's hands 1.0 m behind him, 0.07 to his right and 1.24 up, over a "
                                "rail (the HeavyFlakTwin's crew.loader.l).",
    "stand_ammo_crate_hand_up": "an ammunition man alone on the ground behind a gun's platform, taking rounds from a crate "
                                "(the prop) at his right and handing them up over a rail 1.7 m above his feet, 0.84 m ahead "
                                "of his socket, 0.63 to its right and 1.90 up (the HeavyFlakTwin's crew.ammo.1, to "
                                "stand_load_breech_twin_l).",
    "stand_ammo_give": "an ammunition man taking rounds from a crate (the prop, with the clip) behind his right and holding "
                       "them out to his left, about 0.9 m, to stand_ammo_hand_up.",
    "stand_ammo_hand_up": "an ammunition man taking the round from stand_ammo_give on his right and handing it up over a rail "
                          "or wall about 1.7 m high in front of him (to a gun's loader).",
    "throw_flat": "any man with a rifle going to ground under fire; carry on with the soldier's prone clips.",
}


def clip_json(name, baked):
    c = CREW[name]
    d = {"long": c["long"], "loop": c["loop"], "anchor": c["anchor"], "weapon": c["weapon"], "about": c["about"],
         "fits": FITS.get(name, ""),
         "places": [{"kind": k, "piece": p, "socket": s, "root_offset": [round(float(x), 4) for x in off]} for k, p, s, off in c["place"]],
         "events": c["events"]}
    if name in SEAT_H:
        d["seat_h"] = round(float(SEAT_H[name]), 3)          # (his boots this far under the seat point)
    if name == "throw_flat":
        d["end_offset"] = [THROW["ahead"], 0.0, 0.0]
    if baked.get("props"):
        d["props"] = {pn: {"mesh": "props/%s.glb" % pn, "about": PROP_ABOUT.get(pn, ""),
                           "track_fields": ["t", "x", "y", "z", "qw", "qx", "qy", "qz", "shown"], "track": tr}
                      for pn, tr in baked["props"].items()}
    return d


# ================================================================================================ checking at the pieces
STAGES = {
    ("vehicle", "PanzerIV"): {"parts": [("hatch_commander", 105.0, (0, 1, 0), (-0.63, 0, 2.64))], "drop": "hatch",
                              "keep": lambda W: W[:, 2] > 2.63, "cupola": True},
    # (the armoured car's hull is solid: what is over the hatch's rim, 2.24, is his to clear; its lid open by its clip)
    ("vehicle", "ArmouredCar"): {"open_up": True, "keep": lambda W: W[:, 2] > 2.22},
    # (the assault gun's fighting box is solid too, from its floor at 1.02 to its roof at 1.92: over its hatch's rim, 2.08)
    ("vehicle", "AssaultGun"): {"open_up": True, "keep": lambda W: W[:, 2] > 2.06},
}
for _v in ("M7", "Sexton", "Hummel", "SU76", "SelfPropelledGun", "M10", "M8", "SdKfz222"):
    STAGES[("vehicle", _v)] = {"open_up": True}
# (the SdKfz222's and the M8's open turrets: the floor seen in each is the hull's roof, at 1.62 and 1.60; his feet go
# down through it into the hull, so only what is over it is his to clear)
STAGES[("vehicle", "SdKfz222")]["keep"] = lambda W: W[:, 2] > 1.62
STAGES[("vehicle", "M8")]["keep"] = lambda W: W[:, 2] > 1.60
CHECK_AT = {
    "sit_bench": [("vehicle", "M3Halftrack", "rider.1"), ("vehicle", "M3Halftrack", "rider.8"), ("vehicle", "Truck", "rider.2"),
                  ("vehicle", "Truck", "rider.9"), ("vehicle", "Hanomag", "rider.1"), ("vehicle", "Hanomag", "rider.5"),
                  ("vehicle", "Hanomag", "rider.8")],
    "mg_pair_gunner": [], "mg_pair_feeder": [], "throw_flat": [],
}


def open_up(objs):
    """A piece as it stands at the end of its own open_up clip (hatches, screens and lids open). False if it has none."""
    arm = [o for o in objs if o.type == "ARMATURE"]
    if not arm:
        return False
    arm = arm[0]
    names = {b.name for b in arm.data.bones}

    def bones_of(a):
        return {fc.data_path.split('"')[1] for fc in kit.fcurves_of(a) if fc.data_path.startswith("pose.bones[")}
    acts = [a for a in bpy.data.actions if a.name.split(".")[0] == "open_up" and bones_of(a) and bones_of(a) <= names]
    if not acts:
        return False
    act = acts[-1]
    if arm.animation_data is None:
        arm.animation_data_create()
    arm.animation_data.action = act
    try:
        if len(act.slots):
            arm.animation_data.action_slot = act.slots[0]
    except AttributeError:
        pass
    bpy.context.scene.frame_set(int(act.frame_range[1]))
    bpy.context.view_layer.update()
    return True


def stage_piece(kind, piece):
    objs = load_piece(kind, piece)
    if kind != "vehicle":
        paint_piece(objs, kind, piece)
    st = STAGES.get((kind, piece), {})
    if st.get("open_up"):
        open_up(objs)
    for part, deg, axis, piv in st.get("parts", []):
        pose_part(objs, part, deg, axis, piv)
    for part, mv in st.get("slides", []):
        slide_part(objs, part, mv)
    tree, bm = env_tree(objs, hatch_discs() if st.get("drop") == "hatch" else None)
    return objs, tree, bm, st


# (what the piece does under a clip, for the check: the HeavyFlak's own `fire` clip under the men who work in step with it,
# played from the same moment (the barrel's recoil, the breech opening, round.ready hidden while the loader's round is
# out of the rack); a layer's handwheel turned with his hands)
MOVES = {"stand_load_breech": ("fire",), "stand_ammo_hand_up": ("fire",), "stand_ammo_give": ("fire",),
         "stand_load_breech_twin": ("fire",), "stand_load_breech_twin_l": ("fire",), "stand_ammo_crate_hand_up": ("fire",),
         "sit_handwheel_traverse": ("spin", "handwheel.traverse"), "sit_handwheel_elevate": ("spin", "handwheel.elevate")}


def rest_pose(objs):
    """A piece's rig back at rest, no clip on it."""
    for o in objs:
        if o.type == "ARMATURE":
            if o.animation_data:
                o.animation_data.action = None
            for pb in o.pose.bones:
                pb.location, pb.rotation_quaternion, pb.scale = (0, 0, 0), (1, 0, 0, 0), (1, 1, 1)
    bpy.context.view_layer.update()


def piece_mover(clip, objs, kind, piece):
    """For the check: a function of t that sets the piece as it stands at t under `clip` and gives its BVH (and
    bmesh), or None where the piece stands still. The fire clip is the piece's own, its .glb's action; a handwheel is
    turned about its hub with the layer's hands (wheel_turn)."""
    mv = MOVES.get(clip)
    if mv is None or kind != "fort":
        return None
    arm = [o for o in objs if o.type == "ARMATURE"][0]
    names = [b.name for b in arm.data.bones]
    if mv[0] == "fire":
        def bones_of(a):
            return {fc.data_path.split('"')[1] for fc in kit.fcurves_of(a) if fc.data_path.startswith("pose.bones[")}
        acts = [a for a in bpy.data.actions if a.name.split(".")[0] == "fire" and bones_of(a) and bones_of(a) <= set(names)]
        if not acts:
            return None
        act = acts[-1]
        # (a part the clip's events hide (the twin's round.ready, which its fire clip leaves to the game))
        hide = [(e["part"], e["t"]) for e in CREW[clip]["events"] if e.get("fx") == "hide_part"]
        show = {e["part"]: e["t"] for e in CREW[clip]["events"] if e.get("fx") == "show_part"}

        def at(t):
            if arm.animation_data is None:
                arm.animation_data_create()
            arm.animation_data.action = act
            try:
                if len(act.slots):
                    arm.animation_data.action_slot = act.slots[0]
            except AttributeError:
                pass
            bpy.context.scene.frame_set(int(round(t * FPS)))
            for part, t_hide in hide:
                t_show = show.get(part, 0.0)
                gone = (t >= t_hide or t < t_show) if t_show < t_hide else (t_hide <= t < t_show)
                arm.pose.bones[part].location = (0.0, -60.0, 0.0) if gone else (0.0, 0.0, 0.0)
            bpy.context.view_layer.update()
            return env_tree(objs)
        return at
    part = mv[1]
    if part not in names:
        return None
    piv = next(p["pivot"] for p in piece_json(kind, piece)["parts"] if p["name"] == part)
    long = CREW[clip]["long"]

    def at(t):
        # (pose_part turns about Blender's axis here: (1, 0, 0) is the same axis in both, and wheel_turn says the
        # game's plus in Blender's degrees)
        pose_part(objs, part, wheel_turn(t, long), (1.0, 0.0, 0.0), piv)
        return env_tree(objs)
    return at


def check_hold(m, clip, Mw, tree, keep=None, step=6, mover=None):
    """check(), not counting the hand's own fist round its grip (finger, thumb and hand points within 7 cm of where
    the pose put the hand) and with the ground's own lowest point. With a mover (piece_mover), the piece is tested as
    it stands at each moment: the fire clip under the loader, a layer's handwheel turned with his hands."""
    c = CREW[clip]
    rig, skin = MODEL["rig"], MODEL["skin"]
    N = int(round(c["long"] * FPS))
    Mw = np.asarray(Mw)
    worst, worst_at, gworst, count = 0.0, None, 0.0, 0
    for f in range(0, N + 1, step):
        t = f / FPS
        if mover is not None:
            tree, bm_t = mover(t)
        P = c["fn"](t)
        st = rig.solve(P)
        pts, bones = skin.points(st)
        targets = {}
        for s, side in (("l", 1.0), ("r", -1.0)):
            h = P.get("hand_" + s)
            if h is not None:
                targets[s] = rig.resolve_hand(st, P, h, side)[0]
        wp = pts @ Mw[:3, :3].T + Mw[:3, 3]
        k = np.ones(len(wp), bool) if keep is None else keep(wp)
        for i in np.nonzero(k)[0]:
            p, b = wp[i], str(bones[i])
            loc, nrm, idx, dist = tree.find_nearest(Vector(p), 0.1)
            if loc is None or ((Vector(p) - loc).dot(nrm) >= 0.0 and dist > 0.004):
                continue
            if b.startswith(("hand_", "fingers_", "thumb_")) and b[-1] in targets and np.linalg.norm(pts[i] - targets[b[-1]]) < 0.07:
                continue
            if not inside(tree, p):
                continue
            if dist > 0.012:
                count += 1
            if dist > worst:
                worst, worst_at = dist, (round(t, 3), b)
        gworst = max([gworst] + list(grip_gaps(rig, st, P).values()))
        if mover is not None:
            bm_t.free()
    return {"deepest_m": round(worst, 4), "where": worst_at, "points_past_1_2cm": count, "hand_gap_m": round(gworst, 4)}


def pose_check(P, st, tree, Mw=None, keep=None):
    """A man posed by pose() (P, st) measured against a piece as it stands: the deepest he goes into it (not counting
    a hand's own fist round its grip), how far his hands and feet fell short, and how far his lowest point is off the
    surface under it (m: + floating, - sunk)."""
    rig, skin = MODEL["rig"], MODEL["skin"]
    Mw = np.asarray(P["_root"] if Mw is None else Mw)
    pts, bones = skin.points(st)
    targets = {}
    for s, side in (("l", 1.0), ("r", -1.0)):
        h = P.get("hand_" + s)
        if h is not None and h.get("frame") != "chest":
            targets[s] = rig.resolve_hand(st, P, h, side)[0]
    wp = pts @ Mw[:3, :3].T + Mw[:3, 3]
    k = np.ones(len(wp), bool) if keep is None else keep(wp)
    worst, worst_at, count = 0.0, None, 0
    for i in np.nonzero(k)[0]:
        p, b = wp[i], str(bones[i])
        loc, nrm, idx, dist = tree.find_nearest(Vector(p), 0.1)
        if loc is None or ((Vector(p) - loc).dot(nrm) >= 0.0 and dist > 0.004):
            continue
        if b.startswith(("hand_", "fingers_", "thumb_")) and b[-1] in targets and np.linalg.norm(pts[i] - targets[b[-1]]) < 0.07:
            continue
        if not inside(tree, p):
            continue
        if dist > 0.012:
            count += 1
        if dist > worst:
            worst, worst_at = dist, b
    # his boots: the lowest boot point over the surface straight under it
    feet = {}
    for s in ("l", "r"):
        sel = np.isin(bones, ("foot_" + s, "ball_" + s))
        if sel.any():
            j = int(np.argmin(wp[sel, 2]))
            p = wp[sel][j]
            hit = tree.ray_cast(Vector(p + np.array((0.0, 0.0, 0.3))), Vector((0.0, 0.0, -1.0)), 2.0)
            feet[s] = None if hit[0] is None else round(float(p[2] - hit[0][2]), 4)
    rep = pose_report(P, st)
    return {"deepest_m": round(worst, 4), "where": worst_at, "points_past_1_2cm": count,
            "hand_gap_m": round(max([0.0] + list(rep["hand_gap"].values())), 4), "leg_short_m": max(rep["leg_short"].values()),
            "boot_over_surface_m": feet}


def cupola_check(clip, Mw, step=6):
    """The deepest a man goes into the PanzerIV's cupola ring (cupola_gaps) over a clip: (m, (t, bone))."""
    c = CREW[clip]
    rig, skin = MODEL["rig"], MODEL["skin"]
    Mw = np.asarray(Mw)
    worst, at = 0.0, None
    for f in range(0, int(round(c["long"] * FPS)) + 1, step):
        pts, bones = skin.points(rig.solve(c["fn"](f / FPS)))
        wp = pts @ Mw[:3, :3].T + Mw[:3, 3]
        d, p = cupola_gaps(wp)
        if d > worst:
            worst, at = d, (round(f / FPS, 3), str(bones[int(np.argmin(np.linalg.norm(wp - p, axis=1)))]))
    return worst, at


def grip_gaps(rig, st, P):
    out = {}
    for s, side in (("l", 1.0), ("r", -1.0)):
        h = P.get("hand_" + s)
        if h is None or h.get("frame") == "chest":
            continue
        tg = rig.resolve_hand(st, P, h, side)[0]
        out[s] = float(np.linalg.norm(rig.at(st, "hand_" + s, S.grip_point(rig.J, s)) - tg))
    return out


def ground_check(clip, step=3):
    """For the clips on open ground: the lowest the man goes (m, below 0 is into the ground) and the hands' gap."""
    c = CREW[clip]
    rig, skin = MODEL["rig"], MODEL["skin"]
    N = int(round(c["long"] * FPS))
    low, low_at, gworst = 9.0, None, 0.0
    for f in range(0, N + 1, step):
        P = c["fn"](f / FPS)
        st = rig.solve(P)
        pts, bones = skin.points(st)
        k = int(np.argmin(pts[:, 2]))
        if pts[k, 2] < low:
            low, low_at = float(pts[k, 2]), (round(f / FPS, 3), str(bones[k]))
        gworst = max([gworst] + list(grip_gaps(rig, st, P).values()))
    return {"lowest_m": round(low, 4), "where": low_at, "hand_gap_m": round(gworst, 4)}


def check_everything(only_kind=None):
    """Every clip at the places it is for (or on the ground): returns {clip: [results]}. only_kind "vehicle": just
    the clips' places at the vehicles."""
    out = {}
    todo = {}
    for name, c in CREW.items():
        places = CHECK_AT.get(name)
        if places is None:
            places = [(k, p, s) for k, p, s, off in c["place"]]
        if not places:
            if only_kind is None:
                out[name] = [dict(ground_check(name), at="the ground")]
            continue
        for k, p, s in places:
            if only_kind is None or k == only_kind:
                todo.setdefault((k, p), []).append((name, s))
    for (kind, piece), jobs in todo.items():
        kit.fresh(FPS)
        setup()
        objs, tree, bm, st = stage_piece(kind, piece)
        socks = sockets_of(kind, piece)
        for name, sock in jobs:
            off = next((o for k, p, s, o in CREW[name]["place"] if p == piece and s == sock), (0, 0, 0))
            Mw = place_matrix(kind, piece, sock, off, socks)
            t0 = time.time()
            mover = piece_mover(name, objs, kind, piece)
            r = check_hold(MODEL["man"], name, Mw, tree, st.get("keep"), mover=mover)
            if mover is not None:
                rest_pose(objs)
                r["piece_moving"] = MOVES[name][0]
            if st.get("cupola"):
                # (the turret has no inside and its hatch is painted on: the mesh is tested above the rim, and the
                # cupola's ring from its well up to the rim measured as the ring it is)
                d, where = cupola_check(name, Mw)
                r["cupola_ring_m"] = round(d, 4)
                if d > r["deepest_m"]:
                    r["deepest_m"], r["where"] = round(d, 4), where
            r["at"] = f"{piece} {sock}"
            out.setdefault(name, []).append(r)
            print(f"  {name} at {piece} {sock}: {r} ({time.time() - t0:.0f} s)")
        bm.free()
    return out


# ================================================================================================ men posed where no clip fits
# The crew.pose call that puts a man at each vehicle socket no clip fits, as checked against the vehicle (its hatches
# and screens open by its own open_up clip): stance, seat_h, the grips (sockets) his hands go on, root ("hips": the
# socket is the floor under his hips), stool (the seat point seat_h up from a floor socket: a stool or box under him),
# off (where he fits from the socket: ahead, right, up of its facing; 0 where the socket is right), look (a socket),
# feet ({"l"/"r": (ahead, right, up)} from the socket along its facing: the floor under that ankle, where the stance's
# own place for the boot is in something).
SW = {"grips": ("grip.gunner.l", "grip.gunner.r")}
LW = {"grips": ("grip.layer.l", "grip.layer.r")}
ST = {"stance": "stand"}
POSED = {
    "Hanomag": {"gunner": dict(ST)},
    "FlakHalftrack": {"crew.gunner": dict(SW, stance="sit", seat_h=0.31), "commander": dict(ST), "crew.loader": dict(ST),
                      "crew.loader2": dict(ST)},
    "FlakTruck": {"crew.gunner": dict(SW, stance="sit", seat_h=0.31), "commander": dict(ST), "crew.loader": dict(ST),
                  "crew.loader2": dict(ST)},
    "M16": {"crew.gunner": dict(SW, stance="sit", seat_h=0.29), "commander": dict(ST), "crew.loader": dict(ST),
            "crew.loader2": dict(ST)},
    "M7": {"crew.gunner": dict(SW, stance="sit", seat_h=0.20, stool=True), "commander": dict(ST),
           "crew.loader": dict(ST, look="breech"), "crew.loader2": dict(ST, look="breech")},
    "Sexton": {"crew.gunner": dict(SW, stance="kneel", root="hips"), "commander": dict(ST),
               "crew.loader": dict(ST, look="breech"), "crew.loader2": dict(ST, look="breech")},
    "Hummel": {"crew.gunner": dict(SW, stance="stand"), "commander": dict(ST), "crew.loader": dict(ST, look="breech"),
               "crew.loader2": dict(ST, look="breech")},
    "SU76": {"crew.gunner": dict(SW, stance="sit", seat_h=0.39), "commander": dict(ST), "crew.loader": dict(ST, look="breech")},
    "SelfPropelledGun": {"crew.gunner": dict(SW, stance="sit", seat_h=0.45), "commander": dict(ST),
                         "crew.loader": dict(ST, look="breech")},
    "M10": {"commander": dict(ST, off=(-0.15, 0.0, 0.0)), "crew.gunner": dict(stance="sit", seat_h=0.28),
            "crew.loader": dict(ST, off=(0.0, -0.03, 0.0), look="breech")},
    "M8": {"crew.gunner": dict(stance="sit", seat_h=0.26)},
    "25pdr": {"crew.layer": dict(LW, stance="kneel", off=(0.0, 0.0, 0.03)), "crew.loader": dict(ST, look="breech"),
              "crew.commander": dict(ST), "crew.ammo": dict(ST)},
    "Pak40": {"crew.layer": dict(LW, stance="stand", off=(0.15, -0.3, 0.0)), "crew.loader": dict(ST, look="breech"),
              "crew.commander": dict(ST, off=(0.0, 0.15, 0.0)), "crew.ammo": dict(ST, off=(0.0, 0.15, 0.0))},
    "ZiS3": {"crew.layer": dict(LW, stance="stand", off=(0.15, -0.3, 0.0)), "crew.loader": dict(ST, look="breech"),
             "crew.commander": dict(ST), "crew.ammo": dict(ST)},
    "leFH18": {"crew.layer": dict(LW, stance="stand", off=(0.0, 0.04, 0.0)), "crew.loader": dict(ST, look="breech"),
               "crew.commander": dict(ST), "crew.ammo": dict(ST)},
    "Howitzer105": {"crew.layer": dict(LW, stance="stand", off=(0.0, -0.3, 0.0)), "crew.loader": dict(ST, look="breech"),
                    "crew.commander": dict(ST, off=(0.0, -0.3, 0.0)), "crew.ammo": dict(ST)},
    "Gun75": {"crew.layer": dict(LW, stance="kneel", off=(0.0, -0.15, 0.0)), "crew.loader": dict(ST, look="breech"),
              "crew.commander": dict(ST), "crew.ammo": dict(ST)},
    "AntiTankGun": {"crew.layer": dict(LW, stance="stand", off=(0.15, -0.3, 0.0)), "crew.loader": dict(ST, look="breech"),
                    "crew.commander": dict(ST, off=(0.0, 0.15, 0.0)), "crew.ammo": dict(ST, off=(0.0, 0.3, 0.0))},
}


def posed_call(SK, sock, spec):
    """crew.pose's arguments for a POSED entry (the piece's sockets SK, game axes): (args, kwargs)."""
    s = SK[sock]
    f = np.asarray(s.get("facing", (1, 0, 0)), float)
    f = np.array((f[0], f[1], 0.0)) / max(math.hypot(f[0], f[1]), 1e-9)
    r = np.array((-f[1], f[0], 0.0))                         # (game axes: the right of +x is +y)
    off = spec.get("off", (0.0, 0.0, 0.0))
    seat_h = spec.get("seat_h", 0.0)
    at = np.asarray(s["at"], float) + off[0] * f + off[1] * r + np.array((0.0, 0.0, off[2] + (seat_h if spec.get("stool") else 0.0)))
    g = spec.get("grips")
    gl = SK[g[0]]["at"] if g and g[0] in SK else None
    gr = SK[g[1]]["at"] if g and g[1] in SK else None
    look = SK[spec["look"]]["at"] if spec.get("look") in SK else None
    a0 = np.asarray(s["at"], float)
    feet = {k: [round(float(x), 4) for x in a0 + o[0] * f + o[1] * r + np.array((0.0, 0.0, o[2]))]
            for k, o in spec.get("feet", {}).items()} or None
    return (spec["stance"], seat_h, gl, gr, s.get("facing", (1, 0, 0))), dict(at=[round(float(x), 4) for x in at], look=look,
                                                                              root=spec.get("root", "feet"), feet=feet)


def posed_checks():
    """Each POSED man posed at his vehicle and checked (pose_check): {vehicle: {socket: {call, check}}}."""
    out = {}
    for v, socks in POSED.items():
        kit.fresh(FPS)
        m = setup()
        objs = load_piece("vehicle", v)
        open_up(objs)
        tree, bm = env_tree(objs)
        SK = {s["name"]: s for s in piece_json("vehicle", v)["sockets"]}
        res = {}
        for sock, spec in socks.items():
            if sock not in SK:
                continue
            args, kw = posed_call(SK, sock, spec)
            P, st = pose(m, *args, key=False, place=False, **kw)
            r = pose_check(P, st, tree)
            call = {"stance": args[0], "seat_h": args[1], "grip_l": spec.get("grips", (None, None))[0] if spec.get("grips") else None,
                    "grip_r": spec.get("grips", (None, None))[1] if spec.get("grips") else None, "root": kw["root"],
                    "at": kw["at"], "root_offset": list(spec.get("off", (0.0, 0.0, 0.0))), "stool": bool(spec.get("stool")),
                    "look": spec.get("look"), "feet": kw["feet"]}
            res[sock] = {"call": call, "check": r}
            print(f"  posed {v} {sock}: {r}", flush=True)
        out[v] = res
        bm.free()
    return out


# ================================================================================================ the pictures
PICTURES = {
    "m3_riders": {"kind": "vehicle", "piece": "M3Halftrack",
                  "men": [("sit_bench", 0.4 * i, "rider.%d" % i) for i in range(1, 11)] +
                         [("sit_drive", 1.0, "driver"), ("sit_cab", 2.6, "rider.cab"), ("stand_mg", 1.0, "gunner")],
                  "eye": (-5.4, -3.6, 4.6), "target": (-0.9, 0.0, 1.3), "lens": 35,
                  "about": "Riders on the M3's benches (sit_bench at rider.1-10), its driver (sit_drive), the man beside him "
                           "(sit_cab) and the gunner at the pedestal gun (stand_mg)."},
    "panzer_commander": {"kind": "vehicle", "piece": "PanzerIV", "men": [("commander_hatch", 7.9, "commander")],
                         "eye": (1.5, -2.3, 3.4), "target": (-0.38, 0.0, 2.85), "lens": 40,
                         "about": "The commander up in the PanzerIV's cupola (commander_hatch, the glasses raised), the hatch "
                                  "opened 105 degrees."},
    "flak_crew": {"kind": "fort", "piece": "HeavyFlak",
                  "men": [("sit_handwheel_traverse", 0.3, "crew.layer.traverse"), ("sit_handwheel_elevate", 0.7, "crew.layer.elevate"),
                          ("stand_load_breech", 1.8, "crew.loader"), ("stand_ammo_hand_up", 1.85, "crew.ammo.1"), ("stand_ammo_give", 1.85, "crew.ammo.2")],
                  "slides": [("breech", (0.0, 0.19, 0.0))],
                  "eye": (-6.6, -3.9, 4.0), "target": (-1.8, 0.0, 1.0), "lens": 35,
                  "about": "The HeavyFlak's crew at 1.8 s of the gun's cycle: the layers turning their handwheels, the loader "
                           "putting the round into the open breech, the ammunition men passing the next one along."},
    "flak_recoil": {"kind": "fort", "piece": "HeavyFlak",
                    "men": [("sit_handwheel_traverse", 0.1, "crew.layer.traverse"), ("sit_handwheel_elevate", 0.45, "crew.layer.elevate"),
                            ("stand_load_breech", 0.1, "crew.loader"), ("stand_ammo_hand_up", 0.1, "crew.ammo.1"), ("stand_ammo_give", 0.1, "crew.ammo.2")],
                    "slides": [("barrel", (-0.62, 0.0, 0.0)), ("round.ready", (0.0, 0.0, -60.0))],
                    "eye": (-5.4, -1.0, 3.7), "target": (-1.1, -0.15, 1.75), "lens": 40,
                    "about": "The HeavyFlak's crew 0.1 s after the shot, the barrel run back 0.62 m (as the gun's own fire clip "
                             "has it then; round.ready hidden, the loader holds the next round): the loader at crew.loader, "
                             "clear to the right of the recoil, the round in his arms."},
    "flak_twin": {"kind": "fort", "piece": "HeavyFlakTwin",
                  "men": [("sit_handwheel_traverse", 0.2, "crew.layer.traverse"), ("sit_handwheel_elevate", 0.6, "crew.layer.elevate"),
                          ("stand_load_breech_twin", 3.04, "crew.loader.r"), ("stand_load_breech_twin_l", 3.04, "crew.loader.l"),
                          ("stand_ammo_crate_hand_up", 3.04, "crew.ammo.1")],
                  "slides": [("round.ready", (0.0, 0.0, -60.0))],
                  "eye": (-6.2, -2.9, 3.2), "target": (-1.9, -0.35, 1.3), "lens": 35,
                  "about": "The HeavyFlakTwin's loaders at 3.04 s of the fire clip, both breeches closed: loader.l, back at "
                           "the rail, takes his next round from ammo.1's hands over it; loader.r lifts his out of the ready "
                           "rack (round.ready hidden, his prop round in its place); the layers at their wheels."},
    "hanomag_m7": {"kind": "vehicle", "piece": "Hanomag",
                   "men": [("sit_bench", 0.55 * i, "rider.%d" % i) for i in range(1, 11)] +
                          [("sit_drive", 1.0, "driver"), ("sit_cab", 2.6, "rider.cab")],
                   "also": [{"kind": "vehicle", "piece": "M7", "at": (0.6, -3.4), "yaw": 0.0, "open_up": True,
                             "men": [("stand_mg", 1.0, "crew.mg")], "posed": "POSED"}],
                   "eye": (5.6, -5.2, 6.6), "target": (-0.4, 1.6, 0.8), "lens": 28,
                   "about": "A Hanomag with her riders (sit_bench at rider.1-10), driver (sit_drive) and rider.cab (sit_cab), "
                            "and beside her an M7 (the 12:08 export, her screens open): crew.mg on stand_mg in the raised "
                            "pulpit, and the rest posed by crew.pose as crew.json `posed` has them: the gunner on a 0.20 "
                            "stool (the box under him) with his hands on the handwheels' grips, the commander, and the two "
                            "loaders looking at the breech."},
    "mortar_crew": {"kind": "fort", "piece": "Mortar",
                    "men": [("mortar_number1", 1.2, "crew.number1"), ("mortar_number2", 1.2, "crew.number2"), ("mortar_number3", 1.2, "crew.number3")],
                    "eye": (2.5, 2.3, 1.9), "target": (0.0, 0.0, 0.6), "lens": 35,
                    "about": "The mortar's crew as number two lets the bomb into the muzzle (1.2 s): number one at the sight, "
                             "number three at the bomb box."},
    "mortar_duck": {"kind": "fort", "piece": "Mortar",
                    "men": [("mortar_number1", 1.8, "crew.number1"), ("mortar_number2", 1.8, "crew.number2"), ("mortar_number3", 1.8, "crew.number3")],
                    "eye": (2.5, 2.3, 1.9), "target": (0.0, 0.0, 0.6), "lens": 35,
                    "about": "The same at 1.8 s, the shot just gone: all three ducked away from the muzzle."},
    "mg_pair": {"kind": "ground", "men": [("mg_pair_gunner", 0.3, None), ("mg_pair_feeder", 0.3, None)],
                "eye": (2.7, -1.7, 1.5), "target": (0.35, 0.3, 0.2), "lens": 35,
                "about": "The machine-gun pair lying on one root: the gunner firing, his number two feeding the belt from its box."},
    "throw_flat": {"kind": "ground", "men": [("throw_flat", t, None) for t in (0.0, 0.12, 0.26, 0.4, 0.5, 0.85)],
                   "strip": 1.7, "eye": (5.15, -9.6, 1.25), "target": (5.15, 0.0, 0.45), "lens": 28,
                   "about": "throw_flat at 0, 0.12, 0.26, 0.4, 0.5 and 0.85 s (each frame drawn 1.7 m further along than the last)."},
    "pose_machinegun": {"kind": "fort", "piece": "MachineGun", "pose": True,
                        "eye": (1.7, -2.3, 1.5), "target": (-0.7, 0.1, 0.35), "lens": 35,
                        "about": "crew.pose() at the MachineGun's sockets: crew.gunner sat on the ground (seat_h 0) with his "
                                 "hands at grip.gunner.l/r; crew.loader kneeling, looking along the gun."},
}


def build_picture(name, fast=False):
    spec = PICTURES[name]
    kit.fresh(FPS)
    m = setup()
    S.studio_smooth(m)
    objs, socks = [], {}
    if spec["kind"] != "ground":
        objs = load_piece(spec["kind"], spec["piece"])
        if spec["kind"] != "vehicle":
            paint_piece(objs, spec["kind"], spec["piece"])
        st = STAGES.get((spec["kind"], spec["piece"]), {})
        for part, deg, axis, piv in st.get("parts", []):
            pose_part(objs, part, deg, axis, piv)
        for part, mv in spec.get("slides", []):
            slide_part(objs, part, mv)
        socks = sockets_of(spec["kind"], spec["piece"])
    lmg = None
    if spec.get("pose"):
        for sname in ("crew.gunner", "crew.loader"):
            s = piece_json(spec["kind"], spec["piece"])["sockets"]
            s = {x["name"]: x for x in s}
            sk = s[sname]
            cm = clone(m, with_weapon=True)
            gl = s.get("grip.gunner.l", {}).get("at") if sname == "crew.gunner" else None
            gr = s.get("grip.gunner.r", {}).get("at") if sname == "crew.gunner" else None
            pose(cm, sk.get("stance", "stand"), sk.get("seat_h", 0.0), gl, gr, sk.get("facing", (1, 0, 0)), sk["at"],
                 look=s["muzzle"]["at"])
    for k, (clip, t, sock) in enumerate(spec.get("men", [])):
        if spec["kind"] == "ground":
            Mw = M4(np.eye(3), (spec["strip"] * k, 0.0, 0.0)) if spec.get("strip") else np.eye(4)
        else:
            off = next((o for kk, p, s, o in CREW[clip]["place"] if p == spec["piece"] and s == sock), (0, 0, 0))
            Mw = place_matrix(spec["kind"], spec["piece"], sock, off, socks)
        mm = m
        if CREW[clip]["weapon"] == "lmg":
            if lmg is None:
                gun, bipod = S.build_lmg()
                Wm = Matrix.Translation(Vector(m["arm"].data.bones["weapon"].head_local))
                for ob, bone in ((gun, "weapon"), (bipod, "bipod")):
                    ob.data.transform(Wm)
                    S.mark_rest(ob)
                    S.skin_rigid(ob, bone)
                    S.attach(ob, m["arm"])
                lmg = dict(m, weapon=gun, weapon_part=bipod)
            mm = lmg
        man_at(mm, clip, t, Mw)
        mv = MOVES.get(clip)
        if mv and mv[0] == "spin" and spec["kind"] == "fort":
            # (a layer's handwheel turned as far as his hands have turned it, its handles in his fists)
            piv = next(p["pivot"] for p in piece_json(spec["kind"], spec["piece"])["parts"] if p["name"] == mv[1])
            pose_part(objs, mv[1], wheel_turn(t, CREW[clip]["long"]), (1.0, 0.0, 0.0), piv)
    for extra in spec.get("also", []):
        # (a second piece beside the first, at (x, y) and turned `yaw` degrees, the game's axes: its men on clips, and
        # men posed by crew.pose where no clip fits)
        at_b = g2b((extra["at"][0], extra["at"][1], 0.0))
        yaw_b = -float(extra.get("yaw", 0.0))
        T = M4(Rz(yaw_b), at_b)
        eobjs = load_piece(extra["kind"], extra["piece"], at=tuple(at_b), yaw=yaw_b)
        if extra.get("open_up"):
            open_up(eobjs)
        esocks = sockets_of(extra["kind"], extra["piece"])
        for clip, t, sock in extra.get("men", []):
            off = next((o for kk, p, s_, o in CREW[clip]["place"] if p == extra["piece"] and s_ == sock), (0, 0, 0))
            man_at(m, clip, t, T @ np.asarray(place_matrix(extra["kind"], extra["piece"], sock, off, esocks)))
        js = {x["name"]: x for x in piece_json(extra["kind"], extra["piece"])["sockets"]}
        posed = extra.get("posed", [])
        if posed == "POSED":                  # (every man crew.json `posed` has at this piece, as it has him)
            posed = [(sock, spec) for sock, spec in POSED.get(extra["piece"], {}).items() if sock in js]
        for item in posed:
            cm = clone(m, with_weapon=False)
            if len(item) == 2:
                sock, pspec = item
                args, kw = posed_call(js, sock, pspec)
                cm["skin"] = MODEL["skin"]        # (the pattern man's skin: the same bones, for settling him on a seat)
                pose(cm, *args, **kw)
                if pspec.get("stool"):         # (a plain box for his stool, its top his seat point)
                    h = float(pspec["seat_h"])
                    a = np.asarray(g2b(js[sock]["at"]), float)
                    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(float(a[0]), float(a[1]), float(a[2]) + h / 2))
                    box = bpy.context.active_object
                    box.scale = (0.26, 0.26, h)
                    bpy.context.view_layer.update()
                    box.matrix_world = Matrix(T.tolist()) @ box.matrix_world
            else:
                sock, gl, gr, look = item
                sk = js[sock]
                pose(cm, sk.get("stance", "stand"), sk.get("seat_h", 0.0), gl, gr, sk.get("facing", (1, 0, 0)), sk["at"], look=look)
            cm["arm"].matrix_world = Matrix(T.tolist()) @ cm["arm"].matrix_world
    m["arm"].location = (0.0, 0.0, -50.0)
    kit.studio(sun_elev=42, sun_turn=-35)
    kit.backdrop("A49C84" if spec["kind"] == "ground" else "B5AD98")
    cam = kit.camera(lens=spec.get("lens", 35))
    kit.look_at(cam, spec["eye"], spec["target"])
    bpy.context.scene.camera = cam
    if fast:
        S.shared_render(640, 360, 16)
    else:
        S.shared_render(1280, 720, 48)
    path = os.path.join(WORK, "out", "pictures", name + ".png")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    kit.still(path)
    return path


# ================================================================================================ the whole job
ISSUES = [
    "Vehicles: the sockets' `at` is the seat's top (the M3's, the Truck's, the Hanomag's, the FlakHalftrack's and the "
    "M16's benches, drivers' and cab seats: the ray-cast lands within a few millimetres of `at`; the FlakHalftrack's, "
    "FlakTruck's, M16's, SU76's and SelfPropelledGun's crew.gunner too); the stood men are on the floor at their feet, the M7's and the "
    "Sexton's kneeling crew.gunner on the floor under his hips. crew.json `measure` has every socket; `posed` has every "
    "socket no clip fits, with the crew.pose call that puts a man there and his check.",
    "M3Halftrack (the 17:10 export): `gunner` stands 0.77 behind the pedestal now, the spade grips 0.40 ahead of his "
    "feet and 1.08 up, as stand_mg has them: it goes on the socket as it is (no root_offset; 0.1 cm).",
    "Truck: her cab is shut and its glass dark, no man shown in it (her sockets' notes): sit_drive/sit_cab are not "
    "placed there. Its benches are 0.455 over the floor (the M3's 0.44): sit_bench there leaves the boots 1.5 cm above "
    "the floor.",
    "Hanomag (the 17:58 export) and FlakHalftrack (the 12:08 export; the footwell block gone, the seat backs 0.175 "
    "behind the seat point): the benches fit sit_bench as they are (1.0 cm; rider.5 1.2 cm, his chest at the stern). "
    "The Hanomag's belt boxes now lie across the hull against the step by the left wall: rider.1 is clear of them (1.0 "
    "cm, his seat, as on the other benches) and so is a man stood at `gunner` (crew.pose; 0.1 cm, his soles). "
    "`driver` and `rider.cab` (the 23:28-23:29 exports: the footwells' inner wall faces 0.612 from the middle) fit "
    "sit_drive and sit_cab as they are, in both (0.8 and 0.9 cm). The Hanomag's `gunner` shoulders his gun (no grips): "
    "no clip of ours.",
    "FlakHalftrack and FlakTruck `crew.gunner` (the 23:28 exports: the seat's back 0.2025 behind the seat point, "
    "grip.gunner.r 0.34 ahead, 0.37 to his right and 0.10 over the seat; crew.pose seated, seat_h 0.31, hands on "
    "grip.gunner.l/r): his back is clear of the seat's back now (1.2 cm at most, one point, the ball of his left boot), "
    "but his right hand still stops 4.0 cm short of grip.gunner.r: the handle, low beside his thigh, is 0.66 from his "
    "shoulder and his reach is 0.62, and leaning or turning to it, or sitting 4-7 cm further forward, does not close it "
    "(3.9 cm). The handle 0.08 higher (0.18 over the seat) puts it 0.60 from his shoulder, in reach. `commander`, "
    "`crew.loader`, `crew.loader2` stand clear (1.0 cm at most). The FlakTruck's cab is shut: no driver shown.",
    "M16: `driver` and `rider.cab` fit sit_drive and sit_cab as they are (0.7 and 0.6 cm). `crew.gunner` (the 23:28 "
    "export: the seat's back 0.195 behind the seat point; crew.pose seated, seat_h 0.29, both hands on the column's "
    "grips): his hips still go 1.9 cm into the seat's back (0.21-0.22 behind the seat point, 0.24-0.32 up, about his "
    "middle), and sat 3 or 5 cm further forward he is still 1.9-2.0 cm into it: the back wants another 0.03 or so. "
    "`commander` and the loaders stand clear.",
    "M7 (the 23:29 export: crew.mg and the gun 4.5 cm forward): `crew.mg` on the raised pulpit takes stand_mg and "
    "stand_mg_burst with no root_offset, but his hips still go 1.8 cm into the pulpit's wall behind him (at 1.7 s of "
    "stand_mg, 0.16-0.22 behind the socket, from 0.03 to his right to 0.15 to his left, 0.74-0.85 up). With him 5 cm "
    "further forward and 2 cm to his right he would be clear (1.1 cm), but his hands must stay on the gun: the pulpit's "
    "back 3-5 cm roomier mends it. "
    "`crew.gunner` kneeling (crew.pose stance 'kneel', root 'hips') has both hands on grip.gunner.l/r, but his raised "
    "left knee goes 3.2 cm into the hull's side (0.23 ahead of his hips, 0.26 left, 0.44-0.52 up) wherever his left foot "
    "goes. A stool helps: seated on one 0.20 high under his hips (crew.pose stance 'sit', seat_h 0.20, the seat point "
    "0.20 over the socket) he has both hands on the grips and clears everything (0.9 cm); 0.25 high, 1.3 cm. "
    "`commander`, `crew.loader`, `crew.loader2` stand clear (crew.pose; the breech 0.73 over the floor is too low for "
    "stand_load_breech).",
    "Sexton (the 12:10 export): `crew.gunner` kneels (crew.pose stance 'kneel', root 'hips'), both hands on the grips, "
    "clear (1.0 cm). Hummel: `crew.gunner` stands and leans to the handles, both hands on them (1.3 cm, one point, his "
    "hip). SU-76 (the 20:27 export: its seat 0.39 over the floor's top, the traversing handle 0.05 higher and the "
    "elevating 0.03 nearer him): `crew.gunner` seated (crew.pose, seat_h 0.39) is clear (0.1 cm) with both hands on "
    "grip.gunner.l/r (0.0 cm short). Their commanders and loaders stand clear (crew.pose).",
    "M10 (the 17:10 export): `commander` stands with his toes under a shelf 0.23 over his floor (both shins 2.7 cm into "
    "its edge, 0.04 ahead of him): 0.15 further back he is clear (crew.json `posed` has him there). `crew.loader`'s hip "
    "is 2.5 cm in the turret's wall on his right: 0.03 to the left he is clear. `crew.gunner` (the 23:28 export: the "
    "socket 2 cm back and 4 cm to his right; his seat 0.28 over the floor ahead, no grips): his left calf still goes "
    "1.9 cm into the turret and the gun's mount (0.34-0.49 ahead, 0.14-0.24 left, from the floor to 0.18 over the seat), "
    "and now his right hip 1.8 cm into the wall on his right (0.09-0.14 behind, 0.31 right, 0.11-0.20 over the seat): "
    "his feet set closer and him 2 cm to the left, 1.7 cm. He wants 2 cm more room on both sides. "
    "commander_hatch does not fit: the open turret's wall is 0.65-0.96 over his feet.",
    "M8 (the 23:28 export): `crew.gunner` (the socket 4 cm back and 3 cm to his right; seat 0.26 over the floor ahead): "
    "his right toe is clear of the gun's mount now, but his left calf still goes 1.7 cm into the turret (0.35-0.49 "
    "ahead, 0.16-0.24 left; his feet set closer, 1.5 cm). `commander` is now a hatch socket 0.25 under the open "
    "turret's rim: commander_hatch with root_offset (0, 0, -0.92), tested over the turret's floor (1.60), goes 2.5 cm "
    "in: his left upper arm into something over the rim on his left (0.13-0.28 ahead, 0.30 left, 0.28 over the rim, as "
    "he raises the glasses) and his right hip 1.7 cm into the turret's right wall (0.04-0.13 behind, 0.25-0.31 right, "
    "about the socket's height); 3 cm to his left, or 5 cm back and 2 cm left, 2.5-2.8 cm. Not placed. SdKfz222 (the "
    "23:28 export, the screens thrown open by open_up): `commander` is now a hatch socket 0.25 under the turret's rim: "
    "commander_hatch with root_offset (0, 0, -0.92), tested over the turret's floor (1.62), goes 2.8 cm in: his belly "
    "into a face 0.19 ahead of the socket, facing him (0.21 ahead, 0.07-0.10 right, 0.10 over the socket), and his "
    "right hip 2.5 cm into a ledge about 0.08 over the socket (0.09 behind to 0.20 ahead, 0.11-0.31 right); 6 or 10 cm "
    "further back, the hip still 2.5 cm. Not placed. ArmouredCar (the 23:29 export, the lid and opening 0.225 round): "
    "`commander` takes commander_hatch with root_offset (0, 0, -0.92) as before, but his chest still goes 1.7 cm into "
    "the rim's top at its front (0.175 ahead of the socket, 0.02-0.07 right, at 1.8 s as he leans on it), and 3 or 5 cm "
    "further back the same: the opening's front edge seems to be about 0.17 ahead of the socket, as if the opening "
    "were not centred on it.",
    "SelfPropelledGun (the 20:27 export: its seat 0.45 over the floor's top, the water can laid lengthways against the "
    "left wall, the traversing handle 0.05 higher and the elevating 0.03 nearer him): `crew.gunner` seated (crew.pose, "
    "seat_h 0.45, his boots where the stance puts them) is clear (0.1 cm), his left boot 11.6 cm from the can, with both "
    "hands on grip.gunner.l/r (0.0 cm short). The HeavyFlak's sit_handwheel_traverse/elevate do not fit him: they turn "
    "one wheel with both hands (its hub 0.365 ahead, 0.26 to the side, 0.46 up, facing him), and he has a hand on each "
    "of two wheels lower down (the elevating hub 0.23 ahead, 0.23 right, 0.23 up, facing sideways; the traversing 0.16 "
    "ahead, 0.29 right, 0.12 up, tilted): as the SU-76's layer. `commander` and `crew.loader` stand clear (0.1 cm), and "
    "stay 8 and 12 cm clear of the gun at its full run back (0.65 m, the breech face to x -0.42).",
    "AssaultGun (the 20:10 export): `commander` takes commander_hatch with root_offset (0, 0, -0.92) (his feet 0.92 "
    "under the socket, as its note says; the lid open by its open_up clip): clear (1.2 cm at most, the left of his "
    "chest against the rim at 2.08, 0.13 left of the socket). Its fighting box is solid, from its floor at 1.02 to its "
    "roof at 1.92, as the ArmouredCar's hull: what is over the rim is tested.",
    "Towed guns (the 12:08 export; the ground not modelled, the trails on it): the loaders stand clear at every gun. "
    "`crew.layer` stands in the left trail at the Pak40, ZiS3, AntiTankGun and 105 mm howitzer (his legs 5-10 "
    "cm into trail_l at 0.33-0.55 up, kneeling or standing; at the howitzer the left wheel too). Where "
    "he fits, crew.pose standing (both hands on grip.layer.l/r): the Pak40, ZiS3 and AntiTankGun 0.30 to the left of the "
    "socket and 0.15 ahead (the ZiS3's right hand 0.4 cm short), the 105 mm howitzer 0.30 left; kneeling, the Gun75 0.15 "
    "left; the 25pdr kneels at his socket on the firing platform (0.03 up). The leFH18's layer (the 23:29 export: "
    "crew.layer 0.46 behind and 0.15 right of the old place, no root_offset): at the socket his left knee goes 2.2 cm "
    "into the trail beside it (0.12-0.16 ahead, 0.18-0.20 left, 0.46-0.57 up); crew.pose standing 0.04 to the right of "
    "the socket, both hands on grip.layer.l/r, is clear (0.0 cm). "
    "`crew.commander` stands in a trail at the Pak40 (3.5 cm; 0.15 right is clear), the AntiTankGun (3.1 cm; 0.15 right, "
    "0.8 cm) and the 105 mm howitzer (7.2 cm; 0.30 left is clear); `crew.ammo` at the Pak40 (2.8 cm; 0.15 right clear) "
    "and the AntiTankGun (3.2 cm; 0.30 right clear). crew.json `posed` has each man where he fits.",
    "PanzerIV (the 10:38 export): no `driver` socket. Its `commander` socket's note now says his feet go 0.92 under it, "
    "as commander_hatch's root_offset has them (the rim 1.17 m over his feet, his chest out of it); checked against the "
    "mesh above the rim and against the cupola's ring as measured (1.0 cm). He wears the infantry kit.",
    "Arms (rom.py's ranges at 60 fps, arms_check.py --crew): the arms of the gun crews' clips are tracked (ARM_HOLD: "
    "each hand free to turn on its round or handle within its hold, the elbow to swing, the path the cheapest through "
    "the clip with the joints' speed paid for) and the body carries the hands (comfy_body: the trunk bent, turned and "
    "the hips moved per tenth of a second; the mortar men's number one and three step at most 4 cm, COMFY_CLIP, the "
    "pit being close behind them). mortar_number1-3, sit_handwheel_traverse/elevate, stand_ammo_give, "
    "stand_ammo_hand_up and stand_ammo_crate_hand_up have no arm fault and no joint over 900 degrees a second; the "
    "handwheel layers' elbows are kept 4 cm back of the handles (ROD_WHEEL's behind), so the forearm comes onto the "
    "handle from his side of the wheel and stays off its rim. The three loaders still fault: stand_load_breech (the "
    "right forearm 6 degrees past its turn as the round goes in, the elbow 3 degrees over the level, joints to 1,310 "
    "degrees a second), stand_load_breech_twin (the right elbow 4.6 cm into his side as he lifts the round, the "
    "forearm 4 degrees past its turn, joints to 1,070 a second) and stand_load_breech_twin_l (the left shoulder 19 "
    "degrees across his chest and the forearm 9 past its turn as he takes the round, the elbow 6.6 cm into his side, "
    "joints to 1,500 a second). The round's base passes beside his body as it goes in: the loader wants to stand back "
    "from the breech with the round and lunge in with the ram.",
    "Fits the arm work has cost (against the 22:30 delivery): mortar_number2 at MortarPit crew.number2 2.4 cm (was "
    "0.9: his pelvis on the pit's back corner at 1.6 s, as his body steps 14 cm right to bring the bomb to the muzzle; "
    "at the Mortar 0.6), mortar_number3 1.9 cm (was 0.8: the right forearm on the bomb box's edge at 2.4 s), "
    "stand_load_breech 1.5 cm (was 0.65: his pelvis at 1.5 s, as he steps in to ram) and stand_load_breech_twin_l "
    "8.8 cm (was 1.05: his left upper arm on top of the fuze setter's machine at 0.6 s, the body stepped back with "
    "the round). Limiting the body's step put the arms back into fault (mortar_number2, twin_l), so these are left "
    "for the loaders' rework.",
    "Mortar: crew.number3 is 4 cm from the bomb box (a kneeling man's thigh goes into it): mortar_number3's root goes "
    "0.10 m to his left. MortarPit's number1 is 3 cm off the Mortar's (its root_offset puts his eye at the sight); "
    "MortarPit's number3 (by the niche) has no clip.",
    "HeavyFlak and HeavyFlakTwin (the 10:19Z export): the layers' fists are centred on grip.layer.*.l/r (the handles' "
    "middles) and go round with them as the wheels turn (the clips turn the game's plus way: the right-hand handle "
    "rises, as the piece's own follow clip turns it), the thumbs toward the layers, clear of the rims.",
    "HeavyFlak: crew.loader (0.30 to the right of where it was, out of the recoil) also faces 8.5 degrees left of the "
    "gun's line, so a root_offset alone could not carry stand_load_breech there: the clip is said at the socket as it "
    "stands (no offset), the same moves in the gun's frame, and he stands at the socket, clear of the recoil, while the "
    "gun fires. Its note says the breech's line is 0.3 m to his left; it is 0.50 m across the gun from him. round.ready "
    "agrees with him: the fire clip hides it at 2.6, the moment his prop round shows in its place (standing as it stood), "
    "his hands close on it at 2.72 and lift it out from there; it shows again from 0.6, when he is at the breech. The "
    "clips assume the gun laid level (the breech at its rest).",
    "HeavyFlakTwin: its fire clip does not hide round.ready (the single gun's does): stand_load_breech_twin's events "
    "say when (hide it at 2.7, when loader.r's hands close on it and his prop round shows in its place; show it again at "
    "0.7).",
    "HeavyFlakTwin: loader.l has no ready round, so stand_load_breech_twin_l walks to the rail for each one (1.0 m behind "
    "his socket) and takes it from ammo.1 (stand_ammo_crate_hand_up) there. His way runs between a low strut (from the "
    "platform at (-1.79, -0.73) up to the mount at (-0.90, -0.38): he steps over its low end) and crew.fuze, and a man "
    "standing at crew.fuze is in it: the two overlap from 2.6 to 3.1. crew.fuze 0.3 m further out (y -1.54) clears him "
    "by 4 cm; a ready rack of loader.l's own (a round.ready as loader.r's) would spare him the walk. The fuze setter's "
    "machine stands 0.21 m outboard of loader.l at his waist and chest (its face at y -1.03, 0.94-1.18 m over the "
    "platform): he holds his round upright before him while the guns fire, to keep his elbow off it. crew.fuze and "
    "crew.ammo.2 have no clip.",
    "soldier.py's kneel_base (the soldier pack's own kneeling clips) puts the right boot's toes about 10 cm into the "
    "ground and the knee 4 cm over it. The crew's kneeling men are fixed here (kneel_grounded: the foot stood up on its "
    "tucked toes, the hips let down 4 cm); the soldier pack's kneel clips want the same.",
    "The prone gunner (mg_pair_gunner) is soldier.py's lmg_burst pose: his cheek and left hand press up to 2 cm into the "
    "stock as there. The soldier's LMG has a box magazine on its left: the belt comes out of that box's end.",
]


POSE_LIMITS_TEXT = {
    "stances": "stand, kneel (on the right knee, the left foot forward), sit (on a seat, or on the ground below seat_h 0.12), prone",
    "seat_h": "sit: seats 0.15 to 0.60 over the floor his boots are on (his feet flat on it; from 0.45 the shins hang "
              "more under the knees, from 0.55 his seat is on the seat's front edge, the thighs sloping down). Below "
              "0.12 he sits on the floor itself, knees up. Higher than 0.60 his feet do not reach the floor: give "
              "feet= on a footrest",
    "reach": "the hands go on any two grips the leaned (to 60 degrees), turned and side-bent body brings them to, "
             "measured from the root for a pair of grips 0.3 apart before him: standing, 0.7 to 1.8 up (no crouch: "
             "lower, kneel), to 0.5 ahead at 0.7 up, 0.8 at 1.0-1.4, 0.7 at 1.5-1.6, 0.4 at 1.8; kneeling (the root "
             "at his feet, his hips 0.21 behind it), 0.2 to 1.3 up, to 0.6 ahead at 0.3-0.9 up, 0.4 at 1.1, 0.1 at "
             "1.3; seated (a 0.44 seat), 0.2 to 1.0 over the seat point, to 0.7 ahead at 0.4-0.7, 0.4 at 1.0 (measured "
             "again after the arm fix of 18 November: 1-3 cm more than before). Past that a hand stops short (pose_report says by how much); grips off to one side reach less far",
    "feet": "feet={'l': point, 'r': point}: where each boot stands (the floor under the ankle, the piece's axes); either "
            "may be left out. Standing, a foot below his root lets the hips down as far; a foot on a step bends its knee. "
            "Seated, they replace the feet the seat height puts on the floor (a footrest). Kneeling, 'l' is the forward "
            "foot (its knee up), 'r' the kneeling leg's tucked toes",
    "root": "root='feet' (default: the floor at his feet; sit: the seat point) or 'hips' (the socket is the floor "
            "under his hips: the kneel's root is 0.21 ahead of them)",
    "not": "no collision avoidance: the pose does not bend round the piece; pose_check measures what it goes into",
}


def write_json(path, baked, checks, measure, posed=None):
    clips = {n: clip_json(n, baked.get(n, {})) for n in CREW}
    for n, r in checks.items():
        clips[n]["check"] = r
        clips[n]["hands_short_max_m"] = baked.get(n, {}).get("short")
    data = {
        "name": "crew", "fps": FPS, "units": "metres",
        "about": "The crew pack: the soldier (soldier_GER.glb's skeleton) seated, standing and working at the vehicles' "
                 "and the fortifications' crew sockets. Every clip is an action in crew.glb.",
        "axes": "the game's, as the vehicles' and fortifications' JSON: x forward, y right, z up; quaternions (w, x, y, z)",
        "anchors": {"seat": "the clip's root is the SEAT POINT: the top of the seat under the middle of his seat; his boots are "
                            "the clip's seat_h under it",
                    "feet": "the clip's root is on the FLOOR AT HIS FEET (standing, kneeling); lying, on the ground under his hips",
                    "sockets": "a socket is a point and a facing, the man's feet on it unless its note says otherwise. Where the "
                               "note says seated and gives no height (the M3's and the Truck's driver and rider.N; rider.cab "
                               "too, though its note does not say it), the point is the seat's top (ray-cast: within a few "
                               "millimetres): a seat clip's root goes on the socket. Where the note gives a seat height over "
                               "the floor (the M10's crew.gunner, 0.34), the socket is his feet: the root goes that height "
                               "up (root_offset (0, 0, h)), and a clip whose seat_h differs wants crew.pose. A feet clip's "
                               "root goes on a feet socket as it is, moved by its root_offset where one is given"},
        "root_offset": "where a clip's root goes from a socket: forward, right, up of the socket's facing (the game's axes)",
        "weapon": {"rifle": "his rifle on the weapon bone", "lmg": "play on soldier_GER_lmg.glb (the LMG on the weapon bone)",
                   "none": "the weapon bone is keyed to scale 0 inside his chest: the rifle is not seen"},
        "props": {k: {"mesh": "props/%s.glb" % k, "about": v} for k, v in PROP_ABOUT.items()},
        "pose_function": {"script": "Tools/battle_units/crew.py",
                          "call": "crew.pose(man, stance, seat_h, grip_l, grip_r, facing, at=socket_at, look=None, "
                                  "feet=None, root='feet')",
                          "about": "puts a man at any crew socket: stance stand/kneel/sit/prone, seat_h from his feet down "
                                   "from the seat, his hands by two-bone IK on the grips (or resting), the body leaned, "
                                   "turned and bent to the side until both are on them, the root on the seat point (sit) "
                                   "or the floor at his feet (root='hips': his hips over the socket); feet= puts his boots",
                          "limits": POSE_LIMITS_TEXT,
                          "report": "crew.pose_report(P, st): each hand's gap to its grip and each leg's shortfall (m); "
                                    "crew.pose_check(P, st, tree): what he goes into"},
        "clips": clips,
        "posed": {"about": "every vehicle socket no clip fits, with the crew.pose call that puts a man there (its "
                           "arguments; at in the game's axes, the socket moved by root_offset, and seat_h up on a stool) "
                           "and the check of him against the vehicle, its hatches and screens open (its open_up clip): "
                           "deepest_m into it (not counting a fist round its grip), hand_gap_m, leg_short_m, "
                           "boot_over_surface_m (+ floating, - sunk; the self-propelled floors carry 0.8 cm of mud)",
                  "vehicles": posed or {}},
        "measure": measure,
        "issues": ISSUES,
    }
    with open(path, "w") as f:
        json.dump(data, f, indent=1)
    return data


def fmt_check(rs):
    out = []
    for r in rs:
        if "lowest_m" in r:
            out.append(f"ground: lowest {r['lowest_m'] * 100:+.1f} cm ({r['where'][1]}), hands {r['hand_gap_m'] * 100:.1f} cm")
        else:
            out.append(f"{r['at']}: deepest {r['deepest_m'] * 100:.1f} cm ({r['where'][1] if r['where'] else '-'}), "
                       f"hands {r['hand_gap_m'] * 100:.1f} cm")
    return "; ".join(out)


def write_readme(path, data, pictures):
    L = []
    L.append("# The crew pack\n")
    L.append("The soldier (`../soldier_GER.glb`, the same skeleton) seated, standing and working at the vehicles' and the "
             "fortifications' crew sockets: short loops for the vehicles, the crew clips of the machine gun, the mortar and "
             "the heavy flak, a dive to the ground, and a posing function for any other crew socket. Made by "
             "`Tools/battle_units/crew.py` (on soldier.py); everything is our own.\n")
    L.append("## Files\n")
    L.append("- `crew.glb`: the German soldier with every clip below as an action (60 frames a second, a loop's last frame "
             "is its first). Retarget the actions to the soldier you already have: the bones are the same.")
    L.append("- `crew.json`: every clip's length, loop, anchor, weapon, places (piece, socket and root offset), events, "
             "props' tracks, and the checks; the vehicles' seats as measured; the issues below.")
    L.append("- `props/*.glb`: binoculars, mortar_bomb, flak_round, belt, ammo_box, flak_crate (static meshes; their tracks "
             "in crew.json are in the clip's root frame, the game's axes, 30 a second, with `shown`).")
    L.append("- `pictures/*.png`: the proofs, each pose at its real piece and socket.\n")
    L.append("## Where the root goes (the anchor)\n")
    L.append("A socket is a point and a facing, and the man's feet go on it unless its note says otherwise (the vehicle "
             "thread's rule).\n")
    L.append("- **Seated clips (`anchor: seat`): the root is the SEAT POINT**, the top of the seat under the middle of his "
             "seat; his boots are the clip's `seat_h` under it (crew.json). The sockets whose note says seated with no "
             "height (the M3's and the Truck's `driver` and `rider.N`; `rider.cab` too, though its note does not say so) "
             "are that point: ray-cast straight down, the seat's top is within a few millimetres of `at` (M3 benches 0.44 "
             "over the floor, its front seats 0.40; the Truck's benches 0.455; the HeavyFlak's layers' seats 0.44). Put "
             "the root on the socket. Where a note gives a seat height over the floor instead (the M10's `crew.gunner`, "
             "0.34), the socket is his feet: put the root that height up, and where the clip's `seat_h` is not that "
             "height, pose him with `crew.pose(stance='sit', seat_h=...)`.")
    L.append("- **Every other clip (`anchor: feet`): the root is on the FLOOR AT HIS FEET**; lying, on the ground under "
             "his hips. Where a socket is not that point, crew.json gives the clip's `root_offset` from it (forward, right, "
             "up of the socket's facing).")
    L.append("- Clips on a turning part (the M3's gun, the flak's mount) are posed in that part's frame: attach the man "
             "to the part.\n")
    L.append("## The names, and the other vehicles\n")
    L.append("The clips are named for the job, not the piece, so that the coming vehicles can use them: what each fits, "
             "beyond the pieces it was fitted at, is below and in crew.json (`fits`). The heavy flak's clips were "
             "flak_layer_traverse/elevate, flak_loader, flak_ammo_2 and flak_ammo_1; they are now `sit_handwheel_traverse`, "
             "`sit_handwheel_elevate`, `stand_load_breech`, `stand_ammo_give` and `stand_ammo_hand_up`. Where a vehicle's "
             "seat, grips or breech are not where a clip has them, `crew.pose` seats or stands a man at any socket with his "
             "hands on its grips.\n")
    for n in data["clips"]:
        if FITS.get(n):
            L.append(f"- `{n}`: {FITS[n]}")
    L.append("")
    L.append("## The clips\n")
    L.append("| clip | s | loop | anchor | weapon | where | checked (deepest into the piece; hands off their grips) |")
    L.append("|---|---|---|---|---|---|---|")
    for n, c in data["clips"].items():
        where = ", ".join(sorted({f"{p['piece']} {p['socket']}" + (f" (root {tuple(round(x, 3) for x in p['root_offset'])})" if any(p['root_offset']) else "")
                                  for p in c["places"]})) or "open ground"
        if n == "sit_bench":
            where = "M3Halftrack and Truck rider.1-10"
        L.append(f"| `{n}` | {c['long']} | {'yes' if c['loop'] else 'no'} | {c['anchor']} | {c['weapon']} | {where} | "
                 f"{fmt_check(c.get('check', []))} |")
    L.append("")
    for n, c in data["clips"].items():
        L.append(f"- **{n}**: {c['about']}")
    L.append("")
    L.append("Playing together: `mg_pair_gunner` and `mg_pair_feeder` share one root (on the ground under the gunner's "
             "hips). `mortar_number1/2/3` start together, each at its own socket; the Mortar's own `fire` clip goes at 1.62 "
             "(the event in mortar_number2). `stand_load_breech`, `stand_ammo_hand_up` and `stand_ammo_give` run with the HeavyFlak's own "
             "`fire` clip (3.2 s) from the same moment: t = 0 is the shot; `stand_load_breech_twin`, `stand_load_breech_twin_l` and "
             "`stand_ammo_crate_hand_up` with the HeavyFlakTwin's (3.3 s), the same way. The layers' clips turn the handwheel one turn a "
             "second the game's plus way: play them at the wheel's speed (the game spins it 60 degrees per degree of "
             "training), backwards while it turns minus. "
             "`throw_flat` ends with his hips 1.1 m ahead of its root (`end_offset`): carry on with the soldier's prone "
             "clips there.\n")
    L.append("## Posing a man at any socket\n")
    L.append("```python\nimport crew\nP, st = crew.pose(man, sock['stance'], sock.get('seat_h', 0.0), grip_l, grip_r,\n"
             "                  sock['facing'], at=sock['at'], look=muzzle)    # the game's axes, as the JSON\n```\n")
    L.append("`man` is soldier.assemble's (or crew.load_man's) dict. Stance `sit` with `seat_h` over 0.12 sits him on a "
             "seat (his feet `seat_h` down, his seat settled on the seat's top measured on the skinned mesh); `sit` with "
             "seat_h 0 sits him on the ground, knees up; `kneel`, `prone`, `stand` as the soldier's own. His hands go "
             "to the grips by two-bone IK (the body leaned and turned until both are in easy reach), or rest; his head "
             "looks at `look`. The pose is keyed on his armature and the armature put on the socket "
             "(`pictures/pose_machinegun.png`: the MachineGun's gunner and loader).\n")
    L.append("### What crew.pose handles\n")
    lim = data["pose_function"]["limits"]
    for k in ("stances", "seat_h", "reach", "feet", "root", "not"):
        L.append(f"- **{k}**: {lim[k]}.")
    L.append("\nThe call in full: `" + data["pose_function"]["call"] + "`; then " + data["pose_function"]["report"] + ".\n")
    L.append("### The sockets posed (crew.json `posed`)\n")
    L.append("Every vehicle socket no clip fits, with the call that puts a man there (`at` in the game's axes, the socket "
             "moved by `root_offset`; on a stool `seat_h` up) and his check against the piece with its hatches and "
             "screens open (its own open_up clip). Over 1.2 cm into the piece is a misfit (the issues say what mends it); "
             "on the self-propelled floors a boot under 1 cm into the floor is their 0.8 cm of mud.\n")
    L.append("| vehicle | socket | stance | seat_h | grips | root | root_offset | deepest (cm, where) | hands short (cm) |")
    L.append("|---|---|---|---|---|---|---|---|---|")
    for v, socks in data["posed"]["vehicles"].items():
        for sn, r in socks.items():
            c, k = r["call"], r["check"]
            st = c["stance"] + (" on a stool" if c.get("stool") else "") + (", looks at the breech" if c.get("look") == "breech" else "") + \
                "".join(f", {'left' if fk == 'l' else 'right'} boot at {tuple(fv)}" for fk, fv in (c.get("feet") or {}).items())
            g = (c["grip_l"] + ", " + c["grip_r"]) if c.get("grip_l") else "-"
            off = tuple(round(x, 2) for x in c["root_offset"]) if any(c["root_offset"]) else "-"
            n = k["points_past_1_2cm"]
            d = f"{k['deepest_m'] * 100:.1f} ({k['where'] or '-'}" + (f"; {n} point{'s' if n > 1 else ''} past 1.2" if n else "") + ")" + \
                (" **misfit**" if k["deepest_m"] > 0.012 and n >= 3 else "")
            hg = k["hand_gap_m"] * 100 if c.get("grip_l") else None
            L.append(f"| {v} | {sn} | {st} | {c['seat_h'] if c['stance'] == 'sit' else '-'} | {g} | {c['root']} | {off} | {d} | "
                     f"{'-' if hg is None else '%.1f' % hg + (' **short**' if hg > 0.5 else '')} |")
    L.append("")
    L.append("## The props\n")
    for k, v in PROP_ABOUT.items():
        L.append(f"- `props/{k}.glb`: {v}.")
    L.append("\nA prop's track (crew.json, per clip) gives its frame in the clip's root frame; `shown` 0 means hide it "
             "(the bomb gone down the bore, the round home in the breech or handed on).\n")
    L.append("## What the pieces need (found while fitting)\n")
    for i in data["issues"]:
        L.append(f"- {i}")
    L.append("\n## The vehicles' seats, measured\n")
    L.append("| vehicle | socket | at z | surface under | at - surface | seat over the floor ahead |")
    L.append("|---|---|---|---|---|---|")
    for v, socks in data["measure"].items():
        for sn, r in socks.items():
            L.append(f"| {v} | {sn} | {r['at_z']} | {r['surface_under']} | {r['at_minus_surface']} | {r['seat_over_floor']} |")
    L.append("\n## The pictures\n")
    for name, about in pictures:
        L.append(f"- `pictures/{name}.png`: {about}")
    L.append("\nThe checks: the man skinned as the game skins him, every 6th frame, each point tested against the piece "
             "(inside by ray parity); a hand's own fist round its grip is not counted. The PanzerIV's turret has no "
             "inside and its hatch is painted on: the commander is tested against the mesh above the cupola's rim and "
             "against the cupola's ring (as measured) below it. The HeavyFlak is tested as it stands at each moment: "
             "under the loader and the ammunition men its own fire clip (the barrel's recoil, the breech opening, "
             "round.ready hidden), under a layer his handwheel turned as far as his hands have turned it. On open ground, the lowest point against the ground (z 0). The "
             "reaching clips (mortar numbers two and three, the flak's loader and ammunition men) are keyed through "
             "poses fitted every 0.1 s so that both hands reach; nothing moves faster than a brisk working pace (hands "
             "under about 5 m/s, hips under 2.5 m/s, at 30 samples a second).")
    with open(path, "w") as f:
        f.write("\n".join(L) + "\n")


def deliver_all(stage):
    """Copy every finished file to the shared folder once, then read each back ten seconds later."""
    import hashlib
    import shutil
    sent = []
    for root, dirs, files in os.walk(stage):
        for fn in files:
            src = os.path.join(root, fn)
            rel = os.path.relpath(src, stage)
            dst = os.path.join(OUT, rel)
            os.makedirs(os.path.dirname(dst) + "/", exist_ok=True)
            shutil.copyfile(src, dst)
            sent.append((src, dst))
    time.sleep(10.0)
    ok = True
    for src, dst in sent:
        h = lambda p: hashlib.sha1(open(p, "rb").read()).hexdigest()
        good = os.path.exists(dst) and os.path.getsize(dst) == os.path.getsize(src) and h(dst) == h(src)
        if not good and os.path.exists(dst) and dst.endswith(".png"):
            from PIL import Image, ImageChops
            a, b = Image.open(src), Image.open(dst)
            good = a.size == b.size and ImageChops.difference(a.convert("RGB"), b.convert("RGB")).getbbox() is None
        ok &= good
        print(f"  {'ok ' if good else 'BAD'} {dst} ({os.path.getsize(src)} bytes)")
    return ok


def main(argv):
    only = None
    if "--only" in argv:
        only = set(argv[argv.index("--only") + 1].split(","))
    fast = "--fast" in argv
    want = lambda k: only is None or k in only
    stage = os.path.join(WORK, "out")
    os.makedirs(stage, exist_ok=True)
    state_path = os.path.join(WORK, "state.json")
    state = json.load(open(state_path)) if os.path.exists(state_path) else {}
    t0 = time.time()
    if want("measure"):
        state["measure"] = measure_seats()
        print("measured", round(time.time() - t0))
    if want("export"):
        kit.fresh(FPS)
        m = setup()
        baked = bake(m, list(CREW))
        S.export_glb(os.path.join(stage, "crew.glb"), [m["arm"], m["body"], m["helmet"], m["weapon"], m["weapon_part"]] + m["kit"] + m["sockets"])
        state["baked"] = baked
        for pn in PROP_ABOUT:
            ob = build_prop(pn)
            S.export_glb(os.path.join(stage, "props", pn + ".glb"), [ob], anim=False)
        print("exported", round(time.time() - t0))
    if want("check"):
        state["checks"] = check_everything()
    if only is not None and "vcheck" in only:
        new = check_everything("vehicle")
        vehicles = {p for c in CREW.values() for k, p, s_, o in c["place"] if k == "vehicle"} | \
                   {p for pl in CHECK_AT.values() for k, p, s_ in pl if k == "vehicle"}
        old_c = state.get("checks", {})
        for name in set(old_c) | set(new):
            keep = [r for r in old_c.get(name, []) if r.get("at", "").split(" ")[0] not in vehicles]
            old_c[name] = keep + new.get(name, [])
        state["checks"] = old_c
    if want("posed"):
        state["posed"] = posed_checks()
        print("checked", round(time.time() - t0))
    json.dump(state, open(state_path, "w"))
    if want("pictures"):
        for name in PICTURES:
            build_picture(name, fast=fast)
            print("picture", name, round(time.time() - t0))
    if want("json"):
        data = write_json(os.path.join(stage, "crew.json"), state.get("baked", {}), state.get("checks", {}), state.get("measure", {}),
                          state.get("posed", {}))
        write_readme(os.path.join(stage, "README.md"), data, [(n, PICTURES[n]["about"]) for n in PICTURES])
        print("json and readme", round(time.time() - t0))
    if "--deliver" in argv:
        print("delivered:", deliver_all(stage))


if __name__ == "__main__":
    main(sys.argv)
