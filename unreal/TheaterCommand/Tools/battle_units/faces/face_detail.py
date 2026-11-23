"""Faces that read as men, not mannequins (Brian, 23 November 2026: "The faces need a lot of work"; the art director's
second round the same night).

The game colours a soldier's point as its paint's colour x shade x (skin: the face's tone) x the face's tint where the
face lists it x the mesh's own tint (LandMen MakeLook). Run after face_mesh.py (which gives the near face its lids,
lips and a closed mouth), this tool colours the face, at near and mid, with no new point or triangle:

- eyes (near): the eyeball's iris (paint 47 `iris`, dark, the size face_mesh.py placed it) and white (paint 46
  `eye_white`, no brighter than lit skin), both the file's own paints (`new_paints` and `colours`, which LandMen
  reads); the eyeball's shade set to the skin's about it; in the mesh's tint the pupil and the iris's rim darker, the
  white darker toward the corners and in a band under the upper lid, a little under the lower; in each face's tint the
  iris's colour (blue-grey to dark brown, by the man's hair) and the white kept under his skin's brightness;
- in the mesh's tint (so on all eight faces): the lash line along the upper lid's edge (a fainter one under), a soft
  shadow in the sockets, the lips (thin, the upper in shade, the lower catching light, no pink) and a faint line
  between, the folds from the nose to the mouth's corners and under the lower lip, nostrils, warmth on the nose,
  cheeks and ears, a shadow under the jaw; and the shade the headgear's brim throws over the brow and the eyes (rays
  to the sky met by the helmet or cap: up to 30% darker under a steel helmet, 20% under a cap, gone by the
  cheekbones);
- in each face's tint (near): brows in that face's hair colour along his brow ridge, soft-edged; and for the men
  variety.py gave a moustache, one in his hair's colour with a clear lower edge on the lip, thinner at the ends.
  Files with no faces (the sailors and tankers: one man each) take hair and a day's stubble from variety.py and the
  same brows, in the mesh's tint.

Where the features are is found on the file's own face: the eyes as face_mesh.py recorded them, the open eye's edges
by rays from in front, and the face's front surface down the middle for the nose, the lips, the folds and the chin.

    /home/user/bpyenv/bin/python face_detail.py IN.json OUT.json

It records what it did under `face_detail`; run again on its own output it first takes its own work back off, so it
never doubles up.
"""
import hashlib, json, math, os, sys
import numpy as np
import bpy  # noqa: F401  (Blender as a module: mathutils)
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
VERSION = 2
SKIN, BOOTS, STRAPS = 7, 2, 4
EYE_WHITE, IRIS = 46, 47
COLOURS = {"eye_white": "9E988E", "iris": "4A3A2C"}
LODS = ("near", "mid")
# (the iris's colour by the man's hair: variety.py's HAIR names)
IRISES = {"blue_grey": "5A6670", "grey_green": "56604C", "hazel": "5A4A30", "brown": "4A3A2C", "dark": "3A2C22"}
HAIR = {"1C1916": "black", "2E241D": "dark", "4A3727": "brown", "66503A": "mid", "86704F": "light", "A08A62": "fair",
        "6E3F25": "red", "6F6A63": "grey", "574F47": "greying"}
CAPS = ("soldier_CHN", "soldier_JAP", "soldier_SOV_snow", "soldier_SOV_winter")
BRIM = {"helmet": 0.30, "cap": 0.20}
BASE_STUBBLE = 0.4         # (face_mesh.py's: every man a day's shadow)


def sha(a):
    return hashlib.sha1(np.round(np.asarray(a, float), 4).tobytes()).hexdigest()[:16]


def smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


def band(x, lo, hi, soft):
    """1 inside [lo, hi], easing to 0 over `soft` outside."""
    return smooth((x - (lo - soft)) / soft) * smooth(((hi + soft) - x) / soft)


def gauss(d2, s):
    return np.exp(-d2 / (2 * s * s))


def hexrgb(h):
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float)


def sphere(X):
    """Least-squares sphere through points: centre, radius."""
    A = np.c_[2 * X, np.ones(len(X))]
    b = (X ** 2).sum(1)
    s = np.linalg.lstsq(A, b, rcond=None)[0]
    c = s[:3]
    return c, float(np.sqrt(s[3] + c @ c))


def bvh_of(P, T):
    return BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in T])


class Face:
    """The landmarks of a file's face, from its near mesh (face 0)."""

    def __init__(self, D):
        m = D["near"]
        names = [b["name"] for b in D["bones"]]
        self.hb = names.index("head")
        self.nb = names.index("neck") if "neck" in names else self.hb
        P = np.array(m["points"], float).reshape(-1, 3)
        pa = np.array(m["paints"])
        part = np.array(m["part"])
        T = np.array(m["triangles"]).reshape(-1, 3)
        eye = np.where((part == self.hb) & np.isin(pa, (BOOTS, EYE_WHITE, IRIS)))[0]
        rec = (D.get("face_mesh") or {}).get("eyes") or [None, None]
        self.eyes = []
        for k, sel in enumerate((P[eye, 1] < 0, P[eye, 1] > 0)):
            e = eye[sel]
            c, r = sphere(P[e])
            R = rec[k] if k < len(rec) else None
            if R:
                c, r = np.array(R["c"], float), float(R["r"])
                axis, deg = np.array(R["axis"], float), float(R["iris_deg"])
                W = R["width_mm"] / 1000
            else:
                pole = P[e][np.argmax(P[e, 0])]
                axis = (pole - c) / np.linalg.norm(pole - c)
                deg, W = 17.0, 1.35 * r
            self.eyes.append(dict(c=c, r=r, axis=axis / np.linalg.norm(axis), deg=deg, W=W, pts=e))
        self.ez = float(np.mean([e["c"][2] for e in self.eyes]))
        self.ey = float(np.mean([abs(e["c"][1]) for e in self.eyes]))
        self.ex = float(np.mean([e["c"][0] for e in self.eyes]))
        head = np.where(np.isin(part, (self.hb, self.nb)))[0]
        skin_tri = T[np.isin(T, head).all(1) & np.isin(pa[T], (SKIN,)).all(1)]
        self.bvh = bvh_of(P, skin_tri)
        eye_tri = T[np.isin(T, eye).all(1)]
        self.eye_bvh = bvh_of(P, np.vstack([skin_tri, eye_tri]))
        self.n_skin_tri = len(skin_tri)
        # the front surface down the middle, half a millimetre apart, from above the eyes to under the chin
        st = 0.0005
        zs = np.arange(self.ez + 0.02, self.ez - 0.15, -st)
        xs = np.array([self.front(0.0, z) for z in zs])
        ok = np.isfinite(xs)
        zs, xs = zs[ok], xs[ok]

        def pick(lo, hi, fn):
            s = (zs >= lo) & (zs <= hi)
            j = np.where(s)[0]
            return int(j[fn(xs[j])])

        i_tip = pick(self.ez - 0.075, self.ez - 0.02, np.argmax)
        self.tip = (float(xs[i_tip]), float(zs[i_tip]))
        z_tip = self.tip[1]
        self.subnasale = float(zs[pick(z_tip - 0.020, z_tip - 0.004, np.argmin)])
        self.upper = float(zs[pick(self.subnasale - 0.012, self.subnasale - 0.001, np.argmax)])
        shape = ((D.get("face_mesh") or {}).get("report") or {}).get("shape") or {}
        mo = shape.get("mouth") if isinstance(shape.get("mouth"), dict) else None
        self.mouth = float(mo["line_z"]) if mo else float(zs[pick(self.upper - 0.010, self.upper - 0.001, np.argmin)])
        self.lower = float(zs[pick(self.mouth - 0.010, self.mouth - 0.001, np.argmax)])
        self.fold = float(zs[pick(self.lower - 0.016, self.lower - 0.002, np.argmin)])
        self.chin = float(zs[pick(self.fold - 0.022, self.fold - 0.002, np.argmax)])
        # the lips' borders, a third thinner than the lips' shapes: the upper from the line up two-thirds of the way
        # to its old border (0.45 of the way to the nose's base), the lower down two-thirds of the way to halfway to
        # the fold
        old_ul = self.upper + 0.45 * (self.subnasale - self.upper)
        old_ll = self.lower - 0.5 * (self.lower - self.fold)
        self.ul_top = self.mouth + (2 / 3) * (old_ul - self.mouth)
        self.ll_bot = self.mouth - (2 / 3) * (self.mouth - old_ll)
        self.profile = (zs, xs)
        for e in self.eyes:
            e.update(self.opening(e))
            e["ridge"] = self.ridge(e)
            e["brow"] = self.brow(e)

    def front(self, y, z):
        hit = self.bvh.ray_cast(Vector((0.4, y, z)), Vector((-1.0, 0.0, 0.0)), 0.5)[0]
        return float(hit.x) if hit is not None else float("nan")

    def opening(self, e):
        """The open eye's edges seen from in front: for each y across it, the highest and lowest z where a ray meets
        the eyeball rather than the lids."""
        c = e["c"]
        ys = np.arange(c[1] - 0.016, c[1] + 0.016, 0.00025)
        zs = np.arange(c[2] - 0.012, c[2] + 0.012, 0.0002)
        top = np.full(len(ys), np.nan)
        bot = np.full(len(ys), np.nan)
        for a, y in enumerate(ys):
            hits = []
            for z in zs:
                h = self.eye_bvh.ray_cast(Vector((0.5, y, z)), Vector((-1.0, 0.0, 0.0)), 1.0)
                if h[2] is not None and h[2] >= self.n_skin_tri:
                    hits.append(z)
            if hits:
                top[a], bot[a] = max(hits), min(hits)
        ok = np.isfinite(top)
        return dict(open_y=ys[ok], open_top=top[ok], open_bot=bot[ok])

    def ridge(self, e):
        """The brow ridge over the eye: for each step across (outward from the nose), the height where the brow
        stands most forward, smoothed; kept within a few millimetres of a gentle arch."""
        c, s = e["c"], np.sign(e["c"][1])
        top = float(e["open_top"].max()) if len(e["open_top"]) else c[2] + 0.004
        dys = np.arange(-0.017, 0.026, 0.001)
        zz = np.arange(top + 0.006, c[2] + 0.030, 0.0005)
        rz = []
        for dy in dys:
            xs = np.array([self.front(c[1] + s * dy, z) for z in zz])
            rz.append(zz[int(np.nanargmax(xs))] if np.isfinite(xs).any() else np.nan)
        rz = np.array(rz)
        arch = c[2] + 0.015 + 0.002 * (1 - ((dys - 0.004) / 0.022) ** 2)
        rz = np.where(np.isfinite(rz), rz, arch)
        rz = np.clip(rz, arch - 0.004, arch + 0.003)
        for _ in range(3):
            rz[1:-1] = (rz[:-2] + 2 * rz[1:-1] + rz[2:]) / 4
        return dict(dy=dys, z=rz)

    def brow(self, e):
        """Where the brow's middle runs: low on the ridge (its own height, less 3 mm, kept 9 to 13 mm over the open
        eye's top) and nearly level, highest two-thirds of the way out, the tail a touch lower."""
        top = float(e["open_top"].max()) if len(e["open_top"]) else e["c"][2] + 0.004
        dys = e["ridge"]["dy"]
        base = float(np.clip(np.median(e["ridge"]["z"]) - top - 0.003, 0.009, 0.013))
        z = top + base + 0.0015 * (1 - ((dys - 0.006) / 0.019) ** 2)
        return dict(dy=dys, z=z, above_mm=round(1000 * base, 1))

    def report(self):
        r = dict(eye_z=self.ez, eye_y=self.ey, eye_r_mm=round(1000 * float(np.mean([e["r"] for e in self.eyes])), 1),
                 nose_tip_z=self.tip[1], subnasale_z=self.subnasale, upper_lip_z=self.upper, mouth_z=self.mouth,
                 lower_lip_z=self.lower, fold_z=self.fold, chin_z=self.chin, upper_border_z=self.ul_top,
                 lower_border_z=self.ll_bot)
        r = {k: round(v, 4) if isinstance(v, float) else v for k, v in r.items()}
        r["brow_ridge_above_eye_mm"] = [round(1000 * float(np.mean(e["ridge"]["z"] - e["c"][2])), 1) for e in self.eyes]
        r["brow_above_lid_mm"] = [e["brow"]["above_mm"] for e in self.eyes]
        r["eye_open_mm"] = [round(1000 * float((e["open_top"] - e["open_bot"]).max()), 1) if len(e["open_top"]) else None
                            for e in self.eyes]
        return r


def at_y(e, y, key):
    """An open eye's edge at y (nan off the eye)."""
    ys = e["open_y"]
    if len(ys) == 0:
        return np.full(len(y), np.nan)
    v = np.interp(y, ys, e[key])
    return np.where((y >= ys.min()) & (y <= ys.max()), v, np.nan)


def fields(F, P, N, gap, lod):
    """For skin points P (facings N): each feature's weight, 0 to 1."""
    y, z, x = P[:, 1], P[:, 2], P[:, 0]
    ay = np.abs(y)
    n = len(P)
    on_front = smooth(1 - gap / 0.004)                    # (on the face's surface, not inside the mouth or nose)
    face_on = smooth((x - (F.tip[0] - 0.075)) / 0.02)      # (the front of the head)
    out = {k: np.zeros(n) for k in ("lash", "lash_low", "socket", "brow")}
    for e in F.eyes:
        c, r = e["c"], e["r"]
        dy = (y - c[1]) * np.sign(c[1])                    # (+ outward, away from the nose)
        dz = z - c[2]
        out["socket"] = np.maximum(out["socket"], gauss(dy ** 2 + (dz - 0.002) ** 2 * 1.6, 0.011))
        # the brow: low on the ridge and nearly level (a man's), a third thinner than before, thinning outward, soft
        # at both edges and both ends
        bz = np.interp(dy, e["brow"]["dy"], e["brow"]["z"])
        half = 0.0025 - 0.0012 * smooth((dy + 0.004) / 0.024)
        out["brow"] = np.maximum(out["brow"], band(z - bz, -half, half, 0.0016)
                                 * band(dy, -0.0150, 0.0210, 0.0030) * (x > c[0] - 0.012))
        if lod != "near":
            continue
        # the lash line: on the upper lid's edge over the open eye, a millimetre or so, heavier outward; a fainter
        # line along the lower lid
        t_up = z - at_y(e, y, "open_top")
        t_lo = at_y(e, y, "open_bot") - z
        th = 0.0009 + 0.0007 * smooth((dy + 0.002) / 0.010)
        fr = (x > c[0] + 0.3 * r) & (N[:, 0] > 0.1)
        lash = np.where(np.isfinite(t_up), (1 - smooth((t_up - th) / 0.0009)) * (t_up > -0.0004), 0.0) * fr
        low = np.where(np.isfinite(t_lo), (1 - smooth((t_lo - 0.0004) / 0.0007)) * (t_lo > -0.0003), 0.0) * fr
        out["lash"] = np.maximum(out["lash"], lash)
        out["lash_low"] = np.maximum(out["lash_low"], low * (1 - lash))
    out["socket"] *= face_on * (1 - out["lash"])
    out["brow"] *= face_on * smooth(N[:, 0] * 2 + 0.6)
    # the mouth: thin lips, the upper in shade, the lower catching light; a faint line between, soft corners
    wide = band(ay, 0.0, 0.017, 0.006)
    out["upper_lip"] = wide * band(z, F.mouth, F.ul_top, 0.0012) * on_front
    out["lower_lip"] = wide * band(z, F.ll_bot, F.mouth, 0.0015) * on_front
    out["line"] = band(ay, 0.0, 0.021, 0.004) * band(z, F.mouth - 0.0006, F.mouth + 0.0006, 0.0008)
    out["corner"] = gauss((ay - 0.023) ** 2 + (z - F.mouth) ** 2, 0.003) * on_front
    # the two creases kept: from beside the nose's wing to past the mouth's corner, and under the lower lip
    t = np.clip((F.subnasale + 0.002 - z) / (F.subnasale + 0.002 - (F.mouth - 0.008)), 0, 1)   # (0 at the wing, 1 low)
    fold_y = 0.019 + 0.011 * t
    out["nasolabial"] = gauss((ay - fold_y) ** 2, 0.0025) * band(z, F.mouth - 0.008, F.subnasale + 0.002, 0.004) * on_front \
        * (1 - 0.5 * t)
    out["under_lip"] = band(ay, 0.0, 0.014, 0.006) * gauss((z - F.fold) ** 2, 0.0025) * on_front
    # nostrils: under the tip, either side of the middle, facing down
    out["nostril"] = band(ay, 0.004, 0.010, 0.002) * band(z, F.subnasale, F.tip[1] - 0.006, 0.002) * smooth(-N[:, 2] * 3 - 0.6)
    out["nose"] = gauss(y ** 2 + (z - F.tip[1]) ** 2 + (x - F.tip[0]) ** 2, 0.009) * on_front
    cheek_z = F.ez - 0.032
    out["cheek"] = gauss((ay - (F.ey + 0.012)) ** 2 + (z - cheek_z) ** 2, 0.015) * on_front
    out["ear"] = smooth((ay - 0.062) / 0.01) * band(z, F.ez - 0.045, F.ez + 0.02, 0.012) * smooth((F.tip[0] - 0.05 - x) / 0.02)
    out["under_jaw"] = smooth((F.chin - z) / 0.012) * smooth(-N[:, 2] * 2) * smooth((x - 0.04) / 0.03)
    return out


# the colour each feature brings, as a multiplier on the skin (strength 1); mid takes them softer
TINTS = {
    "lash": (0.42, 0.38, 0.36),
    "lash_low": (0.80, 0.76, 0.75),
    "socket": (0.94, 0.91, 0.91),
    "upper_lip": (0.90, 0.83, 0.81),
    "lower_lip": (0.97, 0.91, 0.89),
    "line": (0.85, 0.80, 0.79),
    "corner": (0.90, 0.86, 0.85),
    "nasolabial": (0.92, 0.89, 0.88),
    "under_lip": (0.91, 0.88, 0.87),
    "nostril": (0.86, 0.80, 0.78),
    "nose": (1.04, 0.95, 0.93),
    "cheek": (1.04, 0.96, 0.94),
    "ear": (1.05, 0.92, 0.90),
    "under_jaw": (0.84, 0.82, 0.82),
}
MID_STRENGTH = 0.7
BROW = 0.75                 # (how far the brow goes from the skin to the hair's colour: brow hair is sparse)
TACHE = 0.88                # (and the moustache)


def front_gap(F, P):
    out = np.empty(len(P))
    for i, p in enumerate(P):
        f = F.front(p[1], p[2])
        out[i] = max(0.0, f - p[0]) if np.isfinite(f) else 1.0
    return out


def brim(D, lod, F, strength):
    """How much the headgear shades each point of the face: the share of the sky (weighted to overhead) that the
    helmet or cap hides from it, eased in, and gone by the cheekbones. Returns (points, darkening 0..strength)."""
    m = D[lod]
    P = np.array(m["points"], float).reshape(-1, 3)
    N = np.array(m["facings"], float).reshape(-1, 3)
    pa = np.array(m["paints"])
    part = np.array(m["part"])
    T = np.array(m["triangles"]).reshape(-1, 3)
    gear = np.zeros(len(T), bool)
    rng = (D.get("helmet_tris") or {}).get(lod)
    if rng:
        gear[rng[0]:rng[0] + rng[1]] = True
    head = part == F.hb
    gear |= head[T].all(1) & ~np.isin(pa[T], (SKIN, BOOTS, STRAPS, EYE_WHITE, IRIS)).any(1)
    C = P[T].mean(1)
    gear &= C[:, 2] > F.ez - 0.03
    if not gear.any():
        return np.zeros(0, int), np.zeros(0)
    bvh = bvh_of(P, T[gear])
    who = np.where(np.isin(part, (F.hb, F.nb)) & np.isin(pa, (SKIN, BOOTS, EYE_WHITE, IRIS)) &
                   (P[:, 2] > F.ez - 0.05) & (P[:, 0] > F.ex - 0.09))[0]
    # the sky: 96 directions over the upper half, denser overhead (a cosine weight), turned a little toward the front
    k = np.arange(96) + 0.5
    cz = np.sqrt(1 - k / 96)
    ph = k * math.pi * (3 - math.sqrt(5))
    s = np.sqrt(1 - cz ** 2)
    dirs = np.c_[s * np.cos(ph), s * np.sin(ph), cz]
    occ = np.zeros(len(who))
    for j, i in enumerate(who):
        n = N[i]
        o = Vector(P[i] + n * 0.0008)
        seen = dirs @ n > 0.05
        if not seen.any():
            continue
        hit = 0
        for d in dirs[seen]:
            if bvh.ray_cast(o, Vector(d), 0.35)[0] is not None:
                hit += 1
        occ[j] = hit / seen.sum()
    fade = smooth((P[who, 2] - (F.ez - 0.035)) / 0.027)
    return who, strength * smooth(occ / 0.5) * fade


def undo(D):
    """Take a previous run's work back off, where the file still holds it."""
    rec = D.pop("face_detail", None)
    if not rec:
        return False
    for lod, r in rec.get("bare_heads", {}).items():          # (version 1)
        m = D[lod]
        if sha(m["tint"]) == r["tint_after"]:
            T = np.array(m["tint"]).reshape(-1, 3)
            T[np.array(r["at"], int)] /= np.array(r["mul"]).reshape(-1, 3)
            m["tint"] = [round(float(v), 4) for v in T.ravel()]
    for lod, r in rec["lods"].items():
        m = D[lod]
        if sha(m["tint"]) == r["tint_after"]:
            T = np.array(m["tint"]).reshape(-1, 3)
            at = np.array(r["at"], int)
            T[at] /= np.array(r["mul"]).reshape(-1, 3)
            m["tint"] = [round(float(v), 4) for v in T.ravel()]
        for i, p in r.get("paints", {}).items():
            if m["paints"][int(i)] in (EYE_WHITE, IRIS):
                m["paints"][int(i)] = p
        for i, s in r.get("shade", {}).items():
            m["shade"][int(i)] = s
    for k, fr in rec["faces"].items():
        f = D["faces"][int(k)]
        for lod, r in fr.items():
            L = f[lod]
            if sha(L.get("tint", [])) == r["after"]:
                L["tint_at"], L["tint"] = r["tint_at"], r["tint"]
    return True


def eyes(D, F, rec):
    """The near eyeballs: iris and white paints, shade as the skin's about the eye, and the mesh's tint over them
    (pupil, the iris's rim, the corners, the band under the upper lid, a little over the lower)."""
    m = D["near"]
    P = np.array(m["points"], float).reshape(-1, 3)
    pa = np.array(m["paints"])
    part = np.array(m["part"])
    sh = np.array(m["shade"], float)
    skin = (pa == SKIN) & (part == F.hb)
    at, mul, n_iris = [], [], 0
    info = []
    for e in F.eyes:
        c, r, ax, deg = e["c"], e["r"], e["axis"], e["deg"]
        near_skin = skin & (np.linalg.norm(P - c, axis=1) < r + 0.012) & (P[:, 0] > c[0] + 0.3 * r)
        s_eye = float(np.median(sh[near_skin])) if near_skin.any() else 0.8
        half = e["W"] / 2
        Ri = r * math.sin(math.radians(deg))
        for i in e["pts"]:
            d = P[i] - c
            u = d / (np.linalg.norm(d) + 1e-12)
            ang = math.degrees(math.acos(float(np.clip(u @ ax, -1, 1))))
            rec["paints"][str(int(i))] = int(pa[i])
            rec["shade"][str(int(i))] = m["shade"][i]
            m["shade"][i] = round(s_eye, 3)
            k = np.ones(3)
            if ang < deg:
                m["paints"][i] = IRIS
                n_iris += 1
                if ang < 0.42 * deg:
                    k *= 0.35                                  # (the pupil)
                elif ang > 0.80 * deg:
                    k *= 0.78                                  # (the iris's darker rim)
            else:
                m["paints"][i] = EYE_WHITE
                # toward the corners, the white in shade
                k *= 1 - 0.42 * float(smooth((abs(d[1]) - Ri) / max(half - Ri, 1e-3)))
            # under the upper lid's edge a band of shadow, over the lower a little
            top = at_y(e, np.array([P[i, 1]]), "open_top")[0]
            bot = at_y(e, np.array([P[i, 1]]), "open_bot")[0]
            if np.isfinite(top):
                k *= 1 - 0.45 * float(smooth((P[i, 2] - (top - 0.0019)) / 0.0013))
                k *= 1 - 0.15 * float(smooth(((bot + 0.0009) - P[i, 2]) / 0.0007))
            if np.abs(k - 1).max() > 0.003:
                at.append(int(i))
                mul.append(k)
        info.append(dict(shade=round(s_eye, 3), iris_mm=round(1000 * Ri, 2)))
    return np.array(at, int), (np.array(mul) if mul else np.zeros((0, 3))), n_iris, info


def iris_colour(hair_hex, slot, east):
    if east:
        return IRISES["dark" if slot % 2 == 0 else "brown"]
    h = HAIR.get(hair_hex.upper(), "brown")
    if h in ("fair", "light", "red"):
        return IRISES["blue_grey" if slot % 2 == 0 else "grey_green"]
    if h in ("brown", "mid"):
        return IRISES["hazel" if slot % 3 == 0 else "brown"]
    return IRISES["dark" if slot % 2 == 0 else "brown"]


def tache_field(F, P, gap):
    """A full moustache: from the upper lip's border up toward the nose, out past the mouth's corners, drooping a
    little and thinner at the ends; the lower edge clear."""
    y, z = P[:, 1], P[:, 2]
    ay = np.abs(y)
    bottom = F.ul_top - 0.35 * np.maximum(0.0, ay - 0.012)
    top = (F.subnasale - 0.0015) - 0.45 * np.maximum(0.0, ay - 0.006)
    v = smooth((z - bottom) / 0.0006) * (1 - smooth((z - top) / 0.0018))
    lat = 1 - smooth((ay - 0.021) / 0.005)
    return v * lat * smooth(1 - gap / 0.004) * (P[:, 0] > F.tip[0] - 0.045)


def blend_face_tint(L, idx, w, ratio):
    """Into one face's tint at points idx: toward `ratio` by weights w (each point either listed already or new)."""
    ta = list(L.get("tint_at", []))
    tt = list(L.get("tint", []))
    pos = {int(i): n for n, i in enumerate(ta)}
    ratio = np.asarray(ratio, float)
    for i, wv in zip(idx, w):
        wv = float(min(1.0, wv))
        if wv <= 0.01:
            continue
        rt = ratio if ratio.ndim == 1 else ratio[0]
        if int(i) in pos:
            n = pos[int(i)]
            old = np.array(tt[3 * n:3 * n + 3], float)
            new = old + wv * (rt - old)
            tt[3 * n:3 * n + 3] = [round(float(v), 4) for v in new]
        else:
            new = 1 + wv * (rt - 1)
            pos[int(i)] = len(ta)
            ta.append(int(i))
            tt += [round(float(v), 4) for v in new]
    L["tint_at"], L["tint"] = ta, tt


def mul_face_tint(L, idx, mul):
    """Multiply one face's tint at points idx by mul (n x 3)."""
    ta = list(L.get("tint_at", []))
    tt = list(L.get("tint", []))
    pos = {int(i): n for n, i in enumerate(ta)}
    for i, k in zip(idx, mul):
        if int(i) in pos:
            n = pos[int(i)]
            tt[3 * n:3 * n + 3] = [round(float(a) * float(b), 4) for a, b in zip(tt[3 * n:3 * n + 3], k)]
        else:
            pos[int(i)] = len(ta)
            ta.append(int(i))
            tt += [round(float(v), 4) for v in k]
    L["tint_at"], L["tint"] = ta, tt


def bare_heads(D, src, lod_mul, F, W_brow, log=print):
    """A file with no `faces` (the sailors and tankers) had no hair, brows or beard shadow at all: give its one man
    variety.py's hair (dark; black for the East Asian armies) at every level and a day's stubble at near and mid, and
    the brows on his ridge (W_brow, by level), all in the mesh's tint."""
    sys.path.insert(0, HERE)
    sys.path.insert(0, os.environ.get("VARIETY_DIR", os.path.join(HERE, "src")))   # (variety.py, from the looks branch)
    import mhhead  # noqa: F401  (human on the path)
    import human
    import variety as V
    S = D.get("face_mesh") or json.load(open(os.path.join(HERE, "seeds.json")))[os.path.basename(src).replace("_gore.json", ".json")]
    base = V.Man(human.look_for(S["seed"], S["nation"]))
    east = S["nation"] == "JAP"
    hair = V.hex_rgb(V.HAIR["black" if east else "dark"])
    skin = V.hex_rgb(D.get("colours", {}).get("skin") or V.GAME_SKIN)
    light = float(hair @ (0.3, 0.59, 0.11))
    out = {}
    ratio = np.clip(hair * 0.9 / skin, 0.18, 1.1)
    ratio = 0.75 * ratio + 0.25 * ratio.mean()
    for lod in ("near", "mid", "far"):
        m = D[lod]
        P = np.array(m["points"], float).reshape(-1, 3) * (1, -1, 1)
        pa = np.array(m["paints"])
        idx = np.where((pa == SKIN) & (P[:, 2] > 1.3))[0]
        tri, bary, dist = base.where(P[idx])
        mk = V.face_marks(base, base.at(tri, bary))
        col = np.tile(skin, (len(idx), 1))
        layers = [(mk["hair"] * 0.92, hair)]
        if lod != "far":
            a_beard = mk["beard"] * BASE_STUBBLE * 0.3 * (1 - 0.7 * V.smooth(light, 0.15, 0.5))
            layers.insert(0, (a_beard, hair * 0.45 + skin * 0.55 * 0.7))
        for a, c in layers:
            col = col * (1 - a[:, None]) + c[None, :] * a[:, None]
        mul = col / skin[None, :]
        lod_mul[lod][idx] *= mul
        nb = 0
        if lod in W_brow:
            bi, bw = W_brow[lod]
            w = np.clip(bw * BROW, 0, 1)[:, None]
            lod_mul[lod][bi] *= 1 + w * (ratio[None, :] - 1)
            nb = int((bw > 0.05).sum())
        out[lod] = int((np.abs(mul - 1).max(1) > 0.02).sum())
        log(f"   {lod}: hair and stubble on {out[lod]} points, brows on {nb} (no faces in this file)")
    return out


def apply(D, log=print, src=None):
    undone = undo(D)
    F = Face(D)
    name = os.path.basename(src or D.get("name", "")).replace("_gore.json", "").replace(".json", "")
    has_gear = bool((D.get("helmet_tris") or {}).get("near"))
    kind = "helmet" if has_gear and not name.startswith(CAPS) else "cap"
    rec = {"version": VERSION, "landmarks": F.report(), "brim": {"kind": kind, "most": BRIM[kind]}, "lods": {}, "faces": {}}
    faces = D.get("faces", [])
    east = name.startswith(("soldier_JAP", "soldier_CHN"))
    lod_mul, W_brow, mouth_W = {}, {}, None
    for lod in ("near", "mid", "far"):
        lod_mul[lod] = np.ones((len(D[lod]["points"]) // 3, 3))
    for lod in LODS:
        m = D[lod]
        P = np.array(m["points"], float).reshape(-1, 3)
        N = np.array(m["facings"], float).reshape(-1, 3)
        pa = np.array(m["paints"])
        part = np.array(m["part"])
        r = {"paints": {}, "shade": {}}
        skin = np.where((pa == SKIN) & np.isin(part, [F.hb, F.nb]))[0]
        gap = front_gap(F, P[skin])
        W = fields(F, P[skin], N[skin], gap, lod)
        k = 1.0 if lod == "near" else MID_STRENGTH
        mul = np.ones((len(skin), 3))
        for nm, rgb in TINTS.items():
            w = np.clip(W[nm] * k, 0, 1)[:, None]
            mul *= 1 + w * (np.array(rgb) - 1)
        lod_mul[lod][skin] *= mul
        info = None
        if lod == "near":
            e_at, e_mul, n_iris, info = eyes(D, F, r)
            if len(e_at):
                lod_mul[lod][e_at] *= e_mul
            mouth_W = (skin, gap)
        # the headgear's shade over the brow and the eyes
        b_at, b_k = brim(D, lod, F, BRIM[kind])
        if len(b_at):
            lod_mul[lod][b_at] *= 1 - b_k[:, None] * np.array([1.0, 0.96, 0.90])[None, :]
        sel = W["brow"] > 0.02
        W_brow[lod] = (skin[sel], W["brow"][sel] * (1.0 if lod == "near" else MID_STRENGTH))
        rec["lods"][lod] = r
        log(f"   {lod}: {len(skin)} skin points; brim shade on {int((b_k > 0.02).sum()) if len(b_at) else 0} "
            f"(most {float(b_k.max()) if len(b_at) else 0:.2f})" + (f"; eyes {info}, {n_iris} iris points" if info else ""))
    if not faces:
        if src is not None:
            rec["bare_heads"] = bare_heads(D, src, lod_mul, F, W_brow, log)
    else:
        # each face (near): brows in his hair's colour on the ridge, his moustache, his iris's colour, his whites kept
        # under his skin's brightness
        m = D["near"]
        P = np.array(m["points"], float).reshape(-1, 3)
        pa = np.array(m["paints"])
        skin, gap = mouth_W
        tw = tache_field(F, P[skin], gap)
        bi, bw = W_brow["near"]
        lips = (D.get("face_mesh") or {}).get("lips", {})
        iris_pts = np.where(pa == IRIS)[0]
        white_pts = np.where(pa == EYE_WHITE)[0]
        skin_rgb = np.array([0xC4, 0x9A, 0x78], float)        # (a mid skin; the ratio is what counts)
        for fk, f in enumerate(faces):
            L = f["near"]
            fr = rec["faces"].setdefault(str(fk), {})
            fr["near"] = {"tint_at": list(L.get("tint_at", [])), "tint": list(L.get("tint", []))}
            tone = np.array(f.get("tone", [1, 1, 1]), float)
            hair = hexrgb(f.get("hair", "3A2E24"))
            ratio = np.clip(hair / (skin_rgb * tone), 0.18, 1.1)
            brow = 0.75 * ratio + 0.25 * ratio.mean()             # (brows a little greyer than the hair's hue)
            blend_face_tint(L, bi, bw * BROW, brow)
            sh, ta = lips.get(str(fk), [0.0, 0.0])
            if ta >= 0.9:
                sel = tw > 0.02
                blend_face_tint(L, skin[sel], tw[sel] * TACHE, ratio * 0.85)
            ic = hexrgb(iris_colour(f.get("hair", "4A3727"), fk, east)) / hexrgb(COLOURS["iris"])
            mul_face_tint(L, iris_pts, np.tile(ic, (len(iris_pts), 1)))
            lum = float(tone @ (0.3, 0.59, 0.11))
            wf = float(np.clip(0.55 + 0.45 * lum, 0.85, 1.0))
            if wf < 0.995:
                mul_face_tint(L, white_pts, np.full((len(white_pts), 3), wf))
            fr["near"]["after"] = sha(L["tint"])
            fr["near"]["iris"] = iris_colour(f.get("hair", "4A3727"), fk, east)
    for lod in LODS:
        m = D[lod]
        M = lod_mul[lod]
        at = np.where(np.abs(M - 1).max(1) > 0.003)[0]
        T = np.array(m["tint"], float).reshape(-1, 3)
        T[at] *= M[at]
        m["tint"] = [round(float(v), 4) for v in T.ravel()]
        rec["lods"][lod].update(at=[int(i) for i in at], mul=[round(float(v), 5) for v in M[at].ravel()],
                                tint_after=sha(m["tint"]))
    if not faces:
        m = D["far"]
        M = lod_mul["far"]
        at = np.where(np.abs(M - 1).max(1) > 0.003)[0]
        T = np.array(m["tint"], float).reshape(-1, 3)
        T[at] *= M[at]
        m["tint"] = [round(float(v), 4) for v in T.ravel()]
        rec["lods"]["far"] = dict(at=[int(i) for i in at], mul=[round(float(v), 5) for v in M[at].ravel()],
                                  tint_after=sha(m["tint"]))
    D.setdefault("new_paints", {})
    D.setdefault("colours", {})
    D["new_paints"].update({"eye_white": EYE_WHITE, "iris": IRIS})
    D["colours"].update(COLOURS)
    D["face_detail"] = rec
    return rec, undone


if __name__ == "__main__":
    src, dst = sys.argv[1], sys.argv[2]
    D = json.load(open(src))
    rec, undone = apply(D, src=src)
    print(os.path.basename(src), "landmarks", json.dumps(rec["landmarks"]), "(took the last run off first)" if undone else "")
    tmp = dst + ".tmp"
    with open(tmp, "w") as f:
        json.dump(D, f, separators=(",", ":"))
    json.load(open(tmp))
    os.replace(tmp, dst)
