"""The Savoia-Marchetti SM.79 Sparviero, the Regia Aeronautica's three-engined medium bomber, laid out to a measured
drawing.

Her lines are measured from a public three-view drawing of the aircraft: measured, not traced, and nothing of the
drawing is used but its measures. The numbers are in the game's form (game/plane_numbers.py: stations of the fuselage
as (x, z of its middle, half its width, half its height), sections of a flying surface as (how far out, leading edge,
trailing edge, height, thickness)), x ahead, z up, in metres, z = 0 on the hub of her nose airscrew.

What planes.py reads from here: NAME and NUMBERS (her numbers, as EXTRA_PLANES[NAME]); SEATS (her two pilots under
her cabin's glass: fitout.SEATS[NAME]); BAY (her bomb bay) and UPRIGHT (her bombs stowed nose up in it); coat() (her
camouflage field, NUMBERS["graded"]); finish() (her white band, her rudder's cross and her nose engine's cylinder heads,
laid on her once she is built). planes.land takes all of these in; install() does the same for a scratch copy of this
file.
"""
import math

from mathutils import Vector, noise

NAME = "SM79"


# ---- the SM.79, as the bomber groups flew her from Spain to the Mediterranean, 1937-43 ------------------------------
# Measured from "Savoia-Marchetti SM.79.svg" (Wikimedia Commons, Kaboldy, CC BY-SA 3.0; its measures only are used),
# its 1920 px rendering. Its three views are drawn at one scale, 88.87 pixels to the metre, set by her span, the real
# 21.20 m; a pixel is 1.1 cm. Her length comes out 15.78 m, from the tip of her nose spinner (x = 5.20) to the
# trailing edge of her rudder (-10.58).
#
# Her shape: a slab-sided fuselage, its sides upright and its roof and belly rounded, the radial in her nose in a
# round cowling; the hump over her (the "gobbo"): the pilots' cabin under a framed windscreen and side windows, rising
# to its highest over the wing and falling in a step to the dorsal gunner's place at its back, his gun firing aft
# over a low fairing; the ventral gondola under her belly behind the wing, its gun firing aft and down; a waist gun at
# a window each side. Her wing low and thick, straight-tapered, its upper surface level from root to tip and its
# thickness falling away outboard, with a long fillet under the root of its trailing edge; the two wing radials in
# round cowlings on long nacelles under it; her tailplane braced by two struts each side to her lower fuselage; her
# fin swept, its rudder hinged straight up and down at her rudder post and running below her tail.
#
# Laid back over the drawing view by view, her outline is within 2 to 3 cm of its lines nearly everywhere; but for
# two places where the head-on view does not agree with the side view: it draws the bottom of her gondola 0.13 m lower
# and the top of her fin 0.08 m lower than the side view does, and the side view's are taken.

# (her fuselage: from the side view's top and bottom and the plan's half width, every half metre or closer; the
#  cowling round her nose radial ahead of 3.5; under her wing her belly lies on its centre section; under her hump
#  her roof is as high as it can be and still lie inside the hump's sides)
_HULL = [(5.2, 0.0, 0.03, 0.03), (5.12, 0.0, 0.13, 0.13), (4.9, 0.0, 0.2, 0.2), (4.8, 0.0, 0.45, 0.45), (4.7, 0.0, 0.62, 0.62),
         (4.55, 0.0, 0.69, 0.69), (4.2, 0.0, 0.705, 0.705), (3.65, 0.0, 0.7, 0.7), (3.53, 0.0, 0.69, 0.69), (3.45, -0.005, 0.68, 0.695),
         (3.3, 0.0185, 0.7, 0.7185), (3.0, 0.0355, 0.712, 0.7355), (2.6, 0.058, 0.72, 0.758), (2.2, 0.0775, 0.725, 0.7775),
         (2.1, 0.085, 0.726, 0.805), (1.95, 0.115, 0.727, 0.885), (1.8, 0.135, 0.728, 0.945), (1.6, 0.15, 0.73, 1.0), (1.4, 0.1675, 0.73, 1.0325),
         (1.2, 0.1775, 0.73, 1.0475), (1.0, 0.1875, 0.73, 1.0575), (0.6, 0.2, 0.73, 1.07), (0.2, 0.205, 0.73, 1.075), (0.0, 0.165, 0.73, 1.035),
         (-0.3, 0.1675, 0.73, 1.0325), (-0.5, 0.169, 0.73, 1.031), (-0.6, 0.195, 0.73, 1.055), (-0.8, 0.267, 0.73, 1.117),
         (-1.3, 0.279, 0.73, 1.099), (-1.8, 0.3065, 0.73, 1.0665), (-2.05, 0.325, 0.728, 1.045), (-2.3, 0.3515, 0.726, 1.0155),
         (-2.8, 0.357, 0.725, 0.982), (-3.3, 0.3605, 0.725, 0.9505), (-4.3, 0.37, 0.692, 0.89), (-5.3, 0.38, 0.645, 0.83),
         (-6.3, 0.3965, 0.566, 0.7565), (-7.3, 0.405, 0.473, 0.692), (-8.3, 0.419, 0.349, 0.627), (-8.8, 0.4245, 0.276, 0.5935),
         (-9.3, 0.4415, 0.191, 0.5545), (-9.55, 0.45, 0.146, 0.53), (-9.8, 0.451, 0.113, 0.511), (-9.86, 0.455, 0.08, 0.495),
         (-9.9, 0.45, 0.02, 0.3)]
# (round at her cowling; behind it a box with its corners eased, as the head-on view draws her sides upright)
_SQUARES = [(5.2, 2.0), (3.6, 2.0), (3.3, 4.5), (-9.0, 4.5), (-9.9, 3.0)]

# (her hump, the pilots' cabin and the gunner's place behind it, one body on her roof: stations (x, its middle, half
#  its width, half its height), its top the side view's, its sides drawn in toward its ridge as the head-on view
#  shows them; its front cut by the two flat panes of her windscreen; behind the cabin the step down to the gunner)
_HUMP = [(2.18, 0.95, 0.62, 0.3), (2.1, 0.95, 0.69, 0.3), (1.95, 0.95, 0.71, 0.3), (1.8, 0.95, 0.71, 0.243), (1.6, 0.95, 0.71, 0.389),
         (1.4, 0.95, 0.71, 0.518), (1.2, 0.95, 0.71, 0.608), (1.0, 0.95, 0.71, 0.687), (0.8, 0.95, 0.71, 0.743), (0.6, 0.95, 0.71, 0.788),
         (0.4, 0.95, 0.71, 0.811), (0.2, 0.95, 0.71, 0.822), (0.16, 0.95, 0.71, 0.81), (0.12, 0.95, 0.71, 0.75), (0.08, 0.95, 0.71, 0.65),
         (0.05, 0.95, 0.71, 0.55), (0.02, 0.95, 0.71, 0.52), (-0.5, 0.95, 0.71, 0.53), (-0.54, 0.95, 0.7, 0.49), (-0.58, 0.95, 0.68, 0.43),
         (-0.62, 0.95, 0.6, 0.3)]
_CABIN = {"square": 1.3, "ease": 4.0, "screen": (2.12, 0.88, 1.84, 1.16, 0.9), "pane": (0.4, 0.26, 1.0, 0.02), "back": 1.25, "sill": 0.96,
          "top": 1.12, "posts": (1.7, 1.48, 1.25)}

# (her wing: the plan's leading and trailing edges; the head-on view's upper surface, level at -0.034, and lower
#  surface, rising 7 cm to the metre out, give each section's height and thickness)
_WING = [(0.0, 2.062, -2.9, -0.531, 0.994), (0.73, 2.062, -2.9, -0.531, 0.994), (0.8, 2.056, -2.665, -0.528, 0.988), (1.0, 2.041, -2.238, -0.52, 0.971),
         (1.25, 2.022, -1.945, -0.51, 0.951), (1.5, 2.003, -1.821, -0.499, 0.93), (1.75, 1.984, -1.789, -0.489, 0.91), (4.0, 1.814, -1.391, -0.408, 0.748),
         (6.0, 1.662, -1.037, -0.322, 0.576), (8.0, 1.511, -0.683, -0.244, 0.419), (9.0, 1.435, -0.506, -0.197, 0.327), (9.8, 1.374, -0.365, -0.166, 0.264),
         (10.0, 1.355, -0.325, -0.159, 0.249), (10.25, 1.228, -0.167, -0.134, 0.199), (10.4, 1.104, -0.032, -0.119, 0.17), (10.5, 1.02, 0.11, -0.095, 0.122),
         (10.55, 0.868, 0.193, -0.083, 0.098), (10.6, 0.69, 0.355, -0.054, 0.04), (10.615, 0.53, 0.5, -0.039, 0.01)]

# (her tailplane: the plan's edges, level, through the top of her tail cone)
_TAIL = [(0.0, -7.5, -9.698, 0.88, 0.175), (0.5, -7.538, -9.698, 0.88, 0.172), (1.0, -7.661, -9.698, 0.88, 0.165), (1.5, -7.864, -9.698, 0.88, 0.155),
         (2.0, -8.157, -9.698, 0.88, 0.14), (2.5, -8.584, -9.69, 0.88, 0.12), (2.75, -8.854, -9.642, 0.88, 0.095), (2.9, -9.08, -9.55, 0.88, 0.06),
         (2.98, -9.25, -9.36, 0.88, 0.01)]

# (her fin and rudder, row by row up the side view: below her tail cone's top the fin is inside her and the rudder
#  behind her post, its foot curving down behind her tail cone; above it the fin's swept leading edge runs down
#  into a fillet on her spine)
_FIN = [(0, -9.75, -9.97, -0.12, 0.02), (0, -9.65, -10.16, -0.04, 0.06), (0, -9.55, -10.34, 0.05, 0.08), (0, -9.45, -10.47, 0.15, 0.1),
        (0, -9.4, -10.53, 0.26, 0.1), (0, -8.0, -10.53, 1.0, 0.135), (0, -7.313, -10.53, 1.1, 0.13),
        (0, -7.616, -10.53, 1.2, 0.125), (0, -7.763, -10.53, 1.4, 0.118), (0, -7.932, -10.53, 1.6, 0.11), (0, -8.157, -10.53, 1.8, 0.1),
        (0, -8.438, -10.53, 2.0, 0.09), (0, -8.607, -10.53, 2.1, 0.084), (0, -8.82, -10.52, 2.2, 0.075), (0, -9.079, -10.452, 2.3, 0.062),
        (0, -9.45, -10.3, 2.38, 0.035), (0, -9.75, -10.1, 2.405, 0.01)]

# (her dorsal gunner's low fairing behind the hump, over which his gun fires aft; her ventral gondola under her belly
#  behind the wing, its slanted front, its gun firing aft and down from its open back)
_FAIRING = [(-0.36, 1.3, 0.28, 0.12), (-0.45, 1.3, 0.33, 0.2), (-0.6, 1.3, 0.34, 0.208), (-0.9, 1.3, 0.36, 0.247), (-1.1, 1.3, 0.36, 0.247), (-1.3, 1.3, 0.34, 0.213),
            (-1.5, 1.3, 0.3, 0.174), (-1.7, 1.3, 0.22, 0.118), (-1.82, 1.3, 0.12, 0.07)]
_GONDOLA = [(-2.93, -0.5, 0.3, 0.12), (-3.05, -0.6, 0.37, 0.2), (-3.15, -0.67, 0.39, 0.3), (-3.3, -0.66, 0.4, 0.313), (-3.55, -0.65, 0.4, 0.323),
            (-3.8, -0.63, 0.4, 0.332), (-4.3, -0.6, 0.39, 0.317), (-4.8, -0.56, 0.38, 0.29), (-5.05, -0.54, 0.37, 0.265), (-5.13, -0.53, 0.3, 0.2),
            (-5.16, -0.52, 0.1, 0.05)]

SAND = "C2A878"           # (the light hazel sand of her upper surfaces, as shipped)
SMOKE = "4E5232"          # (the dark olive green of her soft blotches)
UNDER = "A9ACA8"          # (the light grey under her)
WHITE = "E6E4DA"          # (her band)

# Her coat: the blotches as shipped (the game's own noise, at her patches' size and where the game lays it), with an
# edge of the same breadth everywhere, as the French machines' is tight (the art director, 23 Nov: the edges were a
# staircase of half-metre cells). The coat is laid point by point and the game mixes between points, so an edge
# narrower than her mesh comes out in steps of the mesh (steepening the field alone, as france_d520._EDGE does, left
# steps of 13 cm on her): the field is taken as a distance from the edge, in metres (the noise over its slope), and the
# edge is _EDGE each side of it, about as broad as her rings stand apart (`grain`, closer than the usual), as the
# Me 262's is (planes._me262_edge). Her nose's cowling stays plain sand, as it was (ahead of nose_back the field is
# below nought: her upper colour, the sand of her nose).
_PATCH = 1.9
_EDGE = 0.09                                                # (half the breadth of a sprayed edge, in metres)
_NOSE_BACK = 3.55


def _noise(x, y):
    return noise.noise(Vector(((x + 31.7) / _PATCH, (y * 1.3 + 12.3) / _PATCH, 0.0))) - 0.02      # (the sculptor's coat_of)


def coat():
    """Her camouflage field: the sculptor's own noise, as a distance from a blotch's edge, soft for _EDGE each side."""
    def field(p):
        if p.x > _NOSE_BACK:
            return -1.0
        f = _noise(p.x, p.y)
        slope = math.hypot(_noise(p.x + 0.01, p.y) - f, _noise(p.x, p.y + 0.01) - f) / 0.01
        return max(-2.0, min(2.0, f / max(slope, 0.05) / _EDGE)) * 0.045
    return field


# The radial in her nose, as the sculptor lays one in each of her wing nacelles (fine model only): fourteen steel
# cylinder heads in a ring round the dark face of the engine, round her airscrew's hub. Her nose's face is not flat, as
# the nacelles' is, but falls toward her spinner, so each head stands square to it where the ring crosses it (0.43 m
# out, at 4.809 m, falling 2.46 to 1), 2.5 cm proud of it. (The art director, 23 Nov: the face read as a flat black
# disc, like a hole.) (ring out, face x, its facing (along, out), a head's radius)
_HEADS = (0.43, 4.809, (0.927, 0.376), 0.075)

NUMBERS = {
    "title": "SM.79 Sparviero (our own model)", "year": 1937,
    "hull": _HULL, "square": 2.0, "squares": _SQUARES,
    "screw": 1.55, "blades": 3, "spinner": 3, "engine": "radial", "lip": 4.72, "nose_back": 3.55, "nose_engine": True,
    "hoods": [(_HUMP, (), False)], "cabin": _CABIN, "frames_paint": "upper",
    "wing": _WING, "aileron": (0.58, 0.94, 0.74), "flap": (0.1, 0.56, 0.76),
    "tail": _TAIL, "elevator": (0.08, 0.96, 0.64),
    "fin": _FIN, "rudder": (0.0, 1.0, 0.5),
    "rudder_line": ([(-0.12, -9.85), (2.405, -9.85)], ((-0.12, -9.85), (2.405, -9.85))),
    # (sand with soft green blotches over light grey, as she was shipped; her scribed lines soft, as on every pale machine)
    "patch": 1.9, "colours": {"upper": SAND, "upper2": SMOKE, "under": UNDER, "nose": SAND, "boss": SAND}, "lines": (0.6, 0.45),
    "grain": {"wing": 0.13, "tail": 0.12, "fin": 0.12, "hull": 0.15},
    # (each wing's Alfa Romeo 126 radial: its airscrew 2.62 m out and 0.55 m below her nose's, its nacelle running back
    #  under her wing past its trailing edge; her wheels fold back into it)
    "engines": [(2.62, 1.38, -0.55, True, 3.63)], "well": (2.62, 0.45, 0.5), "pitot": True,
    "tailwheel": (-9.6, -0.19), "mast": (-2.39, 1.3, 1.61, 0.0),
    # (her windows aft, painted on her sides: the waist window each side, at its gun; the two low windows behind it)
    "glass_bands": [(-3.9, -4.4, 0.1, 0.31), (-4.2, -4.6, 0.07, -0.64), (-5.0, -5.4, 0.07, -0.69)],
    # (her guns, in the order the battle knows them: the fixed gun over her cabin; the dorsal gunner's, firing aft over
    #  his fairing; the gondola's, aft and down; a waist gun at each side window)
    "extras": [("body", (_FAIRING, 0.0, 2.2, "coat")), ("body", (_GONDOLA, 0.0, 2.6, "coat")),
               ("rod", ((1.22, 0.0, 1.535), (1.9, 0.0, 1.51), 0.024, 0.018, "steel")),
               ("rod", ((-0.3, 0.0, 1.5), (-1.35, 0.0, 1.78), 0.026, 0.02, "steel")),
               ("rod", ((-4.95, 0.0, -0.62), (-5.95, 0.0, -0.84), 0.022, 0.018, "steel")),
               ("rod", ((-3.95, 0.45, 0.66), (-4.5, 1.0, 0.62), 0.02, 0.016, "steel")),
               # (her tailplane's struts, two each side, from under it to her lower side)
               ("rod", ((-8.2, 1.35, 0.815), (-8.2, 0.33, 0.02), 0.022, 0.022, "upper")),
               ("rod", ((-9.0, 1.35, 0.815), (-9.0, 0.23, 0.05), 0.022, 0.022, "upper"))],
    # (her far model's extras at the game's own grain: from the distance it is drawn at, a ring of 12 is round)
    "plain_extras": (12, 0.3, 10),
    # (Italy, the art director's of 19 Nov: the white band round her rear fuselage is in her paint (finish(), below);
    #  the white cross on her rudder; bare wings; the game's number aft of her waist)
    "side_marks": [], "wing_marks": [], "code": (-4.9, 0.32),
    "band": (-6.15, -6.75),
}

SEATS = [(0, 1.62, 0.32, 0.0), (0, 1.62, -0.32, 0.0)]       # (her two pilots side by side in the cabin)
BAY = (1.4, -0.6)                                           # (her bomb bay, between her wing's spars)
UPRIGHT = (5, 0.66)                                         # (five bombs stowed nose up in a row in it; its roof's height)
FAR_LOADS = True                                            # (her far model carries her bombs too: fitout.plain_loads)


def finish(m, fine=True):
    """What the sculptor does not lay on her: the white band round her rear fuselage, her skin's faces between its two
    ends painted white; the white cross on her rudder, both sides, a small plain shield at its middle, placed as an
    owner's flag is (planes.rudder_flag), its tag `ITA-CROSS`; and on the fine model the cylinder heads of her nose
    engine (_HEADS)."""
    import planes
    s = NUMBERS
    G = planes.game()
    hull = G["Body"](s["hull"], s.get("square", 2.0))
    x_front, x_back = s["band"]
    if "band" not in m.paints:
        m.paints["band"] = WHITE
        m.paint_at["band"] = len(m.paints) - 1
    band = m.paint_at["band"]
    skin = {m.paint_at[k] for k in ("upper", "upper2", "under", "nose") if k in m.paint_at}
    for k, f in enumerate(m.faces):
        if m.fpaint[k] not in skin:
            continue
        c = sum((Vector(m.verts[i]) for i in f), Vector()) / len(f)
        if not (x_back < c.x < x_front):
            continue
        if abs(c.y) > hull.w(-c.x) + 0.03 or abs(c.z - hull.z(-c.x)) > hull.h(-c.x) + 0.03:
            continue                                         # (her fuselage only: nothing out on a strut)
        m.fpaint[k] = band
        if k < len(m.fcoat) and m.fcoat[k]:
            m.fcoat[k] = type(m.fcoat[k])(0)                 # (her coat no longer paints it point by point: it is white)
    if "rudder" in m.part_at:
        mk = planes.rudder_flag(m)
        mk["tag"] = "ITA-CROSS"
        at = next((i for i, q in enumerate(m.marks) if q.get("place") == "code"), len(m.marks))
        m.marks.insert(at, mk)
    m.info.setdefault("file", {})["marks_note"] = ("Italy (art director, 19 Nov): the white band round the rear fuselage is painted on the skin; "
                                                   "ITA-CROSS is the white cross on the rudder with a small plain shield at its middle; the wings are bare")
    if fine:
        ring, x, (along, out), r = _HEADS
        m.use("plane")
        for k in range(14):                                  # (as the sculptor's radial_details: the first at her beam)
            a = 2.0 * math.pi * k / 14.0
            way = Vector((0.0, math.cos(a), math.sin(a)))
            m.rod(Vector((x - 0.11 * along, 0.0, 0.0)) + way * (ring - 0.11 * out), Vector((x + 0.025 * along, 0.0, 0.0)) + way * (ring + 0.025 * out),
                  r, "steel", sides=8)


def install():
    import planes
    planes.land(__import__(__name__))
