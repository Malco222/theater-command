"""Hit clips: what a soldier plays when he is hit or blown over but NOT killed. Keyed straight onto each infantry
file's own skeleton (his own bone lengths, kit and hold on his weapon), at JSON level, numpy only: nothing else in
the file is touched.

    hit_flinch_f/_b/_l/_r          0.3 s  standing, from and back to fire_stand's first pose. The letter is where the
    hit_flinch_kneel_f/_b/_l/_r    0.3 s  round came FROM in his own frame: _f from the front (he jolts back), _b from
    hit_flinch_run_f/_b/_l/_r      0.3 s  behind (jolts forward), _l from his left (jolts to his right), _r from his
                                          right. Kneeling: fire_kneel's first pose. At a run: the run clip from 0.0 to
                                          0.3 s, the legs running on with a break in the stride, trunk and arms jolted.
    hit_flinch_prone               0.5 s  lying, from and back to fire_prone's first pose: head down, shoulders
                                          hunched, the left hand off the rifle onto his helmet and back.
    knock_down_face / _back        0.7 s  a shell bursts close: thrown off his feet onto his front / onto his back,
                                          alive, the weapon kept in his right hand. Not in place (end_offset).
    down_face / down_back          2.0 s  loops: lying stunned where he landed (placed at the root as fire_prone is).
    get_up_face / get_up_back      1.6 s  from the down loop's first pose to idle's first pose.

    python3 hit_clips.py --build [--files GER,USA,...] [--src DIR] [--stage DIR]
        one bundle a file, DIR/soldier_<N>.hit_clips.json = {"clips": {...}, "about_hits": "...", "checks": {...}}
    python3 hit_clips.py --apply --src DIR --out DIR2 [--files ...] [--stage DIR]
        whole soldier files with the staged bundle merged in (clips added, `about_hits` added, nothing else touched)
    python3 hit_clips.py --check [--files ...] [--src DIR] [--stage DIR]
        the checks again on what is staged, read back from the baked keys
    python3 hit_clips.py --pictures [--files GER,USA,JAP] [--src DIR] [--stage DIR] [--pics DIR]
    python3 hit_clips.py --deliver [--content] [--files ...] [--stage DIR]
        (NOT RUN in this pass) merges the staged bundles into the shared soldier files, and with --content into the
        game's Content copies where they exist: each target read again just before its write, backed up first,
        written through a temporary file and os.replace, read back. A bundle goes only when every fault its checks
        list is accepted one by one in ALLOW (file, clip, fault, reason); any other fault keeps that file back.

Axes: metres; x forward, y right, z up. A turn about +y bends him forward, about +z turns him to his right, about +x
leans him to his left. Rotations are keyed [t, w, x, y, z], moves [t, x, y, z] in the parent's frame, as every clip's.
The output is the same bytes every run.
"""
import json
import math
import os
import re
import sys
import time

import numpy as np

sys.dont_write_bytecode = True          # (nothing is written beside the tools this one reads)
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gore  # noqa: E402
import field_clips as fc  # noqa: E402
from field_clips import R_axis, quat, frame_map, unit, sm  # noqa: E402

NAMES = ["CHN", "FRA", "GER", "GER_detail", "ITA", "ITA_desert", "JAP", "POL", "SOV", "SOV_snow", "SOV_winter", "UK",
         "USA", "USA_bar", "USA_bazooka", "USA_loader", "USA_medic", "USA_nco", "USA_para", "USA_radio"]
SRC = "/home/user/art/hits/src/"
STAGE = "/home/user/art/hits/stage/"
PICS = "/home/user/art/hits/pics/clips/"
BACKUP = "/home/user/art/hits/backup/"
SHARED = "/mnt/project-files/battle-models/land/soldier/"
CONTENT = "/home/user/theater-command/unreal/TheaterCommand/Content/Models/Soldiers/"
SET = "hit_clips"
TOOL = "hit_clips.py"

X_, Y_, Z_ = np.array((1.0, 0.0, 0.0)), np.array((0.0, 1.0, 0.0)), np.array((0.0, 0.0, 1.0))
GUN_BONES = ("weapon", "bolt", "bipod")
WHIP = ("whip_1", "whip_2", "whip_3", "whip_4")
ARM = {s: ("upperarm_" + s, "lowerarm_" + s, "hand_" + s) for s in "lr"}
LEG = {s: ("thigh_" + s, "calf_" + s, "foot_" + s) for s in "lr"}

# ---- the contract with the effects team: names, lengths, the pose each starts from and ends in
WAYS = ("f", "b", "l", "r")
FLINCH_LONG = 0.3
PRONE_LONG = 0.5
KNOCK_LONG = 0.7
DOWN_LONG = 2.0
UP_LONG = 1.6
FLINCH_PEAK = 0.085          # (s: the jolt's peak)
FLINCH_FPS = 30              # (the flinches and the knock-downs are keyed 30 a second: they are fast)
RATE = 15                    # (the loops and the get-ups 15 a second, as every clip)


# ================================================================================================ small maths
def Rz(d):
    return R_axis(Z_, d)


def Ry(d):
    return R_axis(Y_, d)


def Rx(d):
    return R_axis(X_, d)


def euler(p=0.0, r=0.0, y=0.0):
    """A turn from pitch (+ forward), roll (+ to his left) and yaw (+ to his right), degrees: yaw o pitch o roll."""
    return Rz(y) @ Ry(p) @ Rx(r)


def angle_of(R):
    q = quat(R)
    return math.degrees(2 * math.acos(min(1.0, abs(float(q[0])))))


def qslerp(Ra, Rb, u):
    """The turn u of the way from Ra to Rb (u may run a little past 0 or 1)."""
    qa, qb = quat(Ra), quat(Rb)
    if qa @ qb < 0:
        qb = -qb
    d = float(np.clip(qa @ qb, -1.0, 1.0))
    th = math.acos(d)
    if th < 1e-6:
        q = qa * (1 - u) + qb * u
    else:
        q = (math.sin((1 - u) * th) * qa + math.sin(u * th) * qb) / math.sin(th)
    return gore.qmat(q / np.linalg.norm(q))


def rpow(R, f):
    """The turn R taken f of the way from rest (f may be negative or over 1)."""
    q = quat(R)
    if q[0] < 0:
        q = -q
    ang = 2 * math.acos(min(1.0, float(q[0])))
    if ang < 1e-9:
        return np.eye(3)
    return R_axis(q[1:], math.degrees(ang * f))


def lean_of(D):
    """A world turn D read as (pitch + forward, side + to his left, twist + to his right), degrees."""
    v, f = D @ Z_, D @ X_
    return (math.degrees(math.atan2(v[0], v[2])), math.degrees(math.atan2(-v[1], v[2])),
            math.degrees(math.atan2(f[1], f[0])))


def times_at(L, fps, extra=()):
    ts = [k / fps for k in range(int(math.floor(L * fps + 1e-6)) + 1)]
    if ts[-1] < L - 1e-6:
        ts.append(L)
    ts = sorted(set(round(t, 4) for t in list(ts) + list(extra)))
    return ts


# ================================================================================================ the man
class Body:
    """One file's man: his rig (field_clips.Man), his near mesh for the ground and the pictures, what he carries."""

    def __init__(self, name, D):
        self.name, self.D = name, D
        self.man = man = fc.Man(D)
        self.nb = man.nb
        self.names = man.rig.names
        M = D["near"]
        self.P = np.array(M["points"], float).reshape(-1, 3)
        self.pa, self.pb = np.array(M["part"]), np.array(M["part2"])
        self.pw = np.array(M["w2"], float)
        self.T = np.array(M["triangles"]).reshape(-1, 3)
        self.paints = np.array(M["paints"])
        self.shade = np.array(M["shade"], float) if "shade" in M else np.ones(len(self.P))
        C = D["clips"]
        self.C = C
        self.weapon = C["idle"].get("weapon", "rifle")
        self.armed = self.weapon != "none" and len(man.weapon_pts) > 0
        b = man.b
        self.wi = b("weapon")
        self.kids = {i: [j for j in range(self.nb) if man.par[j] == i] for i in range(self.nb)}
        self.root_kids = [j for j in range(self.nb) if man.par[j] is not None and man.par[man.par[j]] is None]
        # (a prop on the bolt bone that rides his body, not the weapon: the loader's rocket, the radio man's handset)
        lq0, mv0 = man.locals_of(C["fire_kneel"], 0.0)
        self.bolt_prop = "bolt" in man.ix and float(np.linalg.norm(mv0[b("bolt")])) > 0.05
        gun = [b(n) for n in GUN_BONES if n in man.ix and not (n == "bolt" and self.bolt_prop)]
        whip = [b(n) for n in WHIP if n in man.ix]
        self.whip = whip
        pa = self.pa
        grp = lambda ns: np.isin(pa, [b(n) for n in ns if n in man.ix])
        self.is_gun = np.isin(pa, gun)
        self.is_whip = np.isin(pa, whip)
        Z = {"trunk": grp(("pelvis", "spine_01", "spine_02", "chest", "pouch_l", "pouch_r", "clavicle_l",
                           "clavicle_r") + (("bolt",) if self.bolt_prop else ())),
             "hips": grp(("pelvis", "pouch_l", "pouch_r")),
             "chest": grp(("spine_02", "chest", "clavicle_l", "clavicle_r") + (("bolt",) if self.bolt_prop else ())),
             "head": grp(("neck", "head")),
             "gun": self.is_gun}
        for s in "lr":
            Z["arm_" + s] = grp(("upperarm_" + s, "lowerarm_" + s, "hand_" + s, "fingers_" + s, "thumb_" + s))
            Z["fore_" + s] = grp(("lowerarm_" + s, "hand_" + s, "fingers_" + s, "thumb_" + s))
            Z["leg_" + s] = grp(("thigh_" + s, "calf_" + s, "foot_" + s, "ball_" + s))
            Z["shin_" + s] = grp(("calf_" + s, "foot_" + s, "ball_" + s))
        Z["body"] = ~self.is_gun & ~self.is_whip
        self.Z = Z
        # (the limbs' points far enough from their own joint to count as lying on the ground, not as the joint)
        H = man.H
        self.far = {}
        for s in "lr":
            self.far["arm_" + s] = Z["arm_" + s] & (np.linalg.norm(self.P - H[b("upperarm_" + s)], axis=1) > 0.14)
            self.far["leg_" + s] = Z["leg_" + s] & (np.linalg.norm(self.P - H[b("thigh_" + s)], axis=1) > 0.22)
        for s in "lr":
            near_knee = np.linalg.norm(self.P - H[b("calf_" + s)], axis=1) < 0.2
            Z["knee_" + s] = grp(("thigh_" + s, "calf_" + s)) & near_knee
            Z["foot_" + s] = grp(("foot_" + s, "ball_" + s))
            Z["hand_" + s] = grp(("hand_" + s, "fingers_" + s, "thumb_" + s))
        self.far["head"] = Z["head"] & (np.linalg.norm(self.P - H[b("neck")], axis=1) > 0.07)
        # (the helmet's crown in the head's frame: the top of what rides the head)
        hp = self.P[pa == b("head")]
        self.crown = np.array((np.median(hp[hp[:, 2] > hp[:, 2].max() - 0.03][:, 0]), 0.0, hp[:,
            2].max())) - H[b("head")]
        self.head_back = float(hp[:, 0].min() - H[b("head")][0])
        mz = [s for s in D["sockets"] if s["name"] == "socket_muzzle"]
        self.muzzle = (np.array(mz[0]["at"], float) - H[self.wi]) if mz else np.array((0.8, 0.0, 0.04))
        # (each elbow's hinge and each hand's axis and palm at rest: what the arms' ranges are read from)
        self.hinge0, self.hand0 = {}, {}
        for s, sg in (("l", -1.0), ("r", 1.0)):
            Ms = np.diag((1.0, sg, 1.0))
            u0, l0 = H[b("lowerarm_" + s)] - H[b("upperarm_" + s)], H[b("hand_" + s)] - H[b("lowerarm_" + s)]
            self.hinge0[s] = Ms @ unit(np.cross(Ms @ u0, Ms @ l0))
            a0 = unit(np.array(HAND_A) * (1.0, -sg, 1.0))
            n0 = np.array(HAND_N) * (1.0, -sg, 1.0)
            self.hand0[s] = (a0, unit(n0 - a0 * (a0 @ n0)))
        self.poses = {}
        # (his hands on the weapon as the aim and the carry have them)
        G, X = man.fk(*self.pose("fire_stand", 0.0))
        self.rel0 = {s: self.grip_rel(G, X, s) for s in "lr"}
        G, X = man.fk(*self.pose("idle", 0.0))
        self.rel_idle = {s: self.grip_rel(G, X, s) for s in "lr"}
        self.prop0 = prop_ref(self, G, X) if self.bolt_prop else None

    def pose(self, clip, t=0.0):
        """A clip's pose at t as locals (turn of each bone on its parent, its move), cached."""
        k = (clip, round(t, 5))
        if k not in self.poses:
            self.poses[k] = self.man.locals_of(self.C[clip], t)
        lq, mv = self.poses[k]
        return lq.copy(), mv.copy()

    def skin(self, G, X):
        return gore.skin(self.P, self.pa, self.pb, self.pw, G, X, self.man.H)

    def skin_of(self, lq, mv):
        return self.skin(*self.man.fk(lq, mv))

    def fist(self, G, X, s):
        return self.man.grip(G, X, s)

    def grip_rel(self, G, X, s):
        """Hand s as it lies on the weapon: its turn and its wrist's place in the weapon bone's frame."""
        h = self.man.b("hand_" + s)
        return G[self.wi].T @ G[h], G[self.wi].T @ (X[h] - X[self.wi])

    def gun_on(self, G, X, bone):
        """The weapon bone as it lies on `bone`: its turn and its head's place in that bone's frame."""
        i = self.man.b(bone)
        return G[i].T @ G[self.wi], G[i].T @ (X[self.wi] - X[i])


def set_world(man, lq, mv, bone, Gb, Xb=None, G=None, X=None):
    """Bone `bone` given the world turn Gb (and its head the place Xb, by its move)."""
    if G is None:
        G, X = man.fk(lq, mv)
    i = man.b(bone) if isinstance(bone, str) else bone
    p = man.par[i]
    lq[i] = G[p].T @ Gb
    if Xb is not None:
        mv[i] = G[p].T @ (np.asarray(Xb, float) - X[p]) - (man.H[i] - man.H[p])
    return lq, mv


def turn_world(man, lq, mv, bone, Rw):
    """Bone `bone` (and all it carries) turned by the world turn Rw about its own head."""
    G, X = man.fk(lq, mv)
    i = man.b(bone)
    lq[i] = G[man.par[i]].T @ Rw @ G[i]
    return lq


FOLD = 141.0                 # (degrees: the most this tool's own solving folds a knee or an elbow)


def ik2(man, lq, mv, chain, T, pole=None, end_R=None, share=0.0):
    """A limb (upper bone, lower bone, end bone: an arm to its hand, a leg to its foot) solved so the end bone's head
    is at T, its two bones turned the least from how they lie now, so a target already met changes nothing. `pole`:
    where its middle joint points (world), else where it points now. `end_R`: the end bone's world turn (else it keeps
    the one it has); the lower bone takes `share` of the end bone's twist about it, so a wrist does not wring.
    Returns how far the end is from T (m): not zero when T is out of his reach."""
    top, mid, end = (man.b(n) for n in chain)
    G, X = man.fk(lq, mv)
    S = X[top]
    u0, l0 = X[mid] - X[top], X[end] - X[mid]
    l1, l2 = float(np.linalg.norm(u0)), float(np.linalg.norm(l0))
    dv = np.asarray(T, float) - S
    dist = float(np.linalg.norm(dv))
    fold = math.sqrt(l1 * l1 + l2 * l2 - 2 * l1 * l2 * math.cos(math.radians(180.0 - FOLD)))
    dc = min(max(dist, abs(l1 - l2) + 1e-4, min(fold, float(np.linalg.norm(X[end] - S)))), (l1 + l2) * 0.9995)
    u = dv / max(dist, 1e-9)
    bend_ref = np.cross(u0, l0)
    cur = X[mid] - 0.5 * (X[top] + X[end])
    pl = cur if pole is None else np.asarray(pole, float)
    v = pl - u * (pl @ u)
    if np.linalg.norm(v) < 1e-6:
        v = cur - u * (cur @ u)
    if np.linalg.norm(v) < 1e-6:
        v = np.cross(u, bend_ref if np.linalg.norm(bend_ref) > 1e-9 else Y_)
    v = unit(v)
    if np.linalg.norm(bend_ref) < 1e-7:
        bend_ref = np.cross(u0, v)
    bend_ref = unit(bend_ref)
    ca = (l1 * l1 + dc * dc - l2 * l2) / (2 * l1 * dc)
    K = S + l1 * (u * ca + v * math.sqrt(max(0.0, 1 - ca * ca)))
    Tc = S + u * dc
    up, lo = K - S, Tc - K
    bd = np.cross(up, lo)
    bd = unit(bd) if np.linalg.norm(bd) > 1e-7 else bend_ref
    Gt = frame_map(u0, bend_ref, up, bd) @ G[top]
    Gm = frame_map(l0, bend_ref, lo, bd) @ G[mid]
    Ge = G[end] if end_R is None else np.asarray(end_R, float)
    if share > 0.0 and end_R is not None:
        ax = unit(lo)
        # (the end bone's twist about the lower bone, against how the two lay: the lower bone takes its share)
        t0 = Y_ if abs(ax @ Y_) < 0.9 else X_
        a = Gm @ (G[mid].T @ G[end]) @ (G[end].T @ t0)
        b = Ge @ (G[end].T @ t0)
        a, b = a - ax * (a @ ax), b - ax * (b @ ax)
        if np.linalg.norm(a) > 1e-6 and np.linalg.norm(b) > 1e-6:
            phi = math.degrees(math.atan2(np.cross(unit(a), unit(b)) @ ax, unit(a) @ unit(b)))
            Gm = R_axis(ax, share * phi) @ Gm
    lq[top] = G[man.par[top]].T @ Gt
    lq[mid] = Gt.T @ Gm
    lq[end] = Gm.T @ Ge
    return abs(dist - dc)


# ---- a living arm: the same measures rom.py takes (shoulder turn, elbow, forearm, wrist), kept inside their ranges
ARM_RANGE = {"elev": 166.0, "int": 62.0, "ext": 72.0, "abd90": -18.0, "el_up": 25.0, "pron": 68.0, "sup": 70.0,
             "wr_flex": 66.0, "wr_ext": 56.0, "wr_rad": 12.0, "wr_uln": 22.0}
ANKLE = (-46.0, 21.0)        # (degrees: toes down the most, toes up the most)
HAND_A = (0.7692, -0.4314, -0.4714)     # (the left hand's axis at rest, as rom.py reads a hand; the right's mirrored)
HAND_N = (0.0123, 0.7476, -0.6641)      # (its palm's normal)


def _sgn(a, b, ax):
    a, b = unit(a - ax * (a @ ax)), unit(b - ax * (b @ ax))
    return math.degrees(math.atan2(float(np.cross(a, b) @ ax), float(a @ b)))


def _swing(a, b):
    """The smallest turn taking direction a onto b."""
    v = np.cross(a, b)
    K = np.array(((0.0, -v[2], v[1]), (v[2], 0.0, -v[0]), (-v[1], v[0], 0.0)))
    return np.eye(3) + K + K @ K / max(1.0 + float(a @ b), 1e-9)


def arm_read(body, s, Gch, GU, GH, sh, el, wr):
    """One arm as rom.py reads it, in the chest's frame and that side's terms (y out to the side), degrees."""
    S = np.diag((1.0, -1.0 if s == "l" else 1.0, 1.0)) @ Gch.T
    u, lo = unit(S @ (el - sh)), unit(S @ (wr - el))
    elev = math.degrees(math.acos(float(np.clip(-u[2], -1, 1))))
    k = unit(S @ (GU @ body.hinge0[s]))
    rot = _sgn(_swing(np.array((0.0, 0.0, -1.0)), u) @ np.array((0.0, -1.0, 0.0)), k, u)
    flex = math.degrees(math.atan2(float(np.cross(u, lo) @ k), float(u @ lo)))
    a, n = unit(S @ (GH @ body.hand0[s][0])), unit(S @ (GH @ body.hand0[s][1]))
    t = np.cross(n, a)
    return {"elev": elev, "behind": math.degrees(math.asin(float(np.clip(-u[0], -1, 1)))), "rot": rot,
            "hadd": 90.0 + math.degrees(math.asin(float(np.clip(-u[1], -1, 1)))),
            "abd90": min(elev - 90.0, rot) if elev > 90.0 and math.degrees(math.atan2(u[0], u[1])) < 45.0 else -99.0,
            "el_up": math.degrees(math.asin(float(np.clip(unit(u - lo)[2], -1, 1)))) if flex > 30.0 else -99.0,
            "pron": _sgn(k, n, lo), "wr_flex": math.degrees(math.atan2(-float(lo @ n), float(lo @ a))),
            "wr_dev": math.degrees(math.atan2(-float(lo @ t), float(lo @ a))), "lo": lo, "k": k, "flex": flex}


def arm_over(m, wrist=1.0):
    """How far an arm is out of a living arm's ranges (squared degrees summed)."""
    R = ARM_RANGE
    ov = lambda v, lim: max(0.0, v - lim) ** 2
    c = ov(m["elev"], R["elev"]) + ov(m["behind"], 45.0 - 20.0 * min(1.0, max(0.0, (m["elev"] - 60.0) / 30.0)))
    c += ov(m["rot"], R["int"]) + ov(-m["rot"], R["ext"]) + ov(m["abd90"], R["abd90"]) + ov(m["el_up"], R["el_up"])
    c += ov(m["hadd"], 122.0)
    return c + wrist * (ov(m["pron"], R["pron"]) + ov(-m["pron"], R["sup"]) + ov(m["wr_flex"], R["wr_flex"]) +
                        ov(-m["wr_flex"], R["wr_ext"]) + ov(m["wr_dev"], R["wr_rad"]) + ov(-m["wr_dev"], R["wr_uln"]))


def arm_ease(body, lq, mv, s, turn_hand=True, swivel=True, slack=0.0):
    """An arm of a keyed pose brought inside a living arm's ranges: its elbow swung about the line from shoulder to
    wrist (hand and wrist stay where they are) to the nearest place where shoulder, forearm and wrist can be; then,
    `turn_hand`, the hand turned at the wrist the least that brings the wrist and forearm inside theirs (what it
    holds turns with it)."""
    man = body.man
    U, F, Hd = (man.b(n) for n in ARM[s])
    ch = man.b("chest")
    G, X = man.fk(lq, mv)
    sh, el, wr = X[U], X[F], X[Hd]
    ax = unit(wr - sh)
    A0, w_ = X[man.b("spine_01")], X[man.b("neck")] - X[man.b("spine_01")]
    ww = 0.35 if turn_hand else 1.0

    def cost(phi):
        R = R_axis(ax, phi) if phi else np.eye(3)
        e2 = sh + R @ (el - sh)
        q = A0 + w_ * float(np.clip((e2 - A0) @ w_ / (w_ @ w_), 0.0, 1.0))
        return (arm_over(arm_read(body, s, G[ch], R @ G[U], G[Hd], sh, e2, wr), ww) +
                (max(0.0, 0.13 - float(np.linalg.norm(e2 - q))) * 500.0) ** 2)
    best = (cost(0.0), 0.0) if swivel else (0.0, 0.0)
    if best[0] > 1e-9:
        for phi in range(-168, 181, 12):
            c = cost(float(phi)) + 0.002 * phi * phi if phi else 9e9
            if c < best[0]:
                best = (c, float(phi))
        for d in (-6.0, -3.0, 3.0, 6.0):
            phi = best[1] + d
            c = cost(phi) + 0.002 * phi * phi
            if c < best[0]:
                best = (c, phi)
    if best[1]:
        R = R_axis(ax, best[1])
        GU, GF = R @ G[U], R @ G[F]
        lq[U], lq[F], lq[Hd] = G[man.par[U]].T @ GU, GU.T @ GF, GF.T @ G[Hd]
        G, X = man.fk(lq, mv)
    if turn_hand:
        R = ARM_RANGE
        for _ in range(2):
            m = arm_read(body, s, G[ch], G[U], G[Hd], X[U], X[F], X[Hd])
            p = min(max(m["pron"], -R["sup"] - slack), R["pron"] + slack)
            f = min(max(m["wr_flex"], -R["wr_ext"] - slack), R["wr_flex"] + slack)
            v = min(max(m["wr_dev"], -R["wr_uln"] - slack), R["wr_rad"] + slack)
            if max(abs(p - m["pron"]), abs(f - m["wr_flex"]), abs(v - m["wr_dev"])) < 0.05:
                break
            lo, k = m["lo"], m["k"]
            kp = unit(k - lo * (k @ lo))
            n1 = kp * math.cos(math.radians(p)) + np.cross(lo, kp) * math.sin(math.radians(p))
            B = np.stack((lo, n1, np.cross(n1, lo)), axis=1)
            loh = unit(np.array((1.0, -math.tan(math.radians(f)), -math.tan(math.radians(v)))))
            B = B @ _swing(loh, X_)
            Sw = G[ch] @ np.diag((1.0, -1.0 if s == "l" else 1.0, 1.0))
            GH = frame_map(body.hand0[s][0], body.hand0[s][1], Sw @ B[:, 0], Sw @ B[:, 1])
            lq[Hd] = G[F].T @ GH
            G, X = man.fk(lq, mv)
    return lq


def elbow_out(body, lq, mv, s, most=30.0):
    """An elbow tucked under his back or into his side (as rom reads them, el_back and el_torso): the upper arm swung
    out from his side, the least (in 3 degree steps, at most `most`) that frees it; a pose with none is unchanged."""
    man = body.man
    bad = lambda l_: sum(o for k_, s2, o in pose_rom(body, [(l_, mv)]) if s2 == s and k_ in ("el_back", "el_torso"))
    best = (bad(lq), lq)
    if best[0] <= 0.0:
        return lq
    G, X = man.fk(lq, mv)
    out = G[man.b("chest")] @ np.array((0.0, -1.0 if s == "l" else 1.0, 0.0))
    ax = np.cross(unit(X[man.b("lowerarm_" + s)] - X[man.b("upperarm_" + s)]), out)
    if np.linalg.norm(ax) < 1e-6:
        return lq
    for a in np.arange(3.0, most + 1e-6, 3.0):
        l2 = turn_world(man, lq.copy(), mv, "upperarm_" + s, R_axis(unit(ax), float(a)))
        v = bad(l2)
        if v < best[0]:
            best = (v, l2)
        if v <= 0.0:
            break
    return best[1]


def elbow_tucked(body, pose_, s, deg=8.0):
    """How far (cm, as rom reads el_back and el_torso) his elbow would be under his back or in his side were the upper
    arm swung `deg` degrees in toward his side: 0 where an arm lying out on the ground has that much room."""
    man = body.man
    lq, mv = pose_
    G, X = man.fk(lq, mv)
    out = G[man.b("chest")] @ np.array((0.0, -1.0 if s == "l" else 1.0, 0.0))
    ax = np.cross(unit(X[man.b("lowerarm_" + s)] - X[man.b("upperarm_" + s)]), out)
    if np.linalg.norm(ax) < 1e-6:
        return 0.0
    l2 = turn_world(man, lq.copy(), mv, "upperarm_" + s, R_axis(unit(ax), -deg))
    return sum(o for k_, s2, o in pose_rom(body, [(l2, mv)]) if s2 == s and k_ in ("el_back", "el_torso"))


def behind_ease(body, lq, mv, s):
    """An upper arm lying further behind his body than a shoulder lets it (a man on his back on his pack, his arms
    out on the ground): brought forward to where it can be; the arm rests propped, not flat."""
    man = body.man
    U, F, ch = man.b("upperarm_" + s), man.b("lowerarm_" + s), man.b("chest")
    for _ in range(3):
        G, X = man.fk(lq, mv)
        u = unit(G[ch].T @ (X[F] - X[U]))
        elev = math.degrees(math.acos(float(np.clip(-u[2], -1, 1))))
        over = math.degrees(math.asin(float(np.clip(-u[0], -1, 1)))) - (42.0 - 20.0 * min(1.0, max(0.0,
            (elev - 60.0) / 30.0)))
        if over <= 0.0:
            break
        ax = np.cross(u, X_)
        if np.linalg.norm(ax) < 1e-6:
            break
        l2 = turn_world(man, lq.copy(), mv, "upperarm_" + s, R_axis(G[ch] @ unit(ax), over + 1.0))
        u2 = unit(G[ch].T @ (man.fk(l2, mv)[1][F] - X[U]))
        lq = l2 if u2[0] > u[0] else turn_world(man, lq, mv, "upperarm_" + s, R_axis(G[ch] @ unit(ax), -(over + 1.0)))
    # (nor further across his chest than an arm goes)
    out = np.array((0.0, -1.0 if s == "l" else 1.0, 0.0))
    for _ in range(2):
        G, X = man.fk(lq, mv)
        u = unit(G[ch].T @ (X[F] - X[U]))
        over = 90.0 + math.degrees(math.asin(float(np.clip(-(u @ out), -1, 1)))) - 122.0
        ax = np.cross(u, out)
        if over <= 0.0 or np.linalg.norm(ax) < 1e-6:
            break
        l2 = turn_world(man, lq.copy(), mv, "upperarm_" + s, R_axis(G[ch] @ unit(ax), over + 1.0))
        u2 = unit(G[ch].T @ (man.fk(l2, mv)[1][F] - X[U]))
        lq = l2 if u2 @ out > u @ out else turn_world(man, lq, mv, "upperarm_" + s,
                                                      R_axis(G[ch] @ unit(ax), -(over + 1.0)))
    return lq


def pose_rom(body, poses):
    """rom.py's faults (the measures over their limits, not the speeds) in a pose or a short run of poses (locals),
    as [(measure, side, over)]; [] where rom.py is not at hand."""
    rom = rom_mod()
    if rom is None:
        return []
    man = body.man
    M = np.diag((1.0, -1.0, 1.0))
    if not hasattr(body, "rom_body"):
        ref = rom.Body.of_rest()
        body.rom_body = rom.Body({n: M @ man.H[man.b(n)] for n in rom.CORE if n in man.ix}, ref.hands)
    Ds, Hs = [], []
    for lq, mv in poses:
        G, X = man.fk(lq, mv)
        Ds.append([M @ G[man.b(k)] @ M for k in rom.CORE])
        Hs.append([M @ X[man.b(k)] for k in rom.CORE])
    if len(Ds) == 1:
        Ds, Hs = Ds * 2, Hs * 2
    rep = rom.check(rom.Poses(np.array(Ds), np.array(Hs), 60.0, rom.CORE), body=body.rom_body)
    return [(r["key"], r["side"], float(r["over"])) for r in rep["faults"]]


def rom_values(body, pose_):
    """rom.py's measures of one pose (locals): {measure_side: value}; {} where rom.py is not at hand."""
    rom = rom_mod()
    if rom is None:
        return {}
    man = body.man
    M = np.diag((1.0, -1.0, 1.0))
    if not hasattr(body, "rom_body"):
        ref = rom.Body.of_rest()
        body.rom_body = rom.Body({n: M @ man.H[man.b(n)] for n in rom.CORE if n in man.ix}, ref.hands)
    G, X = man.fk(*pose_)
    D = [M @ G[man.b(k)] @ M for k in rom.CORE]
    Hh = [M @ X[man.b(k)] for k in rom.CORE]
    rep = rom.check(rom.Poses(np.array([D, D]), np.array([Hh, Hh]), 60.0, rom.CORE), body=body.rom_body)
    return {k: float(v[0]) for k, v in rep["series"].items()}


FLINCH_ANKLE = 24.0            # (degrees: the most a flinch bends an ankle up, as rom.py reads it, short of its 25)
WRIST_LIM = {"wr_dev": (-30.0, 20.0), "wr_flex": (-65.0, 75.0)}     # (rom.py's: ulnar/radial, extension/flexion)


def wrist_swivel(body, lq, mv, s, margin=0.3, step=1.5, most=36.0):
    """A wrist bent past a living wrist's range (as rom.py reads it) by a hand held where it is: the elbow swung about
    the line from shoulder to wrist, the hand keeping its place and turn, the least (in `step` degree steps) that
    brings the wrist back inside, and puts no other part of that arm out of its range; a wrist inside is unchanged."""
    def wrist_over(v):
        o = 0.0
        for k_, (lo, hi) in WRIST_LIM.items():
            x = v.get(k_ + "_" + s, float("nan"))
            if x == x:
                o += max(0.0, x - (hi - margin)) + max(0.0, (lo + margin) - x)
        return o
    v0 = rom_values(body, (lq, mv))
    o0 = wrist_over(v0)
    if not v0 or o0 <= 0.0:
        return lq
    arm_keys = ("sh_", "el_", "pron", "sup")
    others0 = sum(o for k_, s2, o in pose_rom(body, [(lq, mv)]) if s2 == s and k_.startswith(arm_keys))
    best = (o0, lq)
    for k in range(1, int(most / step) + 1):
        done = []
        for sg in (1.0, -1.0):
            l2 = swung_arm(body, lq, mv, s, sg * k * step)
            v = rom_values(body, (l2, mv))
            o = wrist_over(v)
            if o < best[0] - 1e-6 and sum(x for k_, s2, x in pose_rom(body, [(l2, mv)])
                                          if s2 == s and k_.startswith(arm_keys)) <= others0 + 1e-6:
                best = (o, l2)
                if o <= 0.0:
                    done.append((v.get("sh_elev_" + s, 0.0), sg, l2))
        if done:
            # (of the two ways that do, the one that leaves the elbow lower)
            return min(done, key=lambda x: (x[0], -x[1]))[2]
    return best[1]


def first_in_range(body, makers, also=None):
    """The first pose of `makers` (functions making a pose, the one meant first) that a living body can be in (rom.py
    finds nothing over its limits, nor `also`, a further test giving how far over a pose is); else the one least
    over."""
    best = None
    for mk in makers:
        p = mk()
        f = pose_rom(body, [p])
        more = also(p) if also is not None else 0.0
        if not f and more <= 0.0:
            return p
        over = sum(x[2] for x in f) + more
        if best is None or over < best[0]:
            best = (over, p)
    return best[1]


def elbow_ease(body, lq, mv, s, least=3.0):
    """An elbow bent backwards (a straight arm carried a little past straight between two keys): bent the right way."""
    man = body.man
    U, F, Hd = (man.b(n) for n in ARM[s])
    ch = man.b("chest")
    G, X = man.fk(lq, mv)
    flex = arm_read(body, s, G[ch], G[U], G[Hd], X[U], X[F], X[Hd])["flex"]
    if flex >= least or flex < -40.0:
        return lq
    k = G[U] @ body.hinge0[s]
    for sgn in (1.0, -1.0):
        l2 = lq.copy()
        l2[F] = (G[U].T @ R_axis(k, sgn * (least - flex)) @ G[U]) @ lq[F]
        G2, X2 = man.fk(l2, mv)
        f2 = arm_read(body, s, G2[ch], G2[U], G2[Hd], X2[U], X2[F], X2[Hd])["flex"]
        if abs(f2 - least) < 0.5 * abs(flex - least):
            return l2
    return lq


def ankle_deg(body, lq, s):
    """An ankle as rom.py reads it: toes up +, toes down -, degrees."""
    man = body.man
    f, bl = man.b("foot_" + s), man.b("ball_" + s)
    f0 = man.H[bl] - man.H[f]
    fr = lq[f] @ f0
    return -math.degrees(math.atan2(f0[2] * fr[0] - f0[0] * fr[2], f0[0] * fr[0] + f0[2] * fr[2]))


def ankle_ease(body, lq, s):
    """A foot turned at the ankle further than an ankle goes: turned back to the end of its range."""
    a = ankle_deg(body, lq, s)
    c = min(max(a, ANKLE[0]), ANKLE[1])
    if abs(a - c) > 0.05:
        f = body.man.b("foot_" + s)
        for sgn in (1.0, -1.0):
            l2 = lq.copy()
            l2[f] = Ry(sgn * (a - c)) @ lq[f]
            if abs(ankle_deg(body, l2, s) - c) < 0.5 * abs(a - c):
                return l2
    return lq


def heel_up(body, lq, mv, s, most=21.0):
    """A foot bent up at the ankle past `most` (his shin pitched over a planted foot): its heel raised about the ball
    of the foot, the ball and toes staying where they are, and the leg solved to it."""
    man = body.man
    f, bl = man.b("foot_" + s), man.b("ball_" + s)
    for _ in range(3):
        a = ankle_deg(body, lq, s)
        if a <= most + 0.05:
            break
        G, X = man.fk(lq, mv)
        ax = G[f] @ Y_
        best = None
        for sg in (1.0, -1.0):
            R = R_axis(ax, sg * (a - most))
            Ge, Xe = R @ G[f], X[bl] + R @ (X[f] - X[bl])
            l2 = lq.copy()
            ik2(man, l2, mv, LEG[s], Xe, end_R=Ge)
            a2 = ankle_deg(body, l2, s)
            if best is None or abs(a2 - most) < best[0]:
                best = (abs(a2 - most), l2)
        lq = best[1]
    return lq


def heel_pivot(body, lq, mv, s, least=-42.0):
    """A planted foot pointed down at the ankle past `least` (his leg stretched out before him, the sole flat): its
    toes raised about its heel, the heel staying where it is, and the leg solved to it."""
    man = body.man
    f, bl = man.b("foot_" + s), man.b("ball_" + s)
    mask = body.Z["foot_" + s]
    for _ in range(3):
        a = ankle_deg(body, lq, s)
        if a >= least - 0.05:
            break
        G, X = man.fk(lq, mv)
        V = gore.skin(body.P[mask], body.pa[mask], body.pb[mask], body.pw[mask], G, X, man.H)
        fw = X[bl] - X[f]
        fw = unit(fw - Z_ * fw[2])
        low = V[V[:, 2] < V[:, 2].min() + 0.015]
        heel = low[int(np.argmin(low @ fw))]
        ax = G[f] @ Y_
        best = None
        for sg in (1.0, -1.0):
            R = R_axis(ax, sg * (least - a))
            Ge, Xe = R @ G[f], heel + R @ (X[f] - heel)
            l2 = lq.copy()
            ik2(man, l2, mv, LEG[s], Xe, end_R=Ge)
            a2 = ankle_deg(body, l2, s)
            if best is None or abs(a2 - least) < best[0]:
                best = (abs(a2 - least), l2)
        lq = best[1]
    return lq


def place_gun(body, lq, mv, Gw, Xw):
    return set_world(body.man, lq, mv, body.wi, Gw, Xw)


def gun_rides(body, lq, mv, rel, s="r"):
    """The weapon bone carried by hand s exactly, as `rel` (grip_rel) has the hand on it."""
    man = body.man
    G, X = man.fk(lq, mv)
    h = man.b("hand_" + s)
    Gw = G[h] @ rel[0].T
    return set_world(man, lq, mv, body.wi, Gw, X[h] - Gw @ rel[1], G, X)


def hold(body, lq, mv, W, rel, anchor="r", other=True, share=0.3):
    """The weapon wanted at W = (turn, place); rel[s] the hands on it (grip_rel). The anchor hand is solved onto it
    and the weapon then rides that hand exactly (it never floats off it); the other hand is solved onto its own grip."""
    man = body.man
    Gw, Xw = W
    s = anchor
    ik2(man, lq, mv, ARM[s], Xw + Gw @ rel[s][1], end_R=Gw @ rel[s][0], share=share)
    lq, mv = gun_rides(body, lq, mv, rel[s], s)
    if other:
        o = "l" if s == "r" else "r"
        G, X = man.fk(lq, mv)
        Gw, Xw = G[body.wi], X[body.wi]
        ik2(man, lq, mv, ARM[o], Xw + Gw @ rel[o][1], end_R=Gw @ rel[o][0], share=share)
    return lq, mv


def keep_prop(body, lq, mv, ref):
    """A prop on the bolt bone (the loader's rocket, the radio man's handset) kept on his chest as `ref` has it."""
    if not body.bolt_prop:
        return lq, mv
    man = body.man
    G, X = man.fk(lq, mv)
    c, bo = man.b("chest"), man.b("bolt")
    Rb, pb = ref
    return set_world(man, lq, mv, bo, G[c] @ Rb, X[c] + G[c] @ pb, G, X)


def prop_ref(body, G, X):
    man = body.man
    c, bo = man.b("chest"), man.b("bolt")
    return G[c].T @ G[bo], G[c].T @ (X[bo] - X[c])


def steady_whip(body, lq, mv, Gref, k=0.85):
    """The radio man's whip aerial kept mostly as upright as the base pose has it, so its tip sways and does not
    sweep metres when his trunk is jolted."""
    if not body.whip:
        return lq
    man = body.man
    G, X = man.fk(lq, mv)
    w = body.whip[0]
    lq[w] = G[man.par[w]].T @ qslerp(G[w], Gref[w], k)
    return lq


# ================================================================================================ the flinches
# The jolt, as a share of its peak: struck (fastest at the start), back through the stance with a small
# overshoot, still.
def jolt(t, L=FLINCH_LONG, tp=FLINCH_PEAK, tu=0.235, over=0.09, snap=2.4):
    if t <= 0.0 or t >= L:
        return 0.0
    if t < tp:
        return 1.0 - (1.0 - t / tp) ** snap
    if t < tu:
        return 1.0 - (1.0 + over) * sm(((t - tp) / (tu - tp)) ** 0.9)
    return -over * (1.0 - sm((t - tu) / (L - tu)))


def run_dip(t):
    """The stumble at a run: down from 0.02 s, held from 0.07 to 0.15 s, up again by 0.25 s."""
    if t <= 0.02 or t >= 0.25:
        return 0.0
    if t < 0.07:
        return sm((t - 0.02) / 0.05)
    if t < 0.15:
        return 1.0
    return 1.0 - sm((t - 0.15) / 0.10)


def head_lag(t, L=FLINCH_LONG, d=0.045):
    """How far his head is left behind the trunk's jolt (+), then thrown past it (-)."""
    if t <= 0.0 or t >= L:
        return 0.0
    return (jolt(t) - jolt(t - d)) * (1.0 - sm((t - 0.2) / (L - 0.2)))


# Each way's jolt at its peak, standing: the chest's lean (pitch + forward, side + to his left, twist + to his right,
# degrees, shared by spine_01, spine_02 and the chest), the pelvis's travel (m, his axes) and its own small tip.
# _f: struck in the front, the chest goes back and the left (leading) shoulder is driven back. _b: the opposite.
# _l: struck from his left, he bends and is pushed to his right and turns away from it; _r its mirror.
JOLT = {"f": dict(lean=(-11.5, 1.5, -8.0), pelvis=(-0.042, 0.004, -0.026), tip=(-2.0, 0.0, -1.5)),
        "b": dict(lean=(11.5, -1.5, 8.0), pelvis=(0.044, -0.004, -0.026), tip=(2.0, 0.0, 1.5)),
        "l": dict(lean=(-2.0, -11.5, 8.0), pelvis=(-0.006, 0.040, -0.024), tip=(0.0, -2.0, 2.0)),
        "r": dict(lean=(-2.0, 11.5, -8.0), pelvis=(-0.006, -0.040, -0.024), tip=(0.0, 2.0, -2.0))}
SPINE_SHARE = (("spine_01", 0.27), ("spine_02", 0.33), ("chest", 0.40))
STANCE = {"stand": dict(base="fire_stand", travel=1.0, lean=1.0),
          "kneel": dict(base="fire_kneel", travel=0.42, lean=0.95),
          "run": dict(base="run", travel=0.8, lean=0.9)}
RUN_BREAK = 0.0              # (s: the legs stay on the run's own keys: the game moves the root at the run's speed, so a
#                              leg played behind the run would skate its planted foot)
RUN_DIP = 0.045              # (m: the stumble: his pelvis drops, his knees give, his feet stay where the run has them)
GUN_LAG = 0.22               # (the weapon's share of the jolt it is left behind by, at first)


class Stand:
    """What holds in a base pose: his feet on the ground, his hands on the weapon, the weapon on his chest."""

    def __init__(self, body, lq, mv):
        man = body.man
        self.lq, self.mv = lq.copy(), mv.copy()
        self.G, self.X = G, X = man.fk(lq, mv)
        self.rel = {s: body.grip_rel(G, X, s) for s in "lr"}
        self.gun_chest = body.gun_on(G, X, "chest")
        self.prop = prop_ref(body, G, X) if body.bolt_prop else None
        self.foot = {s: (G[man.b("foot_" + s)].copy(), X[man.b("foot_" + s)].copy()) for s in "lr"}
        self.knee = {s: X[man.b("calf_" + s)].copy() for s in "lr"}
        # (a leg whose knee is on the ground: it stays there, the thigh turns over it)
        self.knee_down = {s: bool(X[man.b("calf_" + s)][2] < 0.2) for s in "lr"}


def flinch_pose(body, stance, way, t):
    """The pose of hit_flinch[_kneel|_run]_<way> at t."""
    man = body.man
    S = STANCE[stance]
    L = FLINCH_LONG
    run = stance == "run"
    lq, mv = body.pose(S["base"], t if run else 0.0)
    if t <= 0.0 or t >= L:
        return lq, mv
    base = Stand(body, lq, mv)
    J = JOLT[way]
    e, ep, lag = jolt(t), jolt(t, tp=0.10, tu=0.245, over=0.06, snap=2.0), head_lag(t)
    pe = man.b("pelvis")
    if run:
        # (the legs run on, a little behind themselves: the break in the stride; his trunk above them is jolted)
        br = math.sin(math.pi * sm(t / L)) ** 2
        ll, lm = body.pose(S["base"], t - RUN_BREAK * br)
        for s in "lr":
            for n in LEG[s] + ("ball_" + s,):
                lq[man.b(n)] = ll[man.b(n)]
        lq[pe], mv[pe] = ll[pe], lm[pe]
    # (the pelvis: pushed along the round's way, down as his knees give, tipped a little)
    mv[pe] = mv[pe] + np.array(J["pelvis"]) * S["travel"] * ep
    if run:
        # (the break in the stride: his pelvis drops and his knees give whatever the run's phase, his feet staying
        # where the run has them; back by 0.25 s)
        mv[pe] = mv[pe] - Z_ * (RUN_DIP * run_dip(t) + float(np.array(J["pelvis"])[2]) * S["travel"] * ep)
    tp_ = np.array(J["tip"]) * ep * S["lean"]
    G, X = man.fk(lq, mv)
    lq[pe] = euler(*tp_) @ G[pe]
    # (the trunk: the lean and the twist shared up the spine, about the world's axes so the ways are his own frame's)
    ln = np.array(J["lean"]) * e * S["lean"]
    for bn, f in SPINE_SHARE:
        lq = turn_world(man, lq, mv, bn, euler(*(ln * f)))
    # (the neck and head left behind at first, then thrown past: against the chest's own turn from the base)
    Dfull = euler(*(np.array(J["lean"]) * S["lean"]))
    for bn, f in (("neck", 0.45), ("head", 0.55)):
        lq = turn_world(man, lq, mv, bn, rpow(Dfull, -lag * f * 1.15))
    # (the feet stay where they stand; a knee on the ground stays on it)
    if run:
        Gl, Xl = man.fk(ll, lm)
        for s in "lr":
            f = man.b("foot_" + s)
            ik2(man, lq, mv, LEG[s], Xl[f], end_R=Gl[f])
            lq = heel_up(body, lq, mv, s)
    if not run:
        for s in "lr":
            if base.knee_down[s]:
                G, X = man.fk(lq, mv)
                th = man.b("thigh_" + s)
                K = base.knee[s]
                Ln = float(np.linalg.norm(base.X[th] - K))
                mv[pe] = mv[pe] + (K + unit(X[th] - K) * Ln - X[th])
        for s in "lr":
            ik2(man, lq, mv, LEG[s], base.foot[s][1], end_R=base.foot[s][0])
        # (his knees give no further than his ankles let them: where the drop would bend an ankle up past its range
        # (a man who stands crouched already), his pelvis is let down only as far as it can go)
        most = {s: max(FLINCH_ANKLE, ankle_deg(body, base.lq, s) + 0.3) for s in "lr"}
        over = lambda l_: max(ankle_deg(body, l_, s) - most[s] for s in "lr")
        if over(lq) > 0.0:
            lo_, hi_ = 0.0, 0.08
            for _ in range(12):
                mid = 0.5 * (lo_ + hi_)
                l2, m2 = lq.copy(), mv.copy()
                m2[pe] = m2[pe] + Z_ * mid
                for s in "lr":
                    ik2(man, l2, m2, LEG[s], base.foot[s][1], end_R=base.foot[s][0])
                if over(l2) > 0.0:
                    lo_ = mid
                else:
                    hi_ = mid
            mv[pe] = mv[pe] + Z_ * hi_
            for s in "lr":
                ik2(man, lq, mv, LEG[s], base.foot[s][1], end_R=base.foot[s][0])
    # (the weapon rides his chest as the base has it, left behind a little at first; his hands stay on it)
    G, X = man.fk(lq, mv)
    c = man.b("chest")
    Gw, Xw = G[c] @ base.gun_chest[0], X[c] + G[c] @ base.gun_chest[1]
    k = GUN_LAG * max(lag, 0.0) / 0.6
    Gw, Xw = qslerp(Gw, base.G[body.wi], k), Xw * (1 - k) + base.X[body.wi] * k
    lq, mv = hold(body, lq, mv, (Gw, Xw), base.rel)
    # (a wrist the hold bends past its range: the elbow swung, the hand where it holds)
    for s in "lr":
        lq = wrist_swivel(body, lq, mv, s)
    lq, mv = keep_prop(body, lq, mv, base.prop)
    lq = steady_whip(body, lq, mv, base.G)
    return lq, mv


# ---- lying: a jolt through the whole body, then he ducks his head and his left hand comes off to cover his helmet
def prone_lift(t):
    """The shudder lying: from 0.0 s, at its most 0.04 to 0.07 s, gone by 0.16 s (a small one: he flinches DOWN)."""
    if t <= 0.0 or t >= 0.16:
        return 0.0
    if t < 0.04:
        return 1.0 - (1.0 - t / 0.04) ** 2
    if t < 0.07:
        return 1.0
    return 1.0 - sm((t - 0.07) / 0.09)


def prone_kick(t):
    """His knees bend a little and his lower legs twitch up (0.02 to 0.08 s), held to 0.14 s, settled by 0.30 s."""
    if t <= 0.02 or t >= 0.30:
        return 0.0
    if t < 0.08:
        return sm((t - 0.02) / 0.06)
    if t < 0.14:
        return 1.0
    return 1.0 - sm((t - 0.14) / 0.16)


def duck(t, L=PRONE_LONG):
    """His head snapped down and his shoulders hunched from 0.0 s, most of it by 0.07 s, all by 0.10 s, held while
    his hand covers his helmet, back by 0.42 s: 0 at the ends."""
    if t <= 0.0 or t >= 0.42:
        return 0.0
    if t < 0.10:
        return 1.0 - (1.0 - t / 0.10) ** 2.4
    if t < 0.30:
        return 1.0
    return 1.0 - sm((t - 0.30) / 0.12)


HAND_T = (0.04, 0.21, 0.30, 0.46)
TURN_T = (0.04, 0.25, 0.26, 0.48)   # (s: his left wrist's turn from the grip to the helmet and back)
HELM_YAW = 0.0               # (degrees: his fingers on the helmet turned about its crown from across it)
PRONE_SHARE = 0.6            # (hit_flinch_prone: how much of the left wrist's twist on its way to the helmet the forearm takes)


def gun_off(t):
    """How far the weapon is drawn off his face (to his right and down) as he ducks: with the duck, a little ahead of
    it at first and a little behind it on the way back, so his face never comes down onto the stock."""
    if t <= 0.0 or t >= 0.47:
        return 0.0
    if t < 0.30:
        return min(1.0, duck(t) * (0.75 + 0.25 * min(1.0, 2.0 * hand_up(t))) * 1.15)
    return 1.0 - sm((t - 0.37) / 0.10) if t > 0.37 else 1.0   # (s: his left hand off the weapon, on his helmet, off it, back on its grip)


def hand_up(t, L=PRONE_LONG):
    """His left hand: off the weapon at 0.07 s, up over and onto the helmet by 0.22 s, held, back on its grip by
    0.44 s and kept there."""
    a, b, c, d = HAND_T
    if t <= a or t >= d:
        return 0.0
    if t < b:
        return sm((t - a) / (b - a))
    if t < c:
        return 1.0
    return 1.0 - sm((t - c) / (d - c))


def hand_turn(t, L=PRONE_LONG):
    """How far his left hand has turned from its grip to lying on the helmet: mostly even, a little eased, so the
    wrist turns at a steady speed, not all at once in the middle; over TURN_T, a little longer than the hand takes to
    get there and back (HAND_T), so it is still settling as it comes onto the steel and starts back a little before
    it leaves."""
    a, b, c, d = TURN_T
    if t <= a or t >= d:
        return 0.0
    if t < b:
        u = (t - a) / (b - a)
    elif t < c:
        return 1.0
    else:
        u = 1.0 - (t - c) / (d - c)
    return 0.75 * u + 0.25 * sm(u)


HEAD_DOWN = 40.0             # (degrees his head and neck go down at most, lying; stopped short of ground, fist, stock)
HEAD_TURN = 20.0             # (degrees his head turns to his left as it goes down: the face off the stock)
HEAD_SIDE = 20.0             # (degrees his neck bends to his left as his head goes down: the helmet off his right fist)
HUNCH = 7.0                  # (degrees his shoulders come up round his neck)
HEAD_CLEAR = 0.012           # (m: his helmet's brim and his face keep this far off the ground)
PRONE_LIFT = (-1.0, -1.5)    # (degrees spine_02 and the chest are pressed DOWN at the jolt: his shoulders drop)
PRONE_YAW = 3.0              # (degrees his pelvis twitches in yaw at the jolt)
PRONE_KNEE = 14.0            # (degrees both knees bend at the jolt: the lower legs twitch up)
PRONE_MUZZLE = -3.0          # (degrees the muzzle jerks up at the jolt: down, with his shoulders)
PRONE_GUN = (0.0, 0.08, -0.02)     # (m: the weapon drawn to his right and down as he ducks: his face goes past it)
PRONE_DROP = (1.0, 1.5)      # (degrees spine_02 and the chest go down as he ducks: his shoulders lowered)
PRONE_DIP = 5.0              # (degrees the muzzle dips as the left hand lets go of it)
PRONE_ROLL = 9.0             # (degrees the weapon rolls to his right about its barrel as he ducks: stock off his face)
INSIDE_CM = 0.8              # (cm: the most a fist may sit inside his head and helmet, nearest-point test)


def inside_depth(body, G, X, a, b, reach=0.05):
    """How deep (m) the points of mask `a` lie inside the surface of mask `b` (nearest point of b, by its facing)."""
    if not hasattr(body, "F"):
        body.F = np.array(body.D["near"]["facings"], float).reshape(-1, 3)
    V = body.skin(G, X)
    ib = np.nonzero(b)[0]
    Fb = body.F[ib]
    Na = np.einsum("nij,nj->ni", G[body.pa[ib]], Fb)
    Nb = np.einsum("nij,nj->ni", G[body.pb[ib]], Fb)
    N = Na * (1 - body.pw[ib])[:, None] + Nb * body.pw[ib][:, None]
    N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    P, Q = V[ib], V[a]
    worst = 0.0
    for c in range(0, len(Q), 400):
        q = Q[c:c + 400]
        d2 = ((q[:, None, :] - P[None, :, :]) ** 2).sum(-1)
        j = d2.argmin(1)
        dist = np.sqrt(d2[np.arange(len(q)), j])
        s_ = np.einsum("ij,ij->i", q - P[j], N[j])
        dd = np.where((s_ < 0) & (dist < reach), -s_, 0.0)
        worst = max(worst, float(dd.max()) if len(dd) else 0.0)
    return worst


def over_crown(body, G, X, mask, clear=0.005, near=0.02):
    """How far (m) the points of `mask` (a hand) must go up along his head's own up to lie on his helmet, not in it:
    each point against the highest point of his head and helmet within `near` of it, seen from above the head."""
    man = body.man
    h = man.b("head")
    V = body.skin(G, X)
    Hd = (V[body.Z["head"]] - X[h]) @ G[h]
    Q = (V[mask] - X[h]) @ G[h]
    need = 0.0
    for c in range(0, len(Q), 300):
        q = Q[c:c + 300]
        d2 = ((q[:, None, :2] - Hd[None, :, :2]) ** 2).sum(-1)
        top = np.where(d2 < near * near, Hd[None, :, 2], -9.0).max(1)
        need = max(need, float((top + clear - q[:, 2]).max()))
    return max(0.0, need)


def prone_flinch_pose(body, t, full=False, probe=False):
    """`full`: the pose with his head all the way down and his hand on his helmet, its arm eased into a living
    arm's ranges: where the hand's way up and back is taken from. `probe`: the pose before his elbows are kept off
    the ground (what elbow_env reads)."""
    man = body.man
    lq, mv = body.pose("fire_prone", 0.0)
    L = PRONE_LONG
    if (t <= 0.0 or t >= L) and not full:
        return lq, mv
    base = Stand(body, lq, mv)
    d, hu = (1.0, 1.0) if full else (duck(t), hand_up(t))
    up_, kick = (0.0, 0.0) if full else (prone_lift(t), prone_kick(t))
    b = man.b
    pe, c_ = b("pelvis"), b("chest")
    # (the jolt: his pelvis twitches in yaw, his chest and shoulders are thrown up and drop back, his knees bend)
    if up_ > 0.0 or d > 0.0:
        lq = turn_world(man, lq, mv, "pelvis", Rz(PRONE_YAW * up_))
        lq = turn_world(man, lq, mv, "spine_01", Rz(-0.7 * PRONE_YAW * up_))
        lq = turn_world(man, lq, mv, "spine_02", Ry(-PRONE_LIFT[0] * up_ + PRONE_DROP[0] * d))
        lq = turn_world(man, lq, mv, "chest", Ry(-PRONE_LIFT[1] * up_ + PRONE_DROP[1] * d))
    if kick > 0.0:
        for s in "lr":
            G, X = man.fk(lq, mv)
            ca, fo = b("calf_" + s), b("foot_" + s)
            ax = unit(G[b("thigh_" + s)] @ Y_)
            r = X[fo] - X[ca]
            sg = 1.0 if float(np.cross(ax, r)[2]) > 0 else -1.0
            lq = turn_world(man, lq, mv, "calf_" + s, R_axis(ax, sg * PRONE_KNEE * kick))
    G0, X0 = man.fk(lq, mv)
    side = G0[c_] @ Y_
    side = unit(side - Z_ * (side @ Z_))
    # (the weapon: thrown up with his shoulders, its muzzle jerked up; then rolled to his right about its barrel as he
    # ducks, its muzzle dipping a little as the left hand lets go; the right hand keeps it)
    Gw, Xw = base.G[body.wi], base.X[body.wi]
    Xh = base.X[b("hand_r")]
    if up_ > 0.0 or d > 0.0:
        # (carried with his right shoulder, not turned with his chest)
        ua = b("upperarm_r")
        lift = X0[ua] - base.X[ua]
        Xw, Xh = Xw + lift, Xh + lift
        if body.armed:
            # (its muzzle jerked up about the butt in his shoulder)
            if not hasattr(body, "butt"):
                Pg = body.P[body.is_gun] - man.H[body.wi]
                body.butt = Pg[np.argsort(Pg @ unit(body.muzzle))[:20]].mean(0)
            Xb = Xw + Gw @ body.butt
            jerk = R_axis(unit(Gw @ Y_), -PRONE_MUZZLE * up_)
            Gw, Xw, Xh = jerk @ Gw, Xb + jerk @ (Xw - Xb), Xb + jerk @ (Xh - Xb)
    if body.armed:
        bar = unit(Gw @ body.muzzle)
        sg = 1.0 if float(bar @ X_) > 0 else -1.0
        roll = R_axis(bar, -sg * PRONE_ROLL * d)
        dip = R_axis(unit(Gw @ Y_), PRONE_DIP * hu)
        gf = gun_off(t) if not full else 1.0
        shift = (G0[c_] @ np.array((PRONE_GUN[0], PRONE_GUN[1], 0.0))) * gf + Z_ * PRONE_GUN[2] * gf
        Gw2, Xw2 = roll @ dip @ Gw, Xh + roll @ dip @ (Xw - Xh) + shift
        Vw = body.skin_of(*place_gun(body, lq.copy(), mv.copy(), Gw2, Xw2))
        if Vw[body.is_gun, 2].min() < 0.004:
            Gw2, Xw2 = roll @ Gw, Xh + roll @ (Xw - Xh) + shift
    else:
        Gw2, Xw2 = Gw, Xw
    # (his head and neck down and turned to his left, stopped short of the ground, his right fist and the stock)
    ln, lh = lq[b("neck")].copy(), lq[b("head")].copy()
    head_m = body.Z["head"]
    head_s = np.zeros(len(body.P), bool)
    head_s[np.nonzero(head_m)[0][::3]] = True
    fist_r = body.Z["hand_r"]

    # (the turn leads the duck, so his face is off the stock before it goes down)
    turn = HEAD_TURN * max(min(1.0, 1.8 * d), 0.6 * up_)

    fw = unit(np.cross(side, Z_))
    fw = fw if float(fw @ (G0[b("head")] @ X_)) > 0 else -fw
    bend = HEAD_SIDE * max(min(1.0, 1.8 * d), 0.6 * up_)

    def head_at(deg):
        # (down about his shoulders' axis, to his left about the way he faces, turned to his left)
        l2 = lq.copy()
        l2[b("neck")], l2[b("head")] = ln, lh
        l2 = turn_world(man, l2, mv, "neck", R_axis(side, 0.45 * deg) @ R_axis(fw, 0.6 * bend) @ Rz(-0.4 * turn))
        l2 = turn_world(man, l2, mv, "head", R_axis(side, 0.55 * deg) @ R_axis(fw, 0.4 * bend) @ Rz(-0.6 * turn))
        return l2

    def placed(l2):
        l3, m3 = l2.copy(), mv.copy()
        return hold(body, l3, m3, (Gw2, Xw2), base.rel, other=False)

    def clash(l2):
        l3, m3 = placed(l2)
        G3, X3 = man.fk(l3, m3)
        V = body.skin(G3, X3)
        if V[head_m, 2].min() < HEAD_CLEAR:
            return True
        if inside_depth(body, G3, X3, fist_r, head_s) > max(fist0, INSIDE_CM / 100.0):
            return True
        if body.armed and inside_depth(body, G3, X3, gun_m, head_s) > max(gun0, 0.01) + 0.002:
            return True
        return False
    gun_m = body.is_gun
    if ("head_cap",) not in body.poses:
        body.poses[("head_cap",)] = (0.0, 0.0, HEAD_DOWN)
        fist0 = inside_depth(body, base.G, base.X, fist_r, head_s)
        gun0 = inside_depth(body, base.G, base.X, gun_m, head_s) if body.armed else 0.0
        body.poses[("head_cap",)] = (fist0, gun0, HEAD_DOWN)
        if not full and d < 1.0:
            # (the most his head may go down: found once, at the full duck, so it scales smoothly with the duck)
            prone_flinch_pose(body, 0.25)
    fist0, gun0, cap = body.poses[("head_cap",)]
    want_ = cap * d
    if d >= 1.0 and cap >= HEAD_DOWN:
        # (the most his head may go down: the deepest of these with nothing in the way at it or above it)
        cap = 0.0
        for deg in np.arange(2.5, HEAD_DOWN + 1e-6, 2.5):
            if clash(head_at(deg)):
                break
            cap = deg
        body.poses[("head_cap",)] = (fist0, gun0, cap - 1e-3)
        want_ = cap * d
    l2 = head_at(want_)
    lq = l2
    # (his shoulders hunched up round his neck)
    for s, sg in (("l", 1.0), ("r", -1.0)):
        G, X = man.fk(lq, mv)
        ax = G[c_] @ X_
        lq = turn_world(man, lq, mv, "clavicle_" + s, R_axis(ax, -sg * HUNCH * d))
    lq, mv = hold(body, lq, mv, (Gw2, Xw2), base.rel, other=False)
    if not body.armed:
        # (an unarmed man's right hand lies on the ground: it is lifted with his shoulders, never pressed into it)
        fz0 = zone_low(body, base.lq, base.mv, body.Z["fore_r"])[0]
        for _ in range(3):
            z, _p, X = zone_low(body, lq, mv, body.Z["fore_r"])
            need = min(fz0, 0.0) + 0.001 - z
            if need <= 0.0:
                break
            G, X = man.fk(lq, mv)
            h = b("hand_r")
            ik2(man, lq, mv, ARM["r"], X[h] + Z_ * (need + 0.002), end_R=G[h], share=0.3)
    # (the left hand: from its grip, up and over, then down onto the crown of his helmet from above and behind,
    # palm down, and back)
    G, X = man.fk(lq, mv)
    A = man.side["l"]
    h = b("head")
    Gh_grip, Xh_grip = G[body.wi] @ base.rel["l"][0], X[body.wi] + G[body.wi] @ base.rel["l"][1]
    crown = X[h] + G[h] @ (body.crown + np.array((-0.045, -0.035, 0.0)))
    up = G[h] @ Z_
    fwd = unit(G[h] @ np.array((-0.55, 0.75, 0.0)))               # (his fingers lie across the crown, toward its right)
    fwd = unit(fwd - up * (fwd @ up) * 0.6)
    fwd = R_axis(up, HELM_YAW) @ fwd
    Gh_helm = frame_map(A["d0"], A["n0"], fwd, -up)               # (palm onto the helmet)
    wrist = crown + up * 0.035 - Gh_helm @ (A["d0"] * 0.075)
    U_, F_, H_ = (b(n) for n in ARM["l"])
    if full:
        ik2(man, lq, mv, ARM["l"], wrist, pole=unit(-side + Z_ * 0.3), end_R=Gh_helm, share=0.4)
        lq = arm_ease(body, lq, mv, "l")
        return lq, mv
    if ("helm",) not in body.poses:
        lf, mf = prone_flinch_pose(body, 0.2, True)
        Gf, Xf = man.fk(lf, mf)
        # (the hand on the helmet lifted until his fist is out of the steel)
        hf = Gf[h].T @ (Xf[H_] - Xf[h])
        for _ in range(4):
            dep = over_crown(body, Gf, Xf, body.Z["hand_l"])
            if dep <= 0.001:
                break
            ik2(man, lf, mf, ARM["l"], Xf[H_] + (Gf[h] @ Z_) * dep, end_R=Gf[H_], share=0.4)
            Gf, Xf = man.fk(lf, mf)
            hf = Gf[h].T @ (Xf[H_] - Xf[h])
        body.poses[("helm",)] = (Gf[c_].T @ unit(Xf[F_] - 0.5 * (Xf[U_] + Xf[H_])), Gf[h].T @ Gf[H_], hf)
    pole_c, Rh, ph = body.poses[("helm",)]
    end = X[h] + G[h] @ ph
    # (its way: out to his left and up over the helmet, the last of it down and forward onto the crown)
    over = end + up * 0.10 - unit(G[h] @ X_) * 0.05 - side * 0.05
    out = Xh_grip - side * 0.20 + Z_ * 0.03                       # (first out to his left, clear of the brim)
    u = hand_turn(t)
    T = (1 - u) ** 3 * Xh_grip + 3 * u * (1 - u) ** 2 * out + 3 * u ** 2 * (1 - u) * over + u ** 3 * end
    Gh = qslerp(Gh_grip, G[h] @ Rh, u)
    wp = u
    pole = unit(unit(base.X[F_] - 0.5 * (base.X[U_] + base.X[H_])) * (1 - wp) + (G[c_] @ pole_c) * wp)
    ik2(man, lq, mv, ARM["l"], T, pole=pole, end_R=Gh, share=PRONE_SHARE)
    if hu > 0.0 or u > 0.0:
        # (its wrist eased into its range by degrees as the hand leaves the grip and turns, not all at once)
        le = arm_ease(body, lq.copy(), mv, "l", swivel=False, slack=3.0)
        w_ = min(1.0, 3.0 * max(hu, u))
        for i in range(man.nb):
            if not np.array_equal(le[i], lq[i]):
                lq[i] = qslerp(lq[i], le[i], w_)
    if u > 0.05:
        # (his hand lies on the steel, not in it, and passes over its brim, not through it)
        G, X = man.fk(lq, mv)
        dep = over_crown(body, G, X, body.Z["hand_l"]) if u > 0.3 else 0.0
        dep = max(dep, inside_depth(body, G, X, body.Z["hand_l"], head_s) + 0.003)
        dep = min(0.04, dep) * min(1.0, (u - 0.05) / 0.15)
        if dep > 0.0035:
            ik2(man, lq, mv, ARM["l"], X[H_] + (G[h] @ Z_) * dep, end_R=G[H_], share=0.4)
    # (his fingers open off the weapon and flat on the steel)
    fl, tl = b("fingers_l"), b("thumb_l")
    flat_f = R_axis(np.cross(A["d0"], A["n0"]), 18.0)
    flat_t = R_axis(np.cross(A["thumb0"], A["n0"]), 10.0)
    lq[fl] = qslerp(base.lq[fl], flat_f, hu)
    lq[tl] = qslerp(base.lq[tl], flat_t, hu)
    # (his elbows on the ground as his shoulders go down: swung out about shoulder-to-wrist, the hands where they are,
    # never pressed deeper than fire_prone has them; each always the same way, and no faster than ELBOW_DEG_S)
    if probe:
        return lq, mv
    env = elbow_env(body)
    for s in "lr":
        g_, a_, sg_ = env[s]
        ang = float(np.interp(t, g_, a_))
        if ang > 1e-3:
            lq = swung_arm(body, lq, mv, s, sg_ * ang)
    if body.armed:
        lq, mv = gun_rides(body, lq, mv, base.rel["r"])
    lq, mv = keep_prop(body, lq, mv, base.prop)
    lq = steady_whip(body, lq, mv, base.G)
    return lq, mv


ELBOW_GIVE = (0.0, 0.008, 0.016, 0.024)   # (m: how far hit_flinch_prone may let an elbow into the ground, in turn)
ELBOW_DEG_S = 540.0          # (degrees a second: the most hit_flinch_prone swings an elbow out to keep it off the ground)


def swung_arm(body, lq, mv, s, ang):
    """Arm s swung `ang` degrees about the line from its shoulder to its wrist, the hand keeping its place and turn."""
    man = body.man
    U_, F_, H_ = (man.b(n) for n in ARM[s])
    G, X = man.fk(lq, mv)
    l2 = turn_world(man, lq.copy(), mv, ARM[s][0], R_axis(unit(X[H_] - X[U_]), ang))
    l2[H_] = man.fk(l2, mv)[0][F_].T @ G[H_]
    return l2


def elbow_need(body, lq, mv, s, floor, sg, most=45.0, step=1.0):
    """The least swing (degrees, the way sg) of arm s about shoulder-to-wrist that keeps its forearm at or over
    `floor`; where none up to `most` does, the one that brings it highest."""
    mask = body.Z["fore_" + s]
    z = zone_low(body, lq, mv, mask)[0]
    if z >= floor - 2e-4:
        return 0.0
    best = (z, 0.0)
    for k in range(1, int(most / step) + 1):
        z = zone_low(body, swung_arm(body, lq, mv, s, sg * k * step), mv, mask)[0]
        if z >= floor - 2e-4:
            return k * step
        if z > best[0]:
            best = (z, k * step)
    return best[1]


def elbow_env(body):
    """hit_flinch_prone's elbow swings, found once: for each arm the way that needs the least, and at each 60th of a
    second how far (at least what keeps the forearm off the ground, swung sooner and back later, never faster than
    ELBOW_DEG_S; from and back to none at the ends, so at the very first frames a little less than that)."""
    key = ("elbow_env",)
    if key in body.poses:
        return body.poses[key]
    man = body.man
    L = PRONE_LONG
    g = np.array(dense_times(L))
    base = body.pose("fire_prone", 0.0)
    poses = [prone_flinch_pose(body, float(t), probe=True) for t in g]
    out = {}
    arm_keys = ("sh_", "el_", "pron", "sup", "wr_")

    def env_of(need):
        a = need.copy()
        for k in range(1, len(g) - 1):
            a[k] = max(a[k], a[k - 1] - ELBOW_DEG_S * (g[k] - g[k - 1]))
        for k in range(len(g) - 2, 0, -1):
            a[k] = max(a[k], a[k + 1] - ELBOW_DEG_S * (g[k + 1] - g[k]))
        return np.minimum(a, ELBOW_DEG_S * np.minimum(g, L - g))

    def over(s, a, sg):
        ps = [(swung_arm(body, lq, mv, s, sg * x) if x > 1e-3 else lq, mv) for x, (lq, mv) in zip(a, poses)]
        return sum(o for k_, s2, o in pose_rom(body, ps) if s2 == s and k_.startswith(arm_keys))
    for s in "lr":
        fl = min(zone_low(body, base[0], base[1], body.Z["fore_" + s])[0], 0.004)
        # (kept off the ground where a living arm can be swung so; where it cannot, his elbow let down into the soft
        # ground a little (ELBOW_GIVE at the most) rather than his shoulder or wrist turned past their range)
        best = None
        for give in ELBOW_GIVE:
            needs = {sg: np.array([elbow_need(body, lq, mv, s, fl - give, sg) if 0.0 < t < L else 0.0
                                   for t, (lq, mv) in zip(g, poses)]) for sg in (1.0, -1.0)}
            for sg in sorted((1.0, -1.0), key=lambda x: (needs[x].max(), -x)):
                a = env_of(needs[sg])
                o = over(s, a, sg) if a.max() > 1e-3 else 0.0
                if best is None or o < best[0] - 1e-6:
                    best = (o, a, sg)
                if o <= 0.0:
                    break
            if best[0] <= 0.0:
                break
        out[s] = (g, best[1], best[2])
    body.poses[key] = out
    return out


def elbow_clear(body, lq, mv, s, floor, most=45.0, step=1.0):
    """Arm s swung about the line from its shoulder to its wrist, the least that keeps its forearm (elbow) at or over
    `floor`: the hand keeps its place and its turn."""
    man = body.man
    mask = body.Z["fore_" + s]
    if zone_low(body, lq, mv, mask)[0] >= floor - 2e-4:
        return lq
    U_, F_, H_ = (man.b(n) for n in ARM[s])
    G, X = man.fk(lq, mv)
    ax = unit(X[H_] - X[U_])
    Gh = G[H_].copy()
    best = (zone_low(body, lq, mv, mask)[0], lq)
    for k in range(1, int(most / step) + 1):
        for sg in (1.0, -1.0):
            l2 = turn_world(man, lq.copy(), mv, ARM[s][0], R_axis(ax, sg * k * step))
            G2 = man.fk(l2, mv)[0]
            l2[H_] = G2[F_].T @ Gh
            z = zone_low(body, l2, mv, mask)[0]
            if z >= floor - 2e-4:
                return l2
            if z > best[0]:
                best = (z, l2)
    return best[1]


# ================================================================================================ baking
def bake(body, fn, ts, like=None):
    """A clip's tracks from a pose function at the key times `ts` (rotations rounded to 5 places, moves to 4, as
    field_clips.bake writes them). A bone gets a rotation track when it turns at all at any key or when a clip of
    `like` (the base clips) has one; a move track when it moves."""
    man = body.man
    nb = man.nb
    rot = {i: [] for i in range(nb) if man.par[i] is not None}
    move = {i: [] for i in range(nb) if man.par[i] is not None}
    prev = {}
    poses = []
    for t in ts:
        lq, mv = fn(t)
        poses.append((t, lq.copy(), mv.copy()))
        for i in rot:
            q = quat(lq[i])
            if i in prev and float(q @ prev[i]) < 0:
                q = -q
            prev[i] = q
            rot[i].append([round(float(t), 4)] + [round(float(v), 5) for v in q])
            move[i].append([round(float(t), 4)] + [round(float(v), 4) for v in mv[i]])
    want_rot, want_move = set(), set()
    for c in like or ():
        for tr in body.C[c]["tracks"]:
            if "rot" in tr:
                want_rot.add(tr["part"])
            if "move" in tr:
                want_move.add(tr["part"])
    tracks = []
    for i in range(nb):
        if man.par[i] is None:
            continue
        tr = {"part": i, "part_name": man.rig.names[i]}
        if i in want_rot or any(max(abs(v) for v in k[2:]) > 1e-5 for k in rot[i]):
            tr["rot"] = rot[i]
        if i in want_move or any(max(abs(v) for v in k[1:]) > 1e-4 for k in move[i]):
            tr["move"] = move[i]
        if len(tr) > 2:
            tracks.append(tr)
    return tracks, poses


# ================================================================================================ pictures
PALETTE = {0: (98, 104, 92), 1: (90, 95, 86), 2: (44, 38, 34), 3: (50, 46, 40), 4: (58, 48, 40), 5: (84, 92, 84),
           6: (112, 104, 82), 7: (196, 158, 132), 8: (112, 72, 44), 9: (58, 60, 66), 10: (98, 104, 92),
           11: (74, 80, 72), 12: (70, 54, 42), 13: (88, 76, 64), 14: (44, 38, 34)}
GROUND = (150, 158, 124)
GRID = (132, 140, 108)
SHADOW = (112, 120, 92)


class Look:
    """One level of detail of the man, for drawing."""

    def __init__(self, body, lod="mid"):
        M = body.D[lod]
        self.P = np.array(M["points"], float).reshape(-1, 3)
        self.a, self.b = np.array(M["part"]), np.array(M["part2"])
        self.w = np.array(M["w2"], float)
        self.T = np.array(M["triangles"]).reshape(-1, 3)
        pa = np.array(M["paints"])
        sh = np.array(M["shade"], float) if "shade" in M else np.ones(len(self.P))
        col = np.array([PALETTE.get(int(p), (120, 120, 110)) for p in pa], float)
        self.col = col * (0.6 + 0.4 * sh)[:, None]
        man = body.man
        gun = [man.b(n) for n in GUN_BONES if n in man.ix and not (n == "bolt" and body.bolt_prop)]
        self.keep = np.ones(len(self.T), bool) if body.armed else ~np.isin(self.a, gun)[self.T].any(1)
        self.H = man.H

    def skin(self, G, X):
        return gore.skin(self.P, self.a, self.b, self.w, G, X, self.H)


def camera(az, pitch, dist, look):
    """az 0: from straight behind him, + round to his right; pitch: degrees the camera looks down."""
    a, p = math.radians(az), math.radians(pitch)
    back = np.array((-math.cos(a), math.sin(a), 0.0))
    look = np.asarray(look, float)
    eye = look + back * dist * math.cos(p) + Z_ * dist * math.sin(p)
    f = unit(look - eye)
    r = unit(np.cross(Z_, f))
    u = np.cross(f, r)
    return eye, f, r, u


def render(look, V, cam, size=(250, 230), fov=30.0, light=(0.35, 0.45, 0.82)):
    from PIL import Image, ImageDraw
    Wd, Ht = size
    eye, f, r, u = cam
    k = (Wd / 2) / math.tan(math.radians(fov) / 2)

    def proj(Q):
        d = Q - eye
        z = np.maximum(d @ f, 0.05)
        return np.stack([Wd / 2 + k * (d @ r) / z, Ht / 2 - k * (d @ u) / z], -1), z
    img = Image.new("RGB", size, GROUND)
    dr = ImageDraw.Draw(img)
    c0 = np.round(V[:, :2].mean(0) * 2) / 2
    for g in np.arange(-3.0, 3.01, 0.5):
        for a, b in (((c0[0] + g, c0[1] - 3, 0.0), (c0[0] + g, c0[1] + 3, 0.0)),
                     ((c0[0] - 3, c0[1] + g, 0.0), (c0[0] + 3, c0[1] + g, 0.0))):
            s, z = proj(np.array((a, b), float))
            dr.line([tuple(s[0]), tuple(s[1])], fill=GRID, width=1)
    T = look.T[look.keep]
    # (his shadow straight down on the ground: where he touches it and where he does not)
    Vs = V.copy()
    Vs[:, 2] = 0.0
    s, z = proj(Vs)
    for t in T[::2]:
        dr.polygon([tuple(s[t[0]]), tuple(s[t[1]]), tuple(s[t[2]])], fill=SHADOW)
    s, z = proj(V)
    n = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    cen = V[T].mean(1)
    flip = np.einsum("ij,ij->i", n, eye - cen) < 0
    n[flip] *= -1
    Lv = unit(light)
    lit = 0.45 + 0.55 * np.clip(n @ Lv, 0, 1)
    col = look.col[T[:, 0]] * lit[:, None]
    order = np.argsort(-z[T].max(1))
    for i in order:
        t = T[i]
        c = col[i]
        dr.polygon([tuple(s[t[0]]), tuple(s[t[1]]), tuple(s[t[2]])], fill=(int(c[0]), int(c[1]), int(c[2])))
    return img


VIEWS = {"side": dict(az=90.0, pitch=4.0), "rear": dict(az=0.0, pitch=45.0), "front": dict(az=180.0, pitch=8.0),
         "top": dict(az=0.0, pitch=80.0), "left": dict(az=-90.0, pitch=4.0), "q": dict(az=55.0, pitch=22.0)}


def strip(body, look, clip, times, views=("side", "rear"), size=(250, 230), dist=5.2, follow=True, label=None,
          centre=None):
    """A clip's frames at `times`, a row a view; the camera on the man's middle (following him, or held)."""
    from PIL import Image, ImageDraw
    man = body.man
    cells = []
    fr = [gore.pose(man.rig, clip, t) for t in times]
    mids = [0.5 * (X[man.b("pelvis")] + X[man.b("chest")]) for G, X in fr]
    if centre is None:
        centre = np.mean(mids, 0)
    out = Image.new("RGB", (size[0] * len(times), (size[1] + 14) * len(views) + 16), (236, 232, 222))
    d = ImageDraw.Draw(out)
    d.text((4, 2), label or "", fill=(20, 20, 20))
    for vi, v in enumerate(views):
        for k, (t, (G, X)) in enumerate(zip(times, fr)):
            at = mids[k] if follow else centre
            at = np.array((at[0], at[1], 0.75 if follow else centre[2]))
            cam = camera(VIEWS[v]["az"], VIEWS[v]["pitch"], dist, at)
            im = render(look, look.skin(G, X), cam, size)
            x, y = k * size[0], 16 + vi * (size[1] + 14)
            out.paste(im, (x, y + 14))
            d.text((x + 4, y + 1), f"{v}  {t:.2f} s", fill=(40, 40, 40))
    del cells
    return out


def sheet(strips, path, cols=1):
    from PIL import Image
    w = max(s.size[0] for s in strips)
    rows = [strips[i:i + cols] for i in range(0, len(strips), cols)]
    hs = [max(s.size[1] for s in r) for r in rows]
    out = Image.new("RGB", (w * cols, sum(hs)), (236, 232, 222))
    y = 0
    for r, h in zip(rows, hs):
        for k, s in enumerate(r):
            out.paste(s, (k * w, y))
        y += h
    os.makedirs(os.path.dirname(path), exist_ok=True)
    out.save(path)
    return path


# ================================================================================================ keyed poses
def turn(*steps):
    """A turn built from steps (axis letter, degrees), each about the turned body's own axis, in order."""
    R = np.eye(3)
    for ax, deg in steps:
        R = R @ R_axis({"x": X_, "y": Y_, "z": Z_}[ax], deg)
    return R


def zone_low(body, lq, mv, mask):
    """The lowest point of the near mesh's points in `mask`: (its height, its place), and each bone's place."""
    man = body.man
    G, X = man.fk(lq, mv)
    V = gore.skin(body.P[mask], body.pa[mask], body.pb[mask], body.pw[mask], G, X, man.H)
    k = int(V[:, 2].argmin())
    return float(V[k, 2]), V[k], X


def chain_to_ground(body, lq, mv, root, mask, both=False, clear=0.0, most=40.0, post=None, tries=5, hinge=False):
    """Bone `root` (a thigh, an upper arm, the neck) turned about its own head so the lowest point of what it
    carries (`mask`) comes up to the ground (`clear` over it); `both`: or down onto it when it floats. A point right
    under the joint cannot be turned out (the trunk must lift): left as it is. Returns how far it was under."""
    man = body.man
    r_i = man.b(root)
    worst = 0.0
    done = 0.0
    for k in range(tries):
        if post is not None:
            lq, mv = post(lq, mv)
        z, p, X = zone_low(body, lq, mv, mask)
        if k == 0:
            worst = min(0.0, z - clear)
        if abs(z - clear) < 4e-4 or (z > clear and not both):
            break
        r = p - X[r_i]
        rh = math.hypot(r[0], r[1])
        if rh < 0.06:
            break
        if hinge is not False:
            # (a shin turns about its knee's hinge only, a forearm about its elbow's)
            ax = man.fk(lq, mv)[0][man.par[r_i]] @ (Y_ if hinge is True else hinge)
            lever = float(np.cross(ax, r)[2])
            if abs(lever) < 0.03:
                break
            R1 = R_axis(ax, 1.0)
            sgn = 1.0 if float((R1 @ r - r)[2]) * lever > 0 else -1.0
            ang = sgn * math.degrees((clear - z) / abs(lever)) * (1.0 if float((R1 @ r - r)[2]) > 0 else -1.0) * sgn
            ang = max(-most - done, min(most - done, ang))
            done += ang
            lq = turn_world(man, lq, mv, root, R_axis(ax, ang))
            continue
        ang = math.degrees((clear - z) / rh)
        ang = max(-most - done, min(most - done, ang))
        done += ang
        lq = turn_world(man, lq, mv, root, R_axis((r[1] / rh, -r[0] / rh, 0.0), ang))
    if post is not None:
        lq, mv = post(lq, mv)
    return lq, mv, worst


def foot_clear(body, lq, mv, s, clear=0.0):
    """A foot whose toes or heel are under the ground: its toes bent up at the ball of the foot first, then (what
    that cannot clear) the foot turned at the ankle inside the ankle's range, the least that clears them. Each turn is
    found to a quarter of a degree and the toes are not let go when the ankle takes over, so the turn grows and
    shrinks smoothly from one frame to the next instead of jumping between ways or coarse steps."""
    man = body.man
    mask = body.Z["foot_" + s]
    f, bl = man.b("foot_" + s), man.b("ball_" + s)
    z0 = zone_low(body, lq, mv, mask)[0]
    if z0 >= clear - 5e-4:
        return lq
    ok = lambda z: z >= clear - 5e-4

    def ankle(base, d):
        l2 = base.copy()
        l2[f] = base[f] @ Ry(float(d))
        return l2 if ANKLE[0] <= ankle_deg(body, l2, s) <= ANKLE[1] else None

    def toe(base, d):
        l2 = base.copy()
        l2[bl] = base[bl] @ Ry(float(d))
        return l2 if angle_of(l2[bl]) <= 56.0 else None

    def least(turn, base, sgn, most):
        """(the least turn that clears, its pose) or (None, the pose that came highest, how high)."""
        lo, top = 0.0, (zone_low(body, base, mv, mask)[0], base)
        for d in range(4, most + 1, 4):
            l2 = turn(base, sgn * d)
            if l2 is None:
                break
            z = zone_low(body, l2, mv, mask)[0]
            if z > top[0]:
                top = (z, l2)
            if ok(z):
                hi, hl = float(d), l2
                for _ in range(4):
                    mid = 0.5 * (lo + hi)
                    lm = turn(base, sgn * mid)
                    if lm is not None and ok(zone_low(body, lm, mv, mask)[0]):
                        hi, hl = mid, lm
                    else:
                        lo = mid
                return hi, hl, None
            lo = float(d)
        return None, top[1], top[0]
    d, l2, z = least(toe, lq, -1, 50)
    if d is not None:
        return l2
    base = l2 if z > z0 + 1e-4 else lq
    best = (max(z, z0), base)
    found = []
    for sgn, most in ((1, 72), (-1, 48)):
        d, l3, z3 = least(ankle, base, sgn, most)
        if d is not None:
            found.append((d, l3))
        elif z3 > best[0]:
            best = (z3, l3)
    if found:
        return min(found, key=lambda c: c[0])[1]
    return best[1]


def grounded(body, lq, mv, post=None, clear=0.002, lift=True, limbs=True, shin_first=False):
    """Nothing left under the ground: his trunk lifted clear, a foot turned at the ankle, each limb and his head
    turned up at its own joint; what is clear of the ground is not touched. What no joint can turn out of it (a
    knee under a low hip): all of him lifted, last."""
    if lift:
        lq, mv, _ = trunk_lift(body, lq, mv, clear)
    if post is not None:
        lq, mv = post(lq, mv)
    for s in ("lr" if limbs else ""):
        if shin_first:
            # (a foot behind a knee that is down: its shin turned up at the knee first, a little at a time, so the
            # foot is not flicked over at the ankle when its toes touch)
            lq, mv, _ = chain_to_ground(body, lq, mv, "calf_" + s, body.Z["foot_" + s], clear=clear, most=60.0,
                                        hinge=True)
        lq = foot_clear(body, lq, mv, s, clear)
        lq, mv, _ = chain_to_ground(body, lq, mv, "thigh_" + s, body.far["leg_" + s], clear=clear, most=70.0)
        lq, mv, _ = chain_to_ground(body, lq, mv, "calf_" + s, body.Z["foot_" + s], clear=clear, most=60.0,
                                    hinge=True)
        lq = foot_clear(body, lq, mv, s, clear)
    for s in "lr":
        mask = body.far["arm_" + s]
        if s == "r" and body.armed:
            mask = mask | body.is_gun
        lq, mv, _ = chain_to_ground(body, lq, mv, "upperarm_" + s, mask, clear=clear, post=post if s == "r" else None)
    lq, mv, _ = chain_to_ground(body, lq, mv, "neck", body.far["head"], clear=clear)
    if lift:
        z = zone_low(body, lq, mv, ~body.is_whip)[0]
        if z < clear - 1e-3:
            pe = body.man.b("pelvis")
            mv[pe] = mv[pe] + Z_ * (clear - z)
            if post is not None:
                lq, mv = post(lq, mv)
    return lq, mv


def trunk_lift(body, lq, mv, clear=0.0, both=False, zones=("trunk", "head")):
    """His whole body lifted so his trunk (and head) is not under the ground; `both`: or let down onto it."""
    mask = body.Z[zones[0]]
    for z_ in zones[1:]:
        mask = mask | body.Z[z_]
    z, p, X = zone_low(body, lq, mv, mask)
    pe = body.man.b("pelvis")
    if z < clear or both:
        mv[pe] = mv[pe] + Z_ * (clear - z)
    return lq, mv, min(0.0, z - clear)


def rest_trunk(body, lq, mv, rolls=(0.0,), head=False):
    """A lying man's trunk set on the ground. Of the `rolls` tried (degrees about his own length: his kit, a pack, a
    canister on his belt, will not let every man lie flat), the one that lets him lie lowest; then tipped head-up or
    head-down about his hips and lifted, the least that rests both his hips and his chest (or what he carries on
    them; with `head`, his helmet too) on the ground. Returns the pose, the roll and the tilt taken."""
    man = body.man
    pe, ch = man.b("pelvis"), man.b("chest")
    G, X = man.fk(lq, mv)
    along = unit(X[ch] - X[pe])
    flat = along.copy()
    flat[2] = 0.0
    ax = unit(np.cross(Z_, unit(flat)))                  # (a turn about it lifts his chest)
    mh = body.Z["hips"]
    mc = body.Z["chest"] | (body.Z["head"] if head else False)
    P, a, b, w, H = body.P, body.pa, body.pb, body.pw, man.H
    Vh = gore.skin(P[mh], a[mh], b[mh], w[mh], G, X, H) - X[pe]
    Vc = gore.skin(P[mc], a[mc], b[mc], w[mc], G, X, H) - X[pe]
    c0 = X[ch] - X[pe]
    best = None
    for roll in rolls:
        Rr = R_axis(along, roll)
        for tilt in np.arange(-24.0, 24.01, 0.5):
            R = R_axis(ax, -tilt) @ Rr
            zh = float((Vh @ R.T)[:, 2].min())
            zc = float((Vc @ R.T)[:, 2].min())
            lift = max(-zh, -zc)
            cost = abs(zh + lift) + abs(zc + lift) + 0.0015 * abs(tilt)
            # (how high he lies: his hips' and his chest's joints over the ground)
            high = 2 * lift + float((R @ c0)[2])
            score = cost * 3.0 + high + 0.0012 * abs(roll)
            if best is None or score < best[0]:
                best = (score, float(roll), float(tilt), float(lift), R)
    _, roll, tilt, lift, R = best
    lq[pe] = R @ G[pe]
    mv[pe] = mv[pe] + Z_ * (lift - X[pe][2])
    return lq, mv, (roll, tilt)


def gun_flat(body, at, yaw, side=1.0, clear=0.004, tip=0.0):
    """The weapon lying on the ground on its side: its grip-end at `at` (x, y), its muzzle toward `yaw` (degrees, +
    to his right of +x), `side` +1 its right side down; returns (turn, place) of the weapon bone."""
    man = body.man
    Gw = Rz(yaw) @ Ry(-tip) @ Rx(-88.0 * side)
    W = man.weapon_pts - man.H[body.wi]
    if len(W) == 0:
        return Gw, np.array((at[0], at[1], 0.03))
    low = float((W @ Gw.T)[:, 2].min())
    return Gw, np.array((at[0], at[1], clear - low))


def pose_of(body, sp):
    """A whole pose from a short description: `at` the pelvis's place, `pel` its turn (steps), s1 s2 ch nk hd cl cr
    the spine's, neck's, head's and collar bones' own turns (pitch, roll, yaw), leg_l / leg_r (hip forward, out,
    turned out, knee, foot down), arm_l / arm_r (at, pole, d fingers along, t thumb toward; `rel` sh: from the
    shoulder in the world's axes, chest: from the shoulder in the chest's axes, floor: x and y from the pelvis and z
    over the ground), fin_l / fin_r (curl, thumb), gun: None (it rides his right hand as `grip` has it) or a place
    on the ground the right hand is solved onto. `rest`: a lying pose set down on the ground."""
    man = body.man
    b = man.b
    if sp.get("like") is not None:
        lq, mv = sp["like"][0].copy(), sp["like"][1].copy()
    else:
        lq, mv = body.pose("idle", 0.0)
        for i in range(man.nb):
            if man.par[i] is not None and man.rig.names[i] not in GUN_BONES and i not in body.whip:
                lq[i] = np.eye(3)
    pe = b("pelvis")
    if "pel" in sp:
        lq[pe] = turn(*sp["pel"])
    if sp.get("pel_roll"):
        lq[pe] = Rx(sp["pel_roll"]) @ lq[pe]
    if "at" in sp:
        mv[pe] = np.asarray(sp["at"], float) - man.H[pe]
    for key, bone in (("s1", "spine_01"), ("s2", "spine_02"), ("ch", "chest"), ("nk", "neck"), ("hd", "head"),
                      ("cl", "clavicle_l"), ("cr", "clavicle_r")):
        if key in sp:
            lq[b(bone)] = euler(*sp[key])
    for s, sg in (("l", -1.0), ("r", 1.0)):
        if "leg_" + s in sp:
            flex, abd, rot, knee, foot = sp["leg_" + s][:5]
            lq[b("thigh_" + s)] = Rx(sg * abd) @ Ry(-flex) @ Rz(sg * rot)
            lq[b("calf_" + s)] = Ry(knee)
            lq[b("foot_" + s)] = Ry(foot)
            if "ball_" + s in man.ix:
                lq[b("ball_" + s)] = Ry(sp["leg_" + s][5] if len(sp["leg_" + s]) > 5 else 0.0)
    rest = sp.get("rest")
    if rest:
        lq, mv, _ = rest_trunk(body, lq, mv, **rest)
    if sp.get("sit"):
        lq, mv, _ = trunk_lift(body, lq, mv, 0.0, both=True, zones=("trunk",))
    for s in "lr":
        if "foot_" + s in sp:
            F = sp["foot_" + s]
            G, X = man.fk(lq, mv)
            T = np.array((X[pe][0] + F["at"][0], X[pe][1] + F["at"][1], F["at"][2])) if F.get("rel", "abs") == "floor" \
                else np.asarray(F["at"], float)
            ik2(man, lq, mv, LEG[s], T, pole=F.get("pole"), end_R=F.get("R"))
    for s in "lr":
        if "arm_" + s not in sp:
            continue
        A = sp["arm_" + s]
        G, X = man.fk(lq, mv)
        S = X[b("upperarm_" + s)]
        rel = A.get("rel", "sh")
        Gc = G[b("chest")]
        fr = Gc if rel == "chest" else np.eye(3)
        if rel == "floor":
            T = np.array((X[pe][0] + A["at"][0], X[pe][1] + A["at"][1], A["at"][2]))
        elif rel == "abs":
            T = np.asarray(A["at"], float)
        else:
            T = S + fr @ np.asarray(A["at"], float)
        if "gun" in A:
            # (the hand turned as it holds the weapon, the weapon along `gun`[0] with its top toward `gun`[1])
            Gw = frame_map(X_, Z_, fr @ unit(A["gun"][0]), fr @ unit(A["gun"][1]))
            Gh = Gw @ sp.get("grip", body.rel0["r"])[0]
            d_, t_ = Gh @ man.side[s]["d0"], Gh @ man.side[s]["t0"]
        else:
            d_, t_ = fr @ unit(A["d"]), fr @ unit(A["t"])
        lq = man.arm(lq, mv, s, T, fr @ np.asarray(A["pole"], float), d_, t_, share=A.get("share", 0.5))
        if A.get("plant"):
            # (a hand set on the ground: its wrist raised until palm and fingers lie on it, not in it)
            for _ in range(4):
                z = zone_low(body, lq, mv, body.Z["hand_" + s])[0]
                if abs(z - 0.003) < 1e-3:
                    break
                T = T + Z_ * (0.003 - z)
                lq = man.arm(lq, mv, s, T, fr @ np.asarray(A["pole"], float), d_, t_, share=A.get("share", 0.5))
    for s in "lr":
        if "fin_" + s in sp:
            lq = man.fingers(lq, s, *sp["fin_" + s])
    rel_r = sp.get("grip", body.rel0["r"])
    g = sp.get("gun")
    if g is not None:
        G, X = man.fk(lq, mv)
        if "hand" in g:
            # (the weapon lying where his right wrist is wanted, at `hand` (x, y) on the ground)
            W = gun_flat(body, (0.0, 0.0), g.get("yaw", 0.0), g.get("side", 1.0), tip=g.get("tip", 0.0))
            at = np.asarray(g["hand"][:2], float) - (W[0] @ rel_r[1])[:2]
        else:
            at = (X[pe][0] + g["at"][0], X[pe][1] + g["at"][1])

        def lay(dyaw, dy=0.0, dx=0.0):
            W = gun_flat(body, (at[0] + dx, at[1] + dy), g.get("yaw", 0.0) + dyaw, g.get("side", 1.0),
                         tip=g.get("tip", 0.0))
            if g.get("z") is not None:
                W = (W[0], np.array((W[1][0], W[1][1], g["z"])))
            l2, m2 = lq.copy(), mv.copy()
            ik2(man, l2, m2, ARM["r"], W[1] + W[0] @ rel_r[1], end_R=W[0] @ rel_r[0], share=0.4)
            l2 = arm_ease(body, l2, m2, "r", turn_hand=False)
            G2, X2 = man.fk(l2, m2)
            U_, F_, H_ = (b(n) for n in ARM["r"])
            c = arm_over(arm_read(body, "r", G2[b("chest")], G2[U_], G2[H_], X2[U_], X2[F_], X2[H_]))
            return c + 0.02 * dyaw * dyaw + 400.0 * dy * dy + 150.0 * dx * dx, l2
        best = lay(0.0)
        if best[0] > 1e-9 and ("hand" not in g or g.get("search")):
            # (laid where his right hand is, `search`: turned on the ground the least that lets the wrist hold it
            # inside a wrist's range, not moved)
            for dx in (0.0,):
                for dy in ((0.0, -0.06, 0.06, 0.12) if "hand" not in g else (0.0,)):
                    for dw in (0.0, -8.0, 8.0, -16.0, 16.0, -24.0, 24.0, -32.0, 32.0):
                        if best[0] > 1e-9 and (dw or dy or dx):
                            c = lay(dw, dy, dx)
                            if c[0] < best[0]:
                                best = c
        lq = best[1]
    post = lambda l, m: keep_prop(body, *gun_rides(body, l, m, rel_r), body.prop0)
    lq, mv = post(lq, mv)

    def ease(lq, mv):
        # (a living man's arms and ankles: the right hand is not turned where the weapon it holds is laid down)
        if not sp.get("ease", True):
            return lq, mv
        for s in "lr":
            if sp.get("arm_" + s, {}).get("plant") or sp.get("arm_" + s, {}).get("fixed"):
                # (a hand set on the ground, or held where another key has it, is left where and as it is: only its
                # elbow swung to where the arm can be)
                lq = arm_ease(body, lq, mv, s)
                if sp["arm_" + s].get("plant"):
                    # (turned at the wrist, it is set down on the ground again as it is now turned)
                    h = b("hand_" + s)
                    for _ in range(3):
                        z = zone_low(body, lq, mv, body.Z["hand_" + s])[0]
                        if abs(z - 0.003) < 1e-3:
                            break
                        G, X = man.fk(lq, mv)
                        ik2(man, lq, mv, ARM[s], X[h] + Z_ * (0.003 - z), end_R=G[h])
                lq = ankle_ease(body, lq, s)
                continue
            if "arm_" + s in sp or (s == "r" and g is not None):
                lq = arm_ease(body, lq, mv, s, turn_hand=not (s == "r" and g is not None))
                if not (s == "r" and g is not None):
                    lq = behind_ease(body, lq, mv, s)
                    if rest:
                        # (his forearm let down onto the ground by his elbow alone)
                        lq, mv, _ = chain_to_ground(body, lq, mv, "lowerarm_" + s, body.Z["fore_" + s], both=True,
                                                    clear=sp.get("clear", 0.003), most=50.0, hinge=body.hinge0[s])
                        lq = elbow_ease(body, lq, mv, s)
            lq = ankle_ease(body, lq, s)
        return post(lq, mv)
    lq, mv = ease(lq, mv)
    if rest and body.bolt_prop:
        # (set down again with what he carries on his chest in its place)
        lq, mv, _ = rest_trunk(body, lq, mv, **rest)
        lq, mv = post(lq, mv)
    for ch in sp.get("ground", ()):
        root = {"leg_l": "thigh_l", "leg_r": "thigh_r", "arm_l": "upperarm_l", "arm_r": "upperarm_r",
                "head": "neck", "shin_l": "calf_l", "shin_r": "calf_r", "fore_l": "lowerarm_l",
                "fore_r": "lowerarm_r", "knee_l": "thigh_l", "knee_r": "thigh_r"}[ch]
        mask = body.far.get(ch, body.Z.get(ch))
        if ch in ("arm_r", "fore_r") and body.armed:
            mask = mask | body.is_gun
        if ch == "head":
            # (his neck, then his head on it, each as far as a neck goes)
            lq, mv, _ = chain_to_ground(body, lq, mv, "neck", mask, both=True, clear=sp.get("clear", 0.003), most=22.0)
            lq, mv, _ = chain_to_ground(body, lq, mv, "head", body.pa == b("head"), both=True,
                                        clear=sp.get("clear", 0.003), most=22.0)
            continue
        lq, mv, _ = chain_to_ground(body, lq, mv, root, mask, both=True, clear=sp.get("clear", 0.003),
                                    post=post if ch in ("arm_r", "fore_r") else None, hinge=ch.startswith("shin"))
    if rest:
        # (anything of him still under the ground (his helmet's rim under his neck): all of him lifted, his limbs
        # let down again)
        lq, mv, under = trunk_lift(body, lq, mv, 0.0)
        if under < -5e-4:
            for ch in sp.get("ground", ()):
                if ch == "head":
                    continue
                root = {"leg_l": "thigh_l", "leg_r": "thigh_r", "arm_l": "upperarm_l", "arm_r": "upperarm_r"}[ch]
                mask = body.far[ch] | (body.is_gun if ch == "arm_r" and body.armed else False)
                lq, mv, _ = chain_to_ground(body, lq, mv, root, mask, both=True, clear=sp.get("clear", 0.003),
                                            post=post if ch == "arm_r" else None)
    if sp.get("clean", True):
        for _ in range(2):
            lq, mv = ease(lq, mv)
            lq, mv = grounded(body, lq, mv, post, clear=0.002, lift=sp.get("lift", True))
    return lq, mv


# ---- in-betweens: every bone's turn and move carried through the keys on a smooth curve
class Keys:
    """Key poses [(t, lq, mv, how)]; `how`: "" smooth through the key, "stop" at rest in it, "hit" arriving at speed
    and stopped (a landing), "go" leaving at speed from rest (struck)."""

    def __init__(self, body, keys):
        self.body = body
        self.t = np.array([k[0] for k in keys], float)
        self.how = [k[3] if len(k) > 3 else "" for k in keys]
        n, nb = len(keys), body.nb
        Q = np.zeros((n, nb, 4))
        for k, key in enumerate(keys):
            for i in range(nb):
                q = quat(key[1][i])
                if k and float(q @ Q[k - 1, i]) < 0:
                    q = -q
                Q[k, i] = q
        M = np.array([k[2] for k in keys])
        self.Q, self.M = Q, M
        self.dQ, self.dM = self._tangents(Q), self._tangents(M)
        self.fk = [body.man.fk(k[1], k[2]) for k in keys]

    def _tangents(self, V):
        n = len(self.t)
        tin, tout = np.zeros_like(V), np.zeros_like(V)
        for k in range(n):
            a, c = max(k - 1, 0), min(k + 1, n - 1)
            lin_in = (V[k] - V[a]) / max(self.t[k] - self.t[a], 1e-9) if k > 0 else np.zeros_like(V[k])
            lin_out = (V[c] - V[k]) / max(self.t[c] - self.t[k], 1e-9) if k < n - 1 else np.zeros_like(V[k])
            if 0 < k < n - 1:
                w = (self.t[k] - self.t[a]) / (self.t[c] - self.t[a])
                auto = lin_in * (1 - w) + lin_out * w
            else:
                auto = np.zeros_like(V[k])
            h = self.how[k]
            if h == "stop" or (k in (0, n - 1) and h == ""):
                continue
            if h == "hit":
                tin[k] = lin_in
            elif h == "go":
                tout[k] = lin_out
            elif h == "soft":
                tin[k] = tout[k] = 0.5 * auto
            else:
                tin[k] = tout[k] = auto
        return tin, tout

    def seg(self, t):
        k = int(np.clip(np.searchsorted(self.t, t, side="right") - 1, 0, len(self.t) - 2))
        return k, float(np.clip((t - self.t[k]) / (self.t[k + 1] - self.t[k]), 0.0, 1.0))

    def at(self, t):
        k, u = self.seg(t)
        h = self.t[k + 1] - self.t[k]
        h00, h10 = 2 * u ** 3 - 3 * u ** 2 + 1, u ** 3 - 2 * u ** 2 + u
        h01, h11 = -2 * u ** 3 + 3 * u ** 2, u ** 3 - u ** 2

        def herm(V, d):
            return h00 * V[k] + h10 * h * d[1][k] + h01 * V[k + 1] + h11 * h * d[0][k + 1]
        q = herm(self.Q, self.dQ)
        q = q / np.linalg.norm(q, axis=1, keepdims=True)
        lq = np.array([gore.qmat(x) for x in q])
        return lq, herm(self.M, self.dM)

    def held(self, t, bone):
        """Where bone's head and turn would be at t were it carried straight from key to key in the world."""
        k, u = self.seg(t)
        u = sm(u)
        (Ga, Xa), (Gb, Xb) = self.fk[k], self.fk[k + 1]
        return qslerp(Ga[bone], Gb[bone], u), Xa[bone] * (1 - u) + Xb[bone] * u


WRIST_IN = 0.05              # (s: a wrist eased into its range from a clip's first or last key over this long)


PIVOT_DEV = (-26.0, 16.0)    # (degrees: a hand bearing on the ground has its wrist kept between these sideways (rom.py's
#                              wr_dev: ulnar -, radial +; its limits -30 and 20), by turning on its palm (hand_pivot))


def hand_pivot(body, lq, mv, s, lim=PIVOT_DEV):
    """A hand bearing on the ground with its wrist bent sideways past `lim`: the hand turned about the upright through
    what of it bears on the ground (it pivots on its palm: that stays where it is), the least that brings the wrist
    back inside, the arm solved to it. A wrist inside, or a hand off the ground, is left as it is. Changes lq."""
    man = body.man
    U, F, Hd = (man.b(n) for n in ARM[s])
    ch = man.b("chest")

    def dev(l_):
        G_, X_l = man.fk(l_, mv)
        return arm_read(body, s, G_[ch], G_[U], G_[Hd], X_l[U], X_l[F], X_l[Hd])["wr_dev"]

    def excess(d):
        return d - lim[1] if d > lim[1] else d - lim[0] if d < lim[0] else 0.0
    d0 = dev(lq)
    if excess(d0) == 0.0:
        return lq
    G, X = man.fk(lq, mv)
    V = body.skin(G, X)
    on = body.Z["hand_" + s] & (V[:, 2] < 0.012)
    if on.sum() < 3:
        return lq
    c = V[on].mean(0)

    def turned(a):
        l2 = lq.copy()
        R = Rz(a)
        ik2(man, l2, mv, ARM[s], c + R @ (X[Hd] - c), end_R=R @ G[Hd])
        return l2
    k = (dev(turned(2.0)) - d0) / 2.0
    if abs(k) < 0.2:
        return lq
    a = float(np.clip(-excess(d0) / k, -35.0, 35.0))
    l2 = turned(a)
    e = excess(dev(l2))
    if e != 0.0:
        a = float(np.clip(a - e / k, -35.0, 35.0))
        l2 = turned(a)
    lq[:] = l2
    return lq


def gun_up(body, lq, mv, post, clear=0.002, most=40.0, step=2.0):
    """The weapon riding his right hand (post: carried on it) turned up or down at his wrist about the level line
    across it, the least that keeps it out of the ground; one that is out of it is left as it is. Returns (lq, mv)."""
    man = body.man
    if not body.armed or zone_low(body, lq, mv, body.is_gun)[0] >= clear - 1e-3:
        return lq, mv
    h, f = man.b("hand_r"), man.b("lowerarm_r")
    G, X = man.fk(lq, mv)
    ax = np.cross(G[body.wi] @ X_, Z_)
    if np.linalg.norm(ax) < 1e-6:
        return lq, mv
    ax = unit(ax)
    best = None
    for k in range(1, int(most / step) + 1):
        for sg in (1.0, -1.0):
            l2 = lq.copy()
            l2[h] = G[f].T @ R_axis(ax, sg * k * step) @ G[h]
            l2, m2 = post(l2, mv.copy())
            z = zone_low(body, l2, m2, body.is_gun)[0]
            if z >= clear:
                return l2, m2
            if best is None or z > best[0]:
                best = (z, l2, m2)
    return best[1], best[2]


def play(body, keys, t, pins=(), grip=None, ground=True, clear=0.002, lift=True, limbs=True, wrists=True,
         shin_first=False, one_turn=(), behind=True, locks=None, pivot=False, gun_clear=False):
    """The pose at t of a keyed move: the keys' in-between; the limbs in `pins` [(limb, from, to)] (arm_l, arm_r,
    leg_l, leg_r) carried straight through the world between the keys there (a planted hand or foot stays planted);
    the weapon riding his right hand; and nothing left under the ground."""
    man = body.man
    lq, mv = keys.at(t)
    rel = grip(t) if callable(grip) else (grip or body.rel0["r"])
    ride = lambda l, m: keep_prop(body, *gun_rides(body, l, m, rel), body.prop0)
    # (gun_clear: the weapon in his hand kept out of the ground by his wrist, not by lifting the whole of him off it,
    # each time it is carried on his hand)
    post = (lambda l, m: gun_up(body, *ride(l, m), ride, clear)) if gun_clear else ride
    n = len(keys.t)
    exact = any(abs(t - x) < 1e-6 for x in (keys.t[0], keys.t[-1]))
    if exact:
        return lq, mv
    # (the first and last keys are played as they are, so a wrist turned into its range in between is turned so by
    # degrees over WRIST_IN from either end, not all at once on the first frame)
    w_in = sm(min(1.0, min(t - keys.t[0], keys.t[-1] - t) / WRIST_IN))

    def wrist_eased(lq, s):
        l2 = arm_ease(body, lq.copy(), mv, s, swivel=False, slack=3.0, turn_hand=wrists)
        if w_in < 1.0:
            for i in range(man.nb):
                if not np.array_equal(l2[i], lq[i]):
                    l2[i] = qslerp(lq[i], l2[i], w_in)
        return l2

    def pinned(limb):
        return any(l == limb and t0 - 1e-6 <= t <= t1 + 1e-6 for l, t0, t1 in pins)

    def shift(limb):
        # (a planted hand, foot or knee: its joint moved over the ground the way that keeps what of it bears on the
        # ground where it is while the bones above it turn (contact_locks))
        for t0, t1, tt, S in (locks or {}).get(limb, ()):
            if t0 - 1e-6 <= t <= t1 + 1e-6:
                return np.array((np.interp(t, tt, S[:, 0]), np.interp(t, tt, S[:, 1]), 0.0))
        return np.zeros(3)

    def hand_turn(limb, h, Ge):
        # (a hand held on the ground keeps one turn while it bears, the mean of the keys' between the hold's first
        # and last (each key's wrist eased into its range turns it a little, and a hand turned on its spot slides what
        # touches); turned into it as it comes down over its first key gap, and out of it as it lifts over its last)
        for l, t0, t1 in pins:
            if l == limb and t0 - 1e-6 <= t <= t1 + 1e-6:
                ks = [k for k in range(n) if t0 - 1e-6 <= keys.t[k] <= t1 + 1e-6]
                if len(ks) < 3:
                    return Ge
                Rs = [keys.fk[k][0][h] for k in ks[1:-1]]
                R = Rs[0]
                for i, Ri in enumerate(Rs[1:], 2):
                    R = qslerp(R, Ri, 1.0 / i)
                a, b_ = keys.t[ks[1]], keys.t[ks[-2]]
                w = 1.0 if a <= t <= b_ else sm((t - t0) / (a - t0)) if t < a else sm((t1 - t) / (t1 - b_))
                return qslerp(Ge, R, w)
        return Ge

    def knee_held(t, s_):
        # (a knee on the ground goes straight from where the hold's first key has it to where its last has it, not
        # through the keys between: it stays on its spot as far as those two keys allow)
        ca = man.b("calf_" + s_)
        for l, t0, t1 in pins:
            if l == "knee_" + s_ and t0 - 1e-6 <= t <= t1 + 1e-6:
                a, c = keys.held(t0, ca)[1], keys.held(t1, ca)[1]
                u = min(1.0, max(0.0, (t - t0) / max(t1 - t0, 1e-9)))
                P = a * (1 - u) + c * u
                # (its height, though, from each key's between: each was let down onto the ground for the way the
                # thigh stands over it then, and the kneecap under a turning thigh sits higher or lower on the joint)
                ks = [k for k in range(n) if t0 - 1e-6 <= keys.t[k] <= t1 + 1e-6]
                for k0, k1 in zip(ks, ks[1:]):
                    if keys.t[k0] - 1e-6 <= t <= keys.t[k1] + 1e-6:
                        v = (t - keys.t[k0]) / max(keys.t[k1] - keys.t[k0], 1e-9)
                        P[2] = keys.fk[k0][1][ca][2] * (1 - v) + keys.fk[k1][1][ca][2] * v
                        break
                return P + shift("knee_" + s_)
        return keys.held(t, ca)[1]

    def pin(limb):
        # (a hand or foot carried straight from where one key has it to where the next has it, and over the ground;
        # a knee on the ground likewise, the thigh turned over it)
        s_ = limb[-1]
        if limb.startswith("knee"):
            if not pinned("leg_" + s_):
                G, X = man.fk(lq, mv)
                th, ca = man.b("thigh_" + s_), man.b("calf_" + s_)
                Kt = knee_held(t, s_)
                lq[:] = turn_world(man, lq, mv, "thigh_" + s_, _swing(unit(X[ca] - X[th]), unit(Kt - X[th])))
            return
        chain = ARM[s_] if limb.startswith("arm") else LEG[s_]
        Ge, Xe = keys.held(t, man.b(chain[2]))
        toe = None
        if limb.startswith("leg") and "ball_" + s_ in man.ix:
            # (a foot carried by the ball of the foot, not the ankle: as it rolls up onto its ball or down onto its
            # heel between two keys the ball stays on its line and the toes keep the keys' turn in the world)
            bl_, f_ = man.b("ball_" + s_), man.b(chain[2])
            Gb, Xb = keys.held(t, bl_)
            k_, _ = keys.seg(t)
            G0, X0 = keys.fk[k_]
            Xe = Xb - Ge @ (G0[f_].T @ (X0[bl_] - X0[f_]))
            toe = Gb
        Xe = Xe + shift(limb)
        if limb in one_turn:
            Ge = hand_turn(limb, man.b(chain[2]), Ge)
        knee = limb.startswith("leg") and pinned("knee_" + s_)

        def solve():
            if knee:
                knee_leg(body, lq, mv, s_, knee_held(t, s_), Xe, Ge)
            else:
                ik2(man, lq, mv, chain, Xe, end_R=Ge)
            if toe is not None:
                lq[man.b("ball_" + s_)] = Ge.T @ toe
        solve()
        zone = body.Z[("hand_" if limb.startswith("arm") else "foot_") + s_]
        for _ in range(4):
            z = zone_low(body, lq, mv, zone)[0]
            if not ground or z >= clear - 5e-4:
                break
            Xe = Xe + Z_ * (clear - z)
            solve()
        if pivot and limb.startswith("arm"):
            # (a hand bearing on the ground turns a little on its palm as the body moves over it, so the wrist stays
            # inside its range)
            hand_pivot(body, lq, mv, s_)
    for s in "lr":
        if behind:
            lq = behind_ease(body, lq, mv, s)
    # (a knee held on the ground: his pelvis at the height, and with two the roll, that lets the thighs reach them)
    Ks = {s: knee_held(t, s) for s in "lr" if pinned("knee_" + s)}
    if Ks:
        fit_hips(body, lq, mv, Ks)
    for limb, t0, t1 in pins:
        if t0 - 1e-6 <= t <= t1 + 1e-6:
            pin(limb)
    for s in "lr":
        if not pinned("leg_" + s):
            lq = ankle_ease(body, lq, s)
        if not pinned("arm_" + s):
            lq = wrist_eased(lq, s)
    lq, mv = post(lq, mv)
    if ground:
        lq, mv = grounded(body, lq, mv, post, clear, lift, limbs, shin_first)
        on = [(limb, t0, t1) for limb, t0, t1 in pins if lift and t0 - 1e-6 <= t <= t1 + 1e-6]
        pe = man.b("pelvis")
        for it in range(3 if on else 0):
            # (a planted limb put back where the lift took it from; a knee that leaves in the ground: he is lifted)
            for limb, t0, t1 in on:
                pin(limb)
            for s in "lr":
                if not pinned("arm_" + s):
                    lq = wrist_eased(lq, s)
            lq, mv = post(lq, mv)
            z = zone_low(body, lq, mv, ~body.is_whip)[0]
            if z >= clear - 2e-3 or it == 2:
                break
            mv[pe] = mv[pe] + Z_ * (clear - z)
        lq, mv = post(lq, mv)
    for s in "lr":
        lq = elbow_ease(body, lq, mv, s)
    lq, mv = post(lq, mv)
    del n
    return lq, mv


# ================================================================================================ lying, stunned
POSES = {}


def pose(fn):
    POSES[fn.__name__] = fn
    return fn


def cached(fn):
    def get(body, *a):
        k = (fn.__name__,) + a
        if k not in body.poses:
            body.poses[k] = fn(body, *a)
        lq, mv = body.poses[k]
        return lq.copy(), mv.copy()
    get.__name__ = fn.__name__
    return get


@pose
@cached
def face_rest(body):
    """Thrown onto his front: flat, his face turned to his left, the left arm bent up by his head, the right hand
    still on the weapon lying by his right side, his legs a little apart, the right knee bent out."""
    man = body.man

    def make(tw, hand_l=(0.26, -0.20, -0.10), gun_at=(0.62, 0.40), leg=(-3.0, 14.0), rot_r=16.0):
        return over_root(body, pose_of(body, dict(
            at=(0.0, 0.0, 0.2), pel=(("y", 90.0),), rest=dict(rolls=(-12.0, -8.0, -4.0, 0.0, 4.0, 8.0, 12.0)),
            s1=(-2.0, 0.0, 0.0), s2=(-3.0, 0.0, -2.0), ch=(-3.0, 0.0, -3.0),
            nk=(-12.0, 0.0, -24.0 * tw), hd=(-6.0, 0.0, -30.0 * tw),
            cl=(0.0, -6.0, 0.0), cr=(0.0, 4.0, 0.0),
            leg_l=(leg[0], 9.0, leg[1], 8.0, 44.0), leg_r=(2.0, 16.0, rot_r, 16.0, 42.0),
            arm_l=dict(rel="sh", at=hand_l, pole=(-0.25, -1.0, 0.15), d=(1.0, 0.45, -0.05), t=(0.1,
                1.0, 0.25)),
            arm_r=dict(rel="sh", at=(0.20, 0.26, -0.08), pole=(-0.4, 1.0, 0.2), d=(1.0, -0.2, -0.1), t=(0.0, -1.0,
                0.3)),
            fin_l=(28.0, 14.0),
            gun=dict(at=gun_at, yaw=-6.0, side=-1.0),
            ground=("leg_l", "leg_r", "arm_l", "arm_r", "head"))))

    def made(**kw):
        lq, mv = make(1.0, **kw)
        for _ in range(2):
            G = man.fk(lq, mv)[0]
            q = quat(G[man.b("chest")].T @ G[man.b("head")])
            turn_ = abs(math.degrees(2 * math.atan2(float(q[3]), float(q[0]))))
            turn_ = min(turn_, 360.0 - turn_)
            if turn_ <= 62.0:
                break
            lq, mv = make(max(0.2, (54.0 - (turn_ - 62.0)) / 54.0), **kw)
        return lq, mv
    # (as meant first; where this man's build or kit would put a shoulder or a hip past where it turns, the weapon
    # laid a little further out, the left hand lower by his head, the legs turned out less)
    tries = [dict()]
    for hl in ((0.26, -0.20, -0.10), (0.18, -0.30, -0.10), (0.10, -0.36, -0.10)):
        for ga in ((0.62, 0.40), (0.62, 0.48), (0.55, 0.45)):
            for rr in (16.0, 4.0):
                for leg in ((-3.0, 14.0), (5.0, 14.0), (5.0, 0.0), (-3.0, 0.0), (10.0, -10.0)):
                    kw = dict(hand_l=hl, gun_at=ga, leg=leg, rot_r=rr)
                    if kw != dict(hand_l=(0.26, -0.20, -0.10), gun_at=(0.62, 0.40), leg=(-3.0, 14.0), rot_r=16.0):
                        tries.append(kw)
    return first_in_range(body, [lambda kw=kw: made(**kw) for kw in tries])



@pose
@cached
def back_rest(body):
    """Thrown onto his back (his pack, if he has one, under his shoulders): his head toward where he was thrown,
    arms out from his sides, the right hand on the weapon lying by his right side, the right knee up a little."""
    def make(hand_l=(0.20, -0.36, -0.10), rot=(22.0, 28.0)):
        return over_root(body, pose_of(body, dict(
            at=(0.0, 0.0, 0.2), pel=(("y", -90.0),),
            rest=dict(rolls=(-30.0, -24.0, -18.0, -12.0, -6.0, 0.0, 6.0, 12.0, 18.0, 24.0, 30.0), head=True),
            s1=(3.0, 0.0, 0.0), s2=(3.0, 0.0, 2.0), ch=(2.0, 0.0, 2.0),
            nk=(4.0, 0.0, 12.0), hd=(-4.0, 0.0, 16.0),
            leg_l=(6.0, 10.0, rot[0], 10.0, 34.0), leg_r=(24.0, 15.0, rot[1], 44.0, 26.0),
            arm_l=dict(rel="sh", at=hand_l, pole=(0.1, -0.8, -0.6), d=(0.8, -0.6, 0.0), t=(0.3, 0.2, 1.0)),
            arm_r=dict(rel="sh", at=(0.36, 0.22, -0.08), pole=(0.0, 1.0, -0.3), d=(1.0, 0.2, 0.0), t=(0.0, -1.0,
                0.3)),
            fin_l=(34.0, 16.0),
            gun=dict(at=(0.12, 0.40), yaw=10.0, side=-1.0),
            ground=("leg_l", "leg_r", "arm_l", "arm_r", "head"))))
    # (as meant first; where his kit (a pack under his shoulders) would leave his left elbow under his back, that arm
    # laid further out from his side)
    tries = [dict()] + [dict(hand_l=hl, rot=rot) for hl in ((0.20, -0.36, -0.10), (0.16, -0.44, -0.10),
                                                            (0.08, -0.48, -0.12))
                        for rot in ((22.0, 28.0), (8.0, 14.0)) if (hl, rot) != ((0.20, -0.36, -0.10), (22.0, 28.0))]
    # (and with room: an elbow that a little swing in toward his side would put under his back is laid further out,
    # so the landing that comes down into this does not pass it under)
    return first_in_range(body, [lambda kw=kw: make(**kw) for kw in tries], also=lambda p_: elbow_tucked(body, p_, "l"))


def over_root(body, pose_):
    """A lying pose set with his pelvis exactly over the root (as the game has him after it moves the root by a
    knock-down's end_offset)."""
    m = pose_[1][body.man.b("pelvis")]
    return shifted(body, pose_, -float(m[0]), -float(m[1]))


def shifted(body, pose_, dx, dy=0.0):
    """A pose moved over the ground (everything on the root: his pelvis, his weapon)."""
    lq, mv = pose_[0].copy(), pose_[1].copy()
    for i in body.root_kids:
        mv[i] = mv[i] + np.array((dx, dy, 0.0))
    return lq, mv


# ================================================================================================ knocked flat
# A shell bursts close behind him (knock_down_face) or before him (knock_down_back): the blast takes his trunk first,
# his head and arms are left behind and then thrown after it, his feet leave the ground without any push of his own,
# he turns over in the air and lands on the ground trunk first, his legs after, and slides to a stop. The pelvis's
# path is a thrown body's: about 3.6 m/s along, a little up, then falling.
FACE_T = (0.0, 0.07, 0.20, 0.33, 0.43, 0.56, 0.70)
FACE_X = (0.0, 0.20, 0.68, 1.15, 1.48, 1.68, 1.76)
BACK_X = (0.0, -0.20, -0.66, -1.12, -1.44, -1.62, -1.70)
FLY_Z = (None, 0.968, 0.874, 0.60)
KF_T = (0.0, 0.07, 0.14, 0.25, 0.33, 0.42, 0.55, 0.70)     # (knock_down_face's keys: a fall, not a dive)
LAND_T = 0.5                 # (knock_down_back: the key between his seat striking and his shoulders down)
LAG_AT = (0.43, 0.86)        # (knock_down_face: how far his arms are on their way back at 0.07 and 0.14 s)
WRIST_LAG = 0.5              # (knock_down_face: how much of the arm's way back his wrists turn)
ARM_SWING = 0.45             # (knock_down_face: how far his arms are on their way forward at 0.33 s, from 0.25 s's)


def lead_arms(body, Ka, Kb, f=0.3):
    """Key Ka's arms started on their way to key Kb's (f of it): arms thrown by a blast do not go all at once."""
    for s in "lr":
        for n in ("clavicle_" + s,) + ARM[s] + ("fingers_" + s, "thumb_" + s):
            i = body.man.b(n)
            Ka[1][i] = qslerp(Ka[1][i], Kb[1][i], f)


def knock_face_keys(body):
    man = body.man
    b = man.b
    k = man.H[b("pelvis")][2] / 0.9765
    T = KF_T
    K = [(0.0,) + body.pose("fire_stand", 0.0) + ("go",)]
    like = body.pose("fire_stand", 0.0)
    # (his arms and the weapon left behind by his trunk as it is thrown forward: back from the trunk's line, the
    # weapon trailing low in his right hand)
    lag = dict(
        arm_l=dict(rel="chest", at=(-0.22, -0.18, -0.42), pole=(-0.4, -1.0, 0.0), d=(-0.3, 0.1, -1.0), t=(1.0, 0.2,
            0.0)),
        arm_r=dict(rel="chest", at=(-0.20, 0.16, -0.42), pole=(-0.4, 1.0, -0.2), gun=((0.45, 0.05, -0.89), (0.89,
            0.0, 0.45))),
        fin_l=(18.0, 10.0))
    # (struck in the back: his chest driven forward, his head left behind, his heels coming up; his legs as he stood,
    # turned out as the aim has them, carried back under him by his hips' pitch so his feet stay where they were)
    K.append((T[1],) + pose_of(body, dict(
        like=like, at=(0.16, 0.0, 0.955 * k), pel=(("y", 9.0), ("z", 2.0)),
        s1=(2.0, 0.0, 0.0), s2=(2.0, 0.0, 1.0), ch=(1.0, 0.0, 2.0), nk=(-12.0, 0.0, 0.0), hd=(-14.0, 0.0, -3.0))) +
             ("",))
    # (pitched forward a third of the way over, his feet trailing low behind him, his legs turning in from the aim's)
    # (his knees bent no less than the aim has them: a man who aims crouched does not straighten them to bend them again)
    kn0 = {s_: float(lean_of(like[0][b("calf_" + s_)])[0]) for s_ in "lr"}
    K.append((T[2],) + pose_of(body, dict(
        like=like, at=(0.38, 0.01, 0.85 * k), pel=(("y", 23.0), ("z", 4.0)),
        s1=(3.0, 0.0, 0.0), s2=(3.0, 0.0, 2.0), ch=(2.0, 0.0, 2.0), nk=(-12.0, 0.0, 2.0), hd=(-10.0, 0.0, 0.0),
        leg_l=(8.0, 5.0, 16.0, max(26.0, kn0["l"]), 4.0),
        leg_r=(4.0, 7.0, 20.0, max(26.0, kn0["r"]), 6.0), **lag)) + ("",))
    # (two thirds over, diving no more than falling: his legs trailing, his arms still behind)
    K.append((T[3],) + pose_of(body, dict(
        like=like, at=(0.74, 0.02, 0.66 * k), pel=(("y", 51.0), ("z", 7.0)),
        s1=(4.0, 0.0, 0.0), s2=(5.0, 0.0, 2.0), ch=(4.0, 0.0, 3.0), nk=(-12.0, 0.0, 2.0), hd=(-8.0, 0.0, -2.0),
        leg_l=(6.0, 8.0, 10.0, 42.0, 4.0), leg_r=(2.0, 9.0, 10.0, 32.0, 12.0),
        arm_l=dict(rel="chest", at=(-0.205, -0.225, -0.43), pole=(-0.5, -1.0, 0.0), d=(-0.1, -0.2, -1.0), t=(1.0,
            0.1, 0.2)),
        arm_r=dict(rel="chest", at=(-0.18, 0.18, -0.42), pole=(-0.5, 1.0, 0.0), gun=((0.55, 0.05, -0.83), (0.83,
            0.0, 0.55))),
        fin_l=(18.0, 10.0))) + ("",))
    # (his arms left behind as his trunk is driven on, LAG_AT[0] and LAG_AT[1] of the way back from the aim's to how
    # they trail at 0.14 s by then, the rest of the way by 0.25 s; his wrists turned only WRIST_LAG of the arm's way,
    # the weapon swinging back with his forearm rather than wrung in his hand)
    aim, lag2 = like[0], K[2][1].copy()
    hr_ = b("hand_r")
    for kk, f in ((1, LAG_AT[0]), (2, LAG_AT[1]), (3, 1.0)):
        l_ = K[kk][1].copy()
        if kk < 3:
            for s_ in "lr":
                for n_ in ("clavicle_" + s_,) + ARM[s_] + ("fingers_" + s_, "thumb_" + s_):
                    if n_ in man.ix:
                        l_[b(n_)] = qslerp(aim[b(n_)], lag2[b(n_)], f)
        for h_ in (b("hand_l"), hr_):
            l_[h_] = qslerp(aim[h_], l_[h_], WRIST_LAG)
        K[kk] = (K[kk][0],) + keep_prop(body, *gun_rides(body, l_, K[kk][2].copy(), body.rel0["r"]), body.prop0) + \
            (K[kk][3],)
    # (his arms on their way forward to break the fall (they are thrown out before him only as he comes down), the
    # weapon swinging out in his right hand)
    K.append((T[4],) + pose_of(body, dict(
        like=like, at=(1.04, 0.03, 0.50 * k), pel=(("y", 72.0), ("z", 8.0)),
        s1=(2.0, 0.0, 0.0), s2=(3.0, 0.0, 3.0), ch=(3.0, 0.0, 3.0), nk=(-14.0, 0.0, 0.0), hd=(-10.0, 0.0, -6.0),
        leg_l=(4.0, 10.0, 10.0, 32.0, 32.0), leg_r=(0.0, 11.0, 4.0, 38.0, 32.0),
        arm_l=dict(rel="chest", at=(-0.05, -0.26, -0.42), pole=(-0.6, -0.8, 0.0), d=(0.3, -0.15, -0.9), t=(0.7,
            0.7, 0.1)),
        arm_r=dict(rel="chest", at=(-0.03, 0.22, -0.42), pole=(-0.5, 0.9, 0.0), gun=((0.85, 0.08, -0.52), (0.52,
            -0.15, 0.85))),
        fin_l=(16.0, 9.0))) + ("",))
    # (his chest and forearms strike first, hips and lower legs still off the ground)
    rest = face_rest(body)
    Gr, Xr = man.fk(*rest)
    zr = float(Xr[b("pelvis")][2])
    K.append((T[5],) + pose_of(body, dict(
        like=like, at=(1.46, 0.03, zr + 0.05), pel=(("y", 98.0), ("z", 7.0)),
        s1=(-8.0, 0.0, 0.0), s2=(-8.0, 0.0, 0.0), ch=(-6.0, 0.0, -2.0), nk=(-20.0, 0.0, -14.0), hd=(-14.0, 0.0, -16.0),
        leg_l=(10.0, 12.0, 12.0, 22.0, 30.0), leg_r=(10.0, 14.0, 12.0, 26.0, 30.0),
        arm_l=dict(rel="sh", at=(0.06, -0.26, -0.22), pole=(-0.3, -1.0, 0.5), d=(1.0, 0.3, -0.1), t=(0.1, 1.0, 0.3)),
        arm_r=dict(rel="sh", at=(0.12, 0.24, -0.18), pole=(-0.4, 1.0, 0.5), gun=((1.0, 0.1, -0.05), (0.0, -1.0, 0.25))),
        fin_l=(20.0, 12.0), ground=("arm_l",))) + ("",))
    # (at the key before, his arms and the weapon on their way from behind him to how they strike, ARM_SWING of the
    # way: the swing forward to break the fall shared evenly between the two key gaps)
    l4 = K[-2][1].copy()
    for s_ in "lr":
        for n_ in ("clavicle_" + s_,) + ARM[s_] + ("fingers_" + s_, "thumb_" + s_):
            if n_ in man.ix:
                l4[b(n_)] = qslerp(K[-3][1][b(n_)], K[-1][1][b(n_)], ARM_SWING)
    K[-2] = (K[-2][0],) + keep_prop(body, *gun_rides(body, l4, K[-2][2].copy(), body.rel0["r"]), body.prop0) + ("",)
    # (between those two: his arms and the weapon held up off the ground on their way down, so the in-between does not
    # swing them through it and a lift there does not snap them up)
    post = lambda l, m: keep_prop(body, *gun_rides(body, l, m, body.rel0["r"]), body.prop0)
    mid = mix(K[-2][1:3], K[-1][1:3], 0.5)
    mid = grounded(body, mid[0], mid[1], post, clear=0.03, lift=False, limbs=False)
    K.insert(len(K) - 1, (0.5 * (T[4] + T[5]),) + mid + ("",))
    # (his hips and legs down on the ground; he slides)
    lq5, mv5 = shifted(body, rest, 1.68, 0.02)
    lq5[b("calf_l")] = qslerp(lq5[b("calf_l")], Ry(34.0), 0.04)
    lq5[b("calf_r")] = qslerp(lq5[b("calf_r")], Ry(62.0), 0.04)
    lq5 = turn_world(man, lq5, mv5, "neck", Ry(-9.0))
    for s_, d_ in (("l", 9.0), ("r", 0.0)):
        lq5 = turn_world(man, lq5, mv5, "upperarm_" + s_, Ry(-d_))
        lq5 = behind_ease(body, arm_ease(body, lq5, mv5, s_), mv5, s_)
    K.append((T[6], lq5, mv5, "soft"))     # (soft: his arms, thrown forward onto the ground, do not swing on past it)
    K.append((T[7],) + shifted(body, rest, 1.76) + ("stop",))
    return K


def knock_back_keys(body):
    man = body.man
    k = man.H[man.b("pelvis")][2] / 0.9765
    T, Xs = FACE_T, BACK_X
    like = body.pose("fire_stand", 0.0)
    K = [(T[0],) + like + ("go",)]
    # (struck in the front: his chest driven in and back, his head and arms thrown forward, feet left behind)
    K.append((T[1],) + pose_of(body, dict(
        like=like, at=(Xs[1], 0.0, FLY_Z[1] * k), pel=(("y", -9.0), ("z", -3.0)),
        s1=(7.0, 0.0, 0.0), s2=(10.0, 0.0, -2.0), ch=(11.0, 0.0, -3.0), nk=(16.0, 0.0, 0.0), hd=(20.0, 0.0, 4.0),
        leg_l=(24.0, 6.0, 4.0, 14.0, -4.0), leg_r=(15.0, 8.0, 6.0, 24.0, 2.0),
        )) + ("",))
    # (off his feet, going over backwards, folded at the hips, legs swept up before him)
    K.append((T[2],) + pose_of(body, dict(
        like=like, at=(Xs[2], -0.02, FLY_Z[2] * k), pel=(("y", -36.0), ("z", -7.0)),
        s1=(8.0, 0.0, 0.0), s2=(10.0, 0.0, -3.0), ch=(10.0, 0.0, -3.0), nk=(14.0, 0.0, 0.0), hd=(12.0, 0.0, 6.0),
        leg_l=(50.0, 13.0, 8.0, 32.0, 10.0), leg_r=(30.0, 10.0, 6.0, 58.0, 6.0),
        arm_l=dict(rel="chest", at=(0.40, -0.22, 0.02), pole=(-0.4, -0.9, -0.4), d=(0.9, -0.2, 0.3), t=(0.2, 1.0, 0.0)),
        arm_r=dict(rel="sh", at=(0.28, 0.32, 0.10), pole=(-0.4, 0.9, -0.4), gun=((0.9, 0.3, 0.3), (-0.3, 0.0, 0.95))),
        fin_l=(16.0, 10.0))) + ("",))
    K.append((T[3],) + pose_of(body, dict(
        like=like, at=(Xs[3], -0.03, FLY_Z[3] * k), pel=(("y", -64.0), ("z", -9.0)),
        s1=(6.0, 0.0, 0.0), s2=(8.0, 0.0, -2.0), ch=(8.0, 0.0, -2.0), nk=(10.0, 0.0, 0.0), hd=(6.0, 0.0, 4.0),
        leg_l=(64.0, 15.0, 8.0, 42.0, 10.0), leg_r=(46.0, 12.0, 6.0, 72.0, 6.0),
        arm_l=dict(rel="chest", at=(0.34, -0.36, 0.14), pole=(-0.5, -0.8, -0.3), d=(0.7, -0.4, 0.6), t=(0.4, 0.9, 0.0)),
        arm_r=dict(rel="sh", at=(0.22, 0.42, 0.10), pole=(-0.5, 0.8, -0.3), gun=((0.9, 0.35, 0.25), (-0.25, 0.0,
            0.97))),
        fin_l=(12.0, 8.0))) + ("",))
    rest = back_rest(body)
    Gr, Xr = man.fk(*rest)
    zr = float(Xr[man.b("pelvis")][2])
    # (his seat strikes, his trunk still coming down, his legs up)
    K.append((T[4],) + pose_of(body, dict(
        like=like, at=(Xs[4], -0.03, zr + 0.10), pel=(("y", -76.0), ("z", -6.0)),
        s1=(5.0, 0.0, 0.0), s2=(7.0, 0.0, 0.0), ch=(7.0, 0.0, 0.0), nk=(12.0, 0.0, 0.0), hd=(10.0, 0.0, 4.0),
        leg_l=(72.0, 15.0, 8.0, 36.0, 10.0), leg_r=(56.0, 12.0, 6.0, 62.0, 6.0),
        arm_l=dict(rel="sh", at=(0.18, -0.42, 0.05), pole=(0.1, -0.8, -0.6), d=(0.8, -0.6, 0.1), t=(0.3, 0.2, 1.0)),
        arm_r=dict(rel="sh", at=(0.27, 0.42, 0.06), pole=(-0.2, 1.0, -0.3), gun=((0.95, 0.3, 0.1), (0.1, -0.4, 0.9))),
        fin_l=(14.0, 8.0))) + ("",))
    # (his shoulders slam down, his head goes back, arms flung out on the ground, legs still in the air)
    lq5, mv5 = shifted(body, rest, Xs[5], -0.02)
    b = man.b
    for s_, d_ in (("l", 46.0), ("r", 30.0)):
        lq5 = turn_world(man, lq5, mv5, "thigh_" + s_, Ry(-d_))
    lq5[b("calf_l")] = Ry(30.0)
    lq5[b("calf_r")] = Ry(58.0)
    lq5 = turn_world(man, lq5, mv5, "neck", Ry(8.0))
    for s_, d_ in (("l", 0.0), ("r", 6.0)):
        lq5 = turn_world(man, lq5, mv5, "upperarm_" + s_, Ry(d_))
        lq5 = elbow_out(body, behind_ease(body, arm_ease(body, lq5, mv5, s_), mv5, s_), mv5, s_)
    lead_arms(body, K[1], K[2])
    # (his shoulders coming down onto the ground, the weapon swung out to his right so it lands beside him, rolling
    # onto its side: the landing eased over one more key)
    m45 = mix(K[-1][1:3], (lq5, mv5), 0.55)
    K.append((LAND_T,) + pose_of(body, dict(
        like=m45, arm_r=dict(rel="sh", at=(0.42, 0.36, -0.08), pole=(0.0, 1.0, -0.4),
                             gun=((0.95, 0.31, -0.03), (0.28, -0.85, 0.45))))) + ("",))
    K.append((T[5], lq5, mv5, "soft"))     # (soft: his arms, flung out onto the ground, do not swing on past it)
    # (the weapon rolling onto its side as it lands: his right wrist's turn shared evenly over the landing's two key
    # gaps, the landing key's wrist moved along its way to the next key's as far as does that)
    hr_ = b("hand_r")
    A_, L_, B_ = K[-3][1][hr_], K[-2][1][hr_], K[-1][1][hr_]
    d1, d2 = K[-2][0] - K[-3][0], K[-1][0] - K[-2][0]
    f_ = min(np.linspace(-0.4, 0.6, 41), key=lambda u: max(angle_of(A_.T @ qslerp(L_, B_, u)) / d1,
                                                           angle_of(qslerp(L_, B_, u).T @ B_) / d2))
    l_ = K[-2][1].copy()
    l_[hr_] = qslerp(L_, B_, float(f_))
    K[-2] = (K[-2][0],) + keep_prop(body, *gun_rides(body, l_, K[-2][2].copy(), body.rel0["r"]), body.prop0) + ("",)
    K.append((T[6],) + shifted(body, rest, Xs[6]) + ("stop",))
    return K


def knock_pose(body, way, t):
    key = ("knock_keys", way)
    if key not in body.poses:
        body.poses[key] = Keys(body, (knock_face_keys if way == "face" else knock_back_keys)(body))
    # (thrown forward, his feet trail low behind him: each carried straight from where one key has it to the next,
    # not swung down through the ground between them)
    pins = (("leg_l", KF_T[1], KF_T[4]), ("leg_r", KF_T[1], KF_T[4])) if way == "face" else ()
    # (thrown onto his back, his wrists as the keys have them: each key's are in their range, and the in-between's
    # turning a wrist into a range of its own reading would wring the forearm past a living one's)
    return play(body, body.poses[key], t, pins=pins, wrists=(way == "face"))


def mix(a, b, u):
    """Two poses mixed bone by bone (u of the way from a to b)."""
    lq = np.array([qslerp(a[0][i], b[0][i], u) for i in range(len(a[0]))])
    return lq, a[1] * (1 - u) + b[1] * u


def mirrored(body, pose_):
    """A pose as a mirror shows it: his left for his right."""
    man = body.man
    M = np.diag((1.0, -1.0, 1.0))
    lq, mv = pose_
    l2, m2 = lq.copy(), mv.copy()
    for i, n in enumerate(man.rig.names):
        o = n[:-2] + "_r" if n.endswith("_l") else n[:-2] + "_l" if n.endswith("_r") else n
        if o in man.ix:
            j = man.ix[o]
            l2[i], m2[i] = M @ lq[j] @ M, M @ mv[j]
    return l2, m2


def breath(u):
    return 0.5 - 0.5 * math.cos(2 * math.pi * u)


def stir_of(t):
    """The one stir a loop: up from 0.70 s, held 0.95 to 1.15 s, down again by 1.45 s (0 at the seam)."""
    if t <= 0.70 or t >= 1.45:
        return 0.0
    if t < 0.95:
        return sm((t - 0.70) / 0.25)
    if t < 1.15:
        return 1.0
    return 1.0 - sm((t - 1.15) / 0.30)


STIR_HEAD = (17.0, 12.0)     # (degrees: his head lifted off the ground and turned, at the stir)
STIR_ARM = 0.12              # (m: his left forearm drawn in along the ground, at the stir)
STIR_KNEE = 18.0             # (degrees: lying on his back, his right knee drawn up, at the stir)


def down_pose(body, way, t):
    """Lying stunned: a slow breath, his head stirring, the fingers of his left hand; a foot. Once a loop he stirs
    where it can be seen: his head lifted and turned, his left forearm drawn in, on his back a knee drawn up. How far
    the forearm is drawn in is this man's own: the most (of STIR_ARM) that keeps his left shoulder, elbow and wrist
    in a living arm's range at the height of the stir (his elbow not drawn in under his back), found once."""
    key = ("stir_arm", way)
    if key not in body.poses:
        best = None
        for k_ in (1.0, 0.8, 0.65, 0.5, 0.35, 0.2, 0.0):
            p_, ok = _down_pose(body, way, 1.05, k_, probe=True)
            over = sum(f[2] for f in pose_rom(body, [p_]) if f[1] == "l" and f[0].startswith(
                ("sh_", "el_", "pron", "sup", "wr_")))
            if ok and over <= 0.0:
                best = (0.0, k_)
                break
            if best is None or over < best[0]:
                best = (over, k_)
        body.poses[key] = best[1]
    return _down_pose(body, way, t, body.poses[key])


def _down_pose(body, way, t, arm_k, probe=False):
    man = body.man
    b = man.b
    lq, mv = (face_rest if way == "face" else back_rest)(body)
    u = (t / DOWN_LONG) % 1.0
    if u < 1e-9:
        return lq, mv
    br = breath(u)
    sg = -1.0 if way == "face" else 1.0
    # (his ribs: the back rises lying on his front, the chest lying on his back; his head hardly moves with it)
    lq[b("spine_02")] = lq[b("spine_02")] @ Ry(sg * 1.5 * br)
    lq[b("chest")] = lq[b("chest")] @ Ry(-sg * 0.7 * br)
    for s_, k in (("l", 1.0), ("r", -1.0)):
        lq[b("clavicle_" + s_)] = lq[b("clavicle_" + s_)] @ Rx(-k * 1.6 * br)
    stir = math.sin(2 * math.pi * u) + 0.5 * math.sin(4 * math.pi * u)
    nod = math.sin(2 * math.pi * u) ** 2 * math.sin(2 * math.pi * u + 0.8)
    lq[b("neck")] = lq[b("neck")] @ Rz(3.0 * stir) @ Ry(1.5 * nod)
    lq[b("head")] = lq[b("head")] @ Rz(4.0 * stir) @ Ry(-2.0 * nod)
    A = man.side["l"]
    grope = math.sin(math.pi * u) ** 2 * (0.6 + 0.4 * math.sin(6 * math.pi * u))
    lq[b("fingers_l")] = lq[b("fingers_l")] @ R_axis(np.cross(A["d0"], A["n0"]), 16.0 * grope)
    lq[b("thumb_l")] = lq[b("thumb_l")] @ R_axis(np.cross(A["thumb0"], A["n0"]), 8.0 * grope)
    lq[b("hand_l")] = lq[b("hand_l")] @ R_axis(A["d0"], 7.0 * math.sin(2 * math.pi * u) * math.sin(math.pi * u) ** 2)
    foot = "foot_l" if way == "face" else "foot_r"
    lq[b(foot)] = lq[b(foot)] @ Ry(4.0 * math.sin(math.pi * u) ** 2 * math.sin(4 * math.pi * u))
    e = stir_of(u * DOWN_LONG)
    if e > 0.0:
        G, X = man.fk(lq, mv)
        ch, hd = b("chest"), b("head")
        side = unit((G[ch] @ Y_) - Z_ * float((G[ch] @ Y_) @ Z_))
        # (his head: lifted off the ground about his shoulders' axis, whichever way takes it up, and turned)
        r = X[hd] + G[hd] @ body.crown - X[b("neck")]
        sgn = 1.0 if float(np.cross(side, r)[2]) > 0 else -1.0
        for bn, f in (("neck", 0.45), ("head", 0.55)):
            G, X = man.fk(lq, mv)
            lq = turn_world(man, lq, mv, bn, R_axis(side, sgn * STIR_HEAD[0] * f * e) @
                            R_axis(unit(G[hd] @ Z_), STIR_HEAD[1] * f * e))
        # (his left forearm drawn in along the ground toward his shoulder)
        G, X = man.fk(lq, mv)
        Hl, Fl = b("hand_l"), b("lowerarm_l")
        to = X[b("upperarm_l")] - X[Hl]
        to = unit(to - Z_ * float(to @ Z_))
        f0 = X[Hl] - X[Fl]
        Ul = b("upperarm_l")
        had0 = arm_read(body, "l", G[ch], G[Ul], G[Hl], X[Ul], X[Fl], X[Hl])["hadd"]
        ik2(man, lq, mv, ARM["l"], X[Hl] + to * STIR_ARM * e * arm_k, end_R=G[Hl])
        # (the hand turned on the ground with the forearm, not bent at the wrist)
        G2, X2 = man.fk(lq, mv)
        f1 = X2[Hl] - X2[Fl]
        yaw = math.degrees(math.atan2(f1[1], f1[0]) - math.atan2(f0[1], f0[0]))
        yaw = (yaw + 180.0) % 360.0 - 180.0
        ik2(man, lq, mv, ARM["l"], X2[Hl], end_R=Rz(yaw) @ G[Hl])
        G2, X2 = man.fk(lq, mv)
        # (not drawn across his body further than a living shoulder lets it)
        hadd_ok = arm_read(body, "l", G2[ch], G2[Ul], G2[Hl], X2[Ul], X2[Fl], X2[Hl])["hadd"] <= max(had0, 118.0)
        if way == "back":
            # (his right knee drawn up, its heel sliding in along the ground: the thigh turned up at the hip, the shin
            # back down at the knee until the heel is as low as it lay)
            G, X = man.fk(lq, mv)
            th, ca, fo = b("thigh_r"), b("calf_r"), b("foot_r")
            ax = unit(G[b("pelvis")] @ Y_)
            r = X[ca] - X[th]
            sk = 1.0 if float(np.cross(ax, r)[2]) > 0 else -1.0
            z0 = X[fo][2]
            lq = turn_world(man, lq, mv, "thigh_r", R_axis(ax, sk * STIR_KNEE * e))
            G, X = man.fk(lq, mv)
            kx = unit(G[ca] @ Y_) if abs(float(G[ca] @ Y_ @ ax)) > 0.5 else ax
            l0 = lq[ca].copy()
            lo_, hi_ = 0.0, 2.5 * STIR_KNEE * e

            def heel(ang):
                l2 = lq.copy()
                l2[ca] = l0
                l2 = turn_world(man, l2, mv, "calf_r", R_axis(kx, -sk * ang))
                return l2, man.fk(l2, mv)[1][fo][2]
            for _ in range(14):
                mid = 0.5 * (lo_ + hi_)
                if heel(mid)[1] > z0:
                    lo_ = mid
                else:
                    hi_ = mid
            lq = heel(lo_)[0]
            lq[fo] = lq[fo]
    post = lambda l, m: keep_prop(body, *gun_rides(body, l, m, body.rel0["r"]), body.prop0)
    lq, mv = post(lq, mv)
    # (nothing pressed into the ground by it)
    lq, mv, _ = chain_to_ground(body, lq, mv, "neck", body.far["head"], clear=0.003)
    lq, mv, _ = chain_to_ground(body, lq, mv, "upperarm_l", body.far["arm_l"], clear=0.003)
    # (and his left hand, by his head under his shoulder where the upper arm has no lever on it, by his elbow)
    lq, mv, _ = chain_to_ground(body, lq, mv, "lowerarm_l", body.Z["hand_l"], clear=0.003, most=12.0)
    if probe:
        return (lq, mv), e <= 0.0 or hadd_ok
    return lq, mv


# ================================================================================================ getting up
# Keyed on what he bears on: a hand, a knee or a foot that carries him stays on its one spot for as long as it does
# (held there between the keys), and moves only lifted clear of the ground. Each ends in his own idle standing where
# the front foot of his own kneel was set down: `end_offset` is where his pelvis ends (his own axes, as go_prone's).
UP_FACE = {k: f / 30.0 for k, f in dict(up=2, lift=5, stir=7, fours=10, l_up=11, l_on=12, l_dn=13, r_up=14, r_on=15,
                                         r_dn=16, rock=18, off=20, tall=22, kup=24, swing=26, hover=27, land=28,
                                         kneel=31, rise=34, toe=35, step=37, over=39, plant=40, end=48).items()}
UP_BACK = {k: f / 30.0 for k, f in dict(reach=2, sit1=5, prop=8, lb=10, roll=12, side=14, foot=16, load=18, lean=21,
                                         unt=22, push=24, hov=26, kneel=28, rise=32, toe=33, step=36, over=39, down=41,
                                         end=48).items()}
GRIP_T = (1.22, 1.5)         # (his right hand's grip shifts from the aim's to the carry's)
LEFT_ON_T = {"face": (1.26, 1.52), "back": (1.33, 1.58)}     # (his left hand comes onto the weapon)
ANKLE_UP = (-42.0, 23.0)     # (degrees: the most a planted foot is pointed down or bent up at the ankle in a get-up)
SIDE_TUCK = 155.0            # (degrees: his left shin's way over the ground from his thigh's while he sits on that hip)
KNEE_SINK = 0.017            # (m: his own kneel holds the knee this far off the ground: set down onto it)
UP_FPS = 30                  # (the get-ups are keyed 30 a second)
PUSH_AT = (-0.05, 0.22)
LIFT_H = 0.04               # (m: get_up_face's hands held this far over their spots before they are set down)
ARM_BEND = 15.0              # (degrees: the least a get-up's elbow is bent while that hand bears his weight)      # (m: get_up_face's hands set down for the push, behind (-) and out from each shoulder)


def kneel_at(body, foot, flip=False):
    """The file's own kneel (fire_kneel's first pose; `flip`: its mirror, the left knee down), moved over the ground
    so the foot he stands up on (left; right when flipped) is `foot` (x, y)."""
    man = body.man
    kp = body.pose("fire_kneel", 0.0)
    if flip:
        kp = mirrored(body, kp)
    G, X = man.fk(*kp)
    f = X[man.b("foot_r" if flip else "foot_l")]
    lq, mv = shifted(body, kp, foot[0] - f[0], foot[1] - f[1])
    # (the foot of the knee that is down: its toes laid back on the ground, not driven into it as the aim has them)
    lq = foot_clear(body, lq, mv, "l" if flip else "r", 0.002)
    return lq, mv


def thigh_len(body, s):
    H = body.man.H
    return float(np.linalg.norm(H[body.man.b("calf_" + s)] - H[body.man.b("thigh_" + s)]))


def hip_dz(body, lq, mv, s, K):
    """How far his pelvis must go up (+) or down for the hip of leg s to be a thigh's length from the knee at K."""
    X = body.man.fk(lq, mv)[1]
    h = X[body.man.b("thigh_" + s)]
    Lt = thigh_len(body, s) * 0.998
    d2 = (h[0] - K[0]) ** 2 + (h[1] - K[1]) ** 2
    if d2 >= Lt * Lt:
        return 0.0
    return float(K[2] + math.sqrt(Lt * Lt - d2) - h[2])


def knee_leg(body, lq, mv, s, K, F, RF=None):
    """Leg s with its knee pointed at K and its foot's head at F (turned RF): exact when the hip is a thigh's length
    from K and K a shin's length from F."""
    man = body.man
    X = man.fk(lq, mv)[1]
    ik2(man, lq, mv, LEG[s], F, pole=np.asarray(K, float) - 0.5 * (X[man.b("thigh_" + s)] + np.asarray(F, float)),
        end_R=RF)
    return lq


def lifted(a, b, h, u=0.5):
    """The point u of the way from a to b, raised h."""
    return np.asarray(a, float) * (1 - u) + np.asarray(b, float) * u + Z_ * h


def about_knee(K, Fa, Fb, u, h=0.0, L=None):
    """A foot's head carried round its knee (K) from Fa to Fb: turned about the knee over the ground u of the way, its
    height eased u of the way and lifted h, L (a shin's length) from the knee (so the shin keeps its length, not cut
    across a chord)."""
    K = np.asarray(K, float)
    da, db = np.asarray(Fa, float) - K, np.asarray(Fb, float) - K
    aa, ab = math.atan2(da[1], da[0]), math.atan2(db[1], db[0])
    a = aa + ((ab - aa + math.pi) % (2.0 * math.pi) - math.pi) * u
    if L is None:
        L = float(np.linalg.norm(da)) * (1 - u) + float(np.linalg.norm(db)) * u
    z = da[2] * (1 - u) + db[2] * u + h
    r = math.sqrt(max(0.0, L * L - z * z))
    return K + np.array((r * math.cos(a), r * math.sin(a), z))


def knee_low(body, lq, mv, s):
    return zone_low(body, lq, mv, body.Z["knee_" + s] | (body.pa == body.man.b("calf_" + s)))[0]


def fit_hips(body, lq, mv, Ks, its=4):
    """His pelvis raised or lowered (and, with two knees in Ks {s: K}, rolled about the world's x) so each hip is a
    thigh's length from its knee. Changes lq, mv; returns the rise (m) and the roll (degrees)."""
    man = body.man
    pe = man.b("pelvis")
    dz_t, roll_t = 0.0, 0.0
    for _ in range(its):
        d = {s: hip_dz(body, lq, mv, s, K) for s, K in Ks.items()}
        dz = sum(d.values()) / len(d)
        mv[pe] = mv[pe] + Z_ * dz
        dz_t += dz
        r = 0.0
        if len(d) == 2:
            X = man.fk(lq, mv)[1]
            sep = float(np.linalg.norm(X[man.b("thigh_r")] - X[man.b("thigh_l")]))
            r = -math.degrees(math.atan2(d["l"] - d["r"], sep))
            r = max(-12.0 - roll_t, min(12.0 - roll_t, r))
            lq[pe] = Rx(r) @ lq[pe]
            roll_t += r
        if abs(dz) < 3e-4 and abs(r) < 0.05:
            break
    return dz_t, roll_t


def up_key(body, sp, knees=(), feet=(), hip=None, touch=0.002, balls=None):
    """One key of a get-up: pose_of's pose (sp), its pelvis first raised or lowered so the hip of leg `hip` (s, K) is a
    thigh's length from the knee at K (hip "both": both knees in `knees`, the pelvis rolled to fit them); each leg in
    `knees` [(s, K, F, RF[, down])] with its knee on K and its foot at F, each in `feet` [(s, F, RF, pole)] onto F;
    each knee in `knees` (unless `down` is False) let down onto the ground (`touch` over it) or lifted out of it; then
    the weapon on his right hand, and anything of him that is not bearing on the ground (his head, a free arm, the
    weapon) turned out of it at its own joint."""
    man = body.man
    sp = dict(sp, clean=False)
    knees = [list(k) + [True] * (5 - len(k)) for k in knees]
    at0 = sp.get("at")
    roll0 = sp.get("pel_roll", 0.0)
    for it in range(3):
        if hip is not None:
            Ks = {k[0]: np.asarray(k[1], float) for k in knees} if hip == "both" else \
                {hip[0]: next((k[1] for k in knees if k[0] == hip[0]), hip[1])}
            trial = {k: v for k, v in sp.items() if not k.startswith(("arm_", "fin_", "gun", "ground", "foot_"))}
            trial["ease"] = False
            trial["at"] = at0
            trial["pel_roll"] = roll0
            dz, roll = fit_hips(body, *pose_of(body, trial), Ks)
            sp["at"] = (at0[0], at0[1], at0[2] + dz)
            sp["pel_roll"] = roll0 + roll
        lq, mv = pose_of(body, sp)
        for s, K, F, RF, _ in knees:
            lq = knee_leg(body, lq, mv, s, K, F, RF)
        for s, F, RF, pole in feet:
            ik2(man, lq, mv, LEG[s], F, pole=pole, end_R=RF)
            if RF is not None:
                # (a foot set down flat: on its heel where the leg is stretched out, on its ball where the shin is
                # pitched far over it, inside an ankle's range either way)
                Gf0 = man.fk(lq, mv)[0][man.b("foot_" + s)]
                lq = heel_pivot(body, lq, mv, s, ANKLE_UP[0])
                lq = heel_up(body, lq, mv, s, ANKLE_UP[1])
                if (balls or {}).get(s) is not None and "ball_" + s in man.ix:
                    # (its toes, held as they lie, stay flat on the ground as the heel comes up about them)
                    Gf1 = man.fk(lq, mv)[0][man.b("foot_" + s)]
                    balls = dict(balls, **{s: Gf1.T @ Gf0 @ balls[s]})
        for s, R in (balls or {}).items():
            # (the toes of a foot that bears on the ground kept as they are bent through the whole hold)
            if R is not None and "ball_" + s in man.ix:
                lq[man.b("ball_" + s)] = R
        moved = False
        for k in knees:
            if not k[4]:
                continue
            d = touch - knee_low(body, lq, mv, k[0])
            if abs(d) > 0.0015:
                k[1] = np.asarray(k[1], float) + Z_ * d
                moved = True
            # (and the foot behind it kept out of the ground)
            zf = zone_low(body, lq, mv, body.Z["foot_" + k[0]])[0]
            if zf < 0.0005:
                k[2] = np.asarray(k[2], float) + Z_ * (touch - zf)
                moved = True
        if not moved:
            break
    rel = sp.get("grip", body.rel0["r"])
    post = lambda l, m: keep_prop(body, *gun_rides(body, l, m, rel), body.prop0)
    lq, mv = post(lq, mv)
    planted = {k[0] for k in knees} | {f[0] for f in feet}
    for s in "lr":
        if s not in planted:
            lq = ankle_ease(body, lq, s)
            lq = foot_clear(body, lq, mv, s, 0.002)
    lq, mv = grounded(body, lq, mv, post, clear=0.002, lift=False, limbs=False)
    return post(lq, mv)


def seat_key(body, sp, prev, **kw):
    """up_key's pose with his pelvis moved over the ground so that what of his seat bore on the ground in the key
    before (prev) and still bears on it lies where it lay: he sits up or rolls over on it, not sliding."""
    man = body.man
    seat = np.isin(body.pa, [man.b(n) for n in CONTACT["seat"] if n in man.ix])
    Vp = body.skin(*man.fk(*prev))
    at = np.asarray(sp["at"], float)
    out = up_key(body, dict(sp, at=tuple(at)), **kw)
    for _ in range(2):
        V = body.skin(*man.fk(*out))
        c = seat & (Vp[:, 2] < 0.012) & (V[:, 2] < 0.012)
        if c.sum() < 3:
            break
        d = (Vp[c, :2] - V[c, :2]).mean(0)
        if float(np.linalg.norm(d)) < 0.002:
            break
        at = at + np.array((d[0], d[1], 0.0))
        out = up_key(body, dict(sp, at=tuple(at)), **kw)
    return out


def shin_down(body, lq, mv, s, K, F, RF):
    """The foot behind a knee on the ground (K): the shin turned up about the knee, the least that keeps its toes out
    of the ground. Returns the foot's place and turn."""
    man = body.man
    lq = knee_leg(body, lq.copy(), mv, s, K, F, RF)
    lq, mv2, _ = chain_to_ground(body, lq, mv.copy(), "calf_" + s, body.Z["foot_" + s], clear=0.002, most=40.0,
                                 hinge=True)
    lq = foot_clear(body, lq, mv2, s, 0.002)
    G, X = man.fk(lq, mv2)
    return X[man.b("foot_" + s)].copy(), G[man.b("foot_" + s)].copy()


def hand_as(body, G, X, s, pole, **kw):
    """An arm spec (pose_of) that puts hand s where, and turned as, it is in the pose (G, X)."""
    man = body.man
    h = man.b("hand_" + s)
    A = man.side[s]
    return dict(rel="abs", at=X[h].copy(), pole=pole, d=G[h] @ A["d0"], t=G[h] @ A["t0"], fixed=True, **kw)


FACE_HAND_L = (1.0, 0.15, -0.12)   # (get_up_face: his left hand set down for the push, its fingers along this)
OFF_GUN = ((0.9, -0.05, -0.05), (0.05, 0.0, 1.0))  # (get_up_face: the weapon as it comes up off the ground with his
#                              right hand: about level, its muzzle not swung down into the ground on the way)
FACE_STEP = 0.28             # (m: each knee stepped forward under him once he is on his hands and knees)
FACE_ROCK = (-0.10, 0.42, 50.0)  # (get_up_face's rock: his pelvis this far forward of his right knee, this high, pitched)


def up_face_keys(body):
    man = body.man
    b = man.b
    T = UP_FACE
    pe = b("pelvis")
    rest = face_rest(body)
    idle = body.pose("idle", 0.0)
    Gr, Xr = man.fk(*rest)
    Gi, Xi = man.fk(*idle)
    # (each knee is stepped forward once he is up on his hands: where it comes down, and his kneel set so its right
    # knee is there; he stands up over its left foot)
    STEP_L, STEP_R = np.array((FACE_STEP, 0.08, 0.0)), np.array((FACE_STEP, -0.05, 0.0))
    k0 = body.pose("fire_kneel", 0.0)
    Xk0 = man.fk(*k0)[1]
    S = Xr[b("calf_r")][:2] + STEP_R[:2] - Xk0[b("calf_r")][:2]
    kn = shifted(body, k0, float(S[0]), float(S[1]))
    kn[1][pe] = kn[1][pe] - Z_ * KNEE_SINK
    Fl = Xk0[b("foot_l")] + np.array((S[0], S[1], 0.0))
    end = shifted(body, idle, float(Fl[0] - Xi[b("foot_l")][0]), float(Fl[1] - Xi[b("foot_l")][1]))
    Ge, Xe = man.fk(*end)
    Rfl = Ge[b("foot_l")]                         # (the left foot set down as it will stand)
    ik2(man, kn[0], kn[1], LEG["l"], Fl, end_R=Rfl)
    Gk, Xk = man.fk(*kn)
    Kr = Xk[b("calf_r")].copy()
    Fr, Rfr = shin_down(body, kn[0], kn[1], "r", Kr, Xk[b("foot_r")], Gk[b("foot_r")])
    kn = (knee_leg(body, kn[0], kn[1], "r", Kr, Fr, Rfr), kn[1])
    Gk, Xk = man.fk(*kn)
    Kr0 = Kr - STEP_R                             # (where his right knee lies)
    Kl0 = np.array((Xr[b("calf_l")][0], Xr[b("calf_l")][1], Kr[2]))
    Kl = Kl0 + STEP_L
    rfl, rfr = (Xr[b("foot_l")], Gr[b("foot_l")]), (Xr[b("foot_r")], Gr[b("foot_r")])
    rfl2 = (rfl[0] + STEP_L, rfl[1])
    Fre, Rfre = Xe[b("foot_r")], Ge[b("foot_r")]
    # (his hands set down for the push beside his shoulders, a little behind and out from them: a push from further
    # back would carry his elbows up behind his back)
    shl, shr = Xr[b("upperarm_l")], Xr[b("upperarm_r")]
    hl = np.array((shl[0] + PUSH_AT[0], shl[1] - PUSH_AT[1]))
    hr = np.array((shr[0] + PUSH_AT[0], shr[1] + PUSH_AT[1]))
    gun_r = (Gr[body.wi] @ X_, Gr[body.wi] @ Z_)
    lying = (("l", Kl0, rfl[0], rfl[1]), ("r", Kr0, rfr[0], rfr[1]))
    K = [(0.0,) + rest + ("stop",)]
    bl = {k: (p_[0][b("ball_l")], p_[0][b("ball_r")]) for k, p_ in (("rest", rest), ("kn", kn), ("end", end))} \
        if "ball_l" in man.ix else None

    def balls(l=None, r=None):
        return None if bl is None else {"l": bl[l][0] if l else None, "r": bl[r][1] if r else None}
    # (he stirs: his head comes up; his arms still as they lay, so the left is not wrung over at the shoulder as it
    # comes off the ground from beside his head (it comes up on its way to the lift))
    K.append((T["up"],) + up_key(body, dict(like=rest, nk=(-6.0, 0.0, -20.0), hd=(-4.0, 0.0, -22.0),
                                            fin_l=(10.0, 2.0))) + ("",))
    # (his hands set down beside his chest, pushing; his chest coming up)
    stir = up_key(body, dict(
        like=rest,
        s1=(-4.0, 0.0, 0.0), s2=(-6.0, 0.0, 0.0), ch=(-6.0, 0.0, 0.0), nk=(-16.0, 0.0, -8.0), hd=(-10.0, 0.0, -6.0),
        arm_l=dict(rel="abs", at=(hl[0], hl[1], 0.055), pole=(-0.3, -0.9, 0.4), d=FACE_HAND_L,
                   t=(0.1, 1.0, 0.1), plant=True),
        arm_r=dict(rel="abs", at=(hr[0], hr[1], 0.07), pole=(-0.3, 0.9, 0.4), d=(1.0, 0.0, -0.1), t=(0.0, -1.0, 0.3)),
        fin_l=(6.0, -4.0), gun=dict(hand=hr, yaw=-3.0, side=-1.0, search=True)),
        knees=lying)
    G2, X2 = man.fk(*stir)
    # (drawn in by his ribs, his hands held over where they go down, turned as they will be; his head coming up)
    K.append((T["lift"],) + up_key(body, dict(
        like=rest, s1=(-2.0, 0.0, 0.0), s2=(-3.0, 0.0, 0.0), ch=(-3.0, 0.0, 0.0), nk=(-14.0, 0.0, -14.0),
        hd=(-8.0, 0.0, -14.0),
        arm_l=dict(hand_as(body, G2, X2, "l", (-0.3, -0.9, 0.4)), at=X2[b("hand_l")] + Z_ * LIFT_H),
        arm_r=dict(hand_as(body, G2, X2, "r", (-0.3, 0.9, 0.4)), at=X2[b("hand_r")] + Z_ * LIFT_H),
        fin_l=(6.0, -4.0))) + ("",))
    K.append((T["stir"],) + stir + ("",))
    hands = dict(arm_l=hand_as(body, G2, X2, "l", (-0.6, -0.8, 0.3)), arm_r=hand_as(body, G2, X2, "r", (-0.6, 0.8,
        0.3)),
                 fin_l=(4.0, -4.0))
    up_on = dict(s1=(5.0, 0.0, 0.0), s2=(6.0, 0.0, 0.0), ch=(4.0, 0.0, 0.0), nk=(8.0, 0.0, 4.0), hd=(10.0, 0.0, 6.0),
                 **hands)
    mid_y = 0.25 * (Kl0[1] + Kr0[1])
    def bent(make, dxs):
        # (a key on his hands has the arms bent a little at the elbows, not locked straight with the hands beyond
        # reach (where the least change of reach turns an elbow fast): his pelvis the least further forward that
        # gives both elbows ARM_BEND)
        best = None
        for dx in dxs:
            p_ = make(dx)
            v = rom_values(body, p_)
            m = min(v.get("el_flex_l", 90.0), v.get("el_flex_r", 90.0))
            if m >= ARM_BEND:
                return p_, dx
            if best is None or m > best[0]:
                best = (m, p_, dx)
        return best[1], best[2]
    # (pushed up onto his hands and knees, his head hanging)
    p4, dx4 = bent(lambda dx: up_key(body, dict(
        like=K[-1][1:3], at=(Kr0[0] + dx, mid_y, 0.5), pel=(("y", 76.0),), **up_on),
        knees=lying, hip="both", balls=balls("rest", "rest")), (0.06, 0.09, 0.12, 0.15, 0.18, 0.21))
    K.append((T["fours"],) + p4 + ("",))
    # (his left knee stepped forward under him: lifted with its shin, carried, set down; his weight on his hands
    # and right knee)
    def shin(s, K0, F0, R0, u, h):
        Kx = K0 + STEP_L * u + Z_ * h
        return (s, Kx, F0 + STEP_L * u + Z_ * (h * 0.8), R0, False)
    # (lifted straight up off its spot, carried over the ground, set straight down onto the next: it does not skid as
    # it leaves the ground or as it meets it)
    for key, u, h, pl in (("l_up", 0.0, 0.045, 0.06), ("l_on", 1.0, 0.045, 0.09), ("l_dn", 1.0, 0.0, 0.10)):
        kl_ = shin("l", Kl0, rfl[0], rfl[1], u, h) if h else ("l", Kl, rfl2[0], rfl2[1])
        K.append((T[key],) + bent(lambda dx: up_key(body, dict(
            like=K[-1][1:3], at=(Kr0[0] + dx + STEP_L[0] * 0.5 * u, mid_y - 0.02, 0.5), pel=(("y", 76.0),), **up_on),
            knees=(kl_, ("r", Kr0, rfr[0], rfr[1])), hip="both", balls=balls("rest", "rest")),
            [dx4 + 0.03 * i for i in range(5)])[0] + ("",))
    # (then his right, coming down with its toes set under)
    for key, u, h in (("r_up", 0.0, 0.045), ("r_on", 1.0, 0.045), ("r_dn", 1.0, 0.0)):
        if h:
            kr_ = ("r", Kr0 + STEP_R * u + Z_ * h, lifted(rfr[0], Fr, h * 0.8 + 0.01, u) - Z_ * 0.0,
                   qslerp(rfr[1], Rfr, u), False)
        else:
            kr_ = ("r", Kr, Fr, Rfr)
        K.append((T[key],) + bent(lambda dx: up_key(body, dict(
            like=K[-1][1:3], at=(Kr0[0] + dx + STEP_R[0] * (0.5 + 0.5 * u), mid_y + 0.02, 0.5), pel=(("y", 74.0),),
            **up_on),
            knees=(("l", Kl, rfl2[0], rfl2[1]), kr_), hip="both", balls=balls("rest", "kn" if not h else None)),
            [dx4 + 0.03 * i for i in range(5)])[0] + ("",))
    if not body.armed:
        # (an empty right hand stays as it is on the ground while the right knee comes up under him: the arm's
        # solve with his shoulders over the knee carried would turn it over at the wrist)
        i_up, i_on = len(K) - 3, len(K) - 2
        for n_ in ("clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"):
            if n_ in man.ix:
                K[i_on][1][b(n_)] = K[i_up][1][b(n_)].copy()
    # (rocked back towards his heels, his arms stretched out to his hands)
    both = (("l", Kl, rfl2[0], rfl2[1]), ("r", Kr, Fr, Rfr))
    # (his weight taken back over his knees while his hands are still down: when they come off, he is over his knees)
    K.append((T["rock"],) + up_key(body, dict(
        like=K[-1][1:3], at=(Kr[0] + FACE_ROCK[0], mid_y, FACE_ROCK[1]), pel=(("y", FACE_ROCK[2]),),
        s1=(12.0, 0.0, 0.0), s2=(12.0, 0.0, 0.0), ch=(8.0, 0.0, 0.0), nk=(10.0, 0.0, 4.0), hd=(10.0, 0.0, 6.0),
        **hands), knees=both, hip="both", balls=balls("rest", "kn")) + ("",))
    # (his hands off the ground, his trunk coming up over his knees, the weapon low in his right hand)
    arms_low = dict(
        arm_l=dict(rel="sh", at=(0.14, -0.04, -0.40), pole=(-0.5, -1.0, 0.0), d=(0.6, 0.2, -0.8), t=(0.3, 1.0, 0.2)),
        arm_r=dict(rel="sh", at=(0.15, 0.15, -0.41), pole=(-0.5, 0.9, 0.0), gun=((0.9, -0.05, -0.30), (0.3, 0.0, 1.0))),
        fin_l=(24.0, 18.0))
    K.append((T["off"],) + up_key(body, dict(
        like=K[-1][1:3], at=(Kr[0] - 0.12, mid_y, 0.47), pel=(("y", 30.0),),
        s1=(10.0, 0.0, 0.0), s2=(10.0, 0.0, 0.0), ch=(8.0, 0.0, 0.0), nk=(16.0, 0.0, 4.0), hd=(12.0, 2.0, 6.0),
        **dict(arms_low, arm_r=dict(arms_low["arm_r"], gun=OFF_GUN))), knees=both, hip="both",
        balls=balls("rest", "kn")) + ("",))
    # (up on his knees, his weight going over his right knee)
    K.append((T["tall"],) + up_key(body, dict(
        like=K[-1][1:3], at=(Kr[0] - 0.06, Kr[1] - 0.11, 0.5), pel=(("y", 4.0), ("x", 5.0)),
        s1=(2.0, 2.0, 0.0), s2=(4.0, 2.0, 0.0), ch=(4.0, 0.0, 0.0), nk=(16.0, 0.0, 4.0), hd=(12.0, 3.0, 6.0),
        cl=(0.0, 0.0, 0.0), cr=(0.0, 0.0, 0.0), **arms_low),
        knees=both, hip="both", balls=balls("rest", "kn")) + ("",))
    # (his left knee lifted off the ground, the foot behind it lifted with it; his weight on his right knee)
    G4, X4 = man.fk(K[-1][1], K[-1][2])
    K1 = X4[b("calf_l")] + np.array((0.10, 0.02, 0.16))
    F1 = K1 + (X4[b("foot_l")] - X4[b("calf_l")]) + np.array((0.06, 0.0, 0.02))
    R1 = qslerp(G4[b("foot_l")], Rfl, 0.2)
    K.append((T["kup"],) + up_key(body, dict(
        like=K[-1][1:3], at=(Kr[0] - 0.07, Kr[1] - 0.07, 0.5), pel=(("y", 0.0), ("x", 5.0)),
        s1=(2.0, 3.0, 0.0), s2=(4.0, 2.0, 0.0), ch=(4.0, 0.0, 0.0), nk=(16.0, 0.0, 4.0), hd=(12.0, 3.0, 6.0),
        **arms_low),
        knees=(("r", Kr, Fr, Rfr), ("l", K1, F1, R1, False)), hip=("r", Kr), balls=balls(None, "kn")) + ("",))
    # (his left foot brought forward under him, lifted)
    K.append((T["swing"],) + up_key(body, dict(
        like=K[-1][1:3], at=(Kr[0] - 0.07, Kr[1] - 0.06, 0.5), pel=(("y", -3.0), ("x", 5.0)),
        s1=(4.0, 3.0, 0.0), s2=(5.0, 2.0, 0.0), ch=(5.0, 0.0, 0.0), nk=(16.0, 0.0, 4.0), hd=(12.0, 3.0, 6.0),
        **arms_low),
        knees=(("r", Kr, Fr, Rfr),),
        feet=(("l", np.array((0.5 * (Kr[0] - 0.07 + Fl[0]), 0.5 * (Kl[1] + Fl[1]), 0.17)), qslerp(R1, Rfl, 0.7),
               (1.0, -0.2, 0.6)),),
        hip=("r", Kr), balls=balls(None, "kn")) + ("",))
    # (held over the spot where he will stand, turned as it will stand)
    K.append((T["hover"],) + up_key(body, dict(
        like=mix(K[-1][1:3], kn, 0.3), at=(Kr[0] - 0.07, Kr[1] - 0.07, 0.5), pel=(("y", 4.0), ("x", 4.0)),
        s1=(3.0, 2.0, 0.0), s2=(5.0, 2.0, -1.0), ch=(5.0, 0.0, -1.0), nk=(15.0, 0.0, 5.0), hd=(11.0, 4.0, 7.0),
        **arms_low),
        knees=(("r", Kr, Fr, Rfr),), feet=(("l", Fl + Z_ * 0.035, Rfl, (1.0, -0.3, 0.3)),), hip=("r", Kr),
            balls=balls("end", "kn")) + ("",))
    # (it comes down flat, from above; he comes up over it into his kneel)
    K.append((T["land"],) + up_key(body, dict(
        like=mix(K[-1][1:3], kn, 0.5), at=(0.5 * (Kr[0] + Xk[pe][0]) - 0.02, 0.5 * (Kr[1] + Xk[pe][1]) - 0.03, 0.5),
        pel=(("y", 6.0), ("x", 2.0)),
        s1=(7.0, 2.0, 0.0), s2=(9.0, 2.0, -2.0), ch=(7.0, 0.0, -2.0), nk=(15.0, 0.0, 5.0), hd=(11.0, 4.0, 7.0),
        **arms_low),
        knees=(("r", Kr, Fr, Rfr),), feet=(("l", Fl, Rfl, (1.0, -0.1, 0.5)),), hip=("r", Kr), balls=balls("end",
            "kn")) + ("",))
    # (on his right knee, his left hand on his left knee, the weapon low in his right hand)
    kl = Xk[b("calf_l")]
    K.append((T["kneel"],) + up_key(body, dict(
        like=kn, at=tuple(Xk[pe]), s1=(9.0, 2.0, 0.0), s2=(11.0, 2.0, -3.0), ch=(8.0, 0.0, -3.0),
        nk=(14.0, 0.0, 6.0), hd=(10.0, 4.0, 8.0), cl=(0.0, 0.0, 0.0), cr=(0.0, 0.0, 0.0),
        arm_l=dict(rel="abs", at=(kl[0] - 0.03, kl[1] - 0.02, kl[2] + 0.085), pole=(-0.4, -1.0, 0.2),
                   d=(0.8, 0.3, -0.5), t=(0.2, 1.0, 0.3)),
        arm_r=dict(rel="sh", at=(0.20, 0.17, -0.41), pole=(-0.7, 0.6, 0.0), gun=((0.9, 0.06, -0.34), (0.3, 0.0, 1.0))),
        fin_l=(24.0, 18.0)),
        knees=(("r", Kr, Fr, Rfr),), feet=(("l", Fl, Rfl, (1.0, -0.1, 0.5)),), hip=("r", Kr), balls=balls("end",
            "kn")) + ("",))
    # (rising over his left foot, his right knee off the ground, its toes still down; his trunk forward)
    K.append((T["rise"],) + up_key(body, dict(
        like=mix(K[-1][1:3], end, 0.35), at=(Fl[0] - 0.20, Fl[1] + 0.08, 0.66), pel=(("y", 28.0), ("x", 4.0)),
        s1=(10.0, 2.0, 0.0), s2=(10.0, 2.0, -2.0), ch=(6.0, 0.0, -2.0), nk=(8.0, 0.0, 5.0), hd=(6.0, 3.0, 6.0),
        arm_l=dict(rel="sh", at=(0.20, -0.02, -0.40), pole=(-0.5, -1.0, 0.0), d=(0.7, 0.2, -0.7), t=(0.3, 1.0, 0.2)),
        arm_r=dict(rel="sh", at=(0.22, 0.17, -0.39), pole=(-0.7, 0.6, 0.0), gun=((0.9, 0.05, -0.25), (0.3, 0.0, 1.0))),
        fin_l=(20.0, 14.0)),
        feet=(("l", Fl, Rfl, (1.0, -0.1, 0.3)), ("r", Fr, Rfr, (1.0, 0.0, -0.6))), balls=balls("end", "kn")) + ("",))
    # (his right toes lifted off the ground, straight up)
    K.append((T["toe"],) + up_key(body, dict(
        like=mix(K[-1][1:3], end, 0.4), at=(Fl[0] - 0.18, Fl[1] + 0.07, 0.70), pel=(("y", 26.0), ("x", 5.0)),
        s1=(9.0, 2.0, 0.0), s2=(9.0, 2.0, -2.0), ch=(6.0, 0.0, -2.0), nk=(7.0, 0.0, 4.0), hd=(6.0, 2.0, 5.0),
        arm_r=dict(rel="sh", at=(0.23, 0.18, -0.37), pole=(-0.7, 0.6, 0.0), gun=((0.9, 0.06, -0.15), (0.3, 0.0, 1.0)))),
        feet=(("l", Fl, Rfl, (1.0, -0.1, 0.3)),
              ("r", Fr + np.array((0.03, 0.0, 0.05)), qslerp(Rfr, Rfre, 0.15), (1.0, 0.0, -0.4))),
                  balls=balls("end", None)) + ("",))
    # (his right foot swung up beside the left, lifted clear; his weight over his left foot)
    K.append((T["step"],) + up_key(body, dict(
        like=mix(K[-1][1:3], end, 0.5), at=(Fl[0] - 0.14, Fl[1] + 0.05, 0.78), pel=(("y", 22.0), ("x", 5.0)),
        s1=(8.0, 2.0, 0.0), s2=(8.0, 2.0, -2.0), ch=(5.0, 0.0, -2.0), nk=(6.0, 0.0, 4.0), hd=(5.0, 2.0, 4.0),
        arm_r=dict(rel="sh", at=(0.27, 0.20, -0.30), pole=(-0.7, 0.6, 0.0), gun=((0.9, 0.0, 0.25), (-0.25, 0.0, 1.0))),
        grip=up_grip(body, T["step"])),
        feet=(("l", Fl, Rfl, (1.0, -0.1, 0.3)),
              ("r", lifted(Fr, Fre, 0.14, 0.55), qslerp(Rfr, Rfre, 0.7), (1.0, 0.1, 0.4))), balls=balls("end",
                  None)) + ("",))
    # (held over its spot beside the left, turned as it will stand)
    K.append((T["over"],) + up_key(body, dict(
        like=mix(K[-1][1:3], end, 0.6), at=(Fl[0] - 0.08, Fl[1] + 0.08, 0.83), pel=(("y", 14.0), ("x", 4.0)),
        s1=(6.0, 1.0, 0.0), s2=(6.0, 1.0, -1.0), ch=(4.0, 0.0, -1.0), nk=(6.0, 0.0, 3.0), hd=(5.0, 2.0, 3.0),
        arm_r=dict(rel="sh", at=(0.30, 0.17, -0.25), pole=(-0.7, 0.8, 0.0), gun=((0.85, -0.05, 0.50), (-0.5, 0.0,
            0.85))),
        grip=up_grip(body, T["over"])),
        feet=(("l", Fl, Rfl, (1.0, -0.1, 0.3)), ("r", Fre + Z_ * 0.035, Rfre, (1.0, 0.1, 0.3))), balls=balls("end",
            "end")) + ("",))
    # (it is down beside the left; his weight still over the left)
    K.append((T["plant"],) + up_key(body, dict(
        like=mix(K[-1][1:3], end, 0.7), at=(Fl[0] - 0.04, Fl[1] + 0.10, 0.86), pel=(("y", 10.0), ("x", 3.0)),
        s1=(5.0, 1.0, 0.0), s2=(5.0, 1.0, -1.0), ch=(3.0, 0.0, -1.0), nk=(6.0, 0.0, 3.0), hd=(5.0, 2.0, 3.0),
        arm_r=dict(rel="sh", at=(0.26, 0.08, -0.27), pole=(-0.7, 0.8, 0.0), gun=((0.55, -0.30, 0.78), (-0.75, 0.2,
            0.6))),
        grip=up_grip(body, T["plant"])),
        feet=(("l", Fl, Rfl, (1.0, -0.1, 0.3)), ("r", Fre, Rfre, (1.0, 0.1, 0.3))), balls=balls("end", "end")) + ("",))
    K.append((T["end"],) + end + ("stop",))
    return K


BACK_LEAN = {"prop": -70.0, "roll": -66.0, "side": -62.0, "foot": -63.0, "load": -51.0}
#                            (degrees: his pelvis pitched back as he sits propped on his left hand)
BACK_HAND = (-0.04, -0.12)   # (m: his left hand set down this far behind (-x) and out (-y) from his left shoulder)


def up_back_keys(body):
    man = body.man
    b = man.b
    T = UP_BACK
    pe = b("pelvis")
    rest = back_rest(body)
    idle = body.pose("idle", 0.0)
    Gr, Xr = man.fk(*rest)
    Gi, Xi = man.fk(*idle)
    zr = float(Xr[pe][2])
    k0 = mirrored(body, body.pose("fire_kneel", 0.0))
    Gk0, Xk0 = man.fk(*k0)
    kz = float(Xk0[b("calf_l")][2]) - KNEE_SINK
    # (on his left hip: where its knee comes down, a thigh's length forward and out from his left hip)
    side_pel = (("z", -18.0), ("y", BACK_LEAN["side"]), ("x", 36.0))
    trial = pose_of(body, dict(like=rest, at=(Xr[pe][0] + 0.02, Xr[pe][1] - 0.04, zr), pel=side_pel, ease=False,
                               clean=False))
    hipl = man.fk(*trial)[1][b("thigh_l")]
    Lt = thigh_len(body, "l")
    reach = math.sqrt(max(0.0, Lt * Lt - (hipl[2] - kz) ** 2)) * 0.97
    a = math.radians(-24.0)
    Kl = np.array((hipl[0] + reach * math.cos(a), hipl[1] + reach * math.sin(a), kz))
    S = Kl[:2] - Xk0[b("calf_l")][:2]
    kn = shifted(body, k0, float(S[0]), float(S[1]))
    kn[1][pe] = kn[1][pe] - Z_ * KNEE_SINK
    Fr0 = Xk0[b("foot_r")] + np.array((S[0], S[1], 0.0))
    end = shifted(body, idle, float(Fr0[0] - Xi[b("foot_r")][0]), float(Fr0[1] - Xi[b("foot_r")][1]))
    Ge, Xe = man.fk(*end)
    Fr, Rfr = Fr0, Ge[b("foot_r")]
    ik2(man, kn[0], kn[1], LEG["r"], Fr, end_R=Rfr)
    kn = (foot_clear(body, kn[0], kn[1], "l", 0.002), kn[1])
    Gk, Xk = man.fk(*kn)
    Fl, Rfl = Xk[b("foot_l")].copy(), Gk[b("foot_l")].copy()
    Fle = Xe[b("foot_l")]
    # (his left hand set down beside and a little behind where his left shoulder will be as he sits on his left hip,
    # so that the arm he props on goes down from it, not back behind his back)
    trial_s = pose_of(body, dict(like=rest, at=(Xr[pe][0], Xr[pe][1] - 0.06, zr), pel=side_pel, sit=True,
                                 s1=(4.0, -4.0, 0.0), s2=(4.0, -4.0, 4.0), ch=(3.0, -3.0, 4.0), ease=False, clean=False))
    Sl_ = man.fk(*trial_s)[1][b("upperarm_l")]
    HL = np.array((Sl_[0] + BACK_HAND[0], Sl_[1] + BACK_HAND[1]))
    K = [(0.0,) + rest + ("stop",)]
    # (the foot of that knee while he is on his hip: tucked back under him and toward his right, the shin across
    # behind the knee as a man sitting on one hip has it (the thigh turned out, not in))
    dk = unit(np.array((Kl[0] - hipl[0], Kl[1] - hipl[1], 0.0)))
    a2 = math.radians(SIDE_TUCK)
    dsh = np.array((dk[0] * math.cos(a2) - dk[1] * math.sin(a2), dk[0] * math.sin(a2) + dk[1] * math.cos(a2), 0.0))
    Ls = float(np.linalg.norm(man.H[b("foot_l")] - man.H[b("calf_l")]))
    Fl_side = np.array((Kl[0], Kl[1], 0.0)) + dsh * math.sqrt(max(0.0, Ls * Ls - (0.08 - Kl[2]) ** 2)) * 0.995
    # (he props himself up on his left hand, his head and shoulders coming up, his feet lifted and drawn in)
    prop_sp = dict(
        like=rest, at=(Xr[pe][0], Xr[pe][1], zr), pel=(("z", -6.0), ("y", BACK_LEAN["prop"]), ("x", 10.0)), sit=True,
        s1=(10.0, -2.0, 0.0), s2=(12.0, -2.0, 2.0), ch=(10.0, -2.0, 2.0), nk=(16.0, 0.0, 4.0), hd=(10.0, 0.0, 4.0),
        arm_l=dict(rel="abs", at=(HL[0], HL[1], 0.06), pole=(0.3, -1.0, 0.4), d=(-0.3, -0.95, -0.05),
                   t=(0.95, -0.3, 0.1), plant=True),
        arm_r=dict(rel="chest", at=(0.30, 0.06, -0.22), pole=(-0.2, 1.0, -0.5), gun=((0.80, -0.45, 0.35), (-0.3,
            0.2, 1.0))),
        fin_l=(6.0, -4.0))
    prop_feet = (("r", lifted(Xr[b("foot_r")], Fr, 0.05, 0.35), qslerp(Gr[b("foot_r")], Rfr, 0.3), (1.0, 0.2, 0.6)),
                 ("l", lifted(Xr[b("foot_l")], Fl_side + Z_ * 0.08, 0.05, 0.3), None, (1.0, -0.3, 0.6)))
    prop = seat_key(body, prop_sp, rest, feet=prop_feet)
    G1, X1 = man.fk(*prop)
    # (his left hand brought round over the spot it props on, turned as it will be, his head coming up)
    # (the hand carried over the ground on an arc, not swung down through it; his feet stay where they lay)
    hl_, Al_ = b("hand_l"), man.side["l"]
    Rh = qslerp(Gr[hl_], G1[hl_], 0.4)
    # (still in it a moment, the hand at the top of its arc: no in-between that sinks his chest as he sits up)
    K.append((T["reach"],) + up_key(body, dict(
        like=rest, nk=(14.0, 0.0, -6.0), hd=(10.0, 0.0, -6.0),
        arm_l=dict(rel="abs", at=lifted(Xr[hl_], X1[hl_], 0.16, 0.45), pole=(0.2, -1.0, 0.5), d=Rh @ Al_["d0"],
                   t=Rh @ Al_["t0"]),
        fin_l=(6.0, -4.0))) + ("stop",))
    # (sitting up on his seat, half way)
    Xp = man.fk(*prop)[1]
    # (the hand coming down over its spot from above, turned as it will be, not dragged onto it along the ground)
    sit1 = seat_key(body, dict(
        like=mix(K[-1][1:3], prop, 0.5), at=tuple(0.5 * (Xr[pe] + Xp[pe])), sit=True,
        arm_l=dict(rel="abs", at=X1[hl_] + np.array((0.04, -0.06, 0.12)), pole=(0.2, -1.0, 0.5),
                   d=G1[hl_] @ Al_["d0"], t=G1[hl_] @ Al_["t0"]),
        fin_l=(6.0, -4.0)),
        feet=(("r", lifted(Xr[b("foot_r")], Fr, 0.045, 0.18), qslerp(Gr[b("foot_r")], Rfr, 0.15), (1.0, 0.2, 0.6)),
              ("l", lifted(Xr[b("foot_l")], Fl_side + Z_ * 0.08, 0.045, 0.15), None, (1.0, -0.3, 0.6))),
        prev=K[-1][1:3])
    K.append((T["sit1"],) + sit1 + ("",))
    prop = seat_key(body, dict(prop_sp, at=tuple(Xp[pe] + (man.H[pe] - man.H[pe]))), prev=sit1, feet=prop_feet)
    K.append((T["prop"],) + prop + ("",))
    HL3 = X1[b("hand_l")].copy()
    prop_l = hand_as(body, G1, X1, "l", (0.3, -1.0, 0.4))
    # (rolled onto his left hip, leaning on that hand; his left knee falling to the ground, the right foot drawn in)
    K.append((T["roll"],) + seat_key(body, dict(
        like=K[-1][1:3], at=(Xr[pe][0] - 0.01, Xr[pe][1] - 0.05, zr), pel=(("z", -12.0), ("y", BACK_LEAN["roll"]), ("x", 34.0)),
        sit=True,
        s1=(6.0, -4.0, 0.0), s2=(6.0, -4.0, 3.0), ch=(4.0, -3.0, 3.0), nk=(14.0, -2.0, 6.0), hd=(8.0, 0.0, 6.0),
        arm_l=prop_l,
        arm_r=dict(rel="chest", at=(0.32, 0.04, -0.24), pole=(-0.2, 1.0, -0.5), gun=((0.80, -0.40, 0.35), (-0.3,
            0.2, 1.0))),
        fin_l=(6.0, -4.0)),
        knees=(("l", lifted(Xr[b("calf_l")], Kl, 0.06, 0.6), lifted(Xr[b("foot_l")], Fl_side + Z_ * 0.08, 0.05, 0.6),
                None, False),),
        feet=(("r", lifted(Xr[b("foot_r")], Fr, 0.05, 0.7), qslerp(Gr[b("foot_r")], Rfr, 0.6), (1.0, 0.2, 0.6)),)
        , prev=K[-1][1:3]) + ("",))
    # (between: leaning back onto that hand as he turns onto his hip, the hand still on its spot)
    pr_, ro_ = K[-2][1:3], K[-1][1:3]
    Xpr, Xro = man.fk(*pr_)[1], man.fk(*ro_)[1]
    mp = mix(pr_, ro_, 0.4)
    lb = up_key(body, dict(like=mp, at=tuple(man.fk(*mp)[1][pe]), arm_l=dict(prop_l, plant=True), fin_l=(6.0, -4.0)))
    K.insert(len(K) - 1, (T["lb"],) + lb + ("",))
    # (his left knee down on the ground, his weight on his left hip, knee and hand; his right foot coming flat)
    K.append((T["side"],) + seat_key(body, dict(
        like=K[-1][1:3], at=(Xr[pe][0] + 0.0, Xr[pe][1] - 0.06, zr), pel=side_pel, sit=True,
        s1=(4.0, -4.0, 0.0), s2=(4.0, -4.0, 4.0), ch=(3.0, -3.0, 4.0), nk=(12.0, -2.0, 6.0), hd=(6.0, 0.0, 6.0),
        arm_l=prop_l,
        arm_r=dict(rel="chest", at=(0.32, 0.04, -0.26), pole=(-0.2, 1.0, -0.5), gun=((0.85, -0.30, 0.25), (-0.3,
            0.2, 1.0))),
        fin_l=(6.0, -4.0)),
        knees=(("l", Kl, Fl_side + Z_ * 0.08, None),),
        feet=(("r", Fr + Z_ * 0.01, Rfr, (1.0, 0.2, 0.6)),), prev=K[-1][1:3]) + ("",))
    # (where his left knee came down is where it stays: the kneel and the keys before it set round that spot)
    dK = man.fk(*K[-1][1:3])[1][b("calf_l")] - Kl
    dK[2] = 0.0
    Kl, Xk, Fl = Kl + dK, Xk + dK, Fl + dK
    # (his right foot flat; he begins to push up off hand, knee and foot)
    K.append((T["foot"],) + seat_key(body, dict(
        like=K[-1][1:3], at=(Xr[pe][0] + 0.02, Xr[pe][1] - 0.06, zr + 0.02), pel=(("z", -14.0), ("y", BACK_LEAN["foot"]), ("x",
            32.0)),
        s1=(4.0, -3.0, 0.0), s2=(4.0, -3.0, 3.0), ch=(3.0, -2.0, 3.0), nk=(12.0, -2.0, 6.0), hd=(6.0, 0.0, 6.0),
        arm_l=prop_l,
        arm_r=dict(rel="chest", at=(0.32, 0.06, -0.28), pole=(-0.2, 1.0, -0.5), gun=((0.9, -0.2, 0.2), (-0.3, 0.2,
            1.0))),
        fin_l=(6.0, -4.0)),
        knees=(("l", Kl, Fl_side + Z_ * 0.08, None),), feet=(("r", Fr, Rfr, (1.0, 0.2, 0.6)),), hip=("l", Kl),
            prev=K[-1][1:3]) + ("",))
    # (his weight coming forward onto his left knee and right foot, the hand lifting straight off its spot)
    Gfo, Xfo = man.fk(*K[-1][1:3])
    K.append((T["load"],) + seat_key(body, dict(
        like=K[-1][1:3], at=(Xr[pe][0] + 0.04, Xr[pe][1] - 0.06, zr + 0.02), pel=(("z", -14.0), ("y", BACK_LEAN["load"]), ("x",
            29.0)),
        s1=(8.0, -3.0, 0.0), s2=(8.0, -3.0, 3.0), ch=(5.0, -2.0, 3.0), nk=(10.0, -2.0, 5.0), hd=(6.0, 0.0, 4.0),
        arm_l=dict(hand_as(body, Gfo, Xfo, "l", (0.3, -1.0, 0.4)), at=Xfo[b("hand_l")] + Z_ * 0.06),
        arm_r=dict(rel="chest", at=(0.31, 0.13, -0.29), pole=(-0.2, 1.0, -0.5), gun=((0.9, -0.02, 0.12), (-0.25,
            0.2, 1.0))),
        fin_l=(6.0, -4.0)),
        knees=(("l", Kl, Fl_side + Z_ * 0.08, None),), feet=(("r", Fr, Rfr, (1.0, 0.2, 0.6)),), hip=("l", Kl),
        prev=K[-1][1:3]) + ("",))
    # (his weight brought forward over his left knee and right foot, his hand coming off the ground, his seat still
    # on it)
    Gft, Xft = man.fk(*K[-1][1:3])
    off_l = dict(hand_as(body, Gft, Xft, "l", (0.3, -1.0, 0.4)), at=Xft[b("hand_l")] + np.array((0.06, -0.01, 0.07)))
    K.append((T["lean"],) + seat_key(body, dict(
        like=K[-1][1:3], at=(Xr[pe][0] + 0.12, Xr[pe][1] - 0.07, zr + 0.02), pel=(("z", -12.0), ("y", -20.0), ("x",
            26.0)),
        s1=(16.0, -3.0, 0.0), s2=(16.0, -3.0, 2.0), ch=(10.0, -2.0, 2.0), nk=(8.0, -2.0, 4.0), hd=(6.0, 0.0, 4.0),
        arm_l=off_l,
        arm_r=dict(rel="chest", at=(0.30, 0.18, -0.30), pole=(-0.2, 1.0, -0.5), gun=((0.9, 0.05, 0.05), (-0.2, 0.2,
            1.0))),
        fin_l=(8.0, 0.0)),
        knees=(("l", Kl, Fl_side + Z_ * 0.08, None),), feet=(("r", Fr, Rfr, (1.0, 0.2, 0.6)),), hip=("l", Kl),
        prev=K[-1][1:3]) + ("",))
    on_thigh = 0.5 * (Kl + Xk[b("thigh_l")]) + np.array((0.04, -0.08, 0.12))
    # (up on his left knee (made first: the push turns its left foot to it), his left hand on his thigh, the weapon
    # low in his right hand outside his right knee)
    hip = Xk[b("thigh_l")]
    kneel_k = up_key(body, dict(
        like=kn, at=tuple(Xk[pe]), s1=(8.0, -2.0, 0.0), s2=(10.0, -2.0, 3.0), ch=(8.0, 0.0, 3.0), nk=(12.0, 0.0, -6.0),
        hd=(8.0, -4.0, -8.0), cl=(0.0, 0.0, 0.0), cr=(0.0, 0.0, 0.0),
        arm_l=dict(rel="abs", at=on_thigh, pole=(-0.5, -1.0, 0.3),
                   d=(0.7, 0.2, -0.7), t=(0.3, 1.0, 0.3)),
        arm_r=dict(rel="sh", at=(0.22, 0.33, -0.36), pole=(-0.7, 0.8, 0.0), gun=((0.9, 0.18, -0.35), (0.3, 0.0, 1.0))),
        fin_l=(22.0, 18.0)),
        knees=(("l", Kl, Fl, Rfl),), feet=(("r", Fr, Rfr, (1.0, 0.2, 0.6)),), hip=("l", Kl))
    Gkk, Xkk = man.fk(*kneel_k)
    # (off his seat: his hips coming up over his left knee, the left foot brought round behind it, lifted; his left
    # hand on its way to his thigh)
    up = mix(K[-1][1:3], kn, 0.55)
    mid_l = 0.5 * (man.fk(*K[-1][1:3])[1][b("hand_l")] + on_thigh) + Z_ * 0.04
    # (the left foot carried round behind the knee, the shin turning about it, the foot turned with the shin)
    Fk_, Rk_ = Xkk[b("foot_l")], Gkk[b("foot_l")]
    Fp = about_knee(Kl, Fl_side + Z_ * 0.08, Fk_, 0.45, 0.11, Ls * 0.995)
    K.append((T["push"],) + up_key(body, dict(
        like=up, at=(0.5 * (Xr[pe][0] + Xk[pe][0]) + 0.12, 0.5 * (Xr[pe][1] + Xk[pe][1]) - 0.04, 0.3),
        pel=(("z", -8.0), ("y", 2.0), ("x", 14.0)),
        s1=(12.0, -3.0, 0.0), s2=(12.0, -3.0, 2.0), ch=(8.0, -2.0, 2.0), nk=(10.0, -2.0, 4.0), hd=(8.0, 0.0, 4.0),
        arm_l=dict(rel="abs", at=mid_l, pole=(-0.3, -1.0, 0.0), d=(0.6, -0.2, -0.8), t=(0.3, 1.0, 0.2)),
        arm_r=dict(rel="sh", at=(0.24, 0.27, -0.33), pole=(-0.6, 0.8, 0.0), gun=((0.9, 0.12, -0.20), (0.2, 0.0, 1.0))),
        fin_l=(12.0, 8.0)),
        knees=(("l", Kl, Fp, Rz(math.degrees(math.atan2(Fp[1] - Kl[1], Fp[0] - Kl[0]) - math.atan2(Fk_[1] - Kl[1],
                                                                                                   Fk_[0] - Kl[0]))) @ Rk_),),
        feet=(("r", Fr, Rfr, (1.0, 0.2, 0.6)),),
        hip=("l", Kl)) + ("",))
    # (before it: his left foot lifted off its spot first, straight up, as the shin starts round)
    le_, pu_ = K[-2][1:3], K[-1][1:3]
    Gle = man.fk(*le_)[0]
    Fu = about_knee(Kl, Fl_side + Z_ * 0.08, Fk_, 0.12, 0.08, Ls * 0.995)
    mu = mix(le_, pu_, 0.35)
    unt = up_key(body, dict(like=mu, at=tuple(man.fk(*mu)[1][pe])),
                 knees=(("l", Kl, Fu, Rz(math.degrees(math.atan2(Fu[1] - Kl[1], Fu[0] - Kl[0]) -
                                                       math.atan2(Fl_side[1] - Kl[1], Fl_side[0] - Kl[0]))) @
                         Gle[b("foot_l")]),), feet=(("r", Fr, Rfr, (1.0, 0.2, 0.6)),), hip=("l", Kl))
    K.insert(len(K) - 1, (T["unt"],) + unt + ("",))
    K.append((T["kneel"],) + kneel_k + ("",))
    Gkk, Xkk = man.fk(K[-1][1], K[-1][2])
    Fl, Rfl = Xkk[b("foot_l")].copy(), Gkk[b("foot_l")].copy()
    toes = {"l": K[-1][1][b("ball_l")].copy()} if "ball_l" in man.ix else None
    # (before it: his left foot held over its spot behind his knee, to come straight down onto it)
    lk = mix(K[-2][1:3], K[-1][1:3], 0.6)
    hov = up_key(body, dict(like=lk, at=tuple(man.fk(*lk)[1][pe])), knees=(("l", Kl, Fl + Z_ * 0.03, Rfl),),
                 feet=(("r", Fr, Rfr, (1.0, 0.2, 0.6)),), hip=("l", Kl))
    K.insert(len(K) - 1, (T["hov"],) + hov + ("",))
    # (rising over his right foot, the left knee off the ground, its toes still down)
    up = mix(K[-1][1:3], end, 0.35)
    K.append((T["rise"],) + up_key(body, dict(
        like=up, at=(Fr[0] - 0.30, Fr[1] - 0.10, 0.60), pel=(("y", 28.0), ("x", -4.0)),
        s1=(8.0, -2.0, 0.0), s2=(9.0, -2.0, 2.0), ch=(6.0, 0.0, 2.0), nk=(10.0, 0.0, -5.0), hd=(8.0, -3.0, -6.0),
        arm_l=dict(rel="sh", at=(0.14, -0.10, -0.40), pole=(-0.5, -1.0, 0.0), d=(0.7, 0.2, -0.7), t=(0.3, 1.0, 0.2)),
        arm_r=dict(rel="sh", at=(0.22, 0.31, -0.36), pole=(-0.7, 0.8, 0.0), gun=((0.9, 0.16, -0.30), (0.3, 0.0, 1.0))),
        fin_l=(20.0, 14.0)),
        feet=(("r", Fr, Rfr, (1.0, 0.1, 0.3)), ("l", Fl, Rfl, (1.0, 0.0, -0.2))), balls=toes) + ("",))
    # (his left toes lifted off the ground, straight up)
    up = mix(K[-1][1:3], end, 0.4)
    K.append((T["toe"],) + up_key(body, dict(
        like=up, at=(Fr[0] - 0.24, Fr[1] - 0.09, 0.67), pel=(("y", 24.0), ("x", -4.0)),
        s1=(7.0, -2.0, 0.0), s2=(8.0, -2.0, 2.0), ch=(5.0, 0.0, 2.0), nk=(9.0, 0.0, -5.0), hd=(7.0, -2.0, -5.0),
        arm_r=dict(rel="sh", at=(0.23, 0.30, -0.35), pole=(-0.7, 0.8, 0.0), gun=((0.9, 0.15, -0.26), (0.3, 0.0, 1.0)))),
        feet=(("r", Fr, Rfr, (1.0, 0.1, 0.3)),
              ("l", Fl + np.array((0.02, 0.0, 0.07)), Rfl, (1.0, 0.0, -0.4))), balls=toes) + ("",))
    # (his left foot stepped up beside the right, lifted clear; his weight over his right foot)
    up = mix(K[-1][1:3], end, 0.5)
    K.append((T["step"],) + up_key(body, dict(
        like=up, at=(Fr[0] - 0.02, Fr[1] - 0.07, 0.82), pel=(("y", 14.0), ("x", -5.0)),
        s1=(5.0, -2.0, 0.0), s2=(6.0, -2.0, 2.0), ch=(4.0, 0.0, 2.0), nk=(8.0, 0.0, -4.0), hd=(6.0, -2.0, -4.0),
        arm_r=dict(rel="sh", at=(0.27, 0.25, -0.28), pole=(-0.7, 0.8, 0.0), gun=((0.9, 0.05, 0.25), (-0.25, 0.0, 1.0))),
        grip=up_grip(body, T["step"])),
        feet=(("r", Fr, Rfr, (1.0, 0.1, 0.3)),
              ("l", lifted(Fl, Fle, 0.14, 0.62), qslerp(Rfl, Ge[b("foot_l")], 0.6), (1.0, -0.1, 0.4)))) + ("",))
    up = mix(K[-1][1:3], end, 0.6)
    K.append((T["over"],) + up_key(body, dict(
        like=up, at=(0.65 * Fr[0] + 0.35 * Fle[0] + 0.03, 0.65 * Fr[1] + 0.35 * Fle[1], 0.86), pel=(("y", 9.0), ("x",
            -3.0)),
        s1=(4.0, -1.0, 0.0), s2=(5.0, -1.0, 1.0), ch=(3.0, 0.0, 1.0), nk=(8.0, 0.0, -3.0), hd=(6.0, -2.0, -3.0),
        arm_r=dict(rel="sh", at=(0.30, 0.20, -0.24), pole=(-0.7, 0.8, 0.0), gun=((0.85, -0.05, 0.50), (-0.5, 0.0,
            0.85))),
        grip=up_grip(body, T["over"])),
        feet=(("r", Fr, Rfr, (1.0, 0.1, 0.3)), ("l", Fle + Z_ * 0.035, Ge[b("foot_l")], (1.0, -0.1, 0.3)))) + ("",))
    up = mix(K[-1][1:3], end, 0.7)
    K.append((T["down"],) + up_key(body, dict(
        like=up, at=(0.5 * (Fr[0] + Fle[0]) + 0.08, 0.5 * (Fr[1] + Fle[1]) + 0.0, 0.89), pel=(("y", 6.0), ("x", -2.0)),
        s1=(4.0, -1.0, 0.0), s2=(4.0, -1.0, 1.0), ch=(3.0, 0.0, 1.0), nk=(8.0, 0.0, -3.0), hd=(6.0, -2.0, -3.0),
        arm_r=dict(rel="sh", at=(0.25, 0.07, -0.27), pole=(-0.7, 0.8, 0.0), gun=((0.55, -0.30, 0.78), (-0.75, 0.2,
            0.6))),
        grip=up_grip(body, T["down"])),
        feet=(("r", Fr, Rfr, (1.0, 0.1, 0.3)), ("l", Fle, Ge[b("foot_l")], (1.0, -0.1, 0.3)))) + ("",))
    K.append((T["end"],) + end + ("stop",))
    return K


UP_PINS = {"face": (("arm_l", UP_FACE["lift"], UP_FACE["off"]), ("arm_r", UP_FACE["lift"], UP_FACE["off"]),
                    ("knee_l", UP_FACE["stir"], UP_FACE["fours"]), ("knee_l", UP_FACE["l_dn"], UP_FACE["tall"]),
                    ("knee_r", UP_FACE["stir"], UP_FACE["l_dn"]), ("knee_r", UP_FACE["r_dn"], UP_FACE["kneel"]),
                    ("leg_l", UP_FACE["stir"], UP_FACE["fours"]), ("leg_l", UP_FACE["l_dn"], UP_FACE["tall"]),
                    ("leg_r", UP_FACE["stir"], UP_FACE["l_dn"]), ("leg_r", UP_FACE["r_dn"], UP_FACE["rise"]),
                    ("leg_l", UP_FACE["kup"], UP_FACE["end"]), ("leg_r", UP_FACE["over"], UP_FACE["end"]),
                    # (a knee stepped forward: its foot carried straight from key to key, lifted, not swung down
                    # through the ground between them)
                    ("leg_l", UP_FACE["l_up"], UP_FACE["l_dn"]), ("leg_r", UP_FACE["r_up"], UP_FACE["r_dn"])),
           "back": (("arm_l", UP_BACK["sit1"], UP_BACK["load"]), ("knee_l", UP_BACK["side"], UP_BACK["kneel"]),
                    ("leg_r", UP_BACK["foot"], UP_BACK["step"]), ("leg_l", UP_BACK["hov"], UP_BACK["end"]),
                    ("leg_r", UP_BACK["step"], UP_BACK["end"]))}


def up_grip(body, t):
    """His right hand on the weapon: as the aim has it, shifting to the carry's as the weapon comes up to port."""
    u = sm((t - GRIP_T[0]) / (GRIP_T[1] - GRIP_T[0]))
    a, c = body.rel0["r"], body.rel_idle["r"]
    return qslerp(a[0], c[0], u), a[1] * (1 - u) + c[1] * u


def up_keys(body, way):
    key = ("up_keys", way)
    if key not in body.poses:
        body.poses[key] = Keys(body, (up_face_keys if way == "face" else up_back_keys)(body))
    return body.poses[key]


LOCK_FPS = 120               # (a second: how finely contact_locks follows what of a planted limb bears on the ground)


def contact_locks(body, fn, pins, prior=None):
    """For each hold in `pins` (limb, from, to), how far to move that hand's, foot's or knee's joint over the ground
    (x, y) at each LOCK_FPS'th of a second so that what of it bears on the ground stays where it is while the bones
    above it turn: what bears on the ground in two steps running (under 1 cm, as the checks read it) has its mean
    way over the ground taken back, step by step from the hold's first key, as `fn` (the move without the locks)
    has it. Less an even share of where that leaves it at the hold's last key, so the first and the last key are the
    keys' own: {limb: [(from, to, times, shifts)]}. With `prior` (the locks `fn` already plays), what is left is
    added to them."""
    man = body.man
    out = {}
    for j, (limb, t0, t1) in enumerate(pins):
        part = ("hand_" if limb.startswith("arm") else "foot_" if limb.startswith("leg") else "knee_") + limb[-1]
        idx = np.nonzero(np.isin(body.pa, [man.b(n) for n in CONTACT[part] if n in man.ix]))[0]
        n = max(2, int(round((t1 - t0) * LOCK_FPS)) + 1)
        tt = np.linspace(t0, t1, n)
        V = [body.skin(*man.fk(*fn(float(x))))[idx] for x in tt]
        S = np.zeros((n, 2))
        air = np.zeros(n)
        for k in range(1, n):
            a, b_ = V[k - 1], V[k]
            m = (a[:, 2] < 0.01) & (b_[:, 2] < 0.01)
            S[k] = S[k - 1] - ((b_ - a)[m][:, :2].mean(0) if m.sum() >= 3 else 0.0)
            air[k] = 0.0 if m.sum() >= 3 else 1.0
        # (what is left at the last key is let go of while the hand, foot or knee is off the ground, if it is at all
        # in the hold (it comes down, or lifts, that much to one side): else by an even share of it at each step)
        share = np.cumsum(air) / air.sum() if air.sum() >= 2 else (tt - t0) / max(t1 - t0, 1e-9)
        S = S - np.outer(share, S[-1])
        if prior is not None:
            S = S + next(h[3] for h in prior[limb] if abs(h[0] - t0) < 1e-9 and abs(h[1] - t1) < 1e-9)
        out.setdefault(limb, []).append((t0, t1, tt, S))
    return out


UP_LOCK_PASSES = 2


def up_toes(body, way):
    """The get-up's toes (toe_envelope) as the move without the locks bends them, found once."""
    key = ("up_toes", way)
    if key not in body.poses:
        body.poses[key] = {}
        body.poses[key] = toe_envelope(body, lambda x: up_pose(body, way, x, locks=None), UP_LONG)
    return body.poses[key]


def up_locks(body, way):
    """The get-up's contact_locks, found once (UP_LOCK_PASSES times over, each on the move the last leaves)."""
    key = ("up_locks", way)
    if key not in body.poses:
        L = None
        for _ in range(UP_LOCK_PASSES):
            L = contact_locks(body, lambda x, L=L: up_pose(body, way, x, locks=L), UP_PINS[way], prior=L)
        body.poses[key] = L
    return body.poses[key]


UP_ONE_TURN = {"face": ("arm_r",), "back": ("arm_l",)}     # (a planted hand that keeps one turn while it bears (play's one_turn);
#                                                   the others turn on their spot about what of them bears (up_locks))


UP_SEAM = 0.012              # (s of key time: a hold's first or last key is eased through over twice this)


SEAM_DEG = 1.0               # (degrees: a hold's end whose poses either side of it differ by more than this at a joint
#                              (or SEAM_MM at his pelvis) is eased through (up_pose))
SEAM_MM = 1.5


def up_seams(body, way):
    """The key times where a hold (UP_PINS) begins or ends, inside the get-up, and the poses either side of it are
    not the same (found once, without the locks)."""
    key = ("up_seams", way)
    if key not in body.poses:
        man = body.man
        keep = [i for i in range(man.nb) if man.rig.names[i] not in GUN_BONES + WHIP]
        pe = man.b("pelvis")
        out = []
        for x in sorted({x for _, a, b in UP_PINS[way] for x in (a, b) if 1e-6 < x < UP_LONG - 1e-6}):
            A, B = _up_pose(body, way, x - 1e-4, None), _up_pose(body, way, x + 1e-4, None)
            d = max(angle_of(A[0][i].T @ B[0][i]) for i in keep)
            if d > SEAM_DEG or float(np.linalg.norm(A[1][pe] - B[1][pe])) > SEAM_MM * 1e-3:
                out.append(x)
        body.poses[key] = out
    return body.poses[key]


def up_pose(body, way, t, locks="found"):
    """The get-up at t (_up_pose). Where a hold begins or ends (up_seams) the pose solved with it and the one solved
    without it are not quite the same (a knee let go of, his hips no longer fitted to it): the get-up is eased from
    the pose UP_SEAM before it to the pose UP_SEAM after it there, so he does not jerk from one to the other; the
    weapon carried on his right hand through it."""
    if 0.0 < t < UP_LONG - 1e-9:
        for b in up_seams(body, way):
            if abs(t - b) < UP_SEAM - 1e-9:
                def at(x):
                    key = ("up_seam", way, round(x, 6))
                    if locks != "found":
                        return _up_pose(body, way, x, locks)
                    if key not in body.poses:
                        body.poses[key] = _up_pose(body, way, x, locks)
                    return body.poses[key]
                a, c = at(b - UP_SEAM), at(b + UP_SEAM)
                lq, mv = mix(a, c, sm((t - b + UP_SEAM) / (2.0 * UP_SEAM)))
                gt = up_clip_time(body, way, t) if ("up_pace", way) in body.poses else t
                return keep_prop(body, *gun_rides(body, lq, mv, up_grip(body, gt)), body.prop0)
    return _up_pose(body, way, t, locks)


def _up_pose(body, way, t, locks="found"):
    """The get-up at t: the keys played with each planted hand, knee and foot held (UP_PINS) and kept from skidding
    (up_locks; `locks` None: without, or the locks given), the left hand onto the weapon, the toes (up_toes)."""
    man = body.man
    keys = up_keys(body, way)
    if t <= 0.0 or t >= UP_LONG - 1e-9:
        return keys.at(min(max(t, 0.0), UP_LONG))
    if isinstance(locks, str):
        locks = up_locks(body, way)
    # (his right hand's grip shifts at GRIP_T as the clip is played: at the clip time of this key time, once the
    # pace (up_pace) is found)
    gt = (lambda x: up_clip_time(body, way, x)) if ("up_pace", way) in body.poses else (lambda x: x)
    lq, mv = play(body, keys, t, pins=UP_PINS[way], grip=lambda x: up_grip(body, gt(x)), wrists=False,
                  shin_first=True, one_turn=UP_ONE_TURN[way], behind=False,
                  locks=locks, pivot=True, gun_clear=True)
    # (his left hand comes onto the weapon as it comes up to port)
    t0, t1 = LEFT_ON_T[way]
    w = sm((t - t0) / (t1 - t0))
    if w > 0.0 and body.armed:
        G, X = man.fk(lq, mv)
        h = man.b("hand_l")
        Rl, pl = body.rel_idle["l"]
        Gw, Xw = G[body.wi], X[body.wi]
        # (the hand turns at the wrist from how it was to its grip, measured on the forearm, so the wrist keeps a
        # living bend on the way; it is on its grip exactly at the end)
        r0 = lq[h].copy()
        ik2(man, lq, mv, ARM["l"], X[h] * (1 - w) + (Xw + Gw @ pl) * w, end_R=Gw @ Rl, share=0.3)
        lq[h] = qslerp(r0, lq[h], w)
        # (the elbow swung, the hand kept, so the wrist is not bent sideways past its range on the way)
        lq = wrist_swivel(body, lq, mv, "l")
    lq = toes_bent(body, lq, up_toes(body, way), t)
    return lq, mv


# ---- the get-up's pace: the keys above are laid out on their own clock ("key time"); the clip plays that clock
# slower where a joint would turn faster than a living man's (pushing up, a knee swung through) and quicker where
# little moves (the first stir, the last settling), the same poses in the same order, its 1.6 s and its first and
# last pose exactly as they are. What bears on the ground, his balance and his joints' ranges are the poses' own and
# do not change with the pace; only how fast he goes through them does.
UP_TURN = 830.0              # (degrees a second: the most any joint of his turns as the get-up is played: rom.py's 900
#                              less a margin for the game's in-between)
PACE_FPS = 120               # (a second: how finely the key time's joint turns are read)
PACE_BLUR = 2.0              # (steps of 1/PACE_FPS: the pace eased so it does not jerk from one step to the next)


def up_pace(body, way):
    """(t, k): the clip's time t at which each 1/PACE_FPS step k of the get-up's key time is played (both arrays
    0 .. UP_LONG): each step's clip time is its key time stretched by its fastest joint over UP_TURN where that is
    over 1, and by an even factor under 1 elsewhere (found so the whole stays UP_LONG), eased over PACE_BLUR steps,
    never less than its fastest joint needs. Found once."""
    key = ("up_pace", way)
    if key not in body.poses:
        man = body.man
        n = int(round(UP_LONG * PACE_FPS))
        k = np.array([UP_LONG * i / n for i in range(n + 1)])
        P = [up_pose(body, way, float(x))[0] for x in k]
        skip = set(body.whip) | {man.b(x) for x in GUN_BONES if x in man.ix}
        bones = [i for i in range(man.nb) if man.par[i] is not None and i not in skip]
        dk = UP_LONG / n
        s = np.array([max(angle_of(P[j][i].T @ P[j + 1][i]) for i in bones) / dk for j in range(n)])
        need = s / UP_TURN
        w = np.exp(-0.5 * (np.arange(-6, 7) / PACE_BLUR) ** 2)
        w /= w.sum()

        def g_of(c):
            g = np.maximum(need, c)
            g = np.convolve(np.pad(g, 6, mode="edge"), w, mode="valid")
            return np.maximum(g, need)
        lo, hi = 0.0, 1.0
        if g_of(0.0).sum() * dk > UP_LONG:
            # (more turning than UP_LONG holds at UP_TURN: the pace that keeps the fastest joint the least over it)
            g = need / (need.sum() * dk / UP_LONG)
        else:
            for _ in range(60):
                c = 0.5 * (lo + hi)
                if g_of(c).sum() * dk > UP_LONG:
                    hi = c
                else:
                    lo = c
            g = g_of(lo)
        t = np.concatenate(([0.0], np.cumsum(g * dk)))
        t = t * (UP_LONG / t[-1])
        t[-1] = UP_LONG
        body.poses[key] = (t, k)
    return body.poses[key]


def up_key_time(body, way, t):
    """The get-up's key time at clip time t (up_pace)."""
    tt, k = up_pace(body, way)
    return float(np.interp(t, tt, k))


def up_clip_time(body, way, x):
    """The clip time at which the get-up's key time x is played (up_pace)."""
    tt, k = up_pace(body, way)
    return float(np.interp(x, k, tt))


# ================================================================================================ what he carries
def drape_whip(body, lq, mv, r=0.012):
    """The radio man's whip aerial (whip_1..whip_4 off his chest) kept out of the ground: a piece that would end
    under it bends to lie along it, and the pieces after it go on from there."""
    if not body.whip:
        return lq
    man = body.man
    H = man.H
    for k, i in enumerate(body.whip):
        G, X = man.fk(lq, mv)
        if k + 1 < len(body.whip):
            e = H[body.whip[k + 1]]
        else:
            q = body.P[body.pa == i]
            back = H[i] - H[body.whip[k - 1]] if k else Z_
            e = q[int(np.argmax((q - H[i]) @ back))] if len(q) else H[i] + back
        a = e - H[i]
        L = float(np.linalg.norm(a))
        d0 = G[i] @ (a / L)
        p = X[i]
        if p[2] + L * d0[2] >= r:
            continue
        vz = float(np.clip((r - p[2]) / L, -1.0, 1.0))
        hd = d0 - Z_ * d0[2]
        if np.linalg.norm(hd) < 1e-6:
            hd = G[man.par[body.whip[0]]] @ X_
            hd = hd - Z_ * hd[2]
        d = unit(hd) * math.sqrt(1.0 - vz * vz) + Z_ * vz
        ax = np.cross(d0, d)
        sn = float(np.linalg.norm(ax))
        if sn < 1e-9:
            continue
        R = R_axis(ax / sn, math.degrees(math.atan2(sn, float(d0 @ d))))
        lq[i] = G[man.par[i]].T @ R @ G[i]
    return lq


PROP_FPS = 60


def rekey_prop(body, clip, fps=PROP_FPS):
    """A prop on the bolt bone (the loader's rocket, the radio man's handset) hangs off the weapon bone in the file
    but rides his chest: keyed again, `fps` a second, from the pose the game draws between the clip's keys (the weapon
    and the chest each carried straight between their own keys), so it stays in its place on him. Its place on his
    chest at each of the clip's own keys is kept as keyed there (so a first or last key is the stance's own pose)
    and carried over between them."""
    if not body.bolt_prop:
        return clip
    man = body.man
    bo, c, w = man.b("bolt"), man.b("chest"), body.wi
    L = clip["long"]
    kt = sorted(set(round(k[0], 4) for tr in clip["tracks"] for k in tr.get("rot", [])))
    refs = [prop_ref(body, *gore.pose(man.rig, clip, t)) for t in kt]
    bare = dict(clip, tracks=[tr for tr in clip["tracks"] if tr["part"] != bo])
    ts = sorted(set([round(min(L, k / fps), 4) for k in range(int(math.ceil(L * fps - 1e-6)) + 1)] + kt))
    rot, move, prev = [], [], None
    for t in ts:
        j = max(0, min(len(kt) - 2, int(np.searchsorted(kt, t, side="right")) - 1))
        if len(kt) > 1:
            u = float(np.clip((t - kt[j]) / (kt[j + 1] - kt[j]), 0.0, 1.0))
            Rb = qslerp(refs[j][0], refs[j + 1][0], u)
            pb = refs[j][1] * (1 - u) + refs[j + 1][1] * u
        else:
            Rb, pb = refs[0]
        G, X = gore.pose(man.rig, bare, t)
        Gb, Xb = G[c] @ Rb, X[c] + G[c] @ pb
        q = quat(G[w].T @ Gb)
        if prev is not None and float(q @ prev) < 0:
            q = -q
        prev = q
        m = G[w].T @ (Xb - X[w]) - (man.H[bo] - man.H[w])
        rot.append([t] + [round(float(v), 5) for v in q])
        move.append([t] + [round(float(v), 4) for v in m])
    tracks = [tr for tr in clip["tracks"] if tr["part"] != bo]
    tracks.append({"part": bo, "part_name": "bolt", "rot": rot, "move": move})
    tracks.sort(key=lambda tr: tr["part"])
    return dict(clip, tracks=tracks)


# ================================================================================================ the clips
FINE_MM = 1.0                # (mm: the most the game's in-between may put what bears on the ground off the move's)


def fine_times(body, fn, clip, ts):
    """The half-way times between keys where the game's in-between (each bone's turn nlerped between the two keys)
    would put a hand, knee, foot or seat that bears on the ground more than FINE_MM off where the move has it, or
    anything of him under the ground, or his right hand off the weapon: keyed too."""
    man = body.man
    idx = np.nonzero(np.isin(body.pa, [man.b(n) for v in CONTACT.values() for n in v if n in man.ix]))[0]
    h = man.b("hand_r")
    out = []
    for a, b_ in zip(ts, ts[1:]):
        tm = round(0.5 * (a + b_), 4)
        if not a < tm < b_:
            continue
        G, X = man.fk(*fn(tm))
        V = body.skin(G, X)
        Gg, Xg = gore.pose(man.rig, clip, tm)
        Vg = body.skin(Gg, Xg)
        near = idx[V[idx, 2] < 0.02]
        bad = bool(len(near)) and float(np.linalg.norm((Vg[near] - V[near])[:, :2], axis=1).max()) > FINE_MM / 1000
        keep = ~body.is_whip
        bad = bad or float(Vg[keep, 2].min()) < min(float(V[keep, 2].min()), 0.0) - 0.003
        if body.armed and not bad:
            wi = body.wi
            r = G[wi].T @ (X[h] - X[wi])
            rg = Gg[wi].T @ (Xg[h] - Xg[wi])
            bad = float(np.linalg.norm(r - rg)) > 0.004
        if bad:
            out.append(tm)
    return out


TOE_DEG_S = 720.0            # (degrees a second: the most his toes bend or straighten at the ball of the foot)


def toe_bend(R):
    """How far a ball-of-the-foot turn bends the toes up (degrees): its part about the foot's own y, negated."""
    q = quat(R)
    if q[0] < 0:
        q = -q
    v = np.asarray(q[1:], float)
    s_ = float(np.linalg.norm(v))
    if s_ < 1e-9:
        return 0.0
    return -math.degrees(2.0 * math.atan2(s_, float(q[0]))) * float(v[1]) / s_


def toe_envelope(body, fn, L, fps=60):
    """Each foot's toes as bent up at least as far as the move bends them (to keep them out of the ground, or set
    under him) at every 60th of a second, and never bent or straightened faster than TOE_DEG_S: they bend sooner and
    straighten later, never later or less (the first and last pose are the stances' own, as they are)."""
    man = body.man
    grid = dense_times(L, fps)
    out = {}
    sides = [s for s in "lr" if "ball_" + s in man.ix]
    if not sides:
        return out
    poses = [fn(t)[0] for t in grid]
    for s in sides:
        bl = man.b("ball_" + s)
        b = np.array([toe_bend(lq[bl]) for lq in poses])
        a = b.copy()
        for k in range(1, len(grid) - 1):
            a[k] = max(a[k], a[k - 1] - TOE_DEG_S * (grid[k] - grid[k - 1]))
        for k in range(len(grid) - 2, 0, -1):
            a[k] = max(a[k], a[k + 1] - TOE_DEG_S * (grid[k + 1] - grid[k]))
        out[s] = (np.array(grid), a)
    return out


def toes_bent(body, lq, env, t):
    """A pose with each foot's toes bent up to the envelope (toe_envelope) at t."""
    for s, (g, a) in env.items():
        bl = body.man.b("ball_" + s)
        want = float(np.interp(t, g, a))
        cur = toe_bend(lq[bl])
        if want > cur + 1e-4:
            lq[bl] = lq[bl] @ Ry(-(want - cur))
    return lq


SOUND = {"elbow_down": "kit_rattle"}       # (fx with no take of their own: the sound cue they play, LandSound's)


def ev(name, t, bone=None):
    e = {"t": round(float(t), 3), "fx": name, "socket": None, "cue": SOUND.get(name, name)}
    if bone:
        e["bone"] = bone
    return e


def touch_time(body, poses, mask, after=0.12, near=0.03):
    """When the lowest of `mask`'s points first comes down onto the ground (within `near`), after `after` s."""
    for t, lq, mv in poses:
        if t >= after and zone_low(body, lq, mv, mask)[0] < near:
            return t
    return None


def weapon_word(body):
    return "weapon" if body.armed else "empty hands"


def make_clips(body, only=None, log=None):
    """Every clip of the set on this man, with what the game needs to play each."""
    man = body.man
    C = body.C
    out = {}
    want = lambda n: only is None or n in only
    armed = body.armed
    hands = "both fists stay on his weapon" if armed else "his hands keep their places (he carries no weapon)"

    def finish(name, fn, ts, like, clip, fine=False, toes=False):
        memo = {}
        env = toe_envelope(body, fn, clip["long"]) if toes else {}

        def fn2(t):
            k = round(float(t), 5)
            if k not in memo:
                lq, mv = fn(t)
                if body.whip and not name.startswith("hit_flinch") and 0.0 < t < clip["long"]:
                    lq = drape_whip(body, lq, mv)
                lq = toes_bent(body, lq, env, t)
                memo[k] = (lq.copy(), mv.copy())
            return memo[k][0].copy(), memo[k][1].copy()
        tracks, poses = bake(body, fn2, ts, like=like)
        if fine:
            # (a key more half way between two where the game's in-between would slide what bears on the ground)
            extra = fine_times(body, fn2, dict(clip, tracks=tracks), ts)
            if extra:
                ts = sorted(set(ts) | set(extra))
                tracks, poses = bake(body, fn2, ts, like=like)
        clip = dict(clip, tracks=tracks)
        clip = rekey_prop(body, clip)
        return clip, poses

    # ---- the flinches
    FROM = {"f": "the front", "b": "behind", "l": "his left", "r": "his right"}
    GOES = {"f": "back", "b": "forward", "l": "to his right", "r": "to his left"}
    for stance, S in STANCE.items():
        for way in WAYS:
            name = "hit_flinch_" + ("" if stance == "stand" else stance + "_") + way
            if not want(name):
                continue
            base = S["base"]
            ts = times_at(FLINCH_LONG, FLINCH_FPS, (FLINCH_PEAK,))
            run = stance == "run"
            end = [base, FLINCH_LONG if run else 0.0]
            c = {"long": FLINCH_LONG, "loop": False,
                 "stride": round(C[base].get("stride", 0.0) * FLINCH_LONG / C[base]["long"], 3) if run else 0.0,
                 "weapon": C[base].get("weapon", "rifle"), "tracks": None, "events": [],
                 "from": f"{TOOL}: keyed on this man's rig from {base}" +
                         (" (0.0 to 0.3 s on the run's own keys: his pelvis dropped and his knees bent with his feet "
                          "where the run has them)" if run else " at 0.0 s") +
                         ": the trunk's jolt shared up his spine, his head left behind and thrown after, " +
                         ("the weapon carried on his chest and his fists solved onto it" if armed else
                          "his arms carried on his chest"),
                 "start": [base, 0.0], "end": end}
            if run:
                c["events"] = [dict(e) for e in C[base]["events"] if e.get("fx") == "footstep" and e["t"] < FLINCH_LONG]
                # (the base is the run itself run on with the clip, 0.0 to 0.3 s, never one pose of it)
                c["additive_base"] = [base, 0.0, FLINCH_LONG]
                c["additive_base_clip"] = base
            else:
                c["additive_base"] = [base, 0.0]
            what = {"stand": "standing", "kneel": "kneeling", "run": "at a run"}[stance]
            c["about"] = (
                f"Hit but not killed, {what}: a round from {FROM[way]} (his own frame) jolts him {GOES[way]}; the "
                f"peak at {FLINCH_PEAK} s, back by 0.3 s with a small overshoot; {hands}. The game may either lay its "
                f"difference from its base over whatever he is doing (the better use: sharp, and right from any pose), "
                f"or blend into it (0.12 s) and play it once; but played whole it puts him in {base}'s " +
                ("stride" if run else "aim") + f", so blend into it only from {base} itself (and a 0.12 s blend-in "
                f"softens a jolt that peaks at {FLINCH_PEAK} s: 0.03 s keeps it sharp). " +
                (f"Its base is `additive_base` ({base} from 0.0 to {FLINCH_LONG} s: the run run on with the clip, "
                 f"never one pose of it), i.e. {base} sampled at this clip's own time (`additive_base_clip`), so laid "
                 f"over run, run_b, run_c or run_d at his own phase the run's legs cancel out and only the jolt and "
                 f"the stumble are added. The legs' part: his pelvis drops about "
                 f"{RUN_DIP * 100:.1f} cm and his knees give from 0.02 to 0.25 s, his feet where the run has them. "
                 f"Played whole: first key = {base} at 0.0 s, last = {base} at 0.3 s; `stride` is the ground the run "
                 "covers in 0.3 s, so the game carries him on at the run's speed." if run else
                 f"Its base is `additive_base` ({base} at 0.0 s, the same pose as its first and last key: `start` and "
                 f"`end`); his feet do not move in it" +
                 ("; the knee on the ground stays on it" if stance == "kneel" else "") + "."))
            fn = lambda t, stance=stance, way=way: flinch_pose(body, stance, way, t)
            out[name], _ = finish(name, fn, ts, (base,), c)
    if want("hit_flinch_prone"):
        ts = times_at(PRONE_LONG, FLINCH_FPS)
        c = {"long": PRONE_LONG, "loop": False, "stride": 0.0, "weapon": C["fire_prone"].get("weapon", "rifle"),
             "tracks": None, "events": [],
             "from": f"{TOOL}: keyed on this man's rig from fire_prone at 0.0 s: a jolt through the whole body (his "
                     f"chest thrown up about {PRONE_LIFT[1]:.0f} degrees, the pelvis twitched, the lower legs kicked "
                     "up" + (", the muzzle jerked up" if armed else "") + "), then his head ducked down and turned to "
                     "his left (stopped short of the ground on his own mesh" + (" and of his weapon" if armed else "") +
                     "), shoulders hunched, the left arm " + ("solved from its grip" if armed else "lifted") +
                     " onto the crown of his helmet and back",
             "start": ["fire_prone", 0.0], "end": ["fire_prone", 0.0], "additive_base": ["fire_prone", 0.0],
             "about": ("Hit or near-missed lying down: a jolt runs through him at 0.03 to 0.10 s (chest thrown up, "
                       "lower legs kicked up" + (", muzzle jerked" if armed else "") + "), then he ducks, head down "
                       "and turned to his left, shoulders hunched, his left hand off the " +
                       ("weapon" if armed else "ground") + " and onto his helmet (on it 0.29 to 0.31 s, coming from "
                       "0.12 s) and back by 0.5 s; " + ("his right hand keeps its grip on the weapon"
                                                        if armed else "his right hand stays on the ground (he "
                                                        "carries no weapon)") +
                       ". First and last key = fire_prone at 0.0 s. The game may lay its difference from "
                       "`additive_base` (fire_prone at 0.0 s) over the lying man's own pose (the better use: sharp, "
                       "and right however he lies), or blend into it and play it once; played whole it puts him in "
                       "fire_prone, so blend into it only from fire_prone, over 0.03 s rather than 0.12 s if the jolt "
                       "is to stay sharp.")}
        out["hit_flinch_prone"], _ = finish("hit_flinch_prone", lambda t: prone_flinch_pose(body, t), ts,
                                            ("fire_prone",), c)
    # ---- knocked flat, lying, getting up
    pe = man.b("pelvis")
    for way in ("face", "back"):
        kd, dn, up = "knock_down_" + way, "down_" + way, "get_up_" + way
        on = "his front" if way == "face" else "his back"
        if want(kd) or want(dn):
            ts = times_at(KNOCK_LONG, FLINCH_FPS)
            c = {"long": KNOCK_LONG, "loop": False, "stride": 0.0, "weapon": C["fire_stand"].get("weapon", "rifle"),
                 "tracks": None, "events": [],
                 "from": f"{TOOL}: keyed on this man's rig from fire_stand at 0.0 s, the pelvis on a thrown body's "
                         "path (about 3.6 m/s along, falling under gravity), his trunk turning over, head and limbs "
                         "left behind and thrown after; set down on the ground on his own mesh (his kit, his pack)",
                 "start": ["fire_stand", 0.0]}
            clip, poses = finish(kd, lambda t, way=way: knock_pose(body, way, t), ts, ("fire_stand",), c, fine=True,
                                 toes=True)
            mv_end = poses[-1][2][pe]
            clip["end_offset"] = [round(float(v), 3) for v in mv_end]
            evs = []
            t_body = touch_time(body, poses, body.Z["trunk"], after=0.2)
            t_el = touch_time(body, poses, body.Z["fore_l"] | body.Z["fore_r"], after=0.2)
            t_kn = touch_time(body, poses, body.Z["knee_l"] | body.Z["knee_r"], after=0.2)
            if t_el is not None:
                evs.append(ev("elbow_down", t_el, "lowerarm_l"))
            if t_body is not None:
                evs.append(ev("body_fall", t_body, "spine_02"))
            if t_kn is not None and way == "face":
                evs.append(ev("knee_down", t_kn, "calf_l"))
            clip["events"] = sorted(evs, key=lambda e: e["t"])
            clip["end"] = [dn, 0.0]
            clip["about"] = (
                f"A shell bursts close {'behind' if way == 'face' else 'in front of'} him: thrown off his feet "
                f"{'forward' if way == 'face' else 'backward'} onto {on}, alive, " +
                ("the weapon kept in his right hand. " if armed else "his hands empty (he carries no weapon). ") +
                "Blend into it (0.12 s) from whatever he was doing (its first key is fire_stand at 0.0 s) "
                f"and play it once. NOT in place: his pelvis and weapon move over the ground and the root stays; "
                f"`end_offset` is where his pelvis ends (metres, his own axes, as go_prone's). Then move the root by "
                f"end_offset's x and y and play {dn} (its first key is this clip's last pose set at the root), then "
                f"{up}. Events at the moment they touch: " + ", ".join(e["fx"] for e in clip["events"]) + ".")
            if want(kd):
                out[kd] = clip
        if want(dn):
            ts = times_at(DOWN_LONG, RATE)
            c = {"long": DOWN_LONG, "loop": True, "stride": 0.0, "weapon": C["fire_stand"].get("weapon", "rifle"),
                 "tracks": None, "events": [],
                 "from": f"{TOOL}: keyed on this man's rig from {kd}'s last pose (set down on the ground on his own "
                         "mesh): a slow breath, and once a loop a stir: his head lifted and turned, his left forearm "
                         "drawn in along the ground" + (", his right knee drawn up" if way == "back" else "") +
                         ", and back, the loop's seam exact",
                 "start": [kd, KNOCK_LONG], "placed": "at the root: his pelvis over the root, as fire_prone lies",
                 "about": (f"Lying on {on} stunned after {kd}: a loop, slow breath, and one stir a loop that reads "
                           f"at game size (from 0.70 s, held 0.95 to 1.15 s, back by 1.45 s): his head lifted about "
                           f"{STIR_HEAD[0]:.0f} degrees and turned about {STIR_HEAD[1]:.0f}, his left forearm drawn in "
                           f"about {STIR_ARM * 100:.0f} cm" + (f", his right knee drawn up about {STIR_KNEE:.0f} "
                                                               "degrees" if way == "back" else "") + "; " +
                           ("the weapon under his right hand on the ground" if armed else
                                       "his right hand on the ground") + ". In place at the root "
                           f"(his pelvis over it, as fire_prone): its first key is {kd}'s last pose with the root "
                           f"moved by that clip's end_offset (x, y). Then {up}.")}
            out[dn], _ = finish(dn, lambda t, way=way: down_pose(body, way, t), ts, ("fire_stand",), c)
        if want(up):
            T = UP_FACE if way == "face" else UP_BACK
            keys = up_keys(body, way)
            # (played at its own pace (up_pace): each key at the clip time its key time is played at)
            ct = lambda x, way=way: round(up_clip_time(body, way, x), 4)
            kts = [ct(x) for x in keys.t]
            ts = times_at(UP_LONG, UP_FPS, kts)
            # (a key's own time, not one of the even 30 a second beside it: no two keys a few thousandths apart)
            ts = [x for x in ts if x in kts or x in (0.0, UP_LONG) or min(abs(x - y) for y in kts) > 0.006]
            Xend = keys.at(UP_LONG)[1][pe]
            idle_mv = body.pose("idle", 0.0)[1][pe]
            shift = [round(float(Xend[0] - idle_mv[0]), 3), round(float(Xend[1] - idle_mv[1]), 3)]
            c = {"long": UP_LONG, "loop": False, "stride": 0.0, "weapon": C["idle"].get("weapon", "rifle"),
                 "tracks": None,
                 "events": [ev("kit_rattle", ct(T["fours" if way == "face" else "side"] + 0.02), "chest"),
                            ev("kit_rattle", ct(T["kneel"] + 0.05), "chest")],
                 "from": f"{TOOL}: keyed on this man's rig from {dn}'s first pose to idle at 0.0 s, through this "
                         "file's own kneel" + (" (mirrored: the left knee down)" if way == "back" else "") +
                         ", 30 keys a second; each hand, knee and foot that bears on the ground held on its spot "
                         "while it bears, moved only lifted; nothing under the ground on his own mesh",
                 "start": [dn, 0.0], "end": ["idle", 0.0],
                 "end_offset": [round(float(v), 3) for v in Xend], "root_shift": shift,
                 "about": (f"Getting up after {dn}, dazed: " +
                           ("he draws his hands in under his shoulders, pushes up onto hands and knees, kneels up, "
                            "brings his left foot through, lifted, onto the spot where he stands, rises over it and "
                            "steps his right foot up beside it" if way == "face" else
                            "he props himself up on his left hand, rolls onto his left hip, puts his left knee down "
                            "and his right foot flat, pushes up into a kneel on the left knee, rises over the right "
                            "foot and steps his left foot up beside it") +
                           (f"; the weapon in his right hand, brought back to port in both by "
                            f"{ct(LEFT_ON_T[way][1]):.2f} s"
                            if armed else "; his hands coming back to his sides as idle has them") +
                           f". From {dn}'s first pose (blend 0.12 s from wherever the loop is) to idle at 0.0 s "
                           "exactly. NOT in place: he stands up where his " +
                           ("knees were" if way == "face" else "feet were") +
                           f": `end_offset` is where his pelvis ends (his own axes, as go_prone's), and `root_shift` "
                           f"({shift[0]:.2f}, {shift[1]:.2f} m) is how far to move the root after it so idle at 0.0 s "
                           "carries on from the same place (end_offset less idle's own pelvis place). Events: "
                           "kit_rattle.")}
            # (its toes are up_pose's own: the locks that keep what bears on the ground from skidding are found on them)
            out[up], _ = finish(up, lambda t, way=way: up_pose(body, way, up_key_time(body, way, t)), ts,
                                ("idle", "fire_stand"), c, fine=True)
    return out


def about_hits(body):
    n = "soldier_" + body.name
    armed = body.armed
    return (
        f"Hit clips ({TOOL}, Tools/battle_units): what this man plays when he is hit or blown over but NOT killed, "
        "each keyed on his own body" + (" with his own hold on his weapon" if armed else
                                        " (he carries no weapon: his hands keep their places)") +
        ". FLINCHES (0.3 s, loop false; the letter is where the round came FROM in his own frame, f front, b behind, "
        "l his left, r his right, and he jolts the other way): hit_flinch_f/_b/_l/_r standing (first and last key "
        "fire_stand at 0.0 s), hit_flinch_kneel_f/_b/_l/_r (fire_kneel at 0.0 s), hit_flinch_run_f/_b/_l/_r (the "
        "run clip from 0.0 to 0.3 s) and hit_flinch_prone (0.5 s, fire_prone at 0.0 s: a jolt, then head down and "
        "the left hand onto his helmet and back). The game may lay a flinch's difference from its base over "
        "whatever he is doing (the better use: the onset stays sharp and it is right from any pose), or blend into "
        "it (0.12 s) and play it once; but played whole a flinch puts him in its base (the aim, the kneeling aim, the "
        "run's stride, prone), so blend into it only from that base, and over 0.03 s rather than 0.12 s if the jolt "
        "is to stay sharp. Bases: the standing, kneeling and "
        "prone ones carry `additive_base` [clip, 0.0] (one pose); the run ones carry `additive_base` [run, 0.0, "
        "0.3] and `additive_base_clip` run: the run clip run on with the flinch (sampled at the flinch's own time), "
        "never one pose of it, so laid over run, run_b, run_c or run_d at his own phase only the jolt and the "
        "stumble (a pelvis drop and knees giving, the same at any phase) are added. The jolt peaks at "
        f"{FLINCH_PEAK} s. KNOCKED FLAT by a near burst, alive: knock_down_face (thrown forward onto his front: the "
        "burst behind him) and knock_down_back (onto his back: the burst before him), 0.7 s, from fire_stand at 0.0 "
        "s, NOT in place (`end_offset`, where his pelvis ends, as go_prone's: a fixed throw of about 1.7 to 1.8 m; "
        "events elbow_down (sound cue kit_rattle), body_fall, knee_down); then move the root by end_offset (x, y) "
        "and loop down_face / down_back (2.0 s, stunned, one visible stir of head, left arm" +
        " and, on his back, a knee each loop; in place at the root as fire_prone lies), then get_up_face / "
        "get_up_back (1.6 s, from the down loop's first pose to idle at 0.0 s exactly). The get-ups are NOT in "
        "place either: he stands up where his knees (face) or feet (back) were, so `end_offset` is where his pelvis "
        "ends and `root_shift` (x, y) is how far to move the root after the clip so idle carries on from there; "
        "event kit_rattle. " + ("His weapon stays in his right hand through all six and both hands are back on it "
                                "at port by the end of the get-up. " if armed else
                                "He carries no weapon in any of them. ") +
        "Keys: 30 a second in the flinches, knock-downs and get-ups (with one at the jolt's peak, and extra keys "
        "half way between two where the game's in-between would put a planted hand or foot off its spot or him "
        "into the ground), 15 a second in the down loops; every key carries its own time. WHAT THE GAME STILL "
        "NEEDS to play them as written: an additive (or Once) layer for the flinches, which today has only Blend; a "
        "root shift by end_offset (and by a get-up's root_shift) after the clip, with the recording track "
        "reconciled; and a turnover from a knocked-down man to standing that plays get_up_face or get_up_back, not "
        f"go_prone backwards. Checks: the `checks` of {n}.{SET}.json beside the file's staging.")


# ================================================================================================ checks
# Every clip of the set measured from its baked keys, read back the way the game draws them (field_clips.Man /
# gore.pose): what the checks say is what is in the file, not what the pose functions meant.
LIMIT = {"end_deg": 1.0, "end_mm": 5.0, "hold_cm": 1.5, "slide_cm": 1.5, "lift_cm": 1.0, "under_cm": 1.0,
         "turn_deg_s": 900.0, "peak_s": [0.07, 0.10], "slide_up_cm": 2.0, "stand_fb_deg": [12.0, 18.0]}
# (a living man's own ranges, degrees: used where rom.py is not at hand, and reported beside it where it is)
OWN = {"spine_flex": 75.0, "spine_ext": 35.0, "spine_side": 35.0, "spine_twist": 45.0, "neck_flex": 65.0,
       "neck_ext": 70.0, "neck_side": 45.0, "neck_twist": 80.0, "elbow": 152.0, "knee": 155.0,
       "shoulder_behind": 65.0}
ROM_DIRS = (HERE, os.path.join(HERE, "checks"), "/home/user/art/hits/rom_src/")
BASES = ("idle", "run", "fire_stand", "fire_kneel", "fire_prone")
_ROM = []


def rom_mod():
    """rom.py (the range-of-motion check of the mocap animator), where it is at hand; else None."""
    if not _ROM:
        m = None
        for d in ROM_DIRS:
            if os.path.exists(os.path.join(d, "rom.py")):
                if d not in sys.path:
                    sys.path.append(d)
                try:
                    import rom as m
                except Exception:
                    m = None
                break
        _ROM.append(m)
    return _ROM[0]


def rom_report(body, clip, fps=60.0, t0=0.0, t1=None):
    """rom.py's report of a clip of this man (as falls.rom_check reads one), the faults only."""
    rom = rom_mod()
    if rom is None:
        return None
    man = body.man
    M = np.diag((1.0, -1.0, 1.0))                     # (the file's y right -> rom's y left)
    if not hasattr(body, "rom_body"):
        ref = rom.Body.of_rest()
        body.rom_body = rom.Body({n: M @ man.H[man.b(n)] for n in rom.CORE if n in man.ix}, ref.hands)
    t1 = clip["long"] if t1 is None else t1
    n = max(2, int(round((t1 - t0) * fps)) + 1)
    Ds, Hs = [], []
    for f in range(n):
        G, X = gore.pose(man.rig, clip, min(t1, t0 + f / fps))
        Ds.append([M @ G[man.b(k)] @ M for k in rom.CORE])
        Hs.append([M @ X[man.b(k)] for k in rom.CORE])
    rep = rom.check(rom.Poses(np.array(Ds), np.array(Hs), fps, rom.CORE), body=body.rom_body)
    rows = [{"key": r["key"], "side": r["side"], "value": r["value"], "limit": r["limit"], "over": r["over"],
             "t": round(t0 + r["t"], 3)} for r in rep["faults"]]
    fast = [{"joint": j, "deg_s": d["deg_s"], "t": round(t0 + d["t"], 3)} for j, d in rep["fast"]]
    return {"over": rows, "fast": fast}


def own_ranges(body, G, X):
    """The joints this tool keeps inside a living man's range, read off one pose (degrees)."""
    man = body.man
    b = man.b
    sp = lean_of(G[b("pelvis")].T @ G[b("chest")])
    nk = lean_of(G[b("chest")].T @ G[b("head")])
    r = {"spine_flex": sp[0], "spine_ext": -sp[0], "spine_side": abs(sp[1]), "spine_twist": abs(sp[2]),
         "neck_flex": nk[0], "neck_ext": -nk[0], "neck_side": abs(nk[1]), "neck_twist": abs(nk[2])}
    el, kn, sb = 0.0, 0.0, -90.0
    for s in "lr":
        u, lo, h = (X[b(n)] for n in ARM[s])
        el = max(el, math.degrees(math.acos(float(np.clip(unit(lo - u) @ unit(h - lo), -1, 1)))))
        th, ca, fo = (X[b(n)] for n in LEG[s])
        kn = max(kn, math.degrees(math.acos(float(np.clip(unit(ca - th) @ unit(fo - ca), -1, 1)))))
        d = G[b("chest")].T @ unit(lo - u)
        sb = max(sb, math.degrees(math.asin(float(np.clip(-d[0], -1, 1)))))
    r.update(elbow=el, knee=kn, shoulder_behind=sb)
    return r


def body_keys(body, clip):
    """The clip's own key times (a prop keyed again between them has more: those are not counted)."""
    bo = body.man.b("bolt") if body.bolt_prop else -1
    ts = set()
    for tr in clip["tracks"]:
        if tr["part"] != bo:
            ts.update(round(k[0], 4) for k in tr.get("rot", tr.get("move", ())))
    return sorted(ts)


def pose_gap(body, A, B, shift=(0.0, 0.0)):
    """Two poses (locals) against each other: the widest turn between a bone's two turns on its parent, and how far
    apart the pelvis and the weapon are (B moved over the ground by `shift` first)."""
    man = body.man
    worst, wb = 0.0, ""
    for i in range(man.nb):
        if man.par[i] is None:
            continue
        a = angle_of(A[0][i].T @ B[0][i])
        if a > worst:
            worst, wb = a, man.rig.names[i]
    XA, XB = man.fk(*A)[1], man.fk(*B)[1]
    sh = np.array((shift[0], shift[1], 0.0))
    pe = man.b("pelvis")
    return {"turn_deg": round(worst, 3), "bone": wb,
            "pelvis_mm": round(float(np.linalg.norm(XA[pe] - XB[pe] - sh)) * 1000, 2),
            "weapon_mm": round(float(np.linalg.norm(XA[body.wi] - XB[body.wi] - sh)) * 1000, 2)}


def hold_gap(body, G, X, rel, s):
    """Hand s against its place on the weapon (`rel`, grip_rel): the wrist's gap as squad_life measures a hold, the
    fist's as mg_gunners.Fit does (where the grip runs through his fist), cm; and the hand's turn off it, degrees."""
    man = body.man
    h = man.b("hand_" + s)
    A = man.side[s]
    Gw, Xw = G[body.wi], X[body.wi]
    f = A["d0"] * 0.075 + A["n0"] * 0.022
    wrist = float(np.linalg.norm(X[h] - (Xw + Gw @ rel[1])))
    fist = float(np.linalg.norm(man.grip(G, X, s) - (Xw + Gw @ (rel[1] + rel[0] @ f))))
    return max(wrist, fist) * 100, angle_of((Gw @ rel[0]).T @ G[h])


def kind_of(name):
    for k, p in (("run", "hit_flinch_run_"), ("kneel", "hit_flinch_kneel_"), ("prone", "hit_flinch_prone"),
                 ("stand", "hit_flinch_"), ("knock", "knock_down_"), ("down", "down_"), ("up", "get_up_")):
        if name.startswith(p):
            return k
    return ""


def hold_ref(body, name, t, L):
    """Where each hand should be on the weapon at t of a clip: {side: grip_rel}; a hand that is off it is not there."""
    if not body.armed:
        return {}
    man = body.man
    k = kind_of(name)

    def rel_of(clip, tt):
        key = ("rel", clip, round(tt, 4))
        if key not in body.poses:
            G, X = man.fk(*body.pose(clip, tt))
            body.poses[key] = {s: body.grip_rel(G, X, s) for s in "lr"}
        return body.poses[key]
    ends = t < 1e-6 or t > L - 1e-6
    if k == "run":
        return rel_of("run", t)
    if k in ("stand", "kneel"):
        return rel_of(STANCE[k]["base"], 0.0)
    if k == "prone":
        r = rel_of("fire_prone", 0.0)
        return r if ends else {"r": r["r"]}
    if k == "knock":
        return dict(body.rel0) if t < 1e-6 else {"r": body.rel0["r"]}
    if k == "down":
        return {"r": body.rel0["r"]}
    if k == "up":
        r = {"r": up_grip(body, t)}
        if t > L - 1e-6:
            r["l"] = body.rel_idle["l"]
        return r
    return {}


# ---- between the keys: the clip as the game draws it (each key carried straight to the next), 60 a second
DENSE_FPS = 60
CONTACT = {"foot_l": ("foot_l", "ball_l"), "foot_r": ("foot_r", "ball_r"), "knee_l": ("calf_l",),
           "knee_r": ("calf_r",), "hand_l": ("hand_l", "fingers_l", "thumb_l"),
           "hand_r": ("hand_r", "fingers_r", "thumb_r"), "seat": ("pelvis", "pouch_l", "pouch_r")}
# (each part's share of his weight, for his centre of mass: what the mesh points of each bone stand for)
MASS = {"pelvis": 14.0, "spine_01": 9.0, "spine_02": 9.0, "chest": 14.0, "neck": 1.5, "head": 7.0,
        "upperarm_l": 2.8, "upperarm_r": 2.8, "lowerarm_l": 1.7, "lowerarm_r": 1.7, "hand_l": 0.4, "hand_r": 0.4,
        "fingers_l": 0.2, "fingers_r": 0.2, "thigh_l": 10.0, "thigh_r": 10.0, "calf_l": 4.5, "calf_r": 4.5,
        "foot_l": 1.2, "foot_r": 1.2, "ball_l": 0.3, "ball_r": 0.3}
BALANCE_CM = 8.0             # (his centre of mass no further than this outside what he stands, kneels or leans on)


def dense_times(L, fps=DENSE_FPS):
    n = max(1, int(round(L * fps)))
    return [round(L * i / n, 5) for i in range(n + 1)]


def dense_frames(body, clip, ts):
    """(t, G, X, V) at each time: the bones as the game draws them between the keys, and his near mesh skinned."""
    out = []
    for t in ts:
        G, X = gore.pose(body.man.rig, clip, t)
        out.append((t, G, X, body.skin(G, X)))
    return out


def com_of(body, V):
    if not hasattr(body, "mass_sel"):
        man = body.man
        body.mass_sel = [(body.pa == man.b(n), w) for n, w in MASS.items() if n in man.ix]
        body.mass_sel = [(m, w) for m, w in body.mass_sel if m.any()]
    s, m = np.zeros(3), 0.0
    for sel, w in body.mass_sel:
        s += V[sel].mean(0) * w
        m += w
    return s / m


def balance_of(body, V, near=0.02, margin=0.03):
    """How far (m) his centre of mass lies outside what touches the ground (the box round every point of him within
    `near` of it, and `margin` more), and what touches it."""
    body_pts = ~body.is_gun & ~body.is_whip
    on = body_pts & (V[:, 2] < near)
    c = com_of(body, V)
    if not on.any():
        return 9.9, ()
    lo, hi = V[on, :2].min(0) - margin, V[on, :2].max(0) + margin
    d = np.maximum(np.maximum(lo - c[:2], c[:2] - hi), 0.0)
    touch = tuple(k for k, bones in CONTACT.items()
                  if (on & np.isin(body.pa, [body.man.b(n) for n in bones if n in body.man.ix])).any())
    return float(np.linalg.norm(d)), touch


def slides_of(body, frames, under=0.01):
    """Each part that bears on the ground: the most it slides over the ground while it stays on it (the same points
    of it under `under` in two frames running, their mean way over the ground summed over the run): {part: (cm, from,
    to)}."""
    out = {}
    for k, bones in CONTACT.items():
        idx = np.nonzero(np.isin(body.pa, [body.man.b(n) for n in bones if n in body.man.ix]))[0]
        run, worst = None, (0.0, 0.0, 0.0)
        for (ta, _, _, Va), (tb, _, _, Vb) in zip(frames, frames[1:]):
            a, b_ = Va[idx], Vb[idx]
            m = (a[:, 2] < under) & (b_[:, 2] < under)
            if m.sum() >= 3:
                d = float(np.linalg.norm((b_ - a)[m][:, :2], axis=1).mean())
                run = [ta, tb, 0.0] if run is None else run
                run[1], run[2] = tb, run[2] + d
                if run[2] > worst[0]:
                    worst = (run[2], run[0], run[1])
            else:
                run = None
        out[k] = (round(worst[0] * 100, 2), round(worst[1], 3), round(worst[2], 3))
    return out


def fingerprint(D):
    """What a bundle was keyed on: the skeleton and the stances its clips start from and end in."""
    import hashlib
    part = {"bones": D["bones"], "clips": {n: D["clips"][n] for n in BASES}}
    return hashlib.sha1(json.dumps(part, sort_keys=True, separators=(",", ":")).encode()).hexdigest()[:16]


def check_clip(body, name, clips, base_rom=None):
    """One clip's checks from its baked keys: (the measures, the faults in words)."""
    man = body.man
    b = man.b
    c = clips[name]
    L = c["long"]
    k = kind_of(name)
    way = name.rsplit("_", 1)[-1]
    ts = body_keys(body, c)
    fr = [man.locals_of(c, t) for t in ts]
    GX = [man.fk(lq, mv) for lq, mv in fr]
    out = {"long": L, "loop": c["loop"], "keys": len(ts)}
    faults = []
    bad = lambda s: faults.append(s)
    named = lambda ref: man.locals_of(clips[ref[0]] if ref[0] in clips else body.C[ref[0]], ref[1])
    if abs(ts[0]) > 1e-6 or abs(ts[-1] - L) > 1e-6:
        bad(f"keys run {ts[0]} to {ts[-1]} s, the clip is {L} s long")
    # ---- its first and last key against the pose it names
    ends = {}
    eo = c.get("end_offset", (0.0, 0.0, 0.0))
    if k == "down":
        kd = "knock_down_" + way
        if kd in clips:
            e = clips[kd]["end_offset"]
            ends["first"] = dict(vs=[kd, clips[kd]["long"]], moved=[-e[0], -e[1]],
                                 **pose_gap(body, fr[0], named([kd, clips[kd]["long"]]), (-e[0], -e[1])))
        ends["last"] = dict(vs=[name, 0.0], **pose_gap(body, fr[-1], fr[0]))
    else:
        ends["first"] = dict(vs=c["start"], **pose_gap(body, fr[0], named(c["start"])))
        if k == "knock":
            if c["end"][0] in clips:
                ends["last"] = dict(vs=c["end"], moved=[eo[0], eo[1]],
                                    **pose_gap(body, fr[-1], named(c["end"]), (eo[0], eo[1])))
        elif k == "up" and "root_shift" in c:
            rs = c["root_shift"]
            ends["last"] = dict(vs=c["end"], moved=list(rs), **pose_gap(body, fr[-1], named(c["end"]), rs))
        else:
            ends["last"] = dict(vs=c["end"], **pose_gap(body, fr[-1], named(c["end"])))
    for w, e in ends.items():
        if e["turn_deg"] > LIMIT["end_deg"] or e["pelvis_mm"] > LIMIT["end_mm"] or e["weapon_mm"] > LIMIT["end_mm"]:
            bad(f"{w} key is off {e['vs'][0]} at {e['vs'][1]} s: {e['turn_deg']} deg ({e['bone']}), pelvis "
                f"{e['pelvis_mm']} mm, weapon {e['weapon_mm']} mm")
    out["ends"] = ends
    if k == "knock":
        pm = fr[-1][1][b("pelvis")]
        out["end_offset"] = {"is": list(eo), "pelvis_ends": [round(float(v), 3) for v in pm]}
        if float(np.linalg.norm(np.array(eo) - pm)) > 0.002:
            bad(f"end_offset {list(eo)} is not where the pelvis ends {[round(float(v), 3) for v in pm]}")
    if k == "up":
        pm = fr[-1][1][b("pelvis")]
        out["pelvis_ends_from_root_cm"] = round(float(np.hypot(pm[0], pm[1])) * 100, 2)
        out["end_offset"] = {"is": list(eo), "pelvis_ends": [round(float(v), 3) for v in pm],
                             "root_shift": c.get("root_shift")}
        if float(np.linalg.norm(np.array(eo) - pm)) > 0.002:
            bad(f"end_offset {list(eo)} is not where the pelvis ends {[round(float(v), 3) for v in pm]}")
    if k == "run":
        r = body.C["run"]
        out["stride"] = {"is": c["stride"], "run_covers_in_0.3s": round(r["stride"] * L / r["long"], 3)}
        if abs(c["stride"] - r["stride"] * L / r["long"]) > 0.002:
            bad("stride does not match the run's speed")
    # ---- his fists on the weapon
    if body.armed:
        H = {}
        for s in "lr":
            g = [(hold_gap(body, G, X, hold_ref(body, name, t, L)[s], s), t) for t, (G, X) in zip(ts, GX)
                 if s in hold_ref(body, name, t, L)]
            if g:
                (gap, turn_), t_ = max(g, key=lambda x: x[0][0])
                H[s] = {"gap_cm": round(gap, 3), "t": t_, "turn_deg": round(max(x[0][1] for x in g), 2),
                        "keys": len(g)}
                if gap > LIMIT["hold_cm"]:
                    bad(f"hand_{s} is {gap:.1f} cm off its place on the weapon at {t_} s")
        if k == "prone":
            # (the left hand off the weapon and onto his helmet: how near the crown, when it is off)
            h = b("hand_l")
            rel = hold_ref(body, name, 0.0, L)["l"]
            off = [(t, float(np.linalg.norm(X[h] - (X[body.wi] + G[body.wi] @ rel[1])))) for t, (G, X) in zip(ts, GX)]
            cr = [(float(np.linalg.norm(man.grip(G, X, "l") - (X[b("head")] + G[b("head")] @ body.crown))), t)
                  for t, (G, X) in zip(ts, GX)]
            away = [t for t, d in off if d > 0.03]
            H["left_hand"] = {"off_from_s": away[0] if away else None, "off_to_s": away[-1] if away else None,
                              "fist_to_crown_cm": round(min(cr)[0] * 100, 1), "t": min(cr)[1]}
        out["hold"] = H
    else:
        out["hold"] = "unarmed: no weapon to hold"
    # ---- his feet (and a knee that is down) where they stand
    if k in ("stand", "kneel"):
        V0 = body.skin(*GX[0])
        F = {}
        for s in "lr":
            z = body.Z["foot_" + s]
            sl, lf = 0.0, 0.0
            for G, X in GX[1:]:
                d = body.skin(G, X)[z] - V0[z]
                sl = max(sl, float(np.hypot(d[:, 0], d[:, 1]).max()))
                lf = max(lf, float(np.abs(d[:, 2]).max()))
            F["foot_" + s] = {"slide_cm": round(sl * 100, 3), "lift_cm": round(lf * 100, 3)}
            if sl * 100 > LIMIT["slide_cm"] or lf * 100 > LIMIT["lift_cm"]:
                bad(f"foot_{s} slides {sl * 100:.1f} cm, lifts {lf * 100:.1f} cm")
            ca = b("calf_" + s)
            if GX[0][1][ca][2] < 0.2:
                d = max(float(np.linalg.norm(X[ca] - GX[0][1][ca])) for G, X in GX)
                F["knee_" + s] = {"moves_cm": round(d * 100, 3)}
                if d * 100 > LIMIT["slide_cm"]:
                    bad(f"the knee on the ground (calf_{s}) moves {d * 100:.1f} cm")
        out["feet"] = F
    # ---- the ground
    lows = []
    for t, (G, X) in zip(ts, GX):
        V = body.skin(G, X)
        m = ~body.is_whip
        i = int(np.argmin(np.where(m, V[:, 2], 9.0)))
        gz = float(V[body.is_gun, 2].min()) if body.is_gun.any() else 0.0
        wz = float(V[body.is_whip, 2].min()) if body.is_whip.any() else 0.0
        lows.append((float(V[i, 2]), t, man.rig.names[int(body.pa[i])], gz, wz))
    lo = min(lows)
    gr = {"low_cm": round(lo[0] * 100, 2), "t": lo[1], "part": lo[2],
          "weapon_low_cm": round(min(x[3] for x in lows) * 100, 2)}
    if body.is_whip.any():
        gr["aerial_low_cm"] = round(min(x[4] for x in lows) * 100, 2)
    if k in ("knock", "down", "up"):
        # (the stance a clip starts from or ends in is the file's own: its key is reported, not counted)
        own = [x for x in lows if not ((k == "knock" and x[1] < 1e-6) or (k == "up" and x[1] > L - 1e-6))]
        under = [x for x in own if x[0] * 100 < -LIMIT["under_cm"]]
        gr["keys_under_1cm"] = len(under)
        if own:
            w = min(own)
            gr["low_cm"], gr["t"], gr["part"] = round(w[0] * 100, 2), w[1], w[2]
        gr["stance_key_low_cm"] = round(lows[0 if k == "knock" else -1][0] * 100, 2) if k != "down" else None
        if under:
            w = min(under)
            bad(f"{len(under)} keys under the ground by more than 1 cm: worst {w[0] * 100:.1f} cm at {w[1]} s ({w[2]})")
        if body.is_whip.any() and gr["aerial_low_cm"] < -LIMIT["under_cm"]:
            bad(f"the aerial is {gr['aerial_low_cm']} cm under the ground")
    else:
        base = STANCE[k]["base"] if k in STANCE else "fire_prone"
        blow = []
        for t in (ts if k != "run" else [body.C[base]["long"] * i / 48.0 for i in range(48)]):
            V = body.skin(*man.fk(*body.pose(base, t if k == "run" else 0.0)))
            blow.append(float(V[~body.is_whip, 2].min()))
        if k == "run":
            blow = [min(blow)] * len(ts)
        gr["stance_low_cm"] = round(min(blow) * 100, 2)
        gr["under_stance_cm"] = round(max(0.0, max(bl - x[0] for bl, x in zip(blow, lows))) * 100, 2)
        if gr["under_stance_cm"] > LIMIT["under_cm"] and lo[0] * 100 < -LIMIT["under_cm"]:
            bad(f"goes {gr['under_stance_cm']} cm deeper into the ground than {base} itself does")
    out["ground"] = gr
    # ---- between the keys, as the game draws them (60 a second): the ground, the hold, what he bears on, his balance
    if k in ("knock", "down", "up", "prone"):
        dts = dense_times(L)
        frames = dense_frames(body, c, dts)
        own_t = [x for x in frames if not ((k == "knock" and x[0] < 1e-6) or (k == "up" and x[0] > L - 1e-6))]
        if k in ("knock", "down", "up"):
            zs = [(float(V[~body.is_whip, 2].min()), t, man.rig.names[int(body.pa[int(np.argmin(
                np.where(~body.is_whip, V[:, 2], 9.0)))])]) for t, G, X, V in own_t]
            und = [z for z in zs if z[0] * 100 < -LIMIT["under_cm"]]
            w = min(zs)
            gr["dense"] = {"fps": DENSE_FPS, "low_cm": round(w[0] * 100, 2), "t": w[1], "part": w[2],
                           "samples_under_1cm": len(und)}
            if und:
                bad(f"between the keys ({DENSE_FPS} a second) {len(und)} samples under the ground by more than 1 cm: "
                    f"worst {w[0] * 100:.1f} cm at {w[1]} s ({w[2]})")
        if body.armed and k in ("up", "knock", "down", "prone"):
            worst_h = {}
            for t, G, X, V in frames:
                for s_, rel in hold_ref(body, name, t, L).items():
                    g = hold_gap(body, G, X, rel, s_)[0]
                    if s_ not in worst_h or g > worst_h[s_][0]:
                        worst_h[s_] = (g, t)
            out["hold_dense"] = {s_: {"gap_cm": round(g, 3), "t": t} for s_, (g, t) in worst_h.items()}
            for s_, (g, t) in worst_h.items():
                if g > LIMIT["hold_cm"]:
                    bad(f"between the keys hand_{s_} is {g:.1f} cm off its place on the weapon at {t} s")
        if k in ("up", "knock"):
            sl = slides_of(body, frames)
            out["slides_cm"] = sl
            if k == "up":
                for part, (cm, t0_, t1_) in sl.items():
                    if cm > LIMIT["slide_up_cm"]:
                        bad(f"{part} slides {cm:.1f} cm over the ground while it bears on it ({t0_} to {t1_} s)")
        if k == "up":
            bal = [(balance_of(body, V), t) for t, G, X, V in frames]
            off = [(d, t, on) for (d, on), t in bal if d * 100 > BALANCE_CM]
            w = max(bal, key=lambda x: x[0][0])
            out["balance"] = {"worst_cm": round(w[0][0] * 100, 1), "t": w[1], "on": list(w[0][1]),
                              "samples_off": len(off), "samples": len(bal), "limit_cm": BALANCE_CM}
            if off:
                bad(f"his centre of mass is more than {BALANCE_CM:.0f} cm outside what he bears on in {len(off)} of "
                    f"{len(bal)} samples: worst {w[0][0] * 100:.0f} cm at {w[1]} s "
                    f"(on {', '.join(w[0][1]) or 'nothing'})")
    # ---- the jolt: when it peaks and how big
    if k in ("stand", "kneel", "run"):
        base = STANCE[k]["base"]
        ch, pe, hd = b("chest"), b("pelvis"), b("head")
        rows = []
        for t, (G, X) in zip(ts, GX):
            G0, X0 = man.fk(*body.pose(base, t if k == "run" else 0.0))
            D = G[ch] @ G0[ch].T
            rows.append((t, angle_of(D), lean_of(D), X[pe] - X0[pe], angle_of(G[hd] @ G0[hd].T)))
        pk = max(rows, key=lambda r: r[1])
        ax = np.array(pk[2])
        after = [float(np.array(r[2]) @ ax) / float(ax @ ax) for r in rows if r[0] > pk[0]]
        trav = max(rows, key=lambda r: float(np.hypot(r[3][0], r[3][1])))
        J = {"peak_s": pk[0], "chest_deg": round(pk[1], 2), "pitch_deg": round(pk[2][0], 2),
             "side_deg": round(pk[2][1], 2), "twist_deg": round(pk[2][2], 2),
             "pelvis_cm": [round(float(v) * 100, 2) for v in trav[3]], "pelvis_peak_s": trav[0],
             "drop_cm": round(-min(float(r[3][2]) for r in rows) * 100, 2),
             "head_peak_s": max(rows, key=lambda r: r[4])[0], "head_deg": round(max(r[4] for r in rows), 2),
             "overshoot_deg": round(-min(after + [0.0]) * pk[1], 2)}
        out["jolt"] = J
        if not LIMIT["peak_s"][0] - 1e-6 <= pk[0] <= LIMIT["peak_s"][1] + 1e-6:
            bad(f"the jolt peaks at {pk[0]} s, not between 0.07 and 0.10 s")
        want = {"f": pk[2][0] < -5, "b": pk[2][0] > 5, "l": pk[2][1] < -5, "r": pk[2][1] > 5}[way]
        if not want:
            bad(f"the jolt does not go away from a round from the {way} side")
    if k == "prone":
        hd, cl = b("head"), b("clavicle_l")
        G0, X0 = GX[0]
        rows = [(t, lean_of(G[hd] @ G0[hd].T)[0], X0[hd][2] - X[hd][2], angle_of(G0[cl].T @ G[cl]))
                for t, (G, X) in zip(ts, GX)]
        pk = max(rows, key=lambda r: r[1])
        out["jolt"] = {"peak_s": pk[0], "head_down_deg": round(pk[1], 2),
                       "head_drop_cm": round(max(r[2] for r in rows) * 100, 2),
                       "shoulder_deg": round(max(r[3] for r in rows), 2)}
    # ---- the fastest a joint turns between two keys, and the weapon in the world
    sp, gsp = (0.0, "", 0.0), (0.0, 0.0)
    skip = set(body.whip) | {b(n) for n in GUN_BONES if n in man.ix}
    for j in range(1, len(ts)):
        dt = ts[j] - ts[j - 1]
        if dt < 1e-6:
            continue
        for i in range(man.nb):
            if man.par[i] is None or i in skip:
                continue
            w = angle_of(fr[j - 1][0][i].T @ fr[j][0][i]) / dt
            if w > sp[0]:
                sp = (w, man.rig.names[i], ts[j - 1])
        w = angle_of(GX[j - 1][0][body.wi].T @ GX[j][0][body.wi]) / dt
        if w > gsp[0]:
            gsp = (w, ts[j - 1])
    out["fastest_turn"] = {"deg_s": round(sp[0]), "joint": sp[1], "t": sp[2], "weapon_deg_s": round(gsp[0]),
                           "weapon_t": gsp[1]}
    base = {"run": "run"}.get(k)
    if sp[0] > LIMIT["turn_deg_s"]:
        bad(f"{sp[1]} turns {sp[0]:.0f} degrees a second at {sp[2]} s")
    # ---- joint ranges: this tool's own limits, and rom.py's where it is at hand
    worst = {}
    for t, (G, X) in zip(ts, GX):
        for n, v in own_ranges(body, G, X).items():
            if n not in worst or v > worst[n][0]:
                worst[n] = (v, t)
    R = {"own": {n: {"deg": round(v, 1), "limit": OWN[n], "t": t} for n, (v, t) in worst.items() if v > OWN[n] - 15.0}}
    for n, (v, t) in worst.items():
        if v > OWN[n] + 1.0:
            bad(f"{n} {v:.0f} degrees at {t} s (limit {OWN[n]:.0f})")
    rep = rom_report(body, c)
    if rep is not None:
        as_base = base_rom or {}
        st = set()
        for ref in (c.get("start"), c.get("end"), c.get("additive_base"), ["fire_kneel", 0.0] if k == "up" else None):
            if ref and ref[0] in as_base:
                st.update((r["key"], s_) for r in as_base[ref[0]]["over"]
                          for s_ in ((r["side"],) if k != "up" else ("l", "r", "")))
        for r in rep["over"]:
            r["as_stance"] = (r["key"], r["side"]) in st
            if not r["as_stance"]:
                bad(f"rom: {r['key']}{'_' + r['side'] if r['side'] else ''} {r['value']} against {r['limit']} "
                    f"(over by {r['over']}) at {r['t']} s")
        for f in rep["fast"]:
            # (the one this tool's own measure already reported is not listed twice)
            if sp[0] > LIMIT["turn_deg_s"] and f["joint"] == sp[1] and abs(f["t"] - sp[2]) < 0.03:
                continue
            bad(f"rom: {f['joint']} turns {f['deg_s']} degrees a second at {f['t']} s")
        R["rom"] = rep
    out["joints"] = R
    del base
    return out, faults


# Faults accepted one by one, each with its reason: (file, or "*" for every file; the clip; how the fault's text
# begins after the clip's name; the most of its measure accepted (the first number after that beginning: degrees,
# degrees a second, cm); why it is accepted). `deliver` sends a bundle only when every fault in its checks is matched
# here; any other fault, or one past its most, keeps that file back. Nothing is accepted wholesale.
ALLOW = ()


def allowed(name, fault):
    """The reason a fault of soldier_<name> is accepted (ALLOW), or None. An entry's beginning is a pattern (re) the
    fault's text must start with; its measure is the pattern's group `v`, or else the first number after it."""
    clip, _, what = fault.partition(": ")
    for f, c, start, most, why in ALLOW:
        if f not in ("*", name) or c != clip:
            continue
        m0 = re.match(start, what)
        if m0 is None:
            continue
        v = m0.groupdict().get("v")
        if v is None:
            m = re.search(r"[-+]?\d+(?:\.\d+)?", what[m0.end():])
            v = m.group(0) if m else None
        if v is not None and abs(float(v)) <= most + 1e-9:
            return why
    return None


def sort_faults(name, faults):
    """(blocking, accepted) of a file's faults: those ALLOW does not cover, and those it does with its reason."""
    blocking, ok = [], []
    for f in faults:
        why = allowed(name, f)
        if why is None:
            blocking.append(f)
        else:
            ok.append({"fault": f, "why": why})
    return blocking, ok


def check_all(body, clips, log=None):
    """Every clip of the set in `clips` checked: the `checks` of a bundle."""
    man = body.man
    rom = rom_mod()
    base_rom = {}
    if rom is not None:
        for n in BASES:
            L = body.C[n]["long"]
            base_rom[n] = rom_report(body, body.C[n], t1=min(L, 1.0) if n != "run" else L)
    base_low = {}
    for n in BASES + ("go_prone",):
        if n in body.C:
            c = body.C[n]
            V = body.skin(*man.fk(*body.pose(n, c["long"] if n == "go_prone" else 0.0)))
            base_low[n] = round(float(V[~body.is_whip, 2].min()) * 100, 2)
    res = {"tool": TOOL, "file": "soldier_" + body.name, "built_on": fingerprint(body.D),
           "bones": man.nb, "weapon": body.weapon, "armed": bool(body.armed),
           "limits": dict(LIMIT, own_ranges=OWN),
           "joint_ranges_by": "rom.py and this tool's own limits" if rom is not None else "this tool's own limits",
           "how": ("each clip read back from its baked keys as the game draws them; `ends` its first and last key "
                   "against the pose named in `vs` (widest turn of any bone on its parent, pelvis and weapon apart); "
                   "`hold` each fist against its place on the weapon at every key it should be on it (wrist as "
                   "squad_life, fist as mg_gunners.Fit); `feet` every point of a planted foot against the first key; "
                   "`ground` the lowest point of his near mesh and weapon at any key; `jolt` the chest's turn from "
                   "the stance at its peak; `fastest_turn` between two keys; `joints` the ranges"),
           "stance_low_cm": base_low,
           "stance_rom": {n: r["over"] for n, r in base_rom.items() if r and r["over"]},
           "clips": {}, "faults": []}
    for name in clips:
        t0 = time.time()
        out, faults = check_clip(body, name, clips, base_rom)
        if faults:
            out["faults"] = faults
            res["faults"] += [f"{name}: {f}" for f in faults]
        res["clips"][name] = out
        if log:
            log(check_line(name, out) + f"   [{time.time() - t0:.1f} s]")
    res["blocking"], res["accepted"] = sort_faults(body.name, res["faults"])
    res["ok"] = not res["blocking"]
    return res


def check_line(name, o):
    e = o.get("ends", {})
    s = f"  {name:22s} ends " + "/".join(f"{e[w]['turn_deg']:.2f}d {e[w]['pelvis_mm']:.1f}mm" for w in e)
    h = o.get("hold")
    if isinstance(h, dict):
        s += "  hold " + " ".join(f"{k_} {h[k_]['gap_cm']:.2f}cm" for k_ in "rl" if k_ in h)
    if "feet" in o:
        f = o["feet"]
        s += "  feet " + " ".join(f"{f[k_]['slide_cm']:.2f}/{f[k_]['lift_cm']:.2f}" for k_ in ("foot_l", "foot_r"))
    g = o["ground"]
    s += f"  low {g['low_cm']:.1f}cm"
    if "keys_under_1cm" in g:
        s += f" ({g['keys_under_1cm']} under)"
    if "jolt" in o:
        j = o["jolt"]
        s += f"  peak {j['peak_s']}s " + (f"{j['chest_deg']:.1f}d (p {j['pitch_deg']:.1f} s {j['side_deg']:.1f} "
                                           f"t {j['twist_deg']:.1f})" if "chest_deg" in j else
                                           f"head {j['head_down_deg']:.1f}d")
    ft = o["fastest_turn"]
    s += f"  fastest {ft['deg_s']} d/s {ft['joint']}"
    if o.get("faults"):
        s += "\n      FAULT " + "\n      FAULT ".join(o["faults"])
    return s


# ================================================================================================ files
def path_of(d, name):
    return os.path.join(d, f"soldier_{name}.json")


def bundle_path(stage, name):
    return os.path.join(stage, f"soldier_{name}.{SET}.json")


def text_of(D):
    """A soldier file's (or a bundle's) bytes: as the files are written, so an untouched one reads back the same."""
    return json.dumps(D, separators=(",", ":")).encode("utf-8")


def write_whole(path, data):
    tmp = path + ".tmp_" + SET
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def build(names, src, stage, log=print):
    os.makedirs(stage, exist_ok=True)
    ok = True
    for name in names:
        t0 = time.time()
        with open(path_of(src, name), "rb") as f:
            D = json.loads(f.read())
        body = Body(name, D)
        clips = json.loads(json.dumps(make_clips(body)))          # (as the file will hold them)
        log(f"soldier_{name}: {len(clips)} clips keyed in {time.time() - t0:.0f} s; {body.nb} bones, "
            f"{len(body.P)} points, weapon {body.weapon}")
        checks = check_all(body, clips, log)
        bundle = {"clips": clips, "about_hits": about_hits(body), "checks": checks}
        write_whole(bundle_path(stage, name), text_of(bundle))
        log(f"soldier_{name}: {len(checks['faults'])} faults, {len(checks['accepted'])} of them accepted (ALLOW), "
            f"{len(checks['blocking'])} blocking; {bundle_path(stage, name)} "
            f"({os.path.getsize(bundle_path(stage, name))} bytes) in {time.time() - t0:.0f} s")
        for f in checks["blocking"]:
            log("    BLOCKING " + f)
        ok = ok and not checks["blocking"]
    return ok


def near(a, b, tol=0.011):
    """Two checks' values the same, but for the last place of a rounded number."""
    if isinstance(a, dict) and isinstance(b, dict):
        return set(a) == set(b) and all(near(a[k], b[k], tol) for k in a)
    if isinstance(a, (list, tuple)) and isinstance(b, (list, tuple)):
        return len(a) == len(b) and all(near(x, y, tol) for x, y in zip(a, b))
    if isinstance(a, (int, float)) and isinstance(b, (int, float)) and not isinstance(a, bool):
        return abs(a - b) <= tol
    return a == b


def check(names, src, stage, log=print):
    """The checks again on what is staged; and that the staged checks say the same."""
    ok = True
    for name in names:
        with open(path_of(src, name), "rb") as f:
            D = json.loads(f.read())
        with open(bundle_path(stage, name), "rb") as f:
            B = json.loads(f.read())
        body = Body(name, D)
        log(f"soldier_{name}: {len(B['clips'])} clips staged")
        res = check_all(body, B["clips"], log)
        same = near(json.loads(json.dumps(res)), B["checks"])
        log(f"soldier_{name}: {len(res['faults'])} faults, {len(res['blocking'])} blocking; the staged checks " +
            ("say the same" if same else "DO NOT say the same"))
        ok = ok and same and not res["blocking"]
    return ok


def merged(D, B):
    """A soldier file with a bundle merged in: its clips added (a clip of this set already there is replaced where
    it stands; any other clip of the same name is never replaced), `about_hits` added. Nothing else is touched."""
    for n, c in B["clips"].items():
        have = D["clips"].get(n)
        if have is not None and not str(have.get("from", "")).startswith(TOOL):
            raise ValueError(f"the file already has a clip {n} that is not this tool's")
        D["clips"][n] = c
    D["about_hits"] = B["about_hits"]
    return D


def merge_bytes(raw, B, name):
    """The merged file's bytes from the file's bytes; refuses a file whose untouched text would not come back the
    same, or whose skeleton or stances are not the ones the bundle was keyed on."""
    D = json.loads(raw)
    if text_of(D) != raw:
        raise ValueError("the file does not read back byte for byte: not written")
    if fingerprint(D) != B["checks"]["built_on"]:
        raise ValueError("its skeleton or stance clips are not the ones the bundle was keyed on: build again")
    before = {k: v for k, v in D.items() if k not in ("clips", "about_hits")}
    names = list(D["clips"])
    D = merged(D, B)
    if not all(D[k] is before[k] for k in before):
        raise ValueError("something else in the file was touched: not written")
    if [n for n in D["clips"] if n in names] != names:
        raise ValueError("the file's own clips did not keep their order: not written")
    return text_of(D)


def apply(names, src, out, stage, log=print):
    """The staged bundles merged into copies of the files in `src`, written to `out` (never `src`): each file on its
    own; one that fails is logged and the rest still go. True when every one was written and read back."""
    if os.path.realpath(out) == os.path.realpath(src):
        log("--out must not be --src: nothing written")
        return False
    os.makedirs(out, exist_ok=True)
    ok = True
    for name in names:
        try:
            with open(bundle_path(stage, name), "rb") as f:
                B = json.loads(f.read())
            with open(path_of(src, name), "rb") as f:
                raw = f.read()
            data = merge_bytes(raw, B, name)
            write_whole(path_of(out, name), data)
            with open(path_of(out, name), "rb") as f:
                back = json.loads(f.read())
            if not (all(back["clips"].get(n) == c for n, c in B["clips"].items())
                    and back.get("about_hits") == B["about_hits"]):
                raise ValueError("the written file did not read back with the bundle's clips")
        except (OSError, ValueError) as e:
            log(f"soldier_{name}: NOT applied: {e}")
            ok = False
            continue
        log(f"soldier_{name}: {len(raw)} -> {len(data)} bytes, {len(back['clips'])} clips; {path_of(out, name)}")
    return ok


def deliver(names, stage, content=False, log=print):
    """The staged bundles merged into the shared soldier files (and the game's Content copies where they exist).
    A bundle goes only when every fault its checks list is accepted, one by one, in ALLOW (read now, so the list
    decides, not the bundle). Each target: read again just before its write, backed up first (never over an older
    backup of other bytes), written whole through a temporary file and os.replace, read back. Run twice it writes
    the same bytes."""
    import hashlib
    done, held = [], []
    for name in names:
        try:
            with open(bundle_path(stage, name), "rb") as f:
                B = json.loads(f.read())
        except (OSError, ValueError) as e:
            log(f"soldier_{name}: NOT delivered: its bundle could not be read ({e})")
            held.append(name)
            continue
        blocking, ok_ = sort_faults(name, B["checks"]["faults"])
        if blocking:
            held.append(name)
            log(f"soldier_{name}: NOT delivered, {len(blocking)} of its {len(B['checks']['faults'])} faults are not "
                "accepted in ALLOW: " + "; ".join(blocking[:4]) + (" ..." if len(blocking) > 4 else ""))
            continue
        for a in ok_:
            log(f"soldier_{name}: accepted fault {a['fault']} ({a['why']})")
        targets = [("shared", path_of(SHARED, name))]
        if content and os.path.exists(path_of(CONTENT, name)):
            targets.append(("content", path_of(CONTENT, name)))
        for where, path in targets:
            try:
                with open(path, "rb") as f:                   # (read again just before the write)
                    raw = f.read()
                data = merge_bytes(raw, B, name)
            except (OSError, ValueError) as e:
                log(f"soldier_{name} ({where}): NOT delivered: {e}")
                held.append(name)
                continue
            if data == raw:
                log(f"soldier_{name} ({where}): already has them, the same bytes")
                continue
            bdir = os.path.join(BACKUP, where)
            os.makedirs(bdir, exist_ok=True)
            bpath = os.path.join(bdir, f"soldier_{name}.{hashlib.sha1(raw).hexdigest()[:12]}.json")
            if not os.path.exists(bpath):
                write_whole(bpath, raw)
            with open(bpath, "rb") as f:
                if f.read() != raw:
                    log(f"soldier_{name} ({where}): NOT delivered: its backup {bpath} did not read back")
                    held.append(name)
                    continue
            with open(path, "rb") as f:
                if f.read() != raw:
                    log(f"soldier_{name} ({where}): NOT delivered: the file changed while it was being merged")
                    held.append(name)
                    continue
            write_whole(path, data)
            with open(path, "rb") as f:
                if f.read() != data:
                    log(f"soldier_{name} ({where}): WRITTEN BUT DID NOT READ BACK: put back its backup {bpath}")
                    held.append(name)
                    continue
            log(f"soldier_{name} ({where}): {len(raw)} -> {len(data)} bytes; backup {bpath}")
            done.append(path)
    if done:
        try:
            readme_deliver(log)
        except (OSError, ValueError) as e:
            log(f"README.md: NOT written: {e}")
            held.append("README")
    log(f"delivered {len(done)} file(s); held back: " + (", ".join(sorted(set(held))) if held else "none"))
    return done, held


README_HEAD = "## Hit reactions, knock-downs and get-ups"


def readme_section():
    """The soldier README's section on this set (written by --deliver, in place of an older one)."""
    rows = [("hit_flinch_f / _b / _l / _r", FLINCH_LONG, "no", "fire_stand 0.0", "fire_stand 0.0",
             "additive_base [fire_stand, 0.0]"),
            ("hit_flinch_kneel_f / _b / _l / _r", FLINCH_LONG, "no", "fire_kneel 0.0", "fire_kneel 0.0",
             "additive_base [fire_kneel, 0.0]"),
            ("hit_flinch_run_f / _b / _l / _r", FLINCH_LONG, "no", "run 0.0", "run 0.3",
             "additive_base [run, 0.0, 0.3], stride (the run's)"),
            ("hit_flinch_prone", PRONE_LONG, "no", "fire_prone 0.0", "fire_prone 0.0", "additive_base [fire_prone, 0.0]"),
            ("knock_down_face / knock_down_back", KNOCK_LONG, "no", "fire_stand 0.0", "down_face / down_back 0.0",
             "end_offset (the root moved after it)"),
            ("down_face / down_back", DOWN_LONG, "yes", "the knock-down's last pose", "= first", ""),
            ("get_up_face / get_up_back", UP_LONG, "no", "down_face / down_back 0.0", "idle 0.0 (its pose, not its place)",
             "end_offset, root_shift (the root moved after it)")]
    out = [README_HEAD + f" (release 2, {SET}.py, {time.strftime('%d %B %Y')})", "",
           f"Nineteen clips in every infantry rifleman file (the twenty files of {SET}.py), each keyed on the man's "
           "own rig and checked from its baked keys the way the game draws them. Each clip's `about` says how to "
           "play it; the file's `about_hits` says it for the set.", "",
           "| clip | long s | loop | first key | last key | fields |", "|---|---|---|---|---|---|"]
    out += [f"| {a} | {b_} | {c} | {d} | {e} | {f} |" for a, b_, c, d, e, f in rows]
    out += ["",
            "- Flinches: lay the difference from additive_base on whatever he is doing (the better use), or blend "
            "into the clip only from its base pose, over 0.03 s, and back. The run flinches are the run from 0.0 to "
            "0.3 s with the stumble in it: start them from run phase 0, or lay them as a difference.",
            "- Knock-downs: play once, then move the root by end_offset and go on with the down loop.",
            "- Get-ups: from the down loop's first pose to idle's pose at 0.0. Not in place: he stands up where his "
            "knees (face) or feet (back) were, so his feet do not slide; move the root by root_shift (x, y) after the "
            "clip and idle carries on from there.",
            f"- Checks: the `checks` of each file's staged bundle (`{STAGE}soldier_<N>.{SET}.json`). A file is "
            f"delivered only when every fault in its checks is accepted, one by one with its reason, in {SET}.py's "
            "ALLOW list.",
            "- The game still needs: an additive (or once) layer for the flinches, a root shift by end_offset after "
            "a knock-down or a get-up, and a turnover that plays the get-ups.", ""]
    return "\n".join(out)


def readme_deliver(log=print):
    """The section put into the shared soldier README (an older one of the same heading replaced, else set in before
    'Known limits'), the README backed up first and read again just before the write."""
    import hashlib
    path = os.path.join(SHARED, "README.md")
    with open(path, "rb") as f:
        raw = f.read()
    text = raw.decode("utf-8")
    sec = readme_section()
    lines = text.split("\n")
    a = next((i for i, l_ in enumerate(lines) if l_.startswith(README_HEAD)), None)
    if a is not None:
        z = next((i for i in range(a + 1, len(lines)) if lines[i].startswith("## ")), len(lines))
    else:
        a = z = next((i for i, l_ in enumerate(lines) if l_.startswith("## Known limits")), len(lines))
    new = "\n".join(lines[:a] + sec.split("\n") + lines[z:]).encode("utf-8")
    if new == raw:
        log("README.md: already has the section")
        return
    bdir = os.path.join(BACKUP, "shared")
    os.makedirs(bdir, exist_ok=True)
    bpath = os.path.join(bdir, f"README.{hashlib.sha1(raw).hexdigest()[:12]}.md")
    if not os.path.exists(bpath):
        write_whole(bpath, raw)
    with open(path, "rb") as f:
        if f.read() != raw:
            raise ValueError("it changed while the section was being made")
    write_whole(path, new)
    with open(path, "rb") as f:
        if f.read() != new:
            raise ValueError(f"written but did not read back: put back {bpath}")
    log(f"README.md: section '{README_HEAD}' written; backup {bpath}")


# ================================================================================================ pictures
SHOTS = {"flinch": (0.0, 0.07, 0.13, 0.2, 0.3), "prone": (0.0, 0.1, 0.2, 0.3, 0.4, 0.5),
         "knock": (0.0, 0.14, 0.28, 0.42, 0.56, 0.7), "down": (0.0, 0.5, 1.0, 1.5, 2.0),
         "up": (0.0, 0.23, 0.46, 0.69, 0.91, 1.14, 1.37, 1.6)}
CAME = {"f": "round from the FRONT", "b": "round from BEHIND", "l": "round from his LEFT", "r": "round from his RIGHT"}


def pictures(names, src, stage, pics, log=print, only=None):
    """Labelled sheets of a file's staged clips: every flinch at 0, 0.07, 0.13, 0.2 and 0.3 s from the side and from
    the game's high rear camera (45 degrees down), the knock-downs, the down loops and the get-ups."""
    os.makedirs(pics, exist_ok=True)
    made = []
    for name in names:
        with open(path_of(src, name), "rb") as f:
            D = json.loads(f.read())
        with open(bundle_path(stage, name), "rb") as f:
            C = json.loads(f.read())["clips"]
        body = Body(name, D)
        look = Look(body)
        tag = "soldier_" + name
        sheets = []
        for st in ("stand", "kneel", "run"):
            mid = "" if st == "stand" else st + "_"
            for pair in ("fb", "lr"):
                rows = [strip(body, look, C[f"hit_flinch_{mid}{w}"], SHOTS["flinch"], ("side", "rear"),
                              label=f"{tag}   hit_flinch_{mid}{w}   ({CAME[w]}; rows: from his right side, and the "
                                    "game's high rear camera 45 deg down)")
                        for w in pair]
                sheets.append((f"{tag}_flinch_{st}_{pair}.png", rows))
        sheets.append((f"{tag}_flinch_prone.png",
                       [strip(body, look, C["hit_flinch_prone"], SHOTS["prone"], ("side", "rear", "q"), dist=4.6,
                              label=f"{tag}   hit_flinch_prone   (rows: side, high rear camera, three-quarter)")]))
        for way in ("face", "back"):
            kd, dn, up = C["knock_down_" + way], C["down_" + way], C["get_up_" + way]
            sheets.append((f"{tag}_knock_down_{way}.png",
                           [strip(body, look, kd, SHOTS["knock"], ("side",), size=(330, 220), dist=8.6, follow=False,
                                  label=f"{tag}   knock_down_{way}   0.7 s, six frames; the camera held (he is "
                                        f"thrown {abs(kd['end_offset'][0]):.2f} m)"),
                            strip(body, look, kd, SHOTS["knock"], ("side", "rear"), size=(330, 250), dist=5.4,
                                  label="the camera following him: from his right side, and the high rear camera")]))
            sheets.append((f"{tag}_down_{way}.png",
                           [strip(body, look, dn, SHOTS["down"], ("side", "rear", "top"), size=(300, 200), dist=4.8,
                                  label=f"{tag}   down_{way}   2.0 s loop "
                                         "(rows: side, high rear camera, from above)")]))
            sheets.append((f"{tag}_get_up_{way}.png",
                           [strip(body, look, up, SHOTS["up"], ("side", "rear"), size=(240, 240), dist=5.6,
                                  label=f"{tag}   get_up_{way}   1.6 s, eight frames (rows: from his right side, and "
                                        "the high rear camera)"),
                            strip(body, look, up, SHOTS["up"], ("front",), size=(240, 240), dist=5.6,
                                  label="from in front")]))
        for fn, rows in sheets:
            if only and not any(o in fn for o in only):
                continue
            made.append(sheet(rows, os.path.join(pics, fn)))
            log(made[-1])
    return made


# ================================================================================================ command line
def main(argv):
    def opt(flag, default=None):
        return argv[argv.index(flag) + 1] if flag in argv else default
    names = [n for n in (opt("--files") or ",".join(NAMES)).split(",") if n]
    unknown = [n for n in names if n not in NAMES]
    if unknown:
        print("not one of the twenty:", ", ".join(unknown))
        return 2
    src, stage = opt("--src", SRC), opt("--stage", STAGE)
    if "--build" in argv:
        return 0 if build(names, src, stage) else 1
    if "--check" in argv:
        return 0 if check(names, src, stage) else 1
    if "--apply" in argv:
        if "--src" not in argv or "--out" not in argv:
            print("--apply --src DIR --out DIR2 [--files ...] [--stage DIR]")
            return 2
        return 0 if apply(names, src, opt("--out"), stage) else 1
    if "--pictures" in argv:
        if "--files" not in argv:
            names = ["GER", "USA", "JAP"]
        only = opt("--only")
        pictures(names, src, stage, opt("--pics", PICS), only=only.split(",") if only else None)
        return 0
    if "--deliver" in argv:
        if "--with-faults" in argv:
            print("--with-faults is gone: accept a fault one by one in ALLOW, with its reason")
            return 2
        return 1 if deliver(names, stage, content="--content" in argv)[1] else 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
