"""The Soviet heavy tank: the KV-1 of 1941 (welded turret) and 1942 (cast turret).

Modelled to the real type's published measurements and layout (6.68 m hull, 3.32 m over the tracks, 2.71 m high; six
road wheels a side on torsion bars with three return rollers, the drive at the back, the 76 mm gun in a big
turret with a machine gun in its back). Every shape is our own mesh, made from numbers here: nothing traced or taken
from any other model. One model for the war, by a swap: the welded turret of 1941 or the cast one of 1942.
"""

import math

from mathutils import Vector

from motorpool import LEFT, RIGHT, X, Y, Z, inset_plan, sym_plan
from tanks_ger import BY_MONTH_HOW, coat
from works import Works

# The KV-1's faces by the month: the welded turret to the end of 1941, then welded and cast side by side through 1942
# (the cast turret came in from Leningrad and the Urals over that year), cast alone once the welded ones were worn
# out. Shares medium-low: the historian is asked to check them before the art director sees the sheet.
KV1_MONTHS = [{"to": "1941-12", "turret": "welded"},
              {"from": "1942-01", "to": "1942-12", "turret": {"mix": {"welded": 0.5, "cast": 0.5}, "one": "cast"}},
              {"from": "1943-01", "turret": "cast"}]


def kv_hull(v, fine):
    """The KV's hull: a welded box built out over the tracks, a long sloped glacis with the driver's plate standing
    nearly upright over it, the engine deck flat to the back. Returns her measures."""
    v.root(hull_pivot=(0.0, 0.0, 0.85))
    hw, sw, zg, zr = 0.92, 1.30, 1.08, 1.84          # half-width between the tracks and of the upper hull; the guards; the roof
    xn, zn = 3.34, 0.82                               # the nose's edge, where the lower plate meets the glacis
    xd, zd = 2.70, 1.24                               # the glacis' head, where the driver's plate stands on it
    xt = 2.55                                         # the driver's plate's head: the roof begins here
    xg = xn - (zg - zn) / (zd - zn) * (xn - xd)       # where the glacis crosses the guards' height
    # ---- the tub between the tracks, the upper hull out over them ------------------------------------------------
    v.hull([(x, hw, z) for x, z in [(xn, zn), (xg, zg), (-3.34, zg), (-3.34, 0.86), (-3.05, 0.44), (2.80, 0.44)]], coat, bevel=0.016)
    v.hull([(xg, sw, zg), (xd, sw, zd), (xt, sw - 0.02, zr), (-2.70, sw - 0.02, zr), (-3.16, sw - 0.02, 1.56), (-3.34, sw, 1.24), (-3.34, sw, zg)], coat, bevel=0.018)
    gl = Vector((xd - xn, 0, zd - zn)).normalized()                  # up the glacis
    gn = Vector((gl.z, 0, -gl.x))                                    # out of it
    dl = Vector((xt - xd, 0, zr - zd)).normalized()                  # up the driver's plate
    dn = Vector((dl.z, 0, -dl.x))
    rb = Vector((-3.34 + 3.16, 0, 1.24 - 1.56)).normalized()          # down the sloped tail plate
    rn = Vector((-rb.z, 0, rb.x))
    if rn.x > 0:
        rn = -rn
    g_at = lambda u, y: Vector((xn, y, zn)) + gl * u + gn * 0.002    # noqa: E731  (a place on the glacis, u metres up it)
    d_at = lambda u, y: Vector((xd, y, zd)) + dl * u + dn * 0.002    # noqa: E731  (a place on the driver's plate)
    r_at = lambda u, y: Vector((-3.16, y, 1.56)) + rb * u + rn * 0.002   # noqa: E731  (a place on the tail plate)

    # the driver's plate: his visor block a little left of the middle, the bow gun in its ball on the right
    v.visor(d_at(0.30, 0.18) + dn * 0.004, dn, 0.40, 0.13, 0.11)
    v.lathe(d_at(0.28, -0.62) - dn * 0.02, dn, [(0.13, 0.0), (0.13, 0.10), (0.08, 0.16), (0.0, 0.20), (0.0, 0.0)], "upper", v.n(24, 8))
    v.mg_ball(d_at(0.28, -0.62) + dn * 0.11, dn, 0.085, socket="hull_mg", length=0.36, ring=False)
    v.headlamp(d_at(0.42, 0.95) + dn * 0.06, X, 0.075, stalk=0.06)
    if fine:
        v.weld(g_at(0.02, -1.2), g_at(0.02, 1.2), gn)
        v.weld(d_at(0.01, -1.2), d_at(0.01, 1.2), dn)
        for s in (LEFT, RIGHT):                          # the welds down the upper hull's corners and along its roof
            out = Vector((0.0, s, 0.0))
            v.weld(Vector((xt + 0.01, s * (sw - 0.03), zr - 0.02)), Vector((-2.70, s * (sw - 0.03), zr - 0.02)), (out + Z).normalized())
            v.weld(Vector((xd, s * (sw + 0.004), zd)), Vector((xt, s * (sw - 0.016), zr)), (out + X).normalized())
            v.weld(Vector((xg, s * (sw + 0.004), zg + 0.01)), Vector((xd, s * (sw + 0.004), zd)), (out + gn).normalized())
        for x0_, x1_ in ((-2.58, -1.06), (-1.06, 0.40)):  # the bolted plates of the engine deck and the roof over the crew
            for y in (-0.42, 0.42):
                for k in range(7):
                    x = x0_ + (x1_ - x0_) * (k + 0.5) / 7
                    v.ball((x, y, zr + 0.004), 0.016, "upper", 8, 4, dome=True, squash=0.5)
    for s in (LEFT, RIGHT):
        v.hook(Vector((xn + 0.02, s * 0.62, zn - 0.06)), Vector((0.8, 0, -0.5)).normalized(), 0.11)
        v.hook(Vector((-3.36, s * 0.66, 0.80)), -X, 0.11)
        # the track guards: a shelf each side the length of her, bent down over the idler at the front and the
        # sprocket at the back, two bins and the tow cable on each
        y0, y1 = s * (sw - 0.01), s * 1.68
        v.slab((-3.30, y0, zg - 0.016), (3.12, y1, zg), coat, 0.004)
        v.plate([(3.12, y0, zg), (3.12, y1, zg), (3.44, y1, 0.90), (3.44, y0, 0.90)][::int(s)], 0.012, "upper", 0.003)
        v.plate([(-3.30, y1, zg), (-3.30, y0, zg), (-3.50, y0, 0.92), (-3.50, y1, 0.92)][::int(s)], 0.012, "upper", 0.003)
        if fine:
            v.slab((-3.30, s * 1.665, zg - 0.05), (3.12, s * 1.68, zg), "upper")
            for x in (-2.4, -1.2, 0.0, 1.2, 2.4):
                v.slab((x - 0.014, s * (sw - 0.01), zg - 0.10), (x + 0.014, s * 1.64, zg - 0.016), "shade")
        v.bin((-2.95, s * 1.36, zg), (-1.85, s * 1.64, zg + 0.30), lid_side=Y * s)
        v.bin((1.55, s * 1.36, zg), (2.40, s * 1.64, zg + 0.26), lid_side=Y * s)
    # the roof: the driver's periscope lid ahead of the turret; the engine deck behind it: a long armoured louvre
    # down the middle, a round hatch over the engine, grilles each side, the plate turning down to the tail
    v.periscope((2.30, 0.20, zr), X, (0.10, 0.15, 0.10))
    v.hatch_rect("hatch_driver", "hull", (2.12, 0.20, zr), (0.36, 0.50), Z, X, hinge="back", opens=100.0, thick=0.04)
    v.hatch_rect("hatch_radio", "hull", (2.12, -0.55, zr), (0.36, 0.46), Z, X, hinge="back", opens=100.0, thick=0.04)
    v.use("hull")
    v.slab((-2.62, -0.36, zr), (-1.02, 0.36, zr + 0.07), "upper", 0.012)
    v.grille((-1.82, 0.0, zr + 0.07), (1.44, 0.56), Z, X, slats=10, deep=0.025)
    for s in (LEFT, RIGHT):
        v.slab((-2.60, s * 0.46, zr), (-1.24, s * 1.12, zr + 0.04), "upper", 0.01)
        v.grille((-1.92, s * 0.79, zr + 0.04), (1.20, 0.52), Z, X, slats=8, deep=0.02, mesh=True)
    if fine:
        for x in (-2.5, -1.2):
            v.handle((x, -0.12, zr + 0.07), (x, 0.12, zr + 0.07), Z, 0.008, 0.04, "steel")
    # the tail: the transmission's round door, two exhausts under armoured cowls
    v.lathe(r_at(0.18, 0.0), rn, [(0.03, 0.0), (0.03, 0.28), (0.0, 0.31)], "upper", v.n(26, 8))
    if fine:
        v.bolts(r_at(0.18, 0.0) + rn * 0.03, rn, 0.22, 10, r=0.012)
    for i, y in enumerate((0.70, -0.70)):
        v.lathe((-3.36, y, 1.00), -X, [(0.13, 0.0), (0.13, 0.10), (0.09, 0.16), (0.0, 0.18), (0.0, 0.0)], "upper", v.n(18, 6))
        v.tube([(-3.40, y, 1.00), (-3.52, y, 0.98), (-3.60, y, 0.92)], 0.05, "rust", v.n(10, 5))
        v.socket(f"exhaust.{i + 1}", (-3.62, y, 0.90), (-1, 0, -0.3))
    # along the sides: tow cables on the upper hull's walls, a crowbar and a saw
    if fine:
        for s in (LEFT, RIGHT):
            v.cable([(2.2, s * (sw + 0.01), 1.50), (0.4, s * (sw + 0.01), 1.46), (-1.6, s * (sw + 0.01), 1.50)], 0.016)
            for xa, xb in ((1.70, 0.90), (-0.60, -1.40)):
                v.handle(Vector((xa, s * (sw - 0.02), 1.74)), Vector((xb, s * (sw - 0.02), 1.74)), Y * s, 0.011, 0.06)
        v.crowbar((-0.4, -1.52, zg + 0.02), 0.0, 1.3)
    v.aerial("aerial", "hull", (1.60, 1.20, zr), 2.2)

    # ---- the running gear: six wheels a side on torsion bars, three return rollers, the drive at the back --------
    ty, r_w = 1.31, 0.30
    xs = [-2.20, -1.34, -0.48, 0.38, 1.24, 2.10]
    v.running(xs, r_w, ty, (-3.02, 0.58, 0.33), (2.98, 0.62, 0.31), rollers=[(-1.75, 0.88, 0.11), (-0.15, 0.88, 0.11), (1.45, 0.88, 0.11)], drive="rear",
              wheel=dict(w=0.12, tyre=0.05, gap=0.12, bolts=10, sides=36, hub=0.30, axle=0.24, holes=0),
              sprocket_kw=dict(teeth=16, wide=0.40, tooth=0.03, holes=6, bolts=10, sides=32, hub=0.44),
              idler_kw=dict(wide=0.38, kind="dish", spokes=0, tyre=0.0, hub=0.42),
              roller_kw=dict(wide=0.18, tyre=0.02, reach=0.32),
              track=dict(pitch=0.16, wide=0.70, thick=0.04, sag=0.016, style={"kind": "plate", "horn": "centre", "horn_high": 0.10, "grouser": 0.016, "cleats": True}))
    v.use("vehicle")
    if fine:
        for s in (LEFT, RIGHT):
            for x in xs:                                 # each wheel's arm, trailing from its torsion bar's end on the hull's side
                v.rod((x + 0.34, s * (hw + 0.01), 0.42), (x + 0.34, s * (ty - 0.24), 0.42), 0.055, "shade", 10)
                v.rod((x + 0.34, s * (ty - 0.24), 0.42), (x, s * (ty - 0.24), r_w), 0.045, "shade", 8)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((-3.02, s * (hw - 0.01), 0.58), Y * s, [(0.18, 0.0), (0.18, 0.18), (0.09, 0.27), (0.0, 0.30), (0.0, 0.0)], "upper", v.n(22, 8))
        v.lathe((2.98, s * (hw - 0.01), 0.62), Y * s, [(0.13, 0.0), (0.13, 0.10), (0.0, 0.14), (0.0, 0.0)], "upper", v.n(16, 6))

    # sockets every KV has
    v.socket("engine", (-1.82, 0.0, zr + 0.10), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-1.90, -0.79, zr + 0.06), Z, part="hull")
    v.socket("fire.3", (2.30, 0.20, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("hit.front", g_at(0.40, -0.2), gn, part="hull")
    v.socket("hit.left", (0.2, sw, 1.45), Y, part="hull")
    v.socket("hit.right", (0.2, -sw, 1.45), -Y, part="hull")
    v.socket("hit.rear", r_at(0.18, 0.0), rn, part="hull")
    return {"hw": hw, "sw": sw, "zg": zg, "zr": zr}


def _welded_turret(v, x0, zt, th):
    """The welded turret of 1941: flat plates leaning in, the front cut back at its corners, the back overhanging."""
    plan = sym_plan([(0.86, 0.60), (0.62, 0.98), (-0.98, 0.98), (-1.36, 0.58)])
    top = inset_plan(plan, [0.16, 0.17, 0.14, 0.12, 0.14, 0.17, 0.16, 0.07])
    v.prism([(x0 + x, y) for x, y in plan], zt + 0.02, zt + th, coat, top=[(x0 + x, y) for x, y in top], bevel=0.035, segs=3, angle=14.0)
    if v.fine:
        for s in (LEFT, RIGHT):                          # the welds where the plates meet, standing proud at the corners
            n = Vector((0.0, s * 0.98, 0.20)).normalized()
            v.weld(Vector((x0 + 0.60, s * 0.97, zt + 0.06)), Vector((x0 + 0.47, s * 0.82, zt + th - 0.03)), n)
            v.weld(Vector((x0 - 0.96, s * 0.97, zt + 0.06)), Vector((x0 - 0.86, s * 0.84, zt + th - 0.03)), n)


def _cast_turret(v, x0, zt, th):
    """The cast turret of 1942: the same room in one rounded casting, thickest at the front."""
    v.cast([{"z": zt + 0.02, "xc": x0 - 0.25, "af": 1.11, "ar": 1.12, "b": 1.00, "n": 3.0},
            {"z": zt + 0.40, "xc": x0 - 0.25, "af": 1.10, "ar": 1.12, "b": 0.97, "n": 2.9},
            {"z": zt + th - 0.06, "xc": x0 - 0.25, "af": 0.98, "ar": 1.00, "b": 0.82, "n": 2.6}], "upper", sides=v.n(52, 14), top=0.08)
    if v.fine:
        for s in (LEFT, RIGHT):                          # the casting's seam, low round her sides
            v.weld(Vector((x0 + 0.70, s * 0.99, zt + 0.10)), Vector((x0 - 1.20, s * 0.99, zt + 0.10)), Vector((0, s, 0)), r=0.006)


def kv1(fine=True, tag="SOV", turret="welded"):
    """KV-1: 6.68 m hull, 3.32 m wide, 2.71 m high, 45 tonnes. A box of thick plate over six wheels a side, the big
    turret on top with the 76 mm gun, a machine gun in the bow and another in the turret's back. One model for the war
    by a swap: turret "welded" (1941), "cast" (1942 on), or "all" (both, for the game's file, which shows one)."""
    v = Works("KV1", "KV-1", "tank", tag, fine)
    # (the E7 surface pass: shade from her shape, dirt of her ground, as the SU-76; the art director, 22 Nov 17:55Z)
    v.dice = 0.2
    v.worn_kw = dict(shade=True, shade_deep=0.42, ground="steppe", layers=2, marks=0.3, gear=0.8, rim=True, cast=lambda p, part: part in ("turret_cast", "gun"))
    h = kv_hull(v, fine)
    m = v.m
    zr = h["zr"]
    x0, zt, th = 0.15, zr, 0.84
    v.ring_well((x0, 0.0, zt), 0.80)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.84), (0.0, 0.86), (0.0, 0.0)], "shade", v.n(36, 12))
    shells = []
    if turret in ("welded", "all"):
        v.part("turret_welded", "turret", (x0, 0.0, zt), kind="fixed", note="the welded turret of 1941")
        _welded_turret(v, x0, zt, th)
        shells.append("turret_welded")
    if turret in ("cast", "all"):
        v.part("turret_cast", "turret", (x0, 0.0, zt), kind="fixed", note="the cast turret of 1942 on")
        _cast_turret(v, x0, zt, th)
        shells.append("turret_cast")
    v.use("turret")
    roof = zt + th
    # the roof: one big round lid at the back, the commander's panoramic sight on the left, the gunner's periscope,
    # a ventilator dome over the gun's breech
    v.hatch_round("hatch_commander", "turret", (x0 - 0.62, -0.18, roof), 0.32, Z, X, thick=0.035, opens=100.0)
    v.socket("commander", (x0 - 0.62, -0.18, roof - 0.25), X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.lathe((x0 + 0.30, 0.48, roof), Z, [(0.09, 0.0), (0.09, 0.08), (0.06, 0.10), (0.06, 0.20), (0.0, 0.22)], "upper", v.n(18, 8))   # the panoramic sight in its hood
    v.periscope((x0 + 0.42, -0.44, roof), X, (0.10, 0.14, 0.11))
    v.vent((x0 - 0.02, 0.02, roof), 0.13, 0.06)
    # the machine gun in the turret's back, a little right of the middle
    v.mg_ball(Vector((x0 - 1.34, -0.26, zt + 0.44)), -X, 0.085, length=0.30, ring=True)
    v.socket("rear_mg", (x0 - 1.52, -0.26, zt + 0.44), -X, part="turret", note="the machine gun in the turret's back")
    if fine:
        for s in (LEFT, RIGHT):
            n = Vector((0, s * 0.97, 0.22)).normalized()
            v.port(Vector((x0 - 0.30, s * 0.965, zt + 0.32)), n, 0.045)
            v.handle(Vector((x0 + 0.30, s * 0.93, zt + 0.62)), Vector((x0 - 0.60, s * 0.93, zt + 0.62)), n, 0.011, 0.06)
        for at in ((x0 + 0.55, 0.40), (x0 + 0.55, -0.40), (x0 - 1.05, 0.50), (x0 - 1.05, -0.55)):
            v.lift_eye((at[0], at[1], roof), Z, Y, 0.035)
    for s in (LEFT, RIGHT):
        v.mark("SOV", (x0 - 0.62, s * 0.95, zt + 0.46), Vector((0.0, s, 0.18)), 0.16, part="turret")

    # ---- the gun: the 76 mm ZiS-5 behind a big rounded mantlet -------------------------------------------------
    tx, tz = x0 + 0.84, zt + 0.40
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-5.0, 25.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx + 0.03, 0, tz), Y, [(0.40, 0.0), (0.40, 0.25), (0.34, 0.30), (-0.34, 0.30), (-0.40, 0.25), (-0.40, 0.0)], "upper", v.n(36, 12))   # the mantlet, a drum across her face
    v.lathe((tx + 0.22, 0, tz), X, [(0.0, 0.0), (0.0, 0.16), (0.30, 0.15), (0.40, 0.12), (0.40, 0.0)], "upper", v.n(26, 8))                  # the barrel's armoured sleeve
    if fine:
        v.bolts((tx + 0.33, 0, tz), X, 0.20, 6, r=0.013)
    v.mg((tx + 0.24, -0.24, tz - 0.04), X, 0.16)
    v.socket("coax", (tx + 0.42, -0.24, tz - 0.04), X)
    v.part("barrel", "gun", (tx + 0.62, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.40], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.62, 0, tz), 1.92, 0.0762, r0=0.088, r1=0.062, brake=None, sleeve=None)
    v.socket("muzzle", (tx + 2.55, 0, tz), X)
    m.info["bore"] = 0.0762
    m.info["brake"] = False

    v.socket("fire.2", (x0 - 0.62, -0.18, roof + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.turret", (x0 - 0.1, 0.98, zt + 0.42), Y, part="turret")
    v.socket("case", (x0 - 0.62, -0.18, roof + 0.05), Z, part="turret", note="where the empty case is thrown out (through the roof hatch: the KV had no port for it)")
    v.tank_clips(recoil=0.38, kick=0.7, gun_low=-5.0, hatches=["hatch_commander", "hatch_driver", "hatch_radio"],
                 traverse=12.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-1.0, 3.2, 0.42, 172.0))
    v.name_place((x0 - 0.30, zt + 0.26), long=1.0, high=0.12, part="turret", lines=2, bend=0.03 if turret != "welded" else 0.014,
                 parts=shells)                       # the slogan (b477): on the turret's side, under the hand rail
    if turret == "all":
        m.info.setdefault("file", {})["swaps"] = {
            "how": "one model for the whole war. turret: show the chosen turret's part and scale the other's to nothing (a part scaled to "
                   "nothing is not drawn); every fitting on the turret's roof and sides serves both. " + BY_MONTH_HOW,
            "turret": {"default": "welded", "choices": {
                "welded": {"what": "the welded turret of flat plates", "years": "1939-12 to 1942", "parts": ["turret_welded"]},
                "cast": {"what": "the cast turret, one rounded casting", "years": "1942 on (beside the welded one through 1942)", "parts": ["turret_cast"]}}},
            "by_month": KV1_MONTHS}
    return v
