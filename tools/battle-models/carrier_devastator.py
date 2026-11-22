"""The TBD-1 Devastator as the US Navy's carriers flew her at Midway in June 1942, laid out to a measured drawing: her
long slab-sided fuselage and the long low greenhouse of framed flat panes over her pilot and gunner, her corrugated wing, and
the Mark 13 torpedo half sunk in her belly (or the one 1,000 lb bomb she carried in its place).

Her lines are measured from a public three-view drawing of the real aircraft at a known scale: measured, not traced,
and nothing of the drawing is used but its measures. The numbers are in the game's form (game/plane_numbers.py and
fighters.py's docstring), x ahead, z up, in metres, with the airscrew's axis at z = 0; her glazing is the fighters'
`canopy` (fighters.py), a run of pieces each lofted through cross-sections whose few points make flat panes between
framed bars.

What planes.py reads from here (planes.carrier): NAME and NUMBERS; SEATS (who sits under her glass: fitout.SEATS);
coat() (her three blues of 1943, NUMBERS["graded"], built on planes.navy_blues from her hull and FIN_FROM). install()
does the same for a scratch copy of this file, for trying it.
"""
import fitout
from fighters import SKIRT, deck, hood_of, side_at

NAME = "Devastator"

# ---- the TBD-1 Devastator, as at Midway in June 1942 -------------------------------------------------------------
# Measured from "Douglas TBD Devastator 3-view" by Kaboldy (Wikimedia Commons; licensed CC BY-SA 3.0 and 4.0: its
# measures only are used), along her and up at 118.46 pixels to the metre, her length the real 10.67 m from the front
# of her airscrew's hub to the trailing edge of her rudder; across her at 117.0, her span the real 15.24 m (the drawing's
# views are not quite at one scale; her heights are read from the side, her widths from above, and the head-on view
# gives the rise of her wing). Her hub's face stands at x = 4.20.
#
# What she is: the bare hub of her three-bladed airscrew with no spinner, its dome in front; the short cowling of her
# R-1830, its open mouth with the two rows of seven heads of the engine in it, the gun's blast tube in the ring of its
# lip on her starboard side, and her one exhaust pipe on her starboard side behind it; a fuselage deeper than it is wide,
# slab-sided under her greenhouse and tapering to her rudder post; the greenhouse itself, a windscreen of flat panes and
# six sliding sections each a step lower than the one ahead of it, each with its sill, a frame along its side, a frame
# where its side meets its roof and a frame round each end; under it, the pilot at his stick and throttle, the
# middle place empty (her bombardier stayed aboard for a torpedo attack), and the radioman-gunner at the back facing
# aft with both hands on his .30, his hood run forward under the one ahead of it so that his place is open, as it was
# when he fought. Her broad low wing: its centre section with the big fillet at its root, its outer panels tapered and
# turned up to rounded tips; the drawing gives the centre section a little dihedral too, and it has it. The outer panels folded upward at the joint
# the drawing has 3.64 m out: the joint is scribed and its hinge fairings stand on it, the panels spread for flight.
# Her corrugated skin: fine ridges running with the air where the drawing hatches it, on the inner part of each outer
# panel all across, on the rest of it ahead of the aileron, and in a triangle ahead of each elevator's hinge (fitout's
# `corrugation`: ridges in the skin, never stripes of paint). Her main wheels half drawn up into the wing, the lower half
# of each tyre showing; her fixed tailwheel; her hook stowed along her belly. The oil cooler under the root of her
# starboard wing, her radio mast on the deck ahead of her windscreen, the telescope sight through her windscreen, her
# pitot out ahead of her starboard wing.
# Her blue-grey over light grey of 1942; the star on her sides, and on both wings above and below (the Navy's order of
# 5 January 1942, kept until February 1943); her side number in black.
#
# Her fuselage, measured: (x, the height of its middle, half its width, half its depth). The hub, its dome and its
# collar; the face of the engine in the cowling's mouth, the lip, the cowling; her fuselage, its depth and width read
# off the drawing at every station kept; under her greenhouse its top is the glass's, and the stations there stand as
# high as hold her sides straight up to the greenhouse's rails (her `squares`); to her rudder post, where she ends.
HULL = [
    (4.310, 0.0, 0.015, 0.015), (4.303, 0.0, 0.05, 0.05), (4.290, 0.0, 0.068, 0.068), (4.210, 0.0, 0.068, 0.068),
    (4.200, 0.0, 0.137, 0.137), (3.940, 0.0, 0.137, 0.137),
    (3.918, 0.0, 0.11, 0.11), (3.905, 0.0, 0.25, 0.25), (3.895, 0.0, 0.455, 0.455), (3.889, -0.005, 0.485, 0.48),
    (3.878, -0.004, 0.51, 0.505), (3.860, -0.003, 0.535, 0.53), (3.830, 0.0, 0.568, 0.562), (3.778, 0.0025, 0.598, 0.5885),
    (3.525, -0.0105, 0.615, 0.6225), (3.187, -0.028, 0.637, 0.657), (3.018, -0.0365, 0.654, 0.6725), (2.849, -0.038, 0.660, 0.688),
    (2.680, -0.0425, 0.667, 0.7135), (2.512, -0.0445, 0.672, 0.7365), (2.300, -0.0525, 0.682, 0.7545), (1.900, -0.051, 0.684, 0.781),
    (1.414, -0.054, 0.684, 0.804), (0.992, -0.060, 0.688, 0.810), (0.148, -0.060, 0.667, 0.820), (-0.696, -0.038, 0.611, 0.828),
    (-1.329, 0.0005, 0.563, 0.8195), (-1.850, 0.0425, 0.538, 0.8025), (-2.385, 0.074, 0.479, 0.749), (-3.229, 0.1415, 0.397, 0.6525),
    (-4.073, 0.211, 0.308, 0.553), (-4.917, 0.2465, 0.274, 0.4535), (-5.339, 0.2875, 0.239, 0.3325), (-5.420, 0.285, 0.165, 0.315),
    (-5.590, 0.280, 0.132, 0.285), (-5.640, 0.275, 0.006, 0.275)]
POST = -5.64
# (how square her section is, station by station: round at her cowling, slab-sided under her greenhouse, and as round
#  as the rest of her fuselage again aft of her gunner: fitout.squared_hull)
SQUARES = [(3.9, 2.0), (2.9, 2.05), (2.3, 2.15), (1.414, 2.22), (0.992, 2.25), (0.148, 2.38), (-0.696, 2.65), (-1.329, 2.7),
           (-1.85, 2.55), (-2.6, 2.3)]

# Her greenhouse, measured, each section at its frames: (x, the height of its sill where the glass begins, how far out
# the glass is there, the frame along its side (out, up), the frame where its side meets its roof (out, up), the height
# of its top). The tops step down from her pilot's hood to her gunner's, each section a little lower and narrower than
# the one ahead of it, so that it slides under it.
_A = [(-1.810, 0.597, 0.462, (0.427, 0.743), (0.337, 0.805), 0.850), (-1.245, 0.597, 0.462, (0.427, 0.743), (0.337, 0.86), 0.905)]
_B = [(-1.265, 0.590, 0.483, (0.449, 0.743), (0.365, 0.905), 0.940), (-0.684, 0.586, 0.483, (0.449, 0.743), (0.365, 0.975), 1.011)]
_C = [(-0.700, 0.582, 0.508, (0.479, 0.747), (0.393, 0.895), 1.026), (-0.106, 0.578, 0.508, (0.479, 0.755), (0.393, 0.920), 1.100)]
_D = [(-0.122, 0.578, 0.513, (0.410, 0.760), (0.297, 0.954), 1.137), (0.460, 0.574, 0.513, (0.410, 0.760), (0.297, 0.985), 1.190)]
_E = [(0.444, 0.572, 0.513, (0.410, 0.760), (0.297, 0.990), 1.190), (1.030, 0.566, 0.513, (0.410, 0.760), (0.297, 1.015), 1.245)]
_F = [(1.018, 0.566, 0.513, (0.415, 0.777), (0.297, 1.055), 1.255), (1.160, 0.566, 0.513, (0.415, 0.777), (0.297, 1.055), 1.262),
      (1.275, 0.565, 0.513, (0.415, 0.776), (0.297, 1.050), 1.254), (1.414, 0.563, 0.513, (0.415, 0.775), (0.300, 1.032), 1.216),
      (1.566, 0.561, 0.513, (0.415, 0.772), (0.300, 0.990), 1.148)]
_TUCK = 0.012                                               # (how much smaller a section's front is where it runs under the next)
_SLID = 0.55                                                # (how far forward her gunner's hood is run, to lie under the next)
# (her windscreen: the foot of its front pane on her deck, x and half its width; the frame across its head, x and its
#  height, and half its width there between its posts; where the foot of its side panes stands at its head)
_SCREEN = (2.30, 0.30, 1.80, 1.0, 0.27, (0.445, 0.66))


def _section(hull, x, sill, ys, bar, upper, top, band=0.06):
    """Half a section of her greenhouse: its metal rail from her side up to the sill where the glass begins, `ys` out
    from her middle (the first SKIRT points: the rail stands on her skin wherever that is lower); a flat side pane
    leaning in from the sill past the frame along its side to the frame where it meets the roof, (out, up); and its
    roof over to her middle line at `top` in three flat panes."""
    ys = max(ys, side_at(hull, x, sill) + 0.012)
    yb = ys + 0.004
    zb = min(sill - band, fitout.body_top(hull, x, yb) - 0.006)
    y2, z2 = upper
    d = top - z2
    return [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb), (ys, sill), bar, upper, (0.62 * y2, z2 + 0.62 * d), (0.31 * y2, z2 + 0.9 * d), (0.0, top)]


def _tucked(row):
    """A section's front where it runs under the next one: a little narrower and lower all round."""
    x, sill, ys, (y1, z1), (y2, z2), top = row
    t = _TUCK
    return (x, sill, ys - t, (y1 - t, z1), (y2 - t, z2 - t), top - t)


def _piece(hull, rows, hoops, slid=0.0, tuck=True):
    rows = list(rows)
    if tuck:
        rows[-1] = _tucked(rows[-1])
    secs = [(x + slid, _section(hull, x + slid, sl, ys, b, u, t)) for x, sl, ys, b, u, t in rows]
    return {"sections": secs, "skirt": SKIRT, "hoops": hoops, "rails": [SKIRT, 3, 4], "bar": 0.018}


def _tbd_canopy(hull, fine=True):
    """Her greenhouse's pieces for fighters.canopy: the windscreen, the pilot's hood, the four sections behind it, and
    the gunner's hood run forward under the last of them."""
    foot_x, foot_half, head_x, head_z, post, (side_y, side_z) = _SCREEN
    # her windscreen: the flat front pane from its foot on her deck up to the level frame across its head, a quarter pane
    # each side from the foot of its post back to its side panes, and its roof curving up from its head to the front bow
    # of her pilot's hood (it runs under the bow)
    rear = _tucked(_F[-1])
    back = _section(hull, rear[0] + 0.02, *rear[1:])
    head = _section(hull, head_x, side_z, side_y, (0.5 * (side_y + post), 0.5 * (side_z + head_z)), (post, head_z), head_z)
    head = head[:5] + [(0.62 * post, head_z), (0.31 * post, head_z), (0.0, head_z)]
    foot = [deck(hull, foot_x, foot_half, 0.0)] * (len(head) - 1) + [deck(hull, foot_x, 0.0, 0.0)]
    screen = {"sections": [(foot_x, foot), (head_x, head), (rear[0] + 0.02, back)], "skirt": SKIRT, "hoops": [1], "rails": [SKIRT, 4],
              "bare": (0,), "struts": [((0, 4), (1, 4))], "bar": 0.02}
    pilot = _piece(hull, _F, [0, 2, 4] if fine else [0, 4], tuck=False)
    pieces = [screen, pilot]
    for rows in (_E, _D, _C, _B):
        pieces.append(_piece(hull, rows, [0, 1]))
    gunner = _piece(hull, _A, [0, 1], slid=_SLID)
    gunner["bar"] = 0.016
    pieces.append(gunner)
    return pieces


# Where her men sit and how high (the `hoods` the rest of the tools read): her windscreen, her greenhouse at its
# frames, and her gunner's open place behind it, his hood run forward out of it (x, sill, half its width, how far its
# top stands above its sill).
_HOODS = ([(_SCREEN[0], 0.70, _SCREEN[1], 0.01), (_SCREEN[2], _SCREEN[5][1], _SCREEN[5][0], _SCREEN[3] - _SCREEN[5][1])]
          + [(x, sl, ys, t - sl) for x, sl, ys, b, u, t in (_F[-1], _F[1], _F[0], _E[0], _D[0], _C[0], _B[0])]
          + [(-1.30, 0.597, 0.462, 0.43), (-1.70, 0.597, 0.462, 0.43), (-1.85, 0.62, 0.40, 0.23)])

NUMBERS = {
    "title": "TBD-1 Devastator (our own model)", "year": 1937,
    "hull": HULL, "square": 2.3, "squares": SQUARES,
    "screw": 1.68, "blades": 3, "spinner": 6, "engine": "radial", "lip": 3.89,
    "nose_back": 3.06,                                      # (her cowl flaps, between the cowling's back edge and her firewall)
    "hoods": hood_of(_HOODS),
    "canopy": _tbd_canopy,
    "opening": (1.95, -1.80, 45.0),                         # (her cockpit, open under the glass from the pilot's panel to behind her gunner)
    "frames_paint": "upper", "sight": "telescope",
    # (her wing: the centre section 0.58 thick with its fillet swept back into her side at its root; the outer panels
    #  from 2.18 m out tapered and turned up 5.8 degrees, fourteen per cent thick at the joint and ten at the tip, its
    #  tip rounded. The fillet at the root of its trailing edge is a fairing of her extras, from 2.5 m aft, where her
    #  belly begins to rise above her wing, on into her side as the drawing has it)
    "wing": [(0.0, 1.76, -2.30, -0.665, 0.58), (0.509, 1.76, -2.50, -0.631, 0.577), (0.577, 1.76, -2.50, -0.626, 0.576),
             (0.646, 1.76, -2.511, -0.622, 0.576), (0.714, 1.76, -2.385, -0.617, 0.575), (0.85, 1.752, -2.19, -0.608, 0.575),
             (1.124, 1.735, -1.954, -0.590, 0.573), (1.329, 1.727, -1.836, -0.576, 0.571), (2.013, 1.693, -1.642, -0.530, 0.567),
             (2.18, 1.655, -1.600, -0.519, 0.566), (2.286, 1.634, -1.574, -0.508, 0.555), (2.833, 1.600, -1.422, -0.4525, 0.513),
             (3.722, 1.532, -1.177, -0.3625, 0.44), (4.543, 1.490, -0.958, -0.2795, 0.369), (5.432, 1.423, -0.713, -0.1895, 0.30),
             (6.252, 1.372, -0.494, -0.1065, 0.23), (6.799, 1.338, -0.350, -0.0512, 0.181), (7.141, 1.262, -0.190, -0.0166, 0.16),
             (7.346, 1.161, -0.021, 0.0042, 0.128), (7.551, 0.942, 0.267, 0.0249, 0.075), (7.62, 0.62, 0.60, 0.0319, 0.01)],
    # (her flaps from 1.33 m out, where her trailing edge runs straight, so that the flap's hinge lies on its split (the
    #  fillet's curve inboard of it is fixed wing), out to the fold; her ailerons from the fold nearly to her tip,
    #  hinged where the drawing has them, 18 per cent of the chord: the nearest of the game's rings is 14)
    "flap": (0.175, 0.48, 0.76), "aileron": (0.49, 0.965, 0.86),
    # (her tailplane on the top of her rear fuselage, its root cut away for her rudder, its tips rounded; as high as the
    #  side view has its root, where it meets her. The head-on view draws it some 6 cm lower, as it draws her belly and
    #  her wheels some 5 cm lower than the side view does: her heights are the side view's)
    "tail": [(0.0, -4.15, -5.60, 0.58, 0.19), (0.132, -4.155, -5.66, 0.58, 0.19), (0.20, -4.16, -5.78, 0.58, 0.19),
             (0.269, -4.17, -5.90, 0.58, 0.19), (0.338, -4.178, -6.03, 0.58, 0.188), (0.406, -4.183, -6.20, 0.58, 0.187),
             (0.748, -4.318, -6.166, 0.58, 0.172), (1.158, -4.470, -6.115, 0.58, 0.152), (1.637, -4.655, -6.057, 0.58, 0.13),
             (1.979, -4.782, -6.023, 0.58, 0.115), (2.184, -4.866, -5.989, 0.58, 0.104), (2.389, -4.942, -5.896, 0.58, 0.089),
             (2.526, -5.010, -5.803, 0.58, 0.074), (2.662, -5.153, -5.643, 0.58, 0.046), (2.714, -5.36, -5.44, 0.58, 0.01)],
    # (her elevators from where the root's cut-out ends, 0.41 m out, to the tips, the rounded tip aft of the hinge
    #  theirs. A share of the chord, the hinge is swept a little where the drawing's runs straight across; from 0.41 m out
    #  its axis keeps within 2 to 6 cm of the cut, so a full throw shows no step)
    "elevator": (0.15, 1.0, 0.48),
    # (her fin and rudder from the foot of her rudder at her post up: the fillet that runs its leading edge down into
    #  her back, the leading edge, and the top rounded over. Below her fuselage's top it is the foot of her rudder behind
    #  her post, rounded under; ahead of the post it is inside her)
    "fin": [(0, -5.60, -5.645, 0.0, 0.02), (0, -5.50, -5.79, 0.046, 0.10), (0, -5.215, -6.10, 0.131, 0.20), (0, -5.073, -6.255, 0.215, 0.24),
            (0, -4.94, -6.339, 0.30, 0.27), (0, -4.86, -6.398, 0.384, 0.29), (0, -4.78, -6.432, 0.469, 0.30), (0, -4.70, -6.449, 0.553, 0.31),
            (0, -4.55, -6.456, 0.64, 0.32), (0, -4.495, -6.457, 0.66, 0.32), (0, -4.30, -6.458, 0.72, 0.32), (0, -4.20, -6.458, 0.772, 0.32), (0, -4.33, -6.458, 0.787, 0.32),
            (0, -4.42, -6.455, 0.812, 0.32), (0, -4.463, -6.453, 0.832, 0.318), (0, -4.50, -6.452, 0.852, 0.315), (0, -4.578, -6.449, 0.92, 0.31), (0, -4.664, -6.44, 1.026, 0.29),
            (0, -4.782, -6.415, 1.228, 0.265), (0, -4.892, -6.38, 1.431, 0.24), (0, -5.031, -6.322, 1.65, 0.21), (0, -5.124, -6.267, 1.819, 0.185),
            (0, -5.212, -6.196, 1.988, 0.16), (0, -5.305, -6.103, 2.157, 0.13), (0, -5.36, -6.036, 2.241, 0.115), (0, -5.427, -5.956, 2.326, 0.095),
            (0, -5.491, -5.88, 2.376, 0.075), (0, -5.554, -5.825, 2.41, 0.055), (0, -5.66, -5.72, 2.432, 0.01)],
    # (her rudder from its foot to its top, cut where the drawing cuts it (rudder_line): behind her post under her
    #  tailplane, back behind the box over its root from 0.66 to 0.83 m up, then on the fin's own hinge line, 5.46 m aft
    #  at its foot leaning back to 5.52, and the rounded top aft of the line from there up to her tip. No one share of
    #  the chord gives that line, so planes.py stretches each row of her fin to put her hinge's ring on it. It turns on
    #  the fin's hinge line carried straight down and up, so that nothing of it stands ahead of its axis and nothing
    #  swings out through her fin; its foot, behind her post, swings clear of her as it turns)
    "rudder": (0.0, 1.0, 0.48),
    "rudder_line": ([(0.0, -5.64), (0.64, -5.64), (0.66, -5.70), (0.832, -5.70), (0.852, -5.46), (1.988, -5.522), (2.432, -5.677)],
                    ((0.0, -5.415), (2.432, -5.545))),
    "patch": 1.0, "colours": {"upper": "566A78", "upper2": "566A78", "under": "A8AEAD", "nose": "566A78", "boss": "2A2B2D"},
    "tailwheel": (-5.465, -0.31),                           # (her pitot is her own, in her extras: on her starboard wing)
    # (the two rows of seven heads of her R-1830 in the open mouth of her cowling, the face of the engine dark to 0.46 m
    #  out; the lip of the cowling beyond that, rounded over, is her blue-grey)
    "heads": (7, 2, 0.36, 0.075, 0.03), "mouth": 0.46,
    # (her one exhaust pipe, on her starboard side behind her cowling, running aft to its mouth standing off her side;
    #  her one fixed gun, its port in the ring of her cowling's lip on her starboard side)
    "exhaust": (2.86, 4.0, 0.46, 0.15, 0.065, 0.0, 0.0), "exhaust_side": 1.0,
    "cowl_gun": (3.852, 0.524, 0.135),
    "mast": (2.976, 0.60, 1.735, 0.0),
    # (her corrugated skin, where the drawing hatches it: (surface, from, to along its span, (from, to) along its chord
    #  there, (from, to) at the other end, how far apart the ridges are). The inner part of each outer panel from behind
    #  the nose of its leading edge to her flap's hinge; the rest of the outer panel to the rounded tip, to her aileron's
    #  hinge; a triangle ahead of each elevator's hinge, from near its root to nothing 2.26 m out. A ridge every 5.2 cm:
    #  the drawing hatches them 4.3 cm apart, and so close they would weigh her down past her peers)
    # (on her plain model her torpedo, bomb and wheels at the game's own grain, 12 round every 30 cm and rods of 10
    #  sides: at the fine grain they were a third of it, and the map and the far camera cannot tell the difference)
    "plain_extras": (12, 0.3, 10),
    "corrugated": [("wing", 2.31, 3.55, (0.04, 0.74), (0.04, 0.74), 0.052), ("wing", 3.72, 6.78, (0.04, 0.84), (0.04, 0.84), 0.052),
                   ("tail", 0.30, 2.24, (0.22, 0.46), (0.45, 0.46), 0.052)],
    "scoops": [
        # (her oil cooler, a drum under the root of her starboard wing, its mouth open to the air)
        ([(1.77, -0.79, 0.165, 0.165), (1.75, -0.79, 0.19, 0.19), (1.45, -0.79, 0.19, 0.19), (1.0, -0.785, 0.18, 0.18),
          (0.75, -0.77, 0.13, 0.13), (0.57, -0.75, 0.03, 0.03)], 1.06, 2.0, "one")],
    "extras": [
        # (her main wheels half drawn up into her wing, the lower half of each tyre showing: the tyre, rounded at its
        #  shoulders, and its hub)
        ("rod", ((0.19, 1.74, -0.756), (0.19, 1.94, -0.756), 0.345, 0.345, "tyre")),
        ("rod", ((0.19, 1.76, -0.756), (0.19, 1.92, -0.756), 0.38, 0.38, "tyre")),
        ("rod", ((0.19, 1.725, -0.756), (0.19, 1.955, -0.756), 0.13, 0.13, "leg")),
        # (the Mark 13 torpedo, as the drawing has it: half sunk in her belly, its nose down six degrees; its body, its
        #  tail cone and the box of its tail, 0.57 m across and 4.1 m long, the real weapon's size; the two crutches that
        #  steady it)
        ("body", ([(2.74, -1.41, 0.03, 0.03), (2.703, -1.405, 0.182, 0.182), (2.63, -1.397, 0.255, 0.255), (2.521, -1.385, 0.285, 0.285),
                   (-0.282, -1.08, 0.285, 0.285), (-0.493, -1.058, 0.243, 0.243), (-0.703, -1.034, 0.146, 0.146), (-0.858, -1.017, 0.073, 0.073),
                   (-1.36, -0.962, 0.03, 0.03)], 0.0, 2.0, "torpedo")),
        ("rod", ((-0.821, 0.0, -1.033), (-1.360, 0.0, -0.958), 0.243, 0.243, "torpedo")),
        ("rod", ((1.65, 0.11, -0.84), (1.65, 0.15, -1.04), 0.016, 0.016, "leg")),
        ("rod", ((0.95, 0.11, -0.87), (0.95, 0.15, -0.98), 0.016, 0.016, "leg")),
        # (or her one 1,000 lb bomb in its place, on the same crutches: its round nose, its case, its tail cone and the box
        #  of its fins)
        ("body", ([(1.55, -1.12, 0.03, 0.03), (1.45, -1.12, 0.15, 0.15), (1.30, -1.12, 0.215, 0.215), (1.00, -1.12, 0.235, 0.235),
                   (0.35, -1.12, 0.235, 0.235), (0.0, -1.12, 0.16, 0.16), (-0.30, -1.12, 0.06, 0.06), (-0.35, -1.12, 0.03, 0.03)], 0.0, 2.0, "bomb")),
        ("rod", ((-0.02, 0.0, -1.12), (-0.40, 0.0, -1.12), 0.20, 0.20, "bomb")),
        # (her gunner's .30 on its free mounting, raised and looking aft over her turtle deck, its grips at his chest and
        #  clear of his face at any bearing; planes.py takes a steel rod for a gunner's gun)
        ("rod", ((-1.90, 0.0, 0.96), (-2.82, 0.0, 1.10), 0.02, 0.016, "steel")),
        # (the fillet at the root of her wing's trailing edge: a fairing on her belly's edge, swept back out of her wing
        #  and on into her side, as wide as the drawing has it, to nothing 3.20 m aft; its inner side sunk in her belly, so
        #  that it lies on her and never stands off her with daylight under it)
        ("body", ([(-2.30, -0.63, 0.25, 0.012), (-2.40, -0.62, 0.37, 0.035), (-2.50, -0.61, 0.385, 0.05), (-2.58, -0.592, 0.35, 0.055),
                   (-2.66, -0.58, 0.315, 0.05), (-2.74, -0.568, 0.284, 0.05), (-2.82, -0.555, 0.258, 0.045), (-2.90, -0.543, 0.23, 0.038),
                   (-2.98, -0.53, 0.205, 0.03), (-3.06, -0.518, 0.182, 0.02), (-3.14, -0.508, 0.165, 0.01), (-3.20, -0.495, 0.16, 0.003)],
                  0.25, 2.0, "coat", "fine")),
        # (the telescope sight out through the front pane of her windscreen; the hook along her belly under her tail, its
        #  point turned down at its forward end; the small post on the top of her fin; her pitot, out ahead of her
        #  starboard wing on its own, and its thinner tip; the closed doors in her belly under her pilot, where the
        #  bombardier lay to aim on a bombing run (shut for a torpedo attack); the raked mast under her belly behind her
        #  wing: all on the fine model only)
        ("rod", ((1.55, 0.0, 0.93), (2.51, 0.0, 0.915), 0.022, 0.026, "engine", "fine")),
        ("rod", ((-3.62, 0.0, -0.455), (-4.95, 0.0, -0.18), 0.02, 0.016, "leg", "fine")),
        ("rod", ((-3.62, 0.0, -0.455), (-3.52, 0.0, -0.515), 0.02, 0.014, "leg", "fine")),
        ("rod", ((-4.95, 0.0, -0.18), (-5.02, 0.0, -0.215), 0.018, 0.012, "leg", "fine")),
        ("rod", ((1.43, 4.893, -0.171), (2.22, 4.893, -0.171), 0.024, 0.024, "frame", "one", "fine")),
        ("rod", ((2.22, 4.893, -0.171), (2.46, 4.893, -0.171), 0.012, 0.009, "frame", "one", "fine")),
        ("rod", ((-5.67, 0.0, 2.40), (-5.67, 0.0, 2.50), 0.028, 0.024, "upper", "fine")),
        ("body", ([(1.48, -0.868, 0.02, 0.004), (1.45, -0.868, 0.13, 0.006), (1.02, -0.873, 0.13, 0.006), (0.99, -0.873, 0.02, 0.004)],
                  0.0, 4.0, "under", "fine")),
        ("rod", ((-1.93, 0.0, -0.72), (-2.13, 0.0, -1.07), 0.02, 0.014, "under", "fine")),
        # (the fairing at the root of her tailplane on each side, a rounded swelling of her side under it from its nose,
        #  flush with her at 4.03 m aft, out to a third of a metre from her middle under the tailplane's leading edge and
        #  faded back into her side by 4.97 m; its top hidden in the tailplane. On both models: it is her tail's shape)
        ("body", ([(-4.03, 0.59, 0.005, 0.005), (-4.10, 0.55, 0.05, 0.07), (-4.25, 0.50, 0.09, 0.08), (-4.50, 0.46, 0.07, 0.09),
                   (-4.70, 0.45, 0.05, 0.08), (-4.85, 0.45, 0.025, 0.065), (-4.97, 0.45, 0.006, 0.045)], 0.24, 2.0, "coat"))],
    "side_marks": [(-3.0, 0.40, "USA")], "wing_marks": [(5.2, 0.35, 0.62, "USA", "USA", True, True)], "code": (-4.35, 0.28),
    # (her side number in black, as the squadrons painted it in 1942; the game would paint white on so dark a blue. Her
    #  coats of 1943 and 1944 keep the game's white)
    "ink": "1A1A1A",
}

# Who sits under her glass (fitout.SEATS): her pilot under his sliding hood and the radioman-gunner at the back facing
# aft, at his gun. The middle place is empty: the bombardier-navigator stayed aboard for a torpedo attack, as at Midway.
SEATS = [(0, 1.20, 0.0, 0.0), (0, -1.42, 0.0, 180.0)]
FIN_FROM = -4.2                                             # (where her fin's fillet rises out of her back)


def coat(navy_blues):
    """Her three blues as the game paints her from 1943-02 (navy_blues comes from planes): the fleet's own field for her
    fuselage, and her rudder behind her post, where she has ended, intermediate blue as her fin is (the field would take
    it for wing); her elevators beside it stay sea blue."""
    blues = navy_blues(HULL, NUMBERS["square"], FIN_FROM)

    def graded(p):
        if p.x < POST and abs(p.y) < 0.2:
            return 1.0                                      # (her rudder's foot behind her post)
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
