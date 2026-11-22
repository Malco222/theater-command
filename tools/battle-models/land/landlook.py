"""Quick looks at a vehicle while she is being built: stills from fixed places.

    python landlook.py <Name> <out folder> [views] [samples] [WxH] [pose=clip@t,clip@t] [turn=deg] [lift=deg] [gun= paint= mantlet= skirts= zimmerit=1]

views: front, rear, side, top, gear (the running gear, close), game (the game's camera: 16 m off, 30 degrees down),
low, turret, deck. Default: front,rear,gear,game.
"""
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import ground  # noqa: E402
import kit  # noqa: E402
import landmarks  # noqa: E402
import rigid  # noqa: E402
import vehicles  # noqa: E402
import works  # noqa: E402


def main():
    name, out = sys.argv[1], sys.argv[2]
    views = sys.argv[3].split(",") if len(sys.argv) > 3 and sys.argv[3] else ["front", "rear", "gear", "game"]
    spp = int(sys.argv[4]) if len(sys.argv) > 4 else 24
    w, h = (int(a) for a in (sys.argv[5] if len(sys.argv) > 5 else "1280x720").split("x"))
    opt = dict(a.split("=", 1) for a in sys.argv[6:] if "=" in a)
    kit.fresh()
    t = time.time()
    pick = {k: opt[k] for k in ("gun", "paint", "mantlet") if k in opt}       # (what a builder offers besides: gun=short paint=yellow mantlet=cone skirts=r4)
    if opt.get("skirts"):
        pick["skirts"] = True if opt["skirts"] in ("all", "1", "yes") else opt["skirts"]
    if opt.get("zimmerit"):
        pick["zimmerit"] = True
    v = vehicles.build(name, True, **pick)
    m = v.m
    if opt.get("look") == "file":         # what the game's file carries and nothing more: each paint flat, tinted by the points' own colours
        ob, arm = works.to_scene(m, look=None)
    else:
        ob, arm = works.to_scene(m, dust=float(opt.get("dust", 1.0)), wear=float(opt.get("wear", 1.0)))
    print(f"{name}: {m.triangles()} triangles, {len(m.parts)} parts, {len(m.sockets)} sockets, {len(m.clips)} clips, built in {time.time() - t:.1f}s", flush=True)
    pose = {}
    if "turn" in opt and "turret" in m.part_at:
        pose = rigid.merge(pose, rigid.aim(m, "turret", float(opt["turn"])))
    if "lift" in opt and "gun" in m.part_at:
        pose = rigid.merge(pose, rigid.aim(m, "gun", float(opt["lift"])))
    if "roof" in opt and "roof_mg" in m.part_at:          # roof=bearing[,lift]: the roof machine gun swung and tipped
        a, _, b = opt["roof"].partition(",")
        pose = rigid.merge(pose, rigid.aim(m, "roof_mg", float(a)))
        if b:
            pose = rigid.merge(pose, rigid.aim(m, "roof_mg_cradle", float(b)))
    for item in (opt.get("pose", "").split(",") if opt.get("pose") else []):
        clip, at = item.split("@")
        pose = rigid.merge(pose, rigid.pose_of(m, m.clips[clip], float(at)))
    if opt.get("lids") == "wrong":                       # (for a before picture: every lid turned the way it went before 22 November, into the hull)
        for p in m.parts:
            if p.note == "a hatch: + opens" and p.name in pose:
                q, mv, sc = pose[p.name]
                pose[p.name] = (q.conjugated(), mv, sc)
    if pose:
        rigid.apply_pose(arm, pose, model=m)
    landmarks.put(m, ob, arm, name=opt.get("name"), rings=int(opt.get("rings", 0)))
    rut = abs(next(iter(v.belts.values()))["y"]) if v.belts else 0.9
    towed = not v.belts                                   # (a gun or a lorry: no tank has run where she stands)
    if opt.get("ground", "meadow") == "battle":
        field = ground.Battle(rut=rut, ruts=not towed)
    else:
        field = ground.Field(kind=opt.get("ground", "meadow"), rut=400.0 if towed else rut, density=float(opt.get("grass", 0.0)), trees=opt.get("trees") == "1")
    sun = field.sun
    kit.render_setup(w, h, samples=spp)
    cam = kit.camera("cam", 50.0)
    xs = [p.x for p in m.verts]
    zs = [p.z for p in m.verts]
    L, H = max(xs) - min(xs), min(max(zs), 1.36 * works.body_top(m))
    cx = (max(xs) + min(xs)) / 2
    shots = {
        "front": ((cx + L * 1.05, -L * 0.85, H * 1.0), (cx + 0.3, 0, H * 0.36), 50),
        "rear": ((cx - L * 1.05, L * 0.85, H * 1.0), (cx - 0.3, 0, H * 0.36), 50),
        "side": ((cx, -L * 1.6, H * 0.5), (cx, 0, H * 0.4), 50),
        "top": ((cx - 0.01, -0.3, L * 1.7), (cx, 0, 0), 50),
        "gear": ((cx + L * 0.46, -L * 0.42, 0.62), (cx + L * 0.26, -1.0, 0.5), 50),
        "game": ((cx + 16 * math.cos(math.radians(30)) * 0.8, -16 * math.cos(math.radians(30)) * 0.6, 16 * math.sin(math.radians(30))), (cx, 0, H * 0.3), 50),
        # (as the battle's own camera sees her: 45 m off and 22 degrees up, small in the frame)
        "battle": ((cx + 45 * math.cos(math.radians(22)) * 0.7, -45 * math.cos(math.radians(22)) * 0.714, 45 * math.sin(math.radians(22))), (cx, 0, H * 0.3), 70),
        "low": ((cx + L * 1.0, -L * 0.5, 0.9), (cx + 0.3, 0, H * 0.45), 40),
        "turret": ((cx + L * 0.48, -L * 0.4, H * 1.08), (cx + 0.4, 0, H * 0.7), 60),
        "deck": ((cx - L * 0.6, -L * 0.34, H * 1.3), (cx - L * 0.2, 0, H * 0.5), 55),
        "rearlow": ((cx - L * 0.9, -L * 0.55, 1.0), (cx - L * 0.2, 0, H * 0.4), 50),
        "nose": ((cx + L * 0.62, -L * 0.2, H * 0.62), (cx + L * 0.36, 0.1, H * 0.36), 55),
    }
    if "roof_mg_cradle" in m.part_at:                     # close on the roof machine gun: from ahead and to her right, and from behind the commander
        rt = m.parts[m.part_at["roof_mg_cradle"]].pivot
        shots["roofgun"] = (tuple(rt + Vector((1.7, -1.6, 0.75))), tuple(rt + Vector((0.15, 0.0, -0.12))), 45)
        shots["roofback"] = (tuple(rt + Vector((-1.9, -1.2, 0.95))), tuple(rt + Vector((0.25, 0.0, -0.15))), 45)
    os.makedirs(out, exist_ok=True)
    for vw in views:
        eye, tgt, lens = shots[vw]
        cam.data.lens = lens
        kit.look_at(cam, eye, tgt)
        # the sun stands to one side of the camera, so the faces toward it are lit and the shadow falls away
        az = math.degrees(math.atan2(eye[1] - tgt[1], eye[0] - tgt[0])) + float(opt.get("sun", 48.0))
        sun.rotation_euler = (math.radians(90.0 - float(opt.get("elev", 40.0))), 0.0, math.radians(az + 90.0))
        t = time.time()
        kit.still(os.path.join(out, f"{name}-{vw}.png"))
        print(f"  {vw}: {time.time() - t:.1f}s", flush=True)


main()
