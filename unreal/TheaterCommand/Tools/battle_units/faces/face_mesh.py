"""A close-range face with the detail of the man it was cut from (Brian, 23 November 2026: "The faces need a lot of
work").

The battle soldier's near mesh (drawn only within about 9 m of the camera) was thinned to a budget all over, so his face
kept some 600 triangles: too coarse to hold an eyelid, the edge of a lip or a nostril, or a brow in colour. Every
soldier file is cut from MakeHuman's CC0 man for one look (human.look_for(seed, nation); seeds.json says which, found by
fit: the file's face points lie within half a millimetre of that man's), so the detail is still there to take back:

- the near mesh's face (skin on the head bone, from the chin to above the brows, in front of the temples) is split
  red-green: each triangle into four, and where only one edge of a neighbour was split, that neighbour into two, so
  no crack opens. The eyes' and mouth's surroundings and the brows are split once more. Each new point is set on
  MakeHuman's man (along its own facing, else the nearest point of him), so the lids, lips, nostrils and the folds by
  the nose come back. The eyeballs' fronts are split once, on their own spheres.
- every new point takes its neighbours' bone weights, shade, wear (dirt, wet, snow), tint and, in each of the eight
  faces, their move, facing and tint, so the faces keep their shapes; facings are made afresh over the split skin.
- no index moves: each split triangle keeps its place with its first piece, the other pieces and the new points go
  on the end. helmet_tris and the gore twins' ranges stay true.

Mid and far are not touched (the mid face is some 8 pixels tall, its features are colour: face_detail.py).

    /home/user/bpyenv/bin/python face_mesh.py IN.json OUT.json [SEED NATION]

Run after variety.py and shade.py, before face_detail.py. It records what it did under `face_mesh` and will not run
twice on one file.
"""
import json, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from mhhead import Head  # noqa: E402  (bpy first, in there)
import human  # noqa: E402
from mathutils import Vector  # noqa: E402

VERSION = 2
SKIN, BOOTS = 7, 2
PER_POINT = ("points", "facings", "paints", "part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet",
             "snow", "tint")
RAY_MOST = 0.004          # (m) how far along its facing a new point may go to meet MakeHuman's man
NEAREST_MOST = 0.0025     # (m) else, how far to his nearest point
BASE_STUBBLE = 0.4        # (variety's stubble 0.55, heavy 0.85: a clean-shaven man's day's shadow, about 6% darker)
AGREE = 0.3               # (the surface met must face within ~70 degrees of the new point's own facing)


def _n(v):
    v = np.asarray(v, float)
    return v / (np.linalg.norm(v, axis=-1, keepdims=True) + 1e-12)


def sphere(X):
    A = np.c_[2 * X, np.ones(len(X))]
    b = (X ** 2).sum(1)
    s = np.linalg.lstsq(A, b, rcond=None)[0]
    c = s[:3]
    return c, float(np.sqrt(s[3] + c @ c))


class Mesh:
    """One level of a soldier file, as arrays that grow."""

    def __init__(self, m):
        self.n = len(m["points"]) // 3
        self.A = {}
        for k in PER_POINT:
            if k in m:
                a = np.array(m[k], float if k not in ("paints", "part", "part2") else int)
                self.A[k] = a.reshape(self.n, -1) if a.size != self.n else a.reshape(self.n, 1)
        self.T = np.array(m["triangles"], int).reshape(-1, 3)
        self.parents = {}                    # new point -> (a, b)

    @property
    def P(self):
        return self.A["points"]

    def add_mid(self, a, b, pos):
        i = self.n
        for k, a_ in self.A.items():
            if k == "points":
                row = pos
            elif k == "paints":
                row = a_[a]
            elif k in ("part", "part2", "w2"):
                continue
            elif k == "facings":
                row = _n(a_[a] + a_[b])
            else:
                row = (a_[a] + a_[b]) / 2
            self.A[k] = np.vstack([a_, np.asarray(row, a_.dtype).reshape(1, -1)])
        # bone weights: the two ends' weights averaged, the two largest kept
        w = {}
        for j in (a, b):
            p, q, w2 = int(self.A["part"][j, 0]), int(self.A["part2"][j, 0]), float(self.A["w2"][j, 0])
            w[p] = w.get(p, 0.0) + (1 - w2) / 2
            w[q] = w.get(q, 0.0) + w2 / 2
        top = sorted(w.items(), key=lambda kv: -kv[1])[:2]
        p = top[0][0]
        if len(top) > 1 and top[1][1] > 1e-6:
            q, wq = top[1][0], top[1][1] / (top[0][1] + top[1][1])
        else:
            q, wq = p, 0.0
        self.A["part"] = np.vstack([self.A["part"], [[p]]])
        self.A["part2"] = np.vstack([self.A["part2"], [[q]]])
        self.A["w2"] = np.vstack([self.A["w2"], [[wq]]])
        self.parents[i] = (a, b)
        self.n += 1
        return i


def red_green(M, red, place):
    """Split the triangles marked red into four (and close: a neighbour with two split edges is red too, one with a
    single split edge is split into two). place(a, b) gives the new point's position on edge a-b. Returns the
    triangles split and the points added."""
    T = M.T
    red = red.copy()
    E = np.sort(np.stack([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]], 1), axis=2)       # (m, 3, 2)
    key = E[:, :, 0].astype(np.int64) * 10_000_000 + E[:, :, 1]
    while True:
        split = np.isin(key, np.unique(key[red]))
        more = ~red & (split.sum(1) >= 2)
        if not more.any():
            break
        red |= more
    split = np.isin(key, np.unique(key[red]))
    green = ~red & (split.sum(1) == 1)
    mids = {}

    def mid(a, b):
        k = (min(a, b), max(a, b))
        if k not in mids:
            mids[k] = M.add_mid(a, b, place(a, b))
        return mids[k]

    n0 = M.n
    out = [t for t in T]
    extra = []
    for i in np.where(red)[0]:
        a, b, c = (int(v) for v in T[i])
        ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
        out[i] = np.array((a, ab, ca))
        extra += [(ab, b, bc), (ca, bc, c), (ab, bc, ca)]
    for i in np.where(green)[0]:
        t = [int(v) for v in T[i]]
        s = int(np.where(split[i])[0][0])               # (edge s runs from corner s to corner s+1)
        a, b, c = t[s], t[(s + 1) % 3], t[(s + 2) % 3]
        ab = mid(a, b)
        out[i] = np.array((a, ab, c))
        extra.append((ab, b, c))
    M.T = np.vstack([np.array(out), np.array(extra, int).reshape(-1, 3)])
    return int(red.sum()), int(green.sum()), M.n - n0


def vertex_normals(P, T, n):
    """Area-weighted, outward: the file's triangles turn clockwise seen from outside."""
    fn = -np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    vn = np.zeros((n, 3))
    for i in range(3):
        np.add.at(vn, T[:, i], fn)
    return vn


def weld(P):
    """One id per position (to 0.01 mm): points split along a seam share it."""
    key = np.round(P * 1e5).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    return inv.ravel()


def smooth_normals(P, T, sel, passes=2):
    """Outward, area-weighted facings over triangles T, the same at points that share a position, then smoothed over
    the mesh `passes` times; returned for the points `sel` (others zero)."""
    n = len(P)
    cid = weld(P)
    vn = vertex_normals(P, T, n)
    acc = np.zeros((cid.max() + 1, 3))
    np.add.at(acc, cid, vn)
    N = _n(acc[cid])
    rows, cols = graph(T, n)
    deg = np.bincount(rows, minlength=n).astype(float)
    for _ in range(passes):
        s = np.zeros_like(N)
        np.add.at(s, rows, N[cols])
        N2 = _n(N + 0.6 * np.where(deg[:, None] > 0, s / np.maximum(deg, 1)[:, None], 0))
        acc = np.zeros((cid.max() + 1, 3))
        np.add.at(acc, cid, N2)
        N = _n(acc[cid])
    out = np.zeros((n, 3))
    out[sel] = N[sel]
    return out


def front_profile(bvh, y, z0, z1, step=0.00025):
    zs = np.arange(z0, z1, step)
    xs = []
    for z in zs:
        hit = bvh.ray_cast(Vector((0.5, y, z)), Vector((-1.0, 0.0, 0.0)), 1.0)
        xs.append(hit[0].x if hit[0] is not None else np.nan)
    return zs, np.array(xs), None


def mesh_bvh(P, T):
    from mathutils.bvhtree import BVHTree
    return BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in T])


def close_mouth(M, sel_pts, ez, tip_z, rep):
    """Lips at rest meet: the gap between them (the fold into the mouth, seen from the front as a dark slit) is
    closed by drawing the upper lip's edge down and the lower's up to the fold's deepest line, easing off over four
    times the distance (a monotone map, so nothing crosses), fading out toward the corners and deep inside."""
    P = M.P
    T = M.T
    bvh = mesh_bvh(P, T[sel_pts[T].all(1)])
    zs, xs, _ = front_profile(bvh, 0.0, tip_z - 0.055, tip_z - 0.004)
    ok = np.isfinite(xs)
    zs, xs = zs[ok], xs[ok]
    # the upper lip's front (most forward below the nose's base) and the lower lip's, the fold's deepest between
    mid = len(zs) // 2
    # walk from the top (high z) down: zs runs low to high, so reverse
    zr, xr = zs[::-1], xs[::-1]
    i_sub = int(np.argmin(xr[:int(0.016 / 0.00025)]))                   # subnasale: least x under the nose's tip
    i_up = i_sub + int(np.argmax(xr[i_sub:i_sub + int(0.012 / 0.00025)]))  # the upper lip's front
    i_fold = i_up + int(np.argmin(xr[i_up:i_up + int(0.010 / 0.00025)]))  # the fold's deepest
    i_lo = i_fold + int(np.argmax(xr[i_fold:i_fold + int(0.010 / 0.00025)]))
    up_x, lo_x, deep_x = xr[i_up], xr[i_lo], xr[i_fold]
    m = float(zr[i_fold])
    # the gap: from where the upper lip's underside has turned 1.5 mm in from its front, down to where the lower
    # lip comes within 0.5 mm of its own front; the lower lip's edge is drawn up to the line, the upper's underside
    # halfway down (it then faces the ground edge-on, not the camera as a dark band)
    seg_u = np.arange(i_up, i_fold + 1)
    under = seg_u[xr[seg_u] < up_x - 0.0015]
    top = float(zr[under[0]]) if len(under) else m
    seg_l = np.arange(i_fold, i_lo + 1)
    lip = seg_l[xr[seg_l] > lo_x - 0.0005]
    bot = float(zr[lip[0]]) if len(lip) else m
    if top - bot < 0.0005:
        rep["mouth"] = "no gap"
        return m
    s_up, s_lo = 0.5 * max(0.0, top - m), max(0.0, m - bot)
    y, z, x = P[:, 1], P[:, 2], P[:, 0]
    ky = 1 - smooth01((np.abs(y) - 0.010) / 0.016)
    kx = smooth01((x - (deep_x - 0.007)) / 0.005)
    w = ky * kx * sel_pts
    d = z - m
    out = z.copy()
    for s0, side in ((s_up, d > 0), (s_lo, d < 0)):
        if s0 <= 0:
            continue
        a = np.abs(d)
        phi = np.clip(1 - (a - s0) / (3 * s0), 0, 1)
        phi = np.where(a <= s0, 1.0, phi)
        a2 = np.maximum(0.0, a - s0 * phi)
        out = np.where(side, m + np.sign(d) * a2, out)
    moved = w > 0
    P[moved, 2] = z[moved] + (out[moved] - z[moved]) * w[moved]
    rep["mouth"] = dict(subnasale_z=round(float(zr[i_sub]), 4), upper_z=round(float(zr[i_up]), 4), lower_z=round(float(zr[i_lo]), 4),
                        gap_mm=round(1000 * (top - bot), 2), up_mm=round(1000 * s_up, 2), down_mm=round(1000 * s_lo, 2),
                        line_z=round(m, 4), points=int((np.abs(out - z) * w > 1e-5).sum()))
    return m


def smooth01(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


def eye_opening(bvh, ball_tri_ids, c, r):
    """The eyeball seen from the front through its lids: (bottom, top) at its middle, and the width at mid height."""
    zs = np.arange(c[2] - 0.016, c[2] + 0.016, 0.0002)
    vis = []
    for z in zs:
        hit = bvh.ray_cast(Vector((0.5, c[1], z)), Vector((-1.0, 0.0, 0.0)), 1.0)
        vis.append(hit[2] is not None and hit[2] in ball_tri_ids)
    vis = np.array(vis)
    if not vis.any():
        return None
    zv = zs[vis]
    bot, top = float(zv.min()), float(zv.max())
    zm = (bot + top) / 2
    ys = np.arange(c[1] - 0.022, c[1] + 0.022, 0.0002)
    vy = []
    for y in ys:
        hit = bvh.ray_cast(Vector((0.5, y, zm)), Vector((-1.0, 0.0, 0.0)), 1.0)
        vy.append(hit[2] is not None and hit[2] in ball_tri_ids)
    vy = np.array(vy)
    W = float(ys[vy].max() - ys[vy].min()) if vy.any() else 0.026
    return bot, top, W


IRIS_OF_WIDTH = 0.26      # (the iris's radius, of the eye's open width: larger than life, to read; the art director, 0.21 read beady)
OPEN_OF_WIDTH = 0.29      # (the open eye's height, of its width: about 6.2 mm, the lid still over at least the iris's top third)
LID_MOST = 0.0035         # (m) the most an upper lid comes down


def lower_lids(M, sel_skin, eyes, ball_of, rep):
    """The upper lid drawn down over the top third of the iris (the eyes stared: the whites showed all round), and
    each eye's iris placed: its radius from the opening's width, its middle so its foot meets the lower lid. Returns
    each eye's iris (centre direction, angle) for face_detail.py."""
    P = M.P
    out = []
    for k, (c, r, e) in enumerate(eyes):
        ball = ball_of == k
        tris = np.where(ball[M.T].all(1) | sel_skin[M.T].all(1))[0]
        bvh = mesh_bvh(P, M.T[tris])
        ball_ids = set(int(i) for i, t in enumerate(tris) if ball[M.T[t]].all())
        o = eye_opening(bvh, ball_ids, c, r)
        if o is None:
            out.append(None)
            continue
        bot, top, W = o
        Ri = IRIS_OF_WIDTH * W
        z_ic = bot + 0.9 * Ri
        want_top = min(z_ic + Ri / 3, bot + OPEN_OF_WIDTH * W)
        drop = float(np.clip(top - want_top, 0.0, LID_MOST))
        if drop > 0:
            dy = P[:, 1] - c[1]
            half = W / 2
            ty = np.clip(1 - (dy / half) ** 2, 0, 1) ** 1.5       # (nothing at the corners: the lids meet there)
            z = P[:, 2]
            mid_z = (bot + top) / 2
            tz = np.where(z <= top, smooth01((z - mid_z) / 0.0015), 1 - smooth01((z - top) / 0.009))
            near = np.linalg.norm(P - c, axis=1) < r + 0.008
            w = ty * tz * sel_skin * near * (P[:, 0] > c[0])
            mv = w > 1e-4
            P[mv, 2] -= drop * w[mv]
            # never into the eyeball
            d = P[mv] - c
            dl = np.linalg.norm(d, axis=1)
            inside = dl < r + 0.0005
            idx = np.where(mv)[0][inside]
            P[idx] = c + (r + 0.0005) * (d[inside] / dl[inside, None])
        o2 = eye_opening(mesh_bvh(P, M.T[tris]), ball_ids, c, r)
        dz = z_ic - c[2]
        sx = np.sqrt(max(r * r - dz * dz, 1e-8))
        axis = _n(np.array((sx, 0.0, dz)))
        ang = float(np.degrees(np.arcsin(min(0.95, Ri / r))))
        out.append(dict(c=[round(float(v), 5) for v in c], r=round(r, 5), axis=[round(float(v), 4) for v in axis],
                        bottom_z=round(bot, 5), top_z=round(o2[1], 5) if o2 else round(top - drop, 5), iris_z=round(z_ic, 5),
                        iris_deg=round(ang, 2), iris_mm=round(Ri * 1000, 2), width_mm=round(W * 1000, 2),
                        opening_mm=[round((top - bot) * 1000, 2), round((o2[1] - o2[0]) * 1000, 2) if o2 else None],
                        lid_drop_mm=round(drop * 1000, 2)))
    rep["eyes"] = out
    return out


def soften_glabella(M, sel_skin, eyes, ez, passes=8):
    """The skin between the brows smoothed (the finer face brought back MakeHuman's frown lines: a fixed scowl)."""
    P = M.P
    y, z = P[:, 1], P[:, 2]
    ex = float(np.mean([e[0][0] for e in eyes]))
    w = (1 - smooth01((np.abs(y) - 0.010) / 0.008)) * smooth01((z - (ez - 0.004)) / 0.006) * \
        (1 - smooth01((z - (ez + 0.026)) / 0.008)) * (P[:, 0] > ex) * sel_skin
    rows, cols = graph(M.T, M.n)
    deg = np.bincount(rows, minlength=M.n).astype(float)
    for _ in range(passes):
        acc = np.zeros_like(P)
        np.add.at(acc, rows, P[cols])
        avg = np.where(deg[:, None] > 0, acc / np.maximum(deg, 1)[:, None], P)
        P += 0.5 * w[:, None] * (avg - P)
    return int((w > 0.05).sum())


def folded(P, T, tri):
    """The triangles (of `tri`) that face against their neighbourhood: each one's facing against its corners'
    area-weighted facings."""
    A, B, C = P[T[tri, 0]], P[T[tri, 1]], P[T[tri, 2]]
    Fn = -np.cross(B - A, C - A)
    Nv = np.zeros_like(P)
    for j in range(3):
        np.add.at(Nv, T[tri, j], Fn)
    Nv = _n(Nv)
    Fu = _n(Fn)
    dots = (Fu * Nv[T[tri]].mean(1)).sum(1)
    return tri[dots < 0.05]


def unfold(M, sel, eyes, passes=60):
    """Folds smoothed out of the face's skin (sel: its points; the ears are left out): a triangle turned over against
    its neighbours (one was in the files as exported, by the right temple, which showed as a thin pale sliver under
    the helmet's edge; a few tiny ones at the mouth's corners came of the lips' closing) has its corners eased toward
    their neighbours until none is left. The eyelids are kept off the eyeballs."""
    P = M.P
    T = M.T
    tri = np.where(sel[T].all(1))[0]
    rows, cols = graph(T, M.n)
    deg = np.bincount(rows, minlength=M.n).astype(float)
    first = len(folded(P, T, tri))
    moved = np.zeros(M.n, bool)
    for _ in range(passes):
        bad = folded(P, T, tri)
        if not len(bad):
            break
        pts = np.zeros(M.n, bool)
        pts[T[bad].ravel()] = True
        # (and their neighbours, so a fold's whole fan eases)
        nb = np.zeros(M.n, bool)
        nb[cols[pts[rows]]] = True
        pts |= nb & sel
        acc = np.zeros_like(P)
        np.add.at(acc, rows, P[cols])
        avg = acc / np.maximum(deg, 1)[:, None]
        P[pts] += 0.5 * (avg[pts] - P[pts])
        moved |= pts
        for c, r, _ in eyes:
            d = P - c
            dl = np.linalg.norm(d, axis=1)
            inside = moved & (dl < r + 0.0005)
            P[inside] = c + (r + 0.0005) * d[inside] / dl[inside, None]
    return dict(folded_before=int(first), folded_after=int(len(folded(P, T, tri))), points_eased=int(moved.sum()))


VIEWS = [np.array([np.cos(np.radians(p)) * np.cos(np.radians(y)), np.cos(np.radians(p)) * np.sin(np.radians(y)),
                   np.sin(np.radians(p))]) for y in (-35, 0, 35) for p in (0, 12, 25)]


def seen_through_gear(P, T, pa, part, hb, rng, ez, behind=0.004):
    """Head-skin points a camera could see (front or three-quarter, level to 25 degrees above) with the helmet or cap
    right behind them along the line of sight (within 4 mm): skin come through the brim or the band."""
    from mathutils import Vector
    gear = np.zeros(len(T), bool)
    if rng:
        gear[rng[0]:rng[0] + rng[1]] = True
    gear |= (part == hb)[T].all(1) & ~np.isin(pa[T], (SKIN, BOOTS, 4)).any(1)
    if not gear.any():
        return np.zeros(0, int)
    gb = mesh_bvh(P, T[gear])
    allb = mesh_bvh(P, T)
    out = []
    for i in np.where((pa == SKIN) & (part == hb) & (P[:, 2] > ez - 0.005))[0]:
        o = Vector(P[i])
        if gb.find_nearest(o, 0.012)[0] is None:
            continue
        for d in VIEWS:
            v = Vector(d)
            if allb.ray_cast(o + v * 3e-4, v, 1.0)[0] is not None:
                continue
            if gb.ray_cast(o - v * 1e-5, -v, behind)[0] is not None:
                out.append(int(i))
                break
    return np.array(out, int)


def tuck_under_gear(M, D, hb, ez, passes=8):
    """Skin kept under the helmet or cap: the finer face's new points sit on MakeHuman's man, whose brow stands a few
    millimetres proud of the thinned head the cap was fitted to, so under a low cap the skin came through the brim
    (brown flecks of brow on the cap's peak). Each point seen through the headgear goes in 1.5 mm along its facing (its
    neighbours half as far, so no pit), until none is seen."""
    P = M.P
    pa = M.A["paints"][:, 0]
    part = M.A["part"][:, 0]
    rng = (D.get("helmet_tris") or {}).get("near")
    skin = (pa == SKIN) & (part == hb)
    rows, cols = graph(M.T, M.n)
    first, moved = None, set()
    for _ in range(passes):
        bad = seen_through_gear(P, M.T, pa, part, hb, rng, ez)
        if first is None:
            first = len(bad)
        if not len(bad):
            break
        Nv = _n(vertex_normals(P, M.T[(pa == SKIN)[M.T].all(1)], M.n))
        step = np.zeros(M.n)
        step[bad] = 0.0015
        nb = np.zeros(M.n, bool)
        nb[cols[np.isin(rows, bad)]] = True
        nb &= skin & (step == 0)
        step[nb] = 0.00075
        P -= Nv * step[:, None]
        moved |= set(np.where(step > 0)[0].tolist())
    return dict(seen_through_before=int(first or 0), seen_through_after=int(len(seen_through_gear(P, M.T, pa, part, hb, rng, ez))),
                points_tucked=len(moved))


def refine(D, seed, nation):
    m = D["near"]
    M = Mesh(m)
    names = [b["name"] for b in D["bones"]]
    hb = names.index("head")
    n0, t0 = M.n, len(M.T)
    P = M.P
    pa = M.A["paints"][:, 0]
    part = M.A["part"][:, 0]
    # the eyeballs (paint 2 on the head bone, one each side) and the face's box
    eye_pts = np.where((part == hb) & (pa == BOOTS))[0]
    eyes = []
    for sel in (P[eye_pts, 1] < 0, P[eye_pts, 1] > 0):
        e = eye_pts[sel]
        c, r = sphere(P[e])
        eyes.append((c, r, e))
    ex = float(np.mean([e[0][0] for e in eyes]))
    ez = float(np.mean([e[0][2] for e in eyes]))
    ey = float(np.mean([abs(e[0][1]) for e in eyes]))
    head_skin = (pa == SKIN) & (part == hb)
    skin_head_tri = head_skin[M.T].all(1)
    C = P[M.T].mean(1)
    H = Head(human.look_for(seed, nation))
    zs = np.arange(ez - 0.03, ez - 0.11, -0.001)
    prof = []
    for z in zs:
        hit = H.bvh.ray_cast(Vector((0.4, 0.0, z)), Vector((-1.0, 0.0, 0.0)), 0.5)[0]
        prof.append(hit.x if hit is not None else np.nan)
    prof = np.array(prof)
    i_tip = int(np.nanargmax(np.where(zs > ez - 0.075, prof, np.nan)))
    tip_z, tip_x = float(zs[i_tip]), float(prof[i_tip])
    chin_z = ez - 0.13
    # the whole face, cheeks and jaw to the ears' fronts (the turned views showed the coarse cheek and jaw)
    ear = (np.abs(C[:, 1]) > ey + 0.028) & (C[:, 0] < ex - 0.022)
    face_box = ((C[:, 0] > ex - 0.06) & (np.abs(C[:, 1]) < 0.078) & (C[:, 2] > chin_z) & (C[:, 2] < ez + 0.045) & ~ear)
    red1 = skin_head_tri & face_box
    # the eyeballs' fronts
    eye_tri = np.zeros(len(M.T), bool)
    for c, r, e in eyes:
        on = np.isin(M.T, e).all(1)
        eye_tri |= on & (C[:, 0] > c[0] - 0.2 * r)
    eye_of = {}
    for k, (c, r, e) in enumerate(eyes):
        for i in e:
            eye_of[int(i)] = k

    stats = {"projected": 0, "nearest": 0, "kept_mid": 0}

    def place(a, b):
        P = M.A["points"]
        q = (P[a] + P[b]) / 2
        if a in eye_of and b in eye_of and eye_of[a] == eye_of[b]:
            c, r, _ = eyes[eye_of[a]]
            return c + r * _n(q - c)
        nrm = _n(M.A["facings"][a] + M.A["facings"][b])
        hit = H.ray(q, nrm, RAY_MOST)
        if hit is not None and H.normal_at(hit[1], hit[0]) @ nrm > AGREE:
            stats["projected"] += 1
            return hit[0]
        loc, ti, d = H.nearest([q])[0]
        if d < NEAREST_MOST and H.normal_at(ti, loc) @ nrm > AGREE:
            stats["nearest"] += 1
            return loc
        stats["kept_mid"] += 1
        return q

    def track_eyes():
        for i, (a, b) in M.parents.items():
            if i not in eye_of and a in eye_of and b in eye_of and eye_of[a] == eye_of[b]:
                eye_of[i] = eye_of[a]

    r1 = red_green(M, red1 | eye_tri, place)
    track_eyes()
    # once more about the eyes and the mouth, and the eyeballs' fronts again (a clean edge to the iris)
    P = M.P
    pa = M.A["paints"][:, 0]
    part = M.A["part"][:, 0]
    head_skin = (pa == SKIN) & (part == hb)
    C = P[M.T].mean(1)
    near_eye = np.zeros(len(M.T), bool)
    ball2 = np.zeros(len(M.T), bool)
    is_eye = np.zeros(M.n, bool)
    is_eye[list(eye_of)] = True
    for c, r, e in eyes:
        d = C - c
        near_eye |= (d[:, 1] ** 2 + (d[:, 2] * 1.25) ** 2 < 0.019 ** 2) & (d[:, 0] > -0.010)
        # (and the brow over it, so a brow's edge falls soft, not along the coarse triangles)
        near_eye |= (np.abs(d[:, 1]) < 0.024) & (d[:, 2] > 0.006) & (d[:, 2] < 0.020) & (d[:, 0] > -0.012)
        ball2 |= is_eye[M.T].all(1) & (np.linalg.norm(d, axis=1) < r * 1.2) & (d[:, 0] > 0.55 * r)
    mouth = (np.abs(C[:, 1]) < 0.030) & (C[:, 2] < tip_z - 0.010) & (C[:, 2] > tip_z - 0.050) & (C[:, 0] > tip_x - 0.040)
    red2 = (head_skin[M.T].all(1) & (near_eye | mouth)) | ball2
    r2 = red_green(M, red2, place)
    track_eyes()
    P = M.P
    pa = M.A["paints"][:, 0]
    part = M.A["part"][:, 0]
    head_skin = (pa == SKIN) & (part == hb)
    face_skin = head_skin & (P[:, 0] > ex - 0.07)
    ball_of = np.full(M.n, -1)
    for i, k in eye_of.items():
        ball_of[i] = k
    geo = {}
    # the shape: mouths closed at rest, the upper lids down over the irises' tops, the scowl between the brows eased
    close_mouth(M, face_skin, ez, tip_z, geo)
    iris = lower_lids(M, face_skin, eyes, ball_of, geo)
    geo["glabella_points"] = soften_glabella(M, face_skin, eyes, ez)
    # (the face only, cheeks and jaw to the ears' fronts: an ear's curl is folds by nature and stays as it is)
    Cf = M.P[M.T].mean(1)
    ear_f = (np.abs(Cf[:, 1]) > ey + 0.028) & (Cf[:, 0] < ex - 0.022)
    in_face = head_skin[M.T].all(1) & (Cf[:, 0] > ex - 0.06) & (np.abs(Cf[:, 1]) < 0.078) & (Cf[:, 2] > chin_z) & \
        (Cf[:, 2] < ez + 0.045) & ~ear_f
    face_pts = np.zeros(M.n, bool)
    face_pts[M.T[in_face].ravel()] = True
    geo["folds"] = unfold(M, face_pts, eyes)
    geo["under_headgear"] = tuck_under_gear(M, D, hb, ez)
    # facings afresh, smooth, over all the skin of the head and neck (shared where points share a place); each
    # eyeball's from its centre
    P = M.P
    skin_up = (pa == SKIN) & (P[:, 2] > 1.3)
    Ts = M.T[skin_up[M.T].all(1)]
    Nn = smooth_normals(P, Ts, skin_up & (np.isin(part, [hb, names.index("neck")]) if "neck" in names else skin_up))
    ok = np.linalg.norm(Nn, axis=1) > 0.5
    M.A["facings"][ok] = Nn[ok]
    for k, (c, r, e) in enumerate(eyes):
        ball = ball_of == k
        M.A["facings"][ball] = _n(P[ball] - c)
    return M, dict(points=(n0, M.n), triangles=(t0, len(M.T)), level1=r1, level2=r2, place=stats,
                   tip_z=round(tip_z, 4), eye_z=round(ez, 4), shape=geo), iris


def graph(T, n):
    e = np.vstack([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]])
    e = np.unique(np.sort(e, 1), axis=0)
    rows = np.concatenate([e[:, 0], e[:, 1]])
    cols = np.concatenate([e[:, 1], e[:, 0]])
    return rows, cols


def smooth_new(F, G, n0, passes=4, lam=0.5):
    """Laplacian smoothing of a per-point field F (n x k) at the new points only (the old keep their values): the
    parents' average alone leaves a star of the old coarse triangles."""
    rows, cols = G
    n = len(F)
    deg = np.bincount(rows, minlength=n).astype(float)
    F = F.copy()
    for _ in range(passes):
        acc = np.zeros_like(F)
        np.add.at(acc, rows, F[cols])
        avg = np.where(deg[:, None] > 0, acc / np.maximum(deg, 1)[:, None], F)
        F[n0:] = F[n0:] + lam * (avg[n0:] - F[n0:])
    return F


def smooth_set(F, G, sel, passes=4, lam=0.5):
    """Laplacian smoothing of a per-point field at the points `sel` only."""
    rows, cols = G
    n = len(F)
    deg = np.bincount(rows, minlength=n).astype(float)
    F = F.copy()
    for _ in range(passes):
        acc = np.zeros_like(F)
        np.add.at(acc, rows, F[cols])
        avg = np.where(deg[:, None] > 0, acc / np.maximum(deg, 1)[:, None], F)
        F[sel] = F[sel] + lam * (avg[sel] - F[sel])
    return F


def carry_faces(D, M, n0, F0):
    """Each face's move, facing and tint at the new points, from their two parents (recursively, in order); at an old
    point whose facing was made afresh, the face's facing turns with it (F0: the facings before)."""
    G = graph(M.T, M.n)
    for F in D.get("faces", []):
        L = F.get("near")
        if not L:
            continue
        mv, fc, tn = {}, {}, {}
        at = L.get("at", [])
        for k, i in enumerate(at):
            mv[i] = np.array(L["move"][3 * k:3 * k + 3])
            fc[i] = np.array(L["facing"][3 * k:3 * k + 3])
        for k, i in enumerate(L.get("tint_at", [])):
            tn[i] = np.array(L["tint"][3 * k:3 * k + 3])
        base_f = M.A["facings"]
        for i in list(fc):
            if i < n0:
                fc[i] = _n(base_f[i] + (fc[i] - F0[i]))
        for i in range(n0, M.n):
            a, b = M.parents[i]
            ma, mb = mv.get(a, np.zeros(3)), mv.get(b, np.zeros(3))
            m_ = (ma + mb) / 2
            if np.linalg.norm(m_) > 2e-4:
                mv[i] = m_
                da = fc[a] - base_f[a] if a in fc else np.zeros(3)
                db = fc[b] - base_f[b] if b in fc else np.zeros(3)
                fc[i] = _n(base_f[i] + (da + db) / 2)
            ta, tb = tn.get(a, np.ones(3)), tn.get(b, np.ones(3))
            tn[i] = (ta + tb) / 2
        Ft = np.ones((M.n, 3))
        for i, v in tn.items():
            Ft[i] = v
        Ft = smooth_new(Ft, G, n0)
        tn = {i: v for i, v in tn.items() if i < n0}
        for i in range(n0, M.n):
            if np.abs(Ft[i] - 1).max() > 1e-3:
                tn[i] = Ft[i]
        if at or mv:
            keys = sorted(mv)
            L["at"] = keys
            L["move"] = [round(float(v), 4) for i in keys for v in mv[i]]
            L["facing"] = [round(float(v), 3) for i in keys for v in fc[i]]
        keys = sorted(tn)
        L["tint_at"] = keys
        L["tint"] = [round(float(v), 3) for i in keys for v in tn[i]]


def army_of(path):
    b = os.path.basename(path).replace("_gore.json", ".json")
    inv = {"soldier_GER.json": "GER", "soldier_POL.json": "POL", "soldier_SOV.json": "SOV", "soldier_SOV_winter.json": "SOV",
           "soldier_SOV_snow.json": "SOV", "soldier_ITA.json": "ITA", "soldier_ITA_desert.json": "ITA",
           "soldier_JAP.json": "JAP", "soldier_CHN.json": "CHN", "soldier_UK.json": "UK", "soldier_FRA.json": "FRA"}
    return inv.get(b, "USA" if b.startswith("soldier_USA") else None)


def recolour_near(D, M, army, seed, nation):
    """Each face's hair, brows, beard shadow and moustache at near drawn afresh on the finer face, as variety.py draws
    them (its face_marks and colours), on the man the file was cut from. Returns faces whose plan did not match."""
    sys.path.insert(0, os.environ.get("VARIETY_DIR", os.path.join(HERE, "src")))   # (variety.py, from the looks branch)
    import variety as V
    base = V.Man(human.look_for(seed, nation))
    P = M.P * (1, -1, 1)
    pa = M.A["paints"][:, 0]
    idx = np.where((pa == SKIN) & (P[:, 2] > 1.3))[0]
    X = P[idx]
    tri, bary, dist = base.where(X)
    marks = V.face_marks(base, base.at(tri, bary))
    g = V.skin_graph(M.T, idx)
    for key, passes in (("beard", 5), ("tache", 2)):      # (variety's 2 and 1 passes, on a mesh about three times as fine)
        marks[key] = V.smooth_field(marks[key][:, None], g, passes)[:, 0]
    S = V.hex_rgb(D.get("colours", {}).get("skin") or V.GAME_SKIN)
    plan = V.PLAN.get(army, V.PLAN["_european"])
    east = army in ("JAP", "CHN")
    skipped = []
    lips = {}
    D.setdefault("face_mesh_lips", {})
    for slot, F in enumerate(D.get("faces", [])):
        spec = plan[slot] if slot < len(plan) else None
        hair_name = (spec or {}).get("hair", "black" if east else "brown")
        if spec is None or V.HAIR.get(hair_name) != F.get("hair"):
            skipped.append(slot)
            continue
        tone = np.array(F["tone"], float)
        hair = V.hex_rgb(F["hair"])
        shadow, tache = V.LIPS[spec.get("lip", "clean")]
        lips[str(slot)] = [shadow, tache]
        skin = S * tone
        light = float(hair @ (0.3, 0.59, 0.11))
        a_hair = marks["hair"] * 0.92
        # (the brows and the moustache are drawn by face_detail.py, on the brow ridge and the lip's edge; every man
        # has at least a faint shadow of stubble, the art director, 23 November)
        a_brow = marks["brow"] * 0.0
        a_beard = marks["beard"] * max(shadow, BASE_STUBBLE) * 0.3 * (1 - 0.7 * V.smooth(light, 0.15, 0.5))
        a_tache = marks["tache"] * 0.0
        col = np.tile(skin, (len(idx), 1))
        for a, c in ((a_beard, hair * 0.45 + skin * 0.55 * 0.7), (a_tache, hair * 0.85 + skin * 0.15), (a_brow, hair * 0.9),
                     (a_hair, hair)):
            col = col * (1 - a[:, None]) + c[None, :] * a[:, None]
        tint = col / skin[None, :]
        keep = np.abs(tint - 1).max(axis=1) > 0.02
        F["near"]["tint_at"] = [int(i) for i in idx[keep]]
        F["near"]["tint"] = [round(float(v), 3) for v in tint[keep].ravel()]
    D["face_mesh_lips"] = lips
    return skipped


def write_mesh(D, M):
    m = D["near"]
    for k, a in M.A.items():
        if k in ("paints", "part", "part2"):
            m[k] = [int(v) for v in a.ravel()]
        elif k in ("points",):
            m[k] = [round(float(v), 5) for v in a.ravel()]
        elif k in ("facings",):
            m[k] = [round(float(v), 4) for v in a.ravel()]
        else:
            m[k] = [round(float(v), 4) for v in a.ravel()]
    m["triangles"] = [int(v) for v in M.T.ravel()]


ABOUT = ("Close-range face (Tools/battle_units/face_mesh.py): the near mesh's face is split finer, red-green (no crack),"
         " and its new points set on MakeHuman's man the file was cut from (seed and nation below), so lids, lips,"
         " nostrils and the folds by the nose come back; the eyeballs' fronts are split once on their own spheres."
         " No index moves: a split triangle keeps its place with its first piece; the other pieces and the new points"
         " are on the end of near's lists (from the first new point and triangle below), so helmet_tris and the gore"
         " ranges stay true. Each new point takes its parents' bone weights, shade, wear and tint, and in each face"
         " their move, facing and tint. Mid and far are as they were.")


def main(src, dst, seed=None, nation=None):
    D = json.load(open(src))
    if "face_mesh" in D:
        raise SystemExit(f"{src}: face_mesh already run (version {D['face_mesh'].get('version')})")
    if seed is None:
        S = json.load(open(os.path.join(HERE, "seeds.json")))[os.path.basename(src)]
        seed, nation = S["seed"], S["nation"]
    F0 = np.array(D["near"]["facings"], float).reshape(-1, 3)
    M, rep, iris = refine(D, int(seed), nation)
    n0 = rep["points"][0]
    G = graph(M.T, M.n)
    for k in ("shade", "shade_clean", "tint", "dirt", "dirt_wet", "wet", "snow"):
        if k in M.A:
            M.A[k] = smooth_new(M.A[k], G, n0, passes=3)
    # the baked shade over the face's skin smoothed, old points too (on the coarse points it drew the triangles)
    hb = [b["name"] for b in D["bones"]].index("head")
    fs = (M.A["paints"][:, 0] == SKIN) & (M.A["part"][:, 0] == hb)
    for k in ("shade", "shade_clean"):
        if k in M.A:
            M.A[k] = smooth_set(M.A[k], G, fs, passes=4)
    carry_faces(D, M, n0, F0)
    army = army_of(src)
    rep["tints_kept_from_parents"] = recolour_near(D, M, army, int(seed), nation) if (army and D.get("faces")) else "no faces"
    write_mesh(D, M)
    lips = D.pop("face_mesh_lips", {})
    D["face_mesh"] = {"version": VERSION, "seed": int(seed), "nation": nation, "eyes": iris, "lips": lips,
                      "first_new_point": n0,
                      "first_new_triangle": rep["triangles"][0], "about": ABOUT, "report": rep}
    tmp = dst + ".tmp"
    json.dump(D, open(tmp, "w"), separators=(",", ":"))
    os.replace(tmp, dst)
    print(os.path.basename(src), json.dumps(rep))


if __name__ == "__main__":
    a = sys.argv[1:]
    main(a[0], a[1], *(a[2:4] if len(a) >= 4 else ()))
