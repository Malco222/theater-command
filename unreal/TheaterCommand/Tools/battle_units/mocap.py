"""Real motion capture for the soldier: the CMU motion-capture library retargeted onto human.py's 27-bone rig.

Why: Brian asked (2026-11-18) for the soldiers to move "a lot more realistic". Hand-keyed cycles hit a ceiling, so the
body's movement comes from the Carnegie Mellon University Graphics Lab motion-capture database, free for research
and commercial projects (the data itself may not be resold; see mocap_cmu/CREDITS.md, which carries the credit: "The
data used in this project was obtained from mocap.cs.cmu.edu. The database was created with funding from NSF
EIA-0196217."). The BVH files are the 2010 MotionBuilder-friendly conversion; mocap_cmu/fetch_cmu.py downloads the
takes we use. Nothing here comes from any game.

    import human, mocap
    man = human.build(seed=3, nation="GER", stand=0.02, curl=(30, 45, 25))
    act = mocap.apply(man["armature"], "walk")           # makes the action `walk` from mocap_cmu/clips.json
    mocap.apply_all(man["armature"])                     # every clip, one action each
    act2, miss = mocap.rifle_arms(man["armature"], act, "port", name="walk_port")
                                                         # both hands on a rifle (port or ready), baked to plain keys,
                                                         # the walk underneath; adds the `weapon` bone (hand_r's child)

    python mocap.py build [clip..]    rebuilds mocap_cmu/clips.json from the BVH files (fetch them first)
    python mocap.py preview [clip..]  renders preview films (a name "walk+port" previews rifle_arms on the walk)
    python mocap.py sheet [clip..]    the contact sheet clips.png from the rendered frames

HOW THE RETARGET WORKS
  * The BVH is read by a small parser here (numpy), its skeleton posed by forward kinematics, and turned from CMU's
    axes (x left, y up, z front, units of about 5.6 cm) into ours (x front, y left, z up, metres).
  * Each of our bones takes the WORLD rotation of one CMU joint, measured from CMU's T pose (the first frame of every
    file): our bone's world rotation = (CMU joint now) * (CMU joint at the T pose)^-1 * (our bone at the matched pose).
    The matched pose is our rest A pose with the limbs swung, joint by joint from the shoulder and the hip down, onto
    the directions CMU's limbs have in its T pose, so the arms come out at the right height and twist. The trunk,
    neck, head and feet are matched as they stand (our rest is upright and flat-footed, as CMU's T pose is).
  * The pelvis follows CMU's hip joints scaled by the ratio of the two men's leg lengths; the man is set on the
    ground from his soles; the feet are locked on the ground while in contact, by two-bone IK on the legs, and the
    toes are kept out of the ground. In the loops a landing foot glides down onto the ground in its last frames
    (our heel, tipped up, reaches lower than the actor's, and would otherwise touch early and skid in).
  * Loops: the best matching cycle in the take is cut, its seam closed by spreading the small difference over the
    cycle, and resampled to 30 fps with the last frame equal to the first. Travel is removed (IN PLACE); `stride` is
    the metres the man travels per loop, so the game sets play rate = speed / stride * length and the feet stay put.
  * The capture's arms are then fitted to a real arm's range (fit_arms): each elbow back on its hinge, each hand's
    twist and bend eased inside the limits, a capture hand that flipped over repaired from the frames either side,
    and the elbows kept clear of the trunk. rom.py (numpy only) checks every joint of a clip against a real body's
    range of motion and reports the worst, with its frame.
  * One-shots may be played at a changing speed (`speeds`), get a jolt at the hit (`jolt`) and have the arms part
    way to slack (`limp`): fall_forward uses all three so it reads as a man hit, not one lying down.
  * CMU has no belly crawl, so `crawl` (the low crawl under fire, rifle across the forearms) is keyed by hand on the
    rig: belly_crawl() and its settings in CRAWL. `crawl_high` is the captured hands-and-knees crawl.

CLIPS.JSON (Blender's frame: x forward, y LEFT, z up, metres; quaternions w, x, y, z)
  bones[b][f]   the bone's rotation relative to its rest, in its PARENT's space: the bone's local rotation (in its
                parent's frame) is D * RestLocal, where RestLocal is the bone's rest rotation in its parent's frame.
                (Blender's pose-bone quaternion is RestLocal^-1 * D * RestLocal; apply() does that.)
                For Unreal's y-right frame mirror each quaternion as (w, -x, y, -z) and each location as (x, -y, z).
  pelvis_loc[f] the pelvis head's offset from its rest place, in the armature's space (root never moves).
"""

import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
CMU_DIR = os.path.join(HERE, "mocap_cmu")
BVH_DIR = os.path.join(CMU_DIR, "bvh")
CLIPS_JSON = os.path.join(CMU_DIR, "clips.json")
REST_REF = os.path.join(CMU_DIR, "rest_ref.json")    # the reference man's rest (Rest.load), for work without Blender
FPS = 30
SRC_FPS = 120

# Our bone -> (the CMU joint whose world rotation drives it, the CMU joint its direction points at in the T pose, or
# None when the bone is matched as it stands). Bones not named here (root, thumbs, fingers) keep their rest pose
# relative to their parent: CMU did not capture fingers.
MAP = {
    "pelvis": ("Hips", None),
    "spine_01": ("LowerBack", None),
    "spine_02": ("Spine", None),
    "chest": ("Spine1", None),
    "neck": ("Neck1", None),
    "head": ("Head", None),
}
for _s, _S in (("l", "Left"), ("r", "Right")):
    MAP.update({
        f"clavicle_{_s}": (f"{_S}Shoulder", None),
        f"upperarm_{_s}": (f"{_S}Arm", f"{_S}ForeArm"),
        f"lowerarm_{_s}": (f"{_S}ForeArm", f"{_S}Hand"),
        f"hand_{_s}": (f"{_S}FingerBase", f"{_S}HandIndex1"),
        f"thigh_{_s}": (f"{_S}UpLeg", f"{_S}Leg"),
        f"calf_{_s}": (f"{_S}Leg", f"{_S}Foot"),
        f"foot_{_s}": (f"{_S}Foot", None),
        f"ball_{_s}": (f"{_S}ToeBase", None),
    })
# the joint each of our aligned bones points at, on our rig
CHILD = {}
for _s in ("l", "r"):
    CHILD.update({f"upperarm_{_s}": f"lowerarm_{_s}", f"lowerarm_{_s}": f"hand_{_s}", f"hand_{_s}": f"fingers_{_s}",
                  f"thigh_{_s}": f"calf_{_s}", f"calf_{_s}": f"foot_{_s}"})

# CMU (x left, y up, z front) -> ours (x front, y left, z up)
C = np.array([[0.0, 0.0, 1.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]])


# ------------------------------------------------------------------------------------------------ small maths
def _norm(v):
    return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-12)


def swing(a, b):
    """The smallest rotation turning direction a onto b (both (..., 3)); returns (..., 3, 3)."""
    a, b = _norm(np.asarray(a, float)), _norm(np.asarray(b, float))
    v = np.cross(a, b)
    c = np.sum(a * b, axis=-1)
    K = np.zeros(v.shape[:-1] + (3, 3))
    K[..., 0, 1], K[..., 0, 2], K[..., 1, 2] = -v[..., 2], v[..., 1], -v[..., 0]
    K[..., 1, 0], K[..., 2, 0], K[..., 2, 1] = v[..., 2], -v[..., 1], v[..., 0]
    f = 1.0 / np.maximum(1.0 + c, 1e-9)
    R = np.eye(3) + K + K @ K * f[..., None, None]
    return R


def axis_angle(axis, ang):
    """Rotation matrices about unit axes (..., 3) by angles (...) in radians."""
    axis = _norm(np.asarray(axis, float))
    ang = np.asarray(ang, float)
    x, y, z = axis[..., 0], axis[..., 1], axis[..., 2]
    c, s = np.cos(ang), np.sin(ang)
    t = 1 - c
    R = np.empty(np.broadcast(x, ang).shape + (3, 3))
    R[..., 0, 0], R[..., 0, 1], R[..., 0, 2] = t * x * x + c, t * x * y - s * z, t * x * z + s * y
    R[..., 1, 0], R[..., 1, 1], R[..., 1, 2] = t * x * y + s * z, t * y * y + c, t * y * z - s * x
    R[..., 2, 0], R[..., 2, 1], R[..., 2, 2] = t * x * z - s * y, t * y * z + s * x, t * z * z + c
    return R


def rot_z(ang):
    return axis_angle(np.array([0.0, 0.0, 1.0]), ang)


def mat_to_quat(M):
    """(..., 3, 3) -> (..., 4) as w, x, y, z with w >= 0."""
    M = np.asarray(M, float)
    sh = M.shape[:-2]
    M = M.reshape(-1, 3, 3)
    q = np.empty((len(M), 4))
    tr = M[:, 0, 0] + M[:, 1, 1] + M[:, 2, 2]
    for i in range(len(M)):
        m = M[i]
        if tr[i] > 0:
            s = math.sqrt(tr[i] + 1.0) * 2
            q[i] = (0.25 * s, (m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s)
        elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
            s = math.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
            q[i] = ((m[2, 1] - m[1, 2]) / s, 0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s)
        elif m[1, 1] > m[2, 2]:
            s = math.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
            q[i] = ((m[0, 2] - m[2, 0]) / s, (m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s)
        else:
            s = math.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
            q[i] = ((m[1, 0] - m[0, 1]) / s, (m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s)
    q /= np.linalg.norm(q, axis=1, keepdims=True)
    q[q[:, 0] < 0] *= -1
    return q.reshape(sh + (4,))


def quat_to_mat(q):
    q = _norm(np.asarray(q, float))
    w, x, y, z = q[..., 0], q[..., 1], q[..., 2], q[..., 3]
    R = np.empty(q.shape[:-1] + (3, 3))
    R[..., 0, 0], R[..., 0, 1], R[..., 0, 2] = 1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)
    R[..., 1, 0], R[..., 1, 1], R[..., 1, 2] = 2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)
    R[..., 2, 0], R[..., 2, 1], R[..., 2, 2] = 2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)
    return R


def rot_log(R):
    """Rotation vectors (axis * angle) of (..., 3, 3)."""
    q = mat_to_quat(R)
    v = q[..., 1:]
    s = np.linalg.norm(v, axis=-1, keepdims=True)
    ang = 2 * np.arctan2(s, q[..., :1])
    return v / np.maximum(s, 1e-12) * ang


def rot_exp(v):
    v = np.asarray(v, float)
    ang = np.linalg.norm(v, axis=-1)
    return axis_angle(np.where(ang[..., None] > 1e-12, v, np.array([1.0, 0, 0])), ang)


def slerp_mats(A, B, t):
    """Rotation part way from A to B (t in 0..1, broadcast over leading axes)."""
    t = np.asarray(t, float)
    return A @ rot_exp(rot_log(np.swapaxes(A, -1, -2) @ B) * t[..., None])


def smooth(x, k):
    """A centred moving average of length 2k+1 along axis 0 (ends held)."""
    if k <= 0:
        return x
    pad = np.concatenate([np.repeat(x[:1], k, 0), x, np.repeat(x[-1:], k, 0)], 0)
    c = np.cumsum(np.concatenate([np.zeros((1,) + x.shape[1:]), pad], 0), 0)
    return (c[2 * k + 1:] - c[:-2 * k - 1]) / (2 * k + 1)


# ------------------------------------------------------------------------------------------------ BVH
def parse_bvh(path):
    """A BVH file -> dict(names, parents, offsets, chans, data (frames x channels), frame_time)."""
    toks = open(path).read().split()
    i = 0
    names, parents, offsets, chans, stack, ends = [], [], [], [], [], {}
    while True:
        t = toks[i]
        i += 1
        if t in ("ROOT", "JOINT"):
            names.append(toks[i])
            parents.append(stack[-1] if stack else -1)
            assert toks[i + 1] == "{" and toks[i + 2] == "OFFSET"
            offsets.append([float(x) for x in toks[i + 3:i + 6]])
            assert toks[i + 6] == "CHANNELS"
            n = int(toks[i + 7])
            chans.append(toks[i + 8:i + 8 + n])
            i += 8 + n
            stack.append(len(names) - 1)
        elif t == "End":
            ends[stack[-1]] = [float(x) for x in toks[i + 3:i + 6]]
            i += 7
        elif t == "}":
            stack.pop()
        elif t == "MOTION":
            break
    nf = int(toks[i + 1])
    ft = float(toks[i + 4])
    nc = sum(len(c) for c in chans)
    data = np.array(toks[i + 5:i + 5 + nf * nc], dtype=float).reshape(nf, nc)
    return {"names": names, "parents": parents, "offsets": np.array(offsets), "chans": chans, "ends": ends,
            "data": data, "frame_time": ft}


def _axis_rot(ax, deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    R = np.zeros(a.shape + (3, 3))
    i, j = {"X": (1, 2), "Y": (2, 0), "Z": (0, 1)}[ax]
    k = "XYZ".index(ax)
    R[..., k, k] = 1
    R[..., i, i], R[..., i, j], R[..., j, i], R[..., j, j] = c, -s, s, c
    return R


def bvh_fk(b):
    """World rotations (F, J, 3, 3) and positions (F, J, 3) of every joint, turned into OUR axes (source units)."""
    data = b["data"]
    F, J = len(data), len(b["names"])
    W = np.zeros((F, J, 3, 3))
    P = np.zeros((F, J, 3))
    c0 = 0
    for j in range(J):
        R = np.tile(np.eye(3), (F, 1, 1))
        pos = np.zeros((F, 3))
        for k, ch in enumerate(b["chans"][j]):
            v = data[:, c0 + k]
            if ch.endswith("position"):
                pos[:, "XYZ".index(ch[0])] = v
            else:
                R = R @ _axis_rot(ch[0], v)
        c0 += len(b["chans"][j])
        p = b["parents"][j]
        if p < 0:
            W[:, j], P[:, j] = R, b["offsets"][j] + pos
        else:
            W[:, j] = W[:, p] @ R
            P[:, j] = P[:, p] + np.einsum("fij,fj->fi", W[:, p], b["offsets"][j] + pos)
    return np.einsum("ij,fkjl->fkil", C, W), np.einsum("ij,fkj->fki", C, P)


def bvh_path(take):
    return os.path.join(BVH_DIR, take + ".bvh")


# ------------------------------------------------------------------------------------------------ our rig at rest
class Rest:
    """Our rig's rest pose as arrays: bone order (parents first), parent index, head (B, 3), rotation (B, 3, 3),
    plus the sole points of each foot (in the foot bone's and the ball bone's own frames) for the ground."""

    def __init__(self, bones, sole=None):
        order, seen = [], set()
        while len(order) < len(bones):
            for n, d in bones.items():
                if n not in seen and (d["parent"] is None or d["parent"] in seen):
                    order.append(n)
                    seen.add(n)
        self.names = order
        self.ix = {n: i for i, n in enumerate(order)}
        self.parent = [self.ix[bones[n]["parent"]] if bones[n]["parent"] else -1 for n in order]
        self.head = np.array([bones[n]["head"] for n in order], float)
        self.tail = np.array([bones[n]["tail"] for n in order], float)
        self.R = np.array([bones[n]["matrix"] for n in order], float)
        self.sole = sole or {}

    @classmethod
    def of(cls, armature, body=None):
        """From a human.build armature (and its body mesh, for the soles)."""
        bones = {b.name: {"parent": b.parent.name if b.parent else None, "head": list(b.head_local),
                          "tail": list(b.tail_local), "matrix": [list(r) for r in b.matrix_local.to_3x3()]}
                 for b in armature.data.bones}
        sole = {}
        if body is not None:
            sole = _sole_points(body, bones)
        return cls(bones, sole)

    @classmethod
    def load(cls, path=REST_REF):
        """From a saved rest (names, parent names, head, tail, R lists): by default the reference man clips.json is
        built on, human.build(seed=0, stand=0.02, curl=(30, 45, 25)), so clips can be read without Blender."""
        d = json.load(open(path))
        return cls({n: {"parent": p, "head": h, "tail": t, "matrix": R}
                    for n, p, h, t, R in zip(d["names"], d["parent"], d["head"], d["tail"], d["R"])})

    def i(self, n):
        return self.ix[n]

    def leg(self, s="l"):
        h = self.head
        return (np.linalg.norm(h[self.i("calf_" + s)] - h[self.i("thigh_" + s)]) +
                np.linalg.norm(h[self.i("foot_" + s)] - h[self.i("calf_" + s)]))

    def local(self, b):
        """A bone's rest rotation in its parent's frame."""
        p = self.parent[b]
        return self.R[b] if p < 0 else self.R[p].T @ self.R[b]


def _sole_points(body, bones):
    """The underside of each bare foot: the body's points weighted mostly to foot_* or ball_* within 1.5 cm of the
    lowest, in that bone's own rest frame (so the ground test follows the bone)."""
    vg = {g.index: g.name for g in body.vertex_groups}
    pts = {}
    for v in body.data.vertices:
        best = max(v.groups, key=lambda g: g.weight, default=None)
        if best is None or best.weight < 0.5:
            continue
        n = vg[best.group]
        if n.startswith(("foot_", "ball_")):
            pts.setdefault(n, []).append(tuple(v.co))
    out = {}
    lo = min(p[2] for P in pts.values() for p in P)
    for n, P in pts.items():
        P = np.array(P)
        P = P[P[:, 2] < lo + 0.015]
        if len(P) > 60:
            P = P[np.linspace(0, len(P) - 1, 60).astype(int)]
        R = np.array(bones[n]["matrix"])
        out[n] = (P - np.array(bones[n]["head"])) @ R            # rows: R^T (p - head)
    out["ground"] = lo
    return out


# ------------------------------------------------------------------------------------------------ retarget
class Motion:
    """Our rig in motion: world (armature-space) rotation of every bone W (F, B, 3, 3) and the pelvis head H (F, 3),
    at `fps`. Bones the mocap does not drive follow their parents."""

    def __init__(self, rest, W, H, fps):
        self.rest, self.W, self.H, self.fps = rest, W, H, fps

    def __len__(self):
        return len(self.H)

    def copy(self):
        return Motion(self.rest, self.W.copy(), self.H.copy(), self.fps)

    def heads(self):
        """Every bone's head (F, B, 3) by forward kinematics."""
        r = self.rest
        F = len(self.H)
        out = np.zeros((F, len(r.names), 3))
        for b, p in enumerate(r.parent):
            if p < 0:
                out[:, b] = r.head[b]
            elif r.names[b] == "pelvis":
                out[:, b] = self.H
            else:
                out[:, b] = out[:, p] + np.einsum("fij,j->fi", self.W[:, p] @ r.R[p].T, r.head[b] - r.head[p])
        return out

    def point(self, bone, local_pts, heads=None):
        """Points given in a bone's rest frame (K, 3), posed: (F, K, 3)."""
        r = self.rest
        b = r.i(bone)
        hd = (heads if heads is not None else self.heads())[:, b]
        return hd[:, None, :] + np.einsum("fij,kj->fki", self.W[:, b], local_pts)

    def follow_parents(self, bones=None):
        """Re-derive the bones the mocap does not drive from their (possibly changed) parents."""
        r = self.rest
        for b, p in enumerate(r.parent):
            n = r.names[b]
            if p >= 0 and n not in MAP and (bones is None or n in bones):
                self.W[:, b] = self.W[:, p] @ r.R[p].T @ r.R[b]

    def local_quats(self):
        """Blender pose-bone quaternions (F, B, 4): the bone's rotation relative to its rest, in its own rest frame."""
        r = self.rest
        F, B = len(self.H), len(r.names)
        Q = np.zeros((F, B, 4))
        for b, p in enumerate(r.parent):
            if p < 0:
                L = np.swapaxes(r.R[b], -1, -2) @ self.W[:, b]
            else:
                L = np.swapaxes(r.R[p].T @ r.R[b], -1, -2) @ np.swapaxes(self.W[:, p], -1, -2) @ self.W[:, b]
            Q[:, b] = mat_to_quat(L)
        return Q

    def parent_deltas(self):
        """The clips.json form: per bone the rotation D with local rotation = D * RestLocal (parent's space)."""
        r = self.rest
        out = {}
        for b, p in enumerate(r.parent):
            Wp = self.W[:, p] if p >= 0 else np.eye(3)
            D = np.swapaxes(Wp, -1, -2) @ self.W[:, b] @ r.local(b).T
            out[r.names[b]] = mat_to_quat(D)
        return out

    def slice(self, a, b):
        out = Motion(self.rest, self.W[a:b].copy(), self.H[a:b].copy(), self.fps)
        out.scale = getattr(self, "scale", 1.0)
        return out


def retarget(bvh, rest, tpose=0):
    """CMU take -> Motion on our rig at the take's own rate (120 fps), facing and travelling as captured."""
    Ws, Ps = bvh_fk(bvh)
    n = bvh["names"]
    J = {nm: k for k, nm in enumerate(n)}
    o = bvh["offsets"]
    leg_src = np.mean([np.linalg.norm(o[J[f"{S}Leg"]]) + np.linalg.norm(o[J[f"{S}Foot"]]) for S in ("Left", "Right")])
    scale = 0.5 * (rest.leg("l") + rest.leg("r")) / leg_src
    r = rest
    B = len(r.names)
    # the matched pose: our rest with the limbs swung onto CMU's T-pose directions, parent before child
    RT = np.zeros((B, 3, 3))
    for b, p in enumerate(r.parent):
        nm = r.names[b]
        if nm in MAP and MAP[nm][1]:
            Dp = RT[p] @ r.R[p].T
            d_rest = Dp @ (r.head[r.i(CHILD[nm])] - r.head[b])
            d_src = Ps[tpose, J[MAP[nm][1]]] - Ps[tpose, J[MAP[nm][0]]]
            RT[b] = swing(d_rest, d_src) @ Dp @ r.R[b]
        elif nm in MAP or p < 0:
            RT[b] = r.R[b]
        else:
            RT[b] = RT[p] @ r.R[p].T @ r.R[b]
    F = len(Ws)
    W = np.zeros((F, B, 3, 3))
    for b, p in enumerate(r.parent):
        nm = r.names[b]
        if p < 0:
            W[:, b] = r.R[b]
        elif nm in MAP:
            j = J[MAP[nm][0]]
            W[:, b] = Ws[:, j] @ Ws[tpose, j].T @ RT[b]
        else:
            W[:, b] = W[:, p] @ r.R[p].T @ r.R[b]
    # the pelvis: our hip joints' midpoint on CMU's, scaled by the legs
    mid_src = 0.5 * (Ps[:, J["LeftUpLeg"]] + Ps[:, J["RightUpLeg"]]) * scale
    pv = r.i("pelvis")
    mid_rest = 0.5 * (r.head[r.i("thigh_l")] + r.head[r.i("thigh_r")])
    v_local = r.R[pv].T @ (r.head[pv] - mid_rest)
    H = mid_src + np.einsum("fij,j->fi", W[:, pv], v_local)
    m = Motion(rest, W, H, round(1.0 / bvh["frame_time"]))
    m.scale = scale
    m.src_positions = Ps * scale
    m.src_names = n
    return m


# ------------------------------------------------------------------------------------------------ local form
def to_local(m):
    """Motion -> bone-local rotations L (F, B, 3, 3) (Blender's pose form) and pelvis head H."""
    r = m.rest
    L = np.zeros_like(m.W)
    for b, p in enumerate(r.parent):
        if p < 0:
            L[:, b] = r.R[b].T @ m.W[:, b]
        else:
            L[:, b] = (r.R[p].T @ r.R[b]).T @ np.swapaxes(m.W[:, p], -1, -2) @ m.W[:, b]
    return L


def from_local(rest, L, H, fps):
    W = np.zeros_like(L)
    for b, p in enumerate(rest.parent):
        W[:, b] = rest.R[b] @ L[:, b] if p < 0 else W[:, p] @ (rest.R[p].T @ rest.R[b]) @ L[:, b]
    return Motion(rest, W, H.copy(), fps)


# ------------------------------------------------------------------------------------------------ clip making
def yaw_of(m):
    """The way the pelvis faces, per frame (radians about z, 0 = +X)."""
    pv = m.rest.i("pelvis")
    fwd = np.einsum("fij,j->fi", m.W[:, pv] @ m.rest.R[pv].T, np.array([1.0, 0, 0]))
    return np.arctan2(fwd[:, 1], fwd[:, 0])


def turn(m, ang, about=None):
    """Turn the whole motion by `ang` about z (through `about`, default the origin)."""
    Rz = rot_z(ang)
    m.W = np.einsum("ij,fbjk->fbik", Rz, m.W)
    for b, p in enumerate(m.rest.parent):
        if p < 0:
            m.W[:, b] = m.rest.R[b]
    c = np.zeros(3) if about is None else np.asarray(about, float)
    m.H = (m.H - c) @ Rz.T + c
    if hasattr(m, "src_positions"):
        m.src_positions = (m.src_positions - c) @ Rz.T + c
    return m


def sole_heights(m, heads=None, side=None):
    """Lowest sole point per frame per foot: dict side -> (F,) z, and the points (F, K, 3)."""
    hd = m.heads() if heads is None else heads
    out = {}
    for s in ((side,) if side else ("l", "r")):
        P = np.concatenate([m.point("foot_" + s, m.rest.sole["foot_" + s], hd),
                            m.point("ball_" + s, m.rest.sole["ball_" + s], hd)], 1)
        out[s] = (P[:, :, 2].min(1), P)
    return out


def ground_fit(m, pct=20, both=False):
    """Lift or lower the whole man so his soles stand on the ground: the given percentile of the lower foot's sole
    height over the clip, or with `both` (standing still) of the higher one, so both feet reach the ground."""
    sh = sole_heights(m)
    low = (np.maximum if both else np.minimum)(sh["l"][0], sh["r"][0])
    dz = m.rest.sole["ground"] - np.percentile(low, pct)
    m.H[:, 2] += dz
    return dz


def features(m):
    """Per-frame pose features, independent of where he is and which way he faces: key joints relative to the
    pelvis, turned to his own heading, plus their velocities."""
    hd = m.heads()
    r = m.rest
    keys = [r.i(n) for n in ("hand_l", "hand_r", "lowerarm_l", "lowerarm_r", "calf_l", "calf_r", "foot_l", "foot_r",
                             "head", "chest")]
    yaw = np.unwrap(yaw_of(m))
    yaw = smooth(yaw[:, None], 12)[:, 0]
    rel = hd[:, keys] - m.H[:, None]
    c, s = np.cos(-yaw), np.sin(-yaw)
    x = rel[..., 0] * c[:, None] - rel[..., 1] * s[:, None]
    y = rel[..., 0] * s[:, None] + rel[..., 1] * c[:, None]
    pos = np.stack([x, y, rel[..., 2]], -1).reshape(len(hd), -1)
    pos = np.concatenate([pos, m.H[:, 2:3] * 2], 1)
    vel = np.gradient(pos, axis=0) * m.fps * 0.15
    return np.concatenate([pos, vel], 1)


def find_cycle(m, lo, hi, period=None, steps=1):
    """The best loop inside frames lo..hi: returns (a, b, cost) with frame b the closest match to frame a, about one
    (or `steps` full) gait period after it."""
    f = features(m)
    if period is None:
        hd = m.heads()
        z = hd[lo:hi, m.rest.i("foot_l"), 2] - hd[lo:hi, m.rest.i("foot_r"), 2]
        z = z - z.mean()
        ac = np.correlate(z, z, "full")[len(z) - 1:]
        mn = int(0.4 * m.fps)
        period = mn + int(np.argmax(ac[mn:int(2.0 * m.fps)]))
    best = (None, None, 1e9)
    for a in range(lo, hi):
        for b in range(a + int(0.85 * period * steps), min(hi, a + int(1.15 * period * steps)) + 1):
            c = np.sum((f[a] - f[b]) ** 2)
            if c < best[2]:
                best = (a, b, c)
    return best[0], best[1], best[2], period


def close_loop(m, travel=True):
    """Make frame N equal frame 0: remove the travel (in place), then spread the remaining difference of every
    bone's local rotation and of the pelvis over the cycle. m holds frames 0..N inclusive. Returns the stride."""
    N = len(m) - 1
    if travel:
        d = m.H[N] - m.H[0]
        turn(m, -math.atan2(d[1], d[0]))
    else:
        turn(m, -np.angle(np.mean(np.exp(1j * yaw_of(m)))))
    d = m.H[N] - m.H[0]
    stride = float(np.hypot(d[0], d[1])) if travel else 0.0
    t = np.arange(N + 1) / N
    if travel:
        m.H[:, :2] -= np.outer(t, d[:2])
    L = to_local(m)
    for b in range(L.shape[1]):
        E = L[0, b] @ L[N, b].T
        L[:, b] = rot_exp(np.outer(t, rot_log(E))) @ L[:, b]
    H = m.H.copy()
    H += np.outer(t, H[0] - H[N])
    out = from_local(m.rest, L, H, m.fps)
    out.H[:, :2] -= out.H[:N, :2].mean(0) - (m.rest.head[m.rest.i("pelvis")][:2])
    for k in ("scale",):
        if hasattr(m, k):
            setattr(out, k, getattr(m, k))
    return out, stride


def resample(m, times):
    """Motion at fractional source frames `times` (slerp of local rotations, lerp of the pelvis), at FPS."""
    L = to_local(m)
    i0 = np.clip(np.floor(times).astype(int), 0, len(m) - 1)
    i1 = np.clip(i0 + 1, 0, len(m) - 1)
    w = times - i0
    Lr = slerp_mats(L[i0], L[i1], np.broadcast_to(w[:, None], (len(times), L.shape[1])))
    H = m.H[i0] * (1 - w)[:, None] + m.H[i1] * w[:, None]
    out = from_local(m.rest, Lr, H, FPS)
    if hasattr(m, "scale"):
        out.scale = m.scale
    return out


KNEE_SIDE = 35.0      # degrees a leg's bend may lie off its kneecap's way (two_bone_ik; the hip turns the rest)


def two_bone_ik(m, side, target, weight, heads=None, chain=("thigh_", "calf_", "foot_"), pole=None):
    """Bend thigh and calf so the ankle reaches `target` (F, 3) where weight (F,) > 0, the knee staying in the plane
    the mocap gave it (a leg's: kept forward, within KNEE_SIDE of its kneecap's way); the foot keeps its world
    rotation. Soft near full stretch, so the knee never snaps. `pole` (F, 3 or 3,) sets the bend's way instead.
    chain=("upperarm_", "lowerarm_", "hand_") does the same for an arm."""
    r = m.rest
    th, ca, ft = (r.i(c + side) for c in chain)
    hd = m.heads() if heads is None else heads
    hip, knee, ank = hd[:, th], hd[:, ca], hd[:, ft]
    L1 = np.linalg.norm(r.head[ca] - r.head[th])
    L2 = np.linalg.norm(r.head[ft] - r.head[ca])
    T = ank + (target - ank) * weight[:, None]
    d = T - hip
    dist = np.linalg.norm(d, axis=1)
    Ls, s = L1 + L2, 0.012
    soft = Ls - s
    dist_e = np.where(dist > soft, Ls - s * np.exp(-(dist - soft) / s), dist)
    dist_e = np.maximum(dist_e, abs(L1 - L2) + 0.02)
    dh = _norm(d)
    if pole is None:
        pole = knee - hip
        pole = pole - dh * np.sum(pole * dh, 1)[:, None]
        if chain[0] == "thigh_":
            # A leg the capture holds straight shows no bend to keep, and its knee's tiny offset from the hip-ankle
            # line (noise, or a degree of lock) would steer the bend any way, even backward. Below 3 cm of offset
            # the kneecap's own way takes over: square to the knee's hinge (the thigh's y, as it is turned) and to
            # the hip-ankle line, which stays well defined however deep the bend.
            kh = np.einsum("fij,j->fi", m.W[:, th] @ r.R[th].T, np.array((0.0, 1.0, 0.0)))
            kc = _norm(np.cross(dh, kh))
            off = np.linalg.norm(pole, axis=1)
            u = np.clip(1.0 - off / 0.03, 0.0, 1.0)
            pole = pole + kc * (0.03 * u * u * (3 - 2 * u))[:, None]
            # and a knee bends only forward over its foot: one behind the hip-ankle line (locked back) is mirrored
            # to the front, and in frames where a captured knee lies off to one side of the kneecap by more than
            # KNEE_SIDE degrees (a glitch, or the actor's straight leg bent by our IK) its way is taken from the good
            # frames either side
            sd = np.cross(dh, kc)
            phi = np.arctan2(np.sum(pole * sd, 1), np.sum(pole * kc, 1))
            phi = np.where(np.abs(phi) > np.pi / 2, np.sign(phi) * np.pi - phi, phi)
            lim = math.radians(KNEE_SIDE)
            bad = np.abs(phi) > lim
            if bad.any() and not bad.all():
                f_ = np.arange(len(phi))
                phi = np.where(bad, np.interp(f_, f_[~bad], phi[~bad]), phi)
            phi = np.clip(phi, -lim, lim)
            pole = kc * np.cos(phi)[:, None] + sd * np.sin(phi)[:, None]
    else:
        pole = np.broadcast_to(pole, knee.shape)
        pole = pole - dh * np.sum(pole * dh, 1)[:, None]
    ph = _norm(pole)
    cos_a = np.clip((L1 ** 2 + dist_e ** 2 - L2 ** 2) / (2 * L1 * dist_e), -1, 1)
    sin_a = np.sqrt(1 - cos_a ** 2)
    tdir = dh * cos_a[:, None] + ph * sin_a[:, None]
    knee_n = hip + tdir * L1
    ank_n = hip + dh * dist_e[:, None]
    G = swing(knee - hip, tdir)
    Wt = G @ m.W[:, th]
    cdir_old = np.einsum("fij,fj->fi", G, ank - knee)
    Gc = swing(cdir_old, ank_n - knee_n)
    Wc = Gc @ G @ m.W[:, ca]
    m.W[:, th], m.W[:, ca] = Wt, Wc
    return np.linalg.norm(ank_n - T, axis=1)


# How far above the ground each joint must stay when the body is down on it (its flesh, at our man's build):
# lying on the belly the pelvis is about 11 cm up, the chest 15, a knee or an elbow on the ground 4-5.
CLEAR = {"pelvis": 0.11, "spine_01": 0.12, "spine_02": 0.14, "chest": 0.15, "neck": 0.12, "head": 0.10,
         "upperarm_l": 0.07, "upperarm_r": 0.07, "lowerarm_l": 0.04, "lowerarm_r": 0.04,
         "calf_l": 0.05, "calf_r": 0.05}


def body_clear(m, k=4):
    """Lift the whole man (smoothly) wherever a joint of his trunk, a knee or an elbow would sit deeper than its
    flesh allows: the actor's proportions are not ours, so on the floor ours would sink. Returns the lift."""
    r = m.rest
    g = r.sole["ground"] - 0.02                     # (the bare skin's ground: `stand` lifts only the boots' soles)
    hd = m.heads()
    need = np.zeros(len(m))
    for n, c in CLEAR.items():
        need = np.maximum(need, g + c - hd[:, r.i(n), 2])
    need = np.maximum(need, 0.0)
    wide = np.max(np.stack([np.concatenate([need[i:], np.repeat(need[-1:], i)]) if i >= 0 else
                            np.concatenate([np.repeat(need[:1], -i), need[:i]]) for i in range(-k, k + 1)]), 0)
    lift = smooth(wide[:, None], k)[:, 0]
    m.H[:, 2] += lift
    return lift


def hands_clear(m, clear=0.035):
    """Keep each wrist at least `clear` above the ground (a hand flat on it), by two-bone IK on the arm."""
    r = m.rest
    g = r.sole["ground"] - 0.02
    for s in ("l", "r"):
        hd = m.heads()
        w = hd[:, r.i("hand_" + s)]
        pen = np.maximum(0.0, g + clear - w[:, 2])
        if pen.max() < 1e-4:
            continue
        pen = smooth(pen[:, None], 2)[:, 0]
        two_bone_ik(m, s, w + np.outer(pen, [0, 0, 1]), (pen > 0).astype(float), hd,
                    chain=("upperarm_", "lowerarm_", "hand_"))


def hand_steps(m, stride, lift=0.05, knee_v=0.3, pad=2):
    """On hands and knees (a loop): each hand planted on the ground, then stepping forward while the knee of the
    other side moves (the crawl's diagonal gait), lifted `lift` at the top of its step. (The crawl's actor slid her
    hands along the floor; ours step.) By two-bone IK on the arms; the hands keep their captured angle."""
    r = m.rest
    N = len(m) - 1
    v = stride / N
    hd = m.heads()
    out = {}
    for s, o in (("l", "r"), ("r", "l")):
        kp = hd[:N, r.i("calf_" + o)]
        ksp = np.linalg.norm((np.roll(kp, -1, 0) - kp)[:, :2] + np.array([v, 0.0]), axis=1) * m.fps   # travelling
        moving = ksp > knee_v
        if not moving.any():
            continue
        # the longest run of the knee moving, round the loop
        best, start = (0, 0), None
        for k in range(2 * N):
            if moving[k % N]:
                start = k if start is None else start
                if k - start + 1 > best[1] - best[0]:
                    best = (start, k + 1)
            else:
                start = None
        a, b = best[0] - pad, min(best[1] + pad, best[0] - pad + N - 4)
        L, S = b - a, N - (b - a)
        w = hd[:N, r.i("hand_" + s)]
        k_of = (np.arange(N) - b) % N                                # frames since the stance began
        st = k_of < S
        x0 = float(np.mean(w[st, 0])) + v * (S - 1) / 2             # (the planted hand's mean where it was)
        y0 = float(np.mean(w[st, 1]))
        T = w.copy()
        T[st, 0] = x0 - v * k_of[st]
        T[st, 1] = y0
        j = k_of[~st] - S
        u = (j + 1) / (L + 1)
        e = u * u * (3 - 2 * u)
        T[~st, 0] = (x0 - v * S) * (1 - e) + x0 * e
        T[~st, 1] = y0
        T[~st, 2] = w[~st, 2] + lift * np.sin(np.pi * u)
        T = np.concatenate([T, T[:1]])
        two_bone_ik(m, s, T, np.ones(N + 1), hd, chain=("upperarm_", "lowerarm_", "hand_"))
        hd = m.heads()
        out[s] = (int(a % N), int(b % N))
    m.W[-1], m.H[-1] = m.W[0], m.H[0]
    return out


def contacts(h, speed, ground, h_tol=0.035, v_tol=0.45, min_len=3):
    """Frames where a foot is on the ground: low and slow, in runs of at least min_len."""
    c = (h < ground + h_tol) & (speed < v_tol)
    out, k = [], 0
    while k < len(c):
        if c[k]:
            j = k
            while j < len(c) and c[j]:
                j += 1
            if j - k >= min_len:
                out.append((k, j))
            k = j
        else:
            k += 1
    return out


def foot_lock(m, stride=0.0, loop=False, h_tol=0.035, v_tol=0.45, sides=("l", "r"), report=None, reach=0.996,
              merge=2, glide=0):
    """Plant the feet: while a foot is on the ground its sole stays where it landed (in the travelling frame, so in
    place it slides back at exactly the body's speed), first about the ankle, then about the ball as the heel lifts.
    Where a planted leg could not reach its foot (our man's legs and hips are not the actor's), the pelvis dips just
    enough. For a loop the cycle is tiled three times so contacts across the seam lock too. Then the toes are kept
    out of the ground and nothing sinks below it."""
    r = m.rest
    ground = r.sole["ground"]
    N = len(m) - 1 if loop else len(m)
    if loop:
        base = m.slice(0, N)
        work = Motion(r, np.concatenate([base.W] * 3), np.concatenate([base.H] * 3), m.fps)
    else:
        work = m
    F = len(work)
    v_frame = stride / N if loop else 0.0
    tt = np.arange(F)
    hd = work.heads()
    stats, plan = {}, {}
    for s in sides:
        ft, bl = r.i("foot_" + s), r.i("ball_" + s)
        sh, P = sole_heights(work, hd, s)[s]
        trav = hd[:, ft] + np.outer(tt * v_frame, [1, 0, 0])
        # the toe-off pivot: under the ball joint, on the sole, in the foot's frame
        pivot_rest = r.head[bl].copy()
        pivot_rest[2] = ground
        ball_loc = r.R[ft].T @ (pivot_rest - r.head[ft])
        ball_tr = trav + np.einsum("fij,j->fi", work.W[:, ft], ball_loc)
        spd = np.minimum(np.linalg.norm(np.gradient(trav[:, :2], axis=0), axis=1),
                         np.linalg.norm(np.gradient(ball_tr[:, :2], axis=0), axis=1)) * work.fps
        # each foot measured from its own lowest (a retargeted foot can stand a little high or low)
        base_h = min(float(np.percentile(sh, 3)), ground + 0.06)
        cs = contacts(sh, spd, base_h, h_tol, v_tol)
        merged = []
        for c in cs:                                                   # (a flicker of a frame or two is not a step)
            if merged and c[0] - merged[-1][1] <= merge:
                merged[-1] = (merged[-1][0], c[1])
            else:
                merged.append(c)
        cs = merged
        target = hd[:, ft].copy()
        wgt = np.zeros(F)
        fsole = r.sole["foot_" + s]
        heel_loc_f = fsole[np.argmin(fsole[:, 1])]                     # the rearmost point of the foot's sole
        for (c0, c1) in cs:
            idx = np.arange(c0, c1)
            Wf = work.W[idx, ft]
            heel_z = (hd[idx, ft] + np.einsum("fij,j->fi", Wf, heel_loc_f))[:, 2]
            ball_z = (hd[idx, ft] + np.einsum("fij,j->fi", Wf, ball_loc))[:, 2]
            toe_off = (heel_z - ball_z) > 0.025
            # only the end of the contact can be toe-off; a heel-up start (landing on the ball) counts as flat
            flat = ~toe_off
            last = np.where(flat)[0][-1] if flat.any() else 0
            flat = np.arange(len(idx)) <= last
            fi = idx[flat]
            X = trav[fi].mean(0)
            X[2] = (hd[fi, ft, 2] - (sh[fi] - ground)).mean()
            last_flat = fi[-1]
            target[fi] = X - np.outer(fi * v_frame, [1, 0, 0])
            # the toe-off: the ball stays where the locked foot put it, the foot turning about it
            if (idx > last_flat).any():
                Xb = X + work.W[last_flat, ft] @ ball_loc
                Xb[2] = max(Xb[2], ground + 0.002)
                for f in idx[idx > last_flat]:
                    target[f] = Xb - work.W[f, ft] @ ball_loc - np.array([f * v_frame, 0, 0])
            wgt[c0:c1] = 1.0
            for k in range(1, 4):                                   # ease in and out over three frames
                w_ = 1.0 - k / 4.0
                if c0 - k >= 0 and wgt[c0 - k] < w_:
                    wgt[c0 - k] = w_
                    target[c0 - k] = target[c0] + (hd[c0 - k, ft] - hd[c0, ft])
                if c1 - 1 + k < F and wgt[c1 - 1 + k] < w_:
                    wgt[c1 - 1 + k] = w_
                    target[c1 - 1 + k] = target[c1 - 1] + (hd[c1 - 1 + k, ft] - hd[c1 - 1, ft])
        plan[s] = (target, wgt, cs)
    # the pelvis dips where a planted leg would have to stretch past `reach` of its length
    drop = np.zeros(F)
    for s in sides:
        target, wgt, _ = plan[s]
        th = r.i("thigh_" + s)
        Lleg = r.leg(s)
        T = hd[:, r.i("foot_" + s)] + (target - hd[:, r.i("foot_" + s)]) * wgt[:, None]
        d = T - hd[:, th]
        horiz2 = d[:, 0] ** 2 + d[:, 1] ** 2
        zmax = np.sqrt(np.maximum((reach * Lleg) ** 2 - horiz2, 0.0))
        need = np.maximum(0.0, -d[:, 2] - zmax) * (wgt > 0)
        drop = np.maximum(drop, need)
    if drop.max() > 0:
        k = 3
        wide = np.max(np.stack([np.roll(drop, i) for i in range(-k, k + 1)]), 0) if loop else \
            np.max(np.stack([np.concatenate([drop[i:], np.repeat(drop[-1:], i)]) if i >= 0 else
                             np.concatenate([np.repeat(drop[:1], -i), drop[:i]]) for i in range(-k, k + 1)]), 0)
        drop = smooth(wide[:, None], k)[:, 0]
        work.H[:, 2] -= drop
        hd = work.heads()
    for s in sides:
        target, wgt, cs = plan[s]
        ft, bl = r.i("foot_" + s), r.i("ball_" + s)
        err = two_bone_ik(work, s, target, wgt, hd)
        # the toes lie flat on the ground while the foot is on it (turned only with the foot's heading)
        fwd = np.einsum("fij,j->fi", work.W[:, ft] @ r.R[ft].T, np.array([1.0, 0, 0]))
        Wflat = rot_z(np.arctan2(fwd[:, 1], fwd[:, 0])) @ r.R[bl]
        work.W[:, bl] = slerp_mats(work.W[:, bl], Wflat, wgt)
        sl = slice(N, 2 * N) if loop else slice(0, F)
        stats[s] = {"contacts": [(int(a) - (N if loop else 0), int(b) - (N if loop else 0)) for a, b in cs
                                 if (not loop) or (N <= a < 2 * N)],
                    "ik_err_max": round(float(err[sl][wgt[sl] > 0].max()) if (wgt[sl] > 0).any() else 0.0, 4)}
        hd = work.heads()
    stats["pelvis_drop_max"] = round(float(drop[N:2 * N].max() if loop else drop.max()), 4)
    # nothing below the ground: lift a sinking foot by IK, and bend a sinking toe up. With `glide` (the loops: a man
    # on his feet), a landing foot glides down onto the ground over that many frames before its contact (our heel,
    # tipped up, reaches lower than the actor's did, so it would otherwise touch while still moving and skid in)
    for s in sides:
        floor = np.full(F, ground)
        sh0 = sole_heights(work, hd, s)[s][0]
        prev_end = 0
        for (c0, c1) in plan[s][2]:
            a = max(c0 - glide, prev_end + 1)
            if c0 - a >= 2:
                H = max(sh0[a] - ground, 0.0)
                f = np.arange(a, c0)
                floor[f] = np.maximum(floor[f], ground + H * ((c0 - f) / (c0 - a)) ** 2)
            prev_end = c1
        for _ in range(2):
            sh, P = sole_heights(work, hd, s)[s]
            pen = np.maximum(0.0, floor - sh)
            if pen.max() < 0.002:
                break
            ft = r.i("foot_" + s)
            two_bone_ik(work, s, hd[:, ft] + np.outer(pen, [0, 0, 1]), (pen > 0).astype(float), hd)
            hd = work.heads()
        _toe_clear(work, s, hd)
    if loop:
        m.W[:] = np.concatenate([work.W[N:2 * N], work.W[N:N + 1]])
        m.H[:] = np.concatenate([work.H[N:2 * N], work.H[N:N + 1]])
    if report is not None:
        report.update(stats)
    return stats


def _toe_clear(m, s, hd):
    """Bend the toes up wherever their sole would go below the ground (about the ball joint's crosswise axis)."""
    r = m.rest
    ground = r.sole["ground"]
    bl, ft = r.i("ball_" + s), r.i("foot_" + s)
    pts = r.sole["ball_" + s]
    side_ax = np.einsum("fij,j->fi", m.W[:, ft] @ r.R[ft].T, np.array([0.0, 1.0, 0.0]))
    best = np.zeros(len(m))
    for ang in np.radians(np.arange(0, 61, 3)):
        R = axis_angle(side_ax, -ang)
        Wb = R @ m.W[:, bl]
        z = (hd[:, bl, None, :] + np.einsum("fij,kj->fki", Wb, pts))[:, :, 2].min(1)
        ok = (z >= ground - 0.002) & (best == 0)
        best[ok] = ang if ang > 0 else 1e-9
    best[best == 0] = math.radians(60)
    best[best < 1e-6] = 0
    m.W[:, bl] = axis_angle(side_ax, -best) @ m.W[:, bl]


# ------------------------------------------------------------------------------------------------ Blender
def new_action(armature, name, quats, locs=None, frame0=1, cyclic=False):
    """An action on `armature` from pose-bone quaternions {bone: (F, 4)} and pelvis pose locations {bone: (F, 3)},
    one key per frame from `frame0`, written straight into the action's channel bag (Blender 4.4+ layered actions)."""
    import bpy
    old = bpy.data.actions.get(name)
    if old:
        bpy.data.actions.remove(old)
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    slot = act.slots.new(id_type="OBJECT", name=armature.name)
    strip = act.layers.new("mocap").strips.new(type="KEYFRAME")
    bag = strip.channelbag(slot, ensure=True)

    def curve(path, idx, values, group):
        fc = bag.fcurves.new(path, index=idx)
        fc.group = bag.groups.get(group) or bag.groups.new(group)
        n = len(values)
        fc.keyframe_points.add(n)
        co = np.empty(2 * n)
        co[0::2] = np.arange(frame0, frame0 + n)
        co[1::2] = values
        fc.keyframe_points.foreach_set("co", co.tolist())
        fc.keyframe_points.foreach_set("interpolation", [1] * n)       # LINEAR (enum index), keys every frame
        if cyclic:
            fc.modifiers.new("CYCLES")
        fc.update()

    for bone, q in quats.items():
        q = np.array(q, float)
        for k in range(1, len(q)):                                     # (no sign flips between keys)
            if np.dot(q[k], q[k - 1]) < 0:
                q[k] = -q[k]
        for i in range(4):
            curve(f'pose.bones["{bone}"].rotation_quaternion', i, q[:, i], bone)
    for bone, l in (locs or {}).items():
        l = np.asarray(l, float)
        for i in range(3):
            curve(f'pose.bones["{bone}"].location', i, l[:, i], bone)
    n = len(next(iter(quats.values())))
    act.frame_start, act.frame_end = frame0, frame0 + n - 1
    act.use_frame_range = True
    act.use_cyclic = cyclic
    if armature.animation_data is None:
        armature.animation_data_create()
    armature.animation_data.action = act
    try:
        armature.animation_data.action_slot = slot
    except Exception:
        pass
    for pb in armature.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return act


def motion_action(armature, name, m, loop=False):
    """A Motion (on a rig with the same rest as `armature`) as an action."""
    Q = m.local_quats()
    r = m.rest
    pv = r.i("pelvis")
    quats = {n: Q[:, b] for b, n in enumerate(r.names) if r.parent[b] >= 0}
    locs = {"pelvis": (m.H - r.head[pv]) @ r.R[pv]}                    # rows: R^T (H - rest head)
    return new_action(armature, name, quats, locs, cyclic=loop)


def load_clips(path=CLIPS_JSON):
    return json.load(open(path))


def motion_of_clip(clip, rest=None, clips=None):
    """A clips.json clip (a name or the dict) as a Motion on `rest` (the reference man's by default), no Blender."""
    if isinstance(clip, str):
        clip = (clips or load_clips())[clip]
    rest = rest or Rest.load()
    F = int(clip["frames"])
    k = 0.5 * (rest.leg("l") + rest.leg("r")) / clip.get("leg", 0.5 * (rest.leg("l") + rest.leg("r")))
    W = np.zeros((F, len(rest.names), 3, 3))
    for b, p in enumerate(rest.parent):
        n = rest.names[b]
        D = quat_to_mat(np.array(clip["bones"][n])) if n in clip["bones"] else np.eye(3)
        W[:, b] = (W[:, p] if p >= 0 else np.eye(3)) @ D @ rest.local(b)
    pv = rest.i("pelvis")
    m = Motion(rest, W, rest.head[pv] + np.array(clip["pelvis_loc"]) * k, int(clip["fps"]))
    m.loop = bool(clip.get("loop"))
    return m


def carried(clip, carry, until=None, blend=0.3, rest=None, clips=None):
    """A clip with carry_rifle laid over it, as the soldier plays it (soldier.MOCAP): the whole clip, or, given
    `until` (s), the arms going back to the capture over `blend` seconds from then (going prone, the falls).
    Returns (Motion, hand_error (F, 2))."""
    m = motion_of_clip(clip, rest, clips)
    mc, _, err = carry_rifle(m.copy(), carry, cyclic=m.loop)
    if until is not None:
        t = np.arange(len(m)) / m.fps
        w = np.clip((t - until) / blend, 0.0, 1.0)
        w = w * w * (3 - 2 * w)
        mc = from_local(m.rest, slerp_mats(to_local(mc), to_local(m), w[:, None]), m.H, m.fps)
        err = err * (1.0 - w)[:, None]
    mc.loop = m.loop
    return mc, err


def apply(armature, clip, clips=None, name=None):
    """Make the action for `clip` (a name in clips.json, or a clip dict) on any human.build armature."""
    if clips is None and isinstance(clip, str):
        clips = load_clips()
    c = clips[clip] if isinstance(clip, str) else clip
    rest = Rest.of(armature)
    k = 0.5 * (rest.leg("l") + rest.leg("r")) / c.get("leg", 0.5 * (rest.leg("l") + rest.leg("r")))
    quats = {}
    for b, n in enumerate(rest.names):
        if rest.parent[b] < 0 or n not in c["bones"]:
            continue
        D = quat_to_mat(np.array(c["bones"][n]))
        Lr = rest.local(b)
        quats[n] = mat_to_quat(Lr.T @ D @ Lr)
    pv = rest.i("pelvis")
    loc = np.array(c["pelvis_loc"]) * k
    act = new_action(armature, name or (clip if isinstance(clip, str) else c.get("name", "clip")), quats,
                     {"pelvis": loc @ rest.R[pv]}, cyclic=bool(c.get("loop")))
    return act


def apply_all(armature, clips=None):
    clips = clips or load_clips()
    return [apply(armature, n, clips) for n in clips if not n.startswith("_")]


# ------------------------------------------------------------------------------------------------ the clips
CMU_UNIT = 0.0254 / 0.45          # one CMU length unit in metres (the ASF's "length 0.45" inches)


def level_head(m, target_deg, share=0.5):
    """Turn neck and head together about his crosswise axis so the head's mean pitch (forward lean of its bone,
    against our rest) is `target_deg`: CMU's T-pose guess leaves some actors' heads tipped back or down."""
    r = m.rest
    hi, ni = r.i("head"), r.i("neck")
    yaw = yaw_of(m)
    d = m.W[:, hi][:, :, 1]
    fx = d[:, 0] * np.cos(yaw) + d[:, 1] * np.sin(yaw)
    d0 = r.R[hi][:, 1]
    pitch = np.degrees(np.arctan2(fx, d[:, 2]) - math.atan2(d0[0], d0[2]))
    c = math.radians(target_deg - pitch.mean())
    lat = np.stack([-np.sin(yaw), np.cos(yaw), np.zeros_like(yaw)], 1)
    m.W[:, ni] = axis_angle(lat, np.full(len(m), c * share)) @ m.W[:, ni]
    m.W[:, hi] = axis_angle(lat, np.full(len(m), c)) @ m.W[:, hi]
    return target_deg - pitch.mean()


def take_motion(take, rest):
    """The whole take on our rig, its T pose dropped: index i is capture frame i + 1."""
    m = retarget(parse_bvh(bvh_path(take)), rest)
    sc = m.scale
    m = m.slice(1, len(m))
    m.scale = sc
    return m


def steady_span(m, tol=0.12):
    """The frames where he travels at a steady speed (the middle of a walk, not its start and stop)."""
    sp = np.linalg.norm(np.gradient(m.H[:, :2], axis=0), axis=1) * m.fps
    sps = smooth(sp[:, None], int(0.25 * m.fps))[:, 0]
    n = len(sps)
    med = np.median(sps[n // 4: 3 * n // 4])
    ok = np.where(np.abs(sps - med) < tol * med)[0]
    # the longest run of steady frames
    runs, start = [], ok[0]
    for k in range(1, len(ok)):
        if ok[k] != ok[k - 1] + 1:
            runs.append((start, ok[k - 1]))
            start = ok[k]
    runs.append((start, ok[-1]))
    lo, hi = max(runs, key=lambda r_: r_[1] - r_[0])
    return int(lo), int(hi), float(med)


def subtree(rest, bone):
    out = [rest.i(bone)]
    for b, p in enumerate(rest.parent):
        if p in out:
            out.append(b)
    return out


def bend(m, bone, ang, axis=None):
    """Turn `bone` and everything it carries by `ang` radians about a world axis through its head (default his
    crosswise axis, so + leans forward)."""
    r = m.rest
    if axis is None:
        yaw = yaw_of(m)
        axis = np.stack([-np.sin(yaw), np.cos(yaw), np.zeros_like(yaw)], 1)
    R = axis_angle(axis, np.broadcast_to(np.asarray(ang, float), (len(m),)))
    for b in subtree(r, bone):
        m.W[:, b] = R @ m.W[:, b]


def crouch(m, drop, lean):
    """A crouch laid over a motion: the pelvis lower by `drop` metres, the trunk leant forward `lean` degrees
    (spread over the three spine bones, the head kept looking ahead), and the legs bent by IK so every foot keeps
    exactly the path the capture gave it, so the stride and the planted feet are unchanged."""
    r = m.rest
    hd = m.heads()
    ankles = {s: hd[:, r.i("foot_" + s)].copy() for s in ("l", "r")}
    m.H[:, 2] -= drop
    for b in ("spine_01", "spine_02", "chest"):
        bend(m, b, math.radians(lean) / 3)
    bend(m, "neck", -math.radians(lean) * 0.75)
    hd2 = m.heads()
    for s in ("l", "r"):
        two_bone_ik(m, s, ankles[s], np.ones(len(m)), hd2)
        hd2 = m.heads()


def make_loop(spec, rest, verbose=True):
    """A looping clip from spec: take, travel (bool), span (capture frames to search, optional), period (source
    frames, optional), head (target head pitch, degrees, optional), ground percentile, lock (bool)."""
    m = take_motion(spec["take"], rest)
    travel = spec.get("travel", True)
    if spec.get("span"):
        lo, hi = max(0, spec["span"][0] - 1), min(len(m) - 1, spec["span"][1] - 1)
    elif travel:
        lo, hi, _ = steady_span(m)
    else:
        lo, hi = 0, len(m) - 1
    a, b, cost, per = find_cycle(m, lo, hi, period=spec.get("period"), steps=spec.get("steps", 1))
    cyc = m.slice(a, b + 1)
    if spec.get("head") is not None:
        level_head(cyc, spec["head"])
    loop, stride = close_loop(cyc, travel=travel)
    N = len(loop) - 1
    n_out = max(2, int(round(N * FPS / m.fps)))
    out = resample(loop, np.linspace(0, N, n_out + 1))
    if spec.get("floor_loop"):                       # (on hands and knees: the knees stand on the ground)
        hd = out.heads()
        knees = np.minimum(hd[:, rest.i("calf_l"), 2], hd[:, rest.i("calf_r"), 2])
        dz = rest.sole["ground"] - 0.02 + CLEAR["calf_l"] - float(np.percentile(knees, 10))
        out.H[:, 2] += dz
    else:
        dz = ground_fit(out, spec.get("ground", 20), both=spec.get("both_feet", not travel))
    if spec.get("crouch"):
        crouch(out, *spec["crouch"])
    rep = {}
    if spec.get("lock", not spec.get("floor_loop")):
        foot_lock(out, stride, loop=True, report=rep, v_tol=spec.get("v_tol", 0.45), h_tol=spec.get("h_tol", 0.035),
                  glide=6)
    if spec.get("floor_loop"):
        rep["floor_lift_max"] = round(float(np.max(body_clear(out))), 3)
        if spec.get("hand_steps"):
            rep["hand_steps"] = hand_steps(out, stride)
        hands_clear(out)
        out.W[-1], out.H[-1] = out.W[0], out.H[0]
    info = {"take": spec["take"], "a": a + 1, "b": b + 1, "cost": round(float(cost), 4), "period": int(per),
            "stride": round(stride, 3), "frames": n_out + 1, "dz": round(float(dz), 3), "contacts": rep,
            "speed": round(stride / (n_out / FPS), 3) if travel else 0.0}
    if verbose:
        print(spec["name"], info)
    return out, info


def derive(spec, src, verbose=True):
    """A clip cut from another, already made: output frames derive[1]..derive[2] of clip derive[0], maybe played
    backwards and faster. go_prone is get_up backwards, so it ends exactly where get_up starts."""
    m0, info0 = src
    _, a, b = spec["derive"]
    m = m0.slice(a, b + 1)
    if spec.get("reverse"):
        m.W, m.H = m.W[::-1].copy(), m.H[::-1].copy()
    out = resample(m, np.arange(0, len(m) - 1 + 1e-6, spec.get("speed", 1.0)))
    out.W[-1], out.H[-1] = m.W[-1], m.H[-1]                     # (the very last pose, exactly)
    pv = m.rest.i("pelvis")
    fps_src = 4                                                  # capture frames per output frame
    ca, cb = info0["a"] + a * fps_src, info0["a"] + b * fps_src
    info = {"take": info0["take"], "a": ca, "b": cb, "frames": len(out), "dz": info0.get("dz", 0.0),
            "contacts": {}, "end_offset": [round(float(x), 3) for x in out.H[-1] - m.rest.head[pv]],
            "derived": f"{spec['derive'][0]} frames {a + 1}-{b + 1}" + (" reversed" if spec.get("reverse") else "")}
    if verbose:
        print(spec["name"], info)
    return out, info


def make_once(spec, rest, verbose=True):
    """A one-shot clip: capture frames spec["frames"] = (a, b), turned to face +X at its start and set at the origin;
    ground fitted from the standing feet at its start (or end, ground_at="end"); feet locked while planted."""
    m = take_motion(spec["take"], rest)
    a, b = spec["frames"]
    m = m.slice(a - 1, b)
    if spec.get("reverse"):                                      # (played backwards: a get-up becomes a lying-down)
        m.W, m.H = m.W[::-1].copy(), m.H[::-1].copy()
    yaw0 = spec.get("yaw", None)
    if spec.get("anchor"):                                       # (a capture frame to face +X and stand at the origin)
        face_at = spec["anchor"] - a
    else:
        face_at = 0 if spec.get("face_at", "start") == "start" else len(m) - 1
    win = slice(max(0, face_at - 10), face_at + 10)
    ang = -np.angle(np.mean(np.exp(1j * yaw_of(m)[win]))) if yaw0 is None else -math.radians(yaw0)
    turn(m, ang)
    pv = rest.i("pelvis")
    m.H[:, :2] -= m.H[face_at, :2] - rest.head[pv][:2]
    if spec.get("head") is not None:
        level_head(m, spec["head"])
    if spec.get("speeds"):                  # (a speed that changes: (capture frame of the cut, speed) knots)
        kn = np.array(spec["speeds"], float)
        times, t = [], 0.0
        while t < len(m) - 1:
            times.append(t)
            t += m.fps / FPS * float(np.interp(t, kn[:, 0], kn[:, 1]))
        times = np.array(times + [len(m) - 1.0])
    else:
        times = np.arange(0, len(m) - 1 + 1e-6, m.fps / FPS * spec.get("speed", 1.0))
    out = resample(m, times)
    if spec.get("limp"):                    # (the arms part way to hanging slack: (capture frame, how far) knots)
        kn = np.array(spec["limp"], float)
        w = np.interp(times, kn[:, 0], kn[:, 1])
        L = to_local(out)
        for s_ in ("l", "r"):
            for nm in ("upperarm_", "lowerarm_", "hand_"):
                bi = rest.i(nm + s_)
                L[:, bi] = slerp_mats(L[:, bi], np.broadcast_to(np.eye(3), L[:, bi].shape), w)
        out = from_local(rest, L, out.H, FPS)
    if spec.get("jolt"):                    # (the hit: the trunk snaps `deg` forward (+) or back (-) at frame f0,
        f0, deg = spec["jolt"]              # the head lagging a frame, and both settle within about 8 frames)
        t = np.arange(len(out)) - f0
        p1 = np.where(t > 0, t / 2.0 * np.exp(1 - t / 2.0), 0.0)
        p2 = np.where(t > 1, (t - 1) / 2.0 * np.exp(1 - (t - 1) / 2.0), 0.0)
        jd = math.radians(deg)
        bend(out, "spine_02", 0.4 * jd * p1)
        bend(out, "chest", 0.6 * jd * p1)
        bend(out, "neck", -0.6 * jd * p1 + 0.7 * jd * p2)
    k = int(spec.get("ground_frames", 6))
    gsl = slice(0, k) if spec.get("ground_at", "start") == "start" else slice(len(out) - k, None)
    sh = sole_heights(out)
    low = np.minimum(sh["l"][0], sh["r"][0])[gsl]
    dz = rest.sole["ground"] - float(np.median(low)) + spec.get("dz", 0.0)
    out.H[:, 2] += dz
    if spec.get("land"):                    # (the actor fell onto a mat, sank into it and bounced: from frame f1 on,
        f0, f1 = spec["land"]               # his lowest part lies on the ground; from f0 to f1 he is eased down to it)
        hd = out.heads()
        g = rest.sole["ground"] - 0.02
        ex = np.min([hd[:, rest.i(n), 2] - (g + c) for n, c in CLEAR.items()], 0)
        t = np.arange(len(out))
        u = np.clip((t - f0) / max(1, f1 - f0), 0, 1)
        o = np.where(t >= f1, -ex, -ex[f1] * u * u * (3 - 2 * u))
        out.H[:, 2] += smooth(o[:, None], 1)[:, 0]
    rep = {}
    lift = body_clear(out) if spec.get("floor", False) else 0.0
    if spec.get("lock", True):
        foot_lock(out, 0.0, loop=False, report=rep, v_tol=spec.get("v_tol", 0.3), h_tol=spec.get("h_tol", 0.03))
    if spec.get("floor", False):
        hands_clear(out)
    rep["floor_lift_max"] = round(float(np.max(lift)), 3)
    end = out.H[-1] - rest.head[pv]
    info = {"take": spec["take"], "a": a, "b": b, "frames": len(out), "dz": round(dz, 3), "contacts": rep,
            "end_offset": [round(float(x), 3) for x in end]}
    if verbose:
        print(spec["name"], info)
    return out, info


# ------------------------------------------------------------------------------------------------ the rifle
# How the hands hold the rifle, agreed with the soldier artist. The rifle's frame (and the `weapon` bone's, a child
# of hand_r): origin at the right hand's grip, just behind the trigger; +Y along the barrel to the muzzle; +Z up out
# of the rifle's top (the sights); +X to the rifle's right. The right hand holds the grip (the small of the stock);
# the left hand is under the fore-end LEFT_HAND metres further along the barrel and LEFT_DROP below its axis.
LEFT_HAND = 0.35
LEFT_DROP = 0.0          # (the fore-end sits in the left hand's curled fingers; raise to rest it on the palm)
# The right hand's hold, the same for every carry (the weapon bone is fixed to it): round the wrist of the stock,
# the palm on its right side facing left (-x), the hand (wrist to knuckles) forward and down, square to the stock's
# wrist, the fingers curled round under it. (hand's axis wrist to knuckles, palm normal), in the rifle's frame.
GRIP_R = ((0.0, 0.6, -0.8), (-1.0, 0.0, 0.0))
# The carries, as a man standing in the rest pose (facing +x, x forward, y left, z up, metres) holds them, his trunk
# already turned by `twist`: what the rifle's place and angle are in the world for him. Each is then carried by his
# chest as he moves (see carry_rifle).
#   at      where the grip (the right hand's hold, the small of the stock) is
#   barrel  which way the barrel points;  up  which way the rifle's top (its sights) faces
#   left    how far along the barrel from the grip the left hand holds (default LEFT_HAND)
#   twist   how far the trunk turns right (deg; spread over the spine, the neck and head turning back to the front)
#   lean    extra chest lean (deg)
#   ride    the bone whose head carries the rifle's place (its bob and travel): the chest by default
#   follow  how much of that bone's mean lean (a crouched gait's trunk) the carry takes (1 = all of it, the default:
#           the carry keeps its place on the chest; 0 = it keeps its own angle to the ground whatever the lean)
#   steady  how much of the bone's sway about that mean the carry leaves out (0 = it sways with the bone, 1 = it
#           keeps its mean, turning only with his heading)
#   grip_l  the left hand's hold (hand axis wrist to knuckles, palm normal) in the rifle's frame
#   pole_r/pole_l  which way each elbow goes, in the thorax (the chest's frame: x forward, y left, z up)
#   clav_r/clav_l  each clavicle's (elevation, protraction) in degrees, turned in the thorax
# The arms are then solved on their hinges (solve_arm): the elbow placed toward its pole at the arm's lengths, the
# humerus turned so the elbow bends the way it is built to (never backward, never off its hinge), the hand at the
# grip's angle, so every joint stays inside a real body's range (rom.py checks it).
RIFLE_POSES = {
    # port arms: diagonally across the chest a hand's breadth in front of it, muzzle up past the left shoulder,
    # sights toward him; the right hand at the small of the stock just in front of the right hip, the elbow down and
    # back; the left hand round the fore-end at chest height, its forearm about level, the elbow down by the side
    "port": dict(at=(0.28, -0.10, 1.12), barrel=(0.41, 0.49, 0.77), up=(-0.81, -0.19, 0.55), twist=6.0, lean=0.0,
                 steady=0.35, grip_l=((0.40, 0.72, -0.56), (0.81, 0.0, 0.59)),
                 pole_r=(-0.52, -0.36, -0.33), pole_l=(0.0, 0.15, -1.0), clav_r=(0.0, 4.0), clav_l=(2.0, 6.0)),
    # the ready (low): the butt by the right hip, outside the thigh, the muzzle forward and 40 degrees up (about 32
    # in the crouched gaits, which take a quarter of the trunk's lean), turned 20 degrees in across his front; the
    # right hand round the wrist of the stock at the belt, the elbow hanging down and a little back; the left hand
    # under the fore-end from the left, the elbow down and in
    "ready": dict(at=(0.21, -0.185, 1.06), barrel=(0.72, 0.26, 0.643), up=(-0.60, -0.22, 0.766), twist=14.0,
                  lean=3.0, steady=0.6, follow=0.25, grip_l=((0.454, 0.770, -0.442), (0.62, 0.0, 0.78)),
                  pole_r=(-1.0, -0.3, -0.45), pole_l=(-0.35, 0.12, -1.0), clav_r=(0.0, 0.0), clav_l=(2.0, 10.0)),
}


def _frame(y, z):
    """A rotation matrix whose Y axis is y and Z axis (made square to it) is z."""
    y = _norm(np.asarray(y, float))
    z = np.asarray(z, float)
    z = _norm(z - y * np.sum(y * z, -1, keepdims=True))
    x = np.cross(y, z)
    return np.stack([x, y, z], -1)


# human.build's hand, read off MakeHuman's joints (the same for every seed): in the left hand bone's own frame (the
# right's is its mirror, x negated), the palm's normal (out of the palm), the hand's true axis (wrist to the middle
# knuckle: the bone runs to the index knuckle) and the middle knuckle; HAND_LEN is the bone's length they go with.
PALM_N = (0.722, 0.045, -0.691)
HAND_AXIS = (-0.201, 0.969, -0.146)
MID_KNUCKLE = (-0.0228, 0.1103, -0.0166)
HAND_LEN = 0.1184


def hand_grips(rest, grip_l=None, hole=0.034, back=0.004):
    """For each hand: its rotation relative to the rifle's frame (G, so hand = rifle @ G) and the point of the hand
    that holds (in the hand bone's frame: the middle of the fist's hole, `hole` metres out of the palm from the
    middle knuckle and `back` metres toward the wrist). `grip_l` is the left hand's hold (as RIFLE_POSES'); the
    right's is GRIP_R."""
    out = {}
    for s in ("l", "r"):
        h = rest.i("hand_" + s)
        k = np.linalg.norm(rest.tail[h] - rest.head[h]) / HAND_LEN
        mir = np.array([1.0, 1.0, 1.0]) if s == "l" else np.array([-1.0, 1.0, 1.0])
        a, n = _norm(np.array(HAND_AXIS) * mir), np.array(PALM_N) * mir
        n = _norm(n - a * (n @ a))
        grip_local = np.array(MID_KNUCKLE) * mir * k + n * hole - a * back
        ar, nr = GRIP_R if s == "r" else (grip_l or RIFLE_POSES["ready"]["grip_l"])
        ar, nr = _norm(np.asarray(ar, float)), np.asarray(nr, float)
        nr = _norm(nr - ar * (nr @ ar))
        B_r = np.stack([ar, nr, np.cross(ar, nr)], -1)                # rifle frame: hand axis, palm normal, third
        B_h = np.stack([a, n, np.cross(a, n)], -1)                    # the same in the hand bone's frame
        out[s] = (B_r @ B_h.T, grip_local)
    return out


def motion_of_action(armature, action, rest=None):
    """Read an action (rotation quaternion keys on the bones, location on the pelvis) back into a Motion."""
    rest = rest or Rest.of(armature)
    fcs = {}
    for fc in _fcurves(action):
        fcs[(fc.data_path, fc.array_index)] = fc
    f0, f1 = int(round(action.frame_range[0])), int(round(action.frame_range[1]))
    frames = np.arange(f0, f1 + 1)
    F, B = len(frames), len(rest.names)
    L = np.tile(np.eye(3), (F, B, 1, 1))
    for b, n in enumerate(rest.names):
        key = f'pose.bones["{n}"].rotation_quaternion'
        if (key, 0) in fcs:
            q = np.array([[fcs[(key, i)].evaluate(f) for i in range(4)] for f in frames])
            L[:, b] = quat_to_mat(q)
    pv = rest.i("pelvis")
    key = 'pose.bones["pelvis"].location'
    loc = np.array([[fcs[(key, i)].evaluate(f) if (key, i) in fcs else 0.0 for i in range(3)] for f in frames])
    H = rest.head[pv] + loc @ rest.R[pv].T
    m = from_local(rest, L, H, FPS)
    m.frames = frames
    return m


def _fcurves(action):
    try:
        import kit
        return kit.fcurves_of(action)
    except Exception:
        out = []
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    out += list(bag.fcurves)
        return out


def solve_arm(m, side, wrist, pole, hd=None, soft=0.012):
    """Put an arm's wrist on `wrist` (F, 3) on its hinge: the elbow toward `pole` (F, 3, world; only its part square
    to the shoulder-wrist line counts) at the arm's two lengths, then the upper arm and forearm set by aim_limb so the
    rest pose's elbow hinge lies square to the bend (the elbow bends forward on its hinge whatever the target; the
    humerus takes the twist that needs). Soft near full reach, so the elbow never snaps straight. The hand bone is left
    for the caller. Returns how far the wrist falls short (F,)."""
    r = m.rest
    ua, la, ha = r.i("upperarm_" + side), r.i("lowerarm_" + side), r.i("hand_" + side)
    hd = m.heads() if hd is None else hd
    S = hd[:, ua]
    L1 = np.linalg.norm(r.head[la] - r.head[ua])
    L2 = np.linalg.norm(r.head[ha] - r.head[la])
    d = wrist - S
    dist = np.linalg.norm(d, axis=1)
    Ls = L1 + L2
    dist_e = np.where(dist > Ls - soft, Ls - soft * np.exp(-(dist - (Ls - soft)) / soft), dist)
    dist_e = np.maximum(dist_e, abs(L1 - L2) + 0.02)
    dh = _norm(d)
    p = np.broadcast_to(np.asarray(pole, float), d.shape)
    p = _norm(p - dh * np.sum(p * dh, 1)[:, None])
    cos_a = np.clip((L1 ** 2 + dist_e ** 2 - L2 ** 2) / (2 * L1 * dist_e), -1, 1)
    E = S + L1 * (dh * cos_a[:, None] + p * np.sqrt(1 - cos_a ** 2)[:, None])
    Wr = S + dh * dist_e[:, None]
    aim_limb(m, "upperarm_" + side, "lowerarm_" + side, E, Wr)
    return dist - dist_e


def turn_clavicle(m, side, elev, prot):
    """Turn a clavicle (and the arm on it) in the thorax: `elev` degrees up, `prot` degrees forward (arrays (F,) or
    numbers); negative is down / back."""
    r = m.rest
    sg = 1.0 if side == "l" else -1.0
    Dch = m.W[:, r.i("chest")] @ r.R[r.i("chest")].T
    F = len(m)
    R = axis_angle(np.array([0.0, 0.0, 1.0]), -sg * np.radians(np.broadcast_to(prot, (F,)))) @ \
        axis_angle(np.array([1.0, 0.0, 0.0]), sg * np.radians(np.broadcast_to(elev, (F,))))
    G = Dch @ R @ np.swapaxes(Dch, -1, -2)
    for b in subtree(r, "clavicle_" + side):
        m.W[:, b] = G @ m.W[:, b]


def _carry_trunk(m, P):
    """A carry's trunk: turned `twist` degrees right over the three spine bones (the neck and head turning back to
    the front), the chest leant `lean` degrees more (the neck taking it back)."""
    if P.get("twist"):
        for b in ("spine_01", "spine_02", "chest"):
            bend(m, b, -math.radians(P["twist"]) / 3, axis=np.array([0.0, 0.0, 1.0]))
        bend(m, "neck", math.radians(P["twist"]) * 0.55, axis=np.array([0.0, 0.0, 1.0]))
        bend(m, "head", math.radians(P["twist"]) * 0.45, axis=np.array([0.0, 0.0, 1.0]))
    if P.get("lean"):
        bend(m, "chest", math.radians(P["lean"]))
        bend(m, "neck", -math.radians(P["lean"]))


def carry_rifle(m, pose="port", cyclic=False, clav=None):
    """The arms of Motion m put on a rifle carried at `pose` ("port" or "ready", or a dict like RIFLE_POSES'), the
    legs, pelvis and the rest of the capture untouched. The trunk turns for the carry; the rifle's place rides the
    chest (its bob and travel), the carry turning with his heading and taking `follow` of the chest's mean lean and
    (1 - steady) of its sway; the clavicles are set in the thorax; then each arm reaches its grip on its hinge
    (solve_arm, the elbow toward its pole, carried with the rifle) and the hand takes the grip's angle.
    `clav` = ((elev, prot) right, (elev, prot) left) in degrees overrides the pose's.
    Returns (m, rifle_frames (F, 4, 4), hand_error (F, 2))."""
    r = m.rest
    P = RIFLE_POSES[pose] if isinstance(pose, str) else pose
    F = len(m)
    ch = r.i("chest")
    yaw = np.unwrap(yaw_of(m))                                  # his heading: steady over a loop, eased otherwise
    yaw = np.full(F, yaw[:-1].mean()) if cyclic else smooth(yaw, 15)
    # the trunk: a turn to the right (the head turning back), and a little more lean
    _carry_trunk(m, P)
    hd = m.heads()
    # the carry as designed (the rest man, his trunk turned as above), carried by the bone `ride`: its place rides the
    # bone's head; the carry turns with his heading, takes `follow` of the bone's mean lean and (1 - steady) of its
    # sway about that mean. The rifle's place and angle and the elbows' poles all go in that carrying frame K, so the
    # hands keep their hold on the rifle and the arms their set as he leans and sways.
    m0 = Motion(r, r.R[None].copy(), r.head[r.i("pelvis")][None].copy(), m.fps)
    _carry_trunk(m0, P)
    rb = r.i(P.get("ride", "chest"))
    W0, H0 = m0.W[0, rb], m0.heads()[0, rb]
    Fr = _frame(P["barrel"], P["up"])
    Qh = rot_z(-yaw) @ m.W[:, rb] @ W0.T                          # the bone's turn from the design, in his heading
    Q0 = Qh[0]
    Qm = Q0 @ rot_exp(rot_log(Q0.T @ Qh).mean(0))                  # its mean
    sway = rot_exp(rot_log(Qh @ Qm.T) * (1.0 - P.get("steady", 0.0)))
    K = rot_z(yaw) @ sway @ rot_exp(rot_log(Qm) * P.get("follow", 1.0))
    pos = hd[:, rb] + np.einsum("fij,j->fi", K, np.array(P["at"], float) - H0)
    rot = K @ Fr
    # a touch of inertia: the rifle lags its carrier by a frame or two
    k = 2
    if cyclic:
        n = F - 1
        ext = lambda a: np.concatenate([a[n - k:n], a[:n], a[:k + 1]])
        pos = smooth(ext(pos), k)[k:k + F]
        rv = rot_log(np.swapaxes(rot[:1], -1, -2) @ rot)
        rot = rot[:1] @ rot_exp(smooth(ext(rv), k)[k:k + F])
    else:
        pos = smooth(pos, k)
        rv = rot_log(np.swapaxes(rot[:1], -1, -2) @ rot)
        rot = rot[:1] @ rot_exp(smooth(rv, k))
    grips = hand_grips(r, P.get("grip_l"))
    targets = {}
    for s in ("r", "l"):
        G, gl = grips[s]
        Rh = rot @ G
        pt = pos if s == "r" else pos + rot[:, :, 1] * P.get("left", LEFT_HAND) - rot[:, :, 2] * LEFT_DROP
        wrist = pt - np.einsum("fij,j->fi", Rh, gl)
        targets[s] = (Rh, wrist, pt)
    # the clavicles, then the arms on their hinges
    cl = dict(r=P.get("clav_r", (0.0, 0.0)), l=P.get("clav_l", (0.0, 0.0)))
    if clav is not None:
        cl = dict(r=clav[0], l=clav[1])
    for s in ("r", "l"):
        turn_clavicle(m, s, cl[s][0], cl[s][1])
    hd = m.heads()
    err = np.zeros((F, 2))
    for k_, s in enumerate(("r", "l")):
        Rh, wrist, pt = targets[s]
        pole = np.einsum("fij,j->fi", K @ (m0.W[0, ch] @ r.R[ch].T), np.array(P["pole_" + s], float))
        solve_arm(m, s, wrist, pole, hd)
        h = r.i("hand_" + s)
        m.W[:, h] = Rh
        for b in subtree(r, "hand_" + s)[1:]:
            p = r.parent[b]
            m.W[:, b] = m.W[:, p] @ r.R[p].T @ r.R[b]
        hd = m.heads()
        err[:, k_] = np.linalg.norm(hd[:, h] - wrist, axis=1)
    frames = np.tile(np.eye(4), (F, 1, 1))
    # the rifle rides the right hand from here: put its frame where the hand actually got to
    G, gl = grips["r"]
    h = r.i("hand_r")
    frames[:, :3, :3] = m.W[:, h] @ G.T
    frames[:, :3, 3] = hd[:, h] + np.einsum("fij,j->fi", m.W[:, h], gl)
    return m, frames, err


def weapon_bone(armature, rest=None):
    """The `weapon` bone (a child of hand_r, at the grip, +Y along the barrel, +Z up): made if the rig lacks it, from
    the agreed grip, so a rifle parented to it sits in the right hand however the arms move."""
    import bpy
    if "weapon" in armature.data.bones:
        return armature.data.bones["weapon"]
    rest = rest or Rest.of(armature)
    G, gl = hand_grips(rest)["r"]
    h = rest.i("hand_r")
    Rw = rest.R[h] @ G.T
    head = rest.head[h] + rest.R[h] @ gl
    from mathutils import Vector
    bpy.context.view_layer.objects.active = armature
    mode = armature.mode
    bpy.ops.object.mode_set(mode="EDIT")
    eb = armature.data.edit_bones.new("weapon")
    eb.head = Vector(head)
    eb.tail = Vector(head + Rw[:, 1] * 0.25)
    eb.align_roll(Vector(Rw[:, 2]))
    eb.parent = armature.data.edit_bones["hand_r"]
    eb.use_deform = False
    bpy.ops.object.mode_set(mode=mode if mode in ("OBJECT", "POSE") else "OBJECT")
    armature.pose.bones["weapon"].rotation_mode = "QUATERNION"
    return armature.data.bones["weapon"]


def rifle_arms(armature, action, rifle_pose="port", name=None, clav=None):
    """Both hands on a rifle at the port (across the chest) or at the ready (butt by the right hip, muzzle forward
    and up), the action's body moving underneath (carry_rifle). Bakes plain keys into `action` (or into a copy
    called `name`), on the trunk, clavicles, arms and hands; adds the `weapon` bone if the rig lacks it.
    Returns (action, worst hand error in metres)."""
    import bpy
    rest = Rest.of(armature)
    m = motion_of_action(armature, action, rest)
    cyclic = bool(getattr(action, "use_cyclic", False))
    m, frames, err = carry_rifle(m, rifle_pose, cyclic=cyclic, clav=clav)
    weapon_bone(armature, rest)
    Q = m.local_quats()
    keep = {"pelvis", "thigh_l", "thigh_r", "calf_l", "calf_r", "foot_l", "foot_r", "ball_l", "ball_r", "root"}
    quats = {n: Q[:, b] for b, n in enumerate(rest.names) if n not in keep}
    act = action
    if name:
        act = action.copy()
        act.name = name
    _write_quats(armature, act, quats, m.frames)
    return act, float(err.max())


def _write_quats(armature, act, quats, frames):
    """Replace (or make) the rotation keys of the given bones in `act`, one key per frame."""
    fcs = {(fc.data_path, fc.array_index): fc for fc in _fcurves(act)}
    bag = None
    for layer in act.layers:
        for strip in layer.strips:
            for b_ in strip.channelbags:
                bag = b_
    for bone, q in quats.items():
        q = np.array(q, float)
        for k in range(1, len(q)):
            if np.dot(q[k], q[k - 1]) < 0:
                q[k] = -q[k]
        for i in range(4):
            key = (f'pose.bones["{bone}"].rotation_quaternion', i)
            fc = fcs.get(key)
            if fc is None:
                fc = bag.fcurves.new(key[0], index=i)
                fc.group = bag.groups.get(bone) or bag.groups.new(bone)
            fc.keyframe_points.clear()
            fc.keyframe_points.add(len(frames))
            co = np.empty(2 * len(frames))
            co[0::2], co[1::2] = frames, q[:, i]
            fc.keyframe_points.foreach_set("co", co.tolist())
            fc.keyframe_points.foreach_set("interpolation", [1] * len(frames))      # LINEAR
            fc.update()


# ------------------------------------------------------------------------------------------------ the arms in range
# A real arm's limits (degrees; the coordinator's sheet, as rom.py checks them): the forearm's twist from mid-position
# (palm square to the elbow's hinge), the wrist's bend toward the palm and toward the thumb.
# fit_arms eases each a little inside (rom.py reads the bend toward the palm a few degrees larger when the hand is
# also bent far sideways).
ARM_LIMITS = dict(pronation=78.0, supination=83.0, flexion=72.0, extension=62.0, radial=19.0, ulnar=28.0,
                  internal=68.0, external=88.0)


def _soft(v, lo, hi, knee=8.0):
    """v squeezed into [lo, hi], untouched until `knee` degrees from either end, then eased (tanh) up to it."""
    v = np.asarray(v, float)
    a, b = hi - knee, lo + knee
    out = np.where(v > a, a + knee * np.tanh((v - a) / knee), v)
    return np.where(out < b, b - knee * np.tanh((b - out) / knee), out)


def fit_arms(m, seat=(10.0, 25.0), lim=None, knee=8.0, clear=0.004, loop=None):
    """Keep the captured arms inside a real arm's range, the wrists' places unchanged. The capture's hand joints are
    its weakest (markers on the back of a hand, a 3-axis elbow), so after the retarget:
      * each elbow is put back on its hinge: the humerus turned about its own axis until the elbow's hinge lies
        square to the bend the capture shows (the elbow and wrist stay where they are), fully once the elbow is bent
        `seat`[1] degrees, not at all below `seat`[0] (a nearly straight arm shows no bend to read);
      * each hand is eased (soft, `knee` degrees from the limit) into ARM_LIMITS: its twist about the forearm, then
        its bend toward the palm and toward the thumb;
      * each elbow is kept `clear` (m) of the trunk past its own flesh (rom.py's torso), the arm turned out at the
        shoulder where the swing would sink it in (eased over a few frames; `loop` wraps that round a loop);
      * each humerus's own turn (internal / external rotation) is eased into ARM_LIMITS, the arm below the shoulder
        turned about the upper arm (the elbow stays put, the hand swings round).
    Returns {side: (worst degrees taken off the twist, off the bend[, the most the arm was turned out])}."""
    lim = dict(ARM_LIMITS, **(lim or {}))
    loop = getattr(m, "loop", False) if loop is None else loop
    r = m.rest
    F = len(m)
    out = {}
    for s in ("l", "r"):
        M = np.diag((1.0, 1.0 if s == "l" else -1.0, 1.0))
        ua, la, ha = r.i("upperarm_" + s), r.i("lowerarm_" + s), r.i("hand_" + s)
        u0 = M @ _norm(r.head[la] - r.head[ua])
        l0 = M @ _norm(r.head[ha] - r.head[la])
        h0 = M @ _norm(np.cross(u0, l0))                          # the hinge at rest (world, rides the upper arm)
        # 1. the elbow on its hinge
        hd = m.heads()
        E, Wr = hd[:, la], hd[:, ha]
        Du = m.W[:, ua] @ r.R[ua].T
        k = np.einsum("fij,j->fi", Du, h0)
        u = _norm(E - hd[:, ua])
        lo = _norm(Wr - E)
        bend = np.degrees(np.arccos(np.clip(np.sum(u * lo, 1), -1, 1)))
        w = smoothstep((bend - seat[0]) / (seat[1] - seat[0]))
        hb = _norm(np.einsum("ij,fj->fi", M, np.cross(np.einsum("ij,fj->fi", M, u), np.einsum("ij,fj->fi", M, lo))))
        kp = _norm(k - u * np.sum(k * u, 1)[:, None])
        hbp = _norm(hb - u * np.sum(hb * u, 1)[:, None])
        ang = np.arctan2(np.sum(np.cross(kp, hbp) * u, 1), np.sum(kp * hbp, 1)) * w
        G = axis_angle(u, ang)
        Wh = m.W[:, ha].copy()
        for b in subtree(r, "upperarm_" + s):
            m.W[:, b] = G @ m.W[:, b]
        # the forearm back onto the wrist on the turned hinge (the elbow has not moved; only its twist and bend line)
        hd = m.heads()
        Dl = m.W[:, la] @ r.R[la].T
        lnow = _norm(np.einsum("fij,j->fi", Dl, r.head[ha] - r.head[la]))
        Gl = swing(lnow, lo)
        m.W[:, la] = Gl @ m.W[:, la]
        m.W[:, ha] = Wh                                           # (the hand keeps its turn in the world)
        # 2. the hand into its range: read as a twist about the forearm (psi, from mid-position), a bend toward the
        # palm (th) and then toward the thumb (de), each eased into its limits, and the hand rebuilt from them. Frames
        # where the capture's hand has flipped over (bent past anything a wrist does) first take the hand's turn on
        # the forearm from the good frames either side.
        hd = m.heads()
        Du = m.W[:, ua] @ r.R[ua].T
        Sm = lambda v: np.einsum("ij,fj->fi", M, v)               # world <-> this side's terms
        l_ = Sm(_norm(hd[:, ha] - hd[:, la]))
        k_ = Sm(np.einsum("fij,j->fi", Du, h0))
        kp = _norm(k_ - l_ * np.sum(k_ * l_, 1)[:, None])
        mir = np.array([1.0, 1.0, 1.0]) if s == "l" else np.array([-1.0, 1.0, 1.0])
        a0 = r.R[ha] @ _norm(np.array(HAND_AXIS) * mir)
        n0 = r.R[ha] @ (np.array(PALM_N) * mir)
        n0 = _norm(n0 - a0 * (n0 @ a0))

        def read():
            Dh = m.W[:, ha] @ r.R[ha].T
            a_ = Sm(np.einsum("fij,j->fi", Dh, a0))
            n_ = Sm(np.einsum("fij,j->fi", Dh, n0))
            th = np.arcsin(np.clip(-np.sum(n_ * l_, 1), -1, 1))
            npsi = _norm(n_ + np.sin(th)[:, None] * l_)
            psi = np.unwrap(np.arctan2(np.sum(np.cross(kp, npsi) * l_, 1), np.sum(kp * npsi, 1)))
            psi -= 2 * np.pi * np.round(np.median(psi) / (2 * np.pi))        # (the turn nearest mid-position)
            a1 = np.cos(th)[:, None] * l_ + np.sin(th)[:, None] * npsi
            de = np.arctan2(np.sum(a_ * np.cross(npsi, l_), 1), np.sum(a_ * a1, 1))
            return np.degrees(psi), np.degrees(th), np.degrees(de)
        psi, th, de = read()
        bad = (np.abs(de) > 75.0) | (np.abs(th) > 85.0) | (np.abs(psi) > 150.0)
        if bad.any() and not bad.all():
            Rel = np.swapaxes(m.W[:, la], -1, -2) @ m.W[:, ha]
            good = np.where(~bad)[0]
            for f in np.where(bad)[0]:
                i0, i1 = good[good < f], good[good > f]
                if len(i0) and len(i1):
                    p0, p1 = i0[-1], i1[0]
                    Rel[f] = slerp_mats(Rel[p0], Rel[p1], (f - p0) / (p1 - p0))
                else:
                    Rel[f] = Rel[i0[-1] if len(i0) else i1[0]]
            m.W[:, ha] = m.W[:, la] @ Rel
            psi, th, de = read()
        psi2 = _soft(psi, -lim["supination"], lim["pronation"], knee)
        th2 = _soft(th, -lim["extension"], lim["flexion"], knee)
        de2 = _soft(de, -lim["ulnar"], lim["radial"], knee)
        P2, T2, E2 = np.radians(psi2), np.radians(th2), np.radians(de2)
        nq = np.einsum("fij,fj->fi", axis_angle(l_, P2), kp)
        tq = np.cross(nq, l_)
        aq = np.cos(T2)[:, None] * l_ + np.sin(T2)[:, None] * nq
        n3 = -np.sin(T2)[:, None] * l_ + np.cos(T2)[:, None] * nq
        a3 = np.cos(E2)[:, None] * aq + np.sin(E2)[:, None] * tq
        m.W[:, ha] = hand_frame(Sm(a3), Sm(n3), s)
        for b in subtree(r, "hand_" + s)[1:]:
            p = r.parent[b]
            m.W[:, b] = m.W[:, p] @ r.R[p].T @ r.R[b]
        out[s] = (round(float(np.abs(psi2 - psi).max()), 1),
                  round(float(np.hypot(th2 - th, de2 - de).max()), 1))
        # 3. the elbow clear of the body: where the swinging arm's elbow would sink into the trunk (rom.py's slabs of
        # the torso, the elbow's own flesh round it), the whole arm is turned out at the shoulder just enough, eased
        # in and out over a few frames
        import rom
        body = rom.Body.of_rest(r)
        ch = r.i("chest")
        side_y = np.array((0.0, 1.0 if s == "l" else -1.0, 0.0))
        take = np.zeros(F)
        for _ in range(3):
            hd = m.heads()
            pen, _b = rom._elbow_place(body, rom.Poses.of_motion(m), hd[:, la])
            need = np.nan_to_num(pen, nan=-9.0) + clear * 100.0           # cm still to go
            if need.max() <= 0.05:
                break
            arm = np.linalg.norm(hd[:, la] - hd[:, ua], axis=1)
            ang = np.maximum(need, 0.0) / 100.0 / arm
            k = 4
            if loop:                                                       # (a loop's last frame is its first)
                pad = np.concatenate([ang[F - 1 - k:F - 1], ang, ang[1:k + 1]])
            else:
                pad = np.concatenate([ang[:1].repeat(k), ang, ang[-1:].repeat(k)])
            ang = np.max(np.stack([pad[i:i + F] for i in range(2 * k + 1)]), 0)     # held a little either side
            ang = cyc_smooth(ang, 3) if loop else smooth(ang[:, None], 3)[:, 0]
            ang = np.maximum(ang, 0.0) * 1.15
            o = np.einsum("fij,j->fi", m.W[:, ch] @ r.R[ch].T, side_y)            # the trunk's side, outward
            u = _norm(hd[:, la] - hd[:, ua])
            G = axis_angle(_norm(np.cross(u, o)), ang)
            for b in subtree(r, "upperarm_" + s):
                m.W[:, b] = G @ m.W[:, b]
            take += np.degrees(ang)
        if take.any():
            out[s] = out[s] + (round(float(take.max()), 1),)
        # 4. the humerus's own turn (internal / external rotation, as rom.py reads it) eased into its range: the whole
        # arm below the shoulder turned about the upper arm (the elbow stays where it is, the forearm swings round)
        goal = None
        for _ in range(3):
            rot = rom.measure(body, rom.Poses.of_motion(m))["sh_rot_" + s]
            goal = _soft(rot, -lim["external"], lim["internal"], knee) if goal is None else goal
            d = goal - rot
            if np.abs(d).max() < 0.2:
                break
            hd = m.heads()
            u = _norm(hd[:, la] - hd[:, ua])
            G = axis_angle(u, np.radians(d) * (1.0 if s == "l" else -1.0))
            for b in subtree(r, "upperarm_" + s):
                m.W[:, b] = G @ m.W[:, b]
    return out


EASE_BONES = ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
              "ball_l", "ball_r")


def ease_spikes(m, bones=EASE_BONES, limit=850.0, loop=None, rounds=8):
    """Ease each bone's turn on its parent wherever it would turn faster than `limit` deg/s from one frame to the
    next (rom.py flags 900): the capture's hands and elbows jerk at an impact, where markers are lost, and a toe
    laid flat on the ground at a contact snaps there in one frame. The bone's local turn is averaged with its
    neighbours over a few frames round each such step, as many rounds as it takes; its children keep their own
    turns. Returns {bone: fastest deg/s left} for the bones it eased."""
    loop = getattr(m, "loop", False) if loop is None else loop
    r = m.rest
    L = to_local(m)
    F = len(m)
    out = {}
    for n in bones:
        b = r.i(n)
        eased = False
        for _ in range(rounds):
            Q = L[:, b]
            dR = np.swapaxes(Q[:-1], -1, -2) @ Q[1:]
            sp = np.degrees(np.arccos(np.clip((np.trace(dR, axis1=1, axis2=2) - 1) / 2, -1, 1))) * m.fps
            fast = np.where(sp > limit)[0]
            if not len(fast):
                break
            eased = True
            q = mat_to_quat(Q)
            for f in range(1, F):
                if np.dot(q[f], q[f - 1]) < 0:
                    q[f] = -q[f]
            qs = cyc_smooth(q, 1) if loop else smooth(q, 1)
            w = np.zeros(F)
            for f in fast:
                w[max(0, f - 1):f + 3] = 1.0
            w = (cyc_smooth(w, 1) if loop else smooth(w[:, None], 1)[:, 0])
            w = np.clip(w * 1.5, 0.0, 1.0)
            q = q + (qs - q) * w[:, None]
            q /= np.linalg.norm(q, axis=1)[:, None]
            L[:, b] = quat_to_mat(q)
            if loop:
                L[-1, b] = L[0, b]
        if eased:
            Q = L[:, b]
            dR = np.swapaxes(Q[:-1], -1, -2) @ Q[1:]
            out[n] = round(float(np.degrees(np.arccos(np.clip((np.trace(dR, axis1=1, axis2=2) - 1) / 2, -1, 1))).max()
                                 * m.fps), 0)
    if out:
        m.W[:] = from_local(r, L, m.H, m.fps).W
    return out


# ------------------------------------------------------------------------------------------------ the low crawl
# CMU has no belly crawl, so `crawl` is keyed by hand on the rig: chest and hips on the ground, the head low and
# looking ahead, the rifle lying across in front of the face on the forearms (the right hand on the wrist of the
# stock, the fore-end over the left forearm), and one elbow with the same side's knee drawn up together, side by
# side in turn. Each elbow and knee is planted while it pulls or pushes, so it stays put on the ground as the game
# carries the man forward at `stride` per loop.
CRAWL = dict(
    frames=54, stride=0.40,             # one loop (both sides) and the ground it covers
    draw=0.2, pull=0.3, glide=0.15,     # parts of a loop: a side drawing up, then pulling; the body's speed meanwhile
    pelvis_z=0.125,                     # the pelvis head's height, lying on the belly
    neck_up=42.0, head_up=18.0,         # the head raised to look ahead, low (the neck's 60 degrees of extension)
    el_fwd=0.11, el_out=0.26, el_z=0.045,        # an elbow planted this far ahead of and out from its shoulder
    kn_fwd=-0.235, kn_out=0.32, kn_z=0.055,      # a knee drawn up: this far ahead of (- behind) and out from its hip
    kn_back=0.40, kn_in=0.14,                    # the other leg, straight: its knee this far behind and out
    kn_early=0.1, kn_ext=0.22,                   # the knee starts up before its elbow; the leg straightens in this
    pf_frog=40.0, pf_straight=48.0,              # feet pointed (degrees) on the drawn-up and the straight leg
    fa_left=25.0, fa_right=42.0,                 # each forearm's angle in from straight ahead
    fa_rise_l=0.05, fa_rise_r=0.11,              # and its rise toward the hand
    psi=5.0, psi_lo=-8.0, psi_hi=25.0,           # the rifle's muzzle this far forward of pointing straight left
    clav_fwd=(3.0, 4.0), clav_roll=0.0,          # the shoulders drawn toward the head (degrees; + more on the reach)
    shin_up=0.3,                                 # the drawn-up leg's shin: its rise off the ground going back
)
# (rom.py holds it in a real body's range: the drawn-up hip at most 45 degrees out, the clavicles within 30 up and
# 20 back, the neck 60 back, the feet 50 pointed; the knee draw is the frog of a real low crawl, the leg on its inner
# side, so the hip's external rotation, about 70, is past the common 50.)


def smoothstep(u):
    u = np.clip(u, 0.0, 1.0)
    return u * u * (3 - 2 * u)


def cyc_smooth(x, k):
    """smooth() round a loop whose last frame equals its first."""
    n = len(x) - 1
    ext = np.concatenate([x[n - k:n], x[:n], x[:k + 1]])
    return smooth(ext[:, None] if ext.ndim == 1 else ext, k)[k:k + n + 1].reshape(x.shape)


def aim_limb(m, upper, lower, P1, P2, h0=None):
    """Put two bones exactly on the points given (F, 3): `upper` from its head to the joint P1, `lower` from P1 to
    P2 (the points must sit at the bones' lengths). The turn that takes each rest bone onto its target also takes
    the rest pose's hinge (h0, by default square to the two rest bones) onto the target's bend, so the joint bends
    the way it was built to. Returns the two turns and the target hinge (F, 3)."""
    r = m.rest
    a, b = r.i(upper), r.i(lower)
    P0 = m.heads()[:, a]
    end = [k for k, p in enumerate(r.parent) if p == b and not r.names[k].startswith(("thumb", "fingers"))][0]
    u0, l0 = _norm(r.head[b] - r.head[a]), _norm(r.head[end] - r.head[b])
    h0 = _norm(np.cross(u0, l0)) if h0 is None else _norm(np.asarray(h0, float))
    u1, l1 = _norm(P1 - P0), _norm(P2 - P1)
    h1 = np.cross(u1, l1)
    h1 = _norm(np.where((np.linalg.norm(h1, axis=1) < 1e-3)[:, None], h0, h1))

    def frame(d, h):
        h = _norm(h - d * np.sum(d * h, -1, keepdims=True))
        return np.stack(np.broadcast_arrays(d, h, np.cross(d, h)), -1)
    Ru = frame(u1, h1) @ np.swapaxes(frame(u0, h0), -1, -2)
    Rl = frame(l1, h1) @ np.swapaxes(frame(l0, h0), -1, -2)
    m.W[:, a] = Ru @ r.R[a]
    m.W[:, b] = Rl @ r.R[b]
    return Ru, Rl, h1


def hand_frame(axis, palm, side):
    """The hand bone's rotation for a hand whose true axis (wrist to middle knuckle) and palm face the given
    world ways (from human.build's measured hand, PALM_N and HAND_AXIS)."""
    mir = np.array([1.0, 1.0, 1.0]) if side == "l" else np.array([-1.0, 1.0, 1.0])
    a, n = _norm(np.array(HAND_AXIS) * mir), np.array(PALM_N) * mir
    n = _norm(n - a * (n @ a))
    axis = _norm(np.asarray(axis, float))
    palm = np.asarray(palm, float)
    palm = _norm(palm - axis * np.sum(palm * axis, -1, keepdims=True))
    return np.stack([axis, palm, np.cross(axis, palm)], -1) @ np.stack([a, n, np.cross(a, n)], -1).T


def _cradle_frame(psi, th):
    """The rifle lying on its left side across in front of him: barrel `psi` forward of straight left, rising `th`,
    its sights facing back toward him."""
    Y = np.stack([np.sin(psi) * np.cos(th), np.cos(psi) * np.cos(th), np.sin(th)], 1)
    Z = np.stack([-np.cos(psi), np.sin(psi), np.zeros_like(psi)], 1)
    return _frame(Y, Z)


def belly_crawl(rest, P=None):
    """The low crawl (a loop, in place), keyed on our rig from CRAWL. Returns (Motion, info)."""
    P = dict(CRAWL, **(P or {}))
    r = rest
    N, stride, draw, pull = P["frames"], P["stride"], P["draw"], P["pull"]
    F = N + 1
    u = np.arange(F) / N
    X_, Y_, Z_ = np.eye(3)
    # the body over the ground: a surge on each pull, a slow glide while a side draws up; in place, what is left
    fine = np.linspace(0, 1, 2001)
    v = np.full_like(fine, P["glide"])
    for o in (0.0, 0.5):
        ph = (fine - o) % 1.0
        w = (ph >= draw) & (ph < draw + pull)
        v[w] += ((0.5 * stride - P["glide"] * draw) / pull - P["glide"]) * (1 - np.cos(2 * np.pi * (ph[w] - draw) / pull))
    v *= stride / np.trapz(v, fine)
    Xg = np.concatenate([[0], np.cumsum(0.5 * (v[1:] + v[:-1]) * np.diff(fine))])
    xp = np.interp(u, fine, Xg) - stride * u
    xp -= xp[:-1].mean()
    xp_of = lambda uu: np.interp(uu % 1.0, u, xp)
    # the prone man: our rest turned 90 degrees forward (face down, head to +X), the root left as it is
    W = np.einsum("ij,bjk->bik", axis_angle(Y_, math.pi / 2), r.R)[None].repeat(F, 0)
    W[:, r.i("root")] = r.R[r.i("root")]
    pv = r.i("pelvis")
    s = np.cos(2 * np.pi * (u - draw))             # +1: the left side drawn up; -1: the right
    H = np.zeros((F, 3))
    H[:, 0] = r.head[pv][0] + xp
    H[:, 2] = P["pelvis_z"] + 0.006 * np.abs(s)
    m = Motion(r, W, H, FPS)
    # the trunk: the pelvis turns and rolls toward the side drawing up, the spine bends that way (concave), a little
    # arch lifts the shoulders, the neck and head come up to look ahead
    bend(m, "pelvis", -math.radians(7) * s, axis=Z_)
    bend(m, "pelvis", math.radians(6) * s, axis=X_)
    for b_, e in (("spine_01", 2.0), ("spine_02", 3.0), ("chest", 2.0)):
        bend(m, b_, -math.radians(e), axis=Y_)
        bend(m, b_, math.radians(14 / 3) * s, axis=Z_)
    bend(m, "spine_02", -math.radians(9) * s, axis=X_)
    bend(m, "neck", -math.radians(P["neck_up"]), axis=Y_)
    bend(m, "neck", -math.radians(6) * s, axis=Z_)
    bend(m, "head", -math.radians(P["head_up"]), axis=Y_)
    bend(m, "head", -math.radians(4) * s, axis=Z_)
    # the shoulders drawn forward for the reach, the reaching side more
    for side, sg in (("l", 1.0), ("r", -1.0)):
        reach = 0.5 + 0.5 * sg * s
        bend(m, "clavicle_" + side, -sg * np.radians(P["clav_fwd"][0] + P["clav_fwd"][1] * reach), axis=Z_)
        bend(m, "clavicle_" + side, sg * math.radians(P["clav_roll"]), axis=X_)
    hd = m.heads()
    ln = lambda a, b: float(np.linalg.norm(r.head[r.i(b)] - r.head[r.i(a)]))
    L = dict(ua=ln("upperarm_l", "lowerarm_l"), fa=ln("lowerarm_l", "hand_l"), th=ln("thigh_l", "calf_l"),
             sh=ln("calf_l", "foot_l"))
    # where each elbow and knee goes (phi: 0 when that side starts to draw up)
    plan = {}
    for side, sg, off in (("l", 1.0, 0.0), ("r", -1.0, 0.5)):
        phi = (u - off) % 1.0
        phi[-1] = phi[0]
        shx, shy = hd[:, r.i("upperarm_" + side), 0].mean(), hd[:, r.i("upperarm_" + side), 1].mean()
        # the elbow: drawn up and forward, planted for half a loop (its own pull, then the other side's draw), then
        # dragged along with the body while the other side pulls
        x_plant = shx + P["el_fwd"]
        pl_end = x_plant - stride * 0.5
        x, z = np.empty(F), np.full(F, P["el_z"])
        a_ = (phi >= draw) & (phi < draw + 0.5)
        x[a_] = x_plant - stride * (phi[a_] - draw)
        b_ = phi >= draw + 0.5
        x[b_] = pl_end + xp_of(phi[b_] + off) - xp_of(draw + 0.5 + off)
        x_back = pl_end + xp_of(1.0 + off) - xp_of(draw + 0.5 + off)
        c_ = phi < draw
        x[c_] = x_back + (x_plant - x_back) * smoothstep(phi[c_] / draw)
        z[c_] += 0.03 * np.sin(np.pi * phi[c_] / draw)
        E = np.stack([x, np.full(F, shy + sg * P["el_out"]), z], 1)
        # the knee: planted through its push, swung back as the leg straightens and dragged straight, then drawn up
        # in an arc round the hip to plant again
        hip = hd[:, r.i("thigh_" + side)]
        hx, hy = hip[:, 0].mean(), hip[:, 1].mean()
        k_plant = np.array([hx + P["kn_fwd"], hy + sg * P["kn_out"]])
        rel_end = np.array([P["kn_fwd"] - stride * pull, sg * P["kn_out"]])
        rel_ext = np.array([-P["kn_back"], sg * P["kn_in"]])

        def arc(r0, r1, t):
            a0, a1 = math.atan2(sg * r0[1], r0[0]), math.atan2(sg * r1[1], r1[0])
            d0, d1 = np.hypot(*r0), np.hypot(*r1)
            return np.cos(a0 + (a1 - a0) * t) * (d0 + (d1 - d0) * t), sg * np.sin(a0 + (a1 - a0) * t) * (d0 + (d1 - d0) * t)
        Kx, Ky, Kz = np.empty(F), np.empty(F), np.full(F, P["kn_z"])
        pa = (phi >= draw) & (phi < draw + pull)
        Kx[pa], Ky[pa] = k_plant[0] - stride * (phi[pa] - draw), k_plant[1]
        pb = (phi >= draw + pull) & (phi < 1 - P["kn_early"])
        rx, ry = arc(rel_end, rel_ext, smoothstep((phi[pb] - draw - pull) / P["kn_ext"]))
        Kx[pb], Ky[pb] = hip[pb, 0] + rx, hip[pb, 1] + ry
        pc = (phi < draw) | (phi >= 1 - P["kn_early"])
        kph = (np.where(phi[pc] >= 1 - P["kn_early"], phi[pc] - 1, phi[pc]) + P["kn_early"]) / (draw + P["kn_early"])
        rx, ry = arc(rel_ext, k_plant - np.array([hx, hy]), smoothstep(kph))
        Kx[pc], Ky[pc] = hip[pc, 0] + rx, hip[pc, 1] + ry
        Kz[pc] += 0.015 * np.sin(np.pi * kph)
        K = np.stack([Kx, Ky, Kz], 1)
        ang = np.degrees(np.arctan2(sg * (K[:, 1] - hip[:, 1]), K[:, 0] - hip[:, 0]))
        plan[side] = dict(E=E, K=K, straight=np.clip((ang - 80) / (155 - 80), 0, 1))
    # the legs: the knee as planned (a thigh's length from the hip), the shin back along the ground on its inner
    # side when drawn up, back and a little up when straight (the toes on the ground), the foot pointed
    for side, sg in (("l", 1.0), ("r", -1.0)):
        p = plan[side]
        hp = m.heads()[:, r.i("thigh_" + side)]
        K = hp + L["th"] * _norm(p["K"] - hp)
        st = p["straight"][:, None]
        A = K + L["sh"] * _norm(np.array([-0.95, -sg * 0.22, P["shin_up"]]) * (1 - st) + np.array([-0.99, -sg * 0.03, 0.10]) * st)
        _, Rl, h1 = aim_limb(m, "thigh_" + side, "calf_" + side, K, A, h0=-r.R[r.i("thigh_" + side)][:, 0])
        pf = np.radians(P["pf_frog"] + (P["pf_straight"] - P["pf_frog"]) * p["straight"])
        fo, ba = r.i("foot_" + side), r.i("ball_" + side)
        m.W[:, fo] = axis_angle(h1, pf) @ Rl @ r.R[fo]
        m.W[:, ba] = m.W[:, fo] @ r.R[fo].T @ r.R[ba]
    # the arms: a planted elbow stays put, the clavicle giving so the shoulder keeps an upper arm's length from it;
    # each forearm lies on the ground, in and forward
    fore = lambda a, sg, rise: _norm(np.array([math.cos(math.radians(a)), -sg * math.sin(math.radians(a)), rise]))
    Es, Wr_, clav = {}, {}, {}
    for side, sg, al, rise in (("l", 1.0, P["fa_left"], P["fa_rise_l"]), ("r", -1.0, P["fa_right"], P["fa_rise_r"])):
        hd = m.heads()
        E = plan[side]["E"]
        C, S = hd[:, r.i("clavicle_" + side)], hd[:, r.i("upperarm_" + side)]
        cl = np.linalg.norm(S - C, axis=1)
        nrm, d = _norm(E - C), np.linalg.norm(E - C, axis=1)
        a = (cl ** 2 - L["ua"] ** 2 + d ** 2) / (2 * d)
        cen = C + nrm * a[:, None]
        q = S - cen
        q -= nrm * np.sum(q * nrm, 1)[:, None]
        G = swing(S - C, cen + _norm(q) * np.sqrt(np.maximum(cl ** 2 - a ** 2, 0.0))[:, None] - C)
        clav[side] = float(np.degrees(np.linalg.norm(rot_log(G), axis=1)).max())
        for b in subtree(r, "clavicle_" + side):
            m.W[:, b] = G @ m.W[:, b]
        S = m.heads()[:, r.i("upperarm_" + side)]
        Es[side] = S + L["ua"] * _norm(E - S)
        Wr_[side] = Es[side] + L["fa"] * fore(al, sg, rise)
    # the rifle: in the right hand (the agreed grip, so the weapon bone carries it), lying across with its fore-end
    # over the left forearm, the muzzle kept as near `psi` as that allows
    G_r, gl_r = hand_grips(r)["r"]
    El, Wl = Es["l"], Wr_["l"]
    lam = np.linspace(-0.1, 1.35, 59)
    psi0 = math.radians(P["psi"])
    psi = np.full(F, psi0)
    for _ in range(3):
        Rh = _cradle_frame(psi, np.zeros(F)) @ G_r
        o = Wr_["r"] + np.einsum("fij,j->fi", Rh, gl_r)
        T = El[:, None, :] + lam[None, :, None] * (Wl - El)[:, None, :]
        ang = np.arctan2(T[..., 0] - o[:, None, 0], T[..., 1] - o[:, None, 1])
        k = np.argmin(np.abs(ang - psi0), 1)
        psi = cyc_smooth(np.clip(ang[np.arange(F), k], math.radians(P["psi_lo"]), math.radians(P["psi_hi"])), 3)
    Tk = T[np.arange(F), k]
    th = cyc_smooth(np.arctan2(Tk[:, 2] + 0.055 - o[:, 2], np.hypot(*(Tk[:, :2] - o[:, :2]).T)), 3)
    Rh = _cradle_frame(psi, th) @ G_r
    for side in ("l", "r"):
        aim_limb(m, "upperarm_" + side, "lowerarm_" + side, Es[side], Wr_[side])
        h = r.i("hand_" + side)
        m.W[:, h] = Rh if side == "r" else hand_frame(fore(P["fa_left"] - 10, 1.0, -0.15), (0.0, 0.0, -1.0), "l")
        for b in subtree(r, "hand_" + side)[1:]:
            m.W[:, b] = m.W[:, r.parent[b]] @ r.R[r.parent[b]].T @ r.R[b]
    m.W[-1], m.H[-1] = m.W[0], m.H[0]
    info = {"take": None, "a": None, "b": None, "stride": stride, "frames": F, "speed": round(stride / (N / FPS), 3),
            "contacts": {"clavicle_give_max_deg": {k_: round(v_, 1) for k_, v_ in clav.items()}},
            "rifle_psi_deg": [round(float(np.degrees(psi.min())), 1), round(float(np.degrees(psi.max())), 1)]}
    return m, info


def make_keyed(spec, rest, verbose=True):
    m, info = {"belly_crawl": belly_crawl}[spec["make"]](rest, spec.get("params"))
    if verbose:
        print(spec["name"], {k: v for k, v in info.items()})
    return m, info


# ------------------------------------------------------------------------------------------------ previews
PREVIEW_DIR = "/mnt/project-files/battle-models/land/soldier/mocap/"


def preview_man(seed=0):
    """A plain matt-grey man (human.build, as the soldier is built), smooth and subdivided, for the previews."""
    import bpy
    import human
    import kit
    man = human.build(seed=seed, nation="GER", stand=0.02, curl=(30, 45, 25))
    grey = kit.material("matt_grey", kit.hex_rgb("A3A29E"), rough=0.8, spec=0.3)
    for ob in [man["body"]] + man["eyes"]:
        ob.data.materials.clear()
        ob.data.materials.append(grey)
    man["tights"].hide_render = man["skirt"].hide_render = True
    man["tights"].hide_viewport = man["skirt"].hide_viewport = True
    sub = man["body"].modifiers.new("smooth", "SUBSURF")
    sub.levels, sub.render_levels = 0, 1
    return man


def render_clip(man, clip, c, out_base, work, step=2, loops=1, cam_yaw=35.0, dist=4.8, threads=2, samples=16):
    """Render one clip of clips.json (already applied as an action) to out_base.mp4/.gif. A looping travel clip is
    played as the game plays it: the man carried forward at stride per loop, the camera going with him over the
    field, so any foot sliding shows against the ground."""
    import bpy
    import kit
    sc = bpy.context.scene
    arm = man["armature"]
    n = c["frames"]
    total = (n - 1) * loops + 1 if c.get("loop") else n
    speed = c.get("stride", 0.0) / ((n - 1) / FPS) if c.get("loop") else 0.0
    cam = sc.camera
    os.makedirs(work, exist_ok=True)
    for f in os.listdir(work):
        if f.endswith(".png"):
            os.remove(os.path.join(work, f))
    k = 1
    tgt_z = c.get("cam_z", 0.86)
    for t in range(0, total, step):
        sc.frame_set(1 + (t % (n - 1) if c.get("loop") else t))
        x = speed * t / FPS
        arm.location = (x, 0, 0)
        kit.orbit(cam, (x + c.get("cam_dx", 0.0), 0, tgt_z), c.get("cam_dist", dist), c.get("cam_pitch", 5.0), cam_yaw)
        sc.render.filepath = os.path.join(work, f"{k:04d}.png")
        bpy.ops.render.render(write_still=True)
        k += 1
    arm.location = (0, 0, 0)
    kit.encode(work, out_base, fps=FPS // step)
    return out_base


def rifle_proxy(armature):
    """A plain rifle-sized stand-in (1.11 m, boxes) on the `weapon` bone, for previews of rifle_arms."""
    import bpy
    import bmesh
    import kit
    me = bpy.data.meshes.new("rifle_proxy")
    bm = bmesh.new()
    for y0, y1, w, h, zc in ((-0.33, -0.05, 0.040, 0.10, -0.03),      # butt
                             (-0.05, 0.05, 0.030, 0.040, -0.005),     # the wrist of the stock, where the right hand is
                             (0.05, 0.20, 0.040, 0.060, 0.0),         # receiver and magazine
                             (0.20, 0.55, 0.040, 0.045, -0.004),      # fore-end, where the left hand is
                             (0.55, 0.78, 0.018, 0.018, 0.012)):      # barrel
        r = bmesh.ops.create_cube(bm, size=1.0)
        for v in r["verts"]:
            v.co.x *= w
            v.co.y = (y0 + y1) / 2 + v.co.y * (y1 - y0)
            v.co.z = zc + v.co.z * h
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("rifle_proxy", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(kit.material("proxy_wood", kit.hex_rgb("6B4A2B"), rough=0.6))
    ob.parent = armature
    ob.parent_type = "BONE"
    ob.parent_bone = "weapon"
    ob.location = (0, -armature.data.bones["weapon"].length, 0)         # (a bone child sits at the bone's tail)
    return ob


def preview(names=None, out_dir=PREVIEW_DIR, work_dir="/home/user/art/mocap/frames", clips=None, step=2):
    """Preview films for the named clips (all by default). A name "clip+pose" (say "walk+port") previews
    rifle_arms on that clip, with a stand-in rifle, as clip_pose."""
    import bpy
    import kit
    clips = clips or load_clips()
    names = names or [n for n in clips if not n.startswith("_")]
    kit.fresh(FPS)
    kit.studio(sun_elev=40, sun_turn=-40)
    kit.field_ground()
    man = preview_man()
    arm = man["armature"]
    kit.camera(lens=42)
    sc = kit.render_setup(640, 360, samples=16)
    sc.render.threads_mode = "FIXED"
    sc.render.threads = 2
    rifle = None
    for nm in names:
        clip, _, pose = nm.partition("+")
        c = clips[clip]
        act = apply(arm, clip, clips)
        out = clip
        if pose:
            act, err = rifle_arms(arm, act, pose, name=f"{clip}_{pose}")
            arm.animation_data.action = act
            arm.animation_data.action_slot = act.slots[0]
            out = f"{clip}_{pose}"
            rifle = rifle or rifle_proxy(arm)
            print(nm, "worst hand miss", round(err, 4), flush=True)
        elif c.get("rifle"):                # (a clip that carries the rifle in its own arms, as the low crawl)
            weapon_bone(arm)
            rifle = rifle or rifle_proxy(arm)
        if rifle:
            rifle.hide_render = not (pose or c.get("rifle"))
        loops = 2 if c.get("loop") and c["frames"] < 60 else 1
        render_clip(man, out, c, os.path.join(out_dir, out), os.path.join(work_dir, out), step=step, loops=loops)
        print("preview", out, flush=True)
    return names


def sheet(names, out_dir=PREVIEW_DIR, work_dir="/home/user/art/mocap/frames", clips=None):
    """clips.png: one telling frame of each clip (the middle of a loop, the end of a one-shot's main action)."""
    import kit
    clips = clips or load_clips()
    ims, labels = [], []
    for nm in names:
        fr = sorted(f for f in os.listdir(os.path.join(work_dir, nm)) if f.endswith(".png"))
        pick = clips.get(nm, {}).get("sheet_at", 0.5)
        ims.append(os.path.join(work_dir, nm, fr[min(len(fr) - 1, int(pick * (len(fr) - 1)))]))
        labels.append(nm)
    return kit.sheet(ims, os.path.join(out_dir, "clips.png"), cols=4, labels=labels,
                     title="The soldier's motion capture (CMU data, retargeted)")


# The game's clips: which CMU take each comes from and how it is cut. `frames` are capture frames at 120 fps,
# counted from 1 after the BVH's added T pose; a loop's own frames are chosen by find_cycle within `span`.
CLIPS = [
    dict(name="walk", take="35_01", loop=True, head=4,
         note="Subject 35, plain walk. One full two-step cycle."),
    dict(name="run", take="35_18", loop=True, head=0, ground=10,
         note="Subject 35, run/jog: the same man as the walk."),
    dict(name="idle", take="77_02", loop=True, travel=False, period=480, head=6,
         note="Subject 77 (careful actions), standing: weight shifts, a glance about."),
    dict(name="march", take="21_06", loop=True, head=2,
         note="Subject 21, 'soldiers march': a parade march with straight arms swung to the shoulder."),
    dict(name="advance", take="35_18", loop=True, head=None, ground=10, crouch=(0.14, 28),
         note="The run (35_18) with a crouch laid over it: pelvis 14 cm lower, trunk 28 degrees further forward, "
              "head kept up, knees bent by IK on the captured foot paths (CMU's own careful runs, 77_10-13, "
              "are short bursts and circles that do not loop)."),
    dict(name="creep", take="136_09", loop=True, head=8,
         note="Subject 136, 'walk crouched': knees bent, trunk forward, a careful crouched walk."),
    dict(name="get_up", take="77_16", frames=(150, 520), anchor=470, ground_at="end", floor=True,
         cam_dx=0.35, cam_z=0.55, sheet_at=0.45,
         note="Subject 77: from lying on the belly, push up, a knee forward, up into a careful ready crouch."),
    dict(name="go_prone", derive=("get_up", 0, 80), take="77_16", reverse=True, speed=1.4,
         cam_dx=0.35, cam_z=0.55, sheet_at=1.0,
         note="get_up's first 2.7 s played backwards and 1.4x faster: from the ready crouch down onto a knee, the "
              "hands, and flat on the belly. Its last frame is get_up's first, so the two chain exactly."),
    dict(name="fall_forward", take="140_01", frames=(170, 560), reverse=True, face_at="start",
         speeds=[(0, 2.4), (40, 2.6), (90, 4.4), (300, 4.4), (322, 1.0), (390, 1.0)],
         jolt=(1, 18.0), limp=[(0, 0.0), (50, 0.0), (120, 0.65), (310, 0.65), (390, 0.4)],
         ground_at="start", floor=True, cam_dx=0.4, cam_z=0.55, sheet_at=1.0,
         note="140_01's get-up from face down, played backwards: a jolt at the hit (the trunk snaps forward, the "
              "head lags), the knees buckle and he drops at up to 4.4x (the fall about 1.8x faster than a plain "
              "2.1x reversal), the arms only half catching him (part way to slack), then he settles at real "
              "speed. CMU has no shot-and-collapse take."),
    dict(name="fall_back", take="90_18", frames=(63, 300), face_at="start", ground_at="start", floor=True,
         jolt=(1, -12.0), land=(6, 16), lock=False, cam_dx=-0.4, cam_z=0.55, sheet_at=0.6,
         note="Subject 90, 'rug pull fall', with a jolt at the hit (the trunk snaps back, the head lags): the feet "
              "go from under him and he lands on his back, in real time (about half a second from the feet going "
              "to the ground). The actor fell onto a mat, sank into it and bounced; ours is eased down in the fall "
              "and stays on the ground."),
    dict(name="stumble", take="77_19", loop=True, span=(60, 540), period=72, head=None,
         note="Subject 77, 'limping, hurt right leg': bent over, a quick step on the good leg."),
    dict(name="cower", take="79_73", loop=True, travel=False, span=(400, 680), period=200, head=None,
         note="Subject 79, 'scared': hunched, hands up by the face."),
    dict(name="duck", take="77_09", frames=(200, 540), face_at="start", ground_at="start", cam_z=0.7, sheet_at=0.25,
         note="Subject 77, 'duck to avoid flying object': drops into a crouch with the head down, then rises."),
    dict(name="crawl", make="belly_crawl", loop=True, rifle=True, cam_z=0.3, cam_pitch=14.0, cam_dist=3.6,
         sheet_at=0.2,
         note="Keyed by hand (CMU has no belly crawl): the low crawl under fire. Chest and hips on the ground, the "
              "head low, the rifle across the forearms in the right hand; one elbow and the same side's knee draw up "
              "together, side by side in turn; elbows and knees planted while they pull. 0.40 m a loop."),
    dict(name="crawl_high", take="111_03", loop=True, span=(560, 1200), period=240, head=None, floor_loop=True,
         hand_steps=True,
         note="Subject 111, crawling on hands and knees (a woman, pregnant: the only clean crawl in the library), "
              "for moving behind low cover. She slid her hands along the floor; ours are planted and step "
              "(hand_steps), each with the other side's knee."),
]


def clip_data(m, info, spec):
    """A Motion (at 30 fps) as a clips.json entry."""
    r = m.rest
    loop = bool(spec.get("loop"))
    d = m.parent_deltas()
    pv = r.i("pelvis")
    src = (f"CMU {spec['take']} frames {info['a']}-{info['b']}" if spec.get("take") else
           "keyed by hand on the rig (no capture)")
    c = {"fps": FPS, "frames": len(m), "loop": loop,
         "length": round((len(m) - 1) / FPS if loop else len(m) / FPS, 4),
         "stride": info.get("stride", 0.0) if loop and spec.get("travel", True) else 0.0,
         "speed": info.get("speed", 0.0) if loop else 0.0,
         "source": src, "leg": round(0.5 * (r.leg("l") + r.leg("r")), 5), "note": spec.get("note", ""),
         "bones": {n: np.round(d[n], 5).tolist() for n in r.names if r.parent[r.i(n)] >= 0},
         "pelvis_loc": np.round(m.H - r.head[pv], 5).tolist()}
    if "end_offset" in info:
        c["end_offset"] = info["end_offset"]
    for k in ("sheet_at", "cam_z", "cam_dist", "cam_pitch", "cam_dx", "rifle"):
        if k in spec:
            c[k] = spec[k]
    return c


ABOUT = {
    "credit": "The data used in this project was obtained from mocap.cs.cmu.edu. The database was created with funding "
              "from NSF EIA-0196217.",
    "rig": "human.py (27 bones), reference man human.build(seed=0, stand=0.02, curl=(30, 45, 25)), 1.75 m",
    "frame": "Blender's: x forward, y LEFT, z up, metres. For Unreal's y-right frame mirror quaternions as "
             "(w, -x, y, -z) and locations as (x, -y, z).",
    "bones": "per bone, per frame, [w, x, y, z]: the rotation D relative to the bone's rest, in its PARENT's space "
             "(the bone's local rotation in its parent's frame = D * RestLocal). Blender pose quaternion = "
             "RestLocal^-1 * D * RestLocal.",
    "pelvis_loc": "per frame [x, y, z]: the pelvis head's offset from its rest place, in the armature's space. The root "
                  "bone never moves (IN PLACE). Scale it by (this man's leg / leg) for another man.",
    "loops": "frames includes the closing frame, equal to the first: length = (frames - 1) / fps. Play rate for a "
             "travel loop = speed / stride * length (stride: metres travelled per loop by this 1.75 m man).",
}


def build(names=None, out=CLIPS_JSON, verbose=True):
    """Retarget the CLIPS onto the reference man and write clips.json (only `names` if given, keeping the rest)."""
    import kit
    import human
    kit.fresh(FPS)
    man = human.build(seed=0, nation="GER", stand=0.02, curl=(30, 45, 25))
    rest = Rest.of(man["armature"], man["body"])
    data = {}
    if names and os.path.exists(out):
        data = json.load(open(out))
    motions = {}
    for spec in CLIPS:
        if names and spec["name"] not in names and not any(
                s_.get("derive", (None,))[0] == spec["name"] for s_ in CLIPS if s_["name"] in names):
            continue
        if spec.get("derive"):
            m, info = derive(spec, motions[spec["derive"][0]], verbose=verbose)
        elif spec.get("make"):
            m, info = make_keyed(spec, rest, verbose=verbose)
        else:
            m, info = (make_loop if spec.get("loop") else make_once)(spec, rest, verbose=verbose)
        if not spec.get("make"):
            info["fit_arms_deg"] = fit_arms(m, loop=bool(spec.get("loop")))   # (elbows on their hinges, hands in range)
            info["eased_deg_s"] = ease_spikes(m, loop=bool(spec.get("loop")))  # (no joint past rom.py's 900 deg/s)
        motions[spec["name"]] = (m, info)
        if names and spec["name"] not in names:
            continue
        data[spec["name"]] = clip_data(m, info, spec)
        data[spec["name"]]["build"] = {k: v for k, v in info.items() if k not in ("contacts",)}
        data[spec["name"]]["build"]["ik"] = {k: (v if not isinstance(v, dict) else v.get("ik_err_max"))
                                             for k, v in info.get("contacts", {}).items()}
    order = [s["name"] for s in CLIPS if s["name"] in data]
    data = {"_about": ABOUT, **{n: data[n] for n in order}}
    with open(out, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    return data


if __name__ == "__main__":
    sys.path.insert(0, HERE)
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    cmd = args[0] if args else "build"
    if cmd == "build":
        build(args[1:] or None)
    elif cmd == "preview":
        names = preview(args[1:] or None)
    elif cmd == "sheet":
        clips = load_clips()
        sheet(args[1:] or [n for n in clips if not n.startswith("_")], clips=clips)
