"""Quick looks at a work while it is being built: python peek.py <Name> <out folder> [views] [WxHxSPP]
views: front, rear, side, top, close, low, game (comma separated)."""
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import ground  # noqa: E402
import kit  # noqa: E402
import look  # noqa: E402
import rigid  # noqa: E402
import works  # noqa: E402

# (bearing the camera stands on, degrees anticlockwise from the enemy's side; how far, in spans; how high it looks down; lens)
VIEWS = {
    "front": (-28.0, 1.55, 17.0, 50), "rear": (155.0, 1.55, 24.0, 50), "side": (-88.0, 1.6, 10.0, 50), "top": (-20.0, 1.9, 62.0, 50),
    "close": (-35.0, 0.8, 16.0, 45), "low": (-18.0, 1.35, 4.0, 40), "game": (-30.0, 3.4, 30.0, 50), "inside": (150.0, 0.75, 38.0, 40),
}


def main():
    name, out = sys.argv[1], sys.argv[2]
    views = sys.argv[3].split(",") if len(sys.argv) > 3 else ["front", "rear"]
    w, h, spp = (int(v) for v in (sys.argv[4] if len(sys.argv) > 4 else "960x540x24").split("x"))
    kit.fresh()
    t = time.time()
    m = works.BUILDERS[name](True)
    ob, arm = look.stand(m)
    print(f"{name}: {m.triangles()} triangles, {len(m.parts)} parts, {len(m.sockets)} sockets, {len(m.clips)} clips, built in {time.time() - t:.1f}s", flush=True)
    t = time.time()
    ground.Ground(digs=m.info.get("dig", []), worn=m.info.get("worn", []), clear=m.info.get("clear", []), trees="notrees" not in sys.argv)
    print(f"ground: {time.time() - t:.1f}s", flush=True)
    look.render_setup(w, h, spp)
    cam = look.cam()
    span = m.info.get("span", 4.0)
    tgt = Vector(m.info.get("aim", (0.0, 0.0, m.info.get("high", span * 0.16))))
    for v in views:
        yaw, far, pitch, lens = VIEWS[v]
        cam.data.lens = lens
        kit.orbit(cam, tgt, max(span * far * 1.25, 2.2), pitch, yaw)
        t = time.time()
        kit.still(os.path.join(out, f"{name}-{v}.png"))
        print(f"  {v}: {time.time() - t:.1f}s", flush=True)


main()
