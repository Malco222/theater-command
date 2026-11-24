"""Pictures of the far level re-made by far_mend.py, for the art director: drawn the way the battle draws him
(shade_look.py's daylight and vertex colours), standing (idle). One sheet per file: the old far level (the shared file
as it was, from far_mend.py's backup or the shared folder) and the new one (the stage), each from his front, his left
and behind; and the middle level, which the far one is cut from.

    PYTHONPATH=<a checkout with kit.py> /home/user/bpyenv/bin/python far_look.py OLD_DIR NEW_DIR OUT_DIR soldier_CHN [...]
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

SHOTS = (("mid", "new", 25.0, "middle level (cut from it)"), ("far", "old", 25.0, "far, before"),
         ("far", "new", 25.0, "far, now"), ("far", "old", 100.0, "far, before, his left"),
         ("far", "new", 100.0, "far, now, his left"), ("far", "old", 160.0, "far, before, behind"),
         ("far", "new", 160.0, "far, now, behind"))


def sheet(old_dir, new_dir, name, out_dir, w=360, h=760, samples=20):
    import bpy
    import kit
    from mathutils import Vector
    from PIL import Image, ImageDraw
    Ds = {"old": json.load(open(os.path.join(old_dir, name + ".json"))), "new": json.load(open(os.path.join(new_dir, name + ".json")))}
    D = Ds["new"]
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
    for lod, which, yaw, label in SHOTS:
        for ob in [o for o in bpy.data.objects if o.name.startswith("m_")]:
            bpy.data.objects.remove(ob, do_unlink=True)
        M = Ds[which][lod]
        Q, _ = shade.posed(rig, M, c, 0.3 * c["long"])
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
    p = os.path.join(out_dir, f"{name}_far_mend.jpg")
    out.save(p, quality=90)
    print("wrote", p, flush=True)


if __name__ == "__main__":
    old_dir, new_dir, out_dir, *names = sys.argv[1:]
    os.makedirs(out_dir, exist_ok=True)
    for nm in names:
        sheet(old_dir, new_dir, nm, out_dir)
