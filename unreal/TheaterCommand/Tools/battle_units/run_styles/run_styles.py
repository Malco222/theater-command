"""Release 2 run styles (Brian, 23 November 2026: "multiple animations of the players running so not all players are
running exactly the same at the same time. need variety to not make it look weird and more realistic").

Each soldier file gets three more runs and three more advances, each another real runner's motion capture (the CMU
library, cut by run_styles_mocap.py into run_styles_clips.json the way mocap.py cut the file's own run):

    run_b / advance_b   CMU 09_02 (subject 9): a longer, driving stride, the heels kicked high, leaning in
    run_c / advance_c   CMU 16_36 (subject 16): shorter, quicker steps, upright and bouncy
    run_d / advance_d   CMU 09_03 (subject 9, another take): high knees

(the advances with the advance's crouch laid over them, as mocap.py lays it over the run). Keyed straight onto each
file's own skeleton, nothing else in the file touched but `clips` (and a line in about_run_styles):

  * the legs, the pelvis and the trunk are the new runner's, every bone given the same turn from its rest as the
    capture's (mocap.py's world turns, mirrored to the game's y-right axes), the pelvis's travel scaled to this man's
    legs (his run's stride over the capture's) and set on the ground as his own run stands;
  * the trunk keeps what his own carry added to the capture (the turn of the shoulders for the rifle, the head held
    up): each trunk bone's mean turn between his run and its capture is laid on the new runner's;
  * his arms are his own run's (his hold on his rifle, BAR, bazooka, aid bags...), played in step: his run's left foot
    falls where the new runner's does; the weapon (a child of the root) rides the bone it rides in his run (the right
    hand for a rifle at the port) exactly as it does there, so it stays in his hands. A man whose run swings free arms
    (the sailors, the medic) takes the new runner's arms too.

Each new clip names its own (`style_of`: "run" or "advance"): the game picks one of the four per man by his seed
(LandMen, behind the run-styles switch), and far men banked in poses play the base clip's poses.

    python3 run_styles.py --stage DIR [names...]   write each file's new clips to DIR/<name>.run_styles.json and check
    python3 run_styles.py --deliver [names...]     add them to the shared soldier files (read fresh, backed up, written
                                                   whole); run again, it replaces its own clips
    python3 run_styles.py --into DIR [names...]    the same into another folder's soldier files (the game's copies in
                                                   Content/Models/Soldiers: git keeps their past, no backup)
    python3 run_styles.py --check [names...]       check the shared files' run styles

The clips' keys are 15 a second as every clip's: rotations [t, w, x, y, z], moves [t, x, y, z], in the parent's frame,
every bone's rest frame the world's (soldier.clips_json).
"""
import json
import math
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
CLIPS = os.path.join(HERE, "run_styles_clips.json")
SHARED = "/mnt/project-files/battle-models/land/soldier/"
BACKUP = "/home/user/art/looks/backup/run_styles/"
RATE = 15
FPS = 30
MIRROR = np.diag([1.0, -1.0, 1.0])
# new clip: (the file's own clip it is a style of, the capture in run_styles_clips.json)
NEW = [("run_b", "run", "run_b"), ("run_c", "run", "run_c"), ("run_d", "run", "run_d"),
       ("advance_b", "advance", "advance_b"), ("advance_c", "advance", "advance_c"), ("advance_d", "advance", "advance_d")]
# the capture each file's own clip was cut from (CMU 35_18, cut again here exactly as mocap.py cut it: the calibration)
SOURCE = {"run": "run_a", "advance": "advance_a"}
TRUNK = ("pelvis", "spine_01", "spine_02", "chest", "neck", "head")
LEGS = tuple(f"{b}_{s}" for s in "lr" for b in ("thigh", "calf", "foot", "ball"))
ARMS = tuple(f"{b}_{s}" for s in "lr" for b in ("clavicle", "upperarm", "lowerarm", "hand"))
ANCHORS = ("hand_r", "hand_l", "chest", "pelvis")
FREE_ARMS_DEG = 4.0          # (his run's arms within this of the capture's: free arms, the new runner's taken)
HEEL = (-0.061, 0.0, 0.0)    # (soldier.mo_events: the heel behind the ankle)


# ================================================================================================ rotations
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
    """The angle between two turns, degrees."""
    c = (np.trace(A.T @ B) - 1.0) / 2.0
    return math.degrees(math.acos(max(-1.0, min(1.0, c))))


def nlerp(a, b, u):
    if a @ b < 0:
        b = -b
    v = a * (1 - u) + b * u
    return v / np.linalg.norm(v)


def qmean(qs):
    qs = [np.asarray(q, float) for q in qs]
    ref = qs[0]
    acc = np.zeros(4)
    for q in qs:
        acc += q if q @ ref >= 0 else -q
    return acc / np.linalg.norm(acc)


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
        if rot:
            return nlerp(K[i, 1:], K[i + 1, 1:], u)
        v = K[i, 1:] + (K[i + 1, 1:] - K[i, 1:]) * u
    return v / np.linalg.norm(v) if rot else v.copy()


# ================================================================================================ the captures
class Capture:
    """A clip of run_styles_clips.json (clips.json's form, the reference man's rig, Blender's axes) as each bone's
    world turn from its rest in the game's axes (y right), per frame, and the pelvis's travel."""

    def __init__(self, c, rest):
        self.c = c
        names, parent, R = rest["names"], rest["parent"], np.array(rest["R"], float)
        ix = {n: i for i, n in enumerate(names)}
        pix = [ix[p] if p else -1 for p in parent]
        F = int(c["frames"])
        W = np.zeros((F, len(names), 3, 3))
        for b in range(len(names)):            # (rest_ref's order: parents first)
            p = pix[b]
            n = names[b]
            local = R[b] if p < 0 else R[p].T @ R[b]
            D = np.array([q2m(q) for q in c["bones"][n]]) if n in c["bones"] else np.broadcast_to(np.eye(3), (F, 3, 3))
            W[:, b] = (W[:, p] if p >= 0 else np.eye(3)) @ D @ local
        self.T = {n: MIRROR @ (W[:, b] @ R[b].T) @ MIRROR for b, n in enumerate(names)}
        self.parent = {n: (names[pix[b]] if pix[b] >= 0 else None) for b, n in enumerate(names)}
        self.pelvis = np.array(c["pelvis_loc"], float) * np.array((1.0, -1.0, 1.0))
        self.F = F
        self.long = (F - 1) / FPS
        self.stride = float(c["stride"])

    def local(self, bone, parent, f):
        """The bone's turn in its parent's frame (the json's) at frame f (fractional; loops)."""
        f = f % (self.F - 1)
        i = min(int(f), self.F - 2)
        u = f - i
        Tp = (lambda k: self.T[parent][k]) if parent in self.T else (lambda k: np.eye(3))
        a = m2q(Tp(i).T @ self.T[bone][i])
        b = m2q(Tp(i + 1).T @ self.T[bone][i + 1])
        return q2m(nlerp(a, b, u))

    def pelvis_at(self, f):
        f = f % (self.F - 1)
        i = min(int(f), self.F - 2)
        u = f - i
        return self.pelvis[i] * (1 - u) + self.pelvis[i + 1] * u


# ================================================================================================ the file's skeleton
class Skel:
    def __init__(self, bones):
        self.names = [b["name"] for b in bones]
        self.ix = {n: i for i, n in enumerate(self.names)}
        self.parent = [b["parent"] for b in bones]
        self.pname = [b["parent_name"] for b in bones]
        self.H0 = np.array([b["head"] for b in bones], float)

    def fk(self, q, mv):
        """Each bone's world turn G and head X from local turns q (name -> 3x3) and moves mv (name -> 3)."""
        nb = len(self.names)
        G = np.zeros((nb, 3, 3))
        X = np.zeros((nb, 3))
        for i, n in enumerate(self.names):
            p = self.parent[i]
            Q = q.get(n, np.eye(3))
            m = mv.get(n, np.zeros(3))
            if p is None:
                G[i] = Q
                X[i] = self.H0[i] + m
            else:
                G[i] = G[p] @ Q
                X[i] = X[p] + G[p] @ (self.H0[i] - self.H0[p] + m)
        return G, X


def clip_pose(clip, t):
    """A json clip's local turns and moves at time t."""
    q, mv = {}, {}
    for tr in clip["tracks"]:
        n = tr["part_name"]
        if tr.get("rot"):
            q[n] = q2m(key_at(tr["rot"], t, True))
        if tr.get("move"):
            mv[n] = key_at(tr["move"], t, False)
    return q, mv


def sole_z(sk, G, X, side):
    """soldier.mo_events' sole: the lower of the heel and the ball's sole, both on the foot's turn."""
    fb, bb = sk.ix["foot_" + side], sk.ix["ball_" + side]
    heel = sk.H0[fb] + np.array(HEEL)
    heel[2] = 0.0
    sole_b = sk.H0[bb].copy()
    sole_b[2] = 0.0
    zh = (X[fb] + G[fb] @ (heel - sk.H0[fb]))[2]
    zb = (X[fb] + G[fb] @ (sole_b - sk.H0[fb]))[2]
    return min(zh, zb)


def onsets(z, tol=0.012, up=0.035):
    """soldier.mo_events' footfalls on a loop: down when within tol of its lowest, up again only when `up` clear."""
    n = len(z)
    base = z.min()
    on, out = bool(z[0] < base + tol), []
    for i in range(2 * n):
        f = i % n
        if not on and z[f] < base + tol:
            on = True
            if i >= n:
                out.append(f)
        elif on and z[f] > base + up:
            on = False
    return sorted(set(out))


def feet(sk, poses, long):
    """Per foot: its sole's height over the loop (the samples given), the footfalls (s), and the lowest."""
    n = len(poses)
    out = {}
    for s in "lr":
        z = np.array([sole_z(sk, G, X, s) for G, X in poses])
        out[s] = {"z": z, "on": [round(f * long / n, 3) for f in onsets(z)], "low": float(z.min()),
                  "ground": float(np.percentile(z, 10))}
    return out


def left_foot(ft, long):
    on = ft["l"]["on"]
    return (on[0] / long) if on else 0.0


# ================================================================================================ one file
def plan(D, base, src, log=print):
    """What his own clip `base` adds to its capture `src`: the trunk's mean turns, the pelvis's offset, his legs'
    scale, free arms or carried, and the bone each root-child (the weapon) rides."""
    sk = Skel(D["bones"])
    C = D["clips"][base]
    tracks = {tr["part_name"]: tr for tr in C["tracks"]}
    long = float(C["long"])
    if abs(long - src.long) > 1e-3:
        raise RuntimeError(f"{base}: {long} s, its capture {src.long} s: not the capture it was cut from")
    for n in TRUNK + LEGS:
        if n in sk.ix and src.parent.get(n) != sk.pname[sk.ix[n]]:
            raise RuntimeError(f"{n}: parent {sk.pname[sk.ix[n]]} here, {src.parent.get(n)} in the capture")
    ts = [k[0] for k in next(tr for tr in C["tracks"] if tr.get("rot"))["rot"]]
    k = float(C["stride"]) / src.stride
    P = {"sk": sk, "C": C, "tracks": tracks, "long": long, "k": k, "delta": {}, "base": base}
    # the trunk's mean turn over the capture's (his carry's), and how far the legs are from the capture (none)
    legs_off = 0.0
    for n in TRUNK + LEGS:
        tr = tracks.get(n)
        if not tr or not tr.get("rot"):
            continue
        p = sk.pname[sk.ix[n]]
        qs, offs = [], []
        for t in ts:
            S = src.local(n, p, t * FPS)
            F = q2m(key_at(tr["rot"], t, True))
            qs.append(m2q(S.T @ F))
            offs.append(ang_deg(S, F))
        if n in TRUNK:
            P["delta"][n] = q2m(qmean(qs))
        else:
            legs_off = max(legs_off, max(offs))
    P["legs_off_deg"] = round(legs_off, 2)
    # the pelvis: his travel = k x the capture's + an offset (his own ground)
    tr = tracks["pelvis"]
    d = np.array([key_at(tr["move"], t, False) - k * src.pelvis_at(t * FPS) for t in ts])
    P["pelvis_c"] = d.mean(0)
    P["pelvis_res"] = float(np.abs(d - d.mean(0)).max())
    # free arms (the capture's own) or carried (his)
    offs = []
    for n in ("upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r"):
        tr = tracks.get(n)
        if not tr or not tr.get("rot"):
            continue
        p = sk.pname[sk.ix[n]]
        offs += [ang_deg(src.local(n, p, t * FPS), q2m(key_at(tr["rot"], t, True))) for t in ts]
    P["free_arms"] = bool(offs) and float(np.mean(offs)) < FREE_ARMS_DEG
    P["arms_off_deg"] = round(float(np.mean(offs)), 1) if offs else None
    # his own clip, densely: the feet (where the left foot falls), the weapon's anchor
    dense = np.linspace(0.0, long, int(round(long * FPS)) + 1)[:-1]
    poses = [sk.fk(*clip_pose(C, t)) for t in dense]
    P["feet"] = feet(sk, poses, long)
    P["lf"] = left_foot(P["feet"], long)
    P["anchor"] = {}
    for tr in C["tracks"]:
        n = tr["part_name"]
        i = sk.ix[n]
        if n == "pelvis" or sk.pname[i] != "root":
            continue
        best = None
        for a in ANCHORS:
            j = sk.ix[a]
            rp = np.array([G[j].T @ (X[i] - X[j]) for G, X in poses])
            ra = [G[j].T @ G[i] for G, X in poses]
            mean_rot = q2m(qmean([m2q(r) for r in ra]))
            spread = float(np.linalg.norm(rp - rp.mean(0), axis=1).max()) + 0.002 * max(ang_deg(mean_rot, r) for r in ra)
            if best is None or spread < best[1]:
                best = (a, spread)
        P["anchor"][n] = best[0]
        P.setdefault("anchor_spread", {})[n] = round(best[1], 4)
    log(f"   {base}: legs x{k:.4f}, legs off the capture {P['legs_off_deg']} deg, pelvis offset "
        f"{np.round(P['pelvis_c'], 3).tolist()} (+-{P['pelvis_res']:.3f}), arms "
        f"{'free (the runner' + chr(39) + 's own)' if P['free_arms'] else 'carried (his own)'} ({P['arms_off_deg']} deg), "
        f"left foot at {P['lf']:.3f}, anchors {P['anchor']} {P.get('anchor_spread')}")
    return P


def make_clip(P, cap, style, info, log=print):
    """The new clip: the capture `cap`'s body on his skeleton, his own arms and weapon, keyed at 15 a second."""
    sk, C, tracks = P["sk"], P["C"], P["tracks"]
    k = P["k"]
    long = cap.long
    K = max(2, int(round(long * RATE)))
    ts = [long * j / K for j in range(K + 1)]
    order = [tr["part_name"] for tr in C["tracks"]]
    body = [n for n in order if n in TRUNK + LEGS or (P["free_arms"] and n in ARMS)]

    def body_pose(t, dz=0.0):
        f = t * FPS
        q, mv = {}, {}
        for n in body:
            p = sk.pname[sk.ix[n]]
            L = cap.local(n, p, f)
            if n in P["delta"]:
                L = L @ P["delta"][n]
            q[n] = L
        mv["pelvis"] = k * cap.pelvis_at(f) + P["pelvis_c"] + np.array((0.0, 0.0, dz))
        return q, mv

    # where his feet fall in the new run (the body alone decides it), so his own arms can be played in step
    # (and on his own ground: the soles' tenth percentile over the loop, as mocap.ground_fit sets a capture down)
    dense = [j / FPS for j in range(cap.F - 1)]
    ft = feet(sk, [sk.fk(*body_pose(t)) for t in dense], long)
    dz = 0.5 * (P["feet"]["l"]["ground"] + P["feet"]["r"]["ground"] - ft["l"]["ground"] - ft["r"]["ground"])
    lf = left_foot(ft, long)
    shift = P["lf"] - lf

    def old_t(t):
        u = (t / long + shift) % 1.0
        return u * P["long"]

    keys = {n: {"rot": [], "move": []} for n in order}
    for t in ts:
        q, mv = body_pose(t, dz)
        tq, tm = clip_pose(C, old_t(t))
        for n in order:
            if n not in q and n in tq and sk.pname[sk.ix[n]] != "root":
                q[n] = tq[n]
            if n != "pelvis" and n in tm and sk.pname[sk.ix[n]] != "root":
                mv[n] = tm[n]
        # the root's children (the weapon) ride the bone they ride in his own clip, as they ride it there
        Gn, Xn = sk.fk(q, mv)
        Go, Xo = sk.fk(tq, tm)
        for n, a in P["anchor"].items():
            i, j = sk.ix[n], sk.ix[a]
            Gw = Gn[j] @ Go[j].T @ Go[i]
            Xw = Xn[j] + Gn[j] @ Go[j].T @ (Xo[i] - Xo[j])
            r = sk.ix["root"]
            q[n] = Gw
            mv[n] = Xw - sk.H0[r] - (sk.H0[i] - sk.H0[r])
        for n in order:
            tr = tracks[n]
            if tr.get("rot"):
                v = m2q(q.get(n, np.eye(3)))
                prev = keys[n]["rot"][-1][1:] if keys[n]["rot"] else None
                if prev is not None and np.dot(prev, v) < 0:
                    v = -v
                keys[n]["rot"].append([round(t, 4)] + [round(float(x), 5) for x in v])
            if tr.get("move"):
                keys[n]["move"].append([round(t, 4)] + [round(float(x), 4) for x in mv.get(n, np.zeros(3))])
    # the loop closes exactly
    for n in order:
        for kind in ("rot", "move"):
            ks = keys[n][kind]
            if ks:
                v = ks[0][1:]
                if kind == "rot" and np.dot(ks[-1][1:], v) < 0:
                    v = [-x for x in v]
                ks[-1] = [ks[-1][0]] + list(v)
    out_tracks = []
    for tr in C["tracks"]:
        n = tr["part_name"]
        o = {kk: vv for kk, vv in tr.items() if kk not in ("rot", "move")}
        if tr.get("rot"):
            o["rot"] = keys[n]["rot"]
        if tr.get("move"):
            o["move"] = keys[n]["move"]
        out_tracks.append(o)
    evs = [dict(e) for e in C.get("events", []) if e.get("fx") != "footstep"]
    for s in "lr":
        for t in ft[s]["on"]:
            evs.append({"t": round(t, 3), "fx": "footstep", "socket": None, "cue": "footstep", "bone": "foot_" + s})
    evs.sort(key=lambda e: e["t"])
    b = info["build"]
    # (set down exactly on his own ground as the keys play it: the pelvis and what rides the root, lowered together)
    probe = {"tracks": out_tracks}
    nn = int(round(long * FPS))
    fk = feet(sk, [sk.fk(*clip_pose(probe, long * j / nn)) for j in range(nn)], long)
    dz2 = 0.5 * (P["feet"]["l"]["ground"] + P["feet"]["r"]["ground"] - fk["l"]["ground"] - fk["r"]["ground"])
    for tr in out_tracks:
        if tr.get("move") and (tr["part_name"] == "pelvis" or sk.pname[sk.ix[tr["part_name"]]] == "root"):
            for k_ in tr["move"]:
                k_[3] = round(k_[3] + dz2, 4)
    dz += dz2
    clip = {"long": long, "loop": True, "stride": round(k * cap.stride, 3), "weapon": C.get("weapon"),
            "tracks": out_tracks, "events": evs, "style_of": P["base"],
            "from": f"motion capture: run_styles.py's {style} (CMU {b['take']} frames {b['a']}-{b['b']}), with this "
                    f"file's own {'free arms' if P['free_arms'] else 'hold on his weapon'} from his {P['base']}"}
    log(f"   {style}: {long:.3f} s, stride {clip['stride']} m, {len(ts)} keys, ground {dz * 1000:+.1f} mm, left foot "
        f"{lf:.3f} (his {P['lf']:.3f}), footfalls l {ft['l']['on']} r {ft['r']['on']}")
    return clip


def styles_for(D, data, log=print):
    rest = data["_rest"]
    out = {}
    plans = {}
    for name, base, style in NEW:
        if base not in D.get("clips", {}):
            continue
        if base not in plans:
            plans[base] = plan(D, base, Capture(data[SOURCE[base]], rest), log=log)
        out[name] = make_clip(plans[base], Capture(data[style], rest), style, data[style], log=log)
    return out


# ================================================================================================ checks
def check(D, log=print):
    """Each style against its own clip: lists whole, keys in order, turns whole, the loop closed, footfalls both feet,
    the soles on his own ground, the weapon where his hands have it."""
    sk = Skel(D["bones"])
    bad = []
    for name, base, style in NEW:
        c = D["clips"].get(name)
        if base not in D["clips"]:
            continue
        if c is None:
            bad.append(f"{name}: missing")
            continue
        C = D["clips"][base]
        if c.get("style_of") != base:
            bad.append(f"{name}: style_of {c.get('style_of')}")
        if [t["part"] for t in c["tracks"]] != [t["part"] for t in C["tracks"]]:
            bad.append(f"{name}: tracks differ from {base}'s")
        for tr in c["tracks"]:
            for kind in ("rot", "move"):
                ks = tr.get(kind)
                if not ks:
                    continue
                ts = [k_[0] for k_ in ks]
                if ts[0] != 0.0 or abs(ts[-1] - c["long"]) > 1e-3 or any(b_ <= a_ for a_, b_ in zip(ts, ts[1:])):
                    bad.append(f"{name} {tr['part_name']} {kind}: times")
                if kind == "rot" and any(abs(np.linalg.norm(k_[1:]) - 1.0) > 2e-4 for k_ in ks):
                    bad.append(f"{name} {tr['part_name']}: turn not whole")
                if np.abs(np.array(ks[0][1:]) - np.array(ks[-1][1:])).max() > 1e-4:
                    bad.append(f"{name} {tr['part_name']} {kind}: loop open")
        feet_on = {e["bone"] for e in c["events"] if e["fx"] == "footstep"}
        if feet_on != {"foot_l", "foot_r"}:
            bad.append(f"{name}: footfalls {sorted(feet_on)}")
        no, nn = int(round(C["long"] * FPS)), int(round(c["long"] * FPS))
        po = [sk.fk(*clip_pose(C, C["long"] * j / no)) for j in range(no)]
        pn = [sk.fk(*clip_pose(c, c["long"] * j / nn)) for j in range(nn)]
        fo, fn = feet(sk, po, C["long"]), feet(sk, pn, c["long"])
        lo_o = min(fo["l"]["low"], fo["r"]["low"])
        lo_n = min(fn["l"]["low"], fn["r"]["low"])
        g_o = 0.5 * (fo["l"]["ground"] + fo["r"]["ground"])
        g_n = 0.5 * (fn["l"]["ground"] + fn["r"]["ground"])
        if abs(g_n - g_o) > 0.003:
            bad.append(f"{name}: stands {1000 * (g_n - g_o):+.1f} mm off his own run's ground")
        if lo_n < lo_o - 0.015:
            bad.append(f"{name}: a sole {1000 * (lo_o - lo_n):.1f} mm under his own run's lowest")
        # the hands on the weapon: the left hand's place on the weapon as in his own clip
        w, hl = sk.ix["weapon"], sk.ix["hand_l"]
        rel_o = np.array([G[w].T @ (X[hl] - X[w]) for G, X in po])
        rel_n = np.array([G[w].T @ (X[hl] - X[w]) for G, X in pn])
        drift = float(np.abs(rel_n.mean(0) - rel_o.mean(0)).max())
        if C.get("weapon") not in (None, "none") and drift > 0.01:
            bad.append(f"{name}: left hand {drift * 1000:.0f} mm off its place on the weapon")
        log(f"   {name}: ground {1000 * (g_n - g_o):+.1f} mm, lowest sole {1000 * (lo_n - lo_o):+.1f} mm, left hand on "
            f"the weapon {drift * 1000:.1f} mm")
    return bad


# ================================================================================================ the shared files
def targets(names, folder=SHARED):
    out = []
    for f in sorted(os.listdir(folder)):
        if not (f.startswith("soldier_") and f.endswith(".json")) or f.endswith("_gore.json"):
            continue
        n = f[:-5]
        if names and n not in names:
            continue
        out.append(n)
    return out


def stage(dir_, names, log=print):
    data = json.load(open(CLIPS))
    os.makedirs(dir_, exist_ok=True)
    report = {}
    for n in targets(names):
        D = json.load(open(os.path.join(SHARED, n + ".json")))
        if "run" not in D.get("clips", {}):
            continue
        log(n)
        new = styles_for(D, data, log=log)
        D["clips"].update(new)
        bad = check(D, log=log)
        report[n] = bad
        with open(os.path.join(dir_, n + ".run_styles.json"), "w") as f:
            json.dump(new, f, separators=(",", ":"))
        log(f"   {'PASS' if not bad else 'FAIL ' + '; '.join(bad)}")
    return report


ABOUT = ("run_styles.py (release 2, 23 November 2026): run_b, run_c, run_d and advance_b, advance_c, advance_d, each "
         "another runner's motion capture (CMU 09_02, 16_36, 09_03) with this file's own hold on his weapon; "
         "style_of names the clip each is a style of. The game gives each man one of the four (LandMen, the "
         "run-styles switch); far men banked in poses play the base clip's.")


def deliver(names, log=print, folder=SHARED, backup=True):
    data = json.load(open(CLIPS))
    stamp = time.strftime("%Y%m%dT%H%M%S")
    done = []
    for n in targets(names, folder):
        path = os.path.join(folder, n + ".json")
        D = json.load(open(path))
        if "run" not in D.get("clips", {}):
            continue
        log(n)
        if backup:
            os.makedirs(BACKUP, exist_ok=True)
            shutil.copy2(path, os.path.join(BACKUP, f"{n}.{stamp}.json"))
        new = styles_for(D, data, log=log)
        for k_ in [k_ for k_, v in D["clips"].items() if v.get("style_of")]:
            del D["clips"][k_]
        D["clips"].update(new)
        bad = check(D, log=log)
        if bad:
            raise RuntimeError(f"{n}: " + "; ".join(bad))
        D["about_run_styles"] = ABOUT
        tmp = path + ".run_styles.part"
        with open(tmp, "w") as f:
            json.dump(D, f, separators=(",", ":"))
        os.replace(tmp, path)
        done.append(n)
    if folder == SHARED:
        time.sleep(10)          # (the shared folder is a network mount: read back after it settles)
    for n in done:
        D = json.load(open(os.path.join(folder, n + ".json")))
        bad = check(D, log=lambda *_: None)
        log(f"{n}: re-read {'PASS' if not bad else 'FAIL ' + '; '.join(bad)}")
    return done


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "--stage":
        rep = stage(a[1], a[2:])
        print("PASS" if not any(rep.values()) else "FAIL", {k: v for k, v in rep.items() if v})
    elif a and a[0] == "--deliver":
        deliver(a[1:])
    elif a and a[0] == "--into":
        deliver(a[2:], folder=a[1], backup=False)
    elif a and a[0] == "--check":
        bad = {}
        for n in targets(a[1:]):
            D = json.load(open(os.path.join(SHARED, n + ".json")))
            if "run" in D.get("clips", {}):
                print(n)
                bad[n] = check(D)
        print("PASS" if not any(bad.values()) else "FAIL", {k: v for k, v in bad.items() if v})
    else:
        print(__doc__)
