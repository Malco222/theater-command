"""The far level re-made for the five riflemen whose far level came out as sheets (release 2).

The Chinese, French, Japanese, Polish and British riflemen's far levels (the mesh the battle draws past the middle
level's range, and the pose bank's) had their body torn into long sheets: triangles from his chest to his feet (edges
of 0.9 to 1.2 m), so a man of those armies read as a draped tent. The near-to-far thinning had crushed his puttees and
leggings into a few points and joined across them. The German, Italian, Soviet and American files are sound.

Here each of the five gets a new far BODY (cloth and skin: the run of triangles before his kit), cut down from his own
middle level's body with far_levels.py's quadric thinning (a paint's edge held as a crease, an edge between two bones
costing more, so knees and elbows still bend), to the same number of points his far body had. Every new point reads
its arrays off the middle level's surface it stands on: its bones and their mix, facing, shade and wear, tint, and in
each face its move, facing and tint. His far kit, rifle and helmet are kept exactly as they were (their triangles
after the body's, the helmet's still last), so helmet_tris keeps its count and the helmet finishes still fit; pack_tris
is marked again (pack_range.py). Near and mid are not touched.

His gore twin's far level is cut again from the new far (gore.py's own split and caps, the far level only: its
near and mid, clips and gore deaths are kept), and its wear arrays carried over as shade.py does for a twin.

    /home/user/bpyenv/bin/python far_mend.py --stage [names]     stage the files in /home/user/art/far_mend/stage/
    /home/user/bpyenv/bin/python far_mend.py --check [names]     the staged files' far levels: longest edges, counts
    /home/user/bpyenv/bin/python far_mend.py --deliver [names]   write them over the shared ones (backed up first)
    /home/user/bpyenv/bin/python far_mend.py --into DIR [names]  and over the game's copies in DIR (backed up first)
"""
import json
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import far_levels as FL  # noqa: E402
import gore  # noqa: E402
import pack_range as PR  # noqa: E402
import shade  # noqa: E402

SHARED = "/mnt/project-files/battle-models/land/soldier/"
STAGE = "/home/user/art/far_mend/stage/"
BACKUP = "/home/user/art/far_mend/backup/"
NAMES = ["soldier_CHN", "soldier_FRA", "soldier_JAP", "soldier_POL", "soldier_UK"]
KEEP_FROM = 0.0012        # (pieces of the body smaller than this, m2 (the eyes), left out, as the old far had none)
MOVE_MIN = 5e-5           # (a face's move on a far point under this is none)
TINT_MIN = 0.02           # (a face's tint within this of 1 is none)
HANDS = ("hand_", "fingers_", "thumb_")
ABOUT_FAR = ("far level's body re-made by far_mend.py (24 November 2026, release 2): cut down from the middle level's "
             "body to the old far body's size (the old one had been torn into sheets); kit, rifle and helmet as before.")


def tris(M):
    return np.array(M["triangles"], np.int64).reshape(-1, 3)


def longest(M, T=None):
    T = tris(M) if T is None else T
    X = np.array(M["points"], float).reshape(-1, 3)
    return np.max([np.linalg.norm(X[T[:, i]] - X[T[:, (i + 1) % 3]], axis=1) for i in range(3)], axis=0)


def face_extras(D):
    """Each face's tint, move and facing over the middle level's points (none: 1, 0 and his own facing)."""
    M = D["mid"]
    n = len(M["paints"])
    N0 = np.array(M["facings"], float).reshape(-1, 3)
    out = []
    for fc in D.get("faces", []):
        f = fc.get("mid", {})
        tint = np.ones((n, 3))
        if f.get("tint_at"):
            tint[np.array(f["tint_at"])] = np.array(f["tint"], float).reshape(-1, 3)
        move = np.zeros((n, 3))
        facing = N0.copy()
        if f.get("at"):
            at = np.array(f["at"])
            move[at] = np.array(f["move"], float).reshape(-1, 3)
            facing[at] = np.array(f["facing"], float).reshape(-1, 3)
        out += [tint, move, facing]
    return out


def remake(D, log=print):
    """D with its far level's body made again from its middle level (D is changed and returned)."""
    names = [b["name"] for b in D["bones"]]
    nb = len(names)
    Bm = PR.classify(D, "mid")[0]
    Bf = PR.classify(D, "far")[0]
    F = D["far"]
    T = tris(F)
    hf, hc = D["helmet_tris"]["far"]
    assert hf + hc == len(T), "the far helmet is not the last run"
    assert len(np.intersect1d(np.unique(T[:Bf]), np.unique(T[Bf:]))) == 0, "the far body shares points with the kit"
    target = len(np.unique(T[:Bf]))
    old_long = float(longest(F, T[:Bf]).max())
    S = FL.Surface(D["mid"], (0, Bm))
    V, Fc, keys = FL.cut(S, target, KEEP_FROM, nb, log, "far body from the middle level's")
    extras = face_extras(D)
    L, worst, ex = FL.emit(S, V, Fc, keys, nb, extras)
    nbp = len(L["paints"])
    # (a corner where two paints meet is a point per paint, each read off its own paint's surface: their bones could
    # differ, and the seam opened as he moved (5 cm in a cower). Each such set takes one mix of bones, their sum, and
    # one move in each face)
    P = np.array(L["points"]).reshape(-1, 3)
    groups = {}
    for i, p in enumerate(map(tuple, P.tolist())):
        groups.setdefault(p, []).append(i)
    joined = 0
    for g in groups.values():
        if len(g) < 2:
            continue
        bw = np.zeros(nb)
        for i in g:
            bw[L["part"][i]] += 1 - L["w2"][i]
            bw[L["part2"][i]] += L["w2"][i]
        o = np.argsort(-bw)
        a, c = int(o[0]), int(o[1])
        w = round(float(bw[c] / (bw[a] + bw[c])), 3) if bw[c] > 1e-4 * (bw[a] + bw[c]) else 0.0
        for i in g:
            L["part"][i], L["part2"][i], L["w2"][i] = a, (c if w > 0 else a), w
        for fi in range(len(D.get("faces", []))):
            mv = np.array([ex[3 * fi + 1][i] for i in g]).mean(0)
            for i in g:
                ex[3 * fi + 1][i] = mv
        joined += 1
    log(f"    {joined} corners where paints meet given one mix of bones")
    # (his hands' skin last in the body's run, as in every level: pack_range.py reads the body's end there)
    Tb = np.array(L["triangles"], np.int64).reshape(-1, 3)
    part = np.array(L["part"])
    hand = np.array([L["paints"][t[0]] == 7 and names[np.bincount(part[t]).argmax()].startswith(HANDS) for t in Tb])
    Tb = np.concatenate([Tb[~hand], Tb[hand]])
    # the kit, rifle and helmet as they were, their points after the body's
    rest = T[Bf:]
    used = np.unique(rest)
    remap = -np.ones(len(F["paints"]), np.int64)
    remap[used] = nbp + np.arange(len(used))
    new = {}
    for k, v in F.items():
        if k == "triangles":
            continue
        w = 3 if len(v) == 3 * len(F["paints"]) else 1
        old = np.array(v).reshape(-1, w)[used].ravel().tolist() if w == 3 else [v[i] for i in used]
        mine = L.get(k)
        if mine is None:
            raise SystemExit(f"far array {k}: not made for the new body")
        new[k] = list(mine) + old
    new["triangles"] = Tb.ravel().tolist() + remap[rest].ravel().tolist()
    order = ["points", "facings", "paints", "triangles", "part", "part2", "w2"] + [k for k in F if k not in
                                                                                  ("points", "facings", "paints", "triangles", "part", "part2", "w2")]
    D["far"] = {k: new[k] for k in order}
    nBf = len(Tb)
    D["helmet_tris"]["far"] = [nBf + (hf - Bf), hc]
    # each face: its move, facing and tint on the new body's points; on the kit's and helmet's, as they were
    for fi, fc in enumerate(D.get("faces", [])):
        tint, move, facing = (np.array(ex[3 * fi + j]) for j in range(3))
        old = fc.get("far", {})
        f = {}
        if "at" in old:
            at = [i for i in range(nbp) if np.linalg.norm(move[i]) > MOVE_MIN]
            mv = [round(float(x), 4) for i in at for x in move[i]]
            fa = [round(float(x), 3) for i in at for x in facing[i] / max(np.linalg.norm(facing[i]), 1e-9)]
            oa = np.array(old.get("at", []), np.int64)
            keep = [j for j, o in enumerate(oa) if remap[o] >= 0]
            at += [int(remap[oa[j]]) for j in keep]
            mv += [x for j in keep for x in old["move"][3 * j:3 * j + 3]]
            fa += [x for j in keep for x in old["facing"][3 * j:3 * j + 3]]
            f.update({"at": at, "move": mv, "facing": fa})
        ta = [i for i in range(nbp) if np.abs(tint[i] - 1.0).max() > TINT_MIN]
        tv = [round(float(x), 3) for i in ta for x in tint[i]]
        ot = np.array(old.get("tint_at", []), np.int64)
        keep = [j for j, o in enumerate(ot) if remap[o] >= 0]
        ta += [int(remap[ot[j]]) for j in keep]
        tv += [x for j in keep for x in old["tint"][3 * j:3 * j + 3]]
        f.update({"tint_at": ta, "tint": tv})
        fc["far"] = f
    new_long = float(longest(D["far"], Tb).max())
    D["far_mend"] = {"about": ABOUT_FAR, "body_points": [target, nbp], "body_triangles": [int(Bf), int(nBf)],
                     "longest_edge_m": [round(old_long, 3), round(new_long, 3)]}
    log(f"    far body: {target} -> {nbp} points, {Bf} -> {nBf} triangles; longest edge {old_long:.2f} -> {new_long:.2f} m; "
        f"off the middle level's surface: median {np.median(worst) * 100:.2f} cm, most {worst.max() * 100:.2f} cm")
    return D


def twin_far(D, Y, log=print):
    """The gore twin Y with its far level cut again from D's new far (near, mid, clips and deaths kept)."""
    tw, stats, rig = gore.make_twin(D, log=lambda s: log("      " + s) if s.lstrip().startswith("far") else None)
    Y["far"] = tw["far"]
    assert [c["bone"] for c in Y["gore"]["cuts"]] == [c["bone"] for c in tw["gore"]["cuts"]]
    for c, cn in zip(Y["gore"]["cuts"], tw["gore"]["cuts"]):
        c["far"] = cn["far"]
    Y["gore"]["per_lod"]["far"] = tw["gore"]["per_lod"]["far"]
    Y["gore"]["joined"]["far"] = tw["gore"]["joined"]["far"]
    for k in ("faces", "helmet_tris"):
        Y[k] = D[k]
    xr = {"far": {k: (np.array(D["far"][k], float).reshape(-1, 3) if k == "tint" else np.array(D["far"][k], float))
                  for k in shade.WEAR_KEYS}}
    res = shade.shade_twin(Y, xr, D)
    for k, v in res["far"].items():
        Y["far"][k] = [round(float(x), 3) for x in np.asarray(v).ravel()]
    Y["far_mend"] = {"about": "far level cut again from his own file's re-made far (far_mend.py, 24 November 2026)."}
    log(f"    twin far: {len(Y['far']['paints'])} points, {len(Y['far']['triangles']) // 3} triangles; "
        f"longest edge {longest(Y['far']).max():.2f} m")
    return Y


def write(path, D):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(D, f, separators=(",", ":"), allow_nan=False)
    os.replace(tmp, path)


def stage(name, log=print):
    t0 = time.time()
    os.makedirs(STAGE, exist_ok=True)
    D = json.load(open(SHARED + name + ".json"))
    Y = json.load(open(SHARED + name + "_gore.json"))
    log(f"{name}:")
    D = remake(D, log)
    D = PR.with_range(D)
    D.pop("_pack_notes", None)
    Y = twin_far(D, Y, log)
    Y = PR.with_range(Y)
    Y.pop("_pack_notes", None)
    write(STAGE + name + ".json", D)
    write(STAGE + name + "_gore.json", Y)
    log(f"    pack_tris far: {D['pack_tris']['far']} (twin {Y['pack_tris']['far']}); staged in {time.time() - t0:.0f} s")


def check(name, folder=STAGE, log=print):
    out = []
    for nm in (name, name + "_gore"):
        D = json.load(open(folder + nm + ".json"))
        F = D["far"]
        e = longest(F)
        T = tris(F)
        hf, hc = D["helmet_tris"]["far"]
        bad = int((e[:hf] > 0.45).sum())
        n = len(F["paints"])
        lens = {k: len(v) for k, v in F.items()}
        ok_len = all(v in (n, 3 * n) for k, v in lens.items() if k != "triangles")
        ok_idx = int(T.max()) < n and int(T.min()) >= 0
        out.append(f"{nm}: far {n} points, {len(T)} triangles, body+kit edges over 45 cm {bad} (longest {e[:hf].max():.2f} m), "
                   f"arrays {'ok' if ok_len else 'WRONG'}, indices {'ok' if ok_idx else 'WRONG'}, pack {PR.check(folder + nm + '.json')}")
    for s in out:
        log("  " + s)
    return out


def deliver(name, dest, log=print):
    os.makedirs(BACKUP, exist_ok=True)
    stamp = time.strftime("%m%d_%H%M%S")
    for nm in (name, name + "_gore"):
        src, dst = STAGE + nm + ".json", os.path.join(dest, nm + ".json")
        tag = "shared" if dest.startswith("/mnt/") else "game"
        shutil.copy2(dst, os.path.join(BACKUP, f"{nm}.{tag}.{stamp}.json"))
        cur = json.load(open(dst))
        new = json.load(open(src))
        # (only the far level and what rides on it may differ from the file there now)
        for k in set(cur) | set(new):
            if k in ("far", "faces", "helmet_tris", "pack_tris", "far_mend", "gore"):
                continue
            if k in ("near", "mid"):
                if cur[k]["points"] != new[k]["points"] or cur[k]["triangles"] != new[k]["triangles"]:
                    raise SystemExit(f"{dst}: its {k} level changed since staging: stage again")
                continue
            if cur.get(k) != new.get(k):
                raise SystemExit(f"{dst}: {k} changed since staging: stage again")
        tmp = dst + ".tmp"
        shutil.copyfile(src, tmp)
        os.replace(tmp, dst)
        log(f"  {nm}: written to {dst}")


def main():
    a = sys.argv[1:]
    names = [x for x in a if x.startswith("soldier_")] or NAMES
    if "--stage" in a:
        for nm in names:
            stage(nm)
    if "--check" in a:
        for nm in names:
            check(nm)
    if "--deliver" in a:
        for nm in names:
            deliver(nm, SHARED)
        time.sleep(10)
        for nm in names:
            check(nm, SHARED)
    if "--into" in a:
        d = a[a.index("--into") + 1].rstrip("/") + "/"
        for nm in names:
            deliver(nm, d)
            check(nm, d)


if __name__ == "__main__":
    main()
