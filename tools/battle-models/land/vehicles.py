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
    "Type97": tanks_jap.type97,
    "7TP": tanks_pol.tp7,
    "LightTank": tanks_any.light_tank,
    "MediumTank": tanks_any.medium_tank,
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
    "Type97": {"nation": "JAP", "role": "medium tank", "years": "1938-45", "units": "armor (Japan)"},
    "7TP": {"nation": "POL", "role": "light tank", "years": "1937-39", "units": "armor (Poland)"},
    "LightTank": {"nation": "any", "role": "light tank", "years": "1936-42", "units": "armor (every nation without a tank of its own; the United States to 1942, the Soviet Union to 1941)"},
    "MediumTank": {"nation": "any", "role": "medium tank", "years": "1940-45", "units": "armor (every nation without a tank of its own, once it has researched medium tanks)"},
    "StuG3": {"nation": "GER", "role": "assault gun", "years": "1942-45", "units": "assault guns beside Germany's armor and infantry"},
    "SU85": {"nation": "SOV", "role": "tank destroyer", "years": "1943-45", "units": "tank destroyers beside the Soviet Union's armor"},
    "M10": {"nation": "USA", "role": "tank destroyer", "years": "1942-45", "units": "tank destroyers beside the United States' armor (Britain and the Free French were lent her too)"},
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
    "Sexton": {"nation": "UK", "role": "self-propelled gun", "years": "1943-45", "units": "the artillery that keeps up with the armor of Britain and the Commonwealth"},
}


# What a builder takes to build everything the game's file carries (both of a Panzer IV's guns), and the faces a
# vehicle showed through the war, for the sheet of them: (words, what to pass the builder).
WHOLE = {"PanzerIV": {"gun": "all", "skirts": True}}
FACES = {
    "PanzerIV": [("1939 to early 1942: the short gun, dark grey", {"gun": "short", "paint": "grey"}),
                 ("1942: the long gun, dark grey", {"gun": "long", "paint": "grey"}),
                 ("Early 1943: the long gun, dark yellow", {"gun": "long", "paint": "yellow"}),
                 ("May 1943 on: skirts on her hull and turret (this one has lost a plate)", {"gun": "long", "paint": "yellow", "skirts": "r4"})],
    "Tiger": [("1942 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})],
    "Hanomag": [("1939 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})],
    "leFH18": [("1939 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})],
    "StuG3": [("1942 to early 1943: dark grey", {"paint": "grey"}), ("1943 on: dark yellow", {"paint": "yellow"})],
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
    return fn(fine, tag, **choice) if tag else fn(fine, **choice)
