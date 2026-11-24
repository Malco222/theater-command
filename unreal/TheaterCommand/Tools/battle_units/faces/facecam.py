"""A soldier's face as the game colours it (LandMen MakeLook): paint colour (armies.json row) x shade x (skin: the
face's tone) x the face's tint at its tint_at x the mesh's tint; the face's moves and facings applied. numpy raster."""
import json, sys, numpy as np
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from faceview import raster, tilted
from PIL import Image, ImageDraw
NAMES = ["tunic", "trousers", "boots", "lower", "straps", "helmet", "pack", "skin", "wood", "steel", "coat", "coat_dark", "leather", "blanket", "hoof"]
ARMIES = json.load(open(os.environ.get("ARMIES_JSON", "/home/user/work/helmets_ukfra/armies_nu6w43.json")))["armies"]   # (the game's armies.json)

def hexrgb(h): return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float)

def look(D, row, lod="near", k=0, helmet=True):
    m = D[lod]
    P = np.array(m["points"], float).reshape(-1, 3).copy(); N = np.array(m["facings"], float).reshape(-1, 3).copy()
    T = np.array(m["triangles"]).reshape(-1, 3); pa = np.array(m["paints"])
    pal = ARMIES[row]["paints"]
    own = {int(v): D["colours"][n] for n, v in D.get("new_paints", {}).items()} if "new_paints" in D else {}
    base = np.array([hexrgb(pal[NAMES[p]]["rgb"]) if p < 15 else hexrgb(own.get(int(p), pal["tunic"]["rgb"])) for p in pa])
    mul = np.ones((len(P), 3))
    F = D["faces"][k] if D.get("faces") else None
    if F:
        mul[pa == 7] *= np.array(F["tone"])
        L = F[lod]
        if "at" in L:
            at = np.array(L["at"]); P[at] += np.array(L["move"]).reshape(-1, 3); N[at] = np.array(L["facing"]).reshape(-1, 3)
        if L.get("tint_at"):
            ta = np.array(L["tint_at"]); mul[ta] *= np.array(L["tint"]).reshape(-1, 3)
    if "tint" in m: mul *= np.array(m["tint"]).reshape(-1, 3)
    sh = np.array(m.get("shade", [1.0] * len(P)))[:, None]
    C = np.clip(base * sh * mul, 0, 255)
    if not helmet:
        a, c = D["helmet_tris"][lod]; keep = np.ones(len(T), bool); keep[a:a + c] = False; T = T[keep]
    return P, N, T, C

def head_centre(D):
    hb = [b for b in D["bones"] if b["name"] == "head"][0]["head"]
    return (hb[0] + 0.07, hb[1], hb[2] + 0.07)

def tile(D, row, lod="near", k=0, view="front", down=0.0, size=220, zoom=1.0, text="", helmet=True, centre=None):
    P, N, T, C = look(D, row, lod, k, helmet)
    c = centre or head_centre(D)
    P, N, T, C = tilted(P, N, T, C, view, c, down)
    im = Image.fromarray(raster(P, N, T, C, view, size=size, scale=size * 3.2 * zoom, centre=c).astype(np.uint8))
    if text: ImageDraw.Draw(im).text((4, 2), text, fill=(10, 10, 10))
    return np.array(im)

if __name__ == "__main__":
    out = sys.argv[1]
    rows = []
    for spec in sys.argv[2:]:
        path, row, lod = spec.split(":")
        D = json.load(open(path))
        tiles = [tile(D, row, lod, k, v, dn, text=f"{row} {lod} face {k} {v}") for k in (0, 1, 2, 5) for v, dn in (("front", 8), ("q", 8))]
        rows.append(np.concatenate(tiles, 1))
    Image.fromarray(np.concatenate(rows, 0)).save(out, quality=90)
