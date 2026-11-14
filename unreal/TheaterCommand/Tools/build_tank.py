"""The trial tank (art fix 12): one medium tank of our own design, sculpted wholly by this script.

Run by Blender without its window (see build_tank.sh):

    Blender --background --python Tools/build_tank.py -- --out Content/Models/TrialTank.json

Nothing is drawn by hand and nothing is fetched: every part is made here from numbers, so the piece can be
rebuilt and changed by editing them, and the same script can later turn out national variants (a longer gun,
another turret, skirts, a different count of road wheels) by passing other values in SPEC.

The tank is a generic medium tank of the period and not a copy of any real vehicle or of any game's model:
a welded hull with a sloped glacis, a cast-looking turret with a bustle and a commander's cupola, a long gun
with a muzzle brake, six road wheels a side under three return rollers, and a few things stowed on it.

What is written out is plain numbers the game reads when it starts: points (metres; x forward, y to the
right as the game has it, z up, the ground at 0), which way each faces, a paint for each (see PAINTS), and
the triangles. Edges are bevelled so that the lamp catches them. A second, plainer mesh is written for the
far zoom.
"""

import json
import math
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

# The paints, by number. The game gives each its colour: 0 and 1 are the nation's own scheme.
PAINTS = {
    "paint": 0,      # the hull colour
    "shade": 1,      # the same, a shade darker: the second tone of the scheme, and the recesses
    "track": 2,      # track links: dark worn steel
    "rubber": 3,     # the tyres of the road wheels
    "wood": 4,       # tool handles, the jack block
    "steel": 5,      # bare tools, the tow cable, the gun's muzzle
    "canvas": 6,     # a rolled tarpaulin, a bedroll
    "glass": 7,      # headlamps and periscopes
    "rust": 8,       # the exhausts
}

# The numbers the tank is made from, in metres. A national variant is another set of these.
SPEC = {
    "hull_long": 5.5, "hull_wide": 1.78, "hull_floor": 0.42, "hull_deck": 1.18,
    "upper_long": 4.5, "upper_wide": 2.62, "upper_high": 0.5, "glacis": 0.95,
    "turret_long": 2.25, "turret_wide": 1.9, "turret_high": 0.62, "turret_x": 0.25,
    "gun_long": 2.9, "gun_calibre": 0.09,
    "track_wide": 0.46, "track_out": 1.31, "wheels": 6, "wheel_r": 0.33, "wheel_first": -1.78, "wheel_step": 0.72,
    "sprocket_x": -2.52, "sprocket_z": 0.66, "sprocket_r": 0.36,
    "idler_x": 2.42, "idler_z": 0.62, "idler_r": 0.33,
    "rollers": (-1.4, 0.0, 1.4), "roller_z": 0.93, "roller_r": 0.11,
    "bevel": 0.028,
}


def fresh_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make(name, bm, paint, bevel=0.0, segments=1):
    """Turns a bmesh into an object painted one paint, with its edges bevelled if asked."""
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj["paint"] = PAINTS[paint]
    if bevel > 0.0:
        mod = obj.modifiers.new("bevel", "BEVEL")
        mod.width = bevel
        mod.segments = segments
        mod.limit_method = "ANGLE"
        mod.angle_limit = math.radians(40.0)
    return obj


def box(name, size, at, paint, bevel=0.0, turn=(0.0, 0.0, 0.0), shape=None):
    """A box, its middle at 'at'. 'shape' may move its corners before it is placed: it is given each corner
    as a point in a cube from -0.5 to 0.5 and returns where that corner goes, in the same measure."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        p = Vector(shape(v.co.copy())) if shape else v.co.copy()
        v.co = Vector((p.x * size[0], p.y * size[1], p.z * size[2]))
    place = Matrix.Translation(Vector(at)) @ Matrix.Rotation(math.radians(turn[2]), 4, "Z") \
        @ Matrix.Rotation(math.radians(turn[1]), 4, "Y") @ Matrix.Rotation(math.radians(turn[0]), 4, "X")
    bmesh.ops.transform(bm, matrix=place, verts=bm.verts)
    return make(name, bm, paint, bevel)


def drum(name, radius, depth, at, paint, sides=12, axis="Y", radius2=None, bevel=0.0):
    """A cylinder (or a cone frustum) lying along an axis, its middle at 'at'."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=sides, radius1=radius,
                          radius2=radius if radius2 is None else radius2, depth=depth)
    turn = Matrix.Identity(4)
    if axis == "Y":
        turn = Matrix.Rotation(math.radians(90.0), 4, "X")
    elif axis == "X":
        turn = Matrix.Rotation(math.radians(90.0), 4, "Y")
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(at)) @ turn, verts=bm.verts)
    return make(name, bm, paint, bevel)


def track(name, side, s):
    """One track: a band of links run round the sprocket, the road wheels and the idler."""
    # The path of the track, seen from the side: along the ground, up round the idler at the front, back
    # along the top with a little sag between the rollers, and down round the sprocket at the rear.
    path = []
    ground = 0.045
    front, rear = (s["idler_x"], s["idler_z"], s["idler_r"]), (s["sprocket_x"], s["sprocket_z"], s["sprocket_r"])
    first, last = s["wheel_first"], s["wheel_first"] + s["wheel_step"] * (s["wheels"] - 1)
    for k in range(13):                                   # the ground run
        path.append((first - 0.1 + (last - first + 0.2) * k / 12.0, ground))
    for k in range(1, 9):                                 # up round the idler
        a = math.radians(-70.0 + 160.0 * k / 8.0)
        path.append((front[0] + front[2] * math.sin(a) * 1.0 + 0.0, front[1] - front[2] * math.cos(a)))
    top = s["roller_z"] + s["roller_r"] + 0.03
    for k in range(1, 12):                                # the top run, sagging between the rollers
        x = front[0] - 0.12 - (front[0] - rear[0] - 0.24) * k / 12.0
        sag = 0.035 * abs(math.sin(math.pi * (x - s["rollers"][0]) / (s["rollers"][1] - s["rollers"][0])))
        path.append((x, top - sag))
    for k in range(0, 8):                                 # down round the sprocket
        a = math.radians(90.0 + 160.0 * k / 8.0)
        path.append((rear[0] - rear[2] * math.sin(a - math.radians(90.0)) * 1.0, rear[1] + rear[2] * math.cos(a - math.radians(90.0))))
    bm = bmesh.new()
    half, thick = s["track_wide"] * 0.5, 0.05
    y = side * s["track_out"]
    count = len(path)
    rings = []
    for k in range(count):
        here, ahead, behind = Vector(path[k]), Vector(path[(k + 1) % count]), Vector(path[k - 1])
        along = (ahead - behind).normalized()
        out = Vector((along.y, -along.x))                 # outward from the loop
        # (every other link stands a little proud, which is all the detail a link needs at this size)
        proud = thick + (0.018 if k % 2 == 0 else 0.0)
        outer, inner = here + out * proud, here - out * 0.012
        rings.append([bm.verts.new((outer.x, y - half, outer.y)), bm.verts.new((outer.x, y + half, outer.y)),
                      bm.verts.new((inner.x, y + half, inner.y)), bm.verts.new((inner.x, y - half, inner.y))])
    for k in range(count):
        a, b = rings[k], rings[(k + 1) % count]
        for e in range(4):
            bm.faces.new((a[e], a[(e + 1) % 4], b[(e + 1) % 4], b[e]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return make(name, bm, "track")


def build(s):
    parts = []
    b = s["bevel"]
    floor, deck = s["hull_floor"], s["hull_deck"]

    # ---- The hull: a welded tub between the tracks, its nose and tail plates sloped.
    def tub(p):
        if p.z < 0.0 and p.x > 0.0:
            p.x -= 0.11           # the lower nose plate runs back to the floor
        if p.z < 0.0 and p.x < 0.0:
            p.x += 0.07
        if p.z > 0.0 and p.x > 0.0:
            p.x += 0.02
        return p
    parts.append(box("hull", (s["hull_long"], s["hull_wide"], deck - floor), (0.0, 0.0, (floor + deck) * 0.5), "paint", b, shape=tub))

    # ---- The upper hull: over the tracks, with the glacis sloped back and the sides leaning in a little.
    up_top = deck + s["upper_high"]
    def upper(p):
        if p.z > 0.0:
            p.y *= 0.94
            if p.x > 0.0:
                p.x -= s["glacis"] / s["upper_long"]     # the glacis
            else:
                p.x += 0.18 / s["upper_long"]            # the engine deck's tail
        return p
    upper_x = -0.12
    parts.append(box("upper", (s["upper_long"], s["upper_wide"], s["upper_high"]), (upper_x, 0.0, deck + s["upper_high"] * 0.5), "paint", b, shape=upper))
    # The nose between the glacis and the lower plate.
    parts.append(box("nose", (0.62, s["hull_wide"] + 0.02, 0.34), (s["upper_long"] * 0.5 + upper_x + 0.16, 0.0, deck - 0.02), "paint", b,
                     shape=lambda p: Vector((p.x - (0.35 if p.z > 0.0 else 0.0), p.y, p.z))))

    # ---- The track guards, with a flap turned down at each end.
    for side in (-1.0, 1.0):
        y = side * s["track_out"]
        parts.append(box("guard", (5.35, s["track_wide"] + 0.14, 0.04), (-0.05, y, deck + 0.05), "paint", 0.012))
        parts.append(box("flap_front", (0.42, s["track_wide"] + 0.14, 0.035), (2.78, y, deck - 0.06), "paint", 0.0, turn=(0.0, 32.0, 0.0)))
        parts.append(box("flap_rear", (0.34, s["track_wide"] + 0.14, 0.035), (-2.86, y, deck - 0.04), "paint", 0.0, turn=(0.0, -28.0, 0.0)))

    # ---- The running gear.
    for side in (-1.0, 1.0):
        y = side * s["track_out"]
        parts.append(track("track", side, s))
        for k in range(s["wheels"]):
            x = s["wheel_first"] + k * s["wheel_step"]
            parts.append(drum("tyre", s["wheel_r"], 0.3, (x, y, s["wheel_r"] + 0.05), "rubber", 14))
            parts.append(drum("wheel", s["wheel_r"] * 0.78, 0.32, (x, y, s["wheel_r"] + 0.05), "paint", 12))
            parts.append(drum("hub", 0.09, 0.4, (x, y, s["wheel_r"] + 0.05), "shade", 8))
            # (the swing arm of its suspension, seen between the wheels)
            parts.append(box("arm", (0.36, 0.08, 0.1), (x - 0.2, side * (s["hull_wide"] * 0.5 + 0.06), s["wheel_r"] + 0.17), "shade", 0.0, turn=(0.0, -24.0, 0.0)))
        parts.append(drum("sprocket", s["sprocket_r"], 0.34, (s["sprocket_x"], y, s["sprocket_z"]), "shade", 14))
        parts.append(drum("sprocket_hub", 0.16, 0.42, (s["sprocket_x"], y, s["sprocket_z"]), "paint", 10))
        parts.append(drum("idler", s["idler_r"], 0.3, (s["idler_x"], y, s["idler_z"]), "shade", 14))
        parts.append(drum("idler_hub", 0.11, 0.38, (s["idler_x"], y, s["idler_z"]), "paint", 8))
        for x in s["rollers"]:
            parts.append(drum("roller", s["roller_r"], 0.26, (x, y, s["roller_z"]), "rubber", 8))

    # ---- The turret: cast, wider at its waist than at its roof, with a bustle behind.
    tx, tz = s["turret_x"], up_top + 0.02
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=16, radius1=1.0, radius2=0.84, depth=1.0)
    for v in bm.verts:
        # an egg in plan: blunt and narrower at the front, fuller toward the rear
        front = max(v.co.x, 0.0)
        v.co.y *= s["turret_wide"] * 0.5 * (1.0 - 0.18 * front)
        v.co.x *= s["turret_long"] * 0.5 * (0.86 if v.co.x > 0.0 else 1.0)
        v.co.z = (v.co.z + 0.5) * s["turret_high"]
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector((tx - 0.1, 0.0, tz))), verts=bm.verts)
    parts.append(make("turret", bm, "paint", b * 1.4, 2))
    parts.append(box("bustle", (0.72, 1.24, 0.4), (tx - 1.32, 0.0, tz + 0.3), "paint", b,
                     shape=lambda p: Vector((p.x, p.y * (0.86 if p.x < 0.0 else 1.0), p.z + (0.12 if p.x < 0.0 and p.z < 0.0 else 0.0)))))
    # The commander's cupola, with its hatch standing open a hand, and the loader's hatch flat on the roof.
    roof = tz + s["turret_high"]
    parts.append(drum("cupola", 0.3, 0.2, (tx - 0.5, -0.38, roof + 0.1), "paint", 12, axis="Z", radius2=0.27, bevel=0.012))
    parts.append(drum("cupola_hatch", 0.24, 0.035, (tx - 0.5, -0.38, roof + 0.225), "shade", 12, axis="Z"))
    for k in range(5):
        a = math.radians(-70.0 + 35.0 * k)
        parts.append(box("periscope", (0.05, 0.11, 0.045), (tx - 0.5 + 0.3 * math.cos(a), -0.38 + 0.3 * math.sin(a), roof + 0.12), "glass", 0.0,
                         turn=(0.0, 0.0, math.degrees(a))))
    parts.append(box("hatch", (0.5, 0.42, 0.035), (tx - 0.42, 0.42, roof + 0.02), "shade", 0.008))
    parts.append(drum("vent", 0.13, 0.07, (tx + 0.38, 0.0, roof + 0.03), "paint", 10, axis="Z", radius2=0.09))
    parts.append(drum("aerial_foot", 0.035, 0.1, (tx - 1.5, 0.48, tz + 0.55), "shade", 6, axis="Z"))
    parts.append(drum("aerial", 0.008, 1.5, (tx - 1.5, 0.48, tz + 1.35), "steel", 4, axis="Z"))
    # Lifting eyes, and a rail on the bustle with a rolled tarpaulin lashed to it.
    for side in (-1.0, 1.0):
        parts.append(box("eye", (0.1, 0.03, 0.07), (tx + 0.25, side * 0.72, roof - 0.04), "shade"))
    parts.append(drum("tarpaulin", 0.13, 1.05, (tx - 1.74, 0.0, tz + 0.34), "canvas", 8))
    for y in (-0.36, 0.36):
        parts.append(box("strap", (0.3, 0.035, 0.29), (tx - 1.74, y, tz + 0.34), "wood"))

    # ---- The gun: a mantlet, a sleeve, the barrel and its muzzle brake; and the machine-gun beside it.
    gx, gz = tx + s["turret_long"] * 0.5 * 0.86 - 0.16, tz + s["turret_high"] * 0.52
    parts.append(box("mantlet", (0.36, 1.0, 0.56), (gx + 0.06, 0.0, gz), "paint", b * 1.3,
                     shape=lambda p: Vector((p.x, p.y * (0.72 if p.x > 0.0 else 1.0), p.z * (0.74 if p.x > 0.0 else 1.0)))))
    cal = s["gun_calibre"]
    parts.append(drum("sleeve", cal * 1.75, 0.6, (gx + 0.5, 0.0, gz), "paint", 10, axis="X", radius2=cal * 1.35))
    parts.append(drum("barrel", cal, s["gun_long"], (gx + 0.24 + s["gun_long"] * 0.5, 0.0, gz), "paint", 10, axis="X", radius2=cal * 0.82))
    end = gx + 0.24 + s["gun_long"]
    parts.append(drum("brake", cal * 1.5, 0.3, (end - 0.1, 0.0, gz), "shade", 10, axis="X"))
    parts.append(drum("muzzle", cal * 0.9, 0.34, (end - 0.1, 0.0, gz), "steel", 8, axis="X"))
    parts.append(drum("coaxial", 0.022, 0.5, (gx + 0.36, 0.3, gz + 0.02), "steel", 5, axis="X"))

    # ---- On the glacis: the driver's visor, the bow gun in its ball, spare links, and the headlamps.
    slope_x, slope_z = s["upper_long"] * 0.5 + upper_x - s["glacis"] * 0.5, deck + s["upper_high"] * 0.5
    lean = math.degrees(math.atan2(s["upper_high"], s["glacis"]))
    parts.append(box("visor", (0.08, 0.5, 0.16), (slope_x + 0.05, -0.55, slope_z + 0.07), "shade", 0.008, turn=(0.0, -lean, 0.0)))
    parts.append(box("visor_slit", (0.085, 0.3, 0.035), (slope_x + 0.07, -0.55, slope_z + 0.085), "glass", 0.0, turn=(0.0, -lean, 0.0)))
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=6, radius=0.15)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector((slope_x + 0.02, 0.56, slope_z + 0.03))), verts=bm.verts)
    parts.append(make("ball", bm, "paint"))
    parts.append(drum("bow_gun", 0.02, 0.42, (slope_x + 0.26, 0.56, slope_z + 0.02), "steel", 5, axis="X"))
    for k in range(3):
        # (spare track links carried across the nose, where they thicken the armour a little)
        parts.append(box("spare_link", (0.11, 0.42, 0.045), (slope_x + 0.34 - 0.15 * k, 0.0, slope_z - 0.13 + 0.079 * k), "track", 0.0,
                         turn=(0.0, -lean, 0.0)))
    for side in (-1.0, 1.0):
        parts.append(drum("lamp", 0.085, 0.11, (2.5, side * 1.0, deck + 0.16), "paint", 8, axis="X"))
        parts.append(drum("lamp_glass", 0.07, 0.115, (2.51, side * 1.0, deck + 0.16), "glass", 8, axis="X"))
        parts.append(box("lamp_foot", (0.04, 0.04, 0.1), (2.5, side * 1.0, deck + 0.09), "shade"))

    # ---- The engine deck: two grilles with their louvres, and the exhausts across the tail.
    for side in (-1.0, 1.0):
        parts.append(box("grille", (0.95, 0.72, 0.03), (-1.55, side * 0.52, up_top + 0.012), "shade", 0.006))
        for k in range(6):
            parts.append(box("louvre", (0.05, 0.66, 0.035), (-1.95 + 0.16 * k, side * 0.52, up_top + 0.035), "paint", 0.0, turn=(0.0, 30.0, 0.0)))
        parts.append(drum("exhaust", 0.11, 0.78, (-2.82, side * 0.52, deck + 0.22), "rust", 10))
        parts.append(drum("exhaust_pipe", 0.04, 0.3, (-2.82, side * 0.98, deck + 0.3), "rust", 6, axis="Z"))
    parts.append(box("tail_plate", (0.06, 1.9, 0.5), (-2.72, 0.0, deck + 0.1), "paint", 0.012))

    # ---- Stowed on the guards and the deck: a tool box, a shovel and a pick, jerrycans, the jack and its
    # block, and the tow cable coiled on the tail.
    parts.append(box("toolbox", (0.72, 0.36, 0.2), (0.55, -s["track_out"], deck + 0.18), "paint", 0.012))
    parts.append(box("toolbox_lid", (0.74, 0.38, 0.02), (0.55, -s["track_out"], deck + 0.29), "shade"))
    parts.append(drum("shovel_shaft", 0.017, 0.95, (-0.6, -s["track_out"] - 0.06, deck + 0.1), "wood", 5, axis="X"))
    parts.append(box("shovel_blade", (0.26, 0.19, 0.02), (-0.02, -s["track_out"] - 0.06, deck + 0.1), "steel"))
    parts.append(drum("pick_shaft", 0.017, 0.8, (-0.62, -s["track_out"] + 0.1, deck + 0.1), "wood", 5, axis="X"))
    parts.append(box("pick_head", (0.05, 0.42, 0.04), (-0.24, -s["track_out"] + 0.1, deck + 0.1), "steel"))
    for k in range(3):
        parts.append(box("jerrycan", (0.17, 0.33, 0.44), (-1.0 - 0.21 * k, s["track_out"], deck + 0.3), "shade" if k == 1 else "paint", 0.02))
        parts.append(box("can_cap", (0.05, 0.05, 0.04), (-1.0 - 0.21 * k, s["track_out"] - 0.1, deck + 0.54), "steel"))
    parts.append(box("jack", (0.42, 0.16, 0.16), (0.9, s["track_out"], deck + 0.16), "steel", 0.01))
    parts.append(box("jack_block", (0.34, 0.3, 0.12), (0.35, s["track_out"], deck + 0.14), "wood", 0.008))
    bm = bmesh.new()
    cable = []
    for k in range(20):
        a = 2.0 * math.pi * k / 20.0
        ring = []
        for e in range(5):
            c = 2.0 * math.pi * e / 5.0
            r = 0.3 + 0.03 * math.cos(c)
            ring.append(bm.verts.new((-2.25 + r * math.cos(a) * 0.62, r * math.sin(a), up_top + 0.045 + 0.03 * math.sin(c))))
        cable.append(ring)
    for k in range(20):
        a_ring, b_ring = cable[k], cable[(k + 1) % 20]
        for e in range(5):
            bm.faces.new((a_ring[e], a_ring[(e + 1) % 5], b_ring[(e + 1) % 5], b_ring[e]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    parts.append(make("tow_cable", bm, "steel"))
    return parts


def gather(parts, simple=False):
    """Every part's finished mesh (bevels and all) as flat lists the game can read."""
    points, facings, paints, triangles = [], [], [], []
    seen = {}
    graph = bpy.context.evaluated_depsgraph_get()
    crease = math.cos(math.radians(40.0))
    for obj in parts:
        if simple:
            for mod in obj.modifiers:
                mod.show_viewport = False                 # the far mesh has no bevels
            # (and the smallest things are left off it: they cannot be seen from there)
            if obj.name.split(".")[0] in ("periscope", "eye", "strap", "louvre", "lamp_foot", "can_cap", "coaxial", "bow_gun", "aerial",
                                          "aerial_foot", "hub", "idler_hub", "arm", "visor_slit", "lamp_glass", "pick_head", "pick_shaft",
                                          "shovel_shaft", "shovel_blade", "exhaust_pipe", "tow_cable", "spare_link", "toolbox_lid"):
                continue
            graph.update()
        bm = bmesh.new()
        bm.from_object(obj, graph)
        bmesh.ops.transform(bm, matrix=obj.matrix_world, verts=bm.verts)
        bmesh.ops.triangulate(bm, faces=bm.faces)
        bm.normal_update()
        paint = int(obj["paint"])
        for face in bm.faces:
            corners = []
            for loop in face.loops:
                # Which way this corner faces: the faces round its point that lie within forty degrees of this
                # one are taken together, so a bevel is rounded and a true edge stays an edge.
                facing = Vector((0.0, 0.0, 0.0))
                for near in loop.vert.link_faces:
                    if near.normal.dot(face.normal) >= crease:
                        facing += near.normal * near.calc_area()
                facing = facing.normalized() if facing.length > 1.0e-9 else face.normal.copy()
                p = loop.vert.co
                # (the game's "right" is the other hand from Blender's, so y is turned about)
                key = (round(p.x, 4), round(-p.y, 4), round(p.z, 4), round(facing.x, 3), round(-facing.y, 3), round(facing.z, 3), paint)
                index = seen.get(key)
                if index is None:
                    index = len(points) // 3
                    seen[key] = index
                    points.extend(key[0:3])
                    facings.extend(key[3:6])
                    paints.append(paint)
                corners.append(index)
            triangles.extend(corners)
        bm.free()
    return {"points": points, "facings": facings, "paints": paints, "triangles": triangles}


def main():
    out = "TrialTank.json"
    if "--" in sys.argv:
        rest = sys.argv[sys.argv.index("--") + 1:]
        if "--out" in rest:
            out = rest[rest.index("--out") + 1]
    fresh_scene()
    parts = build(SPEC)
    near = gather(parts)
    far = gather(parts, simple=True)
    low = min(near["points"][2::3])
    data = {
        "name": "Trial medium tank",
        "about": "Made by Tools/build_tank.py. Metres; x forward, y right, z up. Paints: " + ", ".join(f"{v} {k}" for k, v in PAINTS.items()),
        "long": round(max(near["points"][0::3]) - min(near["points"][0::3]), 3),
        "high": round(max(near["points"][2::3]) - low, 3),
        "near": near,
        "far": far,
    }
    with open(out, "w") as file:
        json.dump(data, file, separators=(",", ":"))
    print(f"Theater Command: trial tank written to {out}: {len(near['triangles']) // 3} triangles, {len(near['points']) // 3} points "
          f"(far: {len(far['triangles']) // 3} triangles), {data['long']} m long, {data['high']} m high")


main()
