"""A close look at one aircraft, for judging her detail.

    python airlook.py <name> <out folder> [view,view,...] [WxHxSPP] [clip=name@t ...]

views:  front   from ahead and to port, a little above (the sheet's view, closer)
        hood    close on her hood and nose, from ahead and above
        rear    from astern and to starboard, above
        over    from astern and above, a little to port (for a bank, the flaps, the control surfaces)
        under   from below and ahead
        tail, waist, belly   close on a bomber's tail guns, her waist windows, her ball turret
        side    square on her port side
A clip named after the views is laid over `fly` at the time given (bank.port@0.9, watch@2.0, flaps.down@1.5).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402

import kit  # noqa: E402
import stage  # noqa: E402
from airshow import Air, Plane  # noqa: E402


def main():
    name, out = sys.argv[1], sys.argv[2]
    views = sys.argv[3].split(",") if len(sys.argv) > 3 else ["front", "hood", "rear", "under"]
    w, h, spp = (int(v) for v in (sys.argv[4] if len(sys.argv) > 4 else "1280x720x12").split("x"))
    extra = [a.split("=", 1)[1] for a in sys.argv[5:] if a.startswith("clip=")]
    os.makedirs(out, exist_ok=True)
    A = Air(clouds=12.0)
    stage.render_setup(w, h, spp)
    p = Plane(A, name)
    poses = [p.play("fly", 0.6)] + [p.play(c.split("@")[0], float(c.split("@")[1])) for c in extra]
    p.put((0, 0, 0), *poses)
    A.at(0.0)
    s, L = p.f.half * 2.0, p.f.long
    hood = next((k["at"] for k in p.m.sockets if k["name"] == "hatch"), None)
    hx, hz = (hood.x, hood.z) if hood is not None else (0.0, 1.0)
    shots = {"front": ((s * 0.62, s * 0.56, s * 0.3), (L * 0.02, 0, 0.0), 50.0),
             "hood": ((hx + 3.4 + L * 0.12, 2.6 + L * 0.06, hz + 1.5), (hx + 0.2, 0, hz - 0.45), 50.0),
             "rear": ((-s * 0.62, -s * 0.5, s * 0.34), (-L * 0.08, 0, 0.1), 50.0),
             "over": ((-s * 0.55, s * 0.25, s * 0.6), (0, 0, 0.0), 50.0),
             "tail": ((p.f.hull.back - 3.6, 3.3, 1.5), (p.f.hull.back + 1.3, 0, 0.45), 50.0),
             "waist": ((-L * 0.22 - 3.2, 6.4, 1.5), (-L * 0.22, 0, 0.2), 50.0),
             "belly": ((-L * 0.17 + 4.6, 5.2, -3.4), (-L * 0.17, 0, -0.9), 50.0),
             "under": ((s * 0.5, s * 0.42, -s * 0.4), (0, 0, -0.1), 50.0),
             "side": ((0.0, s * 1.0, 0.6), (-L * 0.06, 0, 0.2), 50.0)}
    for v in views:
        eye, look, lens = shots[v]
        A.cam.data.lens = lens
        kit.look_at(A.cam, eye, look)
        path = os.path.join(out, f"{name}-{v}.png")
        kit.still(path)
        print("made", path, flush=True)


main()
