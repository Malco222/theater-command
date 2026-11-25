"""The Italian section's light machine gun, the Breda modello 30 (6.5 mm, 1930 on): our own drawing to period
proportions, built from mg_guns.py's simple solids at the three levels of a soldier file, in the weapon bone's frame
(metres, x along the bore toward the muzzle, y to the gunner's right, z up, the origin where the rifleman's right fist
closes on his rifle's wrist).

What makes it a Breda and no other gun: the long barrel covered in close cooling fins, ring after ring from a hand's
width ahead of the receiver almost to the small flash hider; the odd tall receiver, a plain slab-sided box with a flat
top, longer than any other light gun's; and the magazine, a flat box fixed to the receiver's RIGHT side, standing out
level like a paddle (it is never taken off: it is unlatched, swung forward on its hinge and filled from a charger).
The butt and the pistol grip are wood (the army's own rifle wood), the butt a flat-sided angular slab hung lower than
the receiver's top; everything else is steel. Nothing hangs under the gun but the pistol grip and the folded bipod.

    python3 mg_gun_breda30.py      the gun's size at each level
"""
import math

import numpy as np

import mg_guns as mg

# ---- the gun's measures along it (1.230 m from the butt plate to the flash hider's mouth)
HEEL = -0.345                # (the butt plate's back: the rifle's heel was at -0.33)
MUZZLE = 0.885               # (the flash hider's mouth)
BODY0, BODY1 = -0.086, 0.330  # (the receiver, from the butt to the barrel's bearing: a long box, a third of the gun)
BODY_A = 0.026               # (half across)
BODY_TOP, BODY_LOW = 0.036, -0.048   # (its flat top and its underside over the bore)
STEP_X, FORE_LOW = 0.150, -0.032     # (ahead of the trigger group its underside steps up: the fore part is shallower)
# (the magazine: a flat box on the receiver's right side, just behind the breech; twenty rounds lie in it along the
# bore, side by side outward, so it is as long fore and aft as a cartridge, shallow, and stands a hand and a half out)
MAG_X = 0.272                # (the middle of its root along the gun: its front edge at the breech)
MAG_W = 0.088                # (fore and aft)
MAG_T = 0.030                # (deep)
MAG_L = 0.168                # (out from the receiver's side)
MAG_Z = 0.004                # (its middle over the bore: it feeds level into the breech)
MAG_RAKE = math.radians(15.0)   # (latched to fire it stands out to the right, its outer end a little forward)
# (the barrel: its bearing at the receiver's front, the plain sleeve, the fins, the plain muzzle, the flash hider)
NUT1 = 0.356                 # (the bearing's front)
NUT_R = 0.0265               # (the bearing's ring nut, middle to corner: as much as the receiver's half width)
SLEEVE_R = 0.0165
FIN0, FIN1 = 0.440, 0.792    # (the finned length: two thirds of the barrel that shows)
FIN_R = 0.0200               # (a fin's radius; the barrel between them 0.0115)
CORE_R = 0.0115
FIN_STEP, FIN_T = 0.0180, 0.0055     # (near: a fin every 18 mm, 5.5 mm thick: the real ones are closer and thinner
                                     # than a file's triangles allow)
HINGE_X = 0.698              # (the bipod's clamp round the barrel, two thirds along it; its folded legs' feet lie
                             # under the sleeve, 7 cm ahead of the receiver)
CLAMP = 0.017                # (half the clamp's length)
HIDER0 = 0.842               # (where the flash hider begins)
SIGHT_X = 0.818              # (the front sight, on the plain muzzle)


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


def prism(profile, y0, y1, paint, bone="weapon"):
    """A flat-sided solid from its side view `profile` [(x, z)...] (its corners in order round it; one corner must see
    all the others) between y0 and y1: every face flat, on its own points (a machined block: no edge shades round)."""
    O = [tuple(p) for p in profile]
    n = len(O)
    area = 0.5 * sum(O[k][0] * O[(k + 1) % n][1] - O[(k + 1) % n][0] * O[k][1] for k in range(n))
    if area < 0:
        O = O[::-1]

    def sees(v):
        for k in range(1, n - 1):
            (ax, az), (bx, bz), (cx, cz) = O[v], O[(v + k) % n], O[(v + k + 1) % n]
            if (bx - ax) * (cz - az) - (bz - az) * (cx - ax) <= 1e-12:
                return False
        return True
    v = next(k for k in range(n) if sees(k))
    out = []
    for y, sg in ((y0, -1.0), (y1, 1.0)):
        out.append(face([(O[(v + k) % n][0], y, O[(v + k) % n][1]) for k in range(n)], (0.0, sg, 0.0), paint, bone))
    for k in range(n):
        (xa, za), (xb, zb) = O[k], O[(k + 1) % n]
        out.append(face([(xa, y0, za), (xb, y0, zb), (xb, y1, zb), (xa, y1, za)], (zb - za, 0.0, xa - xb), paint, bone))
    return out


def face(corners, normal, paint, bone="weapon", dark=False):
    """A flat one-sided face on its own points (the corners in order round it), seen from the side `normal` points to:
    an end's cap in two triangles where a fan from its middle would take four."""
    pc = mg.Piece(paint, bone, dark)
    q = [pc.pt(p) for p in corners]
    for k in range(1, len(q) - 1):
        pc.tri(q[0], q[k], q[k + 1])
    return mg.orient(pc, normal=normal)


def close_ring(pc, ring_, normal):
    """Close a loft's end on its own points `ring_` (in order round it): a fan from the first, seen from the side
    `normal` points to."""
    P = np.array(pc.P)
    normal = np.asarray(normal, float)
    for k in range(1, len(ring_) - 1):
        t = (ring_[0], ring_[k], ring_[k + 1])
        nn = np.cross(P[t[1]] - P[t[0]], P[t[2]] - P[t[0]])
        pc.T.append(t if nn @ normal >= 0 else (t[0], t[2], t[1]))
    return pc


def close_end(pc, x, sg):
    """Close an x loft's open end at `x` on the loft's own points (no new ones), seen from the side `sg` (-1 from
    behind, +1 from the front)."""
    P = np.array(pc.P)
    end = [k for k in range(len(P)) if abs(P[k][0] - x) < 1e-9]
    cy, cz = P[end, 1].mean(), P[end, 2].mean()
    end.sort(key=lambda k: math.atan2(P[k][2] - cz, P[k][1] - cy))
    return close_ring(pc, end, (sg, 0.0, 0.0))


def disc(x0, x1, r, z, n, paint="steel", flat=False, dark=False, phase=None):
    """A short round along the bore from x0 to x1, `n` sides, radius `r`, its middle `z` up: a cooling fin, a collar.
    Its rim and both its faces (each face a fan from one corner). `flat`: each face on its own points, so the faces
    are lit as flat discs and the rim round; else all on the rim's points (half the points, the edges shaded round)."""
    if phase is None:
        phase = math.pi / n
    rr = mg.ring(n, r, r, 2.0, phase)
    rim = mg.Piece(paint, "weapon", dark)
    a = [rim.pt((x0, u, z + v)) for (u, v) in rr]
    c = [rim.pt((x1, u, z + v)) for (u, v) in rr]
    for k in range(n):
        rim.quad(a[k], c[k], c[(k + 1) % n], a[(k + 1) % n])
    mg.orient(rim, axis="x")
    if flat:
        out = [rim]
        for x, sg in ((x0, -1.0), (x1, 1.0)):
            out.append(face([(x, u, z + v) for (u, v) in rr], (sg, 0.0, 0.0), paint, dark=dark))
        return out
    P = np.array(rim.P)
    for ring_, sg in ((a, -1.0), (c, 1.0)):
        for k in range(1, n - 1):
            t = (ring_[0], ring_[k], ring_[k + 1])
            nn = np.cross(P[t[1]] - P[t[0]], P[t[2]] - P[t[0]])
            rim.T.append(t if nn[0] * sg >= 0 else (t[0], t[2], t[1]))
    return [rim]


def mag_frame():
    """The magazine's own directions: out along it from the receiver's side (to the right and a little forward),
    across it (fore and aft), and up."""
    out = np.array((math.sin(MAG_RAKE), math.cos(MAG_RAKE), 0.0))
    fore = np.array((math.cos(MAG_RAKE), -math.sin(MAG_RAKE), 0.0))
    return out, fore, np.array((0.0, 0.0, 1.0))


def mag_box(b, flat=True, sink=0.012):
    """The magazine as a box: from `sink` inside the receiver's right side out to its end."""
    out, fore, up = mag_frame()
    root = np.array((MAG_X, BODY_A, b + MAG_Z))
    half = np.array(((MAG_L + sink) / 2, MAG_W / 2, MAG_T / 2))
    cen = root + out * ((MAG_L - sink) / 2)
    A = np.stack([out, fore, up], 1)
    if flat:
        return mg.oriented_box(cen, A, half, "steel")
    pc = block(-half, half, "steel")
    pc.P = [cen + A @ p for p in pc.P]
    return mg.orient(pc)


def fin_places(step, thick, x0=FIN0, x1=FIN1, gap=0.005):
    """Where the fins stand along the barrel: every `step` from x0 to x1, none where the bipod's clamp sits."""
    out = []
    x = x0
    while x + thick <= x1 + 1e-9:
        if x + thick < HINGE_X - CLAMP - gap or x > HINGE_X + CLAMP + gap:
            out.append(x)
        x += step
    return out


def ribbed_bar(b, k, r=0.0245):
    """The finned length at mid: one thick three-sided bar (a ridge along its top, its third side underneath), its
    ends closed, with `k` dark bands across its two upper sides, so it reads as rings and the gaps between them. (The
    underside has none: the folded legs lie along it.)"""
    ph = math.pi / 2
    out = disc(FIN0, FIN1, r, b, 3, phase=ph)
    w = (FIN1 - FIN0) / (2 * k + 1)
    rr = mg.ring(3, r * 1.04, r * 1.04, 2.0, ph)            # (the ridge first, then the left foot, then the right)
    for i in range(k):
        x0 = FIN0 + w * (2 * i + 1)
        pc = mg.Piece("steel", dark=True)
        a = [pc.pt((x0, u, b + v)) for (u, v) in rr]
        c = [pc.pt((x0 + w, u, b + v)) for (u, v) in rr]
        pc.quad(a[0], c[0], c[1], a[1])
        pc.quad(a[2], c[2], c[0], a[0])
        out.append(mg.orient(pc, axis="x"))
    return out


def breda30():
    """The Breda modello 30 light machine gun as a section's gunner carries it on its bipod: 1.23 m. From the butt
    forward: the steel butt plate with its shoulder rest folded forward on the comb; the wooden butt, a flat-sided
    angular slab, its comb a step below the receiver's top and its underside rising straight from the toe to the
    wrist; the receiver, a tall slab-sided box with a flat top, deep over the trigger and stepping up to a shallower
    fore part, the rear sight's leaf standing on its back end, the oil reservoir's long lid along its top (every round
    was oiled as it went in), the cocking handle on its right behind the magazine, the way out for the empty cases
    dark on its left; the wooden pistol grip, the trigger and its guard under its rear; the magazine, a flat box
    standing out level from the receiver's right side on its housing, hinged at its front, the follower's slot dark
    along its top; the barrel's bearing at the receiver's front and the plain slender sleeve ahead of it; then the
    fins, ring after ring on the dark barrel, to two fingers' width behind the flash hider; the bipod's clamp round the
    barrel two thirds along it, the legs folded back under the fins; the plain muzzle with the front sight's blade
    on its block, and the small conical flash hider.
    Mid: the same big shapes, the ring nut narrowing to the sleeve, the finned length a thick bar banded dark and
    light. Far: boxes, the finned length one thicker piece of the barrel, the magazine a flat blade out to the right."""
    g = mg.Gun("breda30", "Breda 30")
    b = mg.BORE
    s2 = 1.0 / math.sqrt(2.0)                               # (a four-sided loft's sides stand this far in of its corners)
    g.paints = {}                                           # (steel and the rifle's wood only: no cartridge shows)
    g.muzzle = np.array((MUZZLE, 0.0, b))
    under = -FIN_R                                          # (the fins' underside over the bore)
    g.hinge = mg.hinge_of(b, under, HINGE_X)
    le = mg.leg_end(g.hinge)
    A = BODY_A
    top, low, fore_low = b + BODY_TOP, b + BODY_LOW, b + FORE_LOW
    m_out, m_fore, m_up = mag_frame()
    BODY = [(BODY0, top), (BODY0, low), (STEP_X, low), (STEP_X, fore_low), (BODY1, fore_low), (BODY1, top)]

    # ------------------------------------------------------------------------------------------ near (LOD0) ----
    # --- the butt: wood, flat-sided, angular; its comb a step below the receiver's top and falling a little to the
    # heel, its underside rising straight from the toe to the wrist; the steel butt plate; the shoulder rest, a steel
    # flap hinged at the plate's top, folded forward on the comb
    BUTT = [(BODY0, b + 0.022), (-0.332, b + 0.010), (HEEL + 0.005, b + 0.002), (HEEL + 0.005, b - 0.104),
            (-0.330, b - 0.113), (-0.160, b - 0.052), (BODY0, b - 0.040)]
    g.add("near", mg.slab_y(BUTT, 0.019, "wood", ch=0.004, middle=True))
    g.add("near", block((HEEL, -0.019, b - 0.110), (HEEL + 0.006, 0.019, b + 0.005), "steel"))
    g.add("near", block((HEEL + 0.002, -0.012, b + 0.008), (-0.285, 0.012, b + 0.0155), "steel"))
    # --- the receiver: a tall plain box, every side flat, deep over the trigger and shallower ahead of it; the oil
    # reservoir's lid along its top; the seam of the trigger group along each side
    g.add("near", prism(BODY, -A, A, "steel"))
    g.add("near", mg.box((0.075, -0.020, top), (0.300, 0.020, top + 0.007), "steel"))
    for sy in (-1.0, 1.0):
        g.add("near", mg.slot((0.030, sy * A, fore_low + 0.002), (0, sy, 0), (1, 0, 0), 0.105, 0.0016, n_end=1))
    # (the rear sight: its leaf standing across the receiver's back end)
    g.add("near", block((-0.040, -0.014, top), (0.010, 0.014, top + 0.006), "steel"))
    g.add("near", block((-0.036, -0.010, top + 0.006), (-0.030, 0.010, top + 0.026), "steel"))
    # (the cocking handle on the right, behind the magazine; the empty cases' way out on the left)
    g.add("near", block((0.150, A, b + 0.010), (0.174, A + 0.020, b + 0.022), "steel"))
    g.add("near", face([(0.222, -A - 0.0006, b - 0.006), (0.300, -A - 0.0006, b - 0.006),
                        (0.300, -A - 0.0006, b + 0.014), (0.222, -A - 0.0006, b + 0.014)], (0, -1, 0), "steel",
                       dark=True))
    # --- the magazine: the housing on the receiver's right side, the hinge at its front, the flat box standing out
    # of it, the follower's slot along its top
    g.add("near", block((MAG_X - 0.052, A, b + MAG_Z - 0.021), (MAG_X + 0.046, A + 0.012, b + MAG_Z + 0.021), "steel"))
    g.add("near", block((MAG_X + 0.046, A, b + MAG_Z - 0.024), (MAG_X + 0.058, A + 0.016, b + MAG_Z + 0.024), "steel"))
    g.add("near", mag_box(b))
    root = np.array((MAG_X, A, b + MAG_Z))
    g.add("near", mg.slot(root + m_out * (0.5 * MAG_L + 0.006) + m_up * (MAG_T / 2), (0, 0, 1), m_out,
                          0.5 * MAG_L - 0.022, 0.006, n_end=2))
    # --- the pistol grip: wood, raked back under the receiver; the trigger guard; the trigger
    gt = np.array((-0.016, 0.0, low - 0.002))
    ga = np.array((-0.36, 0.0, -0.933))
    ga /= np.linalg.norm(ga)
    g.add("near", mg.loft_path([gt, gt + ga * 0.054, gt + ga * 0.104], [(1, 0, 0.40)] * 3,
                               [(0.0140, 0.0180), (0.0152, 0.0195), (0.0142, 0.0185)], 6, "wood", e=2.6, cap0=False))
    # (the guard's back end runs into the grip's front, and the trigger hangs a finger's reach ahead of the grip, as
    # on the German guns)
    tg = [np.array((0.052, 0.0, low + 0.002)), np.array((0.036, 0.0, low - 0.036)), np.array((0.002, 0.0, low - 0.035)),
          np.array((-0.014, 0.0, low - 0.022))]
    g.add("near", mg.loft_path(tg, [(0, 1, 0)] * 4, [(0.0035, 0.006)] * 4, 3, "steel", cap0=False, cap1=False))
    g.add("near", mg.loft_path([np.array((0.018, 0.0, low + 0.001)), np.array((0.011, 0.0, low - 0.023))],
                               [(0, 1, 0)] * 2, [(0.003, 0.0045)] * 2, 3, "steel", cap0=False, cap1=False))
    g.grips["pistol"] = {"at": gt + ga * 0.052, "axis": ga}
    # --- the barrel: its bearing at the receiver's front, the plain slender sleeve, then the fins: the barrel between
    # them dark (it lies in their shadow), each fin a thin disc
    g.add("near", disc(BODY1 - 0.004, NUT1, NUT_R, b, 6, flat=True))
    g.add("near", mg.loft_x([(NUT1 - 0.002, 0.0, b, SLEEVE_R, SLEEVE_R), (FIN0, 0.0, b, SLEEVE_R, SLEEVE_R)], 6,
                            "steel", cap0=False, cap1=False))
    core = mg.loft_x([(FIN0 - 0.002, 0.0, b, CORE_R, CORE_R), (FIN1, 0.0, b, CORE_R, CORE_R)], 6, "steel",
                     cap0=False, cap1=False)
    for pc in core:
        pc.dark = True
    g.add("near", core)
    for x in fin_places(FIN_STEP, FIN_T):
        g.add("near", disc(x, x + FIN_T, FIN_R, b, 6, flat=True))
    # (the bipod's clamp round the barrel)
    g.add("near", disc(HINGE_X - CLAMP, HINGE_X + CLAMP, FIN_R + 0.002, b, 6, flat=True))
    # --- the plain muzzle; the front sight, a blade on its block clamped to the barrel just behind the flash hider
    # (as high over the bore as the rear sight's leaf); the flash hider, a collar and a small cone flaring to its
    # mouth, the mouth dark
    g.add("near", mg.loft_x([(FIN1 - 0.002, 0.0, b, CORE_R, CORE_R), (HIDER0, 0.0, b, 0.0108, 0.0108)], 6, "steel",
                            cap0=False, cap1=False))
    g.add("near", block((SIGHT_X - 0.011, -0.0075, b + CORE_R - 0.004), (SIGHT_X + 0.011, 0.0075, b + CORE_R + 0.007),
                        "steel"))
    g.add("near", block((SIGHT_X - 0.004, -0.0022, b + CORE_R + 0.007), (SIGHT_X + 0.004, 0.0022, b + 0.060), "steel"))
    g.add("near", mg.loft_x([(HIDER0 - 0.006, 0.0, b, 0.0135, 0.0135), (HIDER0 + 0.005, 0.0, b, 0.0135, 0.0135),
                             (MUZZLE, 0.0, b, 0.0185, 0.0185)], 6, "steel", cap0=False, cap1=True,
                            dark_caps=(False, True)))
    # --- the bipod on its clamp, the legs folded back under the fins
    mg.folded_bipod(g, "near", b, under)
    # --- where hands go: along the folded legs; the gun's underside from the butt's toe to the flash hider (the
    # butt's, the receiver's deep rear and its shallower fore part, the bearing's and the sleeve's, the folded legs,
    # the clamp, the last fins, the plain muzzle's); the butt's underside
    g.grips["legs"] = [(np.array((le, 0.0, b + under - 0.012)), np.array((HINGE_X - 0.020, 0.0, b + under - 0.008)))]
    line = [(HEEL + 0.015, b - 0.113), (-0.160, b - 0.052), (BODY0, b - 0.040), (BODY0 + 0.006, low),
            (STEP_X, low), (STEP_X + 0.008, fore_low), (BODY1 - 0.004, fore_low), (BODY1, b - NUT_R),
            (NUT1, b - NUT_R), (NUT1 + 0.004, b - SLEEVE_R), (le - 0.020, b - SLEEVE_R), (le, b + under - 0.012),
            (HINGE_X - 0.020, b + under - 0.008), (HINGE_X + 0.012, b + under - 0.008), (FIN1, b + under),
            (FIN1 + 0.004, b - CORE_R), (HIDER0, b - 0.0135), (MUZZLE, b - 0.0185)]
    g.grips["under"] = [(np.array((x0, 0.0, z0)), np.array((x1, 0.0, z1))) for (x0, z0), (x1, z1) in zip(line, line[1:])]
    g.grips["butt"] = [(np.array((HEEL + 0.015, 0.0, b - 0.113)), np.array((-0.160, 0.0, b - 0.052)))]
    g.grips["keep_out"] = None                 # (the magazine stands out to the right at the bore's height: nothing
                                               # hangs under the gun or on its left where the left fist holds)
    g.grips["hip_l_at"] = 0.36                 # (from the hip: his left fist under the plain sleeve, just behind the
                                               # folded legs' feet)
    g.grips["kneel_l_at"] = 0.22               # (kneeling: his left fist under the receiver's fore part)
    g.grips["park"] = (0.060, 0.0, b - 0.006)  # (the middle of the receiver's solid)
    # (how it rides and lies, the magazine standing 17 cm out of its right side: on his back with its LEFT side to
    # him, the magazine outward; on the ground on its left side, the magazine up: the direction of the gun's own that
    # points to his back, or into the ground, and how far the gun's left side stands off its middle)
    g.grips["slung"] = {"to_back": (0.0, -1.0, 0.0), "off": A + 0.002}
    g.grips["laid"] = {"down": (0.0, -1.0, 0.0), "off": A + 0.002}
    # (some of the rifleman's clips hold his rifle with its right side to his body, where the magazine would go
    # through him: cowering, into his face and forearm; crawling, into his head; running away, into his right thigh;
    # sitting in a lorry or a car with the gun standing between his knees, into his right thigh. How the gun would
    # clear him, for the builder to read: turned about its bore this many degrees (its right side up for more), or
    # carried this far further out to his right, the hands going with it)
    g.grips["roll"] = dict({"cower": 165.0, "crawl": -135.0, "car_ride": -90.0, "car_ride_back": -90.0},
                           **{"bench_" + k: -90.0 for k in ("ride", "sit_l", "rise_l", "sit_r", "rise_r", "sit_l_fore",
                                                            "rise_l_fore", "sit_l_aft", "rise_l_aft", "sit_r_fore",
                                                            "rise_r_fore", "sit_r_aft", "rise_r_aft")})
    g.grips["out"] = {"flee": 0.08}

    # ------------------------------------------------------------------------------------------- mid (LOD1) ----
    # (the butt a tapering box closed at the heel; the receiver with its step; the magazine a box; the grip)
    hz, hb = b - 0.051, 0.061                               # (the heel's middle and half depth)
    g.add("mid", close_end(mg.loft_x([(HEEL, 0.0, hz, 0.019 / s2, hb / s2),
                                      (BODY0, 0.0, b - 0.009, 0.019 / s2, 0.031 / s2)], 4, "wood", cap0=False,
                                     cap1=False, phase=math.pi / 4)[0], HEEL, -1.0))
    g.add("mid", prism(BODY, -A, A, "steel"))
    g.add("mid", mag_box(b, flat=False))
    grip = mg.loft_path([gt - ga * 0.004, gt + ga * 0.104], [(1, 0, 0.40)] * 2, [(0.0148, 0.019)] * 2, 4, "wood",
                        cap0=False, cap1=False)[0]
    g.add("mid", close_ring(grip, [4, 5, 6, 7], ga))        # (its foot closed on its own points)
    # (the bearing's ring nut and the sleeve; the finned length; the muzzle and the flash hider)
    g.add("mid", mg.loft_x([(BODY1, 0.0, b, 0.030, 0.030), (NUT1 + 0.006, 0.0, b, SLEEVE_R * 1.35, SLEEVE_R * 1.35),
                            (FIN0, 0.0, b, SLEEVE_R * 1.35, SLEEVE_R * 1.35)],
                           3, "steel", cap0=False, cap1=False, phase=math.pi / 2))
    g.add("mid", ribbed_bar(b, 7))
    g.add("mid", mg.loft_x([(FIN1 - 0.002, 0.0, b, CORE_R, CORE_R), (HIDER0, 0.0, b, CORE_R, CORE_R),
                            (MUZZLE, 0.0, b, 0.0185, 0.0185)], 3, "steel", cap0=False, cap1=False, phase=math.pi / 2))
    g.add("mid", face([(MUZZLE, u, b + v) for (u, v) in mg.ring(3, 0.0185, 0.0185, 2.0, math.pi / 2)], (1, 0, 0),
                      "steel", dark=True))
    mg.folded_bipod(g, "mid", b, under)

    # ------------------------------------------------------------------------------------------- far (LOD2) ----
    # (boxes: the butt three-sided, flat on top at the comb's width, its sides meeting in a keel along its underside;
    # the receiver, closed at its back where it stands over the butt; the barrel a thin three-sided rod with the
    # finned length one thicker piece on it; the magazine and the grip flat blades)
    c3 = math.cos(math.pi / 6)
    g.add("far", mg.loft_x([(HEEL, 0.0, hz + hb / 3, 0.019 / c3, hb * 4 / 3),
                            (BODY0 + 0.010, 0.0, b - 0.009 + 0.031 / 3, 0.019 / c3, 0.031 * 4 / 3)], 3, "wood",
                           cap0=False, cap1=False, phase=math.pi / 6))
    g.add("far", close_end(mg.loft_x([(BODY0, 0.0, b - 0.006, A / s2, 0.042 / s2),
                                      (BODY1, 0.0, b + 0.002, A / s2, 0.034 / s2)], 4, "steel", cap0=False,
                                     cap1=False, phase=math.pi / 4)[0], BODY0, -1.0))
    g.add("far", mg.loft_x([(BODY1, 0.0, b, 0.016, 0.016), (MUZZLE, 0.0, b, 0.014, 0.014)], 3, "steel", cap0=False,
                           cap1=False, phase=math.pi / 2))
    g.add("far", disc(FIN0, FIN1, 0.026, b, 3, phase=math.pi / 2))
    for sg in (1.0, -1.0):
        r0 = root - m_out * 0.010
        r1 = root + m_out * MAG_L
        g.add("far", face([r0 - m_fore * MAG_W / 2, r1 - m_fore * MAG_W / 2, r1 + m_fore * MAG_W / 2,
                           r0 + m_fore * MAG_W / 2], (0.0, 0.0, sg), "steel"))
    for side in (-1.0, 1.0):
        fx = np.array((0.018, 0.0, 0.0))
        g.add("far", face([gt - ga * 0.004 + fx, gt + ga * 0.102 + fx, gt + ga * 0.102 - fx, gt - ga * 0.004 - fx],
                          (0.0, side, 0.0), "wood"))
    mg.folded_bipod(g, "far", b, under)
    return g


def report():
    g = breda30()
    for L in ("near", "mid", "far"):
        P, N, paint, bone, dark, T = g.mesh(L)
        print(f"breda30 {L}: {len(P)} points, {len(T)} triangles, {int(dark.sum())} dark points, "
              f"x {P[:, 0].min():.3f} to {P[:, 0].max():.3f}, y {P[:, 1].min():.3f} to {P[:, 1].max():.3f}, "
              f"z {P[:, 2].min():.3f} to {P[:, 2].max():.3f}")


if __name__ == "__main__":
    report()
