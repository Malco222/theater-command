"""Assault guns and tank destroyers: a tank's hull with a bigger gun than its turret could carry, set in the hull's own
front (it trains a few degrees each way; the driver turns her for more) or in an open-topped turret.

Each is modelled to the real type's measurements and layout from the published dimensions; every shape is our own
mesh, made from numbers here. They keep the tanks' part and socket names, so the game drives them the same way:
`turret` is the gun's own traverse in its mounting where there is no turret (its limits say how far), `gun` lifts.
"""

import math

from mathutils import Vector

from carriers import wall
from motorpool import LEFT, RIGHT, X, Y, Z
from tanks_any import turn_to
from tanks_ger import BY_MONTH_HOW, GER_COATS, coat, coats, hull_skirts
from tanks_sov import t34_hull
from tanks_usa import m4_gear, m4_tub
from works import Works


# The StuG III's faces by the month (the historian, 18 Nov: she reached units in 1942-04; dark yellow by the order of
# February 1943; skirts with the Panzer IV's; the cast shield was made from November 1943 and reached the front about
# two months on, and casting never kept up, so the box stayed on a large share to the end: half and half is a fair
# guess, low on the share, high that both served together).
STUG3_MONTHS = [{"to": "1943-01", "paint": "grey", "mantlet": "box", "skirts": "off"},
                {"from": "1943-02", "to": "1943-04", "paint": "yellow", "mantlet": "box", "skirts": "off"},
                {"from": "1943-05", "to": "1943-12", "paint": "yellow", "mantlet": "box", "skirts": "on"},
                {"from": "1944-01", "paint": "yellow", "mantlet": {"mix": {"cone": 0.5, "box": 0.5}, "one": "cone"}, "skirts": "on"}]


def stug3(fine=True, tag="GER", paint=None, mantlet="box", skirts=None):
    """Sturmgeschuetz III of 1942 on: 5.4 m hull, 2.95 m wide, only 2.16 m high. The Panzer III's hull (six wheels a
    side on torsion bars, three return rollers, the drive at the front) under a low fixed box for four men, its sides
    built out over the tracks, extra plates bolted on its front; the long 7.5 cm gun a little right of her middle;
    the commander's cupola at the back on the left, the loader's machine gun behind a folding shield on the right.
    One model for the war, by swaps: paint "grey" (to early 1943) or "yellow" (after); mantlet "box", the welded
    box shield round the gun (her only one to the end of 1943, and on about half of them after), or "cone", the
    cast shield shaped like a boar's head of 1944 on ("all" builds both for the game's file, which shows one);
    skirts (May 1943 on, with the yellow coat): True hangs them all, a string names the plates this one has lost
    ("l2,r4"). She keeps the cupola and the loader's shield of December 1942 on through all her months."""
    v = Works("StuG3", "StuG III", "tank", tag, fine, paint=GER_COATS[paint or "grey"][0])
    v.root(hull_pivot=(0.0, 0.0, 0.72))
    m = v.m
    hw, zg, zr, ze = 0.92, 1.06, 1.86, 1.44          # half-width between the tracks; the guards; the roof; the engine deck

    # ---- the tub, the fighting box with its sides built out over the tracks, the engine's part ---------------
    tub = [(-2.55, 0.42), (-2.70, 0.66), (-2.70, zg), (1.58, zg), (2.40, 0.97), (2.66, 0.66), (2.28, 0.39), (-2.35, 0.39)]
    v.hull([(x, hw, z) for x, z in tub], coat, bevel=0.014)
    v.hull([(1.50, 0.90, zg), (1.50, 0.90, 1.46), (1.02, 0.78, zr), (0.86, 1.12, zr), (-0.92, 1.12, zr), (1.16, 1.43, 1.10), (-0.92, 1.43, 1.10), (-0.92, 0.93, zg)], coat, bevel=0.014)
    v.hull([(-0.92, 0.93, zg), (-0.92, 0.93, ze), (-2.38, 0.93, ze), (-2.70, 0.93, 1.24), (-2.70, 0.93, zg)], coat, bevel=0.014)
    for s in (LEFT, RIGHT):
        y0, y1 = s * 0.93, s * 1.46
        v.slab((-2.78, y0, zg - 0.015), (2.56, y1, zg), coat, 0.004)
        v.plate([(2.56, y0, zg), (2.56, y1, zg), (2.86, y1, 0.86), (2.86, y0, 0.86)][::int(s)], 0.012, "upper", 0.003)
        v.plate([(-2.78, y1, zg), (-2.78, y0, zg), (-2.98, y0, 0.82), (-2.98, y1, 0.82)][::int(s)], 0.012, "upper", 0.003)
        if fine:
            for x in (-2.2, -1.2, 0.0, 1.0, 2.0):          # the brackets under the guards, the lip along their edge
                v.slab((x - 0.015, s * 0.93, 0.95), (x + 0.015, s * 1.42, zg - 0.015), "shade")
            v.slab((-2.78, s * 1.445, zg - 0.04), (2.56, s * 1.46, zg), "upper")
    # the nose: two bolted brake hatches on the glacis, spare links across the plate under it, towing hooks
    gl = Vector((2.40 - 1.58, 0, 0.97 - zg)).normalized()             # forward down the glacis
    gn = Vector((-gl.z, 0, gl.x))                                     # up out of it
    g_at = lambda u, y: Vector((1.58, y, zg)) + gl * u + gn * 0.002       # noqa: E731
    for y in (-0.50, 0.50):
        c = g_at(0.42, y)
        v.box((0.50, 0.46, 0.022), c + gn * 0.011, "upper", 0.006, turn=turn_to(gl, gn))
        if fine:
            for dx in (-0.2, 0.2):
                for dy in (-0.18, 0.18):
                    v.bolt(c + gl * dx + Y * dy + gn * 0.022, gn, 0.012)
            v.handle(c + gl * 0.08 - Y * 0.08 + gn * 0.022, c + gl * 0.08 + Y * 0.08 + gn * 0.022, gn, 0.007, 0.03, "steel")
    nf = Vector((0.31, 0, 0.26)).normalized()
    v.spare_links(Vector((2.53, -0.72, 0.815)) + nf * 0.002, Vector((2.53, 0.72, 0.815)) + nf * 0.002, 12, 0.12, 0.30, nf, style={"kind": "plate", "grouser": 0.014})
    for y in (-0.62, 0.62):
        v.hook((2.47, y, 0.52), Vector((0.8, 0, -0.5)).normalized(), 0.085)
        v.hook((-2.70, y, 0.60), -X, 0.085)
    # the box's front: an extra plate bolted on each side of the gun, the driver's visor in the left one
    for y0, y1, ys_ in ((0.39, 0.87, (0.45, 0.63, 0.81)), (-0.87, -0.59, (-0.80, -0.66))):
        v.slab((1.50, y0, 1.085), (1.53, y1, 1.45), coat, 0.005)
        for y in ys_:
            for z in ((1.125, 1.41) if y0 > 0 else (1.125, 1.27, 1.41)):
                v.bolt((1.53, y, z), X, 0.021, 0.016, sides=8)
    v.visor((1.532, 0.52, 1.30), X, 0.36, 0.13, 0.07)
    if fine:
        v.weld((1.50, 0.34, 1.462), (1.50, 0.88, 1.462), X)
        for s in (LEFT, RIGHT):                           # a pistol port in each side of the box
            v.port((0.1, s * 1.30, 1.42), Vector((0, s * 0.93, 0.38)), 0.045)

    # ---- the gun: its armoured housing in the box's front, the cast shield, the long 7.5 cm --------------------
    gx, gy, gz = 1.24, -0.10, 1.50
    v.hull([(1.52, gy + e * 0.46, 1.20) for e in (1, -1)] + [(1.52, gy + e * 0.46, 1.52) for e in (1, -1)] + [(1.32, gy + e * 0.44, 1.80) for e in (1, -1)]
           + [(1.05, gy + e * 0.46, 1.20) for e in (1, -1)] + [(1.05, gy + e * 0.44, 1.80) for e in (1, -1)], coat, bevel=0.014, both=False)
    v.part("turret", "hull", (gx, gy, gz), axis=(0, 0, -1), limits=[-10.0, 10.0], kind="turn",
           note="the gun's own traverse in its mounting, ten degrees each way: + to the right; for more the game turns the whole of her")
    v.slab((1.50, gy - 0.31, gz - 0.25), (1.56, gy + 0.31, gz + 0.25), coat, 0.012)
    v.part("gun", "turret", (gx, gy, gz), axis=(0, -1, 0), limits=[-6.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")

    def ring(x, half_w, half_h, cut):
        return [(x, gy + sy * a, gz + sz * b) for sy in (1, -1) for sz in (1, -1) for a, b in ((half_w, half_h - cut), (half_w - cut, half_h))]
    # two shields for the one mounting: each is its own part on the gun, and the game shows one of them
    if mantlet in ("all", "box"):
        v.part("mantlet_box", "gun", (1.55, gy, gz), kind="fixed", note="the welded box shield round the gun, the recoil gear's sleeve standing out of its face: her only one to the end of 1943")
        v.hull(ring(1.55, 0.35, 0.28, 0.05) + ring(1.94, 0.255, 0.215, 0.045), coat, bevel=0.008, both=False)
        v.lathe((1.94, gy, gz), X, [(0.28, 0.0), (0.28, 0.112), (0.19, 0.112), (0.19, 0.134), (0.0, 0.134), (0.0, 0.0)], coat, v.n(24, 8))
        for sy in (1, -1):
            for sz in (1, -1):
                v.bolt((1.94, gy + sy * 0.205, gz + sz * 0.165), X, 0.017, 0.013, sides=8)
        if fine:
            for sy in (1, -1):                             # the welds along its upper edges
                v.weld((1.56, gy + sy * 0.30, gz + 0.281), (1.93, gy + sy * 0.21, gz + 0.216), Z)
    if mantlet in ("all", "cone"):
        v.part("mantlet_cone", "gun", (1.55, gy, gz), kind="fixed", note="the cast shield shaped like a boar's head: 1944 on, beside the box")
        v.hull(ring(1.55, 0.33, 0.29, 0.10) + ring(1.88, 0.25, 0.23, 0.09) + ring(2.20, 0.125, 0.125, 0.045), coat, bevel=0.03, segs=3, both=False, angle=40.0)
        if fine:
            v.lathe((2.20, gy, gz), X, [(0.03, 0.0), (0.03, 0.118), (0.0, 0.13), (0.0, 0.0)], coat, v.n(24, 8))
    v.part("barrel", "gun", (2.22, gy, gz), slide=(-1, 0, 0), slide_limits=[0.0, 0.42], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((2.22, gy, gz), 1.84, 0.075, r0=0.098, r1=0.066, brake="double", steps=((0.30, None),), sleeve=None)
    v.socket("muzzle", (4.08, gy, gz), X)
    m.info["bore"] = 0.075
    m.info["brake"] = True

    # ---- the roof: the commander's cupola, the loader's hatch and his machine gun behind its shield -----------
    v.use("hull")
    cup = v.cupola((-0.42, 0.62, zr), 0.30, 0.25, slits=7, start=0.0)
    v.hatch_round("hatch_commander", "hull", cup, 0.22, Z, -X, thick=0.03, opens=105.0)
    v.socket("commander", cup + Z * -0.25, X, part="hull", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "hull", (-0.46, -0.60, zr), (0.46, 0.50), Z, X, hinge="back", opens=100.0)
    v.use("hull")
    sx = -0.10
    v.plate([(sx, -0.86, zr), (sx, -0.34, zr), (sx - 0.09, -0.37, zr + 0.36), (sx - 0.09, -0.83, zr + 0.36)], 0.012, "upper", 0.003)
    v.mg((sx - 0.27, -0.60, zr + 0.22), X, 0.8)
    v.socket("mg", (sx + 0.55, -0.60, zr + 0.22), X, part="hull", note="the loader's machine gun, fired over the roof from behind its shield")
    v.periscope((0.52, 0.40, zr), X, (0.08, 0.14, 0.09))
    v.vent((0.25, -0.05, zr), 0.1, 0.045)
    if fine:
        for s in (LEFT, RIGHT):
            v.lift_eye((0.72, s * 0.86, zr), Z, X, 0.03)
            v.lift_eye((-0.82, s * 1.0, zr), Z, X, 0.03)
        v.weld((0.86, -1.1, zr + 0.002), (0.86, -0.8, zr + 0.002), Z)
        v.weld((0.86, 0.8, zr + 0.002), (0.86, 1.1, zr + 0.002), Z)
    v.aerial("aerial", "hull", (-0.86, 1.04, zr), 2.0)
    for s in (LEFT, RIGHT):
        v.mark("GER", (-0.25, s * 1.234, 1.58), Vector((0, s * 0.926, 0.378)), 0.17, part="hull")       # (high on the box's side: it shows over her skirts)

    # ---- the engine deck: four doors, an air intake's cowl each side; two silencers across the tail -------------
    for s in (LEFT, RIGHT):
        v.slab((-1.66, s * 0.06, ze), (-1.00, s * 0.82, ze + 0.02), "upper", 0.006)
        v.slab((-2.34, s * 0.06, ze), (-1.74, s * 0.82, ze + 0.02), "upper", 0.006)
        v.grille((-2.04, s * 0.44, ze + 0.02), (0.44, 0.56), Z, X, slats=6, deep=0.02)
        if fine:
            v.handle((-1.33, s * 0.70, ze + 0.02), (-1.33, s * 0.52, ze + 0.02), Z, 0.008, 0.04, "steel")
            for x in (-1.5, -1.16):
                v.hinge((x - 0.05, s * 0.07, ze + 0.02), (x + 0.05, s * 0.07, ze + 0.02), 0.013)
        v.slab((-2.30, s * 0.93, 1.10), (-1.08, s * 1.27, 1.36), "upper", 0.014)
        v.louvres((-1.69, s * 1.271, 1.23), (1.0, 0.18), Y * s, 4)
    for i, s in enumerate((LEFT, RIGHT)):
        v.silencer((-2.84, s * 0.64, 0.86), (-2.84, s * 0.14, 0.86), 0.10, bands=2, tail=((-2.84, s * 0.64, 0.86), (-0.4, s, -0.3)))
        v.socket(f"exhaust.{i + 1}", (-2.88, s * 0.76, 0.84), (-0.4, s, -0.3))
        if fine:
            v.slab((-2.76, s * 0.38 - 0.02, 0.74), (-2.70, s * 0.38 + 0.02, 0.98), "upper")
            v.wheel_disc((-2.16, s * 0.44, ze + 0.07), Z, 0.26, 0.085, 0.035, bolts=6, sides=24)       # a spare road wheel lying on each after door
    # lamps, tools and stowage on the guards
    v.headlamp((2.30, 1.14, zg), X, 0.06, stalk=0.07, cover=True)
    v.jack((1.75, -1.24, zg), 0.0)
    v.jack_block((-2.40, 1.22, zg), (0.4, 0.24, 0.13))
    v.shovel((1.30, 1.32, zg + 0.022), 0.0, 0.92)
    v.crowbar((-2.6, -1.36, zg + 0.018), 0.0, 1.3)
    v.axe((1.3, 1.16, zg + 0.02), 0.0, 0.75, roll=90.0)
    if fine:
        v.cable([(2.2, -1.36, zg + 0.02), (1.3, -1.38, zg + 0.018), (1.22, -1.30, zg + 0.018), (1.3, -1.10, zg + 0.018), (2.1, -1.08, zg + 0.02)], 0.013)
        v.rod((-2.5, 1.02, zg + 0.07), (-2.2, 1.02, zg + 0.07), 0.06, "red", 12)                           # the fire extinguisher

    # ---- the running gear: six wheels a side on torsion bars, three return rollers, the drive at the front ------
    ty, r_w = 1.255, 0.26
    xs = [-1.43 + 0.572 * i for i in range(6)]
    v.running(xs, r_w, ty, (2.26, 0.64, 0.32), (-2.30, 0.58, 0.30), rollers=[(-1.2, 0.855, 0.155), (0.0, 0.855, 0.155), (1.2, 0.855, 0.155)],
              wheel=dict(w=0.085, tyre=0.035, gap=0.075, bolts=6, sides=32, hub=0.36),
              sprocket_kw=dict(teeth=21, wide=0.24, tooth=0.03, holes=6, sides=30),
              idler_kw=dict(wide=0.24, kind="spoke", spokes=8),
              roller_kw=dict(wide=0.17, tyre=0.02, reach=0.26),
              track=dict(pitch=0.12, wide=0.40, thick=0.028, style={"kind": "plate", "horn": "centre", "horn_high": 0.075, "grouser": 0.014}))
    v.use("vehicle")
    if fine:
        for s in (LEFT, RIGHT):
            for x in xs:                                   # each wheel's arm, trailing from its torsion bar in the hull's side
                v.rod((x + 0.26, s * (hw + 0.01), 0.46), (x + 0.26, s * (ty - 0.19), 0.46), 0.045, "shade", 10)
                v.rod((x + 0.26, s * (ty - 0.19), 0.46), (x, s * (ty - 0.19), r_w), 0.036, "shade", 8)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((2.26, s * 0.90, 0.64), Y * s, [(0.2, 0.0), (0.2, 0.13), (0.14, 0.2), (0.0, 0.24), (0.0, 0.0)], "upper", v.n(22, 8))
        v.lathe((-2.30, s * 0.90, 0.58), Y * s, [(0.12, 0.0), (0.12, 0.08), (0.0, 0.12), (0.0, 0.0)], "upper", v.n(16, 6))

    # ---- her skirts of May 1943 on: five plates a side on a rail, strapped to the box and standing on the guards --
    skirt_parts, skirt_plates = [], []
    if skirts:
        skirt_parts, skirt_plates = hull_skirts(v, set(skirts.split(",")) if isinstance(skirts, str) else (), -2.50, 2.10, 5, 1.62, 1.38, 0.52, (1.44, zg),
                                                side_y=lambda x: 1.30 if -0.9 <= x <= 1.0 else None, mark_z=None, under=True,
                                                bent={"r3": ("fore", 0.11), "l2": ("aft", 0.08)})
        v.use("hull")

    # ---- sockets for what happens to her ----------------------------------------------------------------------
    v.socket("engine", (-1.7, 0.0, ze + 0.04), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-1.33, 0.44, ze + 0.04), Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="hull", note="out of the commander's hatch")
    v.socket("fire.3", (-0.46, -0.60, zr + 0.05), Z, part="hull", note="out of the loader's hatch")
    v.socket("hit.front", (2.66, 0.2, 0.8), X, part="hull")
    v.socket("hit.left", (0.2, 1.33, 1.34), Y, part="hull")
    v.socket("hit.right", (0.2, -1.33, 1.34), -Y, part="hull")
    v.socket("hit.rear", (-2.70, 0.0, 1.1), -X, part="hull")
    v.socket("hit.turret", (0.5, 1.2, 1.66), Y, part="hull", note="the fighting box's side")
    v.socket("case", (-0.46, -0.60, zr + 0.1), -Y, part="hull", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.38, kick=0.8, gun_low=-6.0, hatches=["hatch_commander", "hatch_loader"], traverse=6.0,
                 idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", has_turret=False, mg="mg")
    swaps = {"how": "one model for the whole war. paint: put the chosen coat's colours in place of the paints of the same names. mantlet: show "
                    "the chosen shield's part and scale the other's to nothing (a part scaled to nothing is not drawn). skirts: show their "
                    "parts or scale them to nothing. " + BY_MONTH_HOW,
             "paint": coats(v, paint or "grey"), "by_month": STUG3_MONTHS}
    if mantlet == "all":
        swaps["mantlet"] = {"default": "box", "choices": {
            "box": {"what": "the welded box shield round the gun", "years": "1942 on (beside the cast one from 1944)", "parts": ["mantlet_box"]},
            "cone": {"what": "the cast shield shaped like a boar's head", "years": "1944-01 on, on about half of them", "parts": ["mantlet_cone"]}}}
    if skirts:
        swaps["skirts"] = {"default": "off", "choices": {
            "off": {"what": "no skirts", "years": "to 1943-04", "parts": []},
            "on": {"what": "thin plates hung on a rail along each side of her hull", "years": "1943-05 on, with the dark yellow coat", "parts": skirt_parts,
                   "lose": {"parts": skirt_plates, "how": "each vehicle hides none, one or two of these plates at random, so a battery is not one "
                                                           "vehicle copied; the rails always stay"}}}}
    m.info.setdefault("file", {})["swaps"] = swaps
    v.kill_rings("barrel")                                             # no name: a German crew painted rings round the barrel (b477)
    return v


def su85(fine=True, tag="SOV"):
    """SU-85 of 1943: the T-34's hull with its front plate carried on up into a fixed box where the turret stood, and
    the long 85 mm gun in a heavy cast collar on the right of that plate. 8.15 m over the gun, 2.45 m high."""
    v = Works("SU85", "SU-85", "tank", tag, fine)
    h = t34_hull(v, fine, bow_mg=False)
    m = v.m
    zr = h["zr"]
    zc = 2.08                                             # the box's roof
    xa, xb = 1.82, -0.56                                  # where its front and back plates stand on the hull's roof
    xt = xa - (zc - zr) * 1.75                            # the head of the front plate: the glacis carried on up
    ht = 0.95 - (zc - zr) * math.tan(math.radians(20.0))  # half the roof's width: the sides lean in
    v.use("hull")
    v.hull([(xa, 0.95, zr), (xb, 0.95, zr), (xt, ht, zc), (xb + 0.14, ht, zc)], coat, bevel=0.016)
    sn = [Vector((0, s * math.cos(math.radians(20.0)), math.sin(math.radians(20.0)))) for s in (1.0, -1.0)]
    y_at = lambda z: 0.95 - (z - zr) * math.tan(math.radians(20.0))      # noqa: E731

    # ---- the gun: a fixed cast collar on the front plate, a ball in it, the long barrel ------------------------
    gy, gz = -0.16, 1.52
    gx = 2.94 - (gz - 0.92) * 1.75                        # where the bore meets the front plate
    v.lathe((1.0, gy, gz), X, [(1.26, 0.0), (1.26, 0.27), (1.12, 0.37), (0.5, 0.45), (0.0, 0.45), (0.0, 0.0)], coat, v.n(36, 12))
    if fine:
        v.bolts((gx + 0.02, gy, gz), X, 0.52, 10, r=0.02, high=0.014)
    v.part("turret", "hull", (gx + 0.1, gy, gz), axis=(0, 0, -1), limits=[-8.0, 8.0], kind="turn",
           note="the gun's own traverse in its mounting, eight degrees each way: + to the right; for more the game turns the whole of her")
    v.part("gun", "turret", (gx + 0.1, gy, gz), axis=(0, -1, 0), limits=[-5.0, 25.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.ball((2.26, gy, gz), 0.262, coat, v.n(28, 10), v.n(14, 6))
    v.lathe((2.38, gy, gz), X, [(0.42, 0.0), (0.42, 0.115), (0.36, 0.135), (0.0, 0.16), (0.0, 0.0)], coat, v.n(24, 8))
    v.part("barrel", "gun", (2.76, gy, gz), slide=(-1, 0, 0), slide_limits=[0.0, 0.4], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((2.76, gy, gz), 2.44, 0.085, r0=0.1, r1=0.072, brake=None, sleeve=None)
    v.socket("muzzle", (5.22, gy, gz), X)
    m.info["bore"] = 0.085
    m.info["brake"] = False

    # ---- the roof: the commander's cap on the right, a wide lid behind the gun, sights on the left --------------
    v.use("hull")
    cup = v.cupola((0.26, -0.42, zc), 0.26, 0.15, slits=4, start=0.4)
    v.hatch_round("hatch_commander", "hull", cup, 0.2, Z, -X, thick=0.028, opens=100.0)
    v.socket("commander", cup + Z * -0.25, X, part="hull", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "hull", (-0.12, 0.22, zc), (0.46, 0.56), Z, X, hinge="back", opens=100.0)
    v.use("hull")
    v.slab((0.52, 0.22, zc), (0.74, 0.42, zc + 0.15), coat, 0.03, segs=2)                # the gunner's sight under its hood
    v.periscope((0.42, 0.60, zc), X, (0.09, 0.13, 0.11))
    v.vent((0.62, -0.12, zc), 0.1, 0.045)
    if fine:
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            v.handle(Vector((0.7, s * y_at(1.95), 1.95)), Vector((0.1, s * y_at(1.95), 1.95)), n, 0.011, 0.06)     # high on the plate: the slogan goes under it
            v.port(Vector((-0.2, s * y_at(1.8), 1.8)), n, 0.04)
        for at in ((0.75, 0.6), (0.75, -0.6), (-0.32, 0.0)):
            v.lift_eye((at[0], at[1], zc), Z, Y, 0.03)
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        v.mark("SOV", (1.02, s * (y_at(1.78) + 0.002), 1.78), n, 0.15, part="hull")      # ahead of the hand rail: the plate behind it is the slogan's

    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="hull", note="out of the commander's hatch")
    v.socket("hit.turret", (0.5, y_at(1.8), 1.8), sn[0], part="hull", note="the fighting box's side")
    v.socket("case", (-0.12, 0.22, zc + 0.1), Y, part="hull", note="where the empty case is thrown out")
    v.tank_clips(recoil=0.36, kick=0.9, gun_low=-5.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver"], traverse=6.0,
                 idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_02", has_turret=False, mg=None)
    v.name_place((0.33, 1.75), long=0.9, high=0.11, lines=2)          # the slogan (b477): on the casemate's side, under the hand rail and ahead of the pistol port
    return v


def m10(fine=True, tag="USA"):
    """M10 tank destroyer of 1942: the M4's lower hull and running gear under a hull of thin plate sloped on every
    side and studded with bosses for more, and a five-sided turret with no roof for the long 3-inch gun, a wedge of
    counterweight on its back. 5.97 m hull, 6.83 m over the gun, 3.05 m wide."""
    v = Works("M10", "M10 tank destroyer", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.85))
    m = v.m
    hw, zs, zr = 0.80, 1.06, 1.70                         # half-width between the tracks; the sponson's floor; the roof
    m4_tub(v, hw)

    # ---- the upper hull: every plate sloped, the sides standing out over the tracks -----------------------------
    v.hull([(2.47, 0.95, zs), (1.95, 1.50, zs), (-2.40, 1.50, zs), (-2.80, 1.15, zs), (1.30, 0.95, zr), (1.20, 1.02, zr), (-1.85, 1.02, zr), (-2.30, 0.86, zr)], coat, bevel=0.014)
    gl = Vector((1.30 - 2.47, 0, zr - zs)).normalized()               # up the front plate
    gn = Vector((gl.z, 0, -gl.x))                                     # out of it
    rb = Vector((-2.30 + 2.80, 0, zr - zs)).normalized()              # up the tail plate
    rn = Vector((-rb.z, 0, rb.x))
    sn = [Vector((0, s * 0.8, 0.6)) for s in (1.0, -1.0)]             # out of each side plate
    side_y = lambda z: 1.50 - (z - zs) * 0.75                         # noqa: E731
    g_at = lambda u, y: Vector((2.47, y, zs)) + gl * u + gn * 0.002       # noqa: E731
    r_at = lambda u, y: Vector((-2.80, y, zs)) + rb * u + rn * 0.002      # noqa: E731
    for s, lid in ((LEFT, "hatch_driver"), (RIGHT, "hatch_radio")):
        v.hatch_rect(lid, "hull", g_at(0.98, s * 0.50), (0.50, 0.50), gn, gl, hinge="left" if s > 0 else "right", opens=100.0, thick=0.03, lip=0.012)
        v.periscope(g_at(1.06, s * 0.50) + gn * 0.03, X, (0.07, 0.15, 0.07), part=lid, up=gn)
        v.use("hull")
        v.headlamp(g_at(0.40, s * 0.80) + gn * 0.07, X, 0.062, stalk=0.07)
        v.lamp_guard(g_at(0.40, s * 0.80) + gn * 0.07 + Z * 0.07, 0.062, g_at(0.40, s * 0.80), gn, Y, gl)      # the brush guard: two hoops of bar crossing over the lamp
        v.plate([(2.46, s * 0.84, zs + 0.004), (2.46, s * 1.31, zs + 0.004), (2.92, s * 1.31, zs - 0.02), (2.92, s * 0.84, zs - 0.02)][::int(s)], 0.008, "upper", 0.003)
        v.plate([(2.92, s * 0.84, zs - 0.02), (2.92, s * 1.31, zs - 0.02), (3.00, s * 1.31, zs - 0.10), (3.00, s * 0.84, zs - 0.10)][::int(s)], 0.008, "upper", 0.003)
    v.use("hull")
    if fine:
        # the bosses that more armour could be bolted to: two rows along each side, two across the front plate
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            for z in (1.24, 1.54):
                for x in (1.5, 0.9, 0.3, -0.3, -0.9, -1.5, -2.1):
                    v.bolt((x, s * (side_y(z) + 0.002), z), n, 0.04, 0.032)
            v.handle(Vector((-0.9, s * side_y(1.62), 1.62)), Vector((-1.6, s * side_y(1.62), 1.62)), n, 0.011, 0.05)
        for u in (0.22, 0.60):
            for y in (-0.72, -0.25, 0.25, 0.72):
                v.bolt(g_at(u, y), gn, 0.04, 0.032)
        v.weld(g_at(0.03, -0.92), g_at(0.03, 0.92), gn)
        v.cable([(1.7, -side_y(1.14) - 0.03, 1.14), (0.2, -side_y(1.12) - 0.03, 1.12), (-1.3, -side_y(1.14) - 0.03, 1.14)], 0.014)
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        v.mark("USA", (0.6, s * (side_y(1.39) + 0.003), 1.39), n, 0.2, part="hull")
    # the engine deck behind the turret: the air intake across it, two doors behind that; the tail with its doors
    v.slab((-1.42, -0.72, zr), (-1.14, 0.72, zr + 0.03), "upper", 0.01)
    v.grille((-1.28, 0.0, zr + 0.03), (1.30, 0.20), Z, Y, slats=3, deep=0.02)
    for s in (LEFT, RIGHT):
        v.slab((-1.84, s * 0.04, zr), (-1.46, s * 0.76, zr + 0.025), "upper", 0.008)
        if fine:
            v.handle((-1.65, s * 0.62, zr + 0.025), (-1.65, s * 0.44, zr + 0.025), Z, 0.009, 0.045, "steel")
            v.lift_eye(r_at(0.62, s * 0.8), rn, Y, 0.03)
        v.slab((-2.645, s * 0.03, 0.60), (-2.62, s * 0.52, 1.0), "upper", 0.006)
    for i, y in enumerate((0.16, -0.16)):
        v.tube([(-2.60, y, 1.03), (-2.70, y, 1.01), (-2.80, y, 0.95)], 0.04, "rust", v.n(10, 5))
        v.socket(f"exhaust.{i + 1}", (-2.82, y, 0.94), (-1, 0, -0.45))
    if fine:                                              # the pioneer tools across the tail plate
        v.shovel(r_at(0.30, -0.5) + rn * 0.02, 90.0, 0.95)
        v.axe(r_at(0.50, -0.38) + rn * 0.02, 90.0, 0.78)

    m4_gear(v, hw)

    # ---- the turret: five thin walls leaning in, no roof; the men and the breech are seen from above ------------
    x0, zt, th = 0.30, zr, 0.68
    v.use("hull")
    v.ring_well((x0, 0.0, zt), 0.84)
    v.part("turret", "hull", (x0, 0.0, zt), axis=(0, 0, -1), kind="turn", note="trains all round, by hand: + to the right, clockwise seen from above; the game turns it")
    low = [(0.86, 0.50), (0.42, 0.92), (-0.78, 0.92), (-1.30, 0.50)]
    top = [(0.74, 0.44), (0.38, 0.74), (-0.74, 0.74), (-1.16, 0.42)]
    z0, z1 = zt + 0.03, zt + th
    lo = [Vector((x0 + x, y, z0)) for x, y in low] + [Vector((x0 + x, -y, z0)) for x, y in reversed(low)]
    hi = [Vector((x0 + x, y, z1)) for x, y in top] + [Vector((x0 + x, -y, z1)) for x, y in reversed(top)]
    for i in range(8):
        j = (i + 1) % 8
        c = (lo[i] + lo[j] + hi[j] + hi[i]) / 4
        wall(v, [lo[i], lo[j], hi[j], hi[i]], Vector((x0 - 0.2 - c.x, -c.y, 0.0)), thick=0.03, paint=coat, bevel=0.004)
    v.prism([(p.x, p.y) for p in lo], zt + 0.012, z0 + 0.03, "shade")                                 # her floor, in shadow
    v.lathe((x0, 0, zt - 0.03), Z, [(0.06, 0.88), (0.0, 0.90), (0.0, 0.0)], "shade", v.n(36, 12))
    if fine:
        v.tube([p + Z * 0.004 for p in hi], 0.016, coat, 6, closed=True)                               # the bead round her open top
    # the counterweight: a wedge standing off her back
    v.hull([(x0 - 1.29, 0.47, z0 + 0.22), (x0 - 1.17, 0.40, z1), (x0 - 1.56, 0.34, z1 - 0.03), (x0 - 1.54, 0.42, z0 + 0.34)], coat, bevel=0.02, segs=2)
    # inside: two seats, the ready rounds standing along her back wall
    for at in ((x0 - 0.22, 0.45), (x0 - 0.42, -0.52)):       # (the gunner's: behind the breech ring and left of the recoil guard, where the turret is widest)
        v.rod((at[0], at[1], zt + 0.05), (at[0], at[1], zt + 0.28), 0.025, "steel", 8)
        v.box((0.30, 0.30, 0.06), (at[0], at[1], zt + 0.31), "leather", 0.02)
    if fine:
        for k in range(6):
            y = -0.30 + 0.12 * k
            v.rod((x0 - 1.10, y, zt + 0.07), (x0 - 1.10, y, zt + 0.50), 0.042, "brass", 10)
            v.lathe((x0 - 1.10, y, zt + 0.50), Z, [(0.14, 0.0), (0.09, 0.03), (0.0, 0.042), (0.0, 0.0)], "steel", 10)
        for s in (LEFT, RIGHT):
            v.lift_eye((x0 + 0.1, s * 0.86, zt + 0.42), Vector((0, s * 0.96, 0.27)), X, 0.03)
    # the heavy machine gun on its pintle on her back wall
    pin = Vector((x0 - 1.20, 0.0, z1))
    v.rod(pin, pin + Z * 0.26, 0.02, "steel", 8)
    v.box((0.30, 0.09, 0.11), pin + Z * 0.29 + X * 0.02, "steel", 0.008)
    v.mg(pin + Z * 0.30 + X * 0.15, X, 0.95, r=0.02)
    v.box((0.16, 0.11, 0.2), pin + Z * 0.24 + Y * 0.11 + X * 0.05, "upper", 0.006)
    v.socket("mg", pin + Z * 0.30 + X * 1.12, X, part="turret", note="the heavy machine gun on her back wall")
    v.aerial("aerial", "turret", (x0 - 1.0, 0.62, z1), 2.2)
    for s in (LEFT, RIGHT):
        v.mark("USA", (x0 - 0.2, s * 0.838, zt + 0.36), Vector((0, s * 0.964, 0.266)), 0.15, part="turret")

    # ---- the gun: the long 3-inch behind a wide wedge of a shield; its breech stands in the open turret -----------
    tx, tz = x0 + 0.80, zt + 0.40
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-10.0, 30.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.hull([(tx + 0.03, 0.57, tz - 0.27), (tx + 0.03, 0.57, tz + 0.27), (tx + 0.30, 0.52, tz - 0.13), (tx + 0.30, 0.52, tz + 0.13), (tx + 0.52, 0.17, tz - 0.15), (tx + 0.52, 0.17, tz + 0.15)],
           coat, bevel=0.03, segs=3, angle=40.0)
    v.lathe((tx + 0.50, 0, tz), X, [(0.30, 0.0), (0.30, 0.108), (0.0, 0.15), (0.0, 0.0)], coat, v.n(24, 8))
    v.slab((tx - 0.72, -0.13, tz - 0.14), (tx - 0.02, 0.13, tz + 0.14), "steel", 0.02)                   # the breech end of her, in the open
    v.slab((tx - 0.92, -0.17, tz - 0.17), (tx - 0.66, 0.17, tz + 0.17), "steel", 0.02)
    if fine:
        for s in (LEFT, RIGHT):                           # the recoil guard behind the breech
            v.slab((tx - 1.30, s * 0.21, tz - 0.20), (tx - 0.90, s * 0.225, tz + 0.05), coat, 0.003)
        v.slab((tx - 1.30, -0.225, tz - 0.215), (tx - 0.90, 0.225, tz - 0.20), coat, 0.003)
    v.part("barrel", "gun", (tx + 0.80, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.32], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.80, 0, tz), 2.0, 0.0762, r0=0.098, r1=0.062, brake=None, steps=((0.55, None),), sleeve=None)
    v.socket("muzzle", (tx + 2.82, 0, tz), X)
    m.info["bore"] = 0.0762
    m.info["brake"] = False

    # ---- sockets for the men in her open turret, and for what happens to her -----------------------------------
    v.socket("commander", (x0 - 0.45, -0.50, zt + 0.05), X, part="turret", note="the commander stands here in the open turret, on the right")
    v.socket("crew.gunner", (x0 - 0.22, 0.45, zt + 0.34), X, part="turret", note="the gunner sits here, left of the breech and its guard, his knees beside the breech ring: the top of his seat, 0.28 over the floor")
    v.socket("crew.loader", (x0 - 0.70, 0.30, zt + 0.05), X, part="turret", note="the loader stands here, behind the breech")
    v.socket("engine", (-1.6, 0.0, zr + 0.05), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-1.65, 0.4, zr + 0.05), Z, part="hull")
    v.socket("fire.2", (x0 - 0.2, 0.0, z1), Z, part="turret", note="out of the open turret")
    v.socket("fire.3", g_at(0.98, 0.50) + gn * 0.05, gn, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zt), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(0.5, 0.0), gn, part="hull")
    v.socket("hit.left", (0.2, side_y(1.4), 1.4), sn[0], part="hull")
    v.socket("hit.right", (0.2, -side_y(1.4), 1.4), sn[1], part="hull")
    v.socket("hit.rear", r_at(0.3, 0.0), rn, part="hull")
    v.socket("hit.turret", (x0 - 0.2, 0.84, zt + 0.36), Y, part="turret")
    v.socket("case", (x0 - 0.9, -0.3, zt + 0.1), -X, part="turret", note="where the empty case falls, on her floor")
    v.tank_clips(recoil=0.30, kick=0.9, gun_low=-10.0, hatches=["hatch_driver", "hatch_radio"], traverse=12.0,
                 idle_cue="tank_idle_sherman_loop", drive_cue="tank_sherman_drive_loop", gun_cue="tank_gun_02", blow=(-0.9, 2.8, 0.45, 170.0), mg="mg")
    v.name_place((1.5, 1.39), long=0.62, high=0.12, lines=(2, 1), high_two=0.10)                    # her crew's name (b477): on the hull's side, ahead of the star
    return v
