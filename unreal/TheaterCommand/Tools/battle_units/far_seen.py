"""The far level checked where the battle first draws it: 120 m off as a 50 degree lens on a 1920 x 1080 picture sees
him (about 31 pixels tall), against the middle level at the same distance (the level it takes over from) and the far
level as it was before far_mend.py. Each tile is rendered at its true pixel size, then blown up four times
(nearest pixel) so the two legs, or a slab, can be judged. Standing (idle) from the front, and mid-stride (advance)
from his left and from the front.

    PYTHONPATH=<a checkout with kit.py> /home/user/bpyenv/bin/python far_seen.py OLD_DIR NEW_DIR OUT_DIR soldier_UK [...]
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

DIST, HFOV, SCREEN_W = 120.0, 50.0, 1920
TILE = 64                                            # pixels a side at true size (the man about 31 tall)
POSES = (("idle", 0.3, 25.0, "standing, front"), ("advance", 0.25, 100.0, "advancing, his left"),
         ("advance", 0.25, 25.0, "advancing, front"))
LEVELS = (("mid", "new", "middle (before 120 m)"), ("far", "old", "far, before"), ("far", "new", "far, now"))


def sheet(old_dir, new_dir, name, out_dir, samples=24, blow=4):
    import bpy
    import kit
    from mathutils import Vector
    from PIL import Image, ImageDraw
    Ds = {"old": json.load(open(os.path.join(old_dir, name + ".json"))), "new": json.load(open(os.path.join(new_dir, name + ".json")))}
    D = Ds["new"]
    rig = gore.Rig(D["bones"])
    pal = gore_check.palette(name, D)
    kit.fresh()
    sc = kit.render_setup(TILE, TILE, samples)
    sc.cycles.use_denoising = False
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
    cam.data.sensor_width = 36.0
    cam.data.sensor_fit = "HORIZONTAL"
    per_px = 2.0 * math.tan(math.radians(HFOV / 2.0)) / SCREEN_W          # (the tangent one pixel spans)
    cam.data.lens = 18.0 / (per_px * TILE / 2.0)
    rows = []
    for clip, at, yaw, plabel in POSES:
        c = D["clips"].get(clip) or D["clips"]["idle"]
        row = []
        for lod, which, label in LEVELS:
            for ob in [o for o in bpy.data.objects if o.name.startswith("m_")]:
                bpy.data.objects.remove(ob, do_unlink=True)
            M = Ds[which][lod]
            cc = Ds[which]["clips"].get(clip) or Ds[which]["clips"]["idle"]
            Q, _ = shade.posed(gore.Rig(Ds[which]["bones"]), M, cc, at * cc["long"])
            Q = Q - np.array((Q[:, 0].mean(), Q[:, 1].mean(), 0.0))
            n = len(M["paints"])
            shade_look.build(bpy, M, Q, pal, np.asarray(M.get("shade", [1.0] * n)), "m_" + lod, (0.0, 0.0, 0.0))
            r = math.radians(yaw)
            e = Vector((DIST * math.cos(r), -DIST * math.sin(r), 1.6 + DIST * math.tan(math.radians(3.0))))
            t = Vector((0.0, 0.0, 0.9))
            cam.location = e
            cam.rotation_euler = (t - e).to_track_quat("-Z", "Y").to_euler()
            tmp = os.path.join(out_dir, f"_{name}_t.png")
            sc.render.filepath = tmp
            bpy.ops.render.render(write_still=True)
            im = Image.open(tmp).convert("RGBA")
            bg = Image.new("RGBA", im.size, (138, 150, 166, 255))
            tile = Image.alpha_composite(bg, im).convert("RGB").resize((TILE * blow, TILE * blow), Image.NEAREST)
            ImageDraw.Draw(tile).text((6, 6), f"{label}\n{plabel}", fill=(20, 20, 20))
            row.append(tile)
            os.remove(tmp)
        rows.append(row)
    S = TILE * blow
    out = Image.new("RGB", (S * len(LEVELS), S * len(POSES) + 24), "white")
    for i, row in enumerate(rows):
        for j, tile in enumerate(row):
            out.paste(tile, (j * S, i * S))
    ImageDraw.Draw(out).text((8, S * len(POSES) + 6), f"{name}: 120 m, 50 degree lens on 1920 wide, x{blow} (nearest pixel)", fill=(20, 20, 20))
    p = os.path.join(out_dir, f"{name}_far_at_120m.png")
    out.save(p)
    print("wrote", p, flush=True)


if __name__ == "__main__":
    old_dir, new_dir, out_dir, *names = sys.argv[1:]
    os.makedirs(out_dir, exist_ok=True)
    for nm in names:
        sheet(old_dir, new_dir, nm, out_dir)
