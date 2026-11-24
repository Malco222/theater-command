"""Pictures of a soldier with his pack off (pack_tris left out), for the art director: drawn the way the battle draws
him (shade_look.py's daylight and vertex colours), standing (idle), from behind on his left and from the front on his
right. One sheet per file: near with his pack, near without (behind, front), mid without, far without (behind).

    PYTHONPATH=<a checkout with kit.py> /home/user/bpyenv/bin/python pack_look.py FOLDER OUT_DIR soldier_GER [...]
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
import shade_look  # noqa: E402

SHOTS = (("near", True, 160.0, "with his pack"), ("near", False, 160.0, "pack off"),
         ("near", False, 25.0, "pack off, front"), ("mid", False, 160.0, "mid, pack off"),
         ("far", False, 160.0, "far, pack off"))


def without(M, rng):
    """The level's mesh with the triangles [first, first + count) left out (the points stay)."""
    T = np.array(M["triangles"]).reshape(-1, 3)
    a, n = rng
    keep = np.ones(len(T), bool)
    keep[a:a + n] = False
    out = dict(M)
    out["triangles"] = T[keep].ravel().tolist()
    return out


def sheet(folder, name, out_dir, w=440, h=760, samples=20):
    import bpy
    import kit
    from mathutils import Vector
    from PIL import Image, ImageDraw
    D = json.load(open(os.path.join(folder, name + ".json")))
    rig = gore.Rig(D["bones"])
    pal = gore_check.palette(name, D)
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
    sc.render.film_transparent = True
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    clip = "idle" if "idle" in D["clips"] else sorted(D["clips"])[0]
    c = D["clips"][clip]
    tiles = []
    for lod, whole, yaw, label in SHOTS:
        for ob in [o for o in bpy.data.objects if o.name.startswith("m_")]:
            bpy.data.objects.remove(ob, do_unlink=True)
        M = D[lod] if whole else without(D[lod], D["pack_tris"][lod])
        Q, _ = shade.posed(rig, D[lod], c, 0.3 * c["long"])
        Q = Q - np.array((Q[:, 0].mean(), Q[:, 1].mean(), 0.0))
        n = len(M["paints"])
        shade_look.build(bpy, M, Q, pal, np.asarray(M.get("shade", [1.0] * n)), "m_" + lod, (0.0, 0.0, 0.0))
        r = math.radians(yaw)
        e = Vector((3.0 * math.cos(r), -3.0 * math.sin(r), 1.3))
        t = Vector((0.0, 0.0, 0.95))
        cam.location = e
        cam.rotation_euler = (t - e).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = 50
        tmp = os.path.join(out_dir, f"_{name}_{len(tiles)}.png")
        sc.render.filepath = tmp
        bpy.ops.render.render(write_still=True)
        im = Image.open(tmp).convert("RGBA")
        bg = Image.new("RGBA", im.size, (138, 150, 166, 255))
        tile = Image.alpha_composite(bg, im).convert("RGB")
        ImageDraw.Draw(tile).text((10, 10), label, fill=(20, 20, 20))
        tiles.append(tile)
        os.remove(tmp)
    out = Image.new("RGB", (sum(t.width for t in tiles), h), "white")
    x = 0
    for t in tiles:
        out.paste(t, (x, 0))
        x += t.width
    ImageDraw.Draw(out).text((10, h - 20), name, fill=(20, 20, 20))
    p = os.path.join(out_dir, f"{name}_pack_off.jpg")
    out.save(p, quality=90)
    print("wrote", p, flush=True)


if __name__ == "__main__":
    folder, out_dir, *names = sys.argv[1:]
    os.makedirs(out_dir, exist_ok=True)
    for nm in names:
        sheet(folder, nm, out_dir)
