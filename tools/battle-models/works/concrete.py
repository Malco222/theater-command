"""Works cast in concrete: the pillbox. (The bunker, the fort, the flak tower and the coast battery follow the same way.)

A work is cast as it would be poured: a solid block, its room hollowed out, its embrasures cut through in steps, its
edges chamfered (bits.cast). What can be knocked down is split off beforehand along a ragged break (`split`), so the
pieces fit without a seam until a clip throws them apart.

Our own plain shapes; no army's own pattern.
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

import bits
from bits import X, Y, Z, rnd
from posts import weather
from rigid import smooth


def hexagon(apothem, turn=0.0):
    """A six-sided plan with a flat face toward +x."""
    r = apothem / math.cos(math.radians(30))
    return [(math.cos(math.radians(30 + 60 * k + turn)) * r, math.sin(math.radians(30 + 60 * k + turn)) * r) for k in range(6)]


def local(theta, n, t, z):
    """A place given by how far out along a face's own outward way (n), along the face (t) and up."""
    c, s = math.cos(math.radians(theta)), math.sin(math.radians(theta))
    return Vector((c * n - s * t, s * n + c * t, z))


def face_box(theta, n0, n1, half_wide, z0, z1):
    """A box lying in a face's own frame: from n0 to n1 out along its outward way, `half_wide` either side, z0 to z1."""
    ob = bits.s_box((min(n0, n1), -half_wide, z0), (max(n0, n1), half_wide, z1))
    return bits.s_move(ob, yaw=theta)


def embrasure(theta, out, z, steps):
    """The cutter for a stepped embrasure in the face looking toward `theta`: `out` is how far out the face stands;
    steps are (depth in from the face, width, height) from the mouth inward, each running to the next one's depth."""
    cutters = []
    for k, (d, w, h) in enumerate(steps):
        d1 = steps[k + 1][0] if k + 1 < len(steps) else d + 1.2
        cutters.append(face_box(theta, out - d1, out - d + (0.5 if k == 0 else 0.002), w / 2, z - h / 2, z + h / 2))
    a = cutters[0]
    if len(cutters) > 1:
        bits.s_join(a, *cutters[1:])
    return a


def _slots(ob, k):
    """Mark a solid's faces with a slot number that survives cutting (what was the block, what was the cutter)."""
    name = f"slot{k}"
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    return ob


def s_copy(ob):
    c = ob.copy()
    c.data = ob.data.copy()
    bpy.context.scene.collection.objects.link(c)
    return c


def ragged(ob, size=0.18, amount=0.07, seed=0.0):
    """Make a cutter's faces ragged, so a break is a break and not a saw cut."""
    bits.s_cut_fine(ob, size)
    return bits.s_noise(ob, amount, 0.35, seed)


def split(body, cutter):
    """Break a finished solid in two along a cutter: returns (the piece inside the cutter, the rest). Faces made by
    the break carry slot 1."""
    _slots(body, 0)
    _slots(cutter, 1)
    piece = s_copy(body)
    c2 = s_copy(cutter)
    bits.s_both(piece, c2)
    bits.s_cut(body, cutter)
    return piece, body


def take_slots(m, ob, paints, part=None, smooth_=True):
    """As bits.take, the paint chosen by each face's slot (see `split`): paints is a name for slot 0 and for slot 1,
    or a function(middle, facing, slot)."""
    names = [s.name for s in ob.data.materials]
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.verts.ensure_lookup_table()
    verts = [Vector(v.co) for v in bm.verts]
    by = {}
    for f in bm.faces:
        k = 1 if (names and names[min(f.material_index, len(names) - 1)] == "slot1") else 0
        by.setdefault(k, []).append(tuple(v.index for v in f.verts))
    bm.free()
    for k, faces in by.items():
        used = sorted({i for f in faces for i in f})
        at = {i: j for j, i in enumerate(used)}
        p = paints if callable(paints) else None
        m.add([verts[i] for i in used], [tuple(at[i] for i in f) for f in faces],
              (lambda mid, n, k=k: p(mid, n, k)) if p else paints[k], smooth_ and k == 0, None, part)
    me = ob.data
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)


def rubble(m, at, spread, count, seed=0, size=(0.12, 0.45), paint="concrete", flat=0.7):
    """Broken concrete lying where it fell: lumps with broken faces."""
    g = rnd(seed)
    at = Vector(at)
    for _ in range(count if m.fine else max(3, count // 3)):
        r = g.uniform(*size)
        a, d = g.uniform(0, 6.283), spread * g.random() ** 0.7
        c = at + Vector((math.cos(a) * d, math.sin(a) * d * 0.8, r * 0.3 * flat))
        n = 7 if m.fine else 5
        rings = []
        ph = [g.uniform(0, 6.283) for _ in range(4)]
        for j in (-1, 0, 1):
            rr = r * (1.0 if j == 0 else 0.62)
            rings.append([c + Vector((math.cos(2 * math.pi * k / n + ph[0]) * rr * (1 + 0.3 * math.sin(3 * k + ph[1])), math.sin(2 * math.pi * k / n + ph[0]) * rr * (1 + 0.3 * math.cos(2 * k + ph[2])),
                                      j * r * 0.42 * flat + 0.05 * r * math.sin(k + ph[3]))) for k in range(n)])
        m.skin(rings, paint if g.random() < 0.7 else "concrete.in", (True, True), False)


def net_hooks(m, pts, out_of, paint="rust"):
    """Bent rods cast into a roof's edge to tie camouflage nets to."""
    if not m.fine:
        return
    for p, d in zip(pts, out_of):
        p, d = Vector(p), Vector(d).normalized()
        s = Vector((-d.y, d.x, 0))
        bits.tube(m, [p - s * 0.05, p - s * 0.05 + d * 0.06 + Z * 0.02, p + d * 0.085 + Z * 0.035, p + s * 0.05 + d * 0.06 + Z * 0.02, p + s * 0.05], 0.007, paint, 5)


def steel_door(m, hinge, theta, wide=0.74, high=1.52, paint="upper"):
    """A plate door hung on two hinges: `hinge` is the foot of its hinged edge, `theta` the way it faces when shut.
    Builds into the current part. The door lies from the hinge along the face (to the face's left)."""
    hinge = Vector(hinge)

    def at(n, t, z):
        return hinge + local(theta, n, t, z)
    u, n = local(theta, 0, 1, 0), local(theta, 1, 0, 0)
    bits.plate(m, [(0.0, 0.0), (wide, 0.0), (wide, high), (0.0, high)], 0.03, paint, hinge, u, Z)
    # an edge frame of flat bar, a brace across, the bolt heads
    for a, b in (((0.03, 0.03), (wide - 0.03, 0.03)), ((0.03, high - 0.03), (wide - 0.03, high - 0.03)), ((0.03, 0.03), (0.03, high - 0.03)), ((wide - 0.03, 0.03), (wide - 0.03, high - 0.03)),
                 ((0.03, high * 0.5), (wide - 0.03, high * 0.5))):
        m.box((at(0.02, a[0], a[1]) + at(0.02, b[0], b[1])) / 2, (0.012, abs(b[0] - a[0]) + 0.05, abs(b[1] - a[1]) + 0.05), paint, yaw=theta)
    if m.fine:
        for k in range(7):
            for tt in (0.03, wide - 0.03):
                bits.bolt(m, at(0.027, tt, 0.08 + k * (high - 0.16) / 6), n, 0.011, 0.008)
        for k in range(4):
            for zz in (0.03, high - 0.03, high * 0.5):
                bits.bolt(m, at(0.027, 0.12 + k * (wide - 0.24) / 3, zz), n, 0.011, 0.008)
    for zz in (0.25, high - 0.25):          # the hinges
        m.rod(at(0.03, -0.01, zz - 0.07), at(0.03, -0.01, zz + 0.07), 0.022, "steel", sides=10)
        m.box(at(0.022, 0.13, zz), (0.012, 0.28, 0.05), "steel", yaw=theta)
    # the handle and the bar that holds it shut
    m.rod(at(0.03, wide - 0.14, high * 0.5 - 0.09), at(0.075, wide - 0.14, high * 0.5 - 0.09), 0.01, "steel", sides=6)
    m.rod(at(0.075, wide - 0.14, high * 0.5 - 0.09), at(0.075, wide - 0.14, high * 0.5 - 0.24), 0.012, "steel", sides=6)
    # a look-out slit with its sliding cover
    m.box(at(0.016, wide * 0.5, high * 0.74), (0.012, 0.16, 0.035), "void", yaw=theta)
    m.box(at(0.028, wide * 0.5 + 0.10, high * 0.74), (0.012, 0.18, 0.07), "steel", yaw=theta)


def embrasure_gun(m, parent, pivot, theta, cooling="air", name="mg"):
    """A machine gun in an embrasure on a ball mounting: only what shows from outside. Parts `<name>` (traverse) and
    `<name>.gun` (elevation)."""
    pivot = Vector(pivot)
    d = local(theta, 1, 0, 0)
    s = local(theta, 0, 1, 0)
    m.part(name, parent, pivot, axis=(0, 0, -1), limits=[-28.0, 28.0], kind="turn", note="traverse in the embrasure: plus swings the muzzle to the gunner's right")
    m.use(name)
    # the plate that closes the throat, with the ball in it
    bits.plate(m, [(-0.21, -0.13), (0.21, -0.13), (0.21, 0.13), (-0.21, 0.13)], 0.02, "steel", pivot - d * 0.02, s, Z)
    m.ball(pivot, 0.085, "steel", 16 if m.fine else 8, 10 if m.fine else 6)
    m.part(name + ".gun", name, pivot, axis=tuple(-s), limits=[-10.0, 12.0], kind="turn", note="elevation: plus lifts the muzzle")
    m.use(name + ".gun")
    sides = 18 if m.fine else 8
    r = 0.05 if cooling == "water" else 0.031
    m.lathe(pivot, d, [(0.0, 0.04), (0.08, 0.04), (0.09, r), (0.46, r), (0.47, r + 0.004), (0.49, r + 0.004)], "upper" if cooling == "water" else "steel", sides)
    if cooling != "water" and m.fine:
        up = Z
        for row in range(6):
            a = math.radians(row * 60 + 30)
            for k in range(6):
                off = s * math.cos(a) + up * math.sin(a)
                m.disc(pivot + d * (0.13 + k * 0.053 + (0.026 if row % 2 else 0.0)) + off * (r + 0.0012), off, 0.0105, "void", 8, oval=1.7)
    m.lathe(pivot + d * 0.49, d, [(0.0, 0.02), (0.02, 0.02), (0.085, 0.033), (0.09, 0.033)], "steel", sides, (False, False))
    m.lathe(pivot + d * 0.58, -d, [(0.0, 0.03), (0.07, 0.008)], "void", 12, (False, True))
    m.socket(name + ".muzzle" if name != "mg" else "muzzle", pivot + d * 0.60, tuple(d), part=name + ".gun")
    return name, name + ".gun"


def gun_clip(name="mg", rounds=6, rate=10.0, muzzle="muzzle"):
    long = rounds / rate
    turn, events = [[0.0, 0.0]], []
    for k in range(rounds):
        t = k / rate
        turn += [[t + 0.015, 0.3], [t + 0.05, -0.1], [t + 0.085, 0.04]]
        events.append({"t": round(t, 3), "fx": "muzzle_flash_small", "socket": muzzle, "cue": "lmg_burst" if k == 0 else ""})
    turn.append([long, 0.0])
    return {"long": long, "loop": True, "about": f"A burst of {rounds} rounds from the embrasure, looping while it fires.", "tracks": [{"part": name + ".gun", "turn": turn}], "events": events}


def Pillbox(fine=True, nation="_default", name="Pillbox", cooling="air"):
    """A six-sided machine-gun pillbox: walls 0.6 m thick, a stepped embrasure in five faces, a steel door in the
    sixth behind a blast wall, turf on the roof, earth banked against it."""
    m = bits.model(name, "Pillbox (our own model)", "works", nation, fine)
    m.part("pillbox", None, (0, 0, 0), kind="root")
    A, A_top, wall = 2.15, 2.04, 0.6                # how far out the faces stand at the foot and at the top; the wall's thickness
    z_foot, z_wall, z_roof = -0.35, 1.62, 2.02
    emb_z = 0.98
    out_at = lambda z: A + (A_top - A) * (z - z_foot) / (z_wall - z_foot)        # noqa: E731

    # ---- the body: block, roof slab, room, door, embrasures; then chamfer every edge
    body = bits.s_prism(hexagon(A), z_foot, z_wall, top=hexagon(A_top))
    roof = bits.s_prism(hexagon(A_top + 0.16), z_wall - 0.02, z_roof, top=hexagon(A_top + 0.10))
    bits.s_join(body, roof)
    hoods = [face_box(th, out_at(emb_z + 0.36) - 0.1, out_at(emb_z + 0.36) + 0.09, 0.72, emb_z + 0.31, emb_z + 0.41) for th in (0, 60, -60, 120, -120)]
    bits.s_join(body, *hoods)
    room = bits.s_prism(hexagon(A - wall), 0.03, z_wall - 0.08, top=hexagon(A_top - wall))
    door = face_box(180, A - wall - 0.3, A + 0.3, 0.37, 0.05, 1.57)
    cuts = [room, door]
    steps = [(0.0, 1.16, 0.56), (0.15, 0.90, 0.44), (0.30, 0.64, 0.33), (0.45, 0.42, 0.24)]
    for th in (0, 60, -60, 120, -120):
        cuts.append(embrasure(th, out_at(emb_z), emb_z, steps))
    bits.s_cut(body, *cuts)
    bits.s_bevel(body, 0.03, 2)

    # ---- what a heavy hit breaks: the front face round its embrasure, and a corner of the roof
    break1 = ragged(bits.s_wedge([local(0, A + 0.6, -1.02, 0.36), local(0, A + 0.6, 0.95, 0.30), local(0, A + 0.6, 0.80, 2.3), local(0, A + 0.6, -0.92, 2.3)],
                                 [local(0, A - wall - 0.25, -0.80, 0.50), local(0, A - wall - 0.25, 0.72, 0.42), local(0, A - wall - 0.25, 0.62, 2.3), local(0, A - wall - 0.25, -0.70, 2.3)]), 0.2, 0.08, 2.0)
    front, body = split(body, break1)
    inside = lambda p: (math.hypot(p.x, p.y) < A - wall + 0.05 and p.z < z_wall)        # noqa: E731
    m.use("pillbox")
    take_slots(m, body, lambda mid, n, k: "concrete.in" if (k == 1 or inside(mid)) else "concrete", "pillbox")
    m.part("front", "pillbox", local(0, A - 0.3, 0, 0.36), kind="free", note="the front face round its embrasure: a heavy hit throws it out and down")
    take_slots(m, front, lambda mid, n, k: "concrete.in" if (k == 1 or inside(mid)) else "concrete", "front")
    # the floor
    m.use("pillbox")
    fl = hexagon(A - wall + 0.02)
    m.add([Vector((x, y, 0.031)) for x, y in fl] + [Vector((0, 0, 0.031))], [(k, (k + 1) % 6, 6) for k in range(6)], "concrete.in", False, lambda p: Z)

    # ---- the roof's furniture: turf laid over it, a ventilator, a periscope, hooks for nets round the edge
    g = rnd(5)
    n = 48
    ring = []
    for k in range(n):
        a = 2 * math.pi * k / n
        # (the sods follow the six-sided roof, a ragged hand's breadth short of its edge)
        rr = (A_top - 0.04) / math.cos(math.radians((math.degrees(a) + 30.0) % 60.0 - 30.0))
        rr = min(rr, A_top + 0.22) * (0.9 + 0.07 * bits.fbm((math.cos(a) * 2, math.sin(a) * 2, 1.0), 0.45) + 0.03 * bits.fbm((math.cos(a) * 2, math.sin(a) * 2, 5.0), 0.12))
        ring.append(Vector((math.cos(a) * rr, math.sin(a) * rr, z_roof + 0.012)))
    mid_ring = [Vector((p.x * 0.72, p.y * 0.72, z_roof + 0.13 + 0.04 * bits.fbm((p.x, p.y, 4.0), 0.8))) for p in ring]
    m.add(ring + mid_ring, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)], "turf", True, lambda p: Z)
    m.add(mid_ring + [Vector((0, 0, z_roof + 0.17))], [(k, (k + 1) % n, n) for k in range(n)], "turf", True, lambda p: Z)
    if fine:
        for _ in range(900):
            a, d = g.uniform(0, 6.283), (A_top - 0.3) * g.random() ** 0.5
            base = Vector((math.cos(a) * d, math.sin(a) * d, z_roof + 0.02 + 0.12 * (1.0 - (d / A_top) ** 2)))
            ang = g.uniform(0, 6.283)
            side = Vector((-math.sin(ang), math.cos(ang), 0)) * 0.009
            tip = base + Vector((math.cos(ang) * g.uniform(0.02, 0.08), math.sin(ang) * g.uniform(0.02, 0.08), g.uniform(0.06, 0.2)))
            m.add([base - side, base + side, tip], [(0, 1, 2)], "turf" if g.random() < 0.6 else "garnish.2", False, None)
    m.lathe((-0.75, 0.55, z_roof), Z, [(0.0, 0.07), (0.34, 0.07), (0.36, 0.13), (0.44, 0.13), (0.50, 0.03)], "upper", 14)          # the ventilator's cowl
    m.lathe((0.35, -0.6, z_roof), Z, [(0.0, 0.045), (0.5, 0.045), (0.5, 0.06), (0.62, 0.06)], "upper", 12)                          # the periscope's tube
    m.box((0.35 + 0.045, -0.6, z_roof + 0.57), (0.03, 0.07, 0.05), "glass")
    hooks, ways = [], []
    for th in range(0, 360, 60):
        for t in (-0.9, -0.3, 0.3, 0.9):
            hooks.append(local(th, A_top + 0.135, t, z_roof - 0.14))
            ways.append(local(th, 1, 0, 0))
    net_hooks(m, hooks, ways)

    # ---- the door, in its frame, behind a blast wall
    hinge = local(180, A_top + 0.03 + (A - A_top) * 0.55, -0.37, 0.06)
    m.part("door", "pillbox", hinge, axis=(0, 0, 1), limits=[0.0, 110.0], kind="turn", note="the steel door: plus opens it outward")
    m.use("door")
    steel_door(m, hinge, 180, 0.74, 1.50)
    m.use("pillbox")
    wall_ob = bits.s_box((-A - 1.75, -1.55, -0.3), (-A - 1.35, 1.25, 1.72))
    ret = bits.s_box((-A - 1.75, -1.55, -0.3), (-A - 0.55, -1.15, 1.72))
    bits.s_join(wall_ob, ret)
    bits.cast(m, wall_ob, "concrete", 0.035, "pillbox")
    # a concrete step at the door and a trodden path round the blast wall
    bits.cast(m, bits.s_box((-A - 0.55, -0.45, -0.2), (-A + 0.05, 0.45, 0.05)), "concrete", 0.02, "pillbox")

    # ---- earth banked against the five fighting faces, falling away outward
    path = [local(th, out_at(0.45) + 0.02, t, 0.0) for th, ts in ((-120, (-1.2, 0.0, 1.2)), (-60, (-1.2, 0.0, 1.2)), (0, (-1.2, 0.0, 1.2)), (60, (-1.2, 0.0, 1.2)), (120, (-1.2, 0.0, 1.2))) for t in ts]
    bits.bank(m, [(p.x, p.y) for p in path], [(-1.8, 0.0), (-1.5, 0.05), (-1.2, 0.13), (-0.9, 0.24), (-0.6, 0.37), (-0.3, 0.5), (-0.1, 0.58), (0.03, 0.6)], "earth", seed=7, lump=0.07, step=0.3, turf=0.5)
    bits.patch(m, -0.6, 0.0, 4.6, "earth.dry", seed=3, oval=1.12)

    # ---- the gun in the front embrasure, and the others' places
    pivot = local(0, out_at(emb_z) - 0.47, 0.0, emb_z)
    embrasure_gun(m, "front", pivot, 0.0, cooling)
    m.use("pillbox")
    for k, th in enumerate((60, -60, 120, -120)):
        p = local(th, out_at(emb_z) - 0.47, 0.0, emb_z)
        d = local(th, 1, 0, 0)
        m.socket(f"embrasure.{k + 1}", p, tuple(d), part="pillbox", note="a rifleman or a second gun fires from here")
        bits.crew(m, f"rifle.{k + 1}", p - d * 0.55 - Z * (emb_z - 0.03), tuple(d), "stand", "pillbox", "standing at the embrasure, firing through it")
    bits.crew(m, "gunner", pivot - X * 0.75 - Z * (emb_z - 0.03), (1, 0, 0), "stand", "pillbox", "standing at the gun in the front embrasure")
    bits.crew(m, "loader", pivot - X * 0.6 + Y * 0.5 - Z * (emb_z - 0.03), (0.7, -0.7, 0), "stand", "pillbox", "at the gunner's left, feeding the belt")
    m.socket("hit", local(0, A + 0.05, 0.0, emb_z + 0.1), (1, 0, 0), part="pillbox", note="where the shell or the charge strikes")
    m.socket("smoke", (0.0, 0.0, z_roof - 0.5), (0, 0, 1), part="pillbox", note="smoke pours from the embrasures and the break when it is knocked out")
    m.socket("fire", (0.6, 0.0, 0.5), (1, 0, 0.5), part="pillbox", note="what burns inside shows here")

    # ---- rubble that lies hidden until the break
    m.part("rubble", "pillbox", local(0, A + 0.2, 0, 0.3), kind="free", note="hidden (scale 0) until the front is blown in")
    m.use("rubble")
    rubble(m, local(0, A + 1.0, 0.0, 0.42), 1.25, 34, seed=4)
    m.use("pillbox")

    q1 = Quaternion(Vector((0.08, -1, 0)).normalized(), math.radians(38.0))                 # the front falls outward on its foot
    m.clips["fire"] = gun_clip("mg")
    m.clips["idle"] = {"long": 2.0, "loop": True, "about": "Standing; the rubble hidden.", "tracks": [{"part": "rubble", "scale": [[0.0, 0.0]]}], "events": []}
    m.clips["knocked_out"] = {
        "long": 2.6, "loop": False, "after": "hold",
        "about": "A heavy shell or a charge at the embrasure: the front face is blown out and falls on its foot, rubble is thrown down the bank, the door is blown open, smoke pours out. "
                 "(At rest play `idle`, which keeps the rubble hidden.)",
        "tracks": [{"part": "front", "rot": [[0.0, 1, 0, 0, 0], [0.25, *Quaternion().slerp(q1, 0.45)], [0.7, *Quaternion().slerp(q1, 0.9)], [0.9, *q1], [1.0, *Quaternion().slerp(q1, 0.96)], [1.15, *q1]],
                    "move": [[0.0, 0, 0, 0], [0.25, 0.22, 0.0, 0.05], [0.7, 0.42, 0.02, -0.16], [0.9, 0.5, 0.02, -0.28]]},
                   {"part": "rubble", "scale": [[0.0, 0.0], [0.12, 0.0], [0.3, 1.0]], "move": [[0.0, -1.0, 0, 0.6], [0.12, -1.0, 0, 0.6], [0.3, -0.35, 0, 0.5], [0.75, 0.0, 0, 0.0]]},
                   {"part": "door", "turn": [[0.0, 0.0], [0.08, 0.0], [0.3, 96.0], [0.42, 82.0], [0.6, 90.0]]}],
        "events": [{"t": 0.0, "fx": "shell_burst_big", "socket": "hit", "cue": "explosion_debris"},
                   {"t": 0.05, "fx": "debris", "socket": "hit", "cue": ""},
                   {"t": 0.25, "fx": "smoke_black", "socket": "smoke", "cue": "building_collapse"},
                   {"t": 0.6, "fx": "fire", "socket": "fire", "cue": "fire_start", "cue_loop": "fire_small_loop"}]}
    weather(m, [(pivot + X * 0.75, 0.75, 0.6)] + [(local(th, out_at(emb_z) + 0.05, 0, emb_z + 0.25), 0.5, 0.3) for th in (60, -60, 120, -120)], splash=0.9)
    m.info["span"] = 6.4
    m.info["high"] = 0.9
    m.info["worn"] = [(-0.8, 0.0, 5.0), (-4.2, 1.6, 2.2)]
    m.info["clear"] = [hexagon(A + 0.1), [(-A - 1.9, -1.7), (-A - 0.4, -1.7), (-A - 0.4, 1.4), (-A - 1.9, 1.4)]]
    return m


BUILDERS = {
    "Pillbox": Pillbox,
}
