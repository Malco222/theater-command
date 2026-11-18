"""The heavy concrete works: the gun casemate (bunker) of the land front and the coastal battery on its headland.

Cast as the pillbox is (concrete.py): a solid block, its rooms hollowed out, its embrasures cut in steps, every edge
chamfered; what a heavy hit breaks is split off beforehand along a ragged break. Earth is heaped over and banked
against them as one soft-edged field of earth and turf, carved back where an embrasure or a door must see out.

Our own plain designs; no army's own pattern.
"""
import math

from mathutils import Matrix, Quaternion, Vector

import bits
from bits import X, Y, Z, rnd
from concrete import embrasure_gun, gun_clip, local, net_hooks, ragged, rubble, split, steel_door, take_slots
from guns import shell
from posts import weather
from rigid import smooth


# ---- helpers ------------------------------------------------------------------------------------------------

def steps_cut(theta, out, t0, steps, tail=1.2, mouth=0.6):
    """The cutter for a stepped, splayed opening in the face looking toward `theta` (degrees), standing `out` from the
    origin, its middle `t0` along the face. steps: (depth in from the face, width, z bottom, z top) from the mouth
    inward, each running to the next one's depth; the last runs `tail` further in."""
    cutters = []
    for k, (d, w, z0, z1) in enumerate(steps):
        d1 = steps[k + 1][0] if k + 1 < len(steps) else d + tail
        n0, n1 = out - d1, out - d + (mouth if k == 0 else 0.002)
        ob = bits.s_box((n0, t0 - w / 2, z0), (n1, t0 + w / 2, z1))
        cutters.append(bits.s_move(ob, yaw=theta))
    a = cutters[0]
    if len(cutters) > 1:
        bits.s_join(a, *cutters[1:])
    return a


def s_xz(profile, y0, y1):
    """A solid from an outline in x and z (pairs), drawn out along y from y0 to y1."""
    return bits.s_wedge([(x, y0, z) for x, z in profile], [(x, y1, z) for x, z in profile])


def s_yz(profile, x0, x1):
    """A solid from an outline in y and z (pairs), drawn out along x from x0 to x1."""
    return bits.s_wedge([(x0, y, z) for y, z in profile], [(x1, y, z) for y, z in profile])


def rect_dist(x, y, x0, x1, y0, y1):
    return math.hypot(max(x0 - x, 0.0, x - x1), max(y0 - y, 0.0, y - y1))


def earth_field(m, h_of, xs, ys, hidden, amount=0.55, seed=0.0, part=None, inner=lambda x, y: False, floor=None, snap=None, grass=3.0, scale=1.5):
    """A field of heaped earth over a grid: h_of(x, y) the height, hidden(x, y, h) whether a point lies under concrete or
    the ground (a cell all of whose corners are hidden is left out). A hidden corner `inner` (strictly inside a work's
    walls) is let down to `floor` (just under the roof), so the earth runs out softly over the roof; a cell that would
    bridge from such a corner down the outside of a wall is left out, so no face ever crosses an opening. `snap(x, y)`
    may move a point lying just off a wall onto it (for walls the grid does not follow). Turfed."""
    P = [[(snap(x, y) if snap else None) or (x, y) for y in ys] for x in xs]
    H = [[h_of(*P[i][j]) for j in range(len(ys))] for i in range(len(xs))]
    HID = [[hidden(*P[i][j], H[i][j]) for j in range(len(ys))] for i in range(len(xs))]
    INN = [[inner(*P[i][j]) for j in range(len(ys))] for i in range(len(xs))]
    if floor is not None:
        for i in range(len(xs)):
            for j in range(len(ys)):
                if INN[i][j] and HID[i][j]:
                    H[i][j] = floor
    verts, vid, faces = [], {}, []

    def vi(i, j):
        k = (i, j)
        if k not in vid:
            vid[k] = len(verts)
            verts.append(Vector((P[i][j][0], P[i][j][1], max(H[i][j], 0.004))))
        return vid[k]
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            cs = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
            hid = [HID[a][b] for a, b in cs]
            ins = [INN[a][b] for a, b in cs]
            if all(hid):
                continue
            if any(h and k for h, k in zip(hid, ins)) and (floor is None or any((not k) and H[a][b] < floor - 0.02 for k, (a, b) in zip(ins, cs))):
                continue
            q = [vi(a, b) for a, b in cs]
            # (split along the diagonal that keeps the surface nearest its own heights)
            if abs(H[i][j] - H[i + 1][j + 1]) < abs(H[i + 1][j] - H[i][j + 1]):
                tris = [(q[0], q[1], q[2]), (q[0], q[2], q[3])]
            else:
                tris = [(q[0], q[1], q[3]), (q[1], q[2], q[3])]
            for t in tris:
                if len(set(t)) == 3 and (verts[t[1]] - verts[t[0]]).cross(verts[t[2]] - verts[t[0]]).length > 1e-6:
                    faces.append(t)
    bits.turfed(m, verts, faces, amount, "earth", "turf", seed, part, scale=scale, grass=grass)
    return H


def poly_in(poly, x, y):
    c, j = False, len(poly) - 1
    for i in range(len(poly)):
        (xi, yi), (xj, yj) = poly[i], poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-12) + xi:
            c = not c
        j = i
    return c


def poly_near(poly, x, y):
    """The distance from a point to a polygon's outline, and the nearest point on it."""
    best, at = 1e18, (x, y)
    for k in range(len(poly)):
        (ax, ay), (bx, by) = poly[k], poly[(k + 1) % len(poly)]
        dx, dy = bx - ax, by - ay
        t = min(max(((x - ax) * dx + (y - ay) * dy) / max(dx * dx + dy * dy, 1e-12), 0.0), 1.0)
        px, py = ax + dx * t, ay + dy * t
        d = math.hypot(x - px, y - py)
        if d < best:
            best, at = d, (px, py)
    return best, at


def rebar(m, base, way, long, seed=0, bent=0.3):
    """A reinforcing rod left standing out of the concrete, bent over at the top."""
    if not m.fine:
        return
    g = rnd(seed)
    base, way = Vector(base), Vector(way).normalized()
    side = way.cross(Vector((g.uniform(-1, 1), g.uniform(-1, 1), 0.3))).normalized()
    pts = [base - way * 0.05, base + way * long * 0.5 + side * 0.01, base + way * long * 0.85 + side * long * bent * 0.3,
           base + way * long * 0.95 + side * long * bent]
    bits.tube(m, pts, 0.008, "rust", 5)


def cupola(m, at, r=0.72, high=0.52, slits=6, face=0.0, paint="upper"):
    """A low armoured observation dome on its collar: cast steel, look-out slits round the front with brows over them."""
    at = Vector(at)
    sides = 40 if m.fine else 14
    prof = [(0.0, r + 0.08), (0.06, r + 0.08), (0.08, r)]
    for k in range(1, 9):
        a = math.radians(90.0 * k / 8)
        prof.append((0.08 + math.sin(a) * high, r * math.cos(a) + 0.002 * (k == 8)))
    dome = bits.s_lathe(at, Z, prof, sides)
    cuts = []
    zs = at.z + 0.08 + high * 0.38
    for k in range(slits):
        th = face + (k - (slits - 1) / 2) * (300.0 / slits) * (0.7 if slits > 3 else 1.0)
        ob = bits.s_box((r - 0.12, -0.13, -0.035), (r + 0.3, 0.13, 0.035))
        bits.s_move(ob, (at.x, at.y, zs), yaw=th)
        cuts.append(ob)
    bits.s_cut(dome, *cuts)
    bits.s_bevel(dome, 0.008, 1)
    rr = r * math.cos(math.asin(min(0.99, 0.38)))

    def paint_of(mid, n):
        d = math.hypot(mid.x - at.x, mid.y - at.y)
        h = mid.z - at.z - 0.08
        # (faces lying well inside the dome's skin are the slits' sides and backs)
        if 0.0 < h < high:
            want = r * math.cos(math.asin(min(1.0, h / high)))
            if d < want - 0.03:
                return "void"
        return paint
    bits.take(m, dome, paint_of, True)
    # a brow over every slit, and a ring of bolts round the foot
    for k in range(slits):
        th = face + (k - (slits - 1) / 2) * (300.0 / slits) * (0.7 if slits > 3 else 1.0)
        c, s = math.cos(math.radians(th)), math.sin(math.radians(th))
        d = Vector((c, s, 0))
        p = at + d * (rr + 0.015) + Z * (zs - at.z + 0.06)
        m.box(p, (0.05, 0.32, 0.025), paint, yaw=th, pitch=-25)
    bits.bolt_ring(m, at + Z * 0.06, Z, r + 0.04, 24, 0.016, 0.014, "steel")
    # the hood of the periscope standing out of the crown
    m.lathe(at + Z * (0.08 + high - 0.02), Z, [(0.0, 0.07), (0.16, 0.07), (0.16, 0.085), (0.2, 0.085), (0.22, 0.06)], paint, 16 if m.fine else 8)
    m.box(at + Z * (0.08 + high + 0.12) + Vector((math.cos(math.radians(face)), math.sin(math.radians(face)), 0)) * 0.07, (0.03, 0.09, 0.05), "glass", yaw=face)


def ventilator(m, at, high=0.7, paint="upper"):
    """The air intake: a pipe out of the earth with a mushroom cowl and a grille under it."""
    at = Vector(at)
    s = 20 if m.fine else 8
    m.lathe(at, Z, [(0.0, 0.16), (0.08, 0.16), (0.09, 0.12), (high, 0.12)], paint, s, (False, False))
    top = at + Z * high
    m.lathe(top, Z, [(0.0, 0.13), (0.02, 0.14), (0.20, 0.14), (0.21, 0.13)], "iron", s, (False, False))
    if m.fine:
        for k in range(16):
            a = 2 * math.pi * k / 16
            d = Vector((math.cos(a), math.sin(a), 0))
            m.rod(top + d * 0.142 + Z * 0.02, top + d * 0.142 + Z * 0.2, 0.006, "steel", sides=4)
    m.lathe(top + Z * 0.21, Z, [(0.0, 0.05), (0.0, 0.30), (0.03, 0.31), (0.08, 0.26), (0.13, 0.14), (0.15, 0.02)], paint, s + 8, (True, True))
    bits.bolt_ring(m, top + Z * 0.215, -Z, 0.27, 10, 0.012, 0.01, "steel")


def lamp(m, at):
    m.box(Vector(at) + Z * 0.04, (0.16, 0.16, 0.06), "steel")
    m.ball(Vector(at) - Z * 0.03, 0.065, "glass", 10, 6)
    m.rod(Vector(at) + Z * 0.07, Vector(at) + Z * 0.12, 0.012, "steel", sides=6)


# ---- the bunker -----------------------------------------------------------------------------------------------

def Bunker(fine=True, nation="_default", name="Bunker"):
    """A gun casemate for an anti-tank or field gun: walls and roof 1.6 to 1.8 m thick, one wide stepped embrasure in
    front under an overhanging roof lip, a machine gun covering the entrance flank, the door behind a wing wall at the
    foot of steps, earth heaped over the roof and banked against the flanks and the rear."""
    m = bits.model(name, "Gun casemate (our own model)", "works", nation, fine)
    m.part("bunker", None, (0, 0, 0), kind="root")
    m.use("bunker")
    seg = 2 if fine else 1
    XF, XR, YS = 4.0, -4.0, 4.5             # the front face, the rear face, the flanks
    TOP, FOOT = 3.2, -0.75                  # the roof's top, the foundation's foot
    FLOOR, CEIL = -0.4, 1.5                 # the room's floor and ceiling (the floor sunk below the ground outside)
    RX0, RX1, RY = -2.4, 2.2, 2.9           # the room: rear wall 1.6, front wall 1.8, flanks 1.6 thick
    DY, DW = 1.8, 1.1                       # the passage to the door: its middle, its width
    MGX, MGZ = -1.9, 0.8                    # the machine gun's embrasure in the left flank
    EZ0, EZ1 = 0.30, 1.40                   # the main embrasure's throat (2.6 m wide)

    # ---- the body: one block with its roof lip, the entrance well's walls, the apron; then the cuts
    prof = [(XR, FOOT), (XF, FOOT), (XF, 2.12), (XF + 0.72, 2.24), (XF + 0.80, 2.34), (XF + 0.80, 3.08), (XF + 0.70, TOP), (XR, TOP)]
    body = s_xz(prof, -YS, YS)
    # the apron: a splayed slab in front, a little proud of the ground, falling away
    apron = bits.s_wedge([(XF - 0.2, -2.7, -0.45), (XF + 3.2, -3.9, -0.45), (XF + 3.2, 3.9, -0.45), (XF - 0.2, 2.7, -0.45)],
                         [(XF - 0.2, -2.7, 0.11), (XF + 3.2, -3.9, 0.035), (XF + 3.2, 3.9, 0.035), (XF - 0.2, 2.7, 0.11)])
    # the entrance well: a floor slab, a wing wall across behind it and a short return, steps rising to the left
    WX0, WX1 = XR - 1.75, XR - 1.30          # the wing wall
    WY = 6.0                                 # where the steps come up to the ground
    CX = XF + 2.6                            # how far the front wing walls run out
    wslab = bits.s_box((WX0, 0.40, FOOT + 0.3), (XR + 0.2, WY, FLOOR))
    wing = s_yz([(0.40, FOOT + 0.25), (WY, FOOT + 0.25), (WY, 0.35), (WY - 0.6, 0.9), (WY - 1.5, 2.05), (0.40, 2.05)], WX0, WX1)
    # a low cheek wall holding the earth back where the steps rise past the corner
    cheek = bits.s_wedge([(XR - 0.1, 4.3, FOOT + 0.25), (XR + 0.4, 4.3, FOOT + 0.25), (XR + 0.4, WY, FOOT + 0.25), (XR - 0.1, WY, FOOT + 0.25)],
                         [(XR - 0.1, 4.3, 1.2), (XR + 0.4, 4.3, 1.2), (XR + 0.4, WY, 0.25), (XR - 0.1, WY, 0.25)])
    ret = bits.s_box((WX0, 0.40, FOOT + 0.25), (XR + 0.15, 0.80, 2.05))
    stairs = [bits.s_box((WX1 - 0.05, WY - 2.5 + 0.5 * k, FLOOR - 0.1), (XR - 0.05, WY, FLOOR + 0.08 * (k + 1) - 0.0015 * k)) for k in range(5)]
    # wing walls splayed out either side of the embrasure, holding back the banks that wrap round the front
    cheeks = []
    for sy in (1, -1):
        lo = [(XF - 0.1, sy * 3.3, FOOT), (CX, sy * 3.95, FOOT), (CX, sy * YS, FOOT), (XF - 0.1, sy * YS, FOOT)]
        hi = [(XF - 0.1, sy * 3.3, 2.08), (CX, sy * 3.95, 0.42), (CX, sy * YS, 0.42), (XF - 0.1, sy * YS, 2.08)]
        cheeks.append(bits.s_wedge(lo, hi))
    bits.s_join(body, apron, wslab, wing, ret, cheek, *stairs, *cheeks)

    room = bits.s_box((RX0, -RY, FLOOR), (RX1, RY, CEIL))
    passage = bits.s_box((XR - 0.6, DY - DW / 2, FLOOR + 0.02), (RX0 + 0.1, DY + DW / 2, 1.42))
    main = steps_cut(0.0, XF, 0.0, [(0.0, 4.9, 0.14, 1.92), (0.36, 4.3, 0.18, 1.80), (0.72, 3.7, 0.22, 1.66), (1.08, 3.15, 0.26, 1.53), (1.44, 2.6, EZ0, EZ1)], tail=0.8)
    mg = steps_cut(90.0, YS, -MGX, [(0.0, 1.7, MGZ - 0.52, MGZ + 0.46), (0.3, 1.32, MGZ - 0.40, MGZ + 0.36), (0.6, 0.98, MGZ - 0.28, MGZ + 0.26),
                                    (0.9, 0.66, MGZ - 0.18, MGZ + 0.17), (1.1, 0.42, MGZ - 0.12, MGZ + 0.12), (1.32, 0.86, FLOOR + 0.55, MGZ + 0.55)], tail=0.6)
    scars = []
    if fine:
        # where shells have struck and spalled the face: shallow ragged dishes
        g0 = rnd(31)
        for (fx, fy, fz, r) in ((XF + 0.8, -2.1, 2.75, 0.32), (XF + 0.8, 1.2, 2.55, 0.22), (XF + 0.8, 3.6, 2.85, 0.26), (XF, -2.95, 1.25, 0.24),
                                (XF, 2.8, 0.75, 0.2), (XF + 0.8, -0.3, 2.95, 0.18), (XF + 1.4, -3.75, 1.4, 0.22)):
            ob = bits.s_lathe((0.3, 0, 0), (-1, 0, 0), [(0.0, r * 1.3), (0.3, r * 1.1), (0.3 + r * 0.2, r * 0.75), (0.3 + r * 0.32, r * 0.35), (0.3 + r * 0.38, r * 0.05)], 9)
            ob.data.transform(Matrix.Diagonal((1.0, g0.uniform(0.7, 1.3), g0.uniform(0.6, 1.0), 1.0)))
            bits.s_move(ob, (fx, fy, fz), yaw=g0.uniform(-12, 12), roll=g0.uniform(0, 180))
            scars.append(ragged(ob, r * 0.22, r * 0.3, g0.uniform(0, 9)))
    bits.s_cut(body, room, passage, main, mg, *scars)
    bits.s_bevel(body, 0.04, seg)

    # ---- what a heavy hit breaks: the face round the embrasure and the lip over it
    brk = ragged(bits.s_wedge([(XF + 1.4, -3.45, 0.22), (XF + 1.4, 3.35, 0.26), (XF + 1.4, 3.15, 3.6), (XF + 1.4, -3.25, 3.6)],
                              [(RX1 - 0.25, -2.65, 0.32), (RX1 - 0.25, 2.6, 0.30), (2.75, 2.45, 3.6), (2.75, -2.5, 3.6)]),
                 0.2 if fine else 0.6, 0.09, 3.0)
    front, body = split(body, brk)

    def inside(p):
        return RX0 - 0.03 < p.x < RX1 + 0.03 and abs(p.y) < RY + 0.03 and FLOOR - 0.02 < p.z < CEIL + 0.03 or \
            (XR + 0.7 < p.x < RX0 + 0.1 and abs(p.y - DY) < DW / 2 + 0.02 and p.z < 1.43) or \
            (p.y > RY - 0.05 and p.y < YS - 1.25 and abs(p.x - MGX) < 0.5 and p.z < MGZ + 0.6)
    paint = lambda mid, n, k: "concrete.in" if (k == 1 or inside(mid)) else "concrete"           # noqa: E731
    take_slots(m, body, paint, "bunker")
    m.part("front", "bunker", (XF + 0.5, 0.0, 0.25), kind="free", note="the face round the main embrasure and the roof lip over it: a heavy hit throws it out and down")
    take_slots(m, front, paint, "front")
    m.use("bunker")

    # ---- the earth: heaped over the roof, banked against the flanks and rear; carved back at the door and the gun
    def edge_h(x):
        return 3.36 * (1.0 - 0.45 * smooth((x - 1.0) / 3.0))

    def bump(x, y):
        u, v = (x + 0.7) / 4.2, y / 5.0
        return max(0.0, 1 - u * u) * max(0.0, 1 - v * v)

    def is_in(x, y):
        return XR + 1e-4 < x < XF - 1e-4 and abs(y) < YS - 1e-4

    def roof_cap(x, y):
        # (the roof's earth stops short over the door and over the machine gun)
        return min(3.10 + 1.6 * max(0.0, x - (XR + 0.75)) + 1.6 * max(0.0, 0.35 - y) + 1.6 * max(0.0, y - 4.9),
                   3.10 + 1.6 * max(0.0, (YS - 0.65) - y) + 1.6 * max(0.0, abs(x - MGX) - 1.3))

    def h_of(x, y):
        if is_in(x, y):
            h = 3.36 + 0.55 * bump(x, y) + 0.06 * bits.fbm((x * 0.6, y * 0.6, 3.0), 0.8)
            h -= (h - 3.08) * smooth((x - 2.3) / 0.7)
            return min(h, roof_cap(x, y))
        dx = max(XR - x, 0.0, x - XF)
        dy = max(abs(y) - YS, 0.0)
        d = math.hypot(dx, dy)
        E = edge_h(min(max(x, XR), XF))
        W = 1.6 + 1.35 * E
        h = E * (1.0 - smooth(d / W))
        if x > XF - 1e-4:
            if abs(y) < YS - 1e-4:
                if x < CX + 0.05:
                    return -1.0
                h *= smooth((abs(y) - 3.4) / 1.0)
            h = min(h, max(0.0, 2.08 - (x - XF + 0.1) * (2.08 - 0.42) / (CX - XF + 0.1)) + 0.6 * (abs(y) - YS) - 0.04)
        h += 0.09 * bits.fbm((x * 0.55, y * 0.55, 2.0), 0.7) * smooth(h / 0.5)
        if d < 1e-3:
            h = min(h, roof_cap(x, y))
        # the entrance well: earth against the back of the wing wall, falling away to the steps' head
        if (x < XR + 1e-4 and 0.4 < y < WY and x > WX0) or (XR - 0.1 < x < XR + 0.4 and 4.3 < y < WY):
            return -1.0
        c_wall = 1.3 + 0.75 * rect_dist(x, y, WX0, XR + 0.4, 0.40, WY)
        c_exit = 0.04 + 0.6 * math.hypot(max(WX1 - x, 0.0, x - XR), max(0.0, y - WY) + 3.0 * max(0.0, WY - y))
        c_cheek = (1.15 - 0.95 * min(max((y - 4.3) / (WY - 4.3), 0.0), 1.0)) + 0.8 * max(0.0, x - XR - 0.4) if (y > 4.3 and x > XR - 0.1) else 99.0
        h = min(h, c_wall, c_exit, c_cheek)
        # the machine gun's field of fire: a cutting in the bank, widening outward
        if y > YS - 0.01:
            h = min(h, 0.22 + 1.5 * max(0.0, abs(x - MGX) - 0.75 - 0.5 * max(0.0, y - YS)))
        return h

    def hidden(x, y, h):
        if is_in(x, y):
            return h < TOP - 0.01
        return h < 0.012

    step = 0.32 if fine else 0.8
    xs = [XR + step * k for k in range(-int(7.6 / step), int((XF - XR + 7.0) / step) + 1)]
    ys = [-YS + 0.3 * (1 if fine else 3) * k for k in range(-int(7.0 / (0.3 * (1 if fine else 3))), int((2 * YS + 7.0) / (0.3 * (1 if fine else 3))) + 1)]
    earth_field(m, h_of, xs, ys, hidden, 0.6, 1.0, "bunker", inner=is_in, floor=TOP - 0.03, grass=3.0)
    bits.patch(m, XF + 2.5, 0.0, 4.2, "earth.dry", seed=4, oval=0.8)
    bits.patch(m, XR - 1.5, 5.6, 2.2, "earth.dry", seed=6, oval=1.2)

    # ---- the roof's furniture
    ventilator(m, (-1.6, 1.4, h_of(-1.6, 1.4) - 0.25), 0.75)
    cup = Vector((0.6, -1.6, h_of(0.6, -1.6) - 0.12))
    m.lathe(Vector((cup.x, cup.y, TOP - 0.05)), Z, [(0.0, 0.98), (cup.z - TOP + 0.10, 0.98), (cup.z - TOP + 0.19, 0.9)], "concrete", 32 if fine else 12, (False, True))
    cupola(m, cup + Z * 0.14, 0.72, 0.5, 5, 0.0)
    # hooks for nets along the lip and over the door; rods never cut off where the concrete was left
    hooks, ways = [], []
    for t in [-4.1 + 0.82 * k for k in range(11)]:
        hooks.append((XF + 0.81, t, 2.86))
        ways.append((1, 0, 0))
    for t in (0.6, 1.4, 2.2, 3.0, 3.8):
        hooks.append((XR - 0.01, t, TOP - 0.16))
        ways.append((-1, 0, 0))
    net_hooks(m, hooks, ways)
    g = rnd(17)
    for k in range(9):
        rebar(m, (XF + g.uniform(-0.9, 0.3), g.choice((-1, 1)) * g.uniform(2.9, 4.2), TOP - 0.02), (g.uniform(-0.2, 0.2), g.uniform(-0.2, 0.2), 1), g.uniform(0.18, 0.45), seed=k)
    for k in range(5):
        rebar(m, (g.uniform(WX0 + 0.1, WX1 - 0.1), g.uniform(0.6, 3.4), 2.03), (g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 1), g.uniform(0.15, 0.4), seed=20 + k, bent=0.5)

    # ---- the door, set back in its passage, in a steel frame
    hinge = Vector((XR + 0.75, DY + DW / 2 - 0.06, FLOOR + 0.04))
    for sy in (1, -1):
        m.box((XR + 0.81, DY + sy * (DW / 2 - 0.03), (FLOOR + 1.42) / 2), (0.05, 0.07, 1.42 - FLOOR), "steel")
        bits.bolts(m, (XR + 0.785, DY + sy * (DW / 2 - 0.03), FLOOR + 0.15), (XR + 0.785, DY + sy * (DW / 2 - 0.03), 1.3), 6, (-1, 0, 0), 0.012, 0.008)
    m.box((XR + 0.81, DY, 1.385), (0.05, DW, 0.07), "steel")
    m.part("door", "bunker", hinge, axis=(0, 0, -1), limits=[0.0, 105.0], kind="turn", note="the steel door: plus swings it out into the passage")
    m.use("door")
    steel_door(m, hinge, 180, DW - 0.12, 1.76)
    m.use("bunker")

    # ---- inside: what shows through the embrasure and the door
    lamp(m, (0.0, 0.0, CEIL - 0.12))
    lamp(m, (-1.7, 1.8, CEIL - 0.12))
    bits.tube(m, [(-1.6, 1.4, CEIL + 0.3), (-1.6, 1.4, CEIL - 0.18), (-1.6, 0.6, CEIL - 0.18), (-2.2, 0.6, CEIL - 0.18), (-2.2, -2.4, CEIL - 0.18)], 0.09, "upper", 14 if fine else 6)
    for yy in (-0.6, -1.6):
        m.box((-2.2, yy, CEIL - 0.3), (0.12, 0.2, 0.06), "iron")
    # ready rounds in a rack along the right wall, and two ammunition boxes
    rk = -RY + 0.22
    for xx in (-1.8, -0.9, 0.0):
        m.rod((xx, rk, FLOOR), (xx, rk, FLOOR + 1.1), 0.02, "upper", sides=6)
        m.rod((xx, rk - 0.18, FLOOR), (xx, rk - 0.18, FLOOR + 1.1), 0.02, "upper", sides=6)
    for zz in (0.25, 0.65, 1.05):
        for yy in (rk, rk - 0.18):
            m.rod((-1.85, yy, FLOOR + zz), (0.05, yy, FLOOR + zz), 0.015, "upper", sides=6)
        for k in range(10 if fine else 4):
            shell(m, (-1.75 + k * 0.17 * (1 if fine else 2.5), rk + 0.42, FLOOR + zz + 0.06), (0, -1, 0.02), 0.075, 0.62)
    for k, (xx, yy) in enumerate(((0.6, -2.5), (1.1, -2.55))):
        m.box((xx, yy, FLOOR + 0.17), (0.42, 0.28, 0.34), "upper", yaw=8 * k)
        m.box((xx, yy, FLOOR + 0.35), (0.44, 0.3, 0.025), "upper", yaw=8 * k)
    # a bench and a telephone on the left wall, by the machine gun's niche
    m.box((-0.6, RY - 0.25, FLOOR + 0.45), (1.2, 0.36, 0.05), "timber")
    for xx in (-1.1, -0.1):
        m.box((xx, RY - 0.25, FLOOR + 0.21), (0.05, 0.3, 0.42), "timber.dark")
    m.box((0.6, RY - 0.06, FLOOR + 1.35), (0.22, 0.12, 0.3), "wood.paint")
    m.rod((0.6, RY - 0.13, FLOOR + 1.36), (0.6, RY - 0.13, FLOOR + 1.22), 0.02, "rubber", sides=6)

    # ---- the apron's joints, chips knocked off the face lying on it, a steel frame round the throat
    def apron_z(x):
        return 0.11 - (x - (XF - 0.2)) * (0.075 / 3.4)
    for xx in (XF + 1.1, XF + 2.2):
        hw = 2.7 + (xx - XF + 0.2) * (1.2 / 3.4) - 0.1
        m.box((xx, 0.0, apron_z(xx) + 0.001), (0.025, 2 * hw, 0.008), "concrete.old")
    m.box((XF + 1.7, 0.0, apron_z(XF + 1.7) + 0.001), (3.0, 0.025, 0.008), "concrete.old")
    if fine:
        g1 = rnd(77)
        for k in range(26):
            xx, yy = XF + g1.uniform(0.2, 3.0), g1.uniform(-2.6, 2.6)
            r = g1.uniform(0.025, 0.07)
            m.ball((xx, yy, apron_z(xx) + r * 0.3), (r * g1.uniform(0.8, 1.4), r * g1.uniform(0.8, 1.3), r * 0.6), g1.choice(("concrete", "concrete", "concrete.in")), 6, 4)
    tx = XF - 1.44 + 0.012
    for zz in (EZ0 + 0.04, EZ1 - 0.04):
        m.box((tx, 0.0, zz), (0.024, 2.66, 0.08), "steel")
    for yy in (-1.29, 1.29):
        m.box((tx, yy, (EZ0 + EZ1) / 2), (0.024, 0.08, EZ1 - EZ0), "steel")
    if fine:
        for zz in (EZ0 + 0.04, EZ1 - 0.04):
            bits.bolts(m, (tx + 0.012, -1.2, zz), (tx + 0.012, 1.2, zz), 11, (1, 0, 0), 0.012, 0.01)

    # ---- the machine gun in the left flank
    pivot = Vector((MGX, YS - 1.2, MGZ))
    embrasure_gun(m, "bunker", pivot, 90.0, "air", "mg")
    m.use("bunker")

    # ---- sockets and crew
    gun_at = Vector((0.75, 0.0, FLOOR + 0.01))
    m.socket("gun", gun_at, (1, 0, 0), part="bunker",
             note="the anti-tank or field gun's place: its origin on the floor under the middle of its axle, x toward the muzzle. "
                  "Room: the throat (2.6 m wide, 1.1 m high, its sill 0.7 m above the floor) is 1.45 m ahead of the axle; 2.4 m clear either side; "
                  "3.15 m to the back wall for the trails; 1.9 m under the ceiling. The embrasure's steps let the barrel traverse 30 degrees either way. "
                  "Of the vehicle modeller's towed guns as measured in action: Gun75, 25pdr and ZiS3 stand clear, the leFH18's spades touch the back wall, "
                  "the Howitzer105 is 0.4 m too long and does not go in.")
    m.socket("embrasure.main", (XF - 1.44, 0.0, (EZ0 + EZ1) / 2), (1, 0, 0), part="bunker", note="the throat of the main embrasure: where the gun's muzzle stands when laid")
    bits.crew(m, "layer", gun_at + Vector((-0.55, 0.55, 0.0)), (1, 0, 0), "kneel", "bunker", "kneeling at the left of the breech, eye to the sight, working the elevation")
    bits.crew(m, "loader", gun_at + Vector((-0.75, -0.55, 0.0)), (1, 0.2, 0), "kneel", "bunker", "kneeling at the right of the breech, a round in his arms")
    bits.crew(m, "ammo.1", gun_at + Vector((-1.6, -1.3, 0.0)), (0.6, 0.8, 0), "stand", "bunker", "at the rack on the right wall, passing rounds forward")
    bits.crew(m, "commander", gun_at + Vector((-2.4, 0.3, 0.0)), (1, 0, 0), "stand", "bunker", "behind the trails: the gun's commander, watching the fall of shot over the shield")
    bits.crew(m, "mg.gunner", Vector((MGX, RY - 0.35, FLOOR)), (0, 1, 0), "stand", "bunker", "standing in the niche at the machine gun, firing out of the left flank")
    m.socket("hit", (XF + 0.1, 0.0, 1.2), (1, 0, 0), part="bunker", note="where the shell or the charge strikes the front")
    m.socket("smoke", (XF + 0.2, 0.0, 1.3), (0.5, 0, 1), part="bunker", note="smoke pours out of the embrasure and the break")
    m.socket("fire", (0.2, 0.0, 0.2), (0, 0, 1), part="bunker", note="what burns inside shows here")
    m.socket("door", (XR + 0.3, DY, FLOOR), (-1, 0, 0), part="bunker", note="the doorway: men go in and out here")

    # ---- rubble hidden until the break
    m.part("rubble", "bunker", (XF + 1.5, 0.0, 0.2), kind="free", note="hidden (scale 0) until the front is blown in")
    m.use("rubble")
    rubble(m, (XF + 1.6, 0.0, 0.12), 2.2, 46, seed=4, size=(0.15, 0.55))
    rubble(m, (1.0, 0.0, FLOOR + 0.05), 1.2, 16, seed=5, size=(0.12, 0.35))
    if fine:
        for k in range(8):           # bent rods torn out with the face
            a = g.uniform(-1, 1)
            p = Vector((XF + 1.2 + g.uniform(0, 2.0), a * 2.0, 0.15))
            bits.tube(m, [p, p + Vector((g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 0.25)), p + Vector((g.uniform(-0.6, 0.6), g.uniform(-0.6, 0.6), 0.35))], 0.009, "rust", 5)
    m.use("bunker")

    q1 = Quaternion(Vector((0.05, 1, 0)).normalized(), math.radians(34.0))         # the face falls outward on its foot
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing; the rubble hidden.", "tracks": [{"part": "rubble", "scale": [[0.0, 0.0]]}], "events": []}
    m.clips["fire"] = gun_clip("mg")
    m.clips["fire"]["tracks"].append({"part": "rubble", "scale": [[0.0, 0.0]]})
    m.clips["fire"]["about"] = "The flank machine gun fires a burst, looping while it fires. (The anti-tank gun at `gun` is the vehicle modeller's and fires by its own clip.)"
    m.clips["knocked_out"] = {
        "long": 2.8, "loop": False, "after": "hold",
        "about": "A heavy shell or a charge at the embrasure: the face round it and the roof lip are blown out and fall forward on the apron, rubble is "
                 "thrown out and down into the room, the door is blown open, smoke pours out, the inside burns. (At rest play `idle`, which keeps the rubble hidden.)",
        "tracks": [{"part": "front", "rot": [[0.0, 1, 0, 0, 0], [0.22, *Quaternion().slerp(q1, 0.35)], [0.65, *Quaternion().slerp(q1, 0.85)], [0.85, *q1], [0.97, *Quaternion().slerp(q1, 0.95)], [1.1, *q1]],
                    "move": [[0.0, 0, 0, 0], [0.22, 0.3, 0.0, 0.08], [0.65, 0.65, 0.03, -0.12], [0.85, 0.8, 0.03, -0.3]]},
                   {"part": "rubble", "scale": [[0.0, 0.0], [0.1, 0.0], [0.28, 1.0]], "move": [[0.0, -1.2, 0, 0.9], [0.1, -1.2, 0, 0.9], [0.35, -0.4, 0, 0.6], [0.8, 0.0, 0, 0.0]]},
                   {"part": "door", "turn": [[0.0, 0.0], [0.1, 0.0], [0.32, 100.0], [0.45, 86.0], [0.62, 95.0]]},
                   {"part": "mg.gun", "turn": [[0.0, 0.0], [0.15, -9.0], [2.8, -9.0]]}],
        "events": [{"t": 0.0, "fx": "shell_burst_big", "socket": "hit", "cue": "explosion_debris"},
                   {"t": 0.05, "fx": "debris", "socket": "hit", "cue": ""},
                   {"t": 0.3, "fx": "smoke_black", "socket": "smoke", "cue": "building_collapse"},
                   {"t": 0.7, "fx": "fire", "socket": "fire", "cue": "fire_start", "cue_loop": "fire_small_loop"}]}
    weather(m, [((XF + 0.3, 0.0, 1.2), 1.6, 0.55), ((MGX, YS + 0.1, MGZ + 0.3), 0.6, 0.4)], splash=1.0)
    m.info["span"] = 12.5
    m.info["high"] = 1.6
    m.info["aim"] = (0.3, 0.0, 1.5)
    m.info["worn"] = [(XF + 2.6, 0.0, 4.0), (XR - 1.0, 5.4, 2.6), (XR - 3.0, 6.5, 2.0)]
    m.info["clear"] = [[(XR - 0.1, -YS - 0.1), (XF + 3.3, -YS - 0.1), (XF + 3.3, YS + 0.1), (XR - 0.1, YS + 0.1)], [(WX0 - 0.1, 0.3), (XR + 0.5, 0.3), (XR + 0.5, WY + 0.2), (WX0 - 0.1, WY + 0.2)]]
    m.info["dig"] = [[(WX1 - 0.1, 0.85), (XR + 0.3, 0.85), (XR + 0.3, WY - 0.35), (WX1 - 0.1, WY - 0.35)]]
    m.info["dig_depth"] = -FLOOR
    return m


# ---- the coastal battery ---------------------------------------------------------------------------------------

def projectile(m, base, axis, cal=0.15, long=0.62, paint="upper"):
    """A separate-loading shell: the body, the copper driving band, the ogive and its fuze."""
    base, axis = Vector(base), Vector(axis).normalized()
    r = cal / 2
    n = 16 if m.fine else 7
    m.lathe(base, axis, [(0.0, r * 0.92), (0.015, r), (long * 0.10, r), (long * 0.10, r * 1.035), (long * 0.15, r * 1.035), (long * 0.15, r), (long * 0.52, r),
                         (long * 0.64, r * 0.93), (long * 0.76, r * 0.74), (long * 0.86, r * 0.5)], paint, n, (True, False))
    m.lathe(base + axis * (long * 0.103), axis, [(0.0, r * 1.04), (long * 0.045, r * 1.04)], "brass", n, (False, False))
    m.lathe(base + axis * (long * 0.86), axis, [(0.0, r * 0.5), (long * 0.1, r * 0.3), (long * 0.14, r * 0.12)], "steel.worn", n, (False, True))


def charge_case(m, base, axis, cal=0.15, long=0.7):
    base, axis = Vector(base), Vector(axis).normalized()
    r = cal / 2 * 1.15
    n = 16 if m.fine else 7
    m.lathe(base, axis, [(0.0, r * 1.1), (0.015, r * 1.1), (0.02, r), (long * 0.97, r * 0.96), (long, r * 0.9)], "brass", n, (True, False))
    m.disc(base + axis * long, axis, r * 0.9, "timber.end", n)


def curved_plate_solid(c, r_out, r_in, z0, z1, a0, a1, sides=96, lean=0.0, roof=0.06):
    """A ring of steel standing on its edge between two angles (degrees), as a solid, its face leaning back by `lean`
    metres at the top, closed over by a roof plate: the shield."""
    k = lean / (z1 - z0)
    outer = bits.s_cyl((c.x, c.y, z0), Z, r_out, z1 - z0, sides, r_out - lean)
    zi1 = z1 - roof
    inner = bits.s_cyl((c.x, c.y, z0 - 0.3), Z, r_in + 0.3 * k, zi1 - z0 + 0.3, sides, r_in - k * (zi1 - z0))
    bits.s_cut(outer, inner)
    n = max(4, int(abs(a1 - a0) / 6))
    plan = [(c.x, c.y)] + [(c.x + math.cos(math.radians(a0 + (a1 - a0) * k / n)) * (r_out + 0.6), c.y + math.sin(math.radians(a0 + (a1 - a0) * k / n)) * (r_out + 0.6)) for k in range(n + 1)]
    sector = bits.s_prism(plan, z0 - 0.5, z1 + 0.5)
    bits.s_both(outer, sector)
    return outer


def coast_gun(m, parent, at):
    """A 150 mm class coastal gun on a central pivot pedestal, muzzle toward +x: drum pedestal, a mount that turns with
    its curved shield, a cradle with recoil cylinders above and below, a long barrel that runs back, a sliding breech
    block. Parts: pedestal, mount, cradle, barrel, breech, handwheel.traverse, handwheel.elevate."""
    fine = m.fine
    O = Vector(at)
    sides = 40 if fine else 14
    # ---- the pedestal: a drum bolted to the floor
    m.part("pedestal", parent, O, kind="fixed")
    m.use("pedestal")
    m.lathe(O, Z, [(0.0, 1.18), (0.07, 1.18), (0.08, 1.04), (0.14, 0.98), (0.80, 0.86), (0.84, 0.94), (0.93, 0.94), (0.94, 0.92)], "upper", sides)
    bits.bolt_ring(m, O + Z * 0.07, Z, 1.11, 28, 0.03, 0.035, "steel")
    for k in range(12 if fine else 6):
        a = math.radians((30 if fine else 60) * k + 15)
        d = Vector((math.cos(a), math.sin(a), 0))
        bits.plate(m, [(0.90, 0.0), (1.12, 0.0), (1.12, 0.035), (0.88, 0.42)], 0.025, "upper", O + Z * 0.07, d, Z)
    m.lathe(O + Z * 0.48 + X * 0.0, -X, [(0.88, 0.16), (0.95, 0.16), (0.96, 0.14)], "upper", 16, (False, True))        # an inspection cover behind
    bits.bolt_ring(m, O + Vector((-0.955, 0, 0.48)), -X, 0.12, 8, 0.012, 0.01)
    P = O + Z * 0.94
    # ---- the mount: turntable, deck, carriage cheeks and the shield
    m.part("mount", "pedestal", P, axis=(0, 0, -1), limits=[-60.0, 60.0], kind="turn", note="traverse: plus swings the muzzle to the right (clockwise seen from above), 60 degrees either way")
    m.use("mount")
    m.lathe(P, Z, [(0.0, 0.98), (0.04, 0.98), (0.045, 0.9), (0.14, 0.9), (0.145, 0.86)], "upper", sides)
    bits.bolt_ring(m, P + Z * 0.041, Z, 0.94, 30, 0.016, 0.014, "steel")
    deck_z = P.z + 0.16
    from guns import annulus, seat
    annulus(m, (O.x, O.y, deck_z), 0.86, 2.02, 0.035, "upper", 48 if fine else 16)
    annulus(m, (O.x, O.y, deck_z - 0.035), 1.95, 2.02, 0.09, "upper", 48 if fine else 16)
    if fine:
        for ring, cnt in ((1.0, 30), (1.2, 36), (1.4, 42), (1.6, 48), (1.8, 54)):
            for k in range(cnt):
                a = 2 * math.pi * (k + 0.5 * (cnt % 3)) / cnt
                c = Vector((O.x + math.cos(a) * ring, O.y + math.sin(a) * ring, deck_z))
                m.add([c + Vector((0.035, 0, 0.004)), c + Vector((0, 0.014, 0.004)), c + Vector((-0.035, 0, 0.004)), c + Vector((0, -0.014, 0.004))], [(0, 1, 2, 3)], "upper", False, lambda p: Z)
    for k in range(10 if fine else 0):
        a = math.radians(36 * k + 18)
        d = Vector((math.cos(a), math.sin(a), 0))
        m.rod(P + d * 0.9 + Z * 0.05, Vector((O.x, O.y, deck_z - 0.04)) + d * 1.5, 0.035, "upper", sides=8)
    T = O + Vector((0.32, 0.0, 0.0))
    T.z = deck_z + 0.60
    cheek = [(-0.95, 0.0), (0.95, 0.0), (0.85, 0.35), (0.45, 0.68), (0.2, 0.86), (-0.2, 0.86), (-0.5, 0.66), (-0.85, 0.3)]
    for sy in (1, -1):
        face = Vector((T.x, O.y + sy * 0.62, deck_z))
        bits.plate(m, cheek, 0.05, "upper", face, X, Z)
        m.lathe(T + Y * sy * 0.62, Y * sy, [(0.0, 0.2), (0.07, 0.2), (0.085, 0.17), (0.10, 0.09), (0.12, 0.09)], "upper", 24 if fine else 10, (False, True))
        bits.bolt_ring(m, T + Y * sy * 0.71, Y * sy, 0.15, 10, 0.016, 0.014)
        bits.bolts(m, face + Vector((-0.88, sy * 0.026, 0.07)), face + Vector((0.88, sy * 0.026, 0.07)), 11, Y * sy, 0.016, 0.014)
        m.disc(face + Vector((-0.2, sy * 0.027, 0.38)), Y * sy, 0.14, "void", 16 if fine else 6)
        if fine:
            bits.tube(m, [face + Vector((-0.2 + math.cos(2 * math.pi * k / 18) * 0.15, sy * 0.028, 0.38 + math.sin(2 * math.pi * k / 18) * 0.15)) for k in range(18)], 0.016, "upper", 6, closed=True)
        m.rod(face + Vector((0.92, 0, 0.02)), face + Vector((0.3, 0, 0.8)), 0.035, "upper", sides=8)
    m.box(Vector((T.x + 0.85, O.y, deck_z + 0.25)), (0.06, 1.2, 0.5), "upper")
    bits.bolts(m, Vector((T.x + 0.885, O.y - 0.5, deck_z + 0.4)), Vector((T.x + 0.885, O.y + 0.5, deck_z + 0.4)), 9, X, 0.016, 0.014)
    # the shield: a curved face standing on the deck, its roof, a slot for the gun to lift in, a sight port
    z0, z1 = deck_z - 0.02, deck_z + 2.25
    RS = 2.12
    LEAN = 0.42
    sh = curved_plate_solid(O, RS, RS - 0.08, z0, z1, -70.0, 70.0, 96 if fine else 32, LEAN, 0.07)
    slot = bits.s_box((O.x + 0.25, O.y - 0.36, deck_z + 0.12), (O.x + RS + 0.5, O.y + 0.36, z1 + 0.4))
    port = bits.s_box((O.x + RS - 0.4, -0.12, -0.08), (O.x + RS + 0.4, 0.12, 0.08))
    bits.s_move(port, (0, 0, deck_z + 1.15))
    port.data.transform(Matrix.Translation((O.x, O.y, 0)) @ Matrix.Rotation(math.radians(24.0), 4, "Z") @ Matrix.Translation((-O.x, -O.y, 0)))
    bits.s_cut(sh, slot, port)
    bits.s_bevel(sh, 0.012, 1)
    bits.take(m, sh, "upper")
    # butt straps over the joints of the shield's plates, bolted; bolts round its foot and along the roof's edge
    for a in (-38.0, 38.0, -66.0, 66.0):
        ar = math.radians(a)
        d, tng = Vector((math.cos(ar), math.sin(ar), 0)), Vector((-math.sin(ar), math.cos(ar), 0))
        up = (Z * (z1 - z0) - d * LEAN).normalized()
        out = tng.cross(up).normalized()
        if out.dot(d) < 0:
            out = -out
        bits.plate(m, [(-0.08, 0.0), (0.08, 0.0), (0.08, (z1 - z0) / up.z - 0.03), (-0.08, (z1 - z0) / up.z - 0.03)], 0.014, "upper", O + d * (RS + 0.004) + Z * z0 + out * 0.004, tng, up)
        for kk in range(10):
            f = (0.12 + kk * (z1 - z0 - 0.24) / 9) / (z1 - z0)
            for t in (-0.05, 0.05):
                bits.bolt(m, O + d * (RS - LEAN * f + 0.012) + tng * t + Z * (z0 + f * (z1 - z0)), out, 0.013, 0.012)
    if fine:
        for k in range(-34, 35):
            ar = math.radians(k * 2.0)
            d = Vector((math.cos(ar), math.sin(ar), 0))
            if abs(k) % 2 == 0:
                bits.bolt(m, O + d * RS + Z * (z0 + 0.06), d, 0.014, 0.012)
            if abs(k) % 3 == 0 and abs(math.sin(ar) * RS) > 0.42:
                bits.bolt(m, O + d * (RS - LEAN - 0.06) + Z * z1, Z, 0.013, 0.012)
    # the sight port's hinged cover, swung up
    ar = math.radians(24.0)
    d, tng = Vector((math.cos(ar), math.sin(ar), 0)), Vector((-math.sin(ar), math.cos(ar), 0))
    pc = O + d * (RS - LEAN * 1.25 / 2.27 + 0.03) + Z * (deck_z + 1.25)
    bits.plate(m, [(-0.18, 0.0), (0.18, 0.0), (0.18, 0.24), (-0.18, 0.24)], 0.02, "upper", pc, tng, (d * 0.7 + Z * 0.7).normalized())
    m.rod(pc - tng * 0.2, pc + tng * 0.2, 0.014, "steel", sides=8)
    # the layers' seats, gear columns and dials; the shaft into the mounting
    for sy, what in ((1, "traverse"), (-1, "elevate")):
        s_at = Vector((T.x - 0.75, O.y + sy * 1.02, deck_z + 0.46))
        seat(m, s_at, X)
        col = Vector((T.x - 0.18, O.y + sy * 0.98, deck_z))
        m.rod(col, col + Z * 0.88, 0.04, "upper", sides=10)
        m.box(col + Z * 0.93, (0.22, 0.18, 0.18), "upper")
        m.rod(col + Z * 0.93, Vector((T.x - 0.1, sy * 0.66, deck_z + 0.5)), 0.022, "upper", sides=6)
        bits.crew(m, "layer." + what, s_at, (1, 0, 0), "sit", "mount", "sitting on the mount's seat, turning the " + what + " handwheel", seat_h=0.46,
                  grips={"l": (col + Vector((-0.155, 0.12, 0.93)), (1, 0, 0), "mount"), "r": (col + Vector((-0.155, -0.12, 0.93)), (1, 0, 0), "mount")})
        m.rod(Vector((T.x - 0.05, O.y + sy * 0.86, deck_z + 0.03)), Vector((T.x - 0.05, O.y + sy * 1.22, deck_z + 0.03)), 0.016, "upper", sides=6)
    # the loading tray, swung out to the right of the breech on its arm, rollers in it
    tr0 = Vector((T.x - 2.55, O.y - 0.78, T.z - 0.22))
    for (a, b) in (((0.0, -0.13), (0.0, 0.0)), ((0.0, 0.0), (0.26, 0.0)), ((0.26, 0.0), (0.26, -0.13))):
        pass
    bits.plate(m, [(0.0, 0.0), (1.25, 0.0), (1.25, 0.03), (0.0, 0.03)], 0.26, "upper", tr0, X, Z)
    for sy in (1, -1):
        bits.plate(m, [(0.0, 0.0), (1.25, 0.0), (1.25, 0.12), (0.0, 0.12)], 0.015, "upper", tr0 + Y * sy * 0.13, X, Z)
    if fine:
        for k in range(6):
            m.rod(tr0 + Vector((0.1 + k * 0.21, -0.12, 0.06)), tr0 + Vector((0.1 + k * 0.21, 0.12, 0.06)), 0.025, "steel.worn", sides=10)
    m.rod(tr0 + Vector((0.6, 0, 0.0)), Vector((T.x - 0.6, -0.62, deck_z + 0.3)), 0.03, "upper", sides=8)
    m.rod(tr0 + Vector((0.6, 0, 0.0)), Vector((tr0.x + 0.6, tr0.y, deck_z)), 0.035, "upper", sides=8)
    m.socket("fire", P + Z * 0.8, (0, 0, 1), part="mount", note="where she burns when she is knocked out")
    # ---- the handwheels
    for sy, what in ((1, "traverse"), (-1, "elevate")):
        c = Vector((T.x - 0.18 - 0.13, O.y + sy * 0.98, deck_z + 0.93))
        m.part("handwheel." + what, "mount", c, axis=(-1, 0, 0), kind="spin", note="the game spins it while the gun " + ("trains" if sy > 0 else "elevates"))
        m.use("handwheel." + what)
        bits.handwheel(m, c + Vector((-0.02, 0, 0)), -X, 0.2, "steel", 5, 0.014)
    # ---- the cradle: lifts on the trunnions
    m.part("cradle", "mount", T, axis=(0, -1, 0), limits=[-5.0, 35.0], kind="turn", note="elevation: plus lifts the muzzle, from 5 below level to 35 above")
    m.use("cradle")
    m.rod(T - Y * 0.62, T + Y * 0.62, 0.11, "steel", sides=24 if fine else 10)
    m.lathe(T + X * (-0.75), X, [(0.0, 0.27), (0.03, 0.31), (2.2, 0.31), (2.25, 0.29), (2.3, 0.29)], "upper", sides)         # the sleeve the gun runs in
    for sx in (-0.7, 0.2, 1.1):
        m.lathe(T + X * sx, X, [(0.0, 0.33), (0.12, 0.33)], "upper", sides, (True, True))
    # the recuperator above, the buffer below, with their yokes
    m.lathe(T + Vector((-0.95, 0, 0.47)), X, [(0.0, 0.07), (0.02, 0.12), (2.5, 0.12), (2.52, 0.09), (2.6, 0.06), (2.64, 0.06)], "upper", 24 if fine else 10)
    m.lathe(T + Vector((-0.95, 0, -0.48)), X, [(0.0, 0.07), (0.02, 0.135), (2.45, 0.135), (2.47, 0.1), (2.55, 0.06), (2.6, 0.06)], "upper", 24 if fine else 10)
    for sx in (-0.6, 0.5, 1.4):
        for sz in (1, -1):
            m.box(T + Vector((sx, 0, sz * 0.38)), (0.1, 0.22, 0.16), "upper")
            bits.bolt(m, T + Vector((sx, 0.08, sz * 0.38 + sz * 0.08)), Z * sz, 0.014, 0.012)
            bits.bolt(m, T + Vector((sx, -0.08, sz * 0.38 + sz * 0.08)), Z * sz, 0.014, 0.012)
    m.lathe(T + Vector((0.9, 0, 0.59)), Z, [(0.0, 0.03), (0.03, 0.03), (0.035, 0.045), (0.05, 0.045)], "brass", 8)
    # the elevating arc on the right, toothed
    arc_r = 1.0
    aa = [math.radians(a) for a in range(-150, -59, 5)]
    yy = T.y - 0.36
    outer = [Vector((T.x + math.cos(a) * arc_r, yy, T.z + math.sin(a) * arc_r)) for a in aa]
    inner = [Vector((T.x + math.cos(a) * (arc_r - 0.12), yy, T.z + math.sin(a) * (arc_r - 0.12))) for a in aa]
    n = len(aa)
    for dy, way in ((0.0, Y), (-0.05, -Y)):
        m.add([p + Y * dy for p in outer] + [p + Y * dy for p in inner], [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)], "steel", False, lambda p, w=way: w)
    m.add(outer + [p - Y * 0.05 for p in outer], [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)], "steel", True, lambda p: Vector((p.x - T.x, 0, p.z - T.z)))
    m.add(inner + [p - Y * 0.05 for p in inner], [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)], "steel", True, lambda p: -Vector((p.x - T.x, 0, p.z - T.z)))
    if fine:
        for a in range(-149, -60, 3):
            ar = math.radians(a)
            c = Vector((T.x + math.cos(ar) * (arc_r + 0.014), yy - 0.025, T.z + math.sin(ar) * (arc_r + 0.014)))
            m.box(c, (0.028, 0.05, 0.024), "steel", pitch=a + 90)
    for a in (-140, -70):
        ar = math.radians(a)
        m.rod(Vector((T.x + math.cos(ar) * (arc_r - 0.06), yy - 0.02, T.z + math.sin(ar) * (arc_r - 0.06))), T + Vector((0.4 * math.cos(ar), -0.3, 0.4 * math.sin(ar))), 0.035, "upper", sides=8)
    # the sight on the left, its telescope looking out through the port
    m.box(T + Vector((0.1, 0.42, 0.18)), (0.3, 0.1, 0.12), "upper")
    m.lathe(T + Vector((-0.35, 0.52, 0.32)), X, [(0.0, 0.045), (0.05, 0.05), (0.75, 0.05), (0.78, 0.06), (0.9, 0.06)], "upper", 16 if fine else 8)
    m.disc(T + Vector((-0.351, 0.52, 0.32)), -X, 0.03, "glass", 12)
    m.rod(T + Vector((0.1, 0.42, 0.24)), T + Vector((0.1, 0.52, 0.32)), 0.025, "upper", sides=8)
    # ---- the barrel: runs back in the sleeve when she fires
    m.part("barrel", "cradle", T, slide=(-1, 0, 0), slide_limits=[0.0, 0.9], kind="slide", note="recoil: metres back along the bore as the cradle has lifted it")
    m.use("barrel")
    B = T + X * (-1.22)
    prof = [(0.0, 0.25), (0.02, 0.255), (0.48, 0.255), (0.5, 0.262), (0.56, 0.262), (0.58, 0.245), (2.85, 0.228), (2.88, 0.24), (3.0, 0.24), (3.03, 0.215), (5.6, 0.18),
            (6.9, 0.152), (6.96, 0.152), (7.02, 0.168), (7.16, 0.172), (7.22, 0.165), (7.26, 0.15)]
    m.lathe(B, X, prof, "upper", sides, (True, False))
    m.lathe(B + X * 7.26, -X, [(0.0, 0.15), (0.006, 0.078), (0.7, 0.075)], "void", 24 if fine else 10, (False, True))
    if fine:
        m.lathe(B + X * 7.255, X, [(0.0, 0.15), (0.004, 0.152)], "steel.worn", sides, (False, False))
    ring = bits.s_box((B.x - 0.1, T.y - 0.34, T.z - 0.34), (B.x + 0.52, T.y + 0.34, T.z + 0.34))
    slt = bits.s_box((B.x - 0.02, T.y - 0.5, T.z - 0.21), (B.x + 0.3, T.y + 0.5, T.z + 0.21))
    bore = bits.s_cyl((B.x - 0.3, T.y, T.z), X, 0.078, 1.0, 24 if fine else 10)
    bits.s_cut(ring, slt, bore)
    bits.s_bevel(ring, 0.015, 2 if fine else 1)
    bits.take(m, ring, "steel")
    m.lathe(Vector((B.x + 0.4, T.y, T.z + 0.34)), Z, [(0.0, 0.05), (0.08, 0.05), (0.085, 0.03), (0.13, 0.03)], "steel", 10)     # the lug the recuperator's rod takes
    m.rod(Vector((B.x + 0.4, T.y, T.z + 0.47)), Vector((T.x - 0.95, T.y, T.z + 0.47)), 0.035, "steel.worn", sides=10)
    m.rod(Vector((B.x + 0.4, T.y, T.z - 0.34)), Vector((B.x + 0.4, T.y, T.z - 0.48)), 0.045, "steel", sides=10)
    m.rod(Vector((B.x + 0.4, T.y, T.z - 0.48)), Vector((T.x - 0.95, T.y, T.z - 0.48)), 0.035, "steel.worn", sides=10)
    m.socket("muzzle", B + X * 7.3, (1, 0, 0), part="barrel")
    m.socket("breech", B + X * (-0.15), (-1, 0, 0), part="barrel", note="where the shell and its charge go in, rammed home from the tray")
    # ---- the breech block: a wedge that slides across to the right to open
    m.part("breech", "barrel", B + X * 0.14, slide=(0, -1, 0), slide_limits=[0.0, 0.36], kind="slide", note="plus opens the breech")
    m.use("breech")
    blk = bits.s_box((B.x + 0.0, T.y - 0.34, T.z - 0.2), (B.x + 0.28, T.y + 0.4, T.z + 0.2))
    bits.s_bevel(blk, 0.012, 2 if fine else 1)
    bits.take(m, blk, "steel.worn")
    m.box(Vector((B.x + 0.14, T.y + 0.405, T.z)), (0.16, 0.014, 0.24), "steel")
    m.rod(Vector((B.x - 0.0, T.y + 0.2, T.z + 0.05)), Vector((B.x - 0.12, T.y + 0.2, T.z + 0.05)), 0.02, "steel", sides=8)
    m.rod(Vector((B.x - 0.12, T.y + 0.2, T.z + 0.05)), Vector((B.x - 0.12, T.y - 0.25, T.z - 0.15)), 0.018, "steel", sides=8)
    m.ball(Vector((B.x - 0.12, T.y - 0.25, T.z - 0.15)), 0.035, "timber.dark", 10, 6)
    return {"T": T, "P": P, "O": O, "deck": deck_z, "B": B}


def CoastalBattery(fine=True, nation="_default", name="CoastalBattery"):
    """A coastal gun casemate with its gun: an open-fronted block under a 2 m roof slab, the front opening stepped and
    splayed in plan and in height for a 120 degree arc, the roof's front edge stepped and overhanging; the gun on its
    central pivot; the ammunition door at the rear behind a wing wall; earth over the back half and against the sides.
    x looks out to sea."""
    m = bits.model(name, "Coastal battery casemate and gun (our own model)", "works", nation, fine)
    m.part("battery", None, (0, 0, 0), kind="root")
    m.use("battery")
    seg = 2 if fine else 1
    XR, XE, XL = -6.0, 5.6, 6.65            # the rear face; where the side walls end in front; the lip's front edge
    TOP, SOF, FOOT, FLOOR = 6.0, 4.0, -0.9, 0.15
    YR, YF = 5.4, 7.3                       # half the width at the rear and (at the lip) in front: the walls taper back

    def yside(x):
        return YR + (YF - YR) * (x - (XR - 0.2)) / (XL + 0.25 - (XR - 0.2))
    plan = [(XL + 0.25, -yside(XL + 0.25)), (XL + 0.25, yside(XL + 0.25)), (XR - 0.2, YR), (XR - 0.2, -YR)]
    # the section: the walls to the roof, the roof's front edge stepped down in three (anti-ricochet steps)
    prof = [(XR, FOOT), (XL, FOOT), (XL, 5.16), (6.12, 5.16), (6.12, 5.44), (5.58, 5.44), (5.58, 5.72), (5.04, 5.72), (5.04, TOP), (XR, TOP)]
    body = s_xz(prof, -YF - 0.3, YF + 0.3)
    bits.s_both(body, bits.s_prism(plan, FOOT - 1.0, TOP + 1.0))
    # a hood cast over the rear doorway's bay
    bits.s_join(body, s_xz([(XR - 0.75, 3.42), (XR + 0.3, 3.42), (XR + 0.3, 3.86), (XR - 0.75, 3.74)], -2.35, 2.35))
    RX0, RX1, RY = -2.8, 3.3, 3.72          # the gun room, open in front at its throat
    room = bits.s_box((RX0, -RY, FLOOR), (RX1 + 0.02, RY, SOF))
    # the opening: in plan each step splays wider, in height each lifts the soffit, so the gun has 60 degrees either side
    opening = [bits.s_box((RX1, -RY - 0.03, FLOOR - 0.02), (XL + 1.0, RY + 0.03, 4.25)),
               bits.s_box((4.0, -4.72, FLOOR - 0.04), (XL + 1.0, 4.72, 4.62)),
               bits.s_box((4.7, -5.72, FLOOR - 0.06), (XL + 1.0, 5.72, 4.98)),
               bits.s_box((5.36, -6.72, FLOOR - 0.08), (XL + 1.0, 6.72, 5.3))]
    under = bits.s_box((XE, -YF - 1.0, FOOT - 0.5), (XL + 1.0, YF + 1.0, 4.45))           # the lip overhangs the walls' ends
    passage = bits.s_box((XR - 0.5, -0.85, FLOOR + 0.02), (RX0 + 0.1, 0.85, 2.55))
    store_l = bits.s_box((-2.4, RY - 0.05, FLOOR + 0.03), (0.6, RY + 0.7, 2.4))           # a recess in each wall: ready shells, charges
    store_r = bits.s_box((-2.4, -RY - 0.7, FLOOR + 0.03), (0.0, -RY + 0.05, 2.4))
    cut = room
    bits.s_join(cut, *opening)
    bay = bits.s_box((XR - 1.0, -1.9, 0.12), (XR + 0.45, 1.9, 3.3))                         # the door's bay, set into the rear face
    vents = [bits.s_box((XR - 0.5, sy * 3.6 - 0.36, 4.25), (XR + 0.28, sy * 3.6 + 0.36, 4.75)) for sy in (1, -1)]
    bits.s_cut(body, cut, under, passage, store_l, store_r, bay, *vents)
    scars = []
    if fine:
        g0 = rnd(41)
        for (fx, fy, fz, r) in ((XL, -3.1, 5.0, 0.3), (XL, 2.2, 4.85, 0.24), (6.12, -5.6, 5.3, 0.26), (XE, 6.9, 2.8, 0.3), (XE, -7.0, 1.7, 0.24), (5.58, 4.4, 5.55, 0.2)):
            ob = bits.s_lathe((0.3, 0, 0), (-1, 0, 0), [(0.0, r * 1.3), (0.3, r * 1.1), (0.3 + r * 0.2, r * 0.75), (0.3 + r * 0.32, r * 0.35), (0.3 + r * 0.38, r * 0.05)], 9)
            ob.data.transform(Matrix.Diagonal((1.0, g0.uniform(0.7, 1.3), g0.uniform(0.6, 1.0), 1.0)))
            bits.s_move(ob, (fx, fy, fz), yaw=g0.uniform(-12, 12), roll=g0.uniform(0, 180))
            scars.append(ragged(ob, r * 0.22, r * 0.3, g0.uniform(0, 9)))
        bits.s_cut(body, *scars)
    bits.s_bevel(body, 0.045, seg)
    # what a hit in the opening brings down: a piece of the stepped lip on the left
    brk = ragged(bits.s_wedge([(4.75, 0.7, 4.3), (XL + 0.6, 0.4, 4.3), (XL + 0.6, 4.6, 4.3), (4.75, 4.2, 4.3)],
                              [(4.55, 0.85, TOP + 0.4), (XL + 0.6, 0.5, TOP + 0.4), (XL + 0.6, 4.4, TOP + 0.4), (4.55, 4.0, TOP + 0.4)]), 0.22 if fine else 0.7, 0.1, 5.0)
    lip, body = split(body, brk)

    def inside(p):
        return (RX0 - 0.05 < p.x < RX1 - 0.05 and abs(p.y) < RY + 0.75 and p.z < SOF + 0.03) or (XR + 1.0 < p.x < RX0 + 0.1 and abs(p.y) < 0.9 and p.z < 2.6) or \
            (XR - 0.1 < p.x < XR + 0.4 and abs(abs(p.y) - 3.6) < 0.4 and 4.2 < p.z < 4.8)
    paint = lambda mid, n, k: "concrete.in" if (k == 1 or inside(mid)) else "concrete"           # noqa: E731
    take_slots(m, body, paint, "battery")
    m.part("lip", "battery", (5.6, 2.5, 4.4), kind="free", note="a piece of the stepped roof lip over the opening: a hit in the opening brings it down")
    take_slots(m, lip, paint, "lip")
    m.use("battery")
    # the gun's bed: an eight-sided concrete pad with a steel ring let into it
    O = Vector((1.6, 0.0, FLOOR + 0.12))
    bed = [(O.x + math.cos(math.radians(22.5 + 45 * k)) * 1.7, O.y + math.sin(math.radians(22.5 + 45 * k)) * 1.7) for k in range(8)]
    bits.cast(m, bits.s_prism(bed, FLOOR - 0.2, O.z), "concrete", 0.03)

    # ---- the rear: the ammunition door set back in its passage, a wing wall across behind it
    hinge = Vector((XR + 1.0, 0.72, FLOOR + 0.04))
    for sy in (1, -1):
        m.box((XR + 1.06, sy * 0.81, (FLOOR + 2.55) / 2), (0.06, 0.08, 2.55 - FLOOR), "steel")
        bits.bolts(m, (XR + 1.03, sy * 0.81, FLOOR + 0.2), (XR + 1.03, sy * 0.81, 2.35), 8, (-1, 0, 0), 0.013, 0.01)
    m.box((XR + 1.06, 0.0, 2.51), (0.06, 1.7, 0.08), "steel")
    # the air vents' bars; a ladder up the rear face to the roof; a cable run in its conduit
    for sy in (1, -1):
        for k in range(7 if fine else 0):
            yy = sy * 3.6 - 0.3 + k * 0.6 / (6 if fine else 2)
            m.rod((XR + 0.12, yy, 4.25), (XR + 0.12, yy, 4.75), 0.014, "iron", sides=6)
        m.box((XR + 0.2, sy * 3.6, 4.5), (0.04, 0.72, 0.5), "void")
    ly = -4.4
    for sy in (1, -1):
        m.rod((XR - 0.2, ly + sy * 0.22, 0.05), (XR - 0.2, ly + sy * 0.22, TOP + 0.75), 0.02, "rust", sides=8)
        for zz in (0.9, 2.4, 3.9, 5.4):
            m.rod((XR - 0.2, ly + sy * 0.22, zz), (XR + 0.02, ly + sy * 0.22, zz), 0.015, "rust", sides=6)
        m.rod((XR - 0.2, ly + sy * 0.22, TOP + 0.75), (XR + 0.3, ly + sy * 0.22, TOP + 0.75), 0.02, "rust", sides=8)
        m.rod((XR + 0.3, ly + sy * 0.22, TOP + 0.75), (XR + 0.3, ly + sy * 0.22, TOP - 0.02), 0.02, "rust", sides=8)
    for k in range(int((TOP - 0.3) / 0.3)) if fine else ():
        m.rod((XR - 0.2, ly - 0.22, 0.35 + k * 0.3), (XR - 0.2, ly + 0.22, 0.35 + k * 0.3), 0.014, "rust", sides=6)
    bits.tube(m, [(XR - 0.08, 2.75, -0.1), (XR - 0.08, 2.75, TOP - 0.25), (XR - 0.08, 2.75, TOP - 0.1), (XR + 0.25, 2.75, TOP + 0.02)], 0.035, "upper", 10 if fine else 5)
    for zz in (0.8, 2.0, 3.2, 4.4, 5.4):
        m.box((XR - 0.05, 2.75, zz), (0.1, 0.14, 0.05), "steel")
    for t in (-4.8, -2.4, 2.0, 4.6):                     # drain spouts from the roof
        m.rod((XR + 0.1, t, TOP - 0.3), (XR - 0.35, t, TOP - 0.38), 0.045, "iron", sides=10)
    m.part("door", "battery", hinge, axis=(0, 0, -1), limits=[0.0, 100.0], kind="turn", note="the ammunition door: plus swings it out")
    m.use("door")
    steel_door(m, hinge, 180, 1.44, 2.32)
    m.use("battery")
    WX0, WX1 = XR - 3.1, XR - 2.5
    wing = s_yz([(-3.4, FOOT + 0.4), (3.4, FOOT + 0.4), (3.4, 1.4), (2.9, 2.6), (-2.9, 2.6), (-3.4, 1.4)], WX0, WX1)
    bits.cast(m, wing, "concrete", 0.04)
    # the light track the shell trolley runs on, from the door to the gun
    for sy in (1, -1):
        m.box(((XR + 0.2 + 0.0) / 2 - 0.4, sy * 0.3, FLOOR + 0.03), (abs(XR + 0.2) - 0.8 + 0.3, 0.05, 0.06), "steel.worn")
        if fine:
            for k in range(14):
                xx = XR + 0.5 + k * 0.42
                if xx < -0.4:
                    m.box((xx, sy * 0.3, FLOOR + 0.04), (0.1, 0.12, 0.012), "steel")

    # ---- the earth: over the back half of the roof, against the sides; the courtyard behind left bare
    fpoly = [(XE, -yside(XE)), (XE, yside(XE)), (XR, yside(XR)), (XR, -yside(XR))]

    def is_in(x, y):
        return poly_in(fpoly, x, y) and poly_near(fpoly, x, y)[0] > 1e-3

    def bump(x, y):
        u, v = (x + 2.5) / 4.6, y / (yside(x) + 0.5)
        return max(0.0, 1 - u * u) * max(0.0, 1 - v * v)

    def court(x, y):
        return rect_dist(x, y, XR - 3.8, XR, -YR, YR)

    def h_of(x, y):
        if is_in(x, y):
            h = TOP + 0.16 + 0.75 * bump(x, y) + 0.07 * bits.fbm((x * 0.6, y * 0.6, 3.0), 0.8)
            h -= (h - TOP + 0.12) * smooth((x + 0.9) / 1.4)
            # (it runs out short of the roof's edge, unless the bank outside comes up over the edge there)
            din, (ex, ey) = poly_near(fpoly, x, y)
            h = min(h, TOP - 0.05 + 1.0 * max(0.0, din - 0.6) + 3.0 * max(0.0, h_out(ex, ey) - TOP + 0.1))
            return min(h, TOP - 0.05 + 1.4 * max(0.0, x - (XR + 0.7)))
        return h_out(x, y)

    def h_out(x, y):
        d, _ = poly_near(fpoly, x, y)
        xc = min(max(x, XR), XE)
        E = (TOP + 0.15) * (1.0 - 0.6 * smooth((xc + 1.0) / 6.6))
        W = 1.2 + 0.9 * E
        h = E * (1.0 - smooth(d / W))
        if x > XE - 1e-4:
            if abs(y) < yside(XE) - 1e-3:
                return -1.0
            h = min(h, max(0.0, E - 0.8 * (x - XE)) + 0.7 * (abs(y) - yside(XE)))
        h += 0.12 * bits.fbm((x * 0.45, y * 0.45, 2.0), 0.7) * smooth(h / 0.5)
        if XR - 3.8 < x < XR + 1e-3 and abs(y) < YR:
            return -1.0
        return min(h, 0.03 + 0.62 * court(x, y) + 1.6 * max(0.0, x - XR))

    def hidden(x, y, h):
        if is_in(x, y):
            return h < TOP - 0.01
        return h < 0.012

    def snap(x, y):
        if poly_in(fpoly, x, y):
            return None
        d, at = poly_near(fpoly, x, y)
        return at if d < 0.48 * step else None

    step = 0.3 if fine else 1.2
    xs = [XR - 9.5 + step * k for k in range(int((XE + 5.5 - XR + 9.5) / step) + 1)]
    ys = [-14.8 + step * k for k in range(int(29.6 / step) + 1)]
    earth_field(m, h_of, xs, ys, hidden, 0.62, 2.0, "battery", inner=is_in, floor=TOP - 0.03, snap=snap, grass=1.6, scale=3.2)
    bits.patch(m, XR - 1.8, 0.0, 4.0, "earth.dry", seed=12, oval=0.8)
    bits.patch(m, XL + 2.2, 0.0, 5.0, "earth.dry", seed=13, oval=0.55)

    # ---- the roof: the ranging slit box, a ventilator, hooks for nets, rods left standing
    ob_at = Vector((-3.0, -2.6, 0.0))
    hz = h_of(ob_at.x, ob_at.y)
    box = bits.s_box((ob_at.x - 1.1, ob_at.y - 0.95, TOP - 0.2), (ob_at.x + 1.1, ob_at.y + 0.95, hz + 0.9))
    bits.s_join(box, s_xz([(ob_at.x - 1.25, hz + 0.86), (ob_at.x + 1.4, hz + 0.86), (ob_at.x + 1.4, hz + 1.12), (ob_at.x - 1.25, hz + 1.2)], ob_at.y - 1.1, ob_at.y + 1.1))
    slit = bits.s_box((ob_at.x - 0.6, ob_at.y - 0.7, hz + 0.55), (ob_at.x + 1.6, ob_at.y + 0.7, hz + 0.72))
    hollow = bits.s_box((ob_at.x - 0.75, ob_at.y - 0.6, TOP - 0.4), (ob_at.x + 0.75, ob_at.y + 0.6, hz + 0.8))
    bits.s_cut(box, slit, hollow)
    bits.cast(m, box, "concrete", 0.035, inside=lambda p: abs(p.x - ob_at.x) < 0.78 and abs(p.y - ob_at.y) < 0.63 and p.z < hz + 0.81)
    # a steel frame round the slit, bolted, and its shutter hanging open below it
    for zz in (hz + 0.53, hz + 0.74):
        m.box((ob_at.x + 1.11, ob_at.y, zz), (0.025, 1.5, 0.04), "upper")
    for yy in (-0.73, 0.73):
        m.box((ob_at.x + 1.11, ob_at.y + yy, hz + 0.635), (0.025, 0.04, 0.25), "upper")
    bits.bolts(m, (ob_at.x + 1.125, ob_at.y - 0.66, hz + 0.53), (ob_at.x + 1.125, ob_at.y + 0.66, hz + 0.53), 8, (1, 0, 0), 0.011, 0.008)
    bits.bolts(m, (ob_at.x + 1.125, ob_at.y - 0.66, hz + 0.74), (ob_at.x + 1.125, ob_at.y + 0.66, hz + 0.74), 8, (1, 0, 0), 0.011, 0.008)
    bits.plate(m, [(-0.7, 0.0), (0.7, 0.0), (0.7, 0.2), (-0.7, 0.2)], 0.02, "upper", (ob_at.x + 1.14, ob_at.y, hz + 0.5), Y, Vector((0.25, 0, -1)).normalized())
    m.lathe((ob_at.x + 0.2, ob_at.y, hz + 0.62), X, [(0.0, 0.05), (0.6, 0.05), (0.62, 0.065), (0.7, 0.065)], "upper", 12)       # the rangefinder's eyepiece tube
    ventilator(m, (-1.6, 2.4, h_of(-1.6, 2.4) - 0.25), 0.8)
    ventilator(m, (-4.2, 1.2, h_of(-4.2, 1.2) - 0.25), 0.6)
    hooks, ways = [], []
    for t in [-6.8 + 0.85 * k for k in range(17)]:
        if not (0.6 < t < 4.4):
            hooks.append((XL + 0.01, t, 4.95))
            ways.append((1, 0, 0))
    for sy in (1, -1):
        for xx in (0.5, 1.6, 2.7, 3.8, 4.8):
            hooks.append((xx, sy * (yside(xx) + 0.01), TOP - 0.15))
            ways.append((0, sy, 0))
    for t in (-4.5, -3.5, -2.5, -1.5, 1.5, 2.5, 3.5, 4.5):
        hooks.append((XR - 0.01, t, TOP - 0.15))
        ways.append((-1, 0, 0))
    net_hooks(m, hooks, ways)
    g = rnd(23)
    for k in range(10):
        rebar(m, (g.uniform(1.0, 4.8), g.choice((-1, 1)) * g.uniform(4.6, 6.4), TOP - 0.02), (g.uniform(-0.2, 0.2), g.uniform(-0.2, 0.2), 1), g.uniform(0.2, 0.5), seed=40 + k)
    for k in range(4):
        rebar(m, (g.uniform(WX0 + 0.1, WX1 - 0.1), g.uniform(-2.6, 2.6), 2.6), (0, 0, 1), g.uniform(0.15, 0.35), seed=60 + k, bent=0.5)

    # ---- inside: lamps, ready shells in the left recess, charges in the right, the trolley, tackle
    for at in ((0.0, 0.0), (-2.0, 2.0), (-2.0, -2.0)) if fine else ():
        lamp(m, (at[0], at[1], SOF - 0.12))
    for k in range(13 if fine else 5):
        xx = -2.15 + k * 0.215 * (1 if fine else 2.5)
        projectile(m, (xx, RY + 0.32, FLOOR + 0.12), Z, 0.15, 0.62)
    for zz in (0.12, 0.5):
        bits.angle_iron(m, (-2.35, RY + 0.12, FLOOR + zz), (0.55, RY + 0.12, FLOOR + zz), 0.06, 0.008, "upper", (-1, 0, 0) if False else (0, 1, 1))
    for xx in (-2.3, -0.9, 0.5):
        m.rod((xx, RY + 0.12, FLOOR), (xx, RY + 0.12, FLOOR + 0.62), 0.02, "upper", sides=6)
    for k in range(9 if fine else 4):
        xx = -2.15 + k * 0.24 * (1 if fine else 2.2)
        charge_case(m, (xx, -RY - 0.3, FLOOR + 0.03), Z, 0.15, 0.68)
    for zz in (0.3, 0.62):
        m.rod((-2.35, -RY - 0.1, FLOOR + zz), (-0.05, -RY - 0.1, FLOOR + zz), 0.018, "upper", sides=6)
    # the shell trolley on its track, a shell on it, the handle up
    tc = Vector((-2.3, 0.0, FLOOR))
    m.box(tc + Vector((0, 0, 0.26)), (0.9, 0.56, 0.05), "upper")
    for sx in (-0.3, 0.3):
        for sy in (-0.3, 0.3):
            m.lathe(tc + Vector((sx, sy - 0.03 * (1 if sy > 0 else -1), 0.13)), Y * (1 if sy > 0 else -1), [(0.0, 0.11), (0.03, 0.11), (0.03, 0.09), (0.06, 0.09)], "iron", 16 if fine else 8)
        m.rod(tc + Vector((sx, -0.27, 0.13)), tc + Vector((sx, 0.27, 0.13)), 0.02, "steel", sides=6)
    for sx in (-0.25, 0.25):
        bits.plate(m, [(-0.2, 0.0), (0.2, 0.0), (0.08, 0.1), (-0.08, 0.1)], 0.02, "upper", tc + Vector((sx, 0, 0.285)), Y, Z)
    projectile(m, tc + Vector((-0.31, 0, 0.36)), X, 0.15, 0.62)
    m.rod(tc + Vector((-0.45, 0.2, 0.28)), tc + Vector((-0.95, 0.2, 0.95)), 0.017, "upper", sides=6)
    m.rod(tc + Vector((-0.45, -0.2, 0.28)), tc + Vector((-0.95, -0.2, 0.95)), 0.017, "upper", sides=6)
    m.rod(tc + Vector((-0.95, -0.25, 0.95)), tc + Vector((-0.95, 0.25, 0.95)), 0.02, "timber.dark", sides=8)
    # the rammer's staff leaning by the door, a chain hoist on a rail under the roof over the track
    m.rod((XR + 1.8, -0.85 + 0.05, FLOOR + 0.02), (XR + 1.95, -0.88, FLOOR + 2.9), 0.028, "timber", sides=8)
    m.lathe(Vector((XR + 1.8, -0.8, FLOOR + 0.02)), Vector((0.05, 0, 1)), [(0.0, 0.085), (0.18, 0.085), (0.2, 0.05)], "timber.dark", 12)
    m.box((-1.5, 0.0, SOF - 0.1), (2.6, 0.12, 0.16), "steel")
    m.box((-1.9, 0.0, SOF - 0.22), (0.2, 0.18, 0.12), "steel")
    if fine:
        bits.tube(m, [(-1.95, 0.05, SOF - 0.28), (-1.97, 0.06, 2.4), (-1.96, 0.05, 1.6)], 0.008, "iron", 4)
        bits.tube(m, [(-1.85, -0.05, SOF - 0.28), (-1.86, -0.05, 2.9), (-1.84, -0.04, 2.2), (-1.8, 0.0, 2.15)], 0.008, "iron", 4)

    # ---- the gun
    gk = coast_gun(m, "battery", O)
    m.use("battery")
    m.socket("ground", (XL + 3.0, 0.0, 0.05), (1, 0, 0), part="battery", note="where the blast lifts dust and spray in front when she fires")
    m.socket("hit", (5.0, 1.5, 3.4), (1, 0, 0), part="battery", note="where a shell bursts in the opening")
    m.socket("smoke", (4.6, 0.0, 3.8), (0.6, 0, 1), part="battery", note="smoke rolls out of the opening and up over the lip")
    bits.crew(m, "loader", (gk["T"].x - 2.0, -0.5, gk["deck"]), (1, 0.3, 0), "stand", "mount", "standing on the deck at the right of the breech, laying the shell in the tray")
    bits.crew(m, "rammer", (gk["T"].x - 3.1, 0.1, FLOOR), (1, 0, 0), "stand", "battery", "standing behind the breech with the rammer's staff, driving the shell home")
    bits.crew(m, "ammo.1", (-1.4, RY - 0.4, FLOOR), (0, 1, 0), "stand", "battery", "at the ready shells in the left recess, lifting one onto the trolley")
    bits.crew(m, "ammo.2", (-1.2, -RY + 0.45, FLOOR), (0, -1, 0), "stand", "battery", "at the charges in the right recess, carrying one to the breech")
    bits.crew(m, "commander", (-1.9, 2.4, FLOOR), (1, -0.2, 0), "stand", "battery", "standing at the left rear of the room, field glasses up, giving the order to fire")

    # ---- rubble hidden until the hit
    m.part("rubble", "battery", (4.8, 2.4, 0.2), kind="free", note="hidden (scale 0) until the opening is hit")
    m.use("rubble")
    rubble(m, (5.6, 2.6, FLOOR), 2.4, 40, seed=7, size=(0.15, 0.6))
    rubble(m, (3.3, 0.8, FLOOR), 1.4, 14, seed=8, size=(0.1, 0.3))
    m.use("battery")

    # ---- clips
    long = 3.6
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Laid and still; the rubble hidden.", "tracks": [{"part": "rubble", "scale": [[0.0, 0.0]]}], "events": []}
    m.clips["fire"] = {"long": long, "loop": False, "after": "hold",
                       "about": "One round: the barrel runs back 0.85 m and out again, the cradle kicks, the breech opens, the next shell and charge go in and it closes. "
                                "Lies over whatever the game has laid (traverse and elevation).",
                       "tracks": [{"part": "barrel", "slide": [[0.0, 0.0], [0.07, 0.8], [0.14, 0.86], [0.6, 0.3], [1.05, 0.04], [1.3, 0.0]]},
                                  {"part": "cradle", "turn": [[0.0, 0.0], [0.05, -0.6], [0.2, 0.3], [0.45, -0.1], [0.8, 0.0]]},
                                  {"part": "mount", "turn": [[0.0, 0.0], [0.06, 0.25], [0.3, -0.1], [0.7, 0.0]]},
                                  {"part": "breech", "slide": [[0.0, 0.0], [1.4, 0.0], [1.6, 0.36], [2.9, 0.36], [3.1, 0.0], [long, 0.0]]},
                                  {"part": "rubble", "scale": [[0.0, 0.0]]}],
                       "events": [{"t": 0.0, "fx": "muzzle_flash_big", "socket": "muzzle", "cue": "field_gun_fire"},
                                  {"t": 0.0, "fx": "muzzle_smoke", "socket": "muzzle", "cue": ""},
                                  {"t": 0.04, "fx": "blast_dust", "socket": "ground", "cue": ""},
                                  {"t": 1.55, "fx": "breech_smoke", "socket": "breech", "cue": ""},
                                  {"t": 3.05, "fx": "breech_close", "socket": "breech", "cue": "field_gun_breech"}]}
    m.clips["follow"] = {"long": 24.0, "loop": True, "about": "A slow traverse across the arc and back, the barrel lifting a little (for a look only: the game lays the mount and cradle itself, "
                                                             "5 degrees a second at most, and spins the handwheels as they turn).",
                         "tracks": [{"part": "mount", "turn": [[0.0, 0.0], [6.0, 55.0], [18.0, -55.0], [24.0, 0.0]]},
                                    {"part": "cradle", "turn": [[0.0, 4.0], [6.0, 10.0], [12.0, 6.0], [18.0, 12.0], [24.0, 4.0]]},
                                    {"part": "handwheel.traverse", "turn": [[0.0, 0.0], [6.0, 3300.0], [18.0, -3300.0], [24.0, 0.0]]},
                                    {"part": "handwheel.elevate", "turn": [[0.0, 0.0], [6.0, 360.0], [12.0, 120.0], [18.0, 480.0], [24.0, 0.0]]},
                                    {"part": "rubble", "scale": [[0.0, 0.0]]}], "events": []}
    q = Quaternion(Vector((0.6, 1, 0)).normalized(), math.radians(7.0)) @ Quaternion(Z, math.radians(-9.0))
    qd = Quaternion(Vector((0, 1, 0)), math.radians(9.0))                    # the cradle drops its muzzle
    ql = Quaternion(Vector((0.3, 1, 0.1)).normalized(), math.radians(62.0))  # the lip turns over as it falls
    m.clips["knocked_out"] = {
        "long": 3.0, "loop": False, "after": "hold",
        "about": "A heavy shell bursts in the opening: the mount is thrown askew on its pedestal, the barrel drops and is jammed back out of battery, the breech hangs "
                 "open, a piece of the stepped lip breaks away and falls in front of the opening, rubble falls, she burns. (At rest play `idle`, which keeps the rubble hidden.)",
        "tracks": [{"part": "mount", "rot": [[0.0, 1, 0, 0, 0], [0.12, *q], [0.3, *Quaternion().slerp(q, 0.85)], [0.6, *q]], "move": [[0.0, 0, 0, 0], [0.12, -0.08, 0.05, 0.03], [0.6, -0.1, 0.06, -0.06]]},
                   {"part": "cradle", "rot": [[0.0, 1, 0, 0, 0], [0.15, *qd]]},
                   {"part": "barrel", "slide": [[0.0, 0.0], [0.1, 0.55], [3.0, 0.55]]},
                   {"part": "breech", "slide": [[0.0, 0.0], [0.2, 0.3], [3.0, 0.3]]},
                   {"part": "lip", "rot": [[0.0, 1, 0, 0, 0], [0.3, *Quaternion().slerp(ql, 0.15)], [0.7, *Quaternion().slerp(ql, 0.6)], [0.95, *ql], [1.05, *Quaternion().slerp(ql, 0.94)], [1.15, *ql]],
                    "move": [[0.0, 0, 0, 0], [0.3, 0.3, 0.0, -0.4], [0.7, 0.9, 0.05, -2.6], [0.95, 1.2, 0.1, -4.25]]},
                   {"part": "rubble", "scale": [[0.0, 0.0], [0.15, 0.0], [0.4, 1.0]], "move": [[0.0, 0.0, 0, 3.5], [0.15, 0.0, 0, 3.5], [0.6, 0.2, 0, 0.6], [0.95, 0.0, 0, 0.0]]}],
        "events": [{"t": 0.0, "fx": "shell_burst_big", "socket": "hit", "cue": "explosion_debris"},
                   {"t": 0.05, "fx": "debris", "socket": "hit", "cue": ""},
                   {"t": 0.9, "fx": "dust_big", "socket": "ground", "cue": "building_collapse"},
                   {"t": 0.4, "fx": "smoke_black", "socket": "smoke", "cue": ""},
                   {"t": 0.6, "fx": "fire", "socket": "fire", "cue": "fire_start", "cue_loop": "fire_loop"}]}
    weather(m, [(gk["B"] + X * 7.3, 1.0, 0.85), ((XL, 0.0, 4.6), 2.5, 0.45)], splash=1.2, rust=0.6)
    m.info["span"] = 17.0
    m.info["high"] = 3.0
    m.info["aim"] = (1.0, 0.0, 3.0)
    m.info["worn"] = [(XR - 1.8, 0.0, 4.5), (XL + 1.5, 0.0, 4.0), (XR - 2.0, -7.0, 2.5)]
    m.info["clear"] = [[(XE + 0.1, -yside(XE) - 0.1), (XE + 0.1, yside(XE) + 0.1), (XR - 0.1, yside(XR) + 0.1), (XR - 0.1, -yside(XR) - 0.1)],
                       [(WX0 - 0.1, -3.5), (WX1 + 0.1, -3.5), (WX1 + 0.1, 3.5), (WX0 - 0.1, 3.5)]]
    m.info["file"] = {"controls": {"mount": "the game turns it to the target's bearing, 60 degrees either side of straight out to sea", "cradle": "the game lifts it for the range",
                                   "handwheel.traverse": "spin 60 degrees for each degree the mount turns", "handwheel.elevate": "spin 60 degrees for each degree the cradle lifts"}}
    return m


BUILDERS = {
    "Bunker": Bunker,
    "CoastalBattery": CoastalBattery,
}
