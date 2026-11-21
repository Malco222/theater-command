"""make_crew_heads.py <soldier folder> [out.json]: the airmen's heads, taken from the battle soldiers' (Brian, 2026-11-21:
"can we make their faces more realistic?", "maybe use the current soldier faces as a template?").

Each army's soldier (battle-models/land/soldier/soldier_<ARMY>.json, made by Tools/battle_units/soldier.py from
MakeHuman's CC0 man, and given eight faces by variety.py) lends his head: the near mesh's faces that ride his head bone
alone, less his helmet and its chin strap. Written in his head bone's frame (x ahead, y to his LEFT, as the aircraft
are, z up; metres), each face as the points it moves from face 0. crewface.py fits a flying helmet, goggles and a mask
on them."""
import json
import sys

ARMIES = ("GER", "POL", "SOV", "ITA", "JAP")


def head_of(path):
    d = json.load(open(path))
    n = d["near"]
    P, T, part, paint = n["points"], n["triangles"], n["part"], n["paints"]
    hf, hc = d["helmet_tris"]["near"]
    hb = next(b for b in d["bones"] if b["name"] == "head")["head"]
    tri = []
    for t in range(len(T) // 3):
        ids = T[3 * t:3 * t + 3]
        if hf <= t < hf + hc or not all(part[i] == 6 for i in ids) or any(paint[i] == 4 for i in ids):
            continue                                     # (his helmet, its chin strap, and anything not his head)
        tri.append(ids)
    keep = sorted({i for t in tri for i in t})
    at = {v: j for j, v in enumerate(keep)}

    def local(p):                                        # (his head bone's frame; y to his left)
        return [round(p[0] - hb[0], 4), round(-(p[1] - hb[1]), 4), round(p[2] - hb[2], 4)]

    base = [local(P[3 * v:3 * v + 3]) for v in keep]
    faces = []
    for f in d["faces"]:
        fn = f["near"]
        moved = {}
        mv = fn.get("move", [])
        for j, i in enumerate(fn.get("at", [])):
            if i in at:
                m = mv[j] if isinstance(mv[j], list) else mv[3 * j:3 * j + 3]
                if max(abs(c) for c in m) > 5e-5:
                    moved[at[i]] = [round(m[0], 4), round(-m[1], 4), round(m[2], 4)]
        faces.append({"name": f["name"], "move": moved})
    return {"from": path.split("/")[-1], "points": base,
            "triangles": [[at[t[0]], at[t[2]], at[t[1]]] for t in tri],          # (turned: y is mirrored)
            "eye": [int(any(paint[i] == 2 for i in t)) for t in tri],
            "faces": faces}


def main():
    folder = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "crew_heads.json"
    heads = {a: head_of(f"{folder}/soldier_{a}.json") for a in ARMIES}
    json.dump({"about": __doc__.strip(), "heads": heads}, open(out, "w"), separators=(",", ":"))
    for a, h in heads.items():
        print(a, len(h["points"]), "points", len(h["triangles"]), "triangles", sum(len(f["move"]) for f in h["faces"]), "moves")


if __name__ == "__main__":
    main()
