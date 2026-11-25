"""The Soviet section's light machine gun, the DP (the "Degtyaryov infantry" gun of 1928, 7.62 mm), for the gunners'
files (mg_gunners.py puts it in the Soviet rifleman's rifle's places). Our own drawing, to period proportions, from
the same simple solids as the German guns (mg_guns.py), in the weapon bone's frame: metres, x along the bore toward
the muzzle, y to the gunner's right, z up, the origin where the rifleman's right fist closes on his rifle's wrist.

What makes it this gun and no other is the wide flat pan lying on top of the receiver like a record: 47 rounds
lying round it, their bullets to the middle. The rest: a wooden butt with a rifle's wrist and no pistol grip (the
army's own rifle wood), the trigger guard with the grip safety behind it, a slim receiver with tall sights that see
over the pan, the barrel in a shroud pierced with short slots, the gas tube under it, the wide cone of the flash
hider at the muzzle, and the bipod clamped round the shroud's front end.

    python3 mg_gun_dp.py        the gun's size at each level
"""
import math

import numpy as np

import mg_guns as mg

LENGTH = 1.270               # (heel to the flash hider's mouth)
HEEL = -0.347                # (the butt plate's back: where the rifle's butt was, so it meets his shoulder)
MUZZLE = HEEL + LENGTH       # (the flash hider's mouth)
# (the pan: about 26 cm across and 4 cm deep, its middle over the receiver's front end, so its front half hangs
# over the back of the shroud)
PAN_X = 0.265
PAN_R = 0.132
PAN_Z0, PAN_Z1 = 0.025, 0.061        # (its underside and its top over the bore: it sits on the receiver's top)
# (the barrel's shroud and the gas tube under it)
SHROUD_R = 0.0225
SHROUD_X = (0.275, 0.665)
GAS_Z, GAS_R = -0.029, 0.010         # (the gas tube's middle under the bore, and its radius)
GAS_END = 0.705
UNDER = GAS_Z - GAS_R                # (the gas tube's underside over the bore: the bipod's legs fold back under it)
HINGE_X = 0.620


# ------------------------------------------------------------------------------------------------ small solids ----
def block(lo, hi, paint, bone="weapon"):
    """A small axis-true box on eight shared points (the grip safety, a sight's post: too small for its faces to want
    their own points, so its edges shade round)."""
    pc = mg.Piece(paint, bone)
    lo, hi = np.asarray(lo, float), np.asarray(hi, float)
    ix = [pc.pt((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    for f in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        pc.quad(*[ix[k] for k in f])
    return mg.orient(pc)


def ear(x0, x1, xt0, xt1, y0, y1, z0, z1, paint="steel"):
    """A sight's ear: a small plate standing along the gun, x0 to x1 at its foot (z0), narrowing to xt0 to xt1 at its
    top (z1), between y0 and y1; eight shared points."""
    pc = mg.Piece(paint)
    ix = [pc.pt(p) for p in ((x0, y0, z0), (xt0, y0, z1), (x0, y1, z0), (xt0, y1, z1),
                             (x1, y0, z0), (xt1, y0, z1), (x1, y1, z0), (xt1, y1, z1))]
    for f in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        pc.quad(*[ix[k] for k in f])
    return mg.orient(pc)


def end_fan(side, first, n, sign):
    """Closes one end of a loft's sides on the ring's own points (`first` the ring's first point, n round it): a fan
    from one corner, facing back (sign -1) or forward (1) along the gun. No new points: the end shades round."""
    P = side.P
    for k in range(1, n - 1):
        a, b_, c = first, first + k, first + k + 1
        nn = np.cross(P[b_] - P[a], P[c] - P[a])
        side.tri(*((a, b_, c) if nn[0] * sign >= 0 else (a, c, b_)))
    return side


def lathe(cx, cy, profile, n, paint, bone="weapon", phase=0.0):
    """A solid turned about an upright axis through (cx, cy): `profile` [(radius, z)...] runs round its section the
    way that keeps the solid on the left (out along the underside, up the wall, back in over the top); n points
    round each radius, one where the radius is 0; every point shared, so it shades round."""
    pc = mg.Piece(paint, bone)
    rings = []
    for (r, z) in profile:
        if r <= 1e-9:
            rings.append([pc.pt((cx, cy, z))])
        else:
            rings.append([pc.pt((cx + r * math.cos(2 * math.pi * k / n + phase),
                                 cy + r * math.sin(2 * math.pi * k / n + phase), z)) for k in range(n)])
    for i in range(len(rings) - 1):
        r0, r1 = rings[i], rings[i + 1]
        (ra, za), (rb, zb) = profile[i], profile[i + 1]
        for k in range(n):
            k1 = (k + 1) % n
            if len(r0) == 1 and len(r1) == 1:
                continue
            if len(r0) == 1:
                tris = [(r0[0], r1[k], r1[k1])]
            elif len(r1) == 1:
                tris = [(r0[k], r0[k1], r1[0])]
            else:
                tris = [(r0[k], r0[k1], r1[k1]), (r0[k], r1[k1], r1[k])]
            for (a, b_, c) in tris:
                P = [pc.P[a], pc.P[b_], pc.P[c]]
                m = (P[0] + P[1] + P[2]) / 3
                rad = np.array((m[0] - cx, m[1] - cy, 0.0))
                rad /= max(np.linalg.norm(rad), 1e-12)
                ref = rad * (zb - za) + np.array((0.0, 0.0, -(rb - ra)))     # (outward: to the right of the way round)
                nn = np.cross(P[1] - P[0], P[2] - P[0])
                pc.tri(*((a, b_, c) if nn @ ref >= 0 else (a, c, b_)))
    return pc


def disc(cx, cy, z, r, n, up, paint, rise=None, phase=0.0, dark=False):
    """A flat round face on its own points, facing up (up 1) or down (-1): a fan from one corner, or, with `rise`,
    from a middle point that much higher (a lid rising a little to its middle)."""
    pc = mg.Piece(paint, dark=dark)
    rr = [pc.pt((cx + r * math.cos(2 * math.pi * k / n + phase), cy + r * math.sin(2 * math.pi * k / n + phase), z))
          for k in range(n)]
    if rise is None:
        for k in range(1, n - 1):
            pc.tri(rr[0], rr[k], rr[k + 1])
    else:
        c = pc.pt((cx, cy, z + rise))
        for k in range(n):
            pc.tri(c, rr[k], rr[(k + 1) % n])
    return mg.orient(pc, normal=(0.0, 0.0, float(up)))


def rib(p0, p1, half, high, ease=0.007):
    """A pressed rib lying on a lid from p0 to p1 (both on the lid): a low ridge, `half` its half width at the foot,
    `high` its crest over the lid, its ends eased down; six shared points, no underside."""
    p0, p1 = np.asarray(p0, float), np.asarray(p1, float)
    d = p1 - p0
    ln = np.linalg.norm(d)
    d /= ln
    side = np.cross((0.0, 0.0, 1.0), d)
    side /= np.linalg.norm(side)
    up = np.array((0.0, 0.0, high))
    pc = mg.Piece("steel")
    a0, a1 = pc.pt(p0 - side * half), pc.pt(p0 + side * half)
    b0, b1 = pc.pt(p1 - side * half), pc.pt(p1 + side * half)
    c0, c1 = pc.pt(p0 + d * ease + up), pc.pt(p1 - d * ease + up)
    pc.quad(a0, b0, c1, c0)
    pc.quad(b1, a1, c0, c1)
    pc.tri(a1, a0, c0)
    pc.tri(b0, b1, c1)
    P = np.array(pc.P)
    cen = 0.5 * (p0 + p1) - np.array((0.0, 0.0, high))          # (a point under the ridge: every face looks from it)
    out = []
    for (a, b_, c) in pc.T:
        nn = np.cross(P[b_] - P[a], P[c] - P[a])
        out.append((a, b_, c) if nn @ ((P[a] + P[b_] + P[c]) / 3 - cen) >= 0 else (a, c, b_))
    pc.T = out
    return pc


def lozenge(centre, normal, e1, half_len, r, lift=0.0006):
    """A dark short slot lying on a face (a cooling slot seen from outside): straight sides, a point at each end;
    six points, a fan from one of them, lifted a little off the face."""
    normal = np.asarray(normal, float) / np.linalg.norm(normal)
    e1 = np.asarray(e1, float)
    e1 = e1 - normal * (e1 @ normal)
    e1 /= np.linalg.norm(e1)
    e2 = np.cross(normal, e1)
    c0 = np.asarray(centre, float) + normal * lift
    pc = mg.Piece("steel", dark=True)
    rim = [pc.pt(c0 + e1 * u + e2 * v) for (u, v) in ((half_len, 0.0), (half_len - r, r), (-(half_len - r), r),
                                                      (-half_len, 0.0), (-(half_len - r), -r), (half_len - r, -r))]
    for k in range(1, 5):
        pc.tri(rim[0], rim[k], rim[k + 1])
    return mg.orient(pc, normal=normal)


def pan(g, level, b, n):
    """The pan magazine on the receiver's top: its wall and rolled top edge, its lid rising a little to the middle,
    its underside (seen where it hangs over either side of the gun); near, the pressed ribs out from the middle of the
    lid, and the round catch plate in the middle with the catch across it."""
    z0, z1 = b + PAN_Z0, b + PAN_Z1
    ph = math.pi / n
    if level == "near":
        g.add(level, lathe(PAN_X, 0.0, [(PAN_R, z0), (PAN_R, z1 - 0.006), (PAN_R - 0.006, z1)], n, "steel", phase=ph))
        g.add(level, disc(PAN_X, 0.0, z1, PAN_R - 0.006, n, 1, "steel", rise=0.003, phase=ph))
        g.add(level, disc(PAN_X, 0.0, z0, PAN_R, n, -1, "steel", phase=ph))
        rr = PAN_R - 0.006
        for k in range(8):
            a = math.radians(22.5 + 45.0 * k)
            ca, sa = math.cos(a), math.sin(a)
            r0, r1 = 0.046, 0.114
            g.add(level, rib((PAN_X + r0 * ca, r0 * sa, z1 + 0.003 * (1 - r0 / rr) - 0.0004),
                             (PAN_X + r1 * ca, r1 * sa, z1 + 0.003 * (1 - r1 / rr) - 0.0004), 0.0050, 0.0042))
        g.add(level, lathe(PAN_X, 0.0, [(0.035, z1 + 0.001), (0.033, z1 + 0.0075), (0.0, z1 + 0.008)], 10, "steel",
                           phase=math.pi / 10))
        g.add(level, mg.box((PAN_X - 0.020, -0.0045, z1 + 0.0070), (PAN_X + 0.020, 0.0045, z1 + 0.0105), "steel"))
    else:
        # (mid: the wall and the underside on shared points, the lid on its own so it stays flat; a dark middle)
        pc = mg.Piece("steel")
        rim = [(PAN_X + PAN_R * math.cos(2 * math.pi * k / n + ph), PAN_R * math.sin(2 * math.pi * k / n + ph))
               for k in range(n)]
        lo = [pc.pt((x, y, z0)) for (x, y) in rim]
        hi = [pc.pt((x, y, z1)) for (x, y) in rim]
        for k in range(n):
            k1 = (k + 1) % n
            pc.quad(lo[k], lo[k1], hi[k1], hi[k])
        for k in range(1, n - 1):
            pc.tri(lo[0], lo[k + 1], lo[k])
        P = np.array(pc.P)
        T = []
        for (a, b_, c) in pc.T:
            m = (P[a] + P[b_] + P[c]) / 3
            nn = np.cross(P[b_] - P[a], P[c] - P[a])
            flat = abs(P[a][2] - z0) < 1e-9 and abs(P[b_][2] - z0) < 1e-9 and abs(P[c][2] - z0) < 1e-9
            ref = np.array((0.0, 0.0, -1.0)) if flat else np.array((m[0] - PAN_X, m[1], 0.0))
            T.append((a, b_, c) if nn @ ref >= 0 else (a, c, b_))
        pc.T = T
        g.add(level, pc)
        g.add(level, disc(PAN_X, 0.0, z1, PAN_R, n, 1, "steel", phase=ph))
        g.add(level, disc(PAN_X, 0.0, z1 + 0.0008, 0.034, 6, 1, "steel", dark=True))


# ------------------------------------------------------------------------------------------------------ the DP ----
def dp():
    """The Soviet DP light machine gun of 1928 on, as a section's gunner carries it on its bipod: 1.27 m. From the
    back: the steel butt plate; the wooden butt, deep at the heel, its underside rising to a rifle's wrist where the
    right fist closes (no pistol grip), the grip safety a lever under the wrist behind the trigger guard; the slim
    receiver, the trigger frame under its rear with the guard and trigger, the cocking handle in its slot low on the
    right; the rear sight between its two ears just behind the pan; the pan itself, flat on the receiver's top, its
    lid ribbed out from the round catch plate in the middle; the barrel's shroud pierced with rows of short slots,
    the gas tube under it running on past the shroud to the gas block under the barrel; the tall front sight between
    its ears on the shroud's front, and the bipod's clamp round shroud and gas tube behind it, the legs folded back
    under the gas tube; the bare barrel; the wide cone of the flash hider. Fed from the top, so nothing hangs under
    or beside the gun where a hand holds it."""
    g = mg.Gun("dp", "DP")
    b = mg.BORE
    g.paints = {}                                           # (steel and the rifle's own wood: no paints of its own)
    g.muzzle = np.array((MUZZLE, 0.0, b))
    g.hinge = mg.hinge_of(b, UNDER, HINGE_X)
    le = mg.leg_end(g.hinge)
    S0, S1 = SHROUD_X
    # --- the butt: rifle wood, the comb falling a little from the wrist to the heel, the underside rising from the toe
    # to the wrist with a hump behind the hand (a half pistol grip), then on under the receiver's back end
    butt = [(HEEL, 0.0, b - 0.076, 0.0195, 0.058), (-0.200, 0.0, b - 0.059, 0.0200, 0.047),
            (-0.092, 0.0, b - 0.0455, 0.0190, 0.0375), (-0.056, 0.0, b - 0.031, 0.0172, 0.0245),
            (-0.020, 0.0, b - 0.029, 0.0172, 0.0240), (0.004, 0.0, b - 0.023, 0.0185, 0.029)]
    g.add("near", mg.loft_x(butt, 8, "wood", e=2.6, cap0=False, cap1=False))
    # (the steel butt plate: the butt's end face)
    plate = mg.Piece("steel")
    pm = plate.pt((HEEL, 0.0, b - 0.076))
    pr = [plate.pt((HEEL, u, b - 0.076 + v)) for (u, v) in mg.ring(8, 0.0195, 0.058, 2.6, math.pi / 8)]
    for k in range(8):
        plate.tri(pm, pr[k], pr[(k + 1) % 8])
    g.add("near", mg.orient(plate, normal=(-1.0, 0.0, 0.0)))
    # (the grip safety: a lever let into the wrist's underside behind the guard, squeezed by the hand that holds it)
    g.add("near", block((-0.044, -0.0050, b - 0.0600), (-0.002, 0.0050, b - 0.0500), "steel"))
    g.grips["pistol"] = {"at": np.array((-0.034, 0.0, b - 0.030)), "axis": None}
    # --- the receiver: slim, its corners eased, its back end rounded off over the wrist; the trigger frame under its
    # rear; the guard and the trigger
    g.add("near", mg.loft_x([(-0.014, 0.0, b - 0.004, 0.0150, 0.0230), (-0.002, 0.0, b - 0.006, 0.0210, 0.0320),
                             (S0, 0.0, b - 0.006, 0.0210, 0.0320)], 8, "steel", e=4.0, cap1=False))
    g.add("near", mg.box((0.000, -0.0130, b - 0.0500), (0.105, 0.0130, b - 0.0360), "steel"))
    tg = [np.array((0.078, 0.0, b - 0.048)), np.array((0.066, 0.0, b - 0.080)), np.array((0.026, 0.0, b - 0.083)),
          np.array((0.004, 0.0, b - 0.055))]
    g.add("near", mg.loft_path(tg, [(0, 1, 0)] * 4, [(0.0030, 0.0060)] * 4, 4, "steel", cap0=False, cap1=False))
    g.add("near", mg.loft_path([np.array((0.042, 0.0, b - 0.049)), np.array((0.035, 0.0, b - 0.072))],
                               [(0, 1, 0)] * 2, [(0.0026, 0.0042)] * 2, 4, "steel", cap0=False, cap1=False))
    # (the cocking handle low on the right, drawn back, and the slot it runs in)
    g.add("near", mg.box((0.100, 0.0200, b - 0.0300), (0.118, 0.0400, b - 0.0180), "steel"))
    g.add("near", lozenge((0.180, 0.0210, b - 0.024), (0, 1, 0), (1, 0, 0), 0.082, 0.0040))
    # (the rear sight just behind the pan: its bed on the receiver's top, the leaf lying in it, an ear either side)
    top = b + 0.026
    g.add("near", mg.box((0.066, -0.0125, top - 0.001), (0.128, 0.0125, top + 0.008), "steel"))
    g.add("near", mg.box((0.072, -0.0055, top + 0.008), (0.116, 0.0055, top + 0.016), "steel"))
    for s in (-1.0, 1.0):
        g.add("near", ear(0.094, 0.127, 0.104, 0.123, min(s * 0.0085, s * 0.0125), max(s * 0.0085, s * 0.0125),
                          top + 0.008, top + 0.047))
    # --- the pan
    pan(g, "near", b, 20)
    # --- the barrel's shroud, pierced: two rows of short slots up either side, set between one another
    g.add("near", mg.loft_x([(S0, 0.0, b, SHROUD_R, SHROUD_R), (S1, 0.0, b, SHROUD_R, SHROUD_R)], 12, "steel",
                            cap0=False, phase=0.0))
    face = SHROUD_R * math.cos(math.pi / 12)                # (each flat of the twelve-sided shroud, from the bore)
    for ang, x0 in ((15.0, 0.330), (45.0, 0.357), (165.0, 0.330), (135.0, 0.357)):
        th = math.radians(ang)
        n_ = np.array((0.0, math.cos(th), math.sin(th)))
        for k in range(5):
            g.add("near", lozenge((x0 + 0.054 * k, face * n_[1], b + face * n_[2]), n_, (1, 0, 0), 0.0210, 0.0052))
    # --- the gas tube under the shroud, on past it to the gas block that hangs from the barrel
    g.add("near", mg.loft_x([(S0, 0.0, b + GAS_Z, GAS_R, GAS_R), (GAS_END, 0.0, b + GAS_Z, GAS_R, GAS_R)], 6, "steel",
                            cap0=False, cap1=False))
    g.add("near", mg.box((0.682, -0.0100, b + GAS_Z - 0.013), (0.712, 0.0100, b - 0.008), "steel"))
    # --- the barrel, bare ahead of the shroud; the flash hider: a wide cone, its mouth dark
    g.add("near", mg.loft_x([(S1, 0.0, b, 0.0125, 0.0125), (0.816, 0.0, b, 0.0118, 0.0118)], 8, "steel", cap0=False,
                            cap1=False))
    g.add("near", mg.loft_x([(0.808, 0.0, b, 0.0120, 0.0120), (0.818, 0.0, b, 0.0150, 0.0150),
                             (MUZZLE, 0.0, b, 0.0270, 0.0270)], 10, "steel", cap0=False, cap1=True,
                            dark_caps=(False, True)))
    # --- the front sight on the shroud's front: its foot, the post, an ear either side
    st = b + SHROUD_R
    g.add("near", mg.box((0.628, -0.0100, st - 0.003), (0.658, 0.0100, st + 0.016), "steel"))
    g.add("near", block((0.6405, -0.0018, st + 0.016), (0.6455, 0.0018, st + 0.046), "steel"))
    for s in (-1.0, 1.0):
        g.add("near", ear(0.630, 0.656, 0.637, 0.650, min(s * 0.0070, s * 0.0100), max(s * 0.0070, s * 0.0100),
                          st + 0.016, st + 0.050))
    # --- the bipod: its clamp round the shroud and the gas tube, the legs folded back under the gas tube
    cz = 0.5 * (SHROUD_R + UNDER)
    ch = 0.5 * (SHROUD_R - UNDER) + 0.0025
    g.add("near", mg.loft_x([(HINGE_X - 0.012, 0.0, b + cz, SHROUD_R + 0.0025, ch),
                             (HINGE_X + 0.012, 0.0, b + cz, SHROUD_R + 0.0025, ch)], 10, "steel", e=2.4, cap0=False,
                            cap1=False))
    mg.folded_bipod(g, "near", b, UNDER)
    # --- where hands go: the wrist for the right fist (set above); the gun's underside from the toe to the gas tube's
    # front (the butt, the wrist, the trigger frame, the receiver, the folded legs, the gas tube); the butt's underside
    g.grips["legs"] = [(np.array((le, 0.0, b + UNDER - 0.012)), np.array((0.580, 0.0, b + UNDER - 0.008)))]
    line = [(HEEL, b - 0.134), (-0.200, b - 0.106), (-0.092, b - 0.083), (-0.056, b - 0.0555), (0.000, b - 0.052),
            (0.105, b - 0.050), (0.112, b - 0.038), (S0, b - 0.038), (le, b + UNDER - 0.012),
            (0.580, b + UNDER - 0.008), (GAS_END, b + UNDER)]
    g.grips["under"] = [(np.array((x0, 0.0, z0)), np.array((x1, 0.0, z1)))
                        for (x0, z0), (x1, z1) in zip(line, line[1:])]
    g.grips["butt"] = [(np.array((HEEL + 0.020, 0.0, b - 0.130)), np.array((-0.092, 0.0, b - 0.083)))]
    g.grips["keep_out"] = None                              # (fed from the top: nothing hangs where a hand holds)
    g.grips["hip_l_at"] = 0.300                             # (from the hip, his left fist on the folded legs' feet
                                                            # under the pan's middle: further out along the gun his
                                                            # forearm would come up through the pan's rim)
    g.grips["kneel_l_at"] = 0.230                           # (kneeling, under the receiver ahead of the trigger frame)
    g.grips["park"] = (0.150, 0.0, b - 0.006)               # (well inside the receiver)
    # (how it rides and lies, the pan being 13 cm out either side: on his back with its top to him, the pan flat
    # against his back, the bore this far off it; on the ground leant over to its right on the butt's toe, the pan's
    # rim and the bipod's block: the direction of the gun's own that points into the ground, and how far the ground
    # then is from the gun's origin along it)
    g.grips["slung"] = {"to_back": (0.0, 0.0, 1.0), "off": 0.066}
    g.grips["laid"] = {"down": (0.058, 0.660, -0.749), "off": 0.0913}

    # ------------------------------------------------------------------------------------------- mid (LOD1) ----
    # (the butt, the receiver and the shroud each closed at its seen end on its own points)
    wood = mg.loft_x([(HEEL, 0.0, b - 0.076, 0.0195, 0.058), (-0.050, 0.0, b - 0.031, 0.0172, 0.0245),
                      (0.004, 0.0, b - 0.023, 0.0185, 0.029)], 6, "wood", e=2.6, cap0=False, cap1=False)[0]
    g.add("mid", end_fan(wood, 0, 6, -1))
    body = mg.loft_x([(-0.012, 0.0, b - 0.008, 0.0210, 0.0340), (S0, 0.0, b - 0.008, 0.0210, 0.0340)], 6, "steel",
                     e=4.0, cap0=False, cap1=False)[0]
    g.add("mid", end_fan(body, 0, 6, -1))
    pan(g, "mid", b, 12)
    # (the shroud, its slots one dark run each side; the gas tube; the barrel; the flash hider)
    tube = mg.loft_x([(S0, 0.0, b, SHROUD_R, SHROUD_R), (S1, 0.0, b, SHROUD_R, SHROUD_R)], 6, "steel",
                     cap0=False, cap1=False, phase=0.0)[0]
    g.add("mid", end_fan(tube, 6, 6, 1))
    f6 = SHROUD_R * math.cos(math.pi / 6)
    for ang in (30.0, 150.0):
        th = math.radians(ang)
        n_ = np.array((0.0, math.cos(th), math.sin(th)))
        g.add("mid", lozenge((0.510, f6 * n_[1], b + f6 * n_[2]), n_, (1, 0, 0), 0.100, 0.0070, lift=0.0008))
    g.add("mid", mg.loft_x([(S0, 0.0, b + GAS_Z, GAS_R * 1.2, GAS_R * 1.2), (GAS_END, 0.0, b + GAS_Z, GAS_R * 1.2,
                                                                         GAS_R * 1.2)], 3, "steel", cap0=False,
                           cap1=False, phase=-math.pi / 2))
    g.add("mid", mg.loft_x([(S1, 0.0, b, 0.0130, 0.0130), (0.826, 0.0, b, 0.0130, 0.0130)], 4, "steel", cap0=False,
                           cap1=False))
    g.add("mid", mg.loft_x([(0.826, 0.0, b, 0.0150, 0.0150), (MUZZLE, 0.0, b, 0.0270, 0.0270)], 6, "steel",
                           cap0=False, cap1=True, dark_caps=(False, True)))
    mg.folded_bipod(g, "mid", b, UNDER)

    # ------------------------------------------------------------------------------------------- far (LOD2) ----
    # (boxes: the butt three-sided, its comb the ridge; the receiver, shroud and gas tube one box; the barrel
    # three-sided, widening to the flash hider's mouth; the pan a six-sided slab)
    s2 = 1.0 / math.sqrt(2.0)
    g.add("far", mg.loft_x([(HEEL, 0.0, b - 0.0953, 0.0230, 0.0773), (0.000, 0.0, b - 0.0327, 0.0210, 0.0387)], 3,
                           "wood", cap0=False, cap1=False, phase=math.pi / 2))
    g.add("far", mg.loft_x([(0.000, 0.0, b - 0.007, 0.0215 / s2, 0.032 / s2), (S1, 0.0, b - 0.007, 0.0215 / s2,
                                                                           0.032 / s2)], 4, "steel", cap0=False,
                           cap1=False, phase=math.pi / 4))
    g.add("far", mg.loft_x([(S1, 0.0, b, 0.016, 0.016), (MUZZLE, 0.0, b, 0.024, 0.024)], 3, "steel", cap0=False,
                           cap1=False, phase=math.pi / 2))
    # (the pan's wall, and its lid on points of its own so the top stays flat: on the wall's points it shaded round
    # like a dome)
    g.add("far", lathe(PAN_X, 0.0, [(PAN_R, b + PAN_Z0), (PAN_R, b + PAN_Z1)], 6, "steel", phase=math.pi / 6))
    g.add("far", disc(PAN_X, 0.0, b + PAN_Z1, PAN_R, 6, 1, "steel", phase=math.pi / 6))
    mg.folded_bipod(g, "far", b, UNDER)
    return g


def report():
    g = dp()
    for L in ("near", "mid", "far"):
        P, N, paint, bone, dark, T = g.mesh(L)
        print(f"dp {L}: {len(P)} points, {len(T)} triangles, {int(dark.sum())} dark points, "
              f"x {P[:, 0].min():.3f} to {P[:, 0].max():.3f}, y {P[:, 1].min():.3f} to {P[:, 1].max():.3f}, "
              f"z {P[:, 2].min():.3f} to {P[:, 2].max():.3f}")


if __name__ == "__main__":
    report()
