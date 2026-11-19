"""The aircraft as battle models.

The game already has its aircraft, sculpted by `Tools/build_plane.py` from the numbers in `plane_numbers.py` (copies
of both are in `game/`). Nothing is sculpted again here: each machine is built by that script and then taken apart
into the parts a battle moves:

    plane               the fuselage (the root: the game flies it, the clips only tip and roll it)
    wing.port/starboard each wing outboard of the fuselage, with whatever it carries; breaks away at its root
    tail                the rear fuselage with tailplane, fins and tailwheel; breaks away behind the wing
    prop.N              the blades of each airscrew (numbered from the port side), turning about its hub
    prop.N.blur         the disc a fast-turning airscrew is seen as
    bombs               what hangs under her and can be dropped (where she has it)

    aileron.port/starboard, elevator.port/starboard, rudder (rudder.port/starboard on twin fins), and flap.port/
                        starboard on the single-engined machines: each control surface, hinged where its line is scribed
    turret.N            a gun turret (its dome, its frames, its gunner), training about its ring; turret.N.guns lift
    flexgun.N           a gun on a free mounting (a rear gunner's, a waist gun, a nose gun)

Where a part breaks away both torn ends are closed with the paint `torn`. The paints are the game's own, in its
order, so the game's colours, camouflage patches (blend), soot and wear all carry through; `torn`, `blur` and the
paints of the fit-out (see fitout.py: her crew, her cockpit, her exhausts, her lamps) are added after them.

A battle looks at these machines from a wing's length away, so the game's sculptor is run at a finer grain here
(FINER below: the same curves, more rings), with a ring exactly at each end of every control surface and a line of
points along every hinge, so that the surfaces come away cleanly.

Also here: the parachute and the bomb, small models of their own.

    import planes;  m, facts = planes.BUILDERS["Spitfire"](True)
"""
import math
import os

import bmesh
import bpy
from mathutils import Quaternion, Vector, noise

import fighters
import fitout
import rigid
from rigid import Model, smooth

HERE = os.path.dirname(os.path.abspath(__file__))
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
_G = None


# Where the rings of a flying surface stand along its chord: close at the nose, and one each side of every hinge line
# (ailerons at 0.74, flaps at 0.76, elevators at 0.62, rudders at 0.60).
CHORDS = [0.0, 0.01, 0.04, 0.09, 0.16, 0.25, 0.36, 0.48, 0.60, 0.62, 0.74, 0.76, 0.86, 0.94, 0.98, 1.0]
CHORDS_PLAIN = [0.0, 0.1, 0.35, 0.62, 0.75, 0.9, 1.0]
# (with `grain` in her numbers, a coat of straight-edged wedges: her wing's rings stand closer, and along its chord too)
CHORDS_CLOSE = [0.0, 0.01, 0.04, 0.09, 0.125, 0.16, 0.205, 0.25, 0.305, 0.36, 0.42, 0.48, 0.54, 0.60, 0.62, 0.68, 0.74, 0.76, 0.81, 0.86, 0.90, 0.94, 0.98, 1.0]
BATTLE = {"hull": 0.15, "wing": 0.24, "tail": 0.22, "fin": 0.2}        # (how far apart the rings stand; set for each machine)
FLAP, AILERON, ELEVATOR, RUDDER = (0.1, 0.5, 0.76), (0.54, 0.93, 0.74), (0.08, 0.95, 0.62), (0.06, 1.0, 0.60)
# Where a bomber's bay is: from, to along her (its doors are the strip of her belly BAY_HALF degrees each side of her keel).
BAYS = {"He111": (3.3, 1.3), "Lancaster": (6.4, -3.0), "B17": (2.3, -0.1), "Bomber": (2.0, -0.8), "Avenger": (2.3, -2.0), "TorpedoBomber": (2.0, -1.9),
        "Betty": (2.8, -2.7), "Amerikabomber": (2.6, -2.2), "B29": (6.4, 2.9)}
# The machines whose bay holds a torpedo, or bombs instead (the loads are parts `torpedo` and `bombs`: one is hidden).
BAY_TORPEDO = {"Avenger": (4.1, 0.28), "TorpedoBomber": (3.8, 0.24), "Betty": (5.3, 0.225)}       # (how long it is, half as thick)
BAY_HALF = 27.0
# Open windows in a bomber's sides where a gunner stands at his gun: (from, to along her; from, to in degrees above the
# level of her middle). They are cut on the lines of her skin's own rings, on the fine model only.
WAIST = {"B17": (-4.84, -5.96, 0.0, 41.0), "Amerikabomber": (-5.0, -5.8, 0.0, 27.0)}
# Windows the game's numbers give a machine, made larger where a battle looks in at a man: (from, to, half its height),
# and after those, where a window is not at the usual height, how far up her side its middle is (0 level, 1 her top).
# The B-17's: her nose's cheek window and the two small ones behind it, and the radio room's.
GLASS = {"B17": [(7.43, 6.9, 0.25, 0.33), (6.46, 6.14, 0.13, 0.19), (6.04, 5.72, 0.13, 0.19), (-0.52, -0.87, 0.09, 0.59)]}
# A closed gun position's windows: panes of glass set in her skin, each (from, to along her; from, to round her side
# in degrees up from level), on both sides alike; a pane that runs up to 90 is one panel across her back. They are
# framed by fitout.tail_gunner. (The art director, 2026-11-18: one clear band round her tail read as a missing panel.)
PANES = {"B17": [(-12.3, -12.86, 25.0, 66.0), (-13.03, -13.38, 45.0, 90.0)], "B29": [(-18.04, -18.74, 0.0, 36.0), (-18.51, -19.2, 45.0, 90.0)],
         "Betty": [(-10.45, -11.4, -27.0, 90.0)]}                           # (the Betty's tail is a cone of glass round her gunner)
# The machines whose every turret is aimed from somewhere else in her: nobody sits in one.
REMOTE = {"B29"}
# (limits a ruling has set for one aircraft's turrets, by the turret's number: (train, lift). The historian, 2026-11-18
#  21:19Z, for the Lancaster: her nose turret 95 either side and 45 below to 60 above; her mid-upper all round, from
#  level to 70 above; her tail turret 95 either side and 60 below to 60 above, the widest field in her.)
TURRET_LIMITS = {"Lancaster": {1: ((-95, 95), (-45, 60)), 2: ((-180, 180), (0, 70)), 3: ((-95, 95), (-60, 60))}}
# Where a machine's tail comes away, where the usual place (a little aft of the middle between wing and tailplane) would
# cut through a compartment: the B-17's, just aft of her waist gunners.
TAIL_CUT = {"B17": -6.8}
SPATS = {"Val"}                                                           # (a fixed undercarriage in spats, painted as her sides are)
LINE = 0.4                                                                # (how wide a scribed line is drawn, as a share of the map's)

# The game's sculptor at a battle's grain: each line of it that sets a grain, and what stands there instead.
FINER = [
    ("around = 28 if fine else 14", "around = 40 if fine else 14"),
    ("hull.xs(0.24 if fine else 0.6)", "hull.xs(BATTLE['hull'] if fine else 0.6)"),
    ("hood_around = 20 if fine else 10", "hood_around = 32 if fine else 10"),
    ("hood.xs(0.2 if fine else 0.5)", "hood.xs(0.1 if fine else 0.5)"),
    ("chords = [0.5 * (1.0 - math.cos(math.pi * k / (9 if fine else 5))) for k in range((9 if fine else 5) + 1)]", "chords = BATTLE_CHORDS[fine]"),
    ("wing.us(0.42 if fine else 1.2)", "battle_us(wing, BATTLE['wing'] if fine else 1.2, s.get('flap', BATTLE_FLAP)[:2] + s.get('aileron', BATTLE_AILERON)[:2])"),
    ("tail.us(0.4 if fine else 1.0)", "battle_us(tail, BATTLE['tail'] if fine else 1.0, BATTLE_ELEVATOR[:2])"),
    ("fin.us(0.3 if fine else 0.8)", "battle_us(fin, BATTLE['fin'] if fine else 0.8, BATTLE_RUDDER[:1])"),
    ("n_around = 20 if fine else 10", "n_around = 28 if fine else 10"),
    # (what hangs under her or stands on her: a spat, a wheel, a bomb, a gun barrel, round close up and not cut like a gem)
    ('parts.ball("extra", (at[0], at[1] * hand, at[2]), radii, paint)', 'parts.ball("extra", (at[0], at[1] * hand, at[2]), radii, paint, u=28, v=18)'),
    ("ra, rb, paint=paint, sides=10)", "ra, rb, paint=paint, sides=(22 if max(ra, rb) > 0.08 else 12))"),
    ("[[body.at(x, 2.0 * math.pi * k / 12.0) for k in range(12)] for x in body.xs(0.3)]",
     "[[body.at(x, 2.0 * math.pi * k / 24.0) for k in range(24)] for x in body.xs(0.12)]"),
    ('parts.rod("tailwheel", (x, -0.04, z), (x, 0.04, z), 0.12, paint="tyre", sides=12 if fine else 6)', 'parts.rod("tailwheel", (x, -0.04, z), (x, 0.04, z), 0.12, paint="tyre", sides=20 if fine else 6)'),
]


# A battle's own machines, sculpted from numbers in the game's form (the game has no jet). `jets` is new: the pods of
# jet engines under the wings, each (its stations, how far out), open at both ends, with no airscrew anywhere.
# A coat that is laid by where a point is, not in patches: the numbers give `graded`, a function of a point that is
# above nought in the second upper colour (and soft for a hand's breadth each side of nought).
GRADED = [
    ('    def field(p):\n        return noise.noise(', '    def field(p):\n        if s.get("graded"):\n            return s["graded"](p)\n        return noise.noise('),
    ('        if p.x > nose_back:\n            return "nose"', '        if p.x > nose_back and not s.get("graded"):\n            return "nose"'),
    # (`taper`: a fuselage that is a rounded triangle, not an oval: fitout.hull_body; a marking on its sloping side
    #  lies on the slope, and does not stand upright half inside her)
    ('    hull = Body(s["hull"], s.get("square", 2.0))\n', '    hull = __import__("fitout").hull_body(Body, s)\n'),
    ('"facing": [round(n.x, 3), round(n.y, 3), 0.0], "curve": round(bend, 3), "both": True})',
     '"facing": [round(n.x, 3), round(n.y, 3), round(n.z, 3) if hasattr(hull, "round_of") else 0.0], "curve": round(bend, 3), "both": True})'),
]


# A nose of glass with no step (`windows_back`): behind the part that is glass all round, the windows over the pilots
# are framed as the nose is, down to where her metal belly begins.
# `nose_bar` is how thick her nose's frames are against the game's (the art director, 2026-11-18, of the two heavies'
# first noses: "white birdcages"; a third as thick, and in her skin's own finish: `nose_frames_paint`). `nose_cap` is
# where the ribs begin: ahead of it is one round pane (the B-29's nose was a dome of a few rings of large panes).
# The cooling gills round the back of a radial's cowling (the art director, 2026-11-18: in the canopy frames' pale
# paint they read "as a white collar"). They are their own mesh, `gills`, painted as the cowl is, and between each
# pair is a dark slit, so that they are still seen.
GILLS = [
    ("def radial_details(parts, frames, body, mouth, gills):", "def radial_details(parts, frames, body, mouth, gills, lines=None, plates=None):"),
    ("        parts.strip(frames, [body.at(gills + 0.1, a, 0.0) + (body.at(gills - 0.14, a, 0.03) - body.at(gills + 0.1, a, 0.0)) * 0.5 for a in steps],\n"
     "                    [body.out(gills, a) for a in steps], 0.12, 0.012)\n",
     "        parts.strip(plates if plates is not None else frames, [body.at(gills + 0.1, a, 0.0) + (body.at(gills - 0.14, a, 0.03) - body.at(gills + 0.1, a, 0.0)) * 0.5 for a in steps],\n"
     "                    [body.out(gills, a) for a in steps], 0.12, 0.012)\n"
     "        if lines is not None and parts.fine:\n"
     "            gap = 2.0 * math.pi * k / 16.0\n"
     "            parts.strip(lines, [body.at(gills + 0.1 - 0.08 * j, gap, 0.0) for j in range(4)], [body.out(gills, gap)] * 4, 0.038 * body.w(-gills) / LINE_DRAWN, 0.008)\n"),
    ("            radial_details(parts, frames, hull, lip, nose_back if nose_back < 1.0e8 else lip - 1.0)\n",
     "            radial_details(parts, frames, hull, lip, nose_back if nose_back < 1.0e8 else lip - 1.0, lines, gill_plates)\n"),
    ("                radial_details(parts, frames, nacelle, lead + 1.75, lead + 0.75)\n",
     "                radial_details(parts, frames, nacelle, lead + 1.75, lead + 0.75, lines, gill_plates)\n"),
    ("    frames = bmesh.new()\n", "    frames = bmesh.new()\n    gill_plates = bmesh.new()\n"),
    ('    parts.add("frames", frames, "frame", False, outward=False)\n',
     '    parts.add("frames", frames, "frame", False, outward=False)\n    parts.add("gills", gill_plates, "nose", False, outward=False)\n'),
]

# A window in her side at a height of its own (see GLASS), and a cabin: a hood that is a raised cabin with a roof, glass
# only where its windows are (`cabin` in her numbers; see cabin_paint and cabin_frames).
CABIN = [
    ('        for (x0, x1, half_high) in s.get("glass_bands", []):', '        for (x0, x1, half_high, *high) in s.get("glass_bands", []):'),
    ("abs(p.z - hull.z(-p.x) - hull.h(-p.x) * 0.3) < half_high:", "abs(p.z - hull.z(-p.x) - hull.h(-p.x) * (high[0] if high else 0.3)) < half_high:"),
    ('                   lambda p, n: "glass")\n        # (the frames are on the table\'s model too: they read from above)\n',
     '                   battle_cabin_paint(s["cabin"], hood, coat, hull) if s.get("cabin") else (lambda p, n: "glass"))\n'
     '        if s.get("cabin"):\n'
     '            battle_cabin_frames(parts, frames, hood, s["cabin"], hull)\n'
     '            continue\n'
     '        # (the frames are on the table\'s model too: they read from above)\n'),
    # (a point on the edge of a window takes her coat, not the glass's colour: no pale halo round a pane)
    ('            paint = paint_of(v.co, v.normal)\n            if blend_of and paint in ("upper", "upper2"):',
     '            paint = paint_of(v.co, v.normal)\n'
     '            if paint == "glass" and getattr(paint_of, "coat", None):\n'
     '                paint = paint_of.coat(v.co, v.normal)\n'
     '            if blend_of and paint in ("upper", "upper2"):'),
    ('    hull_paint.blend = coat.blend\n', '    hull_paint.blend = coat.blend\n    hull_paint.coat = coat\n'),
    # (a cabin is a box with flat panes, not a round hood: its own body, and a finer mesh so that its panes' edges are clean)
    ('        hood = Body(hood_stations, 2.3)\n', '        hood = battle_cabin_body(Body, hood_stations, s["cabin"], hull) if s.get("cabin") else Body(hood_stations, 2.3)\n'),
    ('hood_around = 32 if fine else 10', 'hood_around = ((64 if fine else 32) if s.get("cabin") else 32 if fine else 10)'),
]

NOSES = [
    ("        xs = [hull.front - 0.02 - (hull.front - glazed[0] - 0.04) * k / 12.0 for k in range(13)]\n",
     "        x_tip = s.get('nose_cap', hull.front - 0.02)\n"
     "        bar = s.get('nose_bar', 1.0)\n"
     "        xs = [x_tip - (x_tip - glazed[0] - 0.02) * k / 12.0 for k in range(13)]\n"),
    ("            parts.strip(frames, [hull.at(x, a) for x in xs], [hull.out(x, a) for x in xs], 0.016, 0.008)\n",
     "            parts.strip(frames, [hull.at(x, a) for x in xs], [hull.out(x, a) for x in xs], 0.016 * bar, 0.008)\n"
     "            if s.get('windows_back') and hull.out(glazed[1], a).z > -0.5:\n"
     "                more = [glazed[0] - (glazed[0] - glazed[1]) * j / 6.0 for j in range(7)]\n"
     "                parts.strip(frames, [hull.at(x, a) for x in more], [hull.out(x, a) for x in more], 0.016 * bar, 0.008)\n"),
    ("            parts.strip(frames, [hull.at(x, a) for a in angles], [hull.out(x, a) for a in angles], 0.018, 0.008)\n",
     "            ring = angles if x >= glazed[0] - 0.01 else [math.radians(-30.0 + 240.0 * k / 30.0) for k in range(31)]\n"
     "            parts.strip(frames, [hull.at(x, a) for a in ring], [hull.out(x, a) for a in ring], 0.018 * bar, 0.008)\n"),
]


def navy_blues(stations, square, fin_from, line=0.42):
    """The US Navy's three blues over white of 1943: sea blue on top of her, intermediate blue down her sides from a
    line above her middle and on her fin and rudder, white beneath (the under paint)."""
    box = []

    def field(p):
        if not box:
            box.append(game()["Body"](stations, square))
        body = box[0]
        x = min(max(p.x, body.back), body.front)
        w, h, z = body.w(-x), body.h(-x), body.z(-x)
        if abs(p.y) > w + 0.03:
            return -1.0                                  # (her wings and tailplane)
        if p.x < fin_from and p.z > z + h + 0.04:
            return 1.0                                   # (her fin and rudder)
        return (z + line * h) - p.z
    return field


# ---- the Me 262 (Brian, 2026-11-19, of four pictures of one: "here is the ME 262 for insipiration"; "dont do the
# shark nose"). Laid out again against "Messerschmitt Me 262 Schwalbe 3d drawing" (Wikimedia Commons, Voytek S,
# CC BY-SA 3.0) at 167.7 pixels to the metre, her length 10.6 m: measured, not traced. What changed: her fuselage
# is a rounded triangle, widest low down (`taper`), and lower and straighter along her back; her hood is low, with
# flat sloping sides, a flat pane in front between two posts, a hinged middle and a long fixed tail of glass, over
# an open cockpit (the fighters' `canopy`: fighters.py); her fin is 0.6 m lower and her tailplane half way up it;
# her pods hang further out; a wing with its kinked trailing edge. Her nose is plain: four cannon ports, no mouth,
# no eyes. Her coat is a splinter of grey-green and pale grey along her top, with her pale blue-grey coming high up
# her sloping sides and over her fin, blotched with the upper colours as they were sprayed.
_ME262_HULL = [(4.45, -0.03, 0.02, 0.02), (4.4, -0.03, 0.09, 0.08), (4.3, -0.04, 0.18, 0.15), (4.0, -0.022, 0.31, 0.248), (3.5, -0.022, 0.48, 0.378),
               (3.0, -0.025, 0.615, 0.475), (2.5, -0.03, 0.72, 0.54), (2.0, -0.03, 0.8, 0.59), (1.5, -0.0275, 0.84, 0.6275), (1.0, -0.0225, 0.85, 0.6425),
               (0.0, -0.0325, 0.85, 0.6325), (-1.0, -0.0475, 0.82, 0.6075), (-1.5, -0.0425, 0.76, 0.5775), (-2.0, -0.0425, 0.67, 0.5175),
               (-3.0, -0.0425, 0.5, 0.4275), (-3.75, -0.05, 0.37, 0.35), (-4.4, -0.07, 0.265, 0.27), (-5.0, -0.085, 0.17, 0.205),
               (-5.5, -0.0975, 0.1, 0.1475), (-6.0, -0.06, 0.035, 0.06), (-6.15, -0.02, 0.01, 0.015)]
_ME262_TAPER = (0.36, -0.6, [(4.45, 0.0), (4.1, 0.3), (3.2, 0.85), (2.4, 1.0), (-3.2, 1.0), (-5.0, 0.6), (-6.15, 0.3)])
_ME262_EDGE = 46.0                                          # (her cockpit's rim: this far round her from level)
# Her hood: (x, the height its glass begins at, the height of its top, half the width of the flat of its top).
_ME262_HOOD = [(0.47, 0.5, 0.905, 0.1), (0.2, 0.5, 0.955, 0.085), (-0.1, 0.5, 0.94, 0.085), (-0.38, 0.5, 0.895, 0.08)]
# The fixed glass behind it: (x, where its glass begins: out and up, the height of its top, half the flat of its top,
# how far out its metal foot stands on her: None along her cockpit's rim).
_ME262_REAR = [(-0.38, None, 0.5, 0.895, 0.08, None), (-0.6, 0.24, 0.565, 0.84, 0.065, None), (-0.8, 0.15, 0.625, 0.795, 0.045, None),
               (-1.0, 0.05, 0.685, 0.725, 0.02, 0.3), (-1.15, 0.012, 0.7, 0.705, 0.004, 0.26), (-1.5, 0.004, 0.537, 0.54, 0.002, 0.1)]
_ME262_SCREEN = (0.97, 0.2)                                 # (the foot of her windscreen's flat pane, and half its width there)
_ME262_BOX = []


def _me262_hull():
    if not _ME262_BOX:
        _ME262_BOX.append(fitout.hull_body(game()["Body"], {"hull": _ME262_HULL, "square": 3.0, "taper": _ME262_TAPER}))
    return _ME262_BOX[0]


def _me262_section(hull, x, sill, top, flat, out=None, foot=None):
    """Half a section of her hood, nine points: the foot of its metal rail on her (two points), then its glass from
    the sill straight up her sloping side to the shoulder, round the shoulder, and level to her middle line."""
    if foot is None:
        edge = hull.at(x, math.radians(_ME262_EDGE - 7.0))     # (down her side from the rim, and clear of it: the rail covers the rim's padding)
        fy, fz = edge.y + 0.014, edge.z
    else:
        fy, fz = foot, hull.top_at(x, foot) - 0.006
    sy = hull.side_at(x, sill) + 0.012 if out is None else out
    sy = max(sy, flat + 0.002)
    rise = max(top - sill, 0.004)
    shoulder = (flat + min(0.05, 0.6 * (sy - flat)), top - min(0.06, 0.35 * rise))
    return [(max(fy - 0.03, 0.0), fz - 0.06), (fy, fz), (sy, sill), (0.5 * (sy + shoulder[0]), 0.5 * (sill + shoulder[1])), shoulder,
            (flat + min(0.02, 0.25 * (sy - flat)), top - min(0.018, 0.1 * rise)), (flat, top - min(0.004, 0.02 * rise)), (0.5 * flat, top), (0.0, top)]


def _me262_canopy(hull):
    skirt, post = 2, 6
    x0, sill, top, flat = _ME262_HOOD[0]
    head = _me262_section(hull, x0, sill, top, flat)
    fx, fhalf = _ME262_SCREEN
    fz = hull.top_at(fx, fhalf) - 0.01
    # (the windscreen: a flat pane from its foot on her deck to its head between the posts, a quarter pane each side
    #  from the foot's corner back and out to the sill)
    foot = [(fhalf, fz)] * (post + 1) + [(0.5 * fhalf, fz), (0.0, fz)]
    screen = {"sections": [(fx, foot), (x0, head)], "skirt": skirt, "hoops": [-1], "rails": [post, skirt], "bar": 0.024}
    hood = {"sections": [(x, _me262_section(hull, x, sl, t, fl)) for x, sl, t, fl in _ME262_HOOD], "skirt": skirt, "hoops": [0, -1],
            "rails": [skirt], "bar": 0.024}
    rear = {"sections": [(x, _me262_section(hull, x, sl, t, fl, out, ft)) for x, out, sl, t, fl, ft in _ME262_REAR], "skirt": skirt, "hoops": [0],
            "rails": [skirt], "bar": 0.02, "cap": True, "bare": (4, 5)}
    return [screen, hood, rear]


# Her splinter: the two upper colours in turn along each wing, along her spine and out along each tailplane, parted
# by straight lines. Each zone is (whether it begins in the paler colour, its lines); a line runs from a point at
# the leading edge to one at the trailing edge (x, y: y to starboard), or across her spine from starboard to port.
# Lines next to each other lean opposite ways, so the bands are wedges, and no two zones are alike.
_ME262_ZONES = {
    "starboard": (False, [((1.23, 1.5), (-0.92, 2.9)), ((0.25, 4.3), (-0.96, 3.35)), ((0.14, 4.6), (-1.21, 5.7))]),
    "port": (True, [((0.9, -2.4), (-1.1, -1.25)), ((0.74, -2.9), (-1.08, -4.4)), ((-0.17, -5.5), (-1.12, -4.85))]),
    "spine": (False, [((3.7, 1.0), (2.9, -1.0)), ((1.3, 1.0), (2.0, -1.0)), ((-1.3, 1.0), (-2.1, -1.0)), ((-3.3, 1.0), (-2.6, -1.0))]),
    "tail starboard": (False, [((-4.87, 1.25), (-5.79, 0.55))]),
    "tail port": (True, [((-4.6, -0.7), (-5.77, -1.45))]),
}


_ME262_SOFT = 0.06                                          # (half the breadth of a sprayed edge, in metres)
_ME262_GRAIN = {"wing": 0.12, "tail": 0.11}                 # (how far apart her wing's and tailplane's rings stand: half the usual)


def _me262_edge(d):
    """A distance in metres from a paint line as the coat's field takes it. The field is soft for 0.045 each side of
    nought, and the coat is laid point by point: an edge narrower than her mesh's grain comes out in steps, so a line
    is as broad as _ME262_SOFT and her wings' mesh as fine (`grain`), and the line runs straight."""
    return max(-2.0, min(2.0, d / _ME262_SOFT)) * 0.045


def _me262_zone(zone, p):
    """How far a point is into a zone's paler colour, in metres from the nearest of its lines (below nought in the darker)."""
    pale, lines = _ME262_ZONES[zone]
    hand = -1.0 if zone.endswith("port") else 1.0
    near = 9.0
    for (ax, ay), (bx, by) in lines:
        d = ((bx - ax) * (p.y - ay) - (by - ay) * (p.x - ax)) / math.hypot(bx - ax, by - ay) * hand
        if d < 0.0:
            pale = not pale                                 # (beyond this line: the other colour)
        near = min(near, abs(d))
    return near if pale else -near


def _me262_coat(p):
    """Her coat: above nought in her paler upper colour, below in her darker. Splinter wedges over her wings, her
    tailplane and along her spine; her fuselage's sloping sides, her fin and the flanks of her pods in the paler
    colour, with broad soft clouds of the darker sprayed over them, thicker toward her spine."""
    hull = _me262_hull()
    x = min(max(p.x, hull.back), hull.front)
    zc, h = hull.z(-x), max(hull.h(-x), 1.0e-6)
    cloud = noise.noise(Vector((p.x * 1.1 + 7.7, p.z * 1.7 + (11.0 if p.y > 0.0 else 37.0), 0.0)))
    if abs(p.z - zc) <= h + 0.04 and abs(p.y) <= hull.side_at(x, min(max(p.z, zc - 0.98 * h), zc + 0.98 * h)) + 0.035:
        t = (p.z - zc) / h
        top = 0.5 + 0.12 * noise.noise(Vector((p.x * 0.8, 3.1, 0.0)))
        spine = smooth(min(max((t - top) / 0.3 + 0.5, 0.0), 1.0))      # (nought on her side, one on her spine)
        return _me262_edge(_me262_zone("spine", p)) * spine + _me262_edge((cloud + 0.12 - 0.3 * t) * 0.36) * (1.0 - spine)
    if abs(p.y) < 0.13 and p.x < -3.6 and p.z > zc + h:
        return _me262_edge((cloud + 0.04) * 0.36)           # (her fin and rudder)
    zone = ("tail " if p.x < -4.15 else "") + ("starboard" if p.y > 0.0 else "port")
    wedge = _me262_edge(_me262_zone(zone, p))
    if abs(abs(p.y) - 2.26) < 0.5 and p.z < -0.34:
        top = smooth(min(max(((p.z + 0.82) / 0.455 - 0.6) / 0.5 + 0.5, 0.0), 1.0))     # (one along a pod's back, under her wing's wedges)
        return wedge * top + _me262_edge((cloud + 0.16) * 0.36) * (1.0 - top)
    return wedge


EXTRA_PLANES = {
    "Me262": {
        "title": "Me 262A (our own model)", "year": 1944, "tech": "jets",
        "hull": _ME262_HULL, "square": 3.0, "taper": _ME262_TAPER,
        "hoods": fighters.hood_of([(_ME262_SCREEN[0], 0.6, _ME262_SCREEN[1], 0.01)] + [(x, sl, 0.3, t - sl) for x, sl, t, fl in _ME262_HOOD]
                                  + [(-1.0, 0.685, 0.05, 0.04), (-1.5, 0.53, 0.02, 0.01)]),
        "canopy": _me262_canopy, "opening": (0.45, -0.75, _ME262_EDGE),
        "wing": [(0.0, 1.75, -1.32, -0.5, 0.4), (0.85, 1.45, -1.22, -0.468, 0.366), (1.7, 1.157, -1.0, -0.375, 0.26), (2.7, 0.808, -0.9, -0.285, 0.2),
                 (4.0, 0.354, -1.04, -0.165, 0.15), (5.0, 0.005, -1.13, -0.0875, 0.1), (5.96, -0.33, -1.245, -0.03, 0.07), (6.12, -0.47, -1.2, -0.02, 0.05),
                 (6.2, -0.65, -1.05, -0.015, 0.02)],
        "tail": [(0.0, -4.27, -5.8, 0.63, 0.12), (1.0, -4.74, -5.78, 0.63, 0.1), (1.71, -5.075, -5.76, 0.63, 0.07), (1.82, -5.22, -5.7, 0.63, 0.04),
                 (1.86, -5.42, -5.58, 0.63, 0.01)],
        "fin": [(0, -3.45, -6.14, 0.05, 0.14), (0, -3.77, -6.11, 0.3, 0.14), (0, -4.456, -6.01, 1.0, 0.12), (0, -4.946, -5.95, 1.5, 0.09),
                (0, -5.22, -5.89, 1.78, 0.07), (0, -5.36, -5.8, 1.87, 0.05), (0, -5.5, -5.66, 1.91, 0.02)],
        "patch": 1.0, "graded": _me262_coat, "grain": _ME262_GRAIN, "frames_paint": "upper", "lines": (0.6, 0.45),
        "colours": {"upper": "4C5850", "upper2": "8B999E", "under": "A9B8C2", "nose": "4C5850", "boss": "2B2B2B"},
        "well": (1.05, 0.45, 0.36), "pitot": True, "hull_lines": [3.95, 2.95, 2.06, 1.2, -1.78, -2.75, -3.67, -4.9],
        "jets": [([(2.33, -0.862, 0.258, 0.258), (2.2, -0.85, 0.335, 0.335), (1.9, -0.835, 0.415, 0.415), (1.5, -0.82, 0.45, 0.45), (0.0, -0.815, 0.455, 0.455),
                   (-0.6, -0.82, 0.45, 0.45), (-1.1, -0.845, 0.37, 0.37), (-1.5, -0.884, 0.256, 0.256)], 2.26)],
        "side_marks": [(-2.6, 0.3, "GER")], "wing_marks": [(4.5, -0.45, 0.42, "GER", "GER", True, True)], "code": (-1.85, 0.28),
    },
}
fitout.SEATS["Me262"] = [(0, -0.05, 0.0, 0.0)]
# ---- the carrier planes (Brian, 2026-11-18). Paints and first years are the art director's rulings of that day. ----
# (The art director, 2026-11-18, of the first one: she, the Avenger and the generic carrier planes "read as one plane in
#  different paint". Hers are: a deep, portly fuselage, a short sliding hood with metal behind it, the intake under
#  her cowl, and a broad low wing cut off square.)
_HELLCAT_HULL = [(4.3, -0.02, 0.04, 0.04), (4.05, -0.02, 0.16, 0.16), (4.0, -0.08, 0.64, 0.72), (3.75, -0.1, 0.73, 0.83), (2.9, -0.1, 0.75, 0.9), (1.5, -0.03, 0.73, 0.99),
                 (-0.2, 0.06, 0.65, 0.97), (-2.2, 0.21, 0.44, 0.7), (-4.2, 0.35, 0.2, 0.4), (-5.5, 0.43, 0.07, 0.18), (-5.94, 0.45, 0.02, 0.08)]
EXTRA_PLANES["Hellcat"] = {
    "title": "F6F Hellcat (our own model)", "year": 1943,
    "hull": _HELLCAT_HULL, "square": 2.2, "screw": 2.0, "blades": 3, "nose_back": 2.75, "spinner": 2, "engine": "radial", "lip": 4.0,
    "hoods": [([(1.25, 0.97, 0.24, 0.06), (0.75, 1.22, 0.31, 0.24), (0.1, 1.26, 0.32, 0.25), (-0.45, 1.2, 0.28, 0.16), (-0.78, 1.1, 0.2, 0.04)], (0.75, 0.1, -0.45), False)],
    "wing": [(0, 1.72, -1.55, -0.62, 0.5), (0.75, 1.72, -1.55, -0.62, 0.49), (1.9, 1.64, -1.5, -0.6, 0.45), (5.9, 1.0, -0.98, -0.02, 0.22), (6.35, 0.93, -0.92, 0.04, 0.17),
             (6.5, 0.86, -0.86, 0.06, 0.08), (6.53, 0.7, -0.74, 0.07, 0.01)],
    "tail": [(0, -4.2, -5.5, 0.6, 0.13), (2.2, -4.66, -5.45, 0.6, 0.08), (2.7, -4.78, -5.4, 0.6, 0.05), (2.8, -4.95, -5.32, 0.6, 0.01)],
    "fin": [(0, -3.9, -5.9, 0.55, 0.14), (0, -4.5, -5.98, 1.35, 0.11), (0, -4.85, -5.98, 2.05, 0.08), (0, -5.0, -5.93, 2.4, 0.05), (0, -5.12, -5.82, 2.48, 0.01)],
    # (the intake under her cowl)
    "scoops": [([(4.02, -0.9, 0.36, 0.15), (3.92, -0.92, 0.4, 0.17), (3.4, -0.94, 0.36, 0.14), (2.8, -0.96, 0.22, 0.06)], 0.0, 2.4)],
    "patch": 1.0, "graded": navy_blues(_HELLCAT_HULL, 2.2, -3.9),
    "colours": {"upper": "2A3850", "upper2": "55708C", "under": "D9DAD3", "nose": "2A3850", "boss": "2A2B2D"},
    "well": (1.7, 0.42, 0.42), "guns": [(2.5, 0.05, 0.3, 0.022), (2.72, 0.05, 0.22, 0.022), (2.94, 0.05, 0.14, 0.022)], "pitot": True,
    "tailwheel": (-5.35, -0.1), "mast": (-1.3, 1.02, 1.6),
    # (the long-range tank she nearly always flew with, under her middle)
    "extras": [("body", ([(1.75, -1.27, 0.03, 0.03), (1.35, -1.3, 0.25, 0.23), (0.3, -1.32, 0.3, 0.27), (-0.8, -1.3, 0.23, 0.21), (-1.5, -1.26, 0.03, 0.03)], 0.0, 2.0, "under")),
               ("rod", ((0.9, 0.0, -0.95), (0.9, 0.0, -1.12), 0.05, 0.05, "under")), ("rod", ((-0.3, 0.0, -0.9), (-0.3, 0.0, -1.12), 0.05, 0.05, "under"))],
    "side_marks": [(-2.75, 0.34, "USA-BAR")], "wing_marks": [(4.5, 0.05, 0.5, "USA-BAR", "USA-BAR", False, False)], "code": (-3.9, 0.3),
}
JETS = [
    ('    boss_back = 1.0e9 if s.get("engines") else sorted(', '    boss_back = 1.0e9 if s.get("engines") or s.get("jets") else sorted('),
    ('    if not s.get("engines"):\n        airscrew(parts, (hub_x, 0.0, hub_z)', '    if not s.get("engines") and not s.get("jets"):\n        airscrew(parts, (hub_x, 0.0, hub_z)'),
    ('    # The tailwheel, the aerial mast with its wire, and anything fixed under the machine.\n',
     '    for jet_stations, jet_out in s.get("jets", []):\n'
     '        for jet_side in (jet_out, -jet_out):\n'
     '            pod = Body(jet_stations, 2.0, jet_side)\n'
     '            n_pod = 32 if fine else 10\n'
     '            parts.skin("nacelle", [[pod.at(x, 2.0 * math.pi * k / n_pod) for k in range(n_pod)] for x in pod.xs(0.2 if fine else 0.8)],\n'
     '                       with_blend(lambda p, n, front=pod.front, back=pod.back: "engine" if (p.x > front - 0.02 and n.x > 0.5) or (p.x < back + 0.02 and n.x < -0.5) else coat(p, n), coat))\n'
     '    # The tailwheel, the aerial mast with its wire, and anything fixed under the machine.\n'),
]

# A torpedo slung under her: 5.3 m long, with the box of wooden fins on its tail that steadied it in the air.
def _torpedo(x, z, long=5.3, r=0.225):
    a, b = x + 0.5 * long, x - 0.5 * long
    return [("body", ([(a, z, 0.03, 0.03), (a - 0.22, z, 0.17, 0.17), (a - 0.55, z, r, r), (b + 1.0, z, r, r), (b + 0.35, z, 0.1, 0.1), (b, z, 0.03, 0.03)], 0.0, 2.0, "torpedo")),
            ("rod", ((b + 0.02, 0.0, z), (b + 0.42, 0.0, z), 0.24, 0.24, "torpedo"))]


_PALE = {"upper": "A9B19F", "upper2": "A9B19F", "under": "9DA593", "nose": "1E1F22", "boss": "8E9092"}      # (the Japanese navy's pale grey of the early war)
EXTRA_PLANES["Kate"] = {
    "title": "B5N Kate (our own model)", "year": 1937,
    "hull": [(4.1, 0, 0.04, 0.04), (3.85, 0, 0.16, 0.16), (3.8, 0, 0.5, 0.5), (3.55, 0, 0.61, 0.62), (2.8, 0, 0.62, 0.66), (1.5, 0.02, 0.58, 0.72), (-0.5, 0.06, 0.54, 0.72),
             (-2.6, 0.14, 0.4, 0.56), (-4.6, 0.26, 0.2, 0.34), (-5.8, 0.34, 0.07, 0.16), (-6.2, 0.36, 0.02, 0.07)],
    "square": 2.1, "screw": 1.6, "blades": 3, "nose_back": 2.75, "spinner": 2, "engine": "radial", "lip": 3.8,
    "hoods": [([(1.55, 0.74, 0.24, 0.05), (1.0, 0.98, 0.31, 0.24), (-0.4, 1.02, 0.32, 0.26), (-1.8, 0.98, 0.3, 0.22), (-2.7, 0.8, 0.18, 0.04)],
               (1.0, 0.45, -0.1, -0.65, -1.2, -1.8, -2.3), True)],
    "wing": [(0, 1.6, -1.4, -0.5, 0.46), (0.6, 1.6, -1.4, -0.5, 0.45), (1.6, 1.52, -1.38, -0.48, 0.42), (5.6, 0.95, -0.78, -0.05, 0.22), (7.0, 0.6, -0.5, 0.1, 0.12),
             (7.6, 0.3, -0.28, 0.16, 0.06), (7.76, 0.02, -0.1, 0.18, 0.01)],
    "tail": [(0, -4.6, -5.7, 0.42, 0.11), (1.9, -5.0, -5.65, 0.42, 0.07), (2.4, -5.3, -5.55, 0.42, 0.02)],
    "fin": [(0, -4.4, -6.15, 0.36, 0.12), (0, -4.95, -6.2, 1.1, 0.09), (0, -5.4, -6.15, 1.7, 0.05), (0, -5.7, -6.05, 1.85, 0.01)],
    "patch": 0.0, "colours": dict(_PALE), "well": (1.75, 0.4, 0.45), "pitot": True, "tailwheel": (-5.7, -0.15), "mast": (0.2, 1.24, 1.75),
    # (her torpedo; the one heavy armour-piercing bomb she carried instead for bombing from high up; her rear gunner's gun)
    "extras": _torpedo(-0.2, -0.99) + [
        ("body", ([(1.3, -0.96, 0.03, 0.03), (1.0, -0.96, 0.17, 0.17), (0.5, -0.96, 0.205, 0.205), (-0.4, -0.96, 0.19, 0.19), (-1.0, -0.96, 0.07, 0.07), (-1.15, -0.96, 0.03, 0.03)], 0.0, 2.0, "bomb")),
        ("rod", ((-0.85, 0.0, -0.96), (-1.15, 0.0, -0.96), 0.2, 0.2, "bomb")),
        ("rod", ((-2.0, 0.0, 0.96), (-2.85, 0.0, 1.18), 0.022, 0.018, "steel"))],
    "side_marks": [(-3.2, 0.32, "JAP-PLAIN")], "wing_marks": [(5.2, 0.1, 0.62, "JAP-PLAIN", "JAP-PLAIN", True, True)], "code": (-4.3, 0.34),
}
EXTRA_PLANES["Val"] = {
    "title": "D3A Val (our own model)", "year": 1940,
    "hull": [(4.0, 0, 0.04, 0.04), (3.78, 0, 0.15, 0.15), (3.74, 0, 0.5, 0.5), (3.5, 0, 0.62, 0.62), (2.75, 0, 0.63, 0.66), (1.4, 0.02, 0.56, 0.7), (-0.6, 0.06, 0.5, 0.68),
             (-2.7, 0.14, 0.36, 0.52), (-4.7, 0.26, 0.17, 0.3), (-5.85, 0.33, 0.06, 0.14), (-6.2, 0.35, 0.02, 0.07)],
    "square": 2.1, "screw": 1.5, "blades": 3, "nose_back": 2.7, "spinner": 2, "engine": "radial", "lip": 3.74,
    "hoods": [([(1.45, 0.72, 0.24, 0.05), (0.9, 0.96, 0.3, 0.24), (-0.5, 1.0, 0.31, 0.25), (-1.6, 0.95, 0.28, 0.2), (-2.4, 0.76, 0.17, 0.04)], (0.9, 0.4, -0.1, -0.6, -1.1, -1.6), True)],
    # (her wing is an ellipse, like no other dive bomber's)
    "wing": [(0, 1.4, -1.7, -0.5, 0.48), (0.6, 1.4, -1.7, -0.5, 0.47), (2.0, 1.35, -1.63, -0.43, 0.43), (4.0, 1.18, -1.39, -0.3, 0.34), (5.6, 0.91, -1.03, -0.18, 0.24),
             (6.6, 0.61, -0.61, -0.1, 0.14), (7.05, 0.35, -0.24, -0.07, 0.07), (7.18, 0.14, 0.04, -0.06, 0.01)],
    "tail": [(0, -4.6, -5.65, 0.4, 0.1), (1.9, -4.95, -5.6, 0.4, 0.06), (2.35, -5.25, -5.5, 0.4, 0.02)],
    # (the long fillet ahead of her fin)
    "fin": [(0, -3.4, -6.1, 0.36, 0.1), (0, -4.75, -6.18, 1.0, 0.09), (0, -5.25, -6.15, 1.65, 0.05), (0, -5.6, -6.05, 1.85, 0.01)],
    "patch": 0.0, "colours": dict(_PALE), "pitot": True, "tailwheel": (-5.75, -0.15), "mast": (0.2, 1.2, 1.7),
    # (her fixed undercarriage in its spats; the dive brakes under her wings; her bomb on its crutch; her rear gunner's gun)
    "extras": [("rod", ((0.72, 1.55, -0.55), (0.78, 1.55, -1.42), 0.17, 0.14, "under")),
               ("ball", ((0.7, 1.55, -1.58), (0.62, 0.17, 0.29), "under")),
               ("rod", ((0.74, 1.47, -1.74), (0.74, 1.63, -1.74), 0.27, 0.27, "tyre")),
               ("rod", ((0.45, 2.3, -0.64), (0.45, 3.6, -0.5), 0.035, 0.035, "under")),
               ("body", ([(1.3, -0.93, 0.03, 0.03), (1.0, -0.93, 0.16, 0.16), (0.2, -0.93, 0.18, 0.18), (-0.5, -0.92, 0.07, 0.07), (-0.7, -0.92, 0.03, 0.03)], 0.0, 2.0, "bomb")),
               ("rod", ((-0.45, 0.0, -0.92), (-0.7, 0.0, -0.92), 0.17, 0.17, "bomb")),
               ("rod", ((-1.8, 0.0, 0.94), (-2.6, 0.0, 1.14), 0.022, 0.018, "steel"))],
    "side_marks": [(-3.2, 0.3, "JAP-PLAIN")], "wing_marks": [(5.0, 0.1, 0.6, "JAP-PLAIN", "JAP-PLAIN", True, True)], "code": (-4.3, 0.32),
}


def one_coat(p):
    """A coat with no second colour, on a machine whose cowling is painted with the rest of her."""
    return -1.0


# The US Navy's blue-grey over light grey of 1941 and 1942, the star without bars.
_BLUE_GREY = {"upper": "566A78", "upper2": "566A78", "under": "A8AEAD", "nose": "566A78", "boss": "2A2B2D"}
EXTRA_PLANES["Dauntless"] = {
    "title": "SBD Dauntless (our own model)", "year": 1941,
    "hull": [(3.95, 0, 0.04, 0.04), (3.72, 0, 0.15, 0.15), (3.68, 0, 0.5, 0.5), (3.45, 0, 0.64, 0.64), (2.75, 0, 0.66, 0.68), (1.4, 0.02, 0.6, 0.74), (-0.6, 0.06, 0.54, 0.72),
             (-2.6, 0.14, 0.38, 0.54), (-4.6, 0.26, 0.18, 0.32), (-5.8, 0.34, 0.06, 0.15), (-6.14, 0.36, 0.02, 0.07)],
    "square": 2.1, "screw": 1.65, "blades": 3, "nose_back": 2.7, "spinner": 2, "engine": "radial", "lip": 3.68,
    "hoods": [([(1.4, 0.76, 0.25, 0.05), (0.85, 1.02, 0.31, 0.25), (-0.5, 1.06, 0.32, 0.26), (-1.7, 1.0, 0.29, 0.21), (-2.5, 0.8, 0.18, 0.04)], (0.85, 0.35, -0.15, -0.65, -1.15, -1.7), True)],
    # (a straight middle to her wing, then tapered panels turned up)
    "wing": [(0, 1.5, -1.35, -0.52, 0.46), (1.6, 1.5, -1.35, -0.52, 0.44), (5.4, 0.85, -0.7, -0.08, 0.2), (6.0, 0.62, -0.52, -0.01, 0.12), (6.27, 0.35, -0.32, 0.02, 0.06), (6.33, 0.08, -0.12, 0.03, 0.01)],
    "tail": [(0, -4.5, -5.6, 0.42, 0.11), (1.9, -4.9, -5.55, 0.42, 0.06), (2.4, -5.2, -5.45, 0.42, 0.02)],
    "fin": [(0, -3.7, -6.05, 0.36, 0.1), (0, -4.8, -6.12, 1.0, 0.09), (0, -5.25, -6.1, 1.6, 0.05), (0, -5.55, -6.0, 1.75, 0.01)],
    "patch": 1.0, "graded": one_coat, "colours": dict(_BLUE_GREY), "well": (1.15, 0.42, 0.42), "well_in": True, "pitot": True, "tailwheel": (-5.7, -0.15), "mast": (0.3, 1.26, 1.75),
    # (her bomb on its crutch; her rear gunner's pair of guns)
    "extras": [("body", ([(1.45, -0.95, 0.03, 0.03), (1.1, -0.95, 0.19, 0.19), (0.2, -0.95, 0.21, 0.21), (-0.6, -0.94, 0.08, 0.08), (-0.85, -0.94, 0.03, 0.03)], 0.0, 2.0, "bomb")),
               ("rod", ((-0.55, 0.0, -0.94), (-0.85, 0.0, -0.94), 0.2, 0.2, "bomb")),
               ("rod", ((-1.9, 0.07, 0.98), (-2.7, 0.07, 1.2), 0.02, 0.016, "steel"))],
    "side_marks": [(-3.1, 0.34, "USA")], "wing_marks": [(4.4, 0.0, 0.55, "USA", "USA", False, False)], "code": (-4.2, 0.3),
}
EXTRA_PLANES["Avenger"] = {
    "title": "TBF Avenger (our own model)", "year": 1942,
    # (a deep barrel: her belly, with the bay in it, hangs well below her wing and steps up behind the bay to the gun under her tail)
    "hull": [(5.0, 0, 0.04, 0.04), (4.75, 0, 0.17, 0.17), (4.7, 0, 0.58, 0.58), (4.45, 0, 0.72, 0.72), (3.6, -0.06, 0.76, 0.84), (2.2, -0.26, 0.78, 1.16), (0.0, -0.3, 0.76, 1.24),
             (-1.9, -0.26, 0.68, 1.18), (-2.6, 0.02, 0.56, 0.84), (-3.8, 0.16, 0.42, 0.62), (-5.6, 0.36, 0.2, 0.34), (-7.0, 0.48, 0.07, 0.16), (-7.48, 0.5, 0.02, 0.07)],
    "square": 2.3, "screw": 2.0, "blades": 3, "nose_back": 3.55, "spinner": 2, "engine": "radial", "lip": 4.7,
    "hoods": [([(2.3, 0.9, 0.26, 0.05), (1.7, 1.16, 0.33, 0.26), (0.0, 1.22, 0.34, 0.28), (-1.8, 1.18, 0.32, 0.24), (-2.6, 1.0, 0.2, 0.05)], (1.7, 1.1, 0.5, -0.1, -0.7, -1.3, -1.8), True)],
    "wing": [(0, 1.9, -1.5, -0.38, 0.52), (1.9, 1.9, -1.5, -0.38, 0.5), (7.2, 1.0, -0.75, 0.14, 0.22), (7.9, 0.82, -0.62, 0.21, 0.14), (8.2, 0.6, -0.45, 0.24, 0.06), (8.25, 0.35, -0.25, 0.24, 0.01)],
    "tail": [(0, -5.6, -6.9, 0.66, 0.13), (2.6, -6.05, -6.85, 0.66, 0.07), (3.1, -6.35, -6.75, 0.66, 0.02)],
    "fin": [(0, -5.0, -7.4, 0.5, 0.14), (0, -5.5, -7.5, 1.3, 0.12), (0, -5.85, -7.52, 2.2, 0.09), (0, -6.0, -7.5, 2.75, 0.06), (0, -6.1, -7.45, 2.9, 0.01)],
    "patch": 1.0, "graded": one_coat, "colours": dict(_BLUE_GREY), "well": (2.1, 0.42, 0.5), "pitot": True, "tailwheel": (-6.6, -0.1),
    # (the round turret at the end of her long hood, and the gun under her tail)
    "extras": [("dome", ((-3.05, 0.0, 0.98), (0.44, 0.44, 0.44))),
               ("rod", ((-3.2, 0.0, 1.08), (-4.0, 0.0, 1.3), 0.024, 0.02, "steel")),
               ("rod", ((-2.3, 0.0, -1.02), (-3.1, 0.0, -1.2), 0.02, 0.016, "steel"))],
    "side_marks": [(-4.3, 0.4, "USA")], "wing_marks": [(5.6, 0.1, 0.7, "USA", "USA", False, False)], "code": (-5.4, 0.3),
}
# ---- carrier planes for the nations with none of their own: our own shapes, painted by the game for whoever flies them ----
_NAVY = {"upper": "5A5F68", "upper2": "5B6259", "under": "B9C7A9", "nose": "5A5F68", "boss": "2A2B2D"}     # (shown in the Fleet Air Arm's greys over Sky)
EXTRA_PLANES["CarrierFighter"] = {
    "title": "Carrier fighter (our own model)",
    "hull": [(3.7, 0, 0.04, 0.04), (3.48, 0, 0.16, 0.16), (3.44, 0, 0.5, 0.5), (3.2, 0, 0.62, 0.63), (2.45, 0, 0.63, 0.66), (1.1, 0.0, 0.56, 0.72), (-0.6, 0.05, 0.48, 0.68),
             (-2.6, 0.14, 0.28, 0.44), (-4.5, 0.26, 0.08, 0.18), (-5.0, 0.3, 0.02, 0.08)],
    "square": 2.1, "screw": 1.6, "blades": 3, "nose_back": 2.4, "spinner": 2, "engine": "radial", "lip": 3.44, "nation_paint": True,
    "hoods": [([(1.05, 0.68, 0.23, 0.06), (0.55, 0.92, 0.3, 0.23), (-0.4, 0.95, 0.31, 0.24), (-1.2, 0.86, 0.26, 0.16), (-1.7, 0.7, 0.18, 0.04)], (0.55, 0.05, -0.4, -1.2), False)],
    # (a broad wing for a slow landing on a deck)
    "wing": [(0, 1.5, -1.25, -0.48, 0.42), (0.6, 1.5, -1.25, -0.48, 0.41), (1.7, 1.42, -1.2, -0.44, 0.38), (4.9, 0.85, -0.68, -0.06, 0.18), (5.45, 0.6, -0.5, 0.0, 0.1), (5.7, 0.2, -0.25, 0.03, 0.01)],
    "tail": [(0, -3.8, -4.75, 0.42, 0.1), (1.7, -4.15, -4.7, 0.42, 0.06), (2.1, -4.4, -4.62, 0.42, 0.01)],
    "fin": [(0, -3.4, -5.0, 0.36, 0.12), (0, -3.95, -5.08, 1.05, 0.09), (0, -4.4, -5.04, 1.6, 0.04), (0, -4.65, -4.95, 1.72, 0.01)],
    "patch": 2.2, "colours": dict(_NAVY), "well": (1.6, 0.4, 0.42), "pitot": True, "guns": [(2.2, 0.05, 0.3, 0.022), (2.42, 0.05, 0.2, 0.022)],
    "tailwheel": (-4.5, -0.12), "mast": (-1.9, 0.72, 1.3),
    # (the hook under her tail that catches the wire on the deck)
    "extras": [("rod", ((-3.9, 0.0, -0.08), (-4.75, 0.0, -0.3), 0.022, 0.016, "leg")), ("rod", ((-4.72, 0.0, -0.3), (-4.84, 0.0, -0.4), 0.02, 0.012, "leg"))],
    "side_marks": [(-2.3, 0.3, "*")], "wing_marks": [(3.8, 0.1, 0.5, "*", "*", True, True)], "code": (-1.5, 0.3),
}
EXTRA_PLANES["DiveBomber"] = {
    "title": "Carrier dive bomber (our own model)",
    "hull": [(3.9, 0, 0.04, 0.04), (3.68, 0, 0.15, 0.15), (3.64, 0, 0.5, 0.5), (3.4, 0, 0.62, 0.63), (2.7, 0, 0.64, 0.67), (1.4, 0.02, 0.58, 0.72), (-0.6, 0.06, 0.52, 0.7),
             (-2.6, 0.14, 0.36, 0.52), (-4.5, 0.26, 0.17, 0.3), (-5.6, 0.33, 0.06, 0.14), (-5.95, 0.35, 0.02, 0.07)],
    "square": 2.1, "screw": 1.6, "blades": 3, "nose_back": 2.65, "spinner": 2, "engine": "radial", "lip": 3.64, "nation_paint": True,
    "hoods": [([(1.4, 0.74, 0.24, 0.05), (0.85, 0.99, 0.3, 0.24), (-0.4, 1.03, 0.31, 0.25), (-1.5, 0.97, 0.28, 0.2), (-2.3, 0.78, 0.17, 0.04)], (0.85, 0.35, -0.15, -0.65, -1.15, -1.5), True)],
    "wing": [(0, 1.5, -1.3, -0.5, 0.45), (0.6, 1.5, -1.3, -0.5, 0.44), (1.8, 1.44, -1.26, -0.47, 0.4), (5.5, 0.85, -0.7, -0.08, 0.2), (6.2, 0.6, -0.5, 0.0, 0.12), (6.55, 0.25, -0.25, 0.04, 0.01)],
    "tail": [(0, -4.4, -5.45, 0.4, 0.1), (1.9, -4.75, -5.4, 0.4, 0.06), (2.3, -5.0, -5.3, 0.4, 0.02)],
    "fin": [(0, -3.9, -5.85, 0.36, 0.11), (0, -4.6, -5.93, 1.0, 0.09), (0, -5.05, -5.9, 1.62, 0.05), (0, -5.35, -5.8, 1.78, 0.01)],
    "patch": 2.4, "colours": dict(_NAVY), "well": (1.6, 0.42, 0.42), "pitot": True, "tailwheel": (-5.5, -0.15), "mast": (0.3, 1.24, 1.72),
    # (her bomb on its crutch; the brakes under her wings that hold her speed in the dive; her hook; her rear gunner's gun)
    "extras": [("body", ([(1.4, -0.94, 0.03, 0.03), (1.05, -0.94, 0.18, 0.18), (0.2, -0.94, 0.2, 0.2), (-0.55, -0.93, 0.08, 0.08), (-0.8, -0.93, 0.03, 0.03)], 0.0, 2.0, "bomb")),
               ("rod", ((-0.5, 0.0, -0.93), (-0.8, 0.0, -0.93), 0.19, 0.19, "bomb")),
               ("rod", ((0.5, 2.2, -0.66), (0.5, 3.6, -0.5), 0.035, 0.035, "under")),
               ("rod", ((-4.4, 0.0, -0.05), (-5.3, 0.0, -0.28), 0.022, 0.016, "leg")), ("rod", ((-5.27, 0.0, -0.28), (-5.39, 0.0, -0.38), 0.02, 0.012, "leg")),
               ("rod", ((-1.7, 0.0, 0.96), (-2.5, 0.0, 1.16), 0.022, 0.018, "steel"))],
    "side_marks": [(-3.0, 0.32, "*")], "wing_marks": [(4.6, 0.1, 0.58, "*", "*", True, True)], "code": (-4.1, 0.32),
}
EXTRA_PLANES["TorpedoBomber"] = {
    "title": "Carrier torpedo bomber (our own model)",
    "hull": [(4.4, 0, 0.04, 0.04), (4.17, 0, 0.16, 0.16), (4.13, 0, 0.52, 0.52), (3.9, 0, 0.65, 0.66), (3.1, -0.02, 0.67, 0.72), (1.8, -0.08, 0.66, 0.88), (0.0, -0.1, 0.64, 0.92),
             (-1.9, -0.05, 0.54, 0.82), (-3.4, 0.1, 0.38, 0.56), (-5.2, 0.3, 0.18, 0.3), (-6.3, 0.4, 0.06, 0.14), (-6.7, 0.43, 0.02, 0.07)],
    "square": 2.2, "screw": 1.8, "blades": 3, "nose_back": 3.05, "spinner": 2, "engine": "radial", "lip": 4.13, "nation_paint": True,
    "hoods": [([(1.9, 0.78, 0.25, 0.05), (1.3, 1.04, 0.31, 0.25), (-0.2, 1.08, 0.32, 0.26), (-1.9, 1.02, 0.29, 0.21), (-2.8, 0.82, 0.18, 0.04)], (1.3, 0.75, 0.2, -0.35, -0.9, -1.45, -1.9), True)],
    "wing": [(0, 1.75, -1.4, -0.34, 0.5), (1.7, 1.75, -1.4, -0.34, 0.48), (6.4, 0.95, -0.72, 0.1, 0.21), (7.1, 0.7, -0.52, 0.17, 0.12), (7.45, 0.3, -0.25, 0.2, 0.01)],
    "tail": [(0, -5.0, -6.2, 0.52, 0.12), (2.3, -5.4, -6.15, 0.52, 0.07), (2.8, -5.7, -6.05, 0.52, 0.02)],
    "fin": [(0, -4.6, -6.6, 0.44, 0.12), (0, -5.25, -6.68, 1.25, 0.1), (0, -5.75, -6.65, 1.95, 0.05), (0, -6.05, -6.55, 2.15, 0.01)],
    "patch": 2.6, "colours": dict(_NAVY), "well": (1.9, 0.42, 0.46), "pitot": True, "tailwheel": (-6.0, -0.12), "mast": (0.6, 1.3, 1.8),
    # (her hook; her rear gunner's gun)
    "extras": [("rod", ((-4.9, 0.0, 0.0), (-5.85, 0.0, -0.22), 0.022, 0.016, "leg")), ("rod", ((-5.82, 0.0, -0.22), (-5.94, 0.0, -0.32), 0.02, 0.012, "leg")),
               ("rod", ((-2.1, 0.0, 1.0), (-2.95, 0.0, 1.22), 0.022, 0.018, "steel"))],
    "side_marks": [(-3.7, 0.36, "*")], "wing_marks": [(5.2, 0.1, 0.62, "*", "*", True, True)], "code": (-4.7, 0.32),
}


# ---- the big bombers of 2026-11-18 (Brian: "the japanese also had a high level bomber we should add"; "the germans should
#      also in their tech tree have a 4 engine heavy bomber which they were actually thinking of making to attack the US").
#      The design director's b441: the very long range heavy is one technology any nation can research (`tech`); the German
#      model answers to the tag amerikabomber, America's to b29. ----
# The Japanese navy's land-based bombers of the early war: green and brown over grey (the art director).
_LAND = {"upper": "3F4A36", "upper2": "6B5A3F", "under": "A3A69C", "nose": "3F4A36", "boss": "2A2B2D"}
EXTRA_PLANES["Betty"] = {
    "title": "G4M Betty (our own model)", "year": 1941,
    # (a fat cigar of a fuselage, round all the way to the glass cone in her tail)
    "hull": [(8.6, -0.1, 0.14, 0.14), (8.35, -0.1, 0.46, 0.48), (7.8, -0.06, 0.76, 0.8), (7.0, -0.02, 0.92, 1.0), (5.6, 0.0, 1.0, 1.14), (3.0, 0.0, 1.03, 1.2), (0.0, 0.0, 1.03, 1.2),
             (-3.0, 0.02, 1.0, 1.14), (-6.0, 0.08, 0.86, 0.98), (-8.6, 0.16, 0.66, 0.76), (-10.3, 0.2, 0.5, 0.58), (-11.0, 0.2, 0.42, 0.48), (-11.4, 0.2, 0.27, 0.31)],
    "square": 2.0, "screw": 1.7, "blades": 3, "glazed": (7.7, 7.7), "nose_frames": ((8.2,), 8),
    "hoods": [([(6.5, 1.0, 0.42, 0.06), (5.9, 1.3, 0.6, 0.3), (4.4, 1.38, 0.62, 0.34), (3.2, 1.3, 0.5, 0.22), (2.5, 1.18, 0.32, 0.05)], (5.9, 5.4, 4.9, 4.4, 3.8, 3.2), True)],
    "wing": [(0.0, 2.5, -2.4, -0.15, 0.8), (3.3, 2.25, -2.3, -0.08, 0.7), (9.6, 1.2, -1.2, 0.2, 0.3), (11.6, 0.78, -0.72, 0.3, 0.16), (12.3, 0.42, -0.42, 0.33, 0.07), (12.45, 0.1, -0.2, 0.34, 0.01)],
    "tail": [(0.0, -8.3, -10.3, 0.5, 0.3), (3.7, -9.0, -10.2, 0.5, 0.16), (4.3, -9.5, -10.05, 0.5, 0.03)],
    "fin": [(0.0, -7.4, -10.7, 0.6, 0.2), (0.0, -8.5, -10.85, 1.8, 0.26), (0.0, -9.4, -10.9, 3.0, 0.18), (0.0, -10.0, -10.8, 3.7, 0.08), (0.0, -10.4, -10.6, 3.9, 0.01)],
    "patch": 3.4, "colours": dict(_LAND), "engines": [(3.3, 2.25, -0.2, True, 3.6)], "well": (3.3, 0.45, 0.5), "pitot": True,
    "tailwheel": (-9.6, -0.6), "mast": (1.6, 1.2, 1.75),
    # (the gun in her nose; the blister on her back and its gun; a blister each side at her waist and its gun; the cannon in her tail)
    "extras": [("rod", ((8.55, 0.0, -0.1), (9.2, 0.0, -0.13), 0.02, 0.016, "steel")),
               ("dome", ((-2.6, 0.0, 1.2), (0.55, 0.42, 0.36))), ("rod", ((-2.75, 0.0, 1.32), (-3.55, 0.0, 1.6), 0.02, 0.016, "steel")),
               ("dome", ((-4.9, 0.9, 0.3), (0.5, 0.27, 0.36))), ("dome", ((-4.9, -0.9, 0.3), (0.5, 0.27, 0.36))),
               ("rod", ((-5.0, 1.0, 0.32), (-5.5, 1.7, 0.26), 0.02, 0.016, "steel")),
               ("rod", ((-11.3, 0.0, 0.2), (-12.2, 0.0, 0.2), 0.03, 0.022, "steel"))],
    "side_marks": [(-6.9, 0.52, "JAP-PLAIN")], "wing_marks": [(9.0, 0.1, 0.95, "JAP-PLAIN", "JAP-PLAIN", True, True)], "code": (-8.3, 0.36),
}
EXTRA_PLANES["Nell"] = {
    "title": "G3M Nell (our own model)",
    # (slim, with a solid nose and two fins: the bomber Japan began the war with)
    "hull": [(7.2, -0.12, 0.1, 0.1), (6.95, -0.12, 0.34, 0.36), (6.3, -0.06, 0.56, 0.62), (5.2, 0.0, 0.68, 0.8), (3.5, 0.0, 0.72, 0.9), (0.0, 0.0, 0.72, 0.9), (-3.0, 0.05, 0.64, 0.8),
             (-6.0, 0.14, 0.42, 0.52), (-8.2, 0.22, 0.2, 0.28), (-9.25, 0.26, 0.05, 0.1)],
    "square": 2.2, "screw": 1.6, "blades": 3,
    "hoods": [([(5.5, 0.74, 0.34, 0.06), (4.9, 1.02, 0.46, 0.26), (3.8, 1.06, 0.46, 0.27), (3.0, 0.92, 0.34, 0.06)], (4.9, 4.35, 3.8), True)],
    "wing": [(0.0, 2.1, -2.3, -0.4, 0.62), (3.0, 1.9, -2.15, -0.32, 0.56), (9.5, 0.95, -1.05, 0.02, 0.24), (11.6, 0.6, -0.65, 0.13, 0.13), (12.3, 0.3, -0.35, 0.17, 0.06), (12.5, 0.05, -0.15, 0.18, 0.01)],
    "tail": [(0.0, -7.2, -8.8, 0.42, 0.22), (3.2, -7.5, -8.7, 0.42, 0.12), (3.4, -7.75, -8.6, 0.42, 0.03)],
    "fins": [([(0.0, -7.95, -8.45, -0.4, 0.02), (0.0, -7.55, -8.85, 0.0, 0.1), (0.0, -7.4, -9.05, 0.6, 0.13), (0.0, -7.55, -9.0, 1.2, 0.1), (0.0, -7.85, -8.8, 1.65, 0.05), (0.0, -8.15, -8.55, 1.82, 0.01)], 3.3)],
    "patch": 3.2, "colours": dict(_LAND), "engines": [(3.0, 1.9, -0.42, True, 3.2)], "well": (3.0, 0.45, 0.5), "pitot": True,
    "tailwheel": (-8.0, -0.3), "mast": (2.4, 0.9, 1.5),
    # (her torpedo, or the one heavy bomb she carried instead, slung under her: she has no bay; the small turret on her back
    #  and the big one behind it with its cannon; a blister each side at her waist and its gun)
    "extras": _torpedo(0.3, -1.15) + [
        ("body", ([(1.9, -1.12, 0.03, 0.03), (1.6, -1.12, 0.17, 0.17), (1.1, -1.12, 0.205, 0.205), (0.2, -1.12, 0.19, 0.19), (-0.4, -1.12, 0.07, 0.07), (-0.55, -1.12, 0.03, 0.03)], 0.0, 2.0, "bomb")),
        ("rod", ((-0.25, 0.0, -1.12), (-0.55, 0.0, -1.12), 0.2, 0.2, "bomb")),
        ("dome", ((1.2, 0.0, 0.95), (0.36, 0.36, 0.3))), ("rod", ((1.1, 0.0, 1.05), (0.4, 0.0, 1.3), 0.02, 0.016, "steel")),
        ("dome", ((-2.4, 0.0, 0.92), (0.52, 0.46, 0.42))), ("rod", ((-2.6, 0.0, 1.06), (-3.7, 0.0, 1.42), 0.03, 0.022, "steel")),
        ("dome", ((-4.3, 0.56, 0.3), (0.48, 0.26, 0.34))), ("dome", ((-4.3, -0.56, 0.3), (0.48, 0.26, 0.34))),
        ("rod", ((-4.4, 0.66, 0.32), (-4.9, 1.35, 0.26), 0.02, 0.016, "steel"))],
    "side_marks": [(-5.9, 0.34, "JAP-PLAIN")], "wing_marks": [(9.0, 0.0, 0.85, "JAP-PLAIN", "JAP-PLAIN", True, True)], "code": (-7.0, 0.24),
}
EXTRA_PLANES["Amerikabomber"] = {
    "title": "Amerika heavy bomber (our own model)", "tech": "verylongrange", "tag": "amerikabomber",
    # (a long round fuselage with a nose of glass and no step, a long narrow wing on her shoulders, two fins)
    "hull": [(9.5, -0.1, 0.16, 0.16), (9.25, -0.1, 0.52, 0.52), (8.7, -0.06, 0.84, 0.84), (7.9, -0.02, 1.02, 1.03), (6.8, 0.0, 1.09, 1.1), (4.0, 0.0, 1.1, 1.12), (0.0, 0.0, 1.1, 1.12),
             (-4.0, 0.03, 1.02, 1.05), (-7.0, 0.12, 0.8, 0.86), (-9.5, 0.24, 0.5, 0.58), (-10.8, 0.32, 0.26, 0.34), (-11.4, 0.35, 0.06, 0.12)],
    "square": 2.0, "screw": 1.75, "blades": 3, "glazed": (7.9, 6.9), "nose_frames": ((9.0, 8.45, 7.9, 7.4, 6.92), 12), "windows_back": True, "nose_bar": 0.4, "nose_frames_paint": "upper",
    "wing": [(0.0, 2.8, -2.4, 0.5, 0.92), (4.9, 2.35, -2.2, 0.56, 0.78), (9.5, 1.9, -1.75, 0.64, 0.58), (17.5, 1.0, -0.8, 0.8, 0.26), (20.5, 0.6, -0.4, 0.87, 0.13), (21.3, 0.3, -0.2, 0.89, 0.05),
             (21.5, 0.05, -0.05, 0.9, 0.01)],
    "tail": [(0.0, -8.9, -10.9, 0.7, 0.3), (5.3, -9.45, -10.8, 0.7, 0.16), (5.55, -9.75, -10.7, 0.7, 0.04)],
    "fins": [([(0.0, -10.3, -10.75, -0.55, 0.02), (0.0, -9.75, -11.15, -0.05, 0.13), (0.0, -9.5, -11.3, 0.8, 0.18), (0.0, -9.7, -11.25, 1.8, 0.14), (0.0, -10.1, -11.05, 2.5, 0.07),
               (0.0, -10.5, -10.8, 2.75, 0.01)], 5.45)],
    # (two greens over pale blue: the art director)
    "patch": 3.6, "colours": {"upper": "3A4234", "upper2": "525E48", "under": "8FB0BF", "nose": "3A4234", "boss": "2E332A"},
    "engines": [(4.9, 2.35, 0.42, True, 4.3), (9.5, 1.9, 0.52, True, 3.3)], "well": (4.9, 0.5, 0.6), "pitot": True, "mast": (2.0, 1.12, 1.7),
    # (a turret on her back behind the cockpit and another behind the wing; a low one under her, aimed from inside; a gun at each waist window)
    "extras": [("dome", ((4.4, 0.0, 1.16), (0.46, 0.46, 0.38))), ("rod", ((4.5, 0.0, 1.32), (5.4, 0.0, 1.6), 0.024, 0.018, "steel")),
               ("dome", ((-3.6, 0.0, 1.1), (0.5, 0.5, 0.4))), ("rod", ((-3.75, 0.0, 1.24), (-4.75, 0.0, 1.55), 0.03, 0.022, "steel")),
               ("ball", ((-3.3, 0.0, -1.06), (0.5, 0.42, 0.26), "under")), ("rod", ((-3.4, 0.1, -1.18), (-4.3, 0.1, -1.42), 0.022, 0.018, "steel")),
               ("rod", ((-5.4, 0.9, 0.3), (-6.0, 1.8, 0.2), 0.022, 0.018, "steel"))],
    "side_marks": [(-7.5, 0.48, "GER")], "wing_marks": [(15.0, 0.1, 0.9, "GER", "GER", True, True)], "code": (-3.6, 0.5),
}
EXTRA_PLANES["B29"] = {
    "title": "B-29 Superfortress (our own model)", "tech": "verylongrange", "tag": "b29", "bare": True,
    # (a round tube of a fuselage, glass at its nose with no step; her skin is left bare metal)
    "hull": [(11.0, -0.15, 0.2, 0.2), (10.78, -0.15, 0.62, 0.62), (10.25, -0.1, 1.0, 1.0), (9.45, -0.04, 1.27, 1.28), (8.3, 0.0, 1.42, 1.44), (6.5, 0.0, 1.45, 1.45), (0.0, 0.0, 1.45, 1.45),
             (-6.0, 0.0, 1.45, 1.45), (-10.0, 0.1, 1.22, 1.26), (-14.0, 0.3, 0.84, 0.92), (-17.0, 0.45, 0.58, 0.64), (-18.5, 0.5, 0.47, 0.52), (-19.2, 0.5, 0.32, 0.36)],
    "square": 2.0, "screw": 2.5, "blades": 4, "glazed": (9.4, 8.35), "nose_frames": ((10.8, 10.1, 9.4, 8.37), 8), "windows_back": True, "nose_cap": 10.8, "nose_bar": 0.45, "nose_frames_paint": "upper",
    "wing": [(0.0, 2.7, -2.5, -0.2, 1.0), (5.9, 2.2, -1.95, 0.1, 0.82), (11.4, 1.72, -1.35, 0.4, 0.58), (19.0, 1.05, -0.5, 0.8, 0.26), (20.8, 0.85, -0.25, 0.9, 0.14), (21.4, 0.6, -0.05, 0.93, 0.06),
             (21.52, 0.35, 0.1, 0.94, 0.01)],
    "tail": [(0.0, -14.6, -17.5, 0.75, 0.42), (5.8, -15.9, -17.4, 0.75, 0.18), (6.4, -16.3, -17.3, 0.75, 0.1), (6.55, -16.6, -17.1, 0.75, 0.03)],
    "fin": [(0.0, -9.6, -18.2, 1.0, 0.2), (0.0, -13.4, -18.4, 1.6, 0.36), (0.0, -15.3, -18.5, 3.3, 0.32), (0.0, -16.3, -18.55, 5.1, 0.22), (0.0, -16.9, -18.4, 5.8, 0.1), (0.0, -17.4, -18.1, 6.0, 0.02)],
    "patch": 0.0, "colours": {"upper": "B6B9BB", "upper2": "B6B9BB", "under": "AEB1B3", "nose": "B6B9BB", "boss": "2A2B2D"},
    "engines": [(5.9, 2.2, -0.12, True, 5.2), (11.4, 1.72, 0.2, True, 3.8)], "well": (5.9, 0.5, 0.7), "pitot": True,
    # (four low turrets, two on her back and two under her, all aimed from the sighting blisters: one on top and one each
    #  side behind her wing; the gunner's pair of guns in her tail)
    "extras": [("ball", ((6.7, 0.0, 1.42), (0.56, 0.56, 0.26), "upper")),
               ("rod", ((6.85, 0.1, 1.56), (7.85, 0.1, 1.66), 0.024, 0.02, "steel")), ("rod", ((6.85, 0.27, 1.56), (7.85, 0.27, 1.66), 0.024, 0.02, "steel")),
               ("ball", ((7.0, 0.0, -1.42), (0.5, 0.5, 0.24), "under")), ("rod", ((7.15, 0.12, -1.54), (8.15, 0.12, -1.64), 0.024, 0.02, "steel")),
               ("ball", ((-8.6, 0.0, 1.38), (0.5, 0.5, 0.24), "upper")), ("rod", ((-8.75, 0.12, 1.5), (-9.75, 0.12, 1.64), 0.024, 0.02, "steel")),
               ("ball", ((-8.9, 0.0, -1.24), (0.5, 0.5, 0.24), "under")), ("rod", ((-9.05, 0.12, -1.36), (-10.05, 0.12, -1.48), 0.024, 0.02, "steel")),
               ("dome", ((-6.9, 0.0, 1.4), (0.4, 0.4, 0.3))),
               ("dome", ((-7.0, 1.38, 0.35), (0.44, 0.28, 0.42))), ("dome", ((-7.0, -1.38, 0.35), (0.44, 0.28, 0.42))),
               ("rod", ((-19.15, 0.09, 0.5), (-20.0, 0.09, 0.5), 0.026, 0.02, "steel"))],
    "side_marks": [(-11.6, 0.6, "USA-BAR")], "wing_marks": [(16.0, 0.3, 1.1, "USA-BAR", "USA-BAR", False, False)], "code": (-13.8, 0.5),
}


# The B-17 again, to the lines of the real B-17G (Brian, 2026-11-19: "make the b-17 look more like this but in the army
# green color its in now", of a model he linked and five pictures of it; then "use this 3d model is our bench mark for
# all airplanes"). Her lines are measured from a three-view drawing of the real machine (Commons, "Boeing B-17G Flying
# Fortress 3-view line drawing", Kaboldy, CC BY-SA 3.0): measured, not traced, and nothing of the linked model is used.
# What changed from the game's own numbers: her pilots sit in a raised cabin whose roof runs back level past the upper
# turret and falls to her back behind the radio room; her engines are close in, the inner pair ahead of the outer;
# her wing tapers straight from a root well forward; her fin stands on a long fillet up her back, and the gunner's
# cabin is behind her rudder; under her short glass nose is the chin turret with its fairing. Her olive drab over
# grey is as it was.
_B17_FILLET = [(-2.7, 0.6, 0.04, 0.58), (-3.55, 0.6, 0.06, 0.71), (-4.22, 0.6, 0.07, 0.8), (-4.89, 0.6, 0.08, 0.91), (-5.56, 0.6, 0.09, 0.99), (-6.23, 0.6, 0.1, 1.08),
               (-6.8, 0.6, 0.105, 1.17), (-7.57, 0.6, 0.115, 1.29), (-8.24, 0.6, 0.125, 1.52), (-8.75, 0.6, 0.12, 1.84), (-8.9, 0.6, 0.1, 1.86)]
EXTRA_PLANES["B17"] = {
    "title": "B-17G Flying Fortress (our own model)",
    "hull": [(8.3, 0.187, 0.06, 0.06), (8.149, 0.195, 0.24, 0.237), (8.015, 0.169, 0.35, 0.369), (7.881, 0.143, 0.44, 0.484), (7.747, 0.107, 0.52, 0.607), (7.529, 0.107, 0.615, 0.712),
             (7.177, 0.169, 0.72, 0.774), (6.674, 0.178, 0.84, 0.888), (6.171, 0.187, 0.9, 0.967), (5.501, 0.134, 1.0, 1.09), (4.663, 0.064, 1.1, 1.16), (3.825, 0.037, 1.143, 1.187),
             (0.472, 0.03, 1.143, 1.18), (-2.88, 0.02, 1.143, 1.17), (-4.053, -0.016, 1.08, 1.134), (-4.84, -0.011, 1.012, 1.088), (-5.394, -0.007, 0.965, 1.055), (-5.96, 0.0, 0.916, 1.015),
             (-6.8, 0.012, 0.843, 0.956), (-7.07, 0.015, 0.82, 0.937), (-8.747, 0.05, 0.68, 0.81), (-10.42, 0.075, 0.52, 0.685), (-11.2, 0.08, 0.46, 0.62), (-11.36, 0.255, 0.45, 0.785),
             (-12.3, 0.303, 0.42, 0.742), (-12.58, 0.318, 0.411, 0.729), (-12.86, 0.332, 0.402, 0.716), (-13.2, 0.33, 0.385, 0.7), (-13.38, 0.204, 0.375, 0.545), (-13.5, 0.13, 0.36, 0.44),
             (-13.78, 0.11, 0.35, 0.41), (-14.2, 0.09, 0.3, 0.36), (-14.36, 0.085, 0.24, 0.29)],
    "square": 2.0, "screw": 1.76, "blades": 3, "glazed": (7.53, 7.53), "nose_frames": ((7.53,), 6), "nose_bar": 0.6, "nose_cap": 8.2, "nose_frames_paint": "upper",
    "glass_bands": GLASS["B17"],
    # (her cabin and the deck behind it are one body: the windscreen, the pilots' windows and the two in the roof over
    #  them, then the roof back past the upper turret, the radio room's glazed hatch, and its fall to her back)
    "hoods": [([(5.75, 0.35, 0.55, 1.35), (5.42, 0.35, 0.95, 1.35), (4.86, 0.35, 0.95, 1.355), (4.62, 0.35, 0.95, 1.367), (4.12, 0.35, 0.95, 1.367),
                (3.66, 0.35, 0.95, 1.367), (1.14, 0.7, 0.95, 1.017), (0.47, 0.7, 0.95, 0.96), (-0.2, 0.7, 0.94, 0.91), (-0.87, 0.7, 0.91, 0.81), (-1.54, 0.7, 0.86, 0.66),
                (-2.0, 0.7, 0.78, 0.56), (-2.3, 0.7, 0.66, 0.5)], (), False)],
    "cabin": {"square": 4.0, "ease": 1.2, "skirt": (1.06, 0.2), "screen": (5.52, 1.22, 4.83, 1.7, 0.3), "pane": (0.9, 0.66, 1.62, 0.03), "back": 3.66, "sill": 1.09, "top": 1.42, "posts": (4.86, 4.26, 3.66),
              "brow": (4.6, 4.2, 58.0, 80.0), "hatch": (-0.87, -1.73, 67.5)},
    "wing": [(0.0, 3.84, -2.17, -0.5, 1.0), (3.0, 3.39, -1.85, -0.26, 0.84), (6.08, 2.92, -1.52, -0.02, 0.67), (9.0, 2.49, -1.21, 0.21, 0.52), (14.32, 1.68, -0.65, 0.63, 0.26),
             (15.0, 1.57, -0.47, 0.68, 0.2), (15.4, 1.42, -0.2, 0.71, 0.12), (15.72, 1.06, 0.3, 0.74, 0.05), (15.81, 0.72, 0.62, 0.745, 0.02)],
    "tail": [(0.0, -8.4, -11.8, 0.43, 0.42), (5.58, -9.6, -11.26, 0.43, 0.2), (6.12, -9.77, -11.13, 0.43, 0.14), (6.31, -9.92, -10.98, 0.43, 0.09), (6.5, -10.24, -10.67, 0.43, 0.03)],
    "fin": [(0.0, -8.3, -12.85, 0.6, 0.34), (0.0, -8.75, -12.54, 2.44, 0.32), (0.0, -9.17, -12.45, 2.86, 0.3), (0.0, -9.58, -12.32, 3.53, 0.26), (0.0, -9.84, -12.17, 4.27, 0.2),
            (0.0, -10.0, -11.98, 4.6, 0.16), (0.0, -10.42, -11.56, 4.94, 0.09), (0.0, -10.85, -11.18, 5.06, 0.02)],
    "patch": 2.6, "colours": {"upper": "4F5236", "upper2": "595B3F", "under": "84878A", "nose": "4F5236", "boss": "171717"},
    "engines": [(3.06, 3.62, -0.1, True, 2.55), (6.08, 3.16, 0.05, True, 2.05)], "pitot": True, "under_from": -0.6, "frames_paint": "upper",
    "tailwheel": (-8.63, -0.9), "mast": (-0.6, 1.5, 1.95),
    "extras": [("body", (_B17_FILLET[:7], 0.0, 2.0, "coat")), ("body", (_B17_FILLET[6:], 0.0, 2.0, "coat")),
               # (the chin turret, the fairing behind it, and its pair of guns)
               ("ball", ((7.1, 0.0, -0.9), (0.42, 0.44, 0.37), "under")),
               ("body", ([(7.0, -0.88, 0.42, 0.36), (6.6, -0.87, 0.4, 0.3), (6.2, -0.86, 0.33, 0.22), (5.8, -0.88, 0.24, 0.14), (5.5, -0.9, 0.12, 0.07)], 0.0, 2.0, "coat")),
               ("rod", ((7.3, 0.16, -1.0), (8.2, 0.16, -1.16), 0.026, 0.02, "steel")),
               # (the astrodome, the upper turret, the ball under her)
               ("dome", ((6.29, 0.0, 1.12), (0.25, 0.25, 0.24))),
               ("dome", ((3.14, 0.0, 1.72), (0.52, 0.52, 0.47))), ("rod", ((3.3, 0.21, 1.94), (4.4, 0.21, 2.22), 0.026, 0.02, "steel")),
               ("dome", ((-2.82, 0.0, -1.23), (0.56, 0.56, 0.56))), ("rod", ((-2.52, 0.2, -1.46), (-1.62, 0.2, -1.76), 0.026, 0.02, "steel")),
               # (a gun in each cheek of her nose, one at each waist window, one up through the radio room's hatch, a pair in her tail)
               ("rod", ((7.0, 0.72, 0.3), (7.6, 1.12, 0.1), 0.022, 0.018, "steel")),
               ("rod", ((-5.15, 0.93, 0.32), (-5.75, 1.9, 0.15), 0.022, 0.018, "steel")),
               ("rod", ((-1.3, 0.0, 1.4), (-1.95, 0.0, 1.98), 0.022, 0.018, "steel")),
               ("rod", ((-14.3, 0.09, 0.19), (-15.02, 0.09, 0.19), 0.026, 0.02, "steel")),
               # (her main wheels go up forward into the inner nacelles, and half of each tyre stays out in the wind)
               ("rod", ((3.05, 2.9, -0.44), (3.05, 3.22, -0.44), 0.66, 0.66, "tyre")), ("rod", ((3.05, 2.88, -0.44), (3.05, 3.24, -0.44), 0.3, 0.3, "leg")),
               # (the turbo-supercharger under each nacelle)
               ("rod", ((2.5, 3.06, -0.6), (2.5, 3.06, -0.7), 0.26, 0.26, "soot")), ("rod", ((2.2, 6.08, -0.42), (2.2, 6.08, -0.52), 0.26, 0.26, "soot"))],
    "side_marks": [(-3.72, 0.5, "USA-BAR")], "wing_marks": [(11.6, 0.75, 0.95, "USA-BAR", "USA-BAR", False, False)], "code": (-7.3, 0.5),
}

# The single-seat fighters, laid out again to measured drawings with framed glazing of their own (fighters.py).
EXTRA_PLANES.update(fighters.FIGHTERS)


# The Ju 87B-2, laid out again against a three-view drawing of the real machine (Wikimedia Commons, "Junkers Ju 87B-2
# Stuka.svg", Kaboldy, CC BY-SA 3.0; measured, not traced): the deep cowling of her inverted-vee engine with its
# radiator bath under it, the tall framed greenhouse over her two men, the cranked wing (down to the legs, then up to
# the tips) whose trailing edge sweeps forward, the trousered legs and their spats with a siren on each, the dive
# brake under each wing, the square fin and the strutted tailplane. She is black all over (Brian, 2026-11-19, of a
# picture of one: "here is a stuka but i want it in the all black"), with plain crosses.
_BLACK = {"upper": "1D1E20", "upper2": "27292B", "under": "1D1E20", "nose": "1D1E20", "boss": "141414"}
EXTRA_PLANES["Stuka"] = {
    "title": "Ju 87B Stuka (our own model)",
    "hull": [(4.6, 0.0, 0.03, 0.03), (4.47, 0.0, 0.16, 0.16), (4.26, 0.0, 0.26, 0.26), (4.05, 0.0, 0.27, 0.27), (3.99, 0.02, 0.32, 0.35), (3.85, 0.03, 0.38, 0.41),
             (3.5, 0.02, 0.42, 0.47), (3.0, 0.0, 0.44, 0.57), (2.6, -0.06, 0.46, 0.68), (2.0, 0.05, 0.47, 0.68), (1.42, 0.08, 0.47, 0.74), (0.95, -0.1, 0.47, 0.6),
             (0.0, -0.12, 0.47, 0.62), (-1.3, -0.06, 0.46, 0.56), (-1.6, 0.025, 0.43, 0.525), (-2.2, 0.055, 0.37, 0.495), (-3.0, 0.09, 0.3, 0.44), (-3.75, 0.11, 0.255, 0.39),
             (-4.6, 0.155, 0.19, 0.365), (-5.5, 0.2, 0.11, 0.3), (-6.2, 0.22, 0.05, 0.22), (-6.45, 0.25, 0.025, 0.12)],
    "square": 3.0, "screw": 1.7, "blades": 3, "nose_back": None, "spinner": 4,
    # (her greenhouse: a raked windscreen of flat panes, the pilot's hood, the fixed middle with the mast, the gunner's
    #  hood down to his gun; flat sides that lean in, glass right over the top, a frame every third of a metre)
    "hoods": [([(1.8, 0.3, 0.26, 0.82), (1.5, 0.3, 0.37, 0.82), (1.1, 0.3, 0.38, 0.82), (0.0, 0.3, 0.38, 0.82), (-0.5, 0.3, 0.37, 0.82), (-1.1, 0.3, 0.34, 0.73),
                (-1.36, 0.3, 0.3, 0.4), (-1.55, 0.3, 0.22, 0.26)], (), False)],
    "cabin": {"square": 3.2, "ease": 0.3, "skirt": (0.5, 0.25), "fillet": 0.05, "screen": (1.46, 0.8, 1.12, 1.1, 0.3), "pane": (0.31, 0.25, 1.03, 0.03), "back": -1.55,
              "sill": 0.52, "top": 0.95, "hoops": (0.98, 0.66, 0.38, 0.02, -0.16, -0.5, -0.8, -1.1), "spine": True, "roof": True},
    "wing": [(0.0, 1.8, -1.55, -0.5, 0.5), (0.47, 1.8, -1.55, -0.52, 0.5), (1.4, 1.8, -1.24, -0.655, 0.46), (1.6, 1.79, -1.2, -0.65, 0.45), (2.6, 1.71, -0.96, -0.477, 0.38),
             (4.1, 1.6, -0.6, -0.218, 0.3), (5.6, 1.48, -0.05, 0.042, 0.2), (6.5, 1.41, 0.3, 0.198, 0.13), (6.78, 1.36, 0.42, 0.246, 0.09), (6.87, 1.15, 0.5, 0.262, 0.05),
             (6.9, 0.95, 0.62, 0.267, 0.01)],
    "flap": (0.21, 0.585, 0.76), "aileron": (0.6, 0.985, 0.74),
    "tail": [(0.0, -4.87, -5.88, 0.63, 0.13), (2.25, -4.87, -5.88, 0.63, 0.1), (2.4, -4.9, -5.88, 0.63, 0.07), (2.47, -5.02, -5.85, 0.63, 0.02)],
    "fin": [(0, -4.42, -6.5, 0.3, 0.16), (0, -4.72, -6.47, 1.0, 0.14), (0, -5.1, -6.43, 1.8, 0.11), (0, -5.2, -6.4, 1.93, 0.09), (0, -5.34, -6.24, 2.0, 0.03)],
    "patch": 2.6, "colours": dict(_BLACK), "frames_paint": "upper",
    "stubs": (3.82, 2.55, -10.0), "pitot": True, "guns": [(1.97, 0.0, 0.45, 0.02)],
    # (the radiator bath under her engine, and the oil cooler's intake on top of her cowling)
    "scoops": [([(3.6, -0.6, 0.3, 0.2), (3.55, -0.66, 0.37, 0.28), (3.0, -0.68, 0.38, 0.29), (2.65, -0.72, 0.37, 0.31), (2.58, -0.66, 0.3, 0.2)], 0.0, 3.0),
               ([(2.8, 0.6, 0.09, 0.05), (2.74, 0.62, 0.12, 0.08), (2.45, 0.67, 0.11, 0.07), (2.2, 0.7, 0.05, 0.02)], 0.0, 2.6)],
    "tailwheel": (-5.67, -0.52), "mast": (-0.27, 1.1, 2.0),
    "extras": [  # (each leg in its trouser, the spat round the wheel, the tyre showing under it)
               ("body", ([(1.82, -1.1, 0.03, 0.22), (1.72, -1.1, 0.09, 0.3), (1.45, -1.1, 0.115, 0.33), (1.12, -1.1, 0.07, 0.31), (0.93, -1.1, 0.02, 0.27)], 1.5, 3.0, "upper")),
               ("body", ([(2.3, -1.74, 0.03, 0.04), (2.22, -1.73, 0.13, 0.19), (2.0, -1.7, 0.2, 0.29), (1.6, -1.7, 0.22, 0.3), (1.2, -1.71, 0.17, 0.23), (0.85, -1.72, 0.09, 0.11),
                          (0.62, -1.72, 0.02, 0.03)], 1.5, 2.4, "upper")),
               ("rod", ((1.75, 1.41, -2.0), (1.75, 1.59, -2.0), 0.33, 0.33, "tyre")),
               # (the siren on each leg: a small airscrew that howls in the dive)
               ("rod", ((2.0, 1.5, -0.86), (1.78, 1.5, -0.86), 0.025, 0.055, "upper")),
               ("rod", ((1.94, 1.33, -0.86), (1.94, 1.67, -0.86), 0.013, 0.013, "steel")), ("rod", ((1.94, 1.5, -1.03), (1.94, 1.5, -0.69), 0.013, 0.013, "steel")),
               # (the dive brake under each wing, outboard of the leg; the struts under her tailplane)
               ("rod", ((1.38, 1.8, -0.9), (1.3, 3.9, -0.5), 0.035, 0.035, "upper")),
               ("rod", ((1.38, 2.2, -0.82), (1.38, 2.2, -0.68), 0.015, 0.015, "upper")), ("rod", ((1.32, 3.5, -0.57), (1.32, 3.5, -0.42), 0.015, 0.015, "upper")),
               ("rod", ((-5.0, 1.2, 0.6), (-4.76, 0.17, -0.02), 0.02, 0.02, "upper")), ("rod", ((-5.05, 1.2, 0.6), (-4.98, 0.15, -0.02), 0.02, 0.02, "upper")),
               # (her gunner's gun; her bomb on its crutch)
               ("rod", ((-1.33, 0.0, 0.72), (-1.95, 0.0, 0.87), 0.022, 0.018, "steel")),
               ("body", ([(1.55, -0.98, 0.03, 0.03), (1.2, -0.98, 0.2, 0.2), (0.2, -0.98, 0.23, 0.23), (-0.5, -0.98, 0.1, 0.1), (-0.75, -0.98, 0.04, 0.04)], 0.0, 2.0, "bomb"))],
    "side_marks": [(-2.95, 0.34, "GER")], "wing_marks": [(4.7, 0.1, 0.5, "GER", "GER", True, True)], "code": (-1.9, 0.32),
}


def cabin_angle(hood, p):
    """Where round a hood a point of its skin is: degrees up from level on its own side (90 at its top)."""
    w, h = max(hood.w(-p.x), 1.0e-6), max(hood.h(-p.x), 1.0e-6)
    e = 0.5 * hood.square_at(p.x) if hasattr(hood, "square_at") else 1.0 / hood.power
    ty, tz = min(abs(p.y - hood.side) / w, 1.0), min(max((p.z - hood.z(-p.x)) / h, -1.0), 1.0)
    return math.degrees(math.atan2(math.copysign(abs(tz) ** e, tz), ty ** e))


def cabin_paint(cabin, hood, coat, hull):
    """The paint of a raised cabin (`fitout.cabin_body`): glass where its windows are, her own coat elsewhere. Her
    numbers give `cabin`: `screen` (the windscreen's two flat panes: all of them that stands clear of her fuselage is
    glass), `back` (where her side windows end), `sill` and `top` (the heights her side windows lie between), `brow`
    (a window in the roof over each pilot: from, to along her; from, to round her), `hatch` (a glazed hatch across her
    back: from, to, and how far round it begins), `roof` (a greenhouse: all of it above its sill is glass)."""
    brow, hatch, roof = cabin.get("brow"), cabin.get("hatch"), cabin.get("roof")

    def paint(p, n):
        if abs(p.z - hood.pane_z(p.x, p.y)) < 0.004:
            return "glass"                                 # (on a pane of her windscreen)
        if p.x > cabin["back"]:
            if roof and p.z > cabin["sill"]:
                return "glass"                             # (a greenhouse: glass from its sill right over its top)
            if cabin["sill"] < p.z < cabin["top"] and abs(n.y) > 0.5:
                return "glass"
            if brow and brow[1] < p.x < brow[0] and brow[2] < cabin_angle(hood, p) < brow[3]:
                return "glass"
        elif hatch and hatch[1] < p.x < hatch[0] and cabin_angle(hood, p) > hatch[2]:
            return "glass"
        return coat(p, n)
    paint.blend = coat.blend
    paint.coat = coat
    return paint


def cabin_frames(parts, frames, hood, cabin, hull):
    """The bars of a cabin's windows. Her windscreen is two flat panes, each glazed from the post between them out to
    a straight post at its side, under a straight bar along its top and over one along its foot (`pane` in her
    numbers: how far out its foot and its top reach, how high its top's outer end, how far its top stands under her
    roof); what is left of the flat round each pane is plated over in her skin's paint. Then a rail along the sills
    and the tops of her side windows, a post between them, and a bar round each window in her roof."""
    sill, top, back = cabin["sill"], cabin["top"], cabin["back"]
    wide = 1.0 if parts.fine else 1.4
    (xa, za, xb, zb), sweep = cabin["screen"][:4], cabin["screen"][4]
    y_foot, y_top, z_out, drop = cabin.get("pane", (0.87, 0.66, 1.62, 0.03))
    run = (xa - xb) / (zb - za)
    z_in = zb - drop

    def lay(points, half, lift=0.011):
        pts = [p for p in points if p is not None]
        if len(pts) > 1:
            parts.strip(frames, [p for p, n in pts], [n for p, n in pts], half * wide, lift)

    def on_box(x, deg):
        a = math.radians(deg)
        return hood.at(x, a), hood.out(x, a)

    def arc(x, d0, d1, half=0.02, both=True):
        n = max(8, int(abs(d1 - d0) / 7.0))
        for hand in ((1, -1) if both else (1,)):
            lay([on_box(x, (d0 + (d1 - d0) * k / n) if hand > 0 else 180.0 - (d0 + (d1 - d0) * k / n)) for k in range(n + 1)], half, 0.008)

    def rail(deg_at, x0, x1, half=0.018, both=True):
        n = max(2, int((x0 - x1) / 0.1))
        xs = [x0 - (x0 - x1) * k / n for k in range(n + 1)]
        for hand in ((1, -1) if both else (1,)):
            lay([on_box(x, deg_at(x) if hand > 0 else 180.0 - deg_at(x)) for x in xs], half, 0.008)

    # ---- the windscreen. A place on a pane is given by how far out it is and how high: (y, z).
    def on_pane(y, z, hand):
        return Vector((xa - sweep * abs(y) - run * (z - za), y * hand, z))

    def on(y, z):
        """Whether the pane, this far out and this high, is her skin: within the cabin and clear of her fuselage."""
        p = on_pane(y, z, 1.0)
        return hood.back < p.x < hood.front and p.z >= fitout.body_top(hull, p.x, p.y) and hood.holds(p.x, p.y, p.z - 0.004)

    def span(y):
        """From how low to how high the pane is her skin, this far out; None if it is not."""
        held = [z for z in (za - 1.2 + 0.01 * k for k in range(int((zb + 1.5 - za) / 0.01) + 1)) if on(y, z)]
        if not held:
            return None
        ends = []
        for inside, outside in ((held[0], held[0] - 0.01), (held[-1], held[-1] + 0.01)):
            for _ in range(8):
                mid = 0.5 * (inside + outside)
                inside, outside = (mid, outside) if on(y, mid) else (inside, mid)
            ends.append(inside)
        return ends[0], ends[1]

    def top_of(y):
        return z_in + (z_out - z_in) * min(abs(y) / y_top, 1.0)

    def glazed_to(y):
        """How high the glass goes this far out: to the bar along its top, or to the post at its side."""
        if y <= y_top:
            return top_of(y)
        return z_out + (sill - z_out) * (y - y_top) / (y_foot - y_top)

    def sheet(outer, inner, hand):
        """Plating over the flat between two rows of places on a pane."""
        n = hood.pane_n(hand)
        rows = [(frames.verts.new(on_pane(a[0], a[1], hand) + n * 0.006), frames.verts.new(on_pane(b[0], b[1], hand) + n * 0.006)) for a, b in zip(outer, inner)]
        for (a0, b0), (a1, b1) in zip(rows, rows[1:]):
            if (a0.co - b0.co).length < 1.0e-4 and (a1.co - b1.co).length < 1.0e-4:
                continue
            f = frames.faces.new((a0, b0, b1, a1))
            f.normal_update()
            if f.normal.dot(n) < 0.0:
                f.normal_flip()

    y_max = y_foot
    while y_max < 1.5 and span(y_max + 0.01):
        y_max += 0.01                                      # (how far out the flat goes at all)
    cols = sorted(set([y_top * k / 12.0 for k in range(13)] + [y_top + (y_foot - y_top) * k / 10.0 for k in range(11)] + [y_foot + (y_max - y_foot) * k / 8.0 for k in range(1, 9)]))
    spans = [span(y) for y in cols]
    cols, spans = [y for y, sp in zip(cols, spans) if sp], [sp for sp in spans if sp]
    feet = [max(sill, lo) for lo, hi in spans]             # (the foot of the glass: her sill, or her fuselage where that stands higher)
    tops = [max(min(glazed_to(y), hi), ft) for y, (lo, hi), ft in zip(cols, spans, feet)]
    inner = [k for k, y in enumerate(cols) if y <= y_foot + 1.0e-9]
    outer = [k for k, y in enumerate(cols) if y >= y_foot - 1.0e-9]
    for hand in (1.0, -1.0):
        # (what is left of the flat is plated over: above the glass, under its foot down to her fuselage, and all of it outside the post at its side)
        sheet([(cols[k], spans[k][1] + 0.004) for k in inner], [(cols[k], tops[k]) for k in inner], hand)
        sheet([(cols[k], spans[k][0] - 0.012) for k in inner], [(cols[k], feet[k]) for k in inner], hand)
        sheet([(cols[k], spans[k][1] + 0.004) for k in outer], [(cols[k], spans[k][0] - 0.012) for k in outer], hand)
        n = hood.pane_n(hand)
        bar = lambda places, half: lay([(on_pane(y, z, hand), n) for y, z in places], half)
        bar([(y_top * k / 8.0, top_of(y_top * k / 8.0)) for k in range(9)], 0.022)       # (its top)
        post = [(y_top + (y_foot - y_top) * k / 10.0, z_out + (sill - z_out) * k / 10.0) for k in range(11)]
        bar([(y, z) for y, z in post if z >= (span(y) or (sill, sill))[0] - 0.02], 0.024)   # (the post at its side, down to its foot)
        bar([(cols[k], feet[k]) for k in inner], 0.02)                                    # (its foot)
    foot = feet[0]
    post = [(0.0, foot + (z_in + drop - foot) * k / 10.0) for k in range(11)]
    lay([(on_pane(y, z, 1.0), hood.pane_n(0.0)) for y, z in post], 0.026, 0.013)

    # ---- her side windows
    for x in cabin.get("posts", ()):
        arc(x, hood.deg_of(x, sill), hood.deg_of(x, top))
    x_sill = max((hood.cut_x(math.radians(hood.deg_of(xb, sill))) or xb), xb)
    x_top = max((hood.cut_x(math.radians(hood.deg_of(xb, top))) or xb), xb)
    rail(lambda x: hood.deg_of(x, sill), x_sill, back)
    rail(lambda x: hood.deg_of(x, top), x_top, back)
    for x in cabin.get("hoops", ()):                       # (a greenhouse's frames: right over it, sill to sill)
        arc(x, hood.deg_of(x, sill), 180.0 - hood.deg_of(x, sill), 0.02, both=False)
    if cabin.get("spine"):
        rail(lambda x: 90.0, hood.roof_from - 0.02, back + 0.04, 0.018, both=False)
    brow, hatch = cabin.get("brow"), cabin.get("hatch")
    if brow and parts.fine:
        for a in brow[2:]:
            rail(lambda x, a=a: a, brow[0], brow[1], 0.012)
        for x in brow[:2]:
            arc(x, brow[2], brow[3], 0.012)
    if hatch and parts.fine:
        rail(lambda x: hatch[2], hatch[0], hatch[1], 0.014)
        n = max(2, int(round((hatch[0] - hatch[1]) / 0.29)))
        for k in range(n + 1):
            arc(hatch[0] - (hatch[0] - hatch[1]) * k / n, hatch[2], 180.0 - hatch[2], 0.014, both=False)


def battle_us(wing, step, shares=()):
    """Where a flying surface's rings stand along its span: the game's own, closer, and one exactly at each given share
    of the span (the ends of its control surfaces)."""
    us = wing.us(step)
    span = wing.tip - wing.root
    for share in shares:
        u = wing.root + span * share
        us = [v for v in us if abs(v - u) > 0.05 or v in (wing.root, wing.tip)]
        if all(abs(v - u) > 1e-6 for v in us):
            us.append(u)
    return sorted(us)


def game():
    """The game's own aircraft sculptor, loaded without running it, at a battle's grain."""
    global _G
    if _G is None:
        path = os.path.join(HERE, "game", "build_plane.py")
        src = open(path).read().split("\ndef main():")[0]
        for old, new in FINER + JETS + GRADED + NOSES + GILLS + CABIN + fighters.CANOPY:
            if src.count(old) != 1:
                raise ValueError("the game's sculptor has changed here: " + old)
            src = src.replace(old, new)
        _G = {"__file__": path, "__name__": "build_plane", "BATTLE": BATTLE, "BATTLE_CHORDS": {True: CHORDS, False: CHORDS_PLAIN}, "battle_us": battle_us,
              "BATTLE_FLAP": FLAP, "BATTLE_AILERON": AILERON, "BATTLE_ELEVATOR": ELEVATOR, "BATTLE_RUDDER": RUDDER, "LINE_DRAWN": LINE,
              "battle_cabin_paint": cabin_paint, "battle_cabin_frames": cabin_frames, "battle_cabin_body": fitout.cabin_body,
              "battle_canopy": fighters.canopy, "battle_glare": fighters.glare}
        exec(compile(src, path, "exec"), _G)
        _G["PAINTS"]["torpedo"] = len(_G["PAINTS"])        # (a paint of a battle's own: what a torpedo bomber carries)
        _G["COMMON"]["torpedo"] = "6C7174"
        _G["PLANES"].update(EXTRA_PLANES)
    return _G


def names():
    return list(game()["PLANES"])


# What kind of machine each is, and how many are aboard (the parachutes a lost one can leave).
CREW = {"Betty": 7, "Nell": 7, "Amerikabomber": 8, "B29": 11, "Kate": 3, "Val": 2, "Dauntless": 2, "Avenger": 3, "CarrierFighter": 1, "DiveBomber": 2, "TorpedoBomber": 3, "Hellcat": 1, "Me262": 1, "Bf109": 1, "Spitfire": 1, "Zero": 1, "Mustang": 1, "Yak": 1, "Fw190": 1, "Hurricane": 1, "Warhawk": 1, "Fighter": 1,
        "Stuka": 2, "Sturmovik": 2, "He111": 5, "Lancaster": 7, "B17": 10, "Bomber": 5}
ROLE = {"Betty": "bomber", "Nell": "bomber", "Amerikabomber": "bomber", "B29": "bomber", "Kate": "torpedo bomber", "Val": "dive bomber", "Dauntless": "dive bomber", "Avenger": "torpedo bomber", "DiveBomber": "dive bomber", "TorpedoBomber": "torpedo bomber", "Stuka": "dive bomber", "Sturmovik": "attack", "He111": "bomber", "Lancaster": "bomber", "B17": "bomber", "Bomber": "bomber"}


# ---- later coats of paint ----------------------------------------------------------------------------------
# The same machine in other colours, by the month they come in (the art director's unit table, `carrier_paint`). The
# file lists them under `schemes`; the game paints her in the last one whose month has come.
_THREE_BLUES = {"upper": "2A3850", "upper2": "55708C", "under": "D9DAD3", "nose": "2A3850"}
_SEA_BLUE = {"upper": "1E2A40", "upper2": "1E2A40", "under": "1E2A40", "nose": "1E2A40", "finish": "gloss"}     # (the war's one gloss coat)
_DARK_GREEN = {"upper": "2F4236", "upper2": "2F4236", "under": "A3A69C"}
SCHEMES = {
    "Hellcat": [("1944-07", "overall sea blue", _SEA_BLUE)],
    "Dauntless": [("1943-02", "three blues over white", _THREE_BLUES), ("1944-07", "overall sea blue", _SEA_BLUE)],
    "Avenger": [("1943-02", "three blues over white", _THREE_BLUES), ("1944-07", "overall sea blue", _SEA_BLUE)],
    "Zero": [("1942-07", "dark green over grey", _DARK_GREEN)],
    "Kate": [("1942-07", "dark green over grey", _DARK_GREEN)],
    "Val": [("1942-07", "dark green over grey", _DARK_GREEN)],
}
# (the Dauntless and the Avenger are one colour on top in 1941, but their skins are laid out for the three blues that
#  follow: the band down their sides and their fins are `upper2`, the same blue-grey until the later coat)
EXTRA_PLANES["Dauntless"]["graded"] = navy_blues(EXTRA_PLANES["Dauntless"]["hull"], 2.1, -3.6)
EXTRA_PLANES["Avenger"]["graded"] = navy_blues(EXTRA_PLANES["Avenger"]["hull"], 2.3, -4.9)


# A marking that changes with the months (the art director, 2026-11-18): the American star has a red centre until
# May 1942, is plain from June 1942, and has white bars from September 1943. Whatever form a machine's numbers give
# her, the file says which she wears when (`mark_months`), and the game draws that one.
# Japan's navy (the historian through the art director, 07:42 UTC the same day): a thin white ring round the disc on
# the FUSELAGE only, from October 1942 to December 1943, then the plain disc again; the wings' discs stay plain. A form
# with a third word holds only for the marks in that place (a mark's `place`: side, wing, fin or code).
MARK_MONTHS = {"USA": [(None, "USA-RED"), ("1942-06", "USA"), ("1943-09", "USA-BAR")],
               "JAP": [(None, "JAP-PLAIN"), ("1942-10", "JAP-RING", "side"), ("1944-01", "JAP-PLAIN", "side")]}
MARK_FAMILY = {"USA": "USA", "USA-BAR": "USA", "USA-RED": "USA", "JAP-PLAIN": "JAP", "JAP-RING": "JAP"}


def mark_in(tag, month, place=None):
    """The form of a marking in a month (YYYY-MM): the last whose month has come, of those that hold in this place."""
    forms = MARK_MONTHS.get(MARK_FAMILY.get(tag))
    if not forms or not month:
        return tag
    fit = [f[1] for f in forms if (f[0] is None or f[0] <= month) and (len(f) < 3 or f[2] == place)]
    return fit[-1] if fit else tag


# Japan's navy airscrews (the historian through the art director, 2026-11-18): no yellow tips. For the whole war the
# early look: the front of each blade bare metal, its back red-brown, two thin red bands across it near the tip, a bare
# metal spinner. Her spinning disc is pale metal with a faint red ring where the bands are.
NAVY_AIRSCREW = {"blade": "A6AAAC", "blade_back": "5B352A", "blade_band": "B4281F", "boss": "8E9092", "blur": "A9ADAF", "blur_tip": "B4281F"}
NAVY_BANDS = ((0.80, 0.835), (0.885, 0.92))                 # (where the two red bands lie, as parts of the blade's length)


def navy_airscrew(m):
    """Paints a Japanese navy machine's blades: red-brown behind, and the two red bands cut across each."""
    blade, back, band = m.paint_at["blade"], m.paint_at["blade_back"], m.paint_at["blade_band"]
    for pi, part in enumerate(m.parts):
        if not part.name.startswith("prop.") or part.name.endswith(".blur"):
            continue
        hub, axis = Vector(part.pivot), Vector(part.axis).normalized()
        which = [k for k in range(len(m.faces)) if m.fpart[k] == pi and m.fpaint[k] == blade]
        if not which:
            continue

        def radius(i):
            d = m.verts[i] - hub
            return (d - axis * d.dot(axis)).length
        R = max(radius(i) for k in which for i in m.faces[k])
        red = []
        for r0, r1 in NAVY_BANDS:
            inside, outside = m.cut_ring(which, hub, axis, R * r0)
            mid, beyond = m.cut_ring(outside, hub, axis, R * r1)
            red += mid
            which = inside + mid + beyond
        red = set(red)
        for k in which:
            if k in red:
                m.fpaint[k] = band
                continue
            a, b, c = (m.verts[i] for i in m.faces[k][:3])
            if (b - a).cross(c - a).dot(axis) < 0.0:
                m.fpaint[k] = back


def paints_of(G, s, colours=None):
    """Every paint of a machine, by name, from her numbers (or from them with some colours changed: a later coat)."""
    colours = {k: v for k, v in (colours or {}).items() if k != "finish"}
    c = G["colours_of"](dict(s, colours=dict(s["colours"], **colours)) if colours else s)
    paints = {k: c.get(k, "808080") for k in G["PAINTS"]}
    paints["torn"] = "2B2825"
    paints["blur"] = "3A3A36"
    paints["blur_tip"] = "D9B224"
    if fitout.nation(s) == "JAP":
        paints.update(NAVY_AIRSCREW)
    if s.get("lines"):                                       # (a pale machine: her lines toward her paler upper colour, or she reads as a blueprint)
        paints["line"] = rigid.mix_hex(paints["line"], paints["upper2"], s["lines"][1])
    else:
        paints["line"] = rigid.mix_hex(paints["line"], paints["upper"], 0.3)      # (scribed lines seen close: finer and softer than the map's)
    paints.update(fitout.paints_for(s))
    paints.update(fitout.rivet_paints(paints))
    return paints, c


def schemes_of(G, name, s, paints, ink):
    """Her later coats as the file gives them: when each comes in, its name, and only the paints it changes."""
    out = []
    for start, title, change in SCHEMES.get(name, []):
        later, c = paints_of(G, s, change)
        entry = {"from": start, "name": title, "colours": {k: v for k, v in later.items() if paints.get(k) != v}}
        if c["ink"] != ink:
            entry["number_ink"] = c["ink"]
        if change.get("finish"):
            entry["finish"] = change["finish"]              # ("gloss": this coat alone shines a little)
        out.append(entry)
    return out


# ---- her record, painted on her ----------------------------------------------------------------------------
# Brian, 2026-11-18: an ace's kills by his cockpit, a bomber's missions as rows of small yellow bombs on her nose, a
# star over a mission she led. The file says where each mark goes (a slot: a place on her port side and the way her
# skin faces there, in the game's axes); the game paints as many as her record in the campaign has earned, first slot
# first. Rows are level and read from the front.
# The art director's rulings (03:26 UTC that day): each nation's own habit, never a party emblem, no names, nose art or
# badges, nothing copied from any real machine. A victim's mark is the plain marking of the nation that lost the
# machine, in small (Italy a little green, white and red flag; France her roundel). Germany's fighters carry white bars
# on the rudder instead; the Soviet Union's, small red stars under the cockpit. Bombers of Britain and America carry
# the yellow bombs, in rows of twenty, their gunners' kills beneath; other nations' bombers carry nothing. About a hand
# high; at most three rows on a fighter and five on a bomber.
#   role: (rows of kills, rows of missions, marks to a row, half a kill mark, half a bomb's height)
TALLY = {"fighter": (3, 0, 10, 0.05, 0.0), "dive bomber": (1, 2, 12, 0.05, 0.06), "attack": (1, 2, 12, 0.05, 0.06), "torpedo bomber": (1, 2, 12, 0.05, 0.06),
         "bomber": (2, 3, 20, 0.05, 0.06)}
TALLY_ROW = {"He111": 16, "Bomber": 16, "Betty": 16, "Nell": 16}      # (a medium bomber's nose is shorter)
TALLY_AT = {"He111": 4.45, "Amerikabomber": 6.8, "B29": 8.25}                   # (her cockpit is her glass nose: her record goes on the first solid skin behind it)
LEAD = 0.026                                 # (half the star over a mission flown as leader)
BAR = 0.045                                  # (half the height of a white bar on a rudder)


def habit(nation, role):
    """How a nation's machines of this kind wear their record: the art director's ruling."""
    if role == "fighter":
        return {"GER": "bars", "SOV": "stars"}.get(nation, "victims")
    return "bombs" if nation in ("UK", "USA") else "none"


def tally_of(name, s, hull, role, G, fins):
    """Where her record is painted: under the sill of her cockpit on the port side, running aft from her windscreen
    (and, for a fighter Germany flies, on the port face of her rudder)."""
    n_kill, n_miss, per_row, r_kill, r_bomb = TALLY[role]
    per_row = TALLY_ROW.get(name, per_row)
    hoods = s.get("hoods", [])
    hood = G["Body"](hoods[0][0], 2.0) if hoods else None
    nation = fitout.nation(s)
    gap = 0.02

    def side(x, z):
        """The place on her port side at this height, and the way her skin faces there."""
        t = max(-0.97, min(0.97, (z - hull.z(-x)) / hull.h(-x)))
        a = math.pi - math.asin(math.copysign(abs(t) ** (1.0 / hull.power), t))
        return hull.at(x, a), hull.out(x, a)

    def sill(x):
        """How high her skin first comes clear of her hood on that side."""
        if hood is None or not hood.back < x < hood.front:
            return None
        for k in range(0, 90):
            p = hull.at(x, math.radians(90.0 + k))
            dy, dz = p.y / max(hood.w(-x), 1.0e-3), (p.z - hood.z(-x)) / max(hood.h(-x), 1.0e-3)
            if dy * dy + dz * dz > 1.0:
                return p.z
        return None

    if name in TALLY_AT or hood is None:
        x0 = TALLY_AT.get(name, hull.front - 0.3 * (hull.front - hull.back))
    else:
        tall = max(r[3] for r in hoods[0][0])
        x0 = max(r[0] for r in hoods[0][0] if r[3] >= 0.6 * tall) + 0.1          # (from the foot of her windscreen)
    step_k = 2.0 * r_kill + gap
    step_b = max(2.0 * 0.38 * r_bomb, 2.0 * LEAD) + 0.03                       # (a bomb is slimmer than a kill mark is wide)
    xs_kill = [x0 - r_kill - k * step_k for k in range(per_row)]
    xs_bomb = [x0 - 0.5 * step_b - k * step_b for k in range(per_row)]
    reach = xs_kill + (xs_bomb if n_miss else [])
    x_mid = 0.5 * (max(reach) + min(reach))
    sills = [v for v in (sill(x) for x in (max(reach), x_mid, min(reach))) if v is not None]
    top = min(sills) - 0.05 if sills else hull.z(-x_mid) + 0.62 * hull.h(-x_mid)
    lean = 0.75 if role == "fighter" else 0.55            # (no higher than where her side still faces a man standing beside her)
    while side(x_mid, top - r_kill)[1].z > lean:
        top -= 0.01

    def down(z, d):
        """A height lower by this much of her skin (her side leans in toward the top)."""
        n = side(x_mid, z)[1]
        return z - d * math.sqrt(max(0.15, 1.0 - n.z * n.z))

    def row(xs, z):
        out = []
        for x in xs:
            p, n = side(x, z)
            out.append([round(v, 3) for v in (p.x, p.y, p.z, n.x, n.y, n.z)])
        return out

    out = {"side": "port", "nation": nation, "habit": habit(nation, role) if nation != "*" else "by nation",
           "note": "a slot is a place in the game's axes and the way the skin faces there (x, y, z, nx, ny, nz); fill each list in order"}
    z = top
    bombs, stars = [], []
    for _ in range(n_miss):
        z_star = down(z, LEAD + 0.01)
        zc = down(z_star, LEAD + 0.012 + r_bomb)
        if side(x_mid, zc)[1].z < -0.3:
            break
        stars += row(xs_bomb, z_star)
        bombs += row(xs_bomb, zc)
        z = down(zc, r_bomb + gap)
    if bombs:
        out["missions"] = {"tag": "BOMB", "radius": r_bomb, "per_row": per_row, "slots": bombs}
        out["lead"] = {"tag": "LEAD", "radius": LEAD, "per_row": per_row, "slots": stars, "note": "slot k stands over mission k: painted if she led it"}
    kills = []
    for _ in range(n_kill):
        zc = down(z, r_kill)
        if side(x_mid, zc)[1].z < -0.3:
            break
        kills += row(xs_kill, zc)
        z = down(zc, r_kill + gap)
    if kills:
        out["kills"] = {"tag": "KILL-<the loser's nation>, or KILL-STAR (the Soviet habit)", "radius": r_kill, "per_row": per_row, "slots": kills}
    if role == "fighter" and nation in ("GER", "*") and len(fins) == 1:
        fin = G["Wing"](fins[0][0], 1.0, True, fins[0][1])
        span = fin.tip - fin.root
        bars, u, step = [], fin.root + 0.8 * span, 2.0 * 0.3 * BAR + 0.016
        while len(bars) < 30 and u > fin.root + 0.22 * span:
            chord = abs(fin.trail(u) - fin.lead(u))
            n = min(10, 30 - len(bars), int(chord * (0.95 - RUDDER[2] - 0.04) / step))
            for k in range(n):
                c = RUDDER[2] + 0.04 + (0.3 * BAR + k * step) / chord
                p = fin.at(u, c, False)
                bars.append([round(p.x, 3), round(p.y, 3), round(p.z, 3), 0.0, -1.0, 0.0])
            u -= 2.0 * BAR + 0.025
        out["rudder"] = {"tag": "KILL-BAR", "part": "rudder", "radius": BAR, "slots": bars, "note": "Germany's habit: a white bar a kill, on the rudder, and it turns with the rudder"}
    return out


def port(v):
    """A place in the game's axes (y to the right) as the model has it (y to port)."""
    return Vector((v[0], -v[1], v[2]))


class Facts:
    """What the clips and the films need to know about a machine."""


def rudder_flag(m):
    """Where her owner's flag goes on a plain machine's rudder, both sides of it. The art director, 2026-11-18, for
    Brian's "have the country logo show on the tanks and planes just like the major nations": the game paints the
    stripes the unit table gives the country (`fin`: dir h, bands from top to bottom; dir v, from the leading edge
    back). `quad` is the flag's four corners on the rudder's side in x and z (leading-bottom, trailing-bottom,
    trailing-top, leading-top): most of the rudder, clear of its edges and of its tip. `at` and `radius` are its
    middle and half its height, for a game that only draws a square there. It rides the part `rudder`."""
    pi = m.part_at["rudder"]
    edges = [(Vector(m.verts[f[i]]), Vector(m.verts[f[(i + 1) % len(f)]])) for f, fp in zip(m.faces, m.fpart) if fp == pi for i in range(len(f))]
    z0, z1 = min(min(a.z, b.z) for a, b in edges), max(max(a.z, b.z) for a, b in edges)

    def across(z):                                           # (her rudder's leading and trailing edge at this height)
        xs = [a.x + (b.x - a.x) * (z - a.z) / (b.z - a.z) for a, b in edges if (a.z - z) * (b.z - z) < 0.0]
        return max(xs), min(xs)

    zb, zt = z0 + 0.1 * (z1 - z0), z0 + 0.84 * (z1 - z0)
    (lead_b, trail_b), (lead_t, trail_t) = across(zb), across(zt)
    edge = 0.03
    quad = [[lead_b - edge, zb], [trail_b + edge, zb], [trail_t + edge, zt], [lead_t - edge, zt]]
    return {"tag": "*FIN", "place": "fin", "part": "rudder", "at": [round(sum(q[0] for q in quad) / 4.0, 3), 0.05, round(0.5 * (zb + zt), 3)],
            "radius": round(0.5 * (zt - zb), 3), "quad": [[round(q[0], 3), round(q[1], 3)] for q in quad], "facing": [0.0, 1.0, 0.0], "curve": 0.0, "both": True}


def convert(name, fine=True):
    G = game()
    s = G["PLANES"][name]
    if name in GLASS:
        s = dict(s, glass_bands=GLASS[name])
    PAINTS = G["PAINTS"]
    wing, tailw = G["Wing"](s["wing"]), G["Wing"](s["tail"])
    half = wing.tip
    fins = s.get("fins") or [(s["fin"], 0.0)]
    body = G["Body"](s["hull"], s.get("square", 2.0))
    fin_high = max(r[3] for r in fins[0][0]) - min(r[3] for r in fins[0][0])
    BATTLE.update(hull=max(0.15, (body.front - body.back) / 80.0), wing=max(0.24, half / 44.0), tail=max(0.22, tailw.tip / 14.0), fin=max(0.2, fin_high / 12.0))
    BATTLE.update(s.get("grain", {}))
    G["BATTLE_CHORDS"][True] = CHORDS_CLOSE if s.get("grain") else CHORDS
    parts, hull = G["build"](s, fine)
    paint_names = list(PAINTS)
    paints, colours = paints_of(G, s)
    role = ROLE.get(name, "fighter")
    m = Model(name, s.get("title", name), "aircraft", paints, fine)
    chords = G["BATTLE_CHORDS"][fine]

    # ---- where she breaks ------------------------------------------------------------------------
    by_kind = {}
    for ob in parts.objects:
        by_kind.setdefault(ob.name.split(".")[0], []).append(ob)
    root_mid = 0.5 * (wing.lead(wing.root) + wing.trail(wing.root))
    side = hull.w(-root_mid)
    rings_y = sorted({round(abs(v.co.y), 4) for ob in by_kind["wing"] for v in ob.data.vertices})
    y_cut = next((y for y in rings_y if y >= side + 0.12), rings_y[-2])
    rings_x = sorted({round(v.co.x, 4) for ob in by_kind["hull"] for v in ob.data.vertices})
    a, b = wing.trail(wing.root), tailw.lead(tailw.root)
    hood_back = min([min(r[0] for r in h[0]) for h in s.get("hoods", [])] + [a])
    want = TAIL_CUT.get(name, min(b + 0.38 * (a - b), hood_back - 0.2))
    ok = [x for x in rings_x if b + 0.15 < x < a - 0.1] or rings_x
    x_cut = min(ok, key=lambda x: abs(x - want))

    # ---- the airscrews ---------------------------------------------------------------------------
    hubs = []
    jets = s.get("jets", [])
    pods = sorted(((G["Body"](st, 2.0, out * hand), out * hand) for st, out in jets for hand in (1.0, -1.0)), key=lambda q: q[1])     # (port first)
    if not s.get("engines") and not jets:
        hub_x = sorted((r[0] for r in s["hull"]), reverse=True)[s.get("spinner", 2) - 1] + 0.06
        hubs.append(Vector((hub_x, 0.0, hull.z(-hub_x))))
    for (out, lead, z, is_radial, back) in s.get("engines", []):
        for hand in (1.0, -1.0):
            hubs.append(Vector((lead + (1.96 if is_radial else 2.16), out * hand, z)))
    hubs.sort(key=lambda h: h.y)                       # (the game's y is to the right: port first)
    screw = s.get("screw", 1.5 if not s.get("engines") else 1.7)

    def side_of(y):
        return "wing.port" if y < 0.0 else "wing.starboard"      # (the game's y)

    m.part("plane", None, (0, 0, 0), kind="root", note="the fuselage; the game flies her, the clips only tip and roll her")
    wing_z = wing.z(y_cut)
    for hand, nm in ((-1.0, "wing.port"), (1.0, "wing.starboard")):
        m.part(nm, "plane", port((root_mid, hand * y_cut, wing_z)), kind="free", note="breaks away at the root (fall_spin, fall_explode)")
    m.part("tail", "plane", (x_cut, 0.0, hull.z(-x_cut)), kind="free", note="the rear fuselage and tail; breaks away (fall_explode)")

    def paint_of(ob):
        own = int(ob["paint"])
        return own if own >= 0 or not len(ob.data.polygons) else ob.data.attributes["paint"].data[0].value
    has_bombs = any(paint_of(ob) == PAINTS["bomb"] for ob in by_kind.get("extra", []))
    bay = None
    if name in BAYS:
        near = lambda x: min(rings_x, key=lambda r: abs(r - x))
        bay = (near(BAYS[name][0]), near(BAYS[name][1]))
    if has_bombs or bay:
        m.part("bombs", "plane", (0, 0, 0), kind="slide", slide=(0, 0, -1), slide_limits=(0.0, 60.0), note="what she drops (bombs_away)")
    has_torpedo = any(paint_of(ob) == PAINTS["torpedo"] for ob in by_kind.get("extra", [])) or (bay is not None and name in BAY_TORPEDO)
    if has_torpedo:
        has_bombs = has_bombs or bay is not None
        m.part("torpedo", "plane", (0, 0, 0), kind="slide", slide=(0, 0, -1), slide_limits=(0.0, 60.0),
               note="her torpedo (torpedo_away)" + ("; she carries it or her bombs, never both: scale the other away (clips load.torpedo, load.bombs)" if has_bombs else ""))
    if bay:
        for nm, a, fore in (("bay.port", 270.0 - BAY_HALF, 1.0), ("bay.starboard", 270.0 + BAY_HALF, -1.0)):
            ends = [port(hull.at(x, math.radians(a))) for x in bay]
            m.part(nm, "plane", 0.5 * (ends[0] + ends[1]), axis=tuple(((ends[0] - ends[1]) * fore).normalized()), kind="turn", limits=(0, 85),
                   note="a door of her bomb bay, hinged along its outer edge; plus opens it")

    around = 40 if fine else 14
    waist = None
    if fine and name in WAIST:
        wx0, wx1, wa0, wa1 = WAIST[name]
        grid = 360.0 / around
        waist = (min(rings_x, key=lambda r: abs(r - wx0)), min(rings_x, key=lambda r: abs(r - wx1)), round(wa0 / grid) * grid, round(wa1 / grid) * grid)

    cockpit = fighters.opening(s, rings_x) if fine else None          # (a fighter's cockpit, open under her canopy: fighters.opening)

    def waist_hole(c, slack=0.0):
        """Whether a point of her side lies in an open window (either side)."""
        if not waist or not (waist[1] - slack < c.x < waist[0] + slack):
            return False
        a = math.degrees(math.atan2((c.z - hull.z(-c.x)) / hull.h(-c.x), abs(c.y) / hull.w(-c.x)))
        return waist[2] - 40.0 * slack - 0.5 < a < waist[3] + 40.0 * slack + 0.5

    panes = []
    if fine:
        grid = 360.0 / around
        for px0, px1, pa0, pa1 in PANES.get(name, []):
            panes.append((min(rings_x, key=lambda r: abs(r - px0)), min(rings_x, key=lambda r: abs(r - px1)), round(pa0 / grid) * grid, round(pa1 / grid) * grid))

    def pane_at(c, slack=0.0):
        """Whether a point of her skin lies in a pane of glass of a closed gun position (either side)."""
        for x0, x1, a0, a1 in panes:
            if x1 - slack < c.x < x0 + slack:
                a = math.degrees(math.atan2((c.z - hull.z(-c.x)) / hull.h(-c.x), abs(c.y) / hull.w(-c.x)))
                if a0 - 40.0 * slack - 0.5 < a < a1 + 40.0 * slack + 0.5:
                    return True
        return False

    def bay_door(c):
        """The door of her bay a point of her belly is on, if it is (fine model only: the plain one has no keel line to part them at)."""
        if not bay or not fine or not (bay[1] < c.x < bay[0]):
            return None
        a = math.degrees(math.atan2(c.z - hull.z(-c.x), c.y))
        return ("bay.starboard" if c.y > 0.0 else "bay.port") if abs(a + 90.0) < BAY_HALF else None
    prop_parent = []
    for k, h in enumerate(hubs):
        parent = side_of(h.y) if abs(h.y) > y_cut else "plane"
        prop_parent.append(parent)
        m.part(f"prop.{k + 1}", parent, port(h), axis=(1, 0, 0), kind="spin", note="the blades; plus turns them clockwise seen from the cockpit")
        m.part(f"prop.{k + 1}.blur", parent, port(h), axis=(1, 0, 0), kind="spin", note="the disc a fast-turning airscrew is seen as; hidden (scaled away) when it is stopped")

    # ---- the control surfaces --------------------------------------------------------------------
    def cut_at(share):
        return next(c for c in chords if c >= share - 1e-6)
    surf = {}                # its part -> (the skin it is cut from, that skin's curves, from, to, the share of the chord it starts at, its parent, hand)
    span = wing.tip - wing.root
    ail, flap = s.get("aileron", AILERON), s.get("flap", FLAP)
    for hand, nm in ((-1.0, "port"), (1.0, "starboard")):
        W = G["Wing"](s["wing"], hand)
        surf[f"aileron.{nm}"] = ("wing", W, wing.root + span * ail[0], wing.root + span * ail[1], cut_at(ail[2]), f"wing.{nm}", hand)
        if not s.get("engines") and not jets:
            surf[f"flap.{nm}"] = ("wing", W, max(wing.root + span * flap[0], y_cut), wing.root + span * flap[1], cut_at(flap[2]), f"wing.{nm}", hand)
        T = G["Wing"](s["tail"], hand)
        surf[f"elevator.{nm}"] = ("tail", T, T.root + (T.tip - T.root) * ELEVATOR[0], T.root + (T.tip - T.root) * ELEVATOR[1], cut_at(ELEVATOR[2]), "tail", hand)
    for fin_sections, fin_side in fins:
        for hand_side in ((fin_side, -fin_side) if fin_side else (0.0,)):
            F = G["Wing"](fin_sections, 1.0, True, hand_side)
            nm = "rudder" if not fin_side else ("rudder.starboard" if hand_side > 0 else "rudder.port")
            surf[nm] = ("fin", F, F.root + (F.tip - F.root) * RUDDER[0], F.tip + 1e-3, cut_at(RUDDER[2]), "tail", hand_side)
    SWING = {"aileron": (-25, 25), "flap": (0, 45), "elevator": (-28, 22), "rudder": (-30, 30)}
    NOTE = {"aileron": "plus is trailing edge down (a roll to starboard is port aileron plus, starboard minus)", "flap": "plus lowers it",
            "elevator": "plus is trailing edge down (nose down); minus pulls her nose up", "rudder": "plus is trailing edge to starboard (nose to starboard)"}
    for nm, (kind, W, u0, u1, hc, parent, hand) in surf.items():
        ends = []
        for u in (u0, min(u1, W.tip)):
            ends.append(port(0.5 * (W.at(u, hc, True) + W.at(u, hc, False))))
        ax = ends[1] - ends[0]
        if (ax.z < 0.0) if kind == "fin" else (ax.y > 0.0):
            ax = -ax
        m.part(nm, parent, 0.5 * (ends[0] + ends[1]), axis=tuple(ax.normalized()), kind="turn", limits=SWING[nm.split(".")[0]], note=NOTE[nm.split(".")[0]])

    def share(W, u, x):
        u = min(max(u, W.root), W.tip)
        lead, trail = W.lead(u), W.trail(u)
        return (x - lead) / (trail - lead) if abs(trail - lead) > 1e-9 else 0.0

    def on_surface(kind, c):
        """The control surface a face of a flying surface is part of, if it is."""
        for nm, (k, W, u0, u1, hc, parent, hand) in surf.items():
            if k != kind:
                continue
            if kind == "fin":
                if abs(c.y - hand) > 0.5:
                    continue
                u = c.z
            else:
                if (c.y > 0.0) != (hand > 0.0):
                    continue
                u = abs(c.y)
            if u0 + 1e-4 < u < u1 - 1e-4 and share(W, u, c.x) > hc + 1e-4:
                return nm
        return None

    # ---- gun turrets, and guns on free mountings -----------------------------------------------------
    domes = [(Vector(v[0]), Vector(v[1])) for kind, v in s.get("extras", []) if kind == "dome" or (kind == "ball" and v[0][1] == 0.0)]
    rods = []                                           # every gunner's gun, both hands: (its breech, its muzzle) in the game's axes
    for kind, v in s.get("extras", []):
        if kind == "rod" and v[4] == "steel":
            a_, b_ = Vector(v[0]), Vector(v[1])
            if abs(b_.y) > 0.3 and abs(b_.x - a_.x) < 0.2:
                continue
            for hand in ((1.0, -1.0) if (a_.y != 0.0 or b_.y != 0.0) else (1.0,)):
                rods.append((Vector((a_.x, a_.y * hand, a_.z)), Vector((b_.x, b_.y * hand, b_.z))))
    turrets, gun_part = [], {}                           # (number, middle, radii) of each turret; the part of each gun
    balls, remote = set(), set()                         # (the numbers of the turrets that are steel balls, and of those with nobody in them)
    windows = []
    for c, radii in domes:
        # (a blister on her side is fixed to her: a gun in it is on a free mounting, and the blister never trains)
        mine = [] if abs(c.y) > 1e-6 else [i for i, (a_, b_) in enumerate(rods) if (a_ - c).length < max(radii) + 0.5 and i not in gun_part]
        if not mine:
            windows.append((c, radii))                   # (a dome with no guns is a window: an astrodome, a blister a man looks out of)
            continue
        k = len(turrets) + 1
        turrets.append((k, c, radii))
        parent = "tail" if c.x < x_cut else "plane"
        at_end = c.x > hull.front - 1.2 or c.x < hull.back + 1.2
        belly = c.z < hull.z(-c.x) - 0.3 * hull.h(-c.x)
        # (a turret in her nose or her tail sweeps a little past each beam and points well below her: the air battles
        #  asked, 2026-11-18, since a tail gunner's work is the fighter climbing at him from below. The figures are
        #  the usual ones for a British nose or tail turret, from memory: a ruling in TURRET_LIMITS replaces them.)
        train, lift = TURRET_LIMITS.get(name, {}).get(k, (((-75, 75) if belly else (-95, 95)) if at_end else (-180, 180),
                                                          (-85, 10) if belly else (-45, 60) if at_end else (-10, 80)))
        m.part(f"turret.{k}", parent, port(c), axis=(0, 0, -1), kind="turn", limits=train,
               note="a gun turret: its dome, its frames, its gunner; plus trains it clockwise seen from above")
        way = sum((port(rods[i][1] - rods[i][0]) for i in mine), Vector())
        way.z = 0.0
        way = way.normalized() if way.length > 1e-6 else X.copy()
        if belly and name in fitout.BALL and min(radii) > 0.9 * max(radii):
            balls.add(k)                                    # (a round one under her belly is a steel ball, not a glass dome: see fitout.ball_turret)
        elif belly or name in REMOTE:
            remote.add(k)                                   # (a low one under her is worked from inside her: nobody sits in it)
        m.part(f"turret.{k}.guns", f"turret.{k}", port(c), axis=tuple(way.cross(Z).normalized()), kind="turn", limits=lift, note="its guns; plus lifts them")
        for i in mine:
            gun_part[i] = f"turret.{k}.guns"
    flex = []
    for i, (a_, b_) in enumerate(rods):
        if i not in gun_part:
            # (two guns side by side on the middle line are one mounting)
            twin = [j for j, (c_, d_) in enumerate(rods) if j not in gun_part and abs(a_.y) < 0.3 and abs(c_.x - a_.x) < 0.01 and abs(c_.z - a_.z) < 0.01 and abs(abs(c_.y) - abs(a_.y)) < 0.01]
            mine = twin or [i]
            base = sum((rods[j][0] for j in mine), Vector()) / len(mine)
            nm = f"flexgun.{len(flex) + 1}"
            flex.append((nm, base, sum((rods[j][1] for j in mine), Vector()) / len(mine)))
            m.part(nm, "tail" if base.x < x_cut else "plane", port(base), kind="free", note="a gun on a free mounting; it swings about its breech")
            for j in mine:
                gun_part[j] = nm

    def turret_at(p, slack=1.12):
        for k, c, radii in turrets:
            if sum(((p[j] - c[j]) / (radii[j] * slack)) ** 2 for j in range(3)) <= 1.0:
                return f"turret.{k}"
        return None

    def in_ball(p):
        return any(k in balls and sum(((p[j] - c[j]) / (radii[j] * 1.15)) ** 2 for j in range(3)) <= 1.0 for k, c, radii in turrets)

    def where(kind, c, mean, ob):
        if kind in ("dome", "frames", "glints") and in_ball(mean if kind == "dome" else c):
            return None                                     # (the game's glass dome there, its frames and its glints, are left out)
        if kind in ("wing", "tail", "fin"):
            on = on_surface(kind, c)
            if on:
                return on
        if kind in ("hull", "lines") and bay_door(c):
            return bay_door(c)
        if kind in ("hull", "lines") and waist_hole(c):
            return None                                     # (an open window: see fitout.waist)
        if kind in ("hull", "lines") and fighters.in_opening(cockpit, hull, c):
            return None                                     # (her cockpit, open under her canopy: see fitout.cockpit)
        if kind == "lines" and pane_at(c):
            return None                                     # (no panel line is scribed across a pane of glass)
        if kind == "hull":
            return "tail" if c.x < x_cut else "plane"
        if kind == "wing":
            if bay and bay[1] - 0.3 < c.x < bay[0] + 0.3 and abs(c.y) < 0.52 * hull.w(-c.x):
                return None                                 # (the wing runs through her fuselage: what of it would stand in her bay is left out)
            return side_of(c.y) if abs(c.y) > y_cut else "plane"
        if kind in ("tail", "fin", "tailwheel", "tailwheel_leg"):
            return "tail"
        if kind == "blade":
            k = min(range(len(hubs)), key=lambda i: (hubs[i] - Vector((hubs[i].x, mean.y, mean.z))).length)
            return f"prop.{k + 1}"
        if kind == "dome":
            return turret_at(mean, 0.5) or ("tail" if mean.x < x_cut else "plane")
        if kind in ("lines", "frames", "glints", "gills"):
            mean = c
            if kind == "frames" and turret_at(c):
                return turret_at(c)
        if kind == "extra":
            if has_bombs and paint_of(ob) == PAINTS["bomb"]:
                return "bombs"
            if has_torpedo and paint_of(ob) == PAINTS["torpedo"]:
                return "torpedo"
            for i, (a_, b_) in enumerate(rods):
                if (mean - 0.5 * (a_ + b_)).length < 0.03:
                    return gun_part[i]
            if turret_at(mean, 0.3):
                return turret_at(mean, 0.3)
        if kind in ("wire",):
            return "plane"
        if abs(mean.y) > y_cut and mean.x > x_cut:
            return side_of(mean.y)
        if mean.x < x_cut:
            return "tail"
        return "plane"

    # ---- every piece, face by face ---------------------------------------------------------------
    nose_bars, nose_from = s.get("nose_frames_paint"), (s["glazed"][1] - 0.06 if s.get("glazed") else 1.0e9)
    cowl_as_coat = paints["nose"] == paints["upper"]
    doped = fitout.nation(s) == "UK" and all(g[2] < 0.12 for g in s.get("guns", []))
    for ob in parts.objects:
        kind = ob.name.split(".")[0]
        if (kind == "gun" and doped) or kind == "wire":      # (her gun ports are doped over: see fitout.guns. Brian, 2026-11-18:
            bpy.data.objects.remove(ob, do_unlink=True)      #  "get rid of the wire from the back of the plane to the tail")
            continue
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bm.verts.ensure_lookup_table()
        if kind == "lines":
            # (every scribed line is a ribbon whose points come in pairs, one each side of it: drawn in to LINE of its width)
            vs = list(bm.verts)
            # (`lines` in her numbers: (how wide hers are against the usual, how far their colour is toward her paler
            #  upper colour). The joints her fuselage was built in, `hull_lines`, keep the usual width.)
            fine_line = LINE * s.get("lines", (1.0,))[0]
            joints = s.get("hull_lines", []) if s.get("lines") else []
            for i in range(0, len(vs) - 1, 2):
                mid = 0.5 * (vs[i].co + vs[i + 1].co)
                k = LINE if any(abs(mid.x - x) < 0.012 for x in joints) and abs(mid.y) < hull.w(-mid.x) + 0.03 else fine_line
                vs[i].co, vs[i + 1].co = mid + (vs[i].co - mid) * k, mid + (vs[i + 1].co - mid) * k
        bm.faces.ensure_lookup_table()
        layer = bm.faces.layers.int.get("paint")
        by_point = bm.verts.layers.int.get("coat")
        blend_l, soot_l = bm.verts.layers.float.get("blend"), bm.verts.layers.float.get("soot")
        own = int(ob["paint"])
        mean = sum((v.co for v in bm.verts), Vector()) / max(len(bm.verts), 1)
        per_face = kind in ("hull", "wing", "tail", "fin", "lines", "frames", "glints", "gills")
        whole = None if per_face else where(kind, mean, mean, ob)
        fpart = [whole or where(kind, f.calc_center_median(), mean, ob) for f in bm.faces]
        loose_ = surf.keys() | {"bay.port", "bay.starboard", None}     # (parts that are cut out clean, not torn off)
        verts, coat, blend, soot, index = [], [], [], [], {}
        out = {}                                             # part -> (faces, paints, by_point)

        def point(v, part):
            key = (v.index, part)
            if key not in index:
                index[key] = len(verts)
                verts.append(port(v.co))
                coat.append(paint_names[v[by_point]] if by_point is not None else None)
                blend.append(v[blend_l] if blend_l is not None else 0.0)
                soot.append(v[soot_l] if soot_l is not None else 0.0)
            return index[key]

        for f, part in zip(bm.faces, fpart):
            if part is None:
                continue
            of_face = own if own >= 0 else f[layer]
            glassy = kind == "hull" and bool(panes) and pane_at(f.calc_center_median())
            if kind == "frames" and nose_bars and f.calc_center_median().x > nose_from:
                of_face = PAINTS[nose_bars]                  # (the frames of her glass nose, in her skin's own finish)
            if kind == "frames" and s.get("frames_paint"):
                of_face = PAINTS[s["frames_paint"]]          # (every bar of her windows and turrets, painted as her skin is)
            if kind == "gills" and cowl_as_coat:             # (her cowl is painted with the rest of her: so are its gills, grey beneath)
                of_face = PAINTS["under"] if f.normal.z < -0.6 else PAINTS["upper"]
            if kind == "extra" and name in SPATS and own == PAINTS["under"] and mean.z < -0.8 and abs(mean.y) > 1.0:
                # (a fixed undercarriage in spats: the coat of her top on their outer faces, her under paint inside)
                of_face = PAINTS["upper"] if f.normal.y * (1.0 if mean.y > 0.0 else -1.0) > -0.25 else PAINTS["under"]
            corners = tuple(reversed([point(l.vert, part) for l in f.loops]))      # (mirrored, so turned round)
            faces, fp, fc = out.setdefault(part, ([], [], []))
            faces.append(corners)
            fp.append("glass" if glassy else paint_names[of_face] if of_face >= 0 else "upper")
            fc.append(of_face < 0 and not glassy)
        # (where a skin is cut between two parts, both torn ends are closed)
        caps = {}
        if kind in ("hull", "wing"):
            for e in bm.edges:
                if len(e.link_faces) == 2:
                    pa, pb = fpart[e.link_faces[0].index], fpart[e.link_faces[1].index]
                    if pa != pb and pa not in loose_ and pb not in loose_:
                        caps.setdefault(tuple(sorted((pa, pb))), []).append(e)
        base = len(m.verts)
        m.verts.extend(verts)
        m.vcoat.extend([m.paint_at[c] if c is not None else -1 for c in coat])
        m.vblend.extend(blend)
        m.vsoot.extend(soot)
        for part, (faces, fp, fc) in out.items():
            pi = m.part_at[part]
            for f, p, c in zip(faces, fp, fc):
                m.faces.append(tuple(base + i for i in f))
                m.fpaint.append(m.paint_at[p])
                m.fpart.append(pi)
                m.fsmooth.append(True)
                m.fcoat.append(c)
        for (pa, pb), edges in caps.items():
            ring = sorted({v.index for e in edges for v in e.verts})
            pts = [port(bm.verts[i].co) for i in ring]
            mid = sum(pts, Vector()) / len(pts)
            # (put the ring in order round its middle: it lies in one plane, across the wing or the fuselage)
            away = Y if kind == "wing" else X
            u = (pts[0] - mid).normalized()
            w = away.cross(u)
            order = sorted(range(len(pts)), key=lambda i: math.atan2((pts[i] - mid).dot(w), (pts[i] - mid).dot(u)))
            ring_pts = [pts[i] for i in order]
            n = len(ring_pts)
            for part, other in ((pa, pb), (pb, pa)):
                def mid_of(p):
                    fs = [f for f, q in zip(bm.faces, fpart) if q == p]
                    return port(sum((f.calc_center_median() for f in fs), Vector()) / len(fs))
                way = mid_of(other) - mid_of(part)
                m.use(part)
                m.add(ring_pts + [mid], [(k, (k + 1) % n, n) for k in range(n)], "torn", False, lambda q, way=way: way)
        bm.free()
        bpy.data.objects.remove(ob, do_unlink=True)

    # ---- the nose of each control surface, and the cove it turns in --------------------------------
    for nm, (kind, W, u0, u1, hc, parent, hand) in surf.items():
        if kind == "wing":
            us = battle_us(W, BATTLE["wing"] if fine else 1.2, tuple(flap[:2]) + tuple(ail[:2]))
        elif kind == "tail":
            us = battle_us(W, BATTLE["tail"] if fine else 1.0, ELEVATOR[:2])
        else:
            us = battle_us(W, BATTLE["fin"] if fine else 0.8, RUDDER[:1])
        us = [u for u in us if u0 - 2e-3 <= u <= u1 + 2e-3]
        if len(us) < 2:
            continue
        n = len(us)
        pts = [port(W.at(u, hc, True)) for u in us] + [port(W.at(u, hc, False)) for u in us]
        quads = [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)]
        m.use(nm)
        m.add(pts, quads, "under", False, lambda q: X)
        m.use(parent)
        m.add(pts, quads, "line", False, lambda q: -X)
        cs = [c for c in chords if c >= hc - 1e-6]
        for u, way in ((us[0], -1.0), (us[-1], 1.0)):
            if kind == "fin" and way > 0.0:
                continue                                  # (a rudder runs to the top of its fin)
            ring = [port(W.at(u, c, True)) for c in cs] + [port(W.at(u, c, False)) for c in reversed(cs[:-1])]
            mid = sum(ring, Vector()) / len(ring)
            away = (Z if kind == "fin" else Vector((0.0, -hand, 0.0))) * way
            fan = [(k, (k + 1) % len(ring), len(ring)) for k in range(len(ring))]
            m.use(nm)
            m.add(ring + [mid], fan, "line", False, lambda q, o=away: o)
            m.use(parent)
            m.add(ring + [mid], fan, "line", False, lambda q, o=away: -o)

    # ---- the disc of each airscrew ---------------------------------------------------------------
    # (The art director, 2026-11-18: one even disc "reads as a grey plastic plate" close up. So: a faint disc that
    #  leaves her spinner clear, over it the smear of each blade, a little denser, and, where her blades have yellow
    #  tips, a thin ring of yellow near its rim.)
    n_blades = int(s.get("blades", 3))
    navy = fitout.nation(s) == "JAP"                         # (bare metal blades with two red bands: see NAVY_AIRSCREW)
    tips = navy or fitout.nation(s) in ("UK", "USA")
    lo, hi = (NAVY_BANDS[0][0] - 0.01, NAVY_BANDS[1][1] + 0.01) if navy else (0.9, 1.0)
    for k, h in enumerate(hubs):
        m.use(f"prop.{k + 1}.blur")
        c = port(h) + X * 0.02
        steps = 36 if fine else 14

        def band(r0, r1, paint, dx=0.0, a0=0.0, a1=2.0 * math.pi, n=steps, c=c):
            rim = [[c + X * dx + Vector((0.0, math.cos(a0 + (a1 - a0) * i / n), math.sin(a0 + (a1 - a0) * i / n))) * (screw * r) for i in range(n + 1)] for r in (r0, r1)]
            m.add(rim[0] + rim[1], [(i, i + 1, n + 2 + i, n + 1 + i) for i in range(n)], paint, True, lambda q: X)
        band(0.13, 0.95 if tips and not navy else 1.0, "blur")
        if tips:
            # (the art director, 2026-11-18: one thin band was "a crisp gold wire" round a soft blur. Three bands, one
            #  inside the other and each faint, fade in and out as the blades' smear does)
            wide = hi - lo
            band(lo, hi, "blur_tip", dx=0.002)
            band(lo + 0.25 * wide, hi - 0.1 * wide, "blur_tip", dx=0.004)
            band(lo + 0.45 * wide, hi - 0.25 * wide, "blur_tip", dx=0.006)
            if not navy:
                band(0.95, 1.0, "blur")
        if fine:
            for b in range(n_blades):
                a = 2.0 * math.pi * b / n_blades + 0.4 + 0.7 * k
                band(0.15, 0.93, "blur", dx=0.004, a0=a - 0.3, a1=a + 0.3, n=6)
                band(0.15, 0.9, "blur", dx=0.008, a0=a - 0.13, a1=a + 0.13, n=4)

    # ---- sockets ---------------------------------------------------------------------------------
    for k, h in enumerate(hubs):
        m.socket(f"engine.{k + 1}", port(h) - X * 0.5, X, part=prop_parent[k], note="fire and smoke start here")
    for k, (pod, out) in enumerate(pods):
        part = side_of(out) if abs(out) > y_cut else "plane"
        m.socket(f"engine.{k + 1}", port((pod.front - 1.3, out, pod.z(-pod.front + 1.3))), X, part=part, note="fire and smoke start here")
        m.socket(f"jet.{k + 1}", port((pod.back - 0.3, out, pod.z(-pod.back))), -X, part=part, note="a jet's tailpipe: its heat haze, and at full power its thin dark trail")
    for (x, deg) in fitout.NOSE_CANNON.get(name, []):
        at = hull.at(x, math.radians(deg))
        m.socket(f"gun.{len([k for k in m.sockets if k['name'].startswith('gun.')]) + 1}", port(at) + X * 0.05, X, part="plane", note="a cannon's muzzle in her nose")
    n = 0
    for (out_, back, length, bore) in s.get("guns", []):
        for hand in (-1.0, 1.0):
            w = G["Wing"](s["wing"], hand)
            at = w.at(out_, 0.02, True)
            n += 1
            m.socket(f"gun.{n}", port((at.x + length, at.y, at.z - 0.03)), X, part=side_of(hand) if out_ > y_cut else "plane", note="a fixed gun's muzzle")
    if name in fitout.NOSE_GUNS:
        x = hubs[0].x - 0.5
        top = hull.at(x, math.radians(90.0))
        m.socket("gun.nose", (top.x, 0.0, top.z + 0.03), X, part="plane", note="the guns over the engine")
    for n, (a_, b_) in enumerate(rods):
        m.socket(f"turret.{n + 1}", port(b_), port(b_ - a_), part=gun_part[n], note="a gunner's muzzle")
    belly = hull.at(root_mid if not bay else 0.5 * (bay[0] + bay[1]), -math.pi / 2.0)
    m.socket("bombs", (belly.x, 0.0, belly.z - 0.1), -Z, part="plane", note="where her bombs leave")
    if has_torpedo and bay:
        m.socket("torpedo", (belly.x, 0.0, belly.z - 0.1), -Z, part="plane", note="where her torpedo leaves")
    elif has_torpedo:
        st = next(v[0] for kind, v in s.get("extras", []) if kind == "body" and v[3] == "torpedo")
        m.socket("torpedo", (0.5 * (st[0][0] + st[-1][0]), 0.0, st[len(st) // 2][1]), -Z, part="plane", note="where her torpedo leaves")
    belly = hull.at(root_mid, -math.pi / 2.0)
    hoods = s.get("hoods", [])
    if hoods:
        st = hoods[0][0]
        hx = 0.5 * (max(r[0] for r in st) + min(r[0] for r in st))
        hz = max(r[1] + r[3] for r in st)
        m.socket("hatch", (hx, 0.0, hz + 0.1), Z, part="plane", note="where her crew leave")
    else:
        top = hull.at(root_mid + 1.0, math.pi / 2.0)
        m.socket("hatch", (top.x, 0.0, top.z + 0.1), Z, part="plane", note="where her crew leave")
    m.socket("hatch.belly", (belly.x - 1.2, 0.0, hull.at(belly.x - 1.2, -math.pi / 2.0).z - 0.1), -Z, part="plane", note="a bomber's crew leave by the floor")
    for hand, nm in ((-1.0, "port"), (1.0, "starboard")):
        w = G["Wing"](s["wing"], hand)
        tip = w.at(half * 0.985, 0.5, True)
        m.socket(f"wingtip.{nm}", port(tip), -X, part=f"wing.{nm}", note="a trail from the tip in a hard turn")
        rootp = w.at(y_cut, 0.45, True)
        m.socket(f"break.wing.{nm}", port(rootp), port((0.0, -hand, 0.0)), part=f"wing.{nm}", note="the torn root of the wing: it burns and trails")
        m.socket(f"stub.{nm}", port(rootp), port((0.0, hand, 0.0)), part="plane", note="the stub left on the fuselage: fuel burns here")
        tank = w.at(max(y_cut + 0.6, half * 0.3), 0.4, True)
        m.socket(f"tank.{nm}", port(tank), Z, part=f"wing.{nm}", note="a wing tank: a fire in the wing starts here")
    m.socket("fuel", (root_mid, 0.0, hull.z(-root_mid)), Z, part="plane", note="the middle of her: where she blows up")
    m.socket("break.tail", (x_cut, 0.0, hull.z(-x_cut)), -X, part="tail", note="the torn end of the rear fuselage")
    m.socket("trail", (hull.back, 0.0, hull.z(-hull.back)), -X, part="tail", note="smoke streams from here when she is hit")

    # ---- what the game's file also carries -------------------------------------------------------
    nose_back = s.get("nose_back") if s.get("nose_back") is not None else hull.front - (hull.front - hull.back) * 0.22
    wide = hull.w(0.0)

    def weather(v, n, paint, part):
        w, y = 0.0, abs(v.y)
        skin = paint in ("upper", "upper2", "under", "nose")
        if not skin and not (pods and paint.startswith(("rivet", "seam"))):
            return 0.0, 0.0                                  # (only her paintwork wears; a jet's soot lies over her rivets too)
        if wide * 0.8 < y < wide + 0.9 and n[2] > 0.5 and abs(v.z) < 1.0:
            w = max(w, 0.70 * (1.0 - (y - wide * 0.8) / (0.9 + wide * 0.2)))
        if v.x > nose_back - 0.25:
            w = max(w, 0.38)
        if y > wide and n[0] > 0.75 and y < half:
            w = max(w, 0.45)
        # (seen close she is not one flat colour: the paint is a little uneven all over, the exhausts have drawn a long
        #  stain down each side, and oil has run back along her belly)
        g = Vector((v.x, -v.y, v.z))
        grain = noise.noise(Vector((v.x * 1.3 + 7.1, v.y * 1.3 - 3.3, v.z * 2.1 + 1.7)))
        soot = 0.09 * max(grain, 0.0)
        w = max(w, 0.22 * max(-grain - 0.25, 0.0))
        for stained, start, reach, a, *more in f.stains:     # (a stain may say how strong it is and how broad, against an exhaust's)
            strong, spread = (more[0] if more else 0.95), (more[1] if len(more) > 1 else 1.0)
            along = (start - g.x) / reach
            if -0.04 < along < 1.0 and abs(g.y - stained.side) < 2.5:
                away = (g - stained.at(g.x, a)).length
                broad = (0.17 + 0.34 * max(along, 0.0)) * spread
                soot = max(soot, strong * max(0.0, 1.0 - away / broad) ** 1.1 * min(1.0, (along + 0.04) * 10.0) * (1.0 - max(along, 0.0)) ** 0.7)
        # (a jet: her pipes have scorched the tail of each pod, and their smoke has drawn a soft grey stain back
        #  under her rear fuselage; in streaks, never an even band)
        streak = 0.75 + 0.5 * noise.noise(Vector((v.x * 0.35 + 3.0, v.y * 5.0, v.z * 5.0)))
        for pod, out in pods:
            back = (g.x - pod.back) / 1.7
            if -0.1 < back < 1.0 and math.hypot(g.y - pod.side, g.z - pod.z(-g.x)) < pod.w(-g.x) + 0.04:
                soot = max(soot, min(1.0, 1.25 * (1.0 - max(back, 0.0)) ** 1.3) * streak)
        if pods and n[2] < -0.2 and y < hull.w(-v.x) and v.x < s["wing"][0][2]:
            along = min(max((s["wing"][0][2] - v.x) / (s["wing"][0][2] - hull.back), 0.0), 1.0)
            soot = max(soot, 0.75 * min(1.0, along * 6.0) * (1.0 - along) ** 0.5 * max(0.0, 1.0 - y / hull.w(-v.x)) ** 0.6 * streak)
        if not s.get("engines") and not jets and n[2] < -0.5 and y < 0.5 * wide:
            along = (nose_back - v.x) / 4.5
            if 0.0 < along < 1.0:
                soot = max(soot, 0.4 * (1.0 - along) * (1.0 - y / (0.5 * wide)))
        return (w if skin else 0.0), soot
    m.weather = weather
    m.marks = G["marks_of"](s, hull)
    n_code = 1 if s.get("code") else 0                       # (where each is: her sides first, then her wings, her fin, her number)
    n_fin = len(s.get("fin_marks", []))
    for i, mk in enumerate(m.marks):
        mk["place"] = ("side" if i < len(s.get("side_marks", [])) else "code" if i >= len(m.marks) - n_code
                       else "fin" if i >= len(m.marks) - n_code - n_fin else "wing")
    if s.get("nation_paint") and "rudder" in m.part_at and any(mk["tag"] == "*" for mk in m.marks):
        m.marks.insert(len(m.marks) - n_code, rudder_flag(m))        # (a plain machine wears her owner's flag on her rudder)
    m.tally = tally_of(name, s, hull, role, G, fins)
    m.info.update({"rust": "9A9C98", "soot": colours["soot"], "wear_show": 0.3,
                   "file": {"nation_paint": bool(s.get("nation_paint")), "number_ink": colours["ink"], "span": round(2.0 * half, 3),
                            "role": role, "crew": CREW.get(name, 1), "engines": len(hubs) + len(pods), "jets": len(pods), "year": s.get("year"), "tech": s.get("tech"), "tag": s.get("tag"),
                            "clear": {"glass": 0.3, "blur": 0.07, "blur_tip": 0.035}, "tally": m.tally}})
    if navy:
        navy_airscrew(m)
    f = Facts()
    f.name, f.spec, f.hull, f.wing, f.half, f.y_cut, f.x_cut, f.hubs, f.screw = name, s, hull, wing, half, y_cut, x_cut, [port(h) for h in hubs], screw
    f.role, f.crew, f.has_bombs, f.long = role, CREW.get(name, 1), has_bombs or bool(bay), hull.front - hull.back
    f.has_torpedo = has_torpedo
    f.windows = [(port(c), radii) for c, radii in windows]
    if s.get("bare"):
        m.info["file"]["finish"] = "bare metal"             # (her skin is unpainted: it shines as metal does)
    later = schemes_of(G, name, s, paints, colours["ink"])
    if later:
        m.info["file"]["schemes"] = later                   # (her later coats of paint, by the month each comes in)
    family = next((MARK_FAMILY[mk["tag"]] for mk in (m.marks or []) if mk.get("tag") in MARK_FAMILY), None)
    if family:                                              # (her marking by month: swap each mark's tag for the last come)
        m.info["file"]["mark_months"] = [dict({"from": f[0]} if f[0] else {}, tag=f[1], **({"on": f[2]} if len(f) > 2 else {}))
                                         for f in MARK_MONTHS[family]]
    if has_torpedo:
        m.info["file"]["loads"] = (["torpedo", "bombs"] if has_bombs else ["torpedo"])
    f.bay, f.bay_half, f.rings_x = (bay if fine else None), BAY_HALF, rings_x
    f.bay_torpedo = BAY_TORPEDO.get(name) if bay else None
    f.prop_parent = prop_parent
    f.fins = fins
    f.engines, f.pods = len(hubs) + len(pods), pods
    f.year = s.get("year")                                   # (a machine that only comes late in the war)
    f.waist, f.around, f.waist_hole = waist, around, waist_hole
    f.cockpit = cockpit
    f.panes, f.pane_at, f.balls, f.remote = panes, pane_at, balls, remote
    f.surf = list(surf)
    f.turrets = [(k, port(c), radii) for k, c, radii in turrets]
    f.flex = [(nm, port(a_), port(b_)) for nm, a_, b_ in flex]
    f.guns_of = {k: [(port(rods[i][0]), port(rods[i][1])) for i in range(len(rods)) if gun_part[i] == f"turret.{k}.guns"] for k, c, radii in turrets}
    fitout.fit(m, f, s, G, hull)
    clips_for_plane(m, f)
    return m, f


# ---- the clips ----------------------------------------------------------------------------------------

def q_of(roll=0.0, pitch=0.0, yaw=0.0):
    """An attitude: yaw to port, then nose up, then a roll to starboard (degrees), in the model's axes."""
    return (Quaternion(Z, math.radians(yaw)) @ Quaternion(Y, math.radians(-pitch)) @ Quaternion(X, math.radians(roll)))


def rot_keys(f, long, step=0.1):
    """Keys of a turning over time: f(t) -> quaternion."""
    n = max(1, int(round(long / step)))
    out = []
    last = None
    for k in range(n + 1):
        t = long * k / n
        q = f(t)
        if last is not None and q.dot(last) < 0.0:
            q = -q
        last = q
        out.append([round(t, 3), q.w, q.x, q.y, q.z])
    return out


def move_keys(f, long, step=0.1):
    n = max(1, int(round(long / step)))
    return [[round(long * k / n, 3), *f(long * k / n)] for k in range(n + 1)]


def loose(m, clip, part, parent_q, world_q, world_move, long, step=0.1):
    """A part that has come away: it is still the fuselage's child, so what it does in the open air (turning as
    world_q(t), drifting as world_move(t) from where it was) is worked back through the fuselage's own turning."""
    P = m.parts[m.part_at[part]]
    root = m.parts[P.parent]

    def local(t):
        qb = parent_q(t)
        inv = qb.inverted()
        q = inv @ world_q(t)
        at = P.pivot + Vector(world_move(t))                              # where its pivot should be, in the root's frame
        mv = inv @ (at - root.pivot) + root.pivot - P.pivot
        return q, mv
    clip["tracks"].append({"part": part, "rot": rot_keys(lambda t: local(t)[0], long, step), "move": move_keys(lambda t: tuple(local(t)[1]), long, step)})


def props(m, f, how):
    """The tracks of the airscrews: 'fast' (the disc alone), 'slow' (blades alone, turning), 'dead' (blades alone, still)."""
    def tracks(long):
        out = []
        for k in range(len(f.hubs)):
            blades, disc = f"prop.{k + 1}", f"prop.{k + 1}.blur"
            if how == "fast":
                out.append({"part": blades, "scale": [[0.0, 0.02]]})
                out.append({"part": disc, "turn": [[0.0, 0.0], [long, 360.0 * max(1, round(long * 1.5))]]})
            elif how == "slow":
                out.append({"part": blades, "turn": [[0.0, 0.0], [long, 360.0 * max(1, round(long * 1.2))]]})
                out.append({"part": disc, "scale": [[0.0, 0.001]]})
            else:
                out.append({"part": disc, "scale": [[0.0, 0.001]]})
        return out
    return tracks


def clips_for_plane(m, f):
    C = m.clips
    eng = [f"engine.{k + 1}" for k in range(f.engines)]
    big = f.half > 9.0
    rudders = [n for n in f.surf if n.startswith("rudder")]

    def stick(roll=None, pitch=None, yaw=None):
        """What her pilot does with the controls, as tracks of her control surfaces. Each is a list of [t, degrees] keys,
        signed as q_of: roll plus is to starboard, pitch plus is nose up, yaw plus is to port."""
        out = []
        if roll:
            out.append({"part": "aileron.port", "turn": [[t, d] for t, d in roll]})
            out.append({"part": "aileron.starboard", "turn": [[t, -d] for t, d in roll]})
        if pitch:
            out += [{"part": nm, "turn": [[t, -d] for t, d in pitch]} for nm in ("elevator.port", "elevator.starboard")]
        if yaw:
            out += [{"part": nm, "turn": [[t, -d] for t, d in yaw]} for nm in rudders]
        return out
    # -- flying --
    long = 4.0
    # (the sound director's real loops, by the kind of machine)
    engine_cue = "jet_engine_loop" if f.pods else ("b17_drone_loop" if f.name == "B17" else "b24_drone_loop" if len(eng) >= 4 else "bomber_medium_loop" if f.role == "bomber"
                  else "stuka_drone_loop" if f.name == "Stuka" else "fighter_engine_loop")       # (the jet's own loop is on the sound director's board now, for Brian's ear)
    C["fly"] = {"long": long, "loop": True, "about": "Under way: every airscrew a turning disc, and she rides the air a little. The game flies her; this only lives on top.",
                "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=(0.6 if big else 1.4) * math.sin(2 * math.pi * t / long), pitch=(0.25 if big else 0.5) * math.sin(4 * math.pi * t / long + 0.7)), long, 0.25)}]
                + props(m, f, "fast")(long) + stick(roll=[[0.0, 1.2], [1.0, 0.0], [2.0, -1.2], [3.0, 0.0], [4.0, 1.2]], pitch=[[0.0, 0.6], [1.0, 0.0], [2.0, -0.6], [3.0, 0.0], [4.0, 0.6]]),
                "events": [{"t": 0.0, "fx": "engine_loop", "socket": eng[0], "cue": engine_cue}]}
    C["idle"] = {"long": 2.5, "loop": True, "about": "Engines ticking over on the ground or the deck: the blades are seen going round.",
                 "tracks": props(m, f, "slow")(2.5), "events": [{"t": 0.0, "fx": "engine_loop", "socket": eng[0], "cue": "aero_engine_idle_loop"}]}
    C["stopped"] = {"long": 0.1, "loop": False, "after": "hold", "about": "Engines stopped: blades still, no disc. For a parked machine.", "tracks": props(m, f, "dead")(0.1), "events": []}
    for nm, sign in (("bank.port", -1.0), ("bank.starboard", 1.0)):
        deg = 28.0 if big else 62.0
        L, over = (1.6, 10.0) if big else (0.9, 15.0)
        C[nm] = {"long": L, "loop": False, "after": "hold", "about": "Rolls into a turn and holds it, her ailerons over as she rolls and her elevators up as she pulls round "
                 "(lay it over `fly`; play it backwards to roll out).",
                 "tracks": [{"part": "plane", "rot": rot_keys(lambda t, s_=sign, d=deg, L=L: q_of(roll=s_ * d * smooth(t / L)), L, 0.1)}]
                 + stick(roll=[[0.0, 0.0], [0.25 * L, over * sign], [0.7 * L, over * sign], [L, 1.5 * sign]], pitch=[[0.0, 0.0], [L, 7.0]],
                         yaw=[[0.0, 0.0], [0.5 * L, -6.0 * sign], [L, -2.0 * sign]]), "events": []}
    if not big:
        C["peel_off"] = {"long": 1.8, "loop": False, "after": "hold", "about": "Half-rolls and pulls down into a dive (the game then flies her down).",
                         "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=-150.0 * smooth(t / 1.1), pitch=-55.0 * smooth((t - 0.5) / 1.3)), 1.8, 0.1)}]
                         + stick(roll=[[0.0, 0.0], [0.2, -18.0], [0.9, -18.0], [1.2, 0.0]], pitch=[[0.0, 0.0], [0.5, 0.0], [1.0, 16.0], [1.8, 10.0]]), "events": []}
    if "flap.port" in f.surf:
        C["flaps.down"] = {"long": 1.5, "loop": False, "after": "hold", "about": "Her flaps come down, for a landing or a take-off from a deck (play it backwards to raise them).",
                           "tracks": [{"part": nm, "turn": [[0.0, 0.0], [1.5, 35.0]]} for nm in ("flap.port", "flap.starboard")], "events": []}
    # -- shooting --
    guns = [s_["name"] for s_ in m.sockets if s_["name"].startswith("gun.")]
    if guns:
        C["fire"] = {"long": 0.5, "loop": True, "about": "Her fixed guns firing: a flicker at each muzzle, and she shakes with it.",
                     "tracks": [{"part": "plane", "move": [[0.0, 0, 0, 0], [0.06, -0.012, 0, 0.004], [0.12, 0, 0, 0], [0.18, -0.012, 0, -0.004], [0.25, 0, 0, 0], [0.31, -0.012, 0, 0.004], [0.37, 0, 0, 0], [0.43, -0.012, 0, -0.004], [0.5, 0, 0, 0]]}],
                     "events": [{"t": round(0.08 * k, 2), "fx": "muzzle_flash_small", "socket": g, "cue": "plane_guns" if k == 0 else ""} for k in range(6) for g in guns]}
    turrets = [s_["name"] for s_ in m.sockets if s_["name"].startswith("turret.")]
    if turrets:
        swing = []
        for k, c, radii in f.turrets:
            swing.append({"part": f"turret.{k}", "turn": [[0.0, 0.0], [0.15, 4.0], [0.3, 0.0], [0.45, -4.0], [0.6, 0.0]]})
            swing.append({"part": f"turret.{k}.guns", "turn": [[0.0, 0.0], [0.3, 2.5], [0.6, 0.0]]})
        for nm, a, b in f.flex:
            swing.append({"part": nm, "rot": rot_keys(lambda t: q_of(yaw=4.0 * math.sin(2 * math.pi * t / 0.6), pitch=2.0 * math.sin(4 * math.pi * t / 0.6)), 0.6, 0.075)})
        C["fire.turrets"] = {"long": 0.6, "loop": True, "about": "Her gunners firing back: a flicker at each gunner's muzzle, each turret and gun working a little about its aim "
                             "(the game trains a turret at its target with the part's own axis and limits; this lies on top).", "tracks": swing,
                             "events": [{"t": round(0.1 * k + 0.03 * i, 2), "fx": "muzzle_flash_small", "socket": g, "cue": "gunner_burst" if k == 0 and i == 0 else ""} for k in range(5) for i, g in enumerate(turrets)]}
    if f.turrets or f.flex:
        long = 8.0
        swing = []
        for i, (k, c, radii) in enumerate(f.turrets):
            lim = m.parts[m.part_at[f"turret.{k}"]].limits
            reach = 0.55 * min(lim[1], 80.0)
            swing.append({"part": f"turret.{k}", "turn": [[round(long * j / 16.0, 3), round(reach * math.sin(2 * math.pi * j / 16.0 + 1.3 * i), 2)] for j in range(17)]})
            swing.append({"part": f"turret.{k}.guns", "turn": [[round(long * j / 8.0, 3), round(6.0 + 6.0 * math.sin(2 * math.pi * j / 8.0 + 0.7 * i), 2)] for j in range(9)]})
        for i, (nm, a, b) in enumerate(f.flex):
            swing.append({"part": nm, "rot": rot_keys(lambda t, i=i: q_of(yaw=14.0 * math.sin(2 * math.pi * t / long + 1.1 * i), pitch=6.0 * math.sin(4 * math.pi * t / long + 0.5 * i)), long, 0.25)})
        C["watch"] = {"long": long, "loop": True, "about": "Her gunners keeping watch: every turret and gun sweeping slowly across its piece of sky (lay it over `fly`).",
                      "tracks": swing, "events": []}
    if f.has_torpedo:
        in_bay = f.bay_torpedo is not None and "bay.port" in m.part_at
        t0 = 0.9 if in_bay else 0.0
        C["torpedo_away"] = {"long": t0 + 1.5, "loop": False, "after": "hold",
                             "about": "Her torpedo leaves: " + ("the doors of her bay swing open, it" if in_bay else "it") + " drops clear and is gone, and she lifts as its weight comes off (the game's own torpedo is spawned at `torpedo` and takes over).",
                             "tracks": ([{"part": nm, "turn": [[0.0, 0.0], [0.8, 80.0]]} for nm in ("bay.port", "bay.starboard")] if in_bay else []) +
                                       [{"part": "torpedo", "slide": [[0.0, 0.0], [t0, 0.0], [t0 + 0.5, 1.2], [t0 + 1.0, 4.9], [t0 + 1.5, 11.0]], "scale": [[0.0, 1.0], [t0 + 1.4, 1.0], [t0 + 1.5, 0.001]]},
                                        {"part": "plane", "move": [[0.0, 0, 0, 0], [t0, 0, 0, 0], [t0 + 0.4, 0, 0, 0.3], [t0 + 1.5, 0, 0, 0]]}],
                             "events": ([{"t": 0.0, "fx": "", "socket": "torpedo", "cue": "bomb_doors"}] if in_bay else []) + [{"t": t0, "fx": "torpedo_away", "socket": "torpedo", "cue": "torpedo_launch"}]}
        C["load.torpedo"] = {"long": 0.0, "loop": False, "after": "hold", "about": "She carries her torpedo: her bombs are not there (lay it over everything else).",
                             "tracks": [{"part": "bombs", "scale": [[0.0, 0.001]]}] if f.has_bombs else [], "events": []}
        if f.has_bombs:
            C["load.bombs"] = {"long": 0.0, "loop": False, "after": "hold", "about": "She carries her bombs for bombing from high up: her torpedo is not there.",
                               "tracks": [{"part": "torpedo", "scale": [[0.0, 0.001]]}], "events": []}
    if f.has_bombs or f.role in ("bomber", "dive bomber", "attack", "torpedo bomber"):
        tracks = []
        doors = "bay.port" in m.part_at
        t0 = 0.9 if doors else 0.0                         # (a bay's doors open first)
        if doors:
            tracks += [{"part": nm, "turn": [[0.0, 0.0], [0.8, 80.0]]} for nm in ("bay.port", "bay.starboard")]
        if f.has_bombs:
            tracks.append({"part": "bombs", "slide": [[0.0, 0.0], [t0, 0.0], [t0 + 0.5, 1.2], [t0 + 1.0, 4.9], [t0 + 1.5, 11.0]], "scale": [[0.0, 1.0], [t0 + 1.4, 1.0], [t0 + 1.5, 0.001]]})
        C["bombs_away"] = {"long": t0 + 1.5, "loop": False, "after": "hold", "about": "Her bombs leave: " + ("the doors of her bay swing open, her bombs drop out of it and are gone" if doors else
                           "what hangs under her drops clear and is gone" if f.has_bombs else "from under her") + " (the game's falling bombs are spawned at `bombs` and take over).",
                           "tracks": tracks + [{"part": "plane", "move": [[0.0, 0, 0, 0], [t0, 0, 0, 0], [t0 + 0.4, 0, 0, 0.25 if not big else 0.12], [t0 + 1.5, 0, 0, 0]]}],
                           "events": ([{"t": 0.0, "fx": "", "socket": "bombs", "cue": "bomb_doors"}] if doors else []) + [{"t": t0, "fx": "bombs_away", "socket": "bombs", "cue": "bomb_release"}]}
        if doors:
            C["doors.close"] = {"long": 0.9, "loop": False, "after": "hold", "about": "The doors of her bay swing shut on an empty bay.",
                                "tracks": [{"part": nm, "turn": [[0.0, 80.0], [0.9, 0.0]]} for nm in ("bay.port", "bay.starboard")] + [{"part": "bombs", "scale": [[0.0, 0.001]]}] +
                                          ([{"part": "torpedo", "scale": [[0.0, 0.001]]}] if f.has_torpedo else []),
                                "events": [{"t": 0.0, "fx": "", "socket": "bombs", "cue": "bomb_doors"}]}
    # -- being hit --
    C["hit"] = {"long": 0.7, "loop": False, "about": "Struck: she staggers and a puff of smoke and scraps flies from her.",
                "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=9.0 * math.sin(t * 18.0) * (1 - t / 0.7), pitch=-3.0 * math.sin(t * 9.0) * (1 - t / 0.7)), 0.7, 0.05)}],
                "events": [{"t": 0.0, "fx": "hit_flash", "socket": "fuel", "cue": "ricochet_01"}, {"t": 0.02, "fx": "debris_small", "socket": "fuel", "cue": ""}]}
    C["smoking"] = {"long": 2.0, "loop": True, "about": "Damaged but flying: a thin grey stream from an engine.", "tracks": [],
                    "events": [{"t": 0.0, "fx": "smoke_trail_thin", "socket": eng[-1], "cue": ""}]}
    # -- the three ways down (ruling 217: a loss is one model falling) --
    long = 9.0
    C["fall_burn"] = {"long": long, "loop": False, "after": "hold", "about": "Shot down burning: an engine alight, she drops a wing and goes down in a steepening dive, trailing fire and black smoke. "
                      "The game carries her down; this is her attitude, her fire and her smoke. Play `bail_out` over it if her crew get out.",
                      "tracks": [{"part": "plane", "rot": rot_keys(lambda t: q_of(roll=-(18.0 * smooth(t / 2.0) + 50.0 * (t / long) ** 2 * (1.0 if not big else 0.6)), pitch=-(8.0 * smooth(t / 1.5) + 52.0 * smooth((t - 1.0) / 6.0)), yaw=10.0 * smooth(t / long)), long, 0.2)}]
                      + props(m, f, "slow")(long)[:2] + props(m, f, "fast")(long)[2:] + stick(roll=[[0.0, 0.0], [2.0, 9.0], [long, 4.0]], pitch=[[0.0, 0.0], [3.0, 6.0], [long, -8.0]]),
                      "events": [{"t": 0.0, "fx": "engine_fire", "socket": eng[0], "cue": "fire_small_loop"}, {"t": 0.0, "fx": "smoke_trail_black", "socket": eng[0], "cue": ""},
                                 {"t": 2.5, "fx": "fire", "socket": "stub.port" if len(eng) == 1 else "tank.port", "cue": ""}, {"t": 0.0, "fx": "engine_dying", "socket": eng[0], "cue": "plane_down"}]}
    # (a wing gone: she rolls hard toward the missing wing and whirls down; the wing flutters away behind)
    long = 8.0
    spin_q = lambda t: q_of(yaw=-70.0 * t * smooth(t / 1.5)) @ q_of(pitch=-72.0 * smooth(t / 1.8)) @ q_of(roll=-(200.0 if not big else 110.0) * t * smooth(t / 1.0))
    clip = {"long": long, "loop": False, "after": "hold", "about": "A wing shot off: the port wing breaks away at the root, she whirls down out of control, the wing fluttering down behind her. "
            "The game carries her down.",
            "tracks": [{"part": "plane", "rot": rot_keys(spin_q, long, 0.05)}] + props(m, f, "slow")(long)
            + stick(roll=[[0.0, 0.0], [0.5, 20.0]], pitch=[[0.0, 0.0], [0.6, 24.0]], yaw=[[0.0, 0.0], [0.5, -26.0]]),
            "events": [{"t": 0.0, "fx": "hit_flash", "socket": "break.wing.port", "cue": "aircraft_break_up"}, {"t": 0.05, "fx": "fire", "socket": "stub.port", "cue": ""},
                       {"t": 0.05, "fx": "smoke_trail_black", "socket": "stub.port", "cue": "plane_down"}, {"t": 0.1, "fx": "smoke_trail_thin", "socket": "break.wing.port", "cue": ""}]}
    drag = (26.0 if not big else 18.0)
    loose(m, clip, "wing.port", spin_q,
          lambda t: q_of(roll=40.0 * t + 260.0 * t * smooth(t / 2.0), pitch=25.0 * math.sin(t * 2.2), yaw=-95.0 * t),
          lambda t: (-drag * t - 3.0 * t * t, 1.5 * t + 4.0 * math.sin(t * 1.3), 5.0 * t + 2.2 * t * t), long, 0.05)
    C["fall_spin"] = clip
    # (blown apart: both wings and the tail fly off; the pieces tumble and fall away from each other)
    long = 6.0
    body_q = lambda t: q_of(pitch=-80.0 * smooth(t / 2.5), roll=140.0 * t, yaw=25.0 * t)
    clip = {"long": long, "loop": False, "after": "gone", "about": "Blown up: a burst of fire amidships, both wings and the tail fly off and the pieces tumble away, burning. "
            "The game carries the wreck down and takes it away at the end.",
            "tracks": [{"part": "plane", "rot": rot_keys(body_q, long, 0.05)}] + props(m, f, "dead")(long),
            "events": [{"t": 0.0, "fx": "explode", "socket": "fuel", "cue": "plane_explode"}, {"t": 0.0, "fx": "debris", "socket": "fuel", "cue": ""},
                       {"t": 0.1, "fx": "fire", "socket": "fuel", "cue": ""}, {"t": 0.1, "fx": "smoke_trail_black", "socket": "fuel", "cue": ""},
                       {"t": 0.15, "fx": "fire", "socket": "tank.starboard", "cue": ""}, {"t": 0.15, "fx": "smoke_trail_thin", "socket": "break.wing.port", "cue": ""},
                       {"t": 0.15, "fx": "smoke_trail_thin", "socket": "break.tail", "cue": ""}]}
    k = 1.0 if not big else 0.7
    loose(m, clip, "wing.port", body_q, lambda t: q_of(roll=-330.0 * t * k, pitch=30.0 * t, yaw=-120.0 * t * k),
          lambda t: (-22.0 * t - 2.0 * t * t, 9.0 * t * math.exp(-t / 3.0) + 3.0 * t, 6.0 * t + 2.0 * t * t), long, 0.05)
    loose(m, clip, "wing.starboard", body_q, lambda t: q_of(roll=280.0 * t * k, pitch=-50.0 * t, yaw=150.0 * t * k),
          lambda t: (-17.0 * t - 2.5 * t * t, -11.0 * t * math.exp(-t / 3.0) - 2.0 * t, 3.0 * t + 2.6 * t * t), long, 0.05)
    loose(m, clip, "tail", body_q, lambda t: q_of(pitch=420.0 * t * k, yaw=60.0 * t, roll=45.0 * t),
          lambda t: (-30.0 * t - 1.5 * t * t, 2.0 * t, 9.0 * t + 1.5 * t * t), long, 0.05)
    C["fall_explode"] = clip
    # -- her crew --
    crew = min(f.crew, 5)
    C["bail_out"] = {"long": 0.8 + 0.7 * crew, "loop": False, "about": f"Her crew take to their parachutes: {crew} leave one after another (the game spawns the Parachute model at each `chute` event).",
                     "tracks": [], "events": [{"t": round(0.5 + 0.7 * k, 2), "fx": "chute", "socket": "hatch" if (k % 2 == 0 or not big) else "hatch.belly", "cue": ""} for k in range(crew)]}


# ---- the parachute ------------------------------------------------------------------------------------

def parachute(fine=True):
    """An airman under his parachute. The root is his harness (where the lines meet); the canopy opens above it."""
    m = Model("Parachute", "Parachute and airman (our own model)", "parachute",
              {"silk": "E8E5D8", "silk2": "D5D1C2", "line": "CFCBBB", "suit": "5B5A48", "skin": "C59B7C", "boots": "2B2622", "harness": "8D8462", "helmet": "4A3A2A"}, fine)
    m.part("chute", None, (0, 0, 0), kind="root", note="the harness point, where the lines meet; the game lets it down")
    m.part("canopy", "chute", (0, 0, 0), kind="free", note="canopy and lines; scaled from a streamer to full as it opens")
    m.part("airman", "chute", (0, 0, 0), axis=(0, 1, 0), kind="turn", limits=(-25, 25), note="he swings under it")
    R, top, skirt = 3.4, 7.6, 5.4                      # the canopy's radius, the height of its crown and of its skirt
    gores = 20 if fine else 10
    m.use("canopy")
    rows = 7 if fine else 4
    rings = []
    for j in range(rows + 1):
        a = (math.pi / 2.0) * (1.0 - j / rows) * 0.96 + 0.04      # from near the crown (a vent) down to the skirt
        ring = []
        for k in range(gores * 2):
            ang = 2 * math.pi * k / (gores * 2)
            puff = 1.0 + (0.045 if k % 2 else -0.02) * math.sin(math.pi * j / rows) + (0.03 if k % 2 else -0.03) * (j / rows) ** 3
            r = R * math.cos(a) * puff
            ring.append(Vector((r * math.cos(ang), r * math.sin(ang), skirt + (top - skirt) * math.sin(a))))
        rings.append(ring)
    n = gores * 2
    verts = [p for ring in rings for p in ring]
    faces = [(r * n + k, r * n + (k + 1) % n, (r + 1) * n + (k + 1) % n, (r + 1) * n + k) for r in range(rows) for k in range(n)]
    mid = Vector((0, 0, skirt + 0.5))
    m.add(verts, faces, lambda c, nn: "silk" if int((math.atan2(c.y, c.x) % (2 * math.pi)) / (2 * math.pi / gores)) % 2 == 0 else "silk2", True, lambda c: c - mid)
    for k in range(gores):
        ang = 2 * math.pi * (k + 0.0) / gores
        foot = rings[-1][(2 * k) % n]
        if fine or k % 2 == 0:
            m.rod(foot, Vector((0.16 * math.cos(ang), 0.16 * math.sin(ang), 0.25)), 0.012, "line", sides=3, caps=(False, False))
    # the airman: hanging in his harness, arms up to the risers
    m.use("airman")
    m.ball((0, 0, -0.42), (0.17, 0.2, 0.33), "suit", 10, 6)                    # body
    m.ball((0, 0, -0.02), (0.105, 0.1, 0.12), "helmet", 10, 6)                 # head
    m.ball((0.07, 0, -0.03), (0.06, 0.075, 0.085), "skin", 8, 5)               # face
    for sgn in (1.0, -1.0):
        m.rod((0, 0.2 * sgn, -0.2), (0.04, 0.2 * sgn, 0.24), 0.05, "suit", 0.04, sides=6)       # arm up
        m.rod((0.04, 0.2 * sgn, 0.24), (0.02, 0.17 * sgn, 0.27), 0.04, "skin", sides=5)
        m.rod((0.0, 0.1 * sgn, -0.7), (0.12, 0.11 * sgn, -1.12), 0.075, "suit", 0.06, sides=6)   # thigh
        m.rod((0.12, 0.11 * sgn, -1.12), (0.02, 0.11 * sgn, -1.52), 0.06, "suit", 0.05, sides=6)  # shin
        m.rod((0.02, 0.11 * sgn, -1.52), (0.16, 0.11 * sgn, -1.58), 0.05, "boots", sides=5)       # boot
        m.rod((0.0, 0.17 * sgn, -0.25), (0.0, 0.16 * sgn, 0.25), 0.014, "harness", sides=3)       # riser
    m.box((-0.17, 0, -0.38), (0.12, 0.3, 0.4), "harness")                       # the pack on his back
    m.socket("feet", (0.05, 0, -1.6), -Z, part="airman", note="where he touches down")
    m.socket("crown", (0, 0, top), Z, part="canopy")
    C = m.clips
    C["open"] = {"long": 1.8, "loop": False, "after": "hold", "about": "The parachute streams, fills and steadies. Start it as he leaves the machine.",
                 "tracks": [{"part": "canopy", "scale": [[0.0, 0.05, 0.05, 0.55], [0.5, 0.1, 0.1, 1.25], [0.9, 0.55, 0.55, 1.12], [1.15, 1.1, 1.1, 0.93], [1.45, 0.96, 0.96, 1.03], [1.8, 1.0, 1.0, 1.0]]},
                            {"part": "airman", "turn": [[0.0, 22.0], [0.9, 16.0], [1.25, -12.0], [1.8, 5.0]]}],
                 "events": [{"t": 1.0, "fx": "chute_crack", "socket": "crown", "cue": "parachute_open"}]}
    C["drift"] = {"long": 5.0, "loop": True, "about": "Coming down: he swings gently and the canopy breathes. The game lets him down at about 6 m a second, drifting with the wind.",
                  "tracks": [{"part": "airman", "turn": [[0.0, 5.0], [1.25, 0.0], [2.5, -5.0], [3.75, 0.0], [5.0, 5.0]]},
                             {"part": "canopy", "scale": [[0.0, 1.0, 1.0, 1.0], [1.25, 1.02, 1.02, 0.985], [2.5, 1.0, 1.0, 1.0], [3.75, 0.985, 0.985, 1.012], [5.0, 1.0, 1.0, 1.0]]},
                             {"part": "chute", "rot": rot_keys(lambda t: q_of(roll=3.0 * math.sin(2 * math.pi * t / 5.0), pitch=2.0 * math.sin(2 * math.pi * t / 5.0 + 1.3)), 5.0, 0.25)}],
                  "events": []}
    C["collapse"] = {"long": 2.5, "loop": False, "after": "hold", "about": "Down: the canopy spills and settles on the sea or the ground.",
                     "tracks": [{"part": "canopy", "scale": [[0.0, 1.0, 1.0, 1.0], [0.8, 1.15, 1.2, 0.5], [2.5, 1.3, 1.35, 0.06]], "move": [[0.0, 0, 0, 0], [2.5, 2.5, 0.6, -1.2]]}],
                     "events": [{"t": 0.0, "fx": "splash_small", "socket": "feet", "cue": ""}]}
    return m


# ---- the bomb -----------------------------------------------------------------------------------------

def bomb(fine=True):
    """A general-purpose bomb as it falls: a 250 kg case with its tail fins in a ring."""
    m = Model("Bomb", "Bomb (our own model)", "bomb", {"case": "3A3D38", "band": "A0803A", "fin": "2E302C"}, fine)
    m.part("bomb", None, (0, 0, 0), kind="root", note="the game drops it; the clip tips its nose down")
    sides = 14 if fine else 8
    prof = [(0.82, 0.0), (0.74, 0.07), (0.6, 0.14), (0.4, 0.175), (-0.1, 0.18), (-0.45, 0.13), (-0.8, 0.05)]
    rings = [[Vector((x, r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides))) for k in range(sides)] for x, r in prof]
    m.skin(rings, lambda c, n: "band" if 0.42 < c.x < 0.56 else "case", caps=(True, True))
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        d = Vector((0, math.cos(a), math.sin(a)))
        m.add([Vector((-0.42, 0, 0)) + d * 0.1, Vector((-0.86, 0, 0)) + d * 0.04, Vector((-0.86, 0, 0)) + d * 0.2, Vector((-0.6, 0, 0)) + d * 0.2], [(0, 1, 2, 3)], "fin", False)
    ring = [[Vector((x, 0.2 * math.cos(2 * math.pi * k / sides), 0.2 * math.sin(2 * math.pi * k / sides))) for k in range(sides)] for x in (-0.7, -0.86)]
    m.skin(ring, "fin")
    m.socket("nose", (0.82, 0, 0), X, note="it bursts here")
    m.clips["fall"] = {"long": 6.0, "loop": False, "after": "hold", "about": "Released level, its nose drops as it falls (the game carries it forward and down).",
                       "tracks": [{"part": "bomb", "rot": rot_keys(lambda t: q_of(pitch=-78.0 * smooth(t / 5.0), roll=40.0 * t), 6.0, 0.25)}], "events": []}
    return m


def torpedo(fine=True):
    """An air-dropped torpedo as it falls and runs: 5.3 m of steel, its two screws, its rudders, and the box of wooden
    fins on its tail that held it steady in the air and broke off when it struck the water."""
    m = Model("Torpedo", "Aerial torpedo (our own model)", "torpedo", {"case": "6C7174", "head": "4A4D50", "screw": "8C7A46", "wood": "8B7350"}, fine)
    m.part("torpedo", None, (0, 0, 0), kind="root", note="the game drops it and runs it; the clip tips its nose down for the water")
    m.part("screws", "torpedo", (-2.5, 0, 0), axis=(1, 0, 0), kind="spin", note="its two screws; they turn once it is in the water")
    m.part("box", "torpedo", (-2.3, 0, 0), kind="free", note="the wooden fins for the air: scale them away when it enters the water")
    sides = 16 if fine else 8
    prof = [(2.65, 0.0), (2.6, 0.08), (2.45, 0.17), (2.15, 0.225), (-1.6, 0.225), (-2.2, 0.12), (-2.5, 0.05)]
    rings = [[Vector((x, r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides))) for k in range(sides)] for x, r in prof]
    m.use("torpedo")
    m.skin(rings, lambda c, n: "head" if c.x > 1.55 else "case", caps=(True, True))
    for k in range(4):                                   # (its rudders)
        a = k * math.pi / 2
        d = Vector((0, math.cos(a), math.sin(a)))
        m.add([Vector((-1.9, 0, 0)) + d * 0.16, Vector((-2.42, 0, 0)) + d * 0.05, Vector((-2.42, 0, 0)) + d * 0.225, Vector((-2.1, 0, 0)) + d * 0.225], [(0, 1, 2, 3)], "case", False)
    m.use("screws")
    for x, turn in ((-2.52, 0.0), (-2.6, 0.5)):
        for k in range(4 if fine else 2):
            a = turn + k * math.pi / 2
            d, e = Vector((0, math.cos(a), math.sin(a))), Vector((0, -math.sin(a), math.cos(a)))
            m.add([Vector((x, 0, 0)) + d * 0.04 - e * 0.02, Vector((x + 0.03, 0, 0)) + d * 0.17 - e * 0.05, Vector((x - 0.03, 0, 0)) + d * 0.17 + e * 0.05, Vector((x, 0, 0)) + d * 0.04 + e * 0.02],
                  [(0, 1, 2, 3)], "screw", False)
    m.use("box")
    for a in (0.0, math.pi / 2):                         # (two boards across and two up and down: a box open fore and aft)
        d, e = Vector((0, math.cos(a), math.sin(a))), Vector((0, -math.sin(a), math.cos(a)))
        for off in (-0.24, 0.24):
            m.add([Vector((-1.95, 0, 0)) + e * off - d * 0.24, Vector((-1.95, 0, 0)) + e * off + d * 0.24, Vector((-2.65, 0, 0)) + e * off + d * 0.24, Vector((-2.65, 0, 0)) + e * off - d * 0.24],
                  [(0, 1, 2, 3)], "wood", False)
    m.socket("nose", (2.65, 0, 0), X, note="it strikes here")
    m.socket("wake", (-2.6, 0, 0), -X, note="its track of bubbles starts here")
    m.clips["drop"] = {"long": 3.0, "loop": False, "after": "hold", "about": "Released level and low, its nose drops a little so that it enters the water cleanly (the game carries it forward and down).",
                       "tracks": [{"part": "torpedo", "rot": rot_keys(lambda t: q_of(pitch=-16.0 * smooth(t / 2.4)), 3.0, 0.25)}],
                       "events": []}
    m.clips["run"] = {"long": 1.0, "loop": True, "after": "loop", "about": "In the water: the air fins gone, the screws turning. The game runs it to its mark.",
                      "tracks": [{"part": "box", "scale": [[0.0, 0.001]]},
                                 {"part": "screws", "turn": [[0.0, 0.0], [1.0, 1440.0]]}],
                      "events": [{"t": 0.0, "fx": "torpedo_wake", "socket": "wake", "cue": "torpedo_run_loop"}]}
    return m


BUILDERS = {n: (lambda fine=True, n=n: convert(n, fine)) for n in ("Bf109", "Spitfire", "Zero", "Mustang", "Yak", "Fw190", "Me262", "Hurricane", "Warhawk", "Hellcat", "Fighter", "CarrierFighter",
                                                                  "Stuka", "Sturmovik", "Val", "Dauntless", "DiveBomber", "Kate", "Avenger", "TorpedoBomber", "He111", "Nell", "Betty", "Lancaster", "B17",
                                                                  "Amerikabomber", "B29", "Bomber")}
