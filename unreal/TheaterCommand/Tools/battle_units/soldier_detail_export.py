"""The detail pilot (soldier_detail.py's German rifleman, passed by the art director 24 November 2026, round 2) in the
game's own form, as a file of its own beside the German's: soldier_GER_detail.json. Release 2, switched off: the game
draws it only with -TheaterLandDetailMen (Content/Models/Soldiers/Detail/soldier_GER.json); soldier_GER.json is not
touched.

Two steps (then the file chain: variety, shade, brim_mend, face_mesh and face_detail, pack_range, falls, run_styles,
field_clips, riders fit, the gore twin; the order and commands are in the README's "The detailed rifleman" section):

  --export   (Blender python) the man made as soldier.py's export_all makes him (assemble, bake_clips, make_lods),
             with soldier_detail.py's patches; his tunic and shoulder-strap buttons given a paint of their own
             (`button`, a dull pebbled grey, not the army's dark steel: at a metre they had read as holes); written by
             soldier.py's write_game_json to WORK/soldier_GER_detail.fresh.json. Refuses unless the triangles match
             the man the art director passed (/home/user/art/detail/tris.json).
  --combine  (any python) the fresh file's geometry, bones and sockets with the shipping German's clips and tables
             (deaths, run styles, riders, bail: each later tool strips and replaces its own, so the file is whole at
             every step) to WORK/soldier_GER_detail.json, then copied to the shared folder (a new name: nothing of the
             German's is written).

    cd Tools/battle_units
    DETAIL_WORK=/home/user/art/detail_export/ PYTHONPATH=/home/user/work/looks/battle_units:. \\
        /home/user/bpyenv/bin/python soldier_detail_export.py --export
    /home/user/bpyenv/bin/python soldier_detail_export.py --combine
"""
import hashlib
import json
import os
import sys

WORK = os.environ.setdefault("DETAIL_WORK", "/home/user/art/detail_export/")
SHARED = "/mnt/project-files/battle-models/land/soldier/"
NAME = "soldier_GER_detail"
PASSED = "/home/user/art/detail/tris.json"         # (the build the art director passed, round 2)
BUTTON, BUTTON_RGB = 48, "86877F"                   # (dull pebbled grey metal: about 70-90 lit, the AD's carry)
BUTTON_OBJECTS = ("details", "shoulder_straps")     # (their steel is the buttons; buckles, studs and rings stay steel)
FROM_FRESH = ("about", "long", "high", "marks", "near", "mid", "far", "bones", "sockets")
FROM_BASE = ("clips", "deaths", "about_deaths", "about_run_styles", "ride_clips", "about_riders", "bail_clips",
             "about_bail")
REBUILT_LATER = ("faces", "helmets", "inks", "helmet_tris", "about_variety", "about_wear", "face_mesh", "face_detail",
                 "pack_tris", "about_pack")


def export():
    import soldier_detail as SD
    import bpy
    import kit
    S = SD.S
    assert SD.WORK.rstrip("/").endswith("detail_export"), SD.WORK
    sc = kit.fresh(S.FPS)
    m = S.assemble("GER")
    S.bake_clips(m)
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    sc.frame_set(0)
    n = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    parts = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    ref = json.load(open(PASSED))
    print("built", n, flush=True)
    if n != {int(k): v for k, v in ref["tris"].items()} or parts != ref["parts"]:
        raise SystemExit(f"not the man the art director passed: {n} against {ref['tris']}")
    # the buttons' own paint
    S.FIGURE_PAINTS["button"] = BUTTON
    mat = bpy.data.materials.new("button")
    moved = 0
    for lod in lods.values():
        for ob in lod:
            if not ob.name.startswith(BUTTON_OBJECTS):
                continue
            me = ob.data
            names = [mt.name.split(".")[0] for mt in me.materials]
            if "steel" not in names:
                continue
            si = names.index("steel")
            me.materials.append(mat)
            bi = len(me.materials) - 1
            for p in me.polygons:
                if p.material_index == si:
                    p.material_index = bi
                    moved += 1
    print(f"buttons: {moved} faces to paint {BUTTON}", flush=True)
    os.makedirs(WORK, exist_ok=True)
    path = os.path.join(WORK, NAME + ".fresh.json")
    data = S.write_game_json(path, m, lods)
    D = json.load(open(path))
    D["new_paints"] = {"button": BUTTON}
    D["colours"] = {"button": BUTTON_RGB}
    _write(path, D)
    print("wrote", path, {k: (len(D[k]["points"]) // 3, len(D[k]["triangles"]) // 3) for k in ("near", "mid", "far")},
          flush=True)
    return data


def _write(path, D):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(D, f, separators=(",", ":"))
    json.load(open(tmp))
    os.replace(tmp, path)


def _sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def combine():
    fresh = json.load(open(os.path.join(WORK, NAME + ".fresh.json")))
    base_path = os.path.join(SHARED, "soldier_GER.json")
    base_sha = _sha(base_path)
    base = json.load(open(base_path))
    if [(b["name"], b.get("parent")) for b in fresh["bones"]] != [(b["name"], b.get("parent")) for b in base["bones"]]:
        raise SystemExit("the skeleton differs from the German's: his clips would not fit")
    out = {k: v for k, v in base.items() if k not in REBUILT_LATER and k not in FROM_FRESH
           and k not in ("new_paints", "colours")}
    for k in FROM_FRESH:
        out[k] = fresh[k]
    for k in FROM_BASE:
        if k in base:
            out[k] = base[k]
    out["new_paints"] = dict(fresh.get("new_paints", {}))
    out["colours"] = dict(fresh.get("colours", {}))
    out["name"] = "Battle rifleman, GER (detail pilot)"
    out["pack_cloth"] = {"5": ["neck", "upperarm_l", "upperarm_r"], str(BUTTON): []}  # (collar patches, shoulder
    #  straps and buttons: sewn on the tunic, not webbing, for pack_range.py)

    # soldier.py's own clips: the fresh ones where they differ from the shipping file's (beside the pouches' seats)
    def bare(c):
        c = dict(c)
        c["tracks"] = [t for t in c.get("tracks", []) if not str(t.get("part_name", t.get("part", ""))).startswith("pouch")]
        return c                          # (compared as numbers: a written 0.0 and -0.0 are the same key)
    diff, swapped = [], 0
    clips = dict(base["clips"])
    for k, c in fresh["clips"].items():
        if k not in clips:
            clips[k] = c
            diff.append(k + " (new)")
        elif bare(c) != bare(clips[k]):
            clips[k] = c
            diff.append(k)
        elif c != clips[k]:
            clips[k] = c                  # (only the pouches' seats differ: the new pouches ride where they are)
            swapped += 1
    out["clips"] = clips
    print(f"soldier.py clips: {len(fresh['clips'])}; taken fresh where they differ: {diff or 'none'}; "
          f"pouch seats only: {swapped}")
    if out.get("deaths"):
        named = [d for v in out["deaths"].values() for d in (v if isinstance(v, list) else [v])]
        missing = [d for d in named if isinstance(d, str) and d not in clips]
        if missing:
            print("  (deaths naming clips not in the file:", missing[:8], ")")
    path = os.path.join(WORK, NAME + ".json")
    _write(path, out)
    dest = os.path.join(SHARED, NAME + ".json")
    if os.path.exists(dest):
        raise SystemExit(f"{dest} exists: the chain has begun on it; move it aside first")
    tmp = dest + ".part"
    with open(path, "rb") as a, open(tmp, "wb") as b:
        b.write(a.read())
    os.replace(tmp, dest)
    assert _sha(base_path) == base_sha
    print("wrote", path, "and", dest, {k: len(out[k]["points"]) // 3 for k in ("near", "mid", "far")},
          f"{len(clips)} clips")


if __name__ == "__main__":
    if "--export" in sys.argv:
        export()
    if "--combine" in sys.argv:
        combine()
