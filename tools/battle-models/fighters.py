"""The single-seat fighters laid out again to measured drawings, and their glazing (Brian, 2026-11-19: "use this 3d model
is our bench mark for all airplanes", of a B-17 model he had found; the B-17 was done first, in planes.py).

Each machine's lines are measured from a public three-view drawing of the real aircraft at a known scale: measured,
not traced, and nothing of any drawing or model is used but its measures. The numbers are in the game's form (see
game/plane_numbers.py: stations of the fuselage as (x, z of its middle, half its width, half its height), sections of
a flying surface as (how far out, leading edge, trailing edge, height, thickness)), x ahead, z up, in metres, with
the airscrew's axis at z = 0.

Their glazing is new: `canopy` in a machine's numbers replaces the sculptor's round hood. A canopy is a run of pieces
(a windscreen, a sliding hood, the fixed glazing behind it), each a loft through cross-sections: at each station along
her, half a section as a row of points from the sill up and over to her middle line, (out, up). Where the points are
few the panes between them are flat, and the bars between panes are framed: a flat-paned box for the Bf 109, a
greenhouse of many frames for the Zero, a bubble with almost none for the Mustang. Every windscreen has a flat front
pane between straight posts, as all four had.

    canopy = {"pieces": [{"sections": [(x, [(y, z), ...]), ...],     (every section of one piece has as many points)
                          "hoops": [i, ...],                         (a frame round the section i: 0 is the first)
                          "rails": [k, ...],                         (a frame along the point k of every section, both sides)
                          "bar": 0.02}, ...]}                        (half the width of its frames)
"""
import math

import bmesh
from mathutils import Vector

import fitout


# ---- how a section is laid down ------------------------------------------------------------------------------
def side_at(hull, x, z):
    """How far out her fuselage's side stands at this height (nothing above or below her)."""
    w, h, zc = hull.w(-x), max(hull.h(-x), 1e-6), hull.z(-x)
    e = hull.square_at(x) if hasattr(hull, "square_at") else 2.0 / hull.power
    t = abs(z - zc) / h
    return w * (1.0 - t ** e) ** (1.0 / e) if t < 1.0 else 0.0


def bubble(hull, x, sill, hw, top, square=2.2, n=10, band=0.13):
    """Half a section of a hood that slides on rails: from inside her fuselage up the metal rail it runs on (`band`
    deep, its foot on her side), to the sill where the glass begins and is widest, and over to the top in n steps.
    The first SKIRT points are the rail's: they are painted with her coat, not glass."""
    depth = min(band, 0.02 + 0.6 * (top - sill))
    zb = sill - depth
    yb = max(hw + 0.035 * depth / band, side_at(hull, x, zb) + 0.004)
    zb = min(zb, fitout.body_top(hull, x, yb) - 0.006)         # (where she is lower than that, the rail comes down to her)
    p = 2.0 / square
    pts = [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb)]
    for k in range(n + 1):
        a = math.radians(90.0 * k / n)
        pts.append((0.0 if k == n else math.cos(a) ** p * hw, sill + math.sin(a) ** p * (top - sill)))
    return pts


SKIRT = 2                                                   # (how many points of a section are below its sill: the rail)


def headed(sec, post):
    """A section whose top is the straight head of a windscreen: every point nearer her middle than the post is
    raised or lowered to the height the section has at the post, and the post is a point of its own."""
    z = None
    for (y0, z0), (y1, z1) in zip(sec, sec[1:]):
        if y1 <= post <= y0:
            z = z0 + (z1 - z0) * (y0 - post) / max(y0 - y1, 1e-9)
            break
    out = [(y, zz) for y, zz in sec if y > post + 1e-6]
    return out + [(post, z), (0.0, z)]


def resample(points, n):
    """A row of points laid again as n + 1 points, evenly along its length (so that two pieces can be joined)."""
    lengths = [0.0]
    for a, b in zip(points, points[1:]):
        lengths.append(lengths[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    total = lengths[-1] or 1.0
    out = []
    for k in range(n + 1):
        t = total * k / n
        i = max(j for j in range(len(lengths)) if lengths[j] <= t + 1e-12)
        i = min(i, len(points) - 2)
        f = (t - lengths[i]) / max(lengths[i + 1] - lengths[i], 1e-12)
        out.append((points[i][0] + (points[i + 1][0] - points[i][0]) * f, points[i][1] + (points[i + 1][1] - points[i][1]) * f))
    return out


def tween(x0, sec0, x1, sec1, steps):
    """Sections between two of the same number of points, straight from one to the other (flat panes stay flat)."""
    out = []
    for k in range(1, steps):
        t = k / steps
        out.append((x0 + (x1 - x0) * t, [(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t) for a, b in zip(sec0, sec1)]))
    return out


def deck(hull, x, y, sink=0.01):
    """A point on her fuselage's top at x, y out from her middle (a windscreen's foot)."""
    return (y, fitout.body_top(hull, x, y) - sink)


# ---- the glazing itself, for the game's sculptor (planes.py patches it in where the hood was) -----------------
def ring_of(x, half):
    """The whole section, round from the port sill over her top to the starboard sill (y to starboard is the game's)."""
    pts = [Vector((x, -y, z)) for y, z in half]
    pts += [Vector((x, y, z)) for y, z in reversed(half[:-1])]
    return pts


def finish(obj, n, skirt, inner, mid_z, ring_faces):
    """The sculptor closes every ring of a skin, and paints this one glass all over. The faces that closed each ring
    under her (from her starboard sill back under to her port one) are taken away again, and the rail's faces along
    each foot are painted with her coat (a face of paint -1 takes its paint point by point: her coat there); the
    rail's inner face, seen through the glass, is dark metal (paint `inner`)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    layer = bm.faces.layers.int.get("paint")
    bm.faces.ensure_lookup_table()
    rail, gone = [], []
    for i, f in enumerate(bm.faces):
        k = i % n
        if k == n - 1:
            gone.append(f)
        elif k < skirt or n - 1 - skirt <= k:
            f[layer] = -1
            rail.append((f, i >= ring_faces))
    for f, cap in rail:
        f.normal_update()
        c = f.calc_center_median()
        if f.normal.dot(Vector((-1.0, 0.0, 0.0)) if cap else Vector((0.0, c.y, c.z - mid_z))) < 0.0:
            f.normal_flip()                                 # (each face of the rail looks out: its sides outward, its end aft)
        back = bm.faces.new([bm.verts.new(v.co - f.normal * 0.008) for v in reversed(f.verts)])
        back[layer] = inner
    bmesh.ops.delete(bm, geom=gone, context="FACES_ONLY")
    bm.to_mesh(obj.data)
    bm.free()


def canopy(parts, frames, glints, s, hull, coat, paints):
    """Lays the glass of every piece of her canopy, its rails and its frames."""
    fine = parts.fine
    for piece in s["canopy"](hull):
        secs = piece["sections"]
        rings = [ring_of(x, half) for x, half in secs]
        n = len(rings[0])
        if any(len(r) != n for r in rings):
            raise ValueError("every section of a piece of canopy must have as many points")
        skirt = piece.get("skirt", 0)

        def paint(p, nrm):
            return "glass"
        paint.coat, paint.blend = coat, getattr(coat, "blend", None)
        mid_z = min(z for x, half in secs for y, z in half)
        finish(parts.skin("hood", rings, paint, caps=(False, piece.get("cap", False))), n, skirt, paints["steel"], mid_z, (len(rings) - 1) * n)

        def facing(i, k):
            """Which way the glass faces at the point k of the section i."""
            a, b = rings[max(i - 1, 0)][k], rings[min(i + 1, len(rings) - 1)][k]
            c, d = rings[i][max(k - 1, 0)], rings[i][min(k + 1, n - 1)]
            nrm = (b - a).cross(d - c)
            p = rings[i][k]
            out = Vector((0.0, p.y, p.z - mid_z))
            if nrm.length < 1e-9:
                nrm = out
            nrm.normalize()
            return -nrm if nrm.dot(out) < 0.0 else nrm

        bar = piece.get("bar", 0.02) * (1.0 if fine else 1.4)
        for i in piece.get("hoops", []):
            i = i % len(rings)
            ks = range(skirt, n - skirt)                    # (a hoop is a frame round the glass: from sill to sill)
            parts.strip(frames, [rings[i][k] for k in ks], [facing(i, k) for k in ks], bar, 0.006)
        half_n = len(secs[0][1])
        for k in piece.get("rails", []):
            for side in ((k, n - 1 - k) if k != half_n - 1 else (k,)):
                live = [i for i in range(len(rings)) if i not in piece.get("bare", ())]
                parts.strip(frames, [rings[i][side] for i in live], [facing(i, side) for i in live], bar * 0.9, 0.006)
        if piece.get("glint") is not None and fine:
            k = piece["glint"]
            for side in (k, n - 1 - k):
                pts = [r[side] for r in rings[1:-1]]
                if len(pts) > 2:
                    parts.strip(glints, pts, [facing(i + 1, side) for i in range(len(pts))], 0.03, 0.004,
                                taper=lambda t: math.sin(math.pi * min(max(t, 0.02), 0.98)))


def glare(parts, bm, hull, xs, half, lift):
    """The dull panel along the top of her nose, lying on the round of it (the game's is a flat strip, which stood
    off a nose as round as the Mustang's like a plank)."""
    def angle_for(x, y):
        lo, hi = 0.0, math.pi                               # (round her from her starboard side over the top to her port side)
        for _ in range(40):
            mid = 0.5 * (lo + hi)
            if hull.at(x, mid).y > y:
                lo = mid
            else:
                hi = mid
        return 0.5 * (lo + hi)
    m = 6
    rows = []
    for x in xs:
        rows.append([(hull.at(x, angle_for(x, half * (1.0 - 2.0 * j / m))), hull.out(x, angle_for(x, half * (1.0 - 2.0 * j / m)))) for j in range(m + 1)])
    verts = [[bm.verts.new(p + nrm * lift) for p, nrm in row] for row in rows]
    for i in range(len(rows) - 1):
        for j in range(m):
            f = bm.faces.new((verts[i][j], verts[i][j + 1], verts[i + 1][j + 1], verts[i + 1][j]))
            f.normal_update()
            if f.normal.dot(rows[i][j][1]) < 0.0:
                f.normal_flip()


# The sculptor's hood loop with the canopy laid in its place, and its glare panel lying on her nose (see planes.game).
CANOPY = [
    ('        hood = battle_cabin_body(Body, hood_stations, s["cabin"], hull) if s.get("cabin") else Body(hood_stations, 2.3)\n',
     '        if s.get("canopy"):\n'
     '            battle_canopy(parts, frames, glints, s, hull, coat, PAINTS)\n'
     '            break\n'
     '        hood = battle_cabin_body(Body, hood_stations, s["cabin"], hull) if s.get("cabin") else Body(hood_stations, 2.3)\n'),
    ('        parts.strip(glare, [hull.at(x, math.radians(90.0)) for x in xs], [hull.out(x, math.radians(90.0)) for x in xs], half, 0.003)\n',
     '        if s.get("canopy"):\n'
     '            battle_glare(parts, glare, hull, xs, half, 0.003)\n'
     '        else:\n'
     '            parts.strip(glare, [hull.at(x, math.radians(90.0)) for x in xs], [hull.out(x, math.radians(90.0)) for x in xs], half, 0.003)\n'),
]


def hood_of(stations):
    """The `hoods` entry the rest of the tools read (where her pilot sits, where her canopy is): her canopy's own lines,
    as stations (x, height of its sill, half its width there, how far its top stands above its sill)."""
    return [(stations, (), False)]


def opening(s, rings_x):
    """The opening in her fuselage's top under her canopy (the fine model): (front, back, from this angle round her
    up over her top), its ends set on her skin's own rings so that its edges are clean; None if she has none."""
    o = s.get("opening")
    if not o:
        return None
    return (min(rings_x, key=lambda r: abs(r - o[0])), min(rings_x, key=lambda r: abs(r - o[1])), o[2])


def in_opening(op, hull, c):
    """Whether a point of her skin lies in her cockpit's opening."""
    if not op or not (op[1] < c.x < op[0]):
        return False
    a = math.degrees(math.atan2((c.z - hull.z(-c.x)) / hull.h(-c.x), abs(c.y) / hull.w(-c.x)))
    return a > op[2]


# ---- the P-51D Mustang -----------------------------------------------------------------------------------------
# Measured from "North American P-51D Mustang 3-view line drawing" (Commons; U.S. Air Force, public domain) at 94.6
# pixels to the metre, her length 9.83 m from spinner to rudder; the wing's span set to the real 11.28 m. What
# changed from the game's numbers: a fuselage that deepens behind the spinner to the chin intake and tapers to a slim
# tail; the bubble hood on its metal rail, its glass widest at the rail and ending in a point on her spine, over a
# windscreen with one flat armoured pane in front and a quarter pane each side; an open cockpit under it; the belly
# scoop well forward, under her seat, and long, rising into her belly far aft; a straight-tapered wing; the tall fin
# with its rudder hinged straight up and down, and the small fillet that runs it into her spine. Her natural metal,
# the red band behind her spinner and the olive drab panel ahead of her windscreen are as they were.
#
# Her hood, measured (x, the height of its sill where the glass begins, half its width there, the height of its top).
_P51_BUBBLE = [(0.10, 0.41, 0.33, 0.845), (-0.06, 0.42, 0.335, 0.865), (-0.30, 0.435, 0.335, 0.89), (-0.55, 0.455, 0.325, 0.885),
               (-0.80, 0.48, 0.31, 0.86), (-1.00, 0.50, 0.29, 0.83), (-1.20, 0.53, 0.255, 0.775), (-1.36, 0.55, 0.22, 0.72),
               (-1.50, 0.58, 0.17, 0.675), (-1.60, 0.60, 0.11, 0.645), (-1.66, 0.615, 0.04, 0.625)]
_P51_SCREEN = (0.50, 0.19, 0.105)                           # (the foot of her windscreen's front pane, half its width there, and at its head)


def _p51_canopy(hull):
    foot_x, foot_half, head_half = _P51_SCREEN
    x0, sill, hw, top = _P51_BUBBLE[0]
    head = headed(bubble(hull, x0, sill, hw, top), head_half)
    post = len(head) - 2
    # (the windscreen's foot: its front pane stands on her deck; the quarter panes each side run from its foot
    #  corner back and out to the rail, so that every point outside the post is at the post's foot)
    foot = [deck(hull, foot_x, foot_half)] * (len(head) - 1) + [deck(hull, foot_x, 0.0)]
    screen = {"sections": [(foot_x, foot), (x0, head)], "skirt": SKIRT, "hoops": [-1], "rails": [post, SKIRT], "bar": 0.022}
    hood = [(x, bubble(hull, x, sl, w, t)) for x, sl, w, t in _P51_BUBBLE[1:]]
    first = resample(head, len(hood[0][1]) - 1)
    hood = [(x0, first)] + tween(x0, first, hood[0][0], hood[0][1], 3) + hood
    slide = {"sections": hood, "skirt": SKIRT, "hoops": [0], "rails": [SKIRT], "bar": 0.018, "cap": True, "bare": (len(hood) - 1,)}
    return [screen, slide]


MUSTANG = {
    "title": "P-51D Mustang (our own model)",
    "hull": [(3.90, 0.0, 0.03, 0.03), (3.80, 0.0, 0.13, 0.13), (3.62, 0.0, 0.22, 0.22), (3.42, 0.0, 0.30, 0.30), (3.30, 0.0, 0.34, 0.34),
             (3.25, -0.03, 0.355, 0.38), (3.17, -0.08, 0.37, 0.45), (3.0, -0.12, 0.39, 0.51), (2.5, -0.145, 0.415, 0.565), (2.0, -0.135, 0.43, 0.605),
             (1.45, -0.135, 0.435, 0.64), (0.9, -0.13, 0.44, 0.665), (0.4, -0.12, 0.44, 0.67), (-0.4, -0.115, 0.44, 0.66), (-1.2, -0.09, 0.43, 0.62),
             (-1.72, -0.055, 0.40, 0.615), (-2.18, -0.02, 0.367, 0.59), (-2.81, 0.0, 0.317, 0.55), (-3.45, 0.025, 0.267, 0.505), (-4.08, 0.05, 0.225, 0.45),
             (-4.72, 0.04, 0.16, 0.34), (-5.19, 0.02, 0.11, 0.235), (-5.55, 0.0, 0.065, 0.145), (-5.85, -0.01, 0.03, 0.055), (-5.93, -0.01, 0.01, 0.02)],
    "square": 2.3, "screw": 1.70, "blades": 4, "nose_back": 2.85, "spinner": 5,
    "hoods": hood_of([(_P51_SCREEN[0], 0.535, _P51_SCREEN[1], 0.01)] + [(x, sl, w, t - sl) for x, sl, w, t in _P51_BUBBLE]),
    "canopy": _p51_canopy,
    "opening": (0.12, -1.0, 54.0),                          # (her cockpit, open under the glass: from, to, and from 54 degrees round her up over her top)
    "wing": [(0.0, 1.22, -1.56, -0.60, 0.48), (0.45, 1.17, -1.53, -0.565, 0.47), (0.95, 1.09, -1.42, -0.52, 0.43), (1.6, 0.99, -1.30, -0.46, 0.39),
             (2.85, 0.90, -1.07, -0.35, 0.31), (4.12, 0.80, -0.83, -0.24, 0.23), (5.07, 0.73, -0.66, -0.16, 0.18), (5.45, 0.665, -0.575, -0.125, 0.12),
             (5.6, 0.55, -0.46, -0.11, 0.06), (5.64, 0.38, -0.27, -0.105, 0.01)],
    "tail": [(0.0, -4.05, -5.43, 0.31, 0.16), (0.42, -4.19, -5.39, 0.31, 0.15), (1.06, -4.31, -5.33, 0.31, 0.13), (1.69, -4.44, -5.27, 0.31, 0.10),
             (1.90, -4.53, -5.20, 0.31, 0.06), (2.0, -4.72, -5.04, 0.31, 0.02)],
    # (the fin from her keel up: its rudder is the after 40 per cent of it, hinged straight up and down at x = -5.31 as
    #  the drawing has it, and running down under her tailplane to her tail cone; the fillet ahead of it is a scoop)
    "fin": [(0, -4.50, -5.85, 0.0, 0.13), (0, -4.40, -5.92, 0.25, 0.13), (0, -4.455, -5.88, 0.50, 0.13), (0, -4.575, -5.80, 0.82, 0.115),
            (0, -4.695, -5.72, 1.21, 0.095), (0, -4.80, -5.65, 1.52, 0.075), (0, -4.89, -5.61, 1.65, 0.055), (0, -5.02, -5.55, 1.71, 0.02)],
    "patch": 0, "colours": {"upper": "B0B6BA", "upper2": "B0B6BA", "under": "9CA2A6", "nose": "B01818", "boss": "B01818"},
    "stubs": (2.77, 1.87, 17.0), "well": (1.25, 0.42, 0.45), "well_in": True, "pitot": True, "glare": (3.2, 0.62, 0.17),
    "aileron": (0.60, 0.95, 0.74), "flap": (0.08, 0.60, 0.76),
    "guns": [(2.08, 0.05, 0.05, 0.02), (2.27, 0.05, 0.05, 0.02), (2.46, 0.05, 0.05, 0.02)],
    "scoops": [
        # (the radiator's scoop under her belly: its mouth under her seat, a hand's breadth below her skin; long, rising into her far aft)
        ([(0.10, -0.975, 0.25, 0.115), (0.02, -0.97, 0.27, 0.13), (-0.25, -0.91, 0.29, 0.23), (-0.77, -0.90, 0.30, 0.24), (-1.30, -0.84, 0.30, 0.22),
          (-1.83, -0.75, 0.28, 0.18), (-2.36, -0.625, 0.24, 0.105), (-2.89, -0.51, 0.16, 0.04), (-3.05, -0.50, 0.08, 0.02)], 0.0, 2.6),
        # (the fillet ahead of her fin: a thin blade rising out of her spine into the fin's leading edge; its foot is inside her)
        ([(-3.30, 0.47, 0.006, 0.03), (-3.60, 0.47, 0.030, 0.07), (-3.93, 0.47, 0.040, 0.10), (-4.20, 0.47, 0.040, 0.13), (-4.36, 0.48, 0.036, 0.15),
          (-4.48, 0.50, 0.030, 0.20), (-4.56, 0.52, 0.020, 0.22), (-4.60, 0.53, 0.006, 0.20)], 0.0, 2.0)],
    "mast": (-2.76, 0.53, 1.10),
    "side_marks": [(-2.95, 0.3, "USA-BAR")], "wing_marks": [(3.7, -0.05, 0.4, "USA-BAR", "USA-BAR", False, False)], "code": (-3.75, 0.28),
}

FIGHTERS = {"Mustang": MUSTANG}
