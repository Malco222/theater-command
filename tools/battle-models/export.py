"""Writes the battle models out for the game.

    python export.py <out folder> [name,name,...|ships|planes|all] [json|glb|fbx ...]

For each model: <name>.json (the game's own model file: a fine mesh and a plain one, with `part`, `parts`, `sockets`
and `clips` added), and <name>.glb and <name>.fbx (the fine mesh on its skeleton, every clip an action) for Unreal's
own importers or any viewer. Ships go to <out>/sea, aircraft to <out>/air.
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402

import deckpark  # noqa: E402
import kit  # noqa: E402
import planes  # noqa: E402
import rigid  # noqa: E402
import ships  # noqa: E402
import stage  # noqa: E402

AIR = dict(planes.BUILDERS)
AIR["Parachute"] = lambda fine=True: (planes.parachute(fine), None)
AIR["Bomb"] = lambda fine=True: (planes.bomb(fine), None)
AIR["Torpedo"] = lambda fine=True: (planes.torpedo(fine), None)


def one(name, out, kinds):
    air = name in AIR
    park = name in deckpark.BUILDERS                         # (a range of aircraft for a carrier's deck: painted as aircraft are, kept with the ships)
    build = AIR[name] if air else deckpark.BUILDERS[name] if park else ships.BUILDERS[name]
    folder = os.path.join(out, "air" if air else "sea")
    os.makedirs(folder, exist_ok=True)
    t = time.time()
    crease = 50.0 if air or park else 38.0
    kit.fresh()
    far_m, _ = build(False)
    far_ob, _ = rigid.to_blender(far_m, None, crease=crease)
    far = rigid.gather(far_m, far_ob)
    kit.fresh()
    m, _ = build(True)
    stage.PICTURES = False                               # (the files take their paints plain: see stage.ship_look)
    ob, arm = rigid.to_blender(m, None if air or park else stage.ship_look, crease=crease)
    near = rigid.gather(m, ob)
    sizes = {}
    if "json" in kinds:
        path = os.path.join(folder, name + ".json")
        rigid.write_json(m, near, far, path)
        sizes["json"] = os.path.getsize(path)
    if "glb" in kinds or "fbx" in kinds:
        rigid.bake_actions(m, arm)
        if "glb" in kinds:
            sizes["glb"] = os.path.getsize(kit.export_glb(os.path.join(folder, name + ".glb"), [ob, arm]))
        if "fbx" in kinds:
            sizes["fbx"] = os.path.getsize(kit.export_fbx(os.path.join(folder, name + ".fbx"), [ob, arm]))
    line = (f"{name}: {m.triangles()} triangles (plain {far_m.triangles()}), {len(m.parts)} parts, {len(m.sockets)} sockets, {len(m.clips)} clips; "
            + ", ".join(f"{k} {v / 1e6:.1f} MB" for k, v in sizes.items()) + f"; {time.time() - t:.0f}s")
    print(line, flush=True)
    return {"name": name, "title": m.title, "kind": m.kind, "triangles": m.triangles(), "plain": far_m.triangles(), "parts": [p.name for p in m.parts],
            "sockets": [s["name"] for s in m.sockets], "clips": {k: c.get("about", "") for k, c in m.clips.items()}}


def main():
    out = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 else "all"
    kinds = sys.argv[3:] or ["json", "glb", "fbx"]
    names = {"ships": list(ships.BUILDERS) + list(deckpark.BUILDERS), "planes": list(AIR), "parks": list(deckpark.BUILDERS),
             "all": list(ships.BUILDERS) + list(deckpark.BUILDERS) + list(AIR)}.get(which) or which.split(",")
    import json
    index_path = os.path.join(out, "index-sea-air.json")
    index = json.load(open(index_path)) if os.path.exists(index_path) else {}
    for name in names:
        try:
            index[name] = one(name, out, kinds)
        except Exception as e:  # noqa: BLE001
            import traceback
            traceback.print_exc()
            print("FAILED", name, e, flush=True)
        with open(index_path, "w") as f:
            json.dump(index, f, indent=1)


main()
