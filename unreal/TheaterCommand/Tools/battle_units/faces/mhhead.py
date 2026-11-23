"""MakeHuman's full-resolution man for a soldier file's face 0 (as variety.py's Man), in the file's frame (y right)."""
import json, os, sys
import numpy as np
sys.path.insert(0, os.environ.get("LOOKS_TOOLS", "/home/user/work/looks/battle_units"))   # (human.py, makehuman_cc0/)
import bpy  # noqa: F401
from mathutils import Vector
from mathutils.bvhtree import BVHTree
import human

SOLE = 0.02
FILE_ARMY = {"SOVW": "SOV", "SOVS": "SOV", "ITAD": "ITA", "USAB": "USA", "USAZ": "USA", "USAL": "USA", "USAN": "USA",
             "USAM": "USA", "USAR": "USA", "USAP": "USA"}

def _n(v):
    v = np.asarray(v, float)
    return v / (np.linalg.norm(v, axis=-1, keepdims=True) + 1e-12)

class Head:
    def __init__(self, look):
        V, UV, groups = human._load_obj()
        S = human._shaped(look)
        bf = groups["body"]
        feet = min(S[i].y for f, _ in bf for i in f)
        crown = max(S[i].y for f, _ in bf for i in f)
        k = (1.75 - SOLE) / ((crown - feet) * 0.1)
        lift = Vector((0, 0, -feet * 0.1 * k + SOLE))
        P = np.array([tuple(human._bl(v, k) + lift) for v in S])
        t = []
        for fc, _ in bf:
            t.append((fc[0], fc[1], fc[2]))
            if len(fc) == 4:
                t.append((fc[0], fc[2], fc[3]))
        T = np.array(t)
        P = P * (1, -1, 1)                       # (the file's frame: y to his right)
        T = T[:, ::-1]                           # (mirroring turns the winding: keep it the same way out)
        keep = P[T].mean(1)[:, 2] > 1.4          # (the head and neck only)
        self.P, self.T = P, T[keep]
        fn = np.cross(P[self.T[:, 1]] - P[self.T[:, 0]], P[self.T[:, 2]] - P[self.T[:, 0]])
        vn = np.zeros_like(P)
        for i in range(3):
            np.add.at(vn, self.T[:, i], fn)
        self.N = _n(vn)
        self.bvh = BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in self.T])

    def nearest(self, X):
        out = []
        for p in X:
            loc, nrm, ti, d = self.bvh.find_nearest(Vector(p))
            out.append((np.array(loc), ti, d))
        return out

    def bary(self, ti, q):
        a, b, c = (self.P[j] for j in self.T[ti])
        v0, v1, v2 = b - a, c - a, q - a
        d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
        den = d00 * d11 - d01 * d01
        v = (d11 * d20 - d01 * d21) / den
        w = (d00 * d21 - d01 * d20) / den
        return np.array((1 - v - w, v, w))

    def normal_at(self, ti, q):
        return _n(self.bary(ti, q) @ self.N[self.T[ti]])

    def ray(self, o, d, most):
        """The surface along +-d from o within `most`: (point, triangle, distance) or None."""
        best = None
        for s in (1.0, -1.0):
            loc, nrm, ti, dist = self.bvh.ray_cast(Vector(o), Vector(d * s), most)
            if loc is not None and (best is None or dist < best[2]):
                best = (np.array(loc), ti, dist)
        return best

def head_for(seed, army):
    nation = army if army in human.NATION_MIX else "_default"
    return Head(human.look_for(seed, nation))
