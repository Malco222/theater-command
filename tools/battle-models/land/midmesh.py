"""The middle mesh: what the game draws of a vehicle from about 40 to 140 m (the scale plan's TK2, ~15,000 triangles).

Made from the file's own near and far meshes, so every part, paint and colour channel means what it means there:
- the track's links go, and the far mesh's plain track band takes their place (the links are a few pixels at 40 m);
- pieces smaller than `small` (rivets, bolts, little handles) go, unless one is all its part has;
- each part is thinned by edge collapse (Blender's decimate) to the share of the budget left; a part made of fewer than
  `keep` triangles stays as it is;
- parts of one shape (the road wheels on a side) are thinned once and the same mesh is laid on each, so the game can
  draw them as one shared body;
- facings are worked out again: a corner takes the faces round it within `crease` degrees, so plates stay flat and
  round things stay round; every other channel comes from the nearest near point of the same part.

    middle(file_dict, target=15000) -> {"points", "facings", "paints", "triangles", "part", <the near's other channels>}

`part` is numbered as the far mesh numbers it: the parts list without the track links (the middle mesh has none).
"""
import math
import re
from collections import defaultdict

import numpy as np

import bpy  # noqa: F401  (bpy before mathutils)
from mathutils.kdtree import KDTree

LINK = re.compile(r"^track_[lr]\.\d+$")


def _weld(P):
    key = np.round(P * 1000.0).astype(np.int64)
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    return P[first], inv.ravel()


def _pieces(V, F):
    """Connected pieces of a welded mesh: a label per face."""
    par = np.arange(len(V))

    def find(a):
        r = a
        while par[r] != r:
            r = par[r]
        while par[a] != r:
            par[a], a = r, par[a]
        return r
    for a, b, c in F:
        ra = find(a)
        par[find(b)] = ra
        par[find(c)] = ra
    return np.array([find(a) for a in F[:, 0]])


def _decimate(V, F, fpaint, ratio):
    """Edge collapse to about `ratio` of the faces; returns (V, F, face paint)."""
    # (a face that names a welded point twice, or repeats another, can send the collapse round for ever: none go in)
    Fs = np.sort(F, axis=1)
    ok = (Fs[:, 0] != Fs[:, 1]) & (Fs[:, 1] != Fs[:, 2])
    _, once = np.unique(Fs, axis=0, return_index=True)
    single = np.zeros(len(F), dtype=bool)
    single[once] = True
    F, fpaint = F[ok & single], fpaint[ok & single]
    me = bpy.data.meshes.new("mid")
    me.from_pydata(V.tolist(), [], F.tolist())
    me.update()
    paints = sorted(set(int(p) for p in fpaint))
    for p in paints:
        me.materials.append(bpy.data.materials.get(f"mid{p}") or bpy.data.materials.new(f"mid{p}"))
    slot = {p: i for i, p in enumerate(paints)}
    me.polygons.foreach_set("material_index", [slot[int(p)] for p in fpaint])
    ob = bpy.data.objects.new("mid", me)
    bpy.context.scene.collection.objects.link(ob)
    mod = ob.modifiers.new("dec", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = float(ratio)
    mod.use_collapse_triangulate = True
    dg = bpy.context.evaluated_depsgraph_get()
    out = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    out.calc_loop_triangles()
    nv = len(out.vertices)
    co = np.zeros(nv * 3)
    out.vertices.foreach_get("co", co)
    nt = len(out.loop_triangles)
    tv = np.zeros(nt * 3, dtype=np.int64)
    out.loop_triangles.foreach_get("vertices", tv)
    mi = np.zeros(nt, dtype=np.int64)
    out.loop_triangles.foreach_get("material_index", mi)
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    bpy.data.meshes.remove(out)
    return co.reshape(-1, 3), tv.reshape(-1, 3), np.array(paints)[mi]


def _facings(V, F, crease):
    """A facing per corner: the faces round its point that lie within `crease` degrees of its own face, by area."""
    a, b, c = V[F[:, 0]], V[F[:, 1]], V[F[:, 2]]
    cr = np.cross(b - a, c - a)
    area = np.linalg.norm(cr, axis=1)
    fn = cr / np.maximum(area, 1e-12)[:, None]
    around = defaultdict(list)
    for f, tri in enumerate(F):
        for v in tri:
            around[int(v)].append(f)
    cos = math.cos(math.radians(crease))
    out = np.zeros((len(F), 3, 3))
    for f, tri in enumerate(F):
        for k, v in enumerate(tri):
            fs = np.array(around[int(v)])
            near = fs[fn[fs] @ fn[f] >= cos]
            s = (fn[near] * area[near, None]).sum(0)
            L = np.linalg.norm(s)
            out[f, k] = s / L if L > 1e-12 else fn[f]
    return out


def middle(d, target=15000, small=0.05, keep=40, crease=40.0):
    near, far = d["near"], d["far"]
    names = [p["name"] for p in d["parts"]]
    pivots = np.array([p.get("pivot", (0.0, 0.0, 0.0)) for p in d["parts"]], dtype=float)
    P = np.array(near["points"], dtype=float).reshape(-1, 3)
    T = np.array(near["triangles"], dtype=np.int64).reshape(-1, 3)
    part = np.array(near["part"], dtype=np.int64)
    paint = np.array(near["paints"], dtype=np.int64)
    chans = [k for k, v in near.items() if k not in ("points", "facings", "triangles", "part", "paints") and isinstance(v, list) and len(v) == len(part)]
    C = {k: np.array(near[k]) for k in chans}

    link = np.array([bool(LINK.match(n)) for n in names])
    tpart = part[T[:, 0]]
    # (the file's frame is mirrored and its triangles wound the other way about, so in its own numbers a face's
    # (b - a) x (c - a) points out of her as it is: no turning back needed)
    groups = defaultdict(list)
    for i, pi in enumerate(tpart):
        if not link[pi]:
            groups[int(pi)].append(i)

    # the parts' own meshes, welded, with the small pieces gone
    meshes = {}
    for pi, tris in groups.items():
        tt = T[tris]
        V, inv = _weld(P[tt.ravel()])
        F = inv.reshape(-1, 3)
        fp = paint[tt[:, 0]]
        lab = _pieces(V, F)
        big = np.zeros(len(F), dtype=bool)
        sizes = {}
        for L in np.unique(lab):
            m = lab == L
            pts = V[F[m].ravel()]
            sizes[L] = (pts.max(0) - pts.min(0)).max()
            big |= m if sizes[L] >= small else False
        if not big.any():
            best = max(sizes, key=sizes.get)
            big = lab == best
        meshes[pi] = (V, F[big], fp[big])

    # the plain track band from the far mesh, as it is
    FP = np.array(far["points"], dtype=float).reshape(-1, 3)
    FT = np.array(far["triangles"], dtype=np.int64).reshape(-1, 3)
    # (the far mesh numbers its parts without the track links, as the game reads it: put them in the full list's numbers)
    unlinked = np.array([i for i, n in enumerate(names) if not LINK.match(n)])
    fpart = unlinked[np.array(far["part"], dtype=np.int64)]
    coarse = {int(full): k for k, full in enumerate(unlinked)}
    band = {}
    for pi in set(int(x) for x in fpart[FT[:, 0]]):
        if names[pi] in ("track_l", "track_r"):
            band[pi] = FT[fpart[FT[:, 0]] == pi]
    band_tris = sum(len(v) for v in band.values())

    # one shape, thinned once: parts whose meshes are the same about their pivots (to a millimetre and a half)
    def local(pi):
        V, F, _ = meshes[pi]
        return V[np.unique(F.ravel())] - pivots[pi]
    first, twin = {}, {}
    for pi in sorted(meshes):
        V, F, fp = meshes[pi]
        rough = (len(F), tuple(sorted(fp.tolist())))
        for qi in first.get(rough, []):
            a, b = local(pi), local(qi)
            if len(a) != len(b):
                continue
            kd = KDTree(len(b))
            for j, q in enumerate(b):
                kd.insert(q.tolist(), j)
            kd.balance()
            if max(kd.find(q.tolist())[2] for q in a) <= 0.0015:
                twin[pi] = qi
                break
        else:
            first.setdefault(rough, []).append(pi)

    fixed = sum(len(meshes[pi][1]) for pi in meshes if len(meshes[pi][1]) < keep) + band_tris
    free = sum(len(meshes[pi][1]) for pi in meshes if len(meshes[pi][1]) >= keep)
    ratio = min(1.0, max(0.05, (target - fixed) / max(free, 1)))

    out_p, out_n, out_t, out_part, out_paint = [], [], [], [], []
    out_c = {k: [] for k in chans}
    base = 0
    done = {}
    for pi in sorted(meshes):
        if pi in twin:
            V0, F0, fp0, N0 = done[twin[pi]]
            V = V0 - pivots[twin[pi]] + pivots[pi]
            F, fp, N = F0, fp0, N0
        else:
            V, F, fp = meshes[pi]
            if len(F) >= keep and ratio < 1.0:
                V, F, fp = _decimate(V, F, fp, ratio)
            N = _facings(V, F, crease)
            done[pi] = (V, F, fp, N)
        # every other channel from the nearest near point of this part
        own = np.where(part == pi)[0]
        kd = KDTree(len(own))
        for j, q in enumerate(P[own]):
            kd.insert(q.tolist(), j)
        kd.balance()
        nearest = np.array([own[kd.find(q.tolist())[1]] for q in V])
        # corners: (point, facing, paint, channels), the same corner made once
        seen = {}
        tri_idx = []
        for f in range(len(F)):
            row = []
            for k in range(3):
                v = int(F[f, k])
                key = (v, *np.round(N[f, k], 2).tolist(), int(fp[f]))
                if key not in seen:
                    seen[key] = base + len(seen)
                    out_p.append(np.round(V[v], 3))
                    out_n.append(np.round(N[f, k], 2))
                    out_part.append(coarse[pi])
                    out_paint.append(int(fp[f]))
                    for c in chans:
                        out_c[c].append(C[c][nearest[v]])
                row.append(seen[key])
            tri_idx.append(row)
        out_t.extend(tri_idx)
        base += len(seen)
    for pi, tt in sorted(band.items()):
        idx, inv = np.unique(tt.ravel(), return_inverse=True)
        for j in idx:
            out_p.append(FP[j])
            out_n.append(np.array(far["facings"][3 * j:3 * j + 3]))
            out_part.append(coarse[pi])
            out_paint.append(int(far["paints"][j]))
            for c in chans:
                out_c[c].append(far[c][j] if c in far else 0)
        out_t.extend((inv.reshape(-1, 3) + base).tolist())
        base += len(idx)

    # no triangle that names a point twice, has no area, or repeats another (the game's reader and Blender both choke on them)
    TT = np.array(out_t, dtype=np.int64).reshape(-1, 3)
    PP = np.array(out_p, dtype=float).reshape(-1, 3)
    area = np.linalg.norm(np.cross(PP[TT[:, 1]] - PP[TT[:, 0]], PP[TT[:, 2]] - PP[TT[:, 0]]), axis=1)
    ok = (TT[:, 0] != TT[:, 1]) & (TT[:, 1] != TT[:, 2]) & (TT[:, 0] != TT[:, 2]) & (area > 1e-8)
    _, once = np.unique(np.sort(TT, axis=1), axis=0, return_index=True)
    single = np.zeros(len(TT), dtype=bool)
    single[once] = True
    out_t = TT[ok & single]

    def flat(a, digits):
        a = np.asarray(a).ravel()
        return [round(float(x), digits) for x in a] if digits else [int(round(float(x))) for x in a]
    res = {"points": flat(out_p, 3), "facings": flat(out_n, 2), "paints": flat(out_paint, 0), "triangles": flat(out_t, 0), "part": flat(out_part, 0)}
    for c in chans:
        isf = any(isinstance(x, float) and not float(x).is_integer() for x in near[c][:2000])
        res[c] = flat(out_c[c], 2) if isf else flat(out_c[c], 0)
    res["_ratio"] = ratio
    res["_shared"] = len(twin)
    return res
