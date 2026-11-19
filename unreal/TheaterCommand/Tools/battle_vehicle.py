#!/usr/bin/env python3
"""
Makes a campaign model of a land vehicle from the vehicle modeller's battle model.

The battle models (the Library's battle-models/land/vehicles/<Name>.json, made by tools/battle-models) carry a close
mesh of about sixty thousand points, a far mesh of about six thousand, twenty-three paints, the camouflage's blotches
point by point (`blend`), wear and soot, the places of the owner's marks, and the parts, sockets and clips a battle
moves. The campaign's map stands many vehicles on its bases and moves none of their parts, so it takes the far mesh
alone, with its paints, blotches, wear and marks, and leaves the rest. (The art director, 19 November 2026: "use the
battle models' far meshes on the campaign map instead of the old miniatures".)

    python3 Tools/battle_vehicle.py <battle model .json> <Content/Models/Name.json> [--drop <part>]...

Nothing is changed in the mesh: the numbers are copied as they stand.

A model that carries two faces of one part, of which a battle shows one and scales the other to nothing (the KV-1's
welded and cast turrets, its `swaps`), is made into one campaign model for each face: `--drop turret_cast` leaves the
points of that part out and the triangles that used them. A mark tagged with the model's own nation is the owner's.

    python3 Tools/battle_vehicle.py KV1.json Content/Models/KV1.json --drop turret_cast
    python3 Tools/battle_vehicle.py KV1.json Content/Models/KV1Cast.json --drop turret_welded
"""
import json
import re
import sys

source, target = sys.argv[1:3]
drops, rest = [], sys.argv[3:]
while rest:
    flag = rest.pop(0)
    if flag != "--drop" or not rest:
        raise SystemExit(f"battle_vehicle.py: I do not know what to do with {flag}")
    drops.append(rest.pop(0))
with open(source, encoding="utf-8") as file:
    battle = json.load(file)
far = battle["far"]
count = len(far["paints"])
assert len(far["points"]) == count * 3 and len(far["facings"]) == count * 3 and len(far["triangles"]) % 3 == 0
if drops:
    # The far mesh numbers its parts by the model's list of parts with each track's separate links left out
    # (track_l.001 and so on, which only the close mesh carries).
    coarse = [part["name"] for part in battle["parts"] if not re.fullmatch(r"track_[lr]\.\d+", part["name"])]
    assert max(far["part"]) < len(coarse), "the far mesh's part numbers do not fit the list of parts without track links"
    unknown = [name for name in drops if name not in coarse]
    assert not unknown, f"no such part: {unknown}"
    gone = {number for number, name in enumerate(coarse) if name in drops}
    kept = [k for k in range(count) if far["part"][k] not in gone]
    assert len(kept) < count, f"the far mesh has no points of {drops}"
    renumber = {old: new for new, old in enumerate(kept)}
    for key, width in (("points", 3), ("facings", 3), ("paints", 1), ("blend", 1), ("soot", 1), ("wear", 1)):
        if key in far:
            far[key] = [far[key][k * width + j] for k in kept for j in range(width)]
    corners = far["triangles"]
    far["triangles"] = [renumber[corner] for t in range(0, len(corners), 3) if all(corner in renumber for corner in corners[t:t + 3]) for corner in corners[t:t + 3]]
    print(f"left out {drops}: {count - len(kept)} points and {(len(corners) - len(far['triangles'])) // 3} triangles")
    count = len(kept)
mesh = {key: far[key] for key in ("points", "facings", "paints", "triangles", "blend", "soot", "wear") if key in far}
model = {
    "name": battle["name"],
    "about": "Made by Tools/battle_vehicle.py from the vehicle modeller's battle model (its far mesh, unchanged). Metres; "
             "x forward, y right, z up. Paints: " + ", ".join(f"{index} {name}" for index, name in enumerate(battle["paint_names"]))
             + ". `blend` (0 to 100 a point) is how far a point of the coat is into the second coat colour; fill upper with "
             "the nation's paint and upper2 with its second colour. Marks tagged _ are the owner's."
             + (" Left out: " + ", ".join(drops) + "." if drops else ""),
    "kind": "battle-vehicle",
    "was": battle.get("kind", ""),
    "long": battle["long"], "wide": battle["wide"], "high": battle["high"],
    "colours": battle["colours"], "paint_names": battle["paint_names"],
    "marks": [{key: ("_" if key == "tag" and mark[key] == battle.get("nation") else mark[key]) for key in ("tag", "part", "at", "facing", "radius") if key in mark}
              for mark in battle.get("marks", [])],
    "near": mesh, "far": mesh,
}
with open(target, "w", encoding="utf-8") as file:
    json.dump(model, file, separators=(",", ":"))
print(f"{target}: {count} points, {len(far['triangles']) // 3} triangles, {len(model['marks'])} marks, {battle['long']} m long")
