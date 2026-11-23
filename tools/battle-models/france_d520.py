"""The Dewoitine D.520, the Armee de l'Air's best fighter of the battle of France from June 1940, laid out to a
measured drawing (France's other fighters stay the plain `Fighter`, drawn as an MS.406).

Her lines are measured from a public three-view drawing of the real aircraft at a known scale: measured, not traced,
and nothing of the drawing is used but its measures. The numbers are in the game's form (game/plane_numbers.py:
stations of the fuselage as (x, z of its middle, half its width, half its height), sections of a flying surface as
(how far out, leading edge, trailing edge, height, thickness)), x ahead, z up, in metres, with the airscrew's axis at
z = 0; her glazing is the fighters' `canopy` (fighters.py), a run of pieces each lofted through cross-sections whose
few points make flat panes between framed bars.

What planes.py reads from here: NAME and NUMBERS (her numbers, as EXTRA_PLANES[NAME]); SEATS (her pilot under her
glass: fitout.SEATS[NAME]) and SEAT_DROP; coat() (her camouflage field, NUMBERS["graded"]); TAIL_CUT (where her tail
comes away). planes.land takes all of these in; install() does the same for a scratch copy of this file.
"""
import math

from mathutils import Vector, noise

import fitout
from fighters import SKIRT, hood_of

NAME = "D520"


# ---- the D.520, as the Armee de l'Air flew her in May and June 1940 -------------------------------------------------
# Measured from "Dewoitine D.520 3-view line drawing" by Kaboldy (Wikimedia Commons, CC BY-SA 3.0 and GFDL: its
# measures only are used), its 1920 px thumbnail. One sheet, one scale along her and another across her, each from a
# dimension of the real aircraft: along her (side view and plan) 148.86 pixels to the metre, her length the real 8.76 m
# from the tip of her spinner to her rudder; across her (plan and head-on view) 148.33, her span the real 10.18 m. The
# side view is drawn with her airscrew's axis level, as the stations are read. Her spinner's tip is at x = 3.30; her
# rudder ends at x = -5.46.
#
# Her shape: the long nose of the Hispano-Suiza 12Y-45 under its cowling, the six exhaust stubs each side along it and
# the cannon's muzzle standing out of the tip of her pointed spinner (the moteur-canon: the 20 mm HS 404 fired through
# the hub of her three-bladed Ratier airscrew); a fuselage deepest under her cockpit, egg-shaped in section (drawn in
# toward her spine, `taper`), that runs back in a straight taper to her rudder post; her cockpit far aft, over her wing's
# trailing edge, under a windscreen of flat panes and a hood with tall flat sides and a rounded roof, the turtledeck
# behind it carrying a large rear-vision window each side; the radiator's bath under her belly between her wings; her
# low wing, straight-tapered with rounded tips and a fillet at the root of its trailing edge; her tailplane at the middle
# of her rear fuselage, and her fin with its swept leading edge and her tall rudder, rounded at the top and running down
# below her tail cone, hinged straight up and down at her rudder post (`rudder_line`). Her two MAC 1934 guns in each
# wing fire through ports in its leading edge; her mast stands on her turtledeck (no wire); her tailwheel is fixed, its
# fork raked back; her pitot stands under her port wingtip.
#
# Her fuselage: her spinner (its tip open round the cannon's muzzle), the cowling's front ring behind it, her body back to
# her rudder post. Each station read off the drawing's outline: her top and her keel from the side view, her widest from
# the plan (inside her exhaust stubs); under her glass her top is the deck that runs from the windscreen's foot to her
# turtledeck, inside the glass; under her wing the keel is her wing's own underside at its root.
_HULL = [(3.300, 0.0, 0.03, 0.03), (3.293, 0.0, 0.07, 0.07), (3.266, 0.0, 0.094, 0.094), (3.239, 0.0, 0.114, 0.114),
         (3.185, 0.0, 0.148, 0.148), (3.132, 0.0, 0.161, 0.161), (2.985, 0.0, 0.165, 0.165), (2.958, 0.0, 0.208, 0.208),
         (2.946, 0.0, 0.222, 0.222),
         (2.944, -0.003, 0.226, 0.226), (2.917, -0.027, 0.246, 0.259), (2.843, -0.067, 0.297, 0.312), (2.742, -0.101, 0.351, 0.366),
         (2.642, -0.121, 0.384, 0.40), (2.541, -0.131, 0.398, 0.43), (2.44, -0.141, 0.405, 0.453), (2.239, -0.155, 0.415, 0.487),
         (2.037, -0.168, 0.425, 0.521), (1.836, -0.181, 0.435, 0.547), (1.634, -0.195, 0.448, 0.574), (1.432, -0.205, 0.458, 0.591),
         (1.231, -0.208, 0.465, 0.601), (1.029, -0.212, 0.465, 0.611), (0.828, -0.218, 0.465, 0.625), (0.626, -0.222, 0.465, 0.635),
         (0.425, -0.215, 0.462, 0.642), (0.022, -0.208, 0.462, 0.648), (-0.381, -0.202, 0.458, 0.655), (-0.583, -0.185, 0.455, 0.672),
         (-0.784, -0.158, 0.455, 0.692), (-0.986, -0.128, 0.452, 0.709), (-1.187, -0.101, 0.448, 0.729), (-1.389, -0.071, 0.448, 0.746),
         (-1.591, -0.037, 0.442, 0.759), (-1.792, -0.034, 0.435, 0.749), (-1.994, -0.027, 0.425, 0.729), (-2.195, -0.027, 0.411, 0.709),
         (-2.397, -0.02, 0.394, 0.682), (-2.598, -0.017, 0.378, 0.652), (-2.80, -0.017, 0.357, 0.618), (-3.001, -0.024, 0.337, 0.578),
         (-3.203, -0.027, 0.317, 0.541), (-3.404, -0.024, 0.293, 0.511), (-3.606, -0.02, 0.27, 0.48), (-3.807, -0.013, 0.243, 0.447),
         (-4.009, -0.01, 0.216, 0.416), (-4.21, -0.007, 0.192, 0.386), (-4.412, -0.003, 0.175, 0.356), (-4.613, 0.0, 0.169, 0.326),
         (-4.714, 0.0, 0.128, 0.312), (-4.768, -0.003, 0.04, 0.289)]
_SPINNER = 9                                                # (the first nine stations are her spinner, which turns)
# (her section is an egg, widest low down and drawn in toward her spine, most at her cockpit and turtledeck, where the
#  head-on view has her deck narrower than her sides; her spinner and the cowling's front ring are round)
_TAPER = (0.6, 0.0, [(3.3, 0.0), (2.95, 0.0), (2.6, 0.3), (0.4, 0.55), (-0.45, 1.0), (-2.4, 1.0), (-4.8, 0.7)])

# Her windscreen: the flat front pane, from its foot on her deck (x, half its width, height) up to its head (x, height);
# its top's middle (x, height); its quarter panes' foot along her side (x, half its width, height), back to the
# windscreen's rear frame. Her hood (x, the height of its sill, how far out the glass is there, the frame at its shoulder
# (out, up), its top): its front bow, the frame halfway along it, its rear bow; behind that the glass tucked down into
# her turtledeck. (The head-on view stands her windscreen's top 2 cm higher than the side view: her heights are the
# side view's, the head-on view's breadths scaled to them.)
_SCREEN_FOOT = (-0.44, 0.15, 0.455)
_SCREEN_SPRING = (-0.80, 0.745)
_SCREEN_ARCH = (-0.858, 0.776)
_SCREEN_SILL = [(-0.44, 0.15, 0.455), (-0.55, 0.245, 0.40), (-0.66, 0.30, 0.355), (-0.76, 0.325, 0.33), (-0.865, 0.335, 0.325)]
_SCREEN_BACK = (-0.865, (0.235, 0.655), 0.776)
_HOOD = [(-0.885, 0.322, 0.338, (0.238, 0.655), 0.778), (-1.20, 0.318, 0.338, (0.238, 0.652), 0.774), (-1.50, 0.312, 0.335, (0.232, 0.645), 0.766)]
_TUCK = (-1.56, 0.31, 0.29, (0.18, 0.62), 0.72)


def _top(hull, x, y=0.0):
    """How high her skin stands at x, y off her middle line (her sides are drawn in: fitout.hull_body)."""
    return hull.top_at(x, y) if hasattr(hull, "top_at") else fitout.body_top(hull, x, y)


def _section(hull, x, sill, ys, shoulder, top, roof=3, band=0.05):
    """Half a section of her glass: its metal rail from her side up to the sill where the glass begins, `ys` out (the
    first SKIRT points: the rail stands on her skin wherever that is lower); a flat side pane leaning in from the sill
    to its shoulder, (out, up); and its roof, rounded over from the shoulder to her middle line at `top` in `roof` flat
    panes."""
    yb = ys + 0.004
    zb = min(sill - band, _top(hull, x, yb) - 0.006)
    pts = [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb), (ys, sill), shoulder]
    y1, z1 = shoulder
    for k in range(1, roof + 1):
        a = math.radians(90.0 * k / roof)
        pts.append((0.0 if k == roof else y1 * math.cos(a), z1 + (top - z1) * math.sin(a)))
    return pts


def _d520_canopy(hull):
    fx, fy, fz = _SCREEN_FOOT
    sx, sz = _SCREEN_SPRING
    slope = (sz - fz) / (fx - sx)                           # (the front pane's lean: it rises so much for every metre aft)

    def pane(x):
        return fz + slope * (fx - x)
    sill = {x: (y, z) for x, y, z in _SCREEN_SILL}
    # her windscreen: the flat front pane from its straight foot on her deck up to its head; a flat quarter pane each
    # side from the post back to the rear frame, its foot running down her side to the sill and its top the frame that
    # runs back from the head to the hood's shoulder
    zf = _top(hull, fx, fy) - 0.01
    foot = [(fy, zf)] * 4 + [(fy * 2.0 / 3.0, zf), (fy / 3.0, zf), (0.0, zf)]
    mid_x = -0.66
    zm = pane(mid_x)
    post = fy + 0.06 * (fx - mid_x) / (fx - sx)
    mid = _section(hull, mid_x, sill[mid_x][1], sill[mid_x][0], (post, zm), zm)[:4] + [(post * 2.0 / 3.0, zm), (post / 3.0, zm), (0.0, zm)]
    spring = _section(hull, sx, 0.33, 0.33, (0.21, sz), sz)[:4] + [(0.14, sz), (0.07, sz), (0.0, sz)]
    ax, az = _SCREEN_ARCH
    arch = _section(hull, ax, 0.325, 0.335, (0.235, 0.66), az)
    bx, bshoulder, btop = _SCREEN_BACK
    back = _section(hull, bx, sill[bx][1], sill[bx][0], bshoulder, btop)
    screen = {"sections": [(fx, foot), (mid_x, mid), (sx, spring), (ax, arch), (bx, back)], "skirt": SKIRT, "hoops": [-1],
              "rails": [3, SKIRT], "bars": [(0, 3, 6), (2, 3, 6)], "bar": 0.02}
    # her hood, shut: its front bow, the frame halfway along it and its rear bow; its sills and its shoulder frames;
    # behind the rear bow its glass runs on down into her turtledeck
    hood = [(x, _section(hull, x, sl, ys, sh, top)) for x, sl, ys, sh, top in _HOOD + [_TUCK]]
    slide = {"sections": hood, "skirt": SKIRT, "hoops": [0, 1, 2], "rails": [SKIRT, 3], "bar": 0.019, "bare": (len(hood) - 1,)}
    return [screen, slide]


# Her wing: its leading edge straight from root to tip (back 0.144 m for every metre out), its trailing edge straight
# (forward 0.13 m a metre) but for the fillet at its root and a step of 6 cm where her aileron begins, as drawn; the tip
# rounded. Its middle rises 3 degrees to 1.5 m out and 4.5 degrees beyond, as the head-on view has it; 16 per cent
# thick at the root, 10 near the tip, as thick as the head-on view has her. Under her fuselage its root's underside is her keel line in the side view.
_WING = [(0.0, 1.396, -1.02, -0.69, 0.36), (0.46, 1.33, -1.02, -0.665, 0.341), (0.634, 1.305, -0.818, -0.655, 0.334),
         (1.227, 1.224, -0.724, -0.623, 0.309), (1.82, 1.137, -0.65, -0.583, 0.278), (2.414, 1.05, -0.576, -0.537, 0.24),
         (3.007, 0.962, -0.496, -0.49, 0.203), (3.60, 0.882, -0.462, -0.444, 0.165), (3.748, 0.861, -0.401, -0.432, 0.156),
         (4.045, 0.814, -0.361, -0.409, 0.137), (4.342, 0.774, -0.328, -0.386, 0.118), (4.638, 0.734, -0.247, -0.363, 0.10),
         (4.746, 0.714, -0.193, -0.354, 0.093), (4.827, 0.694, -0.133, -0.348, 0.088), (4.881, 0.673, -0.086, -0.343, 0.084),
         (4.935, 0.626, -0.025, -0.339, 0.068), (4.989, 0.599, 0.049, -0.335, 0.044), (5.016, 0.573, 0.096, -0.333, 0.032),
         (5.043, 0.546, 0.156, -0.331, 0.021), (5.07, 0.499, 0.237, -0.329, 0.017), (5.09, 0.371, 0.364, -0.327, 0.01)]

# Her coat: the three-colour camouflage of 1939-40 as two of its colours, the khaki and the dark earth (the game's coat
# has two upper colours; the dark blue-grey is left out), in large soft-edged patches, over the light blue-grey beneath.
_PATCH = 1.7


def coat():
    """Her camouflage field: the game's own noise at her patches' size, on her cowling as on the rest of her (the coat
    of every other machine stops at her nose's own paint)."""
    def field(p):
        return noise.noise(Vector(((p.x + 31.7) / _PATCH, (p.y * 1.3 + 12.3) / _PATCH, 0.0))) - 0.02
    return field


NUMBERS = {
    "title": "D.520 (our own model)", "year": 1940,
    "hull": _HULL, "square": 2.2, "taper": _TAPER,
    "screw": 1.50, "blades": 3, "spinner": _SPINNER, "nose_back": 0.46,
    "stubs": (2.41, 1.56, 15.0),                            # (six exhaust stubs each side of her Hispano, along her cowling)
    "hub_gun": 0.07,                                        # (her cannon's muzzle, standing this far out of her spinner's tip)
    "hoods": hood_of([(_SCREEN_FOOT[0], _SCREEN_FOOT[2], _SCREEN_FOOT[1], 0.01), (_SCREEN_ARCH[0], 0.325, 0.335, _SCREEN_ARCH[1] - 0.325),
                      (_SCREEN_BACK[0], 0.325, 0.335, _SCREEN_BACK[2] - 0.325)]
                     + [(x, sl, ys, top - sl) for x, sl, ys, sh, top in _HOOD]),
    "canopy": _d520_canopy,
    "opening": (-0.50, -1.45, 40.0),                        # (her cockpit, open under the glass from her instrument panel to behind her seat)
    "frames_paint": "upper",
    # (the rear-vision window each side of her turtledeck, behind her hood)
    "skin_windows": [(-1.60, -2.10, 0.34, 0.62)],
    "wing": _WING,
    # (her ailerons from the step in her trailing edge nearly to the tip; her split flaps from her side out to them)
    "aileron": (0.72, 0.96, 0.76), "flap": (0.10, 0.72, 0.76),
    # (her tailplane at the middle of her rear fuselage, flat; its root cut away behind for her rudder; its tips rounded)
    "tail": [(0.0, -3.70, -4.72, 0.27, 0.11), (0.20, -3.75, -4.75, 0.27, 0.11), (0.31, -3.794, -4.849, 0.27, 0.105),
             (0.553, -3.875, -4.943, 0.27, 0.10), (0.796, -3.955, -4.909, 0.27, 0.092), (1.038, -4.036, -4.869, 0.27, 0.085),
             (1.281, -4.116, -4.829, 0.27, 0.076), (1.524, -4.197, -4.789, 0.27, 0.066), (1.605, -4.224, -4.775, 0.27, 0.06),
             (1.685, -4.271, -4.728, 0.27, 0.05), (1.766, -4.358, -4.640, 0.27, 0.035), (1.80, -4.46, -4.53, 0.27, 0.01)],
    "elevator": (0.13, 0.97, 0.60),
    # (her fin and rudder from the foot of her rudder up: below her tail cone's top the fin is inside her, the rudder
    #  behind her post; above it the fin's swept leading edge runs down into a fillet on her spine; her rudder's top and
    #  trailing edge rounded, as drawn)
    "fin": [(0, -4.70, -4.80, -0.29, 0.02), (0, -4.60, -5.218, -0.212, 0.06), (0, -4.55, -5.332, -0.13, 0.08), (0, -4.50, -5.393, -0.05, 0.09),
            (0, -4.45, -5.453, 0.11, 0.10), (0, -4.40, -5.46, 0.19, 0.10), (0, -4.10, -5.453, 0.31, 0.11), (0, -3.25, -5.43, 0.47, 0.11),
            (0, -3.45, -5.413, 0.50, 0.11), (0, -3.653, -5.393, 0.59, 0.105), (0, -3.734, -5.373, 0.675, 0.10), (0, -3.807, -5.346, 0.756, 0.095),
            (0, -3.881, -5.312, 0.836, 0.09), (0, -3.955, -5.285, 0.917, 0.085), (0, -4.022, -5.245, 0.997, 0.08), (0, -4.096, -5.205, 1.078, 0.075),
            (0, -4.170, -5.158, 1.159, 0.07), (0, -4.244, -5.111, 1.239, 0.065), (0, -4.311, -5.057, 1.32, 0.058), (0, -4.385, -4.997, 1.40, 0.05),
            (0, -4.466, -4.923, 1.481, 0.04), (0, -4.573, -4.795, 1.555, 0.025), (0, -4.66, -4.728, 1.568, 0.01)],
    # (her rudder, cut from her fin straight up and down at her post, 4.73 m aft, from its foot to her tip, and turning on
    #  that line)
    "rudder": (0.0, 1.0, 0.48),
    "rudder_line": ([(-0.29, -4.728), (1.568, -4.728)], ((-0.29, -4.728), (1.568, -4.728))),
    # (khaki and a dark earth close in lightness, the earth kept dark so that it reads brown and not tan; her scribed
    #  lines narrow and soft, about a third as strong as the game's)
    "patch": 1.0, "colours": {"upper": "4A5236", "upper2": "54463A", "under": "9EA8AA", "nose": "4A5236", "boss": "3A3C34"},
    "lines": (0.6, 0.55),
    # (her guns: two MAC 1934s in each wing, their ports in its leading edge, outside her airscrew's disc)
    "guns": [(1.95, 0.05, 0.03, 0.02), (2.25, 0.05, 0.03, 0.02)], "well": (1.05, 0.40, 0.34), "pitot": 0.95,
    # (her fixed tailwheel, its fork raked back to its axle)
    "tailwheel": (-4.486, -0.642), "tailwheel_leg": (-4.33, -0.36),
    "mast": (-2.20, 0.66, 1.54, 0.0),
    "scoops": [
        # (her radiator's bath under her belly, between her wings: its mouth under her wing's leading edge, deepest under
        #  the middle of her wing, faired up into her behind)
        ([(0.53, -0.84, 0.30, 0.04), (0.465, -0.90, 0.38, 0.13), (0.364, -0.93, 0.40, 0.165), (0.163, -0.94, 0.40, 0.18),
          (-0.24, -0.93, 0.40, 0.165), (-0.583, -0.90, 0.38, 0.12), (-0.684, -0.88, 0.36, 0.10), (-0.78, -0.84, 0.30, 0.03)], 0.0, 2.6)],
    "extras": [
        # (the dark mouth of her radiator's bath)
        ("body", ([(0.49, -0.95, 0.004, 0.10), (0.48, -0.95, 0.35, 0.12), (0.47, -0.95, 0.35, 0.12), (0.46, -0.95, 0.004, 0.10)], 0.0, 2.6, "engine", "fine"))],
    # (her roundel on each side, as large as her side carries it; one on each wing, top and bottom; the tricolour on her
    #  rudder, blue forward)
    "side_marks": [(-2.25, 0.30, "FRA")], "wing_marks": [(3.65, 0.22, 0.44, "FRA", "FRA", True, True)], "code": (-3.2, 0.28),
    "rudder_stripes": ("FRA-FIN", ("203A78", "E6E4DA", "A82A26")),
}

SEATS = [(0, 0.05, 0.0, 0.0)]                               # (her pilot, under the middle of her hood)
SEAT_DROP = 0.02
TAIL_CUT = -3.1                                             # (where her tail comes away: behind her roundel)


def install():
    import planes
    planes.land(__import__(__name__))
