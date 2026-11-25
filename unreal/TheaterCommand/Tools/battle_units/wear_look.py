"""One man four ways, the way the battle will mix the wear and weather arrays (see the file's about_wear): a fresh
replacement, a man six weeks in the line, the same in rain, and in snow.

    /home/user/bpyenv/bin/python wear_look.py soldier_USA OUT.jpg [stage.npz]   (default: the file's own arrays)
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

STATES = [("fresh replacement", 0.1, 0.0, 0.0), ("six weeks in the line", 1.0, 0.0, 0.0),
          ("in the line, in rain", 0.8, 1.0, 0.0), ("in snow", 0.6, 0.0, 1.0)]
DUST, MUD, SNOW = np.array(shade.DUST), np.array(shade.MUD), np.array(shade.SNOW_RGB)


def mix(rgb, A, W, R, S):
    """The recipe in about_wear, on sRGB paint colours (0 to 1) per point."""
    sc = A["shade_clean"][:, None]
    base = rgb * sc * (1 - 0.35 * A["wet"] * R)[:, None]
    d = ((A["dirt"] + (A["dirt_wet"] - A["dirt"]) * R) * W)[:, None]
    soil = (DUST + (MUD - DUST) * R) * sc
    c = base + (soil - base) * 0.85 * d
    k = (0.9 * A["snow"] * S)[:, None]
    return np.clip(c + (SNOW * (0.6 + 0.4 * sc) - c) * k, 0, 1)


def arrays_of(M, npz, lod):
    if npz is not None:
        return {k.split("|")[1]: npz[k] for k in npz.files if k.startswith(lod + "|")}
    n = len(M["paints"])
    return {k: np.array(M[k], float) if k != "tint" else np.array(M[k], float).reshape(-1, 3)
            for k in ("shade_clean", "dirt", "dirt_wet", "wet", "snow")} if "dirt" in M else None


def main(name, out, npz_path=None, w=1800, h=1000, samples=24):
    import bpy
    import kit
    from mathutils import Vector
    from PIL import Image, ImageDraw, ImageFont
    D = json.load(open(os.path.join(shade.SHARED, name + ".json")))
    rig = gore.Rig(D["bones"])
    pal = gore_check.palette(name, D)
    M = D["near"]
    A = arrays_of(M, np.load(npz_path) if npz_path else None, "near")
    pa = np.array(M["paints"])
    rgb = np.array([[int(pal.get(int(p), "808080")[k:k + 2], 16) / 255.0 for k in (0, 2, 4)] for p in pa])
    kit.fresh()
    sc = kit.render_setup(w, h, samples)
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 1
    kit.studio(sun_elev=48, sun_turn=-35, strength=3.2, sky=0.0)
    if sc.world and sc.world.use_nodes:
        for nd in sc.world.node_tree.nodes:
            if nd.type == "BACKGROUND":
                nd.inputs["Strength"].default_value = 0.0
    kit.backdrop("8A8460")
    sc.render.film_transparent = True
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.lens = 50
    rows = []
    for clip, f, yaw, zc, dist in (("idle", 0.3, 15.0, 1.0, 6.2), ("advance", 0.4, 200.0, 0.95, 6.4)):
        for ob in [o for o in bpy.data.objects if o.name.startswith("w_")]:
            bpy.data.objects.remove(ob, do_unlink=True)
        c = D["clips"][clip]
        Q, _ = shade.posed(rig, M, c, f * c["long"])
        Q = Q - np.array((Q[:, 0].mean(), Q[:, 1].mean(), 0.0))
        r = math.radians(yaw)
        side = np.array((-math.sin(r), math.cos(r), 0.0))       # (across the view, in blender's axes)
        for k, (label, W, R, S) in enumerate(STATES):
            col = mix(rgb, A, W, R, S)
            off = side * (k - 1.5) * 1.05
            ob = shade_look.build(bpy, M, Q, {int(p): "808080" for p in set(pa.tolist())}, np.ones(len(pa)),
                                  f"w_{k}", (0.0, 0.0, 0.0))
            ob.location = tuple(off)
            lin = shade_look.srgb_to_lin(col)
            ob.data.color_attributes["Col"].data.foreach_set(
                "color", np.concatenate([lin, np.ones((len(lin), 1))], 1).ravel())
        e = Vector((dist * math.cos(r), dist * math.sin(r), zc + 0.35))
        t = Vector((0.0, 0.0, zc))
        cam.location = e
        cam.rotation_euler = (t - e).to_track_quat("-Z", "Y").to_euler()
        tmp = out.rsplit(".", 1)[0] + f"_{clip}.png"
        sc.render.filepath = tmp
        bpy.ops.render.render(write_still=True)
        im = Image.open(tmp).convert("RGBA")
        bg = Image.new("RGBA", im.size, (138, 150, 166, 255))
        rows.append(Image.alpha_composite(bg, im).convert("RGB"))
    sheet = Image.new("RGB", (w, h * len(rows)), "white")
    for i, im in enumerate(rows):
        sheet.paste(im, (0, i * h))
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("DejaVuSans.ttf", 30)
    except Exception:
        font = ImageFont.load_default()
    for k, (label, *_) in enumerate(STATES):
        d.text((int(w * (k + 0.5) / 4) - 140, 18), label, fill=(25, 25, 25), font=font)
    sheet.save(out, quality=88)
    print("wrote", out)


if __name__ == "__main__":
    a = sys.argv[1:]
    main(a[0], a[1], a[2] if len(a) > 2 else None)
