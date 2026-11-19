"""Fittings for the named capital ships (capitals.py): what a close camera finds on a deck in 1944-45.

Each piece is built from numbers to the measurements of the real fitting, as the rest of the yard is; none is copied
from any drawing or model. Everything here is in the yard's own axes (metres, x forward, y to port, z up).

    sailor          a man at his station: steel helmet, life jacket, shirt and trousers; standing, sitting at a
                    gun's seat, loading, or aiming with his hands on the grips. Crews ride the part of their mount.
    bofors_quad     the American quadruple 40 mm mounting in its splinter tub, with its crew of seven
    oerlikon        a single 20 mm gun behind its shield, with its gunner in the shoulder rests and a loader
    twin_5in        the American enclosed twin 5-inch mounting (a `sec.*` turret, as the yard's secondary turrets are)
    mk37, mk38      American fire-control directors with their rangefinders and radar dishes
    sk_radar, sg_radar   the air-search bedspring and the surface-search dish
    jp_triple25     Japan's triple 25 mm mounting, with its shield and crew of six
    jp_twin127      Japan's twin 12.7 cm high-angle mounting, open or under its shield
    rangefinder     a Japanese rangefinder on its turning house

Crews: a battle model's men are only in its fine mesh. Their paints are their own (`suit`, `shirt`, `vest`,
`helmet`, `skin`), so the game can leave them out by paint at a distance.
"""

import math

from mathutils import Matrix, Vector

from rigid import X, Y, Z, Model

# The men's colours (US Navy at general quarters: chambray shirt, blue dungarees, kapok life jacket, grey steel
# helmet; the Imperial Navy's sailors in their pale working dress with the khaki helmet). Paints are added to a
# model's own by `crew_paints`.
CREW = {
    "USA": {"suit": "33405A", "shirt": "7C8FA8", "vest": "8E8A6A", "helmet": "5E656B", "skin": "B88A68"},
    "JAP": {"suit": "B9B6A2", "shirt": "C9C6B4", "vest": "8F8C70", "helmet": "6A6A4C", "skin": "B08460"},
}


def crew_paints(paints, navy):
    p = dict(paints)
    for k, v in CREW[navy].items():
        p.setdefault(k, v)
    p.setdefault("gunmetal", "2C2F33")
    p.setdefault("brass", "8C7A4A")
    p.setdefault("ammo", "A88E4E")
    p.setdefault("flash", "D2CCB8")
    return p


def frame(facing):
    """A man's (or a mounting's) own axes from the way he faces: forward, to his left, up."""
    f = Vector((facing[0], facing[1], 0.0))
    if f.length < 1e-9:
        f = X.copy()
    f.normalize()
    return f, Z.cross(f), Z


def _oval_trunk(m, base, f, l, rows, paint, sides=10):
    """A body turned on an oval lathe up the z axis: rows of (height, half width across, half depth, lean forward)."""
    rings = []
    for h, rw, rd, lean in rows:
        c = base + Z * h + f * lean
        rings.append([c + l * (math.cos(2 * math.pi * k / sides) * rw) + f * (math.sin(2 * math.pi * k / sides) * rd) for k in range(sides)])
    m.skin(rings, paint, (True, True), True)


def sailor(m, foot, facing, pose="stand", hands=None, seat=None, navy="USA", lean=0.0, k=1.0):
    """A man standing with his feet at `foot`, facing `facing` (a direction in the deck's plane).

    pose: "stand" (arms at his sides), "aim" (his hands on the grips at `hands`, a pair of points), "sit" (on a seat
    whose top is `seat` high over his feet, thighs forward, hands at `hands`), "load" (arms up, a clip of rounds held
    over his head), "carry" (a round held at his chest). `lean`: how far his shoulders lean forward (metres). `k` his
    size. About 250 triangles."""
    if not m.fine:
        return
    foot = Vector(foot)
    f, l, up = frame(facing)
    sitting = pose == "sit"
    hip_h = (seat + 0.08) if sitting else 0.93 * k
    hip = foot + Z * hip_h + (f * -0.06 if sitting else Vector())
    lean_v = f * lean
    # legs: two lengths each, from his hips to his knees to his feet
    for side in (1, -1):
        h = hip + l * side * 0.1 * k
        if sitting:
            knee = h + f * 0.44 * k - Z * 0.02
            ankle = Vector((knee.x, knee.y, foot.z + 0.08)) + f * 0.06
        else:
            knee = h - Z * 0.44 * k + f * 0.02
            ankle = foot + l * side * 0.11 * k + Z * 0.08
        m.rod(h, knee, 0.078 * k, "suit", rb=0.064 * k, sides=6, caps=(False, False))
        m.rod(knee, ankle, 0.062 * k, "suit", rb=0.05 * k, sides=6, caps=(False, False))
        m.box(ankle - Z * 0.045 + f * 0.05, (0.26 * k, 0.1 * k, 0.09 * k), "gunmetal", yaw=math.degrees(math.atan2(f.y, f.x)))
    # his trunk: hips in his trousers, the life jacket over his chest
    _oval_trunk(m, hip - Z * 0.06, f, l, [(0.0, 0.17 * k, 0.11 * k, 0.0), (0.16 * k, 0.17 * k, 0.12 * k, 0.0)], "suit", 8)
    body = hip + Z * 0.1 * k
    _oval_trunk(m, body, f, l, [(0.0, 0.19 * k, 0.14 * k, 0.0), (0.2 * k, 0.21 * k, 0.16 * k, lean * 0.45), (0.36 * k, 0.2 * k, 0.15 * k, lean * 0.8),
                                (0.44 * k, 0.14 * k, 0.11 * k, lean)], "vest", 10)
    shoulder_c = body + Z * 0.4 * k + lean_v
    neck = shoulder_c + Z * 0.06 * k
    m.rod(neck - Z * 0.03, neck + Z * 0.1 * k, 0.055 * k, "skin", sides=6, caps=(False, False))
    head = neck + Z * 0.2 * k + f * 0.02
    m.ball(head, (0.1 * k, 0.085 * k, 0.115 * k), "skin", 8, 6)
    # the steel helmet: a dome and its brim (the Japanese one deeper and without a brim to speak of)
    if navy == "JAP":
        m.ball(head + Z * 0.03 * k, (0.135 * k, 0.125 * k, 0.12 * k), "helmet", 8, 4, top_only=True)
        m.lathe(head + Z * 0.0, Z, [(0.0, 0.142 * k), (0.035 * k, 0.136 * k)], "helmet", 8, caps=(False, False))
    else:
        m.ball(head + Z * 0.035 * k, (0.14 * k, 0.13 * k, 0.12 * k), "helmet", 8, 4, top_only=True)
        m.lathe(head + Z * 0.025 * k, Z, [(0.0, 0.175 * k), (0.012, 0.172 * k)], "helmet", 10, caps=(True, True))
    # arms: shoulder, elbow, hand
    for side in (1, -1):
        sh = shoulder_c + l * side * 0.21 * k - Z * 0.04
        if pose in ("aim", "sit") and hands:
            hand = Vector(hands[0] if side == 1 else hands[1])
            mid = (sh + hand) * 0.5
            reach = (hand - sh).length
            bend = max(0.0, (0.62 * k) ** 2 / 4.0 - (reach * 0.5) ** 2) ** 0.5
            elbow = mid - Z * bend * 0.8 + l * side * bend * 0.6
        elif pose == "load":
            elbow = sh + l * side * 0.12 * k + Z * 0.26 * k + f * 0.06
            hand = sh + l * side * 0.02 * k + Z * 0.58 * k + f * 0.1
        elif pose == "carry":
            elbow = sh - Z * 0.27 * k + f * 0.08 + l * side * 0.04
            hand = shoulder_c - Z * 0.22 * k + f * 0.3 + l * side * 0.12 * k
        else:
            elbow = sh - Z * 0.29 * k + f * 0.03 + l * side * 0.03
            hand = elbow - Z * 0.27 * k + f * 0.06
        m.rod(sh, elbow, 0.058 * k, "shirt", rb=0.05 * k, sides=6, caps=(True, False))
        m.rod(elbow, hand, 0.048 * k, "shirt", rb=0.04 * k, sides=6, caps=(False, False))
        m.ball(hand, 0.047 * k, "skin", 6, 4)
    if pose == "load":                       # a clip of four rounds held over his head
        c = shoulder_c + Z * 0.62 * k + f * 0.1
        for i in range(4):
            m.rod(c + l * (i - 1.5) * 0.06 - f * 0.22, c + l * (i - 1.5) * 0.06 + f * 0.22, 0.026, "ammo", sides=5)
    if pose == "carry":
        c = shoulder_c - Z * 0.2 * k + f * 0.32
        m.rod(c - l * 0.25, c + l * 0.25, 0.06, "ammo", sides=6)


# ---- the American light guns ----------------------------------------------------------------------------

def bofors_quad(yard, n, x, y, z, bearing=0.0, tub=2.9, tub_high=1.25, crew=True, limits=None, parent=None, deck=True):
    """The quadruple 40 mm mounting (two twin carriages side by side on one turning platform) in its splinter tub,
    the pointer seated on its left and the trainer on its right, four loaders behind the guns and the mount captain
    in the tub. Parts `bofors.N` (trains: plus clockwise seen from above; rest `bearing`) and `bofors.N.guns` (plus
    lifts them). Socket `aa.K` at the muzzles (riding the guns)."""
    m = yard.m
    half = parent or yard.half(x)
    m.use(half)
    o = Vector((x, y, z))
    fine = m.fine
    if tub:
        m.tub(o, tub, tub_high, "upper", wall=0.04, sides=24 if fine else 10, floor="deck" if deck else None)
        if fine:   # ready-use lockers for the clips round the inside of the tub, and a lip on its rim
            for i in range(9):
                a = math.radians(bearing + 180.0 + (i - 4) * 22.0)
                c = o + Vector((math.cos(a), math.sin(a), 0)) * (tub - 0.32)
                m.box(c + Z * 0.48, (0.42, 0.55, 0.9), "upper", yaw=math.degrees(a) + 90.0)
            m.lathe(o + Z * (tub_high - 0.02), Z, [(0.0, tub + 0.035), (0.06, tub + 0.035)], "upper", 24, caps=(False, False))
    tn = m.part(f"bofors.{n}", half, (x, y, z + 0.3), axis=(0, 0, -1), limits=list(limits or [-180, 180]), kind="turn",
                note=f"a quadruple 40 mm mounting: plus trains it clockwise seen from above; rest bearing {int(bearing)}; about 25 degrees a second")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 25.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y

    def W(px, py, pz):
        return o + f * px + l * py + Z * pz
    # the turning platform and its pedestal
    m.lathe(o, Z, [(0.0, 0.62), (0.3, 0.55)], "upper", 14 if fine else 8)
    m.lathe(o + Z * 0.3, Z, [(0.0, 1.55), (0.07, 1.55)], "deck", 20 if fine else 8)
    # the two carriages' trunnion standards, the elevating arcs
    for side in (1, -1):
        m.box(W(0.05, side * 0.52, 0.85), (1.15, 0.2, 1.0), "upper", yaw=bearing * -1.0)
    piv = W(0.15, 0.0, 1.38)
    gn = m.part(f"bofors.{n}.guns", tn, tuple(piv), axis=tuple(-l), limits=[-15, 90], kind="turn", note="the four guns: plus lifts them")
    m.use(gn)
    rest = math.radians(12.0)
    g = (f * math.cos(rest) + Z * math.sin(rest)).normalized()
    side_dir = l
    for side in (1, -1):
        for j, off in enumerate((0.12, 0.36)):
            q = piv + side_dir * side * (off + (0.16 if j else 0.0))
            # the breech and its feed guide (the clip goes in from above), the barrel with its recoil spring and flash hider
            m.box(q - g * 0.15 + Z * 0.05, (0.85, 0.2, 0.32), "upper", yaw=-bearing, pitch=12.0)
            if fine:
                m.box(q - g * 0.25 + Z * 0.36, (0.26, 0.16, 0.42), "upper", yaw=-bearing, pitch=12.0)
                m.box(q - g * 0.25 + Z * 0.62, (0.2, 0.12, 0.14), "ammo", yaw=-bearing, pitch=12.0)
            m.rod(q + g * 0.25, q + g * 0.85, 0.075, "upper", rb=0.07, sides=8 if fine else 5)
            m.rod(q + g * 0.85, q + g * 2.2, 0.04, "gunmetal", sides=8 if fine else 4)
            m.lathe(q + g * 2.2, g, [(0.0, 0.05), (0.08, 0.068), (0.3, 0.078)], "gunmetal", 8 if fine else 4, caps=(False, True))
    tip = piv + g * 2.5
    k = sum(1 for s in m.sockets if s["name"].startswith("aa.")) + 1
    m.socket(f"aa.{k}", tip, g, gn, "a 40 mm mounting's muzzles: four streams of tracer start here")
    m.use(tn)
    # the pointer's and trainer's seats and sights, on arms out each side
    for side in (1, -1):
        s0 = W(-0.25, side * 1.08, 0.37)
        m.box(s0 + Z * 0.35, (0.38, 0.38, 0.06), "gunmetal", yaw=-bearing)
        m.rod(s0, s0 + Z * 0.33, 0.04, "upper", sides=5)
        m.box(W(0.32, side * 1.02, 1.32), (0.2, 0.14, 0.22), "gunmetal", yaw=-bearing)          # his sight
        m.rod(W(0.25, side * 0.8, 0.95), W(0.3, side * 1.0, 1.25), 0.03, "upper", sides=4)
        for hw in (0.12, -0.12):                                                              # his handwheels
            m.lathe(W(0.12, side * 1.05 + hw, 0.92), f, [(0.0, 0.13), (0.03, 0.13)], "gunmetal", 8, caps=(False, False))
    if crew and fine:
        # pointer and trainer seated at the sides; four loaders on the platform behind the breeches; the captain in the tub
        for side in (1, -1):
            sailor(m, W(-0.2, side * 1.08, 0.37), f, "sit", seat=0.38, hands=(W(0.12, side * 1.08 + 0.15, 0.95), W(0.12, side * 1.08 - 0.15, 0.95)), lean=0.08)
        for side, dx, pose in ((1, -0.85, "load"), (-1, -0.85, "load"), (1, -1.35, "carry"), (-1, -1.35, "carry")):
            sailor(m, W(dx, side * 0.42, 0.37), f, pose)
    m.use(half)
    if crew and fine and tub:
        a = math.radians(-bearing + 180.0 + 35.0)
        sailor(m, o + Vector((math.cos(a), math.sin(a), 0)) * (tub - 0.75) + Z * 0.02, f, "stand")
    return tn


def oerlikon(yard, x, y, z, facing=0.0, crew=True, shield=True, tub=0.0, loader=True):
    """A single 20 mm gun on its pedestal: the gun in its cradle with a drum on top, its curved shield, shoulder rests,
    and its gunner standing in them with his hands on the grips; a loader beside him with a drum. Fixed to the deck
    it stands on (it does not train in the model). Socket `aa.K` at the muzzle."""
    m = yard.on(x)
    o = Vector((x, y, z))
    f, l, _ = frame((math.cos(math.radians(facing)), math.sin(math.radians(facing))))
    if tub:
        m.tub(o, tub, 1.1, "upper", wall=0.035, sides=18 if m.fine else 8)
    m.lathe(o, Z, [(0.0, 0.32), (0.08, 0.3), (0.16, 0.13), (1.12, 0.1)], "upper", 10 if m.fine else 6)
    up = math.radians(20.0)
    g = (f * math.cos(up) + Z * math.sin(up)).normalized()
    q = o + Z * 1.32
    m.rod(q - g * 0.55, q + g * 0.32, 0.075, "gunmetal", sides=8 if m.fine else 5)                  # its body and recoil spring casing
    m.rod(q + g * 0.32, q + g * 1.5, 0.026, "gunmetal", sides=6 if m.fine else 4)
    m.lathe(q + g * 1.5, g, [(0.0, 0.034), (0.16, 0.042)], "gunmetal", 6, caps=(False, True))
    k = sum(1 for s in m.sockets if s["name"].startswith("aa.")) + 1
    m.socket(f"aa.{k}", q + g * 1.66, g, yard.half(x), "a 20 mm gun's muzzle: tracer starts here")
    if not m.fine:
        return
    upg = l.cross(g)
    m.lathe(q - g * 0.12 + upg * 0.1, upg, [(0.0, 0.2), (0.13, 0.2)], "gunmetal", 12)               # the drum
    m.rod(o + Z * 1.1, q - g * 0.1, 0.06, "upper", sides=6)
    for side in (1, -1):                                                                          # the shoulder rests and the grips
        a = q - g * 0.55 + l * side * 0.22 - Z * 0.12
        m.rod(q - g * 0.45, a, 0.02, "gunmetal", sides=4)
        m.rod(a - f * 0.06 - Z * 0.1, a + f * 0.04 + Z * 0.08, 0.035, "gunmetal", sides=4)
    if shield:   # the shield: a curved plate across the front with its top cut back, a slot for the gun
        rows = []
        for t in (-1.0, -0.6, -0.2, 0.2, 0.6, 1.0):
            a = t * 0.62
            c = o + f * (0.42 - 0.12 * t * t) + l * (t * 0.62) + Z * 0.62
            rows.append([c, c + Z * (0.92 - 0.12 * abs(t))])
        m.sheet(rows, "upper", f, smooth=True)
        rows_b = [[p - f * 0.02 for p in r] for r in rows]
        m.sheet(rows_b, "upper", -f, smooth=True)
    if crew:
        hands = (q - g * 0.62 + l * 0.17 - Z * 0.02, q - g * 0.62 - l * 0.17 - Z * 0.02)
        sailor(m, o - f * 0.95, f, "aim", hands=hands, lean=0.12)
        if loader:
            sailor(m, o - f * 0.75 + l * 0.85, f + l * -0.4, "carry")


# ---- the American five-inch mounting and directors ------------------------------------------------------

def twin_5in(yard, name, x, y, z, bearing, limits=(-150, 150), base=0.0):
    """The enclosed twin 5-inch/38 mounting: a slab-sided shield with its face and the front of its roof sloped back,
    two sighting hoods on the roof, a door behind, rails and a ventilator on top. Built as a yard turret ("house"),
    so its parts, sockets and clip are the same as every other secondary turret's (`sec.*`)."""
    m = yard.m
    if base > 0:
        yard.on(x).lathe((x, y, z - 0.05), Z, [(0.0, 2.35), (base + 0.05, 2.35)], "upper", 20 if m.fine else 8, caps=(False, True))
    tn, guns = yard.turret(name, x, z + base, 4.7, 2, 4.2, bearing, 0.07, y=y, style="house", rate=20.0, elev=(-15, 85), rest_elev=8.0, limits=limits, floats=False)
    if m.fine:
        p = m.parts[m.part_at[tn]]
        m.use(tn)
        rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
        o = Vector(p.pivot)
        S = 4.7
        hh = 0.62 * S

        def W(px, py, pz):
            return o + rot @ Vector((px, py, pz))
        for side in (1, -1):
            m.box(W(0.95, side * 1.55, hh + 0.2), (0.75, 0.42, 0.4), "upper", yaw=bearing)        # the pointer's and trainer's hoods
            m.disc(W(1.33, side * 1.55, hh + 0.25), rot @ X, 0.09, "glass", 8)
            for dx in (-1.6, -0.4):
                m.box(W(dx, side * 1.3, hh + 0.18), (0.05, 0.05, 0.34), "upper", yaw=bearing)
            m.box(W(-1.0, side * 1.3, hh + 0.35), (1.25, 0.04, 0.04), "rope", yaw=bearing)
        m.box(W(-0.62 * S - 0.02, 0.0, hh * 0.45), (0.06, 0.8, 1.7), "door", yaw=bearing)       # the door behind
        m.lathe(W(-1.9, 0.0, hh), Z, [(0.0, 0.26), (0.32, 0.22), (0.4, 0.3)], "upper", 8)
        for k in range(6):                                                                     # the shield's seams
            zz = 0.3 + k * 0.42
            if zz < hh - 0.1:
                for side in (1, -1):
                    m.box(W(-0.3, side * 0.5 * S + side * 0.004, zz), (0.62 * S * 1.6, 0.01, 0.02), "seam", yaw=bearing)
    return tn, guns


def mk37(yard, name, x, y, z, bearing=0.0, parent=None):
    """The Mark 37 director: a box with its face sloped back, the rangefinder through it with a hood each side, and on
    its roof the two radar dishes (a broad mesh one and the narrow 'orange peel' beside it). A turn part."""
    m = yard.m
    dn = m.part(name, parent or yard.half(x), (x, y, z), axis=(0, 0, -1), kind="turn", note=f"a fire-control director: turns with the guns it serves; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
    o = Vector((x, y, z))

    def W(px, py, pz):
        return o + rot @ Vector((px, py, pz))
    m.lathe(o, Z, [(0.0, 1.7), (0.35, 1.7)], "upper", 18 if m.fine else 8)
    lo = [(2.1, 1.25), (1.6, 1.7), (-2.2, 1.7), (-2.2, -1.7), (1.6, -1.7), (2.1, -1.25)]
    hi = [(1.2, 1.15), (0.8, 1.62), (-2.15, 1.62), (-2.15, -1.62), (0.8, -1.62), (1.2, -1.15)]
    vl = [W(px, py, 0.35) for px, py in lo]
    vh = [W(px, py, 2.45) for px, py in hi]
    n = len(lo)
    mid = W(0, 0, 1.4)
    m.add(vl + vh, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "upper", False, lambda q: q - mid)
    ctr = W(-0.4, 0, 2.47)
    m.add(vh + [ctr], [(k, (k + 1) % n, n) for k in range(n)], "roof", False, lambda q: Z)
    lat = rot @ Y
    c = W(-0.6, 0, 1.75)
    m.lathe(c - lat * 2.35, lat, [(0.0, 0.2), (0.25, 0.24), (4.45, 0.24), (4.7, 0.2)], "upper", 10 if m.fine else 5)
    for side in (1, -1):
        m.box(c + lat * side * 2.2 + rot @ Vector((0.2, 0, 0.12)), (0.55, 0.42, 0.55), "upper", yaw=bearing)
        m.disc(c + lat * side * 2.2 + rot @ Vector((0.48, 0, 0.12)), rot @ X, 0.12, "glass", 8)
    if m.fine:
        for side in (1, -1):
            m.disc(W(1.66, side * 0.75, 1.75), rot @ Vector((0.83, 0, 0.55)), 0.18, "glass", 8)
        m.box(W(-2.21, 0.0, 1.2), (0.05, 0.75, 1.6), "door", yaw=bearing)
    # the radar dishes on the roof: a shallow rectangular mesh (fire control) on a frame, and the tall narrow one
    dish = W(0.1, 0.55, 2.45 + 1.05)
    face = (rot @ X).normalized()
    _mesh_dish(m, dish, face, 2.3, 1.15, "upper")
    m.rod(W(-0.3, 0.55, 2.45), dish - face * 0.25, 0.06, "upper", sides=4)
    peel = W(0.0, -0.9, 2.45 + 0.95)
    _mesh_dish(m, peel, face, 0.75, 1.55, "upper")
    m.rod(W(-0.3, -0.9, 2.45), peel - face * 0.2, 0.05, "upper", sides=4)
    return dn


def _mesh_dish(m, c, face, wide, high, paint):
    """A radar dish seen as a shallow curved mesh: a rim, ribs across, a feed on its arm."""
    side = Z.cross(face).normalized()
    up = face.cross(side).normalized()
    rows = []
    nu, nv = (7, 5) if m.fine else (3, 3)
    for i in range(nu):
        u = -1.0 + 2.0 * i / (nu - 1)
        row = []
        for j in range(nv):
            v = -1.0 + 2.0 * j / (nv - 1)
            depth = 0.18 * (u * u + v * v) * max(wide, high) * 0.25
            row.append(c + side * u * wide * 0.5 + up * v * high * 0.5 + face * depth)
        rows.append(row)
    m.sheet(rows, paint, face, smooth=True)
    m.sheet([[p - face * 0.025 for p in r] for r in rows], paint, -face, smooth=True)
    m.rod(c + face * 0.05, c + face * max(wide, high) * 0.45, 0.025, "steel", sides=4)


def mk38(yard, name, x, y, z, bearing=0.0, arm=4.2, parent=None):
    """The Mark 38 director for the big guns: a taller house with a long rangefinder through it, a hood at each end, and
    the radar's broad aerial on a frame over its front. A turn part."""
    m = yard.m
    dn = m.part(name, parent or yard.half(x), (x, y, z), axis=(0, 0, -1), kind="turn", note=f"the main battery director: turns with the big guns; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
    o = Vector((x, y, z))

    def W(px, py, pz):
        return o + rot @ Vector((px, py, pz))
    m.lathe(o, Z, [(0.0, 2.3), (0.4, 2.3)], "upper", 20 if m.fine else 8)
    plan = Model.round_rect(2.3, -2.6, -1.9, 1.9, 0.7, 3 if m.fine else 1)
    top = Model.round_rect(1.6, -2.5, -1.75, 1.75, 0.55, 3 if m.fine else 1)
    lo = [W(px, py, 0.4) for px, py in plan]
    hi = [W(px, py, 2.8) for px, py in top]
    n = len(lo)
    mid = W(0, 0, 1.6)
    m.add(lo + hi, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "upper", False, lambda q: q - mid)
    m.add(hi + [W(-0.4, 0, 2.82)], [(k, (k + 1) % n, n) for k in range(n)], "roof", False, lambda q: Z)
    lat = rot @ Y
    c = W(-0.4, 0, 2.0)
    m.lathe(c - lat * arm, lat, [(0.0, 0.25), (0.3, 0.3), (arm * 2 - 0.3, 0.3), (arm * 2, 0.25)], "upper", 12 if m.fine else 5)
    for side in (1, -1):
        m.box(c + lat * side * (arm - 0.2) + rot @ Vector((0.25, 0, 0.15)), (0.7, 0.5, 0.7), "upper", yaw=bearing)
        m.disc(c + lat * side * (arm - 0.2) + rot @ Vector((0.61, 0, 0.15)), rot @ X, 0.14, "glass", 8)
    if m.fine:
        for side in (1, -1):
            m.disc(W(2.0, side * 0.9, 1.9), rot @ X, 0.2, "glass", 8)
    # the radar aerial on its frame over the front of the roof
    face = (rot @ X).normalized()
    ac = W(1.0, 0.0, 2.8 + 0.95)
    _mesh_dish(m, ac, face, 3.2, 1.1, "upper")
    for side in (1, -1):
        m.rod(W(0.4, side * 1.2, 2.8), ac + lat * side * 1.2 - face * 0.1, 0.06, "upper", sides=4)
    return dn


def sk_radar(yard, name, x, y, z, size=5.2, parent=None):
    """The big square air-search aerial (a 'bedspring'): a grid of dipoles in a frame on a turning post. Spins."""
    m = yard.m
    rn = m.part(name, parent or yard.half(x), (x, y, z), axis=(0, 0, -1), kind="spin", note="the air-search aerial: turns steadily, about a turn in six seconds")
    m.use(rn)
    m.rod((x, y, z), (x, y, z + 0.7), 0.16, "upper", sides=6)
    c = Vector((x + 0.3, y, z + 0.7 + size * 0.5))
    b = 0.1
    for sy in (-1, 1):
        m.box(c + Vector((0, sy * (size - b) / 2.0, 0)), (b, b, size), "upper")
    for sz in (-1, 1):
        m.box(c + Vector((0, 0, sz * (size - b) / 2.0)), (b, size, b), "upper")
    nn = 7 if m.fine else 2
    for i in range(nn):
        t = (i + 1) / (nn + 1.0) - 0.5
        m.box(c + Vector((0.02, t * size, 0)), (0.05, 0.05, size), "upper")
        m.box(c + Vector((0.02, 0, t * size)), (0.05, size, 0.05), "upper")
    if m.fine:      # the dipoles stand off the face of the grid in rows, and a frame behind holds it to the post
        for i in range(6):
            for j in range(6):
                p = c + Vector((0.32, (i - 2.5) * size / 6.4, (j - 2.5) * size / 6.4))
                m.box(p, (0.04, 0.55, 0.04), "steel")
                m.box(p - Vector((0.15, 0, 0)), (0.3, 0.03, 0.03), "steel")
        for sy in (-1, 1):
            m.rod((x, y, z + 0.7), c + Vector((-0.05, sy * size * 0.45, size * 0.45)), 0.05, "upper", sides=4)
            m.rod((x, y, z + 0.7), c + Vector((-0.05, sy * size * 0.45, -size * 0.45)), 0.05, "upper", sides=4)
    return rn


def sg_radar(yard, name, x, y, z, wide=1.6, parent=None):
    """The small surface-search dish: a shallow wide reflector in a frame on a short post. Spins."""
    m = yard.m
    rn = m.part(name, parent or yard.half(x), (x, y, z), axis=(0, 0, -1), kind="spin", note="the surface-search aerial: turns steadily")
    m.use(rn)
    m.rod((x, y, z), (x, y, z + 0.5), 0.1, "upper", sides=6)
    _mesh_dish(m, Vector((x + 0.1, y, z + 0.5 + wide * 0.25)), X, wide, wide * 0.42, "upper")
    return rn


# ---- the Imperial Navy's light guns and rangefinders ------------------------------------------------------

def jp_triple25(yard, n, x, y, z, bearing=0.0, shield=True, crew=True, tub=2.2, parent=None):
    """Japan's triple 25 mm mounting (Type 96): three guns side by side in one cradle on a pedestal, a long thin flash
    hider on each, the magazines on top, the layer and trainer seated either side; behind a splinter shield (the
    battleships' of 1944-45 had a box shield over the mounting) or in an open tub. Parts `aa25.N` and `aa25.N.guns`."""
    m = yard.m
    half = parent or yard.half(x)
    m.use(half)
    o = Vector((x, y, z))
    if tub:
        m.tub(o, tub, 1.1, "upper", wall=0.04, sides=20 if m.fine else 8, floor="deck")
    tn = m.part(f"aa25.{n}", half, (x, y, z + 0.2), axis=(0, 0, -1), limits=[-180, 180], kind="turn",
                note=f"a triple 25 mm mounting: plus trains it clockwise seen from above; rest bearing {int(bearing)}")
    m.parts[m.part_at[tn]].rest = bearing
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y

    def W(px, py, pz):
        return o + f * px + l * py + Z * pz
    m.lathe(o, Z, [(0.0, 0.45), (0.9, 0.3)], "upper", 10 if m.fine else 6)
    m.lathe(o + Z * 0.2, Z, [(0.0, 1.0), (0.06, 1.0)], "deck", 14 if m.fine else 6)
    piv = W(0.0, 0.0, 1.2)
    gn = m.part(f"aa25.{n}.guns", tn, tuple(piv), axis=tuple(-l), limits=[-10, 85], kind="turn", note="the three guns: plus lifts them")
    m.use(gn)
    e = math.radians(15.0)
    g = (f * math.cos(e) + Z * math.sin(e)).normalized()
    for j in (-1, 0, 1):
        q = piv + l * j * 0.28
        m.box(q - g * 0.2, (1.0, 0.16, 0.24), "upper", yaw=-bearing, pitch=15.0)
        m.rod(q + g * 0.3, q + g * 1.7, 0.03, "gunmetal", sides=6 if m.fine else 4)
        m.lathe(q + g * 1.7, g, [(0.0, 0.045), (0.5, 0.05), (0.55, 0.035)], "gunmetal", 8 if m.fine else 4)
        if m.fine:
            m.box(q - g * 0.2 + Z * 0.22, (0.3, 0.12, 0.26), "gunmetal", yaw=-bearing, pitch=15.0)
    k = sum(1 for s in m.sockets if s["name"].startswith("aa.")) + 1
    m.socket(f"aa.{k}", piv + g * 2.3, g, gn, "a 25 mm mounting's muzzles: three streams of tracer start here")
    m.use(tn)
    for side in (1, -1):
        s0 = W(-0.3, side * 0.85, 0.2)
        m.box(s0 + Z * 0.38, (0.34, 0.34, 0.06), "gunmetal", yaw=-bearing)
        m.rod(s0, s0 + Z * 0.36, 0.035, "upper", sides=5)
        m.box(W(0.25, side * 0.8, 1.35), (0.16, 0.12, 0.2), "gunmetal", yaw=-bearing)
    if shield and m.fine:
        # the box shield of the late war: a front and sides of thin plate, open behind and above the gunners' heads
        S = Model.round_rect(1.45, -1.25, -1.35, 1.35, 0.25, 2)
        pts = [W(px, py, 0.2) for px, py in S]
        keep = [i for i, (px, py) in enumerate(S) if px > -1.2]
        for a_, b_ in zip(keep, keep[1:]):
            if b_ != a_ + 1:
                continue
            pa, pb = pts[a_], pts[b_]
            m.add([pa, pb, pb + Z * 1.75, pa + Z * 1.75], [(0, 1, 2, 3), (3, 2, 1, 0)], "upper", False, None)
    if crew and m.fine:
        for side in (1, -1):
            sailor(m, W(-0.3, side * 0.85, 0.2), f, "sit", seat=0.42, hands=(W(0.15, side * 0.85 + 0.12, 1.0), W(0.15, side * 0.85 - 0.12, 1.0)), navy="JAP", lean=0.08)
        for side, dx, pose in ((1, -0.9, "load"), (-1, -0.9, "load"), (0, -1.5, "carry")):
            sailor(m, W(dx, side * 0.35, 0.2), f, pose, navy="JAP")
    m.use(half)
    if crew and m.fine and tub:
        a = math.radians(-bearing + 180.0 + 40.0)
        sailor(m, o + Vector((math.cos(a), math.sin(a), 0)) * (tub - 0.6) + Z * 0.02, f, "stand", navy="JAP")
    return tn


def jp_single25(yard, x, y, z, facing=0.0, crew=True):
    """A single 25 mm gun on its pedestal (the hundreds added in 1944-45 along every deck), its gunner at the grips."""
    m = yard.on(x)
    o = Vector((x, y, z))
    f, l, _ = frame((math.cos(math.radians(facing)), math.sin(math.radians(facing))))
    m.lathe(o, Z, [(0.0, 0.3), (0.12, 0.12), (1.0, 0.1)], "upper", 8 if m.fine else 5)
    g = (f * math.cos(0.3) + Z * math.sin(0.3)).normalized()
    q = o + Z * 1.15
    m.box(q - g * 0.15, (0.9, 0.16, 0.22), "gunmetal", yaw=facing, pitch=17.0)
    m.rod(q + g * 0.3, q + g * 1.65, 0.03, "gunmetal", sides=6 if m.fine else 4)
    m.lathe(q + g * 1.65, g, [(0.0, 0.045), (0.45, 0.05)], "gunmetal", 6 if m.fine else 4)
    k = sum(1 for s in m.sockets if s["name"].startswith("aa.")) + 1
    m.socket(f"aa.{k}", q + g * 2.1, g, yard.half(x), "a 25 mm gun's muzzle")
    if crew and m.fine:
        sailor(m, o - f * 0.9, f, "aim", hands=(q - g * 0.55 + l * 0.15, q - g * 0.55 - l * 0.15), navy="JAP", lean=0.1)


def jp_twin127(yard, name, x, y, z, bearing=0.0, shield=True, limits=(-170, 170)):
    """Japan's twin 12.7 cm high-angle mounting (Type 89): two guns on one cradle; on the battleships under a rounded
    shield open at the back. A `sec.*` turret: parts, sockets and clip as the yard's."""
    m = yard.m
    tn, guns = yard.turret(name, x, z, 4.0, 2, 4.6, bearing, 0.065, y=y, style="shield" if shield else "open", rate=16.0, elev=(-8, 90), rest_elev=10.0,
                           limits=limits, floats=False)
    if m.fine:
        p = m.parts[m.part_at[tn]]
        m.use(tn)
        rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
        o = Vector(p.pivot)

        def W(px, py, pz):
            return o + rot @ Vector((px, py, pz))
        f = rot @ X
        for side in (1, -1):
            sailor(m, W(-1.1, side * 0.75, 0.0), f, "stand", navy="JAP")
        sailor(m, W(-1.6, 0.0, 0.0), f, "carry", navy="JAP")
    return tn, guns


def rangefinder(yard, name, x, y, z, arm, bearing=0.0, house=1.4, parent=None):
    """A Japanese rangefinder: a long tube with a hood at each end on a round turning house. A turn part."""
    m = yard.m
    dn = m.part(name, parent or yard.half(x), (x, y, z), axis=(0, 0, -1), kind="turn", note=f"a rangefinder: turns with the guns; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(bearing), 3, "Z")
    lat = rot @ Y
    m.lathe((x, y, z), Z, [(0.0, house), (house * 0.9, house * 0.95), (house * 1.05, house * 0.7)], "upper", 16 if m.fine else 8)
    c = Vector((x, y, z + house * 0.62))
    m.lathe(c - lat * arm, lat, [(0.0, 0.3), (0.35, 0.36), (arm * 2 - 0.35, 0.36), (arm * 2, 0.3)], "upper", 12 if m.fine else 5)
    for side in (1, -1):
        m.box(c + lat * side * (arm - 0.3) + rot @ Vector((0.25, 0, 0.0)), (0.8, 0.6, 0.85), "upper", yaw=bearing)
        m.disc(c + lat * side * (arm - 0.3) + rot @ Vector((0.66, 0, 0.0)), rot @ X, 0.16, "glass", 8)
    return dn


# ---- small things -----------------------------------------------------------------------------------------

def float_rack(yard, x, y, z, facing, n=3, long=2.8, wide=1.2):
    """Life rafts stacked on edge against a wall (`facing` the way the wall faces), each a cork ring with its grating."""
    if not yard.m.fine:
        return
    f = Vector(facing).normalized()
    for i in range(n):
        yard.float_(x, y, z + i * 0.12, f, long=long, wide=wide)
        y += f.y * 0.24
        x += f.x * 0.24


def stair(m, foot, top, wide=0.8, paint="upper"):
    """An inclined ladder from one deck to the next: two stringers, treads, hand rails."""
    foot, top = Vector(foot), Vector(top)
    run = top - foot
    side = Z.cross(Vector((run.x, run.y, 0.0))).normalized()
    for s in (-0.5, 0.5):
        a, b = foot + side * s * wide, top + side * s * wide
        m.rod(a, b, 0.035, paint, sides=4, smooth=False)
        m.rod(a + Z * 0.9, b + Z * 0.9, 0.018, "rope", sides=3, smooth=False)
    steps = max(2, int(run.length / 0.28))
    for i in range(1, steps):
        p = foot + run * (i / steps)
        m.box(p, (0.22, wide, 0.03), paint, yaw=math.degrees(math.atan2(run.y, run.x)) + 0.0)
