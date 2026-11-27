"""Tank crews' kit, added to a soldier file so the man in the hatch reads as a tank commander from the game's camera.

Brian's screenshot of the Sherman's commander from above and behind (25 November 2026), the note on it "his figure is
simple": from there the man was a helmet and a plain olive back. The art director's ruling, in its order: first the
headset, because it changes his outline (two ear cups standing proud of the head); second one strap that crosses his
back on a slant (the pistol's shoulder holster worn across the body); the rest only at the near level and none of it
lighter than the cloth it sits on.

What this tool does to a soldier file, and nothing else:
- New points and triangles on the END of each level's lists (near, mid, far), every per-point list filled for them
  (facings, paints, part, part2, w2, shade, shade_clean, dirt, dirt_wet, wet, snow, tint). The old points, triangles
  and per-point values keep their values and their order.
- The one change to old values: `shade` and `shade_clean` of old points right under a new piece (within 1.5 cm) go a
  little darker. Which points and what they were is kept in the block below, so it can be put back.
- A top-level `crew_kit` block: {made_by, about, pieces, ranges, shade_touched}. Run on a file that has the block, the
  tool first takes off its own additions and puts the touched shade back, so a second run gives the same bytes.
- A new point's bones and w2 come from the body's surface under it: the cloth there is carried by its triangle's
  three corners (up to six bones), a point of the file by two, so each new point takes the two bones and the share
  that keep it nearest to where the cloth under it goes, over the keys of the crew's own clips (KITS[..]["clips"]).
- Colours: paints that are there (12 leather, 9 steel, 8 wood, 5 helmet, the jacket's own) with a dark `tint`; the
  game draws paint x shade x tint.

The library (Level, Surface, Kit: ribbons, cups, boxes, the shade bake, the checks) is general; each file's pieces are
a table of functions in KITS.

    python3 crew_kit.py --build USA_tanker [--out DIR]     the Content file with the kit added, staged in DIR
                                                            (default /home/user/art/crew_kit/out/), and its report
    python3 crew_kit.py --check USA_tanker [--out DIR]     the checks on the staged file, printed and written
    python3 crew_kit.py --deliver USA_tanker [--content]   the staged file to the shared folder (and, with --content,
                                                            to Content/Models/Soldiers), each with a backup first;
                                                            refused (exit 1, nothing written) when the staged file fails
                                                            its structural checks or a destination is not the file the
                                                            kit was built on (another tool's work is there)
    --source FILE   build on (and check against) FILE instead of the Content file: a file another tool has staged and
                    will land first (the kit is then delivered after it, on top of it)
    --no-clips      the checks without the clips (quick)
"""
import json
import math
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gore  # noqa: E402

SHARED = "/mnt/project-files/battle-models/land/soldier/"
CONTENT = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Models", "Soldiers"))
TANKS = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Models", "Battle"))
STAGE = "/home/user/art/crew_kit/out/"
BACKUP = "/home/user/art/crew_kit/backup/"
MADE_BY = "Tools/battle_units/crew_kit.py"

LEVELS = ("near", "mid", "far")
GUN_BONES = ("weapon", "bolt", "bipod")
ONE = ("paints", "part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow")   # one a point
THREE = ("points", "facings", "tint")                                                                # three a point
STANDOFF = {"near": 0.004, "mid": 0.008, "far": 0.012}       # a piece's face off the cloth, so nothing flickers
BUDGET = {"near": 3000, "mid": 300, "far": 40}               # points added, at most
EASE = {"near": 1.0, "mid": 0.9, "far": 0.75}                # the share of the shade's darkening a level carries
UNDER = 0.002                 # a new point this far under the body's surface is a fault
STRETCH = 1.25                # a strap edge this much longer posed than at rest is flagged
SHADE_REACH = 0.015           # old points this near under a new piece are darkened
SHADE_MOST = 0.14             # by this share at most (at the piece), easing off to none at SHADE_REACH
SHADE_FLOOR = 0.40            # the files' floor
FOOT = 0.0025                 # a piece's foot over the cloth (two bones cannot carry it with the cloth to the hair)
SLIP = 0.003                  # a seam is left out where two bones cannot carry it with the cloth this near
SINK_COST = 5.0               # a strap's fit (chain_weights): a distance under the cloth counts this many times more
LIFT_EVERY = 2                # the clearance lift (Kit.sink) tests every second key of the clips, as the fit does
RING = 0.300                  # the Sherman's hatch ring: inside radius about the commander socket
RIM_OVER = 0.25               # its rim over the socket
RING_DEEP = 0.10              # the ring's wall reaches this far under the rim (a dark floor closes it there)

# How each stuff takes the weather (the README's "Wear and weather"): how much it darkens soaked, how much snow lies
# on it, the share of the cloth's dirt it carries, and the floor of its baked shadow.
STUFF = {
    "leather": {"wet": 0.58, "snow": 0.50, "dirt": 0.8, "low": 0.58},
    "steel": {"wet": 0.18, "snow": 0.35, "dirt": 0.5, "low": 0.58},
    "rubber": {"wet": 0.20, "snow": 0.40, "dirt": 0.6, "low": 0.58},
    "grip": {"wet": 0.30, "snow": 0.30, "dirt": 0.5, "low": 0.60},
    "cloth": {"wet": 0.92, "snow": 0.70, "dirt": 1.0, "low": 0.50},
    "web": {"wet": 0.80, "snow": 0.60, "dirt": 1.0, "low": 0.55},
}


def unit(v):
    v = np.asarray(v, float)
    n = np.linalg.norm(v, axis=-1, keepdims=True)
    return v / np.maximum(n, 1e-12)


def hexrgb(h):
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float)


def lightness(rgb):
    """CIE L* (0 to 100) of an sRGB colour (0 to 255)."""
    c = np.clip(np.asarray(rgb, float) / 255.0, 0, 1)
    lin = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    y = lin @ np.array((0.2126, 0.7152, 0.0722))
    return float(116.0 * (y ** (1 / 3.0)) - 16.0) if y > 0.008856 else float(903.3 * y)


def paint_names(D):
    """Paint index -> name: the `about` text's list ("Paints: 0 tunic, 1 trousers, ...") and `new_paints`."""
    import re
    out = {}
    m = re.search(r"Paints: (.*?)(?: \(|\. )", D.get("about", ""))
    if m:
        for part in m.group(1).split(","):
            bits = part.strip().split(" ", 1)
            if len(bits) == 2 and bits[0].isdigit():
                out[int(bits[0])] = bits[1].strip()
    for k, v in D.get("new_paints", {}).items():
        out[int(v)] = k
    return out


# ====================================================================================================== surfaces
def closest_on_triangles(Q, A, B, C):
    """Closest point of each triangle to each point, broadcast together (..., 3): the points, and their weights on
    A, B and C."""
    ab, ac, ap = B - A, C - A, Q - A
    d1, d2 = (ab * ap).sum(-1), (ac * ap).sum(-1)
    bp = Q - B
    d3, d4 = (ab * bp).sum(-1), (ac * bp).sum(-1)
    cp = Q - C
    d5, d6 = (ab * cp).sum(-1), (ac * cp).sum(-1)
    va, vb, vc = d3 * d6 - d5 * d4, d5 * d2 - d1 * d6, d1 * d4 - d3 * d2

    def div(a, b):
        return a / np.where(np.abs(b) > 1e-30, b, 1.0)
    den = va + vb + vc
    conds = [(d1 <= 0) & (d2 <= 0), (d3 >= 0) & (d4 <= d3), (vc <= 0) & (d1 >= 0) & (d3 <= 0),
             (d6 >= 0) & (d5 <= d6), (vb <= 0) & (d2 >= 0) & (d6 <= 0), (va <= 0) & (d4 - d3 >= 0) & (d5 - d6 >= 0)]
    wbc = div(d4 - d3, (d4 - d3) + (d5 - d6))
    vs = [0.0 * d1, 1.0 + 0.0 * d1, div(d1, d1 - d3), 0.0 * d1, 0.0 * d1, 1.0 - wbc]
    ws = [0.0 * d1, 0.0 * d1, 0.0 * d1, 1.0 + 0.0 * d1, div(d2, d2 - d6), wbc]
    v = np.select(conds, vs, div(vb, den))
    w = np.select(conds, ws, div(vc, den))
    v, w = np.clip(v, 0, 1), np.clip(w, 0, 1)
    pt = A + ab * v[..., None] + ac * w[..., None]
    return pt, np.stack([1 - v - w, v, w], -1)


class Surface:
    """Triangles to measure against: nearest point (with which side of the surface a point is on) and rays."""

    def __init__(self, P, T, F=None, flip=None):
        self.P = np.asarray(P, float)
        self.T = np.asarray(T, int).reshape(-1, 3)
        T = self.T
        self.A, self.B, self.C = self.P[T[:, 0]], self.P[T[:, 1]], self.P[T[:, 2]]
        n = np.cross(self.B - self.A, self.C - self.A)
        if flip is None:          # (outward is the points' facings, whatever the winding)
            flip = np.zeros(len(T), bool)
            if F is not None:
                f = np.asarray(F, float)
                flip = (n * (f[T[:, 0]] + f[T[:, 1]] + f[T[:, 2]])).sum(1) < 0
        self.flip = flip
        n = np.where(flip[:, None], -n, n)
        self.area2 = np.linalg.norm(n, axis=1)
        self.n = n / np.maximum(self.area2, 1e-15)[:, None]
        self.cen = (self.A + self.B + self.C) / 3.0
        self.rad = np.sqrt(np.maximum.reduce([((X - self.cen) ** 2).sum(1) for X in (self.A, self.B, self.C)]))

    def moved(self, P):
        """The same triangles on moved points (a pose): outward stays the side it was."""
        return Surface(P, self.T, flip=self.flip)

    def only(self, keep):
        """The same surface with only some triangles (a mask or indices over its triangles)."""
        S = Surface.__new__(Surface)
        S.P = self.P
        S.T = self.T[keep]
        for k in ("A", "B", "C", "n", "cen", "rad", "area2", "flip"):
            setattr(S, k, getattr(self, k)[keep])
        return S

    def near_ball(self, c, r):
        """Indices of the triangles that reach into the ball."""
        return np.where(np.linalg.norm(self.cen - c, axis=1) - self.rad < r)[0]

    def nearest(self, Q, K=40, chunk=96):
        """For each point: its distance to the surface, signed (under the surface is negative), the triangle, the
        nearest point on it, that point's weights on the triangle's corners, and the outward direction there."""
        Q = np.atleast_2d(np.asarray(Q, float))
        n, m = len(Q), len(self.T)
        sd = np.zeros(n)
        tri = np.zeros(n, int)
        pt = np.zeros((n, 3))
        bary = np.zeros((n, 3))
        nrm = np.zeros((n, 3))
        if m == 0:
            sd[:] = 9.0
            return {"sd": sd, "tri": tri, "pt": pt, "bary": bary, "n": nrm}
        k = min(K, m)
        cen32, rad32 = self.cen.astype(np.float32), self.rad.astype(np.float32)
        for s in range(0, n, chunk):
            q = Q[s:s + chunk]
            lower = np.linalg.norm(q.astype(np.float32)[:, None, :] - cen32[None], axis=2) - rad32[None]
            if k < m:
                idx = np.argpartition(lower, k - 1, axis=1)[:, :k]
            else:
                idx = np.tile(np.arange(m), (len(q), 1))
            rows = np.arange(len(q))[:, None]
            cp, bc = closest_on_triangles(q[:, None, :], self.A[idx], self.B[idx], self.C[idx])
            d = np.linalg.norm(cp - q[:, None, :], axis=2)
            j = d.argmin(1)
            dmin = d[rows[:, 0], j]
            kth = lower[rows, idx].max(1)
            for i in np.where((kth < dmin) & (k < m))[0]:          # (more triangles could be nearer: all of them)
                more = np.where(lower[i] <= dmin[i])[0]
                cp2, bc2 = closest_on_triangles(q[i][None, :], self.A[more], self.B[more], self.C[more])
                d2 = np.linalg.norm(cp2 - q[i], axis=1)
                kk = min(len(more), idx.shape[1])
                o = np.argsort(d2)[:kk]
                idx[i, :kk], cp[i, :kk], bc[i, :kk], d[i, :kk] = more[o], cp2[o], bc2[o], d2[o]
                if kk < idx.shape[1]:
                    d[i, kk:] = 9.0
                j[i] = 0
                dmin[i] = d2[o[0]]
            r = rows[:, 0]
            best = idx[r, j]
            tie = d <= (dmin[:, None] + 1e-7)                     # (on an edge or a corner: all that share it)
            nsum = (self.n[idx] * tie[..., None]).sum(1)
            off = q - cp[r, j]
            side = (off * nsum).sum(1)
            flat = (off * self.n[best]).sum(1)
            sign = np.where(np.abs(side) > 1e-12, np.sign(side), np.where(flat < 0, -1.0, 1.0))
            sd[s:s + chunk] = dmin * sign
            tri[s:s + chunk] = best
            pt[s:s + chunk] = cp[r, j]
            bary[s:s + chunk] = bc[r, j]
            nrm[s:s + chunk] = unit(nsum)
        return {"sd": sd, "tri": tri, "pt": pt, "bary": bary, "n": nrm}

    def rays(self, O, Dv, reach=9.0, start=1e-5):
        """First hit along each ray (origins O, directions Dv, unit): the distance (inf: none) and the triangle."""
        O = np.atleast_2d(np.asarray(O, float))
        Dv = np.atleast_2d(np.asarray(Dv, float))
        E1, E2 = self.B - self.A, self.C - self.A
        tt = np.full(len(O), np.inf)
        which = np.full(len(O), -1)
        for s in range(0, len(O), 64):
            o, d = O[s:s + 64], Dv[s:s + 64]
            pv = np.cross(d[:, None, :], E2[None])
            det = (E1[None] * pv).sum(2)
            ok = np.abs(det) > 1e-14
            inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
            tv = o[:, None, :] - self.A[None]
            u = (tv * pv).sum(2) * inv
            qv = np.cross(tv, E1[None])
            v = (d[:, None, :] * qv).sum(2) * inv
            t = (E2[None] * qv).sum(2) * inv
            hit = ok & (u >= -1e-9) & (v >= -1e-9) & (u + v <= 1 + 1e-9) & (t > start) & (t < reach)
            t = np.where(hit, t, np.inf)
            j = t.argmin(1)
            tj = t[np.arange(len(o)), j]
            tt[s:s + 64] = tj
            which[s:s + 64] = np.where(np.isfinite(tj), j, -1)
        return tt, which


def openness(Pn, Nn, S, rays=40, reach=0.18, seed=7):
    """How open each point is round its facing (0 shut in, 1 open): rays over the half round its facing against the
    triangles of S within reach, cosine weighted (mg_gunners' bake, with the far triangles left out first). Also
    whether straight up is open."""
    rng = np.random.default_rng(seed)
    u = rng.random((rays, 2))
    r = np.sqrt(u[:, 0])
    th = 2 * math.pi * u[:, 1]
    loc = np.stack([r * np.cos(th), r * np.sin(th), np.sqrt(1 - u[:, 0])], 1)
    open_ = np.ones(len(Pn))
    up = np.ones(len(Pn))
    if len(Pn) == 0:
        return open_, up
    # (the triangles near the whole piece first, then each point's own)
    lo, hi = Pn.min(0) - reach, Pn.max(0) + reach
    box = np.all((S.cen + S.rad[:, None] > lo) & (S.cen - S.rad[:, None] < hi), axis=1)
    Sb = S.only(box)
    for i in range(len(Pn)):
        n = unit(Nn[i])
        a = np.array((1.0, 0, 0)) if abs(n[0]) < 0.9 else np.array((0, 1.0, 0))
        e1 = unit(np.cross(n, a))
        e2 = np.cross(n, e1)
        d = loc[:, :1] * e1 + loc[:, 1:2] * e2 + loc[:, 2:] * n
        o = Pn[i] + n * 4e-4
        Si = Sb.only(Sb.near_ball(o, reach))
        if len(Si.T) == 0:
            continue
        t, _ = Si.rays(np.tile(o, (rays + 1, 1)), np.vstack([d, [[0.0, 0.0, 1.0]]]), reach=reach, start=2e-4)
        # (a near hit counts most)
        open_[i] = 1.0 - float(np.where(np.isfinite(t[:rays]), 1.0 - 0.5 * t[:rays] / reach, 0.0).mean())
        up[i] = 0.0 if np.isfinite(t[rays]) else 1.0
    return open_, up


# ======================================================================================================== the man
class Level:
    """One level of a soldier file as it was before the kit: its points and looks, and its surface (no gun)."""

    def __init__(self, D, L):
        M = D[L]
        self.L = L
        self.names = [b["name"] for b in D["bones"]]
        self.nb = len(self.names)
        self.H = np.array([b["head"] for b in D["bones"]], float)
        self.P = np.array(M["points"], float).reshape(-1, 3)
        self.F = np.array(M["facings"], float).reshape(-1, 3)
        self.Tall = np.array(M["triangles"], int).reshape(-1, 3)
        self.paint = np.array(M["paints"], int)
        self.part = np.array(M["part"], int)
        self.part2 = np.array(M["part2"], int)
        self.w2 = np.array(M["w2"], float)
        self.looks = {k: np.array(M[k], float) for k in ("shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow")}
        self.tint = np.array(M["tint"], float).reshape(-1, 3)
        gun = np.isin(self.part, [self.names.index(n) for n in GUN_BONES if n in self.names])
        self.gun = gun
        self.T = self.Tall[~gun[self.Tall].any(1)]
        self.pnames = paint_names(D)
        self.pindex = {v: k for k, v in self.pnames.items()}
        # (outward: against the winding, as the soldier files are wound; the few triangles whose points' facings
        # plainly say the other side are taken by their facings. A flat cap whose points face out along its rim
        # says nothing by its facings, so the winding is the rule.)
        g = -np.cross(self.P[self.T[:, 1]] - self.P[self.T[:, 0]], self.P[self.T[:, 2]] - self.P[self.T[:, 0]])
        fm = self.F[self.T].sum(1)
        other = ((unit(g) * unit(fm)).sum(1) < -0.5) & (np.linalg.norm(fm, axis=1) > 1.8)
        self.surface = Surface(self.P, self.T, flip=~other)
        # each point's weights over the bones
        self.W = np.zeros((len(self.P), self.nb))
        self.W[np.arange(len(self.P)), self.part] += 1.0 - self.w2
        self.W[np.arange(len(self.P)), self.part2] += self.w2

    def bone(self, name):
        return self.names.index(name)

    def on(self, *bones):
        return np.isin(self.part, [self.names.index(b) for b in bones if b in self.names])

    def painted(self, *paints):
        return np.isin(self.paint, [self.pindex[p] if isinstance(p, str) else p for p in paints])

    def ground(self, mask):
        """The surface of the triangles whose three points are all in the mask."""
        return self.surface.only(mask[self.T].all(1))

    def ride(self, S, Q, poses=None, least=0.02):
        """Bone weights for points Q riding the ground S (a Surface of this level's own points): each takes the
        cloth's own blend at its foot (its nearest point of S: the triangle's corners by its place on it), and of
        that blend the two bones and the share that carry the point nearest to where the cloth carries its foot (the
        point's height over the foot turned by the blend), over `poses` (clip_poses): the least of the worst key's
        distance, a distance inward (under the cloth) counted three times. Returns the weights (two bones at most
        a point) and each point's slip: the farthest the two bones leave it from the cloth's own way."""
        Q = np.atleast_2d(np.asarray(Q, float))
        hit = S.nearest(Q)
        tri = S.T[hit["tri"]]
        Wf = (self.W[tri] * hit["bary"][..., None]).sum(1)
        out = np.zeros_like(Wf)
        slip = np.zeros(len(Q))
        for i in range(len(Q)):
            bones = np.where(Wf[i] > least)[0]
            if len(bones) == 0:
                bones = np.array([int(Wf[i].argmax())])
            if len(bones) == 1:
                out[i, bones[0]] = 1.0
                continue
            if poses is None:                      # (no clips to judge by: the two heaviest)
                o = bones[np.argsort(-Wf[i, bones])[:2]]
                out[i, o] = Wf[i, o] / Wf[i, o].sum()
                continue
            GK, XK = poses
            foot = 0.0
            for c in range(3):                     # the foot, as the file's own points carry it
                v = tri[i, c]
                pa = XK[:, self.part[v]] + (self.P[v] - self.H[self.part[v]]) @ np.swapaxes(GK[:, self.part[v]], 1, 2)
                pb = XK[:, self.part2[v]] + (self.P[v] - self.H[self.part2[v]]) @ np.swapaxes(GK[:, self.part2[v]], 1, 2)
                foot = foot + hit["bary"][i, c] * ((1.0 - self.w2[v]) * pa + self.w2[v] * pb)
            wn = Wf[i, bones] / Wf[i, bones].sum()
            turn = np.einsum("b,kbij->kij", wn, GK[:, bones])
            want = foot + (Q[i] - hit["pt"][i]) @ np.swapaxes(turn, 1, 2)
            up = unit(hit["n"][i] @ np.swapaxes(turn, 1, 2))               # the cloth's outward there, posed
            M = [XK[:, b] + (Q[i] - self.H[b]) @ np.swapaxes(GK[:, b], 1, 2) for b in bones]
            best = (9.0, 9.0, bones[0], bones[0], 0.0)
            for j in range(len(bones)):
                for k in range(j, len(bones)):
                    d = M[k] - M[j]
                    r = want - M[j]
                    ws = _SHARES if k > j else _SHARES[:1]
                    e = r[None] - ws[:, None, None] * d[None]              # (shares, keys, 3): short of the cloth's way
                    off = np.linalg.norm(e, axis=2)
                    sink = np.maximum((e * up[None]).sum(2), 0.0)          # short of it inward: under the cloth
                    cost = (off + 2.0 * sink).max(1)                       # (rather over the cloth than under it)
                    m = int(cost.argmin())
                    if cost[m] < best[0] - 1e-9:
                        best = (float(cost[m]), float(off[m].max()), bones[j], bones[k], float(ws[m]))
            _, e, a, b, w = best
            out[i, a] += 1.0 - w
            out[i, b] += w
            slip[i] = e
        return out, slip

    def nearest_point(self, Q):
        """The body's own point nearest each of Q (for the looks a new point takes from the cloth under it)."""
        used = np.unique(self.T)
        out = np.zeros(len(Q), int)
        for s in range(0, len(Q), 256):
            d = np.linalg.norm(Q[s:s + 256, None, :] - self.P[used][None], axis=2)
            out[s:s + 256] = used[d.argmin(1)]
        return out


_SHARES = np.linspace(0.0, 1.0, 41)          # the shares of a second bone Level.ride tries


def clip_poses(D, clips, every=2):
    """The bones' turns and places (gore.pose) at the keys of some of a file's clips: every `every`th key of each
    and its last. What Level.ride judges two bones by."""
    rig = gore.Rig(D["bones"])
    Gs, Xs = [], []
    for cn in clips:
        c = D["clips"].get(cn)
        if c is None:
            continue
        ts = list(gore.clip_times(c))
        for i in sorted(set(list(range(0, len(ts), every)) + [len(ts) - 1])):
            G, X = gore.pose(rig, c, ts[i])
            Gs.append(np.array(G, float))
            Xs.append(np.array(X, float))
    if not Gs:
        return None
    return np.array(Gs), np.array(Xs)


def clip_pose_weights(D, clips, weights, every=2):
    """How much each key of clip_poses counts when a strap's bones are fitted: a weight a clip (by name; others 1),
    in clip_poses' order."""
    out = []
    for cn in clips:
        c = D["clips"].get(cn)
        if c is None:
            continue
        ts = list(gore.clip_times(c))
        out += [weights.get(cn, 1.0)] * len(sorted(set(list(range(0, len(ts), every)) + [len(ts) - 1])))
    return np.array(out) if out else None


def top_two(W):
    """Weights over the bones kept to the two heaviest of each row, summing to one (as the file carries them)."""
    W = np.atleast_2d(np.asarray(W, float))
    a, b, w2 = two_bones(W)
    out = np.zeros_like(W)
    r = np.arange(len(W))
    out[r, a] += 1.0 - w2
    out[r, b] += w2
    return out


def two_bones(W):
    """Weights over the bones -> the file's two bones and the share on the second (0 to 0.5)."""
    o = np.argsort(-W, axis=1)
    a, b = o[:, 0], o[:, 1]
    r = np.arange(len(W))
    wa, wb = W[r, a], W[r, b]
    w2 = np.clip(wb / np.maximum(wa + wb, 1e-12), 0.0, 0.5)
    b = np.where(w2 < 5e-4, a, b)
    w2 = np.where(w2 < 5e-4, 0.0, w2)
    return a, b, np.round(w2, 3)


def spline(G, step, closed=False):
    """A smooth line through the guide points (Catmull-Rom), sampled about every `step`."""
    G = np.asarray(G, float)
    n = len(G)
    out = []
    segs = n if closed else n - 1
    for i in range(segs):
        p0 = G[(i - 1) % n] if (closed or i > 0) else 2 * G[0] - G[1]
        p1, p2 = G[i % n], G[(i + 1) % n]
        p3 = G[(i + 2) % n] if (closed or i + 2 < n) else 2 * G[-1] - G[-2]
        k = max(2, int(math.ceil(np.linalg.norm(p2 - p1) / step)))
        for j in range(k):
            t = j / k
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                              + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    if not closed:
        out.append(G[-1])
    return np.array(out)


def even(Pth, step, closed=False):
    """The same line in equal steps (about `step` long, the ends kept)."""
    Pth = np.asarray(Pth, float)
    Q = np.vstack([Pth, Pth[:1]]) if closed else Pth
    seg = np.linalg.norm(np.diff(Q, axis=0), axis=1)
    s = np.concatenate([[0], np.cumsum(seg)])
    n = max(2, int(round(s[-1] / step)))
    at = np.linspace(0, s[-1], n + 1)
    if closed:
        at = at[:-1]
    return np.stack([np.interp(at, s, Q[:, k]) for k in range(3)], 1)


class Piece:
    """One piece's mesh at one level, being built. role: 0 a foot on the cloth, 1 proud of it (held off by the
    level's standoff), 2 free (a thing standing on a piece, a hanging cord)."""

    def __init__(self, name, about):
        self.name, self.about = name, about
        self.P, self.N, self.paint, self.tint, self.stuff, self.role, self.W = [], [], [], [], [], [], []
        self.T = []
        self.strap_edges = []        # (i, j): the edges whose stretch is measured as a strap's

    def add(self, p, n, paint, tint, stuff, role, w):
        self.P.append(np.asarray(p, float))
        self.N.append(unit(n))
        self.paint.append(paint)
        self.tint.append(np.broadcast_to(np.asarray(tint, float), (3,)).copy())
        self.stuff.append(stuff)
        self.role.append(role)
        self.W.append(np.asarray(w, float))
        return len(self.P) - 1

    def quad(self, a, b, c, d):
        self.T += [(a, b, c), (a, c, d)]

    def strip(self, row0, row1, closed=False):
        """Triangles between two rows of points (the same count)."""
        n = len(row0)
        for i in range(n if closed else n - 1):
            j = (i + 1) % n
            self.quad(row0[i], row0[j], row1[j], row1[i])

    def fan(self, ring, centre=None):
        if centre is None:
            for i in range(1, len(ring) - 1):
                self.T.append((ring[0], ring[i], ring[i + 1]))
        else:
            for i in range(len(ring)):
                self.T.append((centre, ring[i], ring[(i + 1) % len(ring)]))

    def arrays(self):
        P, N = np.array(self.P).reshape(-1, 3), np.array(self.N).reshape(-1, 3)
        T = np.array(self.T, int).reshape(-1, 3)
        if len(T):
            # (no slivers of no area; each triangle wound outward: cross(b - a, c - a) along its points' facings)
            g = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
            T = T[np.linalg.norm(g, axis=1) > 1e-10]
            g = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
            back = (g * (N[T[:, 0]] + N[T[:, 1]] + N[T[:, 2]])).sum(1) < 0
            T[back] = T[back][:, [0, 2, 1]]
        return P, N, T


class Kit:
    """The pieces of one level being built on the man as he was."""

    def __init__(self, D, L, poses=None, shared=None, pose_w=None, lift_poses=None):
        self.D, self.L = D, L
        self.body = Level(D, L)
        self.poses = poses
        self.lift_poses = poses if lift_poses is None else lift_poses     # (what Kit.sink lifts against)
        self.pose_w = pose_w             # (how much each key counts in a strap's fit: the hatch clips most)
        self.lift = STANDOFF[L]
        self.pieces = []
        self.notes = {}
        self.shared = {} if shared is None else shared      # (what a level built first leaves for the next)

    # ---------------------------------------------------------------------------------------------------- helpers
    def sink(self, Q, W, reach=0.08, far=0.05):
        """How deep each of Q (rest places; W its weights over all the bones) goes under the body's surface over the
        poses (self.lift_poses: the clips' keys, every LIFT_EVERY-th): the most over the keys, in metres (0: never
        under). As the checks count it: another part
        of the body passing through the cloth a point rides (an arm through the side) is left out, and so is a
        surface more than `far` away."""
        B = self.body
        Q = np.atleast_2d(np.asarray(Q, float))
        W = np.atleast_2d(np.asarray(W, float))
        out = np.zeros(len(Q))
        if self.lift_poses is None or len(Q) == 0:
            return out
        S0 = B.surface
        lo, hi = Q.min(0) - reach, Q.max(0) + reach
        S0 = S0.only(np.all((S0.cen + S0.rad[:, None] > lo) & (S0.cen - S0.rad[:, None] < hi), axis=1))
        if len(S0.T) == 0:
            return out
        used = np.unique(S0.T)
        main = np.left_shift(np.int64(1), B.part.astype(np.int64))
        om = bone_masks(B.part, B.part2, B.w2)
        rt = S0.T[S0.nearest(Q)["tri"]]
        pm = om[rt[:, 0]] | om[rt[:, 1]] | om[rt[:, 2]]
        bones = np.where(W.max(0) > 1e-6)[0]
        for b in bones:
            pm = pm | np.where(W[:, b] > 1e-6, np.int64(1) << np.int64(b), np.int64(0))
        a, b2, w2 = B.part[used], B.part2[used], B.w2[used][:, None]
        Pu = B.P[used]
        GK, XK = self.lift_poses
        Pp = np.zeros_like(B.P)
        for k in range(len(GK)):
            G, X = GK[k], XK[k]
            pa = X[a] + np.einsum("nij,nj->ni", G[a], Pu - B.H[a])
            pb = X[b2] + np.einsum("nij,nj->ni", G[b2], Pu - B.H[b2])
            Pp[used] = pa * (1.0 - w2) + pb * w2
            Qp = np.zeros_like(Q)
            for b in bones:
                Qp += W[:, b:b + 1] * (X[b] + (Q - B.H[b]) @ G[b].T)
            Sp = S0.moved(Pp)
            Sp = Sp.only(~folded(B, S0, Sp, G))        # (not against the body's own cloth turned inside out)
            if len(Sp.T) == 0:
                continue
            res = Sp.nearest(Qp)
            d = np.maximum(-res["sd"], 0.0)
            tm = main[Sp.T[:, 0]] | main[Sp.T[:, 1]] | main[Sp.T[:, 2]]
            d[(tm[res["tri"]] & pm) == 0] = 0.0
            d[d > far] = 0.0
            out = np.maximum(out, d)
        return out

    def piece(self, name, about):
        p = Piece(name, about)
        self.pieces.append(p)
        return p

    def paint(self, name):
        return self.body.pindex[name]

    def touch(self, ground, Q):
        """Each of Q on a ground: where its nearest point is, the outward direction, and the bone weights there."""
        hit = ground.nearest(Q)
        hit["surface"] = ground
        out = unit(np.where((hit["sd"] > 1e-4)[:, None], Q - hit["pt"], hit["n"]))
        return hit, out

    def over(self, ground):
        """A ground with the pieces built so far laid on it, so a strap put on later lies over an earlier one."""
        Ps, Ts, flips, n0 = [ground.P], [ground.T], [ground.flip], len(ground.P)
        for pc in self.pieces:
            P, N, T = pc.arrays()
            if len(T):
                Ps.append(P)
                Ts.append(T + n0)
                flips.append(np.zeros(len(T), bool))
                n0 += len(P)
        return Surface(np.vstack(Ps), np.vstack(Ts), flip=np.concatenate(flips))

    def drape(self, ground, guide, step, lift=None, closed=False, taut=8, fine=None, ride=None, ease=0.0,
              ease_most=0.005):
        """A line laid on a ground along guide points given near it: pulled taut (it bridges hollows, never sinks
        under `lift`), then cut in equal steps. Returns the stations: at (lifted), foot (on the ground), out
        (outward), along (the line's direction), W and slip (Level.ride on `ride`, the body's own ground under it,
        when the ground it lies on has other pieces in it), rise (with `ease`: how much higher than `lift` a station
        stands because its slip is over UNDER, `ease` of the excess and `ease_most` at most)."""
        lift = self.lift if lift is None else lift
        fine = fine or step * 0.5
        p = even(spline(guide, fine, closed), fine, closed)
        hit, out = self.touch(ground, p)
        p = hit["pt"] + out * lift
        for _ in range(taut):
            q = p.copy()
            if closed:
                q = 0.5 * p + 0.25 * (np.roll(p, 1, 0) + np.roll(p, -1, 0))
            else:
                q[1:-1] = 0.5 * p[1:-1] + 0.25 * (p[:-2] + p[2:])
            hit, out = self.touch(ground, q)
            low = hit["sd"] < lift
            q[low] = hit["pt"][low] + out[low] * lift
            p = q
        p = even(p, step, closed)
        hit, out = self.touch(ground, p)
        low = hit["sd"] < lift
        p[low] = hit["pt"][low] + out[low] * lift
        # (the outward direction eased along the line, so the strap does not twist from facet to facet)
        for _ in range(2):
            if closed:
                out = unit(out + 0.5 * (np.roll(out, 1, 0) + np.roll(out, -1, 0)))
            else:
                o2 = out.copy()
                o2[1:-1] = out[1:-1] + 0.5 * (out[:-2] + out[2:])
                out = unit(o2)
        if closed:
            along = unit(np.roll(p, -1, 0) - np.roll(p, 1, 0))
        else:
            along = unit(np.gradient(p, axis=0))
        ride = ground if ride is None else ride
        W, slip = self.body.ride(ride, p, self.poses)
        rise = np.zeros(len(p))
        if ease > 0:          # (where the cloth pulls apart under it in the clips, a strap bridges: it stands higher)
            rise = np.clip((slip - UNDER) * ease, 0.0, ease_most)
            for _ in range(2):
                nb = np.maximum(np.roll(rise, 1), np.roll(rise, -1))
                if not closed:
                    nb[0], nb[-1] = rise[1], rise[-2]
                rise = np.maximum(rise, 0.6 * nb)
            p = p + out * rise[:, None]
            W, slip = self.body.ride(ride, p, self.poses)
        return {"at": p, "foot": hit["pt"], "out": out, "along": along, "W": W, "slip": slip, "closed": closed,
                "ride": ride, "rise": rise}

    def ribbon(self, pc, ground, st, width, paint, tint, stuff, lift=None, shape="box", caps=(True, True),
               strap=True, keep=None, deep=0.003, smooth=0, clear=0.0, weights=None):
        """A strap along draped stations. shape "box": two feet on the cloth and two proud corners a station (its
        sides reach down to the cloth, its face is held `lift` off it; where it bridges a hollow its sides go no
        deeper than `deep` under where the cloth would be); "tent": two feet and a ridge (a seam's welt); "flat":
        two proud corners only (a far level's band). width: one number or one a station. smooth: passes of easing
        the weights along the strap, every point of a station taking the station's (so it moves as one across and
        does not stretch in steps where the cloth under it does); clear: the most a station is lifted further so that
        no point of it goes under the body in the clips (Kit.sink), one number or one a station; weights: one set of
        bone weights a station (chain_weights), which every point of the station takes."""
        lift = self.lift if lift is None else lift
        n = len(st["at"])
        clear = np.broadcast_to(np.asarray(clear, float), (n,)).copy()
        width = np.broadcast_to(np.asarray(width, float), (n,))
        side = unit(np.cross(st["out"], st["along"]))
        idx = list(range(n)) if keep is None else [i for i in range(n) if keep[i]]
        if shape == "tent":
            specs = [(-1, 0), (0, 1), (1, 0)]
        elif shape == "flat":
            specs = [(-1, 1), (1, 1)]
        else:
            specs = [(-1, 0), (-1, 1), (1, 1), (1, 0)]
        pos, fac = {}, {}
        for i in idx:
            for k, (sg, role) in enumerate(specs):
                e = st["at"][i] + side[i] * (0.5 * width[i] * sg)
                hit, out = self.touch(ground, e[None])
                if role == 0:
                    p = hit["pt"][0] + out[0] * (FOOT + st["rise"][i])
                    if shape == "box" and hit["sd"][0] - (lift + deep) > FOOT + st["rise"][i]:
                        p = e - out[0] * (lift + deep)                     # (bridging: the side stops short)
                    nn = side[i] * sg + st["out"][i] * 0.15
                else:
                    top = lift * 1.6 if shape == "tent" else lift          # (a welt's ridge: its sides stand steep)
                    if shape == "tent":
                        e = e + st["out"][i] * (top - lift)
                    p = e if hit["sd"][0] >= top else hit["pt"][0] + out[0] * top
                    nn = st["out"][i] + side[i] * (0.45 * sg)
                pos[(i, k)], fac[(i, k)] = p, nn
        closed = st["closed"] and keep is None
        if shape != "tent":
            # (a face along the cloth is held off it at its middle too: across the strap and between stations)
            tops = [k for k, (sg, role) in enumerate(specs) if role == 1]
            pairs = [(idx[a], idx[(a + 1) % len(idx)]) for a in range(len(idx) if closed else len(idx) - 1)]
            for _ in range(5):
                need = {i: 0.0 for i in idx}
                mids = np.array([0.5 * (pos[(i, tops[0])] + pos[(i, tops[1])]) for i in idx])
                sd = ground.nearest(mids)["sd"]
                for i, d in zip(idx, sd):
                    need[i] = max(need[i], self.lift + 4e-4 - d)
                if pairs:                 # (each face's middle, and its two triangles' own)
                    cs = []
                    for i, j in pairs:
                        a, b, c, d = pos[(i, tops[0])], pos[(i, tops[1])], pos[(j, tops[1])], pos[(j, tops[0])]
                        cs += [0.25 * (a + b + c + d), (a + b + c) / 3.0, (a + c + d) / 3.0, (a + b + d) / 3.0,
                               (b + c + d) / 3.0]
                    sd = ground.nearest(np.array(cs))["sd"].reshape(-1, 5).min(1)
                    for (i, j), d in zip(pairs, sd):
                        need[i] = max(need[i], self.lift + 4e-4 - d)
                        need[j] = max(need[j], self.lift + 4e-4 - d)
                if max(need.values()) <= 0:
                    break
                for i in idx:
                    if need[i] > 0:
                        for k in tops:
                            pos[(i, k)] = pos[(i, k)] + st["out"][i] * need[i]
        keys = [(i, k) for i in idx for k in range(len(specs))]
        ns = len(specs)
        if weights is not None:                   # (each station's own set, the same for all its points)
            Wp = top_two(np.repeat(np.asarray(weights, float)[idx], ns, axis=0))
        elif smooth:                              # (the station's weights, eased along the strap)
            Wp, _ = self.body.ride(st["ride"], np.array([pos[q] for q in keys]), self.poses)
            Ws = Wp.reshape(len(idx), ns, -1).mean(1)
            for _ in range(smooth):
                Wn = Ws.copy()
                if closed:
                    Wn = 0.5 * Ws + 0.25 * (np.roll(Ws, 1, 0) + np.roll(Ws, -1, 0))
                elif len(Ws) > 2:
                    Wn[1:-1] = 0.5 * Ws[1:-1] + 0.25 * (Ws[:-2] + Ws[2:])
                Ws = Wn
            Wp = top_two(np.repeat(Ws, ns, axis=0))
        else:
            Wp, _ = self.body.ride(st["ride"], np.array([pos[q] for q in keys]), self.poses)
            Wp = top_two(Wp)
        cap = clear[idx]
        if cap.max() > 0 and self.poses is not None:  # (lifted where the cloth would come over it in the clips)
            P0 = np.array([pos[q] for q in keys])
            outs = np.repeat(np.array([st["out"][i] for i in idx]), ns, axis=0)
            up = np.zeros(len(idx))
            for _ in range(4):
                need = self.sink(P0 + outs * np.repeat(up, ns)[:, None], Wp).reshape(len(idx), ns).max(1)
                if need.max() < 4e-4:
                    break
                up = np.minimum(up + np.where(need > 0, need + 8e-4, 0.0), cap)
                for _ in range(3):                # (eased along the strap)
                    nb = np.maximum(np.roll(up, 1), np.roll(up, -1))
                    if not closed:
                        nb[0], nb[-1] = up[1], up[-2]
                    up = np.minimum(np.maximum(up, 0.7 * nb), np.maximum(cap, 0.7 * nb))
            self.notes.setdefault("still_under_mm", {})[pc.name] = round(float(need.max()) * 1000, 1)
            for j, i in enumerate(idx):
                for k in range(ns):
                    pos[(i, k)] = pos[(i, k)] + st["out"][i] * up[j]
            self.notes.setdefault("lifted_mm", {})[pc.name] = round(float(up.max()) * 1000, 1)
            st["lifted"] = np.zeros(n)
            st["lifted"][idx] = up                # (what a thing laid on the strap later stands on)
        Wp = dict(zip(keys, Wp))                  # (each point rides the cloth under itself)
        rows = []
        for i in idx:
            rows.append([pc.add(pos[(i, k)], fac[(i, k)], paint, tint, stuff, role, Wp[(i, k)])
                         for k, (sg, role) in enumerate(specs)])
        for a in range(len(rows) if closed else len(rows) - 1):
            b = (a + 1) % len(rows)
            pc.strip(rows[a], rows[b])
            if strap:
                pc.strap_edges += [(rows[a][k], rows[b][k]) for k in range(len(rows[a]))]
        if not closed and shape != "flat" and rows:
            ends = [i for i in idx]
            for want, row, i, sgn in ((caps[0], rows[0], ends[0], -1.0), (caps[1], rows[-1], ends[-1], 1.0)):
                if want:                 # (an end's own points, facing along the strap)
                    pc.fan([pc.add(pc.P[j], st["along"][i] * sgn, paint, tint, stuff, pc.role[j], pc.W[j]) for j in row])
        return rows

    def looks(self):
        """Everything this level adds, as the file's lists: the pieces one after another."""
        B = self.body
        out = {"pieces": []}
        Ps, Ns, Ts, paints, tints, stuffs, roles, Ws, edges = [], [], [], [], [], [], [], [], []
        n0 = 0
        for pc in self.pieces:
            P, N, T = pc.arrays()
            if len(P) == 0:
                continue
            Ps.append(P)
            Ns.append(N)
            Ts.append(T + n0)
            paints += pc.paint
            tints += pc.tint
            stuffs += pc.stuff
            roles += pc.role
            Ws += pc.W
            edges += [(a + n0, b + n0) for a, b in pc.strap_edges]
            out["pieces"].append({"name": pc.name, "about": pc.about, "first": n0, "points": len(P),
                                  "first_triangle": sum(len(t) for t in Ts[:-1]), "triangles": len(T)})
            n0 += len(P)
        if not Ps:
            return None
        P, N, T = np.vstack(Ps), np.vstack(Ns), np.vstack(Ts)
        tint = np.array(tints)
        role = np.array(roles)
        a, b, w2 = two_bones(np.array(Ws))
        # the baked shadow: against the body and the kit itself
        both = Surface(np.vstack([B.P, P]), np.vstack([B.T, T + len(B.P)]),
                       flip=np.concatenate([B.surface.flip, np.zeros(len(T), bool)]))
        opn, upo = openness(P, N, both)
        low = np.array([STUFF[s]["low"] for s in stuffs])
        clean = low + (1.0 - low) * np.clip(opn, 0, 1) ** 0.85
        clean = 1.0 - EASE[self.L] * (1.0 - clean)
        under = B.nearest_point(P)
        dk = np.array([STUFF[s]["dirt"] for s in stuffs])
        grime = np.clip(B.looks["shade"][under] / np.maximum(B.looks["shade_clean"][under], 1e-6), 0.0, 1.0)
        shade = clean * (1.0 - dk * (1.0 - grime))
        dirt = B.looks["dirt"][under] * dk
        dirt_wet = B.looks["dirt_wet"][under] * dk
        wet = np.array([STUFF[s]["wet"] for s in stuffs]) * (0.75 + 0.25 * np.clip(opn, 0, 1))
        snow = np.clip(N[:, 2], 0, 1) ** 1.5 * np.array([STUFF[s]["snow"] for s in stuffs]) * upo
        out.update({"P": P, "N": N, "T": T, "paint": np.array(paints, int), "tint": tint, "role": role,
                    "part": a, "part2": b, "w2": w2, "shade": np.clip(shade, SHADE_FLOOR, 1.0),
                    "shade_clean": np.clip(clean, SHADE_FLOOR, 1.0), "dirt": np.clip(dirt, 0, 1),
                    "dirt_wet": np.clip(dirt_wet, 0, 1), "wet": np.clip(wet, 0, 1), "snow": np.clip(snow, 0, 1),
                    "edges": edges, "stuff": stuffs})
        return out


# ============================================================================================ into the file, and out
def strip(D):
    """Take this tool's additions off a file (and put the shade it touched back). Returns whether there were any."""
    blk = D.get("crew_kit")
    if not blk:
        return False
    for L in LEVELS:
        r = blk["ranges"].get(L)
        if not r:
            continue
        M = D[L]
        n0, t0 = r["first_point"], r["first_triangle"]
        assert len(M["paints"]) == n0 + r["points"] and len(M["triangles"]) == 3 * (t0 + r["triangles"]), \
            f"{L}: the kit's points are not the last of the lists (something was added after it)"
        for k in ONE:
            del M[k][n0:]
        for k in THREE:
            del M[k][3 * n0:]
        del M["triangles"][3 * t0:]
        st = blk.get("shade_touched", {}).get(L)
        if st:
            for i, a, b in zip(st["points"], st["shade"], st["shade_clean"]):
                M["shade"][i] = a
                M["shade_clean"][i] = b
    del D["crew_kit"]
    return True


def _r(v, k):
    return [round(float(x), k) for x in np.asarray(v).ravel()]


def put(D, L, looks):
    """One level's additions onto the end of its lists (the soldier files' winding: cross(b - a, c - a) against the
    facing), and the shade under them. Returns its ranges and the shade touched."""
    M = D[L]
    n0, t0 = len(M["paints"]), len(M["triangles"]) // 3
    old = Level(D, L)
    P = np.round(looks["P"], 5)
    M["points"] += _r(P, 5)
    M["facings"] += _r(looks["N"], 3)
    M["tint"] += _r(looks["tint"], 3)
    M["paints"] += [int(v) for v in looks["paint"]]
    M["part"] += [int(v) for v in looks["part"]]
    M["part2"] += [int(v) for v in looks["part2"]]
    M["w2"] += _r(looks["w2"], 3)
    for k in ("shade", "shade_clean"):
        M[k] += _r(looks[k], 3)
    for k in ("dirt", "dirt_wet", "wet", "snow"):
        M[k] += _r(looks[k], 2)
    M["triangles"] += [int(v) + n0 for tr in looks["T"] for v in (tr[0], tr[2], tr[1])]
    # the body's own shade a little darker right under a piece
    new = Surface(P, looks["T"])
    used = np.unique(old.T)
    lo, hi = P.min(0) - SHADE_REACH, P.max(0) + SHADE_REACH
    cand = used[np.all((old.P[used] > lo) & (old.P[used] < hi), axis=1)]
    touched = {"count": 0, "points": [], "shade": [], "shade_clean": []}
    if len(cand):
        d = np.abs(new.nearest(old.P[cand])["sd"])
        for i, di in zip(cand, d):
            if di >= SHADE_REACH:
                continue
            f = 1.0 - SHADE_MOST * EASE[L] * (1.0 - di / SHADE_REACH)
            a, b = M["shade"][i], M["shade_clean"][i]
            a2, b2 = max(SHADE_FLOOR, round(a * f, 3)), max(SHADE_FLOOR, round(b * f, 3))
            if a2 < a or b2 < b:
                touched["points"].append(int(i))
                touched["shade"].append(a)
                touched["shade_clean"].append(b)
                M["shade"][i], M["shade_clean"][i] = min(a, a2), min(b, b2)
        touched["count"] = len(touched["points"])
    return {"first_point": n0, "points": len(P), "first_triangle": t0, "triangles": len(looks["T"])}, touched


def base_md5(D):
    """The md5 of a file as it is written (compact json): what the kit is built on."""
    import hashlib
    return hashlib.md5(json.dumps(D, separators=(",", ":")).encode()).hexdigest()


def make(D, name, log=print):
    """The file's kit built on the man as he was and written into D. Returns the block."""
    spec = KITS[name]
    strip(D)
    on = base_md5(D)
    poses = clip_poses(D, spec["clips"])
    lift_poses = clip_poses(D, spec["clips"], every=LIFT_EVERY)
    pose_w = clip_pose_weights(D, spec["clips"], {c: spec.get("fit_weight", {}).get(
        "hatch" if c in spec["hatch_clips"] else "other", 1.0) for c in spec["clips"]})
    missing = [c for c in spec["clips"] if c not in D["clips"]]
    if missing:
        log(f"(clips not in this file, not fitted to: {missing})")
    built = {}
    shared = {}
    for L in LEVELS:
        K = Kit(D, L, poses, shared, pose_w, lift_poses)
        for fn in spec["pieces"]:
            fn(K)
        built[L] = (K, K.looks())
    blk = {"made_by": MADE_BY, "about": spec["about"], "built_on_md5": on,
           "clips_fitted": [c for c in spec["clips"] if c in D["clips"]], "pieces": [], "ranges": {},
           "shade_touched": {}}
    seen = {}
    for L in LEVELS:
        K, looks = built[L]
        if looks is None:
            blk["ranges"][L] = {"first_point": len(D[L]["paints"]), "points": 0,
                                "first_triangle": len(D[L]["triangles"]) // 3, "triangles": 0}
            continue
        rng, touched = put(D, L, looks)
        blk["ranges"][L] = rng
        blk["shade_touched"][L] = touched
        for p in looks["pieces"]:
            e = seen.get(p["name"])
            if e is None:
                e = {"name": p["name"], "about": p["about"], "at": {}}
                seen[p["name"]] = e
                blk["pieces"].append(e)
            e["at"][L] = {"first_point": rng["first_point"] + p["first"], "points": p["points"],
                          "first_triangle": rng["first_triangle"] + p["first_triangle"], "triangles": p["triangles"]}
        log(f"{L}: +{rng['points']} points, +{rng['triangles']} triangles ("
            + ", ".join(f"{p['name']} {p['points']}" for p in looks["pieces"]) + f"); shade touched on {touched['count']} old points")
    D["crew_kit"] = blk
    return blk, {L: built[L][0].notes for L in LEVELS}


def dump(D, path):
    with open(path + ".tmp", "w") as f:
        json.dump(D, f, separators=(",", ":"))
    os.replace(path + ".tmp", path)


# ============================================================================================ shapes the pieces use
def torso_ground(K, reach=0.12):
    """The cloth a strap lies on: the trunk and collar, and the sleeves' caps at the shoulders (not the arms beyond
    them, the head, the skin or the belt's pouches)."""
    B = K.body
    trunk = B.on("pelvis", "spine_01", "spine_02", "chest", "neck", "clavicle_l", "clavicle_r") & ~B.painted("skin")
    cap = np.zeros(len(B.P), bool)
    for s in "lr":
        j = B.H[B.bone("upperarm_" + s)]
        cap |= B.on("upperarm_" + s) & (np.linalg.norm(B.P - j, axis=1) < reach)
    return B.ground(trunk | cap)


def box(pc, c, ex, ey, ez, paint, tint, stuff, w, role=2):
    """A box about c with half-edges ex, ey, ez (vectors): 8 points, each facing out along its corner."""
    ids = {}
    for i in (-1, 1):
        for j in (-1, 1):
            for k in (-1, 1):
                ids[(i, j, k)] = pc.add(c + i * ex + j * ey + k * ez, i * unit(ex) + j * unit(ey) + k * unit(ez),
                                        paint, tint, stuff, role, w)
    for ax in range(3):
        for s in (-1, 1):
            q = []
            for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                key = [a, b]
                key.insert(ax, s)
                q.append(ids[tuple(key)])
            pc.quad(*q)
    return ids


def stud(K, pc, ground, at, r, h, paint, tint, stuff, seg=6):
    """A round stud standing on the ground at `at` (a point near it): a foot ring on the cloth, a top ring, a crown."""
    hit, out = K.touch(ground, np.asarray(at, float)[None])
    c, n = hit["pt"][0], out[0]
    W = K.body.ride(ground, c[None], K.poses)[0][0]
    a = np.array((1.0, 0, 0)) if abs(n[0]) < 0.9 else np.array((0, 1.0, 0))
    e1 = unit(np.cross(n, a))
    e2 = np.cross(n, e1)
    foot, top = [], []
    for k in range(seg):
        th = 2 * math.pi * k / seg
        rad = math.cos(th) * e1 + math.sin(th) * e2
        hf, of = K.touch(ground, (c + rad * r + n * 0.004)[None])
        foot.append(pc.add(hf["pt"][0] + of[0] * FOOT, rad + n * 0.2, paint, tint, stuff, 0, W))
        top.append(pc.add(c + rad * (r * 0.72) + n * h, n + rad * 0.6, paint, tint, stuff, 1, W))
    pc.strip(foot, top, closed=True)
    pc.fan(top, pc.add(c + n * (h + r * 0.18), n, paint, tint, stuff, 1, W))
    return c, n


def clear_of(K, pts, dist):
    """Which of pts are at least `dist` from every piece built so far."""
    Ps, Ts, n0 = [], [], 0
    for pc in K.pieces:
        P, N, T = pc.arrays()
        if len(T):
            Ps.append(P)
            Ts.append(T + n0)
            n0 += len(P)
    if not Ps:
        return np.ones(len(pts), bool)
    return np.abs(Surface(np.vstack(Ps), np.vstack(Ts)).nearest(pts)["sd"]) >= dist


def runs(keep, least=3):
    """The runs of stations kept (each at least `least` long), as index lists."""
    out, cur = [], []
    for i, k in enumerate(keep):
        if k:
            cur.append(i)
        else:
            if len(cur) >= least:
                out.append(cur)
            cur = []
    if len(cur) >= least:
        out.append(cur)
    return out


def part_of(st, idx):
    """Some of a draped line's stations as an open line of their own."""
    so = {k: (v[idx] if isinstance(v, np.ndarray) else v) for k, v in st.items()}
    so["closed"] = False
    so["along"] = unit(np.gradient(so["at"], axis=0))
    return so


def arc(P, closed=False):
    """Distance along a line of stations from its first."""
    seg = np.linalg.norm(np.diff(np.vstack([P, P[:1]]) if closed else P, axis=0), axis=1)
    return np.concatenate([[0.0], np.cumsum(seg)])[:len(P)], float(seg.sum())


def zone(P, centre, core, ease, closed=False):
    """A share (0 to 1) a station of a line: 1 within `core` of the station `centre` along the line, easing to 0 over
    `ease` beyond (smoothstep)."""
    s, total = arc(P, closed)
    d = np.abs(s - s[centre])
    if closed:
        d = np.minimum(d, total - d)
    u = np.clip((d - core) / max(ease, 1e-9), 0.0, 1.0)
    return 1.0 - u * u * (3.0 - 2.0 * u)


def chain_weights(K, st, exclude=(), ramp=0.07, grid=0.05, least=0.02, jump=0.3, swap="jump", bar=None, width=None):
    """A strap's bone weights, one set a station: every point of a cross-section rides as one, so the strap's width
    stays whole and only its length follows the cloth. Each station rides two bones of the cloth under it (never one
    of `exclude` or the gun's; a bone the cloth there carries less than `least` of is not taken) and a share between
    them. The pairs and shares along the strap are fitted together (least summed cost along the strap): a station's
    cost is where the cloth carries it over the clips (Level.ride's measure: the worst key's distance, a distance
    under the cloth counted three times, each key weighted by K.pose_w); the share moves at most step/ramp from one
    station to the next (a change from one bone to the next spread over `ramp` of strap or more), and the pair
    changes only where the bone the two pairs share carries the strap all but `jump` (so at a change of pair the
    other bones' weights step by `jump` at most; with swap="ramp" also by no more, the bone that leaves and the bone
    that comes together, than the share may move between those two stations, so a change of pair is eased as a
    change of share is). bar: {bone: one True or False a station}, the stations where that bone may not carry the
    strap at all. width (default st["width"]): the cross-section is judged whole, its middle and its two edges (each
    against the cloth under itself), a station's cost the worst of the three, so that where the cloth's bones change
    across the strap the one set of weights is the best for both edges, not only for the middle. Returns the weights
    (stations x bones), each station's distance from the cloth's way at its fit (slip, the worst key unweighted) and
    the bones the strap rides whole, in order along it."""
    B = K.body
    S = st["ride"]
    Q = np.asarray(st["at"], float)
    n = len(Q)
    width = st.get("width") if width is None else width
    probes = [Q]                                 # (the middle of each cross-section, and its two edges)
    if width and "out" in st and "along" in st:
        side = unit(np.cross(st["out"], st["along"]))
        probes += [Q + side * (0.5 * width), Q - side * (0.5 * width)]
    hits = [S.nearest(q) for q in probes]
    tris = [S.T[h["tri"]] for h in hits]
    Wfs = [(B.W[t] * h["bary"][..., None]).sum(1) for h, t in zip(hits, tris)]
    drop = [B.bone(b) for b in tuple(exclude) + GUN_BONES if b in B.names]

    def kept(Wx):
        Wx = Wx.copy()
        Wx[:, drop] = 0.0
        return Wx / np.maximum(Wx.sum(1, keepdims=True), 1e-9)
    Wcs = [kept(x) for x in Wfs]
    Wc = Wcs[0]
    seg = np.linalg.norm(np.diff(Q, axis=0), axis=1)
    W = np.zeros((n, B.nb))
    if K.poses is None:                          # (no clips to judge by: each station the cloth's heaviest two)
        W = top_two(Wc)
        return W, np.zeros(n), sorted(set(int(b) for b in Wc.argmax(1)))
    cand = [int(b) for b in np.where(np.max([x.max(0) for x in Wcs], axis=0) > 0.10)[0]]
    pairs = [(a, b) for k, a in enumerate(cand) for b in cand[k + 1:]]
    if not pairs:
        W[np.arange(n), cand[0] if cand else int(Wc.argmax(1)[0])] = 1.0
        return W, np.zeros(n), cand[:1]
    shares = np.round(np.arange(0.0, 1.0 + 1e-9, grid), 6)
    ns = len(shares)
    GK, XK = K.poses
    pw = np.ones(len(GK)) if K.pose_w is None else np.asarray(K.pose_w, float)
    # where the cloth carries each probe: its foot as the file's points carry it, and the probe's height over the
    # foot turned by the cloth's own blend
    wants, ups, Mbs = [], [], []
    for q, hit, tri, Wf in zip(probes, hits, tris, Wfs):
        foot = np.zeros((len(GK), n, 3))
        for c in range(3):
            v = tri[:, c]
            pa = XK[:, B.part[v]] + np.einsum("knij,nj->kni", GK[:, B.part[v]], B.P[v] - B.H[B.part[v]])
            pb = XK[:, B.part2[v]] + np.einsum("knij,nj->kni", GK[:, B.part2[v]], B.P[v] - B.H[B.part2[v]])
            foot += hit["bary"][None, :, c, None] * ((1.0 - B.w2[v])[None, :, None] * pa + B.w2[v][None, :, None] * pb)
        Wt = Wf / np.maximum(Wf.sum(1, keepdims=True), 1e-9)
        turn = np.einsum("nb,kbij->knij", Wt, GK)
        wants.append(foot + np.einsum("knij,nj->kni", turn, q - hit["pt"]))
        ups.append(unit(np.einsum("knij,nj->kni", turn, hit["n"])))
        Mbs.append({b: XK[:, b][:, None, :] + np.einsum("kij,nj->kni", GK[:, b], q - B.H[b]) for b in cand})
    has = np.any([x > least for x in Wcs], axis=0)
    for bn, m in (bar or {}).items():           # (a bone barred at a station is not had there)
        if bn in B.names:
            has[np.asarray(m, bool), B.bone(bn)] = False
    cost = np.full((n, len(pairs), ns), np.inf)
    off_at = np.zeros((n, len(pairs), ns))
    for pi, (a, b) in enumerate(pairs):
        for si, w in enumerate(shares):
            c, o = np.zeros(n), np.zeros(n)
            for want, up, Mb in zip(wants, ups, Mbs):
                e = want - Mb[a] - w * (Mb[b] - Mb[a])
                off = np.linalg.norm(e, axis=2)
                sk = np.maximum((e * up).sum(2), 0.0)
                c = np.maximum(c, ((off + SINK_COST * sk) * pw[:, None]).max(0))
                o = np.maximum(o, off.max(0))
            ok = (has[:, a] | (w >= 1.0 - 1e-9)) & (has[:, b] | (w <= 1e-9))
            cost[:, pi, si] = np.where(ok, c, np.inf)
            off_at[:, pi, si] = o
    # where a pair's bone carries the strap all but `jump`, the strap may go on in another pair with that bone, at the
    # same weight on it
    pure = {}
    for pi, (a, b) in enumerate(pairs):
        for si, w in enumerate(shares):
            if 1.0 - w >= 1.0 - jump - 1e-9:
                pure.setdefault((a, round(1.0 - w, 4)), []).append((pi, si))
            if w >= 1.0 - jump - 1e-9:
                pure.setdefault((b, round(w, 4)), []).append((pi, si))
    acc = cost[0].copy()
    back = np.zeros((n, len(pairs), ns, 2), int)
    lam = 1e-5
    for i in range(1, n):
        dw = max(1, int(math.floor(seg[i - 1] / ramp / grid + 1e-9)))
        best = np.full((len(pairs), ns), np.inf)
        arg = np.zeros((len(pairs), ns, 2), int)
        for o in range(-dw, dw + 1):              # (along the same pair, the share moving at most dw steps)
            sh = np.full_like(acc, np.inf)
            if o >= 0:
                sh[:, o:] = acc[:, :ns - o]
            else:
                sh[:, :o] = acc[:, -o:]
            sh = sh + lam * abs(o)
            better = sh < best
            best = np.where(better, sh, best)
            si = np.arange(ns)[None, :] - o
            arg[..., 0] = np.where(better, np.arange(len(pairs))[:, None], arg[..., 0])
            arg[..., 1] = np.where(better, si, arg[..., 1])
        for key, states in pure.items():          # (from one pair to another at the same weight on their shared bone)
            if swap == "ramp" and 2.0 * (1.0 - key[1]) > dw * grid + 1e-9:
                continue
            vals = [acc[pi, si] for pi, si in states]
            k = int(np.argmin(vals))
            for pi, si in states:
                v = vals[k] + 10 * lam
                if v < best[pi, si]:
                    best[pi, si] = v
                    arg[pi, si] = states[k]
        acc = cost[i] + best
        back[i] = arg
    flat = int(np.argmin(acc))
    pi, si = divmod(flat, ns)
    path = [(pi, si)]
    for i in range(n - 1, 0, -1):
        pi, si = back[i, pi, si]
        path.append((int(pi), int(si)))
    path = path[::-1]
    slip = np.zeros(n)
    rides = []
    for i, (pi, si) in enumerate(path):
        a, b = pairs[pi]
        w = shares[si]
        W[i, a] += 1.0 - w
        W[i, b] += w
        slip[i] = off_at[i, pi, si]
        whole = a if w < 0.5 else b
        if not rides or rides[-1] != whole:
            rides.append(int(whole))
    return W, slip, rides


def loop_weights(K, st, exclude=(), ramp=0.07):
    """chain_weights for a closed strap: cut at its lowest station (where its two sides meet over one bone) and fitted
    as an open line; the weights put back in the loop's own order."""
    n = len(st["at"])
    iv = int(np.argmin(st["at"][:, 2]))
    order = [(iv + 1 + k) % n for k in range(n)]
    W, slip, chain = chain_weights(K, part_of(st, order), exclude=exclude, ramp=ramp)
    Wl, sl = np.zeros_like(W), np.zeros(n)
    Wl[order], sl[order] = W, slip
    return Wl, sl, chain

# ======================================================================================= the American tank commander
# Tints (the game draws paint x shade x tint; these are the kit's own darkness on paints that are there).
USA_CUP = 0.50          # the earphones' rubber cups, on steel: near black
USA_STRAP = 0.40        # the holster's strap, on leather: dark oiled leather, 25 points of lightness under the jacket
USA_HOLSTER = 0.50
USA_GRIP = 0.42         # the pistol's grip, on wood
USA_BUCKLE = 0.90       # the buckle, on steel
USA_BAND = 0.70         # the goggle strap, on the helmet's own paint
USA_THIN = 0.62         # the thin straps (the microphone's, the glasses'), on leather
USA_SEAM = 0.62         # a seam's welt, on the jacket's own paint
USA_ZIP = 0.70          # the zip, on steel
USA_STUD = 0.80         # snaps, on steel
USA_CUP_PROUD = 0.030   # the ear cups' faces past the helmet's side (the ruling: 2 cm at least; 3 so that the far cup
                        # clears the head's outline from the game's camera behind his left shoulder)
USA_SMOOTH = {"near": 0, "mid": 0, "far": 0}           # (no longer used by the strap: chain_weights instead)
USA_CLEAR = {"near": 0.006, "mid": 0.010, "far": 0.012}  # the most the strap is lifted further, clear of the clips' cloth
USA_CLEAR_FRONT = {"near": 0.012, "mid": 0.014, "far": 0.012}   # the same over the front of the right shoulder, where
                                                                # the prone aim drives the cloth over the strap
USA_SHOULDER = {"near": 0.008, "mid": 0.020, "far": 0.020}      # the strap's standoff over the top of the right shoulder
USA_RAMP = {"near": 0.06, "mid": 0.10, "far": 0.10}             # a change of bone along a strap is spread over this
USA_HOLSTER_CLEAR = 0.004
USA_BACK_STATIONS = {"mid": 10, "far": 7}
USA_STRAP_NOT = ("neck",)                                       # bones the holster's strap never rides
USA_SWAP = "jump"                                               # how its pair of bones changes (chain_weights)
USA_ARM_SIDE = (-0.07, 0.08)       # (y under, x under): his left side and the left of his back below the shoulder
                                   # blade, where the left arm comes down against him in the bail; the coarse levels'
                                   # strap stops short of it (their band stands 1 to 2 cm off the cloth)
USA_THIN_CLEAR = 0.008                                          # the glasses' strap's most lift clear of the cloth
# The glasses' strap's way (its right half, from the back of the neck; the left is its mirror): round the back of the
# neck over the collar, down the side of the chest to a point high on the breastbone
USA_GLASSES_WAY = [(-0.042, 0.0, 1.516), (-0.026, 0.058, 1.512), (0.012, 0.090, 1.506), (0.058, 0.084, 1.492),
                   (0.090, 0.060, 1.468), (0.116, 0.036, 1.444), (0.136, 0.016, 1.420)]

# The holster: on the front of his left chest, left of the zip, its mouth at 1.335 m and its toe 11.5 cm under it
# (1.22 m), slanted 10 degrees (the toe toward his left), flat on the chest (1.6 cm at most), 5.4 cm wide; the pistol's
# butt up out of the mouth and a little toward his middle. The left arm does not come there in any of his clips
# (run, idle, the fire clips, the fall to prone, the hatch and the bail): it hangs against his left side from 1.14 to
# 1.30 m between 40 and 90 degrees round from the front, and reaches the cloth in front of 30 degrees nowhere; the
# thighs come up over the belly below 1.20 m when he kneels. (25 November's holster at 1.11 to 1.32 m and 26 to 69
# degrees round was passed through by the left sleeve in run and idle at every key.)
USA_HOLSTER_AT = (0.146, -0.039, 1.335)
USA_HOLSTER_RUN = (-0.020, 0.115)          # from its mouth's top to its toe, along it, from the strap
USA_HOLSTER_SLANT = 10.0
USA_HOLSTER_HALF = (0.027, 0.027, 0.020)   # its half width at the mouth, a quarter down, the toe
USA_HOLSTER_THICK = (0.016, 0.016, 0.011)  # how far it stands off the chest there
USA_GRIP_WAY = (0.92, 0.40, 0.0)           # the pistol's butt out of the mouth: up, toward his middle, out from the chest

# The holster strap's way round him (guide points about 1 cm off the cloth; the tool lays it on and pulls it taut):
# from the top of the right shoulder, over its middle (y 0.17 to 0.18: the jacket's cloth nearer the collar rides the
# neck, which the strap may not), down across the back on a long slant to his left hip, round his left side low (1.12
# to 1.14 m at the front, clear of the pistol belt's left pouch and under the reach of the hanging left arm), forward
# and up to the holster's toe; the holster itself in the loop (the strap on its toe and on its mouth); from the mouth
# up to the top of the chest and across it at the height of the armpit's top (1.42 m: there the jacket's share on the
# upper arm changes across the chest, not up it, so one set of weights a cross-section serves both its edges), then up
# the front of the shoulder to its top again.
USA_STRAP_WAY = [(0.010, 0.176, 1.482), (-0.035, 0.162, 1.468), (-0.072, 0.118, 1.442), (-0.088, 0.075, 1.405),
                 (-0.121, 0.038, 1.370), (-0.117, 0.000, 1.335), (-0.106, -0.034, 1.298), (-0.105, -0.072, 1.258),
                 (-0.089, -0.109, 1.218), (-0.059, -0.136, 1.180), (-0.029, -0.151, 1.150), (0.003, -0.160, 1.127),
                 (0.037, -0.162, 1.112), (0.073, -0.164, 1.116), (0.104, -0.145, 1.122), (0.125, -0.125, 1.126),
                 (0.137, -0.106, 1.132), (0.145, -0.091, 1.140), (0.151, -0.079, 1.150), (0.156, -0.069, 1.170),
                 (0.161, -0.063, 1.200), (0.165, -0.056, 1.245), (0.164, -0.050, 1.285), (0.156, -0.041, 1.335),
                 (0.144, -0.017, 1.362), (0.131, 0.006, 1.388), (0.117, 0.026, 1.412), (0.112, 0.060, 1.420),
                 (0.104, 0.095, 1.424), (0.095, 0.125, 1.428), (0.086, 0.150, 1.436), (0.074, 0.168, 1.452),
                 (0.052, 0.178, 1.468), (0.030, 0.180, 1.477)]


def usa_ear_cups(K):
    """The tank helmet's earphones: a rubber cup over each ear flap's housing, proud of the helmet's side."""
    B = K.body
    helmet_pts = B.on("head") & B.painted("helmet")
    head = B.ground(B.on("head"))
    P = B.P[helmet_pts]
    side = np.abs(P[:, 1]).max()                       # the helmet's side as seen from behind (the old housings)
    face = P[np.abs(P[:, 1]) > side - 0.004]
    cx, cz = float(face[:, 0].mean()), float(face[:, 2].mean())
    housing = P[np.abs(P[:, 1]) > side - 0.012]
    r_old = float(np.hypot(housing[:, 0] - cx, housing[:, 2] - cz).max())
    proud = USA_CUP_PROUD
    # near and mid: the cup stands on the flap round the old housing, its wall the level's standoff clear of it;
    # far: four points a side (the ruling: 2 to 8 points in all), three on the old housing's own face and one proud.
    if K.L == "near":
        seg = 12
        r0 = max(0.0365, (r_old + K.lift + 0.0005) / math.cos(math.pi / seg))
        rings = [(r0, None), (r0, 0.0100), (r0 - 0.0065, proud - 0.0035)]
    elif K.L == "mid":
        seg = 8
        r0 = max(0.0365, (r_old + K.lift + 0.0005) / math.cos(math.pi / seg))
        rings = [(r0, None), (r0, 0.0120), (r0 - 0.0095, proud)]
    else:
        seg = 3
        rings = [(0.0340, -0.001)]
    pc = K.piece("ear_cups", "The tank helmet's earphones (R-14 receivers in their rubber cups) over the ear flaps' "
                 "housings: a round cup each side, 7 to 8 cm across, standing 3 cm past the helmet's side as seen "
                 "from behind; steel paint with a dark tint (rubber and bakelite, near black). On the head bone. At "
                 "near and mid the cup stands on the flap round the old housing (12 and 8 sides); at far it is "
                 "four points a side, three on the old housing's face and one proud.")
    w = np.zeros(B.nb)
    w[B.bone("head")] = 1.0
    steel = K.paint("steel")
    for sg in (-1.0, 1.0):
        ax = np.array((0.0, sg, 0.0))
        rows = []
        for r, out in rings:
            row = []
            ys = []
            for k in range(seg):
                a = 2 * math.pi * (k + 0.5) / seg
                rad = np.array((math.cos(a), 0.0, math.sin(a)))
                if out is None:        # (the foot: on the flap or whatever lies on it, sunk a millimetre)
                    o = np.array((cx, 0.0, cz)) + rad * r + ax * 0.3
                    t, _ = head.rays(o[None], -ax[None])
                    ys.append(0.3 - t[0] if np.isfinite(t[0]) else np.nan)
                    row.append((rad, r))
                else:
                    p = np.array((cx, 0.0, cz)) + rad * r + ax * (side + out)
                    slope = 0.0 if out < 0 else 0.35 if out < proud - 1e-9 else 0.9
                    row.append(pc.add(p, rad * (1 - slope) + ax * slope, steel, USA_CUP, "rubber", 0 if out < 0 else 1, w))
            if out is None:
                ys = np.array(ys)
                ys = np.where(np.isfinite(ys), ys, np.nanmedian(ys))
                # (where the ray passes the flap through a gap of a coarse level and meets the head's far inside,
                # the foot stays within 3 cm of the rest: the cheek in front of the flap is still met)
                ys = np.clip(ys, np.median(ys) - 0.030, None)
                row = [pc.add(np.array((cx, 0.0, cz)) + rad * r + ax * (y - 0.001), rad, steel, USA_CUP, "rubber", 0, w)
                       for (rad, r), y in zip(row, ys)]
            rows.append(row)
        for a, b in zip(rows[:-1], rows[1:]):
            pc.strip(a, b, closed=True)
        if K.L in ("near", "far"):
            c = pc.add(np.array((cx, 0.0, cz)) + ax * (side + proud), ax, steel, USA_CUP, "rubber", 1, w)
            pc.fan(rows[-1], c)
        else:
            pc.fan(rows[-1])
    K.notes["ear_cups"] = {"helmet_side_m": round(float(side), 4), "cup_out_to_m": round(float(side + proud), 4),
                           "proud_of_helmet_side_m": proud, "old_housing_radius_m": round(r_old, 4),
                           "cup_radius_m": round(float(rings[0][0]), 4), "centre_x_z": [round(cx, 4), round(cz, 4)]}


def strap_back(st, lo=-0.03):
    """The stations of a holster strap's way (as USA_STRAP_WAY runs: from the top of the right shoulder) on his back:
    the first run whose feet are behind x = lo."""
    b = st["foot"][:, 0] < lo
    i0 = int(np.argmax(b))
    i1 = i0
    while i1 < len(b) and b[i1]:
        i1 += 1
    return i0, i1


def straight_back(K, ground, st, near_back, lift, stations=10):
    """A coarse level's strap across the back made straight: the near level's centreline from the right shoulder
    blade to the left underarm (eased, so the near strap's small steps over the spine are not copied), cut in
    `stations` equal steps and laid on this level's cloth along ONE direction (the near run's mean outward, about
    straight back): each station is put where that direction meets this level's cloth and stood off by `lift` at
    least (its middle, its two edges and the faces between stations), the stand-off eased along the run, then the
    band drawn taut over the bumps of his back (it bridges the hollows between them). Seen from behind the band is then the near strap's line whatever the coarse cloth's facets do under it. Returns the strap's
    stations with the back run replaced."""
    at_n, out_n = near_back
    a, b = strap_back(st)
    C = np.array(at_n, float)
    for _ in range(6):                           # (eased: a 5-station mean, the ends kept)
        Cn = C.copy()
        Cn[2:-2] = (C[:-4] + C[1:-3] + C[2:-2] + C[3:-1] + C[4:]) / 5.0
        Cn[1], Cn[-2] = (C[0] + C[1] + C[2]) / 3.0, (C[-3] + C[-2] + C[-1]) / 3.0
        C = Cn
    s, total = arc(C)
    want = np.linspace(0.0, total, stations)
    P = np.stack([np.interp(want, s, C[:, k]) for k in range(3)], 1)
    O = unit(np.stack([np.interp(want, s, out_n[:, k]) for k in range(3)], 1))
    d0 = unit(np.asarray(out_n, float).mean(0))  # (the one direction the run is laid on the cloth along)
    t, _ = ground.rays(P + d0 * 0.3, np.tile(-d0, (len(P), 1)), reach=0.6)
    hit = np.where(np.isfinite(t)[:, None], P + d0 * (0.3 - np.where(np.isfinite(t), t, 0.3))[:, None],
                   ground.nearest(P)["pt"])
    P = hit
    O = unit(O + 2.0 * d0)                       # (the band's face: about the way it is laid on)
    along = unit(np.gradient(P, axis=0))
    side = unit(np.cross(O, along))
    half = 0.5 * st.get("width", 0.032)
    h = np.full(len(P), lift)
    for _ in range(8):
        Q = P + d0 * h[:, None]
        tests = [Q, Q + side * half, Q - side * half]
        need = np.max([lift + 5e-4 - ground.nearest(t)["sd"] for t in tests], axis=0)
        mids = 0.5 * (Q[1:] + Q[:-1])
        ms = 0.5 * (side[1:] + side[:-1])
        mneed = np.max([lift + 5e-4 - ground.nearest(t)["sd"] for t in (mids, mids + ms * half, mids - ms * half)],
                       axis=0)
        need[:-1] = np.maximum(need[:-1], mneed)
        need[1:] = np.maximum(need[1:], mneed)
        if need.max() <= 1e-4:
            break
        h = h + np.maximum(need, 0.0) * 1.2
        hh = h.copy()                            # (eased: a raised station lifts its neighbours most of the way)
        hh[1:-1] = np.maximum(h[1:-1], 0.5 * (h[:-2] + h[2:]))
        hh[1:-1] = np.maximum(hh[1:-1], 0.25 * hh[:-2] + 0.5 * hh[1:-1] + 0.25 * hh[2:])
        h = hh
    # (taut: seen along the band the strap bridges the hollows between the bumps of his back, as a strap pulled tight
    # does, so it does not dip and rise over the coarse cloth's facets and bend on the screen when he turns)
    sa, _ = arc(P)
    dep = (P + d0 * h[:, None]) @ d0
    top = []
    for i in range(len(sa)):                     # (the upper hull of the depths along the band)
        while len(top) >= 2 and ((sa[top[-1]] - sa[top[-2]]) * (dep[i] - dep[top[-2]])
                                 - (dep[top[-1]] - dep[top[-2]]) * (sa[i] - sa[top[-2]])) >= 0:
            top.pop()
        top.append(i)
    h = h + np.maximum(np.interp(sa, sa[top], dep[top]) - dep, 0.0)
    P = P + d0 * h[:, None]
    hit = ground.nearest(P)
    out = {}
    for k, v in st.items():
        if isinstance(v, np.ndarray) and len(v) == len(st["at"]):
            continue
        out[k] = v
    n_new = len(P)
    keep_a, keep_b = np.arange(0, a), np.arange(b, len(st["at"]))
    out["at"] = np.vstack([st["at"][keep_a], P, st["at"][keep_b]])
    out["foot"] = np.vstack([st["foot"][keep_a], hit["pt"], st["foot"][keep_b]])
    out["out"] = np.vstack([st["out"][keep_a], O, st["out"][keep_b]])
    for k in ("rise", "slip"):
        out[k] = np.concatenate([st[k][keep_a], np.zeros(n_new), st[k][keep_b]])
    out["W"] = np.vstack([st["W"][keep_a], np.zeros((n_new, st["W"].shape[1])), st["W"][keep_b]])
    c = out["at"]
    out["along"] = unit(np.roll(c, -1, 0) - np.roll(c, 1, 0)) if out["closed"] else unit(np.gradient(c, axis=0))
    out["straight"] = (a, a + n_new)
    out["straight_standoff_mm"] = (round(float(h.min()) * 1000, 1), round(float(h.max()) * 1000, 1))
    return out


def usa_holster(K):
    """The pistol's shoulder holster worn across the body: its strap over the right shoulder and down across the
    back on a slant to the holster at the front left of his lower chest; a buckle on the strap on the back. (No
    headset cord: a cord on the man's bones cannot hang, and in the slump, the falls and the climb out it would stand
    stiff.)"""
    B = K.body
    L = K.L
    ground = torso_ground(K)
    leather, steel, wood = K.paint("leather"), K.paint("steel"), K.paint("wood")
    lift = K.lift * (1.2 if L == "near" else 1.35)
    step = {"near": 0.014, "mid": 0.050, "far": 0.075}[L]
    width = {"near": 0.030, "mid": 0.032, "far": 0.040}[L]
    st = K.drape(ground, USA_STRAP_WAY, step, lift=lift, closed=True, taut=8, fine=0.007, ease=0.5,
                 ease_most=0.005)
    st["width"] = width
    if L == "near":                       # (the near centreline across the back, for the coarser levels' band)
        a, b = strap_back(st)
        K.shared["usa_strap_back"] = (st["at"][a:b].copy(), st["out"][a:b].copy())
    elif "usa_strap_back" in K.shared:          # (mid and far: the back run straight, on the near strap's line)
        st = straight_back(K, ground, st, K.shared["usa_strap_back"], lift, stations=USA_BACK_STATIONS[L])
    n = len(st["at"])
    foot = st["foot"]
    if "straight" in st:                  # (the straight run behind his shoulder rides his back, not his right arm:
        a, b = st["straight"]             # the arm's share there bent the band in the hatch)
        st["off_arm"] = np.zeros(n, bool)
        st["off_arm"][a:b] = foot[a:b, 0] < -0.04
    # ---- over the top of the right shoulder the strap stands further off (the cloth there is pulled 2 to 4 times
    # its length when the arm moves): USA_SHOULDER over 11 cm of strap, eased back over 4 cm each side
    top = np.where(foot[:, 1] > 0.05)[0]
    itop = int(top[np.argmax(foot[top, 2])])
    zn = zone(st["at"], itop, 0.055, 0.04, closed=True)
    extra = zn * np.maximum(USA_SHOULDER[L] - (lift + st["rise"]), 0.0)
    st["at"] = st["at"] + st["out"] * extra[:, None]
    st["rise"] = st["rise"] + extra
    # ---- how far the clips may lift it further: more over the front of the right shoulder
    fz = np.clip((foot[:, 1] - 0.04) / 0.04, 0, 1) * np.clip((foot[:, 0] + 0.03) / 0.04, 0, 1) \
        * np.clip((foot[:, 2] - 1.30) / 0.04, 0, 1)
    st["cap"] = USA_CLEAR[L] + (USA_CLEAR_FRONT[L] - USA_CLEAR[L]) * fz
    about = ("The pistol's shoulder holster (the M3's cut) worn across the body: the holster on the front of his left "
             "chest, left of the zip, from 1.22 to 1.355 m (where his left arm never comes in his clips), flat on the "
             "chest, the pistol's butt up out of its mouth; its 3 cm strap from the mouth up across the chest, over the "
             "right shoulder, down across the back on a long slant to his left hip, round his left side just over the "
             "pistol belt (under the reach of the hanging arm) and up to the holster's toe; a steel buckle on the "
             "strap on the right shoulder blade. Leather with a dark tint (dark oiled leather, 25 points of lightness "
             "under the jacket); the grip on wood, the buckle on steel. At mid the strap is a flat band 1 to 2 cm off "
             "the cloth, its run across the back laid straight along the near strap's line, and it stops on his back "
             "short of his left side, where his left arm comes down against him in the bail (from the holster's "
             "mouth over the shoulder to there); at far only that band across the back (4 cm, flat), stopping at "
             "the same place. The strap's weights are one set a cross-section (it keeps its width) "
             "along a chain of the cloth's bones (never the neck), each change from one bone to the next spread over "
             "6 cm of strap, each cross-section's set fitted to the cloth under its middle and both its edges over his clips (the tank clips, the bail, run, idle, the fire "
             "clips and the fall to prone). Over the top of the right shoulder it stands 8 mm off the cloth (mid and "
             "far 20); where the cloth would come over it in the clips it stands up to 6 mm (mid 10, far 12; 12 and "
             "14 over the front of the shoulder) further off, and the holster up to 4.")
    if L == "far":
        back = np.zeros(n, bool)
        if "straight" in st:              # (from the shoulder down the straight run across the back)
            back[:st["straight"][1]] = True
        else:
            for i in range(n):            # (from the shoulder down the back to his left side: the first of the way)
                if foot[i, 0] > 0.035 and i > 2:
                    break
                back[i] = True
        arm = (foot[:, 1] < USA_ARM_SIDE[0]) & (foot[:, 0] < USA_ARM_SIDE[1])
        if arm[back].any():               # (the band stops at the first station where his left arm comes against him)
            back[int(np.where(back & arm)[0][0]):] = False
        idx = np.where(back)[0]
        if len(idx) >= 3:
            pc = K.piece("holster", about)
            so = part_of(st, idx)
            W, slip, chain = chain_weights(K, so, exclude=USA_STRAP_NOT, ramp=USA_RAMP[L], swap=USA_SWAP,
                                    bar={"upperarm_r": so["off_arm"]} if "off_arm" in so else None)
            K.ribbon(pc, ground, so, width, leather, USA_STRAP, "leather", lift=lift, shape="flat",
                     clear=so["cap"], weights=W)
            K.notes["holster"] = {"strap_stations": len(idx), "chain": [B.names[c] for c in chain],
                                  "strap_slip_most_mm": round(float(slip.max()) * 1000, 1),
                                  "shoulder_standoff_mm": USA_SHOULDER[L] * 1000}
        return
    pc = K.piece("holster", about)
    # ---- the holster's place: its mouth on the strap's way at USA_HOLSTER_AT
    front = np.where((foot[:, 0] > 0.04) & (foot[:, 1] < 0.0))[0]
    ih = int(front[np.argmin(np.linalg.norm(foot[front] - np.array(USA_HOLSTER_AT), axis=1))])
    c0, n0 = foot[ih], st["out"][ih]
    up = np.array((0.0, 0.0, 1.0))
    aft = unit(np.cross(up, n0))                      # level along the chest, toward the back on his left side
    if aft[1] > 0:
        aft = -aft
    slant = math.radians(USA_HOLSTER_SLANT)
    ax = unit(-up * math.cos(slant) + aft * math.sin(slant))
    ax = unit(ax - n0 * (ax @ n0))                    # the holster's length: down and a little back, along the chest
    hs = USA_HOLSTER_RUN
    stations = {"near": 7, "mid": 3}[L]

    def half(s):                                      # half its width along it
        u = (s - hs[0]) / (hs[1] - hs[0])
        return float(np.interp(u, [0.0, 0.25, 1.0], USA_HOLSTER_HALF))

    def thick(s):                                     # flat on the chest
        u = (s - hs[0]) / (hs[1] - hs[0])
        return float(np.interp(u, [0.0, 0.3, 1.0], USA_HOLSTER_THICK))
    rows, Ws, frames = [], [], []
    first = len(pc.P)
    for k in range(stations):
        s = hs[0] + (hs[1] - hs[0]) * k / (stations - 1)
        hit, out = K.touch(ground, (c0 + ax * s + n0 * 0.02)[None])
        cc, nn = hit["pt"][0], out[0]
        cr = unit(np.cross(nn, ax))                   # across it (toward his middle)
        W = B.ride(ground, cc[None], K.poses)[0][0]
        h, th = half(s), thick(s)
        row = []
        spots = [(-1.0, 0, 0.0), (-0.62, 1, 0.80), (0.0, 1, 1.0), (0.62, 1, 0.80), (1.0, 0, 0.0)] if L == "near" else \
                [(-1.0, 0, 0.0), (-0.6, 1, 0.9), (0.6, 1, 0.9), (1.0, 0, 0.0)]
        for f, role, t in spots:
            hb, ob = K.touch(ground, (cc + cr * (h * f) + nn * 0.01)[None])
            if role == 0:
                p, fac = hb["pt"][0] + ob[0] * FOOT, cr * np.sign(f) + nn * 0.2
            else:
                p, fac = hb["pt"][0] + nn * (max(th * t, K.lift) + FOOT), nn + cr * (0.7 * f)
            row.append(pc.add(p, fac, leather, USA_HOLSTER, "leather", role, W))
        rows.append(row)
        Ws.append(W)
        frames.append((cc, nn, cr, h, th))
    for a, b in zip(rows[:-1], rows[1:]):
        pc.strip(a, b)
    for row, sgn in ((rows[0], -1.0), (rows[-1], 1.0)):          # its mouth and its toe, closed
        pc.fan([pc.add(pc.P[i], ax * sgn, leather, USA_HOLSTER * 0.8, "leather", pc.role[i], pc.W[i]) for i in row])
    # ---- the pistol's butt out of the mouth, up and toward his middle, close to the chest
    cc, nn, cr, h, th = frames[0]
    g = unit(-ax * USA_GRIP_WAY[0] + cr * USA_GRIP_WAY[1] + nn * USA_GRIP_WAY[2])
    gs = unit(np.cross(nn, g))
    gc = cc + nn * (th * 0.50) + cr * (h * 0.10) - ax * 0.004 + g * 0.022
    box(pc, gc, g * 0.025, gs * 0.014, nn * 0.008, wood, USA_GRIP, "grip", Ws[0])
    if L == "near":                                   # (the slide's end and hammer beside it)
        sc = cc + nn * (th * 0.50) - cr * (h * 0.38) - ax * 0.010
        box(pc, sc, -ax * 0.012, cr * 0.014, nn * 0.009, steel, 0.85, "steel", Ws[0])
    # (the holster and the pistol pushed off the chest as the clips need it, a few millimetres at most)
    hold = np.arange(first, len(pc.P))
    need = float(K.sink(np.array(pc.P)[hold], top_two(np.array(pc.W)[hold])).max())
    push = min(need + 8e-4, USA_HOLSTER_CLEAR) if need > 0 else 0.0
    for i in hold:
        pc.P[i] = pc.P[i] + n0 * push
    K.notes.setdefault("lifted_mm", {})["holster_itself"] = round(push * 1000, 1)
    # ---- the strap: all the way round, but for where it runs into the holster
    rel = foot - c0
    s_ax, s_cr = rel @ ax, rel @ frames[0][2]
    inside = (s_ax > hs[0]) & (s_ax < hs[1]) & (np.abs(s_cr) < half(hs[0]) - (0.010 if L == "near" else 0.002)) \
        & (np.linalg.norm(rel, axis=1) < 0.12)
    gone = np.where(inside)[0]
    order = [(int(gone.max()) + 1 + k) % n for k in range(n)] if len(gone) else list(range(n))
    order = [i for i in order if not inside[i]]
    caps = (False, False)
    if L == "mid":                        # (from the holster's mouth over the shoulder and down the back, stopping
        arm = (foot[:, 1] < USA_ARM_SIDE[0]) & (foot[:, 0] < USA_ARM_SIDE[1])     # short of his left side)
        k = next((j for j, i in enumerate(order) if arm[i]), len(order))
        if k < len(order):
            order, caps = order[:k], (False, True)
    so = part_of(st, order)
    W, slip, chain = chain_weights(K, so, exclude=USA_STRAP_NOT, ramp=USA_RAMP[L], swap=USA_SWAP,
                                    bar={"upperarm_r": so["off_arm"]} if "off_arm" in so else None)
    K.ribbon(pc, ground, so, width, leather, USA_STRAP, "leather", lift=lift, caps=caps,
             shape="box" if L == "near" else "flat",       # (mid: a band a centimetre off the cloth, no sides)
             clear=so["cap"], weights=W)
    # ---- the buckle on the back, on the right shoulder blade (above the yoke's seam, where the back is smooth)
    back = np.where((so["foot"][:, 0] < -0.05) & (so["foot"][:, 2] > 1.30))[0]
    ib = int(back[np.argmin(np.abs(so["foot"][back, 1] - 0.095))])
    out, along = so["out"][ib], so["along"][ib]
    at = so["at"][ib] + out * so.get("lifted", np.zeros(len(so["at"])))[ib]
    first_buckle = len(pc.P)
    sd = unit(np.cross(out, along))
    Wb = W[ib]
    if L == "near":
        hl, hw, bar, rise = 0.017, width / 2 + 0.004, 0.0045, K.lift
        for da, ds, la, ls in ((-(hl - bar / 2), 0, bar / 2, hw), ((hl - bar / 2), 0, bar / 2, hw),
                               (0, -(hw - bar / 2), hl, bar / 2), (0, (hw - bar / 2), hl, bar / 2),
                               (0.002, 0, bar * 0.4, hw)):       # (its frame's four sides and its bar, on the strap)
            box(pc, at + along * da + sd * ds + out * (rise * 0.5 + 0.0005), along * la, sd * ls, out * (rise * 0.5),
                steel, USA_BUCKLE, "steel", Wb, role=1)
    else:
        box(pc, at + out * (K.lift * 0.5 + 0.0005), along * 0.019, sd * (width / 2 + 0.005), out * (K.lift * 0.5),
            steel, USA_BUCKLE, "steel", Wb, role=1)
    # (the buckle's frame held its foot's height off the cloth everywhere, where the back curves under it)
    bk = np.arange(first_buckle, len(pc.P))
    low = float(ground.nearest(np.array(pc.P)[bk])["sd"].min())
    if low < FOOT:
        for i in bk:
            pc.P[i] = pc.P[i] + out * (FOOT - low)
    K.notes["holster"] = {"strap_stations": len(order), "strap_width_m": width, "chain": [B.names[c] for c in chain],
                          "strap_slip_most_mm": round(float(slip.max()) * 1000, 1),
                          "shoulder_standoff_mm": USA_SHOULDER[L] * 1000,
                          "holster_mouth_at": [round(float(v), 3) for v in c0],
                          "buckle_at": [round(float(v), 3) for v in at]}
    if "straight" in st:
        K.notes["holster"]["straight_back_stations"] = int(st["straight"][1] - st["straight"][0])


def usa_goggle_strap(K):
    """The goggle strap round the back of the helmet, joining the two ends of the strap that is there on its sides."""
    if K.L != "near":
        return
    B = K.body
    helmet = B.ground(B.on("head") & B.painted("helmet", "lens"))
    pc = K.piece("goggle_strap", "The goggles' strap round the back of the helmet, from the end of the strap on one "
                 "side to the end on the other, over the ribs' ends: 2 cm wide, the helmet's own paint with a dark "
                 "tint. On the head bone. Near only.")
    guide = []
    for deg in range(-128, -181, -13):
        guide.append(deg)
    guide = guide + [-d for d in guide[-2::-1]]
    G = []
    for deg in guide:
        a = math.radians(deg)
        z = 1.697 + 0.015 * (180 - abs(deg)) / 52.0
        G.append((0.045 + 0.135 * math.cos(a), 0.135 * math.sin(a), z))
    st = K.drape(helmet, G, 0.012, taut=8)
    K.ribbon(pc, helmet, st, 0.020, K.paint("helmet"), USA_BAND, "web")


def usa_snaps(K):
    """The snaps on the ear flaps: two a side, before the earphone."""
    if K.L != "near":
        return
    B = K.body
    helmet = B.ground(B.on("head") & B.painted("helmet"))
    pc = K.piece("snaps", "The ear flaps' snap fasteners: a steel stud on each flap before the earphone, where the "
                 "flap is there to take it (the tool tries two a side; on this helmet one a side has the flap under "
                 "it); 1 cm across, steel with a dark tint. On the head bone. Near only.")
    made = 0
    for sg in (-1.0, 1.0):
        for x, z in ((0.094, 1.580), (0.090, 1.640)):
            o = np.array((x, sg * 0.3, z))
            t, _ = helmet.rays(o[None], np.array([[0.0, -sg, 0.0]]))
            if not np.isfinite(t[0]) or 0.3 - t[0] < 0.080:
                continue                   # (no flap there)
            stud(K, pc, helmet, (x, sg * (0.3 - t[0] + 0.003), z), 0.005, K.lift * 1.15, K.paint("steel"), USA_STUD, "steel")
            made += 1
    K.notes["snaps"] = {"studs": made}


def usa_throat_mic(K):
    """The throat microphone's strap round the back and sides of the neck, over the collar's top."""
    if K.L != "near":
        return
    B = K.body
    neck = B.ground(B.painted("skin") & B.on("neck", "head") & (B.P[:, 2] < 1.62))
    pc = K.piece("throat_mic", "The throat microphone's strap (T-30) round the neck: 1 cm wide, seen at the sides "
                 "and the back of the neck between the collar and the helmet; its two microphones sit on the throat "
                 "inside the collar and are not drawn. Leather with a dark tint. It rides the neck's skin. Near only.")
    G = []
    for deg in list(range(-98, -181, -16)) + [180 - 16 * k for k in range(1, 6)]:
        a = math.radians(deg)
        G.append((0.012 + 0.075 * math.cos(a), 0.075 * math.sin(a), 1.5545))
    st = K.drape(neck, G, 0.011, taut=6)
    W, slip, chain = chain_weights(K, st, ramp=USA_RAMP[K.L])
    K.ribbon(pc, neck, st, 0.010, K.paint("leather"), USA_THIN, "leather", weights=W)
    K.notes["throat_mic"] = {"chain": [K.body.names[c] for c in chain], "slip_most_mm": round(float(slip.max()) * 1000, 1)}


def usa_glasses_strap(K):
    """The field glasses' strap round the back of the neck over the collar and down the chest."""
    if K.L != "near":
        return
    cloth = torso_ground(K)
    ground = K.over(cloth)                 # (it lies over the holster's strap where they cross)
    pc = K.piece("glasses_strap", "The field glasses' thin strap: round the back of the neck over the collar, down "
                 "either side of the chest to a point high on the breastbone (1.40 m) where the glasses hang (the glasses are "
                 "the game's own prop in his hands and are not drawn). 1 cm wide, leather with a dark tint; it lies "
                 "over the holster's strap where they cross. It rides the jacket's own bones. Near only.")
    # (the V ends high on the chest, at 1.40 m, over the holster's mouth and the pistol's butt, not across them)
    half = USA_GLASSES_WAY
    G = [half[0]] + half[1:] + [(0.146, 0.0, 1.405)] + [(x, -y, z) for x, y, z in half[:0:-1]]
    st = K.drape(ground, G, 0.012, closed=True, taut=5, ride=cloth)
    W, slip, chain = loop_weights(K, st, ramp=USA_RAMP[K.L])
    K.ribbon(pc, ground, st, 0.010, K.paint("leather"), USA_THIN, "leather", clear=USA_THIN_CLEAR, weights=W)
    K.notes["glasses_strap"] = {"chain": [K.body.names[c] for c in chain],
                                "slip_most_mm": round(float(slip.max()) * 1000, 1)}


def usa_seams(K):
    """The jacket's seams that were not there: over each shoulder, round each armhole, and the zip down the front."""
    if K.L != "near":
        return
    B = K.body
    ground = torso_ground(K, reach=0.15)
    jacket = K.paint(KITS["USA_tanker"]["jacket"])
    pc = K.piece("seams", "The winter combat jacket's seams that the mesh did not have, where two bones a point can "
                 "carry them with the cloth: the zip down the front (on steel, broken only where the straps lie over "
                 "it) and what can ride of the shoulder and armhole seams (welts 7 mm wide on the jacket's own paint "
                 "with a dark tint; a run under about 6 cm is left out, so most of the armhole, whose cloth the "
                 "sleeve's rig pulls about, is not drawn). They ride the jacket's own bones. Near only.")
    lines = []
    for sg in (-1.0, 1.0):
        lines.append(("jacket", [(0.006, sg * 0.110, 1.500), (0.010, sg * 0.150, 1.482), (0.012, sg * 0.188, 1.466)]))
        m = unit(np.array((0.0, sg * math.cos(math.radians(20)), -math.sin(math.radians(20)))))   # the armhole's plane
        c = np.array((0.015, sg * 0.192, 1.392))
        e1 = unit(np.array((0.0, 0.0, 1.0)) - m * m[2])
        e2 = np.array((1.0, 0.0, 0.0))
        ring = [c + 0.105 * math.cos(math.radians(d)) * e1 + 0.095 * math.sin(math.radians(d)) * e2
                for d in range(-125, 126, 25)]
        lines.append(("jacket", ring))
    lines.append(("zip", [(0.105, 0.0, 1.440), (0.140, 0.0, 1.400), (0.156, 0.0, 1.330), (0.158, 0.0, 1.250),
                          (0.156, 0.0, 1.180), (0.162, 0.0, 1.110)]))
    made, slips = 0, []
    for kind, G in lines:
        st = K.drape(ground, G, 0.016, taut=3)
        # (not under a strap; a seam only where it can ride its cloth, the zip all the way down the front)
        keep = clear_of(K, st["at"], 0.009) & (st["slip"] < (4 * SLIP if kind == "zip" else SLIP))
        for idx in runs(keep, 3 if kind == "zip" else 5):
            so = part_of(st, idx)
            if kind == "zip":
                K.ribbon(pc, ground, so, 0.007, K.paint("steel"), USA_ZIP, "steel", shape="tent", strap=False)
            else:
                K.ribbon(pc, ground, so, 0.007, jacket, USA_SEAM, "cloth", shape="tent", strap=False)
            made += len(idx)
        slips.append(float(st["slip"].max()))
    K.notes["seams"] = {"stations": made, "slip_most_mm": round(max(slips) * 1000, 1), "left_out_over_mm": SLIP * 1000}


KITS = {
    "USA_tanker": {
        "file": "soldier_USA_tanker",
        "tank": "Sherman",
        "about": ("The American tank commander's kit (crew_kit.py, 25 November 2026; Brian's screenshot of the "
                  "Sherman's commander from behind and above, \"his figure is simple\"; the art director's ruling: the "
                  "headset first, then one strap across the back, the rest only near and never lighter than the cloth "
                  "under it). New points and triangles on the end of near, mid and far (`ranges`; each piece's own in "
                  "`pieces[].at`), all on bones that were there; colours are paints that were there with a dark "
                  "`tint`. `shade_touched`: the old points whose shade and shade_clean were darkened under a piece, "
                  "and what they were. Run crew_kit.py again after anything that rebuilds the man's lists; a tool "
                  "that cuts its own additions off the end of the lists must be run before this one. The headset's cord is left "
                  "out: on the man's bones a cord cannot hang free, and in the slump, the drop and the climb out it "
                  "would stand stiff or pass through him."),
        "pieces": [usa_ear_cups, usa_holster, usa_goggle_strap, usa_snaps, usa_throat_mic, usa_glasses_strap,
                   usa_seams],
        # (the clips the kit is fitted to and checked in: the commander's in the hatch, the bail, and on foot after
        # it; a clip the file does not have yet is left out of both and said so)
        "clips": ["commander_hatch", "commander_roof_mg", "commander_roof_mg_burst", "die_hatch_slump",
                  "die_hatch_drop", "commander_hatch_duck", "commander_hatch_peek", "bail_climb_out",
                  "bail_step_down", "bail_jump_down", "run", "idle", "fire_stand", "fire_kneel", "fire_prone",
                  "go_prone"],
        "hatch_clips": ["commander_hatch", "commander_roof_mg", "commander_roof_mg_burst", "die_hatch_slump",
                        "die_hatch_drop", "commander_hatch_duck", "commander_hatch_peek"],
        "straps": ["holster", "goggle_strap", "throat_mic", "glasses_strap", "headset_cord"],
        "jacket": "jacket",
        "strap_tint": USA_STRAP,
        # (a strap's bones are fitted to the clips' keys, the worst key counting; a key of the hatch clips, where he
        # is seen most, counts in full, the bail's and the on-foot clips' at half)
        "fit_weight": {"hatch": 1.0, "other": 0.5},
    },
}


# ========================================================================================================== checks
def _edges(T):
    e = np.vstack([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]])
    return np.unique(np.sort(e, axis=1), axis=0)


_DIRS = unit(np.array([(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)]
                     + [(i, j, k) for i in (-1, 1) for j in (-1, 1) for k in (-1, 1)], float))


def shut_in(S, Q, reach=0.5):
    """The share of 14 directions in which a ray from each of Q meets the surface: a point truly under the body's
    surface is shut in all round; one before a patch that faces the wrong way is not."""
    out = np.zeros(len(Q))
    for i, q in enumerate(Q):
        Si = S.only(S.near_ball(q, reach))
        if len(Si.T):
            t, _ = Si.rays(np.tile(q, (len(_DIRS), 1)), _DIRS, reach=reach, start=1e-6)
            out[i] = float(np.isfinite(t).mean())
    return out


ARMS = {"arm_l": ("upperarm_l", "lowerarm_l", "hand_l", "fingers_l", "thumb_l"),
        "arm_r": ("upperarm_r", "lowerarm_r", "hand_r", "fingers_r", "thumb_r")}


class ArmTest:
    """New points inside one of his arms in a pose. A point that rides that arm's bones is not tested. A point under
    the arm's surface by more than UNDER is confirmed by shut_in against that arm's triangles alone (12 of 14 rays
    within 0.25 m); then the cloth under it (its nearest point on the body at rest, carried as the cloth is, 1 mm out)
    is tested the same way: if the cloth is inside the arm too, the arm is passing through the jacket and the piece
    only follows it (inherited); if not, the piece itself is in the arm (its own fault)."""

    def __init__(self, old, Pn, an, bn, wn):
        self.old = old
        maj = np.array([np.bincount(r, minlength=old.nb).argmax() for r in old.part[old.T]])
        self.groups = {}
        for g, names in ARMS.items():
            ids = [old.bone(x) for x in names if x in old.names]
            S = old.surface.only(np.isin(maj, ids))
            rides = np.isin(an, ids) | (np.isin(bn, ids) & (np.asarray(wn) > 0))
            self.groups[g] = (S, ~rides, np.unique(S.T))
        hit = old.surface.nearest(Pn)
        self.home = hit["tri"]
        self.home_pts = old.T[hit["tri"]]
        self.home_bary = hit["bary"]

    def __call__(self, Wo, Wn, Sm):
        """Wo, Wn: the old and new points posed; Sm: the old surface posed. Returns {group: (own points, their
        depths, inherited points)}."""
        out = {}
        for g, (S, test, used) in self.groups.items():
            if len(S.T) == 0:
                continue
            Sp = S.moved(Wo)
            lo, hi = Wo[used].min(0) - 0.05, Wo[used].max(0) + 0.05
            cand = np.where(test & np.all((Wn > lo) & (Wn < hi), axis=1))[0]
            own, dep, inh = np.zeros(0, int), np.zeros(0), np.zeros(0, int)
            if len(cand):
                sd = Sp.nearest(Wn[cand])["sd"]
                k = sd < -UNDER
                deep, d = cand[k], -sd[k]
                if len(deep):
                    shut = shut_in(Sp, Wn[deep], reach=0.25) >= 0.85
                    deep, d = deep[shut], d[shut]
                if len(deep):
                    cloth = (Wo[self.home_pts[deep]] * self.home_bary[deep][..., None]).sum(1)
                    cloth = cloth + Sm.n[self.home[deep]] * 0.001
                    also = shut_in(Sp, cloth, reach=0.25) >= 0.85
                    own, dep, inh = deep[~also], d[~also], deep[also]
            out[g] = (own, dep, inh)
        return out


def under_surface(S, Q, bone=None, tri_bone=None, reach=0.05, size=96, confirm=True):
    """How far each of Q is under a (posed) surface: 0 for a point not under it, else the depth (positive); and the
    triangle it is nearest. A run of points at a time against the triangles near it. bone / tri_bone: each point's
    one bone when it is rigid on one (else -1) and each triangle's likewise: a rigid point's own bone's triangles
    are left out (they move as it does; the rest pose has answered for them). confirm: a point under by more than
    UNDER is confirmed by shut_in (the rest pose's check; in the clips only the rigid points are)."""
    depth = np.zeros(len(Q))
    tri = np.full(len(Q), -1)
    groups = [np.arange(len(Q))] if bone is None else [np.where(bone == b)[0] for b in np.unique(bone)]
    for g in groups:
        for s in range(0, len(g), size):
            ii = g[s:s + size]
            q = Q[ii]
            c = 0.5 * (q.min(0) + q.max(0))
            r = float(np.linalg.norm(q - c, axis=1).max())
            near = S.near_ball(c, r + reach)
            if bone is not None and bone[ii[0]] >= 0:
                near = near[tri_bone[near] != bone[ii[0]]]
            if len(near) == 0:
                continue
            res = S.only(near).nearest(q)
            depth[ii] = np.maximum(-res["sd"], 0.0)
            tri[ii] = near[res["tri"]]
    # (a rigid point's nearest triangle of another bone can be a far one with its back to it, which says nothing:
    # such a point is confirmed by shut_in always)
    deep = np.where(depth > UNDER)[0] if confirm else \
        (np.where((depth > UNDER) & (bone >= 0))[0] if bone is not None else np.zeros(0, int))
    if len(deep):
        inside = shut_in(S, Q[deep]) >= 0.85
        depth[deep[~inside]] = 0.0
    return depth, tri


def folded(L, S_rest, S_pose, G):
    """Which triangles of a posed surface (S_pose, S_rest's triangles of level L moved) the pose turns inside out:
    the posed normal against the rest normal turned by the triangle's corners' own bones. There the body's cloth has
    folded through itself (two bones a point cannot carry it round a joint), and which side of it is "under" means
    nothing."""
    Gv = (1.0 - L.w2)[:, None, None] * G[L.part] + L.w2[:, None, None] * G[L.part2]
    T = S_rest.T
    Rb = (Gv[T[:, 0]] + Gv[T[:, 1]] + Gv[T[:, 2]]) / 3.0
    return (S_pose.n * np.einsum("tij,tj->ti", Rb, S_rest.n)).sum(1) < 0.0


def bone_masks(part, part2, w2):
    """Each point's bones as bits."""
    m = np.left_shift(np.int64(1), np.asarray(part, np.int64))
    return np.where(np.asarray(w2) > 1e-6, m | np.left_shift(np.int64(1), np.asarray(part2, np.int64)), m)


def rigid_bones(part, part2, w2):
    """Each point's one bone when it rides one only, else -1."""
    return np.where((np.asarray(w2) < 1e-6) | (part == part2), part, -1)


def source_of(name, source=None):
    """The file a kit is built on and checked against: `source` when given, else the Content file."""
    return source or os.path.join(CONTENT, KITS[name]["file"] + ".json")


def check(name, out_dir=STAGE, log=print, clips=True, source=None):
    """The checks on the staged file, printed and returned as the report. source: the file the kit was built on
    (source_of). The faults that say the file itself is malformed (lengths, winding, bones, budget, old values, the
    file without the kit not being its source) are also listed under `structural_faults`: --deliver refuses on them."""
    spec = KITS[name]
    path = os.path.join(out_dir, spec["file"] + ".json")
    raw = open(path).read()
    D = json.loads(raw)
    source = source_of(name, source)
    src = json.load(open(source))
    strip(src)                                  # (the man as he was, whether or not the source has the kit already)
    faults, structural = [], []
    rep = {"file": path, "source": source, "made_by": MADE_BY, "levels": {}, "faults": faults,
           "structural_faults": structural}

    def bad(msg):                               # (a structural fault)
        faults.append(msg)
        structural.append(msg)
    blk = D.get("crew_kit")
    if not blk:
        bad("no crew_kit block in the staged file")
        return rep
    rep["built_on_md5"] = blk.get("built_on_md5")
    rep["source_md5"] = base_md5(src)
    if blk.get("built_on_md5") and blk["built_on_md5"] != rep["source_md5"]:
        bad(f"the kit was built on another file than {source} (md5 {blk['built_on_md5']}, the source without a kit "
            f"is {rep['source_md5']})")
    sep = (",", ":")
    # ---- 1. nothing else changed: the staged file with the kit taken off is the source, byte for byte
    D0 = json.loads(raw)
    strip(D0)
    same = json.dumps(D0, separators=sep) == json.dumps(src, separators=sep)
    other = [k for k in set(src) | set(D0) if json.dumps(D0.get(k), separators=sep) != json.dumps(src.get(k), separators=sep)]
    rep["stripped_equals_source"] = same
    rep["keys_changed_beside_the_kit"] = sorted(other)
    rep["staged_is_compact_json"] = json.dumps(D, separators=sep) == raw
    log(f"1. kit taken off, the staged file is the source byte for byte: {same}"
        + ("" if same else f" (differs in {sorted(other)})"))
    if not same:
        bad(f"the staged file without the kit is not the source (keys {sorted(other)})")
    if not rep["staged_is_compact_json"]:
        bad("the staged file is not written as the source is (compact json)")
    rig = gore.Rig(D["bones"])
    nb = len(D["bones"])
    gunb = [rig.index[g] for g in GUN_BONES if g in rig.index]
    names = paint_names(D)
    cols = D["colours"]
    tank = json.load(open(os.path.join(TANKS, spec["tank"] + ".json")))
    piece_names = [p["name"] for p in blk["pieces"]]

    def drawn(M, i):
        """Points' colours as drawn (paint x shade x tint, the file's own colours), 0 to 255."""
        i = np.asarray(i)
        base = np.array([hexrgb(cols[names[int(p)]]) for p in np.array(M["paints"])[i]])
        return base * np.array(M["shade"])[i][:, None] * np.array(M["tint"]).reshape(-1, 3)[i]

    for L in LEVELS:
        R = {}
        rep["levels"][L] = R
        M, M0 = D[L], src[L]
        r = blk["ranges"][L]
        n0, nn, t0, tn = r["first_point"], r["points"], r["first_triangle"], r["triangles"]
        R["added"] = {"points": nn, "triangles": tn, "budget_points": BUDGET[L]}
        if nn > BUDGET[L]:
            bad(f"{L}: {nn} points added, over the budget of {BUDGET[L]}")
        # ---- 2. old values unchanged (prefix compare), every list the right length
        changed = {}
        for k in ONE + THREE + ("triangles",):
            a, b = M0[k], M[k][:len(M0[k])]
            d = sum(1 for x, y in zip(a, b) if x != y)
            if d:
                changed[k] = d
        touched = blk.get("shade_touched", {}).get(L, {"count": 0, "points": []})
        R["old_values_changed"] = changed
        R["shade_touched"] = touched["count"]
        beyond = {k: v for k, v in changed.items() if k not in ("shade", "shade_clean")}
        if beyond or any(v > touched["count"] for v in changed.values()):
            bad(f"{L}: old values changed beyond the shade under the pieces: {changed}")
        if n0 != len(M0["paints"]) or t0 != len(M0["triangles"]) // 3:
            bad(f"{L}: the kit does not start at the end of the old lists")
        n = n0 + nn
        lens = {k: len(M[k]) for k in ONE + THREE + ("triangles",)}
        want = {**{k: n for k in ONE}, **{k: 3 * n for k in THREE}, "triangles": 3 * (t0 + tn)}
        R["lengths_right"] = lens == want
        if lens != want:
            bad(f"{L}: list lengths {lens}, wanted {want}")
        log(f"2. {L}: +{nn} points (budget {BUDGET[L]}), +{tn} triangles; old values changed: {changed or 'none'}"
            f" (shade touched under the pieces: {touched['count']}); lengths right: {lens == want}")
        if nn == 0:
            continue
        P = np.array(M["points"], float).reshape(-1, 3)
        F = np.array(M["facings"], float).reshape(-1, 3)
        T = np.array(M["triangles"], int).reshape(-1, 3)[t0:]
        part, part2, w2 = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
        # ---- 3. winding against the facings (the soldier files' way), no triangle on an old point or of no area
        g = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
        along = (g * (F[T[:, 0]] + F[T[:, 1]] + F[T[:, 2]])).sum(1)
        wrong = int((along >= 0).sum())
        flat = int((np.linalg.norm(g, axis=1) < 1e-10).sum())
        own = bool(((T >= n0) & (T < n)).all())
        R["winding"] = {"triangles": int(len(T)), "against_the_facing": int(len(T) - wrong), "wrong": wrong,
                        "no_area": flat, "all_on_new_points": own}
        if wrong or flat or not own:
            bad(f"{L}: {wrong} new triangles wound with their facing, {flat} of no area, on new points only: {own}")
        fl = np.linalg.norm(F[n0:], axis=1)
        if np.abs(fl - 1).max() > 0.01:
            bad(f"{L}: a new facing is not a unit ({fl.min():.3f} to {fl.max():.3f})")
        # ---- 4. bones and weights
        okb = bool(((part[n0:] >= 0) & (part[n0:] < nb) & (part2[n0:] >= 0) & (part2[n0:] < nb)).all())
        okw = bool(((w2[n0:] >= 0) & (w2[n0:] <= 0.5)).all())
        ong = int(np.isin(part[n0:], gunb).sum() + (np.isin(part2[n0:], gunb) & (w2[n0:] > 0)).sum())
        R["bones"] = {"valid": okb, "w2_0_to_0.5": okw, "on_the_gun": ong,
                      "used": sorted({rig.names[i] for i in set(part[n0:].tolist()) | set(part2[n0:].tolist())})}
        if not okb or not okw or ong:
            bad(f"{L}: bones valid {okb}, w2 in range {okw}, points on the gun's bones {ong}")
        for k, lo, hi in (("shade", SHADE_FLOOR, 1), ("shade_clean", SHADE_FLOOR, 1), ("dirt", 0, 1), ("dirt_wet", 0, 1),
                          ("wet", 0, 1), ("snow", 0, 1), ("tint", 0, 2)):
            v = np.array(M[k][(3 * n0 if k == "tint" else n0):], float)
            if v.min() < lo - 1e-9 or v.max() > hi + 1e-9:
                bad(f"{L}: new {k} out of range ({v.min()} to {v.max()})")
        pn = set(np.array(M["paints"])[n0:].tolist())
        if not pn <= set(np.array(M0["paints"]).tolist()) | set(names):
            bad(f"{L}: a paint that is not in the file: {pn}")
        log(f"3. {L}: winding against the facing on {len(T) - wrong} of {len(T)} new triangles; none of no area: {flat == 0}")
        log(f"4. {L}: bones valid {okb} ({', '.join(R['bones']['used'])}); w2 within 0 to 0.5: {okw}")
        # ---- 5. the rest pose: nothing under the body's surface; faces along the cloth held off it
        old = Level(src, L)
        res = {"sd": -under_surface(old.surface, P[n0:])[0]}
        deep = float(-res["sd"].min())
        under = int((res["sd"] < -UNDER).sum())
        tc = P[T].mean(1)
        tn_ = unit(-g)                                          # (outward: against the winding)
        rc = old.surface.nearest(tc)
        lying = ((tn_ * rc["n"]).sum(1) > 0.8) & (rc["sd"] < 0.03)
        low = float(rc["sd"][lying].min()) if lying.any() else 9.0
        R["rest"] = {"deepest_under_mm": round(max(deep, 0) * 1000, 2), "points_under_2mm": under,
                     "lowest_face_along_the_cloth_mm": round(low * 1000, 2), "standoff_mm": STANDOFF[L] * 1000,
                     "faces_along_the_cloth": int(lying.sum())}
        if under:
            w = np.where(res["sd"] < -UNDER)[0]
            faults.append(f"{L}: rest pose: {under} new points under the body's surface by more than 2 mm (deepest {deep * 1000:.1f} mm)")
            R["rest"]["under_pieces"] = sorted({_piece_at(blk, L, n0 + int(i)) for i in w})
        tpid = np.array([_piece_at(blk, L, int(T[k, 0])) for k in range(len(T))])
        R["rest"]["lowest_face_by_piece_mm"] = {nm: round(float(rc["sd"][lying & (tpid == nm)].min()) * 1000, 2)
                                                for nm in piece_names if (lying & (tpid == nm)).any()}
        if low < STANDOFF[L] - 2e-4:
            lowp = [nm for nm, v in R["rest"]["lowest_face_by_piece_mm"].items() if v < STANDOFF[L] * 1000 - 0.2]
            faults.append(f"{L}: a face along the cloth is {low * 1000:.1f} mm off it (wanted {STANDOFF[L] * 1000:.0f}; pieces {lowp})")
        log(f"5. {L}: rest pose: deepest new point under the body {max(deep, 0) * 1000:.2f} mm ({under} past 2 mm); "
            f"lowest face along the cloth {low * 1000:.1f} mm off it (wanted {STANDOFF[L] * 1000:.0f})")
        # ---- 6. the look: lightness (CIE L*, 0 to 100) of what is drawn
        look = {}
        for p in blk["pieces"]:
            at = p["at"].get(L)
            if at:
                i = np.arange(at["first_point"], at["first_point"] + at["points"])
                look[p["name"]] = round(float(np.mean([lightness(c) for c in drawn(M, i)])), 1)
        jk = [k for k, v in names.items() if v == spec["jacket"]]
        back = np.where(np.isin(np.array(M0["paints"]), jk) & (old.P[:, 0] < -0.03) & (old.P[:, 2] > 1.15))[0]
        jl = float(np.mean([lightness(c) for c in drawn(M, back)])) if len(back) else float("nan")
        look["jacket_back"] = round(jl, 1)
        R["lightness"] = look
        hp = old.P[old.on("head") & old.painted("helmet")]
        cup = blk["pieces"][piece_names.index("ear_cups")]["at"].get(L) if "ear_cups" in piece_names else None
        if cup:
            cp = P[cup["first_point"]:cup["first_point"] + cup["points"]]
            pr = [float(np.abs(cp[cp[:, 1] * sg > 0][:, 1]).max() - np.abs(hp[hp[:, 1] * sg > 0][:, 1]).max()) for sg in (-1, 1)]
            R["ear_cups_proud_of_helmet_side_mm"] = [round(v * 1000, 1) for v in pr]
            cl = max(lightness(c) for c in drawn(M, np.arange(cup["first_point"], cup["first_point"] + cup["points"])))
            R["ear_cups_lightest"] = round(cl, 1)
            if min(pr) < 0.02:
                faults.append(f"{L}: an ear cup stands {min(pr) * 1000:.1f} mm past the helmet's side (wanted 20)")
            if cl >= 30:
                faults.append(f"{L}: an ear cup point's lightness is {cl:.1f} (wanted under 30)")
        hol = blk["pieces"][piece_names.index("holster")]["at"].get(L) if "holster" in piece_names else None
        if hol:
            i = np.arange(hol["first_point"], hol["first_point"] + hol["points"])
            lea = i[(np.array(M["paints"])[i] == [k for k, v in names.items() if v == "leather"][0])
                    & (np.abs(np.array(M["tint"]).reshape(-1, 3)[i, 0] - spec["strap_tint"]) < 1e-6)]
            sl = float(np.mean([lightness(c) for c in drawn(M, lea)]))
            R["strap_lightness"] = round(sl, 1)
            R["strap_darker_than_jacket_by"] = round(jl - sl, 1)
            if L != "far" and jl - sl < 25:
                faults.append(f"{L}: the strap is {jl - sl:.1f} points of lightness darker than the jacket (wanted 25)")
        log(f"6. {L}: lightness drawn: {look}; ear cups proud of the helmet's side "
            f"{R.get('ear_cups_proud_of_helmet_side_mm')} mm; strap under the jacket by {R.get('strap_darker_than_jacket_by')}")
        if not clips:
            continue
        # ---- 7. in the clips, at their own keys: under the body's surface, strap edges' stretch, the hatch ring
        Pn, an, bn, wn = P[n0:], part[n0:], part2[n0:], w2[n0:]
        rb = rigid_bones(an, bn, wn)
        ob = rigid_bones(old.part, old.part2, old.w2)
        tb = np.where((ob[old.T] == ob[old.T][:, :1]).all(1), ob[old.T][:, 0], -1)
        E = _edges(T) - n0
        rest_len = np.linalg.norm(Pn[E[:, 0]] - Pn[E[:, 1]], axis=1)
        pid = np.array([piece_names.index(_piece_at(blk, L, n0 + i)) for i in range(nn)])
        straps = [piece_names.index(s) for s in spec["straps"] if s in piece_names]
        strap_e = np.isin(pid[E[:, 0]], straps) & (rest_len > 0.006)
        # (the cloth each new point rides: its triangle's bones, to tell the point's own cloth from another part
        # of the body passing through it; and that cloth's own edges under the straps, to set their stretch beside)
        om = bone_masks(old.part, old.part2, old.w2)
        main = bone_masks(old.part, old.part, np.zeros(len(old.part)))
        tmain = main[old.T[:, 0]] | main[old.T[:, 1]] | main[old.T[:, 2]]
        ride = old.surface.nearest(Pn)["tri"]
        pmask = (om[old.T[:, 0]] | om[old.T[:, 1]] | om[old.T[:, 2]])[ride] | bone_masks(an, bn, wn)
        CE = _edges(old.T[np.unique(ride[np.isin(pid, straps)])]) if straps else np.zeros((0, 2), int)
        cloth_len = np.linalg.norm(old.P[CE[:, 0]] - old.P[CE[:, 1]], axis=1)
        CE, cloth_len = CE[cloth_len > 0.006], cloth_len[cloth_len > 0.006]
        base = under_surface(old.surface, Pn, rb, tb, confirm=False)[0] * (rb >= 0)
        arm_test = ArmTest(old, Pn, an, bn, wn)
        R["clips"] = {}
        sock = np.array([s["at"] for s in tank["sockets"] if s["name"] == "commander"][0], float)
        for cn in spec["clips"]:
            c = D["clips"][cn]
            ts = gore.clip_times(c)
            worst, worst_at, pairs, ever = 0.0, None, 0, np.zeros(nn, bool)
            cross, cross_most, cloth_most = np.zeros(nn, bool), 0.0, 1.0
            fold_pts, fold_most = np.zeros(nn, bool), 0.0
            st_most, st_at = 1.0, None
            st_piece = {}
            ring_new, ring_old, ring_at = -9.0, -9.0, None
            arm_own, arm_inh, arm_most, arm_at = np.zeros(nn, bool), np.zeros(nn, bool), 0.0, None
            ro = None
            if cn in spec["hatch_clips"]:
                ro = np.array([pl["root_offset"] for pl in c.get("places", []) if pl.get("piece") == spec["tank"]][0], float)
            for t in ts:
                G, X = gore.pose(rig, c, t)
                Wo = gore.skin(old.P, old.part, old.part2, old.w2, G, X, rig.head)
                Wn = gore.skin(Pn, an, bn, wn, G, X, rig.head)
                Sm = old.surface.moved(Wo)
                # (the body's own cloth turned inside out in this pose is left out of the measure: under it means
                # nothing; the points under such a fold alone are counted on their own)
                fold = folded(old, old.surface, Sm, G)
                for g, (own_, dep_, inh_) in arm_test(Wo, Wn, Sm).items():     # (a piece inside his arm)
                    arm_own[own_] = True
                    arm_inh[inh_] = True
                    if len(dep_) and dep_.max() > arm_most:
                        arm_most = float(dep_.max())
                        arm_at = [round(t, 3), g, piece_names[pid[own_[int(dep_.argmax())]]]]
                okt = np.where(~fold)[0]
                dep, tri = under_surface(Sm.only(okt), Wn, rb, tb[okt], confirm=False)
                tri = np.where(tri >= 0, okt[np.maximum(tri, 0)], -1)
                dep = np.maximum(dep - base, 0.0)      # (a rigid point: against how it stood in the rest pose)
                if fold.any():
                    dfo, tfo = under_surface(Sm, Wn, rb, tb, confirm=False)
                    dfo = np.maximum(dfo - base, 0.0)
                    fo = (dfo > UNDER) & (dep <= UNDER) & ((tmain[np.maximum(tfo, 0)] & pmask) != 0)
                    if fo.any():
                        fold_pts |= fo
                        fold_most = max(fold_most, float(dfo[fo].max()))
                u = dep > UNDER
                # (under a triangle none of whose points' first bones is a bone of the cloth the point rides:
                # another part of his body, an arm, is passing through that cloth; the body's own crossing)
                other = u & ((tmain[np.maximum(tri, 0)] & pmask) == 0)
                if other.any():
                    cross |= other
                    cross_most = max(cross_most, float(dep[other].max()))
                sd = -np.where(other, 0.0, dep)
                u = u & ~other
                pairs += int(u.sum())
                if len(CE):
                    cloth_most = max(cloth_most, float((np.linalg.norm(Wo[CE[:, 0]] - Wo[CE[:, 1]], axis=1) / cloth_len).max()))
                ever |= u
                if -sd.min() > worst:
                    worst = float(-sd.min())
                    worst_at = [round(t, 3), piece_names[pid[int(sd.argmin())]]]
                ln = np.linalg.norm(Wn[E[:, 0]] - Wn[E[:, 1]], axis=1) / np.maximum(rest_len, 1e-9)
                for k in np.unique(pid):
                    m = (pid[E[:, 0]] == k) & (rest_len > 0.006)
                    if m.any():
                        st_piece[piece_names[k]] = max(st_piece.get(piece_names[k], 1.0), float(ln[m].max()))
                if strap_e.any() and ln[strap_e].max() > st_most:
                    st_most = float(ln[strap_e].max())
                    j = np.where(strap_e)[0][int(ln[strap_e].argmax())]
                    st_at = [round(t, 3), piece_names[pid[E[j, 0]]]]
                if ro is not None:                    # (the ring: from the socket's axis, from the rim down)
                    for W_, new in ((Wn, True), (Wo[~old.gun], False)):
                        zr = W_[:, 2] + ro[2] - RIM_OVER
                        band = (zr <= 0) & (zr > -RING_DEEP)
                        if band.any():
                            past = float((np.hypot(W_[band, 0] + ro[0], W_[band, 1] + ro[1]) - RING).max())
                            if new and past > ring_new:
                                ring_new, ring_at = past, round(t, 3)
                            if not new:
                                ring_old = max(ring_old, past)
            C = {"keys": len(ts), "deepest_under_mm": round(worst * 1000, 1), "deepest_at": worst_at,
                 "points_ever_under_2mm": int(ever.sum()), "point_keys_under_2mm": pairs,
                 "pieces_under_2mm": sorted({piece_names[k] for k in np.unique(pid[ever])}),
                 "points_by_piece_under_2mm": {piece_names[k]: int((ever & (pid == k)).sum()) for k in np.unique(pid[ever])},
                 "points_inside_another_part_of_the_body": int(cross.sum()),
                 "points_under_only_a_fold_of_the_body": int(fold_pts.sum()),
                 "deepest_under_a_fold_mm": round(fold_most * 1000, 1),
                 "pieces_under_a_fold": sorted({piece_names[k] for k in np.unique(pid[fold_pts])}),
                 "deepest_inside_another_part_mm": round(cross_most * 1000, 1),
                 "points_inside_an_arm_own": int(arm_own.sum()), "deepest_inside_an_arm_mm": round(arm_most * 1000, 1),
                 "deepest_inside_an_arm_at": arm_at,
                 "pieces_inside_an_arm": sorted({piece_names[k] for k in np.unique(pid[arm_own])}),
                 "points_inside_an_arm_with_their_cloth": int(arm_inh.sum()),
                 "strap_stretch_most": round(st_most, 3), "strap_stretch_at": st_at,
                 "cloth_under_the_straps_stretch_most": round(cloth_most, 3),
                 "stretch_by_piece": {k: round(v, 3) for k, v in st_piece.items()}}
            if ro is not None:
                C["ring"] = {"pieces_past_wall_mm": round(ring_new * 1000, 1) if ring_new > -9 else None,
                             "body_past_wall_mm": round(ring_old * 1000, 1) if ring_old > -9 else None, "at": ring_at}
                if ring_new > 0.005 and ring_new > ring_old + 0.005:
                    faults.append(f"{L}: {cn}: the pieces pass the hatch ring's wall by {ring_new * 1000:.0f} mm "
                                  f"(his body by {ring_old * 1000:.0f}) at {ring_at} s")
            R["clips"][cn] = C
            if ever.any():
                faults.append(f"{L}: {cn}: {int(ever.sum())} new points go under the body's surface by more than 2 mm "
                              f"(deepest {worst * 1000:.1f} mm at {worst_at}; pieces {C['pieces_under_2mm']})")
            if arm_own.any():
                faults.append(f"{L}: {cn}: {int(arm_own.sum())} new points go inside his arm while the cloth under them "
                              f"does not (deepest {arm_most * 1000:.1f} mm at {arm_at}; pieces {C['pieces_inside_an_arm']})")
            if fold_pts.any():
                faults.append(f"{L}: {cn}: {int(fold_pts.sum())} new points go more than 2 mm under the body only where "
                              f"its own cloth is turned inside out in the pose (deepest {fold_most * 1000:.1f} mm; "
                              f"pieces {C['pieces_under_a_fold']}): the jacket's skinning, not mended by the kit")
            if st_most > STRETCH:
                faults.append(f"{L}: {cn}: a strap edge stretches to {st_most:.2f} of its rest length at {st_at} "
                              f"(the jacket's own cloth under the straps to {cloth_most:.2f})")
            log(f"7. {L}: {cn} ({len(ts)} keys): deepest under the body {worst * 1000:.1f} mm {worst_at}, "
                f"{int(ever.sum())} points ever past 2 mm ({int(cross.sum())} more inside another part of his body "
                f"that passes through their cloth; {int(fold_pts.sum())} more under only a fold of his own cloth turned "
                f"inside out, deepest {fold_most * 1000:.1f} mm; {int(arm_own.sum())} inside an arm with their cloth outside it, "
                f"deepest {arm_most * 1000:.1f} mm {arm_at or ''}); strap stretch at most {st_most:.3f} {st_at or ''} (the cloth "
                f"under the straps {cloth_most:.3f})"
                + (f"; ring: pieces {C['ring']['pieces_past_wall_mm']} mm past its wall (his body {C['ring']['body_past_wall_mm']})" if ro is not None else ""))
    log("faults: " + ("; ".join(faults) if faults else "none"))
    return rep


def _piece_at(blk, L, i):
    for p in blk["pieces"]:
        at = p["at"].get(L)
        if at and at["first_point"] <= i < at["first_point"] + at["points"]:
            return p["name"]
    return "?"


def build(name, out_dir=STAGE, log=print, source=None):
    """The source file (source_of: the Content file unless another is given) with the kit added, staged; built a
    second time on its own output to show the same bytes."""
    spec = KITS[name]
    src = source_of(name, source)
    D = json.load(open(src))
    had = "crew_kit" in D
    blk, notes = make(D, name, log)
    os.makedirs(out_dir, exist_ok=True)
    p = os.path.join(out_dir, spec["file"] + ".json")
    dump(D, p)
    first = open(p, "rb").read()
    D2 = json.loads(first)
    make(D2, name, log=lambda *a: None)
    again = json.dumps(D2, separators=(",", ":")).encode() == first
    log(f"staged {p} ({len(first)} bytes; the source {'had' if had else 'had no'} kit); a second run on it gives the same bytes: {again}")
    return {"staged": p, "source": src, "bytes": len(first), "second_run_same_bytes": again, "notes": notes,
            "pieces": [{"name": q["name"], "about": q["about"], "levels": sorted(q["at"], key=LEVELS.index)} for q in blk["pieces"]]}


def report(name, out_dir, built=None, log=print, clips=True, source=None):
    spec = KITS[name]
    rep = check(name, out_dir, log, clips=clips, source=source)
    if built:
        rep["build"] = built
        if not built["second_run_same_bytes"]:
            rep["faults"].append("a second run on the staged file does not give the same bytes")
    p = os.path.join(out_dir, spec["file"] + ".crew_kit_report.json")
    with open(p + ".tmp", "w") as f:
        json.dump(rep, f, indent=1)
    os.replace(p + ".tmp", p)
    log("report " + p)
    return rep


def _compact(D):
    return json.dumps(D, separators=(",", ":"))


def landing(name, content=False, out_dir=STAGE):
    """Whether the staged file may go over each destination: it may only when the destination is the file the kit
    was built on (the staged file with the kit taken off), or that same file with an older kit of this tool's on it.
    A destination that differs in anything else holds another tool's work that the staged file does not have:
    writing over it would throw that work away. Returns [(destination, ok, why)]."""
    spec = KITS[name]
    raw = open(os.path.join(out_dir, spec["file"] + ".json")).read()
    base = json.loads(raw)
    strip(base)
    base = _compact(base)
    dests = [SHARED + spec["file"] + ".json"] + ([os.path.join(CONTENT, spec["file"] + ".json")] if content else [])
    out = []
    for d in dests:
        if not os.path.exists(d):
            out.append((d, True, "not there yet"))
            continue
        there = open(d).read()
        if there == raw:
            out.append((d, True, "already the staged file"))
            continue
        X = json.loads(there)
        if _compact(X) == base:
            out.append((d, True, "the file the kit was built on"))
            continue
        if "crew_kit" in X:
            strip(X)
            if _compact(X) == base:
                out.append((d, True, "the file the kit was built on, with an older kit of this tool's"))
                continue
        keys = sorted(k for k in set(X) | set(json.loads(base)) if json.dumps(X.get(k)) != json.dumps(json.loads(base).get(k)))
        out.append((d, False, f"it differs from the file the kit was built on (keys {keys}): another tool's work is "
                               "there that the staged file does not have"))
    return out


def deliver(name, content=False, out_dir=STAGE, log=print):
    """The staged file into the shared folder (and Content/Models/Soldiers with content=True). Nothing is written
    unless the staged file passes the structural checks and every destination is the file the kit was built on
    (landing()); then each destination is backed up, the file written through a temp file beside it (taken away
    again if the write fails) and moved into place, and read back after the shared folder has settled. Returns how
    many destinations were not written or do not read back the same: 0 when all is well."""
    spec = KITS[name]
    src = os.path.join(out_dir, spec["file"] + ".json")
    raw = open(src, "rb").read()
    D = json.loads(raw)
    if "crew_kit" not in D:
        log("NOT DELIVERED: the staged file has no kit")
        return 1
    dests = [SHARED + spec["file"] + ".json"] + ([os.path.join(CONTENT, spec["file"] + ".json")] if content else [])
    lands = landing(name, content, out_dir)
    refused = [(d, why) for d, ok, why in lands if not ok]
    if refused:
        for d, why in refused:
            log(f"LEFT ALONE {d}: {why}; build again from it (--build {name} --source {d}), check it, and deliver "
                "again")
        log("NOT DELIVERED: nothing written")
        return len(dests)
    # (the structural checks against the file it lands on: by landing() that is the file the kit was built on)
    base = next((d for d, ok, why in lands if os.path.exists(d)), None)
    rep = check(name, out_dir, log=lambda *a: None, clips=False, source=base)
    structural = rep.get("structural_faults", ["the checks did not run"])
    if structural or not rep.get("stripped_equals_source", False):
        log("NOT DELIVERED, the staged file fails its structural checks: " + "; ".join(structural or rep["faults"]))
        return len(dests)
    os.makedirs(BACKUP, exist_ok=True)
    stamp = time.strftime("%m%d_%H%M%S", time.gmtime())
    wrote, bad = [], 0
    for d, ok, why in lands:
        if os.path.exists(d):
            where = "content" if d.startswith(CONTENT) else "shared"
            shutil.copy2(d, os.path.join(BACKUP, f"{spec['file']}.json.{where}.{stamp}"))
        tmp = d + ".tmp_crew_kit"
        try:
            with open(tmp, "wb") as f:
                f.write(raw)
            os.replace(tmp, d)
            wrote.append(d)
            log(f"wrote {d} (it was {why})")
        except OSError as e:
            bad += 1
            log(f"NOT WRITTEN {d}: {e}")
        finally:
            if os.path.exists(tmp):
                os.remove(tmp)
    if wrote:
        time.sleep(5)            # (the shared folder is a network mount: read back after it has settled)
    for d in wrote:
        back = open(d, "rb").read()
        same = back == raw
        log(f"read back {d}: {len(back)} bytes, the same as staged: {same}")
        if not same:
            bad += 1
    return bad


def main(argv):
    out = argv[argv.index("--out") + 1] if "--out" in argv else STAGE
    source = argv[argv.index("--source") + 1] if "--source" in argv else None
    quick = "--no-clips" in argv
    for flag in ("--build", "--check", "--deliver"):
        if flag in argv:
            name = argv[argv.index(flag) + 1] if argv.index(flag) + 1 < len(argv) else ""
            if name not in KITS:
                print(f"{flag}: which file? one of {', '.join(KITS)}")
                return 2
    if "--build" in argv:
        name = argv[argv.index("--build") + 1]
        built = build(name, out, source=source)
        rep = report(name, out, built, clips=not quick, source=source)
        return 1 if rep["faults"] else 0
    if "--check" in argv:
        rep = report(argv[argv.index("--check") + 1], out, clips=not quick, source=source)
        return 1 if rep["faults"] else 0
    if "--deliver" in argv:
        return 1 if deliver(argv[argv.index("--deliver") + 1], "--content" in argv, out) else 0
    print(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
