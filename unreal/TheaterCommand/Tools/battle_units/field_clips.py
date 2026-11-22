"""Release 2 field clips, keyed straight onto each soldier file's own skeleton (no rebuild, nothing else in the file
touched): the stretcher bearers and the lineman.

    stretcher_front   the front man of a two-man stretcher: the file's own walk, slowed to about 60% (shorter
    stretcher_rear    steps, a slower beat), head and shoulders down under the load, both arms down at his sides with
                      the poles in his fists (behind his hips for the front man, before them for the rear man). The two
                      play in step on one clock. The rifle is slung across his back.
    splice_kneel      the lineman at a cut wire: kneeling as kneel_down leaves him (so kneel_down, splice_kneel,
                      stand_up play on without a jump), head down, the two ends of the wire in his hands at chest
                      height, twisting them together, and a tug to test the splice. A loop. The rifle slung.
    carried_wounded   the man on the stretcher, alive: on his back, feet first, breathing, a knee drawn up and let down,
                      a hand on his chest, his head turning (the art director's brief: he must read as alive).

Each clip has its own notes for the game: where the poles (the grips) or the wire's ends (the hands) are, frame by
frame, in the root's frame and the game's axes, so the stretcher and the wire are drawn in the hands.

    python3 field_clips.py --stage DIR [names...]     write each file's three clips to DIR/<name>.field_clips.json
    python3 field_clips.py --deliver [names...]       add them to the shared soldier files (only `clips` changes;
                                                      each file is read fresh, backed up, written whole)

The skeleton's rest frames are the world's (a bone's turn is identity at rest), the clips' keys are 15 a second as
every clip's, rotations as [t, w, x, y, z] and moves as [t, x, y, z] in the parent's frame (soldier.clips_json).
"""
import json
import math
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gore  # noqa: E402

SHARED = "/mnt/project-files/battle-models/land/soldier/"
BACKUP = "/home/user/art/looks/backup/field_clips/"
INFANTRY = ["soldier_USA", "soldier_USA_bar", "soldier_USA_nco", "soldier_USA_medic", "soldier_USA_bazooka",
            "soldier_USA_loader", "soldier_USA_radio", "soldier_USA_para", "soldier_GER", "soldier_SOV",
            "soldier_SOV_winter", "soldier_SOV_snow", "soldier_ITA", "soldier_ITA_desert", "soldier_JAP", "soldier_CHN",
            "soldier_POL"]
RATE = 15
MOVERS = ("pelvis", "pouch_l", "pouch_r", "weapon", "bolt")

# ---- the stretcher (Brian's release 2 bearers, the art director's brief of 11-22: poles about 2.3 m, carried at hip
# height by two men in step, slower than a walk, about 60%, heads down)
SLOW = 1.25                  # (the walk's beat, slowed)
STEP = 0.76                  # (the walk's leg swing, shortened: with the slower beat, about 60% of the walk's speed)
POLES_APART = 0.56           # (the two poles' centres, m)
POLE_LONG = 2.3
GRIPS_APART = 2.15           # (front fists to rear fists: the handles 7.5 cm in from each pole's end)
BED_LONG = 1.83              # (the canvas between the poles, in the middle)
GRIP_X = {"front": -0.11, "rear": 0.12}      # (the wrist ahead of the shoulder, root's frame: the load behind / ahead)
CURL = 82.0                  # (the fingers round a pole)
THUMB = 38.0

# ---- the carried man (on his back on the canvas: the root on the canvas's top, his middle over it)
WOUNDED_LONG = 6.0
WOUNDED_X, WOUNDED_Z = -0.1, 0.13

# ---- the splice
SPLICE_LONG = 2.4
TUG = (1.75, 2.15)


# ================================================================================================ rotations
def R_axis(k, deg):
    k = np.asarray(k, float)
    k = k / np.linalg.norm(k)
    th = math.radians(deg)
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + math.sin(th) * K + (1 - math.cos(th)) * K @ K


def quat(R):
    """The inverse of gore.qmat: [w, x, y, z]."""
    t = np.trace(R)
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        q = (0.25 * s, (R[2, 1] - R[1, 2]) / s, (R[0, 2] - R[2, 0]) / s, (R[1, 0] - R[0, 1]) / s)
    elif R[0, 0] > R[1, 1] and R[0, 0] > R[2, 2]:
        s = math.sqrt(1.0 + R[0, 0] - R[1, 1] - R[2, 2]) * 2
        q = ((R[2, 1] - R[1, 2]) / s, 0.25 * s, (R[0, 1] + R[1, 0]) / s, (R[0, 2] + R[2, 0]) / s)
    elif R[1, 1] > R[2, 2]:
        s = math.sqrt(1.0 + R[1, 1] - R[0, 0] - R[2, 2]) * 2
        q = ((R[0, 2] - R[2, 0]) / s, (R[0, 1] + R[1, 0]) / s, 0.25 * s, (R[1, 2] + R[2, 1]) / s)
    else:
        s = math.sqrt(1.0 + R[2, 2] - R[0, 0] - R[1, 1]) * 2
        q = ((R[1, 0] - R[0, 1]) / s, (R[0, 2] + R[2, 0]) / s, (R[1, 2] + R[2, 1]) / s, 0.25 * s)
    q = np.array(q)
    return q / np.linalg.norm(q)


def qpow(R, f):
    """The turn R taken only `f` of the way from rest."""
    q = quat(R)
    if q[0] < 0:
        q = -q
    ang = 2 * math.acos(min(1.0, q[0]))
    if ang < 1e-9:
        return np.eye(3)
    return R_axis(q[1:], math.degrees(ang * f))


def frame_map(a0, b0, a1, b1):
    """The turn taking a0 onto a1 and b0's part across a0 onto b1's across a1."""
    def fr(a, b):
        a = a / np.linalg.norm(a)
        b = b - a * (b @ a)
        b = b / np.linalg.norm(b)
        return np.stack([a, b, np.cross(a, b)], 1)
    return fr(a1, b1) @ fr(a0, b0).T


def unit(v):
    v = np.asarray(v, float)
    return v / max(np.linalg.norm(v), 1e-12)


# ================================================================================================ the man
class Man:
    def __init__(self, D):
        self.D = D
        self.rig = gore.Rig(D["bones"])
        self.ix = self.rig.index
        self.H = self.rig.head
        self.par = self.rig.parent
        self.nb = len(self.H)
        H = lambda b: self.H[self.ix[b]]
        self.side = {}
        for s, med in (("l", 1.0), ("r", -1.0)):
            up, lo = H("lowerarm_" + s) - H("upperarm_" + s), H("hand_" + s) - H("lowerarm_" + s)
            d0 = unit(H("fingers_" + s) - H("hand_" + s))
            th = H("thumb_" + s) - H("hand_" + s)
            t0 = unit(th - d0 * (th @ d0))
            n0 = unit(np.cross(d0, t0))
            if n0 @ np.array((0.0, med, -0.3)) < 0:      # (the palm: the side toward his thigh at rest)
                n0 = -n0
            self.side[s] = {"u0": up, "l0": lo, "l1": float(np.linalg.norm(up)), "l2": float(np.linalg.norm(lo)),
                            "bend0": unit(np.cross(up, lo)), "d0": d0, "t0": t0, "n0": n0,
                            "thumb0": unit(th), "med": med}
        self.weapon_pts = self._weapon_points()
        self.back_x = self._back()

    def b(self, name):
        return self.ix[name]

    def locals_of(self, clip, t):
        G, X = gore.pose(self.rig, clip, t)
        lq = np.zeros((self.nb, 3, 3))
        mv = np.zeros((self.nb, 3))
        for i in range(self.nb):
            p = self.par[i]
            if p is None:
                lq[i] = G[i]
                mv[i] = X[i] - self.H[i]
            else:
                lq[i] = G[p].T @ G[i]
                mv[i] = G[p].T @ (X[i] - X[p]) - (self.H[i] - self.H[p])
        return lq, mv

    def fk(self, lq, mv):
        G = np.zeros((self.nb, 3, 3))
        X = np.zeros((self.nb, 3))
        for i in range(self.nb):
            p = self.par[i]
            if p is None:
                G[i] = lq[i]
                X[i] = self.H[i] + mv[i]
            else:
                G[i] = G[p] @ lq[i]
                X[i] = X[p] + G[p] @ (self.H[i] - self.H[p] + mv[i])
        return G, X

    def _weapon_points(self):
        M = self.D["near"]
        P = np.array(M["points"]).reshape(-1, 3)
        a, b, w = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"])
        wi = self.b("weapon")
        sub = {wi, self.b("bolt"), self.b("bipod")}
        on = np.array([(x in sub) and ((y in sub) or ww < 0.5) for x, y, ww in zip(a, b, w)])
        return P[on]

    def _back(self):
        """How far back his back (and any pack on it) reaches, between his shoulder blades and his belt."""
        M = self.D["near"]
        P = np.array(M["points"]).reshape(-1, 3)
        a = np.array(M["part"])
        body = ~np.isin(a, [self.b("weapon"), self.b("bolt"), self.b("bipod")])
        sel = body & (np.abs(P[:, 1]) < 0.16) & (P[:, 2] > 0.95) & (P[:, 2] < 1.45)
        return float(P[sel, 0].min()) if sel.any() else -0.16

    # ---- the arm, its hand at (wrist) `T` with the hand's fingers along `d` and its thumb toward `t`
    def arm(self, lq, mv, s, T, pole, d, t, share=0.5):
        A = self.side[s]
        G, X = self.fk(lq, mv)
        ua, la, ha = self.b("upperarm_" + s), self.b("lowerarm_" + s), self.b("hand_" + s)
        S = X[ua]
        l1, l2 = A["l1"], A["l2"]
        dv = np.asarray(T, float) - S
        dist = float(np.linalg.norm(dv))
        dc = min(max(dist, abs(l1 - l2) + 1e-4), (l1 + l2) * 0.999)
        u = dv / max(dist, 1e-9)
        v = np.asarray(pole, float) - u * (np.asarray(pole, float) @ u)
        v = unit(v)
        ca = (l1 * l1 + dc * dc - l2 * l2) / (2 * l1 * dc)
        K = S + l1 * (u * ca + v * math.sqrt(max(0.0, 1 - ca * ca)))
        Tc = S + u * dc
        up, lo = K - S, Tc - K
        bend = np.cross(up, lo)
        if np.linalg.norm(bend) < 1e-6:
            bend = np.cross(up, np.asarray(pole, float))
        Gu = frame_map(A["u0"], A["bend0"], up, unit(bend))
        Gl = frame_map(A["l0"], A["bend0"], lo, unit(bend))
        Gh = frame_map(A["d0"], A["t0"], np.asarray(d, float), np.asarray(t, float))
        # (the forearm takes `share` of the hand's turn about it, so the wrist does not wring)
        la_ = unit(lo)
        tf = Gl @ A["t0"]
        th_ = Gh @ A["t0"]
        tf, th_ = unit(tf - la_ * (tf @ la_)), unit(th_ - la_ * (th_ @ la_))
        phi = math.degrees(math.atan2(np.cross(tf, th_) @ la_, tf @ th_))
        Gl = R_axis(la_, share * phi) @ Gl
        Gp = G[self.par[ua]]
        lq[ua] = Gp.T @ Gu
        lq[la] = Gu.T @ Gl
        lq[ha] = Gl.T @ Gh
        return lq

    def fingers(self, lq, s, curl, thumb):
        A = self.side[s]
        lq[self.b("fingers_" + s)] = R_axis(np.cross(A["d0"], A["n0"]), curl)
        lq[self.b("thumb_" + s)] = R_axis(np.cross(A["thumb0"], A["n0"]), thumb)
        return lq

    def grip(self, G, X, s, along=0.075, palm=0.022):
        """Where a pole (or the wire) runs through his fist: down the hand from the wrist, toward the palm."""
        A = self.side[s]
        h = self.b("hand_" + s)
        return X[h] + G[h] @ (A["d0"] * along + A["n0"] * palm)

    def sling(self, lq, mv, G=None, X=None):
        """The rifle (whatever the file's weapon is) slung across his back: muzzle up by his right shoulder, butt by
        his left hip, its flat side on his back (or on the pack), riding with his chest."""
        if len(self.weapon_pts) == 0:
            return lq, mv
        if G is None:
            G, X = self.fk(lq, mv)
        Pw = self.weapon_pts
        lo, hi = Pw[:, 0].min(), Pw[:, 0].max()
        mid = np.array(((lo + hi) / 2, Pw[:, 1].mean(), Pw[:, 2].mean()))
        a = unit((0.0, 0.38, 1.0))                      # (the muzzle up and toward his right: +y in the game's axes)
        Rw = frame_map(np.array((1.0, 0, 0)), np.array((0, 1.0, 0)), a, np.array((-1.0, 0, 0)))
        ch = self.b("chest")
        Gc, Xc = G[ch], X[ch]
        centre_rest = np.array((self.back_x - 0.035, 0.04, 1.3))
        Gw = Gc @ Rw
        target = Xc + Gc @ (centre_rest - self.H[ch])
        wi = self.b("weapon")
        Xw = target - Gw @ (mid - self.H[wi])
        lq[wi] = G[self.par[wi]].T @ Gw
        mv[wi] = G[self.par[wi]].T @ (Xw - X[self.par[wi]]) - (self.H[wi] - self.H[self.par[wi]])
        return lq, mv


def feet_low(man, G, X):
    """His lowest foot point: the heel (the foot's head, less the sole) or the ball."""
    zs = []
    for s in ("l", "r"):
        zs.append(X[man.b("foot_" + s)][2])
        zs.append(X[man.b("ball_" + s)][2])
    return min(zs)


# ================================================================================================ the clips
def carry_pose(man, role, t):
    W = man.D["clips"]["walk"]
    tw = (t / SLOW) % W["long"]
    lq, mv = man.locals_of(W, tw)
    G0, X0 = man.fk(lq, mv)
    h0 = feet_low(man, G0, X0)
    for s in ("l", "r"):
        lq[man.b("thigh_" + s)] = qpow(lq[man.b("thigh_" + s)], STEP)
        lq[man.b("calf_" + s)] = qpow(lq[man.b("calf_" + s)], 0.92)
        lq[man.b("foot_" + s)] = qpow(lq[man.b("foot_" + s)], 0.85)
    pe = man.b("pelvis")
    lq[pe] = qpow(lq[pe], 0.7)
    # (the load: his trunk a little forward, his head down, his shoulders drawn down)
    for b, deg in (("spine_01", 3.0), ("spine_02", 3.0), ("chest", 3.0), ("neck", 7.0), ("head", 9.0)):
        lq[man.b(b)] = lq[man.b(b)] @ R_axis((0, 1, 0), deg)
    for s, sg in (("l", 1.0), ("r", -1.0)):
        lq[man.b("clavicle_" + s)] = R_axis((1, 0, 0), -5.0 * sg) @ R_axis((0, 1, 0), 3.0)
    G1, X1 = man.fk(lq, mv)
    mv[pe][2] += h0 - feet_low(man, G1, X1)            # (his feet on the ground as in the walk)
    G1, X1 = man.fk(lq, mv)
    for s in ("l", "r"):
        A = man.side[s]
        S = X1[man.b("upperarm_" + s)]
        out = -A["med"]                                  # (his outside: -y on his left)
        y = out * POLES_APART / 2
        # (the wrist above the pole by the fist's reach, the arm nearly straight)
        reach = 0.965 * (A["l1"] + A["l2"])
        dx, dy = GRIP_X[role], y - S[1] - out * 0.018
        drop = math.sqrt(max(reach * reach - dx * dx - dy * dy, 0.04))
        T = np.array((S[0] + dx, S[1] + dy, S[2] - drop))
        d = unit((0.25 * np.sign(dx), 0.0, -1.0))
        tt = np.array((1.0, 0.0, 0.0))
        pole = np.array((-1.0, out * 0.35, 0.0))
        lq = man.arm(lq, mv, s, T, pole, d, tt)
        lq = man.fingers(lq, s, CURL, THUMB)
    lq, mv = man.sling(lq, mv)
    return lq, mv


def splice_pose(man, t):
    K = man.D["clips"]["kneel_down"]
    lq, mv = man.locals_of(K, K["long"])
    u = t / SPLICE_LONG
    tw = 2 * math.pi * 3 * min(t, TUG[0]) / TUG[0]         # (three twists, then the tug)
    tug = math.sin(math.pi * min(max((t - TUG[0]) / (TUG[1] - TUG[0]), 0.0), 1.0))
    look = 2.0 * math.sin(2 * math.pi * u)
    for b, deg in (("spine_01", 4.0), ("spine_02", 4.0), ("chest", 4.0), ("neck", 12.0), ("head", 16.0 + look)):
        lq[man.b(b)] = (np.eye(3) if b in ("neck", "head") else lq[man.b(b)]) @ R_axis((0, 1, 0), deg)
    for s in ("l", "r"):
        lq[man.b("clavicle_" + s)] = R_axis((0, 1, 0), 4.0)
    G, X = man.fk(lq, mv)
    ch = man.b("chest")
    Gc, Xc = G[ch], X[ch]
    for s in ("l", "r"):
        A = man.side[s]
        inw = A["med"]                                    # (toward his middle: +y for the left hand)
        sg = 1.0 if s == "l" else -1.0
        circ = 0.012 * np.array((math.cos(tw + (0 if s == "l" else math.pi)), 0.0,
                                 math.sin(tw + (0 if s == "l" else math.pi))))
        off = np.array((0.31, -inw * (0.045 + 0.03 * tug), 0.0)) + circ
        T = Xc + Gc @ off
        d = unit(Gc @ np.array((0.55, inw * 0.75, -0.35)))
        tt = unit(Gc @ np.array((0.1, 0.0, 1.0)))
        twist = 28.0 * math.sin(tw) * sg * (1.0 - tug)
        tt = R_axis(d, twist) @ tt
        pole = Gc @ np.array((-0.3, -inw * 1.0, -0.6))
        lq = man.arm(lq, mv, s, T, pole, d, tt, share=0.6)
        lq = man.fingers(lq, s, 55.0 + 15.0 * tug, 32.0)
    lq, mv = man.sling(lq, mv)
    return lq, mv


def hide_weapon(man, lq, mv):
    """No weapon in his hands (it went with the stretcher party's kit): the weapon bone 2 m under his root."""
    wi = man.b("weapon")
    lq[wi] = np.eye(3)
    mv[wi] = np.array((0.2, 0.0, -2.0)) - man.H[wi]
    return lq, mv


def wounded_pose(man, t):
    """On his back on the stretcher, feet first, alive: breathing, his left knee drawn up and let down, his right hand
    on his chest, his head turning now and then."""
    lq = np.tile(np.eye(3), (man.nb, 1, 1))
    mv = np.zeros((man.nb, 3))
    u = t / WOUNDED_LONG
    pe = man.b("pelvis")
    lq[pe] = R_axis((0, 1, 0), -90.0)                    # (his front up, his head toward -x: the rear man)
    mv[pe] = np.array((WOUNDED_X, 0.0, WOUNDED_Z)) - man.H[pe]
    breathe = 1.2 * math.sin(2 * math.pi * 5 * u)
    for b, deg in (("spine_01", 3.0), ("spine_02", 3.0), ("chest", 4.0 + breathe), ("neck", 6.0), ("head", 10.0)):
        lq[man.b(b)] = R_axis((0, 1, 0), deg)
    turn = 22.0 * (math.sin(2 * math.pi * u) * 0.6 + 0.4 * math.sin(2 * math.pi * 2 * u + 1.0))
    lq[man.b("head")] = lq[man.b("head")] @ R_axis((0, 0, 1), turn)
    knee = 0.5 - 0.5 * math.cos(2 * math.pi * u)           # (up and down once a loop)
    th = 42.0 * knee
    # (a hip bends forward about -y, a knee back about +y; the foot from toes relaxed down to flat on the canvas)
    lq[man.b("thigh_l")] = R_axis((0, 1, 0), -th) @ R_axis((1, 0, 0), -4.0 * knee)
    lq[man.b("calf_l")] = R_axis((0, 1, 0), 2.0 * th)
    lq[man.b("foot_l")] = R_axis((0, 1, 0), 20.0 + 26.0 * knee)
    lq[man.b("foot_r")] = R_axis((0, 1, 0), 20.0)
    lq[man.b("thigh_r")] = R_axis((0, 1, 0), -3.0)
    lq[man.b("calf_r")] = R_axis((0, 1, 0), 6.0)
    G, X = man.fk(lq, mv)
    ch = man.b("chest")
    # (his right hand flat on his chest, his left lying along his side on the canvas)
    Tr = X[ch] + np.array((0.02, 0.02, 0.15 + 0.004 * breathe))
    lq = man.arm(lq, mv, "r", Tr, np.array((0.2, 1.0, 0.3)), unit((0.25, -1.0, -0.15)), np.array((-1.0, 0.0, 0.1)))
    Tl = X[man.b("upperarm_l")] + np.array((0.47, -0.07, -0.06))
    lq = man.arm(lq, mv, "l", Tl, np.array((0.0, -1.0, 0.2)), unit((1.0, 0.1, -0.35)), np.array((0.0, 0.3, 1.0)))
    for s in ("l", "r"):
        lq = man.fingers(lq, s, 22.0, 15.0)
    return hide_weapon(man, lq, mv)


def times_of(L):
    ts = [k / RATE for k in range(int(math.floor(L * RATE + 1e-6)) + 1)]
    if ts[-1] < L - 1e-6:
        ts.append(L)
    return ts


def bake(man, fn, L):
    """The clip's tracks (as soldier.clips_json writes them) and each sample's pose."""
    ts = times_of(L)
    rot = {i: [] for i in range(man.nb) if man.par[i] is not None}
    move = {man.b(b): [] for b in MOVERS if b in man.ix}
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
            rot[i].append([round(t, 4)] + [round(float(v), 5) for v in q])
        for i in move:
            move[i].append([round(t, 4)] + [round(float(v), 4) for v in mv[i]])
    tracks = []
    for i in range(man.nb):
        if man.par[i] is None:
            continue
        tr = {"part": i, "part_name": man.rig.names[i]}
        if not all(abs(k[1] - 1.0) < 1e-4 for k in rot[i]):
            tr["rot"] = rot[i]
        if i in move and any(max(abs(v) for v in k[1:]) > 1e-4 for k in move[i]):
            tr["move"] = move[i]
        if len(tr) > 2:
            tracks.append(tr)
    return tracks, poses


def stride_of(man, poses, L):
    """The ground his stance foot passes under him in a cycle (the game moves the root at stride / long)."""
    xs = {s: [] for s in ("l", "r")}
    zs = {s: [] for s in ("l", "r")}
    for t, lq, mv in poses:
        G, X = man.fk(lq, mv)
        for s in ("l", "r"):
            xs[s].append(X[man.b("ball_" + s)][0])
            zs[s].append(X[man.b("ball_" + s)][2])
    v = []
    ts = [p[0] for p in poses]
    for s in ("l", "r"):
        z = np.array(zs[s])
        x = np.array(xs[s])
        on = z < z.min() + 0.012
        for k in range(len(ts) - 1):
            if on[k] and on[k + 1]:
                v.append((x[k + 1] - x[k]) / (ts[k + 1] - ts[k]))
    return round(float(-np.median(v) * L), 3) if v else 0.0


def clips_for(D):
    man = Man(D)
    W = D["clips"]["walk"]
    L = round(W["long"] * SLOW, 4)
    out = {}
    for role in ("front", "rear"):
        tracks, poses = bake(man, lambda t, r=role: carry_pose(man, r, t), L)
        grips = []
        for t, lq, mv in poses:
            G, X = man.fk(lq, mv)
            gl, gr = man.grip(G, X, "l"), man.grip(G, X, "r")
            grips.append([round(t, 4)] + [round(float(v), 4) for v in (*gl, *gr)])
        evs = [dict(e, t=round(e["t"] * SLOW, 4)) for e in W["events"]]
        st = stride_of(man, poses, L)
        mid = np.mean([g[1:] for g in grips], 0)
        out["stretcher_" + role] = {
            "long": L, "loop": True, "stride": st, "weapon": "slung", "tracks": tracks, "events": evs,
            "from": "field_clips.py: this file's walk (motion capture), slowed and shortened, the arms keyed to the poles",
            "about": (f"The {role} man of a two-man stretcher, walking at about {st / L:.2f} m/s (stride {st} m a "
                      f"{L:.2f} s cycle), head down. Play stretcher_front and stretcher_rear on one clock (same phase), "
                      f"the {'rear' if role == 'front' else 'front'} man's root placed so the front fists are "
                      f"{GRIPS_APART} m ahead of the rear fists (the `grips` means: front root x = rear root x + "
                      f"{GRIPS_APART} + rear mean x - front mean x). The poles: two, {POLE_LONG} m long, {POLES_APART} m "
                      "apart, through `grips` (his left fist then his right, the root's frame, the game's axes), "
                      f"their handles 7.5 cm in from each end; the canvas {BED_LONG} m long in the middle. The rifle "
                      "rides slung on his back."),
            "grips_apart": GRIPS_APART, "pole_long": POLE_LONG, "poles_apart": POLES_APART, "bed_long": BED_LONG,
            "grips": {"track_fields": ["t", "lx", "ly", "lz", "rx", "ry", "rz"], "track": grips,
                      "mean": [round(float(v), 3) for v in mid]},
        }
    tracks, poses = bake(man, lambda t: splice_pose(man, t), SPLICE_LONG)
    hands = []
    for t, lq, mv in poses:
        G, X = man.fk(lq, mv)
        gl, gr = man.grip(G, X, "l", along=0.07, palm=0.03), man.grip(G, X, "r", along=0.07, palm=0.03)
        hands.append([round(t, 4)] + [round(float(v), 4) for v in (*gl, *gr)])
    out["splice_kneel"] = {
        "long": SPLICE_LONG, "loop": True, "stride": 0.0, "weapon": "slung", "tracks": tracks,
        "events": [{"t": round((TUG[0] + TUG[1]) / 2, 4), "fx": "wire_tug", "socket": None, "cue": "wire_tug"}],
        "from": "field_clips.py: kneel_down's last pose, the arms keyed",
        "about": ("The lineman at a cut wire, kneeling (kneel_down, then this, then stand_up), twisting the two ends "
                  "together in his hands three times and giving it a tug (event wire_tug) to test it. The wire's two "
                  "ends run into his fists at `hands` (left then right, the root's frame, the game's axes): draw each "
                  "end from there down to the ground and away along the line. His reel sits on the ground by his "
                  "right knee. The rifle rides slung on his back."),
        "hands": {"track_fields": ["t", "lx", "ly", "lz", "rx", "ry", "rz"], "track": hands},
    }
    tracks, poses = bake(man, lambda t: wounded_pose(man, t), WOUNDED_LONG)
    out["carried_wounded"] = {
        "long": WOUNDED_LONG, "loop": True, "stride": 0.0, "weapon": "none", "tracks": tracks, "events": [],
        "from": "field_clips.py: keyed on the rig",
        "about": ("The man on the stretcher, alive (the dead stay still; he does not): on his back, feet first (his "
                  "head toward -x, the rear man), breathing, his left knee drawn up and let down, his right hand on his "
                  "chest, his head turning now and then. The root goes on the canvas's top, in the middle of the "
                  "stretcher between the two bearers' grips, turned with it. No weapon (the weapon bone rides 2 m "
                  "under his root). Put his white field dressing where the wound is."),
    }
    return out, man


def check(D, clips, man):
    """What the clips must hold: loops close, no hand through the poles' line, feet on the ground, the bones' turns
    sane (no flip from one key to the next)."""
    notes = []
    for name, c in clips.items():
        tmp = dict(D)
        G0, X0 = gore.pose(man.rig, c, 0.0)
        G1, X1 = gore.pose(man.rig, c, c["long"])
        gap = float(np.abs(X1 - X0).max())
        if c["loop"] and gap > 0.01:
            notes.append(f"{name}: the loop does not close ({gap * 100:.1f} cm)")
        jump = 0.0
        prevX = None
        lows = []
        for t in times_of(c["long"]):
            G, X = gore.pose(man.rig, c, t)
            if prevX is not None:
                jump = max(jump, float(np.abs(X - prevX).max()))
            prevX = X
            lows.append(feet_low(man, G, X))
        if jump > 0.25:
            notes.append(f"{name}: a bone jumps {jump * 100:.0f} cm between keys")
        notes.append(f"{name}: lowest foot {min(lows) * 100:.1f} to {max(lows) * 100:.1f} cm up, biggest step between "
                     f"keys {jump * 100:.1f} cm")
        del tmp
    return notes


def stage(dirpath, names):
    os.makedirs(dirpath, exist_ok=True)
    for nm in names:
        D = json.load(open(SHARED + nm + ".json"))
        t0 = time.time()
        clips, man = clips_for(D)
        for n in check(D, clips, man):
            print("   ", nm, n)
        with open(os.path.join(dirpath, nm + ".field_clips.json"), "w") as f:
            json.dump(clips, f, separators=(",", ":"))
        print(nm, f"{time.time() - t0:.1f} s", {k: (v["long"], v["stride"]) for k, v in clips.items()})


def deliver(dirpath, names):
    """Each shared file read fresh, its clips added (nothing else touched), backed up first, written whole through a
    temporary file, and read back."""
    os.makedirs(BACKUP, exist_ok=True)
    for nm in names:
        add = json.load(open(os.path.join(dirpath, nm + ".field_clips.json")))
        path = SHARED + nm + ".json"
        raw = open(path).read()
        D = json.loads(raw)
        bk = BACKUP + nm + ".json"
        if not os.path.exists(bk):
            with open(bk, "w") as f:
                f.write(raw)
        D["clips"].update(add)
        tmp = path + ".tmp"
        with open(tmp, "w") as f:
            json.dump(D, f, separators=(",", ":"))
        os.replace(tmp, path)
        print("added", sorted(add), "to", nm)


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "--stage":
        stage(a[1], a[2:] or INFANTRY)
    elif a and a[0] == "--deliver":
        deliver(a[1], a[2:] or INFANTRY)
    else:
        print(__doc__)
