"""Writes the land vehicles out for the game.

    python export_land.py <out folder> [name,name,...|all] [json glb fbx]

For each: <out>/<Name>.json (the game's own model file, in the warships' format: a fine mesh and a plain one, `part`,
`parts`, `sockets`, `clips`; and for a vehicle `running` (what to roll: every wheel with its radius, each track with
its links and pitch) and `controls` (what the game lays: turret, gun, hatches)), and <Name>.glb and <Name>.fbx (the
fine mesh on its skeleton, every clip an action). The list of them all is kept in <out>/../../index-land.json.
"""
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402

import brushhand  # noqa: E402
import kit  # noqa: E402
import midmesh  # noqa: E402
import rigid  # noqa: E402
import vehicles  # noqa: E402
import works  # noqa: E402


MIDDLE_FROM = 25000      # (a vehicle under this many near triangles is drawn whole at middle distance: no middle mesh)


def add_middle(path, target=15000):
    """The middle mesh (40 to 140 m, the scale plan's TK2) into a written file, as `mid`: see midmesh.py. Its `part`
    numbers count the parts without the track links, as the far mesh's do."""
    with open(path) as f:
        d = json.load(f)
    if len(d["near"]["triangles"]) // 3 < MIDDLE_FROM:
        return 0
    mid = midmesh.middle(d, target=target)
    d["mid"] = {k: v for k, v in mid.items() if not k.startswith("_")}
    d["about"] += (" `mid`: the middle mesh, for about 40 to 140 m (~15,000 triangles): the near mesh thinned, small pieces gone, the far "
                   "mesh's track band in place of the links, alike parts (road wheels) carrying the same mesh; its `part` numbers count "
                   "the parts without the track links, as `far`'s do.")
    with open(path, "w") as f:
        json.dump(d, f, separators=(",", ":"))
    return len(d["mid"]["triangles"]) // 3


def one(name, out, kinds):
    t = time.time()
    kit.fresh()
    far_v = vehicles.build(name, False, whole=True)
    far_ob, _ = works.to_scene(far_v.m, look=None, crease=40.0, weighted=False)
    far = works.gather_land(far_v.m, far_ob)
    kit.fresh()
    v = vehicles.build(name, True, whole=True)
    m = v.m
    ob, arm = works.to_scene(m, look=None)
    near = works.gather_land(m, ob)
    # (the far mesh's `part` numbers count the parts WITHOUT the track links, as the plain build made them; the game's
    # LandKit and Tools/battle_vehicle.py map them back themselves, so they stay that way: do not renumber them)
    sizes = {}
    extra = {"running": v.running_table(), "nation": m.info.get("tag", ""), **vehicles.ABOUT.get(name, {})}
    if "json" in kinds:
        path = os.path.join(out, name + ".json")
        rigid.write_json(m, near, far, path, extra)
        add_middle(path)
        sizes["json"] = os.path.getsize(path)
    if "glb" in kinds or "fbx" in kinds:
        works.bake_actions(m, arm)
        if "glb" in kinds:
            sizes["glb"] = os.path.getsize(kit.export_glb(os.path.join(out, name + ".glb"), [ob, arm]))
        if "fbx" in kinds:
            sizes["fbx"] = os.path.getsize(kit.export_fbx(os.path.join(out, name + ".fbx"), [ob, arm]))
    tris, plain = len(near["triangles"]) // 3, len(far["triangles"]) // 3
    print(f"{name}: {tris} triangles (plain {plain}), {len(m.parts)} parts, {len(m.sockets)} sockets, {len(m.clips)} clips; "
          + ", ".join(f"{k} {s / 1e6:.1f} MB" for k, s in sizes.items()) + f"; {time.time() - t:.0f}s", flush=True)
    links = sum(b["links"] for b in v.belts.values())
    return {"name": name, "title": m.title, "kind": m.kind, **vehicles.ABOUT.get(name, {}), "triangles": tris, "plain": plain,
            "parts": [p.name for p in m.parts if not (p.kind == "free" and p.name.startswith("track_"))], "track_links": links,
            "sockets": [s["name"] for s in m.sockets], "clips": {k: c.get("about", "") for k, c in m.clips.items()},
            **({"crew_mark": m.info["file"]["name_place"]["kind"]} if m.info.get("file", {}).get("name_place", {}).get("ready", bool(m.info.get("file", {}).get("name_place"))) else {})}


def main():
    out = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 else "all"
    kinds = sys.argv[3:] or ["json", "glb", "fbx"]
    names = list(vehicles.BUILDERS) if which == "all" else which.split(",")
    os.makedirs(out, exist_ok=True)
    index_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(out))), "index-land.json")
    index = json.load(open(index_path)) if os.path.exists(index_path) else {}
    with open(os.path.join(os.path.dirname(os.path.abspath(out)), "brush-alphabet.json"), "w") as f:      # the crew's brush hand (b477), for the games
        json.dump(brushhand.table(), f, ensure_ascii=False, indent=1)
    for name in names:
        try:
            index[name] = one(name, out, kinds)
        except Exception as e:  # noqa: BLE001
            import traceback
            traceback.print_exc()
            print("FAILED", name, e, flush=True)
        with open(index_path, "w") as f:
            json.dump(index, f, indent=1)


if __name__ == "__main__":
    main()
