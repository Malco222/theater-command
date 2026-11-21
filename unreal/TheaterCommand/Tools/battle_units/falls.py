"""Deaths for the battle soldiers, baked from a ragdoll (ragdoll.py): every soldier_<ARMY>.json gains a set of
`die_*` clips (falls from standing, running, kneeling, lying, in cover; thrown by a blast; a wounded man crawling then
still) and a `deaths` table the game picks from. numpy only (and PIL for the preview pictures); rom.py (the range-of-
motion checker, as on branch 8egity) beside it for the checks.

Brian (21 November 2026): "when a soldier gets hit have their body be on the ground dead" / "i want it to be
realistic". The game plays a death clip once and keeps its last frame for the rest of the battle (the ground battle
scene's thread draws him there). Each clip here is a physics fall: the man's own body, his joints' real ranges, the
muscle tone going out of him over a fraction of a second, the ground under him; the rifle leaves his hand and lands
where it lands. Nothing is key-framed but the pose he was hit in.

    python falls.py [--armies GER,SOV,...] [--dir battle-models/land/soldier] [--only clips,pictures] [--quick names]

Re-run after any soldier re-export: it strips the clips it added (and the table) and adds them again; the output is
deterministic (every variant has its own fixed draw).
"""

import argparse
import json
import math
import os
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import ragdoll as RG  # noqa: E402

PAINT_ORDER = ("tunic", "trousers", "boots", "lower", "straps", "helmet", "pack", "skin", "wood", "steel", "coat",
               "coat_dark", "leather", "blanket", "hoof", "medic_white", "medic_red", "rank", "rocket")
LOOK = {"tunic": "5F665C", "trousers": "585E57", "boots": "2A2420", "lower": "2C2824", "straps": "2E2620",
        "helmet": "4F5650", "pack": "6E6852", "skin": "BDA08E", "wood": "6B4429", "steel": "34363A",
        "coat": "5F665C", "coat_dark": "4A5048", "leather": "3A2E25", "blanket": "564A3E", "hoof": "2A2420"}


# ------------------------------------------------------------------------------------------------ the clip
def frame_local(rd, R, H):
    """One frame of a Fall (part turns R, heads H) as the file's local turns: {bone: (q, move or None)}."""
    m = rd.man
    names = m.names
    Gw = np.zeros((len(names), 3, 3))
    for b, n in enumerate(names):
        Gw[b] = R[rd.bone_part[n]] @ rd.Gr[b] if n in rd.bone_part else np.eye(3)   # (root; a bag bone: set later)
    loc = {}
    for b, n in enumerate(names):
        if n == "root" or n not in rd.bone_part:
            continue                                  # (a bag bone is placed by clip_from, from where it was packed)
        mv = H[rd.pix[n]] - m.head[b] if n in ("pelvis", "weapon") else None
        loc[n] = (RG.matq(Gw[m.parent[b]].T @ Gw[b]), mv)
    return loc


def bag_rel(man, clip, t, bag):
    """Where each bag bone sits on its host (bone: host) in the pose he was hit in: (turn, offset) in the host's
    frame. The bazooka loader's rocket rides the weapon bone in the file but is packed in the bag on his back."""
    Gw, X = man.fk(man.local_at(clip, t))
    return {b: (h, Gw[man.i(h)].T @ Gw[man.i(b)], Gw[man.i(h)].T @ (X[man.i(b)] - X[man.i(h)])) for b, h in bag.items()}


def bag_place(man, loc, rel):
    """One frame's locals with each bag bone set (turn and move on its own parent) to stay where it was packed."""
    loc = dict(loc)
    Gw, X = man.fk(loc)
    for b, (h, Rr, pr) in rel.items():
        ib, ih, ip = man.i(b), man.i(h), man.parent[man.i(b)]
        G = Gw[ih] @ Rr
        Xb = X[ih] + Gw[ih] @ pr
        loc[b] = (RG.matq(Gw[ip].T @ G), Gw[ip].T @ (Xb - X[ip]) - (man.head[ib] - man.head[ip]))
    return loc


def clip_from(man, frames, note, events):
    """Frames [(t, {bone: (q, move)})] as the file's clip: a track for every bone that turns, the pelvis's and the
    rifle's moves (and a bag bone's: see bag_place), events, end_offset."""
    rel = getattr(man, "bag_rel", None)
    tr = {}
    for (t, loc) in frames:
        if rel:
            loc = bag_place(man, loc, rel)
        for n, (q, mv) in loc.items():
            e = tr.setdefault(n, {"rot": [], "move": []})
            e["rot"].append([round(t, 4)] + [round(float(v), 5) for v in q])
            if mv is not None:
                e["move"].append([round(t, 4)] + [round(float(v), 4) for v in mv])
    tracks = []
    for b, n in enumerate(man.names):
        if n not in tr:
            continue
        rot = tr[n]["rot"]
        if all(abs(abs(r[1]) - 1.0) < 1e-5 for r in rot) and not tr[n]["move"]:
            continue                                  # (a bone that never turns from rest needs no track)
        e = {"part": b, "part_name": n, "rot": _thin(rot)}
        if tr[n]["move"]:
            e["move"] = _thin(tr[n]["move"])
        tracks.append(e)
    end = frames[-1][1]["pelvis"][1]
    clip = {"long": round(frames[-1][0], 4), "loop": False, "stride": 0.0, "weapon": "rifle", "tracks": tracks,
            "events": sorted(events, key=lambda e: e["t"]), "from": note,
            "end_offset": [round(float(v), 3) for v in end]}
    if rel:
        bag_dense(man, clip, rel)
    ch = getattr(man, "chain", None)
    if ch and all(b in man.ix for b in ch):
        chain_drape(man, clip, ch)
    return clip


WHIP_R = 0.012                   # (how far the whip's line keeps off the ground: its half-thickness)
WHIP_SAG = math.radians(12.0)     # (how far each joint of the whip bends down where the whip lies level)


def chain_drape(man, clip, chain, fps=None):
    """A chain of bones off the body, base first (the radio man's whip antenna, whip_1..whip_4 off the chest),
    keyed in a death from the pose the game draws. Each piece goes on from the one before; where the whip lies
    level each joint sags a little (by the square of how level it lies, so the standing whip is straight); and a
    piece that would end under the ground bends down onto it and the pieces after lie along it, as a steel whip
    does. Out of the physics (it is too light to move him)."""
    idx = [man.i(b) for b in chain]
    A, L = [], []
    for k, j in enumerate(idx):
        if k + 1 < len(idx):
            e = man.head[idx[k + 1]]
        else:                                     # (the last piece: to its farthest point)
            q = man.P[man.part == j]
            back = man.head[j] - man.head[idx[k - 1]] if k else np.array((0.0, 0.0, 1.0))
            e = q[int(np.argmax((q - man.head[j]) @ back))] if len(q) else man.head[j] + back
        a = e - man.head[j]
        L.append(float(np.linalg.norm(a)))
        A.append(a / L[-1])
    up = np.array((0.0, 0.0, 1.0))
    fps = fps or BAG_FPS
    long = clip["long"]
    ts = [min(long, k / fps) for k in range(int(math.ceil(long * fps)) + 1)]
    keys = {b: [] for b in chain}
    for t in ts:
        loc = {n: v for n, v in man.local_at(clip, t).items() if n not in keys}
        Gw, X = man.fk(loc)
        Gp = Gw[man.parent[idx[0]]]
        p = X[idx[0]].copy()
        fwd = Gw[man.parent[idx[0]]] @ np.array((1.0, 0.0, 0.0))
        for k, b in enumerate(chain):
            d0 = Gp @ A[k]
            d = d0.copy()
            h = d - d[2] * up
            hn = float(np.linalg.norm(h))
            if k and hn > 1e-6:
                ax = np.cross(d, up)
                d = RG.axis_angle(ax / np.linalg.norm(ax), -WHIP_SAG * hn * hn) @ d
            if p[2] + L[k] * d[2] < WHIP_R:
                vz = float(np.clip((WHIP_R - p[2]) / L[k], -1.0, 1.0))
                hd = d - d[2] * up
                if np.linalg.norm(hd) < 1e-6:
                    hd = fwd - fwd[2] * up
                hd = hd / np.linalg.norm(hd)
                d = hd * math.sqrt(1.0 - vz * vz) + up * vz
            G = RG.arc(d0, d) @ Gp
            q = RG.matq(Gp.T @ G)
            if keys[b] and float(np.dot(q, keys[b][-1][1:])) < 0.0:
                q = -q
            keys[b].append([round(t, 4)] + [round(float(v), 5) for v in q])
            Gp = G
            p = p + L[k] * d
    have = {tr["part_name"]: tr for tr in clip["tracks"]}
    for b, j in zip(chain, idx):
        if b in have:
            have[b]["rot"] = _thin(keys[b])
            have[b].pop("move", None)
        else:
            clip["tracks"].append({"part": j, "part_name": b, "rot": _thin(keys[b])})


BAG_FPS = 120


def bag_dense(man, clip, rel, fps=BAG_FPS):
    """A bag bone's keys again, placed from the pose the game will draw between the 30 fps keys. The bag bone hangs
    off the flying weapon, so where the weapon tumbles fast the slerped weapon and the slerped chest part company
    between keys (up to 14 cm for the loader's rocket in die_thrown_b); keyed at 120 fps from that drawn pose it
    stays inside the bag."""
    long = clip["long"]
    ts = [min(long, k / fps) for k in range(int(math.ceil(long * fps)) + 1)]
    keys = {b: {"rot": [], "move": []} for b in rel}
    for t in ts:
        loc = {n: v for n, v in man.local_at(clip, t).items() if n not in rel}
        loc = bag_place(man, loc, rel)
        for b in rel:
            q, mv = loc[b]
            q = np.asarray(q, float)
            if keys[b]["rot"] and float(np.dot(q, keys[b]["rot"][-1][1:])) < 0.0:
                q = -q                                # (one hemisphere, so the thinning sees a smooth track)
            keys[b]["rot"].append([round(t, 4)] + [round(float(v), 5) for v in q])
            keys[b]["move"].append([round(t, 4)] + [round(float(v), 4) for v in mv])
    for tr in clip["tracks"]:
        if tr["part_name"] in keys:
            tr["rot"] = _thin(keys[tr["part_name"]]["rot"])
            tr["move"] = _thin(keys[tr["part_name"]]["move"])


def fall_events(f, rd, start=None):
    """Sounds of a fall: a knee down, the body on the ground, the rifle landing (first touch of each)."""
    ev = []
    pre = set(start or ())
    for k, t in sorted(f.contact_t.items(), key=lambda kv: kv[1]):
        n = rd.parts[k]
        if k in pre or t <= 0.02:
            continue
        if n in ("calf_l", "calf_r") and not any(e["fx"] == "knee_down" for e in ev):
            ev.append({"t": round(t, 3), "fx": "knee_down", "socket": None, "cue": "knee_down", "bone": n})
        if n in ("spine_02", "chest", "pelvis") and not any(e["fx"] == "body_fall" for e in ev):
            ev.append({"t": round(t, 3), "fx": "body_fall", "socket": None, "cue": "body_fall", "bone": "spine_02"})
        if n == "weapon" and t >= f.release_at and not any(e["fx"] == "rifle_drop" for e in ev):
            ev.append({"t": round(t, 3), "fx": "rifle_drop", "socket": None, "cue": "kit_rattle"})
    return ev


def clip_of(f, rd, note, start=None):
    """A finished Fall as the file's clip."""
    frames = [(t, frame_local(rd, R, H)) for (t, R, H) in f.rec]
    return clip_from(rd.man, frames, note, fall_events(f, rd, start))


def _thin(keys, tol=2e-4):
    """Drop keys that the keys either side give back by straight interpolation (within tol); keep first and last."""
    if len(keys) <= 2:
        return keys
    out = [keys[0]]
    for i in range(1, len(keys) - 1):
        a, b, c = np.array(out[-1][1:]), np.array(keys[i][1:]), np.array(keys[i + 1][1:])
        ta, tb, tc = out[-1][0], keys[i][0], keys[i + 1][0]
        u = (tb - ta) / (tc - ta)
        if np.max(np.abs(a + (c - a) * u - b)) > tol:
            out.append(keys[i])
    out.append(keys[-1])
    return out


# ------------------------------------------------------------------------------------------------ pictures
def _rgb(hexs, k=1.0):
    return tuple(int(min(255, int(hexs[i:i + 2], 16) * k)) for i in (0, 2, 4))


def render(man, P, size=(480, 300), cam=None, look=None, ground=True, light=(0.4, -0.5, 0.75)):
    """A quick picture of the man's near mesh at points P (skinned): flat-shaded triangles, painter's order."""
    from PIL import Image, ImageDraw
    look = look or LOOK
    W, H = size
    img = Image.new("RGB", size, (196, 190, 170))
    dr = ImageDraw.Draw(img)
    cam = cam or {}
    tgt = np.asarray(cam.get("target", (0.4, 0.0, 0.4)), float)
    yaw, pitch, dist = math.radians(cam.get("yaw", -35.0)), math.radians(cam.get("pitch", 22.0)), cam.get("dist", 4.0)
    eye = tgt + dist * np.array((-math.cos(pitch) * math.cos(yaw), -math.cos(pitch) * math.sin(yaw), math.sin(pitch)))
    fw = (tgt - eye) / np.linalg.norm(tgt - eye)
    rt = np.cross(fw, (0.0, 0.0, 1.0))
    rt /= np.linalg.norm(rt)
    up = np.cross(rt, fw)
    foc = cam.get("focal", 1.6) * W / 2

    def proj(X):
        d = X - eye
        z = d @ fw
        return np.stack([W / 2 + foc * (d @ rt) / z, H / 2 - foc * (d @ up) / z], -1), z
    if ground:
        for g in np.arange(-3.0, 3.01, 0.5):
            for a, b in (((g, -3.0, 0.0), (g, 3.0, 0.0)), ((-3.0, g, 0.0), (3.0, g, 0.0))):
                s, z = proj(np.array((a, b), float))
                if np.all(z > 0.1):
                    dr.line([tuple(s[0]), tuple(s[1])], fill=(170, 164, 146), width=1)
    T = np.array(man.data["near"]["triangles"], int).reshape(-1, 3)
    pnt = np.array(man.data["near"]["paints"], int)
    s, z = proj(P)
    v0, v1, v2 = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    nrm = np.cross(v1 - v0, v2 - v0)
    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-12)
    cen = (v0 + v1 + v2) / 3
    facing = np.einsum("ni,ni->n", nrm, cen - eye)
    L = np.asarray(light, float)
    L /= np.linalg.norm(L)
    sh = np.clip(np.abs(nrm @ L), 0.0, 1.0) * 0.65 + 0.35
    order = np.argsort(-z[T].mean(1))
    cols = [_rgb(look.get(PAINT_ORDER[min(p, len(PAINT_ORDER) - 1)], "777777")) for p in range(len(PAINT_ORDER))]
    for t in order:
        if facing[t] > 0 and False:
            continue
        c = cols[pnt[T[t, 0]] if pnt[T[t, 0]] < len(cols) else 0]
        k = sh[t]
        dr.polygon([tuple(s[T[t, 0]]), tuple(s[T[t, 1]]), tuple(s[T[t, 2]])],
                   fill=(int(c[0] * k), int(c[1] * k), int(c[2] * k)))
    # his shadow's line on the ground (where he lies): the lowest points, darkened
    return img


def frames_of(man, clip, times):
    out = []
    for t in times:
        Gw, X = man.fk(man.local_at(clip, t))
        out.append(man.skin(Gw, X))
    return out


def filmstrip(man, clip, n=6, size=(300, 220), cam=None, label=None, look=None):
    from PIL import Image, ImageDraw
    c = man.clips[clip] if isinstance(clip, str) else clip
    L = c["long"]
    times = [L * i / (n - 1) for i in range(n)]
    Ps = frames_of(man, clip, times)
    ims = [render(man, P, size, cam, look) for P in Ps]
    strip = Image.new("RGB", (size[0] * n, size[1] + 22), (236, 232, 222))
    for i, im in enumerate(ims):
        strip.paste(im, (i * size[0], 22))
    d = ImageDraw.Draw(strip)
    d.text((6, 4), (label or (clip if isinstance(clip, str) else "")) + "   " +
           "   ".join("%.2f s" % t for t in times), fill=(30, 30, 30))
    return strip


# ------------------------------------------------------------------------------------------------ checks
_BODY = {}


def rom_check(man, clip, fps=60.0):
    """rom.py's range-of-motion report of a clip of this file (the mocap animator's limits), in its own terms."""
    import rom
    M = np.diag((1.0, -1.0, 1.0))                     # (the file's y right -> rom's y left)
    key = id(man)
    if key not in _BODY:
        ref = rom.Body.of_rest()
        H0 = {n: M @ man.head[man.i(n)] for n in rom.CORE if n in man.ix}
        _BODY[key] = rom.Body(H0, ref.hands)
    c = man.clips[clip] if isinstance(clip, str) else clip
    n = int(round(c["long"] * fps)) + 1
    D, H = [], []
    for f in range(n):
        Gw, X = man.fk(man.local_at(c, f / fps))
        D.append([M @ Gw[man.i(b)] @ M for b in rom.CORE])
        H.append([M @ X[man.i(b)] for b in rom.CORE])
    P = rom.Poses(np.array(D), np.array(H), fps, rom.CORE)
    return rom.check(P, body=_BODY[key])


def ground_check(man, clip):
    """Where the last frame lies: the lowest point, how many points are under the ground by more than 1 cm. Points
    kept out of his shape (`man.loose`, the radio man's whip) are told apart: `loose_min_z`, `loose_under_1cm`."""
    c = man.clips[clip] if isinstance(clip, str) else clip
    Gw, X = man.fk(man.local_at(c, c["long"] + 1.0))
    P = man.skin(Gw, X)
    lo = getattr(man, "loose", None)
    out = {}
    if lo is not None and lo.any():
        out = {"loose_min_z": round(float(P[lo, 2].min()), 3), "loose_under_1cm": int((P[lo, 2] < -0.01).sum())}
        P = P[~lo]
    out.update({"min_z": round(float(P[:, 2].min()), 4), "under_1cm": int((P[:, 2] < -0.01).sum()),
                "low5": round(float(np.percentile(P[:, 2], 5)), 4)})
    return out


# ------------------------------------------------------------------------------------------------ the deaths
# Tone groups: which joints each recipe's (hold, fade) times apply to.
GROUPS = {"legs": ("thigh", "calf", "foot"), "trunk": ("spine_01", "spine_02", "chest"), "neck": ("neck", "head"),
          "arms": ("upperarm", "lowerarm", "hand")}
LIMP = {"legs": (0.0, 0.1), "trunk": (0.0, 0.12), "neck": (0.0, 0.1), "arms": (0.0, 0.08)}

# Every death: the pose he was hit in (a clip and its time; his travel in m/s forward when he was on the move), the
# blows (battle time, part, change of speed in m/s, x forward, y to his right, z up), how long each group of joints
# keeps its tone and how fast it goes (s), when the rifle leaves his hand and how it is thrown, the ground's grip.
# `mirror` makes a second death with every blow's y turned the other way (falling to the other side).
RECIPES = [
    # ---- standing: hit in the chest from in front, the knees go and he goes over backwards
    dict(name="die_back_a", stance="stand", kind="back", start=("fire_stand", 0.25),
         push=[(0.0, "chest", (-1.5, 0.15, 0.15)), (0.0, "head", (-0.6, 0.0, 0.0))],
         tone={"legs": (0.04, 0.14), "trunk": (0.08, 0.25), "neck": (0.0, 0.15), "arms": (0.02, 0.1)},
         release=0.07, throw=(0.6, 0.4, 0.9)),
    dict(name="die_back_b", stance="stand", kind="back", start=("idle", 1.2),
         push=[(0.0, "chest", (-1.8, -0.2, 0.1)), (0.0, "spine_02", (-0.6, 0.0, 0.0))],
         tone={"legs": (0.12, 0.3), "trunk": (0.05, 0.2), "neck": (0.0, 0.12), "arms": (0.0, 0.1)},
         release=0.12, throw=(0.2, 0.9, 0.5)),
    dict(name="die_back_c", stance="stand", kind="back", start=("advance", 0.2), travel=1.2,
         push=[(0.0, "chest", (-2.4, 0.3, 0.3)), (0.0, "head", (-1.0, 0.2, 0.2))],
         tone={"legs": (0.02, 0.1), "trunk": (0.0, 0.15), "neck": (0.0, 0.1), "arms": (0.0, 0.08)},
         release=0.05, throw=(1.0, -0.2, 1.2)),
    # ---- standing: hit from behind or as he leans in; the knees buckle and he pitches onto his face
    dict(name="die_forward_a", stance="stand", kind="forward", start=("advance", 0.45), travel=2.2,
         push=[(0.0, "spine_02", (0.6, 0.0, 0.0))],
         tone={"legs": (0.02, 0.1), "trunk": (0.0, 0.2), "neck": (0.0, 0.15), "arms": (0.0, 0.12)},
         release=0.1, throw=(1.2, 0.3, 0.6)),
    dict(name="die_forward_b", stance="stand", kind="forward", start=("fire_stand", 0.7),
         push=[(0.0, "chest", (1.3, -0.2, 0.0)), (0.0, "pelvis", (-0.4, 0.0, 0.0))],
         tone={"legs": (0.15, 0.25), "trunk": (0.1, 0.25), "neck": (0.0, 0.1), "arms": (0.05, 0.1)},
         release=0.15, throw=(0.5, 0.5, 0.2)),
    dict(name="die_forward_c", stance="stand", kind="forward", start=("walk", 0.3), travel=1.25,
         push=[(0.0, "chest", (0.5, 0.25, 0.0))],
         tone={"legs": (0.06, 0.2), "trunk": (0.15, 0.3), "neck": (0.05, 0.2), "arms": (0.0, 0.15)},
         release=0.2, throw=(0.3, -0.4, 0.3)),
    # ---- standing: hit in the shoulder or side, he twists and falls over sideways
    dict(name="die_side_a", stance="stand", kind="side", start=("fire_stand", 0.4), mirror="die_side_b",
         push=[(0.0, "chest", (-0.3, 2.0, 0.1)), (0.0, "upperarm_l", (-0.6, 1.4, 0.0))],
         tone={"legs": (0.18, 0.3), "trunk": (0.05, 0.2), "neck": (0.0, 0.12), "arms": (0.0, 0.08)},
         release=0.06, throw=(0.4, 0.8, 0.8)),
    dict(name="die_side_c", stance="stand", kind="side", start=("idle", 2.0), mirror="die_side_d",
         push=[(0.0, "chest", (0.2, -1.2, 0.0)), (0.0, "pelvis", (0.0, 0.4, 0.0))],
         tone={"legs": (0.1, 0.25), "trunk": (0.0, 0.2), "neck": (0.0, 0.12), "arms": (0.0, 0.1)},
         release=0.1, throw=(0.6, -0.6, 0.4)),
    # ---- standing: the legs go first; he drops to his knees, sways, and slumps over
    dict(name="die_knees_a", stance="stand", kind="knees", start=("fire_stand", 0.1),
         push=[(0.0, "spine_01", (0.1, 0.0, -0.2)), (0.55, "chest", (0.35, 0.15, 0.0))],
         tone={"legs": (0.0, 0.06), "trunk": (0.45, 0.4), "neck": (0.3, 0.3), "arms": (0.05, 0.25)},
         release=0.2, throw=(0.4, 0.3, 0.0)),
    dict(name="die_knees_b", stance="stand", kind="knees", start=("idle", 0.4),
         push=[(0.0, "chest", (0.25, 0.0, 0.0)), (0.6, "chest", (0.1, -0.45, 0.0))],
         tone={"legs": (0.0, 0.08), "trunk": (0.55, 0.5), "neck": (0.35, 0.35), "arms": (0.1, 0.3)},
         release=0.3, throw=(0.2, 0.5, 0.0)),
    # ---- standing: hit high on one shoulder, he is spun half round as he drops
    dict(name="die_spin_a", stance="stand", kind="spin", start=("advance", 0.1), travel=1.5, mirror="die_spin_b",
         push=[(0.0, "upperarm_r", (-2.6, 0.4, 0.2)), (0.0, "upperarm_l", (1.4, 0.0, 0.0))],
         tone={"legs": (0.08, 0.2), "trunk": (0.04, 0.2), "neck": (0.0, 0.1), "arms": (0.0, 0.06)},
         release=0.04, throw=(0.3, 1.2, 0.8)),
    # ---- a shell or grenade close by: thrown off his feet
    dict(name="die_thrown_a", stance="stand", kind="thrown", start=("advance", 0.35), travel=2.0, blast=(-4.8, 0.6, 3.2),
         tone=LIMP, release=0.0, throw=(-2.0, 1.5, 3.0), spin=(4.0, -6.0, 2.0), mu=0.6),
    dict(name="die_thrown_b", stance="stand", kind="thrown", start=("fire_stand", 0.5), blast=(-1.2, 4.2, 2.6),
         tone=LIMP, release=0.0, throw=(0.5, 3.0, 2.5), spin=(-5.0, 2.0, 6.0), mu=0.6),
    dict(name="die_thrown_c", stance="stand", kind="thrown", start=("idle", 1.6), blast=(3.6, -1.0, 3.6),
         tone=LIMP, release=0.0, throw=(2.5, -1.0, 3.5), spin=(3.0, 5.0, -3.0), mu=0.6),
    # ---- running: he pitches forward at full tilt and slides
    dict(name="die_run_a", stance="run", kind="forward", start=("run", 0.1), travel=3.05,
         push=[(0.0, "chest", (0.4, 0.0, 0.0))],
         tone={"legs": (0.0, 0.08), "trunk": (0.0, 0.12), "neck": (0.0, 0.1), "arms": (0.0, 0.08)},
         release=0.05, throw=(1.0, 0.2, 0.6), mu=0.55),
    dict(name="die_run_b", stance="run", kind="forward", start=("advance", 0.5), travel=3.05,
         push=[(0.0, "chest", (-0.6, 0.6, 0.2))],
         tone={"legs": (0.03, 0.1), "trunk": (0.0, 0.15), "neck": (0.0, 0.1), "arms": (0.0, 0.1)},
         release=0.08, throw=(0.6, 0.9, 0.8), mu=0.55),
    dict(name="die_run_c", stance="run", kind="back", start=("run", 0.45), travel=3.05,
         push=[(0.0, "chest", (-2.6, -0.2, 0.4)), (0.0, "head", (-1.2, 0.0, 0.0))],
         tone=LIMP, release=0.03, throw=(1.5, -0.5, 1.2), mu=0.6),
    # ---- kneeling (firing from a knee)
    dict(name="die_kneeling_a", stance="kneel", kind="forward", start=("fire_kneel", 0.3),
         push=[(0.0, "chest", (0.5, 0.1, 0.0)), (0.8, "chest", (0.2, 0.8, 0.0))],
         tone={"legs": (0.0, 0.1), "trunk": (0.15, 0.3), "neck": (0.0, 0.15), "arms": (0.0, 0.12)},
         release=0.1, throw=(0.5, 0.3, 0.2)),
    dict(name="die_kneeling_b", stance="kneel", kind="back", start=("fire_kneel", 0.8),
         push=[(0.0, "chest", (-1.4, 0.1, 0.2)), (0.0, "head", (-0.6, 0.0, 0.0))],
         tone={"legs": (0.0, 0.1), "trunk": (0.0, 0.15), "neck": (0.0, 0.1), "arms": (0.0, 0.08)},
         release=0.06, throw=(0.6, 0.5, 0.6)),
    dict(name="die_kneeling_c", stance="kneel", kind="side", start=("fire_kneel", 1.1), mirror="die_kneeling_d",
         push=[(0.0, "chest", (0.0, 1.2, 0.1))],
         tone={"legs": (0.0, 0.1), "trunk": (0.05, 0.2), "neck": (0.0, 0.12), "arms": (0.0, 0.1)},
         release=0.08, throw=(0.3, 0.6, 0.3)),
    # ---- lying (firing prone): the head drops, the arms go slack, he may roll a little
    dict(name="die_prone_a", stance="prone", kind="forward", start=("fire_prone", 0.3),
         push=[(0.0, "head", (-0.3, 0.0, 0.6)), (0.0, "chest", (-0.2, 0.0, 0.3))],
         tone={"legs": (0.0, 0.2), "trunk": (0.0, 0.2), "neck": (0.0, 0.15), "arms": (0.0, 0.15)},
         release=0.15, throw=(0.2, 0.2, 0.2)),
    dict(name="die_prone_b", stance="prone", kind="side", start=("fire_prone", 0.9), mirror="die_prone_c",
         push=[(0.0, "chest", (0.0, 1.0, 0.9)), (0.0, "head", (0.0, 0.4, 0.5))],
         tone={"legs": (0.0, 0.2), "trunk": (0.0, 0.2), "neck": (0.0, 0.15), "arms": (0.0, 0.15)},
         release=0.1, throw=(0.2, 0.5, 0.3)),
    # ---- in cover (kneeling behind it): he sinks down slowly and slumps over where he was
    dict(name="die_cover_a", stance="cover", kind="knees", start=("fire_kneel", 0.5), mirror="die_cover_b",
         push=[(0.5, "chest", (0.1, 0.35, 0.0))],
         tone={"legs": (0.1, 0.5), "trunk": (0.3, 0.7), "neck": (0.15, 0.5), "arms": (0.05, 0.4)},
         release=0.25, throw=(0.2, 0.2, 0.0)),
    dict(name="die_cover_c", stance="cover", kind="forward", start=("cower", 1.0),
         push=[(0.4, "chest", (0.3, 0.0, 0.0)), (1.1, "chest", (0.0, -0.8, 0.1))],
         tone={"legs": (0.15, 0.5), "trunk": (0.25, 0.6), "neck": (0.1, 0.4), "arms": (0.1, 0.4)},
         release=0.3, throw=(0.2, -0.2, 0.0)),
]


def tone_fn(hold, fade):
    def f(t):
        if t <= hold:
            return 1.0
        u = (t - hold) / max(fade, 1e-3)
        if u >= 1.0:
            return 0.0
        return 1.0 - u * u * (3.0 - 2.0 * u)
    return f


def mirrored(rc):
    """The same death with every blow (and the rifle's throw, and the blast's spin) to the other side."""
    r = dict(rc)
    r["name"] = rc["mirror"]
    r.pop("mirror")
    fy = lambda v: (v[0], -v[1], v[2])
    sw = lambda p: p.replace("_l", "_R").replace("_r", "_l").replace("_R", "_r")
    r["push"] = [(t, sw(p), fy(dv)) for (t, p, dv) in rc.get("push", [])]
    for k in ("throw", "blast"):
        if k in rc:
            r[k] = fy(rc[k])
    if "spin" in rc:
        s = rc["spin"]
        r["spin"] = (-s[0], s[1], -s[2])
    return r


def all_recipes(only=None):
    out = []
    for rc in RECIPES:
        out.append(rc)
        if rc.get("mirror"):
            out.append(mirrored(rc))
    return [r for r in out if not only or r["name"] in only]


def fixed_turns(man):
    """The riders' turns: the fingers and thumbs as a dead hand holds them (die_back's last frame)."""
    fin = man.local_at("die_back", 1e3)
    rest = (np.array((1.0, 0.0, 0.0, 0.0)), None)              # (a file whose die_back leaves them at rest)
    return {b: fin.get(b, rest)[0] for b in ("fingers_l", "fingers_r", "thumb_l", "thumb_r")}


LIE_MAX = 0.38                      # m: his chest bone's head under this at the end, he lies (sitting up it is ~0.5)
HARDER = (1.0, 1.4, 1.8, 2.3)       # (blows scaled up, in turn, for a body the first blow leaves sitting up)


def harder(rc, k):
    r = dict(rc)
    r["push"] = [(t, p, tuple(v * k for v in dv)) for (t, p, dv) in rc.get("push", [])]
    return r


def simulate_down(man, rd, rc, front=False):
    """simulate(), with harder blows if the first leaves him sitting up (or, for a crawl, not on his front): each
    man's own body falls its own way. Returns (clip, fall, scale)."""
    best = None
    for k in HARDER:
        c, f = simulate(man, rd, harder(rc, k))
        H = f.rec[-1][2]
        z = float(H[rd.pix["chest"]][2])
        fz = float((f.R[rd.pix["chest"]] @ np.array((1.0, 0.0, 0.0)))[2])
        ok = z < LIE_MAX and (not front or fz < -0.35)
        if ok:
            return c, f, k
        score = fz if front else z
        if best is None or score < best[3]:
            best = (c, f, k, score)
    return best[0], best[1], best[2]                 # (a crawler not on his front rolls onto it as he gathers)


def _trim(f, t_min, tol_m=0.004, tol_deg=1.0):
    """Cut the tail where he only lies there (every head within tol_m and every part's turn within tol_deg of the
    last frame); the last frame kept as it came to rest."""
    t1, R1, H1 = f.rec[-1]
    keep = len(f.rec)
    for k in range(len(f.rec) - 1, -1, -1):
        t, R, H = f.rec[k]
        dh = float(np.max(np.linalg.norm(H - H1, axis=1)))
        da = max(float(np.linalg.norm(RG.rotvec(R[i] @ R1[i].T))) for i in range(len(R)))
        if dh > tol_m or math.degrees(da) > tol_deg:
            break
        keep = k + 1
    first = next((k for k, r in enumerate(f.rec) if r[0] >= t_min), len(f.rec) - 1)
    keep = min(len(f.rec), max(keep + 1, first + 1))
    f.rec = f.rec[:keep - 1] + [(f.rec[keep - 1][0], R1, H1)]


def simulate(man, rd, rc, verbose=False):
    clip, t0 = rc["start"]
    dt = 1.0 / 30.0
    travel = np.array((rc.get("travel", 0.0), 0.0, 0.0))
    f = RG.Fall(rd, man.local_at(clip, t0), man.local_at(clip, t0 - dt), dt, travel=travel,
                mu_d=rc.get("mu", 0.7), mu_s=rc.get("mu", 0.7) + 0.2)
    for j in f.joints:
        fam = j["fam"]
        g = next(k for k, v in GROUPS.items() if fam in v)
        h, fd = rc["tone"][g]
        f.tone[j["child"]] = tone_fn(h, fd)
    pushes = []
    for (t, p, dv) in rc.get("push", []):
        pushes.append((t, rd.pix[p], np.array(dv, float)))
    if "blast" in rc:
        # (the whole man takes the blast, the parts nearest its height most; and a turn)
        B = np.array(rc["blast"], float)
        H = f.heads()
        spin = np.array(rc.get("spin", (0.0, 0.0, 0.0)), float)
        c = (rd.mass[:, None] * f.x).sum(0) / rd.mass.sum()
        for k, n in enumerate(rd.parts):
            if n == "weapon":
                continue
            lift = 0.75 + 0.5 * min(1.0, max(0.0, H[k][2] / 1.5))
            pushes.append((0.0, k, B * lift + np.cross(spin, f.x[k] - c) * 0.5))
        for k in range(len(rd.parts)):
            if rd.parts[k] != "weapon":
                f.w[k] += spin * 0.5
    f.push = pushes
    f.release_at = rc.get("release", 0.1)
    f.throw = np.array(rc.get("throw", (0.0, 0.0, 0.0)), float)
    f.throw_w = np.array(rc.get("throw_spin", (0.0, 6.0, 2.0)), float)
    f.run(t_max=rc.get("t_max", 3.0), t_min=rc.get("t_min", 1.0), settle=0.25)
    _trim(f, rc.get("t_min", 1.0))
    start_touch = {k for k, t in f.contact_t.items() if t <= 0.02}
    note = ("ragdoll fall (falls.py): from %s at %.2f s, %s; physics baked, the last frame lies still" %
            (clip, t0, rc["kind"]))
    return clip_of(f, rd, note, start=start_touch), f


# ------------------------------------------------------------------------------------------------ the wounded crawl
# Hit, he goes down on his front; then drags himself on along the ground (the file's belly crawl, slower and slower,
# without his rifle, which lies where it fell), stops, and goes still where he is (a last physics settle). Each:
# the fall (a recipe as above, cut when he is down), how far he gets (crawl loops) and how fast (the crawl's own
# speed times this, at the start and the end).
CRAWLS = [
    dict(name="die_crawl_a", stance="stand", kind="crawl", loops=2.0, speed=(0.8, 0.3),
         fall=dict(name="_fall", kind="forward", start=("advance", 0.45), travel=2.0, t_min=1.4, t_max=1.4,
                   push=[(0.0, "spine_02", (0.5, 0.0, 0.0))],
                   tone={"legs": (0.02, 0.12), "trunk": (0.0, 0.2), "neck": (0.1, 0.3), "arms": (0.1, 0.25)},
                   release=0.1, throw=(1.0, 0.6, 0.6))),
    dict(name="die_crawl_b", stance="kneel", kind="crawl", loops=1.6, speed=(0.7, 0.25),
         fall=dict(name="_fall", kind="forward", start=("fire_kneel", 0.3), t_min=1.3, t_max=1.3,
                   push=[(0.0, "chest", (0.7, -0.1, 0.0))],
                   tone={"legs": (0.0, 0.1), "trunk": (0.1, 0.25), "neck": (0.1, 0.3), "arms": (0.1, 0.25)},
                   release=0.1, throw=(0.4, -0.5, 0.3))),
    dict(name="die_crawl_c", stance="prone", kind="crawl", loops=1.4, speed=(0.6, 0.2),
         fall=dict(name="_fall", kind="forward", start=("fire_prone", 0.3), t_min=0.8, t_max=0.8,
                   push=[(0.0, "head", (0.0, 0.0, 0.3))],
                   tone={"legs": (0.0, 0.3), "trunk": (0.0, 0.3), "neck": (0.0, 0.25), "arms": (0.0, 0.2)},
                   release=0.05, throw=(-0.4, 1.6, 0.6))),
]
CRAWL_GATHER = 1.0                  # s: down, he draws his arms and legs into the crawl (his own muscles, physics)
GATHER_TONE = {"legs": 0.35, "trunk": 0.3, "neck": 0.3, "arms": 1.0}   # (a wounded man's strength: no spring)
GATHER_ROLL = 0.012                 # (of the pelvis's way to the crawl's, a step)
CRAWL_BLEND = 0.3                   # s: then into the crawl itself
CRAWL_EASE = 0.6                    # s: at the end, into the pose his joints can hold limp
CRAWL_SETTLE = {"legs": (0.0, 0.35), "trunk": (0.0, 0.3), "neck": (0.2, 0.5), "arms": (0.0, 0.3)}


def _rz(a):
    c, s = math.cos(a), math.sin(a)
    return np.array(((c, -s, 0.0), (s, c, 0.0), (0.0, 0.0, 1.0)))


def _crawl_lift(man):
    """How far the file's crawl must be raised so that nothing of him but the rifle is under the ground."""
    c = man.clips["crawl"]
    wb = {man.i(n) for n in ("weapon", "bolt", "bipod") if n in man.ix}
    keep = ~np.isin(man.part, list(wb))
    low = 0.0
    for k in range(37):
        Gw, X = man.fk(man.local_at("crawl", c["long"] * k / 36))
        low = min(low, float(man.skin(Gw, X)[keep, 2].min()))
    return -low + 0.004


def crawl_death(man, rd, rc):
    """The wounded crawl: fall (physics), crawl (the file's crawl, turned his way), still (physics). Returns a clip."""
    dt = 1.0 / 30.0
    # 1. the fall, cut once he is down on his front
    _, fa, _k = simulate_down(man, rd, dict(rc["fall"], name=rc["name"]), front=True)
    # (then he gathers himself: his joints driven toward the crawl's first pose, the ground under him)
    Rc, _ = fa._pose(man.local_at("crawl", 0.0))
    t1 = fa.t
    for j in fa.joints:
        fa.target[j["child"]] = Rc[j["parent"]].T @ Rc[j["child"]]
        fam = j["fam"]
        up = GATHER_TONE[next(k for k, vv in GROUPS.items() if fam in vv)]
        fa.tone[j["child"]] = (lambda t, t1=t1, up=up: up * min(1.0, (t - t1) / 0.6))
    # (and rolls himself square onto his belly, facing the way his head lies: the pelvis turned toward the crawl's,
    # a little each step; the joints and the ground bring the rest of him)
    H0 = fa.heads()
    v = H0[rd.pix["head"]] - H0[rd.pix["pelvis"]]
    Z = _rz(math.atan2(v[1], v[0]))
    kp = rd.pix["pelvis"]
    Tw = Z @ Rc[kp]

    def right(f):
        u = min(1.0, (f.t - t1) / 0.3)
        f.R[kp] = RG.from_rotvec(RG.rotvec(Tw @ f.R[kp].T) * GATHER_ROLL * u) @ f.R[kp]
    fz = float((fa.R[rd.pix["chest"]] @ np.array((1.0, 0.0, 0.0)))[2])
    fa.run(t_max=t1 + CRAWL_GATHER + (0.7 if fz > -0.35 else 0.0), t_min=0.0, settle=1e9, on_step=right)
    A = [(t, frame_local(rd, R, H)) for (t, R, H) in fa.rec]
    tA, RA, HA = fa.rec[-1]
    c = man.clips["crawl"]
    L, stride = c["long"], c["stride"]
    hp = man.head[man.i("pelvis")]
    lift = _crawl_lift(man)
    c0 = man.local_at("crawl", 0.0)
    base = HA[rd.pix["pelvis"]] - Z @ (hp + c0["pelvis"][1])
    base[2] = lift
    endA = A[-1][1]
    # 2. the crawl: its clock runs at speed v0 falling to v1 (of its own), over `loops` loops
    v0, v1 = rc["speed"]
    total = rc["loops"] * L
    TB = 2.0 * total / (v0 + v1)
    B = []
    ev = []
    n = int(math.ceil(TB / dt))
    s_prev = 0.0
    for k in range(1, n + 1):
        tau = min(k * dt, TB)
        u = tau / TB
        s = TB * (v0 * u + 0.5 * (v1 - v0) * u * u)      # (the crawl's own clock: speed falling v0 -> v1)
        cl = man.local_at("crawl", s % L)
        w = min(1.0, tau / CRAWL_BLEND)
        w = w * w * (3.0 - 2.0 * w)
        loc = {}
        for nme, (qa, mva) in endA.items():
            if nme == "weapon":
                loc[nme] = (qa, mva)                     # (the rifle lies where it fell)
                continue
            qc = cl.get(nme, (np.array((1.0, 0.0, 0.0, 0.0)), None))[0]
            if nme == "pelvis":
                qc = RG.matq(Z @ RG.qmat(qc))
                pc = base + Z @ (hp + cl["pelvis"][1] + np.array((stride * s / L, 0.0, 0.0)))
                pa = hp + mva
                loc[nme] = (RG.slerp(qa, qc, w), pa + (pc - pa) * w - hp)
            else:
                loc[nme] = (RG.slerp(qa, qc, w), None)
        B.append((tA + tau, loc))
        for e in c["events"]:                             # (his elbows on the ground, as the crawl's own)
            for lp in range(int(s_prev // L), int(s // L) + 1):
                te = lp * L + e["t"]
                if s_prev < te <= s and w >= 1.0:
                    ev.append({"t": round(tA + tau, 3), "fx": e["fx"], "socket": None, "cue": e["cue"],
                               "bone": e.get("bone")})
        s_prev = s
    # (where the blend into the crawl would take a hand or foot into the ground, he is lifted that much, eased)
    wb = [man.i(n) for n in ("weapon", "bolt", "bipod") if n in man.ix]
    keep = ~np.isin(man.part, wb)
    nb = int(math.ceil((CRAWL_BLEND + 0.2) / dt))
    need = np.zeros(len(B))
    for k in range(min(nb, len(B))):
        P = man.skin(*man.fk(B[k][1]))
        need[k] = max(0.0, 0.004 - float(P[keep, 2].min()))
    if need.max() > 0.0:
        wide = np.array([need[max(0, k - 4):k + 5].max() for k in range(len(B))])
        ker = np.hanning(9)[1:-1]
        lift_k = np.convolve(wide, ker / ker.sum(), mode="same")
        lift_k = np.maximum(lift_k, need)
        for k in np.nonzero(lift_k > 0)[0]:
            q, mv = B[k][1]["pelvis"]
            B[k][1]["pelvis"] = (q, mv + np.array((0.0, 0.0, lift_k[k])))
    # 3. still: the crawl eased into the pose his limp joints can hold (no snap as the ragdoll takes him), then a
    # last settle from where he stopped
    f = RG.Fall(rd, B[-1][1], B[-2][1], dt)
    held = frame_local(rd, f.R, f.heads())
    tB = B[-1][0]
    for k in range(len(B)):
        u = (B[k][0] - (tB - CRAWL_EASE)) / CRAWL_EASE
        if u <= 0.0:
            continue
        u = min(1.0, u)
        u = u * u * (3.0 - 2.0 * u)
        loc = {}
        for nme, (q, mv) in B[k][1].items():
            qh, mvh = held[nme]
            loc[nme] = (RG.slerp(q, qh, u), None if mv is None else mv + (mvh - mv) * u)
        B[k] = (B[k][0], loc)
    f = RG.Fall(rd, B[-1][1], B[-2][1], dt)
    for j in f.joints:
        g = next(k for k, vv in GROUPS.items() if j["fam"] in vv)
        f.tone[j["child"]] = tone_fn(*CRAWL_SETTLE[g])
    f.release_at = 0.0
    f.throw = np.zeros(3)
    f.throw_w = np.zeros(3)
    f.run(t_max=2.0, t_min=1.0, settle=0.25)
    _trim(f, 1.0)
    C = [(tB + t, frame_local(rd, R, H)) for (t, R, H) in f.rec[1:]]
    note = ("wounded crawl (falls.py): from %s at %.2f s, down on his front (physics), %.1f loops of the crawl slowing "
            "from %.0f%% to %.0f%%, then still (physics)" % (rc["fall"]["start"][0], rc["fall"]["start"][1],
                                                          rc["loops"], v0 * 100, v1 * 100))
    events = fall_events(fa, rd, {k for k, t in fa.contact_t.items() if t <= 0.02}) + ev
    return clip_from(man, A + B + C, note, events)


# ------------------------------------------------------------------------------------------------ the table
# deaths[stance][kind]: the clips that fit a man hit in that stance, by the Fates picker's kinds (forward, back, side,
# knees, spin, thrown) and the wounded crawl. Every cell has at least one clip; a cell without a fall of its own
# borrows the nearest (a running man spun round, a kneeling man thrown back hard).
TABLE = {
    "stand": {"back": ["die_back_a", "die_back_b", "die_back_c"],
              "forward": ["die_forward_a", "die_forward_b", "die_forward_c"],
              "side": ["die_side_a", "die_side_b", "die_side_c", "die_side_d"],
              "knees": ["die_knees_a", "die_knees_b"], "spin": ["die_spin_a", "die_spin_b"],
              "thrown": ["die_thrown_a", "die_thrown_b", "die_thrown_c"], "crawl": ["die_crawl_a"]},
    "run": {"back": ["die_run_c", "die_back_c"], "forward": ["die_run_a", "die_run_b", "die_forward_a"],
            "side": ["die_spin_a", "die_spin_b", "die_run_b"], "knees": ["die_forward_c", "die_run_a"],
            "spin": ["die_spin_a", "die_spin_b"], "thrown": ["die_thrown_a", "die_thrown_b", "die_thrown_c"],
            "crawl": ["die_crawl_a"]},
    "kneel": {"back": ["die_kneeling_b"], "forward": ["die_kneeling_a"], "side": ["die_kneeling_c", "die_kneeling_d"],
              "knees": ["die_kneeling_a", "die_kneeling_c", "die_kneeling_d"],
              "spin": ["die_kneeling_c", "die_kneeling_d"], "thrown": ["die_kneeling_b", "die_kneeling_c", "die_kneeling_d"],
              "crawl": ["die_crawl_b"]},
    "prone": {"back": ["die_prone_b", "die_prone_c"], "forward": ["die_prone_a"], "side": ["die_prone_b", "die_prone_c"],
              "knees": ["die_prone_a"], "spin": ["die_prone_b", "die_prone_c"], "thrown": ["die_prone_b", "die_prone_c"],
              "crawl": ["die_crawl_c"]},
    "cover": {"back": ["die_cover_a", "die_cover_b"], "forward": ["die_cover_c"], "side": ["die_cover_a", "die_cover_b"],
              "knees": ["die_cover_a", "die_cover_b", "die_cover_c"], "spin": ["die_cover_a", "die_cover_b"],
              "thrown": ["die_kneeling_b", "die_cover_a", "die_cover_b"], "crawl": ["die_crawl_b"]},
}
ABOUT_DEATHS = (
    "Deaths (Tools/battle_units/falls.py on ragdoll.py): every die_* clip named in `deaths` is a physics fall baked "
    "from this man's own body (his parts' masses, his joints' human ranges, the muscle tone going out of him, the "
    "ground under him; the rifle leaves his hand and lands where it lands), so no two kinds fall alike and none sinks "
    "into the ground. Pick: deaths[stance][kind], stance stand, run, kneel, prone or cover (what he was doing when "
    "hit), kind as the Fates picker's man_fall kinds (forward, back, side, knees, spin, thrown) or crawl (wounded: he "
    "falls, drags himself on a little, then goes still); one of the list by a hash of (block id, man). Play it once "
    "(loop false) and hold its last frame for the rest of the battle: the body stays where it fell. Each clip starts "
    "in the pose of `start` [clip, time] (blend into it over ~0.15 s from whatever he was doing); `end_offset` is "
    "where his pelvis ends (metres, his own axes; the clip is not in place: the pelvis and the rifle move); the "
    "weapon bone's last key is the rifle lying on the ground. Events: knee_down, body_fall, rifle_drop "
    "(and elbow_down in a crawl), at the time they touch. The older die_back, die_forward and die_kneel stay in the "
    "file as they were but are not in the table (they sink into the ground).")


def _job(rc):
    m, rd = _W["m"], _W["rd"]
    st = rc["fall"]["start"] if rc["kind"] == "crawl" else rc["start"]
    m.bag_rel = bag_rel(m, st[0], st[1], m.bag) if m.bag else None
    if rc["kind"] == "crawl":
        c = crawl_death(m, rd, rc)
        start = list(rc["fall"]["start"])
    else:
        c, _, k = simulate_down(m, rd, rc)
        start = list(rc["start"])
        if k != 1.0:
            c["from"] += " (blows x%.1f for this man: the first left him sitting up)" % k
    c["start"] = start
    c["weapon"] = m.clips[start[0]].get("weapon", "rifle")
    m.clips["_check"] = c
    g = ground_check(m, "_check")
    rep = rom_check(m, "_check")
    faults = [(r["key"] + r["side"], round(float(r["value"]), 1)) for r in rep["faults"]]
    Gw, X = m.fk(m.local_at(c, c["long"] + 1.0))
    g["chest_z"] = round(float(X[m.i("chest")][2]), 3)
    # (where the weapon ends: on the ground, near him; an unarmed man's weapon bone is in his hand, unseen)
    g["weapon_far"] = 0.0 if rd.unarmed else round(float(np.linalg.norm((X[m.i("weapon")] - X[m.i("pelvis")])[:2])), 2)
    g["weapon_z"] = 0.0 if rd.unarmed else round(float(X[m.i("weapon")][2]), 3)
    return rc["name"], c, g, faults


_W = {}


# What each man carries, where it is not the rifle (4.2 kg): his weapon's weight; `bag`, bones that ride the weapon
# bone in the file but are packed on him (bone: the part they ride); `bag_shape`, a clip where they sit as when he is
# hit, to make them part of his shape; `gear_kg`, weight packed on a part; `loose`, points kept out of his shape (see
# LOOSE). A file with nothing on the weapon bone is unarmed.
KIT = {
    "soldier_USA.json": dict(weapon_kg=4.3),                 # (M1 rifle, loaded)
    "soldier_USA_bar.json": dict(weapon_kg=8.8),             # (the BAR, M1918A2, with its bipod and a magazine)
    "soldier_USA_nco.json": dict(weapon_kg=5.0),             # (Thompson M1A1 with a 30-round magazine)
    "soldier_USA_bazooka.json": dict(weapon_kg=5.9, note=" What he drops is the launcher (the weapon bone, 1.37 m): "
                                     "it falls and lies where it lands as the rifle does."),
    "soldier_USA_loader.json": dict(weapon_kg=2.5, bag={"bolt": "chest"},
                                    note=" What he drops is his carbine; the rocket on the bolt bone stays in the bag "
                                         "on his back through every death (keyed, as in his other clips)."),
    "soldier_USA_radio.json": dict(weapon_kg=2.5, bag={"bolt": "chest"}, bag_shape="idle", gear_kg={"chest": 15.0},
                                   loose="whip", chain=("whip_1", "whip_2", "whip_3", "whip_4"),
                                   note=" What he drops is his carbine. The handset on the bolt bone stays clipped on "
                                        "his left strap through every death. The SCR-300 set on his back (with its "
                                        "battery, about 15 kg) is part of his body: he lands on it and it weighs "
                                        "his chest. Its whip antenna is kept out of his shape, so it does not prop "
                                        "him up.",
                                   note_chain=" The whip's own bones (whip_1 to whip_4) are keyed in each death: it "
                                              "sags a little where it lies level and bends over to lie along the "
                                              "ground where it would go under.",
                                   note_rigid=" The whip is rigid on the chest bone here and can go into the ground "
                                              "in a fall on his back or side."),
    "soldier_USA_medic.json": dict(note=" He carries no weapon: nothing leaves his hand and there is no rifle_drop; "
                                        "the weapon bone's keys carry nothing seen."),
}


def _whip(man):
    """The radio man's whip antenna: the points on its own bones (whip_1..), or before it had them, the steel
    (paint 9) on the chest bone above the set's top, in the rest pose."""
    pa = np.array(man.data["near"]["paints"])
    ch = [man.i(b) for b in KIT["soldier_USA_radio.json"]["chain"] if b in man.ix]
    return np.isin(man.part, ch) | ((man.part == man.i("chest")) & (pa == 9) & (man.P[:, 2] > 1.6))


LOOSE = {"whip": _whip}


def _init(path):
    data = json.load(open(path))
    m = RG.Man(data)
    kit = KIT.get(os.path.basename(path), {})
    m.bag = kit.get("bag", {})
    m.loose = LOOSE[kit["loose"]](m) if kit.get("loose") else None
    m.chain = tuple(kit.get("chain", ()))
    # (`bag_shape`: a clip where the bag bones sit as in every pose he is hit in; their points join his shape there)
    at = bag_rel(m, kit["bag_shape"], 0.0, m.bag) if kit.get("bag_shape") else None
    _W["m"], _W["rd"] = m, RG.Ragdoll(m, fixed_turns(m), weapon_kg=kit.get("weapon_kg"), bag=tuple(m.bag),
                                      loose=m.loose, gear_kg=kit.get("gear_kg"), bag_at=at)


def names_all():
    return [r["name"] for r in all_recipes()] + [r["name"] for r in CRAWLS]


def bake(path, workers=4, quick=None):
    """Every death of one soldier file, into it (the ones added before taken out first). Returns a report."""
    data = json.load(open(path))
    jobs = [r for r in all_recipes() + CRAWLS if not quick or r["name"] in quick]
    from multiprocessing import Pool
    with Pool(workers, initializer=_init, initargs=(path,)) as p:
        out = p.map(_job, jobs, chunksize=1)
    for n in names_all():
        data["clips"].pop(n, None)
    rep = {}
    for name, c, g, faults in out:
        data["clips"][name] = c
        rep[name] = {"long": c["long"], "ground": g, "faults": faults}
    have = set(data["clips"])
    data["deaths"] = {s: {k: [n for n in v if n in have] for k, v in kinds.items()} for s, kinds in TABLE.items()}
    data["about_deaths"] = about_kit(path, data)
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    json.load(open(tmp))                        # (it reads back)
    os.replace(tmp, path)
    return data, rep


def rebag(path):
    """Re-key the bag bones and the whip chain of the deaths already in a file, without new falls (seconds, not
    minutes): for a file baked before bag_dense or chain_drape, or one whose deaths were carried over onto a
    re-export (bones added at the end). Only those bones' tracks change. Returns how many deaths it re-keyed."""
    kit = KIT.get(os.path.basename(path), {})
    bag, chain = kit.get("bag", {}), kit.get("chain", ())
    data = json.load(open(path))
    man = RG.Man(data)
    chain = chain if chain and all(b in man.ix for b in chain) else ()
    if not (bag or chain) or "deaths" not in data:
        return 0
    done = 0
    for n in names_all():
        c = data["clips"].get(n)
        if not c or "start" not in c:
            continue
        s = c["start"]
        if bag:
            bag_dense(man, c, bag_rel(man, man.clips[s[0]], s[1], bag))
        if chain:
            chain_drape(man, c, chain)
        done += 1
    data["about_deaths"] = about_kit(path, data)
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    json.load(open(tmp))
    os.replace(tmp, path)
    return done


def about_kit(path, data):
    """The deaths' note for this file: the common one and what its man carries."""
    kit = KIT.get(os.path.basename(path), {})
    names = {b["name"] for b in data["bones"]}
    ch = kit.get("chain", ())
    extra = (kit.get("note_chain", "") if all(b in names for b in ch) else kit.get("note_rigid", "")) if ch else ""
    return ABOUT_DEATHS + kit.get("note", "") + extra


def picture_sheet(data, out, names=None, cols=6, size=(360, 250)):
    """The end of every death (how each man lies), for a look: one tile each, in the file's own colours."""
    from PIL import Image, ImageDraw
    m = RG.Man(data)
    look = dict(LOOK)
    look.update(data.get("colours", {}))
    names = names or [n for n in names_all() if n in data["clips"]]
    rows = (len(names) + cols - 1) // cols
    W, H = size
    sheet = Image.new("RGB", (W * cols, H * rows), (255, 255, 255))
    for i, n in enumerate(names):
        c = data["clips"][n]
        P = frames_of(m, c, [c["long"] + 1.0])[0]
        tgt = P.mean(0)
        tgt[2] = 0.15
        im = render(m, P, size, cam={"target": tuple(tgt), "yaw": -50.0, "pitch": 38.0, "dist": 2.8}, look=look)
        ImageDraw.Draw(im).text((6, 4), n, fill=(0, 0, 0))
        sheet.paste(im, ((i % cols) * W, (i // cols) * H))
    sheet.save(out, quality=88)


SOLDIERS = "/mnt/project-files/battle-models/land/soldier/"
FILES = {"GER": "soldier_GER.json", "POL": "soldier_POL.json", "SOV": "soldier_SOV.json", "SOVW": "soldier_SOV_winter.json",
         "SOVS": "soldier_SOV_snow.json", "ITA": "soldier_ITA.json", "ITAD": "soldier_ITA_desert.json",
         "JAP": "soldier_JAP.json", "CHN": "soldier_CHN.json", "USA": "soldier_USA.json", "USAB": "soldier_USA_bar.json",
         "USAZ": "soldier_USA_bazooka.json", "USAL": "soldier_USA_loader.json", "USAN": "soldier_USA_nco.json",
         "USAM": "soldier_USA_medic.json", "USAR": "soldier_USA_radio.json"}


def main(argv):
    ap = argparse.ArgumentParser(description="Physics deaths for the battle soldiers' files.")
    ap.add_argument("--armies", default=",".join(FILES), help="army codes, e.g. GER,SOV (default: every file there is)")
    ap.add_argument("--dir", default=SOLDIERS, help="the folder holding the soldier_*.json files")
    ap.add_argument("--only", default="clips,pictures",
                    help="any of clips, pictures; or rebag alone (re-key bag bones and the whip in the deaths there, no new falls)")
    ap.add_argument("--quick", default="", help="only these deaths (names, comma-separated), for a try")
    ap.add_argument("--workers", type=int, default=4)
    a = ap.parse_args(argv)
    only = set(a.only.split(","))
    for army in a.armies.split(","):
        t = time.time()
        path = os.path.join(a.dir, FILES.get(army, "soldier_%s.json" % army))
        if not os.path.exists(path):
            print(army, "no file", path, "(skipped)", flush=True)
            continue
        if only == {"rebag"}:
            print(army, "re-keyed the bag and whip in %d deaths" % rebag(path), flush=True)
            continue
        if "clips" in only:
            data, rep = bake(path, a.workers, set(a.quick.split(",")) - {""})
            under = {n: r["ground"]["under_1cm"] for n, r in rep.items() if r["ground"]["under_1cm"]}
            up = {n: r["ground"]["chest_z"] for n, r in rep.items() if r["ground"]["chest_z"] >= LIE_MAX}
            astray = {n: (r["ground"]["weapon_far"], r["ground"]["weapon_z"]) for n, r in rep.items()
                      if r["ground"]["weapon_far"] > 5.0 or r["ground"]["weapon_z"] > 0.3}   # (a blast throws it 4 m)
            faults = {n: r["faults"] for n, r in rep.items() if r["faults"]}
            loose = {n: (r["ground"]["loose_min_z"], r["ground"]["loose_under_1cm"]) for n, r in rep.items()
                     if r["ground"].get("loose_under_1cm")}
            print(army, "%d deaths in %.0f s" % (len(rep), time.time() - t), "bytes", os.path.getsize(path),
                  "NOT LYING DOWN:" if up else "all lie down", up or "",
                  "WEAPON ASTRAY:" if astray else "weapons on the ground near him", astray or "",
                  "points under 1 cm:", under, "rom faults:", faults,
                  *(("loose points (lowest z, how many under 1 cm):", loose) if loose else ()), flush=True)
        else:
            data = json.load(open(path))
        if "pictures" in only and "deaths" in data:
            out = os.path.join(a.dir, "pictures", "deaths", "ends_%s.jpg" % army)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            picture_sheet(data, out)
            print(army, "sheet", out, flush=True)


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:])
