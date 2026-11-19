"""Quick stills of scenery models while building them.

    python look.py <Name>[,<Name>...] <out folder> [lod=0] [w=960] [h=720] [samples=24] [seed=N] [eye=side|low|high]

Each model stands on a plain earth disc under the studio sun with a 1.75 m pole beside it for scale.
"""
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
import numpy as np  # noqa: E402

import kit  # noqa: E402
import scenery_io  # noqa: E402
from catalogue import build  # noqa: E402


def main():
    names = sys.argv[1].split(",")
    out = sys.argv[2]
    opts = dict(a.split("=", 1) for a in sys.argv[3:] if "=" in a)
    lod, w, h, samples = int(opts.get("lod", 0)), int(opts.get("w", 960)), int(opts.get("h", 720)), int(opts.get("samples", 24))
    for name in names:
        t0 = time.time()
        kit.fresh()
        kit.studio(sun_elev=40, sun_turn=-40, strength=3.4, sky=0.6)
        g = kit.field_ground(300, "earth")
        kw = {"seed": int(opts["seed"])} if "seed" in opts else {}
        geo = build(name, lod, **kw)
        ob = scenery_io.to_blender(geo)
        pole = bpy.data.objects.new("pole", bpy.data.meshes.new("pole"))
        bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=1.75, location=(-1.0 - geo.info.get("foot_m", 1.0), -1.0, 0.875))
        bpy.context.object.data.materials.append(kit.material("pole", kit.hex_rgb("B03A2E"), rough=0.5))
        V = geo.arrays()[0]
        H = max(float(V[:, 2].max()), 1.0)
        Wd = max(H, float(np.ptp(V[:, 0])), float(np.ptp(V[:, 1]))) + 1.0
        cam = kit.camera("cam", lens=40)
        bpy.context.scene.camera = cam
        eye = opts.get("eye", "side")
        dist = max(H, Wd) * 1.55 + 2.0
        if eye == "low":
            kit.look_at(cam, (dist * 0.8, -dist * 0.5, 1.7), (0, 0, H * 0.45))
        elif eye == "high":
            kit.look_at(cam, (dist * 0.7, -dist * 0.7, dist * 0.9), (0, 0, H * 0.3))
        else:
            kit.look_at(cam, (dist * 0.85, -dist * 0.55, H * 0.45 + 1.5), (0, 0, H * 0.45))
        kit.render_setup(w, h, samples)
        path = os.path.join(out, f"{name}-lod{lod}-{eye}.png")
        kit.still(path)
        print(f"{name} lod{lod}: {geo.tris()} triangles, {time.time() - t0:.0f}s -> {path}", flush=True)


if __name__ == "__main__":
    main()
