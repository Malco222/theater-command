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


def m13(fine=True, tag="ITA"):
    """M13/40: 4.9 m long, 2.2 m wide, 2.35 m high. The drive at the front, the engine behind under a deck with a
    silencer on each rear guard."""
    v = Works("M13", "Carro Armato M13/40", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.68))
    hw, sw, zg, zr = 0.66, 1.02, 1.06, 1.76          # half-width between the tracks and of the crew's box; the guards; the roof

    # ---- the tub, the round nose over the drive, the sloped plate above it -------------------------------------
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

    # ---- the crew's box, standing over the tracks: the driver's visor on the left, two machine guns on the right --
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

    # ---- the track guards, the engine deck, a silencer on each rear guard -------------------------------------
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
    v.aerial("aerial", "hull", (-0.46, -0.90, zr), 1.8)

    # ---- the running gear: two bogies of four wheels a side, each on its own leaf spring ----------------------
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
    v.socket("commander", (x0 - 0.16, 0.0, roof - 0.25), X, part="turret", note="the commander stands here, head and shoulders out when the lid is open")
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
    return v
