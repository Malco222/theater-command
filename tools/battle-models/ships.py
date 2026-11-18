"""The warships of the real-time sea battles, each built from numbers to the measurements of its type.

    python ships.py <out folder> [--only Battleship_UK,Destroyer] [--far]

The layouts are the ones the campaign's ships already have (CampaignShips.cpp on the 3D game's branch): the same
navies, the same lengths, the same number of turrets and funnels, so a fleet in a battle is the fleet he sent. What is
new is the detail a close camera asks for, and that things move: turrets train, guns lift and run back, directors
turn, screws and rudder turn, and she can list, capsize, go down by the bow or the stern, or break her back.
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from mathutils import Vector  # noqa: E402

from rigid import PORT, STBD, X, Y, Z, Model, lerp, smooth  # noqa: E402
from shipyard import Hull, Yard, livery, weather_for  # noqa: E402

LETTERS_FORE, LETTERS_AFT = "ABC", "XY"


def floatplane(yard, x, y, z, bearing=0.0, span=11.0):
    """The ship's own seaplane on its catapult: a small machine on one big float and two small ones."""
    m = yard.on(x)
    a = math.radians(bearing)
    f = Vector((math.cos(a), math.sin(a), 0))
    s = Vector((-math.sin(a), math.cos(a), 0))
    o = Vector((x, y, z))
    L = span * 0.82

    def P(fx, sy, uz):
        return o + f * fx + s * sy + Z * uz
    rings = []
    for t, w, hh in ((0.5, 0.1, 0.1), (0.44, 0.5, 0.5), (0.2, 0.55, 0.62), (-0.1, 0.45, 0.55), (-0.5, 0.08, 0.2)):
        rings.append([P(t * L, math.cos(2 * math.pi * k / 8) * w, 2.3 + math.sin(2 * math.pi * k / 8) * hh) for k in range(8)])
    m.skin(rings, "boat", (True, True))
    for hand in (1, -1):      # wings, one above the float struts
        m.add([P(0.28 * L, 0, 2.5), P(0.02 * L, 0, 2.5), P(0.05 * L, hand * span / 2, 2.75), P(0.2 * L, hand * span / 2, 2.75),
               P(0.28 * L, 0, 2.38), P(0.02 * L, 0, 2.38), P(0.05 * L, hand * span / 2, 2.67), P(0.2 * L, hand * span / 2, 2.67)],
              [(0, 1, 2, 3), (7, 6, 5, 4), (0, 3, 7, 4), (1, 5, 6, 2), (3, 2, 6, 7)], "boat", False, None)
        m.add([P(-0.38 * L, 0, 2.5), P(-0.5 * L, 0, 2.5), P(-0.5 * L, hand * span * 0.2, 2.5), P(-0.42 * L, hand * span * 0.2, 2.5)], [(0, 1, 2, 3), (3, 2, 1, 0)], "boat", False, None)
        m.lathe(P(0.3 * L, hand * span * 0.36, 0.95), -f, [(0.0, 0.05), (0.3, 0.26), (L * 0.3, 0.2), (L * 0.36, 0.04)], "boat", 6)
        m.rod(P(0.15 * L, hand * span * 0.36, 1.1), P(0.15 * L, hand * span * 0.36, 2.55), 0.05, "steel", sides=4)
    m.add([P(-0.36 * L, 0.04, 2.5), P(-0.5 * L, 0.04, 2.5), P(-0.5 * L, 0.04, 3.9), P(-0.43 * L, 0.04, 3.9)], [(0, 1, 2, 3), (3, 2, 1, 0)], "boat", False, None)
    m.lathe(P(0.5 * L, 0, 0.75), -f, [(0.0, 0.06), (0.5, 0.5), (L * 0.55, 0.42), (L * 0.75, 0.05)], "boat", 8)
    for t in (0.3, 0.05):
        for hand in (1, -1):
            m.rod(P(t * L, hand * 0.3, 1.1), P(t * L, hand * 0.3, 1.9), 0.05, "steel", sides=4)
    m.lathe(P(0.5 * L, 0, 2.3), f, [(0.0, 0.12), (0.25, 0.02)], "steel", 6)
    m.box(P(0.52 * L, 0, 2.3), (0.06, 2.6, 0.16), "black", yaw=bearing)
    m.ball(P(0.12 * L, 0, 2.85), (1.1, 0.4, 0.32), "glass", 8, 4, top_only=True)


def catapult(yard, x, y, z, long=18.0, bearing=0.0, plane=True):
    m = yard.on(x)
    m.lathe((x, y, z), Z, [(0.0, 1.6), (0.7, 1.6)], "upper", 10)
    m.box((x, y, z + 1.0), (long, 1.5, 0.7), "upper", yaw=bearing)
    if m.fine:
        a = math.radians(bearing)
        for k in range(-4, 5):
            c = Vector((x + math.cos(a) * k * long / 9.5, y + math.sin(a) * k * long / 9.5, z + 1.37))
            m.box(c, (0.1, 1.5, 0.04), "steel", yaw=bearing)
    if plane:
        a = math.radians(bearing)
        floatplane(yard, x + math.cos(a) * long * 0.1, y + math.sin(a) * long * 0.1, z + 1.3, bearing)


class _Scaled:
    """A yard that builds everything nearer a middle line and narrower: a battleship's bridge at a cruiser's size."""

    def __init__(self, yard, x, k):
        self.yard, self.x, self.k = yard, x, k

    def __getattr__(self, name):
        return getattr(self.yard, name)

    def sx(self, v):
        return self.x + (v - self.x) * self.k

    def house(self, x0, x1, half, z, high, paint="upper", r=0.8, y=0.0, **kw):
        return self.yard.house(self.sx(x0), self.sx(x1), half * self.k, z, high, paint, r * self.k, y * self.k, **kw)

    def windows(self, x, half, z, high=0.9, wide=None, y=0.0, back=None, part=None):
        return self.yard.windows(self.sx(x), half * self.k, z, high, wide * self.k if wide else None, y * self.k, back * self.k if back else None, part)

    def director(self, name, x, z, arm, y=0.0, r=None, high=None, bearing=0.0, parent=None):
        return self.yard.director(name, self.sx(x), z, arm * self.k, y * self.k, r * self.k if r else None, high, bearing, parent)

    def mast(self, x, z, high, **kw):
        if kw.get("tripod"):
            kw["tripod"] *= self.k
        return self.yard.mast(self.sx(x), z, high, **kw)

    def searchlight(self, x, y, z, r=0.55, facing=(1, 0, 0)):
        return self.yard.searchlight(self.sx(x), y * self.k, z, r, facing)


def tower(yard, style, x, z, high, half_beam, k=1.0):
    """The bridge and what stands over it. Returns the height of its top and where the foremast's wires start.
    `k` makes it smaller in plan (a cruiser's is two thirds of a battleship's)."""
    m = yard.m
    fine = m.fine
    top = z
    real = yard
    if k != 1.0:
        yard = _Scaled(yard, x, k)

    def R(profile):
        return [(a, b * k) for a, b in profile]

    def X(v):
        return x + (v - x) * k
    if style == "block":            # a big square armoured bridge, a director on its roof, a tripod abaft it
        top = yard.house(x + 7.5, x - 7.0, 6.0, z, high * 0.42, r=1.0)
        yard.windows(x + 7.5, 6.0, z + high * 0.42 - 1.6, 1.0, back=5.0)
        yard.house(x + 9.0, x + 7.0, 6.8, z + high * 0.42 - 2.4, 0.25, r=0.3, top="deck")        # the bridge wings
        top = yard.house(x + 5.5, x - 5.5, 4.6, top, high * 0.2, r=0.8)
        yard.windows(x + 5.5, 4.6, top - 1.5, 0.9, back=3.0)
        yard.house(x + 3.0, x - 3.0, 3.0, top, high * 0.1, r=0.8)
        d = yard.director("director.fore", x + 0.5, top + high * 0.1, 4.6)
        yard.director("director.ha.1", x - 4.0, top, 2.2, y=PORT * 3.4)
        yard.director("director.ha.2", x - 4.0, top, 2.2, y=STBD * 3.4)
        head = yard.mast(x - 8.5, z + high * 0.42, high * 1.05, lean=0.03, tripod=4.0, yards=((0.7, 0.2), (0.86, 0.12)), top=(0.56, 1.7))
        radar(real, "radar.fore", head.x, head.z + 0.3, 2.6, 1.2)
        for hand in (PORT, STBD):
            yard.searchlight(x - 2.0, hand * 5.2, top - high * 0.2 + 0.0, 0.6)
    elif style == "tall":           # a tower mast, narrow and high, a rangefinder with wide arms on its foretop
        yard.house(x + 9.0, x - 8.0, 6.2, z, 3.0, r=1.2)
        z1 = z + 3.0
        m.use(yard.half(X(x + 6.0)))
        m.lathe((X(x + 6.0), 0, z1), Z, R([(0.0, 3.4), (3.6, 3.4), (4.0, 3.0)]), "upper", 16 if fine else 8)   # the conning tower
        yard.windows(x + 9.35, 2.0, z1 + 2.6, 0.5, wide=3.4)
        yard.director("director.conn", x + 6.0, z1 + 4.0, 3.6)
        t = yard.house(x + 2.5, x - 5.0, 4.0, z1, high * 0.32, r=1.0, taper=0.3)
        yard.house(x + 4.2, x - 6.0, 5.6, t, 0.25, r=0.5, top="deck")                                # the admiral's bridge
        yard.windows(x + 2.5, 3.6, t - 1.5, 0.9, back=3.0)
        t = yard.house(x + 1.6, x - 4.2, 3.2, t + 0.25, high * 0.3, r=0.9, taper=0.3)
        yard.house(x + 3.0, x - 5.2, 4.6, t, 0.25, r=0.5, top="deck")                                # the searchlight platform
        for hand in (PORT, STBD):
            yard.searchlight(x - 1.2, hand * 3.9, t + 0.25, 0.75, (0.3, hand, 0.1))
        t = yard.house(x + 1.2, x - 3.6, 2.6, t + 0.25, high * 0.2, r=0.8, taper=0.2)
        t = yard.house(x + 2.6, x - 4.4, 4.4, t, 2.3, r=1.0)                                         # the foretop
        yard.windows(x + 2.6, 4.4, t - 1.6, 0.8, back=4.0)
        yard.director("director.fore", x - 0.8, t, 5.25)
        head = yard.mast(x - 3.2, t, high * 0.42, lean=0.02, yards=((0.45, 0.4), (0.8, 0.22)))
        top = t
    elif style == "pagoda":         # many storeys, each a little smaller, with platforms standing out round it
        tiers = 7
        step = high / (tiers + 0.6)
        for k in range(tiers):
            f = 1.0 - k * 0.07
            yard.house(x + 5.4 * f, x - 5.6 * f + k * 0.35, 4.7 * f, z + k * step, step - 0.35, r=1.0)
            yard.house(x + (6.3 - (k % 2) * 0.9) * f, x - 6.4 * f + k * 0.35, (5.5 - (k % 2) * 0.5) * f, z + (k + 1) * step - 0.35, 0.35, paint="hull", r=0.8, top="deck")
            if k in (2, 4, 5):
                yard.windows(x + 5.4 * f, 4.7 * f, z + k * step + step * 0.42, 0.7, back=3.0)
            if k in (1, 3) and fine:
                for hand in (PORT, STBD):
                    yard.searchlight(x - 2.0, hand * 5.0 * f, z + (k + 1) * step, 0.6, (0.3, hand, 0.0))
        top = z + tiers * step
        yard.director("director.fore", x - 0.4, top, 7.6, r=2.6, high=2.8)
        m.use(yard.half(x))
        head = yard.mast(x - 2.4, top + 2.8, high * 0.22, lean=0.0, yards=((0.6, 0.6),))
        m.box((X(x - 0.4), 0, top + 4.6), (0.3, 3.4 * k, 2.2), "upper")                                      # the aerial frame over the rangefinder
    elif style == "cone":           # a low bridge round the conning tower and a tall fire-control tower abaft it
        yard.house(x + 8.0, x - 6.0, 5.4, z, 5.6, r=1.4)
        yard.windows(x + 8.0, 5.4, z + 3.6, 1.0, back=5.0)
        yard.house(x + 9.6, x + 7.6, 6.6, z + 2.9, 0.25, r=0.3, top="deck")
        m.use(yard.half(X(x + 6.5)))
        m.lathe((X(x + 6.5), 0, z + 5.6), Z, R([(0.0, 2.6), (2.6, 2.6), (3.0, 2.2)]), "upper", 14 if fine else 8)
        yard.windows(x + 9.15, 2.0, z + 7.4, 0.4, wide=2.6)
        m.use(yard.half(x))
        m.lathe((X(x - 0.5), 0, z + 5.6), Z, R([(0.0, 3.0), (high * 0.28, 2.6), (high * 0.3, 3.6), (high * 0.33, 3.6), (high * 0.34, 2.4), (high * 0.62, 2.0),
                                              (high * 0.63, 3.2), (high * 0.66, 3.2), (high * 0.67, 1.9), (high * 0.78, 1.8)]), "upper", 14 if fine else 8)
        yard.director("director.fore", x - 0.5, z + 5.6 + high * 0.78, 4.2, r=2.0, high=2.4)
        radar(real, "radar.fore", X(x - 0.5), z + 5.6 + high * 0.78 + 4.2 * k, 4.6 * k, 1.5, parent="director.fore", spin=False)
        for hand in (PORT, STBD):
            yard.director(f"director.ha.{1 if hand == PORT else 2}", x - 5.0, z + 5.6, 2.3, y=hand * 4.0)
            yard.searchlight(x - 0.5, hand * 3.3, z + 5.6 + high * 0.34, 0.6, (0.2, hand, 0))
        head = yard.mast(x - 3.4, z + 5.6 + high * 0.66, high * 0.5, lean=0.02, yards=((0.5, 0.42),))
        radar(real, "radar.air", head.x, head.z + 0.2, 5.2 * k, 2.2 * k)
        top = z + 5.6 + high * 0.78
    elif style == "drums":          # a round tower of stacked drums, each a director that turns
        yard.house(x + 8.0, x - 6.5, 5.6, z, 3.0, r=1.4)
        yard.windows(x + 8.0, 5.6, z + 1.3, 0.9, back=4.0)
        m.use(yard.half(x))
        m.lathe((x, 0, z + 3.0), Z, R([(0.0, 4.2), (high * 0.2, 4.0), (high * 0.21, 4.9), (high * 0.24, 4.9), (high * 0.25, 3.6), (high * 0.5, 3.3)]), "upper", 18 if fine else 9)
        yard.windows(x + 4.2, 2.6, z + 3.0 + high * 0.12, 0.8, wide=4.6)
        t = z + 3.0 + high * 0.5
        yard.director("director.fore", x, t, 5.4, r=3.3, high=high * 0.13)
        yard.director("director.fore.2", x, t + high * 0.13 + 0.2, 3.8, r=2.6, high=high * 0.1, parent="director.fore")
        yard.director("director.fore.3", x, t + high * 0.23 + 0.4, 2.6, r=1.9, high=high * 0.08, parent="director.fore.2")
        head = yard.mast(x - 2.6, t + high * 0.2, high * 0.5, lean=0.0, yards=((0.6, 0.45),))
        top = t + high * 0.3
    else:                           # plain: a bridge, a director, a pole mast
        top = yard.house(x + 6.0, x - 6.0, 5.6, z, high * 0.4, r=1.0)
        yard.windows(x + 6.0, 5.6, top - 1.6, 1.0, back=4.0)
        top = yard.house(x + 4.4, x - 4.2, 4.2, top, high * 0.28, r=0.8)
        yard.windows(x + 4.4, 4.2, top - 1.5, 0.9, back=3.0)
        yard.director("director.fore", x, top, 3.8)
        head = yard.mast(x - 5.0, z + high * 0.4, high * 1.1, lean=0.04, yards=((0.62, 0.2),), top=(0.5, 1.4))
    return top, head


def radar(yard, name, x, z, wide, high, y=0.0, parent=None, spin=True):
    """A radar aerial: a framed mattress on a short post; the search sets turn all the time."""
    m = yard.m
    rn = m.part(name, parent or yard.half(x), (x, y, z), axis=(0, 0, -1), kind="spin" if spin else "fixed",
                note="a radar aerial: turns steadily, about a turn in six seconds" if spin else "an aerial on its director")
    m.use(rn)
    m.rod((x, y, z), (x, y, z + 0.6), 0.12, "upper", sides=5)
    c = Vector((x, y, z + 0.6 + high / 2))
    # an open frame in the pale grey of her upperworks: bars with the sky between them (the art director's review of
    # 2026-11-18: a solid dark sheet read as a black flag at every masthead)
    b = 0.09 if m.fine else 0.12
    for sy in (-1, 1):
        m.box(c + Vector((0, sy * (wide - b) / 2.0, 0)), (b, b, high), "upper")
    for sz in (-1, 1):
        m.box(c + Vector((0, 0, sz * (high - b) / 2.0)), (b, wide, b), "upper")
    nv, nh = (5, 2) if m.fine else (1, 1)
    for k in range(nv):
        m.box(c + Vector((0, (k + 1) * wide / (nv + 1.0) - wide / 2.0, 0)), (0.06, 0.06, high), "upper")
    for k in range(nh):
        m.box(c + Vector((0, 0, (k + 1) * high / (nh + 1.0) - high / 2.0)), (0.06, wide, 0.06), "upper")
    return rn


def capital(name, title, cls, tag, hull, guns, gun_size, gun_long, bore, funnels, funnel, tower_at, tower_high, tower_style,
            side_turrets, aircraft="stern", mainmast="pole", sec_size=5.0, sec_long=5.6, sec_bore=0.1, fine=True, hull_hex=None,
            stripes=False, screws=4, deck_paint="planks", aa=1.0, belt=True, k=1.0, tubes=0, houses=(2.9, 2.8), aft_high=5.5):
    """A battleship or a cruiser: big guns on the middle line, a bridge tower, funnels, a mainmast. `k` is the size of
    her upperworks in plan beside a battleship's (a cruiser's are about two thirds)."""
    m = Model(name, title, "ship", livery(tag, cls, hull_hex), fine)
    h = Hull(**hull)
    yard = Yard(m, h, break_x=(funnels[0] + funnels[-1]) * 0.5 - 2.0 if len(funnels) > 1 else funnels[0] - funnel[1] * 0.9)
    yard.soot_at, yard.rust_at = [], []
    yard.hull(deck_paint)
    Z0 = yard.deck(0.0)
    half = h.B * 0.5

    # ---- the big guns, bow to stern
    fore = [g for g in guns if g[0] > 0]
    after = [g for g in guns if g[0] <= 0]
    names = [LETTERS_FORE[i] for i in range(len(fore))] + ([LETTERS_AFT[i] for i in range(len(after))] if len(after) > 1 else ["Y"] * len(after))
    mains = []
    for (gx, raised, barrels), letter in zip(fore + after, names):
        bearing = 0.0 if gx > 0 else 180.0
        tn, gs = yard.turret(f"turret.{letter}", gx, yard.deck(gx), gun_size, barrels, gun_long, bearing, bore, raised=gun_size * 0.34 if raised else 0.0,
                             rate=5.0 if cls == "battleship" else 7.0, elev=(-3, 40))
        mains.append((tn, gs, gx, raised))
        yard.fire_points.append((gx, 0.0, yard.deck(gx) + (gun_size * 0.34 if raised else 0.0) + gun_size * 0.4))
    front = min(g[0] for g in fore) - gun_size * 0.98
    back = (max(g[0] for g in after) + gun_size * 0.98) if after else -h.L * 0.3

    # ---- the superstructure: two decks of it between the turrets
    yard.house(front, back, half * 0.66, Z0, houses[0], r=2.0 * k)
    Z1 = Z0 + houses[0]
    yard.house(front - 3.0 * k, back + 7.0 * k, half * 0.46, Z1, houses[1], r=1.6 * k)
    Z2 = Z1 + houses[1]

    # ---- the bridge tower, funnels, mainmast and after control
    top, forehead = tower(yard, tower_style, tower_at, Z2, tower_high, half, k)
    fh, fl, fw, flean = funnel
    for fx in funnels:
        yard.house(fx + fl * 0.62, fx - fl * 0.62, fw * 0.75, Z2, 1.6, r=1.2 * k)
        yard.funnel(fx, Z2 + 1.6, fh, fl, fw, flean)
        if fine:
            for hand in (PORT, STBD):
                yard.searchlight(fx + fl * 0.2, hand * (fw * 0.5 + 1.4), Z2 + 1.6, 0.6, (0.2, hand, 0))
    aft_x = back + 9.0 * k
    yard.house(aft_x + 5.0 * k, aft_x - 4.0 * k, 4.2 * k, Z2, aft_high, r=1.2 * k)
    yard.windows(aft_x - 4.0 * k, 4.2 * k, Z2 + aft_high - 1.7, 0.8, wide=6.0 * k)
    yard.director("director.aft", aft_x, Z2 + aft_high, 4.0 if cls == "battleship" else 3.0, bearing=180.0)
    mast_x = aft_x + 9.0 * k
    mainhead = yard.mast(mast_x, Z2, tower_high * (0.95 if mainmast == "pole" else 1.0), lean=0.05, tripod=3.2 if mainmast == "tripod" else 0.0,
                         yards=((0.7, 0.16),), top=(0.5, 1.3) if mainmast == "tripod" else None)
    yard.wire(forehead, mainhead, 0.025, sag=1.0)
    yard.wire(forehead, (h.frame_x(1.0) + h.rake * 0.9, 0, yard.deck(h.L * 0.49) + 1.0), 0.025)
    yard.wire(mainhead, (-h.L * 0.49, 0, yard.deck(-h.L * 0.49) + 6.5), 0.025)
    m.socket("ensign", (mainhead.x - 2.0, 0, mainhead.z - tower_high * 0.25), (-1, 0, 0), yard.half(mast_x), "the battle ensign flies from the mainmast's gaff")

    # ---- the smaller turrets along each side
    k = 0
    for i in range(side_turrets):
        sx = lerp(front - 9.0 * k, back + 9.0 * k, i / (side_turrets - 1) if side_turrets > 1 else 0.5)
        ahead = sx > (front + back) * 0.5
        up = (i % 2 == (0 if ahead else 1)) and side_turrets >= 4
        for hand in (PORT, STBD):
            k += 1
            yard.turret(f"sec.{'P' if hand == PORT else 'S'}{i + 1}", sx, Z1 + (2.6 if up else 0.0) - 0.4, sec_size, 2, sec_long, 0.0 if ahead else 180.0, sec_bore,
                        y=hand * half * (0.6 if up else 0.7), style="medium", rate=15.0, elev=(-5, 80), rest_elev=8.0,
                        limits=(-150, 30) if (hand == PORT) == ahead else (-30, 150))
            if up:
                m.use(yard.half(sx))
                m.lathe((sx, hand * half * 0.6, Z1), Z, [(0.0, sec_size * 0.48), (2.3, sec_size * 0.48)], "upper", 12 if fine else 6, caps=(False, True))

    # ---- boats and cranes amidships, the aircraft and its catapult
    mid = (front + back) * 0.5
    for hand in (PORT, STBD):
        yard.boat(mid - 4.0 * k, hand * half * 0.26, Z2 + 0.3, 10.5 if k > 0.8 else 8.5)
        yard.boat(mid - 16.0 * k, hand * half * 0.3, Z2 + 0.3, 8.0 if k > 0.8 else 7.0)
        yard.crane(mid + 3.0 * k, hand * half * 0.4, Z2, 9.5 * (0.5 + 0.5 * k), 8.0 * (0.5 + 0.5 * k), bearing=180.0 - hand * 25.0)
    if aircraft == "stern":
        tail = -h.L * 0.42
        for hand in ((PORT, STBD) if cls == "battleship" else (PORT,)):
            catapult(yard, tail, hand * (h.half_at(tail) * 0.55 if cls == "battleship" else 0.0), yard.deck(tail), 17.0, bearing=180.0 - hand * 12.0, plane=(hand == PORT))
        yard.crane(-h.L * 0.475, 0.0, yard.deck(-h.L * 0.475), 9.0, 7.0, bearing=0.0)
    else:
        cx = (funnels[0] + funnels[-1]) * 0.5 if len(funnels) > 1 else funnels[0] - funnel[1] * 1.3
        catapult(yard, cx, 0.0, Z1 + 0.1, min(h.B * 0.9, 26.0), bearing=90.0)

    # ---- light guns: on the superfiring turrets, along the upper deck, on forecastle and quarterdeck
    for tn, gs, gx, raised in mains:
        if raised and aa > 0.5:
            p = m.parts[m.part_at[tn]]
            m.use(tn)
            zt = p.pivot.z + gun_size * 0.34
            m.tub((gx - gun_size * 0.2 * (1 if gx > 0 else -1), 0, zt), 1.5, 0.9, "upper", sides=12 if fine else 6, floor="roof")
            for s_ in (-0.4, 0.4):
                m.rod((gx - gun_size * 0.2 * (1 if gx > 0 else -1), s_, zt + 1.0), (gx + (1.4 if gx > 0 else -1.4), s_, zt + 2.2), 0.05, "steel", sides=4)
    n_side = int(round(6 * aa))
    for i in range(n_side):
        lx = lerp(front - 5.0, back + 5.0, (i + 0.5) / n_side)
        for hand in (PORT, STBD):
            yard.light_gun(lx, hand * (half * 0.46 + 1.5), Z1 + 0.02, 2, facing=hand * 70.0)
    for lx in ([h.L * 0.36, -h.L * 0.36] if aa > 0.5 else []):
        for hand in (PORT, STBD):
            yard.light_gun(lx, hand * h.half_at(lx) * 0.55, yard.deck(lx), 4, facing=hand * 60.0)

    # ---- forecastle and quarterdeck, rails, scuttles, staffs
    xh = h.L * 0.455
    yard.anchor_gear(xh, h.L * 0.39, breakwater=(max(g[0] for g in fore) + gun_size * 1.25, h.half_at(max(g[0] for g in fore) + gun_size * 1.25) * 0.8, 1.1))
    for bx in (h.L * 0.43, h.L * 0.3, -h.L * 0.3, -h.L * 0.44):
        for hand in (PORT, STBD):
            yard.bollards(bx, hand * (h.half_at(bx) - 0.9), yard.deck(bx))
    if fine:
        for vx in (front - 4.0, mid, back + 4.0):
            for hand in (PORT, STBD):
                yard.vent(vx, hand * half * 0.58, Z1, 0.5, 1.5)
        for hx in (h.L * 0.33, h.L * 0.27, -h.L * 0.33):
            m.use(yard.half(hx))
            m.box((hx, 0.0 if hx > 0 else 3.0, yard.deck(hx) + 0.22), (1.8, 1.8, 0.4), "upper")
    yard.rails(-h.L * 0.485, h.L * 0.47)
    yard.portholes(-h.L * 0.4, h.L * 0.42, 2.0, every=3.4, skip=[(back - 8, front + 8)] if belt else [])
    yard.portholes(-h.L * 0.34, h.L * 0.4, 4.3, every=3.4, skip=[(back - 14, front + 14)] if belt else [])
    m.use("aft")
    sx = -h.L * 0.492
    m.rod((sx, 0, yard.deck(sx)), (sx - 0.8, 0, yard.deck(sx) + 6.5), 0.07, "upper", sides=4)
    m.use("fore")
    jx = h.L * 0.49
    m.rod((jx, 0, yard.deck(jx)), (jx + 0.5, 0, yard.deck(jx) + 3.6), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.5, 0, yard.deck(jx) + 3.6), (1, 0, 0), "fore")
    if stripes:      # the red and white stripes the Italian navy painted on its forecastles for its own airmen
        for i in range(8):
            x0, x1 = h.L * 0.33 + i * 4.2, h.L * 0.33 + (i + 1) * 4.2
            rows = [[Vector((xx, yy * (h.half_at(xx) - 0.4), yard.deck(xx) + 0.035 + 0.2 * (1 - yy * yy) * min(1.0, h.half_at(xx) / 4.0))) for yy in (-1, -0.5, 0, 0.5, 1)] for xx in (x0, (x0 + x1) / 2, x1)]
            m.sheet(rows, "red" if i % 2 == 0 else "white", Z, smooth=False)

    if tubes:        # a cruiser's torpedo tubes, a bank on each side of the upper deck
        tx = (front + back) * 0.5 - 14.0 * k
        for hand, side in ((PORT, "port"), (STBD, "starboard")):
            yard.tubes(f"tubes.{side}", tx, Z0 + 0.1, n=tubes, long=7.0, y=hand * max(half * 0.66 - 1.0, half - 5.2), rest=90.0 * -hand, limits=(-50, 50))
    yard.screws_and_rudder(screws=screws, r=2.4 if cls == "battleship" else 2.0, rudders=1)

    # ---- where she burns, where she is hit
    for fx in funnels:
        yard.fire_points.append((fx + 8.0, 0.0, Z2 + 1.0))
    yard.fire_points.append((tower_at - 4.0, 0.0, Z2 + 3.0))
    yard.fire_points.append((aft_x - 6.0, 0.0, Z2))
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, mains, cls)
    return m, yard


def sockets_common(yard):
    m, h = yard.m, yard.h
    for i, (x, y, z) in enumerate(yard.fire_points):
        m.socket(f"fire.{i + 1}", (x, y, z), (0, 0, 1), yard.half(x), "a place she burns when hit: flame at the foot, black smoke leaning downwind")
    k = 0
    for u in (-0.7, -0.35, 0.0, 0.35, 0.7):
        for hand, side in ((PORT, "port"), (STBD, "starboard")):
            k += 1
            p, n = h.side(h.frame_x(u), 0.0, hand)
            m.socket(f"hit.{side}.{(k + 1) // 2}", p, n, yard.half(p.x), "the waterline: a torpedo's column of water or a shell's splash alongside")


def wave(t, long, amp, phase=0.0, n=1):
    return amp * math.sin(2.0 * math.pi * (n * t / long + phase))


def quat_keys(long, fn, step=0.25):
    """Keys of a rotation given as f(t) -> (roll, pitch, yaw) in degrees: roll plus is starboard side down, pitch plus is
    bow up, yaw plus is to starboard."""
    from mathutils import Quaternion
    keys = []
    n = max(1, int(round(long / step)))
    for i in range(n + 1):
        t = long * i / n
        roll, pitch, yaw = fn(t)
        q = Quaternion((0, 0, -1), math.radians(yaw)) @ Quaternion((0, -1, 0), math.radians(pitch)) @ Quaternion((1, 0, 0), math.radians(roll))
        keys.append([t, q.w, q.x, q.y, q.z])
    return keys


def move_keys(long, fn, step=0.25):
    n = max(1, int(round(long / step)))
    return [[long * i / n, *fn(long * i / n)] for i in range(n + 1)]


def clips_for_ship(m, yard, mains, cls):
    h = yard.h
    big = {"battleship": 1.0, "cruiser": 0.7, "carrier": 0.9, "destroyer": 0.45, "submarine": 0.35, "merchant": 0.6, "liner": 0.8, "landing": 0.25}.get(cls, 0.6)
    small = cls == "landing"
    heavy = cls in ("battleship", "carrier")
    gun_cue = {"battleship": "ship_gun_heavy", "cruiser": "ship_gun_medium"}.get(cls, "ship_guns")      # (the sound director's names)
    screws = [p.name for p in m.parts if p.name.startswith("screw.")]
    radars = [p.name for p in m.parts if p.name.startswith("radar.") and p.kind == "spin"]

    # under way: she rides the swell (a small ship works more than a great one), her screws turn, her aerials turn
    long = 12.0
    roll, pitch, heave = (1.2, 0.5, 0.25) if heavy else {"cruiser": (2.0, 0.8, 0.3), "liner": (2.0, 0.7, 0.3), "merchant": (3.0, 1.0, 0.35), "landing": (5.0, 2.4, 0.22)}.get(cls, (4.0, 1.6, 0.45))
    revs = 14 if cls in ("merchant", "liner") else 24
    m.clips["sail"] = {"long": long, "loop": True, "about": "under way in a moderate sea: plays all the time she is afloat; other clips play over it",
                       "tracks": [{"part": "ship", "rot": quat_keys(long, lambda t: (wave(t, long, roll, 0.0, 1) + wave(t, long, roll * 0.3, 0.3, 2), wave(t, long, pitch, 0.2, 2), 0.0)),
                                   "move": move_keys(long, lambda t: (0.0, 0.0, wave(t, long, heave, 0.45, 2)))}]
                       + [{"part": s, "turn": [[0.0, 0.0], [long, 360.0 * revs]]} for s in screws]
                       + [{"part": r, "turn": [[0.0, 0.0], [long, 360.0 * 2]]} for r in radars],
                       "events": [{"t": 0.0, "fx": "wake", "socket": "wake.stern", "cue": "ship_wash_loop"}, {"t": 0.0, "fx": "bow_wave", "socket": "wake.bow", "cue": "ship_engine_loop"}]
                       + ([{"t": 0.0, "fx": "funnel_smoke", "socket": "funnel.1"}] if any(s["name"] == "funnel.1" for s in m.sockets) else [])}

    # a turret fires: each gun runs back and comes out again; the flash and the brown smoke are the game's own effects
    for tn, guns, gx, raised in mains:
        letter = tn.split(".")[1]
        back = m.parts[m.part_at[guns[0]]].slide_limits[1] * 0.75
        tracks, events = [], []
        for i, g in enumerate(guns):
            d = 0.06 * i                                   # (the guns of a turret do not fire on the same instant)
            tracks.append({"part": g, "slide": [[0.0, 0.0], [d, 0.0], [d + 0.07, back], [d + 0.25, back * 0.9], [d + 1.4, 0.0], [2.2, 0.0]]})
            events.append({"t": round(d, 2), "fx": "muzzle_flash_heavy" if cls == "battleship" else "muzzle_flash", "socket": f"muzzle.{tn}.{i + 1}", "cue": gun_cue if i == 0 else ""})
            events.append({"t": round(d + 0.05, 2), "fx": "gun_smoke", "socket": f"muzzle.{tn}.{i + 1}"})
        m.clips[f"fire.{letter}"] = {"long": 2.2, "loop": False, "about": f"turret {letter} fires a salvo: train and lay it first by turning the turret and its guns", "tracks": tracks, "events": events}
    # the whole ship heels a little from a full broadside
    if mains:
        m.clips["broadside.heel"] = {"long": 4.0, "loop": False, "about": "play with the fire clips when every turret fires to one side: she heels away and comes back (mirror for the other side)",
                                     "tracks": [{"part": "ship", "rot": quat_keys(4.0, lambda t: (-1.4 * big * math.exp(-t * 0.9) * math.sin(min(t, 4.0) * 2.2), 0, 0), 0.1)}], "events": []}
    for p in m.parts:
        if p.name.startswith("sec.") and ".gun." not in p.name:
            guns = [q.name for q in m.parts if q.name.startswith(p.name + ".gun.")]
            m.clips[f"fire.{p.name}"] = {"long": 0.9, "loop": False, "about": "a secondary turret fires",
                                         "tracks": [{"part": g, "slide": [[0.0, 0.0], [0.05, 0.35], [0.6, 0.0], [0.9, 0.0]]} for g in guns],
                                         "events": [{"t": 0.0, "fx": "muzzle_flash", "socket": f"muzzle.{p.name}.1", "cue": "ship_guns"}, {"t": 0.0, "fx": "muzzle_flash", "socket": f"muzzle.{p.name}.2"}]}

    # she is hit: a shudder
    m.clips["hit"] = {"long": 1.6, "loop": False, "about": "a shell or a bomb strikes: she shudders; put the burst at the nearest fire socket (a torpedo: the column of water at a hit socket)",
                      "tracks": [{"part": "ship", "rot": quat_keys(1.6, lambda t: (1.1 / big * 0.5 * math.exp(-t * 2.5) * math.sin(t * 9.0), 0.3 * math.exp(-t * 2.5) * math.sin(t * 7.0), 0), 0.05)}],
                      "events": [{"t": 0.0, "fx": "shell_burst", "socket": "fire.1", "cue": "explosion"}]}

    def settle(t, T, a):
        return a * smooth(t / T)

    # she lists and stays listed: a ship that is burning or out of the fight
    for side, sgn in (("port", -1.0), ("starboard", 1.0)):
        m.clips[f"list.{side}"] = {"long": 8.0, "loop": False, "after": "hold", "about": "she takes a list and settles a little: hold the last pose (a ship out of the fight, burning)",
                                   "tracks": [{"part": "ship", "rot": quat_keys(8.0, lambda t, s=sgn: (s * settle(t, 8.0, 9.0), 0.4 * smooth(t / 8.0), 0)),
                                               "move": move_keys(8.0, lambda t: (0, 0, -settle(t, 8.0, 0.9 * big + 0.3)))}],
                                   "events": [{"t": 0.0, "fx": "fire", "socket": "fire.2", "cue": "fire_loop"}]}

    deep = h.D + (26.0 if heavy else (6.0 if small else 16.0))
    # she goes down by the bow: the stern lifts clear, the screws still turning, and she slides under
    T = 26.0 if heavy else (11.0 if small else 20.0)

    def by_bow(t, s=1.0):
        a = smooth(t / (T * 0.7))
        return (s * (6.0 * smooth(t / (T * 0.3)) + 10.0 * smooth((t - T * 0.5) / (T * 0.5))), -(5.0 * smooth(t / (T * 0.35)) + 37.0 * a * a), 0.0)

    def by_bow_move(t):
        a = smooth(t / T)
        return (h.L * 0.1 * a * a, 0.0, -(1.5 * smooth(t / (T * 0.3)) + deep * 1.35 * a ** 2.2))
    m.clips["sink.bow"] = {"long": T, "loop": False, "after": "gone", "about": "she goes down by the bow: the stern lifts clear of the water and she slides under. Then she is gone: leave an oil slick and wreckage",
                           "tracks": [{"part": "ship", "rot": quat_keys(T, by_bow), "move": move_keys(T, by_bow_move)}] + [{"part": s, "turn": [[0.0, 0.0], [T * 0.6, 360.0 * 20], [T, 360.0 * 24]]} for s in screws],
                           "events": [{"t": 0.0, "fx": "fire", "socket": "fire.1"}, {"t": T * 0.5, "fx": "steam_and_bubbles", "socket": "wake.bow", "cue": "ship_sinking"}, {"t": T * 0.95, "fx": "oil_slick", "socket": "wake.stern"}]}

    def by_stern(t):
        a = smooth(t / (T * 0.75))
        return (-(5.0 * smooth(t / (T * 0.3)) + 8.0 * smooth((t - T * 0.5) / (T * 0.5))), 4.0 * smooth(t / (T * 0.35)) + 30.0 * a * a, 0.0)

    def by_stern_move(t):
        a = smooth(t / T)
        return (-h.L * 0.08 * a * a, 0.0, -(1.5 * smooth(t / (T * 0.3)) + deep * 1.3 * a ** 2.2))
    m.clips["sink.stern"] = {"long": T, "loop": False, "after": "gone", "about": "she settles by the stern, the bows rise, and she goes down stern first",
                             "tracks": [{"part": "ship", "rot": quat_keys(T, by_stern), "move": move_keys(T, by_stern_move)}],
                             "events": [{"t": 0.0, "fx": "fire", "socket": "fire.3"}, {"t": T * 0.5, "fx": "steam_and_bubbles", "socket": "wake.stern", "cue": "ship_sinking"}, {"t": T * 0.95, "fx": "oil_slick", "socket": "wake.bow"}]}

    # she rolls over: on her beam ends, then bottom up, the red of her bottom showing, and down
    Tc = 24.0 if heavy else (9.0 if small else 18.0)
    for side, sgn in (("port", -1.0), ("starboard", 1.0)):
        def over(t, s=sgn):
            return (s * (20.0 * smooth(t / (Tc * 0.3)) + 70.0 * smooth((t - Tc * 0.22) / (Tc * 0.25)) + 82.0 * smooth((t - Tc * 0.45) / (Tc * 0.3))), -6.0 * smooth((t - Tc * 0.5) / (Tc * 0.5)), 0.0)

        def over_move(t):
            return (0.0, 0.0, -(1.0 * smooth(t / (Tc * 0.3)) - 2.0 * smooth((t - Tc * 0.3) / (Tc * 0.3)) + (deep * 0.55 + 3.0) * smooth((t - Tc * 0.62) / (Tc * 0.38)) ** 1.5))
        m.clips[f"capsize.{side}"] = {"long": Tc, "loop": False, "after": "gone", "about": "she rolls over on her beam ends and then bottom up, and goes down",
                                      "tracks": [{"part": "ship", "rot": quat_keys(Tc, over), "move": move_keys(Tc, over_move)}],
                                      "events": [{"t": Tc * 0.3, "fx": "steam_and_bubbles", "socket": "fire.2", "cue": "ship_sinking"}, {"t": Tc * 0.95, "fx": "oil_slick", "socket": "fire.2"}]}

    # her magazine goes up: she breaks in two and the halves go down apart
    Tb = 22.0 if heavy else (9.0 if small else 16.0)

    def fore_rot(t):
        return (4.0 * smooth(t / 6.0), 6.0 * smooth(t / 2.5) + 30.0 * smooth((t - 2.0) / (Tb * 0.7)) ** 1.5, 2.0 * smooth(t / 8.0))

    def aft_rot(t):
        return (-7.0 * smooth(t / 5.0), -(5.0 * smooth(t / 2.5) + 26.0 * smooth((t - 3.0) / (Tb * 0.75)) ** 1.5), -2.0 * smooth(t / 8.0))
    m.clips["break"] = {"long": Tb, "loop": False, "after": "gone", "about": "a magazine explodes: she breaks her back; bow and stern rise as the middle goes down, then both halves sink",
                        "tracks": [{"part": "fore", "rot": quat_keys(Tb, fore_rot), "move": move_keys(Tb, lambda t: (3.0 * smooth(t / 6.0), 0, -deep * 0.9 * smooth((t - 1.0) / (Tb - 1.0)) ** 1.6))},
                                   {"part": "aft", "rot": quat_keys(Tb, aft_rot), "move": move_keys(Tb, lambda t: (-3.0 * smooth(t / 6.0), 0, -deep * 0.95 * smooth((t - 2.0) / (Tb - 2.0)) ** 1.7))},
                                   {"part": "ship", "rot": quat_keys(Tb, lambda t: (2.0 * math.exp(-t * 0.8) * math.sin(t * 5.0), 0, 0), 0.1)}],
                        "events": [{"t": 0.0, "fx": "magazine_explosion", "socket": "fire.2", "cue": "magazine_explosion"}, {"t": 0.3, "fx": "fire", "socket": "fire.1"},
                                   {"t": 0.3, "fx": "fire", "socket": "fire.3"}, {"t": Tb * 0.6, "fx": "steam_and_bubbles", "socket": "fire.2", "cue": "ship_sinking"}, {"t": Tb * 0.95, "fx": "oil_slick", "socket": "fire.2"}]}


# ---- the ships ---------------------------------------------------------------------------------------
# Each is our own model of a layout of the period, from published measurements (length, beam, how many turrets and how
# many guns in each, how many funnels, the shape of the bridge), as the campaign's ships are.

def battleship(tag, fine=True, hull_hex=None):
    c = dict(cls="battleship", tag=tag, fine=fine, hull_hex=hull_hex, gun_size=11.5, gun_long=15.0, bore=0.36, funnel=(13.0, 9.0, 6.0, 0.06),
             tower_high=22.0, tower_style="plain", side_turrets=3, aircraft="stern", mainmast="pole")
    if tag == "GER":      # 251 m by 36: four turrets of two 38 cm guns, one funnel, a tower mast
        c.update(name="Battleship_GER", title="Battleship, German layout (our own model)", hull=dict(L=251.0, B=36.0, D=7.2, T=9.3, sheer=2.6, rake=9.0, counter=5.0, flare=0.55),
                 guns=[(74.0, False, 2), (56.0, True, 2), (-60.0, True, 2), (-78.0, False, 2)], gun_long=17.0, bore=0.38, funnels=[4.0], funnel=(13.5, 11.0, 7.4, 0.04),
                 tower_at=32.0, tower_high=24.0, tower_style="tall", aircraft="amidships", screws=3)
    elif tag == "USA":    # 270 m by 33: three turrets of three 16-inch guns, two funnels, a long fine bow
        c.update(name="Battleship_USA", title="Battleship, American layout (our own model)", hull=dict(L=270.0, B=33.0, D=6.6, T=10.0, sheer=5.0, rake=12.0, counter=4.0, flare=0.7, fine=1.55, full=0.72),
                 guns=[(72.0, False, 3), (52.0, True, 3), (-72.0, False, 3)], gun_long=20.0, bore=0.4, funnels=[10.0, -14.0], funnel=(12.5, 9.0, 5.6, 0.05),
                 tower_at=30.0, tower_high=26.0, tower_style="cone", side_turrets=5, aa=1.4)
    elif tag in ("JAP", "JPN"):   # 263 m by 39: three turrets of three 46 cm guns, one raked funnel, a pagoda of a bridge
        c.update(name="Battleship_JAP", title="Battleship, Japanese layout (our own model)", hull=dict(L=263.0, B=38.9, D=6.2, T=10.4, sheer=5.0, sheer_aft=1.0, rake=10.0, counter=4.0, flare=0.6, fine=1.7),
                 guns=[(64.0, False, 3), (44.0, True, 3), (-66.0, False, 3)], gun_size=12.5, gun_long=20.0, bore=0.46, funnels=[-6.0], funnel=(14.0, 10.5, 7.0, 0.24),
                 tower_at=20.0, tower_high=30.0, tower_style="pagoda", mainmast="tripod", aa=1.3)
    elif tag == "ITA":    # 238 m by 33: three turrets of three 15-inch guns, two funnels
        c.update(name="Battleship_ITA", title="Battleship, Italian layout (our own model)", hull=dict(L=238.0, B=32.8, D=6.8, T=9.6, sheer=3.2, rake=9.0, counter=4.0),
                 guns=[(62.0, False, 3), (44.0, True, 3), (-62.0, True, 3)], bore=0.38, funnels=[8.0, -10.0], funnel=(12.0, 8.6, 5.8, 0.05), tower_at=26.0,
                 tower_style="drums", stripes=True)
    elif tag in ("FRA", "VIC", "FFR"):   # 248 m by 33: both turrets forward, four 380 mm guns in each
        c.update(name="Battleship_FRA", title="Battleship, French layout (our own model)", hull=dict(L=248.0, B=33.0, D=6.8, T=9.6, sheer=3.2, rake=9.0, counter=4.0),
                 guns=[(70.0, False, 4), (48.0, True, 4)], gun_size=12.5, bore=0.38, funnels=[-22.0], funnel=(13.0, 9.5, 6.4, 0.3), tower_at=22.0, tower_high=26.0,
                 tower_style="drums", side_turrets=4)
    elif tag == "PLAIN":  # the plain battleship, no navy's own: 215 m by 30; six guns in three turrets of two; two funnels close together
        c.update(name="Battleship", title="Battleship (our own model)", hull=dict(L=215.0, B=30.0, D=6.8, T=8.8, sheer=2.0, rake=6.0, counter=4.0),
                 guns=[(60.0, False, 2), (44.0, True, 2), (-58.0, False, 2)], gun_size=11.0, bore=0.36, funnels=[8.0, -6.0], funnel=(12.0, 7.6, 5.6, 0.06), tower_at=26.0,
                 tower_high=18.0, side_turrets=2, aa=0.7)
    else:                 # Britain's own: 227 m by 31: four guns forward, two above them, four aft; two funnels; a square bridge
        c.update(name="Battleship_UK", title="Battleship, British layout (our own model)", hull=dict(L=227.0, B=31.4, D=7.4, T=9.0, sheer=1.4, rake=4.0, counter=4.5, flare=0.35),
                 guns=[(64.0, False, 4), (48.0, True, 2), (-64.0, False, 4)], gun_size=11.5, gun_long=15.0, bore=0.36, funnels=[8.0, -16.0], funnel=(12.0, 8.4, 6.0, 0.03),
                 tower_at=28.0, tower_high=20.0, tower_style="block", side_turrets=4, aircraft="amidships", mainmast="tripod")
    return capital(**c)


def cruiser(tag, fine=True, hull_hex=None):
    """A heavy cruiser: eight-inch guns in turrets on the middle line, built like a small battleship and half again as fast."""
    c = dict(cls="cruiser", tag=tag, fine=fine, hull_hex=hull_hex, gun_size=7.2, gun_long=9.6, bore=0.2, funnel=(9.5, 6.4, 4.2, 0.08), tower_high=14.0, tower_style="plain",
             side_turrets=2, aircraft="amidships", mainmast="pole", sec_size=3.4, sec_long=4.2, sec_bore=0.08, k=0.66, aa=0.67, belt=False, houses=(2.6, 2.5), aft_high=4.0)
    if tag == "GER":      # 208 m by 21.7: four turrets of two 20.3 cm guns, one big funnel, a tower mast
        c.update(name="Cruiser_GER", title="Heavy cruiser, German layout (our own model)", hull=dict(L=208.0, B=21.7, D=6.4, T=6.6, sheer=2.6, rake=9.0, counter=4.0, flare=0.6, fine=1.7, full=0.7),
                 guns=[(66.0, False, 2), (54.0, True, 2), (-52.0, True, 2), (-64.0, False, 2)], funnels=[6.0], funnel=(10.5, 9.0, 5.4, 0.05), tower_at=30.0, tower_high=17.0,
                 tower_style="tall", side_turrets=3, screws=3, tubes=3)
    elif tag == "USA":    # 186 m by 20: three turrets of three 8-inch guns, two funnels, aircraft amidships
        c.update(name="Cruiser_USA", title="Heavy cruiser, American layout (our own model)", hull=dict(L=186.0, B=20.1, D=6.6, T=6.4, sheer=3.4, rake=8.0, counter=3.5, flare=0.6, fine=1.7, full=0.7),
                 guns=[(56.0, False, 3), (44.0, True, 3), (-54.0, False, 3)], gun_size=7.8, funnels=[12.0, -8.0], funnel=(9.0, 6.0, 4.0, 0.06), tower_at=28.0, tower_high=15.0,
                 tower_style="cone", side_turrets=4, aa=1.0)
    elif tag in ("JAP", "JPN"):   # 204 m by 20.7: five turrets of two 20 cm guns, three forward; a raked double funnel
        c.update(name="Cruiser_JAP", title="Heavy cruiser, Japanese layout (our own model)", hull=dict(L=204.0, B=20.7, D=5.8, T=6.3, sheer=4.6, sheer_aft=0.6, rake=11.0, counter=4.0, flare=0.7, fine=1.6, full=0.7),
                 guns=[(72.0, False, 2), (61.0, True, 2), (50.0, False, 2), (-50.0, True, 2), (-62.0, False, 2)], funnels=[10.0, -3.0], funnel=(9.5, 8.0, 4.6, 0.3), tower_at=32.0,
                 tower_high=17.0, tower_style="pagoda", side_turrets=3, aircraft="stern", tubes=4, mainmast="tripod")
    elif tag in ("UK", "GBR", "ENG"):   # 192 m by 20.1: four turrets of two 8-inch guns, three tall funnels, a high side
        c.update(name="Cruiser_UK", title="Heavy cruiser, British layout (our own model)", hull=dict(L=192.0, B=20.1, D=8.2, T=6.2, sheer=1.2, rake=5.0, counter=4.0, flare=0.3, fine=1.8, full=0.68),
                 guns=[(60.0, False, 2), (49.0, True, 2), (-52.0, True, 2), (-63.0, False, 2)], funnels=[16.0, 4.0, -8.0], funnel=(11.0, 5.4, 3.8, 0.16), tower_at=33.0, tower_high=13.0,
                 tower_style="block", side_turrets=2, tubes=4)
    elif tag == "ITA":    # 183 m by 20.6: four turrets of two, two funnels well apart
        c.update(name="Cruiser_ITA", title="Heavy cruiser, Italian layout (our own model)", hull=dict(L=183.0, B=20.6, D=6.4, T=6.4, sheer=3.0, rake=8.0, counter=3.5, flare=0.5, fine=1.7, full=0.7),
                 guns=[(56.0, False, 2), (45.0, True, 2), (-48.0, True, 2), (-59.0, False, 2)], funnels=[10.0, -14.0], funnel=(9.5, 6.4, 4.4, 0.08), tower_at=27.0, tower_high=15.0,
                 tower_style="drums", side_turrets=3, stripes=True)
    else:                 # the plain cruiser: 180 m by 18; four turrets of two; two funnels
        c.update(name="Cruiser", title="Cruiser (our own model)", hull=dict(L=180.0, B=18.4, D=6.2, T=6.0, sheer=2.4, rake=6.0, counter=3.5, fine=1.7, full=0.68),
                 guns=[(54.0, False, 2), (43.0, True, 2), (-46.0, True, 2), (-57.0, False, 2)], funnels=[8.0, -8.0], tower_at=25.0, tower_high=12.0, side_turrets=2)
    return capital(**c)


def destroyer(tag, fine=True, hull_hex=None):
    """A destroyer: a long lean hull, guns in shields or small houses, banks of torpedo tubes, depth charges on her stern."""
    # the plain destroyer (and Britain's): 114 m by 11, a raised forecastle, four single guns in shields, two funnels
    c = dict(name="Destroyer", title="Destroyer (our own model)", hull=dict(L=114.0, B=11.0, D=3.4, T=3.5, step_at=14.0, step=2.3, sheer=1.4, rake=4.5, counter=2.2, flare=0.5, fine=1.6, full=0.72),
             guns=[(39.0, False), (31.5, True), (-35.0, True), (-45.0, False)], barrels=1, mount="shield", size=3.0, gun_long=5.2, bore=0.12, bridge_at=22.5, bridge="open",
             funnels=[(7.0, 6.4, 4.6, 3.0), (-5.0, 6.0, 4.0, 2.8)], lean=0.1, tubes=[(-15.0, 4), (-25.5, 4)], house=(-31.0, -39.0), mast=13.0, screws=2)
    if tag == "GER":      # 123 m by 11.8: five single guns, two forward and three aft; the fore funnel the bigger, with a cap
        c.update(name="Destroyer_GER", title="Destroyer, German layout (our own model)", hull=dict(L=123.0, B=11.8, D=3.6, T=3.8, step_at=17.0, step=2.4, sheer=1.8, rake=6.0, counter=2.4, flare=0.6, fine=1.6, full=0.72),
                 guns=[(43.0, False), (35.0, True), (-28.0, True), (-37.5, True), (-49.0, False)], size=3.2, gun_long=5.8, bore=0.127, bridge_at=26.0, bridge="box",
                 funnels=[(9.0, 7.4, 5.8, 3.6), (-8.0, 6.2, 4.4, 3.0)], lean=0.06, tubes=[(0.5, 4), (-17.0, 4)], house=(-24.0, -41.0), mast=14.0)
    elif tag == "USA":    # 114.8 m by 12: a flush deck with a long sheer, five guns in closed houses, two funnels, two banks of five tubes
        c.update(name="Destroyer_USA", title="Destroyer, American layout (our own model)", hull=dict(L=114.8, B=12.0, D=4.0, T=3.8, sheer=3.4, rake=5.0, counter=2.2, flare=0.55, fine=1.6, full=0.74),
                 guns=[(38.0, False), (30.0, True), (-23.5, True), (-32.5, True), (-44.0, False)], mount="medium", size=3.5, gun_long=4.9, bore=0.127, bridge_at=21.0, bridge="box",
                 funnels=[(9.5, 6.4, 4.4, 3.0), (-5.5, 6.0, 4.2, 2.9)], lean=0.04, tubes=[(2.0, 5), (-13.0, 5)], house=(-19.5, -36.5), mast=14.0)
    elif tag in ("JAP", "JPN"):   # 118.5 m by 10.8: three houses of two guns, one forward and two aft; two raked funnels; tubes in shields
        c.update(name="Destroyer_JAP", title="Destroyer, Japanese layout (our own model)", hull=dict(L=118.5, B=10.8, D=3.3, T=3.7, step_at=19.0, step=2.3, sheer=2.4, sheer_aft=0.3, rake=7.0, counter=2.4, flare=0.7, fine=1.55, full=0.72),
                 guns=[(40.0, False), (-36.0, True), (-46.0, False)], barrels=2, mount="medium", size=3.7, gun_long=5.6, bore=0.127, bridge_at=27.0, bridge="round",
                 funnels=[(10.0, 6.6, 5.2, 3.0), (-8.0, 6.0, 4.2, 2.7)], lean=0.28, tubes=[(1.0, 4), (-19.0, 4)], house=(-31.0, -40.0), mast=13.5)
    elif tag != "PLAIN":
        c.update(name=f"Destroyer_{tag}", title=f"Destroyer ({tag} paint; our own model)")
    name, title = c["name"], c["title"]
    m = Model(name, title, "ship", livery(tag, "destroyer", hull_hex), fine)
    h = Hull(**c["hull"])
    funnels = c["funnels"]
    yard = Yard(m, h, break_x=(funnels[0][0] + funnels[-1][0]) * 0.5)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("deck")
    Z0 = yard.deck(0.0)
    half = h.B * 0.5
    S, style = c["size"], c["mount"]

    # ---- the after deckhouse (the guns that fire over the others stand on it), the guns
    hx0, hx1 = c["house"]
    yard.house(hx0, hx1, half * 0.5, Z0, 2.3, r=0.7)
    fore = [g for g in c["guns"] if g[0] > 0]
    after = [g for g in c["guns"] if g[0] <= 0]
    letters = [LETTERS_FORE[i] for i in range(len(fore))] + ["WXY"[3 - len(after) + i] for i in range(len(after))]
    mains = []
    for (gx, raised), letter in zip(fore + after, letters):
        z = yard.deck(gx)
        lift = 2.3 if raised else 0.0
        if raised and gx > 0:          # the gun over the forward one stands on its own round house
            m.use(yard.half(gx))
        tn, gs = yard.turret(f"turret.{letter}", gx, z, S, c["barrels"], c["gun_long"], 0.0 if gx > 0 else 180.0, c["bore"], raised=lift, style=style, rate=15.0,
                             elev=(-8, 40) if style == "shield" else (-10, 80), rest_elev=4.0, floats=False)
        mains.append((tn, gs, gx, raised))
    for gx, raised in fore:
        if raised:
            yard.house(gx + 2.6, gx - 4.5, half * 0.42, yard.deck(gx), 2.25, r=0.7, rail=False)

    # ---- the bridge
    bx = c["bridge_at"]
    bz = yard.deck(bx)
    kind = c["bridge"]
    if kind == "open":        # a wheelhouse with an open bridge over it, the director behind
        t1 = yard.house(bx + 5.0, bx - 5.0, 3.9, bz, 2.3, r=0.8)
        t2 = yard.house(bx + 4.4, bx - 1.6, 3.2, t1, 2.2, r=0.7)
        yard.windows(bx + 4.4, 3.2, t2 - 1.35, 0.8, back=2.4)
        yard.house(bx + 4.6, bx + 3.2, 3.5, t2, 1.1, r=0.3, top="deck", rail=False)           # the windscreen of the open bridge
        yard.director("director.fore", bx - 2.6, t1, 2.0, high=2.6)
        top = t2
    elif kind == "box":       # a closed square bridge two decks high, the director on its roof
        t1 = yard.house(bx + 5.4, bx - 5.4, 4.0, bz, 2.4, r=0.8)
        t2 = yard.house(bx + 4.8, bx - 3.0, 3.4, t1, 2.4, r=0.6)
        yard.windows(bx + 4.8, 3.4, t2 - 1.4, 0.85, back=3.0)
        yard.house(bx + 6.2, bx + 4.6, 4.3, t1 + 1.0, 0.2, r=0.3, top="deck")                     # the bridge wings
        yard.director("director.fore", bx + 0.6, t2, 2.3, high=1.6)
        top = t2
    else:                     # a rounded tower bridge
        t1 = yard.house(bx + 4.0, bx - 4.6, 3.6, bz, 2.3, r=1.6)
        t2 = yard.house(bx + 3.2, bx - 3.0, 2.9, t1, 2.3, r=1.8)
        yard.windows(bx + 3.2, 2.2, t2 - 1.3, 0.8, back=1.6)
        t3 = yard.house(bx + 2.0, bx - 2.4, 2.2, t2, 1.7, r=1.4)
        yard.director("director.fore", bx - 0.2, t3, 2.1, high=1.5)
        top = t3
    head = yard.mast(bx - 4.4, t1, c["mast"], lean=0.07, tripod=1.7, yards=((0.6, 0.32), (0.8, 0.18)))
    radar(yard, "radar.fore", head.x, head.z + 0.2, 1.9, 0.9)
    for hand in (PORT, STBD):
        yard.light_gun(bx - 2.2, hand * 3.0, t1 + 0.02, 1, tub=True, facing=hand * 50.0, long=1.5)
    yard.fire_points.append((bx, 0.0, t1))

    # ---- funnels on their casings, the boats beside the first
    for i, (fx, fh, fl, fw) in enumerate(funnels):
        yard.house(fx + fl * 0.72, fx - fl * 0.72, fw * 0.85, Z0, 1.3, r=0.6, rail=False)
        yard.funnel(fx, Z0 + 1.3, fh, fl, fw, c["lean"], cap=0.16 if (tag == "GER" and i == 0) else 0.1)
        yard.fire_points.append((fx + 3.0, 0.0, Z0 + 1.3))
    fx = funnels[0][0]
    for hand in (PORT, STBD):
        yard.boat(fx - 0.5, hand * (half - 1.0), Z0 + 1.5, 7.2)
        m.use(yard.half(fx))
        for t in (-2.4, 2.4):
            m.rod((fx - 0.5 + t, hand * (half - 0.25), Z0), (fx - 0.5 + t, hand * (half - 0.8), Z0 + 2.9), 0.07, "upper", sides=4)

    # ---- torpedo tubes, the searchlight and the light guns between them
    tubes = []
    for i, (tx, n) in enumerate(c["tubes"]):
        tubes.append(yard.tubes(f"tubes.{i + 1}", tx, Z0 + 0.05, n=n, long=7.4))
        if tag in ("JAP", "JPN"):     # a shield over the tubes
            m.use(tubes[-1])
            m.house(tx + 2.4, tx - 2.2, n * 0.42 + 0.25, Z0 + 0.55, 1.45, "upper", 0.5)
    t_aft = c["tubes"][-1][0]
    px = (c["tubes"][0][0] + t_aft) * 0.5 if abs(c["tubes"][0][0] - t_aft) > 12.5 and not any(abs(f[0] - (c["tubes"][0][0] + t_aft) * 0.5) < 4.5 for f in funnels) else t_aft - 5.6
    if px > hx0 + 1.0 or px < hx0 - 30:
        plat = yard.house(px + 1.7, px - 1.7, 3.0, Z0, 2.2, r=0.5, rail=False)
        yard.searchlight(px, 0.0, plat, 0.5, (0, -1, 0.1))
        for hand in (PORT, STBD):
            yard.light_gun(px, hand * 2.0, plat + 0.02, 2, tub=False, facing=hand * 80.0, long=1.6)
    for hand in (PORT, STBD):
        yard.light_gun(hx0 - 1.6, hand * (half * 0.5 - 0.9), Z0 + 2.32, 2, tub=True, facing=hand * 110.0, long=1.7)

    # ---- the stern: depth charges on their rails and their throwers
    sx = -h.L * 0.5 + 5.2
    m.use("aft")
    for i, hand in enumerate((PORT, STBD)):
        y = hand * min(1.5, h.half_at(sx) * 0.42)
        zd = yard.deck(sx)
        m.box((sx, y, zd + 0.22), (5.2, 1.0, 0.12), "steel")
        for k in range(6):
            m.rod((sx - 2.2 + k * 0.86, y - 0.42, zd + 0.62), (sx - 2.2 + k * 0.86, y + 0.42, zd + 0.62), 0.24, "steel", sides=8 if fine else 5)
        for k in (-2.5, 2.5):
            m.rod((sx + k, y - 0.5, zd), (sx + k, y - 0.5, zd + 0.95), 0.04, "upper", sides=3)
            m.rod((sx + k, y + 0.5, zd), (sx + k, y + 0.5, zd + 0.95), 0.04, "upper", sides=3)
        m.socket(f"depth.{i + 1}", (sx - 3.0, y, zd + 0.5), (-1, 0, 0), "aft", "a depth charge rolls off the stern here")
    for hand in (PORT, STBD):
        tx_ = hx1 - 2.4
        m.use(yard.half(tx_))
        m.rod((tx_, hand * (half * 0.72), yard.deck(tx_)), (tx_, hand * (half * 0.72 + 0.5), yard.deck(tx_) + 0.9), 0.16, "steel", sides=6)
        m.socket(f"depth.thrower.{'port' if hand == PORT else 'starboard'}", (tx_, hand * (half * 0.72 + 0.5), yard.deck(tx_) + 0.9), (0, hand, 0.7), yard.half(tx_), "a thrower lobs a depth charge out abeam")

    # ---- forecastle, rails, scuttles, staffs, screws
    yard.anchor_gear(h.L * 0.455, h.L * 0.405, spread=1.3, breakwater=(fore[0][0] + S * 1.5, h.half_at(fore[0][0] + S * 1.5) * 0.8, 0.7))
    for bx_ in (h.L * 0.42, h.L * 0.2, -h.L * 0.2, -h.L * 0.45):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.5), yard.deck(bx_))
    yard.rails(-h.L * 0.485, h.L * 0.47, every=2.4)
    yard.portholes(-h.L * 0.36, h.L * 0.42, 1.25, every=2.8)
    m.use("aft")
    ex = -h.L * 0.49
    m.rod((ex, 0, yard.deck(ex)), (ex - 0.5, 0, yard.deck(ex) + 4.2), 0.05, "upper", sides=4)
    m.socket("ensign", (ex - 0.5, 0, yard.deck(ex) + 4.0), (-1, 0, 0), "aft", "the ensign flies from the staff on her stern")
    m.use("fore")
    jx = h.L * 0.49
    m.rod((jx, 0, yard.deck(jx)), (jx + 0.3, 0, yard.deck(jx) + 2.4), 0.04, "upper", sides=4)
    m.socket("jack", (jx + 0.3, 0, yard.deck(jx) + 2.4), (1, 0, 0), "fore")
    yard.wire(head, (h.frame_x(1.0) + h.rake * 0.8, 0, yard.deck(h.L * 0.49) + 0.6), 0.02)
    yard.wire(head, (ex - 0.5, 0, yard.deck(ex) + 4.2), 0.02, sag=1.5)
    yard.screws_and_rudder(screws=2, blades=3, r=1.55, rudders=1)
    yard.fire_points.append(((hx0 + hx1) * 0.5, 0.0, Z0 + 2.3))
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, mains, "destroyer")

    # ---- her own clips: a spread of torpedoes to either side, a pattern of depth charges
    for side, sgn in (("port", -1.0), ("starboard", 1.0)):
        tracks, events = [], []
        for i, tn in enumerate(tubes):
            n = c["tubes"][i][1]
            tracks.append({"part": tn, "turn": [[0.0, 0.0], [2.6, 90.0 * sgn], [2.6 + 0.5 * n + 1.0, 90.0 * sgn], [2.6 + 0.5 * n + 3.6, 0.0]]})
            for k in range(n):
                events.append({"t": round(2.8 + i * 0.25 + k * 0.5, 2), "fx": "torpedo_launch", "socket": f"torpedo.{tn}.{k + 1}", "cue": "torpedo_launch" if k == 0 else ""})
        long = max(tr["turn"][-1][0] for tr in tracks)
        m.clips[f"torpedoes.{side}"] = {"long": long, "loop": False, "about": f"she fires her torpedoes to {side}: the tubes train out, each fish leaves with a puff and splashes in, the tubes train back",
                                        "tracks": tracks, "events": events}
    m.clips["depth_charges"] = {"long": 6.0, "loop": False, "about": "a pattern of depth charges: two roll off the stern, the throwers fire one to each side; the sea heaves white astern some seconds after",
                                "tracks": [], "events": [{"t": 0.0, "fx": "depth_charge", "socket": "depth.1", "cue": "depth_charge_roll"}, {"t": 0.6, "fx": "depth_charge", "socket": "depth.thrower.port", "cue": "depth_charge_thrower"},
                                                         {"t": 0.6, "fx": "depth_charge", "socket": "depth.thrower.starboard"}, {"t": 1.6, "fx": "depth_charge", "socket": "depth.2"},
                                                         {"t": 5.0, "fx": "depth_charge_burst", "socket": "wake.stern", "cue": "depth_charge"}]}
    return m, yard


def parked(m, x, y, z, bearing=0.0, span=10.6):
    """An aircraft standing on a flight deck, tail down: enough of a shape to read from a fighting camera."""
    a = math.radians(bearing)
    f, sd = Vector((math.cos(a), math.sin(a), 0)), Vector((-math.sin(a), math.cos(a), 0))
    L = span * 0.82
    o = Vector((x, y, z))
    nose, tail = o + f * (L * 0.45) + Z * 2.05, o - f * (L * 0.5) + Z * 0.75
    ax = tail - nose
    u = ax.normalized()
    m.lathe(nose, ax, [(0.0, 0.2), (0.3, 0.6), (L * 0.3, 0.66), (L * 0.62, 0.4), (ax.length, 0.07)], "plane", 8 if m.fine else 6)
    root = nose + u * (L * 0.3) - Z * 0.4
    m.box(root, (L * 0.25, span, 0.15), "plane", yaw=bearing)
    m.box(tail - u * 0.55 + Z * 0.12, (L * 0.12, span * 0.34, 0.08), "plane", yaw=bearing)
    m.box(tail - u * 0.5 + Z * 0.7, (L * 0.14, 0.08, 1.15), "plane", yaw=bearing)
    m.ball(nose + u * (L * 0.34) + Z * 0.5, (1.0, 0.36, 0.34), "glass", 8, 4, top_only=True)
    m.box(nose - u * 0.12, (0.08, 0.14, 2.8), "black", yaw=bearing)
    for hand in (1, -1):
        w = root + sd * hand * 1.5
        m.rod(w, w + f * 0.25 - Z * 1.25, 0.07, "steel", sides=4)
        m.box(w + f * 0.25 - Z * 1.3, (0.6, 0.16, 0.6), "black", yaw=bearing)


def carrier(tag, fine=True, hull_hex=None):
    """An aircraft carrier: a hull, a hangar the length of her, a flight deck over all, an island on the starboard side."""
    # the plain carrier (and Britain's): 230 m by 28.5; a closed bow, an armoured steel deck, the funnel in the island; two lifts
    c = dict(name="Carrier", title="Aircraft carrier (our own model)", hull=dict(L=230.0, B=28.5, D=7.0, T=8.2, sheer=1.2, rake=6.0, counter=4.5, flare=0.5, fine=1.75, full=0.7),
             hangar=9.6, deck_half=14.4, island=(0.1, 24.0), lifts=[(0.27, 0.0), (-0.24, 0.0)], lift=(13.6, 6.8), bow="closed", funnels="island", plane="5A6468", wood=False, disc=False)
    if tag == "USA":      # 247 m by 28.4; an open hangar under a wooden deck; a big island and funnel; three lifts, one on the deck's edge
        c.update(name="Carrier_USA", title="Aircraft carrier, American layout (our own model)", hull=dict(L=247.0, B=28.4, D=7.4, T=8.4, sheer=1.6, rake=7.0, counter=4.5, flare=0.55, fine=1.7, full=0.7),
                 hangar=9.2, deck_half=16.2, island=(0.08, 31.0), lifts=[(0.26, 0.0), (-0.01, 1.0), (-0.3, 0.0)], lift=(14.6, 13.4), bow="open", plane="36425A", wood=True)
    elif tag in ("JAP", "JPN"):   # 257.5 m by 29; a small island well forward; two funnels turned down over the starboard side; three lifts; the red disc on her deck
        c.update(name="Carrier_JAP", title="Aircraft carrier, Japanese layout (our own model)", hull=dict(L=257.5, B=29.0, D=7.0, T=8.6, sheer=2.0, sheer_aft=0.4, rake=9.0, counter=4.5, flare=0.6, fine=1.65, full=0.7),
                 hangar=9.6, deck_half=14.2, island=(0.2, 13.0), lifts=[(0.3, 0.0), (0.02, 0.0), (-0.3, 0.0)], lift=(13.0, 13.0), bow="open", funnels="side", plane="3E4A3C", wood=True, disc=True)
    elif tag != "PLAIN":
        c.update(name=f"Carrier_{tag}", title=f"Aircraft carrier ({tag} paint; our own model)")
    paints = livery(tag, "carrier", hull_hex)
    paints["plane"] = c["plane"]
    if c["wood"]:
        paints["flight"] = "B79E74"
    m = Model(c["name"], c["title"], "ship", paints, fine)
    h = Hull(**c["hull"])
    L, half = h.L, h.B * 0.5
    bx = -2.0
    yard = Yard(m, h, break_x=bx)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("deck")
    Z0 = yard.deck(0.0)
    hz, w = c["hangar"], c["deck_half"]
    fd = Z0 + hz                 # the underside of the flight deck
    iz = fd + 0.7                # its top
    top_paint = "planks" if c["wood"] else "flight"
    closed = c["bow"] == "closed"

    # ---- the hangar, in two halves at the place she would break
    hx0, hx1 = L * 0.36, -L * 0.41
    hw = half * (0.97 if closed else 0.93)
    yard.house(hx0, bx, hw, Z0, hz, paint="hull", r=1.6, top=None, rail=False, walls=closed)
    yard.house(bx, hx1, hw, Z0, hz, paint="hull", r=1.6, top=None, rail=False, walls=closed)
    if not closed and fine:      # the open hangar's roller doors along each side
        x = hx0 - 9.0
        while x > hx1 + 8.0:
            for hand in (PORT, STBD):
                yard.on(x).box((x, hand * (hw + 0.03), Z0 + hz * 0.45), (9.6, 0.08, hz * 0.62), "door")
            x -= 13.5
    if closed:                   # her plating carried up to the flight deck at the bow
        xs = [L * 0.475 - k * (L * 0.475 - hx0 + 2.0) / 8.0 for k in range(9)]
        plan = [(x, max(h.half_at(x) * 0.93, 0.3)) for x in xs] + [(x, -max(h.half_at(x) * 0.93, 0.3)) for x in reversed(xs)]
        top = [(x + 1.2 * (k < 9 and 1 or 1), y * 1.04) for k, (x, y) in enumerate(plan)]
        m.use("fore")
        m.prism(plan, Z0 - 0.3, fd, "hull", top=top, top_paint="hull")
    else:                        # or the deck stands on pillars over an open forecastle
        m.use("fore")
        for px in (L * 0.445, L * 0.405):
            for hand in (PORT, STBD):
                yy = hand * min(h.half_at(px) * 0.7, 4.2)
                m.rod((px, yy, yard.deck(px)), (px, yy * 1.3, fd), 0.3, "upper", sides=6)
    m.use("aft")
    for px in (-L * 0.455, -L * 0.435):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.6, 5.0)
            m.rod((px, yy, yard.deck(px)), (px, yy * 1.3, fd), 0.3, "upper", sides=6)

    # ---- the flight deck
    x_fore, x_aft = L * 0.478, -L * 0.492
    rf, ra = (5.0, 3.0)

    def corner(cx, cy, r, a0):
        return [(cx + math.cos(a0 + k * math.pi / 8) * r, cy + math.sin(a0 + k * math.pi / 8) * r) for k in range(5)]
    wf = w * (0.86 if tag in ("JAP", "JPN") else 0.95)      # (a little narrower at the fore end)
    fore_plan = [(bx, -w)] + [(L * 0.25, -w)] + corner(x_fore - rf, -wf + rf, rf, -math.pi / 2) + corner(x_fore - rf, wf - rf, rf, 0.0) + [(L * 0.25, w), (bx, w)]
    aft_plan = [(bx, w)] + corner(x_aft + ra, w - ra, ra, math.pi / 2) + corner(x_aft + ra, -w + ra, ra, math.pi) + [(bx, -w)]
    m.use("fore")
    m.prism(fore_plan, fd, iz, "upper", top_paint=top_paint, bottom=True)
    m.use("aft")
    m.prism(aft_plan, fd, iz, "upper", top_paint=top_paint, bottom=True)
    # the lines painted on it, the arrester wires, the round-down's stripes
    lifts = [(L * fx, side) for fx, side in c["lifts"]]
    ll, lw = c["lift"]
    x = x_fore - 12.0
    while x > x_aft + 10.0:
        if not any(abs(x - lx) < ll * 0.5 + 4.0 and side == 0.0 for lx, side in lifts) and not (c["disc"] and abs(x - L * 0.38) < 7.6 + 4.2):
            yard.on(x).box((x, 0, iz + 0.02), (6.5, 0.4, 0.03), "line")
        x -= 13.0
    for hand in (PORT, STBD):
        for a, b in ((x_fore - 8.0, bx), (bx, x_aft + 6.0)):
            yard.on((a + b) * 0.5).box(((a + b) * 0.5, hand * (w - 1.3), iz + 0.02), (a - b, 0.3, 0.03), "line")
    m.use("aft")
    for k in range(7):
        xw = -L * 0.2 - k * L * 0.03
        m.rod((xw, -w + 1.6, iz + 0.14), (xw, w - 1.6, iz + 0.14), 0.035, "steel", sides=3, smooth=False)
    if c["disc"]:
        m.use("fore")
        m.disc((L * 0.38, 0, iz + 0.03), Z, 7.6, "red", 32)
        m.use("aft")
        for k in range(6):       # the red and white stripes on her round-down
            m.box((x_aft + 3.2, (k - 2.5) * w * 0.3, iz + 0.025), (4.6, w * 0.15, 0.03), "red" if k % 2 == 0 else "white")

    # ---- the lifts: each a part that goes down to the hangar deck
    for i, (lx, side) in enumerate(lifts):
        ly = (w + lw * 0.5 - 0.6) * side
        ln = m.part(f"lift.{i + 1}", yard.half(lx), (lx, ly, iz), slide=(0, 0, -1), slide_limits=[0.0, round(hz - 0.6, 2)], kind="slide",
                    note="an aircraft lift: slides down to the hangar deck and back; the dark well under it is part of the deck")
        m.use(yard.half(lx))
        plan = Model.round_rect(lx + ll * 0.5, lx - ll * 0.5, ly - lw * 0.5, ly + lw * 0.5, 0.5, 2)
        if side == 0.0:
            m.add([Vector((px, py, iz + 0.012)) for px, py in plan], [tuple(range(len(plan)))], "black", False, lambda q: Z)
        else:                    # a lift on the deck's edge hangs outside the ship: its guides down her side
            for dx in (-ll * 0.4, ll * 0.4):
                m.box((lx + dx, hw * side + 0.3 * side, Z0 + hz * 0.5), (0.5, 0.5, hz), "upper")
        m.use(ln)
        m.prism(plan, iz - 0.3, iz + 0.035, "upper", top_paint=top_paint, bottom=True)
        if fine:
            for dx in (-0.5, 0.5):
                m.box((lx + dx * (ll - 0.2), ly, iz + 0.045), (0.14, lw - 0.6, 0.02), "seam")
            for dy in (-0.5, 0.5):
                m.box((lx, ly + dy * (lw - 0.2), iz + 0.045), (ll - 0.6, 0.14, 0.02), "seam")
        m.socket(f"lift.{i + 1}", (lx, ly, iz + 0.1), (0, 0, 1), ln, "an aircraft comes up here")

    # ---- the island
    ix, il = L * c["island"][0], c["island"][1]
    yc = STBD * (w - 2.6)
    t1 = yard.house(ix + il * 0.5, ix - il * 0.5, 2.4, iz, 3.2, r=1.0, y=yc, rail=False)
    t2 = yard.house(ix + il * 0.5 - 0.8, ix + il * 0.5 - min(9.0, il * 0.6), 2.2, t1, 2.6, r=0.9, y=yc, rail=False)
    yard.windows(ix + il * 0.5 - 0.8, 2.2, t2 - 1.45, 0.9, y=yc, back=3.0)
    t3 = yard.house(ix + il * 0.5 - 1.8, ix + il * 0.5 - min(7.4, il * 0.5), 1.8, t2, 2.2, r=0.8, y=yc, rail=False)
    yard.windows(ix + il * 0.5 - 1.8, 1.8, t3 - 1.35, 0.8, y=yc, back=2.4)
    yard.director("director.fore", ix + il * 0.5 - min(4.6, il * 0.3), t3, 2.2, y=yc)
    mx = ix + il * 0.5 - min(10.0, il * 0.66)
    head = yard.mast(mx, t1, 15.0, lean=0.03, tripod=1.5, yards=((0.6, 0.24), (0.8, 0.14)), y=yc)
    radar(yard, "radar.air", head.x, head.z + 0.2, 4.2, 1.8, y=yc)
    if c["funnels"] == "island":
        fl = il * 0.36
        fxn = ix - il * 0.5 + fl * 0.5 + 1.2
        yard.funnel(fxn, t1, 8.6 if tag == "USA" else 7.4, fl, 3.9, 0.06, y=yc)
        yard.fire_points.append((fxn + 4.0, yc, t1))
    else:                        # the boiler smoke is led out and down over the starboard side, abaft the island
        for k, fx_ in enumerate((-L * 0.02, -L * 0.075)):
            m.use(yard.half(fx_))
            a, b, e = Vector((fx_, STBD * (hw - 1.0), fd - 2.4)), Vector((fx_ - 0.5, STBD * (hw + 3.2), fd - 2.6)), Vector((fx_ - 1.6, STBD * (hw + 5.0), fd - 5.4))
            m.rod(a, b, 1.75, "upper", sides=12 if fine else 6)
            m.rod(b, e, 1.7, "upper", rb=1.55, sides=12 if fine else 6)
            m.ball(b, (1.74, 1.74, 1.74), "upper", 10, 6)
            d = (e - b).normalized()
            m.disc(e + d * 0.02, d, 1.35, "black", 12)
            m.socket(f"funnel.{k + 1}", e + d * 0.6, d, yard.half(fx_), "a funnel's mouth, turned down over the side: her smoke rolls out and aft along the water")
            yard.soot_at.append((e.x, e.y, e.z + 2.0, 6.0))
        yard.fire_points.append((ix, yc, t1))
    for dx in (il * 0.5 + 3.0, -il * 0.5 - 3.0):
        yard.light_gun(ix + dx, yc, iz + 0.02, 4, tub=True, facing=-70.0 if dx > 0 else -110.0, long=2.0)

    # ---- the guns: galleries of light guns under the deck's edge, heavier pairs at her four corners
    k = 0
    for gx in (L * 0.3, L * 0.19, -L * 0.1, -L * 0.22, -L * 0.34):
        for hand in (PORT, STBD):
            if hand == STBD and c["funnels"] == "side" and -L * 0.11 < gx < L * 0.02:
                continue
            if any(side * hand > 0 and abs(gx - lx) < ll * 0.5 + 5.0 for lx, side in lifts):
                continue
            yg = hand * (w + 1.1)
            mm = yard.on(gx)
            mm.box((gx, yg, fd - 0.75), (9.0, 2.6, 0.24), "upper")
            for dx in (-3.6, 3.6):
                mm.rod((gx + dx, hand * hw, fd - 3.2), (gx + dx, yg + hand * 0.9, fd - 0.85), 0.12, "upper", sides=4)
            for dx in (-2.4, 2.4):
                yard.light_gun(gx + dx, yg, fd - 0.62, 2, tub=False, facing=hand * 90.0, long=1.8)
    for i, gx in enumerate((L * 0.37, -L * 0.4)):
        for hand in (PORT, STBD):
            yg = hand * (hw + 0.6)
            zg = fd - 3.4
            mm = yard.on(gx)
            mm.lathe((gx, yg, zg - 2.6), Z, [(0.0, 0.6), (1.6, 2.5), (2.6, 2.7)], "hull", 12 if fine else 6)
            yard.turret(f"sec.{'P' if hand == PORT else 'S'}{i + 1}", gx, zg, 3.6, 2, 4.6, 0.0 if gx > 0 else 180.0, 0.11, y=yg, style="medium", rate=15.0, elev=(-5, 80), rest_elev=8.0,
                        limits=(-150, 30) if (hand == PORT) == (gx > 0) else (-30, 150), floats=False)

    # ---- boats under the overhang, forecastle and quarterdeck, the aircraft ranged aft
    for hand in (PORT, STBD):
        yard.boat(-L * 0.445, hand * min(h.half_at(-L * 0.445) * 0.5, 4.0), yard.deck(-L * 0.445) + 0.5, 8.5)
    yard.anchor_gear(L * 0.455, L * 0.41, spread=2.0)
    for bx_ in (L * 0.43, -L * 0.455):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.8), yard.deck(bx_))
    yard.rails(hx0 + 1.5, L * 0.47)
    yard.rails(-L * 0.485, hx1 - 1.5)
    yard.portholes(-L * 0.4, L * 0.42, 2.0, every=3.4)
    yard.portholes(-L * 0.36, L * 0.4, 4.4, every=3.4)
    dp = m.part("deckpark", "aft", (-L * 0.38, 0, iz), kind="fixed", note="aircraft ranged aft: hide this part when her air group is in the air")
    m.use(dp)
    n_rows = 4 if fine else 2
    for r_ in range(n_rows):
        for j, yy in enumerate((-w * 0.52, 0.0, w * 0.52)):
            parked(m, -L * 0.3 - r_ * 9.4 - (4.2 if j == 1 else 0.0), yy, iz + 0.05, 0.0)
    m.socket("launch", (L * 0.42, 0, iz + 1.5), (1, 0, 0), "fore", "an aircraft leaves the deck here, climbing ahead")
    m.socket("land", (-L * 0.43, 0, iz + 2.5), (-1, 0, 0.1), "aft", "an aircraft comes in over the stern here")
    m.socket("ensign", (head.x - 1.0, yc, head.z - 3.0), (-1, 0, 0), yard.half(head.x), "the battle ensign flies from the island's mast")
    yard.screws_and_rudder(screws=4, r=2.3, rudders=1)
    yard.fire_points += [(L * 0.2, 0.0, iz), (-L * 0.08, 0.0, iz), (-L * 0.27, 0.0, iz)]
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, [], "carrier")
    for i in range(len(lifts)):
        down = round(hz - 0.6, 2)
        m.clips[f"lift.{i + 1}"] = {"long": 16.0, "loop": False, "about": "a lift goes down to the hangar, waits for an aircraft, and comes up with it",
                                   "tracks": [{"part": f"lift.{i + 1}", "slide": [[0.0, 0.0], [5.0, down], [10.0, down], [15.0, 0.0], [16.0, 0.0]]}],
                                   "events": [{"t": 0.0, "fx": "", "socket": f"lift.{i + 1}", "cue": "lift_motor"}]}
    return m, yard


def submarine(tag, fine=True, hull_hex=None):
    """A submarine: a round hull low in the water under a narrow casing, a conning tower, a gun on deck, planes fore and aft."""
    # the plain boat: 80 m by 7
    c = dict(name="Submarine", title="Submarine (our own model)", hull=dict(L=80.0, B=7.0, D=1.8, T=4.6, sheer=0.9, rake=2.5, counter=1.5, flare=0.0, fine=1.9, full=0.6, bilge=2.0, tumble=0.5, wl_fore=0.7),
             tower=(4.0, -4.5, 2.7), gun=14.0, gun_aft=None, aa=-6.5, tubes=(4, 2))
    if tag == "GER":      # 67 m by 6.2: a small tower with a railed gun platform abaft it, the gun before it
        c.update(name="Submarine_GER", title="Submarine, German layout (our own model)", hull=dict(L=67.0, B=6.2, D=1.5, T=4.7, sheer=0.8, rake=2.2, counter=1.4, flare=0.0, fine=1.9, full=0.6, bilge=2.0, tumble=0.48, wl_fore=0.7),
                 tower=(3.4, -3.2, 2.5), gun=11.5, aa=-5.4, tubes=(4, 1))
    elif tag == "USA":    # 95 m by 8.3: a long high casing, a tall narrow tower with its periscope shears, a gun abaft it
        c.update(name="Submarine_USA", title="Submarine, American layout (our own model)", hull=dict(L=95.0, B=8.3, D=2.4, T=4.6, sheer=1.4, rake=3.0, counter=1.6, flare=0.0, fine=1.8, full=0.62, bilge=2.0, tumble=0.56, wl_fore=0.7),
                 tower=(6.0, -5.0, 2.9), gun=None, gun_aft=-13.0, aa=8.2, tubes=(6, 4))
    elif tag != "PLAIN":
        c.update(name=f"Submarine_{tag}", title=f"Submarine ({tag} paint; our own model)")
    m = Model(c["name"], c["title"], "ship", livery(tag, "submarine", hull_hex), fine)
    h = Hull(**c["hull"])
    L = h.L
    yard = Yard(m, h, break_x=-1.0)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("deck", frames=72 if fine else 32)
    x0, x1, th = c["tower"]
    Z0 = yard.deck((x0 + x1) * 0.5)

    # ---- the conning tower: a rounded house, a bulwark round her bridge, the periscope standards
    half = min(1.25, h.half_at((x0 + x1) * 0.5) * 0.8)
    t1 = yard.house(x0, x1, half, Z0, th, r=half * 0.95, rail=False, walls=False)
    yard.house(x0 - 0.1, x0 - 2.6, half * 0.9, t1, 0.95, r=half * 0.8, top="deck", rail=False, walls=False)       # the bridge screen
    m.use(yard.half(x0))
    stand = x0 - 3.6
    m.lathe((stand, 0, t1), Z, [(0.0, 0.42), (1.7, 0.32)], "upper", 10 if fine else 6)
    scopes = []
    for i, (px, r_) in enumerate(((stand, 0.09), (stand - 1.0, 0.07))):
        pn = m.part(f"periscope.{i + 1}", yard.half(px), (px, 0, t1 + 0.4), slide=(0, 0, 1), slide_limits=[0.0, 4.2], kind="slide", note="a periscope: slides up out of its standard")
        m.use(pn)
        m.rod((px, 0, t1 - 2.2), (px, 0, t1 + 1.9 - i * 0.5), r_, "steel", sides=6)
        m.box((px + 0.05, 0, t1 + 1.8 - i * 0.5), (0.2, 0.16, 0.22), "steel")
        m.socket(f"periscope.{i + 1}", (px, 0, t1 + 1.9 - i * 0.5), (0, 0, 1), pn, "the periscope's head: its feather of spray starts here when she runs just under")
        scopes.append(pn)
    m.use(yard.half(x1))
    if tag == "USA":          # the shears that carry her periscopes and aerials
        for dx in (0.0, -1.0):
            m.rod((stand + dx, 0.0, t1), (stand + dx, 0.0, t1 + 3.6), 0.2, "upper", sides=6)
        m.box((stand - 0.5, 0, t1 + 3.3), (1.6, 0.5, 0.3), "upper")
    # the gun platform abaft the tower (or before it), with its light gun
    ax = c["aa"]
    px0, px1 = (x1 + 0.3, x1 - 3.4) if ax < 0 else (x0 + 3.2, x0 - 0.3)
    plat = yard.house(px0, px1, half * 0.95, Z0, th * 0.42, r=half * 0.8, top="deck", rail=True, walls=False)
    yard.light_gun((px0 + px1) * 0.5, 0.0, plat + 0.02, 1, tub=False, facing=180.0 if ax < 0 else 0.0, long=1.5)

    # ---- the deck gun
    mains = []
    gx = c["gun"] if c["gun"] is not None else c["gun_aft"]
    m.use(yard.half(gx))
    m.lathe((gx, 0, yard.deck(gx) - 0.05), Z, [(0.0, 1.3), (0.35, 1.3)], "deck", 12 if fine else 6)
    tn, gs = yard.turret("turret.A", gx, yard.deck(gx) + 0.3, 1.5, 1, 3.6, 0.0 if c["gun"] is not None else 180.0, 0.09, style="shield", rate=12.0, elev=(-5, 35), rest_elev=2.0, floats=False)
    mains.append((tn, gs, gx, False))

    # ---- the planes fore and aft: each pair turns together
    for name, px, zz, span, chord, lim in (("planes.bow", h.frame_x(0.78), -1.3, 2.3, 1.7, 25), ("planes.stern", h.frame_x(-0.88), -2.2, 1.9, 1.6, 25)):
        pn = m.part(name, yard.half(px), (px, 0, zz), axis=(0, -1, 0), limits=[-lim, lim], kind="turn", note="diving planes, port and starboard together: plus turns their fore edges up")
        m.use(pn)
        for hand in (PORT, STBD):
            y0 = hand * max(h.half_water(h.u_of(px)) * 0.85, 0.3)
            plan = [(px + chord * 0.5, y0), (px + chord * 0.3, y0 + hand * span), (px - chord * 0.5, y0 + hand * span), (px - chord * 0.5, y0)]
            if hand == STBD:
                plan = plan[::-1]
            m.prism(plan, zz - 0.07, zz + 0.07, "hull", bottom=True)

    # ---- torpedo tubes: the doors in her bows and stern
    nb, ns = c["tubes"]
    for k in range(nb):
        hand = PORT if k % 2 == 0 else STBD
        zz = -1.5 - (k // 2) * 1.1
        p, n = h.side(h.frame_x(0.93), zz, hand)
        m.use("fore")
        m.disc(p + n * 0.02, n, 0.3, "black", 8, oval=1.6)
        m.socket(f"torpedo.bow.{k + 1}", p + X * 1.0, (1, 0, 0), "fore", "a torpedo leaves her bow here, under water: only a track of bubbles shows")
    for k in range(ns):
        hand = PORT if k % 2 == 0 else STBD
        zz = -1.6 - (k // 2) * 0.9
        p, n = h.side(h.frame_x(-0.95), zz, hand)
        m.use("aft")
        m.disc(p + n * 0.02, n, 0.28, "black", 8, oval=1.4)
        m.socket(f"torpedo.stern.{k + 1}", p - X * 1.0, (-1, 0, 0), "aft", "a torpedo leaves her stern here")

    # ---- the casing: free-flooding holes along it, the jumping wires over all, a few rails by the tower
    if fine:
        x = -L * 0.42
        k = 0
        while x < L * 0.44:
            for hand in (PORT, STBD):
                p, n = h.side(x, yard.deck(x) * (0.55 if k % 2 == 0 else 0.3), hand)
                yard.on(x).disc(p + n * 0.02, n, 0.13, "black", 6, oval=1.8)
            x += 1.25
            k += 1
    bow_top = Vector((h.frame_x(1.0) + h.rake * 0.9, 0, yard.deck(L * 0.49) + 0.5))
    stern_top = Vector((-L * 0.48, 0, yard.deck(-L * 0.48) + 0.5))
    wire_top = Vector((stand, 0, t1 + (3.7 if tag == "USA" else 1.9)))
    yard.wire(bow_top, wire_top, 0.02)
    yard.wire(wire_top, stern_top, 0.02)
    m.use("fore")
    m.rod(bow_top - Z * 0.5, bow_top, 0.05, "upper", sides=4)
    m.use("aft")
    m.rod(stern_top - Z * 0.5, stern_top, 0.05, "upper", sides=4)
    yard.rails(x1 - 6.0, x0 + 6.0, inset=0.1, every=2.0, high=0.9)
    m.socket("ensign", (x1 - 0.2, 0, t1 + 1.6), (-1, 0, 0), yard.half(x1), "her ensign flies from a staff abaft the tower")
    m.rod((x1 - 0.2, 0, t1 - 0.3), (x1 - 0.4, 0, t1 + 1.8), 0.035, "upper", sides=4)
    yard.screws_and_rudder(screws=2, blades=3, r=0.95, rudders=1)
    yard.fire_points += [((x0 + x1) * 0.5, 0.0, t1), (gx, 0.0, yard.deck(gx) + 1.0)]
    sockets_common(yard)
    m.weather = weather_for(yard)
    clips_for_ship(m, yard, mains, "submarine")

    # ---- her own clips: she dives, she comes up, she shows a periscope
    under = t1 + 5.0                       # how far down takes everything out of sight
    scope = t1 + 1.2                       # how far down leaves only a raised periscope showing
    T = 16.0

    def dive_rot(t):
        return (0.0, -7.0 * math.sin(math.pi * smooth(t / T)) - 1.5 * smooth(t / 3.0) * (1 - smooth((t - T + 3.0) / 3.0)), 0.0)
    m.clips["dive"] = {"long": T, "loop": False, "after": "gone", "about": "she dives: bows down, the sea washes over her casing, the tower goes last; then nothing but a swirl",
                       "tracks": [{"part": "ship", "rot": quat_keys(T, dive_rot), "move": move_keys(T, lambda t: (0.0, 0.0, -under * smooth((t - 1.5) / (T - 1.5)) ** 1.3))},
                                  {"part": "planes.bow", "turn": [[0.0, 0.0], [1.5, -20.0], [T - 3.0, -20.0], [T, 0.0]]}, {"part": "planes.stern", "turn": [[0.0, 0.0], [1.5, 12.0], [T - 3.0, 12.0], [T, 0.0]]}],
                       "events": [{"t": 0.0, "fx": "", "socket": "wake.bow", "cue": "dive_alarm"}, {"t": 2.0, "fx": "vents_spray", "socket": "fire.1"}, {"t": T * 0.7, "fx": "steam_and_bubbles", "socket": "fire.1"}]}

    def up_rot(t):
        return (0.0, 9.0 * math.sin(math.pi * smooth(t / 12.0)), 0.0)
    m.clips["surface"] = {"long": 12.0, "loop": False, "about": "she comes up: the bows break out first in a smother of foam, then the tower, and she levels off (start her under: play 'dive' to its end first)",
                          "tracks": [{"part": "ship", "rot": quat_keys(12.0, up_rot), "move": move_keys(12.0, lambda t: (0.0, 0.0, -under * (1 - smooth(t / 9.0)) ** 1.4))},
                                     {"part": "planes.bow", "turn": [[0.0, 22.0], [8.0, 22.0], [12.0, 0.0]]}],
                          "events": [{"t": 2.5, "fx": "steam_and_bubbles", "socket": "wake.bow", "cue": "surfacing"}, {"t": 5.0, "fx": "steam_and_bubbles", "socket": "fire.1"}]}
    m.clips["periscope"] = {"long": 10.0, "loop": False, "after": "hold", "about": "running just under: she rises until only a periscope shows, raises it, and holds there (the feather at its head is the game's own effect)",
                            "tracks": [{"part": "ship", "move": [[0.0, 0.0, 0.0, -under], [6.0, 0.0, 0.0, -scope - 4.2 + 0.9], [10.0, 0.0, 0.0, -scope - 4.2 + 0.9]]},
                                       {"part": "periscope.1", "slide": [[0.0, 0.0], [5.0, 0.0], [8.0, 4.2], [10.0, 4.2]]}],
                            "events": [{"t": 7.0, "fx": "periscope_feather", "socket": "periscope.1"}]}
    for k_, (where, n_) in enumerate((("bow", nb), ("stern", ns))):
        m.clips[f"torpedoes.{where}"] = {"long": 1.0 + 0.8 * n_, "loop": False, "about": f"she fires her {where} tubes, one fish after another",
                                         "tracks": [], "events": [{"t": round(0.3 + 0.8 * k, 2), "fx": "torpedo_launch_submerged", "socket": f"torpedo.{where}.{k + 1}", "cue": "torpedo_launch" if k == 0 else ""} for k in range(n_)]}
    return m, yard


BUILDERS = {
    "Battleship_UK": lambda fine: battleship("UK", fine),
    "Battleship_GER": lambda fine: battleship("GER", fine),
    "Battleship_USA": lambda fine: battleship("USA", fine),
    "Battleship_JAP": lambda fine: battleship("JAP", fine),
    "Battleship_ITA": lambda fine: battleship("ITA", fine),
    "Battleship_FRA": lambda fine: battleship("FRA", fine),
    "Battleship": lambda fine: battleship("PLAIN", fine),
    "Cruiser_UK": lambda fine: cruiser("UK", fine),
    "Cruiser_GER": lambda fine: cruiser("GER", fine),
    "Cruiser_USA": lambda fine: cruiser("USA", fine),
    "Cruiser_JAP": lambda fine: cruiser("JAP", fine),
    "Cruiser_ITA": lambda fine: cruiser("ITA", fine),
    "Cruiser": lambda fine: cruiser("PLAIN", fine),
    "Destroyer": lambda fine: destroyer("PLAIN", fine),
    "Destroyer_GER": lambda fine: destroyer("GER", fine),
    "Destroyer_USA": lambda fine: destroyer("USA", fine),
    "Destroyer_JAP": lambda fine: destroyer("JAP", fine),
    "Carrier": lambda fine: carrier("PLAIN", fine),
    "Carrier_USA": lambda fine: carrier("USA", fine),
    "Carrier_JAP": lambda fine: carrier("JAP", fine),
    "Submarine": lambda fine: submarine("PLAIN", fine),
    "Submarine_GER": lambda fine: submarine("GER", fine),
    "Submarine_USA": lambda fine: submarine("USA", fine),
}

# the ships of a convoy and a landing (merchants.py) are built with this file's pieces, so they are brought in last
import merchants  # noqa: E402

BUILDERS.update(merchants.BUILDERS)
