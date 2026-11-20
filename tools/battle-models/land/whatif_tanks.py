"""What-if tanks: original designs that never ran, for a war that goes its own way.

Brian, 2026-11-19: "i want this to be a sandbox to play and make your own history, not just play out the real one",
and (of the shelf of what-if ships) "make all 5": what-if planes and tanks on the research tree, and their battle
models to the same bar as every other unit (the battleship clip: smooth, no faceting, and detailed).

Each is our own design, drawn in the manner of its years and of no one maker; no real prototype is copied and the
late-war prototypes of ruling 230 (which have their own faces) are left alone. Every shape is our own mesh, made from
numbers here. Like the generic tanks they belong to no army: built in the plain coat with the owner's mark (`tag`
"_"), camouflaged (`upper2`), so the game paints each in its owner's colours (unit-table.json `paint`, `paint2`).
Their file's `whatif` names the class, the plain models it stands nearest (`like`, for a battle that has no place for
it yet) and the research it comes from.

    WhatIf_BreakthroughTank   (breaker) a main turret on a raised barbette, its 150 mm firing over a small fore turret
    WhatIf_TankHunterCar      (tankhunter) eight big wheels under a low boat-shaped hull, a long 75 mm in a flat turret
    WhatIf_FlameTank          (flametank) the medium hull, a cast turret with a flame projector, armoured fuel tanks on her deck
    WhatIf_AmphibiousTank     (amphtank) a tall boat of a hull on tracks, a trim vane on her bow, two screws and rudders astern
    WhatIf_SnowTank           (snowtank) tracks twice the usual width with ice spurs, an engine stove, fur-sealed hatches
    WhatIf_GliderTank         (glidertank) a small two-man tank that fits the assault glider's hold (whatif_planes)
"""

import math

from mathutils import Vector

from carriers import CarrierWorks
from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, inset_plan, sym_plan
from tanks_any import leaf_bogie, medium_hull, rivets, turn_to
from tanks_ger import coat
from works import Works


def _file(v, kind, title, like, tech, crew, note):
    """What the game reads to place a what-if machine: her class, the plain models she stands nearest, her research."""
    v.m.info.setdefault("file", {}).update({
        "class": title, "nation": None, "year": None, "stands_for": None, "crew": crew,
        "whatif": {"kind": kind, "like": like, "from": tech,
                   "note": "an original design that never ran: any army that has the research builds her, in its own paint. " + note},
        "paint": "the plain coat: the game fills upper and upper2 with her owner's paint and paint2 (unit-table.json), and paints its own mark at each `_`"})


def second_turret(v, name, ring, limits, traverse, note):
    """A second turret's controls, for the game to train and lay it as it does the main one."""
    ctl = v.m.info.setdefault("file", {}).setdefault("controls", {})
    ctl["turret_" + name] = {"part": "turret_" + name, "what": "trained by the game: + to the right (clockwise from above). " + note, "degrees_per_second": traverse}
    ctl["gun_" + name] = {"part": "gun_" + name, "what": "laid by the game inside its limits: + muzzle up", "limits": limits}


def second_gun_clip(v, name, recoil, cue):
    """The second turret's gun fires: its barrel runs back and out, and its own flash."""
    from rigid import smooth

    def run_back(t):
        if t < 0.04:
            return recoil * smooth(t / 0.04)
        if t < 0.1:
            return recoil
        return recoil * max(0.0, 1.0 - smooth((t - 0.1) / 0.6))
    keys = [[t, round(run_back(t), 4)] for t in (0.0, 0.01, 0.02, 0.03, 0.04, 0.1, 0.16, 0.24, 0.32, 0.4, 0.5, 0.6, 0.7, 1.6)]
    v.m.clips["fire_" + name] = {"long": 1.6, "loop": False, "after": "hold",
                                  "about": "the " + name + " turret's gun fires: flash at 0.0, its barrel runs back and out again",
                                  "tracks": [{"part": "barrel_" + name, "slide": keys}],
                                  "events": [{"t": 0.0, "fx": "muzzle_flash", "socket": "muzzle_" + name, "cue": cue},
                                             {"t": 0.0, "fx": "muzzle_blast_dust", "socket": "muzzle_" + name, "cue": None}]}


# ---- 1. the breakthrough tank (class `breaker`) -------------------------------------------------------------------

def breakthrough_tank(fine=True, tag="_"):
    """The breakthrough tank: 9.8 m long over her hull, 3.6 m wide, 3.4 m high over the cupola, about 70 tonnes;
    seven men. A long welded hull whose fighting box runs out over her tracks to full width behind a steep front plate
    the width of her; seven big wheels a side on swing arms behind armoured skirts, wide tracks, the drive at the
    back, two exhaust stacks at her tail. Two turrets in line: low on her roof a small cast fore turret with a long
    50 mm anti-tank gun, and behind it her main turret, raised on an armoured barbette so that its short, fat 150 mm
    assault gun fires over the fore turret, as a battleship's second turret fires over her first. The big gun blasts
    bunkers and strongpoints; the small one keeps enemy tanks off her. Slow, heavy, and meant to break a line the
    others then pour through."""
    v = Works("WhatIf_BreakthroughTank", "Breakthrough tank, a what-if class (our own design)", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.84), rock=(-3.0, 3.0))
    hw, sw, zg, zr = 1.02, 1.80, 1.28, 2.02          # half-width between the tracks; of the fighting box; its floor over the tracks; the roof
    xg, xr = 4.78, 3.68                              # the front plate's foot, and its head at the roof
    xd = -1.40                                       # where the engine's deck begins, a step down behind the roof
    zd, xt, zt = 1.90, -4.75, 1.72                   # the deck's head, and its tail
    # ---- the tub between the tracks; the fighting box over them; the engine's part ------------------------------
    v.hull([(x, hw, z) for x, z in [(-4.70, 0.62), (-4.45, 0.46), (4.05, 0.46), (4.85, 0.94), (xg, zg), (-4.95, zg), (-4.95, 0.82)]], coat, bevel=0.016)
    v.hull([(xg, sw, zg), (xr, sw - 0.06, zr), (xd, sw - 0.06, zr), (xd, sw, zg)], coat, bevel=0.018)
    v.hull([(xd + 0.02, sw - 0.02, zg), (xd + 0.02, sw - 0.08, zd), (xt, sw - 0.10, zt), (-4.97, sw - 0.04, zg)], coat, bevel=0.018)
    gl = Vector((xr - xg, 0, zr - zg)).normalized()          # up the front plate
    gn = Vector((gl.z, 0, -gl.x))
    dk = Vector((xt - xd, 0, zt - zd)).normalized()          # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))
    g_at = lambda u, y: Vector((xg, y, zg)) + gl * u + gn * 0.002          # noqa: E731
    d_at = lambda u, y: Vector((xd, y, zd)) + dk * u + dn * 0.002          # noqa: E731
    lean = math.atan2(0.06, zr - zg)
    sn = [Vector((0, s * math.cos(lean), math.sin(lean))) for s in (1.0, -1.0)]
    y_at = lambda z: sw - (z - zg) * math.tan(lean)                        # noqa: E731
    # the sponsons' undersides over the tracks, with their ribs
    for s in (LEFT, RIGHT):
        v.slab((-4.95, s * hw, zg - 0.03), (xg, s * sw, zg), "shade", 0.004)
        if fine:
            for x in (-3.9, -2.6, -1.3, 0.0, 1.3, 2.6, 3.9):
                v.slab((x - 0.016, s * hw, zg - 0.12), (x + 0.016, s * (sw - 0.04), zg - 0.03), "shade")
    # the front plate: the driver's visor on the left, the bow gun in its ball on the right, spare links between,
    # hooks and lamps at its foot; welds along its seams
    v.visor(g_at(0.80, 0.52) + gn * 0.004, Vector((1, 0, 0.2)).normalized(), 0.40, 0.13, 0.11)
    v.lathe(g_at(0.76, -0.56) - gn * 0.02, gn, [(0.14, 0.0), (0.14, 0.10), (0.08, 0.17), (0.0, 0.21), (0.0, 0.0)], "upper", v.n(26, 8))
    v.mg_ball(g_at(0.76, -0.56) + gn * 0.11, Vector((0.94, 0, 0.12)).normalized(), 0.09, socket="hull_mg", length=0.42, ring=False)
    for s in (LEFT, RIGHT):
        v.hook((4.86, s * 0.56, 0.82), Vector((0.8, 0, -0.4)).normalized(), 0.10)
        v.hook((-4.97, s * 0.62, 0.88), -X, 0.10)
        v.headlamp(g_at(0.10, s * 1.52) + gn * 0.02, Vector((1, 0, 0.05)).normalized(), 0.075, stalk=0.08)
    if fine:
        v.spare_links(g_at(0.38, -0.40), g_at(0.38, 0.40), 5, 0.16, 0.70, gn, style={"kind": "plate", "horn": "centre", "grouser": 0.016, "cleats": True})
        v.weld(g_at(0.02, -sw + 0.05), g_at(0.02, sw - 0.05), gn)
        v.weld(g_at(0.995, -sw + 0.1), g_at(0.995, sw - 0.1), (gn + Z).normalized())
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            v.weld(Vector((xr - 0.04, s * (y_at(zr) - 0.005), zr - 0.004)), Vector((xd + 0.04, s * (y_at(zr) - 0.005), zr - 0.004)), (n + Z).normalized())
            v.weld(Vector((xg - 0.04, s * (sw + 0.001), zg + 0.02)), Vector((xr + 0.02, s * (y_at(zr) + 0.001), zr - 0.02)), n)

    # ---- the roof: the driver's and the bow gunner's lids at its front, periscopes, a vent, lifting eyes --------
    v.hatch_rect("hatch_driver", "hull", (3.36, 0.52, zr), (0.36, 0.50), Z, X, hinge="back", opens=105.0, thick=0.03)
    v.hatch_rect("hatch_radio", "hull", (3.36, -0.52, zr), (0.36, 0.50), Z, X, hinge="back", opens=105.0, thick=0.03)
    v.use("hull")
    v.periscope((3.58, 0.24, zr), X, (0.07, 0.14, 0.07))
    v.periscope((3.58, -0.80, zr), X, (0.07, 0.14, 0.07))
    v.vent((1.25, 1.20, zr), 0.11, 0.05)
    if fine:
        for at in ((3.50, 1.45), (3.50, -1.45), (-1.25, 1.45), (-1.25, -1.45)):
            v.lift_eye((at[0], at[1], zr), Z, X, 0.036)
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            v.port(Vector((0.3, s * y_at(1.70), 1.70)), n, 0.045)
            v.port(Vector((2.6, s * y_at(1.70), 1.70)), n, 0.045)
            v.handle(Vector((1.9, s * y_at(1.90), 1.90)), Vector((1.2, s * y_at(1.90), 1.90)), n, 0.011, 0.06)

    # ---- the engine deck: two grilles, the doors behind them, the fans' round grilles; the exhausts at the tail -----
    for s in (LEFT, RIGHT):
        v.box((0.80, 0.86, 0.024), d_at(0.62, s * 0.78) + dn * 0.012, "upper", 0.006, turn=turn_to(dk, dn))
        v.grille(d_at(0.62, s * 0.78) + dn * 0.024, (0.64, 0.74), dn, X, slats=9, deep=0.024)
        v.box((0.78, 0.80, 0.024), d_at(1.56, s * 0.74) + dn * 0.012, "upper", 0.006, turn=turn_to(dk, dn))
        v.fan_grille(d_at(2.62, s * 0.74) + dn * 0.012, 0.30, dn, part="hull")
        if fine:
            v.handle(d_at(1.80, s * 0.74) + dn * 0.024 - Y * 0.1, d_at(1.80, s * 0.74) + dn * 0.024 + Y * 0.1, dn, 0.008, 0.04, "steel")
            for dy in (-0.27, 0.27):
                v.hinge(d_at(1.20, s * 0.74 + dy) + dn * 0.024 - Y * 0.05, d_at(1.20, s * 0.74 + dy) + dn * 0.024 + Y * 0.05, 0.014)
        # (two tall stacks at her tail, a silencer on each, standing clear of the plate on brackets)
        v.silencer((-5.10, s * 0.95, 1.00), (-5.10, s * 0.95, 1.86), 0.11, bands=3, tail=((-5.10, s * 0.95, 1.86), (-0.25, 0, 1)))
        v.tube([(-4.96, s * 0.95, 0.94), (-5.04, s * 0.95, 0.92), (-5.10, s * 0.95, 1.00)], 0.05, "rust", v.n(10, 5))
        if fine:
            for z in (1.18, 1.62):
                v.slab((-5.02, s * 0.93, z - 0.025), (-4.96, s * 0.97, z + 0.025), "shade")
        v.bin((-4.50, s * 1.20, zg + 0.02), (-3.70, s * 1.62, zg + 0.30), lid_side=Y * s)
    v.socket("exhaust.1", (-5.14, 0.95, 1.96), (-0.3, 0, 1))
    v.socket("exhaust.2", (-5.14, -0.95, 1.96), (-0.3, 0, 1))
    v.use("hull")
    if fine:
        v.slab((-5.01, -0.70, 1.10), (-4.97, 0.70, 1.20), "shade", 0.004)              # the tail's towing bar
        v.jerrycan((-4.55, 0.0, zt + 0.02), 90.0)
        # (the tail plate: the engine's access door between the stacks, bolted round, and the track adjusters' covers)
        v.box((0.03, 0.96, 0.46), (-4.965, 0.0, 1.50), "upper", 0.008)
        for y in (-0.40, -0.13, 0.13, 0.40):
            v.bolt((-4.982, y, 1.71), -X, 0.014)
            v.bolt((-4.982, y, 1.29), -X, 0.014)
        v.handle(Vector((-4.985, -0.12, 1.50)), Vector((-4.985, 0.12, 1.50)), -X, 0.009, 0.05, "steel")
        for s in (LEFT, RIGHT):
            v.lathe((-4.95, s * 1.42, 0.80), -X, [(0.05, 0.0), (0.05, 0.12), (0.03, 0.14), (0.0, 0.14), (0.0, 0.0)], "upper", 20)
            v.hook((-4.97, s * 1.20, 1.18), -X, 0.08)
    # (her stowage boxes on the fighting box's sides, behind the barbette)
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        yb = y_at(1.62)
        v.bin((-2.60, s * yb, 1.40), (-1.55, s * (yb + 0.30), 1.84), lid_side=Y * s)

    # ---- tools on her sides and deck ---------------------------------------------------------------------------
    v.shovel((2.2, y_at(1.50) + 0.03, 1.50), 0.0, 1.0, roll=-90.0 + math.degrees(lean))
    v.axe((0.5, y_at(1.50) + 0.03, 1.50), 0.0, 0.8, roll=-90.0 + math.degrees(lean))
    v.crowbar((1.5, -(y_at(1.52) + 0.03), 1.52), 0.0, 1.4)
    v.jack((-2.5, -1.26, zd - 0.04), 0.0)
    v.jack_block((-3.2, -1.26, zd - 0.08), (0.42, 0.22, 0.13), 0.0)
    if fine:
        for s in (LEFT, RIGHT):
            v.cable([(3.9, s * (y_at(1.42) + 0.02), 1.42), (3.0, s * (y_at(1.38) + 0.03), 1.38), (-0.8, s * (y_at(1.38) + 0.03), 1.38), (-1.2, s * (y_at(1.44) + 0.02), 1.44)], 0.016)
    v.roll((-4.40, -0.85, zt + 0.10), (-4.40, 0.85, zt + 0.10), r=0.10, paint=v.own_paint("canvas.drab", "5C5B44"))
    v.aerial("aerial", "hull", (-1.20, -1.42, zr), 2.4)

    # ---- the running gear: seven big wheels a side on swing arms, the drive at the back, behind skirts ---------------
    ty, r_w = 1.42, 0.40
    xs = [-3.36 + 1.12 * i for i in range(7)]
    v.running(xs, r_w, ty, (-4.42, 0.80, 0.38), (4.45, 0.80, 0.34), rollers=[(-2.80, 1.06, 0.10), (0.0, 1.06, 0.10), (2.80, 1.06, 0.10)], drive="rear",
              wheel=dict(w=0.11, tyre=0.045, gap=0.16, bolts=10, sides=36, hub=0.30, axle=0.30),
              sprocket_kw=dict(teeth=17, wide=0.44, tooth=0.034, holes=6, bolts=12, sides=32, hub=0.40),
              idler_kw=dict(wide=0.40, kind="spoke", spokes=8),
              roller_kw=dict(wide=0.16, tyre=0.016, reach=0.28),
              track=dict(pitch=0.16, wide=0.72, thick=0.036, sag=0.016, style={"kind": "plate", "horn": "centre", "horn_high": 0.10, "grouser": 0.018, "cleats": True}))
    v.use("vehicle")
    if fine:
        for s in (LEFT, RIGHT):
            for x in xs:                                 # each wheel's arm, trailing from its pivot on the hull's side
                v.rod((x + 0.36, s * (hw + 0.01), 0.62), (x + 0.36, s * (ty - 0.24), 0.62), 0.06, "shade", 10)
                v.rod((x + 0.36, s * (ty - 0.24), 0.62), (x, s * (ty - 0.24), r_w), 0.05, "shade", 10)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((-4.42, s * (hw - 0.01), 0.80), Y * s, [(0.18, 0.0), (0.18, 0.18), (0.09, 0.27), (0.0, 0.30), (0.0, 0.0)], "upper", v.n(24, 8))
        v.lathe((4.45, s * (hw - 0.01), 0.80), Y * s, [(0.14, 0.0), (0.14, 0.09), (0.0, 0.14), (0.0, 0.0)], "upper", v.n(18, 6))
    # the skirts: heavy plates hung from the sponsons' edge over the top of the wheels, bolted at their heads
    x0s, x1s, ns = -4.30, 4.30, 7
    v.skirts(x0s, x1s, sw + 0.004, 0.80, zg + 0.005, count=ns, thick=0.014, gap=0.016)
    if fine:
        step = (x1s - x0s) / ns
        for s in (LEFT, RIGHT):
            for k in range(ns):
                for f in (0.2, 0.5, 0.8):
                    v.bolt((x0s + (k + f) * step, s * (sw + 0.019), zg - 0.06), Y * s, 0.016)

    # ---- the barbette: an armoured drum the main turret stands on, high enough for its gun to clear the fore turret --
    x0, zb = -0.30, zr + 0.42
    v.lathe((x0, 0, 1.84), Z, [(0.0, 0.0), (0.0, 1.12), (zb - 1.84 - 0.03, 1.12), (zb - 1.84, 1.09), (zb - 1.84, 0.0)], coat, v.n(56, 16))
    if fine:
        for k in range(16):
            a = 2 * math.pi * (k + 0.5) / 16
            v.bolt((x0 + 1.121 * math.cos(a), 1.121 * math.sin(a), zb - 0.09), Vector((math.cos(a), math.sin(a), 0)), 0.016)
    v.ring_well((x0, 0.0, zb), 1.00)

    # ---- the main turret: welded plate, its sides leaning in, a bin on its back, the commander's cupola ----------------
    th = 0.76
    v.part("turret", "hull", (x0, 0.0, zb), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    plan = sym_plan([(1.26, 0.66), (0.80, 1.18), (-0.90, 1.22), (-1.55, 0.80)])
    top = inset_plan(plan, [0.20, 0.18, 0.18, 0.12, 0.18, 0.18, 0.20, 0.14])
    v.prism([(x0 + x, y) for x, y in plan], zb + 0.02, zb + th, coat, top=[(x0 + x, y) for x, y in top], bevel=0.018, segs=2, angle=14.0)
    v.lathe((x0, 0, zb - 0.03), Z, [(0.06, 1.08), (0.0, 1.10), (0.0, 0.0)], "shade", v.n(48, 14))
    roof = zb + th
    v.bin((x0 - 2.02, -0.62, zb + 0.16), (x0 - 1.52, 0.62, zb + 0.58), part="turret", lid_side=-X)
    cup = v.cupola((x0 - 0.62, 0.48, roof), 0.34, 0.22, slits=6, start=math.radians(30))
    v.hatch_round("hatch_commander", "turret", cup, 0.26, Z, -X, thick=0.032, opens=105.0, split=True)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "turret", (x0 - 0.10, -0.52, roof), (0.48, 0.42), Z, X, hinge="front", opens=100.0, thick=0.03)
    v.use("turret")
    v.vent((x0 + 0.38, -0.30, roof), 0.12, 0.05)
    v.periscope((x0 + 0.62, 0.30, roof), X, (0.08, 0.15, 0.08))
    v.periscope((x0 + 0.20, -0.86, roof), X, (0.07, 0.13, 0.07))
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 - 0.30, s * 1.10, zb + 0.34)), Vector((0, s, 0.3)), 0.05)
            v.lift_eye((x0 + 0.70, s * 0.62, roof), Z, X, 0.036)
            v.handle(Vector((x0 + 0.40, s * 1.105, zb + 0.56)), Vector((x0 - 0.40, s * 1.11, zb + 0.56)), Vector((0, s, 0.3)).normalized(), 0.011, 0.06)
        v.lift_eye((x0 - 1.30, 0.0, roof), Z, Y, 0.036)

    # ---- the main gun: a short, fat 150 mm in a broad mantlet, its recoil sleeve over the barrel's root, an MG beside ---
    tx, tz = x0 + 1.22, zb + 0.40
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-6.0, 25.0], kind="turn",
           note="lifts at the trunnions: + muzzle up; the game lays it. Over the fore turret (trained within 30 degrees of ahead) she may not lay lower than -2")
    v.lathe((tx + 0.02, 0, tz), Y, [(0.66, 0.0), (0.66, 0.30), (0.58, 0.38), (-0.58, 0.38), (-0.66, 0.30), (-0.66, 0.0)], "upper", v.n(40, 12))
    v.lathe((tx + 0.30, 0, tz), X, [(0.36, 0.0), (0.36, 0.20), (0.26, 0.25), (0.0, 0.30), (0.0, 0.0)], "upper", v.n(32, 10))
    if fine:
        for y in (-0.50, 0.50):
            for z in (-0.22, 0.22):
                v.bolt((tx + 0.34, y, tz + z), X, 0.018)
    v.mg((tx + 0.34, -0.46, tz - 0.06), X, 0.20)
    v.socket("coax", (tx + 0.56, -0.46, tz - 0.06), X)
    v.part("barrel", "gun", (tx + 0.66, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.42], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.66, 0, tz), 2.30, 0.15, r0=0.20, r1=0.172, brake=None, sleeve=(0.72, 0.235))
    if fine:                                              # (the muzzle's thick ring)
        v.lathe((tx + 0.66 + 2.12, 0, tz), X, [(0.18, 0.0), (0.18, 0.186), (0.16, 0.192), (0.02, 0.192), (0.0, 0.186), (0.0, 0.0)], "upper", v.n(32, 10))
    v.socket("muzzle", (tx + 3.00, 0, tz), X)
    v.m.info["bore"] = 0.15
    v.m.info["brake"] = False

    # ---- the fore turret: a small cast one low on her roof, a long 50 mm and a machine gun ------------------------------
    v.use("hull")
    xf = 2.35
    v.ring_well((xf, 0.0, zr), 0.56)
    v.part("turret_fore", "hull", (xf, 0.0, zr), axis=(0, 0, -1), kind="turn",
           note="the fore turret: trains all round, but its gun clears the barbette only ahead of the beam: + to the right; the game turns it")
    v.cast([{"z": zr + 0.02, "xc": xf, "af": 0.66, "ar": 0.62, "b": 0.62, "n": 2.3},
            {"z": zr + 0.20, "xc": xf, "af": 0.64, "ar": 0.60, "b": 0.60, "n": 2.3},
            {"z": zr + 0.38, "xc": xf - 0.02, "af": 0.54, "ar": 0.54, "b": 0.52, "n": 2.2}], coat, sides=48, top=0.08, steps=3, part="turret_fore")
    froof = zr + 0.46
    v.lathe((xf, 0, zr - 0.03), Z, [(0.05, 0.60), (0.0, 0.62), (0.0, 0.0)], "shade", v.n(36, 10), part="turret_fore")
    v.hatch_round("hatch_fore", "turret_fore", Vector((xf - 0.18, 0.0, froof - 0.005)), 0.21, Z, -X, thick=0.026, opens=100.0)
    v.use("turret_fore")
    v.periscope((xf + 0.16, 0.24, froof - 0.02), X, (0.06, 0.11, 0.06))
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((xf - 0.10, s * 0.605, zr + 0.24)), Vector((0, s, 0.1)), 0.035)
            v.lift_eye((xf - 0.30, s * 0.38, froof - 0.03), Z, X, 0.03)
    fx, fz = xf + 0.60, zr + 0.24
    v.part("gun_fore", "turret_fore", (fx, 0.0, fz), axis=(0, -1, 0), limits=[-8.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((fx - 0.02, 0, fz), Y, [(0.30, 0.0), (0.30, 0.12), (0.25, 0.165), (-0.25, 0.165), (-0.30, 0.12), (-0.30, 0.0)], "upper", v.n(28, 8))
    v.lathe((fx + 0.10, 0, fz), X, [(0.20, 0.0), (0.20, 0.07), (0.13, 0.09), (0.0, 0.11), (0.0, 0.0)], "upper", v.n(20, 8))
    v.mg((fx + 0.08, -0.20, fz), X, 0.16)
    v.socket("coax_fore", (fx + 0.26, -0.20, fz), X)
    v.part("barrel_fore", "gun_fore", (fx + 0.30, 0.0, fz), slide=(-1, 0, 0), slide_limits=[0.0, 0.26], axis=(0, -1, 0), kind="slide", note="runs back along its own length when it fires (metres)")
    v.barrel((fx + 0.30, 0, fz), 2.60, 0.05, r0=0.07, r1=0.044, brake="single", sleeve=(0.34, 0.075))
    v.socket("muzzle_fore", (fx + 2.92, 0, fz), X)
    v.socket("crew.fore", (xf, 0.0, froof - 0.20), X, part="turret_fore", note="the fore turret's gunner, head under his lid")

    v.use("hull")
    v.socket("engine", d_at(0.9, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.62, 0.78) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (3.36, 0.52, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("fire.4", (xf - 0.18, 0.0, froof + 0.05), Z, part="turret_fore", note="out of the fore turret's hatch")
    v.socket("ring", (x0, 0.0, zb), Z, part="hull", note="the main turret's ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(0.5, 0.0), gn, part="hull")
    v.socket("hit.left", (0.6, y_at(1.6), 1.6), sn[0], part="hull")
    v.socket("hit.right", (0.6, -y_at(1.6), 1.6), sn[1], part="hull")
    v.socket("hit.rear", (-4.95, 0.0, 1.1), -X, part="hull")
    v.socket("hit.turret", (x0, 1.16, zb + 0.36), Y, part="turret")
    v.socket("case", (x0 - 0.50, -1.16, zb + 0.4), -Y, part="turret", note="where the empty case is thrown out")
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):           # her owner's mark: the fighting box's sides and the main turret's
        v.mark(tag, (1.30, s * (y_at(1.64) + 0.002), 1.64), n, 0.26, part="hull")
        v.mark(tag, (x0 - 0.62, s * 1.145, zb + 0.38), Vector((0.0, s, 0.32)), 0.2, part="turret")
    v.tank_clips(recoil=0.40, kick=1.0, gun_low=-6.0, hatches=["hatch_commander", "hatch_loader", "hatch_fore", "hatch_driver", "hatch_radio"], open_lids=2,
                 traverse=10.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_02", blow=(-1.2, 3.4, 0.55, 168.0))
    second_turret(v, "fore", (xf, 0.0, zr), [-8.0, 20.0], 22.0, "Trained aft of the beam its gun meets the barbette: the game keeps it within 100 degrees of ahead.")
    second_gun_clip(v, "fore", 0.22, "tank_gun_01")
    _file(v, "breaker", "Breakthrough tank", ["Tiger", "MediumTank"], "breakthroughtanks", 7,
          "Two turrets, the main one firing over the fore one: the 150 mm blasts bunkers and strongpoints, the long 50 mm keeps enemy tanks off her.")
    v.camouflage(cell=0.9)
    return v


# ---- 2. the tank hunter armoured car (class `tankhunter`) ---------------------------------------------------------

def tank_hunter_car(fine=True, tag="_"):
    """The tank hunter armoured car: 7.1 m long over her hull (9.6 m over the gun), 2.5 m wide, 2.3 m high; four men.
    Eight big wheels on four axles, the front two steering, under a low welded hull shaped like a boat: pointed in
    plan at both ends, its sides leaning in over the wheels from a knuckle along her flanks. A driver's visor in each
    end, so that she drives away as fast as she came in; stowage boxes on her flanks between the wheels. A flat
    three-man turret with a long 75 mm gun and a muzzle brake reaching far out over her nose. She finds tanks, kills
    one from far off and is gone before the rest can turn on her."""
    v = CarrierWorks("WhatIf_TankHunterCar", "Tank hunter armoured car, a what-if class (our own design)", "car", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.85), rock=(-4.0, 4.0))
    m = v.m
    R, W, RR = 0.52, 0.25, 0.30
    yw = 1.00
    axles = [2.15, 0.85, -0.85, -2.15]
    zk, zr = 1.10, 1.80                               # the knuckle along her flanks, the roof
    yk, yr = 1.14, 0.94                               # half her width at the knuckle and at the roof's edge
    xn, xnr = 3.56, 2.42                              # the nose's point, and where the roof begins over it (the tail is the same)
    # ---- the belly between the wheels; the boat over it ---------------------------------------------------------
    v.use("hull")
    v.hull([(x, 0.60, z) for x, z in [(3.30, 0.66), (2.90, 0.48), (-2.90, 0.48), (-3.30, 0.66), (-3.46, zk), (3.46, zk)]], coat, bevel=0.012)
    boat = [(xn, 0.42, 0.84), (xn, 0.42, zk + 0.06), (2.70, yk - 0.02, zk), (-2.70, yk - 0.02, zk), (-xn, 0.42, 0.84), (-xn, 0.42, zk + 0.06),
            (xnr, yr, zr), (-xnr, yr, zr), (3.00, 0.62, 0.84), (-3.00, 0.62, 0.84)]
    v.hull(boat, coat, bevel=0.014)
    up_n = [Vector((0.0, s * (zr - zk), yk - yr)).normalized() for s in (LEFT, RIGHT)]
    up_y = lambda z: yk - 0.02 - (z - zk) * (yk - 0.02 - yr) / (zr - zk)          # noqa: E731
    fl = Vector((xnr - xn, 0.0, zr - (zk + 0.06))).normalized()                # up the nose plate
    fn = Vector((fl.z, 0.0, -fl.x))
    f_at = lambda u, y, sx=1.0: Vector((sx * xn, y, zk + 0.06)) + Vector((sx * fl.x, 0.0, fl.z)) * u + Vector((sx * fn.x, 0.0, fn.z)) * 0.002   # noqa: E731
    # (the knuckle's run of plate over each wheel pair, the lip a mudguard's edge)
    for s in (LEFT, RIGHT):
        v.slab((-2.80, s * 0.90, zk - 0.03), (2.80, s * (yk - 0.01), zk), "shade", 0.004)
        if fine:
            for x in (-2.8, -1.5, 0.0, 1.5, 2.8):
                v.slab((x - 0.014, s * 0.62, zk - 0.10), (x + 0.014, s * (yk - 0.04), zk - 0.03), "shade")

    # ---- each end: the driver's visor and his lid, lamps, hooks; she drives either way -----------------------------
    for sx, lid in ((1.0, "hatch_driver"), (-1.0, "hatch_driver_rear")):
        ax = Vector((sx * fn.x, 0.0, fn.z))
        v.visor(f_at(0.62, 0.30, sx) + ax * 0.004, Vector((sx, 0.0, 0.25)).normalized(), 0.40, 0.12, 0.08)
        v.hatch_rect(lid, "hull", (sx * 2.22, 0.30, zr), (0.36, 0.46), Z, X, hinge="back" if sx > 0 else "front", opens=105.0, thick=0.024)
        v.use("hull")
        v.periscope((sx * 2.36, -0.30, zr), X * sx, (0.06, 0.12, 0.06))
        for s in (LEFT, RIGHT):
            v.headlamp(f_at(0.10, s * 0.62, sx) + ax * 0.02, Vector((sx, 0.0, 0.05)).normalized(), 0.07, stalk=0.07)
            v.hook((sx * (xn + 0.02), s * 0.28, 0.92), Vector((sx * 0.8, 0.0, -0.4)).normalized(), 0.08)
        if fine:
            v.weld(f_at(0.02, -0.40, sx), f_at(0.02, 0.40, sx), ax)
            v.weld(f_at(0.99, -0.88, sx), f_at(0.99, 0.88, sx), (ax + Z).normalized())
    if fine:
        for s, n in ((LEFT, up_n[0]), (RIGHT, up_n[1])):
            v.weld(Vector((-2.66, s * (yk - 0.02), zk + 0.006)), Vector((2.66, s * (yk - 0.02), zk + 0.006)), n)
            v.weld(Vector((-xnr + 0.03, s * (yr - 0.004), zr - 0.004)), Vector((xnr - 0.03, s * (yr - 0.004), zr - 0.004)), (n + Z).normalized())

    # ---- her flanks: stowage boxes between the wheels, a door each side, ports, tools ----------------------------------
    for i, s in enumerate((LEFT, RIGHT)):
        n = up_n[i]
        v.bin((-0.42, s * (yk - 0.04), zk + 0.002), (0.42, s * (yk + 0.22), zk + 0.34), lid_side=Y * s)
        c = Vector((1.45, s * up_y(1.45), 1.45)) + n * 0.002
        v.box((0.66, 0.42, 0.018), c + n * 0.009, coat, 0.005, turn=turn_to(X, n))           # her door between the front wheels
        if fine:
            for z in (1.32, 1.58):
                v.hinge(Vector((1.80, s * up_y(z), z)) + n * 0.02 - Z * 0.04, Vector((1.80, s * up_y(z), z)) + n * 0.02 + Z * 0.04, 0.012)
            v.handle(Vector((1.18, s * up_y(1.40), 1.40)) + n * 0.02, Vector((1.18, s * up_y(1.50), 1.50)) + n * 0.02, n, 0.008, 0.035, "steel")
            v.port(Vector((-1.40, s * up_y(1.56), 1.56)), n, 0.04)
            v.port(Vector((0.40, s * up_y(1.62), 1.62)), n, 0.04)
        v.mark(tag, (-1.55, s * (up_y(1.42) + 0.002), 1.42), n, 0.2, part="hull")
    v.shovel((-1.3, up_y(1.66) + 0.03, 1.66), 0.0, 0.95, roll=-90.0 + 16.0)
    v.crowbar((-0.4, -(up_y(1.70) + 0.03), 1.70), 0.0, 1.3)
    if fine:
        v.cable([(2.5, -(up_y(1.30) + 0.02), 1.30), (1.9, -(up_y(1.26) + 0.03), 1.26), (0.6, -(up_y(1.26) + 0.03), 1.26)], 0.013)
    # (the engine at her after end: grilles on the roof, an exhaust and silencer along each flank, low)
    for s in (LEFT, RIGHT):
        v.grille((-1.45, s * 0.50, zr + 0.012), (0.70, 0.52), Z, X, slats=8, deep=0.02)
        v.silencer((-2.95, s * (yk + 0.08), 1.02), (-2.15, s * (yk + 0.08), 1.02), 0.075, bands=2,
                   pipe=[(-1.95, s * (yk - 0.06), 1.06), (-2.15, s * (yk + 0.08), 1.02)], tail=((-2.95, s * (yk + 0.08), 1.02), (-1, s * 0.3, -0.1)))
    v.socket("exhaust.1", (-3.04, yk + 0.10, 1.01), (-1, 0.3, -0.1))
    v.socket("exhaust.2", (-3.04, -(yk + 0.10), 1.01), (-1, -0.3, -0.1))
    v.aerial("aerial", "hull", (-2.0, -0.60, zr), 2.2)
    v.vent((1.4, 0.0, zr), 0.09, 0.04)

    # ---- the wheels and what carries them: eight on four axles, the front two steering --------------------------------
    v.use("vehicle")
    for x in axles:
        v.rod((x, -yw + 0.16, R), (x, yw - 0.16, R), 0.045, "shade", 10)
        v.ball((x, 0.0, R), 0.12, "shade", 12, 8)
    v.rod((0.0, 0.0, 0.62), (2.15, 0.0, 0.62), 0.03, "steel", 8)
    v.rod((0.0, 0.0, 0.62), (-2.15, 0.0, 0.62), 0.03, "steel", 8)
    for s in (LEFT, RIGHT):
        for a, b in ((axles[0], axles[1]), (axles[2], axles[3])):        # a bow spring over each pair, its ends on the axles
            v.leaf_spring((a + b) / 2, s * 0.70, R + 0.08, R + 0.24, (a - b) / 2, leaves=7, wide=0.08)
    for letter, s in SIDES:
        v.steered_wheel(letter, s, axles[0], yw, R, W, RR, lock=28.0, holes=6)
        v.steered_wheel(letter + "2", s, axles[1], yw, R, W, RR, lock=18.0, holes=6)
        v.lorry_wheel(f"wheel_back_{letter}1", "vehicle", (axles[2], s * yw, R), s, R, W, RR, holes=6)
        v.lorry_wheel(f"wheel_back_{letter}2", "vehicle", (axles[3], s * yw, R), s, R, W, RR, holes=6)
    v.socket("dust.left", (axles[3] - 0.4, yw, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (axles[3] - 0.4, -yw, 0.05), (-1, 0, 0.3), part="vehicle")
    # (mud flaps behind the last wheels and before the first)
    v.use("hull")
    for s in (LEFT, RIGHT):
        for x in (axles[0] + R + 0.10, axles[3] - R - 0.10):
            v.slab((x - 0.006, s * 0.86, 0.52), (x + 0.006, s * (yk - 0.03), zk - 0.02), "rubber", 0.003)

    # ---- the turret: low and flat, its sides leaning in hard, a lid each for the commander and the loader -----------
    x0, th = -0.20, 0.46
    v.ring_well((x0, 0.0, zr), 0.80)
    v.part("turret", "hull", (x0, 0.0, zr), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    plan = sym_plan([(1.05, 0.56), (0.70, 0.90), (-0.85, 0.92), (-1.25, 0.62)])
    top = inset_plan(plan, [0.24, 0.22, 0.22, 0.14, 0.22, 0.22, 0.24, 0.16])
    v.prism([(x0 + x, y) for x, y in plan], zr + 0.02, zr + th, coat, top=[(x0 + x, y) for x, y in top], bevel=0.014, segs=2, angle=14.0)
    v.lathe((x0, 0, zr - 0.03), Z, [(0.06, 0.86), (0.0, 0.88), (0.0, 0.0)], "shade", v.n(40, 12))
    roof = zr + th
    v.bin((x0 - 1.62, -0.52, zr + 0.08), (x0 - 1.22, 0.52, zr + 0.40), part="turret", lid_side=-X)
    v.hatch_round("hatch_commander", "turret", Vector((x0 - 0.40, 0.36, roof)), 0.25, Z, -X, thick=0.026, opens=105.0)
    v.socket("commander", Vector((x0 - 0.40, 0.36, roof)) + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "turret", (x0 - 0.30, -0.38, roof), (0.44, 0.40), Z, X, hinge="front", opens=100.0, thick=0.024)
    v.use("turret")
    v.periscope((x0 + 0.40, 0.30, roof), X, (0.07, 0.13, 0.07))
    v.periscope((x0 - 0.12, 0.62, roof), Y, (0.06, 0.11, 0.06))
    v.vent((x0 + 0.20, -0.28, roof), 0.09, 0.04)
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 - 0.30, s * 0.80, zr + 0.22)), Vector((0, s, 0.45)), 0.04)
            v.lift_eye((x0 + 0.55, s * 0.45, roof), Z, X, 0.03)
            v.mark(tag, (x0 - 0.70, s * 0.82, zr + 0.22), Vector((0.0, s, 0.45)), 0.15, part="turret")
        v.lift_eye((x0 - 0.95, 0.0, roof), Z, Y, 0.03)

    # ---- the gun: a long 75 mm in a rounded cast mantlet, its recoil housing over the barrel's root, a double brake ----
    tx, tz = x0 + 0.92, zr + 0.24
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 18.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx + 0.02, 0, tz), Y, [(0.42, 0.0), (0.42, 0.17), (0.36, 0.22), (-0.36, 0.22), (-0.42, 0.17), (-0.42, 0.0)], "upper", v.n(32, 10))
    v.lathe((tx + 0.18, 0, tz), X, [(0.34, 0.0), (0.34, 0.10), (0.24, 0.13), (0.0, 0.16), (0.0, 0.0)], "upper", v.n(24, 8))
    if fine:
        for y in (-0.32, 0.32):
            for z in (-0.12, 0.12):
                v.bolt((tx + 0.20, y, tz + z), X, 0.013)
    v.mg((tx + 0.20, -0.30, tz + 0.02), X, 0.18)
    v.socket("coax", (tx + 0.40, -0.30, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.50, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.48], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.50, 0, tz), 4.70, 0.075, r0=0.105, r1=0.064, brake="double", steps=((0.22, 0.082),), sleeve=(0.55, 0.13))
    v.socket("muzzle", (tx + 5.22, 0, tz), X)
    m.info["bore"] = 0.075
    m.info["brake"] = True

    v.socket("engine", (-1.45, 0.0, zr + 0.05), Z, part="hull", note="the engine's grilles: smoke and the first fire")
    v.socket("fire.1", (-1.45, 0.5, zr + 0.05), Z, part="hull")
    v.socket("fire.2", (x0 - 0.40, 0.36, roof + 0.07), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (2.22, 0.30, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zr), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", f_at(0.5, 0.0), fn, part="hull")
    v.socket("hit.left", (0.0, up_y(1.4), 1.4), up_n[0], part="hull")
    v.socket("hit.right", (0.0, -up_y(1.4), 1.4), up_n[1], part="hull")
    v.socket("hit.rear", f_at(0.5, 0.0, -1.0), Vector((-fn.x, 0, fn.z)), part="hull")
    v.socket("hit.turret", (x0, 0.86, zr + 0.22), Y, part="turret")
    v.socket("case", (x0 - 0.40, -0.84, zr + 0.25), -Y, part="turret", note="where the empty case is thrown out")
    v.car_clips(recoil=0.44, kick=1.1, gun_low=-8.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver"], traverse=20.0, idle_cue="lorry_idle_loop",
                drive_cue="lorry_drive_loop", gun_cue="tank_gun_01", blow=(-0.9, 2.4, 0.4, 165.0), mg="coax")
    ctl = m.info["file"]["controls"]
    ctl["steer"]["parts"] = ["steer_l", "steer_r", "steer_l2", "steer_r2"]
    ctl["steer"]["what"] = "the game steers the first two axles inside their lock: + to the right; the second axle turns two thirds as far as the first"
    for p in ("steer_l2", "steer_r2"):
        m.clips["knocked_out"]["tracks"].append({"part": p, "turn": [[0.0, 0.0], [0.2, 0.0], [0.75, 10.0], [0.9, 8.0], [1.05, 9.0], [3.0, 9.0]]})
    ctl["reverse"] = {"what": "she has a driver at each end: she runs backward as fast as forward (no turning round)"}
    _file(v, "tankhunter", "Tank hunter armoured car", ["M8", "AssaultGun"], "tankhunters", 4,
          "Eight wheels and a driver at each end: a fast long gun that strikes from far off and is gone.")
    v.camouflage(cell=0.8)
    return v


def scorched(v, spots):
    """Soot round her exhausts (as every tank's) and, heavier, round the places in `spots`: (socket, reach, how dark,
    how far it runs back along -x). A flame gun's muzzle blackens the barrel behind it and the turret's face."""
    pipes = [Vector(s["at"]) for s in v.m.sockets if s["name"].startswith("exhaust")]
    burns = [(Vector(next(s["at"] for s in v.m.sockets if s["name"] == name)), reach, most, back) for name, reach, most, back in spots]

    def weather(p, n, paint, part):
        if part.startswith(("track", "wheel", "sprocket", "idler", "roller")):
            return 0.0, 0.0
        p = Vector(p)
        s = 0.0
        for at in pipes:
            d = p - at
            d.z *= 0.4 if d.z > 0.0 else 1.5
            s = max(s, 0.85 * max(0.0, 1.0 - max(0.0, d.length - 0.05) / 0.6) ** 1.8)
        for at, reach, most, back in burns:
            d = p - at
            if d.x < 0.0:
                d.x *= reach / max(back, reach)                  # (the soot is drawn back along the barrel by the blast)
            streak = 0.8 + 0.2 * math.sin(p.y * 37.0 + p.z * 23.0)
            s = max(s, most * streak * max(0.0, 1.0 - max(0.0, d.length - 0.03) / reach) ** 1.4)
        return 0.0, min(s, 1.0)
    v.m.weather = weather


# ---- 3. the flame tank (class `flametank`) -------------------------------------------------------------------------

def flame_tank(fine=True, tag="_"):
    """The flame tank: the medium tank's hull (5.7 m, 2.75 m wide), 2.5 m high; four men. A rounded cast turret with a
    stubby flame projector in place of a gun: a thick armoured sleeve, the projector's tube with its fuel line along
    it, a flared nozzle, and under it the small pilot-light nozzle that lights each jet; a machine gun beside it, and
    the bow gun in her front plate. On her engine deck an armoured hood over two fuel tanks lying side by side, their
    domed ends showing, pipes from its front down into her hull. Soot round the nozzle and back along the projector.
    She burns men out of bunkers, cellars and woods that shells will not clear."""
    v = Works("WhatIf_FlameTank", "Flame tank, a what-if class (our own design)", "tank", tag, fine)
    h = medium_hull(v, fine)
    sw, zr, gn, g_at, d_at, dn = h["sw"], h["zr"], h["gn"], h["g_at"], h["d_at"], h["dn"]
    dk = Vector((-2.62 + 0.85, 0, 1.30 - 1.44)).normalized()

    # ---- the fuel tanks on her engine deck, under an armoured hood -------------------------------------------------
    v.use("hull")
    base = d_at(0.98, 0.0) + dn * 0.002
    turn = turn_to(dk, dn)
    hood_long, hood_wide, hood_high = 1.22, 1.62, 0.50
    for s in (LEFT, RIGHT):                               # (the hood's two sides and its sloped front, of plate)
        v.box((hood_long, 0.03, hood_high), base + dn * (hood_high / 2) + Y * s * (hood_wide / 2 - 0.015), coat, 0.006, turn=turn)
    v.box((hood_long + 0.04, hood_wide, 0.03), base + dn * (hood_high - 0.015), coat, 0.008, turn=turn)                   # its roof
    front = base - dk * (hood_long / 2) + dn * (hood_high / 2)
    v.box((0.03, hood_wide, hood_high), front, coat, 0.006, turn=turn)
    tank_r, tank_long = 0.21, 1.50
    for s in (LEFT, RIGHT):                               # (each tank: a drum with domed ends, lying along her)
        c = base + dn * (tank_r + 0.03) + Y * s * 0.40
        a = c - dk * (tank_long / 2)
        prof = [(tank_long, 0.0), (tank_long, 0.02), (tank_long - 0.03, tank_r * 0.72), (tank_long - 0.09, tank_r * 0.97), (tank_long - 0.14, tank_r),
                (0.14, tank_r), (0.09, tank_r * 0.97), (0.03, tank_r * 0.72), (0.0, 0.02), (0.0, 0.0)]
        v.lathe(a, dk, prof, "upper", v.n(32, 10))
        if fine:
            for f in (0.12, 0.88):                        # (the straps that hold it down, over the drum where it shows)
                q = a + dk * (tank_long * f)
                v.lathe(q - dk * 0.02, dk, [(0.04, 0.0), (0.04, tank_r + 0.012), (0.0, tank_r + 0.012), (0.0, 0.0)], "shade", v.n(32, 10))
            q = a + dk * (tank_long - 0.012)
            v.lathe(q, dk, [(0.03, 0.0), (0.03, 0.05), (0.0, 0.05), (0.0, 0.0)], "steel", 12)                            # (its drain cock)
        # (the pipe from the hood's front down into her hull, to the pump under the turret)
        p0 = front + Y * s * 0.40 - dn * 0.12 - dk * 0.02
        v.tube([p0, p0 - dk * 0.16, p0 - dk * 0.22 - dn * 0.06, p0 - dk * 0.24 - dn * 0.20], 0.034, "steel", v.n(12, 6))
        v.lathe(p0 - dk * 0.24 - dn * 0.20, dn, [(0.03, 0.0), (0.03, 0.06), (0.0, 0.06), (0.0, 0.0)], "upper", v.n(16, 6))    # (where it goes through the deck)
    if fine:
        v.lathe(base + dn * hood_high + dk * 0.32, dn, [(0.05, 0.0), (0.05, 0.07), (0.02, 0.08), (0.0, 0.08), (0.0, 0.0)], "upper", 20)          # (the filler's cap)
        for s in (LEFT, RIGHT):
            for f in (-0.42, 0.0, 0.42):
                v.bolt(base + dn * (hood_high * 0.5) + dk * (f * hood_long) + Y * s * (hood_wide / 2 + 0.001), Y * s, 0.014)
        v.handle(base + dn * (hood_high + 0.002) - Y * 0.25, base + dn * (hood_high + 0.002) + Y * 0.25, dn, 0.009, 0.05, "steel")
    v.socket("fuel", base + dn * (hood_high + 0.05), dn, part="hull", note="the fuel tanks: struck here she burns at once (fire.1 and the engine together)")

    # ---- the turret: a rounded casting, the commander's cupola, the loader's lid ------------------------------------
    x0 = 0.50
    v.ring_well((x0, 0.0, zr), 0.80)
    v.part("turret", "hull", (x0, 0.0, zr), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    v.cast([{"z": zr + 0.02, "xc": x0, "af": 0.96, "ar": 1.02, "b": 0.88, "n": 2.6},
            {"z": zr + 0.28, "xc": x0, "af": 0.92, "ar": 1.00, "b": 0.86, "n": 2.6},
            {"z": zr + 0.56, "xc": x0 - 0.04, "af": 0.70, "ar": 0.86, "b": 0.70, "n": 2.4}], coat, sides=56, top=0.10, steps=3, part="turret")
    roof = zr + 0.66
    v.lathe((x0, 0, zr - 0.03), Z, [(0.06, 0.84), (0.0, 0.86), (0.0, 0.0)], "shade", v.n(36, 12), part="turret")
    cup = v.cupola((x0 - 0.38, 0.22, roof - 0.02), 0.30, 0.20, slits=5, start=math.radians(36))
    v.hatch_round("hatch_commander", "turret", cup, 0.23, Z, -X, thick=0.03, opens=105.0)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_round("hatch_loader", "turret", Vector((x0 - 0.20, -0.40, roof - 0.04)), 0.21, Z, -X, thick=0.026, opens=100.0)
    v.use("turret")
    v.periscope((x0 + 0.30, 0.26, roof - 0.04), X, (0.07, 0.13, 0.07))
    v.vent((x0 + 0.05, -0.05, roof - 0.01), 0.09, 0.04)
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 - 0.20, s * 0.86, zr + 0.30)), Vector((0, s, 0.08)), 0.045)
            v.lift_eye((x0 + 0.35, s * 0.52, roof - 0.07), Z, X, 0.032)
            v.mark(tag, (x0 - 0.50, s * 0.845, zr + 0.32), Vector((0.0, s, 0.12)), 0.15, part="turret")
        v.lift_eye((x0 - 0.70, 0.0, roof - 0.08), Z, Y, 0.032)

    # ---- the flame projector: a cast ball mount, the armoured sleeve, the tube, the nozzle and the pilot light -----------
    tx, tz = x0 + 0.84, zr + 0.30
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 25.0], kind="turn", note="lifts at its ball: + muzzle up; the game lays it")
    v.ball((tx + 0.06, 0.0, tz), 0.24, "upper", v.n(28, 10), v.n(14, 6))
    v.lathe((tx + 0.16, 0, tz), X, [(0.62, 0.0), (0.62, 0.12), (0.58, 0.135), (0.10, 0.15), (0.0, 0.17), (0.0, 0.0)], "upper", v.n(28, 10))          # the sleeve
    if fine:
        for k in range(6):
            a = 2 * math.pi * k / 6
            v.bolt((tx + 0.24, 0.165 * math.cos(a), tz + 0.165 * math.sin(a)), X, 0.012)
    v.mg((tx + 0.18, -0.30, tz + 0.02), X, 0.18)
    v.socket("coax", (tx + 0.38, -0.30, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.78, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.05], axis=(0, -1, 0), kind="slide", note="the projector's tube: it hardly moves when it fires (metres)")
    bx = tx + 0.78
    v.lathe((bx, 0, tz), X, [(0.62, 0.0), (0.62, 0.068), (0.60, 0.074), (0.04, 0.074), (0.0, 0.08), (0.0, 0.0)], "upper", v.n(24, 8))      # the tube
    v.lathe((bx + 0.58, 0, tz), X, [(0.16, 0.0), (0.16, 0.034), (0.14, 0.11), (0.10, 0.118), (0.02, 0.09), (0.0, 0.082), (0.0, 0.0)], "steel", v.n(24, 8))    # the nozzle's flare
    v.lathe((bx + 0.70, 0, tz), X, [(0.045, 0.0), (0.045, 0.03), (0.0, 0.03), (0.0, 0.0)], "black", v.n(16, 6))
    v.rod((bx + 0.02, 0.0, tz - 0.105), (bx + 0.62, 0.0, tz - 0.105), 0.018, "steel", 8)                               # the pilot light's line
    v.lathe((bx + 0.58, 0, tz - 0.105), X, [(0.10, 0.0), (0.10, 0.022), (0.08, 0.03), (0.0, 0.03), (0.0, 0.0)], "steel", 12)    # its little nozzle
    if fine:
        v.rod((bx + 0.0, 0.07, tz + 0.04), (bx + 0.56, 0.07, tz + 0.04), 0.012, "steel", 6)                            # the fuel line along the tube
        for f in (0.15, 0.40):
            v.lathe((bx + f, 0, tz), X, [(0.03, 0.0), (0.03, 0.085), (0.0, 0.085), (0.0, 0.0)], "shade", v.n(24, 8))     # (its clamps)
    v.socket("muzzle", (bx + 0.76, 0, tz), X, note="the flame's nozzle: the jet comes out here, burning (fire_start)")
    v.socket("pilot", (bx + 0.70, 0, tz - 0.105), X, part="barrel", note="the pilot light: a small flame burns here while she is fighting")
    v.m.info["bore"] = 0.06
    v.m.info["brake"] = False
    v.m.info["flame"] = {"reach": 110.0, "note": "a jet of burning fuel to about 110 m; the game draws it (puffs of flame along an arc), the model only fires it"}

    v.socket("engine", d_at(0.4, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", base + dn * 0.6, Z, part="hull", note="the fuel tanks")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (1.66, 0.50, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zr), Z, part="hull", note="the turret ring: where the blast comes from when her fuel goes")
    v.socket("hit.front", g_at(0.4, 0.0), gn, part="hull")
    v.socket("hit.left", (0.3, sw, 1.3), Y, part="hull")
    v.socket("hit.right", (0.3, -sw, 1.3), -Y, part="hull")
    v.socket("hit.rear", (-2.8, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", (x0, 0.86, zr + 0.3), Y, part="turret")
    v.socket("case", (x0 - 0.4, -0.84, zr + 0.3), -Y, part="turret", note="(no cases: she fires no shells; kept for the game's table)")
    for s in (LEFT, RIGHT):
        v.mark(tag, (0.9, s * (sw + 0.002), 1.28), Y * s, 0.2, part="hull")
    v.tank_clips(recoil=0.03, kick=0.15, gun_low=-8.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver", "hatch_radio"],
                 traverse=16.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="fire_start", blow=(-0.8, 2.6, 0.4, 168.0))
    m = v.m
    m.clips["fire"] = {"long": 2.2, "loop": False, "after": "hold",
                       "about": "the flame projector fires a two-second jet of burning fuel: the pilot light flares at 0.0, the jet from the nozzle from 0.1 to 2.0",
                       "tracks": [{"part": "barrel", "slide": [[0.0, 0.0], [0.08, 0.03], [2.0, 0.03], [2.2, 0.0]]}],
                       "events": [{"t": 0.0, "fx": "pilot_flare", "socket": "pilot", "cue": None}, {"t": 0.1, "fx": "flame_jet", "socket": "muzzle", "cue": "fire_start"},
                                  {"t": 2.0, "fx": "flame_jet_end", "socket": "muzzle", "cue": None}]}
    m.clips["pilot"] = {"long": 1.0, "loop": True, "about": "the pilot light burning at the nozzle while she fights", "tracks": [],
                        "events": [{"t": 0.0, "fx": "pilot_light", "socket": "pilot", "cue": None}]}
    m.clips["fire.rock"]["about"] = "the hull's small answer as the jet starts (a flame gun has hardly any kick)"
    m.clips["brew_up"]["about"] = "her fuel goes: a fireball from the deck's tanks and the hatches, then the blast throws the turret off its ring; holds its last pose (follow with `burn`)"
    m.clips["brew_up"]["events"].append({"t": 0.05, "fx": "fireball", "socket": "fuel", "cue": "fire_start"})
    scorched(v, [("muzzle", 0.42, 0.95, 1.6)])
    _file(v, "flametank", "Flame tank", ["MediumTank", "AssaultGun"], "flametanks", 4,
          "A flame projector in place of the gun: short reach, and men in cover break and run before it.")
    v.camouflage()
    return v


# ---- 4. the amphibious tank (class `amphtank`) ---------------------------------------------------------------------

def amphibious_tank(fine=True, tag="_"):
    """The amphibious tank: 7.6 m long, 2.9 m wide, 2.55 m high; four men. A tall watertight hull shaped like a boat:
    a long raked prow, high flat sides with a rubbing strake, the stern overhanging her tracks; six wheels a side on
    swing arms under it. Folded flat on the prow a ribbed trim vane that swings up and forward to hold her bow up in
    the water; under the stern two screws in their guard rings, a rudder behind each. Behind the turret a raised box
    with louvred sides that breathes for the engine with the deck awash; a pump's spout each side. A small welded
    turret with a short 75 mm gun. She swims ashore with the first wave, where no tank can be landed yet."""
    v = Works("WhatIf_AmphibiousTank", "Amphibious tank, a what-if class (our own design)", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.78), rock=(-4.0, 4.0))
    hw, sw, zg, zr = 0.84, 1.45, 1.00, 1.95
    xb, xs_, xt = 3.80, 2.55, -3.82                     # the prow's foot, where it meets the roof, the stern
    # ---- the hull: the tub between the tracks, the boat over and around them -----------------------------------------
    v.hull([(x, hw, z) for x, z in [(-3.30, 0.62), (-3.05, 0.42), (2.75, 0.42), (3.70, 0.92), (xb, zg), (xt, zg), (xt, 0.72)]], coat, bevel=0.014)
    boat = [(xb, sw - 0.32, zg), (3.05, sw, zg), (-3.45, sw, zg), (xt, sw - 0.16, zg + 0.08), (xt, sw - 0.16, 1.58), (-3.00, sw - 0.05, 1.86),
            (xs_, sw - 0.05, zr), (xb + 0.02, sw - 0.32, zg + 0.10)]
    v.hull(boat, coat, bevel=0.016)
    gl = Vector((xs_ - (xb + 0.02), 0, zr - (zg + 0.10))).normalized()           # up the prow
    gn = Vector((gl.z, 0, -gl.x))
    g_at = lambda u, y: Vector((xb + 0.02, y, zg + 0.10)) + gl * u + gn * 0.002   # noqa: E731
    g_len = (Vector((xs_, 0, zr)) - Vector((xb + 0.02, 0, zg + 0.10))).length
    sk = Vector((xt + 3.00, 0, 1.58 - 1.86)).normalized()                        # down the stern's slope
    st_n = Vector((-sk.z, 0, sk.x)) if sk.x < 0 else Vector((sk.z, 0, -sk.x))
    st_n = Vector((-abs(st_n.x), 0, abs(st_n.z)))
    lean = math.atan2(0.05, zr - zg)
    sn = [Vector((0, s * math.cos(lean), math.sin(lean))) for s in (1.0, -1.0)]
    y_at = lambda z: sw - (z - zg) * math.tan(lean)                              # noqa: E731
    for s in (LEFT, RIGHT):
        v.slab((xt + 0.4, s * hw, zg - 0.03), (3.05, s * sw, zg), "shade", 0.004)
        # (the rubbing strake along her side, where she lies against a ship or a jetty)
        v.slab((-3.30, s * (y_at(1.30) - 0.01), 1.26), (3.00, s * (y_at(1.30) + 0.045), 1.33), coat, 0.012, 2)
        if fine:
            for x in (-2.6, -1.3, 0.0, 1.3, 2.6):
                v.slab((x - 0.016, s * hw, zg - 0.11), (x + 0.016, s * (sw - 0.04), zg - 0.03), "shade")
    if fine:
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            v.weld(Vector((xs_ - 0.04, s * (y_at(zr) - 0.005), zr - 0.004)), Vector((-2.96, s * (y_at(zr) - 0.005), zr - 0.004)), (n + Z).normalized())
        v.weld(g_at(0.01, -1.0), g_at(0.01, 1.0), gn)

    # ---- the prow: the trim vane folded flat on it, ribbed; the driver's visor through a slot in it; lamps, a towing eye --
    vane_w, vane_l = 2.10, 0.92
    vc = g_at(0.12 + vane_l / 2, 0.0) + gn * 0.03
    v.hatch_rect("vane", "hull", vc, (vane_l, vane_w), gn, gl, hinge="back", opens=95.0, thick=0.022, handle=False, well=False)
    v.m.parts[v.m.part_at["vane"]].note = "the trim vane: + swings it up and forward off the prow (95: in the water, holding her bow up); folded flat on land"
    v.use("vane")
    if fine:
        for k in range(5):                                # (its ribs on the outer face, and the bracing along its foot)
            y = -vane_w / 2 + 0.12 + k * (vane_w - 0.24) / 4
            a = g_at(0.12 + 0.05, y) + gn * 0.052
            b = g_at(0.12 + vane_l - 0.05, y) + gn * 0.052
            v.box(((b - a).length, 0.025, 0.05), (a + b) / 2 + gn * 0.025, coat, 0.004, turn=turn_to(gl, gn))
        a, b = g_at(0.12 + 0.07, -vane_w / 2 + 0.05) + gn * 0.06, g_at(0.12 + 0.07, vane_w / 2 - 0.05) + gn * 0.06
        v.rod(a, b, 0.024, coat, 10)
    v.use("hull")
    for s in (LEFT, RIGHT):                               # (the vane's struts, folded, and its hinge brackets)
        v.slab(g_at(0.06, s * 0.90) - Vector((0.03, 0.03, 0.0)), g_at(0.06, s * 0.90) + Vector((0.07, 0.03, 0.09)), "shade", 0.004)
        v.headlamp(g_at(0.08, s * 1.18) + gn * 0.03, Vector((1, 0, 0.1)).normalized(), 0.07, stalk=0.08)
        v.hook((xb + 0.04, s * 0.42, 0.88), Vector((0.8, 0, -0.4)).normalized(), 0.09)
    v.visor(g_at(0.12 + vane_l + 0.12, 0.42) + gn * 0.004, Vector((1, 0, 0.3)).normalized(), 0.40, 0.12, 0.08)
    v.hatch_rect("hatch_driver", "hull", (xs_ - 0.28, 0.42, zr), (0.40, 0.50), Z, X, hinge="back", opens=105.0, thick=0.026)
    v.hatch_rect("hatch_radio", "hull", (xs_ - 0.28, -0.42, zr), (0.40, 0.50), Z, X, hinge="back", opens=105.0, thick=0.026)
    v.use("hull")
    v.periscope((xs_ - 0.06, -0.62, zr), X, (0.07, 0.13, 0.07))

    # ---- the deck: the breathing box behind the turret, the engine's grilles, a high exhaust, cleats and a coil of rope --
    v.slab((-1.62, -0.60, zr - 0.02), (-0.70, 0.60, zr + 0.44), coat, 0.014, 2)
    v.slab((-1.66, -0.64, zr + 0.44), (-0.66, 0.64, zr + 0.50), coat, 0.016, 2)                                 # its cap, a lip all round
    for s in (LEFT, RIGHT):
        v.louvres((-1.16, s * 0.605, zr + 0.22), (0.78, 0.30), Y * s, count=6)
    v.louvres((-1.625, 0.0, zr + 0.22), (0.80, 0.30), -X, count=6)
    v.socket("intake", (-1.16, 0.0, zr + 0.55), Z, part="hull", note="where she breathes with her deck awash")
    for s in (LEFT, RIGHT):
        v.grille((-2.50, s * 0.62, 1.84 + 0.012), (0.70, 0.56), Vector((-0.18, 0, 1)).normalized(), X, slats=8, deep=0.02)
    v.tube([(-3.10, -1.05, 1.80), (-3.10, -1.05, 2.25), (-3.16, -1.05, 2.42)], 0.065, "rust", v.n(12, 6))
    v.silencer((-3.10, -1.05, 1.86), (-3.10, -1.05, 2.18), 0.10, bands=2)
    v.lathe((-3.16, -1.05, 2.42), Vector((-0.6, 0, 0.8)).normalized(), [(0.08, 0.0), (0.08, 0.075), (0.0, 0.075), (0.0, 0.0)], "rust", 14)
    v.socket("exhaust.1", (-3.24, -1.05, 2.48), (-0.6, 0, 0.8))
    for at in ((2.25, 1.15), (2.25, -1.15), (-3.25, 1.15), (-3.25, -1.15)):    # (cleats to make her fast)
        z = zr if at[0] > 0 else 1.73
        v.slab((at[0] - 0.13, at[1] - 0.025, z), (at[0] + 0.13, at[1] + 0.025, z + 0.03), "shade", 0.006)
        v.rod((at[0] - 0.05, at[1], z), (at[0] - 0.05, at[1], z + 0.09), 0.018, "shade", 8)
        v.rod((at[0] + 0.05, at[1], z), (at[0] + 0.05, at[1], z + 0.09), 0.018, "shade", 8)
        v.rod((at[0] - 0.16, at[1], z + 0.10), (at[0] + 0.16, at[1], z + 0.10), 0.022, "shade", 8)
    if fine:
        coil = [Vector((-2.30 + 0.24 * math.cos(2 * math.pi * k / 24), 1.02 + 0.24 * math.sin(2 * math.pi * k / 24), 1.81)) for k in range(24)]
        v.tube(coil, 0.03, "rope", 6, closed=True)
        coil = [Vector((-2.30 + 0.17 * math.cos(2 * math.pi * k / 20), 1.02 + 0.17 * math.sin(2 * math.pi * k / 20), 1.84)) for k in range(20)]
        v.tube(coil, 0.03, "rope", 6, closed=True)
    v.shovel((0.3, y_at(1.60) + 0.03, 1.60), 0.0, 0.95, roll=-90.0 + math.degrees(lean))
    v.crowbar((0.6, -(y_at(1.62) + 0.03), 1.62), 0.0, 1.3)
    v.aerial("aerial", "hull", (-0.55, -0.95, zr), 2.2)
    # (a pump's spout each side, high up, that throws the water she ships back over her side)
    for i, s in enumerate((LEFT, RIGHT)):
        p0 = Vector((-0.30, s * y_at(1.66), 1.66))
        v.tube([p0 - Y * s * 0.02, p0 + Y * s * 0.10, p0 + Y * s * 0.16 - Z * 0.06], 0.045, coat, v.n(12, 6))
        v.lathe(p0 + Y * s * 0.16 - Z * 0.06, -Z, [(0.04, 0.0), (0.04, 0.055), (0.0, 0.055), (0.0, 0.0)], "black", 12)
        v.socket("pump." + ("left" if s > 0 else "right"), p0 + Y * s * 0.17 - Z * 0.1, Vector((0, s * 0.6, -0.8)).normalized(), part="hull",
                 note="the bilge pump's jet of water while she swims")

    # ---- the running gear: six wheels a side on swing arms, the drive at the back -----------------------------------
    ty, r_w = 1.10, 0.32
    xs = [-2.10, -1.26, -0.42, 0.42, 1.26, 2.10]
    v.running(xs, r_w, ty, (-2.86, 0.64, 0.30), (2.92, 0.62, 0.27), rollers=[(-1.26, 0.80, 0.08), (1.26, 0.80, 0.08)], drive="rear",
              wheel=dict(w=0.09, tyre=0.035, gap=0.08, bolts=8, sides=32, hub=0.34, axle=0.26),
              sprocket_kw=dict(teeth=14, wide=0.34, tooth=0.03, holes=0, bolts=10, sides=30, hub=0.42),
              idler_kw=dict(wide=0.3, kind="spoke", spokes=6),
              roller_kw=dict(wide=0.12, tyre=0.014, reach=0.22),
              track=dict(pitch=0.13, wide=0.40, thick=0.03, sag=0.014, style={"kind": "plate", "horn": "centre", "horn_high": 0.075, "grouser": 0.016, "cleats": True}))
    v.use("vehicle")
    if fine:
        for s in (LEFT, RIGHT):
            for x in xs:
                v.rod((x + 0.28, s * (hw + 0.01), 0.50), (x + 0.28, s * (ty - 0.19), 0.50), 0.05, "shade", 10)
                v.rod((x + 0.28, s * (ty - 0.19), 0.50), (x, s * (ty - 0.19), r_w), 0.04, "shade", 8)
    # ---- the screws and rudders under her stern: each screw in a guard ring, the rudder behind it ---------------------
    v.use("hull")
    for letter, s in (("l", LEFT), ("r", RIGHT)):
        c = Vector((-3.50, s * 0.50, 0.44))
        v.rod((-3.05, s * 0.50, 0.62), c + X * 0.06, 0.035, "steel", 10)                       # (the shaft out of her bottom)
        v.slab((-3.30, s * 0.50 - 0.015, 0.50), (-3.10, s * 0.50 + 0.015, 0.66), "shade", 0.004)         # (its bracket)
        v.lathe(c + X * 0.12, -X, [(0.22, 0.25), (0.22, 0.29), (0.0, 0.29), (0.0, 0.25)], "upper", v.n(32, 10))        # (the guard ring round the screw)
        v.rod(c + X * 0.02 + Z * 0.27, (-3.42, s * 0.50, 0.70), 0.016, "shade", 6)                                       # (and its stay up to her bottom)
        v.slab((-3.42, s * 0.50 - 0.012, 0.67), (-3.28, s * 0.50 + 0.012, 0.72), "shade", 0.003)
        v.part("screw_" + letter, "hull", c, axis=X, kind="spin", note="a screw: + drives her ahead in the water; the game spins it while she swims")
        v.lathe(c - X * 0.04, X, [(0.14, 0.0), (0.14, 0.04), (0.10, 0.07), (0.0, 0.075), (0.0, 0.0)], "brass", v.n(16, 8))
        for k in range(3):
            a = 2 * math.pi * k / 3 + (0.3 if s > 0 else -0.3)
            out = Vector((0, math.cos(a), math.sin(a)))
            side = X.cross(out)
            blade = [c + out * 0.06 - side * 0.05, c + out * 0.22 - side * 0.07 + X * 0.02, c + out * 0.235 + side * 0.02 + X * 0.04, c + out * 0.06 + side * 0.04 + X * 0.06]
            v.plate(blade, 0.008, "brass", 0.0)
        v.part("rudder_" + letter, "hull", (-3.70, s * 0.50, 0.60), axis=(0, 0, -1), limits=[-30.0, 30.0], kind="turn", note="a rudder: + to the right; the game steers her with them in the water")
        v.slab((-3.86, s * 0.50 - 0.018, 0.20), (-3.70, s * 0.50 + 0.018, 0.66), coat, 0.006)
        v.rod((-3.70, s * 0.50, 0.66), (-3.70, s * 0.50, 0.75), 0.02, "shade", 8)
        v.use("hull")
    if fine:
        for s in (LEFT, RIGHT):
            v.slab((-3.82, s * 0.50 - 0.02, 0.70), (-3.62, s * 0.50 + 0.02, 0.76), "shade", 0.003)

    # ---- the turret: welded, its sides leaning in, a lid each for the commander and the loader -----------------------------
    x0, th = 0.55, 0.56
    v.ring_well((x0, 0.0, zr), 0.72)
    v.part("turret", "hull", (x0, 0.0, zr), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    plan = sym_plan([(0.82, 0.40), (0.46, 0.80), (-0.58, 0.82), (-0.98, 0.50)])
    top = inset_plan(plan, [0.18, 0.16, 0.16, 0.10, 0.16, 0.16, 0.18, 0.12])
    v.prism([(x0 + x, y) for x, y in plan], zr + 0.02, zr + th, coat, top=[(x0 + x, y) for x, y in top], bevel=0.016, segs=2, angle=14.0)
    v.lathe((x0, 0, zr - 0.03), Z, [(0.06, 0.78), (0.0, 0.80), (0.0, 0.0)], "shade", v.n(36, 12))
    roof = zr + th
    cup = v.cupola((x0 - 0.42, 0.24, roof), 0.27, 0.16, slits=5, start=math.radians(36))
    v.hatch_round("hatch_commander", "turret", cup, 0.20, Z, -X, thick=0.028, opens=105.0)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.hatch_rect("hatch_loader", "turret", (x0 - 0.20, -0.36, roof), (0.40, 0.36), Z, X, hinge="front", opens=100.0, thick=0.024)
    v.use("turret")
    v.periscope((x0 + 0.36, 0.20, roof), X, (0.07, 0.13, 0.07))
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 - 0.15, s * 0.73, zr + 0.28)), Vector((0, s, 0.3)), 0.04)
            v.lift_eye((x0 + 0.40, s * 0.40, roof), Z, X, 0.03)
            v.mark(tag, (x0 - 0.45, s * 0.75, zr + 0.28), Vector((0.0, s, 0.3)), 0.14, part="turret")
    # the gun: a short 75 mm in a boxed mantlet, a machine gun beside it
    tx, tz = x0 + 0.78, zr + 0.28
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-8.0, 22.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.box((0.14, 0.62, 0.38), (tx + 0.10, 0.0, tz), "upper", 0.024, segs=2)
    v.lathe((tx + 0.16, 0, tz), X, [(0.22, 0.0), (0.22, 0.10), (0.15, 0.12), (0.0, 0.14), (0.0, 0.0)], "upper", v.n(24, 8))
    if fine:
        for y in (-0.25, 0.25):
            for z in (-0.14, 0.14):
                v.bolt((tx + 0.172, y, tz + z), X, 0.012)
    v.mg((tx + 0.16, -0.22, tz + 0.02), X, 0.18)
    v.socket("coax", (tx + 0.36, -0.22, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.38, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.3], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.38, 0, tz), 1.40, 0.075, r0=0.098, r1=0.086, brake=None, sleeve=(0.30, 0.11))
    v.socket("muzzle", (tx + 1.80, 0, tz), X)
    v.m.info["bore"] = 0.075
    v.m.info["brake"] = False

    v.socket("engine", (-2.50, 0.0, 1.90), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-2.50, 0.62, 1.90), Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (xs_ - 0.28, 0.42, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zr), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(g_len * 0.5, 0.0), gn, part="hull")
    v.socket("hit.left", (0.0, y_at(1.55), 1.55), sn[0], part="hull")
    v.socket("hit.right", (0.0, -y_at(1.55), 1.55), sn[1], part="hull")
    v.socket("hit.rear", (xt, 0.0, 1.3), -X, part="hull")
    v.socket("hit.turret", (x0, 0.78, zr + 0.28), Y, part="turret")
    v.socket("case", (x0 - 0.40, -0.76, zr + 0.3), -Y, part="turret", note="where the empty case is thrown out")
    v.socket("bow_wave", (xb + 0.1, 0.0, 0.9), X, part="hull", note="where her bow meets the water while she swims (the wave and spray)")
    v.socket("wake", (-3.9, 0.0, 0.45), -X, part="hull", note="her screws' wash astern while she swims")
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        v.mark(tag, (1.25, s * (y_at(1.62) + 0.002), 1.62), n, 0.22, part="hull")
    v.tank_clips(recoil=0.28, kick=0.7, gun_low=-8.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver", "hatch_radio"],
                 traverse=16.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.8, 2.5, 0.4, 168.0))
    m = v.m
    vane = m.parts[m.part_at["vane"]].limits[1]
    m.clips["vane_up"] = {"long": 2.5, "loop": False, "after": "hold", "about": "the trim vane is swung up off the prow before she enters the water (play it backward when she is ashore)",
                          "tracks": [{"part": "vane", "turn": [[0.0, 0.0], [0.3, 0.0], [1.2, vane * 0.55], [2.2, vane], [2.5, vane]]}], "events": []}
    spin = [[k * 0.125, 90.0 * k] for k in range(9)]
    m.clips["swim"] = {"long": 1.0, "loop": True, "about": "swimming: both screws turning (the game steers with the rudders), her bow wave and wash, the pumps throwing water; the hull rides the swell",
                       "tracks": [{"part": "screw_l", "turn": spin}, {"part": "screw_r", "turn": [[t, -a] for t, a in spin]},
                                  v.hull_keys(1.0, pitch=lambda t: 0.6 * math.sin(2 * math.pi * t), roll=lambda t: 0.8 * math.sin(2 * math.pi * t + 1.1))],
                       "events": [{"t": 0.0, "fx": "bow_wave", "socket": "bow_wave", "cue": None}, {"t": 0.0, "fx": "wake", "socket": "wake", "cue": None},
                                  {"t": 0.0, "fx": "water_jet", "socket": "pump.left", "cue": None}, {"t": 0.0, "fx": "water_jet", "socket": "pump.right", "cue": None},
                                  {"t": 0.0, "fx": "engine_drive", "socket": "engine", "cue": "tank_loop"}]}
    m.info["file"]["controls"]["rudders"] = {"parts": ["rudder_l", "rudder_r"], "what": "the game steers her with these in the water: + to the right", "limits": [-30.0, 30.0]}
    m.info["file"]["controls"]["screws"] = {"parts": ["screw_l", "screw_r"], "what": "the game spins them while she swims (or plays `swim`): + ahead, the right one turning the other way"}
    m.info["file"]["controls"]["vane"] = {"part": "vane", "what": "folded (0) on land, up (95) in the water: play `vane_up`"}
    m.info["swims"] = {"draught": 1.25, "note": "afloat she floats with the water about 1.25 m up her sides (over the tracks, under the rubbing strake)"}
    _file(v, "amphtank", "Amphibious tank", ["MediumTank", "LightTank"], "amphibioustanks", 4,
          "She swims: a tank that lands with the first wave from her own ship, or crosses a river where no bridge stands.")
    v.camouflage()
    return v


# ---- 5. the snow tank (class `snowtank`) ----------------------------------------------------------------------------

def fur_ring(v, lid, c, r, up, thick):
    """A fur-lined seal round a round hatch lid's edge: a soft brown roll standing just proud of the lid, so that the
    lid shuts on fur and not on frozen steel. It rides with the lid (`lid` is the lid's part)."""
    v.use(lid)
    v.lathe(c, up, [(-0.006, r - 0.012), (thick * 0.4, r + 0.022), (thick * 0.9, r + 0.016), (thick * 1.1, r - 0.006), (thick * 0.5, r - 0.02)],
            v.own_paint("cloth.fur", "5A4634"), v.n(28, 10))


def fur_frame(v, lid, c, size, up, along, thick):
    """The same seal round a square lid: a closed roll along its four edges."""
    v.use(lid)
    c, up, along = Vector(c), Vector(up).normalized(), Vector(along).normalized()
    across = up.cross(along)
    a, b = size[0] / 2 + 0.008, size[1] / 2 + 0.008
    pts = [c + along * x + across * y + up * (thick * 0.45) for x, y in ((a, b), (-a, b), (-a, -b), (a, -b))]
    ring = []
    for i in range(4):                                     # (corners rounded a little)
        p, q = pts[i], pts[(i + 1) % 4]
        ring += [p + (q - p) * 0.06, p + (q - p) * 0.94]
    v.tube(ring, 0.019, v.own_paint("cloth.fur", "5A4634"), 6, closed=True)


def snow_tank(fine=True, tag="_"):
    """The snow tank: 6.7 m long over her hull, 3.5 m wide over her tracks, 2.5 m high; four men. Built for the deep
    cold: tracks twice the width of a medium tank's, studded with ice spurs, on five big steel-tyred wheels a side (the
    rubber of a tyre cracks in hard frost), the track's top run lying on them; a low hull sloped all round to shed snow and shot; a rounded cast turret
    with a long 75 mm. On her deck a stove that warms the engine's oil before she starts (its chimney at her tail)
    and an armoured heater box that blows warm air into the fighting compartment through a lagged duct; every lid
    shuts on a seal of fur. Spare links ride on her front plate; an unditching log rides at her tail. On snow where
    other tanks wallow, she keeps going."""
    v = Works("WhatIf_SnowTank", "Snow tank, a what-if class (our own design)", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.70), rock=(-3.5, 3.5))
    hw, sw, zg, zr = 0.80, 1.74, 1.10, 1.70           # the tub's half-width; the sponsons' over the tracks; their floor; the roof
    xg, xr = 3.34, 2.06                                # the front plate's foot, and its head at the roof
    xd, zt, xt = -1.05, 1.50, -3.20                    # where the engine deck begins (falling to the tail), the tail
    lean = math.atan2(0.30, zr - zg)                   # (her sides lean in hard: snow slides off, shot glances)
    # ---- the tub between the tracks; the low sloped box over them --------------------------------------------------
    v.hull([(x, hw, z) for x, z in [(-3.05, 0.56), (-2.80, 0.42), (2.75, 0.42), (3.40, 0.84), (xg, zg), (-3.25, zg), (-3.25, 0.70)]], coat, bevel=0.014)
    v.hull([(xg, sw, zg), (xr, sw - 0.30, zr), (xd, sw - 0.30, zr), (xd, sw, zg)], coat, bevel=0.016)
    v.hull([(xd + 0.02, sw - 0.02, zg), (xd + 0.02, sw - 0.32, zr - 0.02), (xt + 0.06, sw - 0.36, zt), (xt, sw - 0.10, zg + 0.04), (xt + 0.10, sw, zg)], coat, bevel=0.016)
    gl = Vector((xr - xg, 0, zr - zg)).normalized()          # up the front plate
    gn = Vector((gl.z, 0, -gl.x))
    dk = Vector((xt + 0.06 - xd, 0, zt - zr)).normalized()   # back along the engine deck
    dn = Vector((dk.z, 0, -dk.x))
    g_at = lambda u, y: Vector((xg, y, zg)) + gl * u + gn * 0.002          # noqa: E731
    d_at = lambda u, y: Vector((xd, y, zr)) + dk * u + dn * 0.002          # noqa: E731
    g_len = (Vector((xr, 0, zr)) - Vector((xg, 0, zg))).length
    sn = [Vector((0, s * math.cos(lean), math.sin(lean))) for s in (1.0, -1.0)]
    y_at = lambda z: sw - (z - zg) * math.tan(lean)                        # noqa: E731
    for s in (LEFT, RIGHT):
        v.slab((-3.20, s * hw, zg - 0.03), (xg, s * sw, zg), "shade", 0.004)
        if fine:
            for x in (-2.4, -1.2, 0.0, 1.2, 2.4):
                v.slab((x - 0.016, s * hw, zg - 0.10), (x + 0.016, s * (sw - 0.04), zg - 0.03), "shade")
        # (a mud and snow flap hung under the sponson's nose, over the idler)
        v.slab((xg - 0.32, s * 0.86, zg - 0.26), (xg - 0.26, s * 1.72, zg - 0.01), coat, 0.006)
    # the front plate: spare links laid across it, the driver's visor over them, the bow gun in its ball; lamps, hooks
    if fine:
        v.spare_links(g_at(0.46, -0.84), g_at(0.46, 0.84), 11, 0.152, 0.86, gn, style={"kind": "plate", "horn": "centre", "grouser": 0.02, "cleats": True, "spurs": 3, "spur_high": 0.03})
        v.weld(g_at(0.02, -sw + 0.05), g_at(0.02, sw - 0.05), gn)
        v.weld(g_at(g_len - 0.006, -sw + 0.32), g_at(g_len - 0.006, sw - 0.32), (gn + Z).normalized())
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            v.weld(Vector((xr - 0.04, s * (y_at(zr) - 0.005), zr - 0.004)), Vector((xd + 0.04, s * (y_at(zr) - 0.005), zr - 0.004)), (n + Z).normalized())
            v.weld(Vector((xg - 0.04, s * (sw + 0.001), zg + 0.02)), Vector((xr + 0.02, s * (y_at(zr) + 0.001), zr - 0.02)), n)
    v.visor(g_at(g_len - 0.16, 0.50) + gn * 0.004, Vector((1, 0, 0.5)).normalized(), 0.38, 0.11, 0.09)
    v.lathe(g_at(g_len - 0.20, -0.52) - gn * 0.02, gn, [(0.13, 0.0), (0.13, 0.09), (0.07, 0.15), (0.0, 0.19), (0.0, 0.0)], "upper", v.n(24, 8))
    v.mg_ball(g_at(g_len - 0.20, -0.52) + gn * 0.10, Vector((0.94, 0, 0.16)).normalized(), 0.085, socket="hull_mg", length=0.40, ring=False)
    for s in (LEFT, RIGHT):
        v.hook((3.42, s * 0.52, 0.80), Vector((0.8, 0, -0.4)).normalized(), 0.10)
        v.hook((-3.27, s * 0.56, 0.86), -X, 0.10)
        v.headlamp(g_at(0.10, s * 1.48) + gn * 0.02, Vector((1, 0, 0.05)).normalized(), 0.072, stalk=0.08, cover=True)

    # ---- the roof: the driver's and bow gunner's lids, sealed in fur; periscopes ------------------------------------
    for name, y in (("hatch_driver", 0.50), ("hatch_radio", -0.50)):
        c = Vector((xr - 0.32, y, zr))
        v.hatch_rect(name, "hull", c, (0.38, 0.48), Z, X, hinge="back", opens=105.0, thick=0.028)
        fur_frame(v, name, c, (0.38, 0.48), Z, X, 0.028)
        v.use("hull")
    v.periscope((xr - 0.06, 0.22, zr), X, (0.07, 0.13, 0.07))
    v.periscope((xr - 0.06, -0.80, zr), X, (0.07, 0.13, 0.07))
    if fine:
        for at in ((xr - 0.10, 1.25), (xr - 0.10, -1.25), (xd + 0.12, 1.25), (xd + 0.12, -1.25)):
            v.lift_eye((at[0], at[1], zr), Z, X, 0.034)
        for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
            v.port(Vector((0.4, s * y_at(1.42), 1.42)), n, 0.042)
            v.handle(Vector((1.8, s * y_at(1.52), 1.52)), Vector((1.1, s * y_at(1.52), 1.52)), n, 0.011, 0.06)

    # ---- the engine deck: grilles, her stove and its chimney, the crew's heater and its duct, the tail -----------------
    for s in (LEFT, RIGHT):
        v.box((0.74, 0.70, 0.022), d_at(0.75, s * 0.58) + dn * 0.011, "upper", 0.006, turn=turn_to(dk, dn))
        v.grille(d_at(0.75, s * 0.58) + dn * 0.022, (0.60, 0.58), dn, X, slats=8, deep=0.022)
        v.fan_grille(d_at(1.62, s * 0.56) + dn * 0.012, 0.25, dn, part="hull")
        if fine:
            for dy in (-0.22, 0.22):
                v.hinge(d_at(1.10, s * 0.58 + dy) + dn * 0.022 - Y * 0.05, d_at(1.10, s * 0.58 + dy) + dn * 0.022 + Y * 0.05, 0.013)
    # (the stove that warms her engine's oil and water before a cold start: a drum on the right of her tail, its
    #  firebox door facing aft, a chimney up the back with a rain cap, a pipe into the engine bay)
    st = d_at(2.02, -0.92)
    v.lathe(st, dn, [(0.0, 0.0), (0.0, 0.17), (0.30, 0.17), (0.33, 0.14), (0.34, 0.0)], "upper", v.n(22, 10))
    if fine:
        for z in (0.08, 0.22):
            v.lathe(st, dn, [(z - 0.012, 0.17), (z - 0.012, 0.178), (z + 0.012, 0.178), (z + 0.012, 0.17)], "steel", v.n(22, 10))
        v.slab(st + Vector((-0.20, -0.06, 0.06)), st + Vector((-0.16, 0.06, 0.20)), "shade", 0.004)            # (its firebox door)
        v.handle(st + Vector((-0.205, -0.03, 0.15)), st + Vector((-0.205, 0.03, 0.15)), -X, 0.006, 0.025, "steel")
    ch0 = st + dn * 0.30
    v.tube([ch0, ch0 + Z * 0.40, ch0 + Z * 0.62 - X * 0.10], 0.05, "rust", v.n(12, 6))
    v.lathe(ch0 + Z * 0.62 - X * 0.10, Vector((-0.4, 0, 1)).normalized(), [(0.03, 0.0), (0.03, 0.09), (0.06, 0.09), (0.06, 0.0)], "rust", 12)
    v.tube([st + Y * 0.17 + dn * 0.12, st + Y * 0.30 + dn * 0.10 + X * 0.10, d_at(1.62, -0.30) + dn * 0.01], 0.035, "rust", 8)
    v.socket("heater", ch0 + Z * 0.66 - X * 0.12, Vector((-0.3, 0, 1)).normalized(), part="hull", note="the engine stove's chimney: a thin wisp of wood smoke while she stands in the cold")
    # (the crew's heater: an armoured box on the left of the deck, a louvred intake, a lagged duct forward into the
    #  fighting compartment behind the turret)
    v.box((0.52, 0.44, 0.30), Vector((-1.62, 1.00, zr + 0.06)), "upper", 0.014, segs=2)
    v.louvres((-1.62, 1.225, zr + 0.06), (0.40, 0.18), Y, count=4)
    if fine:
        for x in (-1.80, -1.44):
            v.bolt((x, 1.00, zr + 0.212), Z, 0.012)
    v.tube([(-1.36, 0.92, zr + 0.10), (-1.10, 0.86, zr + 0.10), (-0.80, 0.82, zr + 0.06), (-0.62, 0.80, zr + 0.01)], 0.055, v.own_paint("canvas.drab", "5C5B44"), v.n(12, 6))
    if fine:
        for x, y in ((-1.20, 0.885), (-0.92, 0.835)):
            v.lathe((x, y, zr + 0.10), Vector((1, -0.2, 0)).normalized(), [(-0.012, 0.058), (-0.012, 0.064), (0.012, 0.064), (0.012, 0.058)], "steel", 10)
    v.socket("crew_heater", (-1.62, 1.24, zr + 0.06), Y, part="hull", note="the crew heater's intake: a shimmer of warm air in the cold")
    # (the tail: two exhausts turned down under cowls, the towing pintle, the unditching log on brackets)
    xb, ez = -3.25, 0.96                                # (the tub's tail plate, and the exhausts' height on it)
    for k, s in enumerate((LEFT, RIGHT)):
        v.lathe((xb + 0.01, s * 0.62, ez), -X, [(0.0, 0.0), (0.0, 0.10), (0.13, 0.10), (0.13, 0.0)], "rust", 14)
        v.slab((xb - 0.20, s * 0.62 - 0.15, ez + 0.08), (xb + 0.005, s * 0.62 + 0.15, ez + 0.12), "upper", 0.006)
        v.slab((xb - 0.20, s * 0.62 - 0.15, ez - 0.14), (xb - 0.17, s * 0.62 + 0.15, ez + 0.12), "upper", 0.006)
        v.socket(f"exhaust.{k + 1}", (xb - 0.12, s * 0.62, ez - 0.06), (-0.4, 0, -0.6))
    if fine:
        v.slab((xb - 0.04, -0.30, 0.74), (xb + 0.005, 0.30, 0.80), "shade", 0.004)
        v.box((0.03, 0.62, 0.26), (xb - 0.01, 0.0, 0.88), "upper", 0.006)
        for y in (-0.26, -0.09, 0.09, 0.26):
            v.bolt((xb - 0.027, y, 0.99), -X, 0.013)
            v.bolt((xb - 0.027, y, 0.77), -X, 0.013)
    for s in (LEFT, RIGHT):
        v.slab((xt - 0.16, s * 1.30 - 0.04, 1.24), (xt + 0.02, s * 1.30 + 0.04, 1.30), "shade", 0.004)
        v.slab((xt - 0.16, s * 1.30 - 0.04, 1.24), (xt - 0.12, s * 1.30 + 0.04, 1.48), "shade", 0.004)
    v.lathe((xt - 0.12, 1.62, 1.41), -Y, [(0.0, 0.0), (0.0, 0.13), (3.24, 0.13), (3.24, 0.0)], "wood", v.n(16, 8))
    if fine:
        for y in (1.30, -1.30):
            v.tube([(xt - 0.02, y, 1.49), (xt - 0.10, y, 1.56), (xt - 0.22, y, 1.51), (xt - 0.25, y, 1.39), (xt - 0.18, y, 1.28)], 0.008, "steel", 5)

    # ---- tools and stowage ---------------------------------------------------------------------------------------
    v.shovel((1.6, y_at(1.40) + 0.03, 1.40), 0.0, 1.0, roll=-90.0 + math.degrees(lean))
    v.axe((0.3, y_at(1.40) + 0.03, 1.40), 0.0, 0.8, roll=-90.0 + math.degrees(lean))
    v.crowbar((1.0, -(y_at(1.40) + 0.03), 1.40), 0.0, 1.4)
    v.bin((-2.95, 0.30, zt - 0.02), (-2.50, 0.75, zt + 0.22), lid_side=-X)
    if fine:
        for s in (LEFT, RIGHT):
            v.cable([(3.2, s * (y_at(1.20) + 0.02), 1.20), (2.4, s * (y_at(1.17) + 0.03), 1.17), (-0.6, s * (y_at(1.17) + 0.03), 1.17), (-1.0, s * (y_at(1.20) + 0.02), 1.20)], 0.015)
    v.roll((-2.20, -0.70, zt + 0.14), (-2.20, -0.10, zt + 0.12), r=0.09, paint=v.own_paint("canvas.drab", "5C5B44"))
    v.aerial("aerial", "hull", (-0.90, -1.20, zr), 2.2)

    # ---- the running gear: five big steel-tyred wheels a side, paired either side of the horns; wide spurred tracks --
    ty, r_w = 1.30, 0.42
    xs = [-2.36, -1.18, 0.0, 1.18, 2.36]
    v.running(xs, r_w, ty, (-3.02, 0.70, 0.31), (3.12, 0.64, 0.29), drive="rear", top_on_wheels=True,
              wheel=dict(w=0.15, tyre=0.0, gap=0.20, kind="steel", bolts=10, sides=40, hub=0.30, axle=0.30, ribs=8),
              sprocket_kw=dict(teeth=15, wide=0.52, tooth=0.034, holes=6, bolts=10, sides=32, hub=0.40),
              idler_kw=dict(wide=0.50, kind="spoke", spokes=6),
              track=dict(pitch=0.152, wide=0.86, thick=0.034, sag=0.012,
                         style={"kind": "plate", "horn": "centre", "horn_high": 0.10, "grouser": 0.02, "cleats": True, "spurs": 3, "spur_high": 0.03, "bar": 0.30}))
    v.use("vehicle")
    if fine:
        for s in (LEFT, RIGHT):
            for x in xs:                                 # (the wheels' arms, trailing from their pivots on the hull's side)
                v.rod((x + 0.38, s * (hw + 0.01), 0.66), (x + 0.38, s * (ty - 0.30), 0.66), 0.06, "shade", 10)
                v.rod((x + 0.38, s * (ty - 0.30), 0.66), (x, s * (ty - 0.30), r_w), 0.05, "shade", 10)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((-3.02, s * (hw - 0.01), 0.70), Y * s, [(0.16, 0.0), (0.16, 0.16), (0.08, 0.24), (0.0, 0.27), (0.0, 0.0)], "upper", v.n(24, 8))
        v.lathe((3.12, s * (hw - 0.01), 0.64), Y * s, [(0.12, 0.0), (0.12, 0.08), (0.0, 0.12), (0.0, 0.0)], "upper", v.n(18, 6))

    # ---- the turret: cast and round all over, the commander's cupola and the loader's lid sealed in fur --------------
    x0, th = 0.42, 0.66
    v.ring_well((x0, 0.0, zr), 0.86)
    v.part("turret", "hull", (x0, 0.0, zr), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    v.cast([{"z": zr + 0.02, "xc": x0 - 0.04, "af": 1.00, "ar": 1.14, "b": 0.98, "n": 2.05},
            {"z": zr + 0.26, "xc": x0 - 0.04, "af": 0.98, "ar": 1.12, "b": 0.96, "n": 2.05},
            {"z": zr + 0.50, "xc": x0 - 0.08, "af": 0.82, "ar": 1.00, "b": 0.82, "n": 2.0},
            {"z": zr + th, "xc": x0 - 0.10, "af": 0.62, "ar": 0.80, "b": 0.64, "n": 2.0}], coat, sides=56, top=0.10, steps=3, part="turret")
    v.lathe((x0, 0, zr - 0.03), Z, [(0.06, 0.94), (0.0, 0.96), (0.0, 0.0)], "shade", v.n(48, 14), part="turret")
    roof = zr + th
    cup = v.cupola((x0 - 0.38, 0.30, roof - 0.02), 0.27, 0.16, slits=5, start=math.radians(36), part="turret")
    v.hatch_round("hatch_commander", "turret", cup, 0.20, Z, -X, thick=0.028, opens=105.0)
    fur_ring(v, "hatch_commander", cup, 0.20, Z, 0.028)
    v.socket("commander", cup + Z * -0.25, X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    lc = Vector((x0 - 0.18, -0.34, roof - 0.01))
    v.hatch_round("hatch_loader", "turret", lc, 0.22, Z, X, thick=0.026, opens=100.0)
    fur_ring(v, "hatch_loader", lc, 0.22, Z, 0.026)
    v.use("turret")
    v.periscope((x0 + 0.30, 0.22, roof - 0.01), X, (0.07, 0.13, 0.07))
    v.vent((x0 + 0.10, -0.04, roof - 0.005), 0.10, 0.045)
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 - 0.40, s * 0.95, zr + 0.30)), Vector((0, s, 0.2)), 0.045)
            v.lift_eye((x0 + 0.44, s * 0.48, roof - 0.04), Z, X, 0.034)
            v.handle(Vector((x0 + 0.20, s * 0.955, zr + 0.42)), Vector((x0 - 0.50, s * 0.96, zr + 0.42)), Vector((0, s, 0.25)).normalized(), 0.011, 0.06)
        v.lift_eye((x0 - 1.02, 0.0, zr + 0.40), -X, Y, 0.034)
        # (a box for the crew's kit on the turret's back, and a coil of tow rope on it)
        v.bin((x0 - 1.42, -0.52, zr + 0.18), (x0 - 1.08, 0.52, zr + 0.50), part="turret", lid_side=-X)

    # ---- the gun: a long 75 mm in a rounded cast mantlet, a single-baffle muzzle brake, an MG beside it ----------------
    tx, tz = x0 + 0.90, zr + 0.32
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-7.0, 20.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.lathe((tx - 0.02, 0, tz), Y, [(0.44, 0.0), (0.44, 0.20), (0.40, 0.25), (-0.40, 0.25), (-0.44, 0.20), (-0.44, 0.0)], "upper", v.n(36, 10))
    v.lathe((tx + 0.16, 0, tz), X, [(0.26, 0.0), (0.26, 0.14), (0.18, 0.18), (0.0, 0.21), (0.0, 0.0)], "upper", v.n(28, 10))
    v.mg((tx + 0.20, -0.30, tz + 0.02), X, 0.18)
    v.socket("coax", (tx + 0.40, -0.30, tz + 0.02), X)
    v.part("barrel", "gun", (tx + 0.42, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.34], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.42, 0, tz), 3.30, 0.075, r0=0.10, r1=0.07, brake="single", sleeve=(0.48, 0.12))
    v.socket("muzzle", (tx + 3.76, 0, tz), X)
    v.m.info["bore"] = 0.075
    v.m.info["brake"] = True

    v.use("hull")
    v.socket("engine", d_at(1.0, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.75, 0.58) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (cup.x, cup.y, cup.z + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (xr - 0.32, 0.50, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zr), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(g_len * 0.5, 0.0), gn, part="hull")
    v.socket("hit.left", (0.4, y_at(1.40), 1.40), sn[0], part="hull")
    v.socket("hit.right", (0.4, -y_at(1.40), 1.40), sn[1], part="hull")
    v.socket("hit.rear", (xt, 0.0, 1.2), -X, part="hull")
    v.socket("hit.turret", (x0, 0.96, zr + 0.30), Y, part="turret")
    v.socket("case", (x0 - 0.40, -0.94, zr + 0.34), -Y, part="turret", note="where the empty case is thrown out")
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        v.mark(tag, (-0.40, s * (y_at(1.42) + 0.002), 1.42), n, 0.22, part="hull")
        v.mark(tag, (x0 - 0.62, s * 0.92, zr + 0.36), Vector((0.0, s, 0.4)), 0.18, part="turret")
    v.tank_clips(recoil=0.34, kick=0.8, gun_low=-7.0, hatches=["hatch_commander", "hatch_loader", "hatch_driver", "hatch_radio"],
                 traverse=14.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.9, 2.8, 0.45, 170.0))
    m = v.m
    m.clips["stove"] = {"long": 4.0, "loop": True, "about": "standing in the cold with the engine off: a thin wisp from the stove's chimney, the crew heater's shimmer (play it in a winter camp)",
                        "tracks": [], "events": [{"t": 0.0, "fx": "stove_smoke", "socket": "heater", "cue": None},
                                                 {"t": 0.0, "fx": "warm_air", "socket": "crew_heater", "cue": None}]}
    m.info["cold"] = {"ground_pressure": 0.5, "note": "her tracks are twice a medium tank's width: she bears on the snow about half as hard, and the spurs grip ice. "
                      "The game slows her less than other tanks on snow, ice and mud; she starts in hard frost (the stove) and her crew fights warm."}
    _file(v, "snowtank", "Snow tank", ["MediumTank", "T34"], "snowtanks", 4,
          "Built for the deep cold: wide spurred tracks, a stove for her engine, a heater for her crew, fur round every lid.")
    v.camouflage(cell=0.85)
    return v


# ---- 6. the glider tank (class `glidertank`) ----------------------------------------------------------------------

GLIDER_HOLD = {"long": 4.80, "wide": 2.40, "high": 1.95, "note": "the assault glider's hold, inside: she goes in with about 0.1 m to spare each side"}


def glider_tank(fine=True, tag="_"):
    """The glider tank: 4.4 m long, 2.2 m wide, 1.65 m high; about 8 tonnes; two men. A small light tank built to the
    size of the heavy assault glider's hold: a low riveted hull, four wheels a side on two bogies, the drive at the
    front; a one-man turret (the commander loads, lays and fires) with a 40 mm gun and a machine gun beside it. Rings
    at her corners and along her sides take the lashings that hold her in the glider; her mudguards fold up so she
    goes in. She lands with the airborne troops behind the enemy's line and is their armour until the front arrives."""
    v = Works("WhatIf_GliderTank", "Glider tank, a what-if class (our own design)", "tank", tag, fine)
    v.root(hull_pivot=(0.0, 0.0, 0.52), rock=(-5.0, 5.0))
    hw, sw, zg, zr = 0.56, 1.06, 0.68, 1.06           # the tub's half-width; the hull's over the tracks; its floor over them; the roof
    xg, xr = 2.18, 1.40                                # the front plate's foot and head
    xt = -2.12
    # ---- the hull: a narrow tub between the tracks, a riveted box over them, a short engine deck -----------------------
    v.hull([(x, hw, z) for x, z in [(-1.98, 0.38), (-1.80, 0.30), (1.80, 0.30), (2.22, 0.56), (xg, zg), (xt, zg), (xt, 0.48)]], coat, bevel=0.012)
    v.hull([(xg, sw, zg), (xr, sw - 0.02, zr), (-1.20, sw - 0.02, zr), (xt + 0.02, sw - 0.02, zr - 0.10), (xt, sw, zg)], coat, bevel=0.012)
    gl = Vector((xr - xg, 0, zr - zg)).normalized()
    gn = Vector((gl.z, 0, -gl.x))
    g_at = lambda u, y: Vector((xg, y, zg)) + gl * u + gn * 0.002          # noqa: E731
    g_len = (Vector((xr, 0, zr)) - Vector((xg, 0, zg))).length
    dk = Vector((xt + 0.02 - (-1.20), 0, -0.10)).normalized()
    dn = Vector((dk.z, 0, -dk.x))
    d_at = lambda u, y: Vector((-1.20, y, zr)) + dk * u + dn * 0.002       # noqa: E731
    for s in (LEFT, RIGHT):
        v.slab((xt, s * hw, zg - 0.025), (xg, s * sw, zg), "shade", 0.004)
    if fine:                                               # (her plates riveted, not welded: rows along every seam)
        for s in (LEFT, RIGHT):
            for k in range(22):
                x = xt + 0.10 + k * (xr - 0.10 - xt - 0.10) / 21
                v.bolt((x, s * (sw + 0.001), zr - 0.04), Y * s, 0.011, 0.006)
                v.bolt((x, s * (sw + 0.001), zg + 0.04), Y * s, 0.011, 0.006)
            for k in range(6):
                u = 0.06 + k * (g_len - 0.12) / 5
                v.bolt(g_at(u, s * (sw - 0.04)), gn, 0.011, 0.006)
        for k in range(14):
            v.bolt(g_at(0.04, -sw + 0.08 + k * (2 * sw - 0.16) / 13), gn, 0.011, 0.006)
            v.bolt(g_at(g_len - 0.04, -sw + 0.08 + k * (2 * sw - 0.16) / 13), gn, 0.011, 0.006)
    # the front plate: the driver's visor in the middle, a lamp each side, towing eyes at the nose
    v.visor(g_at(g_len - 0.18, 0.0) + gn * 0.004, Vector((1, 0, 0.35)).normalized(), 0.36, 0.11, 0.07)
    for s in (LEFT, RIGHT):
        v.headlamp(g_at(0.14, s * 0.82) + gn * 0.02, Vector((1, 0, 0.05)).normalized(), 0.06, stalk=0.06)
        v.hook((2.24, s * 0.40, 0.50), Vector((0.8, 0, -0.4)).normalized(), 0.075)
        v.hook((-2.00, s * 0.40, 0.50), -X, 0.075)
    v.hatch_rect("hatch_driver", "hull", (xr - 0.26, 0.0, zr), (0.36, 0.52), Z, X, hinge="back", opens=105.0, thick=0.022)
    v.use("hull")
    v.periscope((xr - 0.04, 0.34, zr), X, (0.06, 0.11, 0.06))

    # ---- the tie-down rings that hold her in the glider: four at her corners, two along each side ------------------------
    ties = []
    for s in (LEFT, RIGHT):
        for x in (1.85, 0.40, -0.90, -1.95):
            at = Vector((x, s * (sw + 0.002), zg + 0.10))
            v.slab((x - 0.06, s * sw, zg + 0.04), (x + 0.06, s * (sw + 0.014), zg + 0.16), "upper", 0.004)
            if fine:
                pts = [at + Y * s * (0.016 + 0.05 * math.sin(math.pi * k / 8)) + X * (0.05 * math.cos(math.pi * k / 8)) - Z * 0.02 for k in range(9)]
                v.tube(pts, 0.011, "steel", 6)
            ties.append(at + Y * s * 0.06 - Z * 0.02)
    for k, at in enumerate(ties):
        v.socket(f"tie.{k + 1}", at, Vector((0, 1 if at.y > 0 else -1, -0.3)).normalized(), part="hull", note="a lashing ring: where a strap runs to the glider's floor while she rides in its hold")

    # ---- the engine deck: one grille, the exhaust and its silencer along the left of the tail, a can and a sheet ---------
    v.box((0.70, 0.86, 0.02), d_at(0.48, 0.0) + dn * 0.01, "upper", 0.006, turn=turn_to(dk, dn))
    v.grille(d_at(0.48, 0.0) + dn * 0.02, (0.56, 0.74), dn, X, slats=8, deep=0.02)
    if fine:
        for y in (-0.30, 0.30):
            v.hinge(d_at(0.10, y) + dn * 0.02 - Y * 0.05, d_at(0.10, y) + dn * 0.02 + Y * 0.05, 0.012)
    v.silencer((-2.18, 0.72, 0.62), (-2.18, -0.10, 0.62), 0.075, bands=2, tail=((-2.18, -0.10, 0.62), (0, -1, 0)))
    v.tube([(-2.12, 0.72, 0.56), (-2.16, 0.72, 0.58), (-2.18, 0.72, 0.62)], 0.035, "rust", 8)
    v.socket("exhaust.1", (-2.18, -0.24, 0.62), (-0.2, -1, 0))
    if fine:
        for y in (0.55, 0.10):
            v.slab((-2.15, y - 0.02, 0.56), (-2.11, y + 0.02, 0.66), "shade", 0.003)
    v.jerrycan((-1.88, -0.62, zr - 0.08), 0.0)

    # ---- the running gear: four wheels a side on two bogies, the drive at the front; narrow tracks --------------------
    ty, r_w = 0.82, 0.24
    xs = [-1.30, -0.66, 0.66, 1.30]
    v.running(xs, r_w, ty, (1.86, 0.44, 0.23), (-1.88, 0.40, 0.21), rollers=[(-0.98, 0.58, 0.06), (0.98, 0.58, 0.06)], drive="front",
              wheel=dict(w=0.06, tyre=0.03, gap=0.05, bolts=6, sides=28, hub=0.34, axle=0.2),
              sprocket_kw=dict(teeth=13, wide=0.24, tooth=0.026, holes=0, bolts=8, sides=26, hub=0.44),
              idler_kw=dict(wide=0.22, kind="spoke", spokes=5),
              roller_kw=dict(wide=0.10, tyre=0.012, reach=0.20),
              track=dict(pitch=0.10, wide=0.26, thick=0.024, sag=0.012, style={"kind": "plate", "horn": "centre", "horn_high": 0.06, "grouser": 0.012, "cleats": False}))
    v.use("vehicle")
    for s in (LEFT, RIGHT):
        for xc in (-0.98, 0.98):
            v.slab((xc - 0.42, s * (ty - 0.17), r_w - 0.04), (xc + 0.42, s * (ty - 0.13), r_w + 0.04), "shade", 0.004)
            v.slab((xc - 0.08, s * (hw + 0.005), r_w - 0.06), (xc + 0.08, s * (ty - 0.13), r_w + 0.12), "upper", 0.006)
            if fine:
                for k in range(4):                           # (the bogie's leaf spring, stacked over its beam)
                    w_ = 0.36 - k * 0.07
                    v.slab((xc - w_, s * (ty - 0.165), r_w + 0.05 + k * 0.018), (xc + w_, s * (ty - 0.135), r_w + 0.066 + k * 0.018), "shade", 0.002)
    v.use("hull")
    # (mudguards over the tracks' ends, hinged: they fold up for loading)
    for name, s in (("guard_fl", LEFT), ("guard_fr", RIGHT)):
        x0, x1 = 2.16, 2.30
        v.part(name, "hull", (x0, s * 0.82, zg - 0.01), axis=(0, -1, 0), limits=[0.0, 80.0], kind="turn", note="a front mudguard: + folds it up, for loading into the glider")
        v.slab((x0, s * 0.70, zg - 0.012), (x1, s * 0.94, zg - 0.004), "upper", 0.002)
        v.slab((x1 - 0.008, s * 0.70, zg - 0.07), (x1, s * 0.94, zg - 0.004), "upper", 0.002)
        if fine:
            v.hinge((x0, s * 0.72, zg - 0.01), (x0, s * 0.92, zg - 0.01), 0.010)
        v.use("hull")

    # ---- the turret: a one-man box, its sides leaning in, a split lid; a 40 mm and a machine gun ------------------------
    x0, th = 0.10, 0.46
    v.ring_well((x0, 0.0, zr), 0.52)
    v.part("turret", "hull", (x0, 0.0, zr), axis=(0, 0, -1), kind="turn", note="trains all round: + to the right, clockwise seen from above; the game turns it")
    plan = sym_plan([(0.58, 0.30), (0.34, 0.56), (-0.46, 0.58), (-0.70, 0.36)])
    top = inset_plan(plan, [0.14, 0.12, 0.12, 0.08, 0.12, 0.12, 0.14, 0.10])
    v.prism([(x0 + x, y) for x, y in plan], zr + 0.02, zr + th, coat, top=[(x0 + x, y) for x, y in top], bevel=0.012, segs=2, angle=14.0)
    v.lathe((x0, 0, zr - 0.025), Z, [(0.05, 0.54), (0.0, 0.56), (0.0, 0.0)], "shade", v.n(32, 10))
    roof = zr + th
    v.hatch_rect("hatch_commander", "turret", (x0 - 0.20, 0.0, roof), (0.42, 0.50), Z, X, hinge="back", opens=110.0, thick=0.022, split=True)
    v.socket("commander", (x0 - 0.20, 0.0, roof - 0.25), X, part="turret", note="the commander, head and shoulders out when the lid is open: this point is 0.25 under the hatch's rim, and his feet go 0.92 under it")
    v.use("turret")
    v.periscope((x0 + 0.28, 0.20, roof), X, (0.06, 0.11, 0.06))
    if fine:
        for s in (LEFT, RIGHT):
            v.port(Vector((x0 - 0.10, s * 0.53, zr + 0.24)), Vector((0, s, 0.3)), 0.035)
            v.lift_eye((x0 + 0.30, s * 0.30, roof), Z, X, 0.026)
            for k in range(5):
                v.bolt((x0 - 0.40 + k * 0.18, s * 0.555, zr + 0.40), Vector((0, s, 0.3)).normalized(), 0.010, 0.005)
            v.mark(tag, (x0 - 0.20, s * 0.545, zr + 0.22), Vector((0.0, s, 0.3)), 0.12, part="turret")
        v.bin((x0 - 0.98, -0.34, zr + 0.08), (x0 - 0.70, 0.34, zr + 0.34), part="turret", lid_side=-X)
    tx, tz = x0 + 0.56, zr + 0.22
    v.part("gun", "turret", (tx, 0.0, tz), axis=(0, -1, 0), limits=[-10.0, 25.0], kind="turn", note="lifts at the trunnions: + muzzle up; the game lays it")
    v.box((0.10, 0.44, 0.28), (tx + 0.06, 0.0, tz), "upper", 0.016, segs=2)
    v.lathe((tx + 0.10, 0, tz), X, [(0.12, 0.0), (0.12, 0.06), (0.08, 0.075), (0.0, 0.085), (0.0, 0.0)], "upper", v.n(20, 8))
    if fine:
        for y in (-0.17, 0.17):
            for z in (-0.10, 0.10):
                v.bolt((tx + 0.113, y, tz + z), X, 0.010)
    v.mg((tx + 0.12, 0.15, tz + 0.01), X, 0.16)
    v.socket("coax", (tx + 0.30, 0.15, tz + 0.01), X)
    v.part("barrel", "gun", (tx + 0.22, 0.0, tz), slide=(-1, 0, 0), slide_limits=[0.0, 0.20], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
    v.barrel((tx + 0.22, 0, tz), 1.80, 0.04, r0=0.055, r1=0.036, brake=None, sleeve=(0.26, 0.068))
    v.socket("muzzle", (tx + 2.02, 0, tz), X)
    v.m.info["bore"] = 0.04
    v.m.info["brake"] = False

    v.use("hull")
    v.aerial("aerial", "hull", (-0.90, 0.80, zr - 0.03), 1.6)
    v.socket("engine", d_at(0.48, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.30, 0.30) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (x0 - 0.20, 0.0, roof + 0.1), Z, part="turret", note="out of the commander's hatch")
    v.socket("fire.3", (xr - 0.26, 0.0, zr + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (x0, 0.0, zr), Z, part="hull", note="the turret ring: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(g_len * 0.5, 0.0), gn, part="hull")
    v.socket("hit.left", (0.0, sw, 0.86), Y, part="hull")
    v.socket("hit.right", (0.0, -sw, 0.86), -Y, part="hull")
    v.socket("hit.rear", (xt, 0.0, 0.80), -X, part="hull")
    v.socket("hit.turret", (x0, 0.56, zr + 0.22), Y, part="turret")
    v.socket("case", (x0 - 0.30, -0.56, zr + 0.24), -Y, part="turret", note="where the empty case is thrown out")
    for s in (LEFT, RIGHT):
        v.mark(tag, (-1.10, s * (sw + 0.002), 0.88), Y * s, 0.18, part="hull")
    v.tank_clips(recoil=0.18, kick=0.5, gun_low=-10.0, hatches=["hatch_commander", "hatch_driver"], open_lids=2,
                 traverse=24.0, idle_cue="tank_idle_loop", drive_cue="tank_loop", gun_cue="tank_gun_01", blow=(-0.6, 1.8, 0.3, 165.0))
    m = v.m
    m.clips["guards_up"] = {"long": 1.2, "loop": False, "after": "hold", "about": "the front mudguards fold up before she drives into the glider (play it backward when she is out)",
                            "tracks": [{"part": "guard_fl", "turn": [[0.0, 0.0], [1.0, 80.0], [1.2, 80.0]]}, {"part": "guard_fr", "turn": [[0.0, 0.0], [1.0, 80.0], [1.2, 80.0]]}], "events": []}
    m.info["file"]["controls"]["mudguards"] = {"parts": ["guard_fl", "guard_fr"], "what": "fold up (80) to load her into the glider: play `guards_up`"}
    m.info["glider"] = {"model": "WhatIf_AssaultGlider", "hold": GLIDER_HOLD, "lashings": [f"tie.{k + 1}" for k in range(len(ties))],
                        "note": "she rides in the assault glider's hold facing its nose; the nose swings open to the side and she drives out down two ramps"}
    _file(v, "glidertank", "Glider tank", ["LightTank"], "assaultgliders", 2,
          "Flown in by the heavy assault glider: the airborne troops' own armour, behind the enemy's line.")
    v.camouflage(cell=0.7)
    return v
