"""The Soviet fast tank: the BT-7 of 1935 (the round turret with its box at the back, the 45 mm gun).

Modelled to the real type's published measurements and layout (5.66 m long, 2.29 m wide, 2.42 m high, 13.9 tonnes;
four big rubber-tyred road wheels a side on Christie springs inside the double hull wall and no return rollers, the
track's top run lying on the wheels; the drive at the back, the idler on a narrow nose; the driver in the middle of
the front; the engine filling the long back half). Every shape is our own mesh, made from numbers here: nothing traced
or taken from any other model. What tells her from across a field: how long and low her hull runs behind a small turret
set forward, the four big wheels with the track sagging between them, the pointed nose.
"""

from mathutils import Vector

from motorpool import LEFT, RIGHT, X, Y, Z
from tanks_any import rivets
from tanks_ger import coat
from tanks_jap import ring_rivets
from works import Works


def bt7(fine=True, tag="SOV"):
    """BT-7 (1935): the fast tank of the tank brigades from Khalkhin Gol to the first year of the German war."""
    v = Works("BT7", "BT-7 fast tank", "tank", tag, fine)
    # (the art director's E7 surface pass from the first build: shade from her shape, dirt of her ground, the steppe)
    v.dice = 0.2
    v.worn_kw = dict(shade=True, shade_deep=0.42, ground="steppe", layers=2, marks=0.3, gear=0.8, rim=True)
    v.root(hull_pivot=(0.0, 0.0, 0.75))
    hw, zg, zr = 0.82, 0.96, 1.64        # the hull's outer wall (the Christie springs stand between it and the inner one); the guards; the roof

    # ---- the hull: a long narrow box, a glacis down to a pointed nose that carries the idlers -------------------------
    xf, zf = 2.38, 1.12                  # the glacis' foot, where the nose begins
    xn, zn, yn = 2.84, 0.96, 0.50        # the nose's top front edge and its half-width
    v.hull([(x, hw, z) for x, z in [(xf, zf), (1.78, zr), (-2.30, zr), (-2.62, 1.42), (-2.78, 1.05), (-2.70, 0.62), (-2.40, 0.40), (2.30, 0.40)]], coat, bevel=0.014)
    v.hull([(2.30, hw, 0.40), (xf, hw, zf), (xn, yn, zn), (xn, yn, 0.70), (2.56, 0.60, 0.42)], coat, bevel=0.012)
    gl = Vector((1.78 - xf, 0, zr - zf)).normalized()                 # up the glacis
    gn = Vector((gl.z, 0, -gl.x))
    nl = Vector((xn - xf, 0, zn - zf)).normalized()                   # forward along the nose's top
    nn = Vector((-nl.z, 0, nl.x))
    if nn.z < 0:
        nn = -nn
    rl = Vector((-2.62 + 2.30, 0, 1.42 - zr)).normalized()            # down the plate over the engine's back
    rn = Vector((rl.z, 0, -rl.x))
    if rn.x > 0:
        rn = -rn
    g_at = lambda u, y: Vector((xf, y, zf)) + gl * u + gn * 0.002      # noqa: E731
    n_at = lambda u, y: Vector((xf, y, zf)) + nl * u + nn * 0.002      # noqa: E731
    r_at = lambda u, y: Vector((-2.30, y, zr)) + rl * u + rn * 0.002   # noqa: E731
    # the driver in the middle: a big lid in the glacis hung at its top, his slit in it
    v.hatch_rect("hatch_driver", "hull", g_at(0.36, 0.0), (0.40, 0.52), gn, gl, hinge="front", opens=95.0, thick=0.022)
    v.visor(g_at(0.30, 0.0) + gn * 0.022, gn, 0.24, 0.07, 0.03, part="hatch_driver")
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.headlamp(n_at(0.22, s * 0.36) + nn * 0.01, X, 0.085, stalk=0.07)
        v.hook((xn + 0.02, s * 0.34, 0.78), X, 0.08)
        v.hook((-2.79, s * 0.42, 0.80), -X, 0.08)
    if fine:
        rivets(v, g_at(0.03, -0.76), g_at(0.03, 0.76), gn, 0.11)
        rivets(v, g_at(0.76, -0.76), g_at(0.76, 0.76), gn, 0.11)
        rivets(v, n_at(0.40, -0.52), n_at(0.40, 0.52), nn, 0.11)
        for s in (LEFT, RIGHT):
            sn = Vector((0, s, 0))
            for z in (zg + 0.05, zr - 0.05):
                rivets(v, (1.72, s * (hw + 0.001), z), (-2.26, s * (hw + 0.001), z), sn, 0.12)
            for x in (1.10, -0.40, -1.60):
                rivets(v, (x, s * (hw + 0.001), zg + 0.12), (x, s * (hw + 0.001), zr - 0.12), sn, 0.12)
            v.port(Vector((1.30, s * (hw + 0.004), 1.38)), sn, 0.035)

    # ---- the track guards: long flat shelves, sloping down over the idler and the sprocket -------------------------
    for s in (LEFT, RIGHT):
        y0, y1 = s * hw, s * 1.14
        v.slab((-2.42, y0, zg - 0.016), (2.20, y1, zg), coat, 0.004)
        v.plate([(2.20, y0, zg), (2.20, y1, zg), (2.82, y1, 0.86), (2.82, y0, 0.86)][::int(s)], 0.01, "upper", 0.003)
        v.plate([(-2.42, y0, zg), (-2.42, y1, zg), (-2.92, y1, 0.88), (-2.92, y0, 0.88)][::-int(s)], 0.01, "upper", 0.003)
        if fine:
            for x in (-1.9, -0.7, 0.5, 1.7):
                v.slab((x - 0.012, s * hw, zg - 0.09), (x + 0.012, s * 1.11, zg - 0.016), "shade")
    v.shovel((-0.6, 1.02, zg + 0.02), 0.0, 0.95)
    v.crowbar((0.4, -1.03, zg + 0.018), 0.0, 1.2)
    v.bin((-2.30, -1.12, zg), (-1.60, -0.86, zg + 0.20), lid_side=Y)
    v.bin((1.20, 0.86, zg), (1.95, 1.12, zg + 0.18), lid_side=-Y)

    # ---- the engine deck: louvres either side of the long lid over the engine, the radiator's grille at the back ----
    for s in (LEFT, RIGHT):
        v.louvres((-1.30, s * 0.46, zr + 0.012), (1.20, 0.50), Z, count=9, down=-X)
        v.box((1.24, 0.54, 0.018), (-1.30, s * 0.46, zr + 0.003), "upper", 0.005)
        v.louvres(r_at(0.20, s * 0.40) + rn * 0.012, (0.24, 0.62), rn, count=3, down=rl)
    v.box((1.40, 0.30, 0.03), (-1.32, 0.0, zr + 0.015), "upper", 0.008)
    if fine:
        for x in (-0.72, -1.92):
            v.handle(Vector((x, -0.08, zr + 0.03)), Vector((x, 0.08, zr + 0.03)), Z, 0.009, 0.04)
        rivets(v, (-0.60, -0.76, zr + 0.002), (-0.60, 0.76, zr + 0.002), Z, 0.12)
    # the two exhausts out of the back plate, down-turned
    for k, s in enumerate((LEFT, RIGHT)):
        v.tube([(-2.70, s * 0.34, 1.02), (-2.86, s * 0.34, 1.00), (-2.92, s * 0.34, 0.92)], 0.055, "rust", v.n(12, 6))
        v.socket("exhaust." + str(k + 1), (-2.93, s * 0.34, 0.90), (-0.6, 0.0, -0.8), part="hull")

    # ---- the running gear: four big wheels a side, twin-tyred round the guide horns, the top run on the wheels -------
    ty, r_w, thick = 0.99, 0.415, 0.025
    wz = 0.44
    xs = [-1.80, -0.60, 0.60, 1.80]
    v.running(xs, r_w, ty, (-2.55, 0.56, 0.29), (2.55, 0.52, 0.28), drive="rear", top_on_wheels=True, wheel_z=wz,
              wheel=dict(w=0.085, tyre=0.05, gap=0.075, bolts=8, holes=6, sides=32, hub=0.26, axle=0.18),
              sprocket_kw=dict(teeth=12, wide=0.24, tooth=0.022, holes=6, bolts=8, sides=26, hub=0.40),
              idler_kw=dict(wide=0.24, kind="dish", holes=6, tyre=0.03),
              track=dict(pitch=0.25, wide=0.26, thick=thick, sag=0.045, style={"kind": "plate", "horn": "centre", "horn_high": 0.075, "grouser": 0.012, "cleats": False, "bar": 0.3}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):
        for x in xs:                                     # each wheel's arm, swinging back from its pivot in the hull wall
            v.rod((x + 0.22, s * (hw - 0.01), 0.60), (x, s * (ty - 0.15), wz), 0.045, "shade", 8)
        v.rod((2.55, s * (yn + 0.08), 0.52), (2.55, s * (ty - 0.14), 0.52), 0.05, "shade", 10)       # the idler's arm out of the nose
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((-2.55, s * (hw - 0.01), 0.56), Y * s, [(0.16, 0.0), (0.16, 0.09), (0.08, 0.13), (0.0, 0.14), (0.0, 0.0)], "upper", v.n(20, 8))

    # ---- the turret, set forward: a short rolled drum, a box out behind it, one big lid ---------------------------------
    x0, zt, th = 0.55, zr, 0.60
    v.ring_well((x0, 0.0, zt), 0.66)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    low = {"z": zt + 0.02, "xc": x0, "yc": 0.0, "af": 0.64, "ar": 0.64, "b": 0.64, "n": 2.0}
    high = {"z": zt + th - 0.025, "xc": x0, "yc": 0.0, "af": 0.64, "ar": 0.64, "b": 0.64, "n": 2.0}
    v.cast([low, high], "upper", sides=v.n(48, 14), top=0.025, steps=2, between=2)
    v.hull([(x0 - 0.36, 0.47, zt + 0.08), (x0 - 0.98, 0.44, zt + 0.16), (x0 - 1.00, 0.44, zt + th - 0.04), (x0 - 0.36, 0.47, zt + th - 0.015)], "upper", bevel=0.02, part="turret")
    v.lathe((x0, 0.0, zt - 0.03), Z, [(0.06, 0.66), (0.0, 0.68), (0.0, 0.0)], "shade", v.n(32, 10))
    roof = zt + th
    ring_rivets(v, low, zt + 0.07, 40, part="turret")
    ring_rivets(v, high, roof - 0.06, 40, part="turret")
    v.hatch_rect("hatch_commander", "turret", (x0 - 0.30, 0.0, roof), (0.56, 0.50), Z, X, hinge="front", opens=100.0, thick=0.022)
    v.socket("commander", (x0 - 0.30, 0.10, roof - 0.25), X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.periscope((x0 + 0.30, 0.30, roof), X, (0.08, 0.15, 0.10))
    v.vent((x0 + 0.26, -0.28, roof), 0.08, 0.04)
    if fine:
        for s in (LEFT, RIGHT):
            n = Vector((0.45, s * 0.89, 0.0)).normalized()
            v.visor(Vector((x0 + 0.29, s * 0.57, zt + 0.40)), n, 0.16, 0.06, 0.03, brow=False)
            v.port(Vector((x0 - 0.62, s * 0.475, zt + 0.32)), Vector((0, s, 0)), 0.035)
            rivets(v, (x0 - 0.40, s * 0.471, roof - 0.04), (x0 - 0.97, s * 0.441, roof - 0.04), Vector((0, s, 0)), 0.10)
        rivets(v, (x0 - 1.001, 0.40, zt + 0.40), (x0 - 1.001, -0.40, zt + 0.40), -X, 0.10)
    # (no star: bare is the common read for Khalkhin Gol and 1941; the art director, 22 Nov 20:46Z, for the historian to confirm)

    # ---- the gun: the 45 mm in a flat shield, the machine gun beside it on its right ---------------------------------
    tx, tz = x0 + 0.60, zt + 0.30
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 25.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx, 0.0, tz), Y, [(0.26, 0.0), (0.26, 0.10), (0.22, 0.13), (-0.22, 0.13), (-0.26, 0.10), (-0.26, 0.0)], "upper", v.n(24, 10))
    v.box((0.05, 0.56, 0.30), (tx + 0.12, 0.0, tz), "upper", 0.012)
    v.lathe((tx + 0.145, 0.0, tz), X, [(0.10, 0.0), (0.10, 0.07), (0.04, 0.08), (0.0, 0.085), (0.0, 0.0)], "upper", v.n(18, 8))
    v.mg((tx + 0.145, -0.17, tz - 0.01), X, 0.14)
    v.socket("coax", (tx + 0.29, -0.17, tz - 0.01), X)
    v.part("barrel", "gun", (tx + 0.24, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.25], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.24, 0.0, tz), 1.52, 0.045, r0=0.058, r1=0.043, brake=None, sleeve=(0.24, 0.072))
    v.socket("muzzle", (tx + 1.76, 0.0, tz), X)
    v.m.info["bore"] = 0.045
    v.m.info["brake"] = False

    # ---- sockets for what happens to her ------------------------------------------------------------------------
    v.socket("engine", (-1.30, 0.0, zr + 0.05), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-1.30, 0.46, zr + 0.05), Z, part="hull")
    v.socket("fire.2", (x0 - 0.30, 0.0, roof + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", g_at(0.36, 0.0) + gn * 0.05, gn, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(0.40, -0.40), gn, part="hull")
    v.socket("hit.left", (0.0, hw + 0.01, 1.30), Y, part="hull")
    v.socket("hit.right", (0.0, -hw - 0.01, 1.30), -Y, part="hull")
    v.socket("hit.rear", (-2.78, 0.0, 0.90), -X, part="hull")
    v.socket("hit.turret", (x0, 0.65, zt + 0.30), Y, part="turret")
    v.socket("case", (x0 - 0.30, -0.64, zt + 0.32), -Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.25, kick=0.5, gun_low=-8.0, hatches=["hatch_commander", "hatch_driver"],
                 traverse=18.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.7, 2.2, 0.36, 165.0))
    v.name_place((x0 + 0.02, zt + 0.30), long=0.62, high=0.10, part="turret", lines=2, bend=0.035)     # the slogan (b477): on the turret's side, in two lines
    return v
