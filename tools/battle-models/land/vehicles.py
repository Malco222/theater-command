"""Every land vehicle by name.

BUILDERS[name](fine) -> the Works that built it (its model is `.m`). ABOUT[name] says whose it is, what it does on the
field and which of the game's units wear it (the unit table's names, generic-units/unit-table.json).
"""
import tanks_ger

BUILDERS = {
    "PanzerIV": tanks_ger.panzer4,
}

ABOUT = {
    "PanzerIV": {"nation": "GER", "role": "medium tank", "years": "1939-45", "units": "armor (Germany, to 1944)"},
}


def build(name, fine=True, tag=None):
    fn = BUILDERS[name]
    return fn(fine, tag) if tag else fn(fine)
