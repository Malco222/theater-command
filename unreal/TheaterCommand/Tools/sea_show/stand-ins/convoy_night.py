#!/usr/bin/env python3
"""A stand-in convoy night for the sea show's cloud test (ruling 241, the submarine scene D1), in the recording's names
(naval-battles/wolfpack-r2/form.md section 5, field-side-read.md section 3). Hand-made, not the battle's: it exercises
every new name (boats surfaced, at periscope depth and deep; a boat's wakeless salvo; asdic; a pattern of depth charges
and an ahead-thrown salvo; a hit, a boat crushed and one forced up and scuttled; a star shell; a sighting report; a
rescue) until the production lead's staged recording lands.

    python3 Tools/sea_show/stand-ins/convoy_night.py   (writes convoy-night.json beside it)
"""
import json
import math
import os

STEP = 2.0
END = 620.0

# Pieces: id -> (side, cls, name, nat, path [(t, x, y)], extra)
PIECES = {}


def piece(pid, side, cls, name, nat, path, **extra):
    PIECES[pid] = dict(side=side, cls=cls, name=name, nat=nat, path=path, extra=extra)


# The convoy: twelve merchants in four columns 900 m apart, 550 m between ships, north at 7 knots (3.6 m/s).
V = 3.6
for c in range(4):
    for r in range(3):
        pid = 1 + c * 3 + r
        x = 10000 + (c - 1.5) * 900
        y = 6000 - r * 550
        piece(pid, "def", "mv", f"Merchant {pid}", "UK", [(0, x, y), (END, x, y + V * END)], column=c)
# The escort: one ahead, one on each bow.
piece(20, "def", "dd", "Escort 1", "UK", [(0, 10000, 7800), (300, 10000, 7800 + V * 300), (520, 8700, 7100), (600, 8690, 7080)])
piece(21, "def", "dd", "Escort 2", "UK", [(0, 8200, 7200), (280, 8200, 7200 + V * 280), (340, 8640, 8540), (365, 8840, 8700), (420, 8700, 8950),
                                          (470, 8530, 8700), (500, 8300, 8450), (620, 7600, 8000)])
piece(22, "def", "dd", "Escort 3", "UK", [(0, 12000, 5800), (100, 12000, 5800 + V * 100), (330, 12460, 9300), (360, 12560, 9550), (420, 12400, 9700),
                                          (620, 11800, 9500)])
# The boats: A closing on the surface, driven down by the star shell, hunted, forced up and scuttled; B at periscope depth,
# fires a wakeless salvo, goes deep, and is crushed by the ahead-thrown salvo.
A0 = (12600, 9800)
def along(p, hdg_deg, dist):
    h = math.radians(hdg_deg)
    return (p[0] + math.sin(h) * dist, p[1] + math.cos(h) * dist)
A1 = along(A0, 225, 7 * 65)
A2 = along(A1, 225, 3 * 85)
A3 = along(A2, 90, 2 * 230)
A4 = along(A3, 90, 1 * 70)
piece(30, "att", "ss", "Boat A", "GER", [(0, *A0), (65, *A1), (150, *A2), (380, *A3), (450, *A4), (620, *A4)])
B0 = (8500, 9200)
B1 = along(B0, 160, 2.5 * 285)
B2 = along(B1, 270, 2 * 196)
piece(31, "att", "ss", "Boat B", "GER", [(0, *B0), (285, *B1), (481, *B2), (620, *B2)])


def place(path, t):
    for (t0, x0, y0), (t1, x1, y1) in zip(path, path[1:]):
        if t0 <= t <= t1:
            k = (t - t0) / (t1 - t0) if t1 > t0 else 0.0
            return x0 + (x1 - x0) * k, y0 + (y1 - y0) * k, x1 - x0, y1 - y0, t1 - t0
    t0, x0, y0 = path[-1]
    return x0, y0, 0.0, 0.0, 1.0


SUNK = {1: 272.0, 31: 481.0, 30: 450.0}
DIVES = {  # piece -> [(from t, state, keel or None, scope)]
    30: [(0, "surfaced", None, False), (65, "periscope", None, True), (150, "deep", 80, False), (360, "surfaced", None, False)],
    31: [(0, "periscope", None, True), (285, "deep", 80, False)],
}
STR = {30: [(0, 1.0), (330, 0.5), (450, 0.0)], 31: [(0, 1.0), (481, 0.0)], 1: [(0, 1.0), (272, 0.0)]}


def at(table, t, default):
    v = default
    for row in table:
        if row[0] <= t:
            v = row
    return v


def unit(pid, t, first):
    p = PIECES[pid]
    x, y, dx, dy, dt = place(p["path"], t)
    gone = pid in SUNK and t >= SUNK[pid]
    speed = 0.0 if gone else math.hypot(dx, dy) / dt
    face = math.atan2(dx, dy) if (dx or dy) else 0.0
    strength = at(STR.get(pid, []), t, (0, 1.0))[1]
    ships = 0 if gone else 1
    u = {"id": pid, "x": round(x, 1), "y": round(y, 1), "face": round(face, 4), "str": strength, "mor": 1, "state": "gone" if gone else "steady",
         "figures": ships, "d": 0, "act": "march", "speed": round(speed, 2), "engaged": -1,
         "extra": {"ships": ships, "sunk": 1 - ships}}
    if pid == 22 and 438 <= t < 450:
        u["engaged"] = 30  # (the boat forced up: her hunter's guns on her as her crew get out)
    if first:
        u.update({"side": p["side"], "kind": p["cls"], "name": p["name"], "nat": p["nat"], "figures0": 1})
        u["extra"].update({"cls": p["cls"], "ships0": 1, "fleet": "Convoy HX 0" if p["side"] == "def" else "Pack"})
    u["extra"].update({k: v for k, v in p["extra"].items()})
    if pid in DIVES:
        _, state, keel, scope = at(DIVES[pid], t, DIVES[pid][0])
        u["extra"]["dive"] = state
        u["extra"]["scope"] = scope
        u["extra"]["keel"] = keel if keel is not None else {"surfaced": 4.7, "periscope": 13, "deep": 80}[state]
        # (Boat A's depth told as it changes, a view at a time, going deep at a metre a second: the pre-check's what-if)
        if pid == 30 and 150 <= t < 217:
            u["extra"]["keel"] = round(13 + (t - 150), 1)
    return u


def ev(t, what, unit_id, side, text, cue="", here=None, **extra):
    e = {"t": t, "what": what, "unit": unit_id, "side": side, "text": text, "extra": extra}
    if cue:
        e["cue"] = cue
    if here is not None:
        e["x"], e["y"] = round(here[0], 1), round(here[1], 1)
    for k in ("x", "y"):
        if k in extra:
            extra[k] = round(extra[k], 1)
    return e


def pos(pid, t):
    x, y, *_ = place(PIECES[pid]["path"], t)
    return x, y


EVENTS = [
    ev(20, "sea:report", 30, "att", "A U-boat reports the convoy.", at=1),
    ev(60, "sea:starshell", 20, "def", "Escort 1 fires star shell.", here=pos(30, 60), by=20),
    ev(65, "sea:dive", 30, "att", "Boat A dives.", dive="periscope", why="radar"),
    ev(120, "sea:torpedo", 31, "att", "Boat B fires a salvo at the convoy.", "torpedo_launch", here=pos(31, 120), target=1, x=pos(1, 270)[0], y=pos(1, 270)[1],
       running=150, fish=4, spread=10, wakeless=True, dive="periscope"),
    ev(150, "sea:dive", 30, "att", "", dive="deep", keel=80, why="evade"),
    ev(270, "sea:torpedoHit", 1, "att", "A torpedo hits Merchant 1.", "torpedo_hit", here=pos(1, 270), **{"from": 31}),
    ev(272, "sea:sunk", 1, "def", "Merchant 1 breaks in two and goes down.", "ship_sinking", here=pos(1, 272), cls="mv", how="break"),
    ev(280, "sea:sonar", 21, "def", "Escort 2 has a contact.", on=31, kind="asdic", range=420, bearing=1.1),
    ev(285, "sea:dive", 31, "att", "Boat B goes deep.", dive="deep", keel=80, why="evade"),
    ev(330, "sea:depthCharge", 22, "def", "Escort 3 drops a pattern on Boat A.", here=pos(22, 330), on=30, x=pos(30, 330)[0], y=pos(30, 330)[1], n=10, set=70,
       sinking=round(70 / 3.0, 1), spread=120, kind="pattern"),
    ev(340, "sea:depthCharge", 21, "def", "Escort 2 drops a pattern on Boat B.", here=pos(21, 340), on=31, x=pos(31, 340)[0], y=pos(31, 340)[1], n=10, set=50,
       sinking=round(50 / 3.0, 1), spread=120, kind="pattern"),
    # (Boat A's hit told at the moment of the drop, Boat B's ten seconds after the bombs went: the pre-check's what-if)
    ev(330, "sea:depthChargeHit", 30, "def", "Boat A is damaged.", here=pos(30, 330), harm=0.5, keel=80, **{"from": 22}),
    ev(360, "sea:dive", 30, "att", "Boat A is forced to the surface.", dive="surfaced", why="forced"),
    ev(400, "sea:sonar", 21, "def", "", on=31, kind="asdic", lost="close"),
    ev(470, "sea:depthCharge", 21, "def", "Escort 2 fires ahead at Boat B.", here=pos(21, 470), on=31, x=pos(31, 470)[0], y=pos(31, 470)[1], n=24, kind="ahead"),
    ev(480, "sea:depthChargeHit", 31, "def", "Boat B is hit.", here=pos(31, 480), harm=1, keel=80, **{"from": 21}),
    ev(450, "sea:sunk", 30, "att", "Boat A is scuttled by her crew.", "ship_sinking", here=pos(30, 450), cls="ss", how="scuttled"),
    ev(481, "sea:sunk", 31, "att", "Boat B is lost with all hands.", here=pos(31, 481), cls="ss", how="crushed"),
    ev(520, "sea:rescue", 20, "def", "Escort 1 picks up survivors.", here=pos(20, 520), on=1),
]
EVENTS.sort(key=lambda e: e["t"])


def main():
    views = []
    t = 0.0
    told = 0
    while t <= END + 1e-9:
        first = not views
        v = {"t": t, "limit": END, "speed": 1, "me": "def", "contact": 0, "balance": {"att": 0.5},
             "units": [unit(pid, t, first) for pid in sorted(PIECES)],
             "events": []}
        while told < len(EVENTS) and EVENTS[told]["t"] <= t + 1e-9:
            v["events"].append(EVENTS[told])
            told += 1
        if t >= END - 1e-9:
            v["over"] = {"winner": "def", "level": "Victory", "lines": ["The convoy holds its course.", "One merchant lost; two boats sunk."]}
        views.append(v)
        t += STEP
    rec = {
        "what": "a stand-in convoy night (hand-made, not the battle's)", "recording": 1, "key": "convoy-night", "seed": 241, "step": STEP,
        "field": {
            "size": 20000, "edge": 5000, "zoneName": "Western Approaches", "title": "A convoy at night (stand-in)", "date": "Practice",
            "when": {"year": 1942, "month": 11}, "lat": 55, "lon": -20,
            "att": {"nat": "GER", "adj": "German"}, "def": {"nat": "UK", "adj": "British"},
            "classes": {
                "dd": {"len": 95, "beam": 10, "speed": 12, "turn": 0.12, "reach": 4000},
                "ss": {"len": 67, "beam": 6.2, "speed": 7, "turn": 0.1, "reach": 3000},
                "mv": {"len": 135, "beam": 17.5, "speed": 3.6, "turn": 0.05, "reach": 0},
            },
            "torp": {"travel": 150, "radius": 400}, "strike": {"windup": 90, "radius": 1500},
            "wind": {"from": 4.2, "speed": 8.0},
            "light": {"sun": -24, "moon": {"lit": 0.45, "up": 22, "from": 140}, "vis": 2500},
            "subs": {"scope": 13, "deep": 80, "crush": 250, "dived": 3.9},
            "dc": {"sink": 3.0, "lethal": 6.0},
        },
        "views": views,
        "after": {"lines": ["The convoy holds its course."], "level": "Victory", "winner": "def", "seconds": END},
    }
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "convoy-night.json")
    with open(out, "w") as f:
        json.dump(rec, f, separators=(",", ":"))
    print(out, len(views), "views,", len(EVENTS), "events")


if __name__ == "__main__":
    main()
