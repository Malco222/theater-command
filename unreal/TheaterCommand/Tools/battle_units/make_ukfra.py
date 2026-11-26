"""The British and French soldiers for the ground battles, until their own are made: the Pole's man (soldier_POL.json,
the nearest cut, which the battles already draw for them) under his own nation's helmet, the British Brodie (Mk II)
or the French Adrian (M26), each with its chin strap, at near, mid and far, and in its field finishes, made exactly as
every other army's are (variety.py's helmet_entry), so the game reads them as it reads any soldier file.

    /home/user/bpyenv/bin/python make_ukfra.py OUTDIR [UK FRA]

The shells are soldier.py's own helmets (battle-models/land/soldier/helmets/<kind>.glb, the head bone's frame), set on
the Pole's head where his wz.31 was, then lifted until no point of his head shows through (the Brodie as drawn sits a
few centimetres into his skull: it was never fitted to a man before), the chin strap stretched up to its new lugs. The
Pole's helmet and its points are taken out and the new shell's put in their place (every index after them moved by
the difference: triangles, and the faces' `at` and `tint_at`). Head points the coarser mid and far shells would cut
are pushed under them. Then his finishes (FINISHES below) and the forehead tucked under the brim, as variety.py does
for every file.

Mended the same night for the art director (23 November):
- The brim. In soldier.py's shells the brim's underside lies on or even above its top (the inside was made by
  offsetting toward a point inside the bowl, which on a wide brim lifts it up to 6 mm), so from above the dark
  underside won and every brim drew near-black. The two sides are parted (thicken) and the underside pushed below the
  top at seven points of every triangle (under_below).
- The Adrian's comb is half as high over the dome and six tenths as wide (comb).
- Far shells are turned on a lathe (lathe_far): a round dome and an even brim, not a decimated tent; the Adrian's comb is
  a low ridge on the mid and far dome (ridge), still showing on a far man.
- Chips are soft olive spots on the rim and the crown edge only (chips_piece), and mud is a soft smear heaviest low
  on the brim and the dome ("smear").
- Strap loops that poked through the brim are tucked 1 mm under it (strap_under).
The wear arrays (shade, tint, dirt, wet, snow) are baked afterwards by ukfra_shade.py (shade.py, as for every soldier
file, then the brim's underside lit to no darker than 0.85).
"""

import copy
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
# variety.py, helmet_finishes.py and human.py are the looks thread's (the o6cg2k branch's Tools/battle_units): put that
# folder beside this one, or name it in LOOKS_TOOLS. The finishes are the same code every other army's file was made by.
sys.path.insert(1, os.environ.get("LOOKS_TOOLS", "/home/user/work/looks/battle_units"))

import bpy  # noqa: E402,F401
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402
from scipy.spatial import cKDTree  # noqa: E402
from scipy.sparse import coo_matrix  # noqa: E402
from scipy.sparse.csgraph import connected_components  # noqa: E402

import variety  # noqa: E402
from glbio import glb, to_json  # noqa: E402
from fitcheck import crossings  # noqa: E402
import ukfra_finishes as UF  # noqa: E402

SOLDIERS = os.environ.get("SOLDIERS", "/mnt/project-files/battle-models/land/soldier/")   # (soldier_POL.json, helmets/)
LODS = ("near", "mid", "far")
OFF = np.array((-0.0005, 0.0, -0.0015))    # (the glb shells against the files' own helmets: the round one sits here)
KINDS = {"UK": ("brodie", 0.032), "FRA": ("adrian", 0.015)}   # (the shell, and how far it is lifted to clear his head)
CHIN = 1.53                                # (below this the chin strap stays where it is: under his chin)
PER_POINT = ("paints", "part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow")
PER_POINT3 = ("points", "facings", "tint")
GAP = 0.003                                # (a head point pushed under the shell goes this far inside it)
FAR = {"brodie": (8, (55.0, 25.0, 4.0)),   # (the far helmet: bearings around, and its rings' heights in degrees from inside:
       "adrian": (10, (55.0, 25.0))}        # the Brodie's shallow dome needs one just above the brim)
R_UNDER = 0.095                            # (the brim's underside: this far out from the helmet's middle and more)
THICK = 0.0015                             # (the brim's steel and paint, top to underside)
CREST = (0.5, 0.6)                         # (the Adrian's comb: its height above the dome, and its width, of soldier.py's)
RIDGE = {"mid": (6, 0.004), "far": (3, 0.005)}   # (the comb at mid and far: lengths, half its width at the foot)


def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


# ------------------------------------------------------------------------------------------------ the finishes' own
# Three things the finishes need that variety.py (the looks thread's, used as it is) does not do for these shells. They
# are put into it from here, for these two files only.
_shell_init = variety.Shell.__init__


def _shell_init_outer(self, data, lod, paints=(variety.PAINT["helmet"],)):
    """Which faces are the helmet's outside, for painting. variety.py asks whether a face looks away from a point
    halfway up inside the helmet, so a wide flat brim's top (looking up, away from the middle sideways) counts as
    inside and takes no dust, wash or mud. Asked from a point under the brim's level instead, a brim's top looks away
    and its underside toward. (Only the painting reads this: the pieces keep variety.py's own outside to lie on.)"""
    _shell_init(self, data, lod, paints)
    low = self.C + np.array((0.0, 0.0, -0.35 * self.H))
    cen = self.P[self.T].mean(1)
    fn = np.cross(self.P[self.T[:, 1]] - self.P[self.T[:, 0]], self.P[self.T[:, 2]] - self.P[self.T[:, 0]])
    self.outer = np.einsum("ij,ij->i", variety._n(fn), variety._n(cen - low)) > 0.0


variety.Shell.__init__ = _shell_init_outer
_paint_field = variety.paint_field


def _paint_field_smear(layer, shell, seed, rimd):
    """A "smear" layer: mud (or grime) rubbed on by hand, heaviest on the brim and the lower dome and thinning to
    nothing on the crown, its edge only a little ragged (variety.py's "mud" is as ragged as camouflage)."""
    if layer["kind"] != "smear":
        return _paint_field(layer, shell, seed, rimd)
    g = variety.blotches(seed + layer.get("seed", 0), layer.get("scale", 0.04))
    hi, slope, rag, lv = layer.get("high", 0.4), layer.get("slope", 2.2), layer.get("rag", 0.35), layer.get("level", 0.0)
    return lambda p, n: slope * (hi - (p[2] - shell.rim) / shell.H) + rag * g(p) - lv


variety.paint_field = _paint_field_smear


def chips_piece(pc, shell, x, inks, seed, lod):
    """A few small chips where the top coat is rubbed through to the olive paint under it, most along the rim (the
    brim's edge, as seen from above), some on the crown's edge, each a little ragged oval of the olive fading out through
    a ring to the coat around it, so it reads as a soft-edged scuff, not a hole. Near only: farther off a chip is less
    than a pixel. (x: "hex" the paint under, "rim" and "crown" how many, "size" its half-length's range in metres;
    "_recipe" the finish's paint, put in by make(), for the colour around each chip.)"""
    if lod != "near":
        return
    rng = np.random.default_rng(seed + 101)
    To = shell.T[shell.outer]
    P = shell.P
    A, B, C = P[To[:, 0]], P[To[:, 1]], P[To[:, 2]]
    G = np.cross(B - A, C - A)
    area = np.linalg.norm(G, axis=1) / 2
    G = G / (2 * area[:, None] + 1e-12)
    M = 8000
    f = rng.choice(len(To), M, p=area / area.sum())
    u, v = rng.random(M), rng.random(M)
    flip = u + v > 1
    u[flip], v[flip] = 1 - u[flip], 1 - v[flip]
    S = A[f] + (B[f] - A[f]) * u[:, None] + (C[f] - A[f]) * v[:, None]
    Ns = G[f]
    q = S - shell.C
    r = np.hypot(q[:, 0], q[:, 1])
    bins = ((np.arctan2(q[:, 1], q[:, 0]) + np.pi) / (2 * np.pi) * 72).astype(int) % 72
    rmax = np.zeros(72)
    np.maximum.at(rmax, bins, r)
    up_ok = Ns[:, 2] > -0.2
    rim = up_ok & (rmax[bins] - r < x.get("rim_width", 0.012)) & (S[:, 2] < shell.rim + 0.5 * shell.H)
    crown = up_ok & (S[:, 2] > shell.top - x.get("crown_depth", 0.03))
    bvh = BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in To])
    # (the colour around: the finish's own paint at that place, as variety.shell_paint lays it)
    rec = x["_recipe"]
    seed0 = sum(ord(c) for c in rec["name"])
    rimd = variety.rim_distance(shell)
    layers = [(L, variety.paint_field(L, shell, seed0, rimd)) for L in rec.get("paint", []) if L["kind"] != "chips"]

    def around(p, n):
        ink = inks(rec["base"]) if rec.get("base") else -1
        for L, fn in layers:
            if not L.get("under", True) and n[2] < -0.45:
                continue
            if fn(p, n) > 0:
                ink = inks(L["hex"])
        return ink
    chip_ink = inks(x["hex"])
    spots = []
    for want, pool in ((x.get("rim", 6), np.where(rim)[0]), (x.get("crown", 2), np.where(crown)[0])):
        rng.shuffle(pool)
        got = 0
        for i in pool:
            if got >= want:
                break
            if all(np.linalg.norm(S[i] - S[j]) > x.get("apart", 0.03) for j in spots):
                spots.append(int(i))
                got += 1
    lift = variety.HELMET_LIFT + 0.0002
    for i in spots:
        o, n = S[i], variety._n(Ns[i])
        X = variety._n(np.cross((0.0, 0.0, 1.0), n)) if abs(n[2]) < 0.95 else np.array((0.0, 1.0, 0.0))
        Y = variety._n(np.cross(n, X))
        t = rng.uniform(0, np.pi)
        X, Y = X * np.cos(t) + Y * np.sin(t), -X * np.sin(t) + Y * np.cos(t)
        a = rng.uniform(*x.get("size", (0.0035, 0.006)))
        b = a * rng.uniform(0.5, 0.85)
        k = 7
        ang = np.arange(k) * 2 * np.pi / k + rng.uniform(0, 0.4, k)
        jig = rng.uniform(0.75, 1.2, k)
        ring_ink = around(o, n)

        def put(dx, dy, ink):
            p = o + X * dx + Y * dy
            hit, n2, _, _ = bvh.ray_cast(Vector(p + n * 0.02), Vector(-n), 0.05)
            if hit is None:
                hit, n2 = Vector(p), Vector(n)
            n2 = variety._n(np.array(n2))
            return pc.add(np.array(hit) + n2 * lift, n2, variety.PAINT["helmet"], ink)
        c0 = put(0.0, 0.0, chip_ink)
        r1 = [put(a * jig[j] * np.cos(ang[j]), b * jig[j] * np.sin(ang[j]), chip_ink) for j in range(k)]
        r2 = [put(1.8 * a * jig[j] * np.cos(ang[j]), 1.8 * b * jig[j] * np.sin(ang[j]), ring_ink) for j in range(k)]
        tris = [(c0, r1[j], r1[(j + 1) % k]) for j in range(k)]
        tris += [(r1[j], r2[j], r2[(j + 1) % k]) for j in range(k)] + [(r1[j], r2[(j + 1) % k], r1[(j + 1) % k]) for j in range(k)]
        for ta, tb, tc in tris:
            va, vb, vc = pc.V[ta], pc.V[tb], pc.V[tc]
            pc.tri(ta, tc, tb) if np.cross(vb - va, vc - va) @ n < 0 else pc.tri(ta, tb, tc)


variety.PIECES["chips"] = chips_piece


# ------------------------------------------------------------------------------------------------ the shells
def decimate(P, N, T, target):
    """The shell with about `target` triangles (Blender's collapse), its points merged first so the cut seams hold."""
    me = bpy.data.meshes.new("shell")
    Pb = P * (1, -1, 1)
    me.from_pydata([tuple(p) for p in Pb], [], [tuple(int(i) for i in t) for t in T])   # (the y flip turns the file's
    #                                                  winding into Blender's: outward normals)
    me.update()
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("shell", me)
    bpy.context.scene.collection.objects.link(ob)
    mod = ob.modifiers.new("dec", "DECIMATE")
    mod.ratio = min(1.0, target / max(1, len(me.polygons)))
    mod.use_collapse_triangulate = True
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    m2 = bpy.data.meshes.new_from_object(ev)
    bm = bmesh.new()
    bm.from_mesh(m2)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.verts.ensure_lookup_table()
    bm.normal_update()
    V = np.array([v.co[:] for v in bm.verts]) * (1, -1, 1)
    Nv = np.array([v.normal[:] for v in bm.verts]) * (1, -1, 1)
    Tt = np.array([[v.index for v in f.verts] for f in bm.faces])
    bm.free()
    bpy.data.objects.remove(ob)
    return V, Nv, Tt


def welded(P):
    """Each point's place: points lying in one place (to a micron) share a number."""
    _, g = np.unique(np.round(P / 1e-6).astype(np.int64), axis=0, return_inverse=True)
    return g.ravel()


def pieces_of(P, T):
    """Which piece of the shell each point is in (points in one place joined): the Adrian's comb is its own."""
    w = welded(P)
    W = w[T]
    n = int(w.max()) + 1
    r = np.concatenate([W[:, 0], W[:, 1], W[:, 2]])
    c = np.concatenate([W[:, 1], W[:, 2], W[:, 0]])
    _, lab = connected_components(coo_matrix((np.ones(len(r)), (r, c)), shape=(n, n)), directed=False)
    return lab[w]


def thicken(P, N, half=THICK / 2):
    """soldier.py's shells draw a brim's top and its underside in one place (each with its own points, facing up and
    down), so whichever is drawn last shows: from above, the dark underside. Part them: where points in one place face
    opposite ways, those facing like the uppermost go up by `half` along its facing, those facing against it go down,
    and the rest (a sharp edge's twin facing sideways) stay."""
    g = welded(P)
    order = np.argsort(g, kind="stable")
    P = P.copy()
    moved = 0
    for grp in np.split(order, np.flatnonzero(np.diff(g[order])) + 1):
        if len(grp) < 2:
            continue
        n = N[grp]
        if (n @ n.T).min() > -0.5:
            continue
        d = n[int(np.argmax(n[:, 2]))]
        s = n @ d
        P[grp[s > 0.3]] += half * d
        P[grp[s < -0.3]] -= half * d
        moved += int((np.abs(s) > 0.3).sum())
    return P, moved


def under_below(P, N, T, log):
    """soldier.py's shells make a helmet's inside by drawing its outside again a little in toward a point inside it, so
    out on a wide brim the underside comes out above the top, up to 4 mm, and its triangles run the other way across the
    top's, so even where their corners are under the top their middles can stand above it: from above one sees the
    underside. Each underside triangle out past the head (facing down, R_UNDER from the middle) is tried at seven
    places against the brim's top straight above; where it is not THICK under it, its corners go down by the shortfall,
    a few times over until none is short."""
    A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    G = -np.cross(B - A, C - A)                    # (the file's winding: clockwise seen from outside)
    G = G / (np.linalg.norm(G, axis=1, keepdims=True) + 1e-12)
    ax = P[np.unique(T), :2].mean(0)
    cen = (A + B + C) / 3
    r = np.hypot(cen[:, 0] - ax[0], cen[:, 1] - ax[1])
    top_t = T[(G[:, 2] > 0.75) & (r > R_UNDER - 0.01)]
    under_t = T[(G[:, 2] < -0.3) & (r > R_UNDER)]
    bvh = BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in top_t])
    W = np.array([(1, 0, 0), (0, 1, 0), (0, 0, 1), (0.5, 0.5, 0), (0, 0.5, 0.5), (0.5, 0, 0.5), (1 / 3, 1 / 3, 1 / 3)])
    W = 0.9 * W + 0.1 / 3                         # (just inside each corner and edge)
    P = P.copy()
    moved = set()
    most = 0.0
    for _ in range(6):
        need = np.zeros(len(P))
        for t in under_t:
            for w in W:
                q = w @ P[t]
                hit = bvh.ray_cast(Vector((float(q[0]), float(q[1]), float(q[2]) + 0.05)), Vector((0.0, 0.0, -1.0)), 0.1)[0]
                if hit is not None:
                    short = q[2] - (hit.z - THICK)
                    if short > 1e-5:
                        need[t] = np.maximum(need[t], short)
        if not need.any():
            break
        for i in np.where(need > 0)[0]:
            P[i, 2] -= need[i]
            moved.add(int(i))
        most = max(most, float(need.max()))
    log(f"   brim: the underside at least {THICK * 1000:.1f} mm under the top everywhere ({len(moved)} points moved down, "
        f"{most * 1000:.1f} mm at most in one go)")
    return P


def dome_height(bvh, x, y, z0):
    """The shell's outside straight below (x, y): the first of its faces met coming down from above."""
    hit = bvh.ray_cast(Vector((float(x), float(y), float(z0) + 0.2)), Vector((0.0, 0.0, -1.0)), 1.0)[0]
    return None if hit is None else float(hit.z)


def comb(P, N, T, log):
    """The Adrian's comb lowered and narrowed (the art director, 23 November: soldier.py's stands a quarter of the
    dome's height and reads as a fin): its height above the dome to CREST[0], its width to CREST[1]. Returns the
    points, which of them are the comb's, and each comb point's height above the dome after the change."""
    lab = pieces_of(P, T)
    cm = lab == lab[int(np.argmax(P[:, 2]))]
    dome_t = T[~cm[T].any(1)]
    bvh = BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in dome_t])
    P = P.copy()
    idx = np.where(cm)[0]
    zd = np.array([dome_height(bvh, P[i, 0], P[i, 1], P[i, 2]) for i in idx], float)
    up = P[idx, 2] - zd
    was = float(up.max())
    P[idx, 2] = zd + np.where(up > 0, CREST[0] * up, up)
    P[idx, 1] = P[idx, 1] * CREST[1]
    log(f"   comb: {len(idx)} points, {was * 100:.1f} cm above the dome at most, now {CREST[0] * was * 100:.1f} cm; "
        f"{2 * float(np.abs(P[idx, 1]).max()) * 1000:.0f} mm across")
    return P, cm, np.where(up > 0, CREST[0] * up, up), dome_t


def outside_only(P, N, T):
    """The faces that look out (away from a point under the brim's level, as the finishes' painting asks): the far
    level keeps only these, so all of its few triangles go to the dome's round and none to its inside."""
    lo, hi = P[np.unique(T)].min(0), P[np.unique(T)].max(0)
    low = np.array(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2] - 0.35 * (hi[2] - lo[2])))
    A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    out = -np.cross(B - A, C - A)                  # (the file's winding: clockwise seen from outside)
    keep = np.einsum("ij,ij->i", out, (A + B + C) / 3 - low) > 0
    return used_only(P, N, T[keep])


def lathe_far(P, N, T, K, rings_at):
    """The far helmet built round, not cut down (a cut-down dome of 50 triangles comes out a pointed tent): the outside
    met by rays from inside it, K bearings around at three heights (the crown, halfway, just above the brim) and the
    crown's top, then the brim's edge (the outline seen from above at each bearing). Outside only, as the far man needs."""
    Po, No, To = outside_only(P, N, T)
    bvh = BVHTree.FromPolygons([tuple(p) for p in Po], [tuple(int(i) for i in t) for t in To])
    lo, hi = Po.min(0), Po.max(0)
    o = np.array(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2] + 0.25 * (hi[2] - lo[2])))
    q = Po - o
    rr = np.hypot(q[:, 0], q[:, 1])
    bear = np.arctan2(q[:, 1], q[:, 0])
    V, Nv = [], []

    def ray(d):
        hit, n, _, _ = bvh.ray_cast(Vector(tuple(o)), Vector(tuple(d)), 1.0)
        if hit is None:
            return None
        n = np.array(n)
        return np.array(hit), n if n @ d > 0 else -n
    apex = ray(np.array((0.0, 0.0, 1.0)))
    V.append(apex[0])
    Nv.append(apex[1])
    rings = []
    for el in rings_at:
        ring = []
        for k in range(K):
            b = 2 * np.pi * k / K
            d = np.array((np.cos(b) * np.cos(np.radians(el)), np.sin(b) * np.cos(np.radians(el)), np.sin(np.radians(el))))
            h = ray(d)
            ring.append(len(V))
            V.append(h[0])
            Nv.append(h[1])
        rings.append(ring)
    edge = []
    for k in range(2 * K):                         # (the brim's edge at twice as many bearings: its outline stays round)
        b = np.pi * k / K
        gap = np.abs(np.angle(np.exp(1j * (bear - b)))) + 10.0 * (q[:, 2] > 0.02)   # (the brim's edge: low down)
        near = gap < 0.06
        if not near.any():
            near = gap <= np.sort(gap)[4]
        j = np.where(near)[0][int(np.argmax(rr[near]))]
        edge.append(len(V))
        V.append(Po[j])
        Nv.append(No[j])
    V, Nv = np.array(V), np.array(Nv)
    Tr = []
    for k in range(K):
        a, b = rings[0][k], rings[0][(k + 1) % K]
        Tr.append(wind(V, (0, a, b), (V[0] + V[a] + V[b]) / 3 - o))
    for r0, r1 in zip(rings[:-1], rings[1:]):
        for k in range(K):
            a, b, c, d = r0[k], r0[(k + 1) % K], r1[k], r1[(k + 1) % K]
            for t in ((a, c, d), (a, d, b)):
                Tr.append(wind(V, t, (V[t[0]] + V[t[1]] + V[t[2]]) / 3 - o))
    last = rings[-1]
    for k in range(K):
        a, b = last[k], last[(k + 1) % K]
        e0, e1, e2 = edge[2 * k], edge[2 * k + 1], edge[(2 * k + 2) % (2 * K)]
        for t in ((a, e0, e1), (a, e1, b), (b, e1, e2)):
            out = (V[t[0]] + V[t[1]] + V[t[2]]) / 3 - o
            Tr.append(wind(V, t, np.array((0.0, 0.0, 1.0)) + 0.3 * out / np.linalg.norm(out)))
    return V, Nv / np.linalg.norm(Nv, axis=1, keepdims=True), np.array(Tr)


def used_only(P, N, T):
    """The points the triangles use, renumbered."""
    u = np.unique(T)
    remap = np.full(len(P), -1)
    remap[u] = np.arange(len(u))
    return P[u], N[u], remap[T]


def wind(P, t, out):
    """A triangle in the file's winding (clockwise seen from outside: its cross product against `out`)."""
    a, b, c = (P[i] for i in t)
    return (t[0], t[2], t[1]) if np.cross(b - a, c - a) @ out > 0 else tuple(t)


def ridge(Pd, Nd, Td, comb_P, comb_up, segs, half_w):
    """The comb at mid or far: a low ridge (a three-sided bar, its top on the middle line) in `segs` lengths, standing on
    that level's own dome, as high above it as the near comb is at each place."""
    bvh = BVHTree.FromPolygons([tuple(p) for p in Pd], [tuple(int(i) for i in t) for t in Td])
    top = comb_up > 0.001
    xs_all = np.round(comb_P[top, 0], 3)
    ux = np.unique(xs_all)
    hx = np.array([comb_up[top][xs_all == x].max() for x in ux])
    x0, x1 = comb_P[:, 0].min() + 0.006, comb_P[:, 0].max() - 0.006
    yc = float(np.median(comb_P[:, 1]))
    V, Nv, Tr = [], [], []
    for x in np.linspace(x0, x1, segs + 1):
        z = dome_height(bvh, x, yc, comb_P[:, 2].max())
        h = max(float(np.interp(x, ux, hx)), 0.004)
        V += [(x, yc - half_w, z - 0.002), (x, yc, z + h), (x, yc + half_w, z - 0.002)]
        Nv += [(0.0, -0.96, 0.28), (0.0, 0.0, 1.0), (0.0, 0.96, 0.28)]
    V, Nv = np.array(V), np.array(Nv)
    for k in range(segs):
        a = 3 * k
        L0, T0, R0, L1, T1, R1 = a, a + 1, a + 2, a + 3, a + 4, a + 5
        for t, o in (((L0, T0, T1), (0, -1, 0.3)), ((L0, T1, L1), (0, -1, 0.3)),
                     ((T0, R0, R1), (0, 1, 0.3)), ((T0, R1, T1), (0, 1, 0.3))):
            Tr.append(wind(V, t, np.array(o)))
    e = 3 * segs
    Tr.append(wind(V, (0, 1, 2), np.array((-1.0, 0, 0))))
    Tr.append(wind(V, (e, e + 1, e + 2), np.array((1.0, 0, 0))))
    return V, Nv, np.array(Tr)


def shells(D, kind, lift, log=print):
    """The new helmet at each level: points, facings, triangles (the file's winding), json frame, on his head. The
    brim is given its steel's thickness (thicken); the Adrian's comb is lowered and narrowed (comb), and at mid and far
    the dome alone is cut down and the comb laid on it as a low ridge (ridge), so it never melts into a peak."""
    hh = np.array([b for b in D["bones"] if b["name"] == "head"][0]["head"])
    g = glb(SOLDIERS + f"helmets/{kind}.glb")["helmet"]
    P = to_json(g["P"]) + hh + OFF + np.array((0.0, 0.0, lift))
    N = to_json(g["N"])
    N = N / np.linalg.norm(N, axis=1, keepdims=True)
    T = np.array(g["T"])
    P, moved = thicken(P, N)
    log(f"   brim: {moved} points parted, the brim {THICK * 1000:.1f} mm thick")
    P = under_below(P, N, T, log)
    out = {}
    if kind != "adrian":
        out["near"] = (P, N, T)
        out["mid"] = decimate(P, N, T, D["helmet_tris"]["mid"][1])
        out["mid"] = (under_below(*out["mid"], log),) + out["mid"][1:]
        out["far"] = lathe_far(P, N, T, *FAR[kind])
        return out
    P, cm, up, dome_t = comb(P, N, T, log)
    out["near"] = (P, N, T)
    comb_P, comb_up = P[cm], up
    Pd0, Nd0, Td0 = used_only(P, N, dome_t)
    for lod in ("mid", "far"):
        segs, hw = RIDGE[lod]
        n_ridge = 4 * segs + 2
        if lod == "far":
            Pd, Nd, Td = lathe_far(Pd0, Nd0, Td0, *FAR[kind])
        else:
            Pd, Nd, Td = decimate(Pd0, Nd0, Td0, D["helmet_tris"][lod][1] - n_ridge)
            Pd = under_below(Pd, Nd, Td, log)
        Vr, Nr, Tr = ridge(Pd, Nd, Td, comb_P, comb_up, segs, hw)
        out[lod] = (np.vstack([Pd, Vr]), np.vstack([Nd, Nr / np.linalg.norm(Nr, axis=1, keepdims=True)]),
                    np.vstack([Td, Tr + len(Pd)]))
        log(f"   {lod}: dome {len(Td)} triangles and the comb's ridge {len(Tr)}")
    return out


def straps(D, kind, lift):
    """The chin strap's points at each level (their indices in the file, and where they go): the strap the Pole's
    helmet hangs moved to the new helmet's lugs, as soldier.py hangs it from each helmet, stretched up by the lift."""
    hh = np.array([b for b in D["bones"] if b["name"] == "head"][0]["head"])
    rs = to_json(glb(SOLDIERS + "helmets/round.glb")["straps"]["P"]) + hh + OFF
    ks = to_json(glb(SOLDIERS + f"helmets/{kind}.glb")["straps"]["P"]) + hh + OFF
    hb = [b["name"] for b in D["bones"]].index("head")
    top = ks[:, 2].max()
    disp = ks - rs
    disp[:, 2] += lift * smooth((ks[:, 2] - CHIN) / (top - CHIN))
    m = D["near"]
    P = np.array(m["points"]).reshape(-1, 3)
    pa, part = np.array(m["paints"]), np.array(m["part"])
    near_idx = np.where((pa == variety.PAINT["straps"]) & (part == hb))[0]
    _, k = cKDTree(rs).query(P[near_idx])
    near_disp = disp[k]
    out = {"near": (near_idx, P[near_idx] + near_disp)}
    tree = cKDTree(P[near_idx])
    for lod in ("mid", "far"):
        m = D[lod]
        Q = np.array(m["points"]).reshape(-1, 3)
        pa, part = np.array(m["paints"]), np.array(m["part"])
        idx = np.where((pa == variety.PAINT["straps"]) & (part == hb))[0]
        _, j = tree.query(Q[idx])
        out[lod] = (idx, Q[idx] + near_disp[j])
    return out


# ------------------------------------------------------------------------------------------------ the splice
def splice(D, lod, newP, newN, newT, strap):
    """The Pole's helmet out of one level and the new shell in: its points where his were, every later index moved."""
    m = D[lod]
    a, c = D["helmet_tris"][lod]
    T = np.array(m["triangles"]).reshape(-1, 3)
    H, B = T[a:a + c], T[:a]
    used = np.unique(H)
    assert not np.isin(B, used).any(), f"{lod}: the body shares the helmet's points"
    n_old = len(m["paints"])
    old_P = np.array(m["points"]).reshape(-1, 3)
    s = int(used.min())
    # the strap first (indices before the splice)
    sidx, sP = strap
    for i, p in zip(sidx, sP):
        m["points"][3 * i:3 * i + 3] = [round(float(v), 4) for v in p]
    # the new points' wear and look from the nearest of the old helmet's (baked again by shade.py afterwards)
    _, near = cKDTree(old_P[used]).query(newP)
    src = used[near]
    k = len(newP)
    gone = np.zeros(n_old, bool)
    gone[used] = True
    before = np.cumsum(gone) - gone                    # (how many of the old helmet's points come before each point)
    remap = np.where(np.arange(n_old) < s, np.arange(n_old), np.arange(n_old) - before + k)
    keep_lo = [i for i in range(s) if not gone[i]]
    keep_hi = [i for i in range(s, n_old) if not gone[i]]
    hb = [b["name"] for b in D["bones"]].index("head")
    for key in PER_POINT:
        if key not in m:
            continue
        old = m[key]
        if key == "paints":
            mid = [variety.PAINT["helmet"]] * k
        elif key in ("part", "part2"):
            mid = [hb] * k
        elif key == "w2":
            mid = [0.0] * k
        else:
            mid = [old[int(i)] for i in src]
        m[key] = [old[i] for i in keep_lo] + mid + [old[i] for i in keep_hi]
    for key in PER_POINT3:
        if key not in m:
            continue
        old = m[key]
        if key == "points":
            mid = [round(float(v), 4) for v in newP.ravel()]
        elif key == "facings":
            mid = [round(float(v), 3) for v in newN.ravel()]
        else:
            mid = [v for i in src for v in old[3 * int(i):3 * int(i) + 3]]
        three = lambda idx: [v for i in idx for v in old[3 * i:3 * i + 3]]
        m[key] = three(keep_lo) + mid + three(keep_hi)
    # indices: the body's triangles, then the new shell's last (as variety.py leaves them)
    B2 = remap[B]
    T2 = np.vstack([B2, newT + s])
    m["triangles"] = [int(i) for i in T2.ravel()]
    D["helmet_tris"][lod] = [int(len(B2)), int(len(newT))]
    # the faces' points
    for f in D.get("faces", []):
        fl = f.get(lod)
        if not fl:
            continue
        for key in ("at", "tint_at"):
            if key in fl:
                arr = np.array(fl[key], int)
                assert not gone[arr].any(), f"a face moves a helmet point ({lod})"
                fl[key] = [int(i) for i in remap[arr]]
    delta = k - len(used)
    n_new = len(m["paints"])
    assert n_new == n_old + delta and len(m["points"]) == 3 * n_new
    return s, k


def push_under(D, lod, log):
    """Head points the shell would cut (beyond its outside, seen from the middle of his head) pushed in under it."""
    m = D[lod]
    a, c = D["helmet_tris"][lod]
    T = np.array(m["triangles"]).reshape(-1, 3)
    P = np.array(m["points"]).reshape(-1, 3)
    pa, part = np.array(m["paints"]), np.array(m["part"])
    hb = [b["name"] for b in D["bones"]].index("head")
    used = np.unique(T[a:a + c])
    bvh = BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in T[a:a + c]])
    hh = np.array([b for b in D["bones"] if b["name"] == "head"][0]["head"])
    o = hh + np.array((0.0, 0.0, 0.015))
    moved = 0
    for i in np.where((part == hb) & (pa != variety.PAINT["helmet"]) & (pa != variety.PAINT["straps"]))[0]:
        if i in used or P[i, 2] < o[2]:
            continue
        if crossings(bvh, o, P[i]) >= 1:
            d = Vector(P[i] - o)
            hit, _, _, dist = bvh.ray_cast(Vector(o), d.normalized(), d.length)
            if hit is None:
                continue
            q = o + np.array(d.normalized()) * max(0.0, dist - GAP)
            m["points"][3 * i:3 * i + 3] = [round(float(v), 4) for v in q]
            moved += 1
    log(f"   {lod}: {moved} head points pushed under the shell")
    return moved


def strap_under(D, lod, log):
    """The chin strap's loops at the lugs (the strap's own points, out under the brim) put a millimetre under the brim's
    underside where they stood through it (as dark dots on the brim's top)."""
    m = D[lod]
    a, c = D["helmet_tris"][lod]
    T = np.array(m["triangles"]).reshape(-1, 3)
    P = np.array(m["points"]).reshape(-1, 3)
    H = T[a:a + c]
    A, B, C = P[H[:, 0]], P[H[:, 1]], P[H[:, 2]]
    G = -np.cross(B - A, C - A)
    down = H[G[:, 2] / (np.linalg.norm(G, axis=1) + 1e-12) < -0.3]
    bvh = BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in down])
    used = np.unique(H)
    ax = P[used, :2].mean(0)
    pa = np.array(m["paints"])
    moved, most = 0, 0.0
    for i in np.where(pa == variety.PAINT["straps"])[0]:
        if P[i, 2] < P[used, 2].min() - 0.01 or np.hypot(P[i, 0] - ax[0], P[i, 1] - ax[1]) < R_UNDER - 0.01:
            continue
        hit = bvh.ray_cast(Vector((float(P[i, 0]), float(P[i, 1]), float(P[i, 2]) - 0.04)), Vector((0.0, 0.0, 1.0)), 0.06)[0]
        if hit is not None and P[i, 2] > hit.z - 0.001:
            most = max(most, P[i, 2] - (hit.z - 0.001))
            m["points"][3 * i + 2] = round(float(hit.z - 0.001), 4)
            moved += 1
    log(f"   {lod}: {moved} strap points put under the brim ({most * 1000:.1f} mm at most)")


def roof(D):
    P = np.array(D["near"]["points"]).reshape(-1, 3)
    a, c = D["helmet_tris"]["near"]
    T = np.array(D["near"]["triangles"]).reshape(-1, 3)[a:a + c]
    return round(float(P[np.unique(T), 2].max()), 3)


# ------------------------------------------------------------------------------------------------ one army
def make(army, outdir, log=print):
    kind, lift = KINDS[army]
    D = json.load(open(SOLDIERS + "soldier_POL.json"))
    for k in ("helmets", "inks"):
        D.pop(k, None)
    sh = shells(D, kind, lift, log)
    st = straps(D, kind, lift)
    for lod in LODS:
        P, N, T = sh[lod]
        was = list(D["helmet_tris"][lod])
        s, k = splice(D, lod, P, N, T, st[lod])
        log(f"   {lod}: {kind} {k} points, {len(T)} triangles (the wz.31's {was[1]})")
    for lod in LODS:
        push_under(D, lod, log)
        strap_under(D, lod, log)
    tucked = variety.tuck_under(D)
    log(f"   forehead tucked under the brim: {tucked}")
    inks = variety.Inks()
    for r in UF.RECIPES[army]:
        for x in r.get("extras", []):
            if x["kind"] == "chips":
                x["_recipe"] = {"name": r["name"], "paint": r.get("paint", []), "base": r.get("base")}
    D["helmets"] = [variety.helmet_entry(D, r, inks) for r in UF.RECIPES[army]]
    # (no "from" or "to" month, whatever variety.py writes: Brian's standing rule, 26 November 2026, a man's
    # markings and paint never hang on the year; finishes_any_year.py)
    for h in D["helmets"]:
        h.pop("from", None)
        h.pop("to", None)
    D["inks"] = inks.list
    D["marks"]["roof"] = roof(D)
    D["name"] = f"soldier_{army}"
    D["about"] = UF.ABOUT[army] + " " + D["about"]
    out = os.path.join(outdir, f"soldier_{army}.json")
    tmp = out + ".tmp"
    with open(tmp, "w") as f:
        json.dump(D, f, separators=(",", ":"))
    json.load(open(tmp))
    os.replace(tmp, out)
    log(f"{army}: {out} ({os.path.getsize(out):,} bytes), finishes {[h['name'] for h in D['helmets']]}, roof {D['marks']['roof']}")
    return D


if __name__ == "__main__":
    a = [x for x in sys.argv[1:] if x != "--"]
    outdir = a[0]
    os.makedirs(outdir, exist_ok=True)
    for army in (a[1:] or ["UK", "FRA"]):
        make(army, outdir)
