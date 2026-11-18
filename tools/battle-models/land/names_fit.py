"""Which of the historian's names each tank can wear, and how (Brian's pick b477).

    python names_fit.py [out.json]      default /mnt/project-files/battle-models/land/names-fit.json

For every vehicle whose file has a name place, each of her army's twenty invented names
(/mnt/project-files/historian/tank-names-b477.json) by the art director's rule (`brushhand.fit`): one line at the
place's letter height if it fits; two lines where the place has room and the name two words; else smaller, never under
9 cm; else the name is not for this tank. A name must fit every side that carries it. Soviet lines are given in
Russian letters. The Japanese tank is left out until her characters are drawn.
"""
import json
import sys

import bpy  # noqa: F401
sys.path.insert(0, __file__.rsplit("/", 1)[0])
import brushhand  # noqa: E402
import vehicles  # noqa: E402

NAMES = "/mnt/project-files/historian/tank-names-b477.json"
ARMY = {"USA": "USA", "UK": "UK", "GBR": "UK", "FRA": "FRA", "SOV": "SOV", "ITA": "ITA", "POL": "POL", "JAP": "JAP", "JPN": "JAP"}


def main(out):
    lists = json.load(open(NAMES))
    table = {"about": "Which invented names each tank can wear and how they are laid (lines, letter height in metres), by the rule in brush-alphabet.json `fit`. "
                      "`fits`: the names to choose from for this tank; `too_long`: never give her these.", "vehicles": {}}
    for name in vehicles.BUILDERS:
        m = vehicles.build(name, True, whole=True).m
        place = m.info.get("file", {}).get("name_place")
        if not place or place["kind"] != "name" or not place.get("ready", True):
            continue
        army = ARMY.get(m.info.get("tag"))
        if army not in lists:
            continue
        fits, too_long = {}, []
        for entry in lists[army]["names"]:
            latin = entry if isinstance(entry, str) else entry[0]
            text = (brushhand.russian(latin) if army == "SOV" else latin).upper()
            laid = [brushhand.fit(text, pl["long"], place["high"], pl.get("lines", place.get("lines", 1)), place.get("least", brushhand.LEAST), place.get("high_two")) for pl in place["places"]]
            if any(g is None for g in laid):
                too_long.append(latin)
                continue
            h, rows = min(laid, key=lambda g: g[0])                       # the same on both sides: the tighter side rules
            fits[latin] = {"paint": rows, "high": round(h, 3), **({"means": entry[1]} if not isinstance(entry, str) and entry[1] != entry[0] else {})}
        table["vehicles"][name] = {"army": army, "sides": [pl["side"] for pl in place["places"]], "fits": fits, "too_long": too_long}
        print("%-8s %s: %d fit (%d on two lines), %d too long %s" % (name, army, len(fits), sum(1 for f in fits.values() if len(f["paint"]) > 1), len(too_long), too_long))
    with open(out, "w") as f:
        json.dump(table, f, ensure_ascii=False, indent=1)
    print("wrote", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "/mnt/project-files/battle-models/land/names-fit.json")
