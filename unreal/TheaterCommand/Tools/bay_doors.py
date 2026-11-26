"""The bomb bay: a real bay cut in the belly of each bomber that carries its bombs inside, with two doors that open.

Run after Tools/build_plane.py, on the files it wrote (see build_plane.sh), or alone:

    python3 Tools/bay_doors.py Content/Models [--only B17,Lancaster]

(Brian, 2026-11-21: "do the bombay doors open on the bombers and you see them drop bombs and then close the bomb bay
doors after?", then "add working doors in release 1" and "have a realistic number of bombs drop from them as well".)

What it does to a model's near shape: the skin of the belly over the bay is cut out along the fuselage's own
rings and lines, and split down the middle into two doors, each hinged along its outer edge. Each door keeps the very
points, facings and paint the belly had there, so with the doors shut the model looks as it did; it gains an inside
face, a little in from the skin. Above the doors a box is the bay itself, in paints of its own (18 its roof, 19
its walls, a beam down the middle of the roof and frames across it), and whatever of the machine's own
insides lay in that box (the wing's centre section, sculpted through the fuselage) is taken out, as it is hidden
behind the skin anyway. In the near shape the bay is loaded: the machine's usual bombs, racked in it from the doors
up, nose forward.

Nothing else moves. Every point the file had keeps its place in the list (the game finds the airscrews by the order of
the points); the new ones are added after them. A point that only the cut belly used stays in the list, unused.

What the file gains, in the form of the battle models (tools/battle-models): `part` in the near shape and the far one (the
far one, drawn for machines away from the eye, has no bay and is all part 0), the part each point belongs to, and `parts`, the parts themselves:

    0 plane          the machine (the game flies it)
    1 bay.port       the port door: turns about `axis` through `pivot`, degrees, plus opens it, 0 to 85
    2 bay.starboard  the starboard door, the same
    3 bombs          the load in the bay (near shape only): hidden once the game lets its own bombs fall

Turns are right-handed about the axis (x forward, y right, z up), as in the battle models' bay doors, so a door
turned 80 degrees hangs nearly straight down from its hinge. A model without an entry in BAYS is left as it was: the
dive bombers, the fighters and the light attack aircraft carry their bombs outside.
"""
import json
import math
import os
import sys

# Each bay: where it runs along the fuselage (front and back, metres; the cut snaps to the nearest ring of the skin),
# half its width (snaps to the nearest line of the skin), how high its roof is (above the middle of the fuselage, as
# a share of the fuselage's half-height), and its usual load: how many bombs, their length and their girth (metres).
# Placed as on the battle models' bombers, which were drawn from published measurements; but the He 111 and the twin
# bomber have the bay just ahead of the wing, as on these models the wing's centre section runs as low as the belly.
BAYS = {
    "B17": {"front": 2.45, "back": -0.1, "half": 0.52, "roof": 0.35, "bombs": 12, "bomb": (1.5, 0.36),         # 500 lb
            "inside": ("A0A4A7", "909497")},                                     # (bare aluminium)
    "Lancaster": {"front": 6.77, "back": -3.37, "half": 0.62, "roof": 0.35, "bombs": 14, "bomb": (1.75, 0.47),  # 1,000 lb
                  "inside": ("767A7E", "82868A")},                               # (a darker grey)
    "He111": {"front": 4.05, "back": 2.45, "half": 0.44, "roof": 0.35, "bombs": 8, "bomb": (1.64, 0.37),        # 250 kg
              "inside": ("8C9484", "838B7C")},                                   # (a grey-green interior)
    "Bomber": {"front": 4.55, "back": 2.15, "half": 0.36, "roof": 0.35, "bombs": 6, "bomb": (1.6, 0.37),        # 250 kg
               "inside": ("8C9484", "838B7C")},
}

PAINT = {"under": 2, "steel": 7, "line": 10, "leg": 13, "bomb": 14, "bay": 18, "bay_wall": 19}
DOOR_THICK = 0.02                 # (the inside face of a door, this far in from its skin)


def v_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def v_cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_unit(a):
    n = math.sqrt(v_dot(a, a))
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-12 else (0.0, 0.0, 0.0)


class Mesh:
    """One shape of a model (near or far) as lists, with room to add points and triangles."""

    def __init__(self, m):
        self.m = m
        P, F = m["points"], m["facings"]
        self.P = [(P[3 * i], P[3 * i + 1], P[3 * i + 2]) for i in range(len(P) // 3)]
        self.F = [(F[3 * i], F[3 * i + 1], F[3 * i + 2]) for i in range(len(F) // 3)]
        self.paint, self.wear, self.blend, self.soot = list(m["paints"]), list(m["wear"]), list(m["blend"]), list(m["soot"])
        T = m["triangles"]
        self.T = [(T[3 * t], T[3 * t + 1], T[3 * t + 2]) for t in range(len(T) // 3)]
        self.part = [0] * len(self.P)
        self.first_new = len(self.P)

    def point(self, p, f, paint, part, wear=0, blend=0, soot=0):
        self.P.append(tuple(round(c, 4) for c in p))
        self.F.append(tuple(round(c, 3) for c in v_unit(f)))
        self.paint.append(paint)
        self.wear.append(wear)
        self.blend.append(blend)
        self.soot.append(soot)
        self.part.append(part)
        return len(self.P) - 1

    def face(self, a, b, c, out):
        """A triangle wound so that its front is to 'out' (the files' triangles turn anticlockwise seen from the front)."""
        n = v_cross(v_sub(self.P[b], self.P[a]), v_sub(self.P[c], self.P[a]))
        self.T.append((a, b, c) if v_dot(n, out) >= 0.0 else (a, c, b))

    def quad(self, a, b, c, d, out):
        self.face(a, b, c, out)
        self.face(a, c, d, out)

    def write(self):
        m = self.m
        m["points"] = [c for p in self.P for c in p]
        m["facings"] = [c for f in self.F for c in f]
        m["paints"], m["wear"], m["blend"], m["soot"] = self.paint, self.wear, self.blend, self.soot
        m["triangles"] = [i for t in self.T for i in t]
        m["part"] = self.part


def hull_of(spec_hull, square):
    """The fuselage as Tools/build_plane.py's Body draws it: its middle, half-width and half-height at x, and its rings."""
    here = os.path.dirname(os.path.abspath(__file__))
    source = open(os.path.join(here, "build_plane.py")).read()
    space = {"math": math, "Vector": None}
    exec(source[source.index("def curve("):source.index("def aerofoil(")], space)
    return space["Body"](spec_hull, square)


def around_of(p, body, square, around):
    """Which of the skin's lines round the fuselage a point is on (0 to around-1, a quarter round from the right side
    being the top), or None if it is not on the skin."""
    x, y, z = p
    w, h, zm = body.w(-x), body.h(-x), body.z(-x)
    if w < 1e-6 or h < 1e-6:
        return None
    r = abs(y / w) ** square + abs((z - zm) / h) ** square
    if abs(r - 1.0) > 0.03 * square:
        return None
    power = 2.0 / square
    c = math.copysign(abs(y / w) ** (1.0 / power), y)
    s = math.copysign(abs((z - zm) / h) ** (1.0 / power), z - zm)
    a = math.atan2(s, c) % (2.0 * math.pi)
    k = a / (2.0 * math.pi / around)
    return int(round(k)) % around if abs(k - round(k)) < 0.08 else None


def bomb_shape(mesh, at, length, girth, part, sides=8):
    """A bomb lying nose forward: a round body with a blunt ogive nose, a tapering tail and a box of four fins."""
    r = girth / 2.0
    x0 = at[0] - length / 2.0
    prof = [(1.0, 0.0), (0.96, 0.5), (0.88, 0.86), (0.76, 1.0), (0.36, 1.0), (0.26, 0.78), (0.2, 0.5)]
    rings = []
    for (u, k) in prof:
        x = x0 + u * length
        ring = []
        for s in range(sides):
            a = 2.0 * math.pi * s / sides
            c, sn = math.cos(a), math.sin(a)
            p = (x, at[1] + c * r * k, at[2] + sn * r * k)
            # (the facing of a body of revolution: mostly round, tipped by the slope of the profile)
            f = (0.35 if u > 0.75 else (-0.3 if u < 0.3 else 0.0), c, sn)
            ring.append(mesh.point(p, f if k > 0 else (1.0, 0.0, 0.0), PAINT["bomb"], part))
        rings.append(ring)
    for ra, rb in zip(rings, rings[1:]):
        for s in range(sides):
            a, b, c, d = ra[s], ra[(s + 1) % sides], rb[(s + 1) % sides], rb[s]
            mid = tuple(sum(mesh.P[i][j] for i in (a, b, c, d)) / 4.0 for j in range(3))
            mesh.quad(a, b, c, d, v_sub(mid, (mid[0], at[1], at[2])))
    # (its tail end, closed)
    tail = rings[-1]
    hub = mesh.point((x0 + 0.2 * length, at[1], at[2]), (-1.0, 0.0, 0.0), PAINT["bomb"], part)
    for s in range(sides):
        mesh.face(hub, tail[s], tail[(s + 1) % sides], (-1.0, 0.0, 0.0))
    # The fins: four thin blades in a cross, from the tail cone back to the end, as wide as the body.
    for a in (0.25 * math.pi, 0.75 * math.pi, 1.25 * math.pi, 1.75 * math.pi):
        c, sn = math.cos(a), math.sin(a)
        side = (0.0, -sn, c)
        q = []
        for (u, k) in ((0.24, 0.35), (0.0, 0.35), (0.0, 1.02), (0.12, 1.02)):
            q.append((x0 + u * length, at[1] + c * r * k, at[2] + sn * r * k))
        for facing in (side, (0.0, sn, -c)):
            ids = [mesh.point(p, facing, PAINT["bomb"], part) for p in q]
            mesh.quad(ids[0], ids[1], ids[2], ids[3], facing)


def belly_under(skin, x, y):
    """The height of the shut doors' skin straight under (x, y): the highest of the door triangles over that point, or
    None where none is."""
    top = None
    for a, b, c in skin:
        if x < min(a[0], b[0], c[0]) or x > max(a[0], b[0], c[0]) or y < min(a[1], b[1], c[1]) or y > max(a[1], b[1], c[1]):
            continue
        den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(den) < 1e-12:
            continue
        u = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / den
        v = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / den
        w = 1.0 - u - v
        if min(u, v, w) < -1e-9:
            continue
        z = u * a[2] + v * b[2] + w * c[2]
        top = z if top is None else max(top, z)
    return top


def rack_lift(skin, xc, yc, length, girth, z):
    """How far a bomb racked with its middle at (xc, yc, z) must be lifted so that, the doors shut, all of it (its fins
    too) stays inside them, a centimetre above their skin: 0 where it already does."""
    r = girth * 0.51
    need = None
    for su in range(17):
        x = xc - length / 2.0 + length * su / 16.0
        for sv in range(11):
            dy = -r + 2.0 * r * sv / 10.0
            zs = belly_under(skin, x, yc + dy)
            if zs is None:
                continue
            low = zs + 0.01 + math.sqrt(max(0.0, r * r - dy * dy))
            need = low if need is None else max(need, low)
    return 0.0 if need is None or need <= z else need - z


def block(mesh, lo, hi, paint):
    """A box with square sides, each face its own points (sharp edges), every face outward but the top."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    sides = (((x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (0.0, 0.0, -1.0)),
             ((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1), (0.0, -1.0, 0.0)),
             ((x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1), (0.0, 1.0, 0.0)),
             ((x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1), (-1.0, 0.0, 0.0)),
             ((x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1), (1.0, 0.0, 0.0)))
    for (a, b, c, d, out) in sides:
        ids = [mesh.point(p, out, paint, 0) for p in (a, b, c, d)]
        mesh.quad(ids[0], ids[1], ids[2], ids[3], out)


def cut(model, spec, plane, fine):
    """Cuts the bay in one shape of a model; returns what it found, for the report."""
    shape = model["near" if fine else "far"]
    mesh = Mesh(shape)
    square = plane.get("square", 2.0)
    body = hull_of(plane["hull"], square)
    zmid = lambda x: body.z(-x)
    hgt = lambda x: body.h(-x)
    around = 28 if fine else 14
    ring_x = body.xs(0.24 if fine else 0.6)
    n = len(mesh.P)

    def ring_of(x):
        k = min(range(len(ring_x)), key=lambda j: abs(ring_x[j] - x))
        return k if abs(ring_x[k] - x) < 2e-3 else None

    line = [None] * n
    ring = [None] * n
    for i in range(n):
        if mesh.paint[i] == PAINT["under"]:
            ring[i] = ring_of(mesh.P[i][0])
            if ring[i] is not None:
                line[i] = around_of(mesh.P[i], body, square, around)
    bottom = 3 * around // 4
    if (3 * around) % 4:
        raise SystemExit("bay_doors: the skin has no line along the keel to part the doors on")
    # Where the cut runs: the rings of the skin nearest the bay's ends, and the lines round it nearest its half-width.
    rf = ring_of(min(ring_x, key=lambda v: abs(v - spec["front"])))
    rb = ring_of(min(ring_x, key=lambda v: abs(v - spec["back"])))
    xf, xb = ring_x[rf], ring_x[rb]
    xm = (xf + xb) / 2.0
    wide = lambda k: abs(math.cos(2.0 * math.pi * k / around)) ** (2.0 / square) * body.w(-xm)
    m = min(range(1, around // 4), key=lambda k: abs(wide(bottom + k) - spec["half"]))
    half = wide(bottom + m)
    eps = 2e-3

    def in_print(p):
        return xb - eps <= p[0] <= xf + eps and abs(p[1]) <= half + 0.02 and p[2] < zmid(p[0])

    def on_door(i):
        return line[i] is not None and rf <= ring[i] <= rb and abs(line[i] - bottom) <= m

    # The doors: every triangle of the belly's skin inside the cut, and the scribed lines lying on it.
    door = {}
    for t, tri in enumerate(mesh.T):
        if all(on_door(i) for i in tri):
            door[t] = 1 if sum(mesh.P[i][1] for i in tri) < 0.0 else 2
    def over_hole(p):
        return xb + 0.01 < p[0] < xf - 0.01 and abs(p[1]) < half - 0.01 and p[2] < zmid(p[0])

    for t, tri in enumerate(mesh.T):
        if t in door or mesh.paint[tri[0]] != PAINT["line"]:
            continue
        if not all(in_print(mesh.P[i]) for i in tri):
            if any(over_hole(mesh.P[i]) for i in tri):
                door[t] = 0                              # (a line running onto a door from the skin: cut at the seam)
            continue
        ys = [mesh.P[i][1] for i in tri]
        if min(ys) < -eps and max(ys) > eps:
            door[t] = 0                                  # (a line across the keel: it would tear; it goes)
        else:
            door[t] = 1 if sum(ys) < 0.0 else 2
    if not door:
        raise SystemExit(f"bay_doors: no belly skin found in the cut ({xb} to {xf}, half {half})")

    # The hole's edge: the doors' edges that no other door shares, as the skin left behind winds them.
    count = {}
    for t, side in door.items():
        if side == 0 or mesh.paint[mesh.T[t][0]] == PAINT["line"]:
            continue
        a, b, c = mesh.T[t]
        for e in ((a, b), (b, c), (c, a)):
            k = (min(e), max(e))
            count[k] = count.get(k, 0) + 1
    edge = [k for k, v in count.items() if v == 1]

    # Each door its own points: the belly's, unchanged, so that shut it is the belly.
    copy = {1: {}, 2: {}}
    for t, side in door.items():
        if side == 0:
            continue
        for i in mesh.T[t]:
            if i not in copy[side]:
                copy[side][i] = mesh.point(mesh.P[i], mesh.F[i], mesh.paint[i], side, mesh.wear[i], mesh.blend[i], mesh.soot[i])
    kept, doors = [], []
    for t, tri in enumerate(mesh.T):
        side = door.get(t)
        if side is None:
            kept.append(tri)
        elif side:
            doors.append((side, tri))
    # Its inside face, a little in from the skin, bare metal.
    inner = {1: {}, 2: {}}
    for side, tri in doors:
        if mesh.paint[tri[0]] == PAINT["line"]:
            continue
        for i in tri:
            if i not in inner[side]:
                p, f = mesh.P[i], mesh.F[i]
                inner[side][i] = mesh.point((p[0] - f[0] * DOOR_THICK, p[1] - f[1] * DOOR_THICK, p[2] - f[2] * DOOR_THICK),
                                            (-f[0], -f[1], -f[2]), PAINT["leg"], side)
    roof = zmid(xm) + spec["roof"] * hgt(xm)
    box_lo = min(mesh.P[i][2] for i in copy[1]) if copy[1] else zmid(mid_ring) - hgt(mid_ring)

    def inside_hull(p):
        w, h, zm = body.w(-p[0]), body.h(-p[0]), body.z(-p[0])
        return w > 1e-6 and h > 1e-6 and abs(p[1] / w) ** square + abs((p[2] - zm) / h) ** square < 0.99

    def in_box(p):
        side = abs(math.cos(2.0 * math.pi * (bottom + m) / around)) ** (2.0 / square) * body.w(-p[0])
        return xb + eps < p[0] < xf - eps and abs(p[1]) < side - eps and box_lo + 0.03 < p[2] < roof + 0.05

    # The machine's own insides that lay in the bay go: hidden behind the skin, they would show through the open doors.
    # (Only what lies wholly inside the fuselage: its skin, and anything that reaches out through it, stays.)
    def hidden_in_bay(tri):
        return (not all(line[i] is not None for i in tri) and all(inside_hull(mesh.P[i]) for i in tri)
                and any(in_box(mesh.P[i]) for i in tri))

    gone = [tri for tri in kept if hidden_in_bay(tri)]
    kept = [tri for tri in kept if not hidden_in_bay(tri)]
    mesh.T = kept + [tuple(copy[side][i] for i in tri) for side, tri in doors]
    for side, tri in doors:
        if mesh.paint[tri[0]] == PAINT["line"]:
            continue
        a, b, c = (inner[side][i] for i in tri)
        mesh.T.append((a, c, b))

    # The bay: a dark wall from every edge of the hole straight up to its roof, and the roof.
    centre = ((xb + xf) / 2.0, 0.0, (box_lo + roof) / 2.0)
    wall = PAINT["bay_wall"]
    for (a, b) in edge:
        pa, pb = mesh.P[a], mesh.P[b]
        mid = ((pa[0] + pb[0]) / 2.0, (pa[1] + pb[1]) / 2.0, (pa[2] + pb[2]) / 2.0)
        inward = v_sub(centre, mid)
        inward = (inward[0], inward[1], 0.0)
        along = v_sub(pb, pa)
        out = v_unit(v_cross(along, (0.0, 0.0, 1.0)))
        if v_dot(out, inward) < 0.0:
            out = (-out[0], -out[1], -out[2])
        q = [mesh.point(pa, out, wall, 0), mesh.point(pb, out, wall, 0),
             mesh.point((pb[0], pb[1], roof), out, wall, 0), mesh.point((pa[0], pa[1], roof), out, wall, 0)]
        mesh.quad(q[0], q[1], q[2], q[3], out)
    corners = [mesh.point(p, (0.0, 0.0, -1.0), PAINT["bay"], 0) for p in
               ((xb, -half, roof), (xf, -half, roof), (xf, half, roof), (xb, half, roof))]
    mesh.quad(corners[0], corners[1], corners[2], corners[3], (0.0, 0.0, -1.0))
    # Its structure: a beam down the middle of the roof (a heavy bomber's catwalk) and frames across it.
    under = roof - 0.004
    block(mesh, (xb + 0.02, -0.08, under - 0.12), (xf - 0.02, 0.08, under), wall)
    frames = max(2, min(5, int(round((xf - xb) / 0.9)) - 1))
    for k in range(frames):
        x = xb + (xf - xb) * (k + 1) / (frames + 1)
        block(mesh, (x - 0.035, -half + 0.01, under - 0.09), (x + 0.035, half - 0.01, under), wall)

    # The hinges: along each door's outer edge.
    parts = {}
    for side, sign in ((1, -1.0), (2, 1.0)):
        rim = [mesh.P[i] for i in copy[side] if abs(abs(mesh.P[i][1]) - half) < eps and mesh.paint[i] != PAINT["line"]]
        back = min(rim, key=lambda p: p[0])
        front = max(rim, key=lambda p: p[0])
        axis = v_unit(v_sub(back, front) if side == 1 else v_sub(front, back))
        pivot = tuple(round((back[j] + front[j]) / 2.0, 4) for j in range(3))
        parts[side] = {"pivot": list(pivot), "axis": [round(c, 4) for c in axis]}

    # The load, racked from the doors up: bottom row first, two racks either side of the middle. Each rack stands clear
    # of the shut doors under it: where the belly rises (a Lancaster's toward her tail) its bombs are lifted, and a place
    # that lifted would not clear the beam is left empty. (Pre-check, 2026-11-21: her aft bombs, racked at one height
    # from the bay's lowest point, showed through her shut doors.)
    loaded = 0
    if fine and spec.get("bombs"):
        length, girth = spec["bomb"]
        across = max(1, int((half - 0.04) // (girth + 0.03)))
        along = max(1, int((xf - xb - 0.1) // (length + 0.1)))
        layers = max(1, int((roof - 0.13 - box_lo - 0.12) // (girth + 0.05)))     # (clear of the beam)
        gap_x = (xf - xb) / along
        skin = [tuple(mesh.P[i] for i in tri) for _, tri in doors]
        lifts = {}
        for layer in range(layers + 2):
            for k in range(along):
                for side in (-1.0, 1.0):
                    for j in range(across):
                        if loaded >= spec["bombs"]:
                            break
                        y = side * (half - 0.02 - girth / 2.0 - j * (girth + 0.03))
                        z = box_lo + 0.08 + girth / 2.0 + layer * (girth + 0.05)
                        if (k, side, j) not in lifts:
                            lifts[(k, side, j)] = rack_lift(skin, xb + gap_x * (k + 0.5), y, length, girth, box_lo + 0.08 + girth / 2.0)
                        lift = lifts[(k, side, j)]
                        if lift > 0.0 or layer >= layers:
                            z += lift
                            if z + girth * 0.51 > roof - 0.14:
                                continue
                        bomb_shape(mesh, (xb + gap_x * (k + 0.5), y, z), length, girth, 3)
                        loaded += 1
    mesh.write()
    return {"cut": [round(xb, 3), round(xf, 3)], "half": round(half, 3), "lines_cut": sum(1 for v in door.values() if v == 0), "roof": round(roof, 3), "doors": parts, "door_triangles": len(doors),
            "gone": len(gone), "bombs": loaded, "points_before": n, "points_after": len(mesh.P),
            "triangles_after": len(mesh.T)}


def fit(path, plane, spec):
    model = json.load(open(path))
    if "parts" in model:
        print(f"Theater Command: {os.path.basename(path)} already has its parts (a battle model, or its bay is cut): left as it is")
        return None
    if len(model["colours"]) != 18:
        raise SystemExit(f"bay_doors: {path} has {len(model['colours'])} colours, not Tools/build_plane.py's 18")
    model["colours"] = model["colours"] + list(spec["inside"])
    near = cut(model, spec, plane, True)
    # (the far shape, drawn for every machine away from the eye, has no line along its keel to part two doors on,
    # and its doors would be a few dots wide: it stays shut, all of it the machine)
    model["far"]["part"] = [0] * len(model["far"]["paints"])
    note = "a door of her bomb bay, hinged along its outer edge; plus opens it"
    model["parts"] = [
        {"name": "plane", "parent": -1, "pivot": [0.0, 0.0, 0.0], "kind": "root", "note": "the machine; the game flies it"},
        {"name": "bay.port", "parent": 0, "pivot": near["doors"][1]["pivot"], "kind": "turn", "axis": near["doors"][1]["axis"],
         "limits": [0, 85], "note": note},
        {"name": "bay.starboard", "parent": 0, "pivot": near["doors"][2]["pivot"], "kind": "turn", "axis": near["doors"][2]["axis"],
         "limits": [0, 85], "note": note},
        {"name": "bombs", "parent": 0, "pivot": [round((near["cut"][0] + near["cut"][1]) / 2.0, 4), 0.0, near["roof"]],
         "kind": "slide", "slide": [0.0, 0.0, -1.0], "slide_limits": [0.0, 60.0],
         "note": f"her load in the bay, {near['bombs']} bombs (near shape only): hidden when the game lets its own fall"},
    ]
    model["about"] = model["about"] + (" Its bomb bay (Tools/bay_doors.py): `part` says which part each point belongs to, "
                                       "`parts` are the parts (0 the machine, 1 and 2 the bay doors, turned about their "
                                       "axis through their pivot, plus opens, 3 the bombs in the bay). Paints 18 bay roof, "
                                       "19 bay walls.")
    with open(path, "w") as out:
        json.dump(model, out, separators=(",", ":"))
    return near


def main():
    args = sys.argv[1:]
    folder = args[0] if args else "Content/Models"
    only = [n for n in (args[args.index("--only") + 1].split(",") if "--only" in args else BAYS) if n in BAYS]
    here = os.path.dirname(os.path.abspath(__file__))
    space = {"PLANES": {}}
    exec(open(os.path.join(here, "plane_numbers.py")).read(), space)
    for name in only:
        near = fit(os.path.join(folder, name + ".json"), space["PLANES"][name], BAYS[name])
        if near is None:
            continue
        print(f"Theater Command: {name}'s bomb bay: cut {near['cut'][0]} to {near['cut'][1]} m, half-width {near['half']} m, "
              f"{near['door_triangles']} door triangles, {near['lines_cut']} of scribed lines cut at the seams, {near['gone']} hidden ones taken out, {near['bombs']} bombs racked; "
              f"near {near['points_before']} to {near['points_after']} points, {near['triangles_after']} triangles")


if __name__ == "__main__":
    main()
