"""Japanese tanks.

The Type 97 medium tank: the army's main tank from China to the last islands. What tells her from across a field:
the rail aerial standing round the roof of her turret like a handrail; the turret set off to the right, a short
57 mm gun in front and a machine gun in its back; the driver's rounded hood on the right of the front plate; six
wheels a side, the long spring casings lying along the hull above them. Every shape is our own mesh, made from
numbers here.
"""

import math

from mathutils import Vector

from motorpool import LEFT, RIGHT, X, Y, Z, outline
from tanks_any import rivets, turn_to
from tanks_ger import BY_MONTH_HOW, coat
from works import Works


# The army's three-colour coat (the art director, 22 Nov 20:23Z): earth brown, deep grass green and dry-grass khaki in
# soft brushed patches 0.6 to 1.2 m across, each about a third of her. All war long: the historian found no order or
# date for plain tanks, only "sometimes" (historian/ija-tank-paint-1937-45.md, 22 Nov), so plain khaki is a small
# share in every month and the three colours are the face a game showing one look gives her.
JAP_KHAKI = "988256"
JAP_CAMO = {"upper": "A68D5C", "upper2": "6B5038", "upper3": "4E5A35"}
JAP_MONTHS = [{"paint": {"mix": {"camo": 0.9, "khaki": 0.1}, "one": "camo"}}]


def brush(v, cell=0.8):
    """The three-colour patches laid in her file (camo.lay3: the brown in `blend`, the green in `blend2`), her file's
    own paints left plain khaki so that they do not show; jap_coats fills them. Comes before her sockets and clips."""
    v.own_paint("upper3", v.m.paints["upper"])
    done = v.camouflage(three=True, cell=cell, cover=(0.5, 0.34), **({"soft": 0.06, "grain": 0.3} if v.fine else {"grain": 0.9}))
    v.m.paints["upper2"] = v.m.paints["upper3"] = v.m.paints["upper"]
    return done


def jap_coats(v):
    """Her paint swap: `khaki` (one colour all over) and `camo` (the three colours in her patches), by month."""
    from motorpool import dark
    shade = v.m.paints.get("shade")
    return {"default": "camo",
            "choices": {"khaki": {"what": "khaki, one colour all over", "years": "a few in any year",
                                  "paints": {"upper": JAP_KHAKI, "upper2": JAP_KHAKI, "upper3": JAP_KHAKI, "shade": shade}},
                        "camo": {"what": "the army's three colours: earth brown, deep grass green and dry-grass khaki in soft brushed patches",
                                 "years": "1937 to 1945", "paints": {**JAP_CAMO, "shade": dark(JAP_CAMO["upper"], 0.62)}}}}


def jap_swaps(v):
    v.m.info.setdefault("file", {})["swaps"] = {"how": "paint: put the chosen coat's colours in place of the paints of the same names. " + BY_MONTH_HOW,
                                                "paint": jap_coats(v), "by_month": JAP_MONTHS}


def ring_rivets(v, key, z, count, part=None, r=0.0105):
    """Rivet heads round a rounded turret at height z, on the plan `key` describes (as for `cast`)."""
    if not v.fine:
        return
    pts = outline(key["xc"], key["af"], key["ar"], key["b"], key.get("n", 2.4), count, key.get("yc", 0.0))
    for i, (x, y) in enumerate(pts):
        (xa, ya), (xb, yb) = pts[i - 1], pts[(i + 1) % count]
        out = Vector((yb - ya, -(xb - xa), 0)).normalized()
        v.bolt((x, y, z), out, r, 0.007, part=part)


def type97(fine=True, tag="JAP"):
    """Type 97 medium tank: 5.5 m long, 2.33 m wide, 2.23 m high. The drive at the front; the engine behind under
    louvred doors, a silencer on each rear guard."""
    v = Works("Type97", "Type 97 medium tank", "tank", tag, fine)
    # (the E7 surface pass: shade from her shape, dirt of her ground; the art director, 22 Nov 17:55Z)
    v.dice = 0.2
    v.worn_kw = dict(shade=True, shade_deep=0.42, ground="field", layers=2, marks=0.45, gear=0.8, rim=True)
    v.root(hull_pivot=(0.0, 0.0, 0.68))
    hw, zg, zr = 0.70, 1.00, 1.44                  # half-width between the tracks; the guards; the roof

    # ---- the tub, the sloped nose plate, the crew's part leaning in a little toward its roof ------------------
    v.hull([(x, hw, z) for x, z in [(-2.62, 0.56), (-2.46, 0.40), (2.05, 0.40), (2.62, 0.62), (2.72, 0.80), (2.56, zg - 0.02), (-2.70, zg - 0.02), (-2.70, 0.68)]], coat, bevel=0.012)
    v.hull([(2.58, hw, zg - 0.03), (1.60, hw, 1.17), (1.60, hw, zg - 0.03)], coat, bevel=0.012)
    v.hull([(1.62, 1.00, zg), (1.50, 0.94, zr), (-0.80, 0.94, zr), (-0.80, 1.00, zg)], coat, bevel=0.012)
    gl = Vector((1.60 - 2.58, 0, 1.17 - (zg - 0.03))).normalized()   # up the nose plate
    gn = Vector((gl.z, 0, -gl.x))
    fl = Vector((1.50 - 1.62, 0, zr - zg)).normalized()             # up the front plate
    fn = Vector((fl.z, 0, -fl.x))
    g_at = lambda u, y: Vector((2.58, y, zg - 0.03)) + gl * u + gn * 0.002     # noqa: E731
    f_at = lambda u, y: Vector((1.62, y, zg)) + fl * u + fn * 0.002           # noqa: E731
    side_y = lambda z: 1.00 - 0.06 * (z - zg) / (zr - zg)                     # noqa: E731
    for s in (LEFT, RIGHT):                              # a lid over each brake, the hooks, the lamps
        v.box((0.46, 0.42, 0.018), g_at(0.50, s * 0.34) + gn * 0.009, "upper", 0.005, turn=turn_to(gl, gn))
        if fine:
            v.handle(g_at(0.34, s * 0.34) - Y * 0.07 + gn * 0.018, g_at(0.34, s * 0.34) + Y * 0.07 + gn * 0.018, gn, 0.007, 0.03, "steel")
            rivets(v, g_at(0.05, s * 0.66), g_at(0.95, s * 0.66), gn, 0.11)
        v.hook((2.68, s * 0.42, 0.60), Vector((0.8, 0, -0.5)).normalized(), 0.085)
        v.hook((-2.70, s * 0.42, 0.66), -X, 0.085)
    v.headlamp((2.30, 0.0, zg + 0.07), X, 0.075, stalk=0.07)
    # the driver's rounded hood on the right of the front plate, the bow gun in its ball on the left
    v.lathe((1.50, -0.46, 1.10), Z, [(0.31, 0.0), (0.31, 0.21), (0.28, 0.245), (0.0, 0.245), (0.0, 0.0)], "upper", v.n(30, 10))
    v.visor((1.748, -0.46, 1.29), X, 0.24, 0.06, 0.02, brow=False)
    if fine:
        for a in (-55.0, 55.0):
            v.visor((1.50 + 0.248 * math.cos(math.radians(a)), -0.46 + 0.248 * math.sin(math.radians(a)), 1.29), Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0)), 0.12, 0.05, 0.016, brow=False)
    v.mg_ball(f_at(0.24, 0.46) + fn * 0.02, Vector((1, 0, 0.05)).normalized(), 0.10, socket="hull_mg", length=0.34)
    v.hatch_rect("hatch_driver", "hull", (1.22, -0.46, zr), (0.40, 0.46), Z, X, hinge="back", opens=100.0, thick=0.022)
    v.use("hull")
    if fine:
        for s in (LEFT, RIGHT):
            sn = Vector((0, s * 0.44, 0.06)).normalized()
            for z in (zg + 0.05, zr - 0.05):
                rivets(v, (1.46, s * (side_y(z) + 0.001), z), (-0.74, s * (side_y(z) + 0.001), z), sn, 0.115)
            for x in (0.70, -0.05, -0.74):
                rivets(v, (x, s * (side_y(zg + 0.16) + 0.001), zg + 0.16), (x, s * (side_y(zr - 0.16) + 0.001), zr - 0.16), sn, 0.115)
            v.mark("JAP", (1.08, s * side_y(1.22), 1.22), Vector((0, s, 0.136)), 0.13, part="hull")
        rivets(v, f_at(0.05, -0.92), f_at(0.05, 0.92), fn, 0.115)

    # ---- the track guards, the engine deck, a silencer on each rear guard under its mesh ---------------------
    for s in (LEFT, RIGHT):
        y0, y1 = s * hw, s * 1.165
        v.slab((-2.72, y0, zg - 0.016), (2.56, y1, zg), coat, 0.004)
        v.plate([(2.56, y0, zg), (2.56, y1, zg), (2.84, y1, 0.80), (2.84, y0, 0.80)][::int(s)], 0.01, "upper", 0.003)
        v.plate([(-2.72, y1, zg), (-2.72, y0, zg), (-2.88, y0, 0.78), (-2.88, y1, 0.78)][::int(s)], 0.01, "upper", 0.003)
        if fine:
            for x in (-2.1, -1.0, 0.2, 1.4, 2.2):
                v.slab((x - 0.012, s * hw, zg - 0.10), (x + 0.012, s * 1.13, zg - 0.016), "shade")
        v.silencer((-1.50, s * 1.03, zg + 0.105), (-2.40, s * 1.03, zg + 0.105), 0.082, bands=2, tail=((-2.40, s * 1.03, zg + 0.105), (-1.0, s * 0.3, 0.0)))
        v.tube([(-1.26, s * 0.93, zg + 0.20), (-1.34, s * 1.03, zg + 0.13), (-1.50, s * 1.03, zg + 0.105)], 0.034, "rust", v.n(10, 5))
        if fine:                                         # the wire guard over the silencer
            for x in (-1.56, -1.95, -2.34):
                v.tube([(x, s * 0.93, zg), (x, s * 0.93, zg + 0.16), (x, s * 0.99, zg + 0.215), (x, s * 1.07, zg + 0.215), (x, s * 1.13, zg + 0.16), (x, s * 1.13, zg)], 0.006, "steel", 5)
            for dy, dz in ((0.93, 0.16), (1.03, 0.215), (1.13, 0.16)):
                v.rod((-1.56, s * dy, zg + dz), (-2.34, s * dy, zg + dz), 0.005, "steel", 5)
        v.socket("exhaust." + ("1" if s > 0 else "2"), (-2.56, s * 1.06, zg + 0.11), (-1.0, s * 0.3, 0.0))
    v.hull([(-0.80, 0.96, zg), (-0.80, 0.93, 1.40), (-2.50, 0.88, 1.22), (-2.72, 0.92, zg)], coat, bevel=0.012)
    dk = Vector((-2.50 + 0.80, 0, 1.22 - 1.40)).normalized()          # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))
    d_at = lambda u, y: Vector((-0.80, y, 1.40)) + dk * u + dn * 0.002        # noqa: E731
    for s in (LEFT, RIGHT):
        v.box((1.30, 0.62, 0.02), d_at(0.78, s * 0.37) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
        v.louvres(d_at(0.50, s * 0.37) + dn * 0.02, (0.46, 0.60), dn, count=6, down=dk)
        v.louvres(d_at(1.14, s * 0.37) + dn * 0.02, (0.46, 0.46), dn, count=5, down=dk)
        if fine:
            for u in (0.3, 1.25):
                v.hinge(d_at(u - 0.05, s * 0.045) + dn * 0.02, d_at(u + 0.05, s * 0.045) + dn * 0.02, 0.011)
            v.handle(d_at(0.82, s * 0.62) - X * 0.07 + dn * 0.02, d_at(0.82, s * 0.62) + X * 0.07 + dn * 0.02, dn, 0.007, 0.03, "steel")
            rivets(v, d_at(0.06, s * 0.86), d_at(1.64, s * 0.82), dn, 0.12)
    v.shovel((0.5, 1.08, zg + 0.02), 0.0, 0.9)
    v.crowbar((0.3, -1.09, zg + 0.018), 0.0, 1.25)
    v.jack((-0.9, 1.06, zg), 0.0)
    v.bin((-1.2, -1.14, zg), (-0.5, -0.99, zg + 0.16), lid_side=-Y)

    # ---- the running gear: six wheels a side, the middle four in pairs on cranks, the springs along the hull ----
    ty, r_w, thick = 0.99, 0.265, 0.026
    wz = r_w + thick
    xs = [-1.80, -1.08, -0.36, 0.36, 1.08, 1.80]
    v.running(xs, r_w, ty, (2.36, 0.72, 0.29), (-2.40, 0.62, 0.27), rollers=[(-1.25, 0.895, 0.09), (0.0, 0.895, 0.09), (1.25, 0.895, 0.09)], wheel_z=wz,
              wheel=dict(w=0.07, tyre=0.03, gap=0.07, bolts=6, sides=30, hub=0.3, axle=0.24),
              sprocket_kw=dict(teeth=16, wide=0.28, tooth=0.03, holes=6, bolts=8, sides=28, hub=0.42),
              idler_kw=dict(wide=0.26, kind="spoke", spokes=6),
              roller_kw=dict(wide=0.14, tyre=0.014, reach=0.26),
              track=dict(pitch=0.115, wide=0.33, thick=thick, sag=0.014, style={"kind": "plate", "horn": "centre", "horn_high": 0.06, "grouser": 0.012, "cleats": False, "bar": 0.3}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):
        yh, ya = s * (hw + 0.07), s * (ty - 0.19)
        v.rod((-1.30, yh, 0.64), (1.30, yh, 0.64), 0.055, "upper", v.n(14, 6))                  # the springs' long casing
        for x in (-1.30, 0.0, 1.30):
            v.slab((x - 0.06, s * (hw - 0.01), 0.56), (x + 0.06, yh + s * 0.03, 0.72), "shade", 0.008)
        for ca in (-0.72, 0.72):                             # each pair's crank: a pivot on the hull, an arm to each axle, a lever up to the spring
            v.rod((ca, s * (hw - 0.01), 0.47), (ca, ya, 0.47), 0.05, "shade", 10)
            for xw in (ca - 0.36, ca + 0.36):
                v.rod((ca, ya, 0.47), (xw, ya, wz), 0.038, "shade", 8)
            v.rod((ca, yh, 0.47), (ca, yh, 0.62), 0.03, "shade", 8)
        for xw, e in ((-1.80, 1.0), (1.80, -1.0)):           # the end wheels, each on an arm of its own with a slanted spring
            v.rod((xw + e * 0.34, s * (hw - 0.01), 0.50), (xw + e * 0.34, ya, 0.50), 0.045, "shade", 10)
            v.rod((xw + e * 0.34, ya, 0.50), (xw, ya, wz), 0.036, "shade", 8)
            v.rod((xw + e * 0.06, yh, 0.36), (xw + e * 0.50, yh, 0.64), 0.04, "upper", 10)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((2.36, s * (hw - 0.01), 0.72), Y * s, [(0.16, 0.0), (0.16, 0.15), (0.08, 0.22), (0.0, 0.24), (0.0, 0.0)], "upper", v.n(22, 8))
        v.lathe((-2.40, s * (hw - 0.01), 0.62), Y * s, [(0.15, 0.0), (0.15, 0.08), (0.0, 0.11), (0.0, 0.0)], "upper", v.n(14, 6))

    # ---- the turret, set off to the right: rolled plate leaning in, the rail aerial round its roof -----------
    x0, y0, zt, th = 0.34, -0.12, zr, 0.54
    v.ring_well((x0, y0, zt), 0.70)
    v.part("turret", "hull", (x0, y0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    low = {"z": zt + 0.02, "xc": x0, "yc": y0, "af": 0.70, "ar": 0.84, "b": 0.72, "n": 2.5}
    high = {"z": zt + th - 0.025, "xc": x0 - 0.02, "yc": y0, "af": 0.56, "ar": 0.72, "b": 0.58, "n": 2.5}
    v.cast([low, high], "upper", sides=v.n(48, 14), top=0.025, steps=2, between=2)
    v.lathe((x0, y0, zt - 0.03), Z, [(0.06, 0.70), (0.0, 0.72), (0.0, 0.0)], "shade", v.n(32, 10))
    roof = zt + th
    mix = lambda t: {k: low[k] + (high[k] - low[k]) * t for k in low}        # noqa: E731
    ring_rivets(v, mix(0.10), zt + 0.02 + (th - 0.045) * 0.10, 44, part="turret")
    ring_rivets(v, mix(0.90), zt + 0.02 + (th - 0.045) * 0.90, 38, part="turret")
    cup = v.cupola((x0 - 0.14, y0 + 0.04, roof - 0.01), 0.31, 0.11, slits=0)
    v.hatch_round("hatch_commander", "turret", cup, 0.25, Z, -X, thick=0.028, opens=100.0, split=True)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.periscope((x0 + 0.34, y0 - 0.22, roof - 0.005), X, (0.06, 0.12, 0.06))
    # the rail aerial: a tube round the front and sides of the roof on short posts, open at the back
    rail_z = roof + 0.15
    angles = [-150.0 + 300.0 * i / 20 for i in range(21)]
    rail = [(x0 - 0.04 + 0.60 * math.cos(math.radians(a)), y0 + 0.60 * math.sin(math.radians(a)), rail_z) for a in angles]
    v.tube(rail, 0.013, "steel", v.n(8, 4))
    for i in (0, 4, 8, 12, 16, 20):
        px, py, _ = rail[i]
        foot = (x0 - 0.04 + (px - x0 + 0.04) * 0.84, y0 + (py - y0) * 0.84, roof - 0.02)
        v.rod(foot, (px, py, rail_z), 0.009, "steel", 6)
    # the machine gun in the turret's back, toward the left
    back = Vector((-0.86, 0.50, 0.0)).normalized()
    v.mg_ball(Vector((x0, y0, zt + 0.28)) + Vector((back.x * 0.77, back.y * 0.66, 0)), Vector((back.x, back.y, 0.12)).normalized(), 0.085, socket="turret_mg", length=0.30)
    v.use("turret")
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 + 0.06, y0 + s * 0.665, zt + 0.27)), Vector((0, s, 0.27)), 0.04)

    # ---- the gun: a short 57 mm, its recoil casing round the root of the barrel -------------------------------
    tx, tz = x0 + 0.56, zt + 0.27
    v.part("gun", "turret", (tx, y0, tz), axis=(0, -1, 0), limits=[-9.0, 21.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx, y0, tz), Y, [(0.23, 0.0), (0.23, 0.17), (0.19, 0.205), (-0.19, 0.205), (-0.23, 0.17), (-0.23, 0.0)], "upper", v.n(28, 10))
    v.lathe((tx + 0.14, y0, tz), X, [(0.16, 0.0), (0.16, 0.11), (0.06, 0.13), (0.0, 0.14), (0.0, 0.0)], "upper", v.n(22, 8))
    if fine:
        v.bolts((tx + 0.21, y0, tz), X, 0.12, 6, r=0.011)
    v.part("barrel", "gun", (tx + 0.22, y0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.2], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.22, y0, tz), 0.78, 0.057, r0=0.078, r1=0.064, brake=None, sleeve=(0.26, 0.098))
    v.socket("muzzle", (tx + 1.02, y0, tz), X)
    v.m.info["bore"] = 0.057
    v.m.info["brake"] = False

    brush(v)                                          # (the three-colour patches: the art director, 22 Nov 20:23Z)

    # ---- sockets for what happens to her ------------------------------------------------------------------------
    v.socket("engine", d_at(0.8, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.5, 0.37) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.22, -0.46, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, y0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", f_at(0.25, 0.0), fn, part="hull")
    v.socket("hit.left", (0.3, 0.98, 1.25), Y, part="hull")
    v.socket("hit.right", (0.3, -0.98, 1.25), -Y, part="hull")
    v.socket("hit.rear", (-2.7, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", (x0, y0 + 0.68, zt + 0.27), Y, part="turret")
    v.socket("case", (x0 - 0.3, y0 - 0.7, zt + 0.3), -Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.18, kick=0.55, gun_low=-9.0, hatches=["hatch_commander", "hatch_driver"], mg="hull_mg",
                 traverse=14.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.8, 2.3, 0.36, 165.0))
    v.name_place((0.3, 1.71), long=0.36, high=0.13, part="turret", writing="down", bend=0.03, script="japanese", high_one=0.15,
                 ink="white")                                 # (the historian, 2026-11-18: Japanese crews brushed their names in white, in China and the Pacific alike)       # brush-written characters (b477): on the turret's side, one under another
    jap_swaps(v)
    return v


def hago(fine=True, tag="JAP"):
    """Type 95 light tank (Ha-Go): 4.38 m long, 2.07 m wide, 2.28 m high, 7.4 tonnes. The drive at the front, the
    engine at the back; a small round turret set off to the left with a 37 mm gun in front and a machine gun in its
    back to the right; the bow machine gun in a box of its own on the left of the front plate, the driver on the right;
    four wheels a side in two pairs on bell cranks, the springs lying in long tubes along the hull. What tells her from
    across a field: how small and high she stands, her turret pushed to one side, the stepped front."""
    v = Works("HaGo", "Type 95 light tank", "tank", tag, fine)
    # (the art director's E7 surface pass from the first build: shade from her shape, dirt of her ground)
    v.dice = 0.2
    v.worn_kw = dict(shade=True, shade_deep=0.42, ground="field", layers=2, marks=0.45, gear=0.8, rim=True)
    v.root(hull_pivot=(0.0, 0.0, 0.60))
    hw, sw, zg, zr = 0.60, 0.98, 0.90, 1.52        # half-width between the tracks and of the crew's box; the guards; the roof

    # ---- the tub, the nose plate over the drive, the crew's box out over the tracks ---------------------------------
    v.hull([(x, hw, z) for x, z in [(-2.05, 0.50), (-1.92, 0.36), (1.62, 0.36), (2.08, 0.56), (2.08, zg - 0.02), (-2.12, zg - 0.02), (-2.12, 0.64)]], coat, bevel=0.012)
    v.hull([(2.10, hw, zg - 0.03), (1.50, hw, 1.10), (1.50, hw, zg - 0.03)], coat, bevel=0.012)
    v.hull([(1.50, sw, zg), (1.42, sw - 0.04, zr), (-0.55, sw - 0.04, zr), (-0.55, sw, zg)], coat, bevel=0.012)
    gl = Vector((1.50 - 2.10, 0, 1.10 - (zg - 0.03))).normalized()    # up the nose plate
    gn = Vector((gl.z, 0, -gl.x))
    fl = Vector((1.42 - 1.50, 0, zr - zg)).normalized()              # up the front plate
    fn = Vector((fl.z, 0, -fl.x))
    g_at = lambda u, y: Vector((2.10, y, zg - 0.03)) + gl * u + gn * 0.002      # noqa: E731
    f_at = lambda u, y: Vector((1.50, y, zg)) + fl * u + fn * 0.002            # noqa: E731
    side_y = lambda z: sw - 0.04 * (z - zg) / (zr - zg)                        # noqa: E731
    # the bow gunner's box on the left of the front: its face leaning back, its cheeks cut at an angle
    v.hull([(1.46, 0.14, zg), (1.46, 0.90, zg), (1.70, 0.22, zg), (1.70, 0.82, zg),
            (1.43, 0.14, 1.40), (1.43, 0.90, 1.40), (1.60, 0.24, 1.34), (1.60, 0.80, 1.34)], coat, 0.010, both=False)
    bn = Vector((1.0, 0.0, (1.70 - 1.60) / (1.34 - zg))).normalized()
    v.mg_ball(Vector((1.665, 0.52, 1.12)), Vector((1.0, 0.0, 0.04)).normalized(), 0.075, socket="hull_mg", length=0.32)
    # the driver on the right: his visor in the front plate, his lid in the roof over him
    v.visor(f_at(0.30, -0.50) + fn * 0.004, fn, 0.30, 0.10, 0.05)
    v.hatch_rect("hatch_driver", "hull", (1.12, -0.50, zr), (0.36, 0.42), Z, X, hinge="back", opens=100.0, thick=0.02)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.hook((2.14, s * 0.40, 0.52), Vector((0.8, 0, -0.5)).normalized(), 0.075)
        v.hook((-2.12, s * 0.40, 0.62), -X, 0.075)
    v.headlamp((1.86, 0.70, zg + 0.02), X, 0.065, stalk=0.08)
    if fine:
        rivets(v, g_at(0.04, -0.56), g_at(0.04, 0.56), gn, 0.11)
        rivets(v, f_at(0.04, -0.92), f_at(0.04, 0.10), fn, 0.11)
        for s in (LEFT, RIGHT):
            sn = Vector((0, s * 0.48, 0.03)).normalized()
            for z in (zg + 0.05, zr - 0.05):
                rivets(v, (1.44, s * (side_y(z) + 0.001), z), (-0.50, s * (side_y(z) + 0.001), z), sn, 0.115)
            for x in (0.55, -0.50):
                rivets(v, (x, s * (side_y(zg + 0.15) + 0.001), zg + 0.15), (x, s * (side_y(zr - 0.15) + 0.001), zr - 0.15), sn, 0.115)
            v.mark("JAP", (0.95, s * side_y(1.20), 1.20), Vector((0, s, 0.06)), 0.12, part="hull")
            v.port(Vector((0.85, s * (side_y(1.26) + 0.004), 1.26)), sn, 0.035)

    # ---- the track guards, the engine deck sloping to the back, the silencer on the right rear guard -------------
    for s in (LEFT, RIGHT):
        y0, y1 = s * hw, s * 1.03
        v.slab((-2.14, y0, zg - 0.016), (1.96, y1, zg), coat, 0.004)
        v.plate([(1.96, y0, zg), (1.96, y1, zg), (2.20, y1, 0.70), (2.20, y0, 0.70)][::int(s)], 0.01, "upper", 0.003)
        if fine:
            for x in (-1.7, -0.6, 0.5, 1.5):
                v.slab((x - 0.012, s * hw, zg - 0.09), (x + 0.012, s * 1.00, zg - 0.016), "shade")
        v.socket("exhaust." + ("1" if s > 0 else "2"), (-2.04, -0.90, zg + 0.10), (-1.0, -0.3, 0.0))
    v.silencer((-1.02, -0.88, zg + 0.095), (-1.86, -0.88, zg + 0.095), 0.075, bands=2, tail=((-1.86, -0.88, zg + 0.095), (-1.0, -0.3, 0.0)))
    v.tube([(-0.86, -0.80, zg + 0.24), (-0.92, -0.88, zg + 0.14), (-1.02, -0.88, zg + 0.095)], 0.030, "rust", v.n(10, 5))
    v.hull([(-0.55, 0.94, zg), (-0.55, 0.92, 1.42), (-1.92, 0.86, 1.20), (-2.12, 0.90, zg)], coat, bevel=0.012)
    dk = Vector((-1.92 + 0.55, 0, 1.20 - 1.42)).normalized()           # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))
    d_at = lambda u, y: Vector((-0.55, y, 1.42)) + dk * u + dn * 0.002         # noqa: E731
    for s in (LEFT, RIGHT):
        v.box((1.10, 0.56, 0.018), d_at(0.66, s * 0.34) + dn * 0.009, "upper", 0.005, turn=turn_to(dk, dn))
        v.louvres(d_at(0.44, s * 0.34) + dn * 0.018, (0.40, 0.52), dn, count=5, down=dk)
        v.louvres(d_at(0.94, s * 0.34) + dn * 0.018, (0.40, 0.42), dn, count=4, down=dk)
        if fine:
            v.hinge(d_at(0.20, s * 0.04) + dn * 0.018, d_at(0.30, s * 0.04) + dn * 0.018, 0.010)
            v.hinge(d_at(1.02, s * 0.04) + dn * 0.018, d_at(1.12, s * 0.04) + dn * 0.018, 0.010)
            rivets(v, d_at(0.05, s * 0.82), d_at(1.34, s * 0.78), dn, 0.12)
    v.shovel((0.2, 1.00, zg + 0.02), 0.0, 0.85)
    v.crowbar((0.1, -1.01, zg + 0.018), 0.0, 1.1)
    v.bin((-1.05, 0.70, zg), (-0.60, 1.00, zg + 0.15), lid_side=Y)

    # ---- the running gear: four wheels a side in two pairs on bell cranks, the springs in tubes along the hull ----
    ty, r_w, thick = 0.86, 0.24, 0.024
    wz = r_w + thick
    xs = [-1.30, -0.74, 0.58, 1.14]
    v.running(xs, r_w, ty, (1.86, 0.62, 0.25), (-1.92, 0.36, 0.23), rollers=[(-0.55, 0.80, 0.075), (0.55, 0.80, 0.075)], wheel_z=wz,
              wheel=dict(w=0.065, tyre=0.03, gap=0.06, bolts=6, sides=28, hub=0.3, axle=0.22),
              sprocket_kw=dict(teeth=14, wide=0.24, tooth=0.028, holes=6, bolts=8, sides=26, hub=0.42),
              idler_kw=dict(wide=0.22, kind="spoke", spokes=6),
              roller_kw=dict(wide=0.12, tyre=0.012, reach=0.24),
              track=dict(pitch=0.10, wide=0.25, thick=thick, sag=0.012, style={"kind": "plate", "horn": "centre", "horn_high": 0.05, "grouser": 0.011, "cleats": False, "bar": 0.3}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):
        yh, ya = s * (hw + 0.06), s * (ty - 0.17)
        v.rod((-1.40, yh, 0.58), (1.24, yh, 0.58), 0.05, "upper", v.n(14, 6))                   # the springs' long tube
        for x in (-1.40, 0.0, 1.24):
            v.slab((x - 0.05, s * (hw - 0.01), 0.51), (x + 0.05, yh + s * 0.03, 0.65), "shade", 0.008)
        for ca in (-1.02, 0.86):                             # each pair's bell crank: a pivot on the hull, an arm to each axle, a lever up to the spring
            v.rod((ca, s * (hw - 0.01), 0.42), (ca, ya, 0.42), 0.045, "shade", 10)
            for xw in (ca - 0.28, ca + 0.28):
                v.rod((ca, ya, 0.42), (xw, ya, wz), 0.034, "shade", 8)
            v.rod((ca, yh, 0.42), (ca, yh, 0.56), 0.028, "shade", 8)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((1.86, s * (hw - 0.01), 0.62), Y * s, [(0.14, 0.0), (0.14, 0.13), (0.07, 0.19), (0.0, 0.21), (0.0, 0.0)], "upper", v.n(20, 8))
        v.lathe((-1.92, s * (hw - 0.01), 0.36), Y * s, [(0.12, 0.0), (0.12, 0.07), (0.0, 0.1), (0.0, 0.0)], "upper", v.n(14, 6))

    # ---- the turret, set off to the left: rolled plate leaning in, a raised hood for the commander's lid ----------
    x0, y0, zt, th = 0.34, 0.18, zr, 0.54
    v.ring_well((x0, y0, zt), 0.58)
    v.part("turret", "hull", (x0, y0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    low = {"z": zt + 0.02, "xc": x0, "yc": y0, "af": 0.56, "ar": 0.60, "b": 0.56, "n": 2.3}
    high = {"z": zt + th - 0.025, "xc": x0 - 0.02, "yc": y0, "af": 0.46, "ar": 0.50, "b": 0.47, "n": 2.3}
    v.cast([low, high], "upper", sides=v.n(44, 14), top=0.025, steps=2, between=2)
    v.lathe((x0, y0, zt - 0.03), Z, [(0.06, 0.58), (0.0, 0.60), (0.0, 0.0)], "shade", v.n(30, 10))
    roof = zt + th
    mix = lambda t: {k: low[k] + (high[k] - low[k]) * t for k in low}        # noqa: E731
    ring_rivets(v, mix(0.10), zt + 0.02 + (th - 0.045) * 0.10, 38, part="turret")
    ring_rivets(v, mix(0.90), zt + 0.02 + (th - 0.045) * 0.90, 32, part="turret")
    cup = v.cupola((x0 - 0.12, y0, roof - 0.01), 0.25, 0.15, slits=4)
    v.hatch_round("hatch_commander", "turret", cup, 0.20, Z, -X, thick=0.025, opens=100.0)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    # the machine gun in the turret's back, to the right (the gunner-commander swung round to it)
    back = Vector((-0.80, -0.60, 0.0)).normalized()
    v.mg_ball(Vector((x0, y0, zt + 0.26)) + Vector((back.x * 0.60, back.y * 0.56, 0)), Vector((back.x, back.y, 0.10)).normalized(), 0.07, socket="turret_mg", length=0.28)
    v.use("turret")
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 + 0.04, y0 + s * 0.53, zt + 0.25)), Vector((0, s, 0.22)), 0.035)

    # ---- the gun: the 37 mm in a small round mantlet, a long thin barrel -----------------------------------------
    tx, tz = x0 + 0.46, zt + 0.26
    v.part("gun", "turret", (tx, y0, tz), axis=(0, -1, 0), limits=[-15.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx, y0, tz), Y, [(0.17, 0.0), (0.17, 0.13), (0.14, 0.16), (-0.14, 0.16), (-0.17, 0.13), (-0.17, 0.0)], "upper", v.n(26, 10))
    v.lathe((tx + 0.10, y0, tz), X, [(0.12, 0.0), (0.12, 0.085), (0.05, 0.10), (0.0, 0.105), (0.0, 0.0)], "upper", v.n(20, 8))
    v.part("barrel", "gun", (tx + 0.16, y0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.15], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.16, y0, tz), 1.06, 0.037, r0=0.050, r1=0.040, brake=None, sleeve=(0.22, 0.066))
    v.socket("muzzle", (tx + 1.24, y0, tz), X)
    v.m.info["bore"] = 0.037
    v.m.info["brake"] = False

    brush(v)                                          # (the three-colour patches: the art director, 22 Nov 20:23Z)

    # ---- sockets for what happens to her ------------------------------------------------------------------------
    v.socket("engine", d_at(0.7, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.45, 0.34) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.12, -0.50, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, y0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", f_at(0.25, -0.40), fn, part="hull")
    v.socket("hit.left", (0.3, 0.97, 1.20), Y, part="hull")
    v.socket("hit.right", (0.3, -0.97, 1.20), -Y, part="hull")
    v.socket("hit.rear", (-2.12, 0.0, 0.78), -X, part="hull")
    v.socket("hit.turret", (x0, y0 + 0.56, zt + 0.25), Y, part="turret")
    v.socket("case", (x0 - 0.2, y0 + 0.55, zt + 0.28), Y, part="turret", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.12, kick=0.35, gun_low=-15.0, hatches=["hatch_commander", "hatch_driver"], mg="hull_mg",
                 traverse=15.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.6, 1.8, 0.30, 160.0))
    v.name_place((0.3, 1.70), long=0.30, high=0.11, part="turret", writing="down", bend=0.03, script="japanese", high_one=0.13,
                 ink="white")                                 # (as the Type 97's: brush-written in white on the turret's side)
    jap_swaps(v)
    return v
