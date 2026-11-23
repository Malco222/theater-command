"""The Liore et Olivier LeO 451, the Armee de l'Air's modern medium bomber of 1939-40, laid out to a measured drawing.

Her lines are measured from a public three-view drawing of the aircraft at a known scale: measured, not traced, and
nothing of the drawing is used but its measures. The numbers are in the game's form (game/plane_numbers.py: stations
of the fuselage as (x, z of its middle, half its width, half its height), sections of a flying surface as (how far out,
leading edge, trailing edge, height, thickness)), x ahead, z up, in metres, z = 0 near the middle of her fuselage
where her wing passes through it.

What planes.py reads from here: NAME and NUMBERS (her numbers, as EXTRA_PLANES[NAME]); SEATS (her pilot under his
glass: fitout.SEATS[NAME]); NOSE_CREW (her navigator in her glass nose); BAY (her bomb bay); coat() (her camouflage
field, NUMBERS["graded"]). planes.land takes all of these in; install() does the same for a scratch copy of this file.
"""
from mathutils import Vector, noise

NAME = "LeO451"


# ---- the LeO 451, as the groupes de bombardement flew her from the winter of 1939 to June 1940 -----------------------
# Measured from "LeO 45 3-view" (Wikimedia Commons: the plan 3-vues of L'Aeronautique, December 1938, author unknown,
# PD-France; its measures only are used), its 1920 px thumbnail. Its three views are drawn at three slightly different
# scales, so each is set by a dimension of the real aircraft: the side view 44.50 pixels to the metre (her length, the
# real 17.17 m, from the tip of her nose to the tail of her fuselage); the plan 42.92 along her (the same length) and
# 44.85 across her (her span, the real 22.52 m); the head-on view 44.05 (her starboard half-span; its port wing is drawn
# short where the plan stands beside it). The sheet's own scale bar reads 44.0. Her nose is at x = 6.60, the tail of
# her fuselage at -10.57, her wing tips 11.26 out.
#
# The drawing is of the first LeO 45 of 1938. What the 451 of 1939-40 has that it does not show, and how it is given
# here: her long glazed nose, finely framed panes all round its first 1.4 m and over its upper half back to her
# pilot's windscreen (`glazed`), within the drawing's outline;
# her pilot's framed canopy, a glass house standing just proud of the line of her roof (`hoods`); the gunner's glazed
# position at the back of her cabin with its 20 mm Hispano-Suiza cannon trained aft, where the drawing shows only the
# step in her roof behind it (a turret of the game's kind, `dome`); and her retractable ventral turret with its MAC
# 1934 gun, let down a little under her belly behind her wing (`ball`). Her outline is the drawing's everywhere else.
#
# Her shape: a deep, narrow fuselage, oval in section and fuller below its middle, its roof level from her windscreen
# to the step behind her gunner's position, then running straight back to her tail while her belly sweeps up to meet
# it; her wing low on her, thick at its root and tapered hard to rounded tips, its leading edge nearly straight and its
# trailing edge swept forward, with 6 degrees of dihedral from root to tip; the two Gnome-Rhone 14N radials in round
# cowlings on long nacelles under it; her tailplane through the top of her tail cone, set at 15 degrees of dihedral,
# with an oval fin and rudder at each tip.
#
# Laid back over the drawing, view by view (a pixel is 2.2 cm), her outline is on average within 1 to 3 cm of its
# lines in the side view, 3 to 5 cm in the plan and the head-on view, and within 7 cm in nine places of ten; but for
# two places where the head-on view does not agree with the other two: there her fins stand 0.4 m taller than it
# draws them, as tall as the side view does, and her tailplane is about 0.1 m higher. Her span is the real 22.52 m and her length 17.17 m, by the scales above.

# (her fuselage: from the side view's top and bottom and the plan's half width, column by column)
_HULL = [(6.6, -0.31, 0.03, 0.03), (6.578, -0.326, 0.1, 0.146), (6.218, -0.315, 0.284, 0.45), (5.858, -0.281, 0.401, 0.618), (5.499, -0.259, 0.495, 0.776),
         (5.139, -0.224, 0.568, 0.877), (4.78, -0.203, 0.619, 0.967), (4.42, -0.113, 0.655, 1.123), (4.061, -0.079, 0.698, 1.179), (3.701, -0.067, 0.718, 1.236),
         (3.342, -0.045, 0.737, 1.281), (2.982, -0.034, 0.753, 1.292), (2.263, -0.023, 0.754, 1.325), (1.544, -0.011, 0.742, 1.337), (0.106, 0.012, 0.73, 1.315),
         (-1.333, 0.068, 0.73, 1.259), (-2.052, 0.113, 0.723, 1.214), (-2.2, 0.024, 0.721, 1.106), (-2.411, 0.034, 0.718, 1.09), (-3.13, 0.101, 0.702, 1.045),
         (-3.85, 0.18, 0.671, 1.011), (-4.57, 0.258, 0.654, 0.955), (-5.29, 0.348, 0.6, 0.91), (-6.1, 0.46, 0.554, 0.843), (-6.91, 0.573, 0.495, 0.753),
         (-7.71, 0.685, 0.438, 0.663), (-8.52, 0.798, 0.376, 0.55), (-9.06, 0.888, 0.335, 0.461), (-9.6, 0.978, 0.29, 0.371), (-10.14, 1.079, 0.22, 0.27),
         (-10.41, 1.134, 0.15, 0.214), (-10.52, 1.134, 0.08, 0.12), (-10.57, 1.134, 0.02, 0.03)]

# (her section, as the head-on view gives it: full and nearly square below her middle, drawn in above it to a quarter
#  less at her spine; rounder at her nose)
_TAPER = (0.74, 0.0, [(6.6, 0.3), (5.5, 1.0), (-10.6, 1.0)])

# (her pilot's canopy: stations (x, its sill, half its width there, how far its top stands above its sill), each just
#  proud of her skin at its sill and 2.5 cm over her roof; its ends inside her)
_HOOD = [(4.64, 0.64, 0.22, 0.08), (4.58, 0.66, 0.428, 0.241), (4.5, 0.7, 0.437, 0.275), (4.35, 0.72, 0.469, 0.333), (4.15, 0.74, 0.495, 0.363),
         (3.9, 0.76, 0.519, 0.396), (3.6, 0.78, 0.54, 0.433), (3.3, 0.8, 0.56, 0.464), (3.05, 0.81, 0.569, 0.464), (2.95, 0.82, 0.569, 0.44),
         (2.9, 0.82, 0.37, 0.2)]

# (her wing: the plan's leading and trailing edges, the head-on view's height and thickness; a fillet under the root
#  of her trailing edge)
_WING = [(0.0, 2.76, -3.0, -0.585, 0.97), (0.72, 2.69, -2.97, -0.507, 0.954), (0.95, 2.665, -2.7, -0.482, 0.948), (1.25, 2.633, -2.42, -0.449, 0.942),
         (2.03, 2.55, -2.16, -0.363, 0.913), (2.75, 2.442, -1.98, -0.275, 0.865), (3.63, 2.31, -1.76, -0.166, 0.807), (4.64, 2.204, -1.529, -0.042, 0.74),
         (5.64, 2.1, -1.3, 0.078, 0.666), (6.44, 2.03, -1.101, 0.167, 0.606), (7.25, 1.96, -0.9, 0.247, 0.551), (8.05, 1.905, -0.69, 0.332, 0.512),
         (8.85, 1.85, -0.48, 0.42, 0.448), (9.65, 1.78, -0.27, 0.507, 0.376), (10.06, 1.75, -0.18, 0.549, 0.346), (10.46, 1.71, -0.04, 0.59, 0.316),
         (10.86, 1.64, 0.24, 0.622, 0.27), (11.1, 1.5, 0.55, 0.643, 0.192), (11.26, 1.12, 0.94, 0.66, 0.02)]

# (her tailplane: the plan's edges; its height rising 0.26 m to the metre out, 15 degrees, from inside her tail cone
#  to the lower half of each fin)
_TAIL = [(0.0, -8.06, -10.38, 0.92, 0.24), (0.42, -8.15, -10.36, 1.029, 0.235), (0.96, -8.19, -10.34, 1.17, 0.23), (2.03, -8.26, -10.22, 1.448, 0.21),
         (3.1, -8.36, -9.99, 1.726, 0.19), (3.45, -8.42, -9.92, 1.817, 0.18)]

# (each fin and rudder: an oval, from the side view row by row; 3.40 m out, standing on the tip of her tailplane)
_FIN = [(0, -9.10, -9.90, 1.15, 0.02), (0, -8.62, -10.20, 1.30, 0.08), (0, -8.33, -10.26, 1.50, 0.11), (0, -8.34, -10.33, 1.75, 0.13),
        (0, -8.40, -10.41, 2.00, 0.14), (0, -8.46, -10.43, 2.20, 0.13), (0, -8.59, -10.39, 2.38, 0.12), (0, -8.75, -10.32, 2.52, 0.10),
        (0, -8.97, -10.21, 2.65, 0.08), (0, -9.29, -10.01, 2.79, 0.05), (0, -9.50, -9.80, 2.85, 0.01)]
_FINS_OUT = 3.40

# (her camouflage's patches, a bomber's size, with a tight sprayed edge, as the D.520's (france_d520._EDGE): the coat
#  blends over 6 cm in the middle of an edge and 12 cm where it is broadest in nine of ten; her wing's, tail's, fins'
#  and fuselage's rings stand closer (`grain`) so that her mesh does not blur it again)
_PATCH = 3.4
_EDGE = 3.0


def coat():
    """Her camouflage field: the game's own noise at her patches' size."""
    def field(p):
        return _EDGE * (noise.noise(Vector(((p.x + 17.3) / _PATCH, (p.y * 1.2 - 8.1) / _PATCH, 0.0))) - 0.02)
    return field


NUMBERS = {
    "title": "LeO 451 (our own model)", "year": 1939,
    "hull": _HULL, "square": 3.0, "taper": _TAPER,
    "screw": 1.8, "blades": 3,
    # (her glass nose, the production 451's long cone: panes all round it for its first 1.4 m, in eight hoops and twelve
    #  ribs, and over its upper half back to her pilot's windscreen; her outline is the drawing's)
    "glazed": (5.2, 4.6), "glazed_sill": 10.0, "windows_back": True, "nose_frames": ((6.4, 6.15, 5.9, 5.65, 5.4, 5.2, 4.9, 4.62), 12), "nose_cap": 6.5,
    "nose_bar": 0.5, "nose_frames_paint": "upper", "cabin_ribs": (50.0, 90.0, 130.0),
    "hoods": [(_HOOD, (4.35, 3.9, 3.45), True)],
    "opening": (4.40, 3.10, 50.0), "frames_paint": "upper",
    "wing": _WING,
    "aileron": (0.58, 0.95, 0.75), "flap": (0.08, 0.56, 0.76),
    "tail": _TAIL, "elevator": (0.1, 0.95, 0.65),
    "fins": [(_FIN, _FINS_OUT)], "rudder": (0.05, 1.0, 0.66),
    # (khaki and dark earth over light blue-grey, as on her fighters; her scribed lines narrow and soft)
    "patch": 1.0, "colours": {"upper": "4A5236", "upper2": "54463A", "under": "9EA8AA", "nose": "4A5236", "boss": "3A3C34"},
    "grain": {"wing": 0.13, "tail": 0.12, "fin": 0.12, "hull": 0.15},
    "lines": (0.6, 0.55),
    # (each Gnome-Rhone 14N: its airscrew 2.75 out and 0.34 below her middle line, its nacelle running back under her
    #  wing to its trailing edge)
    "engines": [(2.75, 2.55, -0.34, True, 4.6)], "well": (2.75, 0.45, 0.5), "pitot": True,
    "tailwheel": (-9.85, 0.36),
    # (her guns, in the order the battle knows them: the 20 mm cannon at the back of her cabin, trained aft over her
    #  tail; the MAC 1934 in her ventral turret, let down under her belly behind her wing)
    "extras": [("dome", ((-1.55, 0.0, 1.18), (0.55, 0.42, 0.17))), ("rod", ((-1.75, 0.0, 1.28), (-2.95, 0.0, 1.44), 0.03, 0.022, "steel")),
               ("ball", ((-2.75, 0.0, -0.96), (0.40, 0.40, 0.18), "under")), ("rod", ((-2.85, 0.0, -1.05), (-3.65, 0.0, -1.24), 0.02, 0.016, "steel"))],
    # (her roundel on each side of her rear fuselage; one on each wing, top and bottom; the tricolour on each rudder,
    #  blue forward, both faces)
    "side_marks": [(-5.6, 0.48, "FRA")], "wing_marks": [(8.4, 0.7, 0.72, "FRA", "FRA", True, True)], "code": (-7.2, 0.4),
    "rudder_stripes": ("FRA-FIN", ("203A78", "E6E4DA", "A82A26")),
}

SEATS = [(0, 3.75, 0.0, 0.0)]                               # (her pilot, under his canopy)
NOSE_CREW = [(5.3, 0.0, -0.25, 0.0)]                        # (her navigator and bomb aimer, in her glass nose)
BAY = (1.7, -1.1)                                           # (her bomb bay, under her wing)


def install():
    import planes
    planes.land(__import__(__name__))
