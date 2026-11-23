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
                          "bars": [(i, k0, k1), ...],                (a frame across the section i from its point k0 to k1)
                          "struts": [((i0, k0), (i1, k1)), ...],     (a straight frame between points of two sections)
                          "bar": 0.02}, ...]}                        (half the width of its frames)
"""
import inspect
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


def tween(x0, sec0, x1, sec1, steps, lean0=0.0, lean1=0.0):
    """Sections between two of the same number of points, straight from one to the other (flat panes stay flat)."""
    out = []
    for k in range(1, steps):
        t = k / steps
        out.append((x0 + (x1 - x0) * t, [(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t) for a, b in zip(sec0, sec1)],
                    lean0 + (lean1 - lean0) * t))
    return out


def deck(hull, x, y, sink=0.01):
    """A point on her fuselage's top at x, y out from her middle (a windscreen's foot)."""
    return (y, fitout.body_top(hull, x, y) - sink)


def box(hull, x, sill, corner, ridge, band=0.035):
    """Half a section of a framed box hood, of flat panes: its metal frame from her side up to the sill where the
    glass begins (the first SKIRT points), a flat side pane from there up to its top corner, and a flat roof pane
    from the corner over to her middle line at the height `ridge`. corner is (out, up)."""
    ys = side_at(hull, x, sill) + 0.003
    zb = sill - band
    yb = max(ys + 0.004, side_at(hull, x, zb) + 0.004)
    return [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb), (ys, sill), corner, (0.0, ridge)]


def green(hull, x, sill, ys, shoulder, top, band=0.06, roof=3):
    """Half a section of a greenhouse of many frames (the Zero's): its metal rail from her side up to the sill where
    the glass begins, `ys` out from her middle (the first SKIRT points: the rail stands on her skin wherever that is
    lower); a flat side pane leaning in from the sill to its shoulder, (out, up); and its roof, rounded over from the
    shoulder to her middle line at `top` in `roof` flat panes."""
    yb = ys + 0.004
    zb = min(sill - band, fitout.body_top(hull, x, yb) - 0.006)
    pts = [(max(yb - 0.03, 0.0), zb - 0.06), (yb, zb), (ys, sill), shoulder]
    y1, z1 = shoulder
    for k in range(1, roof + 1):
        a = math.radians(90.0 * k / roof)
        pts.append((0.0 if k == roof else y1 * math.cos(a), z1 + (top - z1) * math.sin(a)))
    return pts


# ---- the glazing itself, for the game's sculptor (planes.py patches it in where the hood was) -----------------
def ring_of(x, half, lean=0.0):
    """The whole section, round from the port sill over her top to the starboard sill (y to starboard is the game's).
    A section that leans stands forward by `lean` for every metre up from its foot (the Spitfire's windscreen frame)."""
    foot = min(z for y, z in half)
    pts = [Vector((x + lean * (z - foot), -y, z)) for y, z in half]
    pts += [Vector((x + lean * (z - foot), y, z)) for y, z in reversed(half[:-1])]
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
        ring = list(reversed(f.verts))
        ring = ring[1:] + ring[:1]                          # (cut across the same corners as the face it backs: a twisted face's back cut the other way shows through it)
        back = bm.faces.new([bm.verts.new(v.co - f.normal * 0.008) for v in ring])
        back[layer] = inner
    bmesh.ops.delete(bm, geom=gone, context="FACES_ONLY")
    bm.to_mesh(obj.data)
    bm.free()


def pieces(make, hull, fine):
    """The pieces of her canopy, from her numbers' `canopy`: a function of her fuselage, or of her fuselage and whether
    this is the fine model (the map's may be laid with fewer pieces and frames). That is told to a parameter named
    `fine`, or else to a second parameter that must be given (one with a default of its own is left alone)."""
    try:
        params = inspect.signature(make).parameters
    except (TypeError, ValueError):
        return make(hull)
    if "fine" in params:
        return make(hull, fine=fine)
    order = [p for p in params.values() if p.kind in (p.POSITIONAL_ONLY, p.POSITIONAL_OR_KEYWORD)]
    if len(order) >= 2 and order[1].default is order[1].empty:
        return make(hull, fine)
    return make(hull)


def canopy(parts, frames, glints, s, hull, coat, paints):
    """Lays the glass of every piece of her canopy, its rails and its frames, and any small windows behind it."""
    fine = parts.fine
    windows(parts, frames, glints, s, hull)
    for piece in pieces(s["canopy"], hull, fine):
        secs = piece["sections"]
        rings = [ring_of(*sec) for sec in secs]
        n = len(rings[0])
        if any(len(r) != n for r in rings):
            raise ValueError("every section of a piece of canopy must have as many points")
        skirt = piece.get("skirt", 0)

        def paint(p, nrm):
            return "glass"
        paint.coat, paint.blend = coat, getattr(coat, "blend", None)
        mid_z = min(z for sec in secs for y, z in sec[1])
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
        for i, k0, k1 in piece.get("bars", []):
            # (a frame across one section from its point k0 to k1, each side: a post up the side; to the mirror of k0, across the roof)
            i = i % len(rings)
            runs = [range(k0, n - k0)] if k1 >= half_n - 1 else [range(k0, k1 + 1), range(n - 1 - k1, n - k0)]
            for ks in runs:
                parts.strip(frames, [rings[i][k] for k in ks], [facing(i, k) for k in ks], bar * 0.9, 0.006)
        for (i0, k0), (i1, k1) in piece.get("struts", []):
            # (a straight frame from a point of one section to a point of another, each side: a diagonal bar)
            for a, b in ((k0, k1), (n - 1 - k0, n - 1 - k1)):
                p0, p1 = rings[i0 % len(rings)][a], rings[i1 % len(rings)][b]
                pts = [p0 + (p1 - p0) * (t / 6.0) for t in range(7)]
                f0, f1 = facing(i0 % len(rings), a), facing(i1 % len(rings), b)
                parts.strip(frames, pts, [(f0 * (1.0 - t / 6.0) + f1 * (t / 6.0)).normalized() for t in range(7)], bar * 0.8, 0.006)
        if piece.get("glint") is not None and fine:
            k = piece["glint"]
            for side in (k, n - 1 - k):
                pts = [r[side] for r in rings[1:-1]]
                if len(pts) > 2:
                    parts.strip(glints, pts, [facing(i + 1, side) for i in range(len(pts))], 0.03, 0.004,
                                taper=lambda t: math.sin(math.pi * min(max(t, 0.02), 0.98)))


def on_skin(hull, x, z, hand):
    """The point of her side at this height (hand +1 her starboard side, -1 her port), and which way it faces."""
    lo, hi = -math.pi / 2.0, math.pi / 2.0                  # (round her starboard side from under her to over her top)
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        if hull.at(x, mid).z < z:
            lo = mid
        else:
            hi = mid
    a = 0.5 * (lo + hi)
    if hand < 0:
        a = math.pi - a
    return hull.at(x, a), hull.out(x, a)


def windows(parts, frames, glints, s, hull):
    """The small fixed windows in her sides behind her hood, (from, to along her, from, to in height): the dark of
    her inside seen through them, and a frame round each."""
    for x0, x1, z0, z1 in s.get("windows", []):
        xs = [x0 + (x1 - x0) * i / 4.0 for i in range(5)]
        zs = [z0 + (z1 - z0) * j / 3.0 for j in range(4)]
        for hand in (1.0, -1.0):
            bm = bmesh.new()
            grid = []
            for x in xs:
                row = []
                for z in zs:
                    p, nrm = on_skin(hull, x, z, hand)
                    row.append(bm.verts.new(p + nrm * 0.004))
                grid.append(row)
            for i in range(len(xs) - 1):
                for j in range(len(zs) - 1):
                    bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
            parts.add("hood", bm, "engine")
            ring = [(x, z0) for x in xs] + [(x1, z) for z in zs[1:]] + [(x, z1) for x in reversed(xs[:-1])] + [(x0, z) for z in reversed(zs[:-1])]
            on = [on_skin(hull, x, z, hand) for x, z in ring]
            parts.strip(frames, [p for p, n in on], [n for p, n in on], 0.016, 0.006)


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


# The sculptor's hood loop with the canopy laid in its place, and its glare panel lying on her nose (see planes.game);
# and a scoop that is on one side of her only (a fourth item, "one"): the Spitfire's radiator under her starboard
# wing and her oil cooler under her port one. Every other machine's scoops are as they were.
CANOPY = [
    # (and an aerial mast that rakes forward, the Zero's: a fourth item of `mast`, how far its top stands aft of its foot)
    ('        x, z0, z1 = s["mast"]\n'
     '        parts.rod("mast", (x, 0.0, z0), (x - 0.12, 0.0, z1), 0.022, 0.012, paint="frame", sides=6, detail=True)\n',
     '        x, z0, z1 = s["mast"][:3]\n'
     '        rake = s["mast"][3] if len(s["mast"]) > 3 else 0.12\n'
     '        parts.rod("mast", (x, 0.0, z0), (x - rake, 0.0, z1), 0.022, 0.012, paint="frame", sides=6, detail=True)\n'),
    ('        parts.rod("wire", (x - 0.12, 0.0, z1), top, 0.006, paint="frame", sides=4, detail=True)\n',
     '        parts.rod("wire", (x - rake, 0.0, z1), top, 0.006, paint="frame", sides=4, detail=True)\n'),
    ('    for stations, side, square in s.get("scoops", []):\n'
     '        for hand_side in ((side, -side) if side else (0.0,)):\n',
     '    for scoop_row in s.get("scoops", []):\n'
     '        stations, side, square = scoop_row[:3]\n'
     '        for hand_side in ((side, -side) if side and len(scoop_row) < 4 else (side,)):\n'),
    ('        hood = battle_cabin_body(Body, hood_stations, s["cabin"], hull, fine) if s.get("cabin") else Body(hood_stations, 2.3)\n',
     '        if s.get("canopy"):\n'
     '            battle_canopy(parts, frames, glints, s, hull, coat, PAINTS)\n'
     '            break\n'
     '        hood = battle_cabin_body(Body, hood_stations, s["cabin"], hull, fine) if s.get("cabin") else Body(hood_stations, 2.3)\n'),
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
    if hasattr(hull, "round_of"):
        return hull.round_of(c) > op[2]                      # (a fuselage that is not an oval: fitout.hull_body)
    a = math.degrees(math.atan2((c.z - hull.z(-c.x)) / hull.h(-c.x), abs(c.y) / hull.w(-c.x)))
    return a > op[2]


# ---- the P-51D Mustang -----------------------------------------------------------------------------------------
# Measured from "North American P-51D Mustang 3-view line drawing" (Commons; U.S. Air Force, public domain) at 94.6
# pixels to the metre, her length 9.83 m from spinner to rudder; the wing's span set to the real 11.28 m. What
# changed from the game's numbers: a fuselage that deepens behind the spinner to the chin intake and tapers to a slim
# tail; the bubble hood on its metal rail, its glass widest at the rail and ending in a point on her spine, over a
# windscreen with one flat armoured pane in front and a quarter pane each side; an open cockpit under it; the belly
# scoop well forward, under her seat, and long, rising into her belly far aft; a straight-tapered wing; the tall fin
# with its rudder hinged straight up and down, and the small fillet that runs it into her spine.
# Her paint (the art director, 2026-11-20, for the escort over Bremen in December 1943, where her silver was lost in the
# haze): olive drab over neutral grey, as the first Mustangs in England came, with the theatre's white recognition
# marks of that winter (told apart from the Bf 109): her spinner, and a band a foot wide round her nose straight behind
# it; a band fifteen inches wide straight across each wing, inboard, its inner edge about fifteen feet in from the tip;
# and one across each half of her tailplane, centred 33 inches in from its tip; top and bottom (`bands`, see
# planes.battle_bands). No band round her fin: her group's own record names only the wings and tailplane. Later coats
# by the month (planes.SCHEMES): natural metal from April 1944 with the same marks in black, then natural metal with
# her red nose. The olive drab panel ahead of her windscreen stops at her nose band.
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
    "square": 2.3, "screw": 1.70, "blades": 4, "nose_back": 3.0, "spinner": 5,
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
    "patch": 0, "colours": {"upper": "4C4B35", "upper2": "4C4B35", "under": "84878A", "nose": "E9E7DF", "boss": "E9E7DF",
                            "band": "E9E7DF", "band_under": "E9E7DF"},
    "bands": {"wing": (1.07, 1.45), "tail": (0.97, 1.35)},
    "stubs": (2.77, 1.87, 17.0), "well": (1.25, 0.42, 0.45), "well_in": True, "pitot": True, "glare": (3.0, 0.62, 0.17),
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

# ---- the Spitfire Mk I ------------------------------------------------------------------------------------------
# Measured from "Supermarine Spitfire Mk Vb" three-view line drawing (Commons; public domain), whose lines are the Mk I's
# but for her guns and her oil cooler: along her and up at 91 pixels to the metre, her length 9.12 m from spinner to
# rudder; across her at 87.4, her span the real 11.23 m (the drawing's two views are not quite at one scale). What
# changed from the game's numbers: a slim oval fuselage, deepest under her engine where her cowling's lower line falls
# away behind the spinner, and tapering to a knife-edged sternpost with the rudder hung behind it; the true elliptical
# wing, thickest at the root and with its six degrees of dihedral; her tailplane and her fin and rudder to their
# measured outline (the rudder runs down below her tail); her canopy: a windscreen with a flat front pane between
# straight posts and a quarter pane each side, its back frame leaning forward; the blown hood on its rails, the
# widest of its glass just above them, sliding back over her deck; and a small framed window each side behind it.
# Her radiator is under her starboard wing and her oil cooler under her port one, as on the real one. Her eight
# gun ports, her dark green and dark earth over sky and her roundels are as they were.
#
# Her hood, measured (x of its foot, the height of its sill where the glass begins, half its width there, its top).
_SPIT_HOOD = [(-0.252, 0.264, 0.315, 0.632), (-0.36, 0.264, 0.32, 0.643), (-0.47, 0.264, 0.32, 0.643), (-0.58, 0.264, 0.32, 0.623),
              (-0.67, 0.264, 0.318, 0.603), (-0.757, 0.264, 0.315, 0.58)]
_SPIT_SCREEN = (0.297, 0.19, 0.13)                          # (the foot of her windscreen's front pane, half its width there, and at its head)
_SPIT_LEAN = 0.34                                           # (her windscreen's back frame, and her hood's front bow on it, lean forward so much)
_SPIT_BAND = 0.04                                           # (the depth of the metal frame along the foot of her hood, on its rails)


def _spit_canopy(hull):
    foot_x, foot_half, head_half = _SPIT_SCREEN
    x0, sill, hw, top = _SPIT_HOOD[0]
    head = headed(bubble(hull, x0, sill, hw, top, square=2.6, band=_SPIT_BAND), head_half)
    post = len(head) - 2
    foot = [deck(hull, foot_x, foot_half)] * (len(head) - 1) + [deck(hull, foot_x, 0.0)]
    screen = {"sections": [(foot_x, foot, 0.0), (x0, head, _SPIT_LEAN)], "skirt": SKIRT, "hoops": [-1], "rails": [post, SKIRT], "bar": 0.02}
    hood = [(x, bubble(hull, x, sl, w, t, square=2.6, band=_SPIT_BAND), 0.0) for x, sl, w, t in _SPIT_HOOD[1:]]
    first = resample(head, len(hood[0][1]) - 1)
    hood = [(x0, first, _SPIT_LEAN)] + tween(x0, first, hood[0][0], hood[0][1], 3, _SPIT_LEAN, 0.0) + hood
    # (the hood's frames: its front bow and its rear bow, and its sill each side)
    slide = {"sections": hood, "skirt": SKIRT, "hoops": [0, -1], "rails": [SKIRT], "bar": 0.018}
    return [screen, slide]


SPITFIRE = {
    "title": "Spitfire Mk I (our own model)",
    "hull": [(3.55, 0.0, 0.02, 0.02), (3.50, 0.0, 0.08, 0.08), (3.44, 0.0, 0.126, 0.126), (3.35, 0.0, 0.181, 0.181), (3.22, 0.0, 0.236, 0.236),
             (3.09, 0.0, 0.269, 0.269), (2.97, 0.0, 0.297, 0.297),
             (2.91, -0.02, 0.32, 0.322), (2.82, -0.06, 0.352, 0.374), (2.69, -0.11, 0.366, 0.434), (2.43, -0.175, 0.378, 0.50), (2.17, -0.215, 0.383, 0.548),
             (1.90, -0.241, 0.389, 0.578), (1.64, -0.247, 0.395, 0.594), (1.24, -0.252, 0.41, 0.608), (0.85, -0.255, 0.435, 0.612), (0.30, -0.255, 0.45, 0.612),
             (-0.10, -0.245, 0.455, 0.615), (-0.75, -0.153, 0.446, 0.708), (-1.0, -0.152, 0.435, 0.698), (-1.53, -0.143, 0.412, 0.665), (-1.92, -0.1375, 0.378, 0.6375),
             (-2.32, -0.1265, 0.355, 0.6045), (-2.71, -0.1155, 0.329, 0.5605), (-3.11, -0.1045, 0.286, 0.5165), (-3.50, -0.099, 0.252, 0.467),
             (-3.90, -0.0935, 0.229, 0.4285), (-4.30, -0.0785, 0.172, 0.3885), (-4.56, -0.062, 0.126, 0.372), (-4.85, -0.05, 0.08, 0.34),
             (-4.97, -0.05, 0.04, 0.33), (-5.00, -0.05, 0.008, 0.26)],
    "square": 2.0, "screw": 1.63, "blades": 3, "nose_back": None, "spinner": 7,
    "hoods": hood_of([(_SPIT_SCREEN[0], 0.357, _SPIT_SCREEN[1], 0.01)] + [(x, sl, w, t - sl) for x, sl, w, t in _SPIT_HOOD]),
    "canopy": _spit_canopy,
    "opening": (-0.12, -0.72, 47.0),
    "windows": [(-0.80, -1.14, 0.355, 0.49)],
    "frames_paint": "upper",
    # (her elliptical wing: 13.2 per cent thick at the root, 9.4 at the tip; six degrees of dihedral from her root)
    # (its trailing edge sweeps back into her side in the fillet at its root)
    "wing": [(0.0, 1.68, -1.25, -0.72, 0.33), (0.45, 1.68, -1.19, -0.72, 0.33), (0.50, 1.68, -1.09, -0.715, 0.328), (0.55, 1.68, -0.99, -0.71, 0.326),
             (0.60, 1.68, -0.91, -0.704, 0.324), (0.67, 1.68, -0.84, -0.697, 0.322), (0.75, 1.68, -0.795, -0.688, 0.32), (0.85, 1.68, -0.775, -0.678, 0.316),
             (1.34, 1.68, -0.77, -0.627, 0.306), (2.03, 1.65, -0.72, -0.554, 0.285),
             (2.72, 1.60, -0.64, -0.482, 0.259), (3.40, 1.53, -0.51, -0.41, 0.227), (4.09, 1.43, -0.31, -0.338, 0.186), (4.66, 1.31, -0.09, -0.278, 0.146),
             (5.06, 1.19, 0.14, -0.236, 0.108), (5.35, 1.06, 0.35, -0.206, 0.073), (5.52, 0.92, 0.53, -0.188, 0.042), (5.58, 0.86, 0.63, -0.181, 0.026),
             (5.615, 0.77, 0.71, -0.178, 0.01)],
    "tail": [(0.0, -4.14, -5.36, 0.11, 0.14), (0.52, -4.15, -5.36, 0.11, 0.135), (0.89, -4.25, -5.33, 0.11, 0.125), (1.07, -4.31, -5.30, 0.11, 0.115),
             (1.25, -4.41, -5.26, 0.11, 0.10), (1.44, -4.54, -5.17, 0.11, 0.08), (1.53, -4.65, -5.09, 0.11, 0.055), (1.60, -4.80, -4.97, 0.11, 0.02)],
    # (the fin from below her tail up: its rudder runs down behind her sternpost to below her tail cone)
    "fin": [(0, -4.75, -5.10, -0.38, 0.04), (0, -4.55, -5.32, -0.30, 0.07), (0, -4.33, -5.466, -0.15, 0.09), (0, -4.15, -5.53, 0.0, 0.11),
            (0, -4.10, -5.554, 0.25, 0.12), (0, -4.27, -5.548, 0.40, 0.12), (0, -4.45, -5.515, 0.60, 0.11), (0, -4.535, -5.47, 0.80, 0.10),
            (0, -4.66, -5.375, 1.00, 0.08), (0, -4.745, -5.30, 1.10, 0.065), (0, -4.85, -5.215, 1.17, 0.045), (0, -5.0, -5.12, 1.21, 0.02)],
    "patch": 2.1, "colours": {"upper": "4A5237", "upper2": "7A6748", "under": "B5C4A8", "nose": "4A5237", "boss": "171717"},
    "stubs": (2.60, 1.65, 29.0), "well": (1.35, 0.4, 0.42), "pitot": True,
    "aileron": (0.57, 0.93, 0.76), "flap": (0.15, 0.55, 0.80),
    "guns": [(2.3, 0.02, 0.05, 0.03), (2.58, 0.02, 0.05, 0.03), (3.21, 0.02, 0.05, 0.03), (3.49, 0.02, 0.05, 0.03)],
    "scoops": [
        # (her radiator, deep and square, under her starboard wing; her oil cooler, small and round, under her port one)
        ([(0.70, -0.90, 0.16, 0.15), (0.62, -0.915, 0.18, 0.185), (0.30, -0.92, 0.19, 0.19), (-0.10, -0.90, 0.18, 0.16), (-0.35, -0.85, 0.14, 0.10),
          (-0.45, -0.80, 0.08, 0.05)], 1.05, 3.6, "one"),
        ([(1.62, -0.88, 0.08, 0.08), (1.55, -0.885, 0.10, 0.11), (1.25, -0.88, 0.105, 0.12), (0.95, -0.85, 0.09, 0.09), (0.80, -0.80, 0.05, 0.04)], -1.0, 2.0, "one")],
    "tailwheel": (-4.62, -0.66), "mast": (-1.31, 0.56, 1.18),
    "side_marks": [(-2.45, 0.34, "UK-A1")], "wing_marks": [(3.7, 0.05, 0.62, "UK-B", "UK", True, True)], "fin_marks": [(-4.78, 0.75, 0.2, "UK-FIN")], "code": (-1.5, 0.32),
}

# ---- the Bf 109E ------------------------------------------------------------------------------------------------
# Measured from "Messerschmitt Bf 109 E" three-view line drawing by B. Huber (Commons, CC BY-SA 2.0 de: its measures
# only are used), at 125.5 pixels to the metre along her and up, her length 8.64 m from spinner to rudder; across her
# at 127.3, her span the real 9.87 m. What changed from the game's numbers: the blunt spinner; a cowling that swells
# behind it and is deepest under her exhausts, with the oil cooler under her chin and the supercharger's intake on her
# port side; a fuselage narrower than her cowling at the cockpit, tall and slim behind it, ending in a knife edge at
# her rudder post; her straight-tapered wing with its rounded tips and six degrees of dihedral, and her shallow
# radiators under it; her tailplane high on the fin, braced from below by a strut each side; the fin and the rudder
# with its rounded top to their measured outline. Her canopy is of flat panes in heavy frames: a windscreen with a
# flat front pane, its side panes split by a diagonal bar; the hood with flat sides and a flat roof, a post halfway
# along each side; and the glazed wedge behind it that runs down into her spine. Her yellow nose, her greens over pale
# blue and her crosses are as they were.
_BF_SILL = 0.46                                             # (the line of her cockpit's sill along her sides, under all her glass)
_BF_ROOF = (0.16, 0.795, 0.845)                             # (her hood's roof: how far out its corners are, their height, and its middle's)


def _bf_canopy(hull):
    yc, zc, ridge = _BF_ROOF
    # her windscreen: its front pane's foot lies on her cowling; its top, and its back where her hood begins
    foot = box(hull, 0.69, _BF_SILL, deck(hull, 0.69, 0.16, 0.004), fitout.body_top(hull, 0.69) - 0.004)
    screen = {"sections": [(0.69, foot), (0.54, box(hull, 0.54, _BF_SILL, (0.135, 0.812), 0.823)),
                           (0.355, box(hull, 0.355, _BF_SILL, (yc, 0.79), 0.81))],
              "skirt": SKIRT, "hoops": [-1], "rails": [3, 2], "bars": [(1, 3, 4)], "struts": [((1, 3), (2, 2))], "bar": 0.022}
    hood = {"sections": [(x, box(hull, x, _BF_SILL, (yc, zc), ridge)) for x in (0.335, 0.01, -0.335)],
            "skirt": SKIRT, "hoops": [0, -1], "rails": [3, 2], "bars": [(1, 2, 3)], "bar": 0.022}
    # (the glazed wedge behind her hood: its sill climbs her side to her spine, and its roof runs on to meet it)
    back = []
    for x in (-0.345, -0.43, -0.50, -0.56, -0.61, -0.645, -0.665):
        zs = _BF_SILL + (-0.345 - x) / 0.32 * 0.39
        ys = side_at(hull, x, zs)
        back.append((x, box(hull, x, zs, (max(min(yc, ys - 0.012), 0.004), max(zc, zs + 0.012)), max(ridge, zs + 0.02))))
    rear = {"sections": back, "skirt": SKIRT, "rails": [3, 2, 4], "bar": 0.022}
    return [screen, hood, rear]


BF109 = {
    "title": "Bf 109E (our own model)",
    "hull": [(3.30, 0.0, 0.09, 0.09), (3.28, 0.0, 0.115, 0.115), (3.24, 0.0, 0.14, 0.14), (3.20, 0.0, 0.167, 0.167), (3.12, 0.0, 0.207, 0.207),
             (3.04, 0.0, 0.25, 0.25), (3.0, 0.0, 0.268, 0.268),
             (2.96, 0.0, 0.29, 0.295), (2.90, 0.008, 0.31, 0.34), (2.83, 0.02, 0.33, 0.37), (2.71, 0.012, 0.35, 0.42), (2.58, 0.01, 0.362, 0.452),
             (2.39, 0.018, 0.372, 0.478), (2.08, 0.029, 0.382, 0.499), (1.77, 0.043, 0.388, 0.513), (1.45, 0.057, 0.39, 0.527), (1.13, 0.064, 0.387, 0.544),
             (0.81, 0.072, 0.382, 0.552), (0.40, 0.09, 0.377, 0.57), (0.0, 0.11, 0.377, 0.59), (-0.40, 0.16, 0.365, 0.63), (-0.78, 0.2095, 0.35, 0.6455),
             (-1.10, 0.2175, 0.336, 0.6215), (-1.42, 0.2295, 0.322, 0.5935), (-1.74, 0.2415, 0.305, 0.5655), (-2.06, 0.2575, 0.287, 0.5335),
             (-2.38, 0.269, 0.265, 0.498), (-2.70, 0.285, 0.238, 0.466), (-3.01, 0.301, 0.208, 0.426), (-3.33, 0.3215, 0.18, 0.3825),
             (-3.65, 0.339, 0.149, 0.337), (-4.00, 0.36, 0.12, 0.295), (-4.40, 0.39, 0.085, 0.24), (-4.62, 0.405, 0.05, 0.205), (-4.70, 0.41, 0.01, 0.19)],
    "square": 2.2, "screw": 1.5, "blades": 3, "nose_back": 1.75, "spinner": 7,
    "hoods": hood_of([(0.69, 0.632, 0.17, 0.01), (0.54, 0.56, 0.26, 0.26), (0.355, _BF_SILL, 0.29, 0.35), (0.0, _BF_SILL, 0.29, 0.385),
                      (-0.335, _BF_SILL, 0.30, 0.385), (-0.665, 0.84, 0.03, 0.02)]),
    "canopy": _bf_canopy,
    "opening": (0.36, -0.34, 40.0),
    "frames_paint": "upper",
    # (her wing: straight-tapered, its tips rounded; six degrees of dihedral)
    "wing": [(0.0, 1.56, -0.70, -0.461, 0.33), (0.40, 1.56, -0.64, -0.417, 0.325), (0.50, 1.56, -0.55, -0.406, 0.32), (0.63, 1.555, -0.513, -0.392, 0.315),
             (1.18, 1.519, -0.417, -0.331, 0.29), (1.96, 1.471, -0.282, -0.245, 0.255), (2.75, 1.424, -0.146, -0.159, 0.22), (3.54, 1.376, -0.003, -0.072, 0.18),
             (4.32, 1.328, 0.101, 0.014, 0.145), (4.71, 1.304, 0.212, 0.057, 0.12), (4.84, 1.288, 0.26, 0.071, 0.11), (4.886, 1.232, 0.547, 0.076, 0.075),
             (4.918, 1.161, 0.746, 0.08, 0.045), (4.935, 1.05, 0.90, 0.082, 0.015)],
    # (her tailplane, high on her fin)
    "tail": [(0.0, -3.99, -4.91, 0.815, 0.11), (0.39, -4.02, -4.93, 0.815, 0.105), (0.71, -4.06, -4.91, 0.815, 0.10), (1.02, -4.10, -4.86, 0.815, 0.09),
             (1.26, -4.13, -4.84, 0.815, 0.08), (1.35, -4.17, -4.78, 0.815, 0.065), (1.40, -4.25, -4.65, 0.815, 0.045), (1.43, -4.38, -4.50, 0.815, 0.015)],
    # (her fin and rudder: the rudder hinged at her rudder post, its foot sloping up aft, its top rounded over)
    "fin": [(0, -3.9, -4.75, 0.22, 0.10), (0, -3.75, -5.34, 0.45, 0.11), (0, -3.72, -5.33, 0.68, 0.11), (0, -3.89, -5.29, 0.86, 0.10),
            (0, -4.03, -5.22, 1.05, 0.085), (0, -4.17, -5.07, 1.27, 0.07), (0, -4.30, -4.86, 1.45, 0.05), (0, -4.42, -4.68, 1.52, 0.03),
            (0, -4.55, -4.60, 1.535, 0.01)],
    "patch": 1.9, "colours": {"upper": "4B5442", "upper2": "79806C", "under": "8FB0BF", "nose": "D8B600", "boss": "3A4034"},
    "stubs": (2.44, 1.55, -18.0), "well": (1.55, 0.36, 0.42), "guns": [(2.14, 0.1, 0.28, 0.025)], "pitot": True,
    "aileron": (0.60, 0.95, 0.74), "flap": (0.12, 0.58, 0.74),
    "scoops": [
        # (her radiators, shallow, under her wings either side of her; the oil cooler under her chin; the supercharger's intake on her port side)
        ([(0.55, -0.47, 0.17, 0.06), (0.48, -0.48, 0.19, 0.075), (0.15, -0.48, 0.20, 0.08), (-0.15, -0.47, 0.19, 0.07), (-0.35, -0.45, 0.15, 0.05),
          (-0.45, -0.43, 0.10, 0.03)], 0.68, 3.0),
        ([(2.30, -0.44, 0.09, 0.05), (2.22, -0.45, 0.12, 0.075), (1.95, -0.45, 0.13, 0.075), (1.72, -0.44, 0.11, 0.06), (1.60, -0.42, 0.07, 0.03)], 0.0, 2.5),
        ([(1.62, 0.13, 0.055, 0.06), (1.55, 0.13, 0.065, 0.07), (1.35, 0.12, 0.05, 0.055), (1.22, 0.11, 0.02, 0.02)], -0.42, 2.4, "one")],
    "tailwheel": (-4.37, -0.03), "mast": (-0.70, 0.85, 1.24),
    "extras": [("rod", ((-4.27, 0.50, 0.77), (-4.10, 0.12, 0.15), 0.022, 0.018, "under"))],
    "side_marks": [(-2.05, 0.33, "GER")], "wing_marks": [(3.05, 0.2, 0.42, "GER", "GER", True, True)], "code": (-1.2, 0.3),
}

# ---- the A6M2 Zero ----------------------------------------------------------------------------------------------
# Measured from "Mitsubishi A6M2 Zero" three-view line drawing (Commons; its measures only are used), along her and up
# at 149.5 pixels to the metre, her length 9.06 m from spinner to tail cone; across her at 148.2, her span the real
# 12.0 m. What changed from the game's numbers: the small pointed spinner and the hub behind it, the engine's face
# deep in the open mouth of a cowling a little taller than it is wide, deepest under it where the oil cooler's intake
# hangs; a fuselage nearly as wide as the cowling at the cockpit, tapering to a slim tail cone that runs out under her
# rudder to a point; her broad wing, straight-tapered to round tips, fifteen per cent thick at the root and nine at
# the tip, with five and a half degrees of dihedral; her tailplane low on her tail, and the fin with its long fillet
# and its rudder rounded over at the top. Her canopy is the long greenhouse of many frames: a windscreen with a flat
# front pane; the sliding hood, its side panes leaning in to a frame at the shoulder and its roof rounded over in
# narrow panes either side of a middle frame, three hoops across it; the fixed glazing behind, lower, and its glazed
# end that runs down to a point on her spine; and her radio mast, raked forward, standing on it. Her pale grey and
# black cowling and her plain red discs are as they were.
#
# Her greenhouse, measured (x, the height of its sill where the glass begins, how far out it is there, the height of
# its top): the sliding hood, then the fixed glazing behind it, then its end. Its shoulder frame stands 0.19 out.
_ZERO_HOOD = [(1.004, 0.585, 0.338, 1.003), (0.83, 0.585, 0.338, 1.008), (0.65, 0.585, 0.338, 1.012), (0.45, 0.585, 0.338, 1.005),
              (0.262, 0.585, 0.338, 0.995)]
_ZERO_BACK = [(0.248, 0.565, 0.322, 0.976), (0.05, 0.565, 0.322, 0.928), (-0.153, 0.565, 0.322, 0.878), (-0.30, 0.565, 0.322, 0.843),
              (-0.454, 0.565, 0.322, 0.805), (-0.55, 0.565, 0.33, 0.765), (-0.65, 0.568, 0.322, 0.728), (-0.75, 0.58, 0.293, 0.70),
              (-0.83, 0.59, 0.255, 0.672), (-0.90, 0.598, 0.19, 0.642), (-0.94, 0.602, 0.12, 0.62), (-0.968, 0.604, 0.04, 0.609)]
_ZERO_SCREEN = (1.586, 0.15, 1.171, 0.976)                  # (the foot of her windscreen's front pane, half its width, its head, and the top there)


def _zero_shoulder(x, sill, ys, top):
    """The frame at the shoulder of her greenhouse: 0.19 out along her hood and the glazing behind it, the panes above it
    rising a third of the way from it to the top; in the glazed end it stands out nearer the sill as that narrows."""
    if x > -0.454:
        return (0.19, top - (top - sill) * (0.31 if x > 0.255 else 0.26))
    t = min(1.0, (-0.454 - x) / 0.2)
    return (0.19 + (0.8 * ys - 0.19) * t, top - (top - sill) * (0.26 + 0.04 * t))


def _zero_canopy(hull):
    foot_x, foot_half, head_x, head_top = _ZERO_SCREEN
    x0, sill, ys, top = _ZERO_HOOD[0]
    sec = {x: green(hull, x, sl, w, _zero_shoulder(x, sl, w, t), t) for x, sl, w, t in _ZERO_HOOD + _ZERO_BACK}
    # her windscreen: the flat front pane from its foot on her deck up to its head, and the side panes each side from
    # the foot of its posts back and out to the rail; its roof on to the hood's front frame
    head = headed(green(hull, head_x, sill, ys, _zero_shoulder(head_x, sill, ys, head_top), head_top), foot_half)
    post = len(head) - 2
    foot = [deck(hull, foot_x, foot_half)] * (len(head) - 1) + [deck(hull, foot_x, 0.0)]
    screen = {"sections": [(foot_x, foot), (head_x, head), (x0, sec[x0] if len(sec[x0]) == len(head) else resample(sec[x0], len(head) - 1))], "skirt": SKIRT, "hoops": [1, 2],
              "rails": [post, SKIRT], "bar": 0.02}
    n = len(sec[x0]) - 1
    # the sliding hood: three hoops, its sills, its shoulder frames and the frame along its middle
    hood = {"sections": [(x, sec[x]) for x, sl, w, t in _ZERO_HOOD], "skirt": SKIRT, "hoops": [0, 2, 4], "rails": [SKIRT, 3, n], "bar": 0.018}
    # the fixed glazing behind it, lower, with its three hoops, and its glazed end running down to her spine
    back = {"sections": [(x, sec[x]) for x, sl, w, t in _ZERO_BACK], "skirt": SKIRT, "hoops": [0, 2, 4], "rails": [SKIRT, 3], "bar": 0.018,
            "cap": True}
    return [screen, hood, back]


ZERO = {
    "title": "A6M2 Zero (our own model)",
    "hull": [(3.753, 0.0, 0.012, 0.012), (3.74, 0.0, 0.062, 0.062), (3.71, 0.0, 0.107, 0.107), (3.66, 0.0, 0.149, 0.149), (3.60, 0.0, 0.181, 0.181),
             (3.53, 0.0, 0.205, 0.205), (3.47, 0.0, 0.218, 0.218), (3.43, 0.0, 0.228, 0.228), (3.275, 0.0, 0.238, 0.238),
             (3.25, -0.005, 0.36, 0.36), (3.238, -0.015, 0.43, 0.445), (3.225, -0.017, 0.465, 0.48), (3.205, -0.017, 0.492, 0.51),
             (3.19, -0.017, 0.505, 0.525), (3.15, -0.02, 0.531, 0.555), (3.07, -0.027, 0.556, 0.589), (2.99, -0.032, 0.574, 0.606),
             (2.86, -0.03, 0.58, 0.615), (2.72, -0.029, 0.579, 0.62), (2.59, -0.023, 0.567, 0.622), (2.45, -0.018, 0.56, 0.625),
             (2.31, -0.013, 0.552, 0.628), (2.25, -0.025, 0.551, 0.61), (2.05, -0.012, 0.553, 0.617), (1.79, 0.006, 0.55, 0.626),
             (1.586, 0.027, 0.546, 0.642), (1.25, 0.0375, 0.543, 0.6425), (1.15, 0.038, 0.542, 0.6425), (0.85, 0.04, 0.533, 0.64), (0.45, 0.04, 0.524, 0.635),
             (0.05, 0.0375, 0.513, 0.6275), (-0.35, 0.0325, 0.508, 0.6175), (-0.62, 0.03, 0.503, 0.605), (-0.89, 0.025, 0.491, 0.585),
             (-1.156, 0.022, 0.474, 0.57), (-1.424, 0.022, 0.457, 0.557), (-1.692, 0.0205, 0.444, 0.5385), (-1.959, 0.0165, 0.423, 0.5215),
             (-2.227, 0.0165, 0.401, 0.5015), (-2.494, 0.018, 0.38, 0.48), (-2.762, 0.02, 0.351, 0.458), (-3.03, 0.0205, 0.325, 0.4315),
             (-3.43, 0.0145, 0.303, 0.3925), (-3.83, 0.019, 0.26, 0.343), (-4.10, 0.024, 0.22, 0.308), (-4.37, 0.0245, 0.17, 0.2755),
             (-4.50, 0.04, 0.148, 0.26), (-4.77, 0.0665, 0.108, 0.2335), (-5.04, 0.0765, 0.067, 0.2235), (-5.10, 0.057, 0.06, 0.197),
             (-5.24, -0.047, 0.035, 0.067), (-5.30, -0.10, 0.01, 0.01)],
    "square": 2.0, "screw": 1.45, "blades": 3, "nose_back": 2.31, "spinner": 9, "engine": "radial", "lip": 3.245,
    "hoods": hood_of([(_ZERO_SCREEN[0], 0.669, _ZERO_SCREEN[1], 0.01), (_ZERO_SCREEN[2], 0.585, 0.338, _ZERO_SCREEN[3] - 0.585)]
                     + [(x, sl, w, t - sl) for x, sl, w, t in _ZERO_HOOD + _ZERO_BACK]),
    "canopy": _zero_canopy,
    "opening": (1.15, -0.62, 56.0),                         # (her cockpit, open under the glass: its padded rim inboard of the rails)
    "frames_paint": "upper",
    # (her wing: fifteen per cent thick at the root, nine at the tip; five and a half degrees of dihedral; its trailing
    #  edge swept back into her side in the fillet at its root)
    "wing": [(0.0, 2.08, -0.80, -0.446, 0.47), (0.45, 2.05, -0.78, -0.402, 0.443), (0.50, 2.045, -0.76, -0.397, 0.44), (0.55, 2.04, -0.70, -0.393, 0.437),
             (0.62, 2.035, -0.554, -0.386, 0.433), (0.70, 2.027, -0.487, -0.378, 0.428), (1.30, 1.983, -0.382, -0.32, 0.392), (2.00, 1.933, -0.26, -0.252, 0.351),
             (2.70, 1.882, -0.138, -0.184, 0.31), (3.40, 1.831, -0.016, -0.116, 0.268), (4.10, 1.781, 0.107, -0.048, 0.227), (4.68, 1.747, 0.208, 0.008, 0.193),
             (5.02, 1.72, 0.268, 0.041, 0.171), (5.29, 1.686, 0.329, 0.067, 0.155), (5.56, 1.593, 0.449, 0.093, 0.139), (5.70, 1.52, 0.54, 0.107, 0.12),
             (5.83, 1.426, 0.643, 0.12, 0.09), (5.92, 1.312, 0.757, 0.128, 0.065), (5.965, 1.225, 0.837, 0.133, 0.045), (5.99, 1.14, 0.93, 0.135, 0.025),
             (6.0, 1.06, 1.0, 0.136, 0.01)],
    "tail": [(0.0, -2.90, -4.42, 0.11, 0.17), (0.364, -2.972, -4.38, 0.11, 0.165), (0.499, -3.023, -4.367, 0.11, 0.16), (0.769, -3.116, -4.337, 0.11, 0.148),
             (1.039, -3.21, -4.31, 0.11, 0.135), (1.309, -3.304, -4.283, 0.11, 0.12), (1.579, -3.394, -4.247, 0.11, 0.105), (1.849, -3.487, -4.223, 0.11, 0.09),
             (2.119, -3.588, -4.156, 0.11, 0.07), (2.24, -3.671, -4.073, 0.11, 0.055), (2.294, -3.732, -4.006, 0.11, 0.04), (2.321, -3.778, -3.962, 0.11, 0.03),
             (2.36, -3.85, -3.89, 0.11, 0.01)],
    # (the fin from the top of her tail cone up, the long fillet running into its leading edge; the rudder above her tail cone)
    "fin": [(0, -3.00, -5.07, 0.30, 0.13), (0, -3.08, -5.055, 0.42, 0.125), (0, -3.193, -5.03, 0.488, 0.12), (0, -3.277, -5.013, 0.555, 0.112),
            (0, -3.384, -4.972, 0.689, 0.105), (0, -3.491, -4.932, 0.823, 0.097), (0, -3.595, -4.896, 0.957, 0.09), (0, -3.702, -4.859, 1.09, 0.082),
            (0, -3.805, -4.819, 1.224, 0.075), (0, -3.912, -4.785, 1.358, 0.066), (0, -4.026, -4.708, 1.492, 0.055), (0, -4.116, -4.638, 1.559, 0.045),
            (0, -4.21, -4.571, 1.599, 0.035), (0, -4.277, -4.501, 1.625, 0.025), (0, -4.36, -4.42, 1.645, 0.01)],
    "patch": 0.0, "colours": {"upper": "AAB2A0", "upper2": "AAB2A0", "under": "9EA694", "nose": "1E1F22", "boss": "8E9092"},
    "well": (0.95, 0.32, 0.36), "guns": [(2.25, 0.05, 0.14, 0.026)], "pitot": True,
    "aileron": (0.38, 0.92, 0.80), "flap": (0.09, 0.38, 0.78),
    "scoops": [
        # (the oil cooler's intake, hanging under the after part of her cowling)
        ([(2.56, -0.665, 0.09, 0.05), (2.50, -0.685, 0.13, 0.07), (2.40, -0.68, 0.14, 0.072), (2.20, -0.668, 0.13, 0.062), (2.00, -0.645, 0.10, 0.04),
          (1.85, -0.625, 0.05, 0.02)], 0.0, 2.6)],
    "mast": (-0.515, 0.79, 1.52, -0.123),
    "side_marks": [(-2.3, 0.3, "JAP-PLAIN")], "wing_marks": [(3.9, 0.15, 0.55, "JAP-PLAIN", "JAP-PLAIN", True, True)], "code": (-3.35, 0.36),
}

FIGHTERS = {"Mustang": MUSTANG, "Spitfire": SPITFIRE, "Bf109": BF109, "Zero": ZERO}
