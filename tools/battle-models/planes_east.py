"""The Soviet aircraft of the Eastern Front laid out again to the B-17 standard, in fighters.py's form: the Yak-1B
fighter and the Il-2M3 two-seat Sturmovik. They replace the game's old "Yak" and "Sturmovik" numbers in the battles.

No public-domain three-view of either was found, so their lines are laid out from their published measures (length,
span, wing area, height, airscrew) and the proportions every photograph of them shows; nothing of any drawing or model
is used. The numbers are in the game's form (see fighters.py and game/plane_numbers.py): stations of the fuselage as
(x, z of its middle, half its width, half its height), sections of a flying surface as (how far out, leading edge,
trailing edge, height, thickness); x ahead, z up, in metres, the airscrew's axis at z = 0.

    import planes_east; planes_east.install()          (adds them to planes.py's table)
    python planes_east.py <airlook.py's arguments>      e.g.  python planes_east.py Yak /tmp/out front,profile 960x540x16
    python planes_east.py --export <out> [json glb fbx]   writes both, and their damage overlays, as export.py does
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import bpy  # noqa: E402,F401  (before bmesh, which fighters.py takes)
import fighters  # noqa: E402
from fighters import SKIRT, bubble, deck, green, headed, hood_of, resample, tween  # noqa: E402


# ---- the Yak-1B (1942-43) --------------------------------------------------------------------------------------
# Length 8.48 m, span 10.0 m, wing area 17.15 m2, a three-bladed airscrew of 3.0 m. What makes her a Yak-1B and not
# the Yak-1 the game had: the rear fuselage cut down behind the seat and a bubble hood over it (the first Soviet
# fighter with all-round view), a big spinner with the cannon firing through it, the oil cooler's chin under the nose
# and the coolant radiator's bath under the wing's middle; a wing with a straight trailing edge and a leading edge
# swept back to small round tips; a tall, rounded fin. Her coat is the green and black over light blue of 1942.
#
# Her hood (x, the height of its sill, half its width there, the height of its top).
_YAK_BUBBLE = [(0.30, 0.43, 0.30, 0.80), (0.12, 0.44, 0.31, 0.835), (-0.10, 0.45, 0.31, 0.855), (-0.34, 0.46, 0.30, 0.85),
               (-0.58, 0.47, 0.285, 0.82), (-0.80, 0.48, 0.26, 0.775), (-0.98, 0.49, 0.22, 0.72), (-1.12, 0.50, 0.16, 0.66),
               (-1.22, 0.51, 0.09, 0.615), (-1.27, 0.515, 0.03, 0.59)]
_YAK_SCREEN = (0.66, 0.18, 0.10)                        # (the foot of her windscreen's front pane, half its width there, and at its head)


def _yak_canopy(hull):
    foot_x, foot_half, head_half = _YAK_SCREEN
    x0, sill, hw, top = _YAK_BUBBLE[0]
    head = headed(bubble(hull, x0, sill, hw, top), head_half)
    post = len(head) - 2
    foot = [deck(hull, foot_x, foot_half)] * (len(head) - 1) + [deck(hull, foot_x, 0.0)]
    screen = {"sections": [(foot_x, foot), (x0, head)], "skirt": SKIRT, "hoops": [-1], "rails": [post, SKIRT], "bar": 0.022}
    hood = [(x, bubble(hull, x, sl, w, t)) for x, sl, w, t in _YAK_BUBBLE[1:]]
    first = resample(head, len(hood[0][1]) - 1)
    hood = [(x0, first)] + tween(x0, first, hood[0][0], hood[0][1], 3) + hood
    slide = {"sections": hood, "skirt": SKIRT, "hoops": [0, 6], "rails": [SKIRT], "bar": 0.02, "cap": True, "bare": (len(hood) - 1,)}
    return [screen, slide]


YAK = {
    "title": "Yak-1B (our own model)",
    "hull": [(3.15, 0.0, 0.03, 0.03), (3.06, 0.0, 0.13, 0.13), (2.92, 0.0, 0.21, 0.21), (2.75, 0.0, 0.27, 0.27), (2.64, 0.0, 0.29, 0.29),
             (2.60, -0.03, 0.33, 0.37), (2.45, -0.05, 0.36, 0.44), (2.0, -0.06, 0.39, 0.49), (1.4, -0.05, 0.41, 0.53), (0.8, -0.02, 0.43, 0.56),
             (0.3, 0.0, 0.43, 0.57), (-0.3, 0.01, 0.42, 0.56), (-0.9, 0.03, 0.39, 0.52), (-1.5, 0.05, 0.345, 0.47), (-2.2, 0.08, 0.29, 0.41),
             (-3.0, 0.11, 0.225, 0.34), (-3.8, 0.14, 0.165, 0.27), (-4.5, 0.17, 0.11, 0.2), (-5.0, 0.2, 0.065, 0.13), (-5.27, 0.21, 0.03, 0.06),
             (-5.33, 0.21, 0.01, 0.02)],
    "square": 2.2, "screw": 1.50, "blades": 3, "nose_back": None, "spinner": 5,
    "hoods": hood_of([(_YAK_SCREEN[0], 0.52, _YAK_SCREEN[1], 0.01)] + [(x, sl, w, t - sl) for x, sl, w, t in _YAK_BUBBLE]),
    "canopy": _yak_canopy,
    "opening": (0.32, -0.75, 54.0),
    "wing": [(0.0, 1.30, -0.95, -0.47, 0.36), (0.43, 1.28, -0.95, -0.45, 0.33), (1.5, 1.13, -0.90, -0.37, 0.28), (2.8, 0.94, -0.82, -0.27, 0.21),
             (4.0, 0.74, -0.72, -0.17, 0.15), (4.55, 0.62, -0.64, -0.12, 0.11), (4.85, 0.48, -0.53, -0.1, 0.07), (5.0, 0.26, -0.34, -0.09, 0.01)],
    "tail": [(0.0, -4.25, -5.2, 0.26, 0.1), (0.5, -4.33, -5.17, 0.26, 0.09), (1.15, -4.47, -5.1, 0.26, 0.07), (1.48, -4.6, -5.02, 0.26, 0.04),
             (1.6, -4.74, -4.94, 0.26, 0.01)],
    "fin": [(0, -3.85, -5.33, 0.2, 0.12), (0, -4.15, -5.4, 0.55, 0.11), (0, -4.38, -5.38, 0.95, 0.085), (0, -4.6, -5.3, 1.25, 0.06),
            (0, -4.82, -5.18, 1.4, 0.035), (0, -4.98, -5.08, 1.44, 0.01)],
    "patch": 2.0, "colours": {"upper": "4F5E3E", "upper2": "2C2F2A", "under": "8FB0BF", "nose": "4F5E3E", "boss": "4F5E3E"},
    "stubs": (2.38, 1.62, 15.0), "well": (1.35, 0.36, 0.4), "well_in": True, "pitot": True,
    "aileron": (0.56, 0.95, 0.74), "flap": (0.08, 0.54, 0.76),
    "scoops": [
        # (the coolant radiator's bath under her belly, its mouth a little behind the wing's leading edge)
        ([(0.10, -0.68, 0.19, 0.08), (0.0, -0.71, 0.23, 0.11), (-0.55, -0.72, 0.25, 0.12), (-1.05, -0.68, 0.23, 0.09),
          (-1.45, -0.6, 0.15, 0.05), (-1.6, -0.55, 0.06, 0.02)], 0.0, 2.6),
        # (the oil cooler's chin under her nose)
        ([(2.47, -0.5, 0.11, 0.055), (2.38, -0.53, 0.14, 0.08), (1.95, -0.54, 0.14, 0.075), (1.6, -0.5, 0.09, 0.03)], 0.0, 2.4)],
    "tailwheel": (-4.75, -0.2), "mast": (-0.95, 0.5, 0.95),
    "side_marks": [(-2.6, 0.12, "SOV")], "wing_marks": [(3.3, 0.1, 0.42, None, "SOV", False, True)], "fin_marks": [(-4.7, 0.85, 0.22, "SOV")],
    "code": (-1.6, 0.28),
}


# ---- the Il-2M3 Sturmovik (1943) ------------------------------------------------------------------------------
# Length 11.6 m, span 14.6 m, wing area 38.5 m2, a three-bladed airscrew of 3.6 m; two aboard. What makes her an
# Il-2M3: the armoured nose, deep and long, with the coolant radiator's intake as a hump on top behind the spinner and
# the oil cooler's chin under it; the pilot high under a hood of flat panes, the gunner behind him facing aft at an
# open ring with his heavy machine gun, under the after end of the glazing; the outer wings swept back (the "arrow"
# wing of 1943) on a straight centre section; the main wheels folding back into pods under the centre section, half
# out of them; two 23 mm cannon and two machine guns in the wings, and four rocket rails under each outer wing.
#
# Her glazing (x, the height of its sill, how far out the glass begins, the height of its top): the pilot's hood,
# then the fixed glazing behind it running down to the gunner's ring.
_IL2_HOOD = [(1.10, 0.66, 0.37, 1.20), (0.85, 0.66, 0.37, 1.225), (0.55, 0.66, 0.37, 1.235), (0.25, 0.66, 0.37, 1.23)]
_IL2_BACK = [(0.22, 0.65, 0.365, 1.215), (-0.05, 0.65, 0.36, 1.19), (-0.35, 0.65, 0.355, 1.14), (-0.62, 0.65, 0.35, 1.07)]
_IL2_SCREEN = (1.52, 0.17, 1.13, 1.17)                  # (the foot of her windscreen's front pane, half its width, its head, and the top there)


def _il2_shoulder(sill, top):
    return (0.24, top - (top - sill) * 0.3)


def _il2_canopy(hull):
    foot_x, foot_half, head_x, head_top = _IL2_SCREEN
    x0, sill, ys, top = _IL2_HOOD[0]
    sec = {x: green(hull, x, sl, w, _il2_shoulder(sl, t), t, roof=2) for x, sl, w, t in _IL2_HOOD + _IL2_BACK}
    head = headed(green(hull, head_x, sill, ys, _il2_shoulder(sill, head_top), head_top, roof=2), foot_half)
    post = len(head) - 2
    foot = [deck(hull, foot_x, foot_half)] * (len(head) - 1) + [deck(hull, foot_x, 0.0)]
    screen = {"sections": [(foot_x, foot), (head_x, head), (x0, sec[x0] if len(sec[x0]) == len(head) else resample(sec[x0], len(head) - 1))],
              "skirt": SKIRT, "hoops": [1, 2], "rails": [post, SKIRT], "bar": 0.024}
    n = len(sec[x0]) - 1
    hood = {"sections": [(x, sec[x]) for x, sl, w, t in _IL2_HOOD], "skirt": SKIRT, "hoops": [0, 1, 3], "rails": [SKIRT, 3, n], "bar": 0.02}
    back = {"sections": [(x, sec[x]) for x, sl, w, t in _IL2_BACK], "skirt": SKIRT, "hoops": [0, 2, 3], "rails": [SKIRT, 3], "bar": 0.02, "cap": True}
    return [screen, hood, back]


def _rails():
    """Four rocket rails under each outer wing, a rocket on each (paint `bomb`), the rails `leg` (not `steel`, which
    planes.py takes for a gunner's gun)."""
    out = []
    for y, le, z in ((3.3, 1.71, -0.735), (3.75, 1.59, -0.695), (4.2, 1.47, -0.655), (4.65, 1.35, -0.615)):
        out.append(("rod", ((le - 0.2, y, z), (le - 1.3, y, z), 0.016, 0.016, "leg")))
        out.append(("rod", ((le - 0.25, y, z - 0.06), (le - 1.1, y, z - 0.06), 0.041, 0.041, "bomb")))
        out.append(("ball", ((le - 0.25, y, z - 0.06), (0.07, 0.041, 0.041), "bomb")))
    return out


IL2 = {
    "title": "Il-2M3 Sturmovik (our own model)",
    "hull": [(4.5, 0.0, 0.03, 0.03), (4.4, 0.0, 0.14, 0.14), (4.25, 0.0, 0.22, 0.22), (4.05, 0.0, 0.28, 0.28), (3.92, 0.0, 0.30, 0.30),
             (3.88, -0.05, 0.38, 0.47), (3.5, -0.08, 0.44, 0.58), (2.8, -0.08, 0.48, 0.66), (2.0, -0.05, 0.50, 0.70), (1.3, 0.0, 0.52, 0.74),
             (0.5, 0.03, 0.53, 0.75), (-0.3, 0.05, 0.52, 0.74), (-1.0, 0.07, 0.50, 0.70), (-1.6, 0.09, 0.46, 0.64), (-2.4, 0.12, 0.40, 0.55),
             (-3.4, 0.17, 0.32, 0.45), (-4.5, 0.22, 0.24, 0.35), (-5.5, 0.27, 0.16, 0.26), (-6.3, 0.30, 0.10, 0.18), (-6.9, 0.32, 0.04, 0.09),
             (-7.1, 0.33, 0.01, 0.02)],
    "square": 2.5, "screw": 1.8, "blades": 3, "nose_back": None, "spinner": 5,
    "hoods": hood_of([(_IL2_SCREEN[0], 0.76, _IL2_SCREEN[1], 0.01), (_IL2_SCREEN[2], 0.66, 0.37, _IL2_SCREEN[3] - 0.66)]
                     + [(x, sl, w, t - sl) for x, sl, w, t in _IL2_HOOD + _IL2_BACK]),
    "canopy": _il2_canopy,
    "opening": (1.12, -1.65, 56.0),                         # (both cockpits: the pilot's under the hood, the gunner's open at his ring)
    "frames_paint": "upper",
    "wing": [(0.0, 2.0, -1.45, -0.62, 0.52), (0.55, 2.0, -1.44, -0.61, 0.5), (1.5, 1.98, -1.4, -0.6, 0.49), (2.4, 1.95, -1.35, -0.6, 0.47),
             (3.4, 1.68, -1.22, -0.495, 0.4), (4.6, 1.36, -1.06, -0.37, 0.32), (5.8, 1.04, -0.9, -0.24, 0.24), (6.7, 0.8, -0.78, -0.15, 0.17),
             (7.1, 0.62, -0.68, -0.11, 0.11), (7.3, 0.3, -0.45, -0.09, 0.02)],
    "tail": [(0.0, -5.75, -7.0, 0.36, 0.14), (0.6, -5.82, -6.98, 0.36, 0.12), (1.5, -5.98, -6.88, 0.36, 0.09), (2.2, -6.15, -6.75, 0.36, 0.05),
             (2.45, -6.35, -6.6, 0.36, 0.01)],
    "fin": [(0, -5.4, -7.1, 0.3, 0.14), (0, -5.85, -7.2, 0.8, 0.12), (0, -6.15, -7.18, 1.3, 0.09), (0, -6.4, -7.1, 1.68, 0.06),
            (0, -6.62, -6.98, 1.85, 0.03), (0, -6.8, -6.88, 1.88, 0.01)],
    "patch": 2.6, "colours": {"upper": "4F5E3E", "upper2": "2C2F2A", "under": "8FB0BF", "nose": "4F5E3E", "boss": "4F5E3E"},
    "stubs": (3.55, 2.55, 15.0), "pitot": True,
    "aileron": (0.62, 0.97, 0.74), "flap": (0.06, 0.6, 0.76),
    "guns": [(2.05, 0.1, 0.55, 0.03), (2.35, 0.05, 0.14, 0.015)],
    "scoops": [
        # (the coolant radiator's intake: a hump on top of her nose behind the spinner)
        ([(3.84, 0.45, 0.15, 0.06), (3.74, 0.5, 0.2, 0.1), (3.3, 0.53, 0.22, 0.11), (2.9, 0.53, 0.18, 0.07), (2.6, 0.5, 0.08, 0.02)], 0.0, 2.6),
        # (the oil cooler's chin under her nose)
        ([(3.7, -0.62, 0.12, 0.06), (3.6, -0.66, 0.16, 0.09), (3.1, -0.7, 0.17, 0.1), (2.6, -0.66, 0.12, 0.05), (2.3, -0.6, 0.05, 0.02)], 0.0, 2.4),
        # (the radiator's outlet under the centre section, its shutter half open)
        ([(0.6, -0.86, 0.2, 0.08), (0.2, -0.9, 0.26, 0.12), (-0.5, -0.9, 0.26, 0.12), (-0.9, -0.84, 0.2, 0.06), (-1.1, -0.8, 0.08, 0.02)], 0.0, 2.4)],
    "tailwheel": (-6.5, -0.28), "mast": (1.62, 0.73, 1.32, 0.06),
    "extras": [
        # (the main wheels' pods under the centre section, each wheel half out of its pod)
        ("body", ([(2.35, -0.80, 0.04, 0.04), (2.0, -0.84, 0.22, 0.24), (0.8, -0.86, 0.26, 0.27), (-0.4, -0.84, 0.24, 0.24), (-1.1, -0.74, 0.06, 0.06)],
                  1.75, 2.4, "coat")),
        ("rod", ((0.85, 1.67, -1.0), (0.85, 1.83, -1.0), 0.38, 0.38, "tyre")),
        # (the gunner's heavy machine gun on its ring, laid aft and a little up)
        ("rod", ((-1.35, 0.0, 0.93), (-2.4, 0.0, 1.13), 0.022, 0.018, "steel")),
    ] + _rails(),
    "side_marks": [(-3.4, 0.3, "SOV")], "wing_marks": [(4.8, 0.2, 0.55, None, "SOV", False, True)], "fin_marks": [(-6.45, 1.05, 0.26, "SOV")],
    "code": (-2.4, 0.34),
}


EAST = {"Yak": YAK, "Sturmovik": IL2}


def install():
    """Puts the machines in planes.py's own table (and in its sculptor's, if that has already run)."""
    import planes
    planes.EXTRA_PLANES.update(EAST)
    if planes._G is not None:
        planes._G["PLANES"].update(EAST)


if __name__ == "__main__":
    install()
    if sys.argv[1:2] == ["--export"]:
        names = ",".join(n + d for n in EAST for d in ("", "Damage"))
        sys.argv = ["export.py", sys.argv[2], names] + (sys.argv[3:] or ["json", "glb", "fbx"])
        import export  # noqa: F401,E402  (export.py runs as it is imported)
    else:
        sys.argv = ["airlook.py"] + sys.argv[1:]
        import airlook  # noqa: F401,E402  (airlook.py renders as it is imported)
