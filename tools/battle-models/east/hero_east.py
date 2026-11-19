"""One still for the Eastern Front: a KV-1, washed white, coming up the street of a burnt village in the snow with
riflemen beside her. Everything in it is the game's own models (the KV-1, soldier_SOV.glb, the eastern scenery).

    python hero_east.py <png> [--fast]
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..", "land")))

import sets_east  # noqa: E402

SOLDIER = "/mnt/project-files/battle-models/land/soldier/soldier_SOV.glb"
# (the glb carries paint names only, the game colours by them; these are soldier_sov.py's picture colours)
SOV_LOOK = {"tunic": "7B7748", "trousers": "6C6943", "boots": "26211D", "straps": "5B4A33", "helmet": "4E5A3A",
            "pack": "7A7154", "skin": "BDA08E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420", "leather": "3A2E25",
            "blanket": "675D4E"}
# (where each rifleman walks, his clip and how far into it: beside and behind the tank, on the verges)
MEN = [(17.0, -3.6, "walk", 0.1), (13.5, -4.4, "walk", 0.55), (10.0, -3.4, "walk", 0.3), (6.5, -4.6, "advance", 0.2),
       (15.0, 3.7, "walk", 0.8), (11.0, 4.3, "advance", 0.5), (7.0, 3.6, "walk", 0.9), (2.5, -4.0, "walk", 0.45)]


def extra(put):
    import bpy
    import ground
    import vehicles
    import works
    from kit import hex_rgb
    v = vehicles.build("KV1", True, turret="cast")
    ob, arm = works.to_scene(v.m, dust_hex=ground.KINDS["winter"][3])
    arm.location = (21.0, 0.4, 0.0)
    arm.rotation_euler = (0.0, 0.0, math.radians(4.0))
    for x, y, clip, at in MEN:
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=SOLDIER)
        new = [o for o in bpy.data.objects if o not in before]
        a = next(o for o in new if o.type == "ARMATURE")
        for o in new:
            for slot in getattr(o, "material_slots", []):
                m = slot.material
                hexc = SOV_LOOK.get(m.name.split(".")[0]) if m else None
                if hexc and m.node_tree:
                    b = m.node_tree.nodes.get("Principled BSDF")
                    b.inputs["Base Color"].default_value = (*hex_rgb(hexc), 1.0)
                    b.inputs["Roughness"].default_value = 0.4 if m.name.startswith(("steel", "eye")) else 0.85
        a.location = (x, y, 0.0)
        a.rotation_euler = (0.0, 0.0, math.radians(2.0 * (x % 3)))
        act = bpy.data.actions.get(clip) or next(ac for ac in bpy.data.actions if ac.name.startswith(clip))
        a.animation_data_create()
        a.animation_data.action = None
        tr = a.animation_data.nla_tracks.new()
        long = act.frame_range[1] - act.frame_range[0]
        tr.strips.new("pose", int(-at * long), act)
    bpy.context.scene.frame_set(0)


def main():
    out = sys.argv[1]
    fast = "--fast" in sys.argv
    sets_east.village("winter", out, fast, extra=extra, eye=(37.0, -7.2, 2.4), target=(0.0, 1.0, 2.2), lens=32.0, wash=0.85,
                      spp=12 if fast else 64, size=(960, 540) if fast else (1920, 1080))


if __name__ == "__main__":
    main()
