"""Every land vehicle by name.

BUILDERS[name](fine) -> the Works that built it (its model is `.m`). ABOUT[name] says whose it is, what it does on the
field and which of the game's units wear it (the unit table's names, generic-units/unit-table.json).
"""
import armoured_cars
import carriers
import flak
import guns
import guns_at
import sp_guns
import tank_destroyers
import tanks_any
import tanks_fra
import tanks_ger
import tanks_ita
import tanks_jap
import tanks_pol
import tanks_sov
import tanks_uk
import tanks_usa

BUILDERS = {
    "PanzerIV": tanks_ger.panzer4,
    "Panther": tanks_ger.panther,
    "Tiger": tanks_ger.tiger,
    "T34": tanks_sov.t34,
    "T34-85": tanks_sov.t34_85,
    "Sherman": tanks_usa.sherman,
    "Matilda": tanks_uk.matilda,
    "CharB1": tanks_fra.char_b1,
    "M13": tanks_ita.m13,
    "Semovente": tanks_ita.semovente,
    "L3": tanks_ita.l3,
    "Type97": tanks_jap.type97,
    "HaGo": tanks_jap.hago,
    "7TP": tanks_pol.tp7,
    "LightTank": tanks_any.light_tank,
    "MediumTank": tanks_any.medium_tank,
    "AssaultGun": tanks_any.assault_gun,
    "StuG3": tank_destroyers.stug3,
    "SU85": tank_destroyers.su85,
    "M10": tank_destroyers.m10,
    "Gun75": guns.gun75,
    "leFH18": guns.lefh18,
    "25pdr": guns.pounder25,
    "Howitzer105": guns.howitzer105,
    "ZiS3": guns.zis3,
    "Pak40": guns_at.pak40,
    "AntiTankGun": guns_at.anti_tank_gun,
    "Truck": carriers.truck,
    "Hanomag": carriers.hanomag,
    "M3Halftrack": carriers.m3_halftrack,
    "SdKfz222": armoured_cars.sdkfz222,
    "M8": armoured_cars.m8,
    "ArmouredCar": armoured_cars.armoured_car,
    "M16": flak.m16,
    "FlakHalftrack": flak.flak_halftrack,
    "FlakTruck": flak.flak_truck,
    "M7": sp_guns.m7,
    "Sexton": sp_guns.sexton,
    "Hummel": sp_guns.hummel,
    "SU76": sp_guns.su76,
    "SelfPropelledGun": sp_guns.sp_gun,
}

ABOUT = {
    "PanzerIV": {"nation": "GER", "role": "medium tank", "years": "1939-45", "units": "armor (Germany, to 1944)"},
    "Panther": {"nation": "GER", "role": "medium tank", "years": "1943-45", "units": "armor (Germany, from 1944)"},
    "Tiger": {"nation": "GER", "role": "heavy tank", "years": "1942-45", "units": "heavy armor (Germany)"},
    "T34": {"nation": "SOV", "role": "medium tank", "years": "1941-45", "units": "armor (Soviet Union, to 1943)"},
    "T34-85": {"nation": "SOV", "role": "medium tank", "years": "1944-45", "units": "armor (Soviet Union, from 1944)"},
    "Sherman": {"nation": "USA", "role": "medium tank", "years": "1942-45", "units": "armor (United States from 1942; Britain and the Commonwealth from 1943, in their own paint)"},
    "Matilda": {"nation": "UK", "role": "infantry tank", "years": "1939-42", "units": "armor (Britain and the Commonwealth to 1942; Australia throughout)"},
    "CharB1": {"nation": "FRA", "role": "heavy tank", "years": "1936-40", "units": "armor (France, Vichy France, Free France)"},
    "M13": {"nation": "ITA", "role": "medium tank", "years": "1940-43", "units": "armor (Italy)"},
    "Semovente": {"nation": "ITA", "role": "assault gun", "years": "1941-43", "units": "assault guns beside Italy's armor (from 1941-11)"},
    "L3": {"nation": "ITA", "role": "tankette", "years": "1935-43", "units": "armor (Italy, 1939 to 1940; the light tanks beside the M13 after)"},
    "Type97": {"nation": "JAP", "role": "medium tank", "years": "1938-45", "units": "armor (Japan)"},
    "HaGo": {"nation": "JAP", "role": "light tank", "years": "1936-45", "units": "armor (Japan: the light tank of the cavalry, the infantry's support and the island garrisons, beside the Type 97)"},
    "7TP": {"nation": "POL", "role": "light tank", "years": "1937-39", "units": "armor (Poland)"},
    "LightTank": {"nation": "any", "role": "light tank", "years": "1936-42", "units": "armor (every nation without a tank of its own; the United States to 1942, the Soviet Union to 1941)"},
    "MediumTank": {"nation": "any", "role": "medium tank", "years": "1940-45", "units": "armor (every nation without a tank of its own, once it has researched medium tanks)"},
    "StuG3": {"nation": "GER", "role": "assault gun", "years": "1942-45", "units": "assault guns beside Germany's armor and infantry"},
    "SU85": {"nation": "SOV", "role": "tank destroyer", "years": "1943-45", "units": "tank destroyers beside the Soviet Union's armor"},
    "M10": {"nation": "USA", "role": "tank destroyer", "years": "1942-45", "units": "tank destroyers beside the United States' armor (Britain and the Free French were lent her too)"},
    "AssaultGun": {"nation": "any", "role": "assault gun", "years": "1941-45", "units": "assault guns and tank destroyers beside the armor and infantry of every army without one of its own"},
    "Gun75": {"nation": "FRA", "role": "field gun", "years": "1897-1945", "units": "artillery (France, and every nation without a gun of its own)"},
    "leFH18": {"nation": "GER", "role": "field howitzer", "years": "1935-45", "units": "artillery (Germany)"},
    "25pdr": {"nation": "UK", "role": "field gun", "years": "1940-45", "units": "artillery (Britain and the Commonwealth)"},
    "Howitzer105": {"nation": "USA", "role": "field howitzer", "years": "1941-45", "units": "artillery (United States)"},
    "ZiS3": {"nation": "SOV", "role": "field gun", "years": "1942-45", "units": "artillery (Soviet Union)"},
    "Pak40": {"nation": "GER", "role": "anti-tank gun", "years": "1942-45", "units": "anti-tank guns in a German line and its gun pits"},
    "AntiTankGun": {"nation": "any", "role": "anti-tank gun", "years": "1941-45", "units": "anti-tank guns in every other army's line and gun pits"},
    "Truck": {"nation": "any", "role": "lorry", "years": "1936-45", "units": "carrier (every nation but Germany and the United States), and what tows the guns"},
    "Hanomag": {"nation": "GER", "role": "armoured half-track", "years": "1939-45", "units": "carrier (Germany)"},
    "M3Halftrack": {"nation": "USA", "role": "armoured half-track", "years": "1941-45", "units": "carrier (United States)"},
    "SdKfz222": {"nation": "GER", "role": "armoured car", "years": "1938-45", "units": "scouts ahead of Germany's armor and motorized infantry"},
    "M8": {"nation": "USA", "role": "armoured car", "years": "1943-45", "units": "scouts ahead of the United States' armor (the Free French were lent her too)"},
    "ArmouredCar": {"nation": "any", "role": "armoured car", "years": "1936-45", "units": "scouts ahead of every other army's armor and cavalry"},
    "M16": {"nation": "USA", "role": "anti-aircraft half-track", "years": "1943-45", "units": "the light flak that rolls with the United States' columns"},
    "FlakHalftrack": {"nation": "GER", "role": "anti-aircraft half-track", "years": "1940-45", "units": "the light flak that rolls with Germany's armor"},
    "FlakTruck": {"nation": "any", "role": "anti-aircraft lorry", "years": "1936-45", "units": "the light flak that rolls with every other army's columns"},
    "M7": {"nation": "USA", "role": "self-propelled howitzer", "years": "1942-45", "units": "the artillery that keeps up with the United States' armor (the Free French were lent her too)"},
    "Hummel": {"nation": "GER", "role": "self-propelled howitzer", "years": "1943-45", "units": "the heavy artillery that keeps up with Germany's armor"},
    "SU76": {"nation": "SOV", "role": "self-propelled gun", "years": "1943-45", "units": "the light guns that keep up with the Soviet Union's infantry and armor"},
    "SelfPropelledGun": {"nation": "any", "role": "self-propelled gun", "years": "1942-45", "units": "the guns that keep up with the armor of every army without a self-propelled gun of its own"},
    "Sexton": {"nation": "UK", "role": "self-propelled gun", "years": "1943-45", "units": "the artillery that keeps up with the armor of Britain and the Commonwealth"},
}


# What a builder takes to build everything the game's file carries (both of a Panzer IV's guns), and the faces a
# vehicle showed through the war, for the sheet of them: (words, what to pass the builder).
WHOLE = {"PanzerIV": {"gun": "all", "skirts": True, "zimmerit": True}, "StuG3": {"mantlet": "all", "skirts": True}, "Tiger": {"zimmerit": True},
         "Panther": {"ambush": True}}
FACES = {
    "PanzerIV": [("1939 to early 1942: the short gun, dark grey", {"gun": "short", "paint": "grey"}),
                 ("1942: the long gun, dark grey", {"gun": "long", "paint": "grey"}),
                 ("Early 1943: the long gun, dark yellow", {"gun": "long", "paint": "yellow"}),
                 ("May 1943 on: skirts on her hull and turret (this one has lost a plate)", {"gun": "long", "paint": "yellow", "skirts": "r4"}),
                 ("1944: the ribbed paste (zimmerit) on her plates and her turret's shield", {"gun": "long", "paint": "yellow", "skirts": "l3", "zimmerit": True})],
    "Tiger": [("1942 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"}),
              ("1944: the ribbed paste (zimmerit) on her plates", {"paint": "yellow", "zimmerit": True})],
    "Panther": [("1943 on: dark yellow", {"paint": "yellow"}), ("1943 on: olive green and red-brown sprayed over it", {"paint": "camo"}),
                ("Late 1944: the ambush dots over the three colours", {"paint": "camo", "ambush": True})],
    "Hanomag": [("1939 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})],
    "leFH18": [("1939 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})],
    "StuG3": [("1942 to early 1943: the box shield round her gun, dark grey", {"paint": "grey"}),
              ("Early 1943: dark yellow", {"paint": "yellow"}),
              ("May 1943 on: skirts along her hull (this one has lost a plate)", {"paint": "yellow", "skirts": "r4"}),
              ("1944 on, about half of them: the cast shield shaped like a boar's head", {"paint": "yellow", "mantlet": "cone", "skirts": True})],
    "Pak40": [("1942 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})],
    "SdKfz222": [("1938 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})],
    "FlakHalftrack": [("1940 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})],
    "Truck": [("Under her tilt", {}), ("The tilt stowed: her benches", {"tilt": False})],
}


def build(name, fine=True, tag=None, whole=False, **choice):
    """`choice`: what a builder offers besides (a Panzer IV's gun="short", paint="yellow"). `whole`: everything the
    game's file carries, swaps and all (for the export; a picture builds one face of her)."""
    fn = BUILDERS[name]
    if whole:
        choice = {**WHOLE.get(name, {}), **choice}
    v = fn(fine, tag, **choice) if tag else fn(fine, **choice)
    if getattr(v, "dice", None):
        if not getattr(v, "_worn", False):          # (a gun has no tank clips to wear her: she is worn here, her last piece on)
            v.worn(**getattr(v, "worn_kw", {}))
        import works
        works.choice_dirt(v.m, getattr(v, "worn_kw", {}).get("ground"))
    return v


# ---- The Eastern Front's additions (the KV-1, from the Eastern Front thread's 7b6d455c; appended so no other lines move)
import tanks_kv  # noqa: E402

BUILDERS["KV1"] = tanks_kv.kv1
ABOUT["KV1"] = {"nation": "SOV", "role": "heavy tank", "years": "1939-43",
                "units": "armor (Soviet Union, 1941-42, beside the T-34: one tank in four of a block, the unit table's call); the Soviet heavy tank wherever a heavy kind is drawn"}
WHOLE["KV1"] = {"turret": "all"}
FACES["KV1"] = [("1941: the welded turret", {"turret": "welded"}),
                ("1942 on: the cast turret (beside the welded one through 1942)", {"turret": "cast"})]

# ---- Asia's additions (the BT-7: the tanks thread, 22 Nov; appended so no other lines move)
import tanks_bt  # noqa: E402

BUILDERS["BT7"] = tanks_bt.bt7
ABOUT["BT7"] = {"nation": "SOV", "role": "light tank", "years": "1935-42",
                "units": "armor (Soviet Union: the fast tank of the tank brigades, Khalkhin Gol 1939 to 1941, beside the T-26); the Soviet light tank wherever a light kind is drawn"}

# ---- the American light tank (the M3A1 Stuart: the tanks thread, 22 Nov; appended so no other lines move)
import tanks_us_light  # noqa: E402

BUILDERS["Stuart"] = tanks_us_light.stuart
ABOUT["Stuart"] = {"nation": "USA", "role": "light tank", "years": "1941-45",
                   "units": "armor (USA and the Marines: the light tank of 1941-43, the Philippines, Guadalcanal, Tunisia; Britain's 'Honey' in the desert and Burma; China's tank brigades); the American light tank wherever a light kind is drawn"}
WHOLE["BT7"] = {"turret": "all"}
FACES["BT7"] = [("1935: the round turret", {"turret": "round"}),
                ("1937 on: the cone turret (half the BT-7s from 1938)", {"turret": "cone"})]

# ---- light cars and motorcycles (Brian 23 Nov: jeeps and motorcycles in the ground battles; appended so no other lines move)
import light_cars  # noqa: E402

BUILDERS["Jeep"] = light_cars.jeep
ABOUT["Jeep"] = {"nation": "USA", "role": "light car", "years": "1941-45",
                 "units": "scouts, staffs and messengers (United States; Britain and the Commonwealth from 1942 and the Free French, in their own paint; "
                          "the Soviet Union and China were lent her too); the jeep with the .30 on its post rides ahead of the American armor"}
FACES["Jeep"] = [("The .30 Browning on its post, the screen folded", {}), ("A staff car: no gun", {"mg": False})]
BUILDERS["Kubelwagen"] = light_cars.kubelwagen
ABOUT["Kubelwagen"] = {"nation": "GER", "role": "light car", "years": "1940-45",
                       "units": "staffs, scouts, messengers and every German unit's light car, in Europe, Russia and Africa (Italy had some too)"}
FACES["Kubelwagen"] = [("1940 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})]
import motorcycles  # noqa: E402

BUILDERS["R75"] = motorcycles.r75
ABOUT["R75"] = {"nation": "GER", "role": "motorcycle with sidecar", "years": "1941-45",
                "units": "the motorcycle troops and the scouts ahead of Germany's armor and motorized infantry, in Europe, Russia and Africa; solo (the sidecar off), the despatch riders"}
FACES["R75"] = [("With her sidecar and its MG 34, dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"}), ("Solo: a despatch rider", {"sidecar": False})]
