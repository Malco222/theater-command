"""Write the Asian scenery models as game files: <Name>.json (near, mid, far) and <Name>.glb, and index-asia.json.

    python export.py <out folder> [Name,Name,...]

Run with Blender's Python (bpy). The json is the vehicles' format cut down for things that stand still (see
scenery_io.py); the glb carries the same three levels as <Name>, <Name>_LOD1 and <Name>_LOD2, plain colours per paint and the sway as the point attribute _SWAY.
"""
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402,F401  (first: kit needs Blender loaded)
import kit  # noqa: E402
import scenery_io  # noqa: E402
from catalogue import BUILDERS, build  # noqa: E402


def export(name, out):
    kit.fresh()
    geos, objs = {}, []
    for lod, key in ((0, "near"), (1, "mid"), (2, "far")):
        geo = build(name, lod)
        ob = scenery_io.to_blender(geo, name if lod == 0 else f"{name}_LOD{lod}")
        geos[key] = (geo, ob)
        objs.append(ob)
    near = geos["near"][0]
    title = near.info.get("title", name) + " (our own design)"
    extra = {"foot_m": near.info.get("foot_m")}          # (high_m is measured from the near mesh in write_model)
    data = scenery_io.write_model(os.path.join(out, f"{name}.json"), title, geos, extra)
    scenery_io.export_glb(os.path.join(out, f"{name}.glb"), objs)
    return {"name": name, "title": title, "triangles": data["triangles"]["near"], "mid": data["triangles"]["mid"],
            "plain": data["triangles"]["far"], "high_m": data["high"], "foot_m": extra["foot_m"]}


def main():
    out = sys.argv[1]
    names = sys.argv[2].split(",") if len(sys.argv) > 2 and sys.argv[2] else list(BUILDERS)
    os.makedirs(out, exist_ok=True)
    idx_path = os.path.join(os.path.dirname(os.path.abspath(out)), "index-asia.json")
    index = {}
    if os.path.exists(idx_path):
        with open(idx_path) as f:
            index = json.load(f)
    for name in names:
        t0 = time.time()
        index[name] = export(name, out)
        print(f"{name}: {index[name]['triangles']} / {index[name]['mid']} / {index[name]['plain']} triangles, "
              f"{time.time() - t0:.0f}s", flush=True)
    with open(idx_path, "w") as f:
        json.dump(index, f, indent=1)


if __name__ == "__main__":
    main()
