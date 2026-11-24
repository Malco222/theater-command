#!/usr/bin/env python3
"""A stand-in night of two boats that strike, for the sea show's and the convoy looks' cloud tests (sea:surrender, the
naval rules' proposed event behind their subSurrender switch), in the recording's names. Hand-made, not the battle's:
an escorted convoy in the Eastern Mediterranean, July 1942, under a bright moon and a light wind. Boat A (German) is
found, dives, goes deep, is damaged by a pattern and forced up; Escort 1's guns open on her and she strikes, her crew
come out on her casing and leave her, and they scuttle her; Escort 1 lies stopped to windward of her men and picks them
up. Boat B (Italian) is found at periscope depth, goes deep, is hit by Escort 3's ahead-thrown salvo and forced up; she
strikes to Escort 3, which lies stopped near her, sends its whaler across with a boarding party and takes her (as boats
were taken in the war), and picks up her men. A star shell over Boat A, a pattern with bursts the battle did not book,
an ahead salvo with its hit booked, and two rescues whose men climb the escorts' nets.

Two more nights from the same: wolf-pack-abeam, the same but Escort 3 lies stopped abeam of Boat B on her weather side,
so that her whaler, bound for Boat B's lee side, must pull round her bow or her stern; and wolf-pack-abandon, where neither
boat strikes: Boat A, forced up, is abandoned and scuttled by her crew (sea:abandon, sea:men, sea:sunk, no sea:surrender),
and Boat B, forced up, dives again and gets away.

    python3 Tools/sea_show/stand-ins/wolf_pack_surrender.py   (writes wolf-pack-surrender.json, wolf-pack-abeam.json and
                                                               wolf-pack-abandon.json beside it)

The contract this night writes, for the rules thread: a boat strikes on the surface (forced up first where she was under);
from the view that tells sea:surrender her piece is out of the fight (ships 0; not sunk until sea:sunk says so); the guns
engaged on her and any view's gun on her end at it; sea:surrender {unit, ship, how: "scuttled" | "captured", by}; her
sea:abandon and sea:men after it find her; sea:sunk how "scuttled" sends her down; one "captured" has no sea:sunk.
"""
import json
import math
import os

STEP = 2.0
END = 600.0

PIECES = {}


def piece(pid, side, cls, name, nat, path, **extra):
    PIECES[pid] = dict(side=side, cls=cls, name=name, nat=nat, path=path, extra=extra)


def along(p, hdg_deg, dist):
    h = math.radians(hdg_deg)
    return (p[0] + math.sin(h) * dist, p[1] + math.cos(h) * dist)


# The wind from 300 degrees at 4.5 m/s: a boat's crew go over her weather side, and her rescuer lies to windward of them.
WIND_FROM = 300.0
# The convoy: four merchants in two columns 900 m apart, 550 m between ships, north at 7 knots (3.6 m/s).
V = 3.6
for c in range(2):
    for r in range(2):
        pid = 1 + c * 2 + r
        x = 10000 + (c - 0.5) * 900
        y = 6000 - r * 550
        piece(pid, "def", "mv", f"Merchant {pid}", "UK", [(0, x, y), (END, x, y + V * END)], column=c)
# Boat A closes from the north-east on the surface, dives as Escort 2's radar finds her, runs at periscope depth, goes deep
# at 75 and is forced up at 170; she lies stopped where she comes up.
A0 = (12100, 9100)
A1 = along(A0, 225, 3.0 * 20)
A2 = along(A1, 235, 2.0 * 55)
A3 = along(A2, 260, 2.0 * 95)
A4 = along(A3, 260, 1.0 * 60)
piece(30, "att", "ss", "Boat A", "GER", [(0, *A0), (20, *A1), (75, *A2), (170, *A3), (230, *A4), (END, *A4)])
# Boat B at periscope depth to the north-west, closing the convoy's port bow; deep at 90, forced up at 170, stopped.
B0 = (7900, 8900)
B1 = along(B0, 140, 2.5 * 90)
B2 = along(B1, 150, 2.0 * 80)


def boat_b(last):
    """Boat B's path, her last leg (forced up, slowing to a stop) on the heading last; where she stops."""
    stop = along(B2, last, 1.0 * 60)
    piece(31, "att", "ss", "Boat B", "ITA", [(0, *B0), (90, *B1), (170, *B2), (230, *stop), (END, *stop)])
    return stop


B3 = boat_b(150)
# Escort 1 ahead of the convoy: round to Boat A on its asdic contact, over her to drop its pattern at 140, then to
# windward of where she lies, stopped 95 m off her for her men from 320.
E1_STATION = along(A4, WIND_FROM, 95.0)
piece(20, "def", "dd", "Escort 1", "UK", [(0, 10200, 8000), (60, 10200 + 0, 8000 + 6.0 * 60), (140, *along(A2, 260, 2.0 * 65)), (200, 12300, 8500),
                                         (260, *along(E1_STATION, 30, 500)), (300, *along(E1_STATION, 30, 60)), (320, *E1_STATION), (END, *E1_STATION)])
# Escort 2 on the convoy's starboard bow: its radar on Boat A on the surface, and its star shell over her.
piece(21, "def", "dd", "Escort 2", "UK", [(0, 11400, 7200), (END, 11400, 7200 + V * END)])
# Escort 3 on the convoy's port bow: round to Boat B on its asdic contact, its ahead-thrown salvo at 160 from 250 m astern
# of her, then stopped 140 m to windward of where she comes up from 220 (her whaler pulls across), for her men too.
E3_STATION = along(B3, WIND_FROM, 140.0)
# (on the abeam night Boat B's last leg is on LAST_ABEAM: the show brings her to rest about SHOWN_B, short of its end, and
# once her crew are out lays her at about 229, sheered away from Escort 3 on her starboard bow. Escort 3, told to lie
# 140 m off her starboard beam (319, her weather side with the wind from 300), lies some 240 m off it as the show steers
# it, 77 degrees off her bow: her crew going over that side, its whaler bound for her lee side round her bow)
LAST_ABEAM = 228.0
SHOWN_B = (8051.0, 8469.0)
E3_ABEAM = along(SHOWN_B, 319.0, 140.0)


def escort3(station):
    piece(22, "def", "dd", "Escort 3", "UK", [(0, 8600, 7300), (100, 8600, 7300 + 6.0 * 100), (160, *along(along(B1, 150, 2.0 * 75), 330, 250)), (185, *along(station, 200, 250)),
                                             (210, *along(station, 200, 30)), (220, *station), (END, *station)])


escort3(E3_STATION)


def place(path, t):
    for (t0, x0, y0), (t1, x1, y1) in zip(path, path[1:]):
        if t0 <= t <= t1:
            k = (t - t0) / (t1 - t0) if t1 > t0 else 0.0
            return x0 + (x1 - x0) * k, y0 + (y1 - y0) * k, x1 - x0, y1 - y0, t1 - t0
    t0, x0, y0 = path[-1]
    return x0, y0, 0.0, 0.0, 1.0


SUNK = {30: 310.0}                   # Boat B never: a prize
STRUCK = {30: 250.0, 31: 250.0}      # her piece out of the fight from the view that tells it
# (Boat B: the ahead-thrown bombs fired at 160 reach her at 50 m in some 12 s, so the hit they book is told at 172 and she
# is forced up then, 78 s before she strikes: time to come up from 50 m and for Escort 3 to close and lie stopped)
DIVES = {  # piece -> [(from t, state, keel or None, scope)]
    30: [(0, "surfaced", None, False), (20, "periscope", None, True), (75, "deep", 60, False), (170, "surfaced", None, False)],
    31: [(0, "periscope", None, True), (90, "deep", 50, False), (172, "surfaced", None, False)],
}
# Escort 1's guns on Boat A once she is up, no hit booked (its rounds fall about her), until she strikes.
ENGAGED = {20: [(236, 250, 30)]}


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
    out = gone or (pid in STRUCK and t >= STRUCK[pid])
    ships = 0 if out else 1
    u = {"id": pid, "x": round(x, 1), "y": round(y, 1), "face": round(face, 4), "str": 0.0 if out else 1.0, "mor": 1, "state": "gone" if gone else "steady",
         "figures": ships, "d": 0, "act": "march", "speed": round(speed, 2), "engaged": -1,
         "extra": {"ships": ships, "sunk": 1 if gone else 0}}
    for (t0, t1, on) in ENGAGED.get(pid, []):
        if t0 <= t < t1:
            u["engaged"] = on
    if first:
        u.update({"side": p["side"], "kind": p["cls"], "name": p["name"], "nat": p["nat"], "figures0": 1})
        u["extra"].update({"cls": p["cls"], "ships0": 1, "fleet": "Convoy ME 0" if p["side"] == "def" else "Pack"})
    u["extra"].update({k: v for k, v in p["extra"].items()})
    if pid in DIVES:
        _, state, keel, scope = at(DIVES[pid], t, DIVES[pid][0])
        u["extra"]["dive"] = state
        u["extra"]["scope"] = scope
        u["extra"]["keel"] = keel if keel is not None else {"surfaced": 4.7, "periscope": 13, "deep": 60}[state]
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


def contact(by, on, t):
    """A sea:sonar's range (metres) and bearing (radians clockwise from north) from the escort to the boat, from their paths."""
    (ax, ay), (bx, by_) = pos(by, t), pos(on, t)
    return dict(range=round(math.hypot(bx - ax, by_ - ay)), bearing=round(math.atan2(bx - ax, by_ - ay) % (2 * math.pi), 3))


def events():
    return [
    # Boat A: found on the surface by Escort 2's radar and held; down to periscope depth, deep; Escort 1's asdic on her.
    ev(16, "sea:sonar", 21, "def", "Escort 2 has a radar contact.", on=30, kind="radar", **contact(21, 30, 16)),
    ev(20, "sea:dive", 30, "att", "Boat A dives.", dive="periscope", why="radar"),
    ev(40, "sea:starshell", 21, "def", "Escort 2 fires star shell.", here=pos(30, 40), by=21),
    ev(60, "sea:sonar", 20, "def", "Escort 1 has a contact.", on=30, kind="asdic", **contact(20, 30, 60)),
    ev(75, "sea:dive", 30, "att", "", dive="deep", keel=60, why="evade"),
    ev(140, "sea:depthCharge", 20, "def", "Escort 1 drops a pattern on Boat A.", here=pos(20, 140), on=30, x=pos(30, 140)[0], y=pos(30, 140)[1], n=10, set=60,
       sinking=20.0, spread=120, kind="pattern"),
    ev(165, "sea:depthChargeHit", 30, "def", "Boat A is damaged.", here=pos(30, 165), harm=0.6, keel=60, **{"from": 20}),
    ev(170, "sea:dive", 30, "att", "Boat A is forced to the surface.", dive="surfaced", why="forced"),
    # Boat B: Escort 3's asdic on her at periscope depth; deep; its ahead-thrown salvo and the hit it books; forced up.
    ev(100, "sea:sonar", 22, "def", "Escort 3 has a contact.", on=31, kind="asdic", **contact(22, 31, 100)),
    ev(90, "sea:dive", 31, "att", "Boat B goes deep.", dive="deep", keel=50, why="evade"),
    ev(160, "sea:depthCharge", 22, "def", "Escort 3 fires ahead at Boat B.", here=pos(22, 160), on=31, x=pos(31, 160)[0], y=pos(31, 160)[1], n=24, kind="ahead"),
    ev(172, "sea:depthChargeHit", 31, "def", "Boat B is hit.", here=pos(31, 172), harm=0.6, keel=50, **{"from": 22}),
    ev(172, "sea:dive", 31, "att", "Boat B is forced to the surface.", dive="surfaced", why="forced"),
    # Boat B strikes to Escort 3, which takes her; her men in the water, picked up as Escort 3's whaler is alongside her.
    ev(250, "sea:surrender", 31, "att", "Boat B strikes her colours.", here=pos(31, 250), ship=1, how="captured", by=22),
    ev(256, "sea:men", 31, "att", "", ship=1, men=40, dead=0),
    # Boat A strikes as Escort 1's guns open on her; her crew ordered off her, counted, and she is scuttled.
    ev(250, "sea:surrender", 30, "att", "Boat A strikes.", here=pos(30, 250), ship=1, how="scuttled", by=20),
    ev(252, "sea:abandon", 30, "att", "Boat A's crew abandon her.", here=pos(30, 252), ship=1),
    ev(254, "sea:men", 30, "att", "", ship=1, men=44, dead=0),
    ev(310, "sea:sunk", 30, "att", "Boat A is scuttled by her crew.", "ship_sinking", here=pos(30, 310), cls="ss", how="scuttled", ship=1),
    # The rescues: each escort stopped near the men, the count of those picked up rising as they climb its nets.
    ev(300, "sea:rescue", 22, "def", "Escort 3 picks up Boat B's crew.", here=pos(22, 300), on=31, wreck={"unit": 31, "ship": 1}, men=0),
    ev(340, "sea:rescue", 20, "def", "Escort 1 picks up Boat A's crew.", here=pos(20, 340), on=30, wreck={"unit": 30, "ship": 1}, men=0),
    ev(400, "sea:rescue", 22, "def", "", on=31, wreck={"unit": 31, "ship": 1}, men=10),
    ev(430, "sea:rescue", 20, "def", "", on=30, wreck={"unit": 30, "ship": 1}, men=10),
    ev(460, "sea:rescue", 22, "def", "", on=31, wreck={"unit": 31, "ship": 1}, men=22),
    ev(490, "sea:rescue", 20, "def", "", on=30, wreck={"unit": 30, "ship": 1}, men=24),
    ev(520, "sea:rescue", 22, "def", "", on=31, wreck={"unit": 31, "ship": 1}, men=34),
    ev(550, "sea:rescue", 20, "def", "", on=30, wreck={"unit": 30, "ship": 1}, men=36),
    ev(590, "sea:rescue", 22, "def", "", on=31, wreck={"unit": 31, "ship": 1}, men=40, done=True),
    ev(592, "sea:rescue", 20, "def", "", on=30, wreck={"unit": 30, "ship": 1}, men=44, done=True),
    ]


def abandon_events(told):
    """The night with no boat striking: Boat A forced up, abandoned and scuttled by her crew (sea:abandon, sea:men, sea:sunk,
    no sea:surrender); Boat B forced up and diving again to get away, no rescue of hers."""
    kept = [e for e in told if e["what"] != "sea:surrender" and not (e["unit"] == 31 and e["what"] == "sea:men")
            and not (e["what"] == "sea:rescue" and e["extra"].get("on") == 31)]
    kept.append(ev(230, "sea:dive", 31, "att", "Boat B dives.", dive="deep", keel=50, why="evade"))
    return kept


def night(key, title, pids, events, lines):
    views = []
    t = 0.0
    told = 0
    while t <= END + 1e-9:
        first = not views
        v = {"t": t, "limit": END, "speed": 1, "me": "def", "contact": 0, "balance": {"att": 0.5},
             "units": [unit(pid, t, first) for pid in pids],
             "events": []}
        while told < len(events) and events[told]["t"] <= t + 1e-9:
            v["events"].append(events[told])
            told += 1
        if t >= END - 1e-9:
            v["over"] = {"winner": "def", "level": "Victory", "lines": ["The convoy holds its course.", lines]}
        views.append(v)
        t += STEP
    rec = {
        "what": "a stand-in convoy night with boats that strike (hand-made, not the battle's)", "recording": 1, "key": key, "seed": 242, "step": STEP,
        "field": {
            "size": 20000, "edge": 5000, "zoneName": "Eastern Mediterranean", "title": title, "date": "Practice",
            "when": {"year": 1942, "month": 7}, "lat": 33, "lon": 30,
            "att": {"nat": "GER", "adj": "German"}, "def": {"nat": "UK", "adj": "British"},
            "classes": {
                "dd": {"len": 95, "beam": 10, "speed": 12, "turn": 0.12, "reach": 4000},
                "ss": {"len": 67, "beam": 6.2, "speed": 7, "turn": 0.1, "reach": 3000},
                "mv": {"len": 135, "beam": 17.5, "speed": 3.6, "turn": 0.05, "reach": 0},
            },
            "torp": {"travel": 150, "radius": 400}, "strike": {"windup": 90, "radius": 1500},
            "wind": {"from": round(math.radians(WIND_FROM), 3), "speed": 4.5},
            "light": {"sun": -20, "moon": {"lit": 0.8, "up": 35, "from": 120}, "vis": 3500},
            "subs": {"scope": 13, "deep": 60, "crush": 250, "dived": 3.9},
            "dc": {"sink": 3.0, "lethal": 6.0},
        },
        "views": views,
        "after": {"lines": ["The convoy holds its course."], "level": "Victory", "winner": "def", "seconds": END},
    }
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), key + ".json")
    with open(out, "w") as f:
        json.dump(rec, f, separators=(",", ":"))
    print(out, len(views), "views,", len(events), "events")


def main():
    global STRUCK, DIVES
    told = sorted(events(), key=lambda e: e["t"])
    night("wolf-pack-surrender", "Two boats strike (stand-in)", sorted(PIECES), told, "Two boats struck: one scuttled, one taken.")
    # (Escort 3 abeam of Boat B on her weather side: its whaler round her bow or her stern to her lee side)
    boat_b(LAST_ABEAM)
    escort3(E3_ABEAM)
    told = sorted(events(), key=lambda e: e["t"])
    night("wolf-pack-abeam", "Two boats strike, a captor abeam (stand-in)", sorted(PIECES), told, "Two boats struck: one scuttled, one taken.")
    # (no boat strikes: Boat A abandoned and scuttled, Boat B down again and away; she and Escort 3 back as on the first)
    boat_b(150)
    escort3(E3_STATION)
    STRUCK = {30: 252.0}  # (no boat strikes: Boat A's piece out of the fight once her crew abandon her)
    DIVES = dict(DIVES)
    DIVES[31] = DIVES[31] + [(230, "deep", 50, False)]
    told = sorted(abandon_events(events()), key=lambda e: e["t"])
    night("wolf-pack-abandon", "A boat abandoned (stand-in)", sorted(PIECES), told, "One boat abandoned and scuttled, one got away.")


if __name__ == "__main__":
    main()
