"""The Petlyakov Pe-2 as the Soviet air force flew her in 1941 and 1942, laid out to measured drawings: her slim
fuselage with the glazed nose and the long greenhouse over her pilot and navigator, her two Klimov M-105s in slim
nacelles of their own lines, the dive brakes under her outer wings, her twin oval fins at the tips of her tailplane,
and the green and black over light blue of her time with plain red stars.

Her lines are measured from public three-view drawings of the real aircraft at a known scale: measured, not traced,
and nothing of the drawings is used but their measures. The numbers are in the game's form (game/plane_numbers.py and
fighters.py's docstring), x ahead, z up, in metres.

What planes.py reads from here (planes.bomber): NAME and NUMBERS; SEATS (who sits under her glass: fitout.SEATS);
BAY (her bomb bay: planes.BAYS) and NOSE_CANNON (her two fixed guns' muzzles: fitout.NOSE_CANNON).
"""

NAME = "Pe2"

# ---- the Pe-2, 1941-42 ----------------------------------------------------------------------------------------
# Her airframe is measured from "Petljakov Pe-2.svg" by Kaboldy (Wikimedia Commons, 19 October 2012; licensed CC BY-SA
# 3.0: its measures only are used), rendered 1920 pixels wide: 113.4 pixels to the metre, set by her span in the plan
# and the head-on view alike (1946 px for the real 17.16 m). Her length so read, from the tip of her glazed nose to the
# end of her tail cone, is 12.40 m in the plan and 12.37 m in the side view (the 12.66 m usually printed is 2% more).
# That sheet draws the Pe-2K, whose radials sat in fat round cowlings: her own M-105s' nacelles are taken from the
# side and plan of the "Petlyakov Pe-2 3-view silhouette" (Wikimedia Commons; a recognition silhouette of 1954, Crown
# copyright expired, public domain), set to the same span: the airscrew's spinner 1.06 m behind her nose, the nacelle
# ending behind her wing. Her nose tip stands at x = 5.0; her side view is drawn level, as it stands.
#
# What she is: the glazed nose of her navigator's bomb sight; her slim fuselage, deepest under her cockpit, tapering
# to a cone behind her tailplane; the long greenhouse over her pilot and, behind him facing aft, her navigator at his
# ShKAS; the ventral hatch and its gun behind her wing; her low wing, its centre section flat out to the nacelles and
# the outer panels tapering and rising to rounded tips, the slatted dive brake under each outer panel; her two M-105s
# with three-bladed airscrews; her tailplane with an oval fin and rudder at each tip; the mast on her windscreen;
# her tailwheel. Her bombs in her bay. The two fixed guns in her nose.
# Her coat is the Sturmovik's: green and black over light blue; the plain red star on her sides, on both her fins and
# under her wings (none above them from 1941); her tactical number on her fins (codes.py).
#
# Her fuselage, measured: (x, the height of its middle, half its width, half its depth); under her greenhouse its
# top is her skin's, the glass standing on it.
HULL = [
    (5.0, 0.05, 0.03, 0.03), (4.92, 0.06, 0.16, 0.14), (4.75, 0.075, 0.28, 0.215), (4.5, 0.085, 0.335, 0.335), (4.25, 0.10, 0.405, 0.41),
    (4.0, 0.08, 0.467, 0.49), (3.75, 0.075, 0.516, 0.545), (3.5, 0.10, 0.555, 0.62), (3.0, 0.09, 0.617, 0.67), (2.5, 0.08, 0.657, 0.69),
    (2.0, 0.08, 0.675, 0.70), (1.0, 0.075, 0.68, 0.705), (0.0, 0.07, 0.68, 0.69), (-1.0, 0.06, 0.675, 0.66), (-2.0, 0.05, 0.627, 0.62),
    (-3.0, 0.03, 0.565, 0.57), (-4.0, 0.03, 0.494, 0.50), (-5.0, 0.025, 0.424, 0.425), (-5.5, 0.0, 0.39, 0.37), (-6.0, -0.02, 0.33, 0.32),
    (-6.5, -0.02, 0.26, 0.24), (-7.0, 0.0, 0.2, 0.13), (-7.25, 0.02, 0.13, 0.06), (-7.37, 0.02, 0.03, 0.02)]

# Her greenhouse: (x, its sill, half its width there, how far its top stands above its sill), its sill where her
# fuselage is as wide as the glass.
HOOD = [(3.72, 0.63, 0.06, 0.01), (3.55, 0.648, 0.25, 0.097), (3.35, 0.599, 0.38, 0.331), (3.2, 0.591, 0.41, 0.459),
        (3.1, 0.596, 0.42, 0.494), (2.6, 0.632, 0.42, 0.458), (2.1, 0.651, 0.42, 0.439), (1.95, 0.669, 0.4, 0.391),
        (1.75, 0.688, 0.37, 0.332), (1.6, 0.743, 0.25, 0.197), (1.5, 0.777, 0.08, 0.083)]

# Her wing: (how far out, its leading edge, its trailing edge, the height of its chord, its thickness). Flat to the
# nacelles, then rising about 5.5 degrees (the head-on view) to her rounded tips.
WING = [(0.0, 1.87, -1.45, -0.16, 0.58), (0.9, 1.87, -1.43, -0.16, 0.57), (1.5, 1.87, -1.32, -0.155, 0.55), (2.0, 1.83, -1.26, -0.15, 0.53),
        (2.9, 1.76, -1.146, -0.14, 0.49), (3.4, 1.70, -1.04, -0.13, 0.46), (4.0, 1.64, -0.917, -0.07, 0.41), (5.0, 1.49, -0.70, 0.037, 0.33),
        (6.0, 1.349, -0.503, 0.14, 0.26), (7.0, 1.199, -0.291, 0.246, 0.20), (8.0, 1.049, -0.079, 0.34, 0.145), (8.3, 0.99, 0.08, 0.365, 0.11),
        (8.45, 0.89, 0.24, 0.38, 0.07), (8.55, 0.75, 0.40, 0.39, 0.03), (8.58, 0.6, 0.52, 0.395, 0.01)]

# Her tailplane, out to her fins (its tip inside them), rising about 6.7 degrees: its leading edge climbs from her
# tail cone to her fins' middles in the side view, as the head-on view has it. And each fin, an oval on its tip.
TAIL = [(0.0, -5.12, -7.17, 0.02, 0.18), (0.7, -5.16, -7.15, 0.102, 0.18), (1.2, -5.30, -7.08, 0.16, 0.16), (1.7, -5.44, -6.97, 0.219, 0.14),
        (2.0, -5.53, -6.88, 0.254, 0.13), (2.3, -5.62, -6.7, 0.289, 0.12), (2.46, -5.70, -6.61, 0.308, 0.09)]
FIN = [(0.0, -6.11, -6.39, -0.25, 0.02), (0.0, -5.81, -6.69, -0.1, 0.07), (0.0, -5.58, -6.92, 0.2, 0.1), (0.0, -5.49, -7.01, 0.62, 0.11),
       (0.0, -5.56, -6.94, 1.0, 0.1), (0.0, -5.76, -6.74, 1.3, 0.08), (0.0, -5.99, -6.51, 1.45, 0.05), (0.0, -6.17, -6.33, 1.5, 0.01)]
FIN_OUT = 2.47

# One M-105's nacelle from its spinner back under her wing (the game's frame for an engine in line: the spinner's tip
# 2.55 ahead of `lead`, its back at 2.09).
NACELLE = [(2.55, 0.0, 0.04, 0.04), (2.42, 0.0, 0.17, 0.17), (2.27, 0.0, 0.24, 0.24), (2.1, 0.0, 0.3, 0.3), (2.05, 0.0, 0.36, 0.40),
           (1.8, 0.0, 0.42, 0.48), (1.3, 0.0, 0.46, 0.53), (0.6, -0.02, 0.48, 0.56), (0.0, -0.05, 0.48, 0.56), (-1.0, -0.05, 0.46, 0.5),
           (-1.8, 0.0, 0.40, 0.42), (-2.6, 0.06, 0.28, 0.27), (-3.1, 0.08, 0.14, 0.13), (-3.35, 0.08, 0.03, 0.03)]

_GUN = (0.022, 0.018, "steel")                              # (a ShKAS's barrel, as the others' .303s and 7.7s)

NUMBERS = {
    "title": "Pe-2 (our own model)", "year": 1941,
    "hull": HULL, "square": 2.2, "screw": 1.5, "blades": 3,
    "glazed": (4.75, 4.25), "nose_frames": ((4.75, 4.5, 4.25), 8), "nose_bar": 0.5, "nose_frames_paint": "upper",
    "hoods": [(HOOD, (3.55, 3.3, 3.1, 2.6, 2.1, 1.9, 1.7), True)], "frames_paint": "upper",
    "wing": WING, "aileron": (0.62, 0.96, 0.72),
    "tail": TAIL, "elevator": (0.05, 0.95, 0.62),
    "fins": [(FIN, FIN_OUT)], "rudder": (0.06, 1.0, 0.5),
    # (her slatted dive brakes, under her outer panels)
    "brakes": (3.4, 6.6, 0.3, 0.12, 0.1),
    "patch": 2.6, "colours": {"upper": "4F5E3E", "upper2": "2C2F2A", "under": "8FB0BF", "nose": "4F5E3E", "boss": "4F5E3E"},
    "engines": [(2.3, 1.39, -0.38, False, 3.3)], "nacelle": NACELLE, "nacelle_square": 2.2,
    "pitot": True, "tailwheel": (-5.0, -0.78), "mast": (3.14, 1.05, 1.8, 0.0),
    "extras": [
        # (her navigator's ShKAS at the back of the greenhouse, and the ventral hatch's, aft and down, in its fairing)
        ("rod", ((1.75, 0.0, 0.98), (1.0, 0.0, 1.16)) + _GUN),
        ("body", ([(-1.35, -0.6, 0.02, 0.02), (-1.5, -0.62, 0.2, 0.1), (-1.9, -0.63, 0.26, 0.14), (-2.4, -0.6, 0.24, 0.12), (-2.75, -0.55, 0.1, 0.05)], 0.0, 2.0, "coat")),
        ("rod", ((-2.3, 0.0, -0.66), (-3.05, 0.0, -0.72)) + _GUN),
        # (the barrels of her two fixed guns, low in her nose)
        ("rod", ((4.35, 0.24, 0.0), (4.82, 0.24, 0.0), 0.02, 0.016, "leg"))],
    "side_marks": [(-3.6, 0.36, "SOV")], "wing_marks": [(6.0, 0.42, 0.55, None, "SOV", False, True)],
    "fin_marks": [(-6.25, 0.65, 0.3, "SOV")], "fin_marks_at": FIN_OUT, "code": (-4.6, 0.3),
}

# Her pilot, and her navigator behind him facing aft at his gun.
SEATS = [(0, 2.95, 0.1, 0.0), (0, 2.0, -0.1, 180.0)]
# Her bomb bay, under her wing: from, to along her.
BAY = (1.5, -0.6)
# The muzzles of her two fixed guns, low in her nose: (how far along her, where round her in degrees, 90 her top).
NOSE_CANNON = [(4.72, 200.0), (4.72, 340.0)]
