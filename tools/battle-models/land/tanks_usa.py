"""The United States' tanks: the M4 Sherman with the 75 mm gun (the line tank from 1942; Britain and the others who
were lent her show the same model in their own paint).

Modelled to the real type's measurements and layout from the published dimensions; every shape is our own mesh, made
from numbers here. She stands tall: the drive is at the front under a rounded cast nose, the upper hull is a welded
box as wide as her tracks with the two drivers' hoods standing out of its sloped front, six small wheels a side run
in three sprung pairs with a return roller behind each, and the turret is one rounded casting.
"""

import math

from mathutils import Vector

from motorpool import LEFT, RIGHT, SIDES, X, Y, Z
from tanks_ger import coat
from works import Works


def m4_tub(v, hw):
    """The M4's lower hull, shared by everything built on it: the tub between the tracks and the rounded cast nose
    that houses the drive, with the final drives and the towing hooks."""
    fine = v.fine
    # ---- the tub between the tracks, ending forward in the rounded cast nose that houses the drive ----
    k = v.n(14, 5)
    nose = [(2.38 + 0.54 * max(0.0, math.cos(a)) ** (2 / 2.3), 0.74 + (0.33 if a > 0 else 0.31) * math.copysign(abs(math.sin(a)) ** (2 / 2.3), math.sin(a)))
            for a in [math.radians(-90.0 + 180.0 * i / k) for i in range(k + 1)]]
    v.extrude([(-2.50, 0.50), (-2.30, 0.43)] + nose + [(-2.62, 1.07), (-2.62, 0.62)], -hw, hw, coat, bevel=0.012, angle=20.0)
    for s in (LEFT, RIGHT):
        # the final drive's housing out to each sprocket
        v.lathe((2.52, s * (hw - 0.02), 0.67), Y * s, [(0.12, 0.0), (0.12, 0.20), (0.06, 0.28), (0.0, 0.30), (0.0, 0.0)], "upper", v.n(26, 8))
        v.hook(Vector((2.84, s * 0.42, 0.60)), X, 0.1)
        v.hook(Vector((-2.63, s * 0.50, 0.58)), -X, 0.1)
    if fine:
        # the nose is three castings bolted together: a flange stands proud down each join, bolts through it from both sides
        mid = Vector((2.36, 0, 0.74))
        ring = [Vector((x, 0, z)) for x, z in nose[1:-1]]
        outer = [q + (q - mid).normalized() * 0.04 for q in ring]
        inner = [q - (q - mid).normalized() * 0.02 for q in ring]
        for yc in (-0.37, 0.37):
            v.extrude([(q.x, q.z) for q in outer] + [(q.x, q.z) for q in reversed(inner)], yc - 0.028, yc + 0.028, "upper", bevel=0.005)
            for q in outer[1:-1:2] + ([outer[-2]] if len(outer) % 2 else []):
                c = q - (q - mid).normalized() * 0.019
                for e in (-1.0, 1.0):
                    v.bolt((c.x, yc + e * 0.028, c.z), Y * e, 0.013, 0.01)
        # the lip the nose is bolted to the hull's front plate by, and its row of bolts
        v.slab((2.34, -0.78, 1.048), (2.46, 0.78, 1.068), "upper", 0.004)
        for i in range(19):
            v.bolt((2.40, -0.74 + 1.48 * i / 18, 1.068), Vector((0.1, 0, 1)), 0.015, 0.012)
        # a towing shackle hanging in each front hook's eye, and the step welded under the nose
        for s in (LEFT, RIGHT):
            v.tube([(2.90, s * 0.36, 0.60), (2.97, s * 0.36, 0.55), (2.99, s * 0.42, 0.50), (2.97, s * 0.48, 0.55), (2.90, s * 0.48, 0.60)], 0.016, "steel", 6)
            v.lathe((2.56, s * (hw + 0.10), 0.67), Y * s, [(0.0, 0.0), (0.0, 0.17), (0.022, 0.17), (0.03, 0.15), (0.03, 0.0)], "upper", v.n(20, 8))       # the final drive's cover plate
            v.bolts((2.56, s * (hw + 0.13), 0.67), Y * s, 0.125, 10, r=0.012, high=0.009)


def m4_gear(v, hw):
    """The M4's running gear, as the vertical volute spring bogie was built: three brackets a side bolted to the hull,
    each a tall box holding two springs with a skid on its top for the track; under it two arms forked round a wheel
    apiece and hinged on the bracket's foot, a lever from each arm up to the spring's seat; a return roller on an arm
    of its own behind each bracket; the drive at the front. Every wheel's arm is a part (`arm_<l><n>`): it swings on
    the bracket, so a game can work each wheel over the ground (the file's `suspension` table says how far)."""
    fine = v.fine
    ty, r_w, up = 1.054, 0.254, 0.04               # (`up`: the wheels stand on the track's rubber, not on the ground)
    pairs = [-1.45, 0.0, 1.45]
    xs = [b + d for b in pairs for d in (-0.36, 0.36)]
    rollers = [(b - 0.31, 0.835 + up, 0.095) for b in pairs]
    zp, reach = 0.37 + up, 0.31                    # where the arms hinge, and how far ahead of (or behind) it a wheel's axle is
    table = []
    for letter, s in SIDES:
        for i, x in enumerate(xs):
            b = pairs[i // 2]
            e = 1.0 if x > b else -1.0
            v.part(f"arm_{letter}{i + 1}", "vehicle", (b + e * 0.05, s * ty, zp), axis=(0, -e, 0), limits=[-7.0, 9.0], kind="turn",
                   note="a wheel's sprung arm: + lifts its wheel (about 5.4 mm a degree)")
            table.append({"part": f"arm_{letter}{i + 1}", "wheel": f"wheel_{letter}{i + 1}", "side": "left" if s > 0 else "right", "x": round(x, 3),
                          "metres_per_degree": round(reach * math.pi / 180.0, 5), "limits": [-7.0, 9.0]})
    v.m.info.setdefault("file", {})["suspension"] = {
        "kind": "arms", "wheels": table,
        "how": "each road wheel hangs on an arm of its own that swings on its bracket: turn the arm by (how far the ground under that wheel stands above "
               "the level she rests on) / metres_per_degree, inside its limits, and the wheel rides up with it; the two arms of a pair share one spring, "
               "so a pair may also be rocked (one arm up, its partner down by as much). Leave them at 0 on level ground."}
    v.running(xs, r_w, ty, (2.52, 0.67, 0.315), (-2.42, 0.62, 0.28), rollers=rollers, wheel_z=r_w + up,
              wheel=dict(w=0.22, tyre=0.035, double=False, kind="dish", bolts=8, sides=30, hub=0.36, axle=0.02, ribs=6),
              sprocket_kw=dict(teeth=13, wide=0.40, tooth=0.035, holes=0, bolts=8, sides=30, hub=0.46),
              idler_kw=dict(wide=0.24, kind="spoke", spokes=6),
              roller_kw=dict(wide=0.2, tyre=0.012, reach=0.1),
              track=dict(pitch=0.152, wide=0.42, thick=0.045, sag=0.008, style={"kind": "block", "chevron": True, "horn": "ends", "horn_high": 0.07, "grouser": 0.012}),
              wheel_parent=lambda letter, i: f"arm_{letter}{i + 1}")
    v.use("vehicle")
    for letter, s in SIDES:
        yo, yi = s * (ty + 0.15), s * (ty - 0.15)
        lo, hi = min(yo, yi), max(yo, yi)
        for k, b in enumerate(pairs):
            v.use("vehicle")
            # the bracket: a narrow foot between the two wheels, a broad box over them for the springs
            v.extrude([(b + x, z + up) for x, z in [(-0.085, 0.27), (0.085, 0.27), (0.10, 0.44), (0.215, 0.565), (0.215, 0.80), (0.16, 0.855), (-0.16, 0.855),
                                                    (-0.215, 0.80), (-0.215, 0.565), (-0.10, 0.44)]], lo, hi, "upper", bevel=0.014)
            v.slab((b - 0.16, s * (hw - 0.01), 0.58 + up), (b + 0.16, yi, 0.82 + up), "shade", 0.008)             # its foot on the hull's side
            # the skid the track slides over, turned down at each end
            v.extrude([(b + x, z + up) for x, z in [(-0.24, 0.835), (-0.18, 0.888), (0.18, 0.888), (0.25, 0.83), (0.25, 0.81), (0.17, 0.868), (-0.17, 0.868), (-0.24, 0.815)]],
                      s * ty - 0.075, s * ty + 0.075, "steel", bevel=0.003)
            if fine:
                # the cover over the springs with its six bolts, the lip the box stands on, a stiffening rib each side
                v.slab((b - 0.15, yo - s * 0.004, 0.60 + up), (b + 0.15, yo + s * 0.014, 0.80 + up), "upper", 0.006)
                for x in (-0.115, 0.0, 0.115):
                    for z in (0.63, 0.77):
                        v.bolt((b + x, yo + s * 0.014, z + up), Y * s, 0.014, 0.01)
                v.slab((b - 0.235, yi, 0.548 + up), (b + 0.235, yo + s * 0.012, 0.578 + up), "upper", 0.006)
                for e in (-1.0, 1.0):
                    v.plate([Vector((b + e * 0.215, yo + s * 0.012, 0.578 + up)), Vector((b + e * 0.215, yo + s * 0.012, 0.80 + up)),
                             Vector((b + e * 0.185, yo + s * 0.012, 0.80 + up)), Vector((b + e * 0.185, yo + s * 0.012, 0.578 + up))][::int(s * e)], 0.012, "upper", 0.003)
            for e in (-1.0, 1.0):
                i = 2 * k + (1 if e > 0 else 0)
                arm = f"arm_{letter}{i + 1}"
                v.use(arm)
                P, H = Vector((b + e * 0.05, 0, zp)), Vector((b + e * 0.36, 0, r_w + up))
                d = (H - P).normalized()
                n = Vector((-d.z, 0, d.x))
                bar = [P + n * 0.06 - d * 0.04, P - n * 0.06 - d * 0.04, H - n * 0.05 + d * 0.03, H + n * 0.05 + d * 0.03]
                # the arm: a forked forging, a cheek each side of the wheel
                for y0, y1 in (((ty + 0.118), (ty + 0.152)), ((ty - 0.152), (ty - 0.118))):
                    a0, a1 = sorted((s * y0, s * y1))
                    v.extrude([(q.x, q.z) for q in bar], a0, a1, "upper", bevel=0.007 if fine else 0.0)
                    if not fine:
                        break
                if fine:
                    # the lever from the arm up to the spring's seat under the box
                    T = Vector((b + e * 0.175, 0, 0.56 + up))
                    L0 = H + Vector((-e * 0.06, 0, 0.055))
                    dl = (T - L0).normalized()
                    nl = Vector((-dl.z, 0, dl.x))
                    a0, a1 = sorted((s * (ty + 0.12), s * (ty + 0.148)))
                    v.extrude([(q.x, q.z) for q in (L0 + nl * 0.032, L0 - nl * 0.032, T - nl * 0.026, T + nl * 0.026)], a0, a1, "upper", bevel=0.006)
                    v.rod((T.x, s * (ty - 0.12), T.z), (T.x, s * (ty + 0.156), T.z), 0.024, "steel", 8)
                    # the pin the arm swings on, through the bracket's foot, and the cap over the wheel's axle with its bolts
                    v.rod((P.x, yi - s * 0.012, P.z), (P.x, yo + s * 0.02, P.z), 0.04, "upper", 10)
                    v.lathe((P.x, yo + s * 0.02, P.z), Y * s, [(0.018, 0.0), (0.018, 0.026), (0.0, 0.03), (0.0, 0.0)], "steel", 6)
                    hub = Vector((H.x, s * (ty + 0.152), H.z))
                    v.lathe(hub, Y * s, [(0.045, 0.0), (0.045, 0.04), (0.03, 0.062), (0.012, 0.078), (0.0, 0.082), (0.0, 0.0)], "upper", v.n(16, 6))
                    v.bolts(hub + Y * s * 0.012, Y * s, 0.066, 6, r=0.009, high=0.008)
                    v.lathe(hub + Y * s * 0.045, Y * s, [(0.012, 0.0), (0.012, 0.012), (0.0, 0.014), (0.0, 0.0)], "steel", 6)       # the grease plug
            v.use("vehicle")
            if fine:
                # the roller's arm, reaching back from the bracket: a cheek each side of the roller, a nut on its axle
                rx, rz = b - 0.31, 0.835 + up
                for y0, y1 in ((ty + 0.108, ty + 0.134), (ty - 0.134, ty - 0.108)):
                    a0, a1 = sorted((s * y0, s * y1))
                    v.extrude([(b - 0.13, 0.70 + up), (b - 0.13, 0.84 + up), (rx - 0.02, rz + 0.055), (rx - 0.06, rz + 0.02), (rx - 0.06, rz - 0.03), (rx - 0.01, rz - 0.05)],
                              a0, a1, "upper", bevel=0.005)
                v.lathe((rx, s * (ty + 0.134), rz), Y * s, [(0.02, 0.0), (0.02, 0.028), (0.0, 0.032), (0.0, 0.0)], "steel", 6)
        v.lathe((-2.42, s * (hw + 0.04), 0.62), Y * s, [(0.10, 0.0), (0.10, 0.09), (0.0, 0.14), (0.0, 0.0)], "upper", v.n(16, 8))
        if fine:
            # the idler's bracket: the arm the crew turn to tighten the track, with its locking nut
            v.extrude([(-2.42, 0.50), (-2.30, 0.52), (-2.12, 0.72), (-2.12, 0.84), (-2.24, 0.84), (-2.46, 0.72)], *sorted((s * (hw + 0.005), s * (hw + 0.05))), "upper", bevel=0.008)
            v.lathe((-2.19, s * (hw + 0.05), 0.77), Y * s, [(0.03, 0.0), (0.03, 0.04), (0.0, 0.045), (0.0, 0.0)], "steel", 6)
    v.use("hull")


def m4_deck(v, zr, caps=True):
    """The M4's engine deck, behind x = -1.60 where it starts to fall to the tail: the armoured air intake across
    her, the two doors, the filler caps (caps: the pair on the roof ahead of the deck too), the lifting eyes."""
    fine = v.fine
    dk = Vector((-2.58 + 1.60, 0, 1.66 - zr)).normalized()            # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))                                     # up out of it
    d_at = lambda u, y: Vector((-1.60, y, zr)) + dk * u + dn * 0.002      # noqa: E731
    # the engine deck: the armoured air intake across it behind the turret, two big doors in the sloped part with their
    # hinges and handles, four filler caps, lifting eyes at the back corners
    v.slab((-1.50, -0.66, zr), (-1.20, 0.66, zr + 0.085), "upper", 0.02, segs=2)
    v.grille((-1.35, 0.0, zr + 0.085), (1.20, 0.20), Z, Y, slats=3, deep=0.02)
    for s in (LEFT, RIGHT):
        c = d_at(0.52, s * 0.37)
        v.box((0.84, 0.68, 0.03), c + dn * 0.014, "upper", 0.008, turn=_turn(dk, dn))
        if fine:
            v.handle(d_at(0.52, s * 0.56) + dn * 0.03 - dk * 0.09, d_at(0.52, s * 0.56) + dn * 0.03 + dk * 0.09, dn, 0.009, 0.045, "steel")
            for u in (0.24, 0.80):
                v.hinge(d_at(u, s * 0.022) - dk * 0.06 + dn * 0.03, d_at(u, s * 0.022) + dk * 0.06 + dn * 0.03, 0.016)
            for u in (0.14, 0.52, 0.90):
                v.bolt(d_at(u, s * 0.67) + dn * 0.03, dn, 0.014, 0.01)
            v.lift_eye(d_at(0.86, s * 1.14), dn, X, 0.032)
        if caps:
            v.lathe((-0.55, s * 1.08, zr), Z, [(0.035, 0.0), (0.035, 0.06), (0.012, 0.075), (0.0, 0.075)], "upper", v.n(14, 6))
        v.lathe(d_at(0.30, s * 1.08), dn, [(0.035, 0.0), (0.035, 0.06), (0.012, 0.075), (0.0, 0.075)], "upper", v.n(14, 6))


def m4_tail(v):
    """The M4's tail under the deck's overhang: the engine's doors, an air cleaner each side, the two exhausts."""
    fine = v.fine
    rb = Vector((-2.58 + 2.76, 0, 1.66 - 1.18)).normalized()          # up the tail plate
    rn = Vector((-rb.z, 0, rb.x))
    r_at = lambda u, y: Vector((-2.76, y, 1.18)) + rb * u + rn * 0.002    # noqa: E731
    # the tail: the engine's doors in the lower plate, an air cleaner each side under the overhang, two exhausts between
    for s in (LEFT, RIGHT):
        v.slab((-2.645, s * 0.03, 0.60), (-2.62, s * 0.52, 1.0), "upper", 0.006)
        if fine:
            v.handle((-2.645, s * 0.40, 0.76), (-2.645, s * 0.40, 0.88), -X, 0.008, 0.04, "steel")
            for z in (0.68, 0.92):
                v.hinge((-2.648, s * 0.535, z - 0.04), (-2.648, s * 0.535, z + 0.04), 0.014)
        v.silencer((-2.73, s * 0.76, 0.96), (-2.73, s * 0.40, 0.96), 0.085, paint="upper", bands=2)
    for i, y in enumerate((0.16, -0.16)):
        v.tube([(-2.60, y, 1.03), (-2.70, y, 1.01), (-2.80, y, 0.95)], 0.04, "rust", v.n(10, 5))
        v.socket(f"exhaust.{i + 1}", (-2.82, y, 0.94), (-1, 0, -0.45))
    if fine:
        for y in (-0.9, 0.9):
            v.lift_eye(r_at(0.40, y), rn, Y, 0.03)


def sherman(fine=True, tag="USA"):
    """M4 Sherman, welded hull, 75 mm gun: 5.84 m long, 2.62 m wide, 2.74 m high, the muzzle no farther forward than
    her nose."""
    v = Works("Sherman", "M4 Sherman", "tank", tag, fine)
    v.dice = 0.16
    # (her turret, gun shield and nose are castings; her bedrolls and packs are olive canvas, darker than a truck's tilt)
    v.worn_kw = dict(cast=lambda p, part: part in ("turret", "gun") or (part == "hull" and p.x > 2.3 and p.z < 1.1))
    v.m.paints["canvas"], v.m.paints["cloth"] = "66684A", "5C5E44"
    v.root(hull_pivot=(0.0, 0.0, 0.85))
    hw, sw, zs, zr = 0.80, 1.31, 1.06, 1.74        # half-width between the tracks and over them; the sponson's floor; the roof

    m4_tub(v, hw)

    # ---- the upper hull: a welded box the full width of her, its front plate sloped back --------------------
    v.hull([(2.47, sw, zs), (1.37, sw, zr), (-1.60, sw, zr), (-2.58, sw, 1.66), (-2.76, sw, 1.18), (-2.76, sw, zs)], coat, bevel=0.014)
    gl = Vector((1.37 - 2.47, 0, zr - zs)).normalized()               # up the front plate
    gn = Vector((gl.z, 0, -gl.x))                                     # out of it
    rb = Vector((-2.58 + 2.76, 0, 1.66 - 1.18)).normalized()          # up the tail plate
    rn = Vector((-rb.z, 0, rb.x))
    dk = Vector((-2.58 + 1.60, 0, 1.66 - zr)).normalized()            # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))                                     # up out of it
    g_at = lambda u, y: Vector((2.47, y, zs)) + gl * u + gn * 0.002       # noqa: E731  (a place on the front plate, u metres up it)
    r_at = lambda u, y: Vector((-2.76, y, 1.18)) + rb * u + rn * 0.002    # noqa: E731  (a place on the tail plate)
    d_at = lambda u, y: Vector((-1.60, y, zr)) + dk * u + dn * 0.002      # noqa: E731  (a place on the engine deck, u metres back)

    # the two hoods standing out of the front plate, a lid on each, a slot to see through, the bow gun under the right one
    hood_top = zr + 0.07
    for s, lid in ((LEFT, "hatch_driver"), (RIGHT, "hatch_radio")):
        yc = s * 0.53
        v.hull([(1.92, yc + e * 0.30, 1.40) for e in (1, -1)] + [(1.85, yc + e * 0.29, hood_top) for e in (1, -1)]
               + [(1.02, yc + e * 0.29, hood_top) for e in (1, -1)] + [(1.02, yc + e * 0.30, zr - 0.03) for e in (1, -1)], coat, bevel=0.022, both=False, segs=3)
        v.hatch_rect(lid, "hull", (1.40, yc, hood_top), (0.58, 0.46), Z, X, hinge="left" if s > 0 else "right", opens=100.0, thick=0.03, lip=0.012)
        v.periscope((1.50, yc, hood_top + 0.03), X, (0.07, 0.15, 0.07), part=lid)
        v.use("hull")
        if fine:
            v.visor((1.905, yc, 1.62), Vector((1, 0, 0.12)).normalized(), 0.26, 0.085, 0.03, brow=True)
        v.headlamp(g_at(1.00, s * 1.02) + gn * 0.07, X, 0.062, stalk=0.07)
        v.lamp_guard(g_at(1.00, s * 1.02) + gn * 0.07 + Z * 0.07, 0.062, g_at(1.00, s * 1.02), gn, Y, gl)      # the brush guard: two hoops of bar crossing over the lamp
        if fine:
            v.lift_eye(g_at(1.17, s * 1.16), gn, Y, 0.035)
        # the track guards: a short flap ahead over each sprocket, a stub behind
        v.plate([(2.46, s * 0.84, zs + 0.004), (2.46, s * 1.31, zs + 0.004), (2.92, s * 1.31, zs - 0.02), (2.92, s * 0.84, zs - 0.02)][::int(s)], 0.008, "upper", 0.003)
        v.plate([(2.92, s * 0.84, zs - 0.02), (2.92, s * 1.31, zs - 0.02), (3.00, s * 1.31, zs - 0.10), (3.00, s * 0.84, zs - 0.10)][::int(s)], 0.008, "upper", 0.003)
        v.plate([(-2.94, s * 0.84, zs - 0.07), (-2.94, s * 1.31, zs - 0.07), (-2.76, s * 1.31, zs + 0.004), (-2.76, s * 0.84, zs + 0.004)][::int(s)], 0.008, "upper", 0.003)
    v.use("hull")
    v.lathe(g_at(0.50, -0.53) - gn * 0.02, gn, [(0.13, 0.0), (0.13, 0.10), (0.08, 0.17), (0.0, 0.21), (0.0, 0.0)], "upper", v.n(24, 8))
    v.mg_ball(g_at(0.50, -0.53) + gn * 0.11, Vector((0.92, 0, 0.12)).normalized(), 0.085, socket="hull_mg", length=0.40, ring=False)
    v.vent((1.22, 0.0, zr), 0.12, 0.05)
    if fine:
        v.weld(g_at(0.03, -1.28), g_at(0.03, 1.28), gn)
        v.weld(Vector((1.37, -1.28, zr + 0.002)), Vector((1.37, -0.84, zr + 0.002)), Z)
        v.weld(Vector((1.37, 0.84, zr + 0.002)), Vector((1.37, 1.28, zr + 0.002)), Z)

    m4_deck(v, zr)
    # along the sides: the pioneer tools on the left, the tow cable on the right, a star each side
    if fine:
        v.shovel((-1.55, sw + 0.03, 1.52), 0.0, 0.95, roll=90.0)
        v.axe((-1.50, sw + 0.03, 1.30), 180.0, 0.8, roll=90.0)
        v.crowbar((-1.9, -sw - 0.02, 1.56), 0.0, 1.3)
        v.cable([(2.05, -sw - 0.025, 1.24), (1.2, -sw - 0.025, 1.12), (-0.6, -sw - 0.025, 1.105), (-1.9, -sw - 0.025, 1.20)], 0.014)
        for x in (1.6, 0.3, -1.2):                        # the clips the cable lies in
            v.slab((x - 0.02, -sw - 0.045, 1.07), (x + 0.02, -sw, 1.16 if x < 1.0 else 1.21), "upper", 0.003)
        # the grab rails the riders hold, on each side of the hull's top edge at the back
        for s in (LEFT, RIGHT):
            v.handle(Vector((-0.75, s * sw, 1.66)), Vector((-1.45, s * sw, 1.66)), Y * s, 0.011, 0.05)
        # the plates welded on over her ammunition racks in 1943 (one a side, behind the star), a bead of weld round each
        for s in (LEFT, RIGHT):
            a, b_, z0, z1, t = -1.02, -0.22, 1.17, 1.62, 0.026
            v.slab((a, s * sw, z0), (b_, s * (sw + t), z1), "upper", 0.004)
            yy = s * (sw + 0.004)
            for p, q in (((a, yy, z0), (b_, yy, z0)), ((b_, yy, z0), (b_, yy, z1)), ((b_, yy, z1), (a, yy, z1)), ((a, yy, z1), (a, yy, z0))):
                v.weld(p, q, Y * s, r=0.009)
        # the hull's own seams: the front plate to each side, each side to the roof, the tail plate to each side
        for s in (LEFT, RIGHT):
            e = s * (sw - 0.006)
            v.weld((2.455, e, zs + 0.012), (1.385, e, zr - 0.006), (gn + Y * s).normalized(), r=0.009)
            v.weld((1.36, e, zr + 0.001), (-1.59, e, zr + 0.001), (Z + Y * s).normalized(), r=0.008)
            v.weld((-1.61, e, zr), (-2.57, e, 1.665), (Z + Y * s).normalized(), r=0.008)
            v.weld((-2.585, e, 1.655), (-2.755, e, 1.19), (rn + Y * s).normalized(), r=0.009)
            v.weld((2.44, s * (sw + 0.001), zs + 0.004), (-2.74, s * (sw + 0.001), zs + 0.004), Y * s, r=0.007)
        v.weld(Vector((1.372, -0.80, zr + 0.002)), Vector((1.372, 0.80, zr + 0.002)), (Z + gn).normalized(), r=0.008)
        # spare track links across the front plate under the hoods (the crews hung them there for the armour as much as the spares)
        v.spare_links(g_at(0.50, -0.20), g_at(0.50, 1.02), 8, 0.152, 0.42, gn, thick=0.04,
                      style={"kind": "block", "chevron": True, "horn": None, "grouser": 0.012, "connector": "rust"})
        # the barrel's travel lock, folded down on the front plate; the siren under its guard by the left lamp
        for s in (LEFT, RIGHT):
            v.tube([g_at(0.84, s * 0.13), g_at(0.84, s * 0.13) + gn * 0.035, g_at(1.02, s * 0.05) + gn * 0.035], 0.014, "upper", 6)
        v.rod(g_at(1.02, -0.07) + gn * 0.035, g_at(1.02, 0.07) + gn * 0.035, 0.022, "upper", 8)
        sir = g_at(0.80, 0.88) + gn * 0.075
        v.lathe(sir - X * 0.07, X, [(0.0, 0.0), (0.0, 0.05), (0.05, 0.062), (0.11, 0.05), (0.13, 0.066), (0.14, 0.066), (0.14, 0.0)], "upper", v.n(16, 8))
        v.rod(sir - Z * 0.02, g_at(0.80, 0.88), 0.014, "upper", 6)
        v.lamp_guard(sir + Z * 0.06, 0.07, g_at(0.80, 0.88), gn, Y, gl)
        # a tail lamp at each upper corner of the tail plate, under a hoop
        for s in (LEFT, RIGHT):
            t0 = r_at(0.40, s * 1.10)
            v.lathe(t0, rn, [(0.0, 0.0), (0.0, 0.034), (0.03, 0.034), (0.036, 0.022), (0.036, 0.0)], "upper", 12)
            v.lathe(t0 + rn * 0.036, rn, [(0.0, 0.0), (0.0, 0.02), (0.004, 0.0)], "red", 10)
            v.tube([t0 + Y * 0.05, t0 + Y * 0.05 + rn * 0.06, t0 - Y * 0.05 + rn * 0.06, t0 - Y * 0.05], 0.006, "upper", 5)
        # what a crew that has lived in her since the beach carries on the engine deck: the rolled tarpaulin across the
        # tail, bedrolls and packs on the doors, two cans of water on the left, a ration box on the right
        v.roll(d_at(0.90, -0.66) + dn * 0.11, d_at(0.90, 0.66) + dn * 0.11, 0.105, "canvas", straps=3)
        v.roll(d_at(0.66, 0.05) + dn * 0.10, d_at(0.66, 0.62) + dn * 0.10, 0.09, "cloth", straps=2)
        v.box((0.36, 0.30, 0.15), d_at(0.60, -0.36) + dn * 0.09, "canvas", 0.045, turn=_turn(dk, dn), segs=3)
        v.box((0.30, 0.26, 0.13), d_at(0.30, -0.42) + dn * 0.08, "cloth", 0.04, turn=_turn(dk, dn), segs=3)
        v.jerrycan(d_at(0.62, 1.12), 90.0)
        v.jerrycan(d_at(0.82, 1.12), 90.0)
        v.crate(d_at(0.66, -1.08), (0.42, 0.26, 0.20), 0.0)
    for s in (LEFT, RIGHT):
        v.mark("USA", (0.25, s * (sw + 0.002), 1.42), Y * s, 0.2, part="hull")

    m4_tail(v)

    m4_gear(v, hw)

    # ---- the turret: one rounded casting, its back standing out over the engine deck ---------------------------
    x0, zt, th = 0.15, zr, 0.80
    v.ring_well((x0, 0.0, zt), 0.84)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    v.cast([{"z": zt + 0.02, "xc": x0 - 0.02, "af": 0.96, "ar": 0.94, "b": 0.93, "n": 2.4},
            {"z": zt + 0.15, "xc": x0 - 0.02, "af": 1.00, "ar": 1.00, "b": 0.96, "n": 2.5},
            {"z": zt + 0.30, "xc": x0 - 0.02, "af": 1.00, "ar": 1.32, "b": 0.97, "n": 2.7},
            {"z": zt + 0.54, "xc": x0 - 0.02, "af": 0.95, "ar": 1.33, "b": 0.93, "n": 2.7},
            {"z": zt + th - 0.09, "xc": x0 - 0.02, "af": 0.85, "ar": 1.26, "b": 0.85, "n": 2.7}], "upper", sides=v.n(52, 14), top=0.09)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.88), (0.0, 0.90), (0.0, 0.0)], "shade", v.n(36, 12))
    roof = zt + th
    # the commander's round hatch in its ring on the right, the loader's lid on the left, the ventilator behind
    cup = v.cupola((x0 - 0.30, -0.44, roof - 0.01), 0.35, 0.085, slits=0, start=0.3)
    v.hatch_round("hatch_commander", "turret", cup, 0.27, Z, Y, thick=0.03, opens=95.0, split=True)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "turret", (x0 - 0.30, 0.46, roof), (0.52, 0.36), Z, X, hinge="left", opens=100.0, thick=0.028)
    v.use("turret")
    v.vent((x0 - 0.98, 0.0, roof - 0.01), 0.14, 0.06)
    v.periscope((x0 + 0.42, -0.42, roof - 0.01), X, (0.08, 0.15, 0.08))
    v.periscope((x0 + 0.36, 0.44, roof - 0.01), X, (0.08, 0.15, 0.08))
    # the .50 heavy machine gun on its post in front of the commander's hatch, where he can reach its grips standing
    # in his hatch: only he fires it, and only while he is up (Brian, 22 November)
    v.roof_gun("m2", (x0 + 0.58, -0.44, roof - 0.02), arc=(-40.0, 40.0), lay=(-10.0, 30.0))
    if fine:
        v.port(Vector((x0 - 0.30, 0.945, zt + 0.44)), Y, 0.075)
        for at in ((x0 + 0.52, 0.60), (x0 + 0.52, -0.60), (x0 - 1.12, 0.0)):
            v.lift_eye((at[0], at[1], roof - 0.04), Z, X, 0.035)
        # the searchlight on the roof, the blade sight the commander lays her by, the lifting rings on the gun's shield
        v.headlamp((x0 + 0.16, 0.10, roof + 0.075), X, 0.065, stalk=0.075, part="turret")
        v.slab((x0 + 0.56, -0.404, roof - 0.03), (x0 + 0.62, -0.396, roof + 0.055), "steel", part="turret")
        v.slab((x0 + 0.20, -0.404, roof - 0.01), (x0 + 0.215, -0.396, roof + 0.05), "steel", part="turret")
        # the foundry's marks cast proud on her cheek (strokes, not a word), and the seam where the mould's halves met
        for j in range(5):
            v.slab((x0 + 0.26 + j * 0.035, 0.955, zt + 0.50), (x0 + 0.28 + j * 0.035, 0.967, zt + 0.535 + 0.01 * (j % 3)), "upper", part="turret")
    v.aerial("aerial", "turret", (x0 - 1.12, 0.48, roof - 0.04), 2.2)
    for s in (LEFT, RIGHT):
        v.mark("USA", (x0 - 0.62, s * 0.905, zt + 0.44), Vector((-0.12, s, 0.08)), 0.15, part="turret")

    # ---- the gun: the 75 mm in its wide rounded shield ----------------------------------------------------------
    tx, tz = x0 + 0.74, zt + 0.40
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-10.0, 25.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx, 0, tz), Y, [(0.50, 0.0), (0.50, 0.30), (0.43, 0.36), (-0.43, 0.36), (-0.50, 0.30), (-0.50, 0.0)], "upper", v.n(40, 12))        # the shield, a drum across her face
    v.lathe((tx + 0.30, 0, tz), X, [(0.36, 0.0), (0.36, 0.105), (0.22, 0.14), (0.0, 0.17), (0.0, 0.0)], "upper", v.n(26, 8))                # the barrel's collar
    if fine:
        v.bolts((tx + 0.33, 0, tz), X, 0.2, 8, r=0.012)
        v.lathe((tx + 0.34, -0.27, tz + 0.02), X, [(0.03, 0.0), (0.03, 0.045), (0.0, 0.05), (0.0, 0.0)], "upper", 12)                       # the gunner's sight
        for y in (-0.33, 0.33):
            v.lift_eye((tx + 0.02, y, tz + 0.355), Z, X, 0.03)
        v.slab((tx + 0.30, -0.31, tz - 0.035), (tx + 0.352, -0.23, tz + 0.075), "upper", 0.006)                                              # the sight's armoured hood
        for y in (-0.47, 0.47):                                                                                                           # the bolts down each end of the shield
            for z in (-0.2, 0.0, 0.2):
                v.bolt((tx + 0.30 * math.cos(math.asin(z / 0.36)) + 0.025, y, tz + z), X, 0.013, 0.01)
    v.mg((tx + 0.26, 0.25, tz), X, 0.2)
    v.socket("coax", (tx + 0.48, 0.25, tz), X)
    v.part("barrel", "gun", (tx + 0.56, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.32], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.56, 0, tz), 1.46, 0.075, r0=0.082, r1=0.058, brake=None, sleeve=None)
    v.socket("muzzle", (tx + 2.04, 0, tz), X)
    v.m.info["bore"] = 0.075
    v.m.info["brake"] = False

    # ---- sockets for what happens to her ------------------------------------------------------------------------
    v.socket("engine", d_at(0.5, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.5, 0.4) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.40, 0.53, hood_top + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(0.6, 0.0), gn, part="hull")
    v.socket("hit.left", (0.2, sw, 1.4), Y, part="hull")
    v.socket("hit.right", (0.2, -sw, 1.4), -Y, part="hull")
    v.socket("hit.rear", r_at(0.25, 0.0), rn, part="hull")
    v.socket("hit.turret", (x0 + 0.2, 0.95, zt + 0.4), Y, part="turret")
    v.socket("case", (x0 - 0.5, 0.95, zt + 0.42), Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.30, kick=0.9, gun_low=-10.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver", "hatch_radio"],
                 traverse=24.0, idle_cue="tank_idle_sherman_loop", drive_cue="tank_sherman_drive_loop", gun_cue="tank_gun_01", blow=(-0.9, 2.8, 0.45, 170.0))
    v.name_place((1.0, 1.42), long=0.9, high=0.12, lines=(2, 1), high_two=0.10)                    # her crew's name (b477): on the sponson, ahead of the star
    return v


def _turn(along, up):
    """The turn that lays a box's length along `along` and its top toward `up`."""
    from mathutils import Matrix
    a, u = Vector(along).normalized(), Vector(up).normalized()
    return Matrix((a, u.cross(a), u)).transposed()
