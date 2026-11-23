"""The Vickers Wellington Mk IC as RAF Bomber Command flew her in 1940 and 1941, laid out to a measured drawing: her deep
fabric-covered fuselage with the geodetic lattice of her structure showing quietly through its doped skin, the FN5
turrets in her nose and her tail with two .303s each, a .303 at each beam hatch, her long tapered wing with a Pegasus
in a round cowling on each side, her bomb bay under her wing, and the night colours of her time.

Her lines are measured from a public three-view drawing of the real aircraft at a known scale: measured, not traced,
and nothing of the drawing is used but its measures. The numbers are in the game's form (game/plane_numbers.py and
fighters.py's docstring), x ahead, z up, in metres.

What planes.py reads from here (planes.bomber): NAME and NUMBERS; SEATS (who sits under her glass: fitout.SEATS);
BAY (her bomb bay: planes.BAYS) and WAIST (her beam hatches: planes.WAIST).
"""

NAME = "Wellington"

# ---- the Wellington Mk IC, 1940-41 ------------------------------------------------------------------------------
# Measured from "Vickers Wellington.svg" by Emoscopes (Wikimedia Commons, 4 October 2009; licensed CC BY-SA 3.0 and
# GFDL: its measures only are used), rendered 1920 pixels wide: 40.44 pixels to the metre, set by her span in the head-on
# view (1062 px for the real 26.26 m, 86 ft 2 in; the sheet's own scale bar gives 40.4 and the plan's span 1062.5 px).
# The plan, turned by her wing tips and her fuselage's own middle line (132.05 degrees), gives her length, 754 px or
# 18.67 m from the front of her nose turret to the back of her tail turret: the 61 ft 3 in of the Mk I and IC (the
# 64 ft 7 in often printed is a later mark's). The side view is drawn standing on her wheels, nose up: it is levelled by
# her thrust line (5 degrees) and stretched along her by 0.9% to the plan's length; her height so read, from the
# top of her fin to her wheels, is 5.32 m, the real 5.31 m. Her nose turret's front stands at x = 7.0.
#
# What she is: the FN5 turret in her nose, glass over a metal foot, its two guns; her fuselage, deep and round-backed,
# wider below her cabin and slimming to her tail turret, which is the same FN5 turned about with its two guns; her
# cabin's greenhouse over her pilot and the second pilot beside him, the mast on its roof; the DF loop in its
# teardrop fairing behind it, the astrodome over her wing; a gun at a hatch each side of her behind her wing
# (the Mk IC's beam guns, which took the place of the Mk I's ventral turret); her bomb bay under her wing; her long
# wing, its leading and trailing edges straight from root to tip and a little dihedral, the ailerons outboard; a
# Bristol Pegasus XVIII in a long round cowling at each side, its nacelle running back under the wing, and its
# three-bladed de Havilland airscrew; her tall fin with its rounded top and the tailplane on top of her rear fuselage;
# her tailwheel.
# Her skin is doped fabric over her geodetic frame: no rivets on it, but the lattice of the frame under it, two sets of
# members crossing at right angles about 45 degrees to her length, as a faint shade in the paint (`geodetic`: the art
# director's ask, "a quiet lattice in the skin, not a cage"). Her night colours are the Lancaster's: dark green and dark
# earth over black, the black up her sides and over her fin; the roundel with the yellow ring on her sides, the red and
# blue one over her wings and none under them, the fin flash; her squadron's letters (codes.py) in medium sea grey.
#
# Her fuselage, measured: (x, the height of its middle, half its width, half its depth). It begins at the ring of her
# nose turret, which stands out ahead of it, and ends at the ring of her tail turret.
HULL = [
    (6.34, 0.2, 0.3, 0.35), (6.32, 0.2, 0.6, 0.78), (6.0, 0.17, 0.705, 0.84), (5.5, 0.11, 0.766, 0.88), (5.0, 0.06, 0.80, 0.93),
    (4.5, 0.03, 0.84, 0.97), (4.0, 0.005, 0.85, 1.045), (3.5, 0.03, 0.865, 1.12), (3.0, 0.08, 0.87, 1.22), (2.5, 0.135, 0.875, 1.325),
    (2.0, 0.11, 0.875, 1.35), (1.0, 0.065, 0.875, 1.365), (0.0, 0.04, 0.875, 1.38), (-1.0, 0.0, 0.875, 1.34), (-2.0, -0.035, 0.87, 1.325),
    (-3.0, -0.075, 0.865, 1.285), (-4.0, -0.09, 0.853, 1.25), (-5.0, -0.10, 0.836, 1.21), (-6.0, -0.10, 0.804, 1.16), (-7.0, -0.05, 0.74, 1.04),
    (-8.0, -0.035, 0.705, 0.975), (-9.0, 0.03, 0.66, 0.92), (-10.0, 0.08, 0.62, 0.87), (-10.85, 0.11, 0.575, 0.84), (-10.93, 0.11, 0.5, 0.74),
    (-10.97, 0.11, 0.2, 0.3)]

# Her cabin's greenhouse: (x, its sill, half its width there, how far its top stands above its sill). Its width is
# the plan's; its top the side view's; its sill where her fuselage is as wide as the glass, so that the glass rises
# out of her skin's own curve.
HOOD = [(4.65, 0.965, 0.25, 0.065), (4.55, 0.896, 0.42, 0.294), (4.45, 0.833, 0.52, 0.437), (4.3, 0.789, 0.58, 0.521), (4.1, 0.748, 0.63, 0.587),
        (3.8, 0.766, 0.65, 0.614), (3.5, 0.808, 0.66, 0.602), (3.2, 0.912, 0.64, 0.498), (2.9, 1.101, 0.55, 0.309), (2.65, 1.313, 0.38, 0.127)]

# Her wing: (how far out, its leading edge, its trailing edge, the height of its chord, its thickness). Her leading
# and trailing edges run straight from her root to her tip, which is rounded; it rises about 3.5 degrees.
WING = [(0.0, 2.05, -2.55, -0.05, 1.05), (1.0, 2.03, -2.50, -0.05, 1.0), (2.0, 1.95, -2.32, 0.01, 0.93), (3.7, 1.81, -2.00, 0.12, 0.79),
        (5.0, 1.71, -1.78, 0.20, 0.70), (7.0, 1.55, -1.40, 0.32, 0.58), (9.0, 1.38, -1.03, 0.45, 0.465), (11.0, 1.22, -0.68, 0.57, 0.33),
        (12.1, 1.14, -0.47, 0.64, 0.24), (12.6, 1.09, -0.22, 0.67, 0.18), (12.9, 1.05, 0.02, 0.69, 0.12), (13.05, 0.92, 0.2, 0.70, 0.07),
        (13.13, 0.6, 0.5, 0.71, 0.01)]

# Her tailplane, on top of her rear fuselage (the plan's lower half: its upper half is drawn cut), and her fin.
TAIL = [(0.0, -8.3, -10.38, 0.9, 0.30), (0.7, -8.36, -10.36, 0.9, 0.28), (2.47, -8.78, -10.28, 0.9, 0.21), (3.46, -9.0, -10.24, 0.9, 0.17),
        (3.96, -9.12, -10.21, 0.9, 0.14), (4.2, -9.22, -10.16, 0.9, 0.12), (4.45, -9.42, -10.01, 0.9, 0.08), (4.6, -9.65, -9.85, 0.9, 0.02)]
FIN = [(0, -8.3, -10.82, 0.8, 0.26), (0, -8.37, -10.82, 1.0, 0.26), (0, -8.62, -10.8, 1.5, 0.24), (0, -8.84, -10.75, 2.0, 0.22), (0, -9.07, -10.72, 2.5, 0.19),
       (0, -9.31, -10.66, 3.0, 0.16), (0, -9.55, -10.5, 3.5, 0.12), (0, -9.72, -10.37, 3.7, 0.09), (0, -9.9, -10.15, 3.84, 0.05), (0, -10.0, -10.08, 3.88, 0.01)]

_GUN = (0.022, 0.018, "steel")                              # (a .303's barrel and its jacket, as the others')

NUMBERS = {
    "title": "Wellington Mk IC (our own model)", "year": 1940,
    "hull": HULL, "square": 2.2, "screw": 1.72, "blades": 3,
    "hoods": [(HOOD, (4.3, 4.0, 3.75, 3.5, 3.2), True)], "frames_paint": "upper",
    "wing": WING, "aileron": (0.55, 0.93, 0.74),
    "tail": TAIL, "elevator": (0.08, 0.97, 0.68),
    "fin": FIN, "rudder": (0.06, 1.0, 0.62),
    # (her night colours, as the Lancaster's: the black up her sides and over her fin; no rivets anywhere on her
    #  fabric, and the lattice of her frame under it instead)
    "patch": 3.4, "colours": {"upper": "434A33", "upper2": "685A42", "under": "1B1C1E", "nose": "434A33", "boss": "171717"},
    "under_from": 0.45, "fins_under": True, "rivets_under": False, "geodetic": (0.46, 0.009, 0.1),
    # (each Pegasus: its airscrew 2.93 out from her middle and 0.5 above it; the nacelle back under her wing; the nine
    #  heads of the engine in the cowling's mouth; her airscrews' yellow tips)
    "engines": [(2.93, 2.10, 0.5, True, 4.4)], "well": (2.93, 0.45, 0.5), "engine_face": 9, "blade_tips": True, "framed_windows": True,
    "pitot": True, "tailwheel": (-8.85, -1.4), "mast": (3.62, 1.42, 2.3, 0.0),
    "extras": [
        # (her nose turret, glass over its metal foot; the astrodome; her tail turret: and their guns in the order the
        #  battle knows them, then the beam guns)
        ("dome", ((6.5, 0.0, 0.2), (0.48, 0.6, 0.76), 0.0)),
        ("dome", ((0.1, 0.0, 1.38), (0.22, 0.22, 0.2))),
        ("dome", ((-11.1, 0.0, 0.06), (0.54, 0.56, 0.8), -0.05)),
        ("rod", ((6.75, 0.12, 0.45), (7.5, 0.12, 0.5)) + _GUN),
        ("rod", ((-11.4, 0.12, 0.08), (-12.15, 0.12, 0.04)) + _GUN),
        ("rod", ((-3.55, 0.7, 0.42), (-4.15, 1.5, 0.36)) + _GUN),
        # (the spinner on each airscrew's hub)
        ("body", ([(4.5, 0.5, 0.02, 0.02), (4.43, 0.5, 0.12, 0.12), (4.3, 0.5, 0.2, 0.2), (4.14, 0.5, 0.24, 0.24), (4.02, 0.5, 0.24, 0.24)], 2.93, 2.0, "boss")),
        # (the DF loop in its teardrop fairing on a stalk, behind her cabin)
        ("body", ([(2.02, 1.85, 0.01, 0.01), (1.93, 1.85, 0.07, 0.1), (1.78, 1.85, 0.085, 0.14), (1.55, 1.85, 0.06, 0.1), (1.35, 1.85, 0.01, 0.01)], 0.0, 2.0, "coat")),
        ("rod", ((1.75, 0.0, 1.42), (1.72, 0.0, 1.75), 0.03, 0.025, "upper"))],
    "side_marks": [(-6.2, 0.6, "UK-A1")], "wing_marks": [(9.0, 0.17, 0.95, "UK-B", None, True, False)], "fin_marks": [(-9.5, 1.9, 0.4, "UK-FIN")],
    "code": (-4.6, 0.5),
}

# Her pilot to port at his wheel and the second pilot beside him.
SEATS = [(0, 3.75, 0.28, 0.0), (0, 3.75, -0.28, 0.0)]
# Her bomb bay, under her wing: from, to along her.
BAY = (2.9, -2.1)
# The hatch each side of her where her beam gunner stands: (from, to along her; from, to in degrees above her middle).
WAIST = (-3.3, -4.1, 5.0, 45.0)
