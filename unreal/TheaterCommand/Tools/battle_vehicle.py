#!/usr/bin/env python3
"""
Makes a campaign model of a land vehicle from the vehicle modeller's battle model.

The battle models (the Library's battle-models/land/vehicles/<Name>.json, made by tools/battle-models) carry a close
mesh of about sixty thousand points, a far mesh of about six thousand, twenty-three paints, the camouflage's blotches
point by point (`blend`), wear and soot, the places of the owner's marks, and the parts, sockets and clips a battle
moves. The campaign's map stands many vehicles on its bases and moves none of their parts, so it takes the far mesh
alone, with its paints, blotches, wear and marks, and leaves the rest. (The art director, 19 November 2026: "use the
battle models' far meshes on the campaign map instead of the old miniatures".)

    python3 Tools/battle_vehicle.py <battle model .json> <Content/Models/Name.json>

Nothing is changed in the mesh: the numbers are copied as they stand.
"""
import json
import sys

source, target = sys.argv[1:3]
with open(source, encoding="utf-8") as file:
    battle = json.load(file)
far = battle["far"]
count = len(far["paints"])
assert len(far["points"]) == count * 3 and len(far["facings"]) == count * 3 and len(far["triangles"]) % 3 == 0
mesh = {key: far[key] for key in ("points", "facings", "paints", "triangles", "blend", "soot", "wear") if key in far}
model = {
    "name": battle["name"],
    "about": "Made by Tools/battle_vehicle.py from the vehicle modeller's battle model (its far mesh, unchanged). Metres; "
             "x forward, y right, z up. Paints: " + ", ".join(f"{index} {name}" for index, name in enumerate(battle["paint_names"]))
             + ". `blend` (0 to 100 a point) is how far a point of the coat is into the second coat colour; fill upper with "
             "the nation's paint and upper2 with its second colour. Marks tagged _ are the owner's.",
    "kind": "battle-vehicle",
    "was": battle.get("kind", ""),
    "long": battle["long"], "wide": battle["wide"], "high": battle["high"],
    "colours": battle["colours"], "paint_names": battle["paint_names"],
    "marks": [{key: mark[key] for key in ("tag", "part", "at", "facing", "radius") if key in mark} for mark in battle["marks"]],
    "near": mesh, "far": mesh,
}
with open(target, "w", encoding="utf-8") as file:
    json.dump(model, file, separators=(",", ":"))
print(f"{target}: {count} points, {len(far['triangles']) // 3} triangles, {len(model['marks'])} marks, {battle['long']} m long")
