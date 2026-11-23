"""A close-range face with the detail of the man it was cut from (Brian, 23 November 2026: "The faces need a lot of
work").

The battle soldier's near mesh (drawn only within about 9 m of the camera) was thinned to a budget all over, so his face
kept some 600 triangles: too coarse to hold an eyelid, the edge of a lip or a nostril, or a brow in colour. Every
soldier file is cut from MakeHuman's CC0 man for one look (human.look_for(seed, nation); seeds.json says which, found by
fit: the file's face points lie within half a millimetre of that man's), so the detail is still there to take back:

- the near mesh's face (skin on the head bone, from the chin to above the brows, in front of the temples) is split
  red-green: each triangle into four, and where only one edge of a neighbour was split, that neighbour into two, so
  no crack opens. The eyes' and mouth's surroundings and the nose are split once more. Each new point is set on
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

VERSION = 1
SKIN, BOOTS = 7, 2
PER_POINT = ("points", "facings", "paints", "part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet",
             "snow", "tint")
RAY_MOST = 0.004          # (m) how far along its facing a new point may go to meet MakeHuman's man
NEAREST_MOST = 0.0025     # (m) else, how far to his nearest point
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
    head_skin = (pa == SKIN) & (part == hb)
    skin_head_tri = head_skin[M.T].all(1)
    C = P[M.T].mean(1)
    # the mouth's height: the skin's most forward points below the nose come in two bands (the lips); take the
    # gap between by a ray profile down the middle
    H = Head(human.look_for(seed, nation))
    zs = np.arange(ez - 0.03, ez - 0.11, -0.001)
    prof = []
    for z in zs:
        hit = H.bvh.ray_cast(Vector((0.4, 0.0, z)), Vector((-1.0, 0.0, 0.0)), 0.5)[0]
        prof.append(hit.x if hit is not None else np.nan)
    prof = np.array(prof)
    i_tip = int(np.nanargmax(np.where(zs > ez - 0.075, prof, np.nan)))
    tip_z, tip_x = float(zs[i_tip]), float(prof[i_tip])
    lo = zs > ez - 0.12
    chin_z = ez - 0.115
    face_box = ((C[:, 0] > ex - 0.032) & (np.abs(C[:, 1]) < 0.066) & (C[:, 2] > chin_z) & (C[:, 2] < ez + 0.042))
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

    r1 = red_green(M, red1 | eye_tri, place)
    # once more about the eyes, the mouth and the nose
    P = M.P
    pa = M.A["paints"][:, 0]
    part = M.A["part"][:, 0]
    head_skin = (pa == SKIN) & (part == hb)
    C = P[M.T].mean(1)
    near_eye = np.zeros(len(M.T), bool)
    for c, r, e in eyes:
        d = C - c
        near_eye |= (d[:, 1] ** 2 + (d[:, 2] * 1.3) ** 2 < 0.022 ** 2) & (d[:, 0] > -0.012)
    mouth_z = float(np.nan)
    mouth = (np.abs(C[:, 1]) < 0.032) & (C[:, 2] < tip_z - 0.008) & (C[:, 2] > tip_z - 0.055) & (C[:, 0] > tip_x - 0.045)
    nose = (np.abs(C[:, 1]) < 0.022) & (C[:, 2] < ez + 0.004) & (C[:, 2] >= tip_z - 0.012) & (C[:, 0] > tip_x - 0.03)
    red2 = head_skin[M.T].all(1) & (near_eye | mouth | nose)
    r2 = red_green(M, red2, place)
    # facings afresh over the head's skin and the eyeballs (each eyeball's from its centre)
    P = M.P
    pa = M.A["paints"][:, 0]
    part = M.A["part"][:, 0]
    head_skin = (pa == SKIN) & (part == hb)
    Ts = M.T[head_skin[M.T].all(1)]
    vn = vertex_normals(P, Ts, M.n)
    touched = np.zeros(M.n, bool)
    touched[n0:] = True
    changed = np.zeros(len(M.T), bool)
    changed[:t0] = np.any(M.T[:t0] != np.array(m["triangles"], int).reshape(-1, 3), 1)
    changed[t0:] = True
    inner = np.zeros(M.n, bool)
    inner[np.unique(M.T[changed])] = True
    # an old point keeps its facing where it still has an unsplit triangle about it (the seam stays as it was)
    unsplit = np.zeros(M.n, bool)
    unsplit[np.unique(M.T[:t0][~changed[:t0]])] = True
    fresh = inner & head_skin & ~(unsplit & ~touched) & (np.linalg.norm(vn, axis=1) > 0)
    M.A["facings"][fresh] = _n(vn[fresh])
    for k, (c, r, e) in enumerate(eyes):
        ball = np.zeros(M.n, bool)
        ball[e] = True
        for i, (a, b) in M.parents.items():
            if ball[a] and ball[b]:
                ball[i] = True
                eye_of[i] = k
        M.A["facings"][ball] = _n(P[ball] - c)
    return M, dict(points=(n0, M.n), triangles=(t0, len(M.T)), level1=r1, level2=r2, place=stats,
                   tip_z=round(tip_z, 4), eye_z=round(ez, 4))


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
    for slot, F in enumerate(D.get("faces", [])):
        spec = plan[slot] if slot < len(plan) else None
        hair_name = (spec or {}).get("hair", "black" if east else "brown")
        if spec is None or V.HAIR.get(hair_name) != F.get("hair"):
            skipped.append(slot)
            continue
        tone = np.array(F["tone"], float)
        hair = V.hex_rgb(F["hair"])
        shadow, tache = V.LIPS[spec.get("lip", "clean")]
        skin = S * tone
        light = float(hair @ (0.3, 0.59, 0.11))
        a_hair = marks["hair"] * 0.92
        a_brow = marks["brow"] * 0.7
        a_beard = marks["beard"] * shadow * 0.3 * (1 - 0.7 * V.smooth(light, 0.15, 0.5))
        a_tache = marks["tache"] * tache * 0.85
        col = np.tile(skin, (len(idx), 1))
        for a, c in ((a_beard, hair * 0.45 + skin * 0.55 * 0.7), (a_tache, hair * 0.85 + skin * 0.15), (a_brow, hair * 0.9),
                     (a_hair, hair)):
            col = col * (1 - a[:, None]) + c[None, :] * a[:, None]
        tint = col / skin[None, :]
        keep = np.abs(tint - 1).max(axis=1) > 0.02
        F["near"]["tint_at"] = [int(i) for i in idx[keep]]
        F["near"]["tint"] = [round(float(v), 3) for v in tint[keep].ravel()]
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
    M, rep = refine(D, int(seed), nation)
    n0 = rep["points"][0]
    G = graph(M.T, M.n)
    for k in ("shade", "shade_clean", "tint", "dirt", "dirt_wet", "wet", "snow"):
        if k in M.A:
            M.A[k] = smooth_new(M.A[k], G, n0, passes=3)
    carry_faces(D, M, n0, F0)
    army = army_of(src)
    rep["tints_kept_from_parents"] = recolour_near(D, M, army, int(seed), nation) if (army and D.get("faces")) else "no faces"
    write_mesh(D, M)
    D["face_mesh"] = {"version": VERSION, "seed": int(seed), "nation": nation, "first_new_point": n0,
                      "first_new_triangle": rep["triangles"][0], "about": ABOUT, "report": rep}
    tmp = dst + ".tmp"
    json.dump(D, open(tmp, "w"), separators=(",", ":"))
    os.replace(tmp, dst)
    print(os.path.basename(src), json.dumps(rep))


if __name__ == "__main__":
    a = sys.argv[1:]
    main(a[0], a[1], *(a[2:4] if len(a) >= 4 else ()))
