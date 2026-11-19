"""Italy's aircraft for the Mediterranean war, on the fighters' tools (fighters.py: the framed canopy, the open
cockpit under it, the pilot seated by fitout). Importing this adds them to planes.py's machines and builders, so
export.py, airlook.py and the rest take them by name: `import planes_ita` before you ask for "C202".

The C.202 Folgore: laid out from her published measures (length 8.85 m, span 10.58 m, airscrew 3.05 m, wing 16.8 m2)
and the general arrangement every account of her gives: a slim inline nose with the oil cooler under it, the
radiator bath under her cockpit, a flat-paned hood hinged to starboard on a windscreen with one flat front pane,
a high spine faired down from the hood to her fin, a straight-tapered wing with rounded tips, a rounded fin.
Nothing of any drawing or model is used but its measures.

Her paint and marks are the art director's (19 Nov, 03:35Z): sand with green "smoke rings" over light grey; the white
band round her rear fuselage; the white cross on her rudder with a small plain shield at its middle; bare wings.
Each nation's plain period marking, never a party emblem: no fasces anywhere.

The SM.79 Sparviero: three radials, the nose one seated beside the wing ones by the sculptor's `nose_engine` (the
fighters' 3vdnfu 9d76161); sand with soft green blotches, the same band and cross. Her numbers are below hers.
"""
import math

from mathutils import Vector, noise

import fighters
import fitout
import planes
from fighters import SKIRT, bubble, deck, headed, hood_of, resample, tween

# ---- colours (as seen) ---------------------------------------------------------------------------------------------
SAND = "C2A878"           # (the light hazel sand of the desert fighters)
SMOKE = "4E5232"          # (the dark olive green of the rings)
UNDER = "A9ACA8"          # (the light grey under her)
WHITE = "E6E4DA"

# ---- the C.202 Folgore --------------------------------------------------------------------------------------------
# Her hood, (x, the height of its sill where the glass begins, half its width there, the height of its top): a box
# of flat panes, a little narrower at the back where it meets the faired spine.
_C202_HOOD = [(0.04, 0.45, 0.265, 0.80), (-0.22, 0.455, 0.27, 0.83), (-0.50, 0.46, 0.265, 0.83), (-0.80, 0.47, 0.25, 0.80)]
_C202_SCREEN = (0.36, 0.165, 0.115)        # (the foot of her windscreen's front pane, half its width there, and at its head)


def _c202_canopy(hull):
    foot_x, foot_half, head_half = _C202_SCREEN
    x0, sill, hw, top = _C202_HOOD[0]
    box = dict(square=4.5, n=4)                                # (four flat panes from sill to top: a box, framed)
    head = headed(bubble(hull, x0, sill, hw, top, **box), head_half)
    post = len(head) - 2
    foot = [deck(hull, foot_x, foot_half)] * (len(head) - 1) + [deck(hull, foot_x, 0.0)]
    screen = {"sections": [(foot_x, foot), (x0, head)], "skirt": SKIRT, "hoops": [-1], "rails": [post, SKIRT], "bar": 0.024}
    hood = [(x, bubble(hull, x, sl, w, t, **box)) for x, sl, w, t in _C202_HOOD[1:]]
    first = resample(head, len(hood[0][1]) - 1)
    hood = [(x0, first)] + tween(x0, first, hood[0][0], hood[0][1], 2) + hood
    # (frames round the hood at its front, its middle and its back; a rail along the sill and one along the
    #  side's upper edge, where the side panes meet the roof)
    slide = {"sections": hood, "skirt": SKIRT, "hoops": [0, 3, len(hood) - 1], "rails": [SKIRT, SKIRT + 2], "bar": 0.02, "cap": True}
    return [screen, slide]


# (the spine behind her hood: metal, from the hood's back down to her fin, with a small window each side at its head)
_C202_SPINE = [(-0.78, 0.56, 0.245, 0.235), (-1.2, 0.555, 0.215, 0.19), (-1.8, 0.535, 0.17, 0.13), (-2.5, 0.50, 0.12, 0.075),
               (-3.2, 0.45, 0.075, 0.035), (-3.6, 0.42, 0.04, 0.012)]

C202 = {
    "title": "C.202 Folgore (our own model)",
    "hull": [(3.25, 0.0, 0.03, 0.03), (3.17, 0.0, 0.12, 0.12), (3.04, 0.0, 0.19, 0.19), (2.90, 0.0, 0.235, 0.235),
             (2.85, -0.02, 0.29, 0.33), (2.65, -0.04, 0.32, 0.39), (2.25, -0.06, 0.355, 0.47), (1.70, -0.07, 0.38, 0.54),
             (1.10, -0.06, 0.395, 0.585), (0.45, -0.05, 0.40, 0.605), (-0.20, -0.03, 0.395, 0.60), (-0.90, -0.01, 0.37, 0.575),
             (-1.60, 0.01, 0.335, 0.53), (-2.40, 0.03, 0.29, 0.47), (-3.20, 0.05, 0.235, 0.40), (-3.95, 0.065, 0.175, 0.32),
             (-4.60, 0.07, 0.12, 0.235), (-5.10, 0.065, 0.075, 0.155), (-5.45, 0.06, 0.04, 0.085), (-5.60, 0.055, 0.012, 0.025)],
    "square": 2.3, "screw": 1.525, "blades": 3, "nose_back": 2.3, "spinner": 4,
    "hoods": hood_of([(_C202_SCREEN[0], 0.52, _C202_SCREEN[1], 0.01)] + [(x, sl, w, t - sl) for x, sl, w, t in _C202_HOOD]),
    "canopy": _c202_canopy,
    "opening": (0.06, -0.80, 54.0),
    "wing": [(0.0, 1.36, -0.96, -0.43, 0.37), (0.45, 1.34, -0.96, -0.41, 0.36), (1.5, 1.23, -0.91, -0.32, 0.31), (3.0, 1.06, -0.81, -0.19, 0.23),
             (4.4, 0.89, -0.69, -0.07, 0.16), (4.95, 0.79, -0.59, -0.02, 0.11), (5.2, 0.63, -0.43, 0.0, 0.06), (5.29, 0.36, -0.16, 0.01, 0.01)],
    "tail": [(0.0, -4.36, -5.40, 0.13, 0.10), (0.9, -4.46, -5.33, 0.13, 0.07), (1.4, -4.63, -5.22, 0.13, 0.04), (1.56, -4.86, -5.08, 0.13, 0.01)],
    "fin": [(0, -3.98, -5.60, 0.30, 0.11), (0, -4.34, -5.63, 0.80, 0.09), (0, -4.64, -5.56, 1.24, 0.065), (0, -4.90, -5.43, 1.47, 0.035),
            (0, -5.12, -5.28, 1.55, 0.01)],
    "patch": 0, "colours": {"upper": SAND, "upper2": SMOKE, "under": UNDER, "nose": SAND, "boss": SAND},
    "stubs": (2.45, 1.80, 18.0), "well": (1.15, 0.40, 0.44), "pitot": True, "glare": (2.75, 0.50, 0.15),
    "aileron": (0.60, 0.96, 0.74), "flap": (0.08, 0.60, 0.76),
    "guns": [],                                             # (her two heavy machine guns fire through the airscrew from under her cowling)
    "scoops": [
        # (the radiator bath under her cockpit, and the oil cooler under her nose)
        ([(0.55, -0.64, 0.20, 0.05), (0.45, -0.68, 0.23, 0.10), (-0.35, -0.69, 0.24, 0.11), (-0.80, -0.645, 0.20, 0.03)], 0.0, 3.0),
        ([(2.45, -0.43, 0.12, 0.05), (2.35, -0.48, 0.14, 0.08), (1.90, -0.50, 0.14, 0.08), (1.70, -0.47, 0.11, 0.02)], 0.0, 3.0)],
    "tailwheel": (-4.95, -0.30), "mast": (-1.05, 0.74, 1.12),
    "extras": [("body", (_C202_SPINE, 0.0, 2.2, "coat"))],
    "side_marks": [(-3.45, 0.3, "ITA-BAND")], "wing_marks": [], "code": (-2.15, 0.26),
    "band": (-3.25, -3.70),                                 # (the white band round her rear fuselage, from and to)
    "rings": (0.55, 0.09),                                  # (the smoke rings: how big a ring is, and how wide its line)
}

# ---- the SM.79 Sparviero ------------------------------------------------------------------------------------------
# Laid out from her published measures (length 16.2 m, span 21.2 m, wing 61.7 m2, airscrews about 3.1 m) and the
# arrangement every account of her gives: three radials, one in her nose and one in each wing (`nose_engine`, the
# fighters' sculptor, 3vdnfu 9d76161); a slim fuselage; the pilots side by side under a flat-paned hood at the front
# of the hump on her back, the hump running aft to the dorsal gunner's open position, a fixed gun over the hood; the
# bomb-aimer's gondola under her belly with its gun firing aft; a waist window and gun each side; a tapered wing with
# rounded tips set low; a tall rounded fin and a braced tailplane. Her paint (the art director, 19 Nov): sand with
# green blotches over light grey (the brown of the three-tone scheme left out: a third colour on the skin could only
# be whole faces, and read as squares), the white band, the rudder cross; bare wings.
_SM79_HUMP = [(2.95, 0.98, 0.42, 0.3), (2.6, 0.98, 0.5, 0.5), (2.0, 0.99, 0.52, 0.56), (1.2, 0.98, 0.5, 0.56), (0.4, 0.96, 0.46, 0.52),
              (-0.3, 0.92, 0.42, 0.46), (-0.7, 0.9, 0.38, 0.38), (-0.9, 0.86, 0.3, 0.24), (-1.0, 0.84, 0.2, 0.1)]
_SM79_GONDOLA = [(-0.4, -0.8, 0.1, 0.06), (-0.8, -0.92, 0.34, 0.22), (-1.8, -0.98, 0.38, 0.28), (-3.2, -0.96, 0.36, 0.26), (-4.0, -0.88, 0.28, 0.18),
                 (-4.35, -0.82, 0.16, 0.08)]

SM79 = {
    "title": "SM.79 Sparviero (our own model)", "year": 1937,
    "hull": [(7.05, 0.1, 0.06, 0.06), (6.95, 0.1, 0.16, 0.16), (6.82, 0.1, 0.52, 0.52), (6.75, 0.1, 0.66, 0.66), (6.3, 0.1, 0.7, 0.7), (5.95, 0.1, 0.68, 0.7),
             (5.6, 0.08, 0.66, 0.74), (4.5, 0.05, 0.7, 0.84), (3.0, 0.02, 0.72, 0.88), (1.0, 0.0, 0.72, 0.88), (-1.0, 0.0, 0.7, 0.84), (-3.0, 0.02, 0.62, 0.74),
             (-5.0, 0.06, 0.48, 0.6), (-7.0, 0.12, 0.32, 0.44), (-8.4, 0.18, 0.18, 0.3), (-9.05, 0.22, 0.06, 0.16), (-9.2, 0.23, 0.02, 0.05)],
    "square": 2.4, "screw": 1.55, "blades": 3, "nose_back": 5.95, "spinner": 2, "engine": "radial", "lip": 6.75, "nose_engine": True,
    # (the pilots' hood: flat panes from the windscreen's foot back to where the hump takes over)
    "hoods": [([(4.35, 0.86, 0.46, 0.04), (4.05, 1.0, 0.58, 0.26), (3.6, 1.06, 0.62, 0.32), (3.0, 1.08, 0.62, 0.33), (2.55, 1.06, 0.6, 0.3)],
               (4.05, 3.6, 3.15, 2.7), True)],
    "wing": [(0.0, 1.7, -2.3, -0.45, 0.66), (1.0, 1.68, -2.26, -0.42, 0.64), (3.9, 1.45, -2.05, -0.3, 0.55), (7.0, 0.95, -1.7, -0.1, 0.36),
             (9.4, 0.5, -1.35, 0.06, 0.22), (10.25, 0.25, -1.05, 0.12, 0.13), (10.55, -0.05, -0.75, 0.14, 0.05), (10.6, -0.35, -0.55, 0.145, 0.01)],
    "tail": [(0.0, -7.25, -8.95, 0.3, 0.2), (2.2, -7.55, -8.85, 0.3, 0.13), (3.0, -7.95, -8.7, 0.3, 0.06), (3.25, -8.25, -8.55, 0.3, 0.01)],
    "fin": [(0, -6.4, -9.1, 0.35, 0.16), (0, -7.0, -9.25, 1.0, 0.13), (0, -7.6, -9.3, 1.7, 0.1), (0, -8.1, -9.25, 2.15, 0.07), (0, -8.5, -9.1, 2.35, 0.04),
            (0, -8.8, -8.95, 2.42, 0.01)],
    "patch": 1.9, "colours": {"upper": SAND, "upper2": SMOKE, "under": UNDER, "nose": SAND, "boss": SAND},
    "engines": [(3.9, 1.45, -0.34, True, 3.2)], "pitot": True, "tailwheel": (-8.4, -0.45),
    "extras": [("body", (_SM79_HUMP, 0.0, 2.2, "coat")), ("body", (_SM79_GONDOLA, 0.0, 2.2, "coat")),
               # (the fixed gun over the hood; the dorsal gunner's gun at the hump's end; the gondola's gun; a waist gun each side)
               ("rod", ((2.75, 0.0, 1.45), (3.4, 0.0, 1.47), 0.02, 0.016, "steel")),
               ("rod", ((-0.85, 0.0, 1.02), (-1.75, 0.0, 1.3), 0.024, 0.018, "steel")),
               ("rod", ((-4.2, 0.0, -0.9), (-5.0, 0.0, -1.08), 0.022, 0.018, "steel")),
               ("rod", ((-3.45, 0.6, 0.34), (-4.05, 1.1, 0.28), 0.02, 0.016, "steel")),
               # (the gondola's windows, a window each side behind the wing, the waist windows)
               ("ball", ((-1.5, 0.37, -1.0), (0.42, 0.03, 0.11), "glass")),
               ("ball", ((-2.2, 0.66, 0.3), (0.22, 0.03, 0.14), "glass")),
               ("ball", ((-3.5, 0.6, 0.36), (0.3, 0.03, 0.18), "glass")),
               # (the tailplane's struts, from under it to her lower side)
               ("rod", ((-8.0, 1.5, 0.27), (-7.85, 0.24, -0.12), 0.024, 0.024, "upper"))],
    "side_marks": [(-6.15, 0.3, "ITA-BAND")], "wing_marks": [], "code": (-4.6, 0.32),
    "band": (-5.85, -6.45),
}

ITALIANS = {"C202": C202, "SM79": SM79}


# ---- what the sculptor does not do: the band, the rings, the rudder's cross ---------------------------------------
def rings_of(m, s):
    """Her smoke rings: thin green loops wandering over her sand, laid as the second upper colour point by point (the
    file's `blend`), so the game draws them soft-edged on her skin. Where the noise crosses its middle a line runs:
    its crossings close into loops, as the painters' rings did."""
    size, line = s["rings"]
    up = m.paint_at.get("upper")
    if up is None:
        return
    on = [False] * len(m.verts)
    for k, f in enumerate(m.faces):
        if m.fpaint[k] == up:
            for i in f:
                on[i] = True
    blend = list(m.vblend) + [0.0] * (len(m.verts) - len(m.vblend))
    for i, v in enumerate(m.verts):
        if not on[i]:
            continue
        p = Vector((v.x / size + 13.1, abs(v.y) / size + 4.7, v.z / size * 1.3 + 2.9))
        d = abs(noise.noise(p)) / (line / size)
        blend[i] = max(blend[i], max(0.0, 1.0 - d) ** 0.7)
    m.vblend = blend


def band_of(m, s, hull):
    """The white band round her rear fuselage: her skin's faces between its two ends, painted white."""
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
        if abs(c.y) > hull.w(-c.x) + 0.03 or abs(c.z - hull.z(-c.x)) > hull.h(-c.x) + 0.25:
            continue                                         # (her fuselage and the spine over it; nothing out on a tailplane)
        m.fpaint[k] = band
        if k < len(m.fcoat) and m.fcoat[k]:
            m.fcoat[k] = type(m.fcoat[k])(0)                 # (her coat no longer paints it point by point: it is white)


def cross_of(m):
    """The white cross on her rudder, both sides, a small plain shield at its middle: placed as the owner's flag is
    (planes.rudder_flag), its tag `ITA-CROSS`."""
    mk = planes.rudder_flag(m)
    mk["tag"] = "ITA-CROSS"
    return mk


def convert(name, fine=True):
    m, f = planes.convert(name, fine)
    s = planes.game()["PLANES"][name]
    hull = planes.game()["Body"](s["hull"], s.get("square", 2.0))
    if s.get("band"):
        band_of(m, s, hull)
    if s.get("rings"):
        rings_of(m, s)
    m.marks = [mk for mk in m.marks if mk["tag"] != "ITA-BAND"]          # (the band is in her paint, not a decal)
    if "rudder" in m.part_at:
        at = next((i for i, mk in enumerate(m.marks) if mk.get("place") == "code"), len(m.marks))
        m.marks.insert(at, cross_of(m))
    m.info.setdefault("file", {})["marks_note"] = ("Italy (art director, 19 Nov): the white band round the rear fuselage is painted on the skin; "
                                                   "ITA-CROSS is the white cross on the rudder with a small plain shield at its middle; the wings are bare")
    return m, f


# ---- taking her in -------------------------------------------------------------------------------------------------
planes.EXTRA_PLANES.update(ITALIANS)
if planes._G is not None:                                   # (the sculptor already loaded: give it them too)
    planes._G["PLANES"].update(ITALIANS)
for _n in ITALIANS:
    planes.BUILDERS[_n] = (lambda fine=True, n=_n: convert(n, fine))
planes.CREW.update({"C202": 1, "SM79": 5})                  # (the SM.79: two pilots, engineer, radio operator, gunner)
planes.ROLE["SM79"] = "bomber"
fitout.SEATS["SM79"] = [(0, 3.25, 0.3, 0.0), (0, 3.25, -0.3, 0.0)]       # (her two pilots side by side under the hood)
