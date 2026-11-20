"""The SBD-3 Dauntless laid out again to a measured drawing, with the long greenhouse of framed flat panes over her two
men (Brian, 2026-11-19: "use this 3d model is our bench mark for all airplanes", of a B-17 model he had found; the
carrier planes follow the B-17 and the four fighters of fighters.py).

Her lines are measured from a public three-view drawing of the real aircraft at a known scale: measured, not traced,
and nothing of the drawing is used but its measures. The numbers are in the game's form (see game/plane_numbers.py
and fighters.py's docstring), x ahead, z up, in metres, with the airscrew's axis at z = 0 and her spinner's tip where
the game's numbers had it, at x = 3.95.

Her glazing is fighters.py's `canopy`: a run of pieces, each a loft through half-sections of a few points, so that the
panes between them are flat and the bars between panes are framed (the Zero's greenhouse, `fighters.green`, is the
pattern). She is shown as she flew into a fight: the pilot's hood shut; the gunner's hood run forward along its rails
until it lies inside the fixed middle of the greenhouse, his place open to the air behind it, and his two guns up.

planes.py registers her from this module as `install()` does: her numbers by her name, and her tables.
"""
import fighters
import fitout
from fighters import SKIRT, green, hood_of

NAME = "Dauntless"

# ---- the SBD-3 Dauntless ------------------------------------------------------------------------------------------
# Measured from "Douglas SBD-3 Dauntless 3-view line drawing" (Wikimedia Commons, Kaboldy, CC BY-SA 3.0: its measures
# only are used), along her and up at 253.1 pixels to the metre, her length 10.09 m from spinner to rudder; across her
# at 256.5, her span the real 12.66 m (the drawing's views are not quite at one scale). What changed from the game's
# numbers: a small pointed spinner and the hub behind it; the cowling's open mouth with the engine's face deep in it
# and the carburettor's intake in the top of its lip, the cowling swelling behind its lip, a little wider than deep at
# the front and deeper than wide at its back edge, where it stands proud of the fuselage; a deep fuselage, its keel a
# hand's breadth under the wing all along the cockpit, tapering to a tail that is still deep at the rudder post, where
# she ends: the rounded foot behind the post is the foot of her rudder, and swings with it; the wing to its measured
# outline: a straight centre section with no dihedral, its trailing edge swept back into her side in the fillet at its
# root, and tapered outer panels with ten degrees of dihedral and round tips, fifteen per cent thick at the root and
# ten at the tip; the perforated split flaps from her side to the ailerons, their holes in three rows where the
# drawing has them; the tailplane flat, low on her tail; the fin and rudder to their measured outline, with the long
# dorsal fillet that runs the fin's leading edge forward along her spine. Her greenhouse is of flat panes in frames: a
# windscreen with a flat front pane between straight posts, each side pane folded along its diagonal bar into two flat
# panes; the pilot's sliding hood, its sides leaning in to a frame at the shoulder, its roof rounded over; the fixed
# middle with the turnover frame behind it; the gunner's hood run forward inside it, and the folding fairing behind his
# hood folded down, to give his raised guns their room over her spine: his place is open back to its end. Her bomb
# hangs on the crutch that swings it clear of the airscrew, its pivot on a strut down from her belly; her radio mast
# stands on her port side ahead of the windscreen, where the drawing has it, not behind the pilot; the telescope sight
# runs out through the front pane; the hook lies along her belly under her tail and the fixed tailwheel stands where
# the drawing has it. Her blue-grey over light grey is as it was, and her stars and her number are the game's, of the
# game's sizes, set on her new surfaces: the fuselage star midway between the wing's root and the tailplane, the
# wing's star on the middle of its new chord, the number ahead of the tailplane, clear of its root.
#
# Her canopy, measured: (the height of its sill where the glass begins, how far out the glass is there, the frame at
# its shoulder (out, up), the height of its top). The pilot's hood stands from x 1.19 to 0.315, the fixed middle from
# 0.315 to -0.337 with the turnover frame behind it to -0.396, the gunner's hood (shut) from -0.39 to -1.0.
_PILOT = (0.648, 0.43, (0.246, 1.03), 1.205)
_MIDDLE = (0.612, 0.41, (0.234, 1.0), 1.17)
_TURNOVER = (0.60, 0.42, (0.244, 1.005), 1.185)
# (the gunner's hood: its sill and how far out its glass is there; its front bow and its back frame as they stand
#  shut, each (x along her, the frame at its shoulder (out, up), the height of its top): its top falls toward its back
#  as the greenhouse runs down into her spine. It is smaller than the fixed middle and slides forward under it.)
_GUNNER_SILL, _GUNNER_OUT = 0.636, 0.386
_GUNNER = [(-0.39, 0.218, 1.01, 1.158), (-1.0, 0.196, 0.935, 1.075)]
_GUNNER_SLID = 0.68                                         # (how far forward his hood is run, to lie inside the middle)
# (her windscreen: the foot of its front pane and half its width there; its head, half its width there and its
#  height; the height of the foot of its side panes; its back frame, where the pilot's hood begins; and how far
#  along her the diagonal bar of each side pane has its foot, on the sill)
_SCREEN = (1.729, 0.21, 1.37, 0.22, 1.118, 0.78, 1.19)
_BAR_FOOT = 1.461


def _ws_rear(hull, x):
    """The back of her windscreen: the pilot's hood's section, cut off at the foot of the side panes."""
    sill, ys, (sy, sz), top = _PILOT
    foot = _SCREEN[5]
    y = ys + (sy - ys) * (foot - sill) / (sz - sill)
    return green(hull, x, foot, y, (sy, sz), top)


def _sbd_canopy(hull):
    foot_x, foot_half, head_x, head_half, head_z, side_foot, back_x = _SCREEN
    rear = _ws_rear(hull, back_x)                           # (seven points: the rail's two, the sill, the shoulder, the roof's three)
    # her windscreen, of flat panes: the front pane from its straight foot on her deck up to its straight head between
    # the posts; each side pane from the post back to the back frame, folded along its diagonal bar into two flat panes,
    # the one ahead of the bar and over it, down to the front pane's corner, and the one behind it; its roof from the
    # head back to the back frame, where it is as round as the hood's. The head's section leans, so that its sill, where
    # the bar has its foot, stands where the drawing has the bar's foot, and its head still stands over the front pane.
    # The bar's foot is as far out as lays the pane ahead of the bar flat (0.237 m: the drawing's plan has the bar 0.24
    # out). Each side's sill is given twice at the head and its shoulder twice at the back frame, so that the loft lays
    # the two panes as the two triangles they are, folded on the bar.
    zf = fighters.deck(hull, foot_x, foot_half)[1]
    corner, head, shoulder = (foot_x, foot_half, zf), (head_x, head_half, head_z), (back_x, rear[3][0], rear[3][1])
    a, b = [h - c for h, c in zip(head, corner)], [s - c for s, c in zip(shoulder, corner)]
    n = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    sill_y = corner[1] - (n[0] * (_BAR_FOOT - corner[0]) + n[2] * (side_foot - corner[2])) / n[1]
    lean = (head_x - _BAR_FOOT) / (head_z - side_foot)
    yb = sill_y + 0.004
    zb = min(side_foot - 0.06, fitout.body_top(hull, _BAR_FOOT, yb) - 0.006)
    front = [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb), (sill_y, side_foot), (sill_y, side_foot), (head_half, head_z)]
    front += [(head_half * (1.0 - k / 3.0), head_z) for k in range(1, 4)]
    x0 = _BAR_FOOT - lean * (side_foot - (zb - 0.06))       # (the section leans from its lowest point)
    back = rear[:4] + rear[3:]
    foot = [(foot_half, zf)] * (len(back) - 1) + [(0.0, zf)]       # (straight: its middle lies inside her deck's crown)
    screen = {"sections": [(foot_x, foot), (x0, front, lean), (back_x, back)], "skirt": SKIRT, "hoops": [-1], "rails": [4, SKIRT],
              "bars": [(1, 4, len(back) - 1)], "struts": [((1, 2), (2, 3))], "bar": 0.02}
    # the pilot's sliding hood, shut: its front bow, its middle post and its back frame; its sills and shoulder frames
    sill, ys, shoulder, top = _PILOT
    hood = {"sections": [(x, green(hull, x, sill, ys, shoulder, top)) for x in (back_x - 0.008, 0.75, 0.315)], "skirt": SKIRT,
            "hoops": [0, 1, 2], "rails": [SKIRT, 3], "bar": 0.018}
    # the fixed middle, from under the hood's back frame to the turnover frame, with a frame across its middle (its front
    # edge has no frame of its own: the hood's back frame stands over it), and the turnover frame behind it
    sill, ys, shoulder, top = _MIDDLE
    middle = {"sections": [(x, green(hull, x, sill, ys, shoulder, top)) for x in (0.34, -0.035, -0.337)], "skirt": SKIRT,
              "hoops": [1, 2], "rails": [SKIRT, 3], "bar": 0.018}
    sill, ys, shoulder, top = _TURNOVER
    turnover = {"sections": [(x, green(hull, x, sill, ys, shoulder, top)) for x in (-0.345, -0.37, -0.395)], "skirt": SKIRT,
                "hoops": [1], "rails": [], "bar": 0.025}
    # the gunner's hood, run forward inside the middle (its frames all but on the middle's): its front bow and back frame
    gunner = {"sections": [(x + _GUNNER_SLID, green(hull, x + _GUNNER_SLID, _GUNNER_SILL, _GUNNER_OUT, (sy, sz), t)) for x, sy, sz, t in _GUNNER],
              "skirt": SKIRT, "hoops": [0, 1], "rails": [SKIRT, 3], "bar": 0.016}
    return [screen, hood, middle, turnover, gunner]


# Her fuselage: (x, the height of its middle, half its width, half its depth). The drawing was read every 0.25 to 0.5 m
# and closer at the spinner, the lip and the tail; the stations kept are as few as hold the curve through all of those
# readings to within a centimetre (behind the cowling, evenly 0.565 m apart), so that her plain model, which rings her
# at every station, is no heavier than it was.
_HULL = [
    # (her spinner, its back plate and the hub behind it, the shaft into the engine's face)
    (3.95, 0.0, 0.012, 0.012), (3.891, 0.0, 0.063, 0.063), (3.662, 0.0, 0.17, 0.17), (3.652, 0.0, 0.15, 0.15), (3.50, 0.0, 0.147, 0.147),
    (3.414, 0.0, 0.08, 0.08),
    # (the face of the engine in the cowling's mouth, the lip, and the cowling behind it to its back edge, proud of her side)
    (3.413, 0.006, 0.50, 0.50), (3.396, 0.012, 0.556, 0.556), (3.377, 0.025, 0.606, 0.618), (3.338, 0.024, 0.657, 0.656),
    (3.18, 0.008, 0.725, 0.715), (2.60, -0.006, 0.698, 0.749),
    # (the fuselage behind it, its deck rising to the windscreen; under the greenhouse; the turtle deck behind)
    (2.585, -0.01, 0.675, 0.737), (2.231, -0.006, 0.667, 0.768), (1.73, -0.01, 0.643, 0.812), (1.165, -0.03, 0.616, 0.811), (0.6, -0.031, 0.6, 0.811),
    (0.035, -0.022, 0.586, 0.802), (-0.53, 0.006, 0.569, 0.774), (-1.095, 0.047, 0.555, 0.743), (-1.66, 0.093, 0.541, 0.696), (-2.225, 0.133, 0.523, 0.602),
    (-2.79, 0.16, 0.491, 0.52), (-3.355, 0.163, 0.434, 0.462), (-3.92, 0.169, 0.378, 0.408), (-4.485, 0.178, 0.33, 0.356), (-4.88, 0.198, 0.275, 0.336),
    # (under her tailplane she narrows into her rudder post, where she ends in a knife edge, as thick as the rudder's
    #  foot there: the rounded foot behind the post is her rudder's, and is the fin's)
    (-5.33, 0.221, 0.165, 0.31),
    (-5.355, 0.221, 0.006, 0.30)]
_POST = -5.355

NUMBERS = {
    "title": "SBD-3 Dauntless (our own model)", "year": 1941,
    "hull": _HULL,
    "square": 2.3, "screw": 1.65, "blades": 3, "spinner": 5, "engine": "radial", "lip": 3.425,
    "nose_back": 2.7,                                       # (her cowl flaps, just ahead of the cowling's proud back edge)
    # (her glazing for the plain model and for where her men sit: the windscreen's foot and head, the pilot's hood, the
    #  fixed middle and the gunner's hood as it stands shut, at their frames; and the folding fairing behind his hood,
    #  folded down to her turtle deck, over the back of his open place)
    "hoods": hood_of([(_SCREEN[0], 0.80, _SCREEN[1], 0.01), (_SCREEN[2], 0.78, 0.28, _SCREEN[4] - 0.78)]
                     + [(x, _PILOT[0], _PILOT[1], _PILOT[3] - _PILOT[0]) for x in (_SCREEN[6], 0.315)]
                     + [(-0.337, _MIDDLE[0], _MIDDLE[1], _MIDDLE[3] - _MIDDLE[0])]
                     + [(x, _GUNNER_SILL, out, t - _GUNNER_SILL) for x, out, t in ((-0.725, 0.386, 1.158), (-1.0, 0.356, 1.075), (-1.48, 0.29, 0.80))]),
    "canopy": _sbd_canopy,
    # (her two cockpits, open under the glass and behind it: from, to, and from 55 degrees round her; it ends in front
    #  under the pilot's hood, where the glass is still wider than the rim, and behind at the end of the folding
    #  fairing, so that the gunner's mounting and his legs are inside it)
    "opening": (1.16, -1.48, 55.0),
    "frames_paint": "upper",
    # (her wing: a straight centre section, its trailing edge swept back into her side in the fillet at its root;
    #  tapered outer panels with ten degrees of dihedral and round tips; fifteen per cent thick at the root, ten at the
    #  tip. A section stands where the flap ends and the aileron begins, so that the plain model needs no ring between.)
    "wing": [(0.0, 2.01, -1.75, -0.575, 0.53), (0.54, 1.918, -1.72, -0.575, 0.53), (0.596, 1.909, -1.534, -0.575, 0.53),
             (0.791, 1.876, -1.285, -0.575, 0.53), (1.0, 1.84, -1.222, -0.575, 0.53), (1.669, 1.726, -1.222, -0.575, 0.53),
             (1.75, 1.726, -1.209, -0.56, 0.525), (2.058, 1.693, -1.145, -0.505, 0.505), (4.2011, 1.4467, -0.6953, -0.12, 0.3268),
             (5.177, 1.335, -0.491, 0.054, 0.24), (5.567, 1.291, -0.408, 0.128, 0.202), (5.832, 1.208, -0.287, 0.175, 0.18),
             (6.074, 1.046, -0.163, 0.215, 0.14), (6.195, 0.878, -0.052, 0.23, 0.09), (6.29, 0.66, 0.16, 0.24, 0.04), (6.327, 0.45, 0.4, 0.243, 0.01)],
    "flap": (0.09, 0.664, 0.74), "aileron": (0.664, 0.961, 0.74),
    # (her tailplane, flat and low on her tail)
    "tail": [(0.0, -4.09, -5.65, 0.36, 0.14), (1.844, -4.438, -5.56, 0.36, 0.1), (2.37, -4.541, -5.513, 0.36, 0.08), (2.546, -4.651, -5.402, 0.36, 0.06),
             (2.663, -4.825, -5.228, 0.36, 0.035), (2.713, -4.99, -5.03, 0.36, 0.01)],
    # (the fin from the foot of her rudder up, its leading edge straight to the rounded top; the dorsal fillet ahead of it
    #  is a scoop. Below her spine it is the rudder's foot behind her post: the rudder's hinge, 48 per cent of the fin's
    #  chord, is on the post, the foot is as thick as she is there, and it narrows behind the post as the drawing's plan
    #  has it; ahead of the post all of it is inside her)
    "fin": [(0, -5.2212, -5.50, -0.065, 0.02), (0, -4.7042, -6.06, 0.11, 0.43), (0, -4.635, -6.135, 0.27, 0.45), (0, -4.6442, -6.125, 0.45, 0.33),
            (0, -4.17, -6.11, 0.53, 0.17), (0, -4.31, -6.04, 0.8, 0.155), (0, -4.786, -5.827, 1.632, 0.125), (0, -5.066, -5.686, 2.106, 0.085),
            (0, -5.204, -5.536, 2.224, 0.05), (0, -5.31, -5.37, 2.276, 0.01)],
    "patch": 1.0, "colours": {"upper": "566A78", "upper2": "566A78", "under": "A8AEAD", "nose": "566A78", "boss": "2A2B2D", "glare": "141516"},
    # (the carburettor's intake, dark, in the top of the cowling's lip)
    "glare": (3.40, 3.375, 0.085),
    # (her rudder from the foot of the fin to its top, hinged at 48 per cent of its chord, where the drawing's hinge is;
    #  her exhaust each side at the back edge of the cowl; her two guns in their troughs in the cowl's top; the one row
    #  of nine cylinder heads of her R-1820 in the open mouth of the cowling; her telescope sight, among her extras)
    "rudder": (0.0, 1.0, 0.48), "exhaust": (2.89, -33.0, 0.2, 0.04), "nose_guns": (23.0, -0.005),
    "heads": (9, 1, 0.40, 0.085, 0.04), "mouth": 0.50, "sight": "telescope",
    "well": (1.15, 0.42, 0.42), "well_in": True, "pitot": True, "tailwheel": (-5.46, -0.41),
    "scoops": [
        # (the dorsal fillet: a thin blade rising out of her spine into the fin's leading edge; its foot is inside her)
        ([(-2.50, 0.69, 0.004, 0.02), (-3.095, 0.6695, 0.025, 0.0695), (-4.284, 0.7375, 0.042, 0.2425),
          (-4.454, 0.7915, 0.045, 0.3065), (-4.52, 0.80, 0.02, 0.30)], 0.0, 2.0),
        # (her radio mast, a tapered blade on her port side ahead of the windscreen; its leading edge a knife's, so that
        #  it takes no mouth's dark)
        ([(2.06, 1.245, 0.0005, 0.625), (2.059, 1.245, 0.0005, 0.625), (2.02, 1.18, 0.012, 0.56), (1.98, 0.77, 0.011, 0.15),
          (1.968, 0.68, 0.005, 0.06)], -0.40, 2.0, "one")],
    "extras": [
        # (her bomb on its crutch: a thousand-pounder, its round nose, its case, its tail cone and the box of its fins; the
        #  crutch's arms from the pivot under her cowling to the lugs on its sides, and the strut each side from her belly
        #  down to the pivot: these are hers, and stay with her when the bomb goes)
        ("body", ([(1.30, -1.09, 0.03, 0.03), (1.19, -1.09, 0.14, 0.14), (1.08, -1.09, 0.205, 0.205)], 0.0, 2.0, "bomb")),
        ("rod", ((1.08, 0.0, -1.09), (0.20, 0.0, -1.09), 0.205, 0.235, "bomb")),
        ("rod", ((0.20, 0.0, -1.09), (-0.32, 0.0, -1.09), 0.235, 0.07, "bomb")),
        ("rod", ((-0.18, 0.0, -1.09), (-0.48, 0.0, -1.09), 0.225, 0.225, "bomb")),
        ("rod", ((2.03, 0.14, -0.89), (0.88, 0.17, -0.93), 0.022, 0.02, "under")),
        ("rod", ((2.44, 0.10, -0.73), (2.03, 0.14, -0.89), 0.02, 0.02, "under")),
        # (her gunner's two guns, raised; the post of their mounting down to his floor, and the bar his heels rest on: the
        #  map's model has no open place for him, so these two are on the fine model only)
        ("rod", ((-1.32, 0.075, 0.92), (-2.16, 0.075, 1.07), 0.02, 0.016, "steel")),
        ("rod", ((-1.32, 0.0, 0.78), (-1.32, 0.0, -0.28), 0.022, 0.022, "engine", "fine")),
        ("rod", ((-1.36, 0.0, -0.025), (-1.36, 0.54, -0.025), 0.025, 0.025, "engine", "fine")),
        # (the telescope sight, out through the front pane of the windscreen; the engine's nose case in the open mouth of
        #  the cowling, inside the ring of heads and back to the engine; the hook along her belly under her tail that
        #  catches the wire on the deck, from its fitting to its point: all three too small for the map's model)
        ("rod", ((1.86, 0.0, 0.90), (1.0, 0.0, 1.02), 0.03, 0.022, "engine", "fine")),
        ("rod", ((3.19, 0.0, 0.0), (3.418, 0.0, 0.0), 0.28, 0.22, "leg", "fine")),
        ("rod", ((-3.62, 0.0, -0.285), (-5.02, 0.0, -0.135), 0.02, 0.016, "leg", "fine"))],
    "side_marks": [(-2.8, 0.34, "USA")], "wing_marks": [(4.4, 0.4, 0.55, "USA", "USA", False, False)], "code": (-3.7, 0.30),
}

# Her two men: the pilot under his hood, the gunner facing aft in his open place, his guns before him.
SEATS = [(0, 0.66, 0.0, 0.0), (0, -0.72, 0.0, 180.0)]
FIN_FROM = -2.5                                            # (the foot of the dorsal fillet: all above her top behind it is fin)
# Her perforated dive flaps: the holes in three rows along each, at the shares of the chord the drawing has them and
# over the span it has them, from just outside the outer panel's joint nearly to the aileron (the drawing groups them
# three and four together; fitout spaces them evenly).
HOLED = (1.83, 4.10, (0.83, 0.895, 0.96))
# Where she parts when she breaks up: ahead of the dorsal fillet, so that the whole of it goes with her tail.
TAIL_CUT = -2.40


def coat(navy_blues):
    """Her three blues as the game paints her from 1943-02 (navy_blues comes from planes): as navy_blues lays them, and
    behind her rudder post, where she has ended, her fin's intermediate blue on all of her fin there (her rudder and the
    rudder's foot); her tailplane as it was."""
    blues = navy_blues(NUMBERS["hull"], NUMBERS["square"], FIN_FROM)
    fin = []

    def field(p):
        if p.x < _POST:
            if not fin:
                import planes
                G = planes.game()
                fin.append((G["Wing"](NUMBERS["fin"], 1.0, True), G["aerofoil"]))
            w, aerofoil = fin[0]
            if w.root <= p.z <= w.tip:
                lead, trail = w.lead(p.z), w.trail(p.z)
                c = (p.x - lead) / (trail - lead) if abs(trail - lead) > 1e-9 else 2.0
                if -0.01 <= c <= 1.01 and abs(p.y) <= 0.4 * aerofoil(min(max(c, 0.0), 1.0)) * w.thick(p.z) + 0.012:
                    return 1.0
        return blues(p)
    return field


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
