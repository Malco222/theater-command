"""Every land vehicle by name.

BUILDERS[name](fine) -> the Works that built it (its model is `.m`). ABOUT[name] says whose it is, what it does on the
field and which of the game's units wear it (the unit table's names, generic-units/unit-table.json).
"""
import tanks_any
import tanks_ger
import tanks_sov
import tanks_usa

BUILDERS = {
    "PanzerIV": tanks_ger.panzer4,
    "Panther": tanks_ger.panther,
    "Tiger": tanks_ger.tiger,
    "T34": tanks_sov.t34,
    "T34-85": tanks_sov.t34_85,
    "Sherman": tanks_usa.sherman,
    "LightTank": tanks_any.light_tank,
    "MediumTank": tanks_any.medium_tank,
}

ABOUT = {
    "PanzerIV": {"nation": "GER", "role": "medium tank", "years": "1939-45", "units": "armor (Germany, to 1944)"},
    "Panther": {"nation": "GER", "role": "medium tank", "years": "1943-45", "units": "armor (Germany, from 1944)"},
    "Tiger": {"nation": "GER", "role": "heavy tank", "years": "1942-45", "units": "heavy armor (Germany)"},
    "T34": {"nation": "SOV", "role": "medium tank", "years": "1941-45", "units": "armor (Soviet Union, to 1943)"},
    "T34-85": {"nation": "SOV", "role": "medium tank", "years": "1944-45", "units": "armor (Soviet Union, from 1944)"},
    "Sherman": {"nation": "USA", "role": "medium tank", "years": "1942-45", "units": "armor (United States from 1942; Britain and the Commonwealth from 1943, in their own paint)"},
    "LightTank": {"nation": "any", "role": "light tank", "years": "1936-42", "units": "armor (every nation without a tank of its own; the United States to 1942, the Soviet Union to 1941)"},
    "MediumTank": {"nation": "any", "role": "medium tank", "years": "1940-45", "units": "armor (every nation without a tank of its own, once it has researched medium tanks)"},
}


# What a builder takes to build everything the game's file carries (both of a Panzer IV's guns), and the faces a
# vehicle showed through the war, for the sheet of them: (words, what to pass the builder).
WHOLE = {"PanzerIV": {"gun": "all"}}
FACES = {
    "PanzerIV": [("1939 to early 1942: the short gun, dark grey", {"gun": "short", "paint": "grey"}),
                 ("1942: the long gun, dark grey", {"gun": "long", "paint": "grey"}),
                 ("1943 on: the long gun, dark yellow", {"gun": "long", "paint": "yellow"})],
}


def build(name, fine=True, tag=None, whole=False, **choice):
    """`choice`: what a builder offers besides (a Panzer IV's gun="short", paint="yellow"). `whole`: everything the
    game's file carries, swaps and all (for the export; a picture builds one face of her)."""
    fn = BUILDERS[name]
    if whole:
        choice = {**WHOLE.get(name, {}), **choice}
    return fn(fine, tag, **choice) if tag else fn(fine, **choice)
