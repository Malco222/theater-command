#!/usr/bin/env python3
"""A stand-in fleet action for the sea show's cloud test: four destroyers in column running straight at their target and
firing, in the recording's names. Hand-made, not the battle's: a British flotilla against an Italian battleship, June
1941. The column steams north at 16 m/s with the battleship crossing ahead of it from the east; at 150 s the battle has the
flotilla fire (sea:torpedo, its aim dead ahead of the column, where she will be when the torpedoes arrive) and the leader
turns away to starboard under smoke, the rest following him round at his turning point; at 240 s one torpedo hits her
(sea:torpedoHit). A British cruiser of the same side steams east 400 m beyond her track, across the water the tracks run on
into past their aim, as they arrive.

The test this night is for (Brian, 25 November 12:52Z, "the friendly ships would pass though right in front of
torpedoes"): fired at the battle's moment from each ship's own place, as the show did, every follower's tracks would start
behind the ship ahead of her and run through her (each 330 m astern of the next, the aim dead ahead). The show holds a
ship's tubes until her way to the aim is clear of every ship of her side, fires them at the speed that brings them there
with the battle's, from the salvo's firing point (the place the battle told it from) if her own way is not clear by a third
of the run; and stops a track at its aim where an own ship lies across the water it would run on into. The field says ownWater: the rules keep their own
ships out of a running salvo's water (the naval rules' switch), so the sea show's test holds this night to no track through
an own ship anywhere, and every track the battle fired drawn.

Beside it the same column blocked (destroyer-column-blocked.json, the review of 25 November): the British cruiser, another
piece of the same side, lies stopped 200 m short of the aim across every destroyer's way to it (the rules' switch off: they
let her lie in the salvo's water), and the battle tells three hits at 240, 241 and 242 s, more than the tracks the column
can draw by then. The show holds each ship's tubes while the cruiser is in her way, fires those it can round the cruiser's
ends, and gives up the rest at two thirds of the run, each judged over its hold (the cruiser in the salvo's water ahead of
its heads, at some look all that stood in its way: the rules' side, the known failures' line for this night bounds it);
every hit still ends a track that ran to her, none fired at the hit.

    python3 Tools/sea_show/stand-ins/destroyer_column.py   (writes destroyer-column.json and destroyer-column-blocked.json beside it)
"""
import json
import math
import os

STEP = 2.0
END = 330.0
FIRE = 150.0      # the battle's moment the flotilla fires
RUNNING = 90.0    # the torpedoes' running time to the aim (the battle's)
CONTACT = 40.0    # when the guns met (the battle's view field contact)
HIT = FIRE + RUNNING

# The flotilla's leader: north at 16 m/s from (10000, 5000); from FIRE round to starboard on a 400 m circle, then south.
DD_SPEED = 16.0
TURN_R = 400.0
L0 = (10000.0, 5000.0)
L_FIRE = (L0[0], L0[1] + DD_SPEED * FIRE)
TURN_FOR = math.pi * TURN_R / DD_SPEED


def leader(t):
    """The flotilla's place, heading (radians, clockwise from north), helm and run phase at t."""
    if t <= FIRE:
        return L0[0], L0[1] + DD_SPEED * t, 0.0, 0.0, "in"
    if t <= FIRE + TURN_FOR:
        a = DD_SPEED * (t - FIRE) / TURN_R
        return L_FIRE[0] + TURN_R * (1.0 - math.cos(a)), L_FIRE[1] + TURN_R * math.sin(a), a, 1.0, "away"
    x, y = L_FIRE[0] + 2.0 * TURN_R, L_FIRE[1]
    return x, y - DD_SPEED * (t - FIRE - TURN_FOR), math.pi, 0.0, "away"


# The battleship west at 9 m/s, 5400 m north of the flotilla's start: at the torpedoes' arrival dead ahead of its column.
BB_SPEED = 9.0
BB_Y = 10400.0
AIM = (L0[0], BB_Y)


def battleship(t):
    return AIM[0] + BB_SPEED * (HIT - t), BB_Y, 1.5 * math.pi


# The British cruiser east at 12 m/s, 400 m beyond the battleship's track: across the tracks' run on past the aim some 8 s
# after they arrive.
CA_SPEED = 12.0
CA_Y = BB_Y + 400.0
CA_CROSS = HIT + 8.0


def cruiser(t):
    return AIM[0] + CA_SPEED * (t - CA_CROSS), CA_Y, 0.5 * math.pi


# The blocked night's cruiser: west at 0.5 m/s (barely under way), 200 m short of the battleship's track and 60 m east of
# the aim at the flotilla's moment of firing: across every destroyer's way to the aim, in the salvo's water, all night.
BLOCK_SPEED = 0.5
BLOCK_Y = BB_Y - 200.0


def cruiser_blocking(t):
    return AIM[0] + 60.0 - BLOCK_SPEED * (t - FIRE), BLOCK_Y, 1.5 * math.pi


def unit(pid, t, first, blocked=False):
    if pid == 1:
        x, y, face, helm, phase = leader(t)
        u = {"id": 1, "x": round(x, 1), "y": round(y, 1), "face": round(face % (2.0 * math.pi), 4), "str": 1, "mor": 1, "state": "steady", "figures": 4,
             "d": 1100, "act": "fight" if t >= FIRE - 30.0 else "march", "speed": DD_SPEED, "engaged": -1,
             "extra": {"ships": 4, "sunk": 0, "way": DD_SPEED, "speed": "cruise", "target": 10, "foe": -1, "helm": helm, "runPhase": phase,
                       "smoke": FIRE <= t < FIRE + 110.0}}
        if phase == "away":
            u["extra"]["runUntil"] = FIRE + TURN_FOR
        if first:
            u.update({"side": "att", "kind": "dd", "name": "Destroyers 1", "nat": "UK", "figures0": 4, "w": 11})
            u["extra"].update({"cls": "dd", "ships0": 4, "fleet": "Force K"})
        return u
    if pid == 2:
        x, y, face = cruiser_blocking(t) if blocked else cruiser(t)
        way = BLOCK_SPEED if blocked else CA_SPEED
        u = {"id": 2, "x": round(x, 1), "y": round(y, 1), "face": round(face, 4), "str": 1, "mor": 1, "state": "steady", "figures": 1, "d": 180,
             "act": "fight", "speed": way, "engaged": -1,
             "extra": {"ships": 1, "sunk": 0, "way": way, "speed": "cruise", "target": 10, "foe": -1, "helm": 0, "smoke": False}}
        if first:
            u.update({"side": "att", "kind": "ca", "name": "Cruiser 2", "nat": "UK", "figures0": 1, "w": 20})
            u["extra"].update({"cls": "ca", "ships0": 1, "fleet": "Force K"})
        return u
    x, y, face = battleship(t)
    u = {"id": 10, "x": round(x, 1), "y": round(y, 1), "face": round(face, 4), "str": 1 if t < HIT else 0.8, "mor": 1, "state": "steady", "figures": 1, "d": 240,
         "act": "fight", "speed": BB_SPEED, "engaged": -1,
         "extra": {"ships": 1, "sunk": 0, "way": BB_SPEED, "speed": "cruise", "target": -1, "foe": -1, "helm": 0, "smoke": False}}
    if first:
        u.update({"side": "def", "kind": "bb", "name": "Battleship 10", "nat": "ITA", "figures0": 1, "w": 33})
        u["extra"].update({"cls": "bb", "ships0": 1, "fleet": "Battle Fleet"})
    return u


def events(blocked=False):
    lx, ly, *_ = leader(FIRE)
    told = [
        {"t": FIRE, "x": round(lx), "y": round(ly), "cue": "torpedo_launch", "what": "sea:torpedo", "unit": 1, "side": "att",
         "text": "Four destroyers of Force K fire torpedoes at a battleship of the Battle Fleet.",
         "extra": {"target": 10, "x": round(AIM[0]), "y": round(AIM[1]), "running": RUNNING, "share": 1}},
    ]
    # (the blocked night: three hits a second apart, more than the column's tracks drawn by then)
    for k in ((0.0, 1.0, 2.0) if blocked else (0.0,)):
        bx, by, _ = battleship(HIT + k)
        told.append({"t": HIT + k, "x": round(bx), "y": round(by), "cue": "torpedo_hit", "what": "sea:torpedoHit", "unit": 10, "side": "att",
                     "text": "Torpedoes from 4 destroyers of Force K hit a battleship of the Battle Fleet.", "extra": {"from": 1}})
    return told


def write(blocked):
    told = events(blocked)
    views = []
    t = 0.0
    n = 0
    while t <= END + 1e-9:
        first = not views
        # (the guns met at CONTACT, as before any torpedo attack: the show's zig-zag about each side's course fades out)
        v = {"t": t, "limit": END, "speed": 1, "contact": CONTACT, "units": [unit(pid, t, first, blocked) for pid in (1, 2, 10)], "events": []}
        while n < len(told) and told[n]["t"] <= t + 1e-9:
            v["events"].append(told[n])
            n += 1
        if t >= END - 1e-9:
            v["over"] = {"winner": "att", "level": "Victory", "lines": ["The flotilla's torpedoes hit the battleship."]}
        views.append(v)
        t += STEP
    key = "destroyer-column-blocked" if blocked else "destroyer-column"
    rec = {
        "what": "a stand-in fleet action: four destroyers in column running at their target and firing" + (", an own cruiser across their way" if blocked else "") +
                " (hand-made, not the battle's)",
        "recording": True, "key": key, "seed": 251, "step": STEP,
        "field": {
            "size": 20000, "edge": 5000, "zoneName": "Central Mediterranean",
            "title": "A destroyer column fires past its own cruiser (stand-in)" if blocked else "A destroyer column fires ahead (stand-in)", "date": "Practice",
            "when": {"year": 1941, "month": 6}, "lat": 37, "lon": 15.5,
            "att": {"nat": "UK", "adj": "British", "human": False, "fleets": [{"id": 1, "name": "Force K", "ships": {"bb": 0, "cv": 0, "ca": 1, "dd": 4}}]},
            "def": {"nat": "ITA", "adj": "Italian", "human": False, "fleets": [{"id": 2, "name": "Battle Fleet", "ships": {"bb": 1, "cv": 0, "ca": 0, "dd": 0}}]},
            "classes": {
                "bb": {"len": 240, "beam": 33, "speed": 11, "turn": 0.05, "reach": 5000},
                "ca": {"len": 180, "beam": 20, "speed": 13, "turn": 0.08, "reach": 5000},
                "dd": {"len": 110, "beam": 11, "speed": 16, "turn": 0.12, "reach": 5000},
            },
            "torp": {"travel": RUNNING, "radius": 800}, "strike": {"windup": 90, "radius": 1500},
            "wind": {"from": 3.653, "speed": 4.4},
        },
        "views": views,
        "after": {"lines": ["The flotilla's torpedoes hit the battleship."], "level": "Victory", "winner": "att", "seconds": END},
    }
    if not blocked:
        # (the naval rules' switch: their own ships kept out of a running salvo's water; off on the blocked night, where
        # the cruiser lies in it)
        rec["field"]["ownWater"] = True
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), key + ".json")
    with open(out, "w") as f:
        json.dump(rec, f, separators=(",", ":"))
    print(out, len(views), "views,", len(told), "events")


def main():
    write(False)
    write(True)


if __name__ == "__main__":
    main()
