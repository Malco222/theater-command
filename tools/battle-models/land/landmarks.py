"""National markings on the vehicles, for the pictures only.

The game paints its own markings where the model's `marks` say (a tag, a place, a size, a facing, the part it rides).
So that a review picture does not show bare machines, the same list is drawn here as thin painted shapes laid on the
plate: each nation's plain period marking, never a party emblem. Nothing here goes into the game's files.
"""
import math

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import tessellate_polygon

from marks import BLACK, DISC, RED, WHITE, YELLOW, rect, ring, star

FR_BLUE = "24407A"


def shapes(tag):
    """The layers of a marking, lowest first, in a square from -1 to 1 (v is up)."""
    if tag == "GER":            # the beam cross: black, edged white
        return [rect(-1, 1, -0.3, 0.3, WHITE) + rect(-0.3, 0.3, -1, 1, WHITE), rect(-1, 1, -0.17, 0.17, BLACK) + rect(-0.17, 0.17, -1, 1, BLACK)]
    if tag == "USA":            # the plain white star
        return [star(1.0, WHITE)]
    if tag == "SOV":            # the red star
        return [star(1.0, DISC)]
    if tag == "UK":             # the armoured corps' flash: red, white, red
        return [rect(-1, -0.34, -0.6, 0.6, RED) + rect(-0.34, 0.34, -0.6, 0.6, WHITE) + rect(0.34, 1, -0.6, 0.6, RED)]
    if tag == "FRA":            # the roundel: blue middle, white, red outside
        return [ring(0.0, 1.0, RED), ring(0.0, 0.66, WHITE), ring(0.0, 0.33, FR_BLUE)]
    if tag == "JAP":            # the plain red disc on white
        return [rect(-1, 1, -0.66, 0.66, WHITE), ring(0.0, 0.4, DISC)]
    if tag == "ITA":            # a company's tactical bar: a red block, one white stripe
        return [rect(-0.8, 0.8, -0.55, 0.55, RED), rect(-0.12, 0.12, -0.55, 0.55, WHITE)]
    if tag == "POL":
        return []
    return []


def put(model, ob, arm):
    """Lays the model's markings on its skin (as it stands at rest), each riding the part it is on. Returns the objects made."""
    if not model.marks:
        return []
    tree = BVHTree.FromPolygons([tuple(v) for v in model.verts], [tuple(f) for f in model.faces])
    verts, faces, cols, groups = [], [], [], []
    for mk in model.marks:
        for hand in ((1.0, -1.0) if mk.get("both") else (1.0,)):
            at = Vector((mk["at"][0], -mk["at"][1] * hand, mk["at"][2]))            # (the file's y is to the right)
            n = Vector((mk["facing"][0], -mk["facing"][1] * hand, mk["facing"][2])).normalized()
            up = Vector((1, 0, 0)) if abs(n.z) > 0.7 else Vector((0, 0, 1))
            v = (up - n * up.dot(n)).normalized()
            u = v.cross(n)
            r = mk["radius"]
            for layer, polys in enumerate(shapes(mk["tag"])):
                for col, poly in polys:
                    flat = [Vector((a, b, 0.0)) for (a, b) in poly]
                    tris = [[flat[i] for i in t] for t in tessellate_polygon([flat])]
                    for tri in tris:
                        idx = []
                        for q in tri:
                            p = at + (u * q.x + v * q.y) * r
                            hit = tree.ray_cast(p + n * 0.25, -n, 0.5)
                            if hit[0] is not None and hit[1].dot(n) > 0.5:
                                p = hit[0]
                            idx.append(len(verts))
                            verts.append(p + n * (0.003 + 0.0015 * layer))
                            groups.append(mk.get("part", "hull"))
                        a_, b_, c_ = verts[idx[0]], verts[idx[1]], verts[idx[2]]
                        if (b_ - a_).cross(c_ - a_).dot(n) < 0:
                            idx.reverse()
                        faces.append(idx)
                        cols.append(col)
    if not faces:
        return []
    me = bpy.data.meshes.new(model.name + "_marks")
    me.from_pydata([tuple(p) for p in verts], [], faces)
    used = sorted(set(cols))
    for c in used:
        name = "mark:" + c
        mat = bpy.data.materials.get(name)
        if mat is None:
            mat = bpy.data.materials.new(name)
            mat.use_nodes = True
            b = mat.node_tree.nodes["Principled BSDF"]
            rgb = tuple(((int(c[i:i + 2], 16) / 255.0) ** 2.2) * 0.85 for i in (0, 2, 4))        # (a little dulled: paint in the weather)
            b.inputs["Base Color"].default_value = (*rgb, 1.0)
            b.inputs["Roughness"].default_value = 0.7
        me.materials.append(mat)
    me.polygons.foreach_set("material_index", [used.index(c) for c in cols])
    me.update()
    o = bpy.data.objects.new(model.name + "_marks", me)
    bpy.context.scene.collection.objects.link(o)
    o.visible_shadow = False
    for part in sorted(set(groups)):
        g = o.vertex_groups.new(name=part)
        g.add([i for i, p in enumerate(groups) if p == part], 1.0, "REPLACE")
    o.parent = arm
    mod = o.modifiers.new("rig", "ARMATURE")
    mod.object = arm
    return [o]
