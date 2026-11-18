"""British tanks.

The Matilda (Infantry Tank Mark II): slow, thick-skinned, the queen of the desert in 1940 and 1941. What tells her from
across a field: the heavy armoured skirt down each side with its five mud chutes, only the feet of her small wheels
showing under it; the pointed cast nose between the two track horns; the small round cast turret with its 2-pounder.
Every shape is our own mesh, made from numbers here.
"""

import math

import bmesh
from mathutils import Vector

from motorpool import LEFT, RIGHT, X, Y, Z, sym_plan
from tanks_any import turn_to
from tanks_ger import coat
from works import Works


def holed_plate(v, outline, holes, y, thick, paint, bevel=0.005, part=None):
    """A flat plate standing fore and aft with holes cut through it. `outline` and each of `holes` are (x, z) points
    going round; its outer face is at y and it is `thick` deep toward the middle of the vehicle. The plain mesh
    leaves the holes out."""
    bm = bmesh.new()
    y_in = y - math.copysign(thick, y)

    def loop(pts):
        vs = [bm.verts.new((x, y, z)) for x, z in pts]
        return [bm.edges.new((vs[i], vs[(i + 1) % len(vs)])) for i in range(len(vs))]
    edges = loop(outline)
    for h in (holes if v.fine else []):
        edges += loop(h)
    bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=edges)
    front = bm.faces[:]
    rim = [e for e in bm.edges if len(e.link_faces) == 1]
    twin = {vt: bm.verts.new((vt.co.x, y_in, vt.co.z)) for vt in bm.verts[:]}
    for f in front:
        bm.faces.new([twin[vt] for vt in reversed(f.verts)])
    for e in rim:
        a, b = e.verts
        bm.faces.new((a, b, twin[b], twin[a]))
    v.put(bm, paint, bevel, angle=30.0, part=part)


def arc(cx, cz, a, b, a0, a1, n):
    """Points round part of an oval in the side view: from angle a0 to a1 (degrees, 0 forward, 90 up)."""
    return [(cx + a * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cz + b * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def matilda(fine=True, tag="UK"):
    """Matilda II: 5.6 m long, 2.59 m wide, 2.5 m high. The drive at the back, ten small steel wheels a side in
    five pairs behind an armoured skirt, a cast turret with the 2-pounder and a machine gun beside it."""
    v = Works("Matilda", "Matilda II", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.75))
    hw, zg, zr, ty, yo = 0.72, 1.18, 1.50, 1.05, 1.295       # the tub's half-width; the guards; the roof; the track's middle; the skirt's face

    # ---- the tub between the tracks, and the pointed cast nose ahead of it -------------------------------------
    v.extrude([(-2.66, 0.72), (-2.50, 0.46), (1.80, 0.46), (1.80, zg), (-2.66, zg)], -hw, hw, coat, bevel=0.012)
    k = v.n(12, 4)
    under = [(1.98 + 0.80 * math.cos(a) ** 1.25, 0.88 - 0.42 * abs(math.sin(a)) ** 1.25) for a in [math.radians(-90.0 + 90.0 * i / k) for i in range(k + 1)]]
    zn = lambda x: 1.22 - 0.2679 * (x - 1.76)                  # noqa: E731  (the height of the nose's top plate at x)
    p0, p1, p2 = Vector((2.78, 0.88)), Vector((2.78, zn(2.78))), Vector((2.50, zn(2.50)))
    kk = v.n(6, 2)
    over = [tuple(p0 * (1 - t) ** 2 + p1 * 2 * t * (1 - t) + p2 * t * t) for t in [i / kk for i in range(1, kk + 1)]]
    v.extrude([(1.74, 0.46)] + under + over + [(1.74, 1.22)], -hw, hw, coat, bevel=0.03, segs=3, angle=30.0)
    nl = Vector((-1.0, 0, 0.2679)).normalized()               # up the nose's top plate
    nn = Vector((nl.z, 0, -nl.x))                             # out of it
    n_at = lambda u, y: Vector((2.50, y, zn(2.50))) + nl * u + nn * 0.002     # noqa: E731
    if fine:
        for i in range(11):                                  # the bolts along the nose casting's upper edge
            v.bolt(n_at(0.02, -0.25 + 0.5 * i / 10), nn, 0.014, 0.01)
    # the lockers either side of the driver: wedge-shaped boxes on the nose, louvred on top
    for s in (LEFT, RIGHT):
        ya, yb = s * 0.31, s * 0.715
        v.hull([(2.56, ya, zn(2.56) - 0.01), (2.56, yb, zn(2.56) - 0.01), (2.56, ya, 1.055), (2.56, yb, 1.055),
                (1.28, ya, zg - 0.02), (1.28, yb, zg - 0.02), (1.28, ya, 1.37), (1.28, yb, 1.37)], coat, bevel=0.016, both=False)
        tl = Vector((2.56 - 1.28, 0, 1.055 - 1.37)).normalized()     # forward down the locker's lid
        tn = Vector((-tl.z, 0, tl.x))
        lid = Vector((1.92, s * 0.5125, (1.055 + 1.37) / 2)) + tn * 0.002
        v.box((1.06, 0.33, 0.012), lid + tn * 0.006, "upper", 0.004, turn=turn_to(tl, tn))
        v.louvres(lid + tl * 0.05 + tn * 0.012, (0.24, 0.62), tn, count=6, down=tl)
        if fine:
            for d in (-0.3, 0.3):
                v.hinge(lid + tl * d + Y * (s * 0.165) - tl * 0.05 + tn * 0.012, lid + tl * d + Y * (s * 0.165) + tl * 0.05 + tn * 0.012, 0.011)
            v.handle(lid + tl * 0.48 - Y * 0.07 + tn * 0.012, lid + tl * 0.48 + Y * 0.07 + tn * 0.012, tn, 0.007, 0.03, "steel")
        v.headlamp((2.44, s * 0.52, 1.095), X, 0.06, stalk=0.07)
        v.hook((2.72, s * 0.36, 0.74), Vector((0.9, 0, -0.3)).normalized(), 0.09)
        v.hook((-2.66, s * 0.40, 0.80), -X, 0.09)

    # ---- the crew's part: the driver's narrow cab at the front, widening to the fighting room ------------------
    v.prism(sym_plan([(1.76, 0.34), (1.20, 0.90), (-0.98, 0.90)]), zg - 0.01, zr, coat, bevel=0.014, segs=2, angle=14.0)
    v.visor((1.765, 0.0, 1.385), X, 0.36, 0.11, 0.05)
    v.hatch_rect("hatch_driver", "hull", (1.40, 0.0, zr), (0.44, 0.50), Z, X, hinge="back", opens=100.0, thick=0.024)
    v.use("hull")
    v.periscope((1.66, 0.0, zr), X, (0.06, 0.13, 0.06))
    if fine:
        for s in (LEFT, RIGHT):                              # the bolts along the foot of the crew's part
            for i in range(11):
                v.bolt((1.08 - 1.95 * i / 10, s * 0.901, zg + 0.045), Y * s, 0.012, 0.008)
            v.weld((1.74, s * 0.345, zg + 0.2), (1.21, s * 0.895, zg + 0.2), Vector((0.71, s * 0.71, 0)))

    # ---- the engine deck: louvred covers the width of it, an exhaust pipe down each side to the silencers -----
    v.hull([(-0.98, 0.90, zg), (-0.98, 0.88, 1.46), (-2.50, 0.84, 1.33), (-2.70, 0.88, zg)], coat, bevel=0.014)
    dk = Vector((-2.50 + 0.98, 0, 1.33 - 1.46)).normalized()          # back along the deck
    dn = Vector((dk.z, 0, -dk.x))                                     # up out of it
    d_at = lambda u, y: Vector((-0.98, y, 1.46)) + dk * u + dn * 0.002    # noqa: E731
    for s in (LEFT, RIGHT):
        for u, long in ((0.42, 0.62), (1.10, 0.56)):
            v.box((long + 0.06, 0.78, 0.02), d_at(u, s * 0.42) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
            v.grille(d_at(u, s * 0.42) + dn * 0.02, (0.70, long), dn, Y, slats=9, deep=0.02)
        if fine:
            for u in (0.42, 1.10):
                v.hinge(d_at(u - 0.1, s * 0.022), d_at(u + 0.1, s * 0.022), 0.012)
            v.handle(d_at(1.46, s * 0.42) - Y * 0.08 + dn * 0.004, d_at(1.46, s * 0.42) + Y * 0.08 + dn * 0.004, dn, 0.007, 0.035, "steel")
        v.tube([(-1.26, s * 0.86, 1.36), (-1.32, s * 0.97, 1.30), (-1.50, s * 1.02, 1.265), (-2.44, s * 1.02, 1.245), (-2.66, s * 1.00, 1.20), (-2.80, s * 0.92, 1.08), (-2.83, s * 0.84, 1.00)],
               0.038, "rust", v.n(10, 5))
        v.silencer((-2.83, s * 0.84, 1.00), (-2.83, s * 0.10, 1.00), 0.085, bands=2, tail=((-2.83, s * 0.10, 1.00), (-1.0, 0, -0.35)))
        v.slab((-2.72, s * 0.44, 0.96), (-2.68, s * 0.50, 1.04), "shade")
        v.socket("exhaust." + ("1" if s > 0 else "2"), (-2.95, s * 0.10, 0.955), (-1.0, 0, -0.35))

    # ---- the track guards and the armoured skirts ------------------------------------------------------------
    front = arc(2.30, 0.74, 0.37, 0.44, 90.0, -22.0, v.n(10, 4))     # over the nose of the track and under it
    rear = arc(-2.36, 0.72, 0.37, 0.46, 202.0, 90.0, v.n(10, 4))     # under the tail of the track, up and over
    chutes = (-1.48, -0.74, 0.0, 0.74, 1.48)
    holes = [[(c - 0.15, 0.98), (c + 0.15, 0.98), (c + 0.18, 0.62), (c - 0.18, 0.62)] for c in chutes]
    for s in (LEFT, RIGHT):
        v.slab((-2.36, s * hw, zg - 0.022), (2.30, s * (yo - 0.02), zg), coat, 0.004)
        for curve, angles in ((lambda a: (2.30 + 0.37 * math.cos(a), 0.74 + 0.44 * math.sin(a)), (90.0, 74.0, 58.0, 42.0)),
                              (lambda a: (-2.36 + 0.37 * math.cos(a), 0.72 + 0.46 * math.sin(a)), (90.0, 108.0, 126.0, 144.0))):
            pts = [curve(math.radians(a)) for a in angles]
            for (xa, za), (xb, zb) in zip(pts, pts[1:]):       # the guard carried down round each end of the track
                v.plate([(xa, s * 0.84, za), (xa, s * (yo - 0.02), za), (xb, s * (yo - 0.02), zb), (xb, s * 0.84, zb)][::int(s)], 0.008, "upper", 0.002)
        holed_plate(v, front + [(1.74, 0.30), (-1.76, 0.30)] + rear, holes, s * yo, 0.045, "upper", bevel=0.006)
        for c in chutes:
            # inside each chute: the plate that slides the mud off the top of the track and out
            v.plate([(c - 0.19, s * 0.88, 1.0), (c + 0.19, s * 0.88, 1.0), (c + 0.19, s * (yo - 0.04), 0.61), (c - 0.19, s * (yo - 0.04), 0.61)][::int(s)], 0.008, "upper")
            for e in (-1, 1):
                v.slab((c + e * 0.185, s * 0.88, 0.61), (c + e * 0.195, s * (yo - 0.04), 1.0), "shade")
            if fine:
                v.bolts((c, s * (yo + 0.001), 0.47), Y * s, 0.075, 4, r=0.015, start=math.radians(45))
        if fine:
            n = 20
            for i in range(n + 1):                           # the bolts along the skirt's head
                v.bolt((-2.2 + 4.4 * i / n, s * (yo + 0.001), 1.125), Y * s, 0.013, 0.009)
            for c0, c1 in zip(chutes, chutes[1:]):
                v.bolt(((c0 + c1) / 2, s * (yo + 0.001), 0.37), Y * s, 0.013, 0.009)
                v.bolt(((c0 + c1) / 2, s * (yo + 0.001), 0.80), Y * s, 0.013, 0.009)
    # what rides on the guards beside the crew's part
    v.shovel((0.25, 1.09, zg + 0.022), 0.0, 0.95)
    v.crowbar((0.1, -1.12, zg + 0.018), 0.0, 1.3)
    v.axe((0.75, -1.04, zg + 0.02), 0.0, 0.7, roll=90.0)
    for s in (LEFT, RIGHT):
        v.bin((-0.94, s * 0.93, zg), (-0.30, s * 1.24, zg + 0.20), lid_side=Y * s)
    if fine:
        v.cable([(1.9, 0.98, zg + 0.02), (1.3, 1.16, zg + 0.018), (0.9, 1.2, zg + 0.018)], 0.012)

    # ---- the running gear -----------------------------------------------------------------------------------
    r_w, thick = 0.125, 0.028
    pairs = (-1.52, -0.76, 0.0, 0.76, 1.52)
    xs = [c + d for c in pairs for d in (-0.17, 0.17)]
    v.running(xs, r_w, ty, (-2.42, 0.72, 0.30), (2.36, 0.74, 0.30), rollers=[(x, 0.99, 0.06) for x in (-1.5, -0.5, 0.5, 1.5)], drive="rear", wheel_z=r_w + thick,
              wheel=dict(w=0.05, tyre=0.0, gap=0.06, bolts=0, sides=22, hub=0.45, axle=0.1, kind="steel"),
              sprocket_kw=dict(teeth=17, wide=0.30, tooth=0.03, holes=0, bolts=8, sides=28, hub=0.4),
              idler_kw=dict(wide=0.28, kind="spoke", spokes=6),
              roller_kw=dict(wide=0.12, tyre=0.0, reach=0.16),
              track=dict(pitch=0.115, wide=0.36, thick=thick, sag=0.008, style={"kind": "plate", "horn": "centre", "horn_high": 0.05, "grouser": 0.014, "cleats": True}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):
        yb = s * (ty + 0.085)
        for c in pairs:
            v.slab((c - 0.21, yb, 0.105), (c + 0.21, yb + s * 0.022, 0.20), "shade", 0.006)       # the beam joining a pair's axles
            v.slab((c - 0.055, yb, 0.15), (c + 0.055, yb + s * 0.04, 0.58), "shade", 0.006)       # its hanger, up behind the skirt
            if fine:
                for d in (-0.17, 0.17):
                    v.bolt((c + d, yb + s * 0.022, r_w + thick), Y * s, 0.024, 0.012)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((-2.42, s * (hw - 0.01), 0.72), Y * s, [(0.16, 0.0), (0.16, 0.15), (0.08, 0.22), (0.0, 0.24), (0.0, 0.0)], "upper", v.n(22, 8))
        v.lathe((2.36, s * (hw - 0.01), 0.74), Y * s, [(0.16, 0.0), (0.16, 0.08), (0.0, 0.12), (0.0, 0.0)], "upper", v.n(16, 6))

    # ---- the turret: one round casting, the commander's drum on its roof ---------------------------------------
    x0, zt, th = 0.38, zr, 0.62
    v.ring_well((x0, 0.0, zt), 0.76)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    v.cast([{"z": zt + 0.02, "xc": x0, "af": 0.80, "ar": 0.82, "b": 0.79, "n": 2.15},
            {"z": zt + 0.28, "xc": x0, "af": 0.80, "ar": 0.82, "b": 0.79, "n": 2.2},
            {"z": zt + th - 0.07, "xc": x0, "af": 0.76, "ar": 0.79, "b": 0.75, "n": 2.25}], "upper", sides=v.n(48, 14), top=0.07)
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.80), (0.0, 0.82), (0.0, 0.0)], "shade", v.n(36, 12))
    roof = zt + th
    cup = v.cupola((x0 - 0.20, 0.30, roof - 0.01), 0.27, 0.22, slits=0)
    v.hatch_round("hatch_commander", "turret", cup, 0.20, Z, -X, thick=0.028, opens=100.0, split=True)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "turret", (x0 - 0.16, -0.36, roof), (0.44, 0.36), Z, X, hinge="right", opens=100.0, thick=0.026)
    v.use("turret")
    v.periscope((x0 + 0.38, 0.30, roof - 0.01), X, (0.07, 0.14, 0.07))
    v.vent((x0 + 0.34, -0.22, roof - 0.01), 0.09, 0.045)
    if fine:
        v.port(Vector((x0 - 0.30, 0.775, zt + 0.34)), Vector((-0.3, 1, 0.05)), 0.05)
        for at in ((x0 + 0.50, 0.42), (x0 + 0.50, -0.42), (x0 - 0.62, 0.0)):
            v.lift_eye((at[0], at[1], roof - 0.03), Z, X, 0.03)
        for dz in (0.0, 0.13):                               # the two smoke dischargers on her right cheek
            a = Vector((x0 + 0.16, -0.83, zt + 0.25 + dz))
            way = Vector((0.92, -0.06, 0.38)).normalized()
            v.rod(a, a + way * 0.30, 0.048, "steel", 12)
            v.rod(a + way * 0.30, a + way * 0.302, 0.036, "black", 12)
        v.slab((x0 + 0.10, -0.80, zt + 0.20), (x0 + 0.26, -0.76, zt + 0.44), "upper", 0.006)
    v.aerial("aerial", "turret", (x0 - 0.56, -0.36, roof - 0.03), 2.0)
    for s in (LEFT, RIGHT):
        v.mark("UK", (0.45, s * 0.901, zg + 0.19), Y * s, 0.14, part="hull")

    # ---- the gun: the 2-pounder in a rounded shield, a machine gun on its right ---------------------------------
    tx, tz = x0 + 0.60, zt + 0.30
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-12.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx, 0, tz), Y, [(0.40, 0.0), (0.40, 0.25), (0.35, 0.29), (-0.35, 0.29), (-0.40, 0.25), (-0.40, 0.0)], "upper", v.n(36, 12))
    v.lathe((tx + 0.24, 0, tz), X, [(0.26, 0.0), (0.26, 0.075), (0.15, 0.095), (0.0, 0.115), (0.0, 0.0)], "upper", v.n(22, 8))
    if fine:
        v.bolts((tx + 0.262, 0, tz), X, 0.09, 6, r=0.011)
        v.lathe((tx + 0.27, 0.22, tz + 0.04), X, [(0.03, 0.0), (0.03, 0.035), (0.0, 0.04), (0.0, 0.0)], "upper", 12)      # the gunner's sight
    v.mg((tx + 0.22, -0.22, tz), X, 0.34)
    v.socket("coax", (tx + 0.58, -0.22, tz), X)
    v.part("barrel", "gun", (tx + 0.42, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.22], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.42, 0, tz), 1.42, 0.04, r0=0.056, r1=0.034, brake=None, sleeve=None)
    v.socket("muzzle", (tx + 1.86, 0, tz), X)
    v.m.info["bore"] = 0.04
    v.m.info["brake"] = False

    # ---- sockets for what happens to her --------------------------------------------------------------------
    v.socket("engine", d_at(0.75, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.42, 0.42) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.40, 0.0, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", n_at(0.4, 0.0), nn, part="hull")
    v.socket("hit.left", (0.2, yo, 0.8), Y, part="hull")
    v.socket("hit.right", (0.2, -yo, 0.8), -Y, part="hull")
    v.socket("hit.rear", (-2.68, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", (x0, 0.79, zt + 0.3), Y, part="turret")
    v.socket("case", (x0 - 0.3, -0.79, zt + 0.3), -Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.18, kick=0.45, gun_low=-12.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver"],
                 traverse=14.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.8, 2.4, 0.38, 166.0))
    return v
