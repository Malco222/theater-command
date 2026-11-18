"""Re-imports .glb files and prints bones, actions (clips), triangles and sockets: gv_t_glb.py <file.glb> ..."""
import sys
args = sys.argv[1:]
sys.argv = [sys.argv[0]]
import bpy
for path in args:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    tris = sum(sum(len(p.vertices) - 2 for p in m.data.polygons) for m in meshes)
    print(path.split("/")[-1], "bones", len(arms[0].data.bones) if arms else 0, "actions", sorted(a.name for a in bpy.data.actions), "tris", tris,
          "empties", len([o for o in bpy.data.objects if o.type == "EMPTY"]))
