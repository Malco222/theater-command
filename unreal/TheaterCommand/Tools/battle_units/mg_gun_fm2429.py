"""The French section's light machine gun, the FM 24/29 (the 1924 light machine gun in its 1929 form, 7.5 mm, 1930
on): our own drawing to period proportions, built from mg_guns.py's simple solids at the three levels of a soldier
file, in the weapon bone's frame (metres, x along the bore toward the muzzle, y to the gunner's right, z up, the origin
where the rifleman's right fist closes on his rifle's wrist).

What makes it this gun and no other: the STRAIGHT 25-round box magazine standing on TOP of the receiver, leaning a
little forward (flat-sided and straight, where the British gun's curves), and under the receiver the long trigger guard
with TWO triggers in it, one behind the other (the front one fires single shots, the rear one bursts). The rest: the
wooden butt (the army's own rifle wood) with its steel butt plate and the folding shoulder plate lying along its comb;
the separate wooden pistol grip; the slim squared receiver with the magazine's housing on top and the sights set off
to the left of the magazine; the wooden fore-end under the barrel's rear part; the gas cylinder under the barrel; the
bipod clamped round the gas cylinder's front; the slim barrel and the conical flash hider. It feeds from the top, so
nothing hangs under it but the grip and the guard.

    python3 mg_gun_fm2429.py    the gun's size at each level
"""
import math

import numpy as np

import mg_guns as mg

# ---- the gun's measures along it (1.07 m from the butt plate to the flash hider's mouth)
HEEL = -0.345                # (the butt plate's back: the rifle's heel was at -0.33)
MUZZLE = 0.728               # (the flash hider's mouth)
BODY0, BODY1 = -0.092, 0.212  # (the receiver, from the butt's neck to the barrel's collar)
BODY_A = 0.0190              # (its half width: a slim gun)
BODY_TOP, BODY_LOW = 0.026, -0.036   # (its top and its underside over the bore)
# (the magazine: 8.2 cm fore and aft, 2.7 cm across, standing 15 cm over the housing on the receiver's top, straight,
# leaning forward a little)
MAG_X = 0.088                # (the middle of its foot along the gun, over the trigger guard's front half)
MAG_W, MAG_HY = 0.082, 0.0135
MAG_H = 0.160                # (from its foot, sunk in the housing, to its floor plate's top)
MAG_FOOT = 0.028             # (its foot's middle over the bore)
MAG_LEAN = math.radians(8.0)
HOUSING_TOP = 0.037          # (the magazine's housing's top over the bore)
# (the wooden fore-end, the gas cylinder under the barrel, the gas block near the muzzle, the bipod on the gas
# cylinder's front, the flash hider)
FORE0, FORE1 = 0.228, 0.445
GAS_Z, GAS_R = -0.029, 0.0095
GAS_END = 0.600              # (the gas block's middle: the gas cylinder meets the barrel here)
HINGE_X = 0.555              # (the bipod's clamp on the gas cylinder just behind the gas block, two thirds of the way
                             # along the barrel from the receiver, as on the real gun: its legs, folded, end 3 cm
                             # ahead of the fore-end's back, 15 cm ahead of the trigger guard)
HIDER0 = 0.672               # (where the flash hider's cone begins)
# (the sights' line, off to the left of the magazine)
SIGHT_Y, SIGHT_Z = -0.028, 0.054


def block(lo, hi, paint, bone="weapon"):
    """A small box on eight points, each shared by the three faces that meet there: for parts too small for their
    faces' own shading to be seen."""
    pc = mg.Piece(paint, bone)
    lo, hi = np.asarray(lo, float), np.asarray(hi, float)
    for x in (lo[0], hi[0]):
        for y in (lo[1], hi[1]):
            for z in (lo[2], hi[2]):
                pc.pt((x, y, z))
    for f in [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
        pc.quad(*f)
    return mg.orient(pc)


def end_fan(side, first, n, sign):
    """Closes one end of a loft's sides on the ring's own points (`first` the ring's first point, n round it): a fan
    from one corner, facing back (sign -1) or forward (1) along the gun. No new points."""
    P = side.P
    for k in range(1, n - 1):
        a, b_, c = first, first + k, first + k + 1
        nn = np.cross(P[b_] - P[a], P[c] - P[a])
        side.tri(*((a, b_, c) if nn[0] * sign >= 0 else (a, c, b_)))
    return side


def mag_frame(b):
    """The magazine's foot (its middle, x and z), the way up it and the way toward its front, in the side view."""
    up = np.array((math.sin(MAG_LEAN), math.cos(MAG_LEAN)))
    fore = np.array((math.cos(MAG_LEAN), -math.sin(MAG_LEAN)))
    return np.array((MAG_X, b + MAG_FOOT)), up, fore


def mag_point(b, s, h):
    """A point of the magazine's side view: `s` toward its front from its middle line, `h` up it from its foot."""
    foot, up, fore = mag_frame(b)
    p = foot + fore * s + up * h
    return (float(p[0]), float(p[1]))


def mag_cut(b, s, z):
    """The point of the magazine's side view `s` toward its front from its middle line where it comes up through the
    level `z` (the top of whatever it stands in)."""
    foot, up, fore = mag_frame(b)
    h = (z - foot[1] - fore[1] * s) / up[1]
    return mag_point(b, s, h)


def mag_outline(b, s0=-MAG_W / 2, s1=MAG_W / 2, h0=0.0, h1=MAG_H, z0=None):
    """The magazine's side view: a straight box leaning forward (back foot, front foot, front top, back top). With
    `z0`, its foot is cut level there, where it comes out of its housing (or the receiver's top): so no corner of it
    lies buried inside the housing, where the shading would count it shut in and darken the whole magazine."""
    if z0 is None:
        foot = [mag_point(b, s0, h0), mag_point(b, s1, h0)]
    else:
        foot = [mag_cut(b, s0, z0), mag_cut(b, s1, z0)]
    return foot + [mag_point(b, s1, h1), mag_point(b, s0, h1)]


def fm2429():
    """The FM 24/29 light machine gun as a section's gunner carries it on its bipod: 1.07 m. From the butt forward: the
    steel butt plate, the folding shoulder plate hinged at its top and lying forward along the comb; the wooden butt,
    its comb straight and its underside running straight down to the toe; the slim squared receiver, the separate wooden
    pistol grip under its back end, the long trigger guard ahead of the grip with the two triggers one behind the
    other in it (the rear for bursts, the front for single shots), the trigger housing over them, the safety on the
    right over the grip, the cocking handle and its slot on the right, the empty cases' port under the receiver's
    front; the rear sight on its bracket on the receiver's left, behind the magazine (the magazine stands in the
    sights' way, so they are set off to the left); the straight 25-round box magazine standing in its housing on the
    receiver's top and leaning a little forward, a pressed rib down each side and its floor plate a lip round its top,
    the catch behind it and the housing's dust cover laid open forward on the receiver; the barrel's collar at the
    receiver's front, the wooden fore-end under the barrel's rear part, the gas cylinder under the barrel running on
    to the gas block near the muzzle, its regulator on the block's front; the bipod's clamp on the gas cylinder just
    behind the block, its legs folded back under the fore-end; the front sight on its band on the barrel ahead of the
    block, off to the left on its arm, its blade between two ears; the slim barrel; the conical flash hider.
    Mid: the same big shapes (the magazine a flat-sided box leaning on top, the butt, grip and fore-end wood). Far:
    boxes, the magazine a closed box leaning a little forward on the receiver."""
    g = mg.Gun("fm2429", "FM 24/29")
    b = mg.BORE
    s2 = 1.0 / math.sqrt(2.0)                               # (a four-sided loft's sides stand this far in of its corners)
    g.paints = {}                                           # (steel and the rifle's wood only: no cartridge shows)
    g.muzzle = np.array((MUZZLE, 0.0, b))
    under = GAS_Z - GAS_R                                   # (the gas cylinder's underside over the bore)
    g.hinge = mg.hinge_of(b, under, HINGE_X)
    le = mg.leg_end(g.hinge)
    A = BODY_A
    foot, mup, mfore = mag_frame(b)
    up3 = np.array((mup[0], 0.0, mup[1]))                   # (up the magazine and toward its front, in the gun's frame)
    fore3 = np.array((mfore[0], 0.0, mfore[1]))

    # ------------------------------------------------------------------------------------------ near (LOD0) ----
    # --- the butt: wood, an upright oval, its comb straight on behind the receiver's top, its underside running
    # straight down from the neck to the toe (a rifle's butt line, not a bellied one); the steel butt plate; the
    # shoulder plate's hinge on the plate's top and the plate itself folded forward along the comb
    butt = [(HEEL + 0.006, -0.036, 0.0195, 0.060), (-0.250, -0.0266, 0.0200, 0.0506), (-0.160, -0.0171, 0.0190, 0.0411),
            (BODY0 + 0.008, -0.010, 0.0170, 0.032)]
    g.add("near", mg.loft_x([(x, 0.0, b + z, a, h) for (x, z, a, h) in butt], 8, "wood", e=2.8, cap0=False,
                            cap1=False))
    g.add("near", mg.loft_x([(HEEL, 0.0, b - 0.036, 0.0190, 0.0585), (HEEL + 0.006, 0.0, b - 0.036, 0.0205, 0.0610)],
                            8, "steel", e=2.8, cap1=False))
    g.add("near", mg.loft_path([np.array((HEEL + 0.008, -0.0125, b + 0.0265)), np.array((HEEL + 0.008, 0.0125,
                                                                                         b + 0.0265))],
                               [(0, 0, 1)] * 2, [(0.0048, 0.0048)] * 2, 5, "steel"))
    g.add("near", mg.box((HEEL + 0.010, -0.0120, b + 0.0225), (HEEL + 0.094, 0.0120, b + 0.0275), "steel"))
    # --- the receiver: slim and squared, its corners eased, narrowing at the back into the butt's neck
    g.add("near", mg.loft_x([(BODY0, 0.0, b - 0.008, 0.0170, 0.0270), (-0.074, 0.0, b - 0.005, A, 0.031),
                             (BODY1, 0.0, b - 0.005, A, 0.031)], 8, "steel", e=10.0))
    # (the trigger housing under its rear, over the guard)
    g.add("near", mg.box((-0.044, -0.0155, b - 0.045), (0.112, 0.0155, b - 0.034), "steel"))
    # (the safety on the right over the grip; the cocking handle on the right at the front, its knob, and the slot it
    # runs back in; the empty cases' port under the receiver's front, ahead of the housing)
    g.add("near", block((-0.030, A, b - 0.031), (0.004, A + 0.004, b - 0.023), "steel"))
    g.add("near", mg.box((0.164, A, b - 0.018), (0.186, A + 0.016, b - 0.006), "steel"))
    g.add("near", mg.loft_path([np.array((0.175, A + 0.016, b - 0.012)), np.array((0.175, A + 0.027, b - 0.012))],
                               [(0, 0, 1)] * 2, [(0.0068, 0.0068)] * 2, 6, "steel", cap0=False))
    g.add("near", mg.slot((0.115, A, b - 0.012), (0, 1, 0), (1, 0, 0), 0.058, 0.0028, n_end=2))
    g.add("near", mg.hole((0.165, 0.0, b + BODY_LOW), (0, 0, -1), (1, 0, 0), 0.030, 0.0085, n=6))
    # (the seam along either side where the trigger mechanism's frame slides into the receiver from behind)
    for side in (-1.0, 1.0):
        g.add("near", mg.slot((0.050, side * A, b - 0.021), (0, side, 0), (1, 0, 0), 0.112, 0.0013, n_end=1))
    # --- the pistol grip: wood, a little raked, under the receiver's back end
    gt = np.array((-0.024, 0.0, b - 0.040))
    ga = np.array((-0.30, 0.0, -0.954))
    ga /= np.linalg.norm(ga)
    g.add("near", mg.loft_path([gt, gt + ga * 0.055, gt + ga * 0.106], [(1, 0, 0.31)] * 3,
                               [(0.0145, 0.0192), (0.0156, 0.0208), (0.0146, 0.0198)], 8, "wood", e=2.6, cap0=False))
    g.grips["pistol"] = {"at": gt + ga * 0.055, "axis": ga}
    # --- the long trigger guard ahead of the grip, and the two triggers in it, one behind the other
    tg = [np.array((0.108, 0.0, b - 0.044)), np.array((0.102, 0.0, b - 0.080)), np.array((0.084, 0.0, b - 0.086)),
          np.array((0.004, 0.0, b - 0.085)), np.array((-0.013, 0.0, b - 0.066))]
    g.add("near", mg.loft_path(tg, [(0, 1, 0)] * len(tg), [(0.0032, 0.0055)] * len(tg), 4, "steel", cap0=False,
                               cap1=False))
    for x in (0.024, 0.066):
        g.add("near", mg.loft_path([np.array((x, 0.0, b - 0.045)), np.array((x - 0.002, 0.0, b - 0.061)),
                                    np.array((x - 0.009, 0.0, b - 0.072))], [(0, 1, 0)] * 3, [(0.0028, 0.0045)] * 3,
                                   4, "steel", cap0=False, cap1=False))
    # --- the magazine's housing on the receiver's top, the catch behind it, its dust cover laid open forward
    g.add("near", mg.box((MAG_X - 0.050, -0.0185, b + BODY_TOP - 0.001), (MAG_X + 0.048, 0.0185, b + HOUSING_TOP),
                         "steel"))
    g.add("near", mg.box((MAG_X - 0.061, -0.0075, b + BODY_TOP - 0.001), (MAG_X - 0.050, 0.0075, b + HOUSING_TOP + 0.013),
                         "steel"))
    g.add("near", mg.box((MAG_X + 0.048, -0.0165, b + BODY_TOP - 0.001), (BODY1 - 0.010, 0.0165, b + BODY_TOP + 0.0045),
                         "steel"))
    # --- the magazine: a straight flat-sided box leaning a little forward, its edges eased, a pressed rib down each
    # side, its floor plate a lip round its top. (Drawn from where it comes up out of the housing to the floor plate's
    # underside: a corner buried in the housing or the plate would shade the whole magazine as if shut in, near black,
    # where the mid and far magazines are lit like the receiver.)
    g.add("near", mg.slab_y(mag_outline(b, h1=MAG_H - 0.0085, z0=b + HOUSING_TOP), MAG_HY, "steel", ch=0.0025))
    for side in (-1.0, 1.0):
        mid = np.array(mag_point(b, -0.004, 0.5 * (HOUSING_TOP - MAG_FOOT + MAG_H)))
        g.add("near", mg.slot((mid[0], side * MAG_HY, mid[1]), (0, side, 0), up3, 0.055, 0.0030, n_end=2))
    lip = np.array(mag_point(b, 0.0, MAG_H - 0.004))
    g.add("near", mg.oriented_box((lip[0], 0.0, lip[1]), np.stack([fore3, (0.0, 1.0, 0.0), up3], 1),
                                  (MAG_W / 2 + 0.003, MAG_HY + 0.0015, 0.0045), "steel"))
    # --- the rear sight on the left: its bracket on the receiver's side, its base over the receiver's top edge, the
    # leaf standing up at the base's back with its aperture
    g.add("near", mg.box((-0.040, -A - 0.010, b + 0.004), (0.006, -A, b + BODY_TOP + 0.003), "steel"))
    g.add("near", mg.box((-0.044, SIGHT_Y - 0.0075, b + BODY_TOP + 0.003), (0.010, SIGHT_Y + 0.0075,
                                                                           b + BODY_TOP + 0.010), "steel"))
    g.add("near", mg.box((-0.037, SIGHT_Y - 0.0065, b + BODY_TOP + 0.010), (-0.032, SIGHT_Y + 0.0065,
                                                                           b + SIGHT_Z + 0.007), "steel"))
    g.add("near", mg.hole((-0.037, SIGHT_Y, b + SIGHT_Z), (-1, 0, 0), (0, 1, 0), 0.0028, 0.0028, n=6))
    # --- the barrel: its collar at the receiver's front, then plain and slim to the flash hider
    g.add("near", mg.loft_x([(BODY1, 0.0, b, 0.0170, 0.0170), (FORE0, 0.0, b, 0.0170, 0.0170)], 8, "steel",
                            cap0=False, cap1=False))
    g.add("near", mg.loft_x([(FORE0, 0.0, b, 0.0128, 0.0128), (GAS_END - 0.016, 0.0, b, 0.0121, 0.0121),
                             (HIDER0, 0.0, b, 0.0115, 0.0115)], 8, "steel", cap0=False, cap1=False))
    # --- the wooden fore-end under the barrel's rear part: a trough the barrel lies in, the gas cylinder inside it
    fore = [(FORE0, -0.0215, 0.0170, 0.0205), (FORE0 + 0.016, -0.0225, 0.0198, 0.0232),
            (FORE1, -0.0218, 0.0180, 0.0212)]
    g.add("near", mg.loft_x([(x, 0.0, b + z, a, h) for (x, z, a, h) in fore], 8, "wood", e=2.6))
    # --- the gas cylinder on from the fore-end to the gas block, the regulator on the block's front; the bipod's clamp
    # (the gas cylinder's ends stand on the fore-end's front and the block's back, not inside them, so it is shaded as
    # open steel and not as a hollow)
    g.add("near", mg.loft_x([(FORE1, 0.0, b + GAS_Z, GAS_R, GAS_R), (GAS_END - 0.016, 0.0, b + GAS_Z, GAS_R, GAS_R)], 6,
                            "steel", cap0=False, cap1=False))
    g.add("near", mg.box((GAS_END - 0.016, -0.0115, b + under - 0.002), (GAS_END + 0.008, 0.0115, b + 0.007), "steel"))
    g.add("near", mg.loft_x([(GAS_END + 0.008, 0.0, b + GAS_Z + 0.002, 0.0080, 0.0080),
                             (GAS_END + 0.022, 0.0, b + GAS_Z + 0.002, 0.0080, 0.0080)], 6, "steel", cap0=False))
    g.add("near", mg.loft_x([(HINGE_X - 0.014, 0.0, b + GAS_Z, GAS_R + 0.0035, GAS_R + 0.0035),
                             (HINGE_X + 0.014, 0.0, b + GAS_Z, GAS_R + 0.0035, GAS_R + 0.0035)], 8, "steel",
                            cap0=False, cap1=False))
    # --- the front sight: its band round the barrel ahead of the gas block, the arm up and out to the left, the
    # blade between its two ears
    fx0, fx1 = 0.628, 0.646
    fm = 0.5 * (fx0 + fx1)
    g.add("near", mg.loft_x([(fx0, 0.0, b, 0.0148, 0.0148), (fx1, 0.0, b, 0.0148, 0.0148)], 6, "steel", cap0=False,
                            cap1=False))
    g.add("near", mg.loft_path([np.array((fm, -0.007, b + 0.008)), np.array((fm, SIGHT_Y, b + 0.036))],
                               [(1, 0, 0)] * 2, [(0.0045 / s2, 0.009 / s2)] * 2, 4, "steel", cap0=False, cap1=False))
    g.add("near", block((fx0 + 0.002, SIGHT_Y - 0.0085, b + 0.034), (fx1 - 0.002, SIGHT_Y - 0.0058, b + SIGHT_Z + 0.005),
                        "steel"))
    g.add("near", block((fx0 + 0.002, SIGHT_Y + 0.0058, b + 0.034), (fx1 - 0.002, SIGHT_Y + 0.0085, b + SIGHT_Z + 0.005),
                        "steel"))
    g.add("near", block((fm - 0.004, SIGHT_Y - 0.0013, b + 0.034), (fm + 0.004, SIGHT_Y + 0.0013, b + SIGHT_Z), "steel"))
    # --- the flash hider: a collar, then a cone flaring to its mouth, the mouth dark
    g.add("near", mg.loft_x([(HIDER0 - 0.006, 0.0, b, 0.0145, 0.0145), (HIDER0 + 0.005, 0.0, b, 0.0145, 0.0145),
                             (HIDER0 + 0.008, 0.0, b, 0.0125, 0.0125), (MUZZLE, 0.0, b, 0.0200, 0.0200)], 8, "steel",
                            cap0=False, cap1=True, dark_caps=(False, True)))
    # --- the bipod on its clamp, the legs folded back under the gas cylinder and the fore-end
    mg.folded_bipod(g, "near", b, under)
    # --- where hands go: along the folded legs; the gun's underside from the butt's toe to the flash hider (the
    # butt's, the trigger housing's, the receiver's, the fore-end's, the folded legs, the clamp, the gas block and its
    # regulator, the barrel's); the butt's underside
    g.grips["legs"] = [(np.array((le, 0.0, b + under - 0.012)), np.array((HINGE_X - 0.020, 0.0, b + under - 0.008)))]
    line = [(HEEL + 0.012, b - 0.095), (-0.250, b - 0.077), (-0.160, b - 0.058), (BODY0 + 0.008, b - 0.042),
            (-0.044, b - 0.045), (0.112, b - 0.045), (0.116, b + BODY_LOW), (FORE0, b + BODY_LOW),
            (FORE0 + 0.016, b - 0.0457), (le, b + under - 0.012), (HINGE_X - 0.020, b + under - 0.008),
            (HINGE_X + 0.014, b + under - 0.008), (GAS_END - 0.016, b + under - 0.002),
            (GAS_END + 0.022, b + GAS_Z - 0.006), (fx0, b - 0.0148), (HIDER0, b - 0.0145), (MUZZLE, b - 0.020)]
    g.grips["under"] = [(np.array((x0, 0.0, z0)), np.array((x1, 0.0, z1))) for (x0, z0), (x1, z1) in zip(line, line[1:])]
    # (the butt's underside bends where it sweeps down: the same four points as the line under the gun, not one
    # straight line cutting 1 cm into the wood)
    g.grips["butt"] = g.grips["under"][:3]
    g.grips["keep_out"] = None                 # (fed from the top: nothing hangs under it or on its left)
    g.grips["hip_l_at"] = 0.300                # (from the hip: his left fist under the fore-end, round it and the
                                               # folded legs, as near him as his arm's reach wants)
    g.grips["kneel_l_at"] = 0.250              # (kneeling: under the fore-end's back end)
    g.grips["park"] = (0.050, 0.0, b - 0.005)  # (the middle of the receiver's solid)

    # ------------------------------------------------------------------------------------------- mid (LOD1) ----
    g.add("mid", mg.loft_x([(HEEL, 0.0, b - 0.036, 0.0195 / s2, 0.060 / s2),
                            (-0.200, 0.0, b - 0.0213, 0.0195 / s2, 0.0453 / s2),
                            (BODY0 + 0.008, 0.0, b - 0.010, 0.0170 / s2, 0.032 / s2)], 4, "wood", cap1=False,
                           phase=math.pi / 4))
    # (the receiver a box, its front closed on its own corners: no new points)
    body = mg.loft_x([(BODY0, 0.0, b - 0.005, A / s2, 0.031 / s2), (BODY1, 0.0, b - 0.005, A / s2, 0.031 / s2)], 4,
                     "steel", cap0=False, cap1=False, phase=math.pi / 4)[0]
    g.add("mid", end_fan(body, 4, 4, 1.0))
    # (the magazine keeps its flat sides and its lean, its foot cut level on the receiver's top)
    g.add("mid", mg.slab_y(mag_outline(b, z0=b + BODY_TOP), MAG_HY, "steel"))
    g.add("mid", mg.loft_path([gt - ga * 0.012, gt + ga * 0.106], [(1, 0, 0.31)] * 2, [(0.0148, 0.019)] * 2, 4, "wood",
                              cap0=False))
    g.add("mid", mg.loft_x([(BODY1, 0.0, b - 0.022, 0.0195 / s2, 0.0230 / s2),
                            (FORE1, 0.0, b - 0.022, 0.0175 / s2, 0.0210 / s2)], 4, "wood", cap0=False,
                           phase=math.pi / 4))
    # (the barrel and the flash hider one loft, the mouth dark; the gas cylinder)
    g.add("mid", mg.loft_x([(BODY1, 0.0, b, 0.0140, 0.0140), (HIDER0, 0.0, b, 0.0122, 0.0122),
                            (MUZZLE, 0.0, b, 0.0205, 0.0205)], 5, "steel", cap0=False, cap1=True,
                           dark_caps=(False, True)))
    g.add("mid", mg.loft_x([(FORE1, 0.0, b + GAS_Z, GAS_R * 1.2, GAS_R * 1.2),
                            (GAS_END + 0.020, 0.0, b + GAS_Z, GAS_R * 1.2, GAS_R * 1.2)], 3, "steel", cap0=False,
                           cap1=False, phase=-math.pi / 2))
    g.add("mid", block((fx0, SIGHT_Y - 0.006, b + 0.006), (fx1, SIGHT_Y + 0.006, b + SIGHT_Z), "steel"))
    g.add("mid", block((-0.044, SIGHT_Y - 0.007, b + 0.010), (0.006, -A + 0.004, b + SIGHT_Z + 0.004), "steel"))
    mg.folded_bipod(g, "mid", b, under)

    # ------------------------------------------------------------------------------------------- far (LOD2) ----
    # (boxes: the butt three-sided, flat on top at the comb's width and keeled under; the receiver; the magazine a
    # closed box leaning forward on top; the barrel, fore-end and gas cylinder one three-sided box; the grip)
    c3 = math.cos(math.pi / 6)
    g.add("far", mg.loft_x([(HEEL, 0.0, b - 0.036 + 0.060 / 3, 0.0195 / c3, 0.060 * 4 / 3),
                            (BODY0 + 0.015, 0.0, b - 0.011 + 0.032 / 3, 0.0170 / c3, 0.032 * 4 / 3)], 3, "wood",
                           cap0=False, cap1=False, phase=math.pi / 6))
    g.add("far", mg.loft_x([(BODY0, 0.0, b - 0.005, A / s2, 0.031 / s2), (BODY1, 0.0, b - 0.005, A / s2, 0.031 / s2)],
                           4, "steel", cap0=False, cap1=False, phase=math.pi / 4))
    m1 = np.array((foot[0], 0.0, foot[1])) + up3 * MAG_H
    # (its foot sunk so its back corner meets the receiver's top: no daylight under it)
    m0 = m1 - up3 * ((m1[2] - (b + BODY_TOP - MAG_W / 2 * math.sin(MAG_LEAN))) / up3[2])
    g.add("far", mg.loft_path([m0, m1], [(1, 0, 0)] * 2, [(MAG_HY / s2, MAG_W / 2 / s2)] * 2, 4, "steel", cap0=False,
                              cap1=False))
    lid = mg.Piece("steel")
    lid.quad(*[lid.pt(m1 + fore3 * u * MAG_W / 2 + np.array((0.0, v * MAG_HY, 0.0)))
               for (u, v) in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
    g.add("far", mg.orient(lid, normal=up3))
    g.add("far", mg.loft_x([(BODY1, 0.0, b - 0.016, 0.018, 0.031), (MUZZLE, 0.0, b - 0.003, 0.015, 0.019)], 3, "steel",
                           cap0=False, cap1=False, phase=math.pi / 2))
    # (the grip a flat blade seen from either side, each face its own points: it is in his fist)
    for side in (-1.0, 1.0):
        gp = mg.Piece("wood")
        dx = np.array((0.018, 0.0, 0.0))
        q = [gp.pt(p) for p in (gt - ga * 0.010 + dx, gt + ga * 0.104 + dx, gt + ga * 0.104 - dx, gt - ga * 0.010 - dx)]
        gp.quad(*q)
        g.add("far", mg.orient(gp, normal=(0.0, side, 0.0)))
    mg.folded_bipod(g, "far", b, under)

    # --- how it lies on the ground: on its right side, on the butt's right side and the cocking handle's knob (which
    # stands 2.5 cm off the receiver: laid flat on it the butt would float), its muzzle a little up; the magazine lies
    # along the ground beside it. The way into the ground in the gun's frame, and how far its lowest point stands off
    # the gun's origin along it.
    down = np.array((-(A + 0.027 - 0.0205), 0.175 - (HEEL + 0.006), 0.0))
    down /= np.linalg.norm(down)
    g.grips["laid"] = {"down": tuple(float(v) for v in down), "off": float((g.mesh("near")[0] @ down).max())}
    return g


def report():
    g = fm2429()
    for L in ("near", "mid", "far"):
        P, N, paint, bone, dark, T = g.mesh(L)
        print(f"fm2429 {L}: {len(P)} points, {len(T)} triangles, {int(dark.sum())} dark points, "
              f"x {P[:, 0].min():.3f} to {P[:, 0].max():.3f}, y {P[:, 1].min():.3f} to {P[:, 1].max():.3f}, "
              f"z {P[:, 2].min():.3f} to {P[:, 2].max():.3f}")


if __name__ == "__main__":
    report()
