"""The tank commander at his roof gun, posed again so it reads from the game's camera (release 2).

Brian, 25 November, a screenshot of a Sherman from behind and above: her commander up on his seat at the .50, hunched
over it, his fists at his chest and his elbows tucked down in front of him. From behind his back hid his arms and all of
the gun but its barrel, which stood up beside his head: it read as a man holding a rifle up across his face. So the
American is posed again, at this file's level (the clips' keys; no mesh is touched, and tank_crew.py's Blender chain is
not needed):

- commander_roof_mg and commander_roof_mg_burst: up on his seat as before (his feet 0.72 under the socket, the rim at
  his belt), 6 cm aft in the ring so the gun stands clear ahead of him, upright (the old lean taken out), his head up
  and back from the gun (29 cm from the grips at the nearest), both arms out ahead to the .50's spade grips at his
  chest's height with his elbows out wide and nearly level, his shoulders round the gun. From behind and above his
  elbows show either side of his back and his head is clear of the receiver.
- The body's own life (breathing, sway, the looks left and right; the burst's jolts) is the old clips', carried over.
- `places`: the Sherman's root_offset becomes ROOT (the game reads it from the clip).

    python3 commander_poses.py --build [--out DIR]     the tanker's file with the two clips re-keyed, staged; checks
    python3 commander_poses.py --deliver [--content]   staged file to the shared folder (and Content/Models/Soldiers)
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
import field_clips as fc  # noqa: E402
import gore  # noqa: E402

SHARED = "/mnt/project-files/battle-models/land/soldier/"
CONTENT = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Models", "Soldiers"))
TANKS = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Models", "Battle"))
STAGE = "/home/user/art/commander/out/"
BACKUP = "/home/user/art/commander/backup/"
FILE = "soldier_USA_tanker"
TANK = "Sherman"

ROOT = (-0.06, 0.0, -0.72)        # his feet from the commander socket: up on his seat as before, 6 cm aft
LEAN = {"spine_02": -2.0, "chest": -3.0}                    # (degrees forward: the old hunch over the gun taken out)
HEAD = {"neck": 2.0, "head": 2.0}                           # (his head down a little by what he straightened: over the gun)
SHRUG = 4.0                       # (his shoulders forward round the gun, degrees about the upright)
POLE = {"l": (-0.15, -1.0, -0.15), "r": (-0.15, 1.0, -0.15)}  # (each elbow from its shoulder: out wide, a little down)
HAND_IN = 0.2                     # (each fist's knuckles ahead and this much in toward the gun's middle)
CURL, THUMB = 66.0, 20.0
if os.environ.get("CMDR_TRY"):      # (trials: "x z lean_spine lean_chest head pole_back pole_down shrug")
    _v = [float(x) for x in os.environ["CMDR_TRY"].split()]
    ROOT = (_v[0], 0.0, _v[1])
    LEAN = {"spine_02": _v[2], "chest": _v[3]}
    HEAD = {"neck": _v[4], "head": _v[4]}
    POLE = {"l": (_v[5], -1.0, _v[6]), "r": (_v[5], 1.0, _v[6])}
    SHRUG = _v[7]
RING = 0.300                      # the Sherman's hatch ring: inside radius (m), from the socket's axis
RIM_OVER = 0.25                   # the rim over the socket
ROUNDS = [round(0.12 * k, 3) for k in range(6)]


def unit(v):
    v = np.asarray(v, float)
    return v / max(float(np.linalg.norm(v)), 1e-12)


def tank_sockets():
    d = json.load(open(os.path.join(TANKS, TANK + ".json")))
    return {s["name"]: np.array(s["at"], float) for s in d["sockets"]}


def jolt(t, rounds, decay=0.04):
    return sum(math.exp(-(t - r) / decay) for r in rounds if t >= r)


def key_times(clip):
    for tr in clip["tracks"]:
        if "rot" in tr:
            return [k[0] for k in tr["rot"]]
    raise ValueError("no keys")


def turn_world(man, lq, mv, bone, R):
    """Bone (and all it carries) turned by R in the root's frame, about its own joint."""
    G, _ = man.fk(lq, mv)
    i = man.b(bone)
    p = man.par[i]
    lq[i] = G[p].T @ R @ G[p] @ lq[i]
    return lq


def posed(man, clip, t, grips, j=0.0):
    """One key: the old clip's body at t, leant into the gun, his head up, his arms out to the grips."""
    lq, mv = man.locals_of(clip, t)
    y = np.array((0.0, 1.0, 0.0))
    z = np.array((0.0, 0.0, 1.0))
    for b, deg in LEAN.items():
        lq = turn_world(man, lq, mv, b, fc.R_axis(y, deg))
    for b, deg in HEAD.items():
        lq = turn_world(man, lq, mv, b, fc.R_axis(y, deg))
    # (his shoulders round the gun: each clavicle forward about the upright)
    lq = turn_world(man, lq, mv, "clavicle_l", fc.R_axis(z, SHRUG))
    lq = turn_world(man, lq, mv, "clavicle_r", fc.R_axis(z, -SHRUG))
    miss = {}
    for s, sg in (("l", 1.0), ("r", -1.0)):
        d = unit((1.0, HAND_IN * sg, 0.0))
        g = grips[s] + np.array((-0.004 * j, 0.0, 0.0))
        T = man.wrist_for(s, g, d, z)
        lq = man.arm(lq, mv, s, T, POLE[s], d, z, share=0.5)
        lq = man.fingers(lq, s, CURL, THUMB)
        G, X = man.fk(lq, mv)
        miss[s] = float(np.linalg.norm(man.grip(G, X, s) - g))
    return lq, mv, miss


def bake(man, clip, grips, rounds=None):
    ts = key_times(clip)
    rot = {i: [] for i in range(man.nb) if man.par[i] is not None}
    prev = {}
    worst = 0.0
    for t in ts:
        j = jolt(t, rounds) if rounds else 0.0
        lq, mv, miss = posed(man, clip, t, grips, j)
        worst = max(worst, max(miss.values()))
        for i in rot:
            q = fc.quat(lq[i])
            if i in prev and float(q @ prev[i]) < 0:
                q = -q
            prev[i] = q
            rot[i].append([round(t, 4)] + [round(float(v), 5) for v in q])
    out = dict(clip)
    tracks = []
    old = {tr["part"]: tr for tr in clip["tracks"]}
    for i in range(man.nb):
        if man.par[i] is None:
            continue
        tr = {"part": i, "part_name": man.rig.names[i]}
        if not all(abs(k[1] - 1.0) < 1e-4 for k in rot[i]):
            tr["rot"] = rot[i]
        if i in old and "move" in old[i]:
            tr["move"] = old[i]["move"]
        if len(tr) > 2:
            tracks.append(tr)
    out["tracks"] = tracks
    return out, worst


def skinned(man, clip, t, level="near"):
    M = man.D[level]
    P = np.array(M["points"], float).reshape(-1, 3)
    a, b, w = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
    G, X = gore.pose(man.rig, clip, t)
    Wa = X[a] + np.einsum("nij,nj->ni", G[a], P - man.H[a])
    Wb = X[b] + np.einsum("nij,nj->ni", G[b], P - man.H[b])
    gun = np.isin(a, [man.b(n) for n in ("weapon", "bolt", "bipod") if n in man.ix])
    return (Wa * (1 - w)[:, None] + Wb * w[:, None])[~gun], a[~gun], (G, X)


def checks(man, clip, root, socks, log=print):
    """His fists on the grips, his body inside the ring where it passes through it, his arms over the rim, his face
    clear of the gun, and how he stands to it (for the README)."""
    o = socks["commander"] + np.array(root)
    rim = socks["commander"][2] + RIM_OVER - o[2]            # (the rim over his feet)
    deepest, low_arm, face = 0.0, 9.0, 9.0
    gb = socks["grip.roof_mg.l"] * 0.5 + socks["grip.roof_mg.r"] * 0.5 - o
    for t in key_times(clip)[::3]:
        W, a, (G, X) = skinned(man, clip, t)
        r = np.hypot(W[:, 0] + root[0], W[:, 1] + root[1])     # (from the socket's axis)
        band = (W[:, 2] > rim - 0.055) & (W[:, 2] < rim + 0.005)
        if band.any():
            deepest = max(deepest, float((r[band] - RING).max()))
        arms = np.isin(a, [man.b(n + s) for n in ("upperarm_", "lowerarm_", "hand_") for s in "lr"])
        out = arms & (r > RING)
        if out.any():
            low_arm = min(low_arm, float(W[out, 2].min() - rim))
        head = np.isin(a, [man.b("head")])
        face = min(face, float(np.linalg.norm(W[head] - gb, axis=1).min()))
    G, X = gore.pose(man.rig, clip, 0.0)
    sh = 0.5 * (X[man.b("upperarm_l")] + X[man.b("upperarm_r")])
    log(f"   ring: his body {deepest * 100:+.1f} cm past its inside wall at most; his arms {low_arm * 100:.1f} cm over the rim "
        f"at the lowest where they cross it; his head {face * 100:.0f} cm from the grips at the nearest")
    log(f"   the rim {rim:.2f} m over his feet; the grips {gb[2]:.2f} m over his feet, {gb[0]:.2f} m ahead of his root; his "
        f"shoulders {sh[2]:.2f} m up, {sh[0]:+.2f} m; head top {skinned(man, clip, 0.0)[0][:, 2].max():.2f} m")
    return deepest, low_arm, face


def build(out_dir=STAGE, log=print):
    D = json.load(open(SHARED + FILE + ".json"))
    man = fc.Man(D)
    socks = tank_sockets()
    o = socks["commander"] + np.array(ROOT)
    grips = {s: socks["grip.roof_mg." + s] - o for s in ("l", "r")}
    faults = []
    for name, rounds in (("commander_roof_mg", None), ("commander_roof_mg_burst", ROUNDS)):
        c = D["clips"][name]
        c2, worst = bake(man, c, grips, rounds)
        c2["places"] = [dict(p, root_offset=[float(v) for v in ROOT]) if p.get("piece") == TANK else p for p in c["places"]]
        c2["about"] = ("The American commander up on his seat in the hatch, the rim at his belt, upright behind the .50 "
                       "on its post, both arms out ahead to its spade grips at his chest's height, his elbows out wide, "
                       "his head up and back from the gun, looking over it" +
                       (": firing it, each of six rounds jolting his shoulders and head." if rounds else ": watching over it, ready."))
        c2["from"] = (c.get("from", "") + "; commander_poses.py: upright and 6 cm aft, his arms out to the grips with the "
                      "elbows wide, for the game's camera (Brian's screenshot, 25 November)").strip("; ")
        D["clips"][name] = c2
        log(f"{name}: {len(key_times(c2))} keys, fists on the grips within {worst * 1000:.1f} mm")
        deep, low, face = checks(man, c2, ROOT, socks, log)
        if worst > 0.004:
            faults.append(f"{name}: a fist {worst * 1000:.1f} mm off its grip")
        if deep > 0.03:
            faults.append(f"{name}: his body {deep * 100:.1f} cm into the ring")
        if low < 0.005:
            faults.append(f"{name}: an arm {low * 100:.1f} cm over the rim (through it)")
        if face < 0.25:
            faults.append(f"{name}: his head {face * 100:.0f} cm from the grips")
    tc = D.get("tank_clips")
    if isinstance(tc, dict):
        tc["roof_gun_us"] = ("commander_roof_mg and its burst on the Sherman (commander_poses.py, 25 November 2026): up on "
                             "his seat (root_offset -0.06, 0, -0.72), upright, arms out to the .50's grips, elbows wide")
    os.makedirs(out_dir, exist_ok=True)
    p = os.path.join(out_dir, FILE + ".json")
    json.dump(D, open(p + ".tmp", "w"), separators=(",", ":"))
    os.replace(p + ".tmp", p)
    log("staged " + p + "; faults: " + ("; ".join(faults) if faults else "none"))
    return faults


def deliver(content=False):
    os.makedirs(BACKUP, exist_ok=True)
    stamp = time.strftime("%m%d_%H%M%S", time.gmtime())
    src = os.path.join(STAGE, FILE + ".json")
    json.load(open(src))
    dests = [SHARED + FILE + ".json"] + ([os.path.join(CONTENT, FILE + ".json")] if content else [])
    for d in dests:
        if os.path.exists(d):
            shutil.copy2(d, os.path.join(BACKUP, os.path.basename(d) + "." + stamp))
        shutil.copyfile(src, d + ".tmp_cmdr")
        os.replace(d + ".tmp_cmdr", d)
        print("wrote", d)
    time.sleep(10)
    X = json.load(open(SHARED + FILE + ".json"))
    print("read back", len(X["clips"]), "clips;", X["clips"]["commander_roof_mg"]["places"])


def main(argv):
    if "--build" in argv:
        out = argv[argv.index("--out") + 1] if "--out" in argv else STAGE
        return 1 if build(out) else 0
    if "--deliver" in argv:
        deliver("--content" in argv)
        return 0
    print(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
