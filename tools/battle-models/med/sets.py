"""The Mediterranean's ground sets, in the shape every region shares (battle-models/asia/SET-FORMAT.md, with the
additions of 19 Nov): what each kind of battle ground looks like in the desert and in the dry south.

    python sets.py <med folder>          # writes <med>/sets/desert.json and <med>/sets/drysouth.json

The art director's calls (19 Nov, 03:35Z): the default desert field is the stony 170, 140, 106 with patches of fine
sand and camel-thorn scrub, low escarpments and wadis; dunes rarely. The drysouth look is "Europe's dry-south ochre
mix; thin olive scrub; few fields" (world-look brief 3.3); its towns keep Europe's kit (3.4).
Looks only: the rules' ground decides who passes, sees and hides.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import surfaces  # noqa: E402


def sc(model, per_ha, scale=(0.85, 1.2), gap_m=3.0, slope_max=35, near_water=None, **kw):
    return {"model": model, "per_ha": per_ha, "scale": list(scale), "gap_m": gap_m, "slope_max": slope_max, "near_water": near_water, **kw}


def surf(name, about):
    fn, tile = surfaces.SURFACES[name]
    return {"colour": SEEN.get(name, ""), "image": f"surfaces/{name}.png", "tile_m": tile, "about": about}


SEEN = {}

FLAT_ROOFED = {
    "houses": ["FlatRoofHouse", "FlatRoofHouseStone", "FlatRoofHouseTwo"],
    "yard": ["StoneWall"],
    "landmark": {"spire": "SlimTower", "dome": "DomeHouse", "station": "FlatRoofHouseTwo"},
    "edge": [sc("DatePalm", 14, (0.8, 1.2), 6.0)],
    "by_road": [{"model": "TelegraphPole", "every_m": 55, "off_m": 14}],
    "ruin": None,
    "about": ("The flat-roofed town of the world-look brief 3.4: mud 206,186,150 and pale stone 224,208,174, flat roofs a "
              "shade lighter, packed with no straight street; one dome 236,230,214 and one slim tower with a grey-green "
              "cap; palms at the edge (`edge`); a telegraph line along the road out. `station` is a plain two-storey "
              "house until a station of its own is made. `ruin` is the game's own burnt pass until a ruin model exists."),
}


def desert():
    return {
        "look": "desert",
        "region": "med",
        "where": "North Africa from the Atlantic to the Nile, Sinai, the Levant's deserts, Arabia; and wherever else the map's look is desert outside a set's where_boxes",
        "town_kit": "flat_roofed",
        "haze": {"colour": "D6C9B2", "sight_m": 3400},
        "surfaces": {
            "gravel": surf("gravel", "the stony desert: small stones, dark where the sun varnished them, on paler grit (170,140,106 seen)"),
            "sand": surf("sand", "fine sand in patches and drifts, a faint ripple"),
            "wadi_floor": surf("wadi_floor", "a dry wadi's floor: pale silt, faint plates, pebbles"),
            "escarp_rock": surf("escarp_rock", "limestone in beds: the escarpment's cap and rock ground"),
            "scree": surf("scree", "broken stone at an escarpment's foot"),
            "sebkha": surf("sebkha", "a salt flat's grey-white crust (the depressions, the coast's salt marshes)"),
            "track": surf("track", "a desert track: the crust broken, paler grit in the ruts"),
        },
        "grounds": {
            "open": {"surface": "gravel",
                     "patches": [{"surface": "sand", "share": 0.22, "size_m": [15, 70]}],
                     "scatter": [sc("Stones", 22, (0.7, 1.4), 4.0), sc("CamelThorn", 5, (0.7, 1.5), 6.0), sc("Boulder", 0.4, (0.6, 1.3), 25.0)],
                     "about": "the default desert field (art director, 19 Nov): stony gravel with patches of fine sand and camel-thorn scrub"},
            "field": {"surface": "wadi_floor",
                      "pattern": {"kind": "fields", "plot_m": [15, 40], "mix": {"wadi_floor": 0.6, "sand": 0.4},
                                  "edge": [sc("DatePalm", 12, (0.8, 1.2), 6.0)]},
                      "about": "an oasis's gardens: small plots behind mud walls, palms along their edges"},
            "woods": {"surface": "sand", "cover": "canopy",
                      "scatter": [sc("DatePalm", 95, (0.8, 1.25), 5.5), sc("CamelThorn", 8, (0.8, 1.4), 4.0)],
                      "about": "a palm grove: an oasis, or the palms along the coast road"},
            "rough": {"surface": "scree",
                      "patches": [{"surface": "escarp_rock", "share": 0.25, "size_m": [10, 40]}],
                      "scatter": [sc("Boulder", 14, (0.7, 2.4), 6.0, slope_max=55), sc("Stones", 60, (0.7, 1.5), 2.5, slope_max=55),
                                  sc("CamelThorn", 6, (0.7, 1.4), 5.0)],
                      "about": "broken ground: an escarpment's face and foot, the low ridges"},
            "rock": {"surface": "escarp_rock",
                     "scatter": [sc("Boulder", 22, (0.8, 2.8), 5.0, slope_max=70), sc("Stones", 40, (0.7, 1.4), 2.5, slope_max=70)],
                     "about": "bare rock: the escarpments' caps and the rocky jebels; where a sangar is built instead of a trench"},
            "marsh": {"surface": "sebkha", "scatter": [sc("CamelThorn", 2, (0.6, 1.0), 10.0)],
                      "about": "a salt flat: the depressions and the coast's salt marshes, impassable when wet"},
            "road": {"surface": "track", "about": "a desert track, braided where traffic has gone round soft patches"},
            "works": {"surface": "gravel", "cover": "walls",
                      "scatter": [sc("Sangar", 6, (0.9, 1.15), 18.0, slope_max=30)],
                      "about": "field works on ground too hard to dig: sangars of piled stone"},
            "buildings": {"kit": "flat_roofed"},
        },
        "kits": {"flat_roofed": FLAT_ROOFED},
        "relief": {"escarpment": {"high_m": [8, 20], "face_deg": [30, 60], "scree_m": 30},
                   "wadi": {"wide_m": [8, 20], "deep_m": [0.8, 2.0], "floor": "wadi_floor"},
                   "dunes": {"share": 0.03, "about": "rare: the art director's call; only where the map's relief draws them"},
                   "about": "how the field's height is drawn in the desert: low escarpments with scree at their foot, wadis cut into the plain"},
        "effects": {"dust": "sand", "about": ("the word the effects kit takes for this ground (kit d9b3d79, README 'Desert dust'): "
                                              "Blasts->VehicleDust(..., TEXT(\"sand\"), tracked) for each moving vehicle's two dust sockets every frame, "
                                              "and Blasts->Muzzle(..., TEXT(\"sand\")) for the dust veil a gun raises")},
        "sheet": "sheets/desert.jpg",
        "about": ("The desert war's ground: stony plain, sand in patches, scrub, wadis and escarpments, villages of flat roofs "
                  "under the escarpments with palms at the edge, a telegraph line along the coast road. Additions this set "
                  "uses: `patches` on a ground (another surface laid in patches: its share of the ground and their size) and "
                  "`relief` (how the field's heights are drawn); `edge` on a kit (stood round a town's edge)."),
    }


def drysouth():
    return {
        "look": "drysouth",
        "region": "med",
        "where": "Sicily, Sardinia, the south of Italy, Greece and the Aegean, Crete, Cyprus, the coastal hills of the Maghreb and the Levant, Spain's dry south",
        "town_kit": "europe",
        "haze": {"colour": "B3BFCC", "sight_m": 2400},
        "surfaces": {
            "stubble": surf("stubble", "wheat stubble after the harvest: gold rows, straw, earth showing"),
            "plough": surf("plough", "ploughland: brown earth in furrows"),
            "fallow": surf("fallow", "fallow: dry ochre grass with green-brown patches"),
            "macchia_ground": surf("macchia_ground", "the dry hills: thin soil, limestone breaking through, dark scrub in clumps"),
            "limestone": surf("limestone", "bare pale limestone, pitted and cracked"),
            "track": {"colour": SEEN.get("track", ""), "image": "surfaces/track.png", "tile_m": 6.0, "about": "a dusty lane (the desert's track)"},
        },
        "grounds": {
            "open": {"surface": "fallow",
                     "patches": [{"surface": "macchia_ground", "share": 0.3, "size_m": [20, 80]}],
                     "scatter": [sc("Macchia", 14, (0.7, 1.5), 4.0), sc("OliveTree", 1.5, (0.85, 1.15), 15.0), sc("Stones", 6, (0.6, 1.2), 5.0)],
                     "about": "ochre ground with thin olive scrub (world-look brief 3.3: thin olive scrub, canopy 104,110,70)"},
            "field": {"surface": "stubble",
                      "pattern": {"kind": "fields", "plot_m": [30, 90], "mix": {"stubble": 0.5, "plough": 0.25, "fallow": 0.25},
                                  "edge": [sc("StoneWall", 1.2, (1.0, 1.0), 6.0), sc("Macchia", 4, (0.7, 1.3), 6.0)]},
                      "scatter": [sc("Farmhouse", 0.05, (1.0, 1.0), 150.0, slope_max=15)],
                      "about": "fields of wheat (stubble in high summer), plough and fallow, dry-stone walls between, a farm now and then"},
            "woods": {"surface": "macchia_ground", "cover": "canopy",
                      "pattern": {"kind": "fields", "plot_m": [60, 140], "mix": {"macchia_ground": 1.0},
                                  "fill": {"model": "OliveTree", "tile_m": 7.5, "on": "macchia_ground"},
                                  "edge": [sc("StoneWall", 1.2, (1.0, 1.0), 6.0), sc("Cypress", 1.0, (0.85, 1.1), 8.0)]},
                      "about": "olive groves in rows behind dry-stone walls, a line of cypress here and there"},
            "rough": {"surface": "macchia_ground",
                      "scatter": [sc("Macchia", 60, (0.8, 1.7), 2.5, slope_max=50), sc("Boulder", 4, (0.6, 1.6), 10.0, slope_max=55),
                                  sc("Cypress", 0.6, (0.8, 1.1), 20.0)],
                      "about": "the macchia hills: dark scrub, limestone breaking through"},
            "rock": {"surface": "limestone",
                     "scatter": [sc("Boulder", 16, (0.7, 2.4), 6.0, slope_max=70), sc("Macchia", 10, (0.6, 1.2), 4.0, slope_max=60)],
                     "about": "bare limestone: the crags of Sicily's and Greece's hills"},
            "road": {"surface": "track", "about": "a white dusty lane"},
            "works": {"surface": "macchia_ground", "cover": "walls",
                      "scatter": [sc("Sangar", 3, (0.9, 1.15), 18.0, slope_max=30)],
                      "about": "on rock ground too hard to dig, sangars as in the desert"},
            "buildings": {"kit": "europe"},
        },
        "kits": {"south_farm": {"houses": ["Farmhouse"], "yard": ["StoneWall"], "edge": [sc("Cypress", 6, (0.85, 1.1), 7.0)],
                                "about": "the lone farm of the dry south: whitewashed, red tile, outside stair, cypress by it"}},
        "sheet": "sheets/drysouth.jpg",
        "about": ("Sicily and the south: ochre ground and thin olive scrub, fields few and small, olive groves behind dry-stone "
                  "walls, cypress lines, a lone farm. Towns keep Europe's kit (world-look brief 3.4). Uses `patches`, as the desert set."),
    }


def main():
    med = sys.argv[1]
    for name in surfaces.SURFACES:
        p = os.path.join(med, "surfaces", name + ".png")
        if os.path.exists(p):
            from PIL import Image
            import numpy as np
            a = np.asarray(Image.open(p).convert("RGB"), dtype=np.float32).reshape(-1, 3).mean(axis=0)
            SEEN[name] = "".join(f"{int(round(c)):02X}" for c in a)
    os.makedirs(os.path.join(med, "sets"), exist_ok=True)
    for fn in (desert, drysouth):
        data = fn()
        path = os.path.join(med, "sets", data["look"] + ".json")
        with open(path, "w") as f:
            json.dump(data, f, indent=1)
        print(path, os.path.getsize(path))


if __name__ == "__main__":
    main()
