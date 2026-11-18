"""Sandbag works and the weapons that stand in them: a length of sandbag wall, a machine gun on its tripod, a
machine-gun post, a mortar and its pit.

Every shape is our own: a plain medium machine gun (air-cooled, or water-cooled where an army kept the water jacket)
and a plain medium mortar, not a copy of any one army's pattern.
"""
import math

from mathutils import Quaternion, Vector

import bits
from bits import X, Y, Z, rnd
from rigid import smooth


def weather(m, soot_at=(), splash=0.4, rust=0.5):
    """Dirt splashed up from the ground, rust in the low and level places, powder soot where a gun fires."""
    def f(p, n, paint, part):
        n = Vector((float(n[0]), float(n[1]), float(n[2])))
        key = paint.split(".")[0]
        w = s = 0.0
        low = smooth(1.0 - p.z / splash)
        if key in ("sandbag", "concrete", "timber", "planks", "canvas", "net"):
            w = 0.5 * low * (0.55 + 0.45 * bits.fbm(p, 0.4))
            if key == "concrete":
                w += 0.16 * max(0.0, bits.fbm(p * 1.0, 1.4)) + 0.10 * max(0.0, -n.z)
        elif key in ("upper", "upper2", "steel", "iron", "wood"):
            w = rust * max(0.0, bits.fbm(p, 0.22) - 0.12) * (0.55 + 0.45 * max(n.z, 0.0)) + 0.35 * low
        for c, r, k in soot_at:
            d = (p - Vector(c)).length
            s = max(s, k * smooth(1.0 - d / r))
        return w, s
    m.weather = f


# ---- the machine gun ----------------------------------------------------------------------------------

def machine_gun(m, at, parent, cooling="air", prefix=""):
    """A medium machine gun on a low tripod, its muzzle toward +x. Parts: tripod (fixed), cradle (traverse), gun
    (elevation), bolt (the cocking handle). Returns the part names and the trunnion's place."""
    at = Vector(at)
    P = at + Vector((0, 0, 0.40))                 # the top of the tripod's head
    T = P + Vector((0, 0, 0.105))                 # the trunnion: the gun lifts about here; the bore lies through it
    n = prefix
    fine = m.fine
    # -- the tripod
    m.part(n + "tripod", parent, at, kind="fixed")
    m.use(n + "tripod")
    m.lathe(P - Z * 0.13, Z, [(0.0, 0.05), (0.02, 0.058), (0.09, 0.058), (0.105, 0.046), (0.13, 0.046)], "upper", 14)
    feet = [Vector((0.62, 0.44, 0.0)), Vector((0.62, -0.44, 0.0)), Vector((-0.86, 0.0, 0.0))]
    for k, f in enumerate(feet):
        top = P - Z * 0.07 + Vector((f.x, f.y, 0)).normalized() * 0.05
        foot = at + f + Z * 0.03
        m.rod(top, foot, 0.021, "upper", 0.019, sides=10)
        d = (foot - top).normalized()
        m.rod(top + d * 0.10, top + d * 0.16, 0.027, "steel", sides=10)                       # the leg clamp
        if fine:
            bits.bolt(m, top + d * 0.13 + Z * 0.03, Z, 0.012, 0.02, "steel")
        # the shoe: a spade that bites
        out = Vector((f.x, f.y, 0)).normalized()
        side = Vector((-out.y, out.x, 0))
        m.add([foot - side * 0.05 - Z * 0.03, foot + side * 0.05 - Z * 0.03, foot + side * 0.04 + out * 0.07 - Z * 0.03, foot - side * 0.04 + out * 0.07 - Z * 0.03,
               foot - side * 0.05 - Z * 0.02, foot + side * 0.05 - Z * 0.02, foot + side * 0.04 + out * 0.07 - Z * 0.02, foot - side * 0.04 + out * 0.07 - Z * 0.02],
              [(3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], "steel", False, None)
    # the brace between the front legs, and the bar the elevating gear rides on down the rear leg
    a = P - Z * 0.07 + (feet[0] + Z * 0.0 - Vector((0, 0, 0))).normalized() * 0.38 + Z * (-0.19)
    m.rod(at + Vector((0.335, 0.238, 0.19)), at + Vector((0.335, -0.238, 0.19)), 0.011, "steel", sides=6)
    m.rod(P + Vector((-0.06, 0, -0.06)), at + Vector((-0.50, 0, 0.215)), 0.013, "steel", sides=6)

    # -- the cradle: turns on the head
    m.part(n + "cradle", n + "tripod", P, axis=(0, 0, -1), limits=[-40.0, 40.0], kind="turn",
           note="traverse: plus swings the muzzle to the gunner's right")
    m.use(n + "cradle")
    m.lathe(P, Z, [(0.0, 0.05), (0.03, 0.05), (0.04, 0.035)], "steel", 14)
    for sy in (1, -1):
        bits.plate(m, [(-0.05, 0.0), (0.05, 0.0), (0.035, 0.125), (-0.035, 0.125)], 0.012, "upper", P + Vector((0, sy * 0.058, 0.03)), X, Z)
        bits.bolt(m, T + Vector((0, sy * 0.066, 0)), Y * sy, 0.016, 0.012, "steel", 8)
    m.box(P + Vector((0, 0, 0.036)), (0.11, 0.12, 0.014), "upper")
    # the traversing arc behind, with its clamp, and the elevating screw and wheel
    arc_pts = [P + Vector((-0.30 * math.cos(math.radians(a)), 0.30 * math.sin(math.radians(a)), -0.035)) for a in range(-42, 43, 7)]
    bits.tube(m, arc_pts, 0.009, "steel", 6)
    m.rod(P + Vector((-0.04, 0, -0.03)), P + Vector((-0.30, 0, -0.035)), 0.012, "upper", sides=8)
    m.rod(P + Vector((-0.30, 0, -0.05)), T + Vector((-0.30, 0, -0.065)), 0.011, "steel", sides=8)      # the elevating screw
    bits.handwheel(m, P + Vector((-0.30, 0, 0.012)), Z, 0.05, "steel", 3, 0.007, False)

    # -- the gun: lifts on the trunnion
    m.part(n + "gun", n + "cradle", T, axis=(0, -1, 0), limits=[-10.0, 18.0], kind="turn", note="elevation: plus lifts the muzzle")
    m.use(n + "gun")
    m.box(T + Vector((-0.17, 0, 0.0)), (0.37, 0.072, 0.112), "steel")                         # the receiver
    m.box(T + Vector((-0.15, 0, 0.061)), (0.30, 0.078, 0.014), "steel")                       # the top cover
    m.box(T + Vector((-0.29, 0, 0.072)), (0.03, 0.03, 0.012), "steel")                        # its latch
    m.box(T + Vector((-0.06, 0, -0.07)), (0.07, 0.05, 0.03), "steel")                         # the lug the cradle holds
    m.box(T + Vector((-0.30, 0, -0.062)), (0.05, 0.04, 0.02), "steel")                        # where the elevating screw takes hold
    sides = 20 if fine else 10
    if cooling == "water":
        r = 0.05
        m.lathe(T + X * 0.01, X, [(0.0, 0.04), (0.012, r), (0.60, r), (0.612, r + 0.004), (0.63, r + 0.004), (0.64, 0.03)], "upper", sides)
        m.lathe(T + Vector((0.50, 0, r)), Z, [(0.0, 0.014), (0.02, 0.014), (0.025, 0.018), (0.035, 0.018)], "brass", 8)     # the filler cap
        m.lathe(T + Vector((0.58, 0, -r)), -Z, [(0.0, 0.01), (0.03, 0.01)], "brass", 8)                                      # the steam outlet
        muz = 0.64
    else:
        r = 0.031
        m.lathe(T + X * 0.01, X, [(0.0, 0.036), (0.03, 0.036), (0.04, r), (0.57, r), (0.58, 0.035), (0.60, 0.035)], "steel", sides)
        if fine:        # the jacket's cooling holes: rows of dark slots
            for row in range(6):
                a = math.radians(row * 60 + 30)
                for k in range(9):
                    c = T + Vector((0.085 + k * 0.053 + (0.026 if row % 2 else 0.0), math.cos(a) * (r + 0.0012), math.sin(a) * (r + 0.0012)))
                    m.disc(c, Vector((0, math.cos(a), math.sin(a))), 0.0105, "void", 8, oval=1.7)
        muz = 0.60
    # the muzzle: a flared cone, the bore dark in it
    m.lathe(T + X * muz, X, [(0.0, 0.02), (0.02, 0.02), (0.085, 0.033), (0.09, 0.033)], "steel", sides, (False, False))
    m.lathe(T + X * (muz + 0.09), -X, [(0.0, 0.03), (0.07, 0.008)], "void", 12, (False, True))
    # sights
    m.box(T + Vector((muz - 0.03, 0, r + 0.022)), (0.012, 0.006, 0.044), "steel")
    m.box(T + Vector((-0.26, 0, 0.098)), (0.012, 0.045, 0.06), "steel")
    m.box(T + Vector((-0.235, 0, 0.073)), (0.07, 0.03, 0.01), "steel")
    # the feed: a tray on the left, an opening on the right the empties fall from
    m.box(T + Vector((-0.085, 0.062, 0.022)), (0.085, 0.055, 0.02), "steel")
    m.box(T + Vector((-0.085, -0.0365, 0.005)), (0.07, 0.002, 0.035), "void")
    # the spade grips and the trigger between them
    back = T + Vector((-0.355, 0, 0))
    m.box(back + Vector((-0.02, 0, 0)), (0.03, 0.14, 0.03), "steel")
    for sy in (1, -1):
        m.rod(back + Vector((-0.045, sy * 0.055, -0.058)), back + Vector((-0.045, sy * 0.055, 0.058)), 0.016, "timber.dark", sides=10)
        m.rod(back + Vector((-0.02, sy * 0.055, 0.058)), back + Vector((-0.047, sy * 0.055, 0.058)), 0.008, "steel", sides=6)
        m.rod(back + Vector((-0.02, sy * 0.055, -0.058)), back + Vector((-0.047, sy * 0.055, -0.058)), 0.008, "steel", sides=6)
        m.socket("grip.gunner." + ("l" if sy > 0 else "r"), back + Vector((-0.045, sy * 0.055, 0.0)), (1, 0, 0), note="the gunner's " + ("left" if sy > 0 else "right") + " hand")
    m.box(back + Vector((-0.04, 0, 0.0)), (0.012, 0.04, 0.03), "steel")
    m.socket(n + "muzzle", T + X * (muz + 0.10), (1, 0, 0))
    m.socket(n + "eject", T + Vector((-0.085, -0.045, -0.01)), (0.2, -1, -0.3), note="where the empties fall")
    m.socket(n + "feed", T + Vector((-0.085, 0.10, 0.03)), (0, 1, 0), note="where the belt goes in")
    # -- the cocking handle: goes back and forth as she fires
    m.part(n + "bolt", n + "gun", T + Vector((-0.10, -0.04, 0.012)), slide=(-1, 0, 0), slide_limits=[0.0, 0.09], kind="slide")
    m.use(n + "bolt")
    m.rod(T + Vector((-0.10, -0.036, 0.012)), T + Vector((-0.10, -0.075, 0.012)), 0.006, "steel", sides=6)
    m.ball(T + Vector((-0.10, -0.082, 0.012)), 0.012, "steel", 8, 6)
    return {"tripod": n + "tripod", "cradle": n + "cradle", "gun": n + "gun", "bolt": n + "bolt", "trunnion": T, "muzzle": muz}


def ammo_box(m, at, yaw=0.0, open_lid=True, belt_to=None, seed=0):
    """A steel belt box; with `belt_to` a belt of rounds runs from it up to that place (the gun's feed)."""
    at = Vector(at)
    q = Quaternion(Z, math.radians(yaw))
    L, W, H = 0.30, 0.105, 0.19
    c = at + Z * (H / 2)
    m.box(c, (L, W, H), "upper", yaw=yaw)
    m.box(c + Z * (H / 2 - 0.006), (L - 0.02, W - 0.02, 0.004), "void", yaw=yaw)
    if open_lid:
        hinge = c + q @ Vector((0, -W / 2, H / 2))
        m.box(hinge + q @ Vector((0, -0.012, W / 2 * 0.98)), (L, 0.008, W), "upper", yaw=yaw, roll=-8)
    else:
        m.box(c + Z * (H / 2 + 0.004), (L + 0.006, W + 0.006, 0.01), "upper", yaw=yaw)
    for sx in (-1, 1):   # the carrying handle's lugs, and the handle folded on the end
        bits.tube(m, [c + q @ Vector((sx * (L / 2 + 0.002), -0.03, 0.03)), c + q @ Vector((sx * (L / 2 + 0.02), -0.03, 0.0)), c + q @ Vector((sx * (L / 2 + 0.02), 0.03, 0.0)),
                      c + q @ Vector((sx * (L / 2 + 0.002), 0.03, 0.03))], 0.004, "steel", 5)
    if belt_to is not None and m.fine:
        a = c + Z * (H / 2 - 0.01)
        b = Vector(belt_to)
        n = max(6, int((b - a).length / 0.0155))
        pts = []
        for k in range(n + 1):
            t = k / n
            # the belt climbs out of the box and hangs in a shallow curve to the feed
            p = a.lerp(b, t) + Z * (-0.05 * math.sin(math.pi * t) + 0.03 * math.sin(math.pi * min(1.0, t * 3.0)) * (1 - t))
            pts.append(p)
        for k, p in enumerate(pts):
            m.lathe(p - X * 0.03, X, [(0.0, 0.0062), (0.044, 0.0062), (0.052, 0.0042), (0.072, 0.0018)], "brass", 5, (True, False))
        side = X * 0.012
        m.add([p + s for p in pts for s in (-side + X * 0.0, side)], [(2 * k, 2 * k + 1, 2 * k + 3, 2 * k + 2) for k in range(len(pts) - 1)], "canvas", True, None)


def spent(m, at, r, count, seed=0, size=0.05, paint="brass", radius=0.006):
    """Empties lying where they fell."""
    if not m.fine:
        return
    g = rnd(seed)
    at = Vector(at)
    for _ in range(count):
        a, d = g.uniform(0, 6.283), r * g.random() ** 0.6
        p = at + Vector((math.cos(a) * d, math.sin(a) * d, radius + 0.002))
        ang = g.uniform(0, 6.283)
        v = Vector((math.cos(ang), math.sin(ang), 0)) * size
        m.lathe(p, v, [(0.0, radius), (size * 0.8, radius), (size, radius * 0.7)], paint, 5, (True, True))


def mg_fire_clip(names, rounds=6, rate=10.0):
    """A burst: the gun shudders on its cradle, the cocking handle runs back and forth, a flash for every round."""
    long = rounds / rate
    turn, slide, events = [[0.0, 0.0]], [[0.0, 0.0]], []
    for k in range(rounds):
        t = k / rate
        turn += [[t + 0.015, 0.35], [t + 0.05, -0.12], [t + 0.085, 0.05]]
        slide += [[t + 0.02, 0.085], [t + 0.07, 0.0]]
        events.append({"t": round(t, 3), "fx": "muzzle_flash_small", "socket": "muzzle", "cue": "lmg_burst" if k == 0 else ""})
        events.append({"t": round(t + 0.03, 3), "fx": "case_small", "socket": "eject", "cue": ""})
    turn.append([long, 0.0])
    slide.append([long, 0.0])
    return {"long": long, "loop": True, "about": f"A burst of {rounds} rounds, looping while she fires (the game trains the cradle and gun itself; this lies over that).",
            "tracks": [{"part": names["gun"], "turn": turn}, {"part": names["bolt"], "slide": slide}], "events": events}


# ---- the models -----------------------------------------------------------------------------------------

def MachineGun(fine=True, cooling="air", nation="_default", name="MachineGun"):
    m = bits.model(name, "Machine gun on its tripod (our own model" + (", water-cooled)" if cooling == "water" else ")"), "works", nation, fine)
    m.part("mg", None, (0, 0, 0), kind="root")
    names = machine_gun(m, (0, 0, 0), "mg", cooling)
    m.use("mg")
    bits.crew(m, "gunner", (-1.05, 0.0, 0.0), (1, 0, 0), "sit", "mg", "sitting on the ground behind the gun, knees up, hands at grip.gunner.l and grip.gunner.r", seat_h=0.0)
    bits.crew(m, "loader", (-0.45, 0.62, 0.0), (0.6, -0.8, 0), "kneel", "mg", "kneeling at the gunner's left, feeding the belt at `feed`")
    m.clips["fire"] = mg_fire_clip(names)
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Laid and still.", "tracks": [], "events": []}
    weather(m, [(names["trunnion"] + X * (names["muzzle"] + 0.05), 0.12, 0.8)], splash=0.12, rust=0.25)
    m.info["span"] = 2.0
    m.info["worn"] = [(-0.3, 0.0, 1.4)]
    return m


def horseshoe(r, a0, a1, cx=0.0, cy=0.0):
    return bits.arc(cx, cy, r, a0, a1)


def MGNest(fine=True, cooling="air", nation="_default", name="MGNest"):
    """A machine-gun post: a horseshoe of sandbags two bags thick in front with a notch to fire through, earth
    banked against it outside, the gun and its boxes inside."""
    m = bits.model(name, "Machine-gun post (our own model)", "works", nation, fine)
    m.part("post", None, (0, 0, 0), kind="root")
    m.use("post")
    R = 1.55                                               # the wall's middle line
    line = horseshoe(R, -118, 118, -0.35, 0.0)

    def top(f):
        a = -118 + 236 * f                                  # (the notch: four courses for 20 degrees either side of the front)
        return 4 if abs(a) < 19 else (5 if abs(a) < 25 else 7 if abs(a) > 70 else 6)
    bits.bag_wall(m, line, 7, 0.0, seed=11, thick=2, top=top, inward=1.0)
    # earth thrown up against the front and sides, falling away outward
    bank_line = horseshoe(R + 0.18, -112, 112, -0.35, 0.0)
    bits.bank(m, bank_line, [(-1.25, 0.0), (-0.95, 0.07), (-0.55, 0.19), (-0.2, 0.30), (0.0, 0.33), (0.05, 0.0)], "earth", seed=3, lump=0.06)
    bits.patch(m, -0.5, 0.0, 3.4, "earth.dry", seed=5, oval=1.05)
    # a strip of sacking and two logs over the notch's sill, where the gun's blast would tear the bags
    bits.log(m, (-0.35 + R + 0.24, -0.50, 0.50), (-0.35 + R + 0.24, 0.50, 0.50), 0.045, seed=4)
    bits.log(m, (-0.35 + R - 0.02, -0.52, 0.50), (-0.35 + R - 0.02, 0.52, 0.50), 0.045, seed=5)
    names = machine_gun(m, (0.52, 0.0, 0.0), "post", cooling)
    m.use("post")
    feed = names["trunnion"] + Vector((-0.085, 0.10, 0.03))
    ammo_box(m, (0.42, 0.46, 0.0), yaw=8, belt_to=feed)
    ammo_box(m, (-0.05, 0.86, 0.0), yaw=-20, open_lid=False)
    ammo_box(m, (0.02, 0.98, 0.19), yaw=-14, open_lid=False)
    ammo_box(m, (-0.42, 0.92, 0.0), yaw=30, open_lid=False)
    spent(m, (0.42, -0.42, 0.0), 0.28, 46, seed=2, size=0.052)
    # a groundsheet, the spare barrel in its case, an entrenching spade stuck in the bank
    m.add([Vector((-1.25, -0.55, 0.012)), Vector((0.1, -0.62, 0.012)), Vector((0.16, 0.36, 0.014)), Vector((-1.2, 0.42, 0.012))], [(0, 1, 2, 3)], "canvas", True, lambda p: Z)
    m.rod((-0.3, -0.98, 0.05), (0.42, -0.86, 0.05), 0.035, "canvas", sides=10)
    m.rod((0.42, -0.86, 0.05), (0.47, -0.853, 0.05), 0.04, "leather", sides=10)
    sp = Vector((-1.35, 1.72, 0.0))
    m.rod(sp + Vector((0, 0, 0.1)), sp + Vector((-0.12, 0.05, 0.78)), 0.016, "timber", sides=8)
    bits.plate(m, [(-0.08, 0.0), (0.08, 0.0), (0.085, 0.16), (0.0, 0.21), (-0.085, 0.16)], 0.004, "steel", sp + Vector((0.0, 0.0, 0.13)), Vector((0.35, 1, 0)), Vector((0.15, -0.05, -1)))
    bits.crew(m, "gunner", (-0.55, 0.0, 0.0), (1, 0, 0), "sit", "post", "sitting on the ground behind the gun, knees up, hands at grip.gunner.l and grip.gunner.r", seat_h=0.0)
    bits.crew(m, "loader", (0.1, 0.55, 0.0), (0.6, -0.8, 0), "kneel", "post", "kneeling at the gunner's left, feeding the belt at `feed`")
    bits.crew(m, "commander", (-0.6, -0.75, 0.0), (1, 0, 0), "kneel", "post", "kneeling, looking over the wall with field glasses")
    m.socket("hit", (1.3, 0.0, 0.5), (1, 0, 0), part="post", note="where a shell strikes the front")
    m.clips["fire"] = mg_fire_clip(names)
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Laid and still.", "tracks": [], "events": []}
    weather(m, [(names["trunnion"] + X * (names["muzzle"] + 0.05), 0.5, 0.75)])
    m.info["span"] = 4.6
    m.info["worn"] = [(-0.5, 0.0, 3.6)]
    m.info["clear"] = [[(p[0] * 1.25 + 0.1, p[1] * 1.25) for p in horseshoe(R + 0.5, -180, 180, -0.35, 0.0)]]
    return m


def SandbagWall(fine=True, long=3.2, courses=8, nation="_default", name="SandbagWall"):
    """A straight length of sandbag wall, two bags thick, chest high, to build breastworks from."""
    m = bits.model(name, "Sandbag wall, a length (our own model)", "works", nation, fine)
    m.part("wall", None, (0, 0, 0), kind="root")
    m.use("wall")
    line = [(0.0, -long / 2), (0.0, long / 2)]
    bits.bag_wall(m, line, courses, 0.0, seed=21, thick=2, inward=1.0)
    bits.patch(m, -0.2, 0.0, long * 0.62, "earth.dry", seed=8, oval=0.45, yaw=0)
    m.socket("hit", (0.2, 0.0, 0.6), (1, 0, 0), note="where a shell strikes")
    bits.crew(m, "1", (-0.75, -0.7, 0.0), (1, 0, 0), "kneel", "wall", "kneeling behind the wall, firing over it")
    bits.crew(m, "2", (-0.75, 0.7, 0.0), (1, 0, 0), "kneel", "wall", "kneeling behind the wall, firing over it")
    weather(m)
    m.info["span"] = long + 0.6
    m.info["worn"] = [(-0.4, 0.0, long * 0.75)]
    return m


BUILDERS = {
    "MachineGun": MachineGun,
    "MachineGun_water": lambda fine=True: MachineGun(fine, "water", name="MachineGun_water"),
    "MGNest": MGNest,
    "MGNest_water": lambda fine=True: MGNest(fine, "water", name="MGNest_water"),
    "SandbagWall": SandbagWall,
}
