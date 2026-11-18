"""A small stretch of battlefield for showing the battle units in place: for Brian's preview pictures and clips only.

It is NOT the game's battlefield (the 3D game builds that from the field the rules hand over, ruled by the art director:
field-battle-look.md). It borrows that ruling's look so the preview does not promise something else: natural ground in
the campaign's palette, September light, a road, a field, a wood's edge, a farm, a few shell holes, and the ground
running on into haze so a low camera sees no edge.

    import battle_set; stage = battle_set.build(seed=3)
    stage.height(x, y)  -> the ground's height there, to stand a man or a tank on it
"""

import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector, noise
from mathutils.bvhtree import BVHTree

import kit

HAZE = "C9C2B2"


def haze(mat, start=60.0, end=700.0, colour=HAZE, most=0.85):
    """Fade a material toward the haze with distance from the camera (cheaper than a volume, and the same to the eye)."""
    nt = mat.node_tree
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    src = out.inputs["Surface"].links[0].from_socket
    cam = nt.nodes.new("ShaderNodeCameraData")
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["From Min"].default_value = start
    mr.inputs["From Max"].default_value = end
    mr.inputs["To Min"].default_value = 0.0
    mr.inputs["To Max"].default_value = most
    nt.links.new(cam.outputs["View Distance"], mr.inputs["Value"])
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (*kit.hex_rgb(colour), 1)
    em.inputs["Strength"].default_value = 0.62
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(mr.outputs["Result"], mix.inputs["Fac"])
    nt.links.new(src, mix.inputs[1])
    nt.links.new(em.outputs["Emission"], mix.inputs[2])
    nt.links.new(mix.outputs["Shader"], out.inputs["Surface"])
    return mat


class Stage:
    def __init__(self, terrain):
        self.terrain = terrain
        dg = bpy.context.evaluated_depsgraph_get()
        self.bvh = BVHTree.FromObject(terrain, dg)

    def height(self, x, y):
        hit = self.bvh.ray_cast(Vector((x, y, 200.0)), Vector((0, 0, -1)))
        return hit[0].z if hit[0] is not None else 0.0

    def normal(self, x, y):
        hit = self.bvh.ray_cast(Vector((x, y, 200.0)), Vector((0, 0, -1)))
        return hit[1] if hit[1] is not None else Vector((0, 0, 1))


def _ground_height(x, y, road):
    """Gentle rolling ground; the road's bed is smoothed."""
    h = 1.6 * noise.noise(Vector((x / 140.0, y / 140.0, 0.3))) + 0.5 * noise.noise(Vector((x / 35.0, y / 35.0, 1.7)))
    h += 0.004 * (y + 40.0)                                    # the land rises a little toward the far side
    d = road_distance(x, y, road)
    if d < 6.0:
        h -= 0.12 * (1.0 - d / 6.0)
    return h


def road_distance(x, y, road):
    best = 1e9
    for (ax, ay), (bx, by) in zip(road, road[1:]):
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
        best = min(best, math.hypot(x - (ax + t * dx), y - (ay + t * dy)))
    return best


ROAD = [(-14.0, -400.0), (-10.0, -120.0), (-4.0, -40.0), (2.0, 10.0), (12.0, 60.0), (40.0, 160.0), (70.0, 400.0)]


def _terrain(size=900.0, near=180.0):
    """A ground mesh fine near the middle (where the camera looks) and coarse far off."""
    bm = bmesh.new()
    xs = sorted(set([x for x in range(-int(near), int(near) + 1, 2)] + [x for x in range(-int(size / 2), int(size / 2) + 1, 30)]))
    ys = xs
    verts = {}
    for i, x in enumerate(xs):
        for j, y in enumerate(ys):
            verts[i, j] = bm.verts.new((x, y, _ground_height(x, y, ROAD)))
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            bm.faces.new((verts[i, j], verts[i + 1, j], verts[i + 1, j + 1], verts[i, j + 1]))
    me = bpy.data.meshes.new("terrain")
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("terrain", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(_ground_material())
    return ob


def _ground_material():
    """Late-summer grass, worn to earth along the road, with a stubble field's rows to the east of the road."""
    m = bpy.data.materials.new("ground")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["Object"], sep.inputs[0])

    def noise_node(scale, detail=6.0):
        n = nt.nodes.new("ShaderNodeTexNoise")
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
        return n

    def ramp(fac, a, b, pa=0.35, pb=0.65):
        r = nt.nodes.new("ShaderNodeValToRGB")
        r.color_ramp.elements[0].position, r.color_ramp.elements[0].color = pa, (*kit.hex_rgb(a), 1)
        r.color_ramp.elements[1].position, r.color_ramp.elements[1].color = pb, (*kit.hex_rgb(b), 1)
        nt.links.new(fac, r.inputs["Fac"])
        return r

    big, mid, fine = noise_node(0.025, 4), noise_node(0.15, 8), noise_node(2.5, 10)
    grass = ramp(big.outputs["Fac"], "6F7B42", "8D8650")              # two greens of late summer
    dry = ramp(mid.outputs["Fac"], "7A8048", "9C8C5E", 0.45, 0.7)     # dry patches
    mix1 = nt.nodes.new("ShaderNodeMix")
    mix1.data_type = "RGBA"
    mix1.inputs["Factor"].default_value = 0.45
    nt.links.new(grass.outputs["Color"], mix1.inputs["A"])
    nt.links.new(dry.outputs["Color"], mix1.inputs["B"])
    # the stubble field: x > road + 8, rows running north-south
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.wave_type = "BANDS"
    wave.bands_direction = "X"
    wave.inputs["Scale"].default_value = 0.9
    wave.inputs["Distortion"].default_value = 1.5
    nt.links.new(tc.outputs["Object"], wave.inputs["Vector"])
    stub = ramp(wave.outputs["Fac"], "B39E68", "8E7B50", 0.3, 0.8)
    field_mask = nt.nodes.new("ShaderNodeMapRange")
    # field where x > 14 + 0.25*y (east of the road), soft edge 1.5 m
    mathx = nt.nodes.new("ShaderNodeMath")
    mathx.operation = "MULTIPLY_ADD"
    mathx.inputs[1].default_value = -0.25
    mathx.inputs[2].default_value = -14.0
    nt.links.new(sep.outputs["Y"], mathx.inputs[0])
    add = nt.nodes.new("ShaderNodeMath")
    add.operation = "ADD"
    nt.links.new(sep.outputs["X"], add.inputs[0])
    nt.links.new(mathx.outputs["Value"], add.inputs[1])
    field_mask.inputs["From Min"].default_value = 0.0
    field_mask.inputs["From Max"].default_value = 1.5
    nt.links.new(add.outputs["Value"], field_mask.inputs["Value"])
    # and only south of y = 70
    mask2 = nt.nodes.new("ShaderNodeMapRange")
    mask2.inputs["From Min"].default_value = 70.0
    mask2.inputs["From Max"].default_value = 68.5
    nt.links.new(sep.outputs["Y"], mask2.inputs["Value"])
    both = nt.nodes.new("ShaderNodeMath")
    both.operation = "MULTIPLY"
    nt.links.new(field_mask.outputs["Result"], both.inputs[0])
    nt.links.new(mask2.outputs["Result"], both.inputs[1])
    mix2 = nt.nodes.new("ShaderNodeMix")
    mix2.data_type = "RGBA"
    nt.links.new(both.outputs["Value"], mix2.inputs["Factor"])
    nt.links.new(mix1.outputs["Result"], mix2.inputs["A"])
    nt.links.new(stub.outputs["Color"], mix2.inputs["B"])
    # fine mottling
    fr = ramp(fine.outputs["Fac"], "8A8A8A", "FFFFFF", 0.25, 0.75)
    mix3 = nt.nodes.new("ShaderNodeMix")
    mix3.data_type = "RGBA"
    mix3.blend_type = "MULTIPLY"
    mix3.inputs["Factor"].default_value = 0.5
    nt.links.new(mix2.outputs["Result"], mix3.inputs["A"])
    nt.links.new(fr.outputs["Color"], mix3.inputs["B"])
    nt.links.new(mix3.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.97
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.4
    bump.inputs["Distance"].default_value = 0.05
    nt.links.new(fine.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return haze(m)


def _road(stage):
    """A dirt road: a strip 4.5 m wide laid on the ground, with two worn ruts."""
    pts = []
    for (ax, ay), (bx, by) in zip(ROAD, ROAD[1:]):
        n = max(1, int(math.hypot(bx - ax, by - ay) / 2.0))
        for k in range(n):
            t = k / n
            pts.append(Vector((ax + (bx - ax) * t, ay + (by - ay) * t, 0)))
    pts.append(Vector((*ROAD[-1], 0)))
    bm = bmesh.new()
    rows = []
    for i, p in enumerate(pts):
        q = pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]
        side = Vector((q.y, -q.x, 0)).normalized()
        row = []
        for off in (-2.6, -1.4, -0.9, 0.9, 1.4, 2.6):
            at = p + side * off
            z = stage.height(at.x, at.y) + (0.03 if abs(off) < 2.5 else -0.02)
            row.append(bm.verts.new((at.x, at.y, z)))
        rows.append(row)
    uv = []
    for a, b in zip(rows, rows[1:]):
        for k in range(len(a) - 1):
            bm.faces.new((a[k], a[k + 1], b[k + 1], b[k]))
    me = bpy.data.meshes.new("road")
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("road", me)
    bpy.context.scene.collection.objects.link(ob)
    mat = kit.material("road", kit.hex_rgb("9A8A6A"), rough=0.98, noise=0.45, noise_scale=1.6, bump=0.6)
    ob.data.materials.append(haze(mat))
    return ob


def _tree(name, at, h, seed, mats):
    """A broadleaf tree of our own: a leaning trunk, a few limbs, and a crown of lumpy clumps."""
    rnd = random.Random(seed)
    objs = []
    trunk_h = h * rnd.uniform(0.38, 0.48)
    bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=0.22 * h / 12, radius2=0.12 * h / 12, depth=trunk_h,
                                    location=(at.x, at.y, at.z + trunk_h / 2 - 0.2))
    t = bpy.context.object
    t.rotation_euler = (rnd.uniform(-0.06, 0.06), rnd.uniform(-0.06, 0.06), 0)
    t.data.materials.append(mats["bark"])
    objs.append(t)
    for k in range(rnd.randint(5, 8)):
        r = h * rnd.uniform(0.16, 0.26)
        ang = rnd.uniform(0, 2 * math.pi)
        rad = rnd.uniform(0.0, h * 0.2)
        cz = at.z + trunk_h + rnd.uniform(-0.05, 0.35) * h
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=r,
                                              location=(at.x + math.cos(ang) * rad, at.y + math.sin(ang) * rad, cz))
        c = bpy.context.object
        c.scale = (1.0, 1.0, rnd.uniform(0.75, 0.95))
        for v in c.data.vertices:
            w = Vector(v.co)
            v.co = w * (1.0 + 0.28 * noise.noise(w * 1.3 / r + Vector((seed, k, 0))))
        bpy.ops.object.shade_smooth()
        c.data.materials.append(mats["leaf"])
        objs.append(c)
    return objs


def _stone_wall(stage, a, b, mats, seed=1):
    rnd = random.Random(seed)
    a, b = Vector(a), Vector(b)
    n = int((b - a).length / 0.55)
    d = (b - a).normalized()
    yaw = math.atan2(d.y, d.x)
    for i in range(n):
        p = a + d * (i * 0.55)
        for course in range(3):
            if rnd.random() < 0.08 * course:           # a few stones fallen out of the top
                continue
            sx, sy, sz = rnd.uniform(0.45, 0.65), rnd.uniform(0.38, 0.5), rnd.uniform(0.22, 0.3)
            z = stage.height(p.x, p.y) + 0.12 + course * 0.25
            bpy.ops.mesh.primitive_cube_add(size=1, location=(p.x + rnd.uniform(-0.05, 0.05), p.y + rnd.uniform(-0.05, 0.05), z))
            s = bpy.context.object
            s.scale = (sx, sy, sz)
            s.rotation_euler = (rnd.uniform(-0.08, 0.08), rnd.uniform(-0.08, 0.08), yaw + rnd.uniform(-0.15, 0.15))
            bev = s.modifiers.new("bevel", "BEVEL")
            bev.width = 0.05
            bev.segments = 2
            s.data.materials.append(mats["stone"])


def _farm(stage, at, yaw, mats):
    """A small farmhouse of our own: whitewashed walls, a steep roof of old tiles, a broken corner from shellfire."""
    x, y = at
    z = stage.height(x, y)
    bm = bmesh.new()
    L, W, H = 11.0, 6.5, 3.2
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * L, v.co.y * W, (v.co.z + 0.5) * H))
    me = bpy.data.meshes.new("farm_walls")
    bm.to_mesh(me)
    bm.free()
    walls = bpy.data.objects.new("farm_walls", me)
    bpy.context.scene.collection.objects.link(walls)
    walls.location = (x, y, z - 0.3)
    walls.rotation_euler = (0, 0, yaw)
    walls.data.materials.append(mats["plaster"])
    # roof: two slopes
    roof_h = 3.4
    bm = bmesh.new()
    pts = [(-L / 2 - 0.4, -W / 2 - 0.5, H), (L / 2 + 0.4, -W / 2 - 0.5, H), (L / 2 + 0.4, 0, H + roof_h), (-L / 2 - 0.4, 0, H + roof_h),
           (-L / 2 - 0.4, W / 2 + 0.5, H), (L / 2 + 0.4, W / 2 + 0.5, H)]
    vs = [bm.verts.new(p) for p in pts]
    bm.faces.new((vs[0], vs[1], vs[2], vs[3]))
    bm.faces.new((vs[3], vs[2], vs[5], vs[4]))
    me = bpy.data.meshes.new("farm_roof")
    bm.to_mesh(me)
    bm.free()
    roof = bpy.data.objects.new("farm_roof", me)
    bpy.context.scene.collection.objects.link(roof)
    roof.location = walls.location
    roof.rotation_euler = walls.rotation_euler
    sol = roof.modifiers.new("thick", "SOLIDIFY")
    sol.thickness = 0.18
    roof.data.materials.append(mats["tile"])
    # windows and a door as dark insets
    for k, (wx, wz, ww, wh) in enumerate(((-3.5, 1.2, 1.0, 1.1), (0.0, 0.0, 1.1, 2.1), (3.5, 1.2, 1.0, 1.1))):
        bpy.ops.mesh.primitive_cube_add(size=1)
        o = bpy.context.object
        o.scale = (ww, 0.1, wh)
        off = Matrix.Rotation(yaw, 4, "Z") @ Vector((wx, -W / 2 - 0.02, 0))
        o.location = (x + off.x, y + off.y, z - 0.3 + wz + wh / 2 + 0.3)
        o.rotation_euler = (0, 0, yaw)
        o.data.materials.append(mats["dark"])
    # a chimney
    bpy.ops.mesh.primitive_cube_add(size=1)
    c = bpy.context.object
    c.scale = (0.7, 0.7, 2.0)
    off = Matrix.Rotation(yaw, 4, "Z") @ Vector((2.5, 1.2, 0))
    c.location = (x + off.x, y + off.y, z - 0.3 + H + roof_h - 0.2)
    c.data.materials.append(mats["plaster"])


def _crater(stage, x, y, r, mats):
    z = stage.height(x, y)
    bpy.ops.mesh.primitive_circle_add(vertices=24, radius=r, fill_type="TRIFAN", location=(x, y, z + 0.04))
    c = bpy.context.object
    for v in c.data.vertices:
        d = Vector((v.co.x, v.co.y)).length / r
        v.co.z = 0.02 + 0.06 * r * (1 - d * d) * 0 + 0.16 * r * math.exp(-((d - 0.85) ** 2) / 0.03) - 0.05 * max(0.0, d - 1.0)
        k = 1 + 0.15 * noise.noise(Vector((v.co.x, v.co.y, 3.0)))
        v.co.x *= k
        v.co.y *= k
    sub = c.modifiers.new("sub", "SUBSURF")
    sub.levels = sub.render_levels = 1
    bpy.ops.object.shade_smooth()
    c.data.materials.append(mats["earth"])


def _grass(stage, area, count, mats, seed=7):
    """Tufts of long grass scattered near the camera, so the ground has depth at a low angle."""
    rnd = random.Random(seed)
    bm = bmesh.new()
    (x0, y0), (x1, y1) = area
    for _ in range(count):
        x, y = rnd.uniform(x0, x1), rnd.uniform(y0, y1)
        if road_distance(x, y, ROAD) < 3.0:
            continue
        z = stage.height(x, y)
        for b in range(5):
            ang = rnd.uniform(0, 2 * math.pi)
            h = rnd.uniform(0.18, 0.45)
            lean = Vector((math.cos(ang), math.sin(ang), 0)) * rnd.uniform(0.04, 0.15)
            side = Vector((-math.sin(ang), math.cos(ang), 0)) * 0.012
            base = Vector((x + rnd.uniform(-0.05, 0.05), y + rnd.uniform(-0.05, 0.05), z - 0.02))
            v0, v1 = bm.verts.new(base - side), bm.verts.new(base + side)
            v2 = bm.verts.new(base + lean + Vector((0, 0, h)))
            bm.faces.new((v0, v1, v2))
    me = bpy.data.meshes.new("grass")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("grass", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(mats["grass"])
    return ob


def build(seed=3):
    """Lays the stage in the current scene (call kit.fresh() and kit.studio() first). Returns a Stage."""
    mats = {
        "bark": haze(kit.material("bark", kit.hex_rgb("4A3F33"), rough=0.95, noise=0.5, noise_scale=8, bump=0.5)),
        "leaf": haze(kit.material("leaf", kit.hex_rgb("4F6034"), rough=0.85, noise=0.55, noise_scale=3.0, bump=0.8)),
        "stone": haze(kit.material("stone", kit.hex_rgb("8E877A"), rough=0.92, noise=0.5, noise_scale=6, bump=0.6)),
        "plaster": haze(kit.material("plaster", kit.hex_rgb("CFC6B0"), rough=0.9, noise=0.35, noise_scale=2.0, bump=0.3)),
        "tile": haze(kit.material("tile", kit.hex_rgb("8A5A42"), rough=0.85, noise=0.4, noise_scale=4, bump=0.5)),
        "dark": haze(kit.material("dark", kit.hex_rgb("2A2622"), rough=0.9)),
        "earth": haze(kit.material("earth", kit.hex_rgb("5E5140"), rough=0.98, noise=0.4, noise_scale=3, bump=0.7)),
        "grass": haze(kit.material("grasstuft", kit.hex_rgb("7C8448"), rough=0.8, noise=0.5, noise_scale=1.5)),
    }
    terrain = _terrain()
    stage = Stage(terrain)
    _road(stage)
    rnd = random.Random(seed)
    # the wood's edge to the west of the road, ragged
    for k in range(150):
        x = rnd.uniform(-160, -24)
        y = rnd.uniform(18, 170)
        if x > -32 - 0.15 * (y - 18) + rnd.uniform(-8, 8):          # a ragged edge running away to the north-west
            continue
        _tree(f"tree{k}", Vector((x, y, stage.height(x, y))), rnd.uniform(9, 15), seed * 100 + k, mats)
    # single trees along the road and a field hedge
    for k, (x, y) in enumerate(((22, 40), (30, 78), (-20, -20), (48, 120), (-30, 70))):
        _tree(f"lone{k}", Vector((x, y, stage.height(x, y))), rnd.uniform(10, 14), seed * 7 + k, mats)
    _stone_wall(stage, (8.0, 28.0), (60.0, 41.0), mats, seed)
    _stone_wall(stage, (-6.0, 34.0), (-36.0, 30.0), mats, seed + 1)
    _farm(stage, (34.0, 66.0), math.radians(14), mats)
    for x, y, r in ((6, -6, 1.6), (-12, 14, 2.2), (25, 12, 1.4), (-28, -10, 1.9), (40, 22, 2.4)):
        _crater(stage, x, y, r, mats)
    _grass(stage, ((-45, -40), (55, 30)), 9000, mats, seed)
    return stage
