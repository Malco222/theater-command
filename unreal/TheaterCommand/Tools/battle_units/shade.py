"""Shade: the baked light and grime of a delivered soldier file (X.json -> X.json with a `shade` per point).

    /home/user/bpyenv/bin/python shade.py soldier_USA [soldier_GER ...]     stage X.json with shade in /home/user/art/looks/out/
    /home/user/bpyenv/bin/python shade.py --deliver soldier_USA ...        write the shade arrays into the shared X.json
                                                                            (merged into the file as it is then; backed up)
    /home/user/bpyenv/bin/python shade.py --all                             every soldier file in the shared folder
    /home/user/bpyenv/bin/python shade.py --twin soldier_USA                stage his gore twin's (after his own), then
                                                                            --deliver soldier_USA_gore

The battle (LandMen) draws a man's points in their paint's colour times `shade` (near, mid and far each carry their own
array, one number a point; no array means 1 everywhere). This fills it, so the men stop looking like clay:

- SHADOW (ambient occlusion), averaged over a bank of the man's own poses (standing, walking, running, kneeling, firing
  kneeling and lying, crawling, cowering), so a place that is shut in only in the rest pose (the inside of the arm) is not
  painted dark for good: the armpits, between the legs, under the pack, the belt and its pouches, behind the straps, in
  the pockets, under the helmet's brim, the eye sockets and under the chin, the rifle's hand-guard under the hand.
  Rays: 40 a point, cosine-spread over its facing, out to 0.22 m, the near hit counting most.
- CREASES AND EDGES: the hollow of a fold is darkened, a worn edge lifted a little (the mesh's own curvature).
- GRIME (July 1944, after weeks in the field): dust and mud darkening the boots and leggings, up the trousers to the knee
  in splashes; the knees (kneeling) and the elbows and the seat of the trousers worn darker; the cuffs and the hands dirty;
  a blotchy fade over all the cloth so no two hand-breadths of it are quite the same colour.
- mid and far take the near's (nearest points of the same side, facing the same way), so the levels match as they swap.

The arrays keep the file's point order, so faces, helmets, inks and helmet_tris are untouched. A gore twin (X_gore.json)
gets its man's shade for his own points and the nearest for the points it appends (the caps' own paints: 1).
Re-run after any re-export of a soldier file (a re-export writes near/mid/far afresh, without shade).
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
import gore  # noqa: E402  (the json rig: Rig, pose, skin)

SHARED = "/mnt/project-files/battle-models/land/soldier/"
STAGE = "/home/user/art/looks/out/"
BACKUP = "/home/user/art/looks/backup/"

PAINT = {"tunic": 0, "trousers": 1, "boots": 2, "lower": 3, "straps": 4, "helmet": 5, "pack": 6, "skin": 7, "wood": 8,
         "steel": 9, "coat": 10, "coat_dark": 11, "leather": 12, "blanket": 13, "hoof": 14}
CLOTH = {0, 1, 3, 4, 6, 10, 11, 13}          # (woven: fades, takes dust; webbing and leggings too)
LEGS = {1, 2, 3, 11}                         # (trousers, boots, leggings, a long coat's skirt: the mud's)
GORE = {35, 36, 37}

# how much shadow darkens each kind of surface (cloth drinks the light; skin and steel are lit back by themselves)
AO_K = {"cloth": 0.50, "skin": 0.30, "steel": 0.42, "wood": 0.40, "leather": 0.42, "helmet": 0.42}
OCC0, OCC1 = 0.10, 0.85                       # shadow below OCC0 counts nothing, OCC1 and over counts in full
RAYS = 40
REACH = 0.22
POSES = [("idle", 0.3), ("walk", 0.25), ("walk", 0.8), ("run", 0.3), ("advance", 0.4), ("fire_stand", 0.05),
         ("fire_kneel", 0.05), ("fire_prone", 0.05), ("crawl", 0.6), ("cower", 1.0), ("kneel_down", 0.5),
         ("creep", 0.6)]
LOD_K = {"near": 1.0, "mid": 0.9, "far": 0.75}  # (farther off, a little less of it: its points stand for wider patches,
                                               #  part of them hidden, and the eye no longer sees a fold at 100 m)
LO, HI = 0.40, 1.0                            # (the file says 0 to 1)


def kind_of(p):
    if p == 7:
        return "skin"
    if p == 9:
        return "steel"
    if p == 8:
        return "wood"
    if p in (2, 12, 14):
        return "leather"
    if p == 5:
        return "helmet"
    return "cloth"


# ================================================================================================ noise
def _hash3(ix, iy, iz, seed):
    h = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    h = (h ^ (h >> 13)) * 1274126177
    return ((h ^ (h >> 16)) & 0xFFFFFF) / float(0xFFFFFF)


def vnoise(P, scale, seed=0):
    """Smooth value noise in [0, 1] at points P (n,3), cell size `scale` metres."""
    Q = P / scale
    I = np.floor(Q).astype(np.int64)
    F = Q - I
    F = F * F * (3 - 2 * F)
    out = np.zeros(len(P))
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                h = np.array([_hash3(int(a), int(b), int(c), seed) for a, b, c in (I + (dx, dy, dz))])
                w = (F[:, 0] if dx else 1 - F[:, 0]) * (F[:, 1] if dy else 1 - F[:, 1]) * (F[:, 2] if dz else 1 - F[:, 2])
                out += h * w
    return out


def fbm(P, scale, seed=0, oct=3):
    v, a, tot = np.zeros(len(P)), 1.0, 0.0
    for o in range(oct):
        v += a * vnoise(P, scale / (2 ** o), seed + 17 * o)
        tot += a
        a *= 0.5
    return v / tot


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


# ================================================================================================ the mesh
def arrays(M):
    P = np.array(M["points"], float).reshape(-1, 3)
    N = np.array(M["facings"], float).reshape(-1, 3)
    N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    T = np.array(M["triangles"], np.int64).reshape(-1, 3)
    pa = np.array(M["paints"], int)
    a = np.array(M["part"], int)
    b = np.array(M["part2"], int)
    w = np.array(M["w2"], float)
    return P, N, T, pa, a, b, w


def neighbours(n, T):
    nb = [set() for _ in range(n)]
    for x, y, z in T:
        nb[x].update((y, z))
        nb[y].update((x, z))
        nb[z].update((x, y))
    return nb


def curvature(P, N, T):
    """Signed mean-curvature proxy per point: + a hollow (the Laplacian points along the facing), - a ridge; in 1/m."""
    nb = neighbours(len(P), T)
    k = np.zeros(len(P))
    for i, s in enumerate(nb):
        if not s:
            continue
        s = list(s)
        L = P[s].mean(0) - P[i]
        e = np.linalg.norm(P[s] - P[i], axis=1).mean()
        if e > 1e-6:
            k[i] = (L @ N[i]) / (e * e)
    return k


def ray_dirs(n, seed=7):
    """n cosine-weighted directions about +z (a fixed spiral, so every point gets the same set)."""
    i = np.arange(n) + 0.5
    r = np.sqrt(i / n)
    th = i * math.pi * (3 - math.sqrt(5)) + seed
    return np.stack([r * np.cos(th), r * np.sin(th), np.sqrt(1 - r * r)], 1)


def groups(D, M):
    """Who shades whom: 0 the body (shaded by the body and the hands, never by the rifle, which is somewhere else in
    every pose), 1 the hands (by everything: the grip), 2 the weapon (by the weapon only). Per point and per triangle."""
    names = [b["name"] for b in D["bones"]]
    a = np.array(M["part"], int)
    T = np.array(M["triangles"], np.int64).reshape(-1, 3)
    weap = [i for i, n in enumerate(names) if n in ("weapon", "bolt") or n.startswith("weapon")]
    hands = [i for i, n in enumerate(names) if n.split("_")[0] in ("hand", "fingers", "thumb")]
    g = np.zeros(len(a), int)
    g[np.isin(a, hands)] = 1
    g[np.isin(a, weap)] = 2
    return g, g[T[:, 0]]


def occlusion(P, N, T, g=None, gt=None, rays=RAYS, reach=REACH):
    """Per point, the share of its hemisphere shut in within `reach` (near hits weigh more), by the triangles its group
    sees (see groups; with none given, by all)."""
    import bpy  # noqa: F401  (brings mathutils)
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree

    def tree(tris):
        if len(tris) == 0:
            return None
        return BVHTree.FromPolygons([tuple(map(float, p)) for p in P], [tuple(map(int, t)) for t in tris], epsilon=0.0)
    if g is None:
        g = np.zeros(len(P), int)
        gt = np.zeros(len(T), int)
    trees = {0: tree(T[gt != 2]), 1: tree(T), 2: tree(T[gt == 2])}
    D = ray_dirs(rays)
    occ = np.zeros(len(P))
    for i in range(len(P)):
        tr = trees[int(g[i])]
        if tr is None:
            continue
        n = N[i]
        t1 = np.cross(n, (0.0, 0.0, 1.0) if abs(n[2]) < 0.9 else (1.0, 0.0, 0.0))
        t1 /= np.linalg.norm(t1)
        t2 = np.cross(n, t1)
        o = Vector(P[i] + n * 0.003)
        hit = 0.0
        for d in D:
            v = t1 * d[0] + t2 * d[1] + n * d[2]
            loc, _, _, dist = tr.ray_cast(o, Vector(v), reach)
            if loc is not None:
                hit += 1.0 - 0.6 * (dist / reach)
        occ[i] = hit / len(D)
    return occ


def posed(rig, M, clip, t):
    P, N, T, pa, a, b, w = arrays(M)
    G, X = gore.pose(rig, clip, t)
    H = rig.head
    Q = gore.skin(P, a, b, w, G, X, H)
    Na = np.einsum("nij,nj->ni", G[a], N)
    Nb = np.einsum("nij,nj->ni", G[b], N)
    Nq = Na * (1 - w[:, None]) + Nb * w[:, None]
    Nq /= np.maximum(np.linalg.norm(Nq, axis=1, keepdims=True), 1e-9)
    return Q, Nq


# ================================================================================================ the shade
def shadow(D, log=print):
    """The man's shadow: his rest pose (arms a little out, as a man's body is mostly lit) counts 0.6, the bank of his
    poses 0.4 (so the inside of an arm that is shut in only now and then is not dark for good)."""
    rig = gore.Rig(D["bones"])
    M = D["near"]
    P, N, T, *_ = arrays(M)
    g, gt = groups(D, M)
    t0 = time.time()
    rest = occlusion(P, N, T, g, gt)
    bank = []
    for name, f in POSES:
        if name in D["clips"]:
            c = D["clips"][name]
            Q, Nq = posed(rig, M, c, f * c["long"])
            bank.append(occlusion(Q, Nq, T, g, gt))
    occ = 0.6 * rest + 0.4 * (np.mean(bank, 0) if bank else rest)
    log(f"    shadow, rest + {len(bank)} poses: {time.time() - t0:.0f} s, mean {occ.mean():.3f}")
    return occ


def near_shade(D, log=print, occ=None):
    rig = gore.Rig(D["bones"])
    M = D["near"]
    P, N, T, pa, a, b, w = arrays(M)
    if occ is None:
        occ = shadow(D, log)

    kinds = np.array([kind_of(p) for p in pa])
    k_ao = np.array([AO_K[k] for k in kinds])
    # (a little shadow from a strap's own edge or a neighbouring fold is no shadow: open cloth stays its own colour)
    s = 1.0 - k_ao * smooth(OCC0, OCC1, np.clip(occ, 0, 1))

    # creases and worn edges (rest pose: the cloth's own folds)
    kc = curvature(P, N, T)
    scale = np.percentile(np.abs(kc), 90) or 1.0
    kn = np.clip(kc / scale, -1.5, 1.5)
    cloth = np.isin(pa, list(CLOTH))
    s *= np.where(kn > 0, 1.0 - 0.12 * np.clip(kn, 0, 1) * np.where(cloth, 1.0, 0.6), 1.0)
    edge = np.isin(pa, [2, 4, 5, 9, 12])
    s *= np.where((kn < 0) & edge, 1.0 + 0.05 * np.clip(-kn, 0, 1), 1.0)

    # grime, in the rest pose's places (z up from the soles)
    H = rig.head
    z = P[:, 2]
    knee_z = 0.5 * (H[rig.index["calf_l"], 2] + H[rig.index["calf_r"], 2])
    blot = fbm(P, 0.09, seed=3)                  # splashes
    fade = fbm(P, 0.22, seed=11)                 # the cloth's uneven fade
    legs = np.isin(pa, list(LEGS))
    # mud: from the soles to a little over the knee, in splashes, the front of the shins more
    mud_h = 1.0 - smooth(0.06, knee_z + 0.08, z + 0.10 * (blot - 0.5))
    front = np.clip(N[:, 0], 0, 1)
    mud = mud_h * (0.55 + 0.45 * blot) * (0.85 + 0.15 * front)
    s *= np.where(legs, 1.0 - 0.30 * mud, 1.0)
    # the knees (he kneels in the dirt), the seat of the trousers, the elbows
    for side in ("l", "r"):
        kj = H[rig.index["calf_" + side]]
        d = np.linalg.norm(P - kj - np.array((0.05, 0.0, 0.0)), axis=1)
        s *= np.where(legs & (d < 0.11), 1.0 - 0.16 * (1 - d / 0.11) * (0.6 + 0.4 * blot), 1.0)
        ej = H[rig.index["lowerarm_" + side]]
        d = np.linalg.norm(P - ej, axis=1)
        s *= np.where(cloth & (d < 0.08), 1.0 - 0.10 * (1 - d / 0.08), 1.0)
        hj = H[rig.index["hand_" + side]]
        d = np.linalg.norm(P - hj, axis=1)
        s *= np.where((pa == 7) & (d < 0.16), 1.0 - 0.13 * (0.5 + 0.5 * blot), 1.0)     # dirty hands
        s *= np.where((pa == 0) & (d < 0.20), 1.0 - 0.10 * (1 - d / 0.20), 1.0)          # cuffs
    pel = H[rig.index["pelvis"]]
    seat = (pa == 1) & (P[:, 0] < pel[0] - 0.04) & (np.abs(z - (pel[2] - 0.12)) < 0.14)
    s *= np.where(seat, 0.93, 1.0)
    # the face: a little grime and sweat in the creases, never a mask
    head = H[rig.index["head"]]
    face = (pa == 7) & (np.linalg.norm(P - head, axis=1) < 0.16)
    s *= np.where(face, 1.0 - 0.07 * blot, 1.0)
    # the blotchy fade over all the cloth (up to 6 % darker here and there)
    s *= np.where(cloth, 0.94 + 0.06 * fade, 1.0)
    return np.clip(s, LO, HI), occ


def carry(Pn, Nn, sn, pn, P, N, p, k=6):
    """Shade for points P (facing N, paint p) from the near's (Pn, Nn, sn, pn): the k nearest of the same paint (or any)
    that face the same way, weighted by distance."""
    out = np.ones(len(P))
    by_paint = {q: np.where(pn == q)[0] for q in set(pn.tolist())}
    for i in range(len(P)):
        if p[i] in GORE:
            out[i] = 1.0
            continue
        cand = by_paint.get(p[i])
        if cand is None or len(cand) == 0:
            cand = np.arange(len(Pn))
        d = np.linalg.norm(Pn[cand] - P[i], axis=1)
        d = d + 0.03 * (1.0 - np.clip(Nn[cand] @ N[i], -1, 1))
        o = np.argsort(d)[:k]
        wt = 1.0 / (d[o] + 0.004)
        out[i] = float((sn[cand][o] * wt).sum() / wt.sum())
    return out


def shade_file(D, log=print):
    sn, occ = near_shade(D, log)
    Pn, Nn, _, pn, *_ = arrays(D["near"])
    res = {"near": sn}
    for lod in ("mid", "far"):
        if lod in D:
            P, N, _, p, *_ = arrays(D[lod])
            res[lod] = 1.0 - LOD_K[lod] * (1.0 - carry(Pn, Nn, sn, pn, P, N, p))
    # each helmet finish (drawn instead of his own helmet, in the same place): the nearest of his own, by paint
    for i, h in enumerate(D.get("helmets") or []):
        for lod in ("near", "mid", "far"):
            if lod in h and h[lod].get("points"):
                P, N, _, p, *_ = arrays(h[lod])
                res[f"h{i}_{lod}"] = 1.0 - LOD_K[lod] * (1.0 - carry(Pn, Nn, sn, pn, P, N, p))
    return res


def shade_twin(Y, X_shade, X, log=print):
    """A gore twin: his own points keep his shade, the appended ones take the nearest."""
    res = {}
    for lod, s in X_shade.items():
        P, N, _, p, *_ = arrays(Y[lod])
        Px, Nx, _, px, *_ = arrays(X[lod])
        n0 = len(s)
        extra = carry(Px, Nx, np.asarray(s), px, P[n0:], N[n0:], p[n0:])
        res[lod] = np.concatenate([np.asarray(s), extra])
    return res


def mesh_of(D, key):
    """The mesh a staged array belongs to: near/mid/far, or h<i>_<lod> for helmet finish i."""
    if key.startswith("h") and "_" in key:
        i, lod = key[1:].split("_", 1)
        return D["helmets"][int(i)][lod]
    return D[key]


def put(D, res):
    for key, s in res.items():
        mesh_of(D, key)["shade"] = [round(float(x), 3) for x in s]


def report(D, res, log=print):
    for lod, s in res.items():
        if lod not in ("near", "mid", "far"):
            continue
        s = np.asarray(s)
        p = np.array(D[lod]["paints"])
        by = {nm: s[p == i].mean() for nm, i in PAINT.items() if (p == i).any()}
        log(f"    {lod}: {len(s)} points, shade {s.min():.2f}-{s.max():.2f} (mean {s.mean():.3f}); "
            + ", ".join(f"{k} {v:.2f}" for k, v in by.items()))


# ================================================================================================ files
def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def stage(name, log=print):
    src = os.path.join(SHARED, name + ".json")
    D = json.load(open(src))
    log(f"{name}: {os.path.getsize(src):,} bytes")
    res = shade_file(D, log)
    report(D, res, log)
    os.makedirs(STAGE, exist_ok=True)
    np.savez(os.path.join(STAGE, name + "_shade.npz"), **{k: np.asarray(v) for k, v in res.items()})
    return res


def deliver(name, log=print):
    """Merge the staged shade into the shared X.json as it is NOW (only near/mid/far `shade` added or replaced; the
    point arrays must be the ones the shade was made on), after a backup; write to a temp file and swap it in; read back."""
    z = np.load(os.path.join(STAGE, name + "_shade.npz"))
    dest = os.path.join(SHARED, name + ".json")
    for attempt in range(3):
        h0 = sha(dest)
        raw = open(dest).read()
        D = json.loads(raw)
        for key in z.files:
            try:
                n_now = len(mesh_of(D, key)["paints"])
            except (KeyError, IndexError):
                n_now = -1
            if n_now != len(z[key]):
                raise SystemExit(f"{name} {key}: {n_now} points now, shade made for {len(z[key])}: re-stage")
        os.makedirs(BACKUP, exist_ok=True)
        bk = os.path.join(BACKUP, f"{name}.{time.strftime('%H%M%S')}.json")
        open(bk, "w").write(raw)
        put(D, {k: z[k] for k in z.files})
        out = json.dumps(D, separators=(",", ":"))
        if sha(dest) != h0:
            log(f"{name}: changed under us, again")
            continue
        tmp = dest + ".shade-tmp"
        open(tmp, "w").write(out)
        os.replace(tmp, dest)
        time.sleep(10)
        back = open(dest).read()
        ok = back == out
        log(f"{name}: shade written ({len(out):,} bytes, was {len(raw):,}); read back {'ok' if ok else 'MISMATCH'}; backup {bk}")
        return ok
    raise SystemExit(f"{name}: kept changing; not written")


def stage_twin(name, log=print):
    """X_gore.json from X's staged shade (stage X first): his points keep theirs, the appended ones take the nearest;
    the helmet finishes are his own, so they take his finishes' shade as they are."""
    X = json.load(open(os.path.join(SHARED, name + ".json")))
    Y = json.load(open(os.path.join(SHARED, name + "_gore.json")))
    z = np.load(os.path.join(STAGE, name + "_shade.npz"))
    res = shade_twin(Y, {k: z[k] for k in ("near", "mid", "far") if k in z.files}, X, log)
    for k in z.files:
        if k.startswith("h") and len(mesh_of(Y, k)["paints"]) == len(z[k]):
            res[k] = z[k]
    report(Y, res, log)
    np.savez(os.path.join(STAGE, name + "_gore_shade.npz"), **{k: np.asarray(v) for k, v in res.items()})
    return res


def soldier_files():
    names = []
    for f in sorted(os.listdir(SHARED)):
        if f.startswith("soldier_") and f.endswith(".json") and not f.endswith("_gore.json"):
            names.append(f[:-5])
    return names


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "--deliver":
        for nm in a[1:]:
            deliver(nm)
    elif a and a[0] == "--twin":
        for nm in a[1:]:
            stage_twin(nm)
    else:
        for nm in (soldier_files() if a == ["--all"] else (a or ["soldier_USA"])):
            stage(nm)
