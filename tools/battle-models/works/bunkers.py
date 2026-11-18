"""The heavy concrete works: the gun casemate (bunker) of the land front and the coastal battery on its headland.

Cast as the pillbox is (concrete.py): a solid block, its rooms hollowed out, its embrasures cut in steps, every edge
chamfered; what a heavy hit breaks is split off beforehand along a ragged break. Earth is heaped over and banked
against them as one soft-edged field of earth and turf, carved back where an embrasure or a door must see out.

Our own plain designs; no army's own pattern.
"""
import math

from mathutils import Quaternion, Vector

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


def earth_field(m, h_of, xs, ys, hidden, amount=0.55, seed=0.0, part=None, jitter=0.0):
    """A field of heaped earth over a grid: h_of(x, y) the height, hidden(x, y, h) whether a point lies under concrete or
    the ground (a cell all of whose corners are hidden is left out). Turfed in soft-edged patches."""
    H = [[h_of(x, y) for y in ys] for x in xs]
    verts, vid, faces = [], {}, []

    def vi(i, j):
        k = (i, j)
        if k not in vid:
            x, y = xs[i], ys[j]
            if jitter and 0 < i < len(xs) - 1 and 0 < j < len(ys) - 1:
                x += jitter * bits.fbm((x * 1.7, y * 1.7, 11.0), 0.5)
                y += jitter * bits.fbm((x * 1.7, y * 1.7, 23.0), 0.5)
            vid[k] = len(verts)
            verts.append(Vector((x, y, max(H[i][j], 0.004))))
        return vid[k]
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            cs = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
            if all(hidden(xs[a], ys[b], H[a][b]) for a, b in cs):
                continue
            q = [vi(a, b) for a, b in cs]
            # (split along the diagonal that keeps the surface nearest its own heights)
            if abs(H[i][j] - H[i + 1][j + 1]) < abs(H[i + 1][j] - H[i][j + 1]):
                faces += [(q[0], q[1], q[2]), (q[0], q[2], q[3])]
            else:
                faces += [(q[0], q[1], q[3]), (q[1], q[2], q[3])]
    bits.turfed(m, verts, faces, amount, "earth", "turf", seed, part)
    return H


def grass_tufts(m, h_of, hidden, x0, x1, y0, y1, count, seed=0, low=0.25):
    """Blades of rank grass standing out of turfed earth."""
    if not m.fine:
        return
    g = rnd(seed)
    made = 0
    for _ in range(count * 3):
        if made >= count:
            break
        x, y = g.uniform(x0, x1), g.uniform(y0, y1)
        h = h_of(x, y)
        if hidden(x, y, h) or h < low:
            continue
        # (only where the turf grows: the same noise bits.turfed uses)
        n = 0.5 + 0.5 * bits.fbm((x / 1.5, y / 1.5, 0.37), 1.0, 3)
        if n > 0.62:
            continue
        made += 1
        for _k in range(3):
            base = Vector((x + g.uniform(-0.06, 0.06), y + g.uniform(-0.06, 0.06), h - 0.01))
            ang = g.uniform(0, 6.283)
            side = Vector((-math.sin(ang), math.cos(ang), 0)) * 0.01
            tip = base + Vector((math.cos(ang) * g.uniform(0.03, 0.1), math.sin(ang) * g.uniform(0.03, 0.1), g.uniform(0.12, 0.32)))
            m.add([base - side, base + side, tip], [(0, 1, 2)], "turf" if g.random() < 0.55 else "garnish.2", False, None)


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
    wslab = bits.s_box((WX0, 0.40, FOOT + 0.3), (XR + 0.2, 5.05, FLOOR))
    wing = s_yz([(0.40, FOOT + 0.25), (5.05, FOOT + 0.25), (5.05, 0.55), (4.6, 1.05), (3.9, 2.05), (0.40, 2.05)], WX0, WX1)
    ret = bits.s_box((WX0, 0.40, FOOT + 0.25), (XR + 0.15, 0.80, 2.05))
    stairs = [bits.s_box((WX1 - 0.05, 3.05 + 0.4 * k, FLOOR - 0.1), (XR + 0.1, 5.05, FLOOR + 0.1 * (k + 1) - 0.004 * k)) for k in range(4)]
    bits.s_join(body, apron, wslab, wing, ret, *stairs)

    room = bits.s_box((RX0, -RY, FLOOR), (RX1, RY, CEIL))
    passage = bits.s_box((XR - 0.6, DY - DW / 2, FLOOR + 0.02), (RX0 + 0.1, DY + DW / 2, 1.42))
    main = steps_cut(0.0, XF, 0.0, [(0.0, 4.9, 0.14, 1.92), (0.36, 4.3, 0.18, 1.80), (0.72, 3.7, 0.22, 1.66), (1.08, 3.15, 0.26, 1.53), (1.44, 2.6, EZ0, EZ1)], tail=0.8)
    mg = steps_cut(90.0, YS, -MGX, [(0.0, 1.7, MGZ - 0.52, MGZ + 0.46), (0.3, 1.32, MGZ - 0.40, MGZ + 0.36), (0.6, 0.98, MGZ - 0.28, MGZ + 0.26),
                                    (0.9, 0.66, MGZ - 0.18, MGZ + 0.17), (1.1, 0.42, MGZ - 0.12, MGZ + 0.12), (1.32, 0.86, FLOOR + 0.55, MGZ + 0.55)], tail=0.6)
    bits.s_cut(body, room, passage, main, mg)
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
        return 3.36 * (1.0 - 0.74 * smooth((x - 1.0) / 3.2))

    def bump(x, y):
        u, v = (x + 0.7) / 4.2, y / 5.0
        return max(0.0, 1 - u * u) * max(0.0, 1 - v * v)

    def is_in(x, y):
        return XR + 1e-4 < x < XF - 1e-4 and abs(y) < YS - 1e-4

    def h_of(x, y):
        if is_in(x, y):
            h = 3.36 + 0.55 * bump(x, y) + 0.06 * bits.fbm((x * 0.6, y * 0.6, 3.0), 0.8)
            h -= (h - 3.08) * smooth((x - 2.3) / 0.7)
            # (the roof's earth stops short over the door and over the machine gun)
            h = min(h, 3.10 + 1.6 * max(0.0, x - (XR + 0.75)) + 1.6 * max(0.0, 0.35 - y) + 1.6 * max(0.0, y - 4.9))
            h = min(h, 3.10 + 1.6 * max(0.0, (YS - 0.65) - y) + 1.6 * max(0.0, abs(x - MGX) - 1.3))
            return h
        dx = max(XR - x, 0.0, x - XF)
        dy = max(abs(y) - YS, 0.0)
        d = math.hypot(dx, dy)
        E = edge_h(min(max(x, XR), XF))
        W = 1.6 + 1.35 * E
        h = E * (1.0 - smooth(d / W))
        if x > XF:
            h *= smooth((abs(y) - 3.3) / 1.6)
        h += 0.09 * bits.fbm((x * 0.55, y * 0.55, 2.0), 0.7) * smooth(h / 0.5)
        # the entrance well: earth against the back of the wing wall, falling away to the steps' head
        if x < XR + 1e-4 and 0.4 < y < 5.05 and x > WX0:
            return -1.0
        c_wall = 2.0 + 0.8 * rect_dist(x, y, WX0, XR, 0.40, 5.05)
        c_exit = 0.04 + 0.8 * math.hypot(max(WX1 - x, 0.0, x - XR), max(0.0, y - 5.05) + 3.0 * max(0.0, 5.05 - y))
        h = min(h, c_wall, c_exit)
        # the machine gun's field of fire: a cutting in the bank, widening outward
        if y > YS - 0.01:
            h = min(h, 0.22 + 0.85 * max(0.0, abs(x - MGX) - 0.75 - 0.42 * max(0.0, y - YS)))
        return h

    def hidden(x, y, h):
        if is_in(x, y):
            return h < TOP - 0.01
        return h < 0.012

    step = 0.32 if fine else 0.8
    xs = [XR + step * k for k in range(-int(7.6 / step), int((XF - XR + 2.4) / step) + 1)]
    ys = [-YS + 0.3 * (1 if fine else 3) * k for k in range(-int(7.0 / (0.3 * (1 if fine else 3))), int((2 * YS + 7.0) / (0.3 * (1 if fine else 3))) + 1)]
    earth_field(m, h_of, xs, ys, hidden, 0.6, 1.0, "bunker")
    grass_tufts(m, h_of, hidden, xs[0], xs[-1], ys[0], ys[-1], 700, seed=3)
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

    # ---- the machine gun in the left flank
    pivot = Vector((MGX, YS - 1.2, MGZ))
    embrasure_gun(m, "bunker", pivot, 90.0, "air", "mg")
    m.use("bunker")

    # ---- sockets and crew
    gun_at = Vector((0.75, 0.0, FLOOR + 0.01))
    m.socket("gun", gun_at, (1, 0, 0), part="bunker",
             note="the anti-tank or field gun's place: its origin on the floor under the middle of its axle, x toward the muzzle. "
                  "Room: the throat (2.6 m wide, 1.1 m high, its sill 0.7 m above the floor) is 1.45 m ahead of the axle; 2.4 m clear either side; "
                  "3.15 m to the back wall for the trails; 1.9 m under the ceiling. The embrasure's steps let the barrel traverse 30 degrees either way.")
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
    m.info["clear"] = [[(XR - 0.1, -YS - 0.1), (XF + 3.3, -YS - 0.1), (XF + 3.3, YS + 0.1), (XR - 0.1, YS + 0.1)], [(WX0 - 0.1, 0.3), (XR + 0.1, 0.3), (XR + 0.1, 5.2), (WX0 - 0.1, 5.2)]]
    m.info["dig"] = [[(WX1 + 0.02, 0.85), (XR + 0.3, 0.85), (XR + 0.3, 4.75), (WX1 + 0.02, 4.75)]]
    m.info["dig_depth"] = -FLOOR
    return m


BUILDERS = {
    "Bunker": Bunker,
}
