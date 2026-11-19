"""Still pictures of a ship for the reviews: her whole length from a few bearings, and close looks.

    python stills.py <name> <out folder> [views=quarter,beam,bow,stern,top,close:x:y:z:tx:ty:tz] [size=1600x900] [samples=24] [clip=name@t,...]

Each view is a camera set by the ship's length (`quarter` from ahead and above to port, `beam` square on, `bow` low and
fine on the bow, `stern` from astern, `top` from high above) or by hand: `close:x:y:z:tx:ty:tz` puts the camera at
(x, y, z) in her own metres looking at (tx, ty, tz). She lies at rest on a quiet sea with her guns trained a little
out, funnel smoke rising, her aircraft ranged if she is a carrier. Pictures are written as <out>/<name>-<view>.png.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402,F401
from mathutils import Quaternion  # noqa: E402

import deckpark  # noqa: E402
import kit  # noqa: E402
import rigid  # noqa: E402
import ships  # noqa: E402
import stage  # noqa: E402


def train(m, bearing, lift, which=("turret.", "director.", "sec.")):
    out = {}
    for p in m.parts:
        if not p.name.startswith(which) or p.kind != "turn":
            continue
        if ".gun." in p.name:
            out.update(rigid.aim(m, p.name, lift))
            continue
        rest = getattr(p, "rest", 0.0)
        turn = ((bearing - rest + 180.0) % 360.0) - 180.0
        if p.limits:
            turn = min(max(turn, p.limits[0]), p.limits[1])
        out.update(rigid.aim(m, p.name, turn))
    return out


def views_for(L, B, H):
    return {
        "quarter": ((0.78 * L, 0.78 * L, 0.3 * L), (-0.04 * L, 0.0, H * 0.3)),
        "beam": ((0.0, 1.25 * L, 0.07 * L), (0.0, 0.0, H * 0.35)),
        "bow": ((0.78 * L, 0.2 * L, 0.05 * L), (0.18 * L, 0.0, H * 0.4)),
        "stern": ((-0.7 * L, -0.42 * L, 0.2 * L), (-0.05 * L, 0.0, H * 0.35)),
        "top": ((-0.15 * L, 0.38 * L, 0.85 * L), (-0.02 * L, 0.0, 0.0)),
        "aftquarter": ((-0.72 * L, 0.72 * L, 0.3 * L), (0.02 * L, 0.0, H * 0.3)),
    }


def main():
    name, out = sys.argv[1], sys.argv[2]
    opt = dict(a.split("=", 1) for a in sys.argv[3:])
    w, h = (int(v) for v in opt.get("size", "1600x900").split("x"))
    os.makedirs(out, exist_ok=True)
    tmp = os.path.join(out, f".{name}-tmp")
    os.makedirs(tmp, exist_ok=True)
    kit.fresh()
    m, yard = ships.BUILDERS[name](True)
    ships.in_month(m, opt.get("month"))
    ob, arm = rigid.to_blender(m, stage.ship_look)
    deckpark.range_on(m, arm, opt.get("month"))
    hl = yard.h
    L, B = hl.L, hl.B
    sea = stage.Sea(speed=float(opt.get("speed", 6.0)))
    bow, stern = hl.frame_x(1.0), hl.frame_x(-1.0)
    sea.wake(hl.L, hl.B, tmp, half_of=lambda x: hl.half_water(hl.u_of(x)) if stern < x < bow else 0.0, bow=bow, stern=stern)
    fx = stage.Effects(wind=(-7.0, 1.5, 0.0))
    pose = rigid.merge(rigid.pose_of(m, m.clips["sail"], 2.0), train(m, float(opt.get("bearing", 30.0)), float(opt.get("lift", 8.0))))
    for c in filter(None, opt.get("clip", "").split(",")):         # clip=hangar.open@5,launch@1.2: her pose at those moments of those clips
        cn, ct = c.split("@")
        pose = rigid.merge(pose, rigid.pose_of(m, m.clips[cn], float(ct)))
    rigid.apply_pose(arm, pose, model=m)
    for s in m.sockets:
        if s["name"].startswith("funnel."):
            fx.funnel_smoke(lambda s=s: rigid.place(m, pose, s["name"]), thick=0.35, r=2.0)
    t = -14.0
    while t <= 0.0:
        fx.at(t)
        t += 0.25
    sea.at(0.0)
    cam = kit.camera("cam", float(opt.get("lens", 35.0)))
    cam.data.clip_end = 40000.0
    stage.render_setup(w, h, int(opt.get("samples", 24)))
    H = max(p.pivot.z for p in m.parts if p.pivot is not None) if m.parts else 20.0
    std = views_for(L, B, H)
    for v in opt.get("views", "quarter,beam,bow,stern,top").split(","):
        if v.startswith("close:"):
            _, *nums = v.split(":")
            label = nums[6] if len(nums) > 6 else "close"
            x, y, z, tx, ty, tz = (float(n) for n in nums[:6])
            at, look = (x, y, z), (tx, ty, tz)
        else:
            label = v
            at, look = std[v]
        kit.look_at(cam, at, look)
        path = os.path.join(out, f"{name}-{label}.png")
        bpy.context.scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print("made", path, flush=True)


main()
