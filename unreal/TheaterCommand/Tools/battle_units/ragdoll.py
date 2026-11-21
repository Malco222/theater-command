"""A soldier's body as a ragdoll, for baking deaths offline (numpy only, no Blender).

Brian (21 November 2026): "when a soldier gets hit have their body be on the ground dead" / "i want it to be
realistic". A real fall is not a key-framed one: the body is a heavy, jointed thing that drops, folds at the knees,
hips and waist as far as they go, hits the ground part by part and stops where it stops. So falls.py throws the man
of a game soldier file (soldier_<ARMY>.json) into this simulation, with the muscle tone going out of him over a
fraction of a second, records where every bone went, and writes it back as an ordinary clip. The game plays that clip
once and keeps its last frame: a ragdoll's realism at a key-framed clip's cost.

THE MODEL. Rigid bodies by extended position-based dynamics (XPBD: Mueller, Macklin et al. 2020, "Detailed rigid
body simulation with extended position based dynamics"), one solve a substep at 720 Hz:
  * Every body part (19: pelvis, three spine pieces, neck, head, each arm and leg in three, the rifle) is one rigid
    body with a real man's mass (82 kg in all, the rifle 4.2) and an inertia from its own mesh, a floor under it so a hand or the
    neck is not whipped about; collision spheres on its surface (what touches the ground) and along its core (what
    keeps one limb out of another).
  * Parts are joined at the man's own joints (positional constraints at the shared pivot). Each joint's turn (child
    against parent) is split into a swing of the bone's axis and a twist about it, each held in a box of human
    ranges (rom.py's limits, a little inside them); the wrist is held in rom.py's own measure (the forearm's line
    seen from the hand). The ranges are applied last in each substep, so no frame shows a joint past them.
  * Muscle tone: at the hit every joint is still driven toward the pose he had (a soft angular constraint, its
    compliance 1 / (strength x tone)); the tone fades over the kind's own times (the legs going first when the knees
    give way, the trunk first when he is thrown back), so he folds as a man does rather than toppling like a plank or
    dropping like a sack. Joint damping: more with tone in him, some always.
  * Ground: z = 0, static and dynamic friction (earth and grass), no bounce. A pose that starts on the ground is lifted
    clear of it first; parts that start closer than their spheres keep that distance.
  * The rifle rides in his right hand (it goes where the hand goes and takes nothing from it) until he lets go, then
    falls as a body of its own.
  * Once he is down and all but still, cloth and soil take the last of the motion (extra damping), and the fall
    ends when nothing moves.

AXES: the game file's (x forward, y right, z up, metres), quaternions [w, x, y, z] as the file's clips.
"""

import math

import numpy as np

G = np.array((0.0, 0.0, -9.81))


# ------------------------------------------------------------------------------------------------ rotations
def qmat(q):
    """[w, x, y, z] -> 3x3 (works on (..., 4))."""
    q = np.asarray(q, float)
    q = q / np.linalg.norm(q, axis=-1, keepdims=True)
    w, x, y, z = np.moveaxis(q, -1, 0)
    return np.stack([
        np.stack([1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)], -1),
        np.stack([2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)], -1),
        np.stack([2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)], -1)], -2)


def matq(M):
    """3x3 -> [w, x, y, z] (w >= 0), for (..., 3, 3)."""
    M = np.asarray(M, float)
    sh = M.shape[:-2]
    M = M.reshape(-1, 3, 3)
    out = np.zeros((len(M), 4))
    for i, m in enumerate(M):
        t = m[0, 0] + m[1, 1] + m[2, 2]
        if t > 0:
            s = math.sqrt(t + 1.0) * 2
            q = (0.25 * s, (m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s)
        elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
            s = math.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
            q = ((m[2, 1] - m[1, 2]) / s, 0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s)
        elif m[1, 1] > m[2, 2]:
            s = math.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
            q = ((m[0, 2] - m[2, 0]) / s, (m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s)
        else:
            s = math.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
            q = ((m[1, 0] - m[0, 1]) / s, (m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s)
        q = np.array(q)
        out[i] = q / np.linalg.norm(q) * (1 if q[0] >= 0 else -1)
    return out.reshape(sh + (4,))


def axis_angle(axis, ang):
    a = np.asarray(axis, float)
    n = np.linalg.norm(a)
    if n < 1e-12 or abs(ang) < 1e-12:
        return np.eye(3)
    a = a / n
    K = np.array(((0, -a[2], a[1]), (a[2], 0, -a[0]), (-a[1], a[0], 0)))
    return np.eye(3) + math.sin(ang) * K + (1 - math.cos(ang)) * (K @ K)


def rotvec(R):
    """3x3 -> rotation vector (axis * angle)."""
    c = np.clip((np.trace(R) - 1) * 0.5, -1.0, 1.0)
    ang = math.acos(c)
    if ang < 1e-9:
        return np.zeros(3)
    v = np.array((R[2, 1] - R[1, 2], R[0, 2] - R[2, 0], R[1, 0] - R[0, 1]))
    s = np.linalg.norm(v)
    if s < 1e-9:                                  # (180 degrees: the axis from the symmetric part)
        w, V = np.linalg.eigh((R + R.T) * 0.5)
        return V[:, np.argmax(w)] * ang
    return v / s * ang


def from_rotvec(v):
    v = np.asarray(v, float)
    a = np.linalg.norm(v)
    return axis_angle(v / a, a) if a > 1e-12 else np.eye(3)


def frac(R, k):
    """R raised to the k (0 to 1): the same axis, k of the angle."""
    return from_rotvec(rotvec(R) * k)


def arc(a, b):
    """The shortest turn taking unit a onto unit b."""
    a = a / np.linalg.norm(a)
    b = b / np.linalg.norm(b)
    c = np.cross(a, b)
    s = np.linalg.norm(c)
    d = float(np.clip(a @ b, -1.0, 1.0))
    if s < 1e-9:
        if d > 0:
            return np.eye(3)
        p = np.cross(a, (1.0, 0.0, 0.0))
        if np.linalg.norm(p) < 1e-6:
            p = np.cross(a, (0.0, 1.0, 0.0))
        return axis_angle(p, math.pi)
    return axis_angle(c / s, math.atan2(s, d))


def kabsch(P0, P, w):
    """Batched best-fit rotations: P0, P (B, N, 3) about their weighted centroids, w (B, N). Returns R (B, 3, 3), c."""
    ws = w / w.sum(1, keepdims=True)
    c0 = (P0 * ws[..., None]).sum(1)
    c = (P * ws[..., None]).sum(1)
    A = np.einsum("bn,bni,bnj->bij", ws, P - c[:, None], P0 - c0[:, None])
    U, S, Vt = np.linalg.svd(A)
    d = np.sign(np.linalg.det(U @ Vt))
    D = np.ones((len(A), 3))
    D[:, 2] = d
    R = np.einsum("bij,bj,bjk->bik", U, D, Vt)
    return R, c, c0


# ------------------------------------------------------------------------------------------------ the file's man
class Man:
    """A soldier file's skeleton, near mesh and clips; forward kinematics and skinning by the file's own rule."""

    def __init__(self, data, lod="near"):
        self.data = data
        self.bones = data["bones"]
        self.names = [b["name"] for b in self.bones]
        self.ix = {n: i for i, n in enumerate(self.names)}
        self.parent = [b["parent"] if b["parent"] is not None else -1 for b in self.bones]
        self.head = np.array([b["head"] for b in self.bones], float)
        m = data[lod]
        self.P = np.array(m["points"], float).reshape(-1, 3)
        self.part = np.array(m["part"], int)
        self.part2 = np.array(m["part2"], int)
        self.w2 = np.array(m["w2"], float)
        self.clips = data["clips"]

    def i(self, n):
        return self.ix[n]

    def local_at(self, clip, t):
        """{bone: (q local [w,x,y,z], move or None)} of a clip at time t (keys slerped, held past the ends)."""
        c = self.clips[clip] if isinstance(clip, str) else clip
        out = {}
        for tr in c["tracks"]:
            if "rot" in tr:
                q = _sample_q(np.array(tr["rot"], float), t)
            else:
                q = np.array((1.0, 0.0, 0.0, 0.0))        # (a track that only moves its bone: the Thompson's bolt)
            mv = None
            if "move" in tr:
                mv = _sample_v(np.array(tr["move"], float), t)
            out[tr["part_name"]] = (q, mv)
        return out

    def fk(self, local):
        """Local turns (and moves) -> world turns G (B,3,3) and heads X (B,3), the file's rule."""
        B = len(self.names)
        Gw = np.zeros((B, 3, 3))
        X = np.zeros((B, 3))
        for b in range(B):
            p = self.parent[b]
            n = self.names[b]
            q, mv = local.get(n, (None, None))
            Q = qmat(q) if q is not None else np.eye(3)
            mv = np.zeros(3) if mv is None else np.asarray(mv, float)
            if p < 0:
                Gw[b] = Q
                X[b] = self.head[b] + mv
            else:
                Gw[b] = Gw[p] @ Q
                X[b] = X[p] + Gw[p] @ (self.head[b] - self.head[p] + mv)
        return Gw, X

    def skin(self, Gw, X, P=None, part=None, part2=None, w2=None):
        P = self.P if P is None else P
        part = self.part if part is None else part
        part2 = self.part2 if part2 is None else part2
        w2 = self.w2 if w2 is None else w2
        a = X[part] + np.einsum("nij,nj->ni", Gw[part], P - self.head[part])
        b = X[part2] + np.einsum("nij,nj->ni", Gw[part2], P - self.head[part2])
        return a * (1 - w2)[:, None] + b * w2[:, None]


def _sample_q(rot, t):
    ts = rot[:, 0]
    if t <= ts[0]:
        return rot[0, 1:]
    if t >= ts[-1]:
        return rot[-1, 1:]
    k = int(np.searchsorted(ts, t) - 1)
    u = (t - ts[k]) / (ts[k + 1] - ts[k])
    return slerp(rot[k, 1:], rot[k + 1, 1:], u)


def _sample_v(mv, t):
    ts = mv[:, 0]
    if t <= ts[0]:
        return mv[0, 1:]
    if t >= ts[-1]:
        return mv[-1, 1:]
    k = int(np.searchsorted(ts, t) - 1)
    u = (t - ts[k]) / (ts[k + 1] - ts[k])
    return mv[k, 1:] * (1 - u) + mv[k + 1, 1:] * u


def slerp(a, b, u):
    a = np.asarray(a, float)
    b = np.asarray(b, float)
    d = float(a @ b)
    if d < 0:
        b, d = -b, -d
    if d > 0.9995:
        q = a + (b - a) * u
        return q / np.linalg.norm(q)
    th = math.acos(min(d, 1.0))
    s = math.sin(th)
    return (math.sin((1 - u) * th) * a + math.sin(u * th) * b) / s


# ------------------------------------------------------------------------------------------------ the ragdoll's build
# Parts: (name, the file's bones it carries (the first is its own; the rest ride it at a fixed turn), parent part,
# mass kg). A man of about 70 kg with some 12 kg of kit; the rifle 4.2 kg.
PARTS = [
    ("pelvis", ("pelvis", "pouch_l", "pouch_r"), None, 12.0),
    ("spine_01", ("spine_01",), "pelvis", 6.0),
    ("spine_02", ("spine_02",), "spine_01", 7.0),
    ("chest", ("chest", "clavicle_l", "clavicle_r"), "spine_02", 15.0),
    ("neck", ("neck",), "chest", 1.2),
    ("head", ("head",), "neck", 5.8),
    ("upperarm_l", ("upperarm_l",), "chest", 2.1),
    ("lowerarm_l", ("lowerarm_l",), "upperarm_l", 1.3),
    ("hand_l", ("hand_l", "fingers_l", "thumb_l"), "lowerarm_l", 0.5),
    ("upperarm_r", ("upperarm_r",), "chest", 2.1),
    ("lowerarm_r", ("lowerarm_r",), "upperarm_r", 1.3),
    ("hand_r", ("hand_r", "fingers_r", "thumb_r"), "lowerarm_r", 0.5),
    ("thigh_l", ("thigh_l",), "pelvis", 8.5),
    ("calf_l", ("calf_l",), "thigh_l", 3.6),
    ("foot_l", ("foot_l", "ball_l"), "calf_l", 1.4),
    ("thigh_r", ("thigh_r",), "pelvis", 8.5),
    ("calf_r", ("calf_r",), "thigh_r", 3.6),
    ("foot_r", ("foot_r", "ball_r"), "calf_r", 1.4),
    ("weapon", ("weapon", "bolt", "bipod"), None, 4.2),
]

# Joint ranges, degrees, measured from the file's rest (standing, arms 45 degrees down and bent 47 at the elbow, knees
# bent 6): (flex min, flex max), (side min, side max), (twist min, twist max). Kept a little inside rom.py's limits
# (the checker's), the spine's and neck's spread over their joints. "flex" turns about the joint's own flexion axis
# (forward for the spine, neck, hip and shoulder; the hinge for the knee and elbow; toes up at the ankle); "side" to
# the man's right for the spine and neck, outward for the hip, upward for the shoulder.
RANGES = {
    "spine_01": ((-7.0, 24.0), (-8.5, 8.5), (-13.0, 13.0)),
    "spine_02": ((-7.0, 24.0), (-7.5, 7.5), (-11.0, 11.0)),
    "chest": ((-7.0, 24.0), (-7.5, 7.5), (-11.0, 11.0)),
    "neck": ((-26.0, 21.0), (-13.0, 13.0), (-34.0, 34.0)),
    "head": ((-26.0, 21.0), (-13.0, 13.0), (-34.0, 34.0)),
    "upperarm": ((-28.0, 110.0), (-50.0, 40.0), (-24.0, 24.0)),
    "lowerarm": ((-44.0, 92.0), (-6.0, 6.0), (-6.0, 6.0)),
    "hand": ((-80.0, 80.0), (-40.0, 40.0), (-50.0, 50.0)),
    "thigh": ((-20.0, 112.0), (-24.0, 40.0), (-38.0, 38.0)),
    "calf": ((-5.0, 138.0), (-6.0, 6.0), (-8.0, 8.0)),
    "foot": ((-45.0, 22.0), (-14.0, 14.0), (-16.0, 16.0)),
}
# The flexion axes the file's clips use at the knee and elbow (a bent knee's or elbow's turn, measured on fire_kneel
# and fire_stand): positive turns about these bend the joint.
# The hands (rom.py's reference man, in this file's axes): the axis wrist to middle knuckle, and the palm's normal.
HAND = {"l": ((0.769, -0.431, -0.471), (0.012, 0.748, -0.664)), "r": ((0.769, 0.431, -0.471), (0.012, -0.748, -0.664))}
HINGE = {"calf_l": (-0.188, 0.977, -0.104), "calf_r": (0.188, 0.977, 0.104),
         "lowerarm_l": (-0.061, -0.733, 0.678), "lowerarm_r": (0.061, -0.733, -0.678)}


def fib_dirs(n):
    i = np.arange(n) + 0.5
    phi = np.arccos(1 - 2 * i / n)
    th = math.pi * (1 + 5 ** 0.5) * i
    return np.stack([np.cos(th) * np.sin(phi), np.sin(th) * np.sin(phi), np.cos(phi)], 1)


def _unit(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def swing_twist(Rrel, t):
    """Rrel = S @ T: S swings the axis t to where it now points, T twists about t. Returns (swing rotvec, twist)."""
    t2 = Rrel @ t
    S = arc(t, t2)
    T = S.T @ Rrel
    tw = float(rotvec(T) @ t)
    return rotvec(S), tw


# The wrist as rom.py measures it (the forearm's line seen from the hand): flexion +, extension -, radial deviation +,
# ulnar -; a little inside its limits (75, 65, 20, 30). The box above only keeps the wrist's swing sane; this is the
# range that holds.
WRIST = {"flex": (-60.0, 70.0), "dev": (-27.0, 18.0)}


def wrist_angles(Rrel, j):
    u = Rrel.T @ j["fa"]
    ua = float(u @ j["wa"])
    return math.degrees(math.atan2(-float(u @ j["wn"]), ua)), math.degrees(math.atan2(-float(u @ j["wt"]), ua)), u


def wrist_fix(Rrel, j):
    """The nearest wrist turn inside WRIST (rom's flexion and deviation); None if inside."""
    F, D, u = wrist_angles(Rrel, j)
    (f0, f1), (d0, d1) = WRIST["flex"], WRIST["dev"]
    if f0 <= F <= f1 and d0 <= D <= d1:
        return None
    F, D = min(max(F, f0), f1), min(max(D, d0), d1)
    u2 = _unit(j["wa"] - math.tan(math.radians(F)) * j["wn"] - math.tan(math.radians(D)) * j["wt"])
    ax = np.cross(u, u2)
    s, c = float(np.linalg.norm(ax)), float(u @ u2)
    if s < 1e-12:
        return None
    Q = axis_angle(ax / s, math.atan2(s, c))
    return Rrel @ Q.T


def clamp_joint(Rrel, j):
    """The nearest turn inside the joint's range (swing in its ellipse, twist in its range); None if inside."""
    if j.get("twist_first"):                         # (the wrist: its bend is the hand's own, after the forearm's
        w, tw = swing_twist(Rrel.T, j["t"])          # twist, so Rrel = twist @ swing)
        w, tw = -w, -tw
    else:
        w, tw = swing_twist(Rrel, j["t"])
    p1, p2 = float(w @ j["a1"]), float(w @ j["a2"])
    (f0, f1), (s0, s1), (t0, t1) = j["rng"]
    # (each swing held in its own range: an ellipse scaled back toward its middle would also take a hinge's bend
    # away whenever it twisted a little off the hinge)
    out = False
    if not (f0 <= p1 <= f1 and s0 <= p2 <= s1):
        p1, p2 = min(max(p1, f0), f1), min(max(p2, s0), s1)
        out = True
    if tw < t0 or tw > t1:
        tw = min(max(tw, t0), t1)
        out = True
    if j.get("twist_first"):
        Rn = axis_angle(j["t"], tw) @ from_rotvec(p1 * j["a1"] + p2 * j["a2"]) if out else Rrel
        Rw = wrist_fix(Rn, j) if "fa" in j else None
        return Rw if Rw is not None else (Rn if out else None)
    if not out:
        return None
    return from_rotvec(p1 * j["a1"] + p2 * j["a2"]) @ axis_angle(j["t"], tw)


class Ragdoll:
    """The man of a soldier file as rigid parts (see the module's note): each part's mass, centre, inertia,
    collision spheres; the joints with their ranges. Built once per man."""

    SURF_R = 0.018
    GYR = 0.06
    N_DIRS = {"pelvis": 40, "spine_01": 26, "spine_02": 26, "chest": 40, "head": 30, "neck": 10}

    def __init__(self, man, fixed=None, weapon_kg=None, bag=(), loose=None, gear_kg=None, bag_at=None):
        """`weapon_kg`: what his weapon weighs (default the rifle's 4.2; a file with nothing on the weapon bone is
        unarmed: the bone is carried, weightless and unseen, and never let go). `bag`: bones that ride the weapon
        bone in the file but are packed on him (the bazooka loader's rocket in its bag): out of the physics, placed
        by falls.py where they sat in the pose he was hit in. `loose`: a mask over the near points to keep out of his
        shape (the radio man's 3.25 m whip antenna, which would prop him up like a pole). `gear_kg`: weight packed on
        a part ({part: kg}; the radio set on his chest). `bag_at`: {bag bone: (host bone, turn, offset)} where a bag
        bone sits on its host (falls.bag_rel): its points then join the host's shape, so what is clipped on his front
        (the radio man's handset) meets the ground instead of passing into it."""
        self.man = m = man
        self.fixed = dict(fixed or {})
        self.bag = tuple(b for b in bag if b in m.ix)
        gear = dict(gear_kg or {})
        self.PARTS = [(n, tuple(b for b in bones if b not in self.bag), par,
                       (weapon_kg if (n == "weapon" and weapon_kg) else mass) + gear.get(n, 0.0))
                      for (n, bones, par, mass) in PARTS]
        local = {n: (np.asarray(q, float), None) for n, q in self.fixed.items()}
        Gr, Xr = m.fk(local)
        self.Gr, self.Xr = Gr, Xr
        Pr = m.skin(Gr, Xr)
        self.parts = [p[0] for p in self.PARTS]
        self.pix = {n: i for i, n in enumerate(self.parts)}
        self.bone_part = {b: k for k, p in enumerate(self.PARTS) for b in p[1]}
        self.parent = [self.pix[p[2]] if p[2] else -1 for p in self.PARTS]
        B = len(self.PARTS)
        own = np.array([self.bone_part.get(m.names[b], -1) for b in m.part])
        if loose is not None:
            own[np.asarray(loose, bool)] = -1
        self.unarmed = not np.any(own == self.pix["weapon"])
        if self.unarmed:
            self.PARTS[self.pix["weapon"]] = self.PARTS[self.pix["weapon"]][:3] + (0.05,)
        self.mass = np.array([p[3] for p in self.PARTS])
        self.c0 = np.zeros((B, 3))
        self.I0inv = np.zeros((B, 3, 3))
        self.head_off = np.zeros((B, 3))           # each part's own bone head, from its centre (rest)
        extra = {}                                  # (bag bones' points, placed on their host in the rest pose)
        for b, (h, Rr, pr) in (bag_at or {}).items():
            ib, ih = m.i(b), m.i(h)
            G = Gr[ih] @ Rr
            Xb = Xr[ih] + Gr[ih] @ pr
            extra.setdefault(self.bone_part[h], []).append(Xb + (m.P[m.part == ib] - m.head[ib]) @ G.T)
        sph = []                                    # (part, offset from centre (rest), radius, core?)
        for k, (n, bones, par, mass) in enumerate(self.PARTS):
            pts = np.vstack([Pr[own == k]] + extra.get(k, []))
            if not len(pts):                        # (the unarmed man's weapon bone: a point, nothing to touch)
                pts = Xr[m.i(bones[0])][None, :]
            c0 = pts.mean(0)
            self.c0[k] = c0
            C = np.cov((pts - c0).T, bias=True)
            # (a floor under the inertia, as ragdolls have: a light part (a hand, the neck) turned by its joints
            # would otherwise spin a long way in one step)
            I0 = 0.75 * mass * (np.trace(C) * np.eye(3) - C) + mass * self.GYR ** 2 * np.eye(3)
            self.I0inv[k] = np.linalg.inv(I0)
            h0 = Xr[m.i(bones[0])]
            self.head_off[k] = h0 - c0
            if n == "weapon" and self.unarmed:
                continue
            if n == "weapon":
                ax = _unit(Gr[m.i("weapon")] @ np.array((1.0, 0.0, 0.0)))
                s = (pts - h0) @ ax
                cuts = np.linspace(s.min(), s.max(), 9)
                for a, b in zip(cuts[:-1], cuts[1:]):
                    seg = pts[(s >= a) & (s <= b)]
                    if len(seg) < 3:
                        continue
                    for dv in fib_dirs(8):
                        j = np.argmax(seg @ dv)
                        sph.append((k, seg[j] - 0.012 * dv - c0, 0.012, False))
                    cc = seg.mean(0)
                    rr = float(np.percentile(np.linalg.norm(seg - cc, axis=1), 40))
                    sph.append((k, cc - c0, min(max(rr, 0.015), 0.035), True))
                continue
            for dv in fib_dirs(self.N_DIRS.get(n, 16)):
                j = np.argmax(pts @ dv)
                sph.append((k, pts[j] - self.SURF_R * dv - c0, self.SURF_R, False))
            for q in extra.get(k, []):              # (a small thing on him gets its own: the parts' few miss it)
                for dv in fib_dirs(12):
                    j = np.argmax(q @ dv)
                    sph.append((k, q[j] - self.SURF_R * dv - c0, self.SURF_R, False))
            if n in ("pelvis", "spine_01", "spine_02", "chest"):
                x15, x85 = np.percentile(pts[:, 0], 15), np.percentile(pts[:, 0], 85)
                r = 0.45 * (x85 - x15)
                wy = 0.5 * (np.percentile(pts[:, 1], 95) - np.percentile(pts[:, 1], 5))
                for y in np.linspace(-max(wy - r, 0.0), max(wy - r, 0.0), 3):
                    sph.append((k, np.array((0.5 * (x15 + x85), y, float(np.median(pts[:, 2])))) - c0, r, True))
            elif n == "head":
                r = float(np.percentile(np.linalg.norm(pts - pts.mean(0), axis=1), 40))
                sph.append((k, np.zeros(3), r, True))
            else:
                kids = [c for c in range(len(m.names)) if m.parent[c] == m.i(bones[0]) and m.names[c] not in bones]
                tip = Xr[kids[0]] if kids else pts[np.argmax(np.linalg.norm(pts - h0, axis=1))]
                L = np.linalg.norm(tip - h0)
                ax = (tip - h0) / L
                s = (pts - h0) @ ax
                rad = np.linalg.norm((pts - h0) - np.outer(s, ax), axis=1)
                for u in ((0.5,) if n.startswith(("hand", "neck")) else (0.2, 0.5, 0.8)):
                    sel = np.abs(s - u * L) < 0.25 * L + 0.01
                    r = float(np.percentile(rad[sel], 50)) * 0.85 if sel.sum() > 3 else 0.03
                    sph.append((k, h0 + u * L * ax - c0, max(r, 0.02), True))
                if n.startswith(("thigh", "calf", "upperarm", "lowerarm")):
                    # (rings of surface points round the limb: a padded sleeve or trouser leg is wider than its
                    # core, and the few surface points above all lie at its ends)
                    e1 = _unit(np.cross(ax, (0.0, 0.0, 1.0)) if abs(ax[2]) < 0.9 else np.cross(ax, (1.0, 0.0, 0.0)))
                    e2 = np.cross(ax, e1)
                    for u in (0.15, 0.38, 0.62, 0.85):
                        sel = np.abs(s - u * L) < 0.12 * L
                        if sel.sum() < 8:
                            continue
                        q = pts[sel]
                        for a in np.arange(8) * (math.pi / 4):
                            dv = math.cos(a) * e1 + math.sin(a) * e2
                            j = int(np.argmax((q - h0) @ dv))
                            sph.append((k, q[j] - self.SURF_R * dv - c0, self.SURF_R, False))
        self.sp_part = np.array([s[0] for s in sph], int)
        self.sp_off = np.array([s[1] for s in sph], float)
        self.sp_r = np.array([s[2] for s in sph], float)
        self.sp_core = np.array([s[3] for s in sph], bool)
        # joints
        self.joints = []
        for k, (n, bones, par, mass) in enumerate(self.PARTS):
            if par is None:
                continue
            kp = self.pix[par]
            fam = n.rsplit("_", 1)[0] if n.endswith(("_l", "_r")) else n
            side = n[-1] if n.endswith(("_l", "_r")) else ""
            b0 = m.i(bones[0])
            kids = [c for c in range(len(m.names)) if m.parent[c] == b0 and m.names[c] not in bones]
            if n.startswith("foot"):
                tip = Xr[m.i("ball_" + side)]
            elif n.startswith("hand"):
                tip = Xr[m.i("fingers_" + side)]
            elif n == "head":
                tip = Xr[b0] + np.array((0.0, 0.0, 0.15))
            else:
                tip = Xr[kids[0]]
            t = _unit(tip - Xr[b0])
            out = (0.0, -1.0 if side == "l" else 1.0, 0.0)
            if fam == "hand":                       # (flexion toward the palm, radial deviation toward the thumb)
                t = _unit(HAND[side][0])
                a1 = _unit(np.cross(t, HAND[side][1]))
                f2 = Xr[m.i("thumb_" + side)] - Xr[b0]
                f2 = f2 - t * (t @ f2)
            elif n in HINGE:
                a1 = _unit(HINGE[n])
                a1 = _unit(a1 - t * (a1 @ t))
                f2 = np.cross(a1, t)
            else:
                f1 = {"upperarm": (1.0, 0, 0), "thigh": (1.0, 0, 0), "foot": (0, 0, 1.0)}.get(fam, (1.0, 0.0, 0.0))
                f1 = _unit(np.asarray(f1, float) - t * (t @ np.asarray(f1, float)))
                a1 = _unit(np.cross(t, f1))
                f2 = {"upperarm": (0, 0, 1.0), "thigh": out, "foot": out}.get(fam)
                f2 = np.cross(a1, t) if f2 is None else np.asarray(f2, float)
            a2 = _unit(np.cross(t, a1))
            if np.cross(a2, t) @ f2 < 0:
                a2 = -a2
            self.joints.append(dict(name=n, fam=fam, parent=kp, child=k, rp=Xr[b0] - self.c0[kp],
                                    rc=Xr[b0] - self.c0[k], t=t, a1=a1, a2=a2,
                                    rng=np.radians(np.array(RANGES[fam], float)), R0=np.eye(3),
                                    twist_first=(fam == "hand")))
            if fam == "hand":                       # (rom's wrist measure: forearm line, hand axis, palm, thumb way)
                wa, wn = _unit(HAND[side][0]), _unit(HAND[side][1])
                self.joints[-1].update(fa=_unit(Xr[b0] - Xr[m.i("lowerarm_" + side)]), wa=wa, wn=wn,
                                       wt=_unit(np.cross(wn, wa)) * (-1.0 if side == "l" else 1.0))
        # self-collision: core spheres of parts that are not joined and do not touch at rest
        cores = np.nonzero(self.sp_core)[0]
        rest = self.c0[self.sp_part] + self.sp_off
        joined = {(j["parent"], j["child"]) for j in self.joints} | {(j["child"], j["parent"]) for j in self.joints}
        pairs = []
        for a in range(len(cores)):
            for b in range(a + 1, len(cores)):
                i, j = cores[a], cores[b]
                ka, kb = self.sp_part[i], self.sp_part[j]
                if ka == kb or (ka, kb) in joined:
                    continue
                if self.parent[ka] == self.parent[kb] and self.parent[ka] == self.pix["chest"]:
                    continue                        # (the neck and the upper arms all hang off the chest)
                if np.linalg.norm(rest[i] - rest[j]) < (self.sp_r[i] + self.sp_r[j]) * 1.02:
                    continue
                pairs.append((i, j))
        self.pairs = np.array(pairs, int).reshape(-1, 2)


# Muscle tone: how stiffly each joint holds his pose while there is tone in him (N m per radian), and the damping
# of each joint (per second) with the tone in him and when there is none (a dead joint still has some).
HOLD = {"spine_01": 900.0, "spine_02": 900.0, "chest": 900.0, "neck": 120.0, "head": 80.0, "upperarm": 120.0,
        "lowerarm": 60.0, "hand": 8.0, "thigh": 1400.0, "calf": 1100.0, "foot": 250.0}
DAMP_TONE, DAMP_DEAD = 25.0, 6.0
SETTLE_DAMP = 10.0                  # (per second, on every part once he is all but still and down)
SETTLE_LOW = 0.32                   # (m: his chest's centre under this is down)
DAMP_DEAD_OF = {"hand": 18.0, "neck": 18.0, "head": 18.0, "foot": 10.0, "lowerarm": 9.0}


class Fall:
    """One fall of a Ragdoll from a pose, by extended position-based dynamics (rigid parts, one solve a substep;
    Mueller et al. 2020). run(); then .rec has each frame's part turns and heads."""

    def __init__(self, rd, local, local_prev, dt_prev, travel=(0.0, 0.0, 0.0), hz=720, fps=30,
                 mu_s=0.9, mu_d=0.7):
        self.rd = rd
        self.hz, self.fps = hz, fps
        self.h = 1.0 / hz
        self.mu_s, self.mu_d = mu_s, mu_d
        B = len(rd.parts)
        self.joints = [dict(j) for j in rd.joints]
        Ra, Xa = self._pose(local)
        Rb, Xb = self._pose(local_prev)
        self.R, self.x = Ra, Xa
        self._settle(30)
        self.v = (Xa - Xb) / dt_prev + np.asarray(travel, float)
        self.w = np.array([rotvec(Ra[k] @ Rb[k].T) / dt_prev for k in range(B)])
        self.hold = {j["child"]: self.R[j["parent"]].T @ self.R[j["child"]] for j in self.joints}
        self.target = dict(self.hold)
        self.tone = {j["child"]: (lambda t: 0.0) for j in self.joints}
        self.t = 0.0
        self.rec = []
        # the rifle in his right hand until he lets it go
        hr, wk = rd.pix["hand_r"], rd.pix["weapon"]
        piv = self.x[wk] + self.R[wk] @ rd.head_off[wk]
        self.weld = dict(name="weld", fam=None, parent=hr, child=wk, rp=self.R[hr].T @ (piv - self.x[hr]),
                         rc=rd.head_off[wk], R0=self.R[hr].T @ self.R[wk])
        self.release_at = 0.15
        self.throw = np.zeros(3)
        self.throw_w = np.zeros(3)
        self.held = True
        self.contact_t, self.contact_v = {}, {}
        self.still = 0.0
        self.lam_n = np.zeros(len(rd.sp_r))
        wk = rd.pix["weapon"]
        keep = (rd.sp_part[rd.pairs[:, 0]] != wk) & (rd.sp_part[rd.pairs[:, 1]] != wk)
        self.pairs = rd.pairs[keep]          # (the rifle joins in when it leaves his hand)
        self.push = []                              # (t, part, dv): blows to give him
        # (a pose that starts on the ground, lying or kneeling, is lifted clear of it: the ground would otherwise
        # throw him out of it in one step; and parts that start closer than their spheres keep that distance)
        P = self._spheres()
        pen = float(np.max(rd.sp_r - P[:, 2]))
        if pen > 0.0:
            self.x[:, 2] += pen + 0.001
            P[:, 2] += pen + 0.001
        self.gap = self._gaps(self.pairs, P)
        self.slow = 0.0

    # -------------------------------------------------------------------------------------------- state
    def _pose(self, local):
        rd, m = self.rd, self.rd.man
        loc = dict(local)
        for n, q in rd.fixed.items():
            loc[n] = (np.asarray(q, float), None)
        for k, (n, bones, par, mass) in enumerate(rd.PARTS):
            for b in bones[1:]:
                if b not in rd.fixed:
                    loc[b] = (np.array((1.0, 0.0, 0.0, 0.0)), None)
        Gw, X = m.fk(loc)
        R = np.array([Gw[m.i(p[1][0])] for p in rd.PARTS])
        x = np.array([X[m.i(p[1][0])] - R[k] @ rd.head_off[k] for k, p in enumerate(rd.PARTS)])
        return R, x

    def heads(self):
        return self.x + np.einsum("bij,bj->bi", self.R, self.rd.head_off)

    def _spheres(self):
        rd = self.rd
        return self.x[rd.sp_part] + np.einsum("nij,nj->ni", self.R[rd.sp_part], rd.sp_off)

    def _gaps(self, pairs, P):
        rd = self.rd
        if not len(pairs):
            return np.zeros(0)
        i, j = pairs[:, 0], pairs[:, 1]
        return np.minimum(rd.sp_r[i] + rd.sp_r[j], np.linalg.norm(P[j] - P[i], axis=1))

    def _Iinv(self, k):
        return self.R[k] @ self.rd.I0inv[k] @ self.R[k].T

    # -------------------------------------------------------------------------------------------- corrections
    def _pos(self, k1, r1, k2, r2, d, alpha=0.0):
        """Move the point r1 of part k1 by d against the point r2 of part k2 (k2 < 0: the ground). Returns lambda."""
        c = float(np.linalg.norm(d))
        if c < 1e-9:
            return 0.0
        n = d / c
        rd = self.rd
        I1 = self._Iinv(k1)
        a1 = np.cross(r1, n)
        w = 1.0 / rd.mass[k1] + a1 @ I1 @ a1
        if k2 >= 0:
            I2 = self._Iinv(k2)
            a2 = np.cross(r2, n)
            w += 1.0 / rd.mass[k2] + a2 @ I2 @ a2
        lam = c / (w + alpha / (self.h * self.h))
        p = lam * n
        self.x[k1] += p / rd.mass[k1]
        self.R[k1] = from_rotvec(I1 @ np.cross(r1, p)) @ self.R[k1]
        if k2 >= 0:
            self.x[k2] -= p / rd.mass[k2]
            self.R[k2] = from_rotvec(-(I2 @ np.cross(r2, p))) @ self.R[k2]
        return lam

    def _ang(self, k1, k2, phi, alpha=0.0):
        """Turn part k2 by the world turn phi against part k1, shared by their inertia."""
        th = float(np.linalg.norm(phi))
        if th < 1e-9:
            return
        n = phi / th
        I1, I2 = self._Iinv(k1), self._Iinv(k2)
        lam = th / (n @ I1 @ n + n @ I2 @ n + alpha / (self.h * self.h))
        p = lam * n
        self.R[k2] = from_rotvec(I2 @ p) @ self.R[k2]
        self.R[k1] = from_rotvec(-(I1 @ p)) @ self.R[k1]

    def _joint_pos(self, j):
        kp, kc = j["parent"], j["child"]
        r1 = self.R[kp] @ j["rp"]
        r2 = self.R[kc] @ j["rc"]
        d = (self.x[kc] + r2) - (self.x[kp] + r1)
        self._pos(kp, r1, kc, r2, d)

    def _joint_turn(self, j, drive=True):
        kp, kc = j["parent"], j["child"]
        Rp, Rc = self.R[kp], self.R[kc]
        R0 = j["R0"]
        Rrel = R0.T @ Rp.T @ Rc
        if j["fam"] is None:                         # (a weld: no turn at all)
            Rn = np.eye(3)
        else:
            tone = self.tone[kc](self.t) if drive else 0.0
            if tone > 1e-3:
                k = HOLD[j["fam"]] * tone
                E = Rp @ R0 @ self.target[kc] @ Rrel.T @ R0.T @ Rp.T
                self._ang(kp, kc, rotvec(E), alpha=1.0 / k)
                Rp, Rc = self.R[kp], self.R[kc]
                Rrel = R0.T @ Rp.T @ Rc
            Rn = clamp_joint(Rrel, j)
            if Rn is None:
                return
        E = Rp @ R0 @ Rn @ Rc.T
        self._ang(kp, kc, rotvec(E))

    def _settle(self, n):
        """Before the fall: his pose put inside every joint's range, the joints closed (no speed comes of it)."""
        for _ in range(n):
            for j in self.joints:
                self._joint_turn_static(j)
            for j in self.joints:
                self._joint_pos(j)

    def _carry(self):
        """The rifle in his right hand: it goes where the hand takes it (and takes nothing from the hand)."""
        wd = self.weld
        hr, wk = wd["parent"], wd["child"]
        self.R[wk] = self.R[hr] @ wd["R0"]
        piv = self.x[hr] + self.R[hr] @ wd["rp"]
        self.x[wk] = piv - self.R[wk] @ wd["rc"]

    def _joint_turn_static(self, j):
        kp, kc = j["parent"], j["child"]
        Rrel = self.R[kp].T @ self.R[kc]
        Rn = clamp_joint(Rrel, j)
        if Rn is not None:
            self.R[kc] = self.R[kp] @ Rn

    def _ground(self):
        rd = self.rd
        P = self.x[rd.sp_part] + np.einsum("nij,nj->ni", self.R[rd.sp_part], rd.sp_off)
        pen = rd.sp_r - P[:, 2]
        self.lam_n[:] = 0.0
        for i in np.nonzero(pen > 0)[0]:
            k = rd.sp_part[i]
            r = self.R[k] @ rd.sp_off[i]
            p = self.x[k] + r
            pz = rd.sp_r[i] - p[2]
            if pz <= 0:
                continue
            lam = self._pos(k, r, -1, None, np.array((0.0, 0.0, pz)))
            self.lam_n[i] = lam
            # static friction: hold the point where it was if the ground can
            r = self.R[k] @ rd.sp_off[i]
            p = self.x[k] + r
            q = self.xp[k] + self.Rp[k] @ rd.sp_off[i]
            dt_ = (p - q)
            dt_[2] = 0.0
            if np.linalg.norm(dt_) < self.mu_s * pz:
                self._pos(k, r, -1, None, -dt_)
            if k not in self.contact_t:
                self.contact_t[k] = self.t
                self.contact_v[k] = float(-self.v[k][2])

    def _self(self):
        rd = self.rd
        if not len(self.pairs):
            return
        i, j = self.pairs[:, 0], self.pairs[:, 1]
        ki, kj = rd.sp_part[i], rd.sp_part[j]
        Pi = self.x[ki] + np.einsum("nij,nj->ni", self.R[ki], rd.sp_off[i])
        Pj = self.x[kj] + np.einsum("nij,nj->ni", self.R[kj], rd.sp_off[j])
        d = Pj - Pi
        dist = np.linalg.norm(d, axis=1)
        pen = self.gap - dist
        for a in np.nonzero(pen > 0)[0]:
            n = d[a] / max(dist[a], 1e-9)
            r1 = self.R[ki[a]] @ rd.sp_off[i[a]]
            r2 = self.R[kj[a]] @ rd.sp_off[j[a]]
            self._pos(ki[a], r1, kj[a], r2, -n * pen[a])

    # -------------------------------------------------------------------------------------------- the step
    def step(self):
        rd, h = self.rd, self.h
        for (tp, k, dv) in [p for p in self.push if p[0] <= self.t]:
            self.v[k] += dv
        self.push = [p for p in self.push if p[0] > self.t]
        if self.held and self.t >= self.release_at and not rd.unarmed:   # (an unarmed man has nothing to let go)
            self.held = False
            wk = rd.pix["weapon"]
            self.v[wk] += self.throw
            self.w[wk] += self.throw_w
            # (out of the ground first, where his hand had pressed it, without a kick)
            sel = rd.sp_part == wk
            P = self.x[rd.sp_part] + np.einsum("nij,nj->ni", self.R[rd.sp_part], rd.sp_off)
            pw = float(np.max(rd.sp_r[sel] - P[sel, 2]))
            if pw > 0.0:
                self.x[wk, 2] += pw
                P[sel, 2] += pw
            # (then it stays out of what it was touching where it now is: a pair is only kept apart from here on if
            # it starts apart, or the lift would have put it inside his arm and thrown it off at speed)
            core = np.nonzero(rd.sp_core)[0]
            wc = [i for i in core if rd.sp_part[i] == wk]
            add = []
            for i in wc:
                for j in core:
                    if rd.sp_part[j] == wk:
                        continue
                    if np.linalg.norm(P[i] - P[j]) > (rd.sp_r[i] + rd.sp_r[j]) * 1.05:
                        add.append((i, j))
            add = np.array(add, int).reshape(-1, 2)
            self.pairs = np.concatenate([self.pairs, add])
            self.gap = np.concatenate([self.gap, rd.sp_r[add[:, 0]] + rd.sp_r[add[:, 1]]])
        self.xp, self.Rp = self.x.copy(), self.R.copy()
        self.v[:, 2] += G[2] * h
        self.x = self.x + self.v * h
        self.R = np.array([from_rotvec(self.w[k] * h) @ self.R[k] for k in range(len(self.R))])
        for j in self.joints:
            self._joint_turn(j)
        for j in self.joints:
            self._joint_pos(j)
        self._self()
        self._ground()
        for j in self.joints:
            self._joint_pos(j)
        # (the ranges last: the clip is made of the parts' turns, so a joint a millimetre apart never shows, a
        # wrist turned past its range does)
        for j in self.joints:
            self._joint_turn(j, drive=False)
        if self.held:
            self._carry()
        # velocities
        self.v = (self.x - self.xp) / h
        self.w = np.array([rotvec(self.R[k] @ self.Rp[k].T) / h for k in range(len(self.R))])
        # dynamic friction where he lies on the ground
        for i in np.nonzero(self.lam_n > 0)[0]:
            k = rd.sp_part[i]
            r = self.R[k] @ rd.sp_off[i]
            vc = self.v[k] + np.cross(self.w[k], r)
            vt = vc.copy()
            vt[2] = 0.0
            s = np.linalg.norm(vt)
            if s < 1e-6:
                continue
            dv = -vt / s * min(h * self.mu_d * abs(self.lam_n[i]) / (h * h), s)
            n = dv / np.linalg.norm(dv)
            I = self._Iinv(k)
            a = np.cross(r, n)
            wgen = 1.0 / rd.mass[k] + a @ I @ a
            p = dv / wgen
            self.v[k] += p / rd.mass[k]
            self.w[k] += I @ np.cross(r, p)
        # joint damping: the joints' own friction, more while there is tone in him
        for j in self.joints:
            kp, kc = j["parent"], j["child"]
            dmp = DAMP_DEAD_OF.get(j["fam"], DAMP_DEAD) + DAMP_TONE * self.tone[kc](self.t)
            rel = self.w[kc] - self.w[kp]
            Ip, Ic = self._Iinv(kp), self._Iinv(kc)
            n = np.linalg.norm(rel)
            if n < 1e-9:
                continue
            u = rel / n
            wp, wc = u @ Ip @ u, u @ Ic @ u
            dw = rel * (1.0 - math.exp(-dmp * h))
            self.w[kc] -= dw * wc / (wp + wc)
            self.w[kp] += dw * wp / (wp + wc)
        self.t += h
        # (once he has all but stopped, the cloth and the soil take the last of it: a body on the ground does not
        # rock for seconds)
        lin = float(np.max(np.linalg.norm(self.v, axis=1)))
        ang = float(np.max(np.linalg.norm(self.w, axis=1)))
        down = self.x[rd.pix["chest"], 2] < SETTLE_LOW          # (only once he is down: a man slumped sitting
        self.slow = self.slow + h if (self.t > 0.6 and lin < 0.25 and (not self.held or rd.unarmed) and down) else 0.0  # topples)
        if self.slow > 0.1:
            k = math.exp(-SETTLE_DAMP * h * min(1.0, (self.slow - 0.1) / 0.3))
            self.v *= k
            self.w *= k
        self.still = self.still + h if (lin < 0.03 and ang < 0.35) else 0.0

    def run(self, t_max=3.5, t_min=0.9, settle=0.3, on_step=None):
        every = max(1, int(round(self.hz / self.fps)))
        n = int(round(self.t * self.hz))                 # (run again to go on from where it stopped)
        if not self.rec:
            self.rec.append((0.0, self.R.copy(), self.heads()))
        while self.t < t_max - 1e-9:
            if on_step:
                on_step(self)
            self.step()
            n += 1
            if n % every == 0:
                self.rec.append((round(n / self.hz, 5), self.R.copy(), self.heads()))
                if self.t >= t_min and self.still >= settle and not self.held:
                    break
        return self
