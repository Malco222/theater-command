"""Writes the works (fortifications and battlefield defences) out for the game.

    python export.py <out folder> [name,name,...|all] [json glb fbx]

For each work: <Name>.json (the game's own model file: a fine mesh and a plain one, with `part`, `parts`, `sockets`
and `clips`, as the ships and aircraft have them, and `ground`: what the work does to the ground it stands on),
<Name>.glb and <Name>.fbx (the fine mesh on its skeleton, every clip an action). An index of them all is kept in
<out>/index-fortifications.json.
"""
import json
import os
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import bpy  # noqa: E402,F401

import ground  # noqa: E402
import kit  # noqa: E402
import look  # noqa: E402
import rigid  # noqa: E402
import scene  # noqa: E402
import works  # noqa: E402


# which sustained-fire machine gun each army's fixed defences had: air-cooled or water-cooled (the two look different)
MG = {"GER": "air", "UK": "water", "USA": "water", "SOV": "water", "FRA": "air", "ITA": "air", "JAP": "air", "POL": "water", "_default": "air"}
CUE = {"air": "lmg_burst_01", "water": "maxim_burst"}


def r3(v):
    return round(float(v), 3)


def ground_of(m):
    """What the work does to the ground, in the file's axes (y to the right): holes to cut in the terrain under it
    (`dig`, each an outline, with `dig_depth` how deep the model's own floor lies), trodden earth round it (`worn`: x, y,
    radius, strength), and where no grass or scatter should stand (`clear`)."""
    g = {}
    if m.info.get("dig"):
        g["dig"] = [[[r3(p[0]), r3(-p[1])] for p in d] for d in m.info["dig"]]
        g["dig_depth"] = r3(m.info.get("dig_depth", 0.0))
    if m.info.get("worn"):
        g["worn"] = [[r3(c[0]), r3(-c[1]), r3(c[2]), r3(c[3] if len(c) > 3 else 1.0)] for c in m.info["worn"]]
    if m.info.get("clear"):
        g["clear"] = [[[r3(p[0]), r3(-p[1])] for p in d] for d in m.info["clear"]]
    # (the ragged bare ring under and just beyond the piece's own thrown earth, so its bank runs into bare earth and
    # the earth into grass, never a mat with an edge; coarser here than the pictures' own, which is enough for a decal)
    skirt = ground.skirt(m, cell=1.2, reach=1.35)
    if skirt:
        g["skirt"] = [[r3(c[0]), r3(-c[1]), r3(c[2]), r3(c[3])] for c in skirt]
    paths = ground.paths_of(m)
    if paths:
        g["paths"] = [[[r3(p[0]), r3(-p[1])] for p in line] for line in paths]
    tracks = ground.tracks_of(m)
    if tracks:
        g["tracks"] = [[[r3(p[0]), r3(-p[1])] for p in line] for line in tracks]
    return g


def one(name, out, kinds):
    build = works.BUILDERS[name]
    os.makedirs(out, exist_ok=True)
    t = time.time()
    kit.fresh()
    far_m = build(False)
    far_ob, _ = rigid.to_blender(far_m, None, crease=50.0)
    far = rigid.gather(far_m, far_ob)
    kit.fresh()
    m = build(True)
    ob, arm = rigid.to_blender(m, look.works_look, crease=50.0)
    near = rigid.gather(m, ob)
    sizes = {}
    extra = {"nation": m.info.get("nation", "_default"), "ground": ground_of(m)}
    for k in ("takes", "level", "piece", "footprint", "line"):
        if k in m.info:
            extra[k] = m.info[k]
    if "takes" not in extra and name in scene.TAKES:
        extra["takes"] = scene.TAKES[name]
    if "json" in kinds:
        path = os.path.join(out, name + ".json")
        data = rigid.write_json(m, near, far, path, extra)
        keys = m.info.get("socket_keys", {})
        if keys:
            for s in data["sockets"]:
                s.update(keys.get(s["name"], {}))
            with open(path, "w") as f:
                json.dump(data, f, separators=(",", ":"))
        sizes["json"] = os.path.getsize(path)
    if "glb" in kinds or "fbx" in kinds:
        rigid.bake_actions(m, arm)
        if "glb" in kinds:
            sizes["glb"] = os.path.getsize(kit.export_glb(os.path.join(out, name + ".glb"), [ob, arm]))
        if "fbx" in kinds:
            sizes["fbx"] = os.path.getsize(kit.export_fbx(os.path.join(out, name + ".fbx"), [ob, arm]))
    print(f"{name}: {m.triangles()} triangles (plain {far_m.triangles()}), {len(m.parts)} parts, {len(m.sockets)} sockets, {len(m.clips)} clips; "
          + ", ".join(f"{k} {v / 1e6:.2f} MB" for k, v in sizes.items()) + f"; {time.time() - t:.0f}s", flush=True)
    return {"name": name, "title": m.title, "kind": m.kind, "nation": m.info.get("nation", "_default"), "triangles": m.triangles(), "plain": far_m.triangles(),
            "parts": [p.name for p in m.parts], "sockets": [s["name"] for s in m.sockets], "clips": {k: c.get("about", "") for k, c in m.clips.items()},
            **{k: m.info[k] for k in ("takes", "level", "piece") if k in m.info}}


def readme(out, index):
    """The folder's README: the hand-written head and a table of what was written."""
    head = open(os.path.join(HERE, "README.head.md")).read()
    rows = ["", "| Piece | What it is | Triangles (fine / plain) | Parts that move | Clips |", "|---|---|---|---|---|"]
    for name in sorted(index):
        e = index[name]
        moving = [p for p in e["parts"][1:]]
        rows.append(f"| `{name}` | {e['title']} | {e['triangles']:,} / {e['plain']:,} | {', '.join(moving[:8]) + (' ...' if len(moving) > 8 else '') if moving else 'none'} | {', '.join(e['clips']) or 'none'} |")
    with open(os.path.join(out, "README.md"), "w") as f:
        f.write(head + "\n".join(rows) + "\n")


def main():
    out = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 else "all"
    kinds = sys.argv[3:] or ["json", "glb", "fbx"]
    names = list(works.BUILDERS) if which == "all" else which.split(",")
    index_path = os.path.join(out, "index-fortifications.json")
    index = json.load(open(index_path)) if os.path.exists(index_path) else {}
    failed = []
    for name in names:
        try:
            index[name] = one(name, out, kinds)
        except Exception as e:  # noqa: BLE001
            traceback.print_exc()
            print("FAILED", name, e, flush=True)
            failed.append(name)
        with open(index_path, "w") as f:
            json.dump(index, f, indent=1)
    # the armies' own paints and guns: one mesh serves every army (ruling 66(e)); the game recolours the paints named
    # `upper` and `upper2` from the art director's unit table (ruling 78(f)), and picks the machine-gun pieces by
    # `machine_gun`
    import bits
    nations = {}
    for k, rows in sorted(bits.armies().items()):
        n = {"upper": rows[0][1], "upper2": rows[0][2], "machine_gun": MG.get(k, MG["_default"]), "cue_machine_gun": CUE[MG.get(k, MG["_default"])]}
        if len(rows) > 1:
            n["by_date"] = [[when, {"upper": a, "upper2": b, "what": what}] for when, a, b, what in rows]
        if bits.winter_wash(k):
            n["winter_wash"] = True
        nations[k] = n
    with open(os.path.join(out, "nations.json"), "w") as f:
        json.dump({"about": "Every work is one mesh for all armies. Paint `upper` (and `upper2`, the second camouflage colour) with the owner's colour below; nothing else changes. "
                            "`upper` is the country's `paint` in the art director's unit table (ruling 78(f)) and `upper2` its `paint2` where the table gives one; a tag not listed takes `_default`. "
                            "`winter_wash` true is the table's own: a worn white wash over the coat when the battle's ground is snow, as on that army's vehicles. "
                            "`by_date` [[from month YYYY-MM or null for the start, the two colours], ...] is paint that changed in the war, read as the unit table's swaps are: "
                            "Germany's guns are dark grey until early 1943 and dark yellow from 1943-02, the month its tanks change. "
                            "`machine_gun` says which machine-gun pieces that army's posts and pillboxes use: `air` = MachineGun, MGNest, Pillbox; `water` = MachineGun_water, MGNest_water, Pillbox_water.",
                   "table": bits.TABLE, "nations": nations}, f, indent=1)
    readme(out, index)
    print(f"done: {len(names) - len(failed)} written" + (f", FAILED: {', '.join(failed)}" if failed else ""), flush=True)


main()
