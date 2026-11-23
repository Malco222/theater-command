"""Checks a staged soldier file against the file it was made from (face_mesh.py then face_detail.py, version 2):
mid and far change only in tint; at near the old points keep their place except the head's and neck's skin (lids, lips,
between the brows, folds eased, tucked under the headgear: at most 12 mm; the ears 2 mm at most), every old per-point list keeps its values except shade and facings on
that skin and the eyes; only skin and eye triangles are split; no index past the points; the new triangles face out;
no fold bigger than half a square millimetre a camera could see on the face (off the ears); no skin seen through the helmet or cap; the faces' lists are whole.

    python validate_faces.py STAGE_DIR ORIGINALS_DIR
"""
import json, sys, glob, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from poke import file_poking, bvh_of, VIEWS  # noqa: E402
from mathutils import Vector  # noqa: E402
STAGE, ORIG = sys.argv[1], sys.argv[2]
bad = 0
for f in sorted(glob.glob(os.path.join(STAGE, "soldier_*.json"))):
    b = os.path.basename(f); A = json.load(open(os.path.join(ORIG, b))); B = json.load(open(f))
    probs = []
    fm = B["face_mesh"]; n0, t0 = fm["first_new_point"], fm["first_new_triangle"]
    names = [x["name"] for x in A["bones"]]
    hb, nb = names.index("head"), names.index("neck") if "neck" in names else names.index("head")
    for lod in ("near", "mid", "far"):
        a, c = A[lod], B[lod]
        n = len(c["points"]) // 3
        for k, v in c.items():
            if k == "triangles": continue
            if len(v) not in (n, 3 * n): probs.append(f"{lod}.{k} length")
        T = np.array(c["triangles"])
        if T.max() >= n or T.min() < 0: probs.append(f"{lod} triangle index")
        if lod != "near":
            for k in a:
                if k not in ("tint",) and a[k] != c[k]: probs.append(f"{lod}.{k} changed")
            tt = np.array(c["tint"])
            if tt.min() < 0.05 or tt.max() > 1.6: probs.append(f"{lod} tint out of range {tt.min():.2f}..{tt.max():.2f}")
        else:
            pa0 = np.array(a["paints"]); part0 = np.array(a["part"])
            soft = (pa0 == 7) & np.isin(part0, (hb, nb))
            Pa, Pb = np.array(a["points"]).reshape(-1, 3), np.array(c["points"][:3 * n0]).reshape(-1, 3)
            dm = np.linalg.norm(Pa - Pb, axis=1)
            if dm[~soft].max() > 1e-6: probs.append(f"near old points moved off the face: {int((dm[~soft] > 1e-6).sum())}")
            if dm[soft].max() > 0.012: probs.append(f"near face point moved {1000 * dm[soft].max():.1f} mm")
            eyes = (part0 == hb) & (pa0 == 2)
            ex, ey = Pa[eyes, 0].mean(), np.abs(Pa[eyes, 1]).mean()
            ear = soft & (np.abs(Pa[:, 1]) > ey + 0.028) & (Pa[:, 0] < ex - 0.022)
            if dm[ear].max() > 0.002: probs.append(f"an ear point moved {1000 * dm[ear].max():.1f} mm")
            seen, _ = file_poking(B)
            if len(seen): probs.append(f"{len(seen)} skin points seen through the headgear")
            for k in ("part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow", "facings"):
                if k not in a: continue
                va, vb = np.array(a[k], float), np.array(c[k][:len(a[k])], float)
                w = len(va) // len(pa0)
                d = np.abs(va - vb).reshape(len(pa0), w).max(1)
                if k in ("shade", "shade_clean"): d[soft] = 0; d[pa0 == 2] = 0
                if k == "facings": d[pa0 == 7] = 0; d[pa0 == 2] = 0
                if d.max() > 2e-4: probs.append(f"near old {k} changed {d.max():.4f} at {int((d > 2e-4).sum())}")
            if c["paints"][:n0] != a["paints"]:
                ch_p = [i for i in range(n0) if c["paints"][i] != a["paints"][i]]
                if any(a["paints"][i] != 2 for i in ch_p): probs.append("a paint other than the eyes changed")
            Ta, Tb = np.array(a["triangles"]).reshape(-1, 3), np.array(c["triangles"]).reshape(-1, 3)[:t0]
            ch = np.any(Ta != Tb, 1)
            if not np.all(np.isin(pa0[Ta[ch]], (7, 2))): probs.append("a split triangle was not skin or eye")
            if "helmet_tris" in A:
                h0, hc = A["helmet_tris"]["near"]
                if ch[h0:h0 + hc].any(): probs.append("helmet triangle split")
            P = np.array(c["points"]).reshape(-1, 3); N = np.array(c["facings"]).reshape(-1, 3)
            Tn = np.array(c["triangles"]).reshape(-1, 3)
            G = -np.cross(P[Tn[t0:, 1]] - P[Tn[t0:, 0]], P[Tn[t0:, 2]] - P[Tn[t0:, 0]])
            agree = (np.einsum("ij,ij->i", G, N[Tn[t0:]].mean(1)) > 0).mean()
            if agree < 0.97: probs.append(f"new triangles face in: only {agree:.3f} agree")
            # folds left on the head's skin
            pb = np.array(c["paints"]); partb = np.array(c["part"])
            st = ((pb == 7) & (partb == hb))[Tn].all(1)
            Fn = -np.cross(P[Tn[:, 1]] - P[Tn[:, 0]], P[Tn[:, 2]] - P[Tn[:, 0]])
            area = np.linalg.norm(Fn, axis=1) / 2
            Nv = np.zeros_like(P)
            for j in range(3): np.add.at(Nv, Tn[st, j], Fn[st])
            Nv /= np.linalg.norm(Nv, axis=1, keepdims=True) + 1e-12
            dots = np.einsum("ij,ij->i", Fn / (2 * area[:, None] + 1e-15), Nv[Tn].mean(1))
            # (a fold counts where a camera could see it, off the ears: an ear's curl is folds by nature)
            Cc = P[Tn].mean(1)
            off_ear = ~((np.abs(Cc[:, 1]) > ey + 0.028) & (Cc[:, 0] < ex - 0.022))
            big = np.where(st & (dots < 0.0) & (area > 5e-7) & off_ear)[0]
            allb = bvh_of(P, Tn)
            seen_folds = [t for t in big if any(allb.ray_cast(Vector(Cc[t] + v * 3e-4), Vector(v), 1.0)[0] is None for v in VIEWS)]
            if seen_folds: probs.append(f"{len(seen_folds)} folds over 0.5 mm2 to be seen on the face")
            tt = np.array(c["tint"])
            if tt.min() < 0.05 or tt.max() > 1.6: probs.append(f"near tint out of range {tt.min():.2f}..{tt.max():.2f}")
    if B.get("helmet_tris") != A.get("helmet_tris"): probs.append("helmet_tris changed")
    for F, Fa in zip(B.get("faces", []), A.get("faces", [])):
        if F["far"] != Fa["far"]: probs.append(f"face {F['name']} far changed")
        if F["mid"] != Fa["mid"]: probs.append(f"face {F['name']} mid changed")
        nn = len(B["near"]["points"]) // 3
        L = F["near"]
        if any(i >= nn for i in L.get("at", []) + L.get("tint_at", [])): probs.append("face index past points")
        if len(L.get("move", [])) != 3 * len(L.get("at", [])) or len(L.get("tint", [])) != 3 * len(L.get("tint_at", [])): probs.append("face list lengths")
        if len(set(L.get("tint_at", []))) != len(L.get("tint_at", [])): probs.append("face tint_at repeats a point")
    for k in A:
        if k not in ("near", "mid", "far", "faces", "new_paints", "colours") and A[k] != B[k]: probs.append(f"top {k} changed")
    if B.get("face_detail", {}).get("version") != 2: probs.append("face_detail not version 2")
    bad += bool(probs)
    print(b, "OK" if not probs else probs[:6], "near tris %d->%d pts %d->%d" % (t0, len(B["near"]["triangles"]) // 3, n0, len(B["near"]["points"]) // 3))
print("files with problems:", bad)
