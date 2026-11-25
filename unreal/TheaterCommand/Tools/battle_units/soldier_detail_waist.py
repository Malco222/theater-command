"""The detailed rifleman's waist (release 2, behind -TheaterLandDetailMen): the art director's Mac pass (25 November
2026, fault 7) found that "at one metre the torso reads inflated". Read in the game's own poses from behind, three
things made it so: his back was a few big smooth panels (its points about 3 cm apart, against 2 cm on his chest, so the
light slid over it as over a cushion), the skirt of his tunic bulged 4.4 cm out over his seat, and the belt stood 4 cm
proud of the small of his back. This pass mends the finished files (the man and his gore twin) in place, without
running the file chain again:

  1. the back of his tunic, from below the belt to under the shoulder-blade yoke, is meshed finer: every edge there
     longer than 2.4 cm is split, and in the two bands where cloth folds (a hand's breadth over the belt and under it,
     and the strip down his spine) every edge longer than 1.15 cm. A new point sits on the smooth surface the old
     facings describe (Phong's projection, three quarters of the way), and takes its two ends' weights (the two
     strongest bones of their mix), paint, shade, dirt, wet, snow and tint half and half;
  2. the new cloth is laid in soft folds along its facing: one soft roll where the tunic pouches out over the belt
     (deepest, 1.2 cm, just above the belt's top edge, gone 8 cm up), three shallow pulls running down and in to the
     belt 20 to 40 degrees off the vertical (uneven in length and spacing, not mirrored either side of the strap),
     shallow creases gathered under the belt, and a groove down his spine. (The art director's steer of 25 November,
     14:29Z: the first try's three level ridges a side read as ribs drawn on his back.) The valleys are baked a little
     darker in shade and shade_clean (up to 14 per cent), the ridges are as they were. Each facing is turned only by
     what the folds turned the surface, so where there is no fold nothing changes;
  3. the waist drawn in by a smooth field of rest-pose height and side: the skirt over his seat up to 1.9 cm in at the
     back, 1 cm at each side, half a centimetre in front; the belt line 1.9 cm in at the back; nothing above the
     shoulder blades or below the hem. The belt, the Y-straps and anything else lying on the tunic follow the cloth
     under them point by point; hanging kit (bread bag, canteen and cup, gas mask can, spade, pouches) moves whole, each
     cluster of pieces hung together by the least the cloth beside it moved, so nothing sinks into the tunic.

The middle level takes only the waist (3): it is drawn past about 9 m, where folds of a few millimetres do not show.
The far level is not touched. New points and triangles are appended (each split triangle keeps its place with one of
its children): pack_tris, helmet_tris, the faces' points and the gore twin's cuts and caps keep their numbers.

The man and his twin are mended together: triangles that the twin's cuts changed, and points whose weights the cuts
hardened, are left out of the finer mesh in both, so both are split alike and their new points come in the same order.

    cd Tools/battle_units
    /home/user/bpyenv/bin/python soldier_detail_waist.py --mend MAN TWIN OUT_MAN OUT_TWIN    (any python with numpy, scipy)
    /home/user/bpyenv/bin/python soldier_detail_waist.py --deliver                           (the shared files, backed up)

Each mended file carries `waist_mend` (what was added), and the pass refuses a file that has it.
"""
import json
import os
import sys

import numpy as np
from scipy.interpolate import PchipInterpolator
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from scipy.spatial import cKDTree

SHARED = "/mnt/project-files/battle-models/land/soldier/"
NAME = "soldier_GER_detail"
ARM = ("clavicle", "upperarm", "lowerarm", "hand", "fingers", "thumb")
PER_POINT = ("paints", "part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow")
BAKED = ("shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow")
# How far in the waist is drawn, in metres, by rest-pose height: at the middle of his back, in front, at his sides.
WAIST = {
    "z":     [0.770, 0.800, 0.850, 0.900, 0.950, 1.000, 1.050, 1.080, 1.110, 1.150, 1.220, 1.300],
    "back":  [0.000, 0.003, 0.012, 0.018, 0.019, 0.019, 0.019, 0.016, 0.006, 0.002, 0.000, 0.000],
    "front": [0.000, 0.000, 0.002, 0.004, 0.005, 0.005, 0.004, 0.003, 0.002, 0.000, 0.000, 0.000],
    "side":  [0.000, 0.000, 0.004, 0.009, 0.010, 0.009, 0.007, 0.006, 0.003, 0.001, 0.000, 0.000],
}
BACK_Z = (0.88, 1.29)          # (the finer back: from under the belt's gathers to under the shoulder-blade yoke)
EDGE, EDGE_FOLDS, EDGE_PULLS = 0.024, 0.0115, 0.0085
DARKEN = 0.14                  # (the deepest valley's shade, at 4.5 mm in)
ROLL = 0.012                   # (the roll over the belt, metres out at its deepest)
# The pulls above the belt, in (across his back y, height over the belt's top h) from their top to where they meet the
# belt, and how deep: 20 to 40 degrees off the vertical, running in toward the strap down his spine, three of them,
# two on one side and one on the other, of uneven length.
PULLS = (((-0.118, 0.105), (-0.078, 0.008), 0.0022),      # (24 degrees)
         ((-0.070, 0.064), (-0.034, 0.006), 0.0016),      # (32 degrees, shorter)
         ((0.128, 0.088), (0.062, 0.004), 0.0024))        # (38 degrees)
PULL_WIDTH = 0.008


def _islands(T, N):
    r = np.concatenate([T[:, 0], T[:, 1], T[:, 2]])
    c = np.concatenate([T[:, 1], T[:, 2], T[:, 0]])
    return connected_components(coo_matrix((np.ones(len(r)), (r, c)), shape=(N, N)), directed=False)[1]


def _arm_weight(L, bones):
    names = [b["name"] for b in bones]
    isarm = np.array([n.startswith(ARM) for n in names])
    part, part2, w2 = np.array(L["part"]), np.array(L["part2"]), np.array(L["w2"], float)
    return (1 - w2) * isarm[part] + w2 * isarm[part2]


def _co_located(P):
    """For each point, its group of points at the same place (a seam's two sides), and how many share it."""
    key = np.round(P / 1e-4).astype(np.int64)
    _, inv, cnt = np.unique(key, axis=0, return_inverse=True, return_counts=True)
    inv = inv.ravel()
    return inv, cnt


class Field:
    """The waist: a displacement for each rest-pose point of his trunk, from its height and where it lies about the
    middle of his trunk at that height (read from the man's own tunic: its middle front to back, how far its back,
    front and sides stand from it), so that a point on the cloth's surface moves the full amount of the table, one
    nearer his middle proportionally less."""

    def __init__(self, P, torso):
        zs = np.arange(0.76, 1.33, 0.01)
        rows = []
        for z in zs:
            b = P[torso & (np.abs(P[:, 2] - z) < 0.015)]
            mid = b[np.abs(b[:, 1]) < 0.12]
            if len(mid) < 4:
                rows.append((np.nan,) * 4)
                continue
            lo, hi = mid[:, 0].min(), mid[:, 0].max()
            c = 0.5 * (lo + hi)
            rows.append((c, c - lo, hi - c, np.abs(b[:, 1]).max()))
        rows = np.array(rows)
        for k in range(4):                                   # (heights the cloth leaves bare: from their neighbours)
            ok = ~np.isnan(rows[:, k])
            rows[:, k] = np.interp(zs, zs[ok], rows[ok, k])
            rows[:, k] = np.convolve(np.pad(rows[:, k], 2, mode="edge"), np.ones(5) / 5, mode="valid")
        self.zs = zs
        self.cx, self.rb, self.rf, self.rs = rows.T
        self.d = {k: PchipInterpolator(WAIST["z"], WAIST[k], extrapolate=False) for k in ("back", "front", "side")}

    def at(self, P):
        zc = np.clip(P[:, 2], self.zs[0], self.zs[-1])
        return [np.interp(zc, self.zs, a) for a in (self.cx, self.rb, self.rf, self.rs)]

    def __call__(self, P):
        cx, rb, rf, rs = self.at(P)
        d = {k: np.nan_to_num(f(P[:, 2])) for k, f in self.d.items()}
        dx = P[:, 0] - cx
        D = np.zeros_like(P)
        D[:, 0] = np.where(dx < 0, d["back"] * np.minimum(-dx / rb, 1.3), -d["front"] * np.minimum(dx / rf, 1.3))
        D[:, 1] = -np.sign(P[:, 1]) * d["side"] * np.minimum(np.abs(P[:, 1]) / rs, 1.3)
        return D

    def back(self, P):
        """Points on his back half (with a centimetre of his sides), in the finer band."""
        cx = self.at(P)[0]
        return (P[:, 0] < cx + 0.01) & (P[:, 2] > BACK_Z[0]) & (P[:, 2] < BACK_Z[1])


def _edge_target(M):
    z, y = M[:, 2], M[:, 1]
    folds = ((z > 0.965) & (z < 1.045)) | ((z > 1.075) & (z < 1.20)) | ((np.abs(y) < 0.035) & (z > 1.15))
    pulls = (z > 1.08) & (z < 1.20) & (np.abs(y) > 0.02) & (np.abs(y) < 0.15)
    return np.where(pulls, EDGE_PULLS, np.where(folds, EDGE_FOLDS, EDGE))


def _refine(L, field, armw, frozen, rounds=4):
    """Split the back's long edges (see the top), a few rounds, the cloth kept whole: a triangle with one, two or three
    of its edges split is cut into two, three or four, so a split edge is split on both its sides. Triangles with a
    point on a seam (two points at one place: the yoke, a paint's cut) or a frozen point are never in the band, so
    no edge on a seam is split. Returns the new lists."""
    P = [p for p in np.array(L["points"], float).reshape(-1, 3)]
    F = [f for f in np.array(L["facings"], float).reshape(-1, 3)]
    tint = [t for t in np.array(L["tint"], float).reshape(-1, 3)]
    T = np.array(L["triangles"]).reshape(-1, 3).copy()
    at = {k: list(L[k]) for k in PER_POINT}
    arm = list(armw)
    fro = list(frozen)
    added = 0
    for _ in range(rounds):
        Pa, Fa = np.array(P), np.array(F)
        inv, cnt = _co_located(Pa)
        band = field.back(Pa) & (np.array(at["paints"]) == 0) & (np.array(arm) < 0.05) & ~(cnt[inv] > 1) & ~np.array(fro)
        tri_in = band[T].all(1)
        E = np.sort(np.concatenate([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]]), 1)
        mid = 0.5 * (Pa[E[:, 0]] + Pa[E[:, 1]])
        long = np.linalg.norm(Pa[E[:, 0]] - Pa[E[:, 1]], axis=1) > _edge_target(mid)
        marked = sorted({tuple(e) for e in E[np.concatenate([tri_in] * 3) & long]})
        if not marked:
            break
        newid = {}
        for a, b in marked:
            m = 0.5 * (Pa[a] + Pa[b])
            # (on the smooth surface the facings describe: Phong's projection onto each end's tangent plane)
            pa_ = m - np.dot(m - Pa[a], Fa[a]) * Fa[a]
            pb_ = m - np.dot(m - Pa[b], Fa[b]) * Fa[b]
            newid[(a, b)] = len(P)
            P.append(0.25 * m + 0.75 * 0.5 * (pa_ + pb_))
            f = Fa[a] + Fa[b]
            F.append(f / np.linalg.norm(f))
            tint.append(0.5 * (tint[a] + tint[b]))
            for k in BAKED:
                at[k].append(round(0.5 * (at[k][a] + at[k][b]), 3))
            at["paints"].append(at["paints"][a])
            w = {}
            for i in (a, b):
                w[at["part"][i]] = w.get(at["part"][i], 0.0) + 0.5 * (1 - at["w2"][i])
                w[at["part2"][i]] = w.get(at["part2"][i], 0.0) + 0.5 * at["w2"][i]
            top = sorted(w.items(), key=lambda kv: (-kv[1], kv[0]))[:2]
            if len(top) < 2 or top[1][1] <= 0:
                at["part"].append(int(top[0][0])); at["part2"].append(int(top[0][0])); at["w2"].append(0.0)
            else:
                at["part"].append(int(top[0][0])); at["part2"].append(int(top[1][0]))
                at["w2"].append(round(float(top[1][1] / (top[0][1] + top[1][1])), 3))
            arm.append(0.5 * (arm[a] + arm[b]))
            fro.append(False)
            added += 1
        keep, more = [], []
        for a, b, c in T:
            ab, bc, ca = newid.get((min(a, b), max(a, b))), newid.get((min(b, c), max(b, c))), newid.get((min(c, a), max(c, a)))
            n = (ab is not None) + (bc is not None) + (ca is not None)
            if n == 0:
                kids = [[a, b, c]]
            elif n == 3:
                kids = [[a, ab, ca], [ab, b, bc], [ca, bc, c], [ab, bc, ca]]
            elif n == 1:
                kids = ([[a, ab, c], [ab, b, c]] if ab is not None else
                        [[a, b, bc], [a, bc, c]] if bc is not None else [[a, b, ca], [ca, b, c]])
            elif ab is None:
                kids = [[a, b, bc], [a, bc, ca], [ca, bc, c]]
            elif bc is None:
                kids = [[a, ab, ca], [ab, b, c], [ab, c, ca]]
            else:
                kids = [[a, ab, c], [ab, b, bc], [ab, bc, c]]
            keep.append(kids[0])
            more += kids[1:]
        T = np.array(keep + more)
    return np.array(P), np.array(F), T, at, np.array(tint), np.array(arm)


def _vertex_normals(P, T):
    N = np.zeros_like(P)
    fn = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    for k in range(3):
        np.add.at(N, T[:, k], fn)
    n = np.linalg.norm(N, axis=1, keepdims=True)
    return N / np.maximum(n, 1e-12)


def _folds(P, field):
    """How far each back point is laid out (+) or in (-) along its facing, metres, by rest-pose position."""
    z, y = P[:, 2], P[:, 1]
    cx, rb, _, _ = field.at(P)
    b = np.clip((cx - P[:, 0]) / rb, 0, 1)                    # (1 at the middle of his back, 0 at his sides)

    def band(z0, z1, e):
        return np.clip((z - z0) / e, 0, 1) * np.clip((z1 - z) / e, 0, 1)
    D = -0.0040 * np.exp(-(y / 0.02) ** 2) * band(1.13, 1.29, 0.045) * b ** 2           # (the spine's groove)
    # the roll pouched over the belt: from its top edge (where the belt is at this side of him), deepest 1 cm up
    h = z - np.interp(y, field.belt[0], field.belt[1])
    roll = np.clip((h - 0.001) / 0.009, 0, 1) ** 0.7 * np.exp(-np.clip(h - 0.010, 0, None) ** 2 / (2 * 0.024 ** 2))
    D += ROLL * roll * np.clip((h < 0.08) * 1.0, 0, 1) * np.clip(b * 1.5, 0, 1) * np.clip((0.08 - h) / 0.02, 0, 1)
    # the pulls: shallow creases from the cloth above down and in to the belt, each a valley with a soft lip
    # either side (a Ricker profile across it), faded at both ends
    for (y0, h0), (y1, h1), deep in PULLS:
        a0, a1 = np.array([y0, h0]), np.array([y1, h1])
        ab = a1 - a0
        q = np.stack([y, h], 1) - a0
        t = np.clip(q @ ab / (ab @ ab), 0, 1)
        r = np.linalg.norm(q - t[:, None] * ab, axis=1) / PULL_WIDTH
        along = np.sin(np.pi * np.clip(t, 0, 1)) ** 0.6
        D += -deep * (1 - 2 * r ** 2) * np.exp(-r ** 2) * along * (h > 0.004)
    env2 = band(0.93, 1.034, 0.05) * np.clip((1.034 - z) / 0.012, 0, 1)                 # (gathered under it)
    D += env2 * 0.0022 * np.cos(2 * np.pi * y / 0.05 + 0.8 * np.sin(z / 0.03)) * np.clip(b * 1.4, 0, 1)
    return D * np.clip((1.285 - z) / 0.03, 0, 1) * np.clip((z - 0.885) / 0.02, 0, 1)


def _waist(P, F, T, paints, armw, field):
    """The waist (3 at the top) on one level's points: returns the moved points and facings."""
    N = len(P)
    lab = _islands(T, N)
    region = (P[:, 2] > 0.74) & (P[:, 2] < 1.34) & (np.abs(P[:, 1]) < 0.34)
    cloth = (paints == 0) & region & (armw < 0.999)
    torso = cloth & (armw < 0.05)
    D = np.zeros_like(P)
    D[cloth] = field(P[cloth]) * (1 - armw[cloth])[:, None]
    # the cloth's facings turned by the field's own slope
    idx = np.where(cloth)[0]
    h = 1e-3
    J = np.zeros((len(idx), 3, 3))
    for a in range(3):
        e = np.zeros(3)
        e[a] = h
        w = (1 - armw[idx])[:, None]
        J[:, :, a] = ((P[idx] + e + field(P[idx] + e) * w) - (P[idx] - e + field(P[idx] - e) * w)) / (2 * h)
    Fi = np.linalg.solve(np.transpose(J, (0, 2, 1)), F[idx][:, :, None])[:, :, 0]
    F2 = F.copy()
    F2[idx] = Fi / np.linalg.norm(Fi, axis=1, keepdims=True)
    # what lies on the cloth, and what hangs from the belt
    tree = cKDTree(P[torso])
    Dt = D[torso]
    whole = {}
    report = {"follow": 0, "whole": 0}
    for i in np.unique(lab[region & ~cloth]):
        m = lab == i
        if (paints[m] == 0).all() or (armw[m] > 0.5).mean() > 0.5:
            continue
        if np.isin(paints[m], (1, 2, 7)).mean() > 0.5 or (P[m, 2] < 0.70).mean() > 0.2:
            continue                               # (the trousers and boots, his skin: not the trunk's)
        q = P[m]
        dist, nn = tree.query(q, k=4)
        if dist[:, 0].min() > 0.06:
            continue
        follow = Dt[nn].mean(axis=1)
        ext = np.ptp(q, axis=0)
        if ext[1] > 0.2 or ext[2] > 0.25:          # (the belt round him, a strap over him: each point with the cloth)
            D[m] = follow * np.clip((1.33 - q[:, 2]) / 0.03, 0, 1)[:, None]     # (none past the shoulder blades)
            report["follow"] += 1
        else:
            near = dist[:, 0] < max(0.03, dist[:, 0].min() + 0.01)
            whole[i] = (np.where(m)[0], follow[near])
            report["whole"] += 1
    # (pieces hung together, a canteen with its cup and strap, move as one, by the least the cloth beside any moved)
    keys = list(whole)
    root = {k: k for k in keys}

    def find(k):
        while root[k] != k:
            root[k] = root[root[k]]
            k = root[k]
        return k
    box = {k: (P[whole[k][0]].min(0) - 0.005, P[whole[k][0]].max(0) + 0.005) for k in keys}
    trees = {k: cKDTree(P[whole[k][0]]) for k in keys}
    for n, a in enumerate(keys):
        for b in keys[n + 1:]:
            if (box[a][0] > box[b][1]).any() or (box[b][0] > box[a][1]).any():
                continue
            if trees[a].query(P[whole[b][0]], k=1, distance_upper_bound=0.005)[0].min() < 0.005:
                root[find(a)] = find(b)
    groups = {}
    for k in keys:
        groups.setdefault(find(k), []).append(k)
    for g in groups.values():
        cand = np.concatenate([whole[k][1] for k in g])
        v = cand[np.argmin(np.linalg.norm(cand, axis=1))]
        for k in g:
            D[whole[k][0]] = v
    report["groups"] = len(groups)
    report["most_cm"] = round(float(np.linalg.norm(D, axis=1).max()) * 100, 2)
    return P + D, F2, report


def _frozen(main, twin, lod):
    """Points the twin's cuts touched (in triangles the cuts changed, or with weights the cuts hardened): no finer mesh
    about them, in the man or the twin, so both are split alike."""
    a, b = main[lod], twin[lod]
    n = len(a["points"]) // 3
    Ta = np.array(a["triangles"]).reshape(-1, 3)
    Tb = np.array(b["triangles"]).reshape(-1, 3)[:len(Ta)]
    fro = np.zeros(n, bool)
    diff = (Ta != Tb).any(1)
    fro[Ta[diff].ravel()] = True
    for k in ("part", "part2", "w2"):
        fro |= np.array(a[k]) != np.array(b[k])[:n]
    return fro


def _written(old, new, places):
    """new's values as the file's numbers, rounded as the files are, but each value of a point that did not move
    kept exactly as it was written (the face's points carry more places than 4)."""
    o = np.array(old, float).reshape(-1, 3)
    n = new.reshape(-1, 3)
    out = [round(float(v), places) for v in n.ravel()]
    same = np.zeros(len(n), bool)
    same[:len(o)] = np.abs(n[:len(o)] - o).max(1) < 1e-12
    for i in np.where(same)[0]:
        out[3 * i:3 * i + 3] = old[3 * i:3 * i + 3]
    return out


def _write_lod(L, P, F, T=None, at=None, tint=None):
    L["points"] = _written(L["points"], P, 4)
    L["facings"] = _written(L["facings"], F, 3)
    if T is not None:
        L["triangles"] = [int(v) for v in T.ravel()]
        L["tint"] = _written(L["tint"], tint, 3)
        for k in PER_POINT:
            L[k] = at[k]


def _marker(D, rep):
    D["waist_mend"] = {"version": 1, "near_points_added": rep["near"]["points_added"],
                       "near_triangles_added": rep["near"]["triangles_added"],
                       "why": "art director, Mac pass 25 Nov 2026, fault 7: the torso read inflated at one metre"}


def mend_man(D, frozen, fields):
    """The man, in place. Returns the report and what the twin copies: each level's points and facings as they ended,
    and the near level's darkening of the folds' valleys."""
    if "waist_mend" in D:
        raise SystemExit("this file has its waist already (waist_mend): not mended twice")
    rep = {}
    L = D["near"]
    n0, t0 = len(L["points"]) // 3, len(L["triangles"]) // 3
    tint_before = list(L["tint"])
    P, F, T, at, tint, armw = _refine(L, fields["near"], _arm_weight(L, D["bones"]), frozen)
    sel = fields["near"].back(P) & (np.array(at["paints"]) == 0) & (armw < 0.05)
    Nb = _vertex_normals(P, T)
    disp = _folds(P, fields["near"]) * sel
    V = np.where(sel[:, None], Nb, F) * disp[:, None]
    inv, cnt = _co_located(P)                         # (a seam's two sides moved alike)
    acc = np.zeros((cnt.size, 3))
    np.add.at(acc, inv, V)
    P = P + acc[inv] / cnt[inv][:, None]
    Na = _vertex_normals(P, T)
    cloth = np.array(at["paints"]) == 0
    Fn = F + (Na - Nb)
    F = np.where(cloth[:, None], Fn / np.linalg.norm(Fn, axis=1, keepdims=True), F)
    dark = 1.0 - np.clip(-disp / 0.0045, 0, 1) * DARKEN
    for k in ("shade", "shade_clean"):
        at[k] = [round(float(v * dk), 3) if dk < 1 else v for v, dk in zip(at[k], dark)]
    P, F, rep["near"] = _waist(P, F, T, np.array(at["paints"]), armw, fields["near"])
    _write_lod(L, P, F, T, at, tint)
    _fd_keep(D, "near", tint_before)
    rep["near"].update(points_added=len(P) - n0, triangles_added=len(T) - t0)
    M = D["mid"]
    Pm = np.array(M["points"], float).reshape(-1, 3)
    Fm = np.array(M["facings"], float).reshape(-1, 3)
    Tm = np.array(M["triangles"]).reshape(-1, 3)
    Pm, Fm, rep["mid"] = _waist(Pm, Fm, Tm, np.array(M["paints"]), _arm_weight(M, D["bones"]), fields["mid"])
    _write_lod(M, Pm, Fm)
    _marker(D, rep)
    return rep, {"near": (P, F, dark, n0), "mid": (Pm, Fm, None, len(Pm))}


def mend_twin(Dt, man_before, done, frozen, fields):
    """The gore twin, in place, from the man's result: his finer back split alike (the same edges, the same order),
    every point he shares with the man (all the man's points, and the new ones) where the man's ended, with his
    facings and the folds' darkening; the twin's own points (the cuts' caps) moved as the man's nearest point moved."""
    if "waist_mend" in Dt:
        raise SystemExit("this file has its waist already (waist_mend): not mended twice")
    rep = {}
    for lod in ("near", "mid"):
        L = Dt[lod]
        n0, t0 = len(L["points"]) // 3, len(L["triangles"]) // 3
        Pm_end, Fm_end, dark, nm = done[lod]
        Pm0 = np.array(man_before[lod]["points"], float).reshape(-1, 3)
        if lod == "near":
            fro = np.ones(n0, bool)                   # (the twin's own points past the man's: caps, never split)
            fro[:len(frozen)] = frozen
            tint_before = list(L["tint"])
            P, F, T, at, tint, _ = _refine(L, fields["near"], _arm_weight(L, Dt["bones"]), fro)
        else:
            P = np.array(L["points"], float).reshape(-1, 3)
            F = np.array(L["facings"], float).reshape(-1, 3)
            T = None
        k_new = len(P) - n0
        if k_new != len(Pm_end) - nm:
            raise SystemExit(f"twin {lod}: {k_new} new points, the man {len(Pm_end) - nm}: not split alike")
        idx_t = np.concatenate([np.arange(nm), np.arange(n0, n0 + k_new)])
        idx_m = np.concatenate([np.arange(nm), np.arange(nm, nm + k_new)])
        own = np.arange(nm, n0)
        moved = Pm_end[:nm] - Pm0
        if len(own):
            _, near_i = cKDTree(Pm0).query(P[own], k=1)
            P[own] = P[own] + moved[near_i]
        P[idx_t] = Pm_end[idx_m]
        F[idx_t] = Fm_end[idx_m]
        if lod == "near":
            for key in ("shade", "shade_clean"):
                vals = at[key]
                for it, im in zip(idx_t, idx_m):
                    if dark[im] < 1:
                        vals[it] = round(float(vals[it] * dark[im]), 3)
            _write_lod(L, P, F, T, at, tint)
            _fd_keep(Dt, "near", tint_before)
        else:
            _write_lod(L, P, F)
        rep[lod] = {"points_added": int(k_new), "triangles_added": int((len(T) if T is not None else t0) - t0),
                    "own_points_moved": int((np.linalg.norm(moved[near_i], axis=1) > 1e-6).sum()) if len(own) else 0}
    _marker(Dt, rep)
    return rep


def _fd_keep(D, lod, before):
    """face_detail.py's marker kept in step with the tint array (see soldier_detail_export._fd_keep)."""
    import soldier_detail_export as X
    return X._fd_keep(D, lod, before)


def fields_of(D):
    """The waist fields, read from the man's own tunic at each level (the twin is mended with the man's)."""
    out = {}
    for lod in ("near", "mid"):
        L = D[lod]
        P = np.array(L["points"], float).reshape(-1, 3)
        armw = _arm_weight(L, D["bones"])
        torso = ((np.array(L["paints"]) == 0) & (armw < 0.05) & (P[:, 2] > 0.74) & (P[:, 2] < 1.34)
                 & (np.abs(P[:, 1]) < 0.34))
        out[lod] = Field(P, torso)
    out["mid"] = out["near"]          # (one field for both levels, so they draw the same waist)
    out["near"].belt = _belt_top(D, out["near"])
    return out


def _belt_top(D, field):
    """The belt's top edge at his back, as a height for each place across him (y): the highest point of the belt (the
    straps' paint in the long pieces that go round him at the waist) in each centimetre across, smoothed."""
    L = D["near"]
    P = np.array(L["points"], float).reshape(-1, 3)
    T = np.array(L["triangles"]).reshape(-1, 3)
    lab = _islands(T, len(P))
    pa = np.array(L["paints"])
    belt = np.zeros(len(P), bool)
    for i in np.unique(lab[(pa == 4) & (P[:, 2] > 1.0) & (P[:, 2] < 1.12)]):
        m = lab == i
        if np.ptp(P[m, 1]) > 0.3 and np.ptp(P[m, 2]) < 0.1:
            belt |= m
    cx = field.at(P)[0]
    q = P[belt & (P[:, 0] < cx)]
    ys = np.arange(-0.20, 0.205, 0.01)
    zt = np.array([q[np.abs(q[:, 1] - y) < 0.01, 2].max() if (np.abs(q[:, 1] - y) < 0.01).any() else np.nan for y in ys])
    ok = ~np.isnan(zt)
    zt = np.interp(ys, ys[ok], zt[ok])
    zt = np.convolve(np.pad(zt, 2, mode="edge"), np.ones(5) / 5, mode="valid")
    return ys, zt


def mend_pair(man, twin):
    """Both files, as dicts, mended in place; returns the reports."""
    import copy
    fields = fields_of(man)
    frozen = _frozen(man, twin, "near")
    man_before = {lod: copy.deepcopy(man[lod]) for lod in ("near", "mid")}
    rep_man, done = mend_man(man, frozen, fields)
    rep_twin = mend_twin(twin, man_before, done, frozen, fields)
    return rep_man, rep_twin


def check(before, after, twin_before, twin_after):
    """What must hold (raises on a fault): nothing outside the trunk moved; pack, helmet, face and gore numbers kept;
    every old triangle still there or split; no thin or empty triangle; the twin's new points the man's."""
    errs = []
    for lod in ("near", "mid", "far"):
        a, b = before[lod], after[lod]
        n = len(a["points"]) // 3
        P0 = np.array(a["points"]).reshape(-1, 3)
        P1 = np.array(b["points"]).reshape(-1, 3)[:n]
        mv = np.linalg.norm(P1 - P0, axis=1)
        out = (P0[:, 2] < 0.73) | (P0[:, 2] > 1.35)
        if (mv[out] > 1e-6).any():
            errs.append(f"{lod}: {int((mv[out] > 1e-6).sum())} points moved outside the trunk")
        if mv.max() > 0.025:
            errs.append(f"{lod}: a point moved {mv.max() * 100:.1f} cm")
        for k in ("paints", "part", "part2", "w2", "dirt", "dirt_wet", "wet", "snow"):
            if list(a[k]) != list(b[k])[:n]:
                errs.append(f"{lod}: {k} of an old point changed")
        if lod == "far" and a != b:
            errs.append("far changed")
        T1 = np.array(b["triangles"]).reshape(-1, 3)
        A = np.linalg.norm(np.cross(P1[0:0].reshape(0, 3), P1[0:0].reshape(0, 3)), axis=1) if False else None
        Pall = np.array(b["points"]).reshape(-1, 3)
        area = 0.5 * np.linalg.norm(np.cross(Pall[T1[:, 1]] - Pall[T1[:, 0]], Pall[T1[:, 2]] - Pall[T1[:, 0]]), axis=1)
        T0 = np.array(a["triangles"]).reshape(-1, 3)
        new = np.arange(len(T1)) >= len(T0)
        changed = np.zeros(len(T1), bool)
        changed[:len(T0)] = (T1[:len(T0)] != T0).any(1)
        if (area[new | changed] < 1e-8).any():
            errs.append(f"{lod}: {int((area[new | changed] < 1e-8).sum())} empty triangles")
        for key in ("pack_tris", "helmet_tris"):
            s, c = before[key][lod]
            if (T1[s:s + c] != T0[s:s + c]).any():
                errs.append(f"{lod}: {key} triangles changed")
        # (the cloth kept whole: every edge used once or twice, as before, and no new open edge)
        def open_edges(T):
            E = np.sort(np.concatenate([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]]), 1)
            _, c = np.unique(E, axis=0, return_counts=True)
            return int((c == 1).sum())
        if lod == "near":
            Pb = np.array(b["points"]).reshape(-1, 3)

            def open_length(T, P):
                E = np.sort(np.concatenate([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]]), 1)
                u, c = np.unique(E, axis=0, return_counts=True)
                o = u[c == 1]
                return float(np.linalg.norm(P[o[:, 0]] - P[o[:, 1]], axis=1).sum())
            l0, l1 = open_length(T0, P0), open_length(T1, Pb)
            if abs(l1 - l0) > 0.02 * l0 + 0.05:
                errs.append(f"near: open edges {l0:.2f} m before, {l1:.2f} m after (a crack?)")
    if twin_before is not None:
        nm, nt = len(before["near"]["points"]) // 3, len(twin_before["near"]["points"]) // 3
        a = np.array(after["near"]["points"]).reshape(-1, 3)
        t = np.array(twin_after["near"]["points"]).reshape(-1, 3)
        if len(a) - nm != len(t) - nt:
            errs.append(f"twin: {len(t) - nt} new points, the man {len(a) - nm}")
        elif len(a) > nm and np.abs(a[nm:] - t[nt:]).max() > 2e-4:
            errs.append(f"twin: new points differ from the man's by {np.abs(a[nm:] - t[nt:]).max() * 1000:.1f} mm")
        same0 = np.abs(np.array(before["near"]["points"]).reshape(-1, 3) - np.array(twin_before["near"]["points"]).reshape(-1, 3)[:nm]).max(1) < 1e-9
        d = np.abs(a[:nm] - t[:nm]).max(1)
        if (d[same0] > 2e-4).any():
            errs.append(f"twin: {int((d[same0] > 2e-4).sum())} shared points moved differently from the man's")
        g0, g1 = twin_before.get("gore"), twin_after.get("gore")
        if g0 != g1:
            errs.append("twin: gore cuts changed")
    if errs:
        raise SystemExit("waist check FAILED:\n  " + "\n  ".join(errs))
    return "waist check: ok"


def main():
    a = sys.argv
    if "--mend" in a:
        i = a.index("--mend")
        src_m, src_t, out_m, out_t = a[i + 1:i + 5]
    elif "--deliver" in a:
        src_m, src_t = SHARED + NAME + ".json", SHARED + NAME + "_gore.json"
        out_m, out_t = src_m, src_t
    else:
        raise SystemExit(__doc__)
    import copy
    man, twin = json.load(open(src_m)), json.load(open(src_t))
    man0, twin0 = copy.deepcopy(man), copy.deepcopy(twin)
    rm, rt = mend_pair(man, twin)
    print("man:", json.dumps(rm))
    print("twin:", json.dumps(rt))
    print(check(man0, man, twin0, twin))
    if "--deliver" in a:
        import soldier_detail_export as X
        h_m, h_t = X._sha(src_m), X._sha(src_t)
        X._put(out_m, man, h_m, "waist")
        X._put(out_t, twin, h_t, "waist (gore twin)")
    else:
        for p, D in ((out_m, man), (out_t, twin)):
            tmp = p + ".tmp"
            with open(tmp, "w") as f:
                f.write(json.dumps(D, separators=(",", ":"), allow_nan=False))
            os.replace(tmp, p)
            print("wrote", p)


if __name__ == "__main__":
    main()
