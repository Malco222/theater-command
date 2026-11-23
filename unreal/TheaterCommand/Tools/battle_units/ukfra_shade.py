"""Bake the wear arrays onto make_ukfra.py's soldier_UK.json and soldier_FRA.json (shade.py, as for every soldier file), then light
the brim's underside a little (the art director, 23 November): a wide brim's underside bakes as dark as a hollow, and in
the game it read as a black disc round every head. Points under the brim (facing down, out past the dome) are given
at least UNDER_SHADE in shade and shade_clean, in the man's own helmet and in every finish."""
import json, os, sys, time
import numpy as np
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import shade
# Usage: python ukfra_shade.py OUTDIR soldier_UK soldier_FRA   (OUTDIR is where make_ukfra.py wrote them; the stage and
# backup folders are made beside it)
OUT = os.path.abspath(sys.argv[1])
shade.SHARED = OUT + "/"
shade.STAGE = os.path.join(os.path.dirname(OUT), "stage/")
shade.BACKUP = os.path.join(os.path.dirname(OUT), "backup/")
UNDER_SHADE = 0.85
LODS = ("near", "mid", "far")


def light_under(m, idx, axis):
    P = np.array(m["points"]).reshape(-1, 3)[idx]
    N = np.array(m["facings"]).reshape(-1, 3)[idx]
    r = np.hypot(P[:, 0] - axis[0], P[:, 1] - axis[1])
    under = idx[(N[:, 2] < -0.5) & (r > 0.095)]
    lit = np.array(m["points"]).reshape(-1, 3)[under]
    for key in ("shade", "shade_clean"):
        if key in m:
            for i in under:
                m[key][i] = max(m[key][i], UNDER_SHADE)
    return len(under), lit


def brim_under(path):
    D = json.load(open(path))
    said = []
    for lod in LODS:
        m = D[lod]
        a, c = D["helmet_tris"][lod]
        T = np.array(m["triangles"]).reshape(-1, 3)[a:a + c]
        idx = np.unique(T)
        P = np.array(m["points"]).reshape(-1, 3)[idx]
        axis = P[:, :2].mean(0)
        n, lit = light_under(m, idx, axis)
        k = 0
        for h in D["helmets"]:
            # (in a finish, only its shell's own points lying where the man's own helmet was lit: a net, scrim or a
            # leaf hanging there keeps its own shade)
            Ph = np.array(h[lod]["points"]).reshape(-1, 3)
            if len(lit):
                d, _ = cKDTree(lit).query(Ph)
                k += light_under(h[lod], np.where(d < 0.0005)[0], axis)[0]
        said.append(f"{lod} {n} (+{k} in the finishes)")
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(D, f, separators=(",", ":"))
    json.load(open(tmp))
    os.replace(tmp, path)
    print("   brim's underside lit to", UNDER_SHADE, ":", ", ".join(said), flush=True)


for nm in sys.argv[2:]:
    t = time.time()
    shade.stage(nm)
    ok = shade.deliver(nm)
    print(nm, "shade baked and written:", ok, "%.0f s" % (time.time() - t), flush=True)
    brim_under(os.path.join(shade.SHARED, nm + ".json"))
