"""Writes the support teams' weapons out for the game (support_guns.py: each army's machine gun on its mount, and its
mortar).

    python export_support.py <out folder> [name,name,...|all] [json glb fbx]

For each: <out>/<Name>.json in the vehicles' file shape (a fine mesh `near` and a plain one `far`, `part`, `parts`,
`sockets`, `controls`), and <Name>.glb and <Name>.fbx (the fine mesh on its skeleton). No middle mesh (they are small),
no `running` table, and they are not listed in index-land.json; <out>/index-support.json lists them.
"""
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402,F401

import kit  # noqa: E402
import support_guns as props  # noqa: E402
import rigid  # noqa: E402
import works  # noqa: E402


def one(name, out, kinds):
    t = time.time()
    kit.fresh()
    far_v = props.BUILDERS[name](False)
    far_ob, _ = works.to_scene(far_v.m, look=None, crease=40.0, weighted=False)
    far = works.gather_land(far_v.m, far_ob)
    kit.fresh()
    v = props.BUILDERS[name](True)
    m = v.m
    ob, arm = works.to_scene(m, look=None)
    near = works.gather_land(m, ob)
    sizes = {}
    extra = {"nation": "any", **props.ABOUT.get(name, {})}
    if "json" in kinds:
        path = os.path.join(out, name + ".json")
        rigid.write_json(m, near, far, path, extra)
        sizes["json"] = os.path.getsize(path)
    if "glb" in kinds or "fbx" in kinds:
        works.bake_actions(m, arm)
        if "glb" in kinds:
            sizes["glb"] = os.path.getsize(kit.export_glb(os.path.join(out, name + ".glb"), [ob, arm]))
        if "fbx" in kinds:
            sizes["fbx"] = os.path.getsize(kit.export_fbx(os.path.join(out, name + ".fbx"), [ob, arm]))
    tris, plain = len(near["triangles"]) // 3, len(far["triangles"]) // 3
    print(f"{name}: {tris} triangles (plain {plain}), {len(m.parts)} parts, {len(m.sockets)} sockets; "
          + ", ".join(f"{k} {s / 1e3:.0f} KB" for k, s in sizes.items()) + f"; {time.time() - t:.1f}s", flush=True)
    return {"name": name, "title": m.title, "kind": m.kind, **props.ABOUT.get(name, {}), "triangles": tris, "plain": plain,
            "parts": [p.name for p in m.parts], "sockets": [s["name"] for s in m.sockets],
            "controls": sorted(m.info.get("file", {}).get("controls", {}))}


def main():
    out = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 else "all"
    kinds = sys.argv[3:] or ["json", "glb", "fbx"]
    names = list(props.BUILDERS) if which == "all" else which.split(",")
    os.makedirs(out, exist_ok=True)
    index_path = os.path.join(out, "index-support.json")
    index = json.load(open(index_path)) if os.path.exists(index_path) else {}
    for name in names:
        index[name] = one(name, out, kinds)
        with open(index_path, "w") as f:
            json.dump(index, f, indent=1)


if __name__ == "__main__":
    main()
