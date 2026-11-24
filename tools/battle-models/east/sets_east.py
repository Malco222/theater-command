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
RUIN = {"share": "in proportion to the battle damage the game tracks for the town; where none is tracked, by how many "
                 "times the front has crossed it. A village fought over in 1943-44 is about half chimneys.",
        "ruling": "art director, 19 Nov 2026 04:05Z"}
KIT = {
    "eastern_village": {"houses": ["Izba", "IzbaThatch"], "mix": {"Izba": 0.7, "IzbaThatch": 0.3}, "yard": ["WattleFence", "Haystack", "WellSweep"],
                        "ruin": ["BurntChimney"], "ruin_share": RUIN, "by_road": ["TelegraphPole"],
                        "small_town_kit": "railway_town", "landmark": {"spire": None, "dome": None, "station": None},
                        "where": "Russia west of the Volga and Belorussia, with the eastern Polish lands taken in 1939 (the art "
                                 "director's ruling; the Baltic states, central and western Poland and East Prussia keep Europe's kit)",
                        "about": "Log houses gone silver-grey, strung along the street with their gable ends to it, behind wattle "
                                 "fences, a kitchen-garden plot behind each; a well sweep and haystacks in the yards; poles along "
                                 "the road. Small towns take the railway town kit (log walls, green and iron-red roofs); big cities "
                                 "their own city work. Landmarks: Europe's kit's until our own are made."},
    "ukrainian_village": {"houses": ["Khata"], "yard": ["WattleFence", "Haystack", "WellSweep"], "ruin": ["BurntChimney"],
                          "ruin_share": RUIN, "by_road": ["TelegraphPole"], "small_town_kit": "railway_town",
                          "landmark": {"spire": None, "dome": None, "station": None},
                          "where": "the Ukraine, Bessarabia, the Don and the Kuban, roughly south of 50 to 51 N (the art director's ruling)",
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
        "where_boxes": [[27.5, 60.0, 50.0, 57.5], [23.5, 27.5, 50.0, 54.0]],      # (not the Baltic states)
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
def _mesh(name, P, F, mat):
    import bpy
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(map(float, p)) for p in P], [], [tuple(int(i) for i in f) for f in F])
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def _plain(name, hexc, rough=0.9, noise_scale=0.0, dark=0.0, bump=0.0, bump_scale=3.0):
    """A material of one colour, optionally mottled darker by a noise, and (bump) its surface churned by another."""
    import bpy
    from kit import hex_rgb
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Roughness"].default_value = rough
    c = (*hex_rgb(hexc), 1.0)
    if noise_scale:
        n = nt.nodes.new("ShaderNodeTexNoise")
        n.inputs["Scale"].default_value = noise_scale
        n.inputs["Detail"].default_value = 6.0
        r = nt.nodes.new("ShaderNodeValToRGB")
        r.color_ramp.elements[0].color = (c[0] * (1 - dark), c[1] * (1 - dark), c[2] * (1 - dark), 1.0)
        r.color_ramp.elements[1].color = c
        nt.links.new(n.outputs["Fac"], r.inputs["Fac"])
        nt.links.new(r.outputs["Color"], b.inputs["Base Color"])
    else:
        b.inputs["Base Color"].default_value = c
    if bump:
        n2 = nt.nodes.new("ShaderNodeTexNoise")
        n2.inputs["Scale"].default_value = bump_scale
        n2.inputs["Detail"].default_value = 8.0
        n2.inputs["Roughness"].default_value = 0.65
        bp = nt.nodes.new("ShaderNodeBump")
        bp.inputs["Strength"].default_value = bump
        bp.inputs["Distance"].default_value = 0.08
        nt.links.new(n2.outputs["Fac"], bp.inputs["Height"])
        nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m


def mud_road(x0=-140.0, x1=170.0, half=3.7, seed=4, snow=False):
    """The east's road: a wide strip of churned mud (covering the field's own ruts), its edges ragged; wheel ruts
    wandering inside it, wet and dark; puddles in them that show the sky. In winter (snow) the strip is packed snow
    gone grey-brown under the traffic, the ruts dirty slush, the puddles frozen."""
    rng = np.random.default_rng(seed)
    xs = np.arange(x0, x1, 1.0)
    mid = 0.4 * np.sin(xs / 31.0) + 0.25 * np.sin(xs / 9.0)
    P, F = [], []
    for i, (x, c) in enumerate(zip(xs, mid)):
        P += [(x, c - half - rng.uniform(0, 0.6), 0.03), (x, c + half + rng.uniform(0, 0.6), 0.03)]
        if i:
            F.append((2 * i - 2, 2 * i - 1, 2 * i + 1, 2 * i))
    if snow:                                            # the snow the traffic shoved aside, banked low along both edges
        edges = P[:]
        for side in (0, 1):
            sg = -1.0 if side == 0 else 1.0
            B, BF = [], []
            for i, x in enumerate(xs):
                ey = edges[2 * i + side][1]
                h = 0.16 + 0.1 * math.sin(x * 0.37 + side) + rng.uniform(-0.04, 0.04)
                B += [(x, ey - sg * 0.35, 0.025), (x, ey + sg * 0.3, h), (x, ey + sg * (1.1 + rng.uniform(0, 0.5)), 0.0)]
                if i:
                    k = 3 * i
                    BF += [(k - 3, k - 2, k + 1, k), (k - 2, k - 1, k + 2, k + 1)] if side else [(k - 3, k, k + 1, k - 2), (k - 2, k + 1, k + 2, k - 1)]
            _mesh("berm", B, BF, _plain("berm", "D3D4D4", 0.9, 1.2, 0.2))
        _mesh("mud_road", P, F, _plain("packed", "8C8275", 0.8, 0.6, 0.4, bump=0.9, bump_scale=2.2))
        wet = _plain("rut", "5A5046", 0.3, 0.8, 0.3, bump=0.6, bump_scale=6.0)
        water = _plain("meltwater", "40444A", 0.06)
    else:
        _mesh("mud_road", P, F, _plain("mud", "6B5C49", 0.75, 0.35, 0.35, bump=0.7, bump_scale=2.2))
        wet = _plain("rut", "3F3428", 0.35, 0.8, 0.25, bump=0.5, bump_scale=6.0)
        water = _plain("puddle", "4A4C50", 0.04)
    for lane in ((-2.3, -1.25, 0.0, 1.25, 2.3) if snow else (-1.25, 1.25)):
        for wheel in (-0.75, 0.75):
            off = lane + wheel + 0.5 * np.sin(xs / (13.0 + 3 * lane) + wheel) + 0.2 * np.sin(xs / 4.3 + lane)
            P, F = [], []
            for i, (x, c, o) in enumerate(zip(xs, mid, off)):
                w = (0.3 if snow else 0.22) + 0.08 * math.sin(x * 0.7 + lane)
                P += [(x, c + o - w, 0.04), (x, c + o + w, 0.04)]
                if i:
                    F.append((2 * i - 2, 2 * i - 1, 2 * i + 1, 2 * i))
            _mesh("rut", P, F, wet)
    for k in range(int((x1 - x0) / 6)):
        x = rng.uniform(x0, x1)
        y = float(np.interp(x, xs, mid)) + rng.choice([-2.0, -0.5, 0.5, 2.0]) + rng.uniform(-0.4, 0.4)
        a, b = rng.uniform(0.6, 2.2), rng.uniform(0.35, 0.9)
        if snow:
            a, b = 0.6 * a, 0.6 * b
        n = 9
        P = [(x, y, 0.05)] + [(x + a * math.cos(2 * math.pi * j / n) * rng.uniform(0.75, 1.1), y + b * math.sin(2 * math.pi * j / n) * rng.uniform(0.75, 1.1), 0.05)
                              for j in range(n)]
        _mesh("puddle", P, [(0, 1 + j, 1 + (j + 1) % n) for j in range(n)], water)


def tall_grass(boxes, avoid, density=2.5, seed=6, colours=("6B7A3C", "A39C62")):
    """Tufts of tall rough grass, knee to thigh high, green going to straw at the tips; not on the road or in the
    houses' footprints (avoid: (x, y, radius))."""
    from kit import hex_rgb
    import bpy
    rng = np.random.default_rng(seed)
    pts = []
    for x0, x1, y0, y1 in boxes:
        n = int((x1 - x0) * (y1 - y0) * density)
        x, y = rng.uniform(x0, x1, n), rng.uniform(y0, y1, n)
        keep = np.abs(y) > 4.8
        for ax, ay, r in avoid:
            keep &= (x - ax) ** 2 + (y - ay) ** 2 > r * r
        pts.append(np.stack([x[keep], y[keep]], 1))
    pts = np.concatenate(pts)
    n = len(pts)
    P, F, C = [], [], []
    base, tip = np.array(hex_rgb(colours[0])), np.array(hex_rgb(colours[1]))
    k = 0
    for b in range(6):
        a = rng.uniform(0, 2 * np.pi, n)
        h = rng.uniform(0.45, 1.05, n)
        lean = rng.uniform(0.2, 0.5, n) * h
        w = rng.uniform(0.012, 0.024, n)
        bx, by = pts[:, 0] + rng.uniform(-0.12, 0.12, n), pts[:, 1] + rng.uniform(-0.12, 0.12, n)
        ca, sa = np.cos(a), np.sin(a)
        v0 = np.stack([bx - sa * w, by + ca * w, np.zeros(n)], 1)
        v1 = np.stack([bx + sa * w, by - ca * w, np.zeros(n)], 1)
        v2 = np.stack([bx + ca * lean, by + sa * lean, h], 1)
        P.append(np.stack([v0, v1, v2], 1).reshape(-1, 3))
        t = rng.uniform(0, 1, n)[:, None]
        cb = base[None, :] * (0.6 + 0.3 * t)
        ct = base[None, :] * (1 - t) + tip[None, :] * t
        C.append(np.stack([cb, cb, ct], 1).reshape(-1, 3))
        i = k + np.arange(n) * 3
        F.append(np.stack([i, i + 1, i + 2], 1))
        k += n * 3
    P, F, C = np.concatenate(P), np.concatenate(F), np.concatenate(C)
    me = bpy.data.meshes.new("tall_grass")
    me.vertices.add(len(P))
    me.vertices.foreach_set("co", P.astype(np.float32).ravel())
    me.loops.add(len(F) * 3)
    me.loops.foreach_set("vertex_index", F.astype(np.int32).ravel())
    me.polygons.add(len(F))
    me.polygons.foreach_set("loop_start", (np.arange(len(F)) * 3).astype(np.int32))
    me.update(calc_edges=True)
    col = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    col.data.foreach_set("color", np.concatenate([C, np.ones((len(C), 1))], 1).astype(np.float32).ravel())
    m = bpy.data.materials.new("tall_grass")
    m.use_nodes = True
    nt = m.node_tree
    att = nt.nodes.new("ShaderNodeVertexColor")
    att.layer_name = "Col"
    nt.links.new(att.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
    nt.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.8
    me.materials.append(m)
    ob = bpy.data.objects.new("tall_grass", me)
    bpy.context.scene.collection.objects.link(ob)
    print(f"  tall grass: {n} tufts", flush=True)


def snow_look(amount=0.9, steep=0.45):
    """Snow lying on what looks up on a model (roofs to about 60 degrees, fence tops, haystack crowns): in lumps,
    banked in the corners, never on glass. A rigid.to_blender look."""
    from works import _math, _mix, _noise, _ramp
    from winter import SNOW

    def look(paint, mat, nt, bsdf, mix):
        if paint.split(".")[0] in ("glass", "lamp", "fire", "ember", "melt", "ash", "char", "soot", "stove_burnt", "brick_burnt"):
            return
        last = bsdf.inputs["Base Color"].links[0].from_socket
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        ns = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(geo.outputs["Normal"], ns.inputs[0])
        pos = nt.nodes.new("ShaderNodeTexCoord").outputs["Object"]
        lumps = _noise(nt, pos, 2.2, 4.0, 0.6)
        lies = _ramp(nt, ns.outputs[2], steep, steep + 0.25, 0.0, 1.0)
        fac = _math(nt, "MULTIPLY", lies, _ramp(nt, lumps, 0.3, 0.5, 0.35, 1.0))
        fac = _math(nt, "MULTIPLY", fac, amount, clamp=True)
        _, f, a, b, o = _mix(nt)
        nt.links.new(fac, f)
        nt.links.new(last, a)
        b.default_value = (*SNOW, 1.0)
        nt.links.new(o, bsdf.inputs["Base Color"])
    return look


def garden(x, y, yaw, snow=False, seed=None):
    """A kitchen-garden plot behind a house: low, uneven mounds of dug earth in broken rows, the season's dead stalks
    standing in them; under snow a lumpy white with the stalks poking through."""
    rng = np.random.default_rng(seed if seed is not None else int(abs(x * 7.3 + y * 3.1)) % 9973)
    ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    at = lambda u, v, z: (x + u * ca - v * sa, y + u * sa + v * ca, z)
    P, F, S, SF = [], [], [], []
    rows, L, W = 8, 10.0, 7.0
    for r in range(rows):
        v0 = -W / 2 + W * (r + 0.5) / rows + rng.uniform(-0.12, 0.12)
        u = -L / 2 + rng.uniform(0.0, 0.8)
        while u < L / 2 - 0.6:                                              # the row in pieces, with gaps between
            long = rng.uniform(1.6, 4.0)
            u1 = min(L / 2 - rng.uniform(0.0, 0.5), u + long)
            n = max(2, int((u1 - u) / 0.4))
            k = len(P)
            for j in range(n + 1):
                uu = u + (u1 - u) * j / n
                end = min(j, n - j) / max(1, n / 4)                          # the mound's ends sink into the ground
                h = (0.07 + 0.06 * rng.random()) * min(1.0, end + 0.15)
                w = 0.26 + rng.uniform(-0.06, 0.08)
                v = v0 + 0.06 * math.sin(uu * 1.7 + r)
                P += [at(uu, v - w, 0.02), at(uu, v + rng.uniform(-0.05, 0.05), 0.02 + h), at(uu, v + w, 0.02)]
                if j:
                    q = k + 3 * (j - 1)
                    F += [(q, q + 3, q + 4, q + 1), (q + 1, q + 4, q + 5, q + 2)]
                if rng.random() < 0.45:                                       # a dead stalk
                    sx, sy, _ = at(uu + rng.uniform(-0.15, 0.15), v + rng.uniform(-0.12, 0.12), 0.0)
                    tall = rng.uniform(0.25, 0.6)
                    lean = rng.uniform(-0.12, 0.12, 2)
                    m = len(S)
                    S += [(sx - 0.012, sy, 0.02), (sx + 0.012, sy, 0.02), (sx + lean[0], sy + lean[1], 0.02 + h + tall)]
                    SF.append((m, m + 1, m + 2))
            u = u1 + rng.uniform(0.3, 1.2)
    if not snow:                                                            # the dug plot between the rows, its edge ragged
        k = 20
        rim = []
        for j in range(k):
            a = 2 * math.pi * j / k
            cu, cv = math.cos(a), math.sin(a)
            f = 1.0 / max(abs(cu) / (L / 2 + 0.4), abs(cv) / (W / 2 + 0.4))   # (round a rectangle)
            f *= rng.uniform(0.92, 1.04)
            rim.append(at(cu * f, cv * f, 0.015))
        _mesh("plot", [at(0.0, 0.0, 0.015)] + rim, [(0, 1 + j, 1 + (j + 1) % k) for j in range(k)],
              _plain("plot", "54463A", 0.95, 1.8, 0.25, bump=0.5, bump_scale=4.0))
    _mesh("garden", P, F, _plain("garden" + ("_snow" if snow else ""), "D9DDE2" if snow else "45372B", 0.95, 2.5, 0.35 if snow else 0.3))
    if S:
        _mesh("stalks", S, SF, _plain("stalk", "4F4232", 0.9))


def village(look, path, fast=False, extra=None, eye=None, target=None, lens=30.0, wash=0.0, spp=None, size=None):
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
        W.install(wash, wash)
        G.Field(kind="winter", density=4.0, craters=extra is not None)
    else:
        G.Field(kind=kind, density=6.0 if fast else 10.0, trees=False)
    rng = random.Random(7)
    built = {}

    def put(name, x, y, yaw=0.0, scale=1.0):
        if season == "winter":
            name = SEASON_SWAP.get(name, name)
        if name not in built:
            m = E.BUILDERS[name](True)
            m.info.pop("about", None)
            ob, arm = rigid.to_blender(m, look=snow_look() if season == "winter" and name not in ("SpruceSnow", "BirchBare") else None, crease=m.info.pop("crease", 38.0))
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
    avoid = []
    mud_road(snow=season == "winter")
    for side in (1.0, -1.0):
        for i in range(6):
            x = -40.0 + i * 17.0 + rng.uniform(-2, 2)
            y = side * (15.0 + rng.uniform(-1.5, 1.5))
            if burnt and (i + (side > 0)) % 3 == 0:
                put("BurntChimney", x, y, rng.uniform(0, 360))
            elif house == "Izba":
                put("IzbaThatch" if rng.random() < 0.3 else "Izba", x, y, -90.0 * side + rng.uniform(-4, 4))
            else:
                put("Khata", x, y, 180.0 + rng.uniform(-6, 6) if side > 0 else rng.uniform(-6, 6))
            garden(x + rng.uniform(-1, 1), side * 25.5, rng.uniform(-4, 4), snow=season == "winter", seed=i * 2 + (side > 0))                # the kitchen garden behind
            avoid.append((x, y, 5.5))
            for k in range(3):                                                         # the fence along the street
                put("WattleFence", x - 4.1 + k * 4.1, side * 8.5, rng.uniform(-3, 3))
            if i % 2 == 0:
                put("Haystack", x + rng.uniform(-3, 3), side * (33.0 + rng.uniform(0, 4)), rng.uniform(0, 360), rng.uniform(0.85, 1.1))
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
        y = side * rng.uniform(31.0, 70.0 if look == "taiga" else 52.0)
        r, acc = rng.random(), 0.0
        for name, share in trees:
            acc += share
            if r <= acc:
                break
        put(name, x, y, rng.uniform(0, 360), rng.uniform(0.75, 1.15))
    if look != "winter":
        dry = look in ("prairie", "steppe")
        tall_grass([(-75.0, 85.0, -45.0, 45.0)], avoid + [(x_, side_ * 25.5, 6.0) for x_ in np.arange(-40.0, 50.0, 17.0) for side_ in (1, -1)],
                   density=0.9 if fast else 1.8, colours=("8A8448", "C4B37A") if dry else ("5E7034", "A49A5E"))
    if extra:
        extra(put)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    bpy.context.scene.collection.objects.link(cam)
    bpy.context.scene.camera = cam
    cam.data.lens = lens
    from mathutils import Vector
    eye, target = Vector(eye or (-62.0, -30.0, 19.0)), Vector(target or (12.0, 2.0, 2.0))
    cam.location = eye
    cam.rotation_euler = (target - eye).to_track_quat("-Z", "Y").to_euler()
    w, h = size or ((960, 540) if fast else (1600, 900))
    stage.render_setup(w, h, samples=spp or (12 if fast else 40))
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
