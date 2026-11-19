"""The Eastern Front's ground sets (battle-models/asia/SET-FORMAT.md's shape), their ground paints and review pictures.

    python sets_east.py <region folder>                 sets/*.json and surfaces/*.png
    python sets_east.py <region folder> --sheets [look,...] [--fast]     sheets/<look>.png: a village street in each look

A set says what each of the field's ground kinds LOOKS like in one part of the world; the rules' ground still decides
who may pass. Picked by the province's world-map ground look (the art director's table, world-look-brief.md 3.3):
temperate east of the old frontier (central Russia, Belorussia), prairie (the Ukraine), steppe (the lower Volga, the
Kazakh steppe), taiga (the northern forest). Our additions to the shared shape, proposed to the other regions:

- `where_boxes`: west, east, south, north in degrees, for a look the region shares with another (temperate is
  Europe's look too: inside these boxes the eastern set is drawn).
- `seasons`: the same ground in winter and in the thaw, by month: surfaces repainted, models swapped (a birch in leaf
  for a bare one, a spruce for a snow-laden one), a different haze. Looks only.
- a kit's `ruin`: what a house is drawn as once the town has burnt.
"""

import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
LAND = os.path.normpath(os.path.join(HERE, "..", "land"))
sys.path.insert(0, HERE)
sys.path.insert(0, LAND)

# ---- the ground paints: colour as seen, with how rough (the height's strength) and how blotchy
SURFACES = {
    "steppe_grass": ("B0A474", 0.10, 0.16, 6.0),
    "feather_grass": ("C2B88E", 0.08, 0.12, 5.0),
    "wheat": ("CCB46A", 0.07, 0.10, 4.0),
    "stubble": ("B39C6C", 0.10, 0.12, 4.0),
    "chernozem": ("3C3229", 0.18, 0.14, 4.0),
    "meadow_east": ("7F8659", 0.10, 0.18, 6.0),
    "forest_floor": ("4B4A35", 0.14, 0.2, 5.0),
    "bog": ("8C865E", 0.08, 0.22, 8.0),
    "mud_road": ("6A5D4E", 0.2, 0.15, 5.0),
    "snow": ("E4E8EE", 0.05, 0.04, 8.0),
    "slush": ("8A8580", 0.16, 0.18, 5.0),
    "thaw_mud": ("5B4B3B", 0.2, 0.2, 5.0),
}


def periodic_noise(n, beta, seed):
    """Fractal noise that tiles: white noise shaped by 1/f^beta in the frequency domain."""
    rng = np.random.default_rng(seed)
    w = rng.standard_normal((n, n))
    fx = np.fft.fftfreq(n)[:, None]
    fy = np.fft.fftfreq(n)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0
    out = np.real(np.fft.ifft2(np.fft.fft2(w) / f ** beta))
    out -= out.mean()
    return out / (np.abs(out).max() + 1e-9)


def write_surfaces(folder, size=256):
    from PIL import Image
    os.makedirs(os.path.join(folder, "surfaces") + "/", exist_ok=True)
    out = {}
    for k, (name, (hexc, rough, blotch, tile)) in enumerate(SURFACES.items()):
        base = np.array([int(hexc[i:i + 2], 16) for i in (0, 2, 4)], float)
        fine = periodic_noise(size, 0.6, 100 + k)
        broad = periodic_noise(size, 1.6, 200 + k)
        hue = periodic_noise(size, 1.2, 300 + k)
        light = 1.0 + rough * fine + blotch * broad
        rgb = base[None, None, :] * light[:, :, None]
        rgb[:, :, 0] *= 1.0 + 0.04 * hue
        rgb[:, :, 2] *= 1.0 - 0.04 * hue
        height = (0.6 * fine + 0.4 * broad + 1.0) / 2.0
        img = np.dstack([np.clip(rgb, 0, 255), np.clip(height * 255, 0, 255)]).astype(np.uint8)
        p = os.path.join(folder, "surfaces", name + ".png")
        Image.fromarray(img, "RGBA").save(p, optimize=True)
        out[name] = {"colour": hexc, "image": f"surfaces/{name}.png", "tile_m": tile}
    return out


# ---- the village kit, shared by the four sets
KIT = {
    "eastern_village": {"houses": ["Izba"], "yard": ["WattleFence", "Haystack", "WellSweep"], "ruin": ["BurntChimney"],
                        "by_road": ["TelegraphPole"], "landmark": {"spire": None, "dome": None, "station": None},
                        "about": "Log houses strung along the street, gable ends to it, behind wattle fences; a well sweep and a "
                                 "haystack in the yards; poles along the road. Landmarks: Europe's kit's until our own are made."},
    "ukrainian_village": {"houses": ["Khata"], "yard": ["WattleFence", "Haystack", "WellSweep"], "ruin": ["BurntChimney"],
                          "by_road": ["TelegraphPole"], "landmark": {"spire": None, "dome": None, "station": None},
                          "about": "Whitewashed cottages under thatch, each in its own yard among birches and orchards; wattle "
                                   "fences, well sweeps, haystacks."},
}
SEASON_SWAP = {"Birch": "BirchBare", "Spruce": "SpruceSnow"}


def winter(months, haze="C9CDD1", sight=1800):
    return {"months": months,
            "surfaces": {"*": "snow", "road": "slush", "water": None},
            "swap": SEASON_SWAP,
            "haze": {"colour": haze, "sight_m": sight},
            "about": "Everything under snow but the water and the churned roads; bare birches, snow on the spruce.",
            "sheet": "sheets/winter.png"}


def thaw(months):
    return {"months": months,
            "surfaces": {"road": "thaw_mud", "open": "thaw_mud", "field": "chernozem"},
            "swap": {"Birch": "BirchBare"},
            "about": "The roads and open ground gone to mud, the fields black, the birches bare."}


def sets():
    clump = lambda model, per_ha, gap, scale=(0.8, 1.2), **kw: {"model": model, "per_ha": per_ha, "scale": list(scale), "gap_m": gap,
                                                              "slope_max": kw.get("slope_max", 30), "near_water": kw.get("near_water")}
    out = {}
    out["temperate"] = {
        "look": "temperate", "region": "east",
        "where": "central Russia and Belorussia, east of the old frontier",
        "where_boxes": [[23.5, 60.0, 50.0, 57.5]],
        "town_kit": "eastern_village",
        "haze": {"colour": "A9B0A4", "sight_m": 2600},
        "grounds": {
            "open": {"surface": "meadow_east", "scatter": [clump("Birch", 0.8, 25.0), clump("Haystack", 0.4, 30.0)]},
            "field": {"surface": "stubble", "pattern": {"kind": "strips", "plot_m": [30, 120], "strip_m": [12, 30],
                                                        "mix": {"stubble": 0.4, "wheat": 0.3, "chernozem": 0.3}}},
            "woods": {"surface": "forest_floor", "cover": "canopy",
                      "scatter": [clump("Spruce", 150, 5.0, (0.75, 1.15)), clump("Birch", 90, 4.0, (0.8, 1.2))]},
            "rough": {"surface": "meadow_east", "scatter": [clump("Birch", 25, 6.0)]},
            "marsh": {"surface": "bog", "scatter": [clump("Birch", 12, 8.0, (0.6, 0.9), near_water=40)]},
            "road": {"surface": "mud_road"},
            "buildings": {"kit": "eastern_village"},
        },
        "seasons": {"winter": winter([12, 1, 2, 3]), "thaw": thaw([4, 10, 11])},
    }
    out["prairie"] = {
        "look": "prairie", "region": "east",
        "where": "the Ukraine (28 to 42 E, 45 to 51 N)",
        "town_kit": "ukrainian_village",
        "haze": {"colour": "B9B39A", "sight_m": 3500},
        "grounds": {
            "open": {"surface": "steppe_grass", "scatter": [clump("Haystack", 0.6, 30.0), clump("Birch", 0.2, 60.0)]},
            "field": {"surface": "wheat", "pattern": {"kind": "fields", "plot_m": [120, 400],
                                                      "mix": {"wheat": 0.6, "stubble": 0.2, "chernozem": 0.2}}},
            "woods": {"surface": "meadow_east", "cover": "canopy", "scatter": [clump("Birch", 110, 4.5)],
                      "about": "groves in the gullies and round the villages"},
            "rough": {"surface": "steppe_grass", "scatter": [clump("Birch", 10, 8.0)], "about": "the gullies (balki)"},
            "road": {"surface": "mud_road"},
            "buildings": {"kit": "ukrainian_village"},
        },
        "seasons": {"winter": winter([12, 1, 2]), "thaw": thaw([3, 11])},
    }
    out["steppe"] = {
        "look": "steppe", "region": "east",
        "where": "the lower Volga and the Kazakh steppe",
        "town_kit": "ukrainian_village",
        "haze": {"colour": "C2B99E", "sight_m": 4200},
        "grounds": {
            "open": {"surface": "feather_grass", "scatter": [clump("Haystack", 0.15, 60.0)]},
            "field": {"surface": "stubble", "pattern": {"kind": "fields", "plot_m": [200, 600],
                                                        "mix": {"stubble": 0.5, "wheat": 0.3, "steppe_grass": 0.2}}},
            "woods": {"surface": "steppe_grass", "cover": "canopy", "scatter": [clump("Birch", 40, 6.0, (0.6, 0.9))],
                      "about": "few: thin groves along the gullies only"},
            "rough": {"surface": "steppe_grass", "about": "pale dry gullies with a thread of green"},
            "road": {"surface": "mud_road"},
            "buildings": {"kit": "ukrainian_village"},
        },
        "seasons": {"winter": winter([12, 1, 2], "CDCFD0", 2200), "thaw": thaw([3, 11])},
    }
    out["taiga"] = {
        "look": "taiga", "region": "east",
        "where": "the northern forest (Karelia, the north, Siberia)",
        "town_kit": "eastern_village",
        "haze": {"colour": "9EA9A2", "sight_m": 2200},
        "grounds": {
            "open": {"surface": "bog", "scatter": [clump("Birch", 4, 12.0, (0.6, 0.9)), clump("Spruce", 3, 12.0, (0.5, 0.9))]},
            "field": {"surface": "meadow_east", "pattern": {"kind": "strips", "plot_m": [20, 60], "strip_m": [8, 20],
                                                            "mix": {"meadow_east": 0.5, "stubble": 0.3, "chernozem": 0.2}}},
            "woods": {"surface": "forest_floor", "cover": "canopy",
                      "scatter": [clump("Spruce", 260, 4.0, (0.75, 1.2)), clump("Birch", 30, 5.0)]},
            "marsh": {"surface": "bog", "scatter": [clump("Spruce", 8, 10.0, (0.4, 0.7), near_water=60)]},
            "road": {"surface": "mud_road"},
            "buildings": {"kit": "eastern_village",
                          "about": "the art director's railway town (log, green and iron-red roofs) is for east of the Volga; "
                                   "until it is made the log village stands in"},
        },
        "seasons": {"winter": winter([11, 12, 1, 2, 3, 4], "C4CACE", 1600), "thaw": thaw([5, 10])},
    }
    return out


def write_sets(folder):
    surf = write_surfaces(folder)
    os.makedirs(os.path.join(folder, "sets") + "/", exist_ok=True)
    for look, s in sets().items():
        used = set()
        for g in s["grounds"].values():
            if "surface" in g:
                used.add(g["surface"])
            used.update(g.get("pattern", {}).get("mix", {}).keys())
        for season in s["seasons"].values():
            used.update(v for v in season["surfaces"].values() if v)
        s = {**s, "surfaces": {k: surf[k] for k in SURFACES if k in used}, "kits": {s["town_kit"]: KIT[s["town_kit"]]},
             "sheet": f"sheets/{look}.png"}
        order = ["look", "region", "where", "where_boxes", "town_kit", "haze", "surfaces", "grounds", "kits", "seasons", "sheet"]
        s = {k: s[k] for k in order if k in s}
        with open(os.path.join(folder, "sets", look + ".json"), "w") as f:
            json.dump(s, f, indent=1)
        print("set", look, "surfaces", sorted(s["surfaces"]))


# ================================================================================================ the review pictures
def village(look, path, fast=False):
    """A village street in the look, as the battle camera sees it: houses along a dirt road, fences, yards, trees."""
    import bpy
    import kit
    import random
    import ground as G
    import rigid
    import stage
    import scenery_east as E
    kit.fresh()
    season = "winter" if look == "winter" else "summer"
    kind = {"temperate": "meadow", "taiga": "meadow", "prairie": "steppe", "steppe": "steppe", "winter": "winter"}[look]
    if kind == "winter":
        import winter as W
        W.install(0.0, 0.0)
        G.Field(kind="winter", density=4.0, craters=False)
    else:
        G.Field(kind=kind, density=8.0 if fast else 14.0, trees=False)
    rng = random.Random(7)
    built = {}

    def put(name, x, y, yaw=0.0, scale=1.0):
        if season == "winter":
            name = SEASON_SWAP.get(name, name)
        if name not in built:
            m = E.BUILDERS[name](True)
            m.info.pop("about", None)
            ob, arm = rigid.to_blender(m, crease=m.info.pop("crease", 38.0))
            ob.parent = None
            for md in list(ob.modifiers):
                ob.modifiers.remove(md)
            bpy.data.objects.remove(arm)
            ob.location = (0.0, 0.0, -100.0)
            built[name] = ob
        o = built[name].copy()
        bpy.context.scene.collection.objects.link(o)
        o.location = (x, y, 0.0)
        o.rotation_euler = (0.0, 0.0, math.radians(yaw))
        o.scale = (scale, scale, scale)
    house = {"temperate": "Izba", "taiga": "Izba", "winter": "Izba", "prairie": "Khata", "steppe": "Khata"}[look]
    burnt = look == "winter"
    for side in (1.0, -1.0):
        for i in range(6):
            x = -40.0 + i * 17.0 + rng.uniform(-2, 2)
            y = side * (15.0 + rng.uniform(-1.5, 1.5))
            if burnt and (i + (side > 0)) % 3 == 0:
                put("BurntChimney", x, y, rng.uniform(0, 360))
            elif house == "Izba":
                put("Izba", x, y, -90.0 * side + rng.uniform(-4, 4))
            else:
                put("Khata", x, y, 180.0 + rng.uniform(-6, 6) if side > 0 else rng.uniform(-6, 6))
            for k in range(3):                                                         # the fence along the street
                put("WattleFence", x - 4.1 + k * 4.1, side * 8.5, rng.uniform(-3, 3))
            if i % 2 == 0:
                put("Haystack", x + rng.uniform(-3, 3), side * (27.0 + rng.uniform(0, 4)), rng.uniform(0, 360), rng.uniform(0.85, 1.1))
            if i == 2:
                put("WellSweep", x + 6.5, side * 11.5, -90.0 * side)
    for i in range(5):
        put("TelegraphPole", -50.0 + i * 30.0, -6.0, 90.0)
    trees = {"temperate": [("Birch", 0.5), ("Spruce", 0.5)], "taiga": [("Spruce", 0.85), ("Birch", 0.15)],
             "winter": [("Birch", 0.5), ("Spruce", 0.5)], "prairie": [("Birch", 1.0)], "steppe": [("Birch", 1.0)]}[look]
    n_trees = {"temperate": 70, "taiga": 160, "winter": 70, "prairie": 40, "steppe": 10}[look]
    for t in range(n_trees):
        side = 1.0 if t % 2 else -1.0
        x = rng.uniform(-70.0 if side > 0 else -25.0, 80.0)                     # (none in the camera's face)
        y = side * rng.uniform(24.0, 70.0 if look == "taiga" else 48.0)
        r, acc = rng.random(), 0.0
        for name, share in trees:
            acc += share
            if r <= acc:
                break
        put(name, x, y, rng.uniform(0, 360), rng.uniform(0.75, 1.15))
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    bpy.context.scene.collection.objects.link(cam)
    bpy.context.scene.camera = cam
    cam.data.lens = 30
    from mathutils import Vector
    eye, target = Vector((-62.0, -30.0, 19.0)), Vector((12.0, 2.0, 2.0))
    cam.location = eye
    cam.rotation_euler = (target - eye).to_track_quat("-Z", "Y").to_euler()
    w, h = (960, 540) if fast else (1600, 900)
    stage.render_setup(w, h, samples=12 if fast else 40)
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("wrote", path, flush=True)


def main():
    a = sys.argv[1:]
    folder = a[0]
    if "--sheets" in a:
        i = a.index("--sheets")
        looks = a[i + 1].split(",") if len(a) > i + 1 and not a[i + 1].startswith("--") else ["temperate", "prairie", "steppe", "taiga", "winter"]
        os.makedirs(os.path.join(folder, "sheets") + "/", exist_ok=True)
        for look in looks:
            village(look, os.path.join(folder, "sheets", look + ".png"), "--fast" in a)
        return
    write_sets(folder)


if __name__ == "__main__":
    main()
