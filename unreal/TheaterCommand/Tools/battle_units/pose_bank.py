"""The pose bank for far soldiers (the true-scale plan's SO1): about 70 poses an army (112 with the gaits' in-betweens), each a clip at a point of its
cycle, baked once when a battle loads; a far man then shows the bank's pose nearest to where his clip has got to,
instead of being skinned every frame. Falls are not in the bank: a far man's fall plays as today, and he is frozen in
its last pose.

    python pose_bank.py --write OUT.json                      the bank (one file for every army: clips by name, phases)
    python pose_bank.py --bake soldier_USA.json [--lod far]    bake it on one soldier file, timed; --sheet PNG draws it

The bank names poses by clip and phase (0 to 1 of the clip's length), not by seconds, so one bank serves every soldier
file: each army's clips have the same names, and a file without one of them (a rifleman has no `lmg_burst`) leaves its
poses out and its cycle falls back to `instead`. The bake here is the reference for the game's own (LandMen, C++): the
file's own skinning (its `about`), keys blended linearly between the 15 a second.
"""
import json
import os
import sys
import time

import numpy as np

# clip, the phases kept (0 to 1 of its length), and what a file without it shows instead
BANK = [
    ("idle", (0.0, 0.25, 0.5, 0.75), None),
    ("walk", tuple(k / 8 for k in range(8)), None),
    ("run", tuple(k / 8 for k in range(8)), None),
    ("advance", tuple(k / 6 for k in range(6)), "run"),
    ("creep", tuple(k / 6 for k in range(6)), "walk"),
    ("flee", tuple(k / 6 for k in range(6)), "run"),
    ("crawl", tuple(k / 8 for k in range(8)), None),
    ("fire_stand", (0.0, 0.08, 0.3, 0.6), "idle"),
    ("fire_kneel", (0.0, 0.08, 0.3, 0.6), None),
    ("fire_prone", (0.0, 0.08, 0.3, 0.6), None),
    ("cower", (0.2, 0.6), "fire_kneel"),
    ("reload_stand", (0.3, 0.65), "fire_stand"),
    ("reload_kneel", (0.3, 0.65), "fire_kneel"),
    ("reload_prone", (0.3, 0.65), "fire_prone"),
    ("kneel_down", (0.5,), "fire_kneel"),
    ("stand_up", (0.5,), "fire_kneel"),
    ("go_prone", (0.35, 0.7), "fire_prone"),
]
# The gaits' in-between poses (11-22, for mid men drawn as instances: the material goes between the two poses either
# side, and a half step between keeps a man stepping out past 25 m within a pixel of his live self): put after the
# first 70, so those keep their numbers; each cycle is listed in phase order.
DENSE = [
    ("walk", tuple((k + 0.5) / 8 for k in range(8))),
    ("run", tuple((k + 0.5) / 8 for k in range(8))),
    ("advance", tuple((k + 0.5) / 6 for k in range(6))),
    ("creep", tuple((k + 0.5) / 6 for k in range(6))),
    ("flee", tuple((k + 0.5) / 6 for k in range(6))),
    ("crawl", tuple((k + 0.5) / 8 for k in range(8))),
]
# Release 2's field clips (field_clips.py, 11-22): the stretcher pair, the man they carry, and the lineman at his
# splice; put after the gaits' in-betweens, so poses 0 to 111 keep their numbers. (A slow gait: 12 a cycle.)
FIELD = [
    ("stretcher_front", tuple(k / 12 for k in range(12)), "walk"),
    ("stretcher_rear", tuple(k / 12 for k in range(12)), "walk"),
    ("carried_wounded", (0.0, 0.25, 0.5, 0.75), None),
    ("splice_kneel", (0.0, 0.2, 0.45, 0.8), "fire_kneel"),
]
ABOUT = (
    "The pose bank for far soldiers (the true-scale plan, SO1). Each pose is a clip at a phase (0 to 1 of that clip's "
    "`long`), baked once per soldier file when a battle loads: the file's own skinning (its `about`), keys blended "
    "linearly. Each cycle lists its poses in phase order. A far man playing clip C at time t shows pose "
    "cycles[C][round(n * (t mod long) / long) mod n] for a loop (n = len(cycles[C])), and for a one-shot clip the nearest "
    "phase, the last one once he is past it; a man drawn as an instance (the vertex-animation form, mid and far) goes "
    "between the two poses either side of t by its share. Poses 70 on are the gaits' in-between poses (walk, run, "
    "crawl 16 a cycle, advance, creep, flee 12), added 11-22 so a man stepping out past 25 m stays within a pixel of "
    "his live self; the first 70 keep their numbers. Poses 112 on are release 2's field clips (field_clips.py): "
    "stretcher_front and stretcher_rear (12 a cycle), carried_wounded and splice_kneel (4); a file without them (the "
    "sailors) goes by `instead`. A clip not in "
    "`cycles` shows the cycle of `instead[C]`, and a clip the file lacks is left out of its bake and also goes by "
    "`instead`. Falls are not in the bank: a far man's fall plays as today (skinned), then he is frozen in its last "
    "pose. Pose-bank men fire no clip flashes (SO3): their clips' `muzzle_flash` events are skipped; their rounds, if "
    "the recording has them, flash as the battle's distance cap allows. Bake cost measured by pose_bank.py --bake "
    "(numpy, one core): see `bake_ms`."
)


def bank():
    poses, cycles, instead = [], {}, {}
    for clip, phases, alt in BANK:
        cycles[clip] = []
        for ph in phases:
            cycles[clip].append(len(poses))
            poses.append({"clip": clip, "phase": round(float(ph), 4)})
        if alt:
            instead[clip] = alt
    for clip, phases in DENSE:
        for ph in phases:
            cycles[clip].append(len(poses))
            poses.append({"clip": clip, "phase": round(float(ph), 4)})
    for clip, phases, alt in FIELD:
        cycles[clip] = []
        for ph in phases:
            cycles[clip].append(len(poses))
            poses.append({"clip": clip, "phase": round(float(ph), 4)})
        if alt:
            instead[clip] = alt
    for clip in cycles:
        cycles[clip].sort(key=lambda k: poses[k]["phase"])
    # clips not in the bank that a far man may be given, and the cycle he shows for them
    instead.update({"lmg_burst": "fire_prone", "throw_grenade": "fire_stand"})
    return {"about": ABOUT, "poses": poses, "cycles": cycles, "instead": instead,
            "loops": [c for c, _, _ in BANK if c not in ("kneel_down", "stand_up", "go_prone", "reload_stand",
                                                         "reload_kneel", "reload_prone")] + [c for c, _, _ in FIELD]}


# ---------------------------------------------------------------- the file's own skinning (its `about`)
def qmul(a, b):
    w1, x1, y1, z1 = a
    w2, x2, y2, z2 = b
    return np.array((w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2, w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
                     w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2, w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2))


def qmat(q):
    w, x, y, z = q / np.linalg.norm(q)
    return np.array(((1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)),
                     (2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)),
                     (2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y))))


def key_at(keys, t):
    K = np.asarray(keys, float)
    ts = K[:, 0]
    if t <= ts[0]:
        return K[0, 1:]
    if t >= ts[-1]:
        return K[-1, 1:]
    i = int(np.searchsorted(ts, t)) - 1
    u = (t - ts[i]) / max(ts[i + 1] - ts[i], 1e-9)
    a, b = K[i, 1:], K[i + 1, 1:]
    if len(a) == 4 and float(a @ b) < 0:
        b = -b
    return a + (b - a) * u


def pose_bones(data, clip, t):
    """Each bone's turn G (3x3) and head X for clip at time t."""
    bones = data["bones"]
    tracks = {tr["part"]: tr for tr in data["clips"][clip]["tracks"]}
    G, X = [None] * len(bones), [None] * len(bones)
    for i, b in enumerate(bones):
        h = np.array(b["head"], float)
        tr = tracks.get(i)
        q = key_at(tr["rot"], t) if tr and tr.get("rot") else np.array((1.0, 0.0, 0.0, 0.0))
        mv = key_at(tr["move"], t) if tr and tr.get("move") else np.zeros(3)
        p = b["parent"]
        if p is None:
            G[i] = qmat(q) if tr and tr.get("rot") else np.eye(3)
            X[i] = h + mv
        else:
            hp = np.array(bones[p]["head"], float)
            G[i] = G[p] @ qmat(q) if tr and tr.get("rot") else G[p]
            X[i] = X[p] + G[p] @ (h - hp + mv)
    return np.array(G), np.array(X)


def skin(mesh, data, G, X):
    P = np.asarray(mesh["points"], float).reshape(-1, 3)
    heads = np.array([b["head"] for b in data["bones"]], float)
    a, b = np.asarray(mesh["part"], int), np.asarray(mesh["part2"], int)
    w = np.asarray(mesh["w2"], float)[:, None]
    pa = X[a] + np.einsum("nij,nj->ni", G[a], P - heads[a])
    pb = X[b] + np.einsum("nij,nj->ni", G[b], P - heads[b])
    return pa * (1 - w) + pb * w


def bake(data, bk, lod="far"):
    mesh = data[lod]
    out, skipped = [], []
    t0 = time.perf_counter()
    for k, ps in enumerate(bk["poses"]):
        c = data["clips"].get(ps["clip"])
        if c is None:
            skipped.append(ps["clip"])
            out.append(None)
            continue
        G, X = pose_bones(data, ps["clip"], ps["phase"] * float(c["long"]))
        out.append(skin(mesh, data, G, X))
    return out, (time.perf_counter() - t0) * 1000.0, sorted(set(skipped))


# ---------------------------------------------------------------- the sheet: every pose, flat-shaded, from his right
def sheet(data, lod, posed, bk, path, cols=10, cell=150):
    from PIL import Image, ImageDraw, ImageFont
    mesh = data[lod]
    T = np.asarray(mesh["triangles"], int).reshape(-1, 3)
    paints = np.asarray(mesh["paints"], int)
    shade = np.asarray(mesh.get("shade", [1.0] * len(paints)), float)
    cols_hex = data["colours"]
    pal = [tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)) for h in (cols_hex if isinstance(cols_hex, list) else cols_hex.values())]
    rows = (len(posed) + cols - 1) // cols
    im = Image.new("RGB", (cols * cell, rows * (cell + 16) + 30), (232, 226, 214))
    dr = ImageDraw.Draw(im)
    dr.text((8, 8), f"{data['name']}: the {len(posed)} poses of the far men's bank ({lod}, {len(paints)} points), seen from his right", fill=(40, 34, 28))
    light = np.array((0.45, -0.6, 0.66))
    light /= np.linalg.norm(light)
    scale = cell / 2.1
    for k, V in enumerate(posed):
        cx, cy = (k % cols) * cell, 30 + (k // cols) * (cell + 16)
        if V is not None:
            # (seen from his right: screen x = his forward, screen y = up; nearer to the eye = more -y... his right is +y)
            sx = cx + cell * 0.5 + (V[:, 0] - V[:, 0].mean()) * scale
            sy = cy + cell - 6 - V[:, 2] * scale
            depth = V[T].mean(1)[:, 1]                      # (+y is his right, toward the eye)
            n = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]])
            n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
            lit = 0.55 + 0.45 * np.clip(np.abs(n @ light), 0, 1)
            for ti in np.argsort(depth):
                tri = T[ti]
                p = paints[tri[0]]
                c = pal[p] if p < len(pal) else (128, 128, 128)
                f = lit[ti] * float(shade[tri].mean())
                dr.polygon([(float(sx[j]), float(sy[j])) for j in tri], fill=tuple(int(min(255, v * f)) for v in c))
        ps = bk["poses"][k]
        dr.text((cx + 4, cy + cell + 1), f"{k} {ps['clip']} {ps['phase']:.2f}", fill=(60, 52, 44))
    im.save(path)
    return path


def main():
    a = sys.argv
    if "--write" in a:
        out = a[a.index("--write") + 1]
        bk = bank()
        if "--timing" in a:
            bk["bake_ms"] = json.load(open(a[a.index("--timing") + 1]))
        tmp = out + ".tmp"
        with open(tmp, "w") as f:
            json.dump(bk, f, indent=1)
        os.replace(tmp, out)
        print(f"{out}: {len(bk['poses'])} poses, {len(bk['cycles'])} cycles")
    if "--bake" in a:
        src = a[a.index("--bake") + 1]
        lod = a[a.index("--lod") + 1] if "--lod" in a else "far"
        data = json.load(open(src))
        bk = bank()
        posed, ms, skipped = bake(data, bk, lod)
        print(f"{os.path.basename(src)} {lod}: {sum(p is not None for p in posed)} poses baked in {ms:.1f} ms"
              f"{' (left out: ' + ', '.join(skipped) + ')' if skipped else ''}")
        if "--sheet" in a:
            print(sheet(data, lod, posed, bk, a[a.index("--sheet") + 1]))
        return ms


if __name__ == "__main__":
    main()
