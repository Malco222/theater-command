"""A helmet's underside drawn through its top, mended in finished soldier files (the art director, 23 November 2026).

soldier.py makes a helmet's inside by drawing its outside again a little in toward a point inside it. Where the shell
flares out (a brim, a flared skirt, a rim turned outward) that offset comes out above the top: from the game's camera,
which looks down on the men, one sees the inside's dark underside as a black lip round the rim (on the British
Brodie's wide brim it was a black disc: make_ukfra.py mends that one as it builds the file).

This tool takes each soldier file as it is and moves only points of the helmet's own steel (paint 5) that belong to
faces turned downward (an underside), straight down until every such face lies at least GAP under the topmost
upward-facing steel above it. It is tried at seven places on every underside face, a few times over, because the two
sides' triangles run across each other. The same points of every finish's shell move the same way (matched by place
and facing; a point a finish cut into an edge moves as the edge does). A chin strap, forehead, ear or hair point that
lay under the underside before and would stand through it after is put a millimetre under it. Nothing else changes:
no paint, shade, wear, index, facing, extra (net, scrim, leaves, cover, decal), badge slab or upward-facing point.

    /home/user/bpyenv/bin/python brim_mend.py measure FILE...         (the fault per level of detail)
    /home/user/bpyenv/bin/python brim_mend.py mend IN.json OUT.json   (OUT written through a temp file)

Run it again after any re-export or variety.py helmet rebuild (they write soldier.py's shells afresh), then shade.py
is NOT needed again: shade is per point and the points keep theirs.
"""
import json, os, sys
import numpy as np
import bpy  # noqa: F401  (Blender as a module: mathutils comes with it)
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from scipy.spatial import cKDTree

LODS = ("near", "mid", "far")
STEEL = 5            # (the helmet's paint)
UP, DOWN = 0.3, -0.3  # (a face whose facing's z is over UP is seen from above; under DOWN, an underside)
GAP = 0.001          # (an underside at least a millimetre under the top above it)
MOST = 0.010         # (no point moves more than this in all)
PAIR = 0.010         # (an underside more than this over the top under it is not that top's other side: left)
W7 = 0.9 * np.array([(1, 0, 0), (0, 1, 0), (0, 0, 1), (0.5, 0.5, 0), (0, 0.5, 0.5), (0.5, 0, 0.5), (1 / 3, 1 / 3, 1 / 3)]) + 0.1 / 3
NEAR_PT = 0.00015    # (a finish's shell point lies on the man's own helmet to within rounding)


def geo(P, T):
    A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    G = -np.cross(B - A, C - A)                    # (the file's winding: clockwise seen from outside)
    return G / (np.linalg.norm(G, axis=1, keepdims=True) + 1e-12)


def bvh_of(P, T):
    return BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in T])


def sorts(m, H):
    """The helmet's steel faces split into tops (seen from above) and undersides, by the file as delivered."""
    P = np.array(m["points"], float).reshape(-1, 3)
    pa = np.array(m["paints"])
    G = geo(P, H)
    steel = (pa[H] == STEEL).all(1)
    return P, G, H[steel & (G[:, 2] > UP)], H[steel & (G[:, 2] < DOWN)]


def shortfalls(P, top, under):
    """For every underside face and each of its seven places: how far it stands above (the top over it less GAP);
    nan where no top lies over it."""
    bvh = bvh_of(P, top)
    S = np.full((len(under), len(W7)), np.nan)
    for k, t in enumerate(under):
        Q = W7 @ P[t]
        for j, q in enumerate(Q):
            hit = bvh.ray_cast(Vector((q[0], q[1], q[2] + 0.05)), Vector((0.0, 0.0, -1.0)), 0.1)[0]
            if hit is not None:
                S[k, j] = q[2] - (hit.z - GAP)
    return S


def mend_lod(m, H, log, name=""):
    P0, G, top, under = sorts(m, H)
    pa = np.array(m["paints"])
    fixed = np.zeros(len(P0), bool)
    fixed[np.unique(top)] = True                   # (a point a top uses stays: the top must not move)
    fixed |= pa != STEEL
    mov = ~fixed[under]                            # (n_under, 3)
    P = P0.copy()
    before = shortfalls(P, top, under)
    rounds, capped = 0, 0
    for rounds in range(1, 11):
        S = shortfalls(P, top, under) if rounds > 1 else before
        need = np.zeros(len(P))
        for k, j in zip(*np.where((S > 1e-5) & (S < PAIR))):
            w = W7[j] * mov[k]
            if w.sum() < 0.3:                      # (beside a point shared with the top: the two meet at the edge)
                continue
            d = S[k, j] / w.sum()
            for i in under[k][mov[k]]:
                need[i] = max(need[i], d)
        if not need.any():
            break
        room = MOST - (P0[:, 2] - P[:, 2])
        capped += int((need > room + 1e-9).sum())
        P[:, 2] -= np.minimum(need, room)
    after = shortfalls(P, top, under)
    ok = np.zeros(len(under), bool)
    left = 0
    for k, j in zip(*np.where((after > 1e-5) & (after < PAIR))):
        if (W7[j] * mov[k]).sum() >= 0.3:
            left += 1
    dz = P0[:, 2] - P[:, 2]
    rep = dict(top=len(top), under=len(under), through_before=int(((before > GAP) & (before < PAIR)).sum()),
               short_before=int(((before > 1e-5) & (before < PAIR)).sum()), unpaired=int((before >= PAIR).sum()), most_through_mm=round(float(before[before < PAIR].max() - GAP) * 1000, 2) if (before < PAIR).any() else None,
               moved=int((dz > 1e-7).sum()), most_moved_mm=round(float(dz.max()) * 1000, 2), rounds=rounds, capped=capped, left=left)
    return P, dz, rep


def tuck(m, H, P0, P, log):
    """Points of the man (not the helmet) that lay under an underside before and would stand through it now: put a
    millimetre under it. Only z moves, so a face's height over a point is found at the same x, y before and after."""
    _, _, top, under = sorts(dict(m, points=list(P0.ravel())), H)
    if not len(under):
        return 0, 0.0
    used = np.unique(H)
    ax = P0[used, :2].mean(0)
    R = np.hypot(*(P0[used, :2] - ax).T).max() + 0.005
    z0 = P0[used, 2].min() - 0.03
    isin = np.zeros(len(P0), bool)
    isin[used] = True
    cand = np.where(~isin & (np.hypot(*(P0[:, :2] - ax).T) < R) & (P0[:, 2] > z0))[0]
    bvh = bvh_of(P0, under)
    moved, most = 0, 0.0
    for i in cand:
        p = P[i]
        hit, _, idx, _ = bvh.ray_cast(Vector((p[0], p[1], p[2])), Vector((0.0, 0.0, 1.0)), 0.03)
        if hit is None:
            continue
        t = under[idx]
        a, b, c = P0[t]
        # (where the face was over the point, as weights of its corners: same x, y now)
        v0, v1, v2 = b - a, c - a, np.array(hit) - a
        d00, d01, d11, d20, d21 = v0[:2] @ v0[:2], v0[:2] @ v1[:2], v1[:2] @ v1[:2], v2[:2] @ v0[:2], v2[:2] @ v1[:2]
        den = d00 * d11 - d01 * d01
        if abs(den) < 1e-14:
            continue
        wv = (d11 * d20 - d01 * d21) / den
        ww = (d00 * d21 - d01 * d20) / den
        znew = (1 - wv - ww) * P[t[0], 2] + wv * P[t[1], 2] + ww * P[t[2], 2]
        if p[2] > znew - 0.001:
            most = max(most, p[2] - (znew - 0.001))
            P[i, 2] = znew - 0.001
            moved += 1
    return moved, most


def carry(h, P0, N0, H, dz):
    """A finish's shell points take the move of the man's own helmet where they lie: a copied point its source's, a
    point cut into an edge or face the move there. Extras lie off the shell (lifted, or on their own) and stay."""
    used = np.unique(H)
    Ph = np.array(h["points"], float).reshape(-1, 3)
    Nh = np.array(h["facings"], float).reshape(-1, 3)
    kd = cKDTree(P0[used])
    bvh = bvh_of(P0, H)
    G = geo(P0, H)
    out = np.zeros(len(Ph))
    same = cut = 0
    dd, ii = kd.query(Ph, k=6, distance_upper_bound=NEAR_PT)
    for p in range(len(Ph)):
        best, bi = 0.5, -1
        for d, i in zip(dd[p], ii[p]):
            if np.isfinite(d) and i < len(used):
                f = N0[used[i]] @ Nh[p]
                if f > best:
                    best, bi = f, used[i]
        if bi >= 0:
            out[p] = dz[bi]
            same += 1
            continue
        if not np.any(dz):
            continue
        hits = bvh.find_nearest_range(Vector(Ph[p]), NEAR_PT)
        bestf, bt, loc = 0.3, -1, None
        for l, _, idx, _ in hits:
            f = G[idx] @ Nh[p]
            if f > bestf:
                bestf, bt, loc = f, idx, np.array(l)
        if bt < 0:
            continue
        t = H[bt]
        a, b, c = P0[t]
        v0, v1, v2 = b - a, c - a, loc - a
        d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
        den = d00 * d11 - d01 * d01
        if abs(den) < 1e-16:
            continue
        wv = (d11 * d20 - d01 * d21) / den
        ww = (d00 * d21 - d01 * d20) / den
        out[p] = (1 - wv - ww) * dz[t[0]] + wv * dz[t[1]] + ww * dz[t[2]]
        cut += 1
    return out, same, cut


def mend(D, log=print):
    """The file mended in place; returns what was done, per level of detail."""
    said = {}
    for lod in LODS:
        m = D[lod]
        a, c = D["helmet_tris"][lod]
        H = np.array(m["triangles"]).reshape(-1, 3)[a:a + c]
        P0 = np.array(m["points"], float).reshape(-1, 3)
        N0 = np.array(m["facings"], float).reshape(-1, 3)
        P, dz, rep = mend_lod(m, H, log)
        rep["tucked"], most = tuck(m, H, P0, P, log)
        rep["tucked_most_mm"] = round(most * 1000, 2)
        moved = np.where(np.abs(P[:, 2] - P0[:, 2]) > 1e-7)[0]
        for i in moved:
            m["points"][3 * i + 2] = round(float(P[i, 2]), 4)
        fin = []
        for h in D.get("helmets", []):
            off, same, cut = carry(h[lod], P0, N0, H, dz)
            k = np.where(off > 1e-7)[0]
            for i in k:
                h[lod]["points"][3 * i + 2] = round(float(h[lod]["points"][3 * i + 2] - off[i]), 4)
            fin.append((h["name"], len(k), same, cut, len(off)))
        rep["finishes"] = fin
        said[lod] = rep
        log(f"   {lod}: {rep['short_before']} underside places short ({rep['through_before']} through the top, "
            f"{rep['most_through_mm']} mm at most); {rep['moved']} points down ({rep['most_moved_mm']} mm at most, "
            f"{rep['rounds']} rounds, {rep['capped']} capped, {rep['left']} places left); {rep['tucked']} of the man tucked; "
            f"finishes moved {sum(f[1] for f in fin)} points")
    return said


def measure(D):
    out = {}
    for lod in LODS:
        m = D[lod]
        a, c = D["helmet_tris"][lod]
        H = np.array(m["triangles"]).reshape(-1, 3)[a:a + c]
        P, G, top, under = sorts(m, H)
        S = shortfalls(P, top, under)
        out[lod] = dict(top=len(top), under=len(under), through=int((S > GAP).sum()), short=int((S > 1e-5).sum()),
                        most_mm=round(float(np.nanmax(S) - GAP) * 1000, 2) if np.isfinite(S).any() else None)
    return out


if __name__ == "__main__":
    if sys.argv[1] == "measure":
        for p in sys.argv[2:]:
            print(os.path.basename(p)[:-5], measure(json.load(open(p))), flush=True)
    elif sys.argv[1] == "mend":
        src, dst = sys.argv[2], sys.argv[3]
        D = json.load(open(src))
        print(os.path.basename(src)[:-5], flush=True)
        rep = mend(D)
        tmp = dst + ".tmp"
        with open(tmp, "w") as f:
            json.dump(D, f, separators=(",", ":"))
        json.load(open(tmp))
        os.replace(tmp, dst)
        json.dump(rep, open(dst[:-5] + ".mend.json", "w"), indent=1)
