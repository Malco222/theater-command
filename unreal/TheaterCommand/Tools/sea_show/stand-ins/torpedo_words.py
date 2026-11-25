#!/usr/bin/env python3
"""A stand-in fleet action carrying the torpedo words' fields, for the sea show's cloud tests (the naval rules behind
their switch "Friendly torpedoes", 25 November: a piece holds its salvo while an own piece is in its line of fire, and own
pieces turn clear of a running own salvo), until the naval re-recordings land. It is the two-lines recording
(Content/Data/SeaBattles/two-lines.json: the Home Fleet, the player's, against the Battle Fleet) as fought, unchanged but
for the fields written into some pieces' extra over a few spans of views, its key and its title:

    heldFor, heldForName      the own piece in its line of fire, and that piece as the log names it (lower case)
    clearFrom, clearUntil,    the own piece whose running salvo it turns clear of, the battle second that salvo lands
    clearFromName             and the launcher as the log names it
    ordered                   under an order of the player's own (with either)

The spans (battle seconds, each view from the first to the last), set round the battle's own salvos (Destroyers 2 fire at
375.9 and land at 450.9; Destroyers 1 fire at 423 and land at 498; the Battle Fleet's Destroyers 2 at 369.9, Destroyers 1
at 417):
  his own (the Home Fleet):
    Destroyers 2 (two ships then) hold for Battleship 1, under his order, 360 to 374;
    Cruiser 2 (one ship) holds for Destroyers 1 (four ships: "are"), unordered, 380 to 398;
    Destroyers 1 (four ships) turn clear of Destroyers 2's salvo 376 to 452 (unordered to 402, his order from 404), and
      hold for Cruiser 3 404 to 422, under his order: held wins over turning clear; the count runs to 0 at 452;
    Battleship 1 (one ship) turns clear of Destroyers 2's salvo, unordered, 376 to 452;
    Cruiser 1 (one ship) turns clear of Destroyers 1's salvo, under his order, 424 to 500;
    Cruiser 4 (one ship) turns clear of Destroyers 1's salvo 424 to 500, and holds for Cruiser 2 440 to 450, under his
      order only then: held wins;
  the enemy's (the Battle Fleet; never any words):
    Destroyers 2 hold for Destroyers 1, ordered, 356 to 368;
    Cruiser 1 turns clear of Destroyers 1's salvo, unordered, 418 to 494;
    Battleship 1 turns clear of Destroyers 2's salvo, ordered, 370 to 446, and holds for Cruiser 2 380 to 390.

    python3 Tools/sea_show/stand-ins/torpedo_words.py   (writes two-lines-torpedoes.json beside it)

Tools/sea_show/sea_words_test.cpp holds the words this night must show, string for string.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "..", "..", "..", "Content", "Data", "SeaBattles", "two-lines.json")
OUT = os.path.join(HERE, "two-lines-torpedoes.json")

# (piece, from, to, fields): each view from..to (inclusive) of that piece is given these fields.
HOME = "of the Home Fleet"
BATTLE = "of the Battle Fleet"
SPANS = [
    # his own
    (2, 360, 374, dict(heldFor=7, heldForName=f"a battleship {HOME}", ordered=True)),
    (4, 380, 398, dict(heldFor=1, heldForName=f"four destroyers {HOME}")),
    (1, 376, 452, dict(clearFrom=2, clearUntil=450.9, clearFromName=f"two destroyers {HOME}")),
    (1, 404, 452, dict(ordered=True)),
    (1, 404, 422, dict(heldFor=5, heldForName=f"a cruiser {HOME}")),
    (7, 376, 452, dict(clearFrom=2, clearUntil=450.9, clearFromName=f"two destroyers {HOME}")),
    (3, 424, 500, dict(clearFrom=1, clearUntil=498, clearFromName=f"four destroyers {HOME}", ordered=True)),
    (6, 424, 500, dict(clearFrom=1, clearUntil=498, clearFromName=f"four destroyers {HOME}")),
    (6, 440, 450, dict(heldFor=4, heldForName=f"a cruiser {HOME}", ordered=True)),
    # the enemy's
    (11, 356, 368, dict(heldFor=10, heldForName=f"four destroyers {BATTLE}", ordered=True)),
    (14, 418, 494, dict(clearFrom=10, clearUntil=492, clearFromName=f"four destroyers {BATTLE}")),
    (19, 370, 446, dict(clearFrom=11, clearUntil=444.9, clearFromName=f"four destroyers {BATTLE}", ordered=True)),
    (19, 380, 390, dict(heldFor=15, heldForName=f"a cruiser {BATTLE}")),
]


def main():
    with open(SOURCE) as f:
        rec = json.load(f)
    rec["key"] = "two-lines-torpedoes"
    rec["field"]["title"] = rec["field"].get("title", "") + " (stand-in: the torpedo words)"
    given = 0
    for view in rec["views"]:
        t = view["t"]
        for unit in view["units"]:
            for pid, t0, t1, fields in SPANS:
                if unit["id"] == pid and t0 <= t <= t1:
                    unit.setdefault("extra", {}).update(fields)
                    given += 1
    with open(OUT, "w") as f:
        json.dump(rec, f, separators=(",", ":"))
    print(f"wrote {OUT}: {given} piece-views given fields")


if __name__ == "__main__":
    main()
