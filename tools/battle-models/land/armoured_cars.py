"""Armoured cars: the scouts on wheels, built on the carriers' kit (wheels on tyres, steering) with a tank's turret
and a tank's clips.

    SdKfz222      the German light armoured car: a faceted body widest at a knuckle line, pointed at both ends,
                  angular mudguards, a low ten-sided turret open to the sky under two wire screens, a 2 cm
                  automatic gun and a machine gun beside it.
    M8            the American light armoured car: six wheels, a welded body with sponsons over them, a round
                  open-topped turret with a 37 mm gun, a heavy machine gun on a pintle behind.
    ArmouredCar   an armoured car as most armies had one in 1939: a lorry's chassis under riveted plate, a long
                  bonnet behind armoured radiator doors, a tall fighting room, swept mudguards, a spare wheel on
                  her stern, a small eight-sided turret with a 37 mm gun.

PARTS   vehicle (root) > hull > turret > gun > barrel; turret > hatch_screen_l, hatch_screen_r (the wire screens:
        thrown open when her men bail out)
        vehicle > steer_l > wheel_front_l, steer_r > wheel_front_r (the game steers and rolls them), wheel_back_l/r
CLIPS   a tank's (idle, start, drive, move_off, stop, fire, fire.rock, mg_burst, hit, open_up, knocked_out, burn);
        `fire` is a burst from the automatic gun
SOCKETS muzzle, coax, ring, commander, case, engine, exhaust.1, fire.N, hit.*, dust.left, dust.right
Every shape is our own mesh, made from numbers here.
"""

import math

from mathutils import Vector

from carriers import CarrierWorks, wall
from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, inset_plan, sym_plan
from tanks_any import riveted_prism, rivets, turn_to
from tanks_ger import GER_COATS, coat, coats


def on_plane(a, b, c, x=None, y=None, z=None):
    """The point in the plane of a, b, c with two of x, y, z given (the third is found): so that four corners of a
    plate lie flat."""
    a, b, c = Vector(a), Vector(b), Vector(c)
    n = (b - a).cross(c - a)
    if x is None:
        return Vector((a.x - (n.y * (y - a.y) + n.z * (z - a.z)) / n.x, y, z))
    if y is None:
        return Vector((x, a.y - (n.x * (x - a.x) + n.z * (z - a.z)) / n.y, z))
    return Vector((x, y, a.z - (n.x * (x - a.x) + n.y * (y - a.y)) / n.z))


class CarWorks(CarrierWorks):
    def guard(self, line, y0, y1, thick=0.012, lip=0.0, paint=coat):
        """An angular mudguard: flat plates along `line` ((x, z) points over the wheel, from its front to its back),
        carried across from y0 to y1; `lip`: its outer edge turned down by this much."""
        outer = [tuple(p) for p in line]
        inner = []
        for i in range(len(outer) - 1, -1, -1):
            a, b = outer[max(i - 1, 0)], outer[min(i + 1, len(outer) - 1)]
            t = Vector((b[0] - a[0], b[1] - a[1])).normalized()
            inner.append((outer[i][0] - t.y * thick, outer[i][1] + t.x * thick))
        self.extrude(outer + inner, min(y0, y1), max(y0, y1), paint, 0.003)
        if lip:
            edge = y1 if abs(y1) > abs(y0) else y0
            in_ = edge - math.copysign(0.012, edge)
            self.extrude(outer + [(x, z - lip) for x, z in reversed(outer)], min(edge, in_), max(edge, in_), paint, 0.002)

    def wire_screen(self, corners, pitch=0.05, r=0.0032, frame=0.011, paint=coat):
        """A flat frame of rod with wire mesh in it: `corners` going round (three or four, lying flat)."""
        cs = [Vector(c) for c in corners]
        self.tube(cs, frame, paint, 6, closed=True)
        if not self.fine:
            return
        e1 = (cs[1] - cs[0]).normalized()
        n = e1.cross(cs[2] - cs[0]).normalized()
        e2 = n.cross(e1)
        flat = [((c - cs[0]).dot(e1), (c - cs[0]).dot(e2)) for c in cs]
        for along in (0, 1):                # wires one way, then across them
            vs = [p[1 - along] for p in flat]
            k = math.floor(min(vs) / pitch) + 1
            while k * pitch < max(vs) - 1e-6:
                w = k * pitch
                hits = []
                for i in range(len(flat)):
                    p, q = flat[i], flat[(i + 1) % len(flat)]
                    a, b = p[1 - along], q[1 - along]
                    if (a - w) * (b - w) < 0:
                        f = (w - a) / (b - a)
                        hits.append(p[along] + (q[along] - p[along]) * f)
                if len(hits) >= 2 and max(hits) - min(hits) > 0.02:
                    ends = [cs[0] + (e1 * h + e2 * w if along == 0 else e1 * w + e2 * h) for h in (min(hits), max(hits))]
                    self.rod(ends[0], ends[1], r, "steel", 4, caps=False)
                k += 1


def sdkfz222(fine=True, tag="GER", paint=None):
    """The light armoured car (Sd.Kfz. 222): 4.8 m long, 1.95 m wide, 2.0 m to the ridge of her screens. Three men: a
    driver low in the nose, two in the open turret under its wire screens with a 2 cm automatic gun and a machine
    gun. Dark grey to early 1943, dark yellow after: a paint swap."""
    v = CarWorks("SdKfz222", "Sd.Kfz. 222 armoured car", "car", tag, fine, paint=GER_COATS[paint or "grey"][0])
    v.root(hull_pivot=(0.0, 0.0, 0.75), rock=(-4.0, 4.0))
    m = v.m
    R, W, RR = 0.45, 0.21, 0.255
    xf, xr, yw = 1.40, -1.40, 0.82
    zb, zk, zr = 0.56, 1.06, 1.58                         # her belly, the knuckle line, the roof
    yb, yk, yr = 0.56, 0.94, 0.66                         # how far out she is at each

    # ---- the body: three solids of flat plate, every plate lying flat (corners found on their planes) ---------
    K, A = Vector((1.00, yk, zk)), Vector((-0.80, yk, zk))            # the knuckle, at the front and back of the fighting room
    G, F = Vector((1.00, yb, zb)), Vector((-0.80, yb, zb))            # the belly under them
    Rf, D = Vector((0.78, yr, zr)), Vector((-0.66, yr, zr))           # the roof's corners
    H = on_plane(K, A, D, x=1.00, z=1.32)                             # where the nose plate meets the visor plate
    N = Vector((2.22, 0.38, 0.95))                                    # the nose's edge
    L = on_plane(K, G, N, x=2.04, z=0.74)                             # the foot of the nose
    B = Vector((-2.42, 0.52, 1.02))                                   # the stern's knuckle
    C = on_plane(A, B, D, x=-2.30, z=1.26)                            # the end of the engine deck
    E = on_plane(A, F, B, x=-2.28, z=0.72)                            # the foot of the stern
    v.use("hull")
    v.hull([G, K, H, Rf, D, A, F], coat, bevel=0.01)
    v.hull([G, K, H, N, L], coat, bevel=0.01)
    v.hull([F, A, D, C, B, E], coat, bevel=0.01)

    gl = Vector((H.x - N.x, 0.0, H.z - N.z)).normalized()             # up the nose plate
    gn = Vector((gl.z, 0.0, -gl.x))
    fl = Vector((Rf.x - H.x, 0.0, Rf.z - H.z)).normalized()           # up the visor plate
    fn = Vector((fl.z, 0.0, -fl.x))
    dk = Vector((C.x - D.x, 0.0, C.z - D.z)).normalized()             # back along the engine deck
    dn = Vector((dk.z, 0.0, -dk.x))
    sl = Vector((C.x - B.x, 0.0, C.z - B.z)).normalized()             # up the stern plate
    rn = Vector((-sl.z, 0.0, sl.x))
    g_len, d_len = (Vector((H.x, 0, H.z)) - Vector((N.x, 0, N.z))).length, (Vector((C.x, 0, C.z)) - Vector((D.x, 0, D.z))).length
    g_at = lambda u, y: Vector((N.x, y, N.z)) + gl * u + gn * 0.002        # noqa: E731
    f_at = lambda u, y: Vector((H.x, y, H.z)) + fl * u + fn * 0.002        # noqa: E731
    d_at = lambda u, y: Vector((D.x, y, D.z)) + dk * u + dn * 0.002        # noqa: E731
    r_at = lambda u, y: Vector((B.x, y, B.z)) + sl * u + rn * 0.002        # noqa: E731
    up_y = lambda z: yk - (z - zk) * (yk - yr) / (zr - zk)                 # noqa: E731  (the upper side plate)
    low_y = lambda z: yb + (z - zb) * (yk - yb) / (zk - zb)                # noqa: E731  (the lower one)
    up_n = [Vector((0.0, s * (zr - zk), yk - yr)).normalized() for s in (LEFT, RIGHT)]
    low_n = [Vector((0.0, s * (zk - zb), -(yk - yb))).normalized() for s in (LEFT, RIGHT)]

    # ---- the nose plate: a bolted plate over the steering, lifting eyes, hooks, tools ----------------------------
    c = g_at(g_len * 0.52, 0.0)
    v.box((0.46, 0.50, 0.014), c + gn * 0.007, coat, 0.005, turn=turn_to(gl, gn))
    if fine:
        for du in (-0.19, 0.0, 0.19):
            for dy in (-0.21, 0.21):
                v.bolt(c + gl * du + Y * dy + gn * 0.014, gn, 0.011)
        v.handle(c + gl * 0.12 - Y * 0.07 + gn * 0.014, c + gl * 0.12 + Y * 0.07 + gn * 0.014, gn, 0.007, 0.03, "steel")
    for s in (LEFT, RIGHT):
        v.hook((N.x - 0.03, s * 0.26, N.z - 0.1), Vector((0.9, 0, -0.2)).normalized(), 0.075)
        v.hook((B.x + 0.02, s * 0.30, B.z - 0.14), -X, 0.075)
        if fine:
            v.lift_eye(g_at(g_len * 0.9, s * 0.62), gn, Y, 0.026)
    if fine:                                               # the tow cable, looped across her nose
        v.cable([g_at(0.10, 0.27), g_at(0.24, 0.20), g_at(0.30, 0.0), g_at(0.24, -0.20), g_at(0.10, -0.27)], 0.011)
    # the visor plate: two visors; a visor in each upper side plate beside the driver
    for s in (LEFT, RIGHT):
        v.visor(f_at(0.2, s * 0.34) + fn * 0.004, fn, 0.36, 0.12, 0.05)
        v.visor(Vector((0.48, s * up_y(1.36), 1.36)) + up_n[s < 0] * 0.004, up_n[s < 0], 0.26, 0.1, 0.045)
    if fine:
        v.weld(f_at(0.0, -H.y + 0.03), f_at(0.0, H.y - 0.03), (gn + fn).normalized())

    # ---- the sides: a door low in each, between the wheels; the crosses above them -------------------------------
    for i, s in enumerate((LEFT, RIGHT)):
        ln = low_n[i]
        la = Vector((0.0, s * (yk - yb), zk - zb)).normalized()           # up the lower plate
        c = Vector((0.08, s * low_y(0.82), 0.82)) + ln * 0.002
        v.box((0.64, 0.36, 0.014), c + ln * 0.007, coat, 0.005, turn=turn_to(X, ln))
        if fine:
            for dx in (-0.2, 0.2):
                v.hinge(c + X * dx + la * 0.19 + ln * 0.014 - X * 0.05, c + X * dx + la * 0.19 + ln * 0.014 + X * 0.05, 0.011)
            v.handle(c + X * 0.2 - la * 0.06 + ln * 0.014, c + X * 0.08 - la * 0.06 + ln * 0.014, ln, 0.007, 0.03, "steel")
            v.port(Vector((-0.42, s * up_y(1.36), 1.36)), up_n[i], 0.038)
        v.mark(tag, (0.0, s * up_y(1.30), 1.30), up_n[i], 0.13, part="hull")

    # ---- the engine deck: two doors, a grille behind them; the radiator's louvres in the stern plate ------------
    for s in (LEFT, RIGHT):
        c = d_at(d_len * 0.30, s * 0.26)
        v.box((0.64, 0.40, 0.014), c + dn * 0.007, coat, 0.005, turn=turn_to(dk, dn))
        if fine:
            for du in (-0.2, 0.2):
                v.hinge(c + dk * du + Y * s * 0.2 + dn * 0.014 - dk * 0.05, c + dk * du + Y * s * 0.2 + dn * 0.014 + dk * 0.05, 0.011)
            v.handle(c + Y * s * -0.13 + dn * 0.014 - dk * 0.06, c + Y * s * -0.13 + dn * 0.014 + dk * 0.06, dn, 0.007, 0.03, "steel")
    v.grille(d_at(d_len * 0.72, 0.0) + dn * 0.004, (0.50, 0.56), dn, Y, slats=7, deep=0.022)
    s_len = (Vector((C.x, 0, C.z)) - Vector((B.x, 0, B.z))).length
    v.louvres(r_at(s_len * 0.5, 0.0) + rn * 0.002, (0.62, s_len * 0.72), rn, count=4, down=-sl)
    if fine:
        v.weld(d_at(0.0, -yr + 0.03), d_at(0.0, yr - 0.03), (Z + dn).normalized())
    # the silencer under her tail, on the right; the pipe out to the side
    v.silencer((-2.20, -0.10, 0.56), (-1.64, -0.10, 0.56), 0.075, pipe=[(-1.52, -0.1, 0.6), (-1.64, -0.1, 0.56)], tail=((-2.20, -0.10, 0.56), (-0.6, -0.8, -0.15)))
    v.socket("exhaust.1", (-2.3, -0.2, 0.53), (-0.6, -0.8, -0.15))

    # ---- the mudguards: angular, a flat top over each wheel, their outer edges turned down -----------------------
    for s in (LEFT, RIGHT):
        v.guard([(2.05, 0.68), (1.91, 0.97), (1.58, zk), (1.06, zk), (0.88, 0.88)], s * 0.58, s * 0.975, lip=0.045)
        v.guard([(-0.88, 0.88), (-1.06, zk), (-1.62, zk), (-1.93, 0.97), (-2.07, 0.72)], s * 0.56, s * 0.975, lip=0.045)
        v.headlamp((1.48, s * 0.84, zk + 0.004), X, 0.07, stalk=0.085, cover=True)
        if fine:                                               # the width marker: a rod with a ball on it at each front corner
            v.rod((1.95, s * 0.95, 0.9), (1.95, s * 0.95, 1.36), 0.006, "steel", 6)
            v.ball((1.95, s * 0.95, 1.37), 0.02, "white", 8, 6)
    v.headlamp((1.86, 0.66, 0.985), X, 0.04, stalk=0.05)                 # the blackout lamp
    # stowage: cans on the left rear guard, a bin on the right one; a shovel and an axe along the engine's sides
    v.jerrycan((-1.34, 0.89, zk), 0.0)
    v.bin((-1.60, -0.975, zk), (-1.10, -0.80, zk + 0.2), lid_side=-Y)
    v.aerial("aerial", "hull", (-0.62, 0.74, 1.42), 1.9)

    # ---- the wheels: four, the front pair steered -----------------------------------------------------------------
    v.use("vehicle")
    for x in (xf, xr):
        v.tube([(x, -yw + 0.14, R), (x, -0.22, R - 0.03), (x, 0.22, R - 0.03), (x, yw - 0.14, R)], 0.034, "shade", 8)
        v.ball((x, 0.0, R - 0.04), 0.11, "shade", 12, 8, squash=0.85)                                         # the axle's casing
        for s in (LEFT, RIGHT):                                # a coil spring each side, up into the body
            v.rod((x, s * 0.56, R + 0.02), (x - 0.02, s * 0.5, zb + 0.16), 0.045, "shade", 10)
    v.rod((xr, 0.0, R - 0.02), (xf, 0.0, R - 0.02), 0.022, "shade", 8)                                         # the shaft between them
    v.rod((xf - 0.16, -yw + 0.2, R - 0.02), (xf - 0.16, yw - 0.2, R - 0.02), 0.012, "steel", 6)                # the track rod
    for letter, s in SIDES:
        v.steered_wheel(letter, s, xf, yw, R, W, RR, lock=26.0, holes=6)
        v.lorry_wheel(f"wheel_back_{letter}", "vehicle", (xr, s * yw, R), s, R, W, RR, holes=6)
    v.socket("dust.left", (xr - 0.3, yw, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (xr - 0.3, -yw, 0.05), (-1, 0, 0.3), part="vehicle")

    # ---- the turret: ten thin plates leaning in, open above; its front is the gun's own shield -------------------
    x0, th = 0.08, 0.34
    rb, rt = 0.70, 0.57
    z0, z1 = zr + 0.02, zr + 0.02 + th
    v.use("hull")
    v.ring_well((x0, 0.0, zr), 0.60)
    v.part("turret", "hull", (x0, 0.0, zr), axis=(0, 0, -1), kind="turn", note="trains all round, by hand: + to the right, clockwise seen from above; the game turns it")
    ang = [math.radians(18.0 + 36.0 * i) for i in range(10)]
    lo = [Vector((x0 + rb * math.cos(a), rb * math.sin(a), z0)) for a in ang]
    hi = [Vector((x0 + rt * math.cos(a), rt * math.sin(a), z1)) for a in ang]
    for i in range(9):                                     # (the tenth, across her front, is the gun's shield)
        c = (lo[i] + lo[i + 1] + hi[i + 1] + hi[i]) / 4
        wall(v, [lo[i], lo[i + 1], hi[i + 1], hi[i]], Vector((x0 - c.x, -c.y, 0.0)), thick=0.012, paint=coat, bevel=0.003)
    v.prism([(p.x, p.y) for p in lo], zr + 0.012, z0 + 0.02, "shade")                                          # her floor, in shadow
    v.lathe((x0, 0, zr - 0.02), Z, [(0.05, rb - 0.03), (0.0, rb - 0.01), (0.0, 0.0)], "shade", v.n(30, 10))
    if fine:
        v.tube([p + Z * 0.003 for p in hi[:10]] + [], 0.012, coat, 6, closed=False)
        for i in (1, 3, 5, 7):                              # a pistol port in the plates either side
            c = (lo[i] + lo[i + 1] + hi[i + 1] + hi[i]) / 4
            v.port(c, Vector((c.x - x0, c.y, 0.2)), 0.035)
    # inside her: two seats, the gun's pedestal
    for at in ((x0 - 0.28, 0.26), (x0 - 0.18, -0.30)):
        v.rod((at[0], at[1], zr + 0.03), (at[0], at[1], zr + 0.12), 0.022, "steel", 8)
        v.box((0.26, 0.26, 0.05), (at[0], at[1], zr + 0.14), "leather", 0.018)
    v.rod((x0 + 0.34, 0.0, zr + 0.03), (x0 + 0.34, 0.0, zr + 0.14), 0.05, "steel", 10)

    # the wire screens: two halves that meet in a ridge over her, each hung on the turret's edge and thrown outward
    ridge = z1 + 0.19
    for name, s in (("hatch_screen_l", LEFT), ("hatch_screen_r", RIGHT)):
        hinge = Vector((x0, s * hi[1].y, z1 + 0.012))
        v.part(name, "turret", hinge, axis=(-s, 0, 0), limits=[0.0, 115.0], kind="turn", note="a wire screen over the open turret: + is thrown open, outward")
        side = lambda p: Vector((p.x, s * p.y, z1 + 0.012))              # noqa: E731
        P, Q = Vector((x0 + 0.30, s * 0.012, ridge)), Vector((x0 - 0.30, s * 0.012, ridge))
        quads = [[side(hi[1]), side(hi[3]), Q, P],                        # the long panel up to the ridge
                 [side(hi[0]), side(hi[1]), P],                           # its front corner (the front is left open for the guns)
                 [side(hi[3]), side(hi[4]), Q],                           # its back corner
                 [side(hi[4]), Vector((hi[4].x, s * 0.012, z1 + 0.012)), Q]]   # and half the back
        for q in quads:
            v.wire_screen(q if s > 0 else q[::-1])
        v.use("turret")
    v.socket("commander", (x0 - 0.2, 0.05, z1 - 0.25), X, part="turret", note="the commander, who lays the guns, stands here: head and shoulders out when the screens are open")

    # ---- the guns: a 2 cm automatic gun and a machine gun on its left, laid together behind their shield -----------
    tx, tz = x0 + 0.40, zr + 0.2
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 30.0], kind="turn", note="lifts at the trunnions: + muzzles up; the game lays it")
    fa, fb = lo[9], lo[0]                                   # the front plate's foot: right corner, left corner
    ta, tb = hi[9], hi[0]
    wall(v, [fb + Z * 0.01, fa + Z * 0.01, ta, tb], -X, thick=0.014, paint=coat, bevel=0.003)
    gy, my = -0.075, 0.10
    v.slab((tx - 0.42, gy - 0.05, tz - 0.07), (tx + 0.24, gy + 0.05, tz + 0.06), "steel", 0.012)                 # the gun's body
    v.slab((tx - 0.30, gy - 0.15, tz - 0.05), (tx - 0.12, gy - 0.05, tz + 0.03), "steel", 0.01)                  # its magazine, out to the right
    v.slab((tx - 0.36, my - 0.03, tz - 0.035), (tx + 0.24, my + 0.03, tz + 0.045), "steel", 0.01)                # the machine gun's
    v.slab((tx - 0.10, gy - 0.02, tz - 0.16), (tx + 0.06, my + 0.02, tz - 0.07), "steel", 0.01)                  # the cradle they share
    if fine:
        v.rod((tx - 0.1, 0.02, tz + 0.06), (tx - 0.1, 0.02, tz + 0.2), 0.012, "steel", 6)                        # the sight, on its stalk
        v.rod((tx - 0.16, 0.02, tz + 0.2), (tx - 0.02, 0.02, tz + 0.2), 0.02, "steel", 8)
    wall_x = (fa.x + ta.x) / 2
    v.mg((wall_x - 0.02, my, tz), X, 0.52)
    v.socket("coax", (wall_x + 0.52, my, tz), X)
    v.part("barrel", "gun", (tx + 0.24, gy, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.06], axis=(0, -1, 0), kind="slide", note="jumps back along its own length at each round (metres)")
    bl = 1.02
    v.barrel((tx + 0.24, gy, tz), bl, 0.02, r0=0.027, r1=0.017, brake=None, sleeve=None, sides=14)
    v.lathe((tx + 0.24 + bl - 0.01, gy, tz), X, [(0.0, 0.011), (0.10, 0.021), (0.10, 0.027), (0.0, 0.018)], "upper", v.n(14, 8))    # the flash hider
    v.socket("muzzle", (tx + 0.24 + bl + 0.11, gy, tz), X)
    m.info["bore"] = 0.02
    m.info["brake"] = False

    # ---- sockets for what happens to her ---------------------------------------------------------------------------
    v.socket("engine", d_at(d_len * 0.55, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(d_len * 0.72, 0.0) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (x0 - 0.1, 0.0, z1), Z, part="turret", note="out of the open turret")
    v.socket("ring", (x0, 0.0, zr), Z, part="hull", note="the turret ring")
    v.socket("hit.front", g_at(g_len * 0.5, 0.0), gn, part="hull")
    v.socket("hit.left", (0.0, up_y(1.3), 1.3), up_n[0], part="hull")
    v.socket("hit.right", (0.0, -up_y(1.3), 1.3), up_n[1], part="hull")
    v.socket("hit.rear", r_at(s_len * 0.5, 0.0), rn, part="hull")
    v.socket("hit.turret", (x0, rb * 0.95, zr + 0.2), Y, part="turret")
    v.socket("case", (x0 + 0.1, -0.3, zr + 0.05), -Y, part="turret", note="where the empty cases fall, on her floor")
    v.car_clips(burst=5, recoil=0.05, kick=0.25, gun_low=-8.0, hatches=["hatch_screen_l", "hatch_screen_r"], traverse=30.0,
                idle_cue="lorry_idle_loop", drive_cue="lorry_drive_loop", has_turret=False, mg="coax")
    m.info.setdefault("file", {})["swaps"] = {"how": "paint: put the chosen coat's colours in place of the paints of the same names.", "paint": coats(v, paint or "grey")}
    return v


def m8(fine=True, tag="USA"):
    """The light armoured car M8: 5.0 m long, 2.5 m wide, 2.25 m high. Six wheels, all driven; a welded body with
    sponsons over the wheels; a round open-topped turret with a 37 mm gun, a machine gun beside it and a heavy one on
    a pintle behind. Four men."""
    v = CarWorks("M8", "Light armored car M8", "car", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.8), rock=(-4.0, 4.0))
    m = v.m
    R, W, RR = 0.50, 0.24, 0.27
    xs_w = (1.70, -0.33, -1.55)
    yw, hw = 0.98, 0.60                                   # the wheels; the tub's half-width between them
    zs, zr, zd = 1.08, 1.56, 1.50                         # the sponsons' floor, the roof, the head of the engine deck
    ys, yr = 1.12, 0.94                                   # how far out she is at the sponson floor and at the roof
    side_y = lambda z: ys - (z - zs) * (ys - yr) / (zr - zs)              # noqa: E731
    sn = [Vector((0.0, s * (zr - zs), ys - yr)).normalized() for s in (LEFT, RIGHT)]

    # ---- the body: a tub between the wheels, the fighting room and the engine room over it ------------------------
    v.use("hull")
    v.hull([(x, hw, z) for x, z in [(-2.36, 0.74), (-2.20, 0.62), (1.55, 0.62), (2.16, 0.72), (2.40, 1.04), (2.36, zs), (-2.44, zs)]], coat, bevel=0.012)
    gf, rf = Vector((2.38, hw, 1.06)), Vector((1.10, yr, zr))
    v.hull([gf, rf, (-0.80, yr, zr), (1.05, ys, zs), (-0.80, ys, zs)], coat, bevel=0.012)
    v.hull([(-0.80, ys, zs), (-2.44, ys, zs), (-0.80, side_y(zd), zd), (-2.36, side_y(1.40), 1.40)], coat, bevel=0.012)
    gl = Vector((rf.x - gf.x, 0.0, rf.z - gf.z)).normalized()             # up the nose plate
    gn = Vector((gl.z, 0.0, -gl.x))
    g_len = (Vector((rf.x, 0, rf.z)) - Vector((gf.x, 0, gf.z))).length
    dk = Vector((-2.36 + 0.80, 0.0, 1.40 - zd)).normalized()              # back along the engine deck
    dn = Vector((dk.z, 0.0, -dk.x))
    g_at = lambda u, y: Vector((gf.x, y, gf.z)) + gl * u + gn * 0.002      # noqa: E731
    d_at = lambda u, y: Vector((-0.80, y, zd)) + dk * u + dn * 0.002       # noqa: E731

    # the nose plate: the two drivers' flaps with their slits, a bolted plate under them, lamps behind hoops, hooks
    for s in (LEFT, RIGHT):
        c = g_at(1.10, s * 0.40)
        v.box((0.34, 0.50, 0.02), c + gn * 0.01, coat, 0.006, turn=turn_to(gl, gn))
        v.visor(c + gn * 0.02, gn, 0.30, 0.09, 0.03, brow=False)
        if fine:
            for dy in (-0.16, 0.16):
                v.hinge(c + gl * 0.17 + Y * dy + gn * 0.02 - Y * 0.05, c + gl * 0.17 + Y * dy + gn * 0.02 + Y * 0.05, 0.012)
        lamp = g_at(0.34, s * 0.52) + gn * 0.002
        v.headlamp(lamp, X, 0.07, stalk=0.1)
        v.lamp_guard(lamp + Z * 0.1, 0.07, lamp, gn, Y, gl)                # the brush guard: two hoops of bar crossing over the lamp
        v.hook((2.40, s * 0.36, 0.92), X, 0.085)
        v.hook((-2.44, s * 0.40, 0.92), -X, 0.085)
    c = g_at(0.52, 0.0)
    v.box((0.44, 0.56, 0.014), c + gn * 0.007, coat, 0.005, turn=turn_to(gl, gn))
    if fine:
        for du in (-0.18, 0.0, 0.18):
            for dy in (-0.24, 0.24):
                v.bolt(c + gl * du + Y * dy + gn * 0.014, gn, 0.012)
    # the roof ahead of the turret: a lid over each driver, hung at its outer edge
    v.hatch_rect("hatch_driver", "hull", (0.90, 0.42, zr), (0.34, 0.44), Z, X, hinge="left", opens=120.0, thick=0.018)
    v.hatch_rect("hatch_radio", "hull", (0.90, -0.42, zr), (0.34, 0.44), Z, X, hinge="right", opens=120.0, thick=0.018)
    v.use("hull")
    # the engine deck: a grille each side, two doors behind them, a rolled sheet across her tail
    for s in (LEFT, RIGHT):
        v.grille(d_at(0.42, s * 0.40) + dn * 0.004, (0.60, 0.56), dn, dk, slats=9, deep=0.022)
        c = d_at(1.10, s * 0.40)
        v.box((0.56, 0.62, 0.014), c + dn * 0.007, coat, 0.005, turn=turn_to(dk, dn))
        if fine:
            v.handle(c - Y * 0.08 + dk * 0.18 + dn * 0.014, c + Y * 0.08 + dk * 0.18 + dn * 0.014, dn, 0.008, 0.035, "steel")
            for dy in (-0.2, 0.2):
                v.hinge(c + Y * dy - dk * 0.28 + dn * 0.014 - Y * 0.05, c + Y * dy - dk * 0.28 + dn * 0.014 + Y * 0.05, 0.012)
        v.lathe((-2.445, s * 0.86, 1.2), -X, [(0.03, 0.0), (0.03, 0.035), (0.0, 0.04), (0.0, 0.0)], "red", 10)       # a tail lamp
    v.roll((-2.26, -0.78, 1.5), (-2.26, 0.78, 1.5), r=0.1, paint=v.own_paint("canvas.olive", "6E6C50"))   # a darker sheet: tan is the lightest thing on an olive car
    v.slab((-0.84, -yr + 0.04, zd), (-0.80, yr - 0.04, zr), coat, 0.004)                                               # the step down to the deck
    v.silencer((-2.30, -0.86, 0.96), (-1.84, -0.86, 0.96), 0.07, pipe=[(-1.7, -0.7, 0.98), (-1.84, -0.86, 0.96)], tail=((-2.30, -0.86, 0.96), (-1, -0.3, -0.2)))
    v.socket("exhaust.1", (-2.42, -0.9, 0.94), (-1, -0.3, -0.2))
    # under the sponsons: a rack between the front and middle wheels, flaps behind the last
    for s in (LEFT, RIGHT):
        v.bin((0.30, s * 0.64, 0.70), (1.04, s * 1.10, zs - 0.004), lid_side=Y * s)
        v.slab((-2.16, s * 0.64, 0.56), (-2.145, s * 1.11, zs), "rubber", 0.002)
        v.mudguard((xs_w[0], R), R + 0.11, 0.0, 104.0, s * 0.62, s * 1.13, tail=[(1.05, zs + 0.012)])
        v.mark(tag, (0.05, s * side_y(1.32), 1.32), sn[s < 0], 0.16, part="hull")
        if fine:
            v.lift_eye((0.95, s * (yr - 0.05), zr), Z, X, 0.03)
            v.lift_eye((-2.2, s * (side_y(1.41) - 0.06), 1.41), Z, X, 0.03)
    v.aerial("aerial", "hull", (-0.95, 0.80, zd), 2.2)

    # ---- six wheels: the front pair steered, the four behind on a bogie's springs --------------------------------
    v.use("vehicle")
    for x in xs_w:
        v.rod((x, -yw + 0.16, R), (x, yw - 0.16, R), 0.045, "shade", 10)
        v.ball((x, 0.0, R), 0.14, "shade", 12, 8)
    v.rod((xs_w[2], 0.0, R + 0.04), (xs_w[0], 0.0, R + 0.04), 0.024, "steel", 8)                                 # the shafts between them
    v.rod((xs_w[0] - 0.17, -yw + 0.22, R - 0.03), (xs_w[0] - 0.17, yw - 0.22, R - 0.03), 0.013, "steel", 6)
    for s in (LEFT, RIGHT):
        v.leaf_spring(xs_w[0], s * 0.70, 0.78, R + 0.07, 0.46, leaves=6)
        v.leaf_spring((xs_w[1] + xs_w[2]) / 2, s * 0.70, R + 0.09, 0.74, 0.6, leaves=7)
    for letter, s in SIDES:
        v.steered_wheel(letter, s, xs_w[0], yw, R, W, RR, lock=28.0, holes=6)
        v.lorry_wheel(f"wheel_mid_{letter}", "vehicle", (xs_w[1], s * yw, R), s, R, W, RR, holes=6)
        v.lorry_wheel(f"wheel_back_{letter}", "vehicle", (xs_w[2], s * yw, R), s, R, W, RR, holes=6)
    v.socket("dust.left", (xs_w[2] - 0.3, yw, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (xs_w[2] - 0.3, -yw, 0.05), (-1, 0, 0.3), part="vehicle")

    # ---- the turret: a round wall leaning in a little, no roof --------------------------------------------------
    x0, th = -0.05, 0.56
    r0, r1, wt = 0.74, 0.68, 0.035
    v.use("hull")
    v.ring_well((x0, 0.0, zr), 0.66)
    v.part("turret", "hull", (x0, 0.0, zr), axis=(0, 0, -1), kind="turn", note="trains all round, by hand: + to the right, clockwise seen from above; the game turns it")
    zt = zr + 0.02
    v.lathe((x0, 0, zt), Z, [(0.02, r0 - wt), (th, r1 - wt), (th + 0.014, r1 - wt * 0.5), (th, r1), (0.0, r0)], coat, v.n(56, 16))
    v.lathe((x0, 0, zt), Z, [(0.02, 0.0), (0.02, r0 - wt)], "shade", v.n(40, 12))                                 # her floor, in shadow
    v.lathe((x0, 0, zr - 0.03), Z, [(0.06, r0 + 0.01), (0.0, r0 + 0.03), (0.0, 0.0)], "shade", v.n(40, 12))
    top = zt + th
    for at in ((x0 + 0.05, 0.36), (x0 - 0.30, -0.34)):       # two seats
        v.rod((at[0], at[1], zt + 0.02), (at[0], at[1], zt + 0.22), 0.025, "steel", 8)
        v.box((0.28, 0.28, 0.06), (at[0], at[1], zt + 0.25), "leather", 0.02)
    if fine:
        for k in range(5):                                  # ready rounds in clips along her back wall
            a = math.radians(150.0 + 15.0 * k)
            at = Vector((x0 + (r0 - wt - 0.06) * math.cos(a), (r0 - wt - 0.06) * math.sin(a), zt + 0.04))
            v.rod(at, at + Z * 0.3, 0.022, "brass", 8)
        for s in (LEFT, RIGHT):
            v.lift_eye((x0 + 0.1, s * (r1 + 0.03), zt + th * 0.6), Vector((0, s, 0.1)), X, 0.028)
    pin = Vector((x0 - r1 + 0.02, 0.0, top))                  # the heavy machine gun on its pintle on her back wall
    v.rod(pin, pin + Z * 0.24, 0.02, "steel", 8)
    v.box((0.30, 0.09, 0.11), pin + Z * 0.27 + X * 0.02, "steel", 0.008)
    v.mg(pin + Z * 0.28 + X * 0.15, X, 0.9, r=0.02)
    v.box((0.16, 0.11, 0.2), pin + Z * 0.22 + Y * 0.11 + X * 0.05, "upper", 0.006)
    v.socket("mg", pin + Z * 0.28 + X * 1.07, X, part="turret", note="the heavy machine gun on her back wall")
    for s in (LEFT, RIGHT):
        v.mark(tag, (x0 - 0.1, s * (r0 + r1) / 2, zt + th * 0.5), Vector((0, s, 0.1)), 0.13, part="turret")

    # ---- the gun: a 37 mm behind a cast shield, a machine gun on its right -----------------------------------------
    tx, tz = x0 + 0.60, zt + 0.32
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-10.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.hull([(tx + 0.04, 0.29, tz - 0.22), (tx + 0.04, 0.29, tz + 0.2), (tx + 0.22, 0.22, tz - 0.16), (tx + 0.22, 0.22, tz + 0.15), (tx + 0.31, 0.1, tz - 0.1), (tx + 0.31, 0.1, tz + 0.1)],
           coat, bevel=0.03, segs=3, angle=40.0)
    v.lathe((tx + 0.28, 0, tz), X, [(0.26, 0.0), (0.26, 0.062), (0.0, 0.075), (0.0, 0.0)], coat, v.n(20, 8))
    v.slab((tx - 0.5, -0.07, tz - 0.08), (tx - 0.02, 0.07, tz + 0.08), "steel", 0.015)                            # her breech, in the open
    v.mg((tx + 0.2, -0.18, tz + 0.02), X, 0.4)
    v.socket("coax", (tx + 0.62, -0.18, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.5, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.2], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.5, 0, tz), 1.28, 0.037, r0=0.05, r1=0.031, brake=None, sleeve=None)
    v.socket("muzzle", (tx + 1.80, 0, tz), X)
    m.info["bore"] = 0.037
    m.info["brake"] = False

    v.socket("commander", (x0 - 0.3, -0.34, top - 0.25), X, part="turret", note="the commander stands here in the open turret, on the right: head and shoulders over its edge")
    v.socket("crew.gunner", (x0 + 0.05, 0.36, zt + 0.28), X, part="turret", note="the gunner sits here, on the left of the breech")
    v.socket("engine", d_at(0.42, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.42, 0.4) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (x0 - 0.1, 0.0, top), Z, part="turret", note="out of the open turret")
    v.socket("fire.3", (0.90, 0.42, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zr), Z, part="hull", note="the turret ring")
    v.socket("hit.front", g_at(g_len * 0.5, 0.0), gn, part="hull")
    v.socket("hit.left", (0.0, side_y(1.3), 1.3), sn[0], part="hull")
    v.socket("hit.right", (0.0, -side_y(1.3), 1.3), sn[1], part="hull")
    v.socket("hit.rear", (-2.44, 0.0, 1.2), -X, part="hull")
    v.socket("hit.turret", (x0, r0, zt + 0.3), Y, part="turret")
    v.socket("case", (x0 - 0.3, 0.0, zt + 0.05), -X, part="turret", note="where the empty case falls, on her floor")
    v.car_clips(recoil=0.18, kick=0.5, gun_low=-10.0, hatches=["hatch_driver", "hatch_radio"], traverse=20.0, idle_cue="lorry_idle_loop", drive_cue="lorry_drive_loop",
                gun_cue="tank_gun_01", has_turret=False, mg="coax")
    v.name_place((0.95, 1.3), long=0.7, high=0.10, lines=(2, 1), high_two=0.09)                    # her crew's name, sometimes (b477): on the hull's side
    return v


def armoured_car(fine=True, tag="_"):
    """An armoured car as most armies had one in 1939: 4.6 m long, 1.96 m wide, 2.3 m high. A lorry's chassis under
    riveted plate: a long bonnet behind armoured radiator doors, a tall fighting room with a leaning front and
    stern, swept mudguards, a spare wheel on her stern, a small eight-sided turret with a 37 mm gun and a machine
    gun. Three men."""
    v = CarWorks("ArmouredCar", "Armoured car", "car", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.8), rock=(-4.0, 4.0))
    m = v.m
    R, W, RR = 0.46, 0.2, 0.26
    xf, xr, yw = 1.45, -1.30, 0.84
    z0, zk, zr = 1.00, 1.34, 1.80                         # the body's foot, its knuckle line, the roof
    yk, yr = 0.86, 0.66
    xa, xd, xe, xb = 0.80, 0.52, -1.05, -1.80             # the scuttle, the roof's front and back, the stern's knuckle
    up_y = lambda z: yk - (z - zk) * (yk - yr) / (zr - zk)                # noqa: E731
    up_n = [Vector((0.0, s * (zr - zk), yk - yr)).normalized() for s in (LEFT, RIGHT)]

    # ---- the chassis, the bonnet, the fighting room --------------------------------------------------------------
    v.use("hull")
    v.hull([(x, 0.44, z) for x, z in [(-2.02, 0.76), (-1.92, 0.64), (2.08, 0.64), (2.2, 0.76), (2.2, 0.94), (-2.02, 0.94)]], "shade", bevel=0.008)
    nose = 2.18
    v.hull([(nose, 0.44, 0.90), (nose, 0.44, 1.30), (nose, 0.0, 1.34), (xa, 0.62, 0.90), (xa, 0.62, 1.42), (xa, 0.0, 1.47)], coat, 0.012)
    v.hull([(xa, yk, z0), (-1.76, yk, z0), (xa, yk, zk), (xb, yk, zk), (xd, yr, zr), (xe, yr, zr)], coat, 0.012)
    fl = Vector((xd - xa, 0.0, zr - zk)).normalized()                     # up the driver's plate
    fn = Vector((fl.z, 0.0, -fl.x))
    sl = Vector((xe - xb, 0.0, zr - zk)).normalized()                     # up the stern's slope
    rn = Vector((-sl.z, 0.0, sl.x))
    f_at = lambda u, y: Vector((xa, y, zk)) + fl * u + fn * 0.002          # noqa: E731
    r_at = lambda u, y: Vector((xb, y, zk)) + sl * u + rn * 0.002          # noqa: E731
    r_len = (Vector((xe, 0, zr)) - Vector((xb, 0, zk))).length
    bn = [Vector((0.18, s * (nose - xa), 0.0)).normalized() for s in (LEFT, RIGHT)]       # the bonnet's sides face a little forward
    bon_y = lambda x: 0.62 - (x - xa) * 0.18 / (nose - xa)                # noqa: E731

    # the radiator's armoured doors, the bonnet's louvres and its hinge down the middle
    for s in (LEFT, RIGHT):
        v.slab((nose, s * 0.02, 0.93), (nose + 0.022, s * 0.43, 1.28), coat, 0.006)
        if fine:
            for z in (1.0, 1.21):
                v.hinge((nose + 0.022, s * 0.43, z - 0.04), (nose + 0.022, s * 0.43, z + 0.04), 0.012)
            rivets(v, (nose + 0.023, s * 0.06, 0.96), (nose + 0.023, s * 0.06, 1.25), X, 0.1)
        v.louvres((1.5, s * bon_y(1.5), 1.13), (0.78, 0.26), bn[s < 0], count=7)
        if fine:
            rivets(v, (nose - 0.04, s * bon_y(nose - 0.04) + s * 0.001, 0.96), (xa + 0.05, s * bon_y(xa + 0.05) + s * 0.001, 0.96), bn[s < 0], 0.12)
            rivets(v, (nose - 0.04, s * (bon_y(nose - 0.04) - 0.012), 1.30), (xa + 0.05, s * (bon_y(xa + 0.05) - 0.012), 1.415), bn[s < 0], 0.12)
    v.rod((nose - 0.03, 0.0, 1.345), (xa + 0.03, 0.0, 1.473), 0.011, coat, 8)
    if fine:
        for x in (1.1, 1.85):
            for s in (LEFT, RIGHT):
                v.handle((x - 0.06, s * 0.3, 1.34 + (1.47 - 1.34) * (nose - x) / (nose - xa) - 0.022), (x + 0.06, s * 0.3, 1.34 + (1.47 - 1.34) * (nose - x) / (nose - xa) - 0.022), Z, 0.007, 0.03, "steel")
    # the bumper on its irons, hooks, lamps on the mudguards
    v.slab((2.27, -0.86, 0.66), (2.32, 0.86, 0.76), "shade", 0.012, 2)
    for s in (LEFT, RIGHT):
        v.slab((2.12, s * 0.36, 0.69), (2.28, s * 0.42, 0.73), "shade", 0.004)
        v.hook((2.2, s * 0.3, 0.84), X, 0.07)
        v.hook((-2.02, s * 0.3, 0.86), -X, 0.07)
        v.mudguard((xf, R), R + 0.1, 12.0, 112.0, s * 0.5, s * 0.98, tail=[(1.0, 0.98), (0.84, 0.78)])
        v.mudguard((xr, R), R + 0.09, 14.0, 166.0, s * 0.82, s * 0.98)
        v.headlamp((1.62, s * 0.76, R + 0.565), X, 0.085, stalk=0.11)
        v.slab((0.82, s * 0.5, 0.74), (-0.78, s * 0.9, 0.765), "shade", 0.006)                                    # the running board
    # the driver's plate: his visor on the left, a second on the right
    for s in (LEFT, RIGHT):
        v.visor(f_at(0.27, s * 0.34) + fn * 0.004, fn, 0.36, 0.12, 0.05)
    if fine:
        rivets(v, f_at(0.05, -0.80), f_at(0.05, 0.80), fn)
        rivets(v, f_at(0.49, -0.64), f_at(0.49, 0.64), fn)
    # the sides: a door in each, a port in the upper plate, rivets along every seam
    for i, s in enumerate((LEFT, RIGHT)):
        c = Vector((-0.1, s * (yk + 0.002), 1.17))
        v.box((0.62, 0.30, 0.016), c + Y * s * 0.008, coat, 0.005, turn=turn_to(X, Y * s))
        c2 = Vector((-0.1, s * up_y(1.55), 1.55)) + up_n[i] * 0.002
        v.box((0.62, 0.40, 0.016), c2 + up_n[i] * 0.008, coat, 0.005, turn=turn_to(X, up_n[i]))
        if fine:
            for z in (1.08, 1.26):
                v.hinge((0.21, s * (yk + 0.018), z - 0.04), (0.21, s * (yk + 0.018), z + 0.04), 0.012)
            v.handle((-0.36, s * (yk + 0.018), 1.14), (-0.36, s * (yk + 0.018), 1.24), Y * s, 0.008, 0.035, "steel")
            v.port(Vector((-0.1, s * up_y(1.6), 1.6)) + up_n[i] * 0.016, up_n[i], 0.036)
            v.port(Vector((-0.75, s * up_y(1.56), 1.56)), up_n[i], 0.036)
            for z in (z0 + 0.045, zk - 0.045):
                rivets(v, (xa - 0.05, s * (yk + 0.001), z), (-1.72, s * (yk + 0.001), z), Y * s)
            rivets(v, (xa - 0.12, s * up_y(zk + 0.05), zk + 0.05), (xb + 0.14, s * up_y(zk + 0.05), zk + 0.05), up_n[i])
            rivets(v, (xd - 0.04, s * up_y(zr - 0.05), zr - 0.05), (xe + 0.04, s * up_y(zr - 0.05), zr - 0.05), up_n[i])
            for x in (xa - 0.05, -1.72):
                rivets(v, (x, s * (yk + 0.001), z0 + 0.1), (x, s * (yk + 0.001), zk - 0.1), Y * s)
        v.mark(tag, (-1.0, s * (yk + 0.001), 1.17), Y * s, 0.13, part="hull")
    # the stern: the spare wheel on its slope, a door under it
    c = r_at(r_len * 0.52, 0.0)
    v.tyre(c + rn * 0.11, rn, R * 0.96, RR, W * 0.95)
    v.wheel_disc(c + rn * 0.13, rn, RR * 1.05, W * 0.6, tyre=0.0, kind="dish", hub=0.42, bolts=6, holes=6, dish=0.2, cap=0.75, sides=32)
    v.rod(c, c + rn * 0.1, 0.05, "shade", 10)
    v.slab((-1.775, -0.3, z0 + 0.03), (-1.755, 0.3, zk - 0.03), coat, 0.005)
    if fine:
        v.handle((-1.776, 0.2, 1.14), (-1.776, 0.2, 1.22), -X, 0.008, 0.035, "steel")
        for z in (1.08, 1.26):
            v.hinge((-1.776, -0.3, z - 0.04), (-1.776, -0.3, z + 0.04), 0.012)
        rivets(v, r_at(0.05, -0.8), r_at(0.05, 0.8), rn)
    v.silencer((-0.9, -0.56, 0.6), (-0.3, -0.56, 0.6), 0.06, pipe=[(0.2, -0.4, 0.66), (-0.3, -0.56, 0.6)], tail=((-0.9, -0.56, 0.6), (-1, -0.4, -0.1)))
    v.socket("exhaust.1", (-1.02, -0.62, 0.59), (-1, -0.4, -0.1))
    if fine:
        v.shovel((-0.9, yk + 0.02, 1.26), 0.0, 0.9, roll=90.0)
    v.aerial("aerial", "hull", (xe + 0.1, -0.5, zr), 1.8)

    # ---- the wheels and what carries them --------------------------------------------------------------------------
    v.use("vehicle")
    for x in (xf, xr):
        v.rod((x, -yw + 0.14, R), (x, yw - 0.14, R), 0.04, "shade", 10)
    v.ball((xr, 0.0, R), 0.13, "shade", 12, 8)
    v.rod((xr + 0.12, 0.0, R + 0.02), (0.6, 0.0, 0.7), 0.022, "steel", 8)
    v.rod((xf - 0.16, -yw + 0.2, R - 0.03), (xf - 0.16, yw - 0.2, R - 0.03), 0.012, "steel", 6)
    for s in (LEFT, RIGHT):
        v.leaf_spring(xf, s * 0.52, 0.72, R + 0.06, 0.5, leaves=6)
        v.leaf_spring(xr, s * 0.56, 0.74, R + 0.06, 0.58, leaves=7)
    for letter, s in SIDES:
        v.steered_wheel(letter, s, xf, yw, R, W, RR, lock=30.0, holes=6)
        v.lorry_wheel(f"wheel_back_{letter}", "vehicle", (xr, s * yw, R), s, R, W, RR, holes=6)
    v.socket("dust.left", (xr - 0.3, yw, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (xr - 0.3, -yw, 0.05), (-1, 0, 0.3), part="vehicle")

    # ---- the turret: eight plates riveted on a frame, a lid in its roof ---------------------------------------------
    x0, th = -0.28, 0.44
    v.use("hull")
    v.ring_well((x0, 0.0, zr), 0.5)
    v.part("turret", "hull", (x0, 0.0, zr), axis=(0, 0, -1), kind="turn", note="trains all round, by hand: + to the right, clockwise seen from above; the game turns it")
    plan = sym_plan([(0.54, 0.23), (0.23, 0.54), (-0.23, 0.54), (-0.54, 0.23)])
    top = inset_plan(plan, [0.08, 0.07, 0.07, 0.07, 0.06, 0.07, 0.07, 0.07])
    plan_w, top_w = [(x0 + x, y) for x, y in plan], [(x0 + x, y) for x, y in top]
    v.prism(plan_w, zr + 0.02, zr + th, coat, top=top_w, bevel=0.012, segs=2, angle=14.0)
    v.lathe((x0, 0, zr - 0.03), Z, [(0.06, 0.55), (0.0, 0.57), (0.0, 0.0)], "shade", v.n(32, 10))
    roof = zr + th
    riveted_prism(v, plan_w, top_w, zr + 0.02, roof, part="turret")
    v.hatch_round("hatch_commander", "turret", (x0 - 0.12, 0.0, roof), 0.2, Z, -X, thick=0.022, opens=100.0)
    v.socket("commander", (x0 - 0.12, 0.0, roof - 0.25), X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.vent((x0 + 0.24, 0.2, roof), 0.06, 0.03)
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0, s * 0.515, zr + 0.26)), Vector((0, s, 0.16)), 0.036)

    # ---- the gun: a 37 mm in a boxed mantlet, a machine gun on its right ----------------------------------------------
    tx, tz = x0 + 0.44, zr + 0.24
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.box((0.11, 0.42, 0.3), (tx + 0.08, 0.0, tz), "upper", 0.02, segs=2)
    v.lathe((tx + 0.07, 0, tz), Y, [(0.16, 0.0), (0.16, 0.09), (0.13, 0.12), (-0.13, 0.12), (-0.16, 0.09), (-0.16, 0.0)], "upper", v.n(24, 8))
    if fine:
        for y in (-0.17, 0.17):
            for z in (-0.11, 0.11):
                v.bolt((tx + 0.136, y, tz + z), X, 0.011)
    v.mg((tx + 0.1, -0.13, tz + 0.02), X, 0.38)
    v.socket("coax", (tx + 0.5, -0.13, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.13, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.18], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.13, 0, tz), 1.15, 0.037, r0=0.047, r1=0.03, brake=None, sleeve=None)
    v.socket("muzzle", (tx + 1.30, 0, tz), X)
    m.info["bore"] = 0.037
    m.info["brake"] = False

    v.socket("engine", (1.5, 0.0, 1.45), Z, part="hull", note="the bonnet: smoke and the first fire")
    v.socket("fire.1", (1.5, 0.3, 1.42), Z, part="hull")
    v.socket("fire.2", (x0 - 0.12, 0.0, roof + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("ring", (x0, 0.0, zr), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", f_at(0.27, 0.0), fn, part="hull")
    v.socket("hit.left", (-0.4, yk, 1.2), Y, part="hull")
    v.socket("hit.right", (-0.4, -yk, 1.2), -Y, part="hull")
    v.socket("hit.rear", r_at(r_len * 0.3, 0.0), rn, part="hull")
    v.socket("hit.turret", (x0, 0.54, zr + 0.22), Y, part="turret")
    v.socket("case", (x0 - 0.2, -0.5, zr + 0.25), -Y, part="turret", note="where the empty case is thrown out")
    v.car_clips(recoil=0.15, kick=0.5, gun_low=-8.0, hatches=["hatch_commander"], traverse=18.0, idle_cue="lorry_idle_loop", drive_cue="lorry_drive_loop",
                gun_cue="tank_gun_01", blow=(-0.6, 2.0, 0.3, 165.0), mg="coax")
    v.camouflage()
    return v
