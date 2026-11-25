"""The British section's light machine gun, the Bren (Mk I and II, .303, 1938 on): our own drawing to period
proportions, built from mg_guns.py's simple solids at the three levels of a soldier file, in the weapon bone's frame
(metres, x along the bore toward the muzzle, y to the gunner's right, z up, the origin where the rifleman's right fist
closes on his rifle's wrist).

What makes it a Bren and no other gun: the curved 30-round box magazine standing on TOP of the receiver and curving
forward, so the sights stand off to the LEFT of the gun; the deep boxy receiver; the plain barrel with the gas
cylinder under it, the gas block a hand's width behind the flash hider with the foresight standing on the same band
of the barrel, and the bipod on the gas cylinder's front just behind it; the carrying handle with its wooden grip
angled off to the left of the barrel; the conical flash hider. The butt, the pistol grip and the carrying handle's
grip are wood (the army's own rifle wood); everything else is steel. It feeds from the top, so nothing hangs under it
but the pistol grip.

    python3 mg_gun_bren.py      the gun's size at each level
"""
import math

import numpy as np

import mg_guns as mg

# ---- the gun's measures along it (1.156 m from the butt plate to the flash hider's mouth)
HEEL = -0.345                # (the butt plate's back: the rifle's heel was at -0.33)
MUZZLE = 0.811               # (the flash hider's mouth)
BODY0, BODY1 = -0.105, 0.245  # (the receiver, from the butt's socket to the barrel nut)
BODY_A = 0.0245              # (half across)
BODY_TOP, BODY_LOW = 0.028, -0.050   # (its top and its underside over the bore)
# (the magazine: 85 mm fore and aft, 30 mm across, standing on the receiver's top and curving forward about a centre
# ahead of it)
MAG_X = 0.130                # (the middle of its foot along the gun)
MAG_W, MAG_HY = 0.085, 0.015
MAG_R = 0.185                # (its middle line's radius: thirty rimmed rounds fan out to a tight curve)
MAG_SWEEP = 0.92             # (how far round it goes, radians: 21 cm up its back edge, 13 up its front)
# (the barrel, the gas cylinder under it, and the bipod on the gas cylinder's front)
GAS_Z, GAS_R = -0.034, 0.0105
GAS_END = 0.612              # (the gas block, where the gas cylinder meets the barrel: 20 cm behind the muzzle, the
                             # foresight on the same band of the barrel over it, a hand's width of plain barrel between
                             # it and the flash hider)
HINGE_X = 0.575              # (the bipod's sleeve on the gas cylinder's front end, just behind the gas block; its
                             # folded legs end 3.6 cm ahead of the receiver's front, 19 cm ahead of the trigger guard)
HIDER0 = 0.722               # (where the flash hider's cone begins)
# (the sights' line, off to the left of the magazine)
SIGHT_Y, SIGHT_Z = -0.029, 0.062


def block(lo, hi, paint, bone="weapon"):
    """A small box on eight points, each shared by the three faces that meet there (a third of a flat-faced box's
    points): for parts too small for their faces' own shading to be seen."""
    pc = mg.Piece(paint, bone)
    lo, hi = np.asarray(lo, float), np.asarray(hi, float)
    for x in (lo[0], hi[0]):
        for y in (lo[1], hi[1]):
            for z in (lo[2], hi[2]):
                pc.pt((x, y, z))
    for f in [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
        pc.quad(*f)
    return mg.orient(pc)


def mag_point(b, th, r):
    """A point of the magazine's side view: `th` round its curve from its foot, `r` from the curve's centre (which
    lies ahead of the magazine, level with the receiver's top)."""
    return (MAG_X + MAG_R - r * math.cos(th), b + BODY_TOP - 0.004 + r * math.sin(th))


def mag_outline(b, k, sweep=MAG_SWEEP):
    """The magazine's side view: up its back edge (the outside of the curve) in `k` steps, then down its front edge."""
    ths = [sweep * i / (k - 1) for i in range(k)]
    return ([mag_point(b, th, MAG_R + MAG_W / 2) for th in ths] +
            [mag_point(b, th, MAG_R - MAG_W / 2) for th in reversed(ths)])


def mag_rib(b, k, r, half, side, lift=0.0007):
    """A pressed rib down the magazine's side, drawn as a dark line following its curve at `r` from the curve's
    centre, `half` its half width, on the left (side -1) or the right (+1) face."""
    pc = mg.Piece("steel", dark=True)
    y = side * (MAG_HY + lift)
    ths = [0.10 + (MAG_SWEEP - 0.16) * i / (k - 1) for i in range(k)]
    row = []
    for th in ths:
        (x0, z0), (x1, z1) = mag_point(b, th, r + half), mag_point(b, th, r - half)
        row.append((pc.pt((x0, y, z0)), pc.pt((x1, y, z1))))
    for (a0, a1), (b0, b1) in zip(row, row[1:]):
        pc.quad(a0, b0, b1, a1)
    return mg.orient(pc, normal=(0.0, side, 0.0))


def bren():
    """The Bren light machine gun as a section's gunner carries it on its bipod: 1.16 m. From the butt forward: the
    steel butt plate with its shoulder strap folded forward along the comb; the wooden butt, its comb straight behind
    the receiver and its underside sweeping down to the toe; the deep boxy receiver, with the rear sight standing on
    its left side on its bracket and drum (the magazine is in the sights' way, so they are set off to the left), the
    wooden pistol grip, the trigger and its guard under it, the cocking handle and its slot on its right, the change
    lever, the trigger group's seam and the barrel nut's lever on its left; the curved 30-round box magazine standing
    in its opening on top, curving forward, a pressed rib down each side, the magazine catch behind it; the
    barrel, with the carrying handle's wooden grip angled off to its left; the gas cylinder under the barrel, joined
    to it at the gas block (its regulator on the block's front), the bipod on a sleeve round the gas cylinder's front
    just behind the block, its legs folded back under the gas cylinder; the front sight on its bracket to the left of
    the barrel, on the barrel's band over the gas block; a hand's width of plain barrel; the conical flash hider.
    Mid: the same big shapes (the magazine keeps its curve and flat sides, the carrying handle its wood). Far: boxes,
    the magazine a closed box leaning forward on the receiver."""
    g = mg.Gun("bren", "Bren")
    b = mg.BORE
    s2 = 1.0 / math.sqrt(2.0)                               # (a four-sided loft's sides stand this far in of its corners)
    g.paints = {}                                           # (steel and the rifle's wood only: no cartridge shows)
    g.muzzle = np.array((MUZZLE, 0.0, b))
    under = GAS_Z - GAS_R                                   # (the gas cylinder's underside over the bore)
    g.hinge = mg.hinge_of(b, under, HINGE_X)
    le = mg.leg_end(g.hinge)
    A = BODY_A

    # ------------------------------------------------------------------------------------------ near (LOD0) ----
    # --- the butt: wood, oval, its comb straight and its underside sweeping down to the toe; the steel butt plate, and
    # the shoulder strap hinged at the plate's top, folded forward on the comb
    butt = [(HEEL + 0.005, 0.0, b - 0.039, 0.0205, 0.063), (-0.270, 0.0, b - 0.033, 0.0215, 0.056),
            (-0.175, 0.0, b - 0.0225, 0.0200, 0.0445), (BODY0 + 0.010, 0.0, b - 0.012, 0.0180, 0.032)]
    g.add("near", mg.loft_x(butt, 10, "wood", e=2.8, cap0=False, cap1=False))
    g.add("near", mg.loft_x([(HEEL, 0.0, b - 0.039, 0.0200, 0.063), (HEEL + 0.006, 0.0, b - 0.039, 0.0215, 0.0645)],
                            8, "steel", e=2.8, cap1=False))
    g.add("near", mg.box((HEEL + 0.002, -0.011, b + 0.0205), (-0.262, 0.011, b + 0.0260), "steel"))
    # --- the receiver: a deep box, its corners eased, narrowing at the back to the butt's socket, its underside rising
    # at the front to the gas cylinder
    g.add("near", mg.loft_x([(BODY0, 0.0, b - 0.012, 0.0215, 0.033), (-0.030, 0.0, b - 0.011, A, 0.039),
                             (0.195, 0.0, b - 0.011, A, 0.039), (BODY1, 0.0, b - 0.009, A, 0.037)], 8, "steel",
                            e=10.0))
    # (the trigger housing under its rear)
    g.add("near", mg.box((-0.050, -0.0175, b - 0.055), (0.094, 0.0175, b - 0.048), "steel"))
    # (the magazine opening's rim on top, the magazine catch behind it, the opening's sliding cover ahead of it)
    g.add("near", mg.box((MAG_X - 0.049, -0.020, b + BODY_TOP - 0.001), (MAG_X + 0.049, 0.020, b + BODY_TOP + 0.007),
                         "steel"))
    g.add("near", mg.box((MAG_X - 0.064, -0.008, b + BODY_TOP - 0.001), (MAG_X - 0.049, 0.008, b + BODY_TOP + 0.022),
                         "steel"))
    g.add("near", mg.box((MAG_X + 0.049, -0.018, b + BODY_TOP - 0.001), (BODY1 - 0.012, 0.018, b + BODY_TOP + 0.003),
                         "steel"))
    # (the cocking handle on the right, at the front, and its slot along the side)
    g.add("near", mg.box((0.196, A, b - 0.034), (0.222, A + 0.021, b - 0.022), "steel"))
    g.add("near", mg.slot((0.130, A, b - 0.028), (0, 1, 0), (1, 0, 0), 0.062, 0.0035, n_end=2))
    # (on the left: the change lever over the trigger, and the seam along the receiver where the butt and trigger
    # group slide into it)
    g.add("near", mg.box((0.012, -A - 0.006, b - 0.040), (0.046, -A, b - 0.031), "steel"))
    g.add("near", mg.slot((0.050, -A, b - 0.024), (0, -1, 0), (1, 0, 0), 0.120, 0.0014, n_end=1))
    # (the barrel nut's lever on the left front)
    g.add("near", mg.box((BODY1 - 0.016, -A - 0.008, b - 0.012), (BODY1 - 0.004, -A, b + 0.030), "steel"))
    # --- the rear sight on the left: its bracket on the receiver's side, its drum, the leaf standing up with its
    # aperture
    g.add("near", mg.box((-0.082, -A - 0.012, b + 0.004), (-0.036, -A, b + BODY_TOP + 0.002), "steel"))
    g.add("near", mg.loft_path([np.array((-0.046, -A - 0.012, b + 0.016)), np.array((-0.046, -A - 0.024, b + 0.016))],
                               [(0, 0, 1)] * 2, [(0.009, 0.009)] * 2, 5, "steel", cap0=False))
    g.add("near", mg.box((-0.066, SIGHT_Y - 0.008, b + BODY_TOP + 0.002), (-0.061, SIGHT_Y + 0.008, b + SIGHT_Z + 0.011),
                         "steel"))
    g.add("near", mg.hole((-0.066, SIGHT_Y, b + SIGHT_Z), (-1, 0, 0), (0, 1, 0), 0.0036, 0.0036, n=6))
    # --- the magazine: flat-sided, curving forward, a pressed rib down each side
    g.add("near", mg.slab_y(mag_outline(b, 7), MAG_HY, "steel", middle=True))
    for side in (-1.0, 1.0):
        g.add("near", mag_rib(b, 6, MAG_R + 0.004, 0.0035, side))
    # --- the pistol grip: wood, raked back under the receiver; the trigger guard; the trigger
    gt = np.array((-0.016, 0.0, b - 0.050))
    ga = np.array((-0.36, 0.0, -0.933))
    ga /= np.linalg.norm(ga)
    g.add("near", mg.loft_path([gt, gt + ga * 0.054, gt + ga * 0.104], [(1, 0, 0.40)] * 3,
                               [(0.0140, 0.0180), (0.0152, 0.0195), (0.0142, 0.0185)], 8, "wood", e=2.6, cap0=False))
    tg = [np.array((0.086, 0.0, b - 0.053)), np.array((0.068, 0.0, b - 0.092)), np.array((0.027, 0.0, b - 0.092)),
          np.array((0.001, 0.0, b - 0.076))]
    g.add("near", mg.loft_path(tg, [(0, 1, 0)] * 4, [(0.0035, 0.006)] * 4, 3, "steel", cap0=False, cap1=False))
    g.add("near", mg.loft_path([np.array((0.046, 0.0, b - 0.054)), np.array((0.039, 0.0, b - 0.078))],
                               [(0, 1, 0)] * 2, [(0.003, 0.0045)] * 2, 3, "steel", cap0=False, cap1=False))
    g.grips["pistol"] = {"at": gt + ga * 0.052, "axis": ga}
    # --- the barrel: the barrel nut at the receiver's front, then plain to the flash hider, a little thinner forward
    g.add("near", mg.loft_x([(BODY1, 0.0, b, 0.0205, 0.0205), (BODY1 + 0.026, 0.0, b, 0.0205, 0.0205),
                             (BODY1 + 0.028, 0.0, b, 0.0155, 0.0155), (GAS_END, 0.0, b, 0.0138, 0.0138),
                             (HIDER0, 0.0, b, 0.0125, 0.0125)], 8, "steel", cap0=False, cap1=False))
    # (the carrying handle: a collar round the barrel ahead of the nut, an arm up and out to the left, the wooden grip
    # running forward from it, angled off to the left and up, clear of the sights' line)
    g.add("near", mg.loft_x([(0.283, 0.0, b, 0.0185, 0.0185), (0.301, 0.0, b, 0.0185, 0.0185)], 8, "steel",
                            cap0=False, cap1=False))
    h0 = np.array((0.318, -0.046, b + 0.030))
    h1 = np.array((0.424, -0.064, b + 0.046))
    g.add("near", mg.loft_path([np.array((0.292, -0.012, b + 0.010)), np.array((0.300, -0.034, b + 0.024)), h0],
                               [(0, 0, 1)] * 3, [(0.005, 0.005)] * 3, 3, "steel", cap0=False, cap1=False))
    g.add("near", mg.loft_path([h0, 0.5 * (h0 + h1), h1], [(0, 0, 1)] * 3,
                               [(0.0125, 0.0125), (0.0140, 0.0140), (0.0125, 0.0125)], 6, "wood"))
    # --- the gas cylinder under the barrel, the gas block joining them, the regulator on the block's front; the
    # bipod's sleeve round the gas cylinder
    g.add("near", mg.loft_x([(BODY1, 0.0, b + GAS_Z, GAS_R, GAS_R), (GAS_END, 0.0, b + GAS_Z, GAS_R, GAS_R)], 6,
                            "steel", cap0=False, cap1=False))
    g.add("near", mg.box((GAS_END - 0.016, -0.0125, b + under - 0.001), (GAS_END + 0.010, 0.0125, b + 0.006), "steel"))
    g.add("near", mg.loft_x([(GAS_END + 0.010, 0.0, b + GAS_Z + 0.004, 0.0085, 0.0085),
                             (GAS_END + 0.026, 0.0, b + GAS_Z + 0.004, 0.0085, 0.0085)], 5, "steel", cap0=False))
    g.add("near", mg.loft_x([(HINGE_X - 0.017, 0.0, b + GAS_Z, GAS_R + 0.0035, GAS_R + 0.0035),
                             (HINGE_X + 0.017, 0.0, b + GAS_Z, GAS_R + 0.0035, GAS_R + 0.0035)], 6, "steel",
                            cap0=False, cap1=False))
    # --- the front sight, off to the left on its bracket: a band round the barrel over the gas block (one fitting on
    # the real barrel), the arm up and out, the blade between its two guards
    fx0, fx1 = GAS_END - 0.014, GAS_END + 0.008
    g.add("near", mg.loft_x([(fx0, 0.0, b, 0.0160, 0.0160), (fx1, 0.0, b, 0.0160, 0.0160)], 6, "steel", cap0=False,
                            cap1=False))
    fm = 0.5 * (fx0 + fx1)
    g.add("near", mg.loft_path([np.array((fm, -0.007, b + 0.008)), np.array((fm, SIGHT_Y, b + 0.042))], [(1, 0, 0)] * 2,
                               [(0.0045 / s2, 0.011 / s2)] * 2, 4, "steel", cap0=False, cap1=False))
    g.add("near", block((fx0, SIGHT_Y - 0.008, b + 0.040), (fx1, SIGHT_Y - 0.0055, b + SIGHT_Z + 0.006), "steel"))
    g.add("near", block((fx0, SIGHT_Y + 0.0055, b + 0.040), (fx1, SIGHT_Y + 0.008, b + SIGHT_Z + 0.006), "steel"))
    g.add("near", block((fx0 + 0.007, SIGHT_Y - 0.0013, b + 0.040), (fx1 - 0.007, SIGHT_Y + 0.0013, b + SIGHT_Z),
                        "steel"))
    # --- the flash hider: a collar, then a cone flaring to its mouth, the mouth dark
    g.add("near", mg.loft_x([(HIDER0 - 0.008, 0.0, b, 0.0150, 0.0150), (HIDER0 + 0.006, 0.0, b, 0.0150, 0.0150),
                             (HIDER0 + 0.010, 0.0, b, 0.0130, 0.0130), (MUZZLE, 0.0, b, 0.0210, 0.0210)], 8, "steel",
                            cap0=False, cap1=True, dark_caps=(False, True)))
    # --- the bipod on the gas cylinder's sleeve, its legs folded back under the gas cylinder
    mg.folded_bipod(g, "near", b, under)
    # --- where hands go: along the folded legs; the gun's underside from the butt's toe to the flash hider (the
    # butt's, the trigger housing's, the receiver's, the folded legs, the bipod's hinge, the gas block and its
    # regulator, the barrel's); the butt's underside
    g.grips["legs"] = [(np.array((le, 0.0, b + under - 0.012)), np.array((HINGE_X - 0.020, 0.0, b + under - 0.008)))]
    line = [(HEEL + 0.015, b - 0.100), (-0.230, b - 0.074), (BODY0, b - 0.045), (-0.050, b - 0.055),
            (0.094, b - 0.055), (0.104, b + BODY_LOW), (0.195, b + BODY_LOW), (le, b + under - 0.012),
            (HINGE_X - 0.020, b + under - 0.008), (HINGE_X + 0.012, b + under - 0.008), (GAS_END + 0.010, b + under),
            (GAS_END + 0.026, b + GAS_Z - 0.0045), (GAS_END + 0.030, b - 0.0135), (HIDER0, b - 0.0125),
            (MUZZLE, b - 0.021)]
    g.grips["under"] = [(np.array((x0, 0.0, z0)), np.array((x1, 0.0, z1))) for (x0, z0), (x1, z1) in zip(line, line[1:])]
    g.grips["butt"] = [(np.array((HEEL + 0.015, 0.0, b - 0.100)), np.array((BODY0, 0.0, b - 0.045)))]
    g.grips["keep_out"] = None                 # (fed from the top: nothing hangs under it or on its left)
    g.grips["hip_l_at"] = 0.30                 # (from the hip: his left fist on the folded legs under the gas cylinder,
                                               # as near him as the legs allow: his arm is at its reach)
    g.grips["kneel_l_at"] = 0.19               # (kneeling with the gun up: his left fist under the receiver's front,
                                               # behind the folded legs' feet)
    g.grips["park"] = (0.060, 0.0, b - 0.012)  # (the middle of the receiver's solid)

    # ------------------------------------------------------------------------------------------- mid (LOD1) ----
    g.add("mid", mg.loft_x([(HEEL, 0.0, b - 0.039, 0.0205 / s2, 0.063 / s2),
                            (-0.230, 0.0, b - 0.027, 0.0205 / s2, 0.049 / s2),
                            (BODY0, 0.0, b - 0.012, 0.0180 / s2, 0.032 / s2)], 4, "wood", cap1=False,
                           phase=math.pi / 4))
    g.add("mid", mg.loft_x([(BODY0, 0.0, b - 0.011, A / s2, 0.039 / s2), (BODY1, 0.0, b - 0.011, A / s2, 0.039 / s2)],
                           4, "steel", phase=math.pi / 4))
    # (the magazine keeps its curve and its flat sides)
    g.add("mid", mg.slab_y(mag_outline(b, 4), MAG_HY, "steel"))
    g.add("mid", mg.loft_path([gt - ga * 0.010, gt + ga * 0.104], [(1, 0, 0.40)] * 2, [(0.0148, 0.019)] * 2, 4, "wood",
                              cap0=False))
    g.add("mid", mg.loft_x([(BODY1, 0.0, b, 0.0150, 0.0150), (HIDER0, 0.0, b, 0.0125, 0.0125)], 5, "steel",
                           cap0=False, cap1=False))
    g.add("mid", mg.loft_x([(HIDER0, 0.0, b, 0.0130, 0.0130), (MUZZLE, 0.0, b, 0.0210, 0.0210)], 6, "steel",
                           cap0=False, cap1=True, dark_caps=(False, True)))
    g.add("mid", mg.loft_x([(BODY1, 0.0, b + GAS_Z, GAS_R, GAS_R), (GAS_END, 0.0, b + GAS_Z + 0.006, GAS_R, 0.017)],
                           3, "steel", cap0=False, phase=math.pi / 2))
    # (the carrying handle's wooden grip, off to the left)
    g.add("mid", mg.loft_path([np.array((0.296, -0.014, b + 0.012)), h0, h1], [(0, 0, 1)] * 3,
                              [(0.005, 0.005), (0.013, 0.013), (0.013, 0.013)], 3, "wood", cap0=False))
    mg.folded_bipod(g, "mid", b, under)

    # ------------------------------------------------------------------------------------------- far (LOD2) ----
    # (boxes: the butt, the receiver, the magazine leaning forward on top, the barrel and gas cylinder as one, the grip)
    # (the butt three-sided: flat on top at the comb's width, its sides meeting in a keel along its underside, so it
    # keeps its depth seen from the side and its width seen from above; the two triangles it saves close the
    # magazine's top)
    c3 = math.cos(math.pi / 6)
    g.add("far", mg.loft_x([(HEEL, 0.0, b - 0.039 + 0.063 / 3, 0.0205 / c3, 0.063 * 4 / 3),
                            (BODY0 + 0.015, 0.0, b - 0.011 + 0.032 / 3, 0.0180 / c3, 0.032 * 4 / 3)], 3, "wood",
                           cap0=False, cap1=False, phase=math.pi / 6))
    g.add("far", mg.loft_x([(BODY0, 0.0, b - 0.011, A / s2, 0.039 / s2), (BODY1, 0.0, b - 0.011, A / s2, 0.039 / s2)],
                           4, "steel", cap0=False, cap1=False, phase=math.pi / 4))
    # (the magazine: a box from its foot's middle to its top's middle, its foot set down into the receiver so no
    # corner of it stands clear of the receiver's top, its top closed: the game looks down on him)
    m0 = np.array((MAG_X, 0.0, b + BODY_TOP - 0.004))
    m1 = np.array(mag_point(b, MAG_SWEEP, MAG_R))
    m1 = np.array((m1[0], 0.0, m1[1]))
    lean = (m1 - m0) / np.linalg.norm(m1 - m0)
    m0 = m0 - lean * 0.022
    g.add("far", mg.loft_path([m0, m1], [(1, 0, 0)] * 2, [(MAG_HY / s2, MAG_W / 2 / s2)] * 2, 4, "steel", cap0=False,
                              cap1=False))
    fore = np.cross((0.0, 1.0, 0.0), lean)                  # (across the magazine's top, toward the muzzle)
    lid = mg.Piece("steel")
    lid.quad(*[lid.pt(m1 + fore * u * MAG_W / 2 + np.array((0.0, v * MAG_HY, 0.0)))
               for (u, v) in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
    g.add("far", mg.orient(lid, normal=lean))
    g.add("far", mg.loft_x([(BODY1, 0.0, b - 0.012, 0.016, 0.030), (MUZZLE, 0.0, b - 0.002, 0.015, 0.019)], 3, "steel",
                           cap0=False, cap1=False, phase=math.pi / 2))
    # (the grip a flat blade seen from either side, each face its own points: it is in his fist)
    for side in (-1.0, 1.0):
        gp = mg.Piece("wood")
        q = [gp.pt(p) for p in (gt - ga * 0.010 + np.array((0.018, 0.0, 0.0)), gt + ga * 0.102 + np.array((0.018, 0.0, 0.0)),
                                gt + ga * 0.102 - np.array((0.018, 0.0, 0.0)), gt - ga * 0.010 - np.array((0.018, 0.0, 0.0)))]
        gp.quad(*q)
        g.add("far", mg.orient(gp, normal=(0.0, side, 0.0)))
    mg.folded_bipod(g, "far", b, under)
    return g


def report():
    g = bren()
    for L in ("near", "mid", "far"):
        P, N, paint, bone, dark, T = g.mesh(L)
        print(f"bren {L}: {len(P)} points, {len(T)} triangles, {int(dark.sum())} dark points, "
              f"x {P[:, 0].min():.3f} to {P[:, 0].max():.3f}, y {P[:, 1].min():.3f} to {P[:, 1].max():.3f}, "
              f"z {P[:, 2].min():.3f} to {P[:, 2].max():.3f}")


if __name__ == "__main__":
    report()
