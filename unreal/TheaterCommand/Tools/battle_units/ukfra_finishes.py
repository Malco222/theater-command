"""The helmet finishes of the British and French soldiers (make_ukfra.py), in helmet_finishes.py's form (see there for
what each field means). Our own drawing of what the two armies wore, from the period's issue and photographs; colours
are estimates, not measured paint. No badge is drawn: British helmets carried none as issued (a unit's painted sign is
left off), and the French Adrian's front badge (the arm's grenade or anchor) is left off too.
"""

LEAF = ["4E6B2C", "5C7A32", "3F5A25", "6F7F3B"]
LEAF_DRY = ["8A8A4A", "A3965E", "6B5A3A"]
HESSIAN = ["8E7B55", "77744A", "6B6A44", "5E6040"]   # (scrim: hessian strips dyed green and brown, and raw)

UK_PAINT = "50513A"              # (the helmet paint the battles give the British: armies.json's UK row)
UK_WEAR = "3C3D2F"               # (the rough paint rubbed thin at the rim: darker, toward the steel)
UK = [   # (the Brodie, Mk II: a shallow bowl with a wide flat brim all round)
    {"name": "plain", "about": "Issue Mk II steel helmet in khaki-green paint with sand or sawdust in it to kill the shine, "
     "the rim rubbed toward the steel. No badge as issued (a unit's painted sign is left off).",
     "from": "1939-09", "to": "1945-08", "share": 0.4,
     "paint": [{"kind": "wear", "hex": UK_WEAR}]},
    {"name": "net", "about": "The British helmet net (the wide mesh Britain also made for the Americans), worn bare, as "
     "most men had it from 1942.",
     "from": "1941-06", "to": "1945-08", "share": 0.3,
     "paint": [{"kind": "wear", "hex": UK_WEAR}],
     "extras": [{"kind": "net", "hex": "5A5A3E", "mesh": 0.036, "under": UK_PAINT}]},
    {"name": "net and scrim", "about": "Net with strips of hessian scrim, green, brown and raw, pushed through it and hanging "
     "past the brim: Sicily, Italy, Normandy and the Low Countries, 1943 to 1945.",
     "from": "1943-07", "to": "1945-05", "share": 0.35,
     "paint": [{"kind": "wear", "hex": UK_WEAR}],
     "extras": [{"kind": "net", "hex": "5C5A40", "mesh": 0.038, "under": UK_PAINT, "far_mix": 0.35},
                {"kind": "scrim", "hex": HESSIAN, "count": 12, "wide": 0.026}]},
    {"name": "net and leaves", "about": "Net with leafy sprigs from the hedges and orchards stuck through it (Normandy and the "
     "summer advance, 1944).",
     "from": "1944-06", "to": "1944-10", "front": "west", "season": "summer", "share": 0.2,
     "paint": [{"kind": "wear", "hex": UK_WEAR}],
     "extras": [{"kind": "net", "hex": "5C5A40", "mesh": 0.038, "under": UK_PAINT},
                {"kind": "scrim", "hex": HESSIAN[:2], "count": 4, "wide": 0.022},
                {"kind": "foliage", "hex": LEAF + LEAF_DRY[:1], "count": 11, "at": [6, 30], "size": 0.075}]},
    {"name": "desert sand", "about": "Repainted in the desert in a sandy light stone, often with sand thrown on the wet paint; "
     "the green shows at the chips (the Western Desert, 1940 to 1943).",
     "from": "1940-06", "to": "1943-05", "front": "desert", "share": 0.55, "base": "B4A27A",
     "paint": [{"kind": "wear", "hex": "6E6A4E"}, {"kind": "dust", "hex": "C6B48C", "level": 0.35, "hard": False}]},
    {"name": "desert dust", "about": "Never repainted: the khaki-green helmet under a coat of desert dust.",
     "from": "1940-06", "to": "1943-05", "front": "desert", "share": 0.3,
     "paint": [{"kind": "dust", "hex": "A89A76", "level": 0.2, "hard": False}, {"kind": "wear", "hex": UK_WEAR}]},
    {"name": "mud", "about": "Mud smeared on by hand to kill the shine.",
     "from": "1939-09", "to": "1945-05", "share": 0.05,
     "paint": [{"kind": "mud", "hex": "564A38", "level": 0.05, "high": 0.55, "scale": 0.03},
               {"kind": "mud", "hex": "6C604C", "level": 0.35, "high": 0.9, "seed": 3, "scale": 0.03}]},
    {"name": "whitewash", "about": "Whitewash brushed over in the snow (the Ardennes and Holland, winter 1944-45), rubbed "
     "back to the green at the rim.",
     "from": "1944-12", "to": "1945-02", "season": "winter", "share": 0.25, "inside": None,
     "paint": [{"kind": "wash", "hex": "E3E0D5", "level": -0.4, "under": False},
               {"kind": "mud", "hex": "B8B5A4", "level": 0.35, "high": 0.4, "under": False}]},
]

FR_PAINT = "50513A"              # (armies.json's FRA row)
FR_WEAR = "3D3C2E"
FRA = [   # (the Adrian, M26: a round crown in pressed pieces, a brim fore and aft, the crest along the top)
    {"name": "plain", "about": "Issue Adrian M26 in matt khaki paint, the crest and the brim's edge rubbed toward the steel. "
     "The front badge (the arm's grenade or anchor) is left off.",
     "from": "1939-09", "to": "1945-08", "share": 0.55,
     "paint": [{"kind": "wear", "hex": FR_WEAR}]},
    {"name": "road dust", "about": "The same helmet under the dust of the roads in May and June 1940.",
     "from": "1940-05", "to": "1940-06", "season": "summer", "share": 0.3,
     "paint": [{"kind": "dust", "hex": "8F8669", "level": 0.3, "hard": False}, {"kind": "wear", "hex": FR_WEAR}]},
    {"name": "horizon blue", "about": "An older helmet never repainted, still in the faded horizon blue of before 1935 "
     "(reservists' issue).",
     "from": "1939-09", "to": "1940-06", "share": 0.07, "base": "76838C",
     "paint": [{"kind": "wear", "hex": "4E575C", "level": 0.2}]},
    {"name": "mud", "about": "Mud smeared on by hand (the winter of 1939-40 on the frontier, and the spring rains).",
     "from": "1939-09", "to": "1945-05", "share": 0.08,
     "paint": [{"kind": "mud", "hex": "564A38", "level": 0.05, "high": 0.55, "scale": 0.03},
               {"kind": "mud", "hex": "6C604C", "level": 0.35, "high": 0.9, "seed": 3, "scale": 0.03}]},
    {"name": "desert sand", "about": "Painted sand in Africa and the Levant (1940 to 1943).",
     "from": "1940-06", "to": "1943-05", "front": "desert", "share": 0.6, "base": "B6A47C",
     "paint": [{"kind": "wear", "hex": "6E6A4E"}, {"kind": "dust", "hex": "C6B48C", "level": 0.35, "hard": False}]},
]

RECIPES = {"UK": UK, "FRA": FRA}

ABOUT = {
    "UK": "The British soldier for now (make_ukfra.py, 23 November 2026): the Polish rifleman's man, kit, rifle and clips "
          "(soldier_POL, the nearest cut, which stood in for the British before) under the British Brodie steel helmet "
          "(Mk II) and its chin strap, with its own field finishes, until a British soldier is made. Paint him with "
          "armies.json's UK rows.",
    "FRA": "The French soldier for now (make_ukfra.py, 23 November 2026): the Polish rifleman's man, kit, rifle and clips "
           "(soldier_POL, the nearest cut, which stood in for the French before) under the French Adrian steel helmet "
           "(M26) and its chin strap, with its own field finishes, until a French soldier is made. Paint him with "
           "armies.json's FRA row.",
}
