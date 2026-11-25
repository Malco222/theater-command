#!/usr/bin/env python3
"""A stand-in weather day for the fleet's clouds and weather (the release 2 ocean plan, step 13), in the recording's
own names (Battle/SeaFeed.cpp's field, naval-battles/wolfpack-r2/form.md section 5).

Nothing here is a fought action: each takes a recording the project already has and rewrites its field's weather, so a
Mac can play a storm sky and a squall day before the rules thread sends a field sky of its own. The pieces, their
tracks, the views and the seed are the recording's; only field.light, field.wind, field.sky (and the day's title, date
and key) are this script's, and in the gale the one star shell is dropped, a daylight action having none.

  north-atlantic-deck.json  a winter North Atlantic convoy fight by day under a closed deck: cover 0.95, base 450 m, top
                            2,800 m, rain 0.6, wind 13.5 m/s from the west-south-west. From the sea show's stand-in
                            convoy night (Tools/sea_show/stand-ins/convoy-night.json, ruling 241), by day.
  (The squall dawn off Samar, samar-dawn.json, is make_stand_ins.py's, with this day's squall sky; the night gale,
  north-atlantic-gale.json, is make_stand_ins.py's too, at 19 m/s.)

The figures:
  - North Atlantic, January: the mean cloud cover over the ocean north of 50 N in winter is nine tenths and over, and
    the sky in a frontal rainband is a closed deck; nimbostratus over the winter North Atlantic sits with its base at
    300-600 m and its top at 2-4 km [EST within those bands: 450 m and 2,800 m]. 13.5 m/s is the task's own wind: force
    6, the top of a strong breeze, a sea of about 4 m - a hard convoy day, short of the storm that stops a fight.
  - Sunrise at 55 N, 20 W in mid-January is about 0830 GMT; the sun stands 8.5 degrees up by about 1000 and no higher
    than 13 degrees all day, so 8.5 is a true winter forenoon [EST for the hour]. Seeing 4,000 m is the rain's.
  - Samar, 25 October 1944: the action opened at about 0645 local with sunrise near 0615, so the sun stood some 6
    degrees up [EST from the hour]; the Philippine Sea that morning was broken cumulus with heavy rain squalls, which
    the escort carriers and their destroyers used for cover - the reason the squall is this day's point. Seeing is the
    open tropical sea's, 20 km.

    python3 -I Tools/sea_waves/stand-ins/weather_days.py [out dir]   (writes it beside itself, or where it is told)
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))


def load(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
        return json.load(f)


def save(where, name, day):
    path = os.path.join(where, name)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(day, f, separators=(",", ":"))
    print("%s: %d views, %.1f MB" % (name, len(day["views"]), os.path.getsize(path) / 1048576.0))


def gale():
    day = load("Tools/sea_show/stand-ins/convoy-night.json")
    day["what"] = ("a stand-in North Atlantic overcast (hand-made, not the battle's): the stand-in convoy night's own "
                   "pieces and tracks by day under a closed deck, for the fleet's clouds and weather "
                   "(Tools/sea_waves/stand-ins/weather_days.py, the release 2 ocean plan step 13)")
    day["key"] = "north-atlantic-deck"
    f = day["field"]
    f["title"] = "A convoy under a winter overcast (stand-in)"
    f["zoneName"] = "Western Approaches"
    f["when"] = {"year": 1943, "month": 1}
    # The wind: the night's own bearing (4.2 rad, west-south-west), at the task's 13.5 m/s.
    f["wind"] = {"from": 4.2, "speed": 13.5}
    # Daylight: a winter forenoon, the rain cutting the seeing to 4 km. No moon by day.
    f["light"] = {"sun": 8.5, "vis": 4000}
    f["sky"] = {"kind": "deck", "cover": 0.95, "base": 450, "top": 2800, "rain": 0.6,
                "word": "overcast, rain"}
    # A daylight action fires no star shell.
    dropped = 0
    for view in day["views"]:
        events = view.get("events")
        if not events:
            continue
        kept = [e for e in events if e.get("what") != "sea:starshell"]
        dropped += len(events) - len(kept)
        view["events"] = kept
    print("north-atlantic-deck: %d star shell(s) dropped" % dropped)
    return day




def main():
    # (beside this script, or into a directory named on the line: the cloud runner writes them again there and holds
    # them to the committed two, byte for byte)
    where = sys.argv[1] if len(sys.argv) > 1 else HERE
    os.makedirs(where, exist_ok=True)
    save(where, "north-atlantic-deck.json", gale())
    print("weather_days: play it with -TheaterSeaBattle=Tools/sea_waves/stand-ins/north-atlantic-deck.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
