"""The TBF-1 Avenger laid out again to a measured drawing, with her long greenhouse and her turret (Brian, 2026-11-19:
"use this 3d model is our bench mark for all airplanes", of a B-17 model he had found; the B-17 and the four
single-seat fighters were done first, in planes.py and fighters.py).

Her lines are measured from a public three-view drawing of the real aircraft at a known scale: measured, not traced,
and nothing of any drawing or model is used but its measures. The numbers are in the game's form (game/
plane_numbers.py: stations of the fuselage as (x, z of its middle, half its width, half its height), sections of a
flying surface as (how far out, leading edge, trailing edge, height, thickness)), x ahead, z up, in metres, with the
airscrew's axis at z = 0; her glazing is the fighters' `canopy` (fighters.py), a run of pieces each lofted through
cross-sections whose few points make flat panes between framed bars.

What planes.py reads from here: NAME and NUMBERS (her numbers, as EXTRA_PLANES[NAME]); SEATS (who sits under her
glass: fitout.SEATS[NAME]); coat() (her three blues of 1943, NUMBERS["graded"], built on planes.navy_blues from her
hull and FIN_FROM, where her fin begins); BAY and TORPEDO (her bomb bay and the torpedo in it: planes.BAYS[NAME],
planes.BAY_TORPEDO[NAME]); TURRET (how far her turret trains: planes.TURRET_LIMITS[NAME]). planes.py takes all of these
in itself (planes.carrier); install() does the same for a scratch copy of this file, for trying it.
"""
import fighters
import fitout
from fighters import SKIRT, deck, hood_of, side_at

NAME = "Avenger"

# ---- the TBF-1 Avenger, as at Midway in June 1942 ----------------------------------------------------------------
# Measured from "Grumman TBF Avenger 3-view line drawing" by Kaboldy (Wikimedia Commons, CC BY-SA 3.0: its measures
# only are used), along her and up at 193.3 pixels to the metre, her length 12.49 m from spinner to rudder (the real
# one 12.48 m); across her at 196.7, her span the real 16.51 m (the drawing's views are not quite at one scale, and its
# head-on view stands about two per cent low: her heights are read from the side). Her dimensions were checked against
# the Bureau of Aeronautics' TBF-1/1C descriptive arrangement of 1943 (US Navy, public domain). Her nose stays where
# the game had it, her spinner's tip at x = 5.0.
#
# What changed from the game's numbers: the deep barrel of a fuselage, 2.3 m deep and 1.58 wide at her cockpit, its
# keel flat under the bay from below the cowling's skirt to behind her wing, then stepping up sharply at the window of
# the gun under her tail, and tapering to a slim tail cone at x = -6.88 (the game's ran on to -7.48); the open mouth of
# her cowling, the hub of her airscrew running back into it, and in it the nose case and the fourteen heads of her
# R-2600 in two rows of seven; the fat exhaust stub each side behind the cowl, running down, its mouth turned out; the
# long dorsal fin that rises out of her back behind the turret and runs up into her fin, widening as it goes, and the
# deep rear fuselage under her tailplane; her broad wing, its middle flat and straight, its outer panels tapering and
# turned up 5.9 degrees to rounded tips (the game's was 0.38 m low and tapered from the root), its long flaps from her
# root out to her ailerons; her tailplane, rounded at its tips, set at the top of her rear fuselage under the fin, its
# elevators hinged where the drawing has them and stopping short of the tips; her tall fin and rudder with straight
# edges, and the short aerial post on its top. Her greenhouse is new: a windscreen with a flat front pane
# between straight posts, a flat quarter pane and a small corner pane each side, its head framed level, its side posts
# raking back from the corners of its sides and its roof curving up to the pilot's sliding hood; the hood, the hood of
# the middle station behind it and the fixed glazing behind that, each with flat side panes leaning in to a frame at
# the shoulder and a roof of narrow panes, hooped where the real one was, and its glazed back leaning forward over the
# front of the turret; an open cockpit under all of it, the pilot at his controls and the man at the middle station.
# The turret is the ball on her back, 1.2 m across and standing out a little from her sides, glass above its ring and
# metal below it with a small window each side, and its one .50 (the drawing shows two; the TBF-1 had one); the .30
# looks aft and down out of a port in the step under her tail.
# The small windows of her radio compartment are in her sides; her wheels fold outward into her wings; her tailwheel,
# which retracted, is left off. Her blue-grey and her stars are the game's, the stars moved onto her new surfaces.
#
# Her fuselage, measured: the spinner and the hub, then the cowling's mouth, her barrel, the step behind her bay at
# x = -3.79 (from the keel up to the gun window), and her tail cone. (Along her middle and her tail cone the stations
# stand a little under 0.6 m apart, where the map's model puts its rings: there their values are read off the curve
# through the measured ones, and lie within 12 mm of it.)
HULL = [(5.000, 0.0, 0.015, 0.015), (4.96, 0.0, 0.073, 0.073), (4.88, 0.0, 0.107, 0.107), (4.75, 0.0, 0.116, 0.116), (4.52, 0.0, 0.135, 0.135),
        (4.37, 0.0, 0.14, 0.14), (4.345, 0.036, 0.675, 0.69), (4.30, 0.0387, 0.709, 0.731), (4.005, 0.0255, 0.767, 0.885),
        (3.64, 0.0015, 0.786, 0.995), (3.25, 0.016, 0.79, 1.054), (2.86, 0.04, 0.79, 1.093), (2.34, 0.075, 0.79, 1.128), (1.80, 0.10, 0.745, 1.153),
        (1.21, 0.11, 0.758, 1.163), (0.62, 0.11, 0.764, 1.163), (0.03, 0.107, 0.771, 1.16), (-0.54, 0.096, 0.779, 1.149), (-1.10, 0.075, 0.785, 1.128),
        (-1.62, 0.04, 0.81, 1.093), (-2.20, 0.06, 0.787, 1.098), (-2.73, 0.026, 0.748, 1.033), (-3.26, 0.010, 0.696, 0.955), (-3.79, -0.0015, 0.631, 0.8565),
        (-4.15, 0.189, 0.587, 0.569), (-4.48, 0.17, 0.546, 0.49), (-5.06, 0.0998, 0.459, 0.325), (-5.64, 0.1057, 0.351, 0.225), (-6.22, 0.1505, 0.227, 0.158),
        (-6.76, 0.204, 0.09, 0.106), (-6.88, 0.22, 0.01, 0.03)]

# Her greenhouse, measured. The windscreen: the foot of its flat front pane on her deck, x and half its width; its head,
# x and half its width there, and the height of the level frame across its head; and the corner of its side, out and
# up, at its head. (From above, the real windscreen looks round; its panes were flat, and so are these.)
SCREEN = (2.17, 0.18, 1.91, 0.105, 1.555, (0.42, 1.13))
# Her hood and the glazing behind it: (x, the height of its sill where the glass begins, how far out it is there, the
# frame at its shoulder (out, up), the height of its top). The pilot's sliding hood, the middle station's, the fixed
# glazing; and the glazed back of it, which leans forward over the turret's ring.
HOOD = [(1.56, 1.06, 0.53, (0.285, 1.585), 1.81), (0.83, 1.06, 0.53, (0.285, 1.585), 1.81)]
MIDDLE = [(0.81, 1.06, 0.51, (0.28, 1.585), 1.80), (0.51, 1.06, 0.51, (0.28, 1.585), 1.80), (0.20, 1.06, 0.51, (0.28, 1.585), 1.80)]
BACK = [(0.17, 1.06, 0.50, (0.275, 1.585), 1.80), (-0.15, 1.06, 0.50, (0.275, 1.585), 1.80), (-0.48, 1.06, 0.50, (0.275, 1.58), 1.785),
        (-0.79, 1.06, 0.50, (0.27, 1.575), 1.755)]
BACK_END = (-1.25, 1.06, 0.46, (0.25, 1.55), 1.73, 0.32)    # (and how far it leans forward for every metre up)


def _tbf_section(hull, x, sill, ys, shoulder, top, band=0.08):
    """Half a section of her greenhouse (fighters.green, with her roof): its metal rail from her side up to the sill
    where the glass begins, `ys` out from her middle (the first SKIRT points, standing on her skin); a flat side pane
    leaning in from the sill to the frame at its shoulder, (out, up); and its roof over to her middle line at `top` in
    three flat panes, the narrowest in the middle between two frames 0.07 out from it."""
    ys = max(ys, side_at(hull, x, sill) + 0.012)
    yb = ys + 0.004
    zb = min(sill - band, fitout.body_top(hull, x, yb) - 0.006)
    y1, z1 = shoulder
    d = top - z1
    return [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb), (ys, sill), (y1, z1), (0.72 * y1, z1 + 0.68 * d), (0.25 * y1, z1 + 0.975 * d), (0.0, top)]


def _tbf_canopy(hull, fine=True):
    """Her greenhouse's pieces for fighters.canopy, which tells it which model it is laying (`fine`): on the map's
    model, the plain greenhouse at the end."""
    foot_x, foot_half, head_x, head_half, head_z, corner = SCREEN
    hood = [(x, _tbf_section(hull, x, sl, w, sh, t)) for x, sl, w, sh, t in HOOD]
    # her windscreen: the flat front pane from its foot on her deck up to the level frame across its head; a quarter
    # pane each side from the foot of the post out to the corner of its side, and a small corner pane under that; then
    # its side panes back to the hood, and its roof, curving up from the frame across its head to the hood's front bow
    head = _tbf_section(hull, head_x, corner[1], corner[0], (0.27, head_z), head_z)
    head = head[:4] + [(0.19, head_z), (head_half, head_z), (0.0, head_z)]
    post = len(head) - 2
    foot = [deck(hull, foot_x, foot_half, 0.0)] * (len(head) - 1) + [deck(hull, foot_x, 0.0, 0.0)]
    bow = _tbf_section(hull, 1.73, 1.095, 0.48, (0.28, 1.555), 1.74)
    bow = bow[:4] + [(0.20, 1.69), (0.07, 1.735), (0.0, 1.74)]
    screen = {"sections": [(foot_x, foot), (head_x, head), (1.73, bow), hood[0]], "skirt": SKIRT, "hoops": [], "bars": [(1, 3, 6)],
              "rails": [SKIRT, 3], "bare": (0,), "struts": [((0, post), (1, post)), ((0, SKIRT), (1, SKIRT)), ((1, SKIRT), (2, 3))],
              "bar": 0.02}
    # the pilot's sliding hood: a frame round each end, its sills, its shoulder frames and the two frames along its top
    slide = {"sections": hood, "skirt": SKIRT, "hoops": [0, 1], "rails": [SKIRT, 3, 5], "bar": 0.018}
    # the middle station's hood behind it, hooped in the middle
    middle = {"sections": [(x, _tbf_section(hull, x, sl, w, sh, t)) for x, sl, w, sh, t in MIDDLE], "skirt": SKIRT, "hoops": [0, 1, 2],
              "rails": [SKIRT, 3, 5], "bar": 0.018}
    # the fixed glazing, its hoops, and its glazed back leaning forward over the turret's ring
    xe, sle, we, she, te, lean = BACK_END
    back = [(x, _tbf_section(hull, x, sl, w, sh, t)) for x, sl, w, sh, t in BACK] + [(xe, _tbf_section(hull, xe, sle, we, she, te), lean)]
    fixed = {"sections": back, "skirt": SKIRT, "hoops": [0, 1, 2, 3, 4], "rails": [SKIRT, 3], "bar": 0.018, "cap": True}
    if not fine:
        # (the map's model: the same greenhouse in one piece from the foot of its windscreen to its glazed back, its roof
        #  two flat panes a side, framed at the windscreen's head, the hood's front and the back)
        def plain(sec):
            return (sec[0], sec[1][:5] + sec[1][-1:]) + tuple(sec[2:])
        whole = [plain(sec) for sec in [(foot_x, foot), (head_x, head), hood[0]] + back[3:]]
        return [{"sections": whole, "skirt": SKIRT, "hoops": [1, 2, 4], "rails": [SKIRT, 3], "bare": (0,), "bar": 0.018, "cap": True}]
    return [screen, slide, middle, fixed]


NUMBERS = {
    "title": "TBF-1 Avenger (our own model)", "year": 1942,
    "hull": HULL,
    "square": 2.3, "screw": 2.0, "blades": 3, "nose_back": 3.30, "spinner": 5, "engine": "radial", "lip": 4.355,
    "hoods": hood_of([(SCREEN[0], 1.20, SCREEN[1], 0.01), (SCREEN[2], SCREEN[5][1], SCREEN[5][0], SCREEN[4] - SCREEN[5][1])]
                     + [(x, sl, w, t - sl) for x, sl, w, sh, t in HOOD + MIDDLE + BACK]
                     + [(BACK_END[0] + 0.1, BACK_END[1], BACK_END[2], BACK_END[4] - BACK_END[1])]),
    "canopy": _tbf_canopy,
    "opening": (1.72, -0.85, 60.0),                         # (her cockpit, open under the glass from the pilot's panel to behind the middle station)
    "frames_paint": "upper",
    # (her wing: its middle flat, 0.6 m thick and as broad as it is deep; the outer panels tapered and turned up 5.9
    #  degrees from where they fold, 0.15 m thick at the tip, the tip rounded)
    "wing": [(0.0, 2.145, -1.548, 0.0, 0.70), (2.08, 2.142, -1.547, 0.0, 0.655), (2.6, 2.063, -1.435, 0.054, 0.607), (7.674, 1.302, -0.346, 0.577, 0.147),
             (8.10, 1.237, -0.25, 0.621, 0.105), (8.18, 1.198, -0.235, 0.629, 0.085), (8.235, 1.12, 0.30, 0.635, 0.045), (8.255, 1.07, 0.76, 0.637, 0.03),
             (8.264, 1.0, 0.95, 0.638, 0.01)],
    # (her tailplane, its tips rounded, at the top of her rear fuselage)
    "tail": [(0.0, -5.30, -6.60, 1.06, 0.28), (0.25, -5.335, -6.75, 1.06, 0.275), (0.45, -5.37, -7.08, 1.06, 0.27), (0.81, -5.44, -7.473, 1.06, 0.25),
             (1.57, -5.579, -7.369, 1.06, 0.22), (2.34, -5.719, -7.263, 1.06, 0.19), (2.68, -5.773, -7.118, 1.06, 0.175), (3.00, -5.85, -6.73, 1.06, 0.13),
             (3.10, -5.94, -6.51, 1.06, 0.09), (3.185, -6.15, -6.28, 1.06, 0.02)],
    # (her fin from inside her tail cone up: thick where it is the deep rear fuselage under her tailplane; the bottom of
    #  her rudder running straight back from the end of her tail cone to a rounded corner; her leading and trailing
    #  edges straight, as drawn, and her top rounded over. The drawing hinges her rudder from x = -6.33 at its foot to
    #  -6.43 at its top; the game hinges every rudder at 60 per cent of its chord, a few centimetres aft of that)
    "fin": [(0, -5.00, -6.88, 0.25, 0.44), (0, -5.03, -6.875, 0.322, 0.44), (0, -5.06, -7.19, 0.39, 0.44), (0, -5.12, -7.39, 0.44, 0.44),
            (0, -5.27, -7.25, 1.22, 0.345), (0, -5.495, -7.146, 1.82, 0.266), (0, -5.72, -7.042, 2.42, 0.188), (0, -5.869, -6.972, 2.82, 0.136),
            (0, -5.942, -6.938, 3.016, 0.11), (0, -6.06, -6.80, 3.10, 0.08), (0, -6.40, -6.50, 3.135, 0.01)],
    "patch": 1.0, "colours": {"upper": "566A78", "upper2": "566A78", "under": "A8AEAD", "nose": "566A78", "boss": "2A2B2D"},
    "well": (3.4, 0.30, 0.43), "well_in": True, "pitot": True,
    "aileron": (0.684, 0.985, 0.74), "flap": (0.10, 0.684, 0.76),       # (her ailerons outboard; her flaps from her root out to them)
    "elevator": (0.08, 0.84, 0.36),                         # (her elevators hinged at 36 per cent of the chord, as drawn, out to 84 per
                                                            #  cent of her tailplane's span: its rounded tips are fixed)
    "exhaust": (3.33, -20.0, 0.28, 0.0, 0.09, 20.0, 90.0),  # (a fat stub each side, from her skin behind the cowl, running aft and down,
                                                            #  its mouth turned straight out where the drawing has it)
    "heads": (7, 2, 0.53, 0.09, 0.03),                      # (her R-2600's fourteen heads, two rows of seven, in the open mouth of her cowl:
                                                            #  their ring clear of her cowling and of her nose case)
    "windows": [(-2.24, -2.55, -0.20, 0.16), (-3.42, -3.72, -0.13, 0.03)],      # (her radio compartment's, each side)
    "scoops": [
        # (the dorsal fin: a blade rising out of her back behind the turret, widening and rising aft into her fin; its foot is
        #  inside her, and so is the lower half of the blade, which the sculptor makes as deep as the upper)
        ([(-2.70, 1.00, 0.004, 0.01), (-4.27, 0.727, 0.098, 0.429),
          (-4.69, 0.57, 0.17, 0.612), (-4.89, 0.48, 0.24, 0.712), (-5.20, 0.52, 0.22, 0.69), (-5.40, 0.50, 0.08, 0.62)], 0.0, 1.2)],
    "mast": (0.85, 1.80, 2.42, 0.10),
    # (the turret: a ball on her back, metal below its ring at z 1.16 and glass above, a dark window each side of the
    #  metal, its one .50 looking aft; the .30 under her tail, out of the step behind her bay, through a dark port where
    #  it leaves her skin; the short aerial post on top of her fin, with no wire; and her engine's nose case behind the
    #  hub, inside her heads. The port, the post and the nose case are on the fine model only. Only a gun is steel:
    #  planes.py takes a steel rod for a gunner's gun)
    "extras": [("dome", ((-1.635, 0.0, 1.13), (0.57, 0.60, 0.495), 1.16, (-1.39, -1.86, 0.80, 1.02))),
               ("rod", ((-1.75, 0.0, 1.30), (-2.66, 0.0, 1.325), 0.026, 0.02, "steel")),
               ("rod", ((-3.45, 0.0, -0.47), (-4.22, 0.0, -0.66), 0.02, 0.016, "steel")),
               ("rod", ((-3.95, 0.0, -0.593), (-4.02, 0.0, -0.610), 0.045, 0.045, "engine", "fine")),
               ("rod", ((-6.52, 0.0, 3.11), (-6.52, 0.0, 3.36), 0.03, 0.015, "upper", "fine")),
               ("body", ([(4.335, 0.023, 0.10, 0.10), (4.31, 0.023, 0.24, 0.24), (4.27, 0.023, 0.31, 0.31), (4.20, 0.023, 0.345, 0.345),
                          (3.97, 0.023, 0.35, 0.35)], 0.0, 2.0, "engine", "fine"))],
    "side_marks": [(-4.25, 0.45, "USA")], "wing_marks": [(5.6, 0.35, 0.7, "USA", "USA", False, False)], "code": (-5.2, 0.3),
}

# Who sits under her glass (fitout.SEATS): her pilot under his sliding hood, and the man at the middle station behind
# him, facing forward. Her turret gunner is seated by the turret itself (fitout.gunners), both hands on its spade grips.
SEATS = [(0, 1.20, 0.0, 0.0), (0, -0.45, 0.0, 0.0)]
FIN_FROM = -5.1                                             # (her fin's root, at the foot of its leading edge: aft of it, above her back, is fin;
                                                            #  her dorsal fin ahead of it is her spine, sea blue like the top of her fuselage)
BAY = (2.9, -1.75)                                          # (her bay, from and to along her: its doors under her flat keel)
TORPEDO = (4.1, 0.28)                                       # (the Mark 13 in it, how long and half as thick: as the game had it)
# How far her turret trains and lifts (planes.TURRET_LIMITS): not past 150 degrees either way, where its .50 would come
# through the glazed back of her greenhouse at the man at the middle station.
TURRET = {1: ((-150, 150), (-10, 80))}


def coat(navy_blues):
    """Her three blues as the game paints her from 1943-02 (navy_blues comes from planes): the fleet's own field for
    her fuselage, with her tailplane and elevators sea blue on top as her wings are (they stand high above her slim
    tail cone, where the field would take them for fin), and her rudder intermediate blue as her fin is (aft of her tail
    cone, where the field would take it for wing)."""
    blues = navy_blues(HULL, NUMBERS["square"], FIN_FROM)
    tail_z, back = NUMBERS["tail"][0][3], HULL[-1][0]

    def graded(p):
        if p.x < FIN_FROM and abs(p.z - tail_z) < 0.17 and abs(p.y) > 0.19:
            return -1.0                                     # (her tailplane and elevators)
        if p.x < back and p.z > 0.2:
            return 1.0                                      # (her fin and rudder aft of her tail cone)
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
