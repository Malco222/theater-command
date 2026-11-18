"""Sculpting by distance fields, for the battle units' soft shapes (a man's body, a rifle's stock, a horse).

A shape is said as a field: at each point in space, how far it is to the surface (negative inside). Plain shapes
(balls, eggs, tapered limbs, rounded boxes, rings, flared tubes) are joined with a blend of so many metres, so a limb
grows out of the body the way flesh and cloth do instead of meeting it at a seam. One field per cloth (`Layer`);
the layers are joined hard, so where the tunic's skirt hangs over the trousers there is a true hem.

The field is sampled on a grid and turned into a closed quad mesh by surface nets (`surface_nets`), which the
caller then thins to its triangle budget with Blender's Decimate. numpy only: nothing to install.

Coordinates are Blender's: metres, x forward, y to the LEFT, z up.
"""
import math

import numpy as np

BIG = 1.0e3


def _v(a):
    return np.asarray(a, dtype=np.float64)


def frame_from(x_axis, up_hint=(0.0, 0.0, 1.0)):
    """A rotation matrix (columns = local x, y, z in world) whose x is `x_axis` and whose z leans toward `up_hint`."""
    x = _v(x_axis)
    x = x / np.linalg.norm(x)
    u = _v(up_hint)
    y = np.cross(u, x)
    if np.linalg.norm(y) < 1e-6:
        y = np.cross((0.0, 1.0, 0.0), x)
    y /= np.linalg.norm(y)
    z = np.cross(x, y)
    return np.stack([x, y, z], axis=1)


def rot_euler(rx=0.0, ry=0.0, rz=0.0):
    """Degrees, applied X then Y then Z (as Blender's XYZ euler)."""
    a, b, c = (math.radians(v) for v in (rx, ry, rz))
    X = np.array([[1, 0, 0], [0, math.cos(a), -math.sin(a)], [0, math.sin(a), math.cos(a)]])
    Y = np.array([[math.cos(b), 0, math.sin(b)], [0, 1, 0], [-math.sin(b), 0, math.cos(b)]])
    Z = np.array([[math.cos(c), -math.sin(c), 0], [math.sin(c), math.cos(c), 0], [0, 0, 1]])
    return Z @ Y @ X


class Prim:
    """A shape that knows its distance field and the box outside which it is far away."""
    lo = hi = None

    def local(self, px, py, pz):
        c, R = self.c, self.R
        dx, dy, dz = px - c[0], py - c[1], pz - c[2]
        if R is None:
            return dx, dy, dz
        # q = R^T d
        return (R[0, 0] * dx + R[1, 0] * dy + R[2, 0] * dz,
                R[0, 1] * dx + R[1, 1] * dy + R[2, 1] * dz,
                R[0, 2] * dx + R[1, 2] * dy + R[2, 2] * dz)

    def box_round(self, reach):
        """World bounding box of a local box of half size `reach` (a 3-vector) round c, turned by R."""
        reach = _v(reach)
        if self.R is None:
            ext = reach
        else:
            ext = np.abs(self.R) @ reach
        self.lo, self.hi = self.c - ext, self.c + ext


class Sphere(Prim):
    def __init__(self, c, r):
        self.c, self.r, self.R = _v(c), float(r), None
        self.box_round((r, r, r))

    def d(self, px, py, pz):
        return np.sqrt((px - self.c[0]) ** 2 + (py - self.c[1]) ** 2 + (pz - self.c[2]) ** 2) - self.r


class Ellipsoid(Prim):
    def __init__(self, c, r, R=None):
        self.c, self.r, self.R = _v(c), _v(r), (None if R is None else _v(R))
        self.box_round(self.r)

    def d(self, px, py, pz):
        x, y, z = self.local(px, py, pz)
        r = self.r
        k0 = np.sqrt((x / r[0]) ** 2 + (y / r[1]) ** 2 + (z / r[2]) ** 2)
        k1 = np.sqrt((x / r[0] ** 2) ** 2 + (y / r[1] ** 2) ** 2 + (z / r[2] ** 2) ** 2)
        return k0 * (k0 - 1.0) / np.maximum(k1, 1e-9)


class RoundCone(Prim):
    """A limb: round at each end, radius ra at a and rb at b, tapering straight between."""

    def __init__(self, a, b, ra, rb):
        self.a, self.b, self.ra, self.rb = _v(a), _v(b), float(ra), float(rb)
        self.c, self.R = (self.a + self.b) * 0.5, None
        rm = max(ra, rb)
        self.lo = np.minimum(self.a, self.b) - rm
        self.hi = np.maximum(self.a, self.b) + rm

    def d(self, px, py, pz):
        a, b, r1, r2 = self.a, self.b, self.ra, self.rb
        ba = b - a
        l2 = float(ba @ ba)
        rr = r1 - r2
        a2 = l2 - rr * rr
        il2 = 1.0 / l2
        pax, pay, paz = px - a[0], py - a[1], pz - a[2]
        y = pax * ba[0] + pay * ba[1] + paz * ba[2]
        z = y - l2
        qx, qy, qz = pax * l2 - ba[0] * y, pay * l2 - ba[1] * y, paz * l2 - ba[2] * y
        x2 = qx * qx + qy * qy + qz * qz
        y2 = y * y * l2
        z2 = z * z * l2
        k = math.copysign(1.0, rr) * rr * rr * x2
        d_end = np.sqrt(x2 + z2) * il2 - r2
        d_start = np.sqrt(x2 + y2) * il2 - r1
        d_mid = (np.sqrt(np.maximum(x2 * a2 * il2, 0.0)) + y * rr) * il2 - r1
        return np.where(np.sign(z) * a2 * z2 > k, d_end, np.where(np.sign(y) * a2 * y2 < k, d_start, d_mid))


class RoundBox(Prim):
    def __init__(self, c, half, r=0.0, R=None):
        self.c, self.half, self.rad, self.R = _v(c), _v(half), float(r), (None if R is None else _v(R))
        self.box_round(self.half)

    def d(self, px, py, pz):
        x, y, z = self.local(px, py, pz)
        h, r = self.half, self.rad
        qx, qy, qz = np.abs(x) - h[0] + r, np.abs(y) - h[1] + r, np.abs(z) - h[2] + r
        out = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2 + np.maximum(qz, 0) ** 2)
        return out + np.minimum(np.maximum(qx, np.maximum(qy, qz)), 0.0) - r


class Torus(Prim):
    """A ring round local z, of radii (major along x, major along y) and tube radius `minor`."""

    def __init__(self, c, major, minor, R=None, major_y=None):
        self.c, self.R = _v(c), (None if R is None else _v(R))
        self.mx, self.my, self.minor = float(major), float(major if major_y is None else major_y), float(minor)
        m = max(self.mx, self.my) + minor
        self.box_round((m, m, minor))

    def d(self, px, py, pz):
        x, y, z = self.local(px, py, pz)
        # (an elliptic ring: scale y to make it round, then measure)
        s = self.mx / self.my
        ring = np.sqrt(x * x + (y * s) ** 2) - self.mx
        return np.sqrt(ring * ring + z * z) - self.minor


class Tube(Prim):
    """An upright tube with an oval section that flares from (rx0, ry0) at z0 to (rx1, ry1) at z1, its rims
    rounded by `round`. For a tunic's skirt, a boot's shaft, a sleeve's cuff. R turns it (local z is its axis)."""

    def __init__(self, c, z0, z1, r0, r1, round=0.01, R=None):
        self.c, self.R = _v(c), (None if R is None else _v(R))
        self.z0, self.z1, self.r0, self.r1, self.rnd = float(z0), float(z1), _v(r0), _v(r1), float(round)
        m = max(self.r0.max(), self.r1.max())
        self.box_round((m, m, max(abs(z0), abs(z1))))

    def d(self, px, py, pz):
        x, y, z = self.local(px, py, pz)
        t = np.clip((z - self.z0) / (self.z1 - self.z0), 0.0, 1.0)
        rx = self.r0[0] + (self.r1[0] - self.r0[0]) * t
        ry = self.r0[1] + (self.r1[1] - self.r0[1]) * t
        rad = np.sqrt((x / rx) ** 2 + (y / ry) ** 2)
        dr = (rad - 1.0) * np.minimum(rx, ry)
        hz = (self.z1 - self.z0) * 0.5
        dz = np.abs(z - (self.z0 + self.z1) * 0.5) - hz
        r = self.rnd
        qx, qz = dr + r, dz + r
        return np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qz, 0) ** 2) + np.minimum(np.maximum(qx, qz), 0.0) - r


class HalfSpace(Prim):
    """Everything on the far side of a plane through c from `n` (n points out of the solid)."""

    def __init__(self, c, n):
        self.c, self.n, self.R = _v(c), _v(n) / np.linalg.norm(n), None
        self.lo = self.hi = None   # (it reaches everywhere)

    def d(self, px, py, pz):
        return (px - self.c[0]) * self.n[0] + (py - self.c[1]) * self.n[1] + (pz - self.c[2]) * self.n[2]


def smin(a, b, k):
    if k <= 0.0:
        return np.minimum(a, b)
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b + (a - b) * h - k * h * (1.0 - h)


def smax(a, b, k):
    return -smin(-a, -b, k)


class Layer:
    """One cloth (or the skin): shapes joined smoothly, cut by others. Each op is (how, shape, k):
    'add' joins with a blend of k metres, 'sub' carves, 'cut' keeps only what lies inside (an intersection)."""

    def __init__(self, name, paint):
        self.name, self.paint, self.ops = name, paint, []

    def add(self, p, k=0.0):
        self.ops.append(("add", p, k))
        return p

    def sub(self, p, k=0.0):
        self.ops.append(("sub", p, k))
        return p

    def cut(self, p, k=0.0):
        self.ops.append(("cut", p, k))
        return p

    def bounds(self):
        los = [p.lo for how, p, k in self.ops if how == "add"]
        his = [p.hi for how, p, k in self.ops if how == "add"]
        return np.min(los, axis=0), np.max(his, axis=0)

    def at_points(self, P):
        """Distance at an (N, 3) array of points."""
        px, py, pz = P[:, 0], P[:, 1], P[:, 2]
        d = np.full(len(P), BIG)
        for how, p, k in self.ops:
            v = p.d(px, py, pz)
            d = smin(d, v, k) if how == "add" else smax(d, -v, k) if how == "sub" else smax(d, v, k)
        return d


class Grid:
    def __init__(self, lo, hi, h):
        self.h = float(h)
        self.lo = _v(lo) - 2 * h
        n = np.ceil((_v(hi) + 2 * h - self.lo) / h).astype(int) + 1
        self.n = n
        self.x = (self.lo[0] + np.arange(n[0]) * h).astype(np.float32)
        self.y = (self.lo[1] + np.arange(n[1]) * h).astype(np.float32)
        self.z = (self.lo[2] + np.arange(n[2]) * h).astype(np.float32)

    def span(self, lo, hi, margin):
        """Index slices of the grid that cover the box lo..hi grown by margin."""
        out = []
        for ax, arr in enumerate((self.x, self.y, self.z)):
            i0 = int(np.searchsorted(arr, lo[ax] - margin))
            i1 = int(np.searchsorted(arr, hi[ax] + margin)) + 1
            out.append(slice(max(i0, 0), min(i1, len(arr))))
        return tuple(out)

    def layer(self, L):
        """The layer's field over the grid; each shape is worked only near where it is."""
        F = np.full(tuple(self.n), BIG, dtype=np.float32)
        margin = 3 * self.h
        for how, p, k in L.ops:
            if p.lo is None:
                sl = (slice(None),) * 3
            else:
                sl = self.span(p.lo, p.hi, margin + k)
            px = self.x[sl[0]][:, None, None]
            py = self.y[sl[1]][None, :, None]
            pz = self.z[sl[2]][None, None, :]
            v = p.d(px, py, pz).astype(np.float32)
            v = np.broadcast_to(v, F[sl].shape)
            if how == "add":
                F[sl] = smin(F[sl], v, k)
            elif how == "sub":
                F[sl] = smax(F[sl], -v, k)
            else:
                if p.lo is None:
                    F[sl] = smax(F[sl], v, k)
                else:
                    # (outside its box the cutting shape is far: everything there is cut away)
                    G = np.full_like(F, BIG)
                    G[sl] = smax(F[sl], v, k)
                    F = G
        return F


def surface_nets(F, lo, h):
    """The surface F = 0 as a quad mesh (naive surface nets): one point in each cell the surface crosses, at the mean
    of its crossings, and a quad across each crossed edge of the grid. Returns (verts (N,3), quads (M,4))."""
    nx, ny, nz = F.shape
    inside = F < 0
    S = np.zeros((nx - 1, ny - 1, nz - 1, 3), np.float32)
    C = np.zeros((nx - 1, ny - 1, nz - 1), np.float32)
    for ax in range(3):
        a = np.take(F, np.arange(F.shape[ax] - 1), axis=ax)
        b = np.take(F, np.arange(1, F.shape[ax]), axis=ax)
        cross = (a < 0) != (b < 0)
        t = np.where(cross, a / np.where(cross, a - b, 1.0), 0.0).astype(np.float32)
        idx = np.indices(a.shape, dtype=np.float32)
        pos = np.stack(idx, axis=-1)
        pos[..., ax] += t
        pos *= cross[..., None]
        cnt = cross.astype(np.float32)
        # each edge along `ax` touches the four cells round it in the other two axes
        o1, o2 = [d for d in range(3) if d != ax]
        for d1 in (0, 1):
            for d2 in (0, 1):
                src = [slice(None)] * 3
                src[o1] = slice(d1, d1 + (F.shape[o1] - 1))
                src[o2] = slice(d2, d2 + (F.shape[o2] - 1))
                S += pos[tuple(src)]
                C += cnt[tuple(src)]
    has = C > 0
    index = -np.ones(C.shape, np.int64)
    index[has] = np.arange(int(has.sum()))
    verts = S[has] / C[has][:, None]
    verts = verts.astype(np.float64) * h + lo
    quads = []
    for ax in range(3):
        a = np.take(F, np.arange(F.shape[ax] - 1), axis=ax)
        b = np.take(F, np.arange(1, F.shape[ax]), axis=ax)
        o1, o2 = [d for d in range(3) if d != ax]
        cross = (a < 0) != (b < 0)
        # keep edges whose four neighbouring cells exist
        sl = [slice(None)] * 3
        sl[o1] = slice(1, F.shape[o1] - 1)
        sl[o2] = slice(1, F.shape[o2] - 1)
        cr = cross[tuple(sl)]
        e = np.argwhere(cr)
        e[:, o1] += 1
        e[:, o2] += 1
        out_up = (a[tuple(e.T)] < 0)          # inside below, outside above along ax: the face looks along +ax
        def cell(d1, d2):
            c = e.copy()
            c[:, o1] -= d1
            c[:, o2] -= d2
            return index[c[:, 0], c[:, 1], c[:, 2]]
        q = np.stack([cell(1, 1), cell(0, 1), cell(0, 0), cell(1, 0)], axis=1)
        # (o1, o2, ax) is a right-handed order for ax = 0 and 2 and left-handed for ax = 1
        flip = ~out_up if ax != 1 else out_up
        q[flip] = q[flip][:, ::-1]
        quads.append(q)
    quads = np.concatenate(quads)
    quads = quads[(quads >= 0).all(axis=1)]
    return verts, quads


def on_surface(L, start, way, steps=60):
    """Walk from `start` along `way` until the layer's surface is met (sphere tracing). Returns (point, outward normal)."""
    p = np.array(start, dtype=float)
    w = np.array(way, dtype=float)
    w /= np.linalg.norm(w)
    for _ in range(steps):
        d = float(L.at_points(p[None, :])[0])
        if abs(d) < 1e-5:
            break
        p = p + w * d
    e = 1e-4
    g = np.array([(L.at_points((p + np.eye(3)[i] * e)[None, :])[0] - L.at_points((p - np.eye(3)[i] * e)[None, :])[0]) for i in range(3)])
    return p, g / np.linalg.norm(g)
