"""Soryu and Hiryu: the Second Carrier Division, as they were at Pearl Harbor (December 1941) and Midway (June 1942).

Our own models, made from numbers: their published dimensions and the layout their photographs of 1938-42 show, built
with one hull builder and a table of what differs between the sisters (`SORYU`, `HIRYU`).

    Soryu   227.5 m overall, 222.0 m long and 21.3 m broad on the waterline, 7.6 m draught; a 216.9 by 26.0 m wooden
            flight deck 12.8 m over the water, over two hangars; three lifts, the forward one on the centre line
            abreast the island, the other two a little to starboard of it (16.0 by 11.5 m forward; 11.5 by 12.0
            amidships; 11.8 by 10.0 aft); a small island forward on her STARBOARD side, built out on a sponson of its
            own so that it takes nothing from the deck's width, with her tripod mast behind it; a low forecastle.
    Hiryu   227.4 m overall, 222.9 by 22.3 m on the waterline (beamier), 7.8 m draught; the flight deck 216.9 by 27.0 m,
            12.8 m over the water as her sister's; her forecastle raised (after the Fourth Fleet storm of 1935), so
            that the open space under the deck's fore end is lower; lifts 16.0 by 13.0, 13.0 by 12.0 and 11.8 by
            13.0 m, laid as her sister's; her island on the PORT side, further aft, near amidships, standing in over
            the edge of her deck, the forward lift right before it (the photograph of her burning after Midway,
            NH 73064, shows that lift's platform thrown up and "standing upright just in front of the island"; the
            published "abreast the island" is her sister's sentence).

Both: two funnels led out of her starboard side amidships and turned down toward the water; six twin 12.7 cm/40
Type 89 high-angle mountings, open, on sponsons below the flight deck's edge, two forward and one aft each side
(`sec.P1-3`, `sec.S1-3`); two Type 94 directors for them, one on the island's roof (from there it could lay all six
mountings: `director.HAS` on Soryu, `director.HAP` on Hiryu, whose island is to port though the published sentence,
again her sister's, says starboard) and the other on a sponson under the deck's edge on the far side; the 25 mm Type
96 guns in open mountings on platforms
along her sides and on a platform under the forward end of her flight deck (Soryu fourteen twin; Hiryu seven
triple and five twin), each mounting a part `aa25.N` that trains with `aa25.N.guns` that lifts, every one manned;
Type 95 directors for them; searchlights; the radio masts on each side aft, hinged down outboard for flying; boats
in their bays and under the after overhang; nine arrester wires (Hiryu three of them forward, for landing over the
bow); the windbreak ahead of the forward lift, lowered; the white identification kana on the after deck (Soryu's
"sa", Hiryu's "hi"). Her range of aircraft (`deckpark`) stands between the middle and the after lifts, clear of both.

Paint: the Imperial Navy's grey all over, an unpainted wooden flight deck with its steel tie-down strips across it,
white lines, red and white stripes on its after end. Nothing is copied from any drawing, plan or model; where the
photographs and the published figures could not settle a place (the light guns' and the lifts' stations along her)
it is our own reckoning.
"""

import math
import random

from mathutils import Matrix, Vector

import capitals as cap
import fittings as fit
from rigid import PORT, STBD, X, Y, Z, Model, lerp
from shipyard import Yard, livery, weather_for


# ---- the two sisters: what differs between them --------------------------------------------------------------

class Spec:
    def __init__(self, **kw):
        self.__dict__.update(kw)


# the hull's deck edge (half widths as shares of half her beam): a long fine cruiser's bow, a long parallel middle,
# a rounded cruiser stern under the after overhang
SORYU_PLAN = [(-113.75, 0.0), (-113.4, 0.28), (-112.4, 0.46), (-110.0, 0.62), (-105.0, 0.77), (-97.0, 0.88), (-87.0, 0.95), (-72.0, 0.99),
              (-58.0, 1.0), (28.0, 1.0), (42.0, 0.97), (56.0, 0.91), (68.0, 0.83), (80.0, 0.72), (90.0, 0.6), (98.0, 0.48), (104.0, 0.37),
              (108.5, 0.26), (111.5, 0.16), (113.75, 0.0)]

# the flight deck's port edge, aft to fore: full width over most of her length, its after corners rounded, its fore
# end tapering to a squared nose over the forecastle (the starboard edge is its mirror)
SORYU_PORT = [(-113.1, 4.0), (-112.8, 7.6), (-111.9, 9.9), (-110.2, 11.2), (-107.0, 12.1), (-100.0, 12.7), (-88.0, 13.0), (42.0, 13.0),
              (52.0, 12.7), (64.0, 11.9), (76.0, 10.8), (88.0, 9.6), (97.0, 8.7), (101.6, 8.1), (103.2, 7.3), (103.8, 5.0)]
HIRYU_PORT = [(-113.1, 4.2), (-112.8, 7.8), (-111.9, 10.2), (-110.2, 11.6), (-107.0, 12.6), (-100.0, 13.2), (-88.0, 13.5), (44.0, 13.5),
              (54.0, 13.2), (66.0, 12.3), (78.0, 11.2), (90.0, 10.0), (98.0, 9.1), (101.8, 8.5), (103.3, 7.6), (103.8, 5.2)]

SORYU = Spec(
    name="Soryu", title="Aircraft carrier Soryu, 1941-42 (our own model)", kana="sa",
    L=227.5, B=22.2, T=7.6, D=7.3, sheer=1.5, sheer_aft=0.3, rake=3.5, counter=2.0, flare=0.8, plan=SORYU_PLAN,
    port=SORYU_PORT, iz=12.8, thick=0.55, hangar=(83.0, -93.0), bx=-2.0,
    # her island: on its own sponson out from the starboard edge (x from, x to, out to), its levels (fore, aft,
    # half width, height), the middle of its houses, the tripod's foot
    island=Spec(hand=STBD, spon=(39.0, 58.0), out=18.9, yi=-15.8, mast_x=42.6, mast_high=17.0,
                levels=[(55.5, 45.0, 2.3, 2.5), (55.0, 45.6, 2.1, 2.4), (54.2, 46.5, 1.85, 2.2)]),
    lifts=[(44.0, 0.0, 16.0, 11.5), (-14.0, -1.2, 11.5, 12.0), (-75.5, -1.2, 11.8, 10.0)],
    funnels=[(2.0, 1.95), (-7.6, 1.8)],
    guns=[78.0, 66.5, -74.0],
    hadirs=[(PORT, 59.0)],
    bow_aa=[(3.5, 2), (0.0, 2), (-3.5, 2)],
    aa=[(PORT, [(30.0, 2), (23.5, 2)]), (PORT, [(-27.0, 2), (-33.5, 2)]), (PORT, [(-86.0, 2), (-92.5, 2)]),
        (STBD, [(24.0, 2)]), (STBD, [(-27.0, 2), (-33.5, 2)]), (STBD, [(-86.0, 2), (-92.5, 2)])],
    aadirs=[(PORT, 36.0), (PORT, -20.0), (STBD, -20.0), (STBD, -80.0)],
    lights=[(PORT, 10.0), (PORT, -58.0), (STBD, 14.0), (STBD, -58.0)],
    wires=[-36.0, -42.0, -48.0, -54.0, -60.0, -86.0, -92.0, -98.0, -104.0], barriers=[16.0, 22.0],
    masts=[-45.0, -54.0, -63.0], bays=[(-41.0, 8.5), (-51.0, 7.0)],
    screws=4, screw_r=1.85,
)

HIRYU = Spec(
    name="Hiryu", title="Aircraft carrier Hiryu, 1941-42 (our own model)", kana="hi",
    L=227.4, B=23.2, T=7.8, D=7.6, sheer=2.2, sheer_aft=0.3, rake=3.0, counter=1.5, flare=0.85, plan=SORYU_PLAN,
    port=HIRYU_PORT, iz=12.8, thick=0.55, hangar=(83.0, -93.0), bx=-2.0,
    island=Spec(hand=PORT, spon=(6.0, 24.6), out=17.2, yi=13.9, mast_x=9.0, mast_high=19.0,
                levels=[(22.0, 11.0, 2.3, 2.6), (21.4, 11.6, 2.1, 2.4), (20.6, 12.5, 1.9, 2.3)]),
    lifts=[(30.5, 0.0, 16.0, 13.0), (-16.0, -1.2, 13.0, 12.0), (-78.0, -1.2, 11.8, 13.0)],
    funnels=[(2.0, 2.0), (-7.6, 1.85)],
    guns=[78.0, 66.5, -74.0],
    hadirs=[(STBD, 58.0)],
    bow_aa=[(2.7, 3), (-2.7, 3)],
    aa=[(PORT, [(34.5, 3), (28.0, 2)]), (PORT, [(-27.0, 3), (-33.5, 2)]), (PORT, [(-88.0, 3)]),
        (STBD, [(30.0, 3), (23.5, 2)]), (STBD, [(-27.0, 3), (-33.5, 2)]), (STBD, [(-88.0, 2)])],
    aadirs=[(PORT, 40.0), (PORT, -20.0), (STBD, 36.0), (STBD, -20.0)],
    lights=[(PORT, -58.0), (STBD, 14.0), (STBD, -58.0)],
    wires=[-40.0, -46.0, -52.0, -58.0, -86.0, -94.0, 56.0, 62.0, 68.0], barriers=[3.0, -3.5],
    masts=[-45.0, -54.0, -63.0], bays=[(-41.0, 8.5), (-51.0, 7.0)],
    screws=4, screw_r=1.9,
)

# the Zero's range (`deckpark.places`): from its pivot it reaches 3.8 m forward (the foremost airscrews) and 44.5 m aft
# (the last tails)
RANGE_FWD, RANGE_AFT = 3.8, 44.5

PAINT = {"flight": "B49B70", "planks": "B49B70", "plane": "3E4A3C", "green": "2F8F4E", "plankline": "8E7652", "dodger": "726E5C"}


# ---- small pieces --------------------------------------------------------------------------------------------

def ccw(plan):
    """A plan turned so that it goes round anticlockwise seen from above."""
    a = sum(plan[i][0] * plan[(i + 1) % len(plan)][1] - plan[(i + 1) % len(plan)][0] * plan[i][1] for i in range(len(plan)))
    return plan if a > 0 else list(reversed(plan))


def edge_of(sp, hand, x):
    """The flight deck's edge at x on one side (y as it is: starboard's negative)."""
    return hand * cap.edge_at(sp.port, x)


def glaze(m, plan, z, high, keep=lambda out: True, every=0.95, gap=0.13):
    """A band of windows along a house's walls: panes on each edge of its plan whose outward way `keep` passes."""
    n = len(plan)
    for k in range(n):
        a, b = Vector((*plan[k], z)), Vector((*plan[(k + 1) % n], z))
        d = b - a
        L = d.length
        if L < 0.45:
            continue
        out = Vector((d.y, -d.x, 0.0)).normalized()
        if not keep(out):
            continue
        cnt = max(1, round(L / every))
        yaw = math.degrees(math.atan2(d.y, d.x))
        for i in range(cnt):
            c = a + d * ((i + 0.5) / cnt) + out * 0.025 + Z * (high * 0.5)
            m.box(c, (L / cnt - gap, 0.06, high), "glass", yaw=yaw)


def grow(plan, d):
    """A plan (anticlockwise) grown outward by d all round."""
    n = len(plan)
    out = []
    for k in range(n):
        a, p, c = Vector(plan[k - 1]), Vector(plan[k]), Vector(plan[(k + 1) % n])
        n1 = Vector((p.y - a.y, -(p.x - a.x))).normalized()
        n2 = Vector((c.y - p.y, -(c.x - p.x))).normalized()
        b = (n1 + n2).normalized()
        out.append(tuple(p + b * (d / max(0.3, b.dot(n1)))))
    return out


def rim(x0, x1, ya, yo, r, hand, steps=4):
    """The outer edge of a platform out from her side, as a row of points: from her side at x1 out round its corners
    to its outer edge and back in at x0 (`ya` where it starts out from, `yo` its outer edge, both as half widths)."""
    pts = [(x1, ya)]
    for k in range(steps + 1):
        a = math.radians(90.0 * k / steps)
        pts.append((x1 - r + r * math.cos(a), yo - r + r * math.sin(a)))
    for k in range(steps + 1):
        a = math.radians(90.0 + 90.0 * k / steps)
        pts.append((x0 + r + r * math.cos(a), yo - r + r * math.sin(a)))
    pts.append((x0, ya))
    return [(px, hand * py) for px, py in pts]


def bulwark(m, pts, z, high, paint="upper", thick=0.06):
    """A thin splinter screen standing along a row of points, its top rolled into a lip."""
    for a, b in zip(pts, pts[1:]):
        a, b = Vector((a[0], a[1], z)), Vector((b[0], b[1], z))
        d = b - a
        if d.length < 1e-3:
            continue
        yaw = math.degrees(math.atan2(d.y, d.x))
        mid = (a + b) * 0.5
        m.box(mid + Z * (high * 0.5), (d.length + thick, thick, high), paint, yaw=yaw)
        if m.fine:
            m.box(mid + Z * (high + 0.02), (d.length + 0.1, 0.12, 0.05), paint, yaw=yaw)


def hull_door(m, x, y, z, hand):
    """A watertight door in her side (its sill at z): its coaming, the door, six dogs round its edge, a handle."""
    m.box((x, y + hand * 0.01, z + 0.9), (0.98, 0.06, 2.06), "upper")
    m.box((x, y + hand * 0.035, z + 0.9), (0.8, 0.06, 1.88), "door")
    for dz in (0.35, 0.95, 1.55):
        for s in (1, -1):
            m.box((x + s * 0.43, y + hand * 0.07, z + dz), (0.1, 0.06, 0.05), "steel")
    m.box((x - 0.25, y + hand * 0.08, z + 0.95), (0.05, 0.04, 0.22), "steel")


def louvre(m, x, y, z, hand, wide=1.0, high=0.6):
    """A ventilator's louvred panel in a wall: a frame, the dark opening, its slats sloping down outward."""
    m.box((x, y + hand * 0.01, z), (wide, 0.04, high), "black")
    for s in (1, -1):
        m.box((x + s * (wide * 0.5 + 0.03), y + hand * 0.03, z), (0.07, 0.08, high + 0.12), "upper")
        m.box((x, y + hand * 0.03, z + s * (high * 0.5 + 0.03)), (wide, 0.08, 0.07), "upper")
    for k in range(5):
        m.box((x, y + hand * 0.04, z - high * 0.5 + (k + 0.5) * high / 5), (wide - 0.02, 0.09, 0.025), "upper", roll=hand * 40.0)


def dodger(m, run, z, high, every=0.95, sag=0.025, thick=0.02, paint="dodger"):
    """A canvas dodger laced along a rail: a thin weathered sheet standing on a row of points (going round with the
    outside on the right), its top hanging in a shallow sag between the lashings, a lashing every `every` metres."""
    run = [Vector((q[0], q[1], 0.0)) for q in run]
    pts = []
    for a, b in zip(run, run[1:]):
        steps = max(1, int((b - a).length / 0.16))
        for i in range(steps):
            pts.append(a.lerp(b, i / steps))
    pts.append(run[-1])
    s, acc = [0.0], 0.0
    for a, b in zip(pts, pts[1:]):
        acc += (b - a).length
        s.append(acc)
    stations = max(1, round(acc / every))
    outs, tops = [], []
    for i, p in enumerate(pts):
        d = pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]
        outs.append(Vector((d.y, -d.x, 0.0)).normalized())
        u = (s[i] / acc) * stations
        tops.append(z + high - sag * math.sin(math.pi * (u - math.floor(u))))
    n = len(pts)
    ob = [Vector((p.x, p.y, z)) + o * thick * 0.5 for p, o in zip(pts, outs)]
    ot = [Vector((p.x, p.y, t)) + o * thick * 0.5 for p, o, t in zip(pts, outs, tops)]
    ib = [Vector((p.x, p.y, z)) - o * thick * 0.5 for p, o in zip(pts, outs)]
    it = [Vector((p.x, p.y, t)) - o * thick * 0.5 for p, o, t in zip(pts, outs, tops)]
    verts = ob + ot + ib + it
    faces = []
    for i in range(n - 1):
        faces.append((i, i + 1, n + i + 1, n + i))                          # (outside)
        faces.append((2 * n + i, 3 * n + i, 3 * n + i + 1, 2 * n + i + 1))  # (inside)
        faces.append((n + i, n + i + 1, 3 * n + i + 1, 3 * n + i))          # (its hemmed top)
    faces += [(0, n, 3 * n, 2 * n), (n - 1, 2 * n + n - 1, 3 * n + n - 1, n + n - 1)]
    mids = [(Vector((p.x, p.y, (z + t) * 0.5))) for p, t in zip(pts, tops)]

    def out(mid):
        i = min(range(n), key=lambda j: (Vector((pts[j].x, pts[j].y, mid.z)) - mid).length_squared)
        if mid.z > tops[i] - 0.004:
            return Z
        w = mid - mids[i]
        if w.length < 0.004:                                                # (an end)
            return (pts[0] - pts[1]) if i < n // 2 else (pts[-1] - pts[-2])
        return w
    m.add(verts, faces, paint, False, out)
    for k in range(stations + 1):                                          # (the lashings round the rail at each station)
        i = min(range(n), key=lambda j: abs(s[j] - acc * k / stations))
        m.box(ot[i] - Z * 0.03, (0.035, thick + 0.03, 0.07), "rope", yaw=math.degrees(math.atan2(outs[i].y, outs[i].x)) + 90.0)


def round_sponson(yard, x, yc, z, r, wall, hand, thick=0.3, rail=True, brackets=True):
    """A round platform out from her side: a full round outboard, squared back into the wall (`wall` the wall's y,
    the platform's inner edge buried a little inside it); brackets under it to the wall; a rail round its edge."""
    m = yard.on(x)
    fine = m.fine
    n = 14 if fine else 6
    pts = []
    for i in range(n + 1):
        a = math.radians(-100.0 + 200.0 * i / n)         # (from abaft round through outboard to forward of her)
        pts.append((x - r * math.sin(a), yc + hand * r * math.cos(a)))
    inner = wall - hand * 0.25
    pts = [(x + r * 0.985, inner)] + pts + [(x - r * 0.985, inner)]
    plan = ccw(pts)
    cap.slab(m, plan, z - thick, z, paint="upper", top="deck", bottom=True)
    if fine and rail:
        from ships import railing
        rl = []
        for i in range(n + 1):
            a = math.radians(-96.0 + 192.0 * i / n)
            rl.append(Vector((x - (r - 0.1) * math.sin(a), yc + hand * (r - 0.1) * math.cos(a), z)))
        railing(m, rl, high=1.0, every=1.5)
    if brackets:
        for dx in ((-r * 0.6, 0.0, r * 0.6) if fine else (0.0,)):
            m.rod((x + dx, wall, z - 2.6), (x + dx, yc + hand * r * 0.45, z - thick), 0.09, "upper", sides=4)
        if fine:                                        # (a deep web plate under its middle, with lightening holes)
            span = abs(yc - wall) + 0.5
            m.box((x, (wall + yc) * 0.5 + hand * 0.25, z - thick - 0.65), (0.08, span, 1.3), "upper")
            for k in range(max(0, int((span - 0.9) / 0.85))):
                for s in (1, -1):
                    m.disc((x + s * 0.045, wall + hand * (0.75 + k * 0.85), z - thick - 0.65), (s, 0, 0), 0.26, "black", 10)
    return plan


# ---- guns and directors ----------------------------------------------------------------------------------------

def aa25(yard, n, x, y, z, bearing=0.0, guns=2, crew=True, tub=0.0, parent=None, overhead=None):
    """Japan's 25 mm Type 96 mounting, twin or triple: the guns side by side in one cradle on a pedestal, a long flash
    hider on each and the box magazine standing on top; the layer and the trainer on their seats either side with their
    handwheels and sights, two loaders behind, open (no shield, as in 1941-42), on a round platform that turns with it;
    a man with a box of clips at the ready-use locker behind it. Parts `aa25.N` (trains, plus clockwise seen from
    above) and `aa25.N.guns` (plus lifts them); socket `aa.K` at the muzzles. (After `fittings.jp_triple25`, with the
    crew's feet on the platform.) `overhead`: the height of a deck over the mounting; where a loader's raised clip
    would not clear it he holds the clip at his chest."""
    m = yard.m
    fine = m.fine
    half = parent or yard.half(x)
    m.use(half)
    o = Vector((x, y, z))
    if tub:
        m.tub(o, tub, 1.05, "upper", wall=0.04, sides=20 if fine else 8, floor="deck")
    spread = 0.3 * (guns - 1) * 0.5 + 0.2                 # (half the cradle's width)
    rp = max(1.05, spread + 0.72)                         # (the turning platform's radius)
    kind = "triple" if guns == 3 else "twin"
    tn = m.part(f"aa25.{n}", half, (x, y, z + 0.2), axis=(0, 0, -1), limits=[-180, 180], kind="turn",
                note=f"a {kind} 25 mm mounting: plus trains it clockwise seen from above; rest bearing {int(bearing)}; about 30 degrees a second")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 30.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y

    def W(px, py, pz):
        return o + f * px + l * py + Z * pz
    m.lathe(o, Z, [(0.0, 0.42), (0.12, 0.34), (0.95, 0.26)], "upper", 12 if fine else 6)
    m.lathe(o + Z * 0.14, Z, [(0.0, rp), (0.06, rp)], "deck", 20 if fine else 8)
    for s in (1, -1):                                     # the carriage's cheeks up to the trunnions
        m.box(W(0.0, s * (spread + 0.07), 0.92), (0.62, 0.07, 0.66), "upper", yaw=-bearing)
    piv = W(0.0, 0.0, 1.2)
    gn = m.part(f"aa25.{n}.guns", tn, tuple(piv), axis=tuple(-l), limits=[-10, 85], kind="turn", note=f"the {guns} guns: plus lifts them")
    m.use(gn)
    e = math.radians(15.0)
    g = (f * math.cos(e) + Z * math.sin(e)).normalized()
    up = g.cross(l).normalized()
    for j in range(guns):
        q = piv + l * ((j - (guns - 1) * 0.5) * 0.3)
        m.box(q - g * 0.2, (1.05, 0.15, 0.24), "gunmetal", yaw=-bearing, pitch=15.0)
        m.rod(q + g * 0.32, q + g * 1.7, 0.03, "gunmetal", sides=6 if fine else 4)
        m.lathe(q + g * 1.7, g, [(0.0, 0.045), (0.48, 0.05), (0.53, 0.036)], "gunmetal", 8 if fine else 4)
        if fine:
            m.box(q - g * 0.16 + up * 0.27, (0.34, 0.1, 0.3), "gunmetal", yaw=-bearing, pitch=15.0)     # the magazine
            m.rod(q - g * 0.7, q - g * 0.9 - up * 0.15, 0.02, "gunmetal", sides=4)
    m.box(piv - g * 0.05, (0.35, spread * 2.0 + 0.1, 0.12), "gunmetal", yaw=-bearing, pitch=15.0)
    k = sum(1 for s in m.sockets if s["name"].startswith("aa.")) + 1
    m.socket(f"aa.{k}", piv + g * 2.3, g, gn, f"a {kind} 25 mm mounting's muzzles: {guns} streams of tracer start here")
    m.use(tn)
    for s in (1, -1):
        s0 = W(-0.34, s * (spread + 0.56), 0.2)
        m.rod(s0, s0 + Z * 0.38, 0.035, "upper", sides=5)
        m.box(s0 + Z * 0.41, (0.32, 0.32, 0.06), "gunmetal", yaw=-bearing)                # his seat
        m.box(W(0.22, s * (spread + 0.38), 1.34), (0.2, 0.1, 0.18), "gunmetal", yaw=-bearing)   # his sight
        if fine:
            m.lathe(W(0.06, s * (spread + 0.16), 0.97), l * s, [(0.0, 0.13), (0.025, 0.13)], "gunmetal", 10, caps=(False, False))
            m.rod(W(0.06, s * (spread + 0.07), 0.97), W(0.06, s * (spread + 0.16), 0.97), 0.02, "gunmetal", sides=4)
    if crew and fine:
        for s in (1, -1):
            fit.sailor(m, W(-0.34, s * (spread + 0.56), 0.2), f, "sit", seat=0.43,
                       hands=(W(0.0, s * (spread + 0.25) + 0.1, 0.97), W(0.0, s * (spread + 0.25) - 0.1, 0.97)), navy="JAP", lean=0.08)
        pose = "carry" if overhead is not None and z + 0.2 + 2.2 > overhead else "load"
        fit.sailor(m, W(-0.92, 0.3, 0.2), f, pose, navy="JAP")
        fit.sailor(m, W(-0.92, -0.32, 0.2), f, pose, navy="JAP")
    m.use(half)
    if fine:                                              # the ready-use locker and the man at it, off the platform
        b = Vector((math.cos(math.radians(-bearing + 180.0)), math.sin(math.radians(-bearing + 180.0)), 0.0))
        side = Z.cross(b)
        lk = o + b * (rp + 0.75) + side * 0.6
        m.box(lk + Z * 0.38, (0.6, 0.9, 0.76), "upper", yaw=math.degrees(math.atan2(b.y, b.x)))
        if crew:
            fit.sailor(m, o + b * (rp + 0.55) - side * 0.45, -b, "carry", navy="JAP")
    return tn


def twin127(yard, name, x, y, z, bearing, limits, crew=True):
    """The twin 12.7 cm/40 Type 89 high-angle mounting, open, as the carriers had it in 1941-42: the two guns in one
    cradle on a pedestal, each with its breech, its recoil cylinder over the barrel and its loading tray; the layer
    and the trainer at their handwheels and sights either side, two loaders with rounds raised behind the breeches, the
    fuze-setter at his machine, the mount's captain. A `sec.*` turret (parts, sockets and clip as the yard's)."""
    m = yard.m
    S = 3.6
    tn, guns = yard.turret(name, x, z, S, 2, 4.6, bearing, 0.065, y=y, style="open", rate=16.0, elev=(-8, 90), rest_elev=10.0,
                           limits=limits, floats=False)
    if not m.fine:
        return tn, guns
    p = m.parts[m.part_at[tn]]
    o = Vector(p.pivot)                                   # (0.45 over the platform)
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
    f, l = rot @ X, rot @ Y

    def W(px, py, pz):
        return o + rot @ Vector((px, py, pz))
    hh = 0.66 * S
    e = math.radians(10.0)
    ax = (f * math.cos(e) + Z * math.sin(e)).normalized()
    upg = ax.cross(l).normalized()
    for k, gname in enumerate(guns):                     # on each gun: its recoil cylinder, its loading tray
        gp = Vector(m.parts[m.part_at[gname]].pivot)
        m.use(gname)
        m.rod(gp + upg * 0.2 - ax * 0.3, gp + upg * 0.2 + ax * 1.5, 0.075, "upper", sides=8)
        m.box(gp - ax * 1.95 - upg * 0.05, (0.9, 0.22, 0.06), "steel", yaw=bearing, pitch=10.0)
        m.box(gp - ax * 0.15, (0.75, 0.42, 0.5), "upper", yaw=bearing, pitch=10.0)       # the cradle's sleeve
    m.use(tn)
    m.box(W(0.0, 0.0, hh * 0.5 - 0.42), (0.9, 1.3, 0.22), "upper", yaw=bearing)          # the cradle's carriage, between the guns
    for s in (1, -1):                                     # side frames up to the trunnions
        m.box(W(0.05, s * 0.95, hh * 0.5 - 0.62), (1.1, 0.08, 1.25), "upper", yaw=bearing)
        # the layer's (port) and trainer's (starboard) stand: a bracket, the handwheel, the sight
        m.box(W(0.25, s * 1.18, 0.35), (0.25, 0.45, 0.08), "upper", yaw=bearing)
        m.rod(W(0.25, s * 1.32, -0.45), W(0.25, s * 1.32, 0.42), 0.06, "upper", sides=6)
        wheel = W(0.42, s * 1.35, 0.42)
        m.lathe(wheel, f, [(0.0, 0.2), (0.03, 0.2)], "steel", 12, caps=(False, False))
        m.rod(W(0.3, s * 1.35, 0.42), wheel, 0.025, "steel", sides=4)
        m.lathe(W(0.1, s * 1.15, 1.0), f, [(0.0, 0.07), (0.55, 0.07), (0.6, 0.09)], "gunmetal", 8)
        m.rod(W(0.25, s * 1.0, 0.75), W(0.25, s * 1.12, 0.98), 0.03, "upper", sides=4)
    # the fuze-setting machine at the left rear, the ammunition hoist's mouth at the right
    m.box(W(-1.35, 1.0, 0.0), (0.5, 0.45, 0.9), "upper", yaw=bearing)
    m.box(W(-1.35, 1.0, 0.5), (0.38, 0.32, 0.12), "gunmetal", yaw=bearing)
    m.box(W(-1.5, -1.05, -0.05), (0.6, 0.6, 0.8), "upper", yaw=bearing)
    if crew:
        for s in (1, -1):
            fit.sailor(m, W(-0.25, s * 1.4, -0.45), f, "aim", hands=(W(0.38, s * 1.35 + 0.17, 0.48), W(0.38, s * 1.35 - 0.17, 0.48)), navy="JAP", lean=0.05)
            fit.sailor(m, W(-2.25, s * 0.5, -0.45), f, "load", navy="JAP")
        fit.sailor(m, W(-1.85, 1.0, -0.45), f.lerp(l, 0.4), "carry", navy="JAP")
        fit.sailor(m, W(-2.2, -1.5, -0.45), f.lerp(-l, 0.3), "stand", navy="JAP")
    return tn, guns


def director94(yard, name, x, y, z, bearing=0.0, parent=None):
    """The Type 94 high-angle director: a squat round house on a pedestal, its 4.5 m rangefinder through it with a hood
    at each end, the layers' window across its face, a hatch on its roof. A turn part."""
    m = yard.m
    half = parent or yard.half(x)
    dn = m.part(name, half, (x, y, z), axis=(0, 0, -1), kind="turn", note=f"a high-angle director: turns with the 12.7 cm guns it serves; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y
    o = Vector((x, y, z))
    m.lathe(o, Z, [(0.0, 0.5), (0.9, 0.45), (1.0, 1.25), (2.3, 1.2), (2.55, 0.8)], "upper", 18 if m.fine else 8)
    c = o + Z * 1.85
    m.lathe(c - l * 2.25, l, [(0.0, 0.2), (0.25, 0.24), (4.25, 0.24), (4.5, 0.2)], "upper", 10 if m.fine else 5)
    if m.fine:
        for s in (1, -1):
            m.box(c + l * s * 2.05 + f * 0.15, (0.55, 0.42, 0.55), "upper", yaw=-bearing)
            m.disc(c + l * s * 2.05 + f * 0.43, f, 0.12, "glass", 8)
        m.box(o + Z * 1.75 + f * 1.1, (0.08, 1.2, 0.35), "glass", yaw=-bearing)
        m.lathe(o + Z * 2.55, Z, [(0.0, 0.35), (0.12, 0.33)], "upper", 10)
    return dn


def director95(yard, name, x, y, z, bearing=0.0, crew=True):
    """The Type 95 director for the 25 mm guns: a small rangefinder and sight on a pedestal, worked by two men."""
    m = yard.m
    half = yard.half(x)
    dn = m.part(name, half, (x, y, z), axis=(0, 0, -1), kind="turn", note=f"a 25 mm director: turns with the guns it serves; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y
    o = Vector((x, y, z))
    m.lathe(o, Z, [(0.0, 0.3), (0.1, 0.2), (1.15, 0.15), (1.2, 0.3)], "upper", 10 if m.fine else 5)
    c = o + Z * 1.45
    m.lathe(c - l * 0.75, l, [(0.0, 0.1), (0.1, 0.12), (1.4, 0.12), (1.5, 0.1)], "upper", 8 if m.fine else 4)
    m.box(c, (0.5, 0.5, 0.45), "upper", yaw=-bearing)
    if crew and m.fine:
        for s in (1, -1):
            fit.sailor(m, o - f * 0.55 + l * s * 0.5, f, "aim", hands=(c - f * 0.2 + l * (s * 0.45 + 0.12) - Z * 0.15, c - f * 0.2 + l * (s * 0.45 - 0.12) - Z * 0.15),
                       navy="JAP", lean=0.1)
    return dn


# ---- the funnels: led out of her starboard side and turned down -------------------------------------------------

def downtake(yard, x, wall_y, z0, r, out=6.0, drop=4.4):
    """One of her funnels: the trunk out of the starboard side bending down toward the water (`capitals.bent_funnel`),
    with the casing round its root, two stays from her side to its bend, its seams, and the pipe of the cooling spray
    that was led to its mouth."""
    cap.bent_funnel(yard, x, wall_y, z0, r, out=out, drop=drop)
    m = yard.on(x)
    # (the funnel's path, as `bent_funnel` lays it)
    p_root = Vector((x, wall_y, z0))
    th = math.pi * 0.42
    mid = Vector((x - 0.4 - 0.8 * 0.5, wall_y - (out - drop) - math.sin(th * 0.5) * drop, z0 + 0.25 - (1.0 - math.cos(th * 0.5)) * drop))
    m.box(p_root + Vector((-0.2, -0.25, 0.0)), (r * 2.6, 0.5, r * 2.3), "upper")          # the casing where it leaves her side
    if m.fine:
        for dz in (-r * 1.1, r * 1.1):
            m.box(p_root + Vector((-0.2, -0.52, dz)), (r * 2.7, 0.08, 0.1), "seam")
        for dx in (-r * 0.6, r * 0.6):                    # two stays from her side up to the bend
            m.rod((x - 0.6 + dx, wall_y + 0.05, z0 - 3.6), mid + Vector((dx * 0.8, 0.0, -r * 0.7)), 0.09, "upper", sides=5)
        pipe = []                                       # (along its crown, from her side to its mouth)
        for i in range(10):
            t = i / 9.0
            if t < 0.45:
                u = t / 0.45
                c = Vector((x - 0.4 * u, wall_y - (out - drop) * u, z0 + 0.25 * u))
                nrm = Z.copy()
            else:
                u = (t - 0.45) / 0.55
                a = u * th
                c = Vector((x - 0.4 - 0.8 * u, wall_y - (out - drop) - math.sin(a) * drop, z0 + 0.25 - (1.0 - math.cos(a)) * drop))
                nrm = Vector((0.0, -math.sin(a), math.cos(a)))
            pipe.append(c + nrm * (r * 1.18 * lerp(1.0, 1.08, i / 10.0) + 0.065) + X * 0.0)
        pipe[0] = pipe[0] + Vector((0.0, 0.25, 0.0))
        for a_, b_ in zip(pipe[:-1], pipe[1:-1]):
            m.rod(a_, b_, 0.06, "steel", sides=5)
        # the spray ring round the trunk just short of its mouth (the sea water sprayed into the smoke to cool it and
        # keep it down), fed by that pipe, clamped to the trunk
        end = Vector((x - 1.2, wall_y - (out - drop) - math.sin(th) * drop, z0 + 0.25 - (1.0 - math.cos(th)) * drop))
        a8 = th * (8.0 / 9.0 - 0.45) / 0.55
        prev = Vector((x - 0.4 - 0.8 * (8.0 / 9.0 - 0.45) / 0.55, wall_y - (out - drop) - math.sin(a8) * drop, z0 + 0.25 - (1.0 - math.cos(a8)) * drop))
        tan = (end - prev).normalized()
        side = X.cross(tan).normalized()
        up = tan.cross(side).normalized()
        rr = r * lerp(1.0, 1.08, 0.9)
        c0 = end - tan * 0.3
        ring = [c0 + side * math.cos(2 * math.pi * k / 16) * (rr * 1.18 + 0.12) + up * math.sin(2 * math.pi * k / 16) * (rr + 0.12) for k in range(16)]
        for k in range(16):
            m.rod(ring[k], ring[(k + 1) % 16], 0.045, "steel", sides=4, caps=(False, False))
            if k % 4 == 0:
                inner = c0 + (ring[k] - c0) * ((rr * 0.95) / ((ring[k] - c0).length) if k % 8 else (rr * 1.12) / ((ring[k] - c0).length))
                m.rod(ring[k], inner, 0.035, "steel", sides=4)
        feed = min(ring, key=lambda q: (q - pipe[-2]).length)
        m.rod(pipe[-2], feed, 0.06, "steel", sides=5)


# ---- her ground tackle ------------------------------------------------------------------------------------------

def _plate(m, base, way, across, long, w0, w1, th, paint, taper_th=1.0):
    """A tapered plate: from the middle of its base edge `long` along `way`, `w0` wide there and `w1` at its tip
    (across `across`), `th` thick (thinning to `th * taper_th` at the tip)."""
    nn = way.cross(across).normalized()
    vs = []
    for t, w, k_ in ((0.0, w0, th), (long, w1, th * taper_th)):
        for s_ in (-1, 1):
            for k in (-1, 1):
                vs.append(base + way * t + across * (s_ * w * 0.5) + nn * (k * k_ * 0.5))
    c = sum(vs, Vector()) / 8.0
    m.add(vs, [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)], paint, False, lambda q: q - c)


def ground_tackle(yard, x_hawse, x_cap, spread=2.2):
    """On the forecastle: each cable from its deck pipe to its capstan, its links standing up along it and a stopper
    on it; on her bow the hawse's lip and the stockless anchor housed in it, its shank up the hawse and its flukes
    against her side. (After `Yard.anchor_gear`.)"""
    m, h = yard.m, yard.h
    fine = m.fine
    ka = max(0.42, min(1.0, h.B / 30.0))
    for hand in (PORT, STBD):
        y0 = hand * min(spread, h.half_at(x_hawse) * 0.55)
        yc = hand * spread * 0.55
        yard.on(x_cap)
        pts = [Vector((lerp(x_hawse, x_cap, t), lerp(y0, yc, t), 0.0)) for t in [k / 10.0 for k in range(11)]]
        pts = [Vector((q.x, q.y, yard.deck(q.x) + 0.02)) for q in pts]
        d = pts[-1] - pts[0]
        yaw = math.degrees(math.atan2(d.y, d.x))
        if fine:
            m.strip(pts[1:-1], [Z] * 9, 0.11, "steel", 0.02)
            for k in range(1, 10):
                m.box(pts[k] + Z * 0.08, (0.34, 0.07, 0.13), "steel", yaw=yaw)
            m.box(pts[4] + Z * 0.18, (0.7, 0.55, 0.32), "steel", yaw=yaw)              # the stopper
            m.box(pts[4] + Z * 0.03, (1.0, 0.75, 0.06), "steel", yaw=yaw)
        yard.capstan(x_cap, yc, yard.deck(x_cap))
        m.lathe((x_hawse, y0, yard.deck(x_hawse) - 0.02), Z, [(0.0, 0.5 * ka), (0.14, 0.44 * ka)], "steel", 12 if fine else 6)
        m.disc((x_hawse, y0, yard.deck(x_hawse) + 0.125), Z, 0.34 * ka, "black", 12 if fine else 6)
        # the hawse on her bow: a doubling plate round it, its lip, the pipe's dark mouth; the stockless anchor housed
        # in it, its shank up the pipe so that only a hand's breadth of it shows, its crown just under the lip and its
        # two flukes lying up against her plating either side of the shank
        p, n = h.side(x_hawse + 2.5 * ka, yard.deck(x_hawse) - max(2.6 * ka, 1.3), hand)
        rl = 0.66 * ka
        m.lathe(p - n * 0.04, n, [(0.0, rl + 0.32), (0.07, rl + 0.28)], "upper", 18 if fine else 6)
        m.lathe(p - n * 0.06, n, [(0.0, rl), (0.17, rl - 0.06)], "upper", 16 if fine else 6)
        m.disc(p + n * 0.115, n, 0.5 * ka, "black", 12)
        side = Z.cross(n).normalized()
        upv = n.cross(side).normalized()                  # (up along her plating)
        if upv.z < 0.0:
            upv = -upv
        crown = p - upv * (rl + 0.3 + 0.17 * ka) + n * (0.07 + 0.2 * ka)
        m.rod(crown, p - upv * 0.15 + n * 0.02, 0.13 * ka, "steel", sides=6)
        _plate(m, crown - upv * 0.17 * ka, upv, side, 0.34 * ka, 1.0 * ka, 0.8 * ka, 0.4 * ka, "steel")     # (the crown)
        for s_ in (1, -1):
            way = (upv * 0.94 + side * s_ * 0.34).normalized()
            across = n.cross(way).normalized()
            base = crown + side * s_ * 0.3 * ka - n * 0.1 * ka + way * 0.1 * ka
            _plate(m, base, way, across, 0.9 * ka, 0.6 * ka, 0.26 * ka, 0.12 * ka, "steel", taper_th=0.5)
        yard.rust_at.append((p.x, p.y, p.z - 0.3, 5.0 * ka))


# ---- her island -------------------------------------------------------------------------------------------------

def island(yard, sp, zg, fd):
    """The island: a sponson (Soryu's out over her starboard side, clear of the deck; Hiryu's standing in over her port
    edge), three levels (the chart house, the navigating bridge, the compass bridge with its windows all round), the
    rangefinder and the signal lights on top, the tripod mast at its after end with its yard, lookout top and gaff.
    Returns the gaff's end (the ensign's socket)."""
    m = yard.m
    fine = m.fine
    I = sp.island
    hand, iz = I.hand, sp.iz
    x0, x1 = I.spon
    m.use(yard.half((x0 + x1) * 0.5))
    # the sponson: at the flight deck's level its inner edge on the deck's edge, its outer corners rounded; below the
    # deck a block from her hangar's wall out to it, on stays
    xs = [lerp(x0, x1, k / 8.0) for k in range(9)]
    inner = [(x, edge_of(sp, hand, x)) for x in xs]
    r = 1.3
    outer = []
    for cx, a0 in ((x1 - r, 0.0), (x0 + r, 90.0)):
        for k in range(4):
            a = math.radians(a0 + 30.0 * k)
            outer.append((cx + r * math.cos(a), hand * (I.out - r + r * math.sin(a))))
    cap.slab(m, ccw(inner + outer), fd, iz, paint="hull", top="deck")
    wall = hand * min(yard.h.half_at(x) for x in (x0, x1)) * 0.97 - hand * 0.25
    low = ccw([(x1, wall)] + outer + [(x0, wall)])
    cap.slab(m, low, zg - 0.4, fd, paint="hull", top=None, bottom=True)
    wall_mid = hand * yard.h.half_at((x0 + x1) * 0.5) * 0.97
    for dx in ((x0 + 2.0, (x0 + x1) * 0.5, x1 - 2.0) if fine else ((x0 + x1) * 0.5,)):
        m.rod((dx, wall_mid, zg - 3.4), (dx, hand * (I.out - 0.6), zg - 0.4), 0.16, "upper", sides=5)
    if fine:
        from ships import railing
        ya = max(abs(edge_of(sp, hand, x0)), abs(edge_of(sp, hand, x1))) + 0.4
        railing(m, [Vector((px, py, iz)) for px, py in rim(x0 + 0.12, x1 - 0.12, ya, I.out - 0.12, r - 0.12, hand)], high=1.0, every=1.6)
    # the levels
    tz = iz
    yi = I.yi
    tops = []
    for k, (xf, xa, hw, hh) in enumerate(I.levels):
        rr = 1.25 if k else 0.9
        pl = Model.round_rect(xf, xa, yi - hw, yi + hw, rr, 4 if fine else 1)
        cap.slab(m, pl, tz, tz + hh, paint="upper", top="roof" if k == len(I.levels) - 1 else "deck")
        cap.walls_detail(yard, pl, tz, tz + hh, doors=k == 0, scuttles=k == 0, rail=k > 0)
        if fine and k >= 1:
            glaze(m, pl, tz + hh - 1.3, 0.85, keep=(lambda out: out.x > -0.35) if k == 1 else (lambda out: True))
            cap.slab(m, grow(pl, 0.14), tz + hh - 1.4, tz + hh - 1.33, paint="upper", top="upper", bottom=True)      # the sill's ledge
        tops.append((xf, xa, hw, tz + hh))
        tz += hh
    if fine:
        from ships import railing
        # the bridge's wings either side, at the navigating bridge's roof, railed, on brackets; its running lights
        xf1, xa1, hw1, top1 = tops[1]
        for s in (1, -1):
            y0 = yi + s * (hw1 - 0.05)
            y1 = yi + s * (hw1 + 1.3)
            wing = ccw([(xf1 - 0.6, y0), (xf1 - 0.9, y1), (xf1 - 3.4, y1), (xf1 - 3.6, y0)])
            cap.slab(m, wing, top1 - 0.12, top1, paint="upper", top="deck", bottom=True)
            railing(m, [Vector((xf1 - 0.66, y0 + s * 0.15, top1)), Vector((xf1 - 0.94, y1 - s * 0.08, top1)), Vector((xf1 - 3.36, y1 - s * 0.08, top1)),
                        Vector((xf1 - 3.54, y0 + s * 0.15, top1))], high=1.0, every=1.0)
            for bx_ in (xf1 - 1.0, xf1 - 3.2):
                m.rod((bx_, yi + s * (hw1 - 0.02), top1 - 1.2), (bx_, y1 - s * 0.2, top1 - 0.12), 0.05, "upper", sides=4)
            m.box((xf1 - 1.0, yi + s * (hw1 + 0.08), top1 - 1.0), (0.3, 0.16, 0.3), "red" if s * 1.0 == PORT else "green")
        # the compass bridge's canvas dodger laced along the rail round its front, the binnacle behind it
        xf2, xa2, hw2, top2 = tops[2]
        pl2 = Model.round_rect(xf2 - 0.07, xa2 + 0.07, yi - hw2 + 0.07, yi + hw2 - 0.07, 1.18, 4)
        n2 = len(pl2)
        ks = [k for k in range(n2) if (lambda d: d.length > 0.05 and Vector((d.y, -d.x)).normalized().x > 0.25)(Vector(pl2[(k + 1) % n2]) - Vector(pl2[k]))]
        k0 = next(k for k in ks if (k - 1) % n2 not in ks)                    # (the run's first edge, going round)
        run = [Vector(pl2[k0])]
        k = k0
        while k in ks:
            run.append(Vector(pl2[(k + 1) % n2]))
            k = (k + 1) % n2
        dodger(m, run, top2 + 0.04, 0.75)
        # the binnacle: a pedestal in the grey, its brass hood, the two soft-iron spheres either side of it
        bn = Vector((xf2 - 2.1, yi - 0.2, top2))
        m.lathe(bn, Z, [(0.0, 0.22), (0.08, 0.17), (0.72, 0.13), (0.78, 0.2)], "upper", 10)
        m.lathe(bn + Z * 0.78, Z, [(0.0, 0.21), (0.12, 0.22), (0.2, 0.15), (0.24, 0.05)], "brass", 10)
        for s in (1, -1):
            m.rod(bn + Z * 0.7, bn + Z * 0.7 + Y * s * 0.3, 0.025, "upper", sides=4)
            m.ball(bn + Z * 0.74 + Y * s * 0.36, (0.09, 0.09, 0.09), "upper", 8, 6)
    xf, xa, hw, top = tops[-1]
    # on the compass bridge's roof: her Type 94 high-angle director on a short pedestal (from up here it could lay all
    # the 12.7 cm guns), two signal lights, the binocular stands at its front rail
    m.use(yard.half(xf))
    xdir = (xf + xa) * 0.5 - 1.0
    m.lathe((xdir, yi, top), Z, [(0.0, 0.62), (0.3, 0.58), (0.35, 0.5)], "upper", 14 if fine else 6)
    director94(yard, "director.HA" + ("P" if hand == PORT else "S"), xdir, yi, top + 0.35, bearing=hand * -90.0, parent=yard.half(xf))
    m.use(yard.half(xf))
    for s in (1, -1):
        yard.searchlight(xa + 1.0, yi + s * (hw - 0.55), top, 0.35, (0.4, s * 1.0, 0.0))
    if fine:
        for s in (-0.9, 0.0, 0.9):
            b = Vector((xf - 0.6, yi + s, top))
            m.rod(b, b + Z * 1.05, 0.05, "upper", sides=5)
            m.box(b + Z * 1.15 + X * 0.1, (0.35, 0.36, 0.16), "gunmetal")
        fit.sailor(m, Vector((xf - 1.3, yi + 0.9, top)), X, "stand", navy="JAP")
        fit.sailor(m, Vector((xf - 1.3, yi - 0.4, top)), Vector((1.0, -0.3, 0.0)), "stand", navy="JAP")
    # the tripod mast at the after end: the pole, two struts, a lookout's top, the signal yard, the gaff
    mx = I.mast_x
    head = Vector((mx - 0.5, yi, iz + I.mast_high))
    m.lathe((mx, yi, iz), head - Vector((mx, yi, iz)), [(0.0, 0.3), (I.mast_high * 0.6, 0.22), (I.mast_high, 0.09)], "upper", 8 if fine else 5)
    j = Vector((mx - 0.5 * 0.62, yi, iz + I.mast_high * 0.62))
    # (its struts' feet on the island's sponson, both outboard of the flight deck's edge)
    feet = [abs(yi) - 1.5, abs(yi) + 1.5]
    clear = abs(edge_of(sp, hand, mx - 2.2)) + 0.3 - feet[0]
    feet = [v + max(0.0, clear) for v in feet]
    feet[1] = min(feet[1], I.out - 0.4)
    for v in feet:
        m.rod((mx - 2.2, hand * v, iz), j, 0.15, "upper", sides=6 if fine else 4)
    tp = Vector((mx - 0.33, yi, iz + I.mast_high * 0.64))
    m.lathe(tp - Z * 0.12, Z, [(0.0, 0.8), (0.12, 0.8)], "deck", 14 if fine else 6)
    yd = Vector((mx - 0.4, yi, iz + I.mast_high * 0.78))
    m.rod(yd - Y * 3.4, yd + Y * 3.4, 0.07, "upper", rb=0.07, sides=4)
    gaff = Vector((mx - 3.4, yi, iz + I.mast_high * 0.7))
    m.rod(Vector((mx - 0.42, yi, iz + I.mast_high * 0.66)), gaff, 0.06, "upper", sides=4)
    if fine:
        from ships import railing
        railing(m, [tp + Vector((0.7 * math.cos(a), 0.7 * math.sin(a), 0.0)) for a in [2 * math.pi * k / 10 for k in range(11)]], high=0.9, every=0.8)
        for s in (1, -1):
            yard.wire(yd + Y * s * 3.2, Vector((mx - 8.0, yi + s * 1.6, top - 1.2)), 0.018)
        yard.wire(head, Vector((xf, yi, top + 0.2)), 0.02)
        yard.wire(head, gaff, 0.018)
        for s in (1, -1):                              # the signal halyards' blocks under the yard
            m.box(yd + Y * s * 2.6 - Z * 0.15, (0.12, 0.12, 0.2), "steel")
    # ladders up the island's after face
    if fine:
        from ships import ladder
        xf0, xa0, hw0, hh0 = I.levels[0]
        ladder(m, Vector((xa0, yi - 1.0, iz)), iz + hh0, (-1, 0, 0))
        ladder(m, Vector((I.levels[1][1], yi + 0.8, iz + hh0)), iz + hh0 + I.levels[1][3], (-1, 0, 0))
    return gaff


# ---- her deck's kana ------------------------------------------------------------------------------------------

KANA = {
    # strokes in the character's own square (u across, to the pilot's right; v up it, toward her bow)
    "sa": [[(0.04, 0.68), (0.5, 0.69), (0.96, 0.7)], [(0.3, 0.94), (0.3, 0.72), (0.3, 0.48)],
           [(0.7, 0.95), (0.7, 0.66), (0.69, 0.44), (0.63, 0.28), (0.52, 0.15), (0.36, 0.06), (0.2, 0.02)]],
    "hi": [[(0.27, 0.52), (0.55, 0.6), (0.82, 0.68)], [(0.27, 0.96), (0.27, 0.5), (0.27, 0.18), (0.31, 0.08), (0.42, 0.05), (0.62, 0.05), (0.88, 0.07)]],
}


def kana(m, which, xc, yc, z, size):
    """Her identification kana in white strokes on the after deck, upright for a pilot coming in from astern."""
    def P(u, v):
        return Vector((xc + (v - 0.5) * size, yc - (u - 0.5) * size, z))
    w = 0.1 * size * 0.5
    for st in KANA[which]:
        pts = [P(u, v) for u, v in st]
        m.strip(pts, [Z] * len(pts), w, "white", 0.045)
        for p in (pts[0], pts[-1]):                      # (round ends, so a stroke's end and a turn read as painted)
            m.disc(p + Z * 0.046, Z, w, "white", 10)


# ---- the ship --------------------------------------------------------------------------------------------------

def carrier(sp, fine=True):
    paints = livery("JAP", "carrier")
    paints.update(PAINT)
    paints = fit.crew_paints(paints, "JAP")
    m = Model(sp.name, sp.title, "ship", paints, fine)
    h = cap.PlanHull(sp.plan, L=sp.L, B=sp.B, D=sp.D, T=sp.T, sheer=sp.sheer, sheer_aft=sp.sheer_aft, rake=sp.rake, counter=sp.counter,
                     flare=sp.flare, fine=1.65, full=0.7, wl_fore=0.5, bilge=6.0)
    bx = sp.bx
    yard = Yard(m, h, break_x=bx)
    yard.soot_at, yard.rust_at = [], []
    yard.hull("deck")
    D = h.deck_z
    iz = sp.iz
    fd = iz - sp.thick                       # the flight deck's underside
    zg = fd - 1.4                            # the 12.7 cm sponsons' platforms, under the deck's edge
    zc = fd - 0.75                           # the catwalks along her deck's edges
    hz = iz - D(0.0) - 0.4                   # how far a lift goes down to the upper hangar's deck
    hx0, hx1 = sp.hangar
    I = sp.island
    deck_fore, deck_aft = sp.port[-1][0], sp.port[0][0]
    star = [(x, -y) for x, y in sp.port]

    def w0(x):
        return h.half_at(x) * 0.97

    # what stands out from her sides, so that the galleries and the brackets leave room for them: (hand, x0, x1)
    busy = []
    for fx, r in sp.funnels:
        busy.append((STBD, fx - r - 1.6, fx + r + 0.6))
    busy.append((I.hand, I.spon[0] - 0.3, I.spon[1] + 0.3))
    for gx in sp.guns:
        for hand in (PORT, STBD):
            busy.append((hand, gx - 3.6, gx + 3.6))
    for hand, group in sp.aa:
        xs_ = [gx for gx, _ in group]
        busy.append((hand, min(xs_) - 2.6, max(xs_) + 2.6))
    for hand, dx in sp.hadirs:
        busy.append((hand, dx - 2.1, dx + 2.1))
    for hand, dx in sp.aadirs:
        busy.append((hand, dx - 1.9, dx + 1.9))
    for hand, dx in sp.lights:
        busy.append((hand, dx - 1.7, dx + 1.7))

    def free(hand, x, pad=0.0):
        return not any(hd == hand and a - pad <= x <= b + pad for hd, a, b in busy)

    # ---- the hangar's walls from her upper deck to the flight deck, following her sides, in two halves at her break
    for x0, x1, part in ((bx, hx0, "fore"), (hx1, bx, "aft")):
        xs = [lerp(x0, x1, k / 24.0) for k in range(25)]
        plan = cap.sym([(x1, 0.0)] + [(x, w0(x)) for x in reversed(xs)] + [(x0, 0.0)])
        m.use(part)
        cap.slab(m, plan, D(bx) - 0.5, fd, paint="hull", top=None)
    if fine:
        # a row of scuttles along the hangar's walls (the crew's spaces outboard of the hangar), a few doors, the
        # line of the upper deck's edge
        x = hx1 + 4.0
        k = 0
        while x < hx0 - 3.0:
            for hand in (PORT, STBD):
                if not free(hand, x, 0.6):
                    continue
                if any(abs(x - bxx) < bl * 0.5 + 1.2 for bxx, bl in sp.bays):
                    continue
                yy = hand * (w0(x) + 0.02)
                m_ = yard.on(x)
                if k % 6 == 3:
                    hull_door(m_, x, yy, D(x) + 0.15, hand)
                else:
                    m_.disc((x, yy + hand * 0.005, D(x) + 1.75), (0, hand, 0), 0.17, "glass", 8)
                    if k % 2 == 0:
                        m_.disc((x, yy + hand * 0.005, fd - 1.0), (0, hand, 0), 0.17, "glass", 8)
                    elif k % 6 == 1:
                        louvre(m_, x, yy, fd - 1.1, hand)
            x += 3.1
            k += 1
        for a, b in ((hx1 + 0.5, bx - 0.05), (bx + 0.05, hx0 - 0.5)):
            for hand in (PORT, STBD):
                xs = [lerp(a, b, t / 12.0) for t in range(13)]
                m.use(yard.half((a + b) * 0.5))
                m.strip([Vector((x, hand * (w0(x) + 0.03), D(x) + 0.25)) for x in xs], [Vector((0, hand, 0))] * 13, 0.06, "seam", 0.0)
    # the hangar's fore and after bulkheads: the windows of the stations under the flight deck's fore end, doors aft
    m.use("fore")
    if fine:
        wf = w0(hx0)
        for i in range(-3, 4):
            yy = i * min(1.25, wf / 4.0)
            m.box((hx0 + 0.03, yy, fd - 1.05), (0.06, 0.85, 0.7), "glass")
        for s in (1, -1):
            m.box((hx0 + 0.03, s * wf * 0.62, D(hx0) + 1.05), (0.06, 0.8, 1.9), "door")
        m.use("aft")
        for s in (1, -1):
            m.box((hx1 - 0.03, s * w0(hx1) * 0.5, D(hx1) + 1.05), (0.06, 0.8, 1.9), "door")
            m.disc((hx1 - 0.02, s * w0(hx1) * 0.25, D(hx1) + 1.8), -X, 0.17, "glass", 8)

    # ---- the catwalks: a walkway along each side just under the flight deck's edge, out past it, on brackets from the
    # hangar's wall, railed; broken where a sponson, a platform or a funnel stands out
    from ships import railing
    for hand in (PORT, STBD):
        edge = sp.port if hand == PORT else star
        xs = [x for x in [hx1 + 0.6 + 0.8 * k for k in range(int((hx0 - hx1 - 1.2) / 0.8) + 1)] if free(hand, x, 0.4)]
        runs, cur = [], []
        for x in xs:
            if cur and (x - cur[-1] > 1.0 or (cur[-1] < bx <= x)):
                runs.append(cur)
                cur = []
            cur.append(x)
        if cur:
            runs.append(cur)
        for run in runs:
            a, b = run[0], run[-1]
            if b - a < 1.5:
                continue
            n_ = max(2, int((b - a) / 2.5))
            pts = [lerp(a, b, t / n_) for t in range(n_ + 1)]
            m.use(yard.half((a + b) * 0.5))
            wl = [Vector((x, hand * (abs(cap.edge_at(edge, x)) - 0.35), zc)) for x in pts]
            wr = [Vector((x, hand * (abs(cap.edge_at(edge, x)) + 1.05), zc)) for x in pts]
            m.sheet([wl, wr], "deck", Z, smooth=False)
            m.sheet([[p - Z * 0.08 for p in wl], [q - Z * 0.08 for q in wr]], "upper", -Z, smooth=False)
            m.strip([q - Z * 0.04 for q in wr], [Vector((0, hand, 0))] * len(wr), 0.045, "upper", 0.0)
            if fine:
                railing(m, [q - Vector((0, hand * 0.06, 0)) for q in wr], high=1.0, every=2.5)
                for x in pts:
                    e = abs(cap.edge_at(edge, x))
                    m.rod((x, hand * w0(x), zc - 1.9), (x, hand * (e + 0.85), zc - 0.08), 0.06, "upper", sides=4)
    # brackets under the flight deck's overhang
    if fine:
        x = deck_fore - 12.0
        while x > deck_aft + 8.0:
            for hand in (PORT, STBD):
                e = abs(edge_of(sp, hand, x))
                if hx1 < x < hx0 and e - w0(x) > 0.6 and free(hand, x, 0.3):
                    yard.on(x).rod((x, hand * w0(x), fd - 1.2), (x, hand * (e - 0.3), fd - 0.04), 0.09, "upper", sides=4)
            x -= 6.0

    # ---- the flight deck, its pillars over the open bow and stern, its tie-down strips, lines, wires and barriers
    for x0, x1, part in ((bx, deck_fore, "fore"), (deck_aft, bx, "aft")):
        m.use(part)
        cap.slab(m, cap.cut_deck(sp.port, star, x0, x1), fd, iz, paint="upper", top="flight", bottom=True)
    for px in (hx0 + 9.0, deck_fore - 4.8):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.55, 4.0)
            yard.on(px).rod((px, yy, D(px) - 0.05), (px, yy * 1.2, fd), 0.3, "upper", sides=8 if fine else 5)
    for px in (hx1 - 6.0, hx1 - 13.0, deck_aft + 4.0):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.6, 5.0)
            yard.on(px).rod((px, yy, D(px) - 0.05), (px, yy * 1.25, fd), 0.3, "upper", sides=8 if fine else 5)
    lifts = [(lx, ly, ll, lw, "flight") for lx, ly, ll, lw in sp.lifts]
    # where her range of aircraft (`deckpark`) stands: the foremost machines' airscrews just abaft the middle lift, the
    # last tails just forward of the after lift, so that every lift may work with the range on deck
    park_x = sp.lifts[1][0] - sp.lifts[1][2] * 0.5 - RANGE_FWD - 0.5
    assert park_x - RANGE_AFT > sp.lifts[2][0] + sp.lifts[2][2] * 0.5 + 0.3, "the range would stand over the after lift"

    def on_lift(x, y=0.0, pad=0.0):
        return any(abs(x - lx) < ll * 0.5 + pad and abs(y - ly) < lw * 0.5 + pad for lx, ly, ll, lw, _ in lifts)
    kana_x = deck_aft + 14.0
    if fine:
        # the steel tie-down strips across the planking, every 5.6 m
        x = deck_fore - 3.0
        while x > deck_aft + 2.0:
            e = min(abs(cap.edge_at(sp.port, x)), abs(cap.edge_at(star, x))) - 0.35
            m_ = yard.on(x)
            if e > 1.0:
                cuts = sorted([(ly - lw * 0.5 - 0.3, ly + lw * 0.5 + 0.3) for lx, ly, ll, lw, _ in lifts if abs(x - lx) < ll * 0.5 + 0.3])
                if I.hand == PORT and I.spon[0] - 0.5 < x < I.spon[1] + 0.5:
                    hi_ = I.yi - I.levels[0][2] - 0.2
                else:
                    hi_ = e
                lo_ = -e
                segs, a = [], lo_
                for c0, c1 in cuts:
                    segs.append((a, min(c0, hi_)))
                    a = max(a, c1)
                segs.append((a, hi_))
                for a_, b_ in segs:
                    if b_ - a_ > 0.5:
                        m_.box((x, (a_ + b_) * 0.5, iz + 0.01), (0.09, b_ - a_, 0.02), "steel")
            x -= 5.6
    # the windbreak ahead of the forward lift (lowered flush into the deck)
    xwb0 = sp.lifts[0][0] + sp.lifts[0][2] * 0.5 + 1.5
    xwb1 = xwb0 + 1.6
    x = deck_fore - 6.0
    while x > deck_aft + 20.0:      # the centre line, dashed
        if not on_lift(x, 0.0, 3.0) and not abs(x - kana_x) < 6.5 and not xwb0 - 3.0 < x < xwb1 + 3.0:
            yard.on(x).box((x, 0, iz + 0.02), (5.0, 0.4, 0.03), "line")
        x -= 9.0
    for hand in (PORT, STBD):        # two lines along her, one each side of the centre line, broken at the lifts
        for a, b in ((deck_aft + 8.0, bx - 0.3), (bx + 0.3, deck_fore - 18.0)):
            pieces, last = [], a
            for lx, _, ll, _, _ in sorted(lifts + [((xwb0 + xwb1) * 0.5, 0.0, xwb1 - xwb0, 0.0, "")]):
                if a < lx < b:
                    pieces.append((last, lx - ll * 0.5 - 0.6))
                    last = lx + ll * 0.5 + 0.6
            pieces.append((last, b))
            for p0, p1 in pieces:
                if p1 - p0 > 1.0:
                    yard.on((p0 + p1) * 0.5).box(((p0 + p1) * 0.5, hand * 3.8, iz + 0.02), (p1 - p0, 0.3, 0.03), "line")
    m.use("aft")
    aft_edge = [(y, x) for x, y in sp.port[:7]]       # (the after edge: where the deck ends, by y)
    half_aft = sp.port[6][1]
    for k in range(-5, 6):           # the red and white stripes on her after end
        yy = k * 2.2
        if abs(yy) + 1.1 > half_aft - 0.2:
            continue
        x0 = cap.edge_at(aft_edge, abs(yy) + 1.1) + 0.25
        x1 = deck_aft + 6.0
        if x0 < x1 - 0.5:
            m.box(((x0 + x1) * 0.5, yy, iz + 0.025), (x1 - x0, 2.2 if k % 2 == 0 else 2.18, 0.03), "red" if k % 2 == 0 else "white")
    kana(m.use(yard.half(kana_x)), sp.kana, kana_x, 0.0, iz, 8.0)
    seam_ys = [j * 0.62 + 0.31 for j in range(-23, 23)]
    l0f, l0a, l0w, _ = I.levels[0]

    def on_deck_island(x, y):
        return I.hand == PORT and l0a - 0.6 < x < l0f + 0.6 and y > I.yi - l0w - 0.15
    if fine:
        # the planking's seams, fore and aft along her, broken at the lifts, at her break and at an island on the deck
        for yy in seam_ys:
            xs = [deck_aft + 0.25 * k for k in range(int((deck_fore - deck_aft) / 0.25) + 1)]
            ok = [x for x in xs if abs(yy) < min(abs(cap.edge_at(sp.port, x)), abs(cap.edge_at(star, x))) - 0.3]
            if len(ok) < 2:
                continue
            cuts = [(bx - 0.02, bx + 0.02)]
            cuts += [(lx - ll * 0.5 - 0.12, lx + ll * 0.5 + 0.12) for lx, ly, ll, lw, _ in lifts if abs(yy - ly) < lw * 0.5 + 0.12]
            if on_deck_island(l0a, yy):
                cuts.append((l0a - 0.6, l0f + 0.6))
            a = ok[0]
            for c0, c1 in sorted(cuts) + [(ok[-1], ok[-1])]:
                b = min(c0, ok[-1])
                if b - a > 0.4:
                    m_ = yard.on((a + b) * 0.5)
                    w = 0.018
                    zs_ = iz + 0.012
                    m_.add([Vector((a, yy - w, zs_)), Vector((b, yy - w, zs_)), Vector((b, yy + w, zs_)), Vector((a, yy + w, zs_))],
                           [(0, 1, 2, 3)], "plankline", False, lambda q_: Z)
                a = max(a, c1)
        # the beams under the flight deck where it stands out over the open forecastle and quarterdeck
        for xa_, xb_ in ((hx0 + 0.6, deck_fore - 0.6), (deck_aft + 0.6, hx1 - 0.6)):
            x = xa_ + 1.0
            while x < xb_:
                if not (deck_fore - 4.2 < x):
                    e = min(abs(cap.edge_at(sp.port, x)), abs(cap.edge_at(star, x))) - 0.45
                    yard.on(x).box((x, 0.0, fd - 0.2), (0.24, 2.0 * e, 0.44), "upper")
                x += 2.4
            for gy in (-5.0, 0.0, 5.0):
                b_ = min(xb_, deck_fore - 4.2)
                if b_ - xa_ > 1.0:
                    yard.on((xa_ + b_) * 0.5).box(((xa_ + b_) * 0.5, gy, fd - 0.28), (b_ - xa_, 0.3, 0.6), "upper")
    for xw in sp.wires:              # the arrester wires on their sheaves
        m.use(yard.half(xw))
        w_ = min(cap.edge_at(sp.port, xw), -cap.edge_at(star, xw)) - 1.8
        m.rod((xw, -w_, iz + 0.12), (xw, w_, iz + 0.12), 0.035, "steel", sides=3, smooth=False)
        for hand in (PORT, STBD):
            m.lathe((xw, hand * w_, iz), Z, [(0.0, 0.22), (0.18, 0.18)], "steel", 8 if fine else 4)
            if fine:
                m.box((xw, hand * (w_ + 0.45), iz + 0.06), (0.5, 0.5, 0.12), "steel")
    for xb in sp.barriers:           # the crash barriers, lowered
        m.use(yard.half(xb))
        lo_ = -min(11.0, abs(cap.edge_at(star, xb)) - 1.5)
        hi_ = min(11.0, abs(cap.edge_at(sp.port, xb)) - 1.5)
        if I.hand == PORT and I.spon[0] - 1.0 < xb < I.spon[1] + 1.0:
            hi_ = min(hi_, I.yi - I.levels[0][2] - 0.5)
        m.rod((xb, lo_, iz + 0.18), (xb, hi_, iz + 0.18), 0.05, "steel", sides=4, smooth=False)
        for yy in (lo_, hi_):
            m.box((xb, yy - math.copysign(0.4, yy), iz + 0.25), (0.4, 0.4, 0.5), "steel")
    if fine:
        # the windbreak: hinged panels of steel slats, lowered into a shallow well across the deck ahead of the forward
        # lift (raised, they sheltered the aircraft ranged behind them), a hinge at the fore edge of each panel
        e = min(abs(edge_of(sp, PORT, xwb1)), abs(edge_of(sp, STBD, xwb1))) - 1.4
        m.use(yard.half(xwb0))
        m.box(((xwb0 + xwb1) * 0.5, 0.0, iz + 0.007), (xwb1 - xwb0, 2.0 * e, 0.014), "black")
        for k in range(7):
            m.box((lerp(xwb0 + 0.12, xwb1 - 0.12, k / 6.0), 0.0, iz + 0.013), (0.11, 2.0 * e - 0.1, 0.026), "upper")
        npan = max(2, round(2.0 * e / 2.3))
        for k in range(npan + 1):
            yy = lerp(-e + 0.04, e - 0.04, k / npan)
            m.box(((xwb0 + xwb1) * 0.5, yy, iz + 0.013), (xwb1 - xwb0, 0.08, 0.026), "upper")
            if k < npan:
                for t in (0.25, 0.75):
                    m.box((xwb1 + 0.06, lerp(yy, yy + 2.0 * e / npan, t), iz + 0.03), (0.22, 0.2, 0.06), "steel")
    m.use("fore")                     # the steam jet on her centre line forward that shows the pilots the wind
    m.lathe((deck_fore - 2.0, 0.0, iz), Z, [(0.0, 0.25), (0.08, 0.18), (0.35, 0.06)], "steel", 8)
    if fine:                          # the landing lights out at her deck's edges aft: red abaft, green forward of them
        for lx_, paint in ((deck_aft + 8.5, "red"), (deck_aft + 15.0, "green")):
            for hand in (PORT, STBD):
                e = edge_of(sp, hand, lx_)
                m.use(yard.half(lx_))
                m.box((lx_, e + hand * 0.6, fd - 0.2), (1.4, 1.2, 0.12), "upper")
                for dx in (-0.4, 0.4):
                    m.box((lx_ + dx, e + hand * 0.8, fd + 0.1), (0.3, 0.3, 0.45), "upper")
                    m.disc((lx_ + dx, e + hand * 0.8, fd + 0.33), Z, 0.12, paint, 8)

    # ---- the island and its mast
    gaff = island(yard, sp, zg, fd)

    # ---- the funnels turned down over the starboard side
    for fx, r in sp.funnels:
        downtake(yard, fx, -w0(fx) + 0.4, fd - 2.4, r, out=6.0, drop=4.4)

    # ---- six twin 12.7 cm on sponsons below the flight deck's edge, two pairs forward and one aft, and the Type 94
    # directors that lay them, one each side
    n_p = n_s = 0
    for gx in sp.guns:
        for hand in (PORT, STBD):
            e = abs(edge_of(sp, hand, gx))
            yc = hand * (e + 1.9)
            round_sponson(yard, gx, yc, zg, 3.3, hand * w0(gx), hand)
            if hand == PORT:
                n_p += 1
                nm = f"sec.P{n_p}"
            else:
                n_s += 1
                nm = f"sec.S{n_s}"
            bearing = 0.0 if gx > 0 else 180.0
            lim = (-165, 20) if (hand == PORT) == (gx > 0) else (-20, 165)
            twin127(yard, nm, gx, yc, zg, bearing, lim)
    for hand, dx in sp.hadirs:
        e = abs(edge_of(sp, hand, dx))
        yc = hand * (e + 1.3)
        round_sponson(yard, dx, yc, fd - 0.6, 1.7, hand * w0(dx), hand)
        director94(yard, "director.HA" + ("P" if hand == PORT else "S"), dx, yc, fd - 0.6, bearing=hand * -90.0)
        if fine:
            for s in (1, -1):
                fit.sailor(m.use(yard.half(dx)), Vector((dx + s * 0.55, yc + hand * 1.15, fd - 0.6)), Vector((0, hand, 0)), "stand", navy="JAP")

    # ---- the 25 mm: on platforms along her sides at the flight deck's edge, and under the fore end of her deck
    q = 0
    zp = fd - 0.95                      # (the platforms' deck: the guns' muzzles about level with the flight deck)
    for hand, group in sp.aa:
        xs_ = [gx for gx, _ in group]
        x0, x1 = min(xs_) - 2.3, max(xs_) + 2.3
        e_out = max(abs(edge_of(sp, hand, x)) for x in (x0, x1, (x0 + x1) * 0.5))
        y_in = min(w0(x) for x in (x0, x1)) - 0.25
        y_out = e_out + 3.4
        m.use(yard.half((x0 + x1) * 0.5))
        pl = Model.round_rect(x1, x0, hand * y_in, hand * y_out, 1.4, 3 if fine else 1)
        cap.slab(m, pl, zp - 0.3, zp, paint="upper", top="deck", bottom=True)
        for bxx in ([x0 + 1.0, (x0 + x1) * 0.5, x1 - 1.0] if fine else [(x0 + x1) * 0.5]):
            m.rod((bxx, hand * w0(bxx), zp - 2.6), (bxx, hand * (y_out - 0.8), zp - 0.3), 0.09, "upper", sides=4)
        if fine:
            bulwark(m, rim(x0 + 0.05, x1 - 0.05, e_out - 0.1, y_out - 0.05, 1.35, hand)[::-1], zp, 0.9)
            for ga, gb in zip(xs_, xs_[1:]):         # (two life rafts lashed outside the bulwark between each pair of guns)
                fit.float_rack(yard, (ga + gb) * 0.5, hand * (y_out - 0.03), zp + 0.12, (0, hand, 0), n=2, long=2.2, wide=0.95)
        for gx, guns in group:
            q += 1
            yy = hand * (abs(edge_of(sp, hand, gx)) + 1.75)
            bearing = (90.0 if hand == STBD else -90.0) + (-25.0 if gx > 0 else 25.0) * (1 if hand == STBD else -1)
            aa25(yard, q, gx, yy, zp, bearing=bearing, guns=guns)
    # the bow platform: just under the flight deck's fore end, on pillars from the forecastle, the guns' pivots a hand's
    # breadth ahead of the deck's end and the director behind them under it. On Soryu's low forecastle it stands 2.5 m
    # under the deck, room for a loader holding his clip up over his head; on Hiryu's, a deck higher, it stands just
    # clear of the forecastle, and her loaders under the deck's end hold their clips at the chest
    zb = fd - 2.5
    xm = deck_fore + 0.2
    xd = deck_fore - 2.5                                  # (the director: behind the guns, under the deck's end)
    xb0, xb1 = deck_fore - 3.6, xm + 2.1
    zb = max(zb, max(D(lerp(xb0, xb1, k / 8.0)) for k in range(9)) + 0.55)
    wb = max(abs(y) for y, _ in sp.bow_aa) + 2.2
    m.use("fore")
    pl = Model.round_rect(xb1, xb0, -wb, wb, 1.6, 3 if fine else 1)
    cap.slab(m, pl, zb - 0.3, zb, paint="upper", top="deck", bottom=True)
    for s in (1, -1):
        for px in (xb0 + 1.2, xb1 - 1.6):
            m.rod((px, s * 1.4, D(px) - 0.05), (px, s * 1.4, zb - 0.3), 0.2, "upper", sides=8 if fine else 5)
    if fine:
        railing(m, [Vector((v, u, zb)) for u, v in rim(-wb + 0.06, wb - 0.06, xb0 + 0.5, xb1 - 0.06, 1.54, 1)], high=1.0, every=1.4)
        fit.stair(m, (xb0 - 1.7, -wb + 1.0, D(xb0 - 1.7) + 0.02), (xb0 + 0.05, -wb + 1.0, zb), wide=0.7)
    for yy, guns in sp.bow_aa:
        q += 1
        aa25(yard, q, xm, yy, zb, bearing=0.0 if abs(yy) < 1.0 else math.copysign(-20.0, yy), guns=guns, overhead=fd)
    # the Type 95 directors for the 25 mm: on the side platforms, and one on the bow platform
    for k, (hand, dx) in enumerate(sp.aadirs):
        e = abs(edge_of(sp, hand, dx))
        yc = hand * (e + 1.0)
        round_sponson(yard, dx, yc, zp + 0.4, 1.45, hand * w0(dx), hand)
        director95(yard, f"director.AA{k + 1}", dx, yc, zp + 0.4, bearing=hand * -90.0)
    director95(yard, f"director.AA{len(sp.aadirs) + 1}", xd, 0.0, zb, bearing=0.0)

    # ---- searchlights on small sponsons at the deck's edge
    for hand, lx in sp.lights:
        e = abs(edge_of(sp, hand, lx))
        yc = hand * (e + 1.0)
        round_sponson(yard, lx, yc, fd - 1.1, 1.3, hand * w0(lx), hand)
        yard.searchlight(lx, yc, fd - 1.1, 0.55, (0.2, hand * 1.0, 0.15))

    # ---- the radio masts each side, lowered outboard for flying, on their hinges at the galleries
    for mxx in sp.masts:
        for hand in (PORT, STBD):
            foot = Vector((mxx, hand * (abs(edge_of(sp, hand, mxx)) + 0.55), zc + 0.2))
            tip = foot + Vector((0.0, hand * 12.5, 1.2))
            m_ = yard.on(mxx)
            m_.box(foot - Z * 0.1, (0.9, 0.5, 0.2), "steel")
            for dx in (-0.35, 0.35):
                m_.rod(foot + X * dx, tip + X * dx * 0.3, 0.07, "upper", sides=4)
            if fine:
                for k in range(1, 12):
                    a = foot.lerp(tip, k / 12.0)
                    m_.rod(a - X * 0.33 * lerp(1.0, 0.3, k / 12.0), a + X * 0.33 * lerp(1.0, 0.3, k / 12.0), 0.025, "upper", sides=3, smooth=False)
                m_.lathe(tip, Vector((0, hand, 0.1)), [(0.0, 0.05), (2.0, 0.02)], "upper", 4)

    # ---- boats: in bays in her sides abaft the funnels, and two under the after overhang
    for bxx, bl in sp.bays:
        for hand in (PORT, STBD):
            yw = hand * w0(bxx)
            m_ = yard.on(bxx)
            m_.box((bxx, yw + hand * 0.02, D(bxx) + 1.5), (bl + 1.6, 0.06, 2.5), "black")
            if fine:                                  # the bay's frame: a lintel over it, a jamb each end, a sill
                m_.box((bxx, yw + hand * 0.1, D(bxx) + 2.82), (bl + 2.2, 0.22, 0.18), "upper")
                m_.box((bxx, yw + hand * 0.1, D(bxx) + 0.18), (bl + 2.2, 0.22, 0.14), "upper")
                for s_ in (1, -1):
                    m_.box((bxx + s_ * (bl * 0.5 + 0.95), yw + hand * 0.1, D(bxx) + 1.5), (0.22, 0.22, 2.7), "upper")
            yard.boat(bxx, yw + hand * 0.3, D(bxx) + 0.5, bl)
            if fine:
                for dx in (-bl * 0.3, bl * 0.3):
                    m_.rod((bxx + dx, yw, D(bxx) + 2.55), (bxx + dx, yw + hand * 0.9, D(bxx) + 2.55), 0.07, "upper", sides=4)
                    m_.rod((bxx + dx, yw + hand * 0.9, D(bxx) + 2.55), (bxx + dx, yw + hand * 0.9, D(bxx) + 1.4), 0.025, "rope", sides=3, smooth=False)
                m_.box((bxx, yw + hand * 0.04, D(bxx) + 2.85), (bl + 1.8, 0.12, 0.15), "upper")
    for hand in (PORT, STBD):
        yard.boat(hx1 - 9.5, hand * 2.3, D(hx1 - 9.5) + 0.55, 7.5)

    # ---- her flight deck's crew: officers by the island facing it, handlers by the lifts and abaft the after one, all
    # clear of where her range of aircraft stands, of the lifts, the wires' sheaves and each other
    if fine:
        rg = random.Random(1941 if sp.name == "Soryu" else 1942)
        ix = (I.levels[0][0] + I.levels[0][1]) * 0.5
        iy = I.yi - I.hand * (I.levels[0][2] + 2.2)
        l1, l2, l3 = sp.lifts
        spots = [(ix, iy, 4, True), (l1[0] + l1[2] * 0.5 + 7.5, 3.0, 3, False), (l2[0], 10.0, 2, False), (l2[0], -10.0, 2, False),
                 (l3[0] - l3[2] * 0.5 - 5.0, 9.0, 2, False), (l3[0] - l3[2] * 0.5 - 5.0, -9.0, 2, False)]
        placed = []

        def clear_spot(q):
            if on_lift(q.x, q.y, 0.8) or abs(q.y) > abs(cap.edge_at(sp.port, q.x)) - 1.0 or on_deck_island(q.x, q.y + 0.5):
                return False
            if park_x - RANGE_AFT - 0.6 < q.x < park_x + RANGE_FWD + 0.6 or any(abs(q.x - xw) < 0.6 for xw in sp.wires):
                return False
            return all((q - p_).length > 1.0 for p_ in placed)
        for cx, cy, k_, officers in spots:
            for i in range(k_):
                foot = None
                for _ in range(40):
                    q = Vector((cx + rg.uniform(-3.0, 3.0), cy + rg.uniform(-2.0, 2.0), iz + 0.035))
                    if clear_spot(q):
                        foot = q
                        break
                if foot is None:
                    continue
                placed.append(foot)
                if officers:                          # (facing the island, give or take)
                    a = math.atan2(I.yi - foot.y, ix - foot.x) + rg.uniform(-0.5, 0.5)
                else:
                    a = rg.uniform(0, 2 * math.pi)
                fit.sailor(m.use(yard.half(foot.x)), foot, Vector((math.cos(a), math.sin(a), 0)), "stand", navy="JAP")

    # ---- forecastle and quarterdeck: anchor gear, bollards, fairleads, rails, scuttles, staffs; her screws
    ground_tackle(yard, h.L * 0.466, h.L * 0.425, spread=2.2)
    for bx_ in (h.L * 0.475, h.L * 0.45, -h.L * 0.44, -h.L * 0.475):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.8), D(bx_))
    yard.rails(hx0 + 1.5, deck_fore - 4.4)
    yard.rails(deck_fore + 4.4, h.L * 0.488)
    yard.rails(-h.L * 0.49, hx1 - 1.0)
    yard.portholes(-h.L * 0.38, h.L * 0.42, 1.7, every=3.4, skip=[(fx - 3.0, fx + 3.0) for fx, _ in sp.funnels])
    yard.portholes(-h.L * 0.36, h.L * 0.4, 4.1, every=3.4)
    if fine:
        # her side's plating: the seams of three strakes along her, laid on her skin, clear of the break
        gap = yard.tear + 0.6
        for zs in (1.6, 4.45, D(0.0) - 0.75):
            for hand in (PORT, STBD):
                for a, b in ((-h.L * 0.43, bx - gap), (bx + gap, h.L * 0.43)):
                    n_ = max(2, int((b - a) / 3.0))
                    pts, nrm = [], []
                    for k in range(n_ + 1):
                        x = lerp(a, b, k / n_)
                        if zs > D(x) - 0.4:
                            continue
                        q, nn = h.side(x, zs, hand)
                        pts.append(q)
                        nrm.append(nn)
                    if len(pts) > 1:
                        m.use(yard.half((a + b) * 0.5))
                        m.strip(pts, nrm, 0.03, "seam", 0.008)
        # the accommodation ladders, stowed along her side under the upper deck's edge, and the boat booms
        for hand in (PORT, STBD):
            for lx0, ln in ((24.0, 7.5), (-64.0, 7.5)):
                zl = D(lx0) - 1.25
                q0, n0 = h.side(lx0, zl, hand)
                q1, n1 = h.side(lx0 - ln, zl, hand)
                m_ = yard.on(lx0 - ln * 0.5)
                for dz in (0.0, 0.62):
                    m_.rod(q0 + n0 * 0.28 + Z * dz, q1 + n1 * 0.28 + Z * dz, 0.04, "upper", sides=4)
                for k in range(1, int(ln / 0.42)):
                    t = k / int(ln / 0.42)
                    c = q0.lerp(q1, t) + n0.lerp(n1, t) * 0.28
                    m_.rod(c, c + Z * 0.62, 0.02, "upper", sides=3, smooth=False)
                for t in (0.15, 0.85):
                    c = q0.lerp(q1, t)
                    nn = n0.lerp(n1, t)
                    m_.box(c + nn * 0.14 + Z * 0.31, (0.12, 0.12, 0.7), "steel", yaw=math.degrees(math.atan2(nn.y, nn.x)))
            bq0, bn0 = h.side(38.0, D(38.0) - 0.55, hand)
            bq1, bn1 = h.side(26.0, D(26.0) - 0.55, hand)
            m_ = yard.on(32.0)
            m_.rod(bq0 + bn0 * 0.2, bq1 + bn1 * 0.2, 0.08, "upper", rb=0.05, sides=5)
            m_.box(bq0 + bn0 * 0.1, (0.3, 0.3, 0.3), "steel", yaw=math.degrees(math.atan2(bn0.y, bn0.x)))
            m_.box(bq1 + bn1 * 0.12, (0.2, 0.25, 0.25), "steel", yaw=math.degrees(math.atan2(bn1.y, bn1.x)))
    m.use("aft")
    sx = -h.L * 0.497
    m.rod((sx + 0.8, 0, D(sx + 0.8)), (sx - 0.6, 0, D(sx + 0.8) + 3.0), 0.06, "upper", sides=4)
    m.use("fore")
    jx = h.L * 0.493
    m.rod((jx, 0, D(jx)), (jx + 0.4, 0, D(jx) + 2.6), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.4, 0, D(jx) + 2.6), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=sp.screws, r=sp.screw_r, rudders=1)

    cap.carrier_finish(yard, lifts, deck_fore, deck_aft, iz, hz, [{"model": "DeckPark_Zero"}], deck_fore - 8.0, deck_aft + 8.0, gaff, yard.half(I.mast_x))
    dp = m.parts[m.part_at["deckpark"]]
    dp.pivot = Vector((park_x, 0.0, iz))
    dp.note = ("where her aircraft are ranged: the range named in `deck_park` stands at this pivot (its x forward) and rides with "
               "this part; it stands between the middle and the after lifts, clear of both, so that every lift may work with the range on deck")
    if fine:                          # the lifts are planked as the deck is
        for i, (lx, ly, ll, lw, _) in enumerate(lifts):
            m.use(f"lift.{i + 1}")
            for yy in seam_ys:
                if abs(yy - ly) < lw * 0.5 - 0.35:
                    a, b, w, zl = lx - ll * 0.5 + 0.3, lx + ll * 0.5 - 0.3, 0.018, iz + 0.047
                    m.add([Vector((a, yy - w, zl)), Vector((b, yy - w, zl)), Vector((b, yy + w, zl)), Vector((a, yy + w, zl))], [(0, 1, 2, 3)], "plankline", False, lambda q_: Z)
    ix = (I.levels[0][0] + I.levels[0][1]) * 0.5
    yard.fire_points += [(sp.lifts[0][0], 0.0, iz), (sp.lifts[1][0], 0.0, iz), (sp.lifts[2][0], 0.0, iz), (ix, I.yi, iz + 2.0),
                         (sp.funnels[0][0], -w0(sp.funnels[0][0]) - 2.0, zg)]
    from ships import sockets_common
    sockets_common(yard)
    m.weather = weather_for(yard)
    cap.carrier_clips(m, yard, len(lifts), hz)
    m.info.setdefault("file", {}).update({"class": "Soryu" if sp.name == "Soryu" else "Hiryu", "navy": "JAP", "year": 1941, "stands_for": "Carrier_JAP",
                                          "paint": "IJN grey all over; an unpainted wooden flight deck with steel tie-down strips, white lines and kana, red and white stripes on its after end"})
    return m, yard


def soryu(fine=True):
    """Soryu as she was at Pearl Harbor and Midway: her island forward on the starboard side."""
    return carrier(SORYU, fine)


def hiryu(fine=True):
    """Hiryu as she was at Pearl Harbor and Midway: her island amidships on the port side."""
    return carrier(HIRYU, fine)


BUILDERS = {"Soryu": lambda fine: soryu(fine), "Hiryu": lambda fine: hiryu(fine)}
