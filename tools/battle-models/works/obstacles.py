"""Obstacles: barbed wire (triple concertina and a double-apron fence), dragon's teeth, steel hedgehogs, a marked
minefield and a swing-beam road bar.

Every shape is our own plain design. The "length" pieces run along y from -5 to +5 with the enemy toward +x, so
lengths join end to end. Wire is drawn thicker than life (6 mm, 12 mm in the plain mesh) so it reads from afar.
"""
import math

from mathutils import Matrix, Quaternion, Vector

import bits
from bits import X, Y, Z, rnd
from posts import weather
from rigid import smooth

PICKETS = (-5.0, -5.0 / 3.0, 5.0 / 3.0, 5.0)


def trod(m, x, y, r, s=0.8):
    """A circle of trodden earth for the stage and the game to wear into the ground (no pad is modelled)."""
    m.info.setdefault("worn", []).append((round(x, 3), round(y, 3), round(r, 3), s))


# ---- wire helpers ---------------------------------------------------------------------------------------

def patchy(m, seed, x=0.0, wide=0.7, r=(0.55, 1.2), s=(0.35, 0.8), every=1.5):
    """Bare patches along a belt, for the stage to wear (nobody walks a belt's length, so there is no path under it:
    the grass is trodden out only here and there, where the wiring party worked)."""
    g = rnd(seed)
    y = -5.0 + g.uniform(0.2, every)
    while y < 5.0:
        m.info.setdefault("worn", []).append((round(x + g.uniform(-wide, wide), 3), round(y, 3), round(g.uniform(*r), 3), round(g.uniform(*s), 2)))
        y += g.uniform(0.6, 1.6) * every


def wire(m, pts, r, paint="wire", sides=None, smooth_=True):
    """A wire bent through points: frames carried along it, faces wound outward (no test needed), ends open."""
    pts = [Vector(p) for p in pts]
    if len(pts) < 2:
        return
    sides = sides or (5 if m.fine else 3)
    n = len(pts)
    rings, side = [], None
    for i, p in enumerate(pts):
        d = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
        if d.length < 1e-9:
            d = X.copy()
        d.normalize()
        if side is None:
            side = d.cross(Z)
            if side.length < 1e-4:
                side = d.cross(X)
        else:
            side = side - d * side.dot(d)
            if side.length < 1e-6:
                side = d.cross(Z) if abs(d.z) < 0.9 else d.cross(X)
        side.normalize()
        up = d.cross(side)
        rings.append([p + (side * math.cos(2 * math.pi * k / sides) + up * math.sin(2 * math.pi * k / sides)) * r for k in range(sides)])
    verts = [q for ring in rings for q in ring]
    faces = [(i * sides + k, i * sides + (k + 1) % sides, (i + 1) * sides + (k + 1) % sides, (i + 1) * sides + k) for i in range(n - 1) for k in range(sides)]
    m.add(verts, faces, paint, smooth_, None)


def barbs(m, pts, every, size=0.04, paint="wire", seed=0, r=0.006, collar=True):
    """Four-point barbs along a wire: a twist of wire round it and four short spikes, crossed in pairs."""
    if not m.fine:
        return
    g = rnd(seed)
    pts = [Vector(p) for p in pts]
    verts, faces = [], []
    run, nxt = 0.0, g.uniform(0, every)
    for a, b in zip(pts, pts[1:]):
        ln = (b - a).length
        if ln < 1e-9:
            continue
        d = (b - a) / ln
        while nxt <= run + ln:
            p = a.lerp(b, (nxt - run) / ln)
            s = d.cross(Vector((g.uniform(-1, 1), g.uniform(-1, 1), g.uniform(-1, 1))))
            if s.length < 1e-3:
                s = d.cross(Z) if abs(d.z) < 0.9 else d.cross(X)
            s.normalize()
            t = d.cross(s)
            # the twist: a short fat collar (four sides)
            base = len(verts)
            for e in ((-0.008, 0.008) if collar else ()):
                for k in range(4):
                    a4 = math.pi * k / 2 + 0.4
                    verts.append(p + d * e + (s * math.cos(a4) + t * math.sin(a4)) * (r * 1.55))
            if collar:
                faces += [(base + k, base + (k + 1) % 4, base + 4 + (k + 1) % 4, base + 4 + k) for k in range(4)]
            # four spikes: three-sided cones
            for sp, lean in ((s, 1), (-s, 1), (t, -1), (-t, -1)):
                ax = (sp + d * 0.32 * lean * g.uniform(0.6, 1.3)).normalized()
                u = ax.cross(d)
                if u.length < 1e-4:
                    u = ax.cross(Z)
                u.normalize()
                v = ax.cross(u)
                root = p + sp * r * 0.9
                base = len(verts)
                for k in range(3):
                    a3 = 2 * math.pi * k / 3
                    verts.append(root + (u * math.cos(a3) + v * math.sin(a3)) * 0.0028)
                verts.append(root + ax * size * g.uniform(0.8, 1.15))
                # (u x v = ax: round the ring anticlockwise about the spike, then to its tip: outward)
                faces += [(base + k, base + (k + 1) % 3, base + 3) for k in range(3)]
            nxt += every * g.uniform(0.75, 1.25)
        run += ln
    if faces:
        m.add(verts, faces, paint, False, None)


def wrap(m, at, axis, r, turns=3.0, rise=0.012, wr=0.0045, paint="wire"):
    """A few turns of tie wire wound round something (a picket, a crossing)."""
    at, axis = Vector(at), Vector(axis).normalized()
    side = axis.cross(Z)
    if side.length < 1e-4:
        side = axis.cross(X)
    side.normalize()
    up = axis.cross(side)
    n = int(turns * (8 if m.fine else 4))
    pts = [at + axis * (rise * turns * (k / n - 0.5)) + (side * math.cos(2 * math.pi * turns * k / n) + up * math.sin(2 * math.pi * turns * k / n)) * r for k in range(n + 1)]
    wire(m, pts, wr, paint, 4 if m.fine else 3)


def y_at(pts, y):
    """Where a polyline running up y crosses y."""
    for a, b in zip(pts, pts[1:]):
        if a.y <= y <= b.y and b.y > a.y:
            return a.lerp(b, (y - a.y) / (b.y - a.y))
    return pts[0] if y <= pts[0].y else pts[-1]


def cut_y(pts, ya, yb):
    """The run of a polyline (y rising along it) between ya and yb."""
    pts = [Vector(p) for p in pts]
    return [y_at(pts, ya)] + [p for p in pts if ya < p.y < yb] + [y_at(pts, yb)]


def halves():
    """(part name, y0, y1) of the six half-bays."""
    out = []
    for N in range(1, 4):
        y0, y1 = PICKETS[N - 1], PICKETS[N]
        mid = (y0 + y1) / 2
        out += [(f"bay.{N}.a", y0, mid), (f"bay.{N}.b", mid, y1)]
    return out


def bay_parts(m, root, what="wire"):
    for N in range(1, 4):
        m.part(f"bay.{N}.a", root, (0.0, PICKETS[N - 1], 0.0), kind="free",
               note=f"the {what} of bay {N} on picket {N}'s side (pivot at picket {N}): a breach springs it back to its picket, a tank flattens it")
        m.part(f"bay.{N}.b", root, (0.0, PICKETS[N], 0.0), kind="free",
               note=f"the {what} of bay {N} on picket {N + 1}'s side (pivot at picket {N + 1})")


def picket_part(m, root, N):
    m.part(f"picket.{N}", root, (0.0, PICKETS[N - 1], 0.0), axis=(0, -1, 0), limits=[0.0, 90.0], kind="turn",
           note="plus leans it over toward -x (a tank crossing from the enemy's side pushes it down)")


def wire_clips(m, spread=1.1):
    for N in range(1, 4):
        mid = (PICKETS[N - 1] + PICKETS[N]) / 2
        wob = [[0.0, 1.0, 1.0, 1.0], [0.05, 1.06, 0.62, 1.12], [0.14, spread + 0.06, 0.16, 1.06], [0.27, spread + 0.04, 0.34, 0.94],
               [0.42, spread, 0.2, 1.0], [0.6, spread, 0.28, 0.96], [0.85, spread, 0.235, 0.98], [1.2, spread, 0.25, 0.97]]
        m.clips[f"breach.{N}"] = {
            "long": 1.6, "loop": False, "after": "hold",
            "about": f"A charge cuts bay {N}: each half springs back to its own picket with a wobble and lies there bunched, leaving a gap at `gap.{N}`.",
            "tracks": [{"part": f"bay.{N}.a", "scale": wob}, {"part": f"bay.{N}.b", "scale": [list(k) for k in wob]},
                       {"part": f"picket.{N}", "turn": [[0.0, 0.0], [0.1, 4.0], [0.3, 2.5], [1.6, 3.0]]},
                       {"part": f"picket.{N + 1}", "turn": [[0.0, 0.0], [0.1, 3.0], [0.3, 1.5], [1.6, 2.0]]}],
            "events": [{"t": 0.0, "fx": "blast", "socket": f"gap.{N}", "cue": "grenade_blast"}]}
        flat = [[0.0, 1.0, 1.0, 1.0], [0.35, 1.0, 1.0, 1.0], [0.6, 1.15, 1.0, 0.55], [0.95, 1.3, 1.02, 0.16], [1.4, 1.32, 1.03, 0.12], [2.2, 1.32, 1.03, 0.12]]
        lean = [[0.0, 0.0], [0.4, 0.0], [0.75, 30.0], [1.1, 68.0], [1.35, 77.0], [1.6, 75.0], [2.2, 75.0]]
        m.clips[f"crush.{N}"] = {
            "long": 2.2, "loop": False, "after": "hold",
            "about": f"A tank crosses bay {N} from the enemy's side: the wire is pressed flat into the ground and pickets {N} and {N + 1} are pushed over toward -x.",
            "tracks": [{"part": f"bay.{N}.a", "scale": flat}, {"part": f"bay.{N}.b", "scale": [list(k) for k in flat]},
                       {"part": f"picket.{N}", "turn": lean}, {"part": f"picket.{N + 1}", "turn": [[t + 0.1, a] for t, a in lean[:-1]] + [[2.2, 75.0]]}],
            "events": []}
        m.socket(f"gap.{N}", (0.0, mid, 0.5), (1, 0, 0), part=m.parts[0].name, note=f"the middle of bay {N}: where a charge is laid, where a tank crosses")
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing.", "tracks": [], "events": []}


def angle_picket(m, base, top_z, leg=0.05, thick=0.006, paint="steel", open_to=(-1, 0, 0), sunk=0.35, lean=(0.0, 0.0)):
    base = Vector(base)
    a = base - Z * sunk
    b = base + Vector((lean[0], lean[1], top_z))
    bits.angle_iron(m, a, b, leg, thick, paint, open_to)
    return a, b


def screw_picket(m, base, top_z, eyes, paint="steel", lean=(0.0, 0.0)):
    """A screw picket: a round bar wound into a screw at its foot and bent into eyes for the wire."""
    base = Vector(base)
    r = 0.011
    top = base + Vector((lean[0], lean[1], top_z))
    sides = 8 if m.fine else 4
    m.rod(base - Z * 0.05, top - Z * 0.03, r, paint, sides=sides)
    # the last turn of the screw showing at the ground
    n = 14 if m.fine else 6
    wire(m, [base + Vector((math.cos(2 * math.pi * k / n) * 0.055, math.sin(2 * math.pi * k / n) * 0.055, 0.02 - 0.06 * k / n)) for k in range(n + 1)], 0.009, paint, sides)
    # eyes: little loops in the plane of the line, to its side
    ne = 10 if m.fine else 5
    for h in list(eyes) + [top_z - 0.03]:
        c = base + Vector((lean[0] * h / top_z, lean[1] * h / top_z, h)) + X * 0.04
        pts = [c + Vector((-math.cos(2 * math.pi * k / ne) * 0.035, 0.0, math.sin(2 * math.pi * k / ne) * 0.035)) for k in range(ne + 1)]
        wire(m, pts, 0.008, paint, 6 if m.fine else 3)


def reel(m, at, yaw=0.0, seed=0, lying=False):
    """A reel of barbed wire on its wooden drum, the loose end trailing to the ground."""
    at = Vector(at)
    q = Quaternion(Z, math.radians(yaw))
    ax = q @ (Z if lying else Y)
    c = at + (Z * 0.16 if lying else Z * 0.24)
    sides = 20 if m.fine else 8
    if lying:
        c = at
    g = rnd(seed)
    prof = [(0.0, 0.05)]
    for k in range(9 if m.fine else 3):
        f = (k + 0.5) / (9 if m.fine else 3)
        prof.append((0.02 + 0.28 * f, 0.18 + 0.012 * math.sin(k * 2.1 + g.uniform(0, 3))))
    prof.append((0.32, 0.05))
    m.lathe(c - ax * 0.16, ax, prof, "wire", sides, (True, True))
    for e in (-0.17, 0.17):
        m.lathe(c + ax * (e - 0.012), ax, [(0.0, 0.235), (0.024, 0.235)], "timber", sides, (True, True))
    m.rod(c - ax * 0.2, c + ax * 0.2, 0.025, "timber.end", sides=8)
    if m.fine:
        side = ax.cross(Z)
        if side.length < 1e-4:
            side = q @ X
        side.normalize()
        p0 = c + side * 0.19
        tail = [p0 + side * (0.12 * k) + Z * (-c.z * min(1.0, k / 2.5) + 0.01) + ax * (0.05 * math.sin(k)) for k in range(7)]
        wire(m, tail, 0.005, "wire", 4)
        barbs(m, tail, 0.11, 0.03, "wire", seed + 3, 0.005)


# ---- 1. triple concertina ------------------------------------------------------------------------------

def coil(cx, cz, r, y0, y1, seed, per_turn, pitch=0.28, squash=1.0, floor=None):
    """A concertina coil: a helix along y with a little wander in its pitch, its radius and its middle."""
    g = rnd(seed)
    ph = g.uniform(0.0, 50.0)
    a = g.uniform(0.0, 2 * math.pi)
    pts, y = [], y0
    step = 2 * math.pi / per_turn
    while True:
        rr = r * (1.0 + 0.08 * bits.fbm((ph + 3.0, y * 1.9, 0.3), 0.6) + 0.03 * math.sin(a * 2.0 + ph))
        x = cx + math.cos(a) * rr + 0.035 * bits.fbm((ph, y * 0.7, 5.0), 0.8)
        z = cz + math.sin(a) * rr * squash + 0.03 * bits.fbm((ph + 9.0, y * 0.9, 1.0), 0.8)
        if floor is not None:
            z = max(z, floor + 0.01 * (1.0 + math.sin(a * 3)))
        pts.append(Vector((x, min(y, y1), z)))
        if y >= y1:
            break
        pitch_here = pitch * (1.0 + 0.4 * bits.fbm((ph + 7.0, y * 0.55, 2.0), 0.5))
        y += max(pitch_here, 0.12) / per_turn
        a += step
    return pts


def Wire(fine=True, nation="_default", name="Wire"):
    """Triple concertina: two coils side by side on the ground, one on top, on four pickets, with strand wires along
    the coils' tops tying them to the pickets."""
    m = bits.model(name, "Barbed wire, triple concertina, a length (our own model)", "works", nation, fine)
    root = m.part("wire", None, (0, 0, 0), kind="root")
    bay_parts(m, root, "coils")
    for N in range(1, 5):
        picket_part(m, root, N)
    r_wire = 0.006 if fine else 0.012
    sides = 5 if fine else 3
    per_turn = 13 if fine else 7
    R = 0.45
    coils = [coil(0.43, R * 0.92, R, -5.0, 5.0, 11, per_turn, squash=0.95, floor=0.0),
             coil(-0.43, R * 0.92, R, -5.0, 5.0, 23, per_turn, squash=0.95, floor=0.0),
             coil(0.0, 1.05, R * 0.98, -5.0, 5.0, 37, per_turn, squash=0.93)]
    top_z = 1.5
    # strands along the coils' tops (and one low between the bottom coils), sagging a little between pickets
    strands = []
    for cx, cz in ((0.43, R * 0.92 + R * 0.93), (-0.43, R * 0.92 + R * 0.93), (0.0, 1.05 + R * 0.93), (0.0, 0.14)):
        pts = []
        for k in range(81 if fine else 21):
            y = -5.0 + 10.0 * k / (80 if fine else 20)
            # the span it lies in, and how far along
            for N in range(3):
                if PICKETS[N] <= y <= PICKETS[N + 1]:
                    f = (y - PICKETS[N]) / (PICKETS[N + 1] - PICKETS[N])
            pts.append(Vector((cx + 0.01 * math.sin(y * 2.3), y, cz - 0.05 * math.sin(math.pi * f))))
        strands.append(pts)
    for k, (nm, y0, y1) in enumerate(halves()):
        m.use(nm)
        for c, pts in enumerate(coils):
            run = cut_y(pts, y0, y1)
            wire(m, run, r_wire, "wire", sides)
            barbs(m, run, 0.55, 0.042, "wire", 100 * k + c, r_wire, collar=False)
        for s, pts in enumerate(strands):
            run = cut_y(pts, y0, y1)
            wire(m, run, r_wire * 0.85, "wire", 4 if fine else 3)
            barbs(m, run, 0.2, 0.035, "wire", 500 + 10 * k + s, r_wire, collar=False)
    # the pickets: angle iron at the ends, screw pickets between
    g = rnd(4)
    for N in range(1, 5):
        y = PICKETS[N - 1]
        m.use(f"picket.{N}")
        lean = (g.uniform(-0.04, 0.04), g.uniform(-0.03, 0.03))
        if N in (1, 4):
            angle_picket(m, (0.0, y, 0.0), top_z, 0.05, 0.006, "steel", (-1, 0.3 if N == 1 else -0.3, 0), lean=lean)
        else:
            screw_picket(m, (0.0, y, 0.0), top_z, (0.14, 0.86), "steel", lean)
        # tie wires from the picket out to each strand
        for cx, cz in ((0.43, R * 0.92 + R * 0.93 - 0.05), (-0.43, R * 0.92 + R * 0.93 - 0.05), (0.0, 1.05 + R * 0.93), (0.0, 0.14)):
            h = min(cz, top_z - 0.02)
            at = Vector((lean[0] * h / top_z, y + lean[1] * h / top_z, h))
            if fine:
                wrap(m, at, Z, 0.035, 2.5, 0.01, 0.004)
            if abs(cx) > 0.1:
                wire(m, [at, at.lerp(Vector((cx, y, cz)), 0.5) + Z * 0.02, Vector((cx, y, cz))], 0.004 if fine else 0.008, "wire", 4 if fine else 3)
    # what lies about: a reel at the rear, a spare picket in the grass, offcuts
    m.use(root)
    reel(m, (-1.6, 3.4, 0.0), 25, seed=3)
    bits.angle_iron(m, (-1.3, -2.6, 0.03), (-1.55, -1.15, 0.05), 0.05, 0.006, "steel", (0.0, 0.3, 1.0))
    if fine:
        g = rnd(9)
        for c, (cx, cy) in enumerate(((-1.0, -3.6), (-1.25, 1.2), (1.2, -0.4))):
            n = 22
            pts = [Vector((cx + math.cos(k * 0.55) * 0.16 * (1 + 0.3 * math.sin(k)), cy + k * 0.02, 0.012 + 0.11 * abs(math.sin(k * 0.55)) * (1 - k / n))) for k in range(n)]
            wire(m, pts, 0.004, "wire", 4)
            barbs(m, pts, 0.12, 0.03, "wire", 900 + c, 0.004)
    trod(m, -1.6, 3.4, 0.55)
    for N, y in enumerate(PICKETS):
        trod(m, 0.0, y, 0.32)
    m.socket("hit", (0.5, 0.0, 0.8), (1, 0, 0), part=root, note="where a shell strikes")
    wire_clips(m)
    weather(m, splash=0.25, rust=0.8)
    m.info["span"] = 8.5
    m.info["high"] = 0.7
    m.info.setdefault("worn", []).append((-1.6, 3.4, 0.9))
    patchy(m, 5)
    return m


# ---- 2. double-apron fence ------------------------------------------------------------------------------

def WireFence(fine=True, nation="_default", name="WireFence"):
    """A double-apron fence: a line of long pickets with four barbed strands, diagonal wires from their tops to
    short anchor pickets in front and behind, and barbed strands along both aprons."""
    m = bits.model(name, "Barbed wire, double-apron fence, a length (our own model)", "works", nation, fine)
    root = m.part("fence", None, (0, 0, 0), kind="root")
    bay_parts(m, root, "strands")
    for N in range(1, 5):
        picket_part(m, root, N)
    r_wire = 0.006 if fine else 0.012
    sides = 5 if fine else 3
    H, A, AZ = 1.45, 2.9, 0.26             # the long pickets' tops, how far out the anchors stand, their tops
    heights = (0.3, 0.68, 1.06, 1.42)
    nseg = 41 if fine else 13

    def run(fx, fz, sag):
        pts = []
        for k in range(nseg):
            y = -5.0 + 10.0 * k / (nseg - 1)
            for N in range(3):
                if PICKETS[N] <= y <= PICKETS[N + 1]:
                    f = (y - PICKETS[N]) / (PICKETS[N + 1] - PICKETS[N])
            pts.append(Vector((fx + 0.006 * math.sin(y * 3.1), y, fz - sag * math.sin(math.pi * f))))
        return pts
    lines = [run(0.0, h, 0.025) for h in heights]
    for side in (1, -1):
        for f in (0.22, 0.47, 0.73):
            lines.append(run(side * A * f, H + (AZ - H) * f - 0.02, 0.05))
        lines.append(run(side * A, AZ - 0.04, 0.07))              # along the anchors' heads
    owner = {1: "bay.1.a", 2: "bay.1.b", 3: "bay.2.b", 4: "bay.3.b"}
    for k, (nm, y0, y1) in enumerate(halves()):
        m.use(nm)
        for s, pts in enumerate(lines):
            seg = cut_y(pts, y0, y1)
            wire(m, seg, r_wire, "wire", sides)
            barbs(m, seg, 0.18 if s < 4 else 0.22, 0.038, "wire", 40 * k + s, r_wire)
    g = rnd(17)
    for N in range(1, 5):
        y = PICKETS[N - 1]
        lean = (g.uniform(-0.03, 0.03), g.uniform(-0.025, 0.025))
        top = Vector((lean[0], y + lean[1], H))
        # the diagonal wires and their ties ride the bay beside the picket (they flatten with it)
        m.use(owner[N])
        for side in (1, -1):
            anchor = Vector((side * A, y, AZ - 0.03))
            pts = [top.lerp(anchor, t) - Z * 0.03 * math.sin(math.pi * t) for t in (0, 0.25, 0.5, 0.75, 1.0)]
            wire(m, pts, r_wire, "wire", sides)
            barbs(m, pts, 0.16, 0.038, "wire", 700 + 2 * N + side, r_wire)
            if fine:
                d = (anchor - top).normalized()
                for f in (0.22, 0.47, 0.73):
                    wrap(m, top.lerp(anchor, f) - Z * 0.03 * math.sin(math.pi * f), d, 0.016, 2.0, 0.01, 0.0035)
        m.use(f"picket.{N}")
        angle_picket(m, (0.0, y, 0.0), H + 0.05, 0.05, 0.006, "steel", (-1, 0.25 * (1 if N % 2 else -1), 0), lean=lean)
        if fine:
            for h in heights:
                wrap(m, (lean[0] * h / H + 0.012, y + lean[1] * h / H, h), Z, 0.04, 2.5, 0.01, 0.004)
        # the short anchor pickets: stakes driven leaning outward, the wire hitched round their heads
        m.use(root)
        for side in (1, -1):
            foot = Vector((side * (A - 0.12), y + g.uniform(-0.05, 0.05), -0.25))
            head = Vector((side * A, y, AZ))
            bits.log(m, foot, head, 0.035, "timber", seed=100 + N * 3 + side, sides=8 if fine else 5)
            if fine:
                wrap(m, head - Z * 0.04, (head - foot).normalized(), 0.04, 2.5, 0.012, 0.004)
            trod(m, head.x, y, 0.16)
        trod(m, 0.0, y, 0.28)
    m.use(root)
    reel(m, (-3.9, 2.2, 0.0), -15, seed=5)
    bits.angle_iron(m, (-3.6, -1.8, 0.03), (-3.4, -0.3, 0.04), 0.05, 0.006, "steel", (0.0, 0.2, 1.0))
    bits.log(m, (-3.75, -2.7, 0.04), (-3.3, -2.9, 0.035), 0.035, "timber", seed=7)
    bits.log(m, (-3.8, -2.5, 0.04), (-3.32, -2.62, 0.04), 0.035, "timber", seed=8)
    trod(m, -3.85, 2.2, 0.6)
    m.socket("hit", (0.5, 0.0, 0.8), (1, 0, 0), part=root, note="where a shell strikes")
    wire_clips(m, spread=1.0)
    weather(m, splash=0.25, rust=0.8)
    m.info["span"] = 8.0
    m.info["high"] = 0.6
    m.info.setdefault("worn", []).append((-3.8, 2.2, 1.0))
    patchy(m, 9)
    return m

# ---- 3. dragon's teeth ------------------------------------------------------------------------------------

SLAB_TOP = 0.02                             # the footing: its top just at the soil, its edges buried
ROWS_X = (1.75, 0.6, -0.6, -1.75)          # front (the enemy's side) to rear
ROW_H = (0.72, 0.98, 1.22, 1.46)           # how high each row stands
PITCH = 1.25


def row_ys(r):
    """Where the teeth of a row stand: each row half a pitch along from the one in front (the odd rows' last tooth
    stands on the length's end, so lengths join without a doubled tooth)."""
    if r % 2 == 0:
        return [-5.0 + PITCH / 2 + PITCH * c for c in range(8)]
    return [-5.0 + PITCH + PITCH * c for c in range(8)]


def in_lane(y):
    return abs(y) < 1.5


def tuft(m, at, h, paint, g, blades=None):
    """A tuft of grass or a weed: thin blades fanning out of one root."""
    at = Vector(at)
    n = blades or (7 if m.fine else 3)
    verts, faces = [], []
    for k in range(n):
        a = g.uniform(0, 2 * math.pi)
        lean = g.uniform(0.15, 0.6)
        d = Vector((math.cos(a) * lean, math.sin(a) * lean, 1.0)).normalized()
        side = Vector((-math.sin(a), math.cos(a), 0.0)) * g.uniform(0.006, 0.012)
        root = at + Vector((g.uniform(-0.02, 0.02), g.uniform(-0.02, 0.02), 0.0))
        tip = root + d * h * g.uniform(0.55, 1.1) + Vector((math.cos(a), math.sin(a), 0)) * h * 0.15
        b = len(verts)
        verts += [root - side, root + side, tip]
        faces.append((b, b + 1, b + 2))
    m.add(verts, faces, paint, True, None)


def finish(m, ob, paint, part=None, chamfer=0.03, rough=0.008, seed=0.0, cut=0.26):
    """Chamfer, cut fine, roughen and take a solid in; returns the range of points it became."""
    if m.fine:
        bits.s_bevel(ob, chamfer, 2)
        if cut:
            bits.s_cut_fine(ob, cut)
        if rough:
            bits.s_noise(ob, rough, 0.35, seed)
    else:
        bits.s_bevel(ob, chamfer, 1)
    first = len(m.verts)
    bits.take(m, ob, paint, True, part)
    return first, len(m.verts)


def chunk(g, at, size, tilt=25.0):
    """A broken lump of concrete as a solid."""
    n = g.randint(5, 7)
    plan = []
    for k in range(n):
        a = 2 * math.pi * (k + g.uniform(-0.25, 0.25)) / n
        r = size * g.uniform(0.6, 1.0)
        plan.append((math.cos(a) * r, math.sin(a) * r * g.uniform(0.7, 1.0)))
    h = size * g.uniform(0.6, 1.1)
    top = [(p[0] * g.uniform(0.45, 0.8), p[1] * g.uniform(0.45, 0.8)) for p in plan]
    ob = bits.s_prism(plan, 0.0, h, top, (g.uniform(-0.2, 0.2) * size, g.uniform(-0.2, 0.2) * size))
    return bits.s_move(ob, Vector(at) - Z * h * 0.12, g.uniform(0, 360), g.uniform(-tilt, tilt), g.uniform(-tilt, tilt))


def DragonsTeeth(fine=True, nation="_default", name="DragonsTeeth"):
    """Four staggered rows of cast teeth rising from the enemy's side to the rear, on a common footing sunk under the
    soil: only a little concrete shows between them where the earth and turf have not grown over it."""
    m = bits.model(name, "Dragon's teeth, a length (our own model)", "works", nation, fine)
    root = m.part("teeth", None, (0, 0, 0), kind="root")
    g = rnd(31)
    m.use(root)
    xa, xb = -2.5, 2.35
    # the footing: cast in four bays, its top at the soil
    for k in range(4):
        y0, y1 = -5.0 + 2.5 * k + (0.01 if k else 0.0), -5.0 + 2.5 * (k + 1) - (0.01 if k < 3 else 0.0)
        ob = bits.s_box((xa, y0, -0.3), (xb, y1, SLAB_TOP + g.uniform(-0.004, 0.004)))
        finish(m, ob, "concrete.old", root, 0.02, 0.006, 3.0 + k, 0.7)
    # earth and turf over it: thick and banked at both long edges (no edge of the footing shows), thinning inward
    # and broken, so the concrete shows through in ragged places between the teeth
    step = 0.2 if fine else 0.5
    xs = [xa - 0.7 + step * i for i in range(int((xb - xa + 1.4) / step) + 1)]
    ys = [-5.7 + 11.4 * j / int(11.4 / step) for j in range(int(11.4 / step) + 1)]
    verts, faces = [], []
    for i, x in enumerate(xs):
        for y in ys:
            inside = min(x - xa, xb - x)
            ends = min(y + 5.0, 5.0 - y)          # (below 0 past either end of the length)
            lump = 0.012 * bits.fbm((x * 2.3, y * 2.3, 9.0), 0.5)
            # how far out the cover runs before it is under the field: it wanders, so no edge is straight
            run = 0.42 + 0.26 * bits.fbm((x * 0.45 + 3.0, y * 0.45, 8.0), 1.0)
            if inside < 0.0:
                z = -0.04 + (SLAB_TOP + 0.095 + lump) * smooth(1.0 + inside / run)
            else:
                cover = 0.06 * smooth(1.0 - inside / 1.0) + 0.07 * (bits.fbm((x * 0.7, y * 0.7, 4.0), 1.0) + 0.12 * bits.fbm((x * 3.0, y * 3.0, 6.0), 0.5) + 0.015)
                cover = max(cover, 0.045 * smooth(1.0 - inside / 0.35))
                z = SLAB_TOP + (cover + lump if cover > 0.006 else -0.015)
            if ends < 0.0:
                # past the ends the cover runs out under the field too (or under the next length's, where lengths join)
                z = -0.04 + (max(z, SLAB_TOP + 0.03) + 0.04) * smooth(1.0 + ends / run)
            verts.append(Vector((x + 0.03 * bits.fbm((x, y * 1.7, 2.0), 0.4), y, z)))
    ny = len(ys)
    for i in range(len(xs) - 1):
        for j in range(ny - 1):
            faces.append((i * ny + j, (i + 1) * ny + j, (i + 1) * ny + j + 1, i * ny + j + 1))
    bits.turfed(m, verts, faces, 0.5, "earth", "turf", seed=2.0, part=root, low=SLAB_TOP + 0.025, scale=1.1)
    chips = {(0, 1), (2, 6), (1, 0), (3, 2)}
    lane, bases = {}, []
    for r, x0 in enumerate(ROWS_X):
        for c, y0 in enumerate(row_ys(r)):
            h = ROW_H[r] * g.uniform(0.96, 1.03)
            b = (0.66 + 0.34 * r / 3.0) * g.uniform(0.95, 1.04)
            t = b * g.uniform(0.27, 0.35)
            off = (g.uniform(-0.035, 0.035), g.uniform(-0.035, 0.035))
            plan = [(-b / 2, -b / 2), (b / 2, -b / 2), (b / 2, b / 2), (-b / 2, b / 2)]
            top = [(p[0] * t / b + off[0], p[1] * t / b + off[1]) for p in plan]
            ob = bits.s_prism(plan, SLAB_TOP - 0.05, h, top)
            if (r, c) in chips and fine and not in_lane(y0):
                cut = bits.s_box((-0.16, -0.16, -0.16), (0.16, 0.16, 0.16))
                corner = g.choice([(1, 1), (1, -1), (-1, 1)])
                bits.s_move(cut, (corner[0] * (t / 2 + 0.07) + off[0], corner[1] * (t / 2 + 0.07) + off[1], h - 0.02), g.uniform(20, 70), g.uniform(15, 40), g.uniform(-20, 20))
                bits.s_cut(ob, cut)
                if r % 2:
                    cut2 = bits.s_box((-0.1, -0.1, -0.1), (0.1, 0.1, 0.1))
                    bits.s_move(cut2, (-corner[0] * b / 2, corner[1] * b / 2, SLAB_TOP + 0.14), 45, 20, 10)
                    bits.s_cut(ob, cut2)
            pos = (x0 + g.uniform(-0.04, 0.04), y0 + g.uniform(-0.04, 0.04), 0.0)
            bits.s_move(ob, pos, g.uniform(-4, 4))
            bases.append((pos, b))
            paint = "concrete" if g.random() < 0.6 else "concrete.old"
            if in_lane(y0):
                nm = f"tooth.{r + 1}.{c + 1}"
                m.part(nm, root, (pos[0], pos[1], SLAB_TOP), kind="free", note="a tooth of the lane the engineers blow: thrown over and broken by `breach`")
                lane[nm] = (r, y0, finish(m, ob, paint, nm, 0.035, 0.003, r * 8 + c + 0.5, 0.5), Vector((pos[0], pos[1], SLAB_TOP)), b, h)
            else:
                finish(m, ob, paint, root, 0.035, 0.003, r * 8 + c + 0.5, 0.5)
    m.use(root)
    # old chips lying at the teeth's feet
    for k in range(9 if fine else 3):
        x = g.uniform(-2.2, 2.2)
        y = g.choice([-4.0, -3.0, -1.9, 1.9, 3.0, 4.0]) + g.uniform(-0.3, 0.3)
        finish(m, chunk(g, (x, y, SLAB_TOP + 0.02), g.uniform(0.04, 0.09)), "concrete.old", root, 0.008, 0.006, 40 + k, 0.0)
    # grass and weeds grown up between the teeth: thick along both edges, round the teeth's feet, sparse in between
    for k in range(260 if fine else 24):
        u = g.random()
        if u < 0.45:
            x = g.choice([xa, xb]) + g.uniform(-0.3, 0.45)
            y = g.uniform(-5.0, 5.0)
        elif u < 0.85:
            (px, py, _), b = g.choice(bases)
            a = g.uniform(0, 2 * math.pi)
            x, y = px + math.cos(a) * b * 0.55, py + math.sin(a) * b * 0.55
        else:
            x, y = g.uniform(xa, xb), g.uniform(-5.0, 5.0)
        weed = g.random() < 0.18
        tuft(m, (x, y, SLAB_TOP + 0.01), g.uniform(0.28, 0.48) if weed else g.uniform(0.1, 0.24), "garnish.2" if weed or g.random() < 0.25 else "turf", g)
    # -- the rubble of the blown lane: hidden until the breach
    m.part("rubble.1", root, (0.0, 0.0, SLAB_TOP), kind="free", note="the broken stumps of the lane's teeth: hidden (scale 0) until `breach`")
    m.part("rubble.2", root, (0.0, 0.0, 0.5), kind="free", note="broken concrete thrown about the lane: hidden (scale 0) until `breach`")
    m.part("rubble.3", root, (0.0, 0.0, 0.5), kind="free", note="more of it, further out: hidden (scale 0) until `breach`")
    m.use("rubble.1")
    for nm, (r, y0, rng, piv, b, h) in lane.items():
        lo = [piv + Vector((sx * b * 0.47, sy * b * 0.47, -0.04)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        hi = [piv + Vector((sx * b * g.uniform(0.3, 0.4), sy * b * g.uniform(0.3, 0.4), g.uniform(0.12, 0.34))) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        finish(m, bits.s_wedge(lo, hi), "concrete", "rubble.1", 0.02, 0.03, 90 + r * 3 + y0, 0.1)
        for k in range(3 if fine else 1):
            a = piv + Vector((g.uniform(-0.15, 0.15), g.uniform(-0.15, 0.15), 0.22))
            d = Vector((g.uniform(-0.6, 0.6), g.uniform(-0.6, 0.6), 1.0)).normalized()
            bits.tube(m, [a - d * 0.1, a + d * 0.15, a + d * 0.25 + Vector((g.uniform(-0.1, 0.1), g.uniform(-0.1, 0.1), -0.05))], 0.009, "rust", 6 if fine else 3)
    for part, n, rad in (("rubble.2", 12, (0.3, 1.8)), ("rubble.3", 10, (1.6, 3.6))):
        m.use(part)
        for k in range(n):
            a = g.uniform(0, 2 * math.pi)
            d = g.uniform(*rad)
            x, y = math.cos(a) * d * 1.3, math.sin(a) * d * 0.6
            finish(m, chunk(g, (x, y, SLAB_TOP + 0.02), g.uniform(0.09, 0.28)), g.choice(["concrete", "concrete.old"]), part, 0.012, 0.012, 120 + k, 0.0)
    # -- how the lane's teeth fall: the front row thrown toward the enemy, the rear row back, the middle two tipped over
    tracks = []
    for nm, (r, y0, (v0, v1), piv, b, h) in lane.items():
        if abs(y0) < 0.1:
            out = g.choice([-1.0, 1.0]) * 0.5
        elif abs(y0) > 1.0:
            out = -math.copysign(0.6, y0)                 # (the outer ones fall a little inward, clear of their neighbours)
        else:
            out = math.copysign(1.0, y0)
        ax = Y.copy() if r in (0, 1) else -Y
        alpha = math.degrees(math.atan2((b - b * 0.31) / 2, h - SLAB_TOP))
        ang = 90.0 + alpha + g.uniform(-3, 9)
        q = Quaternion(Z, math.radians(g.uniform(-20, 20))) @ Quaternion(ax, math.radians(ang))
        sc = Vector((0.9, 0.9, 0.76))
        if r == 0:
            dxy = Vector((g.uniform(2.0, 2.6), out * g.uniform(0.2, 0.5), 0))
        elif r == 3:
            dxy = Vector((-g.uniform(2.0, 2.6), out * g.uniform(0.2, 0.5), 0))
        else:
            dxy = Vector(((0.3 if r == 1 else -0.3), out * g.uniform(0.05, 0.2), 0))
        low = min((q @ Vector(((m.verts[i] - piv).x * sc.x, (m.verts[i] - piv).y * sc.y, (m.verts[i] - piv).z * sc.z))).z for i in range(v0, v1))
        mz = SLAB_TOP + 0.01 - (piv.z + low)
        apex = g.uniform(1.4, 2.0) if r in (0, 3) else g.uniform(0.35, 0.6)
        q2 = Quaternion().slerp(q, 0.55)
        qb = Quaternion(ax, math.radians(-4.0)) @ q
        tracks.append({"part": nm,
                       "rot": [[0.0, 1, 0, 0, 0], [0.05, 1, 0, 0, 0], [0.32, q2.w, q2.x, q2.y, q2.z], [0.62, q.w, q.x, q.y, q.z], [0.72, qb.w, qb.x, qb.y, qb.z], [0.85, q.w, q.x, q.y, q.z]],
                       "move": [[0.0, 0, 0, 0], [0.05, 0, 0, 0.12], [0.32, dxy.x * 0.6, dxy.y * 0.6, apex], [0.62, dxy.x, dxy.y, mz], [0.72, dxy.x * 1.02, dxy.y * 1.02, mz + 0.07], [0.85, dxy.x * 1.03, dxy.y * 1.03, mz]],
                       "scale": [[0.0, 1.0, 1.0, 1.0], [0.04, sc.x, sc.y, sc.z]]})
    tracks += [{"part": "rubble.1", "scale": [[0.0, 0.0], [0.03, 0.0], [0.06, 1.0]]},
               {"part": "rubble.2", "scale": [[0.0, 0.0], [0.04, 0.0], [0.12, 0.55], [0.55, 1.0]], "move": [[0.0, 0, 0, 0], [0.04, 0, 0, 0.8], [0.3, 0, 0, 0.9], [0.55, 0, 0, 0.0]]},
               {"part": "rubble.3", "scale": [[0.0, 0.0], [0.06, 0.0], [0.15, 0.45], [0.75, 1.0]], "move": [[0.0, 0, 0, 0], [0.06, 0, 0, 1.0], [0.4, 0, 0, 1.4], [0.75, 0, 0, 0.0]]}]
    m.clips["breach"] = {"long": 2.0, "loop": False, "after": "hold",
                         "about": "Engineers blow a lane about 3 m wide through the middle: the lane's teeth are thrown over and broken, stumps and broken concrete left in their place.",
                         "tracks": tracks,
                         "events": [{"t": 0.0, "fx": "blast", "socket": "gap", "cue": "explosion_debris"},
                                    {"t": 0.6, "fx": "dust", "socket": "gap", "cue": ""}]}
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing; the rubble hidden.",
                       "tracks": [{"part": p, "scale": [[0.0, 0.0]]} for p in ("rubble.1", "rubble.2", "rubble.3")], "events": []}
    m.socket("gap", (0.0, 0.0, 0.5), (1, 0, 0), part=root, note="the middle of the lane the engineers blow")
    m.socket("hit", (2.0, 0.0, 0.5), (1, 0, 0), part=root, note="where a shell strikes")
    weather(m, splash=0.55)
    m.info["span"] = 9.0
    m.info["high"] = 0.6
    # (nobody walks along a belt of teeth: no path beside it, only bare patches here and there where the cover is thin.
    #  The field's grass stands right up onto the cover's banked edges; it is kept off the footing alone)
    gw = rnd(77)
    m.info.setdefault("worn", []).extend([(gw.choice([xa - 0.1, xb + 0.1]) + gw.uniform(-0.3, 0.3), gw.uniform(-4.6, 4.6), gw.uniform(0.45, 0.8), gw.uniform(0.35, 0.6)) for _ in range(6)])
    m.info["clear"] = [[(xb - 0.3 + 0.15 * bits.fbm((0, y, 1.0), 0.7), y) for y in [-4.8 + 0.48 * k for k in range(21)]] +
                       [(xa + 0.3 + 0.15 * bits.fbm((0, y, 2.0), 0.7), y) for y in [4.8 - 0.48 * k for k in range(21)]]]
    return m


# ---- 4. steel hedgehogs ---------------------------------------------------------------------------------

def angle_bar(m, a, b, leg, thick, paint, open_to, seed=0, bend=0.0, edge=None):
    """Rolled angle: an L with a filleted root and eased toes, a little bent and dented along its length."""
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    d = (b - a) / L
    o = Vector(open_to) - d * Vector(open_to).dot(d)
    o.normalize()
    p = d.cross(o)
    l1, l2 = (o + p).normalized(), (o - p).normalized()
    t = thick
    if m.fine:
        fr = 0.006
        sec = [(0.0, 0.0), (leg - 0.003, 0.0), (leg, 0.003), (leg, t - 0.002), (leg - 0.002, t)]
        for k in range(5):
            ang = math.pi * 1.5 - math.pi / 2 * k / 4
            sec.append((t + fr + math.cos(ang) * fr, t + fr + math.sin(ang) * fr))
        sec += [(t, leg - 0.002), (t - 0.002, leg), (0.003, leg), (0.0, leg - 0.003)]
        root_i = 7
        toes = {1, 2, 3, 10, 11, 12}
    else:
        toes = {1, 4}
        sec = [(0.0, 0.0), (leg, 0.0), (leg, t), (t, t), (t, leg), (0.0, leg)]
        root_i = 3
    n = len(sec)
    g = rnd(seed)
    nodes = 10 if m.fine else 2
    bend_dir = (l1 + l2).normalized()
    ph = g.uniform(0, 6)
    rings = []
    for i in range(nodes):
        f = i / (nodes - 1)
        c = a + d * (L * f) + bend_dir * (bend * math.sin(math.pi * f)) + l1 * (0.002 * math.sin(7 * f + ph)) * (m.fine)
        rings.append([c + l1 * s1 + l2 * s2 for s1, s2 in sec])
    verts = [q for ring in rings for q in ring]
    for k in range(n):
        k1 = (k + 1) % n
        e = (sec[k1][0] - sec[k][0], sec[k1][1] - sec[k][1])
        nn = (l1 * e[1] - l2 * e[0]).normalized()
        m.add(verts, [(i * n + k, i * n + k1, (i + 1) * n + k1, (i + 1) * n + k) for i in range(nodes - 1)], edge if (edge and k in toes) else paint, True, lambda q, w=nn: w)
    for ring, way in ((rings[0], -d), (rings[-1], d)):
        m.add(ring, [(root_i, k, (k + 1) % n) for k in range(n) if root_i not in (k, (k + 1) % n)], edge or paint, False, lambda q, w=way: w)
    return l1, l2


def rubbed(mid, n):
    """The edges of the legs and the cut ends: bright where they have been rubbed, rusted dark where not."""
    return "steel.worn" if bits.fbm(mid * 3.7, 0.5) > -0.15 else "rust"


def rust_patches(m, much=0.85, scale=0.32):
    """Over the weather: rust breaking through the dark steel in soft-edged patches (point by point)."""
    base = m.weather

    def f(p, n, paint, part):
        w, s_ = base(p, n, paint, part)
        if paint == "iron":
            v = bits.fbm(Vector(p) + Vector((3.3, 1.7, 0.0)), scale) + 0.35 * bits.fbm(Vector(p) * 3.0, scale)
            w = max(w, much * smooth((v + 0.05) / 0.3))
        return w, s_
    m.weather = f


def rivet(m, at, facing, r=0.011):
    if m.fine:
        m.lathe(at, facing, [(0.0, r), (0.004, r * 0.85), (0.007, r * 0.45)], "iron", 6, (False, True))


HH_ARM = 0.95


def hedgehog(m, at, yaw=0.0, seed=0, wired=False):
    """One steel hedgehog: three angles crossing square at their middles, gusseted and bolted, on little feet.
    Returns its middle and its six ends."""
    at = Vector(at)
    g = rnd(seed)
    q = Quaternion(Z, math.radians(yaw)) @ Vector((1, 1, 1)).normalized().rotation_difference(Z)
    dirs = [q @ X, q @ Y, q @ Z]
    foot = 0.012
    C = at + Z * (HH_ARM / math.sqrt(3) + foot)
    leg, thick = 0.12, 0.012
    for i in range(3):
        di, dj, dk = dirs[i], dirs[(i + 1) % 3], dirs[(i + 2) % 3]
        L = HH_ARM * g.uniform(0.98, 1.02)
        angle_bar(m, C - di * L, C + di * L, leg, thick, "iron", -(dj + dk), seed * 7 + i, g.uniform(0.0, 0.006), rubbed)
    # gussets where each pair crosses, riveted to both in rows
    for i in range(3):
        di, dj, dk = dirs[i], dirs[(i + 1) % 3], dirs[(i + 2) % 3]
        out = [(0.0, 0.0), (-0.36, 0.0), (-0.36, -leg), (-leg, -0.36), (0.0, -0.36)]
        bits.plate(m, [(a_ + 0.004, b_ + 0.004) for a_, b_ in out], 0.01, "iron", C + dk * 0.006, di, dj)
        pts = [(-0.035, -0.035), (-0.085, -0.035), (-0.035, -0.085), (-0.085, -0.085)]
        for row in (-0.035, -0.085):
            for along in (-0.16, -0.23, -0.3):
                pts += [(row, along), (along, row)]
        for a_, b_ in pts:
            rivet(m, C + di * a_ + dj * b_ + dk * 0.011, dk)
    # the feet: a small plate welded under each low end
    ends = []
    for i in range(3):
        for sgn in (-1, 1):
            e = C + dirs[i] * HH_ARM * sgn
            ends.append(e)
            if sgn < 0:
                c = Vector((e.x, e.y, 0.0))
                u = Vector((dirs[i].x, dirs[i].y, 0)).normalized()
                v = Z.cross(u)
                bits.plate(m, [(-0.11, -0.09), (0.09, -0.09), (0.09, 0.09), (-0.11, 0.09)], foot, "iron", c + Z * foot, u, v, centred=False)
                if m.fine:
                    # the weld: a bead round where the angle meets the plate
                    bead = [c + Z * (foot + 0.006) + (u * math.cos(2 * math.pi * k / 9) + v * math.sin(2 * math.pi * k / 9)) * 0.075 for k in range(10)]
                    wire(m, bead, 0.007, "iron", 4)
                    for sx in (-1, 1):
                        rivet(m, c + u * 0.065 + v * 0.06 * sx + Z * foot, Z)
    if wired and m.fine:
        # a few turns of barbed wire wound round an upper arm, its end trailing to the ground
        top = max(range(3), key=lambda i: dirs[i].z)
        dt = dirs[top]
        side = dt.cross(Z).normalized()
        upv = dt.cross(side)
        pts = []
        for k in range(60):
            f = k / 59
            ang = f * 2 * math.pi * 4.5
            pts.append(C + dt * (0.35 + 0.3 * f) + (side * math.cos(ang) + upv * math.sin(ang)) * (0.1 + 0.01 * math.sin(k)))
        last = pts[-1]
        for k in range(1, 9):
            f = k / 8
            pts.append(last.lerp(Vector((last.x + 0.6, last.y - 0.25, 0.015)), f) + Z * 0.15 * math.sin(math.pi * f))
        wire(m, pts, 0.005, "wire", 5)
        barbs(m, pts, 0.12, 0.035, "wire", seed + 50, 0.005)
    return C, ends


def Hedgehog(fine=True, nation="_default", name="Hedgehog"):
    m = bits.model(name, "Steel hedgehog (our own model)", "works", nation, fine)
    root = m.part("hedgehog", None, (0, 0, 0), kind="root")
    m.use(root)
    hedgehog(m, (0, 0, 0), 12.0, seed=1, wired=True)
    trod(m, 0.0, 0.0, 0.85)
    m.socket("hit", (0.0, 0.0, 0.6), (1, 0, 0), part=root, note="where a shell strikes")
    weather(m, splash=0.3, rust=0.75)
    rust_patches(m)
    m.info["span"] = 2.4
    m.info["high"] = 0.55
    return m


def Hedgehogs(fine=True, nation="_default", name="Hedgehogs"):
    """A 10 m row of five hedgehogs, each turned its own way; a tank can shove any one of them over."""
    m = bits.model(name, "Steel hedgehogs, a row of five (our own model)", "works", nation, fine)
    root = m.part("row", None, (0, 0, 0), kind="root")
    g = rnd(77)
    m.use(root)
    for k in range(5):
        y = -4.0 + 2.0 * k + g.uniform(-0.15, 0.15)
        x = g.uniform(-0.25, 0.25)
        nm = f"hedgehog.{k + 1}"
        piv = Vector((x, y, HH_ARM / math.sqrt(3) + 0.012))
        m.part(nm, root, piv, kind="free", note="a hedgehog: `shove." + str(k + 1) + "` rolls it a quarter turn and slides it back")
        m.use(nm)
        v0 = len(m.verts)
        hedgehog(m, (x, y, 0.0), g.uniform(0, 120), seed=10 + k, wired=(k == 2))
        v1 = len(m.verts)
        m.use(root)
        trod(m, x, y, 0.8)
        m.socket(nm, piv, (1, 0, 0), part=nm, note="the hedgehog's middle")
        # the shove: rolled a quarter turn toward -x about its middle, kept on the ground, and slid back about a metre
        ax = Vector((g.uniform(-0.2, 0.2), -1.0, 0.0)).normalized()
        keys_r, keys_m = [], []
        steps = [(0.0, 0.0, 0.0), (0.3, 0.0, 0.0), (0.7, 30.0, 0.25), (1.1, 60.0, 0.55), (1.5, 90.0, 0.85), (1.9, 90.0, 1.0)]
        for t, deg, frac in steps:
            qq = Quaternion(ax, math.radians(deg))
            low = min((qq @ (m.verts[i] - piv)).z for i in range(v0, v1)) + piv.z
            keys_r.append([t, qq.w, qq.x, qq.y, qq.z])
            keys_m.append([t, -1.0 * frac, g.uniform(-0.05, 0.05) * frac, -low + (0.0 if deg == 0 else -0.01)])
        keys_m[0][3] = keys_m[1][3] = 0.0
        m.clips[f"shove.{k + 1}"] = {"long": 1.9, "loop": False, "after": "hold",
                                     "about": f"A tank pushes hedgehog {k + 1} over from the enemy's side: it rolls a quarter turn and slides about a metre back.",
                                     "tracks": [{"part": nm, "rot": keys_r, "move": keys_m}],
                                     "events": [{"t": 0.3, "fx": "dust", "socket": nm, "cue": ""}]}
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing.", "tracks": [], "events": []}
    m.socket("hit", (0.3, 0.0, 0.6), (1, 0, 0), part=root, note="where a shell strikes")
    weather(m, splash=0.3, rust=0.75)
    rust_patches(m)
    m.info["span"] = 8.5
    m.info["high"] = 0.55
    return m


# ---- 5. a marked minefield -----------------------------------------------------------------------------

def mound(m, cx, cy, R, H, paint="earth", seed=0, hole=0.0, base=0.004):
    """A low mound of turned earth (with a hole in its middle for a half-dug mine). Returns its points' range."""
    g = rnd(seed)
    n = 18 if m.fine else 8
    nr = 4 if m.fine else 2
    v0 = len(m.verts)
    el, tw = (0.0 if hole else g.uniform(0.08, 0.25)), g.uniform(0, math.pi)
    rings = []
    for j in range(nr + 1):
        f = j / nr
        r = hole + (R - hole) * f if hole else R * (0.12 + 0.88 * f)
        h = (H * (1.0 - f) ** 1.3) if hole else H * (1.0 - f * f)
        ring = []
        for k in range(n):
            a = 2 * math.pi * k / n
            rr = r * (1.0 + (0.3 * bits.fbm((math.cos(a) * 1.4 + seed * 0.37, math.sin(a) * 1.4, f * 0.5), 0.5)
                             + 0.14 * bits.fbm((math.cos(a) * 3.5, math.sin(a) * 3.5 + seed * 0.37, 2.0), 0.4)) * (0.4 + 0.6 * f))
            rr *= 1.0 + el * math.cos(2 * (a - tw))
            ring.append(Vector((cx + math.cos(a) * rr, cy + math.sin(a) * rr, base + max(0.0, h + 0.012 * bits.fbm((cx + math.cos(a) * rr * 3, cy + math.sin(a) * rr * 3, seed), 0.3) * (1 - f)))))
        rings.append(ring)
    if hole:
        rings.insert(0, [Vector((cx + (q.x - cx) * 0.9, cy + (q.y - cy) * 0.9, -0.03)) for q in rings[0]])
    verts = [q for ring in rings for q in ring]
    faces = [(j * n + k, j * n + (k + 1) % n, (j + 1) * n + (k + 1) % n, (j + 1) * n + k) for j in range(len(rings) - 1) for k in range(n)]
    m.add(verts, faces, paint, True, lambda q: Z)
    if not hole:
        top = Vector((cx, cy, base + H))
        m.add(rings[0] + [top], [(k, (k + 1) % n, n) for k in range(n)], paint, True, lambda q: Z)
    return v0, len(m.verts)


def plate_mine(m, at, up=Z, yaw=0.0, paint="upper"):
    """A flat round anti-tank plate mine, 0.32 m across: a ribbed body, the pressure plate standing in its middle,
    a plug in the side and a folding carrying handle."""
    at, up = Vector(at), Vector(up).normalized()
    sides = 26 if m.fine else 10
    prof = [(0.0, 0.148), (0.006, 0.156), (0.012, 0.16), (0.07, 0.16), (0.078, 0.157), (0.083, 0.15), (0.087, 0.125)]
    if m.fine:
        prof += [(0.089, 0.118), (0.087, 0.112), (0.089, 0.105), (0.087, 0.099), (0.089, 0.092)]
        prof += [(0.09, 0.08), (0.094, 0.077), (0.1, 0.077), (0.103, 0.072), (0.106, 0.065), (0.112, 0.064), (0.113, 0.04), (0.118, 0.032), (0.119, 0.016), (0.119, 0.0)]
    else:
        prof = [(0.0, 0.15), (0.08, 0.16), (0.09, 0.08), (0.112, 0.065), (0.119, 0.0)]
    m.lathe(at, up, prof, paint, sides, (True, True))
    side = up.cross(Z) if abs(up.z) < 0.99 else X.copy()
    side = Quaternion(up, math.radians(yaw)) @ side.normalized()
    w = up.cross(side)
    # the side plug for the second fuze
    m.lathe(at + up * 0.038 + side * 0.155, side, [(0.0, 0.022), (0.018, 0.022), (0.022, 0.019), (0.03, 0.019)], "steel", 10 if m.fine else 5)
    # the carrying handle, folded down against the side, and its two lugs
    if m.fine:
        n = 9
        pts = []
        for k in range(n):
            a = -0.55 + 1.1 * k / (n - 1)
            pts.append(at + up * (0.035 + 0.012 * math.cos(a * 2.6)) + (w * math.cos(a) - side * math.sin(a)) * (0.172 + 0.006 * abs(math.sin(a * 2.0))))
        wire(m, pts, 0.0045, "steel", 5)
        for e in (pts[0], pts[-1]):
            m.box(e - (e - at - up * 0.035).normalized() * 0.01, (0.02, 0.02, 0.02), paint)
    else:
        m.rod(at + up * 0.035 + w * 0.16 - side * 0.06, at + up * 0.035 + w * 0.16 + side * 0.06, 0.008, "steel", sides=3)


def crater(m, cx, cy, R=0.95, seed=0):
    """A mine crater: a ring of thrown earth round a blackened bowl, clods flung about. Points are sooted."""
    g = rnd(seed)
    n = 24 if m.fine else 10
    v0 = len(m.verts)
    prof = [(0.0, 0.016), (0.25, 0.02), (0.45, 0.06), (0.6, 0.17), (0.7, 0.19), (0.85, 0.09), (1.0, 0.03), (1.25, 0.004)] if m.fine else [(0.0, 0.006), (0.45, 0.06), (0.65, 0.18), (1.0, 0.03), (1.25, 0.004)]
    rings = []
    for f, h in prof:
        ring = []
        for k in range(n):
            a = 2 * math.pi * k / n
            r = R * f * (1.0 + 0.26 * bits.fbm((math.cos(a) * 1.5 + seed, math.sin(a) * 1.5, f), 0.5))
            ring.append(Vector((cx + math.cos(a) * r, cy + math.sin(a) * r, max(0.004, h * (1.0 + 0.8 * bits.fbm((cx + math.cos(a) * 2.5, cy + math.sin(a) * 2.5, seed + f), 0.35))))))
        rings.append(ring)
    verts = [q for ring in rings for q in ring]
    faces = [(j * n + k, j * n + (k + 1) % n, (j + 1) * n + (k + 1) % n, (j + 1) * n + k) for j in range(len(rings) - 1) for k in range(n)]
    m.add(verts, faces, lambda mid, nn: "earth.dry" if (Vector((mid.x - cx, mid.y - cy, 0))).length > R * 0.5 else "earth", True, lambda q: Z)
    for k in range(10 if m.fine else 3):
        a, d = g.uniform(0, 2 * math.pi), R * g.uniform(1.1, 1.9)
        m.ball((cx + math.cos(a) * d, cy + math.sin(a) * d, 0.02), (g.uniform(0.04, 0.09), g.uniform(0.04, 0.08), g.uniform(0.03, 0.05)), "earth", 6 if m.fine else 4, 4)
    for i in range(v0, len(m.verts)):
        dd = Vector((m.verts[i].x - cx, m.verts[i].y - cy, 0)).length
        m.vsoot[i] = 0.85 * smooth(1.0 - dd / (R * 0.75))
    return v0


def Minefield(fine=True, nation="_default", name="Minefield"):
    """A strip 10 m by 6 m of plate mines, most buried, marked on the defender's side and both ends by a low fence of
    thin posts and one strand of plain wire hung with plain pennants."""
    m = bits.model(name, "Minefield, a strip (our own model)", "works", nation, fine)
    root = m.part("field", None, (0, 0, 0), kind="root")
    m.use(root)
    g = rnd(5)
    X0, X1 = -3.0, 3.0
    # -- the marker fence: posts round three sides
    posts = [(X0, -5.0 + 10.0 * k / 6) for k in range(7)]
    left = [(X0 + 6.0 * k / 4, 5.0) for k in range(1, 5)]
    right = [(X0 + 6.0 * k / 4, -5.0) for k in range(1, 5)]
    tops = {}
    for k, (x, y) in enumerate(posts + left + right):
        lean = Vector((g.uniform(-0.04, 0.04), g.uniform(-0.04, 0.04), 0))
        top = Vector((x, y, 0.6)) + lean
        bits.log(m, (x, y, -0.15), top, 0.022, "timber", seed=200 + k, sides=7 if fine else 4, taper=0.85)
        tops[(round(x, 2), round(y, 2))] = top
        trod(m, x, y, 0.1)

    def strand(seq):
        pts = []
        for (xa, ya), (xb, yb) in zip(seq, seq[1:]):
            a, b = tops[(round(xa, 2), round(ya, 2))] - Z * 0.04, tops[(round(xb, 2), round(yb, 2))] - Z * 0.04
            n = 10 if fine else 4
            for j in range(n):
                f = j / n
                pts.append(a.lerp(b, f) - Z * 0.07 * math.sin(math.pi * f))
        pts.append(tops[(round(seq[-1][0], 2), round(seq[-1][1], 2))] - Z * 0.04)
        return pts
    runs = [strand(list(reversed(right)) + [posts[0]]), strand(posts), strand([posts[-1]] + left)]
    for run in runs:
        wire(m, run, 0.004 if fine else 0.01, "wire", 4 if fine else 3)
    for k, p in enumerate(tops.values()):
        if fine:
            wrap(m, p - Z * 0.04, Z, 0.026, 2.0, 0.008, 0.003)
    # pennants: little plain triangles hung point down, one every 0.7 m or so
    count = 0
    for run in runs:
        total = sum((b - a).length for a, b in zip(run, run[1:]))
        s = 0.35
        while s < total - 0.2:
            acc = 0.0
            for a, b in zip(run, run[1:]):
                ln = (b - a).length
                if acc + ln >= s:
                    p = a.lerp(b, (s - acc) / ln)
                    d = (b - a).normalized()
                    break
                acc += ln
            hang = Quaternion(d, math.radians(g.uniform(-18, 18))) @ (-Z)
            bits.plate(m, [(-0.1, 0.0), (0.1, 0.0), (g.uniform(-0.015, 0.015), 0.17)], 0.003, "sign" if count % 2 == 0 else "sign.2", p + hang * 0.006, d, hang)
            if fine:
                # the tab folded over the wire
                bits.plate(m, [(-0.1, 0.0), (0.1, 0.0), (0.1, 0.02), (-0.1, 0.02)], 0.003, "sign" if count % 2 == 0 else "sign.2", p, d, -hang.cross(d))
            count += 1
            s += 0.7 * g.uniform(0.9, 1.1)
    # -- the mines: staggered rows; most buried under a patch or a low mound, four half dug in
    spots = []
    for r, (x, ys) in enumerate(((2.15, (-3.75, -1.25, 1.25, 3.75)), (0.75, (-2.5, 0.0, 2.5)), (-0.65, (-3.75, -1.25, 1.25, 3.75)), (-2.0, (-2.5, 0.0, 2.5)))):
        for y in ys:
            spots.append((x + g.uniform(-0.25, 0.25), y + g.uniform(-0.3, 0.3)))
    shown = {1: 1, 5: 2, 8: 3, 12: 4}
    m.info["clear"] = [[(x + math.cos(a) * 0.45, y + math.sin(a) * 0.45) for a in (k * math.pi / 4 for k in range(8))] for x, y in spots]
    for i, (x, y) in enumerate(spots):
        if i in shown:
            N = shown[i]
            nm = f"mine.{N}"
            tilt = Vector((g.uniform(-0.12, 0.12), g.uniform(-0.12, 0.12), 1.0)).normalized()
            at = Vector((x, y, -0.02))
            m.part(nm, root, at, kind="free", note="a half-dug plate mine and the earth dug out round it: `detonate." + str(N) + "` blows it")
            m.use(nm)
            mound(m, x, y, 0.42, 0.07, "earth", seed=400 + i, hole=0.17)
            # the spoil thrown out when it was half dug, heaped beside it
            sa = g.uniform(0, 2 * math.pi)
            mound(m, x + math.cos(sa) * 0.55, y + math.sin(sa) * 0.55, g.uniform(0.22, 0.3), g.uniform(0.07, 0.1), "earth", seed=500 + i)
            plate_mine(m, at, tilt, g.uniform(0, 360))
            m.socket(nm, at + Z * 0.08, (0, 0, 1), part=nm, note="the mine: where it goes off")
            cr = f"crater.{N}"
            m.part(cr, root, (x, y, 0.0), kind="free", note="the crater it leaves: hidden (scale 0) until `detonate." + str(N) + "`")
            m.use(cr)
            crater(m, x, y, 0.95, seed=600 + i)
            m.use(root)
        else:
            mound(m, x, y, g.uniform(0.34, 0.5), g.uniform(0.025, 0.05), "earth", seed=700 + i)
    # what the engineers left: an empty mine crate on the defender's side and a prodder lying by it
    cr = Vector((-3.7, 2.4, 0.0))
    q = 18.0
    m.box(cr + Vector((0, 0, 0.11)), (0.42, 0.62, 0.2), "planks", yaw=q)
    m.box(cr + Vector((0, 0, 0.205)), (0.37, 0.57, 0.012), "void", yaw=q)
    m.box(cr + Vector((-0.42, 0.1, 0.025)), (0.45, 0.66, 0.025), "planks", yaw=q + 25)          # its lid on the ground
    for e in (-0.22, 0.22):
        qq = Quaternion(Z, math.radians(q))
        m.box(cr + qq @ Vector((0, e, 0.11)), (0.44, 0.03, 0.05), "timber", yaw=q)
    m.rod((-3.5, 1.4, 0.02), (-3.6, 2.4, 0.03), 0.009, "timber", sides=6)
    m.rod((-3.5, 1.4, 0.02), (-3.47, 1.1, 0.02), 0.004, "steel", sides=5)
    trod(m, -3.7, 2.2, 0.7)
    # -- clips
    for N in range(1, 5):
        m.clips[f"detonate.{N}"] = {"long": 1.2, "loop": False, "after": "hold",
                                    "about": f"Mine {N} goes off under a tank or a man: the mine is gone and a crater lies in its place.",
                                    "tracks": [{"part": f"mine.{N}", "scale": [[0.0, 1.0], [0.02, 0.0]]},
                                               {"part": f"crater.{N}", "scale": [[0.0, 0.0], [0.02, 0.2], [0.18, 1.08], [0.4, 1.0]]}],
                                    "events": [{"t": 0.0, "fx": "mine_blast", "socket": f"mine.{N}", "cue": "explosion_debris"}]}
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Lying in wait; the craters hidden.",
                       "tracks": [{"part": f"crater.{N}", "scale": [[0.0, 0.0]]} for N in range(1, 5)], "events": []}
    m.socket("hit", (0.0, 0.0, 0.3), (1, 0, 0), part=root, note="where a shell strikes")
    weather(m, splash=0.2, rust=0.5)
    m.info["span"] = 9.0
    m.info["high"] = 0.2
    m.info.setdefault("worn", []).extend([(-3.7, 2.3, 0.8)])
    return m


# ---- 6. the road bar -----------------------------------------------------------------------------------

def chain(m, a, b, sag=0.12, link=0.05, paint="steel"):
    """A hanging chain of oval links, each turned a quarter to the last."""
    a, b = Vector(a), Vector(b)
    n = max(3, int((a - b).length * 1.25 / link))
    pts = [a.lerp(b, k / n) - Z * sag * math.sin(math.pi * k / n) for k in range(n + 1)]
    sides = 8 if m.fine else 4
    for k in range(n):
        p, q = pts[k], pts[k + 1]
        d = (q - p).normalized()
        mid = (p + q) / 2
        side = d.cross(Z)
        if side.length < 1e-3:
            side = X.copy()
        side.normalize()
        if k % 2:
            side = d.cross(side).normalized()
        ring = [mid + d * math.cos(2 * math.pi * j / sides) * link * 0.62 + side * math.sin(2 * math.pi * j / sides) * link * 0.36 for j in range(sides)]
        wire(m, ring + [ring[0]], 0.0055, paint, 4 if m.fine else 3)


def RoadBar(fine=True, nation="_default", name="RoadBar"):
    """A swing-beam barrier across a 5 m road: a counterweighted beam painted in bands, its pivot post set in a
    concrete block, a rest post with a cradle at the far side."""
    m = bits.model(name, "Road bar, a swing beam across a road (our own model)", "works", nation, fine)
    root = m.part("bar", None, (0, 0, 0), kind="root")
    m.use(root)
    g = rnd(8)
    PY, PZ = -3.2, 1.0                     # the pivot: on the road's right-hand side
    RY = 3.05                              # the rest post
    # -- the road: the stage wears it to earth (m.info["worn"]); here only the two wheel ruts, darker and a little
    #    sunk-looking, their middles and edges wandering, and a few stones
    nx = 50 if fine else 12
    for k, ty in enumerate((-0.85, 0.85)):
        rows = []
        for i in range(nx + 1):
            x = -5.0 + 10.0 * i / nx
            cy = ty + 0.16 * bits.fbm((x * 0.3, 4.0 * k, 1.0), 0.8) + 0.03 * bits.fbm((x * 1.7, k, 2.0), 0.5)
            # (it narrows to almost nothing where the ground is firm, and fades out at both ends)
            hw = 0.16 * min(max(0.75 + 1.3 * bits.fbm((x * 0.6, 7.0 + k, 3.0), 0.6), 0.12), 1.5) * smooth((5.0 - abs(x)) / 1.2)
            wl = max(hw * (1.0 + 0.55 * bits.fbm((x * 3.1, 11.0 + k, 4.0), 0.4)), 0.003)
            wr = max(hw * (1.0 + 0.55 * bits.fbm((x * 3.1, 19.0 + k, 5.0), 0.4)), 0.003)
            row = [Vector((x, cy - wr, 0.006)), Vector((x, cy - wr * 0.45, 0.0045)), Vector((x, cy, 0.004)),
                   Vector((x, cy + wl * 0.45, 0.0045)), Vector((x, cy + wl, 0.006))]
            if not fine:
                row = [row[0], row[2], row[4]]
            rows.append(row)
        m.sheet(rows, "earth", Z, True)
    for k in range(14 if fine else 5):
        sx, sy = g.uniform(-4.6, 4.6), g.uniform(-2.2, 2.2)
        r = g.uniform(0.025, 0.07)
        m.ball((sx, sy, r * 0.25), (r, r * g.uniform(0.7, 1.0), r * 0.6), g.choice(["concrete.old", "concrete", "earth.dry"]), 7 if fine else 4, 4)
    # -- the pivot post: a block of concrete, two steel cheeks bolted into it, the pin through them
    blk = bits.s_box((-0.36, PY - 0.36, -0.4), (0.36, PY + 0.36, 0.38))
    finish(m, blk, "concrete", root, 0.035, 0.006, 2.0, 0.3)
    for sx in (-1, 1):
        c = Vector((sx * 0.085, PY, 0.38))
        bits.plate(m, [(-0.14, 0.0), (0.14, 0.0), (0.1, PZ - 0.38 + 0.08), (0.0, PZ - 0.38 + 0.12), (-0.1, PZ - 0.38 + 0.08)], 0.016, "upper", c, Y, Z)
        bits.plate(m, [(-0.16, -0.08), (0.16, -0.08), (0.16, 0.08), (-0.16, 0.08)], 0.012, "upper", c + X * sx * 0.06 + Z * 0.006, Y, X * sx, centred=False)
        if fine:
            for by in (-0.11, 0.11):
                bits.bolt(m, c + X * sx * 0.06 + Y * by + Z * 0.012, Z, 0.02, 0.016, "steel")
        # the pin's head and nut
        m.lathe(Vector((sx * 0.093, PY, PZ)), X * sx, [(0.0, 0.035), (0.016, 0.035), (0.022, 0.028)], "steel", 6, (True, True))
    m.rod((-0.13, PY, PZ), (0.13, PY, PZ), 0.016, "steel", sides=12 if fine else 5)
    # -- the rest post: a squared timber with a steel cradle the beam drops into, and the chain that locks it
    rp = Vector((0.0, RY, 0.0))
    bits.beam(m, rp + Vector((0, 0, -0.3)), rp + Vector((0, 0, 0.88)), 0.15, 0.15, "timber")
    m.box(rp + Vector((0, 0, 0.895)), (0.17, 0.17, 0.03), "timber.dark")
    for sx in (-1, 1):
        bits.plate(m, [(-0.05, 0.0), (0.05, 0.0), (0.04, 0.2), (-0.04, 0.2)], 0.01, "upper", rp + Vector((sx * 0.075, 0, 0.76)), Y, Z)
        if fine:
            bits.bolt(m, rp + Vector((sx * 0.081, 0, 0.8)), X * sx, 0.014, 0.01, "steel")
    bits.plate(m, [(-0.08, -0.05), (0.08, -0.05), (0.08, 0.05), (-0.08, 0.05)], 0.01, "upper", rp + Vector((0, 0, 0.915)), X, Y)
    trod(m, 0.0, RY, 0.35)
    trod(m, 0.0, PY, 0.6)
    chain(m, rp + Vector((0.08, 0.0, 0.84)), rp + Vector((0.12, -0.05, 0.5)), 0.04, 0.05)
    if fine:
        m.box(rp + Vector((0.13, -0.05, 0.46)), (0.04, 0.025, 0.05), "brass")
    # -- the beam: tapered, its corners eased, painted in bands; a counterweight box on the short end
    beam = m.part("beam", root, (0.0, PY, PZ), axis=(1, 0, 0), limits=[0.0, 80.0], kind="turn",
                  note="the barrier: plus lifts it (0 down across the road, 80 up)")
    m.use(beam)
    y0, y1 = PY - 1.05, RY + 0.32
    n = 30 if fine else 8
    rings = []
    for i in range(n + 1):
        y = y0 + (y1 - y0) * i / n
        f = max(0.0, (y - PY) / (y1 - PY))
        hw, hh = 0.07 - 0.022 * f, 0.075 - 0.025 * f
        cz = PZ - 0.012 * f * f
        e = 0.018
        pts2 = [(hw, hh - e), (hw - e, hh), (-hw + e, hh), (-hw, hh - e), (-hw, -hh + e), (-hw + e, -hh), (hw - e, -hh), (hw, -hh + e)]
        rings.append([Vector((px, y, cz + pz)) for px, pz in pts2])

    def bands(mid, nn):
        if mid.y < PY + 0.25:
            return "timber.dark"
        return "sign" if int((mid.y - (PY + 0.25)) / 0.5) % 2 == 0 else "sign.3"
    m.skin(rings, bands, (True, True), False, cap_paint="timber.end")
    # steel straps where the pin goes through, and at the tip
    for yy, w in ((PY - 0.09, 0.03), (PY + 0.09, 0.03), (y1 - 0.06, 0.04)):
        f = max(0.0, (yy - PY) / (y1 - PY))
        hw, hh = 0.07 - 0.022 * f + 0.004, 0.075 - 0.025 * f + 0.004
        m.box((0.0, yy, PZ - 0.012 * f * f), (2 * hw, w, 2 * hh), "upper")
    # the counterweight: a steel box of stones hung under the short end
    cw = Vector((0.0, PY - 0.75, PZ - 0.08))
    m.box(cw, (0.3, 0.5, 0.32), "upper")
    m.box(cw + Z * 0.162, (0.27, 0.47, 0.006), "void")
    if fine:
        for k in range(8):
            m.ball(cw + Vector((g.uniform(-0.09, 0.09), g.uniform(-0.18, 0.18), 0.16)), (g.uniform(0.04, 0.06), g.uniform(0.04, 0.06), 0.035), "concrete.old", 7, 4)
        for sy in (-1, 1):
            for sx in (-1, 1):
                bits.bolt(m, cw + Vector((sx * 0.151, sy * 0.2, 0.1)), X * sx, 0.013, 0.008, "steel")
    for sx in (-1, 1):
        m.box(cw + Vector((sx * 0.12, 0, 0.2)), (0.02, 0.5, 0.06), "upper")
    m.use(root)
    # sandbags round the pivot block, an oil can on the block
    for k, a in enumerate((-60, -20, 20, 60)):
        bits.sandbag(m, (-0.55 + 0.1 * math.cos(math.radians(a)), PY - 0.55 * math.sin(math.radians(a)) * 1.2, 0.0), 90 + a, seed=k + 3)
    m.lathe(Vector((0.18, PY + 0.2, 0.38)), Z, [(0.0, 0.05), (0.12, 0.05), (0.14, 0.03), (0.17, 0.012), (0.2, 0.01)], "upper2", 12 if fine else 5)
    m.socket("tip", (0.0, y1, PZ), (0, 1, 0), part="beam", note="the beam's free end")
    m.socket("hit", (0.3, 0.0, 0.9), (1, 0, 0), part=root, note="where a shell strikes")
    bits.crew(m, "sentry", (-0.9, PY - 0.2, 0.0), (0.3, 1, 0), "stand", root, "standing by the pivot post, a hand on the counterweight to lift the bar")
    m.clips["lift"] = {"long": 3.0, "loop": False, "after": "hold", "about": "The bar is swung up to let traffic through.",
                       "tracks": [{"part": "beam", "turn": [[0.0, 0.0], [0.3, 1.5], [0.8, 14.0], [1.6, 45.0], [2.3, 72.0], [2.7, 80.5], [3.0, 80.0]]}], "events": []}
    m.clips["lower"] = {"long": 3.0, "loop": False, "after": "hold", "about": "The bar is let down into its cradle.",
                        "tracks": [{"part": "beam", "turn": [[0.0, 80.0], [0.4, 76.0], [1.2, 48.0], [2.0, 14.0], [2.5, 0.0], [2.62, 1.2], [2.75, 0.0], [3.0, 0.0]]}], "events": []}
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Down across the road.", "tracks": [], "events": []}
    weather(m, splash=0.3, rust=0.4)
    m.info["span"] = 7.5
    m.info["high"] = 0.8
    m.info.setdefault("worn", []).extend([(-0.9, PY - 0.2, 1.1, 1.0)] + [(-5.0 + 1.0 * k, 0.0, 2.0, 1.0) for k in range(11)])
    m.info["clear"] = [[(-0.9, PY - 0.9), (0.6, PY - 0.9), (0.6, PY + 0.5), (-0.9, PY + 0.5)]]
    return m


BUILDERS = {"Wire": Wire, "WireFence": WireFence, "DragonsTeeth": DragonsTeeth, "Hedgehog": Hedgehog, "Hedgehogs": Hedgehogs, "Minefield": Minefield, "RoadBar": RoadBar}
