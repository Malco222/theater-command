"""French tanks.

The Char B1 bis: the heavy tank of 1940. What tells her from across a field: the tracks running right round the
tall, narrow hull as on the tanks of the last war, a flat armoured plate filling each side inside the loop (the
radiator's big louvred grille in the left one, the crew's door in the right); the short 75 mm howitzer low in the
nose beside the driver's box; a small one-man cast turret with a 47 mm gun and a domed look-out on top.
Every shape is our own mesh, made from numbers here.
"""

import math

from mathutils import Vector

from motorpool import LEFT, RIGHT, X, Y, Z
from tanks_any import rivets, turn_to
from tanks_ger import coat
from tanks_uk import arc, holed_plate
from works import Works


def char_b1(fine=True, tag="FRA"):
    """Char B1 bis: 6.3 m long, 2.5 m wide, 2.8 m high. The drive at the back, twelve small steel wheels a side
    behind the side plate, the track carried back along the top on rollers under a mudguard the length of her."""
    v = Works("CharB1", "Char B1 bis", "tank", tag, fine)
    # (the E7 surface pass: shade from her shape, dirt of her ground; the art director, 22 Nov 17:55Z)
    v.dice = 0.2
    v.worn_kw = dict(shade=True, shade_deep=0.42, ground="field", dust=0.5, grime=0.5, marks=0.3, gear=0.6, rim=True, cast=lambda p, part: part == "turret")
    v.root(hull_pivot=(0.0, 0.0, 0.9))
    hw, zr, ty, yp = 0.72, 1.90, 0.98, 1.21          # the hull's half-width; its roof; the track's middle; the side plate's face

    # ---- the hull: one tall narrow box between the tracks, a rounded nose, the engine's roof falling to the tail --
    v.extrude([(-3.02, 0.84), (-2.88, 0.54), (2.38, 0.54), (2.72, 0.68), (2.90, 0.90), (2.86, 1.25), (1.95, 1.84), (1.75, zr), (-1.15, zr), (-2.90, 1.60), (-3.04, 1.30)],
              -hw, hw, coat, bevel=0.016, angle=12.0)
    gl = Vector((1.95 - 2.86, 0, 1.84 - 1.25)).normalized()           # up the sloped front plate
    gn = Vector((gl.z, 0, -gl.x))
    dk = Vector((-2.90 + 1.15, 0, 1.60 - zr)).normalized()            # back along the engine's roof
    dn = Vector((dk.z, 0, -dk.x))
    g_at = lambda u, y: Vector((2.86, y, 1.25)) + gl * u + gn * 0.002     # noqa: E731
    d_at = lambda u, y: Vector((-1.15, y, zr)) + dk * u + dn * 0.002      # noqa: E731
    # the driver's box on the left of the nose, its front a door that lifts
    v.hull([(2.66, 0.08, 1.30), (2.66, 0.70, 1.30), (2.62, 0.08, 1.94), (2.62, 0.70, 1.94), (1.62, 0.08, 1.94), (1.62, 0.70, 1.94), (1.62, 0.08, 1.30), (1.62, 0.70, 1.30)],
           coat, bevel=0.016, both=False)
    v.hatch_rect("hatch_driver", "hull", (2.652, 0.39, 1.63), (0.46, 0.46), Vector((1, 0, 0.06)).normalized(), Z, hinge="front", opens=80.0, thick=0.03)
    v.visor((2.685, 0.39, 1.68), X, 0.26, 0.07, 0.03, part="hatch_driver", brow=False)
    v.use("hull")
    v.periscope((2.30, 0.39, 1.94), X, (0.07, 0.15, 0.07))
    v.port(Vector((2.2, 0.702, 1.70)), Y, 0.05)
    if fine:
        rivets(v, (2.60, 0.705, 1.36), (1.70, 0.705, 1.36), Y, 0.12)
        rivets(v, (2.58, 0.705, 1.90), (1.70, 0.705, 1.90), Y, 0.12)
        rivets(v, g_at(0.10, -0.66), g_at(1.0, -0.66), gn, 0.12)
        rivets(v, g_at(0.06, -0.62), g_at(0.06, 0.02), gn, 0.12)
    # the 75 mm howitzer low on the right of the nose: she is laid by turning the whole tank
    gy, gz = -0.36, 1.07
    v.box((0.03, 0.60, 0.50), (2.885, gy, gz), "upper", 0.008, turn=turn_to(Vector((0.11, 0, 1)), X))
    v.lathe((2.84, gy, gz), Y, [(0.21, 0.0), (0.21, 0.17), (0.17, 0.21), (-0.17, 0.21), (-0.21, 0.17), (-0.21, 0.0)], "upper", v.n(30, 10))
    v.barrel((2.98, gy, gz), 0.36, 0.075, r0=0.105, r1=0.09, brake=None, sleeve=None)
    v.socket("hull_gun", (3.36, gy, gz), X, note="the 75 mm howitzer's muzzle")
    if fine:
        for dy in (-0.26, 0.26):
            for dz in (-0.21, 0.0, 0.21):
                v.bolt((2.905 - dz * 0.11, gy + dy, gz + dz), X, 0.014, 0.01)
    v.headlamp((2.80, 0.30, 1.26), X, 0.07, stalk=0.06, cover=True)
    for s in (LEFT, RIGHT):
        v.hook((2.86, s * 0.45, 0.80), Vector((0.9, 0, -0.3)).normalized(), 0.1)
    # the tail: a heavy hook for towing, the gearbox's cover
    v.box((0.03, 0.9, 0.34), (-3.045, 0.0, 1.06), "upper", 0.008)
    v.hook((-3.06, 0.0, 0.80), -X, 0.14)
    if fine:
        v.bolts((-3.061, 0.0, 1.06), -X, 0.13, 6, r=0.014)
        v.spare_links((-3.062, -0.32, 1.42), (-3.062, 0.32, 1.42), 3, 0.21, 0.30, -X, style={"kind": "plate", "grouser": 0.016, "cleats": False})

    # ---- the engine's roof: doors with their handles, a grille across the back ------------------------------
    for s in (LEFT, RIGHT):
        v.box((0.80, 0.60, 0.02), d_at(0.52, s * 0.34) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
        if fine:
            v.handle(d_at(0.52, s * 0.56) - X * 0.08 + dn * 0.02, d_at(0.52, s * 0.56) + X * 0.08 + dn * 0.02, dn, 0.007, 0.035, "steel")
            for u in (0.25, 0.80):
                v.hinge(d_at(u - 0.05, s * 0.035) + dn * 0.02, d_at(u + 0.05, s * 0.035) + dn * 0.02, 0.012)
    v.box((0.56, 1.20, 0.02), d_at(1.38, 0.0) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
    v.grille(d_at(1.38, 0.0) + dn * 0.02, (1.12, 0.46), dn, Y, slats=7, deep=0.02)
    v.aerial("aerial", "hull", (-0.95, -0.56, zr), 1.9)

    # ---- the mudguards over the top of each track, a silencer on the back of each ----------------------------
    for s in (LEFT, RIGHT):
        v.slab((-1.85, s * 0.70, 1.555), (2.70, s * 1.25, 1.572), coat, 0.004)
        front = [(2.70 + 0.49 * math.cos(math.radians(a)), 1.08 + 0.49 * math.sin(math.radians(a))) for a in (90.0, 74.0, 58.0, 42.0, 26.0, 12.0)]
        back = [(-1.85, 1.572)] + [(-2.75 + 0.47 * math.cos(math.radians(a)), 0.86 + 0.47 * math.sin(math.radians(a))) for a in (100.0, 118.0, 136.0, 154.0)]
        for pts in (front, back):
            for (xa, za), (xb, zb) in zip(pts, pts[1:]):
                v.plate([(xa, s * 0.72, za), (xa, s * 1.25, za), (xb, s * 1.25, zb), (xb, s * 0.72, zb)][::int(s)], 0.009, "upper", 0.002)
        if fine:
            for x in (-1.2, 0.0, 1.2, 2.3):                      # the stays under the guard
                v.slab((x - 0.014, s * 0.72, 1.50), (x + 0.014, s * 1.22, 1.555), "shade")
        v.silencer((-0.55, s * 0.98, 1.675), (-1.65, s * 0.98, 1.675), 0.095, bands=3, tail=((-1.65, s * 0.98, 1.675), (-1.0, 0, 0.12)))
        v.tube([(-0.30, s * 0.72, 1.74), (-0.30, s * 0.90, 1.72), (-0.42, s * 0.98, 1.69), (-0.55, s * 0.98, 1.675)], 0.04, "rust", v.n(10, 5))
        for x in (-0.8, -1.4):
            v.slab((x - 0.02, s * 0.88, 1.572), (x + 0.02, s * 1.08, 1.60), "shade")
        v.socket("exhaust." + ("1" if s > 0 else "2"), (-1.86, s * 0.98, 1.70), (-1.0, 0, 0.2))
    v.shovel((1.2, 1.0, 1.594), 0.0, 0.95)
    v.crowbar((1.0, -1.02, 1.59), 0.0, 1.3)
    v.jack((0.2, 1.0, 1.572), 0.0)

    # ---- the side plates inside the loop of each track --------------------------------------------------------
    n = v.n(10, 4)
    outline = [(2.21, 0.27)] + arc(2.70, 1.08, 0.30, 0.30, -34.8, 90.0, n) + [(-1.85, 1.34)] + arc(-2.75, 0.86, 0.26, 0.26, 100.0, 224.0, n) + [(-2.27, 0.27)]
    for s in (LEFT, RIGHT):
        holed_plate(v, outline, [], s * yp, 0.04, "upper", bevel=0.006)
        for (hx, hz, hr) in ((2.70, 1.08, 0.15), (-2.75, 0.86, 0.14)):       # a bolted cap over each end wheel's hub
            v.lathe((hx, s * yp, hz), Y * s, [(0.0, hr), (0.02, hr), (0.045, hr * 0.6), (0.05, 0.0)][::-1], "upper", v.n(20, 8))
            if fine:
                v.bolts((hx, s * (yp + 0.021), hz), Y * s, hr * 0.8, 6, r=0.012)
        if fine:
            rivets(v, (2.55, s * (yp + 0.001), 1.30), (-1.80, s * (yp + 0.001), 1.28), Y * s, 0.145)
            rivets(v, (2.15, s * (yp + 0.001), 0.33), (-2.20, s * (yp + 0.001), 0.33), Y * s, 0.145)
            for x in (-1.9, 1.95):
                rivets(v, (x, s * (yp + 0.001), 0.42), (x, s * (yp + 0.001), 1.2), Y * s, 0.13)
    # the radiator's grille in the left plate; the crew's door in the right one
    v.box((1.36, 0.016, 0.78), (-0.95, yp + 0.008, 0.86), "upper", 0.005)
    v.grille((-0.95, yp + 0.016, 0.86), (1.28, 0.70), Y, X, slats=9, deep=0.03)
    v.hatch_rect("hatch_side", "hull", (0.62, -yp, 0.86), (0.62, 0.78), -Y, X, hinge="front", opens=110.0, thick=0.02, well=False)
    v.use("hull")

    # ---- the running gear ---------------------------------------------------------------------------------------
    r_w, thick = 0.11, 0.035
    xs = [-2.05 + 4.10 * i / 11 for i in range(12)]
    v.running(xs, r_w, ty, (-2.75, 0.86, 0.36), (2.70, 1.08, 0.40), rollers=[(1.8, 1.41, 0.07), (0.6, 1.40, 0.07), (-0.6, 1.39, 0.07), (-1.8, 1.38, 0.07)],
              drive="rear", wheel_z=r_w + thick,
              wheel=dict(w=0.07, tyre=0.0, gap=0.08, bolts=0, sides=20, hub=0.45, axle=0.1, kind="steel"),
              sprocket_kw=dict(teeth=13, wide=0.40, tooth=0.04, holes=0, bolts=8, sides=28, hub=0.4),
              idler_kw=dict(wide=0.40, kind="spoke", spokes=8),
              roller_kw=dict(wide=0.2, tyre=0.0, reach=0.2),
              track=dict(pitch=0.21, wide=0.50, thick=thick, sag=0.006, style={"kind": "plate", "horn": "centre", "horn_high": 0.06, "grouser": 0.02, "cleats": False}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):
        yb = s * (ty + 0.115)
        for i in range(0, 12, 2):                                # the beam joining each pair of wheels, its hanger up behind the plate
            c = (xs[i] + xs[i + 1]) / 2
            v.slab((xs[i] - 0.03, yb, 0.10), (xs[i + 1] + 0.03, yb + s * 0.022, 0.19), "shade", 0.006)
            v.slab((c - 0.05, yb, 0.14), (c + 0.05, yb + s * 0.04, 0.50), "shade", 0.006)
    v.use("hull")

    # ---- the turret: a small casting for one man, a domed look-out on its roof, a door in its back ------------
    x0, zt, th = 0.80, zr, 0.50
    v.ring_well((x0, 0.0, zt), 0.56)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    v.cast([{"z": zt + 0.02, "xc": x0, "af": 0.62, "ar": 0.64, "b": 0.60, "n": 2.3},
            {"z": zt + 0.24, "xc": x0, "af": 0.61, "ar": 0.63, "b": 0.59, "n": 2.35},
            {"z": zt + th - 0.06, "xc": x0, "af": 0.55, "ar": 0.58, "b": 0.53, "n": 2.4}], "upper", sides=v.n(44, 14), top=0.06)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.60), (0.0, 0.62), (0.0, 0.0)], "shade", v.n(32, 12))
    roof = zt + th
    cup = v.cupola((x0 - 0.12, 0.10, roof - 0.01), 0.25, 0.20, slits=3, start=math.radians(60))
    v.ball(cup - Z * 0.012, 0.215, "upper", v.n(24, 10), v.n(8, 4), dome=True, squash=0.5)
    v.hatch_rect("hatch_commander", "turret", (x0 - 0.625, 0.0, zt + 0.25), (0.34, 0.40), Vector((-1, 0, 0.1)).normalized(), Z, hinge="back", opens=95.0, thick=0.03, well=False)
    v.socket("commander", (x0 - 0.75, 0.0, zt + 0.25), -X, part="turret", note="the commander sits out on his door at the back of the turret")
    v.use("turret")
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 - 0.05, s * 0.59, zt + 0.30)), Vector((0, s, 0.15)), 0.045)
            v.lift_eye((x0 + 0.30, s * 0.30, roof - 0.02), Z, X, 0.026)
        v.periscope((x0 + 0.34, -0.02, roof - 0.01), X, (0.06, 0.11, 0.06))
    for s in (LEFT, RIGHT):
        v.mark("FRA", (x0 - 0.36, s * 0.55, zt + 0.27), Vector((-0.45, s, 0.12)), 0.12, part="turret")

    # ---- the turret's gun: a 47 mm in a boxed shield, a machine gun on its right -------------------------------
    tx, tz = x0 + 0.44, zt + 0.25
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-18.0, 18.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx, 0, tz), Y, [(0.27, 0.0), (0.27, 0.19), (0.23, 0.225), (-0.23, 0.225), (-0.27, 0.19), (-0.27, 0.0)], "upper", v.n(30, 10))
    v.lathe((tx + 0.18, 0.05, tz), X, [(0.22, 0.0), (0.22, 0.07), (0.12, 0.085), (0.0, 0.1), (0.0, 0.0)], "upper", v.n(20, 8))
    v.mg((tx + 0.16, -0.15, tz), X, 0.30)
    v.socket("coax", (tx + 0.48, -0.15, tz), X)
    v.part("barrel", "gun", (tx + 0.34, 0.05, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.2], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.34, 0.05, tz), 1.16, 0.047, r0=0.062, r1=0.042, brake=None, sleeve=None)
    v.socket("muzzle", (tx + 1.52, 0.05, tz), X)
    v.m.info["bore"] = 0.047
    v.m.info["brake"] = False

    # ---- sockets for what happens to her ----------------------------------------------------------------------
    v.socket("engine", d_at(0.6, 0.0) + dn * 0.05, Z, part="hull", note="engine roof: smoke and the first fire")
    v.socket("fire.1", d_at(1.38, 0.0) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (x0 - 0.7, 0.0, zt + 0.3), -X, part="turret", note="out of the turret's door")
    v.socket("fire.3", (2.72, 0.39, 1.63), X, part="hull", note="out of the driver's door")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(0.5, -0.3), gn, part="hull")
    v.socket("hit.left", (-0.95, yp, 0.86), Y, part="hull", note="the radiator's grille: her weak place")
    v.socket("hit.right", (0.2, -yp, 0.9), -Y, part="hull")
    v.socket("hit.rear", (-3.04, 0.0, 1.1), -X, part="hull")
    v.socket("hit.turret", (x0, 0.6, zt + 0.25), Y, part="turret")
    v.socket("case", (x0 - 0.2, -0.6, zt + 0.3), -Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.18, kick=0.4, gun_low=-12.0, hatches=["hatch_commander", "hatch_side", "hatch_driver"],
                 traverse=12.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.9, 2.3, 0.36, 164.0))
    v.name_place((1.7, 1.3), long=0.9, high=0.13)                     # her name (b477): every B1 carried one, on the hull's side near the front
    return v
