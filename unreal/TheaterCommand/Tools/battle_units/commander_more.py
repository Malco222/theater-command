"""The tank commanders' duck and peek: two clips for the drill that closes and opens the hatch (release 2).

The tank thread's drill flips a tank's `buttoned` flag (0.3 to 1.6 s after a round passes her commander or the enemy
comes near; up again after 8 to 30 quiet seconds). The rider code plays, on soldier_USA_tanker (the Sherman) and on
soldier_GER_panzer (the Panzer IV and the Tiger):

- commander_hatch_duck (0.4 s, no loop) when `buttoned` turns true. From commander_hatch's pose at 0 s (`start`; the
  game blends 0.12 s to it from wherever he was). A startled man: at once his head goes down between his shoulders
  and his arms snap in off the rim, the glasses up against his chest and his elbows against his ribs (0.2 s); his
  knees give from 0.09 s and he falls through the ring at about a free fall's pace, curling forward and sitting back
  as he goes, with no slowing at the end. At the last key the top of his head is under the rim: the event `hide`
  there (stop drawing him), and the glasses' `shown` is 0. (His arms lead because they must: his elbows stand out
  over the rim, wider than the ring, and pass it only pressed to his ribs.)
- commander_hatch_peek (2.2 s, no loop) when `buttoned` turns false. From the duck's last pose (`start`) he comes up
  at the back of the ring: quickly while his eyes are still under the rim (0 to 0.25 s), then his eyes creep over it
  (0.38 m/s at most) and ease to a stop a hand over it (11 cm, 0.62 s; only his head and the tops of his shoulders
  out), the glasses to his eyes and his elbows down in front of him. He looks round, a slow even pan eased at both
  ends: 20 degrees to his left, held a quarter of a second, across to 20 degrees to his right, held (his head and
  chest together, the glasses on his eyes; his head turns at 116 degrees a second at most). Then he faces ahead and
  stands up, and once his shoulders are over the rim his elbows come out over it, he leans forward and sets the
  glasses down: commander_hatch's pose at 0 s, exactly, at the last key, so commander_hatch follows with no jump. The
  event `show` at the first key. (2.2 s, not 2.0: the slow look takes 1.2 s, and the stand-up after it is kept to the
  pace of the man's own arm moves in commander_hatch; at 2.0 s his arms would swing out at 900 degrees a second.)

Both are keyed here at the json's level (no mesh is touched, no other clip): commander_hatch's pose at 0 s is the
standing pose, and every key is that pose with his pelvis lowered (move keys on the pelvis: the root, the floor at his
feet, does not move; his feet stay where they are and the knees fold), his back, neck and shoulders turned, and his
arms solved again to the glasses, which ride his chest or his eyes or lie on the rim. The glasses' `props` track is
written with the hands (30 a second, the clip's root frame). Each clip carries the `anchor`, `places` and `weapon`
of the file's commander_hatch.

The hatches are measured from the tanks' files (the rim's height and the ring's inside radius at the `commander`
socket); nothing here assumes the Sherman's numbers for the German cupolas.

    python3 commander_more.py --build [--out DIR]      both files with the two clips, staged in DIR; the checks
    python3 commander_more.py --check [--out DIR]      the checks again on the staged files, as they are written
    python3 commander_more.py --deliver [--content]    staged files to the shared folder (and Content/Models/Soldiers)
"""

import hashlib
import json
import math
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import field_clips as fc  # noqa: E402
import gore  # noqa: E402

SHARED = "/mnt/project-files/battle-models/land/soldier/"
CONTENT = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Models", "Soldiers"))
TANKS = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Models", "Battle"))
STAGE = "/home/user/art/commander_more/out/"
BACKUP = "/home/user/art/commander_more/backup/"
REPORT = "commander_more_report.json"
FILES = {"soldier_USA_tanker": ("Sherman",), "soldier_GER_panzer": ("PanzerIV", "Tiger")}
BASE, DUCK, PEEK = "commander_hatch", "commander_hatch_duck", "commander_hatch_peek"
DROP_CLIP = "die_hatch_drop"      # (the file's own check of it gives the ring's tolerance)

RATE = 30                         # keys a second (the glasses' track is 30 a second too, so hands and glasses agree)
DUCK_LONG = 0.4
PEEK_LONG = 2.2
UNDER = 0.06                      # the duck ends with the top of his head (and all of him) this far under the rim
EYES_OVER = 0.11                  # the peek: his eyes this far over the rim while he looks round
SCAN = 20.0                       # degrees each way that he looks round (his head and chest together)
# (when, in the peek, s)
# The rise is laid down as his eyes' height over the rim, and the crouch-to-peek blend is solved to it at each key:
# from the duck's end at rest, quickly up to the rim (he is under it; the speed eases in and out), arriving at
# RISE_SPEED; on over the rim at that speed, easing to a stop with his eyes EYES_OVER over it at PEEK_AT.
RIM_AT = 0.25                     # his eyes at the rim
RISE_SPEED = 0.38                 # m/s: his eyes over the rim, at most
PEEK_AT = 0.62                    # his eyes at EYES_OVER (the peek's pose)
RISE_EASE = 0.3                   # under the rim: the share of the time spent speeding up (and slowing to RISE_SPEED)
GLASSES_UP = (0.05, 0.45)         # the glasses from his chest to his eyes
LOOK = ((0.47, 0.0), (0.71, -1.0), (0.96, -1.0), (1.42, 1.0), (1.67, 1.0), (1.97, 0.0))   # (time, share of SCAN: left is -; he holds at each end)
TURN_EASE = 0.25                  # each turn of the look: this share of it speeding up, the same slowing down, even between
OUT = (1.67, 2.04)                # up out of the hatch to the standing height (he faces ahead again as he rises)
LEAN_OUT = (1.88, 2.20)           # forward to the rim again (he comes up at the back of the ring, where he peeked)
ELBOWS_OUT = (1.89, 2.20)         # his elbows from in front of him out over the rim (his shoulders are over it by then)
GLASSES_DOWN = (1.94, 2.20)       # the glasses from his eyes down onto the rim
SHRUG_DOWN = 0.11                 # his shoulders let down from this long after his elbows start out, to the end
KNEES_IN = 0.06                   # his knees turn in again from this long after he starts up
SHRUG_OUT = 16.0                  # his shoulders lifted while his elbows come over the rim (degrees; the duck's)
SCAN_WINDOW = (LOOK[0][0], LOOK[4][0])   # his eyes are checked against the rim between these (he looks round at the peek's height)
TURN_MOST, HOLD_LEAST, CREEP_MOST = 120.0, 0.25, 0.40   # the checks: head's turn (deg/s), hold at each side (s), eyes over the rim (m/s)
# (when, in the duck, s: a startled man's head and arms go before his legs let him fall)
FLINCH = (0.15, 2.0)              # his head down and shoulders up: done by, and how hard it starts
ARMS_IN = (0.20, 1.15)            # the glasses to his chest, elbows in
FALL_FROM = 0.09                  # his knees give from here: a drop before his elbows are in puts his forearms through the lip
FALL_POWER = 2.0                  # the fall's law: 2 is a steady pull (9.5 to 9.9 m/s2 for these men: about a free fall)
PULL_BACK = 0.06                  # he pulls back from the rim's front as he flinches (m)
CURL_FROM = 0.14                  # he curls forward and sits back from here (his shoulders are in the ring by then)
KNEES_OUT = (0.0, 0.2)            # his knees turn out as they fold (not before: a straight leg would only twist)

YAW_SHARE = {"spine_01": 0.11, "spine_02": 0.20, "chest": 0.23, "neck": 0.18, "head": 0.28}
TRUNK = ("pelvis", "spine_01", "spine_02", "chest", "neck", "head")
# The three poses the clips move between, as changes to commander_hatch's pose at 0 s. Degrees: + leans or nods
# forward. `drop`, `back`: his pelvis down and back from where it stands (m). `pit`: his hips and back; `nod`: his
# neck and head. `shrug`, `hunch`: his shoulders up, and forward. `chest`, `eyes`: where the glasses ride (the rest
# lies on the rim, as commander_hatch has them at 0 s), and `chest_r`, `eyes_r` the same for how they are turned.
# `hold`: 0 his hands over the top of the glasses (as on the rim), 1 under them (as commander_hatch raises them).
# `tuck`: his elbows from out wide on the rim (0) to in against him (1); `front`: tucked at his sides (0) or down in
# front of his chest (1). `knee`: knees out and forward. `lift`: the glasses lifted on their way (m).
STAND = {"drop": 0.0, "back": 0.0, "pit": {}, "nod": {}, "shrug": 0.0, "hunch": 0.0, "chest": 0.0, "eyes": 0.0,
         "chest_r": 0.0, "eyes_r": 0.0, "hold": 0.0, "tuck": 0.0, "front": 0.0, "knee": 0.0, "yaw": 0.0, "lift": 0.0}
CROUCH = dict(STAND, drop=None, back=0.36, pit={"pelvis": 4.0, "spine_01": 5.0, "spine_02": 9.0, "chest": 11.0},
              nod={"neck": 14.0, "head": 18.0}, shrug=16.0, hunch=8.0, chest=1.0, chest_r=1.0, hold=0.0, tuck=1.0, knee=1.0)
PEEKING = dict(STAND, drop=None, back=0.09, pit={"pelvis": -6.0, "spine_01": -3.0, "spine_02": -3.0, "chest": -3.0},
               nod={"neck": 4.0, "head": None}, shrug=14.0, hunch=8.0, eyes=1.0, eyes_r=1.0, hold=1.0, tuck=1.0,
               front=1.0, knee=1.0)
GLASSES_AT_CHEST = ((0.14, 0.0, 1.45), 35.0)       # (rest pose: where the eyepieces lie on his chest; barrels down, deg)
# (which way each elbow goes from the line shoulder to wrist: forward, out, up, in his chest's frame)
ELBOW_OVER = None                 # coming in off the rim: a way to go by (none: straight to the tuck)
ELBOW_SIDE = (0.08, 0.10, -1.0)   # tucked: down against his ribs
ELBOW_FRONT = (0.2, 0.0, -1.0)    # tucked: down in front of his chest (the glasses at his eyes)
KNEE_OUT = (1.0, 0.45, 0.0)       # the knee's way from the hip in the crouch (forward, out, up)
GAP = 0.015                       # the checks: fists on the glasses
BETWEEN = 120                     # the checks: samples a second between the keys
SEAM_DEG, SEAM_M = 0.5, 0.005
X_, Y_, Z_ = np.eye(3)


def unit(v):
    v = np.asarray(v, float)
    return v / max(float(np.linalg.norm(v)), 1e-12)


def sm(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def ramp(t, t0, t1):
    return sm((t - t0) / (t1 - t0))


def out_fast(x, p=2.0):
    """0 to 1 with its speed at the start (a flinch)."""
    x = min(max(x, 0.0), 1.0)
    return 1.0 - (1.0 - x) ** p


def sm_area(u):
    """The area under sm from 0 to u."""
    u = min(max(u, 0.0), 1.0)
    return u ** 3 - 0.5 * u ** 4


def plateau_area(x, a):
    """The area from 0 to x under a speed that rises (sm) over the first share a, stays 1, falls over the last a.
    Its whole area is 1 - a."""
    x = min(max(x, 0.0), 1.0)
    if x <= a:
        return a * sm_area(x / a)
    if x <= 1.0 - a:
        return a / 2.0 + (x - a)
    return (1.0 - a) - a * sm_area((1.0 - x) / a)


def pan(x, a=TURN_EASE):
    """0 to 1 at an even speed, easing in over the first share a and out over the last (its top speed is 1/(1-a) of
    the mean: a slow sweep, not a smoothstep's 1.5)."""
    return plateau_area(x, a) / (1.0 - a)


def rise_eyes(t, e0, top):
    """The peek's rise as his eyes' height over the rim (m) at t: e0 at rest at 0 s; up to the rim at RIM_AT, arriving
    at RISE_SPEED (fast: he is under it); on over it at RISE_SPEED, easing to a stop at `top` at PEEK_AT."""
    tr, v, tp, a = RIM_AT, RISE_SPEED, PEEK_AT, RISE_EASE
    if t <= 0.0:
        return e0
    if t <= tr:
        k = (-e0 - v * tr * 0.5) / (tr * (1.0 - a))       # (the bump's height that brings him to the rim on time)
        x = t / tr
        return e0 + tr * (k * plateau_area(x, a) + v * sm_area(x))
    td = 2.0 * (top / v + tr) - tp                      # (from here the creep eases to its stop)
    if t <= td:
        return v * (t - tr)
    T = tp - td
    x = min(t - td, T)
    return v * (td - tr) + v * (x - T * sm_area(x / T))


def about(R, axis):
    """The turn R, known to be about `axis`, as signed degrees."""
    q = fc.quat(R)
    return math.degrees(2.0 * math.atan2(float(q[1:] @ axis), float(q[0])))


def mix_turns(pairs):
    """Turns mixed by weights (through their quaternions)."""
    q0 = None
    acc = np.zeros(4)
    for w, R in pairs:
        if w <= 1e-9:
            continue
        q = fc.quat(R)
        if q0 is None:
            q0 = q
        if q @ q0 < 0:
            q = -q
        acc += w * q
    return gore.qmat(acc / np.linalg.norm(acc))


def mix(A, B, w):
    """Pose A to pose B by w (numbers, and the dicts of them)."""
    out = {}
    for k in A:
        if isinstance(A[k], dict):
            out[k] = {b: A[k].get(b, 0.0) + (B[k].get(b, 0.0) - A[k].get(b, 0.0)) * w for b in set(A[k]) | set(B[k])}
        else:
            out[k] = A[k] + (B[k] - A[k]) * w
    return out


def mix_by(A, B, ws):
    """As mix, each value by its own share (ws[key], or ws[""])."""
    out = {}
    for k in A:
        w = ws.get(k, ws.get("", 0.0))
        out[k] = mix({k: A[k]}, {k: B[k]}, w)[k]
    return out


# ================================================================================================ the hatches
def hatch_of(tank):
    """The commander's hatch as the tank's file has it: the socket, the rim's height and the ring's inside radius
    (the ring's own wall: the turret's points that face in toward the socket's axis, at the top)."""
    d = json.load(open(os.path.join(TANKS, tank + ".json")))
    sock = np.array(next(s for s in d["sockets"] if s["name"] == "commander")["at"], float)
    names = [p["name"] for p in d["parts"]]
    N = d["near"]
    P = np.array(N["points"], float).reshape(-1, 3)
    F = np.array(N["facings"], float).reshape(-1, 3)
    part = np.array(N["part"])
    pa = np.array(N["paints"])
    r = np.hypot(P[:, 0] - sock[0], P[:, 1] - sock[1])
    rad = np.stack([P[:, 0] - sock[0], P[:, 1] - sock[1]], 1) / np.maximum(r, 1e-9)[:, None]
    inward = (F[:, :2] * rad).sum(1) < -0.5
    sel = (part == names.index("turret")) & (r < 0.5) & (r > 0.15) & inward & (P[:, 2] > sock[2] + 0.1)
    rows = {}
    for z, rr in zip(P[sel, 2], r[sel]):
        rows.setdefault((round(float(z), 3), round(float(rr), 3)), 0)
        rows[(round(float(z), 3), round(float(rr), 3))] += 1
    walls = [k for k, n in rows.items() if n >= 12]           # (a ring of points, not a fitting's few)
    rim = max(z for z, _ in walls)
    ring = min(rr for z, rr in walls if abs(z - rim) < 1e-6)
    foot = min(z for z, rr in walls if abs(rr - ring) < 0.002)
    sides = rows[(rim, ring)]
    black = d["paint_names"].index("black")
    disc = (pa == black) & (r < ring) & (np.abs(P[:, 2] - rim) < 0.01) & (part == names.index("turret"))
    return {"tank": tank, "socket": [float(v) for v in sock], "rim": float(rim), "rim_over_socket": round(float(rim - sock[2]), 4),
            "ring": float(ring), "ring_sides": int(sides), "ring_flat": round(float(ring * math.cos(math.pi / sides)), 4),
            "wall_from": round(float(foot - rim), 4), "hole_disc": round(float(r[disc].max()), 4) if disc.any() else None}


# (under the lip the German cupolas widen into their drums: closed shells in the tanks' files, so tank_crew.py's
# measure of them is kept: from, to (m from the rim) and the radius. The Sherman's ring stands on her roof: no drum.)
DRUMS = {"PanzerIV": (-0.335, -0.045, 0.345), "Tiger": (-0.215, -0.035, 0.36)}
LIP = (-0.055, 0.005)             # the band of the ring's own wall, from the rim (commander_poses.py's)


# ================================================================================================ the man
class Rider:
    """One tank crewman: commander_hatch's pose at 0 s, the two ways his hands hold the glasses, and the solver."""

    def __init__(self, D):
        self.D = D
        man = self.man = fc.Man(D)
        base = self.base = D["clips"][BASE]
        self.lq0, self.mv0 = man.locals_of(base, 0.0)
        self.G0, self.X0 = man.fk(self.lq0, self.mv0)
        track = base["props"]["binoculars"]["track"]
        self.row0 = list(track[0])
        self.p0 = np.array(track[0][1:4], float)
        self.R0 = gore.qmat(np.array(track[0][4:8], float))
        # (his hands on the glasses: low, as they lie on the rim, and high, as commander_hatch has them at his eyes)
        up = max(track, key=lambda r: r[3])
        Gu, Xu = gore.pose(man.rig, base, up[0])
        pu, Ru = np.array(up[1:4], float), gore.qmat(np.array(up[4:8], float))
        hd = man.b("head")
        self.eyes_at = (Gu[hd].T @ (pu - Xu[hd]), Gu[hd].T @ Ru)        # the glasses at his eyes, in his head's frame
        self.raised_at = float(up[0])
        self.hold, self.grip_ring = {}, {}
        for s in ("l", "r"):
            h = man.b("hand_" + s)
            lo_p, lo_R = self.R0.T @ (self.X0[h] - self.p0), self.R0.T @ self.G0[h]
            hi_p, hi_R = Ru.T @ (Xu[h] - pu), Ru.T @ Gu[h]
            d = hi_R @ lo_R.T                                           # (a turn about the barrel under his hand)
            q = fc.quat(d)
            if q[0] < 0:
                q = -q
            ax = unit(q[1:])
            ang = math.degrees(2.0 * math.acos(min(1.0, q[0])))
            # (the barrel's middle line: the point the turn leaves still, solved across the axis)
            e1 = unit(np.cross(ax, Z_))
            e2 = np.cross(ax, e1)
            E = np.stack([e1, e2])
            c = E.T @ np.linalg.solve(np.eye(2) - E @ d @ E.T, E @ (hi_p - d @ lo_p))
            slip = hi_p - (c + d @ (lo_p - c))
            self.hold[s] = {"p": lo_p, "R": lo_R, "axis": ax, "deg": ang, "c": c, "slip": slip}
            g = self.R0.T @ (man.grip(self.G0, self.X0, s) - self.p0)
            self.grip_ring[s] = {"x": float(g[0]), "c": c[1:].copy(), "r": float(np.linalg.norm(g[1:] - c[1:]))}   # (the axis is the barrels' own, x)
        # (the legs as they stand: the solver turns them from here, so the standing pose comes back exactly)
        self.leg = {}
        for s in ("l", "r"):
            th, ca, fo = man.b("thigh_" + s), man.b("calf_" + s), man.b("foot_" + s)
            up0, lo0 = self.X0[ca] - self.X0[th], self.X0[fo] - self.X0[ca]
            u0 = unit(self.X0[fo] - self.X0[th])
            self.leg[s] = {"up": up0, "lo": lo0, "bend": unit(np.cross(up0, lo0)), "l1": float(np.linalg.norm(up0)),
                           "l2": float(np.linalg.norm(lo0)), "ankle": self.X0[fo].copy(),
                           "pole": unit(up0 - u0 * (up0 @ u0))}       # (the way his knee stands from the line hip to ankle)
        # (the arms: how the standing pose's arm bones are twisted about themselves, against this solver's own way)
        self.twist = {}
        for s in ("l", "r"):
            ua, la, ha = man.b("upperarm_" + s), man.b("lowerarm_" + s), man.b("hand_" + s)
            Gu_, Gl_, K, Tc = self._arm(self.G0, self.X0, s, self.X0[ha], self.G0[ha], self.X0[la] - self.X0[ua])
            self.twist[s] = (about(self.G0[ua] @ Gu_.T, unit(K - self.X0[ua])), about(self.G0[la] @ Gl_.T, unit(Tc - K)))
        # (his body's points, for the measures)
        M = D["near"]
        self.P = np.array(M["points"], float).reshape(-1, 3)
        self.a, self.b2, self.w2 = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
        gun = [man.b(n) for n in ("weapon", "bolt", "bipod") if n in man.ix]
        self.body = ~np.isin(self.a, gun)
        iris = D.get("new_paints", {}).get("iris")
        self.iris = (np.array(M["paints"]) == iris) & self.body
        self.on_head = (self.a == hd) & self.body
        pc, tilt = GLASSES_AT_CHEST
        ch = man.b("chest")
        self.chest_at = (np.array(pc, float) - man.H[ch], fc.R_axis(Y_, tilt))
        self.glass = glasses_points()

    # ---- measures
    def skinned(self, G, X):
        H = self.man.H
        Wa = X[self.a] + np.einsum("nij,nj->ni", G[self.a], self.P - H[self.a])
        Wb = X[self.b2] + np.einsum("nij,nj->ni", G[self.b2], self.P - H[self.b2])
        return Wa * (1 - self.w2)[:, None] + Wb * self.w2[:, None]

    # ---- the solver
    def _arm(self, G, X, s, T, Gh, pole, share=0.5):
        """field_clips.Man.arm's two-bone solve, giving the bones' turns (so their twists can be set after)."""
        A = self.man.side[s]
        S = X[self.man.b("upperarm_" + s)]
        l1, l2 = A["l1"], A["l2"]
        dv = np.asarray(T, float) - S
        dist = float(np.linalg.norm(dv))
        dc = min(max(dist, abs(l1 - l2) + 1e-4), (l1 + l2) * 0.999)
        u = dv / max(dist, 1e-9)
        v = unit(np.asarray(pole, float) - u * (np.asarray(pole, float) @ u))
        ca = (l1 * l1 + dc * dc - l2 * l2) / (2 * l1 * dc)
        K = S + l1 * (u * ca + v * math.sqrt(max(0.0, 1 - ca * ca)))
        Tc = S + u * dc
        up, lo = K - S, Tc - K
        bend = np.cross(up, lo)
        if np.linalg.norm(bend) < 1e-6:
            bend = np.cross(up, np.asarray(pole, float))
        Gu = fc.frame_map(A["u0"], A["bend0"], up, unit(bend))
        Gl = fc.frame_map(A["l0"], A["bend0"], lo, unit(bend))
        la_ = unit(lo)
        tf, th_ = Gl @ A["t0"], Gh @ A["t0"]
        tf, th_ = unit(tf - la_ * (tf @ la_)), unit(th_ - la_ * (th_ @ la_))
        phi = math.degrees(math.atan2(np.cross(tf, th_) @ la_, tf @ th_))
        Gl = fc.R_axis(la_, share * phi) @ Gl
        return Gu, Gl, K, Tc

    def _turn(self, lq, mv, bone, R):
        """Bone (and all it carries) turned by R in the root's frame, about its own joint (commander_poses.py's)."""
        G, _ = self.man.fk(lq, mv)
        i = self.man.b(bone)
        p = self.man.par[i]
        lq[i] = G[p].T @ R @ G[p] @ lq[i]
        return lq

    def hand_on(self, s, h):
        """His hand on the glasses, in the glasses' frame: the wrist and the hand's turn, `h` of the way from the
        low hold round the barrel to the high one."""
        Hd = self.hold[s]
        T = fc.R_axis(Hd["axis"], Hd["deg"] * h)
        return Hd["c"] + T @ (Hd["p"] - Hd["c"]) + Hd["slip"] * h, T @ Hd["R"]

    def pose(self, S):
        """The pose S (see STAND) on him: each bone's local turn and move, the glasses' place and turn, and how far
        each wrist is from where the glasses want it."""
        man = self.man
        lq, mv = self.lq0.copy(), self.mv0.copy()
        pel = man.b("pelvis")
        mv[pel] = self.mv0[pel] + np.array((-S["back"], 0.0, -S["drop"]))
        turned = 0.0
        for b in TRUNK:
            pit = S["pit"].get(b, 0.0) + S["nod"].get(b, 0.0)
            yaw = S["yaw"] * YAW_SHARE.get(b, 0.0)
            if abs(pit) > 1e-9:
                lq = self._turn(lq, mv, b, fc.R_axis(fc.R_axis(Z_, turned) @ Y_, pit))
            if abs(yaw) > 1e-9:
                lq = self._turn(lq, mv, b, fc.R_axis(Z_, yaw))
                turned += yaw
        for s, sg in (("l", 1.0), ("r", -1.0)):
            c = man.b("clavicle_" + s)
            lq[c] = fc.R_axis(Z_, sg * S["hunch"]) @ fc.R_axis(X_, -sg * S["shrug"]) @ self.lq0[c]
        G, X = man.fk(lq, mv)
        # (the legs: his ankles where they stand, the knees folding forward and out)
        for s, sg in (("l", -1.0), ("r", 1.0)):
            L = self.leg[s]
            th, ca, fo = man.b("thigh_" + s), man.b("calf_" + s), man.b("foot_" + s)
            hip = X[th]
            dv = L["ankle"] - hip
            dist = float(np.linalg.norm(dv))
            dc = min(max(dist, abs(L["l1"] - L["l2"]) + 1e-4), (L["l1"] + L["l2"]) * 0.9999)
            u = dv / dist
            pole = L["pole"] * (1.0 - S["knee"]) + unit((KNEE_OUT[0], sg * KNEE_OUT[1], KNEE_OUT[2])) * S["knee"]
            v = unit(pole - u * (pole @ u))
            cs = (L["l1"] ** 2 + dc * dc - L["l2"] ** 2) / (2 * L["l1"] * dc)
            K = hip + L["l1"] * (u * cs + v * math.sqrt(max(0.0, 1 - cs * cs)))
            up, lo = K - hip, hip + u * dc - K
            bend = unit(np.cross(up, lo))
            Gt = fc.frame_map(L["up"], L["bend"], up, bend) @ self.G0[th]
            Gc = fc.frame_map(L["lo"], L["bend"], lo, bend) @ self.G0[ca]
            lq[th] = G[man.par[th]].T @ Gt
            lq[ca] = Gt.T @ Gc
            lq[fo] = Gc.T @ self.G0[fo]                 # (the boot flat on the floor, as it stands)
        # (the glasses: on the rim, on his chest, at his eyes)
        ch, hd = man.b("chest"), man.b("head")
        at = [(self.p0, self.R0), (X[ch] + G[ch] @ self.chest_at[0], G[ch] @ self.chest_at[1]),
              (X[hd] + G[hd] @ self.eyes_at[0], G[hd] @ self.eyes_at[1])]
        wp = (1.0 - S["chest"] - S["eyes"], S["chest"], S["eyes"])
        wr = (1.0 - S["chest_r"] - S["eyes_r"], S["chest_r"], S["eyes_r"])
        p = sum(w * q for w, (q, _) in zip(wp, at)) + Z_ * S["lift"]
        R = mix_turns([(w, r) for w, (_, r) in zip(wr, at)])
        # (the arms to them: the elbows from where they stand on the rim, in over it, then down against him)
        Gy = fc.R_axis(Z_, S["yaw"] * (YAW_SHARE["spine_01"] + YAW_SHARE["spine_02"] + YAW_SHARE["chest"]))
        miss = {}
        for s, sg in (("l", -1.0), ("r", 1.0)):
            ua, la, ha = man.b("upperarm_" + s), man.b("lowerarm_" + s), man.b("hand_" + s)
            wp_, wR = self.hand_on(s, S["hold"])
            T, Gh = p + R @ wp_, R @ wR
            k = S["tuck"]
            side = lambda v: G[ch] @ unit((v[0], sg * v[1], v[2]))
            e0 = Gy @ unit(self.X0[la] - self.X0[ua])
            e1 = side(ELBOW_OVER) if ELBOW_OVER else None
            e2 = side(ELBOW_SIDE) * (1.0 - S["front"]) + side(ELBOW_FRONT) * S["front"]
            # (a curve through the three: off the rim inward first, down after)
            pole = e0 * (1 - k) ** 2 + e1 * 2 * k * (1 - k) + e2 * k * k if ELBOW_OVER else e0 * (1 - k) + e2 * k
            Gu, Gl, K, Tc = self._arm(G, X, s, T, Gh, pole)
            f = (1.0 - k) ** 2
            Gu = fc.R_axis(unit(K - X[ua]), f * self.twist[s][0]) @ Gu
            Gl = fc.R_axis(unit(Tc - K), f * self.twist[s][1]) @ Gl
            lq[ua] = G[man.par[ua]].T @ Gu
            lq[la] = Gu.T @ Gl
            lq[ha] = Gl.T @ Gh
            miss[s] = float(np.linalg.norm(Tc - T))
        return lq, mv, (p, R), miss

    def top_of(self, S, what="all"):
        """How high he reaches in pose S: all of him and the glasses, or his eyes (m over his feet)."""
        lq, mv, (p, R), _ = self.pose(S)
        G, X = self.man.fk(lq, mv)
        W = self.skinned(G, X)
        if what == "eyes":
            return float(W[self.iris, 2].mean())
        return max(float(W[self.body, 2].max()), float((self.glass @ R.T + p)[:, 2].max()))

    def lower_to(self, S, height, what):
        """The pose S with its `drop` set so that he reaches just `height`."""
        lo, hi = 0.0, 0.75
        for _ in range(40):
            mid = 0.5 * (lo + hi)
            if self.top_of(dict(S, drop=mid), what) > height:
                lo = mid
            else:
                hi = mid
        return dict(S, drop=round(0.5 * (lo + hi), 4))

    def level_eyes(self, S):
        """The pose S with his head nodded so that the glasses at his eyes look level."""
        S = dict(S, nod=dict(S["nod"], head=0.0))
        for _ in range(6):
            _, _, (p, R), _ = self.pose(dict(S, drop=0.3))
            S["nod"]["head"] += math.degrees(math.asin(max(-1.0, min(1.0, float(R[2, 0])))))
        S["nod"]["head"] = round(S["nod"]["head"], 3)
        return S


_GLASS = []


def glasses_points():
    """The glasses' own points (Content/Models/Battle/Binoculars.json; a box their size if it is not there)."""
    if not _GLASS:
        try:
            B = json.load(open(os.path.join(TANKS, "Binoculars.json")))
            _GLASS.append(np.array(B["near"]["points"], float).reshape(-1, 3))
        except OSError:
            lo, hi = (-0.005, -0.0884, -0.0304), (0.155, 0.0884, 0.0304)
            _GLASS.append(np.array([(x, y, z) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])], float))
    return _GLASS[0]


# ================================================================================================ the two clips
def poses_of(rider, rim):
    """The crouch under the rim and the peek over it, fitted to this man (the rim is `rim` over his feet)."""
    crouch = rider.lower_to(dict(CROUCH, drop=0.0), rim - UNDER, "all")
    peek = rider.level_eyes(dict(PEEKING, drop=0.0))
    peek = rider.lower_to(peek, rim + EYES_OVER, "eyes")
    return crouch, peek


def duck_at(t, crouch):
    """The duck: frightened, so it starts at once. His head and shoulders go first and his arms snap in off the rim
    to his chest; he falls through the ring with his knees folding, sitting back as he curls; no slowing at the end
    (he is hidden there)."""
    flinch = out_fast(t / FLINCH[0], FLINCH[1])
    arms = out_fast(t / ARMS_IN[0], ARMS_IN[1])
    fall = max(0.0, (t - FALL_FROM) / (DUCK_LONG - FALL_FROM)) ** FALL_POWER
    curl = max(0.0, (t - CURL_FROM) / (DUCK_LONG - CURL_FROM)) ** 1.6
    S = mix_by(STAND, crouch, {"": flinch, "drop": fall, "back": curl, "pit": curl, "knee": ramp(t, *KNEES_OUT),
                               "chest": arms, "chest_r": arms ** 2, "hold": arms, "tuck": arms})
    S["back"] += PULL_BACK * flinch * (1.0 - curl)
    S["lift"] = 0.03 * 4.0 * arms * (1.0 - arms)
    return S


def look_at(t):
    for (t0, a), (t1, b) in zip(LOOK[:-1], LOOK[1:]):
        if t0 <= t <= t1:
            return SCAN * (a + (b - a) * pan((t - t0) / (t1 - t0)))
    return 0.0


class Rise:
    """The peek's rise for one man: how high his eyes are (over the rim) at each share of the blend from the crouch to
    the peek, so that the blend can be solved to the eyes' height rise_eyes lays down."""

    def __init__(self, rider, crouch, peek, rim, n=200):
        self.ws = np.linspace(0.0, 1.0, n + 1)
        self.es = np.array([rider.top_of(mix_by(crouch, peek, {"": float(w)}), "eyes") - rim for w in self.ws])
        if not np.all(np.diff(self.es) > 0):
            raise ValueError("his eyes do not rise all the way from the crouch to the peek")
        self.e0, self.e1 = float(self.es[0]), float(self.es[-1])

    def up(self, t):
        """The blend from the crouch (0) to the peek (1) at t."""
        if t <= 0.0:
            return 0.0
        if t >= PEEK_AT:
            return 1.0
        return float(np.interp(rise_eyes(t, self.e0, self.e1), self.es, self.ws))


def peek_at(t, crouch, peek, rise):
    """The peek: up from the crouch, quick under the rim and a creep over it, to his eyes over the rim, the glasses to
    his eyes; he looks round to his left, then his right (a slow pan, held at each side); then up and out to the
    standing pose, the glasses back down on the rim."""
    up = rise.up(t)
    g = ramp(t, *GLASSES_UP)
    S = mix_by(crouch, peek, {"": up, "chest": g, "eyes": g, "chest_r": g, "eyes_r": g, "front": g,
                              "hold": ramp(t, GLASSES_UP[0] + 0.04, GLASSES_UP[1] - 0.04)})
    out = ramp(t, *OUT)
    if out > 0.0:
        down = ramp(t, *GLASSES_DOWN)
        flare = ramp(t, *ELBOWS_OUT)
        lean = ramp(t, *LEAN_OUT)
        S = mix_by(S, STAND, {"": out, "eyes": down, "eyes_r": down, "hold": ramp(t, GLASSES_DOWN[0] + 0.02, GLASSES_DOWN[1] - 0.02),
                              "tuck": flare, "front": flare, "hunch": flare, "back": lean, "pit": lean, "nod": lean,
                              "knee": ramp(t, OUT[0] + KNEES_IN, OUT[1])})
        # (his shoulders: lifted as he rises, so that his elbows clear the rim on their way out; let down last)
        S["shrug"] = (peek["shrug"] + (SHRUG_OUT - peek["shrug"]) * ramp(t, OUT[0], ELBOWS_OUT[0])) * (1.0 - ramp(t, ELBOWS_OUT[0] + SHRUG_DOWN, PEEK_LONG))
    S["yaw"] = look_at(t)
    return S


def key_times(long):
    n = int(round(long * RATE))
    return [round(k / RATE, 4) for k in range(n)] + [round(long, 4)]


def prop_row(t, p, R, shown):
    q = fc.quat(R)
    if q[0] < 0:
        q = -q
    return [round(t, 4)] + [round(float(v), 4) + 0.0 for v in p] + [round(float(v), 5) + 0.0 for v in q] + [1 if shown else 0]


def bake(rider, fn, long, first=None, last=None):
    """The clip's tracks and the glasses' track from the pose at each key. `first`, `last`: (tracks, row) whose keys
    this clip's first or last key copies exactly (the pose it starts in or ends in)."""
    man = rider.man
    ts = key_times(long)
    bones = [tr["part"] for tr in rider.base["tracks"]]
    rot, mvs, rows, worst = {i: [] for i in bones}, {i: [] for i in bones}, [], 0.0
    for t in ts:
        lq, mv, (p, R), miss = fn(t)
        worst = max(worst, max(miss.values()))
        for i in bones:
            rot[i].append(fc.quat(lq[i]))
            mvs[i].append(mv[i].copy())
        rows.append(prop_row(t, p, R, True))
    base_keys = {tr["part"]: tr for tr in rider.base["tracks"]}
    tracks = []
    for i in bones:
        tr = {"part": i, "part_name": man.rig.names[i]}
        old = base_keys[i]
        if "rot" in old:
            qs = [np.array([round(float(v), 5) for v in q]) for q in rot[i]]
            if first is not None:
                qs[0] = np.array(first[0][i]["rot"], float)
            if last is not None:
                qs[-1] = np.array(last[0][i]["rot"], float)
            # (each key on the same side as its neighbour, counted from the key that is copied)
            order = range(len(qs) - 2, -1, -1) if last is not None else range(1, len(qs))
            step = 1 if last is not None else -1
            for k in order:
                if float(qs[k] @ qs[k + step]) < 0:
                    qs[k] = -qs[k]
            tr["rot"] = [[t] + [float(v) + 0.0 for v in q] for t, q in zip(ts, qs)]
        ms = [[round(float(v), 4) + 0.0 for v in m] for m in mvs[i]]
        if "move" in old or any(max(abs(v) for v in m) > 1e-4 for m in ms):
            if first is not None and "move" in first[0][i]:
                ms[0] = list(first[0][i]["move"])
            if last is not None and "move" in last[0][i]:
                ms[-1] = list(last[0][i]["move"])
            tr["move"] = [[t] + m for t, m in zip(ts, ms)]
        tracks.append(tr)
    if first is not None:
        rows[0] = [ts[0]] + list(first[1][1:8]) + [1]
    if last is not None:
        rows[-1] = [ts[-1]] + list(last[1][1:8]) + [1]
    return tracks, rows, worst


def keys_at(clip, t, row=None):
    """A clip's keys at one of its key times, by bone ({part: {"rot": [...], "move": [...]}}), and its glasses' row."""
    out = {}
    for tr in clip["tracks"]:
        e = {}
        for kind in ("rot", "move"):
            if kind in tr:
                k = min(tr[kind], key=lambda r: abs(r[0] - t))
                if abs(k[0] - t) > 1e-6:
                    raise ValueError(f"no key at {t}")
                e[kind] = list(k[1:])
        out[tr["part"]] = e
    if row is None:
        row = min(clip["props"]["binoculars"]["track"], key=lambda r: abs(r[0] - t))
    return out, list(row)


def clips_for(D, hatches, log=print):
    """The two clips for one file (and the fitted poses, for the report)."""
    rider = Rider(D)
    base = rider.base
    rim = hatches[0]["rim_over_feet"]
    crouch, peek = poses_of(rider, rim)
    stand = keys_at(base, 0.0, rider.row0)
    d_tracks, d_rows, d_miss = bake(rider, lambda t: rider.pose(duck_at(t, crouch)), DUCK_LONG, first=stand)
    d_rows[-1][8] = 0
    prop = {k: v for k, v in base["props"]["binoculars"].items() if k != "track"}
    duck = {"long": DUCK_LONG, "loop": False, "stride": 0.0, "weapon": base["weapon"], "tracks": d_tracks,
            "events": [{"t": DUCK_LONG, "fx": "hide", "socket": None, "cue": ""}], "anchor": base["anchor"],
            "about": "The commander ducks into his turret (the tank's `buttoned` turning true: a round has passed him, or the "
                     "enemy is near). From commander_hatch's pose at 0 s (`start`: blend to it from wherever he was), fast from "
                     f"the first key: his head goes down between his shoulders and his arms snap in off the rim ({ARMS_IN[0]} s), "
                     f"the glasses up against his chest and his elbows against his ribs; his knees give ({FALL_FROM} s) and he "
                     "falls straight down through the ring, curling forward, with no slowing. The event `hide` at "
                     f"the last key ({DUCK_LONG} s): the top of his head is {UNDER * 100:.0f} cm under the rim there; stop drawing "
                     "him, and the glasses (`shown` 0 at that key). The root (the floor at his feet) does not move: the "
                     "pelvis' move keys lower him.",
            "from": "commander_more.py: keyed on this man from commander_hatch's pose at 0 s",
            "places": json.loads(json.dumps(base["places"])), "start": [BASE, 0.0],
            "props": {"binoculars": dict(prop, track=d_rows)}}
    d_end = keys_at(duck, DUCK_LONG)
    rise = Rise(rider, crouch, peek, rim)
    p_tracks, p_rows, p_miss = bake(rider, lambda t: rider.pose(peek_at(t, crouch, peek, rise)), PEEK_LONG, first=d_end, last=stand)
    peek_clip = {"long": PEEK_LONG, "loop": False, "stride": 0.0, "weapon": base["weapon"], "tracks": p_tracks,
                 "events": [{"t": 0.0, "fx": "show", "socket": None, "cue": ""}], "anchor": base["anchor"],
                 "about": "The commander comes back up, carefully (the tank's `buttoned` turning false). From "
                          "commander_hatch_duck's last pose (`start`), under the rim: the event `show` at the first key (draw "
                          f"him again). Up quickly while his eyes are under the rim (to the rim at {RIM_AT} s), then his eyes "
                          f"creep over it ({RISE_SPEED} m/s at most) and ease to a stop {EYES_OVER * 100:.0f} cm over it ({PEEK_AT} s; "
                          "his head and the tops of his shoulders out), the glasses to his eyes. He looks round in a slow, even "
                          f"pan eased at both ends: {SCAN:.0f} degrees to his left ({LOOK[1][0]} to {LOOK[2][0]} s, held), across to "
                          f"{SCAN:.0f} degrees to his right ({LOOK[3][0]} to {LOOK[4][0]} s, held), his head and chest together, the "
                          f"glasses on his eyes; then faces ahead and stands up (from {OUT[0]} s), his elbows out over the rim "
                          f"({ELBOWS_OUT[0]} s) and the glasses back down on it, to commander_hatch's pose at 0 s exactly at the "
                          f"last key ({PEEK_LONG} s): play commander_hatch from 0 s after it, with no blend.",
                 "from": "commander_more.py: keyed on this man from commander_hatch_duck's last pose to commander_hatch's at 0 s",
                 "places": json.loads(json.dumps(base["places"])), "start": [DUCK, DUCK_LONG],
                 "props": {"binoculars": dict(prop, track=p_rows)}}
    log(f"   fitted: the crouch lowers his pelvis {crouch['drop']:.3f} m, the peek {peek['drop']:.3f} m (head nodded "
        f"{peek['nod']['head']:+.1f}); a wrist off the glasses by {max(d_miss, p_miss) * 1000:.1f} mm at most; the standing "
        f"arms' own twists {', '.join(f'{a:+.1f}/{b:+.1f}' for a, b in rider.twist.values())} degrees")
    fit = {"crouch_drop_m": crouch["drop"], "peek_drop_m": peek["drop"], "peek_head_nod_deg": peek["nod"]["head"],
           "wrist_miss_m": round(max(d_miss, p_miss), 5), "glasses_raised_at_s": rider.raised_at,
           "peek_eyes_from_m": round(rise.e0, 4), "peek_eyes_to_m": round(rise.e1, 4)}
    return {DUCK: duck, PEEK: peek_clip}, fit


# ================================================================================================ the checks
def turn_between(a, b):
    d = abs(float(np.array(a, float) @ np.array(b, float)))
    return math.degrees(2.0 * math.acos(min(1.0, d / (np.linalg.norm(a) * np.linalg.norm(b)))))


def seam(clip_a, ta, clip_b, tb):
    """The largest difference between clip_a at ta and clip_b at tb: any bone's turn (degrees, local), any bone's
    place (m, in the root's frame), the glasses' place (m) and turn (degrees)."""
    ka, ra = keys_at(clip_a, ta)
    kb, rb = keys_at(clip_b, tb)
    worst, who = 0.0, None
    for i in set(ka) | set(kb):
        qa = ka.get(i, {}).get("rot", [1.0, 0.0, 0.0, 0.0])
        qb = kb.get(i, {}).get("rot", [1.0, 0.0, 0.0, 0.0])
        d = turn_between(qa, qb)
        if d > worst:
            worst, who = d, i
    return worst, who, float(np.linalg.norm(np.array(ra[1:4]) - np.array(rb[1:4]))), turn_between(ra[4:8], rb[4:8])


def glasses_at(clip, t):
    """The glasses' row at any time: between two keys their place goes straight and their turn the short way; they
    are shown while the key before says so."""
    tr = clip["props"]["binoculars"]["track"]
    k = min(max(int(math.floor(t * RATE + 1e-6)), 0), len(tr) - 1)
    a = tr[k]
    b = tr[min(k + 1, len(tr) - 1)]
    f = 0.0 if b[0] <= a[0] else min(max((t - a[0]) / (b[0] - a[0]), 0.0), 1.0)
    if f < 1e-6:
        return np.array(a[1:4], float), gore.qmat(np.array(a[4:8], float)), bool(a[8])
    qa, qb = np.array(a[4:8], float), np.array(b[4:8], float)
    if qa @ qb < 0:
        qb = -qb
    q = qa + (qb - qa) * f
    return np.array(a[1:4], float) + (np.array(b[1:4], float) - np.array(a[1:4], float)) * f, gore.qmat(q / np.linalg.norm(q)), bool(a[8])


def check_clip(rider, name, clip, hatch, tol, log=print):
    """One clip at one tank, read as it is written: his head against the rim over time, his body (and the glasses)
    against the ring at every key and BETWEEN the keys (the clip sampled `BETWEEN` a second, as the game plays it), his
    fists on the glasses, the fastest joint."""
    man = rider.man
    rim, ring = hatch["rim_over_feet"], hatch["ring"]
    off = np.array(hatch["root_offset"], float)
    ts = [k[0] for k in next(tr for tr in clip["tracks"] if "rot" in tr)["rot"]]
    bands = [(rim + LIP[0], rim + LIP[1], ring)]
    if hatch["tank"] in DRUMS:
        a, b, r = DRUMS[hatch["tank"]]
        bands.append((rim + a, rim + min(b, LIP[0]), r))

    def measure(t):
        G, X = gore.pose(man.rig, clip, t)
        W = rider.skinned(G, X)
        r = np.hypot(W[:, 0] + off[0], W[:, 1] + off[1])         # (from the socket's axis)
        fwd = G[man.b("head")] @ X_
        m = {"head": float(W[rider.on_head, 2].max() - rim), "eyes": float(W[rider.iris, 2].mean() - rim),
             "top": float(W[rider.body, 2].max() - rim), "lip": None, "drum": None, "glass": None, "glass_top": None, "gap": None,
             "yaw": math.degrees(math.atan2(float(fwd[1]), float(fwd[0])))}
        for k, (z0, z1, rr) in enumerate(bands):
            band = rider.body & (W[:, 2] > z0) & (W[:, 2] < z1)
            if band.any():
                d = r[band] - rr
                j = int(np.argmax(d))
                m["lip" if k == 0 else "drum"] = (float(d[j]), man.rig.names[int(rider.a[band][j])], int((d > 0.012).sum()))
        p, R, shown = glasses_at(clip, t)
        if shown:
            Pg = rider.glass @ R.T + p
            rg = np.hypot(Pg[:, 0] + off[0], Pg[:, 1] + off[1])
            m["glass_top"] = float(Pg[:, 2].max() - rim)
            band = (Pg[:, 2] > bands[0][0]) & (Pg[:, 2] < bands[0][1])
            if band.any():
                m["glass"] = float((rg[band] - ring).max())
            gaps = []
            for s in ("l", "r"):
                g = R.T @ (man.grip(G, X, s) - p)
                gr = rider.grip_ring[s]
                gaps.append((math.hypot(g[0] - gr["x"], float(np.linalg.norm(g[1:] - gr["c"])) - gr["r"]), s))
            m["gap"] = max(gaps)
        return m

    def worst(ms, key):
        best = None
        for t, m in ms:
            v = m[key]
            if v is not None and (best is None or (v[0] if isinstance(v, tuple) else v) > (best[1][0] if isinstance(best[1], tuple) else best[1])):
                best = (t, v)
        return best

    at_keys = [(t, measure(t)) for t in ts]
    n = int(round(clip["long"] * BETWEEN))
    between = [(round(k / BETWEEN, 5), measure(k / BETWEEN)) for k in range(n + 1)]
    head = [[t, round(m["head"], 4)] for t, m in at_keys]
    eyes = [[t, round(m["eyes"], 4)] for t, m in at_keys]
    tops = [[t, round(m["top"], 4)] + ([round(m["glass_top"], 4)] if m["glass_top"] is not None else []) for t, m in at_keys]

    def ring_of(ms):
        lip, drum, glass = worst(ms, "lip"), worst(ms, "drum"), worst(ms, "glass")
        return {"deepest_m": round(lip[1][0], 4) if lip else None, "where": [lip[0], lip[1][1]] if lip else None,
                "points_past_1_2cm": max([m["lip"][2] for _, m in ms if m["lip"] is not None] or [0]),
                "glasses_deepest_m": round(glass[1], 4) if glass else None,
                "drum_deepest_m": round(drum[1][0], 4) if drum else None, "drum_where": [drum[0], drum[1][1]] if drum else None}

    gap_k, gap_b = worst(at_keys, "gap"), worst(between, "gap")
    fastest = {"deg_s": 0.0, "bone": None, "t": None}
    for tr in clip["tracks"]:
        if "rot" not in tr:
            continue
        for ka, kb in zip(tr["rot"][:-1], tr["rot"][1:]):
            w = turn_between(ka[1:], kb[1:]) / (kb[0] - ka[0])
            if w > fastest["deg_s"]:
                fastest.update(deg_s=round(w, 1), bone=tr["part_name"], t=ka[0])
    speed = max(abs(b[1] - a[1]) / (b[0] - a[0]) for a, b in zip(head[:-1], head[1:]))
    # (how he looks round: his head's heading (degrees, + to his right: atan2 of its forward in the root's frame),
    # how fast it turns from key to key, and how long it stays within 1 degree of each side)
    yaw = [[t, round(m["yaw"], 2)] for t, m in at_keys]
    rates = [(abs(b[1] - a[1]) / (b[0] - a[0]), a[0]) for a, b in zip(yaw[:-1], yaw[1:])]
    lo, hi = min(y for _, y in yaw), max(y for _, y in yaw)

    def held(side):
        best, run = 0.0, None
        for t, y in yaw:
            if (y <= lo + 1.0) if side < 0 else (y >= hi - 1.0):
                run = t if run is None else run
                best = max(best, t - run)
            else:
                run = None
        return round(best, 4)
    res = {"at": hatch["tank"] + " commander", "keys": len(ts), "head_top_over_rim_m": head, "eyes_over_rim_m": eyes,
           "highest_over_rim_m": tops, "head_top_fastest_m_s": round(speed, 2),
           "ring": dict(ring_of(at_keys), tolerance_m=tol, between_keys=dict(ring_of(between), samples_a_second=BETWEEN)),
           "fists": {"gap_m": round(gap_k[1][0], 5), "where": [gap_k[0], gap_k[1][1]], "limit_m": GAP,
                     "between_keys_gap_m": round(gap_b[1][0], 5), "between_keys_where": [gap_b[0], gap_b[1][1]]},
           "fastest_turn": fastest,
           "head_yaw": {"deg_by_key": yaw, "least_most_deg": [lo, hi], "fastest_deg_s": [round(max(rates)[0], 1), max(rates)[1]],
                        "held_within_1deg_s": {"left": held(-1), "right": held(1)}}}
    if name == PEEK:
        # (the rise: when his eyes reach the rim and 8 cm over it, how fast they go from the rim up to the scan, and
        # how fast while they are still under it)
        e = [v for _, v in eyes]
        top_k = max(range(len(e)), key=lambda k: e[k])
        k0 = next(k for k in range(top_k + 1) if e[k] >= 0.0)
        k8 = next(k for k in range(top_k + 1) if e[k] >= 0.08 - 1e-9)
        when = lambda k, lv: round(ts[k - 1] + (ts[k] - ts[k - 1]) * (lv - e[k - 1]) / (e[k] - e[k - 1]), 4) if k > 0 else ts[0]
        over = [((e[k + 1] - e[k]) / (ts[k + 1] - ts[k]), ts[k]) for k in range(len(e) - 1) if e[k] >= 0.0 and ts[k] < SCAN_WINDOW[1] - 1e-6]
        under = [((e[k + 1] - e[k]) / (ts[k + 1] - ts[k]), ts[k]) for k in range(len(e) - 1) if e[k] < 0.0 and ts[k] < SCAN_WINDOW[0]]
        res["rise"] = {"eyes_at_rim_s": when(k0, 0.0), "eyes_8cm_over_s": when(k8, 0.08),
                       "eyes_fastest_over_rim_m_s": [round(max(over)[0], 3), max(over)[1]],
                       "eyes_fastest_under_rim_m_s": [round(max(under)[0], 3), max(under)[1]],
                       "eyes_first_keys_cm": [[t, round(v * 100, 2)] for t, v in eyes if t <= PEEK_AT + 0.05]}
    return res


def faults_of(name, res, log=print):
    """What a clip's measures at one tank fail, in words."""
    out = []
    at = res["at"]
    head, eyes, tops = res["head_top_over_rim_m"], res["eyes_over_rim_m"], res["highest_over_rim_m"]
    if name == DUCK:
        end = max(max(tops[-1][1:]), head[-1][1])
        if end > -0.05 + 1e-6:
            out.append(f"{name} at {at}: he ends {end * 100:+.1f} cm from the rim (wanted 5 cm under it or more)")
        if head[0][1] < 0.3:
            out.append(f"{name} at {at}: he does not start stood up")
    else:
        if max(max(tops[0][1:]), head[0][1]) > -0.05 + 1e-6:
            out.append(f"{name} at {at}: he does not start under the rim")
        scan = [e[1] for e in eyes if SCAN_WINDOW[0] - 1e-6 <= e[0] <= SCAN_WINDOW[1] + 1e-6]
        if min(scan) < 0.08 - 1e-6 or max(scan) > 0.12 + 1e-6:
            out.append(f"{name} at {at}: his eyes {min(scan) * 100:.1f} to {max(scan) * 100:.1f} cm over the rim while he looks round (wanted 8 to 12)")
        hy, ri = res["head_yaw"], res["rise"]
        if hy["fastest_deg_s"][0] > TURN_MOST + 1e-6:
            out.append(f"{name} at {at}: his head turns {hy['fastest_deg_s'][0]:.0f} degrees a second at {hy['fastest_deg_s'][1]} s (wanted {TURN_MOST:.0f} at most)")
        for side in ("left", "right"):
            if hy["held_within_1deg_s"][side] < HOLD_LEAST - 1e-6:
                out.append(f"{name} at {at}: he holds his look to the {side} {hy['held_within_1deg_s'][side]:.2f} s (wanted {HOLD_LEAST} at least)")
        if ri["eyes_fastest_over_rim_m_s"][0] > CREEP_MOST + 1e-6:
            out.append(f"{name} at {at}: his eyes go over the rim at {ri['eyes_fastest_over_rim_m_s'][0]:.2f} m/s (wanted {CREEP_MOST} at most)")
    rg = res["ring"]
    for label, r_ in (("", rg), (" between the keys", rg["between_keys"])):
        if r_["deepest_m"] is not None and r_["deepest_m"] > rg["tolerance_m"] + 1e-6:
            out.append(f"{name} at {at}: his {r_['where'][1]} {r_['deepest_m'] * 100:.1f} cm past the ring's wall at {r_['where'][0]} s{label} "
                       f"(the file's die_hatch_drop: {rg['tolerance_m'] * 100:.1f})")
        if r_["glasses_deepest_m"] is not None and r_["glasses_deepest_m"] > rg["tolerance_m"] + 1e-6:
            out.append(f"{name} at {at}: the glasses {r_['glasses_deepest_m'] * 100:.1f} cm past the ring's wall{label}")
        if r_["drum_deepest_m"] is not None and r_["drum_deepest_m"] > rg["tolerance_m"] + 1e-6:
            out.append(f"{name} at {at}: his {r_['drum_where'][1]} {r_['drum_deepest_m'] * 100:.1f} cm into the cupola's drum under the lip "
                       f"at {r_['drum_where'][0]} s{label}")
    if res["fists"]["gap_m"] > GAP:
        out.append(f"{name} at {at}: a fist {res['fists']['gap_m'] * 100:.1f} cm off the glasses at {res['fists']['where'][0]} s")
    if res["fists"]["between_keys_gap_m"] > GAP:
        out.append(f"{name} at {at}: a fist {res['fists']['between_keys_gap_m'] * 100:.1f} cm off the glasses between the keys "
                   f"({res['fists']['between_keys_where'][0]} s)")
    return out


def old_measure(rider, clip, hatch):
    """The same ring measure on one of the file's own clips (for the comparison)."""
    man = rider.man
    rim, ring, off = hatch["rim_over_feet"], hatch["ring"], np.array(hatch["root_offset"], float)
    worst = (-9.0, None, None)
    for t in [k[0] for k in next(tr for tr in clip["tracks"] if "rot" in tr)["rot"]]:
        G, X = gore.pose(man.rig, clip, t)
        W = rider.skinned(G, X)
        r = np.hypot(W[:, 0] + off[0], W[:, 1] + off[1])
        band = rider.body & (W[:, 2] > rim + LIP[0]) & (W[:, 2] < rim + LIP[1])
        if band.any():
            d = r[band] - ring
            j = int(np.argmax(d))
            if d[j] > worst[0]:
                worst = (float(d[j]), t, man.rig.names[int(rider.a[band][j])])
    return {"deepest_m": round(worst[0], 4), "where": [worst[1], worst[2]]}


def hatches_for(D, tanks):
    """Each of the file's tanks' hatches, with where commander_hatch puts his feet in it and the ring's tolerance."""
    out = []
    places = {p["piece"]: p for p in D["clips"][BASE]["places"]}
    shown = {r["at"].split()[0]: r for r in D.get("tank_clips", {}).get("check", {}).get(DROP_CLIP, [])}
    for v in tanks:
        h = hatch_of(v)
        off = places[v]["root_offset"]
        h["root_offset"] = [float(x) for x in off]
        h["rim_over_feet"] = round(h["rim"] - (h["socket"][2] + off[2]), 4)
        h["tolerance_m"] = float(shown[v]["deepest_m"]) if v in shown else 0.03
        if v in DRUMS:
            h["drum"] = list(DRUMS[v])
        out.append(h)
    return out


def check_file(name, D, log=print):
    """All the checks for one file as it is written. Gives (report, faults)."""
    tanks = FILES[name]
    hatches = hatches_for(D, tanks)
    rider = Rider(D)
    rep = {"hatches": hatches, "clips": {}, "seams": {}, "compare": {}}
    faults = []
    for h in hatches:
        log(f"   {h['tank']}: socket {h['socket']}, the rim {h['rim_over_socket']:.3f} over it ({h['rim_over_feet']:.2f} over his feet), "
            f"the ring's inside radius {h['ring']:.3f} ({h['ring_sides']} sides, {h['ring_flat']:.3f} to their flats), its wall "
            f"{-h['wall_from'] * 100:.0f} cm deep; the painted hole {h['hole_disc']}; tolerance {h['tolerance_m'] * 100:.2f} cm")
        if abs(h["rim_over_feet"] - hatches[0]["rim_over_feet"]) > 1e-3:
            faults.append(f"{name}: the rim is not the same height over his feet at {h['tank']} as at {hatches[0]['tank']}")
    for cn in (DUCK, PEEK):
        c = D["clips"].get(cn)
        if c is None:
            faults.append(f"{name}: no {cn}")
            continue
        rep["clips"][cn] = {"long": c["long"], "start": c.get("start"), "events": c["events"], "at": []}
        for h in hatches:
            res = check_clip(rider, cn, c, h, h["tolerance_m"], log)
            rep["clips"][cn]["at"].append(res)
            faults += faults_of(cn, res)
            hd, ey, rg = res["head_top_over_rim_m"], res["eyes_over_rim_m"], res["ring"]
            pick = [hd[min(range(len(hd)), key=lambda i: abs(hd[i][0] - t))] for t in
                    ((0.0, 0.1333, 0.2667, 0.4) if cn == DUCK else (0.0, 0.4, 0.8, 1.2, 1.6, 2.0, PEEK_LONG))]
            log(f"   {cn} at {h['tank']}: head top over the rim " + ", ".join(f"{v * 100:+.1f} cm at {t} s" for t, v in pick))
            if cn == PEEK:
                scan = [e[1] for e in ey if SCAN_WINDOW[0] - 1e-6 <= e[0] <= SCAN_WINDOW[1] + 1e-6]
                ri, hy = res["rise"], res["head_yaw"]
                log(f"      eyes over the rim while he looks round ({SCAN_WINDOW[0]} to {SCAN_WINDOW[1]} s): {min(scan) * 100:.1f} to {max(scan) * 100:.1f} cm")
                log(f"      rise: eyes at the rim at {ri['eyes_at_rim_s']} s, 8 cm over it at {ri['eyes_8cm_over_s']} s; fastest "
                    f"{ri['eyes_fastest_under_rim_m_s'][0]} m/s under the rim ({ri['eyes_fastest_under_rim_m_s'][1]} s), "
                    f"{ri['eyes_fastest_over_rim_m_s'][0]} m/s over it ({ri['eyes_fastest_over_rim_m_s'][1]} s)")
                log(f"      look round: head {hy['least_most_deg'][0]:+.1f} to {hy['least_most_deg'][1]:+.1f} degrees, fastest "
                    f"{hy['fastest_deg_s'][0]} deg/s ({hy['fastest_deg_s'][1]} s), held within 1 degree {hy['held_within_1deg_s']['left']} s "
                    f"left and {hy['held_within_1deg_s']['right']} s right")
            else:
                log(f"      at the end: head top {hd[-1][1] * 100:+.1f} cm, the highest of him and the glasses {max(res['highest_over_rim_m'][-1][1:]) * 100:+.1f} cm; "
                    f"his head's fastest fall {res['head_top_fastest_m_s']} m/s")
            cm_ = lambda v: "none in the band" if v is None else "%+.1f" % (v * 100)
            bk = rg["between_keys"]
            log(f"      ring: body {cm_(rg['deepest_m'])} cm past its wall at most at the keys ({rg['where'][1]} at {rg['where'][0]} s; "
                f"{rg['points_past_1_2cm']} points past 1.2 cm), {cm_(bk['deepest_m'])} between them ({bk['where'][1]} at {bk['where'][0]} s); "
                f"tolerance {rg['tolerance_m'] * 100:.1f}; glasses {cm_(rg['glasses_deepest_m'])} ({cm_(bk['glasses_deepest_m'])}); "
                f"drum {cm_(rg['drum_deepest_m'])} ({cm_(bk['drum_deepest_m'])})")
            log(f"      fists on the glasses within {res['fists']['gap_m'] * 1000:.1f} mm at the keys ({res['fists']['where']}), "
                f"{res['fists']['between_keys_gap_m'] * 1000:.1f} mm between them; fastest joint "
                f"{res['fastest_turn']['deg_s']} deg/s ({res['fastest_turn']['bone']} at {res['fastest_turn']['t']} s)")
    if DUCK in D["clips"] and PEEK in D["clips"]:
        duck, peek, base = D["clips"][DUCK], D["clips"][PEEK], D["clips"][BASE]
        for label, a, ta, b, tb in (("duck_start_from_commander_hatch", duck, 0.0, base, 0.0),
                                    ("peek_start_from_duck_end", peek, 0.0, duck, duck["long"]),
                                    ("peek_end_to_commander_hatch", peek, peek["long"], base, 0.0)):
            deg, who, m, pdeg = seam(a, ta, b, tb)
            rep["seams"][label] = {"bone_turn_deg": round(deg, 4), "bone": rider.man.rig.names[who] if who is not None else None,
                                   "glasses_place_m": round(m, 5), "glasses_turn_deg": round(pdeg, 4)}
            # (the bones' places follow from their turns and moves: compared as posed)
            Ga, Xa = gore.pose(rider.man.rig, a, ta)
            Gb, Xb = gore.pose(rider.man.rig, b, tb)
            keep = [i for i in range(rider.man.nb) if rider.man.rig.names[i] not in ("weapon", "bolt", "bipod")]
            rep["seams"][label]["joint_place_m"] = round(float(np.linalg.norm(Xa[keep] - Xb[keep], axis=1).max()), 5)
            log(f"   seam {label}: bones within {deg:.3f} degrees, joints within {rep['seams'][label]['joint_place_m'] * 1000:.2f} mm, "
                f"the glasses within {m * 1000:.2f} mm and {pdeg:.3f} degrees")
            if deg > SEAM_DEG or m > SEAM_M or pdeg > SEAM_DEG or rep["seams"][label]["joint_place_m"] > SEAM_M:
                faults.append(f"{name}: seam {label}: {deg:.2f} degrees, {m * 1000:.1f} mm")
        if duck.get("start") != [BASE, 0.0] or peek.get("start") != [DUCK, duck["long"]]:
            faults.append(f"{name}: `start` is not written as asked")
        ev_d, ev_p = duck["events"], peek["events"]
        if not (len(ev_d) == 1 and ev_d[0]["fx"] == "hide" and abs(ev_d[0]["t"] - duck["long"]) < 1e-9):
            faults.append(f"{name}: the duck's `hide` is not at its last key")
        if not (len(ev_p) == 1 and ev_p[0]["fx"] == "show" and ev_p[0]["t"] == 0.0):
            faults.append(f"{name}: the peek's `show` is not at its first key")
        rows = duck["props"]["binoculars"]["track"]
        if rows[-1][8] != 0 or any(r[8] != 1 for r in rows[:-1]) or any(r[8] != 1 for r in peek["props"]["binoculars"]["track"]):
            faults.append(f"{name}: the glasses' `shown` is not 1 through both clips and 0 at the duck's last key")
        for cn, c in ((DUCK, duck), (PEEK, peek)):
            if any(c.get(k) != base.get(k) for k in ("anchor", "places", "weapon")) or c["weapon"] != "none":
                faults.append(f"{name}: {cn}'s anchor, places or weapon are not commander_hatch's")
            n = int(round(c["long"] * 30)) + 1
            tr = c["props"]["binoculars"]["track"]
            if len(tr) != n or any(abs(r[0] - k / 30.0) > 6e-5 for k, r in enumerate(tr)):
                faults.append(f"{name}: {cn}'s glasses are not keyed 30 a second")
    for cn in (BASE, DROP_CLIP):
        rep["compare"][cn] = [dict(old_measure(rider, D["clips"][cn], h), at=h["tank"] + " commander") for h in hatches]
    rep["compare"]["note"] = ("this tool's ring measure on two of the file's own clips, at their keys. It counts every point of him in "
                              "the band that is farther out than the ring's inside wall, so hands that lie outside the ring on "
                              "the roof count too: die_hatch_drop, whose hands trail over the rim, reads large here. The "
                              "tolerance is the file's own check of die_hatch_drop (tank_clips.check), which counts the ring's "
                              "metal only; the two new clips are held to it by the stricter measure")
    log("   (this measure on the file's own clips, hands outside the ring on the roof counted too: " +
        "; ".join(f"{cn} " + ", ".join(f"{r['deepest_m'] * 100:+.1f} cm at {r['at'].split()[0]} ({r['where'][1]}, {r['where'][0]} s)"
                                        for r in rep["compare"][cn]) for cn in (BASE, DROP_CLIP)) + ")")
    return rep, faults


# ================================================================================================ the files
def strip(D):
    """The file with this tool's additions taken out again (to see that nothing else changed)."""
    D = json.loads(json.dumps(D))
    for cn in (DUCK, PEEK):
        D["clips"].pop(cn, None)
    tc = D.get("tank_clips", {})
    tc["names"] = [n for n in tc.get("names", []) if n not in (DUCK, PEEK)]
    for k in ("show", "hide", "hatch_drill"):
        tc.pop(k, None)
    for cn in (DUCK, PEEK):
        tc.get("check", {}).pop(cn, None)
    return D


def add_to(D, name, log=print):
    """The two clips and their lines in `tank_clips` put into the file's data; gives the faults the checks find."""
    hatches = hatches_for(D, FILES[name])
    clips, fit = clips_for(D, hatches, log)
    for cn in (DUCK, PEEK):
        D["clips"][cn] = clips[cn]
    tc = D["tank_clips"]
    for cn in (DUCK, PEEK):
        if cn not in tc["names"]:
            tc["names"].append(cn)
    tc["hide"] = ("die_hatch_drop's event `hide` and commander_hatch_duck's (at its last key): stop drawing him (he is down "
                  "inside the turret), and the glasses")
    tc["show"] = "commander_hatch_peek's event `show` (at its first key): draw him again (he is coming up under the rim)"
    tc["hatch_drill"] = (f"commander_hatch_duck ({DUCK_LONG} s, no loop) when the tank's `buttoned` turns true, commander_hatch_peek "
                         f"({PEEK_LONG} s, no loop) when it turns false, then commander_hatch from 0 s (the peek's last key is "
                         "that pose exactly). A clip's `start` names the clip and time whose pose it begins in: the duck "
                         "begins in commander_hatch's at 0 s (blend 0.12 s to it from wherever he was), the peek in the duck's "
                         "last. While he is ducked nothing of him is drawn. Both go where commander_hatch goes (`places`), "
                         "with no gun (`weapon` none) and the glasses' `props` track (commander_more.py, 25 November 2026)")
    rep, faults = check_file(name, D, log)
    rep["fit"] = fit
    for cn in (DUCK, PEEK):
        if cn in rep["clips"]:
            tc["check"][cn] = [{"deepest_m": r["ring"]["deepest_m"], "where": r["ring"]["where"],
                                "points_past_1_2cm": r["ring"]["points_past_1_2cm"], "hand_gap_m": r["fists"]["gap_m"],
                                "cupola_ring_m": max(0.0, r["ring"]["deepest_m"]), "at": r["at"],
                                "how": "commander_more.py: his points past the ring's inside wall, in the band 5.5 cm under the rim"}
                               for r in rep["clips"][cn]["at"]]
    return rep, faults


def dump(D, path):
    with open(path + ".tmp", "w") as f:
        json.dump(D, f, separators=(",", ":"))
    os.replace(path + ".tmp", path)


def build(out_dir=STAGE, log=print):
    os.makedirs(out_dir, exist_ok=True)
    report = {"made_by": "commander_more.py", "files": {}, "faults": []}
    for name in FILES:
        src = os.path.join(CONTENT, name + ".json")
        raw = open(src).read()
        D = json.loads(raw)
        before = json.dumps(strip(D), separators=(",", ":"))
        log(f"{name}:")
        rep, faults = add_to(D, name, log)
        if json.dumps(strip(D), separators=(",", ":")) != before:
            faults.append(f"{name}: something besides the two clips and their lines changed")
        # (made again on what was just made, it must come out the same)
        text = json.dumps(D, separators=(",", ":"))
        again = json.loads(text)
        add_to(again, name, lambda *a: None)
        rep["same_when_made_again"] = json.dumps(again, separators=(",", ":")) == text
        if not rep["same_when_made_again"]:
            faults.append(f"{name}: made again on its own output, the file comes out different")
        p = os.path.join(out_dir, name + ".json")
        dump(D, p)
        rep["staged"] = p
        rep["staged_sha1"] = hashlib.sha1(text.encode()).hexdigest()
        rep["built_from"] = {"file": src, "sha1": hashlib.sha1(raw.encode()).hexdigest()}
        rep["clips_in_file"] = len(D["clips"])
        rep["bytes"] = [len(raw), len(text)]
        report["files"][name] = rep
        report["faults"] += faults
        log(f"   staged {p} ({len(raw)} to {len(text)} bytes, {len(D['clips'])} clips; made again it is "
            f"{'the same' if rep['same_when_made_again'] else 'DIFFERENT'})")
    dump(report, os.path.join(out_dir, REPORT))
    log("faults: " + ("; ".join(report["faults"]) if report["faults"] else "none"))
    return report["faults"]


def check(out_dir=STAGE, log=print):
    """The checks again on the staged files as they are written. The report is written again; what only the build
    knows of a file (the fit, the source it was built from) is kept while the staged file is still that one."""
    report = {"made_by": "commander_more.py --check", "files": {}, "faults": []}
    try:
        old = json.load(open(os.path.join(out_dir, REPORT))).get("files", {})
    except (OSError, ValueError):
        old = {}
    for name in FILES:
        p = os.path.join(out_dir, name + ".json")
        raw = open(p).read()
        D = json.loads(raw)
        log(f"{name} ({p}):")
        rep, faults = check_file(name, D, log)
        rep["staged"] = p
        rep["staged_sha1"] = hashlib.sha1(raw.encode()).hexdigest()
        was = old.get(name, {})
        if was.get("staged_sha1") == rep["staged_sha1"]:
            for k in ("fit", "built_from", "bytes", "clips_in_file", "same_when_made_again"):
                if k in was:
                    rep[k] = was[k]
        src_raw = open(os.path.join(CONTENT, name + ".json")).read()
        rep["only_the_clips_differ_from_content"] = strip(json.loads(src_raw)) == strip(D)
        rep["content_sha1_now"] = hashlib.sha1(src_raw.encode()).hexdigest()
        if not rep["only_the_clips_differ_from_content"]:
            faults.append(f"{name}: the staged file differs from Content's in more than the two clips and their lines (build again)")
        log(f"   against Content's file: {'only the two clips and their lines differ' if rep['only_the_clips_differ_from_content'] else 'MORE differs'}")
        for cn in (DUCK, PEEK):
            if cn not in D["tank_clips"]["names"]:
                faults.append(f"{name}: {cn} is not in tank_clips.names")
        report["files"][name] = rep
        report["faults"] += faults
    dump(report, os.path.join(out_dir, REPORT))
    log("faults: " + ("; ".join(report["faults"]) if report["faults"] else "none"))
    return report["faults"]


def deliver(content=False, out_dir=STAGE):
    """The staged files to the shared folder (and Content): checked first (nothing is written if a check fails), each
    destination backed up, the new file written beside it and moved into place. A destination that differs from the
    staged file in more than the two clips and their lines is left alone (build again from it). Gives how many
    destinations were not written."""
    faults = check(out_dir, lambda *a: None)
    if faults:
        print("NOT DELIVERED, the staged files fail their checks: " + "; ".join(faults))
        return len(FILES)
    os.makedirs(BACKUP, exist_ok=True)
    stamp = time.strftime("%m%d_%H%M%S", time.gmtime())
    left, wrote = 0, []
    for name in FILES:
        src = os.path.join(out_dir, name + ".json")
        text = open(src).read()
        new = json.loads(text)
        dests = [SHARED + name + ".json"] + ([os.path.join(CONTENT, name + ".json")] if content else [])
        for d in dests:
            if os.path.exists(d):
                if strip(json.load(open(d))) != strip(new):
                    print("LEFT ALONE (it differs from the staged file in more than the two clips; build again from it):", d)
                    left += 1
                    continue
                shutil.copy2(d, os.path.join(BACKUP, os.path.basename(d) + "." + stamp + (".content" if d.startswith(CONTENT) else ".shared")))
            with open(d + ".tmp_cmdr_more", "w") as f:
                f.write(text)
            os.replace(d + ".tmp_cmdr_more", d)
            wrote.append((d, text))
            print("wrote", d)
    if wrote:
        time.sleep(10)                # (the shared folder is a network mount: read back after it has settled)
    for d, text in wrote:
        back = open(d).read()
        X = json.loads(back)
        print("read back", d, "the same" if back == text else "DIFFERENT", len(X["clips"]), "clips;", [n for n in (DUCK, PEEK) if n in X["clips"]])
        if back != text:
            left += 1
    return left


def main(argv):
    out = argv[argv.index("--out") + 1] if "--out" in argv else STAGE
    if "--build" in argv:
        return 1 if build(out) else 0
    if "--check" in argv:
        return 1 if check(out) else 0
    if "--deliver" in argv:
        return 1 if deliver("--content" in argv, out) else 0
    print(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
