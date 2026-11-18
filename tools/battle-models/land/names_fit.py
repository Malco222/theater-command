"""Which of the historian's names each tank can wear, and how (Brian's pick b477).

    python names_fit.py [out.json]      default /mnt/project-files/battle-models/land/names-fit.json

For every vehicle whose file has a name place, each of her army's twenty invented names
(/mnt/project-files/historian/tank-names-b477.json) by the art director's rule (`brushhand.fit`): one line at the
place's letter height if it fits; two lines where the place has room and the name two words; else smaller, never under
9 cm; else the name is not for this tank. A name must fit every side that carries it. Soviet lines are given in
Russian letters. The Japanese tank wears characters: only the names whose characters are drawn fit her
(`not_drawn` lists the rest). A tank that another army drives too (`ALSO`) has that army's names under `also`.
"""
import json
import sys

import bpy  # noqa: F401
sys.path.insert(0, __file__.rsplit("/", 1)[0])
import brushhand  # noqa: E402
import vehicles  # noqa: E402

NAMES = "/mnt/project-files/historian/tank-names-b477.json"
ARMY = {"USA": "USA", "UK": "UK", "GBR": "UK", "FRA": "FRA", "SOV": "SOV", "ITA": "ITA", "POL": "POL", "JAP": "JAP", "JPN": "JAP"}
ALSO = {"Sherman": ["UK"], "M10": ["UK"], "M7": ["UK"]}        # Britain drove these too: her crews paint her own names on them


def fits_of(place, entries, army):
    """(fits, too_long) for one army's list of names on one tank's place."""
    fits, too_long = {}, []
    for entry in entries:
        latin = entry if isinstance(entry, str) else entry[0]
        if place.get("script") == "japanese":
            text = brushhand.japanese(latin)
            if text is None:
                continue                                    # (no characters drawn for this one: see `not_drawn`)
            laid = [brushhand.fit_down(text, pl["long"], place["high"], place.get("least", brushhand.LEAST)) for pl in place["places"]]
        else:
            text = (brushhand.russian(latin) if army == "SOV" else latin).upper()
            laid = [brushhand.fit(text, pl["long"], place["high"], pl.get("lines", place.get("lines", 1)), place.get("least", brushhand.LEAST), place.get("high_two")) for pl in place["places"]]
        if any(g is None for g in laid):
            too_long.append(latin)
            continue
        h, rows = min(laid, key=lambda g: g[0])                       # the same on both sides: the tighter side rules
        fits[latin] = {"paint": rows, "high": round(h, 3), **({"means": entry[1]} if not isinstance(entry, str) and entry[1] != entry[0] else {})}
    return fits, too_long


def main(out):
    lists = json.load(open(NAMES))
    table = {"about": "Which invented names each tank can wear and how they are laid (lines, letter height in metres), by the rule in brush-alphabet.json `fit`. "
                      "`fits`: the names to choose from for this tank; `too_long`: never give her these. `also`: the same for another army that drives her "
                      "(a British Sherman wears a British name). The Japanese tank's `paint` is her characters, one under another; `not_drawn`: names whose "
                      "characters are not drawn, never give her these either.", "vehicles": {}}
    for name in vehicles.BUILDERS:
        m = vehicles.build(name, True, whole=True).m
        place = m.info.get("file", {}).get("name_place")
        if not place or place["kind"] != "name" or not place.get("ready", True):
            continue
        army = ARMY.get(m.info.get("tag"))
        if army not in lists:
            continue
        fits, too_long = fits_of(place, lists[army]["names"], army)
        table["vehicles"][name] = {"army": army, "sides": [pl["side"] for pl in place["places"]], "fits": fits, "too_long": too_long}
        if place.get("script") == "japanese":
            table["vehicles"][name]["not_drawn"] = [e if isinstance(e, str) else e[0] for e in lists[army]["names"] if brushhand.japanese(e if isinstance(e, str) else e[0]) is None]
        print("%-8s %s: %d fit (%d on two lines), %d too long %s" % (name, army, len(fits), sum(1 for f in fits.values() if len(f["paint"]) > 1), len(too_long), too_long))
        for other in ALSO.get(name, []):
            f2, t2 = fits_of(place, lists[other]["names"], other)
            table["vehicles"][name].setdefault("also", {})[other] = {"fits": f2, "too_long": t2}
            print("%-8s   also %s: %d fit (%d on two lines), %d too long %s" % (name, other, len(f2), sum(1 for f in f2.values() if len(f["paint"]) > 1), len(t2), t2))
    with open(out, "w") as f:
        json.dump(table, f, ensure_ascii=False, indent=1)
    print("wrote", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "/mnt/project-files/battle-models/land/names-fit.json")
