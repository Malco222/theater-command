"""The F4F-4 Wildcat, the US Navy's fighter at Midway in June 1942, laid out to a measured drawing (the game drew her
as its generic carrier fighter until now: there was no model of her).

Her lines are measured from a public three-view drawing of the real aircraft at a known scale: measured, not traced,
and nothing of the drawing is used but its measures. The numbers are in the game's form (game/plane_numbers.py:
stations of the fuselage as (x, z of its middle, half its width, half its height), sections of a flying surface as
(how far out, leading edge, trailing edge, height, thickness)), x ahead, z up, in metres, with the airscrew's axis at
z = 0; her glazing is the fighters' `canopy` (fighters.py), a run of pieces each lofted through cross-sections whose
few points make flat panes between framed bars.

What planes.py reads from here: NAME and NUMBERS (her numbers, as EXTRA_PLANES[NAME]); SEATS (her pilot under her
glass: fitout.SEATS[NAME]) and SEAT_DROP; coat() (her three blues of 1943, NUMBERS["graded"], built on planes.navy_blues
from her hull and FIN_FROM, where her fin begins); TAIL_CUT (where her tail comes away). planes.py takes all of these
in itself (planes.carrier); install() does the same for a scratch copy of this file, for trying it.
"""
import math

import fitout
from fighters import SKIRT, hood_of

NAME = "Wildcat"


# ---- the F4F-4 Wildcat, as at Midway in June 1942 ------------------------------------------------------------------
# Measured from "Grumman F4F Wildcat 3-view line drawing" by Kaboldy (Wikimedia Commons, CC BY-SA 3.0 and 4.0: its
# measures only are used). One scale for each view, each from a dimension of the real aircraft: her side view along
# her and up at 142.8 pixels to the metre, her length the real 8.76 m from the tip of her airscrew's hub to her rudder;
# her plan view across her at 143.26, her span the real 11.58 m; her head-on view at the plan's scale, its height set
# by her airscrew's axis (it is read for her sections and her glass, and stands within 2 cm of the side view). Her nose
# is at x = 3.60, the tip of her hub; her rudder ends at x = -5.16.
#
# Her shape: the short barrel of a fuselage, deepest under her cockpit (1.80 m from her spine to her keel behind her
# hood), its sides drawn in toward her spine (`taper`) so that her narrow turtledeck and the sills of her glass stand
# where the head-on view has them, and her belly with the shallow keel between her wheels; the R-1830's cowling, its
# lip, the open mouth with the nose case and the fourteen heads of her engine in it, two rows of seven, and the
# carburettor's intake in the top of the lip and the intercoolers' ducts in its sides; the 3-blade Curtiss Electric
# airscrew with its hub and the dome of its cap; her straight-tapered mid wing, cut off square at its tips, 4.4
# degrees of dihedral from the root, the joint of her Sto-Wing fold scribed where the drawing has it with the blister
# of its hinge on top; her tailplane on the top of her rear fuselage, its root cut away behind for her rudder; her tall
# fin and rudder, with the long fillet that runs the fin's leading edge forward along her spine (a scoop ahead of a
# fin whose rudder hinges straight up and down, at x = -4.50 as drawn).
#
# Her glass: a windscreen with a flat front pane, its arched head framed, and a quarter pane each side, back to its
# frame; the sliding hood, its side panes leaning in to the frame at its shoulder and its roof rounded over, a frame
# halfway along it; the rails it slides back on along her turtledeck; a small rear-vision window each side of her
# turtledeck and a window each side of her belly under her seat. Her pilot sits in the open cockpit under the glass,
# his hands on stick and throttle, his reflector sight before him. Her six .50s are in her wings, three a side, their
# ports in the leading edge where the drawing has them. Her wheels are up: each main wheel lies in its well in the side
# of her fuselage behind the cowling, the side of its tyre about flush with her skin and its hub sunk in the well; her
# tailwheel is fixed, its fork raked back from the fairing under her tail, and her hook lies stowed under her tail. Her
# mast rakes forward from her turtledeck (no wire), a short post stands on the top of her fin, where the wire ran to,
# and another under her belly; her pitot stands under her port wingtip; the two racks under her wings are empty.
#
# Her fuselage: the hub (its cap, the collar behind it, the hub round the blades' roots, and the shaft into the engine's
# face: the boss), the face of her cowling (the engine's dark mouth, open to 0.45 m), its lip, her cowling and her
# body to the end of her tail cone. (Behind the cowling her stations stand about 0.6 m apart where her lines run
# straight; each was read off the curve through the drawing's readings, every 0.14 m along her.)
_HULL = [(3.600, 0.0, 0.012, 0.012), (3.578, 0.0, 0.046, 0.046), (3.530, 0.0, 0.065, 0.065), (3.405, 0.0, 0.067, 0.067),
         (3.400, 0.0, 0.09, 0.09), (3.262, 0.0, 0.09, 0.09), (3.256, 0.0, 0.13, 0.13), (3.092, 0.0, 0.13, 0.13),
         (3.086, 0.0, 0.075, 0.075), (2.953, 0.0, 0.075, 0.075),
         (2.951, 0.0, 0.45, 0.45), (2.946, 0.012, 0.50, 0.515), (2.928, 0.026, 0.535, 0.56), (2.844, 0.0225, 0.579, 0.60),
         (2.704, 0.010, 0.628, 0.641), (2.494, 0.005, 0.670, 0.695), (2.213, 0.0015, 0.705, 0.7405), (2.00, 0.008, 0.719, 0.763),
         (1.80, 0.003, 0.717, 0.788), (1.30, 0.023, 0.690, 0.814), (0.95, 0.037, 0.677, 0.833), (0.55, 0.048, 0.677, 0.845),
         (0.15, 0.08, 0.686, 0.85), (-0.15, 0.144, 0.697, 0.896), (-0.31, 0.208, 0.702, 0.951), (-0.60, 0.194, 0.705, 0.911),
         (-1.20, 0.173, 0.675, 0.833), (-1.80, 0.146, 0.603, 0.734), (-2.40, 0.111, 0.521, 0.616), (-2.70, 0.097, 0.476, 0.554), (-2.97, 0.114, 0.430, 0.524),
         (-3.50, 0.14, 0.335, 0.46), (-4.00, 0.13, 0.23, 0.36), (-4.50, 0.054, 0.13, 0.196), (-4.85, 0.045, 0.05, 0.12),
         (-4.885, 0.055, 0.035, 0.10), (-4.91, 0.055, 0.018, 0.07), (-4.925, 0.055, 0.004, 0.03)]
# (round at her cowling; from behind it her sides are drawn in toward her spine, most under her hood and along her
#  turtledeck, where the head-on view has the sills of her glass and the drawing's plan her rear-vision windows; her
#  tail cone ends round under the foot of her rudder, at her tail light)
_TAPER = (0.2, -0.3, [(3.6, 0.0), (1.30, 0.0), (0.95, 0.11), (0.83, 0.18), (0.70, 0.264), (0.567, 0.296), (0.47, 0.38), (0.39, 0.423),
                      (0.078, 0.602), (-0.31, 0.973), (-0.62, 0.85), (-0.78, 0.92), (-1.2, 0.9), (-2.97, 0.9), (-3.6, 0.6), (-4.93, 0.3)])
# (and below her middle she is an oval from her cowling's lip back to behind her hood: her cowling is a ring and her
#  belly round, as the head-on view has them; her airscrew's hub is as it was)
_ROUND_BELOW = [(2.96, 0.0), (2.951, 1.0), (-0.3, 1.0), (-0.6, 0.0)]
#
# Her windscreen: the flat front pane, from its foot on her deck (x, half its width, height) up to the springing of its
# arched head (x, height), leaning back as the drawing has it; the arch's top (x, height); its quarter panes' foot along
# her deck (x, half its width, height), back to the windscreen's rear frame.
_SCREEN_FOOT = (0.967, 0.16, 0.872)
_SCREEN_SPRING = (0.69, 1.1185)
_SCREEN_ARCH = (0.567, 1.232)
_SCREEN_SILL = [(0.967, 0.16, 0.872), (0.83, 0.262, 0.828), (0.69, 0.305, 0.80), (0.567, 0.322, 0.787), (0.405, 0.328, 0.765)]
_SCREEN_BACK = (0.405, (0.168, 1.105), 1.232)               # (its rear frame: x, its shoulder (out, up), its top)
# Her sliding hood (x, the height of its sill, how far out the glass is there, the frame at its shoulder (out, up), its
# top): its front bow, the frame halfway along it, its rear bow; and behind that the glass tucked down into her
# turtledeck, under the turtledeck's skin.
_HOOD = [(0.39, 0.753, 0.335, (0.174, 1.115), 1.236), (0.078, 0.725, 0.335, (0.174, 1.105), 1.215), (-0.31, 0.697, 0.335, (0.172, 1.085), 1.18)]
_TUCK = (-0.37, 0.68, 0.29, (0.13, 1.02), 1.11)


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


def _f4f_canopy(hull):
    fx, fy, fz = _SCREEN_FOOT
    sx, sz = _SCREEN_SPRING
    slope = (sz - fz) / (fx - sx)                           # (the front pane's lean: it rises so much for every metre aft)

    def pane(x):
        return fz + slope * (fx - x)
    sill = {x: (y, z) for x, y, z in _SCREEN_SILL}
    # her windscreen: the front pane from its straight foot on her deck up to the springing of its arch, flat between
    # its posts; above that its arched head, framed; a quarter pane each side from the post back to the rear frame, its
    # foot along her deck and its top the frame that runs back level from the arch to the hood's shoulder
    zf = _top(hull, fx, fy) - 0.01
    foot = [(fy, zf)] * 4 + [(fy * 2.0 / 3.0, zf), (fy / 3.0, zf), (0.0, zf)]
    mid_x = 0.83
    zm = pane(mid_x)
    post = fy + 0.003 * (fx - mid_x) / (fx - sx)
    mid = _section(hull, mid_x, sill[mid_x][1], sill[mid_x][0], (post, zm), zm)[:4] + [(post * 2.0 / 3.0, zm), (post / 3.0, zm), (0.0, zm)]
    spring = _section(hull, sx, sill[sx][1], sill[sx][0], (0.165, sz), sz)[:4] + [(0.11, sz), (0.055, sz), (0.0, sz)]
    ax, az = _SCREEN_ARCH
    arch = _section(hull, ax, sill[ax][1], sill[ax][0], (0.166, 1.12), az)
    bx, bshoulder, btop = _SCREEN_BACK
    back = _section(hull, bx, sill[bx][1], sill[bx][0], bshoulder, btop)
    screen = {"sections": [(fx, foot), (mid_x, mid), (sx, spring), (ax, arch), (bx, back)], "skirt": SKIRT, "hoops": [-1],
              "rails": [3, SKIRT], "bars": [(0, 3, 6), (3, 3, 6)], "bar": 0.02}
    # her sliding hood, shut: its front bow, the frame halfway along it and its rear bow; its sills and its shoulder
    # frames; behind the rear bow its glass runs on down into her turtledeck
    hood = [(x, _section(hull, x, sl, ys, sh, top)) for x, sl, ys, sh, top in _HOOD + [_TUCK]]
    slide = {"sections": hood, "skirt": SKIRT, "hoops": [0, 1, 2], "rails": [SKIRT, 3], "bar": 0.019, "bare": (len(hood) - 1,)}
    return [screen, slide]


# Her wing: straight-tapered, its leading edge at x = 1.667 - 0.0454 y and its trailing edge at -0.941 + 0.1387 y,
# 4.4 degrees of dihedral from the root (its middle at 0.031 + 0.0773 (y - 1)), 15 per cent thick at the root and 9 near
# the tip; cut off square at the tip, its corners rounded, the trailing one more than the leading.
_WING = [(0.0, 1.667, -0.941, -0.046, 0.512), (0.80, 1.631, -0.830, 0.0155, 0.461), (3.70, 1.499, -0.428, 0.240, 0.274),
         (5.52, 1.416, -0.175, 0.380, 0.157), (5.665, 1.388, -0.149, 0.391, 0.148), (5.741, 1.317, 0.27, 0.397, 0.12),
         (5.772, 1.20, 0.60, 0.40, 0.08), (5.79, 1.02, 0.80, 0.401, 0.03)]


def _wing_top(y, x, wing=_WING):
    """How high her wing's upper skin stands at y out, x along (the sculptor's aerofoil, between her sections)."""
    for a, b in zip(wing, wing[1:]):
        if a[0] <= y <= b[0]:
            t = (y - a[0]) / (b[0] - a[0])
            lead, trail, z, thick = (a[k] + (b[k] - a[k]) * t for k in (1, 2, 3, 4))
            c = min(max((lead - x) / (lead - trail), 0.0), 1.0)
            fo = 10.0 * (0.2969 * math.sqrt(c) - 0.126 * c - 0.3516 * c ** 2 + 0.2843 * c ** 3 - 0.1036 * c ** 4)
            return z + 0.5 * fo * thick
    raise ValueError(y)


# The joint of her Sto-Wing fold across the top of each wing, (x, y out), from her leading edge back past the blister
# of the fold's hinge, in under it toward her root and out again to her trailing edge; the blister, (x, y, half its
# length, half its breadth, how proud it stands).
_FOLD = [(1.600, 1.762), (1.283, 1.829), (1.073, 1.839), (0.920, 1.846), (0.808, 1.790), (0.794, 1.658), (0.773, 1.379), (0.724, 1.340),
         (0.585, 1.298), (0.375, 1.298), (0.166, 1.319), (-0.044, 1.354), (-0.253, 1.407), (-0.463, 1.473), (-0.672, 1.546), (-0.745, 1.569)]
_BLISTER = (0.801, 1.853, 0.12, 0.056, 0.03)


def _blister():
    """The blister's stations, lying on her wing's upper skin (half of it inside the wing)."""
    x, y, half, wide, high = _BLISTER
    return [(x + half * f, _wing_top(y, x + half * f) - 0.004, wide * w + 0.003, high * w + 0.002)
            for f, w in ((1.0, 0.0), (0.75, 0.66), (0.35, 0.94), (0.0, 1.0), (-0.35, 0.94), (-0.75, 0.66), (-1.0, 0.0))]

NUMBERS = {
    "title": "F4F-4 Wildcat (our own model)", "year": 1942,
    "hull": _HULL, "square": 2.4, "taper": _TAPER,
    # (her cowling and her belly round, not square, back under her cockpit: below her middle she is an oval)
    "round_below": _ROUND_BELOW,
    "screw": 1.49, "blades": 3, "spinner": 8, "engine": "radial", "lip": 2.97, "nose_back": 2.017,
    "gill_proud": 0.0,                                      # (her cowl's flaps closed, flush with her, as drawn)
    # (her glazing for the plain model and for where her pilot sits: the windscreen's foot, its arch and its rear frame,
    #  and her hood at its frames)
    "hoods": hood_of([(_SCREEN_FOOT[0], _SCREEN_FOOT[2], _SCREEN_FOOT[1], 0.01), (_SCREEN_ARCH[0], 0.787, 0.322, _SCREEN_ARCH[1] - 0.787),
                      (_SCREEN_BACK[0], 0.765, 0.328, _SCREEN_BACK[2] - 0.765)]
                     + [(x, sl, ys, top - sl) for x, sl, ys, sh, top in _HOOD]),
    "canopy": _f4f_canopy,
    "opening": (0.55, -0.25, 58.0),                         # (her cockpit, open under the glass from her instrument panel to behind her seat)
    "frames_paint": "upper",
    # (the small windows: a rear-vision window each side of her turtledeck, and one each side of her belly under her
    #  seat; their frames in her paint where each is, on her sides or under her)
    "skin_windows": [(-0.623, -0.777, 0.543, 0.634), (0.589, 0.267, -0.683, -0.494)],
    # (and the rails her hood slides back on, along each side of her turtledeck from its rear bow)
    "skin_rails": [(-0.33, 0.693, -0.99, 0.650)],
    "wing": _WING,
    # (her ailerons from the outer end of her flaps to 0.3 m short of the joint of her wingtip, a fixed panel of her
    #  trailing edge beyond them; her split flaps from her side out to them)
    "aileron": (0.64, 0.918, 0.74), "flap": (0.13, 0.64, 0.76),
    # (her tailplane on the top of her rear fuselage, flat; its root cut away behind for her rudder; its tips rounded)
    "tail": [(0.0, -3.58, -4.48, 0.578, 0.105), (0.20, -3.638, -4.653, 0.578, 0.104), (0.283, -3.662, -4.867, 0.578, 0.103),
             (0.345, -3.68, -5.104, 0.578, 0.102), (1.10, -3.90, -5.032, 0.578, 0.09), (1.90, -4.133, -4.957, 0.578, 0.075),
             (2.003, -4.162, -4.947, 0.578, 0.07), (2.05, -4.205, -4.915, 0.578, 0.06), (2.068, -4.27, -4.84, 0.578, 0.045),
             (2.075, -4.42, -4.68, 0.578, 0.02)],
    # (her elevators from the outer corner of the notch for her rudder out to the drawing's end of them, near her tips:
    #  their front edge, the hinge, runs straight, and the axis they turn on lies along it)
    "elevator": (0.1663, 0.968, 0.48),
    # (her fin from the foot of her rudder up, the foot lying down on her tail cone as drawn: the rudder the after 64
    #  per cent of it, hinged straight up and down at x = -4.50 where the drawing has it; the leading edge, swept back
    #  and running down into the long fillet along her spine, is a scoop ahead of it, so that the hinge stays upright)
    "fin": [(0, -4.281, -4.89, 0.155, 0.15), (0, -4.259, -4.929, 0.193, 0.15), (0, -4.160, -5.104, 0.333, 0.15), (0, -4.130, -5.157, 0.403, 0.148),
            (0, -4.137, -5.143, 0.543, 0.145), (0, -4.156, -5.111, 0.823, 0.135), (0, -4.166, -5.094, 1.103, 0.125),
            (0, -4.184, -5.062, 1.383, 0.115), (0, -4.200, -5.034, 1.663, 0.10), (0, -4.223, -4.993, 1.803, 0.09),
            (0, -4.275, -4.90, 1.85, 0.08), (0, -4.331, -4.80, 1.88, 0.06), (0, -4.399, -4.68, 1.898, 0.02)],
    "rudder": (0.0, 1.0, 0.36),
    "patch": 1.0, "colours": {"upper": "566A78", "upper2": "566A78", "under": "A8AEAD", "nose": "566A78", "boss": "8E9092", "glare": "141516"},
    # (the carburettor's intake, dark, in the top of the cowling's lip)
    "glare": (2.950, 2.938, 0.19),
    # (the fourteen heads of her R-1830, two rows of seven, in the open mouth of her cowl)
    "heads": (7, 2, 0.36, 0.075, 0.04), "mouth": 0.45,
    # (her exhaust each side, low behind the cowl and just ahead of her wing's root, short and flat to her skin)
    "exhaust": (1.95, -8.0, 0.22, 0.01, 0.028),
    # (her tailwheel: its fork raked back from the foot of the fairing under her tail to its axle)
    "tailwheel": (-3.826, -0.595), "tailwheel_leg": (-3.69, -0.43),
    # (her pitot under her port wingtip, the share of her span it stands at; her wingtip lamps on the tip itself, at
    #  (x, y out) where the drawing has them)
    "pitot": 0.965, "lamps": (0.88, 5.79),
    # (her guns: three .50s in each wing, out past the fold, their ports in its leading edge)
    "guns": [(1.986, 0.05, 0.0, 0.025), (2.185, 0.05, 0.0, 0.025), (3.082, 0.05, 0.0, 0.025)],
    # (her main wheels drawn up into her sides: (x, z of the middle of each as the side view has it, the tyre's radius,
    #  the hub's, how much the side view squeezes the circle up and down: the wheel lies in her side, which leans in);
    #  each a wheel of its own on the fine model, in a well cut into her skin)
    "side_wheels": (1.350, -0.408, 0.327, 0.212, 0.91),
    # (her Sto-Wing fold: the joint across the top and the bottom of each wing)
    "fold": _FOLD,
    "hull_lines": [2.70, 2.03, -0.57, -0.97, -1.375, -2.24, -2.65, -3.056, -3.50],
    "scoops": [
        # (the fin's swept leading edge, and the fillet that runs it forward along her spine: a blade rising out of her
        #  back; its foot is inside her)
        ([(-2.85, 0.60, 0.004, 0.04), (-3.109, 0.5855, 0.02, 0.0655), (-3.389, 0.616, 0.033, 0.116), (-3.669, 0.6785, 0.042, 0.2285),
          (-3.879, 0.8055, 0.048, 0.4055), (-4.019, 0.93, 0.05, 0.53), (-4.159, 1.081, 0.046, 0.631), (-4.299, 1.685, 0.03, 0.185),
          (-4.34, 1.84, 0.008, 0.04)], 0.0, 2.0)],
    "mast": (-1.162, 1.0, 1.537, -0.224),                   # (raked forward from her turtledeck)
    "extras": [
        # (the shallow keel under her belly between her wheels)
        #  (falling steadily aft to its deepest under her wing's leading edge, as drawn)
        ("body", ([(1.76, -0.80, 0.02, 0.02), (1.62, -0.79, 0.08, 0.065), (1.40, -0.80, 0.10, 0.085), (1.20, -0.815, 0.10, 0.105),
                   (0.95, -0.83, 0.10, 0.12), (0.84, -0.83, 0.095, 0.105), (0.75, -0.825, 0.075, 0.08), (0.70, -0.815, 0.06, 0.06),
                   (0.64, -0.80, 0.02, 0.02)], 0.0, 2.0, "under")),
        # (the fairing under her tail that her tailwheel's fork hangs from)
        ("body", ([(-3.50, -0.28, 0.01, 0.03), (-3.56, -0.31, 0.04, 0.07), (-3.64, -0.35, 0.045, 0.11), (-3.70, -0.355, 0.045, 0.115),
                   (-3.78, -0.335, 0.045, 0.095), (-3.87, -0.31, 0.045, 0.085), (-3.91, -0.27, 0.01, 0.03)], 0.0, 2.0, "under", "fine")),
        # (the two racks under her wings, empty)
        ("body", ([(0.72, -0.13, 0.02, 0.02), (0.66, -0.145, 0.07, 0.065), (0.55, -0.152, 0.095, 0.082), (0.25, -0.152, 0.095, 0.082),
                   (0.08, -0.145, 0.07, 0.06), (0.02, -0.13, 0.02, 0.02)], 1.309, 2.0, "under", "fine")),
        # (the blister over each wing's fold hinge)
        ("body", (_blister(), _BLISTER[1], 2.0, "upper", "fine")),
        # (the intercoolers' ducts in the sides of the cowling's lip, dark)
        ("body", ([(2.957, 0.0, 0.004, 0.02), (2.955, 0.0, 0.028, 0.10), (2.949, 0.0, 0.028, 0.10), (2.947, 0.0, 0.004, 0.02)], 0.485, 2.0, "engine", "fine")),
        # (the engine's nose case in the open mouth of her cowl, inside her heads, back to the engine; and the collar of
        #  her airscrew's hub behind its blades)
        ("rod", ((2.62, 0.0, 0.0), (2.955, 0.0, 0.0), 0.25, 0.20, "engine", "fine")),
        ("rod", ((2.953, 0.0, 0.0), (3.089, 0.0, 0.0), 0.092, 0.092, "boss", "fine")),
        # (the short post on the top of her fin, on the fin just ahead of her rudder's hinge, so that it stays still as
        #  the rudder turns; the post under her belly; and the hook stowed under her tail, its point at her end)
        ("rod", ((-4.48, 0.0, 1.88), (-4.48, 0.0, 2.055), 0.012, 0.008, "frame", "fine")),
        ("rod", ((-0.588, 0.0, -0.70), (-0.588, 0.0, -1.10), 0.012, 0.008, "frame", "fine")),
        ("rod", ((-4.45, 0.0, -0.115), (-4.84, 0.0, -0.097), 0.016, 0.016, "leg", "fine")),
        ("rod", ((-4.84, 0.0, -0.097), (-4.885, 0.0, -0.122), 0.024, 0.016, "leg", "fine"))],
    # (her star on each side, her number ahead of it as her squadrons painted it in 1942; and a star on each wing, top
    #  and bottom, as from January 1942 until February 1943 (the file has no month for the second pair to go: in her
    #  later coats she keeps all four))
    "side_marks": [(-1.95, 0.40, "USA")], "wing_marks": [(4.3, 0.62, 0.52, "USA", "USA", True, True)], "code": (-1.2, 0.30),
}

SEATS = [(0, 0.02, 0.0, 0.0)]                               # (her pilot, under the middle of her hood)
SEAT_DROP = 0.04                                            # (and down in his seat: his eyes at his sight, as the drawing seats him)
TAIL_CUT = -2.6                                             # (where her tail comes away: behind her star, and her number ahead of it)
FIN_FROM = -2.85                                            # (behind this, anything above her back is her fin, its fillet and her rudder)


def coat(navy_blues):
    """Her three blues as the game paints her from 1943-02 (navy_blues comes from planes): graded over her stations
    without the step up to her turtledeck under her hood, so that the line between her blues runs on level there; her
    tailplane sea blue on top as her wings are (it stands on the top of her slim tail cone, where the field would take
    it for fin)."""
    blues = navy_blues([r for r in _HULL if not -0.35 < r[0] < 0.90], NUMBERS["square"], FIN_FROM)
    tail_z = NUMBERS["tail"][0][3]

    def graded(p):
        if p.x < FIN_FROM and abs(p.z - tail_z) < 0.07 and abs(p.y) > 0.075:
            return -1.0                                     # (her tailplane and elevators)
        return blues(p)
    return graded


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
