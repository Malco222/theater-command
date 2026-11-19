"""The Eastern Front's scenery: the village and the trees the battles stand among, our own designs.

    python scenery_east.py <out folder> [name,name,...|all] [json glb]     the game's files (SET-FORMAT.md's shape)
    python scenery_east.py --picture <png> [names]                          a line-up of them, for review

Every model is built twice: `near` (fine) and `far` (the plain shape for whole-view and working height), in the
vehicles' file format (rigid.py), kind "scenery", metres, x forward, z up from the ground at the foot. Added for
scenery (SET-FORMAT.md): `sway` point by point (0 to 1, how far the wind moves it), `foot_m` and `high_m`.

The models:
    Izba            a log house of Russia and Belarus: crossed-log corners, carved white window frames and painted
                    shutters, a steep plank roof, a brick chimney
    Khata           the Ukrainian cottage: whitewashed clay walls on a brown plinth, a thick thatched hip roof
    BurntChimney    what the war leaves of a village: the stove and its chimney standing in the ash, charred logs
    Birch, BirchBare  the white birch in leaf and bare (winter)
    Spruce, SpruceSnow  the dark spruce, and laden with snow
    Haystack        a tall stack round its pole
    WattleFence     a woven fence of withies between stakes, 4 m
    WellSweep       a village well with its long counterweighted sweep
    TelegraphPole   a pole with its crossbar and insulators
"""

import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LAND = os.path.normpath(os.path.join(HERE, "..", "land"))
sys.path.insert(0, LAND)

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit  # noqa: E402
import rigid  # noqa: E402

PAINTS = {
    "log": "7C6046", "log_end": "A3845E", "plank": "6F6A60", "board": "77654E", "thatch": "9C8256", "thatch_dark": "6E5C40",
    "whitewash": "E0DACB", "plinth": "7E5E42", "frame": "DCD8CC", "shutter": "4E7A86", "glass": "1D2124", "door": "5C4A38",
    "brick": "8E4E3C", "plaster": "C9C1B1", "soot": "2E2A27", "char": "1D1B19", "ash": "5D5853",
    "birch_bark": "DCD7CB", "birch_mark": "2F2C29", "birch_leaf": "7C9246", "twig": "4E4038",
    "spruce": "2D4535", "spruce_dark": "23372B", "bark": "4A3C30", "snow": "E6EAEE",
    "hay": "A68F5B", "hay_old": "7F6F4F", "wood": "6E5A44", "withy": "7A6648", "pole": "5C4C3C", "insulator": "9DB2A9",
}
Z = Vector((0.0, 0.0, 1.0))


def new(name, title, fine):
    m = rigid.Model(name, title, "scenery", PAINTS, fine)
    m.part("body")
    m.use("body")
    return m


def slab(m, pts, thick, paint):
    """A flat piece: a plane polygon (points going round) given a thickness along its facing."""
    pts = [Vector(p) for p in pts]
    n = (pts[1] - pts[0]).cross(pts[2] - pts[0]).normalized()
    c = sum(pts, Vector()) / len(pts)
    k = len(pts)
    verts = pts + [p + n * thick for p in pts]
    faces = [tuple(range(k)), tuple(range(2 * k - 1, k - 1, -1))] + [(i, (i + 1) % k, k + (i + 1) % k, k + i) for i in range(k)]
    mid = c + n * thick / 2
    m.add(verts, faces, paint, False, lambda p: p - mid)


def gable_roof(m, L, W, z0, pitch, over_x, over_y, thick, paint, ridge_paint=None):
    """Two roof planes meeting at a ridge along x, eaves at z0, overhanging the walls."""
    rise = (W / 2 + over_y) * math.tan(math.radians(pitch))
    zr = z0 + rise - over_y * math.tan(math.radians(pitch))
    ze = z0 - over_y * math.tan(math.radians(pitch))
    x0, x1 = -L / 2 - over_x, L / 2 + over_x
    for s in (1.0, -1.0):
        e = s * (W / 2 + over_y)
        pts = [(x0, 0.0, zr), (x1, 0.0, zr), (x1, e, ze), (x0, e, ze)] if s > 0 else [(x1, 0.0, zr), (x0, 0.0, zr), (x0, e, ze), (x1, e, ze)]
        slab(m, pts, thick, paint)
    if ridge_paint:
        m.rod((x0, 0.0, zr + thick * 0.6), (x1, 0.0, zr + thick * 0.6), thick * 1.1, ridge_paint, sides=6 if m.fine else 4)
    return zr


def window(m, at, face, w, h, frame="frame", shutter="shutter", carved=True):
    """A window on a wall facing `face` (a unit x or y vector): dark glass, a white frame with a peaked head, and two
    painted shutters standing open."""
    at, face = Vector(at), Vector(face)
    side = Z.cross(face).normalized()
    yaw = math.degrees(math.atan2(face.y, face.x))
    m.box(at + face * 0.01, (0.06, w, h), "glass", yaw=yaw)
    t = 0.07 if m.fine else 0.09
    for dz in (h / 2 + t / 2, -h / 2 - t / 2):
        m.box(at + face * 0.04 + Z * dz, (0.06, w + 2 * t, t), frame, yaw=yaw)
    for ds in (1.0, -1.0):
        m.box(at + face * 0.04 + side * ds * (w / 2 + t / 2), (0.06, t, h), frame, yaw=yaw)
    if m.fine:
        m.box(at + face * 0.035, (0.04, 0.04, h), frame, yaw=yaw)                                    # the mullion
        m.box(at + face * 0.035 + Z * (h * 0.18), (0.04, w, 0.035), frame, yaw=yaw)
    if carved:
        top = at + face * 0.045 + Z * (h / 2 + t)
        hw = w / 2 + t * 1.6
        slab(m, [top + side * hw, top - side * hw, top + Z * 0.26], 0.05, frame)                   # the carved peak
        m.box(at + face * 0.05 - Z * (h / 2 + t + 0.05), (0.07, w + 3 * t, 0.06), frame, yaw=yaw)    # the sill
    if shutter:
        for ds in (1.0, -1.0):
            m.box(at + face * 0.07 + side * ds * (w / 2 + t + w * 0.27), (0.04, w * 0.5, h + 0.05), shutter, yaw=yaw)


# ================================================================================================ the houses
def izba(fine=True):
    """A log house, 6.6 x 5.4 m, its gable end with three windows to the street (+x)."""
    m = new("Izba", "Izba: log house (our own design)", fine)
    L, W, r = 6.6, 5.4, 0.13
    courses = 10 if fine else 1
    step = 2 * r * 0.9
    m.box((0.0, 0.0, 0.12), (L + 0.1, W + 0.1, 0.24), "plinth")                                # the stone footing
    if fine:
        for k in range(courses):
            z = 0.24 + r + k * step
            for y in (W / 2, -W / 2):
                m.rod((-L / 2 - 0.28, y, z), (L / 2 + 0.28, y, z), r * (1.0 - 0.02 * (k % 3)), "log", sides=6)
            for x in (L / 2, -L / 2):
                m.rod((x, -W / 2 - 0.28, z + step / 2), (x, W / 2 + 0.28, z + step / 2), r * (1.0 - 0.02 * ((k + 1) % 3)), "log", sides=6)
            for x in (L / 2 + 0.28, -L / 2 - 0.28):                                                 # the log ends, cut pale
                for y in (W / 2, -W / 2):
                    m.disc((x, y, z), (math.copysign(1, x), 0, 0), r * 0.92, "log_end", sides=6)
        top = 0.24 + r + courses * step
    else:
        top = 2.75
        m.box((0.0, 0.0, 0.24 + (top - 0.24) / 2), (L + 0.2, W + 0.2, top - 0.24), "log")
    # the gables: boards, closing the triangle under the roof, with a small window high in the street end
    pitch = 42.0
    rise = (W / 2) * math.tan(math.radians(pitch))
    for x in (L / 2 + 0.04, -L / 2 - 0.04):
        s = math.copysign(1.0, x)
        pts = [(x, W / 2, top), (x, -W / 2, top), (x, 0.0, top + rise)] if s > 0 else [(x, -W / 2, top), (x, W / 2, top), (x, 0.0, top + rise)]
        slab(m, pts, 0.06 * -s, "board")
    zr = gable_roof(m, L, W, top, pitch, 0.45, 0.55, 0.07, "plank", "board" if fine else None)
    # windows: three on the street end, two on the long side, under carved heads
    face_x = Vector((1.0, 0.0, 0.0))
    wx = L / 2 + r + 0.02
    for y in (-1.55, 0.0, 1.55):
        window(m, (wx, y, 1.45), face_x, 0.72, 0.95, carved=fine, shutter="shutter" if fine else None)
    window(m, (wx - 0.02, 0.0, top + rise * 0.42), face_x, 0.42, 0.5, shutter=None, carved=False)
    face_y = Vector((0.0, 1.0, 0.0))
    for x in (-1.3, 1.3):
        window(m, (x, W / 2 + r + 0.02, 1.45), face_y, 0.72, 0.95, carved=fine, shutter="shutter" if fine else None)
    # the porch and door on the far long side, the chimney through the ridge
    m.box((-1.6, -W / 2 - 0.55, 0.3), (1.6, 1.1, 0.12), "board")
    m.box((-1.6, -W / 2 - r - 0.03, 1.2), (0.95, 0.08, 1.9), "door")
    if fine:
        for x in (-2.3, -0.9):
            m.rod((x, -W / 2 - 1.0, 0.3), (x, -W / 2 - 1.0, 2.3), 0.07, "wood", sides=6)
        slab(m, [(-0.7, -W / 2 - 0.05, 2.45), (-2.5, -W / 2 - 0.05, 2.45), (-2.5, -W / 2 - 1.2, 2.2), (-0.7, -W / 2 - 1.2, 2.2)], 0.05, "plank")
    m.box((-0.9, 0.0, zr - 0.3), (0.62, 0.62, 2.0), "brick")
    m.box((-0.9, 0.0, zr + 0.72), (0.76, 0.76, 0.1), "brick")
    m.info["about"] = {"foot_m": round(math.hypot(L / 2 + 0.5, W / 2 + 1.2), 2), "sway": "none"}
    return m


def khata(fine=True):
    """The Ukrainian cottage: whitewashed clay walls, a brown plinth, a thick thatched hip roof, 7.4 x 4.8 m."""
    m = new("Khata", "Khata: whitewashed cottage (our own design)", fine)
    L, W, H = 7.4, 4.8, 2.35
    m.house(-L / 2, L / 2, W / 2, 0.0, 0.45, "plinth", r=0.25 if fine else 0.0, steps=2 if fine else 1)
    m.house(-L / 2 + 0.06, L / 2 - 0.06, W / 2 - 0.06, 0.45, H - 0.45, "whitewash", r=0.25 if fine else 0.0, steps=2 if fine else 1)
    # the thatch: a thick lip at the eaves, then the hip roof up to a short ridge
    o = 0.6
    eave = m.round_rect(-L / 2 - o, L / 2 + o, -W / 2 - o, W / 2 + o, 0.5 if fine else 0.0, 2 if fine else 1)
    m.prism(eave, H - 0.28, H, "thatch_dark", bottom=True)
    ridge = m.round_rect(-L * 0.27, L * 0.27, -0.12, 0.12, 0.1 if fine else 0.0, 2 if fine else 1)
    m.prism(eave, H, H + 2.6, "thatch", top=ridge)
    if fine:
        for x in (-L * 0.27, L * 0.27):                                             # the ridge's bound ends
            m.ball((x, 0.0, H + 2.62), (0.35, 0.22, 0.16), "thatch_dark", u=8, v=4)
    # small windows with blue frames, the door, the chimney
    face_y = Vector((0.0, 1.0, 0.0))
    for x in (-2.2, 0.3, 2.4):
        window(m, (x, W / 2 + 0.02, 1.4), face_y, 0.6, 0.72, frame="shutter", shutter=None, carved=False)
    window(m, (L / 2 + 0.02, 0.0, 1.4), Vector((1.0, 0.0, 0.0)), 0.6, 0.72, frame="shutter", shutter=None, carved=False)
    m.box((-0.9, W / 2 + 0.02, 1.0), (0.08, 0.9, 1.75), "door")
    m.box((1.2, -0.6, H + 2.0), (0.55, 0.55, 1.4), "plaster")
    m.box((1.2, -0.6, H + 2.74), (0.65, 0.65, 0.08), "plaster")
    m.info["about"] = {"foot_m": round(math.hypot(L / 2 + o, W / 2 + o), 2), "sway": "none"}
    return m


def burnt_chimney(fine=True):
    """The stove left standing when the house has burnt: its block, the chimney, charred stubs of the walls, ash."""
    m = new("BurntChimney", "Burnt-out house: the stove and chimney (our own design)", fine)
    rng = random.Random(5)
    m.prism(m.oval(0.0, 0.0, 3.9, 3.3, 14 if fine else 8), 0.0, 0.04, "ash")
    m.box((0.2, 0.3, 0.8), (1.6, 1.45, 1.6), "plaster")
    m.box((0.2, 0.3, 1.66), (1.7, 1.55, 0.12), "soot")
    m.box((0.98, 0.3, 0.6), (0.06, 0.5, 0.4), "char")                                             # the stove's mouth
    m.box((0.45, 0.55, 3.0), (0.6, 0.6, 2.7), "brick")
    m.box((0.45, 0.55, 4.38), (0.72, 0.72, 0.1), "soot")
    if fine:
        m.box((0.45, 0.55, 2.2), (0.62, 0.62, 1.0), "soot")                                      # the smoke's black up the stack
    L, W = 6.2, 5.0
    for k, (a, b) in enumerate((((-L / 2, W / 2), (L / 2, W / 2)), ((L / 2, W / 2), (L / 2, -W / 2)), ((L / 2, -W / 2), (-L / 2, -W / 2)),
                                ((-L / 2, -W / 2), (-L / 2, W / 2)))):
        courses = rng.randint(1, 3) if fine else 1
        for c in range(courses):
            f0, f1 = rng.uniform(0.0, 0.25), rng.uniform(0.55, 1.0)
            pa = Vector((a[0] + (b[0] - a[0]) * f0, a[1] + (b[1] - a[1]) * f0, 0.13 + c * 0.23))
            pb = Vector((a[0] + (b[0] - a[0]) * f1, a[1] + (b[1] - a[1]) * f1, 0.13 + c * 0.23))
            m.rod(pa, pb, 0.12, "char", sides=6 if fine else 4)
    for i in range(4 if fine else 2):                                                             # fallen beams
        x, y, a = rng.uniform(-2.0, 2.4), rng.uniform(-1.8, 1.8), rng.uniform(0, math.pi)
        long = rng.uniform(2.0, 3.6)
        d = Vector((math.cos(a), math.sin(a), 0.0)) * long / 2
        lift = Vector((0.0, 0.0, rng.uniform(0.0, 0.6)))
        m.rod(Vector((x, y, 0.1)) - d, Vector((x, y, 0.1)) + d + lift, rng.uniform(0.08, 0.12), "char", sides=6 if fine else 4)
    m.info["about"] = {"foot_m": 4.0, "sway": "none"}
    return m


# ================================================================================================ the trees
def _crown_clumps(m, rng, at, n, size, paint, droop=0.0, u=7, v=4):
    for i in range(n):
        a = rng.uniform(0, 2 * math.pi)
        rr = size * rng.uniform(0.2, 1.0)
        c = Vector((at.x + math.cos(a) * rr, at.y + math.sin(a) * rr, at.z + rng.uniform(-0.5, 0.6) * size - droop * rr))
        s = size * rng.uniform(0.28, 0.42)
        m.ball(c, (s, s * rng.uniform(0.8, 1.1), s * rng.uniform(0.55, 0.8)), paint, u=u, v=v)


def _hash(v, k=0.0):
    x = math.sin(v.x * 12.9898 + v.y * 78.233 + v.z * 37.719 + k) * 43758.5453
    return x - math.floor(x)


def birch(fine=True, leaves=True):
    """A birch about 15 m: a slim white stem with dark dashes (never rings), leaning a little; boughs rising and then
    hanging. In leaf an open oval crown of many clumps of different sizes with sky between them; bare, a spray of
    boughs and twigs."""
    name = "Birch" if leaves else "BirchBare"
    m = new(name, f"Birch{'' if leaves else ', bare'} (our own design)", fine)
    rng = random.Random(11 if leaves else 12)
    h = 15.0
    lean = Vector((0.45, -0.25, h))

    def marks(mid, n):
        if mid.z < 0.9 and _hash(mid, 1.0) < 0.75:
            return "birch_mark"                                              # the rough dark foot
        band = math.sin(mid.z * 2.3 + 2.0 * math.atan2(n.y, n.x) + 1.7 * math.sin(mid.z * 0.7))
        return "birch_mark" if band > 0.86 else "birch_bark"
    sides = 8 if fine else 5
    rings = 14 if fine else 3
    prof = [(h * t, 0.19 * (1.0 - t) ** 1.1 + 0.02) for t in [i / rings for i in range(rings + 1)]]
    m.lathe((0.0, 0.0, 0.0), lean, prof, marks, sides=sides)
    axis = lean.normalized()
    boughs = (16 if leaves else 22) if fine else 6
    ends = []
    for j in range(boughs):
        f0 = 0.35 + 0.6 * (j + rng.uniform(0, 1)) / boughs
        at = axis * h * f0
        a = j * 2.4 + rng.uniform(-0.4, 0.4)                                  # (round the stem like leaves on a shoot)
        long = h * rng.uniform(0.16, 0.24) * (1.25 - f0)
        mid = at + Vector((math.cos(a) * long * 0.55, math.sin(a) * long * 0.55, long * 0.6))
        end = mid + Vector((math.cos(a) * long * 0.5, math.sin(a) * long * 0.5, -long * 0.3))       # (then hanging)
        m.rod(at, mid, 0.07 * (1.15 - f0), "birch_bark", rb=0.03, sides=5 if fine else 3)
        m.rod(mid, end, 0.03, "twig", rb=0.012, sides=4 if fine else 3)
        ends.append((mid, end, long))
        if fine and not leaves:
            for k in range(4):
                b = mid + (end - mid) * rng.uniform(0.0, 0.9)
                d = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1.3, -0.4))) * long * 0.3
                m.rod(b, b + d, 0.014, "twig", rb=0.004, sides=3)
                m.rod(b + d * 0.5, b + d * 0.5 + Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), -1.0)) * long * 0.15, 0.007, "twig", rb=0.003,
                      sides=3)
    if leaves:
        # the crown: clumps scattered through an oval round the boughs' ends, bigger and denser outside, gaps between
        n = 34 if fine else 9
        c0 = axis * h * 0.68
        for i in range(n):
            mid, end, long = ends[i % len(ends)]
            p = mid + (end - mid) * rng.uniform(0.2, 1.0) + Vector((rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), rng.uniform(-0.8, 0.5)))
            off = p - c0
            off.z *= 0.5
            if off.length > 3.4:                                              # (kept inside the crown's oval)
                p = c0 + (p - c0) * (3.4 / max(off.length, 1e-6))
            s = rng.uniform(0.55, 1.15) if fine else rng.uniform(1.0, 1.6)
            m.ball(p, (s, s * rng.uniform(0.75, 1.1), s * rng.uniform(0.6, 0.85)), "birch_leaf", u=6 if fine else 5, v=4)
    m.info["about"] = {"foot_m": 0.3, "sway": "crown"}
    m.info["crease"] = 80.0                                                   # (the clumps shaded round, not cut)
    return m


def spruce(fine=True, snow=False):
    """A spruce about 18 m: a narrow dark spire of drooping tiers, each tier a ragged star of branch masses (tips and
    gaps); laden, patches of snow lie on the tiers' tops, never the whole tree white."""
    name = "SpruceSnow" if snow else "Spruce"
    m = new(name, f"Spruce{', snow-laden' if snow else ''} (our own design)", fine)
    rng = random.Random(21 if snow else 22)
    h = 18.0
    m.rod((0.0, 0.0, 0.0), (0.0, 0.0, h * 0.97), 0.24, "bark", rb=0.03, sides=6 if fine else 4)
    tiers = 17 if fine else 6
    sides = 14 if fine else 6
    for k in range(tiers):
        t = k / tiers
        z0 = h * (0.1 + 0.86 * t) + rng.uniform(-0.25, 0.25)
        rad = 2.7 * (1.0 - t) ** 0.9 + 0.3
        drop = rad * 0.55 + 0.35
        ring, out = [], []
        for i in range(sides):
            a = 2 * math.pi * (i + rng.uniform(-0.25, 0.25)) / sides + k * 0.7
            tipf = (1.12 if i % 2 == 0 else 0.68) if fine else 1.0
            rr = rad * tipf * rng.uniform(0.8, 1.15)
            out.append(Vector((math.cos(a) * rr, math.sin(a) * rr, z0 - drop * tipf * rng.uniform(0.8, 1.2))))
        under = [Vector((p.x * 0.55, p.y * 0.55, p.z + drop * 0.35)) for p in out]
        tip = Vector((0.0, 0.0, z0 + 0.15))

        def paint(mid, n, snow_on=snow):
            if snow_on and n.z > 0.45 and _hash(mid, 3.0) < 0.55:
                return "snow"
            return "spruce" if n.z > -0.2 else "spruce_dark"
        verts = out + under + [tip]
        nn = sides
        mid0 = Vector((0.0, 0.0, z0 - drop * 0.4))
        m.add(verts, [(2 * nn, i, (i + 1) % nn) for i in range(nn)], paint, True, lambda p, c=mid0: p - c)      # the skirt's top
        under_faces = [(i, nn + i, nn + (i + 1) % nn, (i + 1) % nn) for i in range(nn)] + [(nn + i, 2 * nn, nn + (i + 1) % nn) for i in range(nn)]
        m.add(verts, under_faces, paint, True, lambda p: -Z)                                                    # and beneath it
    m.rod((0.0, 0.0, h * 0.95), (0.0, 0.0, h + 0.6), 0.05, "spruce", rb=0.01, sides=4)
    m.info["about"] = {"foot_m": 0.4, "sway": "crown"}
    return m


# ================================================================================================ the yard
def haystack(fine=True):
    m = new("Haystack", "Haystack round its pole (our own design)", fine)
    sides = 12 if fine else 7
    prof = [(0.0, 1.45), (0.5, 1.62), (1.4, 1.6), (2.4, 1.3), (3.2, 0.82), (3.75, 0.32), (3.95, 0.08)]
    rng = random.Random(3)
    rings = []
    for d, r in (prof if fine else prof[::2] + [prof[-1]]):
        rings.append([Vector((math.cos(2 * math.pi * k / sides) * r * rng.uniform(0.93, 1.07), math.sin(2 * math.pi * k / sides) * r * rng.uniform(0.93, 1.07), d))
                      for k in range(sides)])
    m.skin(rings, lambda mid, n: "hay_old" if (mid.z < 0.5 or n.z > 0.75) else "hay", caps=(False, True))
    m.rod((0.0, 0.0, 3.6), (0.05, 0.02, 4.9), 0.06, "pole", rb=0.04, sides=5)
    if fine:
        for a in (0.4, 2.5, 4.4):                                              # the binding poles laid over the top
            d = Vector((math.cos(a), math.sin(a), 0.0))
            m.rod(d * 0.25 + Vector((0, 0, 3.92)), d * 1.45 + Vector((0, 0, 2.6)), 0.04, "pole", sides=4)
    m.info["about"] = {"foot_m": 1.65, "sway": "none"}
    return m


def wattle_fence(fine=True):
    """4 m of woven fence between stakes, 1.25 m high: withies running in and out of the stakes."""
    m = new("WattleFence", "Wattle fence, 4 m (our own design)", fine)
    stakes = [x * 0.8 - 2.0 for x in range(6)]
    for i, x in enumerate(stakes):
        m.rod((x, 0.0, 0.0), (x, 0.0, 1.38 + 0.06 * (i % 2)), 0.045, "pole", rb=0.035, sides=5 if fine else 3)
    if fine:
        for row in range(9):
            z = 0.18 + row * 0.12
            pts = [Vector((-2.05 + 4.1 * k / 20, 0.05 * math.sin(math.pi * k / 2.5 + row * math.pi), z + 0.015 * math.sin(k * 1.3 + row)))
                   for k in range(21)]
            for a, b in zip(pts, pts[1:]):
                m.rod(a, b, 0.022, "withy", sides=4, caps=(False, False))
    else:
        m.box((0.0, 0.0, 0.7), (4.1, 0.1, 1.05), "withy")
    m.info["about"] = {"foot_m": 2.05, "sway": "none"}
    return m


def well_sweep(fine=True):
    """The village well: a log box over the shaft, a forked post, and the long sweep with its weight and bucket."""
    m = new("WellSweep", "Well with its sweep (our own design)", fine)
    for k in range(4 if fine else 1):
        z = 0.12 + k * 0.22
        if fine:
            for y in (0.55, -0.55):
                m.rod((-0.7, y, z), (0.7, y, z), 0.11, "log", sides=6)
            for x in (0.55, -0.55):
                m.rod((x, -0.7, z + 0.11), (x, 0.7, z + 0.11), 0.11, "log", sides=6)
        else:
            m.box((0.0, 0.0, 0.5), (1.3, 1.3, 1.0), "log")
    if fine:
        m.box((0.0, 0.0, 0.98), (0.95, 0.95, 0.04), "board")
    post = Vector((-2.4, 0.0, 0.0))
    m.rod(post, post + Vector((0, 0, 3.8)), 0.13, "wood", rb=0.1, sides=6 if fine else 4)
    pivot = post + Vector((0, 0, 3.75))
    for s in (1.0, -1.0):
        m.rod(pivot + Vector((0, s * 0.05, -0.4)), pivot + Vector((0, s * 0.18, 0.25)), 0.06, "wood", sides=4)
    # the sweep: up to the well's side, its butt end down toward the ground with the weight lashed on
    hi = Vector((0.0, 0.0, 6.6))
    lo = pivot + (pivot - hi).normalized() * 2.6
    m.rod(lo, hi, 0.09, "wood", rb=0.05, sides=6 if fine else 4)
    m.box(lo + (hi - lo).normalized() * 0.4, (0.5, 0.4, 0.35), "log", pitch=math.degrees(math.atan2(hi.z - lo.z, hi.x - lo.x)))
    m.rod(hi, Vector((0.0, 0.0, 1.6)), 0.03, "pole", sides=4)
    m.lathe((0.0, 0.0, 1.25), Z, [(0.0, 0.14), (0.35, 0.17)], "wood", sides=8 if fine else 5, caps=(True, False))
    m.info["about"] = {"foot_m": 2.6, "sway": "none"}
    return m


def telegraph_pole(fine=True):
    m = new("TelegraphPole", "Telegraph pole (our own design)", fine)
    m.rod((0.0, 0.0, 0.0), (0.0, 0.0, 7.2), 0.13, "pole", rb=0.09, sides=7 if fine else 4)
    m.box((0.0, 0.0, 6.7), (0.1, 1.5, 0.1), "pole")
    for y in ((-0.65, -0.22, 0.22, 0.65) if fine else (-0.65, 0.65)):
        m.lathe((0.0, y, 6.75), Z, [(0.0, 0.025), (0.06, 0.05), (0.13, 0.03)], "insulator", sides=6 if fine else 4)
    m.info["about"] = {"foot_m": 0.2, "sway": "none"}
    return m


BUILDERS = {
    "Izba": izba, "Khata": khata, "BurntChimney": burnt_chimney,
    "Birch": lambda fine=True: birch(fine, True), "BirchBare": lambda fine=True: birch(fine, False),
    "Spruce": lambda fine=True: spruce(fine, False), "SpruceSnow": lambda fine=True: spruce(fine, True),
    "Haystack": haystack, "WattleFence": wattle_fence, "WellSweep": well_sweep, "TelegraphPole": telegraph_pole,
}


# ================================================================================================ the files
def sway_of(points, high, how):
    """Point by point: nothing below a third of the height, then growing to 1 at the top (crowns and their boughs)."""
    if how != "crown":
        return [0] * (len(points) // 3)
    out = []
    for z in points[2::3]:
        t = max(0.0, (z - high * 0.3) / (high * 0.7))
        out.append(round(min(1.0, t) ** 1.4, 2))
    return out


def one(name, out, kinds):
    kit.fresh()
    far_m = BUILDERS[name](False)
    ob, arm = rigid.to_blender(far_m, crease=far_m.info.get("crease", 38.0))
    far = rigid.gather(far_m, ob)
    kit.fresh()
    m = BUILDERS[name](True)
    ob, arm = rigid.to_blender(m, crease=m.info.pop("crease", 38.0))
    far_m.info.pop("crease", None)
    near = rigid.gather(m, ob)
    about = m.info.pop("about", {})
    high = max(near["points"][2::3])
    for g in (near, far):
        g["sway"] = sway_of(g["points"], high, about.get("sway"))
    extra = {"foot_m": about.get("foot_m", 1.0), "high_m": round(high, 2), "region": "east"}
    sizes = {}
    if "json" in kinds:
        p = os.path.join(out, name + ".json")
        rigid.write_json(m, near, far, p, extra)
        sizes["json"] = os.path.getsize(p)
    if "glb" in kinds:
        sizes["glb"] = os.path.getsize(kit.export_glb(os.path.join(out, name + ".glb"), [ob, arm]))
    tris, plain = len(near["triangles"]) // 3, len(far["triangles"]) // 3
    print(f"{name}: {tris} triangles (plain {plain}), high {high:.1f} m; " + ", ".join(f"{k} {s / 1e3:.0f} kB" for k, s in sizes.items()), flush=True)
    return {"name": name, "title": m.title, "triangles": tris, "plain": plain, "high_m": round(high, 2), "foot_m": extra["foot_m"]}


def picture(path, names, ground="steppe", fine=True, spp=24, size=(1600, 900), cam=None, place=None):
    """The models in a row on a field, as the battle camera would see a village street."""
    sys.path.insert(0, LAND)
    import ground as G
    import stage
    kit.fresh()
    if ground == "winter":
        import winter
        winter.install(0.0, 0.0)
        G.Field(kind="winter", density=4.0)
    else:
        G.Field(kind=ground, density=10.0, trees=False)
    x = 0.0
    spots = place or {}
    for i, n in enumerate(names):
        m = BUILDERS[n](fine)
        m.info.pop("about", None)
        ob, arm = rigid.to_blender(m, crease=m.info.pop("crease", 38.0))
        w = {"Izba": 9.0, "Khata": 10.0, "BurntChimney": 9.0, "WellSweep": 8.0, "Haystack": 5.0, "WattleFence": 5.0}.get(n, 5.0)
        at = spots.get(n, (x + w / 2, 0.0, 0.0))
        arm.location = at[:3] if len(at) >= 3 else (*at, 0.0)
        if len(at) > 3:
            arm.rotation_euler = (0.0, 0.0, math.radians(at[3]))
        x += w
    camo = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    bpy.context.scene.collection.objects.link(camo)
    bpy.context.scene.camera = camo
    camo.data.lens = 35
    eye, target = cam or ((x / 2 - 4.0, -64.0, 13.0), (x / 2, 0.0, 3.5))
    camo.location = eye
    d = Vector(target) - Vector(eye)
    camo.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    stage.render_setup(size[0], size[1], samples=spp)
    sc = bpy.context.scene
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("wrote", path)


def main():
    a = sys.argv[1:]
    if a and a[0] == "--picture":
        names = a[2].split(",") if len(a) > 2 else list(BUILDERS)
        ground = a[3] if len(a) > 3 else "steppe"
        picture(a[1], names, ground)
        return
    out = a[0]
    which = a[1] if len(a) > 1 else "all"
    kinds = a[2:] or ["json", "glb"]
    names = list(BUILDERS) if which == "all" else which.split(",")
    os.makedirs(out + "/", exist_ok=True)
    index_path = os.path.join(os.path.dirname(os.path.abspath(out)), "index-east.json")
    index = json.load(open(index_path)) if os.path.exists(index_path) else {}
    for n in names:
        index[n] = one(n, out, kinds)
        with open(index_path, "w") as f:
            json.dump(index, f, indent=1)


if __name__ == "__main__":
    main()
