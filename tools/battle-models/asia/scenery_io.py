"""From a `Geo` to Blender (for the pictures) and to the game's files (`<Name>.json`, `<Name>.glb`).

The model file is the vehicles' format (rigid.py's `write_json`) cut down for things that stand still: `near`, `mid`
and `far`, each with `points`, `facings`, `paints`, `triangles` and, new for scenery, `sway` point by point (0 to 1:
how far the wind moves that point, as the Eastern Front's files write it). `colours` and `paint_names` give each paint its colour; `two_sided` names the
paints drawn from both faces (leaves, fronds, thatch fringes). y is written to the right as in every battle model.
"""
import json
import math
import os

import bpy
import numpy as np

import kit

# How each kind of paint looks in the pictures (the game has its own materials; these only make the review honest).
#   rough, translucent share (leaves pass light), noise (mottling), noise scale, bump
LOOKS = {
    "leaf": (0.62, 0.35, 0.22, 9.0, 0.0),
    "frond": (0.58, 0.30, 0.18, 6.0, 0.0),
    "grass": (0.7, 0.30, 0.20, 12.0, 0.0),
    "bark": (0.92, 0.0, 0.45, 18.0, 0.6),
    "wood": (0.85, 0.0, 0.35, 30.0, 0.3),
    "bamboo": (0.45, 0.10, 0.25, 4.0, 0.1),
    "thatch": (0.95, 0.05, 0.50, 40.0, 0.7),
    "weave": (0.9, 0.0, 0.40, 60.0, 0.5),
    "brick": (0.9, 0.0, 0.40, 22.0, 0.5),
    "plaster": (0.9, 0.0, 0.25, 8.0, 0.2),
    "tile": (0.7, 0.0, 0.30, 25.0, 0.3),
    "earth": (0.97, 0.0, 0.45, 6.0, 0.6),
    "stone": (0.9, 0.0, 0.40, 10.0, 0.8),
    "gilt": (0.35, 0.0, 0.15, 10.0, 0.1),
    "water": (0.08, 0.0, 0.05, 3.0, 0.0),
    "straw": (0.9, 0.1, 0.45, 50.0, 0.5),
    "metal": (0.6, 0.0, 0.30, 20.0, 0.2),
}


def look_of(paint):
    for k in LOOKS:
        if paint.startswith(k):
            return k
    return "wood"


def material_for(geo_name, paint, hexcol):
    name = f"{paint}#{hexcol}"
    m = bpy.data.materials.get(name)
    if m:
        return m
    rough, trans, noise, scale, bump = LOOKS[look_of(paint)]
    rgb = kit.hex_rgb(hexcol)
    m = kit.material(name, rgb, rough=rough, noise=noise, noise_scale=scale, bump=bump, spec=0.3)
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    # every piece's own small shift in shade, so a stand of the same model is not one colour (Object Info random)
    oi = nt.nodes.new("ShaderNodeObjectInfo")
    hsv = nt.nodes.new("ShaderNodeHueSaturation")
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["To Min"].default_value, mr.inputs["To Max"].default_value = 0.86, 1.12
    nt.links.new(oi.outputs["Random"], mr.inputs["Value"])
    nt.links.new(mr.outputs["Result"], hsv.inputs["Value"])
    src = bsdf.inputs["Base Color"].links[0].from_socket if bsdf.inputs["Base Color"].links else None
    if src is not None:
        nt.links.new(src, hsv.inputs["Color"])
    else:
        hsv.inputs["Color"].default_value = (*rgb, 1)
    nt.links.new(hsv.outputs["Color"], bsdf.inputs["Base Color"])
    if trans:
        out = nt.nodes["Material Output"]
        tr = nt.nodes.new("ShaderNodeBsdfTranslucent")
        nt.links.new(hsv.outputs["Color"], tr.inputs["Color"])
        mix = nt.nodes.new("ShaderNodeMixShader")
        mix.inputs["Fac"].default_value = trans
        nt.links.new(bsdf.outputs[0], mix.inputs[1])
        nt.links.new(tr.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs["Surface"])
    if look_of(paint) == "water":
        bsdf.inputs["Roughness"].default_value = 0.06
    if look_of(paint) == "gilt":
        bsdf.inputs["Metallic"].default_value = 0.85
    return m


def to_blender(geo, name=None, collection=None, smooth_angle=50.0, at=(0, 0, 0), turn=0.0, scale=1.0):
    V, S, F, Fp = geo.arrays()
    me = bpy.data.meshes.new(name or geo.name)
    me.from_pydata(V.tolist(), [], F)
    me.update()
    used = sorted(set(Fp.tolist()))
    slot = {k: i for i, k in enumerate(used)}
    for k in used:
        p = geo.paint_names[k]
        me.materials.append(material_for(geo.name, p, geo.paint_hex[p]))
    mi = np.array([slot[k] for k in Fp], dtype=np.int32)
    me.polygons.foreach_set("material_index", mi)
    attr = me.attributes.new("sway", "FLOAT", "POINT")
    attr.data.foreach_set("value", S.astype(np.float32))
    me.polygons.foreach_set("use_smooth", np.ones(len(F), dtype=bool))
    ob = bpy.data.objects.new(name or geo.name, me)
    (collection or bpy.context.scene.collection).objects.link(ob)
    mod = ob.modifiers.new("smooth", "NODES")
    mod.node_group = smooth_group(smooth_angle)          # (a failure here is a bug: let it show)
    ob.location = at
    ob.rotation_euler = (0.0, 0.0, turn)
    ob.scale = (scale, scale, scale)
    return ob


_SMOOTH = {}


def smooth_group(angle):
    """Smooth by angle as a node group (Blender 4.1+ keeps no auto smooth on the mesh)."""
    g = _SMOOTH.get(angle)
    if g is not None:
        try:
            if bpy.data.node_groups.get(g.name) is g:
                return g
        except ReferenceError:                           # (removed by a fresh scene)
            pass
    g = bpy.data.node_groups.new(f"smooth{angle:.0f}", "GeometryNodeTree")
    g.interface.new_socket("Geometry", in_out="INPUT", socket_type="NodeSocketGeometry")
    g.interface.new_socket("Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
    gi = g.nodes.new("NodeGroupInput")
    go = g.nodes.new("NodeGroupOutput")
    ss = g.nodes.new("GeometryNodeSetShadeSmooth")
    ea = g.nodes.new("GeometryNodeInputMeshEdgeAngle")
    lt = g.nodes.new("FunctionNodeCompare")
    lt.data_type = "FLOAT"
    lt.operation = "LESS_THAN"
    lt.inputs[1].default_value = math.radians(angle)
    g.links.new(ea.outputs["Unsigned Angle"], lt.inputs[0])
    ss.domain = "EDGE"
    g.links.new(gi.outputs[0], ss.inputs["Geometry"])
    g.links.new(lt.outputs[0], ss.inputs["Shade Smooth"])
    g.links.new(ss.outputs[0], go.inputs[0])
    _SMOOTH[angle] = g
    return g


def gather(ob):
    """The mesh as the game reads it: points, facings, paints, triangles, sway (y to the right)."""
    dg = bpy.context.evaluated_depsgraph_get()
    oe = ob.evaluated_get(dg)
    me = oe.to_mesh()
    me.calc_loop_triangles()
    nl = len(me.loops)
    lv = np.zeros(nl, dtype=np.int32)
    me.loops.foreach_get("vertex_index", lv)
    co = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    cn = np.zeros(nl * 3)
    me.corner_normals.foreach_get("vector", cn)
    cn = cn.reshape(-1, 3)
    sw = np.zeros(len(me.vertices), dtype=np.float32)
    me.attributes["sway"].data.foreach_get("value", sw)
    pmat = np.zeros(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("material_index", pmat)
    lpoly = np.zeros(nl, dtype=np.int32)
    for p in me.polygons:
        lpoly[p.loop_start:p.loop_start + p.loop_total] = p.index
    names = [m.name.split("#")[0] for m in me.materials]
    tris = np.zeros(len(me.loop_triangles) * 3, dtype=np.int32)
    me.loop_triangles.foreach_get("loops", tris)
    oe.to_mesh_clear()
    pos = np.round(co[lv], 3)
    nor = np.round(cn, 2)
    paint = pmat[lpoly]
    sway = np.round(sw[lv], 2)
    key = np.concatenate([pos, nor, paint[:, None], sway[:, None]], axis=1)
    _, first, inverse = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inverse = inverse.ravel()
    order = np.argsort(first)
    rank = np.empty_like(order)
    rank[order] = np.arange(len(order))
    first = first[order]
    index = rank[inverse]
    p, n = pos[first].copy(), nor[first].copy()
    p[:, 1] *= -1.0
    n[:, 1] *= -1.0
    t = index[tris].reshape(-1, 3)[:, [0, 2, 1]]
    return {"points": [round(float(v), 3) for v in p.ravel()], "facings": [round(float(v), 2) for v in n.ravel()],
            "paints": [int(v) for v in paint[first]], "triangles": [int(v) for v in t.ravel()],
            "sway": [round(float(v), 2) for v in sway[first]], "_names": names}


def write_model(path, title, geos, extra=None):
    """geos: {'near': (geo, ob), 'mid': ..., 'far': ...}. Paint indices are put into one list for all three."""
    names, colours = [], []
    out = {}
    two = set()
    for lod, (geo, ob) in geos.items():
        g = gather(ob)
        local = g.pop("_names")
        remap = []
        for nm in local:
            if nm not in names:
                names.append(nm)
                colours.append(geo.paint_hex[nm])
            remap.append(names.index(nm))
        g["paints"] = [remap[k] for k in g["paints"]]
        out[lod] = g
        two |= geo.two_sided
    near = out["near"]
    xs, ys, zs = near["points"][0::3], near["points"][1::3], near["points"][2::3]
    data = {
        "name": title,
        "about": "Made by tools/battle-models/asia (scenery). Metres; x east, y right, z up from the ground at its foot. "
                 "Stands still: `sway` (0 to 1, point by point) is how far the wind moves each point; nothing else moves. "
                 "Looks only: the battle's ground decides cover and passage. Paints: " + ", ".join(f"{i} {k}" for i, k in enumerate(names)),
        "kind": "scenery",
        "region": "asia",
        "colours": colours,
        "paint_names": names,
        "two_sided": sorted(two & set(names)),
        **out,
        "long": round(max(xs) - min(xs), 2), "wide": round(max(ys) - min(ys), 2), "high": round(max(zs), 2),
        "high_m": round(max(zs), 2),
        "triangles": {k: len(v["triangles"]) // 3 for k, v in out.items()},
    }
    if extra:
        data.update(extra)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    return data


def export_glb(path, objs):
    """The fine mesh and its two plainer levels as <Name>, <Name>_LOD1, <Name>_LOD2. Each paint is a plain material
    of its colour (named as in the json, "paint#RRGGBB"); the sway rides along as the point attribute _SWAY (0 to 1),
    and no vertex colour is written, so any glTF reader shows the model in its own colours."""
    plain, swapped = {}, []
    for ob in objs:
        me = ob.data
        if "_SWAY" not in me.attributes:
            sw = np.zeros(len(me.vertices), dtype=np.float32)
            me.attributes["sway"].data.foreach_get("value", sw)
            me.attributes.new("_SWAY", "FLOAT", "POINT").data.foreach_set("value", sw)
        for i, m in enumerate(me.materials):
            if m is None:
                continue
            if m.name not in plain:
                paint, _, hexcol = m.name.partition("#")
                pm = bpy.data.materials.new(m.name + "~plain")
                pm.diffuse_color = (*kit.hex_rgb(hexcol), 1.0) if hexcol else m.diffuse_color
                pm.use_nodes = True
                b = pm.node_tree.nodes["Principled BSDF"]
                b.inputs["Base Color"].default_value = pm.diffuse_color
                b.inputs["Roughness"].default_value = 0.85
                b.inputs["Metallic"].default_value = 0.0
                pm.use_backface_culling = m.use_backface_culling
                plain[m.name] = (m, pm)
            swapped.append((me, i, m))
            me.materials[i] = plain[m.name][1]
    for m, pm in plain.values():                         # the glb names its materials as the json names its paints
        nm = m.name
        m.name = nm + "~render"
        pm.name = nm
    try:
        kit._select_only(objs)
        bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_animations=False,
                                  export_apply=True, export_yup=True, export_extras=True, export_vertex_color="NONE",
                                  export_attributes=True)
    finally:
        for me, i, m in swapped:
            me.materials[i] = m
        for m, pm in plain.values():
            nm = pm.name
            bpy.data.materials.remove(pm)
            m.name = nm
    return path
