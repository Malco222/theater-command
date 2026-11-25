"""The light machine guns the gunners carry (release 2, Brian, 25 November 14:15Z: "There should be a machine gun teams
that lay down suppressing fire, such as the German MG 42 and the US machine gun"). Our own drawings, to period
proportions, built from simple solids at the three levels of a soldier file, in the weapon bone's frame of the shared
skeleton: metres, x along the bore toward the muzzle, y to the gunner's right, z up, the origin where the rifleman's
right fist closes on his rifle's wrist (the skeleton's `weapon` bone, socket_weapon). The bipod's legs ride the
`bipod` bone, its head at the gun's own hinge under the barrel jacket (`HINGE`: the gunner's file moves the bone's head
there from the rifleman's 0.600 forward, 0.018 up), so they fold back under the jacket at rest and swing down about
the hinge in `lmg_burst`.

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
JACKET = 0.0235              # (the barrel jacket's half width)
HINGE = np.array((0.600, 0.0, BORE - JACKET - 0.006))   # (the bipod's hinge under the jacket: its bone's head)
LEG_END = 0.358              # (the folded legs' feet, along the jacket: legs 0.24 m, so in lmg_burst's swing (107 degrees
                             # down about the hinge) the feet just meet the ground under the gun as the rifleman lies)


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
    stamped box receiver with the feed cover on top, the squared barrel jacket with its rows of oval cooling holes on top
    and left and the long barrel-change opening on the right, the conical muzzle booster with the front sight, the butt
    and pistol grip in dark brown plastic, the bipod hinged at the jacket's front, a 50-round belt drum hung on the left
    of the feed."""
    g = Gun("mg42", "MG 42")
    g.hinge = HINGE
    b = BORE
    g.paints = {"bakelite": "3A2B22", "brass": "9C7A3C"}
    g.muzzle = np.array((0.872, 0.0, b))
    # --- the butt: lofted, rounded, its top straight with the bore, its underside falling to the heel
    butt = [(-0.360, 0.0, b - 0.012, 0.0215, 0.062), (-0.300, 0.0, b - 0.010, 0.0222, 0.058),
            (-0.190, 0.0, b - 0.001, 0.0205, 0.045), (-0.090, 0.0, b + 0.007, 0.0190, 0.031),
            (-0.030, 0.0, b + 0.010, 0.0215, 0.032)]
    # (the butt plate: a dark steel cap on the heel, as the MG 42's)
    g.add("near", loft_x(butt, 8, "bakelite", e=3.0, cap0=False, cap1=False))
    g.add("near", loft_x([(-0.366, 0.0, b - 0.012, 0.0220, 0.063), (-0.359, 0.0, b - 0.012, 0.0222, 0.064)], 8,
                         "steel", e=3.0, cap1=False))
    # --- the receiver: a long box, its corners eased, the trigger housing under its rear
    rec = [(-0.034, 0.0, b + 0.012, 0.0245, 0.040), (0.300, 0.0, b + 0.012, 0.0245, 0.040)]
    g.add("near", loft_x(rec, 10, "steel", e=5.0))
    g.add("near", box((-0.030, -0.0185, b - 0.045), (0.050, 0.0185, b - 0.026), "steel"))   # (trigger housing)
    # (the feed cover over its top, hinged at the rear, a little wider, its top ribbed by a dark seam)
    g.add("near", box((0.010, -0.0262, b + 0.046), (0.205, 0.0262, b + 0.062), "steel"))
    g.add("near", hole((0.11, 0.0, b + 0.062), (0, 0, 1), (1, 0, 0), 0.085, 0.0035, n=6))
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
    tg = [np.array((0.046, 0.0, b - 0.044)), np.array((0.045, 0.0, b - 0.072)), np.array((0.026, 0.0, b - 0.084)),
          np.array((-0.006, 0.0, b - 0.080)), np.array((-0.024, 0.0, b - 0.066))]
    g.add("near", loft_path(tg[:1] + tg[2:], [(0, 1, 0)] * 4, [(0.003, 0.0055)] * 4, 4, "steel", cap0=False, cap1=False))
    g.add("near", loft_path([np.array((0.014, 0.0, b - 0.045)), np.array((0.012, 0.0, b - 0.062)),
                             np.array((0.004, 0.0, b - 0.070))], [(0, 1, 0)] * 3, [(0.0025, 0.004)] * 3, 4, "steel",
                            cap0=False, cap1=False))
    g.grips["pistol"] = {"at": gt + ga * 0.058, "axis": ga}
    # --- the barrel jacket: squared, its corners rounded; the holes; the front plate
    J0, J1, JA = 0.300, 0.735, JACKET
    g.add("near", loft_x([(J0, 0.0, b, JA, JA), (J1, 0.0, b, JA, JA)], 12, "steel", e=4.5, cap0=False))
    for k in range(5):                                   # (top: five long ovals)
        x = 0.355 + 0.075 * k
        g.add("near", hole((x, 0.0, b + JA), (0, 0, 1), (1, 0, 0), 0.024, 0.0085, n=6))
    for k in range(5):                                   # (left: five, as the top)
        x = 0.355 + 0.075 * k
        g.add("near", hole((x, -JA, b + 0.002), (0, -1, 0), (1, 0, 0), 0.024, 0.0095, n=6))
    g.add("near", hole((0.470, JA, b + 0.001), (0, 1, 0), (1, 0, 0), 0.115, 0.0125, n=8))   # (right: the opening)
    g.add("near", hole((0.660, JA, b + 0.001), (0, 1, 0), (1, 0, 0), 0.024, 0.0095, n=6))
    # (the bipod's clamp round the jacket, and the front sight on its folding base)
    g.add("near", loft_x([(0.586, 0.0, b, JA + 0.0025, JA + 0.0025), (0.614, 0.0, b, JA + 0.0025, JA + 0.0025)], 12,
                         "steel", e=4.5, cap0=False, cap1=False))
    g.add("near", box((0.700, -0.009, b + JA - 0.001), (0.728, 0.009, b + JA + 0.010), "steel"))
    g.add("near", box((0.712, -0.0028, b + JA + 0.010), (0.720, 0.0028, b + JA + 0.040), "steel"))
    # --- the muzzle booster: a cone narrowing from the jacket's front plate, then flaring at the muzzle; the bore
    boo = [(0.735, 0.0, b, 0.0200, 0.0200), (0.775, 0.0, b, 0.0152, 0.0152), (0.842, 0.0, b, 0.0150, 0.0150),
           (0.872, 0.0, b, 0.0195, 0.0195)]
    g.add("near", loft_x(boo, 10, "steel", cap0=False, cap1=True, dark_caps=(False, True)))
    front = Piece("steel")                                # (the jacket's front plate, squared, round the booster)
    sq = [front.pt((J1, u, b + v)) for (u, v) in ring(12, JA, JA, 4.5, math.pi / 12)]
    rd = [front.pt((J1, u, b + v)) for (u, v) in ring(12, 0.0200, 0.0200, 2.0, math.pi / 12)]
    for k in range(12):
        front.quad(sq[k], sq[(k + 1) % 12], rd[(k + 1) % 12], rd[k])
    g.add("near", orient(front, normal=(1, 0, 0)))
    # --- the bipod: a hinge block under the clamp, two legs folded back under the jacket, a foot on each
    g.add("near", box((HINGE[0] - 0.012, -0.016, b - JA - 0.010), (HINGE[0] + 0.012, 0.016, b - JA + 0.002), "steel",
                      "bipod"))
    for s in (-1.0, 1.0):
        top = np.array((HINGE[0] - 0.004, 0.011 * s, b - JA - 0.008))
        end = np.array((LEG_END, 0.014 * s, b - JA - 0.015))
        mid = 0.5 * (top + end)
        g.add("near", loft_path([top, mid, end], [(0, 0, 1)] * 3, [(0.0045, 0.0055), (0.004, 0.005), (0.0045, 0.005)],
                                4, "steel", "bipod", cap0=False, cap1=False))
        g.add("near", oriented_box(end + np.array((-0.008, 0.0, -0.002)), np.eye(3), (0.010, 0.007, 0.004), "steel",
                                   "bipod"))
    g.grips["legs"] = [(np.array((0.300, 0.0, b - JA - 0.004)), np.array((LEG_END, 0.0, b - JA - 0.010))),
                       (np.array((LEG_END, 0.0, b - JA - 0.012)), np.array((0.580, 0.0, b - JA - 0.006)))]
    # --- the belt drum on the left of the feed, its belt's mouth going in
    drum = []
    cy0, cy1 = -0.030, -0.104
    cx, cz, R = 0.112, b - 0.022, 0.052
    side = Piece("steel")
    n = 12
    r0 = [side.pt((cx + R * math.cos(2 * math.pi * k / n), cy0, cz + R * math.sin(2 * math.pi * k / n))) for k in range(n)]
    r1 = [side.pt((cx + R * math.cos(2 * math.pi * k / n), cy1, cz + R * math.sin(2 * math.pi * k / n))) for k in range(n)]
    for k in range(n):
        side.quad(r0[k], r0[(k + 1) % n], r1[(k + 1) % n], r1[k])
    drum.append(orient(side, normal=None))
    for yy, sg in ((cy0, 1.0), (cy1, -1.0)):
        cp = Piece("steel")
        c = cp.pt((cx, yy, cz))
        rr = [cp.pt((cx + R * math.cos(2 * math.pi * k / n), yy, cz + R * math.sin(2 * math.pi * k / n))) for k in range(n)]
        for k in range(n):
            cp.tri(c, rr[k], rr[(k + 1) % n])
        drum.append(orient(cp, normal=(0.0, sg, 0.0)))
    # (its lid's hinge and latch, and the strap ring on top)
    drum.append(box((cx - 0.006, cy1 - 0.004, cz - 0.030), (cx + 0.006, cy1 + 0.001, cz + 0.030), "steel"))
    drum.extend(loft_path([np.array((cx - 0.020, -0.067, cz + R - 0.002)), np.array((cx, -0.067, cz + R + 0.014)),
                           np.array((cx + 0.020, -0.067, cz + R - 0.002))], [(0, 1, 0)] * 3,
                          [(0.0025, 0.0025)] * 3, 4, "steel", cap0=False, cap1=False))
    g.add("near", drum)
    # (the belt from the drum's mouth into the tray: brass rounds in their links)
    g.add("near", box((0.092, -0.040, b + 0.024), (0.132, -0.0245, b + 0.040), "brass"))
    g.add("near", box((0.094, -0.0455, b + 0.010), (0.130, -0.034, b + 0.036), "brass"))
    g.grips["under"] = [(np.array((-0.330, 0.0, b - 0.074)), np.array((-0.170, 0.0, b - 0.040))),
                        (np.array((-0.170, 0.0, b - 0.040)), np.array((-0.040, 0.0, b - 0.030))),
                        (np.array((0.050, 0.0, b - 0.045)), np.array((0.300, 0.0, b - 0.028)))] + g.grips["legs"]
    g.grips["butt"] = [(np.array((-0.330, 0.0, b - 0.074)), np.array((-0.090, 0.0, b - 0.024)))]

    # ------------------------------------------------------------------------------------------- mid (LOD1) ----
    g.add("mid", loft_x([(-0.362, 0.0, b - 0.012, 0.022, 0.062), (-0.200, 0.0, b - 0.002, 0.021, 0.046),
                         (-0.030, 0.0, b + 0.010, 0.021, 0.032)], 6, "bakelite", e=3.0, cap1=False))
    g.add("mid", loft_x([(-0.034, 0.0, b + 0.014, 0.0245, 0.042), (0.300, 0.0, b + 0.014, 0.0245, 0.042)], 6, "steel",
                        e=5.0, cap0=False))
    # (the grip's top runs up into the receiver: no trigger housing at this distance to close the gap)
    g.add("mid", loft_path([gt - ga * 0.016, gt + ga * 0.108], [(1, 0, 0.44)] * 2, [(0.0145, 0.019)] * 2, 4, "bakelite",
                           cap0=False))
    g.add("mid", loft_x([(J0, 0.0, b, JA, JA), (J1, 0.0, b, JA, JA)], 6, "steel", e=4.5, cap0=False, cap1=False))
    g.add("mid", hole((0.47, -JA, b), (0, -1, 0), (1, 0, 0), 0.15, 0.009, n=6))
    g.add("mid", loft_x([(J1, 0.0, b, 0.020, 0.020), (0.872, 0.0, b, 0.017, 0.017)], 6, "steel", cap0=True, cap1=True,
                        dark_caps=(False, True)))
    for s in (-1.0, 1.0):
        g.add("mid", loft_path([np.array((HINGE[0] - 0.004, 0.011 * s, b - JA - 0.008)),
                                np.array((LEG_END, 0.014 * s, b - JA - 0.016))], [(0, 0, 1)] * 2, [(0.005, 0.006)] * 2, 3,
                               "steel", "bipod", cap0=False, cap1=False))
    dm = Piece("steel")
    n = 8
    q0 = [dm.pt((cx + R * math.cos(2 * math.pi * k / n), cy0, cz + R * math.sin(2 * math.pi * k / n))) for k in range(n)]
    q1 = [dm.pt((cx + R * math.cos(2 * math.pi * k / n), cy1, cz + R * math.sin(2 * math.pi * k / n))) for k in range(n)]
    for k in range(n):
        dm.quad(q0[k], q0[(k + 1) % n], q1[(k + 1) % n], q1[k])
    for k in range(1, n - 1):
        dm.tri(q0[0], q0[k + 1], q0[k])
        dm.tri(q1[0], q1[k], q1[k + 1])
    g.add("mid", orient(dm))

    # ------------------------------------------------------------------------------------------- far (LOD2) ----
    g.add("far", loft_x([(-0.362, 0.0, b - 0.012, 0.022, 0.060), (-0.030, 0.0, b + 0.010, 0.021, 0.032)], 4,
                        "bakelite", e=2.0, cap0=False, cap1=False, phase=math.pi / 4))
    g.add("far", loft_x([(-0.030, 0.0, b + 0.008, 0.026, 0.038), (0.300, 0.0, b + 0.008, 0.026, 0.038),
                         (0.872, 0.0, b, 0.020, 0.020)], 4, "steel", e=2.0, cap0=False, cap1=False, phase=math.pi / 4))
    g.add("far", loft_path([gt - ga * 0.016, gt + ga * 0.105], [(1, 0, 0.44)] * 2, [(0.015, 0.019)] * 2, 3, "bakelite",
                           cap0=False, cap1=False))
    dfar = Piece("steel")
    a_ = [dfar.pt((cx + R * math.cos(2 * math.pi * k / 4 + math.pi / 4), cy0, cz + R * math.sin(2 * math.pi * k / 4 + math.pi / 4)))
          for k in range(4)]
    b_ = [dfar.pt((cx + R * math.cos(2 * math.pi * k / 4 + math.pi / 4), cy1, cz + R * math.sin(2 * math.pi * k / 4 + math.pi / 4)))
          for k in range(4)]
    for k in range(4):
        dfar.quad(a_[k], a_[(k + 1) % 4], b_[(k + 1) % 4], b_[k])
    # (the drum is not drawn this far off)
    for s in (-1.0, 1.0):
        # (a flat blade each, seen from above and from below: each face its own points, so each has its own facing)
        for up in (1.0, -1.0):
            lg = Piece("steel", "bipod")
            p0 = lg.pt((HINGE[0] - 0.004, 0.011 * s, b - JA - 0.008))
            p1 = lg.pt((LEG_END, 0.014 * s - 0.006, b - JA - 0.016))
            p2 = lg.pt((LEG_END, 0.014 * s + 0.006, b - JA - 0.016))
            lg.tri(p0, p1, p2)
            g.add("far", orient(lg, normal=(0.0, 0.0, up)))
    return g


GUNS = {"mg42": mg42}


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
