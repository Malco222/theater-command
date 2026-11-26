"""The tanks (art fix 12): tanks of our own design, sculpted wholly by this script.

Run by Blender without its window (see build_tank.sh):

    Blender --background --python Tools/build_tank.py -- --out Content/Models/TrialTank.json
    Blender --background --python Tools/build_tank.py -- --all Content/Models

The first writes the trial tank alone; the second writes every tank in VARIANTS, each under the name the game
knows its tank by, so that the game stands it on the table in place of the one it builds in code.

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
    "stripe": 9,     # the thin lines that part the fields of a camouflage
}

# A figure's paints, by number: the game gives each the colour of the nation's own kit.
FIGURE_PAINTS = {"tunic": 0, "trousers": 1, "boots": 2, "lower": 3, "straps": 4, "helmet": 5, "pack": 6, "skin": 7, "wood": 8, "steel": 9,
                 "coat": 10, "coat_dark": 11, "leather": 12, "blanket": 13, "hoof": 14}
PAINT_SET = PAINTS     # (the paints the part being made is painted from: a vehicle's, or a figure's)
FAR_RATIO = 1.0        # (how much of each part's mesh is kept in the plainer, far mesh: a figure's is thinned, since a base carries several)
CAMO = None            # (a camouflage in fields for the vehicle being made: the size of its fields in metres, or None for a plain coat)

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
    # What a variant may change besides the measures above:
    "scale": 1.0,            # the whole tank larger or smaller
    "upper_lean": 0.94,      # how far the upper hull's sides lean in toward its roof (1 upright)
    "tail": 0.18,            # how far the engine deck's tail slopes in
    "interleaved": False,    # a second row of road wheels set between the first, inboard of them
    "skirt": None,           # side plates over the running gear: (top, bottom, how many mud chutes)
    "turret": "cast",        # "cast" (an egg in plan), "box" (welded plates) or "round" (a drum)
    "turret_taper": 0.84,    # the turret's roof against its waist
    "turret_y": 0.0,         # the turret set to one side
    "bustle": True, "cupola": True, "brake": True,
    "hull_gun": None,        # a gun in the hull's front beside the driver: (calibre, length, how far to the side)
    "stowage": True,         # True: the tools, cans, jack and cable; "box": one stowage box on the right rear guard; False: nothing
    "waist": 0.0,            # an upright band of this height under the sloped upper hull (its nose plate then starts that much higher)
    "mantlet": "taper",      # "taper" (narrowing toward the gun) or "square" (a plain square plate)
    "bow_gun": True, "spare_links": True,
    "silencer": False,       # one silencer across the tail in place of the two exhausts
}

# The tanks the game asks for by name. Each is the trial tank's numbers with some changed: generic tanks of the
# period in each nation's manner (how many wheels, where the turret sits, how long the gun), not copies of any
# drawing or of any other game's model.
VARIANTS = {
    # A medium tank with many small road wheels and a short gun for the infantry's support.
    "PanzerIV": {"hull_long": 5.6, "wheels": 8, "wheel_r": 0.235, "wheel_first": -2.02, "wheel_step": 0.58, "rollers": (-1.6, -0.55, 0.5, 1.55),
                 "roller_z": 0.86, "turret": "box", "turret_taper": 0.9, "gun_long": 1.55, "gun_calibre": 0.105, "brake": False},
    # Sloped all round, on five large wheels with no rollers, the turret well forward.
    "T34": {"hull_long": 5.9, "upper_long": 5.0, "upper_lean": 0.74, "glacis": 1.55, "tail": 0.5, "wheels": 5, "wheel_r": 0.41, "wheel_first": -1.9,
            "wheel_step": 0.93, "rollers": (), "sprocket_x": -2.72, "idler_x": 2.6, "idler_z": 0.58, "sprocket_z": 0.6, "turret": "cast",
            "turret_long": 1.9, "turret_wide": 1.6, "turret_high": 0.55, "turret_x": 0.85, "bustle": False, "cupola": False,
            "gun_long": 2.5, "gun_calibre": 0.08, "brake": False, "track_wide": 0.52},
    "T34-85": {"hull_long": 5.9, "upper_long": 5.0, "upper_lean": 0.74, "glacis": 1.55, "tail": 0.5, "wheels": 5, "wheel_r": 0.41, "wheel_first": -1.9,
               "wheel_step": 0.93, "rollers": (), "sprocket_x": -2.72, "idler_x": 2.6, "idler_z": 0.58, "sprocket_z": 0.6, "turret": "cast",
               "turret_long": 2.5, "turret_wide": 1.95, "turret_high": 0.68, "turret_x": 0.65, "bustle": False, "cupola": True,
               "gun_long": 3.7, "gun_calibre": 0.09, "brake": False, "track_wide": 0.52},
    # Tall, on three bogies of two wheels, with a rounded cast turret.
    "Sherman": {"hull_long": 5.6, "hull_deck": 1.3, "upper_high": 0.72, "upper_lean": 0.9, "glacis": 1.25, "wheels": 6, "wheel_r": 0.26,
                "wheel_xs": (-1.85, -1.3, -0.3, 0.25, 1.25, 1.8), "rollers": (-1.55, 0.0, 1.55), "roller_z": 0.98, "sprocket_x": 2.5, "idler_x": -2.5,
                "sprocket_z": 0.72, "idler_z": 0.6, "turret": "round", "turret_long": 2.1, "turret_wide": 1.95, "turret_high": 0.72,
                "turret_taper": 0.8, "turret_x": 0.35, "bustle": True, "gun_long": 2.2, "gun_calibre": 0.095, "brake": False},
    # Low and heavy, its running gear behind side plates with mud chutes; a small turret and a slim gun.
    "Matilda": {"hull_long": 5.6, "hull_deck": 1.05, "upper_high": 0.5, "upper_lean": 0.97, "glacis": 0.8, "wheels": 10, "wheel_r": 0.15,
                "wheel_first": -1.95, "wheel_step": 0.43, "rollers": (-1.4, 0.0, 1.4), "roller_z": 0.9, "skirt": (1.02, 0.34, 5),
                "turret": "round", "turret_long": 1.7, "turret_wide": 1.6, "turret_high": 0.62, "turret_taper": 0.92, "turret_x": 0.3,
                "bustle": False, "gun_long": 1.7, "gun_calibre": 0.045, "brake": False},
    # A heavy tank: a great box of a hull on wide tracks and interleaved wheels, with a long heavy gun.
    "Tiger": {"scale": 1.12, "hull_long": 5.7, "upper_long": 4.9, "upper_wide": 2.9, "upper_lean": 1.0, "upper_high": 0.62, "glacis": 0.32,
              "tail": 0.05, "wheels": 8, "wheel_r": 0.36, "wheel_first": -1.95, "wheel_step": 0.56, "rollers": (), "interleaved": True,
              "track_wide": 0.62, "track_out": 1.36, "turret": "round", "turret_long": 2.7, "turret_wide": 2.2, "turret_high": 0.7,
              "turret_taper": 0.97, "turret_x": 0.05, "gun_long": 4.0, "gun_calibre": 0.105, "brake": True},
    # Long, with a well-sloped glacis, interleaved wheels and a long gun.
    "Panther": {"scale": 1.08, "hull_long": 6.0, "upper_long": 5.2, "upper_lean": 0.82, "glacis": 1.5, "tail": 0.3, "wheels": 8, "wheel_r": 0.37,
                "wheel_first": -2.1, "wheel_step": 0.6, "rollers": (), "interleaved": True, "track_wide": 0.56, "turret": "box",
                "turret_long": 2.5, "turret_wide": 2.0, "turret_high": 0.66, "turret_taper": 0.78, "turret_x": 0.0, "gun_long": 4.4,
                "gun_calibre": 0.085, "brake": True},
    # Tall and narrow, the tracks running right round the hull behind side plates; a gun in the hull and a small turret.
    "CharB1": {"hull_long": 6.2, "hull_deck": 1.5, "upper_high": 0.35, "upper_wide": 2.4, "upper_lean": 1.0, "glacis": 0.5, "wheels": 9,
               "wheel_r": 0.17, "wheel_first": -2.3, "wheel_step": 0.56, "rollers": (-1.8, -0.6, 0.6, 1.8), "roller_z": 1.42, "roller_r": 0.1,
               "sprocket_x": -2.85, "sprocket_z": 0.98, "sprocket_r": 0.5, "idler_x": 2.8, "idler_z": 0.98, "idler_r": 0.5,
               "skirt": (1.5, 0.3, 3), "turret": "round", "turret_long": 1.5, "turret_wide": 1.4, "turret_high": 0.6, "turret_taper": 0.86,
               "turret_x": 0.45, "bustle": False, "gun_long": 1.2, "gun_calibre": 0.055, "brake": False, "hull_gun": (0.085, 0.75, 0.55)},
    # A small riveted box on four bogies of two small wheels.
    "M13": {"scale": 0.86, "hull_long": 5.4, "upper_lean": 0.98, "glacis": 0.7, "wheels": 8, "wheel_r": 0.19,
            "wheel_xs": (-1.9, -1.45, -0.85, -0.4, 0.2, 0.65, 1.25, 1.7), "rollers": (-1.3, 0.0, 1.3), "roller_z": 0.88, "turret": "box",
            "turret_long": 1.8, "turret_wide": 1.6, "turret_high": 0.62, "turret_taper": 0.86, "turret_x": 0.3, "bustle": False,
            "gun_long": 1.6, "gun_calibre": 0.055, "brake": False},
    # Small, the turret set to one side, with a short gun.
    "Type97": {"scale": 0.88, "hull_long": 5.4, "upper_lean": 0.9, "glacis": 1.0, "wheels": 6, "wheel_r": 0.27, "wheel_first": -1.75,
               "wheel_step": 0.7, "rollers": (-1.2, 0.0, 1.2), "roller_z": 0.9, "sprocket_x": 2.45, "idler_x": -2.45, "turret": "cast",
               "turret_long": 1.8, "turret_wide": 1.6, "turret_high": 0.6, "turret_x": 0.25, "turret_y": 0.18, "bustle": False,
               "gun_long": 1.05, "gun_calibre": 0.075, "brake": False,
               # (the art director's sheet: khaki, with fields of brown (108, 78, 54) and green (86, 98, 62) parted by thin yellow lines (212, 180, 78))
               "camo": {"size": 1.15, "a": "6C4E36", "b": "56623E", "stripe": "D4B44E"}},
    # A light tank: small, on two pairs of bogies, with a small turret and a thin gun.
    "LightTank": {"scale": 0.78, "hull_long": 5.2, "upper_lean": 0.95, "glacis": 0.8, "wheels": 8, "wheel_r": 0.18,
                  "wheel_xs": (-1.75, -1.3, -0.75, -0.3, 0.3, 0.75, 1.3, 1.75), "rollers": (-1.5, -0.5, 0.5, 1.5), "roller_z": 0.86,
                  "sprocket_x": 2.4, "idler_x": -2.4, "turret": "box", "turret_long": 1.6, "turret_wide": 1.5, "turret_high": 0.62,
                  "turret_taper": 0.84, "turret_x": 0.2, "bustle": False, "gun_long": 1.5, "gun_calibre": 0.045, "brake": False},
}
VARIANTS["7TP"] = dict(VARIANTS["LightTank"])
# The medium tank of every army with none of its own (the art director's generic units brief, part 5): 5.7 m over
# the tracks, 2.7 wide, 2.5 to the turret's roof. Five middle-sized road wheels and three rollers a side, the drive
# in front; upright hull sides with a nose plate sloping back from 1.3 m to the deck at 1.86 m and a tail plate
# sloping down again; a six-sided turret amidships, its sides leaning in, with a flat hatch, a plain square mantlet
# and a short gun; one stowage box, a silencer across the tail, a lamp on each guard, the driver's flap in the nose.
VARIANTS["MediumTank"] = {"hull_long": 5.5, "hull_wide": 1.78, "hull_deck": 1.0, "upper_long": 5.3, "upper_wide": 2.7, "upper_high": 0.86,
                          "upper_lean": 1.0, "waist": 0.3, "glacis": 1.1, "tail": 0.55,
                          "wheels": 5, "wheel_r": 0.36, "wheel_first": -1.96, "wheel_step": 0.98, "rollers": (-1.3, 0.0, 1.3),
                          "roller_z": 0.98, "roller_r": 0.12, "sprocket_x": 2.55, "sprocket_z": 0.66, "sprocket_r": 0.30,
                          "idler_x": -2.55, "idler_z": 0.62, "idler_r": 0.30, "track_wide": 0.40, "track_out": 1.15,
                          "turret": "hex", "turret_long": 2.5, "turret_wide": 2.0, "turret_high": 0.6, "turret_taper": 0.72, "turret_x": 0.1,
                          "bustle": False, "cupola": False, "mantlet": "square", "gun_long": 1.8, "gun_calibre": 0.075, "brake": False,
                          "bow_gun": False, "spare_links": False, "silencer": True, "stowage": "box"}


def fresh_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make(name, bm, paint, bevel=0.0, segments=1):
    """Turns a bmesh into an object painted one paint, with its edges bevelled if asked."""
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj["paint"] = PAINT_SET[paint]
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


def wheel_places(s):
    """Where along the hull each road wheel stands."""
    if s.get("wheel_xs"):
        return list(s["wheel_xs"])
    return [s["wheel_first"] + k * s["wheel_step"] for k in range(s["wheels"])]


def track(name, side, s):
    """One track: a band of links run round the sprocket, the road wheels and the idler."""
    # The path of the track, seen from the side: along the ground, up round the idler at the front, back
    # along the top with a little sag between the rollers, and down round the sprocket at the rear.
    path = []
    ground = 0.045
    front, rear = (s["idler_x"], s["idler_z"], s["idler_r"]), (s["sprocket_x"], s["sprocket_z"], s["sprocket_r"])
    xs = wheel_places(s)
    first, last = min(xs), max(xs)
    front, rear = (front, rear) if front[0] > rear[0] else (rear, front)   # (whichever of the two wheels leads)
    for k in range(13):                                   # the ground run
        path.append((first - 0.1 + (last - first + 0.2) * k / 12.0, ground))
    for k in range(1, 9):                                 # up round the idler
        a = math.radians(-70.0 + 160.0 * k / 8.0)
        path.append((front[0] + front[2] * math.sin(a) * 1.0 + 0.0, front[1] - front[2] * math.cos(a)))
    rollers = s["rollers"]
    # (with no return rollers the top run lies on the road wheels' own tops)
    top = (s["roller_z"] + s["roller_r"] + 0.03) if rollers else (2.0 * s["wheel_r"] + 0.09)
    for k in range(1, 12):                                # the top run, sagging between the rollers
        x = front[0] - 0.12 - (front[0] - rear[0] - 0.24) * k / 12.0
        sag = 0.035 * abs(math.sin(math.pi * (x - rollers[0]) / (rollers[1] - rollers[0]))) if len(rollers) > 1 else 0.0
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
    k_long = s["hull_long"] / 5.5        # (what is placed along the hull is placed by the hull's own length)
    half = s["hull_long"] * 0.5

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
            p.y *= s["upper_lean"]
            if p.x > 0.0:
                p.x -= s["glacis"] / s["upper_long"]     # the glacis
            else:
                p.x += s["tail"] / s["upper_long"]       # the engine deck's tail
        return p
    upper_x = -0.12
    waist = s["waist"]
    if waist > 0.0:
        parts.append(box("waist", (s["upper_long"], s["upper_wide"], waist), (upper_x, 0.0, deck + waist * 0.5), "paint", b))
    parts.append(box("upper", (s["upper_long"], s["upper_wide"], s["upper_high"] - waist), (upper_x, 0.0, deck + waist + (s["upper_high"] - waist) * 0.5), "paint", b, shape=upper))
    # The nose between the glacis and the lower plate.
    parts.append(box("nose", (0.62, s["hull_wide"] + 0.02, 0.34), (s["upper_long"] * 0.5 + upper_x + 0.16, 0.0, deck - 0.02), "paint", b,
                     shape=lambda p: Vector((p.x - (0.35 if p.z > 0.0 else 0.0), p.y, p.z))))

    # ---- The track guards, with a flap turned down at each end.
    for side in (-1.0, 1.0):
        y = side * s["track_out"]
        parts.append(box("guard", (s["hull_long"] - 0.15, s["track_wide"] + 0.14, 0.04), (-0.05, y, deck + 0.05), "paint", 0.012))
        parts.append(box("flap_front", (0.42, s["track_wide"] + 0.14, 0.035), (half + 0.03, y, deck - 0.06), "paint", 0.0, turn=(0.0, 32.0, 0.0)))
        parts.append(box("flap_rear", (0.34, s["track_wide"] + 0.14, 0.035), (-half - 0.11, y, deck - 0.04), "paint", 0.0, turn=(0.0, -28.0, 0.0)))
        if s["skirt"]:
            # Side plates over the running gear, with mud chutes cut in them as dark slots.
            top, bottom, chutes = s["skirt"]
            parts.append(box("skirt", (s["hull_long"] - 0.5, 0.05, top - bottom), (0.0, y + side * (s["track_wide"] * 0.5 + 0.04), (top + bottom) * 0.5), "paint", 0.012))
            for c in range(chutes):
                cx = -half * 0.62 + (s["hull_long"] * 0.62) * c / max(chutes - 1, 1)
                parts.append(box("chute", (0.42, 0.06, (top - bottom) * 0.34), (cx, y + side * (s["track_wide"] * 0.5 + 0.045), bottom + (top - bottom) * 0.3), "shade"))

    # ---- The running gear.
    for side in (-1.0, 1.0):
        y = side * s["track_out"]
        parts.append(track("track", side, s))
        places = wheel_places(s)
        if s["interleaved"]:
            # (a second row set between the first and inboard of it, as wide wheels overlapped on the heavy tanks)
            for x in [(places[i] + places[i + 1]) * 0.5 for i in range(len(places) - 1)]:
                parts.append(drum("inner_wheel", s["wheel_r"], 0.2, (x, y - side * 0.2, s["wheel_r"] + 0.05), "shade", 12))
        for x in places:
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

    # ---- The turret: cast (an egg in plan, wider at its waist than at its roof), welded of plates, or a drum;
    # with a bustle behind where the tank has one.
    tx, ty, tz = s["turret_x"], s["turret_y"], up_top + 0.02
    tw, tl = s["turret_wide"] / 1.9, s["turret_long"] / 2.25      # (the turret's fittings are placed by its own size)
    bm = bmesh.new()
    if s["turret"] == "box":
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            roofed = v.co.z > 0.0
            v.co.y *= s["turret_wide"] * (s["turret_taper"] if roofed else 1.0) * (0.8 if v.co.x > 0.0 else 1.0)
            v.co.x *= s["turret_long"] * (0.9 if roofed else 1.0)
            v.co.z = (v.co.z + 0.5) * s["turret_high"]
    elif s["turret"] == "hex":
        # Six-sided in plan, flat across the front and the back, its sides leaning in to the roof.
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=6, radius1=1.0, radius2=s["turret_taper"], depth=1.0)
        bmesh.ops.rotate(bm, cent=(0.0, 0.0, 0.0), matrix=Matrix.Rotation(math.radians(30.0), 3, "Z"), verts=bm.verts)
        for v in bm.verts:
            v.co.x *= s["turret_long"] * 0.5 / math.cos(math.radians(30.0))
            v.co.y *= s["turret_wide"] * 0.5
            v.co.z = (v.co.z + 0.5) * s["turret_high"]
    else:
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=16, radius1=1.0, radius2=s["turret_taper"], depth=1.0)
        for v in bm.verts:
            # cast: blunt and narrower at the front, fuller toward the rear; a drum is the same all round
            front = max(v.co.x, 0.0) if s["turret"] == "cast" else 0.0
            v.co.y *= s["turret_wide"] * 0.5 * (1.0 - 0.18 * front)
            v.co.x *= s["turret_long"] * 0.5 * (0.86 if v.co.x > 0.0 and s["turret"] == "cast" else 1.0)
            v.co.z = (v.co.z + 0.5) * s["turret_high"]
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector((tx - 0.1, ty, tz))), verts=bm.verts)
    parts.append(make("turret", bm, "paint", b * 1.4, 2))
    if s["bustle"]:
        parts.append(box("bustle", (0.72 * tl, 1.24 * tw, 0.4), (tx - 0.1 - s["turret_long"] * 0.5 - 0.12, ty, tz + 0.3), "paint", b,
                         shape=lambda p: Vector((p.x, p.y * (0.86 if p.x < 0.0 else 1.0), p.z + (0.12 if p.x < 0.0 and p.z < 0.0 else 0.0)))))
    # The commander's cupola, with its hatch standing open a hand, and the loader's hatch flat on the roof.
    roof = tz + s["turret_high"]
    if s["cupola"]:
        parts.append(drum("cupola", 0.3 * tw, 0.2, (tx - 0.5 * tl, ty - 0.38 * tw, roof + 0.1), "paint", 12, axis="Z", radius2=0.27 * tw, bevel=0.012))
        parts.append(drum("cupola_hatch", 0.24 * tw, 0.035, (tx - 0.5 * tl, ty - 0.38 * tw, roof + 0.225), "shade", 12, axis="Z"))
        for k in range(5):
            a = math.radians(-70.0 + 35.0 * k)
            parts.append(box("periscope", (0.05, 0.11, 0.045), (tx - 0.5 * tl + 0.3 * tw * math.cos(a), ty - 0.38 * tw + 0.3 * tw * math.sin(a), roof + 0.12), "glass", 0.0,
                             turn=(0.0, 0.0, math.degrees(a))))
    else:
        parts.append(box("hatch", (0.5 * tl, 0.42 * tw, 0.035), (tx - 0.42 * tl, ty - 0.4 * tw, roof + 0.02), "shade", 0.008))
    parts.append(box("hatch", (0.5 * tl, 0.42 * tw, 0.035), (tx - 0.42 * tl, ty + 0.42 * tw, roof + 0.02), "shade", 0.008))
    parts.append(drum("vent", 0.13 * tw, 0.07, (tx + 0.38 * tl, ty, roof + 0.03), "paint", 10, axis="Z", radius2=0.09 * tw))
    back = tx - 0.1 - s["turret_long"] * 0.5 - (0.3 if s["bustle"] else -0.12)
    parts.append(drum("aerial_foot", 0.035, 0.1, (back, ty + 0.48 * tw, tz + 0.55), "shade", 6, axis="Z"))
    # (a whip as thick as a model-maker's wire, and dark: one true to scale is thinner than a point of the screen and glitters)
    parts.append(drum("aerial", 0.022, 1.1, (back, ty + 0.48 * tw, tz + 1.15), "shade", 5, axis="Z"))
    # Lifting eyes, and a rail on the bustle with a rolled tarpaulin lashed to it.
    for side in (-1.0, 1.0):
        parts.append(box("eye", (0.1, 0.03, 0.07), (tx + 0.25 * tl, ty + side * 0.72 * tw, roof - 0.04), "shade"))
    if s["bustle"]:
        parts.append(drum("tarpaulin", 0.13, 1.05 * tw, (back - 0.24, ty, tz + 0.34), "canvas", 8))
        for y in (-0.36 * tw, 0.36 * tw):
            parts.append(box("strap", (0.3, 0.035, 0.29), (back - 0.24, ty + y, tz + 0.34), "wood"))

    # ---- The gun: a mantlet, a sleeve, the barrel and its muzzle brake; and the machine-gun beside it.
    gx, gz = tx + s["turret_long"] * 0.5 * (0.86 if s["turret"] == "cast" else 0.92) - 0.16, tz + s["turret_high"] * 0.52
    if s["mantlet"] == "square":
        parts.append(box("mantlet", (0.3, 0.66 * tw, 0.5 * min(s["turret_high"] / 0.62, 1.2)), (gx + 0.06, ty, gz), "paint", b * 1.3))
    else:
        parts.append(box("mantlet", (0.36, 1.0 * tw, 0.56 * min(s["turret_high"] / 0.62, 1.2)), (gx + 0.06, ty, gz), "paint", b * 1.3,
                         shape=lambda p: Vector((p.x, p.y * (0.72 if p.x > 0.0 else 1.0), p.z * (0.74 if p.x > 0.0 else 1.0)))))
    cal = s["gun_calibre"]
    parts.append(drum("sleeve", cal * 1.75, 0.6, (gx + 0.5, ty, gz), "paint", 10, axis="X", radius2=cal * 1.35))
    parts.append(drum("barrel", cal, s["gun_long"], (gx + 0.24 + s["gun_long"] * 0.5, ty, gz), "paint", 10, axis="X", radius2=cal * 0.82))
    end = gx + 0.24 + s["gun_long"]
    if s["brake"]:
        parts.append(drum("brake", cal * 1.5, 0.3, (end - 0.1, ty, gz), "shade", 10, axis="X"))
    parts.append(drum("muzzle", cal * 0.9, 0.34, (end - 0.1, ty, gz), "steel", 8, axis="X"))
    parts.append(drum("coaxial", 0.022, 0.5, (gx + 0.36, ty + 0.3 * tw, gz + 0.02), "steel", 5, axis="X"))
    if s["hull_gun"]:
        # A gun in the hull's front, low beside the driver.
        hc, hl, hy = s["hull_gun"]
        hx = s["upper_long"] * 0.5 - 0.12 - s["glacis"] * 0.3
        parts.append(box("hull_mount", (0.5, 0.62, 0.5), (hx, hy, deck + 0.08), "paint", b))
        parts.append(drum("hull_gun", hc, hl, (hx + 0.25 + hl * 0.5, hy, deck + 0.1), "paint", 10, axis="X", radius2=hc * 0.85))
        parts.append(drum("hull_muzzle", hc * 0.8, 0.1, (hx + 0.25 + hl, hy, deck + 0.1), "steel", 8, axis="X"))

    # ---- On the glacis: the driver's visor, the bow gun in its ball, spare links, and the headlamps.
    slope_x, slope_z = s["upper_long"] * 0.5 + upper_x - s["glacis"] * 0.5, deck + waist + (s["upper_high"] - waist) * 0.5
    lean = math.degrees(math.atan2(s["upper_high"] - waist, s["glacis"]))
    parts.append(box("visor", (0.08, 0.5, 0.16), (slope_x + 0.05, -0.55, slope_z + 0.07), "shade", 0.008, turn=(0.0, -lean, 0.0)))
    parts.append(box("visor_slit", (0.085, 0.3, 0.035), (slope_x + 0.07, -0.55, slope_z + 0.085), "glass", 0.0, turn=(0.0, -lean, 0.0)))
    if s["bow_gun"]:
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=6, radius=0.15)
        bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector((slope_x + 0.02, 0.56, slope_z + 0.03))), verts=bm.verts)
        parts.append(make("ball", bm, "paint"))
        parts.append(drum("bow_gun", 0.02, 0.42, (slope_x + 0.26, 0.56, slope_z + 0.02), "steel", 5, axis="X"))
    for k in range(3 if s["spare_links"] else 0):
        # (spare track links carried across the nose, where they thicken the armour a little)
        parts.append(box("spare_link", (0.11, 0.42, 0.045), (slope_x + 0.34 - 0.15 * k, 0.0, slope_z - 0.13 + 0.079 * k), "track", 0.0,
                         turn=(0.0, -lean, 0.0)))
    for side in (-1.0, 1.0):
        parts.append(drum("lamp", 0.085, 0.11, (half - 0.25, side * 1.0, deck + 0.16), "paint", 8, axis="X"))
        parts.append(drum("lamp_glass", 0.07, 0.115, (half - 0.24, side * 1.0, deck + 0.16), "glass", 8, axis="X"))
        parts.append(box("lamp_foot", (0.04, 0.04, 0.1), (half - 0.25, side * 1.0, deck + 0.09), "shade"))

    # ---- The engine deck: two grilles with their louvres, and the exhausts across the tail.
    deck_x = -s["upper_long"] * 0.5 + upper_x + s["tail"] + 0.6      # the middle of the grilles
    for side in (-1.0, 1.0):
        parts.append(box("grille", (0.95, 0.72, 0.03), (deck_x, side * 0.52, up_top + 0.012), "shade", 0.006))
        for k in range(6):
            parts.append(box("louvre", (0.05, 0.66, 0.035), (deck_x - 0.4 + 0.16 * k, side * 0.52, up_top + 0.035), "paint", 0.0, turn=(0.0, 30.0, 0.0)))
        if not s["silencer"]:
            parts.append(drum("exhaust", 0.11, 0.78, (-half - 0.07, side * 0.52, deck + 0.22), "rust", 10))
            parts.append(drum("exhaust_pipe", 0.04, 0.3, (-half - 0.07, side * 0.98, deck + 0.3), "rust", 6, axis="Z"))
    if s["silencer"]:
        # One silencer across the tail, on two brackets, with its pipe turned down at one end.
        parts.append(drum("silencer", 0.13, 1.5, (-half - 0.09, 0.0, deck + 0.24), "rust", 12))
        for side in (-1.0, 1.0):
            parts.append(box("silencer_bracket", (0.12, 0.05, 0.2), (-half - 0.04, side * 0.55, deck + 0.2), "shade"))
        parts.append(drum("silencer_pipe", 0.04, 0.26, (-half - 0.09, 0.82, deck + 0.14), "rust", 6, axis="Z"))
    parts.append(box("tail_plate", (0.06, 1.9, 0.5), (-half + 0.03, 0.0, deck + 0.1), "paint", 0.012))
    if not s["stowage"]:
        return parts
    if s["stowage"] == "box":
        # One stowage box on the right rear guard, and nothing else.
        parts.append(box("stowage_box", (0.9, 0.36, 0.26), (-1.35 * k_long, -s["track_out"], deck + 0.2), "paint", 0.012))
        parts.append(box("stowage_lid", (0.92, 0.38, 0.02), (-1.35 * k_long, -s["track_out"], deck + 0.34), "shade"))
        return parts

    # ---- Stowed on the guards and the deck: a tool box, a shovel and a pick, jerrycans, the jack and its
    # block, and the tow cable coiled on the tail.
    parts.append(box("toolbox", (0.72, 0.36, 0.2), (0.55 * k_long, -s["track_out"], deck + 0.18), "paint", 0.012))
    parts.append(box("toolbox_lid", (0.74, 0.38, 0.02), (0.55 * k_long, -s["track_out"], deck + 0.29), "shade"))
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
            ring.append(bm.verts.new((deck_x - 0.75 + r * math.cos(a) * 0.62, r * math.sin(a), up_top + 0.045 + 0.03 * math.sin(c))))
        cable.append(ring)
    for k in range(20):
        a_ring, b_ring = cable[k], cable[(k + 1) % 20]
        for e in range(5):
            bm.faces.new((a_ring[e], a_ring[(e + 1) % 5], b_ring[(e + 1) % 5], b_ring[e]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    parts.append(make("tow_cable", bm, "steel"))
    return parts


def camo_field(p):
    """Where a point of a camouflaged vehicle lies among the fields of its coat: above 0.18 it is in a field of the second
    colour, below -0.18 in one of the third, and between them in the vehicle's own. The lines that part the fields run
    where it is exactly one or the other."""
    from mathutils import noise
    return noise.noise(Vector((p.x / CAMO + 3.1, p.y / CAMO + 7.7, p.z / CAMO * 1.6 + 1.3)))


def gather(parts, simple=False):
    """Every part's finished mesh (bevels and all) as flat lists the game can read."""
    points, facings, paints, triangles = [], [], [], []
    camo_a, camo_b = [], []
    seen = {}
    graph = bpy.context.evaluated_depsgraph_get()
    crease = math.cos(math.radians(40.0))
    for obj in parts:
        if simple:
            for mod in obj.modifiers:
                mod.show_viewport = False                 # the far mesh has no bevels
            if FAR_RATIO < 1.0 and len(obj.data.polygons) > 6:
                thin = obj.modifiers.new("thin", "DECIMATE")
                thin.ratio = FAR_RATIO
            # (and the smallest things are left off it: they cannot be seen from there)
            if obj.name.split(".")[0] in ("periscope", "eye", "strap", "louvre", "lamp_foot", "can_cap", "coaxial", "bow_gun", "aerial",
                                          "aerial_foot", "hub", "idler_hub", "arm", "visor_slit", "lamp_glass", "pick_head", "pick_shaft",
                                          "shovel_shaft", "shovel_blade", "exhaust_pipe", "tow_cable", "spare_link", "toolbox_lid", "chute", "inner_wheel",
                                          "hull_muzzle", "slat", "door_line", "stake", "hoop", "bench", "spoke", "sight", "round", "handspike", "mg", "nave", "rim", "buckle", "chin_strap", "brace", "bolt", "sling", "pack_flap",
                                          "collar", "knee", "bread_bag", "bottle", "nose", "elbow", "ear", "rein", "cheek_strap", "stirrup",
                                          "stirrup_strap", "girth", "forelock", "hock", "grip"):
                continue
            graph.update()
        bm = bmesh.new()
        bm.from_object(obj, graph)
        bmesh.ops.transform(bm, matrix=obj.matrix_world, verts=bm.verts)
        bmesh.ops.triangulate(bm, faces=bm.faces)
        paint = int(obj["paint"])
        # (The art director's sheet for the Pacific: a Japanese tank is khaki with fields of brown and of green, parted
        # by thin yellow lines. The painted plates are cut into small triangles, each point says how far it is into a
        # brown field and into a green one, and the game mixes the three; the yellow lines are thin ribbons laid along
        # the very edge of each field, triangle by triangle, so they are sharp however coarse the plates are.)
        # (Only the big plates carry the fields: hull, turret and guards. A wheel or a hatch cut that fine made the
        # tank twelve times as heavy as its fellows for nothing the eye could find.)
        fields = CAMO is not None and paint == PAINT_SET.get("paint", -1) and PAINT_SET is PAINTS and max(obj.dimensions) > 0.9
        if fields:
            limit = 0.7 if simple else 0.4
            for _ in range(8):
                # (the thin faces of a bevelled edge are left whole: only the flats of the plates are cut)
                long = [e for e in bm.edges if e.calc_length() > limit and all(f.calc_area() > 0.02 for f in e.link_faces)]
                if not long:
                    break
                bmesh.ops.subdivide_edges(bm, edges=long, cuts=1)
                bmesh.ops.triangulate(bm, faces=bm.faces)
        bm.normal_update()
        ribbons = []
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
                    if fields:
                        f = camo_field(p)
                        step = lambda t: 0.0 if t <= 0.0 else 1.0 if t >= 1.0 else t * t * (3.0 - 2.0 * t)
                        camo_a.append(int(round(100.0 * step((f - 0.15) / 0.06))))
                        camo_b.append(int(round(100.0 * step((-0.15 - f) / 0.06))))
                    else:
                        camo_a.append(0)
                        camo_b.append(0)
                corners.append(index)
            triangles.extend(corners)
            if fields and not simple and face.normal.z > -0.5 and face.calc_area() > 0.004:
                # The line along the edge of a field, where it crosses this triangle.
                at = [loop.vert.co.copy() for loop in face.loops]
                value = [camo_field(q) for q in at]
                for edge_of in (0.18, -0.18):
                    cuts = []
                    for a, b in ((0, 1), (1, 2), (2, 0)):
                        if (value[a] - edge_of) * (value[b] - edge_of) < 0.0:
                            cuts.append(at[a].lerp(at[b], (edge_of - value[a]) / (value[b] - value[a])))
                    if len(cuts) == 2 and (cuts[1] - cuts[0]).length > 1.0e-4:
                        ribbons.append((cuts[0], cuts[1], face.normal.copy()))
        for a, b, n in ribbons:
            side = n.cross(b - a).normalized() * 0.016
            along = (b - a).normalized() * 0.006              # (run on a little at each end, so the pieces of a line meet)
            first = len(points) // 3
            for q in (a - along + side, a - along - side, b + along - side, b + along + side):
                q = q + n * 0.004
                points.extend((round(q.x, 4), round(-q.y, 4), round(q.z, 4)))
                facings.extend((round(n.x, 3), round(-n.y, 3), round(n.z, 3)))
                paints.append(PAINTS["stripe"])
                camo_a.append(0)
                camo_b.append(0)
            triangles.extend((first, first + 1, first + 2, first, first + 2, first + 3))
        bm.free()
    out = {"points": points, "facings": facings, "paints": paints, "triangles": triangles}
    if CAMO is not None:
        out["camo_a"], out["camo_b"] = camo_a, camo_b
    return out


# ---- Lorries, half-tracks and guns ---------------------------------------------------------------

def tyre_wheel(parts, x, y, r, wide, hub="paint", sides=14):
    """A road wheel on a rubber tyre, with its hub."""
    parts.append(drum("tyre", r, wide, (x, y, r), "rubber", sides, bevel=0.02))
    parts.append(drum("rim", r * 0.6, wide + 0.02, (x, y, r), hub, 10))
    parts.append(drum("hub", r * 0.2, wide + 0.08, (x, y, r), "shade", 8))


def hooped_tilt(parts, x0, x1, half, foot, top):
    """A canvas tilt over hoops: flat sides rounded over at the top."""
    bm = bmesh.new()
    rings = []
    steps = 9
    for x in (x0, x1):
        ring = []
        for k in range(steps + 1):
            a = math.pi * k / steps                        # from one eave over the top to the other
            y = -math.cos(a) * half
            z = foot + (top - foot) * (0.55 + 0.45 * math.sin(a)) if 0 < k < steps else foot + (top - foot) * 0.55
            ring.append(bm.verts.new((x, y, z)))
        ring.insert(0, bm.verts.new((x, -half, foot)))
        ring.append(bm.verts.new((x, half, foot)))
        rings.append(ring)
    count = len(rings[0])
    for k in range(count - 1):
        bm.faces.new((rings[0][k], rings[0][k + 1], rings[1][k + 1], rings[1][k]))
    bm.faces.new(rings[0])
    bm.faces.new(list(reversed(rings[1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    parts.append(make("tilt", bm, "canvas", 0.02))
    for k in range(4):                                     # the hoops showing through as ridges
        x = x0 + (x1 - x0) * (k + 0.5) / 4.0
        parts.append(box("hoop", (0.05, half * 2.0 + 0.03, 0.03), (x, 0.0, top + 0.005), "canvas"))


def build_truck(s):
    """A lorry of about three tons: bonnet, cab, a planked load bed under a canvas tilt."""
    parts = []
    b = 0.02
    front, rear, r = s["front_x"], s["rear_x"], s["wheel_r"]
    for side in (-1.0, 1.0):
        tyre_wheel(parts, front, side * 0.86, r, 0.22)
        for x in s["rear_axles"]:
            tyre_wheel(parts, x, side * 0.8, r, 0.42 if s["twin"] else 0.24)
        # mudguards over the front wheels, with a lamp on each; the running board under the door
        parts.append(box("guard", (0.95, 0.32, 0.05), (front, side * 0.86, r * 2.0 + 0.1), "shade", 0.015,
                         shape=lambda p: Vector((p.x, p.y, p.z - (0.9 if abs(p.x) > 0.4 else 0.0)))))
        parts.append(drum("lamp", 0.09, 0.12, (front + 0.5, side * 0.72, 1.1), "paint", 8, axis="X"))
        parts.append(drum("lamp_glass", 0.075, 0.125, (front + 0.51, side * 0.72, 1.1), "glass", 8, axis="X"))
        parts.append(box("board", (1.0, 0.24, 0.04), (0.8, side * 1.0, 0.62), "shade", 0.01))
        parts.append(box("window", (0.6, 0.02, 0.36), (0.72, side * 0.96, 1.85), "glass"))
        parts.append(box("door_line", (0.02, 0.02, 0.9), (1.2, side * 0.955, 1.25), "shade"))
    # the frame, the axles and the bumper
    parts.append(box("frame", (rear - 1.2 - (front + 0.9), 0.8, 0.14), ((rear - 1.2 + front + 0.9) * 0.5, 0.0, 0.62), "shade"))
    parts.append(drum("axle", 0.05, 1.6, (front, 0.0, r), "shade", 6))
    for x in s["rear_axles"]:
        parts.append(drum("axle", 0.07, 1.4, (x, 0.0, r), "shade", 6))
    parts.append(box("bumper", (0.08, 1.9, 0.14), (front + 1.0, 0.0, 0.66), "shade", 0.02))
    # the bonnet with its radiator, and the cab with its windscreen
    parts.append(box("bonnet", (1.55, 1.0, 0.82), (front + 0.12, 0.0, 1.16), "paint", b * 2.0,
                     shape=lambda p: Vector((p.x, p.y * (0.86 if p.x > 0.0 else 1.0), p.z - (0.1 if p.x > 0.0 and p.z > 0.0 else 0.0)))))
    parts.append(box("radiator", (0.04, 0.7, 0.6), (front + 0.91, 0.0, 1.12), "shade"))
    for k in range(6):
        parts.append(box("slat", (0.03, 0.04, 0.56), (front + 0.925, -0.3 + k * 0.12, 1.12), "paint"))
    parts.append(box("cab", (1.1, 1.9, 1.42), (0.75, 0.0, 1.47), "paint", b * 2.0,
                     shape=lambda p: Vector((p.x - (0.16 if p.x > 0.0 and p.z > 0.0 else 0.0), p.y, p.z))))
    parts.append(box("windscreen", (0.03, 1.6, 0.44), (1.2, 0.0, 1.9), "glass", 0.0, turn=(0.0, -18.0, 0.0)))
    parts.append(box("cab_roof", (1.0, 1.94, 0.05), (0.7, 0.0, 2.2), "canvas" if s["soft_cab"] else "shade", 0.015))
    # the load bed with its planked sides, and the tilt
    bed0, bed1 = 0.1, rear - 1.35
    parts.append(box("bed", (bed0 - bed1, 2.16, 0.12), ((bed0 + bed1) * 0.5, 0.0, 0.96), "wood", 0.01))
    for side in (-1.0, 1.0):
        parts.append(box("bed_side", (bed0 - bed1, 0.05, 0.56), ((bed0 + bed1) * 0.5, side * 1.08, 1.26), "paint", 0.01))
        for k in range(5):
            parts.append(box("stake", (0.07, 0.03, 0.58), (bed0 - 0.3 - k * (bed0 - bed1 - 0.5) / 4.0, side * 1.11, 1.26), "shade"))
    parts.append(box("tailboard", (0.05, 2.16, 0.56), (bed1, 0.0, 1.26), "paint", 0.01))
    if s["tilt"]:
        hooped_tilt(parts, bed1 + 0.03, bed0 - 0.03, 1.08, 1.5, 2.55)
    # a spare wheel behind the cab, and a jerrycan
    parts.append(drum("spare", r * 0.95, 0.2, (0.02, 0.0, 1.75), "rubber", 12, axis="X"))
    parts.append(box("jerrycan", (0.33, 0.17, 0.44), (0.9, -1.04, 0.86), "shade", 0.02))
    return parts


def build_halftrack(s):
    """An armoured half-track: steered wheels in front, a short track behind, an open fighting compartment."""
    parts = []
    b = 0.025
    r = s["wheel_r"]
    for side in (-1.0, 1.0):
        tyre_wheel(parts, s["front_x"], side * 0.9, r, 0.24, hub="shade")
        parts.append(box("front_guard", (1.0, 0.34, 0.05), (s["front_x"], side * 0.9, r * 2.0 + 0.1), "paint", 0.015,
                         shape=lambda p: Vector((p.x, p.y, p.z - (1.2 if p.x > 0.35 else 0.0)))))
        parts.append(drum("lamp", 0.08, 0.11, (s["front_x"] + 0.45, side * 0.62, 1.12), "paint", 8, axis="X"))
        parts.append(drum("lamp_glass", 0.065, 0.115, (s["front_x"] + 0.46, side * 0.62, 1.12), "glass", 8, axis="X"))
        # the track unit: a band round a sprocket, road wheels and an idler
        unit = dict(SPEC)
        unit.update({"wheels": s["track_wheels"], "wheel_r": s["track_wheel_r"], "wheel_first": s["track_first"], "wheel_step": s["track_step"],
                     "rollers": (), "sprocket_x": s["track_first"] - 0.55 if not s["drive_front"] else s["track_first"] + s["track_step"] * (s["track_wheels"] - 1) + 0.55,
                     "idler_x": s["track_first"] + s["track_step"] * (s["track_wheels"] - 1) + 0.55 if not s["drive_front"] else s["track_first"] - 0.55,
                     "sprocket_z": s["track_wheel_r"] + 0.12, "idler_z": s["track_wheel_r"] + 0.08, "sprocket_r": s["track_wheel_r"] * 0.95,
                     "idler_r": s["track_wheel_r"] * 0.9, "track_wide": 0.3, "track_out": 0.92})
        parts.append(track("track", side, unit))
        places = wheel_places(unit)
        if s["interleaved"]:
            for x in [(places[i] + places[i + 1]) * 0.5 for i in range(len(places) - 1)]:
                parts.append(drum("inner_wheel", unit["wheel_r"], 0.14, (x, side * 0.8, unit["wheel_r"] + 0.05), "shade", 12))
        for x in places:
            parts.append(drum("tyre", unit["wheel_r"], 0.2, (x, side * 0.92, unit["wheel_r"] + 0.05), "rubber", 12))
            parts.append(drum("wheel", unit["wheel_r"] * 0.7, 0.22, (x, side * 0.92, unit["wheel_r"] + 0.05), "paint", 10))
        parts.append(drum("sprocket", unit["sprocket_r"], 0.24, (unit["sprocket_x"], side * 0.92, unit["sprocket_z"]), "shade", 12))
        parts.append(drum("idler", unit["idler_r"], 0.22, (unit["idler_x"], side * 0.92, unit["idler_z"]), "shade", 12))
        parts.append(box("track_guard", (s["body_long"] * 0.62, 0.36, 0.04), (s["body_x"] - s["body_long"] * 0.16, side * 0.94, 1.0), "paint", 0.012))
    # the armoured body: a bonnet, and a compartment whose sides lean (out below the fold, in above it) or stand upright
    lean = s["lean"]
    parts.append(box("bonnet", (1.5, 1.3, 0.62), (s["front_x"] + 0.1, 0.0, 1.16), "paint", b,
                     shape=lambda p: Vector((p.x, p.y * (0.72 if p.x > 0.0 else 1.0) * (lean if p.z > 0.0 else 1.0), p.z - (0.12 if p.x > 0.0 and p.z > 0.0 else 0.0)))))
    parts.append(box("radiator", (0.04, 0.7, 0.4), (s["front_x"] + 0.86, 0.0, 1.1), "shade"))
    long, half = s["body_long"], s["body_wide"] * 0.5
    parts.append(box("lower", (long, s["body_wide"], 0.5), (s["body_x"], 0.0, 1.1), "paint", b,
                     shape=lambda p: Vector((p.x, p.y * (1.0 if p.z > 0.0 else 2.0 - lean), p.z))))
    parts.append(box("upper", (long, s["body_wide"], 0.6), (s["body_x"], 0.0, 1.65), "paint", b,
                     shape=lambda p: Vector((p.x - (0.25 / long if p.x > 0.0 and p.z > 0.0 else 0.0), p.y * (lean if p.z > 0.0 else 1.0), p.z))))
    parts.append(box("well", (long * 0.62, s["body_wide"] * lean - 0.16, 0.05), (s["body_x"] - long * 0.16, 0.0, 1.93), "shade"))
    parts.append(box("visor", (0.04, 0.5, 0.16), (s["body_x"] + long * 0.5 - 0.12, -0.38, 1.7), "glass"))
    parts.append(box("visor", (0.04, 0.5, 0.16), (s["body_x"] + long * 0.5 - 0.12, 0.38, 1.7), "glass"))
    for k in range(3):                                      # the benches' backs seen in the open well
        parts.append(box("bench", (long * 0.5, 0.06, 0.2), (s["body_x"] - long * 0.18, (k - 1) * 0.5, 1.86), "wood" if k != 1 else "shade"))
    # a machine-gun on its shield at the front of the well, and one at the rear
    parts.append(box("gun_shield", (0.04, 0.5, 0.3), (s["body_x"] + long * 0.2, 0.0, 2.12), "paint", 0.008))
    parts.append(drum("mg", 0.022, 0.7, (s["body_x"] + long * 0.2 + 0.3, 0.0, 2.1), "steel", 5, axis="X"))
    parts.append(box("rear_door", (0.04, 0.9, 0.9), (s["body_x"] - long * 0.5 - 0.01, 0.0, 1.45), "shade", 0.008))
    if s["roller"]:
        parts.append(drum("roller", 0.16, 1.1, (s["front_x"] + 1.05, 0.0, 0.62), "shade", 10))
    for k in range(3):
        parts.append(box("jerrycan", (0.17, 0.33, 0.44), (s["body_x"] - 0.3 - 0.2 * k, half + 0.06, 1.25), "shade" if k == 1 else "paint", 0.02))
    parts.append(drum("aerial", 0.02, 1.0, (s["body_x"] + long * 0.3, -half * lean + 0.08, 2.45), "shade", 5, axis="Z"))
    return parts


def build_gun(s):
    """A field gun on its carriage: wheels, a shield, the barrel in its cradle, and the trail behind."""
    parts = []
    r, half = s["wheel_r"], s["track"]
    for side in (-1.0, 1.0):
        if s["spoked"]:
            parts.append(drum("felloe", r, 0.1, (0.0, side * half, r), "wood", 14))
            parts.append(drum("iron_tyre", r + 0.012, 0.08, (0.0, side * half, r), "steel", 14))
            parts.append(drum("nave", r * 0.2, 0.2, (0.0, side * half, r), "shade", 8))
            for k in range(6):
                a = math.pi * k / 6.0
                parts.append(box("spoke", (r * 1.8, 0.03, 0.05), (0.0, side * (half + 0.06), r), "shade", 0.0, turn=(0.0, math.degrees(a), 0.0)))
        else:
            tyre_wheel(parts, 0.0, side * half, r, 0.22)
    parts.append(drum("axle", 0.06, half * 2.0, (0.0, 0.0, r), "shade", 6))
    # the saddle, and the cradle with the barrel raised a little
    high, up = s["high"], math.radians(s["raised"])
    parts.append(box("saddle", (0.5, 0.5, high - r), (0.0, 0.0, (high + r) * 0.5), "paint", 0.02))
    def along(d, lift=0.0):
        return (d * math.cos(up) - lift * math.sin(up), 0.0, high + d * math.sin(up) + lift * math.cos(up))
    bore = s["bore"]
    lean = -s["raised"]
    def tube(name, radius, length, mid, paint, sides=10, lift=0.0, radius2=None):
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=sides, radius1=radius, radius2=radius if radius2 is None else radius2, depth=length)
        place = Matrix.Translation(Vector(along(mid, lift))) @ Matrix.Rotation(math.radians(90.0) - up, 4, "Y")
        bmesh.ops.transform(bm, matrix=place, verts=bm.verts)
        parts.append(make(name, bm, paint))
    tube("barrel", bore, s["ahead"] + s["behind"], (s["ahead"] - s["behind"]) * 0.5, "paint", 10, radius2=bore * 1.25)
    tube("breech", bore * 2.1, 0.34, -s["behind"] + 0.05, "shade", 8)
    tube("recoil", bore * 1.5, s["recoil"], s["recoil"] * 0.5 - 0.2, "paint", 8, lift=bore * 2.4 * (1.0 if s["above"] else -1.0))
    tube("cradle", bore * 1.3, s["recoil"] * 0.8, s["recoil"] * 0.4 - 0.15, "shade", 6, lift=-bore * 1.3 if s["above"] else bore * 1.3)
    if s["brake"]:
        tube("brake", bore * 1.7, 0.3, s["ahead"] - 0.12, "shade", 8)
    tube("muzzle", bore * 0.85, 0.06, s["ahead"] + 0.01, "steel", 8)
    # the shield: two plates meeting at a fold, the upper leaning back, with a port for the barrel
    foot, top, wide = s["shield_foot"], s["shield_top"], s["shield"]
    for side in (-1.0, 1.0):
        parts.append(box("shield", (0.03, wide - bore * 1.6, top - foot), (0.32, side * (wide + bore * 1.6) * 0.5, (top + foot) * 0.5), "paint", 0.008,
                         shape=lambda p: Vector((p.x - (6.0 if p.z > 0.0 else 0.0), p.y, p.z))))
    parts.append(box("shield_top", (0.03, bore * 3.2, (top - high) * 0.6), (0.2, 0.0, top - (top - high) * 0.3), "paint", 0.006))
    # the trail: two legs spread behind, or one box trail; a spade at each end
    legs = (-1.0, 1.0) if s["split"] else (0.0,)
    for leg in legs:
        spread = math.radians(16.0) * leg
        length = s["trail"]
        mid = Vector((-length * 0.5 * math.cos(spread), -length * 0.5 * math.sin(spread) + leg * 0.18, r * 0.62))
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co = Vector((v.co.x * length, v.co.y * (0.16 if s["split"] else 0.34), v.co.z * 0.16))
        place = Matrix.Translation(mid) @ Matrix.Rotation(spread, 4, "Z") @ Matrix.Rotation(math.radians(-9.0), 4, "Y")
        bmesh.ops.transform(bm, matrix=place, verts=bm.verts)
        parts.append(make("trail", bm, "paint", 0.015))
        end = Vector((-length * math.cos(spread), -length * math.sin(spread) + leg * 0.18, 0.12))
        parts.append(box("spade", (0.06, 0.42 if s["split"] else 0.5, 0.3), (end.x, end.y, 0.15), "shade", 0.008))
        parts.append(drum("handspike", 0.02, 0.5, (end.x + 0.25, end.y, 0.42), "wood", 5, axis="X"))
    if s["platform"]:
        # a round firing platform carried under the trail and laid under the wheels
        parts.append(drum("platform", half + 0.35, 0.06, (0.0, 0.0, 0.03), "shade", 20, axis="Z"))
    # the layer's seat, the sight, and a box of rounds beside the trail
    parts.append(box("seat", (0.26, 0.26, 0.04), (-0.45, -half * 0.7, r + 0.25), "shade", 0.008))
    parts.append(box("sight", (0.06, 0.06, 0.2), (0.1, -bore * 3.0, high + 0.22), "steel"))
    parts.append(box("round_box", (0.5, 0.3, 0.24), (-1.0, half + 0.15, 0.12), "wood", 0.01))
    for k in range(3):
        parts.append(drum("round", bore * 0.95, 0.5, (-1.0, half - 0.25 - k * (bore * 2.2), bore), "steel", 8, axis="X", radius2=bore * 0.7))
    return parts


TRUCK = {"kind": "truck", "scale": 1.0, "front_x": 1.95, "rear_x": -1.55, "rear_axles": (-1.55,), "wheel_r": 0.43, "twin": True, "tilt": True, "soft_cab": False,
         "cupola": False, "turret_high": 0.0}
HALFTRACK = {"kind": "halftrack", "scale": 1.0, "front_x": 2.1, "wheel_r": 0.42, "track_wheels": 6, "track_wheel_r": 0.3, "track_first": -2.0, "track_step": 0.52,
             "interleaved": True, "drive_front": True, "body_x": -0.75, "body_long": 4.0, "body_wide": 1.95, "lean": 0.8, "roller": False}
GUN = {"kind": "gun", "scale": 1.0, "wheel_r": 0.55, "track": 0.85, "spoked": False, "ahead": 2.2, "behind": 0.65, "bore": 0.075, "high": 1.1, "raised": 9.0,
       "brake": False, "above": False, "recoil": 1.3, "shield": 0.85, "shield_foot": 0.5, "shield_top": 1.6, "split": True, "trail": 2.8, "platform": False}

OTHERS = {
    # Lorries, half-tracks and guns, under the names the game knows them by.
    "Truck": dict(TRUCK),
    # An armoured half-track with leaning plates and interleaved wheels; and a boxy one with a roller at its nose.
    "Hanomag": dict(HALFTRACK),
    "M3Halftrack": dict(HALFTRACK, track_wheels=4, track_wheel_r=0.2, track_first=-1.75, track_step=0.42, interleaved=False, drive_front=True,
                        lean=1.0, body_long=3.9, roller=True),
    # A light field howitzer on a split trail; a gun-howitzer with a muzzle brake on a box trail and a firing platform;
    # a short howitzer on a split trail; a long divisional gun; and an old field gun on spoked wheels.
    "leFH18": dict(GUN, bore=0.085, ahead=2.0, recoil=1.5, above=False, shield=0.95),
    "25pdr": dict(GUN, bore=0.07, ahead=1.9, brake=True, split=False, trail=2.6, platform=True, shield=1.0, shield_top=1.55, above=False, wheel_r=0.46),
    "Howitzer105": dict(GUN, bore=0.085, ahead=1.75, above=True, recoil=1.2, shield=0.75, shield_top=1.45, wheel_r=0.5, track=0.95),
    "ZiS3": dict(GUN, bore=0.06, ahead=2.9, brake=True, above=False, recoil=1.5, shield=0.8, shield_foot=0.42, shield_top=1.4, high=0.98, wheel_r=0.46),
    "Gun75": dict(GUN, bore=0.06, ahead=2.2, spoked=True, split=False, trail=2.5, wheel_r=0.66, shield=0.75, shield_top=1.5, high=1.15, above=False),
}


# ---- The infantry figure --------------------------------------------------------------------------

# (The art director's ruling on the figures: they read as wooden lay figures, so arms and legs are a third thicker
# than they were first made, and a horse's legs half as thick again.)
THICKER = {"thigh": 1.33, "shin": 1.33, "knee": 1.33, "upper_arm": 1.33, "forearm": 1.33, "elbow": 1.33, "hand": 1.15,
           "gaskin": 1.5, "cannon": 1.5, "hock": 1.5, "hoof": 1.3}


def limb(name, a, b, ra, rb, paint, sides=8):
    """A tapered round limb from one point to another."""
    ra, rb = ra * THICKER.get(name, 1.0), rb * THICKER.get(name, 1.0)
    a, b = Vector(a), Vector(b)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=sides, radius1=ra, radius2=rb, depth=(b - a).length)
    turn = Vector((0.0, 0.0, 1.0)).rotation_difference((b - a).normalized()).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((a + b) * 0.5) @ turn, verts=bm.verts)
    return make(name, bm, paint)


def ball(name, at, size, paint, flat_below=None, u=10, v=6):
    """A ball, or an egg ('size' may be three radii); 'flat_below' cuts it level at that height above its middle."""
    radii = size if isinstance(size, tuple) else (size, size, size)
    radii = tuple(r * THICKER.get(name, 1.0) for r in radii)
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=1.0)
    for vert in bm.verts:
        z = vert.co.z if flat_below is None else max(vert.co.z, flat_below)
        vert.co = Vector((vert.co.x * radii[0], vert.co.y * radii[1], z * radii[2]))
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(at)), verts=bm.verts)
    return make(name, bm, paint)


def oval(name, at, rx, ry, high, paint, top=1.0, sides=12, lean=0.0):
    """A round body part standing on 'at': an oval in plan, 'top' times as large at its top as at its foot, leaning forward by 'lean'."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=sides, radius1=1.0, radius2=top, depth=1.0)
    for vert in bm.verts:
        up = vert.co.z + 0.5
        vert.co = Vector((vert.co.x * rx + lean * up, vert.co.y * ry, up * high))
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(at)), verts=bm.verts)
    return make(name, bm, paint, 0.012)


def build_figure(k):
    """A rifleman stepping forward with his rifle at the port, held across his body in both hands. About 1.75
    metres with his helmet."""
    parts = []
    pose = k.get("pose", "rifle")
    knee, hip, belt, shoulder = 0.5, 0.9, 0.99, 1.43
    if pose == "rider":
        # astride: the thighs forward and out round the horse's barrel, the shins hanging to the stirrups
        for side in (-1.0, 1.0):
            at_knee, at_foot = (0.3, side * 0.31, 0.66), (0.2, side * 0.35, 0.26)
            parts.append(limb("thigh", (0.0, side * 0.1, hip), at_knee, 0.09, 0.066, "trousers"))
            parts.append(ball("knee", at_knee, 0.066, "trousers", u=8, v=5))
            parts.append(limb("shin", at_knee, at_foot, 0.064, 0.05, "boots" if k["tall_boots"] else "lower"))
            parts.append(box("boot", (0.27, 0.105, 0.1), (at_foot[0] + 0.06, at_foot[1], at_foot[2] - 0.04), "boots", 0.03))
            parts.append(box("stirrup", (0.12, 0.13, 0.02), (at_foot[0] + 0.06, at_foot[1], at_foot[2] - 0.1), "steel"))
            parts.append(limb("stirrup_strap", (0.05, side * 0.3, 0.84), (at_foot[0] + 0.05, at_foot[1], at_foot[2] - 0.08), 0.012, 0.012, "straps", 4))
    # the legs: the left foot a pace forward and its knee a little bent, the right behind
    feet = {-1.0: (0.16, -0.12), 1.0: (-0.1, 0.12)}            # by side: how far forward, how far out
    for side in (() if pose == "rider" else (-1.0, 1.0)):
        fx, fy = feet[side]
        kx = fx * 0.55 + (0.05 if side < 0.0 else 0.0)
        parts.append(box("boot", (0.27, 0.105, 0.1), (fx + 0.045, fy, 0.05), "boots", 0.03, turn=(0.0, 0.0, -8.0 * side)))
        parts.append(limb("shin", (fx, fy, 0.1), (kx, fy * 0.98, knee), 0.05, 0.064, "boots" if k["tall_boots"] else "lower"))
        parts.append(ball("knee", (kx, fy * 0.98, knee), 0.066, "trousers", u=8, v=5))
        parts.append(limb("thigh", (kx, fy * 0.98, knee), (0.0, fy * 0.8, hip), 0.066, 0.09, "trousers"))
    # the tunic: its skirt below the belt, the body rounded and fuller at the chest, the shoulders and the collar
    parts.append(oval("skirt", (0.0, 0.0, 0.8), 0.125, 0.185, 0.2, "tunic", top=0.9))
    parts.append(oval("body", (0.0, 0.0, 0.98), 0.105, 0.165, 0.5, "tunic", top=1.16, lean=0.03))
    parts.append(oval("belt", (0.0, 0.0, belt - 0.025), 0.115, 0.172, 0.05, "straps", top=1.0, sides=12))
    parts.append(box("buckle", (0.02, 0.05, 0.04), (0.12, 0.0, belt), "steel"))
    parts.append(oval("collar", (0.03, 0.0, 1.46), 0.06, 0.075, 0.045, "tunic", top=0.9, sides=8))
    for side in (-1.0, 1.0):
        parts.append(ball("shoulder", (0.02, side * 0.185, shoulder), 0.08, "tunic", u=8, v=5))
    def rifle_from(butt, way):
        long = k["rifle"]
        tip = butt + way * long
        over = Vector((0.0, 0.0, 0.018)) if abs(way.x) < 0.5 and abs(way.z) < 0.95 else Vector((0.016, 0.0, 0.0))
        parts.append(limb("butt", butt, butt + way * (long * 0.3), 0.036, 0.026, "wood", 6))
        parts.append(limb("stock", butt + way * (long * 0.3), butt + way * (long * 0.78), 0.024, 0.02, "wood", 6))
        parts.append(limb("barrel", butt + way * (long * 0.4) + over, tip + over, 0.011, 0.01, "steel", 5))
        parts.append(box("bolt", (0.04, 0.05, 0.03), tuple(butt + way * (long * 0.34) + over + Vector((0.01, 0.02, 0.0))), "steel"))
        if k["bayonet"] and pose == "rifle":
            parts.append(limb("bayonet", tip + over, tip + way * 0.4 + over, 0.008, 0.003, "steel", 4))
        return long

    # what he holds across his body, at the port: its foot by the right hip, its head up past the left shoulder
    butt = Vector((0.13, 0.25, 0.98))
    way = Vector((0.1, -0.52, 0.85)).normalized()
    if pose == "rifle":
        long = rifle_from(butt, way)
        parts.append(limb("sling", butt + way * (long * 0.1) + Vector((0.0, 0.0, -0.04)), butt + way * (long * 0.7) + Vector((0.0, 0.0, -0.035)), 0.006, 0.006, "straps", 4))
    else:
        # (a rider's and a pioneer's rifle is slung across his back, clear of his pack)
        behind = -0.27 if k["pack"] else -0.15
        long = rifle_from(Vector((behind, 0.2, 0.84)), Vector((0.02, -0.42, 0.9)).normalized())
        parts.append(limb("sling", (0.1, -0.12, shoulder + 0.04), (0.08, 0.12, belt + 0.04), 0.012, 0.012, "straps", 4))
    if pose == "shovel":
        # a pioneer's shovel, carried the same way: the grip by the hip, the blade up
        long = 1.15
        parts.append(limb("shaft", butt, butt + way * long, 0.02, 0.02, "wood", 6))
        parts.append(box("grip", (0.03, 0.11, 0.03), tuple(butt), "wood", 0.008))
        blade = butt + way * (long + 0.12)
        turn = Vector((0.0, 0.0, 1.0)).rotation_difference(way).to_matrix().to_4x4()
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for vert in bm.verts:
            vert.co = Vector((vert.co.x * 0.03, vert.co.y * (0.2 if vert.co.z < 0.0 else 0.16), vert.co.z * 0.28))
        bmesh.ops.transform(bm, matrix=Matrix.Translation(blade) @ turn, verts=bm.verts)
        parts.append(make("blade", bm, "steel", 0.008))
    # the arms
    if pose == "rider":
        # both hands low in front, on the reins
        for side in (-1.0, 1.0):
            elbow, hand = Vector((0.05, side * 0.28, 1.15)), Vector((0.3, side * 0.07, 1.06))
            parts.append(limb("upper_arm", (0.02, side * 0.21, shoulder), elbow, 0.058, 0.05, "tunic"))
            parts.append(limb("forearm", elbow, hand, 0.05, 0.04, "tunic"))
            parts.append(ball("elbow", tuple(elbow), 0.052, "tunic", u=8, v=5))
            parts.append(ball("hand", tuple(hand), (0.045, 0.04, 0.04), "skin", u=8, v=5))
    else:
        # the right hand at the small of the butt, the left under the fore-end
        long = k["rifle"] if pose == "rifle" else 1.15
        right_hand, left_hand = butt + way * (long * 0.24), butt + way * (long * 0.56)
        r_elbow, l_elbow = Vector((-0.04, 0.3, 1.14)), Vector((0.1, -0.29, 1.17))
        parts.append(limb("upper_arm", (0.02, 0.21, shoulder), r_elbow, 0.058, 0.05, "tunic"))
        parts.append(limb("forearm", r_elbow, right_hand + Vector((-0.02, 0.02, -0.01)), 0.05, 0.04, "tunic"))
        parts.append(ball("hand", tuple(right_hand), (0.042, 0.04, 0.045), "skin", u=8, v=5))
        parts.append(limb("upper_arm", (0.02, -0.21, shoulder), l_elbow, 0.058, 0.05, "tunic"))
        parts.append(limb("forearm", l_elbow, left_hand + Vector((-0.01, -0.02, -0.03)), 0.05, 0.04, "tunic"))
        parts.append(ball("hand", tuple(left_hand + Vector((0.0, 0.0, -0.02))), (0.042, 0.04, 0.045), "skin", u=8, v=5))
        parts.append(ball("elbow", tuple(r_elbow), 0.052, "tunic", u=8, v=5))
        parts.append(ball("elbow", tuple(l_elbow), 0.052, "tunic", u=8, v=5))
    # the neck and the head, turned a little to his left
    parts.append(limb("neck", (0.02, 0.0, 1.46), (0.03, 0.0, 1.56), 0.052, 0.048, "skin"))
    hx = 0.04
    parts.append(ball("head", (hx, 0.0, 1.63), (0.1, 0.086, 0.108), "skin"))
    parts.append(box("nose", (0.03, 0.024, 0.04), (hx + 0.098, -0.008, 1.625), "skin", 0.008))
    # the helmet
    top = 1.665
    kind = k["helmet"]
    if kind == "stahl":
        parts.append(ball("helmet", (hx - 0.012, 0.0, top), (0.128, 0.118, 0.105), "helmet", flat_below=0.0))
        parts.append(drum("helmet_skirt", 0.15, 0.085, (hx - 0.037, 0.0, top - 0.04), "helmet", 12, axis="Z", radius2=0.124))
        parts.append(box("visor", (0.06, 0.2, 0.012), (hx + 0.113, 0.0, top + 0.002), "helmet", 0.004))
    elif kind == "brodie":
        parts.append(ball("helmet", (hx - 0.012, 0.0, top + 0.02), (0.118, 0.112, 0.07), "helmet", flat_below=0.0))
        parts.append(drum("brim", 0.175, 0.016, (hx - 0.012, 0.0, top + 0.02), "helmet", 14, axis="Z", radius2=0.13))
    elif kind == "soviet":
        parts.append(ball("helmet", (hx - 0.012, 0.0, top), (0.126, 0.116, 0.108), "helmet", flat_below=0.0))
        parts.append(drum("helmet_skirt", 0.138, 0.05, (hx - 0.02, 0.0, top - 0.025), "helmet", 12, axis="Z", radius2=0.124))
    elif kind == "adrian":
        parts.append(ball("helmet", (hx - 0.012, 0.0, top + 0.01), (0.118, 0.11, 0.1), "helmet", flat_below=0.0))
        parts.append(drum("brim", 0.15, 0.016, (hx - 0.002, 0.0, top + 0.008), "helmet", 14, axis="Z", radius2=0.122))
        parts.append(box("crest", (0.2, 0.018, 0.03), (hx - 0.022, 0.0, top + 0.105), "helmet", 0.006))
    elif kind == "dome":
        # (Rulings 78 and 79; the art director's numbers.) The plain helmet, our own and no army's: a low dome with a
        # level rim all round and no crest. Wider in the rim than the Japanese one, and with no crest, so it is neither
        # that nor the French one.
        parts.append(ball("helmet", (hx - 0.014, 0.0, top + 0.015), (0.124, 0.114, 0.1), "helmet", flat_below=0.0))
        parts.append(drum("brim", 0.151, 0.018, (hx - 0.014, 0.0, top + 0.006), "helmet", 14, axis="Z", radius2=0.124))
    elif kind == "peaked":
        # (The art director's numbers.) The peaked helmet, our own: a taller egg with a peak in front only, close to
        # the head behind. It has no flare at the sides or the neck, so it is not the German one.
        parts.append(ball("helmet", (hx - 0.014, 0.0, top), (0.13, 0.117, 0.132), "helmet", flat_below=0.0))
        parts.append(drum("helmet_skirt", 0.127, 0.04, (hx - 0.016, 0.0, top - 0.02), "helmet", 12, axis="Z", radius2=0.125))
        parts.append(box("peak", (0.075, 0.19, 0.012), (hx + 0.128, 0.0, top - 0.014), "helmet", 0.004, turn=(0.0, 10.0, 0.0)))
    elif kind == "sun":
        # The sun helmet, our own: a tall cloth-covered dome with a sloping brim, longest over the neck, and a darker
        # band round the foot of the crown.
        parts.append(ball("helmet", (hx - 0.014, 0.0, top + 0.015), (0.13, 0.12, 0.138), "helmet", flat_below=0.0))
        parts.append(drum("brim", 0.186, 0.055, (hx - 0.034, 0.0, top - 0.012), "helmet", 14, axis="Z", radius2=0.126))
        parts.append(drum("band", 0.1305, 0.02, (hx - 0.014, 0.0, top + 0.027), "straps", 12, axis="Z", radius2=0.1285))
    elif kind == "fieldcap":
        # The field cap, our own: the soft cap's crown, flatter on top, with a short peak and no sun flap.
        parts.append(ball("helmet", (hx - 0.008, 0.0, top - 0.005), (0.112, 0.104, 0.06), "helmet", flat_below=0.0))
        parts.append(drum("cap_band", 0.107, 0.036, (hx - 0.008, 0.0, top - 0.015), "helmet", 12, axis="Z", radius2=0.105))
        parts.append(box("visor", (0.06, 0.15, 0.01), (hx + 0.12, 0.0, top - 0.022), "helmet", 0.004))
    elif kind == "eastern":
        parts.append(ball("helmet", (hx - 0.012, 0.0, top), (0.122, 0.114, 0.108), "helmet", flat_below=0.0))
        parts.append(drum("brim", 0.14, 0.014, (hx - 0.006, 0.0, top), "helmet", 14, axis="Z", radius2=0.124))
    elif kind == "cap":
        # (The art director's sheet for the Pacific: the Japanese soldier wears a soft field cap with a short peak and
        # a sun flap of four cloth strips hanging to his collar behind and at each side. It is cloth, in the cloth's colour.)
        parts.append(ball("helmet", (hx - 0.008, 0.0, top - 0.005), (0.112, 0.104, 0.07), "helmet", flat_below=0.0))
        parts.append(drum("cap_band", 0.107, 0.03, (hx - 0.008, 0.0, top - 0.012), "helmet", 12, axis="Z", radius2=0.104))
        parts.append(box("visor", (0.07, 0.15, 0.01), (hx + 0.125, 0.0, top - 0.014), "helmet", 0.004))
        parts.append(box("flap", (0.01, 0.1, 0.15), (hx - 0.118, 0.045, top - 0.095), "helmet", 0.003, turn=(0.0, 6.0, 0.0)))
        parts.append(box("flap", (0.01, 0.1, 0.15), (hx - 0.118, -0.045, top - 0.095), "helmet", 0.003, turn=(0.0, 6.0, 0.0)))
        for side in (-1.0, 1.0):
            parts.append(box("flap", (0.09, 0.01, 0.14), (hx - 0.062, side * 0.103, top - 0.09), "helmet", 0.003))
    elif kind == "round":
        parts.append(ball("helmet", (hx - 0.012, 0.0, top - 0.005), (0.124, 0.116, 0.112), "helmet", flat_below=-0.12))
    else:
        parts.append(ball("helmet", (hx - 0.012, 0.0, top - 0.01), (0.128, 0.12, 0.118), "helmet", flat_below=-0.1))
    parts.append(limb("chin_strap", (hx + 0.05, -0.085, 1.66), (hx + 0.04, 0.0, 1.535), 0.008, 0.008, "straps", 4))
    parts.append(limb("chin_strap", (hx + 0.05, 0.085, 1.66), (hx + 0.04, 0.0, 1.535), 0.008, 0.008, "straps", 4))
    # what he carries
    for side in (-1.0, 1.0):                                # the braces over each shoulder, front and back
        parts.append(limb("brace", (0.105, side * 0.07, belt + 0.03), (0.075, side * 0.13, shoulder + 0.04), 0.014, 0.014, "straps", 4))
        parts.append(limb("brace", (-0.1, side * 0.07, belt + 0.03), (-0.07, side * 0.13, shoulder + 0.04), 0.014, 0.014, "straps", 4))
    if k["chest_pouches"]:
        for side in (-1.0, 1.0):
            parts.append(box("pouch", (0.07, 0.11, 0.19), (0.13, side * 0.105, 1.2), "pack", 0.015))
    else:
        for side in (-1.0, 1.0):
            for n in range(2):
                parts.append(box("pouch", (0.05, 0.07, 0.09), (0.115 - 0.02 * n, side * (0.065 + 0.085 * n), belt + 0.005), "straps", 0.01))
    if k["pack"]:
        parts.append(box("pack", (0.14, 0.3, 0.33), (-0.165, 0.0, 1.24), "pack", 0.03))
        parts.append(box("pack_flap", (0.145, 0.31, 0.03), (-0.165, 0.0, 1.3), "straps"))
        parts.append(drum("bedroll", 0.055, 0.34, (-0.165, 0.0, 1.45), "pack", 8))
    if k["roll"]:
        # a blanket roll worn across the body, from the left shoulder to the right hip
        ring = []
        for n in range(11):
            t = 2.0 * math.pi * n / 10.0
            lift = math.sin(t)
            ring.append((0.15 * math.cos(t) - 0.01, -0.03 - 0.2 * lift, 1.2 + 0.25 * lift))
        for n in range(10):
            parts.append(limb("roll", ring[n], ring[n + 1], 0.05, 0.05, "pack", 7))
    if k["canister"]:
        parts.append(drum("canister", 0.055, 0.27, (-0.15, 0.03, belt - 0.03), "helmet", 10))
    parts.append(ball("bottle", (-0.1, 0.18, belt - 0.07), (0.05, 0.035, 0.07), "pack", u=8, v=5))
    parts.append(box("bread_bag", (0.07, 0.16, 0.14), (-0.09, -0.16, belt - 0.08), "pack", 0.02))
    if pose == "rider":
        for part in parts:
            part.location.z += 0.72
        parts += horse()
        bpy.context.view_layer.update()
    return parts


def horse():
    """A troop horse standing square, saddled, with a rolled cloak behind the saddle. About 1.6 metres at the withers."""
    parts = []
    parts.append(ball("barrel", (-0.1, 0.0, 1.22), (0.86, 0.3, 0.33), "coat", u=12, v=8))
    parts.append(ball("chest", (0.5, 0.0, 1.27), (0.32, 0.27, 0.35), "coat", u=10, v=6))
    parts.append(ball("rump", (-0.68, 0.0, 1.28), (0.35, 0.29, 0.33), "coat", u=10, v=6))
    parts.append(limb("horse_neck", (0.6, 0.0, 1.38), (1.14, 0.0, 1.9), 0.22, 0.125, "coat", 10))
    parts.append(ball("poll", (1.13, 0.0, 1.93), 0.135, "coat", u=8, v=5))
    parts.append(limb("horse_head", (1.1, 0.0, 1.95), (1.58, 0.0, 1.66), 0.135, 0.08, "coat", 10))
    parts.append(ball("muzzle", (1.6, 0.0, 1.65), (0.1, 0.085, 0.09), "coat_dark", u=8, v=5))
    for side in (-1.0, 1.0):
        parts.append(limb("ear", (1.12, side * 0.07, 2.03), (1.08, side * 0.09, 2.17), 0.035, 0.012, "coat", 4))
        parts.append(limb("rein", (1.5, side * 0.085, 1.7), (0.3, side * 0.07, 1.78), 0.009, 0.009, "leather", 4))
        parts.append(limb("cheek_strap", (1.22, side * 0.115, 1.9), (1.5, side * 0.09, 1.7), 0.009, 0.009, "leather", 4))
    parts.append(limb("mane", (0.68, 0.0, 1.6), (1.1, 0.0, 2.04), 0.055, 0.04, "coat_dark", 5))
    parts.append(limb("forelock", (1.1, 0.0, 2.05), (1.28, 0.0, 1.96), 0.04, 0.02, "coat_dark", 5))
    parts.append(limb("tail", (-0.98, 0.0, 1.4), (-1.2, 0.0, 0.72), 0.07, 0.03, "coat_dark", 6))
    for side in (-1.0, 1.0):
        for top, joint, foot, thick in (((0.5, side * 0.15, 1.1), (0.55, side * 0.15, 0.58), (0.53, side * 0.15, 0.1), 0.09),
                                        ((-0.62, side * 0.17, 1.14), (-0.8, side * 0.17, 0.62), (-0.72, side * 0.17, 0.1), 0.11)):
            parts.append(limb("gaskin", top, joint, thick, 0.055, "coat", 8))
            parts.append(ball("hock", joint, 0.058, "coat", u=8, v=5))
            parts.append(limb("cannon", joint, foot, 0.05, 0.038, "coat", 6))
            parts.append(drum("hoof", 0.06 * THICKER["hoof"], 0.1, (foot[0], foot[1], 0.05), "hoof", 8, axis="Z", radius2=0.048 * THICKER["hoof"]))
    # the saddle on its blanket, with a girth, and a rolled cloak behind it
    parts.append(box("saddle_blanket", (0.72, 0.64, 0.05), (-0.02, 0.0, 1.535), "blanket", 0.015,
                     shape=lambda p: Vector((p.x, p.y, p.z - (2.2 if abs(p.y) > 0.4 else 0.0)))))
    parts.append(box("saddle", (0.5, 0.4, 0.08), (0.0, 0.0, 1.585), "leather", 0.025,
                     shape=lambda p: Vector((p.x, p.y, p.z + (0.5 if p.z > 0.0 and abs(p.x) > 0.4 else 0.0)))))
    parts.append(drum("cloak_roll", 0.07, 0.52, (-0.3, 0.0, 1.63), "pack", 8))
    for side in (-1.0, 1.0):
        parts.append(limb("girth", (0.08, side * 0.3, 1.5), (0.08, side * 0.2, 0.93), 0.02, 0.02, "leather", 4))
    return parts


RIFLEMAN = {"kind": "figure", "scale": 1.0, "helmet": "pot", "tall_boots": False, "pack": True, "roll": False, "canister": False, "chest_pouches": False,
            "rifle": 1.11, "bayonet": False}
FIGURES = {
    # One rifleman for each helmet the game knows, with the kit that goes with it.
    "Rifleman_Pot": dict(RIFLEMAN),
    "Rifleman_Stahl": dict(RIFLEMAN, helmet="stahl", tall_boots=True, pack=False, canister=True),
    "Rifleman_Brodie": dict(RIFLEMAN, helmet="brodie", chest_pouches=True, rifle=1.13),
    "Rifleman_Soviet": dict(RIFLEMAN, helmet="soviet", tall_boots=True, pack=False, roll=True, rifle=1.23, bayonet=True),
    "Rifleman_Adrian": dict(RIFLEMAN, helmet="adrian", rifle=1.08),
    "Rifleman_Round": dict(RIFLEMAN, helmet="round", rifle=1.05),
    "Rifleman_Eastern": dict(RIFLEMAN, helmet="eastern", rifle=1.27, bayonet=True),
    "Rifleman_Cap": dict(RIFLEMAN, helmet="cap", rifle=1.27, bayonet=True),
    "Rifleman_Dome": dict(RIFLEMAN, helmet="dome"),
    "Rifleman_Peaked": dict(RIFLEMAN, helmet="peaked"),
    "Rifleman_Sun": dict(RIFLEMAN, helmet="sun"),
    "Rifleman_Fieldcap": dict(RIFLEMAN, helmet="fieldcap"),
}
# (and each as a pioneer with a shovel, and mounted)
for _name, _spec in list(FIGURES.items()):
    FIGURES[_name.replace("Rifleman", "Pioneer")] = dict(_spec, pose="shovel")
    FIGURES[_name.replace("Rifleman", "Rider")] = dict(_spec, pose="rider")


def marks_of(s):
    """Where the game paints its markings, on the right-hand side (it mirrors them for the left): the nation's own
    mark on the turret's side at its waist, a little behind the turret's middle (on the hull's side instead where
    the turret is small or set to one side), and the number on the upper hull's side behind it. Each is a place
    (x, y, z), a size, and how far the plate it lies on leans back. "roof" is the height of the turret's roof with
    its cupola: the aerial stands higher, and must not be taken for the top of the tank."""
    k = s["scale"]
    turret_z = s["hull_deck"] + s["upper_high"] + 0.02
    lean = s["upper_wide"] * 0.5 * (1.0 - s["upper_lean"]) / s["upper_high"]
    hull_y = s["upper_wide"] * 0.5 * (1.0 + s["upper_lean"]) * 0.5 + 0.015
    hull_z = s["hull_deck"] + s["upper_high"] * 0.5
    high = min(0.26, s["upper_high"] * 0.56)
    number = [-0.95 * s["hull_long"] / 5.5, hull_y, hull_z, high, lean]
    if s["turret_y"] != 0.0 or s["turret_wide"] < 1.55:
        cross = [0.1, hull_y, hull_z, min(0.2, s["upper_high"] * 0.42), lean]
    else:
        back = s["turret_long"] * 0.1
        if s["turret"] == "box":
            side = s["turret_wide"] * 0.5 * (1.0 + s["turret_taper"]) * 0.5
            back = s["turret_long"] * 0.24
        else:
            across = math.sqrt(max(1.0 - (back / (s["turret_long"] * 0.5)) ** 2, 0.0))
            side = across * (1.0 + s["turret_taper"]) * 0.5 * s["turret_wide"] * 0.5
        lean_t = s["turret_wide"] * 0.5 * (1.0 - s["turret_taper"]) / s["turret_high"]
        cross = [s["turret_x"] - 0.1 - back, side + 0.015, turret_z + s["turret_high"] * 0.5, min(0.2, s["turret_high"] * 0.34), lean_t]
    return {
        "cross": [round(cross[0] * k, 3), round(cross[1] * k, 3), round(cross[2] * k, 3), round(cross[3] * k, 3), round(cross[4], 3)],
        "number": [round(number[0] * k, 3), round(number[1] * k, 3), round(number[2] * k, 3), round(number[3] * k, 3), round(number[4], 3)],
        "roof": round((turret_z + s["turret_high"] + (0.25 if s["cupola"] else 0.05)) * k, 3),
    }


def write(name, title, s, out):
    """Sculpts one model and writes it out."""
    fresh_scene()
    global PAINT_SET, FAR_RATIO, CAMO
    kind = s.get("kind", "tank")
    CAMO = s["camo"]["size"] if s.get("camo") else None
    PAINT_SET = FIGURE_PAINTS if kind == "figure" else PAINTS
    FAR_RATIO = 0.42 if kind == "figure" else 1.0   # (about 450 triangles a rifleman, as the art director ruled)
    parts = (build_figure(s) if kind == "figure" else build_truck(s) if kind == "truck" else build_halftrack(s) if kind == "halftrack"
             else build_gun(s) if kind == "gun" else build(s))
    near = gather(parts)
    far = gather(parts, simple=True)
    k = s["scale"]
    if k != 1.0:
        for mesh in (near, far):
            mesh["points"] = [round(v * k, 4) for v in mesh["points"]]
    low = min(near["points"][2::3])
    high = round(max(near["points"][2::3]) - low, 3)
    if kind == "tank":
        marks = marks_of(s)
    elif kind == "truck":
        # the nation's mark on each door of the cab; no number
        marks = {"cross": [0.72 * k, 0.975 * k, 1.3 * k, 0.16 * k, 0.0], "roof": round(2.25 * k, 3)}
    elif kind == "halftrack":
        half = s["body_wide"] * 0.5
        marks = {"cross": [round((s["body_x"] + 0.5) * k, 3), round((half * (1.0 + s["lean"]) * 0.5 + 0.02) * k, 3), round(1.65 * k, 3), 0.17 * k,
                           round(half * (1.0 - s["lean"]) / 0.6, 3)],
                 "number": [round((s["body_x"] - 0.9) * k, 3), round((half * (1.0 + s["lean"]) * 0.5 + 0.02) * k, 3), round(1.65 * k, 3), 0.22 * k,
                            round(half * (1.0 - s["lean"]) / 0.6, 3)],
                 "roof": round(2.0 * k, 3)}
    else:
        marks = {"roof": high}                              # a gun carries no markings
    data = {
        "name": title,
        "about": "Made by Tools/build_tank.py. Metres; x forward, y right, z up. Paints: " + ", ".join(f"{v} {k}" for k, v in PAINT_SET.items()),
        "long": round(max(near["points"][0::3]) - min(near["points"][0::3]), 3),
        "high": high,
        "marks": marks,
        "near": near,
        "far": far,
    }
    if s.get("camo"):
        data["camo"] = {"a": s["camo"]["a"], "b": s["camo"]["b"], "stripe": s["camo"]["stripe"]}
    with open(out, "w") as file:
        json.dump(data, file, separators=(",", ":"))
    print(f"Theater Command: {name} written to {out}: {len(near['triangles']) // 3} triangles, {len(near['points']) // 3} points "
          f"(far: {len(far['triangles']) // 3} triangles), {data['long']} m long, {data['high']} m high")


def main():
    rest = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "--all" in rest:
        folder = rest[rest.index("--all") + 1]
        only = rest[rest.index("--only") + 1].split(",") if "--only" in rest else None
        for name, changes in VARIANTS.items():
            if only and name not in only:
                continue
            spec = dict(SPEC)
            spec.update(changes)
            write(name, name + " (our own design)", spec, folder.rstrip("/") + "/" + name + ".json")
        for name, spec in list(OTHERS.items()) + list(FIGURES.items()):
            if only and name not in only:
                continue
            write(name, name + " (our own design)", spec, folder.rstrip("/") + "/" + name + ".json")
        return
    out = rest[rest.index("--out") + 1] if "--out" in rest else "TrialTank.json"
    write("trial tank", "Trial medium tank", dict(SPEC), out)


main()
