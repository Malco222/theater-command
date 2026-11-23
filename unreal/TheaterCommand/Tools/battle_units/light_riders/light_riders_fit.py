#!/usr/bin/env python3
"""light_riders_fit.py: the jeep and motorcycle riders' clips (release 2) onto every infantry soldier file.

light_riders.py (Blender, crew.py's pattern man) makes the seated clips once, on one man, at the light vehicles'
seats, and writes light_riders_ref.json beside this file: every clip sampled at its keys, each bone's turn in the
world (the game's axes, from the rest, which is the world's), the pelvis's place, each wrist and ankle, and the
rifle's frame and its butt's floor point where he holds one. This puts them on each file's own man, with numpy only:

  - the trunk, the head, the hands, the fingers and the feet keep the reference's turns, the pelvis its place; in the
    clips with a backrest behind him, a man whose kit sticks out behind further than the reference's sits forward
    by the difference (up to SHIFT_MAX), his hands and boots staying where they were;
  - each arm reaches its wrist's place and each leg its ankle's with its own lengths (two-bone IK, the elbow and
    knee bent the way the reference's were, the least turn: the twist is kept);
  - his weapon by the clip's mode: "none" put away 2 m under his feet, out of sight (the game hides it, as in the
    tank crew's clips); "slung" on his back, riding his chest exactly as in his own stretcher_front; "rifle" his
    own weapon stood on the reference's butt point with the reference's turn, a hand slid down it where his weapon
    is shorter than the reference's;
  - bones the reference has not (the pouches, the radio's whip) ride their parent as at rest.

    python3 light_riders_fit.py --stage DIR [names]      fit and check, write DIR/<file>.riders.json (clips only)
    python3 light_riders_fit.py --deliver [names]        back up, fit, check, write into the shared soldier files
    python3 light_riders_fit.py --check [names]          check the shared files' delivered clips

The 17 infantry files (FILES); names are soldier_<X> or X. Nothing else in a file changes: deliver adds the clips,
a `ride_clips` block (the contract with the ground battle) and `about_riders`, and checks every other key and clip
is as it was.
"""
import json
import math
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.environ.get("RIDERS_FIT_REF", os.path.join(HERE, "light_riders_ref.json"))
SHARED = "/mnt/project-files/battle-models/land/soldier/"
BACKUP = "/home/user/art/looks/backup/riders/"
FILES = ["soldier_" + n for n in ("CHN", "FRA", "GER", "ITA", "ITA_desert", "JAP", "POL", "SOV", "SOV_snow", "SOV_winter",
                                  "UK", "USA", "USA_bar", "USA_bazooka", "USA_loader", "USA_nco", "USA_radio")]
GUN_AWAY = np.array((0.2, 0.0, -2.0))       # (soldier_tankers.py's: the weapon put away under his feet)
BODY = ("pelvis", "spine_01", "spine_02", "chest", "neck", "head",
        "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "fingers_l", "thumb_l",
        "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "fingers_r", "thumb_r",
        "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r")
ARM = {s: (f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}") for s in "lr"}
LEG = {s: (f"thigh_{s}", f"calf_{s}", f"foot_{s}") for s in "lr"}
SLUNG_FROM = ("stretcher_front", "stretcher_rear", "reel_walk")
SLUNG_ON = ("chest", "spine_02", "spine_01", "pelvis")
# the clips with a backrest behind him (a car's seats, the sidecar's), and how far forward a man may sit for his kit
BACKREST = ("car_drive", "car_drive_b", "car_drive_c", "car_ride", "car_ride_back", "die_seat_slump", "die_ride_slump",
            "sidecar_gunner", "sidecar_gunner_b", "sidecar_ride", "die_sidecar_slump")
BACK_Z = (0.90, 1.15)        # (standing: from the belt to the backrest's top once seated)
SHIFT_MAX = 0.06
MUZZLE_KEEP = 0.07           # (a hand's grip no nearer his weapon's end than this)
# the checks (metres, degrees)
GAP_TOL = {"hand": 0.012, "ankle": 0.015}
REF_TOL = 0.05               # (a hand or ankle within this of the reference's where the reference itself reached)


# ================================================================================================ turns
def q2m(q):
    w, x, y, z = np.asarray(q, float) / np.linalg.norm(q)
    return np.array(((1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)),
                     (2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)),
                     (2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y))))


def m2q(M):
    t = np.trace(M)
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        q = (0.25 * s, (M[2, 1] - M[1, 2]) / s, (M[0, 2] - M[2, 0]) / s, (M[1, 0] - M[0, 1]) / s)
    elif M[0, 0] > M[1, 1] and M[0, 0] > M[2, 2]:
        s = math.sqrt(1.0 + M[0, 0] - M[1, 1] - M[2, 2]) * 2
        q = ((M[2, 1] - M[1, 2]) / s, 0.25 * s, (M[0, 1] + M[1, 0]) / s, (M[0, 2] + M[2, 0]) / s)
    elif M[1, 1] > M[2, 2]:
        s = math.sqrt(1.0 + M[1, 1] - M[0, 0] - M[2, 2]) * 2
        q = ((M[0, 2] - M[2, 0]) / s, (M[0, 1] + M[1, 0]) / s, 0.25 * s, (M[1, 2] + M[2, 1]) / s)
    else:
        s = math.sqrt(1.0 + M[2, 2] - M[0, 0] - M[1, 1]) * 2
        q = ((M[1, 0] - M[0, 1]) / s, (M[0, 2] + M[2, 0]) / s, (M[1, 2] + M[2, 1]) / s, 0.25 * s)
    q = np.array(q)
    return q / np.linalg.norm(q)


def ang_deg(A, B):
    c = (np.trace(A.T @ B) - 1.0) / 2.0
    return math.degrees(math.acos(max(-1.0, min(1.0, c))))


def minrot(u, v):
    """The least turn taking direction u to direction v."""
    u = u / np.linalg.norm(u)
    v = v / np.linalg.norm(v)
    c = float(u @ v)
    if c > 1.0 - 1e-12:
        return np.eye(3)
    a = np.cross(u, v)
    s = np.linalg.norm(a)
    if s < 1e-9:                                 # (opposite: half a turn about any square axis)
        p = np.cross(u, (1.0, 0.0, 0.0) if abs(u[0]) < 0.9 else (0.0, 1.0, 0.0))
        p /= np.linalg.norm(p)
        return 2.0 * np.outer(p, p) - np.eye(3)
    k = a / s
    K = np.array(((0, -k[2], k[1]), (k[2], 0, -k[0]), (-k[1], k[0], 0)))
    return np.eye(3) + s * K + (1 - c) * (K @ K)


def key_at(keys, t, rot):
    K = np.asarray(keys, float)
    ts = K[:, 0]
    if t <= ts[0]:
        v = K[0, 1:]
    elif t >= ts[-1]:
        v = K[-1, 1:]
    else:
        i = int(np.searchsorted(ts, t, side="right")) - 1
        u = (t - ts[i]) / max(ts[i + 1] - ts[i], 1e-9)
        a, b = K[i, 1:], K[i + 1, 1:]
        if rot and a @ b < 0:
            b = -b
        v = a + (b - a) * u
    return v / np.linalg.norm(v) if rot else v.copy()


# ================================================================================================ a file's man
class Man:
    def __init__(self, D):
        bones = D["bones"]
        self.names = [b["name"] for b in bones]
        self.ix = {n: i for i, n in enumerate(self.names)}
        self.parent = [b["parent"] for b in bones]
        self.H0 = np.array([b["head"] for b in bones], float)
        P = np.asarray(D["near"]["points"], float).reshape(-1, 3)
        part = np.asarray(D["near"]["part"])
        gun = [self.ix[n] for n in ("weapon", "bolt", "bipod") if n in self.ix]
        self.gun_pts = P[np.isin(part, gun)] - self.H0[self.ix["weapon"]]     # (the weapon's frame, at rest)
        body = P[~np.isin(part, gun)]
        m = (body[:, 2] > BACK_Z[0]) & (body[:, 2] < BACK_Z[1]) & (np.abs(body[:, 1]) < 0.2)
        self.back = float(body[m, 0].min())                   # (how far his kit sticks out behind, at the backrest)

    def fk(self, q, mv):
        nb = len(self.names)
        G = np.zeros((nb, 3, 3))
        X = np.zeros((nb, 3))
        for i in range(nb):
            p = self.parent[i]
            Q = q.get(self.names[i], np.eye(3))
            m = mv.get(self.names[i], np.zeros(3))
            if p is None:
                G[i] = Q
                X[i] = self.H0[i] + m
            else:
                G[i] = G[p] @ Q
                X[i] = X[p] + G[p] @ (self.H0[i] - self.H0[p] + m)
        return G, X

    def place(self, G, X_pelvis):
        """Heads from world turns G, the pelvis at X_pelvis, the root at the origin."""
        X = np.zeros((len(self.names), 3))
        for i, n in enumerate(self.names):
            p = self.parent[i]
            if p is None:
                X[i] = self.H0[i]
            elif n == "pelvis":
                X[i] = X_pelvis
            else:
                X[i] = X[p] + G[p] @ (self.H0[i] - self.H0[p])
        return X

    def subtree(self, name):
        out, todo = [], [self.ix[name]]
        while todo:
            i = todo.pop()
            out.append(i)
            todo += [j for j, p in enumerate(self.parent) if p == i]
        return out


def clip_pose(clip, t):
    q, mv = {}, {}
    for tr in clip["tracks"]:
        n = tr["part_name"]
        if tr.get("rot"):
            q[n] = q2m(key_at(tr["rot"], t, True))
        if tr.get("move"):
            mv[n] = key_at(tr["move"], t, False)
    return q, mv


def slung_frame(D, man):
    """His slung weapon's frame on the bone it rides (his own stretcher_front's: the one it moves least against)."""
    src = next(c for c in SLUNG_FROM if c in D["clips"])
    clip = D["clips"][src]
    w = man.ix["weapon"]
    best = None
    for on in SLUNG_ON:
        b = man.ix[on]
        rels = []
        for t in np.linspace(0.0, clip["long"], 9):
            G, X = man.fk(*clip_pose(clip, t))
            rels.append((G[b].T @ G[w], G[b].T @ (X[w] - X[b])))
        spread = max(np.linalg.norm(r[1] - rels[0][1]) + 0.3 * math.radians(ang_deg(r[0], rels[0][0])) for r in rels)
        if best is None or spread < best[0]:
            best = (spread, on, rels[0][0], rels[0][1])
    return {"from": src, "on": best[1], "R": best[2], "t": best[3], "spread": best[0]}


# ================================================================================================ the fit
def two_bone(S, E, W, T, a, b, hint):
    """Shoulder S, elbow E, wrist W (now) and the wrist's target T: the new elbow and wrist, bent in the plane E is
    bent in now (short of T where T is out of reach); where the limb is near straight, so that plane is barely
    there, toward `hint` (the way an elbow or a knee goes) instead."""
    d_v = T - S
    d = float(np.linalg.norm(d_v))
    u = d_v / max(d, 1e-9)
    dc = min(max(d, abs(a - b) + 1e-4), a + b - 1e-4)
    pole = (E - S) - u * float((E - S) @ u)
    h = np.asarray(hint, float) - u * float(np.asarray(hint, float) @ u)
    pole = pole + POLE_HINT * h / max(float(np.linalg.norm(h)), 1e-9)
    v = pole / np.linalg.norm(pole)
    ca = (a * a + dc * dc - b * b) / (2 * a * dc)
    sa = math.sqrt(max(0.0, 1.0 - ca * ca))
    E2 = S + a * (ca * u + sa * v)
    return E2, S + u * dc


POLE_HINT = 0.03            # (m: a limb bent less than this off its straight line takes its bend from the hint)
HINT = {**{f"upperarm_{s}": (-0.3, (1 if s == "l" else -1) * 0.5, -1.0) for s in "lr"},
        **{f"thigh_{s}": (1.0, (1 if s == "l" else -1) * 0.15, 0.2) for s in "lr"}}


def reach(man, G, X, chain, target):
    """Turn the chain's two bones (the least turns) so its end's head is at target; G changed in place."""
    i0, i1, i2 = (man.ix[n] for n in chain)
    S, E, W = X[i0], X[i1], X[i2]
    a = float(np.linalg.norm(man.H0[i1] - man.H0[i0]))
    b = float(np.linalg.norm(man.H0[i2] - man.H0[i1]))
    E2, W2 = two_bone(S, E, W, target, a, b, HINT[chain[0]])
    R1 = minrot(E - S, E2 - S)
    G[i0] = R1 @ G[i0]
    lower = R1 @ (W - E)
    R2 = minrot(lower, W2 - E2)
    G[i1] = R2 @ R1 @ G[i1]
    return float(np.linalg.norm(W2 - target))


CHILD = {"pelvis": "spine_01", "spine_01": "spine_02", "spine_02": "chest", "chest": "neck", "neck": "head",
         **{f"clavicle_{s}": f"upperarm_{s}" for s in "lr"}, **{f"upperarm_{s}": f"lowerarm_{s}" for s in "lr"},
         **{f"lowerarm_{s}": f"hand_{s}" for s in "lr"}, **{f"hand_{s}": f"fingers_{s}" for s in "lr"},
         **{f"thigh_{s}": f"calf_{s}" for s in "lr"}, **{f"calf_{s}": f"foot_{s}" for s in "lr"},
         **{f"foot_{s}": f"ball_{s}" for s in "lr"}}


def ref_turns(ref, rc):
    """The reference's world turns, frame by frame, with any bone the solver turned half round its own length from
    one key to the next (the twist about a bone it only points can come out either way) turned back, so that every
    bone turns smoothly (the man is the same: only the twist that no key asked for is undone)."""
    names = ref["bones"]
    rest = {n: np.asarray(ref["rest"][n], float) for n in names}
    T = np.array([[q2m(q) for q in frame] for frame in rc["q"]])          # (frames, bones, 3, 3)
    for b, c in CHILD.items():
        i = names.index(b)
        axis = rest[c] - rest[b]
        axis = axis / np.linalg.norm(axis)
        K = np.array(((0, -axis[2], axis[1]), (axis[2], 0, -axis[0]), (-axis[1], axis[0], 0)))
        half = np.eye(3) + 2.0 * (K @ K)
        for f in range(1, len(T)):
            if ang_deg(T[f - 1, i], T[f, i]) > 120.0 and ang_deg(T[f - 1, i], T[f, i] @ half) < 60.0:
                T[f:, i] = T[f:, i] @ half          # (and every key after it, which the solver kept turned so)
    return T


def gun_placed(man, R, butt):
    """His weapon turned R, its lowest point on butt: the weapon bone's head."""
    pts = man.gun_pts @ R.T
    low = pts[int(np.argmin(pts[:, 2]))]
    return np.asarray(butt, float) - low


def fit_clip(man, ref, name, rc, slung, shift):
    rix = {n: i for i, n in enumerate(ref["bones"])}
    nb = len(man.names)
    w = man.ix["weapon"]
    mode = rc["mode"]
    xs = man.gun_pts[:, 0]
    keep_hi = float(xs.max()) - MUZZLE_KEEP
    frames, gaps = [], {"hand": 0.0, "ankle": 0.0, "slide": 0.0}
    RT = ref_turns(ref, rc)
    for f, t in enumerate(rc["times"]):
        G = np.zeros((nb, 3, 3))
        for i, n in enumerate(man.names):
            p = man.parent[i]
            if n in BODY and n in rix:
                G[i] = RT[f, rix[n]]
            else:
                G[i] = G[p] if p is not None else np.eye(3)
        pel = np.asarray(rc["pelvis"][f], float) + (shift, 0.0, 0.0)
        X = man.place(G, pel)
        # his weapon
        if mode == "rifle":
            Rw = q2m(rc["W"][f][3:])
            Xw = gun_placed(man, Rw, rc["butt"][f])
        elif mode == "slung":
            b = man.ix[slung["on"]]
            Rw = G[b] @ slung["R"]
            Xw = X[b] + G[b] @ slung["t"]
        else:
            Rw, Xw = np.eye(3), GUN_AWAY.copy()
        # the hands: the reference's wrists (on a shorter weapon, slid down it as far as the upper grip needs)
        tgt = {s: np.asarray(rc["wrist"][s][f], float) for s in "lr"}
        if mode == "rifle":
            Wr = q2m(rc["W"][f][3:])
            Wr_x = np.asarray(rc["W"][f][:3], float)
            hold = rc["hold"][f]
            slide = 0.0
            for s in "lr":
                g_loc = Wr.T @ (np.asarray(rc["grip"][s][f], float) - Wr_x)     # (the grip on the reference's rifle)
                g_his = g_loc + Wr.T @ (Wr_x - Xw)                               # (the same place, his weapon's frame)
                slide = min(slide, keep_hi - float(g_his[0]))
            for s, h in zip("lr", hold):
                tgt[s] = tgt[s] + h * slide * Rw[:, 0]
            gaps["slide"] = min(gaps["slide"], slide)
        for s in "lr":
            gaps["hand"] = max(gaps["hand"], reach(man, G, X, ARM[s], tgt[s]))
            X = man.place(G, pel)
            for c in (f"hand_{s}", f"fingers_{s}", f"thumb_{s}"):
                G[man.ix[c]] = RT[f, rix[c]]
            gaps["ankle"] = max(gaps["ankle"], reach(man, G, X, LEG[s], np.asarray(rc["ankle"][s][f], float)))
            X = man.place(G, pel)
            for c in (f"foot_{s}", f"ball_{s}"):
                G[man.ix[c]] = RT[f, rix[c]]
        # the bones that ride their parent (pouches, the radio's whip) follow the turned limbs; then the weapon's tree
        for i, n in enumerate(man.names):
            p = man.parent[i]
            if p is not None and n not in BODY:
                G[i] = G[p]
        for i in man.subtree("weapon"):
            G[i] = Rw
        X = man.place(G, pel)
        off = Xw - X[w]
        for i in man.subtree("weapon"):
            X[i] = X[i] + off
        frames.append((G, X))
        for s in "lr":
            gaps["hand"] = max(gaps["hand"], float(np.linalg.norm(X[man.ix[f"hand_{s}"]] - tgt[s])))
            gaps["ankle"] = max(gaps["ankle"], float(np.linalg.norm(X[man.ix[f"foot_{s}"]] - np.asarray(rc["ankle"][s][f], float))))
    return frames, gaps


def tracks_of(man, frames, times):
    """Local turns and the pelvis's and weapon's moves as json tracks (signs kept continuous, unmoving bones left out)."""
    nb = len(man.names)
    out = []
    for i, n in enumerate(man.names):
        p = man.parent[i]
        if p is None:
            continue
        locs = [G[p].T @ G[i] for G, X in frames]
        tr = {"part": i, "part_name": n}
        if any(ang_deg(L, np.eye(3)) > 0.01 for L in locs):
            keys, prev = [], None
            for t, L in zip(times, locs):
                q = m2q(L)
                if prev is not None and q @ prev < 0:
                    q = -q
                prev = q
                keys.append([round(t, 4)] + [round(float(x), 5) for x in q])
            tr["rot"] = keys
        if n in ("pelvis", "weapon"):
            mv = [G[p].T @ (X[i] - X[p]) - (man.H0[i] - man.H0[p]) for G, X in frames]
            tr["move"] = [[round(t, 4)] + [round(float(x), 4) for x in m] for t, m in zip(times, mv)]
        if len(tr) > 2:
            out.append(tr)
    assert len(out) <= nb
    return out


FROM = "light_riders.py (crew.py's pattern man at the light vehicles' seats), fitted to this man by light_riders_fit.py"


_PATTERN = {}


def pattern_back():
    """How far the pattern man's kit sticks out behind (his file's: the seats were fitted to it)."""
    if "back" not in _PATTERN:
        _PATTERN["back"] = Man(json.load(open(os.path.join(SHARED, "soldier_GER.json")))).back
    return _PATTERN["back"]


def riders_for(D, ref, log=print):
    man = Man(D)
    gref = pattern_back()
    shift = min(SHIFT_MAX, max(0.0, gref - man.back))
    slung = slung_frame(D, man)
    clips, report = {}, {}
    for name, rc in ref["clips"].items():
        sh = shift if name in BACKREST else 0.0
        frames, gaps = fit_clip(man, ref, name, rc, slung, sh)
        c = {"long": rc["long"], "loop": rc["loop"], "stride": 0.0, "weapon": rc["mode"], "events": [],
             "anchor": rc["anchor"], "about": rc["about"], "from": FROM,
             "places": rc["places"], "tracks": tracks_of(man, frames, rc["times"])}
        if rc.get("death"):
            c["death"] = rc["death"]
        if sh:
            c["about"] += f" (This man's kit sits {sh * 100:.0f} cm further from the backrest than the pattern man's: " \
                          "he sits that much further forward, his hands and boots where they were.)"
        clips[name] = c
        report[name] = {k: round(v, 4) for k, v in gaps.items()}
    info = {"shift": round(shift, 4), "back": round(man.back, 4), "slung_on": slung["on"], "slung_from": slung["from"],
            "slung_spread": round(float(slung["spread"]), 4),
            "weapon_long": round(float(man.gun_pts[:, 0].max() - man.gun_pts[:, 0].min()), 3)}
    return clips, report, info


# ================================================================================================ the checks
def check(D, names=None, ref=None, log=print):
    """The delivered clips against the reference: each hand and ankle where the reference's is (within REF_TOL,
    beyond each man's reach difference), turns continuous, loops closed, the weapon put away or on him."""
    man = Man(D)
    ref = ref or json.load(open(REF))
    bad = []
    for name, rc in ref["clips"].items():
        if names and name not in names:
            continue
        c = D["clips"].get(name)
        if c is None:
            bad.append(f"{name} missing")
            continue
        worst = {"hand": 0.0, "ankle": 0.0}
        for f, t in enumerate(rc["times"]):
            G, X = man.fk(*clip_pose(c, t))
            for s in "lr":
                d_h = float(np.linalg.norm(X[man.ix[f"hand_{s}"]] - np.asarray(rc["wrist"][s][f])))
                d_a = float(np.linalg.norm(X[man.ix[f"foot_{s}"]] - np.asarray(rc["ankle"][s][f])))
                if rc["mode"] == "rifle":
                    d_h = 0.0 if d_h < 0.2 else d_h         # (a hand slid down his own weapon: measured at the fit)
                worst["hand"] = max(worst["hand"], d_h)
                worst["ankle"] = max(worst["ankle"], d_a)
            if c["weapon"] == "none" and X[man.ix["weapon"]][2] > -1.0:
                bad.append(f"{name} weapon not put away at {t}")
                break
        if worst["hand"] > REF_TOL or worst["ankle"] > REF_TOL:
            bad.append(f"{name} hand {worst['hand'] * 100:.1f} / ankle {worst['ankle'] * 100:.1f} cm off the reference's")
        for tr in c["tracks"]:
            for k in ("rot",):
                keys = tr.get(k) or []
                for a, b in zip(keys, keys[1:]):
                    if np.dot(a[1:], b[1:]) < 0:
                        bad.append(f"{name} {tr['part_name']} sign flips at {b[0]}")
                        break
                    if math.degrees(2 * math.acos(min(1.0, abs(float(np.dot(a[1:], b[1:])))))) > (75.0 if name.startswith("die_") else 40.0):
                        bad.append(f"{name} {tr['part_name']} jumps at {b[0]}")
                        break
                if c["loop"] and keys and np.abs(np.asarray(keys[0][1:]) - np.asarray(keys[-1][1:])).max() > 1e-3:
                    bad.append(f"{name} {tr['part_name']} loop not closed")
    return bad


# ================================================================================================ the contract
ABOUT = ("light_riders.py and light_riders_fit.py (release 2, 23 November 2026): the men who ride the light vehicles "
         "(jeeps, field cars, motorcycles and sidecars, battle-models/land/vehicles/): a clip for each seat and a death "
         "for each, made on one pattern man at the vehicles' own seat sockets and grips and fitted to this man (his "
         "arms and legs reach the same grips, pedals and footrests). `ride_clips` says which clip goes at which seat. "
         "Each clip's root goes at its seat socket moved by its root_offset (forward, right, up of the socket's "
         "facing, the game's axes), as the tank crew's commander_hatch goes at its hatch. Each car seat's death falls to "
         "the car's outside (die_seat_slump/_r for the driver, die_ride_slump/_l for a passenger), so two men side "
         "by side never slump onto each other. Fitted to the mended Kübelwagen and Kurogane seats (the seat-fix "
         "models).")


def contract(ref):
    seats, deaths, by_vehicle = [], {}, {}
    for name, rc in ref["clips"].items():
        if rc.get("death"):
            seats.append(name)
            deaths[name] = rc["death"]
            for p in rc["places"]:
                by_vehicle.setdefault(p["piece"], {})[p["socket"]] = {"clip": name, "root_offset": p["root_offset"]}
    return {
        "names": list(ref["clips"]),
        "seats": seats,
        "death_of": deaths,
        "by_vehicle": by_vehicle,
        "places": "each clip's `places` (and by_vehicle here): the vehicle's socket moved by root_offset (forward, "
                  "right, up of the socket's facing, the game's axes); the clip's root goes there, facing as the "
                  "socket does. A seat's root is the seat's point (anchor `seat`); the Jeep's gunner's is the floor at "
                  "his feet (anchor `feet`). Loops are 4 s; the deaths 2.4 s, the last pose held from 1.5 s (play once "
                  "and hold). Each car seat has exactly one death in its `places`, falling to the car's outer "
                  "side (away from the man beside him): die_seat_slump (driver, folds to his left) and die_seat_slump_r "
                  "(the right-hand-drive Fiat 508 and Kurogane drivers, to his right); die_ride_slump (a passenger on "
                  "the car's right, slumps to his right) and die_ride_slump_l (a passenger on the left, to his left). "
                  "Find a seat's death by the death clips' `places`, as you find its loop.",
        "weapon": "`weapon`: `none` the rifle put away 2 m under his feet (hide the weapon, bolt and bipod triangles, "
                  "as for the tank crew); `slung` on his back, drawn; `rifle` stood between his knees in his hands, "
                  "drawn (car_ride, car_ride_back, die_ride_slump, die_ride_slump_l).",
        "turn_with_gun": "jeep_gunner, sidecar_gunner and sidecar_gunner_b hold the gun's grips (grip.gunner.l/.r): if "
                         "the game turns the gun on its mount (mg_mount), turn the man with it about the mount's "
                         "upright axis.",
        "grip_labels": "the cars' grip.driver.l is on the driver's right and .r on his left (the clips take each hand "
                       "from where the grips are, so nothing needs changing); the motorcycles' bar grips are on their "
                       "steering part: if the game turns the bars, the hands are where the bars are straight.",
        "far": "the far pose bank (pose_bank.json) has no seated poses: draw riders with the near or mid meshes.",
        "check": checks_at_seats(),
    }


def checks_at_seats():
    """The pattern man's checks at every seat (light_riders_checks.json: light_riders.py's check_all), worst first."""
    path = os.path.join(os.path.dirname(REF), "light_riders_checks.json")
    if not os.path.exists(path):
        return {}
    C = json.load(open(path))
    return {"how": C["how"], "clips": {k: sorted(v, key=lambda r: -r["deepest_m"]) for k, v in C["clips"].items()}}


# ================================================================================================ stage, deliver
def targets(names):
    if not names:
        return list(FILES)
    out = []
    for n in names:
        n = n if n.startswith("soldier_") else "soldier_" + n
        if n not in FILES:
            raise SystemExit(f"not an infantry file: {n}")
        out.append(n)
    return out


def stage(dir_, names, log=print):
    ref = json.load(open(REF))
    os.makedirs(dir_, exist_ok=True)
    rep = {}
    for n in targets(names):
        D = json.load(open(os.path.join(SHARED, n + ".json")))
        clips, gaps, info = riders_for(D, ref, log)
        D["clips"].update(clips)
        bad = check(D, ref=ref)
        rep[n] = {"bad": bad, "info": info, "gaps": gaps}
        with open(os.path.join(dir_, n + ".riders.json"), "w") as f:
            json.dump(clips, f, separators=(",", ":"))
        worst_h = max(g["hand"] for g in gaps.values())
        worst_a = max(g["ankle"] for g in gaps.values())
        log(f"{n:22s} {'PASS' if not bad else 'FAIL'}  shift {info['shift'] * 100:3.1f} cm  slung on {info['slung_on']:8s} "
            f"weapon {info['weapon_long']:.2f} m  worst hand gap {worst_h * 100:.1f} cm, ankle {worst_a * 100:.1f} cm"
            + ("" if not bad else "  " + "; ".join(bad[:4])))
    return rep


def unchanged(before, after, added):
    """Every key and clip of before is in after, the same."""
    diff = [k for k in before if k != "clips" and before[k] != after.get(k)]
    diff += [f"clip {k}" for k in before["clips"] if before["clips"][k] != after["clips"].get(k)]
    extra = [k for k in after if k not in before and k not in added]
    return diff + [f"new key {k}" for k in extra]


def deliver(names, log=print, folder=SHARED):
    ref = json.load(open(REF))
    stamp = time.strftime("%Y%m%dT%H%M%S")
    os.makedirs(BACKUP, exist_ok=True)
    done = []
    for n in targets(names):
        path = os.path.join(folder, n + ".json")
        shutil.copy2(path, os.path.join(BACKUP, f"{n}.{stamp}.json"))
        D = json.load(open(path))
        before = json.loads(json.dumps(D))
        for k_ in [k_ for k_ in D["clips"] if k_ in ref["clips"]]:      # (a re-run replaces its own clips only)
            del D["clips"][k_]
            del before["clips"][k_]
        clips, gaps, info = riders_for(D, ref, log)
        D["clips"].update(clips)
        bad = check(D, ref=ref)
        if bad:
            raise RuntimeError(f"{n}: " + "; ".join(bad))
        block = contract(ref)
        block["this_man"] = info
        block["fit"] = gaps
        D["ride_clips"] = block
        D["about_riders"] = ABOUT
        bad = unchanged(before, D, ("ride_clips", "about_riders"))
        if [b for b in bad if b not in ("ride_clips", "about_riders")]:
            raise RuntimeError(f"{n}: changed {bad}")
        tmp = path + ".riders.part"
        with open(tmp, "w") as f:
            json.dump(D, f, separators=(",", ":"))
        os.replace(tmp, path)
        done.append(n)
        log(f"{n}: written")
    time.sleep(10)              # (the shared folder is a network mount: read back after it settles)
    for n in done:
        D = json.load(open(os.path.join(folder, n + ".json")))
        bad = check(D, ref=ref)
        log(f"{n}: re-read {'PASS' if not bad else 'FAIL ' + '; '.join(bad)}")
    return done


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "--stage":
        rep = stage(a[1], a[2:])
        print("PASS" if not any(r["bad"] for r in rep.values()) else "FAIL")
    elif a and a[0] == "--deliver":
        deliver(a[1:])
    elif a and a[0] == "--check":
        ref = json.load(open(REF))
        bad = {}
        for n in targets(a[1:]):
            bad[n] = check(json.load(open(os.path.join(SHARED, n + ".json"))), ref=ref)
            print(n, "PASS" if not bad[n] else bad[n])
        print("PASS" if not any(bad.values()) else "FAIL")
    else:
        print(__doc__)
