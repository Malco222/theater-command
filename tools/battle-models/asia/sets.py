"""The Asian ground sets, written to battle-models/asia/sets/<name>.json in the shape of SET-FORMAT.md.

    python sets.py <battle-models/asia folder>

A set says what each of the field's ground kinds LOOKS like in one part of the world, picked by the province's
world-map ground look (art director's world-look-brief.md 3.3) and its town kit (3.4). Densities are per hectare (a
25 m cell is 1/16 ha). Models from another region's folder say so with "region". Looks only: the rules' ground
decides cover and passage.
"""
import json
import os
import sys

from surfaces import SURFACES, make


def surf(*names):
    out = {}
    for n in names:
        if n not in SURFACES:
            raise KeyError(f"no surface {n!r} in surfaces.py")
        out[n] = {"colour": make(n, 64, seed=sum(map(ord, n)))[2], "image": f"surfaces/{n}.png", "tile_m": SURFACES[n][1]}
    return out


def sc(model, per_ha, scale=(0.85, 1.15), gap_m=4.0, slope_max=35, near_water=None, region=None, **kw):
    d = {"model": model, "per_ha": per_ha, "scale": list(scale), "gap_m": gap_m, "slope_max": slope_max, "near_water": near_water}
    if region:
        d["region"] = region
    d.update(kw)
    return d


EAST_SNOW = {"snow": {"colour": "E4E8EE", "image": "../east/surfaces/snow.png", "tile_m": 8.0},
             "thaw_mud": {"colour": "5B4B3B", "image": "../east/surfaces/thaw_mud.png", "tile_m": 5.0}}

KITS = {
    "thatch_and_tin": {
        "houses": ["StiltHouse", "StiltHouseTin"],
        "yard": ["Banana", "CoconutPalmYoung", "StrawStack"],
        "landmark": {"spire": "Shrine", "dome": None, "station": "StiltHouseTin"},
        "about": "Houses on posts loose along a red earth track or a shore, each among bananas and a palm or two; a "
                 "tin-roofed shed by the water.",
    },
    "indian_town_burma": {
        "houses": ["StiltHouse", "StiltHouseTin"],
        "yard": ["Banana", "CoconutPalmYoung"],
        "landmark": {"spire": "Shrine", "dome": "Shrine", "station": "StiltHouseTin"},
        "about": "Burma's villages are on posts as in the thatch-and-tin kit; the town's gilt-capped stepped shrine is "
                 "its landmark. The Indian town's whitewashed flat-roofed houses will come from the flat-roofed kit.",
    },
    "walled_town": {
        "houses": ["ChinaHouse", "Farmyard", "ThatchHouse"],
        "yard": ["StrawStack", "Willow"],
        "ruin": [],
        "by_road": ["Willow"],
        "landmark": {"spire": "TieredTower", "dome": "TieredTower", "station": "ChinaHouse"},
        "about": "Grey brick farmhouses under grey-blue tile, each with its walled yard; thatch on the poorer houses; "
                 "willows at every pond and ditch; the tiered tower over the town.",
    },
    "walled_town_north": {
        "houses": ["NorthHouse", "Farmyard", "NorthHouse"],
        "yard": ["StrawStack", "GraveMound"],
        "ruin": [],
        "by_road": ["Poplar"],
        "landmark": {"spire": "TieredTower", "dome": "TieredTower", "station": "NorthHouse"},
        "about": "North China and Manchuria: flat, arched mud roofs on mud-walled houses, a brick farmyard for the "
                 "richer family; poplars in rows along the roads.",
    },
}


def jungle():
    return {
        "look": "jungle", "region": "asia",
        "where": "Burma, Malaya, Indochina's hills, Siam's south, the Indies, the Philippines, New Guinea",
        "town_kit": "thatch_and_tin",
        "haze": {"colour": "8E9A8C", "sight_m": 2500},
        "surfaces": surf("jungle_floor", "laterite", "laterite_grass", "paddy_mud", "hill_grass", "scree_wet"),
        "grounds": {
            "woods": {"surface": "jungle_floor", "cover": "canopy",
                      "scatter": [sc("JungleTree", 70, (0.8, 1.25), 6), sc("JungleGiant", 3, (0.85, 1.15), 25),
                                  sc("Bamboo", 8, (0.8, 1.2), 7), sc("Banana", 25, (0.8, 1.2), 3),
                                  sc("Undergrowth", 260, (0.7, 1.4), 1.5)],
                      "about": "a closed canopy over a dark floor; under it the thicket a man walks into and vanishes"},
            "open": {"surface": "laterite_grass",
                     "scatter": [sc("ElephantGrass", 420, (0.8, 1.3), 1.6), sc("Banana", 4, (0.8, 1.2), 8),
                                 sc("JungleTree", 1.5, (0.8, 1.2), 30)],
                     "about": "clearings of grass taller than a man, red earth showing through"},
            "field": {"surface": "paddy_mud",
                      "pattern": {"kind": "paddy", "plot_m": [12, 30], "bund_m": 0.5, "flooded": 0.33,
                                  "fill": {"model": "RicePatch", "tile_m": 2.0, "on": "unflooded"},
                                  "edge": [sc("CoconutPalm", 6, (0.85, 1.15), 10)]},
                      "about": "small rice plots behind low bunds, a third flooded; palms along the bunds"},
            "rough": {"surface": "hill_grass",
                      "scatter": [sc("ElephantGrass", 160, (0.8, 1.2), 2), sc("Bamboo", 6, (0.8, 1.2), 8),
                                  sc("Boulder", 4, (0.7, 1.4), 6), sc("JungleTree", 12, (0.8, 1.2), 10)],
                      "about": "steep hillside of grass and bamboo, the jungle broken by slips"},
            "rock": {"surface": "scree_wet",
                     "scatter": [sc("RockOutcrop", 6, (0.7, 1.4), 12, 60), sc("Boulder", 14, (0.6, 1.4), 4, 60),
                                 sc("ElephantGrass", 30, (0.6, 1.0), 3)]},
            "marsh": {"surface": "paddy_mud",
                      "scatter": [sc("Reeds", 380, (0.8, 1.2), 1.5, near_water=40), sc("CoconutPalmTall", 3, (0.85, 1.1), 15, near_water=30)]},
            "road": {"surface": "laterite", "about": "a red earth track, rutted"},
            "buildings": {"kit": "thatch_and_tin"},
            "suburb": {"kit": "thatch_and_tin"},
        },
        "kits": {k: KITS[k] for k in ("thatch_and_tin", "indian_town_burma")},
        "kit_boxes": {"indian_town_burma": [[92.0, 101.0, 10.0, 28.5]]},
        "seasons": {
            "wet": {"months": [5, 6, 7, 8, 9, 10],
                    "surfaces": {"*": None, "road": "paddy_mud", "open": None},
                    "swap": {}, "haze": {"colour": "7E8A80", "sight_m": 1400},
                    "about": "the monsoon: rain most days, tracks turned to mud, streams over their banks, paddies all flooded",
                    "pattern": {"field": {"flooded": 0.9}}},
        },
    }


def monsoon():
    return {
        "look": "monsoon", "region": "asia",
        "where": "South and central China, Formosa, Korea, Japan; the deltas of Indochina and Siam",
        "town_kit": "walled_town",
        "haze": {"colour": "A0A898", "sight_m": 3500},
        "surfaces": surf("paddy_mud", "laterite_grass", "hill_grass", "scree_wet", "laterite"),
        "grounds": {
            "field": {"surface": "paddy_mud",
                      "pattern": {"kind": "paddy", "plot_m": [10, 25], "bund_m": 0.45, "flooded": 0.33,
                                  "fill": {"model": "RicePatch", "tile_m": 2.0, "on": "unflooded"},
                                  "edge": [sc("Willow", 1.2, (0.8, 1.2), 20, near_water=15), sc("GraveMound", 0.8, (0.8, 1.3), 15)]},
                      "about": "rice country: plots a third the size of Europe's fields, a third flooded and pale"},
            "open": {"surface": "laterite_grass",
                     "scatter": [sc("GraveMound", 1.2, (0.8, 1.3), 12), sc("Willow", 0.8, (0.8, 1.2), 25),
                                 sc("Bamboo", 1.5, (0.8, 1.2), 15)]},
            "woods": {"surface": "hill_grass", "cover": "canopy",
                      "scatter": [sc("HillPine", 55, (0.8, 1.2), 6), sc("Bamboo", 18, (0.8, 1.2), 6),
                                  sc("JungleTree", 8, (0.7, 1.0), 10), sc("Undergrowth", 60, (0.7, 1.2), 2)],
                      "about": "the hill woods: pine with bamboo groves in the folds"},
            "rough": {"surface": "hill_grass",
                      "pattern": {"kind": "terraces", "step_m": [1.2, 2.5], "flooded": 0.3, "on_slope": [6, 30],
                                  "fill": {"model": "RicePatch", "tile_m": 2.0, "on": "unflooded"}},
                      "scatter": [sc("HillPine", 6, (0.8, 1.2), 12), sc("Boulder", 2, (0.7, 1.3), 8)],
                      "about": "terraces climbing the hill feet as contour lines; pine above them"},
            "rock": {"surface": "scree_wet",
                     "scatter": [sc("RockOutcrop", 6, (0.7, 1.4), 12, 60), sc("Boulder", 12, (0.6, 1.4), 4, 60),
                                 sc("HillPine", 4, (0.6, 0.9), 10, 60)]},
            "marsh": {"surface": "paddy_mud", "scatter": [sc("Reeds", 400, (0.8, 1.2), 1.5, near_water=40)]},
            "road": {"surface": "laterite", "about": "a raised earth causeway between the paddies"},
            "buildings": {"kit": "walled_town"},
            "suburb": {"kit": "walled_town"},
        },
        "kits": {k: KITS[k] for k in ("walled_town", "thatch_and_tin")},
        "kit_boxes": {"thatch_and_tin": [[97.0, 110.0, 5.0, 21.0]]},
        "seasons": {
            "wet": {"months": [6, 7, 8, 9], "surfaces": {"*": None, "road": "paddy_mud"}, "swap": {},
                    "haze": {"colour": "8E968A", "sight_m": 2000}, "pattern": {"field": {"flooded": 0.85}},
                    "about": "the summer rains: every paddy under water, the causeways the only dry going"},
            "winter": {"months": [12, 1, 2], "surfaces": {"*": None, "field": "paddy_mud"}, "swap": {},
                       "haze": {"colour": "B0B0A4", "sight_m": 4500}, "pattern": {"field": {"flooded": 0.05, "fill": None}},
                       "about": "the paddies drained to stubble and mud"},
        },
    }


def dryplain():
    return {
        "look": "dryplain", "region": "asia",
        "where": "North China, Manchuria, Inner Mongolia's farmed edge; India's dry plain",
        "town_kit": "walled_town_north",
        "haze": {"colour": "C4B898", "sight_m": 4000},
        "surfaces": {**surf("loess", "loess_furrows", "gaoliang_stubble", "scree_dry", "hill_grass", "paddy_mud"), **EAST_SNOW},
        "grounds": {
            "field": {"surface": "loess_furrows",
                      "pattern": {"kind": "fields", "plot_m": [30, 90],
                                  "mix": {"loess_furrows": 0.45, "loess": 0.2, "gaoliang_stubble": 0.35},
                                  "fill": {"model": "Gaoliang", "tile_m": 6.0, "on": "gaoliang_stubble", "months": [6, 7, 8, 9]},
                                  "edge": [sc("GraveMound", 1.0, (0.8, 1.3), 15)]},
                      "about": "small fields in ochre and green; gaoliang taller than a man in late summer, hiding a whole company"},
            "open": {"surface": "loess",
                     "scatter": [sc("GraveMound", 1.5, (0.8, 1.3), 12), sc("Poplar", 0.4, (0.8, 1.2), 30),
                                 sc("StrawStack", 0.3, (0.8, 1.2), 30)]},
            "woods": {"surface": "hill_grass", "cover": "canopy",
                      "scatter": [sc("Poplar", 45, (0.8, 1.2), 5), sc("Willow", 20, (0.8, 1.2), 7),
                                  sc("HillPine", 10, (0.8, 1.2), 8)],
                      "about": "planted groves: poplar and willow round the villages and along the river"},
            "rough": {"surface": "loess",
                      "pattern": {"kind": "terraces", "step_m": [2.0, 4.0], "flooded": 0.0, "on_slope": [8, 35]},
                      "scatter": [sc("DryBoulder", 1.5, (0.7, 1.3), 10)],
                      "about": "loess hills cut into dry terraces, gullies with sheer yellow walls"},
            "rock": {"surface": "scree_dry",
                     "scatter": [sc("DryOutcrop", 5, (0.7, 1.4), 14, 60), sc("DryBoulder", 10, (0.6, 1.3), 4, 60),
                                 sc("HillPine", 3, (0.6, 0.9), 12, 60)]},
            "marsh": {"surface": "paddy_mud", "scatter": [sc("Reeds", 300, (0.8, 1.2), 1.5, near_water=40),
                                                          sc("Willow", 3, (0.8, 1.2), 12, near_water=25)]},
            "road": {"surface": "loess", "about": "a sunken cart track worn down into the loess, poplars along it"},
            "buildings": {"kit": "walled_town_north"},
            "suburb": {"kit": "walled_town_north"},
        },
        "kits": {k: KITS[k] for k in ("walled_town_north", "walled_town")},
        "seasons": {
            "winter": {"months": [11, 12, 1, 2, 3], "surfaces": {"*": "snow", "road": "thaw_mud"},
                       "swap": {"Gaoliang": None}, "haze": {"colour": "C8CCD0", "sight_m": 5000},
                       "where_boxes": [[115.0, 135.0, 38.0, 53.5]],
                       "about": "Manchuria and the north-east under snow; elsewhere on the plain the fields are bare and frozen"},
            "thaw": {"months": [4], "surfaces": {"*": None, "road": "thaw_mud", "field": "thaw_mud"},
                     "where_boxes": [[115.0, 135.0, 38.0, 53.5]], "about": "the spring thaw: deep mud"},
        },
    }


def zagros():
    return {
        "look": "desert", "also_looks": ["steppe", "drysouth"], "region": "asia",
        "where": "the Zagros and the Kurdish hills, Iran's western mountains",
        # the hills north of the plain; the Zagros east of it, its front along Mandali, Mehran and Dehloran; the
        # Bakhtiari south-east of Khuzestan; Fars. None of them over the river plain's boxes
        "where_boxes": [[42.0, 48.0, 34.4, 38.5], [45.6, 50.5, 33.2, 34.4], [47.2, 50.5, 32.0, 33.2], [48.6, 50.5, 30.0, 32.0],
                        [50.5, 55.0, 27.5, 33.0]],
        "town_kit": "flat_roofed",
        "haze": {"colour": "B8B0A0", "sight_m": 6000},
        "surfaces": surf("zagros_scrub", "scree_dry"),
        "grounds": {
            "rough": {"surface": "zagros_scrub",
                      "scatter": [sc("DryBoulder", 8, (0.6, 1.5), 5), sc("DryOutcrop", 1.5, (0.7, 1.3), 18)],
                      "about": "long bedded ridges of warm stone, thin scrub, no trees but in the valleys"},
            "rock": {"surface": "scree_dry",
                     "scatter": [sc("DryOutcrop", 7, (0.8, 1.6), 12, 65), sc("DryBoulder", 16, (0.6, 1.5), 4, 65)]},
            "open": {"surface": "zagros_scrub", "scatter": [sc("DryBoulder", 2, (0.6, 1.3), 8)]},
            "buildings": {"kit": "flat_roofed", "region": "med"},
        },
        "kits": {},
        "about": "Draws only inside its boxes, over whichever dry look the map gives there; the rest of the ground "
                 "(desert, the flat-roofed town) is the Mediterranean's set.",
    }


def riverplain():
    return {
        "look": "desert", "also_looks": ["steppe", "drysouth"], "region": "asia",
        "where": "the Tigris and Euphrates plain from Baghdad to Basra, and the marshes",
        # Baghdad's reach to the hill front at Mandali; the Tigris by Kut to Dehloran's foot; the south to Basra
        "where_boxes": [[43.0, 45.6, 33.2, 34.4], [43.0, 47.2, 32.0, 33.2], [44.0, 48.6, 29.8, 32.0]],
        "town_kit": "flat_roofed",
        "haze": {"colour": "D0C4A4", "sight_m": 3000},
        "surfaces": surf("alluvial", "paddy_mud", "loess_furrows"),
        "grounds": {
            "open": {"surface": "alluvial", "scatter": [sc("Reeds", 2, (0.6, 1.0), 10, near_water=60)],
                     "about": "flat grey-brown silt dried into plates, salt white in the hollows"},
            "field": {"surface": "loess_furrows",
                      "pattern": {"kind": "ditched_plots", "plot_m": [40, 120], "ditch_m": 1.5, "water_in_ditch": True,
                                  "mix": {"loess_furrows": 0.6, "alluvial": 0.4},
                                  "edge": [sc("DatePalm", 10, (0.85, 1.15), 7, region="med", stand_in="CoconutPalm")]},
                      "about": "irrigated plots between ditches, date palms in rows along the canals"},
            "woods": {"surface": "alluvial", "cover": "canopy",
                      "scatter": [sc("DatePalm", 110, (0.85, 1.15), 6, region="med", stand_in="CoconutPalm"),
                                  sc("Banana", 4, (0.8, 1.2), 6)],
                      "about": "the palm groves along the rivers"},
            "marsh": {"surface": "paddy_mud",
                      "scatter": [sc("Reeds", 520, (0.9, 1.3), 1.4, near_water=60)],
                      "about": "the great reed marshes of the lower rivers, reeds twice a man's height"},
            "buildings": {"kit": "flat_roofed", "region": "med"},
        },
        "kits": {},
        "about": "Draws only inside its box over the dry look; the flat-roofed town and the date palm are the "
                 "Mediterranean's (the coconut palm stands in until the date palm lands).",
    }


SETS = {"jungle": jungle, "monsoon": monsoon, "dryplain": dryplain, "zagros": zagros, "riverplain": riverplain}


def main():
    root = sys.argv[1]
    os.makedirs(os.path.join(root, "sets"), exist_ok=True)
    for name, fn in SETS.items():
        with open(os.path.join(root, "sets", f"{name}.json"), "w") as f:
            json.dump(fn(), f, indent=1)
        print("wrote", name)


if __name__ == "__main__":
    main()
