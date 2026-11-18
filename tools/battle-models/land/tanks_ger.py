"""Germany's tanks: Panzer IV (the line tank), Panther (from 1944), Tiger (the heavy unit).

Each is modelled to the real type's measurements and layout from the published dimensions; every shape is our own
mesh, made from numbers here. Markings are the plain beam cross and turret numbers, nothing else.
"""

import math

from mathutils import Vector

from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, bm_box, outline, rot, round_plan
from works import Works


def coat(mid, n):
    """The coat, and its shadowed underside."""
    return "shade" if n.z < -0.75 else "upper"


def plane_of(p0, p1, p2, toward):
    """The outward facing of the plane through three points (`toward` any point on its outer side)."""
    p0, p1, p2 = Vector(p0), Vector(p1), Vector(p2)
    n = (p1 - p0).cross(p2 - p0).normalized()
    return n if n.dot(Vector(toward) - p0) > 0 else -n


def panzer4(fine=True, tag="GER"):
    """Panzer IV Ausf. H: 5.92 m hull, 2.88 m wide, 2.68 m high; eight small road wheels a side on leaf-sprung
    bogies, four return rollers, drive at the front; 7.5 cm L/48 with a double-baffle muzzle brake."""
    v = Works("PanzerIV", "Panzer IV", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.75))
    m = v.m

    # ---- the hull: a tub between the tracks, a superstructure over them -----------------------------
    hw = 0.925
    tub = [(-2.62, 0.44), (-2.74, 0.66), (-2.74, 1.10), (1.72, 1.10), (2.56, 1.03), (2.67, 0.88), (2.60, 0.62), (2.28, 0.40), (-2.40, 0.40)]
    v.hull([(x, hw, z) for x, z in tub], coat, bevel=0.014)
    sw = 1.31
    v.hull([(1.67, sw, 1.72), (1.74, sw, 1.10), (-0.95, sw, 1.72), (-2.72, sw, 1.60), (-2.72, sw, 1.10)], coat, bevel=0.014)
    # track guards: one long plate a side, turned down at each end
    for s in (LEFT, RIGHT):
        y0, y1 = s * 0.93, s * 1.44
        v.slab((-2.80, y0, 1.085), (2.62, y1, 1.10), coat, 0.004)
        v.plate([(2.62, y0, 1.10), (2.62, y1, 1.10), (2.94, y1, 0.90), (2.94, y0, 0.90)][::int(s)], 0.012, "upper", 0.003)
        v.plate([(-2.80, y1, 1.10), (-2.80, y0, 1.10), (-3.00, y0, 0.84), (-3.00, y1, 0.84)][::int(s)], 0.012, "upper", 0.003)
        if fine:
            for x in (-2.2, -1.2, 0.0, 1.0, 2.0):          # the brackets under them
                v.slab((x - 0.015, s * 0.93, 0.98), (x + 0.015, s * 1.40, 1.085), "shade")
            # the lip along the outer edge
            v.slab((-2.80, s * 1.425, 1.06), (2.62, s * 1.44, 1.10), "upper")
    # the nose: bolted brake and gearbox hatches on the glacis, spare links across the lower plate
    gl = Vector((2.56 - 1.72, 0, 1.03 - 1.10)).normalized()
    gn = Vector((-gl.z, 0, gl.x))
    for y in (-0.52, 0.52):
        c = Vector((2.12, y, 1.10 + (2.12 - 1.72) * (1.03 - 1.10) / (2.56 - 1.72)))
        v.put(bm_box((0.5, 0.44, 0.022), c + gn * 0.011, rot(0, math.degrees(math.atan2(gl.z, gl.x)))), "upper", 0.006)
        if fine:
            v.bolts(c + gn * 0.022, gn, 0.0, 1, r=0.012)
            for dx in (-0.2, 0.2):
                for dy in (-0.17, 0.17):
                    v.bolt(c + gl * dx + Y * dy + gn * 0.022, gn, 0.012)
            v.vent(c + gl * 0.02 + gn * 0.02, 0.07, 0.035)
    c = Vector((2.18, 0.0, 1.10 + (2.18 - 1.72) * (1.03 - 1.10) / (2.56 - 1.72)))
    v.put(bm_box((0.42, 0.5, 0.02), c + gn * 0.01, rot(0, math.degrees(math.atan2(gl.z, gl.x)))), "upper", 0.006)
    if fine:
        for dx in (-0.17, 0.17):
            for dy in (-0.2, 0.0, 0.2):
                v.bolt(c + gl * dx + Y * dy + gn * 0.02, gn, 0.012)
        v.handle(c + Y * -0.08 + gn * 0.02, c + Y * 0.08 + gn * 0.02, gn, 0.007, 0.03, "steel")
    # (lower nose plate runs from (2.67, 0.88) down to (2.60, 0.62))
    nf = Vector((0.26, 0, 0.07)).normalized()
    v.spare_links((2.655, -0.72, 0.76), (2.655, 0.72, 0.76), 12, 0.12, 0.2, Vector((0.97, 0, 0.12)).normalized())
    for y in (-0.62, 0.62):
        v.hook((2.50, y, 0.50), Vector((0.8, 0, -0.5)).normalized(), 0.085)
        v.hook((-2.72, y, 0.62), -X, 0.085)
    # the driver's plate: his visor on the left, the hull machine gun in its ball on the right
    v.visor((1.705, 0.55, 1.47), Vector((1.0, 0, 0.11)).normalized(), 0.36, 0.13, 0.07)
    v.mg_ball((1.71, -0.55, 1.43), Vector((1.0, 0, 0.05)).normalized(), 0.115, socket="hull_mg")
    if fine:
        v.port((1.2, sw + 0.002, 1.48), Y * 1.0, 0.05)
        v.port((1.2, -sw - 0.002, 1.48), Y * -1.0, 0.05)
        # weld lines where the plates meet
        for s in (LEFT, RIGHT):
            v.weld((1.70, s * 0.3, 1.105), (1.70, s * 1.28, 1.105), X)
    # hatches for the driver and the radio operator
    v.hatch_rect("hatch_driver", "hull", (1.22, 0.62, 1.72), (0.46, 0.50), Z, X, hinge="front", opens=105.0)
    v.hatch_rect("hatch_radio", "hull", (1.22, -0.62, 1.72), (0.46, 0.50), Z, X, hinge="front", opens=105.0)
    v.use("hull")
    # the engine deck: two big doors with grilles, and the air louvres on each side
    dz = (1.60 - 1.72) / (-2.72 + 0.95)
    dn = Vector((-dz, 0, 1)).normalized()
    da = Vector((1, 0, dz)).normalized()
    for y in (-0.56, 0.56):
        c = Vector((-1.85, y, 1.72 + (-1.85 + 0.95) * dz))
        v.put(bm_box((1.25, 0.9, 0.02), c + dn * 0.01, rot(0, math.degrees(math.atan2(da.z, da.x)))), "upper", 0.006)
        v.grille(c + da * 0.18 + dn * 0.02, (0.62, 0.6), dn, Y, slats=9, deep=0.022)
        if fine:
            v.handle(c + da * -0.45 + Y * -0.1 + dn * 0.02, c + da * -0.45 + Y * 0.1 + dn * 0.02, dn, 0.007, 0.035, "steel")
            for hy in (-0.3, 0.3):
                v.hinge(c + da * 0.62 + Y * (hy - 0.05) + dn * 0.02, c + da * 0.62 + Y * (hy + 0.05) + dn * 0.02, 0.013)
    for s in (LEFT, RIGHT):
        v.slab((-2.62, s * sw, 1.16), (-1.25, s * (sw + 0.09), 1.52), "upper", 0.012)
        v.louvres((-1.93, s * (sw + 0.091), 1.34), (1.2, 0.28), Y * s, 5)
    # the rear plate: the main silencer across it, the small one for the traverse engine beside it
    v.silencer((-2.92, -0.62, 0.93), (-2.92, 0.46, 0.93), 0.15, pipe=[(-2.74, 0.2, 1.02), (-2.86, 0.2, 1.0), (-2.92, 0.2, 0.96)], tail=((-2.92, -0.62, 0.93), (-0.3, -1, -0.2)))
    v.silencer((-2.88, 0.56, 0.95), (-2.88, 0.86, 0.95), 0.085, bands=1, tail=((-2.88, 0.86, 0.95), (-0.5, 1, -0.3)))
    v.socket("exhaust.1", (-2.98, -0.78, 0.9), (-0.3, -1, -0.2))
    v.socket("exhaust.2", (-2.94, 0.98, 0.92), (-0.5, 1, -0.3))
    if fine:
        for y in (-0.5, 0.0, 0.34):
            v.slab((-2.80, y - 0.02, 0.80), (-2.74, y + 0.02, 1.06), "upper")
        v.hook((-2.74, 0.0, 0.56), -X, 0.1)
    # lamps, tools and stowage
    v.headlamp((2.38, 1.14, 1.10), X, 0.075, stalk=0.09)
    v.headlamp((2.38, -1.14, 1.10), X, 0.06, stalk=0.07, cover=True)
    v.jack((-0.75, -1.22, 1.10), 0.0)
    v.jack_block((-2.25, 1.20, 1.10), (0.4, 0.24, 0.13))
    v.shovel((0.15, 1.36, 1.125), 0.0, 0.95)
    v.axe((-1.0, -1.38, 1.125), 180.0, 0.75, roll=90.0)
    v.crowbar((0.05, -1.38, 1.118), 0.0, 1.35)
    v.cable([(1.55, 1.2, 1.13), (0.9, 1.26, 1.125), (-0.6, 1.26, 1.125), (-1.1, 1.22, 1.125), (-1.25, 1.12, 1.125), (-1.1, 1.04, 1.125), (0.9, 1.04, 1.125), (1.5, 1.08, 1.13)], 0.013)
    if fine:
        # two spare road wheels in their rack on the left
        for x in (-1.72, -1.2):
            v.wheel_disc((x, 1.39, 1.36), Y, 0.235, 0.085, 0.032, bolts=6, sides=24)
        v.slab((-2.0, 1.33, 1.10), (-0.92, 1.345, 1.42), "upper")
        v.box((0.5, 0.2, 0.17), (1.55, -1.2, 1.185), "upper", 0.01)        # the track tool box
        v.rod((0.75, -1.2, 1.17), (1.05, -1.2, 1.17), 0.06, "red", 12)     # the fire extinguisher
    v.aerial("aerial", "hull", (-2.35, 1.22, 1.60), 2.0)

    # ---- the running gear ----------------------------------------------------------------------------
    ty = 1.225
    xs = [-1.76 + 0.503 * i for i in range(8)]
    r_w, r_s, r_i, r_r = 0.235, 0.335, 0.31, 0.125
    spr, idl = (2.50, 0.70), (-2.52, 0.64)
    rollers = [(-1.50, 0.905), (-0.50, 0.905), (0.50, 0.905), (1.50, 0.905)]
    for letter, s in SIDES:
        v.use("vehicle")
        for i, x in enumerate(xs):
            v.road_wheel(f"wheel_{letter}{i + 1}", "vehicle", x, s * ty, r_w, r_w, s, w=0.085, tyre=0.032, gap=0.075, bolts=6, sides=30)
        v.sprocket(f"sprocket_{letter}", "vehicle", spr[0], s * ty, spr[1], r_s, s, teeth=18, wide=0.24, tooth=0.03, holes=6, sides=30)
        v.idler(f"idler_{letter}", "vehicle", idl[0], s * ty, idl[1], r_i, s, wide=0.24, kind="spoke", spokes=7)
        for i, (x, z) in enumerate(rollers):
            v.roller(f"roller_{letter}{i + 1}", "vehicle", x, s * ty, z, r_r, s, wide=0.17, tyre=0.02, reach=0.26)
        wheels = [(x, r_w, r_w) for x in xs] + [(spr[0], spr[1], r_s)] + [(x, z, r_r) for x, z in reversed(rollers)] + [(idl[0], idl[1], r_i)]
        v.track(letter, s, s * ty, wheels, pitch=0.12, wide=0.40, thick=0.028, style={"kind": "plate", "horn": "centre", "horn_high": 0.075, "grouser": 0.014})
        # the bogies: a leaf spring under each pair of wheels, on the root so they stay with the wheels
        v.use("vehicle")
        for k in range(4):
            xa, xb = xs[2 * k], xs[2 * k + 1]
            xm = (xa + xb) / 2
            yi = s * (ty - 0.30)
            v.slab((xm - 0.11, s * 0.93, 0.36), (xm + 0.11, s * (ty - 0.2), 0.58), "shade", 0.01)
            if fine:
                for j in range(4):                       # the leaves
                    half = 0.27 - j * 0.045
                    v.slab((xm - half, yi - 0.04, 0.30 - j * 0.02), (xm + half, yi + 0.04, 0.315 - j * 0.02), "shade")
                for xw in (xa, xb):                      # the swing arms
                    v.rod((xm + (0.06 if xw > xm else -0.06), yi, 0.46), (xw, yi, r_w), 0.035, "shade", 8)
                v.bolts((xm, s * (ty - 0.2) + s * 0.001, 0.47), Y * s, 0.075, 6, r=0.012)
        # final drive housing at the sprocket
        v.use("hull")
        v.lathe((spr[0], s * 0.90, spr[1]), Y * s, [(0.22, 0.0), (0.22, 0.13), (0.16, 0.2), (0.0, 0.24), (0.0, 0.0)], "upper", v.n(22, 8))
        v.socket("dust." + ("left" if s > 0 else "right"), (-2.75, s * ty, 0.05), (-1, 0, 0.3), part="vehicle")

    # ---- the turret -----------------------------------------------------------------------------------
    x0, zt = 0.18, 1.72
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    base = [(1.02, 0.60), (0.22, 0.93), (-0.72, 0.86), (-1.06, 0.50)]
    roof = [(0.92, 0.48, 2.21), (0.24, 0.65, 2.27), (-0.62, 0.61, 2.29), (-0.93, 0.40, 2.29)]
    v.hull([(x0 + x, y, zt + 0.02) for x, y in base] + [(x0 + x, y, z) for x, y, z in roof], coat, bevel=0.016)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.82), (0.0, 0.84), (0.0, 0.0)], "shade", v.n(36, 12))        # the ring's collar
    # the bin on the turret's back
    v.bin((x0 - 1.52, -0.46, 1.86), (x0 - 1.06, 0.46, 2.22), bevel=0.03, lid_side=(-1, 0, 0), latches=3)
    # side doors: two leaves each side, the fore leaf a part that opens
    for letter, s in SIDES:
        p0 = Vector((x0 + 0.22, s * 0.93, zt + 0.02))
        p1 = Vector((x0 - 0.72, s * 0.86, zt + 0.02))
        p2 = Vector((x0 + 0.24, s * 0.65, 2.27))
        p3 = Vector((x0 - 0.62, s * 0.61, 2.29))
        n = plane_of(p0, p1, p2, p0 + Y * s)
        cc = (p0 + p1 + p2 + p3) / 4 + n * 0.004
        along = (p0 - p1).normalized()
        upw = n.cross(along) * s
        for k, nm in ((0.21, "hatch_gunner" if s > 0 else "hatch_loader"), (-0.19, None)):
            c = cc + along * k
            if nm:
                v.hatch_rect(nm, "turret", c, (0.36, 0.40), n, along, hinge="front", opens=110.0, thick=0.022, well=True)
                v.use(nm)
                v.port(c + along * -0.02 + n * 0.022, n, 0.04)
                v.use("turret")
            else:
                v.hatch_rect("door_" + letter, "turret", c, (0.36, 0.40), n, along, hinge="back", opens=110.0, thick=0.022, well=True)
                v.use("turret")
        # vision port on the fore side plate, lifting eye above
        q0, q1 = Vector((x0 + 1.02, s * 0.60, zt + 0.02)), Vector((x0 + 0.92, s * 0.48, 2.21))
        nf2 = plane_of(q0, p0, q1, q0 + Y * s + X)
        v.port((q0 + p0 + q1 + p2) / 4 + nf2 * 0.004, nf2, 0.05)
        v.lift_eye((x0 + 0.62, s * 0.5, 2.235), Z, X, 0.035)
    v.lift_eye((x0 - 0.86, 0.0, 2.292), Z, Y, 0.035)
    # the commander's cupola and its lid
    cup = v.cupola((x0 - 0.56, 0.0, 2.285), 0.335, 0.34, slits=5, start=math.radians(36))
    v.hatch_round("hatch_commander", "turret", cup + Z * 0.0, 0.25, Z, -X, thick=0.03, opens=105.0)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander stands here, head and shoulders out when the lid is open")
    v.use("turret")
    v.vent((x0 + 0.42, -0.2, 2.245), 0.12, 0.05)
    v.port((x0 + 0.2, 0.32, 2.262), Z, 0.05)
    # turret face: the gunner's sight port and a visor each side of the gun
    if fine:
        for y in (-0.38, 0.38):
            v.visor((x0 + 0.985, y, 2.02), Vector((1, 0, 0.18)).normalized(), 0.16, 0.09, 0.03, brow=False)

    # ---- the gun --------------------------------------------------------------------------------------
    tx, tz = x0 + 0.92, 1.98
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    # the mantlet: a curved shield, and the armoured housing of the recoil gear
    plan = [(0.10, -0.47), (0.24, -0.40), (0.30, -0.2), (0.30, 0.2), (0.24, 0.40), (0.10, 0.47)]
    v.prism([(tx + x, y) for x, y in plan], tz - 0.27, tz + 0.27, "upper", bevel=0.018, segs=2)
    v.lathe((tx + 0.28, 0, tz), X, [(0.52, 0.0), (0.52, 0.115), (0.46, 0.135), (0.0, 0.17), (0.0, 0.0)], "upper", v.n(24, 8))
    v.slab((tx + 0.28, -0.11, tz - 0.25), (tx + 0.62, 0.11, tz - 0.12), "upper", 0.012)
    if fine:
        v.bolts((tx + 0.30, 0, tz), X, 0.20, 8, r=0.013)
    v.mg((tx + 0.30, -0.30, tz + 0.02), X, 0.34)
    v.socket("coax", (tx + 0.66, -0.30, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.80, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.42], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.80, 0, tz), 2.70, 0.075, r0=0.098, r1=0.066, brake="double", steps=((0.36, None),), sleeve=None)
    v.socket("muzzle", (tx + 3.52, 0, tz), X)
    v.m.info["bore"] = 0.075

    # ---- sockets for what happens to her ------------------------------------------------------------------
    v.socket("engine", (-1.9, 0.0, 1.66), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-1.7, 0.3, 1.66), Z, part="hull")
    v.socket("fire.2", (x0 - 0.56, 0.0, 2.6), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.22, 0.62, 1.74), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", (2.62, 0.2, 0.9), X, part="hull")
    v.socket("hit.left", (0.2, 1.31, 1.4), Y, part="hull")
    v.socket("hit.right", (0.2, -1.31, 1.4), -Y, part="hull")
    v.socket("hit.rear", (-2.74, 0.0, 1.2), -X, part="hull")
    v.socket("hit.turret", (x0 + 0.6, 0.78, 2.0), Y, part="turret")
    v.socket("case", (x0 - 0.2, -0.75, 2.0), -Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.38, kick=0.9, gun_low=-8.0, hatches=["hatch_commander", "hatch_gunner", "hatch_loader", "hatch_driver", "hatch_radio"],
                 traverse=14.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.9, 2.9, 0.42, 172.0))
    return v
