#!/usr/bin/env python3
"""The decks the gale's freeboard tests use, measured again from the ship models (the release 2 ocean plan, step 14(c)).

Battle/FleetRideGale.h holds, for each class the gale is tested against, her deck heights over her waterline at rest: her
forecastle deck at 0.45 of her length forward of her middle on her middle line, her weather deck at her side 0.1 of her
length abaft her middle, and her quarterdeck at her side 0.4 abaft it. This reads them again out of the modeller's own
files (Content/Models/Ships/<Model>.json, metres, x forward and z up from her waterline: the lowest point of a deck surface
there, paints deck, planks or flight) and fails if one has moved by over 5 cm, or if a model's length or beam no longer
matches the row's. A row with no model of its own (a corvette's and a frigate's: the stage draws every escort with
Destroyer*.json today) is listed and not measured.

Usage: python3 -I Tools/sea_waves/gale_decks.py [--list]
"""

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
HEADER = os.path.join(ROOT, "Source", "TheaterCommand", "Battle", "FleetRideGale.h")
MODELS = os.path.join(ROOT, "Content", "Models", "Ships")
HULL_PARTS = ("ship", "fore", "aft")
DECK_PAINTS = (2, 3, 4)  # deck, planks, flight
NEAR = 5e-2  # metres a deck may differ from the row

ROW = re.compile(
    r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*'
    r"([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*"
    r"([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*(true|false)\s*,\s*([0-9.]+)\s*\}"
)


def rows():
    """The Decks table of FleetRideGale.h: (name, model, long, beam, speed, bow, waist, stern, waist out, stern out, boat).
    (Each row's last number, her trim as the game draws her, is gale_test's to check: it runs the convoy looks' StepBoat.)"""
    text = open(HEADER, encoding="utf-8").read()
    start = text.find("Deck Decks[] = {")
    end = text.find("};", start)
    if start < 0 or end < 0:
        raise SystemExit("gale_decks: FleetRideGale.h's Decks table not found")
    out = []
    for m in ROW.finditer(text[start:end]):
        out.append((m.group(1), m.group(2)) + tuple(float(m.group(k)) for k in range(3, 11)) + (m.group(11) == "true",))
    return out


def deck_at(model, along, window):
    """The lowest deck surface at `along` metres forward of her middle, within `window` either way (metres, over her
    waterline), and how many of the model's points were taken there."""
    near = model["near"]
    points, paints, part = near["points"], near["paints"], near["part"]
    names = [p["name"] for p in model["parts"]]
    best, count = None, 0
    for v in range(len(points) // 3):
        x, _y, z = points[3 * v : 3 * v + 3]
        if names[part[v]] not in HULL_PARTS or paints[v] not in DECK_PAINTS:
            continue
        if abs(x - along) > window:
            continue
        count += 1
        if best is None or z < best:
            best = z
    return best, count


def main():
    listing = "--list" in sys.argv[1:]
    checked, failed, skipped = 0, 0, 0
    for name, model, long, beam, speed, bow, waist, stern, waist_out, stern_out, boat in rows():
        if not model:
            skipped += 1
            print("  note  %s: no model of her own yet (her decks are estimates; the stage draws her as a destroyer today)" % name)
            continue
        path = os.path.join(MODELS, model + ".json")
        if not os.path.exists(path):
            failed += 1
            print("  FAIL  %s: %s.json is not in Content/Models/Ships" % (name, model))
            continue
        d = json.load(open(path, encoding="utf-8"))
        window = 0.02 * d["length"]
        got = {}
        for which, at, want in (("bow", 0.45, bow), ("waist", -0.1, waist), ("stern", -0.4, stern)):
            z, count = deck_at(d, at * d["length"], window)
            got[which] = (z, count, want)
        size_ok = abs(d["length"] - long) <= 0.1 and abs(d["beam"] - beam) <= 0.1
        line = "%s (%s.json, %.1f m x %.1f m, draught %.1f m, her waterline %s): " % (name, model, d["length"], d["beam"], d["draught"], d["waterline"])
        parts = []
        ok = size_ok
        for which in ("bow", "waist", "stern"):
            z, count, want = got[which]
            near = z is not None and count > 3 and abs(z - want) <= NEAR
            ok = ok and near
            parts.append("%s %s against the row's %.3f m%s" % (which, "none" if z is None else "%.3f m" % z, want, "" if near else " <-- MOVED"))
        checked += 1
        failed += 0 if ok else 1
        print("  %s  %s%s%s" % ("ok  " if ok else "FAIL", line, "; ".join(parts), "" if size_ok else "; her length or beam has moved"))
        if boat and listing:
            print("        (a boat: her casing, where her men stand, is the deck here)")
    print("gale_decks: %d models measured, %d skipped (no model of her own), %d moved" % (checked, skipped, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
