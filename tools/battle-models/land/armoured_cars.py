"""Armoured cars: the scouts on wheels, built on the carriers' kit (wheels on tyres, steering) with a tank's turret
and a tank's clips.

    SdKfz222      the German light armoured car: a faceted body widest at a knuckle line, pointed at both ends,
                  angular mudguards, a low ten-sided turret open to the sky under two wire screens, a 2 cm
                  automatic gun and a machine gun beside it.

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
from motorpool import LEFT, RIGHT, SIDES, X, Y, Z
from tanks_any import turn_to
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

    def car_clips(self, burst=0, every=0.13, burst_cue="flak_light_fire", **kw):
        """A tank's clips for a car on wheels. Her front wheels flop over when she is knocked out, the game steers
        them, and with `burst` her gun is an automatic one: `fire` is that many rounds, `every` seconds apart."""
        self.tank_clips(**kw)
        m = self.m
        m.clips["start"]["events"][0]["cue"] = None                 # (a lorry's engine, not a tank's)
        steers = [p for p in ("steer_l", "steer_r") if p in m.part_at]
        for p in steers:
            m.clips["knocked_out"]["tracks"].append({"part": p, "turn": [[0.0, 0.0], [0.2, 0.0], [0.75, 15.0], [0.9, 13.0], [1.05, 14.0], [3.0, 14.0]]})
        if burst:
            recoil = kw.get("recoil", 0.05)
            keys, events = [[0.0, 0.0]], []
            for i in range(burst):
                t = i * every
                keys += [[round(t + 0.02, 3), recoil], [round(t + every * 0.8, 3), 0.0]]
                events.append({"t": round(t, 3), "fx": "muzzle_flash", "socket": "muzzle", "cue": burst_cue if i == 0 else None})
            long = round(burst * every + 0.6, 2)
            keys.append([long, 0.0])
            m.clips["fire"] = {"long": long, "loop": False, "after": "hold", "about": f"a burst from her automatic gun: {burst} rounds, the barrel jumping to each",
                               "tracks": [{"part": "barrel", "slide": keys}], "events": events}
        ctl = m.info["file"]["controls"]
        if steers:
            ctl["steer"] = {"parts": steers, "what": "the game steers the front wheels inside their lock: + to the right", "limits": m.parts[m.part_at[steers[0]]].limits}
        return self


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


_ = (RIGHT,)
