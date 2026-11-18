"""The Soviet Union's tanks: the T-34 with the 76 mm gun (the line tank) and the T-34-85 (from 1944).

Each is modelled to the real type's measurements and layout from the published dimensions; every shape is our own
mesh, made from numbers here. Both stand on the one hull: sloped plate all round, five big wheels a side on Christie
springs with the track lying on them, the drive at the back.
"""

import math

from mathutils import Matrix, Vector

from motorpool import LEFT, RIGHT, X, Y, Z, inset_plan, sym_plan
from tanks_ger import coat
from works import Works


def t34_hull(v, fine, bow_mg=True):
    """The hull the two T-34s share: 5.92 m long, 3.00 m wide, the roof 1.45 m up. Returns the places a turret needs:
    the ring's middle, the roof's height, and the plates' own measures (for sockets). bow_mg: the machine gun in the
    front plate (the assault guns built on this hull have their gun there)."""
    v.root(hull_pivot=(0.0, 0.0, 0.8))
    hw, sw, st, zf, zr = 0.92, 1.47, 0.95, 0.92, 1.56
    # ---- a tub between the tracks; over them a box whose every side leans in --------------------------
    v.hull([(x, hw, z) for x, z in [(-2.66, 0.42), (-2.94, 0.62), (-2.94, 0.94), (2.94, 0.94), (3.00, 0.86), (2.50, 0.42)]], coat, bevel=0.014)
    v.hull([(2.94, sw, zf), (1.82, st, zr), (-2.22, st, zr), (-2.94, sw, zf)], coat, bevel=0.016)
    gl = Vector((1.82 - 2.94, 0, zr - zf)).normalized()               # up the glacis
    gn = Vector((gl.z, 0, -gl.x))                                     # out of it
    sn = [Vector((0, s * (zr - zf), sw - st)).normalized() for s in (1.0, -1.0)]       # out of each side plate
    rb = Vector((-2.22 + 2.94, 0, zr - zf)).normalized()             # up the tail plate
    rn = Vector((-rb.z, 0, rb.x))

    def side_y(z):
        return sw - (z - zf) * (sw - st) / (zr - zf)
    g_at = lambda u, y: Vector((2.94, y, zf)) + gl * u + gn * 0.002       # noqa: E731  (a place on the glacis, u metres up it)
    r_at = lambda u, y: Vector((-2.94, y, zf)) + rb * u + rn * 0.002      # noqa: E731  (a place on the tail plate)
    # the nose: the beam the two front plates meet on, towing hooks, the driver's big hatch, the machine gun's bulge
    v.rod((2.95, -sw + 0.03, 0.90), (2.95, sw - 0.03, 0.90), 0.05, "upper", v.n(14, 6))
    v.hatch_rect("hatch_driver", "hull", g_at(0.72, 0.36), (0.50, 0.62), gn, gl, hinge="front", opens=80.0, thick=0.045, lip=0.012)
    if fine:
        # the two periscopes in their armoured hoods on the lid
        turn = Matrix((gl, Vector((0, 1, 0)), gn)).transposed()
        for y in (0.22, 0.50):
            v.box((0.13, 0.12, 0.06), g_at(0.86, y) + gn * 0.075, "upper", 0.012, turn=turn, part="hatch_driver")
            v.box((0.012, 0.08, 0.022), g_at(0.86, y) + gn * 0.08 - gl * 0.066, "glass", turn=turn, part="hatch_driver")
    v.use("hull")
    if bow_mg:
        v.lathe(g_at(0.66, -0.52) - gn * 0.02, gn, [(0.12, 0.0), (0.12, 0.09), (0.07, 0.16), (0.0, 0.2), (0.0, 0.0)], "upper", v.n(24, 8))
        v.mg_ball(g_at(0.66, -0.52) + gn * 0.10, gn, 0.085, socket="hull_mg", length=0.36, ring=False)
    for s in (LEFT, RIGHT):
        v.hook(Vector((2.99, s * 0.62, 0.74)), X, 0.1)
        v.hook(Vector((-2.95, s * 0.62, 0.72)), -X, 0.1)
        # the track guards: a mudguard curved down round the front of each track, a short flap behind
        arc = [(2.70 + 0.388 * math.cos(math.radians(a)), 0.62 + 0.388 * math.sin(math.radians(a))) for a in (51.8, 40.0, 28.0, 16.0, 4.0)]
        for (xa, za), (xb, zb) in zip(arc, arc[1:]):
            v.plate([(xa, s * 0.95, za), (xa, s * 1.49, za), (xb, s * 1.49, zb), (xb, s * 0.95, zb)][::int(s)], 0.007, "upper", 0.002)
        v.plate([(-3.14, s * 0.95, 0.84), (-3.14, s * 1.49, 0.84), (-2.94, s * 1.49, 0.925), (-2.94, s * 0.95, 0.925)][::int(s)], 0.007, "upper", 0.003)
    if fine:
        v.spare_links(g_at(0.26, -0.344), g_at(0.26, 0.344), 4, 0.172, 0.5, gn, style={"kind": "plate", "grouser": 0.012, "cleats": True})
        v.weld(g_at(0.04, -1.2), g_at(0.04, 1.2), gn)
    v.headlamp(Vector((1.96, side_y(1.30) + 0.02, 1.30)), X, 0.07, stalk=0.06)
    # the roof ahead of the turret; the engine deck behind it: a domed hatch in the middle, a louvred cover each side,
    # and the mesh over the gearbox at the back
    v.slab((-1.70, -0.34, zr), (-0.62, 0.34, zr + 0.03), "upper", 0.01)
    v.ball((-1.16, 0.0, zr + 0.03), 0.16, "upper", 14, 6, dome=True, squash=0.45)
    if fine:
        for x in (-1.6, -0.72):
            v.handle((x, -0.1, zr + 0.03), (x, 0.1, zr + 0.03), Z, 0.008, 0.04, "steel")
    for s in (LEFT, RIGHT):
        v.slab((-1.72, s * 0.40, zr), (-0.60, s * 0.91, zr + 0.055), "upper", 0.012)
        v.grille((-1.16, s * 0.655, zr + 0.055), (1.0, 0.40), Z, X, slats=8, deep=0.02)
    v.slab((-2.19, -0.86, zr), (-1.78, 0.86, zr + 0.045), "upper", 0.012)
    v.grille((-1.985, 0.0, zr + 0.045), (1.56, 0.34), Z, Y, slats=6, deep=0.02, mesh=True)
    # the tail plate: the round gearbox door, an exhaust in its armoured cowl each side of it
    v.lathe(r_at(0.36, 0.0), rn, [(0.035, 0.0), (0.035, 0.26), (0.0, 0.30)], "upper", v.n(26, 8))
    if fine:
        v.bolts(r_at(0.36, 0.0) + rn * 0.035, rn, 0.2, 8, r=0.012)
        for y in (-0.9, 0.9):
            v.hinge(r_at(0.02, y - 0.1), r_at(0.02, y + 0.1), 0.02)
    for i, y in enumerate((0.56, -0.56)):
        v.lathe(r_at(0.40, y), rn, [(0.11, 0.0), (0.09, 0.08), (0.0, 0.12)], "upper", v.n(16, 6))
        v.tube([r_at(0.40, y) + rn * 0.06, r_at(0.36, y) + rn * 0.2, r_at(0.24, y) + rn * 0.3], 0.045, "rust", v.n(10, 5))
        v.socket(f"exhaust.{i + 1}", r_at(0.22, y) + rn * 0.32, (-1, 0, -0.5))
    # along the sides: the fuel drums on the after part, hand rails for the riders, a saw, the tow cable
    for y_sign, xs_ in ((RIGHT, (-0.25, -1.30)), (LEFT, (-1.30,))):
        for x in xs_:
            c = Vector((x, y_sign * side_y(1.26), 1.26)) + sn[0 if y_sign > 0 else 1] * 0.2
            v.silencer(c + X * 0.42, c - X * 0.42, 0.17, paint="upper", bands=2)
    if fine:
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            for xa, xb in ((1.55, 0.95), (0.55, -0.05)):
                v.handle(Vector((xa, s * side_y(1.38), 1.38)), Vector((xb, s * side_y(1.38), 1.38)), n, 0.011, 0.06)
        v.cable([(2.2, side_y(1.12) + 0.03, 1.12), (0.8, side_y(1.08) + 0.03, 1.08), (-0.4, side_y(1.12) + 0.03, 1.12)], 0.014)
        v.crowbar((0.2, -side_y(1.36) - 0.02, 1.36), 0.0, 1.2)
    v.aerial("aerial", "hull", (1.40, -side_y(1.46) + 0.04, 1.48), 2.0)

    # ---- the running gear: five big wheels a side, no return rollers, the drive at the back ---------
    ty, r_w = 1.22, 0.415
    v.running([-1.90, -0.93, 0.0, 0.93, 1.86], r_w, ty, (-2.66, 0.58, 0.31), (2.70, 0.62, 0.25), drive="rear", top_on_wheels=True,
              wheel=dict(w=0.15, tyre=0.045, gap=0.11, bolts=10, sides=36, hub=0.36, axle=0.26, holes=0),
              sprocket_kw=dict(teeth=12, wide=0.34, tooth=0.012, holes=0, bolts=8, sides=30, hub=0.5),
              idler_kw=dict(wide=0.34, kind="dish", spokes=0, tyre=0.0, hub=0.4),
              track=dict(pitch=0.172, wide=0.50, thick=0.034, sag=0.015, style={"kind": "plate", "horn": "centre", "horn_high": 0.1, "grouser": 0.012, "cleats": True, "bar": 0.3}))
    v.use("hull")
    for s in (LEFT, RIGHT):
        # the final drive's housing at the back, the idler's crank at the front
        v.lathe((-2.66, s * 0.86, 0.58), Y * s, [(0.16, 0.0), (0.16, 0.16), (0.08, 0.25), (0.0, 0.28), (0.0, 0.0)], "upper", v.n(22, 8))
        v.lathe((2.70, s * 0.88, 0.62), Y * s, [(0.12, 0.0), (0.12, 0.09), (0.0, 0.13), (0.0, 0.0)], "upper", v.n(16, 8))
    # sockets every T-34 has
    v.socket("engine", (-1.16, 0.0, zr + 0.06), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-1.16, 0.69, zr + 0.06), Z, part="hull")
    v.socket("fire.3", g_at(0.72, 0.36) + gn * 0.05, gn, part="hull", note="out of the driver's hatch")
    v.socket("hit.front", g_at(0.5, -0.1), gn, part="hull")
    v.socket("hit.left", (0.2, side_y(1.2), 1.2), sn[0], part="hull")
    v.socket("hit.right", (0.2, -side_y(1.2), 1.2), sn[1], part="hull")
    v.socket("hit.rear", r_at(0.36, 0.0), rn, part="hull")
    return {"zr": zr, "side_y": side_y, "sn": sn}


def t34(fine=True, tag="SOV"):
    """T-34 of 1942-43: 5.92 m hull, 3.00 m wide, 2.45 m high, 6.68 m over the gun; the six-sided turret with two
    round hatches, the 76 mm F-34 in a rounded mantlet under its recoil housing."""
    v = Works("T34", "T-34", "tank", tag, fine)
    h = t34_hull(v, fine)
    zr = h["zr"]
    x0, zt = 0.62, zr
    v.ring_well((x0, 0.0, zt), 0.74)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    th = 0.72
    plan = sym_plan([(1.00, 0.42), (0.34, 0.95), (-0.50, 0.95), (-1.10, 0.52)])
    top = inset_plan(plan, [0.24, 0.26, 0.24, 0.18, 0.24, 0.26, 0.24, 0.10])
    v.prism([(x0 + x, y) for x, y in plan], zt + 0.02, zt + th, coat, top=[(x0 + x, y) for x, y in top], bevel=0.03, segs=3, angle=14.0)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.78), (0.0, 0.80), (0.0, 0.0)], "shade", v.n(36, 12))
    roof = zt + th
    # two round lids side by side, the gunner's sight and the loader's periscope ahead of them, a ventilator between
    v.hatch_round("hatch_commander", "turret", (x0 - 0.40, 0.36, roof), 0.26, Z, X, thick=0.03, opens=95.0)
    v.hatch_round("hatch_loader", "turret", (x0 - 0.40, -0.36, roof), 0.26, Z, X, thick=0.03, opens=95.0)
    v.socket("commander", (x0 - 0.40, 0.36, roof - 0.25), X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.periscope((x0 + 0.42, 0.40, roof), X, (0.09, 0.13, 0.11))
    v.periscope((x0 + 0.42, -0.40, roof), X, (0.09, 0.13, 0.11))
    v.vent((x0 + 0.12, 0.0, roof), 0.12, 0.05)
    if fine:
        for s in (LEFT, RIGHT):
            n = Vector((0, s * 0.94, 0.34)).normalized()
            v.handle(Vector((x0 + 0.16, s * 0.80, zt + 0.42)), Vector((x0 - 0.40, s * 0.80, zt + 0.42)), n, 0.011, 0.06)
            v.port(Vector((x0 - 0.20, s * 0.865, zt + 0.24)), n, 0.04)
        for at in ((x0 + 0.5, 0.0), (x0 - 0.85, 0.3), (x0 - 0.85, -0.3)):
            v.lift_eye((at[0], at[1], roof), Z, Y, 0.03)
    for s in (LEFT, RIGHT):
        v.mark("SOV", (x0 - 0.70, s * 0.74, zt + 0.38), Vector((-0.42, s * 0.84, 0.34)), 0.15, part="turret")

    # ---- the gun ------------------------------------------------------------------------------------------
    tx, tz = x0 + 0.92, zt + 0.44
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-5.0, 28.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx + 0.02, 0, tz), Y, [(0.36, 0.0), (0.36, 0.22), (0.30, 0.27), (-0.30, 0.27), (-0.36, 0.22), (-0.36, 0.0)], "upper", v.n(36, 12))     # the mantlet, a drum across her face
    v.lathe((tx + 0.18, 0, tz), X, [(0.66, 0.0), (0.66, 0.115), (0.58, 0.14), (0.0, 0.18), (0.0, 0.0)], "upper", v.n(24, 8))                        # the recoil gear's housing
    v.slab((tx + 0.22, -0.15, tz + 0.08), (tx + 0.80, 0.15, tz + 0.20), "upper", 0.03, segs=2)
    v.mg((tx + 0.24, -0.26, tz - 0.02), X, 0.16)
    v.socket("coax", (tx + 0.42, -0.26, tz - 0.02), X)
    v.part("barrel", "gun", (tx + 0.70, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.39], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.70, 0, tz), 1.50, 0.0762, r0=0.086, r1=0.063, brake=None, sleeve=None)
    v.socket("muzzle", (tx + 2.22, 0, tz), X)
    v.m.info["bore"] = 0.0762
    v.m.info["brake"] = False

    v.socket("fire.2", (x0 - 0.40, 0.36, roof + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.turret", (x0 + 0.3, 0.86, zt + 0.4), Y, part="turret")
    v.socket("case", (x0 - 0.9, 0.0, zt + 0.5), -X, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.34, kick=0.9, gun_low=-5.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver"],
                 traverse=24.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.8, 3.0, 0.42, 170.0))
    return v


def t34_85(fine=True, tag="SOV"):
    """T-34-85 of 1944: the same hull; a big cast turret for three men with the commander's cupola on its left, the
    85 mm gun; 8.15 m over the gun, 2.72 m high."""
    v = Works("T34-85", "T-34-85", "tank", tag, fine)
    h = t34_hull(v, fine)
    zr = h["zr"]
    x0, zt = 0.62, zr
    v.ring_well((x0, 0.0, zt), 0.82)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    th = 0.82
    v.cast([{"z": zt + 0.02, "xc": x0 - 0.10, "af": 1.12, "ar": 1.22, "b": 1.02, "n": 2.7},
            {"z": zt + 0.36, "xc": x0 - 0.10, "af": 1.14, "ar": 1.28, "b": 1.01, "n": 2.7},
            {"z": zt + th - 0.07, "xc": x0 - 0.10, "af": 1.00, "ar": 1.18, "b": 0.88, "n": 2.7}], "upper", sides=v.n(48, 14), top=0.07)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.86), (0.0, 0.88), (0.0, 0.0)], "shade", v.n(36, 12))
    roof = zt + th
    cup = v.cupola((x0 - 0.52, 0.44, roof - 0.01), 0.34, 0.25, slits=5, start=0.3)
    v.hatch_round("hatch_commander", "turret", cup, 0.25, Z, X, thick=0.03, opens=95.0, split=True)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_round("hatch_loader", "turret", (x0 - 0.50, -0.42, roof), 0.25, Z, X, thick=0.03, opens=95.0)
    v.use("turret")
    v.periscope((x0 + 0.40, 0.42, roof - 0.01), X, (0.09, 0.13, 0.11))
    v.periscope((x0 + 0.30, -0.44, roof - 0.01), X, (0.09, 0.13, 0.11))
    for y in (-0.2, 0.2):
        v.vent((x0 - 1.02, y, roof - 0.015), 0.14, 0.08)
    if fine:
        for s in (LEFT, RIGHT):
            n = Vector((0, s * 0.96, 0.28)).normalized()
            v.handle(Vector((x0 + 0.20, s * 0.97, zt + 0.46)), Vector((x0 - 0.50, s * 0.98, zt + 0.46)), n, 0.011, 0.06)
        for at in ((x0 + 0.62, 0.36), (x0 + 0.62, -0.36), (x0 - 0.98, 0.52), (x0 - 0.98, -0.52)):
            v.lift_eye((at[0], at[1], roof - 0.03), Z, X, 0.03)
    for s in (LEFT, RIGHT):
        v.mark("SOV", (x0 - 0.75, s * 0.97, zt + 0.42), Vector((-0.2, s * 0.95, 0.25)), 0.16, part="turret")

    # ---- the gun ------------------------------------------------------------------------------------------
    tx, tz = x0 + 0.98, zt + 0.44
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-5.0, 22.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx + 0.02, 0, tz), Y, [(0.44, 0.0), (0.44, 0.24), (0.38, 0.29), (-0.38, 0.29), (-0.44, 0.24), (-0.44, 0.0)], "upper", v.n(36, 12))     # the mantlet
    v.lathe((tx + 0.20, 0, tz), X, [(0.56, 0.0), (0.56, 0.15), (0.48, 0.18), (0.0, 0.26), (0.0, 0.0)], "upper", v.n(26, 8))                         # its collar
    if fine:
        v.bolts((tx + 0.32, 0, tz), X, 0.215, 8, r=0.012)
    v.mg((tx + 0.26, -0.30, tz), X, 0.14)
    v.socket("coax", (tx + 0.42, -0.30, tz), X)
    v.part("barrel", "gun", (tx + 0.72, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.33], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.72, 0, tz), 2.70, 0.085, r0=0.105, r1=0.068, brake=None, steps=((0.22, None),), sleeve=None)
    v.socket("muzzle", (tx + 0.72 + 2.72, 0, tz), X)
    v.m.info["bore"] = 0.085
    v.m.info["brake"] = False

    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.turret", (x0 + 0.2, 0.98, zt + 0.4), Y, part="turret")
    v.socket("case", (x0 - 1.1, 0.0, zt + 0.5), -X, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.30, kick=1.0, gun_low=-5.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver"],
                 traverse=24.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.9, 3.1, 0.42, 170.0))
    return v
