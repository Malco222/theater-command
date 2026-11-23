import json, sys, glob, os, numpy as np
"""Check face_mesh.py + face_detail.py output (STAGE) against the files they were made from (ORIGINALS): only near's
face grew, nothing else moved.   python validate_faces.py STAGE/ ORIGINALS/"""
S = sys.argv[2] if len(sys.argv) > 2 else "/mnt/project-files/battle-models/land/soldier/"
bad = 0
STAGE = sys.argv[1] if len(sys.argv) > 1 else "/home/user/work/faces/stage/"
for f in sorted(glob.glob(os.path.join(STAGE, "soldier_*.json"))):
    b = os.path.basename(f); A = json.load(open(S + b)); B = json.load(open(f))
    probs = []
    fm = B["face_mesh"]; n0, t0 = fm["first_new_point"], fm["first_new_triangle"]
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
        else:
            if c["points"][:3 * n0] != a["points"]: probs.append("near old points moved")
            pa0 = np.array(a["paints"])
            for k in ("part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow", "facings"):
                if k not in a: continue
                va, vb = np.array(a[k], float), np.array(c[k][:len(a[k])], float)
                w = len(va) // len(pa0)
                d = np.abs(va - vb).reshape(len(pa0), w).max(1)
                if k == "shade": d[pa0 == 2] = 0
                if k == "facings": d[pa0 == 7] = 0; d[pa0 == 2] = 0
                if d.max() > 2e-4: probs.append(f"near old {k} changed {d.max():.4f} at {int((d > 2e-4).sum())}")
            if c["paints"][:n0] != a["paints"]:
                ch_p = [i for i in range(n0) if c["paints"][i] != a["paints"][i]]
                if any(a["paints"][i] != 2 for i in ch_p): probs.append("a paint other than the eyes changed")
            Ta, Tb = np.array(a["triangles"]).reshape(-1, 3), np.array(c["triangles"]).reshape(-1, 3)[:t0]
            ch = np.any(Ta != Tb, 1)
            pa = np.array(a["paints"])
            if not np.all(np.isin(pa[Ta[ch]], (7, 2))): probs.append("a split triangle was not skin or eye")
            if "helmet_tris" in A:
                h0, hc = A["helmet_tris"]["near"]
                if ch[h0:h0 + hc].any(): probs.append("helmet triangle split")
        # winding: new triangles face out like their neighbours (facings agree)
        if lod == "near":
            P = np.array(c["points"]).reshape(-1, 3); N = np.array(c["facings"]).reshape(-1, 3)
            Tn = np.array(c["triangles"]).reshape(-1, 3)[t0:]
            G = -np.cross(P[Tn[:, 1]] - P[Tn[:, 0]], P[Tn[:, 2]] - P[Tn[:, 0]])
            agree = (np.einsum("ij,ij->i", G, N[Tn].mean(1)) > 0).mean()
            if agree < 0.97: probs.append(f"new triangles face in: only {agree:.3f} agree")
    if B.get("helmet_tris") != A.get("helmet_tris"): probs.append("helmet_tris changed")
    for F, Fa in zip(B.get("faces", []), A.get("faces", [])):
        for lod in ("mid", "far"):
            if lod == "far" and F[lod] != Fa[lod]: probs.append(f"face {F['name']} far changed")
        nn = len(B["near"]["points"]) // 3
        L = F["near"]
        if any(i >= nn for i in L.get("at", []) + L.get("tint_at", [])): probs.append("face index past points")
        if len(L.get("move", [])) != 3 * len(L.get("at", [])) or len(L.get("tint", [])) != 3 * len(L.get("tint_at", [])): probs.append("face list lengths")
    for k in A:
        if k not in ("near", "mid", "far", "faces", "new_paints", "colours") and A[k] != B[k]: probs.append(f"top {k} changed")
    bad += bool(probs)
    print(b, "OK" if not probs else probs[:6], "near tris %d->%d pts %d->%d" % (t0, len(B["near"]["triangles"]) // 3, n0, len(B["near"]["points"]) // 3))
print("files with problems:", bad)
