"""The ships a convoy and a landing are made of: a freighter, a tanker, a troopship, and two landing craft.

The fleets fight over these: the convoy battles and the invasions of the campaign need something to escort, to sink
and to put ashore. None is any yard's design: each is our own model to the usual measurements of its kind. They are
built with the shipyard's pieces (shipyard.py) and take the warships' clips (ships.clips_for_ship), so a freighter
lists, burns, breaks her back and goes down like any other ship. Each carries what such ships were given in the war:
a gun on her stern, a smaller one in her bows, light guns in tubs, rafts where the men can reach them.

    Freighter           a cargo steamer: forecastle, bridge island, poop; five hatches; two masts with their derricks
    Tanker              bridge amidships, engines and funnel aft, a low deck of pipes with a catwalk over it
    Troopship           a liner taken up for trooping: a long house of several decks, two funnels, boats along each side
    LandingCraft        an infantry landing craft: a box of a boat with a ramp for a bow, thirty-six men in her well
    TankLandingCraft    a tank landing craft: an open tank deck behind a bow ramp, the wheelhouse aft
"""

import math

from mathutils import Vector

from rigid import PORT, STBD, X, Y, Z, Model, lerp, smooth
from shipyard import Hull, Yard, livery, weather_for

EXTRA = {"tarp": "4B5044", "crate": "8A7A5C", "khaki": "5E6248", "helmet": "454A3A", "pack": "7C765A", "face": "A9866A"}


def dark(hx, k):
    return "".join("%02X" % int(min(255, int(hx[i:i + 2], 16) * k)) for i in (0, 2, 4))


def paints_for(hull, upper=None, deck="5C4E48"):
    """A merchantman's war paint: grey over all, her steel decks the red-brown of deck paint, tarpaulins dark."""
    p = livery("PLAIN", "merchant", hull)
    p.update(EXTRA)
    if upper:
        p["upper"] = upper
        p["roof"], p["door"] = dark(upper, 0.78), dark(upper, 0.8)
    p["deck"] = deck
    return p


def clips():
    import ships                      # (ships.py brings this file in at its foot, so it is fetched when it is wanted)
    return ships


# ---- pieces --------------------------------------------------------------------------------------------

def split(yard, x0, x1):
    """The lengths of a run from x0 to x1 on either side of the break in her back: [(part, fore end, after end)]."""
    fore, rear = max(x0, x1), min(x0, x1)
    bx = yard.break_x
    if rear >= bx:
        return [("fore", fore, rear)]
    if fore <= bx:
        return [("aft", fore, rear)]
    return [("fore", fore, bx), ("aft", bx, rear)]


def house(yard, x0, x1, half, z, high, **kw):
    """A deckhouse that may lie across the break in her back: built in two lengths, so each half of her keeps its own."""
    kw.setdefault("rail", False)
    pieces = split(yard, x0, x1)
    top = z + high
    for part, a, b in pieces:
        two = len(pieces) > 1
        top = yard.house(a, b, half, z, high, part=part, square=(two and part == "aft", two and part == "fore"), **kw)
    return top


def deck_rail(yard, x0, x1, half, z, ends=(True, True), inset=0.15):
    """Guard rails round the edge of a house's roof."""
    if not yard.m.fine:
        return
    fore, rear = max(x0, x1) - inset, min(x0, x1) + inset
    yard.rails(rear, fore, every=2.4, z_of=lambda x: z, y_of=lambda x: half - inset)
    for on, x in ((ends[0], fore), (ends[1], rear)):
        if not on:
            continue
        m = yard.on(x)
        n = max(1, int(2 * (half - inset) / 2.4))
        pts = [Vector((x, -half + inset + 2 * (half - inset) * k / n, z)) for k in range(n + 1)]
        for a, b in zip(pts, pts[1:]):
            m.rod(a, a + Z * 1.05, 0.03, "upper", sides=3, caps=(False, True), smooth=False)
            for hz in (1.05, 0.58):
                m.rod(a + Z * hz, b + Z * hz, 0.018, "rope", sides=3, caps=(False, False), smooth=False)


def bulwark(yard, x0, x1, high=1.05, every=2.0):
    """A solid bulwark along the edge of a well deck on each side: her side plating carried up waist high, with freeing ports at its foot."""
    m, h = yard.m, yard.h
    fore, rear = max(x0, x1), min(x0, x1)
    n = max(1, int((fore - rear) / (every if m.fine else every * 3)))
    xs = [rear + (fore - rear) * k / n for k in range(n + 1)]
    for part, a, b in split(yard, fore, rear):
        sel = sorted(set([x for x in xs if b < x < a] + [a, b]))
        if len(sel) < 2:
            continue
        m.use(part)
        k_ = len(sel)
        faces = [(k, k + 1, k_ + k + 1, k_ + k) for k in range(k_ - 1)]
        for hand in (PORT, STBD):
            lo = [Vector((x, hand * h.half_at(x), h.deck_z(x) - 0.05)) for x in sel]
            hi = [Vector((p.x, p.y, p.z + high + 0.05)) for p in lo]
            lo_in = [Vector((p.x, p.y - hand * 0.07, p.z)) for p in lo]
            hi_in = [Vector((p.x, p.y - hand * 0.07, p.z)) for p in hi]
            m.add(lo + hi, faces, "hull", False, lambda mid, s=hand: Vector((0, s, 0)))
            m.add(lo_in + hi_in, faces, "upper", False, lambda mid, s=hand: Vector((0, -s, 0)))
            m.add(hi + hi_in, faces, "upper", False, lambda mid: Z)
            for end in (0, -1):
                m.add([lo[end], lo_in[end], hi_in[end], hi[end]], [(0, 1, 2, 3)], "upper", False, lambda mid, e=end: X if e else -X)
            if m.fine:
                for k in range(1, k_ - 1, 3):
                    p = lo[k]
                    m.box((p.x, p.y + hand * 0.012, p.z + 0.3), (0.95, 0.03, 0.28), "black")


def step_wall(yard, x):
    """The bulkhead across her where the deck steps up to a forecastle or a poop: doors, scuttles, a ladder each side."""
    m, h = yard.m, yard.h
    a, b = h.deck_z(x + 0.3), h.deck_z(x - 0.3)
    face = 1.0 if a < b else -1.0                     # (the way the wall looks: toward the lower deck)
    lo, hi = min(a, b), max(a, b)
    half = h.half_at(x) - 0.12
    m = yard.on(x - face * 0.3)
    m.box((x + face * 0.03, 0, (lo + hi) / 2), (0.12, 2 * half, hi - lo), "upper")
    if not m.fine:
        return
    for hand in (PORT, STBD):
        m.box((x + face * 0.1, hand * half * 0.42, lo + 0.97), (0.05, 0.75, 1.85), "door")
        for sy in (0.16, 0.68):
            m.disc((x + face * 0.1, hand * half * sy, lo + 1.65), (face, 0, 0), 0.16, "glass", 8)
        y = hand * (half - 0.9)
        foot, head = Vector((x + face * 1.0, y, lo)), Vector((x + face * 0.1, y, hi + 0.05))
        for s in (-0.22, 0.22):
            m.rod(foot + Y * s, head + Y * s, 0.03, "upper", sides=3, smooth=False)
        for k in range(1, 8):
            c = foot + (head - foot) * (k / 8.0)
            m.rod(c - Y * 0.22, c + Y * 0.22, 0.02, "upper", sides=3, caps=(False, False), smooth=False)


def hatch(yard, x, long, wide, z, high=1.0):
    """A cargo hatch: a steel coaming, boards across it, a tarpaulin over all with its edge turned down and wedged."""
    m = yard.on(x)
    m.house(x + long / 2, x - long / 2, wide / 2, z, high, "upper", 0.1, top_paint="tarp", steps=1)
    if not m.fine:
        return z + high
    m.house(x + long / 2 + 0.04, x - long / 2 - 0.04, wide / 2 + 0.04, z + high - 0.34, 0.37, "tarp", 0.1, top_paint="tarp", steps=1)
    m.house(x + long / 2 + 0.08, x - long / 2 - 0.08, wide / 2 + 0.08, z + high - 0.42, 0.08, "steel", 0.1, steps=1)
    n = 6
    rows = [[Vector((x + (long / 2 + 0.04) * (2.0 * i / n - 1.0), (wide / 2 + 0.04) * (j / 2.0 - 1.0), z + high + 0.035 + 0.2 * (1.0 - (j / 2.0 - 1.0) ** 2)))
             for j in range(5)] for i in range(n + 1)]
    m.sheet(rows, "tarp", Z)
    for hand in (PORT, STBD):                       # the stays of the coaming
        k = -long / 2 + 0.7
        while k < long / 2 - 0.4:
            m.box((x + k, hand * (wide / 2 + 0.05), z + high * 0.3), (0.08, 0.14, high * 0.6), "upper")
            k += 1.5
    return z + high + 0.24


def cowl(yard, x, y, z, high=2.2, r=0.45, facing=0.0):
    """A cowl ventilator: a trunk with its bell mouth turned to the wind."""
    m = yard.on(x)
    a = math.radians(facing)
    f = Vector((math.cos(a), math.sin(a), 0.0))
    sides = 10 if m.fine else 6
    m.lathe((x, y, z), Z, [(0.0, r * 0.62), (high - r * 0.9, r * 0.62)], "upper", sides, caps=(False, False))
    el = Vector((x, y, z + high - r * 0.9))
    m.ball(el, r * 0.64, "upper", sides, 4)
    m.lathe(el, f, [(r * 0.1, r * 0.62), (r * 0.9, r * 1.12), (r * 1.0, r * 1.16)], "upper", sides, caps=(False, False))
    m.disc(el + f * r * 0.98, f, r * 1.14, "black", sides)


def winch(yard, x, y, z):
    """A cargo winch: a drum between two frames on a bed, a warping end on each side, its engine beside it."""
    m = yard.on(x)
    m.box((x, y, z + 0.14), (1.5, 1.7, 0.28), "steel")
    m.rod((x, y - 0.55, z + 0.72), (x, y + 0.55, z + 0.72), 0.3, "steel", sides=8 if m.fine else 5)
    if m.fine:
        for s in (-0.7, 0.7):
            m.box((x, y + s, z + 0.6), (0.9, 0.1, 0.95), "upper")
            m.rod((x, y + s * 1.08, z + 0.72), (x, y + s * 1.42, z + 0.72), 0.2, "steel", sides=8)
        m.box((x - 0.75, y, z + 0.5), (0.5, 0.9, 0.55), "upper")


def cargo_mast(yard, x, z, high, fore=None, aft=None, mast_house=True, half=3.0, shrouds=True):
    """A mast between two hatches, its derricks topped up over them: `fore` and `aft` are how far the derricks reach
    that way (None: none). A winch stands by each derrick. Returns the masthead."""
    m = yard.on(x)
    fine = m.fine
    top = z
    if mast_house:
        top = yard.house(x + 2.2, x - 2.2, half, z, 2.4, r=0.3, rail=False)
    head = yard.mast(x, top, high, lean=0.0, yards=((0.58, 0.17),))
    cross = top + high * 0.58
    arm = high * 0.17
    for way, reach in ((1.0, fore), (-1.0, aft)):
        if not reach:
            continue
        for hand in (PORT, STBD):
            m = yard.on(x)
            foot = Vector((x + way * 0.7, hand * 1.35, top + 0.9))
            tip = foot + Vector((way * reach * 0.97, hand * 1.1, reach * 0.16))
            m.rod(foot, tip, 0.17, "upper", rb=0.1, sides=6 if fine else 4)
            m.lathe(foot - Z * 0.9, Z, [(0.0, 0.2), (0.9, 0.16)], "steel", 6)
            yard.wire(Vector((x, hand * arm * 0.85, cross)), tip, 0.022)
            yard.wire(tip, (x + way * 3.1, hand * 1.5, z + 0.8), 0.018)
            winch(yard, x + way * 3.1, hand * 1.5, z)
    if shrouds and fine:
        e = yard.h.half_at(x) - 0.25
        for hand in (PORT, STBD):
            for dx in (-1.7, 0.0, 1.7):
                yard.wire(Vector((x, hand * 0.2, cross - 0.3)), (x + dx, hand * e, yard.h.deck_z(x + dx) + 1.0), 0.02)
    return head


def kingposts(yard, x, z, high, apart, reach=None, way=-1.0):
    """A pair of derrick posts with a tie between their heads, a derrick on each."""
    fine = yard.m.fine
    for hand in (PORT, STBD):
        m = yard.on(x)
        m.lathe((x, hand * apart, z), Z, [(0.0, 0.33), (high * 0.9, 0.26), (high, 0.3), (high + 0.25, 0.12)], "upper", 8 if fine else 5)
        if reach:
            foot = Vector((x + way * 0.5, hand * apart, z + 1.0))
            tip = foot + Vector((way * reach * 0.94, -hand * apart * 0.45, reach * 0.3))
            m.rod(foot, tip, 0.14, "upper", rb=0.09, sides=6 if fine else 4)
            yard.wire(Vector((x, hand * apart, z + high)), tip, 0.02)
    m = yard.on(x)
    m.rod((x, apart, z + high * 0.93), (x, -apart, z + high * 0.93), 0.13, "upper", sides=6 if fine else 4)


def boat_davits(yard, x, y, z, long, hand):
    """A lifeboat in its chocks at the deck's edge under a pair of davits, the falls rove."""
    yard.boat(x, y, z + 0.08, long)
    m = yard.on(x)
    if not m.fine:
        return
    w = long * 0.15
    for t in (-0.34, 0.34):
        bx = x + t * long
        foot = Vector((bx, y + hand * (w + 0.28), z))
        knee = Vector((bx, y + hand * (w + 0.28), z + long * 0.3))
        head = Vector((bx, y, z + long * 0.4))
        m.rod(foot, knee, 0.085, "upper", sides=5)
        m.rod(knee, head, 0.085, "upper", rb=0.06, sides=5)
        m.rod(head, (bx, y, z + long * 0.14), 0.02, "rope", sides=3, caps=(False, False), smooth=False)


def bandstand(yard, x, z, r, high=1.4, y=0.0, wall=0.95):
    """A round gun platform raised on a drum, a splinter wall waist high round it. Returns the height of its floor."""
    m = yard.on(x)
    sides = 16 if m.fine else 8
    if high > 0.05:
        m.lathe((x, y, z - 0.05), Z, [(0.0, r * 0.5), (high + 0.05, r * 0.55)], "upper", sides, caps=(False, False))
        if m.fine:
            for k in range(6):
                a = 2 * math.pi * (k + 0.5) / 6
                m.rod((x + math.cos(a) * r * 0.55, y + math.sin(a) * r * 0.55, z + high * 0.3), (x + math.cos(a) * r * 0.93, y + math.sin(a) * r * 0.93, z + high), 0.07, "upper", sides=4)
    m.lathe((x, y, z + high), Z, [(0.0, r * 0.9), (0.1, r), (0.2, r)], "upper", sides, caps=(True, False))
    m.tub((x, y, z + high + 0.2), r, wall, "upper", sides=sides, floor="deck")
    return z + high + 0.22


def gun(yard, name, x, z, size, long, bore, bearing, elev):
    """A single gun in a shield on its pedestal (the gun a merchantman was given). Returns what clips_for_ship wants."""
    m = yard.on(x)
    m.lathe((x, 0, z), Z, [(0.0, size * 0.2), (0.32, size * 0.16)], "steel", 8)
    tn, gs = yard.turret(name, x, z - 0.15, size, 1, long, bearing, bore, style="shield", rate=12.0, elev=elev, rest_elev=5.0, floats=False, limits=(-150, 150))
    return (tn, gs, x, False)


def rafts(yard, x, y, z, hand, n=1):
    """Life rafts lashed on edge against a house's side."""
    for k in range(n):
        yard.float_(x - k * 3.0, y, z, (0, hand, 0), long=2.5, wide=1.25)


def soldier(m, x, y, z):
    """A man standing in a landing craft, as the camera over a beach sees him: helmet, shoulders, pack, rifle slung.
    (A stand-in: the land battles' own soldiers are the land team's models.)"""
    if not m.fine:
        m.box((x, y, z + 0.6), (0.26, 0.44, 1.2), "khaki")
        m.box((x - 0.2, y, z + 1.1), (0.16, 0.34, 0.42), "pack")
        m.ball((x, y, z + 1.56), (0.16, 0.15, 0.13), "helmet", 6, 4, top_only=True)
        return
    for sy in (-0.1, 0.1):
        m.rod((x, y + sy, z), (x, y + sy, z + 0.84), 0.085, "khaki", rb=0.1, sides=5, caps=(False, False))
    m.ball((x, y, z + 1.14), (0.15, 0.23, 0.36), "khaki", 8, 4)
    for sy in (-0.27, 0.27):
        m.rod((x, y + sy, z + 1.38), (x + 0.1, y + sy * 0.9, z + 0.96), 0.055, "khaki", sides=4, caps=(False, True))
    m.ball((x + 0.01, y, z + 1.58), (0.1, 0.095, 0.12), "face", 6, 4)
    m.lathe((x, y, z + 1.6), Z, [(0.0, 0.17), (0.02, 0.165), (0.09, 0.13), (0.14, 0.05)], "helmet", 8)
    m.box((x - 0.2, y, z + 1.16), (0.16, 0.32, 0.4), "pack")
    m.rod((x + 0.05, y + 0.22, z + 0.95), (x + 0.1, y + 0.27, z + 1.82), 0.02, "steel", sides=3)


def tank_shape(m, x, y, z, way=1.0, paint="khaki"):
    """A medium tank as cargo: enough of a shape to read as one. The game stands its own tank models here."""
    m.box((x, y, z + 0.78), (5.5, 1.7, 0.85), paint)
    for hand in (PORT, STBD):
        m.box((x, y + hand * 1.1, z + 0.5), (5.8, 0.55, 1.0), "steel")
        m.box((x, y + hand * 1.1, z + 1.03), (5.9, 0.64, 0.06), paint)
    m.box((x + 0.2 * way, y, z + 1.3), (3.8, 2.5, 0.36), paint)
    m.lathe((x + 0.1 * way, y, z + 1.46), Z, [(0.0, 1.05), (0.55, 0.95), (0.7, 0.6)], paint, 10 if m.fine else 6)
    m.rod((x + 1.0 * way, y, z + 1.8), (x + 3.7 * way, y, z + 1.86), 0.07, "steel", sides=6)


def staffs(yard, ensign_x, jack_x):
    m, h = yard.m, yard.h
    m.use("aft")
    z = yard.deck(ensign_x)
    m.rod((ensign_x, 0, z), (ensign_x - 0.6, 0, z + 5.0), 0.06, "upper", sides=4)
    m.socket("ensign", (ensign_x - 0.6, 0, z + 4.8), (-1, 0, 0), "aft", "her ensign flies from the staff on her stern")
    m.use("fore")
    z = yard.deck(jack_x)
    m.rod((jack_x, 0, z), (jack_x + 0.3, 0, z + 2.8), 0.05, "upper", sides=4)
    m.socket("jack", (jack_x + 0.3, 0, z + 2.8), (1, 0, 0), "fore")


def scuttles(yard, x0, x1, half, z, every=2.6):
    """A row of scuttles along both sides of a house."""
    if not yard.m.fine:
        return
    x = min(x0, x1) + 1.0
    while x < max(x0, x1) - 0.8:
        for hand in (PORT, STBD):
            yard.on(x).disc((x, hand * (half + 0.02), z), (0, hand, 0), 0.17, "glass", 8)
        x += every


def lights(yard, x0, x1, half, z, every=2.3, wide=1.25, high=0.95):
    """A row of big square windows along both sides of a house (a liner's promenade)."""
    if not yard.m.fine:
        return
    x = min(x0, x1) + 1.4
    while x < max(x0, x1) - 1.2:
        for hand in (PORT, STBD):
            yard.on(x).box((x, hand * (half + 0.02), z), (wide, 0.05, high), "glass")
        x += every


# ---- the freighter -------------------------------------------------------------------------------------

def freighter(fine=True, hull_hex=None):
    """A cargo steamer of the three-island kind, 134 m by 17.4: a forecastle, a bridge island amidships with her one
    tall funnel, a poop; two hatches before the bridge, one in it, two abaft; a mast between each pair with its derricks."""
    m = Model("Freighter", "Freighter (our own model)", "ship", paints_for(hull_hex or "575D61", "7B8286"), fine)
    L = 134.0
    h = Hull(L=L, B=17.4, D=3.4, T=7.4, sheer=1.0, sheer_aft=0.7, rake=1.6, counter=5.5, flare=0.2, fine=2.8, full=0.62, bilge=6.0,
             step_at=49.0, step=2.5, step_aft_at=-52.0, step_aft=-2.5, wl_fore=0.72)
    yard = Yard(m, h, break_x=7.4)
    yard.soot_at, yard.rust_at, yard.rusty = [], [], 1.8
    yard.hull("deck")
    Z0 = yard.deck(0.0)

    # ---- the bridge island: her sides carried up a deck; the bridge at its fore end, a hatch, the engineers' house with funnel and boats
    ix0, ix1 = 18.0, -14.0
    hw = min(h.half_at(ix0), h.half_at(ix1)) - 0.02
    Zb = house(yard, ix0, ix1, hw, Z0 - 0.3, 2.8, paint="hull", r=0.5, walls=False)
    scuttles(yard, ix0, ix1, hw, Zb - 1.15)
    deck_rail(yard, ix0, ix1, hw, Zb)
    b0, b1 = 17.0, 9.2
    t1 = yard.house(b0, b1, 6.4, Zb, 2.4, r=0.6, rail=False)
    yard.windows(b0, 6.4, Zb + 1.25, 0.7, wide=9.5)
    t1 = yard.house(b0 + 0.5, b1 - 0.4, hw, t1, 0.18, r=0.2, top="planks", rail=True)             # the navigating bridge and its wings
    t2 = yard.house(b0 - 0.9, b1 + 1.3, 3.7, t1, 2.3, r=0.5, top="planks", rail=True)
    yard.windows(b0 - 0.9, 3.7, t1 + 1.1, 0.8, back=3.2)
    m = yard.on(b0)
    m.lathe((b0 - 2.2, 0, t2), Z, [(0.0, 0.22), (1.0, 0.2), (1.15, 0.3), (1.35, 0.22)], "brass", 8)        # the standard compass
    sig = yard.mast(b1 + 2.4, t2, 6.0, lean=0.0, yards=((0.72, 0.32),))
    for hand in (PORT, STBD):
        yard.light_gun(b1 + 1.6, hand * (hw - 1.15), t1, 1, tub=True, facing=hand * 55.0, long=1.5)
        rafts(yard, b0 - 3.0, hand * 6.5, Zb + 1.2, hand)
    kingposts(yard, 8.3, Zb, 9.5, 3.4, reach=7.0, way=-1.0)
    hatch(yard, 3.5, 5.4, 5.6, Zb - 0.02, 0.8)
    c0, c1 = 0.2, -13.4
    Zc = yard.house(c0, c1, 5.6, Zb, 2.4, r=0.6, top="planks", rail=False)
    deck_rail(yard, c0, c1, 5.6, Zc)
    fx = -5.4
    yard.house(fx + 3.0, fx - 3.6, 2.6, Zc, 1.3, r=0.5, rail=False, walls=False)
    yard.funnel(fx, Zc + 1.3, 10.5, 4.4, 3.8, lean=0.04, cap=0.14)
    m = yard.on(fx)
    m.box((fx - 5.6, 0, Zc + 0.45), (2.2, 2.2, 0.9), "upper")                                        # the engine-room skylight
    if fine:
        for sx in (-0.5, 0.5):
            for sy in (-0.55, 0.55):
                m.disc((fx - 5.6 + sx, sy, Zc + 0.92), Z, 0.26, "glass", 8)
    for cx, cy in ((fx + 3.9, 1.7), (fx + 3.9, -1.7), (fx - 4.2, 2.3), (fx - 4.2, -2.3)):
        cowl(yard, cx, cy, Zc, 2.6 if cx > fx else 2.1, 0.5, 0.0)
    for hand in (PORT, STBD):
        for bx_ in (-3.0, -10.0):
            boat_davits(yard, bx_, hand * 4.4, Zc, 6.4, hand)

    # ---- the well decks: hatches under their tarpaulins, a mast between each pair, bulwarks along the sides
    tops = {}
    for hx, long, wide in ((41.0, 9.5, 6.4), (24.3, 10.4, 7.0), (-21.6, 10.4, 7.0), (-41.2, 9.6, 6.4)):
        tops[hx] = hatch(yard, hx, long, wide, yard.deck(hx) - 0.07)
    fore_head = cargo_mast(yard, 33.0, yard.deck(33.0) - 0.06, 17.5, fore=9.4, aft=9.8)
    main_head = cargo_mast(yard, -31.3, yard.deck(-31.3) - 0.06, 17.5, fore=9.8, aft=9.4)
    bulwark(yard, 18.15, 48.9)
    bulwark(yard, -51.9, -14.15)
    for hand in (PORT, STBD):
        rafts(yard, 33.0, hand * 3.05, yard.deck(33.0) + 1.2, hand)
        rafts(yard, -31.3, hand * 3.05, yard.deck(-31.3) + 1.2, hand)
        for vx in (46.5, 19.6, -16.0, -48.0):
            cowl(yard, vx, hand * (h.half_at(vx) - 2.0), yard.deck(vx), 2.0, 0.42, 0.0)
    # what she carries on deck: crated aircraft on the second hatch, two tanks under canvas on the fourth
    m = yard.on(24.3)
    for hand in (PORT, STBD):
        m.box((24.3, hand * 1.75, tops[24.3] + 1.2), (9.0, 3.1, 2.5), "crate")
        if fine:
            for k in (-3.0, 0.0, 3.0):
                m.box((24.3 + k, hand * 1.75, tops[24.3] + 1.2), (0.16, 3.16, 2.56), "upper")
    m = yard.on(-21.6)
    for k in (-2.9, 2.9):
        tank_shape(m, -21.6 + k * 1.0, 0.0, tops[-21.6] - 0.25, way=1.0, paint="tarp")

    # ---- forecastle: anchors, windlass, a gun in its tub
    step_wall(yard, 49.0)
    step_wall(yard, -52.0)
    yard.anchor_gear(L * 0.472, L * 0.44, spread=1.6)
    m = yard.on(60.2)
    zf = yard.deck(60.2)
    m.box((60.2, 0, zf + 0.4), (1.4, 3.6, 0.8), "steel")
    m.rod((60.2, -2.1, zf + 0.75), (60.2, 2.1, zf + 0.75), 0.42, "steel", sides=8 if fine else 5)
    mains = []
    gz = bandstand(yard, 53.6, yard.deck(53.6), 2.3, high=1.2)
    mains.append(gun(yard, "turret.A", 53.6, gz, 2.2, 3.2, 0.076, 0.0, (-5, 80)))

    # ---- poop: a house, the stern gun on its platform, light guns
    p0, p1 = -53.6, -57.6
    ph = min(3.6, h.half_at(p1) - 1.1)
    Zp = yard.house(p0, p1, ph, yard.deck(p0), 2.3, r=0.5, rail=True)
    for hand in (PORT, STBD):
        yard.light_gun((p0 + p1) / 2, hand * (ph - 1.2), Zp, 1, tub=True, facing=hand * 125.0, long=1.5)
    sx = -61.6
    gz = bandstand(yard, sx, yard.deck(sx), 2.6, high=1.3)
    mains.append(gun(yard, "turret.Y", sx, gz, 2.8, 4.4, 0.1, 180.0, (-5, 35)))

    # ---- rails, scuttles, staffs, her rigging, her one screw
    yard.rails(49.3, L * 0.49, every=2.4)
    yard.rails(-L * 0.488, -52.3, every=2.4)
    yard.portholes(50.5, 62.5, 1.3, every=2.6)
    yard.portholes(-63.0, -53.0, 1.3, every=2.6)
    for bx_ in (L * 0.43, 47.0, 20.5, -16.5, -49.5, -L * 0.45):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.7), yard.deck(bx_))
    staffs(yard, -L * 0.492, L * 0.49)
    yard.wire(fore_head, (h.frame_x(1.0) + h.rake * 0.8, 0, yard.deck(L * 0.49) + 0.5), 0.022)
    yard.wire(fore_head, sig, 0.02, sag=0.6)
    yard.wire(sig, main_head, 0.02, sag=1.6)
    yard.wire(main_head, (-L * 0.488, 0, yard.deck(-L * 0.488) + 1.0), 0.022)
    yard.screws_and_rudder(screws=1, blades=4, r=2.6, rudders=1)

    yard.fire_points += [(41.0, 0.0, tops[41.0]), (13.0, 0.0, t2), (fx + 3.0, 0.0, Zc + 1.3), (-21.6, 0.0, tops[-21.6] + 1.5), ((p0 + p1) / 2, 0.0, Zp)]
    S = clips()
    S.sockets_common(yard)
    m.weather = weather_for(yard)
    S.clips_for_ship(m, yard, mains, "merchant")
    return m, yard


# ---- the tanker ----------------------------------------------------------------------------------------

def catwalk(yard, x0, z0, x1, z1, wide=1.2):
    """A fore-and-aft gangway on trestles over a tanker's deck, railed on both sides."""
    m = yard.m
    fore, rear = max(x0, x1), min(x0, x1)

    def zf(x):
        return lerp(z0, z1, (x - x0) / (x1 - x0))
    for part, a, b in split(yard, fore, rear):
        n = max(1, int((a - b) / 3.0))
        xs = [b + (a - b) * k / n for k in range(n + 1)]
        m.use(part)
        for p, q in zip(xs, xs[1:]):
            vs = [Vector((x, sy * wide / 2, zf(x) + dz)) for x in (p, q) for sy in (-1, 1) for dz in (-0.12, 0.0)]
            mid = sum(vs, Vector()) / 8
            m.add(vs, [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)], lambda c, n_: "planks" if n_.z > 0.7 else "upper", False, lambda c: c - mid)
        for k, x in enumerate(xs):
            if k % 2 == 0 and b + 0.5 < x < a - 0.5:
                for sy in (-1, 1):
                    m.rod((x, sy * wide * 0.42, yard.deck(x)), (x, sy * wide * 0.42, zf(x) - 0.1), 0.07, "upper", sides=4)
                if m.fine:
                    m.rod((x, -wide * 0.42, yard.deck(x) + 0.2), (x, wide * 0.42, zf(x) - 0.3), 0.04, "upper", sides=3)
    yard.rails(rear + 0.1, fore - 0.1, every=3.0, high=1.0, z_of=zf, y_of=lambda x: wide / 2 - 0.05)


def pipe_run(yard, x0, x1, y, lift, r=0.16):
    """A cargo pipe along the deck on low chairs."""
    m = yard.m
    fore, rear = max(x0, x1), min(x0, x1)
    for part, a, b in split(yard, fore, rear):
        n = max(1, int((a - b) / (5.0 if m.fine else 15.0)))
        xs = [b + (a - b) * k / n for k in range(n + 1)]
        m.use(part)
        for p, q in zip(xs, xs[1:]):
            m.rod((p, y, yard.deck(p) + lift), (q, y, yard.deck(q) + lift), r, "upper", sides=6 if m.fine else 4, caps=(True, True))
            if m.fine:
                m.box((p + 0.3, y, yard.deck(p) + lift * 0.5), (0.2, r * 3.0, lift), "steel")
                m.rod((p, y, yard.deck(p) + lift), (p + 0.14, y, yard.deck(p) + lift), r * 1.5, "steel", sides=8)      # a flange


def tanker(fine=True, hull_hex=None):
    """A tanker, 152 m by 20.6: low in the water, a short forecastle, her bridge on an island amidships, engines and
    funnel right aft; pipes and tank lids along her deck and a gangway over them from end to end."""
    m = Model("Tanker", "Tanker (our own model)", "ship", paints_for(hull_hex or "50565A", "767D81", deck="5A4640"), fine)
    L = 152.0
    h = Hull(L=L, B=20.6, D=2.6, T=8.8, sheer=1.3, sheer_aft=0.8, rake=2.0, counter=6.0, flare=0.22, fine=2.8, full=0.62, bilge=6.0,
             step_at=60.0, step=2.6, step_aft_at=-37.0, step_aft=-2.6, wl_fore=0.72)
    yard = Yard(m, h, break_x=-2.0)
    yard.soot_at, yard.rust_at, yard.rusty = [], [], 1.8
    yard.hull("deck")
    Z0 = yard.deck(0.0)

    # ---- the bridge island amidships
    ix0, ix1 = 15.0, 2.0
    hw = min(h.half_at(ix0), h.half_at(ix1)) - 0.02
    Zb = yard.house(ix0, ix1, hw, Z0 - 0.3, 2.9, paint="hull", r=0.5, walls=False, rail=False)
    scuttles(yard, ix0, ix1, hw, Zb - 1.2)
    deck_rail(yard, ix0, ix1, hw, Zb)
    t1 = yard.house(14.0, 3.2, 6.6, Zb, 2.4, r=0.6, rail=False)
    yard.windows(14.0, 6.6, Zb + 1.25, 0.7, wide=10.0)
    t1 = yard.house(14.5, 6.0, hw, t1, 0.18, r=0.2, top="planks", rail=True)
    t2 = yard.house(13.2, 5.4, 4.0, t1, 2.3, r=0.5, top="planks", rail=True)
    yard.windows(13.2, 4.0, t1 + 1.1, 0.8, back=3.4)
    m = yard.on(11.0)
    m.lathe((11.4, 0, t2), Z, [(0.0, 0.22), (1.0, 0.2), (1.15, 0.3), (1.35, 0.22)], "brass", 8)
    sig = yard.mast(7.4, t2, 9.0, lean=0.0, yards=((0.7, 0.24),))
    for hand in (PORT, STBD):
        yard.light_gun(7.4, hand * (hw - 1.15), t1, 1, tub=True, facing=hand * 60.0, long=1.5)
        rafts(yard, 9.0, hand * 6.7, Zb + 1.2, hand)
        boat_davits(yard, 4.6, hand * (hw - 1.6), Zb, 5.6, hand)

    # ---- the tank deck: the gangway, the pipes, the tank lids, the manifold and its derricks
    zfc, zpp = h.deck_z(60.4) + 0.02, h.deck_z(-37.4) + 0.02
    catwalk(yard, 60.0, zfc, ix0, Zb)
    catwalk(yard, ix1, Zb, -37.0, zpp)
    for y, r in ((1.55, 0.2), (2.15, 0.16), (-1.6, 0.2), (-2.2, 0.13)):
        pipe_run(yard, 58.0, ix0 + 0.4, y, 0.5, r)
        pipe_run(yard, ix1 - 0.4, -35.5, y, 0.5, r)
    lids = [54.0, 46.0, 38.0, 30.0, 22.0, -12.5, -20.0, -27.5, -34.0]
    for lx in lids:
        for y in (-7.3, -4.4, 4.4, 7.3):
            if abs(y) > h.half_at(lx) - 1.3:
                continue
            m = yard.on(lx)
            z = yard.deck(lx) - 0.05
            m.lathe((lx, y, z), Z, [(0.0, 0.66), (0.6, 0.66), (0.7, 0.55), (0.72, 0.0)], "upper", 10 if fine else 6, caps=(False, False))
            if fine:
                m.rod((lx + 1.0, y, z), (lx + 1.0, y, z + 1.9), 0.06, "upper", sides=4)
                m.ball((lx + 1.0, y, z + 1.95), 0.14, "upper", 6, 4)
                m.rod((lx - 0.3, y - 0.5, z + 0.72), (lx + 0.3, y + 0.5, z + 0.72), 0.03, "steel", sides=3)
    m = yard.on(-6.0)
    for px in (-5.2, -6.2, -7.2):
        zz = yard.deck(px) + 0.95
        m.rod((px, -h.half_at(px) + 0.5, zz), (px, h.half_at(px) - 0.5, zz), 0.19, "upper", sides=6 if fine else 4)
        if fine:
            for y in (-6.0, -3.6, 3.6, 6.0):
                m.rod((px, y, zz), (px, y, zz + 0.55), 0.04, "steel", sides=3)
                m.disc((px, y, zz + 0.56), Z, 0.26, "red", 8)
            for hand in (PORT, STBD):
                m.rod((px, hand * 5.0, yard.deck(px)), (px, hand * 5.0, zz), 0.09, "steel", sides=4)
    kingposts(yard, -9.4, yard.deck(-9.4), 9.0, 6.4, reach=6.5, way=1.0)
    bulwark(yard, ix0 + 0.15, 59.9, high=0.9)
    bulwark(yard, -36.9, ix1 - 0.15, high=0.9)

    # ---- forecastle
    step_wall(yard, 60.0)
    step_wall(yard, -37.0)
    yard.anchor_gear(L * 0.474, L * 0.446, spread=1.7)
    m = yard.on(69.6)
    zf = yard.deck(69.6)
    m.box((69.6, 0, zf + 0.4), (1.4, 3.8, 0.8), "steel")
    m.rod((69.6, -2.2, zf + 0.75), (69.6, 2.2, zf + 0.75), 0.42, "steel", sides=8 if fine else 5)
    fore_head = cargo_mast(yard, 61.4, yard.deck(61.4), 15.5, aft=7.0, mast_house=False)
    mains = []
    gz = bandstand(yard, 65.6, yard.deck(65.6), 2.3, high=1.1)
    mains.append(gun(yard, "turret.A", 65.6, gz, 2.2, 3.2, 0.076, 0.0, (-5, 80)))

    # ---- the poop: the after house, funnel, boats, the stern gun
    q0, q1 = -39.5, -62.0
    qh = min(6.8, h.half_at(q1) - 1.0)
    Zp = h.deck_z(q0 - 1.0) + 0.02
    Z1 = yard.house(q0, q1, qh, Zp - 0.1, 2.6, r=0.8, top="planks", rail=False)
    deck_rail(yard, q0, q1, qh, Z1)
    Z2 = yard.house(q0 - 1.6, q1 + 6.0, qh - 2.6, Z1, 2.3, r=0.6, rail=True)
    yard.windows(q0 - 1.6, qh - 2.6, Z1 + 1.15, 0.7, wide=(qh - 2.6) * 1.7)
    fx = -50.2
    yard.house(fx + 3.4, fx - 3.6, 2.9, Z2, 1.2, r=0.5, rail=False, walls=False)
    yard.funnel(fx, Z2 + 1.2, 9.0, 5.2, 4.2, lean=0.05, cap=0.14)
    main_head = yard.mast(q0 - 2.6, Z2, 14.0, lean=0.0, yards=((0.7, 0.2),))
    for cx, cy in ((fx + 4.4, 1.6), (fx + 4.4, -1.6), (fx - 4.6, 1.8), (fx - 4.6, -1.8)):
        cowl(yard, cx, cy, Z2, 2.4, 0.5, 0.0)
    m = yard.on(fx)
    m.box((q1 + 3.0, 0, Z1 + 0.45), (2.4, 2.4, 0.9), "upper")
    for hand in (PORT, STBD):
        for bx_ in (-44.6, -53.4):
            boat_davits(yard, bx_, hand * (qh - 1.25), Z1, 7.0, hand)
        yard.light_gun(q1 + 1.6, hand * (qh - 1.4), Z1, 1, tub=True, facing=hand * 125.0, long=1.5)
        rafts(yard, q0 - 6.0, hand * (qh - 2.6 + 0.05), Z1 + 1.2, hand, n=2)
    sx = -69.4
    gz = bandstand(yard, sx, yard.deck(sx), 2.7, high=1.3)
    mains.append(gun(yard, "turret.Y", sx, gz, 2.8, 4.4, 0.1, 180.0, (-5, 35)))

    yard.rails(60.3, L * 0.49, every=2.4)
    yard.rails(-L * 0.488, -37.3, every=2.4)
    yard.portholes(61.5, 71.0, 1.3, every=2.6)
    yard.portholes(-70.0, -38.5, 1.3, every=2.6)
    for bx_ in (L * 0.44, 57.0, 18.0, -4.0, -33.0, -L * 0.455):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.7), yard.deck(bx_))
    staffs(yard, -L * 0.492, L * 0.49)
    yard.wire(fore_head, (h.frame_x(1.0) + h.rake * 0.8, 0, yard.deck(L * 0.49) + 0.5), 0.022)
    yard.wire(fore_head, sig, 0.02, sag=1.6)
    yard.wire(sig, main_head, 0.02, sag=1.6)
    yard.screws_and_rudder(screws=1, blades=4, r=2.8, rudders=1)

    yard.fire_points += [(42.0, 0.0, yard.deck(42.0) + 0.6), (9.0, 0.0, t2), (-20.0, 0.0, yard.deck(-20.0) + 0.6), (fx + 3.5, 0.0, Z2 + 1.2), (sx + 5.0, 0.0, yard.deck(sx + 5.0))]
    S = clips()
    S.sockets_common(yard)
    m.weather = weather_for(yard)
    S.clips_for_ship(m, yard, mains, "merchant")
    m.clips["ablaze"] = {"long": 6.0, "loop": False, "after": "hold",
                         "about": "her cargo takes fire: flame the length of her tank deck and burning oil spreading on the water; she burns until she goes (play a list or a sinking over it)",
                         "tracks": [], "events": [{"t": 0.0, "fx": "oil_fire", "socket": "fire.1", "cue": "fire_loop"}, {"t": 0.8, "fx": "oil_fire", "socket": "fire.3"},
                                                  {"t": 1.6, "fx": "oil_fire", "socket": "fire.2"}, {"t": 2.4, "fx": "burning_oil_on_water", "socket": "hit.port.3"},
                                                  {"t": 3.0, "fx": "burning_oil_on_water", "socket": "hit.starboard.3"}]}
    return m, yard


# ---- the troopship -------------------------------------------------------------------------------------

def troopship(fine=True, hull_hex=None):
    """A liner taken up for trooping, 192 m by 24.6, grey over all: a high side with three rows of scuttles, a long
    house three decks high with a promenade, two funnels, boats under davits the length of her boat deck."""
    m = Model("Troopship", "Troopship (our own model)", "ship", paints_for(hull_hex or "6C747A", None, deck="5E564E"), fine)
    L = 192.0
    h = Hull(L=L, B=24.6, D=9.4, T=8.4, sheer=2.4, sheer_aft=0.9, rake=5.5, counter=7.5, flare=0.4, fine=2.3, full=0.62, bilge=5.0, wl_fore=0.6)
    yard = Yard(m, h, break_x=-1.0)
    yard.soot_at, yard.rust_at, yard.rusty = [], [], 1.3
    yard.hull("planks")
    Z0 = yard.deck(0.0)

    # ---- the long house: promenade deck, boat deck, the officers' house with the funnels on it
    a0, a1 = 50.0, -58.0
    hw = min(h.half_at(a0), h.half_at(a1)) - 0.05
    Za = house(yard, a0, a1, hw, Z0 - 0.35, 3.05, r=2.6, top="planks", walls=False)
    lights(yard, a0 - 3.0, a1 + 2.0, hw, Za - 1.35)
    deck_rail(yard, a0 - 1.0, a1, hw, Za, ends=(False, True))
    b0, b1 = 48.0, -52.0
    hb = hw - 3.4
    Zb = house(yard, b0, b1, hb, Za, 2.6, r=2.2, top="planks")
    deck_rail(yard, b0 - 1.0, b1, hb, Zb, ends=(False, True))
    c0, c1 = 39.0, -42.0
    hc = 5.4
    Zc = house(yard, c0, c1, hc, Zb, 2.5, r=1.4, top="deck")
    # the bridge on the fore end of the boat deck
    w0, w1 = 47.0, 41.0
    yard.house(w0 + 0.6, w1 + 1.6, hw - 0.8, Zb, 0.18, r=0.3, top="planks", rail=True)
    t2 = yard.house(w0, w1, 6.2, Zb + 0.18, 2.5, r=1.2, top="planks", rail=True)
    yard.windows(w0, 6.2, Zb + 1.3, 0.85, back=3.6)
    yard.windows(b0, hb, Za + 1.3, 0.8, wide=hb * 1.5)
    m = yard.on(w0)
    m.lathe((w0 - 2.4, 0, t2), Z, [(0.0, 0.22), (1.0, 0.2), (1.15, 0.3), (1.35, 0.22)], "brass", 8)
    sig = yard.mast(w1 + 1.0, t2, 7.0, lean=0.0, yards=((0.7, 0.34),))
    funnels = (14.0, -15.0)
    for fx in funnels:
        house(yard, fx + 5.6, fx - 5.8, 3.9, Zc, 1.4, r=0.8, walls=False)
        yard.funnel(fx, Zc + 1.4, 11.5, 8.6, 6.2, lean=0.08, cap=0.12)
        for cx, cy in ((fx + 7.6, 2.4), (fx + 7.6, -2.4), (fx - 8.0, 2.8), (fx - 8.0, -2.8)):
            cowl(yard, cx, cy, Zc, 2.8, 0.6, 0.0)
    for sx_ in (30.0, -1.0, -32.0):
        m = yard.on(sx_)
        m.box((sx_, 0, Zc + 0.5), (3.6, 2.6, 1.0), "upper")
        if fine:
            for k in (-1.0, 0.0, 1.0):
                for sy in (-0.7, 0.7):
                    m.disc((sx_ + k, sy, Zc + 1.02), Z, 0.28, "glass", 8)
    # boats along each side of the boat deck, rafts against the upper house, light guns
    nb = 9
    for k in range(nb):
        bx_ = lerp(38.0, -47.0, k / (nb - 1.0))
        for hand in (PORT, STBD):
            boat_davits(yard, bx_, hand * (hw - 1.3), Za, 7.6, hand)
    for hand in (PORT, STBD):
        for k in range(8):
            rx = lerp(34.0, -37.0, k / 7.0)
            if abs(rx - yard.break_x) > 1.6:
                yard.float_(rx, hand * (hc + 0.05), Zb + 1.25, (0, hand, 0), long=2.5, wide=1.25)
        for gx in (36.0, -1.0, -39.0):
            yard.light_gun(gx, hand * (hc - 1.25), Zc, 2 if gx == -1.0 else 1, tub=True, facing=hand * 80.0, long=1.6)
        yard.light_gun(b1 + 2.0, hand * (hb - 1.4), Zb, 1, tub=True, facing=hand * 130.0, long=1.5)

    # ---- the fore deck: two hatches, the foremast and its derricks, a gun in the bows
    tops = {}
    for hx, long, wide in ((74.0, 8.0, 6.0), (58.0, 9.0, 7.0), (-65.5, 8.0, 7.0), (-79.0, 7.0, 6.0)):
        tops[hx] = hatch(yard, hx, long, wide, yard.deck(hx) - 0.1, 1.1)
    fore_head = cargo_mast(yard, 66.2, yard.deck(66.2) - 0.08, 22.0, fore=7.6, aft=8.0, half=2.8)
    main_head = cargo_mast(yard, -72.4, yard.deck(-72.4) - 0.08, 20.0, fore=7.0, aft=6.4, half=2.8)
    yard.anchor_gear(L * 0.462, L * 0.428, spread=2.0, breakwater=(79.5, h.half_at(79.5) * 0.8, 0.9))
    m = yard.on(84.6)
    zf = yard.deck(84.6)
    m.box((84.6, 0, zf + 0.45), (1.6, 4.2, 0.9), "steel")
    m.rod((84.6, -2.4, zf + 0.8), (84.6, 2.4, zf + 0.8), 0.45, "steel", sides=8 if fine else 5)
    mains = []
    gz = bandstand(yard, 79.0 + 2.6, yard.deck(81.6), 2.4, high=1.2)
    mains.append(gun(yard, "turret.A", 81.6, gz, 2.2, 3.2, 0.076, 0.0, (-5, 80)))

    # ---- the after deck: two hatches, the mainmast, the docking bridge, the stern gun
    d0, d1 = -84.0, -87.2
    dh = min(3.6, h.half_at(d1) - 1.3)
    Zd = yard.house(d0, d1, dh, yard.deck(d0) - 0.05, 2.3, r=0.5, rail=True)
    sx = -90.6
    gz = bandstand(yard, sx, yard.deck(sx), 2.7, high=1.3)
    mains.append(gun(yard, "turret.Y", sx, gz, 3.0, 4.8, 0.12, 180.0, (-5, 35)))
    for hand in (PORT, STBD):
        yard.light_gun(-60.2, hand * (h.half_at(-60.2) - 2.2), yard.deck(-60.2), 1, tub=True, facing=hand * 110.0, long=1.5)
        for vx in (52.0, 62.5, -61.5, -76.0):
            cowl(yard, vx, hand * (h.half_at(vx) - 2.2), yard.deck(vx), 2.6, 0.5, 0.0)

    yard.rails(-L * 0.488, L * 0.475)
    for drop in (1.6, 4.0, 6.4):
        yard.portholes(-L * 0.44 + drop, L * 0.43 - drop * 1.2, drop, every=2.7)
    for bx_ in (L * 0.44, 55.0, -61.0, -L * 0.46):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.9), yard.deck(bx_))
    staffs(yard, -L * 0.493, L * 0.49)
    yard.wire(fore_head, (h.frame_x(1.0) + h.rake * 0.85, 0, yard.deck(L * 0.49) + 0.6), 0.024)
    yard.wire(fore_head, sig, 0.02, sag=1.0)
    yard.wire(sig, main_head, 0.02, sag=3.0)
    yard.wire(main_head, (-L * 0.488, 0, yard.deck(-L * 0.488) + 1.0), 0.024)
    yard.screws_and_rudder(screws=2, blades=4, r=2.6, rudders=1)

    yard.fire_points += [(66.0, 0.0, yard.deck(66.0) + 2.4), (44.0, 0.0, t2), (funnels[0] + 6.0, 0.0, Zc + 1.4), (funnels[1] + 6.0, 0.0, Zc + 1.4), (-72.0, 0.0, yard.deck(-72.0) + 2.4)]
    S = clips()
    S.sockets_common(yard)
    m.weather = weather_for(yard)
    S.clips_for_ship(m, yard, mains, "liner")
    return m, yard


# ---- landing craft -------------------------------------------------------------------------------------

class Craft:
    """The box of a landing craft's hull: a flat bottom rising at the bow to the sill of the ramp, straight sides, an
    open well from `well_from` to the bow with a deck abaft it. It is in two halves like any ship."""

    def __init__(self, yard, L, B, F, T, wall, floor, well_from, ramp_w, bow_long, rise=0.5):
        self.yard, self.L, self.B, self.F, self.T = yard, L, B, F, T
        self.wall, self.floor, self.well_from, self.ramp_w, self.bow_long, self.rise = wall, floor, well_from, ramp_w, bow_long, rise
        self.x0, self.x1 = -L / 2.0, L / 2.0
        self.xb = self.x1 - bow_long
        self.build()

    def hb(self, x):
        """Half her width."""
        return lerp(self.B / 2, self.ramp_w / 2 + self.wall, smooth((x - self.xb) / self.bow_long)) if x > self.xb else self.B / 2

    def hi(self, x):
        """Half the width of her well."""
        return self.hb(x) - self.wall

    def top(self, x):
        """The height of her side: it rises toward the bow."""
        return self.F + self.rise * smooth((x - self.L * 0.1) / (self.L * 0.4))

    def bottom(self, x):
        t = self.T * (1.0 - 0.4 * smooth((self.x0 + self.L * 0.22 - x) / (self.L * 0.22)))      # (the bottom rises a little to the stern, over the screws)
        z = -t
        if x > self.xb:
            z = lerp(z, self.floor - 0.3, smooth((x - self.xb) / self.bow_long))                 # (and comes up to the sill of the ramp)
        return z

    def outer(self, x):
        b, zb, f = self.hb(x), self.bottom(x), self.top(x)
        chine = zb + 0.1 * (f - zb)
        port = [Vector((x, b * 0.82, zb)), Vector((x, b, chine)), Vector((x, b, max(-0.18, chine + 0.03))), Vector((x, b, f))]
        return [Vector((p.x, -p.y, p.z)) for p in reversed(port)] + [Vector((x, 0.0, zb))] + port

    def inner(self, x):
        b, f, fl = self.hi(x), self.top(x), self.floor
        port = [Vector((x, b * 0.8, fl)), Vector((x, b, fl)), Vector((x, b, (f + fl) / 2)), Vector((x, b, f))]
        return [Vector((p.x, -p.y, p.z)) for p in reversed(port)] + [Vector((x, 0.0, fl))] + port

    def build(self):
        yard = self.yard
        m, bx = yard.m, yard.break_x
        n = 18 if m.fine else 8
        xs = sorted(set([round(self.x0 + self.L * k / n, 4) for k in range(n + 1)] + [self.xb, bx, self.well_from]))

        def paint(mid, n_):
            return "antifoul" if mid.z < -0.12 else "hull"

        def u_cap(x, way, pnt, part):
            O, I = self.outer(x), self.inner(x)
            k_ = len(O)
            m.use(part)
            m.add(O + I, [(k, k + 1, k_ + k + 1, k_ + k) for k in range(k_ - 1)], pnt, False, lambda mid: way)

        def full_cap(x, way, pnt, part):
            O = self.outer(x)
            k_ = len(O)
            mid = Vector((x, 0, (self.top(x) + self.bottom(x)) / 2))
            m.use(part)
            m.add(O + [mid], [(k, (k + 1) % k_, k_) for k in range(k_)], pnt, False, lambda c: way)
        for part, sel in (("aft", [x for x in xs if x <= bx]), ("fore", [x for x in xs if x >= bx])):
            m.use(part)
            m.skin([self.outer(x) for x in sel], paint, caps=(False, False), closed=False, smooth=False)
            well = [x for x in sel if x >= self.well_from]
            if len(well) > 1:
                m.skin([self.inner(x) for x in well], lambda mid, n_: "deck" if n_.z > 0.7 else "upper", caps=(False, False), closed=False, smooth=False, inside=True)
                for hand in (PORT, STBD):
                    m.sheet([[Vector((x, hand * self.hb(x), self.top(x))), Vector((x, hand * self.hi(x), self.top(x)))] for x in well], "upper", Z, smooth=False)
            solid = [x for x in sel if x <= self.well_from]
            if len(solid) > 1:
                m.sheet([[Vector((x, self.hb(x), self.top(x))), Vector((x, 0, self.top(x) + 0.04)), Vector((x, -self.hb(x), self.top(x)))] for x in solid], "deck", Z, smooth=False)
        full_cap(self.x0, -X, paint, "aft")
        if bx >= self.well_from:
            u_cap(bx, X, "torn", "aft")
            u_cap(bx, -X, "torn", "fore")
        else:
            full_cap(bx, X, "torn", "aft")
            full_cap(bx, -X, "torn", "fore")
        u_cap(self.x1, X, "hull", "fore")
        # the bulkhead at the after end of the well
        x, b, f, fl = self.well_from, self.hi(self.well_from), self.top(self.well_from), self.floor
        m.use(yard.half(x + 0.01))
        m.add([Vector((x, -b, fl)), Vector((x, b, fl)), Vector((x, b, f)), Vector((x, -b, f))], [(0, 1, 2, 3)], "upper", False, lambda c: X)
        m.socket("wake.bow", (self.x1, 0, 0), (1, 0, 0), "ship", "her blunt bow at the waterline: she pushes a wide low wave")
        m.socket("wake.stern", (self.x0, 0, 0), (-1, 0, 0), "ship", "her stern at the waterline: the wake starts here")
        m.info.setdefault("file", {}).update({"length": self.L, "beam": self.B, "draught": self.T, "waterline": [round(self.x0, 2), round(self.x1 - self.bow_long * 0.4, 2)]})

    def fender(self, drop=0.14, r=0.07):
        """The rope fender along each side under the gunwale."""
        m, yard = self.yard.m, self.yard
        if not m.fine:
            return
        n = 14
        xs = [self.x0 + 0.1 + (self.L - 0.2) * k / n for k in range(n + 1)]
        for hand in (PORT, STBD):
            for a, b in zip(xs, xs[1:]):
                yard.on((a + b) / 2)
                m.rod((a, hand * (self.hb(a) + r * 0.6), self.top(a) - drop), (b, hand * (self.hb(b) + r * 0.6), self.top(b) - drop), r, "rope", sides=5, caps=(False, False))

    def ramp(self, long, lean=24.0, down=72.0, thick=0.1):
        """The bow ramp, hinged at the sill, and the cheeks of her sides that close the gap beside it."""
        m = self.yard.m
        x, z, wide = self.x1 - 0.02, self.floor, self.ramp_w - 0.06
        name = m.part("ramp", "fore", (x, 0, z), axis=(0, 1, 0), limits=[0.0, down + 8.0], kind="turn",
                      note=f"the bow ramp: plus lowers it; about {int(down)} degrees puts its lip on the beach")
        a = math.radians(lean)
        e1, n_in = Vector((math.sin(a), 0, math.cos(a))), Vector((-math.cos(a), 0, math.sin(a)))
        H = Vector((x, 0, z))
        # the cheeks: her sides carried forward to the rake of the ramp
        f = self.top(self.x1)
        for hand in (PORT, STBD):
            y0, y1 = hand * self.hi(self.x1), hand * self.hb(self.x1)
            m.use("fore")
            vs = [Vector((self.x1, y, zz)) for y in (y0, y1) for zz in (z - 0.3, f)] + [Vector((self.x1 + math.tan(a) * (f - z), y, f)) for y in (y0, y1)]
            mid = sum(vs, Vector()) / 6
            m.add(vs, [(0, 1, 4), (2, 5, 3), (1, 3, 5, 4), (0, 4, 5, 2)], "hull", False, lambda c: c - mid)
        m.use(name)
        m.box(H + e1 * long / 2 - n_in * thick / 2, (long, wide, thick), "hull", pitch=90.0 - lean)
        for hand in (PORT, STBD):
            m.box(H + e1 * long / 2 + n_in * 0.09 + Y * hand * (wide / 2 - 0.04), (long, 0.07, 0.2), "upper", pitch=90.0 - lean)
        if m.fine:
            s = 0.25
            while s < long - 0.1:
                m.box(H + e1 * s + n_in * 0.02, (0.05, wide * 0.9, 0.04), "steel", pitch=90.0 - lean)
                s += 0.3
            for k in (-1, 0, 1):
                m.box(H + e1 * long * 0.5 - n_in * (thick + 0.05) + Y * k * wide * 0.33, (long * 0.94, 0.08, 0.1), "hull", pitch=90.0 - lean)
        m.socket("ramp", H + e1 * long, e1, name, "the lip of the ramp: with the ramp down, the men and the tanks go off here")
        return name

    def screw(self, k, y, r, blades=3):
        m = self.yard.m
        x = self.x0 + r * 2.2
        z = self.bottom(x) - r * 0.9
        name = m.part(f"screw.{k}", "aft", (x, y, z), axis=(1, 0, 0), kind="spin", note="a screw: turns while she is under way")
        m.use("aft")
        m.rod((x + r * 4.0, y, self.bottom(x + r * 4.0) + 0.05), (x + 0.05, y, z), r * 0.14, "antifoul", sides=5)
        m.rod((x + r * 0.5, y, z), (x + r * 0.5, y, self.bottom(x + r * 0.5) + 0.05), r * 0.12, "antifoul", sides=4)
        m.use(name)
        m.lathe((x + 0.05, y, z), -X, [(0.0, r * 0.2), (r * 0.5, r * 0.16), (r * 0.7, 0.01)], "brass", 6)
        for b in range(blades):
            a = 2 * math.pi * b / blades
            along = Vector((0, math.cos(a), math.sin(a)))
            flat = X.cross(along) * math.cos(0.75) + X * math.sin(0.75)
            c = Vector((x, y, z)) + along * r * 0.6
            vs = [c - along * r * 0.42 - flat * r * 0.12, c - along * r * 0.42 + flat * r * 0.12, c + along * r * 0.4 + flat * r * 0.3, c + along * r * 0.4 - flat * r * 0.3]
            m.add(vs, [(0, 1, 2, 3), (3, 2, 1, 0)], "brass", False, None)
        m.socket(f"wash.{k}", (x - 0.4, y, 0.0), (-1, 0, 0), "ship", "over a screw: the churned water rises here")
        return name

    def rudder(self, y=0.0, name="rudder", high=None):
        m = self.yard.m
        x = self.x0 + 0.25
        top = self.bottom(x) + 0.02
        high = high or self.T * 0.8
        rn = m.part(name, "aft", (x, y, top), axis=(0, 0, -1), limits=[-35, 35], kind="turn", note="the rudder: plus is helm to starboard")
        m.use(rn)
        m.box((x - high * 0.25, y, top - high / 2), (high * 0.8, 0.05, high), "antifoul")
        return rn


def craft_clips(m, yard, craft, exhaust=None):
    """The clips every landing craft has over a ship's own: she takes the ground, her ramp goes down and comes up."""
    S = clips()
    S.sockets_common(yard)
    m.weather = weather_for(yard)
    S.clips_for_ship(m, yard, [], "landing")
    if exhaust:
        m.clips["sail"]["events"].append({"t": 0.0, "fx": "exhaust_haze", "socket": exhaust})
    down = m.parts[m.part_at["ramp"]].limits[1] - 8.0
    m.clips["beach"] = {"long": 2.4, "loop": False, "after": "hold",
                        "about": "she takes the ground: her bow rides up a little and she stops (stop `sail` first); then `ramp.down`",
                        "tracks": [{"part": "ship", "rot": S.quat_keys(2.4, lambda t: (0.6 * math.exp(-t * 2.0) * math.sin(t * 7.0), 1.6 * smooth(t / 0.7) + 0.5 * math.exp(-t * 2.5) * math.sin(t * 8.0), 0.0), 0.1),
                                    "move": S.move_keys(2.4, lambda t: (0.0, 0.0, 0.12 * smooth(t / 0.7)), 0.2)}],
                        "events": [{"t": 0.0, "fx": "surf", "socket": "wake.bow", "cue": "craft_grounds"}]}
    m.clips["ramp.down"] = {"long": 1.6, "loop": False, "after": "hold", "about": "the ramp drops: it falls, strikes the beach and bounces once. Hold it down while she unloads",
                            "tracks": [{"part": "ramp", "turn": [[0.0, 0.0], [0.25, down * 0.08], [0.6, down * 0.5], [0.95, down], [1.1, down - 5.0], [1.3, down], [1.6, down]]}],
                            "events": [{"t": 0.95, "fx": "ramp_splash", "socket": "ramp", "cue": "ramp_drop"}]}
    m.clips["ramp.up"] = {"long": 4.0, "loop": False, "after": "hold", "about": "the ramp is wound up again before she backs off the beach",
                          "tracks": [{"part": "ramp", "turn": [[0.0, down], [3.6, 2.0], [4.0, 0.0]]}], "events": [{"t": 0.0, "fx": "", "socket": "ramp", "cue": "ramp_winch"}]}


def landing_craft(fine=True, hull_hex=None):
    """An infantry landing craft, 11 m by 3.3: a shallow box of a boat with a ramp for a bow. Thirty-six men stand in
    her well; her coxswain steers from the port side aft; a machine gun in a tub on each quarter."""
    L, B, F, T = 11.0, 3.3, 1.25, 0.7
    m = Model("LandingCraft", "Landing craft (our own model)", "ship", paints_for(hull_hex or "66727C", None, deck="50575C"), fine)
    h = Hull(L=L, B=B, D=F, T=T, sheer=0.0, rake=0.0, counter=0.0, stern="transom", transom=1.0, flare=0.0, fine=8.0, full=0.3, bilge=8.0, wl_fore=0.96)
    yard = Yard(m, h, break_x=0.6)
    yard.soot_at, yard.rust_at, yard.rusty = [], [], 1.4
    c = Craft(yard, L, B, F, T, wall=0.2, floor=0.22, well_from=-2.5, ramp_w=2.2, bow_long=1.7, rise=0.5)
    c.ramp(1.75, lean=24.0, down=72.0)
    c.fender()
    c.screw(1, 0.0, 0.3)
    c.rudder()
    m.use("aft")
    m.add([Vector((c.x0 + 1.8, 0.03, c.bottom(c.x0 + 1.8))), Vector((c.x0 + 0.5, 0.03, c.bottom(c.x0 + 0.5) - 0.42)), Vector((c.x0 + 0.5, 0.03, c.bottom(c.x0 + 0.5))),
           Vector((c.x0 + 1.8, -0.03, c.bottom(c.x0 + 1.8))), Vector((c.x0 + 0.5, -0.03, c.bottom(c.x0 + 0.5) - 0.42)), Vector((c.x0 + 0.5, -0.03, c.bottom(c.x0 + 0.5)))],
          [(0, 1, 2), (5, 4, 3), (0, 3, 4, 1), (1, 4, 5, 2)], "antifoul", False, None)                   # the skeg
    # the after deck: the engine under its cover, the coxswain at his wheel behind a shield, a gun on each quarter
    za = F + 0.02
    m.box((-4.0, 0.0, za + 0.2), (1.5, 1.3, 0.4), "upper")
    if fine:
        for k in (-0.4, 0.0, 0.4):
            m.box((-4.0 + k, 0.0, za + 0.41), (0.12, 1.0, 0.03), "steel")
    m.rod((c.x0 - 0.02, -0.9, za - 0.35), (c.x0 - 0.3, -0.9, za - 0.35), 0.06, "steel", sides=6)
    m.socket("exhaust", (c.x0 - 0.3, -0.9, za - 0.35), (-1, 0, 0), "aft", "her exhaust: a thin blue haze, a spit of water")
    # her coxswain stands on a step in the after port corner of the well, his wheel before him
    cx = c.well_from + 0.5
    cy = c.hi(cx) - 0.45
    m.box((cx, cy, c.floor + 0.2), (0.8, 0.8, 0.4), "upper")
    m.box((cx + 0.5, cy, c.floor + 0.75), (0.22, 0.5, 1.5), "upper")
    m.disc((cx + 0.37, cy, c.floor + 1.4), (-1, 0, 0.35), 0.23, "steel", 10)
    m.box((cx + 0.75, cy, c.top(cx) + 0.25), (0.05, 0.9, 0.6), "upper")
    soldier(m, cx - 0.02, cy, c.floor + 0.4)
    m.socket("helm", (cx, cy, c.floor + 0.4), (1, 0, 0), "aft", "where her coxswain stands")
    for k, hand in enumerate((PORT, STBD)):
        gx, gy = -4.85, hand * 0.98
        m.tub((gx, gy, za), 0.42, 0.5, "upper", sides=10 if fine else 6, floor="deck")
        m.rod((gx, gy, za), (gx, gy, za + 0.95), 0.045, "steel", sides=4)
        fwd = Vector((0.8, hand * 0.3, 0.52)).normalized()
        at = Vector((gx, gy, za + 1.0))
        m.rod(at - fwd * 0.25, at + fwd * 0.75, 0.03, "steel", sides=4)
        m.box(at - fwd * 0.05, (0.3, 0.12, 0.14), "steel", yaw=math.degrees(math.atan2(fwd.y, fwd.x)), pitch=31.0)
        m.socket(f"aa.{k + 1}", at + fwd * 0.75, fwd, "aft", "a machine gun's muzzle: tracer starts here")
    for sx in (-5.1, -2.9):
        for hand in (PORT, STBD):
            m.box((sx, hand * 1.45, za + 0.08), (0.3, 0.1, 0.16), "steel")
    if fine:
        m.rod((-5.2, 1.3, za + 0.07), (-2.8, 1.3, za + 0.07), 0.025, "crate", sides=4)           # her boat hook
        m.box((-3.1, -0.75, za + 0.1), (0.5, 0.35, 0.2), "steel")                                  # her anchor, lashed down
        m.rod((-3.1, -0.75, za + 0.1), (-2.75, -0.75, za + 0.32), 0.03, "steel", sides=4)
    # the men in her well: in two parts, one for each half of her, so the game can take them out as they land
    names = {"aft": m.part("troops.1", "aft", (-1.0, 0, c.floor), kind="fixed", note="the men in the after half of her well (stand-ins: hide them as they go ashore, or stand the game's own soldiers here)"),
             "fore": m.part("troops.2", "fore", (2.5, 0, c.floor), kind="fixed", note="the men in the fore half of her well")}
    x = -1.25
    row = 0
    while x < c.x1 - 0.85:
        across = max(1, int((2 * c.hi(x) - 0.25) / 0.62))
        m.use(names[yard.half(x)])
        for j in range(across):
            y = (j - (across - 1) / 2.0) * 0.62 + (0.06 if row % 2 else -0.06)
            soldier(m, x + (0.05 if j % 2 else -0.05), y, c.floor + 0.02)
        x += 0.7
        row += 1
    yard.fire_points += [(3.2, 0.0, c.floor + 0.2), (-0.8, 0.0, c.floor + 0.2), (-4.0, 0.0, za + 0.4)]
    craft_clips(m, yard, c, exhaust="exhaust")
    return m, yard


def tank_landing_craft(fine=True, hull_hex=None):
    """A tank landing craft, 36.5 m by 9.8: an open tank deck behind a bow ramp, three tanks on it, a walkway along
    each side; aft, a deckhouse with her wheelhouse on it, two light guns, a kedge anchor to haul her off the beach."""
    L, B, F, T = 36.5, 9.8, 1.9, 1.1
    m = Model("TankLandingCraft", "Tank landing craft (our own model)", "ship", paints_for(hull_hex or "66727C", None, deck="50575C"), fine)
    h = Hull(L=L, B=B, D=F, T=T, sheer=0.0, rake=0.0, counter=0.0, stern="transom", transom=1.0, flare=0.0, fine=8.0, full=0.3, bilge=8.0, wl_fore=0.96)
    yard = Yard(m, h, break_x=2.0)
    yard.soot_at, yard.rust_at, yard.rusty = [], [], 1.5
    c = Craft(yard, L, B, F, T, wall=1.15, floor=0.4, well_from=-8.5, ramp_w=3.9, bow_long=6.0, rise=0.5)
    c.ramp(3.3, lean=30.0, down=68.0, thick=0.16)
    c.fender(drop=0.2, r=0.09)
    for k, y in enumerate((2.6, 0.0, -2.6)):
        c.screw(k + 1, y, 0.42)
    c.rudder(1.3, "rudder.1", 0.9)
    c.rudder(-1.3, "rudder.2", 0.9)
    za = F + 0.03
    # the deckhouse and the wheelhouse on it
    d0, d1 = -9.6, -15.4
    Zd = yard.house(d0, d1, 3.3, za, 2.2, r=0.4, rail=True)
    w0, w1 = -10.3, -13.3
    Zw = yard.house(w0, w1, 1.9, Zd, 2.1, r=0.35, rail=False)
    yard.windows(w0, 1.9, Zd + 1.05, 0.7, back=1.6)
    head = yard.mast(w1 + 0.6, Zw, 5.0, lean=0.0, yards=((0.7, 0.3),))
    yard.funnel(-14.5, Zd, 2.4, 1.1, 0.9, lean=0.0, cap=0.14, pipes=False)
    for hand in (PORT, STBD):
        yard.light_gun(-16.9, hand * 2.7, za, 1, tub=True, facing=hand * 150.0, long=1.5)
        yard.float_(-12.6, hand * 3.36, za + 1.25, (0, hand, 0), long=2.2, wide=1.1)
        for bx_ in (-17.2, -7.0, 4.0, 11.0):
            yard.bollards(bx_, hand * (c.hb(bx_) - 0.5), c.top(bx_) + 0.01)
        for vx in (-4.0, 7.0):
            yard.vent(vx, hand * (c.hb(vx) - 0.55), c.top(vx), 0.28, 0.9)
    yard.rails(c.x0 + 0.3, c.x1 - 6.5, every=2.4, high=1.0, z_of=lambda x: c.top(x), y_of=lambda x: c.hb(x) - 0.1)
    # the kedge anchor on its frame over the stern
    m = yard.on(c.x0)
    for hand in (PORT, STBD):
        m.rod((c.x0 + 0.8, hand * 0.9, za), (c.x0 - 0.5, hand * 0.25, za + 1.5), 0.06, "upper", sides=4)
    m.box((c.x0 - 0.5, 0, za + 0.8), (0.14, 0.14, 1.5), "steel")
    m.box((c.x0 - 0.5, 0, za + 0.12), (0.2, 1.3, 0.16), "steel")
    m.rod((c.x0 + 1.6, -0.5, za + 0.5), (c.x0 + 1.6, 0.5, za + 0.5), 0.3, "steel", sides=8)
    m.socket("kedge", (c.x0 - 0.5, 0, 0.0), (-1, 0, 0), "aft", "her kedge anchor's cable enters the water astern here when she is on the beach")
    # her cargo
    for k, x in enumerate((-4.6, 3.6, 11.0)):
        name = m.part(f"cargo.{k + 1}", yard.half(x), (x, 0, c.floor), kind="fixed", note="a tank on her deck (a stand-in: hide it, or stand the game's own tank model here; it drives off over the ramp)")
        m.use(name)
        tank_shape(m, x, 0.0, c.floor + 0.02, way=1.0)
        m.socket(f"cargo.{k + 1}", (x, 0, c.floor), (1, 0, 0), name, "where a vehicle stands, facing the ramp")
    yard.fire_points += [(8.0, 0.0, c.floor + 0.3), (-3.0, 0.0, c.floor + 0.3), (-12.5, 0.0, Zd)]
    craft_clips(m, yard, c)
    return m, yard


BUILDERS = {
    "Freighter": lambda fine: freighter(fine),
    "Tanker": lambda fine: tanker(fine),
    "Troopship": lambda fine: troopship(fine),
    "LandingCraft": lambda fine: landing_craft(fine),
    "TankLandingCraft": lambda fine: tank_landing_craft(fine),
}
