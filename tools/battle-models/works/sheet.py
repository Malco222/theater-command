"""The line-up sheet of the works for Brian.

    python sheet.py shots <out folder> [name,name,...|all] [WxHxSPP] [force]     one picture of each on the stage
    python sheet.py sheet <out folder>                                          lay them out on sheets

Pictures go to <out>/shots/<Name>.png, the sheets to <out>/lineup-<n>-<group>.jpg.
"""
import json
import math
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
    ("Posts and pits", [("MGNest", "Machine-gun post"), ("FlakPit", "Heavy flak pit"), ("SandbagRing", "Sandbag ring, light flak"), ("GunPit", "Gun pit"), ("AntiTankPit", "Anti-tank gun pit"), ("MortarPit", "Mortar pit"),
                        ("SandbagWall", "Sandbag wall"), ("ObservationPost", "Observation post"), ("CamoNet", "Camouflage net")]),
    ("Concrete", [("Pillbox", "Pillbox"), ("Bunker", "Bunker, gun casemate"), ("CoastalBattery", "Coastal battery"), ("FlakTower", "Flak tower"), ("DragonsTeeth", "Dragon's teeth")]),
    ("Wire and obstacles", [("Wire", "Wire belt"), ("WireFence", "Wire fence"), ("Hedgehogs", "Hedgehogs"), ("Minefield", "Minefield, marked"), ("RoadBar", "Road bar")]),
    ("Fieldworks and dug works", [("FieldworksLine", "Fieldworks, a line of five pieces"), ("Fieldworks", "Fieldworks, fire bay"), ("FieldworksCorner", "Fieldworks, corner"),
                                  ("FieldworksDugout", "Fieldworks, dugout"), ("AntiTankDitch", "Anti-tank ditch"), ("Foxhole", "Foxhole")]),
]
# a length of a line is shown with the line's two end pieces on it, so that nothing in a picture stops in a cut:
# (the piece that closes its start, the piece that closes its far end)
CAPS = {"Fieldworks": ("FieldworksEnd_start", "FieldworksEnd"), "FieldworksCorner": ("FieldworksEnd_start", "FieldworksEnd"),
        "FieldworksDugout": ("FieldworksEnd_start", "FieldworksEnd"), "AntiTankDitch": ("AntiTankDitchEnd_start", "AntiTankDitchEnd")}
# pictures of several pieces joined as the game joins them: the pieces in order along the line, from its start
LINES = {"FieldworksLine": ["FieldworksEnd_start", "Fieldworks", "FieldworksCorner", "FieldworksDugout", "FieldworksEnd"]}
# their views: (bearing, metres off, how high it looks down, lens, how far along the line from its middle the camera aims)
LINE_VIEW = {"FieldworksLine": (-152.0, 36.5, 25.0, 35, -3.0)}
# (bearing, how far in spans, how high it looks down, lens) where the usual three-quarter view does not suit
VIEW = {"LightFlak": (-28.0, 2.7, 14.0, 50), "HeavyFlak": (-28.0, 2.25, 15.0, 50), "HeavyFlakTwin": (-28.0, 2.3, 15.0, 50), "FlakTower": (-32.0, 1.5, 10.0, 50), "CoastalBattery": (-30.0, 1.5, 14.0, 50), "Bunker": (-30.0, 1.5, 15.0, 50), "CamoNet": (-28.0, 1.5, 22.0, 50),
        "Fieldworks": (-140.0, 1.8, 30.0, 50), "FieldworksCorner": (-140.0, 1.8, 30.0, 50), "FieldworksDugout": (-140.0, 1.8, 30.0, 50), "Foxhole": (-150.0, 1.4, 32.0, 50),
        "AntiTankDitch": (-40.0, 1.75, 28.0, 50), "MortarPit": (-150.0, 1.95, 30.0, 50), "GunPit": (-150.0, 1.5, 28.0, 50), "AntiTankPit": (-150.0, 1.5, 28.0, 50)}
# a pose that shows the piece better than rest: {part: degrees}
# (the guns are trained across the picture, never at the camera: a barrel seen end on is a thin rod standing up)
LAID = {"HeavyFlak": {"mount": -54.0, "cradle": 40.0}, "HeavyFlakTwin": {"mount": -54.0, "cradle": 42.0}, "LightFlak": {"mount": -52.0, "cradle": 36.0}, "Searchlight": {"mount": 30.0, "drum": 40.0}}
# a work whose guns all follow one target: the bearing on the ground they are trained to (degrees from the work's front,
# anticlockwise seen from above) and how high they are laid
TRAIN = {"FlakTower": (58.0, 45.0)}
# another thread's model stood at a socket for the picture (the game puts it there itself): socket, the file under the
# land models' folder
LAND = os.environ.get("LAND_MODELS", "/mnt/project-files/battle-models/land/")


def _guest(*files):
    return next((g for g in files if os.path.exists(os.path.join(LAND, g))), files[0])


# The land vehicles' towed guns, stood in a work for the eye. Each work shows the gun of the army whose paint it wears
# (WEARS): the British pit a 25-pounder; the 75 is the gun of France and of armies with none of their own, and stood
# in German casemates as a captured piece.
GUESTS = {"GunPit": ("gun", _guest("vehicles/25pdr.glb", "vehicles/Gun75.glb", "gun/Gun75.glb")),
          "AntiTankPit": ("gun", _guest("vehicles/Pak40.glb", "vehicles/AntiTankGun.glb", "vehicles/Gun75.glb")),
          "Bunker": ("gun", _guest("vehicles/Gun75.glb", "gun/Gun75.glb"))}
# lamps lit inside a work for its picture: (place in the work's own axes, watts), so what stands in the dark shows
LAMPS = {"Bunker": [((1.35, 0.9, 1.1), 14.0), ((1.35, -0.9, 1.1), 14.0), ((-0.4, 0.0, 1.3), 22.0)]}
# where the sun stands for a work's picture (turn, height), where the usual sun leaves its face dark
SUN = {}


# whose paint a piece wears in its picture (in the game every piece takes its owner's: see nations.json)
WEARS = {"HeavyFlak": "GER", "HeavyFlakTwin": "GER:1943-06", "LightFlak": "UK", "FlakTower": "GER", "FlakPit": "USA", "SandbagRing": "SOV",
         "CoastalBattery": "GER:1943-06", "Bunker": "GER", "GunPit": "UK", "AntiTankPit": "GER:1943-06", "Searchlight": "UK", "MortarPit": "USA", "Mortar": "USA"}
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


def line(name, out, w, h, spp):
    """Several pieces joined end to end as the game joins them (each one's start on the last one's far end)."""
    kit.fresh()
    s = Set()
    x, y, tris, first = 0.0, -10.0, 0, None
    for piece in LINES[name]:
        it = s.put(piece, (x, y, 0.0))
        first = first or (x, y)
        tris += it.m.triangles()
        x += it.m.info.get("file", {}).get("joins", {}).get("far_end_dx", 0.0)
        y += 10.0
    mid = (0.5 * (first[0] + x), 0.5 * (first[1] + y - 10.0))
    reach = 0.5 * (y - 10.0 - first[1]) + 6.0
    s.lay(trees=True, flat=reach * 2.2, fine=reach + 4.0, centre=mid, grass_area=reach * 2.0 + 14.0, grass_count=320000, sun_turn=150.0, sun_elev=40.0)
    look.render_setup(w, h, spp)
    cam = look.cam()
    yaw, far, pitch, lens, along = LINE_VIEW[name]
    cam.data.lens = lens
    kit.orbit(cam, Vector((mid[0], mid[1] + along, -0.3)), far, pitch, yaw)
    kit.still(os.path.join(out, "shots", name + ".png"))
    return {"triangles": tris, "parts": 0, "clips": [], "pieces": len(LINES[name])}


def shot(name, out, w, h, spp):
    if name in LINES:
        return line(name, out, w, h, spp)
    kit.fresh()
    s = Set()
    it = s.put(name, **({"nation": WEARS[name]} if name in WEARS else {}))
    if name in CAPS:
        s.put(CAPS[name][0], (0.0, -10.0, 0.0))
        s.put(CAPS[name][1], (it.m.info.get("file", {}).get("joins", {}).get("far_end_dx", 0.0), 10.0, 0.0))
    for target in [it] + list(it.mounted.values()):
        laid = LAID.get(target.name)
        if laid and name in TRAIN and target is not it:
            # (every gun of the work on the one target; the mount's own sense of turning is read from its axis)
            bearing, high = TRAIN[name]
            sense = 1.0 if target.m.parts[target.m.part_at["mount"]].axis[2] > 0 else -1.0
            laid = {"mount": sense * (bearing - target.yaw), "cradle": high if target.name != "LightFlak" else high - 8.0}
        if laid:
            target.set(target.clip("idle", 0.0), *[target.aim(p, d) for p, d in laid.items()])
    if name in GUESTS and os.path.exists(os.path.join(LAND, GUESTS[name][1])):
        p, d = it.where(GUESTS[name][0])
        s.guest(os.path.join(LAND, GUESTS[name][1]), tuple(p), math.degrees(math.atan2(d.y, d.x)), bits.paint_of(WEARS.get(name, "_default"))[0])
    for at, watts in LAMPS.get(name, []):
        lamp = bpy.data.lights.new("lamp", "POINT")
        lamp.energy, lamp.color, lamp.shadow_soft_size = watts, (1.0, 0.9, 0.76), 0.12
        ob = bpy.data.objects.new("lamp", lamp)
        ob.location = at
        bpy.context.scene.collection.objects.link(ob)
    span = it.m.info.get("span", 4.0)
    area = max(38.0, span * 1.6)
    turn, elev = SUN.get(name, (150.0, 40.0))
    s.lay(trees=True, flat=max(34.0, span * 2.2), fine=max(26.0, span * 1.2), grass_area=area, grass_count=int(min(320000, 120000 * (area / 38.0) ** 2)), sun_turn=turn, sun_elev=elev)
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
        if name not in works.BUILDERS and name not in LINES:
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


def page(images, out, cols, names, notes, title):
    """Our own sheet: pictures in a grid on paper, the name beneath each in letters sized to the picture (the shared sheet's are fixed, and vanish on pictures this wide)."""
    from PIL import Image, ImageDraw
    ims = [Image.open(p).convert("RGB") for p in images]
    w, h = max(i.width for i in ims), max(i.height for i in ims)
    rows = (len(ims) + cols - 1) // cols
    pad = w // 60
    name_f, note_f, title_f = kit._font(w // 26), kit._font(w // 44), kit._font(w // 20)
    label_h = int(w / 26 * 1.3 + w / 44 * 1.6)
    top = int(w / 20 * 1.9)
    W = cols * w + (cols + 1) * pad
    H = top + rows * (h + label_h + pad) + pad
    S = Image.new("RGB", (W, H), (232, 226, 214))
    d = ImageDraw.Draw(S)
    d.text((pad, int(w / 20 * 0.4)), title, fill=(40, 34, 28), font=title_f)
    for k, im in enumerate(ims):
        r, c = divmod(k, cols)
        last = len(ims) - r * cols if r == rows - 1 else cols
        x = pad + c * (w + pad) + (cols - last) * (w + pad) // 2
        y = top + r * (h + label_h + pad)
        S.paste(im, (x, y))
        d.text((x + 6, y + h + int(w / 26 * 0.12)), names[k], fill=(40, 34, 28), font=name_f)
        f, size = note_f, w // 44
        while d.textlength(notes[k], font=f) > w - 12 and size > 10:
            # (a long note is set smaller, never over its neighbour's)
            size -= max(1, size // 20)
            f = kit._font(size)
        d.text((x + 6, y + h + int(w / 26 * 1.3)), notes[k], fill=(104, 94, 80), font=f)
    S.save(out, quality=90)
    S.resize((W // 2, H // 2), Image.LANCZOS).save(out[:-4] + "-half.jpg", quality=88)
    return out


def sheets(out, index=None):
    meta = json.load(open(os.path.join(out, "shots", "meta.json")))
    if index and os.path.exists(index):
        # (the counts as exported: a shot's own count goes stale when a piece is mended after its picture)
        for n, row in json.load(open(index)).items():
            if n in meta:
                meta[n]["triangles"] = row["triangles"]
    army = {"GER": "German paint, to 1943", "GER:1943-06": "German paint, from 1943", "UK": "British paint", "USA": "American paint", "SOV": "Soviet paint"}
    made = []
    for k, (title, items) in enumerate(GROUPS):
        have = [(n, label) for n, label in items if os.path.exists(os.path.join(out, "shots", n + ".png"))]
        if not have:
            continue
        notes = []
        for n, _ in have:
            bit = [f"{meta[n]['triangles']:,} triangles"] if n in meta else []
            if n in LINES:
                bit.append("an end, a fire bay, a corner, a dugout and an end, joined as the game joins them")
                bit[0] += " in all"
            if n in CAPS:
                bit.append("shown with the line's two end pieces")
            if n in meta and meta[n]["parts"] > 1:
                bit.append(f"{meta[n]['parts']} moving parts")
            if n in WEARS:
                bit.append("shown in " + army[WEARS[n]])
            notes.append("   \u00b7   ".join(bit))
        path = os.path.join(out, f"lineup-{k + 1}-{title.lower().replace(',', '').replace(' ', '-')}.jpg")
        page([os.path.join(out, "shots", n + ".png") for n, _ in have], path, 2 if len(have) <= 6 else 3, [label for _, label in have], notes, "Fortifications and defences: " + title.lower())
        made.append(path)
        print(path, flush=True)
    return made


def main():
    what, out = sys.argv[1], sys.argv[2]
    if what == "sheet":
        sheets(out, sys.argv[3] if len(sys.argv) > 3 else None)
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
