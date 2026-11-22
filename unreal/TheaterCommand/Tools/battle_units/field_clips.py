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
# (the tank modeller's Stretcher prop, battle-models/ground-props/Stretcher.json: grip.* at x +-1.1, y +-0.29, z 0.14
# over its root; the carried man at `lie`, z 0.11, head to +x, the head end first)
POLES_APART = 0.58
POLE_LONG = 2.3
GRIPS_APART = 2.2
GRIP_Z = 0.14
# (the CableReel prop: grip.left / grip.right at y -+0.115, z 0.56 over its root)
REEL_GRIP_Y, REEL_GRIP_Z = 0.115, 0.56
REEL_AHEAD = 0.47            # (the reel's handles ahead of his pelvis, so its frame clears his knees)
REEL_HIGH = 0.9              # (the handles' height over the ground)
GRIP_X = {"front": -0.11, "rear": 0.12}      # (the wrist ahead of the shoulder, root's frame: the load behind / ahead)
CURL = 82.0                  # (the fingers round a pole)
THUMB = 38.0

# ---- the carried man (on his back on the canvas: the root on the canvas's top, his middle over it)
WOUNDED_LONG = 6.0
WAITING_LONG = 8.0
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
        self._body = None
        self.settled = {}

    def body_points(self):
        """His body's points (not the weapon) with their two bones and weight, and which of them are his hips, his
        back above them (and any pack on it), and his lower legs: for setting a lying man down on the ground."""
        if self._body is None:
            M = self.D["near"]
            P = np.array(M["points"]).reshape(-1, 3)
            a, b, w = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
            gun = [self.b(x) for x in ("weapon", "bolt", "bipod") if x in self.ix]
            keep = ~np.isin(a, gun) & ~np.isin(b, gun)
            P, a, b, w = P[keep], a[keep], b[keep], w[keep]
            zone = lambda names: np.isin(a, [self.b(x) for x in names if x in self.ix])
            self._body = (P, a, b, w, {
                "hips": zone(("pelvis", "pouch_l", "pouch_r")),
                "back": zone(("spine_01", "spine_02", "chest", "neck", "head", "clavicle_l", "clavicle_r")),
                "legs": zone(("thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r"))})
        return self._body

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

    def wrist_for(self, s, grip_at, d, t, along=0.075, palm=0.022):
        """Where his wrist goes for his fist (as `grip` reads it) to close on `grip_at`, the hand along d, thumb t."""
        A = self.side[s]
        Gh = frame_map(A["d0"], A["t0"], np.asarray(d, float), np.asarray(t, float))
        return np.asarray(grip_at, float) - Gh @ (A["d0"] * along + A["n0"] * palm)

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

    def lay_weapon(self, lq, mv):
        """His weapon lying on the ground by his right side, along him, muzzle toward his head, on its side."""
        if len(self.weapon_pts) == 0:
            return lq, mv
        G, X = self.fk(lq, mv)
        Pw = self.weapon_pts
        lo, hi = Pw[:, 0].min(), Pw[:, 0].max()
        mid = np.array(((lo + hi) / 2, Pw[:, 1].mean(), Pw[:, 2].mean()))
        half = float(np.abs(Pw[:, 1] - Pw[:, 1].mean()).max())
        along = X[self.b("head")] - X[self.b("pelvis")]
        along[2] = 0.0
        along = unit(along)
        right = X[self.b("upperarm_r")] - X[self.b("upperarm_l")]
        right[2] = 0.0
        right = unit(right)
        Gw = frame_map(np.array((1.0, 0, 0)), np.array((0, 1.0, 0)), along, np.array((0.0, 0.0, 1.0)))
        target = 0.5 * (X[self.b("upperarm_r")] + X[self.b("thigh_r")]) + right * 0.3
        target[2] = half + 0.005
        wi = self.b("weapon")
        Xw = target - Gw @ (mid - self.H[wi])
        p = self.par[wi]
        lq[wi] = G[p].T @ Gw
        mv[wi] = G[p].T @ (Xw - X[p]) - (self.H[wi] - self.H[p])
        return lq, mv

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


def sm(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def ramp(t, t0, t1):
    return sm((t - t0) / (t1 - t0))


def feet_low(man, G, X):
    """His lowest foot point: the heel (the foot's head, less the sole) or the ball."""
    zs = []
    for s in ("l", "r"):
        zs.append(X[man.b("foot_" + s)][2])
        zs.append(X[man.b("ball_" + s)][2])
    return min(zs)


# ================================================================================================ the clips
def slow_walk(man, t, bend=(3.0, 3.0, 3.0, 7.0, 9.0)):
    """The file's own walk, slowed (SLOW) and its legs' swing shortened (STEP), trunk and head down by `bend`
    (spine_01, spine_02, chest, neck, head, degrees), shoulders drawn down, feet kept on the ground as in the walk."""
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
    for b, deg in zip(("spine_01", "spine_02", "chest", "neck", "head"), bend):
        lq[man.b(b)] = lq[man.b(b)] @ R_axis((0, 1, 0), deg)
    for s, sg in (("l", 1.0), ("r", -1.0)):
        lq[man.b("clavicle_" + s)] = R_axis((1, 0, 0), -5.0 * sg) @ R_axis((0, 1, 0), 3.0)
    G1, X1 = man.fk(lq, mv)
    mv[pe][2] += h0 - feet_low(man, G1, X1)
    return lq, mv


def carry_pose(man, role, t):
    """A stretcher bearer: arms down at his sides, nearly straight, a pole handle in each fist (the prop's grips
    POLES_APART apart), just behind his hips (front man) or before them (rear man)."""
    lq, mv = slow_walk(man, t)
    G1, X1 = man.fk(lq, mv)
    for s in ("l", "r"):
        A = man.side[s]
        S = X1[man.b("upperarm_" + s)]
        out = -A["med"]                                  # (his outside: -y on his left)
        d = unit((0.25 * np.sign(GRIP_X[role]), 0.0, -1.0))
        tt = np.array((1.0, 0.0, 0.0))
        # (the fist on the pole's line: the wrist's sideways place for it, then its drop from the shoulder for a
        # nearly straight arm)
        o = man.wrist_for(s, np.zeros(3), d, tt)
        reach = 0.965 * (A["l1"] + A["l2"])
        dx = GRIP_X[role]
        dy = out * POLES_APART / 2 + o[1] - S[1]
        drop = math.sqrt(max(reach * reach - dx * dx - dy * dy, 0.04))
        T = np.array((S[0] + dx, S[1] + dy, S[2] - drop))
        pole = np.array((-1.0, out * 0.35, 0.0))
        lq = man.arm(lq, mv, s, T, pole, d, tt)
        lq = man.fingers(lq, s, CURL, THUMB)
    lq, mv = man.sling(lq, mv)
    return lq, mv


def reel_pose(man, t):
    """The lineman walking his line out: bent forward, the cable reel carried before him by its two handles (the
    CableReel prop's grip.left / grip.right), its frame clear of his knees."""
    lq, mv = slow_walk(man, t, bend=(6.0, 6.0, 5.0, 4.0, 10.0))
    G1, X1 = man.fk(lq, mv)
    pz = X1[man.b("pelvis")][2] - (man.H[man.b("pelvis")][2] + 0.0)
    for s in ("l", "r"):
        A = man.side[s]
        out = -A["med"]
        grip = np.array((X1[man.b("pelvis")][0] + REEL_AHEAD, out * REEL_GRIP_Y, REEL_HIGH + 0.6 * pz))
        d = unit((0.3, -out * 0.25, -0.92))
        tt = unit((1.0, 0.0, 0.25))
        T = man.wrist_for(s, grip, d, tt)
        pole = np.array((-0.6, out * 1.0, -0.5))
        lq = man.arm(lq, mv, s, T, pole, d, tt, share=0.6)
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


SETTLED = {}
HAND_DOWN = 0.075


BACK_SHARE = (("spine_01", 0.55), ("spine_02", 0.45))
BEND_MOST = 12.0             # (how far his back may arch or round to lie on the ground)
TILT = (-6.0, 20.0)          # (how far his trunk may lie head-down or propped head-up, on a pack)
SINK = 0.03                  # (how far a pack may press into soft ground or the canvas)


def settle(man, lq, mv):
    """How a man on his back lies on the ground. His trunk is tilted (head up, propped on a pack, or head down) and his
    back arched or rounded a little, the least that rests both his seat and his shoulders (or the pack under them,
    which may press SINK into the ground) on the ground; then his legs are let down (or up) at the hips until his
    heels and calves rest on it. Returns (tilt, lift, bend, hip): degrees about y at the pelvis (+ his head up), metres,
    degrees about y shared by spine_01 and spine_02, degrees about y at both thighs."""
    P, a, b, w, Z = man.body_points()
    pe = man.b("pelvis")

    def back(l2, deg):
        for s, f in BACK_SHARE:
            l2[man.b(s)] = R_axis((0, 1, 0), deg * f) @ l2[man.b(s)]
        return l2

    best = None
    for bend in np.arange(-BEND_MOST, BEND_MOST + 0.01, 0.5):
        G, X = man.fk(back(lq.copy(), bend), mv)
        Q = gore.skin(P, a, b, w, G, X, man.H) - X[pe]
        for tilt in np.arange(TILT[0], TILT[1] + 0.01, 0.5):
            r = math.radians(tilt)
            z = X[pe][2] - Q[:, 0] * math.sin(r) + Q[:, 2] * math.cos(r)
            zh, zb = float(z[Z["hips"]].min()), float(z[Z["back"]].min())
            lift = max(-zh, -zb - SINK)
            gh, gb = zh + lift, zb + lift
            cost = gh + max(gb, 0.0) + 0.25 * max(-gb, 0.0) + 0.002 * (abs(tilt) + abs(bend))
            if best is None or cost < best[0]:
                best = (cost, float(tilt), float(lift), float(bend))
    _, tilt, lift, bend = best
    l2, m2 = back(lq.copy(), bend), mv.copy()
    l2[pe] = R_axis((0, 1, 0), tilt) @ l2[pe]
    m2[pe] = m2[pe] + np.array((0.0, 0.0, lift))
    best = None
    for hip in np.arange(-15.0, 15.01, 0.25):
        l3 = l2.copy()
        for s in ("thigh_l", "thigh_r"):
            l3[man.b(s)] = R_axis((0, 1, 0), hip) @ l3[man.b(s)]
        z = gore.skin(P, a, b, w, *man.fk(l3, m2), man.H)[:, 2]
        miss = abs(float(z[Z["legs"]].min())) + 0.0005 * abs(hip)
        if best is None or miss < best[0]:
            best = (miss, float(hip))
    return tilt, lift, bend, best[1]


def fold_whip(man, lq, mv, out=0.42):
    """A radio man's long whip antenna (whip_1 to whip_4, if the file has it) taken down: its four sections folded
    back and forth into one bundle lying by his left side from the set toward his feet, a little splayed; `out` is how
    far to his left its far end lies (inside the poles on a stretcher)."""
    if "whip_1" not in man.ix:
        return lq
    G, X = man.fk(lq, mv)
    W = [man.b(n) for n in ("whip_1", "whip_2", "whip_3", "whip_4")]
    r0 = unit(man.H[W[1]] - man.H[W[0]])
    feet = unit(X[man.b("pelvis")] - X[man.b("neck")])
    left = unit(X[man.b("upperarm_l")] - X[man.b("upperarm_r")])
    foot = X[W[0]]
    end = foot + feet * 0.62 + left * out
    end[2] = 0.02
    d = unit(end - foot)
    up = unit(np.cross(d, left))
    prev = G[man.par[W[0]]]
    for k, i in enumerate(W):
        dk = (d if k % 2 == 0 else -d) + left * (0.04 * k)
        Gk = frame_map(r0, unit(np.cross(r0, (0.0, 0.0, 1.0))), unit(dk), up)
        lq[i] = prev.T @ Gk
        prev = Gk
    return lq


def wounded_pose(man, t, waiting=False):
    """On his back, alive: breathing, his left knee drawn up and let down, his right hand on his chest, his head
    turning now and then; head to +x (the stretcher's head end). Waiting on the ground for the bearers, his right hand
    is pressed to the wound at his side, he breathes harder, lifts his left arm now and then to call them, and his
    rifle lies by his right side."""
    lq = np.tile(np.eye(3), (man.nb, 1, 1))
    mv = np.zeros((man.nb, 3))
    L = WAITING_LONG if waiting else WOUNDED_LONG
    u = t / L
    pe = man.b("pelvis")
    lq[pe] = R_axis((0, 1, 0), -90.0)                    # (his front up, his head toward -x: the rear man)
    mv[pe] = np.array((WOUNDED_X, 0.0, WOUNDED_Z)) - man.H[pe]
    breathe = (1.8 * math.sin(2 * math.pi * 12 * u)) if waiting else 1.2 * math.sin(2 * math.pi * 5 * u)
    for b, deg in (("chest", breathe), ("neck", 8.0), ("head", 12.0)):
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
    # (set down on the ground or the canvas: hips, heels and shoulders (or the pack under them) touching, found once
    # from the first pose and kept, so nothing sinks in and nothing floats)
    if man not in SETTLED:
        SETTLED[man] = settle(man, lq, mv)
    tilt, lift, bend, hip = SETTLED[man]
    Rt = R_axis((0, 1, 0), tilt)
    lq[pe] = Rt @ lq[pe]
    mv[pe] = mv[pe] + np.array((0.0, 0.0, lift))
    for s in ("thigh_l", "thigh_r"):
        lq[man.b(s)] = R_axis((0, 1, 0), hip) @ lq[man.b(s)]
    for s, f in BACK_SHARE:
        lq[man.b(s)] = R_axis((0, 1, 0), bend * f) @ lq[man.b(s)]
    G, X = man.fk(lq, mv)
    ch = man.b("chest")
    # (his right hand flat on his chest, or pressed to the wound at his right side; his left lying along his side, or
    # lifted to call the bearers)
    # (the right hand's place on him in his own frame, so it rides a propped trunk; its ways tilted with him)
    if waiting:
        sp = man.b("spine_01")
        Tr = X[sp] + G[sp] @ np.array((0.13 + 0.004 * breathe, 0.1, -0.02))
        lq = man.arm(lq, mv, "r", Tr, Rt @ np.array((-0.2, 1.0, 0.2)), Rt @ unit((0.6, -0.7, -0.3)),
                     Rt @ np.array((-0.3, 0.0, 1.0)))
    else:
        Tr = X[ch] + G[ch] @ np.array((0.15 + 0.004 * breathe, 0.02, -0.02))
        lq = man.arm(lq, mv, "r", Tr, Rt @ np.array((0.2, 1.0, 0.3)), Rt @ unit((0.25, -1.0, -0.15)),
                     Rt @ np.array((-1.0, 0.0, 0.1)))
    lift = ramp(u, 0.38, 0.52) * (1 - ramp(u, 0.64, 0.8)) if waiting else 0.0
    wave = 0.06 * math.sin(2 * math.pi * 3 * u) * lift
    Tl0 = X[man.b("upperarm_l")] + np.array((0.47, -0.07, 0.0))
    Tl0[2] = HAND_DOWN                                   # (his wrist just over the ground, his hand lying on it)
    Tl1 = X[man.b("upperarm_l")] + np.array((0.12, -0.12 + wave, 0.5))
    Tl = Tl0 * (1 - lift) + Tl1 * lift
    dl = unit(np.array((1.0, 0.1, -0.35)) * (1 - lift) + np.array((0.1, 0.0, 1.0)) * lift)
    tl = unit(np.array((0.0, 0.3, 1.0)) * (1 - lift) + np.array((1.0, 0.2, 0.0)) * lift)
    lq = man.arm(lq, mv, "l", Tl, np.array((0.0, -1.0, 0.2)), dl, tl)
    for s in ("l", "r"):
        lq = man.fingers(lq, s, 22.0 if not (waiting and s == "r") else 45.0, 15.0)
    lq = fold_whip(man, lq, mv, 0.42 if waiting else 0.2)
    # (turned end for end about the upright, so his head is at +x: every bone below the pelvis goes with it)
    Rz = R_axis((0, 0, 1), 180.0)
    at = man.H[pe] + mv[pe]
    lq[pe] = Rz @ lq[pe]
    mv[pe] = Rz @ at - man.H[pe]
    if not waiting:
        return hide_weapon(man, lq, mv)
    return man.lay_weapon(lq, mv)


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
            "about": (f"The {role} man of a two-man stretcher (the Stretcher prop, battle-models/ground-props), walking "
                      f"at about {st / L:.2f} m/s (stride {st} m a {L:.2f} s cycle), head down, the prop's "
                      f"grip.{role.replace('rear', 'back')}.left/right in his fists: `grips` (his left fist then his "
                      "right, every key, the root's frame, the game's axes; `mean` their average). Play "
                      "stretcher_front and stretcher_rear on one clock (same phase, in step). Place the prop's root at "
                      f"the mean of the four fists less {GRIP_Z} m, head end (+x) toward the front man; then his root x "
                      f"= the prop's x {'+' if role == 'front' else '-'} {GRIPS_APART / 2} - his grips' mean x. The "
                      "carried man (carried_wounded) at the prop's `lie`. The rifle rides slung on his back."),
            "grips_apart": GRIPS_APART, "poles_apart": POLES_APART,
            "grips": {"track_fields": ["t", "lx", "ly", "lz", "rx", "ry", "rz"], "track": grips,
                      "mean": [round(float(v), 3) for v in mid]},
        }
    # (the lineman walking his line out with the reel)
    tracks, poses = bake(man, lambda t: reel_pose(man, t), L)
    reel = []
    for t, lq, mv in poses:
        G, X = man.fk(lq, mv)
        gl, gr = man.grip(G, X, "l"), man.grip(G, X, "r")
        c = 0.5 * (gl + gr) - np.array((0.0, 0.0, REEL_GRIP_Z))
        reel.append([round(t, 4)] + [round(float(v), 4) for v in (*c, *gl, *gr)])
    st = stride_of(man, poses, L)
    out["reel_walk"] = {
        "long": L, "loop": True, "stride": st, "weapon": "slung", "tracks": tracks,
        "events": [dict(e, t=round(e["t"] * SLOW, 4)) for e in W["events"]],
        "from": "field_clips.py: this file's walk (motion capture), slowed and shortened, bent, the arms keyed to the reel",
        "about": (f"The lineman walking his line out, about {st / L:.2f} m/s (stride {st} m a {L:.2f} s cycle), bent "
                  "forward, the CableReel prop (battle-models/ground-props) carried before him by its two handles: "
                  "`reel` gives, every key, the prop's root (x, y, z) and his left and right fists (on its grip.left "
                  "and grip.right), the root's frame, the game's axes; the prop faces as he does. Turn its drum by the "
                  "wire paid out (the prop's README). The rifle rides slung on his back."),
        "reel": {"track_fields": ["t", "x", "y", "z", "lx", "ly", "lz", "rx", "ry", "rz"], "track": reel},
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
        "about": ("The man on the stretcher, alive (the dead stay still; he does not): on his back, head to +x (the "
                  "Stretcher prop's head end, carried first), breathing, his left knee drawn up and let down, his right "
                  "hand on his chest, his head turning now and then. His root goes at the prop's `lie` socket, turned "
                  "with it. No weapon (the weapon bone rides 2 m under his root). His white field dressing is the "
                  "game's to place."),
    }
    tracks, poses = bake(man, lambda t: wounded_pose(man, t, waiting=True), WAITING_LONG)
    out["wounded_waiting"] = {
        "long": WAITING_LONG, "loop": True, "stride": 0.0, "weapon": "laid", "tracks": tracks, "events": [],
        "from": "field_clips.py: keyed on the rig",
        "about": ("A wounded man on the ground waiting for the bearers, alive: on his back (head to +x, his root on "
                  "the ground under his middle), breathing hard, his right hand pressed to the wound at his right side, "
                  "his left knee drawn up and let down, his head turning, and once a loop his left arm lifted to call "
                  "them. His rifle lies by his right side. When the bearers reach him: carried_wounded on the "
                  "stretcher (the same lie, so it swaps without a jump but for the hands)."),
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
