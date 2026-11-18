"""A human range-of-motion check for our rig: every joint of every frame of a clip, measured against what a real body
can do, the worst of each reported with its frame. numpy only (importing it renders nothing and needs no Blender,
except to read a Blender action).

Why: Brian (18 November 2026), on the soldiers' arms: "a real body could never move like that". Our clips are
retargeted capture, IK and hand keys; none of them is safe from an arm going where a shoulder cannot. This measures
it instead of looking at it.

    import rom, mocap
    rep = rom.check(mocap.motion_of_clip("walk"))               # a mocap.Motion (clips.json clip, carry_rifle output)
    rep = rom.check_action(armature, action)                     # a Blender action on a human.build rig (needs bpy)
    rep = rom.check_states(states, rig, fps=60)                  # soldier.py: a list of Rig.solve / clip_state /
                                                                 # Performance.state dicts ({"D": world turns from
                                                                 # rest, "H": heads}), the rig its Rig (H0, hand0)
    print(rom.text(rep))                                         # the faults, worst first
    rom.table({"walk": rep, ...})                                # markdown: per clip, the worst of every measure
    python rom.py [clip ...] [--md out.md] [--json out.json]     # clips.json raw and with the soldier's carries

THE RIG. human.py's 27 bones (root, pelvis, spine_01, spine_02, chest, neck, head; clavicle, upperarm, lowerarm, hand,
thumb, fingers a side; thigh, calf, foot, ball a side), or the soldier's rig built on the same joints (its weapon,
bolt and bipod bones are not body and are left out). Everything is read from world turns (each bone's rotation from
its rest, D) and joint positions (heads), so the bones' rolls do not matter. The anatomical frames:
  thorax  the chest bone's turn (at rest: x forward, y left, z up). The arms are measured in it, the right arm
          mirrored, so both sides read the same (y out to that side).
  pelvis  the pelvis bone's turn. The hips are measured in it.
THE MEASURES (degrees unless said):
  shoulder   the humerus (shoulder to elbow) in the thorax: elevation (0 hanging, 90 level, 180 overhead: flexion
             and abduction both); behind the body's frontal plane (extension with the arm low, horizontal abduction
             with it level; the limit goes from 50 to 30 between 60 and 90 of elevation); horizontal adduction (90
             plus the angle across the midline); axial rotation, internal +, from the hanging arm with the elbow's
             hinge square to the body (the forearm pointing forward when bent) carried to the humerus's direction by
             the shortest swing (so with the arm raised forward the neutral forearm points up, raised to the side it
             points forward); read off the elbow's hinge, so it holds for a straight arm too. Abduction past 90
             needs external rotation: past 90 in the side half with the arm internally rotated is a fault, sized by
             the smaller of the two.
  elbow      flexion (0 straight; below 0 is hyperextension), the forearm off its hinge plane (varus/valgus), the
             way its point goes (the bisector away from the bend): up more than 30 over the thorax's level is
             flagged, and where the elbow itself is: into the torso (cm past the skin, its own 3.5 cm of flesh
             counted) or behind the back (cm behind the back's surface, within the torso's width).
  forearm    pronation + / supination -, from mid-position (palm square to the elbow's hinge, facing in: thumb up
             with the forearm forward); all the twist between the elbow's hinge and the palm is counted here.
  wrist      flexion + / extension -, radial deviation + / ulnar -: the hand's axis (wrist to the middle knuckle)
             against the forearm, in the hand's own frame.
  clavicle   elevation and protraction (+) / retraction (-) of the sternoclavicular line, from its rest, in the thorax.
  hip        the femur (hip to knee) in the pelvis: flexion + / extension -, abduction + / adduction -, axial
             rotation internal + (the kneecap from forward, carried as the shoulder's).
  knee       flexion, hyperextension below 0, off its hinge.  ankle  dorsiflexion + / plantarflexion -, from rest
             (standing flat-footed), about the shin's hinge.
  spine      pelvis to chest: flexion + / extension -, lateral bend, twist.  neck  chest to head, the same.
  speed      each joint's turn (a bone against its parent) from one frame to the next, as degrees a second.
             clips.json's keys are 30 a second, played with linear interpolation, so key to key is the same speed
             as between 60 fps frames. A flip or a snap shows here.
LIMITS: the coordinator's sheet (18 November 2026) where it gives one; the rest (marked "mine" in LIMITS) are common
clinical ranges, there so a broken spine or neck or a wrung knee shows too.
"""

import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import mocap  # noqa: E402  (numpy only at import)

CORE = ["root", "pelvis", "spine_01", "spine_02", "chest", "neck", "head",
        "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "thumb_l", "fingers_l",
        "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "thumb_r", "fingers_r",
        "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r"]
PARENT = {"pelvis": "root", "spine_01": "pelvis", "spine_02": "spine_01", "chest": "spine_02", "neck": "chest",
          "head": "neck", "thigh_l": "pelvis", "thigh_r": "pelvis"}
for _s in ("l", "r"):
    PARENT.update({f"clavicle_{_s}": "chest", f"upperarm_{_s}": f"clavicle_{_s}", f"lowerarm_{_s}": f"upperarm_{_s}",
                   f"hand_{_s}": f"lowerarm_{_s}", f"thumb_{_s}": f"hand_{_s}", f"fingers_{_s}": f"hand_{_s}",
                   f"calf_{_s}": f"thigh_{_s}", f"foot_{_s}": f"calf_{_s}", f"ball_{_s}": f"foot_{_s}"})
JOINT = {"pelvis": "pelvis (the body's turn)", "spine_01": "lower back", "spine_02": "mid back", "chest": "upper back",
         "neck": "neck (base)", "head": "neck (top)", "clavicle": "clavicle", "upperarm": "shoulder",
         "lowerarm": "elbow", "hand": "wrist", "thumb": "thumb", "fingers": "fingers", "thigh": "hip", "calf": "knee",
         "foot": "ankle", "ball": "toes"}

# The limits, each a measure that must stay at or under it (degrees; cm for the elbow's place; degrees a second for
# speed). key: (label, measure, sign, limit, mine). `sign` -1 reads the measure's negative side (extension etc.).
# The shoulder's "behind" limit depends on elevation (see behind_limit).
CHECKS = [
    ("sh_elev", "shoulder flexion/abduction (elevation)", "sh_elev", 1, 170.0, False),
    ("sh_behind", "shoulder extension / horizontal abduction (behind the body)", "sh_behind", 1, None, False),
    ("sh_hadd", "shoulder horizontal adduction", "sh_hadd", 1, 130.0, False),
    ("sh_int", "shoulder internal rotation", "sh_rot", 1, 70.0, False),
    ("sh_ext", "shoulder external rotation", "sh_rot", -1, 90.0, False),
    ("sh_abd90", "abduction past 90 without external rotation", "sh_abd90", 1, 0.0, False),
    ("el_flex", "elbow flexion", "el_flex", 1, 145.0, False),
    ("el_hyper", "elbow hyperextension", "el_flex", -1, 0.0, False),
    ("el_hinge", "elbow off its hinge (varus/valgus)", "el_hinge", 1, 20.0, True),
    ("el_up", "elbow pointing up (over the level)", "el_up", 1, 30.0, True),
    ("el_torso", "elbow into the torso (cm)", "el_torso", 1, 0.0, False),
    ("el_back", "elbow behind the back (cm)", "el_back", 1, 0.0, False),
    ("pron", "forearm pronation", "pron", 1, 80.0, False),
    ("sup", "forearm supination", "pron", -1, 85.0, False),
    ("wr_flex", "wrist flexion", "wr_flex", 1, 75.0, False),
    ("wr_ext", "wrist extension", "wr_flex", -1, 65.0, False),
    ("wr_rad", "wrist radial deviation", "wr_dev", 1, 20.0, False),
    ("wr_uln", "wrist ulnar deviation", "wr_dev", -1, 30.0, False),
    ("cl_elev", "clavicle elevation", "cl_elev", 1, 30.0, False),
    ("cl_dep", "clavicle depression", "cl_elev", -1, 10.0, True),
    ("cl_pro", "clavicle protraction", "cl_pro", 1, 20.0, False),
    ("cl_ret", "clavicle retraction", "cl_pro", -1, 20.0, False),
    ("hip_flex", "hip flexion", "hip_flex", 1, 125.0, False),
    ("hip_ext", "hip extension", "hip_flex", -1, 25.0, False),
    ("hip_abd", "hip abduction", "hip_abd", 1, 45.0, False),
    ("hip_add", "hip adduction", "hip_abd", -1, 30.0, True),
    ("hip_int", "hip internal rotation", "hip_rot", 1, 45.0, True),
    ("hip_ext_rot", "hip external rotation", "hip_rot", -1, 50.0, True),
    ("kn_flex", "knee flexion", "kn_flex", 1, 145.0, False),
    ("kn_hyper", "knee hyperextension", "kn_flex", -1, 0.0, False),
    ("kn_hinge", "knee off its hinge (varus/valgus)", "kn_hinge", 1, 15.0, True),
    ("an_dorsi", "ankle dorsiflexion", "an_dorsi", 1, 25.0, False),
    ("an_plant", "ankle plantarflexion", "an_dorsi", -1, 50.0, False),
    ("sp_flex", "spine flexion (pelvis to chest)", "sp_flex", 1, 80.0, True),
    ("sp_ext", "spine extension", "sp_flex", -1, 30.0, True),
    ("sp_lat", "spine lateral bend", "sp_lat", 1, 35.0, True),
    ("sp_rot", "spine rotation", "sp_rot", 1, 45.0, True),
    ("nk_flex", "neck flexion (chest to head)", "nk_flex", 1, 50.0, True),
    ("nk_ext", "neck extension", "nk_flex", -1, 60.0, True),
    ("nk_lat", "neck lateral bend", "nk_lat", 1, 45.0, True),
    ("nk_rot", "neck rotation", "nk_rot", 1, 80.0, True),
]
SIDED = {"sh_", "el_", "pron", "sup", "wr_", "cl_", "hip", "kn_", "an_"}
SPEED = 900.0               # degrees a second, any joint
TOL = 1.0                   # degrees (or 1 cm) of slack before a measure counts as over (rounding, skin)
ELBOW_R = 0.035             # the elbow's own flesh (m), for the torso tests

# The torso, measured on the reference man's body (the points weighted to pelvis, spines and chest), as slabs:
# (bone it rides, z from, z to, x back, x front, half width), in the rest man's frame. Lower slabs ride the lower bones.
TORSO = [("pelvis", 0.90, 1.05, -0.095, 0.130, 0.154), ("spine_01", 1.05, 1.15, -0.064, 0.132, 0.142),
         ("spine_02", 1.15, 1.25, -0.076, 0.136, 0.154), ("chest", 1.25, 1.35, -0.086, 0.145, 0.191),
         ("chest", 1.35, 1.48, -0.087, 0.134, 0.200)]
TORSO_REF = {"pelvis": (-0.006, 0.0, 0.987), "spine_01": (-0.028, 0.0, 1.055), "spine_02": (-0.018, 0.0, 1.118),
             "chest": (-0.049, 0.0, 1.316)}


def behind_limit(elev):
    """How far behind the body's frontal plane the humerus may go: extension 50 with the arm low, horizontal
    abduction 30 with it level or higher, a blend between 60 and 90 of elevation."""
    return 50.0 - 20.0 * np.clip((np.asarray(elev) - 60.0) / 30.0, 0.0, 1.0)


# ------------------------------------------------------------------------------------------------ small maths
def _n(v):
    v = np.asarray(v, float)
    return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-12)


def _dot(a, b):
    return np.sum(a * b, axis=-1)


def _deg(x):
    return np.degrees(x)


def _asin(x):
    return np.arcsin(np.clip(x, -1.0, 1.0))


def _mv(M, v):
    """(F, 3, 3) @ (F, 3) or (3,)."""
    v = np.asarray(v, float)
    return np.einsum("fij,fj->fi", M, np.broadcast_to(v, M.shape[:-1]))


def _signed(a, b, axis):
    """Signed angle (deg) from a to b about axis (all (F, 3)); a and b are taken square to the axis."""
    a = _n(a - axis * _dot(a, axis)[:, None])
    b = _n(b - axis * _dot(b, axis)[:, None])
    return _deg(np.arctan2(_dot(np.cross(a, b), axis), _dot(a, b)))


def _carried_ref(d0, ref0, d):
    """The reference ref0 (square to d0) carried along the shortest swing from d0 to d (F, 3)."""
    R = mocap.swing(np.broadcast_to(d0, d.shape), d)
    return np.einsum("fij,j->fi", R, ref0)


# ------------------------------------------------------------------------------------------------ the rig at rest
class Body:
    """What the measures need of a rig at rest: its joints (heads), each hand's axis and palm (world, rest), the
    elbows' hinges and the torso's slabs, all in the rest man's frame (x forward, y left, z up)."""

    def __init__(self, H0, hands):
        self.H0 = {n: np.asarray(H0[n], float) for n in CORE if n in H0}
        self.hands = {}
        self.hinge = {}
        for s in ("l", "r"):
            M = np.diag((1.0, 1.0 if s == "l" else -1.0, 1.0))
            a, n = (np.asarray(v, float) for v in hands[s])
            n = _n(n - a * (a @ n))
            self.hands[s] = (_n(a), n)
            u0 = M @ (self.H0["lowerarm_" + s] - self.H0["upperarm_" + s])
            l0 = M @ (self.H0["hand_" + s] - self.H0["lowerarm_" + s])
            self.hinge[s] = M @ _n(np.cross(u0, l0))            # (world, rest; rides the upper arm)
        self.torso = []
        for b, z0, z1, xb, xf, hw in TORSO:
            off = self.H0[b] - np.array(TORSO_REF[b])          # (another man: his bone heads carry the slabs)
            self.torso.append((b, z0 + off[2], z1 + off[2], xb + off[0], xf + off[0], hw))

    @classmethod
    def of_rest(cls, rest=None):
        """From a mocap.Rest (human.build's rig); the reference man's by default."""
        rest = rest or mocap.Rest.load()
        hands = {}
        for s in ("l", "r"):
            h = rest.i("hand_" + s)
            mir = np.array((1.0, 1.0, 1.0)) if s == "l" else np.array((-1.0, 1.0, 1.0))
            hands[s] = (rest.R[h] @ (np.array(mocap.HAND_AXIS) * mir), rest.R[h] @ (np.array(mocap.PALM_N) * mir))
        return cls({n: rest.head[rest.i(n)] for n in rest.names}, hands)

    @classmethod
    def of_rig(cls, rig):
        """From soldier.py's Rig (its H0 heads and hand0 = (axis, toward the thumb, palm normal) per hand)."""
        return cls(rig.H0, {s: (rig.hand0[s][0], rig.hand0[s][2]) for s in ("l", "r")})


# ------------------------------------------------------------------------------------------------ the poses
class Poses:
    """A clip as arrays: the core bones' world turns from rest D (F, B, 3, 3) and heads H (F, B, 3), at fps."""

    def __init__(self, D, H, fps, names=CORE, frames=None):
        self.names = list(names)
        self.ix = {n: i for i, n in enumerate(self.names)}
        self.D, self.H, self.fps = np.asarray(D, float), np.asarray(H, float), float(fps)
        self.frames = np.arange(len(self.D)) if frames is None else np.asarray(frames)

    def __len__(self):
        return len(self.D)

    def d(self, n):
        return self.D[:, self.ix[n]]

    def h(self, n):
        return self.H[:, self.ix[n]]

    @classmethod
    def of_motion(cls, m):
        r = m.rest
        keep = [n for n in CORE if n in r.ix]
        ix = [r.i(n) for n in keep]
        D = m.W[:, ix] @ np.swapaxes(r.R[ix], -1, -2)
        H = m.heads()[:, ix]
        return cls(D, H, m.fps, keep, getattr(m, "frames", None))

    @classmethod
    def of_states(cls, states, fps=60.0, frames=None):
        keep = [n for n in CORE if n in states[0]["D"]]
        D = np.array([[st["D"][n] for n in keep] for st in states])
        H = np.array([[st["H"][n] for n in keep] for st in states])
        return cls(D, H, fps, keep, frames)


# ------------------------------------------------------------------------------------------------ the measures
def measure(body, P):
    """Every measure, per frame: {key: (F,)} (keys ending _l/_r for a side), degrees (cm for el_torso/el_back)."""
    out = {}
    F = len(P)
    Dch, Dpv = P.d("chest"), P.d("pelvis")
    down = np.array((0.0, 0.0, -1.0))
    for s in ("l", "r"):
        M = np.diag((1.0, 1.0 if s == "l" else -1.0, 1.0))
        S = M[None] @ np.swapaxes(Dch, -1, -2)                       # world -> thorax, this side's terms
        sh, el, wr = P.h("upperarm_" + s), P.h("lowerarm_" + s), P.h("hand_" + s)
        u = _n(_mv(S, el - sh))
        lo = _n(_mv(S, wr - el))
        # shoulder
        elev = _deg(np.arccos(np.clip(-u[:, 2], -1, 1)))
        out["sh_elev_" + s] = elev
        out["sh_behind_" + s] = _deg(_asin(-u[:, 0]))
        out["sh_hadd_" + s] = 90.0 + _deg(_asin(-u[:, 1]))
        k = _n(_mv(S, _mv(P.d("upperarm_" + s), body.hinge[s])))     # the elbow's hinge now (thorax, side)
        ref = _carried_ref(down, np.array((0.0, -1.0, 0.0)), u)
        rot = _signed(ref, k, u)                                      # internal +
        out["sh_rot_" + s] = rot
        plane = _deg(np.arctan2(u[:, 0], u[:, 1]))                   # 0 out to the side, 90 forward
        out["sh_abd90_" + s] = np.where((elev > 90.0) & (plane < 45.0), np.minimum(elev - 90.0, rot), np.nan)
        # elbow
        flex = _deg(np.arctan2(_dot(np.cross(u, lo), k), _dot(u, lo)))
        out["el_flex_" + s] = flex
        out["el_hinge_" + s] = np.abs(_deg(_asin(_dot(lo, k))))
        apex = _n(u - lo)
        out["el_up_" + s] = np.where(flex > 30.0, _deg(_asin(apex[:, 2])), np.nan)
        torso, back = _elbow_place(body, P, el)
        out["el_torso_" + s], out["el_back_" + s] = torso, back
        # forearm and wrist
        a0, n0 = body.hands[s]
        a = _n(_mv(S, _mv(P.d("hand_" + s), a0)))
        n = _n(_mv(S, _mv(P.d("hand_" + s), n0)))
        out["pron_" + s] = _signed(k, n, lo)                          # pronation +
        t = np.cross(n, a)                                            # toward the thumb (this side's terms)
        out["wr_flex_" + s] = _deg(np.arctan2(-_dot(lo, n), _dot(lo, a)))
        out["wr_dev_" + s] = _deg(np.arctan2(-_dot(lo, t), _dot(lo, a)))
        # clavicle
        c = _n(_mv(S, P.h("upperarm_" + s) - P.h("clavicle_" + s)))
        c0 = _n(M @ (body.H0["upperarm_" + s] - body.H0["clavicle_" + s]))
        out["cl_elev_" + s] = _deg(_asin(c[:, 2])) - math.degrees(math.asin(c0[2]))
        out["cl_pro_" + s] = _deg(np.arctan2(c[:, 0], c[:, 1])) - math.degrees(math.atan2(c0[0], c0[1]))
        # hip, knee, ankle (in the pelvis, this side's terms)
        Sp = M[None] @ np.swapaxes(Dpv, -1, -2)
        th, ca, ft, bl = (P.h(b + s) for b in ("thigh_", "calf_", "foot_", "ball_"))
        f = _n(_mv(Sp, ca - th))
        sn = _n(_mv(Sp, ft - ca))
        out["hip_flex_" + s] = _deg(np.arctan2(f[:, 0], -f[:, 2]))
        out["hip_abd_" + s] = _deg(np.arctan2(f[:, 1], np.hypot(f[:, 0], f[:, 2])))
        kc = _n(_mv(Sp, _mv(P.d("thigh_" + s), M @ np.array((1.0, 0.0, 0.0)))))   # the kneecap's way
        out["hip_rot_" + s] = _signed(_carried_ref(down, np.array((1.0, 0.0, 0.0)), f), kc, f)
        kh = _n(_mv(Sp, _mv(P.d("thigh_" + s), M @ np.array((0.0, 1.0, 0.0)))))   # the knee's hinge
        out["kn_flex_" + s] = _deg(np.arctan2(_dot(np.cross(f, sn), kh), _dot(f, sn)))
        s0_ = _n(M @ (body.H0["foot_" + s] - body.H0["calf_" + s]))
        rest_off = math.degrees(math.asin(float(s0_ @ np.array((0.0, 1.0, 0.0)))))   # (its rest: the leg's own slant)
        out["kn_hinge_" + s] = np.abs(_deg(_asin(_dot(sn, kh))) - rest_off)
        fr = M @ np.einsum("fji,fj->fi", P.d("calf_" + s), bl - ft).T  # the foot in the shin's rest frame
        f0 = M @ (body.H0["ball_" + s] - body.H0["foot_" + s])
        y = np.array((0.0, 1.0, 0.0))
        out["an_dorsi_" + s] = -_signed(np.broadcast_to(f0, (F, 3)), fr.T, np.broadcast_to(y, (F, 3)))
    # spine and neck
    for key, lo_b, hi_b in (("sp", "pelvis", "chest"), ("nk", "chest", "head")):
        R = np.swapaxes(P.d(lo_b), -1, -2) @ P.d(hi_b)
        up = R[:, :, 2]
        out[key + "_flex"] = _deg(np.arctan2(up[:, 0], up[:, 2]))
        out[key + "_lat"] = np.abs(_deg(_asin(up[:, 1])))
        q = mocap.mat_to_quat(R)
        out[key + "_rot"] = np.abs(_deg(2 * np.arctan2(q[:, 3], q[:, 0])))
    return out


def _elbow_place(body, P, el):
    """The elbow (world (F, 3)) against the torso: (cm into it past its flesh, cm behind the back within its width);
    nan where neither applies."""
    F = len(el)
    into = np.full(F, np.nan)
    back = np.full(F, np.nan)
    for b, z0, z1, xb, xf, hw in body.torso:
        q = np.einsum("fji,fj->fi", P.d(b), el - P.h(b)) + body.H0[b]     # the elbow in the rest man's frame
        inz = (q[:, 2] >= z0) & (q[:, 2] < z1)
        xc, ax = 0.5 * (xb + xf), 0.5 * (xf - xb)
        dx, dy = q[:, 0] - xc, q[:, 1]
        e = np.sqrt((dx / ax) ** 2 + (dy / hw) ** 2)
        dist = np.hypot(dx, dy) * (1.0 - 1.0 / np.maximum(e, 1e-9))      # past the surface (m, - inside)
        pen = (ELBOW_R - dist) * 100.0
        into = np.where(inz, np.fmax(into, pen), into)
        bk = (xb - q[:, 0]) * 100.0
        bk = np.where(np.abs(q[:, 1]) < hw * 0.9, bk, np.nan)
        back = np.where(inz, np.fmax(back, bk), back)
    return into, back


def speeds(P):
    """Each joint's turn rate (deg/s) from frame k to k+1: {child bone: (F-1,)}."""
    out = {}
    for n in P.names:
        p = PARENT.get(n)
        if p is None or p not in P.ix:
            continue
        R = np.swapaxes(P.d(p), -1, -2) @ P.d(n)
        dR = np.swapaxes(R[:-1], -1, -2) @ R[1:]
        tr = np.clip((np.trace(dR, axis1=1, axis2=2) - 1.0) / 2.0, -1.0, 1.0)
        out[n] = _deg(np.arccos(tr)) * P.fps
    return out


# ------------------------------------------------------------------------------------------------ the check
def check(src, body=None, fps=None, name=""):
    """The range-of-motion report of a clip: `src` a mocap.Motion or a Poses. Returns a dict:
    rows   one per check (and side): key, label, side, value (the worst, as the limit reads it), frame, t (s),
           limit, over (value - limit; > 0 is a fault), mine (the limit is not the coordinator's)
    speed  per joint: deg/s at worst, its frame (the step from that frame to the next), over 900 or not
    faults the rows over by more than TOL, worst first, and the joints over SPEED"""
    P = src if isinstance(src, Poses) else Poses.of_motion(src)
    if fps:
        P.fps = float(fps)
    body = body or (Body.of_rest(src.rest) if hasattr(src, "rest") else Body.of_rest())
    meas = measure(body, P)
    rows = []
    for key, label, mkey, sign, lim, mine in CHECKS:
        sides = ("l", "r") if any(mkey.startswith(p) for p in SIDED) else ("",)
        for s in sides:
            v = meas[mkey + ("_" + s if s else "")] * sign
            if key == "sh_behind":
                L = behind_limit(meas["sh_elev_" + s])
            else:
                L = np.full(len(v), lim)
            over = v - L
            if np.all(np.isnan(over)):
                continue
            f = int(np.nanargmax(over))
            rows.append({"key": key, "label": label, "side": s, "value": round(float(v[f]), 1),
                         "frame": int(P.frames[f]),
                         "t": round(float(P.frames[f]) / P.fps, 3), "limit": round(float(L[f]), 1),
                         "over": round(float(over[f]), 1), "mine": mine})
    sp = {}
    for n, w in speeds(P).items():
        f = int(np.argmax(w))
        sp[n] = {"deg_s": round(float(w[f])), "frame": int(P.frames[f]), "t": round(float(P.frames[f]) / P.fps, 3),
                 "over": bool(w[f] > SPEED)}
    faults = sorted((r for r in rows if r["over"] > TOL), key=lambda r: -r["over"])
    fast = sorted(((n, d) for n, d in sp.items() if d["over"]), key=lambda x: -x[1]["deg_s"])
    return {"name": name, "frames": len(P), "fps": P.fps, "rows": rows, "speed": sp, "faults": faults, "fast": fast,
            "series": meas}


def check_action(armature, action, rest=None, name=None):
    """A Blender action on a human.build armature (needs bpy)."""
    m = mocap.motion_of_action(armature, action, rest)
    return check(m, name=name or action.name)


def check_states(states, rig=None, fps=60.0, name="", frames=None):
    """soldier.py states (dicts with "D" world turns from rest and "H" heads), one per frame at fps, on `rig`
    (soldier.Rig; its H0 and hand0 give the rest). Without a rig, the reference man's rest is used."""
    body = Body.of_rig(rig) if rig is not None else Body.of_rest()
    return check(Poses.of_states(states, fps, frames), body=body, name=name)


def check_performance(perf, t0, t1, fps=60.0, name=""):
    """soldier.Performance from t0 to t1 (s), sampled at fps."""
    ts = np.arange(t0, t1 + 1e-9, 1.0 / fps)
    return check_states([perf.state(t) for t in ts], perf.rig, fps, name, frames=np.arange(len(ts)))


def side_name(s):
    return {"l": "left", "r": "right", "": ""}[s]


def text(rep, n=8):
    """The faults of one report, worst first, as lines."""
    out = []
    for r in rep["faults"][:n]:
        out.append(f"  {r['label']}{' ' + side_name(r['side']) if r['side'] else ''}: {r['value']:g} "
                   f"(limit {r['limit']:g}{', mine' if r['mine'] else ''}) at frame {r['frame']} ({r['t']:g} s)")
    for nm, d in rep["fast"][:n]:
        out.append(f"  {joint_name(nm)} turning {d['deg_s']} deg/s at frame {d['frame']} ({d['t']:g} s)")
    return "\n".join(out) or "  ok"


def joint_name(bone):
    base, _, s = bone.rpartition("_") if bone[-2:] in ("_l", "_r") else (bone, "", "")
    return JOINT.get(base, base) + (" " + side_name(s) if s else "")


# ------------------------------------------------------------------------------------------------ tables
COLS = [  # (column, check keys whose worst it shows)
    ("sh. elev ≤170", ["sh_elev"]), ("sh. behind ≤50/30", ["sh_behind"]), ("horiz. add. ≤130", ["sh_hadd"]),
    ("int. rot ≤70", ["sh_int"]), ("ext. rot ≤90", ["sh_ext"]), ("abd>90 w/o ER", ["sh_abd90"]),
    ("elbow ≤145", ["el_flex"]), ("hyperext ≤0", ["el_hyper"]), ("elbow up ≤30*", ["el_up"]),
    ("in torso cm", ["el_torso"]), ("behind back cm", ["el_back"]),
    ("pron ≤80", ["pron"]), ("sup ≤85", ["sup"]), ("wr flex ≤75", ["wr_flex"]), ("wr ext ≤65", ["wr_ext"]),
    ("radial ≤20", ["wr_rad"]), ("ulnar ≤30", ["wr_uln"]), ("clav elev ≤30", ["cl_elev"]),
    ("clav pro ≤20", ["cl_pro"]), ("clav ret ≤20", ["cl_ret"]),
]
COLS_BODY = [
    ("hip flex ≤125", ["hip_flex"]), ("hip ext ≤25", ["hip_ext"]), ("hip abd ≤45", ["hip_abd"]),
    ("hip add ≤30*", ["hip_add"]), ("hip rot ≤45/50*", ["hip_int", "hip_ext_rot"]),
    ("knee ≤145", ["kn_flex"]), ("knee hyper ≤0", ["kn_hyper"]), ("dorsi ≤25", ["an_dorsi"]),
    ("plantar ≤50", ["an_plant"]), ("spine*", ["sp_flex", "sp_ext", "sp_lat", "sp_rot"]),
    ("neck*", ["nk_flex", "nk_ext", "nk_lat", "nk_rot"]),
]


def _cell(rep, keys):
    rows = [r for r in rep["rows"] if r["key"] in keys]
    if not rows:
        return "–"
    r = max(rows, key=lambda r: r["over"] / max(abs(r["limit"]), 10.0))
    short = {"sp_flex": "fl", "sp_ext": "ex", "sp_lat": "lat", "sp_rot": "rot",
             "nk_flex": "fl", "nk_ext": "ex", "nk_lat": "lat", "nk_rot": "rot"}
    side = r["side"] or short.get(r["key"], "")
    if r["key"] in ("hip_int", "hip_ext_rot"):
        side = ("in " if r["key"] == "hip_int" else "ex ") + r["side"]
    v = f"{r['value']:.0f}" if r["key"] not in ("el_torso", "el_back") else f"{r['value']:.1f}"
    if r["key"] == "sh_behind" or (r["key"] in ("sp_flex", "sp_ext", "sp_lat", "sp_rot", "nk_flex", "nk_ext", "nk_lat",
                                                 "nk_rot")):
        v += f"/{r['limit']:.0f}"
    s = f"{v} {side}@{r['frame']}".replace("  ", " ")
    return f"**{s}**" if r["over"] > TOL else s


def table(reports, cols=None, title_col="clip"):
    """Markdown: one row per report, each cell the worst of that measure (value, side, @frame); over the limit in
    bold. reports: {name: check(...) result}."""
    cols = cols or COLS
    head = "| " + title_col + " | " + " | ".join(c for c, _ in cols) + " |"
    sep = "|---|" + "---|" * len(cols)
    lines = [head, sep]
    for name, rep in reports.items():
        lines.append(f"| {name} | " + " | ".join(_cell(rep, ks) for _, ks in cols) + " |")
    return "\n".join(lines)


def speed_table(reports):
    """Markdown: per report the fastest joint (deg/s, @frame) and every joint over SPEED."""
    lines = ["| clip | fastest joint | over 900°/s |", "|---|---|---|"]
    for name, rep in reports.items():
        n, d = max(rep["speed"].items(), key=lambda x: x[1]["deg_s"])
        fast = ", ".join(f"{joint_name(j)} {dd['deg_s']} @{dd['frame']}" for j, dd in rep["fast"]) or "none"
        lines.append(f"| {name} | {joint_name(n)} {d['deg_s']} @{d['frame']} | {fast} |")
    return "\n".join(lines)


# ------------------------------------------------------------------------------------------------ the clips
# The carries the soldier lays over the capture (soldier.MOCAP): clip, carry, until (s), blend (s).
CARRIES = [("idle", "port", None, None), ("walk", "port", None, None), ("run", "port", None, None),
           ("advance", "ready", None, None), ("creep", "ready", None, None), ("go_prone", "ready", 0.25, 0.45),
           ("fall_forward", "ready", 0.06, 0.3), ("fall_back", "ready", 0.04, 0.25)]


def run_all(names=None, carries=True):
    """Every clips.json clip as captured (raw), and with the soldier's carries: {name: report}."""
    clips = mocap.load_clips()
    rest = mocap.Rest.load()
    body = Body.of_rest(rest)
    raw, car = {}, {}
    for n in clips:
        if n.startswith("_") or (names and n not in names):
            continue
        raw[n] = check(mocap.motion_of_clip(n, rest, clips), body, name=n)
    if carries:
        for n, carry, until, blend in CARRIES:
            if names and n not in names:
                continue
            m, err = mocap.carried(n, carry, until, blend or 0.3, rest, clips)
            rep = check(m, body, name=f"{n}+{carry}")
            rep["hand_miss_cm"] = round(float(err.max()) * 100, 2)
            car[f"{n}+{carry}"] = rep
    return raw, car


def _strip(rep):
    return {k: v for k, v in rep.items() if k != "series"}


if __name__ == "__main__":
    args = sys.argv[1:]
    md = args[args.index("--md") + 1] if "--md" in args else None
    js = args[args.index("--json") + 1] if "--json" in args else None
    names = [a for a in args if not a.startswith("--") and a not in (md, js)] or None
    raw, car = run_all(names)
    for part, reps in (("raw", raw), ("carried", car)):
        for n, rep in reps.items():
            extra = f"  (hands miss up to {rep['hand_miss_cm']} cm)" if "hand_miss_cm" in rep else ""
            print(f"{part:8s} {n}: {len(rep['faults'])} faults, {len(rep['fast'])} fast{extra}")
            print(text(rep))
    if md:
        with open(md, "w") as f:
            f.write("### Carried (as the soldier plays them)\n\n" + table(car) + "\n\n" + table(car, COLS_BODY) + "\n\n"
                    + speed_table(car) + "\n\n### Raw clips\n\n" + table(raw) + "\n\n" + table(raw, COLS_BODY) + "\n\n"
                    + speed_table(raw) + "\n")
    if js:
        json.dump({"raw": {k: _strip(v) for k, v in raw.items()}, "carried": {k: _strip(v) for k, v in car.items()}},
                  open(js, "w"), indent=1)
