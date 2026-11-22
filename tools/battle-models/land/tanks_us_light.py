"""The American light tank: the M3A1 Stuart of 1942 (the welded horseshoe turret with no cupola, the 37 mm gun).

Modelled to the real type's published measurements and layout (4.53 m long, 2.24 m wide, 2.30 m high, 12.9 tonnes;
two volute-spring bogies a side, each two twin-tyred wheels under a bracket with a return roller on its top; the drive
sprocket high at the front and the big trailing idler down on the ground at the back; the driver on the left of the
front and the bow gunner on the right with his machine gun in a ball; the radial engine in the back). Every shape is our
own mesh, made from numbers here: nothing traced or taken from any other model. What tells her from across a field:
how tall and boxy she stands for so small a tank, the track running down at the back to the idler on the ground, the
little gun in a wide shield on a round-fronted turret.
"""

import math

from mathutils import Vector

from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, inset_plan, sym_plan
from tanks_any import rivets
from tanks_ger import coat
from works import Works


def stuart(fine=True, tag="USA"):
    """M3A1 light tank: the US Army's and the Marines' light tank of 1942-43 from the Philippines to Guadalcanal and
    Tunisia, and the British 'Honey' of the desert and Burma."""
    v = Works("Stuart", "M3A1 light tank (Stuart)", "tank", tag, fine)
    # (the art director's E7 surface pass from the first build: shade from her shape, dirt of her ground. Her ground is
    #  the tropic's wet mud, for the Philippines and Guadalcanal: the art director, 22 Nov 20:54Z)
    v.dice = 0.2
    v.worn_kw = dict(shade=True, shade_deep=0.42, ground="tropic", layers=2, marks=0.3, gear=0.8, rim=True)
    v.root(hull_pivot=(0.0, 0.0, 0.70))
    hw, sw, zg, zr = 0.66, 0.94, 0.95, 1.58      # half-width of the tub between the tracks and of the hull over them; the sponsons' floor; the roof

    # ---- the tub with the rounded housing over the transmission, the box of the hull out over the tracks ---------------
    v.hull([(x, hw, z) for x, z in [(1.88, 0.42), (2.06, 0.47), (2.18, 0.56), (2.25, 0.68), (2.26, 0.80), (2.23, zg), (-2.14, zg), (-2.26, 0.82), (-2.16, 0.54), (-1.96, 0.42)]], coat, bevel=0.016)
    v.hull([(2.23, sw, zg), (2.03, sw, zr), (-2.08, sw, zr), (-2.26, sw, zr - 0.12), (-2.26, sw, zg)], coat, bevel=0.014)
    fl = Vector((2.03 - 2.23, 0, zr - zg)).normalized()            # up the front plate
    fn = Vector((fl.z, 0, -fl.x))
    f_at = lambda u, y: Vector((2.23, y, zg)) + fl * u + fn * 0.002      # noqa: E731
    # the driver on the left and the bow gunner on the right: a visor each in the front plate, his gun in a ball under
    # the gunner's, their lids in the roof over them
    v.visor(f_at(0.42, 0.42), fn, 0.30, 0.11, 0.05)
    v.visor(f_at(0.48, -0.42), fn, 0.24, 0.10, 0.045)
    v.mg_ball(f_at(0.20, -0.42) + fn * 0.02, Vector((0.96, 0, 0.06)).normalized(), 0.075, socket="hull_mg", length=0.34)
    v.use("hull")
    for lid, s in (("hatch_driver", LEFT), ("hatch_radio", RIGHT)):
        v.hatch_rect(lid, "hull", (1.70, s * 0.42, zr), (0.46, 0.40), Z, X, hinge="left" if s > 0 else "right", opens=100.0, thick=0.022)
        v.use("hull")
        v.periscope((1.98, s * 0.42, zr), X, (0.07, 0.13, 0.08))
    for s in (LEFT, RIGHT):
        v.headlamp((2.18, s * 0.80, zg + 0.04), X, 0.075, stalk=0.06, cover=True)
        v.hook((2.27, s * 0.40, 0.62), Vector((0.8, 0, -0.4)).normalized(), 0.07)
        v.hook((-2.27, s * 0.40, 0.68), -X, 0.07)
    if fine:
        rivets(v, f_at(0.03, -0.90), f_at(0.03, 0.90), fn, 0.10)
        rivets(v, f_at(0.62, -0.90), f_at(0.62, 0.90), fn, 0.10)
        for s in (LEFT, RIGHT):
            sn = Vector((0, s, 0))
            for z in (zg + 0.04, zr - 0.04):
                rivets(v, (2.10, s * (sw + 0.001), z), (-2.20, s * (sw + 0.001), z), sn, 0.10)
            for x in (1.45, -0.40, -1.20):
                rivets(v, (x, s * (sw + 0.001), zg + 0.10), (x, s * (sw + 0.001), zr - 0.10), sn, 0.10)
            v.port(Vector((0.95, s * (sw + 0.004), 1.30)), sn, 0.035)
            v.mark("USA", (-0.75, s * (sw + 0.002), 1.27), Y * s, 0.17, part="hull")

    # ---- the track guards along the sponsons' foot, turned down over the sprocket; the engine deck and the tail -------
    for s in (LEFT, RIGHT):
        y0, y1 = s * sw, s * 1.12
        v.slab((-2.16, y0, zg - 0.014), (2.12, y1, zg), coat, 0.004)
        v.plate([(2.12, y0, zg), (2.12, y1, zg), (2.36, y1, 0.82), (2.36, y0, 0.82)][::int(s)], 0.01, "upper", 0.003)
        if fine:
            for x in (-1.6, -0.5, 0.6, 1.6):
                v.slab((x - 0.012, s * sw, zg - 0.08), (x + 0.012, s * 1.10, zg - 0.014), "shade")
    v.shovel((0.3, 1.04, zg + 0.02), 0.0, 0.90)
    v.crowbar((-0.4, -1.04, zg + 0.018), 0.0, 1.2)
    v.bin((-2.08, 0.96, zg), (-1.40, 1.12, zg + 0.24), lid_side=Y)
    for s in (LEFT, RIGHT):
        v.grille((-1.30, s * 0.42, zr + 0.004), (1.10, 0.62), Z, X, slats=10)
    v.box((0.90, 0.14, 0.03), (-1.30, 0.0, zr + 0.015), "upper", 0.008)
    if fine:
        for x in (-0.80, -1.80):
            v.handle(Vector((x, -0.08, zr + 0.03)), Vector((x, 0.08, zr + 0.03)), Z, 0.009, 0.04)
        rivets(v, (-0.66, -0.88, zr + 0.002), (-0.66, 0.88, zr + 0.002), Z, 0.11)
    # the exhaust under its deflector on the back plate, two pipes
    v.box((0.10, 1.10, 0.22), (-2.31, 0.0, 1.14), "upper", 0.015)
    for k, s in enumerate((LEFT, RIGHT)):
        v.tube([(-2.27, s * 0.36, 1.04), (-2.37, s * 0.36, 1.02), (-2.40, s * 0.36, 0.96)], 0.045, "rust", v.n(12, 6))
        v.socket("exhaust." + str(k + 1), (-2.41, s * 0.36, 0.94), (-0.5, 0.0, -0.85), part="hull")
        v.headlamp((-2.27, s * 0.70, 1.30), -X, 0.04, stalk=0.02)

    # ---- the running gear: two volute-spring bogies a side, the sprocket high in front, the idler down on the ground ---
    ty, r_w, thick = 0.97, 0.255, 0.035
    wz = r_w + thick
    pairs = [-0.72, 0.80]
    xs = [b + d for b in pairs for d in (-0.30, 0.30)]
    rollers = [(b, 0.76, 0.06) for b in reversed(pairs)]
    v.running(xs, r_w, ty, (1.92, 0.62, 0.27), (-1.82, 0.30, 0.265), rollers=rollers, wheel_z=wz,
              wheel=dict(w=0.08, tyre=0.035, gap=0.05, kind="dish", holes=5, bolts=6, sides=28, hub=0.32, axle=0.10),
              sprocket_kw=dict(teeth=14, wide=0.30, tooth=0.03, holes=0, bolts=8, sides=28, hub=0.42),
              idler_kw=dict(wide=0.24, kind="spoke", spokes=6),
              roller_kw=dict(wide=0.14, tyre=0.012, reach=0.12),
              track=dict(pitch=0.14, wide=0.295, thick=thick, sag=0.012, style={"kind": "block", "chevron": False, "horn": "centre", "horn_high": 0.06, "grouser": 0.008}))
    for letter, s in SIDES:
        yo, yi = s * (ty + 0.12), s * (ty - 0.12)
        lo, hi = min(yo, yi), max(yo, yi)
        v.use("vehicle")
        for b in pairs:
            # the bracket: a narrow foot between the two wheels, a box over them for the spring, the roller on its top
            v.extrude([(b + x, z) for x, z in [(-0.05, 0.30), (0.05, 0.30), (0.07, 0.48), (0.20, 0.58), (0.20, 0.69), (-0.20, 0.69), (-0.20, 0.58), (-0.07, 0.48)]],
                      lo, hi, "upper", bevel=0.012)
            v.slab((b - 0.14, s * (hw - 0.01), 0.50), (b + 0.14, yi, 0.68), "shade", 0.008)
            for e in (-1.0, 1.0):              # each wheel's arm, from the bracket's foot out to its axle, and the lever up to the spring
                v.rod((b + e * 0.05, s * (ty + 0.125), 0.36), (b + e * 0.30, s * (ty + 0.125), wz), 0.03, "upper", 8)
                if fine:
                    v.rod((b + e * 0.24, s * (ty + 0.118), wz + 0.05), (b + e * 0.14, s * (ty + 0.118), 0.58), 0.02, "upper", 6)
            if fine:
                v.slab((b - 0.15, yo - s * 0.004, 0.59), (b + 0.15, yo + s * 0.012, 0.68), "upper", 0.005)
                for x in (-0.10, 0.0, 0.10):
                    v.bolt((b + x, yo + s * 0.012, 0.635), Y * s, 0.012, 0.008)
        # the idler's trailing arm, back from the hull to the idler on the ground; the sprocket's housing
        v.use("hull")
        v.extrude([(-1.40, 0.52), (-1.30, 0.60), (-1.76, 0.36), (-1.86, 0.27)], *sorted((s * (hw + 0.005), s * (ty - 0.13))), "upper", bevel=0.008)
        v.lathe((1.92, s * (hw - 0.01), 0.62), Y * s, [(0.14, 0.0), (0.14, 0.10), (0.07, 0.15), (0.0, 0.16), (0.0, 0.0)], "upper", v.n(20, 8))

    # ---- the turret: welded, round in front and flat at the back, its sides leaning in; a lid in two halves ------------
    x0, zt, th = 0.28, zr, 0.62
    v.ring_well((x0, 0.0, zt), 0.60)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    R = 0.64
    plan = sym_plan([(R * math.cos(math.radians(a)), R * math.sin(math.radians(a))) for a in (8, 24, 40, 56, 72, 88)] + [(-0.36, 0.63), (-0.58, 0.54), (-0.62, 0.40)])
    top = inset_plan(plan, [0.10] * 6 + [0.08, 0.06, 0.04, 0.06, 0.08] + [0.10] * 7)
    plan_w = [(x0 + x, y) for x, y in plan]
    top_w = [(x0 + x, y) for x, y in top]
    v.prism(plan_w, zt + 0.02, zt + th, coat, top=top_w, bevel=0.018, segs=2, angle=14.0)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.60), (0.0, 0.62), (0.0, 0.0)], "shade", v.n(32, 10))
    roof = zt + th
    v.hatch_rect("hatch_commander", "turret", (x0 - 0.12, 0.0, roof), (0.50, 0.56), Z, X, hinge="back", opens=100.0, thick=0.022, split=True)
    v.socket("commander", (x0 - 0.12, 0.12, roof - 0.25), X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    for s in (LEFT, RIGHT):
        v.periscope((x0 + 0.26, s * 0.26, roof), X, (0.06, 0.12, 0.07))
        if fine:
            v.port(Vector((x0 - 0.10, s * 0.585, zt + 0.30)), Vector((0, s, 0.16)), 0.035)
            v.visor(Vector((x0 + 0.30, s * 0.505, zt + 0.40)), Vector((0.55, s * 0.80, 0.18)).normalized(), 0.14, 0.05, 0.025, brow=False)
    if fine:
        v.port(Vector((x0 - 0.585, 0.0, zt + 0.30)), Vector((-1, 0, 0.06)), 0.035)
        for at in ((x0 + 0.30, 0.0), (x0 - 0.45, 0.30), (x0 - 0.45, -0.30)):
            v.lift_eye((at[0], at[1], roof), Z, Y, 0.025)

    # ---- the gun: the 37 mm in the wide curved shield of its mount, the machine gun beside it on the left -------------
    tx, tz = x0 + 0.52, zt + 0.30
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-10.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx, 0.0, tz), Y, [(0.34, 0.0), (0.34, 0.17), (0.30, 0.205), (-0.30, 0.205), (-0.34, 0.17), (-0.34, 0.0)], "upper", v.n(30, 10))
    v.lathe((tx + 0.19, 0.0, tz), X, [(0.10, 0.0), (0.10, 0.075), (0.04, 0.085), (0.0, 0.09), (0.0, 0.0)], "upper", v.n(18, 8))
    v.mg((tx + 0.17, 0.17, tz - 0.01), X, 0.14)
    v.socket("coax", (tx + 0.31, 0.17, tz - 0.01), X)
    if fine:
        v.port(Vector((tx + 0.19, -0.17, tz + 0.03)), X, 0.025)              # the gunner's sight
    v.part("barrel", "gun", (tx + 0.28, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.22], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.28, 0.0, tz), 1.48, 0.037, r0=0.052, r1=0.040, brake=None, sleeve=(0.28, 0.064))
    v.socket("muzzle", (tx + 1.76, 0.0, tz), X)
    v.m.info["bore"] = 0.037
    v.m.info["brake"] = False

    # ---- sockets for what happens to her ------------------------------------------------------------------------
    v.socket("engine", (-1.30, 0.0, zr + 0.05), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-1.30, 0.42, zr + 0.05), Z, part="hull")
    v.socket("fire.2", (x0 - 0.12, 0.0, roof + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.70, 0.42, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", f_at(0.35, 0.0), fn, part="hull")
    v.socket("hit.left", (0.2, sw + 0.01, 1.25), Y, part="hull")
    v.socket("hit.right", (0.2, -sw - 0.01, 1.25), -Y, part="hull")
    v.socket("hit.rear", (-2.27, 0.0, 1.30), -X, part="hull")
    v.socket("hit.turret", (x0, 0.62, zt + 0.30), Y, part="turret")
    v.socket("case", (x0 - 0.30, 0.60, zt + 0.32), Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.22, kick=0.45, gun_low=-10.0, hatches=["hatch_commander", "hatch_driver", "hatch_radio"],
                 traverse=20.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.6, 2.0, 0.34, 165.0))
    v.name_place((0.70, 1.27), long=0.75, high=0.11, lines=(2, 1), high_two=0.09)                 # her crew's name (b477): on the sponson, ahead of the star
    return v
