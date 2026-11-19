"""Fit check on an exported soldier .glb (any army's): every clip, every 3rd frame at 60 fps, how deep each worn piece's
vertices sit inside the body (the uniform-and-skin mesh, eyes left out). A vertex counts as inside only when at least two
of three rays cast from it cross the body an odd number of times; its depth is then its distance to the body's surface.

    python fit_check.py soldier_ITA.glb out.json [--pieces pouches,details,helmet]   (the bpy module; nothing is rendered)

Default pieces: pouches, details and the helmet. Printed per clip: piece/paint, deepest cm @frame, (vertex-frames over
1.2 cm, the misfit line). A hat's or a puttee's hidden underside reads as inside in every frame, idle included: compare
with soldier_GER.glb before calling it a fault."""
import sys, json
sys.path.insert(0, '/home/user/tc-unreal/unreal/TheaterCommand/Tools/battle_units')
import kit, bpy, bmesh, numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
kit.fresh(60)
bpy.ops.import_scene.gltf(filepath=sys.argv[1])
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
meshes = {o.name.split('.')[0]: o for o in bpy.data.objects if o.type == 'MESH' and o.parent == arm}
body = next(o for n, o in meshes.items() if n.startswith('body'))
bm = bmesh.new(); bm.from_mesh(body.data)
print('body boundary edges', sum(1 for e in bm.edges if e.is_boundary), 'nonmanifold', sum(1 for e in bm.edges if not e.is_manifold), 'of', len(bm.edges))
# drop the eyes from the body for the test (they sit inside the head)
names = [m.name for m in body.data.materials]
keepf = [p.index for p in body.data.polygons if names[p.material_index] != 'eye']
sc = bpy.context.scene

def mat_of(o):
    names = [m.name for m in o.data.materials]
    vm = {}
    for p in o.data.polygons:
        for v in p.vertices:
            vm.setdefault(v, names[p.material_index] if names else '-')
    return [vm.get(i, '-') for i in range(len(o.data.vertices))]
VMAT = {n: mat_of(o) for n, o in meshes.items()}

def deformed(o, faces=None):
    dg = bpy.context.evaluated_depsgraph_get()
    oe = o.evaluated_get(dg); me = oe.to_mesh(); mw = o.matrix_world
    V = [mw @ v.co for v in me.vertices]
    F = [tuple(me.polygons[i].vertices) for i in (faces if faces is not None else range(len(me.polygons)))]
    oe.to_mesh_clear(); return V, F

DIRS = [Vector((1, 0.013, 0.007)).normalized(), Vector((0.011, 1, 0.017)).normalized(), Vector((0.009, 0.014, 1)).normalized()]
def inside(t, p):
    votes = 0
    for d in DIRS:
        n, q = 0, p.copy()
        for _ in range(64):
            loc, nrm, idx, dist = t.ray_cast(q, d)
            if loc is None: break
            n += 1; q = loc + d * 1e-5
        votes += n % 2
    return votes >= 2

WANT = sys.argv[sys.argv.index('--pieces') + 1].split(',') if '--pieces' in sys.argv else ['pouches', 'details', 'helmet']
NEW = [n for n in meshes if any(n.startswith(w) for w in WANT)]
res = {}
for act in sorted(bpy.data.actions, key=lambda a: a.name):
    arm.animation_data.action = act
    try:
        arm.animation_data.action_slot = act.slots[0]
    except Exception:
        pass
    f0, f1 = act.frame_range
    rec = {}
    for f in range(int(f0), int(f1) + 1, 3):
        sc.frame_set(f)
        BV, BF = deformed(body, keepf)
        t = BVHTree.FromPolygons(BV, BF)
        for n in NEW:
            V, _ = deformed(meshes[n])
            mats = VMAT[n]
            for i, p in enumerate(V):
                loc, nrm, idx, dist = t.find_nearest(p, 0.2)
                if loc is None or dist < 0.004:
                    continue
                if (p - loc).dot(nrm) > 0 and dist > 0.02:
                    continue
                if not inside(t, p):
                    continue
                k = f'{n}/{mats[i]}'
                r = rec.setdefault(k, [0.0, f, 0, None])
                if dist > 0.012: r[2] += 1
                if dist > r[0]:
                    r[0], r[1], r[3] = dist, f, [round(c, 3) for c in p]
    res[act.name] = {k: [round(v[0] * 100, 2), v[1], v[2], v[3]] for k, v in rec.items()}
    print(act.name, '|', '; '.join(f'{k} {v[0]} cm @{v[1]} (n>{v[2]})' for k, v in sorted(res[act.name].items(), key=lambda kv: -kv[1][0])), flush=True)
json.dump(res, open(sys.argv[2], 'w'), indent=1)
