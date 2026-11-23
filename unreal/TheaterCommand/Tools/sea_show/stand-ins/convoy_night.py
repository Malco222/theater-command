#!/usr/bin/env python3
"""A stand-in convoy night for the sea show's cloud test (ruling 241, the submarine scene D1), in the recording's names
(naval-battles/wolfpack-r2/form.md section 5, field-side-read.md section 3). Hand-made, not the battle's: it exercises
every new name (boats surfaced, at periscope depth and deep; a boat's wakeless salvo; asdic, radar, a radio bearing and a
sighting, which find the boats the defender does not see until then, a contact held long with no attack on her, a boat
whose views say who is running in on her (heard, as the D1 stager writes it), a merchant's stern gun answering her and
another's bow gun on a boat fine on his bow, an escort's guns on a boat up, boats' deck guns laid on a merchant (one a
Type IX's 105 mm, gunMm) with their hits and his told one by one (sea:gunHit: one as she is about to dive, the last ones
as a straggler is shelled under) and the gun actions' words (sea:gun), a boat's gun told as she comes up from deep
before her casing is clear, one view past her dive and on a ship astern of her tower, a merchant engaged on a boat with
no gun told him, and an aircraft running in on a boat past another one not yet found; a pattern of depth charges and an
ahead-thrown salvo; hits, a boat crushed and one forced up, given up and scuttled; a star shell; a sighting report; a
rescue) until the production lead's staged recording lands.

    python3 Tools/sea_show/stand-ins/convoy_night.py   (writes convoy-night.json and convoy-night-air.json beside it)

convoy-night-air is the same night with Boat D astern and the escort's aircraft that runs in on her: kept to a night of
its own, as in this one they would shift every random draw the show makes after them, and the convoy looks' checks are
played on this night's draws as they are.
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
# The boats: A closing on the surface, driven down by the star shell, hunted, forced up, given up by her crew and scuttled;
# B at periscope depth, fires a wakeless salvo, goes deep, is damaged by a pattern and crushed by the ahead-thrown salvo.
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
# C shadows the convoy's port quarter on the surface, 1250 m off its last port merchant, until she dives at 240 and goes deep
# at 300, closing it at 2.5 m/s under. Her hunters are told by her views alone (heard, and each escort's echo on her: no
# sea:sonar for her), so it is her heard that holds her and lets her go: the sea show's SetMine. She is a Type IX, her deck
# gun a 105 mm the battle names (gunMm), not the German table's 88.
C0 = (7400, 5000)
C1 = (C0[0], C0[1] + V * 240)
C2 = along(C1, 45, 2.5 * 60)
C3 = along(C2, 45, 2.5 * 320)
piece(32, "att", "ss", "Boat C", "GER", [(0, *C0), (240, *C1), (300, *C2), (620, *C3)], gunMm=105)
# D (convoy-night-air only) trails the convoy on the surface, two kilometres astern of its port column, and finishes a
# straggler with her deck gun: Merchant 13, torpedoed before the night's views begin, stopped and settling 600 m off her
# starboard beam; an aircraft of the escort's runs in on her from ahead, over Boat C not yet found (her guns quiet), and
# lets go on her; she dives, goes deep and falls astern, unseen again for the rest of the night.
D0 = (7550, 3000)
D1 = (D0[0], D0[1] + V * 76)
D2 = (D1[0], D1[1] + 2.5 * (END - 76))
piece(33, "att", "ss", "Boat D", "GER", [(0, *D0), (76, *D1), (END, *D2)])
piece(13, "def", "mv", "Merchant 13", "UK", [(0, 8150, 3150), (END, 8150, 3150)])
AIR = {33, 13}


def place(path, t):
    for (t0, x0, y0), (t1, x1, y1) in zip(path, path[1:]):
        if t0 <= t <= t1:
            k = (t - t0) / (t1 - t0) if t1 > t0 else 0.0
            return x0 + (x1 - x0) * k, y0 + (y1 - y0) * k, x1 - x0, y1 - y0, t1 - t0
    t0, x0, y0 = path[-1]
    return x0, y0, 0.0, 0.0, 1.0


SUNK = {1: 272.0, 31: 481.0, 30: 450.0, 13: 40.0}
DIVES = {  # piece -> [(from t, state, keel or None, scope)]
    30: [(0, "surfaced", None, False), (65, "periscope", None, True), (150, "deep", 80, False), (360, "surfaced", None, False)],
    31: [(0, "periscope", None, True), (285, "deep", 80, False)],
    32: [(0, "surfaced", None, False), (240, "periscope", None, True), (300, "deep", 60, False)],
    33: [(0, "surfaced", None, False), (76, "periscope", None, True), (120, "deep", 60, False)],
}
# Whom the defender finds Boat C by (her views' heard, written as it changes, as the D1 stager does; and the escort's echo on
# her while it holds her): Escort 2's radar on her on the surface from 100, lost at 130 with her still up within the night's
# seeing of Merchant 3 (his lookouts keep her in sight until she is under: no pop on the surface in plain view); Escort 1's
# asdic on her deep from 472 as it comes back for the survivors, lost at 500 (she is let go FoundFor after). (472, off the
# 2.5 s beat of the swirls she has laid since she went deep at 300: found, her next swirl must come on that beat, not at once)
HEARD = {32: [(0, []), (100, [21]), (130, []), (472, [20]), (500, [])]}
ECHO = {21: [(100, 130, 32, "radar")], 20: [(472, 500, 32, "asdic")]}
# Deck guns laid (extra.gun, as each view says, while she is up): Boat D's on Merchant 13 from 20, her gun found before
# anything else has her, until the straggler goes down under it at 40 (her last two hits told at that moment, as the
# battle shells her under), as her lookouts see the aircraft coming; Boat C's on Merchant 3 from 64, her first hit on him at
# 63.8, before the view that says her gun is on (a gun's hit from her finds her at the hit), one more at 130 and two at
# 175, told one view past her dive at 240 (the show holds her gun as she is told down, her casing still clear); Merchant 3's
# gunners answering her with his stern gun from 80 until she dives, one hit on her at 205 and one at 238, a view before she
# dives (drawn as told, not owed to a round that never comes); Boat A's on Escort 3 from 400, forced up at 360 from 80 m and
# told at her gun 40 s on (the battle's surfaced and her crew up, while the show still has her coming up: it holds her gun
# until her casing is clear), then from 436 on Escort 2 coming up astern of her, in the blind arc her tower makes (a view the
# battle should never send: the show holds her gun at its arc's edge, no round), until her crew give her up at 446; and
# Merchant 12's gunners on her from 400, fine on his bow (his bow gun's: his stern gun does not bear). Each hit is told as
# it strikes and taken from her hull in that view.
GUN = {3: [(80, 240, 32)], 12: [(400, 446, 30)], 30: [(400, 436, 22), (436, 446, 21)], 32: [(64, 242, 3)], 33: [(20, 40, 13)]}
STR = {30: [(0, 1.0), (330, 0.5), (450, 0.0)], 31: [(0, 1.0), (358, 0.7), (481, 0.0)], 1: [(0, 1.0), (272, 0.0)],
       3: [(0, 1.0), (64, 0.96), (130, 0.92), (176, 0.84)], 32: [(0, 1.0), (206, 0.9), (238, 0.8)],
       13: [(0, 0.6), (26, 0.45), (32, 0.3), (40, 0.0)]}
# Guns engaged other than the gun told (a warship's line, "engaged"): Escort 3's on Boat A forced up, until her crew give her
# up; Escort 2's on Boat C up, from 110 to her dive (no hit booked: its rounds all fall about her); and Merchant 2 engaged on
# Boat C from 150 with no gun told him (a merchant's gun fires only where told: none of his).
ENGAGED = {22: [(436, 446, 30)], 21: [(110, 240, 32)], 2: [(150, 160, 32)]}


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
    for (t0, t1, on) in ENGAGED.get(pid, []):
        if t0 <= t < t1:
            u["engaged"] = on
    for (t0, heard) in HEARD.get(pid, []):
        if t0 <= t < t0 + STEP - 1e-9:
            u["extra"]["heard"] = heard
    for (t0, t1, on, kind) in ECHO.get(pid, []):
        if t0 <= t < t1:
            u["extra"]["echo"] = {"on": on, "kind": kind}
    for (t0, t1, on) in GUN.get(pid, []):
        if t0 <= t < t1:
            u["extra"]["gun"] = on
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


def contact(by, on, t):
    """A sea:sonar's range (metres) and bearing (radians clockwise from north) from the escort to the boat, from their paths."""
    (ax, ay), (bx, by_) = pos(by, t), pos(on, t)
    return dict(range=round(math.hypot(bx - ax, by_ - ay)), bearing=round(math.atan2(bx - ax, by_ - ay) % (2 * math.pi), 3))


EVENTS = [
    ev(20, "sea:report", 30, "att", "A U-boat reports the convoy.", at=1),
    # Whom the defender (the player: "me") finds, for the boats he does not see until then (the sea show's SetMine): Boat A's
    # report taken by direction-finding (a bearing, a line on the plot: it finds nothing), Escort 1's radar on her up and losing
    # her as she dives, Escort 3's asdic on her deep, and its lookouts on her once she is forced up and has broken surface.
    ev(21, "sea:sonar", 20, "def", "", on=30, kind="bearing", err=0.05, **contact(20, 30, 21)),
    ev(58, "sea:sonar", 20, "def", "Escort 1 has a radar contact.", on=30, kind="radar", **contact(20, 30, 58)),
    ev(70, "sea:sonar", 20, "def", "", on=30, kind="radar", lost="deep"),
    ev(300, "sea:sonar", 22, "def", "Escort 3 has a contact.", on=30, kind="asdic", **contact(22, 30, 300)),
    ev(436, "sea:sonar", 22, "def", "Escort 3 sights the boat on the surface.", on=30, kind="sight", **contact(22, 30, 436)),
    # (Escort 1's asdic on Boat B running down her tracks, held a hundred seconds with no attack on her until its station
    # ahead of the convoy calls it back and it loses her: she is in sight all the while it holds her, and FoundFor after)
    ev(130, "sea:sonar", 20, "def", "Escort 1 has a contact.", on=31, kind="asdic", **contact(20, 31, 130)),
    ev(230, "sea:sonar", 20, "def", "", on=31, kind="asdic", lost="faded"),
    # (Boat C's gun action on Merchant 3 and his gunners' answer, each told with its words and ceased as she dives; her
    # first hit on him before the view that says her gun is on, her later ones and his on her, each told as it strikes)
    ev(63.8, "sea:gunHit", 3, "att", "A shell from a U-boat's deck gun hits Merchant 3.", here=pos(3, 63.8), n=1, **{"from": 32}),
    ev(64, "sea:gun", 32, "att", "Boat C opens fire on Merchant 3 with her deck gun.", on=3, why="attack"),
    ev(80, "sea:gun", 3, "def", "Merchant 3's gunners return fire on the U-boat.", on=32, why="fight"),
    ev(130, "sea:gunHit", 3, "att", "Merchant 3 is hit again by the U-boat's gun.", here=pos(3, 130), n=1, **{"from": 32}),
    ev(175, "sea:gunHit", 3, "att", "Two more shells hit Merchant 3.", here=pos(3, 175), n=2, **{"from": 32}),
    ev(205, "sea:gunHit", 32, "def", "Merchant 3's gunners hit the U-boat.", here=pos(32, 205), n=1, **{"from": 3}),
    ev(238, "sea:gunHit", 32, "def", "Merchant 3's gunners hit the U-boat again.", here=pos(32, 238), n=1, **{"from": 3}),
    ev(240, "sea:gun", 32, "att", "", on=3, why="cease"),
    ev(240, "sea:gun", 3, "def", "", on=32, why="cease"),
    # (Boat A forced up: her crew at her deck gun against Escort 3, then Escort 2 astern, until they give her up; Merchant
    # 12's gunners on her)
    ev(400, "sea:gun", 30, "att", "Boat A's crew make for her deck gun.", on=22, why="fight"),
    ev(400, "sea:gun", 12, "def", "Merchant 12's gunners open fire on the surfaced U-boat.", on=30, why="fight"),
    ev(446, "sea:gun", 30, "att", "", on=21, why="cease"),
    ev(446, "sea:gun", 12, "def", "", on=30, why="cease"),
    ev(60, "sea:starshell", 20, "def", "Escort 1 fires star shell.", here=pos(30, 60), by=20),
    ev(65, "sea:dive", 30, "att", "Boat A dives.", dive="periscope", why="radar"),
    ev(120, "sea:torpedo", 31, "att", "Boat B fires a salvo at the convoy.", "torpedo_launch", here=pos(31, 120), target=1, x=pos(1, 270)[0], y=pos(1, 270)[1],
       running=150, fish=4, spread=10, wakeless=True, dive="periscope"),
    ev(150, "sea:dive", 30, "att", "", dive="deep", keel=80, why="evade"),
    ev(240, "sea:dive", 32, "att", "Boat C dives.", dive="periscope", why="sighted"),
    ev(300, "sea:dive", 32, "att", "", dive="deep", keel=60, why="evade"),
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
    # (Escort 2 loses Boat B close as it runs in over her, its pattern's charge at 358 the blow the battle books on her: in
    # sight FoundFor after the blow, not only after the loss and the drop; it gains her again for its ahead-thrown run, and
    # that contact is closed by her end)
    ev(335, "sea:sonar", 21, "def", "", on=31, kind="asdic", lost="close"),
    ev(358, "sea:depthChargeHit", 31, "def", "Boat B is damaged.", here=pos(31, 358), harm=0.3, keel=80, **{"from": 21}),
    ev(456, "sea:sonar", 21, "def", "Escort 2 regains the contact.", on=31, kind="asdic", **contact(21, 31, 456)),
    # (Boat A's crew give her up, forced up at 360: the escort's guns off her, and she goes down scuttled at 450)
    ev(446, "sea:surrender", 30, "att", "Boat A's crew give her up.", ship=1, how="scuttle", by=22),
    ev(470, "sea:depthCharge", 21, "def", "Escort 2 fires ahead at Boat B.", here=pos(21, 470), on=31, x=pos(31, 470)[0], y=pos(31, 470)[1], n=24, kind="ahead"),
    ev(480, "sea:depthChargeHit", 31, "def", "Boat B is hit.", here=pos(31, 480), harm=1, keel=80, **{"from": 21}),
    ev(450, "sea:sunk", 30, "att", "Boat A is scuttled by her crew.", "ship_sinking", here=pos(30, 450), cls="ss", how="scuttled"),
    ev(481, "sea:sunk", 31, "att", "Boat B is lost with all hands.", here=pos(31, 481), cls="ss", how="crushed"),
    ev(520, "sea:rescue", 20, "def", "Escort 1 picks up survivors.", here=pos(20, 520), on=1),
]
EVENTS.sort(key=lambda e: e["t"])
# convoy-night-air's own: Boat D's gun on Merchant 3 (begun and ceased, told) brings an aircraft of the escort's running in
# on her from ahead (over Boat C, not yet found: her guns quiet as it passes), letting go on her; she dives and goes deep.
AIR_EVENTS = [
    ev(20, "sea:gun", 33, "att", "Boat D opens fire on a straggler with her deck gun.", on=13, why="finish"),
    ev(26, "sea:gunHit", 13, "att", "A shell from a U-boat's deck gun hits Merchant 13.", here=pos(13, 26), n=1, **{"from": 33}),
    ev(30, "sea:air", 33, "def", "An escort aircraft runs in on a U-boat astern of the convoy.", kind="patrol", n=1, running=40),
    ev(32, "sea:gunHit", 13, "att", "Merchant 13 is hit again.", here=pos(13, 32), n=1, **{"from": 33}),
    # (the straggler shelled under: her last two hits told at the moment she goes, played before it)
    ev(40, "sea:sunk", 13, "def", "Merchant 13 goes down under the U-boat's gunfire.", "ship_sinking", here=pos(13, 40), cls="mv", how="shelled"),
    ev(40, "sea:gunHit", 13, "att", "Two more shells hit Merchant 13.", here=pos(13, 40), n=2, **{"from": 33}),
    ev(40, "sea:gun", 33, "att", "", on=13, why="cease"),
    ev(76, "sea:dive", 33, "att", "Boat D dives.", dive="periscope", why="aircraft"),
    ev(120, "sea:dive", 33, "att", "", dive="deep", keel=60, why="evade"),
]


def night(key, title, pids, events):
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
            v["over"] = {"winner": "def", "level": "Victory", "lines": ["The convoy holds its course.", "One merchant lost; two boats sunk."]}
        views.append(v)
        t += STEP
    rec = {
        "what": "a stand-in convoy night (hand-made, not the battle's)", "recording": 1, "key": key, "seed": 241, "step": STEP,
        "field": {
            "size": 20000, "edge": 5000, "zoneName": "Western Approaches", "title": title, "date": "Practice",
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
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), key + ".json")
    with open(out, "w") as f:
        json.dump(rec, f, separators=(",", ":"))
    print(out, len(views), "views,", len(events), "events")


def main():
    night("convoy-night", "A convoy at night (stand-in)", [pid for pid in sorted(PIECES) if pid not in AIR], EVENTS)
    night("convoy-night-air", "A convoy at night, an aircraft over it (stand-in)", sorted(PIECES), sorted(EVENTS + AIR_EVENTS, key=lambda e: e["t"]))


if __name__ == "__main__":
    main()
