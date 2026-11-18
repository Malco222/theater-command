"""Tanks no one nation owns: the light tank and the medium tank that every army without a type of its own shows
(and that the great powers show before their own types arrive).

Both are our own designs, drawn in the manner of their years and of no one maker: the light tank a riveted
three-man machine of the middle 1930s with a 37 mm gun, the medium tank a welded one of about 1940 with a 50 mm
gun. Every shape is our own mesh, made from numbers here.
"""

import math

from mathutils import Matrix, Vector

from motorpool import LEFT, RIGHT, X, Y, Z, inset_plan, sym_plan
from tanks_ger import coat
from works import Works


def turn_to(along, up):
    """The turn that lays a box's length along `along` and its top toward `up`."""
    a, u = Vector(along).normalized(), Vector(up).normalized()
    return Matrix((a, u.cross(a), u)).transposed()


def rivets(v, a, b, facing, every=0.115, r=0.0105, part=None):
    """A row of rivet heads from a to b on a plate that faces `facing`."""
    if not v.fine:
        return
    a, b = Vector(a), Vector(b)
    n = max(1, int(round((b - a).length / every)))
    for i in range(n + 1):
        v.bolt(a.lerp(b, i / n), facing, r, 0.007, part=part)


def riveted_prism(v, plan, top, z0, z1, part=None, rows=(0.12, 0.88), every=0.115):
    """Rivet rows along the foot and the head of every face of a plate turret built on `plan` (its roof on `top`)."""
    if not v.fine:
        return
    n = len(plan)
    mid = Vector((sum(p[0] for p in plan) / n, sum(p[1] for p in plan) / n, (z0 + z1) / 2))
    for k in range(n):
        a, b = Vector((plan[k][0], plan[k][1], z0)), Vector((plan[(k + 1) % n][0], plan[(k + 1) % n][1], z0))
        ta, tb = Vector((top[k][0], top[k][1], z1)), Vector((top[(k + 1) % n][0], top[(k + 1) % n][1], z1))
        nrm = (b - a).cross(ta - a).normalized()
        if nrm.dot((a + b) / 2 - mid) < 0:
            nrm = -nrm
        for f in rows:
            p, q = a.lerp(ta, f), b.lerp(tb, f)
            d = (q - p).normalized() * 0.05
            rivets(v, p + d + nrm * 0.001, q - d + nrm * 0.001, nrm, every, part=part)


def leaf_bogie(v, xa, xb, y_out, y_hull, r_w, s, top=0.62, fine=True):
    """Two road wheels hung from one leaf spring: its middle clamped to a bracket on the hull's side, an arm from
    each end down to a wheel's axle. On the root, so it stays with the wheels while the hull rocks."""
    xm = (xa + xb) / 2
    yi = y_out - s * 0.02
    v.slab((xm - 0.09, min(y_hull, yi), top - 0.16), (xm + 0.09, max(y_hull, yi), top + 0.02), "shade", 0.01)
    if not fine:
        return
    half0 = (xb - xa) / 2 + 0.03
    for j in range(5):                                   # the leaves, the longest on top
        half = half0 - j * 0.055
        v.slab((xm - half, yi - 0.035, top - 0.014 - j * 0.016), (xm + half, yi + 0.035, top - j * 0.016), "shade")
    for xw in (xa, xb):                                  # the arms, and the cap on each axle
        v.rod((xw, yi, top - 0.01), (xw, yi, r_w), 0.028, "shade", 8)
        v.lathe((xw, yi - s * 0.01, r_w), Y * s, [(0.04, 0.0), (0.04, 0.04), (0.0, 0.055), (0.0, 0.0)], "upper", v.n(12, 6))
    v.bolts((xm, yi + s * 0.036, top - 0.07), Y * s, 0.045, 4, r=0.011)


def light_tank(fine=True, tag="_"):
    """The light tank: 4.5 m long, 2.25 m wide, 2.15 m high. A riveted hull, the drive at the front, four wheels a
    side in two leaf-sprung pairs, an eight-sided plate turret with a 37 mm gun and a machine gun beside it."""
    v = Works("LightTank", "Light tank", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.62))
    hw = 0.64
    # ---- the tub between the tracks, the sloped nose plate over the gearbox, the crew's box, the engine's --------
    v.hull([(x, hw, z) for x, z in [(-2.16, 0.52), (-2.02, 0.36), (1.50, 0.36), (2.08, 0.60), (2.18, 0.80), (2.02, 0.97), (-2.20, 0.97), (-2.20, 0.60)]], coat, bevel=0.012)
    v.hull([(2.02, hw, 0.97), (1.22, hw, 1.24), (1.22, hw, 0.97)], coat, bevel=0.012)
    v.hull([(1.24, 0.99, 0.975), (1.12, 0.95, 1.50), (-0.50, 0.95, 1.50), (-0.50, 0.99, 0.975)], coat, bevel=0.012)
    v.hull([(-0.50, 0.93, 0.975), (-0.50, 0.90, 1.36), (-2.12, 0.86, 1.18), (-2.20, 0.90, 0.975)], coat, bevel=0.012)
    gl = Vector((1.22 - 2.02, 0, 1.24 - 0.97)).normalized()          # up the nose plate
    gn = Vector((gl.z, 0, -gl.x))
    fl = Vector((1.12 - 1.24, 0, 1.50 - 0.975)).normalized()         # up the driver's plate
    fn = Vector((fl.z, 0, -fl.x))
    dk = Vector((-2.12 + 0.50, 0, 1.18 - 1.36)).normalized()         # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))
    g_at = lambda u, y: Vector((2.02, y, 0.97)) + gl * u + gn * 0.002      # noqa: E731
    f_at = lambda u, y: Vector((1.24, y, 0.975)) + fl * u + fn * 0.002     # noqa: E731
    d_at = lambda u, y: Vector((-0.50, y, 1.36)) + dk * u + dn * 0.002     # noqa: E731

    def side_y(z):                                       # the crew's box leans in a little
        return 0.99 - (z - 0.975) * 0.04 / 0.525
    # the track guards: one long plate a side, turned down at each end
    for s in (LEFT, RIGHT):
        y0, y1 = s * 0.64, s * 1.12
        v.slab((-2.25, y0, 0.955), (2.10, y1, 0.972), coat, 0.004)
        v.plate([(2.10, y0, 0.972), (2.10, y1, 0.972), (2.30, y1, 0.84), (2.30, y0, 0.84)][::int(s)], 0.01, "upper", 0.003)
        v.plate([(-2.25, y1, 0.972), (-2.25, y0, 0.972), (-2.40, y0, 0.80), (-2.40, y1, 0.80)][::int(s)], 0.01, "upper", 0.003)
        if fine:
            for x in (-1.7, -0.6, 0.5, 1.6):
                v.slab((x - 0.012, s * 0.64, 0.87), (x + 0.012, s * 1.09, 0.955), "shade")
            v.slab((-2.25, s * 1.105, 0.935), (2.10, s * 1.12, 0.972), "upper")
    # the nose: the driver's lid on the right, a bolted plate over the gearbox on the left, hooks, lamps
    v.hatch_rect("hatch_driver", "hull", g_at(0.46, -0.31), (0.50, 0.48), gn, gl, hinge="back", opens=115.0, thick=0.022)
    v.use("hull")
    c = g_at(0.46, 0.31)
    v.box((0.50, 0.48, 0.02), c + gn * 0.01, "upper", 0.006, turn=turn_to(gl, gn))
    if fine:
        for du in (-0.2, 0.2):
            for dy in (-0.19, 0.0, 0.19):
                v.bolt(c + gl * du + Y * dy + gn * 0.02, gn, 0.012)
        v.vent(c + gn * 0.02, 0.06, 0.03)
    for s in (LEFT, RIGHT):
        v.hook((2.14, s * 0.42, 0.66), Vector((0.8, 0, -0.4)).normalized(), 0.08)
        v.hook((-2.20, s * 0.42, 0.64), -X, 0.08)
        v.headlamp((1.86, s * 0.90, 0.972), X, 0.065, stalk=0.1)
    # the driver's plate: his visor on the right, a port on the left
    v.visor(f_at(0.31, -0.40) + fn * 0.004, fn, 0.36, 0.12, 0.05)
    v.port(f_at(0.31, 0.44), fn, 0.05)
    # rivets, in rows along every seam
    for s in (LEFT, RIGHT):
        sn = Vector((0, s * 0.525, 0.04)).normalized()
        for z in (1.03, 1.45):
            rivets(v, (1.10, s * side_y(z) + s * 0.001, z), (-0.44, s * side_y(z) + s * 0.001, z), sn)
        for x in (1.11, 0.34, -0.44):
            rivets(v, (x, s * side_y(1.13) + s * 0.001, 1.13), (x, s * side_y(1.36) + s * 0.001, 1.36), sn)
        rivets(v, (-0.58, s * 0.905, 1.31), (-2.06, s * 0.868, 1.15), Vector((0, s * 0.385, 0.03)).normalized(), 0.13)
        rivets(v, (-0.58, s * 0.93, 1.02), (-2.10, s * 0.90, 1.02), Vector((0, s * 0.385, 0.03)).normalized(), 0.13)
        rivets(v, g_at(0.06, s * 0.60), g_at(0.78, s * 0.60), gn)
    rivets(v, f_at(0.05, -0.90), f_at(0.05, 0.90), fn)
    rivets(v, f_at(0.49, -0.88), f_at(0.49, 0.88), fn)
    rivets(v, g_at(0.05, -0.56), g_at(0.05, 0.56), gn)
    # the engine deck: a louvred grille each side, two doors behind them
    for s in (LEFT, RIGHT):
        v.box((0.66, 0.56, 0.02), d_at(0.45, s * 0.40) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
        v.grille(d_at(0.45, s * 0.40) + dn * 0.02, (0.44, 0.52), dn, Y, slats=8, deep=0.02)
        v.box((0.56, 0.60, 0.02), d_at(1.22, s * 0.37) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
        if fine:
            v.handle(d_at(1.36, s * 0.37) + dn * 0.02 - Y * 0.08, d_at(1.36, s * 0.37) + dn * 0.02 + Y * 0.08, dn, 0.007, 0.035, "steel")
            for dy in (-0.2, 0.2):
                v.hinge(d_at(0.94, s * 0.37 + dy) + dn * 0.02 - Y * 0.05, d_at(0.94, s * 0.37 + dy) + dn * 0.02 + Y * 0.05, 0.012)
    rivets(v, d_at(0.08, -0.84), d_at(0.08, 0.84), dn, 0.13)
    rivets(v, d_at(1.58, -0.80), d_at(1.58, 0.80), dn, 0.13)
    # the tail: the silencer across it
    v.silencer((-2.34, -0.50, 0.86), (-2.34, 0.36, 0.86), 0.11, pipe=[(-2.20, 0.16, 0.94), (-2.28, 0.16, 0.93), (-2.34, 0.16, 0.9)], tail=((-2.34, -0.50, 0.86), (-0.3, -1, -0.2)))
    v.socket("exhaust.1", (-2.38, -0.64, 0.84), (-0.3, -1, -0.2))
    # tools and stowage on the guards
    v.shovel((0.1, 1.04, 0.995), 0.0, 0.9)
    v.crowbar((-0.2, -1.05, 0.99), 0.0, 1.2)
    v.jack((-1.55, -0.88, 0.972), 0.0)
    v.bin((-2.0, 0.70, 0.972), (-1.25, 1.08, 1.17), lid_side=Y)
    if fine:
        v.cable([(1.5, -0.78, 0.99), (0.6, -0.86, 0.985), (-0.6, -0.86, 0.985), (-0.9, -0.80, 0.985)], 0.012)
    v.aerial("aerial", "hull", (-0.40, 0.86, 1.50), 1.8)

    # ---- the running gear -----------------------------------------------------------------------------
    ty, r_w = 0.90, 0.27
    xs = [-1.22, -0.58, 0.24, 0.88]
    v.running(xs, r_w, ty, (1.82, 0.60, 0.26), (-1.84, 0.50, 0.23), rollers=[(-0.98, 0.82, 0.09), (0.48, 0.82, 0.09)],
              wheel=dict(w=0.06, tyre=0.03, gap=0.05, bolts=6, sides=30, hub=0.34, axle=0.2),
              sprocket_kw=dict(teeth=14, wide=0.2, tooth=0.028, holes=5, bolts=8, sides=28, hub=0.42),
              idler_kw=dict(wide=0.18, kind="spoke", spokes=6),
              roller_kw=dict(wide=0.12, tyre=0.012, reach=0.22),
              track=dict(pitch=0.09, wide=0.26, thick=0.022, sag=0.012, style={"kind": "plate", "horn": "centre", "horn_high": 0.055, "grouser": 0.011, "cleats": False, "bar": 0.3}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):
        for xa, xb in ((xs[0], xs[1]), (xs[2], xs[3])):
            leaf_bogie(v, xa, xb, s * (ty + 0.115), s * hw, r_w, s, top=0.64, fine=fine)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((1.82, s * (hw - 0.01), 0.60), Y * s, [(0.14, 0.0), (0.14, 0.13), (0.07, 0.2), (0.0, 0.22), (0.0, 0.0)], "upper", v.n(20, 8))
        v.lathe((-1.84, s * (hw - 0.01), 0.50), Y * s, [(0.12, 0.0), (0.12, 0.07), (0.0, 0.1), (0.0, 0.0)], "upper", v.n(14, 6))

    # ---- the turret: eight plates riveted on a frame, the commander's little cupola at the back -------------
    x0, zt, th = 0.30, 1.50, 0.50
    v.ring_well((x0, 0.0, zt), 0.56)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    plan = sym_plan([(0.60, 0.27), (0.33, 0.60), (-0.36, 0.62), (-0.68, 0.36)])
    top = inset_plan(plan, [0.07, 0.09, 0.09, 0.09, 0.06, 0.09, 0.09, 0.09])
    plan_w = [(x0 + x, y) for x, y in plan]
    top_w = [(x0 + x, y) for x, y in top]
    v.prism(plan_w, zt + 0.02, zt + th, coat, top=top_w, bevel=0.012, segs=2, angle=14.0)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.60), (0.0, 0.62), (0.0, 0.0)], "shade", v.n(32, 10))
    roof = zt + th
    riveted_prism(v, plan_w, top_w, zt + 0.02, roof, part="turret")
    cup = v.cupola((x0 - 0.26, 0.0, roof), 0.23, 0.15, slits=4, start=math.radians(45))
    v.hatch_round("hatch_commander", "turret", cup, 0.17, Z, -X, thick=0.025, opens=100.0)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.vent((x0 + 0.22, 0.26, roof), 0.07, 0.035)
    v.periscope((x0 + 0.28, -0.26, roof), X, (0.06, 0.11, 0.06))
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 + 0.10, s * 0.563, zt + 0.28)), Vector((0, s, 0.17)), 0.04)
        v.lift_eye((x0 + 0.38, 0.0, roof), Z, Y, 0.028)

    # ---- the gun: a 37 mm in a boxed mantlet, its recoil cylinder under it, a machine gun on its right -----
    tx, tz = x0 + 0.50, zt + 0.26
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 22.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.box((0.12, 0.52, 0.34), (tx + 0.11, 0.0, tz), "upper", 0.02, segs=2)
    v.lathe((tx + 0.10, 0, tz), Y, [(0.20, 0.0), (0.20, 0.10), (0.16, 0.135), (-0.16, 0.135), (-0.20, 0.10), (-0.20, 0.0)], "upper", v.n(24, 8))
    if fine:
        for y in (-0.22, 0.22):
            for z in (-0.13, 0.13):
                v.bolt((tx + 0.171, y, tz + z), X, 0.012)
    v.rod((tx + 0.16, 0.0, tz - 0.085), (tx + 0.62, 0.0, tz - 0.085), 0.036, "upper", 12)
    v.mg((tx + 0.14, -0.17, tz + 0.02), X, 0.40)
    v.socket("coax", (tx + 0.56, -0.17, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.17, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.2], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.17, 0, tz), 1.30, 0.037, r0=0.048, r1=0.03, brake=None, sleeve=None)
    v.socket("muzzle", (tx + 1.49, 0, tz), X)
    v.m.info["bore"] = 0.037
    v.m.info["brake"] = False

    v.socket("engine", d_at(0.6, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.45, 0.4) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", g_at(0.46, -0.31) + gn * 0.05, gn, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", f_at(0.25, 0.0), fn, part="hull")
    v.socket("hit.left", (0.3, 0.98, 1.25), Y, part="hull")
    v.socket("hit.right", (0.3, -0.98, 1.25), -Y, part="hull")
    v.socket("hit.rear", (-2.2, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", (x0, 0.6, zt + 0.25), Y, part="turret")
    v.socket("case", (x0 - 0.3, -0.6, zt + 0.3), -Y, part="turret", note="where the empty case is thrown out")
    for s in (LEFT, RIGHT):                              # her nation's mark on each side of the turret, behind the port (her hull side is the number's)
        v.mark(tag, (x0 - 0.17, s * 0.571, zt + 0.26), Vector((0.03, s, 0.19)), 0.13, part="turret")
    v.tank_clips(recoil=0.16, kick=0.5, gun_low=-8.0, hatches=["hatch_commander", "hatch_driver"],
                 traverse=18.0, idle_cue="tank_idle_loop", drive_cue="light_tank_loop", gun_cue="tank_gun_01", blow=(-0.7, 2.2, 0.32, 165.0))
    v.camouflage(cell=0.62)
    return v


def medium_hull(v, fine, box=True):
    """The medium tank's hull and running gear: a welded tub with a sloped front plate, the drive at the back, five
    wheels a side on swing arms with three return rollers. `box` False leaves out the crew's box with its roof lids,
    the bow gun and the aerial: the assault gun stands a fixed box of her own there. Returns her measures."""
    v.root(hull_pivot=(0.0, 0.0, 0.72))
    hw, sw, zg, zr = 0.84, 1.14, 1.02, 1.52          # half-width between the tracks and of the upper hull; the guards; the roof
    # ---- the tub, the crew's part with its sloped front, the engine's part falling away to the tail ----------
    v.hull([(x, hw, z) for x, z in [(-2.70, 0.56), (-2.52, 0.40), (2.20, 0.40), (2.72, 0.70), (2.60, zg), (-2.78, zg), (-2.78, 0.70)]], coat, bevel=0.014)
    if box:
        v.hull([(2.60, sw, zg), (1.95, sw, zr), (-0.85, sw, zr), (-0.85, sw, zg)], coat, bevel=0.014)
    v.hull([(-0.85, 1.10, zg), (-0.85, 1.07, 1.44), (-2.62, 1.03, 1.30), (-2.80, 1.07, zg)], coat, bevel=0.014)
    gl = Vector((1.95 - 2.60, 0, zr - zg)).normalized()               # up the front plate
    gn = Vector((gl.z, 0, -gl.x))
    dk = Vector((-2.62 + 0.85, 0, 1.30 - 1.44)).normalized()          # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))
    g_at = lambda u, y: Vector((2.60, y, zg)) + gl * u + gn * 0.002       # noqa: E731
    d_at = lambda u, y: Vector((-0.85, y, 1.44)) + dk * u + dn * 0.002    # noqa: E731
    # the track guards: a shelf each side the length of her, turned down at each end, a bin on each beside the engine
    for s in (LEFT, RIGHT):
        y0, y1 = s * hw, s * 1.37
        v.slab((-2.86, y0, zg - 0.016), (2.66, y1, zg), coat, 0.004)
        v.plate([(2.66, y0, zg), (2.66, y1, zg), (2.96, y1, 0.82), (2.96, y0, 0.82)][::int(s)], 0.012, "upper", 0.003)
        v.plate([(-2.86, y1, zg), (-2.86, y0, zg), (-3.04, y0, 0.80), (-3.04, y1, 0.80)][::int(s)], 0.012, "upper", 0.003)
        if fine:
            for x in (-2.2, -1.1, 0.0, 1.1, 2.1):
                v.slab((x - 0.014, s * hw, zg - 0.11), (x + 0.014, s * 1.33, zg - 0.016), "shade")
            v.slab((-2.86, s * 1.355, zg - 0.04), (2.66, s * 1.37, zg), "upper")
        v.bin((-2.30, s * 1.14, zg), (-1.45, s * 1.345, zg + 0.21), lid_side=Y * s)
        v.hook((2.66, s * 0.50, 0.62), Vector((0.8, 0, -0.4)).normalized(), 0.09)
        v.hook((-2.78, s * 0.50, 0.66), -X, 0.09)
        v.headlamp((2.30, s * 1.22, zg), X, 0.07, stalk=0.1)
    # the front plate: the driver's visor on the left, the bow gun in its ball on the right
    v.visor(g_at(0.50, 0.46) + gn * 0.004, Vector((1, 0, 0.15)).normalized(), 0.36, 0.12, 0.10)
    if box:
        v.lathe(g_at(0.46, -0.46) - gn * 0.02, gn, [(0.12, 0.0), (0.12, 0.09), (0.07, 0.15), (0.0, 0.19), (0.0, 0.0)], "upper", v.n(24, 8))
        v.mg_ball(g_at(0.46, -0.46) + gn * 0.10, Vector((0.94, 0, 0.1)).normalized(), 0.08, socket="hull_mg", length=0.38, ring=False)
    if fine:
        v.weld(g_at(0.03, -1.1), g_at(0.03, 1.1), gn)
        v.spare_links(g_at(0.27, -0.26), g_at(0.27, 0.26), 4, 0.13, 0.42, gn, style={"kind": "plate", "grouser": 0.014, "cleats": True})
    # the roof: a lid each for the driver and the bow gunner
    if box:
        v.hatch_rect("hatch_driver", "hull", (1.66, 0.50, zr), (0.42, 0.50), Z, X, hinge="front", opens=105.0)
        v.hatch_rect("hatch_radio", "hull", (1.66, -0.50, zr), (0.42, 0.50), Z, X, hinge="front", opens=105.0)
    v.use("hull")
    # the engine deck: two louvred grilles, two doors behind them; an exhaust standing at each corner of the tail
    for s in (LEFT, RIGHT):
        v.box((0.70, 0.74, 0.022), d_at(0.50, s * 0.50) + dn * 0.011, "upper", 0.006, turn=turn_to(dk, dn))
        v.grille(d_at(0.50, s * 0.50) + dn * 0.022, (0.54, 0.62), dn, X, slats=8, deep=0.022)
        v.box((0.66, 0.74, 0.022), d_at(1.30, s * 0.46) + dn * 0.011, "upper", 0.006, turn=turn_to(dk, dn))
        if fine:
            v.handle(d_at(1.50, s * 0.46) + dn * 0.022 - Y * 0.09, d_at(1.50, s * 0.46) + dn * 0.022 + Y * 0.09, dn, 0.008, 0.04, "steel")
            for dy in (-0.25, 0.25):
                v.hinge(d_at(0.97, s * 0.46 + dy) + dn * 0.022 - Y * 0.05, d_at(0.97, s * 0.46 + dy) + dn * 0.022 + Y * 0.05, 0.013)
        v.silencer((-2.90, s * 0.66, 0.84), (-2.90, s * 0.66, 1.20), 0.075, bands=2, tail=((-2.90, s * 0.66, 1.20), (-0.2, 0, 1)))
        v.tube([(-2.79, s * 0.66, 0.80), (-2.86, s * 0.66, 0.78), (-2.90, s * 0.66, 0.84)], 0.04, "rust", v.n(10, 5))
    v.socket("exhaust.1", (-2.92, 0.66, 1.30), (-0.3, 0, 1))
    v.socket("exhaust.2", (-2.92, -0.66, 1.30), (-0.3, 0, 1))
    # tools on the guards beside the crew's part
    v.shovel((0.2, 1.27, zg + 0.022), 0.0, 0.95)
    v.axe((-0.2, 1.20, zg + 0.02), 180.0, 0.75, roll=90.0)
    v.crowbar((0.0, -1.30, zg + 0.018), 0.0, 1.35)
    v.jack((-0.4, -1.24, zg), 0.0)
    if fine:
        v.cable([(2.0, -1.2, zg + 0.02), (1.2, -1.22, zg + 0.018), (0.6, -1.22, zg + 0.018)], 0.013)
    if box:
        v.aerial("aerial", "hull", (-0.70, -1.0, zr), 2.0)

    # ---- the running gear: five wheels a side on swing arms, the drive at the back --------------------------
    ty, r_w = 1.12, 0.33
    xs = [-1.72, -0.86, 0.0, 0.86, 1.72]
    v.running(xs, r_w, ty, (-2.44, 0.68, 0.29), (2.36, 0.70, 0.27), rollers=[(-1.29, 0.86, 0.1), (0.0, 0.86, 0.1), (1.29, 0.86, 0.1)], drive="rear",
              wheel=dict(w=0.09, tyre=0.035, gap=0.08, bolts=8, sides=32, hub=0.34, axle=0.26),
              sprocket_kw=dict(teeth=15, wide=0.36, tooth=0.03, holes=0, bolts=10, sides=30, hub=0.42),
              idler_kw=dict(wide=0.3, kind="spoke", spokes=8),
              roller_kw=dict(wide=0.14, tyre=0.014, reach=0.24),
              track=dict(pitch=0.13, wide=0.42, thick=0.03, sag=0.014, style={"kind": "plate", "horn": "centre", "horn_high": 0.075, "grouser": 0.014, "cleats": True}))
    v.use("vehicle")
    if fine:
        for s in (LEFT, RIGHT):
            for x in xs:                                 # each wheel's arm, trailing from its pivot on the hull's side
                v.rod((x + 0.30, s * (hw + 0.01), 0.50), (x + 0.30, s * (ty - 0.2), 0.50), 0.05, "shade", 10)
                v.rod((x + 0.30, s * (ty - 0.2), 0.50), (x, s * (ty - 0.2), r_w), 0.04, "shade", 8)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((-2.44, s * (hw - 0.01), 0.68), Y * s, [(0.16, 0.0), (0.16, 0.16), (0.08, 0.24), (0.0, 0.26), (0.0, 0.0)], "upper", v.n(22, 8))
        v.lathe((2.36, s * (hw - 0.01), 0.70), Y * s, [(0.12, 0.0), (0.12, 0.08), (0.0, 0.12), (0.0, 0.0)], "upper", v.n(16, 6))
    return {"hw": hw, "sw": sw, "zg": zg, "zr": zr, "gl": gl, "gn": gn, "g_at": g_at, "d_at": d_at, "dn": dn}


def medium_tank(fine=True, tag="_"):
    """The medium tank: 5.7 m long, 2.75 m wide, 2.4 m high. A welded hull with a sloped front plate, the drive at
    the back, five wheels a side on swing arms with three return rollers, a welded turret with a 50 mm gun, a bin
    on its back and the commander's cupola."""
    v = Works("MediumTank", "Medium tank", "tank", tag, fine)
    h = medium_hull(v, fine)
    sw, zr, gn, g_at, d_at, dn = h["sw"], h["zr"], h["gn"], h["g_at"], h["d_at"], h["dn"]

    # ---- the turret: welded plate, its sides leaning in, a bin on its back -----------------------------------
    x0, zt, th = 0.55, zr, 0.60
    v.ring_well((x0, 0.0, zt), 0.80)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    plan = sym_plan([(0.92, 0.42), (0.52, 0.87), (-0.55, 0.89), (-1.05, 0.52)])
    top = inset_plan(plan, [0.22, 0.20, 0.20, 0.12, 0.20, 0.20, 0.22, 0.16])
    v.prism([(x0 + x, y) for x, y in plan], zt + 0.02, zt + th, coat, top=[(x0 + x, y) for x, y in top], bevel=0.016, segs=2, angle=14.0)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.84), (0.0, 0.86), (0.0, 0.0)], "shade", v.n(36, 12))
    roof = zt + th
    v.bin((x0 - 1.44, -0.44, zt + 0.14), (x0 - 1.04, 0.44, zt + 0.50), part="turret", lid_side=-X)
    cup = v.cupola((x0 - 0.50, 0.0, roof), 0.32, 0.24, slits=5, start=math.radians(36))
    v.hatch_round("hatch_commander", "turret", cup, 0.24, Z, -X, thick=0.03, opens=105.0, split=True)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "turret", (x0 + 0.12, -0.38, roof), (0.44, 0.38), Z, X, hinge="front", opens=100.0, thick=0.026)
    v.use("turret")
    v.vent((x0 + 0.30, 0.32, roof), 0.11, 0.05)
    v.periscope((x0 + 0.46, -0.05, roof), X, (0.07, 0.14, 0.07))
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 - 0.02, s * 0.79, zt + 0.30)), Vector((0, s, 0.33)), 0.045)
            v.lift_eye((x0 + 0.42, s * 0.50, roof), Z, X, 0.032)
        v.lift_eye((x0 - 0.82, 0.0, roof), Z, Y, 0.032)

    # ---- the gun: a 50 mm in a rounded mantlet ---------------------------------------------------------------
    tx, tz = x0 + 0.84, zt + 0.30
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-9.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx + 0.02, 0, tz), Y, [(0.40, 0.0), (0.40, 0.19), (0.34, 0.24), (-0.34, 0.24), (-0.40, 0.19), (-0.40, 0.0)], "upper", v.n(32, 10))
    v.lathe((tx + 0.18, 0, tz), X, [(0.34, 0.0), (0.34, 0.085), (0.24, 0.11), (0.0, 0.14), (0.0, 0.0)], "upper", v.n(24, 8))
    v.mg((tx + 0.16, -0.27, tz), X, 0.16)
    v.socket("coax", (tx + 0.34, -0.27, tz), X)
    v.part("barrel", "gun", (tx + 0.46, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.3], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.46, 0, tz), 1.95, 0.05, r0=0.068, r1=0.043, brake=None, sleeve=None)
    v.socket("muzzle", (tx + 2.43, 0, tz), X)
    v.m.info["bore"] = 0.05
    v.m.info["brake"] = False

    v.socket("engine", d_at(0.6, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.5, 0.5) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.66, 0.50, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(0.4, 0.0), gn, part="hull")
    v.socket("hit.left", (0.3, sw, 1.3), Y, part="hull")
    v.socket("hit.right", (0.3, -sw, 1.3), -Y, part="hull")
    v.socket("hit.rear", (-2.8, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", (x0, 0.82, zt + 0.3), Y, part="turret")
    v.socket("case", (x0 - 0.4, -0.82, zt + 0.3), -Y, part="turret", note="where the empty case is thrown out")
    for s in (LEFT, RIGHT):                              # four marks, as the great powers' tanks carry them: hull sides, turret sides
        v.mark(tag, (0.9, s * (sw + 0.002), 1.28), Y * s, 0.2, part="hull")
        v.mark(tag, (x0 - 0.28, s * 0.787, zt + 0.30), Vector((0.02, s, 0.33)), 0.15, part="turret")
    v.tank_clips(recoil=0.26, kick=0.7, gun_low=-9.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver", "hatch_radio"],
                 traverse=16.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.8, 2.6, 0.4, 168.0))
    v.camouflage()
    return v


def assault_gun(fine=True, tag="_"):
    """The assault gun: the medium tank's hull with a fixed box where her turret stood, its front plate the hull's
    own carried on up, and a 75 mm gun in a cast collar on the right of that plate. For every army with no assault
    gun or tank destroyer of its own. 7.7 m over the gun, 2.75 m wide, 2.1 m high over the commander's cap."""
    v = Works("AssaultGun", "Assault gun", "tank", tag, fine)
    h = medium_hull(v, fine, box=False)
    m = v.m
    sw, zg, gl, gn, g_at, d_at, dn = h["sw"], h["zg"], h["gl"], h["gn"], h["g_at"], h["d_at"], h["dn"]
    zc = 1.92                                             # the box's roof
    lean = math.radians(12.0)                             # its sides lean in
    xt = 2.60 + gl.x / gl.z * (zc - zg)                   # the head of the front plate: the hull's own, carried on up
    xb = -0.85 + (zc - zg) * math.tan(math.radians(8.0))  # the head of the back plate
    ht = sw - (zc - zg) * math.tan(lean)                  # half the roof's width
    y_at = lambda z: sw - (z - zg) * math.tan(lean)       # noqa: E731
    sn = [Vector((0, s * math.cos(lean), math.sin(lean))) for s in (1.0, -1.0)]
    v.use("hull")
    v.hull([(2.60, sw, zg), (-0.85, sw, zg), (xt, ht, zc), (xb, ht, zc)], coat, bevel=0.016)

    # ---- the gun: a fixed cast collar on the front plate, a ball in it, the barrel -------------------------------
    gy, gz = -0.16, 1.48
    gp = g_at((gz - zg) / gl.z, gy)                       # where the bore meets the front plate
    v.lathe((gp.x - 0.9, gy, gz), X, [(1.20, 0.0), (1.20, 0.23), (1.08, 0.31), (0.5, 0.37), (0.0, 0.37), (0.0, 0.0)], coat, v.n(36, 12))
    v.part("turret", "hull", (gp.x + 0.1, gy, gz), axis=(0, 0, -1), limits=[-10.0, 10.0], kind="turn",
           note="the gun's own traverse in its mounting, ten degrees each way: + to the right; for more the game turns the whole of her")
    v.part("gun", "turret", (gp.x + 0.1, gy, gz), axis=(0, -1, 0), limits=[-6.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.ball((gp.x + 0.30, gy, gz), 0.215, coat, v.n(28, 10), v.n(14, 6))
    v.lathe((gp.x + 0.40, gy, gz), X, [(0.36, 0.0), (0.36, 0.1), (0.30, 0.118), (0.0, 0.14), (0.0, 0.0)], coat, v.n(24, 8))
    bx = gp.x + 0.74
    v.part("barrel", "gun", (bx, gy, gz), slide=(-1, 0, 0), slide_limits=[0.0, 0.32], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((bx, gy, gz), 1.9, 0.075, r0=0.088, r1=0.062, brake=None, sleeve=None)
    v.socket("muzzle", (bx + 1.92, gy, gz), X)
    m.info["bore"] = 0.075
    m.info["brake"] = False

    # ---- the roof: the commander's cap on the right, the loader's lid on the left, the driver's ahead of it ------
    v.use("hull")
    cup = v.cupola((-0.05, -0.44, zc), 0.27, 0.16, slits=4, start=0.4)
    v.hatch_round("hatch_commander", "hull", cup, 0.2, Z, -X, thick=0.028, opens=100.0)
    v.socket("commander", cup + Z * -0.25, X, part="hull", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "hull", (-0.28, 0.40, zc), (0.46, 0.52), Z, X, hinge="back", opens=100.0)
    v.hatch_rect("hatch_driver", "hull", (1.12, 0.46, zc), (0.40, 0.46), Z, X, hinge="front", opens=105.0)
    v.use("hull")
    v.slab((0.86, 0.0, zc), (1.06, 0.18, zc + 0.14), coat, 0.03, segs=2)                 # the gunner's sight under its hood
    v.periscope((0.74, -0.62, zc), X, (0.09, 0.13, 0.11))
    v.vent((0.42, -0.02, zc), 0.1, 0.045)
    v.aerial("aerial", "hull", (-0.52, -0.80, zc), 2.0)
    v.roll((-0.86, -0.62, 1.62), (-0.86, 0.62, 1.62), r=0.09, paint=v.own_paint("canvas.drab", "5C5B44"))      # her sheet, strapped across the back plate (dark enough to sit in her coat)
    if fine:
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            v.handle(Vector((0.9, s * y_at(1.78), 1.78)), Vector((0.3, s * y_at(1.78), 1.78)), n, 0.011, 0.06)
            v.port(Vector((-0.3, s * y_at(1.62), 1.62)), n, 0.04)
            v.weld(Vector((xt - 0.03, s * ht, zc)), Vector((xb + 0.03, s * ht, zc)), (n + Z).normalized())
        v.weld(Vector((xt, -ht + 0.03, zc)), Vector((xt, ht - 0.03, zc)), (gn + Z).normalized())
        for at in ((1.2, -0.66), (1.2, 0.8), (-0.52, 0.0)):
            v.lift_eye((at[0], at[1], zc), Z, Y, 0.03)

    v.socket("engine", d_at(0.6, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.5, 0.5) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="hull", note="out of the commander's hatch")
    v.socket("fire.3", (1.12, 0.46, zc + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("hit.front", g_at(0.4, 0.3), gn, part="hull")
    v.socket("hit.left", (0.3, y_at(1.5), 1.5), sn[0], part="hull")
    v.socket("hit.right", (0.3, -y_at(1.5), 1.5), sn[1], part="hull")
    v.socket("hit.rear", (-2.8, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", (0.5, y_at(1.75), 1.75), sn[0], part="hull", note="the fighting box's side")
    v.socket("case", (-0.28, 0.40, zc + 0.1), Y, part="hull", note="where the empty case is thrown out")
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        v.mark(tag, (0.3, s * (y_at(1.46) + 0.002), 1.46), n, 0.2, part="hull")
    v.tank_clips(recoil=0.3, kick=0.8, gun_low=-6.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver"], traverse=6.0,
                 idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_02", has_turret=False, mg=None)
    v.camouflage()
    return v
