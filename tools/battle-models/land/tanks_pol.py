"""Polish tanks.

The 7TP: the light tank of September 1939. What tells her from across a field: a wide, low riveted hull between two
narrow tracks; eight small wheels a side in two leaf-sprung bogies, four rollers over them; a tall turret set to
the left with a boxed-out back for the wireless, a slim 37 mm gun and beside it the fat armoured sleeve of a
machine gun; the silencer lying across her tail. Every shape is our own mesh, made from numbers here.
"""

import math

from mathutils import Vector

from motorpool import LEFT, RIGHT, X, Y, Z, inset_plan, sym_plan
from tanks_any import leaf_bogie, riveted_prism, rivets, turn_to
from tanks_ger import coat
from works import Works


def tp7(fine=True, tag="POL"):
    """7TP: 4.6 m long, 2.4 m wide, 2.27 m high. The drive at the front, the diesel behind under a louvred deck."""
    v = Works("7TP", "7TP light tank", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.62))
    hw, zg, zr = 0.88, 0.96, 1.46                  # the hull's half-width; the guards; the crew box's roof

    # ---- the hull: a wide tub, a sloped nose plate, the crew's box, the engine room behind it -------------------
    v.hull([(x, hw, z) for x, z in [(-2.20, 0.60), (-2.05, 0.42), (1.60, 0.42), (2.12, 0.64), (2.22, 0.82), (2.08, zg), (-2.24, zg), (-2.28, 0.72)]], coat, bevel=0.012)
    v.hull([(2.08, hw, zg - 0.01), (1.20, hw, 1.21), (1.20, hw, zg - 0.01)], coat, bevel=0.012)
    v.hull([(1.22, hw, zg), (1.14, hw - 0.02, zr), (-0.36, hw - 0.02, zr), (-0.36, hw, zg)], coat, bevel=0.012)
    v.hull([(-0.36, hw, zg), (-0.36, hw - 0.02, 1.33), (-2.08, hw - 0.02, 1.29), (-2.30, hw, 1.10), (-2.30, hw, zg)], coat, bevel=0.012)
    gl = Vector((1.20 - 2.08, 0, 1.21 - (zg - 0.01))).normalized()   # up the nose plate
    gn = Vector((gl.z, 0, -gl.x))
    fl = Vector((1.14 - 1.22, 0, zr - zg)).normalized()             # up the front plate
    fn = Vector((fl.z, 0, -fl.x))
    dk = Vector((-2.08 + 0.36, 0, 1.29 - 1.33)).normalized()        # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))
    g_at = lambda u, y: Vector((2.08, y, zg - 0.01)) + gl * u + gn * 0.002     # noqa: E731
    f_at = lambda u, y: Vector((1.22, y, zg)) + fl * u + fn * 0.002           # noqa: E731
    d_at = lambda u, y: Vector((-0.36, y, 1.33)) + dk * u + dn * 0.002        # noqa: E731
    # the driver sits on the right: two lids on the nose plate that open like a book, a flap above them in the front plate
    v.hatch_rect("hatch_driver", "hull", g_at(0.52, -0.44), (0.62, 0.56), gn, gl, hinge="right", opens=120.0, thick=0.02, split=True)
    v.use("hull")
    v.visor(f_at(0.27, -0.44) + fn * 0.004, fn, 0.46, 0.15, 0.04)
    v.box((0.58, 0.50, 0.016), g_at(0.52, 0.40) + gn * 0.008, "upper", 0.005, turn=turn_to(gl, gn))            # the gearbox's lid on the left
    if fine:
        for du in (-0.24, 0.24):
            for dy in (-0.2, 0.0, 0.2):
                v.bolt(g_at(0.52 + du, 0.40 + dy) + gn * 0.016, gn, 0.012)
        rivets(v, g_at(0.05, -0.82), g_at(0.05, 0.82), gn, 0.11)
        rivets(v, f_at(0.05, -0.82), f_at(0.05, 0.82), fn, 0.11)
        rivets(v, f_at(0.45, -0.80), f_at(0.45, 0.80), fn, 0.11)
        for s in (LEFT, RIGHT):
            rivets(v, g_at(0.12, s * 0.83), g_at(0.86, s * 0.83), gn, 0.11)
            for z, xa, xb in ((zg + 0.06, 1.12, -2.2), (zr - 0.05, 1.08, -0.30), (1.27, -0.44, -2.02)):
                rivets(v, (xa, s * (hw - 0.012), z), (xb, s * (hw - 0.012), z), Y * s, 0.115)
            for x in (1.10, 0.36, -0.30):
                rivets(v, (x, s * (hw - 0.008), zg + 0.17), (x, s * (hw - 0.016), zr - 0.16), Y * s, 0.115)
    for s in (LEFT, RIGHT):
        v.hook((2.16, s * 0.50, 0.62), Vector((0.8, 0, -0.5)).normalized(), 0.08)
        v.hook((-2.28, s * 0.50, 0.70), -X, 0.08)
        v.headlamp((1.80, s * 1.02, zg), X, 0.065, stalk=0.1)

    # ---- the track guards: narrow shelves over the tracks ------------------------------------------------------
    for s in (LEFT, RIGHT):
        y0, y1 = s * hw, s * 1.215
        v.slab((-2.26, y0, zg - 0.016), (2.06, y1, zg), coat, 0.004)
        v.plate([(2.06, y0, zg), (2.06, y1, zg), (2.26, y1, 0.82), (2.26, y0, 0.82)][::int(s)], 0.01, "upper", 0.003)
        v.plate([(-2.26, y1, zg), (-2.26, y0, zg), (-2.40, y0, 0.78), (-2.40, y1, 0.78)][::int(s)], 0.01, "upper", 0.003)
        if fine:
            for x in (-1.7, -0.6, 0.5, 1.6):
                v.slab((x - 0.012, s * hw, zg - 0.09), (x + 0.012, s * 1.19, zg - 0.016), "shade")
    v.shovel((0.3, 1.06, zg + 0.022), 0.0, 0.9)
    v.crowbar((0.2, -1.07, zg + 0.018), 0.0, 1.2)
    v.jack((-1.2, 1.05, zg), 0.0)
    v.bin((-1.9, -1.19, zg), (-1.2, -0.92, zg + 0.17), lid_side=-Y)

    # ---- the engine deck: louvred lids, a grille across the tail plate, the silencer lying across the back ------
    for s in (LEFT, RIGHT):
        v.box((0.74, 0.72, 0.02), d_at(0.46, s * 0.42) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
        v.louvres(d_at(0.46, s * 0.42) + dn * 0.02, (0.56, 0.62), dn, count=6, down=dk)
        v.box((0.66, 0.72, 0.02), d_at(1.26, s * 0.42) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
        v.grille(d_at(1.26, s * 0.42) + dn * 0.02, (0.64, 0.56), dn, Y, slats=7, deep=0.02)
        if fine:
            for u in (0.25, 1.1):
                v.hinge(d_at(u - 0.05, s * 0.045) + dn * 0.02, d_at(u + 0.05, s * 0.045) + dn * 0.02, 0.011)
    tl = Vector((-2.30 + 2.08, 0, 1.10 - 1.29)).normalized()        # down the tail plate
    tn = Vector((-tl.z, 0, tl.x))
    t_at = lambda u, y: Vector((-2.08, y, 1.29)) + tl * u + tn * 0.002         # noqa: E731
    v.grille(t_at(0.145, 0.0) + tn * 0.004, (1.30, 0.22), tn, Y, slats=4, deep=0.02)
    v.silencer((-2.44, -0.56, 1.02), (-2.44, 0.46, 1.02), 0.09, bands=3, tail=((-2.44, -0.56, 1.02), (-0.3, -1.0, -0.1)),
               pipe=[(-2.30, 0.30, 1.06), (-2.38, 0.30, 1.05), (-2.44, 0.30, 1.02)])
    for y in (-0.40, 0.30):
        v.slab((-2.44, y - 0.02, 0.90), (-2.30, y + 0.02, 0.94), "shade")
    v.socket("exhaust.1", (-2.48, -0.72, 1.00), (-0.3, -1.0, -0.1))

    # ---- the running gear: two bogies of four small wheels a side, each bogie on its own leaf spring ---------
    ty, r_w, thick = 1.075, 0.16, 0.022
    wz = r_w + thick
    xs = [-1.38, -1.02, -0.60, -0.24, 0.24, 0.60, 1.02, 1.38]
    v.running(xs, r_w, ty, (1.86, 0.62, 0.26), (-1.90, 0.52, 0.23), rollers=[(x, 0.80, 0.085) for x in (-1.2, -0.4, 0.4, 1.2)], wheel_z=wz,
              wheel=dict(w=0.05, tyre=0.022, gap=0.05, bolts=0, sides=26, hub=0.4, axle=0.14),
              sprocket_kw=dict(teeth=14, wide=0.2, tooth=0.028, holes=5, bolts=8, sides=28, hub=0.42),
              idler_kw=dict(wide=0.18, kind="spoke", spokes=6),
              roller_kw=dict(wide=0.12, tyre=0.012, reach=0.22),
              track=dict(pitch=0.09, wide=0.26, thick=thick, sag=0.012, style={"kind": "plate", "horn": "centre", "horn_high": 0.055, "grouser": 0.011, "cleats": False, "bar": 0.3}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):
        yi = s * (ty + 0.085)
        for k in (0, 4):
            ca, cb = (xs[k] + xs[k + 1]) / 2, (xs[k + 2] + xs[k + 3]) / 2
            leaf_bogie(v, ca, cb, s * (ty + 0.105), s * hw, 0.32, s, top=0.56, fine=fine)
            for cc, pair in ((ca, (xs[k], xs[k + 1])), (cb, (xs[k + 2], xs[k + 3]))):
                for xw in pair:
                    v.rod((cc, yi, 0.32), (xw, yi, wz), 0.026, "shade", 8)
                    if fine:
                        v.lathe((xw, yi - s * 0.01, wz), Y * s, [(0.035, 0.0), (0.035, 0.035), (0.0, 0.048), (0.0, 0.0)], "upper", 10)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((1.86, s * (hw - 0.01), 0.62), Y * s, [(0.12, 0.0), (0.12, 0.13), (0.06, 0.19), (0.0, 0.21), (0.0, 0.0)], "upper", v.n(20, 8))
        v.lathe((-1.90, s * (hw - 0.01), 0.52), Y * s, [(0.12, 0.0), (0.12, 0.07), (0.0, 0.1), (0.0, 0.0)], "upper", v.n(14, 6))

    # ---- the turret, set to the left: tall plates leaning in, the wireless boxed out behind --------------------
    x0, y0, zt, th = 0.42, 0.12, zr, 0.70
    v.ring_well((x0, y0, zt), 0.62)
    v.part("turret", "hull", (x0, y0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    plan = sym_plan([(0.56, 0.27), (0.34, 0.60), (-0.40, 0.62), (-0.64, 0.38)])
    top = inset_plan(plan, [0.12, 0.13, 0.12, 0.06, 0.12, 0.13, 0.12, 0.08])
    plan_w = [(x0 + x, y0 + y) for x, y in plan]
    top_w = [(x0 + x, y0 + y) for x, y in top]
    v.prism(plan_w, zt + 0.02, zt + th, coat, top=top_w, bevel=0.012, segs=2, angle=14.0)
    v.lathe((x0, y0, zt - 0.03), Z, [(0.06, 0.62), (0.0, 0.64), (0.0, 0.0)], "shade", v.n(32, 10))
    roof = zt + th
    riveted_prism(v, plan_w, top_w, zt + 0.02, roof, part="turret", every=0.11)
    v.slab((x0 - 1.02, y0 - 0.34, zt + 0.22), (x0 - 0.56, y0 + 0.34, zt + 0.62), "upper", 0.012, segs=2)       # the wireless's box
    if fine:
        for z in (zt + 0.27, zt + 0.57):
            rivets(v, (x0 - 1.021, y0 - 0.29, z), (x0 - 1.021, y0 + 0.29, z), -X, 0.1)
        for s in (LEFT, RIGHT):
            rivets(v, (x0 - 0.98, y0 + s * 0.341, zt + 0.27), (x0 - 0.66, y0 + s * 0.341, zt + 0.27), Y * s, 0.1)
            rivets(v, (x0 - 0.98, y0 + s * 0.341, zt + 0.57), (x0 - 0.66, y0 + s * 0.341, zt + 0.57), Y * s, 0.1)
    v.hatch_rect("hatch_commander", "turret", (x0 - 0.20, y0, roof), (0.46, 0.56), Z, X, hinge="back", opens=100.0, thick=0.022, split=True)
    v.socket("commander", (x0 - 0.20, y0, roof - 0.25), X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.lathe((x0 + 0.20, y0 + 0.20, roof - 0.005), Z, [(0.09, 0.0), (0.09, 0.05), (0.07, 0.065), (0.0, 0.07), (0.0, 0.0)], "upper", v.n(16, 6))     # his all-round periscope
    v.vent((x0 + 0.18, y0 - 0.20, roof - 0.005), 0.07, 0.03)
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 - 0.03, y0 + s * 0.555, zt + 0.36)), Vector((0, s, 0.19)), 0.04)
    v.aerial("aerial", "turret", (x0 - 0.92, y0 + 0.26, zt + 0.62), 1.9)

    # ---- the gun: a slim 37 mm in a boxed shield, the machine gun's armoured sleeve on its right ---------------
    tx, tz, gy = x0 + 0.42, zt + 0.36, y0 + 0.06
    v.part("gun", "turret", (tx, gy, tz), axis=(0, -1, 0), limits=[-10.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.box((0.16, 0.54, 0.38), (tx + 0.10, y0, tz), "upper", 0.03, segs=3)
    if fine:
        for y in (-0.22, 0.22):
            for z in (-0.14, 0.14):
                v.bolt((tx + 0.181, y0 + y, tz + z), X, 0.012)
    v.rod((tx + 0.17, y0 - 0.18, tz), (tx + 0.78, y0 - 0.18, tz), 0.058, "upper", v.n(16, 8))                   # the machine gun's sleeve
    v.rod((tx + 0.78, y0 - 0.18, tz), (tx + 0.84, y0 - 0.18, tz), 0.018, "steel", 8)
    v.socket("coax", (tx + 0.86, y0 - 0.18, tz), X)
    v.part("barrel", "gun", (tx + 0.18, gy, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.2], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.18, gy, tz), 1.42, 0.037, r0=0.046, r1=0.03, brake=None, sleeve=(0.46, 0.062))
    v.socket("muzzle", (tx + 1.62, gy, tz), X)
    v.m.info["bore"] = 0.037
    v.m.info["brake"] = False

    # ---- sockets for what happens to her ------------------------------------------------------------------------
    v.socket("engine", d_at(0.8, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.46, 0.42) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (x0 - 0.20, y0, roof + 0.1), Z, part="turret", note="out of the turret's lid")
    v.socket("fire.3", g_at(0.52, -0.44) + gn * 0.05, gn, part="hull", note="out of the driver's lids")
    v.socket("ring", (x0, y0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", f_at(0.25, 0.0), fn, part="hull")
    v.socket("hit.left", (0.3, hw, 1.2), Y, part="hull")
    v.socket("hit.right", (0.3, -hw, 1.2), -Y, part="hull")
    v.socket("hit.rear", (-2.3, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", (x0, y0 + 0.6, zt + 0.35), Y, part="turret")
    v.socket("case", (x0 - 0.3, y0 - 0.6, zt + 0.35), -Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.16, kick=0.5, gun_low=-10.0, hatches=["hatch_commander", "hatch_driver"],
                 traverse=16.0, idle_cue="tank_idle_loop", drive_cue="light_tank_loop", gun_cue="tank_gun_01", blow=(-0.7, 2.1, 0.32, 165.0))
    return v
