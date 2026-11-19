"""Italian tanks.

The M13/40: the medium tank of the desert war. What tells her from across a field: a tall riveted box of a crew
room standing the full width of her over the tracks, two machine guns side by side in the front plate, a rounded
nose over the drive, eight small wheels a side in two sprung bogies of four, a horseshoe-shaped riveted turret with a
47 mm gun. Every shape is our own mesh, made from numbers here.
"""

import math

from mathutils import Vector

from motorpool import LEFT, RIGHT, X, Y, Z, inset_plan, sym_plan
from tanks_any import leaf_bogie, riveted_prism, rivets, turn_to
from tanks_ger import coat
from works import Works


HW, SW, ZG, ZR = 0.66, 1.02, 1.06, 1.76          # half-width between the tracks and of the crew's box; the guards; the roof


def _front(v, fine):
    """The tub, the round nose over the drive and the sloped plate above it, with its lids, hooks and lamps."""
    hw, zg = HW, ZG
    v.hull([(x, hw, z) for x, z in [(-2.34, 0.56), (-2.20, 0.40), (1.90, 0.40), (2.12, 0.46), (2.12, zg - 0.02), (-2.40, zg - 0.02), (-2.40, 0.70)]], coat, bevel=0.012)
    v.lathe((2.10, -hw + 0.01, 0.74), Y, [(0.0, 0.0), (0.0, 0.29), (0.012, 0.30), (2 * hw - 0.032, 0.30), (2 * hw - 0.02, 0.29), (2 * hw - 0.02, 0.0)], "upper", v.n(32, 10))
    v.hull([(2.16, hw, zg - 0.03), (1.34, hw, 1.33), (1.34, hw, zg - 0.03)], coat, bevel=0.012)
    gl = Vector((1.34 - 2.16, 0, 1.33 - (zg - 0.03))).normalized()   # up the sloped plate
    gn = Vector((gl.z, 0, -gl.x))
    g_at = lambda u, y: Vector((2.16, y, zg - 0.03)) + gl * u + gn * 0.002     # noqa: E731
    for s in (LEFT, RIGHT):                              # a lid over each brake, the hooks, the lamps
        v.box((0.44, 0.40, 0.018), g_at(0.44, s * 0.32) + gn * 0.009, "upper", 0.005, turn=turn_to(gl, gn))
        if fine:
            v.handle(g_at(0.30, s * 0.32) - Y * 0.07 + gn * 0.018, g_at(0.30, s * 0.32) + Y * 0.07 + gn * 0.018, gn, 0.007, 0.03, "steel")
            for dy in (-0.12, 0.12):
                v.hinge(g_at(0.66, s * 0.32 + dy) - Y * 0.04 + gn * 0.018, g_at(0.66, s * 0.32 + dy) + Y * 0.04 + gn * 0.018, 0.011)
            rivets(v, g_at(0.05, s * 0.62), g_at(0.82, s * 0.62), gn, 0.1)
            for a in (-50.0, -15.0, 20.0, 55.0):         # the bolts round the nose's ends
                v.bolt((2.10 + 0.25 * math.cos(math.radians(a)), s * (hw - 0.008), 0.74 + 0.25 * math.sin(math.radians(a))), Y * s, 0.013, 0.008)
        v.hook((2.36, s * 0.40, 0.60), Vector((0.8, 0, -0.5)).normalized(), 0.08)
        v.hook((-2.40, s * 0.40, 0.66), -X, 0.08)
        v.headlamp((1.96, s * 0.90, zg), X, 0.065, stalk=0.1)
    return g_at, gl, gn


def _crew_box(v, fine):
    """The M13's crew box, standing over the tracks: the driver's visor on the left, two machine guns on the right,
    the crew's door in the left side."""
    sw, zg, zr = SW, ZG, ZR
    v.hull([(1.38, sw, zg), (1.26, sw - 0.02, zr), (-0.60, sw - 0.02, zr), (-0.60, sw, zg)], coat, bevel=0.012)
    fl = Vector((1.26 - 1.38, 0, zr - zg)).normalized()
    fn = Vector((fl.z, 0, -fl.x))
    f_at = lambda u, y: Vector((1.38, y, zg)) + fl * u + fn * 0.002           # noqa: E731
    v.visor(f_at(0.44, 0.46) + fn * 0.004, fn, 0.38, 0.13, 0.05)
    v.port(f_at(0.44, 0.0), fn, 0.04)
    c = f_at(0.36, -0.46)
    v.lathe(c - fn * 0.02, fn, [(0.0, 0.0), (0.0, 0.23), (0.05, 0.23), (0.09, 0.19), (0.12, 0.0)][::-1], "upper", v.n(28, 10))
    for dy in (-0.06, 0.06):
        v.mg(c + fn * 0.10 + Y * dy, fn, 0.34)
    v.socket("hull_mg", c + fn * 0.46, fn)
    if fine:
        v.bolts(c + fn * 0.031, fn, 0.20, 8, r=0.012)
        for s in (LEFT, RIGHT):
            sn = Vector((0, s, 0.03)).normalized()
            for z in (zg + 0.06, zr - 0.06):
                rivets(v, (1.24, s * (sw - 0.008), z), (-0.54, s * (sw - 0.008), z), sn, 0.11)
            for x in (1.24, 0.36, -0.54):
                rivets(v, (x, s * (sw - 0.006), zg + 0.17), (x, s * (sw - 0.014), zr - 0.17), sn, 0.11)
        for u in (0.06, 0.66):
            rivets(v, f_at(u, -0.94), f_at(u, 0.94), fn, 0.11)
    # the crew's door in the left side
    v.hatch_rect("hatch_side", "hull", (0.80, sw - 0.012, zg + 0.36), (0.56, 0.52), Vector((0, 1, 0.03)).normalized(), X, hinge="front", opens=110.0, thick=0.02, well=False)
    v.use("hull")
    v.periscope((1.02, 0.46, zr), X, (0.07, 0.14, 0.07))
    return f_at, fn


def _back(v, fine, aerial=(-0.46, -0.90, ZR)):
    """The track guards, the engine deck, a silencer on each rear guard, the tools; the aerial at `aerial`."""
    hw, zg = HW, ZG
    for s in (LEFT, RIGHT):
        y0, y1 = s * hw, s * 1.10
        v.slab((-2.40, y0, zg - 0.016), (2.14, y1, zg), coat, 0.004)
        v.plate([(2.14, y0, zg), (2.14, y1, zg), (2.36, y1, 0.90), (2.36, y0, 0.90)][::int(s)], 0.01, "upper", 0.003)
        v.plate([(-2.40, y1, zg), (-2.40, y0, zg), (-2.52, y0, 0.86), (-2.52, y1, 0.86)][::int(s)], 0.01, "upper", 0.003)
        if fine:
            for x in (-1.9, -1.0, 1.7):
                v.slab((x - 0.012, s * hw, zg - 0.10), (x + 0.012, s * 1.07, zg - 0.016), "shade")
        v.silencer((-1.10, s * 0.99, zg + 0.10), (-2.00, s * 0.99, zg + 0.10), 0.078, bands=2, tail=((-2.00, s * 0.99, zg + 0.10), (-1.0, 0, 0.05)))
        v.tube([(-0.90, s * 0.90, zg + 0.22), (-0.96, s * 0.99, zg + 0.14), (-1.10, s * 0.99, zg + 0.10)], 0.034, "rust", v.n(10, 5))
        v.socket("exhaust." + ("1" if s > 0 else "2"), (-2.16, s * 0.99, zg + 0.12), (-1.0, 0, 0.1))
    v.hull([(-0.60, 0.94, zg), (-0.60, 0.92, 1.50), (-2.24, 0.88, 1.30), (-2.42, 0.92, zg)], coat, bevel=0.012)
    dk = Vector((-2.24 + 0.60, 0, 1.30 - 1.50)).normalized()          # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))
    d_at = lambda u, y: Vector((-0.60, y, 1.50)) + dk * u + dn * 0.002        # noqa: E731
    for s in (LEFT, RIGHT):
        v.box((0.84, 0.60, 0.02), d_at(0.52, s * 0.36) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
        v.louvres(d_at(0.52, s * 0.36) + dn * 0.02, (0.44, 0.70), dn, count=7, down=dk)
        v.box((0.50, 0.62, 0.02), d_at(1.30, s * 0.36) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
        v.grille(d_at(1.30, s * 0.36) + dn * 0.02, (0.56, 0.42), dn, Y, slats=6, deep=0.02)
        if fine:
            for u in (0.25, 0.80):
                v.hinge(d_at(u - 0.05, s * 0.045) + dn * 0.02, d_at(u + 0.05, s * 0.045) + dn * 0.02, 0.011)
            rivets(v, d_at(0.06, s * 0.84), d_at(1.58, s * 0.80), dn, 0.12)
    v.shovel((0.3, 1.06, zg + 0.34), 0.0, 0.9, roll=90.0)
    v.jack((-0.86, -1.0, zg), 0.0)
    v.bin((1.42, -1.08, zg), (1.90, -0.72, zg + 0.18), lid_side=-Y)
    v.aerial("aerial", "hull", aerial, 1.8)
    return d_at, dk, dn


def _running(v, fine):
    """Two bogies of four wheels a side, each on its own leaf spring; the drive's and the idler's housings."""
    hw = HW
    ty, r_w, thick = 0.95, 0.16, 0.022
    wz = r_w + thick
    xs = [-1.55, -1.17, -0.75, -0.37, 0.15, 0.53, 0.95, 1.33]
    v.running(xs, r_w, ty, (1.98, 0.70, 0.27), (-2.04, 0.58, 0.25), rollers=[(-1.15, 0.86, 0.085), (-0.05, 0.86, 0.085), (1.05, 0.86, 0.085)], wheel_z=wz,
              wheel=dict(w=0.05, tyre=0.022, gap=0.05, bolts=0, sides=26, hub=0.4, axle=0.16),
              sprocket_kw=dict(teeth=14, wide=0.2, tooth=0.028, holes=6, bolts=8, sides=28, hub=0.42),
              idler_kw=dict(wide=0.18, kind="spoke", spokes=6),
              roller_kw=dict(wide=0.12, tyre=0.012, reach=0.24),
              track=dict(pitch=0.09, wide=0.26, thick=thick, sag=0.014, style={"kind": "plate", "horn": "centre", "horn_high": 0.055, "grouser": 0.011, "cleats": False, "bar": 0.3}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):
        yi = s * (ty + 0.095)
        for k in (0, 4):
            ca, cb = (xs[k] + xs[k + 1]) / 2, (xs[k + 2] + xs[k + 3]) / 2
            leaf_bogie(v, ca, cb, s * (ty + 0.115), s * hw, 0.33, s, top=0.60, fine=fine)
            for cc, (xa, xb) in ((ca, (xs[k], xs[k + 1])), (cb, (xs[k + 2], xs[k + 3]))):     # each pair's rocker, down from the spring's end to the axles
                for xw in (xa, xb):
                    v.rod((cc, yi, 0.33), (xw, yi, wz), 0.026, "shade", 8)
                    if fine:
                        v.lathe((xw, yi - s * 0.01, wz), Y * s, [(0.035, 0.0), (0.035, 0.035), (0.0, 0.048), (0.0, 0.0)], "upper", 10)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((1.98, s * (hw - 0.01), 0.70), Y * s, [(0.14, 0.0), (0.14, 0.14), (0.07, 0.2), (0.0, 0.22), (0.0, 0.0)], "upper", v.n(20, 8))
        v.lathe((-2.04, s * (hw - 0.01), 0.58), Y * s, [(0.14, 0.0), (0.14, 0.07), (0.0, 0.1), (0.0, 0.0)], "upper", v.n(14, 6))


def m13(fine=True, tag="ITA"):
    """M13/40: 4.9 m long, 2.2 m wide, 2.35 m high. The drive at the front, the engine behind under a deck with a
    silencer on each rear guard."""
    v = Works("M13", "Carro Armato M13/40", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.68))
    sw, zg, zr = SW, ZG, ZR
    g_at, gl, gn = _front(v, fine)
    f_at, fn = _crew_box(v, fine)
    d_at, dk, dn = _back(v, fine)
    _running(v, fine)

    # ---- the turret: riveted plate bent to a horseshoe, one wide lid in its roof ------------------------------
    x0, zt, th = 0.38, zr, 0.54
    v.ring_well((x0, 0.0, zt), 0.66)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    # (flat cheeks to the front, the back rolled round in one sweep: twelve degrees a plate, so it shades as a curve)
    plan = sym_plan([(0.68, 0.28), (0.46, 0.62)] + [(-0.09 + 0.67 * math.cos(math.radians(a)), 0.67 * math.sin(math.radians(a))) for a in range(90, 175, 12)])
    top = inset_plan(plan, [0.08] + [0.07] * (len(plan) - 2) + [0.05])
    plan_w = [(x0 + x, y) for x, y in plan]
    top_w = [(x0 + x, y) for x, y in top]
    v.prism(plan_w, zt + 0.02, zt + th, coat, top=top_w, bevel=0.014, segs=2, angle=14.0)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.66), (0.0, 0.68), (0.0, 0.0)], "shade", v.n(32, 10))
    roof = zt + th
    riveted_prism(v, plan_w, top_w, zt + 0.02, roof, part="turret", every=0.105)
    v.hatch_rect("hatch_commander", "turret", (x0 - 0.16, 0.0, roof), (0.52, 0.70), Z, X, hinge="back", opens=100.0, thick=0.024, split=True)
    v.socket("commander", (x0 - 0.16, 0.0, roof - 0.25), X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    for s in (LEFT, RIGHT):
        v.periscope((x0 + 0.30, s * 0.30, roof), X, (0.06, 0.12, 0.06))
        if fine:
            v.port(Vector((x0 + 0.02, s * 0.60, zt + 0.30)), Vector((0, s, 0.13)), 0.04)
        v.mark("ITA", (x0 - 0.42, s * 0.565, zt + 0.30), Vector((-0.36, s, 0.13)), 0.10, part="turret")
    if fine:
        v.port(Vector((x0 - 0.725, 0.0, zt + 0.30)), Vector((-1, 0, 0.11)), 0.04)

    # ---- the gun: a 47 mm, its cradle sleeved, a machine gun on its right ---------------------------------------
    tx, tz = x0 + 0.56, zt + 0.27
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-10.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx, 0, tz), Y, [(0.30, 0.0), (0.30, 0.19), (0.26, 0.225), (-0.26, 0.225), (-0.30, 0.19), (-0.30, 0.0)], "upper", v.n(30, 10))
    v.box((0.10, 0.50, 0.36), (tx + 0.13, 0.0, tz), "upper", 0.03, segs=3)
    if fine:
        for y in (-0.2, 0.2):
            for z in (-0.13, 0.13):
                v.bolt((tx + 0.181, y, tz + z), X, 0.012)
    v.mg((tx + 0.14, -0.16, tz + 0.02), X, 0.34)
    v.socket("coax", (tx + 0.50, -0.16, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.18, 0.04, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.2], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.18, 0.04, tz), 1.20, 0.047, r0=0.06, r1=0.04, brake=None, sleeve=(0.36, 0.078))
    v.socket("muzzle", (tx + 1.40, 0.04, tz), X)
    v.m.info["bore"] = 0.047
    v.m.info["brake"] = False

    # ---- sockets for what happens to her ------------------------------------------------------------------------
    v.socket("engine", d_at(0.6, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.52, 0.36) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (x0 - 0.16, 0.0, roof + 0.1), Z, part="turret", note="out of the turret's lid")
    v.socket("fire.3", (0.80, sw + 0.05, zg + 0.36), Y, part="hull", note="out of the crew's door")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", f_at(0.35, 0.0), fn, part="hull")
    v.socket("hit.left", (0.2, sw, 1.4), Y, part="hull")
    v.socket("hit.right", (0.2, -sw, 1.4), -Y, part="hull")
    v.socket("hit.rear", (-2.4, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", (x0, 0.64, zt + 0.27), Y, part="turret")
    v.socket("case", (x0 - 0.3, -0.64, zt + 0.3), -Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.18, kick=0.5, gun_low=-10.0, hatches=["hatch_commander", "hatch_side"],
                 traverse=14.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.7, 2.2, 0.34, 165.0))
    v.name_place((0.6, 1.4), long=0.55, high=0.09, sides=(LEFT,))      # a name was seldom seen (b477): small, on the hull's side, one side only
    return v


ZC = 1.80          # the Semovente's roof


def semovente(fine=True, tag="ITA"):
    """Semovente 75/18: the M13's hull and running gear with a low fixed fighting box where her crew box and turret
    stood, its front plate leaning back, a short 75 mm howitzer in a cast mounting right of the middle. 4.9 m long,
    2.2 m wide, 1.85 m high. Italy's assault gun of 1941 to 1943: what the desert called her was a gun that could
    stand off a British tank behind a low crest."""
    v = Works("Semovente", "Semovente da 75/18", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.68))
    sw, zg = SW, ZG
    g_at, gl, gn = _front(v, fine)

    # ---- the fighting box: front plate leaning back, sides straight down over the guards, a flat roof ----------
    xf0, xf1, xr = 1.40, 1.06, -0.62
    v.hull([(xf0, sw, zg), (xf1, sw - 0.03, ZC), (xr, sw - 0.03, ZC), (xr, sw, zg)], coat, bevel=0.014)
    fl = Vector((xf1 - xf0, 0, ZC - zg)).normalized()
    fn = Vector((fl.z, 0, -fl.x))
    f_at = lambda u, y: Vector((xf0, y, zg)) + fl * u + fn * 0.002       # noqa: E731
    u_top = (Vector((xf1, 0, ZC)) - Vector((xf0, 0, zg))).length
    v.visor(f_at(0.50, 0.50) + fn * 0.004, fn, 0.36, 0.12, 0.05)
    sn = [Vector((0, s, 0.04)).normalized() for s in (1.0, -1.0)]
    if fine:
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            for z in (zg + 0.06, ZC - 0.06):
                rivets(v, (1.30 - (z - zg) * 0.43, s * (sw - 0.008 - (z - zg) * 0.04), z), (xr + 0.06, s * (sw - 0.008 - (z - zg) * 0.04), z), n, 0.11)
            for x in (0.56, -0.02, xr + 0.06):
                rivets(v, (x, s * (sw - 0.009), zg + 0.17), (x, s * (sw - 0.033), ZC - 0.17), n, 0.11)
            v.port(Vector((0.20, s * (sw - 0.018), 1.46)), n, 0.04)
        for u in (0.06, u_top - 0.06):
            rivets(v, f_at(u, -0.94), f_at(u, 0.94), fn, 0.11)
        for y in (-0.94, 0.94):
            rivets(v, f_at(0.12, y), f_at(u_top - 0.12, y), fn, 0.11)
        rivets(v, (xf1 - 0.05, -0.92, ZC + 0.001), (xr + 0.06, -0.92, ZC + 0.001), Z, 0.12)
        rivets(v, (xf1 - 0.05, 0.92, ZC + 0.001), (xr + 0.06, 0.92, ZC + 0.001), Z, 0.12)
        rivets(v, (xr + 0.002, -0.90, ZC - 0.06), (xr + 0.002, 0.90, ZC - 0.06), -X, 0.11)
    # spare links laid across the nose plate, and a tarpaulin rolled along the back of the box
    v.spare_links(g_at(0.12, -0.80), g_at(0.12, -0.80) + Y * 1.6, 9, 0.09, 0.26, gn, thick=0.022, horn=True,
                  style={"kind": "plate", "horn": "centre", "horn_high": 0.055, "grouser": 0.011, "cleats": False, "bar": 0.3})
    v.use("hull")

    # ---- the gun: a fixed cast collar on the front plate, the mounting turning in it, the barrel in its sleeve ------
    gy, gz = -0.12, 1.40
    gu = (gz - zg) / fl.z
    gp = f_at(gu, gy)                                       # where the bore meets the front plate
    v.lathe(gp - fn * 0.06, fn, [(0.16, 0.0), (0.16, 0.25), (0.11, 0.29), (0.0, 0.31), (0.0, 0.0)], coat, v.n(32, 12))
    if fine:
        v.bolts(gp + fn * 0.10, fn, 0.255, 10, r=0.013)
    v.part("turret", "hull", (gp.x + 0.12, gy, gz), axis=(0, 0, -1), limits=[-18.0, 20.0], kind="turn",
           note="the gun's own traverse in its mounting, 18 degrees left and 20 right: + to the right; for more the game turns the whole of her")
    v.part("gun", "turret", (gp.x + 0.12, gy, gz), axis=(0, -1, 0), limits=[-12.0, 22.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.ball((gp.x + 0.16, gy, gz), 0.20, coat, v.n(28, 10), v.n(14, 6))
    v.box((0.14, 0.44, 0.36), (gp.x + 0.30, gy, gz), coat, 0.04, segs=3)                # the shield on the cradle's face
    v.lathe((gp.x + 0.36, gy, gz), X, [(0.62, 0.0), (0.62, 0.082), (0.60, 0.092), (0.04, 0.098), (0.0, 0.106), (0.0, 0.0)], coat, v.n(28, 10))   # the sleeve
    if fine:
        for y in (-0.17, 0.17):
            for z in (-0.13, 0.13):
                v.bolt((gp.x + 0.371, gy + y, gz + z), X, 0.013)
    bx = gp.x + 0.40
    v.part("barrel", "gun", (bx, gy, gz), slide=(-1, 0, 0), slide_limits=[0.0, 0.32], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((bx, gy, gz), 1.02, 0.075, r0=0.066, r1=0.058, brake=None, sleeve=None)
    v.lathe((bx + 0.96, gy, gz), X, [(0.06, 0.0), (0.06, 0.064), (0.0, 0.066), (0.0, 0.0)], coat, v.n(24, 8))        # the swell at the muzzle
    v.socket("muzzle", (bx + 1.03, gy, gz), X)
    v.m.info["bore"] = 0.075
    v.m.info["brake"] = False

    # ---- the roof: one wide lid over the fighting box, the driver's periscope, a machine gun on a post for aircraft --
    v.use("hull")
    v.hatch_rect("hatch_commander", "hull", (-0.20, 0.0, ZC), (0.64, 0.92), Z, X, hinge="back", opens=105.0, thick=0.024)
    v.socket("commander", (-0.20, 0.0, ZC - 0.25), X, part="hull", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.periscope((0.96, 0.50, ZC), X, (0.07, 0.14, 0.07))
    v.periscope((0.70, -0.40, ZC), X, (0.06, 0.12, 0.08))                # the gunner's sight
    v.rod((-0.46, 0.66, ZC), (-0.46, 0.66, ZC + 0.36), 0.022, "steel", 10)
    v.box((0.10, 0.08, 0.06), (-0.44, 0.66, ZC + 0.39), "steel", 0.008)
    v.mg((-0.40, 0.66, ZC + 0.40), Vector((1, 0.0, 0.18)).normalized(), 0.62)
    v.box((0.20, 0.06, 0.10), (-0.56, 0.66, ZC + 0.40), "steel", 0.01)                # its stock and grips
    v.socket("mg", (0.22, 0.66, ZC + 0.51), Vector((1, 0.0, 0.18)).normalized(), part="hull", note="the machine gun on its post, for aircraft (pictures only: the game fires her main gun)")
    if fine:
        for at in ((0.70, 0.62), (0.70, -0.85), (-0.52, -0.62)):
            v.lift_eye((at[0], at[1], ZC), Z, Y, 0.03)
        v.vent((0.30, 0.20, ZC), 0.08, 0.04)

    d_at, dk, dn = _back(v, fine, aerial=(-0.48, -0.88, ZC))
    v.roll((-0.70, -0.70, 1.56), (-0.70, 0.50, 1.56), r=0.09, paint=v.own_paint("canvas.drab", "5C5B44"))
    v.jerrycan((-1.60, -0.80, zg), 90.0)
    v.jerrycan((-1.88, -0.80, zg), 90.0)
    _running(v, fine)

    v.use("hull")
    v.socket("engine", d_at(0.6, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.52, 0.36) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (-0.20, 0.0, ZC + 0.1), Z, part="hull", note="out of the roof's lid")
    v.socket("hit.front", f_at(0.30, 0.40), fn, part="hull")
    v.socket("hit.left", (0.2, sw, 1.4), Y, part="hull")
    v.socket("hit.right", (0.2, -sw, 1.4), -Y, part="hull")
    v.socket("hit.rear", (-2.4, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", (0.0, sw - 0.02, 1.62), Y, part="hull", note="the fighting box's side")
    v.socket("case", (-0.20, -0.30, ZC + 0.1), Y, part="hull", note="where the empty case is thrown out")
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        v.mark(tag, (-0.32, s * (sw - 0.022), 1.50), n, 0.10, part="hull")
    v.tank_clips(recoil=0.28, kick=0.7, gun_low=-12.0, hatches=["hatch_commander"], traverse=8.0,
                 idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_02", has_turret=False, mg=None)
    v.name_place((0.75, 1.42), long=0.55, high=0.09, sides=(LEFT,))
    return v


def l3(fine=True, tag="ITA"):
    """L3/35: the tankette of 1935, the most numerous Italian armoured vehicle of 1940. Two men side by side in a
    riveted box 3.2 m long, 1.4 m wide and 1.28 m high: the driver on the left behind his visor, the gunner on the
    right behind two machine guns in one mounting. Six small wheels a side in three sprung pairs, the drive at the
    front, a big idler at the back. Our own mesh from numbers."""
    v = Works("L3", "Carro Veloce L3/35", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.48))
    hw, sw, zg, zr = 0.40, 0.56, 0.70, 1.26     # the tub between the tracks; the box over them; the guards; the roof
    # ---- the tub, a nose plate up to the box, the box, the engine deck behind ------------------------------------
    v.hull([(x, hw, z) for x, z in [(-1.40, 0.42), (-1.30, 0.30), (1.08, 0.30), (1.46, 0.52), (1.46, zg - 0.02), (-1.48, zg - 0.02), (-1.48, 0.52)]], coat, bevel=0.01)
    v.hull([(1.46, sw, zg), (0.92, sw, 0.98), (0.92, sw, zg)], coat, bevel=0.01)
    gl = Vector((0.92 - 1.46, 0, 0.98 - zg)).normalized()
    gn = Vector((gl.z, 0, -gl.x))
    g_at = lambda u, y: Vector((1.46, y, zg)) + gl * u + gn * 0.002     # noqa: E731
    v.hull([(0.94, sw, zg), (0.94, sw, 0.975), (0.80, sw - 0.02, zr), (-0.42, sw - 0.02, zr), (-0.42, sw, zg)], coat, bevel=0.012)
    fl = Vector((0.80 - 0.94, 0, zr - 0.975)).normalized()
    fn = Vector((fl.z, 0, -fl.x))
    f_at = lambda u, y: Vector((0.94, y, 0.975)) + fl * u + fn * 0.002   # noqa: E731
    v.hull([(-0.42, sw - 0.02, zg), (-0.42, sw - 0.04, 1.14), (-1.40, sw - 0.06, 0.98), (-1.50, sw - 0.04, zg)], coat, bevel=0.01)
    dk = Vector((-1.40 + 0.42, 0, 0.98 - 1.14)).normalized()
    dn = Vector((dk.z, 0, -dk.x))
    d_at = lambda u, y: Vector((-0.42, y, 1.14)) + dk * u + dn * 0.002  # noqa: E731
    for s in (LEFT, RIGHT):                      # the guards over the tracks
        y0, y1 = s * hw, s * (sw + 0.14)
        v.slab((-1.50, y0, zg - 0.014), (1.40, y1, zg), coat, 0.004)
        v.plate([(1.40, y0, zg), (1.40, y1, zg), (1.56, y1, 0.60), (1.56, y0, 0.60)][::int(s)], 0.008, "upper", 0.003)
        v.plate([(-1.50, y1, zg), (-1.50, y0, zg), (-1.60, y0, 0.58), (-1.60, y1, 0.58)][::int(s)], 0.008, "upper", 0.003)
        v.hook((1.50, s * 0.26, 0.42), Vector((0.8, 0, -0.5)).normalized(), 0.06)
        v.hook((-1.48, s * 0.26, 0.46), -X, 0.06)
        v.headlamp((1.30, s * 0.58, zg), X, 0.055, stalk=0.08)
    # ---- the front plate: the driver's visor on the left, the twin guns in their mounting on the right -----------
    v.visor(f_at(0.15, 0.24) + fn * 0.004, fn, 0.30, 0.10, 0.045)
    c = f_at(0.15, -0.20)
    v.lathe(c - fn * 0.015, fn, [(0.0, 0.0), (0.0, 0.17), (0.04, 0.17), (0.075, 0.14), (0.10, 0.0)][::-1], "upper", v.n(24, 10))
    v.part("turret", "hull", c, axis=(0, 0, -1), limits=[-12.0, 12.0], kind="turn", note="the twin guns' mounting turns 12 degrees each way: + to the right; for more the game turns the whole of her")
    v.part("gun", "turret", c, axis=(0, -1, 0), limits=[-12.0, 15.0], kind="turn", note="the twin guns lift in their mounting: + up; the game lays them")
    v.box((0.10, 0.22, 0.12), c + fn * 0.10, "upper", 0.015, segs=2, turn=turn_to(fn, Z))
    for dy in (-0.055, 0.055):
        v.mg(c + fn * 0.14 + Y * dy, fn, 0.42)
    v.socket("muzzle", c + fn * 0.56, fn, note="the twin machine guns: fire is a burst")
    v.part("barrel", "gun", c + fn * 0.14, slide=(-fn.x, 0, -fn.z), slide_limits=[0.0, 0.02], kind="slide", note="the guns' small kick (metres)")
    v.use("hull")
    v.m.info["bore"] = 0.008
    v.m.info["brake"] = False
    # ---- the roof: a lid over each man, a port in each side, rivets on every seam --------------------------------
    v.hatch_rect("hatch_commander", "hull", (0.30, -0.22, zr), (0.46, 0.38), Z, X, hinge="back", opens=110.0, thick=0.018)
    v.socket("commander", (0.30, -0.22, zr - 0.25), X, part="hull", note="the gunner, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_driver", "hull", (0.30, 0.24, zr), (0.46, 0.38), Z, X, hinge="back", opens=110.0, thick=0.018)
    v.use("hull")
    sn = [Vector((0, s, 0.04)).normalized() for s in (1.0, -1.0)]
    if fine:
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            for z in (zg + 0.05, zr - 0.05):
                rivets(v, (0.88, s * (sw - 0.006), z), (-0.38, s * (sw - 0.006), z), n, 0.10)
            for x in (0.86, 0.24, -0.38):
                rivets(v, (x, s * (sw - 0.006), zg + 0.14), (x, s * (sw - 0.016), zr - 0.14), n, 0.10)
            v.port(Vector((0.55, s * (sw - 0.012), 1.04)), n, 0.035)
            rivets(v, g_at(0.04, s * 0.50), g_at(0.56, s * 0.50), gn, 0.10)
            rivets(v, d_at(0.05, s * 0.44), d_at(0.95, s * 0.42), dn, 0.11)
        for u in (0.05, 0.25):
            rivets(v, f_at(u, -0.50), f_at(u, 0.50), fn, 0.10)
        rivets(v, g_at(0.04, -0.46), g_at(0.04, 0.46), gn, 0.10)
    # ---- the engine deck: a louvred lid each side, the silencer across the tail ---------------------------------
    for s in (LEFT, RIGHT):
        v.box((0.48, 0.34, 0.016), d_at(0.42, s * 0.22) + dn * 0.008, "upper", 0.005, turn=turn_to(dk, dn))
        v.louvres(d_at(0.42, s * 0.22) + dn * 0.016, (0.28, 0.38), dn, count=6, down=dk)
    v.silencer((-1.56, -0.36, 0.68), (-1.56, 0.30, 0.68), 0.075, pipe=[(-1.44, 0.10, 0.86), (-1.50, 0.10, 0.80), (-1.56, 0.10, 0.74)], tail=((-1.56, -0.36, 0.68), (-0.3, -1, -0.2)))
    v.socket("exhaust.1", (-1.60, -0.44, 0.66), (-0.3, -1, -0.2))
    v.shovel((-0.05, sw + 0.02, 0.92), 0.0, 0.75, roll=90.0)
    v.aerial("aerial", "hull", (-0.36, -0.46, zr - 0.02), 1.4)
    # ---- the running gear: three pairs of small wheels a side, a big idler behind ---------------------------------
    ty, r_w, thick = 0.53, 0.105, 0.018
    wz = r_w + thick
    xs = [-0.92, -0.66, -0.26, 0.00, 0.40, 0.66]
    v.running(xs, r_w, ty, (1.30, 0.44, 0.20), (-1.30, 0.30, 0.22), rollers=[(-0.55, 0.60, 0.06), (0.55, 0.60, 0.06)], wheel_z=wz,
              wheel=dict(w=0.045, tyre=0.02, gap=0.04, bolts=0, sides=24, hub=0.4, axle=0.12),
              sprocket_kw=dict(teeth=12, wide=0.15, tooth=0.024, holes=0, bolts=6, sides=26, hub=0.42),
              idler_kw=dict(wide=0.14, kind="spoke", spokes=5),
              roller_kw=dict(wide=0.09, tyre=0.01, reach=0.16),
              track=dict(pitch=0.07, wide=0.17, thick=thick, sag=0.01, style={"kind": "plate", "horn": "centre", "horn_high": 0.04, "grouser": 0.009, "cleats": False, "bar": 0.3}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):                      # each pair under a leaf spring, the springs hung from a long girder
        for k in (0, 2, 4):
            leaf_bogie(v, xs[k], xs[k + 1], s * (ty + 0.085), s * hw, r_w, s, top=0.40, fine=fine)
        v.slab((-1.05, s * (ty + 0.07), 0.44), (0.80, s * (ty + 0.10), 0.50), "shade", 0.008)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((1.30, s * (hw - 0.01), 0.44), Y * s, [(0.12, 0.0), (0.12, 0.10), (0.06, 0.15), (0.0, 0.16), (0.0, 0.0)], "upper", v.n(18, 8))
    # ---- what happens to her ----------------------------------------------------------------------------------
    v.socket("engine", d_at(0.5, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.42, 0.22) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (0.30, -0.22, zr + 0.1), Z, part="hull", note="out of the gunner's lid")
    v.socket("ring", (0.20, 0.0, zr - 0.3), Z, part="hull", note="where the blast comes from when she brews up")
    v.socket("hit.front", f_at(0.15, 0.0), fn, part="hull")
    v.socket("hit.left", (0.2, sw, 1.0), Y, part="hull")
    v.socket("hit.right", (0.2, -sw, 1.0), -Y, part="hull")
    v.socket("hit.rear", (-1.5, 0.0, 0.7), -X, part="hull")
    v.socket("hit.turret", (0.4, -sw, 1.1), -Y, part="hull", note="the box's side")
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        v.mark(tag, (-0.06, s * (sw - 0.012), 1.02), n, 0.09, part="hull")
    v.tank_clips(recoil=0.02, kick=0.15, gun_low=-12.0, hatches=["hatch_commander", "hatch_driver"], traverse=12.0,
                 idle_cue="tank_idle_loop", drive_cue="light_tank_loop", gun_cue="tank_mg_burst", has_turret=False, mg=None, blow=(-0.3, 1.2, 0.2, 120.0))
    return v
