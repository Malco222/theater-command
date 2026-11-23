"""Faces that read as men, not mannequins (Brian, 23 November 2026: "The faces need a lot of work").

The game colours a soldier's point as its paint's colour x shade x (skin: the face's tone) x the face's tint where the
face lists it x the mesh's own tint (LandMen MakeLook). A face had only a faint brow and stubble tint in each of the
eight faces: the eyes were dark slits (the eyeballs painted as boots), the brows barely darker than the skin, the lips
the skin's own colour. This tool gives every soldier file, at near and mid, with no new point or triangle:

- eyes: the eyeball's front (iris, the pupil darker) in paint 47 `iris` and the rest in paint 46 `eye_white`, both the
  file's own paints (`new_paints` and `colours`, which LandMen reads); the eyeball's shade lifted so the white reads;
- in the mesh's tint (so on all eight faces, moving with each face's shape): the line of the upper lid and lashes, a
  soft shadow in the eye sockets, nostrils, the lips (the upper a little darker) and the line between them, the
  mouth's corners, warmth on the nose, cheeks and ears, a shadow under the jaw;
- in each face's tint: brows in that face's hair colour, thick enough to read from the close camera.

Where the features are is found on the file's own face: the eyeballs' centres, and the face's front surface (rays
from in front, 1 mm apart) down the middle for the nose tip, the lips, the line between them and the chin.

    /home/user/bpyenv/bin/python face_detail.py IN.json OUT.json

Run after variety.py and shade.py (they write the faces and the mesh's tint afresh). It records what it did under
`face_detail`; run again on its own output it first takes its own work back off, so it never doubles up.
"""
import hashlib, json, math, os, sys
import numpy as np
import bpy  # noqa: F401  (Blender as a module: mathutils)
from mathutils import Vector
from mathutils.bvhtree import BVHTree

VERSION = 1
SKIN, BOOTS = 7, 2
EYE_WHITE, IRIS = 46, 47
COLOURS = {"eye_white": "D6D0C4", "iris": "4A3D31"}
LODS = ("near", "mid")


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


class Face:
    """The landmarks of a file's face, from its near mesh (face 0)."""

    def __init__(self, D):
        m = D["near"]
        self.hb = [b["name"] for b in D["bones"]].index("head")
        P = np.array(m["points"], float).reshape(-1, 3)
        pa = np.array(m["paints"])
        part = np.array(m["part"])
        T = np.array(m["triangles"]).reshape(-1, 3)
        eye = np.where((part == self.hb) & (pa == BOOTS))[0]
        self.eyes = []
        for sel in (P[eye, 1] < 0, P[eye, 1] > 0):
            e = eye[sel]
            c, r = sphere(P[e])
            pole = P[e][np.argmax(P[e, 0])]
            self.eyes.append(dict(c=c, r=r, axis=(pole - c) / np.linalg.norm(pole - c)))
        self.ez = float(np.mean([e["c"][2] for e in self.eyes]))
        self.ey = float(np.mean([abs(e["c"][1]) for e in self.eyes]))
        head = np.where(part == self.hb)[0]
        tri = T[np.isin(T, head).all(1) & np.isin(pa[T], (SKIN,)).all(1)]
        self.bvh = BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in tri])
        # the front surface down the middle, a millimetre apart
        zs = np.arange(self.ez + 0.02, self.ez - 0.14, -0.001)
        xs = np.array([self.front(0.0, z) for z in zs])
        ok = np.isfinite(xs)
        zs, xs = zs[ok], xs[ok]
        below = zs < self.ez - 0.02
        i_tip = np.argmax(np.where(below & (zs > self.ez - 0.075), xs, -1))
        self.tip = (float(xs[i_tip]), float(zs[i_tip]))
        # under the tip: the lip's top (subnasale: least x), the upper lip, the line (least x), the lower lip, the fold
        j = i_tip + np.argmin(xs[i_tip:i_tip + 22])
        self.subnasale = float(zs[j])
        k_ul = j + np.argmax(xs[j:j + 16])
        k_ml = k_ul + np.argmin(xs[k_ul:k_ul + 12])
        k_ll = k_ml + np.argmax(xs[k_ml:k_ml + 14])
        k_fold = k_ll + np.argmin(xs[k_ll:k_ll + 16])
        k_chin = k_fold + np.argmax(xs[k_fold:k_fold + 22])
        self.upper, self.mouth, self.lower = float(zs[k_ul]), float(zs[k_ml]), float(zs[k_ll])
        self.fold, self.chin = float(zs[k_fold]), float(zs[k_chin])
        self.profile = (zs, xs)

    def front(self, y, z):
        hit = self.bvh.ray_cast(Vector((0.4, y, z)), Vector((-1.0, 0.0, 0.0)), 0.5)[0]
        return float(hit.x) if hit is not None else float("nan")

    def report(self):
        return {k: round(v, 4) if isinstance(v, float) else v for k, v in dict(
            eye_z=self.ez, eye_y=self.ey, eye_r_mm=round(1000 * float(np.mean([e["r"] for e in self.eyes])), 1),
            nose_tip_z=self.tip[1], subnasale_z=self.subnasale, upper_lip_z=self.upper, mouth_z=self.mouth,
            lower_lip_z=self.lower, fold_z=self.fold, chin_z=self.chin).items()}


def sphere(X):
    """Least-squares sphere through points: centre, radius."""
    A = np.c_[2 * X, np.ones(len(X))]
    b = (X ** 2).sum(1)
    s = np.linalg.lstsq(A, b, rcond=None)[0]
    c = s[:3]
    return c, float(np.sqrt(s[3] + c @ c))


def fields(F, P, N, front_gap):
    """For points P (facings N): each feature's weight, 0 to 1."""
    y, z = P[:, 1], P[:, 2]
    ay = np.abs(y)
    on_front = smooth(1 - front_gap / 0.004)              # (on the face's surface, not inside the mouth or nose)
    out = {}
    # the upper lid's line and lashes: a thin arc over each eye's opening
    lid, sock, brow = np.zeros(len(P)), np.zeros(len(P)), np.zeros(len(P))
    for e in F.eyes:
        dy = (y - e["c"][1]) * np.sign(e["c"][1])           # (+ outward, away from the nose)
        dz = z - e["c"][2]
        r = e["r"]
        arc = 0.55 * r * (1 - (dy / (1.15 * r)) ** 2)        # (the opening's top, highest a little out from the middle)
        lid = np.maximum(lid, band(dz - arc, -0.0015, 0.0022, 0.002) * band(dy, -0.9 * r, 1.05 * r, 0.003))
        sock = np.maximum(sock, gauss(dy ** 2 + (dz - 0.002) ** 2 * 1.6, 0.012))
        # the brow: from over the inner corner to past the outer, a little arched, thinning outward
        top = 0.017 + 0.004 * (1 - ((dy - 0.004) / 0.024) ** 2)
        thick = 0.0042 * (1 - 0.45 * smooth((dy + 0.012) / 0.034))
        brow = np.maximum(brow, band(dz - top, -thick, thick, 0.0025) * band(dy, -0.016, 0.022, 0.004))
    face_on = smooth((P[:, 0] - (F.tip[0] - 0.075)) / 0.02)   # (the front of the head)
    out["lid"] = lid * face_on
    out["socket"] = sock * face_on * (1 - out["lid"])
    out["brow"] = brow * face_on * smooth(N[:, 0] * 2 + 0.6)
    # the mouth: lips as wide as the mouth (about 5 cm), the upper from the line to the upper lip's top, the lower to
    # the fold
    wide = band(ay, 0.0, 0.019, 0.007)
    ul_top = F.upper + 0.45 * (F.subnasale - F.upper)
    ll_bot = F.lower - 0.5 * (F.lower - F.fold)
    out["upper_lip"] = wide * band(z, F.mouth, ul_top, 0.0018) * on_front
    out["lower_lip"] = wide * band(z, ll_bot, F.mouth, 0.002) * on_front
    out["line"] = band(ay, 0.0, 0.022, 0.004) * band(z, F.mouth - 0.0012, F.mouth + 0.0012, 0.0016)
    out["corner"] = gauss((ay - 0.024) ** 2 + (z - F.mouth) ** 2, 0.004) * on_front
    # nostrils: under the tip, either side of the middle, facing down
    out["nostril"] = band(ay, 0.004, 0.011, 0.002) * band(z, F.subnasale, F.tip[1] - 0.006, 0.002) * smooth(-N[:, 2] * 3 - 0.6)
    out["nose"] = gauss(y ** 2 + (z - F.tip[1]) ** 2 + (P[:, 0] - F.tip[0]) ** 2, 0.009) * on_front
    cheek_z = F.ez - 0.032
    out["cheek"] = gauss((ay - (F.ey + 0.012)) ** 2 + (z - cheek_z) ** 2, 0.015) * on_front
    out["ear"] = smooth((ay - 0.062) / 0.01) * band(z, F.ez - 0.045, F.ez + 0.02, 0.012) * smooth((F.tip[0] - 0.05 - P[:, 0]) / 0.02)
    out["under_jaw"] = smooth((F.chin - z) / 0.012) * smooth(-N[:, 2] * 2) * smooth((P[:, 0] - 0.04) / 0.03)
    return out


# the colour each feature brings, as a multiplier on the skin (strength 1); mid takes them softer
TINTS = {
    "lid": (0.50, 0.46, 0.44),
    "socket": (0.90, 0.86, 0.86),
    "upper_lip": (0.95, 0.85, 0.83),
    "lower_lip": (1.00, 0.88, 0.86),
    "line": (0.55, 0.47, 0.46),
    "corner": (0.80, 0.74, 0.73),
    "nostril": (0.86, 0.80, 0.78),
    "nose": (1.04, 0.95, 0.93),
    "cheek": (1.04, 0.96, 0.94),
    "ear": (1.05, 0.92, 0.90),
    "under_jaw": (0.84, 0.82, 0.82),
}
MID_STRENGTH = 0.7
BROW = 0.88                 # (how far the brow goes from the skin to the hair's colour)


def hexrgb(h):
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float)


def front_gap(F, P):
    return np.array([max(0.0, F.front(p[1], p[2]) - p[0]) if np.isfinite(F.front(p[1], p[2])) else 1.0 for p in P])


def undo(D):
    """Take a previous run's work back off, where the file still holds it."""
    rec = D.pop("face_detail", None)
    if not rec:
        return False
    for lod, r in rec.get("bare_heads", {}).items():
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
        for i, p in r["paints"].items():
            if m["paints"][int(i)] in (EYE_WHITE, IRIS):
                m["paints"][int(i)] = p
        for i, s in r["shade"].items():
            m["shade"][int(i)] = s
    for k, fr in rec["faces"].items():
        f = D["faces"][int(k)]
        for lod, r in fr.items():
            L = f[lod]
            if sha(L.get("tint", [])) == r["after"]:
                L["tint_at"], L["tint"] = r["tint_at"], r["tint"]
    return True


def eyes(D, F, lod, rec):
    m = D[lod]
    P = np.array(m["points"], float).reshape(-1, 3)
    pa = np.array(m["paints"])
    part = np.array(m["part"])
    eye = np.where((part == F.hb) & (pa == BOOTS))[0]
    n_iris = 0
    for i in eye:
        e = F.eyes[0] if P[i, 1] < 0 else F.eyes[1]
        d = P[i] - e["c"]
        ang = math.degrees(math.acos(np.clip(d @ e["axis"] / (np.linalg.norm(d) + 1e-9), -1, 1)))
        lim = 36.0 if lod == "near" else 25.0
        rec["paints"][str(int(i))] = int(pa[i])
        rec["shade"][str(int(i))] = m["shade"][i]
        if ang < lim:
            m["paints"][i] = IRIS
            n_iris += 1
        else:
            m["paints"][i] = EYE_WHITE
        lift = 0.78 if lod == "near" else 0.6
        m["shade"][i] = round(max(m["shade"][i], lift), 3)
    # the pupil: the eyeball's very front a good deal darker (in the tint)
    return eye, n_iris


def bare_heads(D, src, rec, log=print):
    """A file with no `faces` (the sailors and tankers) had no hair, brows or beard shadow at all: give its one man
    variety.py's hair and brows (dark; black for the East Asian armies) in the mesh's tint, at every level."""
    if D.get("faces"):
        return
    here = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, here)
    sys.path.insert(0, os.environ.get("VARIETY_DIR", os.path.join(here, "src")))
    import mhhead  # noqa: F401  (human on the path)
    import human
    import variety as V
    S = D.get("face_mesh") or json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "seeds.json")))[os.path.basename(src)]
    base = V.Man(human.look_for(S["seed"], S["nation"]))
    east = S["nation"] == "JAP"
    hair = V.hex_rgb(V.HAIR["black" if east else "dark"])
    skin = V.hex_rgb(D.get("colours", {}).get("skin") or V.GAME_SKIN)
    out = {}
    for lod in ("near", "mid", "far"):
        m = D[lod]
        P = np.array(m["points"], float).reshape(-1, 3) * (1, -1, 1)
        pa = np.array(m["paints"])
        T = np.array(m["triangles"]).reshape(-1, 3)
        idx = np.where((pa == SKIN) & (P[:, 2] > 1.3))[0]
        tri, bary, dist = base.where(P[idx])
        mk = V.face_marks(base, base.at(tri, bary))
        col = np.tile(skin, (len(idx), 1))
        for a, c in ((mk["brow"] * 0.7, hair * 0.9), (mk["hair"] * 0.92, hair)):
            col = col * (1 - a[:, None]) + c[None, :] * a[:, None]
        mul = col / skin[None, :]
        keep = np.abs(mul - 1).max(1) > 0.02
        at, mm = idx[keep], mul[keep]
        Tn = np.array(m["tint"], float).reshape(-1, 3)
        Tn[at] *= mm
        m["tint"] = [round(float(v), 4) for v in Tn.ravel()]
        out[lod] = dict(at=[int(i) for i in at], mul=[round(float(v), 5) for v in mm.ravel()], tint_after=sha(m["tint"]))
        log(f"   {lod}: hair and brows on {len(at)} points (no faces in this file)")
    rec["bare_heads"] = out


def apply(D, log=print, src=None):
    undone = undo(D)
    F = Face(D)
    rec = {"version": VERSION, "landmarks": F.report(), "lods": {}, "faces": {}}
    skin_rgb = None
    for lod in LODS:
        m = D[lod]
        P = np.array(m["points"], float).reshape(-1, 3)
        N = np.array(m["facings"], float).reshape(-1, 3)
        pa = np.array(m["paints"])
        part = np.array(m["part"])
        r = {"paints": {}, "shade": {}}
        if lod == "near":
            eye, n_iris = eyes(D, F, lod, r)
        else:
            eye, n_iris = np.zeros(0, int), 0
        skin = np.where((pa == SKIN) & np.isin(part, [F.hb, [b["name"] for b in D["bones"]].index("neck")]))[0]
        gap = front_gap(F, P[skin])
        W = fields(F, P[skin], N[skin], gap)
        k = 1.0 if lod == "near" else MID_STRENGTH
        mul = np.ones((len(skin), 3))
        for name, rgb in TINTS.items():
            w = np.clip(W[name] * k, 0, 1)[:, None]
            mul *= 1 + w * (np.array(rgb) - 1)
        # the pupil
        pm = np.ones((len(eye), 3))
        for j, i in enumerate(eye):
            e = F.eyes[0] if P[i, 1] < 0 else F.eyes[1]
            d = (P[i] - e["c"]) / (np.linalg.norm(P[i] - e["c"]) + 1e-9)
            if d @ e["axis"] > math.cos(math.radians(12)):
                pm[j] = 0.45
        at = np.concatenate([skin, eye])
        mm = np.concatenate([mul, pm])
        keep = np.abs(mm - 1).max(1) > 0.003
        at, mm = at[keep], mm[keep]
        T = np.array(m["tint"], float).reshape(-1, 3)
        T[at] *= mm
        m["tint"] = [round(float(v), 4) for v in T.ravel()]
        r.update(at=[int(i) for i in at], mul=[round(float(v), 5) for v in mm.ravel()], tint_after=sha(m["tint"]))
        rec["lods"][lod] = r
        # the brows, in each face's hair colour
        # (at near the faces' own brows, drawn afresh on the finer face by face_mesh.py, are enough; mid's few points
        # over each eye take a soft brow)
        bw = W["brow"] * (0.0 if lod == "near" else MID_STRENGTH)
        bi = skin[bw > 0.02]
        bwv = bw[bw > 0.02]
        for fk, f in enumerate(D.get("faces", [])):
            L = f[lod]
            tone = np.array(f.get("tone", [1, 1, 1]), float)
            if skin_rgb is None:
                skin_rgb = np.array([0xC4, 0x9A, 0x78], float)     # (a mid skin; the ratio is what counts)
            hair = hexrgb(f.get("hair", "3A2E24"))
            ratio = np.clip(hair / (skin_rgb * tone), 0.18, 1.1)
            ratio = 0.75 * ratio + 0.25 * ratio.mean()             # (brows a little greyer than the hair's hue)
            ta = list(L.get("tint_at", []))
            tt = list(L.get("tint", []))
            fr = rec["faces"].setdefault(str(fk), {})
            fr[lod] = {"tint_at": list(ta), "tint": list(tt)}
            pos = {int(i): n for n, i in enumerate(ta)}
            for i, w in zip(bi, bwv):
                w = min(1.0, float(w) * BROW)
                if int(i) in pos:
                    n = pos[int(i)]
                    old = np.array(tt[3 * n:3 * n + 3], float)
                    new = old + w * (ratio - old)
                    tt[3 * n:3 * n + 3] = [round(float(v), 4) for v in new]
                else:
                    new = 1 + w * (ratio - 1)
                    ta.append(int(i))
                    tt += [round(float(v), 4) for v in new]
            L["tint_at"], L["tint"] = ta, tt
            fr[lod]["after"] = sha(tt)
        log(f"   {lod}: eyes {len(eye)} points ({n_iris} iris), {len(at)} points tinted, brows on {len(bi)} points")
    D.setdefault("new_paints", {})
    D.setdefault("colours", {})
    if src is not None:
        bare_heads(D, src, rec, log)
    D["new_paints"].update({"eye_white": EYE_WHITE, "iris": IRIS})
    D["colours"].update(COLOURS)
    D["face_detail"] = rec
    return rec, undone


if __name__ == "__main__":
    src, dst = sys.argv[1], sys.argv[2]
    D = json.load(open(src))
    rec, undone = apply(D, src=src)
    print(os.path.basename(src), "landmarks", rec["landmarks"], "(took the last run off first)" if undone else "")
    tmp = dst + ".tmp"
    with open(tmp, "w") as f:
        json.dump(D, f, separators=(",", ":"))
    json.load(open(tmp))
    os.replace(tmp, dst)
