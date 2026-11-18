"""National markings on the aircraft, for the pictures only.

The game paints its own markings where the model's `marks` say (a tag, a place, a size, a facing). So that a review
picture does not show bare machines, the same list is drawn here as thin painted shapes laid on the skin: each nation's
plain period marking, never a party emblem. Nothing here goes into the game's files.
"""
import math

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import tessellate_polygon

BLUE, RED, WHITE, YELLOW, BLACK = "1C2C55", "A82A26", "E6E4DA", "D9B03A", "141414"
US_BLUE, DISC = "1B2A4A", "B0261E"


def ring(r0, r1, col, seg=32):
    out = []
    steps = max(1, int(math.ceil((r1 - r0) / 0.3)))
    for j in range(steps):
        a, b = r0 + (r1 - r0) * j / steps, r0 + (r1 - r0) * (j + 1) / steps
        for k in range(seg):
            t0, t1 = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
            quad = [(a * math.cos(t0), a * math.sin(t0)), (b * math.cos(t0), b * math.sin(t0)), (b * math.cos(t1), b * math.sin(t1)), (a * math.cos(t1), a * math.sin(t1))]
            out.append((col, quad if a > 1e-6 else quad[1:]))
    return out


def rect(u0, u1, v0, v1, col, cell=0.3):
    out = []
    nu, nv = max(1, int(math.ceil((u1 - u0) / cell))), max(1, int(math.ceil((v1 - v0) / cell)))
    for i in range(nu):
        for j in range(nv):
            a, b = u0 + (u1 - u0) * i / nu, u0 + (u1 - u0) * (i + 1) / nu
            c, d = v0 + (v1 - v0) * j / nv, v0 + (v1 - v0) * (j + 1) / nv
            out.append((col, [(a, c), (b, c), (b, d), (a, d)]))
    return out


def star(r, col, inner=0.382):
    pts = []
    for k in range(10):
        a = math.pi / 2 + 2 * math.pi * k / 10
        rr = r if k % 2 == 0 else r * inner
        pts.append((rr * math.cos(a), rr * math.sin(a)))
    out = []
    for k in range(10):
        a, b = pts[k], pts[(k + 1) % 10]
        for j in range(3):
            t0, t1 = j / 3.0, (j + 1) / 3.0
            quad = [(a[0] * t0, a[1] * t0), (a[0] * t1, a[1] * t1), (b[0] * t1, b[1] * t1), (b[0] * t0, b[1] * t0)]
            out.append((col, quad if j else quad[1:3] + [(0.0, 0.0)]))
    return out


def shapes(tag):
    """The layers of a marking, lowest first, in a square from -1 to 1 (v is up on a side, forward on a wing)."""
    if tag == "GER":
        return [rect(-1, 1, -0.24, 0.24, WHITE) + rect(-0.24, 0.24, -1, 1, WHITE), rect(-1, 1, -0.15, 0.15, BLACK) + rect(-0.15, 0.15, -1, 1, BLACK)]
    if tag == "UK-A1":
        return [ring(0.0, 1.0, YELLOW), ring(0.0, 0.84, BLUE), ring(0.0, 0.55, WHITE), ring(0.0, 0.27, RED)]
    if tag == "UK":
        return [ring(0.0, 1.0, BLUE), ring(0.0, 0.62, WHITE), ring(0.0, 0.3, RED)]
    if tag == "UK-B":
        return [ring(0.0, 1.0, BLUE), ring(0.0, 0.42, RED)]
    if tag == "ITA":                                    # a company's sign: its colour, a white bar for the platoon
        return [rect(-1, 1, -0.6, 0.6, RED), rect(-0.14, 0.14, -0.6, 0.6, WHITE)]
    if tag == "FRA":
        return [ring(0.0, 1.0, RED), ring(0.0, 0.66, WHITE), ring(0.0, 0.33, "2B4C8C")]
    if tag == "UK-TANK":                                # the red-white-red flash British tanks wore, 1941 and 1942
        return [rect(-1, -0.333, -0.56, 0.56, RED) + rect(-0.333, 0.333, -0.56, 0.56, WHITE) + rect(0.333, 1, -0.56, 0.56, RED)]
    if tag == "UK-FIN":
        return [rect(0.25, 0.75, -0.9, 0.9, RED) + rect(-0.25, 0.25, -0.9, 0.9, WHITE) + rect(-0.75, -0.25, -0.9, 0.9, BLUE)]
    if tag == "JAP-PLAIN":
        return [ring(0.0, 1.0, DISC)]
    if tag == "USA":
        return [ring(0.0, 1.0, US_BLUE), star(0.98, WHITE)]
    if tag == "USA-BAR":
        return [ring(0.0, 1.0, US_BLUE) + rect(-2.05, -0.9, -0.33, 0.33, US_BLUE) + rect(0.9, 2.05, -0.33, 0.33, US_BLUE),
                rect(-1.95, -0.95, -0.24, 0.24, WHITE) + rect(0.95, 1.95, -0.24, 0.24, WHITE), star(0.98, WHITE)]
    if tag == "SOV":
        return [star(1.0, DISC)]
    return []


def put(model, ob, arm):
    """Lays the model's markings on its skin (as it stands at rest), each riding the part it is on. Returns the objects made."""
    if not model.marks:
        return []
    tree = BVHTree.FromPolygons([tuple(v) for v in model.verts], [tuple(f) for f in model.faces])
    y_cut = abs(model.parts[model.part_at["wing.port"]].pivot.y)
    x_cut = model.parts[model.part_at["tail"]].pivot.x
    verts, faces, cols, groups = [], [], [], []
    for mk in model.marks:
        sides = (1.0, -1.0) if mk.get("both") else (1.0,)
        for hand in sides:
            at = Vector((mk["at"][0], -mk["at"][1] * hand, mk["at"][2]))            # (the file's y is to the right)
            n = Vector((mk["facing"][0], -mk["facing"][1] * hand, mk["facing"][2])).normalized()
            up = Vector((1, 0, 0)) if abs(n.z) > 0.7 else Vector((0, 0, 1))
            v = (up - n * up.dot(n)).normalized()
            u = v.cross(n)
            flip = -1.0 if (mk["tag"] == "UK-FIN" and u.x < 0) else 1.0
            part = "tail" if at.x < x_cut else ("wing.port" if at.y > y_cut else "wing.starboard" if at.y < -y_cut else "plane")
            r = mk["radius"]
            for layer, polys in enumerate(shapes(mk["tag"])):
                for col, poly in polys:
                    # (cut small before it is laid on her: a big flat piece dips under a round fuselage in its middle,
                    #  and her skin comes through it)
                    flat = [Vector((a * flip, b, 0.0)) for (a, b) in poly]
                    tris = [[flat[i] for i in t] for t in tessellate_polygon([flat])]
                    longest = max(((t[i] - t[(i + 1) % 3]).length for t in tris for i in range(3)), default=0.0) * r
                    for _ in range(min(4, max(0, math.ceil(math.log2(max(longest / 0.13, 1.0)))))):
                        tris = [piece for (a, b, c) in tris for ab, bc, ca in [((a + b) / 2, (b + c) / 2, (c + a) / 2)]
                                for piece in ((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca))]
                    for tri in tris:
                        idx = []
                        for q in tri:
                            p = at + (u * q.x + v * q.y) * r
                            hit = tree.ray_cast(p + n * 0.6, -n, 1.2)
                            if hit[0] is not None and hit[1].dot(n) > 0.3:
                                p = hit[0]
                            idx.append(len(verts))
                            verts.append(p + n * (0.012 + 0.004 * layer))
                            groups.append(part)
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
            rgb = tuple(((int(c[i:i + 2], 16) / 255.0) ** 2.2) for i in (0, 2, 4))
            b.inputs["Base Color"].default_value = (*rgb, 1.0)
            b.inputs["Roughness"].default_value = 0.6
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
