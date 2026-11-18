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
    if tag == "_":              # built for any nation: the picture's stand-in for her owner's own mark, if one is set
        return list(OWN)
    return []


# A vehicle built for any nation (tag `_`) carries no marking of her own: the game draws each small nation's own where
# her marks say. A picture may stand one in: these are its layers, lowest first (`own_rings`).
OWN = []


def own_rings(*cols):
    """The picture's stand-in for a small nation's own mark on the vehicles built for any nation: rings of these
    colours from the outside in (Iraq's: black, white, green). None: bare paint again."""
    OWN[:] = [ring(0.0, 1.0 - i / len(cols), c) for i, c in enumerate(cols)]


def own_marking(layers):
    """The stand-in as the art director's table gives a country's mark (unit-table.json, `insignia`): layers lowest
    first in a square from -1 to 1 (v is up), each a `ring` [r0, r1], a `tri` [r, turn in degrees], a `star`
    [r, inner, points] or a `rect` [u0, u1, v0, v1] with its colour `c`. Neighbours of one colour are laid as one
    layer (Sweden's three crowns are 24 gold blocks)."""
    out, last = [], None
    for lay in layers or []:
        c = lay["c"]
        if "ring" in lay:
            polys = ring(lay["ring"][0], lay["ring"][1], c)
        elif "rect" in lay:
            polys = rect(*lay["rect"], c)
        elif "star" in lay:
            r, inner, n = (list(lay["star"]) + [0.382, 5])[:3]
            pts = [((r if k % 2 == 0 else r * inner) * math.cos(math.pi / 2 + math.pi * k / n), (r if k % 2 == 0 else r * inner) * math.sin(math.pi / 2 + math.pi * k / n)) for k in range(2 * int(n))]
            polys = star(r, c, inner) if int(n) == 5 else [(c, [(0.0, 0.0), pts[k], pts[(k + 1) % len(pts)]]) for k in range(len(pts))]
        elif "tri" in lay:
            r, turn = (list(lay["tri"]) + [0.0])[:2]
            pts = [(r * math.cos(math.radians(90.0 + turn + 120.0 * k)), r * math.sin(math.radians(90.0 + turn + 120.0 * k))) for k in range(3)]
            polys = [(c, [(0.0, 0.0), pts[k], pts[(k + 1) % 3]]) for k in range(3)]
        else:
            continue
        if out and last == c:
            out[-1] = out[-1] + polys
        else:
            out.append(polys)
        last = c
    OWN[:] = out


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
    text = name.replace("_", " ") if name else None
    if text and place.get("script") == "japanese":          # her characters, or nothing: never Latin letters on her
        text = brushhand.japanese(text)
        if text is None:
            print("crew_mark: no characters are drawn for %r: %s wears no name" % (name, model.name))
    if place["kind"] == "name" and text:
        if model.info.get("tag") == "SOV" and text.isascii():
            text = brushhand.russian(text)
        down = place.get("writing") == "down"
        for pl in place["places"]:
            at, n, run, up = back(pl["at"]), back(pl["facing"]).normalized(), back(pl["along"]).normalized(), back(pl["up"]).normalized()
            if place.get("script") == "japanese":
                got = brushhand.fit_down(text, pl["long"], place["high"], place.get("least", brushhand.LEAST), place.get("high_one"))
            else:
                got = brushhand.fit(text, pl["long"], place["high"], pl.get("lines", place.get("lines", 1)), place.get("least", brushhand.LEAST), place.get("high_two"))
            if got is None:
                print("crew_mark: %r does not fit %s's %s side (%.2f m): she wants a shorter name" % (text, model.name, pl["side"], pl["long"]))
                continue
            h, rows = got
            laid = [brushhand.word(r, seed=model.name + pl["side"], down=down) for r in rows]
            h = min([h] + [pl["long"] / max(0.5, long) for _, long in laid])
            coat = 0                                        # (paint laid over paint lies ON it: no two pieces of a letter in one plane, or the overlap renders as a dark spot)
            for k, (quads, long) in enumerate(laid):
                if down:
                    ox, oy = 0.0, long * h / 2
                else:
                    ox = -long * h / 2
                    oy = -h / 2 + (len(rows) - 1) * 0.675 * h - k * 1.35 * h
                for q in quads:
                    idx = []
                    coat += 1
                    for (a, b) in q:
                        p = at + run * (ox + a * h) + up * (oy + b * h)
                        hit = tree.ray_cast(p + n * 0.25, -n, 0.5)
                        if hit[0] is not None and hit[1].dot(n) > 0.4:
                            p = hit[0]
                        idx.append(len(verts))
                        verts.append(p + n * (0.0045 + 0.00012 * (coat % 16)))
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


# Where rain has run down her (the art director, 2026-11-18): a few soft dark marks on a side, each from under a top
# edge or something that stands off the plate (a hatch, a port, a box), unlike in length and spacing; six to ten on a
# hull's side, never a hundred, or a big plate reads as planks. Rain is soft: each a few centimetres wide at its top with
# blurred edges, fading downward; a thin dark even one reads as a seam. For the pictures only: the games do their own dirt.
RAIN_PARTS = ("hull", "turret")
RAIN_DARK = 0.28                                            # how much of the coat's light the darkest mark takes, down its middle
RAIN_ACROSS = ((-1.0, 0.0), (-0.55, 0.22), (-0.2, 0.85), (0.0, 1.0), (0.2, 0.85), (0.55, 0.22), (1.0, 0.0))     # soft at both edges: (where across, how dark)


def rain_marks(model, tree, verts, faces, cols, groups, runs):
    """Finds where rain would run on her sides as she stands, and lays each mark as a soft strip on the skin: `runs`
    gets how dark each new point is (0 at a strip's edges and foot)."""
    import random
    from works import COATS
    m = model
    coat = {m.paint_at[p] for p in m.paints if p.split(".")[0] in COATS}
    part_name = {i: k for k, i in m.part_at.items()}
    ok_part = {m.part_at[p] for p in RAIN_PARTS if p in m.part_at}
    if not ok_part or not coat:
        return
    xs = [p[0] for p in m.verts]
    zs = [p[2] for p in m.verts]
    x_lo, z_lo = min(xs), min(zs)
    sx, sz = 0.03, 0.02
    nx, nz = int((max(xs) - x_lo) / sx) + 1, int((max(zs) - z_lo) / sz) + 1
    rng = random.Random("rain:" + model.name)
    keep_off = []                                           # her marking and her crew's name keep their own room
    for mk in m.marks:
        keep_off.append((mk["at"][0], mk["at"][2], mk["radius"] + 0.05, mk["radius"] + 0.05, None if mk.get("both") else (1.0 if -mk["at"][1] > 0 else -1.0)))
    for pl in model.info.get("file", {}).get("name_place", {}).get("places", []):
        keep_off.append((pl["at"][0], pl["at"][2], pl["long"] / 2 + 0.05, 0.2, 1.0 if pl["side"] == "left" else -1.0))
    for s in (1.0, -1.0):
        def look(x, z):
            hit = tree.ray_cast(Vector((x, s * 6.0, z)), Vector((0.0, -s, 0.0)), 6.0)
            if hit[0] is None:
                return None
            return hit[0], hit[1], m.fpart[hit[2]], m.fpaint[hit[2]], hit[3]
        plate = lambda g: g is not None and g[2] in ok_part and g[3] in coat and g[1].y * s > 0.5 and abs(g[1].z) < 0.75
        same = lambda a, b: a[2] == b[2] and a[1].dot(b[1]) > 0.97 and abs((a[0] - b[0]).dot(b[1])) < 0.004      # two looks at one flat plate

        def under(g, ref):
            """Where a mark lies at this look: on the plate itself, or on the plate under a small thing standing on
            it (a rivet's head, a weld, a hinge: rain runs round them); None where the plate ends."""
            if g is None or g[2] != ref[2]:
                return None
            if plate(g) and same(g, ref):
                return g[0]
            off = (g[0] - ref[0]).dot(ref[1])
            if -0.004 < off < 0.02:
                return g[0] - Vector((0.0, off / ref[1].y, 0.0))
            return None

        starts = []
        for i in range(nx + 1):
            col = [look(x_lo + i * sx, z_lo + j * sz) for j in range(nz + 1)]
            j = nz
            while j > 0:
                if not plate(col[j]):
                    j -= 1
                    continue
                top = j                                     # a run of one plate, from its top down
                while j > 0 and plate(col[j - 1]) and same(col[j - 1], col[j]):
                    j -= 1
                room = (top - j) * sz
                j -= 1
                if room < 0.12:
                    continue
                g = col[top]
                k = top + 1                                 # what is over it, past the rounding of its own edge
                while k <= nz and k <= top + 2 and plate(col[k]) and not same(col[k], g) and abs(col[k][4] - g[4]) < 0.012:
                    k += 1
                over = col[k] if k <= nz else None
                if over is None or over[4] > g[4] + 0.03:
                    weight = 1.0                            # a top edge: sky over it, or the next plate set well back
                elif over[4] < g[4] - 0.008 or over[3] not in coat:
                    # something stands off the plate: a hatch's rim, a port, a rail, a lamp's foot. A small thing only:
                    # under a big overhang (a sponson's floor, a stowage bin) the plate is out of the rain
                    if not any(b is not None and plate(b) and abs((b[0] - g[0]).dot(g[1])) < 0.012 for b in col[k + 1:k + 8]):
                        continue
                    weight = 4.0
                else:
                    continue                                # only a bend between two plates
                # rain has to reach it: nothing over her side there (a track guard, a sponson, a bin) for a man's height up
                if tree.ray_cast(g[0] + g[1] * 0.06 + Vector((0.0, 0.0, 0.02)), Vector((0.0, s * 0.2, 1.0)).normalized(), 2.0)[0] is not None:
                    continue
                # (a tall plate's top is likelier to carry one than a scrap's: on a half-track's leaning side most
                # of the marks would else fall on her bonnet's edges and lockers, where nobody sees them)
                starts.append((weight * min(2.0, room / 0.25), i, top, room, g[2]))
        for on in sorted(ok_part):
            pool = [c for c in starts if c[4] == on]
            if not pool:
                continue
            span = (max(c[1] for c in pool) - min(c[1] for c in pool)) * sx
            most = 10 if part_name[on] == "hull" else 4
            want = max(2, min(most, int(round(span * 1.5)) + rng.randint(-1, 1)))
            taken, spare, apart, seen = [], [], (0.3, 0.7), 0
            while seen < want and len(taken) < most:        # (seen: those long enough to read; a scrap of a mark does not count)
                if not pool:
                    # a short hull side (a half-track's body) holds four at that spacing: the art director asks six
                    # to ten a side, so the starts passed over are tried again, closer together
                    if part_name[on] != "hull" or seen >= 6 or not spare or apart[0] < 0.3:
                        break
                    pool, spare, apart, want = spare, [], (0.2, 0.34), min(want, 7)
                pick = rng.uniform(0.0, sum(c[0] for c in pool))
                for k, c in enumerate(pool):
                    pick -= c[0]
                    if pick <= 0.0:
                        break
                cand = pool.pop(k)
                _, i, j, room, _ = cand
                x0, z0 = x_lo + i * sx + rng.uniform(-0.012, 0.012), z_lo + j * sz + 0.006
                if any(abs(x0 - tx) < rng.uniform(*apart) for tx in taken):
                    spare.append(cand)
                    continue
                long = min(room * rng.uniform(0.55, 1.0), rng.uniform(0.14, 0.6))
                wide, dark = rng.uniform(0.09, 0.19), rng.uniform(0.5, 1.0)
                if any((hand is None or hand == s) and abs(x0 - kx) < rx + wide / 2 and kz - rz < z0 and z0 - long < kz + rz for kx, kz, rx, rz, hand in keep_off):
                    continue
                n = max(3, int(long / 0.025))
                for wide in (wide, wide * 0.6):             # (a narrower one where a fitting stands in a broad one's way)
                    rows, x, ref = [], x0, None
                    for k in range(n + 1):
                        t = k / n
                        hw = wide / 2 * (1.0 - 0.35 * t)
                        five = [look(x + d * hw, z0 - t * long) for d, _ in RAIN_ACROSS]
                        if ref is None and plate(five[len(five) // 2]):
                            ref = five[len(five) // 2]              # the plate the mark runs down
                        laid = [under(g, ref) for g in five] if ref is not None else [None]
                        if any(p is None for p in laid) or abs(laid[0].y - laid[-1].y) > 0.025:
                            if rows:
                                break
                            ref = None
                            continue                        # (the rounding of the edge it starts under)
                        rise = (0.3, 0.75)[len(rows)] if len(rows) < 2 else 1.0
                        rows.append([(p + ref[1] * 0.002, dark * rise * (1.0 - t) ** 1.15 * a) for p, (_, a) in zip(laid, RAIN_ACROSS)])
                        x += rng.uniform(-0.002, 0.002)
                    if len(rows) >= 5:
                        break
                if len(rows) < 5:
                    continue
                taken.append(x0)
                seen += len(rows) * long / n >= 0.2
                base, nc = len(verts), len(RAIN_ACROSS)
                for row in rows:
                    for p, a in row:
                        verts.append(p)
                        groups.append(part_name[on])
                        runs[len(verts) - 1] = a
                for k in range(len(rows) - 1):
                    for c in range(nc - 1):
                        q = [base + nc * k + c, base + nc * k + c + 1, base + nc * (k + 1) + c + 1, base + nc * (k + 1) + c]
                        if (verts[q[1]] - verts[q[0]]).cross(verts[q[2]] - verts[q[0]]).y * s < 0:
                            q.reverse()
                        faces.append(q)
                        cols.append("rain")


def rain_strength(model):
    """How dark her marks are down their middles. On a grey coat a mark of the same depth reads about half as
    strongly as on a green or a yellow one (measured on the grey half-track beside the Sexton): it takes half as
    much again."""
    import rigid
    rgb = rigid.hex_rgb(model.paints["upper"])
    grey = (max(rgb) - min(rgb)) / max(max(rgb), 1e-6) < 0.4
    return RAIN_DARK * (1.5 if grey else 1.0)


def _rain_material(dark=RAIN_DARK):
    mat = bpy.data.materials.get("rain mark %.3f" % dark)
    if mat is None:
        mat = bpy.data.materials.new("rain mark %.3f" % dark)
        mat.use_nodes = True
        nt = mat.node_tree
        for nd in list(nt.nodes):
            if nd.type != "OUTPUT_MATERIAL":
                nt.nodes.remove(nd)
        out = next(nd for nd in nt.nodes if nd.type == "OUTPUT_MATERIAL")
        how = nt.nodes.new("ShaderNodeAttribute")
        how.attribute_name = "run"
        fac = nt.nodes.new("ShaderNodeMath")
        fac.operation = "MULTIPLY"
        fac.inputs[1].default_value = dark
        nt.links.new(how.outputs["Fac"], fac.inputs[0])
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        film = nt.nodes.new("ShaderNodeBsdfDiffuse")
        film.inputs["Color"].default_value = (0.03, 0.026, 0.022, 1.0)
        mix = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(fac.outputs[0], mix.inputs[0])
        nt.links.new(clear.outputs[0], mix.inputs[1])
        nt.links.new(film.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return mat



def put(model, ob, arm, burn=False, name=None, rings=0, rain=True):
    """Lays the model's markings on its skin (as it stands at rest), each riding the part it is on. Returns the objects made.
    burn: the marking darkens as its object's colour (red, 0 to 1) says she has burnt, as works.land_look(burn=...) does.
    name, rings: her crew's own mark too (b477), where her file has a place for it: a word, or a count of barrel rings.
    rain: the few soft marks where rain has run down her sides (`rain_marks`)."""
    tree = BVHTree.FromPolygons([tuple(v) for v in model.verts], [tuple(f) for f in model.faces])
    verts, faces, cols, groups, runs = [], [], [], [], {}
    if rain:
        rain_marks(model, tree, verts, faces, cols, groups, runs)
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
        if c == "rain":
            me.materials.append(_rain_material(rain_strength(model)))
            continue
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
    if runs:
        how = me.color_attributes.new("run", "FLOAT_COLOR", "POINT")
        for i, a in runs.items():
            how.data[i].color = (a, a, a, 1.0)
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
