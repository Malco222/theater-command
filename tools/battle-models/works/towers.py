"""The flak tower and the searchlight.

The tower is cast as the pillbox is (solids cut and chamfered), but it is a building: a square keep with a round
bastion at each corner carrying a heavy gun in an open pit, a gallery for the light guns two thirds of the way up, a
command post and its rangefinder on the roof. Its guns are not in its file: they stand at its sockets (`takes`), so
each can be laid, fired and knocked out as its own model.

Our own plain designs; no army's own pattern.
"""
import math

import bmesh
import bpy
from mathutils import Quaternion, Vector

import bits
import concrete
from bits import X, Y, Z, rnd
from concrete import local
from posts import weather


def fbox(theta, n0, n1, t0, t1, z0, z1):
    """A box in a face's own frame: n0..n1 out along the face's outward way, t0..t1 along the face, z0..z1."""
    return bits.s_move(bits.s_box((min(n0, n1), min(t0, t1), z0), (max(n0, n1), max(t0, t1), z1)), yaw=theta)


def s_many(obs):
    """Several solids that do not touch each other as one cutter (no union needed: one cut instead of dozens)."""
    bm = bmesh.new()
    for ob in obs:
        bm.from_mesh(ob.data)
        me = ob.data
        bpy.data.objects.remove(ob)
        bpy.data.meshes.remove(me)
    me = bpy.data.meshes.new("tmp")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("tmp", me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def ladder(m, foot, top, out, wide=0.42, paint="steel"):
    """A steel ladder up a wall: two flat rails stood off the wall, a rung every 0.3 m."""
    foot, top, out = Vector(foot), Vector(top), Vector(out).normalized()
    side = out.cross(Z).normalized()
    up = (top - foot)
    n = int(up.length / 0.3)
    for s in (-1, 1):
        bits.tube(m, [foot + side * s * wide / 2 + out * 0.16, top + side * s * wide / 2 + out * 0.16, top + side * s * wide / 2 + out * 0.16 + Z * 0.9 - out * 0.3], 0.018, paint, 6)
        for k in range(0, n, 6):
            p = foot + up * (k / n) + side * s * wide / 2
            m.rod(p, p + out * 0.16, 0.012, paint, sides=5)
    if m.fine:
        for k in range(1, n):
            p = foot + up * (k / n) + out * 0.16
            m.rod(p - side * wide / 2, p + side * wide / 2, 0.011, paint, sides=5)


def rail(m, pts, high=1.0, paint="steel", closed=False, every=1.6):
    """A tube handrail on stanchions, with a knee rail."""
    pts = [Vector(p) for p in pts]
    for h in (high, high * 0.5):
        bits.tube(m, [p + Z * h for p in pts], 0.02 if h == high else 0.014, paint, 6, closed=closed)
    seq = pts + ([pts[0]] if closed else [])
    for a, b in zip(seq, seq[1:]):
        n = max(1, int((b - a).length / every))
        for k in range(n + (0 if closed else 0)):
            p = a.lerp(b, k / n)
            m.rod(p, p + Z * high, 0.018, paint, sides=6)
    if not closed:
        m.rod(pts[-1], pts[-1] + Z * high, 0.018, paint, sides=6)


def FlakTower(fine=True, nation="_default", name="FlakTower"):
    m = bits.model(name, "Flak tower (our own design)", "works", nation, fine)
    m.part("tower", None, (0, 0, 0), kind="root")
    HB = 16.0                         # the keep's half width
    BC, BR = 14.5, 6.0                # where each bastion's middle stands, and its radius
    Z0, ZR, ZP, ZB, ZG = -0.6, 26.0, 27.2, 29.4, 18.0       # foot, roof deck, parapet top, bastion top, gallery deck
    PIT = 1.3                         # how deep the gun pits are
    GW = 2.6                          # how far the galleries stand out
    sides = 28 if fine else 12
    lifts = [2.9, 6.6, 10.6, 14.6, 20.6, 23.9]              # where one day's pour ended and the next began
    square = lambda h: [(h, -h), (h, h), (-h, h), (-h, -h)]          # noqa: E731
    corners = [(1, 1), (1, -1), (-1, -1), (-1, 1)]
    faces4 = (0, 90, 180, 270)

    # ---- the keep, its roof deck sunk within a parapet
    body = bits.s_prism(square(HB), Z0, ZP)
    bits.s_cut(body, bits.s_prism(square(HB - 0.7), ZR, ZP + 1.0))
    # ---- galleries: along each face on a splayed corbel, and round each bastion
    for th in faces4:
        g = bits.s_wedge([local(th, HB - 0.3, -BC, ZG - 2.6), local(th, HB - 0.3, BC, ZG - 2.6), local(th, HB + 0.05, BC, ZG - 2.6), local(th, HB + 0.05, -BC, ZG - 2.6)],
                         [local(th, HB - 0.3, -BC, ZG - 0.45), local(th, HB - 0.3, BC, ZG - 0.45), local(th, HB + GW, BC, ZG - 0.45), local(th, HB + GW, -BC, ZG - 0.45)])
        bits.s_join(body, g)
        slab = fbox(th, HB - 0.3, HB + GW, -BC, BC, ZG - 0.5, ZG + 1.1)
        bits.s_cut(slab, fbox(th, HB - 1.0, HB + GW - 0.32, -BC - 1.0, BC + 1.0, ZG, ZG + 2.0))
        bits.s_join(body, slab)
    for sx, sy in corners:
        c = (sx * BC, sy * BC)
        ring = bits.s_lathe((c[0], c[1], 0.0), Z, [(ZG - 3.0, BR - 0.3), (ZG - 0.45, BR + GW), (ZG + 1.1, BR + GW)], sides)
        bits.s_cut(ring, bits.s_cyl((c[0], c[1], ZG), Z, BR + GW - 0.32, 2.0, sides))
        # the bastion itself: a drum from the ground to above the roof, the day-joints turned into it
        prof = [(Z0 - 0.05, BR)]
        if fine:
            for z in lifts:
                prof += [(z, BR), (z + 0.01, BR - 0.07), (z + 0.09, BR - 0.07), (z + 0.1, BR)]
        prof += [(ZB - 0.5, BR), (ZB - 0.3, BR + 0.25), (ZB, BR + 0.25)]
        drum = bits.s_lathe((c[0], c[1], 0.0), Z, prof, sides)
        bits.s_cut(drum, bits.s_cyl((c[0], c[1], ZB - PIT), Z, BR - 0.75, 3.0, sides))
        bits.s_join(drum, ring)
        bits.s_join(body, drum)
    # ---- on the roof: the command post under its slab, and a hoist house by each bastion
    post = bits.s_box((-4.0, -4.0, ZR - 0.2), (4.0, 4.0, ZR + 4.2))
    bits.s_join(post, bits.s_prism(square(4.6), ZR + 4.2, ZR + 4.9, top=square(4.45)))
    bits.s_join(body, post)
    for sx, sy in corners:
        bits.s_join(body, bits.s_box((sx * 8.2 - 1.3, sy * 8.2 - 1.3, ZR - 0.2), (sx * 8.2 + 1.3, sy * 8.2 + 1.3, ZR + 2.7)))
    # ---- the porch over the main door at the rear
    porch = fbox(180, HB - 0.2, HB + 3.0, -3.2, 3.2, Z0, 5.4)
    bits.s_cut(porch, fbox(180, HB - 0.5, HB + 4.0, -2.2, 2.2, -0.05, 4.4))
    bits.s_join(body, porch)

    # ---- everything cut into it at one stroke: windows, doors, the command post's slits, the day-joints
    cuts = []
    windows = []                      # (theta, t, z, wide, high)
    for th in faces4:
        for z in (4.5, 8.5, 12.5, 22.4):
            for t in (-6.2, -3.1, 0.0, 3.1, 6.2):
                if th == 180 and z == 4.5 and abs(t) < 3.5:
                    continue
                if th in (90, 270) and z == 22.4 and t == 0.0:
                    continue
                windows.append((th, t, z, 1.1, 1.6))
    for th, t, z, w, h in windows:
        cuts.append(fbox(th, HB - 0.55, HB + 0.5, t - w / 2, t + w / 2, z - h / 2, z + h / 2))
    cuts.append(fbox(180, HB - 1.3, HB + 0.3, -1.7, 1.7, -0.05, 4.0))                    # the main door's recess
    for th in (90, 270):
        cuts.append(fbox(th, HB - 0.8, HB + 0.5, -5.0, -3.8, -0.05, 2.2))                # a side door
        cuts.append(fbox(th, HB - 0.8, HB + 0.5, -1.2, 1.2, 21.1, 23.7))                 # the loading door, high up
    for th in faces4:
        cuts.append(fbox(th, 3.3, 4.6, -3.3, 3.3, ZR + 2.75, ZR + 3.25))                 # the command post's look-out slits
    for sx, sy in corners:                                                              # the hoist houses' doorways, facing the middle
        th = math.degrees(math.atan2(-sy, -sx))
        o = bits.s_box((0.9, -0.5, ZR + 0.02), (1.6, 0.5, ZR + 2.05))
        cuts.append(bits.s_move(o, at=(sx * 8.2, sy * 8.2, 0.0), yaw=th))
    if fine:
        for th in faces4:
            for z in lifts:
                cuts.append(fbox(th, HB - 0.07, HB + 0.4, -9.4, 9.4, z, z + 0.09))
    bits.s_cut(body, s_many(cuts))
    if fine:
        bits.s_bevel(body, 0.045, 1, 30.0)

    # ---- a length of the front parapet that a heavy hit throws down
    brk = concrete.ragged(bits.s_wedge([local(0, HB - 1.6, -3.4, ZR + 0.25), local(0, HB - 1.6, 2.9, ZR + 0.4), local(0, HB - 1.6, 2.5, ZP + 1.0), local(0, HB - 1.6, -3.0, ZP + 1.0)],
                                       [local(0, HB + 1.0, -3.9, ZR - 0.55), local(0, HB + 1.0, 3.3, ZR - 0.35), local(0, HB + 1.0, 2.9, ZP + 1.0), local(0, HB + 1.0, -3.4, ZP + 1.0)]),
                          0.5, 0.16, 3.0) if fine else None
    dark = lambda mid, n, k: "concrete.in" if k == 1 else "concrete"        # noqa: E731
    if brk is not None:
        piece, body = concrete.split(body, brk)
        m.use("tower")
        concrete.take_slots(m, body, dark, "tower")
        m.part("parapet", "tower", local(0, HB - 0.3, 0.0, ZR), kind="free", note="a length of the front parapet: a heavy hit throws it down the face")
        concrete.take_slots(m, piece, dark, "parapet")
    else:
        bits.take(m, body, "concrete", True, "tower")
    m.use("tower")

    # ---- what is hung on the concrete: shutters, doors, ladders, rails
    g = rnd(11)
    for th, t, z, w, h in windows:
        back = HB - 0.5
        is_open = fine and g.random() < 0.16
        if is_open:
            m.box(local(th, back + 0.02, t, z), (0.02, w, h), "void", yaw=th)
            for s in (-1, 1):                    # leaves swung right back against the wall
                m.box(local(th, HB + 0.035, t + s * (w * 0.5 + w * 0.26), z), (0.03, w * 0.5, h - 0.04), "upper", yaw=th)
        else:
            ajar = g.uniform(0.0, 0.03)
            for s in (-1, 1):
                m.box(local(th, back + 0.2 + ajar * s, t + s * w * 0.25, z), (0.03, w * 0.5 - 0.012, h - 0.03), "upper", yaw=th)
                if fine:
                    for zz in (-0.5, 0.5):
                        m.box(local(th, back + 0.222 + ajar * s, t + s * w * 0.25, z + zz), (0.012, w * 0.5 - 0.05, 0.05), "steel", yaw=th)
            if fine:
                m.box(local(th, back + 0.225, t, z), (0.014, 0.05, 0.3), "steel", yaw=th)
    # the main doors: two leaves of plate, framed and bolted
    for s in (-1, 1):
        mid = local(180, HB - 1.22, s * 0.85, 1.95)
        m.box(mid, (0.06, 1.66, 3.9), "upper", yaw=180)
        if fine:
            for zz in (0.25, 1.95, 3.65):
                m.box(local(180, HB - 1.18, s * 0.85, zz), (0.02, 1.6, 0.09), "steel", yaw=180)
            for k in range(9):
                for tt in (0.1, 1.6):
                    bits.bolt(m, local(180, HB - 1.165, s * tt, 0.2 + k * 0.44), local(180, 1, 0, 0), 0.02, 0.014)
    m.box(local(180, HB - 1.16, 0.0, 1.9), (0.03, 0.09, 3.8), "steel", yaw=180)
    for th in (90, 270):
        hinge = local(th, HB - 0.72, -5.0, 0.0)
        concrete.steel_door(m, hinge, th, 1.2, 2.15)
        # the loading door and its beam
        m.box(local(th, HB - 0.7, 0.0, 22.4), (0.05, 2.4, 2.6), "upper", yaw=th)
        m.box(local(th, HB - 0.67, 0.0, 22.4), (0.02, 0.07, 2.6), "steel", yaw=th)
        a, b = local(th, HB - 0.3, 0.0, 24.25), local(th, HB + 2.4, 0.0, 24.25)
        for dz in (-0.14, 0.14):
            m.box((a + b) / 2 + Z * dz, ((b - a).length, 0.16, 0.02), "steel", yaw=th)
        m.box((a + b) / 2, ((b - a).length, 0.02, 0.28), "steel", yaw=th)
        if fine:
            blk = local(th, HB + 2.0, 0.0, 24.0)
            m.box(blk - Z * 0.1, (0.22, 0.12, 0.25), "steel", yaw=th)
            bits.tube(m, [blk - Z * 0.2, blk - Z * 1.6], 0.008, "steel", 5)
            bits.tube(m, [blk - Z * 1.6, blk - Z * 1.72 + local(th, 0.05, 0, 0), blk - Z * 1.8, blk - Z * 1.74 - local(th, 0.05, 0, 0)], 0.014, "steel", 6)
    for sx, sy in corners:                       # the hoist houses' doors
        th = math.degrees(math.atan2(-sy, -sx))
        d = local(th, 1, 0, 0)
        c = Vector((sx * 8.2, sy * 8.2, 0.0))
        m.box(c + d * 0.95 + Z * (ZR + 1.03), (0.04, 0.98, 2.0), "upper", yaw=th)
    if fine:
        # ladders: gallery to roof on the two flanks, roof to each bastion, roof to the command post's top
        for th in (90, 270):
            ladder(m, local(th, HB - 0.02, 7.6, ZG + 0.02), local(th, HB - 0.02, 7.6, ZP), local(th, 1, 0, 0))
        for sx, sy in corners:
            th = math.degrees(math.atan2(-sy, -sx))
            d = local(th, 1, 0, 0)
            c = Vector((sx * BC, sy * BC, 0.0))
            ladder(m, c + d * (BR + 0.25) + Z * ZR, c + d * (BR + 0.25) + Z * ZB, d)
        ladder(m, local(180, 4.0, 2.6, ZR), local(180, 4.6, 2.6, ZR + 4.9) - local(180, 0.62, 0, 0), local(180, 1, 0, 0))
        rail(m, [(4.35, -4.35, ZR + 4.9), (4.35, 4.35, ZR + 4.9), (-4.35, 4.35, ZR + 4.9), (-4.35, -4.35, ZR + 4.9)], 1.0, "steel", closed=True)
        # down-pipes from the roof at the flanks of each face, and a cable run
        for th in faces4:
            for t in (-8.4, 8.4):
                bits.tube(m, [local(th, HB + 0.09, t, ZG - 2.7), local(th, HB + 0.09, t, 0.1)], 0.06, "steel.worn", 8)
                for z in (2.0, 6.0, 10.0, 14.0):
                    m.box(local(th, HB + 0.07, t, z), (0.1, 0.2, 0.05), "steel", yaw=th)
        # the aerial on the command post
        mast = Vector((-3.6, 3.6, ZR + 4.9))
        m.lathe(mast, Z, [(0.0, 0.07), (4.5, 0.045), (7.5, 0.02)], "steel", 8)
        for a in (20, 140, 260):
            foot = mast + Vector((math.cos(math.radians(a)) * 2.2, math.sin(math.radians(a)) * 2.2, 0.0))
            foot.x, foot.y = max(min(foot.x, 4.3), -4.3), max(min(foot.y, 4.3), -4.3)
            bits.tube(m, [mast + Z * 4.4, foot], 0.006, "steel", 4)
        # hooks for nets round the roof's edge
        hooks, ways = [], []
        for th in faces4:
            for k in range(-4, 5):
                hooks.append(local(th, HB + 0.02, k * 1.9, ZP - 0.25))
                ways.append(local(th, 1, 0, 0))
        concrete.net_hooks(m, hooks, ways)

    # ---- the rangefinder on the command post: a long tube on a turning pedestal, an eyepiece housing, two seats
    top = Vector((0.6, 0.0, ZR + 4.9))
    m.lathe(top, Z, [(0.0, 0.55), (0.12, 0.55), (0.14, 0.32), (1.05, 0.26), (1.1, 0.36), (1.22, 0.36)], "upper", 20 if fine else 10)
    m.part("director", "tower", top + Z * 1.22, axis=(0, 0, -1), kind="turn", note="the rangefinder: turns all round, plus is clockwise seen from above")
    m.use("director")
    dz = top + Z * 1.5
    m.lathe(dz - Y * 3.0, Y, [(0.0, 0.2), (0.5, 0.2), (0.55, 0.13), (5.45, 0.13), (5.5, 0.2), (6.0, 0.2)], "upper", 16 if fine else 8)
    m.box(dz, (0.5, 0.7, 0.42), "upper")
    m.box(dz - Z * 0.2, (0.36, 0.44, 0.2), "upper")
    for s in (-1, 1):
        m.lathe(dz + Y * s * 2.75, X, [(0.19, 0.11), (0.24, 0.11), (0.25, 0.09)], "upper", 12 if fine else 6)
        m.disc(dz + Y * s * 2.75 + X * 0.252, X, 0.085, "glass", 12)
        if fine:
            m.lathe(dz + Y * s * 0.14 - X * 0.25, -X, [(0.0, 0.03), (0.1, 0.03), (0.12, 0.04)], "rubber", 8)
            # a seat on an arm either side of the pedestal
            seat = dz + Vector((-0.85, s * 0.5, -0.75))
            bits.tube(m, [dz - Z * 0.3 + Y * s * 0.2, dz + Vector((-0.5, s * 0.5, -0.9)), seat - Z * 0.03], 0.022, "steel", 6)
            m.lathe(seat - Z * 0.03, Z, [(0.0, 0.17), (0.03, 0.19), (0.05, 0.17)], "leather", 12)
            bits.crew(m, f"rangefinder.{1 if s > 0 else 2}", seat - Z * 0.44, (1, 0, 0), "sit", "director", "sitting at the rangefinder's eyepiece", seat_h=0.44)
    m.use("tower")
    bits.crew(m, "commander", (-2.4, -1.2, ZR + 4.9), (1, 0, 0), "stand", "tower", "standing on the command post, field glasses up")

    # ---- the guns' places, and where the tower shows it has been hit
    takes = {}
    for k, (sx, sy) in enumerate(corners):
        m.socket(f"gun.{k + 1}", (sx * BC, sy * BC, ZB - PIT), (1, 0, 0), part="tower",
                 note="a heavy flak gun stands here in its pit (build it from its own model, HeavyFlak): floor 10.5 m across inside the wall, the wall's top 1.3 m above the floor")
        takes[f"gun.{k + 1}"] = "HeavyFlak"
        out = Vector((sx, sy, 0)).normalized()
        for j, a in enumerate((-38.0, 38.0)):
            d = Quaternion(Z, math.radians(a)) @ out
            n = k * 2 + j + 1
            m.socket(f"light.{n}", (sx * BC + d.x * (BR + 1.2), sy * BC + d.y * (BR + 1.2), ZG), tuple(d), part="tower",
                     note="a light flak gun stands here on the gallery (LightFlak), facing outward")
            takes[f"light.{n}"] = "LightFlak"
    m.info["takes"] = takes
    m.socket("hit", (HB + 0.1, 0.0, ZR + 0.4), (1, 0, 0), part="tower", note="where a heavy bomb or shell strikes: the front parapet")
    for k, (th, t, z) in enumerate(((0, 3.1, 12.5), (90, -3.1, 8.5), (270, 0.0, 12.5), (0, -6.2, 22.4))):
        m.socket(f"smoke.{k + 1}", local(th, HB - 0.2, t, z), tuple(local(th, 1, 0, 0.4)), part="tower", note="smoke pours from this window when she burns inside")
    m.socket("fire", local(0, HB - 0.3, 3.1, 12.3), (1, 0, 0.3), part="tower", note="what burns inside shows at this window")
    m.socket("entrance", local(180, HB + 3.4, 0.0, 0.0), (-1, 0, 0), part="tower", note="men go in and out here")

    # ---- rubble thrown on the gallery below the break, hidden until then
    m.part("rubble", "tower", local(0, HB + 1.2, 0, ZG), kind="free", note="hidden (scale 0) until the parapet is thrown down")
    m.use("rubble")
    concrete.rubble(m, local(0, HB + 1.2, 0.0, ZG + 0.1), 1.1, 26, seed=6, size=(0.15, 0.5))
    concrete.rubble(m, local(0, HB + 4.6, -0.5, 0.25), 2.6, 30, seed=8, size=(0.2, 0.75))
    m.use("tower")
    q1 = Quaternion(Vector((0.1, -1, 0.05)).normalized(), math.radians(150.0))
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing; the rubble hidden.", "tracks": [{"part": "rubble", "scale": [[0.0, 0.0]]}], "events": []}
    m.clips["follow"] = {"long": 16.0, "loop": True, "about": "The rangefinder follows a formation across the sky (for a look only: the game turns `director` itself).",
                         "tracks": [{"part": "director", "turn": [[0.0, -40.0], [8.0, 40.0], [16.0, -40.0]]}], "events": []}
    m.clips["knocked_out"] = {
        "long": 3.4, "loop": False, "after": "hold",
        "about": "A heavy bomb on the front parapet: a length of it is thrown out and falls the tower's whole height, rubble lies on the gallery and at the foot, smoke pours from the windows. "
                 "The tower itself stands (nothing in the field could bring one down); its guns are knocked out one by one as their own models. (At rest play `idle`.)",
        "tracks": [{"part": "parapet", "rot": [[0.0, 1, 0, 0, 0], [0.5, *Quaternion().slerp(q1, 0.2)], [1.4, *Quaternion().slerp(q1, 0.6)], [2.45, *q1]],
                    "move": [[0.0, 0, 0, 0], [0.5, 1.6, 0.1, -0.6], [1.0, 2.9, 0.2, -4.2], [1.6, 4.2, 0.3, -11.5], [2.1, 5.2, 0.3, -19.5], [2.45, 5.8, 0.3, -25.6]],
                    "scale": [[0.0, 1.0], [2.44, 1.0], [2.45, 0.0]]},
                   {"part": "rubble", "scale": [[0.0, 0.0], [0.3, 0.0], [0.6, 1.0]]}],
        "events": [{"t": 0.0, "fx": "shell_burst_big", "socket": "hit", "cue": "explosion_debris"},
                   {"t": 0.1, "fx": "debris", "socket": "hit", "cue": ""},
                   {"t": 0.5, "fx": "smoke_black", "socket": "smoke.1", "cue": ""},
                   {"t": 0.9, "fx": "smoke_black", "socket": "smoke.4", "cue": ""},
                   {"t": 1.2, "fx": "fire", "socket": "fire", "cue": "fire_start", "cue_loop": "fire_loop"},
                   {"t": 2.45, "fx": "dust_big", "socket": "entrance", "cue": "building_collapse"}]}
    weather(m, [(local(0, HB + 0.1, 3.1, 13.6), 2.2, 0.5), (local(0, HB, 0.0, ZR + 0.6), 3.0, 0.35)], splash=0.8)
    m.info["span"] = 60.0
    m.info["high"] = 14.0
    m.info["worn"] = [(0.0, 0.0, 30.0), (-24.0, 0.0, 6.0)]
    m.info["clear"] = [square(HB + 0.2)] + [[(sx * BC + math.cos(math.radians(a)) * (BR + 0.3), sy * BC + math.sin(math.radians(a)) * (BR + 0.3)) for a in range(0, 360, 20)] for sx, sy in corners]
    return m


# ---- the searchlight -------------------------------------------------------------------------------------------

def Searchlight(fine=True, nation="_default", name="Searchlight"):
    """A 150 cm anti-aircraft searchlight on a four-armed base: the drum turns all round on its fork and tips from
    level to straight up; a long arm lets a man stand clear of the glare and swing it."""
    m = bits.model(name, "Anti-aircraft searchlight (our own design)", "works", nation, fine)
    m.part("light", None, (0, 0, 0), kind="root")
    S = 28 if fine else 12
    # ---- the base: four arms on screw jacks, a drum in the middle
    for a in (45, 135, 225, 315):
        d = Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0.0))
        side = Vector((-d.y, d.x, 0.0))
        tip = d * 1.35
        m.add([d * 0.3 + side * 0.11 + Z * 0.22, d * 0.3 - side * 0.11 + Z * 0.22, tip - side * 0.07 + Z * 0.2, tip + side * 0.07 + Z * 0.2,
               d * 0.3 + side * 0.11 + Z * 0.42, d * 0.3 - side * 0.11 + Z * 0.42, tip - side * 0.07 + Z * 0.3, tip + side * 0.07 + Z * 0.3],
              [(0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)], "upper", False, None)
        m.lathe(tip, Z, [(0.0, 0.14), (0.025, 0.14), (0.03, 0.035), (0.34, 0.035), (0.35, 0.02), (0.42, 0.02)], "steel", 12 if fine else 6)
        if fine:
            m.rod(tip + Z * 0.4 - side * 0.11, tip + Z * 0.4 + side * 0.11, 0.012, "steel", sides=6)
    m.lathe((0, 0, 0.18), Z, [(0.0, 0.42), (0.26, 0.42), (0.28, 0.5), (0.36, 0.5), (0.38, 0.4)], "upper", S)
    if fine:
        bits.bolt_ring(m, (0, 0, 0.54), Z, 0.45, 12, 0.014, 0.01)
    P = Vector((0, 0, 0.56))
    m.part("mount", "light", P, axis=(0, 0, -1), kind="turn", note="traverse, all round: plus is clockwise seen from above")
    m.use("mount")
    m.lathe(P, Z, [(0.0, 0.4), (0.05, 0.4), (0.07, 0.3), (0.2, 0.26)], "upper", S)
    T = P + Z * 1.22                 # the drum's trunnions
    R, L = 0.86, 0.62                # the drum's radius and half its length
    # the fork: two arms rising either side of the drum
    for s in (-1, 1):
        arm = [P + Vector((0, s * 0.28, 0.16)), P + Vector((0, s * (R + 0.1), 0.34)), T + Y * s * (R + 0.1) - Z * 0.1, T + Y * s * (R + 0.1) + Z * 0.08]
        bits.tube(m, arm, 0.055, "upper", 10 if fine else 6, radii=[0.07, 0.065, 0.055, 0.05])
        m.lathe(T + Y * s * (R + 0.02), Y * s, [(0.0, 0.1), (0.14, 0.1), (0.15, 0.07), (0.19, 0.07)], "steel", 14 if fine else 8)
    bits.tube(m, [P + Vector((0, -R - 0.1, 0.34)), P + Vector((-0.25, 0, 0.26)), P + Vector((0, R + 0.1, 0.34))], 0.03, "upper", 6)
    # the control arm: out to the left and back, a hand bar at its end
    arm_root = T + Y * (R + 0.2)
    hand = arm_root + Vector((-0.5, 2.6, -0.25))
    bits.tube(m, [arm_root, arm_root + Y * 0.3, hand], 0.024, "steel", 8)
    bits.tube(m, [hand - X * 0.22, hand + X * 0.22], 0.016, "steel", 6)
    bits.crew(m, "operator", hand + Vector((0.0, 0.45, -(hand.z))), (0, -1, 0), "stand", "mount", "standing clear of the glare at the end of the control arm, swinging the light by it",
              grips={"l": (hand + X * 0.18, (0, -1, 0)), "r": (hand - X * 0.18, (0, -1, 0))})
    # the elevation handwheel on the right arm
    bits.handwheel(m, T - Y * (R + 0.24) - Z * 0.35, -Y, 0.16, "steel", 4)
    m.part("drum", "mount", T, axis=(0, -1, 0), limits=[-5.0, 90.0], kind="turn", note="elevation: plus tips the glass upward, to straight up")
    m.use("drum")
    # the drum: a short barrel, a dished back, a rolled rim round the glass
    m.lathe(T - X * L, X, [(0.0, 0.25), (0.03, 0.42), (0.1, 0.62), (0.22, 0.78), (0.36, R), (2 * L - 0.08, R), (2 * L - 0.06, R + 0.03), (2 * L, R + 0.03), (2 * L, R - 0.03)], "upper", S, (True, False))
    m.disc(T + X * (L - 0.02), X, R - 0.03, "glass", S)
    if fine:
        # the glass is in strips held by bars; a guard of thin rods before it
        for k in range(-3, 4):
            y = k * 0.22
            h = math.sqrt(max((R - 0.03) ** 2 - y * y, 0.0))
            m.box(T + X * (L - 0.012) + Y * y, (0.012, 0.014, 2 * h), "steel")
        for a in range(0, 180, 30):
            d = Y * math.cos(math.radians(a)) + Z * math.sin(math.radians(a))
            bits.tube(m, [T + X * L + d * (R - 0.03), T + X * (L + 0.05), T + X * L - d * (R - 0.03)], 0.005, "steel", 4)
        # bands round the barrel, with the screws that hold them
        for x in (-0.22, 0.2):
            m.lathe(T + X * x, X, [(0.0, R + 0.004), (0.05, R + 0.004)], "upper2", S, (False, False))
            m.lathe(T + X * x, X, [(0.0, R + 0.012), (0.012, R + 0.012)], "steel", S, (True, True))
        bits.bolt_ring(m, T - X * (L - 0.2), -X, 0.55, 10, 0.012, 0.008)
    # the chimney for the arc's heat on top, with its cowl; the lamp's door on the side; carrying handles
    m.lathe(T + Z * (R - 0.02) - X * 0.05, Z, [(0.0, 0.12), (0.2, 0.12), (0.21, 0.19), (0.23, 0.19), (0.31, 0.05)], "upper", 14 if fine else 8)
    m.box(T + Y * (R - 0.0) * 0.0 - Z * (R + 0.005) - X * 0.05, (0.34, 0.3, 0.03), "upper")
    for s in (-1, 1):
        if fine:
            p = T + Vector((-0.28, s * R * 0.72, R * 0.72))
            bits.tube(m, [p, p + Vector((0, s * 0.06, 0.06)), p + Vector((0.3, s * 0.06, 0.06)), p + X * 0.3], 0.011, "steel", 5)
    # the arc's feed housing at the back, its cable
    m.lathe(T - X * L, -X, [(0.0, 0.2), (0.2, 0.18), (0.22, 0.1), (0.3, 0.1)], "upper", 14 if fine else 8)
    if fine:
        cable = [T - X * (L + 0.3), T - X * (L + 0.5) - Z * 0.4, P + Vector((-0.6, 0.0, -0.1)), Vector((-1.2, 0.2, 0.05)), Vector((-2.2, 0.5, 0.03)), Vector((-3.4, 0.2, 0.03))]
        m.use("light")
        bits.tube(m, cable[2:], 0.02, "rubber", 6)
        m.use("drum")
    m.socket("beam", T + X * (L + 0.02), (1, 0, 0), part="drum", note="the beam leaves here, along the drum's own forward way")
    m.part("glass.broken", "drum", T + X * L, kind="free", note="shards in the rim, hidden (scale 0) until she is knocked out")
    m.use("glass.broken")
    g = rnd(5)
    for k in range(9 if fine else 4):
        a = g.uniform(0, 6.283)
        d = Y * math.cos(a) + Z * math.sin(a)
        s2 = Y * -math.sin(a) + Z * math.cos(a)
        r0 = R - 0.04
        m.add([T + X * (L - 0.015) + d * r0 + s2 * 0.16, T + X * (L - 0.015) + d * r0 - s2 * 0.16, T + X * (L - 0.015) + d * g.uniform(0.25, 0.6) + s2 * g.uniform(-0.1, 0.1)], [(0, 1, 2)], "glass", False, lambda p: X)
    m.disc(T + X * (L - 0.03), X, R - 0.04, "void", S)
    m.use("light")
    m.socket("hit", T, (1, 0, 0), part="light")
    m.socket("cable", (-3.4, 0.2, 0.03), (-1, 0, 0), part="light", note="the cable runs from here to the generator")
    bits.patch(m, 0.0, 0.0, 2.3, "earth.dry", seed=4)
    qk = Quaternion(Vector((0.2, 1, 0)).normalized(), math.radians(-14.0))
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing, the glass whole.", "tracks": [{"part": "glass.broken", "scale": [[0.0, 0.0]]}], "events": []}
    m.clips["sweep"] = {"long": 14.0, "loop": True, "about": "Searching the sky: slow swings across, the beam high (for a look: the game lays `mount` and `drum` itself).",
                        "tracks": [{"part": "mount", "turn": [[0.0, -50.0], [7.0, 50.0], [14.0, -50.0]]}, {"part": "drum", "turn": [[0.0, 52.0], [3.5, 66.0], [7.0, 50.0], [10.5, 68.0], [14.0, 52.0]]},
                                   {"part": "glass.broken", "scale": [[0.0, 0.0]]}], "events": []}
    m.clips["knocked_out"] = {"long": 1.6, "loop": False, "after": "hold", "about": "A hit: the glass goes, the drum drops on its trunnions and the light is out.",
                              "tracks": [{"part": "glass.broken", "scale": [[0.0, 0.0], [0.04, 1.0]]},
                                         {"part": "drum", "rot": [[0.0, 1, 0, 0, 0], [0.5, *Quaternion().slerp(qk, 0.8)], [0.8, *qk], [1.0, *Quaternion().slerp(qk, 0.92)], [1.2, *qk]]}],
                              "events": [{"t": 0.0, "fx": "glass_burst", "socket": "beam", "cue": ""}, {"t": 0.0, "fx": "shell_burst", "socket": "hit", "cue": "shell_impact_near"},
                                         {"t": 0.3, "fx": "smoke_thin", "socket": "beam", "cue": ""}]}
    m.info["file"] = {"controls": {"traverse": "mount", "elevate": "drum", "beam": "beam", "glass_paint": "glass",
                                   "note": "Lit, the glass glows and the beam is the game's own effect from `beam`; knocked out, hide the paint `glass` on part `drum` (the dark disc behind shows) "
                                           "and show `glass.broken`."}}
    weather(m, [], splash=0.3, rust=0.4)
    m.info["span"] = 4.6
    m.info["high"] = 1.3
    m.info["worn"] = [(0.0, 0.0, 3.0)]
    return m


BUILDERS = {"FlakTower": FlakTower, "Searchlight": Searchlight}
