"""Skin through the headgear: a head-skin point a camera could see (front or three-quarter, level to 25 degrees above)
with the helmet or cap right behind it along the line of sight (within 4 mm): skin come through the brim or band."""
import json, sys, numpy as np
import bpy  # noqa: F401
from mathutils import Vector
from mathutils.bvhtree import BVHTree

VIEWS = [np.array([np.cos(np.radians(p)) * np.cos(np.radians(y)), np.cos(np.radians(p)) * np.sin(np.radians(y)),
                   np.sin(np.radians(p))]) for y in (-35, 0, 35) for p in (0, 12, 25)]


def bvh_of(P, T):
    return BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in T])


def gear_mask(T, pa, part, hb, rng):
    gear = np.zeros(len(T), bool)
    if rng:
        gear[rng[0]:rng[0] + rng[1]] = True
    gear |= (part == hb)[T].all(1) & ~np.isin(pa[T], (7, 2, 4, 46, 47)).any(1)
    return gear


def poking(P, T, pa, part, hb, rng, ez, behind=0.004):
    gear = gear_mask(T, pa, part, hb, rng)
    if not gear.any():
        return np.zeros(0, int), {}
    gb = bvh_of(P, T[gear])
    allb = bvh_of(P, T)
    cand = np.where((pa == 7) & (part == hb) & (P[:, 2] > ez - 0.005))[0]
    out, how = [], {}
    for i in cand:
        o = Vector(P[i])
        if gb.find_nearest(o, 0.012)[0] is None:
            continue
        for d in VIEWS:
            v = Vector(d)
            if allb.ray_cast(o + v * 3e-4, v, 1.0)[0] is not None:
                continue                       # (hidden from this camera)
            h = gb.ray_cast(o - v * 1e-5, -v, behind)
            if h[0] is not None:
                out.append(int(i)); how[int(i)] = d
                break
    return np.array(out, int), how


def file_poking(D, lod="near"):
    m = D[lod]
    P = np.array(m["points"], float).reshape(-1, 3)
    T = np.array(m["triangles"]).reshape(-1, 3)
    pa = np.array(m["paints"]); part = np.array(m["part"])
    hb = [b["name"] for b in D["bones"]].index("head")
    e = (part == hb) & np.isin(pa, (2, 46, 47))
    return poking(P, T, pa, part, hb, (D.get("helmet_tris") or {}).get(lod), float(P[e, 2].mean()))


if __name__ == "__main__":
    for p in sys.argv[1:]:
        D = json.load(open(p))
        bad, _ = file_poking(D)
        P = np.array(D["near"]["points"]).reshape(-1, 3)
        print(p.split("/")[-2] + "/" + p.split("/")[-1], "skin seen through the headgear at near:", len(bad),
              ("y %.3f..%.3f z %.3f..%.3f" % (P[bad, 1].min(), P[bad, 1].max(), P[bad, 2].min(), P[bad, 2].max())) if len(bad) else "")
