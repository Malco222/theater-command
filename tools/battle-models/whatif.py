"""What-if ships: original classes that never sailed, for a war that goes its own way.

Brian, 2026-11-19: "i like the historical ships but I also want to create new ships and make it so its not tied
directly to history"; "i want this to be a sandbox to play and make your own history". These are the shelf of ready
designs any great power can build after research (small nations with captured or stolen plans, ruling 235). They
belong to no navy: each is built in the plain navy's paint, and the game paints her in her owner's greys (the unit
table's hull grey for `hull` and `upper`), as it does the plain ships.

    WhatIf_BattleCarrier    big guns forward, a hangar and flight deck over her after half
    (the others follow: super battleship, raider cruiser, anti-air cruiser, scout submarine, escort carrier)

Every shape is our own, made from numbers. None follows any real ship's plan, and nothing is taken from any other
game. Each has the parts, sockets and clips every ship has (see the README's "Ships"): her big guns are `turret.*`
with `.gun.N`, her secondaries `sec.P*`/`sec.S*`, her light guns train (`bofors.N`), a carrier's lifts are `lift.N`
with the deck park, launch and landing places of every carrier. Her file's `whatif` names her class and the plain
models she is nearest (`like`), for a battle that has no place for her yet.
"""

import math

from mathutils import Vector

import fittings as fit
from capitals import AA_TRAINS, aa_clip, carrier_finish, slab, sponson, sym, walls_detail  # noqa: F401
from rigid import PORT, STBD, X, Y, Z, Model, lerp
from shipyard import Hull, Yard, livery, weather_for

# the crew of a ship that belongs to no navy in particular: plain blue-grey working dress, grey steel helmet
fit.CREW.setdefault("PLAIN", {"suit": "3B4552", "shirt": "6F7F92", "vest": "8A8468", "helmet": "61686E", "skin": "B4886A"})


def _file(m, kind, title, like, year_from):
    m.info.setdefault("file", {}).update({
        "class": title, "navy": None, "year": None, "stands_for": None,
        "whatif": {"kind": kind, "like": like, "from": year_from,
                   "note": "an original design that never sailed: any navy that has the research builds her, in its own paint"},
        "paint": "the plain navy's greys; the game swaps hull and upper for her owner's hull grey (the unit table)"})


def _bridge_windows(m, plan, z, high=0.9, front_of=None):
    """A row of panes round the front and sides of a house laid out by `plan`."""
    n_ = len(plan)
    for k in range(n_):
        a, b = Vector((*plan[k], 0)), Vector((*plan[(k + 1) % n_], 0))
        if front_of is not None and min(a.x, b.x) < front_of:
            continue
        d = b - a
        if d.length < 0.9:
            continue
        o_ = Vector((d.y, -d.x, 0)).normalized()
        panes = max(1, int(d.length / 1.05))
        for i in range(panes):
            c = a + d * ((i + 0.5) / panes) + o_ * 0.03
            e = d.normalized() * (d.length / panes * 0.5 - 0.09)
            m.add([c - e + Z * z, c + e + Z * z, c + e + Z * (z + high), c - e + Z * (z + high)], [(0, 1, 2, 3)], "glass", False,
                  lambda q, o=o_: o)


# ---- 1. the battle carrier ---------------------------------------------------------------------------------

def battle_carrier(fine=True):
    """A battleship in her fore half, a carrier in her after half: 236 m by 32; two triple turrets forward, the second
    firing over the first; her bridge tower, funnel and secondaries amidships; abaft them a closed hangar under a
    flight deck 118 m long, one lift, two catapults at its fore end launching past the bridge to either side, a
    range of aircraft aft. She fights a gun action and flies her own strikes, with about half a carrier's aircraft."""
    paints = livery("PLAIN", "battleship")
    paints.update({"flight": "4E5459", "plane": "5A6468"})
    paints = fit.crew_paints(paints, "PLAIN")
    m = Model("WhatIf_BattleCarrier", "Battle carrier, a what-if class (our own design)", "ship", paints, fine)
    h = Hull(L=236.0, B=32.0, D=7.0, T=9.4, sheer=4.2, sheer_aft=0.5, rake=9.0, counter=4.5, flare=0.6, fine=1.6, full=0.72)
    L, half = h.L, h.B * 0.5
    bx = -3.0                                     # (she breaks where the bridge ends and the hangar begins)
    yard = Yard(m, h, break_x=bx)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("planks")
    D = yard.deck
    Z0 = D(0.0)

    # ---- the big guns: two triple turrets forward, B over A
    S, LONG, BORE = 11.5, 18.5, 0.38
    mains = []
    for name, gx, raised in (("turret.A", 70.0, 0.0), ("turret.B", 52.0, S * 0.34)):
        tn, gs = yard.turret(name, gx, D(gx), S, 3, LONG, 0.0, BORE, raised=raised, rate=5.0, elev=(-3, 40), limits=(-150, 150))
        mains.append((tn, gs, gx, raised > 0))
        yard.fire_points.append((gx, 0.0, D(gx) + raised + S * 0.4))

    # ---- the superstructure between B and the hangar: two decks of houses, a tower, a funnel
    z1 = yard.house(41.0, 1.5, half * 0.64, Z0 - 0.3, 3.1, r=2.2)
    z2 = yard.house(37.0, 5.0, half * 0.44, z1, 2.7, r=1.6)
    m.use("fore")
    tz = z2
    tiers = ((36.0, 21.0, 6.0, 2.7), (34.5, 22.0, 5.2, 2.6), (33.0, 23.0, 4.6, 2.6), (31.8, 24.5, 4.0, 2.5))
    for k, (xf, xa, hw, hh) in enumerate(tiers):
        pl = Model.round_rect(xf, xa, -hw, hw, 1.6 if k < 2 else 1.1, 3 if fine else 1)
        slab(m, pl, tz, tz + hh, top="roof" if k == len(tiers) - 1 else "deck")
        walls_detail(yard, pl, tz, tz + hh, doors=k == 0, scuttles=k in (0, 2), rail=True)
        if fine and k in (1, 3):
            _bridge_windows(m, pl, tz + hh - 1.45, 0.95, front_of=xa + 3.0)
        tz += hh
    top_z = tz
    yard.director("director.fore", 28.5, top_z, 4.6, bearing=0.0)
    # bridge wings off the second tier, on brackets
    if fine:
        from ships import railing
        for hand in (PORT, STBD):
            wz = z2 + 2.7 + 2.6
            m.box((33.0, hand * 7.8, wz - 0.08), (3.6, 4.0, 0.16), "deck")
            railing(m, [(34.8, hand * 5.4, wz), (34.8, hand * 9.8, wz), (31.2, hand * 9.8, wz), (31.2, hand * 5.4, wz)], every=1.3)
            for dx in (-1.4, 1.4):
                m.rod((33.0 + dx, hand * 5.4, wz - 2.4), (33.0 + dx, hand * 9.4, wz - 0.15), 0.07, "upper", sides=4)
    # the pole mast abaft the tower, its air-search aerial, a surface set at its head, the gaff for her ensign
    from ships import radar
    mx = 20.0
    head = yard.mast(mx, z2, 25.0, lean=0.05, tripod=3.0, yards=((0.62, 0.18), (0.84, 0.12)), top=(0.56, 1.2))
    radar(yard, "radar.air", mx - 0.6, z2 + 14.6, 4.4, 2.2)
    radar(yard, "radar.surface", head.x + 0.1, head.z + 0.1, 1.6, 0.9, spin=True)
    yard.wire(head, (h.frame_x(1.0) + h.rake * 0.9, 0, D(L * 0.49) + 1.0), 0.025)
    m.socket("ensign", (head.x - 1.6, 0, head.z - 6.0), (-1, 0, 0), "fore", "the battle ensign flies from the mast's gaff")
    # one big raked funnel with its cap, a searchlight platform each side
    fxn = 11.0
    yard.house(fxn + 6.4, fxn - 6.4, 4.4, z2, 1.6, r=1.3, rail=False)
    yard.funnel(fxn, z2 + 1.6, 11.5, 10.5, 7.2, 0.14)
    if fine:
        for hand in (PORT, STBD):
            yard.searchlight(fxn + 3.0, hand * 5.8, z2 + 1.6, 0.6, (0.2, hand, 0))
    # an after control position on the hangar's fore end, looking aft over the flight deck
    yard.house(8.5, 2.0, 3.6, z2, 3.0, r=1.0)
    yard.windows(2.0, 3.6, z2 + 1.4, 0.9, back=-3.0)
    yard.director("director.aft", 5.0, z2 + 3.0, 3.6, bearing=180.0)

    # ---- the secondaries: three twin mounts a side on the first deck up, the middle one raised
    for i, (sx, up) in enumerate(((36.0, False), (22.0, True), (8.0, False))):
        ahead = sx > 20.0
        for hand in (PORT, STBD):
            yy = hand * (half * 0.64 - 2.4 if not up else half * 0.6 - 2.2)
            zz = z1 + (2.4 if up else 0.0) - 0.05
            if up:
                m.use(yard.half(sx))
                m.lathe((sx, yy, z1), Z, [(0.0, 2.5), (2.35, 2.5)], "upper", 14 if fine else 6, caps=(False, True))
            yard.turret(f"sec.{'P' if hand == PORT else 'S'}{i + 1}", sx, zz, 4.6, 2, 5.6, 0.0 if ahead else 180.0, 0.12, y=yy, style="medium",
                        rate=15.0, elev=(-5, 80), rest_elev=8.0, limits=(-150, 30) if (hand == PORT) == ahead else (-30, 150), floats=False)
    for hand, nm in ((PORT, "director.sec.port"), (STBD, "director.sec.starboard")):
        zp = z2 + 2.7
        m.use("fore")
        m.box((27.5, hand * 7.8, zp - 0.1), (4.4, 3.6, 0.2), "deck")
        for dx in (-1.5, 1.5):
            m.rod((27.5 + dx, hand * 5.6, zp - 2.4), (27.5 + dx, hand * 9.2, zp - 0.25), 0.08, "upper", sides=4)
        yard.director(nm, 27.5, zp, 2.6, y=hand * 7.9, bearing=hand * -60.0)

    # ---- the hangar and the flight deck over the after half
    hz = 6.6                                      # the hangar's height
    hx0, hx1 = 1.5, -L * 0.46
    hw = half * 0.93
    yard.house(hx0, bx, hw, Z0 - 0.2, hz, paint="hull", r=1.4, top=None, rail=False, walls=False)
    yard.house(bx, hx1, hw, Z0 - 0.2, hz, paint="hull", r=1.4, top=None, rail=False, walls=False)
    fd = Z0 - 0.2 + hz                            # the flight deck's underside
    iz = fd + 0.62                                # its top
    if fine:                                      # the hangar's plating: its seams, the rolling doors in its sides
        for hand in (PORT, STBD):
            x = hx0 - 8.0
            while x > hx1 + 7.0:
                yard.on(x).box((x, hand * (hw + 0.03), Z0 + hz * 0.42), (8.4, 0.08, hz * 0.6), "door")
                for dz in (0.18, 0.66):
                    yard.on(x).box((x, hand * (hw + 0.07), Z0 + hz * dz), (8.4, 0.03, 0.06), "seam")
                x -= 12.0
            yard.portholes(hx1 + 4.0, hx0 - 4.0, 2.2, every=3.6)
    # the deck's plan: square at its fore end against the bridge, rounded at its after end, overhanging the stern
    x_fore, x_aft = 1.0, -L * 0.497
    w = half * 0.97
    ra = 4.0
    aft_corner = [(x_aft + ra + math.cos(math.pi / 2 + k * math.pi / 8) * ra, w - ra + math.sin(math.pi / 2 + k * math.pi / 8) * ra) for k in range(5)]
    aft_corner += [(x_aft + ra + math.cos(math.pi + k * math.pi / 8) * ra, -w + ra + math.sin(math.pi + k * math.pi / 8) * ra) for k in range(5)]
    fore_plan = [(bx, -w), (x_fore, -w), (x_fore, w), (bx, w)]
    aft_plan = [(bx, w)] + aft_corner + [(bx, -w)]
    m.use("fore")
    m.prism(fore_plan, fd, iz, "upper", top_paint="flight", bottom=True)
    m.use("aft")
    m.prism(aft_plan, fd, iz, "upper", top_paint="flight", bottom=True)
    # pillars under its overhang at the stern
    for px in (-L * 0.47, -L * 0.488):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.6, 5.5)
            m.rod((px, yy, D(px)), (px, yy * 1.35, fd), 0.3, "upper", sides=6)
    # its markings: the centre line, the edge lines, the arrester wires, a blast screen across its fore end
    x = x_fore - 6.0
    while x > x_aft + 8.0:
        if abs(x - (-30.0)) > 10.0:
            yard.on(x).box((x, 0, iz + 0.02), (6.0, 0.4, 0.03), "line")
        x -= 11.0
    for hand in (PORT, STBD):
        for a_, b_ in ((x_fore - 1.0, bx), (bx, x_aft + 6.0)):
            yard.on((a_ + b_) * 0.5).box(((a_ + b_) * 0.5, hand * (w - 1.2), iz + 0.02), (a_ - b_, 0.28, 0.03), "line")
    m.use("aft")
    for k in range(6):
        xw = -L * 0.3 - k * L * 0.028
        m.rod((xw, -w + 1.6, iz + 0.14), (xw, w - 1.6, iz + 0.14), 0.035, "steel", sides=3, smooth=False)
        for hand in (PORT, STBD):
            m.box((xw, hand * (w - 1.4), iz + 0.1), (0.3, 0.3, 0.2), "steel")
    # the gallery round the deck's edge: a walkway a little below it with a rail, its brackets
    if fine:
        from ships import railing
        for hand in (PORT, STBD):
            for a_, b_ in ((x_fore, bx), (bx, x_aft + ra)):
                yard.on((a_ + b_) * 0.5).box(((a_ + b_) * 0.5, hand * (w + 0.75), iz - 0.95), (a_ - b_, 1.4, 0.1), "deck")
            for gx in range(int(x_fore - 2), int(x_aft + ra), -6):
                yard.on(gx).rod((gx, hand * hw, fd - 1.5), (gx, hand * (w + 1.35), iz - 1.0), 0.06, "upper", sides=4)
            railing(yard.on(-L * 0.25), [(x_fore, hand * (w + 1.45), iz - 0.9), (x_aft + ra, hand * (w + 1.45), iz - 0.9)], every=1.8)

    # ---- two catapults at the deck's fore end, each launching past the bridge on its own side: a raised track in its
    # trough, its shuttle at the after end, a ramp at its fore end, its control cab beside it, and its crew
    for hand in (PORT, STBD):
        cx, cy = -13.0, hand * (w - 4.2)
        yaw = hand * 6.0
        c_, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        mm = yard.on(cx)
        mm.box((cx, cy, iz + 0.05), (23.0, 2.2, 0.1), "black", yaw=yaw)                    # the trough
        mm.box((cx, cy, iz + 0.22), (22.0, 1.0, 0.34), "steel", yaw=yaw)                    # the track
        mm.box((cx + c_ * 11.6, cy + s_ * 11.6, iz + 0.32), (1.8, 1.6, 0.5), "steel", yaw=yaw, pitch=-14.0)   # the ramp at its end
        if fine:
            for k in range(-5, 6):
                c = Vector((cx + c_ * k * 2.0, cy + s_ * k * 2.0, iz + 0.4))
                mm.box(c, (0.08, 1.0, 0.03), "seam", yaw=yaw)
            mm.box((cx - c_ * 9.6, cy - s_ * 9.6, iz + 0.62), (1.2, 2.0, 0.5), "steel", yaw=yaw)              # its shuttle at rest
            mm.box((cx - c_ * 9.6, cy - s_ * 9.6, iz + 0.95), (0.5, 0.3, 0.3), "brass", yaw=yaw)
            ky = cy - hand * 2.8                                                                                  # its control cab, inboard
            mm.box((cx - 7.0, ky, iz + 0.9), (1.8, 1.4, 1.8), "upper")
            mm.box((cx - 6.08, ky, iz + 1.35), (0.04, 1.1, 0.5), "glass")
            for k, (dx, dy, pose) in enumerate(((-7.0, hand * -1.3, "stand"), (-10.6, hand * 1.6, "stand"), (-4.0, hand * 2.0, "stand"))):
                fit.sailor(mm, Vector((cx + dx, cy + dy, iz + 0.02)), (1.0, -hand * 0.4 if k == 1 else 0.0, 0.0), pose, navy="PLAIN")
    m.socket("launch.2", (x_fore, STBD * (w - 3.0), iz + 1.5), (math.cos(math.radians(-6)), math.sin(math.radians(-6)), 0), "fore",
             "an aircraft leaves the starboard catapult here, climbing ahead past the bridge")

    # ---- the flight deck's crew: handlers at the lift and at the head of the range, an officer by the island
    if fine:
        m.use("aft")
        for dx, dy, fx_, fy_ in ((-21.0, 5.0, 0.0, -1.0), (-21.5, -4.0, 0.2, 1.0), (-37.5, 6.5, 1.0, 0.0), (-40.0, -2.0, -1.0, 0.3),
                                 (-48.0, 9.0, 1.0, -0.4), (-49.0, -9.5, 1.0, 0.5), (-52.0, 0.5, 1.0, 0.0)):
            fit.sailor(m, Vector((dx, dy, iz + 0.02)), (fx_, fy_, 0.0), "stand", navy="PLAIN")

    # ---- the light guns: quadruple mountings on sponsons along the hangar, on B's roof, on the houses; singles forward
    q = 0
    for gx in (-14.0, -40.0, -66.0, -92.0):
        for hand in (PORT, STBD):
            q += 1
            sy = hand * (hw + 2.9)
            sponson(yard, gx, sy, fd - 2.0, 3.1, hand * hw)
            fit.bofors_quad(yard, q, gx, sy, fd - 2.0, bearing=hand * 90.0, tub=2.75, limits=(-150, 150) if False else None)
    for tn, gs, gx, raised in mains:
        if raised:
            q += 1
            p = m.parts[m.part_at[tn]]
            fit.bofors_quad(yard, q, gx - 3.4, 0.0, p.pivot.z + S * 0.3 + 0.1, bearing=0.0, tub=2.6, parent=tn)
    for gx, gy, gz, bearing in ((39.0, 4.6, z2, 0.0), (17.0, 6.6, z2, 0.0)):
        for hand in (PORT, STBD):
            q += 1
            fit.bofors_quad(yard, q, gx, hand * gy, gz, bearing=bearing + hand * -50.0, tub=2.6)
    for gx in (82.0, 86.0, 90.0, 94.0):
        for hand in (PORT, STBD):
            fit.oerlikon(yard, gx, hand * (h.half_at(gx) - 1.4), D(gx), facing=hand * 70.0, loader=(int(gx) % 8 == 2))
    for gx in (44.0, 30.0, 16.0):
        for hand in (PORT, STBD):
            fit.oerlikon(yard, gx, hand * (half * 0.64 - 0.9), z1, facing=hand * 80.0, loader=False)

    # ---- forecastle: anchors and a breakwater; bollards, rails, scuttles, the jack staff; boats on the houses
    yard.anchor_gear(L * 0.455, L * 0.385, spread=2.6, breakwater=(80.0, h.half_at(80.0) * 0.78, 1.2))
    for bx_ in (L * 0.43, L * 0.32, 44.0):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.9), D(bx_))
    yard.rails(hx0 + 1.0, L * 0.47)
    yard.portholes(hx0 + 2.0, L * 0.44, 2.2, every=3.4)
    yard.portholes(hx0 + 4.0, L * 0.42, 4.6, every=3.4)
    for hand in (PORT, STBD):
        yard.boat(15.5, hand * (half * 0.44 + 0.4), z2 + 0.3, 8.5)
    if fine:
        for vx in (60.0, 44.0):
            for hand in (PORT, STBD):
                yard.vent(vx, hand * h.half_at(vx) * 0.5, D(vx), 0.45, 1.3)
    m.use("fore")
    jx = L * 0.492
    m.rod((jx, 0, D(jx)), (jx + 0.4, 0, D(jx) + 3.4), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.4, 0, D(jx) + 3.4), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=4, r=2.4, rudders=2)

    # ---- the carrier's own: her lift, the deck park, where aircraft leave and land
    carrier_finish(yard, [(-30.0, 0.0, 13.0, 12.0, "flight")], x_fore, x_aft, iz, hz - 0.6,
                   [{"model": "DeckPark_CarrierFighter"}], x_fore, -L * 0.45, (head.x - 1.6, 0, head.z - 6.0), "fore")
    m.parts[m.part_at["deckpark"]].pivot = Vector((-56.0, 0.0, iz))
    m.sockets[:] = [s for s in m.sockets if not (s["name"] == "ensign" and s["note"].startswith("the battle ensign flies from the island"))]
    for s in m.sockets:
        if s["name"] == "launch":
            s["at"] = (x_fore, PORT * (w - 3.0), iz + 1.5)
            s["facing"] = (math.cos(math.radians(6)), math.sin(math.radians(6)), 0)
            s["note"] = "an aircraft leaves the port catapult here, climbing ahead past the bridge"

    # ---- where she burns, and her clips
    yard.fire_points += [(31.0, 0.0, z2 + 2.0), (fxn, 0.0, z2 + 2.0), (-20.0, 0.0, iz), (-60.0, 0.0, iz), (-95.0, 0.0, iz), (86.0, 0.0, D(86.0))]
    from ships import clips_for_ship, sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, mains, "battleship")
    m.clips["lift.1"] = {"long": 16.0, "loop": False, "about": "the lift goes down to the hangar, waits for an aircraft, and comes up with it",
                         "tracks": [{"part": "lift.1", "slide": [[0.0, 0.0], [5.0, hz - 0.6], [10.0, hz - 0.6], [15.0, 0.0], [16.0, 0.0]]}],
                         "events": [{"t": 0.0, "fx": "", "socket": "lift.1", "cue": "lift_motor"}]}
    aa_clip(m)
    _file(m, "battle_carrier", "Battle carrier", ["Battleship", "Carrier"], "research: Carrier aviation and Heavy battleships")
    return m, yard



# ---- pieces the gun ships share ------------------------------------------------------------------------------

def _tiers(yard, tiers, z, windows=(1, 3), top="roof", part="fore"):
    """A tower of rounded tiers, each (x fore, x aft, half width, height); panes round the fronts of the tiers named in
    `windows`. Returns the height of its top."""
    m = yard.m
    fine = m.fine
    m.use(part)
    for k, (xf, xa, hw, hh) in enumerate(tiers):
        pl = Model.round_rect(xf, xa, -hw, hw, min(1.6, hw * 0.35) if k < 2 else min(1.1, hw * 0.3), 3 if fine else 1)
        slab(m, pl, z, z + hh, top=top if k == len(tiers) - 1 else "deck")
        walls_detail(yard, pl, z, z + hh, doors=k == 0, scuttles=k not in windows, rail=True)
        if fine and k in windows:
            _bridge_windows(m, pl, z + hh - 1.45, 0.95, front_of=xa + (xf - xa) * 0.35)
        z += hh
    return z


def _wings(yard, x, y_in, y_out, z, long=3.6):
    """Bridge wings standing out each side on brackets, railed."""
    m = yard.m
    if not m.fine:
        return
    from ships import railing
    for hand in (PORT, STBD):
        m.box((x, hand * (y_in + y_out) * 0.5, z - 0.08), (long, y_out - y_in, 0.16), "deck")
        railing(m, [(x + long * 0.5, hand * y_in, z), (x + long * 0.5, hand * y_out, z), (x - long * 0.5, hand * y_out, z), (x - long * 0.5, hand * y_in, z)], every=1.3)
        for dx in (-long * 0.38, long * 0.38):
            m.rod((x + dx, hand * y_in, z - 2.4), (x + dx, hand * (y_out - 0.4), z - 0.15), 0.07, "upper", sides=4)


def _sec_directors(yard, x, y, z, part="fore", arm=2.6):
    m = yard.m
    for hand, nm in ((PORT, "director.sec.port"), (STBD, "director.sec.starboard")):
        m.use(part)
        m.box((x, hand * y, z - 0.1), (4.4, 3.6, 0.2), "deck")
        for dx in (-1.5, 1.5):
            m.rod((x + dx, hand * (y - 2.2), z - 2.4), (x + dx, hand * (y + 1.4), z - 0.25), 0.08, "upper", sides=4)
        yard.director(nm, x, z, arm, y=hand * (y + 0.1), bearing=hand * -60.0, parent=part)


def _forecastle(yard, xh, xc, breakwater, bollards, rails, ports, spread=2.4):
    h, D = yard.h, yard.deck
    yard.anchor_gear(xh, xc, spread=spread, breakwater=breakwater)
    for bx_ in bollards:
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.9), D(bx_))
    yard.rails(*rails)
    for drop, a, b in ports:
        yard.portholes(a, b, drop, every=3.4)


def _staffs(yard, jack=True, ensign_staff=False):
    m, h, D = yard.m, yard.h, yard.deck
    L = h.L
    if jack:
        m.use("fore")
        jx = L * 0.492
        m.rod((jx, 0, D(jx)), (jx + 0.4, 0, D(jx) + 3.4), 0.06, "upper", sides=4)
        m.socket("jack", (jx + 0.4, 0, D(jx) + 3.4), (1, 0, 0), "fore")
    if ensign_staff:
        m.use("aft")
        sx = -L * 0.494
        m.rod((sx + 0.6, 0, D(sx + 0.6)), (sx - 0.3, 0, D(sx + 0.6) + 6.0), 0.07, "upper", sides=4)


def _finish(m, yard, mains, cls, kind, title, like, research):
    from ships import clips_for_ship, sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, mains, cls)
    aa_clip(m)
    _file(m, kind, title, like, research)
    return m, yard


def _paints(cls="battleship"):
    paints = livery("PLAIN", cls)
    paints.update({"flight": "4E5459", "plane": "5A6468"})
    return fit.crew_paints(paints, "PLAIN")


# ---- 2. the super battleship -----------------------------------------------------------------------------------

def super_battleship(fine=True):
    """The largest gun ship anyone could build: 292 m by 40; four triple turrets of the heaviest guns, two forward and
    two aft, each inner one firing over the outer; a broad citadel; a tower whose face slopes back as it rises; two
    raked funnels; eight twin secondary turrets; her floatplanes on two catapults at her stern."""
    m = Model("WhatIf_SuperBattleship", "Super battleship, a what-if class (our own design)", "ship", _paints(), fine)
    h = Hull(L=292.0, B=40.0, D=7.8, T=11.0, sheer=5.2, sheer_aft=0.8, rake=11.0, counter=5.0, flare=0.7, fine=1.65, full=0.74)
    L, half = h.L, h.B * 0.5
    yard = Yard(m, h, break_x=3.0)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("planks")
    D = yard.deck
    Z0 = D(0.0)

    S, LONG, BORE = 13.6, 23.5, 0.46
    mains = []
    for name, gx, raised, bearing in (("turret.A", 100.0, 0.0, 0.0), ("turret.B", 79.0, S * 0.34, 0.0), ("turret.X", -79.0, S * 0.34, 180.0), ("turret.Y", -100.0, 0.0, 180.0)):
        tn, gs = yard.turret(name, gx, D(gx), S, 3, LONG, bearing, BORE, raised=raised, rate=4.0, elev=(-3, 41), limits=(-150, 150))
        mains.append((tn, gs, gx, raised > 0))
        yard.fire_points.append((gx, 0.0, D(gx) + raised + S * 0.4))

    # ---- the citadel's houses
    z1 = yard.house(66.0, -64.0, half * 0.62, Z0 - 0.3, 3.3, r=2.6)
    z2 = yard.house(60.0, -56.0, half * 0.42, z1, 3.0, r=2.0)
    # ---- the forward tower: its face slopes back tier by tier, its back stands upright
    tiers = ((58.0, 34.0, 8.0, 3.0), (55.6, 34.5, 7.2, 2.9), (53.2, 35.0, 6.5, 2.8), (50.8, 35.5, 5.9, 2.8), (48.4, 36.0, 5.3, 2.7), (46.0, 36.6, 4.6, 2.6))
    top_z = _tiers(yard, tiers, z2, windows=(1, 4))
    _wings(yard, 52.0, 7.4, 11.6, z2 + 3.0 + 2.9)
    yard.director("director.fore", 41.5, top_z, 7.0, bearing=0.0, parent="fore")
    if fine:
        from ships import railing
        m.use("fore")
        m.box((40.0, 0.0, top_z + 3.6), (5.0, 4.2, 0.16), "deck")          # the platform over the director, its radar
        for hand in (PORT, STBD):
            m.rod((38.0, hand * 1.6, top_z), (38.0, hand * 1.6, top_z + 3.5), 0.1, "upper", sides=5)
    from ships import radar
    radar(yard, "radar.fire", 40.0, top_z + 3.7, 3.0, 1.3)
    _sec_directors(yard, 46.0, 9.2, z2 + 3.0 * 2, arm=3.0)
    # ---- the foremast (a tripod abaft the tower), its aerials; two raked funnels
    mx = 31.0
    head = yard.mast(mx, z2, 33.0, lean=0.05, tripod=3.6, yards=((0.6, 0.2), (0.82, 0.14)), top=(0.52, 1.5))
    radar(yard, "radar.air", mx - 0.8, z2 + 18.0, 5.6, 2.6)
    radar(yard, "radar.surface", head.x + 0.1, head.z + 0.1, 1.8, 1.0, spin=True)
    yard.wire(head, (h.frame_x(1.0) + h.rake * 0.9, 0, D(L * 0.49) + 1.0), 0.025)
    for fxn, fh in ((17.0, 14.0), (-5.0, 13.0)):
        yard.house(fxn + 7.6, fxn - 7.6, 5.6, z2, 1.8, r=1.6, rail=False)
        yard.funnel(fxn, z2 + 1.8, fh, 12.6, 8.4, 0.15)
        if fine:
            for hand in (PORT, STBD):
                yard.searchlight(fxn + 4.0, hand * 7.0, z2 + 1.8, 0.65, (0.2, hand, 0))
    # ---- the after tower and mainmast; the ensign on its gaff
    at_z = _tiers(yard, ((-30.0, -42.0, 5.0, 2.7), (-31.0, -41.0, 4.3, 2.6), (-32.0, -40.0, 3.6, 2.4)), z2, windows=(1,), part="aft")
    yard.director("director.aft", -36.0, at_z, 5.6, bearing=180.0, parent="aft")
    mainx = -22.0
    mhead = yard.mast(mainx, z2, 24.0, lean=0.05, yards=((0.7, 0.16),))
    radar(yard, "radar.surface.aft", mhead.x, mhead.z + 0.1, 1.5, 0.9, spin=True)
    yard.wire(head, mhead, 0.025, sag=1.2)
    yard.wire(mhead, (-L * 0.49, 0, D(-L * 0.49) + 7.0), 0.025)
    m.socket("ensign", (mhead.x - 1.8, 0, mhead.z - 6.0), (-1, 0, 0), "aft", "the battle ensign flies from the mainmast's gaff")

    # ---- eight twin secondary turrets, four a side; the inner pair raised
    for i, (sx, up) in enumerate(((56.0, False), (40.0, True), (-38.0, True), (-54.0, False))):
        ahead = sx > 0
        for hand in (PORT, STBD):
            yy = hand * (half * 0.62 - 2.9 if not up else half * 0.42 + 0.4)
            zz = z1 + (0.0 if not up else 3.0) - 0.05
            yard.turret(f"sec.{'P' if hand == PORT else 'S'}{i + 1}", sx, zz, 6.0, 2, 7.6, 0.0 if ahead else 180.0, 0.15, y=yy, style="medium",
                        rate=12.0, elev=(-5, 75), rest_elev=8.0, limits=(-150, 30) if (hand == PORT) == ahead else (-30, 150), floats=False)

    # ---- light guns: quadruple mountings on the turrets' roofs and round the houses; singles along the decks
    q = 0
    for tn, gs, gx, raised in mains:
        if raised:
            q += 1
            p = m.parts[m.part_at[tn]]
            ox = -4.0 if gx > 0 else 4.0
            fit.bofors_quad(yard, q, gx + ox, 0.0, p.pivot.z + S * 0.3 + 0.1, bearing=p.rest, tub=2.6, parent=tn)
    for gx, gy, bearing in ((64.0, 7.0, 0.0), (26.0, 10.6, 0.0), (8.0, 10.6, 0.0), (-14.0, 10.6, 180.0), (-60.0, 7.0, 180.0)):
        for hand in (PORT, STBD):
            q += 1
            fit.bofors_quad(yard, q, gx, hand * gy, z2, bearing=bearing + hand * (-55.0 if bearing == 0.0 else 55.0), tub=2.7)
    for gx in (114.0, 119.0, 124.0, -116.0, -121.0, -126.0):
        for hand in (PORT, STBD):
            fit.oerlikon(yard, gx, hand * (h.half_at(gx) - 1.5), D(gx), facing=hand * 75.0, loader=(int(abs(gx)) % 2 == 0))
    for gx in (48.0, 30.0, 12.0, -6.0, -24.0, -44.0):
        for hand in (PORT, STBD):
            fit.oerlikon(yard, gx, hand * (half * 0.62 - 0.9), z1, facing=hand * 80.0, loader=False)

    # ---- her aircraft at the stern: two catapults, a crane; boats amidships under their cranes
    from ships import catapult
    tail = -L * 0.43
    for hand in (PORT, STBD):
        catapult(yard, tail, hand * h.half_at(tail) * 0.5, D(tail), 18.0, bearing=180.0 - hand * 12.0, plane=True)
    yard.crane(-L * 0.478, 0.0, D(-L * 0.478), 10.0, 9.0, bearing=0.0)
    for hand in (PORT, STBD):
        yard.boat(7.0, hand * (half * 0.42 - 2.4), z2 + 0.3, 11.0)
        yard.boat(-14.0, hand * (half * 0.42 - 2.4), z2 + 0.3, 8.5)
        yard.crane(2.0, hand * (half * 0.42 - 0.6), z2, 10.5, 8.8, bearing=180.0 - hand * 30.0)

    _forecastle(yard, L * 0.455, L * 0.395, (112.0, h.half_at(112.0) * 0.8, 1.4), (L * 0.43, L * 0.33, -L * 0.3, -L * 0.45),
                (-L * 0.488, L * 0.47), ((2.2, -L * 0.42, L * 0.44), (4.8, -L * 0.36, L * 0.42)), spread=3.0)
    if fine:
        for vx in (74.0, 60.0, -60.0, -72.0):
            for hand in (PORT, STBD):
                yard.vent(vx, hand * h.half_at(vx) * 0.55, D(vx), 0.5, 1.4)
    _staffs(yard, jack=True, ensign_staff=True)
    yard.screws_and_rudder(screws=4, r=2.8, rudders=2)
    yard.fire_points += [(48.0, 0.0, z2 + 3.0), (17.0, 0.0, z2 + 2.0), (-5.0, 0.0, z2 + 2.0), (-36.0, 0.0, z2), (118.0, 0.0, D(118.0)), (-120.0, 0.0, D(-120.0))]
    return _finish(m, yard, mains, "battleship", "super_battleship", "Super battleship", ["Battleship"], "research: Heavy battleships, then Super-heavy guns")


# ---- 3. the raider cruiser ------------------------------------------------------------------------------------

def raider_cruiser(fine=True):
    """Battleship guns on a fast cruiser's hull: 228 m by 25, a long fine clipper bow, thin armour; two twin turrets of
    big guns, one at each end; a low raked bridge, two raked funnels; a floatplane on a catapult amidships to find
    her prey; torpedo tubes each side; dual-purpose guns along her upper deck."""
    m = Model("WhatIf_RaiderCruiser", "Raider cruiser, a what-if class (our own design)", "ship", _paints("cruiser"), fine)
    h = Hull(L=228.0, B=25.0, D=7.0, T=8.0, sheer=5.8, sheer_aft=0.4, rake=14.0, counter=4.0, flare=0.85, fine=1.85, full=0.68)
    L, half = h.L, h.B * 0.5
    yard = Yard(m, h, break_x=0.0)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("planks")
    D = yard.deck
    Z0 = D(0.0)

    S, LONG, BORE = 10.6, 20.0, 0.38
    mains = []
    for name, gx, bearing in (("turret.A", 64.0, 0.0), ("turret.Y", -70.0, 180.0)):
        tn, gs = yard.turret(name, gx, D(gx), S, 2, LONG, bearing, BORE, raised=0.0, rate=6.0, elev=(-3, 40), limits=(-150, 150))
        mains.append((tn, gs, gx, False))
        yard.fire_points.append((gx, 0.0, D(gx) + S * 0.4))

    z1 = yard.house(52.0, -54.0, half * 0.6, Z0 - 0.3, 2.9, r=2.0)
    z2 = yard.house(46.0, -16.0, half * 0.42, z1, 2.6, r=1.6)
    # a low bridge, raked back hard
    tiers = ((44.0, 31.0, 4.6, 2.6), (41.4, 31.6, 4.0, 2.5), (38.8, 32.2, 3.4, 2.4))
    top_z = _tiers(yard, tiers, z2, windows=(0, 2))
    _wings(yard, 40.0, 4.0, 7.2, z2 + 2.6)
    yard.director("director.fore", 35.0, top_z, 4.4, bearing=0.0, parent="fore")
    from ships import catapult, radar
    mx = 27.0
    head = yard.mast(mx, z2, 22.0, lean=0.16, tripod=2.4, yards=((0.62, 0.16),), top=(0.5, 1.0))
    radar(yard, "radar.air", head.x + 1.0, z2 + 13.0, 3.6, 1.8)
    radar(yard, "radar.surface", head.x + 0.1, head.z + 0.1, 1.4, 0.8, spin=True)
    yard.wire(head, (h.frame_x(1.0) + h.rake * 0.9, 0, D(L * 0.49) + 1.0), 0.025)
    for fxn, fh in ((15.0, 11.0), (-6.0, 10.4)):
        yard.house(fxn + 6.0, fxn - 6.0, 3.8, z2 if fxn > -16 else z1, 1.4, r=1.2, rail=False)
        yard.funnel(fxn, (z2 if fxn > -16 else z1) + 1.4, fh, 10.0, 6.0, 0.24)
    # amidships: the catapult across her deck with its floatplane, the crane that recovers it
    catapult(yard, -26.0, 0.0, z1 + 0.1, 20.0, bearing=90.0, plane=True)
    for hand in (PORT, STBD):
        yard.crane(-18.5, hand * (half * 0.42 - 0.8), z2, 9.0, 9.5, bearing=180.0 - hand * 40.0)
    # the after control position and mainmast
    at_z = _tiers(yard, ((-36.0, -46.0, 3.6, 2.6), (-37.0, -45.0, 3.0, 2.3)), z1, windows=(1,), part="aft")
    yard.director("director.aft", -41.0, at_z, 3.6, bearing=180.0, parent="aft")
    mainx = -34.0
    mhead = yard.mast(mainx, z1, 20.0, lean=0.16, yards=((0.7, 0.14),))
    yard.wire(head, mhead, 0.025, sag=1.0)
    yard.wire(mhead, (-L * 0.49, 0, D(-L * 0.49) + 6.0), 0.025)
    m.socket("ensign", (mhead.x - 1.6, 0, mhead.z - 5.0), (-1, 0, 0), "aft", "the battle ensign flies from the mainmast's gaff")
    # torpedo tubes each side abaft the funnels
    for hand, side in ((PORT, "port"), (STBD, "starboard")):
        yard.tubes(f"tubes.{side}", -10.0, Z0 + 0.1, n=4, long=7.4, y=hand * (half * 0.6 + 1.0), rest=90.0 * -hand, limits=(-50, 50))
    # dual-purpose twin turrets: three a side on the upper deck
    for i, (sx, up) in enumerate(((46.0, False), (24.0, True), (-48.0, False))):
        ahead = sx > 0
        for hand in (PORT, STBD):
            yy = hand * (half * 0.6 - 2.3 if not up else half * 0.42 + 0.3)
            zz = z1 + (2.6 if up else 0.0) - 0.05
            yard.turret(f"sec.{'P' if hand == PORT else 'S'}{i + 1}", sx, zz, 4.4, 2, 5.4, 0.0 if ahead else 180.0, 0.12, y=yy, style="medium",
                        rate=15.0, elev=(-5, 80), rest_elev=8.0, limits=(-150, 30) if (hand == PORT) == ahead else (-30, 150), floats=False)
    _sec_directors(yard, 33.0, 6.2, z2 + 2.6 * 2 - 0.2, arm=2.4)
    q = 0
    for gx, gy, gz, bearing in ((50.0, 4.6, z1, 0.0), (8.0, 5.4, z2, 0.0), (-14.0, 5.4, z2, 180.0), (-52.0, 4.4, z1, 180.0)):
        for hand in (PORT, STBD):
            q += 1
            fit.bofors_quad(yard, q, gx, hand * gy, gz, bearing=bearing + hand * (-55.0 if bearing == 0.0 else 55.0), tub=2.5)
    for gx in (78.0, 84.0, 90.0, -84.0, -90.0):
        for hand in (PORT, STBD):
            fit.oerlikon(yard, gx, hand * (h.half_at(gx) - 1.3), D(gx), facing=hand * 75.0, loader=(int(abs(gx)) % 3 == 0))
    for hand in (PORT, STBD):
        yard.boat(3.5, hand * (half * 0.42 - 1.8), z2 + 0.3, 8.0)
    _forecastle(yard, L * 0.45, L * 0.385, (76.0, h.half_at(76.0) * 0.78, 1.1), (L * 0.43, L * 0.32, -L * 0.33, -L * 0.45),
                (-L * 0.488, L * 0.47), ((2.0, -L * 0.42, L * 0.44), (4.4, -L * 0.36, L * 0.41)))
    _staffs(yard, jack=True, ensign_staff=True)
    yard.screws_and_rudder(screws=4, r=2.2, rudders=1)
    yard.fire_points += [(38.0, 0.0, z2 + 2.0), (15.0, 0.0, z2 + 1.4), (-6.0, 0.0, z2 + 1.4), (-26.0, 0.0, z1), (82.0, 0.0, D(82.0)), (-88.0, 0.0, D(-88.0))]
    return _finish(m, yard, mains, "cruiser", "raider_cruiser", "Raider cruiser", ["Cruiser", "Battleship"], "research: Fast capital ships")


# ---- 4. the anti-air cruiser ----------------------------------------------------------------------------------

def aa_cruiser(fine=True):
    """The carriers' bodyguard: 166 m by 17; six twin dual-purpose gun houses on her middle line, three forward and three
    aft, each pair stepped up over the one before; a tall bridge under a big air-search aerial on a tripod; two
    funnels; light guns wherever there is room."""
    m = Model("WhatIf_AACruiser", "Anti-air cruiser, a what-if class (our own design)", "ship", _paints("cruiser"), fine)
    h = Hull(L=166.0, B=17.0, D=6.8, T=6.0, sheer=4.2, sheer_aft=0.4, rake=8.0, counter=3.5, flare=0.65, fine=1.75, full=0.68)
    L, half = h.L, h.B * 0.5
    yard = Yard(m, h, break_x=-2.0)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("planks")
    D = yard.deck
    Z0 = D(0.0)

    S, LONG, BORE = 5.4, 6.8, 0.13
    mains = []
    for name, gx, raised, bearing in (("turret.A", 54.0, 0.0, 0.0), ("turret.B", 45.0, 2.3, 0.0), ("turret.C", 36.0, 4.6, 0.0),
                                      ("turret.X", -36.0, 4.6, 180.0), ("turret.Y", -45.0, 2.3, 180.0), ("turret.Z", -54.0, 0.0, 180.0)):
        tn, gs = yard.turret(name, gx, D(gx), S, 2, LONG, bearing, BORE, raised=raised, style="medium", rate=15.0, elev=(-5, 80), rest_elev=10.0,
                             limits=(-160, 160), floats=False)
        mains.append((tn, gs, gx, raised > 3.0))
        yard.fire_points.append((gx, 0.0, D(gx) + raised + 2.0))

    z1 = yard.house(31.0, -31.0, half * 0.62, Z0 - 0.3, 2.7, r=1.6)
    z2 = yard.house(30.0, -6.0, half * 0.5, z1, 2.5, r=1.3)
    top_z = _tiers(yard, ((29.0, 17.0, 3.8, 2.6), (27.6, 17.6, 3.3, 2.5), (26.2, 18.4, 2.8, 2.4)), z2, windows=(0, 2))
    _wings(yard, 25.0, 3.4, 6.4, z2 + 2.6)
    yard.director("director.fore", 22.0, top_z, 3.0, bearing=0.0, parent="fore")
    from ships import radar
    radar(yard, "radar.fire", 22.0, top_z + 2.2, 2.0, 1.0, parent="director.fore", spin=False)
    # the tall tripod and its big air-search aerial: her reason for being
    mx = 12.0
    head = yard.mast(mx, z2, 25.0, lean=0.03, tripod=3.4, yards=((0.5, 0.18), (0.86, 0.12)), top=(0.62, 1.6))
    if fine:
        m.use("fore")
        m.box((mx + 0.4, 0, z2 + 15.6), (3.8, 3.2, 0.14), "deck")
    radar(yard, "radar.air", mx + 0.6, z2 + 15.8, 7.4, 3.6)
    radar(yard, "radar.surface", head.x + 0.1, head.z + 0.1, 1.5, 0.9, spin=True)
    yard.wire(head, (h.frame_x(1.0) + h.rake * 0.9, 0, D(L * 0.49) + 1.0), 0.022)
    for fxn, fz, fh in ((2.0, z2, 9.4), (-14.0, z1, 10.2)):
        yard.house(fxn + 4.8, fxn - 4.8, 3.2, fz, 1.3, r=1.0, rail=False)
        yard.funnel(fxn, fz + 1.3, fh, 8.2, 5.0, 0.1)
    at_z = _tiers(yard, ((-22.0, -30.0, 3.2, 2.5), (-23.0, -29.0, 2.7, 2.2)), z1, windows=(1,), part="aft")
    yard.director("director.aft", -26.0, at_z, 3.0, bearing=180.0, parent="aft")
    mainx = -20.0
    mhead = yard.mast(mainx, z1, 18.0, lean=0.04, yards=((0.72, 0.12),))
    radar(yard, "radar.air.2", mhead.x, mhead.z - 3.2, 2.6, 1.3)
    yard.wire(head, mhead, 0.022, sag=1.0)
    yard.wire(mhead, (-L * 0.49, 0, D(-L * 0.49) + 5.0), 0.022)
    m.socket("ensign", (mhead.x - 1.4, 0, mhead.z - 4.0), (-1, 0, 0), "aft", "the battle ensign flies from the mainmast's gaff")
    # light guns: quadruple mountings on the houses' corners and on the raised gun houses; singles along her sides
    q = 0
    for gx, gy, gz, bearing in ((29.0, 4.6, z1, 0.0), (9.0, 5.0, z2, 0.0), (-4.0, 4.6, z2, 180.0), (-30.0, 4.2, z1, 180.0)):
        for hand in (PORT, STBD):
            q += 1
            fit.bofors_quad(yard, q, gx, hand * gy, gz, bearing=bearing + hand * (-55.0 if bearing == 0.0 else 55.0), tub=2.4)
    for tn, gs, gx, raised in mains:
        if raised:
            q += 1
            p = m.parts[m.part_at[tn]]
            fit.bofors_quad(yard, q, gx + (-2.6 if gx > 0 else 2.6), 0.0, p.pivot.z + S * 0.36 + 0.05, bearing=p.rest, tub=2.2, parent=tn)
    for gx in (62.0, 66.0, 70.0, -62.0, -66.0, -70.0):
        for hand in (PORT, STBD):
            fit.oerlikon(yard, gx, hand * (h.half_at(gx) - 1.1), D(gx), facing=hand * 75.0, loader=(int(abs(gx)) % 4 == 2))
    for gx in (24.0, 16.0, -10.0, -18.0, -24.0):
        for hand in (PORT, STBD):
            fit.oerlikon(yard, gx, hand * (half * 0.62 - 0.8), z1, facing=hand * 80.0, loader=False)
    for hand in (PORT, STBD):
        yard.boat(-6.0, hand * (half * 0.5 + 0.9), z1 + 0.3, 7.0)
    _forecastle(yard, L * 0.45, L * 0.39, (60.0, h.half_at(60.0) * 0.8, 1.0), (L * 0.43, L * 0.3, -L * 0.33, -L * 0.45),
                (-L * 0.488, L * 0.47), ((2.0, -L * 0.42, L * 0.44), (4.2, -L * 0.36, L * 0.41)), spread=2.0)
    _staffs(yard, jack=True, ensign_staff=True)
    yard.screws_and_rudder(screws=2, r=2.0, rudders=1)
    yard.fire_points += [(22.0, 0.0, z2 + 2.0), (2.0, 0.0, z2 + 1.3), (-14.0, 0.0, z1 + 1.3), (-26.0, 0.0, z1), (66.0, 0.0, D(66.0)), (-66.0, 0.0, D(-66.0))]
    return _finish(m, yard, mains, "cruiser", "aa_cruiser", "Anti-air cruiser", ["Cruiser"], "research: Dual-purpose guns and Search radar")


# ---- 5. the scout submarine -----------------------------------------------------------------------------------

def scout_submarine(fine=True):
    """A big submarine that carries her own scout: 122 m by 11; a long watertight hangar faired into the fore face of
    her tower, its round door looking forward; a catapult down her fore casing to the bow; a folding crane to lift the
    floatplane back aboard; her deck gun abaft the tower. Her aircraft finds fleets; she herself raids shipping."""
    from ships import floatplane, submarine
    m, yard = submarine("WHATIF_SCOUT", fine)
    h, D = yard.h, yard.deck
    L = h.L
    # the crew of a boat of no navy in particular
    for k, v in fit.crew_paints({}, "PLAIN").items():
        if k not in m.paints:
            m.paints[k] = v
            m.paint_at[k] = len(m.paints) - 1
    x0, x1, th = 1.0, -11.0, 4.4
    hx0, hx1, r = 27.0, 0.4, 2.35                         # the hangar: fore end, after end (in the tower's face), its radius
    zc = D(14.0) + r - 0.3
    m.use("fore")
    m.lathe((hx1, 0, zc), X, [(0.0, r), (hx0 - hx1 - 0.6, r), (hx0 - hx1, r * 0.94)], "upper", 28 if fine else 12, caps=(False, False))
    # its foot faired into the casing
    m.prism([(hx0 - 0.4, -r * 0.82), (hx0 - 0.4, r * 0.82), (hx1, r * 0.82), (hx1, -r * 0.82)], D(14.0) - 0.05, zc - r * 0.4, "upper", top_paint="upper")
    # the door: a shallow dome on its hinge, with its locking wheels
    door = m.part("door.hangar", "fore", (hx0, 0, zc + r * 0.95), axis=(0, -1, 0), limits=[0, 100], kind="turn",
                  note="the hangar's door: plus swings it up and forward about its top hinge, a visor over the catapult")
    m.use(door)
    m.lathe((hx0, 0, zc), X, [(0.0, r * 0.97), (0.35, r * 0.9), (0.6, r * 0.55), (0.68, 0.0)], "upper", 28 if fine else 12, caps=(False, False))
    if fine:
        for a in (30, 150, 210, 330):                       # its dogs round the rim, the hinge across its top, a handwheel
            c = Vector((hx0 + 0.3, math.cos(math.radians(a)) * r * 0.86, zc + math.sin(math.radians(a)) * r * 0.86))
            m.box(c + X * 0.12, (0.3, 0.16, 0.16), "seam")
        m.rod((hx0 + 0.05, -r * 0.5, zc + r * 0.93), (hx0 + 0.05, r * 0.5, zc + r * 0.93), 0.12, "seam", sides=8)
        m.lathe((hx0 + 0.66, 0, zc), X, [(0.0, 0.3), (0.08, 0.3)], "seam", 12)
    m.use("fore")
    if fine:
        for k in range(1, 6):                              # the hangar's hoops: the frames that carry its plating
            xk = hx1 + (hx0 - hx1) * k / 6.0
            m.lathe((xk - 0.08, 0, zc), X, [(0.0, r + 0.04), (0.16, r + 0.04)], "upper", 28, caps=(False, False))
        from ships import railing
        railing(m, [(hx1 + 0.5, 0.9, zc + r), (hx0 - 1.0, 0.9, zc + r)], every=1.6)
        railing(m, [(hx1 + 0.5, -0.9, zc + r), (hx0 - 1.0, -0.9, zc + r)], every=1.6)
    # the catapult down the fore casing to the bow, its trolley carrying the floatplane, wings folded back
    cz = D(40.0) + 0.6
    m.prism([(L * 0.47, -0.7), (L * 0.47, 0.7), (hx0 + 1.0, 0.7), (hx0 + 1.0, -0.7)], D(40.0) - 0.1, cz, "steel", top_paint="steel")
    if fine:
        for k in range(14):
            xk = hx0 + 2.0 + k * 2.2
            m.box((xk, 0, cz + 0.03), (0.08, 1.4, 0.05), "seam")
            for hand in (PORT, STBD):
                m.rod((xk, hand * 0.6, cz - 0.05), (xk, hand * 0.9, D(xk) + 0.02), 0.06, "upper", sides=4)
    # her seaplane on the catapult's trolley: a part of its own, so the catapult can throw it off the bow
    run = L * 0.47 - (hx0 + 6.0) - 2.0
    m.part("seaplane", "fore", (hx0 + 6.0, 0.0, cz), slide=(1, 0, 0), slide_limits=[0.0, round(run, 2)], kind="slide",
           note="her floatplane on the catapult: the 'launch' clip runs it to the bow; the game then hides it and flies its own seaplane from 'launch'")
    on = yard.on
    yard.on = lambda x: m.use("seaplane")
    floatplane(yard, hx0 + 6.0, 0.0, cz - 0.6, bearing=0.0, span=7.2)
    yard.on = on
    m.use("fore")
    m.socket("launch", (L * 0.47, 0.0, cz + 1.2), facing=(1, 0, 0), part="fore", note="the end of the catapult: where her seaplane leaves the boat")
    m.clips["hangar.open"] = {"long": 5.0, "loop": False, "after": "hold", "about": "the hangar door swings up and open about its top hinge (play it backwards to shut it)",
                              "tracks": [{"part": "door.hangar", "turn": [[0.0, 0.0], [0.6, 0.0], [4.4, 100.0], [5.0, 100.0]]}],
                              "events": [{"t": 0.0, "fx": "", "socket": "launch", "cue": "hangar_door"}]}
    m.clips["launch"] = {"long": 2.4, "loop": False, "after": "hold", "about": "the catapult fires: her seaplane runs down the fore casing and off the bow. At the end hide the part 'seaplane' and fly the game's own seaplane from the 'launch' socket",
                         "tracks": [{"part": "seaplane", "slide": [[0.0, 0.0], [0.8, 0.0], [1.0, round(run * 0.12, 2)], [1.5, round(run, 2)], [2.4, round(run, 2)]]}],
                         "events": [{"t": 0.8, "fx": "gun_smoke", "socket": "launch", "cue": "catapult"}]}
    # the folding crane on her port side by the hangar door
    yard.crane(hx0 - 2.5, PORT * 1.2, zc + r - 0.1, 3.0, 4.6, bearing=40.0)
    # her crew: the plane's handlers by the catapult, a man on the hangar
    if fine:
        m.use("fore")
        for dx, dy in ((hx0 + 3.0, 1.4), (hx0 + 3.6, -1.4), (hx0 + 10.5, 1.2)):
            fit.sailor(m, Vector((dx, dy, D(dx) + 0.05)), (1.0, -0.3 * (1 if dy > 0 else -1), 0.0), "stand", navy="PLAIN")
    yard.fire_points.append(((hx0 + hx1) * 0.5, 0.0, zc + r))
    m.info["title"] = m.title = "Scout submarine, a what-if class (our own design)"
    _file(m, "scout_submarine", "Scout submarine", ["Submarine"], "research: Ocean-going submarines and Carrier aviation")
    return m, yard


# ---- 6. the escort carrier ------------------------------------------------------------------------------------

def escort_carrier(fine=True):
    """A flight deck laid over a merchant hull: 152 m by 21; an open hangar on her upper deck, a small island to
    starboard, two lifts, a catapult forward; light guns in galleries under the deck's edge and a gun on her stern.
    Cheap and quick to build: she covers a convoy where land-based aircraft cannot reach."""
    paints = _paints("carrier")
    paints["flight"] = "B79E74"
    m = Model("WhatIf_EscortCarrier", "Escort carrier, a what-if class (our own design)", "ship", paints, fine)
    h = Hull(L=152.0, B=21.0, D=8.2, T=7.6, sheer=1.4, sheer_aft=0.3, rake=5.0, counter=3.0, flare=0.25, fine=1.45, full=0.8)
    L, half = h.L, h.B * 0.5
    bx = -2.0
    yard = Yard(m, h, break_x=bx)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("deck")
    D = yard.deck
    Z0 = D(0.0)
    hz = 5.6
    hx0, hx1 = L * 0.4, -L * 0.42
    hw = half * 0.94
    yard.house(hx0, bx, hw, Z0 - 0.1, hz, paint="hull", r=1.0, top=None, rail=False, walls=False)
    yard.house(bx, hx1, hw, Z0 - 0.1, hz, paint="hull", r=1.0, top=None, rail=False, walls=False)
    fd = Z0 - 0.1 + hz
    iz = fd + 0.5
    if fine:      # the open hangar's sides: roller curtains between frames
        x = hx0 - 6.0
        while x > hx1 + 5.0:
            for hand in (PORT, STBD):
                yard.on(x).box((x, hand * (hw + 0.03), Z0 + hz * 0.45), (7.0, 0.08, hz * 0.62), "door")
                yard.on(x).box((x - 4.2, hand * (hw + 0.06), Z0 + hz * 0.5), (0.3, 0.12, hz * 0.95), "upper")
            x -= 9.0
    # the flight deck, on its own girders over the bow and stern
    x_fore, x_aft = L * 0.482, -L * 0.49
    w = half * 1.02
    rf, ra = 3.0, 2.0

    def corner(cx, cy, rr, a0):
        return [(cx + math.cos(a0 + k * math.pi / 8) * rr, cy + math.sin(a0 + k * math.pi / 8) * rr) for k in range(5)]
    fore_plan = [(bx, -w), (L * 0.3, -w)] + corner(x_fore - rf, -w * 0.9 + rf, rf, -math.pi / 2) + corner(x_fore - rf, w * 0.9 - rf, rf, 0.0) + [(L * 0.3, w), (bx, w)]
    aft_plan = [(bx, w)] + corner(x_aft + ra, w - ra, ra, math.pi / 2) + corner(x_aft + ra, -w + ra, ra, math.pi) + [(bx, -w)]
    m.use("fore")
    m.prism(fore_plan, fd, iz, "upper", top_paint="flight", bottom=True)
    m.use("aft")
    m.prism(aft_plan, fd, iz, "upper", top_paint="flight", bottom=True)
    for px in (L * 0.46, L * 0.43, -L * 0.455, -L * 0.475):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.6, 4.0)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.4, fd), 0.24, "upper", sides=6)
    if fine:     # the deck's planks run fore and aft: their seams; the lines painted on it; the arrester wires
        for k in range(-7, 8):
            yy = k * w / 8.0
            yard.on(L * 0.2).box((L * 0.2, yy, iz + 0.012), (L * 0.5, 0.04, 0.01), "seam")
            yard.on(-L * 0.25).box((-L * 0.25, yy, iz + 0.012), (L * 0.46, 0.04, 0.01), "seam")
    x = x_fore - 8.0
    while x > x_aft + 6.0:
        yard.on(x).box((x, 0, iz + 0.02), (5.0, 0.35, 0.03), "line")
        x -= 10.0
    m.use("aft")
    for k in range(6):
        xw = -L * 0.22 - k * L * 0.035
        m.rod((xw, -w + 1.4, iz + 0.12), (xw, w - 1.4, iz + 0.12), 0.03, "steel", sides=3, smooth=False)
    # the catapult on the port side forward
    mm = yard.on(L * 0.36)
    mm.box((L * 0.36, PORT * 4.0, iz + 0.05), (26.0, 1.6, 0.08), "black")
    mm.box((L * 0.36, PORT * 4.0, iz + 0.14), (25.0, 0.8, 0.16), "steel")
    # the island: a small house on the starboard edge forward of amidships, its bridge, mast and aerial, the funnel pipes
    ix = L * 0.17
    yc = STBD * (w - 1.8)
    t1 = yard.house(ix + 6.0, ix - 6.0, 1.7, iz, 2.8, r=0.8, y=yc, rail=False)
    t2 = yard.house(ix + 5.2, ix - 2.4, 1.9, t1, 2.4, r=0.8, y=yc + STBD * 0.3)
    yard.windows(ix + 5.2, 1.9, t2 - 1.4, 0.9, y=yc + STBD * 0.3, back=2.6)
    yard.director("director.fore", ix + 1.4, t2, 2.0, y=yc + STBD * 0.3, parent=yard.half(ix))
    from ships import radar
    head = yard.mast(ix - 3.8, t1, 13.0, lean=0.02, tripod=1.2, yards=((0.7, 0.12),), y=yc)
    radar(yard, "radar.air", head.x + 0.5, head.z - 3.6, 3.2, 1.6, y=yc)
    m.socket("ensign", (head.x - 1.0, yc, head.z - 2.0), (-1, 0, 0), yard.half(head.x), "the battle ensign flies from the island's mast")
    # her diesel's exhausts: two pipes led out over the starboard side abaft the island
    for k, fx_ in enumerate((-L * 0.06, -L * 0.1)):
        mm = yard.on(fx_)
        a = Vector((fx_, STBD * (hw - 0.4), fd - 1.6))
        b = Vector((fx_, STBD * (w + 1.6), fd - 1.4))
        e = Vector((fx_ - 0.4, STBD * (w + 1.9), iz + 2.4))
        mm.rod(a, b, 0.7, "upper", sides=12 if fine else 6)
        mm.rod(b, e, 0.7, "upper", sides=12 if fine else 6)
        mm.ball(b, (0.72, 0.72, 0.72), "upper", 10, 6)
        mm.disc(e + Z * 0.01, Z, 0.6, "black", 12)
        m.socket(f"funnel.{k + 1}", e + Z * 0.4, (0, 0, 1), yard.half(fx_), "an exhaust pipe over her starboard side")
        yard.soot_at.append((e.x, e.y, e.z + 1.0, 4.0))
    # galleries of light guns under the deck's edge; a 5-inch on her stern, a pair of twins at the bow
    q = 0
    for gx in (L * 0.3, L * 0.08, -L * 0.14, -L * 0.34):
        for hand in (PORT, STBD):
            if hand == STBD and -L * 0.12 < gx < L * 0.0:
                continue
            q += 1
            yg = hand * (w + 1.9)
            mm = yard.on(gx)
            mm.box((gx, yg, fd - 1.0), (6.4, 3.6, 0.22), "upper")
            for dx in (-2.6, 2.6):
                mm.rod((gx + dx, hand * hw, fd - 3.4), (gx + dx, yg + hand * 1.4, fd - 1.1), 0.1, "upper", sides=4)
            fit.bofors_quad(yard, q, gx, yg, fd - 0.9, bearing=hand * 90.0, tub=2.4) if q % 2 == 1 else None
            if q % 2 == 0:
                fit.oerlikon(yard, gx - 1.6, yg, fd - 0.9, facing=hand * 90.0, loader=False)
                fit.oerlikon(yard, gx + 1.6, yg, fd - 0.9, facing=hand * 90.0, loader=True)
    m.use("aft")
    sx = -L * 0.47
    yard.turret("sec.S1", sx, D(sx) + 0.1, 3.0, 1, 4.4, 180.0, 0.12, style="open", rate=12.0, elev=(-5, 80), rest_elev=6.0, limits=(-150, 150), floats=False)
    # a lift fore and aft, the deck park, launch and landing
    lifts = [(L * 0.26, 0.0, 11.0, 10.0, "flight"), (-L * 0.3, 0.0, 10.0, 10.0, "flight")]
    carrier_finish(yard, lifts, x_fore, x_aft, iz, hz - 0.6, [{"model": "DeckPark_CarrierFighter"}], x_fore - 4.0, -L * 0.45, (head.x - 1.0, yc, head.z - 2.0), yard.half(head.x))
    m.parts[m.part_at["deckpark"]].pivot = Vector((-L * 0.04, 0.0, iz))
    m.sockets[:] = [s for s in m.sockets if not (s["name"] == "ensign" and s["note"].startswith("the battle ensign flies from the island's mast") and s["part"] != yard.half(head.x))]
    if fine:
        for dx, dy, fx_, fy_ in ((L * 0.3, 2.6, 1.0, 0.0), (L * 0.26 + 7.0, -3.0, 0.0, 1.0), (L * 0.06, 3.2, 1.0, 0.0), (L * 0.05, -5.0, 1.0, 0.3)):
            yard.on(dx)
            fit.sailor(m, Vector((dx, dy, iz + 0.02)), (fx_, fy_, 0.0), "stand", navy="PLAIN")
    yard.anchor_gear(L * 0.455, L * 0.42, spread=1.8)
    for bx_ in (L * 0.44, -L * 0.46):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.7), D(bx_))
    yard.portholes(-L * 0.42, L * 0.42, 2.2, every=4.2)
    yard.screws_and_rudder(screws=1, r=2.4, rudders=1)
    yard.fire_points += [(L * 0.2, 0.0, iz), (-L * 0.05, 0.0, iz), (-L * 0.28, 0.0, iz), (ix, yc, t1)]
    from ships import clips_for_ship, sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, [], "carrier")
    for i in range(len(lifts)):
        m.clips[f"lift.{i + 1}"] = {"long": 16.0, "loop": False, "about": "a lift goes down to the hangar, waits for an aircraft, and comes up with it",
                                   "tracks": [{"part": f"lift.{i + 1}", "slide": [[0.0, 0.0], [5.0, hz - 0.6], [10.0, hz - 0.6], [15.0, 0.0], [16.0, 0.0]]}],
                                   "events": [{"t": 0.0, "fx": "", "socket": f"lift.{i + 1}", "cue": "lift_motor"}]}
    aa_clip(m)
    _file(m, "escort_carrier", "Escort carrier", ["Carrier"], "research: Carrier aviation (any merchant yard can build her)")
    return m, yard


BUILDERS = {
    "WhatIf_BattleCarrier": lambda fine: battle_carrier(fine),
    "WhatIf_SuperBattleship": lambda fine: super_battleship(fine),
    "WhatIf_RaiderCruiser": lambda fine: raider_cruiser(fine),
    "WhatIf_AACruiser": lambda fine: aa_cruiser(fine),
    "WhatIf_ScoutSubmarine": lambda fine: scout_submarine(fine),
    "WhatIf_EscortCarrier": lambda fine: escort_carrier(fine),
}
