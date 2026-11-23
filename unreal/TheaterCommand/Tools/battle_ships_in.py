#!/usr/bin/env python3
"""Bring the battle modeller's ship models (tools/battle-models, kind "ship") into the game.

Each named file is read from the shared battle-models folder and written to Content/Models/Ships/<name>.json, slimmed:
the camouflage blend and the modeller's clips are left out (the stage moves its ships itself), and the numbers are
written without spaces. Everything AAirStage and FMiniatureMesh::Sculpted read is kept: colours, paint_names, near and
far (points, facings, paints, triangles, wear, soot, salt, part), parts, sockets, and the sizes. The sockets are also
written alone to <name>.sockets.json, for the stage (AirStage StageParts::SocketsOf).

    python3 -I Tools/battle_ships_in.py /mnt/project-files/battle-models/sea Submarine Destroyer_GER Freighter ...
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "Content", "Models", "Ships")


def slim(model):
    if model.get("kind") != "ship":
        raise SystemExit("not a ship model: kind %r" % model.get("kind"))
    model.pop("clips", None)
    for zoom in ("near", "far"):
        model[zoom].pop("blend", None)
    return model


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    source = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    for name in sys.argv[2:]:
        with open(os.path.join(source, name + ".json")) as f:
            model = slim(json.load(f))
        path = os.path.join(OUT, name + ".json")
        with open(path, "w") as f:
            json.dump(model, f, separators=(",", ":"))
        # (the sockets alone beside it, so that the stage finds a funnel or a periscope without reading the whole model again)
        with open(os.path.join(OUT, name + ".sockets.json"), "w") as f:
            json.dump({"name": model.get("name"), "sockets": model.get("sockets", [])}, f, separators=(",", ":"))
        near, far = model["near"], model["far"]
        print("%-16s %5.2f MB  near %6d triangles  far %6d" % (name, os.path.getsize(path) / 1e6, len(near["triangles"]) // 3, len(far["triangles"]) // 3))


if __name__ == "__main__":
    main()
