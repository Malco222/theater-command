"""Battle damage for an aircraft: bullet holes and flak holes, as a model of its own that lies on her skin.

Brian, 2026-11-18: "if a plane gets hit show the bullet holes or flak holes".

    overlay("B17")  ->  the model "B17Damage"

It has every part of the aircraft it is for (same names, parents and pivots, no faces of their own) and every clip
of hers, so whatever poses her poses it: lay it on her at the same place and play the same clips on both. Its own
faces are in parts `damage.1`, `damage.2`, ...: each one hit, hanging on the part of her it struck (a wing, the
tail), so it goes with that part when she breaks up. Each is either

- "bullets": a raking row of holes, each a dark hole whose lip shows bare metal on one stretch of its rim only; or
- "flak": one ragged tear, a few petals of torn metal bent into it (most dark, one or two catching the light), a
  halo of soot round it, and a scatter of small splinter holes.

Six flat paints: `hole`, `torn` (bare metal in the light), `lip` (bare metal seen faintly through her paint), `bent`
(torn metal in the dark of the hole), `soot` and `soot_thin`. Three are see-through: the file says how much under
`clear` (lip 0.4, soot 0.35, soot_thin 0.17), as an aircraft's file does for her glass.

Holes are drawn larger than life (a real 13 mm hole is not seen from a battle camera): about 8 cm across on a
fighter, 10 cm on a heavy bomber; a tear 30 to 55 cm.

Three clips of its own show them: `damage` (seconds stand for hits: at time k the first k are there, all kinds in an
order that spreads them over her), `damage.bullets` and `damage.flak` (the same for one kind each; lay one over the
other). At rest every hit shows. An aircraft not hit simply has no overlay.

The file's `damage` lists every hit: its part, kind, the part of her it is on, where, and which way it faces.
"""
import math
import random

from mathutils import Vector
from mathutils.bvhtree import BVHTree

import planes
from rigid import Model, Part

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
SKIN = {"upper", "upper2", "under", "nose"}                  # her skin, and
LAID = {"line", "rivet", "rivet_under", "stain", "soot", "patch"}     # what lies on it: a hole may lie over these too


PAINTS = {"hole": "0A0A0B", "torn": "7E8287", "lip": "7E8287", "bent": "2B2D2F", "soot": "2A2622", "soot_thin": "2A2622"}
CLEAR = {"lip": 0.4, "soot": 0.35, "soot_thin": 0.17}       # how much of each see-through paint shows over her own


def look(paint, mat, nt, bsdf, mix, **kw):
    """For the pictures: the hole is dead black, the torn metal bare, the lip and the soot seen through."""
    if paint in ("hole", "soot", "soot_thin"):
        bsdf.inputs["Roughness"].default_value = 1.0
    else:                                                    # (bright from every side: polished metal would mirror a dark sea)
        bsdf.inputs["Metallic"].default_value = 0.15
        bsdf.inputs["Roughness"].default_value = 0.5 if paint != "bent" else 0.8
    if paint in CLEAR:
        bsdf.inputs["Alpha"].default_value = CLEAR[paint]


def overlay(name, fine=True, model=None):
    """The damage model for the aircraft `name` (`model`: hers, if it is already built)."""
    m = model if model is not None else planes.BUILDERS[name](fine)[0]
    rng = random.Random(sum(ord(c) * (i + 3) for i, c in enumerate(name)))
    tree = BVHTree.FromPolygons([tuple(v) for v in m.verts], [tuple(f) for f in m.faces])
    paint_names = list(m.paints)

    def cast(o, way, far=9.0):
        way = Vector(way).normalized()
        loc, nor, idx, _ = tree.ray_cast(Vector(o), way, far)
        if loc is None:
            return None
        if nor.dot(way) > 0.0:
            nor = -nor
        return loc, nor.normalized(), m.parts[m.fpart[idx]].name, paint_names[m.fpaint[idx]].split(".")[0]

    def good(h):
        return h is not None and (h[3] in SKIN or h[3] in LAID)

    def on_skin(p, n, lift):
        """The point of her skin under p (seen along -n), stood off it by `lift`."""
        h = cast(p + n * 0.3, -n, 0.75)
        return h[0] + h[1] * lift if good(h) else None

    S = {s["name"]: s["at"] for s in m.sockets}
    span = 2.0 * abs(S["wingtip.port"].y)
    big = min(2.0, max(1.0, span / 14.0))                    # (1 for a fighter, 2 for a four-engined bomber)
    r_hole = 0.036 * (0.8 + 0.25 * big)
    r_tear = 0.15 * (0.55 + 0.45 * big) * 1.25

    d = Model(name + "Damage", m.title + ": battle damage", "damage", dict(PAINTS), fine)
    d.parts = [Part(p.name, p.parent, p.pivot, axis=p.axis, limits=p.limits, slide=p.slide, slide_limits=p.slide_limits, kind=p.kind, note=p.note) for p in m.parts]
    d.part_at = dict(m.part_at)
    d.clips = {k: dict(c, events=[]) for k, c in m.clips.items()}          # (what she does, it does; her sounds and effects stay hers)
    hits = []

    def hole(c, n, r0, sides=9):
        """One hole at c on her skin: (points, {paint: faces}), or None if it runs off her."""
        u = n.orthogonal().normalized()
        v = n.cross(u)
        a0 = rng.uniform(0.0, 6.283)
        mid = on_skin(c, n, 0.004)
        if mid is None:
            return None
        lit0, lit_n = rng.randrange(sides), (rng.choice((2, 2, 3)) if sides >= 7 else 1)     # (bare metal shows on one stretch of the rim: a whole ring of it was a washer stuck on her)
        inner, outer = [], []
        for i in range(sides):
            a = a0 + 6.283 * i / sides
            way = u * math.cos(a) + v * math.sin(a)
            r1 = r0 * rng.uniform(0.55, 1.1)
            lit = (i - lit0) % sides <= lit_n
            p1 = on_skin(c + way * r1, n, 0.009 * big ** 0.5 if lit else 0.006)   # (where it is lit its lip stands up)
            p2 = on_skin(c + way * (r1 + r0 * rng.uniform(0.2, 0.4)), n, 0.0035)
            if p1 is None or p2 is None:
                return None
            inner.append(p1)
            outer.append(p2)
        pts = [mid] + inner + outer
        faces = {"hole": [(0, 1 + i, 1 + (i + 1) % sides) for i in range(sides)], "torn": [], "lip": []}
        for i in range(sides):
            j = (i + 1) % sides
            faces["torn" if (i - lit0) % sides < lit_n else "lip"] += [(1 + i, 1 + sides + i, 1 + sides + j), (1 + i, 1 + sides + j, 1 + j)]
        return pts, faces

    def tear(c, n, R, sides=13, around=20):
        """A ragged hole a shell splinter tore, longer one way than the other: a few petals of torn metal bent into
        it, most of them in its dark and one or two catching the light, and a halo of soot round it."""
        u = n.orthogonal().normalized()
        v = n.cross(u)
        a0, turn, stretch = rng.uniform(0.0, 6.283), rng.uniform(0.0, 3.1416), rng.uniform(1.15, 1.5)
        e = u * math.cos(turn) + v * math.sin(turn)
        f = n.cross(e)
        mid = on_skin(c, n, 0.011)
        if mid is None:
            return None

        def way_of(a):
            w = u * math.cos(a) + v * math.sin(a)
            return e * (w.dot(e) * stretch) + f * (w.dot(f) / stretch ** 0.5)

        half, rim, edge, radii = [], [], [], []
        ph1, ph2 = rng.uniform(0.0, 6.283), rng.uniform(0.0, 6.283)
        for i in range(sides):
            a = a0 + 6.283 * i / sides
            way = way_of(a)
            # (lobes and a few notches: every other point long and short made a star)
            r = R * (0.8 + 0.13 * math.sin(2.0 * a + ph1) + 0.07 * math.sin(3.0 * a + ph2)) * (rng.uniform(0.62, 0.8) if rng.random() < 0.25 else rng.uniform(0.94, 1.06))
            p0, p1 = on_skin(c + way * r * 0.5, n, 0.011), on_skin(c + way * r, n, 0.012)
            p2 = on_skin(c + way * (r + R * rng.uniform(0.04, 0.08)), n, 0.0125)
            if p0 is None or p1 is None or p2 is None:
                return None
            half.append(p0)
            rim.append(p1)
            edge.append(p2)
            radii.append(r)
        pts = [mid] + half + rim + edge
        H, RM, E = 1, 1 + sides, 1 + 2 * sides
        lip0, lip_n = rng.randrange(sides), rng.choice((3, 4, 5))
        faces = {"hole": [], "lip": [], "bent": [], "torn": [], "soot": [], "soot_thin": []}
        for i in range(sides):
            j = (i + 1) % sides
            faces["hole"] += [(0, H + i, H + j), (H + i, RM + i, RM + j), (H + i, RM + j, H + j)]
            if (i - lip0) % sides < lip_n or rng.random() < 0.15:           # (a lip on part of the rim only: all round, it outlined the tear like a sticker)
                faces["lip"] += [(RM + i, E + i, E + j), (RM + i, E + j, RM + j)]
        # the petals: three to six of them, no two alike, folded in over the dark
        count = rng.choice((3, 4, 4, 5, 6))
        sides_with = sorted(rng.sample(range(sides), count))
        lit = set(rng.sample(sides_with, rng.choice((1, 1, 2))))
        for i in sides_with:
            j = (i + 1) % sides
            base = (rim[i] + rim[j]) * 0.5 - mid
            reach = rng.uniform(0.28, 0.85)                                  # (how far in it is bent: no two the same)
            skew = (rim[j] - rim[i]) * rng.uniform(-0.45, 0.45)
            tip = mid + base * (1.0 - reach) + skew + n * 0.006
            pts.append(tip)
            faces["torn" if i in lit else "bent"].append((RM + i, len(pts) - 1, RM + j))
        # the soot: two rings of it, the outer one thinner, each as far round her as her skin goes
        def as_far(way, r, lift):
            for k in (1.0, 0.85, 0.7):
                p = on_skin(c + way * (r * k), n, lift)
                if p is not None:
                    return p
            return None

        aft = -X - n * (-X).dot(n)                           # (the wind lays the soot back along her)
        aft = aft.normalized() if aft.length > 0.25 else None
        rings = [[], [], []]
        for i in range(around):
            a = a0 + 6.283 * i / around
            way = way_of(a)
            r_rim = radii[int(round((i / around) * sides)) % sides]
            blown = 1.0 + 0.8 * max(0.0, way.normalized().dot(aft)) ** 2 if aft is not None else 1.0
            r1 = R * rng.uniform(1.15, 1.6) * blown
            r2 = r1 + R * rng.uniform(0.25, 0.75) * blown
            for ring, r, lift in ((0, r_rim * 0.9, 0.006), (1, r1, 0.008), (2, r2, 0.008)):
                p = as_far(way, r, lift)
                rings[ring].append(None if p is None else len(pts))
                if p is not None:
                    pts.append(p)
        for i in range(around):
            j = (i + 1) % around
            for ring, paint in ((0, "soot"), (1, "soot_thin")):
                q = (rings[ring][i], rings[ring + 1][i], rings[ring + 1][j], rings[ring][j])
                if None not in q:
                    faces[paint] += [(q[0], q[1], q[2]), (q[0], q[2], q[3])]
        return pts, faces

    def lay(pieces, kind, c, n, on):
        """A hit made of `pieces`, as the next part."""
        k = len(hits) + 1
        part = d.part(f"damage.{k}", on, pivot=tuple(c), kind="fixed", note=kind)
        d.use(part)
        for (pts, faces), nn in pieces:
            for paint in ("soot_thin", "soot", "hole", "lip", "bent", "torn"):         # (what lies lowest first)
                if faces.get(paint):
                    d.add(pts, faces[paint], paint, smooth=False, out=lambda mid, nn=nn: nn)
        hits.append({"part": part, "kind": kind, "on": on, "at": [round(x, 3) for x in c], "facing": [round(x, 3) for x in n], "holes": len(pieces)})

    def bullets(h):
        c, n, on, _ = h
        fore = (X - n * X.dot(n))
        fore = fore.normalized() if fore.length > 0.2 else n.orthogonal().normalized()
        side = n.cross(fore)
        # (a burst rakes her: along the fuselage, or slantwise out along a wing)
        turn = math.radians(rng.uniform(-18.0, 18.0) if abs(n.z) < 0.75 else rng.choice((-1, 1)) * rng.uniform(40.0, 72.0))
        along = fore * math.cos(turn) + side * math.sin(turn)
        across = n.cross(along)
        count, gap = rng.choice((5, 6, 6, 7, 8)), 0.27 * big ** 0.5
        steps, far = [], 0.0                                 # (no two gaps alike, and the line wanders and bends: an even row looks stamped on her)
        for j in range(count):
            steps.append(far)
            far += gap * rng.uniform(0.5, 1.6)
        bend = rng.uniform(-0.06, 0.06)
        pieces = []
        for j in range(count):
            t = steps[j] - 0.5 * steps[-1]
            p = c + along * t + across * (gap * rng.uniform(-0.42, 0.42) + bend * t * t / gap)
            hj = cast(p + n * 0.3, -n, 0.75)
            if not good(hj):
                continue
            one = hole(hj[0], hj[1], r_hole * rng.uniform(0.8, 1.2))
            if one:
                pieces.append((one, hj[1]))
        if len(pieces) < 3:
            return False
        lay(pieces, "bullets", c, n, on)
        return True

    def flak(h):
        c, n, on, _ = h
        big_one = tear(c, n, r_tear * rng.uniform(0.8, 1.2))
        if big_one is None:
            return False
        pieces = [(big_one, n)]
        u = n.orthogonal().normalized()
        v = n.cross(u)
        for _ in range(rng.choice((7, 8, 9, 10))):                       # the splinters round it
            a, far = rng.uniform(0.0, 6.283), r_tear * rng.uniform(1.7, 4.6)
            hj = cast(c + (u * math.cos(a) + v * math.sin(a)) * far + n * 0.3, -n, 0.75)
            if not good(hj):
                continue
            one = hole(hj[0], hj[1], r_hole * rng.uniform(0.45, 0.85), sides=5)
            if one:
                pieces.append((one, hj[1]))
        lay(pieces, "flak", c, n, on)
        return True

    # ---- where she is hit ---------------------------------------------------------------------------
    chord = 0.11 * span

    def wing(side, frac):
        root, tip = S["break.wing.port"], S["wingtip.port"]
        p = root.lerp(tip, frac)
        x = S["tank.port"].x + rng.uniform(-0.22, 0.22) * chord
        h = cast((x, side * p.y, p.z + 3.0), -Z)
        return h if good(h) and h[1].z > 0.55 else None

    def flank(side, frac, tilt=None):
        a, b = S["fuel"], S["trail"]
        x, zc = a.x + (b.x - a.x) * frac, a.z + (b.z - a.z) * frac
        th = math.radians(tilt if tilt is not None else rng.uniform(28.0, 66.0))
        way = Vector((0.0, -side * math.cos(th), -math.sin(th)))
        h = cast(Vector((x, 0.0, zc)) - way * 5.0, way)
        return h if good(h) and h[1].dot(-way) > 0.45 else None

    def fin(side, _frac):
        a, b = S["fuel"], S["trail"]
        for up in (0.075, 0.06, 0.045, 0.03):
            x = b.x + (a.x - b.x) * rng.uniform(0.06, 0.13)
            h = cast((x, side * 4.0, b.z + up * span * (2.2 / big)), (0.0, -side, 0.0))
            if good(h) and abs(h[1].y) > 0.8:
                return h
        return flank(side, 0.82)

    plan = [(bullets, wing, 1, 0.45), (bullets, flank, 1, 0.45), (flak, wing, -1, 0.55), (bullets, fin, -1, 0.0), (bullets, wing, -1, 0.3),
            (flak, flank, -1, 0.32), (bullets, wing, 1, 0.75), (flak, flank, 1, 0.7), (bullets, flank, -1, 0.62)]
    if big > 1.3:
        plan += [(flak, wing, 1, 0.62), (bullets, wing, -1, 0.8), (flak, fin, 1, 0.0), (bullets, flank, 1, 0.2), (flak, wing, -1, 0.22)]
    for make, where, side, frac in plan:
        for _ in range(8):                                   # (a place that runs off her, or onto her glass, is tried again a little way off)
            h = where(side, min(max(frac + rng.uniform(-0.05, 0.05), 0.05), 0.95))
            if h is not None and make(h):
                break

    def shows(parts):
        return [{"part": p, "scale": [[0.0, 0.001], [i + 0.999, 0.001], [i + 1.0, 1.0]]} for i, p in enumerate(parts)]

    every = [h["part"] for h in hits]
    for clip, kind in (("damage", None), ("damage.bullets", "bullets"), ("damage.flak", "flak")):
        mine = [h["part"] for h in hits if kind is None or h["kind"] == kind]
        d.clips[clip] = {"long": float(len(mine)), "loop": False, "after": "hold", "tracks": shows(mine), "events": [],
                         "about": ("Her hits, one after another: at second k the first k are there" + (" (all kinds, spread over her)." if kind is None else f" (the {kind} only; lay it over the other kind's).")
                                   + " Hold it at the count of hits she has taken; 0 shows none.")}
    d.info = {"file": {"for": name, "damage": hits, "clear": dict(CLEAR)}}
    d.hits = every
    return d
