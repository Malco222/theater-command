"""Gore twins: a delivered soldier json made into its twin whose limbs can come off (X.json -> X_gore.json).

    python gore.py soldier_USA [soldier_GER ...]        (numpy only: reads X.json from the shared soldier folder,
                                                         writes X_gore.json to the stage, /home/user/art/gore/out/)
    python gore.py --deliver soldier_USA                (copies a staged twin into the shared folder, new files only)
    /home/user/bpyenv/bin/python gore_check.py soldier_USA   (the checks and the picture: Blender as a module)

Nothing of the man is rebuilt: the delivered json is post-processed, every array keeping its order.

THE TWIN. Bones, sockets, faces, helmets, inks, helmet_tris and marks as X.json's (same bones, same order, same rest), so
every clip of X plays on it. Eight cuts, the ragdoll's own joints: upperarm_l/_r (the arm off at the shoulder), lowerarm_l/
_r (at the elbow), thigh_l/_r (the whole leg at the hip), calf_l/_r (at the knee). The man is cut into nine regions (the
body, and each cut bone's own stretch of limb). Every point whose two bones lie in two regions (the band at a joint's
crease, where the weights cross) goes 100% to one region (w2 = 0): the region its triangles were given (by the summed
weights of their points, small islands smoothed away; between the two legs the crotch goes to the pelvis); rigid things
(straps, leggings, badges, pouches: any paint but cloth and skin) go whole to one side. Where a triangle of one region meets
a triangle of another the point is duplicated (the copy rides the other side), so no triangle spans a cut. Each open side
is closed by a cap recessed into its own side: a ragged ring of torn cloth (the uniform's own paint), a soaked edge
(`blood`), raw flesh (`wound`) and a short rough bone end (`bone`). Behind the seam each side also carries a hidden
lining: a copy of the other side's cloth for a few centimetres past the seam, a few millimetres inside it, riding its own
side; when a joined joint bends it closes the crack the hard weights open, and when the limb is off it is the torn sleeve
or trouser leg round the wound.

ORDER. near, mid and far keep their original points first, in order, with their values (part/part2/w2 changed only at the
band), and their original triangles first (re-pointed only where a seam copy replaces a point); the seam copies, then the
caps and linings cut by cut, are appended. So faces, helmets, inks and helmet_tris of X.json apply by index.
"""
import hashlib
import json
import math
import os
import shutil
import sys
import time

import numpy as np

SHARED = "/mnt/project-files/battle-models/land/soldier/"
STAGE = "/home/user/art/gore/out/"
CUTS = ["upperarm_l", "lowerarm_l", "upperarm_r", "lowerarm_r", "thigh_l", "calf_l", "thigh_r", "calf_r"]
NEXT = {"upperarm_l": "lowerarm_l", "lowerarm_l": "hand_l", "upperarm_r": "lowerarm_r", "lowerarm_r": "hand_r",
        "thigh_l": "calf_l", "calf_l": "foot_l", "thigh_r": "calf_r", "calf_r": "foot_r"}
WOUND, BONE, BLOOD = 35, 36, 37
GORE_PAINTS = {"wound": WOUND, "bone": BONE, "blood": BLOOD}
GORE_COLOURS = {"wound": "6A1A16", "bone": "C2B89C", "blood": "2B1813"}
CLOTH = {0, 1, 7, 10, 11}          # tunic, trousers, skin, coat, coat_dark: split at the seam; any other paint is rigid kit
BONE_R = {"upperarm": 0.0115, "lowerarm": 0.0095, "thigh": 0.0135, "calf": 0.0125}     # the bone end's radius (m)
LOD_DETAIL = {"near": 3, "mid": 2, "far": 1}
LININGS = ()   # sides given a hidden cloth lining past the seam: tried (it flaps out of bent joints); off, see `joined`
PROTRUDE = []
LINING_POSES = 0.9    # ... in this share of the clips' poses (the rest are the deepest bends, where the seam opens)


# ================================================================================================ the rig
class Rig:
    def __init__(self, bones):
        self.bones = bones
        self.names = [b["name"] for b in bones]
        self.parent = [b["parent"] for b in bones]
        self.head = np.array([b["head"] for b in bones], float)
        self.index = {n: i for i, n in enumerate(self.names)}
        nb = len(bones)
        # region of each bone: 0 the body, k+1 the innermost cut k whose bone is it or an ancestor
        self.region = np.zeros(nb, int)
        for i in range(nb):
            j = i
            while j is not None:
                if self.names[j] in CUTS:
                    self.region[i] = CUTS.index(self.names[j]) + 1
                    break
                j = self.parent[j]
        self.rparent = [None] + [int(self.region[self.parent[self.index[c]]]) for c in CUTS]
        self.rbone = [None] + [self.index[c] for c in CUTS]          # each cut region's own bone

    def ancestors(self, i):
        out = []
        while i is not None:
            out.append(i)
            i = self.parent[i]
        return out

    def lca(self, a, b):
        A = self.ancestors(a)
        for j in self.ancestors(b):
            if j in A:
                return j
        return 0

    def rdepth(self, r):
        d = 0
        while r:
            r = self.rparent[r]
            d += 1
        return d

    def rlca(self, a, b):
        A = []
        r = a
        while r is not None:
            A.append(r)
            r = self.rparent[r] if r else None
        r = b
        while r is not None:
            if r in A:
                return r
            r = self.rparent[r] if r else None
        return 0

    def cut_mask(self, regions):
        """The cuts between a set of regions (bit k for CUTS[k]): every region on the way up from each to their
        common one, that one left out."""
        regions = [int(r) for r in regions]
        top = regions[0]
        for r in regions[1:]:
            top = self.rlca(top, r)
        m = 0
        for r in regions:
            while r and r != top:
                m |= 1 << (r - 1)
                r = self.rparent[r]
        return m

    def related(self, a, b):
        """Regions a and b are one, or parent and child."""
        return a == b or self.rparent[a] == b or self.rparent[b] == a

    def bone_in(self, bone, r):
        """The bone a point on `bone` rides once it is put in region r: itself if it is in r, else up the tree to r's
        first bone (the body: the first body bone above it), else r's own cut bone."""
        for j in self.ancestors(bone):
            if self.region[j] == r:
                return j
        return self.rbone[r] if r else 0


# ================================================================================================ helpers
def weld(P):
    key = np.round(np.asarray(P) * 1e4).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    return inv.ravel()


class DSU:
    def __init__(self, n):
        self.p = np.arange(n)

    def find(self, a):
        p = self.p
        r = a
        while p[r] != r:
            r = p[r]
        while p[a] != r:
            p[a], a = r, p[a]
        return r

    def union(self, a, b):
        a, b = self.find(a), self.find(b)
        if a != b:
            self.p[b] = a


def hash01(*k):
    h = hashlib.md5(repr(k).encode()).digest()
    return int.from_bytes(h[:4], "little") / 2 ** 32


def tri_normals(P, T):
    n = -np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])      # (the json's winding: clockwise seen from outside)
    l = np.linalg.norm(n, axis=1, keepdims=True)
    return n / np.maximum(l, 1e-12), l.ravel() * 0.5


def point_bone(rig, a, b, w, r):
    """The bones of a point (on a, and by w on b) once it is put in region r: as it was if both its bones are in r; its
    one bone in r, alone; else (between the legs) their common bone, or its main bone's first ancestor in r, or r's own
    cut bone."""
    R = rig.region
    bones = [(a, 1 - w)] + ([(b, w)] if w > 0 and b != a else [])
    inr = [x for x, _ in bones if R[x] == r]
    if len(inr) == len(bones):
        return (a, b, w)
    if len(inr) == 1:
        return (inr[0], inr[0], 0.0)
    if len(bones) == 2 and not rig.related(R[a], R[b]) and rig.rlca(R[a], R[b]) == r:
        x = rig.lca(a, b)
        return (x, x, 0.0)
    main = max(bones, key=lambda xw: xw[1])[0]
    x = rig.bone_in(main, r)
    return (x, x, 0.0)


# ================================================================================================ posing (the json's own skinning)
def qmat(q):
    w, x, y, z = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
                     [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
                     [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]])


def axis_angle(axis, deg):
    a = np.asarray(axis, float) / np.linalg.norm(axis)
    h = math.radians(deg) / 2
    return qmat((math.cos(h), *(a * math.sin(h))))


def _samp(keys, t, rot):
    ts = [k[0] for k in keys]
    if t <= ts[0]:
        v = np.array(keys[0][1:], float)
    elif t >= ts[-1]:
        v = np.array(keys[-1][1:], float)
    else:
        i = int(np.searchsorted(ts, t, side="right")) - 1
        a, b = np.array(keys[i][1:], float), np.array(keys[i + 1][1:], float)
        u = (t - ts[i]) / (ts[i + 1] - ts[i])
        if rot and a @ b < 0:
            b = -b
        v = a * (1 - u) + b * u
    return v / np.linalg.norm(v) if rot else v


def pose(rig, clip=None, t=0.0, extra=None):
    """Each bone's turn G and head X at time t of a clip (the json's formula: G = G_parent q, X = X_parent +
    G_parent (head - head_parent + move)). `extra` {bone: (R or None, move)} adds a turn and a move on top."""
    nb = len(rig.bones)
    H = rig.head
    q = [np.eye(3) for _ in range(nb)]
    mv = [np.zeros(3) for _ in range(nb)]
    if clip is not None:
        for tr in clip["tracks"]:
            i = tr["part"]
            if "rot" in tr:
                q[i] = qmat(_samp(tr["rot"], t, True))
            if "move" in tr:
                mv[i] = _samp(tr["move"], t, False)
    for i, (Rx, m_) in (extra or {}).items():
        if Rx is not None:
            q[i] = q[i] @ Rx
        mv[i] = mv[i] + np.asarray(m_, float)
    G = np.zeros((nb, 3, 3))
    X = np.zeros((nb, 3))
    for i in range(nb):
        p = rig.parent[i]
        if p is None:
            G[i] = q[i]
            X[i] = H[i] + mv[i]
        else:
            G[i] = G[p] @ q[i]
            X[i] = X[p] + G[p] @ (H[i] - H[p] + mv[i])
    return G, X


def skin(P, a, b, w, G, X, H):
    A = X[a] + np.einsum("nij,nj->ni", G[a], P - H[a])
    B = X[b] + np.einsum("nij,nj->ni", G[b], P - H[b])
    w = np.asarray(w, float)[:, None]
    return A * (1 - w) + B * w


def clip_times(clip, step=None):
    """The clip's own key times (every key of every track), or every `step` s."""
    L = clip["long"]
    if step:
        k = max(1, int(round(L / step)))
        return [L * i / k for i in range(k + 1)]
    ts = sorted({round(k[0], 6) for tr in clip["tracks"] for key in ("rot", "move") for k in tr.get(key, [])})
    return ts or [0.0]


def pose_bank(rig, clips, per_clip=6):
    Gs, Xs = [], []
    for name, c in clips.items():
        for t in clip_times(c, c["long"] / (per_clip - 1)):
            G, X = pose(rig, c, t)
            Gs.append(G)
            Xs.append(X)
    return np.array(Gs), np.array(Xs)


def bank_positions(P, a, b, w, bank, H):
    """Points (k,3) with bones a, b and weight w over every pose of the bank -> (poses, k, 3)."""
    G, X = bank
    A = X[:, a] + np.einsum("snij,nj->sni", G[:, a], P - H[a])
    B = X[:, b] + np.einsum("snij,nj->sni", G[:, b], P - H[b])
    w = np.asarray(w, float)[None, :, None]
    return A * (1 - w) + B * w


# ================================================================================================ the split
def split_lod(M, rig, lod, bank):
    """One level of detail: the hardened band and the seam copies. Returns the new arrays and what the caps need."""
    P = np.array(M["points"], float).reshape(-1, 3)
    F = np.array(M["facings"], float).reshape(-1, 3)
    paint = np.array(M["paints"], int)
    part = np.array(M["part"], int)
    part2 = np.array(M["part2"], int)
    w2 = np.array(M["w2"], float)
    T = np.array(M["triangles"], int).reshape(-1, 3)
    n, m = len(P), len(T)
    R = rig.region
    H = rig.head
    V = weld(P)
    nv = V.max() + 1
    VP = np.zeros((nv, 3))
    VP[V] = P
    TV = V[T]
    band = (R[part] != R[part2]) & (w2 > 0)
    # welded edges -> triangles, neighbours, components, paint patches
    edge_tris = {}
    for t, (a_, b_, c_) in enumerate(TV):
        for u, v in ((a_, b_), (b_, c_), (c_, a_)):
            edge_tris.setdefault((min(u, v), max(u, v)), []).append(t)
    tnb = [[] for _ in range(m)]
    tedge = []
    for e, ts in edge_tris.items():
        if len(ts) == 2:
            tnb[ts[0]].append(ts[1])
            tnb[ts[1]].append(ts[0])
            tedge.append((ts[0], ts[1], e))
    dsu = DSU(nv)
    for a_, b_, c_ in TV:
        dsu.union(a_, b_)
        dsu.union(a_, c_)
    comp = np.array([dsu.find(v) for v in TV[:, 0]])
    tpaint = paint[T[:, 0]]
    pd = DSU(m)
    for t in range(m):
        for u in tnb[t]:
            if tpaint[u] == tpaint[t]:
                pd.union(t, u)
    patch = np.array([pd.find(t) for t in range(m)])
    rigid = ~np.isin(tpaint, list(CLOTH))
    area = tri_normals(P, T)[1]
    # every triangle starts in the region of its points' first bone where they agree, else the body
    treg = np.zeros(m, int)
    for t in range(m):
        rs = {int(R[part[i]]) for i in T[t]} | {int(R[part2[i]]) for i in T[t] if w2[i] > 0}
        treg[t] = rs.pop() if len(rs) == 1 else 0
    # then cut by cut, outer cuts first (each arm and leg off its body, then the forearm and the shin off them): a
    # minimum cut over the triangles, each side of the cut costing how far its points stray from where his own weights
    # carry them over his clips (area x distance), the seam costing how wide it opens there (length x crack x 5 cm, x2)
    G_, X_ = bank
    openv = {}
    for e, ts in edge_tris.items():
        if len(ts) == 1:
            for v in e:
                openv.setdefault(v, set())
    for t in range(m):
        for v in TV[t]:
            if v in openv:
                openv[v].add(t)
    openv = {v: sorted(ts) for v, ts in openv.items()}
    order = sorted(range(len(CUTS)), key=lambda k: rig.rdepth(k + 1))
    cut_report = {}
    for k in order:
        c = k + 1
        cut = CUTS[k]
        C = rig.index[cut]
        Pb = rig.parent[C]
        sub = {r for r in range(len(CUTS) + 1) if r and (r == c or rig.rdepth(r) > rig.rdepth(c) and
                                                       rig.rlca(r, c) == c)}
        S_in = np.array([R[b_] in sub for b_ in range(len(R))])
        p_reg = rig.rparent[c]
        if p_reg == 0:
            S_out = ~S_in
            active_region = 0
        else:
            S_out = np.array([R[b_] == p_reg for b_ in range(len(R))])
            active_region = p_reg
        touch = S_in[part] | (S_in[part2] & (w2 > 0))
        comps = set(comp[np.flatnonzero(touch[T].any(1))])
        act = np.array([comp[t] in comps and (treg[t] == p_reg or treg[t] in sub) for t in range(m)])
        # (for a leg, the other leg's triangles stay out; so do the body's for a forearm or shin)
        ai = np.flatnonzero(act)
        if len(ai) == 0:
            continue
        pts = np.unique(T[ai])
        # each point's place riding in or out
        def riding(S, fallback):
            a1, b1, w1 = part[pts].copy(), part2[pts].copy(), w2[pts].copy()
            ina, inb = S[a1], S[b1] & (w1 > 0)
            both = ina & (inb | (w1 <= 0))
            only_a = ina & ~both
            only_b = ~ina & inb
            none = ~ina & ~inb
            a1[only_a] = a1[only_a]; b1[only_a] = a1[only_a]; w1[only_a] = 0
            a1[only_b] = b1[only_b]; w1[only_b] = 0
            b1[only_b] = a1[only_b]
            a1[none] = fallback; b1[none] = fallback; w1[none] = 0
            return a1, b1, w1
        O = bank_positions(P[pts], part[pts], part2[pts], w2[pts], bank, H)
        ia, ib, iw = riding(S_in, C)
        oa, ob, ow = riding(S_out, Pb)
        Pin = bank_positions(P[pts], ia, ib, iw, bank, H)
        Pout = bank_positions(P[pts], oa, ob, ow, bank, H)
        def score(d):
            return d.mean(0) + 0.25 * d.max(0)
        s_in = score(np.linalg.norm(Pin - O, axis=2))
        s_out = score(np.linalg.norm(Pout - O, axis=2))
        crk = score(np.linalg.norm(Pin - Pout, axis=2))
        loc = {int(p): j for j, p in enumerate(pts)}
        vcrack = {}
        for p, j in loc.items():
            vcrack[V[p]] = max(vcrack.get(V[p], 0.0), crk[j])
        node = {int(t): j for j, t in enumerate(ai)}
        N = len(ai)
        src, snk = N, N + 1
        SC = 1e8
        BIG = int(1e11)
        rows, cols, caps = [], [], []
        forced_out = 0
        for t in ai:
            j = node[int(t)]
            u_in = area[t] * np.mean([s_in[loc[int(i)]] for i in T[t]])
            u_out = area[t] * np.mean([s_out[loc[int(i)]] for i in T[t]])
            # never next to a region it may not touch (the other leg; the body, for a forearm or shin)
            if any((not act[u]) and not rig.related(int(treg[u]), c) for u in tnb[t]):
                u_in = 1e3
                forced_out += 1
            # (the cut stays in the band: a triangle all of whose points ride only the limb's bones stays on the limb,
            # one none of whose points does stays off it)
            tin = [bool(S_in[part[i]] and (w2[i] <= 0 or S_in[part2[i]])) for i in T[t]]
            tany = [bool(S_in[part[i]] or (w2[i] > 0 and S_in[part2[i]])) for i in T[t]]
            if all(tin) and not rigid[t]:
                u_out = 1e3
            elif not any(tany):
                u_in = max(u_in, 1e3)
            rows += [src, j]
            cols += [j, snk]
            caps += [min(BIG, int(u_out * SC) + 1), min(BIG, int(u_in * SC) + 1)]
        for t, u, e in tedge:
            if act[t] and act[u]:
                l = float(np.linalg.norm(VP[e[0]] - VP[e[1]]))
                w = 2.0 * l * 0.05 * 0.5 * (vcrack.get(e[0], 0) + vcrack.get(e[1], 0))
                if rigid[t] and rigid[u] and patch[t] == patch[u]:
                    w = 1e3
                cw = min(BIG, int(w * SC) + 1)
                rows += [node[t], node[u]]
                cols += [node[u], node[t]]
                caps += [cw, cw]
        # an open edge of the mesh (a cuff, a collar) is never crossed by the cut: the triangles round it go together
        for v, ts in openv.items():
            ts = [node[t] for t in ts if act[t]]
            for x, y in zip(ts[:-1], ts[1:]):
                rows += [x, y]
                cols += [y, x]
                caps += [BIG, BIG]
        # rigid kit in its own piece (no neighbours across): tie its triangles
        for pt in np.unique(patch[ai][rigid[ai]]):
            ts = [node[int(t)] for t in ai if patch[t] == pt]
            for x, y in zip(ts[:-1], ts[1:]):
                rows += [x, y]
                cols += [y, x]
                caps += [BIG, BIG]
        from scipy.sparse import csr_matrix
        from scipy.sparse.csgraph import maximum_flow
        Cm = csr_matrix((np.array(caps, np.int64), (np.array(rows), np.array(cols))), shape=(N + 2, N + 2))
        Cm.sum_duplicates()
        fl = maximum_flow(Cm, src, snk, method="dinic").flow
        Rm = (Cm - fl).tocsr()
        Rm.eliminate_zeros()
        seen = np.zeros(N + 2, bool)
        stack = [src]
        seen[src] = True
        while stack:
            x = stack.pop()
            st, en = Rm.indptr[x], Rm.indptr[x + 1]
            for y, cap in zip(Rm.indices[st:en], Rm.data[st:en]):
                if cap > 0 and not seen[y]:
                    seen[y] = True
                    stack.append(y)
        lab_in = seen[:N]
        keep = np.array([treg[t] in sub for t in ai])
        treg[ai[lab_in & ~keep]] = c
        treg[ai[~lab_in]] = p_reg
        cut_report[cut] = {"active": int(N), "in": int(lab_in.sum()), "forced out": forced_out}
    # the deeper regions take the triangles their own bones carry (the hand, the foot) as they started
    # points: the region(s) of their triangles; the original keeps the one it strays least in, a copy for each other
    def pcost(i, r):
        b_ = point_bone(rig, part[i], part2[i], w2[i], r)
        if b_ == (part[i], part2[i], w2[i]):
            return 0.0
        return 1.0 + rig.rdepth(r) * 0.01 - (rw_[i, r] if r < rw_.shape[1] else 0)
    rw_ = np.zeros((n, len(CUTS) + 1))
    np.add.at(rw_, (np.arange(n), R[part]), 1 - w2)
    np.add.at(rw_, (np.arange(n), R[part2]), w2)
    rigid_moved = 0
    unrelated = sum(1 for t in range(m) for u in tnb[t] if not rig.related(treg[t], treg[u]))
    # points: the region(s) of their triangles; the original keeps the cheapest, a copy for each other
    preg = [set() for _ in range(n)]
    for t in range(m):
        for i in T[t]:
            preg[i].add(int(treg[t]))
    npart, npart2, nw2 = part.copy(), part2.copy(), w2.copy()
    dup, new_src, new_bone = {}, [], []
    hardened = 0
    for i in range(n):
        rs = preg[i]
        own = {int(R[part[i]])} | ({int(R[part2[i]])} if w2[i] > 0 else set())
        if not rs or (len(own) == 1 and rs == own):
            continue
        r0 = min(rs, key=lambda r: (pcost(i, r), rig.rdepth(r)))
        for r in sorted(rs):
            b = point_bone(rig, part[i], part2[i], w2[i], r)
            if r == r0:
                if b != (part[i], part2[i], w2[i]):
                    hardened += 1
                npart[i], npart2[i], nw2[i] = b
            else:
                dup[(i, r)] = n + len(new_src)
                new_src.append(i)
                new_bone.append(b)
    T2 = T.copy()
    for t in range(m):
        r = int(treg[t])
        for k in range(3):
            j = dup.get((T[t, k], r))
            if j is not None:
                T2[t, k] = j
    src = np.array(new_src, int).reshape(-1)
    arr = {
        "P": np.concatenate([P, P[src]]), "F": np.concatenate([F, F[src]]),
        "paint": np.concatenate([paint, paint[src]]),
        "part": np.concatenate([npart, np.array([b[0] for b in new_bone], int)]),
        "part2": np.concatenate([npart2, np.array([b[1] for b in new_bone], int)]),
        "w2": np.concatenate([nw2, np.array([b[2] for b in new_bone], float)]),
        "src": np.concatenate([np.arange(n), src]),
    }
    if "shade" in M:
        sh = np.array(M["shade"], float)
        arr["shade"] = np.concatenate([sh, sh[src]])
    info = {"n": n, "m": m, "band_points": int(band.sum()), "hardened": hardened, "seam_copies": len(src),
            "unrelated_neighbours": int(unrelated), "rigid_pieces_moved_whole": rigid_moved,
            "treg": treg, "cut_report": cut_report, "T2": T2, "V": V, "VP": VP, "TV": TV, "comp": comp, "rigid": rigid}
    info["loops"] = seam_loops(TV, treg, rig, VP)
    return arr, info


def seam_loops(TV, treg, rig, VP=None):
    """For each cut: the loops (welded vertex lists) where its region meets its parent region, in the order the limb
    side's boundary runs (u -> v along a triangle of the limb). An edge counts where a triangle of the parent region
    lies across it and none of the limb's (so the doubled or flipped triangles of a thinned level do not break a loop);
    pieces of a broken loop are joined end to start, and a piece whose ends lie within 6 cm is closed."""
    dtri = {}
    for t, (a, b, c) in enumerate(TV):
        for u, v in ((a, b), (b, c), (c, a)):
            dtri.setdefault((u, v), []).append(t)
    res = {}
    for k, cut in enumerate(CUTS):
        c = k + 1
        pr = rig.rparent[c]
        half = {}
        for t in np.flatnonzero(treg == c):
            a, b, cc = TV[t]
            for u, v in ((a, b), (b, cc), (cc, a)):
                across = [x for x in dtri.get((v, u), []) + dtri.get((u, v), []) if x != t]
                if any(treg[x] == c for x in across):
                    continue
                if any(treg[x] == pr for x in across):
                    half.setdefault(u, []).append((v, t))
        used, loops = set(), []
        for u0 in sorted(half):
            for (v0, t0) in half[u0]:
                if (u0, v0) in used:
                    continue
                loop, closed = [u0], False
                u, v, t = u0, v0, t0
                for _ in range(100000):
                    used.add((u, v))
                    if v == u0:
                        closed = True
                        break
                    loop.append(v)
                    cand = [(x, tt) for (x, tt) in half.get(v, []) if (v, x) not in used]
                    if not cand:
                        break
                    pick = cand[0]
                    if len(cand) > 1:          # a pinch: walk the fan of the limb's triangles round v from t
                        tt = t
                        for _ in range(64):
                            tri = list(TV[tt])
                            w = tri[(tri.index(v) + 1) % 3]
                            hit = [ct for ct in cand if ct[0] == w]
                            if hit:
                                pick = hit[0]
                                break
                            tw = [x for x in dtri.get((w, v), []) if treg[x] == c]
                            if not tw:
                                break
                            tt = tw[0]
                    u, v, t = v, pick[0], pick[1]
                loops.append({"verts": loop, "closed": closed and len(loop) >= 3})
        # join open pieces end to start (a piece walked from its middle leaves its head behind)
        opened = [lp["verts"] for lp in loops if not lp["closed"]]
        done = [lp for lp in loops if lp["closed"]]
        changed = True
        while changed and len(opened) > 1:
            changed = False
            for i in range(len(opened)):
                for j in range(len(opened)):
                    if i != j and opened[i][-1] == opened[j][0]:
                        opened[i] = opened[i] + opened[j][1:]
                        opened.pop(j)
                        changed = True
                        break
                if changed:
                    break
        if len(opened) > 1 or (opened and not done):
            # (pieces whose triangles run both ways round a thinned level: join them as an undirected ring if they are one)
            und = {}
            for vs in opened:
                for x, y in zip(vs[:-1], vs[1:]):
                    und.setdefault(x, set()).add(y)
                    und.setdefault(y, set()).add(x)
            if und and all(len(n) == 2 for n in und.values()):
                start = min(und)
                ring, prev = [start], None
                cur = start
                while True:
                    nxt = [n for n in sorted(und[cur]) if n != prev]
                    if not nxt or nxt[0] == start:
                        break
                    prev, cur = cur, nxt[0]
                    ring.append(cur)
                if len(ring) == len(und) and len(ring) >= 3:
                    fwd = sum(1 for vs in opened for x, y in zip(vs[:-1], vs[1:])
                              if (ring.index(y) - ring.index(x)) % len(ring) == 1)
                    if fwd * 2 < sum(len(vs) - 1 for vs in opened):
                        ring = ring[::-1]
                    opened = []
                    done.append({"verts": ring, "closed": True, "joined": True, "unordered": True})
        for vs in opened:
            if len(vs) >= 4 and vs[-1] == vs[0]:
                done.append({"verts": vs[:-1], "closed": True, "joined": True})
            elif len(vs) >= 3 and VP is not None and np.linalg.norm(VP[vs[-1]] - VP[vs[0]]) < 0.06:
                done.append({"verts": vs, "closed": True, "joined": True, "bridged": True})
            else:
                done.append({"verts": vs, "closed": False})
        res[cut] = done
    return res


# ================================================================================================ caps and linings
class Builder:
    """Appends points and triangles (the json's winding) to a mesh's arrays."""

    def __init__(self, arr, T2):
        self.P = [p for p in arr["P"]]
        self.F = [f for f in arr["F"]]
        self.paint = list(arr["paint"])
        self.part = list(arr["part"])
        self.part2 = list(arr["part2"])
        self.w2 = list(arr["w2"])
        self.shade = list(arr["shade"]) if "shade" in arr else None
        self.T = [tuple(t) for t in T2]

    def point(self, p, paint, bone, facing=None):
        self.P.append(np.asarray(p, float))
        self.F.append(np.zeros(3) if facing is None else np.asarray(facing, float))
        self.paint.append(int(paint))
        self.part.append(int(bone[0]))
        self.part2.append(int(bone[1]))
        self.w2.append(float(bone[2]))
        if self.shade is not None:
            self.shade.append(1.0)
        return len(self.P) - 1

    def tri(self, a, b, c, out):
        """A triangle facing `out` (a direction), in the json's winding."""
        pa, pb, pc = self.P[a], self.P[b], self.P[c]
        nrm = -np.cross(pb - pa, pc - pa)
        if nrm @ out < 0:
            b, c = c, b
        self.T.append((a, b, c))

    def smooth_facings(self, p0, t0):
        """Facings of the points from p0 on: the mean of their triangles' (from t0 on)."""
        acc = {}
        for (a, b, c) in self.T[t0:]:
            pa, pb, pc = self.P[a], self.P[b], self.P[c]
            nrm = -np.cross(pb - pa, pc - pa)
            for i in (a, b, c):
                if i >= p0:
                    acc[i] = acc.get(i, 0) + nrm
        for i in range(p0, len(self.P)):
            v = acc.get(i)
            if v is not None and np.linalg.norm(v) > 1e-12:
                self.F[i] = v / np.linalg.norm(v)
            elif not np.any(self.F[i]):
                self.F[i] = np.array([1.0, 0, 0])


def side_points(info, arr, T2):
    """(welded vertex, region) -> the point (index) that region's triangles use there."""
    V2 = info["V"][arr["src"]]
    vp = {}
    for t in range(len(T2)):
        r = int(info["treg"][t])
        for j in T2[t]:
            vp.setdefault((int(V2[j]), r), int(j))
    return vp


def loop_frame(rig, cut, Q):
    """Centre, the cap's normal (stump to limb), the bone's line through the cut and the loop's mean radius."""
    nb = rig.head[rig.index[NEXT[cut]]] - rig.head[rig.index[cut]]
    nb = nb / np.linalg.norm(nb)
    seg = np.roll(Q, -1, axis=0) - Q
    l = np.linalg.norm(seg, axis=1)
    mid = (Q + np.roll(Q, -1, axis=0)) / 2
    c = (mid * l[:, None]).sum(0) / max(l.sum(), 1e-9)
    _, _, vt = np.linalg.svd(Q - c)
    npl = vt[2] * (1 if vt[2] @ nb >= 0 else -1)
    ncap = npl if npl @ nb > 0.5 else nb
    rad = float(np.linalg.norm((Q - c) - np.outer((Q - c) @ ncap, ncap), axis=1).mean())
    return c, ncap, nb, rad


def build_cap(B, rig, cut, lod, side, loop, VP, vp, region, bone_main, cloth_paint, seed, xw=None):
    """A cap closing `loop` on one side (side +1 the stump, -1 the limb end), recessed into its own side."""
    rng = np.random.default_rng(seed)
    Q = VP[loop]
    nq = len(loop)
    c, ncap, nb, rad = loop_frame(rig, cut, Q)
    out = ncap * side                       # the stump's cap faces the limb, the limb's faces the stump
    du = float(np.clip(0.30 * rad, 0.007, 0.022))
    det = LOD_DETAIL[lod]
    p0, t0 = len(B.P), len(B.T)
    ring_bones = []
    for v in loop:
        j = vp[(int(v), region)]
        ring_bones.append((B.part[j], B.part2[j], B.w2[j]))
    main = (bone_main, bone_main, 0.0)
    # the rings: (scale toward the centre, depth into the side, paint of the band outside it)
    if det == 3:
        spec = [("cloth", 0.70, 0.08, 0.60, 0.12), ("blood", 0.55, 0.05, 0.95, 0.12), ("wound", 0.30, 0.08, 1.25, 0.25)]
    elif det == 2:
        spec = [("cloth", 0.68, 0.06, 0.6, 0.1), ("blood", 0.5, 0.05, 0.95, 0.1)]
    else:
        spec = [("cloth", 0.6, 0.0, 0.6, 0.0)]
    rings = [{i: ("ring0", Q[i], ring_bones[i]) for i in range(nq)}]
    sel = [list(range(nq))]
    for k, (pn, s_, js, d_, jd) in enumerate(spec):
        # (the torn edge keeps every vertex of the seam; the rings inside it at most 16)
        if k == 0 or nq <= 16:
            ix = list(range(nq))
        else:
            ix = sorted({int(round(j * nq / 16)) % nq for j in range(16)})
        rr = {}
        for i in ix:
            tooth = (1 if i % 2 else -1) if k == 0 and det >= 2 else 0
            s = s_ + js * (0.6 * tooth + 0.8 * (rng.random() - 0.5))
            d = du * (d_ + jd * (rng.random() - 0.5) * 2)
            rr[i] = (pn, c + (Q[i] - c) * s - out * d, ring_bones[i])
        rings.append(rr)
        sel.append(ix)
    centre = c - out * du * 1.35
    cache = {}

    ring0 = {}

    def P_(k, i, pv):
        key = (k, i, pv)
        if key not in cache:
            _, pos, bone = rings[k][i]
            cache[key] = B.point(pos, pv, bone)
            if k == 0:
                ring0[cache[key]] = i
        return cache[key]

    for k in range(len(rings) - 1):
        pn = spec[k][0]
        A, Bs = sel[k], sel[k + 1]
        for j in range(len(Bs)):
            b0_, b1_ = Bs[j], Bs[(j + 1) % len(Bs)]
            pa_ = A.index(b0_)
            seg = [b0_]
            while seg[-1] != b1_:
                pa_ = (pa_ + 1) % len(A)
                seg.append(A[pa_])
            pv = cloth_paint[b0_] if pn == "cloth" else (BLOOD if pn == "blood" else WOUND)
            for x, y in zip(seg[:-1], seg[1:]):
                B.tri(P_(k, x, pv), P_(k, y, pv), P_(k + 1, b0_, pv), out)
            B.tri(P_(k, b1_, pv), P_(k + 1, b1_, pv), P_(k + 1, b0_, pv), out)
    last = len(rings) - 1
    lastp = WOUND if spec and spec[-1][0] == "wound" else BLOOD
    cpi = B.point(centre + out * du * 0.15 * (rng.random() - 0.5), WOUND, main)
    L_ = sel[last]
    lp = {i: B.point(rings[last][i][1], WOUND, rings[last][i][2]) for i in L_} if lastp != WOUND else None
    for j in range(len(L_)):
        i, i2 = L_[j], L_[(j + 1) % len(L_)]
        if lp is None:
            B.tri(P_(last, i, WOUND), P_(last, i2, WOUND), cpi, out)
        else:
            B.tri(lp[i], lp[i2], cpi, out)
    # the bone end: a short rough cylinder along the bone, standing out of the flesh but under the cut
    sides = {3: 7, 2: 4, 1: 3}[det]
    kind = cut.split("_")[0]
    rb = min(BONE_R[kind], 0.32 * rad)
    t_ = ((c - rig.head[rig.index[cut]]) @ ncap) / max(nb @ ncap, 0.3)
    b0 = rig.head[rig.index[cut]] + nb * t_
    off = (b0 - c) - ((b0 - c) @ ncap) * ncap
    if np.linalg.norm(off) > 0.35 * rad:
        off *= 0.35 * rad / np.linalg.norm(off)
    b0 = c + off
    e1 = np.cross(nb, [0.0, 0.0, 1.0])
    if np.linalg.norm(e1) < 0.2:
        e1 = np.cross(nb, [1.0, 0.0, 0.0])
    e1 /= np.linalg.norm(e1)
    e2 = np.cross(nb, e1)
    axis = nb * (1 if nb @ out > 0 else -1)             # pointing out of this side
    cosd = max(abs(nb @ ncap), 0.5)
    base = b0 - axis * (du * 1.6 / cosd)
    top = b0 - axis * (du * 0.42 / cosd)
    bot_i, top_i = [], []
    for k in range(sides):
        a = 2 * math.pi * (k + rng.random() * 0.3) / sides
        rr = rb * (0.85 + 0.3 * rng.random())
        radial = e1 * math.cos(a) * rr + e2 * math.sin(a) * rr
        bot_i.append(B.point(base + radial, BONE, main))
        top_i.append(B.point(top + radial + axis * du * 0.35 * (rng.random() - 0.5), BONE, main))
    for k in range(sides):
        k2 = (k + 1) % sides
        mid = (B.P[bot_i[k]] + B.P[top_i[k2]]) / 2 - b0
        o = mid - (mid @ axis) * axis
        B.tri(bot_i[k], bot_i[k2], top_i[k2], o)
        B.tri(bot_i[k], top_i[k2], top_i[k], o)
    tc = B.point(top - axis * du * 0.12, BONE, main)
    for k in range(sides):
        B.tri(top_i[k], top_i[(k + 1) % sides], tc, axis)
    if det >= 2:                            # (closed under the flesh too)
        bc = B.point(base, BONE, main)
        for k in range(sides):
            B.tri(bot_i[k], bot_i[(k + 1) % sides], bc, -axis)
    else:
        for k in range(1, sides - 1):
            B.tri(bot_i[0], bot_i[k], bot_i[k + 1], -axis)
    B.smooth_facings(p0, t0)
    # the weights while the cut is whole: the torn edge rides as the man's own skin at its seam point, the rest of
    # the cap halfway between the two bones (so it stays inside the joint as it bends)
    ci = rig.index[cut]
    joined = {}
    if xw is not None:
        half = (rig.parent[ci], ci, 0.5)
        for j in range(p0, len(B.P)):
            joined[j] = xw[ring0[j]] if j in ring0 else half
    return {"points": [p0, len(B.P) - p0], "triangles": [t0, len(B.T) - t0], "centre": c, "normal": ncap,
            "radius": rad, "depth": du, "joined": joined}


def build_lining(B, info, arr, T2, rig, cut, lod, side_region, other_region, loops, vp, rad, seed, bank, M0):
    """The hidden lining of one side: a copy of the other side's cloth for a few centimetres past the seam, inside it,
    riding this side's bones at the nearest seam point. Each of its points is set deep enough to stay under the other
    side's skin in every pose of the clips (checked against the posed skin at its source point); a point that cannot
    is left out, with its triangles."""
    import heapq
    TV, treg, VP, comp, rigid = info["TV"], info["treg"], info["VP"], info["comp"], info["rigid"]
    seam = set(int(v) for lp in loops for v in lp["verts"])
    if not seam:
        return {"points": [len(B.P), 0], "triangles": [len(B.T), 0]}
    comps = {comp[t] for t in range(len(TV)) if treg[t] == side_region and seam & set(int(x) for x in TV[t])}
    tris = [t for t in range(len(TV)) if treg[t] == other_region and not rigid[t] and comp[t] in comps]
    adj = {}
    for t in tris:
        a, b, c = (int(x) for x in TV[t])
        for u, v in ((a, b), (b, c), (c, a)):
            l = float(np.linalg.norm(VP[u] - VP[v]))
            adj.setdefault(u, []).append((v, l))
            adj.setdefault(v, []).append((u, l))
    dist, srcv = {}, {}
    h = []
    for v in seam:
        if v in adj:
            dist[v] = 0.0
            srcv[v] = v
            heapq.heappush(h, (0.0, v))
    while h:
        d, u = heapq.heappop(h)
        if d > dist.get(u, 1e9):
            continue
        for v, l in adj.get(u, []):
            if d + l < dist.get(v, 1e9):
                dist[v] = d + l
                srcv[v] = srcv[u]
                heapq.heappush(h, (d + l, v))
    L = float(np.clip(1.0 * rad, 0.035, 0.085))
    eps = {"near": 0.003, "mid": 0.005, "far": 0.008}[lod]
    # vertex normals (welded) of the other side
    vn = {}
    for t in tris:
        a, b, c = (VP[int(x)] for x in TV[t])
        nrm = -np.cross(b - a, c - a)
        for x in TV[t]:
            vn[int(x)] = vn.get(int(x), 0) + nrm
    sel = []
    for t in tris:
        vs = [int(x) for x in TV[t]]
        ds = [dist.get(v, 1e9) for v in vs]
        if (min(ds) == 0.0 and max(ds) <= 1.5 * L) or max(ds) <= L:
            sel.append(t)
    verts = sorted({int(x) for t in sel for x in TV[t]})
    # each point's depth: under the man's own (blended) skin at its source point in most poses of the clips, and under
    # the other side's skin too wherever the joint is turned less than 60 degrees there
    rep = {}
    for t in sel:
        for k, x in enumerate(TV[t]):
            rep.setdefault(int(x), int(arr["src"][int(T2[t][k])]))
    H = rig.head
    depth = {}
    if verts:
        Vp = np.array([VP[v] for v in verts])
        Nn = np.array([vn[v] / max(np.linalg.norm(vn[v]), 1e-12) for v in verts])
        so, oo = [], []
        for v in verts:
            q = vp.get((srcv.get(v, v), side_region))
            so.append((B.part[q], B.part2[q], B.w2[q]) if q is not None else (rig.rbone[side_region] or 0,) * 2 + (0.0,))
            q2 = vp.get((v, other_region))
            oo.append((B.part[q2], B.part2[q2], B.w2[q2]) if q2 is not None else so[-1])
        o = np.array([rep[v] for v in verts])
        xa, xb, xw = np.array(M0["part"])[o], np.array(M0["part2"])[o], np.array(M0["w2"], float)[o]

        def frame(a, b, w):
            P_ = bank_positions(Vp, a, b, w, bank, H)
            R_ = bank[0][:, a] * (1 - w)[None, :, None, None] + bank[0][:, b] * w[None, :, None, None]
            n_ = np.einsum("snij,nj->sni", R_, Nn)
            return P_, n_ / np.maximum(np.linalg.norm(n_, axis=2, keepdims=True), 1e-9)

        PS, nS = frame(*(np.array(x) for x in zip(*so)))         # the lining point at depth 0, and its dive
        PX, nX = frame(xa, xb, xw)                               # the man's own skin there
        PO, nO = frame(*(np.array(x) for x in zip(*oo)))         # the other side's skin there
        need_all = np.full((len(bank[0]), len(verts)), -1.0)
        for PR, nR, gate in ((PX, nX, None), (PO, nO, 0.5)):
            e0 = ((PS - PR) * nR).sum(2)
            ck = (nS * nR).sum(2)
            need = np.where(ck > 0.25, (e0 + eps) / np.maximum(ck, 0.25), np.where(e0 + eps <= 0, -1.0, 1e9))
            if gate is not None:
                need = np.where((nO * nS).sum(2) > gate, need, -1.0)
            need_all = np.maximum(need_all, need)
        req = np.quantile(need_all, LINING_POSES, axis=0)
        med = np.quantile(need_all, 0.5, axis=0)
        for k, v in enumerate(verts):
            if med[k] <= 0.45 * rad:          # (a point out of the skin in most poses is left out; the rest go as deep
                depth[v] = min(max(eps + 0.25 * dist.get(v, 0.0), float(req[k])), 0.45 * rad)   # as they need, at most)
        dvec = np.array([depth.get(v, np.nan) for v in verts])
        eX = ((PS - PX) * nX).sum(2) - dvec[None] * (nS * nX).sum(2)          # out of the man's own skin (m)
        ok = ~np.isnan(dvec)
        PROTRUDE.append((float((eX[:, ok] > 0.005).mean()) if ok.any() else 0.0, float(np.nanmax(eX[:, ok])) if ok.any() else 0.0, int(ok.sum())))
    p0, t0 = len(B.P), len(B.T)
    made = {}
    dropped = 0
    for t in sel:
        vs = [int(x) for x in TV[t]]
        if not all(v in depth for v in vs):
            dropped += 1
            continue
        idx = []
        for k, v in enumerate(vs):
            j = int(T2[t][k])
            key = (v, int(B.paint[j]), tuple(np.round(B.F[j], 3)))
            if key not in made:
                s = srcv.get(v, v)
                q = vp.get((s, side_region))
                bone = (B.part[q], B.part2[q], B.w2[q]) if q is not None else (rig.rbone[side_region] or 0,) * 2 + (0.0,)
                nn = vn[v] / max(np.linalg.norm(vn[v]), 1e-12)
                made[key] = B.point(VP[v] - nn * depth[v], B.paint[j], bone, B.F[j])
            idx.append(made[key])
        B.T.append(tuple(idx))
    return {"points": [p0, len(B.P) - p0], "triangles": [t0, len(B.T) - t0], "length": L, "inset": eps,
            "left_out": dropped}


# ================================================================================================ the twin
def make_twin(D, per_clip=6, log=print):
    rig = Rig(D["bones"])
    bank = pose_bank(rig, D["clips"], per_clip)
    twin = {"name": D["name"], "about": None, "long": D["long"], "high": D["high"]}
    if "marks" in D:
        twin["marks"] = D["marks"]
    gore = {"cuts": [], "paints": dict(GORE_PAINTS), "per_lod": {}}
    stats = {}
    sockets = [dict(s) for s in D["sockets"]]
    sock_new = []
    for lod in ("near", "mid", "far"):
        M = D[lod]
        arr, info = split_lod(M, rig, lod, bank)
        T2 = info["T2"]
        B = Builder(arr, T2)
        vp = side_points(info, arr, T2)
        cap_joined = {}
        st = {"points": [info["n"], None], "triangles": [info["m"], None], "band_points": info["band_points"],
              "hardened": info["hardened"], "seam_copies": [info["n"], info["seam_copies"]],
              "unrelated_neighbours": info["unrelated_neighbours"], "cuts": {}}
        for k, cut in enumerate(CUTS):
            c_reg = k + 1
            p_reg = rig.rparent[c_reg]
            pbone = rig.parent[rig.index[cut]]
            loops = [lp for lp in info["loops"][cut] if lp["closed"]]
            bad = [lp for lp in info["loops"][cut] if not lp["closed"]]
            stray = [b for b in bad if len(b["verts"]) <= 3]
            if bad and (len(stray) < len(bad) or (not loops and lod == "near")):
                raise RuntimeError(f"{lod} {cut}: open seam {[len(b['verts']) for b in bad]}")
            if stray:
                log(f"    {lod} {cut}: {len(stray)} stray seam edge(s) beside the loop left uncapped")
            if not loops:
                if lod == "near":
                    raise RuntimeError(f"{lod} {cut}: no seam")
                # (a thinned level whose limb is a shell of its own, touching nothing of its parent: it comes away whole)
                log(f"    {lod} {cut}: no seam (the limb meets its parent at {len(stray)} edge(s) at most at this "
                    f"level): no caps")
                st["cuts"][cut] = {"loops": [], "stump": {"cap_points": [len(B.P), 0], "cap_triangles": [len(B.T), 0]},
                                   "limb": {"cap_points": [len(B.P), 0], "cap_triangles": [len(B.T), 0]}}
                continue
            rec = {"loops": [len(lp["verts"]) for lp in loops]}
            if stray:
                rec["stray_edges"] = len(stray)
            for side, reg, other, bone_main, nm in ((1, p_reg, c_reg, pbone, "stump"), (-1, c_reg, p_reg, rig.index[cut], "limb")):
                caps = []
                for li, lp in enumerate(loops):
                    vs = lp["verts"] if side == -1 else lp["verts"][::-1]
                    cp = [B.paint[vp[(int(v), reg)]] for v in vs]
                    cl = [p for p in cp if p in CLOTH]
                    major = max(set(cl), key=cl.count) if cl else 0
                    cloth = [p if p in CLOTH else major for p in cp]
                    xw = []
                    for v in vs:
                        o = int(arr["src"][vp[(int(v), reg)]])
                        xw.append((int(M["part"][o]), int(M["part2"][o]), float(M["w2"][o])))
                    caps.append(build_cap(B, rig, cut, lod, side, np.array(vs), info["VP"], vp, reg, bone_main, cloth,
                                          seed=int(hash01(cut, lod, side, li) * 65535), xw=xw))
                    for j, w_ in caps[-1]["joined"].items():
                        regs = [rig.region[w_[0]], rig.region[w_[1]] if w_[2] > 0 else rig.region[w_[0]], k + 1, p_reg]
                        cap_joined[j] = (w_, rig.cut_mask(regs))
                lin = {"points": [len(B.P), 0], "triangles": [len(B.T), 0]}
                if nm in LININGS:
                    lin = build_lining(B, info, arr, T2, rig, cut, lod, reg, other, loops, vp,
                                       max(c["radius"] for c in caps), seed=(k, side), bank=bank, M0=M)
                if lin.get("left_out"):
                    log(f"    {lod} {cut} {nm}: lining {lin['triangles'][1]} triangles, {lin['left_out']} left out")
                rec[nm] = {"cap_points": [caps[0]["points"][0], sum(c["points"][1] for c in caps)],
                           "cap_triangles": [caps[0]["triangles"][0], sum(c["triangles"][1] for c in caps)]}
                if LININGS:
                    rec[nm].update({"lining_points": lin["points"], "lining_triangles": lin["triangles"]})
                if nm == "stump":
                    big = max(caps, key=lambda c: c["radius"])
                    rec["_frame"] = (big["centre"], big["normal"], big["radius"], big["depth"])
            st["cuts"][cut] = rec
        # the man's own weights for every point the split changed (and its seam copies), and the caps' joined weights:
        # used while none of the cuts in its mask is off, the twin moves exactly as he does and the caps stay inside
        jl = []
        R = rig.region
        for j in range(len(arr["part"])):
            o = int(arr["src"][j])
            xo = (int(M["part"][o]), int(M["part2"][o]), float(M["w2"][o]))
            tw = (int(B.part[j]), int(B.part2[j]), float(B.w2[j]))
            if j < info["n"] and tw[0] == xo[0] and abs(tw[2] - xo[2]) < 1e-6 and (tw[2] == 0 or tw[1] == xo[1]):
                continue
            regs = [R[tw[0]], R[tw[1]] if tw[2] > 0 else R[tw[0]], R[xo[0]], R[xo[1]] if xo[2] > 0 else R[xo[0]]]
            m_ = rig.cut_mask(regs)
            if m_ == 0:
                continue
            jl.append((j, xo, m_))
        for j, (w_, m_) in sorted(cap_joined.items()):
            jl.append((j, w_, m_))
        joined_lod = {"points": [j for j, _, _ in jl], "part": [w[0] for _, w, _ in jl],
                      "part2": [w[1] for _, w, _ in jl], "w2": [round(w[2], 3) for _, w, _ in jl],
                      "cuts": [m for _, _, m in jl]}
        Pn = np.array(B.P)
        mesh = {"points": [round(float(v), 4) for v in Pn.ravel()],
                "facings": [round(float(v), 3) for v in np.array(B.F).ravel()],
                "paints": [int(v) for v in B.paint], "triangles": [int(v) for t in B.T for v in t],
                "part": [int(v) for v in B.part], "part2": [int(v) for v in B.part2],
                "w2": [round(float(v), 3) for v in B.w2]}
        if B.shade is not None:
            mesh["shade"] = [round(float(v), 3) for v in B.shade]
        # the originals exactly as they were (their values, not re-rounded)
        n = info["n"]
        mesh["points"][:3 * n] = M["points"]
        mesh["facings"][:3 * n] = M["facings"]
        for key in ("part", "part2", "w2"):
            for i in range(n):
                if M[key][i] == (arr[key][i] if key != "w2" else round(float(arr[key][i]), 3)):
                    mesh[key][i] = M[key][i]
        twin[lod] = mesh
        st["joined"] = joined_lod
        st["points"][1] = len(B.P)
        st["triangles"][1] = len(B.T)
        stats[lod] = st
        log(f"  {lod}: points {n} -> {len(B.P)}, triangles {info['m']} -> {len(B.T)}, band {info['band_points']}, "
            f"copies {info['seam_copies']}, loops " + ", ".join(f"{c} {st['cuts'][c]['loops']}" for c in CUTS))
    twin["bones"] = D["bones"]
    # sockets: the delivered ones, then a wound and an end per cut (from the near cut)
    for cut in CUTS:
        c, ncap, rad, du = stats["near"]["cuts"][cut]["_frame"]
        pb = rig.parent[rig.index[cut]]
        sock_new.append({"name": f"wound_{cut}", "part": pb, "part_name": rig.names[pb],
                         "at": [round(float(v), 4) for v in c], "facing": [round(float(v), 4) for v in ncap]})
        sock_new.append({"name": f"end_{cut}", "part": rig.index[cut], "part_name": cut,
                         "at": [round(float(v), 4) for v in c], "facing": [round(float(v), 4) for v in -ncap]})
    twin["sockets"] = sockets + sock_new
    twin["clips"] = {}
    colours = dict(D.get("colours", {}))
    colours.update(GORE_COLOURS)
    twin["colours"] = colours
    newp = dict(D.get("new_paints", {}))
    newp.update(GORE_PAINTS)
    twin["new_paints"] = newp
    for key in ("helmet_paints", "pair", "faces", "helmets", "inks", "helmet_tris", "about_variety"):
        if key in D:
            twin[key] = D[key]
    for cut in CUTS:
        pb = rig.parent[rig.index[cut]]
        c = {"bone": cut, "part": rig.index[cut], "parent": rig.names[pb], "parent_part": pb,
             "joint": [round(float(v), 4) for v in rig.head[rig.index[cut]]],
             "limb": [rig.names[i] for i in range(len(rig.names)) if rig.index[cut] in rig.ancestors(i)],
             "sockets": [f"wound_{cut}", f"end_{cut}"]}
        for lod in ("near", "mid", "far"):
            r = dict(stats[lod]["cuts"][cut])
            r.pop("_frame", None)
            c[lod] = r
        gore["cuts"].append(c)
    for lod in ("near", "mid", "far"):
        s = stats[lod]
        gore["per_lod"][lod] = {"points": s["points"], "triangles": s["triangles"], "seam_copies": s["seam_copies"],
                                "band_points_hardened": s["band_points"]}
    gore["joined"] = {lod: stats[lod]["joined"] for lod in ("near", "mid", "far")}
    gore["about"] = ABOUT_GORE
    twin["gore"] = gore
    twin["about"] = D["about"] + " " + ABOUT_TWIN
    return twin, stats, rig


ABOUT_TWIN = ("GORE TWIN (Tools/battle_units/gore.py): this man with his limbs able to come off at eight joints; see `gore`. "
              "Same bones, same sockets first, same faces, helmets and inks as his own file; `clips` is empty for the "
              "deaths thread's gore deaths. The helmet triangles (helmet_tris) are no longer the last of each list: the "
              "caps follow them, so leave out [first, first + count) only.")
ABOUT_GORE = (
    "Eight cuts at the ragdoll's own joints: upperarm_l/_r (the arm at the shoulder), lowerarm_l/_r (the elbow), "
    "thigh_l/_r (the whole leg at the hip), calf_l/_r (the knee). No skin is shared across a cut: at each joint every point "
    "rides 100% one side (w2 0 across the cut) and the seam is doubled, so moving a cut bone away from its parent (a bone "
    "move in a clip; the battle keeps moves on every bone) takes the limb off clean, with nothing stretched. Each open side "
    "is closed by a cap recessed into its own side: torn cloth (the uniform's own paint), a soaked edge (paint `blood`), "
    "raw flesh (`wound`) and a short rough bone end (`bone`); at rest the caps are inside him. "
    "JOINED: hard weights open the bent joints (the shoulder and hip most: a crack of 5 to 10 cm in a kneel, the caps red "
    "inside it). `joined` puts that right: per level of detail a list of points (`points`) with the man's own weights "
    "(`part`, `part2`, `w2`) and `cuts`, a mask (bit k for cut k in the order of `cuts` below: 1 upperarm_l, 2 lowerarm_l, "
    "4 upperarm_r, 8 lowerarm_r, 16 thigh_l, 32 calf_l, 64 thigh_r, 128 calf_r). Skin each listed point with the listed "
    "weights while none of the cuts in its mask is off, with its own once one is: whole, the twin then moves exactly as "
    "his own file (every original point; the seam copies with it) and the caps ride the joint inside him; a cut off "
    "takes its own points to hard weights and leaves every other joint whole. (Without it the twin still works, "
    "cracked.) Per cut: `bone`, `parent`, `joint` (the cut bone's rest head, these axes), `limb` (the bones that go with "
    "it), `sockets` (`wound_<bone>` on the stump, on the parent bone, at the cut's centre facing out of the stump: blood "
    "spurts, smoke, the flies; `end_<bone>` on the limb, facing out of its end), and per level of detail `loops` (seam "
    "vertices) and for `stump` and `limb` the [first, count] of their cap's points and triangles. Arrays: near, mid and "
    "far keep the delivered man's points and triangles first, in order, with their values (only part/part2/w2 changed, "
    "at the joints' bands), then the seam copies, then the caps cut by cut. Paints: wound 35, bone 36, blood 37 "
    "(`colours`, `new_paints`): darken or colour them as the battle likes. With gore off, draw X.json as ever; with gore "
    "on, swap to this file at the hit and play a gore death from its `clips` (the deaths thread's).")


# ================================================================================================ files
def stage_twin(name, log=print, per_clip=6):
    t0 = time.time()
    src = os.path.join(SHARED, name + ".json")
    D = json.load(open(src))
    log(f"{name}: read {src} ({os.path.getsize(src):,} bytes)")
    twin, stats, rig = make_twin(D, per_clip, log)
    os.makedirs(STAGE, exist_ok=True)
    out = os.path.join(STAGE, name + "_gore.json")
    with open(out, "w") as f:
        json.dump(twin, f, separators=(",", ":"))
    log(f"{name}: staged {out} ({os.path.getsize(out):,} bytes) in {time.time() - t0:.0f} s")
    return out


def deliver(name):
    """Copy a staged twin into the shared folder: a new file only, never over an existing one; read back after 10 s."""
    local = os.path.join(STAGE, name + "_gore.json")
    dest = os.path.join(SHARED, name + "_gore.json")
    if os.path.exists(dest):
        raise SystemExit(f"{dest} exists: not overwritten")
    h = hashlib.sha256(open(local, "rb").read()).hexdigest()
    shutil.copyfile(local, dest)
    time.sleep(10)
    h2 = hashlib.sha256(open(dest, "rb").read()).hexdigest()
    ok = h == h2 and os.path.getsize(dest) == os.path.getsize(local)
    print(f"delivered {dest}: {os.path.getsize(dest):,} bytes, sha256 {h2[:16]}..., read back {'ok' if ok else 'MISMATCH'}")
    return ok


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "--deliver":
        for nm in a[1:]:
            deliver(nm)
    else:
        for nm in a or ["soldier_USA"]:
            stage_twin(nm)
