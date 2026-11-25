#!/usr/bin/env python3
"""The release 2 sea's look stand-ins (the release 2 ocean plan, step 4(d)): review material only, never a gate.

Until the rules send the dated ask's data (Tools/sea_waves/asks.txt: when.hour, light, sky, sea for every sea battle),
no recording carries an hour, a sky or a swell. These three are real recordings with only their field's look added, so
a Mac review can see a dawn, a gale and a swell on battles that were really fought by the show. Each is labelled a
stand-in in its "what" and its title. The views (what the ships did) are the recording's own, byte for byte.

  samar-dawn.json           the action off Samar at 6.8 h: the sun 6 deg up from 095, seen 20 km, broken cumulus at
                            0.50 (base 600 m, top 3,500 m) with rain squalls at 0.7, a 1.5 m swell at 11 s from 080
                            (its sky is the weather's, Tools/sea_waves/stand-ins/weather_days.py: the squalls the
                            escort carriers hid in are the day's point)
  north-atlantic-gale.json  the stand-in convoy night in a gale: wind 19 m/s, a deck at 0.9, rain 0.5, vis 3,000 m, a
                            4 m swell at 13 s running with the wind
  fleet-swell.json          light ships against battleships with a Mediterranean swell, 1.2 m at 9 s from 300

They live here, outside Tools/sea_show/stand-ins, so the sea show's own counts do not move.

    python3 -I Tools/sea_waves/stand-ins/make_stand_ins.py [OUT_DIR]   (default: beside this script; the cloud test
                                                                       writes them again elsewhere and compares)
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
OUT = HERE
LABEL = "a STAND-IN for review (the release 2 ocean plan, step 4(d)): a real recording with only its field's look added; "


def made(source, out, key, title, look, wind=None):
    with open(os.path.join(ROOT, source), encoding="utf-8") as f:
        rec = json.load(f)
    if not isinstance(rec, dict) or not isinstance(rec.get("field"), dict):
        sys.exit(f"{source}: not a recording with a field")
    rec["what"] = LABEL + "from " + source + ". " + str(rec.get("what", ""))
    rec["key"] = key
    field = rec["field"]
    field["title"] = title
    for name, value in look.items():
        field[name] = value
    if wind is not None:
        field["wind"] = wind
    with open(os.path.join(OUT, out), "w", encoding="utf-8", newline="\n") as f:
        json.dump(rec, f, separators=(",", ":"))
        f.write("\n")
    print(f"{out}: from {source}, the field's look: {', '.join(sorted(look))}{', wind' if wind else ''}")


def main():
    made("Content/Data/SeaBattles/samar.json", "samar-dawn.json", "samar-dawn",
         "The action off Samar at dawn (stand-in)",
         {"when": {"year": 1944, "month": 9, "hour": 6.8},
          "light": {"sun": {"up": 6, "from": 95}, "vis": 20000},
          "sky": {"cover": 0.50, "kind": "cumulus", "base": 600, "top": 3500, "rain": 0.7, "word": "broken cloud, rain squalls"},
          "sea": {"swell": {"height": 1.5, "period": 11, "from": 80}, "fetch": 900}})
    gale_from = 4.2  # (the stand-in night's own wind bearing, radians: the swell runs with it)
    made("Tools/sea_show/stand-ins/convoy-night.json", "north-atlantic-gale.json", "north-atlantic-gale",
         "A convoy at night in a gale (stand-in)",
         {"light": {"sun": -24, "moon": {"lit": 0.45, "up": 22, "from": 140}, "vis": 3000},
          "sky": {"cover": 0.9, "kind": "deck", "base": 450, "top": 3000, "rain": 0.5, "word": "gale"},
          "sea": {"swell": {"height": 4.0, "period": 13, "from": round(math.degrees(gale_from) % 360.0, 1)}, "fetch": 1500}},
         wind={"from": gale_from, "speed": 19.0})
    made("Content/Data/SeaBattles/light-ships.json", "fleet-swell.json", "fleet-swell",
         "Light ships against battleships, with a swell (stand-in)",
         {"sea": {"swell": {"height": 1.2, "period": 9, "from": 300}, "fetch": 400}})


if __name__ == "__main__":
    if len(sys.argv) > 2:
        sys.exit(__doc__)
    if len(sys.argv) == 2:
        OUT = os.path.abspath(sys.argv[1])
        os.makedirs(OUT, exist_ok=True)
    main()
