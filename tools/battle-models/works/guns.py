"""Guns on fixed mountings: the heavy anti-aircraft gun on its pedestal, the light automatic anti-aircraft gun, the
searchlight. (Guns on wheels are the vehicle modeller's; these are bolted down.)

Our own designs: a plain heavy anti-aircraft gun of about 90 mm and a plain light automatic of about 40 mm, each for
every nation in its own paint, with no emblem (ruling 66(e)); not a copy of any one army's gun.
"""
import math

from mathutils import Quaternion, Vector

import bits
from bits import X, Y, Z, rnd
from posts import weather
from rigid import smooth


def annulus(m, at, r0, r1, thick, paint, sides=32, a0=0.0, a1=360.0):
    """A flat ring (or part of one) lying level: inner radius r0, outer r1, its top at `at`."""
    at = Vector(at)
    full = abs(a1 - a0) >= 359.9
    n = sides if full else max(2, int(sides * abs(a1 - a0) / 360.0))
    angs = [math.radians(a0 + (a1 - a0) * k / n) for k in range(n + (0 if full else 1))]
    cnt = len(angs)
    top_i = [at + Vector((math.cos(a) * r0, math.sin(a) * r0, 0)) for a in angs]
    top_o = [at + Vector((math.cos(a) * r1, math.sin(a) * r1, 0)) for a in angs]
    bot_i = [p - Z * thick for p in top_i]
    bot_o = [p - Z * thick for p in top_o]
    last = cnt if full else cnt - 1
    quads = [(k, (k + 1) % cnt, cnt + (k + 1) % cnt, cnt + k) for k in range(last)]
    m.add(top_i + top_o, quads, paint, False, lambda p: Z)
    m.add(bot_i + bot_o, quads, paint, False, lambda p: -Z)
    m.add(top_o + bot_o, quads, paint, True, lambda p: Vector((p.x - at.x, p.y - at.y, 0)))
    if r0 > 1e-4:
        m.add(top_i + bot_i, quads, paint, True, lambda p: -Vector((p.x - at.x, p.y - at.y, 0)))
    if not full:
        for k in (0, cnt - 1):
            d = Vector((-math.sin(angs[k]), math.cos(angs[k]), 0)) * (-1 if k == 0 else 1)
            m.add([top_i[k], top_o[k], bot_o[k], bot_i[k]], [(0, 1, 2, 3)], paint, False, lambda p, d=d: d)


def shell(m, at, axis, cal=0.09, long=0.86, paint="upper", seed=0):
    """A fixed round: the brass case and the shell in it, its fuze at the nose."""
    at, axis = Vector(at), Vector(axis).normalized()
    r = cal / 2
    sides = 14 if m.fine else 7
    m.lathe(at, axis, [(0.0, r * 1.28), (0.012, r * 1.28), (0.014, r * 1.16), (long * 0.60, r * 1.1), (long * 0.66, r * 1.02)], "brass", sides, (True, False))
    m.lathe(at + axis * (long * 0.66), axis, [(0.0, r), (long * 0.16, r), (long * 0.27, r * 0.7), (long * 0.31, r * 0.42)], paint, sides, (False, False))
    m.lathe(at + axis * (long * 0.97), axis, [(0.0, r * 0.42), (long * 0.03, r * 0.16)], "steel.worn", sides, (False, True))


def seat(m, at, facing, paint="upper"):
    """A layer's iron seat on its post: a dished pan and a low back."""
    at = Vector(at)
    f = Vector(facing).normalized()
    s = Vector((-f.y, f.x, 0))
    m.rod(at - Z * 0.42, at - Z * 0.02, 0.022, paint, sides=10)
    pan = [at + f * (math.cos(a) * 0.19) + s * (math.sin(a) * 0.2) + Z * (0.03 * (math.cos(a) ** 2) - 0.0) for a in [2 * math.pi * k / 14 for k in range(14)]]
    m.add(pan + [at - Z * 0.012], [(k, (k + 1) % 14, 14) for k in range(14)], paint, True, lambda p: Z)
    m.add(pan + [at - Z * 0.03], [(k, (k + 1) % 14, 14) for k in range(14)], paint, True, lambda p: -Z)
    back = at - f * 0.2
    m.rod(back, back + Z * 0.26 - f * 0.04, 0.012, paint, sides=6)
    bits.plate(m, [(-0.15, 0.0), (0.15, 0.0), (0.13, 0.14), (-0.13, 0.14)], 0.012, paint, back + Z * 0.2 - f * 0.03, s, Z)


def dial(m, at, facing, r=0.11, paint="upper"):
    """A data dial in its housing: where the layers read what the predictor sends."""
    at, f = Vector(at), Vector(facing).normalized()
    m.lathe(at, f, [(0.0, r), (0.07, r), (0.085, r * 0.92)], paint, 16, (True, False))
    m.disc(at + f * 0.086, f, r * 0.9, "sign.3", 16)
    m.disc(at + f * 0.088, f, r * 0.12, "steel", 8)
    if m.fine:
        side = f.cross(Z).normalized()
        up = side.cross(f)
        for k in range(12):
            a = 2 * math.pi * k / 12
            p = at + f * 0.088 + (side * math.cos(a) + up * math.sin(a)) * r * 0.74
            m.disc(p, f, r * 0.045, "steel", 4)
        m.add([at + f * 0.089 + side * 0.008, at + f * 0.089 - side * 0.008, at + f * 0.089 + up * r * 0.7], [(0, 1, 2)], "sign", False, lambda p: f)


def heavy_gun(m, parent, at=(0.0, 0.0, 0.0), shield=False, twin=False):
    """The heavy anti-aircraft gun on a fixed pedestal, muzzle toward +x, laid level. Builds the parts under `parent`.
    `twin`: two barrels side by side in one cradle on a broader mounting (parts barrel.l, barrel.r, breech.l, breech.r)."""
    fine = m.fine
    O = Vector(at)
    sides = 32 if fine else 14
    w = 0.52 if twin else 0.0                      # how much further out each cheek stands
    q = w * 0.9                                    # ... and the pedestal and turntable are that much stouter
    bores = [(0.52, ".l"), (-0.52, ".r")] if twin else [(0.0, "")]
    # ---- the pedestal: bolted to its bed, it never moves
    m.part("pedestal", parent, O, kind="fixed")
    m.use("pedestal")
    # the bed: an eight-sided block of concrete with its holding-down bolts
    bed = [(math.cos(math.radians(22.5 + 45 * k)) * (1.55 + w * 1.3), math.sin(math.radians(22.5 + 45 * k)) * (1.55 + w * 1.3)) for k in range(8)]
    bits.cast(m, bits.s_prism([(O.x + x, O.y + y) for x, y in bed], O.z - 0.25, O.z + 0.16), "concrete", 0.03)
    m.lathe(O + Z * 0.16, Z, [(0.0, 0.84 + q), (0.05, 0.84 + q), (0.055, 0.66 + q), (0.10, 0.62 + q), (0.80, 0.46 + q), (0.84, 0.52 + q), (0.92, 0.52 + q), (0.93, 0.50 + q)], "upper", sides)
    bits.bolt_ring(m, O + Z * 0.21, Z, 0.75 + q, 16 if not twin else 24, 0.026, 0.03, "steel")
    for k in range(8):       # the webs that stiffen the foot
        a = math.radians(45 * k)
        d = Vector((math.cos(a), math.sin(a), 0))
        s = Vector((-d.y, d.x, 0))
        bits.plate(m, [(0.60 + q, 0.0), (0.82 + q, 0.0), (0.82 + q, 0.03), (0.585 + q, 0.30)], 0.02, "upper", O + Z * 0.21, d, Z)
    m.lathe(O + Z * 0.60, X, [(0.44 + q, 0.10), (0.50 + q, 0.10), (0.51 + q, 0.085)], "upper", 12, (False, True))          # an inspection cover
    bits.bolt_ring(m, O + Vector((0.505 + q, 0, 0.60)), X, 0.07, 6, 0.01, 0.008, "steel")

    # ---- the mounting: turns on the pedestal, carries the layers and the platform
    P = O + Z * 1.09
    m.part("mount", "pedestal", P, axis=(0, 0, -1), kind="turn", note="traverse, all round: plus is clockwise seen from above")
    m.use("mount")
    m.lathe(P, Z, [(0.0, 0.56 + q), (0.035, 0.56 + q), (0.04, 0.50 + q), (0.16, 0.50 + q), (0.165, 0.47 + q)], "upper", sides)       # the turntable
    bits.bolt_ring(m, P + Z * 0.036, Z, 0.53 + q, 20 if not twin else 32, 0.014, 0.012, "steel")
    Tx, Tz = -0.12, 2.25
    T = O + Vector((Tx, 0, Tz))                    # the trunnions
    cheek = [(-0.50, 0.0), (0.58, 0.0), (0.52, 0.45), (0.25, 0.90), (0.14, 1.05), (0.0, 1.11), (-0.14, 1.05), (-0.26, 0.90), (-0.44, 0.42)]
    for sy in (1, -1):
        face = Vector((T.x, O.y + sy * (0.43 + w), P.z + 0.165))
        bits.plate(m, cheek, 0.045, "upper", face, X, Z)
        # a stiffening rib round the cheek's edge, and its bearing cap with a ring of bolts
        m.lathe(T + Y * sy * (0.43 + w), Y * sy, [(0.0, 0.15), (0.06, 0.15), (0.075, 0.13), (0.085, 0.07), (0.10, 0.07)], "upper", 20, (False, True))
        bits.bolt_ring(m, T + Y * sy * (0.492 + w), Y * sy, 0.115, 8, 0.013, 0.012, "steel")
        bits.bolts(m, face + Vector((-0.42, sy * 0.0225, 0.06)), face + Vector((0.50, sy * 0.0225, 0.06)), 9, Y * sy, 0.014, 0.012)
        # a rib up each edge of the cheek, and a lightening hole
        m.rod(face + Vector((0.55, 0, 0.02)), face + Vector((0.20, 0, 0.95)), 0.03, "upper", sides=8)
        m.rod(face + Vector((-0.47, 0, 0.02)), face + Vector((-0.24, 0, 0.92)), 0.03, "upper", sides=8)
        m.disc(face + Vector((0.08, sy * 0.024, 0.42)), Y * sy, 0.13, "void", 16)
        bits.tube(m, [face + Vector((0.08 + math.cos(2 * math.pi * k / 16) * 0.14, sy * 0.026, 0.42 + math.sin(2 * math.pi * k / 16) * 0.14)) for k in range(16)], 0.014, "upper", 6, closed=True)
        for x0 in (-0.36, 0.36):
            m.box(P + Vector((Tx + x0 + 0.12, sy * (0.40 + w), 0.21)), (0.05, 0.16, 0.09), "upper")
    m.box(P + Vector((0.40, 0, 0.42)), (0.045, 0.86 + 2 * w, 0.50), "upper")                                             # the front transom
    bits.bolts(m, P + Vector((0.424, -0.36 - w, 0.60)), P + Vector((0.424, 0.36 + w, 0.60)), 7 if not twin else 13, X, 0.014, 0.012)
    bits.bolts(m, P + Vector((0.424, -0.36 - w, 0.24)), P + Vector((0.424, 0.36 + w, 0.24)), 7 if not twin else 13, X, 0.014, 0.012)
    # the platform the crew ride on: tread plate on brackets, a rail round the back
    deck_z = O.z + 0.66
    Rd = 1.82 + w * 1.3                            # the platform's edge
    annulus(m, (O.x, O.y, deck_z), 0.70 + q, Rd, 0.035, "upper", 40 if fine else 16)
    annulus(m, (O.x, O.y, deck_z - 0.035), Rd - 0.08, Rd - 0.02, 0.07, "upper", 40 if fine else 16)               # its kerb
    if fine:                                                                                                    # the tread pattern: rings of studs
        for ring, cnt in [(0.86 + q + 0.18 * k, int((0.86 + q + 0.18 * k) * 30.5)) for k in range(int((Rd - 0.24 - 0.86 - q) / 0.18) + 1)]:
            for k in range(cnt):
                a = 2 * math.pi * (k + 0.5 * (cnt % 3)) / cnt
                c = Vector((O.x + math.cos(a) * ring, O.y + math.sin(a) * ring, deck_z))
                m.add([c + Vector((0.03, 0, 0.004)), c + Vector((0, 0.012, 0.004)), c + Vector((-0.03, 0, 0.004)), c + Vector((0, -0.012, 0.004))], [(0, 1, 2, 3)], "upper", False, lambda p: Z)
    for k in range(8):
        a = math.radians(22.5 + 45 * k)
        d = Vector((math.cos(a), math.sin(a), 0))
        m.rod(P + d * (0.52 + q) + Z * 0.06, Vector((O.x, O.y, deck_z - 0.04)) + d * (Rd - 0.57), 0.03, "upper", sides=8)
        m.rod(Vector((O.x, O.y, deck_z - 0.05)) + d * (0.72 + q), Vector((O.x, O.y, deck_z - 0.05)) + d * (Rd - 0.06), 0.022, "upper", sides=6)
    rail = [a for a in range(100, 261, 20)]
    posts_top = []
    for a in rail:
        d = Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0))
        foot = Vector((O.x, O.y, deck_z)) + d * (Rd - 0.06)
        m.rod(foot, foot + Z * 0.95, 0.017, "upper", sides=8)
        m.lathe(foot, Z, [(0.0, 0.035), (0.02, 0.035)], "upper", 8)
        posts_top.append(foot + Z * 0.95)
    arc_pts = [Vector((O.x + math.cos(math.radians(a)) * (Rd - 0.06), O.y + math.sin(math.radians(a)) * (Rd - 0.06), deck_z + 0.95)) for a in range(100, 261, 5)]
    bits.tube(m, arc_pts, 0.017, "upper", 8)
    bits.tube(m, [p - Z * 0.47 for p in arc_pts], 0.012, "upper", 6)
    # the layers: one each side, a seat, a handwheel and his dials
    for sy, what in ((1, "traverse"), (-1, "elevate")):
        s_at = Vector((O.x - 0.28, O.y + sy * (1.06 + w), deck_z + 0.44))
        seat(m, s_at, X)
        col = Vector((O.x + 0.26, O.y + sy * (0.80 + w), deck_z))
        m.rod(col, col + Z * 0.86, 0.035, "upper", sides=10)                                                     # his gear column
        m.box(col + Z * 0.90, (0.20, 0.16, 0.16), "upper")
        dial(m, col + Vector((-0.10, 0, 1.05)), Vector((-0.8, 0, 0.6)))
        dial(m, P + Vector((Tx + 0.30, sy * (0.48 + w), 0.80)), Y * sy, 0.09)
        m.rod(P + Vector((0.2, sy * (0.47 + w), 0.5)), col + Z * 0.86, 0.02, "upper", sides=6)                         # the shaft into the mounting
        bits.crew(m, "layer." + what, s_at, (1, 0, 0), "sit", "mount", "sitting on the platform's seat, turning the " + what + " handwheel", seat_h=0.44,
                  grips={"l": (col + Vector((-0.135, 0.12, 0.90)), (1, 0, 0), "mount"), "r": (col + Vector((-0.135, -0.12, 0.90)), (1, 0, 0), "mount")})
        # the foot rest
        m.rod(Vector((O.x + 0.45, O.y + sy * (0.95 + w), deck_z + 0.02)), Vector((O.x + 0.45, O.y + sy * (1.25 + w), deck_z + 0.02)), 0.015, "upper", sides=6)
    # the fuze setter: a box with two cups, at the left rear where the loaders pass
    fz = Vector((O.x - 0.78 - w * 0.5, O.y + 0.62 + w * 1.2, deck_z))
    m.rod(fz, fz + Z * 0.95, 0.04, "upper", sides=10)
    m.box(fz + Z * 1.06, (0.34, 0.42, 0.24), "upper")
    for dy in (-0.1, 0.1):
        m.lathe(fz + Vector((-0.17, dy, 1.08)), Vector((-1, 0, 0.35)), [(0.0, 0.075), (0.10, 0.075), (0.10, 0.058), (0.02, 0.05)], "steel", 14, (False, False))
        m.disc(fz + Vector((-0.17, dy, 1.08)) + Vector((-1, 0, 0.35)).normalized() * 0.022, Vector((-1, 0, 0.35)), 0.05, "void", 12)
    bits.handwheel(m, fz + Vector((0.0, 0.235, 1.06)), Y, 0.11, "steel", 4)
    bits.crew(m, "fuze", fz + Vector((-0.55, 0.0, 0.0)), (1, 0, 0), "stand", "mount", "standing on the platform, setting fuzes: he puts each round nose first into a cup")
    for y0, tag in bores:
        bits.crew(m, "loader" + tag, Vector((O.x - 1.25, O.y + y0 - (0.20 if not twin else 0.0), deck_z)), (1, 0.15 if not twin else 0.0, 0), "stand", "mount",
                  "standing on the platform behind the breech: he takes a round and throws it home")
    # three rounds ready in a rack on the platform
    for k in range(3):
        base = Vector((O.x - 1.42, O.y - 0.62 - w * 1.6 - 0.14 * k, deck_z))
        shell(m, base, Z, 0.09, 0.86)
        m.lathe(base, Z, [(0.0, 0.075), (0.12, 0.075)], "upper", 10, (False, False))
    m.box(Vector((O.x - 1.42, O.y - 0.76 - w * 1.6, deck_z + 0.5)), (0.03, 0.46, 0.03), "upper")
    m.rod(Vector((O.x - 1.42, O.y - 0.99 - w * 1.6, deck_z)), Vector((O.x - 1.42, O.y - 0.99 - w * 1.6, deck_z + 0.5)), 0.012, "upper", sides=6)
    m.rod(Vector((O.x - 1.42, O.y - 0.53 - w * 1.6, deck_z)), Vector((O.x - 1.42, O.y - 0.53 - w * 1.6, deck_z + 0.5)), 0.012, "upper", sides=6)
    m.socket("fire", P + Z * 0.5, (0, 0, 1), part="mount", note="where she burns when she is knocked out")

    # ---- the handwheels: each spins as its layer works
    for sy, what in ((1, "traverse"), (-1, "elevate")):
        c = Vector((O.x + 0.20, O.y + sy * (0.80 + w), deck_z + 0.90)) + Vector((-0.115, 0, 0))
        m.part("handwheel." + what, "mount", c, axis=(-1, 0, 0), kind="spin", note="the game spins it while the gun " + ("trains" if sy > 0 else "elevates"))
        m.use("handwheel." + what)
        bits.handwheel(m, c + Vector((-0.02, 0, 0)), -X, 0.17, "steel", 4, 0.012)
    # ---- the cradle: lifts on the trunnions
    m.part("cradle", "mount", T, axis=(0, -1, 0), limits=[-3.0, 85.0], kind="turn", note="elevation: plus lifts the muzzle, from 3 below level to 85 degrees")
    m.use("cradle")
    m.rod(T - Y * (0.40 + w), T + Y * (0.40 + w), 0.085, "steel", sides=20)                                      # the trunnion pin
    T0 = T
    if twin:        # the yoke that makes one cradle of the two troughs
        m.box(T0 + Vector((0.2, 0, -0.16)), (0.9, 2 * w - 0.40, 0.26), "upper")
        m.box(T0 + Vector((1.2, 0, -0.2)), (0.3, 2 * w - 0.40, 0.14), "upper")
    for y0, tag in bores:
        T = T0 + Y * y0
        # the cradle: a trough the gun slides in, with the buffer under it and the recuperator on top
        trough = [(-0.21, 0.0), (-0.21, -0.30), (-0.14, -0.36), (0.14, -0.36), (0.21, -0.30), (0.21, 0.0), (0.16, 0.0), (0.16, -0.26), (0.10, -0.31), (-0.10, -0.31), (-0.16, -0.26), (-0.16, 0.0)]
        x0, x1 = -0.52, 1.62
        lo = [T + Vector((x0, y, z)) for y, z in trough]
        hi = [T + Vector((x1, y, z)) for y, z in trough]
        k = len(trough)
        mid_line = (T + Vector((x0, 0, -0.18)), T + Vector((x1, 0, -0.18)))
        m.add(lo + hi, [(i, (i + 1) % k, k + (i + 1) % k, k + i) for i in range(k)], "upper", False,
              lambda p: (Vector((0, p.y - T.y, p.z - T.z + 0.18)) if (abs(p.y - T.y) > 0.185 or p.z - T.z < -0.335) else -Vector((0, p.y - T.y, p.z - T.z + 0.18))))
        for ring, way in ((lo, -X), (hi, X)):
            m.add(ring, [(0, 1, 10, 11), (1, 2, 9, 10), (2, 3, 8, 9), (3, 4, 7, 8), (4, 5, 6, 7)], "upper", False, lambda p, w=way: w)
        m.lathe(T + Vector((1.50, 0, 0.0)), X, [(0.0, 0.175), (0.02, 0.19), (0.16, 0.19), (0.18, 0.175)], "upper", sides, (False, False))   # the ring the barrel runs in
        m.lathe(T + Vector((-0.40, 0, -0.455)), X, [(0.0, 0.05), (0.01, 0.075), (1.55, 0.075), (1.56, 0.05), (1.62, 0.05)], "steel", 16)    # the buffer
        m.lathe(T + Vector((-0.42, 0, 0.30)), X, [(0.0, 0.07), (0.015, 0.095), (1.62, 0.095), (1.635, 0.07), (1.70, 0.045), (1.74, 0.045)], "upper", 20)   # the recuperator
        for xx in (-0.30, 0.50, 1.15):
            m.box(T + Vector((xx, 0, 0.19)), (0.07, 0.24, 0.07), "upper")
            bits.bolt(m, T + Vector((xx, 0.09, 0.225)), Z, 0.014, 0.012)
            bits.bolt(m, T + Vector((xx, -0.09, 0.225)), Z, 0.014, 0.012)
        m.lathe(T + Vector((0.3, 0, 0.395)), Z, [(0.0, 0.03), (0.03, 0.03), (0.035, 0.04), (0.05, 0.04)], "brass", 8)                         # the filling plug
    T = T0
    # the elevating arc: a toothed sector under the cradle on the right
    arc_r = 0.80
    aa = [math.radians(a) for a in range(-150, -29, 6)]
    for sy_face in (0,):
        yy = T.y - 0.275 if not twin else T.y + 0.0225
        outer = [Vector((T.x + math.cos(a) * arc_r, yy, T.z + math.sin(a) * arc_r)) for a in aa]
        inner = [Vector((T.x + math.cos(a) * (arc_r - 0.10), yy, T.z + math.sin(a) * (arc_r - 0.10))) for a in aa]
        n = len(aa)
        for dy, way in ((0.0, Y), (-0.045, -Y)):
            m.add([p + Y * dy for p in outer] + [p + Y * dy for p in inner], [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)], "steel", False, lambda p, w=way: w)
        m.add(outer + [p - Y * 0.045 for p in outer], [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)], "steel", True, lambda p: Vector((p.x - T.x, 0, p.z - T.z)))
        m.add(inner + [p - Y * 0.045 for p in inner], [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)], "steel", True, lambda p: -Vector((p.x - T.x, 0, p.z - T.z)))
        if fine:
            for a in range(-149, -30, 3):
                ar = math.radians(a)
                c = Vector((T.x + math.cos(ar) * (arc_r + 0.012), yy - 0.0225, T.z + math.sin(ar) * (arc_r + 0.012)))
                m.box(c, (0.024, 0.045, 0.02), "steel", pitch=a + 90)
        for a in (-140, -40):          # the arms that hold it to the cradle
            ar = math.radians(a)
            m.rod(Vector((T.x + math.cos(ar) * (arc_r - 0.05), yy - 0.02, T.z + math.sin(ar) * (arc_r - 0.05))), T + Vector((0.35 * math.cos(ar) + (0.25 if a > -90 else -0.2), -0.24 if not twin else -0.03, -0.33 if not twin else -0.2)), 0.028, "upper", sides=8)
    if shield and not twin:
        sh = [(-0.2, -1.05), (0.9, -1.25), (1.25, -0.6), (1.25, 0.6), (0.9, 1.25), (-0.2, 1.05), (-0.2, 0.36), (0.5, 0.36), (0.5, -0.36), (-0.2, -0.36)]
        for half in (sh[:5] + [(0.5, -0.36), (-0.2, -0.36)][::-1][::-1],):
            pass
        for sy in (1, -1):
            outl = [(0.38 * sy, -0.25), (1.2 * sy, -0.25), (1.3 * sy, 0.5), (1.0 * sy, 1.2), (0.38 * sy, 0.95)]
            if sy < 0:
                outl = outl[::-1]
            bits.plate(m, outl, 0.012, "upper", T + Vector((1.1, 0, 0.0)), Y, Vector((-0.12, 0, 1)))
    for y0, tag in bores:
        T = T0 + Y * y0
        # ---- the barrel: runs back in the cradle when she fires
        m.part("barrel" + tag, "cradle", T, slide=(-1, 0, 0), slide_limits=[0.0, 0.75], kind="slide", note="recoil: metres back along the bore as the cradle has lifted it")
        m.use("barrel" + tag)
        prof = [(0.0, 0.135), (0.02, 0.15), (0.85, 0.145), (0.87, 0.128), (0.95, 0.125), (2.35, 0.098), (2.37, 0.108), (2.50, 0.108), (2.52, 0.094), (4.50, 0.066), (4.52, 0.078), (4.70, 0.078), (4.715, 0.07)]
        m.lathe(T + X * (-0.10), X, prof, "upper", sides, (True, False))
        m.lathe(T + X * (4.615), -X, [(0.0, 0.07), (0.004, 0.047), (0.5, 0.045)], "void", 20, (False, True))            # the bore
        # the breech ring, with the slot the block slides across in
        ring = bits.s_box((T.x - 0.52, T.y - 0.20, T.z - 0.20), (T.x - 0.09, T.y + 0.20, T.z + 0.20))
        slot = bits.s_box((T.x - 0.44, T.y - 0.30, T.z - 0.115), (T.x - 0.25, T.y + 0.30, T.z + 0.115))
        bore = bits.s_cyl((T.x - 0.60, T.y, T.z), X, 0.062, 0.6, 20)
        bits.s_cut(ring, slot, bore)
        bits.s_bevel(ring, 0.012, 2)
        bits.take(m, ring, "steel")
        m.lathe(T + Vector((-0.30, 0, 0.21)), Z, [(0.0, 0.035), (0.05, 0.035), (0.055, 0.02), (0.13, 0.02)], "steel", 10)       # the lug the recuperator rod takes
        m.rod(T + Vector((-0.30, 0, 0.30)), T + Vector((-0.42, 0, 0.30)), 0.022, "steel.worn", sides=8)
        m.rod(T + Vector((-0.30, 0, -0.21)), T + Vector((-0.30, 0, -0.455)), 0.03, "steel", sides=8)
        m.rod(T + Vector((-0.30, 0, -0.455)), T + Vector((-0.40, 0, -0.455)), 0.022, "steel.worn", sides=8)
        # the breech lever on the right
        m.rod(T + Vector((-0.30, -0.205, 0.10)), T + Vector((-0.30, -0.27, 0.10)), 0.02, "steel", sides=8)
        m.rod(T + Vector((-0.30, -0.255, 0.10)), T + Vector((-0.52, -0.255, 0.30)), 0.014, "steel", sides=6)
        m.ball(T + Vector((-0.52, -0.255, 0.30)), 0.03, "timber.dark", 10, 6)
        m.socket("muzzle" + tag, T + X * 4.64, (1, 0, 0), part="barrel" + tag)
        m.socket("breech" + tag, T + Vector((-0.56, 0, 0)), (-1, 0, 0), part="barrel" + tag, note="where the round goes in and the empty case comes out")
        # ---- the breech block: slides across to the right to open
        m.part("breech" + tag, "barrel" + tag, T + Vector((-0.345, 0, 0)), slide=(0, -1, 0), slide_limits=[0.0, 0.19], kind="slide", note="plus opens the breech")
        m.use("breech" + tag)
        blk = bits.s_box((T.x - 0.435, T.y - 0.20, T.z - 0.11), (T.x - 0.255, T.y + 0.215, T.z + 0.11))
        bits.s_bevel(blk, 0.01, 2)
        bits.take(m, blk, "steel.worn")
        m.box(T + Vector((-0.345, 0.218, 0.0)), (0.10, 0.012, 0.14), "steel")
    T = T0
    return {"T": T, "P": P, "O": O, "deck": deck_z, "bores": bores}


def heavy_fire_clip(low=True):
    return {"long": 3.2, "loop": False, "after": "hold",
            "about": "One round: the barrel runs back and out, the breech throws the empty case, the next round goes in. Lies over whatever the game has laid (traverse and elevation).",
            "tracks": [{"part": "barrel", "slide": [[0.0, 0.0], [0.06, 0.62], [0.12, 0.66], [0.55, 0.20], [0.95, 0.02], [1.15, 0.0]]},
                       {"part": "cradle", "turn": [[0.0, 0.0], [0.05, -0.5], [0.18, 0.25], [0.4, -0.08], [0.7, 0.0]]},
                       {"part": "breech", "slide": [[0.0, 0.0], [0.62, 0.0], [0.78, 0.19], [2.35, 0.19], [2.5, 0.0], [3.2, 0.0]]}],
            "events": [{"t": 0.0, "fx": "muzzle_flash_big", "socket": "muzzle", "cue": "flak_heavy_fire_01"},
                       {"t": 0.0, "fx": "muzzle_smoke", "socket": "muzzle", "cue": ""},
                       {"t": 0.05, "fx": "blast_dust", "socket": "ground", "cue": ""},
                       {"t": 0.8, "fx": "case_eject", "socket": "breech", "cue": "tank_shell_case"},
                       {"t": 2.45, "fx": "breech_close", "socket": "breech", "cue": "field_gun_breech"}]}


def HeavyFlak(fine=True, nation="_default", name="HeavyFlak", shield=False):
    m = bits.model(name, "Heavy anti-aircraft gun on a fixed pedestal (our own design)", "works", nation, fine)
    m.part("gun", None, (0, 0, 0), kind="root")
    g = heavy_gun(m, "gun", (0, 0, 0), shield)
    m.use("gun")
    m.socket("ground", (2.2, 0.0, 0.05), (1, 0, 0), part="gun", note="where the blast lifts dust when she fires low")
    m.socket("hit", (0.6, 0.0, 1.4), (1, 0, 0), part="gun", note="where a hit strikes")
    bits.crew(m, "ammo.1", (-2.6, 0.9, 0.0), (1, -0.2, 0), "stand", "gun", "standing on the ground behind, passing a round up")
    bits.crew(m, "ammo.2", (-2.9, -0.7, 0.0), (1, 0.2, 0), "stand", "gun", "carrying a round from the locker")
    bits.crew(m, "commander", (-3.2, -2.2, 0.0), (1, 0.3, 0), "stand", "gun", "standing, field glasses up, giving the order to fire")
    m.clips["fire"] = heavy_fire_clip()
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Laid and still.", "tracks": [], "events": []}
    m.clips["follow"] = {"long": 12.0, "loop": True, "about": "Following a formation across the sky (for a look only: the game trains the mount and cradle itself, 6 degrees a second at most, and spins the handwheels as they turn).",
                         "tracks": [{"part": "mount", "turn": [[0.0, -25.0], [6.0, 25.0], [12.0, -25.0]]},
                                    {"part": "cradle", "turn": [[0.0, 48.0], [3.0, 62.0], [6.0, 50.0], [9.0, 60.0], [12.0, 48.0]]},
                                    {"part": "handwheel.traverse", "turn": [[0.0, 0.0], [6.0, 3600.0], [12.0, 0.0]]},
                                    {"part": "handwheel.elevate", "turn": [[0.0, 0.0], [3.0, 1500.0], [6.0, 200.0], [9.0, 1300.0], [12.0, 0.0]]}], "events": []}
    q = Quaternion(Vector((1, 0.3, 0)).normalized(), math.radians(7.0))
    m.clips["knocked_out"] = {"long": 2.2, "loop": False, "after": "hold",
                              "about": "A hit knocks her out: the mounting is thrown askew on its pedestal, the barrel is jammed back out of battery, the breech hangs open; she burns at `fire`.",
                              "tracks": [{"part": "mount", "rot": [[0.0, 1, 0, 0, 0], [0.12, q.w, q.x, q.y, q.z], [0.3, *Quaternion().slerp(q, 0.82)], [0.6, q.w, q.x, q.y, q.z]],
                                          "move": [[0.0, 0, 0, 0], [0.12, -0.05, 0.03, 0.02], [0.6, -0.06, 0.03, -0.04]]},
                                         {"part": "barrel", "slide": [[0.0, 0.0], [0.1, 0.42], [2.2, 0.42]]},
                                         {"part": "breech", "slide": [[0.0, 0.0], [0.2, 0.19], [2.2, 0.19]]}],
                              "events": [{"t": 0.0, "fx": "shell_burst", "socket": "hit", "cue": "explosion_debris"},
                                         {"t": 0.3, "fx": "fire", "socket": "fire", "cue": "fire_start", "cue_loop": "fire_small_loop"}]}
    weather(m, [(g["T"] + X * 4.6, 0.7, 0.85)], splash=0.3, rust=0.45)
    m.info["span"] = 5.4
    m.info["high"] = 1.5
    m.info["worn"] = [(0.0, 0.0, 4.2)]
    m.info["clear"] = [[(math.cos(math.radians(a)) * 2.0, math.sin(math.radians(a)) * 2.0) for a in range(0, 360, 20)]]
    m.info["file"] = {"controls": {"mount": "the game turns it to the target's bearing", "cradle": "the game lifts it to the target's height",
                                   "handwheel.traverse": "spin 60 degrees for each degree the mount turns", "handwheel.elevate": "spin 60 degrees for each degree the cradle lifts"}}
    return m


def HeavyFlakTwin(fine=True, nation="_default", name="HeavyFlakTwin"):
    """Two heavy barrels in one cradle on a broad mounting: what stands on a flak tower's gun platforms."""
    m = bits.model(name, "Twin heavy anti-aircraft gun on a fixed pedestal (our own design)", "works", nation, fine)
    m.part("gun", None, (0, 0, 0), kind="root")
    g = heavy_gun(m, "gun", (0, 0, 0), False, twin=True)
    m.use("gun")
    m.socket("ground", (2.6, 0.0, 0.05), (1, 0, 0), part="gun", note="where the blast lifts dust when she fires low")
    m.socket("hit", (0.6, 0.0, 1.4), (1, 0, 0), part="gun", note="where a hit strikes")
    bits.crew(m, "ammo.1", (-3.2, 1.2, 0.0), (1, -0.2, 0), "stand", "gun", "standing behind, passing a round up to the left gun")
    bits.crew(m, "ammo.2", (-3.2, -1.2, 0.0), (1, 0.2, 0), "stand", "gun", "standing behind, passing a round up to the right gun")
    bits.crew(m, "commander", (-3.6, -2.8, 0.0), (1, 0.3, 0), "stand", "gun", "standing, field glasses up, giving the order to fire")
    one = heavy_fire_clip()
    late = 0.1                                      # the right gun fires a tenth of a second after the left
    m.clips["fire"] = {"long": 3.3, "loop": False, "after": "hold",
                       "about": "A salvo: the left barrel, then the right a tenth of a second after; each runs back and out, throws its empty case and takes the next round. "
                                "Lies over whatever the game has laid (traverse and elevation).",
                       "tracks": [{"part": "barrel.l", "slide": one["tracks"][0]["slide"]},
                                  {"part": "barrel.r", "slide": [[0.0, 0.0]] + [[t + late, v] for t, v in one["tracks"][0]["slide"]]},
                                  {"part": "cradle", "turn": [[0.0, 0.0], [0.05, -0.5], [0.15, 0.1], [0.2, -0.45], [0.33, 0.25], [0.55, -0.08], [0.85, 0.0]]},
                                  {"part": "breech.l", "slide": one["tracks"][2]["slide"]},
                                  {"part": "breech.r", "slide": [[0.0, 0.0]] + [[t + late, v] for t, v in one["tracks"][2]["slide"][1:]]}],
                       "events": [{"t": 0.0, "fx": "muzzle_flash_big", "socket": "muzzle.l", "cue": "flak_heavy_fire_01"},
                                  {"t": 0.0, "fx": "muzzle_smoke", "socket": "muzzle.l", "cue": ""},
                                  {"t": late, "fx": "muzzle_flash_big", "socket": "muzzle.r", "cue": "flak_heavy_fire_01"},
                                  {"t": late, "fx": "muzzle_smoke", "socket": "muzzle.r", "cue": ""},
                                  {"t": 0.05, "fx": "blast_dust", "socket": "ground", "cue": ""},
                                  {"t": 0.8, "fx": "case_eject", "socket": "breech.l", "cue": "tank_shell_case"},
                                  {"t": 0.8 + late, "fx": "case_eject", "socket": "breech.r", "cue": "tank_shell_case"},
                                  {"t": 2.45, "fx": "breech_close", "socket": "breech.l", "cue": "field_gun_breech"},
                                  {"t": 2.45 + late, "fx": "breech_close", "socket": "breech.r", "cue": "field_gun_breech"}]}
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Laid and still.", "tracks": [], "events": []}
    m.clips["follow"] = {"long": 12.0, "loop": True, "about": "Following a formation across the sky (for a look only: the game trains the mount and cradle itself, 5 degrees a second at most, and spins the handwheels as they turn).",
                         "tracks": [{"part": "mount", "turn": [[0.0, -25.0], [6.0, 25.0], [12.0, -25.0]]},
                                    {"part": "cradle", "turn": [[0.0, 48.0], [3.0, 62.0], [6.0, 50.0], [9.0, 60.0], [12.0, 48.0]]},
                                    {"part": "handwheel.traverse", "turn": [[0.0, 0.0], [6.0, 3600.0], [12.0, 0.0]]},
                                    {"part": "handwheel.elevate", "turn": [[0.0, 0.0], [3.0, 1500.0], [6.0, 200.0], [9.0, 1300.0], [12.0, 0.0]]}], "events": []}
    q = Quaternion(Vector((1, 0.3, 0)).normalized(), math.radians(6.0))
    m.clips["knocked_out"] = {"long": 2.2, "loop": False, "after": "hold",
                              "about": "A hit knocks her out: the mounting is thrown askew on its pedestal, one barrel is jammed back out of battery, the breeches hang open; she burns at `fire`.",
                              "tracks": [{"part": "mount", "rot": [[0.0, 1, 0, 0, 0], [0.12, q.w, q.x, q.y, q.z], [0.3, *Quaternion().slerp(q, 0.82)], [0.6, q.w, q.x, q.y, q.z]],
                                          "move": [[0.0, 0, 0, 0], [0.12, -0.05, 0.03, 0.02], [0.6, -0.06, 0.03, -0.04]]},
                                         {"part": "barrel.l", "slide": [[0.0, 0.0], [0.1, 0.42], [2.2, 0.42]]},
                                         {"part": "breech.l", "slide": [[0.0, 0.0], [0.2, 0.19], [2.2, 0.19]]},
                                         {"part": "breech.r", "slide": [[0.0, 0.0], [0.3, 0.19], [2.2, 0.19]]}],
                              "events": [{"t": 0.0, "fx": "shell_burst", "socket": "hit", "cue": "explosion_debris"},
                                         {"t": 0.3, "fx": "fire", "socket": "fire", "cue": "fire_start", "cue_loop": "fire_small_loop"}]}
    weather(m, [(g["T"] + X * 4.6 + Y * 0.52, 0.7, 0.85), (g["T"] + X * 4.6 - Y * 0.52, 0.7, 0.85)], splash=0.3, rust=0.45)
    m.info["span"] = 6.4
    m.info["high"] = 1.6
    m.info["worn"] = [(0.0, 0.0, 4.8)]
    m.info["clear"] = [[(math.cos(math.radians(a)) * 2.6, math.sin(math.radians(a)) * 2.6) for a in range(0, 360, 20)]]
    m.info["file"] = {"controls": {"mount": "the game turns it to the target's bearing", "cradle": "the game lifts it to the target's height (both barrels lift together)",
                                   "handwheel.traverse": "spin 60 degrees for each degree the mount turns", "handwheel.elevate": "spin 60 degrees for each degree the cradle lifts"}}
    return m


# ---- the light automatic anti-aircraft gun ---------------------------------------------------------------

def light_gun(m, parent, at=(0.0, 0.0, 0.0)):
    fine = m.fine
    O = Vector(at)
    sides = 24 if fine else 10
    m.part("pedestal", parent, O, kind="fixed")
    m.use("pedestal")
    pad = [(math.cos(math.radians(30 + 60 * k)) * 0.80, math.sin(math.radians(30 + 60 * k)) * 0.80) for k in range(6)]
    # (the bed is cast all but flush with the ground: a slab standing proud reads as a display stand)
    bits.cast(m, bits.s_prism([(O.x + x, O.y + y) for x, y in pad], O.z - 0.2, O.z + 0.045), "concrete", 0.015)
    m.lathe(O + Z * 0.045, Z, [(0.0, 0.42), (0.085, 0.42), (0.09, 0.30), (0.125, 0.27), (0.675, 0.17), (0.715, 0.21), (0.775, 0.21)], "upper", sides)
    bits.bolt_ring(m, O + Z * 0.13, Z, 0.36, 10, 0.018, 0.02, "steel")
    for k in range(6):
        a = math.radians(60 * k)
        d = Vector((math.cos(a), math.sin(a), 0))
        s = Vector((-d.y, d.x, 0))
        bits.plate(m, [(0.26, 0.0), (0.40, 0.0), (0.40, 0.02), (0.25, 0.20)], 0.014, "upper", O + Z * 0.13, d, Z)
    P = O + Z * 0.82
    m.part("mount", "pedestal", P, axis=(0, 0, -1), kind="turn", note="traverse, all round: plus is clockwise seen from above")
    m.use("mount")
    m.lathe(P, Z, [(0.0, 0.24), (0.03, 0.24), (0.035, 0.20), (0.10, 0.20)], "upper", sides)
    T = P + Vector((0.0, 0, 0.42))
    for sy in (1, -1):
        bits.plate(m, [(-0.20, 0.0), (0.20, 0.0), (0.12, 0.36), (0.05, 0.47), (-0.05, 0.47), (-0.12, 0.36)], 0.025, "upper", P + Vector((0, sy * 0.22, 0.10)), X, Z)
        m.lathe(T + Y * sy * 0.22, Y * sy, [(0.0, 0.075), (0.035, 0.075), (0.045, 0.05)], "upper", 14, (False, True))
        # a seat each side for the two layers, slung out on an arm, with a foot bar
        arm0 = P + Vector((-0.10, sy * 0.24, 0.06))
        s_at = P + Vector((-0.42, sy * 0.62, -0.10))
        m.rod(arm0, s_at - Z * 0.03 + X * 0.08, 0.024, "upper", sides=8)
        seat(m, s_at + Z * 0.0, X)
        m.rod(P + Vector((0.12, sy * 0.24, 0.02)), P + Vector((0.28, sy * 0.62, -0.42)), 0.018, "upper", sides=6)
        m.rod(P + Vector((0.28, sy * 0.46, -0.42)), P + Vector((0.28, sy * 0.80, -0.42)), 0.016, "upper", sides=6)
        bits.crew(m, "layer." + ("traverse" if sy > 0 else "elevate"), s_at, (1, 0, 0), "sit", "mount", "sitting on the mounting's seat, cranking the " + ("traverse" if sy > 0 else "elevation") + " handle", seat_h=0.72)
    m.box(P + Vector((0.18, 0, 0.22)), (0.02, 0.44, 0.24), "upper")
    bits.crew(m, "loader", P + Vector((-0.75, 0.05, -0.82)), (1, 0, 0), "stand", "mount", "standing behind on the ground, dropping clips of four rounds into the hopper")
    m.socket("fire", P + Z * 0.3, (0, 0, 1), part="mount")
    for sy, what in ((1, "traverse"), (-1, "elevate")):
        c = P + Vector((-0.02, sy * 0.44, 0.30))
        m.part("handwheel." + what, "mount", c, axis=(0, sy, 0), kind="spin")
        m.use("handwheel." + what)
        m.rod(c - Y * sy * 0.19, c, 0.014, "steel", sides=6)
        # a crank with two handles
        m.rod(c + Vector((0, 0, -0.11)), c + Vector((0, 0, 0.11)), 0.012, "steel", sides=6)
        m.rod(c + Vector((0, 0, 0.11)), c + Vector((0, sy * 0.09, 0.11)), 0.014, "timber.dark", sides=8)
        m.rod(c + Vector((0, 0, -0.11)), c + Vector((0, sy * 0.09, -0.11)), 0.014, "timber.dark", sides=8)
    m.part("cradle", "mount", T, axis=(0, -1, 0), limits=[-5.0, 88.0], kind="turn", note="elevation: plus lifts the muzzle")
    m.use("cradle")
    m.rod(T - Y * 0.21, T + Y * 0.21, 0.045, "steel", sides=14)
    # the body: a long box with the feed hopper on top and the case chute behind
    body = bits.s_box((T.x - 0.62, T.y - 0.13, T.z - 0.15), (T.x + 0.52, T.y + 0.13, T.z + 0.15))
    bits.s_bevel(body, 0.018, 2)
    bits.take(m, body, "upper")
    hop = bits.s_wedge([(T.x - 0.30, T.y - 0.07, T.z + 0.14), (T.x - 0.02, T.y - 0.07, T.z + 0.14), (T.x - 0.02, T.y + 0.07, T.z + 0.14), (T.x - 0.30, T.y + 0.07, T.z + 0.14)],
                       [(T.x - 0.34, T.y - 0.10, T.z + 0.50), (T.x + 0.02, T.y - 0.10, T.z + 0.50), (T.x + 0.02, T.y + 0.10, T.z + 0.50), (T.x - 0.34, T.y + 0.10, T.z + 0.50)])
    hole = bits.s_box((T.x - 0.31, T.y - 0.075, T.z + 0.2), (T.x - 0.01, T.y + 0.075, T.z + 0.6))
    bits.s_cut(hop, hole)
    bits.s_bevel(hop, 0.008, 1)
    bits.take(m, hop, "upper")
    for k in range(4):                                                              # a clip of four rounds standing in the hopper
        shell(m, T + Vector((-0.28 + 0.075 * k + 0.02, 0, 0.24)), Vector((0.15, 0, 1)), 0.04, 0.42, "upper")
    m.box(T + Vector((-0.70, 0, -0.10)), (0.18, 0.16, 0.10), "upper", pitch=-25)       # the chute the empties leave by
    # the recoil spring's housing round the barrel's root, and the ring sight on its arm
    m.lathe(T + X * 0.52, X, [(0.0, 0.11), (0.02, 0.12), (0.55, 0.10), (0.57, 0.075)], "upper", sides)
    m.rod(T + Vector((0.30, 0.13, 0.10)), T + Vector((0.30, 0.34, 0.34)), 0.012, "steel", sides=6)
    c = T + Vector((0.30, 0.34, 0.44))
    for rr in (0.10, 0.06, 0.025):
        bits.tube(m, [c + Vector((0, math.cos(2 * math.pi * k / 20) * rr, math.sin(2 * math.pi * k / 20) * rr)) for k in range(20)], 0.004, "steel", 4, closed=True)
    for a in range(4):
        d = Vector((0, math.cos(math.pi * a / 2 + 0.785), math.sin(math.pi * a / 2 + 0.785)))
        m.rod(c + d * 0.025, c + d * 0.10, 0.003, "steel", sides=4)
    m.rod(T + Vector((-0.50, 0.13, 0.10)), T + Vector((-0.50, 0.34, 0.44)), 0.01, "steel", sides=6)
    m.ball(T + Vector((-0.50, 0.34, 0.44)), 0.012, "steel", 8, 6)
    m.part("barrel", "cradle", T, slide=(-1, 0, 0), slide_limits=[0.0, 0.22], kind="slide", note="recoil: metres back along the bore")
    m.use("barrel")
    m.lathe(T + X * 0.50, X, [(0.0, 0.05), (0.62, 0.045), (1.75, 0.032), (1.77, 0.036), (1.80, 0.036)], "steel", 20 if fine else 8, (True, False))
    m.lathe(T + X * 2.30, X, [(0.0, 0.036), (0.03, 0.036), (0.20, 0.07), (0.205, 0.066)], "steel", 20 if fine else 8, (False, False))      # the flash hider
    m.lathe(T + X * 2.505, -X, [(0.0, 0.066), (0.19, 0.022), (0.5, 0.02)], "void", 16, (False, True))
    m.socket("muzzle", T + X * 2.52, (1, 0, 0), part="barrel")
    m.socket("eject", T + Vector((-0.82, 0, -0.16)), (-0.8, 0, -0.6), part="cradle", note="where the empties fall")
    return {"T": T, "P": P}


def light_fire_clip(rounds=4, rate=2.0):
    long = rounds / rate
    slide, turn, events = [[0.0, 0.0]], [[0.0, 0.0]], []
    for k in range(rounds):
        t = k / rate
        slide += [[t + 0.03, 0.20], [t + 0.22, 0.02], [t + 0.3, 0.0]]
        turn += [[t + 0.03, -0.35], [t + 0.12, 0.15], [t + 0.25, 0.0]]
        events.append({"t": round(t, 3), "fx": "muzzle_flash", "socket": "muzzle", "cue": "flak_light_fire" if k == 0 else ""})
        events.append({"t": round(t + 0.12, 3), "fx": "case_eject", "socket": "eject", "cue": ""})
    slide.append([long, 0.0])
    turn.append([long, 0.0])
    return {"long": long, "loop": True, "about": f"A clip of {rounds} rounds, two a second, looping while she fires. Lies over what the game has laid.",
            "tracks": [{"part": "barrel", "slide": slide}, {"part": "cradle", "turn": turn}], "events": events}


def LightFlak(fine=True, nation="_default", name="LightFlak"):
    m = bits.model(name, "Light automatic anti-aircraft gun on a fixed pedestal (our own design)", "works", nation, fine)
    m.part("gun", None, (0, 0, 0), kind="root")
    g = light_gun(m, "gun", (0, 0, 0))
    m.use("gun")
    m.socket("hit", (0.3, 0.0, 1.0), (1, 0, 0), part="gun")
    m.clips["fire"] = light_fire_clip()
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Laid and still.", "tracks": [], "events": []}
    q = Quaternion(Vector((1, -0.4, 0)).normalized(), math.radians(9.0))
    m.clips["knocked_out"] = {"long": 1.6, "loop": False, "after": "hold", "about": "A hit knocks her out: the mounting thrown askew, the barrel jammed back.",
                              "tracks": [{"part": "mount", "rot": [[0.0, 1, 0, 0, 0], [0.1, q.w, q.x, q.y, q.z]], "move": [[0.0, 0, 0, 0], [0.1, -0.03, -0.02, -0.02]]},
                                         {"part": "barrel", "slide": [[0.0, 0.0], [0.08, 0.14], [1.6, 0.14]]}],
                              "events": [{"t": 0.0, "fx": "shell_burst", "socket": "hit", "cue": "explosion_debris"}, {"t": 0.2, "fx": "fire", "socket": "fire", "cue": "fire_start", "cue_loop": "fire_small_loop"}]}
    weather(m, [(g["T"] + X * 2.5, 0.4, 0.85)], splash=0.2, rust=0.45)
    m.info["span"] = 3.0
    m.info["high"] = 1.45
    m.info["worn"] = [(0.0, 0.0, 2.4)]
    m.info["clear"] = [[(math.cos(math.radians(a)) * 1.0, math.sin(math.radians(a)) * 1.0) for a in range(0, 360, 30)]]
    return m


BUILDERS = {
    "HeavyFlak": HeavyFlak,
    "HeavyFlakTwin": HeavyFlakTwin,
    "LightFlak": LightFlak,
}
