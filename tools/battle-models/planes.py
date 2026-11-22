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

import carrier_avenger
import carrier_dauntless
import carrier_devastator
import carrier_hellcat
import carrier_wildcat
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
# Each control surface: (from, to along its span, the share of the chord its hinge is at). A machine's numbers may give
# her own (`flap`, `aileron`, `elevator`, `rudder`); the share snaps to the next ring of CHORDS (CHORDS_PLAIN on the
# plain model), and a rudder that runs to 1.0 runs to the top of its fin.
FLAP, AILERON, ELEVATOR, RUDDER = (0.1, 0.5, 0.76), (0.54, 0.93, 0.74), (0.08, 0.95, 0.62), (0.06, 1.0, 0.60)
# Where a bomber's bay is: from, to along her (its doors are the strip of her belly BAY_HALF degrees each side of her keel).
# (the Avenger's is set from carrier_avenger.py, below)
BAYS = {"He111": (3.3, 1.3), "Lancaster": (6.4, -3.0), "B17": (2.3, -0.1), "Bomber": (2.0, -0.8), "TorpedoBomber": (2.0, -1.9),
        "Betty": (2.8, -2.7), "Amerikabomber": (2.6, -2.2), "B29": (6.4, 2.9)}
# The machines whose bay holds a torpedo, or bombs instead (the loads are parts `torpedo` and `bombs`: one is hidden).
# (the Avenger's is set from carrier_avenger.py, below)
BAY_TORPEDO = {"TorpedoBomber": (3.8, 0.24), "Betty": (5.3, 0.225)}       # (how long it is, half as thick)
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
PANES = {"B17": [(-12.3, -12.86, 36.0, 54.0), (-13.2, -13.26, 54.0, 90.0)], "B29": [(-18.04, -18.74, 0.0, 36.0), (-18.51, -19.2, 45.0, 90.0)],
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
    ("tail.us(0.4 if fine else 1.0)", "battle_us(tail, BATTLE['tail'] if fine else 1.0, s.get('elevator', BATTLE_ELEVATOR)[:2])"),
    ("fin.us(0.3 if fine else 0.8)", "battle_us(fin, BATTLE['fin'] if fine else 0.8, rudder_ends(s.get('rudder', BATTLE_RUDDER)))"),
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
    ('    hull = Body(s["hull"], s.get("square", 2.0))\n', '    hull = __import__("fitout").hull_body(Body, s, fine)\n'),
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

# A machine's own numbers where the sculptor gives every machine the same (each key optional: without it she is built
# as before). `elevator` and `rudder` (see RUDDER above): the hinge lines it scribes on her tailplane and fin follow
# them, where the surface is cut (battle_hinge). `heads`: the cylinder heads of the radial in her nose, its mouth cut
# open back to them on the fine model (battle_recess, battle_heads). `mouth`: how far out from her middle line the face
# of that engine is dark (the rest of it is her cowling's lip), and with `heads` cut open. A `dome` in her extras with
# a third value: a turret whose lower body is metal (battle_dome). Applied after GILLS, whose lines they take.
OWN = [
    # (`engine_face`: the radial in each of her nacelles is laid by fitout.engine_face, its cylinders standing out round
    #  its nose case as they are seen through the cowling's mouth; the sculptor's ring of heads is then left out, and
    #  the nose case is in the engine's dark paint)
    ("                radial_details(parts, frames, nacelle, lead + 1.75, lead + 0.75, lines, gill_plates)\n",
     "                radial_details(parts, frames, nacelle, lead + 1.75, lead + 0.75, lines, gill_plates, 'face' if s.get('engine_face') else None)\n"),
    ('                parts.rod("crankcase", (lead + 1.7, side, z), (lead + 2.02, side, z), 0.16, 0.1, paint="leg", sides=10)\n',
     '                parts.rod("crankcase", (lead + 1.7, side, z), (lead + 2.02, side, z), 0.16, 0.1, paint="engine" if s.get("engine_face") else "leg", sides=10)\n'),
    ("lines_on_wing(parts, lines, tail, True, (0.08, 0.95, 0.62), (0.0, 0.0, 0.0), (), panel=())",
     "lines_on_wing(parts, lines, tail, True, battle_hinge(s['elevator'], chords) if 'elevator' in s else (0.08, 0.95, 0.62), (0.0, 0.0, 0.0), (), panel=())"),
    ("                us = [fin.root + (fin.tip - fin.root) * (0.06 + 0.9 * k / 12.0) for k in range(13)]\n"
     "                for upper in (True, False):\n"
     "                    parts.strip(lines, [fin.at(u, 0.6, upper) for u in us], [fin.out(u, 0.6, upper) for u in us], 0.011)\n",
     "                own_rudder = s.get('rudder')\n"
     "                if own_rudder is None:\n"
     "                    us, own_hinge = [fin.root + (fin.tip - fin.root) * (0.06 + 0.9 * k / 12.0) for k in range(13)], 0.6\n"
     "                else:\n"
     "                    own_lo, own_hi = own_rudder[0], min(own_rudder[1], 0.96)\n"
     "                    us, own_hinge = [fin.root + (fin.tip - fin.root) * (own_lo + (own_hi - own_lo) * k / 12.0) for k in range(13)], battle_hinge(own_rudder, chords)[2]\n"
     "                for upper in (True, False):\n"
     "                    parts.strip(lines, [fin.at(u, own_hinge, upper) for u in us], [fin.out(u, own_hinge, upper) for u in us], 0.011)\n"),
    ("def radial_details(parts, frames, body, mouth, gills, lines=None, plates=None):", "def radial_details(parts, frames, body, mouth, gills, lines=None, plates=None, heads=None):"),
    ("    if parts.fine:\n        for k in range(14):\n", "    if parts.fine and heads == 'face':\n        pass\n    elif parts.fine and heads:\n        battle_heads(parts, body, mouth, heads)\n    elif parts.fine:\n        for k in range(14):\n"),
    ("            radial_details(parts, frames, hull, lip, nose_back if nose_back < 1.0e8 else lip - 1.0, lines, gill_plates)\n",
     "            if parts.fine and s.get('heads'):\n"
     "                battle_recess(parts, hull, s, lip, boss_back)\n"
     "            radial_details(parts, frames, hull, lip, nose_back if nose_back < 1.0e8 else lip - 1.0, lines, gill_plates, s.get('heads'))\n"),
    ("        if radial and lip is not None and p.x > lip - 0.02 and n.x > 0.6:\n",
     "        if radial and lip is not None and p.x > lip - 0.02 and n.x > 0.6 and (s.get('mouth') is None or math.hypot(p.y - hull.side, p.z - hull.z(-lip)) < s['mouth']):\n"),
    ("            at, radii = values\n            dome(parts, frames, at, radii)\n",
     "            at, radii = values[:2]\n"
     "            if len(values) > 2:\n"
     "                battle_dome(parts, frames, at, radii, *values[2:])\n"
     "            else:\n"
     "                dome(parts, frames, at, radii)\n"),
    # (An extra rod, ball or body whose last value is "fine" is laid on the fine model only: a thin post, a rail or a
    #  brace that the map's model, seen from far off, does not need.)
    ("            a, b, ra, rb, paint = values\n",
     "            a, b, ra, rb, paint = values[:5]\n            if 'fine' in values[5:] and not fine:\n                continue\n"),
    # (An extra rod with "one" among its last values is laid where its numbers put it, on that side only, and not
    #  mirrored: the Devastator's pitot, on her starboard wing alone.)
    ("            for hand in ((1.0, -1.0) if a[1] != 0.0 or b[1] != 0.0 else (1.0,)):\n",
     "            for hand in ((1.0, -1.0) if (a[1] != 0.0 or b[1] != 0.0) and 'one' not in values[5:] else (1.0,)):\n"),
    ("            at, radii, paint = values\n",
     "            at, radii, paint = values[:3]\n            if 'fine' in values[3:] and not fine:\n                continue\n"),
    ("            stations, side, square, paint = values\n",
     "            stations, side, square, paint = values[:4]\n            if 'fine' in values[4:] and not fine:\n                continue\n"),
]

NOSES = [
    # (`glazed_sill`: behind the part that is glass all round, her windows come down her sides to a sill this many
    #  degrees round her above her middle line, on one of her skin's own rows so that its edge is clean, and she is
    #  metal below it; `cabin_ribs`: where round her the frames run along her over those windows)
    ('(p.x > glazed[1] and n.z > -0.55)', '(p.x > glazed[1] and ((p.z - hull.z(-p.x)) / hull.h(-p.x) > math.sin(math.radians(s["glazed_sill"])) ** hull.power if "glazed_sill" in s else n.z > -0.55))'),
    # (`nacelle`: an engine in line in a nacelle of her own lines: see fitout.nacelle_of)
    ("                nacelle = Body([(lead + 2.55, z, 0.04, 0.04), (lead + 2.1, z, 0.34, 0.34), (lead + 2.05, z - 0.04, 0.4, 0.48), (lead + 0.6, z - 0.1, 0.47, 0.6),\n"
     "                                (lead - 1.2, z - 0.14, 0.44, 0.5), (lead - back, z - 0.04, 0.1, 0.1)], 2.3, side)\n",
     "                nacelle = battle_nacelle(globals(), lead, z, False, back, side, s)\n"),
    ("        xs = [hull.front - 0.02 - (hull.front - glazed[0] - 0.04) * k / 12.0 for k in range(13)]\n",
     "        x_tip = s.get('nose_cap', hull.front - 0.02)\n"
     "        bar = s.get('nose_bar', 1.0)\n"
     "        xs = [x_tip - (x_tip - glazed[0] - 0.02) * k / 12.0 for k in range(13)]\n"),
    ("            parts.strip(frames, [hull.at(x, a) for x in xs], [hull.out(x, a) for x in xs], 0.016, 0.008)\n",
     "            parts.strip(frames, [hull.at(x, a) for x in xs], [hull.out(x, a) for x in xs], 0.016 * bar, 0.008)\n"
     "            if s.get('windows_back') and 'glazed_sill' not in s and hull.out(glazed[1], a).z > -0.5:\n"
     "                more = [glazed[0] - (glazed[0] - glazed[1]) * j / 6.0 for j in range(7)]\n"
     "                parts.strip(frames, [hull.at(x, a) for x in more], [hull.out(x, a) for x in more], 0.016 * bar, 0.008)\n"
     "        for deg in ([s['glazed_sill'], 180.0 - s['glazed_sill']] + list(s.get('cabin_ribs', ())) if 'glazed_sill' in s else []):\n"
     "            more = [glazed[0] - (glazed[0] - glazed[1]) * j / 8.0 for j in range(9)]\n"
     "            parts.strip(frames, [hull.at(x, math.radians(deg)) for x in more], [hull.out(x, math.radians(deg)) for x in more], 0.016 * bar, 0.008)\n"),
    ("            parts.strip(frames, [hull.at(x, a) for a in angles], [hull.out(x, a) for a in angles], 0.018, 0.008)\n",
     "            ring = angles if x >= glazed[0] - 0.01 else [math.radians(-30.0 + 240.0 * k / 30.0) for k in range(31)]\n"
     "            if 'glazed_sill' in s and x < glazed[0] - 0.01:\n"
     "                ring = [math.radians(s['glazed_sill'] + (180.0 - 2.0 * s['glazed_sill']) * k / 30.0) for k in range(31)]\n"
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



def sun_faded(stations, square, fin_from, wide=0.62, back=0.62, side=0.16, fin=0.38, seed=0):
    """Olive drab as the sun and the weather left it (Brian's B-17G pictures, 2026-11-20): palest on her wings and
    tailplane (`wide`) and along her back (`back`), least low down her sides (`side`), her fin between (`fin`); and
    panel by panel, no two alike, a few of them fresh from the depot. For `graded` with `patch`: her `upper` is the
    paint as it came, `upper2` as faded as it gets, and each point of her skin says how far between (the coat's
    field: -0.045 fresh, 0.045 the most faded)."""
    box = []

    def dice(*key):                                      # (the same number for every point of one panel)
        h = math.sin(sum(k * m for k, m in zip(key, (12.9898, 78.233, 37.719, 4.581))) + seed * 1.618) * 43758.5453
        return h - math.floor(h)

    def ease(a, b, t):
        t = min(max((t - a) / (b - a), 0.0), 1.0)
        return t * t * (3.0 - 2.0 * t)

    def field(p):
        if not box:
            box.append(game()["Body"](stations, square))
        body = box[0]
        x = min(max(p.x, body.back), body.front)
        w, h, z = body.w(-x), body.h(-x), body.z(-x)
        hand = 1 if p.y > 0.0 else -1
        if abs(p.y) > w + 0.03:                          # (her wings, nacelles and tailplane: panels about 0.9 m by 1.5 m)
            base, key = wide, (math.floor(p.x / 0.9), math.floor(abs(p.y) / 1.5) * hand, 1)
        elif p.x < fin_from and p.z > z + h + 0.04:      # (her fin and rudder)
            base, key = fin, (math.floor(p.x / 0.8), math.floor(p.z / 0.7), 2)
        else:                                            # (her fuselage: a panel 1.15 m long and a fifth of her half round)
            rel = (p.z - z) / max(h, 0.1)
            base = side + (back - side) * ease(-0.15, 0.85, rel)
            key = (math.floor(p.x / 1.15), math.floor((math.atan2(p.z - z, p.y) + math.pi) / (math.pi / 5.0)), 3)
        r = dice(*key)
        t = base + (r - 0.5) * 0.17                      # (panel to panel a little apart: Brian's pictures, 2026-11-20, are nearly even)
        if dice(*key, 9) < 0.04:
            t -= 0.24                                    # (a panel put on new)
        t += 0.06 * noise.noise(Vector((p.x * 0.45 + 3.1, p.y * 0.45 - 7.7, p.z * 0.45 + 1.3)))
        t = min(max(t, 0.0), 1.0)
        return 0.09 * t - 0.045
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
# The three of our own drawing are each in a module of her own: carrier_hellcat.py, carrier_dauntless.py and
# carrier_avenger.py. (A module uses planes only inside its functions: this file imports it before it is whole.)
def carrier(module):
    """Takes in a carrier plane from her module: her NAME, her NUMBERS, who sits under her glass (SEATS), and where her fin
    begins (FIN_FROM). Her coat is the US Navy's blues, laid by where a point is: navy_blues on her own fuselage from
    FIN_FROM, unless her module gives coat(navy_blues), which returns hers. (In 1941 and 1942 she is one colour on top,
    but her skin is laid out for the three blues that follow: the band down her sides and her fin are `upper2`, the same
    blue-grey until the later coat.) Any table of her own the module gives is put where the others' are: HOLED
    (fitout.HOLED), SEAT_DROP (fitout.SEAT_DROP), BAY (BAYS), TORPEDO (BAY_TORPEDO), TAIL_CUT and TURRET (her
    TURRET_LIMITS)."""
    numbers = module.NUMBERS
    numbers["graded"] = module.coat(navy_blues) if hasattr(module, "coat") else navy_blues(numbers["hull"], numbers["square"], module.FIN_FROM)
    EXTRA_PLANES[module.NAME] = numbers
    fitout.SEATS[module.NAME] = module.SEATS
    for table, into in (("HOLED", fitout.HOLED), ("SEAT_DROP", fitout.SEAT_DROP), ("BAY", BAYS), ("TORPEDO", BAY_TORPEDO), ("TAIL_CUT", TAIL_CUT),
                        ("TURRET", TURRET_LIMITS)):
        if hasattr(module, table):
            into[module.NAME] = getattr(module, table)


# (The art director, 2026-11-18, of the first one: she, the Avenger and the generic carrier planes "read as one plane in
#  different paint". Hers are: a deep, portly fuselage, a short sliding hood with metal behind it, the intake under
#  her cowl, and a broad low wing cut off square. Her numbers are in carrier_hellcat.py.)
carrier(carrier_hellcat)
# (the Wildcat, the fleet's fighter before her, at Midway in June 1942: her numbers are in carrier_wildcat.py)
carrier(carrier_wildcat)

JETS = [
    # (and `nose_engine`: a machine with engines on her wings that has one in her nose as well, a trimotor's: her nose
    #  airscrew, spinner and cowling are seated as a single-engined machine's are)
    ('    boss_back = 1.0e9 if s.get("engines") else sorted(', '    boss_back = 1.0e9 if (s.get("engines") and not s.get("nose_engine")) or s.get("jets") else sorted('),
    ('    if not s.get("engines"):\n        airscrew(parts, (hub_x, 0.0, hub_z)', '    if (not s.get("engines") or s.get("nose_engine")) and not s.get("jets"):\n        airscrew(parts, (hub_x, 0.0, hub_z)'),
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
# ---- the D3A1 Val, laid out again to Brian's benchmark for all aircraft (2026-11-19) against "Aichi D3A1 Val - 3
# vistas" (Wikimedia Commons, MMK-Draw, CC BY-SA 4.0) at 267 pixels to the metre along her and 265 across, her length
# 10.195 m and her span 14.365 m: measured, not traced. What changed: a round cowling a little broader than the
# fuselage behind it, with the oil cooler's bath under it; a deep fuselage with a level deck and a slim tail; her
# elliptical wing, flat between the legs and rising outboard of them, its trailing edge run into her side in a long
# fillet; the long low fillet ahead of her fin; a tall round-topped rudder; a tailplane as rounded as her wing. Her
# greenhouse is four framed pieces over an open cockpit (the fighters' `canopy`: fighters.py): a windscreen with a
# flat pane between two posts and the tube of her bomb sight through it, the pilot's hood, the fixed middle with
# the mast, the gunner's hood a step lower each time (they slid one under the other), and a glazed end that falls
# to his gun. Her legs are fixed, in trousers and long spats; a brake under each wing outboard of them. Her pale
# grey and black cowling and her plain red discs are as they were.
#
# Her greenhouse, measured: (x, the height of its sill where the glass begins, how far out it is there, the height
# of the frame at its shoulder (None in its glazed end), the height of its top).
_VAL_HOOD = [(1.28, 0.60, 0.378, 0.908, 0.994), (0.99, 0.594, 0.378, 0.908, 0.994), (0.70, 0.588, 0.378, 0.905, 0.994)]
_VAL_MID = [(0.72, 0.588, 0.375, 0.88, 0.950), (0.40, 0.584, 0.375, 0.88, 0.950), (0.13, 0.58, 0.375, 0.88, 0.950)]
_VAL_REAR = [(0.15, 0.578, 0.372, 0.835, 0.927), (-0.058, 0.576, 0.372, 0.835, 0.927), (-0.245, 0.573, 0.372, 0.835, 0.927), (-0.49, 0.57, 0.372, 0.835, 0.925)]
_VAL_END = [(-0.47, 0.57, 0.369, None, 0.90), (-0.60, 0.575, 0.366, None, 0.867), (-0.70, 0.58, 0.36, None, 0.796), (-0.80, 0.585, 0.348, None, 0.729),
            (-0.87, 0.59, 0.27, None, 0.68), (-0.92, 0.595, 0.16, None, 0.645), (-0.945, 0.60, 0.04, None, 0.625)]
_VAL_SCREEN = (1.56, 0.13, 1.33, 0.985)                     # (the foot of her windscreen's front pane, half its width, its head, and the top there)
_VAL_POST = 0.15                                            # (her windscreen's posts stand this far out at its head)


def _val_section(hull, x, sill, ys, shoulder, top, post=None, lean=0.886):
    """Half a section of her greenhouse: the metal rail it stands on (the first SKIRT points: its foot on her skin);
    a flat side pane leaning in from the sill to the frame at its shoulder (`lean`: how far out the shoulder is, as
    a share of the sill's); and its roof, nearly flat, over a frame 0.19 out to her middle line. `post` adds a point
    where a windscreen's post meets it."""
    yb = ys + 0.004
    zb = min(sill - 0.05, fitout.body_top(hull, x, yb) - 0.006)
    if shoulder is None:
        shoulder = sill + 0.78 * (top - sill)
    ysh, yr = lean * ys, min(0.19, 0.55 * ys)
    zr = shoulder + 0.8 * (top - shoulder)
    pts = [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb), (ys, sill), (ysh, shoulder), (yr, zr)]
    if post is not None:
        pts.append((post, zr + (top - zr) * (yr - post) / yr))
    return pts + [(0.0, top)]


def _val_canopy(hull):
    skirt = fighters.SKIRT
    foot_x, foot_half, head_x, head_top = _VAL_SCREEN
    x0, sill, ys, shoulder, top = _VAL_HOOD[0]
    # her windscreen: the flat front pane from its foot on her deck up to its head between the posts, and a side pane
    # each side from the foot of its post back and out to the rail; its roof on to the hood's front frame
    head = fighters.headed(_val_section(hull, head_x, sill + 0.02, ys - 0.012, shoulder - 0.012, head_top), _VAL_POST)
    post = len(head) - 2
    foot = [fighters.deck(hull, foot_x, foot_half)] * (len(head) - 1) + [fighters.deck(hull, foot_x, 0.0)]
    screen = {"sections": [(foot_x, foot), (head_x, head), (x0, _val_section(hull, x0, sill, ys, shoulder, top, _VAL_POST))], "skirt": skirt,
              "hoops": [1, 2], "rails": [post, skirt], "bar": 0.02}

    def run(rows):
        return [(x, _val_section(hull, x, sl, w, sh, t)) for x, sl, w, sh, t in rows]
    # the pilot's hood, the fixed middle and the gunner's hood: a frame round each end and one between, the sills, the
    # shoulder frames and the roof frames; the gunner's has one frame more across its roof
    hood = {"sections": run(_VAL_HOOD), "skirt": skirt, "hoops": [1, 2], "rails": [skirt, 3, 4], "bar": 0.018}
    mid = {"sections": run(_VAL_MID), "skirt": skirt, "hoops": [0, 1, 2], "rails": [skirt, 3, 4], "bar": 0.018}
    rear = {"sections": run(_VAL_REAR), "skirt": skirt, "hoops": [0, 1, 3], "bars": [(2, 3, 5)], "rails": [skirt, 3, 4], "bar": 0.018}
    # its glazed end, falling to his gun
    end = {"sections": run(_VAL_END), "skirt": skirt, "hoops": [0], "rails": [skirt, 3], "bar": 0.016, "cap": True}
    return [screen, hood, mid, rear, end]


_VAL_LEG = 1.69                                             # (her legs stand this far out)
EXTRA_PLANES["Val"] = {
    "title": "D3A1 Val (our own model)", "year": 1940,
    # (the boss of her airscrew, which had no spinner; the mouth of her cowling and its rounded lip; her cowling to its gills; then her fuselage)
    "hull": [(4.0, 0.0, 0.03, 0.03), (3.975, 0.0, 0.055, 0.055), (3.86, 0.0, 0.06, 0.06), (3.84, 0.0, 0.1, 0.1), (3.64, 0.0, 0.1, 0.1), (3.62, 0.0, 0.17, 0.17),
             (3.52, 0.0, 0.2, 0.2), (3.49, 0.0, 0.47, 0.47), (3.475, 0.0, 0.5, 0.505), (3.45, 0.0, 0.54, 0.548), (3.4, 0.0, 0.567, 0.581), (3.3, -0.005, 0.59, 0.622),
             (3.2, -0.01, 0.594, 0.635), (3.0, -0.012, 0.597, 0.65), (2.8, -0.01, 0.598, 0.655), (2.63, -0.009, 0.598, 0.656),
             (2.6, -0.012, 0.545, 0.648), (2.4, 0.0, 0.552, 0.688), (2.2, 0.008, 0.554, 0.702), (2.0, 0.015, 0.555, 0.714), (1.8, 0.017, 0.554, 0.725),
             (1.56, 0.012, 0.552, 0.728), (1.28, 0.012, 0.547, 0.73), (0.7, 0.008, 0.537, 0.727), (0.13, 0.002, 0.521, 0.723), (-0.49, -0.009, 0.5, 0.709),
             (-0.75, -0.024, 0.492, 0.689), (-0.95, -0.043, 0.487, 0.671), (-1.2, -0.052, 0.484, 0.655), (-1.6, -0.064, 0.471, 0.629), (-2.0, -0.069, 0.457, 0.6),
             (-2.4, -0.077, 0.435, 0.562), (-3.0, -0.086, 0.392, 0.512), (-3.6, -0.09, 0.34, 0.447), (-4.2, -0.088, 0.28, 0.372), (-4.8, -0.072, 0.21, 0.278),
             (-5.2, -0.064, 0.16, 0.21), (-5.6, -0.055, 0.1, 0.134), (-5.9, -0.047, 0.055, 0.072), (-6.1, -0.04, 0.025, 0.03), (-6.19, -0.03, 0.008, 0.01)],
    "square": 2.2, "screw": 1.53, "blades": 3, "nose_back": 2.62, "spinner": 5, "engine": "radial", "lip": 3.49,
    "hoods": fighters.hood_of([(_VAL_SCREEN[0], 0.72, _VAL_SCREEN[1], 0.01), (_VAL_SCREEN[2], 0.62, 0.366, _VAL_SCREEN[3] - 0.62)]
                              + [(x, sl, w, t - sl) for x, sl, w, sh, t in _VAL_HOOD[:2] + _VAL_MID[:2] + _VAL_REAR + _VAL_END[1:]]),
    "canopy": _val_canopy,
    "opening": (1.28, -0.75, 52.0),                         # (her cockpit, open under the glass from her panel to behind her gunner)
    "frames_paint": "upper", "lines": (0.6, 0.45),
    # (her wing: an ellipse, like no other dive bomber's; flat out to her legs, six and a half degrees of dihedral beyond
    #  them; sixteen per cent thick at the root, ten at the fold; its trailing edge swept back into her side in the fillet)
    "wing": [(0.0, 2.25, -1.45, -0.455, 0.57), (0.45, 2.25, -1.45, -0.455, 0.57), (0.5, 2.25, -1.43, -0.455, 0.57), (0.55, 2.248, -1.25, -0.455, 0.57),
             (0.6, 2.247, -1.107, -0.455, 0.57), (0.65, 2.24, -0.994, -0.455, 0.57), (0.7, 2.235, -0.901, -0.455, 0.57), (0.8, 2.232, -0.796, -0.455, 0.57),
             (0.9, 2.228, -0.736, -0.455, 0.568), (1.0, 2.224, -0.706, -0.455, 0.565), (1.2, 2.217, -0.71, -0.455, 0.56), (1.5, 2.202, -0.71, -0.455, 0.55),
             (1.7, 2.19, -0.71, -0.455, 0.54), (2.1, 2.164, -0.71, -0.409, 0.506), (2.4, 2.138, -0.706, -0.374, 0.481), (2.7, 2.112, -0.702, -0.34, 0.456),
             (3.0, 2.078, -0.687, -0.305, 0.43), (3.3, 2.04, -0.665, -0.271, 0.405), (3.6, 1.999, -0.635, -0.236, 0.38), (3.9, 1.958, -0.597, -0.201, 0.357),
             (4.2, 1.909, -0.552, -0.167, 0.334), (4.5, 1.857, -0.496, -0.132, 0.311), (4.8, 1.804, -0.436, -0.098, 0.288), (5.1, 1.744, -0.365, -0.063, 0.265),
             (5.4, 1.677, -0.29, -0.028, 0.242), (5.7, 1.602, -0.204, 0.006, 0.218), (6.0, 1.52, -0.123, 0.041, 0.193), (6.3, 1.418, 0.002, 0.075, 0.166),
             (6.6, 1.299, 0.137, 0.11, 0.136), (6.8, 1.209, 0.246, 0.133, 0.112), (6.9, 1.164, 0.317, 0.145, 0.097), (7.0, 1.111, 0.403, 0.156, 0.078),
             (7.05, 1.087, 0.459, 0.162, 0.066), (7.1, 1.059, 0.53, 0.168, 0.05), (7.15, 1.014, 0.635, 0.174, 0.03), (7.183, 0.88, 0.78, 0.177, 0.01)],
    "aileron": (0.5, 0.92, 0.8), "flap": (0.14, 0.5, 0.815),
    "brakes": (2.47, 4.2, 0.3, 0.13, 0.09),                  # (the slat under each wing, outboard of her legs, that held her speed in the dive)
    "tail": [(0.0, -4.3, -6.1, 0.0, 0.17), (0.3, -4.337, -6.116, 0.0, 0.165), (0.6, -4.43, -6.139, 0.0, 0.155), (0.9, -4.516, -6.113, 0.0, 0.142),
             (1.2, -4.614, -6.068, 0.0, 0.126), (1.5, -4.749, -6.008, 0.0, 0.108), (1.8, -4.929, -5.921, 0.0, 0.086), (1.98, -5.056, -5.832, 0.0, 0.07),
             (2.1, -5.165, -5.727, 0.0, 0.055), (2.14, -5.217, -5.674, 0.0, 0.045), (2.18, -5.277, -5.584, 0.0, 0.03), (2.212, -5.4, -5.47, 0.0, 0.01)],
    # (her fin from inside her fuselage up: the long low fillet that runs into its leading edge, then the fin, and the rudder rounded over its top)
    "fin": [(0, -2.38, -5.97, 0.2, 0.1), (0, -2.38, -5.97, 0.5, 0.1), (0, -2.5, -5.97, 0.52, 0.1), (0, -2.62, -5.97, 0.535, 0.1), (0, -2.8, -5.97, 0.55, 0.1),
            (0, -3.0, -5.97, 0.565, 0.1), (0, -3.4, -5.97, 0.585, 0.1), (0, -3.8, -5.965, 0.609, 0.105), (0, -4.0, -5.965, 0.62, 0.11), (0, -4.1, -5.96, 0.64, 0.115),
            (0, -4.2, -5.96, 0.66, 0.12), (0, -4.3, -5.955, 0.751, 0.125), (0, -4.4, -5.945, 0.908, 0.125), (0, -4.5, -5.92, 1.095, 0.12), (0, -4.6, -5.905, 1.256, 0.112),
            (0, -4.7, -5.88, 1.394, 0.104), (0, -4.8, -5.84, 1.522, 0.094), (0, -4.9, -5.79, 1.634, 0.083), (0, -5.0, -5.715, 1.731, 0.07), (0, -5.1, -5.62, 1.813, 0.055),
            (0, -5.2, -5.52, 1.866, 0.04), (0, -5.3, -5.44, 1.888, 0.025), (0, -5.36, -5.4, 1.892, 0.01)],
    "patch": 0.0, "colours": dict(_PALE), "pitot": True, "tailwheel": (-5.33, -0.56), "mast": (0.16, 0.94, 1.42, 0.02),
    # (the oil cooler's bath under the after part of her cowling)
    "scoops": [([(2.95, -0.66, 0.1, 0.03), (2.86, -0.7, 0.18, 0.09), (2.76, -0.715, 0.2, 0.1), (2.4, -0.71, 0.2, 0.1), (2.2, -0.7, 0.17, 0.085), (2.06, -0.67, 0.09, 0.03)], 0.0, 2.6)],
    "extras": [  # (each leg in its trouser, the long spat round the wheel, the tyre showing under it)
               ("body", ([(2.14, -0.88, 0.02, 0.27), (2.05, -0.88, 0.075, 0.29), (1.92, -0.88, 0.085, 0.29), (1.8, -0.88, 0.06, 0.28), (1.72, -0.88, 0.015, 0.26)], _VAL_LEG, 2.6, "coat")),
               ("body", ([(2.48, -1.56, 0.02, 0.03), (2.44, -1.562, 0.07, 0.17), (2.4, -1.565, 0.095, 0.25), (2.3, -1.57, 0.135, 0.35), (2.2, -1.56, 0.153, 0.39),
                          (2.05, -1.515, 0.16, 0.405), (1.8, -1.525, 0.16, 0.395), (1.6, -1.53, 0.155, 0.36), (1.4, -1.53, 0.14, 0.31), (1.2, -1.53, 0.115, 0.245),
                          (1.0, -1.544, 0.075, 0.153), (0.9, -1.555, 0.04, 0.08), (0.87, -1.555, 0.01, 0.02)], _VAL_LEG, 2.3, "coat")),
               ("rod", ((1.93, _VAL_LEG - 0.09, -1.79), (1.93, _VAL_LEG + 0.09, -1.79), 0.35, 0.35, "tyre")),
               # (the tube of her bomb sight, through her windscreen, on its two feet)
               ("rod", ((2.06, 0.0, 0.81), (1.42, 0.0, 0.81), 0.02, 0.02, "engine")),
               ("rod", ((1.87, 0.0, 0.73), (1.87, 0.0, 0.8), 0.012, 0.012, "engine")), ("rod", ((1.64, 0.0, 0.725), (1.64, 0.0, 0.8), 0.012, 0.012, "engine")),
               # (her bomb, and the crutch that swung it clear of her airscrew)
               ("body", ([(1.73, -0.93, 0.03, 0.03), (1.62, -0.93, 0.13, 0.13), (1.45, -0.93, 0.19, 0.19), (1.2, -0.93, 0.2, 0.2), (0.75, -0.93, 0.2, 0.2),
                          (0.5, -0.93, 0.14, 0.14), (0.2, -0.93, 0.06, 0.06), (0.0, -0.93, 0.03, 0.03)], 0.0, 2.0, "bomb")),
               ("rod", ((0.26, 0.0, -0.93), (-0.03, 0.0, -0.93), 0.16, 0.16, "bomb")),
               ("rod", ((1.5, 0.14, -0.69), (1.05, 0.2, -0.9), 0.016, 0.016, "leg")),
               # (her rear gunner's gun, out through the end of his glass)
               ("rod", ((-0.72, 0.0, 0.665), (-1.42, 0.0, 0.7), 0.022, 0.018, "steel"))],
    "side_marks": [(-2.55, 0.34, "JAP-PLAIN")], "wing_marks": [(4.9, 0.55, 0.62, "JAP-PLAIN", "JAP-PLAIN", True, True)], "code": (-3.7, 0.3),
}
fitout.SEATS["Val"] = [(0, 0.88, 0.0, 0.0), (0, -0.2, 0.0, 180.0)]

# ---- the B5N2 Kate, laid out again to Brian's benchmark for all aircraft (2026-11-19) against "Nakajima Kate"
# (Wikimedia Commons, Kaboldy, CC BY-SA 3.0) at 257 pixels to the metre, her length 10.30 m and her span 15.518 m:
# measured, not traced. What changed: a round cowling broader than the fuselage behind it; a fuselage as deep below
# her airscrew's line as above it, slab-sided and slim to the tail; a long straight-tapered wing, flat out to 2.3 m
# and rising beyond, its trailing edge run into her side in a fillet; a tall fin with a straight raked leading edge
# and a rounded rudder; a small tailplane on her airscrew's line. Her greenhouse is seven framed pieces over one open
# cockpit (the fighters' `canopy`): a windscreen, five hoods each a step lower and narrower than the one before
# (they slid one under the other), and a glazed end that falls to the gunner's gun. Pilot, observer and gunner; the
# gunner faces aft with both hands on his gun. Her wheels are up. Her torpedo hangs nose down under her, as it did;
# the heavy bomb she carried instead is there too (the game shows one). Pale grey, black cowling, plain red discs.
#
# Her greenhouse, measured: (x, the height of its sill where the glass begins, how far out it is there, the height
# of the frame at its shoulder (None in its glazed end), the height of its top).
_KATE_HOODS = [
    [(1.69, 0.62, 0.357, 0.893, 0.995), (1.36, 0.62, 0.357, 0.893, 0.995), (1.03, 0.62, 0.357, 0.893, 0.995)],
    [(1.05, 0.625, 0.352, 0.872, 0.983), (0.75, 0.625, 0.352, 0.87, 0.977), (0.46, 0.625, 0.352, 0.868, 0.972)],
    [(0.48, 0.65, 0.343, 0.86, 0.958), (0.17, 0.65, 0.343, 0.857, 0.951), (-0.14, 0.65, 0.343, 0.853, 0.944)],
    [(-0.12, 0.66, 0.341, 0.836, 0.927), (-0.44, 0.66, 0.341, 0.83, 0.918), (-0.76, 0.66, 0.341, 0.825, 0.91)],
    [(-0.74, 0.655, 0.339, 0.812, 0.895), (-1.03, 0.655, 0.339, 0.808, 0.89), (-1.33, 0.655, 0.339, 0.804, 0.886)]]
_KATE_END = [(-1.31, 0.65, 0.337, None, 0.86), (-1.4, 0.65, 0.333, None, 0.835), (-1.5, 0.648, 0.32, None, 0.783), (-1.6, 0.645, 0.27, None, 0.724),
             (-1.68, 0.642, 0.18, None, 0.68), (-1.73, 0.64, 0.08, None, 0.655), (-1.75, 0.64, 0.03, None, 0.645)]
_KATE_SCREEN = (2.13, 0.14, 1.72, 0.99)                     # (the foot of her windscreen's front pane, half its width, its head, and the top there)
_KATE_POST = 0.16                                           # (her windscreen's posts stand this far out at its head)
_KATE_LEAN = 0.75                                           # (her side panes lean in further than the Val's)


def _kate_canopy(hull):
    skirt = fighters.SKIRT
    foot_x, foot_half, head_x, head_top = _KATE_SCREEN
    sec = lambda x, sl, w, sh, t, post=None: _val_section(hull, x, sl, w, sh, t, post, _KATE_LEAN)
    x0, sill, ys, shoulder, top = _KATE_HOODS[0][0]
    head = fighters.headed(sec(head_x, sill + 0.02, ys - 0.012, shoulder - 0.012, head_top), _KATE_POST)
    post = len(head) - 2
    foot = [fighters.deck(hull, foot_x, foot_half)] * (len(head) - 1) + [fighters.deck(hull, foot_x, 0.0)]
    pieces = [{"sections": [(foot_x, foot), (head_x, head), (x0, sec(x0, sill, ys, shoulder, top, _KATE_POST))], "skirt": skirt,
               "hoops": [1, 2], "rails": [post, skirt], "bar": 0.02}]
    for k, rows in enumerate(_KATE_HOODS):                  # (a frame round each end and one between, the sills, the shoulder frames and the roof frames)
        pieces.append({"sections": [(x, sec(x, sl, w, sh, t)) for x, sl, w, sh, t in rows], "skirt": skirt,
                       "hoops": [1, 2] if k == 0 else [0, 1, 2], "rails": [skirt, 3, 4], "bar": 0.018})
    pieces.append({"sections": [(x, sec(x, sl, w, sh, t)) for x, sl, w, sh, t in _KATE_END], "skirt": skirt, "hoops": [0], "rails": [skirt, 3],
                   "bar": 0.016, "cap": True})
    return pieces


def _kate_torpedo(nose=3.25, tail=-2.22, r=0.225):
    """Her torpedo, 5.47 m of it, hung nose down by two and a half degrees so that its tail clears her."""
    z = lambda x: -0.975 - 0.0385 * (x - 0.3)
    st = [(nose, 0.03), (nose - 0.22, 0.17), (nose - 0.55, r), (tail + 1.0, r), (tail + 0.35, 0.1), (tail, 0.03)]
    return [("body", ([(x, z(x), w, w) for x, w in st], 0.0, 2.0, "torpedo")),
            ("rod", ((tail + 0.02, 0.0, z(tail + 0.02)), (tail + 0.42, 0.0, z(tail + 0.42)), 0.24, 0.24, "torpedo")),
            # (the braces that steady it)
            ("rod", ((2.3, 0.1, -0.63), (2.3, 0.15, -0.9), 0.014, 0.014, "leg")), ("rod", ((1.2, 0.1, -0.7), (1.2, 0.15, -0.84), 0.014, 0.014, "leg"))]


EXTRA_PLANES["Kate"] = {
    "title": "B5N2 Kate (our own model)", "year": 1937,
    "hull": [(4.1, 0.0, 0.03, 0.03), (4.075, 0.0, 0.075, 0.075), (3.92, 0.0, 0.08, 0.08), (3.9, 0.0, 0.12, 0.12), (3.74, 0.0, 0.12, 0.12), (3.72, 0.0, 0.06, 0.06),
             (3.62, 0.0, 0.09, 0.09), (3.6, 0.0, 0.2, 0.2), (3.585, 0.0, 0.433, 0.433), (3.58, 0.0, 0.535, 0.535), (3.565, 0.0, 0.585, 0.585), (3.54, 0.0, 0.62, 0.62),
             (3.5, 0.0, 0.662, 0.662), (3.45, 0.0, 0.701, 0.701), (3.4, 0.0, 0.727, 0.727), (3.3, 0.0, 0.751, 0.751), (3.2, 0.0, 0.747, 0.747), (3.0, 0.0, 0.719, 0.719),
             (2.8, 0.0, 0.675, 0.675), (2.64, 0.0, 0.63, 0.63),
             (2.61, -0.004, 0.555, 0.617), (2.4, 0.006, 0.573, 0.646), (2.2, 0.003, 0.59, 0.673), (2.13, 0.004, 0.59, 0.686), (2.0, 0.006, 0.59, 0.694),
             (1.7, 0.001, 0.59, 0.699), (1.03, -0.013, 0.59, 0.703), (0.46, -0.017, 0.589, 0.699), (-0.14, -0.016, 0.585, 0.689), (-0.76, -0.014, 0.565, 0.674),
             (-1.33, -0.008, 0.538, 0.657), (-1.5, -0.005, 0.529, 0.65), (-1.75, -0.001, 0.517, 0.641), (-2.0, 0.0, 0.5, 0.626), (-2.4, -0.002, 0.476, 0.597),
             (-2.6, -0.002, 0.456, 0.581), (-3.0, -0.002, 0.421, 0.546), (-3.4, -0.002, 0.386, 0.503), (-3.8, -0.004, 0.343, 0.458), (-4.2, -0.006, 0.3, 0.406),
             (-4.6, 0.011, 0.25, 0.364), (-5.0, 0.025, 0.2, 0.32), (-5.4, 0.053, 0.145, 0.277), (-5.8, 0.085, 0.09, 0.235), (-5.95, 0.05, 0.065, 0.17),
             (-6.05, 0.005, 0.04, 0.094), (-6.15, 0.007, 0.02, 0.041), (-6.2, 0.02, 0.008, 0.01)],
    "square": 2.2, "screw": 1.62, "blades": 3, "nose_back": 2.63, "spinner": 5, "engine": "radial", "lip": 3.585,
    "hoods": fighters.hood_of([(_KATE_SCREEN[0], 0.7, _KATE_SCREEN[1], 0.01), (_KATE_SCREEN[2], 0.62, 0.345, _KATE_SCREEN[3] - 0.62)]
                              + [(x, sl, w, t - sl) for rows in _KATE_HOODS for x, sl, w, sh, t in rows[:2]]
                              + [(x, sl, w, t - sl) for x, sl, w, sh, t in _KATE_HOODS[-1][2:] + _KATE_END[1:]]),
    "canopy": _kate_canopy,
    "opening": (1.7, -1.5, 60.0),                           # (her cockpit, open under the glass from her panel to behind her gunner)
    "frames_paint": "upper", "lines": (0.6, 0.45),
    # (her wing: straight-tapered and long, flat out to 2.3 m and five degrees of dihedral beyond; seventeen per cent
    #  thick at the root; its leading edge eased and its trailing edge swept back into her side)
    "wing": [(0.0, 2.0, -2.33, -0.468, 0.68), (0.5, 2.0, -2.326, -0.468, 0.68), (0.55, 1.99, -2.143, -0.468, 0.68), (0.6, 1.968, -1.968, -0.468, 0.68),
             (0.65, 1.945, -1.812, -0.468, 0.68), (0.7, 1.937, -1.692, -0.468, 0.679), (0.75, 1.933, -1.583, -0.468, 0.678), (0.8, 1.93, -1.493, -0.467, 0.677),
             (0.9, 1.922, -1.365, -0.464, 0.67), (1.0, 1.914, -1.291, -0.46, 0.663), (1.2, 1.898, -1.252, -0.453, 0.649), (1.5, 1.875, -1.2, -0.442, 0.627),
             (2.0, 1.832, -1.112, -0.426, 0.594), (2.3, 1.809, -1.06, -0.415, 0.572), (2.5, 1.793, -1.023, -0.393, 0.557), (2.7, 1.778, -0.988, -0.366, 0.544),
             (3.0, 1.754, -0.937, -0.326, 0.517), (3.5, 1.712, -0.848, -0.26, 0.472), (4.0, 1.673, -0.758, -0.193, 0.431), (4.5, 1.634, -0.665, -0.125, 0.391),
             (5.0, 1.591, -0.575, -0.06, 0.345), (5.5, 1.552, -0.482, 0.008, 0.305), (6.0, 1.513, -0.393, 0.071, 0.264), (6.5, 1.471, -0.299, 0.138, 0.222),
             (6.8, 1.447, -0.218, 0.179, 0.197), (7.0, 1.43, -0.143, 0.205, 0.181), (7.16, 1.401, -0.062, 0.226, 0.167), (7.28, 1.369, 0.02, 0.242, 0.156),
             (7.4, 1.327, 0.121, 0.258, 0.145), (7.52, 1.268, 0.253, 0.274, 0.13), (7.58, 1.229, 0.335, 0.281, 0.118), (7.64, 1.183, 0.436, 0.289, 0.1),
             (7.7, 1.113, 0.584, 0.297, 0.075), (7.74, 1.03, 0.76, 0.302, 0.045), (7.759, 0.93, 0.9, 0.305, 0.01)],
    "aileron": (0.545, 0.92, 0.73), "flap": (0.171, 0.454, 0.82),
    "tail": [(0.0, -4.25, -6.0, -0.012, 0.2), (0.3, -4.306, -5.994, -0.012, 0.2), (0.5, -4.411, -5.971, -0.012, 0.2), (0.8, -4.508, -5.935, -0.012, 0.199),
             (1.0, -4.574, -5.912, -0.012, 0.186), (1.5, -4.737, -5.854, -0.012, 0.145), (1.8, -4.831, -5.819, -0.012, 0.118), (2.0, -4.897, -5.795, -0.012, 0.1),
             (2.2, -4.983, -5.768, -0.012, 0.082), (2.3, -5.064, -5.729, -0.012, 0.07), (2.4, -5.196, -5.644, -0.012, 0.05), (2.44, -5.29, -5.56, -0.012, 0.03),
             (2.462, -5.4, -5.45, -0.012, 0.01)],
    # (her fin from inside her fuselage up: a short fillet into its straight raked leading edge, the rudder rounded over its top)
    "fin": [(0, -4.2, -5.88, 0.2, 0.1), (0, -4.22, -5.88, 0.38, 0.1), (0, -4.25, -5.9, 0.411, 0.1), (0, -4.3, -5.9, 0.446, 0.1), (0, -4.333, -5.896, 0.5, 0.1),
            (0, -4.388, -5.884, 0.6, 0.098), (0, -4.446, -5.877, 0.7, 0.096), (0, -4.501, -5.865, 0.8, 0.093), (0, -4.559, -5.853, 0.9, 0.09),
            (0, -4.614, -5.841, 1.0, 0.087), (0, -4.672, -5.83, 1.1, 0.083), (0, -4.727, -5.818, 1.2, 0.079), (0, -4.781, -5.806, 1.3, 0.074),
            (0, -4.84, -5.779, 1.4, 0.068), (0, -4.898, -5.728, 1.5, 0.06), (0, -4.961, -5.651, 1.6, 0.05), (0, -5.011, -5.592, 1.65, 0.043),
            (0, -5.085, -5.51, 1.7, 0.034), (0, -5.171, -5.413, 1.74, 0.022), (0, -5.25, -5.33, 1.755, 0.01)],
    "patch": 0.0, "colours": dict(_PALE), "well": (1.5, 0.42, 0.42), "pitot": True, "tailwheel": (-5.74, -0.46), "mast": (-0.75, 0.9, 1.53, 0.0),
    # (the oil cooler's mouth under her, behind the cowling)
    "scoops": [([(2.72, -0.6, 0.08, 0.03), (2.62, -0.63, 0.14, 0.07), (2.42, -0.645, 0.14, 0.07), (2.26, -0.655, 0.07, 0.03)], 0.0, 2.6)],
    # (her torpedo; the one heavy armour-piercing bomb she carried instead for bombing from high up; her rear gunner's gun)
    "extras": _kate_torpedo() + [
        ("body", ([(1.3, -0.96, 0.03, 0.03), (1.0, -0.96, 0.17, 0.17), (0.5, -0.96, 0.205, 0.205), (-0.4, -0.96, 0.19, 0.19), (-1.0, -0.96, 0.07, 0.07), (-1.15, -0.96, 0.03, 0.03)], 0.0, 2.0, "bomb")),
        ("rod", ((-0.85, 0.0, -0.96), (-1.15, 0.0, -0.96), 0.2, 0.2, "bomb")),
        ("rod", ((-1.58, 0.0, 0.7), (-2.3, 0.0, 0.78), 0.022, 0.018, "steel"))],
    "side_marks": [(-2.9, 0.36, "JAP-PLAIN")], "wing_marks": [(5.4, 0.5, 0.7, "JAP-PLAIN", "JAP-PLAIN", True, True)], "code": (-3.9, 0.3),
}
fitout.SEATS["Kate"] = [(0, 1.27, 0.0, 0.0), (0, 0.15, 0.0, 0.0), (0, -0.95, 0.0, 180.0)]


def one_coat(p):
    """A coat with no second colour, on a machine whose cowling is painted with the rest of her."""
    return -1.0


# The US Navy's blue-grey over light grey of 1941 and 1942, the star without bars.
_BLUE_GREY = {"upper": "566A78", "upper2": "566A78", "under": "A8AEAD", "nose": "566A78", "boss": "2A2B2D"}
carrier(carrier_dauntless)                                  # (her own dive bomber and torpedo bomber, in the game's order)
carrier(carrier_avenger)
carrier(carrier_devastator)
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
             (-12.3, 0.303, 0.42, 0.742), (-12.58, 0.318, 0.411, 0.729), (-12.86, 0.332, 0.402, 0.716), (-13.2, 0.33, 0.385, 0.7), (-13.26, 0.14, 0.375, 0.48), (-13.5, 0.13, 0.36, 0.44),
             (-13.78, 0.11, 0.35, 0.41), (-14.2, 0.09, 0.3, 0.36), (-14.36, 0.085, 0.24, 0.29)],
    # (the tail gunner's cabin under her rudder is a box of flat windows standing on her tail cone, not a round hood:
    #  fitout.cab_hull. Its roof at 0.968, just over his helmet and on a row of her fin, so that her fin and the foot of
    #  her rudder meet it with no gap (in_cab); its foot at 27 degrees, its roof's edge at 54: her rings stand on both.
    #  Behind it her cone is as low as it is further aft, so that its rear window stands over the cone)
    "cab": (-11.2, -11.36, -13.2, -13.26, 0.968, 27.0, 54.0),
    "square": 2.0, "screw": 1.76, "blades": 3, "glazed": (7.53, 7.53), "nose_frames": ((7.53,), 0), "nose_bar": 0.6, "nose_cap": 8.2, "nose_frames_paint": "upper",
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
    # (her olive drab as the sun left it: palest on her wings and back, panel by panel; neutral grey beneath)
    "patch": 2.6, "colours": {"upper": "4C4B35", "upper2": "77735A", "under": "84878A", "nose": "4C4B35", "boss": "171717"},
    "engines": [(3.06, 3.62, -0.1, True, 2.55), (6.08, 3.16, 0.05, True, 2.05)], "pitot": True, "under_from": -0.6, "frames_paint": "upper",
    "engine_face": 9, "blade_tips": True, "framed_windows": True, "fifties": True,
    # (her rudder: a narrow one, about a quarter of her fin's breadth behind its hinge, as in Brian's pictures)
    "rudder": (0.06, 1.0, 0.74),
    "tailwheel": (-8.63, -0.9), "mast": (-0.6, 1.5, 1.95),
    "extras": [("body", (_B17_FILLET[:7], 0.0, 2.0, "coat")), ("body", (_B17_FILLET[6:], 0.0, 2.0, "coat")),
               # (the chin turret, the fairing behind it, and its pair of guns)
               ("ball", ((7.1, 0.0, -0.9), (0.42, 0.44, 0.37), "upper")),
               ("body", ([(7.0, -0.88, 0.42, 0.36), (6.6, -0.87, 0.4, 0.3), (6.2, -0.86, 0.33, 0.22), (5.8, -0.88, 0.24, 0.14), (5.5, -0.9, 0.12, 0.07)], 0.0, 2.0, "upper")),
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
EXTRA_PLANES["B17"]["graded"] = sun_faded(EXTRA_PLANES["B17"]["hull"], EXTRA_PLANES["B17"]["square"], -8.3)

# The He 111 again, to the lines of the real machine (Brian, 2026-11-19: "use this 3d model is our bench mark for all
# airplanes"), measured from a three-view drawing (Wikimedia Commons, "Heinkel He 111 H-1 3-view line drawing",
# Kaboldy, CC BY-SA 3.0; 165 px to the metre, set by her span of 22.6 m; measured, not traced). What changed from the
# game's own numbers: her fuselage is an oval taller than it is wide, and its nose is a bullet of glass with no step,
# glass all round ahead of her pilot's feet and down to a sill beside him (`glazed_sill`), where he sits to port with
# his hands on his wheel and the man at the nose gun kneels ahead of him; her engines hang low in nacelles of her own
# lines (`nacelle`), each with its radiator bath under it and its wheel's doors behind that; her wing's middle is
# straight and thick between her fuselage and her engines, its trailing edge cut forward there, and its outer panels
# taper and rise; her tailplane and fin are the broad ovals they were; the gunner on her back sits facing aft under a
# glazed hood open behind him; and the bath under her has a gun at each end. Her coat is a splinter of two dark
# greens, parted by straight lines, over pale blue; crosses on her wings and fuselage only, and her fin plain (the
# art director, 2026-11-19).
_HE111_HULL = [(7.0, -0.02, 0.02, 0.02), (6.97, -0.02, 0.13, 0.12), (6.9, -0.021, 0.29, 0.306), (6.8, -0.022, 0.38, 0.398), (6.65, -0.02, 0.45, 0.47), (6.4, -0.015, 0.545, 0.605),
               (6.2, -0.01, 0.627, 0.70), (5.94, 0.0, 0.705, 0.80), (5.6, 0.01, 0.772, 0.90), (5.27, 0.018, 0.816, 0.962), (5.0, 0.025, 0.839, 1.017), (4.7, 0.032, 0.856, 1.046),
               (4.42, 0.04, 0.868, 1.067), (4.13, 0.043, 0.875, 1.073), (3.0, 0.035, 0.89, 1.085), (2.15, 0.03, 0.89, 1.09), (0.95, 0.025, 0.885, 1.075), (0.4, 0.033, 0.88, 1.063),
               (-0.2, 0.042, 0.86, 1.042), (-1.0, 0.049, 0.79, 1.0), (-1.6, 0.052, 0.757, 0.96), (-2.0, 0.058, 0.742, 0.924), (-3.0, 0.034, 0.697, 0.87), (-4.0, 0.022, 0.636, 0.79),
               (-5.0, 0.022, 0.563, 0.688), (-6.0, 0.025, 0.46, 0.575), (-6.4, 0.023, 0.42, 0.532), (-7.0, 0.035, 0.34, 0.435), (-7.8, 0.005, 0.26, 0.35), (-8.4, 0.0, 0.19, 0.254),
               (-8.8, 0.0, 0.139, 0.142), (-9.0, -0.005, 0.076, 0.073), (-9.12, 0.0, 0.01, 0.01)]
# (the hood over the gunner on her back: along her, half its width, the height of its top; round in front as a
#  fairing, then straight-sided back to its open end)
_HE111_HOOD = [(2.28, 0.3, 1.2), (2.2, 0.42, 1.26), (2.1, 0.52, 1.335), (2.0, 0.575, 1.385), (1.9, 0.605, 1.415), (1.8, 0.62, 1.423), (1.4, 0.62, 1.423), (1.0, 0.62, 1.423)]
_HE111_BOX = []


def _he111_hull():
    if not _HE111_BOX:
        _HE111_BOX.append(game()["Body"](_HE111_HULL, 2.2))
    return _HE111_BOX[0]


def _he111_canopy(hull):
    skirt = fighters.SKIRT

    def sec(x, ys, top):
        sill = fitout.body_top(hull, x, ys) + 0.055
        return _val_section(hull, x, sill, ys, None, max(top, sill + 0.05), None, 0.8)
    n = len(sec(*_HE111_HOOD[0]))
    foot = [fighters.deck(hull, 2.37, 0.1)] * (n - 1) + [fighters.deck(hull, 2.37, 0.0)]
    k = next(i for i, r in enumerate(_HE111_HOOD) if r[1] >= 0.62)
    return [{"sections": [(2.37, foot)] + [(x, sec(x, ys, top)) for x, ys, top in _HE111_HOOD[:k + 1]], "skirt": skirt, "hoops": [3, k + 1], "rails": [skirt, n - 1], "bar": 0.018},
            {"sections": [(x, sec(x, ys, top)) for x, ys, top in _HE111_HOOD[k:]], "skirt": skirt, "hoops": [1, 2], "rails": [skirt, 3, n - 1], "bar": 0.018}]


# Her splinter: as the Me 262's (see _ME262_ZONES), her own lines. On her fin a line is in (along her, height).
_HE111_ZONES = {
    "starboard": (False, [((4.74, 1.6), (0.2, 2.9)), ((4.5, 4.3), (-0.05, 3.9)), ((3.95, 6.6), (0.3, 8.2)), ((3.3, 9.3), (0.4, 9.9))]),
    "port": (True, [((4.74, -2.6), (0.1, -1.5)), ((4.4, -4.9), (0.0, -6.2)), ((3.7, -7.7), (0.3, -7.1)), ((3.2, -9.6), (0.42, -10.4))]),
    "spine": (True, [((3.4, 1.0), (2.6, -1.0)), ((0.6, 1.0), (1.3, -1.0)), ((-1.4, 1.0), (-2.3, -1.0)), ((-3.9, 1.0), (-3.2, -1.0)), ((-5.6, 1.0), (-6.3, -1.0))]),
    "tail starboard": (True, [((-6.5, 1.9), (-8.8, 1.2))]),
    "tail port": (False, [((-6.35, -1.0), (-8.9, -2.1))]),
    "fin": (False, [((-6.6, 1.2), (-8.8, 2.0))]),
}
_HE111_SOFT = 0.09                                          # (half the breadth of a sprayed edge, in metres)


def splinter(zones, zone, u, v, soft):
    """How far a point (u along her, v out to starboard) is into a zone's paler colour, as the coat's field takes it:
    each line it is beyond turns the colour over, and an edge is `soft` broad each side of its line."""
    pale, lines = zones[zone]
    hand = -1.0 if zone.endswith("port") else 1.0
    near = 9.0
    for (ax, ay), (bx, by) in lines:
        d = ((bx - ax) * (v - ay) - (by - ay) * (u - ax)) / math.hypot(bx - ax, by - ay) * hand
        if d < 0.0:
            pale = not pale
        near = min(near, abs(d))
    return max(-2.0, min(2.0, (near if pale else -near) / soft)) * 0.045


def _he111_coat(p):
    """Her coat: bands across her fuselage from her nose to her tail, wedges out along each wing and each tailplane,
    and one line across her fin."""
    hull = _he111_hull()
    x = min(max(p.x, hull.back), hull.front)
    zc, h, w = hull.z(-x), hull.h(-x), hull.w(-x)
    if abs(p.y) < 0.14 and p.x < -6.2 and p.z > zc + h - 0.02:
        return splinter(_HE111_ZONES, "fin", p.x, p.z, _HE111_SOFT)
    if abs(p.y) <= w + 0.04 and abs(p.z - zc) <= h + 0.5:
        return splinter(_HE111_ZONES, "spine", p.x, p.y, _HE111_SOFT)       # (her fuselage, the hood on her back, the bath under her)
    zone = ("tail " if p.x < -6.0 else "") + ("starboard" if p.y > 0.0 else "port")
    return splinter(_HE111_ZONES, zone, p.x, p.y, _HE111_SOFT)


def _he111_bath():
    """The bath under her, with four round windows down each side (a gun out of each end of it: see her extras)."""
    st = [(1.06, -1.04, 0.2, 0.05), (0.92, -1.12, 0.33, 0.13), (0.6, -1.17, 0.4, 0.21), (0.3, -1.185, 0.42, 0.2), (-0.4, -1.185, 0.42, 0.2), (-0.95, -1.16, 0.4, 0.19),
          (-1.2, -1.12, 0.36, 0.15), (-1.4, -1.07, 0.27, 0.09), (-1.46, -1.04, 0.15, 0.04)]
    return [("body", (st, 0.0, 2.3, "under"))] + [("rod", ((x, 0.4, -1.2), (x, 0.428, -1.2), 0.07, 0.07, "glass")) for x in (0.38, -0.2, -0.47, -0.78)]


EXTRA_PLANES["He111"] = {
    "title": "He 111H (our own model)",
    "hull": _HE111_HULL, "square": 2.2, "screw": 1.72, "blades": 3,
    # (glass all round ahead of 5.94, and from there back to her cabin's last frame down to the sill; the round pane at
    #  her tip, then ten ribs to the first hoop; over her cabin the frames run along her at the heads of its side
    #  windows, each side of her roof and down its middle)
    "glazed": (5.94, 4.13), "glazed_sill": 9.0, "windows_back": True, "nose_frames": ((6.65, 6.3, 5.94, 5.27, 4.42, 4.15), 10), "nose_cap": 6.65, "nose_bar": 0.5,
    "nose_frames_paint": "upper", "cabin_ribs": (45.0, 72.0, 90.0, 108.0, 135.0), "nose_wheel": True, "nose_gunner": True,
    "glass_bands": [(-0.13, -0.58, 0.175, 0.36)],
    "hoods": fighters.hood_of([(2.37, 1.1, 0.1, 0.01)] + [(x, 0.88, ys, top - 0.88) for x, ys, top in _HE111_HOOD]),
    "canopy": _he111_canopy, "opening": (2.15, 0.95, 62.0), "opening_plain": True, "frames_paint": "upper", "lines": (0.6, 0.45),
    "wing": [(0.0, 4.741, -0.95, -0.4, 1.15), (0.8, 4.741, -0.75, -0.4, 1.15), (0.88, 4.741, -0.5, -0.4, 1.15), (0.95, 4.741, -0.3, -0.4, 1.15), (1.04, 4.741, 0.0, -0.4, 1.15),
             (1.13, 4.741, 0.2, -0.4, 1.15), (1.25, 4.741, 0.295, -0.4, 1.15), (1.5, 4.741, 0.295, -0.4, 1.15), (1.8, 4.741, 0.21, -0.4, 1.15), (2.1, 4.741, 0.101, -0.395, 1.13),
             (2.4, 4.741, 0.01, -0.385, 1.08), (2.7, 4.741, -0.051, -0.365, 1.0), (3.0, 4.741, -0.087, -0.34, 0.93), (3.37, 4.741, -0.099, -0.31, 0.84), (3.6, 4.686, -0.087, -0.3, 0.81),
             (4.2, 4.535, -0.045, -0.227, 0.756), (6.0, 4.092, 0.089, 0.002, 0.614), (9.0, 3.36, 0.307, 0.389, 0.371), (10.2, 3.063, 0.404, 0.542, 0.272), (10.5, 2.966, 0.501, 0.58, 0.245),
             (10.8, 2.784, 0.676, 0.616, 0.2), (11.1, 2.445, 1.021, 0.658, 0.12), (11.25, 2.1, 1.35, 0.68, 0.05), (11.3, 1.85, 1.6, 0.69, 0.01)],
    "aileron": (0.56, 0.93, 0.76), "flap": (0.3, 0.55, 0.78),
    "tail": [(0.0, -6.19, -9.02, -0.01, 0.33), (0.6, -6.211, -8.997, -0.01, 0.31), (1.2, -6.241, -8.931, -0.01, 0.28), (1.8, -6.32, -8.846, -0.01, 0.25), (2.4, -6.459, -8.767, -0.01, 0.21),
             (3.0, -6.671, -8.64, -0.01, 0.16), (3.3, -6.823, -8.537, -0.01, 0.13), (3.6, -7.035, -8.373, -0.01, 0.1), (3.8, -7.25, -8.15, -0.01, 0.06), (3.9, -7.41, -7.992, -0.01, 0.04),
             (3.96, -7.6, -7.8, -0.01, 0.01)],
    "fin": [(0, -6.25, -8.9, 0.45, 0.2), (0, -6.368, -8.87, 0.7, 0.2), (0, -6.447, -8.834, 1.0, 0.19), (0, -6.556, -8.773, 1.4, 0.17), (0, -6.671, -8.682, 1.8, 0.15),
            (0, -6.756, -8.622, 2.0, 0.14), (0, -6.871, -8.543, 2.2, 0.12), (0, -7.023, -8.44, 2.4, 0.1), (0, -7.12, -8.373, 2.5, 0.09), (0, -7.235, -8.295, 2.6, 0.075),
            (0, -7.386, -8.167, 2.7, 0.055), (0, -7.495, -8.077, 2.75, 0.04), (0, -7.671, -7.901, 2.8, 0.01)],
    "patch": 1.0, "graded": _he111_coat, "grain": {"wing": 0.2, "tail": 0.2}, "under_from": -0.5,
    # (her two greens close together and dark: the art director, 2026-11-19, "her upper coat reads as one very dark
    #  green, with the splinter showing only up close or in raking light")
    "colours": {"upper": "343A2E", "upper2": "404A36", "under": "8FB0BF", "nose": "343A2E", "boss": "2A2F26"},
    # (each engine: its airscrew 2.58 out and 0.4 below her middle line; the nacelle from its spinner back over and
    #  under her wing to her trailing edge, deepest where its wheel's doors are)
    "engines": [(2.583, 4.23, -0.4, False, 4.3)],
    "nacelle": [(2.55, 0.0, 0.04, 0.04), (2.4, 0.0, 0.2, 0.2), (2.25, 0.0, 0.3, 0.3), (2.1, 0.0, 0.36, 0.36), (2.05, 0.01, 0.42, 0.4), (1.75, -0.005, 0.5, 0.475),
                (1.3, 0.0, 0.53, 0.5), (0.8, -0.005, 0.55, 0.545), (0.5, -0.095, 0.56, 0.655), (0.0, -0.14, 0.56, 0.72), (-1.0, -0.15, 0.54, 0.72), (-1.8, -0.1, 0.48, 0.62),
                (-2.6, 0.01, 0.36, 0.41), (-3.3, 0.08, 0.2, 0.2), (-4.2, 0.05, 0.04, 0.03)],
    "scoops": [([(6.02, -0.84, 0.28, 0.07), (5.95, -0.86, 0.36, 0.11), (5.4, -0.88, 0.38, 0.11), (5.02, -0.9, 0.3, 0.08)], 2.583, 2.6)],
    "pitot": True, "tailwheel": (-7.35, -0.72), "mast": (0.0, 1.09, 1.9, 0.0),
    # (her guns, in the order the battle knows them: her nose, her back, the bath's aft end, its fore end)
    "extras": _he111_bath() + [
        ("rod", ((6.95, 0.0, -0.14), (7.6, 0.0, -0.22), 0.022, 0.018, "steel")), ("rod", ((0.98, 0.0, 1.3), (0.2, 0.0, 1.52), 0.022, 0.018, "steel")),
        ("rod", ((-1.3, 0.0, -1.1), (-2.05, 0.0, -1.42), 0.022, 0.018, "steel")), ("rod", ((0.95, 0.0, -1.2), (1.65, 0.0, -1.43), 0.022, 0.018, "steel"))],
    "side_marks": [(-3.4, 0.5, "GER")], "wing_marks": [(7.6, 1.95, 0.75, "GER", "GER", True, True)], "code": (-2.2, 0.45),
}
fitout.SEATS["He111"] = [(0, 1.58, 0.0, 180.0)]
fitout.NOSE_CREW["He111"] = [(4.95, 0.32, None, 0.0), (6.28, -0.1, 0.06, 0.0)]


# The Lancaster again, to the lines of the real machine (Brian, 2026-11-19: "use this 3d model is our bench mark for all
# airplanes"), measured from a three-view drawing (Wikimedia Commons, "Avro Lancaster 3-view line drawings", Emoscopes,
# GFDL and CC BY 3.0; a small sheet, 21.6 px to the metre, set by her span of 31.09 m; the plan turned upright by her
# wing tips and the tail-down side view levelled by the slope of her back; measured, not traced; no aerial wire). What
# changed from the game's own numbers: her fuselage is a deep slab, flat along its top from her cabin to her tail with
# its underside sweeping up behind the bay; her nose carries the bomb aimer's blister under the front turret; her
# cabin is a long greenhouse of flat panes standing on her sills, open underneath, its after part narrower on a
# rising sill, with the pilot to port at his wheel; her wing's middle is straight and its outer panels taper and rise;
# her four engines hang in deep nacelles of their own lines with a radiator under each spinner (`nacelles`: the inner
# pair run back past the trailing edge, the outer end under the wing); her fins are tall ovals at the tips of a broad
# tailplane set high on her; and her tail turret has four guns, a hand of its gunner on each pair. Her night colours
# are as they were, and by the art director's ruling there is no rivet on her black undersides (`rivets_under`).
_LANC_EDGE = 27.0                                           # (her cabin is open from here up, round her from her beam: a ring of her skin's own)
_LANC_CABIN = (0.70, 0.60, 1.35, 1.62)                      # (the sill of its side panes, how far out and how high their top frame, its roof)
# (its after part: where along her, half the width at its sill, the sill, the frame at its shoulder (out, high), its roof)
_LANC_AFTER = [(6.2, 0.62, 0.95, 0.5, 1.37, 1.62), (5.7, 0.52, 0.99, 0.43, 1.37, 1.6), (5.2, 0.38, 1.07, 0.32, 1.35, 1.54),
               (4.92, 0.24, 1.15, 0.2, 1.31, 1.47), (4.72, 0.1, 1.21, 0.08, 1.27, 1.37)]


def _lanc_section(hull, x, ys=None, sill=None, shoulder=None, top=None, fair=2):
    """Half a section of her greenhouse: its foot on the edge of her skin where her cabin is open under it, metal from
    there up to the sill where the glass begins (`fair` points between, lying where her skin would: over her cabin's
    after part her skin runs in and up to a narrower sill), a flat side pane leaning in to the frame at its shoulder,
    and its roof over a frame 0.19 out to her middle line. With no more than x: a section of the cabin's fore part."""
    a0 = math.radians(_LANC_EDGE)
    foot = hull.at(x, a0)
    yb, zb = foot.y + 0.004, foot.z
    if ys is None:
        ys, (sill, out, high, top) = yb - 0.004, _LANC_CABIN
        shoulder = (out, high)
        between = [(yb + (ys - yb) * k / (fair + 1.0), zb + (sill - zb) * k / (fair + 1.0)) for k in range(1, fair + 1)]
    else:
        a1 = math.acos(min(ys / hull.w(-x), 1.0) ** (1.0 / hull.power))          # (where round her that sill is)
        between = [hull.at(x, a0 + (a1 - a0) * k / (fair + 1.0), 0.004) for k in range(1, fair + 1)]
        between = [(q.y, q.z) for q in between]
    yr = min(0.19, 0.55 * shoulder[0])
    return [(yb - 0.03, zb - 0.06), (yb, zb)] + between + [(ys, sill), shoulder, (yr, shoulder[1] + 0.8 * (top - shoulder[1])), (0.0, top)]


def _lanc_canopy(hull):
    skirt = fighters.SKIRT
    # her windscreen: from its foot along the edge of her skin and across her deck, back to the cabin's first frame
    x0 = 7.9
    edge = hull.at(x0, math.radians(_LANC_EDGE))
    foot = [(edge.y - 0.026, edge.z - 0.06), (edge.y + 0.004, edge.z), (edge.y, edge.z + 0.02),
            fighters.deck(hull, x0, _LANC_CABIN[1]), fighters.deck(hull, x0, 0.19), fighters.deck(hull, x0, 0.0)]
    screen = {"sections": [(x0, foot), (7.45, _lanc_section(hull, 7.45, fair=0))], "skirt": skirt, "hoops": [0, 1], "rails": [skirt, 3, 5], "bar": 0.022}
    # her cabin: three frames round its fore part, a step in to its after part, which narrows on a rising sill to its
    # end; her skin runs up under that sill (two more points of metal in each section's foot)
    run = [(x, _lanc_section(hull, x)) for x in (7.45, 6.97, 6.25)]
    run += [(x, _lanc_section(hull, x, ys, sill, (out, high), top)) for x, ys, sill, out, high, top in _LANC_AFTER]
    cabin = {"sections": run, "skirt": skirt + 2, "hoops": [0, 1, 2, 4, 5, 6], "rails": [skirt + 2, 5, 6, 7], "bar": 0.02, "cap": True}
    return [screen, cabin]


# (one Merlin's nacelle from its spinner back to her wing's leading edge: the cowling, deep over the radiator under it)
_LANC_EGG = [(2.55, 0.0, 0.04, 0.04), (2.42, 0.0, 0.19, 0.19), (2.27, 0.0, 0.28, 0.28), (2.1, 0.0, 0.34, 0.34), (2.05, -0.11, 0.42, 0.51), (1.8, -0.28, 0.48, 0.72),
             (1.4, -0.34, 0.52, 0.8), (0.6, -0.35, 0.55, 0.83), (-0.2, -0.35, 0.56, 0.85)]
_LANC_INNER = _LANC_EGG + [(-1.5, -0.36, 0.56, 0.86), (-2.8, -0.33, 0.52, 0.82), (-3.8, -0.2, 0.42, 0.65), (-4.6, -0.12, 0.25, 0.33), (-5.2, -0.1, 0.05, 0.05)]
_LANC_OUTER = _LANC_EGG + [(-1.2, -0.28, 0.54, 0.78), (-2.0, -0.12, 0.46, 0.6), (-2.8, 0.05, 0.32, 0.38), (-3.5, 0.15, 0.12, 0.15), (-3.8, 0.18, 0.03, 0.03)]


def _lanc_mouth(lead, z, out):
    """The radiator's mouth under a spinner, dark, its lip a little proud of the cowling."""
    return ([(lead + 2.02, z - 0.63, 0.33, 0.27), (lead + 1.94, z - 0.65, 0.385, 0.33), (lead + 1.6, z - 0.69, 0.4, 0.33), (lead + 1.2, z - 0.67, 0.36, 0.25)], out, 2.6)


EXTRA_PLANES["Lancaster"] = {
    "title": "Lancaster B.I (our own model)",
    # (the bomb aimer's blister is a dome ahead of this; then her nose up to the deck ahead of the windscreen, her
    #  cabin's sides (her skin is open above them), her slab of a fuselage, flat on top, its underside sweeping up
    #  behind the bay, and the tray under her tail turret)
    "hull": [(10.0, -0.42, 0.22, 0.25), (9.9, -0.42, 0.44, 0.47), (9.75, -0.42, 0.535, 0.56), (9.6, -0.43, 0.59, 0.6), (9.3, -0.415, 0.7, 0.715), (9.0, -0.225, 0.78, 0.965),
             (8.8, -0.125, 0.82, 1.095), (8.4, -0.09, 0.86, 1.17), (8.1, -0.07, 0.88, 1.21), (7.9, -0.085, 0.885, 1.225), (7.4, -0.1, 0.885, 1.22), (6.2, -0.1225, 0.885, 1.2375),
             (5.4, -0.13, 0.885, 1.21), (4.9, -0.08, 0.885, 1.25), (4.3, -0.005, 0.885, 1.315), (2.0, 0.0, 0.885, 1.315), (-2.4, 0.005, 0.885, 1.31), (-3.0, 0.04, 0.875, 1.275),
             (-4.0, 0.08, 0.86, 1.235), (-5.1, 0.125, 0.82, 1.19), (-6.0, 0.205, 0.79, 1.11), (-6.9, 0.255, 0.76, 1.055), (-7.8, 0.31, 0.715, 0.99), (-9.0, 0.355, 0.64, 0.925),
             (-9.6, 0.39, 0.6, 0.88), (-9.85, 0.4, 0.565, 0.86), (-10.0, -0.15, 0.5, 0.3), (-10.3, -0.12, 0.42, 0.22), (-10.5, -0.08, 0.3, 0.12), (-10.55, -0.06, 0.15, 0.05)],
    "square": 2.8, "screw": 1.98, "blades": 3, "under_from": 0.45, "fins_under": True, "rivets_under": False,
    "hoods": fighters.hood_of([(7.9, 0.7, 0.3, 0.44), (7.45, 0.7, 0.815, 0.92), (6.2, 0.7, 0.815, 0.92), (5.2, 0.9, 0.5, 0.64), (4.72, 1.1, 0.2, 0.27)]),
    "canopy": _lanc_canopy, "opening": (7.9, 4.9, 30.0), "opening_cabin": True, "pilot_wheel": True, "frames_paint": "upper",
    # (five small windows along each side behind her wing)
    "windows": [(x, x - 0.28, 0.64, 0.76) for x in (0.05, -0.75, -1.55, -2.45, -3.25)],
    # (straight to just outside her inner engines, then tapering and rising to her tips)
    "wing": [(0.0, 4.66, -0.09, 0.04, 1.0), (4.35, 4.66, -0.09, 0.04, 1.0), (5.4, 4.52, 0.0, 0.16, 0.93), (6.6, 4.37, 0.125, 0.298, 0.84), (8.4, 4.14, 0.3, 0.482, 0.75),
             (9.6, 3.99, 0.42, 0.62, 0.66), (10.8, 3.84, 0.54, 0.749, 0.587), (12.0, 3.685, 0.66, 0.883, 0.504), (13.2, 3.53, 0.78, 1.0, 0.431), (14.4, 3.375, 0.925, 1.135, 0.347),
             (14.95, 3.24, 1.1, 1.19, 0.27), (15.3, 2.92, 1.43, 1.225, 0.17), (15.545, 2.35, 1.95, 1.25, 0.02)],
    "aileron": (0.54, 0.86, 0.78), "flap": (0.06, 0.54, 0.8),
    "tail": [(0.0, -7.07, -9.46, 0.8, 0.34), (0.95, -7.07, -9.46, 0.8, 0.33), (4.6, -7.2, -9.05, 0.8, 0.2), (5.0, -7.25, -8.95, 0.8, 0.16), (5.07, -7.3, -8.9, 0.8, 0.1)],
    # (a tall oval fin at each tip of her tailplane, as much of it under the tailplane as will clear the ground)
    "fins": [([(0.0, -7.85, -8.1, -0.98, 0.02), (0.0, -7.6, -8.38, -0.85, 0.1), (0.0, -7.4, -8.62, -0.6, 0.15), (0.0, -7.25, -8.82, -0.2, 0.18), (0.0, -7.2, -8.92, 0.4, 0.2),
               (0.0, -7.22, -8.9, 1.1, 0.2), (0.0, -7.3, -8.82, 1.7, 0.18), (0.0, -7.42, -8.7, 2.2, 0.15), (0.0, -7.6, -8.5, 2.6, 0.11), (0.0, -7.8, -8.25, 2.85, 0.06),
               (0.0, -7.95, -8.08, 2.92, 0.01)], 5.07)],
    "patch": 3.4, "colours": {"upper": "434A33", "upper2": "685A42", "under": "1B1C1E", "nose": "434A33", "boss": "171717"},
    # (each engine: its airscrew's line, the inner pair lower and further forward; the nacelles and their radiators' mouths)
    "engines": [(3.52, 4.8, -0.17, False, 5.2), (7.76, 4.18, 0.1, False, 3.8)],
    "nacelles": {3.52: _LANC_INNER, 7.76: _LANC_OUTER}, "nacelle_square": 2.6,
    "scoops": [_lanc_mouth(4.8, -0.17, 3.52), _lanc_mouth(4.18, 0.1, 7.76)],
    "pitot": True, "tailwheel": (-7.7, -1.2), "mast": (6.3, 1.6, 2.0, 0.0),
    "extras": [
        # (her nose turret, the bomb aimer's blister under it, the astrodome behind her cabin, the turret on her back
        #  in its fairing, her tail turret: and their guns in the order the battle knows them)
        ("dome", ((9.3, 0.0, 0.55), (0.47, 0.45, 0.47))), ("dome", ((9.78, 0.0, -0.4), (0.44, 0.44, 0.44))), ("dome", ((4.45, 0.0, 1.3), (0.22, 0.22, 0.2))),
        ("dome", ((-3.2, 0.0, 1.42), (0.52, 0.52, 0.54))), ("dome", ((-10.02, 0.0, 0.66), (0.53, 0.52, 0.62))),
        ("body", ([(-2.17, 1.25, 0.05, 0.05), (-2.5, 1.27, 0.4, 0.15), (-2.9, 1.29, 0.6, 0.2), (-3.2, 1.29, 0.63, 0.22), (-3.7, 1.29, 0.58, 0.2), (-4.2, 1.27, 0.35, 0.13),
                   (-4.6, 1.25, 0.05, 0.04)], 0.0, 2.0, "coat")),
        ("rod", ((9.55, 0.1, 0.62), (10.33, 0.1, 0.63), 0.022, 0.018, "steel")), ("rod", ((-3.42, 0.13, 1.6), (-4.18, 0.13, 1.84), 0.022, 0.018, "steel")),
        ("rod", ((-10.1, 0.31, 0.84), (-10.98, 0.31, 0.86), 0.022, 0.018, "steel")), ("rod", ((-10.1, 0.31, 0.46), (-10.98, 0.31, 0.44), 0.022, 0.018, "steel"))],
    # (her side roundel half a metre forward of where it was, so that her squadron letters are 0.75 m and her starboard
    #  fin hides none of them from far abeam: the art director, 2026-11-20, codes.py)
    "side_marks": [(-5.1, 0.55, "UK-A1")], "wing_marks": [(11.5, 2.1, 1.0, "UK-B", None, True, False)], "fin_marks": [(-7.75, 1.5, 0.4, "UK-FIN")], "fin_marks_at": 5.07,
    "code": (-3.9, 0.55),
}
fitout.SEATS["Lancaster"] = [(0, 7.0, 0.33, 0.0), (0, 6.55, -0.36, 0.0), (0, 5.55, 0.0, 0.0)]
fitout.SEAT_DROP["Lancaster"] = 0.08


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


def battle_hinge(surface, chords):
    """A control surface of a machine's own numbers, its hinge moved back to the next ring of the chord, where convert()
    cuts it."""
    return tuple(surface[:2]) + (next(c for c in chords if c >= surface[2] - 1e-6),)


def rudder_ends(rudder):
    """The shares of her fin's span that a ring must stand at for her rudder: its foot, and its head unless it runs to
    the top of the fin."""
    return tuple(rudder[:1]) if rudder[1] >= 1.0 else tuple(rudder[:2])


# Recognition bands (`bands` in her numbers; the art director, 2026-11-20, for the Mustang over Bremen in December 1943):
# a band of her own paint, `band` (and `band_under` beneath her), straight across each wing, across each half of her
# tailplane and round her fin and rudder, each as {"wing": (from, to) metres out from her middle line, "tail": (from,
# to) the same, "fin": (from, to) metres up}. A hard paint, laid by face, so that its edges are clean: a ring of her
# skin stands at each edge (battle_band_shares), and a point on an edge keeps her coat. Her nose band is her `nose`
# paint (ahead of `nose_back`). Without `bands` she is built as before, and her file has no band paints.
BAND_PAINTS = ("band", "band_under")
BAND_EDGE = 0.003                                           # (a point this close to a band's edge, or on it, is her coat)


def battle_band_shares(s, where, surface):
    """The shares of a flying surface's span at which a ring must stand: each edge of her band on it."""
    band = (s.get("bands") or {}).get(where)
    if not band:
        return ()
    span = surface.tip - surface.root
    return tuple((v - surface.root) / span for v in band)


def battle_bands(s, coat):
    """Her coat with her recognition bands laid over it (see BAND_PAINTS)."""
    bands = s.get("bands")
    if not bands:
        return coat
    Wing = game()["Wing"]
    wing, tail, fin = Wing(s["wing"]), Wing(s["tail"]), Wing((s.get("fins") or [(s["fin"], 0.0)])[0][0], 1.0, True)
    under_from = s.get("under_from", -0.3)

    def inside(v, band):
        return band[0] + BAND_EDGE < v < band[1] - BAND_EDGE

    def across(surface, u, x):
        u = min(max(u, surface.root), surface.tip)
        return surface.trail(u) - 0.05 < x < surface.lead(u) + 0.05

    def paint(p, n):
        y = abs(p.y)
        if ("wing" in bands and inside(y, bands["wing"]) and across(wing, y, p.x) and abs(p.z - wing.z(y)) < 0.4) or \
           ("tail" in bands and inside(y, bands["tail"]) and across(tail, y, p.x) and abs(p.z - tail.z(y)) < 0.25) or \
           ("fin" in bands and inside(p.z, bands["fin"]) and across(fin, p.z, p.x) and y < 0.2):
            return "band_under" if n.z < under_from else "band"
        return coat(p, n)
    paint.blend = coat.blend
    return paint


def head_numbers(body, lip, heads):
    """A machine's `heads` in full: (how many in a row, how many rows, how far out from her middle line their ring is,
    each head's radius, how far the front row stands back behind her cowling's lip). All but the first may be left
    off, for one row on the game's own ring, 0.075 and -0.015."""
    n, rows, ring, r, back = tuple(heads) + (1, None, 0.075, -0.015)[len(heads) - 1:]
    return int(n), int(rows), (0.62 * body.w(-(lip - 0.05)) if ring is None else ring), r, back


def battle_recess(parts, hull, s, lip, boss_back):
    """The open mouth of the radial in her nose, on the fine model of a machine with `heads`. In the game's sculptor the
    front of her skin between her airscrew's hub and her lip is a flat dark face, and a head behind it is not seen. So
    the bands of that face that look forward and lie within `mouth` of her middle line (without `mouth`, within 3 cm
    outside her ring of heads) are cut out of her skin, a ring of it at a time, and her cowling is open back to her
    engine: its inside runs back from the ring the face is cut at, as far out as her heads need and never through her
    skin; a floor closes it just behind her last row; and her engine's nose case runs back to the floor from the ring
    in front, so that a band of the face that lies inside the middle of her heads (no further out than their ring less
    half a head) is kept as the front of her nose case. All of it is her engine's dark paint. Where no band can be cut,
    her face is as it was."""
    n, rows, ring, r, back = head_numbers(hull, lip, s["heads"])
    hole, need = s.get("mouth") or ring + r + 0.03, ring + r + 0.03
    floor = lip - back - 2.0 * r * rows - 0.01
    skin = next((ob for ob in parts.objects if ob.name.split(".")[0] == "hull"), None)
    if skin is None:
        return
    bm = bmesh.new()
    bm.from_mesh(skin.data)
    bm.normal_update()
    yc, zc = hull.side, hull.z(-lip)

    def out_of(p):
        return math.hypot(p.y - yc, p.z - zc)
    rings, bands = {}, {}
    for v in bm.verts:
        rings.setdefault(round(v.co.x, 6), []).append(v)
    for f in bm.faces:
        key = tuple(sorted({round(v.co.x, 6) for v in f.verts}, reverse=True))
        if len(key) == 2:
            bands.setdefault(key, []).append(f)
    xs = sorted((x for x, vs in rings.items() if len(vs) > 2), reverse=True)
    run = []
    for x0, x1 in zip(xs, xs[1:]):
        fs = bands.get((x0, x1), [])
        clear = max(out_of(v.co) for v in rings[x0] + rings[x1]) <= ring - 0.5 * r      # (inside the middle of her heads: her nose case)
        if fs and lip - 0.06 < 0.5 * (x0 + x1) < boss_back and not clear and all(f.normal.x > 0.6 and out_of(f.calc_center_median()) < hole for f in fs):
            run.append((x0, x1, fs))
        elif run:
            break
    inner = sorted(rings[run[0][0]], key=lambda v: v.index) if run else []
    edge = sorted(rings[run[-1][1]], key=lambda v: v.index) if run else []
    if not run or len(inner) != len(edge) or floor > run[-1][1] - 0.02:
        bm.free()
        return
    edge_x, around = run[-1][1], len(edge)
    bmesh.ops.delete(bm, geom=[f for x0, x1, fs in run for f in fs], context="FACES")

    def inside(x, k):
        """Her cowling's inside at x, at the k-th point round her (as her skin's rings count them): out as far as her heads
        need, or as the mouth if that is wider; 3 cm inside her skin, and never inside the mouth nor through her skin."""
        q = hull.at(x, 2.0 * math.pi * k / around)
        d = Vector((0.0, q.y - yc, q.z - zc))
        wide = out_of(edge[k].co)
        far = max(min(max(wide, need), d.length - 0.03), min(wide, d.length - 0.004))
        return Vector((x, yc, zc)) + d.normalized() * far
    walls = [edge] + [[bm.verts.new(inside(x, k)) for k in range(around)] for x in [x for x in xs if floor + 0.01 < x < edge_x - 1.0e-4] + [floor]]
    case = [bm.verts.new(Vector((floor, v.co.y, v.co.z))) for v in inner]
    paint, dark = bm.faces.layers.int.get("paint"), _G["PAINTS"]["engine"]
    for a_, b_ in list(zip(walls, walls[1:])) + [(walls[-1], case), (case, inner)]:
        for k in range(around):
            f = bm.faces.new((a_[k], a_[(k + 1) % around], b_[(k + 1) % around], b_[k]))
            if paint is not None:
                f[paint] = dark
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(skin.data)
    bm.free()


def battle_heads(parts, body, lip, heads):
    """The cylinder heads of the radial in her nose, as her numbers give them (`heads`: see head_numbers). Each row
    stands a head's breadth behind the one before it, turned half a head round, so that its heads show between the
    first's; the first row has a head at the top. A head that her skin hides from every side (its whole front inside
    her skin: behind a face that is not cut open) is not laid."""
    from mathutils.bvhtree import BVHTree
    n, rows, ring, r, back = head_numbers(body, lip, heads)
    z = body.z(-lip)
    skin = next((ob for ob in parts.objects if ob.name.split(".")[0] == "hull"), None)
    tree = None
    if skin is not None:
        bm = bmesh.new()
        bm.from_mesh(skin.data)
        tree = BVHTree.FromBMesh(bm)
        bm.free()

    def hidden(p):
        """Whether a point lies inside her skin: a ray from it forward leaves her skin from within (and does not
        merely graze it)."""
        hit = tree.ray_cast(p, Vector((1.0, 0.0, 0.0))) if tree is not None else (None,)
        return hit[0] is not None and hit[1].x > 0.05
    for j in range(rows):
        front = lip - back - 2.0 * r * j
        for k in range(n):
            a = 0.5 * math.pi + 2.0 * math.pi * (k + 0.5 * j) / n
            y, h = body.side + math.cos(a) * ring, z + math.sin(a) * ring
            if all(hidden(Vector((front, y + 0.9 * r * math.cos(t) * e, h + 0.9 * r * math.sin(t) * e))) for e in (0.0, 1.0)
                   for t in [2.0 * math.pi * i / 8.0 for i in range(8)]):
                continue
            parts.rod("cylinder", (front - 1.8 * r, y, h), (front, y, h), r, paint="steel", sides=8, detail=True)


def battle_dome(parts, frames, at, radii, skirt, window=None):
    """A gun turret whose lower body is metal: a `dome` in her extras with a third value, how high its ring is. Its ball
    is glass above the ring and her coat ('upper') below it, one piece that trains with the turret; its hoop lies on the
    ring, and its two ribs run over the top from it. A fourth value lays a dark oval window each side of the metal:
    (from, to along her, from, to up her), where it is with the turret at rest. The frames and windows are on the fine
    model only, as the game's are."""
    from mathutils.bvhtree import BVHTree
    paints = _G["PAINTS"]
    cx, cy, cz = at
    rx, ry, rz = radii
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * rx + cx, v.co.y * ry + cy, v.co.z * rz + cz))
    if cz - rz < skirt < cz + rz:
        bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1.0e-6, plane_co=(0.0, 0.0, skirt), plane_no=(0.0, 0.0, 1.0))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    layer = bm.faces.layers.int.new("paint")
    for f in bm.faces:
        f[layer] = paints["upper" if f.calc_center_median().z < skirt else "glass"]
    if window and parts.fine:
        x0, x1, z0, z1 = window
        tree = BVHTree.FromBMesh(bm)
        xm, zm, ax, az = 0.5 * (x0 + x1), 0.5 * (z0 + z1), 0.5 * abs(x1 - x0), 0.5 * abs(z1 - z0)
        turns = [2.0 * math.pi * k / 16.0 for k in range(16)]
        laid = []
        for hand in (1.0, -1.0):
            # (a dark oval on the metal, and its pane a little smaller over it: each point laid on the ball's own faces)
            for paint, size, lift in (("engine", 1.0, 0.006), ("glass", 0.82, 0.010)):
                pts = []
                for x, z in [(xm, zm)] + [(xm + ax * size * f * math.cos(t), zm + az * size * f * math.sin(t)) for f in (0.5, 1.0) for t in turns]:
                    hit, normal, _, _ = tree.ray_cast(Vector((x, cy + hand * (ry + 1.0), z)), Vector((0.0, -hand, 0.0)))
                    if hit is None:
                        rest = 1.0 - ((x - cx) / rx) ** 2 - ((z - cz) / rz) ** 2
                        hit, normal = Vector((x, cy + hand * ry * math.sqrt(max(rest, 0.0)), z)), Vector((0.0, hand, 0.0))
                    pts.append(hit + normal * lift)
                laid.append((paint, hand, pts))
        for paint, hand, pts in laid:
            vs = [bm.verts.new(p) for p in pts]
            mid, inner, outer = vs[0], vs[1:17], vs[17:]
            corners = [(mid, inner[k], inner[(k + 1) % 16]) for k in range(16)]
            corners += [(inner[k], outer[k], outer[(k + 1) % 16], inner[(k + 1) % 16]) for k in range(16)]
            for c in corners:
                f = bm.faces.new(c)
                f.normal_update()
                if f.normal.y * hand < 0.0:
                    f.normal_flip()
                f[layer] = paints[paint]
    parts.add("dome", bm, None, False, outward=False)
    if not parts.fine:
        return
    up = max(-0.95, min(0.95, (skirt - cz) / rz))
    out = math.sqrt(1.0 - up * up)
    angles = [2.0 * math.pi * k / 28.0 for k in range(29)]
    pts = [Vector((cx + math.cos(a) * out * rx, cy + math.sin(a) * out * ry, cz + up * rz)) for a in angles]
    parts.strip(frames, pts, [Vector((math.cos(a) * out, math.sin(a) * out, up)).normalized() for a in angles], 0.018, 0.01)
    for across in (-0.35, 0.35):
        shrink = math.sqrt(1.0 - across * across)
        a0 = math.asin(max(-1.0, min(1.0, up / shrink)))
        arc = [a0 + (math.pi - 2.0 * a0) * k / 14.0 for k in range(15)]
        pts = [Vector((cx + math.cos(a) * shrink * rx, cy + across * ry, cz + math.sin(a) * shrink * rz)) for a in arc]
        parts.strip(frames, pts, [Vector((math.cos(a) * shrink, across, math.sin(a) * shrink)).normalized() for a in arc], 0.016, 0.01)


# Her recognition bands (battle_bands): her coat with them laid over it, and a ring of each surface at each band's edge.
BANDS = [
    ("    coat = coat_of(s)\n", "    coat = battle_bands(s, coat_of(s))\n"),
    ("battle_us(wing, BATTLE['wing'] if fine else 1.2, s.get('flap', BATTLE_FLAP)[:2] + s.get('aileron', BATTLE_AILERON)[:2])",
     "battle_us(wing, BATTLE['wing'] if fine else 1.2, s.get('flap', BATTLE_FLAP)[:2] + s.get('aileron', BATTLE_AILERON)[:2] + battle_band_shares(s, 'wing', wing))"),
    ("battle_us(tail, BATTLE['tail'] if fine else 1.0, s.get('elevator', BATTLE_ELEVATOR)[:2])",
     "battle_us(tail, BATTLE['tail'] if fine else 1.0, tuple(s.get('elevator', BATTLE_ELEVATOR)[:2]) + battle_band_shares(s, 'tail', tail))"),
    ("battle_us(fin, BATTLE['fin'] if fine else 0.8, rudder_ends(s.get('rudder', BATTLE_RUDDER)))",
     "battle_us(fin, BATTLE['fin'] if fine else 0.8, rudder_ends(s.get('rudder', BATTLE_RUDDER)) + battle_band_shares(s, 'fin', fin))"),
]


def game():
    """The game's own aircraft sculptor, loaded without running it, at a battle's grain."""
    global _G
    if _G is None:
        path = os.path.join(HERE, "game", "build_plane.py")
        src = open(path).read().split("\ndef main():")[0]
        for old, new in FINER + JETS + GRADED + NOSES + GILLS + CABIN + OWN + fighters.CANOPY + BANDS:
            if src.count(old) != 1:
                raise ValueError("the game's sculptor has changed here: " + old)
            src = src.replace(old, new)
        _G = {"__file__": path, "__name__": "build_plane", "BATTLE": BATTLE, "BATTLE_CHORDS": {True: CHORDS, False: CHORDS_PLAIN}, "battle_us": battle_us,
              "BATTLE_FLAP": FLAP, "BATTLE_AILERON": AILERON, "BATTLE_ELEVATOR": ELEVATOR, "BATTLE_RUDDER": RUDDER, "LINE_DRAWN": LINE,
              "battle_cabin_paint": cabin_paint, "battle_cabin_frames": cabin_frames, "battle_cabin_body": fitout.cabin_body, "battle_nacelle": fitout.nacelle_of,
              "battle_canopy": fighters.canopy, "battle_glare": fighters.glare, "rudder_ends": rudder_ends, "battle_heads": battle_heads,
              "battle_recess": battle_recess, "battle_dome": battle_dome, "battle_hinge": battle_hinge,
              "battle_bands": battle_bands, "battle_band_shares": battle_band_shares}
        exec(compile(src, path, "exec"), _G)
        _G["PAINTS"]["torpedo"] = len(_G["PAINTS"])        # (a paint of a battle's own: what a torpedo bomber carries)
        _G["COMMON"]["torpedo"] = "6C7174"
        for k in BAND_PAINTS:                                # (a machine's recognition bands: see battle_bands; white unless her numbers say)
            _G["PAINTS"][k] = len(_G["PAINTS"])
            _G["COMMON"][k] = "E9E7DF"
        _G["PLANES"].update(EXTRA_PLANES)
    return _G


def names():
    return list(game()["PLANES"])


# What kind of machine each is, and how many are aboard (the parachutes a lost one can leave).
CREW = {"Betty": 7, "Nell": 7, "Amerikabomber": 8, "B29": 11, "Kate": 3, "Val": 2, "Dauntless": 2, "Avenger": 3, "Devastator": 3, "CarrierFighter": 1, "DiveBomber": 2, "TorpedoBomber": 3, "Hellcat": 1, "Wildcat": 1, "Me262": 1, "Bf109": 1, "Spitfire": 1, "Zero": 1, "Mustang": 1, "Yak": 1, "Fw190": 1, "Hurricane": 1, "Warhawk": 1, "Fighter": 1,
        "Stuka": 2, "Sturmovik": 2, "He111": 5, "Lancaster": 7, "B17": 10, "Bomber": 5}
ROLE = {"Betty": "bomber", "Nell": "bomber", "Amerikabomber": "bomber", "B29": "bomber", "Kate": "torpedo bomber", "Val": "dive bomber", "Dauntless": "dive bomber", "Avenger": "torpedo bomber", "Devastator": "torpedo bomber", "DiveBomber": "dive bomber", "TorpedoBomber": "torpedo bomber", "Stuka": "dive bomber", "Sturmovik": "attack", "He111": "bomber", "Lancaster": "bomber", "B17": "bomber", "Bomber": "bomber"}


# ---- later coats of paint ----------------------------------------------------------------------------------
# The same machine in other colours, by the month they come in (the art director's unit table, `carrier_paint`). The
# file lists them under `schemes`; the game paints her in the last one whose month has come.
_THREE_BLUES = {"upper": "2A3850", "upper2": "55708C", "under": "D9DAD3", "nose": "2A3850"}
_SEA_BLUE = {"upper": "1E2A40", "upper2": "1E2A40", "under": "1E2A40", "nose": "1E2A40", "finish": "gloss"}     # (the war's one gloss coat)
_DARK_GREEN = {"upper": "2F4236", "upper2": "2F4236", "under": "A3A69C"}
_BARE_MUSTANG = {"upper": "B0B6BA", "upper2": "B0B6BA", "under": "9CA2A6"}
SCHEMES = {
    "Hellcat": [("1944-07", "overall sea blue", _SEA_BLUE)],
    "Wildcat": [("1943-02", "three blues over white", _THREE_BLUES), ("1944-07", "overall sea blue", _SEA_BLUE)],
    "Dauntless": [("1943-02", "three blues over white", _THREE_BLUES), ("1944-07", "overall sea blue", _SEA_BLUE)],
    "Avenger": [("1943-02", "three blues over white", _THREE_BLUES), ("1944-07", "overall sea blue", _SEA_BLUE)],
    "Devastator": [("1943-02", "three blues over white", _THREE_BLUES), ("1944-07", "overall sea blue", _SEA_BLUE)],
    "Zero": [("1942-07", "dark green over grey", _DARK_GREEN)],
    "Kate": [("1942-07", "dark green over grey", _DARK_GREEN)],
    "Val": [("1942-07", "dark green over grey", _DARK_GREEN)],
    # (the Mustang: olive drab over grey with white bands as she first flew from England, fighters.MUSTANG; natural metal
    #  from April 1944, her spinner, nose band and bands then black, as the theatre painted them on bare metal; from
    #  December 1944, the bands gone, her nose red)
    "Mustang": [("1944-04", "natural metal, black bands", dict(_BARE_MUSTANG, nose="1C1C1C", boss="1C1C1C", band="1C1C1C", band_under="1C1C1C")),
                ("1944-12", "natural metal", dict(_BARE_MUSTANG, nose="B01818", boss="B01818", band=_BARE_MUSTANG["upper"], band_under=_BARE_MUSTANG["under"]))],
}


# A marking that changes with the months (the art director, 2026-11-18): the American star has a red centre until
# May 1942, is plain from June 1942, has white bars edged in red from July 1943 (ordered 29 June; the historian,
# 2026-11-20) and edged in blue from September 1943. Whatever form a machine's numbers give
# her, the file says which she wears when (`mark_months`), and the game draws that one.
# Japan's navy (the historian through the art director, 07:42 UTC the same day): a thin white ring round the disc on
# the FUSELAGE only, from October 1942 to December 1943, then the plain disc again; the wings' discs stay plain. A form
# with a third word holds only for the marks in that place (a mark's `place`: side, wing, fin or code).
MARK_MONTHS = {"USA": [(None, "USA-RED"), ("1942-06", "USA"), ("1943-07", "USA-BAR-RED"), ("1943-09", "USA-BAR")],
               "JAP": [(None, "JAP-PLAIN"), ("1942-10", "JAP-RING", "side"), ("1944-01", "JAP-PLAIN", "side")]}
MARK_FAMILY = {"USA": "USA", "USA-BAR": "USA", "USA-BAR-RED": "USA", "USA-RED": "USA", "JAP-PLAIN": "JAP", "JAP-RING": "JAP"}


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
    paints = {k: c.get(k, "808080") for k in G["PAINTS"] if k not in BAND_PAINTS}
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
    if s.get("bands"):                                       # (her recognition bands, last: no other machine's paints move)
        paints.update({k: c.get(k, "E9E7DF") for k in BAND_PAINTS})
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
TALLY_AT = {"He111": 3.85, "Amerikabomber": 6.8, "B29": 8.25}                   # (her cockpit is her glass nose: her record goes on the first solid skin behind it)
LEAD = 0.026                                 # (half the star over a mission flown as leader)
BAR = 0.045                                  # (half the height of a white bar on a rudder)


def habit(nation, role):
    """How a nation's machines of this kind wear their record: the art director's ruling."""
    if role == "fighter":
        return {"GER": "bars", "SOV": "stars"}.get(nation, "victims")
    return "bombs" if nation in ("UK", "USA") else "none"


def tally_of(name, s, hull, role, G, fins, fine=True, model=None):
    """Where her record is painted: under the sill of her cockpit on the port side, running aft from her windscreen
    (and, for a fighter Germany flies, on the port face of her rudder). A bomber of any nation wears the art director's
    panel of 2026-11-20 (tallies.py): her missions and her gunners' kills, under her pilot's window."""
    if role == "bomber" and model is not None:
        import livery
        import tallies
        hoods = s.get("hoods", [])
        nation = fitout.nation(s)
        art = livery.PLACES.get(name, {}).get("art")
        out = {"side": "port", "nation": nation,
               "habit": ("own: " + tallies.OWN[nation]) if nation in tallies.OWN else "victims" if nation != "*" else "by nation (rules.own)",
               "note": "a slot is a place in the game's axes and the way the skin faces there (x, y, z, nx, ny, nz); fill each list in order (rules)"}
        out.update(tallies.panel(name, s, hull, G["Body"](hoods[0][0], 2.0) if hoods else None, nation, model,
                                 marks=[mk for mk in (model.marks or []) if mk.get("place") == "side"],
                                 art=(art[0], art[1], art[1] * 864.0 / 1408.0) if art else None))
        for kind, k, why in out.pop("_trouble"):
            print(f"tally {name}: {kind} slot {k} {why}", flush=True)
        return out
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
        rud = s.get("rudder", RUDDER)                   # (her own rudder: the bars aft of its hinge as this model cuts it, and within its height)
        hinge = battle_hinge(rud, G["BATTLE_CHORDS"][fine])[2] if "rudder" in s else RUDDER[2]
        bars, u, step = [], fin.root + min(0.8, rud[1] - 0.04) * span, 2.0 * 0.3 * BAR + 0.016
        while len(bars) < 30 and u > fin.root + max(0.22, rud[0] + 0.04) * span:
            chord = abs(fin.trail(u) - fin.lead(u))
            n = min(10, 30 - len(bars), int(chord * (0.95 - hinge - 0.04) / step))
            for k in range(n):
                c = hinge + 0.04 + (0.3 * BAR + k * step) / chord
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
    if (not s.get("engines") or s.get("nose_engine")) and not jets:                 # (a trimotor's nose engine too)
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

    def in_cab(c):
        """Whether the middle of a face lies well inside her tail gunner's cabin, under its roof (her numbers' `cab`:
        fitout.cab_hull): 0.3 behind its front, so that a face of her fin that reaches forward out of it is kept whole."""
        _, front, back, _, roof = s["cab"][:5]
        return back < c.x < front - 0.3 and c.z < roof - 0.01

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
    elev, rud = s.get("elevator", ELEVATOR), s.get("rudder", RUDDER)
    for hand, nm in ((-1.0, "port"), (1.0, "starboard")):
        W = G["Wing"](s["wing"], hand)
        surf[f"aileron.{nm}"] = ("wing", W, wing.root + span * ail[0], wing.root + span * ail[1], cut_at(ail[2]), f"wing.{nm}", hand)
        if not s.get("engines") and not jets:
            surf[f"flap.{nm}"] = ("wing", W, max(wing.root + span * flap[0], y_cut), wing.root + span * flap[1], cut_at(flap[2]), f"wing.{nm}", hand)
        T = G["Wing"](s["tail"], hand)
        surf[f"elevator.{nm}"] = ("tail", T, T.root + (T.tip - T.root) * elev[0], T.root + (T.tip - T.root) * elev[1], cut_at(elev[2]), "tail", hand)
    for fin_sections, fin_side in fins:
        for hand_side in ((fin_side, -fin_side) if fin_side else (0.0,)):
            F = G["Wing"](fin_sections, 1.0, True, hand_side)
            nm = "rudder" if not fin_side else ("rudder.starboard" if hand_side > 0 else "rudder.port")
            top = F.tip + 1e-3 if rud[1] >= 1.0 else F.root + (F.tip - F.root) * rud[1]     # (a rudder that runs to 1.0 runs to the top of its fin)
            surf[nm] = ("fin", F, F.root + (F.tip - F.root) * rud[0], top, cut_at(rud[2]), "tail", hand_side)
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
        if kind == "fin" and fine and s.get("cab") and in_cab(c):
            return None                                     # (her fin's root and her rudder's foot stand in the tail gunner's cabin: left out, so that its windows look in on him)
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
                if s.get("graded"):
                    # (in a coat laid by where a point is, each plate is in the colour her skin is there: her under colour
                    #  below the line her skin's own is drawn at, her second upper colour where the coat lays it, as the
                    #  sculptor's coat_of does: only with `patch`)
                    c = f.calc_center_median()
                    of_face = (PAINTS["under"] if f.normal.z < s.get("under_from", -0.3) else
                               PAINTS["upper2"] if s.get("patch", 0.0) > 0.0 and s["graded"](c) > 0.0 else PAINTS["upper"])
                else:
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
            us = battle_us(W, BATTLE["tail"] if fine else 1.0, tuple(elev[:2]))
        else:
            us = battle_us(W, BATTLE["fin"] if fine else 0.8, rudder_ends(rud))
        us = [u for u in us if u0 - 2e-3 <= u <= u1 + 2e-3]
        cut = False
        if kind == "fin" and fine and s.get("cab"):
            # (her rudder's foot stands in her tail gunner's cabin, and is left out there (in_cab): its nose and cove
            #  begin at the cabin's roof, and so does the cap across its foot, the rudder's own (it closes her rudder
            #  when it swings out over the box's edge); her fin's cap under it would lie on the roof, and is not drawn)
            _, front, back, _, roof = s["cab"][:5]
            kept = [u for u in us if not (back < W.at(u, hc, True).x < front and W.at(u, hc, True).z < roof - 0.01)]
            cut, us = len(kept) < len(us), kept
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
            if kind == "fin" and way > 0.0 and u1 > W.tip:
                continue                                  # (a rudder that runs to the top of its fin)
            ring = [port(W.at(u, c, True)) for c in cs] + [port(W.at(u, c, False)) for c in reversed(cs[:-1])]
            mid = sum(ring, Vector()) / len(ring)
            away = (Z if kind == "fin" else Vector((0.0, -hand, 0.0))) * way
            fan = [(k, (k + 1) % len(ring), len(ring)) for k in range(len(ring))]
            m.use(nm)
            m.add(ring + [mid], fan, "line", False, lambda q, o=away: o)
            if cut and way < 0.0:
                continue
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
    if name in fitout.NOSE_GUNS or "nose_guns" in s:
        x = hubs[0].x - 0.5
        top = hull.at(x, math.radians(90.0))
        m.socket("gun.nose", (top.x, 0.0, top.z + 0.03), X, part="plane", note="the guns over the engine")
    elif s.get("cowl_gun"):                                 # (one gun firing through the ring of her cowling: see fitout.guns)
        m.socket("gun.nose", port(s["cowl_gun"]) + X * 0.01, X, part="plane", note="the gun in her cowling")
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
        if (not s.get("engines") or s.get("nose_engine")) and not jets and n[2] < -0.5 and y < 0.5 * wide:
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
    m.tally = tally_of(name, s, hull, role, G, fins, fine, m)
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
    f.hinges = {"elevator": cut_at(elev[2]), "rudder": cut_at(rud[2])}      # (the share of the chord each is cut at, snapped to her rings)
    f.turrets = [(k, port(c), radii) for k, c, radii in turrets]
    f.flex = [(nm, port(a_), port(b_)) for nm, a_, b_ in flex]
    f.guns_of = {k: [(port(rods[i][0]), port(rods[i][1])) for i in range(len(rods)) if gun_part[i] == f"turret.{k}.guns"] for k, c, radii in turrets}
    f.gun_rods = [(gun_part[i], port(a_), port(b_)) for i, (a_, b_) in enumerate(rods)]      # (every gun: its part, its breech, its muzzle)
    f.engine_list = list(s.get("engines", []))
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


BUILDERS = {n: (lambda fine=True, n=n: convert(n, fine)) for n in ("Bf109", "Spitfire", "Zero", "Mustang", "Yak", "Fw190", "Me262", "Hurricane", "Warhawk", "Hellcat", "Wildcat", "Fighter", "CarrierFighter",
                                                                  "Stuka", "Sturmovik", "Val", "Dauntless", "DiveBomber", "Kate", "Avenger", "Devastator", "TorpedoBomber", "He111", "Nell", "Betty", "Lancaster", "B17",
                                                                  "Amerikabomber", "B29", "Bomber")}
