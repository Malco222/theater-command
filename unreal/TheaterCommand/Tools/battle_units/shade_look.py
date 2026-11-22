"""Before/after pictures of a soldier's shade, drawn the way the battle draws him: each point its paint's colour times
its shade as a vertex colour, on a plain rough surface, in daylight (a sun and the sky), no textures.

    /home/user/bpyenv/bin/python shade_look.py soldier_USA OUT.png [npz]   (npz: a staged shade; default: the file's own)
"""
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gore  # noqa: E402
import gore_check  # noqa: E402
import shade  # noqa: E402


def srgb_to_lin(c):
    c = np.asarray(c, float)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def build(bpy, M, Q, pal, s, name, offset):
    T = np.array(M["triangles"]).reshape(-1, 3)
    pa = np.array(M["paints"])
    Pb = Q.copy()
    Pb[:, 1] *= -1
    Pb += np.array(offset)
    me = bpy.data.meshes.new(name)
    me.from_pydata(Pb.tolist(), [], T[:, [0, 2, 1]].tolist())
    me.update()
    rgb = np.array([[int(pal.get(int(p), "808080")[k:k + 2], 16) / 255.0 for k in (0, 2, 4)] for p in pa])
    col = np.clip(rgb * np.asarray(s)[:, None], 0, 1)
    lin = srgb_to_lin(col)
    attr = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    attr.data.foreach_set("color", np.concatenate([lin, np.ones((len(lin), 1))], 1).ravel())
    me.polygons.foreach_set("use_smooth", np.ones(len(T), bool))
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    mat = bpy.data.materials.get("vc") or make_mat(bpy)
    me.materials.append(mat)
    return ob


def make_mat(bpy):
    m = bpy.data.materials.new("vc")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    a = nt.nodes.new("ShaderNodeVertexColor")
    a.layer_name = "Col"
    nt.links.new(a.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.85
    # (the battle's look: a sun and a flat sky fill with no shadowing of its own, so the fill is an emission of the
    # vertex colour, and the world gives no light)
    bsdf.inputs["Emission Color"].default_value = (1, 1, 1, 1)
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    nt.links.new(a.outputs["Color"], mul.inputs[6])
    mul.inputs[7].default_value = (0.42, 0.44, 0.48, 1)
    nt.links.new(mul.outputs[2], bsdf.inputs["Emission Color"])
    bsdf.inputs["Emission Strength"].default_value = 1.0
    return m


def main(name, out, npz=None, grey=False, w=900, h=1000, samples=24, shots=(("advance", 0.4, 200.0, 1.0, 4.2), ("fire_kneel", 0.05, 20.0, 0.8, 3.6), ("idle", 0.3, 15.0, 1.15, 2.9))):
    import bpy
    import kit
    D = json.load(open(os.path.join(shade.SHARED, name + ".json")))
    rig = gore.Rig(D["bones"])
    pal = gore_check.palette(name, D)
    if grey:                                   # (the shade alone: every paint one light grey)
        pal = {k: "B4B4B4" for k in list(pal) + list(range(64))}
    M = D["near"]
    n = len(M["paints"])
    after = np.load(npz)["near"] if npz else np.array(M.get("shade", [1.0] * n))
    before = np.ones(n)
    kit.fresh()
    sc = kit.render_setup(w, h, samples)
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 1
    kit.studio(sun_elev=48, sun_turn=-35, strength=3.2, sky=0.0)
    if sc.world:
        sc.world.color = (0, 0, 0)
        if sc.world.use_nodes:
            for nd in sc.world.node_tree.nodes:
                if nd.type == "BACKGROUND":
                    nd.inputs["Strength"].default_value = 0.0
    kit.backdrop("B9B09A")
    sc.render.film_transparent = True          # (the sky is pasted in after as a flat grey-blue, lighting nothing)
    from mathutils import Vector
    from PIL import Image
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    tiles = []
    for clip, f, yaw, zc, dist in shots:
        for ob in [o for o in bpy.data.objects if o.name.startswith(("b_", "a_"))]:
            bpy.data.objects.remove(ob, do_unlink=True)
        c = D["clips"][clip]
        Q, _ = shade.posed(rig, M, c, f * c["long"])
        Q = Q - np.array((Q[:, 0].mean(), Q[:, 1].mean(), 0.0))
        build(bpy, M, Q, pal, before, "b_" + clip, (0.0, -0.55, 0))      # (json y right: before on the left)
        build(bpy, M, Q, pal, after, "a_" + clip, (0.0, 0.55, 0))
        r = math.radians(yaw)
        e = Vector((dist * math.cos(r), dist * math.sin(r), zc + 0.35))
        t = Vector((0.0, 0.0, zc))
        cam.location = e
        cam.rotation_euler = (t - e).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = 50
        tmp = out.replace(".png", f"_{clip}.png")
        sc.render.filepath = tmp
        bpy.ops.render.render(write_still=True)
        im = Image.open(tmp).convert("RGBA")
        bg = Image.new("RGBA", im.size, (138, 150, 166, 255))
        tiles.append(Image.alpha_composite(bg, im).convert("RGB"))
    W = sum(t.width for t in tiles)
    sheet = Image.new("RGB", (W, tiles[0].height), "white")
    x = 0
    for t in tiles:
        sheet.paste(t, (x, 0))
        x += t.width
    sheet.save(out)
    print("wrote", out)


if __name__ == "__main__":
    a = sys.argv[1:]
    g = "--grey" in a
    a = [x for x in a if x != "--grey"]
    main(a[0], a[1], a[2] if len(a) > 2 else None, grey=g)
