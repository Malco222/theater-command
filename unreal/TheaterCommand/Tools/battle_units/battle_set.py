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
    # the stubble field east of the road: straw in patches with weeds (no drill rows: at battle distance they only
    # beat against the pixels into bands)
    straw0 = ramp(mid.outputs["Fac"], "A8945F", "97875A", 0.35, 0.7)
    patch = ramp(noise_node(0.05, 3).outputs["Fac"], "000000", "FFFFFF", 0.42, 0.62)
    straw = nt.nodes.new("ShaderNodeMix")              # paler and darker patches across the field
    straw.data_type = "RGBA"
    nt.links.new(patch.outputs["Color"], straw.inputs["Factor"])
    nt.links.new(straw0.outputs["Color"], straw.inputs["A"])
    straw.inputs["B"].default_value = (*kit.hex_rgb("8C7D52"), 1)
    weeds = ramp(noise_node(0.6, 6).outputs["Fac"], "000000", "FFFFFF", 0.56, 0.66)   # about a fifth in weeds
    weedy = nt.nodes.new("ShaderNodeMix")
    weedy.data_type = "RGBA"
    nt.links.new(weeds.outputs["Color"], weedy.inputs["Factor"])
    nt.links.new(straw.outputs["Result"], weedy.inputs["A"])
    weedy.inputs["B"].default_value = (*kit.hex_rgb("7D8046"), 1)
    stub = weedy
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
    nt.links.new(stub.outputs["Result"], mix2.inputs["B"])
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


def _leaf_spray():
    """One spray of leaves (a dozen small leaf blades in a loose ball about 40 cm across), instanced by the thousand
    over each crown so a tree has a ragged, leafy outline and light through it, not a smooth green ball."""
    ob = bpy.data.objects.get("leafspray")
    if ob:
        return ob
    rnd = random.Random(5)
    bm = bmesh.new()
    for _ in range(12):
        c = Vector((rnd.uniform(-0.16, 0.16), rnd.uniform(-0.16, 0.16), rnd.uniform(-0.12, 0.12)))
        a = Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-1, 1))).normalized() * 0.075
        b = a.cross(Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-1, 1)))).normalized() * 0.035
        vs = [bm.verts.new(c - a), bm.verts.new(c + b), bm.verts.new(c + a * 1.1), bm.verts.new(c - b)]
        bm.faces.new(vs)
    me = bpy.data.meshes.new("leafspray")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("leafspray", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = (0, 0, -60)                       # the source sits under the ground; only its instances show
    m = bpy.data.materials.new("leafspray")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    info = nt.nodes.new("ShaderNodeObjectInfo")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    els = ramp.color_ramp.elements
    els[0].color = (*kit.hex_rgb("3C4E26"), 1)
    els[1].color = (*kit.hex_rgb("6A7A3A"), 1)
    mid = els.new(0.55)
    mid.color = (*kit.hex_rgb("51632F"), 1)
    nt.links.new(info.outputs["Random"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.7
    if "Transmission Weight" in bsdf.inputs:
        bsdf.inputs["Transmission Weight"].default_value = 0.0
    if "Subsurface Weight" in bsdf.inputs:            # a little light through the leaves
        bsdf.inputs["Subsurface Weight"].default_value = 0.15
    me.materials.append(haze(m))
    return ob


def _leaf_nodes(name, density, scale):
    """Geometry nodes that scatter leaf sprays over a crown clump at `density` sprays a square metre, each turned
    and sized at random, keeping the clump inside as the shaded heart of the crown."""
    ng = bpy.data.node_groups.get(name)
    if ng:
        return ng
    ng = bpy.data.node_groups.new(name, "GeometryNodeTree")
    ng.interface.new_socket("Geometry", in_out="INPUT", socket_type="NodeSocketGeometry")
    ng.interface.new_socket("Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
    n, ln = ng.nodes, ng.links
    gi, go = n.new("NodeGroupInput"), n.new("NodeGroupOutput")
    dist = n.new("GeometryNodeDistributePointsOnFaces")
    dist.distribute_method = "RANDOM"
    dist.inputs["Density"].default_value = density
    obi = n.new("GeometryNodeObjectInfo")
    obi.inputs["Object"].default_value = _leaf_spray()
    obi.inputs["As Instance"].default_value = True
    iop = n.new("GeometryNodeInstanceOnPoints")
    rot = n.new("FunctionNodeRandomValue")
    rot.data_type = "FLOAT_VECTOR"
    vin = [i for i in rot.inputs if i.type == "VECTOR"]
    vin[0].default_value, vin[1].default_value = (0, 0, 0), (6.283, 6.283, 6.283)
    size = n.new("FunctionNodeRandomValue")
    size.data_type = "FLOAT"
    fin = [i for i in size.inputs if i.type == "VALUE" and i.name in ("Min", "Max")]
    fin[0].default_value, fin[1].default_value = 0.6 * scale, 1.4 * scale
    join = n.new("GeometryNodeJoinGeometry")
    ln.new(gi.outputs[0], dist.inputs["Mesh"])
    ln.new(dist.outputs["Points"], iop.inputs["Points"])
    ln.new(obi.outputs["Geometry"], iop.inputs["Instance"])
    ln.new(next(o for o in rot.outputs if o.type == "VECTOR"), iop.inputs["Rotation"])
    ln.new(next(o for o in size.outputs if o.type == "VALUE"), iop.inputs["Scale"])
    ln.new(iop.outputs["Instances"], join.inputs["Geometry"])
    ln.new(gi.outputs[0], join.inputs["Geometry"])
    ln.new(join.outputs["Geometry"], go.inputs[0])
    return ng


def _clothe_crowns():
    """Leaves go on last: a geometry-nodes modifier on every clump costs each later scene update, so the stage adds
    them once, after everything else is laid."""
    near, far = _leaf_nodes("leaves_near", 26.0, 1.0), _leaf_nodes("leaves_far", 6.0, 1.9)
    for ob in list(bpy.data.objects):
        if "crown" in ob:
            mod = ob.modifiers.new("leaves", "NODES")
            mod.node_group = near if ob["crown"] == "near" else far


def _tree(name, at, h, seed, mats, near=False):
    """A broadleaf tree of our own: a leaning trunk, limbs reaching into the crown, and a crown of lumpy clumps
    clothed in leaf sprays (dense on the trees near the road, lighter in the far wood)."""
    rnd = random.Random(seed)
    objs = []
    trunk_h = h * rnd.uniform(0.38, 0.48)
    bpy.ops.mesh.primitive_cone_add(vertices=12, radius1=0.24 * h / 12, radius2=0.12 * h / 12, depth=trunk_h,
                                    location=(at.x, at.y, at.z + trunk_h / 2 - 0.2))
    t = bpy.context.object
    t.rotation_euler = (rnd.uniform(-0.06, 0.06), rnd.uniform(-0.06, 0.06), 0)
    bpy.ops.object.shade_smooth()
    t.data.materials.append(mats["bark"])
    objs.append(t)
    top = Vector((at.x, at.y, at.z + trunk_h - 0.3))
    for k in range(rnd.randint(5, 8)):
        r = h * rnd.uniform(0.16, 0.26)
        ang = rnd.uniform(0, 2 * math.pi)
        rad = rnd.uniform(0.0, h * 0.2)
        cz = at.z + trunk_h + rnd.uniform(-0.05, 0.35) * h
        centre = Vector((at.x + math.cos(ang) * rad, at.y + math.sin(ang) * rad, cz))
        if near:                                     # a limb from the trunk's top toward this clump
            d = centre - top
            bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=0.09 * h / 12, radius2=0.03 * h / 12,
                                            depth=d.length * 0.9, location=top + d * 0.45)
            limb = bpy.context.object
            limb.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
            bpy.ops.object.shade_smooth()
            limb.data.materials.append(mats["bark"])
            objs.append(limb)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3 if near else 2, radius=r * 0.88, location=centre)
        c = bpy.context.object
        c.scale = (1.0, 1.0, rnd.uniform(0.75, 0.95))
        for v in c.data.vertices:
            w = Vector(v.co)
            v.co = w * (1.0 + 0.28 * noise.noise(w * 1.3 / r + Vector((seed, k, 0))))
        bpy.ops.object.shade_smooth()
        c.data.materials.append(mats["crown"])
        c["crown"] = "near" if near else "far"
        objs.append(c)
    return objs


def _fieldstone_material():
    """Fieldstone: warm brown-grey, each stone its own shade, darker weathering in the hollows and patches of pale
    green-yellow and dark lichen."""
    m = bpy.data.materials.get("fieldstone")
    if m:
        return m
    m = bpy.data.materials.new("fieldstone")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.93
    tc = nt.nodes.new("ShaderNodeTexCoord")
    tone = nt.nodes.new("ShaderNodeAttribute")
    tone.attribute_name = "tone"
    base = nt.nodes.new("ShaderNodeValToRGB")
    els = base.color_ramp.elements
    els[0].color = (*kit.hex_rgb("6F675B"), 1)
    els[1].color = (*kit.hex_rgb("9A8F7C"), 1)
    mid = els.new(0.5)
    mid.color = (*kit.hex_rgb("857A68"), 1)
    nt.links.new(tone.outputs["Fac"], base.inputs["Fac"])

    def noise_tex(scale, detail):
        n = nt.nodes.new("ShaderNodeTexNoise")
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
        return n

    def mix(a, b, fac, kind="MIX"):
        x = nt.nodes.new("ShaderNodeMix")
        x.data_type = "RGBA"
        x.blend_type = kind
        nt.links.new(a, x.inputs["A"])
        if isinstance(b, tuple):
            x.inputs["B"].default_value = b
        else:
            nt.links.new(b, x.inputs["B"])
        if isinstance(fac, float):
            x.inputs["Factor"].default_value = fac
        else:
            nt.links.new(fac, x.inputs["Factor"])
        return x.outputs["Result"]

    def mask(n, lo, hi):
        r = nt.nodes.new("ShaderNodeMapRange")
        r.inputs["From Min"].default_value = lo
        r.inputs["From Max"].default_value = hi
        nt.links.new(n.outputs["Fac"], r.inputs["Value"])
        return r.outputs["Result"]

    grain = noise_tex(9.0, 8)
    shade = nt.nodes.new("ShaderNodeValToRGB")
    shade.color_ramp.elements[0].color = (0.7, 0.7, 0.7, 1)
    shade.color_ramp.elements[1].color = (1.15, 1.15, 1.15, 1)
    nt.links.new(grain.outputs["Fac"], shade.inputs["Fac"])
    col = mix(base.outputs["Color"], shade.outputs["Color"], 0.8, "MULTIPLY")
    pale = noise_tex(2.3, 6)
    col = mix(col, (*kit.hex_rgb("A2A275"), 1), mask(pale, 0.58, 0.64))          # pale lichen
    dark = noise_tex(3.1, 6)
    col = mix(col, (*kit.hex_rgb("3F3B33"), 1), mask(dark, 0.62, 0.68))          # dark lichen and damp
    nt.links.new(col, bsdf.inputs["Base Color"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.5
    bump.inputs["Distance"].default_value = 0.02
    nt.links.new(grain.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return haze(m)


def _stone_wall(stage, a, b, mats, seed=1):
    """A dry-stone field wall of gathered fieldstone. Broken stone, flat-bedded with sharp edges and flat faces:
    blocks, slabs and wedges laid in rough courses battered in toward the top, long through-stones crossing the wall,
    small chinking stones in the gaps, copes set on edge along the top, and only a few rounded stones. Weeds grow in
    clumps at its foot, a few top stones are gone and the far end has tumbled onto the grass."""
    rnd = random.Random(seed)
    a, b = Vector((a[0], a[1], 0.0)), Vector((b[0], b[1], 0.0))
    length = (b - a).length
    d = (b - a).normalized()
    side = Vector((-d.y, d.x, 0.0))
    yaw = math.atan2(d.y, d.x)
    tumble = length - rnd.uniform(2.5, 4.0)          # past here the wall has come down
    bm = bmesh.new()
    tone = bm.verts.layers.float.new("tone")

    def place(points, at, turn, tilt, roll):
        rot = Matrix.Rotation(yaw + turn, 4, "Z") @ Matrix.Rotation(tilt, 4, "X") @ Matrix.Rotation(roll, 4, "Y")
        vs = [bm.verts.new(at + rot @ p) for p in points]
        res = bmesh.ops.convex_hull(bm, input=vs, use_existing_faces=False)
        hull = {g for g in res["geom"] if isinstance(g, bmesh.types.BMVert)}
        junk = list({g for g in vs if g not in hull})
        if junk:
            bmesh.ops.delete(bm, geom=junk, context="VERTS")
        k = rnd.random()
        for g in res["geom"]:
            if isinstance(g, bmesh.types.BMVert):
                g[tone] = k

    def broken(lx, ly, lz, wedge=0.0):
        """A broken block: a box with its corners knocked off at random, faces a little out of square, and
        (for a wedge) one end thinner than the other. Its convex hull gives flat faces and sharp edges."""
        pts = []
        for sx in (-0.5, 0.5):
            taper = 1.0 - wedge * (sx + 0.5)
            for sy in (-0.5, 0.5):
                for sz in (-0.5, 0.5):
                    cut = rnd.uniform(0.0, 0.3)
                    pts.append(Vector((sx * lx * (1 - cut * rnd.random()), sy * ly * (1 - cut * rnd.random()),
                                       sz * lz * taper * (1 - cut * rnd.random()))))
        for _ in range(rnd.randint(3, 7)):                # a few extra facets on the faces
            ax = rnd.randrange(3)
            q = Vector((rnd.uniform(-0.45, 0.45) * lx, rnd.uniform(-0.45, 0.45) * ly, rnd.uniform(-0.45, 0.45) * lz))
            q[ax] = math.copysign((lx, ly, lz)[ax] * rnd.uniform(0.38, 0.5), rnd.uniform(-1, 1))
            pts.append(q)
        return pts

    def rounded(lx, ly, lz):
        o = Vector((rnd.uniform(0, 50), rnd.uniform(0, 50), rnd.uniform(0, 50)))
        pts = []
        for i in range(26):
            v = Vector((rnd.gauss(0, 1), rnd.gauss(0, 1), rnd.gauss(0, 1))).normalized()
            v *= 1.0 + 0.2 * noise.noise(v * 1.5 + o)
            pts.append(Vector((v.x * lx * 0.5, v.y * ly * 0.5, v.z * lz * 0.45)))
        return pts

    def stone_at(s, face, z, lx, ly, lz, width, kind):
        at = a + d * s + side * face * (width / 2 - ly / 2 + rnd.uniform(-0.02, 0.03))
        g = stage.height(at.x, at.y)
        if kind == "round":
            pts = rounded(lx, ly, lz)
        else:
            pts = broken(lx, ly, lz, rnd.uniform(0.2, 0.5) if kind == "wedge" else 0.0)
        place(pts, Vector((at.x, at.y, g + z + lz / 2)), rnd.uniform(-0.12, 0.12),
              face * rnd.uniform(0.02, 0.1), rnd.uniform(-0.08, 0.08))
        return g

    def course(z0, h, width, gap, max_s, start=0.0, top=False):
        s = start + rnd.uniform(0.0, 0.3)
        through_next = s + rnd.uniform(0.8, 1.6)
        while s < min(length, max_s):
            if top:                                        # copes on edge, leaning a little, with gaps
                lx = rnd.uniform(0.09, 0.2)
                if rnd.random() > gap:
                    at = a + d * (s + lx / 2) + side * rnd.uniform(-0.03, 0.03)
                    g = stage.height(at.x, at.y)
                    lz = rnd.uniform(0.22, 0.34)
                    place(broken(lx, width * rnd.uniform(0.9, 1.15), lz), Vector((at.x, at.y, g + z0 + lz * 0.45)),
                          rnd.uniform(-0.1, 0.1), 0.0, rnd.uniform(-0.25, 0.25))
                s += lx * rnd.uniform(1.0, 1.15)
                continue
            if s > through_next and rnd.random() < 0.7:   # a long through-stone across the whole wall
                lx = rnd.uniform(0.25, 0.4)
                lz = h * rnd.uniform(0.55, 0.75)
                at = a + d * (s + lx / 2)
                g = stage.height(at.x, at.y)
                place(broken(lx, width * rnd.uniform(1.05, 1.2), lz), Vector((at.x, at.y, g + z0 + lz / 2)),
                      rnd.uniform(-0.08, 0.08), rnd.uniform(-0.04, 0.04), rnd.uniform(-0.05, 0.05))
                s += lx * 0.95
                through_next = s + rnd.uniform(1.0, 2.0)
                continue
            lx = rnd.choice((rnd.uniform(0.2, 0.35), rnd.uniform(0.32, 0.6), rnd.uniform(0.32, 0.6), rnd.uniform(0.55, 0.9)))
            for face in (-1, 1):
                if rnd.random() < gap:
                    continue
                r = rnd.random()
                kind = "round" if r < 0.08 else "wedge" if r < 0.3 else "block"
                lz = h * (rnd.uniform(0.45, 0.7) if kind != "round" and rnd.random() < 0.4 else rnd.uniform(0.8, 1.1))
                ly = width * rnd.uniform(0.35, 0.55)
                zz = z0 + rnd.uniform(-0.02, 0.03)
                stone_at(s + lx / 2, face, zz, lx * 0.98, ly, lz, width, kind)
                if lz < h * 0.75:                          # a thin slab: bed another on it to make up the course
                    stone_at(s + lx / 2 + rnd.uniform(-0.05, 0.05), face, zz + lz, lx * rnd.uniform(0.6, 0.95), ly,
                             h - lz + rnd.uniform(-0.02, 0.04), width, "block")
                for _ in range(rnd.randint(0, 2)):         # chinking stones in the joint
                    cl = rnd.uniform(0.05, 0.11)
                    stone_at(s + rnd.uniform(-0.04, 0.04), face, zz + rnd.uniform(0.0, h * 0.7), cl,
                             cl * rnd.uniform(1.0, 2.0), cl * rnd.uniform(0.5, 0.9), width + 0.02, "block")
            s += lx

    # base course sunk a little into the turf, then narrower courses, then copes on edge along the top
    z, width = -0.07, 0.8
    for c, (h, gap) in enumerate(((0.3, 0.0), (0.26, 0.02), (0.24, 0.03), (0.22, 0.05))):
        course(z, h, width, gap, tumble if c < 1 else tumble - rnd.uniform(0.3, 1.5) * c)
        z += h
        width *= 0.88
    course(z, 0.3, width, 0.12, tumble - rnd.uniform(1.5, 3.0), top=True)
    # the tumbled end: a low base course, then stones spilled onto the grass on both sides
    course(-0.07, 0.24, 0.72, 0.35, length, start=tumble)
    s = tumble
    while s < length + 1.2:
        for _ in range(rnd.randint(1, 3)):
            at = a + d * (s + rnd.uniform(-0.3, 0.3)) + side * rnd.uniform(-1.6, 1.6)
            g = stage.height(at.x, at.y)
            lz = rnd.uniform(0.1, 0.24)
            pts = broken(rnd.uniform(0.2, 0.5), rnd.uniform(0.18, 0.4), lz) if rnd.random() > 0.15 else rounded(0.3, 0.25, lz)
            place(pts, Vector((at.x, at.y, g + lz * 0.3)), rnd.uniform(-1.5, 1.5), rnd.uniform(-0.4, 0.4),
                  rnd.uniform(-0.4, 0.4))
        s += rnd.uniform(0.3, 0.6)
    me = bpy.data.meshes.new("fieldwall")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("fieldwall", me)
    bpy.context.scene.collection.objects.link(ob)
    bev = ob.modifiers.new("edges", "BEVEL")            # a small chamfer so the broken edges catch the light
    bev.width = 0.01
    bev.segments = 1
    bev.limit_method = "ANGLE"
    bev.angle_limit = math.radians(35)
    me.materials.append(_fieldstone_material())
    # weeds and long grass in clumps at its foot
    _grass(stage, None, 0, mats, seed + 11, line=(a, b, 0.25, 0.9, int(length * 1.6)))
    return ob


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


def _broken_points(rnd, lx, ly, lz, wedge=0.0):
    """Points whose convex hull is a broken chunk: a box with its corners knocked off at random, faces a little out
    of square, extra facets, and (for a wedge) one end thinner than the other. Flat faces, sharp edges."""
    pts = []
    for sx in (-0.5, 0.5):
        taper = 1.0 - wedge * (sx + 0.5)
        for sy in (-0.5, 0.5):
            for sz in (-0.5, 0.5):
                cut = rnd.uniform(0.0, 0.35)
                pts.append(Vector((sx * lx * (1 - cut * rnd.random()), sy * ly * (1 - cut * rnd.random()),
                                   sz * lz * taper * (1 - cut * rnd.random()))))
    for _ in range(rnd.randint(3, 7)):
        ax = rnd.randrange(3)
        q = Vector((rnd.uniform(-0.45, 0.45) * lx, rnd.uniform(-0.45, 0.45) * ly, rnd.uniform(-0.45, 0.45) * lz))
        q[ax] = math.copysign((lx, ly, lz)[ax] * rnd.uniform(0.38, 0.5), rnd.uniform(-1, 1))
        pts.append(q)
    return pts


def _crater_material():
    """A fresh shell hole: raw dark earth inside (wet and darkest at the bottom), churned clods on the lip, and a fan
    of darker spray thrown across the grass, thickest on one side, speckled and ragged so it never reads as a disc."""
    m = bpy.data.materials.get("crater")
    if m:
        return m
    m = bpy.data.materials.new("crater")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.96
    tc = nt.nodes.new("ShaderNodeTexCoord")

    def attr(name):
        n = nt.nodes.new("ShaderNodeAttribute")
        n.attribute_name = name
        return n.outputs["Fac"]

    def noise_tex(scale, detail=8.0):
        n = nt.nodes.new("ShaderNodeTexNoise")
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
        return n.outputs["Fac"]

    def op(kind, x, y):
        n = nt.nodes.new("ShaderNodeMath")
        n.operation = kind
        for i, v in enumerate((x, y)):
            if isinstance(v, float):
                n.inputs[i].default_value = v
            else:
                nt.links.new(v, n.inputs[i])
        return n.outputs["Value"]

    def remap(v, lo, hi):
        n = nt.nodes.new("ShaderNodeMapRange")
        n.inputs["From Min"].default_value = lo
        n.inputs["From Max"].default_value = hi
        nt.links.new(v, n.inputs["Value"])
        return n.outputs["Result"]

    d, fan = attr("d"), attr("fan")
    dd = op("ADD", d, op("SUBTRACT", op("MULTIPLY", noise_tex(2.5), 0.4), 0.2))      # d made ragged
    col = nt.nodes.new("ShaderNodeValToRGB")
    els = col.color_ramp.elements
    stops = ((0.0, "251E19"), (0.35, "30271F"), (0.7, "3E3329"), (0.92, "57493B"), (1.1, "554636"), (1.5, "66563F"))
    els[0].position, els[0].color = 0.0, (*kit.hex_rgb(stops[0][1]), 1)
    els[1].position, els[1].color = 1.0, (*kit.hex_rgb(stops[-1][1]), 1)
    for pos, hx in stops[1:-1]:
        e = els.new(pos / 1.5)
        e.color = (*kit.hex_rgb(hx), 1)
    nt.links.new(op("DIVIDE", dd, 1.5), col.inputs["Fac"])
    clods = noise_tex(16.0, 6)
    fr = nt.nodes.new("ShaderNodeValToRGB")
    fr.color_ramp.elements[0].color = (0.55, 0.55, 0.55, 1)
    fr.color_ramp.elements[1].color = (1.3, 1.3, 1.3, 1)
    nt.links.new(clods, fr.inputs["Fac"])
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 0.75
    nt.links.new(col.outputs["Color"], mul.inputs["A"])
    nt.links.new(fr.outputs["Color"], mul.inputs["B"])
    nt.links.new(mul.outputs["Result"], bsdf.inputs["Base Color"])
    # wet at the bottom: smoother there
    rough = nt.nodes.new("ShaderNodeMapRange")
    rough.inputs["From Min"].default_value = 0.0
    rough.inputs["From Max"].default_value = 0.5
    rough.inputs["To Min"].default_value = 0.6
    rough.inputs["To Max"].default_value = 0.97
    nt.links.new(d, rough.inputs["Value"])
    nt.links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])
    # alpha: solid to the lip, then speckled spray, thinning outward and thickest on the fan side
    core = remap(dd, 1.3, 1.12)
    speck = remap(noise_tex(7.0, 4), 0.66, 0.58)                 # sparse flecks of thrown earth
    out = remap(d, 2.7, 1.15)
    spray = op("MULTIPLY", op("MULTIPLY", speck, out), op("POWER", fan, 2.0))   # thrown mostly one way
    spray = remap(spray, 0.12, 0.45)
    nt.links.new(op("MAXIMUM", core, spray), bsdf.inputs["Alpha"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.9
    bump.inputs["Distance"].default_value = 0.04
    nt.links.new(clods, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return haze(m)


def _turf_material():
    """Torn turf: grass on top, raw earth on the torn sides and underneath."""
    m = bpy.data.materials.get("turf")
    if m:
        return m
    m = bpy.data.materials.new("turf")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.95
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Normal"], sep.inputs[0])
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["From Min"].default_value = 0.3
    mr.inputs["From Max"].default_value = 0.6
    nt.links.new(sep.outputs["Z"], mr.inputs["Value"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs["A"].default_value = (*kit.hex_rgb("43372B"), 1)
    mix.inputs["B"].default_value = (*kit.hex_rgb("5A5F37"), 1)
    nt.links.new(mr.outputs["Result"], mix.inputs["Factor"])
    nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    return haze(m)


def _crater(stage, x, y, r, mats, seed=0, water=False):
    """A shell hole: a lip of thrown earth about a fifth of the radius high, a churned bowl of raw dark earth inside,
    clods and torn turf flaps flung over the lip, and a spray of earth across the grass out to three radii, thickest
    on the side the blast threw it. Older holes hold a little water. Laid over the ground (the ground is not cut)."""
    rnd = random.Random(seed * 31 + int(x * 7 + y * 13))
    h = 0.2 * r
    seg, rings = 56, 32
    blast = rnd.uniform(0, 2 * math.pi)
    bm = bmesh.new()
    lay = bm.verts.layers.float.new("d")
    fan_l = bm.verts.layers.float.new("fan")
    grid = []
    off = Vector((rnd.uniform(0, 50), rnd.uniform(0, 50), 0))
    for i in range(rings + 1):
        d = 3.0 * (i / rings) ** 1.35
        ring = []
        for j in range(seg if i else 1):
            ang = 2 * math.pi * j / seg
            wob = 1 + 0.14 * noise.noise(Vector((math.cos(ang) * 1.5, math.sin(ang) * 1.5, 0)) + off)
            dw = d * wob
            px, py = x + math.cos(ang) * dw * r, y + math.sin(ang) * dw * r
            if d < 0.95:
                z = 0.03 + h * (d / 0.95) ** 2.2
            else:
                z = 0.02 + h * math.exp(-((d - 0.95) / 0.32) ** 2)
            rough = noise.noise(Vector((px * 2.6, py * 2.6, 0)) + off) * 0.045 * r * (1 if 0.3 < d < 1.4 else 0.25)
            v = bm.verts.new((px, py, stage.height(px, py) + max(0.012, z + rough)))
            v[lay] = d
            v[fan_l] = 0.5 + 0.5 * math.cos(ang - blast)
            ring.append(v)
        grid.append(ring)
    centre = grid[0][0]
    for j in range(seg):
        bm.faces.new((centre, grid[1][j], grid[1][(j + 1) % seg]))
    for a, b in zip(grid[1:], grid[2:]):
        for j in range(seg):
            bm.faces.new((a[j], b[j], b[(j + 1) % seg], a[(j + 1) % seg]))
    me = bpy.data.meshes.new("crater")
    bm.to_mesh(me)
    bm.free()
    c = bpy.data.objects.new("crater", me)
    bpy.context.scene.collection.objects.link(c)
    for poly in me.polygons:
        poly.use_smooth = True
    sub = c.modifiers.new("sub", "SUBSURF")
    sub.levels = sub.render_levels = 1
    me.materials.append(_crater_material())
    # clods: broken chunks of every size, a few on the crest, most tumbled down the outer slope, thinning out into
    # the spray on the blast side; and a few flaps of torn turf
    cb = bmesh.new()
    tb = bmesh.new()

    def lip_z(px, py, dist):
        return stage.height(px, py) + 0.02 + h * math.exp(-((max(dist / r, 0.95) - 0.95) / 0.32) ** 2)

    for k in range(int(110 * r)):
        u = rnd.random()
        if u < 0.15:
            band, big = rnd.uniform(0.85, 1.05), 1.0           # the crest
        elif u < 0.7:
            band, big = rnd.uniform(1.05, 1.75), 0.9           # tumbled down the outer slope
        else:
            band, big = rnd.uniform(1.75, 2.7), 0.45           # out into the spray
        ang = blast + rnd.gauss(0, 1.0 if band > 1.75 else 1.6)
        dist = r * band
        px, py = x + math.cos(ang) * dist, y + math.sin(ang) * dist
        size = math.exp(rnd.uniform(math.log(0.025), math.log(0.2))) * big
        turf = rnd.random() < 0.07 and band < 1.6               # a sod of torn turf, half lying earth side up
        if turf:
            lx, ly, lz = size * 2.6 + 0.08, size * 1.9 + 0.06, rnd.uniform(0.035, 0.06)
            flip = math.pi * (rnd.random() < 0.5)
            rot = (Matrix.Rotation(rnd.uniform(0, 6.3), 4, "Z") @ Matrix.Rotation(flip + rnd.uniform(-0.6, 0.6), 4, "X"))
            base = Vector((px, py, lip_z(px, py, dist) + 0.03))
            tgt = tb
        else:
            lx, ly, lz = size, size * rnd.uniform(0.6, 1.0), size * rnd.uniform(0.45, 0.85)
            rot = (Matrix.Rotation(rnd.uniform(0, 6.3), 4, "Z") @ Matrix.Rotation(rnd.uniform(-0.6, 0.6), 4, "X")
                   @ Matrix.Rotation(rnd.uniform(-0.6, 0.6), 4, "Y"))
            base = Vector((px, py, lip_z(px, py, dist) + lz * 0.25))
            tgt = cb
        vs = [tgt.verts.new(base + rot @ q) for q in _broken_points(rnd, lx, ly, lz)]
        res = bmesh.ops.convex_hull(tgt, input=vs, use_existing_faces=False)
        hull = {g for g in res["geom"] if isinstance(g, bmesh.types.BMVert)}
        junk = [v for v in vs if v not in hull]
        if junk:
            bmesh.ops.delete(tgt, geom=junk, context="VERTS")
    for bmx, name, mat, smooth in ((cb, "clods", mats["earth"], False), (tb, "turf", _turf_material(), False)):
        mm = bpy.data.meshes.new(name)
        bmx.to_mesh(mm)
        bmx.free()
        for poly in mm.polygons:
            poly.use_smooth = smooth
        ob = bpy.data.objects.new(name, mm)
        bpy.context.scene.collection.objects.link(ob)
        mm.materials.append(mat)
    if water:                                          # an older hole: a little muddy water in the bottom
        bpy.ops.mesh.primitive_circle_add(vertices=32, radius=r * 0.32, fill_type="NGON",
                                          location=(x, y, stage.height(x, y) + 0.05))
        w = bpy.context.object
        for v in w.data.vertices:
            v.co.x *= 1 + 0.15 * noise.noise(Vector((v.co.x * 3, v.co.y * 3, 1)))
        wm = bpy.data.materials.get("puddle") or bpy.data.materials.new("puddle")
        wm.use_nodes = True
        wb = wm.node_tree.nodes["Principled BSDF"]
        wb.inputs["Base Color"].default_value = (*kit.hex_rgb("2B2A24"), 1)
        wb.inputs["Roughness"].default_value = 0.04
        w.data.materials.append(wm)
    return c


def _grass(stage, area, count, mats, seed=7, holes=(), line=None):
    """Tufts of long grass scattered near the camera, so the ground has depth at a low angle. With `line`
    (a, b, near, far, n) they grow instead in a band from `near` to `far` metres either side of the line a-b."""
    rnd = random.Random(seed)
    bm = bmesh.new()

    spots = []
    if line:                                   # clumps of every size along the line, with bare stretches between
        la, lb, near, far, n = line
        dd = (lb - la)
        sd = Vector((-dd.y, dd.x, 0)).normalized()
        for _ in range(n):
            c = la + dd * rnd.uniform(-0.03, 1.03) + sd * rnd.choice((-1, 1)) * rnd.uniform(near, far)
            r = rnd.uniform(0.08, 0.45)
            for _ in range(int(2 + 28 * rnd.random() ** 2.5)):
                ang, rr = rnd.uniform(0, 2 * math.pi), r * math.sqrt(rnd.random())
                spots.append((c.x + math.cos(ang) * rr, c.y + math.sin(ang) * rr))
    else:
        (x0, y0), (x1, y1) = area
        spots = [(rnd.uniform(x0, x1), rnd.uniform(y0, y1)) for _ in range(count)]

    for x, y in spots:
        if road_distance(x, y, ROAD) < 3.0:
            continue
        if any((x - hx) ** 2 + (y - hy) ** 2 < (1.3 * hr) ** 2 for hx, hy, hr in holes):   # no grass in a fresh crater
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
        "crown": haze(kit.material("crown", kit.hex_rgb("33421F"), rough=0.9, noise=0.5, noise_scale=3.0, bump=0.8)),
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
        _tree(f"lone{k}", Vector((x, y, stage.height(x, y))), rnd.uniform(10, 14), seed * 7 + k, mats, near=True)

    _stone_wall(stage, (8.0, 28.0), (60.0, 41.0), mats, seed)
    _stone_wall(stage, (-6.0, 34.0), (-36.0, 30.0), mats, seed + 1)
    _farm(stage, (34.0, 66.0), math.radians(14), mats)
    holes = ((6, -6, 1.6), (-12, 14, 2.2), (25, 12, 1.4), (-28, -10, 1.9), (40, 22, 2.4))
    for k, (x, y, r) in enumerate(holes):
        _crater(stage, x, y, r, mats, seed, water=k in (1, 3))
    _grass(stage, ((-45, -40), (55, 30)), 9000, mats, seed, holes)
    _clothe_crowns()
    return stage
