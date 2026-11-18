"""A sheet of small pictures of many ships: python sheet.py <out.png> <view[+view]> <name,name,...> [cols] [WxHxSPP]

With two views (bow+beam) and two columns each ship has a row: from the bow quarter, and on the broadside."""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402

import kit  # noqa: E402
import rigid  # noqa: E402
import ships  # noqa: E402
import stage  # noqa: E402


def main():
    out, view, names = sys.argv[1], sys.argv[2], sys.argv[3].split(",")
    cols = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    w, h, spp = (int(v) for v in (sys.argv[5] if len(sys.argv) > 5 else "640x360x10").split("x"))
    tmp = os.path.splitext(out)[0] + "-parts"
    views = view.split("+")
    pics, labels = [], []
    for name in names:
        kit.fresh()
        t = time.time()
        try:
            m, yard = ships.BUILDERS[name](True)
        except Exception as e:  # noqa: BLE001
            import traceback
            traceback.print_exc()
            print("FAILED", name, e, flush=True)
            continue
        ob, arm = rigid.to_blender(m, stage.ship_look)
        stage.Sea(wave=1.2, cells=360, fine=3.0)
        stage.render_setup(w, h, spp)
        cam = kit.camera("cam", 40.0)
        cam.data.clip_end = 40000.0
        L = yard.h.L
        k = min(1.0, L / 110.0)                 # (a small craft is looked at from higher, and at her own height)
        shots = {"bow": ((L * 0.92, -L * 0.66, L * (0.24 + 0.2 * (1 - k))), (L * 0.04, 0, 6 * k), 40),
                 "beam": ((0, -L * 1.3, L * (0.16 + 0.1 * (1 - k))), (0, 0, 5 * k), 40),      # on the broadside, a little above: every turret shows
                 "top": ((0, -L * 0.03, L * 1.05), (0, 0, 0), 38), "quarter": ((-L * 0.72, -L * 0.52, L * (0.22 + 0.2 * (1 - k))), (-L * 0.03, 0, 8 * k), 40)}
        title = m.title.replace(' (our own model)', '').replace(' layout', '')
        for i, v in enumerate(views):
            eye, tgt, lens = shots[v]
            cam.data.lens = lens
            kit.look_at(cam, eye, tgt)
            path = os.path.join(tmp, f"{name}-{v}.png")
            kit.still(path)
            pics.append(path)
            labels.append(f"{title} · {L:.0f} m · {m.triangles() / 1000:.0f}k triangles · {len(m.parts)} parts" if i == 0 else f"{title} · {'on the broadside' if v == 'beam' else v}")
        print(f"{name}: {m.triangles()} triangles, {len(m.parts)} parts, {len(m.sockets)} sockets, {len(m.clips)} clips, {time.time() - t:.0f}s", flush=True)
    kit.sheet(pics, out, cols=cols, labels=labels)
    print("made", out)


main()
