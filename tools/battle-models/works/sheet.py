"""The line-up sheet of the works for Brian.

    python sheet.py shots <out folder> [name,name,...|all] [WxHxSPP] [force]     one picture of each on the stage
    python sheet.py sheet <out folder>                                          lay them out on sheets

Pictures go to <out>/shots/<Name>.png, the sheets to <out>/lineup-<n>-<group>.jpg.
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
from mathutils import Vector  # noqa: E402

import kit  # noqa: E402
import look  # noqa: E402
import works  # noqa: E402
from scene import Set  # noqa: E402

import bits  # noqa: E402  (after the others: it puts the folder above first on the path)

# what each piece is called on the sheet, in the order of the sheets
GROUPS = [
    ("Guns and lights", [("HeavyFlak", "Heavy flak gun"), ("HeavyFlakTwin", "Twin heavy flak gun"), ("LightFlak", "Light flak gun"), ("Searchlight", "Searchlight"),
                         ("MachineGun", "Machine gun, air-cooled"), ("MachineGun_water", "Machine gun, water-cooled"), ("Mortar", "Mortar")]),
    ("Posts and pits", [("MGNest", "Machine-gun post"), ("FlakPit", "Heavy flak pit"), ("SandbagRing", "Sandbag ring, light flak"), ("GunPit", "Gun pit"), ("MortarPit", "Mortar pit"),
                        ("SandbagWall", "Sandbag wall"), ("ObservationPost", "Observation post"), ("CamoNet", "Camouflage net")]),
    ("Concrete", [("Pillbox", "Pillbox"), ("Bunker", "Bunker, gun casemate"), ("CoastalBattery", "Coastal battery"), ("FlakTower", "Flak tower"), ("DragonsTeeth", "Dragon's teeth")]),
    ("Wire, obstacles and dug works", [("Wire", "Wire belt"), ("WireFence", "Wire fence"), ("Hedgehogs", "Hedgehogs"), ("Minefield", "Minefield, marked"), ("RoadBar", "Road bar"),
                                       ("Foxhole", "Foxhole"), ("Fieldworks", "Fieldworks"), ("FieldworksCorner", "Fieldworks, corner"), ("FieldworksDugout", "Fieldworks, dugout"),
                                       ("AntiTankDitch", "Anti-tank ditch")]),
]
# (bearing, how far in spans, how high it looks down, lens) where the usual three-quarter view does not suit
VIEW = {"LightFlak": (-28.0, 2.7, 14.0, 50), "HeavyFlak": (-28.0, 1.7, 15.0, 50), "HeavyFlakTwin": (-28.0, 1.75, 15.0, 50), "FlakTower": (-32.0, 1.8, 10.0, 50), "CoastalBattery": (-30.0, 1.5, 14.0, 50), "Bunker": (-30.0, 1.5, 15.0, 50), "CamoNet": (-28.0, 1.5, 22.0, 50),
        "Fieldworks": (-140.0, 1.5, 30.0, 50), "FieldworksCorner": (-140.0, 1.5, 30.0, 50), "FieldworksDugout": (-140.0, 1.5, 30.0, 50), "Foxhole": (-150.0, 1.4, 32.0, 50),
        "AntiTankDitch": (-40.0, 1.5, 28.0, 50), "MortarPit": (-150.0, 1.5, 30.0, 50), "GunPit": (-150.0, 1.5, 28.0, 50)}
# a pose that shows the piece better than rest: {part: degrees}
LAID = {"HeavyFlak": {"mount": 28.0, "cradle": 42.0}, "HeavyFlakTwin": {"mount": 24.0, "cradle": 48.0}, "LightFlak": {"mount": 25.0, "cradle": 38.0}, "Searchlight": {"mount": 30.0, "drum": 40.0}}


# whose paint a piece wears in its picture (in the game every piece takes its owner's: see nations.json)
WEARS = {"HeavyFlak": "GER", "HeavyFlakTwin": "GER:1943-06", "LightFlak": "UK", "FlakTower": "GER", "FlakPit": "USA", "SandbagRing": "SOV",
         "CoastalBattery": "GER:1943-06", "Bunker": "GER", "GunPit": "UK", "Searchlight": "UK", "MortarPit": "USA", "Mortar": "USA"}
# the armies shown side by side in the paint picture
ARMIES = [("GER", "Germany, to 1943"), ("GER:1943-06", "Germany, from 1943"), ("UK", "Britain"), ("USA", "United States"),
          ("SOV", "Soviet Union"), ("JAP", "Japan"), ("ITA", "Italy"), ("FRA", "France")]


def paints(out, w, h, spp):
    """One gun in eight armies' paint, each named beneath it: the one mesh serves them all."""
    from bpy_extras.object_utils import world_to_camera_view
    from PIL import Image, ImageDraw
    kit.fresh()
    s = Set()
    spots = []
    for k, (tag, label) in enumerate(ARMIES):
        r, c = divmod(k, 4)
        # (the camera stands out in front: the first army on the left, the heavy guns behind the light ones)
        at = (-5.5 + r * 10.0, (c - 1.5) * 7.6 + r * 3.8, 0.0)
        it = s.put("LightFlak" if r else "HeavyFlak", at, 0.0, nation=tag)
        laid = LAID[it.name]
        it.set(it.clip("idle", 0.0), *[it.aim(p, d + (k % 3 - 1) * 9.0) for p, d in laid.items()])
        spots.append((Vector((at[0] + (2.2 if r else 3.9), at[1], 0.0)), label, bits.paint_of(tag)[0]))
    s.lay(trees=True, flat=60.0, fine=36.0, grass_area=44.0, grass_count=150000, sun_turn=150.0, sun_elev=40.0)
    look.render_setup(w, h, spp)
    cam = look.cam()
    cam.data.lens = 50
    kit.orbit(cam, Vector((-0.5, 1.9, 0.9)), 47.0, 24.0, 0.0)
    path = os.path.join(out, "paints.png")
    kit.still(path)
    bpy.context.view_layer.update()
    im = Image.open(path).convert("RGB")
    d = ImageDraw.Draw(im, "RGBA")
    font = kit._font(max(15, int(w * 0.0165)))
    for p, label, hexa in spots:
        v = world_to_camera_view(bpy.context.scene, cam, p)
        x, y = v.x * w, (1.0 - v.y) * h
        tw = d.textlength(label, font=font)
        sw = int(w * 0.0165)
        box = (x - tw / 2 - sw - 14, y - 4, x + tw / 2 + 10, y + sw + 8)
        d.rectangle(box, fill=(232, 226, 214, 225))
        d.rectangle((box[0] + 5, y + 2, box[0] + 5 + sw, y + 2 + sw), fill=tuple(int(hexa[i:i + 2], 16) for i in (0, 2, 4)) + (255,), outline=(40, 34, 28, 255))
        d.text((box[0] + sw + 12, y), label, fill=(40, 34, 28, 255), font=font)
    jpg = path[:-4] + ".jpg"
    im.save(jpg, quality=92)
    print(jpg, flush=True)
    return jpg


def shot(name, out, w, h, spp):
    kit.fresh()
    s = Set()
    it = s.put(name, **({"nation": WEARS[name]} if name in WEARS else {}))
    for target in [it] + list(it.mounted.values()):
        laid = LAID.get(target.name)
        if laid:
            target.set(target.clip("idle", 0.0), *[target.aim(p, d) for p, d in laid.items()])
    span = it.m.info.get("span", 4.0)
    area = max(38.0, span * 1.6)
    s.lay(trees=True, flat=max(34.0, span * 2.2), fine=max(26.0, span * 1.2), grass_area=area, grass_count=int(min(320000, 120000 * (area / 38.0) ** 2)), sun_turn=150.0, sun_elev=40.0)
    look.render_setup(w, h, spp)
    cam = look.cam()
    tgt = Vector(it.m.info.get("aim", (0.0, 0.0, it.m.info.get("high", span * 0.16))))
    yaw, far, pitch, lens = VIEW.get(name, (-28.0, 1.55, 17.0, 50))
    cam.data.lens = lens
    kit.orbit(cam, tgt, max(span * far * 1.25, 2.2), pitch, yaw)
    kit.still(os.path.join(out, "shots", name + ".png"))
    tris = it.m.triangles()
    return {"triangles": tris, "parts": len(it.m.parts), "clips": list(it.m.clips)}


def shots(out, names, w, h, spp, force):
    meta_path = os.path.join(out, "shots", "meta.json")
    os.makedirs(os.path.dirname(meta_path), exist_ok=True)
    meta = json.load(open(meta_path)) if os.path.exists(meta_path) else {}
    for name in names:
        if name not in works.BUILDERS:
            print("no such work yet:", name, flush=True)
            continue
        if os.path.exists(os.path.join(out, "shots", name + ".png")) and not force:
            continue
        t = time.time()
        try:
            meta[name] = shot(name, out, w, h, spp)
            print(f"{name}: {time.time() - t:.0f}s", flush=True)
        except Exception:  # noqa: BLE001
            traceback.print_exc()
            print("FAILED", name, flush=True)
        with open(meta_path, "w") as f:
            json.dump(meta, f, indent=1)


def sheets(out):
    meta = json.load(open(os.path.join(out, "shots", "meta.json")))
    made = []
    for k, (title, items) in enumerate(GROUPS):
        have = [(n, label) for n, label in items if os.path.exists(os.path.join(out, "shots", n + ".png"))]
        if not have:
            continue
        labels = [f"{label}   ({meta[n]['triangles']:,} triangles" + (f", {meta[n]['parts']} moving parts" if meta[n]["parts"] > 1 else "") + ")" if n in meta else label for n, label in have]
        path = os.path.join(out, f"lineup-{k + 1}-{title.lower().replace(',', '').replace(' ', '-')}.jpg")
        png = path[:-4] + ".png"
        kit.sheet([os.path.join(out, "shots", n + ".png") for n, _ in have], png, cols=2 if len(have) <= 6 else 3, labels=labels, title="Fortifications and defences: " + title.lower())
        from PIL import Image
        Image.open(png).convert("RGB").save(path, quality=90)
        os.remove(png)
        made.append(path)
        print(path, flush=True)
    return made


def main():
    what, out = sys.argv[1], sys.argv[2]
    if what == "sheet":
        sheets(out)
        return
    which = sys.argv[3] if len(sys.argv) > 3 else "all"
    size = next((a for a in sys.argv[3:] if "x" in a and a[0].isdigit()), "960x540x32")
    w, h, spp = (int(v) for v in size.split("x"))
    if what == "paints":
        paints(out, w, h, spp)
        return
    names = [n for _, items in GROUPS for n, _ in items] if which == "all" else which.split(",")
    shots(out, names, w, h, spp, "force" in sys.argv)


main()
