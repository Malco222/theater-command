"""The sizes against men: several bursts in one row on the same ground, seen by one camera, with a man (1.75 m)
standing at the foot of each, so their sizes can be judged against each other and against a man.

    python sizes.py <out folder> [sizes=mortar,gun75,how150,bomb250] [ground=earth] [frames=8,40] [res=1920x1080x16]
"""
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import burst  # noqa: E402
import strip  # noqa: E402

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

# where each size lands along the row (x, y): far enough apart that the fans do not cross
ROW = {"grenade": -44.0, "mortar": -38.0, "gun75": -24.0, "how105": -12.0, "how150": 0.0, "bomb250": 42.0, "bomb500": 50.0}
HIDE_NEAR = ((-20.0, -20.0, 7.0),)       # (the lone tree at -20, -20 stands in front of the row)
LINE_Y = 2.0


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    sizes = strip.arg("sizes", "mortar,gun75,how150,bomb250").split(",")
    ground = strip.arg("ground", "earth")
    frames = [int(f) for f in strip.arg("frames", "8,40").split(",")]
    w, h, spp = (int(v) for v in strip.arg("res", "1920x1080x16").split("x"))
    st = strip.lay_stage(strip.arg("cache", "/home/user/fxlab"), "day")
    if ground != "earth":
        strip.retint(ground)
    for ob in bpy.data.objects:
        x, y = ob.matrix_world.translation.x, ob.matrix_world.translation.y
        if ob.type == "MESH" and ob.name != "terrain" and any((x - hx) ** 2 + (y - hy) ** 2 < hr * hr for hx, hy, hr in HIDE_NEAR):
            ob.hide_render = True
    import stage
    sc = stage.render_setup(w, h, spp)
    sc.cycles.volume_bounces = 0
    F = burst.Field(height=st.height, seed=3, wind=(1.6, 0.4, 0.0), volumes=False)
    xs = [ROW[s] for s in sizes]
    mid = 0.5 * (min(xs) + max(xs))
    span = max(xs) - min(xs) + 60.0
    back = span / (2 * math.tan(math.radians(27.0))) * 1.05
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.lens = 35
    look = Vector((mid, LINE_Y, st.height(mid, LINE_Y) + 14.0))
    cam.location = Vector((mid, LINE_Y - back, st.height(mid, LINE_Y - back) + 22.0))
    cam.rotation_euler = (look - cam.location).to_track_quat("-Z", "Y").to_euler()
    for i, size in enumerate(sizes):
        x = ROW[size]
        hit = Vector((x, LINE_Y, st.height(x, LINE_Y)))
        info = burst.ground_burst(F, -0.02, hit, size=size, ground=ground, seed=3 + i, cam=None)
        print(size, info, flush=True)
        # a man at its foot: just off the crater's lip on the camera's side, a little to the right
        S = burst.RECIPE["sizes"][size]
        bpy.ops.import_scene.gltf(filepath="/mnt/project-files/battle-models/land/soldier/soldier_GER_LOD1.glb")
        root = next(o for o in bpy.context.selected_objects if o.type == "ARMATURE")
        mx, my = x + 1.0 + 0.3 * S["crater_m"], LINE_Y - (S["crater_m"] * 0.6 + 2.0)
        root.location = (mx, my, st.height(mx, my))
        root.rotation_euler = (0, 0, math.radians(200))
    for f in frames:
        path = os.path.join(out, f"sizes-{ground}-f{f:03d}.png")
        if os.path.exists(path):
            continue
        F.at(f / 30.0)
        tick = time.time()
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print(f"  frame {f}: {time.time() - tick:.1f}s", flush=True)


if __name__ == "__main__":
    main()
