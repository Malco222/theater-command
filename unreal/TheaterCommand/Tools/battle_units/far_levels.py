"""SO2: two coarser levels for far soldiers, cut down from each file's own far level.

WORK IN PROGRESS (22 Nov, paused for release 2's field clips): far2 is close, but the lump folds the arms into a sheet
and the rifle's stock collapses; the quadric decimator is to be replaced by one that measures the error against the far
surface itself, in a few poses. Nothing has been written to the shared folder; nothing in the game reads it yet.

    far2   about 150 points, for 100 to 200 m (a man is 10 to 20 pixels tall)
    lump   about 60 points, past 200 m (under 10 pixels)

Each is the far level decimated by quadric error: the edge whose loss changes the shape least goes first, a paint's
edge is held like a crease (so the helmet, face, hands, tunic, trousers, boots and rifle keep their colours where
they were), and an edge between two bones costs more (so a knee or an elbow still bends). Pieces of kit too small
to cover a pixel at that range are left out. Every new point reads its arrays back off the far surface it stands on:
its bones and their mix, shade, wear, tint and facing (and a helmet finish's ink), so it skins, shades and wears as
the far man does. The man's own helmet (helmet_tris, the last triangles) and each helmet finish are cut down on their
own, to the same size, so a helmet swap still fits, and each face's tint is carried over onto the new skin points.

They are written as side files next to the soldier files, not into them, because the game wires them only after the
art director has seen the stills:

    battle-models/land/soldier/far_levels/<name>.far_levels.json
        far2, lump                the level arrays, the same shape as the file's far (helmet triangles last)
        helmet_tris               {far2: [first, count], lump: [first, count]}
        faces[k]                  {name, far2: {tint_at, tint}, lump: {...}}   (in the file's face order)
        helmets[k]                {name, far2: {...arrays, ink}, lump: {...}}  (in the file's helmet order)
        made_from                 the file's far level it was cut from (points, triangles, a digest), so a stale
                                  side file shows after a re-export

    python3 far_levels.py [names...]          (default: every crowd soldier file; tankers and gore twins are not)
    python3 far_levels.py --check [names...]  (reads the side files back and checks them against their soldier file)
"""
import hashlib
import heapq
import json
import os
import sys
import time

import numpy as np

SHARED = "/mnt/project-files/battle-models/land/soldier/"
OUT = os.environ.get("FAR_LEVELS_OUT", SHARED + "far_levels/")
CROWD = ["soldier_USA", "soldier_USA_bar", "soldier_USA_nco", "soldier_USA_medic", "soldier_USA_bazooka",
         "soldier_USA_loader", "soldier_USA_radio", "soldier_USA_para", "soldier_GER", "soldier_GER_panzer",
         "soldier_SOV", "soldier_SOV_winter", "soldier_SOV_snow", "soldier_ITA", "soldier_ITA_desert", "soldier_JAP",
         "soldier_CHN", "soldier_POL", "soldier_USA_sailor", "soldier_JAP_sailor", "soldier_UK_sailor",
         "soldier_GER_sailor", "soldier_ITA_sailor", "soldier_SOV_sailor"]
# (level: whole man's points, his helmet's share of them, the smallest piece of kit kept: its longest side times its
# middle one, m2, about a pixel at the level's nearest range on a 1920-pixel 70-degree view)
LEVELS = {"far2": (150, 16, 0.008), "lump": (60, 8, 0.03)}
MIN_ISLAND = 4     # (a piece of kit kept keeps at least this many corners: a tetrahedron, not a flat triangle)
RANGES = {"far2": [100, 200], "lump": [200, None]}
WEAR = ["shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow"]
CREASE = 2.0       # a paint's edge (or an open edge) against moving off it
BONE = 0.5         # an edge between bones, against its length squared
TOL = 1e-4


# ================================================================================================ decimation
def plane_q(n, p, w):
    v = np.array([n[0], n[1], n[2], -float(np.dot(n, p))])
    return w * np.outer(v, v)


class Decimator:
    """Quadric edge collapse on a welded mesh whose faces each carry a key (the paint; a helmet's paint and ink).
    The count it aims at is the json's: a point for each key a vertex's live faces carry."""

    def __init__(self, V, F, key, skin, island=None):
        self.V = np.array(V, float)
        self.F = np.array(F, np.int64)
        self.key = list(key)
        self.alive = np.ones(len(self.F), bool)
        self.S = np.array(skin, float)
        m = len(self.V)
        self.vf = [set() for _ in range(m)]
        for fi, f in enumerate(self.F):
            for a in f:
                self.vf[a].add(fi)
        self.Q = np.zeros((m, 4, 4))
        for fi, (a, b, c) in enumerate(self.F):
            n = np.cross(self.V[b] - self.V[a], self.V[c] - self.V[a])
            ar = np.linalg.norm(n)
            if ar < 1e-12:
                continue
            n /= ar
            # (each face's plane counts once, whatever its size: the error is a distance, so a small piece such as
            # the head or a hand is held as firmly as the body, as a pixel does not care how big the piece is)
            q = plane_q(n, self.V[a], 1.0)
            self.Q[a] += q
            self.Q[b] += q
            self.Q[c] += q
        ef = {}
        for fi, (a, b, c) in enumerate(self.F):
            for x, y in ((a, b), (b, c), (c, a)):
                ef.setdefault((min(x, y), max(x, y)), []).append(fi)
        self.locked = np.zeros(m, bool)
        for (x, y), fs in ef.items():
            if len(fs) > 2:                  # (a seam three faces meet on: left as it is)
                self.locked[[x, y]] = True
            if len(fs) == 1 or (len(fs) == 2 and self.key[fs[0]] != self.key[fs[1]]):
                e = self.V[y] - self.V[x]
                L2 = float(e @ e)
                for fi in fs:
                    a, b, c = self.F[fi]
                    nf = np.cross(self.V[b] - self.V[a], self.V[c] - self.V[a])
                    nn = np.cross(e, nf)
                    ln = np.linalg.norm(nn)
                    if ln < 1e-12:
                        continue
                    q = plane_q(nn / ln, self.V[x], CREASE)
                    self.Q[x] += q
                    self.Q[y] += q
        self.stamp = np.zeros(m, np.int64)
        self.count = sum(len(self.keys_at(v)) for v in range(m))
        self.island = np.zeros(m, np.int64) if island is None else np.array(island)
        self.left = {int(r): int((self.island == r).sum()) for r in np.unique(self.island)}

    def keys_at(self, v):
        return {self.key[f] for f in self.vf[v]}

    def ring(self, v):
        r = set()
        for f in self.vf[v]:
            r.update(int(x) for x in self.F[f])
        r.discard(v)
        return r

    def candidates(self, u, v):
        Q = self.Q[u] + self.Q[v]
        pu, pv = self.V[u], self.V[v]
        out = []
        if not self.locked[u] and not self.locked[v]:
            A = Q[:3, :3]
            if abs(np.linalg.det(A)) > 1e-14:
                p = np.linalg.solve(A, -Q[:3, 3])
                if np.linalg.norm(p - 0.5 * (pu + pv)) <= np.linalg.norm(pu - pv):
                    out.append(p)
            out.append(0.5 * (pu + pv))
        if not self.locked[v]:
            out.append(pu.copy())
        if not self.locked[u]:
            out.append(pv.copy())
        res = []
        for p in out:
            h = np.append(p, 1.0)
            res.append((float(h @ Q @ h), p))
        res.sort(key=lambda t: t[0])
        return res

    def legal(self, u, v, p):
        if self.left[int(self.island[u])] <= MIN_ISLAND:
            return False
        shared = self.vf[u] & self.vf[v]
        opp = set()
        for f in shared:
            opp.update(int(x) for x in self.F[f])
        opp -= {u, v}
        if (self.ring(u) & self.ring(v)) != opp:
            return False
        after = (self.vf[u] | self.vf[v]) - shared
        if not after:
            return False
        for w in opp:                        # (no point left with no face)
            if not (self.vf[w] - shared):
                return False
        seen = set()
        for f in after:
            a, b, c = (u if x == v else int(x) for x in self.F[f])
            tri = frozenset((a, b, c))
            if len(tri) < 3 or tri in seen:
                return False
            seen.add(tri)
            P0 = [p if x == u else self.V[x] for x in (a, b, c)]
            n1 = np.cross(P0[1] - P0[0], P0[2] - P0[0])
            o = self.F[f]
            n0 = np.cross(self.V[o[1]] - self.V[o[0]], self.V[o[2]] - self.V[o[0]])
            l1, l0 = np.linalg.norm(n1), np.linalg.norm(n0)
            if l1 < 1e-9 or l1 < 0.02 * l0:
                return False
            if l0 > 1e-12 and float(n1 @ n0) / (l1 * l0) < 0.2:
                return False
        return True

    def cost(self, u, v):
        L2 = float(np.sum((self.V[u] - self.V[v]) ** 2))
        bone = 0.5 * float(np.abs(self.S[u] - self.S[v]).sum())
        for c, p in self.candidates(u, v):
            if self.legal(u, v, p):
                return c + BONE * bone * L2, p
        return None, None

    def push(self, heap, u, v):
        if u > v:
            u, v = v, u
        c, p = self.cost(u, v)
        if c is not None:
            heapq.heappush(heap, (c, u, v, int(self.stamp[u]), int(self.stamp[v]), tuple(p)))

    def run(self, target):
        heap = []
        done = set()
        for f in self.F:
            for x, y in ((f[0], f[1]), (f[1], f[2]), (f[2], f[0])):
                k = (min(x, y), max(x, y))
                if k not in done:
                    done.add(k)
                    self.push(heap, int(k[0]), int(k[1]))
        while self.count > target and heap:
            c, u, v, su, sv, p = heapq.heappop(heap)
            if su != self.stamp[u] or sv != self.stamp[v] or not self.vf[u] or not self.vf[v]:
                continue
            p = np.array(p)
            if not self.legal(u, v, p):
                continue
            touched = self.ring(u) | self.ring(v) | {u, v}
            before = sum(len(self.keys_at(w)) for w in touched)
            shared = self.vf[u] & self.vf[v]
            for f in shared:
                self.alive[f] = False
                for x in self.F[f]:
                    self.vf[x].discard(f)
            for f in list(self.vf[v]):
                self.F[f] = [u if x == v else x for x in self.F[f]]
                self.vf[u].add(f)
            self.vf[v] = set()
            self.left[int(self.island[u])] -= 1
            for w in (set(int(x) for f in shared for x in self.F[f]) - {u, v}):
                if not self.vf[w]:
                    self.left[int(self.island[w])] -= 1
            self.V[u] = p
            self.Q[u] += self.Q[v]
            self.S[u] = 0.5 * (self.S[u] + self.S[v])
            self.locked[u] = self.locked[u] or self.locked[v]
            self.count += sum(len(self.keys_at(w)) for w in touched) - before
            near = self.ring(u) | {u}
            for w in near:
                self.stamp[w] += 1
            for w in near:
                for z in self.ring(w):
                    self.push(heap, w, z)
        return self.count


# ================================================================================================ the far surface
def closest_on_tris(p, A, B, C):
    """For one point p, the closest point on each triangle (A, B, C: (k, 3)): distances and barycentrics (k, 3)."""
    ab, ac, ap = B - A, C - A, p - A
    d1, d2 = np.einsum("ij,ij->i", ab, ap), np.einsum("ij,ij->i", ac, ap)
    bp = p - B
    d3, d4 = np.einsum("ij,ij->i", ab, bp), np.einsum("ij,ij->i", ac, bp)
    cp = p - C
    d5, d6 = np.einsum("ij,ij->i", ab, cp), np.einsum("ij,ij->i", ac, cp)
    k = len(A)
    bary = np.zeros((k, 3))
    va = d3 * d6 - d5 * d4
    vb = d5 * d2 - d1 * d6
    vc = d1 * d4 - d3 * d2
    den = np.where(np.abs(va + vb + vc) < 1e-18, 1e-18, va + vb + vc)
    v_ = vb / den
    w_ = vc / den
    bary[:] = np.stack([1 - v_ - w_, v_, w_], 1)
    sel = np.zeros(k, bool)

    def put(mask, b):
        nonlocal sel
        m = mask & ~sel
        bary[m] = b[m] if b.ndim == 2 else b
        sel |= m

    put((d1 <= 0) & (d2 <= 0), np.array([1.0, 0, 0]))
    put((d3 >= 0) & (d4 <= d3), np.array([0, 1.0, 0]))
    put((d6 >= 0) & (d5 <= d6), np.array([0, 0, 1.0]))
    with np.errstate(divide="ignore", invalid="ignore"):
        t = np.clip(d1 / np.where(d1 - d3 == 0, 1e-18, d1 - d3), 0, 1)
        put((vc <= 0) & (d1 >= 0) & (d3 <= 0), np.stack([1 - t, t, 0 * t], 1))
        t = np.clip(d2 / np.where(d2 - d6 == 0, 1e-18, d2 - d6), 0, 1)
        put((vb <= 0) & (d2 >= 0) & (d6 <= 0), np.stack([1 - t, 0 * t, t], 1))
        t = np.clip((d4 - d3) / np.where((d4 - d3) + (d5 - d6) == 0, 1e-18, (d4 - d3) + (d5 - d6)), 0, 1)
        put((va <= 0) & (d4 - d3 >= 0) & (d5 - d6 >= 0), np.stack([0 * t, 1 - t, t], 1))
    q = bary[:, :1] * A + bary[:, 1:2] * B + bary[:, 2:] * C
    return np.linalg.norm(q - p, axis=1), bary


class Surface:
    """A level's mesh (or a helmet finish's) as the far man has it: arrays to read back by key at any point."""

    def __init__(self, M, tri_range=None, ink=False):
        self.P = np.array(M["points"], float).reshape(-1, 3)
        self.N = np.array(M["facings"], float).reshape(-1, 3)
        T = np.array(M["triangles"], np.int64).reshape(-1, 3)
        if tri_range is not None:
            T = T[tri_range[0]:tri_range[1]]
        self.T = T
        self.pa = np.array(M["paints"], int)
        self.ink = np.array(M["ink"], int) if ink else None
        self.part = np.array(M["part"], int)
        self.part2 = np.array(M["part2"], int)
        self.w2 = np.array(M["w2"], float)
        self.wear = {k: np.array(M[k], float) for k in WEAR if k in M}
        self.tint = np.array(M.get("tint", [1.0] * (3 * len(self.P))), float).reshape(-1, 3)
        self.tkey = [self.key_of(t) for t in T]

    def key_of(self, t):
        # (a helmet finish's ink is a point's own, blending across a triangle as a colour does, so the key is the
        # paint alone: each new point takes the ink of the far corner it stands nearest)
        ks = [int(self.pa[i]) for i in t]
        return max(set(ks), key=ks.count)

    def read(self, p, key, nb, extra=None):
        idx = [i for i, k in enumerate(self.tkey) if k == key]
        T = self.T[idx]
        d, bary = closest_on_tris(p, self.P[T[:, 0]], self.P[T[:, 1]], self.P[T[:, 2]])
        j = int(np.argmin(d))
        t, b = T[j], bary[j]
        out = {"dist": float(d[j])}
        bw = np.zeros(nb)
        for i, bi in zip(t, b):
            bw[self.part[i]] += bi * (1 - self.w2[i])
            bw[self.part2[i]] += bi * self.w2[i]
        order = np.argsort(-bw)
        a, c = int(order[0]), int(order[1])
        s = bw[a] + bw[c]
        if bw[c] < 1e-4 * max(s, 1e-9):
            out["bones"] = (a, a, 0.0)
        else:
            out["bones"] = (a, c, float(bw[c] / s))
        n = b @ self.N[t]
        out["facing"] = n / max(np.linalg.norm(n), 1e-9)
        for k, arr in self.wear.items():
            out[k] = float(b @ arr[t])
        out["tint"] = b @ self.tint[t]
        if self.ink is not None:
            out["ink"] = int(self.ink[t[int(np.argmax(b))]])
        if extra is not None:
            out["extra"] = b @ extra[t]
        return out


def weld(P):
    key = np.round(P / TOL).astype(np.int64)
    u, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    return P[first], inv.ravel()


def islands(F, m):
    par = list(range(m))

    def f(a):
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a
    for a, b, c in F:
        for x, y in ((a, b), (b, c)):
            rx, ry = f(x), f(y)
            if rx != ry:
                par[rx] = ry
    return np.array([f(a) for a in F[:, 0]])


def cut(S, target, keep_from, nb, log, what):
    """One mesh (a Surface's triangles) cut down to about `target` json points: (V, F, keys)."""
    V, inv = weld(S.P)
    F = inv[S.T]
    keys = list(S.tkey)
    good = (F[:, 0] != F[:, 1]) & (F[:, 1] != F[:, 2]) & (F[:, 0] != F[:, 2])
    F = F[good]
    keys = [k for k, g in zip(keys, good) if g]
    isl = islands(F, len(V))
    keepf = np.ones(len(F), bool)
    dropped = 0
    roots = np.unique(isl)
    big = max(roots, key=lambda r: int((isl == r).sum()))
    for r in roots:
        fs = np.nonzero(isl == r)[0]
        vs = np.unique(F[fs].ravel())
        ext = np.sort(V[vs].max(0) - V[vs].min(0))
        if r != big and ext[2] * ext[1] < keep_from:
            keepf[fs] = False
            dropped += 1
    F = F[keepf]
    keys = [k for k, g in zip(keys, keepf) if g]
    used = np.unique(F.ravel())
    remap = -np.ones(len(V), np.int64)
    remap[used] = np.arange(len(used))
    V = V[used]
    F = remap[F]
    # (each welded vertex's bones, as a weight on every bone: the cost of an edge between bones)
    skin = np.zeros((len(V), nb))
    back = remap[inv]
    for i in range(len(S.P)):
        j = back[i]
        if j >= 0:
            skin[j, S.part[i]] += 1 - S.w2[i]
            skin[j, S.part2[i]] += S.w2[i]
    skin /= np.maximum(skin.sum(1, keepdims=True), 1e-9)
    vis = np.zeros(len(V), np.int64)
    fis = islands(F, len(V))
    for f, r in zip(F, fis):
        vis[f] = r
    D = Decimator(V, F, keys, skin, vis)
    start = D.count
    got = D.run(target)
    Fo = D.F[D.alive]
    ko = [k for k, a in zip(D.key, D.alive) if a]
    log(f"      {what}: {len(np.unique(S.T))} points, {dropped} small pieces left out, {start} -> {got} points, "
        f"{len(Fo)} triangles, {len(np.unique(fis))} pieces")
    return D.V, Fo, ko


def emit(S, V, F, keys, nb, face_tints=None):
    """The json level arrays for a cut mesh: a point per (vertex, key), every array read off the far surface."""
    pts = {}
    order = []
    tris = []
    for f, k in zip(F, keys):
        tri = []
        for v in f:
            kk = (int(v), k)
            if kk not in pts:
                pts[kk] = len(order)
                order.append(kk)
            tri.append(pts[kk])
        tris.append(tri)
    L = {"points": [], "facings": [], "paints": [], "triangles": [int(x) for t in tris for x in t], "part": [],
         "part2": [], "w2": []}
    for k in WEAR:
        if k in S.wear:
            L[k] = []
    L["tint"] = []
    if S.ink is not None:
        L["ink"] = []
    worst = []
    ft = [[] for _ in (face_tints or [])]
    for v, k in order:
        p = V[v]
        r = S.read(p, k, nb)
        worst.append(r["dist"])
        L["points"] += [round(float(x), 4) for x in p]
        L["facings"] += [round(float(x), 3) for x in r["facing"]]
        L["paints"].append(int(k[0]) if isinstance(k, tuple) else int(k))
        a, b, w = r["bones"]
        L["part"].append(a)
        L["part2"].append(b)
        L["w2"].append(round(w, 3))
        for kk in WEAR:
            if kk in S.wear:
                L[kk].append(round(r[kk], 3))
        L["tint"] += [round(float(x), 3) for x in r["tint"]]
        if S.ink is not None:
            L["ink"].append(r["ink"])
        for fi, arr in enumerate(face_tints or []):
            ft[fi].append(S.read(p, k, nb, extra=arr)["extra"])
    return L, np.array(worst), ft


def digest(M):
    h = hashlib.sha1()
    for k in ("points", "paints", "triangles", "part", "part2", "w2"):
        h.update(json.dumps(M[k]).encode())
    return h.hexdigest()[:16]


def make(name, log=print):
    D = json.load(open(SHARED + name + ".json"))
    nb = len(D["bones"])
    F = D["far"]
    T = np.array(F["triangles"]).reshape(-1, 3)
    hf, hc = D["helmet_tris"]["far"]
    n = len(F["paints"])
    # (each face's tint on every far point: 1 where it gives none)
    face_tints = []
    for fc in D.get("faces", []):
        arr = np.ones((n, 3))
        fa = fc.get("far", {})
        if fa.get("tint_at"):
            arr[np.array(fa["tint_at"])] = np.array(fa["tint"]).reshape(-1, 3)
        face_tints.append(arr)
    out = {"about": ("SO2 coarse levels for far men, cut from this file's far level by far_levels.py: far2 for "
                     f"{RANGES['far2'][0]} to {RANGES['far2'][1]} m, lump past {RANGES['lump'][0]} m. Same arrays as "
                     "the far level (helmet triangles last); faces[k] and helmets[k] in the file's order."),
           "made_from": {"file": name + ".json", "far_points": n, "far_triangles": len(T), "far_digest": digest(F)},
           "ranges": RANGES, "helmet_tris": {}, "faces": [{"name": fc.get("name")} for fc in D.get("faces", [])],
           "helmets": [{"name": h.get("name")} for h in D.get("helmets", [])]}
    t0 = time.time()
    for lev, (tot, helm, keep_from) in LEVELS.items():
        log(f"    {lev}: about {tot} points, helmet {helm}, kit kept from {keep_from} m")
        body = Surface(F, (0, hf))
        own = Surface(F, (hf, hf + hc))
        Vb, Fb, kb = cut(body, tot - helm, keep_from, nb, log, "body")
        Vh, Fh, kh = cut(own, helm, keep_from, nb, log, "own helmet")
        Lb, wb, ftb = emit(body, Vb, Fb, kb, nb, face_tints)
        Lh, wh, fth = emit(own, Vh, Fh, kh, nb, face_tints)
        nbp = len(Lb["paints"])
        L = {}
        for k in Lb:
            if k == "triangles":
                L[k] = Lb[k] + [x + nbp for x in Lh[k]]
            else:
                L[k] = Lb[k] + Lh[k]
        out[lev] = L
        out["helmet_tris"][lev] = [len(Lb["triangles"]) // 3, len(Lh["triangles"]) // 3]
        dd = np.concatenate([wb, wh])
        log(f"      {lev}: {len(L['paints'])} points, {len(L['triangles']) // 3} triangles "
            f"(far {n} / {len(T)}); off the far surface: median {np.median(dd) * 100:.2f} cm, "
            f"95% {np.percentile(dd, 95) * 100:.2f} cm, most {dd.max() * 100:.2f} cm")
        for fi, fc in enumerate(D.get("faces", [])):
            arr = np.array(ftb[fi] + fth[fi]) if face_tints else np.zeros((0, 3))
            at = [i for i in range(len(arr)) if np.abs(arr[i] - 1).max() > 0.03]
            out["faces"][fi][lev] = {"tint_at": at, "tint": [round(float(x), 3) for i in at for x in arr[i]]}
        for hi, h in enumerate(D.get("helmets", [])):
            HS = Surface(h["far"], ink=True)
            Vk, Fk, kk = cut(HS, helm, keep_from, nb, log, f"helmet {h.get('name')}")
            Lk, wk, _ = emit(HS, Vk, Fk, kk, nb)
            out["helmets"][hi][lev] = Lk
    log(f"    {name}: {time.time() - t0:.0f} s")
    return out


def write(name, out):
    os.makedirs(OUT, exist_ok=True)
    path = OUT + name + ".far_levels.json"
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(out, f, separators=(",", ":"))
    os.replace(tmp, path)
    return path


def check(name, log=print):
    D = json.load(open(SHARED + name + ".json"))
    X = json.load(open(OUT + name + ".far_levels.json"))
    nb = len(D["bones"])
    bad = []
    if X["made_from"]["far_digest"] != digest(D["far"]):
        bad.append("stale: the soldier file's far level changed since")
    for lev in LEVELS:
        L = X[lev]
        n = len(L["paints"])
        T = np.array(L["triangles"]).reshape(-1, 3)
        for k in ("points", "facings", "tint"):
            if len(L[k]) != 3 * n:
                bad.append(f"{lev} {k} length")
        for k in ["part", "part2", "w2"] + [w for w in WEAR if w in D["far"]]:
            if len(L.get(k, [])) != n:
                bad.append(f"{lev} {k} length")
        if T.min() < 0 or T.max() >= n:
            bad.append(f"{lev} triangle index")
        if max(L["part"] + L["part2"]) >= nb:
            bad.append(f"{lev} bone")
        f0, c0 = X["helmet_tris"][lev]
        if f0 + c0 != len(T):
            bad.append(f"{lev} helmet_tris not last")
        P = np.array(L["points"]).reshape(-1, 3)
        ar = np.linalg.norm(np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]]), axis=1)
        if (ar < 1e-10).any():
            bad.append(f"{lev} {int((ar < 1e-10).sum())} flat triangles")
        for h in X["helmets"]:
            Lh = h[lev]
            if len(Lh["ink"]) != len(Lh["paints"]):
                bad.append(f"{lev} helmet {h['name']} ink")
        log(f"    {name} {lev}: {n} points, {len(T)} triangles, helmet {c0}")
    if len(X["faces"]) != len(D.get("faces", [])) or len(X["helmets"]) != len(D.get("helmets", [])):
        bad.append("faces or helmets count")
    return bad


def main(args):
    chk = "--check" in args
    names = [a for a in args if not a.startswith("--")] or [c for c in CROWD if os.path.exists(SHARED + c + ".json")]
    allbad = {}
    for nm in names:
        if chk:
            b = check(nm)
            if b:
                allbad[nm] = b
            continue
        print(nm)
        out = make(nm)
        print("    wrote", write(nm, out))
    if chk:
        print("all good" if not allbad else json.dumps(allbad, indent=1))


if __name__ == "__main__":
    main(sys.argv[1:])
