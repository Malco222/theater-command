"""Does a man of the crew pack fit at one of a work's crew sockets?

    python fit.py <Work> <socket> <clip> [with=fire] [offset=forward,right,up] [step=0.1] [reach=0.06] [skip=bone,bone]
                  [quiet] [crew=<crew.glb>]

Plays the soldier thread's clip (an action in their crew.glb) with his root on the socket, the work playing its own
clip beside him where `with=` names one, and measures every `step` seconds how deep the points of his body go into the
work's mesh. Prints the deepest point, when, on which bone, and where it is in the work's own axes (x forward, y to the
LEFT, z up, as the builders are written). The soldier thread's own check passes a man who goes no deeper than 1.2 cm;
this one gives the same depths as theirs at the places they have reported (the mortar pit's wall, the bomb box).

A point counts as inside when the nearest face of the work is within `reach` of it and faces away from it squarely
(a point off the edge of a face that looks the other way is not inside anything). Deeper than `reach` it is not seen.
Faces painted `void` (a bore's dark lining, a doorway's dark) are left out: they are drawn inside out on purpose.
His hands are left out unless `skip=` says otherwise: they are on the grips, the round or the bomb by design, and the
soldier thread measures them apart (its hand gap). `quiet` prints only the moments over 1.2 cm and the total.

`frames()` gives the same man, moment by moment, to any other measure (how near a wall he comes, where his feet are).
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

import kit  # noqa: E402
import scene  # noqa: E402

CREW = "/mnt/project-files/battle-models/land/soldier/crew/crew.glb"
NOT_HIS_BODY = ("rifle", "rifle_bolt")
HANDS = "hand_l,hand_r,fingers_l,fingers_r,thumb_l,thumb_r"
PASS = 0.012


def world_mesh(ob, deps, solid_only=False):
    """An object's mesh as it stands now: points in the world, faces, and each point's heaviest bone."""
    ev = ob.evaluated_get(deps)
    me = ev.to_mesh()
    mw = ev.matrix_world
    verts = [mw @ v.co for v in me.vertices]
    names = [s.material.name if s.material else "" for s in ev.material_slots]
    polys = [tuple(p.vertices) for p in me.polygons if not (solid_only and names and "void" in names[p.material_index])]
    groups = [max(v.groups, key=lambda g: g.weight).group if v.groups else -1 for v in me.vertices]
    ev.to_mesh_clear()
    return verts, polys, groups


def frames(name, socket, clip, piece_clip=None, offset=(0.0, 0.0, 0.0), step=0.1, crew=CREW):
    """Stands the work, puts the man at its socket and plays his clip: yields (t, item, [(point, bone), ...]) with the
    points of his body in the work's own axes."""
    kit.fresh()
    it = scene.Set().put(name, mount=False)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=crew)
    new = [o for o in bpy.data.objects if o not in before]
    man = next(o for o in new if o.type == "ARMATURE")
    body = [o for o in new if o.type == "MESH" and o.find_armature() == man and o.name.split(".")[0] not in NOT_HIS_BODY]
    holder = bpy.data.objects.new("man", None)
    bpy.context.scene.collection.objects.link(holder)
    for o in new:
        if o.parent is None:
            o.parent = holder
    act = bpy.data.actions[clip]
    man.animation_data_create()
    man.animation_data.action = act
    if getattr(act, "slots", None):
        man.animation_data.action_slot = act.slots[0]
    sc = bpy.context.scene
    fps = sc.render.fps / sc.render.fps_base              # (the importer lays the pack's keys out at the scene's rate)
    long = act.frame_range[1] / fps
    t = 0.0
    while t < long - 1e-6:
        if piece_clip:
            it.set(it.clip(piece_clip, t))
        p, d = it.where(socket)
        left = Vector((-d.y, d.x, 0.0))
        holder.location = p + d * offset[0] - left * offset[1] + Vector((0, 0, offset[2]))
        holder.rotation_euler = (0.0, 0.0, math.atan2(d.y, d.x))
        sc.frame_set(int(t * fps), subframe=(t * fps) % 1.0)
        deps = bpy.context.evaluated_depsgraph_get()
        inv = it.arm.matrix_world.inverted()
        pts = []
        for ob in body:
            verts, _, groups = world_mesh(ob, deps)
            names = [g.name for g in ob.vertex_groups]
            pts += [(inv @ v, names[g] if g >= 0 else ob.name) for v, g in zip(verts, groups)]
        yield t, it, pts
        t += step


def depth(it, pts, reach=0.06, skip=()):
    """How deep each point is inside the work as it is posed now: [(depth, bone, point), ...] for those inside."""
    deps = bpy.context.evaluated_depsgraph_get()
    wv, wp, _ = world_mesh(it.ob, deps, solid_only=True)
    inv = it.arm.matrix_world.inverted()
    tree = BVHTree.FromPolygons([inv @ v for v in wv], wp)
    out = []
    for v, bone in pts:
        if bone in skip:
            continue
        loc, nrm, _, dist = tree.find_nearest(v, reach)
        if loc is not None and (v - loc).dot(nrm) < -0.7 * dist:
            out.append((dist, bone, v))
    return out


def main():
    name, socket, clip = sys.argv[1:4]
    opt = dict(a.split("=", 1) for a in sys.argv[4:] if "=" in a)
    quiet = "quiet" in sys.argv[4:]
    skip = set(opt.get("skip", HANDS).split(","))
    off = [float(v) for v in opt.get("offset", "0,0,0").split(",")]
    print(f"{name} {socket}: {clip}" + (f" with the work's own `{opt['with']}`" if "with" in opt else "") + (f", root offset {off}" if any(off) else ""))
    worst, past = (0.0, None), 0
    for t, it, pts in frames(name, socket, clip, opt.get("with"), off, float(opt.get("step", 0.1)), opt.get("crew", CREW)):
        inside = depth(it, pts, float(opt.get("reach", 0.06)), skip)
        past += sum(1 for d in inside if d[0] > PASS)
        if not inside:
            continue
        d, bone, at = max(inside, key=lambda e: e[0])
        if not quiet or d > PASS:
            print(f"  {t:4.1f} s: {d * 100:4.1f} cm  {bone:12s} at ({at.x:+.2f}, {at.y:+.2f}, {at.z:+.2f}), {math.hypot(at.x, at.y):.2f} from the middle, bearing {math.degrees(math.atan2(at.y, at.x)):+.0f}")
        if d > worst[0]:
            worst = (d, (t, bone, tuple(round(c, 3) for c in at)))
    print(f"deepest {worst[0] * 100:.1f} cm" + (f" at {worst[1][0]:.1f} s, {worst[1][1]}, {worst[1][2]}" if worst[1] else "") + f"; points past 1.2 cm over the clip: {past}")


if __name__ == "__main__":
    main()
