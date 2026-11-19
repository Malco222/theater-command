"""Geometry for the scenery of the Asian battlefields: trunks, stems, leaves, fronds, blades, thatch, tiles, walls.

Everything is made from numbers here (no outside mesh, no picture of anyone's model). A model is a `Geo`: points,
faces, a paint for every face (by name; the game colours each paint from the model file's `colours`), and for every
point a `sway`, how far the wind moves it (0 at a trunk's foot, 1 at a frond's tip). Metres, z up, ground at z = 0,
x east and y north as Blender has them (the file writer flips y to the game's right-handed-left convention).

The same builders make three levels of detail: each takes `lod` 0 (near), 1 (mid) or 2 (far) and spends sides, rings
and leaves by it, so a palm seen from 1,500 m is a few hundred triangles and the one beside the camera a few thousand.
"""

import math

import numpy as np

TAU = 2.0 * math.pi


def unit(v):
    v = np.asarray(v, dtype=float)
    n = np.linalg.norm(v, axis=-1, keepdims=True)
    return v / np.where(n < 1e-12, 1.0, n)


def perp(d):
    """Some unit vector square to d."""
    d = unit(d)
    a = np.array((0.0, 0.0, 1.0)) if abs(d[2]) < 0.9 else np.array((1.0, 0.0, 0.0))
    return unit(np.cross(d, a))


def rot(axis, ang):
    """Rotation matrix about a unit axis."""
    x, y, z = unit(axis)
    c, s = math.cos(ang), math.sin(ang)
    C = 1 - c
    return np.array([[c + x * x * C, x * y * C - z * s, x * z * C + y * s],
                     [y * x * C + z * s, c + y * y * C, y * z * C - x * s],
                     [z * x * C - y * s, z * y * C + x * s, c + z * z * C]])


def bezier(p0, p1, p2, p3, n):
    t = np.linspace(0.0, 1.0, n)[:, None]
    return ((1 - t) ** 3) * p0 + 3 * ((1 - t) ** 2) * t * p1 + 3 * (1 - t) * t * t * p2 + t ** 3 * p3


def frames(P):
    """Parallel-transport frames along a polyline: tangents T and normals N, B (N x B = T)."""
    P = np.asarray(P, dtype=float)
    T = np.zeros_like(P)
    T[1:-1] = P[2:] - P[:-2]
    T[0] = P[1] - P[0]
    T[-1] = P[-1] - P[-2]
    T = unit(T)
    N = np.zeros_like(P)
    N[0] = perp(T[0])
    for i in range(1, len(P)):
        b = np.cross(T[i - 1], T[i])
        if np.linalg.norm(b) < 1e-9:
            N[i] = N[i - 1]
        else:
            ang = math.acos(max(-1.0, min(1.0, float(np.dot(T[i - 1], T[i])))))
            N[i] = rot(b, ang) @ N[i - 1]
    B = np.cross(T, N)
    return T, unit(N), unit(B)


class Geo:
    """A mesh under construction. paints: name -> hex, in the order first used."""

    def __init__(self, name, paints):
        self.name = name
        self.paint_hex = dict(paints)          # every paint the model may use, name -> hex (the file keeps this order)
        self.paint_names = list(self.paint_hex)
        self.V, self.S, self.F, self.Fp = [], [], [], []
        self.nv = 0
        self.two_sided = set()
        self.info = {}

    # -------------------------------------------------------------------------------- raw
    def add(self, verts, faces, paint, sway=0.0):
        """verts (n,3); faces as index lists into verts; one paint (name) for all; sway scalar or (n,)."""
        verts = np.asarray(verts, dtype=float).reshape(-1, 3)
        k = self.paint_names.index(paint)
        base = self.nv
        self.V.append(verts)
        sw = np.broadcast_to(np.asarray(sway, dtype=float), (len(verts),)).copy()
        self.S.append(np.clip(sw, 0.0, 1.0))
        for f in faces:
            self.F.append([base + int(i) for i in f])
            self.Fp.append(k)
        self.nv += len(verts)
        return base

    def add_many(self, verts, faces, paints, sway):
        """Many small pieces at once: verts (n, k, 3), faces a template over 0..k-1, paints a name per piece,
        sway (n, k)."""
        verts = np.asarray(verts, float)
        n, k = verts.shape[:2]
        base = self.nv
        self.V.append(verts.reshape(-1, 3))
        self.S.append(np.clip(np.asarray(sway, float).reshape(-1), 0, 1))
        idx = np.array([self.paint_names.index(p) for p in paints])
        for i in range(n):
            o = base + i * k
            for f in faces:
                self.F.append([o + j for j in f])
                self.Fp.append(int(idx[i]))
        self.nv += n * k

    def arrays(self):
        V = np.concatenate(self.V) if self.V else np.zeros((0, 3))
        S = np.concatenate(self.S) if self.S else np.zeros(0)
        return V, S, self.F, np.array(self.Fp, dtype=int)

    def tris(self):
        return sum(len(f) - 2 for f in self.F)

    # -------------------------------------------------------------------------------- shapes
    def tube(self, P, R, sides, paint, sway=0.0, cap0=False, cap1=True, rough=None, seed=0, flat_end=False, twist=0.0):
        """A tube along polyline P (n,3) with radius R (n,) or scalar. rough: (amp, lobes) bumps the section."""
        P = np.asarray(P, dtype=float)
        n = len(P)
        R = np.broadcast_to(np.asarray(R, dtype=float), (n,))
        sw = np.broadcast_to(np.asarray(sway, dtype=float), (n,))
        T, N, B = frames(P)
        rng = np.random.default_rng(abs(int(seed)))
        ph = rng.uniform(0, TAU, 4)
        a = np.linspace(0.0, TAU, sides, endpoint=False)
        verts, sv = [], []
        for i in range(n):
            ai = a + twist * i / max(1, n - 1)
            rr = np.full(sides, R[i])
            if rough is not None:
                amp, lobes = rough
                rr = rr * (1.0 + amp * (np.sin(lobes * ai + ph[0] + 0.7 * i) * 0.6 + np.sin((lobes + 3) * ai + ph[1] - 0.4 * i) * 0.4))
            ring = P[i] + rr[:, None] * (np.cos(ai)[:, None] * N[i] + np.sin(ai)[:, None] * B[i])
            verts.append(ring)
            sv.append(np.full(sides, sw[i]))
        verts = np.concatenate(verts)
        sv = np.concatenate(sv)
        faces = []
        for i in range(n - 1):
            for j in range(sides):
                j1 = (j + 1) % sides
                faces.append([i * sides + j, i * sides + j1, (i + 1) * sides + j1, (i + 1) * sides + j])
        extra = []
        if cap1:
            if flat_end or R[-1] > 1e-4:
                c = len(verts)
                extra.append(P[-1] + T[-1] * (0.0 if flat_end else R[-1] * 0.4))
                for j in range(sides):
                    faces.append([(n - 1) * sides + j, (n - 1) * sides + (j + 1) % sides, c])
        if cap0:
            c = len(verts) + len(extra)
            extra.append(P[0])
            for j in range(sides):
                faces.append([(j + 1) % sides, j, c])
        if extra:
            verts = np.concatenate([verts, np.array(extra)])
            sv = np.concatenate([sv, np.array([sw[-1]] * (1 if cap1 else 0) + [sw[0]] * (1 if cap0 else 0))])
        return self.add(verts, faces, paint, sv)

    def lathe(self, prof, sides, paint, at=(0, 0, 0), sway=0.0, cap_top=True, cap_bottom=False, scale_xy=(1.0, 1.0), rot_z=0.0):
        """A solid of revolution: prof is [(r, z), ...] from the bottom up."""
        prof = np.asarray(prof, dtype=float)
        a = np.linspace(0.0, TAU, sides, endpoint=False) + rot_z
        verts = []
        for r, z in prof:
            verts.append(np.stack([at[0] + r * np.cos(a) * scale_xy[0], at[1] + r * np.sin(a) * scale_xy[1], np.full(sides, at[2] + z)], 1))
        verts = np.concatenate(verts)
        n = len(prof)
        faces = [[i * sides + j, i * sides + (j + 1) % sides, (i + 1) * sides + (j + 1) % sides, (i + 1) * sides + j]
                 for i in range(n - 1) for j in range(sides)]
        extra = []
        if cap_top and prof[-1][0] > 1e-5:
            c = len(verts)
            extra.append((at[0], at[1], at[2] + prof[-1][1]))
            faces += [[(n - 1) * sides + j, (n - 1) * sides + (j + 1) % sides, c] for j in range(sides)]
        if cap_bottom and prof[0][0] > 1e-5:
            c = len(verts) + len(extra)
            extra.append((at[0], at[1], at[2] + prof[0][1]))
            faces += [[(j + 1) % sides, j, c] for j in range(sides)]
        if extra:
            verts = np.concatenate([verts, np.array(extra)])
        return self.add(verts, faces, paint, sway)

    def box(self, lo, hi, paint, sway=0.0, M=None, at=None, bottom=True):
        """An axis box from lo to hi; M (3x3) turns it about `at` (default its centre)."""
        lo, hi = np.asarray(lo, float), np.asarray(hi, float)
        c = [(lo[0], lo[1], lo[2]), (hi[0], lo[1], lo[2]), (hi[0], hi[1], lo[2]), (lo[0], hi[1], lo[2]),
             (lo[0], lo[1], hi[2]), (hi[0], lo[1], hi[2]), (hi[0], hi[1], hi[2]), (lo[0], hi[1], hi[2])]
        c = np.array(c)
        if M is not None:
            o = (lo + hi) / 2 if at is None else np.asarray(at, float)
            c = (c - o) @ np.asarray(M).T + o
        faces = [[4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]]
        if bottom:
            faces.append([3, 2, 1, 0])
        return self.add(c, faces, paint, sway)

    def prism(self, outline, z0, z1, paint, sway=0.0, top=True, bottom=False):
        """Upright walls on a closed outline [(x, y), ...] (counter-clockwise), z0 to z1."""
        o = np.asarray(outline, float)
        n = len(o)
        v = np.concatenate([np.column_stack([o, np.full(n, z0)]), np.column_stack([o, np.full(n, z1)])])
        faces = [[i, (i + 1) % n, n + (i + 1) % n, n + i] for i in range(n)]
        if top:
            faces.append(list(range(n, 2 * n)))
        if bottom:
            faces.append(list(range(n - 1, -1, -1)))
        return self.add(v, faces, paint, sway)

    def blade(self, base, direction, side, length, width, segs, paint, droop=0.0, fold=0.0, sway0=0.2, sway1=1.0,
              shape="lance", twist=0.0, tip_sway=None):
        """A leaf or grass blade: a strip from `base` along `direction`, widest by `shape`, its width across `side`.
        droop bends it down toward its tip (radians at the tip), fold creases it along the midrib (radians)."""
        if shape in ("lance", "grass", "oval"):
            segs = max(int(segs), 2)                     # (these close at both ends: one segment would have no width)
        d0, s0 = unit(direction), unit(side)
        up0 = unit(np.cross(s0, d0))
        u = np.linspace(0.0, 1.0, segs + 1)
        if shape == "lance":
            w = width * np.sin(np.pi * np.clip(u, 0, 1) ** 0.75) ** 0.85
        elif shape == "grass":
            w = width * (1.0 - u) ** 0.8 * np.clip(u * 8, 0, 1) ** 0.3
        elif shape == "oval":
            w = width * np.sin(np.pi * np.clip(u, 0, 1) ** 0.9) ** 0.6
        elif shape == "strap":
            w = width * np.clip((1.0 - u) * 5, 0, 1) ** 0.5 * np.clip(u * 10, 0.25, 1) ** 0.5
        else:
            w = width * np.ones_like(u)
        # the midrib bends down (about `side`) and twists as it goes
        pts, sides_v, ups = [], [], []
        p = np.asarray(base, float).copy()
        d, s, up = d0.copy(), s0.copy(), up0.copy()
        step = length / segs
        for i in range(segs + 1):
            pts.append(p.copy())
            sides_v.append(s.copy())
            ups.append(up.copy())
            if i < segs:
                R = rot(s, -droop / segs)
                if twist:
                    R = rot(d, twist / segs) @ R
                d, up = R @ d, R @ up
                s = unit(np.cross(d, up))
                p = p + d * step
        verts, sw = [], []
        for i in range(segs + 1):
            c = pts[i]
            wv = sides_v[i] * w[i]
            lift = ups[i] * w[i] * math.sin(fold) * 0.5
            verts += [c + wv * math.cos(fold) - lift * 0 + lift, c, c - wv * math.cos(fold) + lift]
            t = sway0 + (sway1 - sway0) * u[i] if tip_sway is None else tip_sway(u[i])
            sw += [t, t, t]
        faces = []
        for i in range(segs):
            a, b = 3 * i, 3 * (i + 1)
            faces += [[a, a + 1, b + 1, b], [a + 1, a + 2, b + 2, b + 1]]
        self.two_sided.add(paint)
        return self.add(np.array(verts), faces, paint, np.array(sw))

    def blob(self, centre, radii, paint, sub=2, sway=0.3, seed=0, rough=0.25, flat_bottom=0.0, hf=0.0, mottle=None):
        """A lumpy ellipsoid (a far crown, a bush, a stone): an icosphere pushed about by low noise, and by `hf`
        of higher noise for a cauliflower surface. mottle: [(paint, share), ...] paints faces at random."""
        V, F = icosphere(sub)
        rng = np.random.default_rng(abs(int(seed)))
        k = rng.normal(size=(10, 3))
        ph = rng.uniform(0, TAU, 10)
        bump = sum(np.sin(V @ k[i] * (1.5 + i * 0.7) + ph[i]) for i in range(6)) / 6.0
        if hf:
            bump = bump + hf / max(rough, 1e-3) * sum(np.sin(V @ k[i] * (5.0 + i * 1.3) + ph[i]) for i in range(6, 10)) / 4.0
        V = V * (1.0 + rough * bump)[:, None]
        V = V * np.asarray(radii, float)
        if flat_bottom:
            V[:, 2] = np.maximum(V[:, 2], -radii[2] * flat_bottom)
        V = V + np.asarray(centre, float)
        if not mottle:
            return self.add(V, F, paint, sway)
        base = self.add(V, [], paint, sway)
        names, shares = zip(*mottle)
        pick = rng.choice(len(names), size=len(F), p=np.array(shares) / sum(shares))
        for f, q in zip(F, pick):
            self.F.append([base + i for i in f])
            self.Fp.append(self.paint_names.index(names[q]))
        return base

    def quad_strip(self, left, right, paint, sway_l=0.0, sway_r=0.0):
        """Faces between two matching polylines."""
        L, R = np.asarray(left, float), np.asarray(right, float)
        n = len(L)
        v = np.concatenate([L, R])
        f = [[i, i + 1, n + i + 1, n + i] for i in range(n - 1)]
        sw = np.concatenate([np.broadcast_to(sway_l, (n,)), np.broadcast_to(sway_r, (n,))])
        return self.add(v, f, paint, sw)

    def grid(self, fn, nu, nv, paint, sway=0.0, wrap_u=False):
        """A surface from fn(u, v) -> (x, y, z) on a (nu+1) x (nv+1) grid (u wraps if wrap_u)."""
        us = np.linspace(0, 1, nu + (0 if wrap_u else 1), endpoint=not wrap_u)
        vs = np.linspace(0, 1, nv + 1)
        cols = len(us)
        verts = np.array([fn(u, v) for v in vs for u in us])
        faces = []
        for j in range(nv):
            for i in range(nu):
                i1 = (i + 1) % cols if wrap_u else i + 1
                faces.append([j * cols + i, j * cols + i1, (j + 1) * cols + i1, (j + 1) * cols + i])
        sw = sway(verts) if callable(sway) else sway
        return self.add(verts, faces, paint, sw)

    def transformed(self, M, t, since):
        """Turn by M and move by t every point added since point index `since`."""
        V = np.concatenate(self.V) if self.V else np.zeros((0, 3))
        V[since:] = V[since:] @ np.asarray(M).T + np.asarray(t)
        self.V = [V]
        self.S = [np.concatenate(self.S)]




def leaf_cloud(g, rng, c, r, n, L, W, paints, sway=(0.5, 0.8), up_bias=0.3, droop=0.25, shell=0.45, fold=0.18,
               tangent=0.85, under=-0.6):
    """A cloud of n leaves filling the outer part of an ellipsoid (centre c, radii r): each leaf a folded diamond
    (two triangles) lying across the surface, its tip out and a little down. paints: [(name, share), ...]."""
    if n <= 0:
        return
    c, r = np.asarray(c, float), np.asarray(r, float)
    d = unit(rng.normal(size=(n, 3)) + np.array((0, 0, up_bias)))
    low = d[:, 2] < under
    d[low, 2] *= -0.4
    d = unit(d)
    dist = 1.0 - shell * rng.random(n) ** 1.5
    base = c + r * d * dist[:, None]
    t = unit(np.cross(d, rng.normal(size=(n, 3))))
    dirn = unit(t * tangent + d * 0.5 + np.array((0, 0, -droop)))
    side = unit(np.cross(d, dirn))
    nrm = unit(np.cross(dirn, side))
    Ls = L * rng.uniform(0.75, 1.3, n)[:, None]
    Ws = W * rng.uniform(0.8, 1.25, n)[:, None]
    tip = base + dirn * Ls + np.array((0, 0, -1.0)) * Ls * droop * 0.4
    mid = base + dirn * Ls * 0.45 + nrm * Ws * fold
    left, right = mid + side * Ws * 0.5, mid - side * Ws * 0.5
    V = np.stack([base, left, tip, right], 1)
    names, shares = zip(*paints)
    pick = rng.choice(len(names), size=n, p=np.array(shares) / sum(shares))
    s0 = sway[0] + (sway[1] - sway[0]) * dist
    S = np.stack([s0, s0 + 0.1, s0 + 0.2, s0 + 0.1], 1)
    for p in names:
        g.two_sided.add(p)
    g.add_many(V, [[0, 1, 2], [0, 2, 3]], [names[i] for i in pick], S)

_ICO = {}


def icosphere(sub):
    if sub in _ICO:
        return _ICO[sub]
    t = (1.0 + 5 ** 0.5) / 2.0
    V = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
         (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    F = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6),
         (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    V = [np.array(v, float) / np.linalg.norm(v) for v in V]
    for _ in range(sub):
        cache, F2 = {}, []

        def mid(a, b):
            k = (min(a, b), max(a, b))
            if k not in cache:
                m = unit(V[a] + V[b])
                V.append(m)
                cache[k] = len(V) - 1
            return cache[k]
        for a, b, c in F:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            F2 += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        F = F2
    _ICO[sub] = (np.array(V), [list(f) for f in F])
    return _ICO[sub]
