"""Stills of a delivered soldier file, to check a nation's kit: standing, and at a chosen moment of chosen clips.

    python soldier_look.py <soldier_X.glb> <out.png> [clips=idle,kneel_down,go_prone,crawl] [at=0.8] [w=640] [h=800]

`at` is how far through each clip (0 to 1). One picture per pose, side by side.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402

import kit  # noqa: E402

# The pictures' colours (soldier.py's LOOK rows; the game paints by number)
LOOKS = {
    "JAP": {"tunic": "A08856", "trousers": "9A8452", "boots": "4A3626", "lower": "948052", "straps": "5A4632", "helmet": "9C8656",
            "pack": "7E7050", "skin": "C2A07E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420", "leather": "4A3626",
            "blanket": "6A5E48"},
    "POL": {"tunic": "7C7450", "trousers": "746C4A", "boots": "3A2A1E", "lower": "5E4A34", "straps": "4A3624", "helmet": "5E6044",
            "pack": "7A7254", "skin": "BDA08E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420", "leather": "3A2E25",
            "blanket": "5C5040"},
    "CHN": {"tunic": "7F8A96", "trousers": "7A8590", "boots": "2E2C2A", "lower": "7A8490", "straps": "6E6450", "helmet": "727E8A",
            "pack": "8A8070", "skin": "C2A07E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420", "leather": "3A2E25",
            "blanket": "6A6A5E"},
}


def main():
    src, out = sys.argv[1], sys.argv[2]
    opts = dict(a.split("=", 1) for a in sys.argv[3:] if "=" in a)
    want = opts.get("clips", "idle,kneel_down,go_prone,crawl").split(",")
    at = float(opts.get("at", 0.8))
    w, h = int(opts.get("w", 640)), int(opts.get("h", 800))
    frames = []
    kit.fresh(60)
    bpy.ops.import_scene.gltf(filepath=src)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    nation = opts.get("nation") or next((k for k in LOOKS if k in os.path.basename(src)), None)
    if nation:
        for m in bpy.data.materials:
            key = m.name.split(".")[0]
            if key in LOOKS[nation] and m.use_nodes:
                b = m.node_tree.nodes.get("Principled BSDF")
                if b:
                    b.inputs["Base Color"].default_value = (*kit.hex_rgb(LOOKS[nation][key]), 1)
                    b.inputs["Roughness"].default_value = 0.85
    acts = {a.name.split("|")[-1]: a for a in bpy.data.actions}
    print("clips:", sorted(acts)[:60], flush=True)
    poses = [None] + [c for c in want if c in acts]
    seen = []
    for p in poses:
        if p in seen:
            continue
        seen.append(p)
    kit.studio(sun_elev=40, sun_turn=-40, strength=3.4, sky=0.6)
    kit.field_ground(60, "earth")
    cam = kit.camera("cam", lens=60)
    bpy.context.scene.camera = cam
    kit.render_setup(w, h, 24)
    sc = bpy.context.scene
    for p in seen:
        arm.animation_data_create()
        if p is None:
            arm.animation_data.action = None
            for b in arm.pose.bones:
                b.matrix_basis.identity()
            sc.frame_set(0)
        else:
            act = acts[p]
            arm.animation_data.action = act
            try:
                slot = act.slots[0]
                arm.animation_data.action_slot = slot
            except Exception:  # noqa: BLE001
                pass
            f0, f1 = act.frame_range
            sc.frame_set(int(f0 + (f1 - f0) * at))
        low = p and any(k in p for k in ("prone", "crawl"))
        kit.look_at(cam, (2.6, -2.2, 0.9 if low else 1.3), (0, 0, 0.35 if low else 0.9))
        path = os.path.join(os.path.dirname(os.path.abspath(out)), f"_pose_{p or 'rest'}.png")
        kit.still(path)
        frames.append(path)
    from PIL import Image
    ims = [Image.open(f) for f in frames]
    sheet = Image.new("RGB", (w * len(ims), h), (255, 255, 255))
    for i, im in enumerate(ims):
        sheet.paste(im.convert("RGB"), (i * w, 0))
    sheet.save(out)
    for f in frames:
        os.remove(f)
    print("wrote", out, [p or "rest" for p in seen])


if __name__ == "__main__":
    main()
