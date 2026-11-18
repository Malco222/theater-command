"""Quick looks at a ship while she is being built: a few stills from fixed places. python look.py <name> <out folder> [fast]"""
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import deckpark  # noqa: E402
import kit  # noqa: E402
import rigid  # noqa: E402
import ships  # noqa: E402
import stage  # noqa: E402


def main():
    name, out = sys.argv[1], sys.argv[2]
    views = sys.argv[3].split(",") if len(sys.argv) > 3 else ["bow", "beam", "top", "quarter"]
    kit.fresh()
    t = time.time()
    m, yard = ships.BUILDERS[name](True)
    ob, arm = rigid.to_blender(m, stage.ship_look)
    deckpark.range_on(m, arm)
    print(f"{name}: {m.triangles()} triangles, {len(m.parts)} parts, {len(m.sockets)} sockets, {len(m.clips)} clips, built in {time.time() - t:.1f}s")
    stage.sea_and_sky(calm=True)
    kit.render_setup(1280, 720, samples=int(os.environ.get("LOOK_SPP", "24")))
    cam = kit.camera("cam", 50.0)
    L = yard.h.L
    shots = {
        "bow": ((L * 0.75, -L * 0.42, L * 0.12), (L * 0.1, 0, 8), 45),
        "beam": ((0, -L * 1.05, L * 0.06), (0, 0, 9), 40),
        "top": ((0, -L * 0.05, L * 1.0), (0, 0, 0), 35),
        "quarter": ((-L * 0.7, L * 0.45, L * 0.2), (-L * 0.05, 0, 8), 45),
        "close": ((L * 0.36, -L * 0.16, L * 0.075), (L * 0.14, 0, 13), 40),
        "aft": ((-L * 0.4, -L * 0.14, L * 0.07), (-L * 0.2, 0, 10), 40),
        "low": ((L * 0.62, -L * 0.2, 4.0), (L * 0.2, 0, 14), 35),
        # (a small craft: the places above are a big ship's)
        "small": ((L * 0.95, -L * 0.75, L * 0.5), (L * 0.05, 0, L * 0.06), 45),
        "smallaft": ((-L * 0.9, -L * 0.7, L * 0.45), (0, 0, L * 0.06), 45),
        "smalllow": ((L * 1.25, -L * 0.55, L * 0.14), (0, 0, L * 0.09), 50),
    }
    for v in views:
        eye, tgt, lens = shots[v]
        cam.data.lens = lens
        kit.look_at(cam, eye, tgt)
        t = time.time()
        kit.still(os.path.join(out, f"{name}-{v}.png"))
        print(f"  {v}: {time.time() - t:.1f}s")


main()
