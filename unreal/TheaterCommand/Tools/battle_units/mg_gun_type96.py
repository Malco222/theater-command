"""The Japanese section's light machine gun, the Type 96 (6.5 mm, 1936 on): our own drawing to period proportions,
built from mg_guns.py's simple solids at the three levels of a soldier file, in the weapon bone's frame (metres, x
along the bore toward the muzzle, y to the gunner's right, z up, the origin where the rifleman's right fist closes on
his rifle's wrist).

What makes it a Type 96 and no other gun: the short, sharply curved 30-round box magazine standing on TOP of the
receiver and curling forward over it like a banana, together with the carrying handle standing up on the barrel just
ahead of the receiver (a wooden grip on a steel bracket, leaning forward); and the barrel ribbed with cooling fins
from the receiver to the gas block, thick where the fins are and thin ahead of them. The rest: a slim wooden butt
with a steel shoulder plate, a full wooden pistol grip close under the butt's front, a slim gas tube under the
barrel, the bipod on the gas block near the muzzle, the sights set off to the left (the magazine stands in the
middle), a low bracket for a telescopic sight on the receiver's right (no telescope fitted), a conical flash hider.
The butt, the pistol grip and the carrying handle's grip are wood (the army's own rifle wood); everything else is
steel. It feeds from the top, so nothing hangs under it but the pistol grip. Slung, it rides on his back with its
right side to him, the magazine lying out sideways along his back; laid down, it lies on its right side.

    python3 mg_gun_type96.py      the gun's size at each level
"""
import math

import numpy as np

import mg_guns as mg

# ---- the gun's measures along it (1.07 m from the shoulder plate to the flash hider's mouth)
HEEL = -0.345                # (the shoulder plate's back: the rifle's heel was at -0.33)
BUTT_Z, BUTT_H, BUTT_A = -0.041, 0.057, 0.018    # (the butt at the heel: its middle under the bore, its half height
                                                 # and half width; slim, its comb dropping a little to the heel)
MUZZLE = 0.725               # (the flash hider's mouth)
BODY0, BODY1 = -0.075, 0.205  # (the receiver, from the butt's socket to where the barrel goes in)
BODY_A = 0.0220              # (its sides from the middle: a slim gun)
BODY_TOP, BODY_LOW = 0.030, -0.038   # (its top and its underside over the bore)
COCK = 0.011                 # (how far the cocking handle stands out of the receiver's right side)
# (the magazine: 78 mm fore and aft, 29 mm across, standing on the receiver's top and curling forward about a centre
# just ahead of it: tighter than the Bren's, and a shorter box)
MAG_X = 0.088                # (the middle of its foot along the gun)
MAG_W, MAG_HY = 0.078, 0.0145
MAG_R = 0.135                # (its middle line's radius)
MAG_SWEEP = 1.10             # (how far round it goes, radians: 19 cm up its back edge, 10 up its front)
# (the barrel: finned from the receiver to the gas block, plain from there to the flash hider)
FIN0, FIN1 = 0.205, 0.522    # (the finned length)
FIN_R0, FIN_R1 = 0.0200, 0.0175      # (the fins' rim from the bore, at the receiver and at the gas block)
BARREL_R = 0.0108            # (the plain barrel ahead of the fins)
# (the gas tube under the barrel, slim, and the gas block that joins them; the bipod on the gas block)
GAS_Z, GAS_R = -0.034, 0.0075
GAS_END = 0.556              # (the gas block's front)
HINGE_X = 0.540              # (the bipod's hinge under the gas block: its folded legs end 2 cm ahead of the
                             # receiver's front and lower than its underside, 15 cm ahead of the trigger guard)
HIDER0 = 0.652               # (where the flash hider begins)
# (the sights' line, off to the left of the magazine)
SIGHT_Y, SIGHT_Z = -0.0290, 0.058
# (the carrying handle's wooden grip: from its foot on the bracket to its top, leaning forward and a little left)
HANDLE0 = (0.246, -0.006, 0.050)
HANDLE1 = (0.322, -0.016, 0.106)


# ------------------------------------------------------------------------------------------------ small solids ----
def block(lo, hi, paint, bone="weapon"):
    """A small box on eight points, each shared by the three faces that meet there (a third of a flat-faced box's
    points): for parts too small for their faces' own shading to be seen."""
    pc = mg.Piece(paint, bone)
    lo, hi = np.asarray(lo, float), np.asarray(hi, float)
    ix = [pc.pt((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    for f in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        pc.quad(*[ix[k] for k in f])
    return mg.orient(pc)


def end_fan(side, first, n, facing):
    """Closes one end of a loft's sides on the ring's own points (`first` the ring's first point, n round it): a fan
    from one corner, facing along `facing`. No new points: the end shades round."""
    P = side.P
    facing = np.asarray(facing, float)
    for k in range(1, n - 1):
        a, b_, c = first, first + k, first + k + 1
        nn = np.cross(P[b_] - P[a], P[c] - P[a])
        side.tri(*((a, b_, c) if nn @ facing >= 0 else (a, c, b_)))
    return side


def blade(corners, paint, bone="weapon"):
    """A flat piece seen from both sides (far off: a grip in a fist, the carrying handle): the corners in order
    round it, each face on its own points so each has its own facing."""
    P = [np.asarray(p, float) for p in corners]
    nn = np.cross(P[1] - P[0], P[2] - P[0])
    out = []
    for sg in (1.0, -1.0):
        pc = mg.Piece(paint, bone)
        ix = [pc.pt(p) for p in P]
        for k in range(1, len(ix) - 1):
            pc.tri(ix[0], ix[k], ix[k + 1])
        out.append(mg.orient(pc, normal=nn * sg))
    return out


# ------------------------------------------------------------------------------------------------ the magazine ----
def mag_point(b, th, r):
    """A point of the magazine's side view: `th` round its curve from its foot, `r` from the curve's centre (which
    lies ahead of the magazine, level with the receiver's top)."""
    return (MAG_X + MAG_R - r * math.cos(th), b + BODY_TOP - 0.004 + r * math.sin(th))


def mag_outline(b, k, sweep=MAG_SWEEP, k_front=None):
    """The magazine's side view: up its back edge (the outside of the curve) in `k` steps, then down its front edge
    (in `k_front` steps, else as many: the inside of the curve, shorter, can do with fewer)."""
    ths = [sweep * i / (k - 1) for i in range(k)]
    kf = k if k_front is None else k_front
    thf = [sweep * i / (kf - 1) for i in range(kf)]
    return ([mag_point(b, th, MAG_R + MAG_W / 2) for th in ths] +
            [mag_point(b, th, MAG_R - MAG_W / 2) for th in reversed(thf)])


def mag_rib(b, k, r, half, side, lift=0.0007):
    """A pressed rib down the magazine's side, drawn as a dark line following its curve at `r` from the curve's
    centre, `half` its half width, on the left (side -1) or the right (+1) face."""
    pc = mg.Piece("steel", dark=True)
    y = side * (MAG_HY + lift)
    ths = [0.12 + (MAG_SWEEP - 0.20) * i / (k - 1) for i in range(k)]
    row = []
    for th in ths:
        (x0, z0), (x1, z1) = mag_point(b, th, r + half), mag_point(b, th, r - half)
        row.append((pc.pt((x0, y, z0)), pc.pt((x1, y, z1))))
    for (a0, a1), (b0, b1) in zip(row, row[1:]):
        pc.quad(a0, b0, b1, a1)
    return mg.orient(pc, normal=(0.0, side, 0.0))


# ------------------------------------------------------------------------------------------------ the fins ----
def fin_r(x):
    """The fins' rim from the bore at x along the barrel."""
    return FIN_R0 + (FIN_R1 - FIN_R0) * (x - FIN0) / (FIN1 - FIN0)


def fin_groove(b, x0, x1, n=8, phase=None, first=-1, flats=5, lift=0.0008):
    """The dark gap between two cooling fins, seen from outside: a dark band lying on the finned barrel's `n`-sided
    rim from x0 to x1, over `flats` of its flats from corner `first` on (the rim's corners stand at `phase` and every
    n-th of a turn on from it; the default is the near barrel's eight-sided rim and its top five flats: the gas tube
    and the folded legs hide the rest from below)."""
    pc = mg.Piece("steel", dark=True)
    phase = math.pi / n if phase is None else phase
    rows = []
    for x in (x0, x1):
        r = fin_r(x) + lift
        rows.append([pc.pt((x, r * math.cos(phase + 2 * math.pi * (first + k) / n),
                            b + r * math.sin(phase + 2 * math.pi * (first + k) / n))) for k in range(flats + 1)])
    P = pc.P
    for k in range(flats):
        for (a, b_, c) in ((rows[0][k], rows[1][k], rows[1][k + 1]), (rows[0][k], rows[1][k + 1], rows[0][k + 1])):
            m = (P[a] + P[b_] + P[c]) / 3
            nn = np.cross(P[b_] - P[a], P[c] - P[a])
            pc.tri(*((a, b_, c) if nn @ np.array((0.0, m[1], m[2] - b)) >= 0 else (a, c, b_)))
    return pc


# ---------------------------------------------------------------------------------------------------- the gun ----
def type96():
    """The Type 96 light machine gun as a section's gunner carries it on its bipod: 1.07 m. From the butt forward:
    the steel shoulder plate; the slim wooden butt, its comb dropping a little from the receiver to the heel and its
    underside rising from the toe to the pistol grip; the wooden pistol grip, full and raked, close under the butt's front, the trigger
    and its guard ahead of it; the slim receiver, with the rear sight on its left side (a bracket, the drum that
    sets it, the leaf standing up with its peep hole: the magazine is in the sights' way, so they are set off to the
    left), the low bracket for a telescopic sight along its right side (no telescope fitted), the way out for the
    spent cases under it and the cocking handle and its slot low on the right, the barrel's lock on its left front; the
    curved 30-round box magazine standing in its opening on top, curling forward, a pressed rib down each side, the
    magazine catch behind it; the barrel, ribbed with cooling fins from the receiver to the gas block (the dark gaps
    between the fins drawn on it), and plain from there on; the carrying handle on a steel band round the barrel's
    back end, its bracket standing up and its wooden grip leaning forward from it; the slim gas tube under the
    barrel, joined to it at the gas block, the regulator on the block's front; the bipod hinged under the gas block,
    its legs folded back under the gas tube; the front sight on its bracket to the left of the barrel, just ahead of
    the gas block, between its two guards; the conical flash hider.
    Mid: the same big shapes (the magazine keeps its curve and flat sides, the carrying handle its wood, the finned
    barrel its thickness and a few dark gaps). Far: boxes, the magazine a wedge fanning forward on the receiver (its
    back edge long, its front edge short, its top cut square to the curve's end), the carrying handle a wooden blade
    ahead of it."""
    g = mg.Gun("type96", "Type 96")
    b = mg.BORE
    s2 = 1.0 / math.sqrt(2.0)                               # (a four-sided loft's sides stand this far in of its
                                                            # corners)
    g.paints = {}                                           # (steel and the rifle's wood only: no cartridge shows)
    g.muzzle = np.array((MUZZLE, 0.0, b))
    under = GAS_Z - GAS_R                                   # (the gas tube's underside over the bore)
    g.hinge = mg.hinge_of(b, under, HINGE_X)
    le = mg.leg_end(g.hinge)
    A = BODY_A
    top = b + BODY_TOP
    f8 = math.cos(math.pi / 8) ** (2.0 / 8.0)               # (an eight-sided loft's flats stand this far in of its
                                                            # sizes, squared off by exponent 8: the receiver's)

    # ------------------------------------------------------------------------------------------ near (LOD0) ----
    # --- the butt: wood, slim, its comb straight and its underside rising from the toe to the grip; the steel
    # shoulder plate on its end
    butt = [(HEEL + 0.005, 0.0, b + BUTT_Z, BUTT_A, BUTT_H), (-0.250, 0.0, b - 0.0310, 0.0183, 0.0485),
            (-0.160, 0.0, b - 0.0200, 0.0176, 0.0400), (BODY0 + 0.017, 0.0, b - 0.0065, 0.0165, 0.0290)]
    g.add("near", mg.loft_x(butt, 8, "wood", e=3.0, cap0=False, cap1=False))
    g.add("near", mg.loft_x([(HEEL, 0.0, b + BUTT_Z, BUTT_A - 0.0005, BUTT_H),
                             (HEEL + 0.006, 0.0, b + BUTT_Z, BUTT_A + 0.001, BUTT_H + 0.0015)],
                            8, "steel", e=3.0, cap1=False))
    # --- the receiver: slim, its corners eased, narrowing at the back into the butt (the butt's front end stands
    # inside it, so it needs no end face there) and a little at the front to the barrel; the grip's frame under its
    # rear
    mid_z = b + 0.5 * (BODY_TOP + BODY_LOW)
    half = 0.5 * (BODY_TOP - BODY_LOW) / f8
    g.add("near", mg.loft_x([(BODY0, 0.0, mid_z - 0.002, 0.0130, 0.0240), (-0.045, 0.0, mid_z, A / f8, half),
                             (0.170, 0.0, mid_z, A / f8, half), (BODY1, 0.0, mid_z + 0.003, 0.0215, half - 0.003)],
                            8, "steel", e=8.0, cap0=False))
    g.add("near", mg.box((-0.060, -0.0160, b - 0.052), (0.088, 0.0160, b + BODY_LOW + 0.002), "steel"))
    # (the magazine opening's rim on top, and the magazine catch behind it)
    g.add("near", mg.box((MAG_X - 0.046, -0.0185, top - 0.001), (MAG_X + 0.046, 0.0185, top + 0.007), "steel"))
    g.add("near", block((MAG_X - 0.060, -0.007, top - 0.001), (MAG_X - 0.046, 0.007, top + 0.020), "steel"))
    # (on the right: the low bracket for the telescopic sight, a rail along the side with nothing on it; under it the
    # way out for the spent cases; under that the cocking handle, pushed forward, and the slot it runs back in)
    g.add("near", mg.box((-0.040, A - 0.001, b + 0.006), (0.052, A + 0.007, b + 0.022), "steel"))
    g.add("near", mg.slot((0.100, A, b - 0.008), (0, 1, 0), (1, 0, 0), 0.036, 0.0075, n_end=2))
    g.add("near", mg.box((0.146, A - 0.001, b - 0.028), (0.166, A + COCK, b - 0.016), "steel"))
    g.add("near", mg.slot((0.105, A, b - 0.022), (0, 1, 0), (1, 0, 0), 0.065, 0.0030, n_end=1))
    # (on the left at the front: the barrel's lock)
    g.add("near", block((BODY1 - 0.034, -A - 0.008, b - 0.012), (BODY1 - 0.016, -A + 0.001, b + 0.022), "steel"))
    # --- the rear sight on the left: its bracket on the receiver's side, the drum that sets it, the leaf standing up
    # with its peep hole
    g.add("near", mg.box((-0.062, -A - 0.011, b + 0.004), (0.004, -A + 0.001, top + 0.002), "steel"))
    g.add("near", mg.loft_path([np.array((-0.022, -A - 0.011, b + 0.016)), np.array((-0.022, -A - 0.024, b + 0.016))],
                               [(0, 0, 1)] * 2, [(0.009, 0.009)] * 2, 5, "steel", cap0=False))
    g.add("near", mg.box((-0.058, SIGHT_Y - 0.008, top + 0.002), (-0.053, SIGHT_Y + 0.008, b + SIGHT_Z + 0.010),
                         "steel"))
    g.add("near", mg.hole((-0.058, SIGHT_Y, b + SIGHT_Z), (-1, 0, 0), (0, 1, 0), 0.0036, 0.0036, n=6))
    # --- the magazine: flat-sided, curling forward, a pressed rib down each side
    g.add("near", mg.slab_y(mag_outline(b, 7), MAG_HY, "steel", middle=True))
    for side in (-1.0, 1.0):
        g.add("near", mag_rib(b, 6, MAG_R + 0.006, 0.0035, side))
    # --- the pistol grip: wood, full and raked back, close under the butt's front; the trigger guard; the trigger
    gt = np.array((-0.024, 0.0, b - 0.050))
    ga = np.array((-0.31, 0.0, -0.951))
    ga /= np.linalg.norm(ga)
    g.add("near", mg.loft_path([gt, gt + ga * 0.056, gt + ga * 0.110], [(1, 0, 0.33)] * 3,
                               [(0.0140, 0.0190), (0.0155, 0.0210), (0.0160, 0.0220)], 8, "wood", e=2.6, cap0=False))
    tg = [np.array((0.064, 0.0, b - 0.051)), np.array((0.054, 0.0, b - 0.086)), np.array((0.018, 0.0, b - 0.090)),
          np.array((-0.011, 0.0, b - 0.078))]
    g.add("near", mg.loft_path(tg, [(0, 1, 0)] * 4, [(0.0035, 0.006)] * 4, 3, "steel", cap0=False, cap1=False))
    g.add("near", mg.loft_path([np.array((0.031, 0.0, b - 0.051)), np.array((0.023, 0.0, b - 0.075))],
                               [(0, 1, 0)] * 2, [(0.003, 0.0045)] * 2, 3, "steel", cap0=False, cap1=False))
    g.grips["pistol"] = {"at": gt + ga * 0.056, "axis": ga}
    # --- the barrel: finned from the receiver to the gas block (its rim drawn, the gaps between the fins dark bands
    # on it), then plain to the flash hider
    g.add("near", mg.loft_x([(FIN0, 0.0, b, FIN_R0, FIN_R0), (FIN1, 0.0, b, FIN_R1, FIN_R1)], 8, "steel",
                            cap0=False))
    for k in range(10):
        x = 0.247 + 0.027 * k
        g.add("near", fin_groove(b, x, x + 0.011))
    g.add("near", mg.loft_x([(FIN1, 0.0, b, BARREL_R + 0.0008, BARREL_R + 0.0008),
                             (HIDER0 + 0.004, 0.0, b, BARREL_R, BARREL_R)], 6, "steel", cap0=False, cap1=False))
    # (the carrying handle: a steel band round the barrel's back end, the bracket standing up from it, the wooden grip
    # leaning forward and a little to the left from the bracket's top)
    g.add("near", mg.loft_x([(0.212, 0.0, b, 0.0228, 0.0228), (0.234, 0.0, b, 0.0228, 0.0228)], 8, "steel",
                            cap0=False, cap1=False))
    h0 = np.array((HANDLE0[0], HANDLE0[1], b + HANDLE0[2]))
    h1 = np.array((HANDLE1[0], HANDLE1[1], b + HANDLE1[2]))
    g.add("near", mg.loft_path([np.array((0.222, 0.0, b + 0.018)), np.array((0.229, -0.003, b + 0.037)), h0],
                               [(0, 1, 0)] * 3, [(0.0095, 0.0060), (0.0080, 0.0055), (0.0065, 0.0055)], 4, "steel",
                               cap0=False, cap1=False))
    hd = (h1 - h0) / np.linalg.norm(h1 - h0)
    g.add("near", mg.loft_path([h0, 0.5 * (h0 + h1), h1], [(0, 1, 0)] * 3,
                               [(0.0120, 0.0120), (0.0140, 0.0140), (0.0125, 0.0125)], 6, "wood", cap1=False))
    # (the grip's top end: the steel washer and nut that hold it on the bracket's rod)
    nut = mg.Piece("steel")
    nc = nut.pt(h1 + hd * 0.003)
    across = np.cross((0.0, 1.0, 0.0), hd)
    across /= np.linalg.norm(across)
    up_ = np.cross(hd, across)
    nr = [nut.pt(h1 + 0.0125 * (across * math.cos(math.pi / 6 + k * math.pi / 3) +
                               up_ * math.sin(math.pi / 6 + k * math.pi / 3))) for k in range(6)]
    for k in range(6):
        nut.tri(nc, nr[k], nr[(k + 1) % 6])
    g.add("near", mg.orient(nut, normal=hd))
    # --- the gas tube under the barrel, slim; the gas block joining them, the regulator on the block's front
    g.add("near", mg.loft_x([(BODY1 - 0.004, 0.0, b + GAS_Z, GAS_R, GAS_R),
                             (GAS_END - 0.030, 0.0, b + GAS_Z, GAS_R, GAS_R)], 6, "steel", cap0=False, cap1=False))
    g.add("near", mg.box((GAS_END - 0.032, -0.0120, b + under - 0.002), (GAS_END, 0.0120, b + 0.005), "steel"))
    g.add("near", mg.loft_x([(GAS_END, 0.0, b + GAS_Z + 0.003, 0.0080, 0.0080),
                             (GAS_END + 0.014, 0.0, b + GAS_Z + 0.003, 0.0080, 0.0080)], 5, "steel", cap0=False))
    # --- the front sight, off to the left on its bracket: an arm up and out from the gas block's top, the blade
    # between its two guards
    fx0, fx1 = GAS_END - 0.024, GAS_END - 0.006
    fm = 0.5 * (fx0 + fx1)
    g.add("near", mg.loft_path([np.array((fm, -0.006, b + 0.002)), np.array((fm, SIGHT_Y, b + 0.040))],
                               [(1, 0, 0)] * 2, [(0.0045 / s2, 0.009 / s2)] * 2, 4, "steel", cap0=False, cap1=False))
    g.add("near", block((fx0, SIGHT_Y - 0.0080, b + 0.038), (fx1, SIGHT_Y - 0.0055, b + SIGHT_Z + 0.006), "steel"))
    g.add("near", block((fx0, SIGHT_Y + 0.0055, b + 0.038), (fx1, SIGHT_Y + 0.0080, b + SIGHT_Z + 0.006), "steel"))
    g.add("near", block((fx0 + 0.006, SIGHT_Y - 0.0013, b + 0.038), (fx1 - 0.006, SIGHT_Y + 0.0013, b + SIGHT_Z),
                        "steel"))
    # --- the flash hider: a collar on the muzzle, then a long cone flaring to its mouth, the mouth dark
    g.add("near", mg.loft_x([(HIDER0, 0.0, b, 0.0138, 0.0138), (HIDER0 + 0.014, 0.0, b, 0.0138, 0.0138),
                             (MUZZLE, 0.0, b, 0.0225, 0.0225)], 8, "steel", cap0=False, cap1=True,
                            dark_caps=(False, True)))
    # --- the bipod under the gas block, its legs folded back under the gas tube
    mg.folded_bipod(g, "near", b, under)
    # --- where hands go: along the folded legs; the gun's underside from the butt's toe to the flash hider (the
    # butt's, the grip's frame's, the receiver's, the folded legs, the bipod's hinge, the gas block and its
    # regulator, the barrel's, the flash hider's); the butt's underside
    g.grips["legs"] = [(np.array((le, 0.0, b + under - 0.012)),
                        np.array((HINGE_X - 0.020, 0.0, b + under - 0.008)))]
    line = [(HEEL + 0.012, b - 0.094), (-0.250, b - 0.077), (-0.160, b - 0.058), (BODY0, b - 0.038),
            (-0.060, b - 0.052), (0.088, b - 0.052), (0.098, b + BODY_LOW), (BODY1, b + BODY_LOW + 0.004),
            (le, b + under - 0.012), (HINGE_X - 0.020, b + under - 0.008), (HINGE_X + 0.012, b + under - 0.008),
            (GAS_END, b + under - 0.002), (GAS_END + 0.014, b + GAS_Z - 0.005), (GAS_END + 0.018, b - 0.0116),
            (HIDER0, b - 0.0138), (MUZZLE, b - 0.0225)]
    g.grips["under"] = [(np.array((x0, 0.0, z0)), np.array((x1, 0.0, z1)))
                        for (x0, z0), (x1, z1) in zip(line, line[1:])]
    g.grips["butt"] = [(np.array((HEEL + 0.012, 0.0, b - 0.094)), np.array((BODY0, 0.0, b - 0.038)))]
    g.grips["keep_out"] = None                 # (fed from the top: nothing hangs under it or on its left)
    g.grips["hip_l_at"] = 0.32                 # (from the hip: his left fist on the folded legs under the gas tube)
    g.grips["kneel_l_at"] = 0.20               # (kneeling: his left fist under the receiver's front end, ahead of the
                                               # trigger guard)
    g.grips["park"] = (0.050, 0.0, b - 0.004)  # (the middle of the receiver's solid)
    # (how it rides and lies: on his back with its right side to him, the magazine standing out sideways along his
    # back, the cocking handle's knob the furthest the right side stands off the bore; on the ground on its right
    # side, the magazine and the grip lying out flat either way, resting on the knob and the telescope's rail. The
    # direction of the gun's own that points to his back, or into the ground, and how far that side stands off it)
    g.grips["slung"] = {"to_back": (0.0, 1.0, 0.0), "off": A + COCK}
    g.grips["laid"] = {"down": (0.0, 1.0, 0.0), "off": A + COCK}

    # ------------------------------------------------------------------------------------------- mid (LOD1) ----
    # (the butt, the receiver, the grip, the finned barrel, the gas tube and the handle each closed at its seen ends
    # on its own points)
    wood = mg.loft_x([(HEEL, 0.0, b + BUTT_Z, BUTT_A / s2, (BUTT_H - 0.002) / s2),
                      (BODY0, 0.0, b - 0.0080, 0.0172 / s2, 0.0320 / s2)], 4, "wood", cap0=False, cap1=False,
                     phase=math.pi / 4)[0]
    g.add("mid", end_fan(wood, 0, 4, (-1.0, 0.0, 0.0)))
    body = mg.loft_x([(BODY0, 0.0, mid_z, A / s2, 0.034 / s2), (BODY1, 0.0, mid_z, A / s2, 0.034 / s2)], 4, "steel",
                     cap0=False, cap1=False, phase=math.pi / 4)[0]
    end_fan(body, 0, 4, (-1.0, 0.0, 0.0))
    g.add("mid", end_fan(body, 4, 4, (1.0, 0.0, 0.0)))
    # (the magazine keeps its curve and its flat sides)
    g.add("mid", mg.slab_y(mag_outline(b, 5, k_front=4), MAG_HY, "steel"))
    grip = mg.loft_path([gt - ga * 0.010, gt + ga * 0.110], [(1, 0, 0.33)] * 2, [(0.0150, 0.0205)] * 2, 4, "wood",
                        cap0=False, cap1=False)[0]
    g.add("mid", end_fan(grip, 4, 4, ga))
    # (the finned barrel thick, five-sided with a corner on top, four of the gaps between its fins dark on its two top
    # flats; the plain barrel thin; the flash hider's cone, four-sided)
    fins = mg.loft_x([(FIN0, 0.0, b, FIN_R0, FIN_R0), (FIN1, 0.0, b, FIN_R1, FIN_R1)], 5, "steel", cap0=False,
                     cap1=False, phase=math.pi / 2)[0]
    g.add("mid", end_fan(fins, 5, 5, (1.0, 0.0, 0.0)))
    for k in range(4):
        x = 0.262 + 0.064 * k
        g.add("mid", fin_groove(b, x, x + 0.030, n=5, phase=math.pi / 2, first=-1, flats=2, lift=0.0012))
    g.add("mid", mg.loft_x([(FIN1, 0.0, b, BARREL_R + 0.001, BARREL_R + 0.001),
                            (HIDER0 + 0.010, 0.0, b, BARREL_R + 0.001, BARREL_R + 0.001)], 3, "steel", cap0=False,
                           cap1=False, phase=math.pi / 2))
    g.add("mid", mg.loft_x([(HIDER0 + 0.010, 0.0, b, 0.0130, 0.0130), (MUZZLE, 0.0, b, 0.0225, 0.0225)], 4, "steel",
                           cap0=False, cap1=True, dark_caps=(False, True), phase=0.0))
    # (the gas tube, rising at its front into the gas block)
    gas = mg.loft_x([(BODY1, 0.0, b + GAS_Z, GAS_R * 1.2, GAS_R * 1.2),
                     (GAS_END, 0.0, b + GAS_Z + 0.008, GAS_R * 1.4, 0.019)], 3, "steel", cap0=False, cap1=False,
                    phase=math.pi / 2)[0]
    g.add("mid", end_fan(gas, 3, 3, (1.0, 0.0, 0.0)))
    # (the carrying handle's wooden grip standing on the barrel)
    hand = mg.loft_path([np.array((0.224, 0.0, b + 0.016)), h0, h1], [(0, 1, 0)] * 3,
                        [(0.006, 0.006), (0.0135, 0.0135), (0.0135, 0.0135)], 3, "wood", cap0=False, cap1=False)[0]
    g.add("mid", end_fan(hand, 6, 3, h1 - h0))
    mg.folded_bipod(g, "mid", b, under)

    # ------------------------------------------------------------------------------------------- far (LOD2) ----
    # (boxes: the butt and the receiver three-sided, each flat on top with its sides meeting in a keel along its
    # underside, so each keeps its depth seen from the side and its width seen from above; the magazine a wedge
    # fanning forward on the receiver; the barrel three-sided, thick to thin; the grip and the carrying handle flat
    # blades)
    c3 = math.cos(math.pi / 6)
    wood = mg.loft_x([(HEEL, 0.0, b + BUTT_Z + (BUTT_H - 0.002) / 3, BUTT_A / c3, (BUTT_H - 0.002) * 4 / 3),
                      (BODY0, 0.0, mid_z + 0.034 / 3, A / c3, 0.034 * 4 / 3)], 3, "wood", cap0=False,
                     cap1=False, phase=math.pi / 6)[0]
    g.add("far", end_fan(wood, 0, 3, (-1.0, 0.0, 0.0)))     # (its end closed on its own points; its front the
                                                            # receiver's own section, so the two join with no gap)
    g.add("far", mg.loft_x([(BODY0, 0.0, mid_z + 0.034 / 3, A / c3, 0.034 * 4 / 3),
                            (BODY1, 0.0, mid_z + 0.034 / 3, A / c3, 0.034 * 4 / 3)], 3, "steel",
                           cap0=False, cap1=False, phase=math.pi / 6))
    # (the magazine: a wedge on the box's own eight points, its foot level in the receiver's top, its top end cut
    # square to the curve's end, its back edge (the outside of the curve) long and its front edge short, so it keeps
    # the curve's forward fan seen from the side where a straight leaning box would pass for the Bren's; its top
    # closed, the game looking down on him, its foot open inside the receiver)
    mag = mg.Piece("steel")
    ends = [mag_point(b, 0.0, MAG_R + MAG_W / 2), mag_point(b, MAG_SWEEP, MAG_R + MAG_W / 2),
            mag_point(b, MAG_SWEEP, MAG_R - MAG_W / 2), mag_point(b, 0.0, MAG_R - MAG_W / 2)]
    mp = [[mag.pt((x, y, z)) for y in (-MAG_HY, MAG_HY)] for (x, z) in ends]
    mag.quad(*[mp[k][0] for k in range(4)])                 # (its two flat sides)
    mag.quad(*[mp[k][1] for k in range(4)])
    for k in range(3):                                      # (its back edge, its top, its front edge)
        mag.quad(mp[k][0], mp[k + 1][0], mp[k + 1][1], mp[k][1])
    g.add("far", mg.orient(mag))
    g.add("far", mg.loft_x([(BODY1, 0.0, b - 0.008, 0.021, 0.030), (MUZZLE, 0.0, b - 0.002, 0.016, 0.019)], 3,
                           "steel", cap0=False, cap1=False, phase=math.pi / 2))
    g.add("far", blade([gt - ga * 0.010 + np.array((0.019, 0.0, 0.0)), gt + ga * 0.108,
                        gt - ga * 0.010 - np.array((0.019, 0.0, 0.0))], "wood"))
    # (the handle's blade leans to the left as the grip does, its foot a little right of the middle, so it shows from
    # above and from behind as well as from the side)
    g.add("far", blade([np.array((0.214, 0.012, b + 0.016)), np.array((0.252, 0.012, b + 0.016)),
                        h1 + np.array((0.0, -0.006, 0.0))], "wood"))
    mg.folded_bipod(g, "far", b, under)
    return g


def report():
    g = type96()
    for L in ("near", "mid", "far"):
        P, N, paint, bone, dark, T = g.mesh(L)
        print(f"type96 {L}: {len(P)} points, {len(T)} triangles, {int(dark.sum())} dark points, "
              f"x {P[:, 0].min():.3f} to {P[:, 0].max():.3f}, y {P[:, 1].min():.3f} to {P[:, 1].max():.3f}, "
              f"z {P[:, 2].min():.3f} to {P[:, 2].max():.3f}")


if __name__ == "__main__":
    report()
