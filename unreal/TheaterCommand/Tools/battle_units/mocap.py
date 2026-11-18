"""Real motion capture for the soldier: the CMU motion-capture library retargeted onto human.py's 27-bone rig.

Why: Brian asked (2026-11-18) for the soldiers to move "a lot more realistic". Hand-keyed cycles hit a ceiling, so the
body's movement comes from the Carnegie Mellon University Graphics Lab motion-capture database, which its makers
released for any use (see mocap_cmu/CREDITS.md: "The data used in this project was obtained from mocap.cs.cmu.edu.
The database was created with funding from NSF EIA-0196217."). The BVH files are the 2010 MotionBuilder-friendly
conversion; mocap_cmu/fetch_cmu.py downloads the takes we use. Nothing here comes from any game.

    import human, mocap
    man = human.build(seed=3, nation="GER", stand=0.02, curl=(30, 45, 25))
    mocap.apply(man["armature"], "walk")                 # makes the action `walk` from mocap_cmu/clips.json
    mocap.apply_all(man["armature"])                     # every clip, one action each
    mocap.rifle_arms(man["armature"], act, "port")       # both hands on a rifle, the body's mocap underneath

    python mocap.py build            rebuilds mocap_cmu/clips.json from the BVH files (fetch them first)
    python mocap.py preview [clip..] renders the preview films and the contact sheet

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
    toes are kept out of the ground.
  * Loops: the best matching cycle in the take is cut, its seam closed by spreading the small difference over the
    cycle, and resampled to 30 fps with the last frame equal to the first. Travel is removed (IN PLACE); `stride` is
    the metres the man travels per loop, so the game sets play rate = speed / stride * length and the feet stay put.

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


def two_bone_ik(m, side, target, weight, heads=None):
    """Bend thigh and calf so the ankle reaches `target` (F, 3) where weight (F,) > 0, the knee staying in the plane
    the mocap gave it; the foot keeps its world rotation. Soft near full stretch, so the knee never snaps."""
    r = m.rest
    th, ca, ft = r.i("thigh_" + side), r.i("calf_" + side), r.i("foot_" + side)
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
    pole = knee - hip
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
              merge=2):
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
        cs = contacts(sh, spd, ground, h_tol, v_tol)
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
    # nothing below the ground: lift a sinking foot by IK, and bend a sinking toe up
    for s in sides:
        for _ in range(2):
            sh, P = sole_heights(work, hd, s)[s]
            pen = np.maximum(0.0, ground - sh)
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
        fc.keyframe_points.foreach_set("interpolation", [2] * n)       # LINEAR (enum index), keys every frame
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


def make_loop(spec, rest, verbose=True):
    """A looping clip from spec: take, travel (bool), span (capture frames to search, optional), period (source
    frames, optional), head (target head pitch, degrees, optional), ground percentile, lock (bool)."""
    m = take_motion(spec["take"], rest)
    travel = spec.get("travel", True)
    if spec.get("span"):
        lo, hi = spec["span"][0] - 1, spec["span"][1] - 1
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
    dz = ground_fit(out, spec.get("ground", 20), both=spec.get("both_feet", not travel))
    rep = {}
    if spec.get("lock", True):
        foot_lock(out, stride, loop=True, report=rep, v_tol=spec.get("v_tol", 0.45), h_tol=spec.get("h_tol", 0.035))
    info = {"take": spec["take"], "a": a + 1, "b": b + 1, "cost": round(float(cost), 4), "period": int(per),
            "stride": round(stride, 3), "frames": n_out + 1, "dz": round(float(dz), 3), "contacts": rep,
            "speed": round(stride / (n_out / FPS), 3) if travel else 0.0}
    if verbose:
        print(spec["name"], info)
    return out, info


def make_once(spec, rest, verbose=True):
    """A one-shot clip: capture frames spec["frames"] = (a, b), turned to face +X at its start and set at the origin;
    ground fitted from the standing feet at its start (or end, ground_at="end"); feet locked while planted."""
    m = take_motion(spec["take"], rest)
    a, b = spec["frames"]
    m = m.slice(a - 1, b)
    yaw0 = spec.get("yaw", None)
    face_at = 0 if spec.get("face_at", "start") == "start" else len(m) - 1
    win = slice(max(0, face_at - 10), face_at + 10) if face_at == 0 else slice(face_at - 10, None)
    ang = -np.angle(np.mean(np.exp(1j * yaw_of(m)[win]))) if yaw0 is None else -math.radians(yaw0)
    turn(m, ang)
    pv = rest.i("pelvis")
    m.H[:, :2] -= m.H[face_at, :2] - rest.head[pv][:2]
    if spec.get("head") is not None:
        level_head(m, spec["head"])
    out = resample(m, np.arange(0, len(m) - 1 + 1e-6, m.fps / FPS))
    k = int(spec.get("ground_frames", 6))
    gsl = slice(0, k) if spec.get("ground_at", "start") == "start" else slice(len(out) - k, None)
    sh = sole_heights(out)
    low = np.minimum(sh["l"][0], sh["r"][0])[gsl]
    dz = rest.sole["ground"] - float(np.median(low)) + spec.get("dz", 0.0)
    out.H[:, 2] += dz
    rep = {}
    if spec.get("lock", True):
        foot_lock(out, 0.0, loop=False, report=rep, v_tol=spec.get("v_tol", 0.3), h_tol=spec.get("h_tol", 0.03))
    end = out.H[-1] - rest.head[pv]
    info = {"take": spec["take"], "a": a, "b": b, "frames": len(out), "dz": round(dz, 3), "contacts": rep,
            "end_offset": [round(float(x), 3) for x in end]}
    if verbose:
        print(spec["name"], info)
    return out, info


# ------------------------------------------------------------------------------------------------ previews
PREVIEW_DIR = "/mnt/project-files/battle-models/land/soldier/mocap/"


def preview_man(seed=0):
    """A plain matt-grey man (human.build, as the soldier is built), smooth and subdivided, for the previews."""
    import bpy
    import human
    import kit
    man = human.build(seed=seed, nation="GER", stand=0.02, curl=(30, 45, 25))
    grey = kit.material("matt_grey", kit.hex_rgb("8C8B86"), rough=0.75, spec=0.3)
    for ob in [man["body"]] + man["eyes"]:
        ob.data.materials.clear()
        ob.data.materials.append(grey)
    man["tights"].hide_render = man["skirt"].hide_render = True
    man["tights"].hide_viewport = man["skirt"].hide_viewport = True
    sub = man["body"].modifiers.new("smooth", "SUBSURF")
    sub.levels, sub.render_levels = 0, 1
    return man


def render_clip(man, clip, c, out_base, work, step=2, loops=1, cam_yaw=35.0, dist=4.6, threads=2, samples=16):
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
    tgt_z = c.get("cam_z", 0.85)
    for t in range(0, total, step):
        sc.frame_set(1 + (t % (n - 1) if c.get("loop") else t))
        x = speed * t / FPS
        arm.location = (x, 0, 0)
        kit.orbit(cam, (x + c.get("cam_dx", 0.0), 0, tgt_z), c.get("cam_dist", dist), c.get("cam_pitch", 6.0), cam_yaw)
        sc.render.filepath = os.path.join(work, f"{k:04d}.png")
        bpy.ops.render.render(write_still=True)
        k += 1
    arm.location = (0, 0, 0)
    kit.encode(work, out_base, fps=FPS // step)
    return out_base


def preview(names=None, out_dir=PREVIEW_DIR, work_dir="/home/user/art/mocap/frames", clips=None, step=2):
    """Preview films for the named clips (all by default) and the contact sheet clips.png."""
    import bpy
    import kit
    clips = clips or load_clips()
    names = names or [n for n in clips if not n.startswith("_")]
    kit.fresh(FPS)
    kit.studio(sun_elev=40, sun_turn=-40)
    kit.field_ground()
    man = preview_man()
    kit.camera(lens=50)
    sc = kit.render_setup(640, 360, samples=16)
    sc.render.threads_mode = "FIXED"
    sc.render.threads = 2
    for nm in names:
        c = clips[nm]
        apply(man["armature"], nm, clips)
        loops = 2 if c.get("loop") and c["frames"] < 60 else 1
        render_clip(man, nm, c, os.path.join(out_dir, nm), os.path.join(work_dir, nm), step=step, loops=loops)
        print("preview", nm, flush=True)
    return names


def sheet(names, out_dir=PREVIEW_DIR, work_dir="/home/user/art/mocap/frames", clips=None):
    """clips.png: one telling frame of each clip (the middle of a loop, the end of a one-shot's main action)."""
    import kit
    clips = clips or load_clips()
    ims, labels = [], []
    for nm in names:
        fr = sorted(f for f in os.listdir(os.path.join(work_dir, nm)) if f.endswith(".png"))
        pick = clips[nm].get("sheet_at", 0.5)
        ims.append(os.path.join(work_dir, nm, fr[min(len(fr) - 1, int(pick * (len(fr) - 1)))]))
        labels.append(nm)
    return kit.sheet(ims, os.path.join(out_dir, "clips.png"), cols=4, labels=labels,
                     title="The soldier's motion capture (CMU data, retargeted)")
