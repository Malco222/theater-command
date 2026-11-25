"""The light machine guns the gunners carry (release 2, Brian, 25 November 14:15Z: "There should be a machine gun teams
that lay down suppressing fire, such as the German MG 42 and the US machine gun"). Our own drawings, to period
proportions, built from simple solids at the three levels of a soldier file, in the weapon bone's frame of the shared
skeleton: metres, x along the bore toward the muzzle, y to the gunner's right, z up, the origin where the rifleman's
right fist closes on his rifle's wrist (the skeleton's `weapon` bone, socket_weapon). The bipod's legs ride the
`bipod` (the left) and `bolt` (the right) bones, both heads at the gun's own hinge under the barrel (`Gun.hinge`: the
gunner's file moves the bones' heads there), so they fold back under the barrel at rest and come down about the hinge,
splayed apart, in `lmg_burst` and `fire_prone`.

Each gun gives, per level, its points (with facings, paints, bones and whether a point is a dark hollow: a cooling
hole, the bore) and triangles, and the places the hands go (`grips`): the pistol grip's bar for the right fist, the
lines along the gun's underside for a supporting hand, the butt's underside for the left hand lying behind it.

    python3 mg_guns.py --report     every gun's size at each level
"""
import math
import sys

import numpy as np

BORE = -0.010                # (the bore's height over the rifleman's fist (the rifle's is 0.040): the gun's butt is
                             # straight behind its bore and its sights stand high over it, so it sits 5 cm lower than
                             # the rifle for its butt to meet his shoulder, his cheek to lie on the butt and the sights
                             # to come to his eye as the rifle's do in the aimed clips)
BAKELITE = "2A1D17"          # (the butts and grips: dark red-brown moulded plastic, lit well under the receiver's steel;
                             # the art director's round 1, 25 November)
BRASS = "9C7A3C"             # (the cartridges' cases)
BULLET = "6A4630"            # (their bullets: gilding metal, darker than the brass)
# (the MG 42's barrel jacket: squared, one depth from the receiver to the booster, four fifths of the receiver's depth;
# its underside in line with the receiver's)
JACKET = 0.025               # (half across)
JACKET_UP = 0.032            # (half up)
JACKET_Z = 0.005             # (its centre over the bore)
LEG = 0.290                  # (a bipod leg, hinge to foot: folded back under the jacket at rest; in lmg_burst and
                             # fire_prone each leg is keyed down and out to the ground, the feet about 28 cm apart)
SPREAD = 0.28
LEFT_LEG, RIGHT_LEG = "bipod", "bolt"    # (each leg its own bone, so the two splay apart: the left on the skeleton's
                                         # `bipod`, the right on its `bolt`, the gun having no bolt to work)


def hinge_of(b, under, x=0.600):
    """The bipod's hinge under the barrel (its bones' head): `under` is the barrel's underside over the bore."""
    return np.array((x, 0.0, b + under - 0.006))


def leg_end(hinge):
    """Where the folded legs' feet lie along the gun."""
    return float(hinge[0]) - 0.004 - LEG


HINGE = hinge_of(BORE, JACKET_Z - JACKET_UP)
LEG_END = leg_end(HINGE)


class Piece:
    """One solid: its own points (a point shared by two triangles is shaded smooth across them; a flat face has its own
    points), triangles wound outward (counter-clockwise seen from outside in x, y, z; written the files' way round at the
    end), one paint, one bone; `dark` marks a hollow (shaded down to the file's floor)."""

    def __init__(self, paint, bone="weapon", dark=False):
        self.P = []
        self.T = []
        self.paint = paint
        self.bone = bone
        self.dark = dark

    def pt(self, p):
        self.P.append(np.asarray(p, float))
        return len(self.P) - 1

    def tri(self, a, b, c):
        self.T.append((a, b, c))

    def quad(self, a, b, c, d):
        self.T.append((a, b, c))
        self.T.append((a, c, d))


def box(lo, hi, paint, bone="weapon"):
    """An axis-true box, each face flat."""
    pc = Piece(paint, bone)
    lo, hi = np.asarray(lo, float), np.asarray(hi, float)
    c = [np.array((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    for f in faces:
        ix = [pc.pt(c[k]) for k in f]
        pc.quad(*ix)
    return orient(pc)


def oriented_box(centre, axes, half, paint, bone="weapon"):
    """A box about `centre` with its three axes (unit, columns of `axes`) and half sizes `half`."""
    pc = box(-np.asarray(half, float), np.asarray(half, float), paint, bone)
    A = np.asarray(axes, float)
    pc.P = [np.asarray(centre, float) + A @ p for p in pc.P]
    return orient(pc)


def ring(n, a, b, e=2.0, phase=0.0):
    """n points round a superellipse of half sizes a (across, y) and b (up, z): exponent 2 an ellipse, higher squarer."""
    out = []
    for k in range(n):
        th = 2 * math.pi * k / n + phase
        c, s = math.cos(th), math.sin(th)
        out.append((a * math.copysign(abs(c) ** (2.0 / e), c), b * math.copysign(abs(s) ** (2.0 / e), s)))
    return out


def loft_x(stations, n, paint, bone="weapon", e=2.0, cap0=True, cap1=True, phase=None, dark_caps=(False, False)):
    """A solid along x through `stations` [(x, y, z, half across, half up)], its sides smooth round, each end capped flat.
    Returns the pieces (the sides and each cap)."""
    if phase is None:
        phase = math.pi / n
    side = Piece(paint, bone)
    rings = []
    for (x, cy, cz, a, b) in stations:
        rings.append([side.pt((x, cy + u, cz + v)) for (u, v) in ring(n, a, b, e, phase)])
    for i in range(len(rings) - 1):
        r0, r1 = rings[i], rings[i + 1]
        for k in range(n):
            k1 = (k + 1) % n
            side.quad(r0[k], r1[k], r1[k1], r0[k1])
    out = [orient(side, axis="x")]
    for end, want, dk in ((0, cap0, dark_caps[0]), (-1, cap1, dark_caps[1])):
        if not want:
            continue
        (x, cy, cz, a, b) = stations[end]
        cp = Piece(paint, bone, dark=dk)
        c = cp.pt((x, cy, cz))
        rr = [cp.pt((x, cy + u, cz + v)) for (u, v) in ring(n, a, b, e, phase)]
        for k in range(n):
            cp.tri(c, rr[k], rr[(k + 1) % n])
        out.append(orient(cp, normal=(-1.0 if end == 0 else 1.0, 0.0, 0.0)))
    return out


def loft_path(path, ups, sizes, n, paint, bone="weapon", e=2.0, cap0=True, cap1=True):
    """A solid along a polyline `path`, its sections square to the path, each section's `up` given (sizes: half across,
    half up)."""
    path = [np.asarray(p, float) for p in path]
    side = Piece(paint, bone)
    rings = []
    frames = []
    for i, p in enumerate(path):
        if i == 0:
            t = path[1] - path[0]
        elif i == len(path) - 1:
            t = path[-1] - path[-2]
        else:
            t = path[i + 1] - path[i - 1]
        t = t / np.linalg.norm(t)
        up = np.asarray(ups[i], float)
        up = up - t * (up @ t)
        up /= np.linalg.norm(up)
        across = np.cross(up, t)
        frames.append((t, across, up))
        a, b = sizes[i]
        rings.append([side.pt(p + across * u + up * v) for (u, v) in ring(n, a, b, e, math.pi / n)])
    for i in range(len(rings) - 1):
        r0, r1 = rings[i], rings[i + 1]
        for k in range(n):
            k1 = (k + 1) % n
            side.quad(r0[k], r1[k], r1[k1], r0[k1])
    out = [orient(side, path=path)]
    for end, want in ((0, cap0), (-1, cap1)):
        if not want:
            continue
        t, across, up = frames[end]
        a, b = sizes[end]
        cp = Piece(paint, bone)
        c = cp.pt(path[end])
        rr = [cp.pt(path[end] + across * u + up * v) for (u, v) in ring(n, a, b, e, math.pi / n)]
        for k in range(n):
            cp.tri(c, rr[k], rr[(k + 1) % n])
        out.append(orient(cp, normal=(-t if end == 0 else t)))
    return out


def prism_y(profile, y0, y1, paint, bone="weapon"):
    """A solid with a side view `profile` [(x, z)...] (convex, in order) between y0 and y1: flat sides, smooth round
    nothing (a stamped part)."""
    pcs = []
    n = len(profile)
    for y, sgn in ((y0, -1.0), (y1, 1.0)):
        cp = Piece(paint, bone)
        cx = sum(p[0] for p in profile) / n
        cz = sum(p[1] for p in profile) / n
        c = cp.pt((cx, y, cz))
        rr = [cp.pt((x, y, z)) for (x, z) in profile]
        for k in range(n):
            cp.tri(c, rr[k], rr[(k + 1) % n])
        pcs.append(orient(cp, normal=(0.0, sgn, 0.0)))
    for k in range(n):
        (xa, za), (xb, zb) = profile[k], profile[(k + 1) % n]
        sp = Piece(paint, bone)
        q = [sp.pt((xa, y0, za)), sp.pt((xb, y0, zb)), sp.pt((xb, y1, zb)), sp.pt((xa, y1, za))]
        sp.quad(*q)
        mid = np.array(((xa + xb) / 2, 0.0, (za + zb) / 2))
        out = mid - np.array((sum(p[0] for p in profile) / n, 0.0, sum(p[1] for p in profile) / n))
        pcs.append(orient(sp, normal=out))
    return pcs


def hole(centre, normal, e1, r1, r2, n=8, paint="steel", bone="weapon", lift=0.0006):
    """A dark oval lying on a face (a cooling hole seen from outside): a fan of n, lifted a little off the face."""
    normal = np.asarray(normal, float) / np.linalg.norm(normal)
    e1 = np.asarray(e1, float)
    e1 = e1 - normal * (e1 @ normal)
    e1 /= np.linalg.norm(e1)
    e2 = np.cross(normal, e1)
    c0 = np.asarray(centre, float) + normal * lift
    pc = Piece(paint, bone, dark=True)
    c = pc.pt(c0)
    rr = [pc.pt(c0 + e1 * r1 * math.cos(2 * math.pi * k / n) + e2 * r2 * math.sin(2 * math.pi * k / n)) for k in range(n)]
    for k in range(n):
        pc.tri(c, rr[k], rr[(k + 1) % n])
    return orient(pc, normal=normal)


def slot(centre, normal, e1, half_len, r, n_end=4, paint="steel", bone="weapon", lift=0.0006):
    """A dark round-ended slot lying on a face (a cooling slot seen from outside): straight sides, a half round at each
    end, `half_len` from its centre to either end, `r` its half width; a fan, lifted a little off the face."""
    normal = np.asarray(normal, float) / np.linalg.norm(normal)
    e1 = np.asarray(e1, float)
    e1 = e1 - normal * (e1 @ normal)
    e1 /= np.linalg.norm(e1)
    e2 = np.cross(normal, e1)
    c0 = np.asarray(centre, float) + normal * lift
    pc = Piece(paint, bone, dark=True)
    c = pc.pt(c0)
    rim = []
    for end, a0 in ((1.0, -90.0), (-1.0, 90.0)):
        cc = c0 + e1 * end * (half_len - r)
        for k in range(n_end + 1):
            a = math.radians(a0 + 180.0 * k / n_end)
            rim.append(pc.pt(cc + e1 * r * math.cos(a) + e2 * r * math.sin(a)))
    for k in range(len(rim)):
        pc.tri(c, rim[k], rim[(k + 1) % len(rim)])
    return orient(pc, normal=normal)


def _ccw(O):
    O = np.asarray(O, float)
    area = 0.5 * sum(O[k, 0] * O[(k + 1) % len(O), 1] - O[(k + 1) % len(O), 0] * O[k, 1] for k in range(len(O)))
    return O if area > 0 else O[::-1].copy()


def _fan(O):
    """A fan over a side view's outline [(x, z)]: from its middle when every triangle from there turns the same way
    (returns None, the middle to be added), else from the first corner that sees every other one (returns its index)."""
    n = len(O)
    m = O.mean(0)

    def ok(c, pairs):
        s = [(O[i, 0] - c[0]) * (O[j, 1] - c[1]) - (O[i, 1] - c[1]) * (O[j, 0] - c[0]) for i, j in pairs]
        return all(v > 1e-9 for v in s)
    for v in range(n):
        if ok(O[v], [((v + k) % n, (v + k + 1) % n) for k in range(1, n - 1)]):
            return v
    assert ok(m, [(k, (k + 1) % n) for k in range(n)]), "the outline is no fan"
    return None


def slab_y(profile, hy, paint, bone="weapon", ch=0.0, middle=False):
    """A flat-sided solid (a moulded butt): its side view `profile` [(x, z)...] in order round it, between y -hy and
    +hy, the edges round it eased by `ch` (0: square). Each flat side its own points; the band round it shares its
    points, so its edges shade round."""
    O = _ccw(profile)
    n = len(O)
    E = np.roll(O, -1, 0) - O
    En = np.stack([E[:, 1], -E[:, 0]], 1)                 # (each edge's outward normal: the outline runs counter-clockwise)
    En /= np.linalg.norm(En, axis=1)[:, None]
    if ch > 0:
        V = En + np.roll(En, 1, 0)
        V /= np.linalg.norm(V, axis=1)[:, None]
        cosh = np.maximum(np.einsum("ij,ij->i", V, En), 0.5)
        Ii = O - V * (ch / cosh)[:, None]
    else:
        Ii = O
    pcs = []
    for y, sg in ((-hy, -1.0), (hy, 1.0)):
        fp = Piece(paint, bone)
        rr = [fp.pt((p[0], y, p[1])) for p in Ii]
        v = None if middle else _fan(Ii)
        if v is None:
            c = fp.pt((Ii[:, 0].mean(), y, Ii[:, 1].mean()))
            for k in range(n):
                fp.tri(c, rr[k], rr[(k + 1) % n])
        else:
            for k in range(1, n - 1):
                fp.tri(rr[v], rr[(v + k) % n], rr[(v + k + 1) % n])
        pcs.append(orient(fp, normal=(0.0, sg, 0.0)))
    band = Piece(paint, bone)
    if ch > 0:
        rings = [(Ii, -hy), (O, -hy + ch), (O, hy - ch), (Ii, hy)]
    else:
        rings = [(O, -hy), (O, hy)]
    R = [[band.pt((p[0], y, p[1])) for p in ring] for ring, y in rings]
    P = np.array(band.P)
    for r in range(len(R) - 1):
        ysg = -1.0 if (ch > 0 and r == 0) else (1.0 if (ch > 0 and r == len(R) - 2) else 0.0)
        for k in range(n):
            k1 = (k + 1) % n
            ref = np.array((En[k, 0], ysg, En[k, 1]))
            for tri in ((R[r][k], R[r][k1], R[r + 1][k1]), (R[r][k], R[r + 1][k1], R[r + 1][k])):
                a, b_, c = tri
                nn = np.cross(P[b_] - P[a], P[c] - P[a])
                band.tri(*(tri if nn @ ref >= 0 else (a, c, b_)))
    pcs.append(band)
    return pcs


def orient(pc, normal=None, axis=None, path=None):
    """Wind every triangle of the piece outward: away from its centre (or along `normal` for a flat one, away from the
    x axis through the piece's centre for an x loft, away from the path for a path loft)."""
    P = np.array(pc.P)
    cen = P.mean(0)
    out = []
    for (a, b, c) in pc.T:
        n = np.cross(P[b] - P[a], P[c] - P[a])
        m = (P[a] + P[b] + P[c]) / 3
        if normal is not None:
            ref = np.asarray(normal, float)
        elif axis == "x":
            ref = m - np.array((m[0], cen[1], cen[2]))
            ref = ref if np.linalg.norm(ref) > 1e-9 else m - cen
        elif path is not None:
            pa = np.array(path)
            seg = pa[np.argmin(np.linalg.norm(pa - m, axis=1))]
            ref = m - seg
        else:
            ref = m - cen
        out.append((a, b, c) if n @ ref >= 0 else (a, c, b))
    pc.T = out
    return pc


class Gun:
    """A gun's three levels (lists of pieces) and its hand places, muzzle and name."""

    def __init__(self, name, about):
        self.name = name
        self.about = about
        self.levels = {"near": [], "mid": [], "far": []}
        self.grips = {}
        self.muzzle = None
        self.paints = {}
        self.hinge = None

    def add(self, level, pieces):
        if isinstance(pieces, Piece):
            pieces = [pieces]
        self.levels[level].extend(pieces)

    def mesh(self, level):
        """Points, facings, paint names, bones, darks and triangles of one level (triangles wound outward, x y z)."""
        P, N, paint, bone, dark, T = [], [], [], [], [], []
        for pc in self.levels[level]:
            base = len(P)
            Pp = np.array(pc.P)
            Nn = np.zeros_like(Pp)
            for (a, b, c) in pc.T:
                n = np.cross(Pp[b] - Pp[a], Pp[c] - Pp[a])
                for k in (a, b, c):
                    Nn[k] += n
            ln = np.linalg.norm(Nn, axis=1)
            ln[ln < 1e-12] = 1.0
            Nn /= ln[:, None]
            P.extend(Pp)
            N.extend(Nn)
            paint.extend([pc.paint] * len(Pp))
            bone.extend([pc.bone] * len(Pp))
            dark.extend([pc.dark] * len(Pp))
            T.extend([(a + base, b + base, c + base) for (a, b, c) in pc.T])
        return (np.array(P), np.array(N), paint, bone, np.array(dark, bool), np.array(T, int))


# ---------------------------------------------------------------------------------------------------- the MG 42 ----
def mg42():
    """The German general-purpose machine gun of 1942 on, as carried by a section's gunner on its bipod: 1.22 m, a
    stamped box receiver with the feed cover on top, the long squared barrel jacket (one depth to the muzzle booster,
    four fifths of the receiver's) with its row of round-ended cooling slots on the left and the long barrel-change
    opening on the right, the conical booster with the front sight, the fishtail butt (narrow at its neck, flaring to a
    toe hooked down) and the pistol grip in dark red-brown plastic, the bipod hinged under the jacket's front, its legs
    folded back under it, a 50-round belt drum hung on the left of the feed."""
    g = Gun("mg42", "MG 42")
    b = BORE
    JA, JU, JZ = JACKET, JACKET_UP, JACKET_Z
    under = JZ - JU                                          # (the jacket's underside over the bore)
    g.hinge = hinge_of(b, under)
    le = leg_end(g.hinge)
    g.paints = {"bakelite": BAKELITE, "brass": BRASS, "bullet": BULLET}
    g.muzzle = np.array((0.872, 0.0, b))
    J0, J1 = 0.300, 0.735
    # --- the butt: a fishtail, its comb straight on from the receiver, its underside sweeping down to the toe (under a
    # fifth of the gun: 0.238 of 1.220)
    BUTT = [(-0.110, b + 0.030), (-0.200, b + 0.031), (-0.290, b + 0.033), (-0.338, b + 0.036), (-0.348, b + 0.028),
            (-0.349, b - 0.030), (-0.350, b - 0.080), (-0.346, b - 0.100), (-0.334, b - 0.106), (-0.322, b - 0.098),
            (-0.312, b - 0.084), (-0.285, b - 0.072), (-0.240, b - 0.058), (-0.175, b - 0.044), (-0.110, b - 0.028)]
    g.add("near", slab_y(BUTT, 0.020, "bakelite", ch=0.005, middle=True))
    # --- the receiver: a long box, its corners eased, its back sloping down to the butt's neck; the trigger housing
    # under its rear
    g.add("near", loft_x([(-0.110, 0.0, b + 0.001, 0.0245, 0.029), (-0.050, 0.0, b + 0.012, 0.0245, 0.040),
                          (J0, 0.0, b + 0.012, 0.0245, 0.040)], 10, "steel", e=5.0))
    g.add("near", box((-0.030, -0.0185, b - 0.045), (0.050, 0.0185, b - 0.026), "steel"))   # (trigger housing)
    # (the feed cover over its top, hinged at the rear, a little wider, its top ribbed by a dark seam)
    g.add("near", box((0.010, -0.0262, b + 0.046), (0.205, 0.0262, b + 0.062), "steel"))
    g.add("near", slot((0.11, 0.0, b + 0.062), (0, 0, 1), (1, 0, 0), 0.085, 0.0035, n_end=2))
    # (the rear sight: a leaf on the receiver's top ahead of the cover, and its notch)
    g.add("near", box((0.212, -0.010, b + 0.052), (0.236, 0.010, b + 0.060), "steel"))
    g.add("near", box((0.220, -0.007, b + 0.060), (0.228, 0.007, b + 0.075), "steel"))
    # (the feed tray's mouth on the left, where the belt goes in, and the links' way out on the right)
    g.add("near", box((0.060, -0.034, b + 0.020), (0.160, -0.0245, b + 0.046), "steel"))
    g.add("near", hole((0.110, 0.0249, b + 0.033), (0, 1, 0), (1, 0, 0), 0.032, 0.008, n=6))
    # (the cocking handle on the right, toward the front)
    g.add("near", box((0.238, 0.0245, b + 0.002), (0.266, 0.048, b + 0.018), "steel"))
    # --- the pistol grip: raked back under the receiver; the trigger guard; the trigger
    gt = np.array((-0.018, 0.0, b - 0.040))
    ga = np.array((-0.40, 0.0, -0.917))
    ga /= np.linalg.norm(ga)
    gp = [gt, gt + ga * 0.055, gt + ga * 0.108]
    g.add("near", loft_path(gp, [(1, 0, 0.44)] * 3, [(0.0140, 0.0180), (0.0152, 0.0195), (0.0140, 0.0185)], 8,
                            "bakelite", e=2.6, cap0=False))
    tg = [np.array((0.046, 0.0, b - 0.044)), np.array((0.026, 0.0, b - 0.084)), np.array((-0.006, 0.0, b - 0.080)),
          np.array((-0.024, 0.0, b - 0.066))]
    g.add("near", loft_path(tg, [(0, 1, 0)] * 4, [(0.003, 0.0055)] * 4, 4, "steel", cap0=False, cap1=False))
    g.add("near", loft_path([np.array((0.014, 0.0, b - 0.045)), np.array((0.012, 0.0, b - 0.062)),
                             np.array((0.004, 0.0, b - 0.070))], [(0, 1, 0)] * 3, [(0.0025, 0.004)] * 3, 4, "steel",
                            cap0=False, cap1=False))
    g.grips["pistol"] = {"at": gt + ga * 0.058, "axis": ga}
    # --- the barrel jacket: squared, its corners rounded, one depth to the front; five round-ended slots in a row on
    # the left, the long barrel-change opening on the right
    g.add("near", loft_x([(J0, 0.0, b + JZ, JA, JU), (J1, 0.0, b + JZ, JA, JU)], 12, "steel", e=4.5, cap0=False,
                         cap1=False))
    for k in range(5):
        g.add("near", slot((0.340 + 0.054 * k, -JA, b + JZ), (0, -1, 0), (1, 0, 0), 0.021, 0.0085, n_end=3))
    g.add("near", slot((0.462, JA, b + JZ), (0, 1, 0), (1, 0, 0), 0.105, 0.0115, n_end=3))
    # (the bipod's clamp round the jacket, and the front sight on its folding base)
    g.add("near", loft_x([(0.586, 0.0, b + JZ, JA + 0.0025, JU + 0.0025), (0.614, 0.0, b + JZ, JA + 0.0025, JU + 0.0025)],
                         12, "steel", e=4.5, cap0=False, cap1=False))
    top = b + JZ + JU
    g.add("near", box((0.700, -0.009, top - 0.001), (0.728, 0.009, top + 0.010), "steel"))
    g.add("near", box((0.712, -0.0028, top + 0.010), (0.720, 0.0028, b + 0.077), "steel"))
    # --- the muzzle booster: a cone narrowing from the jacket's front plate, then flaring at the muzzle; the bore
    boo = [(J1, 0.0, b, 0.0200, 0.0200), (0.775, 0.0, b, 0.0152, 0.0152), (0.842, 0.0, b, 0.0150, 0.0150),
           (0.872, 0.0, b, 0.0195, 0.0195)]
    g.add("near", loft_x(boo, 10, "steel", cap0=False, cap1=True, dark_caps=(False, True)))
    front = Piece("steel")                                # (the jacket's front plate, squared, round the booster)
    sq = [front.pt((J1, u, b + JZ + v)) for (u, v) in ring(12, JA, JU, 4.5, math.pi / 12)]
    rd = [front.pt((J1, u, b + v)) for (u, v) in ring(12, 0.0200, 0.0200, 2.0, math.pi / 12)]
    for k in range(12):
        front.quad(sq[k], sq[(k + 1) % 12], rd[(k + 1) % 12], rd[k])
    g.add("near", orient(front, normal=(1, 0, 0)))
    # --- the bipod; the belt drum on the left of the feed, its rounds coming up out of its mouth into the feed
    folded_bipod(g, "near", b, under)
    belt_drum(g, "near", 0.064, 0.158, -0.0785, b + 0.004, 0.050)
    feed_rounds(g, "near", 0.068, [(-0.037, b + 0.034), (-0.049, b + 0.043), (-0.061, b + 0.050), (-0.073, b + 0.054)])
    # --- where hands go: along the folded legs; the gun's underside (the butt's, the receiver's, the trigger
    # housing's, the receiver's again, the legs, the jacket's front); the butt's underside
    g.grips["legs"] = [(np.array((le, 0.0, b + under - 0.012)), np.array((0.580, 0.0, b + under - 0.008)))]
    line = [(-0.300, b - 0.074), (-0.250, b - 0.060), (-0.180, b - 0.045), (-0.110, b - 0.030), (-0.040, b - 0.030),
            (0.050, b - 0.045), (J0 - 0.002, b - 0.030), (le, b + under - 0.012), (0.580, b + under - 0.008),
            (J1, b + under)]
    g.grips["under"] = [(np.array((x0, 0.0, z0)), np.array((x1, 0.0, z1))) for (x0, z0), (x1, z1) in zip(line, line[1:])]
    g.grips["butt"] = [(np.array((-0.300, 0.0, b - 0.074)), np.array((-0.110, 0.0, b - 0.030)))]

    # ------------------------------------------------------------------------------------------- mid (LOD1) ----
    BUTT_MID = [(-0.110, b + 0.030), (-0.338, b + 0.036), (-0.349, b + 0.022), (-0.350, b - 0.080),
                (-0.334, b - 0.106), (-0.312, b - 0.084), (-0.200, b - 0.050), (-0.110, b - 0.028)]
    g.add("mid", slab_y(BUTT_MID, 0.020, "bakelite"))
    g.add("mid", loft_x([(-0.110, 0.0, b + 0.001, 0.0245, 0.029), (-0.050, 0.0, b + 0.012, 0.0245, 0.040),
                         (J0, 0.0, b + 0.012, 0.0245, 0.040)], 6, "steel", e=5.0, cap0=False, cap1=False))
    # (the grip's top runs up into the receiver: no trigger housing at this distance to close the gap)
    g.add("mid", loft_path([gt - ga * 0.016, gt + ga * 0.108], [(1, 0, 0.44)] * 2, [(0.0145, 0.019)] * 2, 4, "bakelite",
                           cap0=False))
    # (the jacket a box to the front, its front plate; its slots one dark run each side)
    s2 = 1.0 / math.sqrt(2.0)
    g.add("mid", loft_x([(J0, 0.0, b + JZ, JA / s2, JU / s2), (J1, 0.0, b + JZ, JA / s2, JU / s2)], 4, "steel",
                        phase=math.pi / 4, cap0=False))
    g.add("mid", hole((0.448, -JA, b + JZ), (0, -1, 0), (1, 0, 0), 0.130, 0.0085, n=6))
    g.add("mid", hole((0.462, JA, b + JZ), (0, 1, 0), (1, 0, 0), 0.105, 0.0110, n=6))
    g.add("mid", loft_x([(J1, 0.0, b, 0.020, 0.020), (0.872, 0.0, b, 0.017, 0.017)], 6, "steel", cap0=False, cap1=True,
                        dark_caps=(False, True)))
    folded_bipod(g, "mid", b, under)
    belt_drum(g, "mid", 0.064, 0.158, -0.0785, b + 0.004, 0.050, n=8, shared=True)

    # ------------------------------------------------------------------------------------------- far (LOD2) ----
    # (boxes: the butt flaring from its neck to the heel, the receiver, the jacket, the booster three-sided)
    g.add("far", loft_x([(-0.110, 0.0, b + 0.001, 0.020 / s2, 0.029 / s2),
                         (-0.348, 0.0, b - 0.031, 0.020 / s2, 0.067 / s2)], 4, "bakelite", cap0=False, cap1=False,
                        phase=math.pi / 4))
    g.add("far", loft_x([(-0.110, 0.0, b + 0.001, 0.0245 / s2, 0.029 / s2), (J0, 0.0, b + 0.012, 0.0245 / s2, 0.040 / s2)],
                        4, "steel", cap0=False, cap1=False, phase=math.pi / 4))
    g.add("far", loft_x([(J0, 0.0, b + JZ, JA / s2, JU / s2), (J1, 0.0, b + JZ, JA / s2, JU / s2)], 4, "steel",
                        cap0=False, cap1=False, phase=math.pi / 4))
    g.add("far", loft_x([(J1, 0.0, b, 0.020, 0.020), (0.872, 0.0, b, 0.017, 0.017)], 3, "steel", cap0=False, cap1=False,
                        phase=math.pi / 2))
    g.add("far", loft_path([gt - ga * 0.016, gt + ga * 0.105], [(1, 0, 0.44)] * 2, [(0.015, 0.019)] * 2, 3, "bakelite",
                           cap0=False, cap1=False))
    folded_bipod(g, "far", b, under)
    # (the drum is not drawn this far off)
    return g


# ------------------------------------------------------------------------------------------ parts the guns share ----
def belt_drum(g, level, x0, x1, cy, cz, R, n=12, shared=False):
    """A 50-round belt drum hung on the left of the feed: a short can lying along the gun (the belt coils in it with
    its rounds lying along the bore, as they go into the feed), its curved side, both ends; near, the belt's mouth dark
    on its top toward the gun and the carrying handle on top. `shared`: the ends
    drawn on the side's own points (fewer points, the edges shaded round)."""
    side = Piece("steel")
    ang = [2 * math.pi * k / n for k in range(n)]
    r0 = [side.pt((x0, cy + R * math.cos(a), cz + R * math.sin(a))) for a in ang]
    r1 = [side.pt((x1, cy + R * math.cos(a), cz + R * math.sin(a))) for a in ang]
    for k in range(n):
        side.quad(r0[k], r0[(k + 1) % n], r1[(k + 1) % n], r1[k])
    if shared:
        for k in range(1, n - 1):
            side.tri(r0[0], r0[k], r0[k + 1])
            side.tri(r1[0], r1[k + 1], r1[k])
        # (the winding: the side's quads outward from the can's axis, the ends along it)
        P = np.array(side.P)
        T = []
        for (a, b_, c) in side.T:
            m = (P[a] + P[b_] + P[c]) / 3
            nn = np.cross(P[b_] - P[a], P[c] - P[a])
            if abs(m[0] - x0) < 1e-9 and abs(P[a][0] - x0) < 1e-9 and abs(P[b_][0] - x0) < 1e-9 and abs(P[c][0] - x0) < 1e-9:
                ref = np.array((-1.0, 0, 0))
            elif all(abs(P[i][0] - x1) < 1e-9 for i in (a, b_, c)):
                ref = np.array((1.0, 0, 0))
            else:
                ref = np.array((0.0, m[1] - cy, m[2] - cz))
            T.append((a, b_, c) if nn @ ref >= 0 else (a, c, b_))
        side.T = T
        g.add(level, side)
        return
    out = [orient(side, axis=None, path=[np.array((x0, cy, cz)), np.array((x1, cy, cz))])]
    for xx, sg in ((x0, -1.0), (x1, 1.0)):
        cp = Piece("steel")
        rr = [cp.pt((xx, cy + R * math.cos(a), cz + R * math.sin(a))) for a in ang]
        for k in range(1, n - 1):
            cp.tri(rr[0], rr[k], rr[k + 1])
        out.append(orient(cp, normal=(sg, 0.0, 0.0)))
    if level == "near":
        th = math.radians(62.0)
        nm = np.array((0.0, math.cos(th), math.sin(th)))
        out.append(hole(np.array(((x0 + x1) / 2, cy, cz)) + nm * R, nm, (1, 0, 0), 0.5 * (x1 - x0) - 0.006, 0.011, n=8))
        out.extend(loft_path([np.array((x0 + 0.016, cy - 0.016, cz + R * 0.95)),
                              np.array(((x0 + x1) / 2, cy - 0.016, cz + R + 0.014)),
                              np.array((x1 - 0.016, cy - 0.016, cz + R * 0.95))], [(0, 0, 1)] * 3,
                             [(0.0025, 0.0025)] * 3, 4, "steel", cap0=False, cap1=False))
    g.add(level, out)


def feed_rounds(g, level, x0, path, n=5):
    """Cartridges in their belt going into the feed from the drum's mouth: each lying along the bore, its brass case
    (57 mm, 12 mm across) and its darker bullet forward, side by side along `path` [(y, z)...] (the first in the
    feed's mouth); a band of the belt's links round their cases."""
    for (y, z) in path:
        g.add(level, loft_x([(x0, y, z, 0.0060, 0.0060), (x0 + 0.050, y, z, 0.0057, 0.0057)], n, "brass", cap0=False,
                            cap1=False))
        g.add(level, loft_x([(x0 + 0.050, y, z, 0.0044, 0.0044), (x0 + 0.080, y, z, 0.0009, 0.0009)], n, "bullet",
                            cap0=False, cap1=False))
    pts = [np.array((x0 + 0.022, y, z)) for (y, z) in path]
    if len(pts) >= 2:
        g.add(level, loft_path(pts, [(1, 0, 0)] * len(pts), [(0.0068, 0.0075)] * len(pts), 4, "steel", cap0=False,
                               cap1=False))


def folded_bipod(g, level, b, under, sides=4):
    """The bipod: a hinge block under the barrel at the gun's hinge (near only) and two legs folded back to the legs'
    end, a foot on each (near): the left leg on the `bipod` bone, the right on the `bolt` (so each can splay out on its
    own when the legs come down), the block on the `bipod`. `under` is the barrel's underside's height over the bore
    (negative)."""
    hx = float(g.hinge[0])
    le = leg_end(g.hinge)
    if level == "near":
        g.add(level, box((hx - 0.012, -0.016, b + under - 0.010), (hx + 0.012, 0.016, b + under + 0.002), "steel",
                         LEFT_LEG))
    for s in (-1.0, 1.0):
        bone = LEFT_LEG if s < 0 else RIGHT_LEG
        top = np.array((hx - 0.004, 0.011 * s, b + under - 0.008))
        end = np.array((le, 0.014 * s, b + under - 0.015))
        if level == "near":
            mid = 0.5 * (top + end)
            g.add(level, loft_path([top, mid, end], [(0, 0, 1)] * 3,
                                   [(0.0045, 0.0055), (0.004, 0.005), (0.0045, 0.005)], sides, "steel", bone,
                                   cap0=False, cap1=False))
            g.add(level, oriented_box(end + np.array((-0.008, 0.0, -0.002)), np.eye(3), (0.010, 0.007, 0.004), "steel",
                                      bone))
        elif level == "mid":
            g.add(level, loft_path([top, end], [(0, 0, 1)] * 2, [(0.005, 0.006)] * 2, 3, "steel", bone,
                                   cap0=False, cap1=False))
        else:
            # (a flat blade each, seen from above and from below: each face its own points, so each has its own facing)
            for up in (1.0, -1.0):
                lg = Piece("steel", bone)
                p0 = lg.pt(top)
                p1 = lg.pt(end + np.array((0.0, -0.006, 0.0)))
                p2 = lg.pt(end + np.array((0.0, 0.006, 0.0)))
                lg.tri(p0, p1, p2)
                g.add(level, orient(lg, normal=(0.0, 0.0, up)))


def foot_of(g, s):
    """A leg's foot (its pad's middle) from the hinge, at rest (folded), the weapon's frame: s -1 the left, +1 the
    right."""
    le = leg_end(g.hinge)
    return np.array((le - 0.008 - float(g.hinge[0]), 0.014 * s, -0.009 - 0.002))


# ---------------------------------------------------------------------------------------------------- the MG 34 ----
def mg34():
    """The German machine gun of 1934 on (still carried beside the MG 42 to the end), on its bipod: 1.22 m, a rounded
    receiver with the hinged feed cover on top, the round barrel jacket pierced all round with rows of oval holes, the
    front sight on its post at the jacket's front, the muzzle cone, the sloped butt and the pistol grip in dark brown
    plastic, the rocking trigger, the rear sight on the jacket's back end, the bipod, a 50-round belt drum on the left."""
    g = Gun("mg34", "MG 34")
    b = BORE
    g.paints = {"bakelite": BAKELITE, "brass": BRASS, "bullet": BULLET}
    g.muzzle = np.array((0.852, 0.0, b))
    JA = 0.0205                                            # (the round jacket's radius)
    g.hinge = hinge_of(b, -JA)
    le = leg_end(g.hinge)
    # --- the butt: sloped, its comb falling from the receiver toward the heel, a hand's hollow under it
    butt = [(-0.360, 0.0, b - 0.020, 0.0210, 0.058), (-0.300, 0.0, b - 0.017, 0.0218, 0.055),
            (-0.200, 0.0, b - 0.010, 0.0195, 0.040), (-0.120, 0.0, b - 0.004, 0.0170, 0.030),
            (-0.040, 0.0, b + 0.004, 0.0210, 0.032)]
    g.add("near", loft_x(butt, 8, "bakelite", e=3.0, cap0=False, cap1=False))
    g.add("near", loft_x([(-0.366, 0.0, b - 0.020, 0.0215, 0.059), (-0.359, 0.0, b - 0.020, 0.0218, 0.060)], 8,
                         "steel", e=3.0, cap1=False))
    # --- the receiver: rounded, the feed cover over its fore part, the trigger housing under its rear
    g.add("near", loft_x([(-0.040, 0.0, b + 0.008, 0.0240, 0.034), (0.270, 0.0, b + 0.008, 0.0240, 0.034)], 12,
                         "steel", e=2.8))
    g.add("near", loft_x([(0.000, 0.0, b + 0.036, 0.0255, 0.012), (0.175, 0.0, b + 0.036, 0.0255, 0.012)], 8,
                         "steel", e=3.5))
    g.add("near", hole((0.09, 0.0, b + 0.0485), (0, 0, 1), (1, 0, 0), 0.075, 0.003, n=6))
    g.add("near", box((-0.034, -0.0175, b - 0.046), (0.046, 0.0175, b - 0.022), "steel"))   # (trigger housing)
    g.add("near", box((0.150, 0.0235, b - 0.004), (0.178, 0.044, b + 0.010), "steel"))      # (cocking handle)
    # (the feed tray's mouth on the left, the links' way out on the right)
    g.add("near", box((0.050, -0.033, b + 0.014), (0.150, -0.0235, b + 0.040), "steel"))
    g.add("near", hole((0.100, 0.0242, b + 0.024), (0, 1, 0), (1, 0, 0), 0.030, 0.007, n=6))
    # --- the pistol grip, the trigger guard, the rocking trigger
    gt = np.array((-0.020, 0.0, b - 0.040))
    ga = np.array((-0.36, 0.0, -0.933))
    ga /= np.linalg.norm(ga)
    g.add("near", loft_path([gt, gt + ga * 0.055, gt + ga * 0.105], [(1, 0, 0.40)] * 3,
                            [(0.0138, 0.0175), (0.0150, 0.0192), (0.0140, 0.0180)], 8, "bakelite", e=2.6, cap0=False))
    tg = [np.array((0.044, 0.0, b - 0.044)), np.array((0.024, 0.0, b - 0.084)), np.array((-0.008, 0.0, b - 0.080)),
          np.array((-0.026, 0.0, b - 0.064))]
    g.add("near", loft_path(tg, [(0, 1, 0)] * 4, [(0.003, 0.0055)] * 4, 4, "steel", cap0=False, cap1=False))
    g.add("near", loft_path([np.array((0.012, 0.0, b - 0.045)), np.array((0.010, 0.0, b - 0.060)),
                             np.array((0.003, 0.0, b - 0.070))], [(0, 1, 0)] * 3, [(0.0026, 0.0045)] * 3, 4, "steel",
                            cap0=False, cap1=False))
    g.grips["pistol"] = {"at": gt + ga * 0.056, "axis": ga}
    # --- the round jacket pierced all round; the rear sight on its back end; the front sight post; the muzzle cone
    J0, J1 = 0.270, 0.715
    g.add("near", loft_x([(J0, 0.0, b, JA + 0.0035, JA + 0.0035), (J0 + 0.030, 0.0, b, JA + 0.0035, JA + 0.0035),
                          (J0 + 0.032, 0.0, b, JA, JA), (J1, 0.0, b, JA, JA)], 12, "steel", cap0=False))
    for k in range(5):
        x = 0.345 + 0.070 * k
        for ang in (90.0, 30.0, 150.0):                    # (top, and either upper side)
            th = math.radians(ang)
            n_ = np.array((0.0, math.cos(th), math.sin(th)))
            g.add("near", hole((x, JA * n_[1], b + JA * n_[2]), n_, (1, 0, 0), 0.016, 0.0065, n=5))
    g.add("near", box((J0 + 0.004, -0.0095, b + JA + 0.003), (J0 + 0.028, 0.0095, b + JA + 0.011), "steel"))
    g.add("near", box((J0 + 0.012, -0.0065, b + JA + 0.011), (J0 + 0.020, 0.0065, b + JA + 0.045), "steel"))
    g.add("near", loft_x([(0.586, 0.0, b, JA + 0.0025, JA + 0.0025), (0.614, 0.0, b, JA + 0.0025, JA + 0.0025)], 10,
                         "steel", cap0=False, cap1=False))
    g.add("near", box((0.684, -0.008, b + JA - 0.001), (0.706, 0.008, b + JA + 0.008), "steel"))
    g.add("near", box((0.692, -0.0026, b + JA + 0.008), (0.699, 0.0026, b + JA + 0.044), "steel"))
    g.add("near", loft_x([(J1, 0.0, b, JA, JA), (0.745, 0.0, b, 0.0165, 0.0165), (0.818, 0.0, b, 0.0150, 0.0150),
                          (0.852, 0.0, b, 0.0185, 0.0185)], 10, "steel", cap0=True, cap1=True, dark_caps=(False, True)))
    folded_bipod(g, "near", b, -JA)
    belt_drum(g, "near", 0.053, 0.147, -0.0775, b - 0.002, 0.050, n=10)
    feed_rounds(g, "near", 0.058, [(-0.036, b + 0.028), (-0.048, b + 0.037), (-0.060, b + 0.044)], n=4)
    g.grips["legs"] = [(np.array((le, 0.0, b - JA - 0.012)), np.array((0.580, 0.0, b - JA - 0.008)))]
    line = [(-0.330, b - 0.076), (-0.170, b - 0.050), (-0.040, b - 0.028), (0.046, b - 0.046), (J0 - 0.002, b - 0.026),
            (le, b - JA - 0.012), (0.580, b - JA - 0.008), (J1, b - JA)]
    g.grips["under"] = [(np.array((x0, 0.0, z0)), np.array((x1, 0.0, z1))) for (x0, z0), (x1, z1) in zip(line, line[1:])]
    g.grips["butt"] = [(np.array((-0.330, 0.0, b - 0.076)), np.array((-0.100, 0.0, b - 0.034)))]
    # ------------------------------------------------------------------------------------------- mid (LOD1) ----
    g.add("mid", loft_x([(-0.362, 0.0, b - 0.020, 0.021, 0.058), (-0.200, 0.0, b - 0.010, 0.0195, 0.040),
                         (-0.040, 0.0, b + 0.004, 0.021, 0.032)], 6, "bakelite", e=3.0, cap1=False))
    g.add("mid", loft_x([(-0.040, 0.0, b + 0.012, 0.0245, 0.040), (0.270, 0.0, b + 0.012, 0.0245, 0.040)], 6, "steel",
                        e=3.0, cap0=False))
    g.add("mid", loft_path([gt - ga * 0.016, gt + ga * 0.105], [(1, 0, 0.40)] * 2, [(0.0145, 0.0185)] * 2, 4,
                           "bakelite", cap0=False))
    g.add("mid", loft_x([(J0, 0.0, b, JA + 0.003, JA + 0.003), (J1, 0.0, b, JA, JA)], 6, "steel", cap0=False,
                        cap1=False))
    g.add("mid", hole((0.49, 0.0, b + JA), (0, 0, 1), (1, 0, 0), 0.17, 0.007, n=6))
    g.add("mid", loft_x([(J1, 0.0, b, 0.019, 0.019), (0.852, 0.0, b, 0.017, 0.017)], 6, "steel", cap0=True, cap1=True,
                        dark_caps=(False, True)))
    folded_bipod(g, "mid", b, -JA)
    belt_drum(g, "mid", 0.053, 0.147, -0.0775, b - 0.002, 0.050, n=8, shared=True)
    # ------------------------------------------------------------------------------------------- far (LOD2) ----
    g.add("far", loft_x([(-0.362, 0.0, b - 0.020, 0.021, 0.056), (-0.040, 0.0, b + 0.004, 0.021, 0.032)], 4,
                        "bakelite", e=2.0, cap0=False, cap1=False, phase=math.pi / 4))
    g.add("far", loft_x([(-0.040, 0.0, b + 0.008, 0.026, 0.036), (0.270, 0.0, b + 0.008, 0.026, 0.036),
                         (0.852, 0.0, b, 0.019, 0.019)], 4, "steel", e=2.0, cap0=False, cap1=False, phase=math.pi / 4))
    g.add("far", loft_path([gt - ga * 0.016, gt + ga * 0.102], [(1, 0, 0.40)] * 2, [(0.015, 0.0185)] * 2, 3,
                           "bakelite", cap0=False, cap1=False))
    folded_bipod(g, "far", b, -JA)
    return g


GUNS = {"mg42": mg42, "mg34": mg34}


def report():
    for k, f in GUNS.items():
        g = f()
        for L in ("near", "mid", "far"):
            P, N, paint, bone, dark, T = g.mesh(L)
            print(f"{k} {L}: {len(P)} points, {len(T)} triangles, {int(dark.sum())} dark points, "
                  f"x {P[:, 0].min():.3f} to {P[:, 0].max():.3f}, z {P[:, 2].min():.3f} to {P[:, 2].max():.3f}")


if __name__ == "__main__":
    if "--report" in sys.argv:
        report()
