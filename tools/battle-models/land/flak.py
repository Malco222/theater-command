"""Anti-aircraft wagons: the light flak that rolls with the columns. Each is a carrier's bare body (carriers.py,
fit="guns") with a gun mount stood in it, a tank's clips (her `fire` is a burst) and a carrier's steering.

    M16            the American half-track with four heavy machine guns in a powered mount: the gunner sits between
                   two round side plates, two guns and their chests outboard of each, a plate before him.
    FlakHalftrack  the German armoured half-track with a 2 cm automatic gun on a pedestal in her body, behind a
                   small shield.
    FlakTruck      a lorry with a 20 mm automatic gun stood on her floor behind a wide shield with swept wings, as
                   most armies put one.

PARTS   a carrier's, and: hull > turret (the mount: trains all round) > gun (lifts to 85 degrees) > barrel (jumps to
        each round)
CLIPS   idle, start, drive, move_off, stop, fire (a burst), fire.rock, hit, knocked_out, burn
SOCKETS a carrier's, and: muzzle (the four-gun mount: muzzle.1 to muzzle.4 too), case, crew.gunner (seated on the
        mount, turns with it), crew.loader, crew.loader2 (the four-gun mount), commander
Every shape is our own mesh, made from numbers here.
"""

import math

from mathutils import Vector

import carriers
from guns import sheet
from motorpool import LEFT, RIGHT, X, Y, Z, frame
from tanks_ger import coat, coats


def ring_wheel(v, at, ax, r=0.1, grip=True, turn=0.0):
    """A handwheel: a rim on three spokes about a hub, on an axle along `ax`. turn: where round the rim its handle
    stands (degrees). Returns the middle of the handle, where a hand takes it."""
    at, ax = Vector(at), Vector(ax).normalized()
    _, side, up = frame(ax)
    n = v.n(18, 8)
    v.rod(at - ax * 0.12, at, 0.012, "steel", 8)
    v.tube([at + (side * math.cos(2 * math.pi * k / n) + up * math.sin(2 * math.pi * k / n)) * r for k in range(n)], 0.010, "steel", 6, closed=True)
    for k in range(3):
        d = side * math.cos(2 * math.pi * k / 3 + 0.5) + up * math.sin(2 * math.pi * k / 3 + 0.5)
        v.rod(at, at + d * r, 0.007, "steel", 6, caps=False)
    v.lathe(at - ax * 0.012, ax, [(0.03, 0.0), (0.03, 0.02), (0.0, 0.024), (0.0, 0.0)], "steel", 8)
    if grip:
        a = math.radians(turn)
        g0 = at + (side * math.cos(a) + up * math.sin(a)) * r
        v.rod(g0, g0 + ax * 0.07, 0.011, "wood", 8)
        return g0 + ax * 0.035
    return None


def flash_barrel(v, base, length, bore=0.02, r0=0.03, r1=0.018):
    """An automatic gun's barrel with a flash cone on its muzzle. Returns where the muzzle is."""
    b = Vector(base)
    v.barrel(tuple(b), length, bore, r0=r0, r1=r1, brake=None, sleeve=None, sides=14)
    v.lathe(tuple(b + X * (length - 0.01)), X, [(0.0, bore * 0.55), (0.10, bore * 1.05), (0.10, bore * 1.35), (0.0, bore * 0.9)], "upper", v.n(14, 8))
    return b + X * (length + 0.10)


# =====================================================================================================================
def light_flak(v, at, parent="hull", lift=0.0, shield="wide", low=-5.0):
    """A 20 mm automatic anti-aircraft gun on its mount, stood on a floor at `at` (the foot of its base), looking
    forward. lift: how high a pedestal it stands on (none: it stands on three feet). shield: "wide" (two halves with
    swept wings), "narrow" (one small plate) or None. Makes the parts turret > gun > barrel and the sockets muzzle,
    case and crew.gunner. Returns the turning platform's middle and the trunnions' middle."""
    at = Vector(at)
    fine = v.fine
    foot = at + Z * lift
    P = foot + Z * 0.12
    p = lambda x, y, z: P + Vector((x, y, z))             # noqa: E731
    n_big = v.n(32, 12)

    # ---- what stands on the floor: a pedestal or three feet, and the ring she turns on ---------------------------
    v.use(parent)
    if lift > 0.02:
        v.lathe(at, Z, [(lift + 0.01, 0.0), (lift + 0.01, 0.21), (0.07, 0.25), (0.035, 0.28), (0.035, 0.37), (0.0, 0.37), (0.0, 0.0)], coat, v.n(24, 10))
        v.bolts(at + Z * 0.035, Z, 0.325, 8)
        if fine:
            for k in range(4):                             # the webs that stay it
                a = math.pi / 4 + k * math.pi / 2
                d = Vector((math.cos(a), math.sin(a), 0.0))
                q = Vector((-d.y, d.x, 0.0)) * 0.01
                v.hull([tuple(at + d * 0.2 + Z * 0.035 + q), tuple(at + d * 0.2 + Z * 0.035 - q), tuple(at + d * 0.35 + Z * 0.035 + q), tuple(at + d * 0.35 + Z * 0.035 - q),
                        tuple(at + d * 0.2 + Z * lift * 0.8 + q), tuple(at + d * 0.2 + Z * lift * 0.8 - q)], coat, 0.002, both=False)
    else:
        for k in range(3):                                 # three feet, a levelling screw on each
            a = math.pi + k * 2 * math.pi / 3
            d = Vector((math.cos(a), math.sin(a), 0.0))
            q = Vector((-d.y, d.x, 0.0))
            v.hull([tuple(at + d * 0.36 + q * w + Z * z) for w in (-0.07, 0.07) for z in (0.03, 0.10)]
                   + [tuple(at + d * 0.70 + q * w + Z * z) for w in (-0.045, 0.045) for z in (0.03, 0.075)], coat, 0.006, both=False)
            v.lathe(at + d * 0.64, Z, [(0.02, 0.0), (0.02, 0.085), (0.0, 0.085), (0.0, 0.0)], "steel", v.n(14, 8))
            v.rod(at + d * 0.64 + Z * 0.02, at + d * 0.64 + Z * 0.17, 0.014, "steel", 8)
            if fine:
                v.rod(at + d * 0.64 + Z * 0.16 - q * 0.05, at + d * 0.64 + Z * 0.16 + q * 0.05, 0.007, "steel", 6)
    v.lathe(foot, Z, [(0.12, 0.0), (0.12, 0.43), (0.09, 0.46), (0.0, 0.46), (0.0, 0.0)], coat, n_big)
    v.bolts(foot + Z * 0.12, Z, 0.395, 12, r=0.011)

    # ---- the mount: a platform that turns, two cheeks up to the trunnions, the gunner's seat behind ----------------
    v.part("turret", parent, tuple(P), axis=(0, 0, -1), kind="turn", note="the gun's mount: trains all round, by hand: + to the right, clockwise seen from above; the game turns it")
    v.lathe(P, Z, [(0.03, 0.0), (0.03, 0.50), (0.0, 0.50), (0.0, 0.0)], coat, n_big)
    v.tube([p(0.5 * math.cos(2 * math.pi * k / n_big), 0.5 * math.sin(2 * math.pi * k / n_big), 0.03) for k in range(n_big)], 0.011, coat, 6, closed=True)
    tz = 0.55
    T = p(0.05, 0.0, tz)
    for s in (LEFT, RIGHT):
        y0, y1 = sorted((s * 0.19, s * 0.215))
        v.extrude([(P.x - 0.22, P.z + 0.03), (P.x + 0.30, P.z + 0.03), (P.x + 0.30, P.z + 0.18), (T.x + 0.08, T.z + 0.06), (T.x - 0.08, T.z + 0.06), (P.x - 0.22, P.z + 0.26)],
                  P.y + y0, P.y + y1, coat, 0.004)
        v.lathe(T + Y * (s * 0.215), Y * s, [(0.03, 0.0), (0.03, 0.05), (0.0, 0.062), (0.0, 0.0)], "steel", v.n(14, 8))               # the trunnion's cap
        if fine:
            v.bolts(T + Y * (s * 0.2155), Y * s, 0.045, 6, r=0.006, high=0.004)
            for x in (-0.14, 0.2):
                v.bolt(p(x, s * 0.2155, 0.09), Y * s, 0.009, 0.006)
        v.slab(p(-0.34, s * 0.22, 0.03), p(-0.12, s * 0.36, 0.045), coat, 0.004)                                                     # his foot rests
        # the boxes of rounds, racked either side
        v.slab(p(-0.08, s * 0.27, 0.03), p(0.24, s * 0.43, 0.25), coat, 0.012)
        v.slab(p(-0.085, s * 0.265, 0.25), p(0.245, s * 0.435, 0.265), coat, 0.004)
        if fine:
            v.handle(p(0.03, s * 0.35, 0.266), p(0.13, s * 0.35, 0.266), Z, r=0.006, reach=0.025, paint="steel")
    v.slab(p(0.08, -0.19, 0.03), p(0.29, 0.19, 0.16), coat, 0.01)                                                                  # the gear case between the cheeks
    v.rod(p(-0.22, 0.0, 0.08), p(-0.22, 0.0, 0.2), 0.03, coat, 8)
    sx = -0.67                                             # the seat's point: a seated man's knees end short of the cradle, his boots stand between the cheeks
    v.tube([p(-0.22, 0.0, 0.2), p(sx + 0.18, 0.0, 0.26), p(sx + 0.02, 0.0, 0.29)], 0.022, coat, 8)                                  # the seat's bracket
    v.box((0.34, 0.34, 0.045), p(sx - 0.02, 0.0, 0.315), "leather", 0.016)
    v.tube([p(sx - 0.19, 0.0, 0.3), p(sx - 0.2205, 0.0, 0.41), p(sx - 0.2205, 0.0, 0.52)], 0.016, coat, 6)                             # (up from the seat's rear edge: its face 0.19 behind his seat's point and more)
    v.slab(p(sx - 0.2235, -0.15, 0.5), p(sx - 0.2175, 0.15, 0.7), coat, 0.002)                                                      # the seat's back: a plate,
    v.box((0.015, 0.30, 0.2), p(sx - 0.21, 0.0, 0.6), "leather", 0.005)                                                             # and a thin pad on it: its face 0.2025 behind the seat's point (a seated man's back wants 0.195)
    v.tube([p(-0.12, -0.225, 0.40), p(-0.23, -0.235, 0.44)], 0.02, coat, 8)                                                         # the laying gear's arm, back from the right cheek to its wheel
    grip_r = ring_wheel(v, p(-0.23, -0.33, 0.44), -Y, r=0.1)                                                                        # he lays her with one hand (its handle 0.34 ahead of his seat's point, where his hand reaches it),
    grip_l = ring_wheel(v, p(-0.27, 0.33, 0.5), Vector((-0.5, 0.86, 0.0)), r=0.1, turn=180.0)                                       # and trains her with the other
    v.socket("crew.gunner", p(sx, 0.0, 0.34), X, part="turret", note="the gunner, seated behind the gun: the top of his seat, 0.31 over the platform his boots are on; he turns with the mount")
    v.socket("grip.gunner.r", grip_r, -Y, part="turret", note="his right hand: the handle of the wheel he lays her by (its middle)")
    v.socket("grip.gunner.l", grip_l, Vector((-0.5, 0.86, 0.0)), part="turret", note="his left hand: the handle of the wheel he trains her by (its middle)")

    # ---- her shield ------------------------------------------------------------------------------------------------
    if shield == "wide":
        lean = math.radians(12.0)
        w = Vector((-math.sin(lean), 0.0, math.cos(lean)))
        nrm = Vector((math.cos(lean), 0.0, math.sin(lean)))
        O = p(0.37, 0.0, 0.10)
        half = [(0.11, 0.0), (0.74, 0.0), (0.74, 0.70), (0.54, 0.92), (0.11, 0.92)]
        port = [(0.17, 0.68), (0.31, 0.68), (0.31, 0.80), (0.17, 0.80)]
        sheet(v, half, [port], O, Y, w, 0.008, coat, 0.002)
        sheet(v, [(-a, b) for a, b in reversed(half)], [], O, Y, w, 0.008, coat, 0.002)
        sweep = math.radians(35.0)
        ul, ur = Vector((-math.sin(sweep), math.cos(sweep), 0.0)), Vector((math.sin(sweep), math.cos(sweep), 0.0))
        sheet(v, [(0.0, 0.0), (0.30, 0.10), (0.30, 0.56), (0.0, 0.70)], [], O + Y * 0.74, ul, w, 0.008, coat, 0.002)                 # the wings, swept back
        sheet(v, [(0.0, 0.10), (0.30, 0.0), (0.30, 0.70), (0.0, 0.56)], [], O - Y * 0.74 - ur * 0.30, ur, w, 0.008, coat, 0.002)
        for s in (LEFT, RIGHT):
            v.rod(p(0.30, s * 0.215, 0.16), O + Y * (s * 0.46) + w * 0.26 - nrm * 0.008, 0.012, coat, 6)                             # the stays that hold it
            v.rod(T + Vector((0.06, s * 0.215, 0.03)), O + Y * (s * 0.42) + w * 0.62 - nrm * 0.008, 0.012, coat, 6)
            if fine:
                for b in (0.1, 0.6):
                    hp = O + Y * (s * 0.74) + w * b + nrm * 0.004
                    v.hinge(hp - w * 0.04, hp + w * 0.04, 0.01)
    elif shield == "narrow":
        lean = math.radians(10.0)
        w = Vector((-math.sin(lean), 0.0, math.cos(lean)))
        nrm = Vector((math.cos(lean), 0.0, math.sin(lean)))
        O = p(0.36, 0.0, 0.22)
        sheet(v, [(-0.42, 0.0), (0.42, 0.0), (0.42, 0.50), (0.30, 0.70), (0.09, 0.70), (0.09, 0.22), (-0.09, 0.22), (-0.09, 0.70), (-0.30, 0.70), (-0.42, 0.50)],
              [], O, Y, w, 0.010, coat, 0.002)
        for s in (LEFT, RIGHT):
            v.rod(p(0.30, s * 0.215, 0.16), O + Y * (s * 0.3) + w * 0.06 - nrm * 0.01, 0.012, coat, 6)
            v.rod(T + Vector((0.06, s * 0.215, 0.03)), O + Y * (s * 0.3) + w * 0.5 - nrm * 0.01, 0.012, coat, 6)

    # ---- the gun: lifts at the trunnions; her receiver hangs behind them, her barrel runs out before ---------------
    v.part("gun", "turret", tuple(T), axis=(0, -1, 0), limits=[low, 85.0], kind="turn", note="lifts at the trunnions: + muzzle up, almost to the upright; the game lays it")
    g = lambda x, y, z: T + Vector((x, y, z))             # noqa: E731
    for s in (LEFT, RIGHT):
        v.lathe(g(0.0, s * 0.105, 0.0), Y * s, [(0.08, 0.0), (0.08, 0.075), (0.0, 0.085), (0.0, 0.0)], coat, v.n(16, 8))            # the trunnion's boss
    v.slab(g(-0.28, -0.075, -0.105), g(0.24, 0.075, -0.05), coat, 0.006)                                                           # the cradle's trough
    for s in (LEFT, RIGHT):
        v.slab(g(-0.2, s * 0.075, -0.105), g(0.2, s * 0.095, 0.03), coat, 0.004)
    arc = [(T.x + 0.27 * math.cos(math.radians(a)), T.z + 0.27 * math.sin(math.radians(a))) for a in range(-150, -29, 12)]
    arc += [(T.x + 0.22 * math.cos(math.radians(a)), T.z + 0.22 * math.sin(math.radians(a))) for a in range(-30, -151, -12)]
    v.extrude(arc, T.y - 0.187, T.y - 0.167, "steel", 0.002)                                                                       # the arc she is lifted by
    v.slab(g(-0.30, -0.05, -0.05), g(0.40, 0.05, 0.07), "steel", 0.012, 2)                                                         # the receiver
    v.slab(g(-0.22, -0.036, 0.07), g(0.22, 0.036, 0.092), "steel", 0.006)                                                          # its cover
    v.rod(g(-0.30, 0.0, 0.01), g(-0.38, 0.0, 0.01), 0.032, "steel", 10)                                                            # the buffer
    v.slab(g(-0.12, 0.05, -0.038), g(0.02, 0.31, 0.046), "steel", 0.01)                                                            # the magazine, in from her left
    v.slab(g(-0.14, -0.13, -0.045), g(0.0, -0.05, 0.02), "steel", 0.008)                                                           # where the empties are thrown out
    v.rod(g(0.40, 0.0, 0.01), g(0.55, 0.0, 0.01), 0.042, coat, v.n(14, 8))                                                          # the barrel's sleeve
    v.rod(g(0.10, -0.05, 0.03), g(0.10, -0.085, 0.03), 0.009, "steel", 6)                                                           # the cocking handle
    v.ball(g(0.10, -0.095, 0.03), 0.016, "steel", 8, 6)
    # the sight on its arm, left of the gun: a ring ahead, the eyepiece behind
    v.tube([g(-0.16, 0.095, 0.0), g(-0.16, 0.15, 0.2), g(-0.16, 0.15, 0.3)], 0.012, "steel", 6)
    v.rod(g(-0.22, 0.15, 0.3), g(0.16, 0.15, 0.3), 0.009, "steel", 6)
    v.slab(g(-0.27, 0.12, 0.265), g(-0.19, 0.18, 0.335), "shade", 0.008)
    if fine:
        n = 16
        v.tube([g(0.16, 0.15 + 0.055 * math.cos(2 * math.pi * k / n), 0.3 + 0.055 * math.sin(2 * math.pi * k / n)) for k in range(n)], 0.005, "steel", 5, closed=True)
        v.tube([g(0.16, 0.15 + 0.025 * math.cos(2 * math.pi * k / n), 0.3 + 0.025 * math.sin(2 * math.pi * k / n)) for k in range(n)], 0.004, "steel", 5, closed=True)
        v.rod(g(0.16, 0.095, 0.3), g(0.16, 0.205, 0.3), 0.003, "steel", 4, caps=False)
        v.rod(g(0.16, 0.15, 0.245), g(0.16, 0.15, 0.355), 0.003, "steel", 4, caps=False)
    v.socket("case", g(-0.07, -0.2, -0.05), -Y, part="gun", note="where the empty cases are thrown out, to her right")
    base = g(0.40, 0.0, 0.01)
    v.part("barrel", "gun", tuple(base), slide=(-1, 0, 0), slide_limits=[0.0, 0.06], axis=(0, -1, 0), kind="slide", note="jumps back along its own length at each round (metres)")
    muzzle = flash_barrel(v, base, 1.30)
    v.socket("muzzle", tuple(muzzle), X)
    v.m.info["bore"], v.m.info["brake"] = 0.02, False
    return P, T


# =====================================================================================================================
def tombstone(v, xa, xb, y0, y1, lo, high=0.36, rise=0.09, out=0.0):
    """A chest of belted rounds for the heavy machine gun, as the record has it: flat-sided and flat-footed, its top a
    low arch (the men called it the tombstone), a touch taller than it is long; the lid shuts on a rim where the
    sides end. xa..xb long, y0..y1 wide, standing on lo. out: +1 or -1 names the side its catch is on (0: none)."""
    fine = v.fine
    y0, y1 = min(y0, y1), max(y0, y1)
    xm, half, side = (xa + xb) / 2, (xb - xa) / 2, high - rise
    R = (half * half + rise * rise) / (2.0 * rise)
    a0 = math.asin(half / R)
    n = 10 if fine else 4
    arch = [(xm + R * math.sin(a0 - 2.0 * a0 * i / n), lo + high - R + R * math.cos(a0 - 2.0 * a0 * i / n)) for i in range(n + 1)]
    v.extrude([(xa, lo), (xb, lo)] + arch, y0, y1, coat, 0.005)
    v.slab((xa - 0.004, y0 - 0.004, lo + side - 0.016), (xb + 0.004, y1 + 0.004, lo + side - 0.002), coat, 0.002)       # the rim its lid shuts on
    if not fine:
        return
    ym = (y0 + y1) / 2
    zt = lo + high - R + math.sqrt(R * R - 0.05 * 0.05)
    v.handle((xm - 0.05, ym, zt), (xm + 0.05, ym, zt), Z, r=0.005, reach=0.02, paint="steel")                           # carried by the handle on its top
    if out:
        yf = y1 if out > 0 else y0
        v.slab((xa + 0.045, yf, lo + 0.05), (xb - 0.045, yf + out * 0.004, lo + side - 0.05), coat, 0.002)              # its side, pressed for stiffness
        v.slab((xm - 0.02, yf + out * 0.003, lo + side - 0.05), (xm + 0.02, yf + out * 0.011, lo + side + 0.022), "steel", 0.002)   # the catch
        for x in (xa + 0.03, xb - 0.03):
            v.bolt((x, yf, lo + 0.03), Y * out, 0.007, 0.004)


def quad_mount(v, at, parent="hull", high=0.28, low=-5.0, seat="canvas.seat"):
    """Four heavy machine guns in a powered mount, stood on a floor at `at`, looking forward: the gunner sits between
    two side plates that carry the trunnions; outboard of each a round plate turns with the guns, with two guns and
    their chests on it; a plate before him lifts with them. Makes the parts turret > gun > barrel and the sockets
    muzzle, muzzle.1 to muzzle.4, case and crew.gunner. Returns the turntable's middle and the trunnions' middle."""
    at = Vector(at)
    fine = v.fine
    P = at + Z * high
    p = lambda x, y, z: P + Vector((x, y, z))             # noqa: E731
    n_big = v.n(32, 12)
    v.use(parent)
    v.lathe(at, Z, [(high, 0.0), (high, 0.40), (high - 0.05, 0.43), (0.05, 0.43), (0.03, 0.50), (0.0, 0.50), (0.0, 0.0)], coat, n_big)      # the drum it turns on
    v.bolts(at + Z * 0.03, Z, 0.465, 10)

    v.part("turret", parent, tuple(P), axis=(0, 0, -1), kind="turn", note="the mount: trains all round, under power: + to the right, clockwise seen from above; the game turns it")
    v.lathe(P, Z, [(0.04, 0.0), (0.04, 0.47), (0.0, 0.47), (0.0, 0.0)], coat, n_big)
    tz = 0.84
    T = p(0.0, 0.0, tz)
    side = [(P.x - 0.455, P.z + 0.04), (P.x + 0.49, P.z + 0.04), (P.x + 0.49, P.z + 0.40), (P.x + 0.15, P.z + 0.97), (P.x - 0.15, P.z + 0.97), (P.x - 0.455, P.z + 0.56)]
    for s in (LEFT, RIGHT):
        y0, y1 = sorted((s * 0.31, s * 0.33))
        v.extrude(side, P.y + y0, P.y + y1, coat, 0.004)
        v.lathe(T + Y * (s * 0.33), Y * s, [(0.035, 0.0), (0.035, 0.09), (0.0, 0.11), (0.0, 0.0)], coat, v.n(18, 8))                 # the trunnion's bearing
    v.slab(p(0.47, -0.31, 0.04), p(0.49, 0.31, 0.40), coat, 0.003)                                                                  # the plate before his legs (0.67 ahead of his seat's point: room for his boots)
    v.slab(p(-0.455, -0.31, 0.04), p(-0.435, 0.31, 0.56), coat, 0.003)                                                              # and behind his back
    v.slab(p(-0.76, -0.27, 0.04), p(-0.455, 0.27, 0.36), coat, 0.02, 2)                                                             # the power unit behind him: a little engine and its dynamo
    v.rod(p(-0.6, -0.2, 0.44), p(-0.6, 0.06, 0.44), 0.085, coat, v.n(16, 8))
    v.rod(p(-0.6, 0.08, 0.44), p(-0.6, 0.2, 0.44), 0.06, "steel", v.n(14, 8))
    v.tube([p(-0.7, -0.14, 0.5), p(-0.76, -0.14, 0.56), p(-0.82, -0.14, 0.5), p(-0.82, -0.14, 0.2)], 0.014, "steel", 6)                # its exhaust
    if fine:
        v.louvres(p(-0.762, 0.0, 0.2), (0.3, 0.2), -X, count=6, down=-Z)
    v.box((0.37, 0.40, 0.05), p(-0.215, 0.0, 0.3), seat, 0.018)                                                                     # his seat
    v.box((0.04, 0.40, 0.34), p(-0.415, 0.0, 0.55), seat, 0.016)                                                                    # (its back 0.195 behind the seat's point: a seated man's back wants 0.185)
    v.rod(p(0.2, 0.0, 0.04), p(0.12, 0.0, 0.6), 0.016, "steel", 8)                                                                  # the column with the two grips he steers her by
    v.rod(p(0.12, -0.13, 0.6), p(0.12, 0.13, 0.6), 0.012, "steel", 6)
    for s in (LEFT, RIGHT):
        v.rod(p(0.12, s * 0.13, 0.6), p(0.07, s * 0.13, 0.7), 0.016, "black", 8)
    v.socket("crew.gunner", p(-0.2, 0.0, 0.33), X, part="turret", note="the gunner, seated between the guns: the top of his seat, 0.29 over the turntable his boots are on; he turns with the mount")
    for name, s in (("l", LEFT), ("r", RIGHT)):
        v.socket(f"grip.gunner.{name}", p(0.095, s * 0.13, 0.65), X, part="turret", note="a hand on the column's grip (its middle): he trains and lays her by them")

    # ---- what lifts: the round plates, the guns and their chests on them, the plate before the gunner --------------
    v.part("gun", "turret", tuple(T), axis=(0, -1, 0), limits=[low, 85.0], kind="turn", note="the four guns lift together at the trunnions: + muzzles up, almost to the upright; the game lays them")
    g = lambda x, y, z: T + Vector((x, y, z))             # noqa: E731
    k = 0
    spots = []
    for s in (LEFT, RIGHT):
        v.rod(g(0.0, s * 0.33, 0.0), g(0.0, s * 0.385, 0.0), 0.07, "steel", v.n(14, 8))
        v.lathe(g(0.0, s * 0.385, 0.0), Y * s, [(0.016, 0.0), (0.016, 0.30), (0.0, 0.30), (0.0, 0.0)], coat, v.n(36, 12))             # the round plate
        if fine:
            v.bolts(g(0.0, s * 0.401, 0.0), Y * s, 0.265, 12, r=0.009)
        v.slab(g(-0.24, s * 0.40, -0.25), g(0.18, s * 0.485, 0.25), coat, 0.012)                                                    # the carrier the guns are bolted to
        v.slab(g(0.0, s * 0.345, -0.03), g(0.42, s * 0.372, 0.03), coat, 0.004)                                                     # the arm out to his plate
        for dz in (0.17, -0.17):
            k += 1
            y = s * 0.54
            v.slab(g(-0.44, y - 0.045, dz - 0.06), g(0.12, y + 0.045, dz + 0.07), "steel", 0.012, 2)                                  # the receiver
            v.slab(g(-0.36, y - 0.03, dz + 0.07), g(-0.02, y + 0.03, dz + 0.085), "steel", 0.004)
            for dy in (-0.028, 0.028):
                v.rod(g(-0.44, y + dy, dz), g(-0.52, y + dy, dz), 0.011, "steel", 6)                                                # the back plate's buffer and grips
            v.rod(g(0.12, y, dz + 0.005), g(0.2, y, dz + 0.005), 0.034, "steel", v.n(12, 8))                                          # the barrel's bearing
            # its chest, outboard, fixed to the gun's own cradle (the two a side stand one above the other): a
            # flat-sided box with a low arched top, a chute in to the gun
            lo = dz - 0.14 if dz > 0 else dz - 0.20
            tombstone(v, T.x - 0.28, T.x + 0.06, T.y + s * 0.60, T.y + s * 0.765, T.z + lo, out=s)
            v.slab(g(-0.2, s * 0.585, dz - 0.03), g(-0.06, s * 0.60, dz + 0.04), "steel", 0.003)
            spots.append((k, y, dz))
    lean = math.radians(8.0)
    w = Vector((-math.sin(lean), 0.0, math.cos(lean)))
    nrm = Vector((math.cos(lean), 0.0, math.sin(lean)))
    O = g(0.42, 0.0, -0.22)
    sheet(v, [(-0.375, 0.0), (0.375, 0.0), (0.375, 0.48), (0.27, 0.58), (-0.27, 0.58), (-0.375, 0.48)], [[(-0.11, 0.32), (0.11, 0.32), (0.11, 0.45), (-0.11, 0.45)]],
          O, Y, w, 0.012, coat, 0.003)
    v.slab(g(0.27, -0.03, 0.12), g(0.33, 0.03, 0.2), "shade", 0.006)                                                                # the sight behind its window
    v.rod(g(0.33, 0.0, 0.14), O + w * 0.3 - nrm * 0.012, 0.01, "steel", 6)
    v.socket("case", g(-0.1, 0.0, -0.5), -Z, part="gun", note="the empties and links fall under the guns")
    base = g(0.2, 0.0, 0.0)
    v.part("barrel", "gun", tuple(base), slide=(-1, 0, 0), slide_limits=[0.0, 0.03], axis=(0, -1, 0), kind="slide", note="the four barrels: they jump back a little at each round (metres)")
    for k, y, dz in spots:
        v.mg(g(0.2, y, dz + 0.005), X, length=1.1, r=0.022)
        v.socket(f"muzzle.{k}", g(1.32, y, dz + 0.005), X)
    v.socket("muzzle", g(1.32, 0.0, 0.0), X, note="the middle of the four muzzles")
    v.m.info["bore"], v.m.info["brake"] = 0.0127, False
    return P, T


# =====================================================================================================================
def _flak_sockets(v, P, T, commander, loaders, fire):
    v.use("hull")
    # (a place is where a man's feet are: (x, y, z), or ((x, y, z), the way he faces, a note))
    place = lambda q, note: q if not isinstance(q[0], (int, float)) else (q, X, note)      # noqa: E731
    at, facing, note = place(commander, "the gun's commander stands here, watching the sky")
    v.socket("commander", at, facing, part="hull", note=note)
    for i, q in enumerate(loaders):
        at, facing, note = place(q, "a loader stands here, beside the mount, with the next box of rounds")
        v.socket("crew.loader" + ("" if i == 0 else str(i + 1)), at, facing, part="hull", note=note)
    v.socket("ring", tuple(P), Z, part="hull", note="the foot of the gun's mount")
    v.socket("hit.turret", tuple(T + Y * 0.3), Y, part="turret")
    v.socket(fire, tuple(P + Z * 0.3), Z, part="hull", note="at the gun: her ready rounds burn")


def m16(fine=True, tag="USA"):
    """The four-gun half-track (M16): the half-track's body with a powered mount of four heavy machine guns in place
    of her benches. 2.6 m to the guns. Five men: a driver and the commander in the cab, the gunner in the mount, a
    loader either side of it."""
    v = carriers.m3_halftrack(fine, tag, fit="guns", name="M16", title="Half-track M16, four heavy machine guns")
    v.m.kind = "aa"
    zs = 0.94
    P, T = quad_mount(v, (-1.6, 0.0, zs), "hull")
    stern = "a loader stands here at the stern corner, by the spare chests, clear of the mount as it turns"
    _flak_sockets(v, P, T, ((-0.55, 0.0, zs), X, "the gun's commander stands here, between the two pairs of barrels, watching the sky"),
                  [((-2.28, 0.78, zs), X, stern), ((-2.28, -0.78, zs), X, stern)], "fire.3")
    v.use("hull")
    for s in (LEFT, RIGHT):                                # spare chests in a row across her stern, clear of the mount as it turns
        for k, dx in enumerate((0.0, 0.018, -0.012)):
            y = s * (0.40 + 0.185 * k)
            tombstone(v, -2.87 + dx, -2.53 + dx, y, y + s * 0.165, zs)
    v.car_clips(burst=10, every=0.09, burst_cue="machine_gun", muzzles=["muzzle.1", "muzzle.2", "muzzle.3", "muzzle.4"], recoil=0.02, kick=0.15, gun_low=-5.0,
                hatches=[], traverse=60.0, idle_cue="halftrack_idle_loop", drive_cue="halftrack_drive_loop", has_turret=False, mg=None)
    return v


def flak_halftrack(fine=True, tag="GER", paint=None):
    """The flak half-track: the armoured half-track's body with a 2 cm automatic gun on a pedestal in place of her
    benches, behind a small shield. 2.6 m over the shield. Dark grey to early 1943, dark yellow after: a paint swap.
    Five men: a driver and the commander in the cab, the gunner on the mount, two loaders."""
    v = carriers.hanomag(fine, tag, paint=paint, fit="guns", name="FlakHalftrack", title="Sd.Kfz. 251 with a 2 cm flak gun")
    v.m.kind = "aa"
    fl = 0.74
    P, T = light_flak(v, (-1.25, 0.0, fl), "hull", lift=0.42, shield="narrow", low=0.0)
    # (the mount's ring fills her floor from side to side: the men stand on the track guards, 0.20 over the floor)
    _flak_sockets(v, P, T, ((-1.72, -0.68, 0.94), X, "the gun's commander stands here on the right track guard, behind the gun and clear of the falling cases, watching the sky"),
                  [((-1.28, 0.70, 0.94), -Y, "a loader stands here on the left track guard beside the gun, facing it: the magazine goes in at his waist"),
                   ((-1.80, 0.66, 0.94), -Y, "the second loader stands here on the left track guard behind the first, facing inboard, with the next magazines")], "fire.3")
    v.use("hull")
    for s in (LEFT, RIGHT):                                # the boxes of rounds on the track guards, inside her walls
        for x in (-0.45, -0.8):
            v.slab((x - 0.15, s * 0.56, 0.94), (x + 0.15, s * 0.82, 1.16), coat, 0.012)
            if fine:
                v.handle((x - 0.05, s * 0.69, 1.161), (x + 0.05, s * 0.69, 1.161), Z, r=0.006, reach=0.022, paint="steel")
    v.car_clips(burst=6, every=0.14, recoil=0.05, kick=0.2, gun_low=0.0, hatches=[], traverse=45.0, idle_cue="halftrack_idle_loop", drive_cue="halftrack_drive_loop",
                has_turret=False, mg=None)
    v.m.info.setdefault("file", {})["swaps"] = {"how": "paint: put the chosen coat's colours in place of the paints of the same names.", "paint": coats(v, paint or "grey")}
    return v


def flak_truck(fine=True, tag="_"):
    """The flak lorry: the 3-ton lorry with her tilt and benches off and a 20 mm automatic gun stood on her floor on
    its three feet, behind a wide shield with swept wings. 2.1 m to the cab's roof. Five men: a driver and the
    commander in the cab, the gunner on the mount, two loaders."""
    v = carriers.truck(fine, tag, fit="guns", name="FlakTruck", title="Flak lorry, 20 mm gun")
    v.m.kind = "aa"
    fl = 1.1
    P, T = light_flak(v, (-2.0, 0.0, fl), "hull", lift=0.0, shield="wide", low=-5.0)
    _flak_sockets(v, P, T, ((-0.70, -0.6, fl), X, "the gun's commander stands here, by the boxes of rounds, watching the sky"),
                  [((-2.05, 0.76, fl), -Y, "a loader stands here on the gun's left, facing it: the magazine goes in at his thigh, he bends to it"),
                   ((-0.9, 0.5, fl), X, "the second loader stands here, by the boxes of rounds, with the next magazines")], "fire.3")
    v.use("hull")
    for y in (-0.72, -0.36, 0.0):                         # the boxes of rounds against the headboard
        v.slab((-0.44, y - 0.16, fl), (-0.1, y + 0.16, fl + 0.24), coat, 0.012)
        if fine:
            v.handle((-0.32, y, fl + 0.241), (-0.22, y, fl + 0.241), Z, r=0.006, reach=0.022, paint="steel")
    v.car_clips(burst=6, every=0.14, recoil=0.05, kick=0.35, gun_low=-5.0, hatches=[], traverse=45.0, idle_cue="lorry_idle_loop", drive_cue="lorry_drive_loop",
                has_turret=False, mg=None)
    v.camouflage(plain=v.body_inside)
    return v
