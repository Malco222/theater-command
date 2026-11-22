"""The gore twins' checks and pictures (gore.py's twin of a man against his own file).

    /home/user/bpyenv/bin/python gore_check.py soldier_USA [--only numbers,eyes,picture] [--samples 36]

numbers (numpy): the order kept (points, facings, paints, triangles of X first, as they were but for the band's weights
    and the seam copies), the weights valid, w2 = 0 across every cut, every clip of X posed on both (the largest shift of
    an original point and how far from the nearest joint any shift lies), each cut pulled 0.4 m away and turned (no
    triangle spans the gap, nothing else moves, both sides closed: no new open edge).
eyes (Blender's ray cast): every clip of X at four moments, seen from eight sides; how many rays first meet a cap
    (the caps' triangles: torn cloth, blood, flesh, bone) or a gore paint, on the twin.
picture: X_gore.png: the eight cuts pulled apart, a close view of a stump and its limb end, the joined man in a pose.
"""
import json
import math
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gore

ART = "/home/user/art/gore/"
PICS = os.path.join(gore.STAGE, "pictures", "gore")


def load(name):
    X = json.load(open(os.path.join(gore.SHARED, name + ".json")))
    Y = json.load(open(os.path.join(gore.STAGE, name + "_gore.json")))
    return X, Y


def arrays(M):
    return (np.array(M["points"], float).reshape(-1, 3), np.array(M["part"]), np.array(M["part2"]),
            np.array(M["w2"], float), np.array(M["triangles"]).reshape(-1, 3), np.array(M["paints"]))


def weights(Y, lod, off=0, joined=True):
    """The bones and weights to skin a level of detail with: the twin's own, or (joined) with `gore.joined` for every
    listed point none of whose cuts is in `off` (a mask of the cuts that are off)."""
    M = Y[lod]
    a, b, w = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
    if joined:
        J = Y["gore"]["joined"][lod]
        idx = np.array(J["points"], int)
        use = (np.array(J["cuts"], int) & off) == 0
        a[idx[use]] = np.array(J["part"])[use]
        b[idx[use]] = np.array(J["part2"])[use]
        w[idx[use]] = np.array(J["w2"], float)[use]
    return a, b, w


def cap_tris(Y, lod, lining=False):
    """Triangle indices of every cap (and, if asked, lining) at a level of detail."""
    out = []
    for c in Y["gore"]["cuts"]:
        for side in ("stump", "limb"):
            r = c[lod][side]
            out += list(range(r["cap_triangles"][0], r["cap_triangles"][0] + r["cap_triangles"][1]))
            if lining and "lining_triangles" in r:
                out += list(range(r["lining_triangles"][0], r["lining_triangles"][0] + r["lining_triangles"][1]))
    return np.array(sorted(out), int)


# ================================================================================================ numbers
def numbers(name, X, Y, log):
    rig = gore.Rig(X["bones"])
    rep = {}
    # same shape
    rep["bones identical"] = X["bones"] == Y["bones"]
    rep["sockets: X's first, unchanged"] = Y["sockets"][:len(X["sockets"])] == X["sockets"]
    rep["sockets added"] = [s["name"] for s in Y["sockets"][len(X["sockets"]):]]
    rep["copied as X's"] = {k: (Y.get(k) == X.get(k)) for k in ("faces", "helmets", "inks", "helmet_tris", "marks", "name",
                                                                 "high", "long", "helmet_paints", "pair") if k in X}
    rep["clips"] = Y["clips"]
    rep["paints"] = {k: Y["new_paints"].get(k) for k in gore.GORE_PAINTS}
    joints = np.array([rig.head[rig.index[c]] for c in gore.CUTS])
    per_lod = {}
    for lod in ("near", "mid", "far"):
        MX, MY = X[lod], Y[lod]
        P0, a0, b0, w0, T0, p0 = arrays(MX)
        P1, a1, b1, w1, T1, p1 = arrays(MY)
        n, m = len(P0), len(T0)
        r = {}
        r["points"] = f"{n} -> {len(P1)}"
        r["triangles"] = f"{m} -> {len(T1)}"
        r["original points, facings, paints as they were"] = bool(
            MY["points"][:3 * n] == MX["points"] and MY["facings"][:3 * n] == MX["facings"] and MY["paints"][:n] == MX["paints"])
        if "shade" in MX:
            r["shade as it was"] = MY["shade"][:n] == MX["shade"]
        chg = np.flatnonzero((a0 != a1[:n]) | (b0 != b1[:n]) | (np.abs(w0 - w1[:n]) > 1e-9))
        r["original points whose bones changed"] = int(len(chg))
        R = rig.region
        band = (R[a0] != R[b0]) & (w0 > 0)
        r["  of which in a band (two regions)"] = int(band[chg].sum())
        # triangles: the originals first, re-pointed only to a copy of the same point
        diff = T1[:m] != T0
        ok = True
        Vw = gore.weld(np.concatenate([P0, P1[n:]]))
        for t, k in zip(*np.nonzero(diff)):
            j, i = T1[t, k], T0[t, k]
            if j < n or not (np.array_equal(P1[j], P0[i]) and p1[j] == p0[i]):
                ok = False
        r["original triangles re-pointed"] = int(diff.any(1).sum())
        r["  every re-pointing to a copy of the same point (place, facing, paint)"] = ok
        # weights valid
        nb = len(rig.names)
        r["weights valid (bones in range, 0 <= w2 <= 0.5)"] = bool(
            a1.min() >= 0 and a1.max() < nb and b1.min() >= 0 and b1.max() < nb and w1.min() >= 0 and w1.max() <= 0.5)
        r["points riding the weapon, bolt or bipod that changed"] = int(np.isin(a1[chg], [rig.index.get(x, -1) for x in ("weapon", "bolt", "bipod")]).sum())
        cross = (R[a1] != R[b1]) & (w1 > 0)
        r["points with weight across a cut (w2 > 0 between two regions)"] = int(cross.sum())
        r["band points (X) now with w2 = 0"] = f"{int((w1[:n][band] == 0).sum())} of {int(band.sum())}"
        J = Y["gore"]["joined"][lod]
        ja, jb, jw = np.array(J["part"]), np.array(J["part2"]), np.array(J["w2"], float)
        r["joined: listed points"] = len(J["points"])
        r["joined: weights valid, every mask a cut's"] = bool(
            ja.min() >= 0 and ja.max() < nb and jb.min() >= 0 and jb.max() < nb and jw.min() >= 0 and jw.max() <= 0.5
            and min(J["cuts"]) > 0 and max(J["cuts"]) < 256)
        aj, bj, wj = weights(Y, lod)
        r["joined: every original point with X's own weights"] = bool(
            np.array_equal(aj[:n], a0) and np.array_equal(bj[:n][w0 > 0], b0[w0 > 0]) and np.allclose(wj[:n], w0))
        # caps, linings, copies
        r["cap triangles"] = int(len(cap_tris(Y, lod)))
        r["lining triangles"] = int(len(cap_tris(Y, lod, True)) - len(cap_tris(Y, lod)))
        r["seam copies"] = Y["gore"]["per_lod"][lod]["seam_copies"]
        # deaths thread's indices
        fa = set()
        for f in Y.get("faces", []):
            for key in ("at", "tint_at"):
                fa |= set(f[lod].get(key, []) if isinstance(f.get(lod), dict) else [])
        r["face points (variety) touched by the split"] = len(fa & set(chg.tolist()))
        if "helmet_tris" in Y:
            h0, hc = Y["helmet_tris"][lod]
            r["helmet triangles unchanged"] = bool(np.array_equal(T1[h0:h0 + hc], T0[h0:h0 + hc]))
        per_lod[lod] = r
    rep["per lod"] = per_lod
    # every clip: the shift of the original points (near, mid, far), and where
    shifts = {}
    worst = {"near": 0, "mid": 0, "far": 0}
    far_from_joint = 0.0
    worst_j = {"near": 0, "mid": 0, "far": 0}
    seam_j = {"near": 0, "mid": 0, "far": 0}
    for cname, clip in X["clips"].items():
        times = gore.clip_times(clip)
        row = {}
        for lod in ("near", "mid", "far"):
            P0, a0, b0, w0, T0, _ = arrays(X[lod])
            P1, a1, b1, w1, _, _ = arrays(Y[lod])
            aj, bj, wj = weights(Y, lod)
            src = np.array(Y["gore"]["per_lod"][lod]["seam_copies"])
            n = len(P0)
            ncp = int(src[1])
            mx, at = 0.0, None
            for t in times:
                G, Xb = gore.pose(rig, clip, t)
                A = gore.skin(P0, a0, b0, w0, G, Xb, rig.head)
                Bj = gore.skin(P1[:n + ncp], aj[:n + ncp], bj[:n + ncp], wj[:n + ncp], G, Xb, rig.head)
                worst_j[lod] = max(worst_j[lod], float(np.linalg.norm(A - Bj[:n], axis=1).max()))
                Vw = gore.weld(P1[:n + ncp])
                first = {}
                for i_, v_ in enumerate(Vw[:n].tolist()):
                    first.setdefault(v_, i_)
                cp = np.arange(n, n + ncp)
                partner = np.array([first.get(v_, -1) for v_ in Vw[n:n + ncp].tolist()])
                okp = partner >= 0
                if okp.any():
                    seam_j[lod] = max(seam_j[lod], float(np.linalg.norm(Bj[cp[okp]] - Bj[partner[okp]], axis=1).max()))
                B = gore.skin(P1[:n], a1[:n], b1[:n], w1[:n], G, Xb, rig.head)
                d = np.linalg.norm(A - B, axis=1)
                moved = d > 0.001
                if moved.any():
                    dj = np.linalg.norm(P0[moved][:, None, :] - joints[None], axis=2).min(1)
                    far_from_joint = max(far_from_joint, float(dj.max()))
                if d.max() > mx:
                    i = int(d.argmax())
                    mx, at = float(d.max()), (round(t, 3), rig.names[a0[i]], rig.names[b0[i]], round(float(w0[i]), 2))
            row[lod] = (round(mx, 4), at)
            worst[lod] = max(worst[lod], mx)
        shifts[cname] = row
    rep["clips: largest shift of an original point (m) per lod"] = {k: v for k, v in sorted(
        shifts.items(), key=lambda kv: -kv[1]["near"][0])}
    rep["clips: worst shift per lod (m)"] = {k: round(v, 4) for k, v in worst.items()}
    rep["clips, joined weights: worst shift of an original point per lod (m)"] = {k: round(v, 6) for k, v in worst_j.items()}
    rep["clips, joined weights: widest seam (a copy from its original, m)"] = {k: round(v, 6) for k, v in seam_j.items()}
    rep["clips: farthest a shifted point (>1 mm) lies from the nearest of the 8 joints (m, rest)"] = round(far_from_joint, 3)
    # pull each cut apart
    pulls = {}
    for cut in gore.CUTS:
        ci = rig.index[cut]
        limb = [i for i in range(len(rig.names)) if ci in rig.ancestors(i)]
        nbv = rig.head[rig.index[gore.NEXT[cut]]] - rig.head[ci]
        nbv /= np.linalg.norm(nbv)
        ax = np.cross(nbv, [0, 0, 1.0])
        G, Xb = gore.pose(rig, None, 0, {ci: (gore.axis_angle(ax, 40), nbv * 0.4)})
        row = {}
        for lod in ("near", "mid", "far"):
            P1, _, _, _, T1, _ = arrays(Y[lod])
            a1, b1, w1 = weights(Y, lod, off=1 << gore.CUTS.index(cut))      # this cut off, the other seven whole
            Q = gore.skin(P1, a1, b1, w1, G, Xb, rig.head)
            d = np.linalg.norm(Q - P1, axis=1)
            on_limb = np.isin(a1, limb)
            mixed = np.isin(a1, limb) != np.isin(b1, limb)
            moved = d > 1e-7
            tri_mixed = int((moved[T1].any(1) & ~moved[T1].all(1)).sum())
            others = float(d[~(on_limb | np.isin(b1, limb))].max())
            # closed: weld each side's (original-derived + caps) triangles by place and count open edges, against X's own
            lin = set(cap_tris(Y, lod, True).tolist()) - set(cap_tris(Y, lod).tolist())
            keep = np.array([t for t in range(len(T1)) if t not in lin])
            opened = {}
            for nm, sel in (("stump", ~moved[T1[keep]].any(1)), ("limb", moved[T1[keep]].all(1))):
                TT = T1[keep][sel]
                W = gore.weld(Q)[TT]
                E = {}
                for x, y, z in W:
                    for u, v in ((x, y), (y, z), (z, x)):
                        k = (min(u, v), max(u, v))
                        E[k] = E.get(k, 0) + 1
                opened[nm] = sum(1 for v in E.values() if v == 1)
            row[lod] = {"triangles spanning the gap": tri_mixed, "points with weight on both sides": int(mixed.sum()),
                        "largest move of anything not on the limb (m)": others,
                        "open edges stump / limb": f"{opened['stump']} / {opened['limb']}"}
        pulls[cut] = row
    # X's own open edges, for comparison
    base = {}
    for lod in ("near", "mid", "far"):
        P0, _, _, _, T0, _ = arrays(X[lod])
        W = gore.weld(P0)[T0]
        E = {}
        for x, y, z in W:
            for u, v in ((x, y), (y, z), (z, x)):
                k = (min(u, v), max(u, v))
                E[k] = E.get(k, 0) + 1
        base[lod] = sum(1 for v in E.values() if v == 1)
    rep["X's own open edges (kit, collar) per lod"] = base
    rep["pulled apart (0.4 m along the bone, turned 40 degrees; the other cuts whole, on joined weights)"] = pulls
    return rep


# ================================================================================================ eyes
def view_dirs():
    out = []
    for k in range(6):
        a = math.radians(30 + 60 * k)
        out.append((math.cos(a) * math.cos(math.radians(15)), math.sin(a) * math.cos(math.radians(15)), math.sin(math.radians(15))))
    out.append((0.3, 0.2, 0.93))          # from above
    out.append((0.5, -0.4, -0.77))        # from low
    return [np.array(d) / np.linalg.norm(d) for d in out]


def eyes(name, X, Y, log, step=0.25, max_frames=16, lods=("near", "mid", "far"), joined=True):
    """Every clip of X every `step` s (at most `max_frames` a clip, and its last frame), seen from eight sides: from each
    side a ray is cast at four points inside every cap triangle (its centre and halfway to each corner); a cap is seen
    where the first thing that ray meets is a cap's triangle. Counts per level of detail, and the clips where any is."""
    import bpy  # noqa: F401  (mathutils comes with it)
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    rig = gore.Rig(X["bones"])
    dirs = view_dirs()
    res = {}
    t0 = time.time()
    for lod in lods:
        P1, a1, b1, w1, T1, p1 = arrays(Y[lod])
        if joined:
            a1, b1, w1 = weights(Y, lod)
        ct = cap_tris(Y, lod)
        caps = np.zeros(len(T1), bool)
        caps[ct] = True
        lt = np.setdiff1d(cap_tris(Y, lod, True), ct)
        gpaint = np.isin(p1[T1[:, 0]], list(gore.GORE_PAINTS.values()))
        tot = {"frames": 0, "rays": 0, "cap seen": 0, "of them gore paint": 0, "wound (red)": 0, "bone": 0,
               "blood (dark)": 0, "worst": [], "worst wound": []}
        pmap = {gore.WOUND: "wound (red)", gore.BONE: "bone", gore.BLOOD: "blood (dark)"}
        ptri = p1[T1[:, 0]]
        for cname, clip in X["clips"].items():
            L = clip["long"]
            k = min(max_frames - 1, max(1, int(round(L / step))))
            ts = [L * i / k for i in range(k + 1)]
            seen = 0
            red = 0
            for t in ts:
                G, Xb = gore.pose(rig, clip, t)
                Q = gore.skin(P1, a1, b1, w1, G, Xb, rig.head)
                bvh = BVHTree.FromPolygons(Q.tolist(), T1.tolist(), all_triangles=True)
                tri = Q[T1[ct]]
                cen = tri.mean(1)
                S = np.concatenate([cen, (cen[:, None, :] + tri) / 2], axis=0) if False else \
                    np.concatenate([cen] + [(cen + tri[:, j]) / 2 for j in range(3)])
                owner = np.concatenate([ct] * 4)
                rad = float(np.linalg.norm(Q.max(0) - Q.min(0))) + 0.5
                tot["frames"] += 1
                for d in dirs:
                    O = S + d * rad
                    D = Vector((-d).tolist())
                    for o in O.tolist():
                        hit = bvh.ray_cast(Vector(o), D, 2 * rad)
                        tot["rays"] += 1
                        fi = hit[2]
                        if fi is not None and caps[fi]:
                            seen += 1
                            tot["cap seen"] += 1
                            if gpaint[fi]:
                                tot["of them gore paint"] += 1
                                tot[pmap[int(ptri[fi])]] += 1
                                red += int(ptri[fi]) == gore.WOUND
            if seen:
                tot["worst"].append((cname, seen))
            if red:
                tot["worst wound"].append((cname, red))
        tot["worst"].sort(key=lambda x: -x[1])
        tot["worst wound"].sort(key=lambda x: -x[1])
        res[lod] = tot
        log(f"  eyes {lod} ({'joined weights' if joined else 'own weights'}): {tot['frames']} frames x {len(dirs)} sides, {tot['rays']:,} rays at the caps; "
            f"cap seen {tot['cap seen']} (gore paint {tot['of them gore paint']}: wound {tot['wound (red)']}, bone "
            f"{tot['bone']}, blood {tot['blood (dark)']})  [{time.time() - t0:.0f} s]", flush=True)
    return res


# ================================================================================================ picture
PAINT_NAMES = ["tunic", "trousers", "boots", "lower", "straps", "helmet", "pack", "skin", "wood", "steel", "coat",
               "coat_dark", "leather", "blanket", "hoof"]
DEFAULT_COLOURS = {"tunic": "8A7F5C", "trousers": "5E5440", "boots": "5A3F2B", "lower": "5A4F36", "straps": "7F7556",
                   "helmet": "4C4E37", "pack": "7A7254", "skin": "BDA08E", "wood": "6E4329", "steel": "3A3C38",
                   "coat": "6A6450", "coat_dark": "4A4638", "leather": "3A2E25", "blanket": "857A58", "hoof": "2A2420"}
NATION_CLOTH = {"soldier_GER": ("5F665C", "565C52", "4A4E3C")}     # (the studio's: the GER file carries no colours)


def palette(name, Y):
    col = dict(DEFAULT_COLOURS)
    if name in NATION_CLOTH:
        col["tunic"], col["trousers"], col["helmet"] = NATION_CLOTH[name]
    col.update(Y.get("colours", {}))
    pal = {i: col[nm] for i, nm in enumerate(PAINT_NAMES)}
    for nm, i in Y.get("new_paints", {}).items():
        pal[i] = col.get(nm, "808080")
    return pal


class Studio:
    def __init__(self, name, Y, w, h, samples):
        import bpy
        import kit
        self.bpy, self.kit = bpy, kit
        kit.fresh()
        self.sc = kit.render_setup(w, h, samples)
        self.sc.cycles.use_denoising = True
        kit.studio(sun_elev=40, sun_turn=-40, strength=3.0, sky=0.6)
        g = kit.backdrop("D8D2C4")
        self.pal = palette(name, Y)
        self.mats = {}
        for i, hx in self.pal.items():
            nm = f"p{i}"
            if i == gore.WOUND:
                m = kit.material(nm, kit.hex_rgb(hx), rough=0.32, spec=0.7, noise=0.35, noise_scale=90, bump=0.6)
            elif i == gore.BLOOD:
                m = kit.material(nm, kit.hex_rgb(hx), rough=0.25, spec=0.8, noise=0.2, noise_scale=70)
            elif i == gore.BONE:
                m = kit.material(nm, kit.hex_rgb(hx), rough=0.7, noise=0.25, noise_scale=120, bump=0.4)
            elif i in (0, 1, 3, 4, 6, 10, 11, 13):
                m = kit.material(nm, kit.hex_rgb(hx), rough=0.9, sheen=0.3, noise=0.25, noise_scale=55)
            elif i == 7:
                m = kit.material(nm, kit.hex_rgb(hx), rough=0.55)
            else:
                m = kit.material(nm, kit.hex_rgb(hx), rough=0.6, metal=0.3 if i == 9 else 0.0)
            self.mats[i] = m
        self.objs = []

    def man(self, M, Q, offset=(0, 0, 0), name="man", flat=None):
        """A posed mesh (json axes) in Blender (y flipped, winding turned), a material slot per paint."""
        bpy = self.bpy
        T = np.array(M["triangles"]).reshape(-1, 3)
        pt = np.array(M["paints"])
        Pb = Q.copy()
        Pb[:, 1] *= -1
        Pb += np.array(offset)
        me = bpy.data.meshes.new(name)
        me.from_pydata(Pb.tolist(), [], T[:, [0, 2, 1]].tolist())
        me.update()
        ob = bpy.data.objects.new(name, me)
        self.sc.collection.objects.link(ob)
        tp = pt[T[:, 0]]
        used = sorted(set(tp.tolist()))
        for i in used:
            me.materials.append(self.mats.get(i) or self.kit.material(f"p{i}", (1, 0, 1)))
        slot = {p: k for k, p in enumerate(used)}
        me.polygons.foreach_set("material_index", np.array([slot[p] for p in tp], np.int32))
        me.polygons.foreach_set("use_smooth", np.ones(len(T), bool))
        me.update()
        self.objs.append(ob)
        return ob

    def clear(self):
        for ob in self.objs:
            self.bpy.data.objects.remove(ob, do_unlink=True)
        self.objs = []

    def shoot(self, path, eye, target, ortho=None, lens=50):
        from mathutils import Vector
        bpy = self.bpy
        cam = bpy.data.objects.get("gcam")
        if cam is None:
            cam = bpy.data.objects.new("gcam", bpy.data.cameras.new("gcam"))
            self.sc.collection.objects.link(cam)
        self.sc.camera = cam
        e = Vector((eye[0], -eye[1], eye[2]))
        t = Vector((target[0], -target[1], target[2]))
        cam.location = e
        cam.rotation_euler = (t - e).to_track_quat("-Z", "Y").to_euler()
        if ortho:
            cam.data.type = "ORTHO"
            cam.data.ortho_scale = ortho
        else:
            cam.data.type = "PERSP"
            cam.data.lens = lens
        cam.data.clip_start = 0.01
        self.sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        return path


def pulled(rig, cut, dist=0.4, turn=40.0, toward=None):
    """The extra on the cut bone: moved `dist` along the limb and turned (about the axis that tips its end toward
    `toward`, a direction, else about limb x up)."""
    ci = rig.index[cut]
    nbv = rig.head[rig.index[gore.NEXT[cut]]] - rig.head[ci]
    nbv /= np.linalg.norm(nbv)
    if toward is None:
        ax = np.cross(nbv, [0, 0, 1.0])
    else:
        ax = -np.cross(nbv, toward)
    return {ci: (gore.axis_angle(ax, turn), nbv * dist)}, nbv


def cut_view(rig, Y, cut):
    """Where to look at a cut pulled apart: the gap's middle, a view from the side (out from the body, a little from
    the front), leaning toward the stump's face."""
    ci = rig.index[cut]
    nbv = rig.head[rig.index[gore.NEXT[cut]]] - rig.head[ci]
    nbv /= np.linalg.norm(nbv)
    sock = next(s for s in Y["sockets"] if s["name"] == f"wound_{cut}")
    c = np.array(sock["at"])
    lateral = np.array([0.0, 1.0 if cut.endswith("_r") else -1.0, 0.0])
    if cut.startswith("thigh") or cut.startswith("calf"):
        perp = np.array([1.0, 0.35 * lateral[1], 0.0])
    else:
        perp = lateral + np.array([0.55, 0, 0])
    perp = perp - (perp @ nbv) * nbv
    perp /= np.linalg.norm(perp)
    v = perp + 0.55 * nbv + np.array([0, 0, 0.15])
    v /= np.linalg.norm(v)
    return c, nbv, v


def picture(name, X, Y, samples=36, log=print, test=False):
    from PIL import Image, ImageDraw
    import kit
    rig = gore.Rig(X["bones"])
    tmp = os.path.join(ART, "render", name)
    os.makedirs(tmp, exist_ok=True)
    os.makedirs(PICS, exist_ok=True)
    S = Studio(name, Y, 450, 300, samples)
    M = Y["near"]
    P1, a1, b1, w1, T1, _ = arrays(M)
    panels = []
    t0 = time.time()
    for cut in gore.CUTS:
        c, nbv, v = cut_view(rig, Y, cut)
        extra, _ = pulled(rig, cut, 0.4, 65.0, toward=v)
        G, Xb = gore.pose(rig, None, 0, extra)
        Q = gore.skin(P1, a1, b1, w1, G, Xb, rig.head)
        S.man(M, Q)
        g = c + nbv * 0.22
        f = S.shoot(os.path.join(tmp, f"cut_{cut}.png"), g + v * 3.0, g, ortho=0.72)
        S.clear()
        panels.append(f)
    log(f"  eight cuts rendered in {time.time() - t0:.0f} s")
    # the close view: the right leg off at the hip and the left arm at the shoulder? one stump and its limb end
    S.sc.render.resolution_x, S.sc.render.resolution_y = 900, 600
    cut = "thigh_r"
    c, nbv, v = cut_view(rig, Y, cut)
    v2 = v + np.array([0.25, 0.0, 0.25])
    v2 /= np.linalg.norm(v2)
    extra, _ = pulled(rig, cut, 0.45, 70.0, toward=v2)
    G, Xb = gore.pose(rig, None, 0, extra)
    S.man(M, gore.skin(P1, a1, b1, w1, G, Xb, rig.head))
    g = c + nbv * 0.25
    close = S.shoot(os.path.join(tmp, "close.png"), g + v2 * 2.2, g, lens=85)
    S.clear()
    # the joined man in a pose: X's own and the twin, side by side
    S.sc.render.resolution_x, S.sc.render.resolution_y = 900, 600
    clip, tt = "fire_kneel", 0.0
    G, Xb = gore.pose(rig, X["clips"][clip], tt)
    P0, a0, b0, w0, _, _ = arrays(X["near"])
    S.man(X["near"], gore.skin(P0, a0, b0, w0, G, Xb, rig.head), offset=(0, 0.55, 0), name="X")
    S.man(M, gore.skin(P1, a1, b1, w1, G, Xb, rig.head), offset=(0, -0.55, 0), name="twin")
    joined = S.shoot(os.path.join(tmp, "joined.png"), (3.6, 1.6, 1.5), (0.1, 0.0, 0.62), lens=50)
    S.clear()
    log(f"  close and joined rendered in {time.time() - t0:.0f} s")
    # the sheet
    W, H = 1800, 1250
    sheet = Image.new("RGB", (W, H), (232, 226, 214))
    d = ImageDraw.Draw(sheet)
    font = kit._font(22)
    small = kit._font(17)
    d.text((16, 10), f"{name}_gore: the gore twin (gore.py). Top: joined in {clip} (his own file left, the twin right); "
                     f"a stump and its limb end (thigh_r, pulled 0.45 m). Below: the eight cuts pulled 0.4 m apart.",
           fill=(30, 30, 30), font=font)
    sheet.paste(Image.open(joined), (0, 44))
    sheet.paste(Image.open(close), (900, 44))
    d.text((12, 50), f"{clip}: X.json (left) and X_gore.json (right)", fill=(30, 30, 30), font=small)
    d.text((912, 50), "close: right leg off at the hip; stump (left) and limb end", fill=(30, 30, 30), font=small)
    for k, f in enumerate(panels):
        x, y = 450 * (k % 4), 44 + 600 + 6 + 300 * (k // 4)
        sheet.paste(Image.open(f), (x, y))
        d.text((x + 8, y + 6), gore.CUTS[k], fill=(30, 30, 30), font=small)
    out = os.path.join(PICS, f"{name}_gore.png")
    sheet.save(out)
    log(f"  picture {out} ({time.time() - t0:.0f} s)")
    return out


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("names", nargs="+")
    ap.add_argument("--only", default="numbers,eyes,picture")
    ap.add_argument("--samples", type=int, default=36)
    ap.add_argument("--every", type=int, default=3)
    o = ap.parse_args()
    os.makedirs(os.path.join(ART, "reports"), exist_ok=True)
    for name in o.names:
        X, Y = load(name)
        if "numbers" in o.only:
            t0 = time.time()
            rep = numbers(name, X, Y, print)
            json.dump(rep, open(os.path.join(ART, "reports", name + "_numbers.json"), "w"), indent=1, default=str)
            print(json.dumps({k: v for k, v in rep.items() if not k.startswith("clips: largest")}, indent=1, default=str)[:3000])
            print(f"numbers in {time.time() - t0:.0f} s", flush=True)
        if "eyes" in o.only:
            rep = {"joined weights": eyes(name, X, Y, print),
                   "own weights (no joined)": eyes(name, X, Y, print, lods=("near",), joined=False)}
            json.dump(rep, open(os.path.join(ART, "reports", name + "_eyes.json"), "w"), indent=1, default=str)
            for mode, rr in rep.items():
                for lod, r in rr.items():
                    print(mode, lod, r["worst"][:12], flush=True)
        if "picture" in o.only:
            picture(name, X, Y, samples=o.samples)
