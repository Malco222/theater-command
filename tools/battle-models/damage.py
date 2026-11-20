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

    overlay("B17", what="engines")  ->  the model "B17EngineOut"

An engine that is out (the air battles' straggler, 2026-11-18: "a bomber losing an engine slows and falls out of
formation") is a second model of the same kind, for an aircraft with two or more airscrews (MANY_ENGINED). For each
engine N it has `feather.N`, her airscrew with its blades turned edge on to the wind and still (hide her own `prop.N`
and `prop.N.blur` on the aircraft herself), and `scorch.N`, soot laid back over the nacelle and the wing behind it.
At rest all of them show. Clips `feather.N` and `scorch.N` hide or show one part each: held at 0 it is hidden, at 1
shown. Hold each engine's two clips always (at 0 for a sound engine), or scale the parts yourself; no two clips touch
the same part, so they may be laid over each other and over anything. The file's `engines` lists them.
"""
import math
import random

from mathutils import Quaternion, Vector
from mathutils.bvhtree import BVHTree

import planes
from rigid import Model, Part

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
SKIN = {"upper", "upper2", "under", "nose"}                  # her skin, and
LAID = {"line", "rivet", "rivet_under", "stain", "soot", "patch"}     # what lies on it: a hole may lie over these too


PAINTS = {"hole": "0A0A0B", "torn": "7E8287", "lip": "7E8287", "bent": "2B2D2F", "soot": "2A2622", "soot_thin": "2A2622"}
CLEAR = {"lip": 0.4, "soot": 0.35, "soot_thin": 0.17}       # how much of each see-through paint shows over her own
MANY_ENGINED = ("Amerikabomber", "B17", "B29", "Betty", "Bomber", "He111", "Lancaster", "Nell")    # two airscrews or more: each has an <Name>EngineOut


def look(paint, mat, nt, bsdf, mix, **kw):
    """For the pictures: the hole is dead black, the torn metal bare, the lip and the soot seen through."""
    if paint in ("hole", "soot", "soot_thin"):
        bsdf.inputs["Roughness"].default_value = 1.0
    else:                                                    # (bright from every side: polished metal would mirror a dark sea)
        bsdf.inputs["Metallic"].default_value = 0.15
        bsdf.inputs["Roughness"].default_value = 0.5 if paint != "bent" else 0.8
    if paint in CLEAR:
        bsdf.inputs["Alpha"].default_value = CLEAR[paint]


def overlay(name, fine=True, model=None, what="holes"):
    """The damage model for the aircraft `name` (`model`: hers, if it is already built): her bullet and flak holes
    ("holes": <name>Damage), or her engines out ("engines": <name>EngineOut; None if she has only one airscrew)."""
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

    airscrews = sorted((int(p.name.split(".")[1]), i) for i, p in enumerate(m.parts) if p.name.startswith("prop.") and p.name.count(".") == 1)
    if what == "engines":
        if len(airscrews) < 2:
            return None
        d = Model(name + "EngineOut", m.title + ": an engine out", "damage",
                  {"blade": m.paints.get("blade", "1B1C1E"), "soot": PAINTS["soot"], "soot_thin": PAINTS["soot_thin"]}, fine)
    else:
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
    for make, where, side, frac in (plan if what == "holes" else []):
        for _ in range(8):                                   # (a place that runs off her, or onto her glass, is tried again a little way off)
            h = where(side, min(max(frac + rng.uniform(-0.05, 0.05), 0.05), 0.95))
            if h is not None and make(h):
                break

    # ---- an engine that is out: her airscrew feathered and still, her nacelle scorched ----------------
    rng2 = random.Random(977 * len(name) + sum(ord(c) for c in name))      # (its own dice: the hits above stay as they were)
    engines = {}

    def streak(path, widths, lift, ok=lambda h: True):
        """A streak of soot along `path` ([(point on her skin, its facing)]), `widths` wide at each point: its two edges
        found on her skin again, so that it lies on her as she curves. Returns (points, faces) of one quad strip."""
        left, right = [], []
        for i, ((p, n), w) in enumerate(zip(path, widths)):
            a, b = path[max(i - 1, 0)][0], path[min(i + 1, len(path) - 1)][0]
            t = b - a
            t = t.normalized() if t.length > 1e-6 else X.copy()
            side = n.cross(t)
            side = side.normalized() if side.length > 1e-6 else Y.copy()
            for sgn, edge in ((1.0, left), (-1.0, right)):
                q = p + side * (0.5 * w * sgn)
                h = cast(q + n * 0.35, -n, 0.7)
                edge.append(h[0] + h[1] * lift if (good(h) and ok(h)) else q + n * lift)
        m_ = len(path)
        return left + right, [(i, i + 1, m_ + i + 1, m_ + i) for i in range(m_ - 1)]

    def taper(nn, wide, lo=0.25, head=0.18):
        """Widths along a streak of nn points: quickly up to `wide`, then thinning to its end, never quite to nothing."""
        out = []
        for i in range(nn):
            t = i / max(nn - 1, 1)
            out.append(wide * max(lo, min(1.0, t / head) * (1.0 - 0.75 * t ** 1.6)))
        return out

    def tail_soot(k, e):
        """An inboard engine's smoke streams straight back over her tailplane on its side (Brian, 2026-11-20: "a lot
        more dark smoke strains from that engine and directly behind it on the tail"; the art director's G1): soot on
        the tailplane's leading edge and top there, thickest at the edge, and a lighter veil low on her fin's side.
        Laid a little proud of her group's marks and letters, so that it darkens them where it reaches them."""
        side = 1.0 if e.y > 0 else -1.0
        lift = 0.017
        edge = {}
        x_le = None
        for xi in range(0, 120):                         # (her tailplane's leading edge on the engine's line)
            x = -6.5 - 0.05 * xi
            h = cast((x, e.y, e.z + 4.0), -Z)
            if good(h) and h[2] in ("tail",) and h[1].z > 0.5 and h[0].z < e.z + 1.6:
                x_le = x
                break
        if x_le is None:
            return
        z_tp = cast((x_le - 0.3, e.y, e.z + 4.0), -Z)
        z_tp = z_tp[0].z if good(z_tp) else e.z
        half_w = 4
        for i, dz in enumerate((-0.16, -0.1, -0.05, 0.0)):  # (round the leading edge and its de-icer boot, seen from ahead)
            for j in range(-half_w, half_w + 1):
                y = e.y + 0.62 * (j / half_w)
                h = cast((x_le + 1.5, y, z_tp + dz), -X, 3.0)
                if h is not None and (good(h) or h[3] == "boot") and h[2] == "tail":
                    edge[i, j] = h[0] + h[1] * lift
        part = d.part(f"tailsoot.{k}", "tail", pivot=tuple(Vector((x_le, e.y, z_tp))), kind="fixed",
                      note=f"engine {k}'s smoke has streamed over her tail: soot on the tailplane's leading edge and top on its side, a veil low on her fin")
        d.use(part)
        made = 0
        for s_ in range(8):                              # (over the top, streaks back from the edge along the air)
            off = (rng2.uniform(-1.0, 1.0) ** 3) * 0.95
            core = abs(off) < 0.3
            path = []
            for i in range(18):
                h = cast((x_le + 0.25 - 0.13 * i, e.y + off, z_tp + 2.5), -Z)
                if not (h is not None and (good(h) or h[3] == "boot") and h[2] == "tail" and h[1].z > 0.4):
                    if path:
                        break
                    continue
                path.append((h[0], h[1]))
            if len(path) < 3:
                continue
            pts, fs = streak(path, taper(len(path), rng2.uniform(0.18, 0.32) * (1.3 if core else 1.0), lo=0.3, head=0.05), lift + 0.0004 * s_, ok=lambda h: h[2] == "tail")
            d.add(pts, fs, "soot" if core else "soot_thin", smooth=False, out=lambda mid: Z)
            made += 1
        keys = list(edge)
        at = {key: n for n, key in enumerate(keys)}
        faces = []
        for (i, j) in keys:
            q4 = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
            if all(c in edge for c in q4) and max((edge[a_] - edge[b_]).length for a_ in q4 for b_ in q4) < 0.9:
                a_, b_, c_, d_ = (at[c] for c in q4)
                faces += [(a_, b_, c_), (a_, c_, d_)]
        if faces:
            d.add([edge[key] for key in keys], faces, "soot", smooth=False, out=lambda mid: X)
        x_fin = None
        for xi in range(0, 160):                         # (her fin: low on its side toward the engine)
            x = -6.0 - 0.05 * xi
            h = cast((x, side * 3.0, z_tp + 0.9), Vector((0.0, -side, 0.0)), 6.0)
            if good(h) and h[2] == "tail" and abs(h[0].y) < 0.35 and abs(h[1].y) > 0.6:
                x_fin = x
                break
        if x_fin is not None:
            for s_ in range(4):                          # (a lighter veil, in a few long streaks low on her fin's side)
                z = z_tp + 0.5 + 0.2 * s_ + rng2.uniform(-0.05, 0.05)
                path = []
                for i in range(22):
                    h = cast((x_fin - 0.1 - 0.15 * i, side * 3.0, z), Vector((0.0, -side, 0.0)), 6.0)
                    if not (good(h) and h[2] in ("tail", "rudder") and abs(h[0].y) < 0.35 and abs(h[1].y) > 0.6):
                        if path:
                            break
                        continue
                    path.append((h[0], h[1]))
                if len(path) < 4:
                    continue
                pts, fs = streak(path, taper(len(path), rng2.uniform(0.2, 0.3), lo=0.3, head=0.1), lift, ok=lambda h: abs(h[1].y) > 0.5)
                d.add(pts, fs, "soot_thin", smooth=False, out=lambda mid: Vector((0.0, side, 0.0)))
        if not made:
            return
        engines.setdefault(k, {"engine": k})["tailsoot"] = part
    if what == "engines":
        for k, pi in airscrews:
            hub = Vector(m.parts[pi].pivot)
            mine = [f for f, fp in zip(m.faces, m.fpart) if fp == pi]
            root = {}                                        # (each blade is a piece of its own: find them)

            def find(v):
                while root.setdefault(v, v) != v:
                    root[v] = root[root[v]]
                    v = root[v]
                return v

            for f in mine:
                for v in f[1:]:
                    root[find(v)] = find(f[0])
            blades = {}
            for f in mine:
                blades.setdefault(find(f[0]), []).append(f)
            part = d.part(f"feather.{k}", m.parts[m.parts[pi].parent].name, pivot=tuple(hub), kind="fixed",
                          note=f"engine {k}'s airscrew feathered: her blades edge on to the wind, and still")
            d.use(part)
            for faces in blades.values():
                ids = sorted({v for f in faces for v in f})
                pts = [Vector(m.verts[v]) - hub for v in ids]
                tip = max(pts, key=lambda p: math.hypot(p.y, p.z))
                along = Vector((0.0, tip.y, tip.z)).normalized()
                across = X.cross(along)

                def width(th):                               # (how broad the blade is seen from dead ahead)
                    q = Quaternion(along, th)
                    w = [(q @ p).dot(across) for p in pts]
                    return max(w) - min(w)

                q = Quaternion(along, min((math.radians(a) for a in range(0, 180, 2)), key=width))
                at = {v: i for i, v in enumerate(ids)}
                d.add([hub + q @ p for p in pts], [tuple(at[v] for v in f) for f in faces], "blade", smooth=True)
            engines[k] = {"engine": k, "feather": part, "on": m.parts[m.parts[pi].parent].name}
        chord_e = 0.11 * span
        wing_sockets = [sk for sk in m.sockets if sk["name"].startswith("engine.") and abs(sk["at"].y) >= 0.6]
        inboard = sorted(wing_sockets, key=lambda sk: abs(sk["at"].y))[:2] if len(wing_sockets) >= 4 else []
        for sk in wing_sockets:
            k, e = int(sk["name"].split(".")[1]), sk["at"]
            # (back over the wing to its trailing edge, as the art director's G1 has it: find her trailing edge behind it)
            x_te = e.x - 0.95 * chord_e
            x = e.x
            while x > e.x - 3.0 * chord_e:
                h = cast((x, e.y, e.z + 3.0), -Z)
                if good(h) and h[1].z > 0.35 and h[2].startswith(("wing", "aileron", "flap")):
                    x_te = x
                elif x < e.x - 0.6 * chord_e:
                    break
                x -= 0.05
            long_e, w0, w1 = max(0.95 * chord_e, e.x + 0.15 - x_te - 0.04), 0.085 * chord_e, 0.21 * chord_e
            nacelle = min(long_e, 0.9 * 0.95 * chord_e * 0.42)    # (the blackest part, over her nacelle's top)
            nx, half = 8, 3
            grid, on = {}, None
            for i in range(nx):
                t = i / (nx - 1.0)
                for j in range(-half, half + 1):
                    reach = (w0 + (w1 - w0) * 0.35 * t) * (j / half) * (rng2.uniform(0.7, 1.15) if abs(j) == half else 1.0)
                    x = e.x + 0.15 - nacelle * t * (rng2.uniform(0.9, 1.08) if i == nx - 1 else 1.0)
                    h = cast((x, e.y + reach, e.z + 3.0), -Z)
                    if good(h) and h[1].z > 0.35:
                        grid[i, j] = h[0] + h[1] * 0.009
                        on = on or h[2]
            if len(grid) < 12:
                continue
            part = d.part(f"scorch.{k}", on, pivot=tuple(e), kind="fixed", note=f"engine {k} has burned: soot laid back over her nacelle and the wing behind it")
            d.use(part)
            keys = list(grid)
            at = {key: n for n, key in enumerate(keys)}
            faces = {"soot": [], "soot_thin": []}
            for i in range(nx - 1):
                t = i / (nx - 1.0)
                for j in range(-half, half):
                    q4 = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                    if not all(c in grid for c in q4):
                        continue
                    zs = [grid[c].z for c in q4]
                    if max(zs) - min(zs) > 0.3 or (j in (-half, half - 1) and rng2.random() < (0.25 if t < 0.65 else 0.6)):
                        continue                             # (not across the step of a nacelle's side; and ragged edges, raggedest at its end)
                    a, b, c, e4 = (at[c] for c in q4)
                    thick = j in (-2, -1, 0, 1)              # (blackest on the nacelle)
                    faces["soot" if thick else "soot_thin"] += [(a, b, c), (a, c, e4)]
            for paint in ("soot_thin", "soot"):
                if faces[paint]:
                    d.add([grid[key] for key in keys], faces[paint], paint, smooth=False, out=lambda mid: Z)
            # (then back over the wing to her trailing edge in streaks along the air, never a blotch: the art director's G1)
            for s_ in range(9):
                off = (rng2.uniform(-1.0, 1.0) ** 3) * w1 * 0.95
                core = abs(off) < 0.35 * w1
                x0 = e.x + 0.15 - nacelle * rng2.uniform(0.45, 0.9)
                reach = (long_e - (e.x + 0.15 - x0)) * (rng2.uniform(0.85, 1.0) if core else rng2.uniform(0.45, 0.9))
                path = []
                for i in range(int(reach / 0.18) + 2):
                    x = x0 - min(i * 0.18, reach)
                    h = cast((x, e.y + off + 0.05 * math.sin(i * 0.7 + s_), e.z + 3.0), -Z)
                    if not (good(h) and h[1].z > 0.35 and h[2].startswith(("wing", "aileron", "flap", "engine", "nacelle", "plane"))):
                        break
                    path.append((h[0], h[1]))
                if len(path) < 4:
                    continue
                pts, fs = streak(path, taper(len(path), rng2.uniform(0.16, 0.3) * (1.4 if core else 1.0)), 0.0095 + 0.0004 * s_, ok=lambda h: h[1].z > 0.3)
                d.add(pts, fs, "soot" if core else "soot_thin", smooth=False, out=lambda mid: Z)
            engines.setdefault(k, {"engine": k, "on": on})["scorch"] = part
            if sk in inboard and "tail" in m.part_at:
                tail_soot(k, e)
    extras = [v[key] for v in engines.values() for key in ("feather", "scorch", "tailsoot") if key in v]

    def shows(parts):
        return [{"part": p, "scale": [[0.0, 0.001], [i + 0.999, 0.001], [i + 1.0, 1.0]]} for i, p in enumerate(parts)]

    if what == "engines":
        for p in extras:
            n = p.split(".")[1]
            words = (f"her airscrew feathered and still (hide `prop.{n}` and `prop.{n}.blur` on the aircraft herself)" if p.startswith("feather")
                     else "her nacelle and the wing behind it sooted back to the trailing edge" if p.startswith("scorch")
                     else "her tailplane on that side sooted along its leading edge and top, and a veil low on her fin (an inboard engine's smoke streams over them)")
            d.clips[p] = {"long": 1.0, "loop": False, "after": "hold", "tracks": [{"part": p, "scale": [[0.0, 0.001], [0.999, 0.001], [1.0, 1.0]]}], "events": [],
                          "about": f"Engine {n}: {words}. Held at 0 it is hidden, at 1 shown; hold it always, at 0 while that engine is sound. It touches no other part."}
        d.info = {"file": {"for": name, "engines": [engines[k] for k in sorted(engines)], "clear": {k: CLEAR[k] for k in ("soot", "soot_thin")}}}
        return d

    every = [h["part"] for h in hits]
    for clip, kind in (("damage", None), ("damage.bullets", "bullets"), ("damage.flak", "flak")):
        mine = [h["part"] for h in hits if kind is None or h["kind"] == kind]
        d.clips[clip] = {"long": float(len(mine)), "loop": False, "after": "hold", "tracks": shows(mine), "events": [],
                         "about": ("Her hits, one after another: at second k the first k are there" + (" (all kinds, spread over her)." if kind is None else f" (the {kind} only; lay it over the other kind's).")
                                   + " Hold it at the count of hits she has taken; 0 shows none.")}
    d.info = {"file": {"for": name, "damage": hits, "clear": dict(CLEAR)}}
    d.hits = every
    return d
