import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def crossings(bvh, o, p, eps=1e-5):
    """How many times the segment o->p crosses the shell's surface."""
    o = Vector(o); p = Vector(p)
    d = p - o
    L = d.length
    d.normalize()
    n, t = 0, 0.0
    while True:
        hit, nrm, idx, dist = bvh.ray_cast(o + d * (t + eps), d, L - t - eps)
        if hit is None:
            return n
        n += 1
        t = t + eps + dist


def poke(H, T, pts, centre, lift=0.0):
    """Points of the head showing through the helmet (H lifted by `lift`): beyond its outside (2 crossings from the
    head's middle) and in its steel (1). Returns (outside, in_steel)."""
    bvh = BVHTree.FromPolygons([tuple(p + np.array((0, 0, lift))) for p in H], [tuple(int(i) for i in t) for t in T])
    c = np.array([crossings(bvh, centre, p) for p in pts])
    return int((c >= 2).sum()), int((c == 1).sum())
