"""National markings on the aircraft, and the record a machine wears, for the pictures only.

The game paints its own markings where the model's `marks` say (a tag, a place, a size, a facing). So that a review
picture does not show bare machines, the same list is drawn here as thin painted shapes laid on the skin: each nation's
plain period marking, never a party emblem. Nothing here goes into the game's files.

A machine's record (Brian, 2026-11-18: an ace's kills by his cockpit, a bomber's missions as small yellow bombs) is
drawn the same way from the slots in the model's `tally`: see `record()`. The marks are our own plain shapes; nothing
is copied from any real machine's nose.
"""
import math

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import tessellate_polygon

BLUE, RED, WHITE, YELLOW, BLACK = "1C2C55", "A82A26", "E6E4DA", "D9B03A", "141414"
US_BLUE, DISC = "1B2A4A", "B0261E"
GREEN, BOMB_YELLOW, BAR_WHITE = "2F6B3A", "D2A82E", "DAD8CE"


def poly(points, col):
    return [(col, list(points))]


def bomb(col):
    """A small bomb standing on its nose, fins up."""
    half = [(0.0, -1.0), (0.14, -0.86), (0.26, -0.62), (0.32, -0.32), (0.32, -0.02), (0.26, 0.24), (0.15, 0.46), (0.1, 0.56)]
    body = half + [(-x, y) for x, y in reversed(half[1:])]
    fins = [(0.1, 0.5), (0.36, 0.74), (0.36, 1.0), (0.1, 0.84), (-0.1, 0.84), (-0.36, 1.0), (-0.36, 0.74), (-0.1, 0.5)]
    return poly(body, col) + poly(fins, col)


def mix(a, b, t):
    return "".join("%02X" % round(int(a[i:i + 2], 16) * (1.0 - t) + int(b[i:i + 2], 16) * t) for i in (0, 2, 4))


def ring(r0, r1, col, seg=32):
    out = []
    steps = max(1, int(math.ceil((r1 - r0) / 0.3)))
    for j in range(steps):
        a, b = r0 + (r1 - r0) * j / steps, r0 + (r1 - r0) * (j + 1) / steps
        for k in range(seg):
            t0, t1 = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
            quad = [(a * math.cos(t0), a * math.sin(t0)), (b * math.cos(t0), b * math.sin(t0)), (b * math.cos(t1), b * math.sin(t1)), (a * math.cos(t1), a * math.sin(t1))]
            out.append((col, quad if a > 1e-6 else quad[1:]))
    return out


def rect(u0, u1, v0, v1, col, cell=0.3):
    out = []
    nu, nv = max(1, int(math.ceil((u1 - u0) / cell))), max(1, int(math.ceil((v1 - v0) / cell)))
    for i in range(nu):
        for j in range(nv):
            a, b = u0 + (u1 - u0) * i / nu, u0 + (u1 - u0) * (i + 1) / nu
            c, d = v0 + (v1 - v0) * j / nv, v0 + (v1 - v0) * (j + 1) / nv
            out.append((col, [(a, c), (b, c), (b, d), (a, d)]))
    return out


def star(r, col, inner=0.382, points=5):
    pts = []
    for k in range(2 * points):
        a = math.pi / 2 + math.pi * k / points
        rr = r if k % 2 == 0 else r * inner
        pts.append((rr * math.cos(a), rr * math.sin(a)))
    out = []
    for k in range(2 * points):
        a, b = pts[k], pts[(k + 1) % (2 * points)]
        for j in range(3):
            t0, t1 = j / 3.0, (j + 1) / 3.0
            quad = [(a[0] * t0, a[1] * t0), (a[0] * t1, a[1] * t1), (b[0] * t1, b[1] * t1), (b[0] * t0, b[1] * t0)]
            out.append((col, quad if j else quad[1:3] + [(0.0, 0.0)]))
    return out


def offset(points, e, arc=6):
    """A shape grown by `e` all round (a painted edge about it): straight along its sides, round at its points, and
    mitred in its notches. `points` go round once, either way."""
    pts = [Vector((x, y)) for x, y in points]
    area = sum(a.x * b.y - b.x * a.y for a, b in zip(pts, pts[1:] + pts[:1]))
    if area < 0:
        pts.reverse()
    n = len(pts)
    out = []
    for i in range(n):
        a, b, c = pts[i - 1], pts[i], pts[(i + 1) % n]
        d0, d1 = (b - a).normalized(), (c - b).normalized()
        n0, n1 = Vector((d0.y, -d0.x)), Vector((d1.y, -d1.x))                    # (outward, for a shape that goes round anticlockwise)
        turn = d0.x * d1.y - d0.y * d1.x
        if turn > 1e-6:                                                          # (a point: round it)
            t0, t1 = math.atan2(n0.y, n0.x), math.atan2(n1.y, n1.x)
            while t1 < t0:
                t1 += 2.0 * math.pi
            for k in range(arc + 1):
                t = t0 + (t1 - t0) * k / arc
                out.append((b.x + e * math.cos(t), b.y + e * math.sin(t)))
        else:                                                                    # (a notch: where the two grown sides meet)
            m = (n0 + n1)
            m = m / max(m.length, 1e-6)
            cos = max(0.2, m.dot(n0))
            out.append(tuple(b + m * (e / cos)))
    return out


def grow_radial(points, e, steps=240):
    """A shape that is star-shaped about its middle (a flower) grown by `e` all round: for each way out from the
    middle, as far as the grown shape reaches (round at its points, smooth in its notches, never spiked)."""
    pts = [Vector(p) for p in points]
    out = []
    for k in range(steps):
        t = 2.0 * math.pi * k / steps
        d = Vector((math.cos(t), math.sin(t)))
        reach = 0.0
        for q in pts:
            along = d.dot(q)
            off2 = q.length_squared - along * along
            if off2 <= e * e:
                reach = max(reach, along + math.sqrt(e * e - off2))
        out.append((d.x * reach, d.y * reach))
    return out


def bomb_outline():
    half = [(0.0, -1.0), (0.14, -0.86), (0.26, -0.62), (0.32, -0.32), (0.32, -0.02), (0.26, 0.24), (0.15, 0.46), (0.1, 0.56)]
    body = half + [(-x, y) for x, y in reversed(half[1:])]
    fins = [(0.1, 0.5), (0.36, 0.74), (0.36, 1.0), (0.1, 0.84), (-0.1, 0.84), (-0.36, 1.0), (-0.36, 0.74), (-0.1, 0.5)]
    return body, fins


def blossom():
    """A plain five-petal flower, a petal up: each petal round with a small notch at its tip (Japan's own mark for a kill)."""
    out = []
    for k in range(200):
        t = 2.0 * math.pi * k / 200
        petal = abs(math.cos(2.5 * (t - math.pi / 2)))                              # (1 at a petal's middle, 0 between petals)
        r = 0.42 + 0.58 * petal ** 0.55
        tip = (2.5 * (t - math.pi / 2) / math.pi) % 1.0                           # (0 at a petal's middle)
        tip = min(tip, 1.0 - tip)
        r -= 0.16 * math.exp(-(tip / 0.035) ** 2)                                # (the notch)
        out.append((r * math.cos(t), r * math.sin(t)))
    return out


_GROWN = {}


def _grown_blossom():
    if "b" not in _GROWN:
        _GROWN["b"] = grow_radial(blossom(), EDGE)
    return _GROWN["b"]


def plane_top():
    """A plain aircraft seen from above, nose up: the victim of any other nation."""
    half = [(0.0, 1.0), (0.085, 0.9), (0.1, 0.42), (0.98, 0.24), (1.0, 0.08), (0.1, 0.02), (0.075, -0.58), (0.42, -0.68),
            (0.42, -0.84), (0.06, -0.86), (0.0, -0.96)]
    return half + [(-x, y) for x, y in reversed(half[1:-1])]


EDGE_DARK = "1E1E1B"
EDGE = 0.111                                                                     # (a thin edge, about half a centimetre on a 9 cm symbol)


def kill_mark(tag, finish):
    """A kill symbol on a bomber's panel (the art director's ruling of 2026-11-20, section 4), for a dark or a light
    finish under it: on dark paint a dark symbol carries a thin white edge; on light paint a white one a thin dark edge."""
    dark = finish != "light"
    if tag == "KILL-GER":                                                        # (our plain black cross with its own white edges)
        return shapes("GER")
    if tag == "KILL-JAP":                                                        # (a plain red disc; a thin white surround on dark paint)
        return ([ring(0.0, 1.0 + EDGE, WHITE, 24)] if dark else []) + [ring(0.0, 1.0, DISC, 24)]
    if tag == "KILL-ITA":                                                        # (a plain upright green, white and red flag: no shield, no fasces)
        e = EDGE * 0.9
        return [rect(-1.0 - e, 1.0 + e, -0.66 - e, 0.66 + e, WHITE if dark else EDGE_DARK, 1.0),
                rect(-1.0, -0.34, -0.66, 0.66, GREEN, 1.0) + rect(-0.34, 0.34, -0.66, 0.66, WHITE, 1.0) + rect(0.34, 1.0, -0.66, 0.66, RED, 1.0)]
    if tag == "KILL-UK":                                                         # (a roundel with its outer ring, so it reads on dark paint)
        return ([] if dark else [ring(0.0, 1.0 + EDGE, EDGE_DARK, 24)]) + [ring(0.0, 1.0, YELLOW, 24), ring(0.0, 0.84, BLUE, 24), ring(0.0, 0.55, WHITE, 24), ring(0.0, 0.27, RED, 20)]
    if tag == "KILL-SOV":                                                        # (a plain red star: the Soviet Union's victim, and its own mark for any kill)
        st = [(math.cos(math.pi / 2 + math.pi * k / 5) * (1.0 if k % 2 == 0 else 0.382), math.sin(math.pi / 2 + math.pi * k / 5) * (1.0 if k % 2 == 0 else 0.382)) for k in range(10)]
        return ([poly(offset(st, EDGE), WHITE)] if dark else []) + [star(1.0, DISC)]
    if tag == "KILL-USA":                                                        # (a white star on a blue disc, without bars)
        return [ring(0.0, 1.0 + EDGE, WHITE if dark else EDGE_DARK, 24), ring(0.0, 1.0, US_BLUE, 24), star(0.98, WHITE)]
    if tag == "KILL-FRA":
        return ([ring(0.0, 1.0 + EDGE, WHITE, 24)] if dark else []) + [ring(0.0, 1.0, RED, 24), ring(0.0, 0.66, WHITE, 24), ring(0.0, 0.33, BLUE, 20)]
    if tag == "KILL-BLOSSOM":                                                    # (Japan's own mark for every kill: white, pale pink reads as white under the grade)
        b = blossom()
        return ([] if dark else [poly(_grown_blossom(), EDGE_DARK)]) + [poly(b, WHITE)]
    if tag == "KILL-PLANE":                                                      # (anyone else: white on a dark finish, black on a light one)
        return [poly(plane_top(), WHITE if dark else EDGE_DARK)]
    return shapes(tag)


def bomb_mark(finish):
    """A mission's bomb on a bomber's panel: plain yellow on a dark finish; edged in black on a light one."""
    body, fins = bomb_outline()
    if finish == "light":
        return [poly(offset(body, 0.083, 4), BLACK) + poly(offset(fins, 0.083, 4), BLACK), bomb(BOMB_YELLOW)]
    return [bomb(BOMB_YELLOW)]


def layers(insignia):
    """A country's own marking as the art director's unit table gives it (`insignia`: layers lowest first in a square
    from -1 to 1, each a ring [r0, r1], a star [r, inner, points], a rect [u0, u1, v0, v1] or a tri [r, turn], with
    its colour c), as the layers `shapes` gives for a great power's. Neighbours of one colour lie in one layer: each
    layer stands a few millimetres prouder than the one under it, and a device drawn as two dozen pieces (Sweden's
    three crowns) stood a hand's breadth off her skin by its last piece, its parts sliding over each other from
    the side."""
    out, last = [], None
    for one in insignia:
        c = one["c"]
        if "ring" in one:
            shape = ring(one["ring"][0], one["ring"][1], c)
        elif "star" in one:
            r, inner, points = one["star"]
            shape = star(r, c, inner, int(points))
        elif "rect" in one:
            shape = rect(*one["rect"], c)
        elif "tri" in one:
            r, turn = one["tri"]
            shape = poly([(r * math.sin(math.radians(turn + 120.0 * k)), r * math.cos(math.radians(turn + 120.0 * k))) for k in range(3)], c)
        else:
            continue
        if out and c == last:
            out[-1] = out[-1] + shape
        else:
            out.append(shape)
        last = c
    return out


def flag(mk, fin):
    """A flag's stripes on a rudder (a mark of place `fin` with a `quad`: the four corners of the flag on her rudder's
    side, in x and z, leading-bottom, trailing-bottom, trailing-top, leading-top), cut into the bands the unit table
    gives the country (`fin`: dir h, bands from top to bottom; dir v, bands from the leading edge back)."""
    lb, tb, tt, lt = (Vector((q[0], q[1])) for q in mk["quad"])
    r, at, n = mk["radius"], mk["at"], len(fin["bands"])
    out = []
    for i, c in enumerate(fin["bands"]):
        box = [(0.0, 1.0 - (i + 1) / n), (1.0, 1.0 - (i + 1) / n), (1.0, 1.0 - i / n), (0.0, 1.0 - i / n)] if fin["dir"] == "h" else [(i / n, 0.0), ((i + 1) / n, 0.0), ((i + 1) / n, 1.0), (i / n, 1.0)]
        pts = [lb.lerp(tb, s_).lerp(lt.lerp(tt, s_), t_) for s_, t_ in box]
        out += poly([((p.x - at[0]) / r, (p.y - at[2]) / r) for p in pts], c)
    return [out]


def shapes(tag):
    """The layers of a marking, lowest first, in a square from -1 to 1 (v is up on a side, forward on a wing)."""
    if tag == "GER":
        return [rect(-1, 1, -0.24, 0.24, WHITE) + rect(-0.24, 0.24, -1, 1, WHITE), rect(-1, 1, -0.15, 0.15, BLACK) + rect(-0.15, 0.15, -1, 1, BLACK)]
    if tag == "UK-A1":
        return [ring(0.0, 1.0, YELLOW), ring(0.0, 0.84, BLUE), ring(0.0, 0.55, WHITE), ring(0.0, 0.27, RED)]
    if tag == "UK":
        return [ring(0.0, 1.0, BLUE), ring(0.0, 0.62, WHITE), ring(0.0, 0.3, RED)]
    if tag == "UK-B":
        return [ring(0.0, 1.0, BLUE), ring(0.0, 0.42, RED)]
    if tag == "UK-FIN":
        return [rect(0.25, 0.75, -0.9, 0.9, RED) + rect(-0.25, 0.25, -0.9, 0.9, WHITE) + rect(-0.75, -0.25, -0.9, 0.9, BLUE)]
    if tag == "JAP-PLAIN":
        return [ring(0.0, 1.0, DISC)]
    if tag == "JAP-RING":                                                        # (late 1942 to the end of 1943, on the fuselage: a thin white ring round the disc, a tenth of its width)
        return [ring(0.0, 1.2, WHITE), ring(0.0, 1.0, DISC)]
    if tag == "USA":
        return [ring(0.0, 1.0, US_BLUE), star(0.98, WHITE)]
    if tag == "USA-RED":                                                         # (until May 1942: a red disc in the middle of the star)
        return [ring(0.0, 1.0, US_BLUE), star(0.98, WHITE), ring(0.0, 0.29, RED, 20)]
    if tag == "USA-BAR":
        return [ring(0.0, 1.0, US_BLUE) + rect(-2.05, -0.9, -0.33, 0.33, US_BLUE) + rect(0.9, 2.05, -0.33, 0.33, US_BLUE),
                rect(-1.95, -0.95, -0.24, 0.24, WHITE) + rect(0.95, 1.95, -0.24, 0.24, WHITE), star(0.98, WHITE)]
    if tag == "USA-BAR-RED":                                                     # (July and August 1943, by the historian: the same, edged in red)
        return [ring(0.0, 1.09, RED) + rect(-2.05, -0.9, -0.33, 0.33, RED) + rect(0.9, 2.05, -0.33, 0.33, RED),
                ring(0.0, 1.0, US_BLUE), rect(-1.95, -0.95, -0.24, 0.24, WHITE) + rect(0.95, 1.95, -0.24, 0.24, WHITE), star(0.98, WHITE)]
    if tag == "SOV":
        return [star(1.0, DISC)]
    # ---- a machine's record: a victim's mark is that nation's own plain marking in small ----
    if tag == "KILL-GER":
        return shapes("GER")
    if tag == "KILL-UK":
        return shapes("UK")
    if tag == "KILL-USA":
        return shapes("USA")
    if tag == "KILL-JAP":
        return [ring(0.0, 1.0, WHITE, 20), ring(0.0, 0.84, DISC, 20)]          # (a thin white edge, or a red disc is lost on dark paint)
    if tag == "KILL-SOV":
        return [star(1.0, WHITE), star(0.78, DISC)]
    if tag == "KILL-FRA":
        return [ring(0.0, 1.0, RED, 20), ring(0.0, 0.66, WHITE, 20), ring(0.0, 0.33, BLUE, 20)]
    if tag == "KILL-CHI":
        return [ring(0.0, 1.0, "0A1C8C", 20), star(0.82, WHITE, 0.6, 12)]       # (China's white sun on blue, as the game draws it on her own aircraft)
    if tag == "KILL-ITA":
        return [rect(-1.0, -0.34, -0.66, 0.66, GREEN, 1.0) + rect(-0.34, 0.34, -0.66, 0.66, WHITE, 1.0) + rect(0.34, 1.0, -0.66, 0.66, RED, 1.0)]
    if tag == "KILL-STAR":                                                       # (the Soviet habit: a small red star a kill)
        return [star(1.0, WHITE), star(0.8, DISC)]
    if tag == "KILL-BAR" or tag.startswith("KILL-"):                             # (Germany's habit on the rudder; and any nation not drawn here)
        return [rect(-0.3, 0.3, -1.0, 1.0, BAR_WHITE, 1.0)]
    if tag == "BOMB":
        return [bomb(BOMB_YELLOW)]
    if tag == "LEAD":
        return [star(1.0, BOMB_YELLOW)]
    return []


def habit(nation, role):
    """How a nation's machines of this kind wear their record (the art director's ruling of 2026-11-18)."""
    if role == "fighter":
        return {"GER": "bars", "SOV": "stars"}.get(nation, "victims")
    return "bombs" if nation in ("UK", "USA") else "none"


def record(model, rec):
    """The marks of a machine's record, as entries like the model's own marks, from the slots in her `tally`.

    rec: {"kills": [["GER", 14], ["ITA", 3]], "missions": 32, "lead": [12, 19], "nation": "UK", "habit": ...}
    (nation is only needed for a generic machine; habit overrides the nation's own.)"""
    t = getattr(model, "tally", None) or {}
    if not rec or not t:
        return []
    if "panel" in t:                                                             # (a bomber: the art director's panel of 2026-11-20, tallies.py)
        import tallies
        # (the light test reads the paint she wears under the middle of the panel: her owner's colours on a plain
        #  machine, a mottle where the panel lies on one; the art director, 19:48Z)
        skin = rec.get("skin") or tallies.worn_at(t["panel"], getattr(model, "paints", None) or {})
        finish = rec.get("finish") or tallies.finish_of(skin)
        seat = int(rec.get("seat", 0))
        out = []
        for one in tallies.fill(t, rec, seat):
            row = t[one["kind"]]
            slot = row["slots"][one["slot"]]
            hands = t["hand"][one["kind"]]
            lean, size, du, dv = hands[(one["slot"] + 37 * seat) % len(hands)]
            out.append({"tag": one["tag"], "at": slot[:3], "facing": slot[3:], "radius": row["radius"] * size, "small": True,
                        "turn": lean, "nudge": [du, dv], "fade": one["fade"], "skin": skin, "tin": one.get("tin"), "finish": finish,
                        "up": [0.0, 0.0, 1.0]})                                  # (upright on her side, even where it faces the sky a good deal)
        return out
    nation = rec.get("nation") or t.get("nation", "*")
    how = rec.get("habit") or habit(nation, model.info["file"].get("role", "fighter"))
    kills = [n for nat, count in rec.get("kills", []) for n in [nat] * int(count)]
    out = []

    def fill(row, tags):
        for tag, slot in zip(tags, row["slots"]):
            if tag:
                out.append({"tag": tag, "at": slot[:3], "facing": slot[3:], "radius": row["radius"], "part": row.get("part"), "small": True})

    if how == "bars" and "rudder" in t:
        fill(t["rudder"], ["KILL-BAR"] * len(kills))
    elif how == "stars" and "kills" in t:
        fill(t["kills"], ["KILL-STAR"] * len(kills))
    elif how in ("victims", "bombs") and "kills" in t:
        fill(t["kills"], ["KILL-" + n for n in kills])
    if how == "bombs" and "missions" in t:
        flown, led = int(rec.get("missions", 0)), set(rec.get("lead", []))
        fill(t["missions"], ["BOMB"] * flown)
        fill(t["lead"], ["LEAD" if (k + 1) in led and k < flown else None for k in range(len(t["lead"]["slots"]))])
    if rec.get("name") and "name" in t:                # (an ace's name over his kills: acepanel.py)
        import acepanel
        out += acepanel.marks(t["name"], rec["name"], rec.get("ink") or acepanel.ink_under(model, t["name"], rec.get("nation") or t.get("nation")))
    return out


def put(model, ob, arm, rec=None, own=None, fin=None):
    """Lays the model's markings on its skin (as it stands at rest), each riding the part it is on, and her record if
    one is given (see `record`). Returns the objects made. `own` is what a plain machine wears where her file says
    the owner's own marking (`*`): a tag, the unit table's `insignia` layers, or a function of the mark's facing that
    gives one (for the pictures). `fin` is the unit table's `fin` for her owner: the flag on her rudder (`*FIN`)."""
    todo = list(model.marks or []) + (record(model, rec) if "wing.port" in model.part_at else [])
    if not todo:
        return []
    # (only her painted skin: a mark whose corner landed on a scribed line or a rivet stood proud there, and its lower
    #  layer came through its upper one)
    names = list(model.paints)
    skin = [f for k, f in enumerate(model.faces) if names[model.fpaint[k]] in ("upper", "upper2", "under", "nose", "boss") or (k < len(model.fcoat) and model.fcoat[k])]
    tree = BVHTree.FromPolygons([tuple(v) for v in model.verts], [tuple(f) for f in skin])
    whole = "wing.port" in model.part_at                                         # (an aircraft; or a range of them on a deck, each mark naming its part)
    y_cut = abs(model.parts[model.part_at["wing.port"]].pivot.y) if whole else 0.0
    x_cut = model.parts[model.part_at["tail"]].pivot.x if whole else 0.0
    verts, faces, cols, groups = [], [], [], []
    for mk in todo:
        if mk["tag"] == "#":
            continue
        small = bool(mk.get("small"))                                            # (a record mark: a hand high, matt, a little worn)
        proud = 0.009 if small else 0.012                                        # (clear of her scribed lines and rivets)
        sides = (1.0, -1.0) if mk.get("both") else (1.0,)
        for hand in sides:
            at = Vector((mk["at"][0], -mk["at"][1] * hand, mk["at"][2]))            # (the file's y is to the right)
            n = Vector((mk["facing"][0], -mk["facing"][1] * hand, mk["facing"][2])).normalized()
            up = Vector((1, 0, 0)) if abs(n.z) > 0.7 else Vector((0, 0, 1))
            if mk.get("up"):                                                     # (a machine that is not level says which way is up)
                up = Vector((mk["up"][0], -mk["up"][1] * hand, mk["up"][2]))
            v = (up - n * up.dot(n)).normalized()
            u = v.cross(n)
            if mk.get("nudge"):                                                  # (the painter's hand: a little off its place, and leaning)
                at = at + u * mk["nudge"][0] + v * mk["nudge"][1]
            if mk.get("turn"):
                t_ = math.radians(mk["turn"])
                u, v = u * math.cos(t_) + v * math.sin(t_), v * math.cos(t_) - u * math.sin(t_)
            flip = -1.0 if (mk["tag"] == "UK-FIN" and u.x < 0) else 1.0
            part = ("tail" if at.x < x_cut else ("wing.port" if at.y > y_cut else "wing.starboard" if at.y < -y_cut else "plane")) if whole else mk.get("part")
            if mk.get("part") in model.part_at:
                part = mk["part"]
            r = mk["radius"]
            tag = mk["tag"]
            if tag == "*" and own:
                tag = own(n) if callable(own) else own
            if tag == "*FIN":                                                    # (her owner's flag on her rudder; nothing if nobody says whose she is)
                if not fin or not mk.get("quad"):
                    continue
                flip = 1.0 if u.x > 0 else -1.0
            if "finish" in mk:                                                   # (a bomber's panel: the marks for the finish under it)
                drawn = bomb_mark(mk["finish"]) if tag == "BOMB" else kill_mark(tag, mk["finish"])
            else:
                drawn = flag(mk, fin) if tag == "*FIN" else layers(tag) if isinstance(tag, list) else mk["drawn"] if "drawn" in mk else shapes(tag)
            for layer, polys in enumerate(drawn):
                for col, poly in polys:
                    # (cut small before it is laid on her: a big flat piece dips under a round fuselage in its middle,
                    #  and her skin comes through it)
                    flat = [Vector((a * flip, b, 0.0)) for (a, b) in poly]
                    tris = [[flat[i] for i in t] for t in tessellate_polygon([flat])]
                    longest = max(((t[i] - t[(i + 1) % 3]).length for t in tris for i in range(3)), default=0.0) * r
                    for _ in range(min(4, max(0, math.ceil(math.log2(max(longest / 0.13, 1.0)))))):
                        tris = [piece for (a, b, c) in tris for ab, bc, ca in [((a + b) / 2, (b + c) / 2, (c + a) / 2)]
                                for piece in ((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca))]
                    for tri in tris:
                        idx = []
                        for q in tri:
                            p = at + (u * q.x + v * q.y) * r
                            hit = tree.ray_cast(p + n * 0.6, -n, 1.2)
                            if hit[0] is not None and hit[1].dot(n) > 0.3:
                                p = hit[0]
                            idx.append(len(verts))
                            verts.append(p + n * (proud + (0.002 if small else 0.004) * layer))
                            groups.append(part)
                        a_, b_, c_ = verts[idx[0]], verts[idx[1]], verts[idx[2]]
                        if (b_ - a_).cross(c_ - a_).dot(n) < 0:
                            idx.reverse()
                        faces.append(idx)
                        if "fade" in mk:                                         # (a panel's mark: its row's tin, faded toward the skin by its age)
                            c_ = mk["tin"] if (mk.get("tin") and col == BOMB_YELLOW) else col
                            c_ = mix(c_, mk["skin"], round(1.0 - mk["fade"], 2))     # (to the hundredth: a kill's 0.85 is drawn at 0.85)
                            cols.append(mix(c_, "6E6A5C", 0.05) + "m")
                        else:
                            cols.append(mix(col, "6E6A5C", 0.14) + "m" if small else col)
    if not faces:
        return []
    me = bpy.data.meshes.new(model.name + "_marks")
    me.from_pydata([tuple(p) for p in verts], [], faces)
    used = sorted(set(cols))
    for c in used:
        name = "mark:" + c
        mat = bpy.data.materials.get(name)
        if mat is None:
            mat = bpy.data.materials.new(name)
            mat.use_nodes = True
            b = mat.node_tree.nodes["Principled BSDF"]
            rgb = tuple(((int(c[i:i + 2], 16) / 255.0) ** 2.2) for i in (0, 2, 4))
            b.inputs["Base Color"].default_value = (*rgb, 1.0)
            b.inputs["Roughness"].default_value = 0.9 if c.endswith("m") else 0.6
            if c.endswith("m"):
                b.inputs["Specular IOR Level"].default_value = 0.15
        me.materials.append(mat)
    me.polygons.foreach_set("material_index", [used.index(c) for c in cols])
    me.update()
    o = bpy.data.objects.new(model.name + "_marks", me)
    bpy.context.scene.collection.objects.link(o)
    o.visible_shadow = False
    for part in sorted(set(groups)):
        g = o.vertex_groups.new(name=part)
        g.add([i for i, p in enumerate(groups) if p == part], 1.0, "REPLACE")
    o.parent = arm
    mod = o.modifiers.new("rig", "ARMATURE")
    mod.object = arm
    return [o]
