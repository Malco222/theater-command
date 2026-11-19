"""USS Yorktown (CV-5), as she was at Midway in June 1942: the named ship for the Pearl Harbor and Midway battles.

    Yorktown   America's fleet carrier of the 1937 design, stands in for `Carrier_USA` in 1941-42

Laid out against the builder's booklet of general plans (Newport News, main and forecastle decks: her frames 4 ft
apart, frame 0 at the forward perpendicular) and measured against photographs of her at Pearl Harbor in May 1942 and
at Midway on 4 June 1942. Nothing of any drawing or model is copied: every shape is our own, made from numbers.

What she was: 824 ft 9 in (251.4 m) over all with her flight deck's overhang aft, 809 ft 6 in (246.7 m) her hull,
770 ft (234.7 m) at the waterline; 83 ft 3 in (25.4 m) at the waterline and 109 ft 6 in (33.4 m) over her gun
platforms; a 802 by 86 ft wooden flight deck, rounded down at its after end, over an open hangar whose sides close with
roller curtains (some rolled up, the hangar showing; her boats stowed in bays at the hangar deck aft); three
centre-line lifts (48 by 44 ft) at frames 32, 120 and 179; two hydraulic catapults forward on the flight deck;
arresting wires aft and forward (she could land aircraft over her bow) with their barriers; hinged radio masts lowered
flat along her galleries. Her island to starboard: the bridge with its faceted front, a tripod with the sky control
top and the CXAM air-search aerial over it (fitted 1940), a long flat-sided funnel, a pole mast at its after end where
the ensign flew, a Mark 33 director at each end, Primary Flight Control in a glazed booth on the island's port side
aft, and the aircraft crane at the deck's edge abaft it. Her guns: eight single 5-inch/38 in open mountings on
platforms at the four corners of her hangar, two forward and two aft each side; four quadruple 1.1-inch ("Chicago
pianos"), two before and two abaft the island, one of each pair at the flight deck and one over it; twenty-four 20 mm
Oerlikons on platforms along her galleries (fitted at Norfolk in December 1941) and .50 calibre machine guns on her
island. All of them manned.

Her paint at Midway (Ships-2, September 1941): Measure 12 graded for a carrier, sea blue 5-S from the waterline to the
hangar deck, ocean grey 5-O over it to the top of the island, haze grey 5-H on her masts and their tops; deck blue 20-B
on steel decks; the flight deck stained with Norfolk's 250-N flight deck stain, a dark blue-grey. Her life rafts
painted out in the ocean grey of the walls they hang on.

Parts, sockets and clips are those of every carrier (see the README's "Ships" and "A carrier's deck"): `sec.P1..P4` and
`sec.S1..S4` the 5-inch, counted from forward; `aa28.1..4` the 1.1-inch quads with their `.guns`; `aa.N` every light
gun's muzzle; `lift.1..3`; `deckpark` (its range stands forward of the after lift, which stays clear of it);
`radar.air` (the CXAM, spinning); `director.fore`, `director.aft`.
"""

import math

from mathutils import Matrix, Vector

import fittings as fit
from rigid import PORT, STBD, X, Y, Z, Model, lerp
from shipyard import Yard, livery, weather_for

# (named before capitals is imported: capitals reads every named ship's BUILDERS as it loads)
BUILDERS = {"Yorktown": lambda fine: yorktown(fine)}

import capitals as cap  # noqa: E402


# her hull's deck edge, off the general plans (half widths as shares of 13.2 m): the main deck aft of frame 24, the
# forecastle deck forward of it (she steps up a deck there); a broad round stern, the bow flared out over a fine entry
YT_PLAN = [(-123.3, 0.0), (-123.0, 0.13), (-122.0, 0.3), (-120.0, 0.46), (-117.5, 0.64), (-115.0, 0.78), (-110.0, 0.88), (-100.0, 0.955),
           (-90.0, 0.985), (-75.0, 1.0), (30.0, 1.0), (40.0, 0.99), (50.0, 0.975), (60.0, 0.955), (70.0, 0.955), (80.0, 0.935), (90.0, 0.895),
           (100.0, 0.82), (106.0, 0.75), (110.0, 0.68), (115.0, 0.53), (119.0, 0.37), (121.5, 0.22), (123.3, 0.0)]
# the flight deck's edges, aft to fore: 802 ft by 86, its after end out over the stern on pillars and knees (824 ft 9 in
# over all with it), narrowing over the forecastle to its squared fore end with her stem standing out ahead of it; to
# starboard it is built out by the island for the island, its 1.1-inch mountings and the crane
YT_PORT = [(-128.1, 5.5), (-127.7, 9.6), (-126.7, 11.8), (-124.9, 12.6), (-120.0, 12.9), (-108.0, 13.1), (88.0, 13.1), (96.0, 12.6),
           (103.0, 11.85), (109.5, 10.95), (114.6, 10.1), (115.9, 8.9), (116.4, 6.0)]
YT_STAR = [(x, -y) for x, y in YT_PORT[:6]] + [(-34.5, -13.1), (-33.0, -15.6), (42.5, -15.6), (44.0, -13.1)] + [(x, -y) for x, y in YT_PORT[6:]]
FD_AFT, FD_FORE, RD_X = -128.1, 116.4, -124.4    # the flight deck's ends; where its round-down begins

# Measure 12 graded (carriers): sea blue to the hangar deck, ocean grey above, haze grey masts; deck blue; stained deck;
# the life rafts in the ocean grey of their walls
YT_PAINT = {"hull": "414D5D", "upper": "6B7681", "light": "939CA3", "deck": "3D4857", "planks": "46525F", "flight": "46525F",
            "roof": "3D4857", "door": "5C6671", "seam": "3E4956", "plane": "6E7A86", "boat": "66717C", "canvas": "4A5562", "cork": "6B7681"}

IZ, FD = 13.8, 13.1                     # the flight deck's top and its underside, over the waterline
ZG = IZ - 1.4                           # the gallery walkway round her under the deck's edge
YI = -12.8                              # the middle of the island's foot


def edge(hand, x):
    """The flight deck's edge at x on one side (its y, signed)."""
    return cap.edge_at(YT_PORT if hand == PORT else YT_STAR, x)


def plate(m, pts, high, centre, paint="upper", thick=0.05, lip=True):
    """A thin upright plate along a row of points at its foot (a bulwark, a splinter screen): an outer face away from
    `centre`, an inner face toward it, a narrow top."""
    pts = [Vector(p) for p in pts]
    n = len(pts)
    if n < 2:
        return
    outs = []
    for i, p in enumerate(pts):
        d = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
        o = Vector((d.y, -d.x, 0.0)).normalized()
        if o.dot(Vector((p.x - centre.x, p.y - centre.y, 0.0))) < 0:
            o = -o
        outs.append(o)
    lo_o = [p + o * thick * 0.5 for p, o in zip(pts, outs)]
    lo_i = [p - o * thick * 0.5 for p, o in zip(pts, outs)]
    hi_o = [p + Z * high for p in lo_o]
    hi_i = [p + Z * high for p in lo_i]
    c = Vector((centre.x, centre.y, 0.0))
    for a, b, way in ((lo_o, hi_o, 1.0), (lo_i, hi_i, -1.0)):
        m.add(a + b, [(k, k + 1, n + k + 1, n + k) for k in range(n - 1)], paint, True,
              lambda q, w=way: Vector((q.x - c.x, q.y - c.y, 0.0)) * w)
    if lip:
        m.add(hi_o + hi_i, [(k, k + 1, n + k + 1, n + k) for k in range(n - 1)], paint, False, lambda q: Z)
        for k in (0, n - 1):
            m.add([lo_o[k], lo_i[k], hi_i[k], hi_o[k]], [(0, 1, 2, 3)], paint, False, lambda q, d=(pts[k] - pts[1 if k == 0 else n - 2]): d)


def ccw(plan):
    """A plan turned anticlockwise seen from above."""
    a = sum(plan[i][0] * plan[(i + 1) % len(plan)][1] - plan[(i + 1) % len(plan)][0] * plan[i][1] for i in range(len(plan)))
    return plan if a > 0 else list(reversed(plan))


def web(m, x, yz, thick, paint="upper"):
    """A flat plate standing athwartships at x, its outline `yz` ((y, z) points, a fan from the first), `thick` through."""
    n = len(yz)
    a = [Vector((x - thick * 0.5, y_, z_)) for y_, z_ in yz]
    b = [Vector((x + thick * 0.5, y_, z_)) for y_, z_ in yz]
    m.add(a, [(0, k, k + 1) for k in range(1, n - 1)], paint, False, lambda q: -X)
    m.add(b, [(0, k, k + 1) for k in range(1, n - 1)], paint, False, lambda q: X)
    c = sum((Vector((x, y_, z_)) for y_, z_ in yz), Vector()) / n
    m.add(a + b, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], paint, False, lambda q: Vector((0.0, q.y - c.y, q.z - c.z)))


def officer(m, foot, facing, glasses=True):
    """An officer (or a lookout) standing, his binoculars up to his eyes; or with his hands at his sides."""
    f = Vector((facing[0], facing[1], 0.0)).normalized()
    l = Z.cross(f)
    foot = Vector(foot)
    if not glasses:
        fit.sailor(m, foot, f, "stand", navy="USA")
        return
    c = foot + Z * 1.66 + f * 0.2
    fit.sailor(m, foot, f, "aim", hands=(c + l * 0.075 - Z * 0.03, c - l * 0.075 - Z * 0.03), navy="USA", lean=0.03)
    if m.fine:
        for s in (1, -1):
            m.rod(c + l * s * 0.045 - f * 0.07, c + l * s * 0.045 + f * 0.09, 0.03, "gunmetal", sides=6)


# ---- her own fittings ---------------------------------------------------------------------------------------------

def quad_28(yard, n, x, y, z, bearing=0.0, tub=2.3, tub_high=1.15, crew=True, limits=None, parent=None):
    """The quadruple 1.1-inch mounting (the 'Chicago piano'): four water-cooled guns side by side on one slide, an
    eight-round clip standing up out of each breech, the pointer's and the trainer's seats and ring sights either side,
    on a turning platform over its pedestal, in a splinter tub with its ready-use lockers. Its crew: pointer and
    trainer seated, four loaders behind the breeches, the mount captain in the tub. Parts `aa28.N` (trains: plus
    clockwise seen from above; rest `bearing`, port negative; `limits` keep its fire off the island) and
    `aa28.N.guns` (plus lifts them); socket `aa.K` at the muzzles, riding the guns."""
    m = yard.m
    half = parent or yard.half(x)
    m.use(half)
    o = Vector((x, y, z))
    fine = m.fine
    if tub:
        m.tub(o, tub, tub_high, "upper", wall=0.04, sides=26 if fine else 10, floor="deck")
        if fine:      # the ready-use lockers for the clips round the back of the tub, the lip on its rim
            for i in range(7):
                a = math.radians(-bearing + 180.0 + (i - 3) * 27.0)
                c = o + Vector((math.cos(a), math.sin(a), 0.0)) * (tub - 0.27)
                m.box(c + Z * 0.47, (0.34, 0.62, 0.9), "upper", yaw=math.degrees(a) + 90.0)
            m.lathe(o + Z * (tub_high - 0.02), Z, [(0.0, tub + 0.035), (0.06, tub + 0.035)], "upper", 26, caps=(False, False))
    tn = m.part(f"aa28.{n}", half, (x, y, z + 0.25), axis=(0, 0, -1), limits=list(limits or [-180, 180]), kind="turn",
                note=f"a quadruple 1.1-inch mounting: plus trains it clockwise seen from above; rest bearing {int(bearing)}; about 24 degrees a second")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 24.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y

    def W(px, py, pz):
        return o + f * px + l * py + Z * pz
    # the pedestal, the turning platform, the carriage and its trunnion standards
    m.lathe(o, Z, [(0.0, 0.72), (0.25, 0.64)], "upper", 16 if fine else 8)
    m.lathe(o + Z * 0.25, Z, [(0.0, 1.45), (0.07, 1.45)], "deck", 22 if fine else 8)
    m.box(W(-0.15, 0.0, 0.58), (1.3, 1.1, 0.52), "upper", yaw=-bearing)
    for side in (1, -1):
        m.box(W(0.05, side * 0.74, 0.97), (1.05, 0.16, 0.92), "upper", yaw=-bearing)
    piv = W(0.05, 0.0, 1.32)
    gn = m.part(f"aa28.{n}.guns", tn, tuple(piv), axis=tuple(-l), limits=[-15, 110], kind="turn", note="the four guns on their slide: plus lifts them")
    m.use(gn)
    e = math.radians(15.0)
    g = (f * math.cos(e) + Z * math.sin(e)).normalized()
    up = l.cross(g).normalized()
    if up.z < 0:
        up = -up
    m.box(piv - g * 0.22, (1.35, 1.26, 0.36), "upper", yaw=-bearing, pitch=15.0)                  # the slide, holding the four breeches
    for j, off in enumerate((-0.45, -0.15, 0.15, 0.45)):
        q = piv + l * off
        m.rod(q + g * 0.42, q + g * 1.28, 0.074, "upper", rb=0.07, sides=10 if fine else 5)      # the water jacket
        m.rod(q + g * 1.28, q + g * 2.3, 0.034, "gunmetal", sides=8 if fine else 4)              # the barrel
        m.lathe(q + g * 2.3, g, [(0.0, 0.04), (0.16, 0.052), (0.2, 0.046)], "gunmetal", 8 if fine else 4, caps=(False, True))
        if fine:
            m.box(q - g * 0.3 + up * 0.36, (0.13, 0.1, 0.42), "brass", yaw=-bearing, pitch=15.0)   # its clip of eight rounds
            m.box(q - g * 0.3 + up * 0.62, (0.15, 0.12, 0.05), "gunmetal", yaw=-bearing, pitch=15.0)
            m.lathe(q + g * 1.28, g, [(0.0, 0.085), (0.05, 0.085)], "upper", 10, caps=(False, False))
    if fine:
        for side in (1, -1):         # the ring sights on their arms, out over each seat
            c = piv + l * side * 0.95 + g * 0.35 + up * 0.25
            m.rod(piv + l * side * 0.64, c, 0.025, "steel", sides=4)
            m.lathe(c, g, [(0.0, 0.15), (0.02, 0.15)], "steel", 12, caps=(False, False))
            m.lathe(c - g * 0.45, g, [(0.0, 0.03), (0.03, 0.03)], "steel", 6, caps=(False, False))
    k = sum(1 for s in m.sockets if s["name"].startswith("aa.")) + 1
    m.socket(f"aa.{k}", piv + g * 2.5, g, gn, "a 1.1-inch mounting's four muzzles: tracer starts here")
    m.use(tn)
    for side in (1, -1):              # the pointer's and the trainer's seats, their handwheels
        s0 = W(-0.35, side * 1.06, 0.32)
        m.box(s0 + Z * 0.36, (0.38, 0.38, 0.06), "gunmetal", yaw=-bearing)
        m.rod(s0, s0 + Z * 0.34, 0.04, "upper", sides=5)
        for hw in (0.13, -0.13):
            m.lathe(W(0.08, side * 1.06 + hw, 0.92), f, [(0.0, 0.13), (0.03, 0.13)], "gunmetal", 8, caps=(False, False))
        m.rod(W(0.0, side * 0.82, 0.8), W(0.06, side * 1.06, 0.92), 0.035, "upper", sides=4)
    if crew and fine:
        for side in (1, -1):
            fit.sailor(m, W(-0.3, side * 1.06, 0.32), f, "sit", seat=0.4, hands=(W(0.08, side * 1.06 + 0.13, 0.92), W(0.08, side * 1.06 - 0.13, 0.92)), lean=0.08)
        for side, dx, py, pose in ((1, -0.95, 0.3, "load"), (-1, -0.95, 0.3, "load"), (1, -1.42, 0.5, "carry"), (-1, -1.42, 0.5, "carry")):
            fit.sailor(m, W(dx, side * py, 0.32), f, pose)
    m.use(half)
    if crew and fine and tub:
        a = math.radians(-bearing + 180.0 + 58.0)
        fit.sailor(m, o + Vector((math.cos(a), math.sin(a), 0.0)) * (tub - 0.62) + Z * 0.02, f, "stand")
    return tn


def open_5in_38(yard, name, x, y, z, bearing, limits, rest_elev=10.0):
    """The single 5-inch/38 in its open mounting, no shield, as the 1937 carriers had them at the corners of their
    hangars: the stand on the platform's deck and a round floor turning with the gun; the carriage, two side plates
    carrying the trunnions; the slide round the barrel with the two recoil cylinders under it and the counter-recoil
    cylinder over it; the breech housing; the loading tray and the rammer behind the breech; the fuze setter on the
    carriage's left; the pointer's and the trainer's seats either side with their handwheels and telescopes. Its crew:
    pointer and trainer seated at their telescopes, the fuze setter with a round, the loader at the tray, a shellman
    bringing up the next and the mount captain. A `sec.*` turret (`yard.turret` gives the part, its `.gun.1` and the
    muzzle socket); its geometry laid by +bearing (0 or 180)."""
    m = yard.m
    tn, guns = yard.turret(name, x, z, 3.4, 1, 4.9, bearing, 0.07, y=y, style="open", rate=15.0, elev=(-15, 85), rest_elev=rest_elev,
                           limits=limits, floats=False)
    fine = m.fine
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
    f, l = rot @ X, rot @ Y
    base = Vector((x, y, z))

    def W(px, py, pz):                        # (pz from the platform's deck)
        return base + rot @ Vector((px, py, pz))
    gp = Vector(m.parts[m.part_at[guns[0]]].pivot)
    tz = gp.z - z                             # the trunnions' height
    e = math.radians(rest_elev)
    g = f * math.cos(e) + Z * math.sin(e)
    up = Z * math.cos(e) - f * math.sin(e)
    m.use(tn)
    # the round floor turning with it, over the stand's foot
    m.prism(Model.oval(x, y, 1.6, 1.6, 28 if fine else 10), z + 0.3, z + 0.45, "upper", top_paint="deck", bottom=True)
    if not fine:
        return tn, guns
    # the carriage: a side plate either side, the trunnions through them
    for s in (1, -1):
        prof = [(-0.85, 0.45), (0.55, 0.45), (0.32, tz + 0.26), (-0.3, tz + 0.26)]
        a = [W(px, s * 0.56, pz) for px, pz in prof]
        b = [W(px, s * 0.64, pz) for px, pz in prof]
        mid = W(-0.1, s * 0.6, tz * 0.6)
        m.add(a + b, [(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], "upper", False, lambda q, c=mid: q - c)
        m.lathe(W(0.0, s * 0.64, tz), l * s, [(0.0, 0.17), (0.09, 0.17), (0.11, 0.12)], "steel", 12)
    # the loading tray behind the breech (on the carriage), the rammer beside it, their brackets
    tc = gp - g * 1.95 - up * 0.17
    m.box(tc, (0.85, 0.3, 0.05), "steel", yaw=bearing, pitch=rest_elev)
    for s in (1, -1):
        m.box(tc + l * s * 0.15 + up * 0.05, (0.85, 0.03, 0.12), "steel", yaw=bearing, pitch=rest_elev)
        m.rod(W(-0.82, s * 0.6, 1.0), tc + l * s * 0.14 - up * 0.03 + g * 0.2, 0.035, "upper", sides=4)
    m.box(tc + l * 0.32 + up * 0.08, (0.95, 0.18, 0.22), "upper", yaw=bearing, pitch=rest_elev)
    # the fuze setter on the carriage's left, a round standing in it
    m.box(W(-0.95, 0.86, 1.08), (0.42, 0.36, 0.62), "upper", yaw=bearing)
    m.rod(W(-0.95, 0.86, 0.45), W(-0.95, 0.86, 0.78), 0.08, "upper", sides=6)
    m.rod(W(-0.95, 0.86, 1.39), W(-0.95, 0.86, 1.72), 0.063, "ammo", sides=8)
    m.lathe(W(-0.95, 0.86, 1.72), Z, [(0.0, 0.063), (0.14, 0.035), (0.2, 0.0)], "brass", 8, caps=(False, False))
    # the pointer's and the trainer's seats, their handwheels on a column before each, their telescopes
    for s in (1, -1):
        m.rod(W(-0.38, s * 1.3, 0.45), W(-0.38, s * 1.3, 0.86), 0.045, "upper", sides=6)
        m.box(W(-0.38, s * 1.3, 0.88), (0.36, 0.34, 0.06), "gunmetal", yaw=bearing)
        m.box(W(-0.55, s * 1.3, 1.12), (0.05, 0.3, 0.4), "gunmetal", yaw=bearing)
        m.rod(W(0.27, s * 1.3, 0.45), W(0.27, s * 1.3, 1.22), 0.07, "upper", sides=6)
        m.box(W(0.27, s * 1.3, 1.3), (0.24, 0.42, 0.18), "upper", yaw=bearing)
        for hw in (0.17, -0.17):
            m.lathe(W(0.13, s * 1.3 + hw, 1.36), -f, [(0.0, 0.12), (0.025, 0.12)], "gunmetal", 10)
            m.rod(W(0.15, s * 1.3 + hw, 1.36), W(0.27, s * 1.3 + hw * 0.6, 1.33), 0.02, "steel", sides=4)
        m.lathe(W(-0.06, s * 1.3, 1.74), f, [(0.0, 0.045), (0.1, 0.045), (0.12, 0.06), (0.62, 0.06), (0.66, 0.07), (0.7, 0.07)], "gunmetal", 10)
        m.rod(W(0.27, s * 1.3, 1.39), W(0.27, s * 1.3, 1.69), 0.03, "upper", sides=4)
        m.rod(W(0.27, s * 1.3, 1.6), W(0.25, s * 0.64, 1.5), 0.03, "upper", sides=4)
    # the gun's own: the slide, recoil and counter-recoil cylinders, the breech housing and its lever
    m.use(guns[0])
    m.box(gp + g * 0.45, (1.9, 0.46, 0.42), "upper", yaw=bearing, pitch=rest_elev)
    for s in (1, -1):
        m.rod(gp - g * 0.3 - up * 0.27 + l * s * 0.13, gp + g * 1.75 - up * 0.27 + l * s * 0.13, 0.068, "upper", sides=8)
        m.lathe(gp + g * 1.75 - up * 0.27 + l * s * 0.13, g, [(0.0, 0.075), (0.06, 0.075)], "steel", 8)
    m.rod(gp - g * 0.2 + up * 0.29, gp + g * 1.6 + up * 0.29, 0.085, "upper", sides=8)
    m.box(gp - g * 0.82, (0.62, 0.46, 0.46), "steel", yaw=bearing, pitch=rest_elev)
    m.rod(gp - g * 1.0 + l * 0.24 + up * 0.1, gp - g * 1.35 + l * 0.3 + up * 0.32, 0.03, "steel", sides=4)
    m.lathe(gp + g * 5.87, g, [(0.0, 0.085), (0.05, 0.085)], "upper", 10, caps=(False, False))
    # the crew
    m.use(tn)
    for s in (1, -1):
        fit.sailor(m, W(-0.33, s * 1.3, 0.45), f, "sit", seat=0.45, hands=(W(0.06, s * 1.3 + 0.17, 1.42), W(0.06, s * 1.3 - 0.17, 1.42)), navy="USA", lean=0.06)
    fit.sailor(m, W(-1.55, 0.9, 0.0), f, "carry", navy="USA")
    fit.sailor(m, W(-2.25, -0.55, 0.0), f, "carry", navy="USA")
    fit.sailor(m, W(-2.5, 0.75, 0.0), f + l * 0.15, "carry", navy="USA")
    fit.sailor(m, W(-1.75, -1.25, 0.0), f + l * 0.35, "stand", navy="USA")
    return tn, guns


def mk33(yard, name, x, y, z, bearing=0.0, parent=None):
    """The Mark 33 director of the 5-inch battery (before radar): a squat house with its face sloped back, the
    rangefinder through its after part with a hood at each end, the pointer's and trainer's ports in its face, a door
    behind. A turn part (its geometry laid by +bearing: 0 or 180)."""
    m = yard.m
    dn = m.part(name, parent or yard.half(x), (x, y, z), axis=(0, 0, -1), kind="turn", note=f"a fire-control director: turns with the 5-inch guns; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
    o = Vector((x, y, z))

    def W(px, py, pz):
        return o + rot @ Vector((px, py, pz))
    m.lathe(o, Z, [(0.0, 1.45), (0.3, 1.45)], "upper", 18 if m.fine else 8)
    lo = [(1.75, 0.95), (1.3, 1.3), (-1.75, 1.3), (-1.75, -1.3), (1.3, -1.3), (1.75, -0.95)]
    hi = [(0.95, 0.9), (0.6, 1.24), (-1.7, 1.24), (-1.7, -1.24), (0.6, -1.24), (0.95, -0.9)]
    vl = [W(px, py, 0.3) for px, py in lo]
    vh = [W(px, py, 2.05) for px, py in hi]
    nn = len(lo)
    mid = W(0, 0, 1.2)
    m.add(vl + vh, [(k, (k + 1) % nn, nn + (k + 1) % nn, nn + k) for k in range(nn)], "upper", False, lambda q: q - mid)
    m.add(vh + [W(-0.4, 0, 2.07)], [(k, (k + 1) % nn, nn) for k in range(nn)], "roof", False, lambda q: Z)
    lat = rot @ Y
    c = W(-0.7, 0, 1.55)
    m.lathe(c - lat * 2.0, lat, [(0.0, 0.17), (0.2, 0.21), (3.8, 0.21), (4.0, 0.17)], "upper", 10 if m.fine else 5)
    for side in (1, -1):
        m.box(c + lat * side * 1.85 + rot @ Vector((0.15, 0, 0.1)), (0.45, 0.36, 0.45), "upper", yaw=bearing)
        m.disc(c + lat * side * 1.85 + rot @ Vector((0.38, 0, 0.1)), rot @ X, 0.1, "glass", 8)
    if m.fine:
        for side in (1, -1):
            m.disc(W(1.47, side * 0.62, 1.45), rot @ Vector((0.9, 0, 0.45)), 0.15, "glass", 8)
            m.box(W(-0.9, side * 0.75, 2.12), (0.5, 0.4, 0.1), "roof", yaw=bearing)          # the hatches in its roof
        m.box(W(-1.76, 0.0, 1.05), (0.05, 0.7, 1.4), "door", yaw=bearing)
        m.box(W(-1.4, 0.0, 2.3), (0.06, 1.5, 0.06), "steel", yaw=bearing)                    # a grab rail over the door
    return dn


def cxam(yard, name, x, y, z, parent, wide=5.2, high=4.6):
    """The CXAM air-search aerial (17 by 15 ft): a lattice frame standing on a turntable round the masthead, rows of
    dipoles across its face, a screen of wires behind them. It turns about the mast, the frame a little before it."""
    m = yard.m
    rn = m.part(name, parent, (x, y, z), axis=(0, 0, -1), kind="spin", note="the CXAM air-search aerial: turns steadily round the masthead")
    m.use(rn)
    fine = m.fine
    m.lathe((x, y, z), Z, [(0.0, 0.55), (0.25, 0.55), (0.32, 0.42)], "light", 14 if fine else 6)
    fx = x + 0.7                                     # (the plane of its face)
    c = Vector((fx, y, z + 0.55 + high * 0.5))
    w2, h2 = wide * 0.5, high * 0.5
    for dx in (-0.16, 0.16):                         # the frame: two chords along each edge, front and back
        for sy in (-1, 1):
            m.box(c + Vector((dx, sy * w2, 0.0)), (0.09, 0.09, high + 0.09), "light")
        for sz in (-1, 1):
            m.box(c + Vector((dx, 0.0, sz * h2)), (0.09, wide + 0.09, 0.09), "light")
    # the arms from the turntable to the frame's back
    for sy in (-1, 1):
        for sz in (-1, 1):
            m.rod((x, y + sy * 0.3, z + 0.32), c + Vector((-0.16, sy * w2 * 0.5, sz * h2 * 0.92)), 0.05, "light", sides=4)
    if fine:
        steps = 8
        for sy in (-1, 1):                           # the lacing between the chords: a zigzag up each side
            for i in range(steps):
                a = c + Vector((-0.16 if i % 2 else 0.16, sy * w2, -h2 + high * i / steps))
                b = c + Vector((0.16 if i % 2 else -0.16, sy * w2, -h2 + high * (i + 1) / steps))
                m.rod(a, b, 0.02, "light", sides=3, smooth=False)
        for sz in (-1, 1):
            for i in range(steps + 2):
                a = c + Vector((-0.16 if i % 2 else 0.16, -w2 + wide * i / (steps + 2), sz * h2))
                b = c + Vector((0.16 if i % 2 else -0.16, -w2 + wide * (i + 1) / (steps + 2), sz * h2))
                m.rod(a, b, 0.02, "light", sides=3, smooth=False)
        for i in range(1, 9):                        # the screen of wires behind, upright
            yy = -w2 + wide * i / 9.0
            m.rod(c + Vector((-0.16, yy, -h2)), c + Vector((-0.16, yy, h2)), 0.012, "rope", sides=3, caps=(False, False), smooth=False)
        for j in range(4):                           # the dipoles: rows across its face on their feeders
            zz = -h2 + high * (j + 0.5) / 4.0
            m.box(c + Vector((0.16, 0.0, zz)), (0.04, wide - 0.2, 0.04), "steel")
            for i in range(6):
                yy = -w2 + wide * (i + 0.5) / 6.0
                m.box(c + Vector((0.42, yy, zz)), (0.03, 0.62, 0.03), "steel")
                m.box(c + Vector((0.29, yy, zz)), (0.26, 0.03, 0.03), "steel")
    return rn


def gun_platform(yard, x0, x1, hand, z, out, guns, wall_x=None):
    """A platform for a pair of 5-inch at a corner of her hangar: built out from her side on brackets, a bulwark round
    its outer edge, a ladder up from the gallery. `guns`: their x. Returns the guns' y."""
    m = yard.on((x0 + x1) * 0.5)
    h = yard.h
    xs = [lerp(x0, x1, k / 16.0) for k in range(17)]
    inner = min(abs(h.half_at(xx)) for xx in xs) * 0.985 - 0.35
    gy = hand * (out - 1.9)
    plan = Model.round_rect(x1, x0, hand * inner, hand * out, 1.6, 3 if m.fine else 1)
    m.prism(plan, z - 0.4, z, "upper", top_paint="deck", bottom=True)
    if m.fine:
        # the bulwark: along the outer edge and round its ends, back to the ship's side
        n = len(plan)
        keep = {k for k in range(n) if abs(plan[k][1]) > inner + 0.3}
        k = next(k for k in sorted(keep) if (k - 1) % n not in keep)
        pts = []
        while k in keep and len(pts) < n:
            pts.append(Vector((*plan[k], z)))
            k = (k + 1) % n
        ctr = Vector(((x0 + x1) * 0.5, hand * inner, 0.0))
        plate(m, [p - Vector((0.0, hand * 0.03, 0.0)) for p in pts], 1.05, ctr)
        for gx in guns:               # a ready-use locker by each gun, a ladder at the after end
            m.box((gx - 2.6, hand * (inner + 0.55), z + 0.5), (0.9, 0.6, 1.0), "upper")
        for xb in xs[::3]:
            m.rod((xb, hand * (inner + 0.2), z - 3.2), (xb, hand * (out - 0.6), z - 0.38), 0.09, "upper", sides=4)
        from ships import ladder
        ladder(m, Vector((min(x0, x1) + 1.4, hand * (inner + 0.05), z)), z + (IZ - 1.2 - z) if z < IZ - 1.5 else z + 1.0, Vector((0, -hand, 0)))
    return gy


def oerlikon_platform(yard, hand, xs, z, foot):
    """A row of 20 mm guns on a platform built out at a gallery's edge, its outer edge following the flight deck's edge
    over it: each gun in the curve of its shield far enough out that its gunner stands clear of the deck's edge, every
    other with a loader, a rail round the platform's outer edge, braces under it from `foot(x)` (the ship's side, or the
    flight deck's underside where her side is open)."""
    m = yard.on(sum(xs) / len(xs))
    out = 2.9
    x0, x1 = min(xs) - 1.6, max(xs) + 1.6
    n = 8 if m.fine else 3
    ss = [lerp(x0, x1, k / n) for k in range(n + 1)]

    def e(x):
        return abs(edge(hand, x))
    plan = [(x, hand * (e(x) - 0.25)) for x in ss]
    plan += [(x1, hand * (e(x1) + out - 0.5)), (x1 - 0.5, hand * (e(x1 - 0.5) + out))]
    plan += [(x, hand * (e(x) + out)) for x in reversed(ss[1:-1])]
    plan += [(x0 + 0.5, hand * (e(x0 + 0.5) + out)), (x0, hand * (e(x0) + out - 0.5))]
    cap.slab(m, ccw(plan), z - 0.25, z, paint="upper", top="deck", bottom=True)
    if m.fine:
        from ships import railing
        railing(m, [(x, hand * (e(x) + out - 0.08), z) for x in [x0 + 0.5] + ss[1:-1] + [x1 - 0.5]], high=1.0, every=1.5)
        nb = int((x1 - x0) / 3.2)
        for k in range(nb + 1):
            xb = x0 + 0.5 + k * (x1 - x0 - 1.0) / max(1, nb)
            m.rod(foot(xb), (xb, hand * (e(xb) + out - 0.3), z - 0.22), 0.07, "upper", sides=4)
    for i, gx in enumerate(xs):
        fit.oerlikon(yard, gx, hand * (e(gx) + 1.9), z, facing=hand * 84.0, loader=i % 2 == 0)


def boat_on_davits(yard, x, y, z, long):
    """A boat hanging in her davits over the side: two quadrant davits from the walkway, the falls to her ends."""
    yard.boat(x, y, z, long)
    m = yard.on(x)
    hand = 1.0 if y > 0 else -1.0
    for t in (-0.32, 0.32):
        xd = x + t * long
        foot = Vector((xd, y - hand * 1.9, z - 0.6))
        top = Vector((xd, y - hand * 1.2, z + long * 0.2 + 1.0))
        arm = Vector((xd, y, z + long * 0.2 + 1.05))
        m.rod(foot, top, 0.11, "upper", sides=6)
        m.rod(top, arm, 0.09, "upper", sides=6)
        yard.wire(arm, Vector((xd, y, z + long * 0.11 * 1.1)), 0.025)


def jib_crane(yard, x, y, z, high=5.6, reach=11.5, toward=185.0, lift=34.0):
    """The aircraft crane at the flight deck's edge abaft the island: a round post with the sheave housing on its head,
    a lattice jib on a heel pin low on the post, topped up out over the side astern of the island (`toward`: the way it
    points, degrees anticlockwise from ahead seen from above), the topping lift from the post's head to the jib's, the
    fall and its hook block."""
    m = yard.on(x)
    fine = m.fine
    a = math.radians(toward)
    f = Vector((math.cos(a), math.sin(a), 0.0))
    s = Z.cross(f)
    m.lathe((x, y, z), Z, [(0.0, 0.55), (0.12, 0.55), (0.3, 0.42), (high, 0.32), (high + 0.06, 0.36), (high + 0.18, 0.36)], "upper", 14 if fine else 6)
    m.box(Vector((x, y, z + high + 0.42)) + f * 0.2, (0.8, 0.4, 0.5), "upper", yaw=toward)
    foot = Vector((x, y, z + 1.45)) + f * 0.45
    m.box(foot - f * 0.12, (0.42, 0.95, 0.42), "upper", yaw=toward)
    e = math.radians(lift)
    head = foot + (f * math.cos(e) + Z * math.sin(e)) * reach
    w0, w1 = 0.42, 0.18
    for sy in (-1, 1):
        for sz in (-1, 1):
            m.rod(foot + s * sy * w0 + Z * sz * w0 * 0.7, head + s * sy * w1 + Z * sz * w1 * 0.7, 0.06, "upper", sides=4)
    if fine:
        steps = 12
        for i in range(steps):
            t0, t1 = i / steps, (i + 1) / steps
            for sy in (-1, 1):                          # the lacing up each side, and across the top and bottom
                p0 = foot + (head - foot) * t0 + s * sy * lerp(w0, w1, t0) + Z * lerp(w0, w1, t0) * 0.7 * (1 if i % 2 else -1)
                p1 = foot + (head - foot) * t1 + s * sy * lerp(w0, w1, t1) + Z * lerp(w0, w1, t1) * 0.7 * (-1 if i % 2 else 1)
                m.rod(p0, p1, 0.03, "upper", sides=3, smooth=False)
            for sz in (-1, 1):
                p0 = foot + (head - foot) * t0 + Z * sz * lerp(w0, w1, t0) * 0.7 + s * lerp(w0, w1, t0) * (1 if i % 2 else -1)
                p1 = foot + (head - foot) * t1 + Z * sz * lerp(w0, w1, t1) * 0.7 + s * lerp(w0, w1, t1) * (-1 if i % 2 else 1)
                m.rod(p0, p1, 0.03, "upper", sides=3, smooth=False)
    m.lathe(head - s * 0.17, s, [(0.0, 0.16), (0.34, 0.16)], "steel", 10 if fine else 5)      # the sheave at its head
    for sy in (-1, 1):
        yard.wire(Vector((x, y, z + high + 0.6)) + s * sy * 0.12, head + s * sy * 0.15 + Z * 0.12, 0.028)
    low = Vector((head.x, head.y, z + 2.3))
    yard.wire(head - Z * 0.2, low + Z * 0.45, 0.025)
    m.box(low + Z * 0.25, (0.3, 0.22, 0.42), "steel")
    m.lathe(low + Z * 0.04, Z, [(0.0, 0.05), (0.2, 0.05)], "steel", 6)
    m.rod(low + Z * 0.04, low + Z * 0.04 + f * 0.12 - Z * 0.12, 0.03, "steel", sides=4)


def radio_mast(yard, x_heel, long, y, z, hand, way=-1.0):
    """A hinged radio mast lowered flat along the ship's side outboard of the gallery: its heel in a hinge on a bracket
    from the gallery's edge, the mast laid fore and aft (`way` -1 its head aft) and resting in a crutch near its head."""
    m = yard.on(x_heel + way * long * 0.5)
    fine = m.fine
    a = Vector((x_heel, y, z))
    b = a + X * way * long
    m.lathe(a, b - a, [(0.0, 0.17), (0.35, 0.17), (0.4, 0.15), (long * 0.6, 0.11), (long, 0.06)], "upper", 8 if fine else 4)
    m.box(a + X * way * 0.2, (0.55, 0.44, 0.44), "steel")
    for t in (0.0, 0.72):
        p = a + X * way * (long * t + (0.2 if t == 0.0 else 0.0))
        g = Vector((p.x, hand * (abs(edge(hand, p.x)) + 1.2), ZG - 0.06))
        m.box((g + p) * 0.5 - Z * 0.15, (0.24, abs(p.y - g.y) + 0.24, 0.12), "upper")
        m.rod(g - Z * 0.1, p - Z * 0.17, 0.05, "upper", sides=4)
        if fine and t > 0:
            for sy in (-1, 1):
                m.rod(p - Z * 0.2 + Y * sy * 0.2, p + Z * 0.1 + Y * sy * 0.2, 0.03, "steel", sides=4)


def facet_plan(xf, xa, ym, hw, c, r=0.8, steps=3):
    """A bridge house's plan with a three-faceted front: a flat middle `c` either side of `ym`, a cheek either side
    turned back 30 degrees out to the full half width `hw`, rounded after corners. Anticlockwise from the middle face's
    starboard end. Returns the plan and how far the cheeks reach back."""
    d = (hw - c) * math.tan(math.radians(30.0))
    pts = [(xf, ym - c), (xf, ym + c), (xf - d, ym + hw)]
    for q, (cx, cy) in enumerate(((xa + r, ym + hw - r), (xa + r, ym - hw + r))):
        for k in range(steps + 1):
            a = math.pi * 0.5 * (1 + q + k / steps)
            pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    pts.append((xf - d, ym - hw))
    return pts, d


def facet_run(xf, ym, hw, c, d, back, inset=0.0):
    """The run of a faceted front, anticlockwise: back along the starboard side, the cheek, the middle face, the other
    cheek, back along the port side (inset that far into the house)."""
    k = math.tan(math.radians(15.0)) * inset
    return [(xf - d - back, ym - hw + inset), (xf - d - k, ym - hw + inset), (xf - inset, ym - c + k), (xf - inset, ym + c - k),
            (xf - d - k, ym + hw - inset), (xf - d - back, ym + hw - inset)]


def facet_windows(m, run, z, high, pane_w=0.95):
    """A band of windows along a run of plan points (anticlockwise round a house): panes on every face it passes."""
    for a, b in zip(run, run[1:]):
        a, b = Vector((a[0], a[1], 0.0)), Vector((b[0], b[1], 0.0))
        d = b - a
        L = d.length
        if L < 0.3:
            continue
        d /= L
        nrm = Vector((d.y, -d.x, 0.0))
        k_n = max(1, int(L / pane_w)) if m.fine else 1
        pane = L / k_n
        yaw = math.degrees(math.atan2(nrm.y, nrm.x))
        for k in range(k_n):
            c = a + d * (k + 0.5) * pane + nrm * 0.03
            m.box((c.x, c.y, z + high * 0.5), (0.06, pane - (0.16 if m.fine else 0.06), high), "glass", yaw=yaw)


def round_down(m, x0, x1, top, drop, thick):
    """The flight deck's after end bent down into its round-down: flat at x1, falling `drop` by its end at x0."""
    xs = sorted(set([x1] + [lerp(x1, x0, k / (7 if m.fine else 2)) for k in range(1, 8 if m.fine else 3)] + [x for x, _ in YT_PORT if x0 < x < x1]), reverse=True)
    rows = []
    for x in xs:
        t = (x1 - x) / (x1 - x0)
        zt = top - drop * t * t
        yp, ys = edge(PORT, x), edge(STBD, x)
        rows.append([Vector((x, yp, zt)), Vector((x, ys, zt)), Vector((x, ys, zt - thick)), Vector((x, yp, zt - thick))])
    n = len(rows)

    def sheet(a, b, paint, smooth, way):          # (a strip between two of the four lines along it)
        vs = [p for r in rows for p in (r[a], r[b])]
        m.add(vs, [(2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2) for i in range(n - 1)], paint, smooth, lambda q: way)
    sheet(0, 1, "flight", True, Z)
    sheet(3, 2, "upper", False, -Z)
    sheet(0, 3, "upper", False, Y)
    sheet(1, 2, "upper", False, -Y)
    m.add(rows[-1], [(0, 1, 2, 3)], "upper", False, lambda q: -X)


def hangar_bay(yard, hand, xa, xb, z0, yin, yout, fd, what=None):
    """A bay of the hangar with its roller curtain rolled up: the hangar deck going in, deep beams across under the
    flight deck, the girder over the opening with the curtain rolled on its drum, a grey bulkhead some way in with the
    dark of the hangar beyond it, and what stands there (`what`: "tractor", "bombs", "hands")."""
    m = yard.on((xa + xb) * 0.5)
    xc, L = (xa + xb) * 0.5, xb - xa
    hz = fd - z0
    m.box((xc, hand * (yin + 0.03), z0 + hz * 0.5), (L - 0.1, 0.05, hz - 0.04), "door")
    m.box((xc - L * 0.08, hand * (yin + 0.06), z0 + hz * 0.42), (L * 0.52, 0.03, hz * 0.8), "black")
    if not m.fine:
        return
    for k in range(3):
        bx = xa + L * (k + 0.5) / 3.0
        m.box((bx, hand * (yin + yout) * 0.5, fd - 0.32), (0.24, yout - yin, 0.64), "upper")
        m.box((bx, hand * (yin + yout) * 0.5, fd - 0.66), (0.4, yout - yin, 0.05), "upper")
        m.box((bx + 0.6, hand * (yin + 1.4), fd - 0.1), (0.3, 0.3, 0.14), "glass")
    m.box((xc, hand * (yout - 0.22), fd - 0.4), (L - 0.06, 0.3, 0.8), "upper")
    m.rod((xa + 0.25, hand * (yout - 0.18), fd - 1.06), (xb - 0.25, hand * (yout - 0.18), fd - 1.06), 0.27, "door", sides=12)
    for yy in (yin + 1.3, yin + 2.9):
        m.box((xc, hand * yy, z0 + 0.012), (L - 0.12, 0.12, 0.024), "steel")
    if what == "tractor":                     # a tow tractor, its towbar down
        c = Vector((xc - 0.6, hand * (yin + 2.0), z0))
        m.box(c + Z * 0.62, (2.3, 1.3, 0.62), "door")
        m.box(c + Vector((-0.5, 0, 1.08)), (0.5, 0.5, 0.3), "gunmetal")
        m.rod(c + Vector((0.3, 0, 0.93)), c + Vector((0.15, 0, 1.35)), 0.03, "gunmetal", sides=4)
        m.lathe(c + Vector((0.15, 0, 1.35)), Vector((-0.3, 0, 1.0)), [(0.0, 0.2), (0.03, 0.2)], "gunmetal", 10)
        for dx in (-0.75, 0.75):
            for sy in (-1, 1):
                m.lathe(c + Vector((dx, sy * 0.55, 0.32)), Y * sy, [(0.0, 0.32), (0.24, 0.32)], "black", 12)
        m.rod(c + Vector((1.15, 0, 0.4)), c + Vector((2.6, 0, 0.08)), 0.05, "steel", sides=5)
        fit.sailor(m, c + Vector((-0.5, 0, 0.93)) + X * 0.06, X, "sit", seat=0.3, hands=(c + Vector((0.15, 0.17, 1.34)), c + Vector((0.15, -0.17, 1.34))), navy="USA")
    elif what == "bombs":                     # a bomb cart with two 500 lb bombs, a man at its handle
        c = Vector((xc + 0.4, hand * (yin + 2.2), z0))
        m.box(c + Z * 0.38, (1.9, 0.9, 0.1), "steel")
        for dx in (-0.7, 0.7):
            for sy in (-1, 1):
                m.lathe(c + Vector((dx, sy * 0.48, 0.17)), Y * sy, [(0.0, 0.17), (0.08, 0.17)], "black", 8)
        for sy in (-0.22, 0.22):
            b0 = c + Vector((-0.8, sy, 0.62))
            m.lathe(b0, X, [(0.0, 0.03), (0.12, 0.14), (0.4, 0.19), (1.2, 0.19), (1.45, 0.13), (1.6, 0.0)], "gunmetal", 10)
            for fy, fz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                m.box(b0 + Vector((0.08, sy * 0.0 + fy * 0.12, fz * 0.12)), (0.3, 0.02 if fy == 0 else 0.22, 0.22 if fy == 0 else 0.02), "gunmetal")
        m.rod(c + Vector((0.95, 0, 0.4)), c + Vector((1.7, 0, 0.95)), 0.03, "steel", sides=4)
        fit.sailor(m, c + Vector((2.05, 0.0, 0.0)), -X, "aim", hands=(c + Vector((1.7, -0.12, 0.95)), c + Vector((1.7, 0.12, 0.95))), navy="USA", lean=0.1)
    elif what == "hands":                     # two hands at a hose reel, fuel drums stood by the bulkhead
        c = Vector((xc, hand * (yin + 1.6), z0))
        m.lathe(c + Vector((0.0, -0.45 * hand, 0.5)), Y, [(0.0, 0.5), (0.05, 0.5), (0.06, 0.25), (0.84, 0.25), (0.85, 0.5), (0.9, 0.5)], "upper", 14)
        for dx in (-1.6, -1.0):
            m.lathe(c + Vector((dx, hand * -0.6, 0.0)), Z, [(0.0, 0.29), (0.88, 0.29)], "steel", 12)
        fit.sailor(m, c + Vector((1.1, hand * 0.6, 0.0)), Vector((-1.0, -0.5 * hand, 0.0)), "stand", navy="USA")
        fit.sailor(m, c + Vector((-0.4, hand * 1.1, 0.0)), Vector((0.4, -hand, 0.0)), "carry", navy="USA")


# ---- the ship -------------------------------------------------------------------------------------------------------

def yorktown(fine=True):
    """USS Yorktown (CV-5) as she was at Midway, 4 June 1942 (her sister Enterprise much the same). See the module's
    head for what she was and where it comes from."""
    paints = livery("USA", "carrier")
    paints.update(YT_PAINT)
    paints = fit.crew_paints(paints, "USA")
    m = Model("Yorktown", "Aircraft carrier USS Yorktown (CV-5), Midway, June 1942 (our own model)", "ship", paints, fine)
    h = cap.PlanHull(YT_PLAN, L=246.6, B=26.45, D=7.0, T=7.9, sheer=1.1, sheer_aft=-0.5, rake=7.5, counter=4.0, flare=0.9, fine=1.6, full=0.72,
                     wl_fore=0.5, bilge=6.0, step_at=67.0, step=2.6)
    bx = -38.0                                # (where she breaks: abaft the island and the midships lift)
    yard = Yard(m, h, break_x=bx)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("deck", line=7.05)              # (sea blue to the hangar deck; the forecastle's sides above it ocean grey)
    D = h.deck_z
    iz, fd, zg = IZ, FD, ZG
    hz = iz - D(0.0)                          # how far a lift goes down to the hangar deck
    from ships import ladder, railing

    # ---- the hangar's sides from her main deck to the flight deck: roller curtains at her side amidships (one bay in
    # four rolled up, the hangar showing), her boats' bays aft with the curtains drawn in behind them, shops and the
    # uptakes' casing to starboard, the forecastle's rooms forward; closed across at frame 15 forward and over the
    # after lift's trunk aft
    hx0, hx1 = 100.0, -112.0
    boat_y, open_y = 9.45, 8.7                # how far in the side stands in a boats' bay, in a curtain bay rolled up

    def side_bays(hand):
        """(x0, x1, kind) along one side: curtain, closed, uptake, forecastle, boats."""
        if hand == PORT:
            return [(-112.0, -92.0, "closed"), (-92.0, -44.0, "boats"), (-44.0, 67.0, "curtain"), (67.0, 100.0, "forecastle")]
        return [(-112.0, -92.0, "closed"), (-92.0, -54.0, "boats"), (-54.0, -22.0, "closed"), (-22.0, 35.0, "uptake"), (35.0, 67.0, "curtain"),
                (67.0, 100.0, "forecastle")]

    def lengths(hand):
        """Each length of one side between its web frames: (x0, x1, kind, its number in its stretch)."""
        out = []
        for a, b, kind in side_bays(hand):
            n_bay = 1 if kind == "boats" else max(1, round((b - a) / 8.6))
            step = (b - a) / n_bay
            for i in range(n_bay):
                k = "open" if kind == "curtain" and (i + (2 if hand == STBD else 0)) % 4 == 1 else kind
                out.append((a + i * step, a + (i + 1) * step, k, i))
        return out
    bays = {hand: [(a, b, k) for a, b, k, _ in lengths(hand) if k in ("boats", "open")] for hand in (PORT, STBD)}

    def wall_edge(hand):
        xs = sorted(set([hx1, hx0] + [lerp(hx1, hx0, k / 40.0) for k in range(41)] + [q for a_, b_, _ in bays[hand] for q in (a_ - 0.05, a_ + 0.05, b_ - 0.05, b_ + 0.05)]))
        out = []
        for x in xs:
            inside = [k_ for a_, b_, k_ in bays[hand] if a_ < x < b_]
            out.append((x, hand * ((boat_y if inside[0] == "boats" else open_y) if inside else h.half_at(x) * 0.985)))
        return out
    wport, wstar = wall_edge(PORT), wall_edge(STBD)
    for x0, x1, part in ((bx, hx0, "fore"), (hx1, bx, "aft")):
        m.use(part)
        plan = cap.cut_deck(wport, wstar, x0, x1, every=2.5)
        cap.slab(m, plan, D(x0 if part == "fore" else x1) - 0.5, fd, paint="upper", top=None)

    def brace_foot(hand, x, z_top):
        """Where a brace under a gallery or a platform at z_top starts: on the hangar's side under it, on the
        forecastle's side at its deck forward of the hangar, or, where her side is open (a boats' bay, a curtain
        rolled up), a knee from the flight deck's underside."""
        if hx1 < x < hx0 and not any(a_ - 0.4 < x < b_ + 0.4 for a_, b_, _ in bays[hand]):
            return Vector((x, hand * h.half_at(x) * 0.985, z_top - 1.9))
        if x >= hx0:
            return Vector((x, hand * h.half_at(x) * 0.97, D(x) - 0.05))
        return Vector((x, hand * (abs(edge(hand, x)) - 1.2), fd))

    # the boats in their bays aft on chocks on the hangar deck, the pillars at the ship's side between them
    boats = {PORT: ((-85.0, 9.0), (-72.0, 11.0), (-58.5, 11.0), (-48.5, 7.0)), STBD: ((-85.5, 9.0), (-73.0, 11.0), (-60.5, 9.0))}
    pillars = {PORT: (-91.6, -79.0, -65.25, -52.5), STBD: (-91.6, -79.75, -66.75)}
    to_hang = {"tractor": (PORT, 1), "bombs": (PORT, 2), "hands": (PORT, 0)}
    for hand in (PORT, STBD):
        opened = 0
        for xa, xb, kind, i in lengths(hand):
            xc = (xa + xb) * 0.5
            m.use(yard.half(xc))
            z0 = D(xc)
            if kind == "boats":
                # the curtain drawn across behind the boats, pillars at the ship's side holding up the deck
                cuts = [xa] + [p for p in pillars[hand] if p > xa + 0.5] + [xb]
                wy = hand * (boat_y + 0.03)
                for ca, cb in zip(cuts, cuts[1:]):
                    cc = (ca + cb) * 0.5
                    m.use(yard.half(cc))
                    m.box((cc, wy, z0 + (fd - z0) * 0.5 - 0.2), (cb - ca - 0.4, 0.06, fd - z0 - 1.0), "door")
                    if fine:
                        for j in range(1, 8):
                            m.box((cc, wy + hand * 0.02, z0 + 0.3 + j * (fd - z0 - 1.2) / 8.0), (cb - ca - 0.5, 0.03, 0.05), "seam")
                for px in pillars[hand]:
                    m.use(yard.half(px))
                    py = hand * (h.half_at(px) * 0.985 - 0.3)
                    m.rod((px, py, D(px)), (px, py, fd), 0.24, "upper", sides=8 if fine else 5)
                continue
            wy = hand * (h.half_at(xc) * 0.985 + 0.03)
            if kind == "open":
                what = next((w_ for w_, (hd, j) in to_hang.items() if hd == hand and j == opened), None)
                hangar_bay(yard, hand, xa, xb, z0, open_y, h.half_at(xc) * 0.985, fd, what)
                opened += 1
            elif kind == "curtain":
                m.box((xc, wy, z0 + (fd - z0) * 0.5 - 0.25), (xb - xa - 0.7, 0.06, fd - z0 - 1.1), "door")
                if fine:
                    for j in range(1, 8):
                        m.box((xc, wy + hand * 0.02, z0 + 0.3 + j * (fd - z0 - 1.2) / 8.0), (xb - xa - 0.8, 0.03, 0.05), "seam")
            elif fine:
                # closed walls: scuttles in a row, a door now and then, louvres on the uptakes' casing
                step = xb - xa
                out = Vector((0, hand, 0))
                if kind == "uptake":
                    for j in range(3):
                        m.box((xc - step * 0.3 + j * step * 0.3, wy + hand * 0.02, z0 + 3.6), (1.6, 0.05, 1.0), "black")
                        for r in range(4):
                            m.box((xc - step * 0.3 + j * step * 0.3, wy + hand * 0.05, z0 + 3.2 + r * 0.25), (1.6, 0.04, 0.05), "upper")
                else:
                    for j in range(4):
                        px = xa + (j + 0.5) * step / 4.0
                        zz = max(z0, D(px)) + (2.2 if kind == "forecastle" else 3.3)
                        if zz < fd - 0.6:
                            m.disc((px, hand * (h.half_at(px) * 0.985 + 0.05), zz), out, 0.17, "glass", 8)
                    if i % 2 == 0 and kind == "closed":
                        m.box((xa + step * 0.62, hand * (h.half_at(xa + step * 0.62) * 0.985 + 0.05), z0 + 1.0), (0.78, 0.05, 1.9), "door")
            # the web frame between bays, standing proud of the wall
            if fine and i > 0:
                m.use(yard.half(xa))
                m.box((xa, hand * (h.half_at(xa) * 0.985 + 0.12), (z0 + fd) * 0.5), (0.36, 0.24, fd - z0), "upper")
        if fine:      # (and one at the after end of each open bay that ends a stretch)
            for xa, xb, kind, i in lengths(hand):
                if kind == "open" and not any(abs(xb - a2) < 0.01 and i2 > 0 for a2, _, _, i2 in lengths(hand)):
                    m.use(yard.half(xb)).box((xb, hand * (h.half_at(xb) * 0.985 + 0.12), (D(xb) + fd) * 0.5), (0.36, 0.24, fd - D(xb)), "upper")
    for hand, row in boats.items():
        for bxx, bl in row:
            yy = hand * 11.3
            yard.boat(bxx, yy, D(bxx) + 0.45, bl)
            if fine:
                for t in (-0.3, 0.3):
                    m.box((bxx + t * bl, yy, D(bxx) + 0.22), (0.35, bl * 0.24, 0.44), "upper")

    # ---- the flight deck, its round-down aft, its supports over the open bow and stern
    for x0, x1, part in ((bx, FD_FORE, "fore"), (RD_X, bx, "aft")):
        m.use(part)
        cap.slab(m, cap.cut_deck(YT_PORT, YT_STAR, x0, x1, every=4.0), fd, iz, paint="upper", top="flight", bottom=True)
    m.use("aft")
    round_down(m, FD_AFT, RD_X, iz, 0.42, iz - fd)
    for px in (102.0, 108.5, 113.6):          # pillars from the forecastle deck
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.55, 5.0)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.15, fd), 0.28, "upper", sides=8 if fine else 5)
    yard.on(115.4).box((115.6, 0.0, fd - 0.35), (0.6, 17.0, 0.7), "upper")        # the girder across under its fore end
    for px in (-114.5, -119.0, -122.5):        # and from the quarterdeck
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.6, 6.0)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.25, fd), 0.3, "upper", sides=8 if fine else 5)
    yard.on(-112.5).box((-112.6, 0.0, fd - 0.35), (0.6, 22.0, 0.7), "upper")
    m.use("aft")                               # the girder under the overhang beyond her stern, the knees up to it
    m.box((-125.8, 0.0, fd - 0.42), (0.5, 21.0, 0.6), "upper")
    for hand in (PORT, STBD):
        m.rod((-121.2, hand * 2.6, D(-121.2)), (-125.8, hand * 6.5, fd - 0.7), 0.24, "upper", sides=8 if fine else 5)
    # the stern's houses at the forecastle deck's level: the propeller shop and stores over the quarterdeck, a platform
    st = Model.round_rect(-112.0, -117.2, -5.0, 5.0, 0.6, 2 if fine else 1)
    cap.slab(m, st, D(-114.0) - 0.2, D(-114.0) + 2.7, paint="upper", top="deck")
    cap.walls_detail(yard, st, D(-114.0), D(-114.0) + 2.7, doors=True, scuttles=True)
    for hand in (PORT, STBD):
        yard.boat(-119.5, hand * 3.4, D(-119.5) + 0.4, 6.0)

    # ---- the galleries round her under the deck's edge, on brackets, railed (broken by the gun platforms and the
    # island's walkway); the 20 mm on their platforms along them; the hinged radio masts lowered flat outboard of them
    gal = {PORT: [(-91.0, 67.5), (90.5, 109.5)], STBD: [(-91.0, -34.0), (44.5, 67.5), (90.5, 109.5)]}
    oer = {PORT: [(104.0, 99.0, 94.0), (60.0, 55.0, 50.0), (-44.0, -49.0, -54.0, -59.0), (-79.0, -84.0)],
           STBD: [(104.0, 99.0, 94.0), (61.0, 56.0, 51.0), (-44.0, -49.0, -54.0, -59.0), (-79.0, -84.0)]}
    for hand in (PORT, STBD):
        busy = [(min(g) - 1.8, max(g) + 1.8) for g in oer[hand]]
        for a, b in gal[hand]:
            for p0, p1 in ((a, bx), (bx, b)) if a < bx < b else ((a, b),):
                pts = [lerp(p0, p1, k / max(2, int((p1 - p0) / 5.0))) for k in range(max(2, int((p1 - p0) / 5.0)) + 1)]
                m.use(yard.half((p0 + p1) * 0.5))
                strip = [Vector((x, hand * (abs(edge(hand, x)) + 0.55), zg)) for x in pts]
                m.strip(strip, [Z] * len(strip), 0.72, "deck", 0.1)
                if fine:
                    run = []
                    for x in [lerp(p0, p1, k / max(1, int((p1 - p0) / 1.2))) for k in range(max(1, int((p1 - p0) / 1.2)) + 1)]:
                        if any(lo_ <= x <= hi_ for lo_, hi_ in busy):
                            if len(run) > 1:
                                railing(m, run, high=1.0, every=2.4)
                            run = []
                        else:
                            run.append(Vector((x, hand * (abs(edge(hand, x)) + 1.22), zg)))
                    if len(run) > 1:
                        railing(m, run, high=1.0, every=2.4)
                    for x in pts:
                        m.rod(brace_foot(hand, x, zg), (x, hand * (abs(edge(hand, x)) + 1.1), zg - 0.08), 0.07, "upper", sides=4)
    for hand in (PORT, STBD):
        for xs in oer[hand]:
            oerlikon_platform(yard, hand, list(xs), iz - 1.25, lambda xb_, hd=hand: brace_foot(hd, xb_, iz - 1.25))
    masts = {PORT: (45.0, 28.0, 11.0, -6.0), STBD: (-62.0,)}
    for hand, heels in masts.items():
        for xh in heels:
            radio_mast(yard, xh, 13.5, hand * (abs(edge(hand, xh)) + 1.7), zg + 0.42, hand)

    # ---- the flight deck's fittings: the centre line, the steel tie-down strips across it, the plank seams along it,
    # the catapults forward, the arresting wires aft and forward, the barriers
    lifts = [(78.8, 0.0, 13.4, 14.6, "flight"), (-28.4, 0.0, 13.4, 14.6, "flight"), (-101.0, 0.0, 13.4, 14.6, "flight")]

    def clear(x, pad):
        return not any(abs(x - lx) < ll * 0.5 + pad for lx, _, ll, _, _ in lifts) and abs(x - bx) > 0.6
    x = FD_FORE - 2.9
    while x > RD_X + 1.5:                 # the dashed centre line
        if clear(x, 3.0):
            yard.on(x).box((x, 0, iz + 0.02), (4.0, 0.3, 0.03), "line")
        x -= 9.0
    if fine:
        x = FD_FORE - 2.1
        while x > RD_X + 0.4:             # the tie-down strips, every 3.7 m
            if clear(x, 0.8):
                w_ = min(abs(edge(PORT, x)), abs(edge(STBD, x))) - 0.7
                yard.on(x).box((x, 0.0, iz + 0.012), (0.16, 2.0 * w_, 0.024), "steel")
            x -= 3.7
        # the seams between her planks, laid fore and aft (every third one: the planks are about 10 cm wide), kept off
        # the lifts and from under the island
        zs = iz + 0.004
        for j in range(-43, 44):
            yy = j * 0.3
            xs_in = [lerp(RD_X, FD_FORE, k / 400.0) for k in range(401)]
            inside = [x for x in xs_in if min(abs(edge(PORT, x)), abs(edge(STBD, x))) - 0.3 > abs(yy)]
            if len(inside) < 2:
                continue
            cuts = [(inside[0], inside[-1])]
            holes = [(lx - ll * 0.5 - 0.15, lx + ll * 0.5 + 0.15) for lx, _, ll, lw, _ in lifts if abs(yy) < lw * 0.5 + 0.15] + [(bx - 0.05, bx + 0.05)]
            if yy < -10.1:
                holes.append((-26.3, 35.3))
            for lo_, hi_ in holes:
                cuts = [piece for a_, b_ in cuts for piece in ((a_, min(b_, lo_)), (max(a_, hi_), b_)) if piece[1] - piece[0] > 0.5]
            for a_, b_ in cuts:
                m.use(yard.half((a_ + b_) * 0.5))
                m.add([(a_, yy - 0.012, zs), (b_, yy - 0.012, zs), (b_, yy + 0.012, zs), (a_, yy + 0.012, zs)], [(0, 1, 2, 3)], "seam", False, lambda q: Z)
    m.use("fore")
    for cy in (3.4, -3.4):                # the two H-2 catapults' tracks forward of the fore lift
        m.box((100.2, cy, iz + 0.03), (25.0, 0.45, 0.05), "steel")
        m.box((112.9, cy, iz + 0.06), (1.4, 1.0, 0.12), "steel")
        if fine:
            m.box((88.5, cy, iz + 0.07), (0.9, 0.9, 0.14), "steel")
    wires = [(-48.0, "aft"), (-53.5, "aft"), (-59.0, "aft"), (-64.5, "aft"), (-70.0, "aft"), (-75.5, "aft"), (-81.0, "aft"), (-86.5, "aft"),
             (-113.0, "aft"), (49.0, "fore"), (54.0, "fore"), (59.0, "fore"), (64.0, "fore")]
    for xw, part in wires:
        m.use(part)
        w_ = min(abs(edge(PORT, xw)), abs(edge(STBD, xw)), 13.1) - 2.0
        m.rod((xw, -w_, iz + 0.12), (xw, w_, iz + 0.12), 0.035, "steel", sides=3, smooth=False)
        if fine:
            for hand in (PORT, STBD):
                m.lathe((xw, hand * w_, iz), Z, [(0.0, 0.22), (0.18, 0.18)], "steel", 8)
            for t in (-0.72, -0.26, 0.26, 0.72):                   # the bow springs holding the wire up off the deck
                m.box((xw, t * w_, iz + 0.045), (0.14, 0.55, 0.09), "steel")
    for xb_, part in ((-42.5, "aft"), (-46.5, "aft"), (44.0, "fore")):   # the barriers, lowered flat
        m.use(part)
        m.rod((xb_, -10.6, iz + 0.18), (xb_, 10.6, iz + 0.18), 0.05, "steel", sides=4, smooth=False)
        for hand in (PORT, STBD):         # each stanchion folded down flat into its well, its hinge at the foot
            m.box((xb_ + 0.75, hand * 11.0, iz + 0.1), (1.6, 0.22, 0.2), "upper")
            m.box((xb_, hand * 11.0, iz + 0.14), (0.3, 0.5, 0.28), "steel")

    # ---- the 5-inch: two at each corner of her hangar on their platforms, in open mountings without shields
    n_p = n_s = 0
    for (x0, x1, gxs, zp) in ((68.8, 89.8, (84.5, 73.0), D(78.0) + 0.5), (-109.6, -90.6, (-96.0, -104.0), D(-100.0) + 3.7)):
        for hand in (PORT, STBD):
            gy = gun_platform(yard, x0, x1, hand, zp, 16.65, gxs)
            for gx in gxs:
                if hand == PORT:
                    n_p += 1
                    nm = f"sec.P{n_p}"
                else:
                    n_s += 1
                    nm = f"sec.S{n_s}"
                bearing = 0.0 if gx > 0 else 180.0
                lim = (-165, 20) if (hand == PORT) == (gx > 0) else (-20, 165)
                open_5in_38(yard, nm, gx, gy, zp, bearing, lim, rest_elev=10.0)

    # ---- the island: its walkway and boats outboard under the deck's built-out edge; three levels to the bridge, the
    # tripod with the sky control top and the CXAM over it, the long funnel with the pole mast at its after end where
    # the ensign flies, a Mark 33 at each end, the 1.1-inch either end of it, Primary Flight Control on its port side
    m.use("fore")
    for xb_ in (-30.0, -22.0, -14.0, -6.0, 2.0, 10.0, 18.0, 26.0, 34.0, 41.0):      # the brackets under the built-out deck
        yard.on(xb_).rod((xb_, -h.half_at(xb_) * 0.985, fd - 3.2), (xb_, -15.3, fd - 0.05), 0.16, "upper", sides=5)
    wz = D(0.0) + 2.6                                                                 # the boats' walkway, a deck up from the hangar deck
    m.use("fore")
    w_in = h.half_at(4.0) * 0.985
    m.box((4.0, -(w_in - 0.05 + 13.85) * 0.5, wz - 0.1), (56.0, 13.85 - w_in + 0.05, 0.2), "deck")
    if fine:
        railing(m, [(-24.0, -13.8, wz), (32.0, -13.8, wz)], high=1.0, every=2.0)
        for xb_ in (-22.0, -10.0, 2.0, 14.0, 26.0):
            m.rod((xb_, -w_in, wz - 1.4), (xb_, -13.75, wz - 0.12), 0.07, "upper", sides=4)
    for bxx, bl in ((-12.0, 9.2), (5.0, 9.2), (21.0, 8.0)):
        boat_on_davits(yard, bxx, -15.25, wz + 0.2, bl)

    m.use("fore")
    tz = iz
    levels = ((35.0, -26.0, -10.3, -15.3, 2.6), (30.4, -20.6, -10.5, -15.1, 2.6))
    rafts = (-17.0, -9.0, -1.0, 6.0)                                                   # life rafts on the second level's walls
    for k, (xf, xa, y0, y1, hh) in enumerate(levels):
        pl = Model.round_rect(xf, xa, y0, y1, 0.9, 3 if fine else 1)
        cap.slab(m, pl, tz, tz + hh)
        cap.walls_detail(yard, pl, tz, tz + hh, doors=True, scuttles=True, rail=k == 0, skip=[(r_ - 1.6, r_ + 1.6) for r_ in rafts] if k else ())
        if k:
            for r_ in rafts:
                yard.float_(r_, y0, tz + 1.3, (0, 1, 0), long=2.4, wide=1.1)
                yard.float_(r_, y1, tz + 1.3, (0, -1, 0), long=2.4, wide=1.1)
        tz += hh
    m.use("fore")
    z2 = iz + 5.2
    steps = 3 if fine else 1
    br, d_br = facet_plan(30.0, 11.0, YI, 2.4, 1.2, 0.9, steps)                      # the flag bridge, its faceted front glazed
    cap.slab(m, br, z2, z2 + 2.6)
    facet_windows(m, facet_run(30.0, YI, 2.4, 1.2, d_br, 4.0), z2 + 1.0, 0.9)
    z3 = z2 + 2.6
    nav, d_nav = facet_plan(28.4, 12.2, YI, 2.0, 1.0, 0.8, steps)                   # the navigating bridge and pilot house
    cap.slab(m, nav, z3, z3 + 2.4)
    facet_windows(m, facet_run(28.4, YI, 2.0, 1.0, d_nav, 3.2), z3 + 1.05, 0.95)
    z4 = z3 + 2.4
    for hand, y_out in ((PORT, -8.6), (STBD, -16.55)):                                 # its wings, on brackets
        y_in = -10.4 if hand == PORT else -15.2
        yw, wl = (y_out + y_in) * 0.5, abs(y_out - y_in)
        m.box((26.2, yw, z3 - 0.08), (4.0, wl, 0.16), "deck")
        if fine:
            railing(m, [(28.2, y_out, z3), (24.2, y_out, z3)], every=1.3)
            railing(m, [(28.2, y_in, z3), (28.2, y_out, z3)], every=1.3)
            for dx in (-1.4, 1.4):
                m.rod((26.2 + dx, y_in - hand * 0.1, z3 - 1.6), (26.2 + dx, y_out - hand * 0.3, z3 - 0.16), 0.06, "upper", sides=4)
            # the signal flag bag at the wing's after edge, a signal lamp at its outer end; the voice pipes by the house,
            # a binocular stand at the rail with the officer of the watch at it
            m.box((24.75, yw, z3 + 0.45), (0.9, wl - 0.5, 0.9), "upper")
            m.box((24.75, yw, z3 + 0.92), (0.96, wl - 0.44, 0.06), "canvas")
            yard.searchlight(27.6, y_out - hand * 0.42, z3, 0.28, (1.0, hand * 0.6, 0.0))
            for k, px in enumerate((27.65, 27.4, 27.15)):
                py = y_in + hand * 0.22
                m.rod((px, py, z3), (px, py, z3 + 1.1 + 0.06 * k), 0.035, "upper", sides=6)
                m.lathe((px, py, z3 + 1.1 + 0.06 * k), Vector((-0.4, 0.0, 1.0)), [(0.0, 0.035), (0.1, 0.07), (0.13, 0.085)], "brass", 8, caps=(False, False))
            sx, sy = 26.75, y_out - hand * 0.55
            m.rod((sx, sy, z3), (sx, sy, z3 + 1.25), 0.05, "upper", sides=6)
            m.box((sx, sy, z3 + 1.31), (0.16, 0.24, 0.12), "gunmetal")
            for s_ in (-1, 1):
                m.rod((sx - 0.05, sy + s_ * 0.06, z3 + 1.38), (sx + 0.18, sy + s_ * 0.06, z3 + 1.38), 0.035, "gunmetal", sides=6)
            fit.sailor(m, (sx - 0.62, sy, z3), (1.0, 0.0, 0.0), "aim", hands=((sx - 0.08, sy + 0.1, z3 + 1.28), (sx - 0.08, sy - 0.1, z3 + 1.28)), navy="USA", lean=0.18)
    mk33(yard, "director.fore", 22.0, YI, z4, 0.0, parent="fore")
    m.use("fore")
    if fine:
        # the open bridge on the pilot house's roof: a windscreen round its faceted front, canvas dodgers along its sides,
        # the rail round the rest; the captain and a lookout at the screen
        ctr = Vector((22.0, YI, 0.0))
        front = [Vector((px, py, z4)) for px, py in facet_run(28.4, YI, 2.0, 1.0, d_nav, 0.6, inset=0.1)]
        plate(m, front, 1.0, ctr, "upper", thick=0.05)
        plate(m, [p + Z * 1.0 for p in front[1:-1]], 0.32, ctr, "glass", thick=0.025, lip=False)
        for hand in (PORT, STBD):
            yy = YI + hand * (2.0 - 0.1)
            plate(m, [Vector((28.4 - d_nav - 0.6, yy, z4)), Vector((24.6, yy, z4))], 1.05, ctr, "canvas", thick=0.03)
            railing(m, [Vector((24.6, yy, z4)), Vector((13.4, yy, z4))], every=1.6)      # (the tripod's after legs stand at its end)
        railing(m, [Vector((12.4, YI + 1.2, z4)), Vector((12.4, YI - 1.2, z4))], every=1.2)
        officer(m, (26.9, YI + 0.7, z4), (1.0, 0.1))
        officer(m, (27.1, YI - 0.9, z4), (0.9, -0.5))
    # the tripod from the pilot house's roof to the sky control top
    m.use("fore")
    top_z = iz + 16.4
    head = Vector((14.8, YI, top_z))
    m.rod((16.6, YI, z4), head + Vector((0.4, 0, 0.1)), 0.34, "light", rb=0.28, sides=10 if fine else 5)
    for hand in (PORT, STBD):
        m.rod((12.8, YI + hand * 1.75, z4), head + Vector((-0.3, hand * 0.3, 0.1)), 0.28, "light", rb=0.24, sides=9 if fine else 5)
    if fine:
        for zz in (z4 + 2.2, z4 + 3.9):                                                # the platforms' ladders and stays
            m.rod((16.6 - (zz - z4) * 0.12, YI, zz), (12.8 + (zz - z4) * 0.14, YI + 1.75 - (zz - z4) * 0.3, zz), 0.05, "light", sides=4)
            m.rod((16.6 - (zz - z4) * 0.12, YI, zz), (12.8 + (zz - z4) * 0.14, YI - 1.75 + (zz - z4) * 0.3, zz), 0.05, "light", sides=4)
    pa = Model.round_rect(18.8, 10.8, YI - 3.6, YI + 3.6, 1.2, 3 if fine else 1)     # the searchlight and lookout platform
    m.prism(pa, top_z - 0.18, top_z, "light", top_paint="deck", bottom=True)
    if fine:     # its splinter plating round the edge, an opening aft where the ladder comes up
        q = 2 * (3 + 1)
        run = [Vector((10.8, YI - 0.5, top_z))] + [Vector((px, py, top_z)) for px, py in pa[q:] + pa[:q]] + [Vector((10.8, YI + 0.5, top_z))]
        plate(m, run, 1.05, Vector((14.8, YI, 0.0)), "light", thick=0.05)
    for hand in (PORT, STBD):
        yard.searchlight(17.3, YI + hand * 2.6, top_z, 0.6, (1.0, hand * 0.5, 0.05))
    m.use("fore")
    sky = Model.round_rect(16.4, 12.0, YI - 2.1, YI + 2.1, 0.7, 3 if fine else 1)     # the sky control station, glazed round
    cap.slab(m, sky, top_z, top_z + 2.2, paint="light", top="roof")
    if fine:
        yard.windows(16.4, 1.6, top_z + 1.05, 0.75, y=YI, back=2.6)
        railing(m, [Vector((px, py, top_z + 2.2)) for px, py in sky] + [Vector((*sky[0], top_z + 2.2))], every=1.4, paint="light")
    for gx_, gy_, face in ((18.2, YI + 3.0, 40.0), (18.2, YI - 3.0, -40.0), (11.4, YI + 3.0, 140.0), (11.4, YI - 3.0, -140.0)):
        fit.mg50(yard, gx_, gy_, top_z, facing=face, part="fore")
    m.use("fore")
    sky_top = top_z + 2.2
    cxam(yard, "radar.air", 14.2, YI, sky_top, "fore")
    m.use("fore")
    m.lathe((14.2, YI, sky_top), Z, [(0.0, 0.16), (8.0, 0.07)], "light", 7 if fine else 4)    # the topmast through the aerial's turntable
    yd = sky_top + 6.1
    m.rod((14.2, YI - 3.4, yd), (14.2, YI + 3.4, yd), 0.06, "light", sides=4)
    if fine:
        for hand in (PORT, STBD):
            yard.wire((14.2, YI + hand * 3.3, yd), (14.2, YI + hand * 3.3, yd - 1.6), 0.015)
            m.ball((14.2, YI + hand * 3.3, yd - 1.85), 0.2, "black", 8, 6)                  # the signal shapes hung from the yard
            m.lathe((14.6, YI + hand * 1.0, yd - 0.6), Z, [(0.0, 0.1), (0.35, 0.1)], "black", 6)   # the truck lights
        yard.wire((14.2, YI, sky_top + 7.8), (14.2, YI - 3.3, yd), 0.015)
        yard.wire((14.2, YI, sky_top + 7.8), (14.2, YI + 3.3, yd), 0.015)
    # the funnel: long and flat-sided, its after end over the island's after house
    yard.funnel(-2.0, z2, iz + 14.4 - z2, 18.0, 4.3, lean=0.0, cap=0.07, y=YI - 0.1, flat=True)
    m.use("fore")
    fh = iz + 14.4 - z2
    if fine:
        for hand in (PORT, STBD):                                                    # the stiffening bands round its walls
            for share in (0.3, 0.64):
                m.box((-2.0, YI - 0.1 + hand * 2.18, z2 + fh * share), (18.0 - 2.7, 0.07, 0.14), "upper")
        ladder(m, Vector((7.0, YI - 0.1, z2)), z2 + fh - 0.3, Vector((1, 0, 0)))
    for hand in (PORT, STBD):                                                        # the searchlight platforms on its sides
        yy = YI - 0.1 + hand * (2.15 + 0.8)
        zp = iz + 10.4
        m.box((3.2, yy, zp - 0.07), (3.6, 1.6, 0.14), "deck")
        for dx in (-1.3, 1.3):
            m.rod((3.2 + dx, YI - 0.1 + hand * 2.1, zp - 1.2), (3.2 + dx, yy + hand * 0.6, zp - 0.1), 0.06, "upper", sides=4)
        if fine:
            railing(m, [(5.0, yy + hand * 0.75, zp), (1.4, yy + hand * 0.75, zp)], every=1.2)
        yard.searchlight(3.7, yy, zp, 0.62, (0.4, hand * 0.9, 0.08))
        fit.mg50(yard, 1.75, yy + hand * 0.05, zp, facing=hand * 160.0, part="fore")
    # the after house, the aft director on it, the pole mast at the funnel's after end with the ensign's gaff
    m.use("fore")
    aft_h = Model.round_rect(-12.0, -20.2, -10.7, -14.9, 0.8, 3 if fine else 1)
    cap.slab(m, aft_h, z2, z2 + 2.6)
    cap.walls_detail(yard, aft_h, z2, z2 + 2.6, doors=True, scuttles=True, rail=True)
    mk33(yard, "director.aft", -16.4, YI, z2 + 2.6, 180.0, parent="fore")
    m.use("fore")
    pm = Vector((-11.5, YI, z2))                                                     # (its foot on the first level's roof)
    pm0 = pm + Z * 2.6
    m.lathe(pm, Z, [(0.0, 0.28), (2.6, 0.26), (14.6, 0.17), (19.6, 0.08)], "light", 8 if fine else 5)
    for zz in (4.0, 8.0):                                                            # held to the funnel's after wall
        for s_ in (-1, 1):
            m.rod((-11.03, YI + s_ * 0.7, z2 + zz - 0.25), (pm.x, YI, z2 + zz), 0.06, "light", sides=4)
    m.rod(pm0 + Vector((0, -3.0, 12.6)), pm0 + Vector((0, 3.0, 12.6)), 0.06, "light", sides=4)
    gaff0, gaff1 = pm0 + Vector((-0.15, 0, 10.4)), pm0 + Vector((-3.8, 0, 12.2))
    m.rod(gaff0, gaff1, 0.06, "light", sides=4)
    if fine:
        yard.wire(pm0 + Vector((0, 0, 16.8)), gaff1, 0.015)
        yard.wire((14.2, YI, sky_top + 5.9), pm0 + Vector((0.1, 0, 12.0)), 0.02, sag=0.4)        # the stay from the topmast
        ladder(m, Vector((-20.6, YI + 0.8, iz + 2.6)), z2, Vector((-1, 0, 0)))
        ladder(m, Vector((-20.2, YI + 0.8, z2)), z2 + 2.6, Vector((-1, 0, 0)))
        ladder(m, Vector((35.0, YI - 0.6, iz)), iz + 2.6, Vector((1, 0, 0)))
        ladder(m, Vector((11.0, YI + 1.0, z2)), z3, Vector((-1, 0, 0)))
    # Primary Flight Control: a booth cantilevered from the island's port side aft over the flight deck on a pair of
    # curved knees, glazed fore and aft, its port windows open; the air officer and his talker in it
    m.use("fore")
    pf0, pf1, pfi, pfo = -11.9, -9.2, -10.9, -8.55
    zf = z2 + 2.6
    cap.slab(m, Model.round_rect(pf1, pf0, pfi, pfo, 0.2, 2 if fine else 1), zf - 0.22, zf, paint="upper", top="deck", bottom=True)
    cap.slab(m, Model.round_rect(pf1 + 0.1, pf0 - 0.06, pfi, pfo + 0.12, 0.25, 2 if fine else 1), zf + 2.0, zf + 2.16, paint="upper", top="roof", bottom=True)
    m.box(((pf0 + pf1) * 0.5, pfi + 0.05, zf + 1.0), (pf1 - pf0, 0.1, 2.0), "upper")
    zl, zr = z2 - 1.6, zf - 0.22
    for kx in (pf1 - 0.35, pf0 + 0.35):
        curve = [(pfo - 0.1 - (pfo - 0.1 + 10.6) * math.sin(t), zl + (zr - zl) * math.cos(t)) for t in [math.pi * 0.5 * i / (8 if fine else 3) for i in range((8 if fine else 3) + 1)]]
        web(m, kx, [(-10.6, zr)] + curve, 0.14)
    if fine:
        ctr = Vector(((pf0 + pf1) * 0.5, pfi, 0.0))
        run = [Vector((pf1 - 0.03, pfi + 0.1, zf)), Vector((pf1 - 0.03, pfo - 0.03, zf)), Vector((pf0 + 0.03, pfo - 0.03, zf)), Vector((pf0 + 0.03, pfi + 0.1, zf))]
        plate(m, run, 1.05, ctr, "upper", thick=0.06)
        for p in run[1:3] + [Vector(((pf0 + pf1) * 0.5, pfo - 0.03, zf))]:
            m.box(p + Z * 1.52, (0.08, 0.08, 0.95), "upper")
        for px in (pf1 - 0.03, pf0 + 0.03):
            m.box((px, (pfi + pfo) * 0.5 + 0.05, zf + 1.52), (0.04, pfo - pfi - 0.3, 0.88), "glass")
        officer(m, (-10.3, -9.2, zf), (0.0, 1.0))
        fit.sailor(m, (-11.25, -9.75, zf), (0.6, 1.0, 0.0), "stand", navy="USA")
        m.box((-11.25 + 0.08, -9.75 + 0.14, zf + 1.38), (0.1, 0.1, 0.08), "gunmetal")                 # his telephone's mouthpiece
    # the 1.1-inch: one before the island at the flight deck, one over it on the island's fore end; the same abaft (their
    # training kept off the island's houses)
    quad_28(yard, 1, 40.0, -13.28, iz, bearing=-20.0, limits=(-125, 175), parent="fore")
    quad_28(yard, 2, 32.7, YI, iz + 2.6, bearing=0.0, tub=2.15, limits=(-133, 133), parent="fore")
    quad_28(yard, 3, -23.3, YI, iz + 2.6, bearing=180.0, tub=2.15, limits=(-138, 138), parent="fore")
    quad_28(yard, 4, -30.0, -13.28, iz, bearing=-160.0, limits=(-170, 120), parent="fore")
    # the aircraft crane at the deck's edge abaft the island, its jib topped up out over the side astern
    jib_crane(yard, -32.8, -14.85, iz, toward=185.0)

    # ---- her flight deck's crew: plane handlers forward of the range and by the catapults, officers by the island
    if fine:
        crew = [(-40.5, 3.0, 160.0), (-39.5, -2.5, 200.0), (-44.5, 7.5, 90.0), (-44.0, -8.0, 270.0), (-40.0, 0.6, 180.0), (-44.5, 0.0, 0.0),
                (96.0, 6.0, 0.0), (97.5, 7.3, 200.0), (94.0, -6.5, 30.0),
                (8.0, -8.6, 90.0), (9.4, -8.9, 120.0), (15.0, -8.4, 180.0), (21.0, -9.0, 200.0), (0.0, -8.0, 250.0), (-8.5, -8.7, 60.0)]
        for cx, cy, face in crew:
            a = math.radians(face)
            foot = Vector((cx, cy, iz + 0.035))
            fit.sailor(m.use(yard.half(cx)), foot, Vector((math.cos(a), math.sin(a), 0)), "stand", navy="USA")

    # ---- forecastle and quarterdeck: anchors and cables, bollards, rails, scuttles; staffs; screws and rudder
    yard.anchor_gear(h.L * 0.47, 104.5, spread=2.4)
    for bx_ in (111.0, 104.0, -116.0, -121.0):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 1.0), D(bx_))
    yard.rails(hx0 + 1.0, h.L * 0.486)
    yard.rails(-h.L * 0.492, hx1 - 0.8)
    yard.portholes(-h.L * 0.45, h.L * 0.44, 1.5, every=3.6)
    yard.portholes(-h.L * 0.42, h.L * 0.42, 3.4, every=3.6)
    m.use("aft")
    sx = -h.L * 0.496
    m.rod((sx + 0.6, 0, D(sx + 0.6)), (sx - 0.3, 0, D(sx + 0.6) + 4.0), 0.07, "upper", sides=4)
    m.use("fore")
    jx = h.L * 0.492
    m.rod((jx, 0, D(jx)), (jx + 0.3, 0, D(jx) + 1.9), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.3, 0, D(jx) + 1.9), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=4, r=2.1, rudders=1)

    carrier_park = [{"model": "DeckPark_CarrierFighter"}]
    cap.carrier_finish(yard, lifts, FD_FORE, FD_AFT, iz, hz, carrier_park, 108.0, -118.0, gaff1 + Vector((-0.2, 0, -0.4)), "fore")
    # (her range stands with its foremost row at frame 141, so that its last machines stop short of the after lift)
    m.parts[m.part_at["deckpark"]].pivot = Vector((-51.5, 0.0, iz))
    if fine:      # the lifts' platforms: an angle round each, the seams of their planks fore and aft like the deck's
        for i, (lx, ly, ll, lw, _) in enumerate(lifts):
            m.use(f"lift.{i + 1}")
            for s_ in (-1, 1):
                m.box((lx, ly + s_ * (lw * 0.5 - 0.14), iz + 0.05), (ll - 1.1, 0.1, 0.03), "steel")
                m.box((lx + s_ * (ll * 0.5 - 0.14), ly, iz + 0.05), (0.1, lw - 1.1, 0.03), "steel")
            for j in range(-22, 23):
                yy = ly + j * 0.3
                m.add([(lx - ll * 0.5 + 0.4, yy - 0.012, iz + 0.039), (lx + ll * 0.5 - 0.4, yy - 0.012, iz + 0.039),
                       (lx + ll * 0.5 - 0.4, yy + 0.012, iz + 0.039), (lx - ll * 0.5 + 0.4, yy + 0.012, iz + 0.039)], [(0, 1, 2, 3)], "seam", False, lambda q: Z)
    yard.fire_points += [(78.8, 0.0, iz), (-28.4, 2.0, iz), (-80.0, 0.0, iz), (2.0, YI, iz + 8.0), (-6.0, -10.0, iz - 3.0), (60.0, 0.0, iz - 4.0)]
    from ships import sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    cap.carrier_clips(m, yard, len(lifts), hz)
    m.info.setdefault("file", {}).update({
        "class": "Yorktown", "navy": "USA", "year": 1942, "stands_for": "Carrier_USA", "beam": 25.4,
        "paint": "Measure 12 graded: sea blue 5-S to the hangar deck, ocean grey 5-O above, haze grey 5-H masts; deck blue 20-B decks; "
                 "the flight deck stained dark blue-grey (250-N)"})
    return m, yard
