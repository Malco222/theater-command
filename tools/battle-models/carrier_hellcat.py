"""The F6F-3 Hellcat laid out again to a measured drawing, and her glazing (Brian, 2026-11-19: "use this 3d model is our
bench mark for all airplanes", of a B-17 model he had found; the fighters were done to it in fighters.py, and the three
carrier planes after them, each in a module of its own).

Her lines are measured from a public three-view drawing of the real aircraft at a known scale: measured, not traced,
and nothing of any drawing or model is used but its measures. The numbers are in the game's form (game/plane_numbers.py)
and her canopy in the fighters' (fighters.py's docstring): a windscreen with a flat front pane between straight posts
and a quarter pane each side, the sliding hood of flat side panes leaning in to a frame at the shoulder and a rounded
roof, and the small fixed windows in the metal of her turtledeck behind it; an open cockpit under the glass, her pilot
in it with his hands on stick and throttle (fitout.py does that from SEATS).

`install()` puts these numbers over the game's for testing (planes.py will import this module when they are taken).
"""
import math

import fighters
import fitout

NAME = "Hellcat"


# ---- the F6F-3 Hellcat ------------------------------------------------------------------------------------------
# Measured from "Grumman F6F Hellcat 3-view line drawing" by Kaboldy (Wikimedia Commons, CC BY-SA 4.0: its measures only
# are used), along her and up at 248.5 pixels to the metre, her length the real 10.24 m from the tip of her airscrew's
# hub to her rudder; across her at 252, her span the real 13.06 m (the drawing's views are not quite at one scale; its
# front view is low by a tenth of a metre at her hood, and is read for shapes only). What changed from the game's
# numbers: the bare hub of her airscrew standing well ahead of a blunt cowling, its round mouth with the engine in it
# and its deep chin, the intakes low in its face; a fuselage deepest under her cowl flaps, its belly below her wing, her
# back drawn in to a narrow turtledeck behind her hood (`taper`) and falling to a short tail cone low under her rudder;
# her broad wing, straight-tapered and cut off square, flat across its middle and turned up seven and a half degrees
# outboard of it, fifteen per cent thick at the root; her tailplane low on her tail cone, not on top of it; her tall fin
# with the fillet that runs it into her spine, and the rudder that hangs down behind her tail cone, hinged straight up
# and down. Her canopy is new (see below), her mast stands straight up on her turtledeck and a short post on the top of
# her fin, her three guns each side are where the drawing has them (the inner barrel well out ahead of the wing, the
# middle one a stub, the outer a port), and her long-range tank hangs under her middle, a metre further forward than it
# was, on the post of its rack and the two straps it hung in. Her tailwheel is gone: it folded up into her in flight.
# Her three blues over white and her stars are as they were, the stars moved onto her new surfaces where she wore them.
#
# Her fuselage: the hub (three stations, its dome and its barrel: the boss), the face of her cowling (the engine's dark
# mouth round the nose case, then its lip), her cowling and her body to the end of her tail cone. (Her stations stand
# a little under 0.6 m apart where her lines run straight, so that the map's model has no ring more than it needs.)
_F6F_HULL = [(4.30, 0.0, 0.012, 0.012), (4.265, 0.0, 0.09, 0.09), (3.77, 0.0, 0.145, 0.145),
             (3.702, 0.0, 0.16, 0.16), (3.692, 0.0, 0.42, 0.42),
             (3.666, -0.08, 0.44, 0.66), (3.655, -0.065, 0.462, 0.685), (3.60, -0.0555, 0.553, 0.726), (3.45, -0.0555, 0.655, 0.7985),
             (3.03, -0.072, 0.743, 0.876), (2.45, -0.07, 0.793, 0.958), (1.87, -0.0425, 0.796, 0.9975), (1.29, -0.013, 0.779, 1.027),
             (0.71, -0.01, 0.7515, 1.03), (0.20, 0.005, 0.725, 1.035), (-0.05, 0.0875, 0.713, 1.1125), (-0.14, 0.18, 0.704, 1.195),
             (-0.30, 0.157, 0.692, 1.168), (-0.85, 0.108, 0.655, 1.091), (-1.42, 0.061, 0.606, 1.0065), (-1.99, 0.017, 0.552, 0.919),
             (-2.56, -0.024, 0.49, 0.827), (-3.13, -0.0615, 0.42, 0.7335), (-3.70, -0.095, 0.342, 0.64), (-4.10, -0.1075, 0.29, 0.5725),
             (-4.66, -0.214, 0.205, 0.386), (-5.24, -0.309, 0.129, 0.241), (-5.815, -0.32, 0.077, 0.16)]
# (round as far back as her cockpit; behind it her back is drawn in, so that the turtledeck behind her hood is narrow and
#  the small windows in it look up and out, as the drawing's plan view has them)
_F6F_TAPER = (0.40, 0.0, [(4.30, 0.0), (0.20, 0.0), (0.05, 0.25), (-0.14, 1.0), (-5.815, 1.0)])
#
# Her canopy, measured. Its glass begins along her sides at one height, the sill, over a metal frame and the rail it
# slides on; its side panes lean in to a frame at the shoulder, and its roof rounds over between the shoulders.
_F6F_SILL = 0.875                                           # (the height her glass begins at, her hood's and her windscreen's)
_F6F_BAND = 0.05                                            # (the depth of the metal frame and the rail under it)
# Her windscreen: the foot of its front pane (x, half its width) and its head (x, its height, half its width); the
# frame between them where its quarter panes round out to her sides (x); and its rear frame (x, half its width at the
# sill, the shoulder (out, up), its top), on which her hood's front bow closes.
_F6F_SCREEN = (1.005, 0.15, 0.755, 1.35, 0.10)
_F6F_SCREEN_MID = 0.92
_F6F_SCREEN_BACK = (0.59, 0.42, (0.20, 1.31), 1.42)
# Her sliding hood (x, half its width at the sill, the shoulder (out, up), its top): its front bow, close behind the
# windscreen's frame, the frame halfway along each side (the second), its rear bow; and behind that the glass running
# on down into her turtledeck under the fixed frame there.
_F6F_HOOD = [(0.567, 0.425, (0.208, 1.312), 1.441), (0.268, 0.425, (0.208, 1.31), 1.458), (-0.074, 0.421, (0.204, 1.275), 1.42)]
_F6F_TUCK = (-0.115, 0.36, (0.13, 1.23), 1.33)


def _top(hull, x, y=0.0):
    """How high her skin stands at x, y off her middle line (her back is drawn in: fitout.hull_body)."""
    return hull.top_at(x, y) if hasattr(hull, "top_at") else fitout.body_top(hull, x, y)


def _f6f_section(hull, x, ys, shoulder, top, roof=3):
    """Half a section of her canopy (the Zero's greenhouse, on her own skin): its metal rail from her side up to the sill
    where the glass begins, `ys` out (the first SKIRT points: the rail stands on her skin wherever that is lower); a flat
    side pane leaning in from the sill to its shoulder, (out, up); and its roof, rounded over from the shoulder to her
    middle line at `top` in `roof` flat panes."""
    yb = ys + 0.004
    zb = min(_F6F_SILL - _F6F_BAND, _top(hull, x, yb) - 0.006)
    pts = [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb), (ys, _F6F_SILL), shoulder]
    y1, z1 = shoulder
    for k in range(1, roof + 1):
        a = math.radians(90.0 * k / roof)
        pts.append((0.0 if k == roof else y1 * math.cos(a), z1 + (top - z1) * math.sin(a)))
    return pts


def _f6f_canopy(hull):
    foot_x, foot_half, head_x, head_z, head_half = _F6F_SCREEN
    back_x, back_ys, back_shoulder, back_top = _F6F_SCREEN_BACK
    # her windscreen: the flat front pane from its foot on her deck up to its straight head, between its two posts; a
    # quarter pane each side from the post back to the rear frame, rounding out to her side in two flat facets; the
    # small roof pane over it from the head to the rear frame, its sides the frames that run on from the head of each
    # post to the shoulder of her hood
    rear = _f6f_section(hull, back_x, back_ys, back_shoulder, back_top, roof=2)
    head = _f6f_section(hull, head_x, back_ys - 0.02, (head_half, head_z), head_z, roof=2)
    head = head[:4] + [(0.5 * head_half, head_z), (0.0, head_z)]
    zf = _top(hull, foot_x, foot_half) - 0.01
    foot = [(foot_half, zf)] * 4 + [(0.5 * foot_half, zf), (0.0, zf)]
    t = (foot_x - _F6F_SCREEN_MID) / (foot_x - head_x)      # (the post there is on the line from its foot to its head: the front pane stays flat)
    yp, zp = foot_half + (head_half - foot_half) * t, zf + (head_z - zf) * t
    mid = _f6f_section(hull, _F6F_SCREEN_MID, back_ys - 0.02, (yp, zp), zp, roof=2)[:4] + [(0.5 * yp, zp), (0.0, zp)]
    screen = {"sections": [(foot_x, foot), (_F6F_SCREEN_MID, mid), (head_x, head), (back_x, rear)], "skirt": fighters.SKIRT, "hoops": [-1],
              "rails": [3, 2], "bars": [(0, 3, 5), (2, 3, 5)], "bar": 0.022}
    # her sliding hood: its front and rear bows, its sills and its shoulder frames, and the frame halfway along each
    # side from the sill to the shoulder; its roof clear; behind the rear bow its glass runs on down into her turtledeck
    hood = [(x, _f6f_section(hull, x, ys, sh, top)) for x, ys, sh, top in _F6F_HOOD + [_F6F_TUCK]]
    slide = {"sections": hood, "skirt": fighters.SKIRT, "hoops": [0, -2], "rails": [2, 3], "bars": [(1, 2, 3)], "bar": 0.02,
             "bare": (len(hood) - 1,)}
    return [screen, slide]


NUMBERS = {
    "title": "F6F-3 Hellcat (our own model)", "year": 1943,
    "hull": _F6F_HULL, "square": 2.2, "taper": _F6F_TAPER, "screw": 2.0, "blades": 3, "nose_back": 2.48, "spinner": 3,
    "engine": "radial", "lip": 3.70,
    "hoods": fighters.hood_of([(_F6F_SCREEN[0], 1.01, _F6F_SCREEN[1], 0.01), (_F6F_SCREEN[2], _F6F_SILL, 0.40, _F6F_SCREEN[3] - _F6F_SILL),
                               (_F6F_SCREEN_BACK[0], _F6F_SILL, _F6F_SCREEN_BACK[1], _F6F_SCREEN_BACK[3] - _F6F_SILL)]
                              + [(x, _F6F_SILL, ys, top - _F6F_SILL) for x, ys, sh, top in _F6F_HOOD]),
    "canopy": _f6f_canopy,
    "opening": (0.58, -0.08, 60.0),                         # (her cockpit, open under the glass: its padded rim inboard of the rails)
    # (the small windows in the metal of her turtledeck behind her hood: one in each side, and one over her spine between
    #  them, laid as a pair that meets on her middle line)
    "windows": [(-0.137, -0.375, 0.905, 1.26), (-0.137, -0.37, 1.30, 1.38)],
    "frames_paint": "upper",
    # (her wing: flat across its middle to the fold, then turned up seven and a half degrees; fifteen per cent thick at
    #  the root, fourteen at the fold and ten near the tip, its upper surface where the drawing's front view has it and
    #  its lower one where the side view has the section at the tip; straight-tapered, and cut off square at the tip
    #  with its corners rounded)
    "wing": [(0.0, 2.111, -1.139, -0.455, 0.57), (0.783, 2.044, -1.003, -0.445, 0.545), (1.95, 1.943, -0.801, -0.4025, 0.45),
             (3.13, 1.8404, -0.5972, -0.2521, 0.375), (4.307, 1.738, -0.394, -0.102, 0.30), (6.167, 1.581, -0.077, 0.129, 0.195),
             (6.36, 1.566, -0.04, 0.149, 0.19), (6.45, 1.507, 0.299, 0.151, 0.186), (6.526, 1.23, 0.87, 0.162, 0.02)],
    "aileron": (0.66, 0.945, 0.76), "flap": (0.12, 0.66, 0.76),
    # (her tailplane, low on her tail cone; its root cut away behind for her rudder)
    "tail": [(0.0, -3.95, -5.40, 0.16, 0.30), (0.12, -3.99, -5.41, 0.16, 0.295), (0.335, -4.05, -5.724, 0.16, 0.285), (2.28, -4.627, -5.724, 0.16, 0.16),
             (2.62, -4.725, -5.599, 0.16, 0.123), (2.79, -4.835, -5.415, 0.16, 0.088), (2.89, -5.04, -5.16, 0.16, 0.01)],
    # (her fin from below her tail cone up: its rudder the after 40 per cent of it, hinged straight up and down at x =
    #  -5.354 as the drawing has it, and hanging down behind her tail cone; its leading edge, swept back, and the fillet
    #  that runs it into her spine are a scoop ahead of it, so that the rudder's hinge stays upright)
    "fin": [(0, -4.49, -5.82, -0.20, 0.22), (0, -4.48, -5.93, -0.07, 0.24), (0, -4.55, -5.877, 0.30, 0.25), (0, -4.775, -5.745, 1.30, 0.225),
            (0, -4.85, -5.692, 1.64, 0.15), (0, -4.90, -5.561, 1.80, 0.10), (0, -5.05, -5.30, 1.883, 0.02)],
    "patch": 1.0, "colours": {"upper": "2A3850", "upper2": "55708C", "under": "D9DAD3", "nose": "2A3850", "boss": "2A2B2D"},
    "well": (1.7, 0.42, 0.42), "pitot": True,
    "guns": [(2.375, 0.05, 0.315, 0.028), (2.575, 0.05, 0.12, 0.022), (2.78, 0.05, -0.06, 0.048)],
    "scoops": [
        # (the intakes low in the face of her cowling under the engine's mouth: the deep one in the middle, and one each
        #  side of it, inside her lip)
        ([(3.67, -0.61, 0.30, 0.10), (3.45, -0.664, 0.311, 0.106), (3.00, -0.77, 0.15, 0.04)], 0.0, 2.6),
        ([(3.686, -0.37, 0.06, 0.10), (3.60, -0.40, 0.05, 0.08)], 0.33, 2.4),
        # (the fillet ahead of her fin, and its swept leading edge: a blade rising out of her spine; its foot is inside her)
        ([(-3.70, 0.4725, 0.008, 0.0725), (-4.10, 0.43, 0.10, 0.145), (-4.32, 0.466, 0.12, 0.254), (-4.50, 0.64, 0.11, 0.43),
          (-4.68, 1.02, 0.084, 0.41), (-4.90, 1.74, 0.02, 0.06)], 0.0, 2.0)],
    "mast": (-0.748, 1.20, 1.76, 0.0),                      # (straight up on her turtledeck)
    # (the long-range tank she nearly always flew with, under her middle: the post of its rack ahead, and the two straps
    #  it hung in down each side of it; and the post on the top of her fin, though not the wire from it)
    "extras": [("body", ([(2.79, -1.555, 0.02, 0.02), (2.75, -1.553, 0.091, 0.088), (2.631, -1.552, 0.161, 0.154), (2.392, -1.548, 0.246, 0.237),
                          (1.913, -1.543, 0.314, 0.301), (1.434, -1.535, 0.326, 0.313), (0.955, -1.528, 0.31, 0.299), (0.356, -1.519, 0.248, 0.239),
                          (-0.123, -1.511, 0.159, 0.153), (-0.362, -1.507, 0.104, 0.10), (-0.481, -1.505, 0.067, 0.065), (-0.54, -1.51, 0.02, 0.02)],
                         0.0, 2.0, "under")),
               ("rod", ((1.578, 0.0, -1.0), (1.578, 0.0, -1.235), 0.028, 0.028, "under")),
               ("rod", ((1.37, 0.336, -0.95), (1.37, 0.336, -1.53), 0.015, 0.015, "under")),
               ("rod", ((1.067, 0.326, -0.95), (1.067, 0.326, -1.53), 0.015, 0.015, "under")),
               ("rod", ((-5.06, 0.0, 1.86), (-5.19, 0.0, 2.15), 0.02, 0.012, "upper"))],
    "side_marks": [(-2.75, 0.34, "USA-BAR")], "wing_marks": [(4.5, 0.68, 0.5, "USA-BAR", "USA-BAR", False, False)], "code": (-3.95, 0.3),
}

SEATS = [(0, 0.12, 0.0, 0.0)]                               # (her pilot, under the highest of her hood, well back from her windscreen)
SEAT_DROP = 0.10                                            # (and down in his seat: his eyes a little over his sight, as the drawing seats him)
TAIL_CUT = -3.70                                            # (where her tail comes away: a station, behind her star and ahead of her number)
FIN_FROM = -3.9                                             # (behind this, anything above her back is her fin, its fillet and her rudder)


def coat(navy_blues):
    """Her three blues as the game paints her from 1943-02 (navy_blues comes from planes): graded over her stations
    without the step up to her turtledeck behind her hood, so that the line between her blues runs on level there."""
    return navy_blues([r for r in NUMBERS["hull"] if not -0.35 < r[0] < 0.15], NUMBERS["square"], FIN_FROM)


def install():
    import planes
    NUMBERS["graded"] = coat(planes.navy_blues)
    planes.EXTRA_PLANES[NAME] = NUMBERS
    if planes._G is not None:
        planes._G["PLANES"][NAME] = NUMBERS
    fitout.SEATS[NAME] = SEATS
    for table, key in ((fitout.HOLED, "HOLED"), (fitout.SEAT_DROP, "SEAT_DROP"), (planes.BAYS, "BAY"),
                       (planes.BAY_TORPEDO, "TORPEDO"), (planes.TAIL_CUT, "TAIL_CUT"), (planes.TURRET_LIMITS, "TURRET")):
        if key in globals():
            table[NAME] = globals()[key]
