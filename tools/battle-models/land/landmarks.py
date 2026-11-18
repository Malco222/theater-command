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

import brushhand
from marks import BLACK, DISC, RED, WHITE, YELLOW, rect, ring, star

FR_BLUE = "24407A"


def shapes(tag, shown=None):
    """The layers of a marking, lowest first, in a square from -1 to 1 (v is up). shown: for a nation whose marking
    changed with the months (works.MARK_MONTHS), which of them to draw."""
    if tag == "GER":            # the beam cross: black, edged white
        return [rect(-1, 1, -0.3, 0.3, WHITE) + rect(-0.3, 0.3, -1, 1, WHITE), rect(-1, 1, -0.17, 0.17, BLACK) + rect(-0.17, 0.17, -1, 1, BLACK)]
    if tag == "USA":            # the plain white star
        return [star(1.0, WHITE)]
    if tag == "SOV":            # the red star
        return [star(1.0, DISC)]
    if tag == "UK":             # by the month: bare paint, then the recognition flash (white, red, white), then the white Allied star
        if shown == "none":
            return []
        if shown == "star":
            return [star(1.0, WHITE)]
        return [rect(-1, -0.34, -0.6, 0.6, WHITE) + rect(-0.34, 0.34, -0.6, 0.6, RED) + rect(0.34, 1, -0.6, 0.6, WHITE)]
    if tag == "FRA":            # the roundel: blue middle, white, red outside
        return [ring(0.0, 1.0, RED), ring(0.0, 0.66, WHITE), ring(0.0, 0.33, FR_BLUE)]
    if tag == "JAP":            # the plain red disc on white
        return [rect(-1, 1, -0.66, 0.66, WHITE), ring(0.0, 0.4, DISC)]
    if tag == "ITA":            # a company's tactical bar: a red block, one white stripe
        return [rect(-0.8, 0.8, -0.55, 0.55, RED), rect(-0.12, 0.12, -0.55, 0.55, WHITE)]
    if tag == "POL":
        return []
    return []


# The crew's own paint (b477): a little thinner and duller than the factory's marking, about 200,196,186 in the sun.
NAME_PAINT = {"white": "D6D2C8", "black": "1E1D1B"}


def crew_mark(model, tree, name, rings, verts, faces, cols, groups):
    """The crew's name or rings where the file's `name_place` says, for the pictures: `name` a word (a Soviet slogan
    given in Latin letters is put into Russian ones), `rings` how many rings a German barrel wears."""
    place = model.info.get("file", {}).get("name_place")
    if not place:
        return
    col = NAME_PAINT[place["ink"]["use"]]
    back = lambda p: Vector((p[0], -p[1], p[2]))            # (the file's y is to the right)
    if place["kind"] == "name" and name:
        text = name.replace("_", " ")
        if model.info.get("tag") == "SOV" and text.isascii():
            text = brushhand.russian(text)
        down = place.get("writing") == "down"
        words = text.split()
        if place.get("lines", 1) > 1 and len(words) > 1:    # a slogan in two lines, broken as near its middle as its words allow
            cut = min(range(1, len(words)), key=lambda k: abs(len(" ".join(words[:k])) - len(" ".join(words[k:]))))
            rows = [" ".join(words[:cut]), " ".join(words[cut:])]
        else:
            rows = [text]
        for pl in place["places"]:
            at, n, run, up = back(pl["at"]), back(pl["facing"]).normalized(), back(pl["along"]).normalized(), back(pl["up"]).normalized()
            laid = [brushhand.word(r, seed=model.name + pl["side"], down=down) for r in rows]
            h = min([place["high"]] + [pl["long"] / max(0.5, long) for _, long in laid])
            for k, (quads, long) in enumerate(laid):
                if down:
                    ox, oy = 0.0, long * h / 2
                else:
                    ox = -long * h / 2
                    oy = -h / 2 + (len(rows) - 1) * 0.675 * h - k * 1.35 * h
                for q in quads:
                    idx = []
                    for (a, b) in q:
                        p = at + run * (ox + a * h) + up * (oy + b * h)
                        hit = tree.ray_cast(p + n * 0.25, -n, 0.5)
                        if hit[0] is not None and hit[1].dot(n) > 0.4:
                            p = hit[0]
                        idx.append(len(verts))
                        verts.append(p + n * 0.0045)
                        groups.append(pl.get("part", "hull"))
                    a_, b_, c_ = verts[idx[0]], verts[idx[1]], verts[idx[2]]
                    if (b_ - a_).cross(c_ - a_).dot(n) < 0:
                        idx.reverse()
                    faces.append(idx)
                    cols.append(col)
    if place["kind"] == "rings" and rings:
        for rg in place.get("rings", []):
            at, d = back(rg["at"]), back(rg["toward"]).normalized()
            u = d.cross(Vector((0, 0, 1))).normalized()
            w = u.cross(d)
            seg = 20
            for k in range(min(rings, rg["most"])):
                c, r = at + d * (k * rg["step"]), rg["radius"][k] + 0.0025
                base = len(verts)
                for e in (-0.5, 0.5):
                    for j in range(seg):
                        t = 2 * math.pi * j / seg
                        verts.append(c + d * (e * rg["wide"]) + (u * math.cos(t) + w * math.sin(t)) * r)
                        groups.append(rg["part"])
                for j in range(seg):
                    quad = [base + j, base + (j + 1) % seg, base + seg + (j + 1) % seg, base + seg + j]
                    a_, b_, c_ = verts[quad[0]], verts[quad[1]], verts[quad[2]]
                    if (b_ - a_).cross(c_ - a_).dot(a_ - c) < 0:
                        quad.reverse()
                    faces.append(quad)
                    cols.append(col)


def put(model, ob, arm, burn=False, name=None, rings=0):
    """Lays the model's markings on its skin (as it stands at rest), each riding the part it is on. Returns the objects made.
    burn: the marking darkens as its object's colour (red, 0 to 1) says she has burnt, as works.land_look(burn=...) does.
    name, rings: her crew's own mark too (b477), where her file has a place for it: a word, or a count of barrel rings."""
    if not model.marks and not (name or rings):
        return []
    tree = BVHTree.FromPolygons([tuple(v) for v in model.verts], [tuple(f) for f in model.faces])
    verts, faces, cols, groups = [], [], [], []
    if name or rings:
        crew_mark(model, tree, name, rings, verts, faces, cols, groups)
    for mk in model.marks:
        for hand in ((1.0, -1.0) if mk.get("both") else (1.0,)):
            at = Vector((mk["at"][0], -mk["at"][1] * hand, mk["at"][2]))            # (the file's y is to the right)
            n = Vector((mk["facing"][0], -mk["facing"][1] * hand, mk["facing"][2])).normalized()
            up = Vector((1, 0, 0)) if abs(n.z) > 0.7 else Vector((0, 0, 1))
            v = (up - n * up.dot(n)).normalized()
            u = v.cross(n)
            r = mk["radius"]
            for layer, polys in enumerate(shapes(mk["tag"], mk.get("shown"))):
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
        name = ("burnt mark:" if burn else "mark:") + c
        mat = bpy.data.materials.get(name)
        if mat is None:
            mat = bpy.data.materials.new(name)
            mat.use_nodes = True
            b = mat.node_tree.nodes["Principled BSDF"]
            rgb = tuple(((int(c[i:i + 2], 16) / 255.0) ** 2.2) * 0.85 for i in (0, 2, 4))        # (a little dulled: paint in the weather)
            b.inputs["Base Color"].default_value = (*rgb, 1.0)
            b.inputs["Roughness"].default_value = 0.7
            if burn:
                nt = mat.node_tree
                info = nt.nodes.new("ShaderNodeObjectInfo")
                sep = nt.nodes.new("ShaderNodeSeparateXYZ")
                nt.links.new(info.outputs["Color"], sep.inputs[0])
                far = nt.nodes.new("ShaderNodeMapRange")
                far.inputs["From Min"].default_value, far.inputs["From Max"].default_value = 0.3, 1.0
                far.inputs["To Min"].default_value, far.inputs["To Max"].default_value = 0.0, 0.86
                nt.links.new(sep.outputs[0], far.inputs["Value"])
                mx = nt.nodes.new("ShaderNodeMix")
                mx.data_type = "RGBA"
                mx.inputs[6].default_value = (*rgb, 1.0)
                mx.inputs[7].default_value = (0.04, 0.032, 0.026, 1.0)
                nt.links.new(far.outputs["Result"], mx.inputs[0])
                nt.links.new(mx.outputs[2], b.inputs["Base Color"])
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
