"""The helmet finishes each army's soldier wore in the field, as variety.py builds them (Brian, 20 November 2026:
"Make the helmets details with camo and markings on them").

Drawn from the historian's notes (a research pass per army and a second pass checking it, November 2026; the
notes are summed up in each finish's `about`). Rules the game keeps: no swastika (so no German eagle decal: its
left side stays plain paint), no rayed rising-sun flag, no MVSN or other fascist badge. The Polish, Soviet, Italian
royal-army, Japanese army and Chinese national badges are allowed. Colours are estimates from collectors' and
museum descriptions, not measured paint.

A finish:
  name, about       what it is, in a few words, and the notes behind it
  from, to          the months it was worn (YYYY-MM)
  season            "any", "summer" (snow-free months) or "winter"
  front             "any", "east", "west", "south" (Italy and the Balkans), "desert", "pacific", "china"
  share             how common it was among the men it fits then and there (the game weighs the finishes that fit)
  base              the shell's new colour (None: the paint as delivered); inside: the inside's ("same", None, a hex)
  paint             layers over the base, in order: blotch, wash, wear, dust, snow, fade, mud (hard: cut in with a
                    sharp edge, as brushed; soft: blended point to point, as sprayed or dusted)
  extras            decal, band, net, scrim, foliage, plume (variety.py's builders say what each takes)

Bearings are degrees round the helmet seen from above (0 ahead, 90 his left, -90 his right); `at` is degrees up
from the helmet's middle (about the level of his temples; 90 is the crown).
"""

PAINTS = {"SOVW": (5, 13)}       # (what counts as the headgear: the ushanka is cloth and fur)

LEAF = ["4E6B2C", "5C7A32", "3F5A25", "6F7F3B"]
LEAF_DRY = ["8A8A4A", "A3965E", "6B5A3A"]
GRASS = ["6F7F3B", "8A8A4A", "A3965E"]
STEEL = "3A3C38"
WORN_GREY = "414540"            # (field grey rubbed thin to the steel: darker, not black)

GER = [
    {"name": "plain", "about": "Issue M40 or M42 in rough matt field grey (the grit finish ordered March 1940), worn at the rim. "
     "The eagle decal on the left side is left off by the game's rule.",
     "from": "1940-03", "to": "1945-05", "share": 0.7,
     "paint": [{"kind": "wear", "hex": WORN_GREY}]},
    {"name": "apple green, national shield", "about": "Smooth M35 in apple green with the black-white-red shield on the right "
     "side under the vent (ordered off on 21 March 1940; most helmets in Poland, fewer in France).",
     "from": "1939-09", "to": "1940-06", "share": 0.7, "base": "66705A",
     "paint": [{"kind": "wear", "hex": "4E5446", "level": 0.15}],
     "extras": [{"kind": "decal", "decal": "shield_tricolour", "bearing": -90, "at": 6, "size": 0.034}]},
    {"name": "road dust", "about": "Plain helmet under a film of summer road dust (marches in the east).",
     "from": "1941-06", "to": "1944-08", "season": "summer", "front": "east", "share": 0.2,
     "paint": [{"kind": "dust", "hex": "625F55", "level": 0.3, "hard": False}, {"kind": "wear", "hex": WORN_GREY}]},
    {"name": "band", "about": "A loop of inner tube (or a bread-bag strap) round the dome to hold foliage: often worn empty.",
     "from": "1939-09", "to": "1945-05", "share": 0.12,
     "paint": [{"kind": "wear", "hex": WORN_GREY}],
     "extras": [{"kind": "band", "hex": "2A2A28", "at": 11, "wide": 0.024}]},
    {"name": "band and foliage", "about": "Inner-tube band with leafy twigs and grass pushed under it, densest at the front corners.",
     "from": "1939-09", "to": "1945-05", "season": "summer", "share": 0.15,
     "paint": [{"kind": "wear", "hex": WORN_GREY}],
     "extras": [{"kind": "band", "hex": "2A2A28", "at": 11, "wide": 0.024},
                {"kind": "foliage", "hex": LEAF, "count": 8, "at": [10, 13], "size": 0.11},
                {"kind": "foliage", "plant": "grass", "hex": GRASS, "count": 3, "at": [10, 13], "size": 0.12,
                 "bearings": [55, -55, 130]}]},
    {"name": "mud", "about": "Mud smeared on by hand to kill the shine (a soldier's habit all war).",
     "from": "1939-09", "to": "1945-05", "share": 0.05,
     "paint": [{"kind": "mud", "hex": "5A4D3B", "level": 0.05, "high": 0.55, "scale": 0.03},
               {"kind": "mud", "hex": "6E6250", "level": 0.35, "high": 0.9, "seed": 3, "scale": 0.03}]},
    {"name": "net, scrim and foliage", "about": "The issue helmet net (from August 1942) or a cut-down vehicle net, with hessian "
     "strips and sprigs through it.",
     "from": "1942-08", "to": "1945-05", "season": "summer", "share": 0.25,
     "paint": [{"kind": "wear", "hex": WORN_GREY}],
     "extras": [{"kind": "net", "hex": "6B6347", "mesh": 0.032, "under": "4F5650"},
                {"kind": "scrim", "hex": ["857558", "7A6E54"], "count": 5},
                {"kind": "foliage", "hex": LEAF + LEAF_DRY[:1], "count": 8, "at": [20, 60], "size": 0.1}]},
    {"name": "three-colour camouflage", "about": "Dark yellow sprayed over, with green and red-brown blotches in vehicle paint "
     "(paint ordered February 1943; most on helmets in Italy and Normandy).",
     "from": "1943-03", "to": "1945-05", "front": "any", "share": 0.15, "base": "BFA670",
     "paint": [{"kind": "blotch", "hex": "4F5536", "scale": 0.04, "level": 0.08, "hard": False},
               {"kind": "blotch", "hex": "5A3A2B", "scale": 0.035, "level": 0.22, "seed": 7, "hard": False},
               {"kind": "wear", "hex": "8C8158"}]},
    {"name": "wire and foliage", "about": "Chicken wire bent into a basket over a plain helmet and stuffed with hedge branches "
     "(the commonest Normandy look).",
     "from": "1944-06", "to": "1945-05", "season": "summer", "front": "west", "share": 0.4,
     "paint": [{"kind": "wear", "hex": WORN_GREY}],
     "extras": [{"kind": "net", "hex": "6E706C", "mesh": 0.026, "cord": 0.0012, "lift": 0.007, "under": "4F5650", "far_mix": 0.15},
                {"kind": "foliage", "hex": LEAF + ["6B5A3A"], "count": 15, "at": [10, 80], "size": 0.12, "far": 6}]},
    {"name": "whitewash", "about": "Lime or white paint brushed over the field grey in winter, rubbed back to grey at the "
     "rim and high points (less common the first winter).",
     "from": "1941-12", "to": "1945-03", "season": "winter", "share": 0.35, "inside": None,
     "paint": [{"kind": "wash", "hex": "E3E0D5", "level": -0.4, "under": False},
               {"kind": "mud", "hex": "BDB8AA", "level": 0.35, "high": 0.4, "under": False}]},
    {"name": "white cover", "about": "The white side of the issue snow cover, or a cut bedsheet tied round (from winter 1942).",
     "from": "1942-11", "to": "1945-03", "season": "winter", "share": 0.15, "base": "E8E6DC", "inside": None,
     "paint": [{"kind": "blotch", "hex": "D6D2C4", "scale": 0.03, "level": 0.25, "hard": False}],
     "extras": [{"kind": "band", "hex": "B9B4A4", "at": -8, "wide": 0.006, "thick": 0.002}]},
    {"name": "desert tan", "about": "Tan sprayed at the depots over the field grey (ordered March 1941), bleached on top and "
     "chipped grey at the edges; mostly smooth.",
     "from": "1941-06", "to": "1943-05", "front": "desert", "share": 0.85, "base": "8F7A45",
     "paint": [{"kind": "fade", "hex": "B09A6C", "level": 0.75, "hard": False}, {"kind": "wear", "hex": "6E6A52"}]},
    {"name": "sand, rough", "about": "The lighter sand (ordered March 1942), often with real sand in the paint.",
     "from": "1942-03", "to": "1943-05", "front": "desert", "share": 0.35, "base": "B49B6C",
     "paint": [{"kind": "blotch", "hex": "A68E60", "scale": 0.012, "level": 0.25, "hard": False},
               {"kind": "wear", "hex": "7C735A"}]},
]

POL = [   # (the wz.31, the helmet this file wears: the Adrian, most Poles' helmet in 1939, is another shape)
    {"name": "plain", "about": "Issue wz.31 in dark Polish khaki with cork in the paint, chipped to red-lead primer and steel. "
     "No badge: army helmets carried none after 1930.",
     "from": "1939-09", "to": "1939-10", "share": 0.6,
     "paint": [{"kind": "wear", "hex": "4A4A38"}, {"kind": "wear", "hex": "8E5A3C", "seed": 9, "level": 0.1, "band": 0.002}]},
    {"name": "September dust", "about": "The same helmet under the road dust of a dry September.",
     "from": "1939-09", "to": "1939-10", "share": 0.25,
     "paint": [{"kind": "dust", "hex": "887E61", "level": 0.3, "hard": False}, {"kind": "wear", "hex": "4A4A38"}]},
    {"name": "cord and foliage", "about": "Twine wound round the dome with leaves, grass and straw tucked under it (field practice, "
     "not issue).",
     "from": "1939-09", "to": "1939-10", "share": 0.08,
     "paint": [{"kind": "wear", "hex": "4A4A38"}],
     "extras": [{"kind": "band", "hex": "7A6B4F", "at": 12, "wide": 0.008, "thick": 0.002},
                {"kind": "foliage", "hex": LEAF[:2] + ["4F6B2E"], "count": 6, "at": [11, 14], "size": 0.1},
                {"kind": "foliage", "plant": "grass", "hex": ["9C8A55", "6E7A3A"], "count": 3, "at": [11, 14], "size": 0.13,
                 "bearings": [120, -120, 170]}]},
    {"name": "early smooth khaki", "about": "One of the first helmets (1932-33), in a smoother, lighter khaki.",
     "from": "1939-09", "to": "1939-10", "share": 0.05, "base": "76744F",
     "paint": [{"kind": "wear", "hex": "55543F", "level": 0.2}]},
    {"name": "coast grey", "about": "Grey wz.31 of the Navy and the coast defence (Hel, Oksywie, Gdynia).",
     "from": "1939-09", "to": "1939-10", "share": 0.02, "base": "5F6260",
     "paint": [{"kind": "wear", "hex": "45474A"}]},
]

SOV_WEAR = "3E4436"
SOV = [   # (the SSh-40, and the SSh-39 that looks the same at range)
    {"name": "plain", "about": "Plain issue green, no markings (the field helmet carried none), worn to steel at the rim.",
     "from": "1941-06", "to": "1945-08", "share": 0.75,
     "paint": [{"kind": "wear", "hex": SOV_WEAR}]},
    {"name": "summer dust", "about": "Pale dust on the top in dry summers (the 1942 steppe, Kursk).",
     "from": "1941-06", "to": "1945-08", "season": "summer", "share": 0.15,
     "paint": [{"kind": "dust", "hex": "747355", "level": 0.3, "hard": False}, {"kind": "wear", "hex": SOV_WEAR}]},
    {"name": "mud", "about": "Mud thrown up on the lower rim in the spring thaw and autumn rains, left on.",
     "from": "1941-06", "to": "1945-08", "share": 0.1,
     "paint": [{"kind": "mud", "hex": "5C4A36", "level": 0.25, "high": 0.35}, {"kind": "wear", "hex": SOV_WEAR}]},
    {"name": "red star", "about": "A red star of about 70 mm brushed on the front by some units (rare all war; never issue).",
     "from": "1939-11", "to": "1945-08", "share": 0.03,
     "paint": [{"kind": "wear", "hex": SOV_WEAR}],
     "extras": [{"kind": "decal", "decal": "red_star", "bearing": 0, "at": 33, "size": 0.07}]},
    {"name": "red star outline", "about": "The star drawn as a red line, a pre-war way still seen.",
     "from": "1939-11", "to": "1945-08", "share": 0.01,
     "paint": [{"kind": "wear", "hex": SOV_WEAR}],
     "extras": [{"kind": "decal", "decal": "red_star_outline", "bearing": 0, "at": 33, "size": 0.07}]},
    {"name": "whitewash", "about": "Whitened in the field with whatever was at hand (chalk wash, mostly), rubbed back to green "
     "at the rim and high points; worn most with the snow suit.",
     "from": "1941-11", "to": "1945-03", "season": "winter", "share": 0.12, "inside": None,
     "paint": [{"kind": "wash", "hex": "E4E1D6", "level": -0.4, "under": False},
               {"kind": "mud", "hex": "CBC7BA", "level": 0.35, "high": 0.4, "under": False}]},
    {"name": "white paint", "about": "White oil paint instead of chalk: brighter, and fuller.",
     "from": "1941-11", "to": "1945-03", "season": "winter", "share": 0.04, "inside": None,
     "paint": [{"kind": "wash", "hex": "E8ECEC", "level": -0.75, "under": False}]},
    {"name": "snow on the crown", "about": "Plain green with snow lying on top.",
     "from": "1941-11", "to": "1945-03", "season": "winter", "share": 0.25,
     "paint": [{"kind": "wear", "hex": SOV_WEAR}, {"kind": "snow", "hex": "EEF0F1", "level": 0.8, "hard": False}]},
    {"name": "fringe", "about": "A matting fringe wound round the helmet, grass-coloured tufts on it (scouts, snipers, men "
     "in defence).",
     "from": "1941-07", "to": "1945-08", "season": "summer", "share": 0.02,
     "paint": [{"kind": "wear", "hex": SOV_WEAR}],
     "extras": [{"kind": "band", "hex": "7A6E4C", "at": 14, "wide": 0.007, "thick": 0.002},
                {"kind": "band", "hex": "7A6E4C", "at": 22, "wide": 0.006, "thick": 0.002},
                {"kind": "foliage", "plant": "grass", "hex": ["6E7A3E", "8E8A52", "A99C63"], "count": 12, "at": [13, 23],
                 "size": 0.08, "blades": 5, "bearings": [0, 30, -30, 60, -60, 90, -90, 120, -120, 150, -150, 180],
                 "rise": 0.6, "out": 0.9, "back": 0.3, "far": 6}]},
    {"name": "draped net", "about": "The rifleman's camouflage net thrown over the helmet with grass woven in (recon troops).",
     "from": "1941-07", "to": "1945-08", "season": "summer", "share": 0.02,
     "extras": [{"kind": "net", "hex": "6E6650", "mesh": 0.026, "cord": 0.0018, "under": "4E5A3A"},
                {"kind": "foliage", "plant": "grass", "hex": ["6E7A3E", "8E8A52", "A99C63"], "count": 8, "at": [20, 70],
                 "size": 0.09}]},
]

SOVW = [   # (the ushanka: cloth crown, fur flaps)
    {"name": "red star", "about": "The red star badge on the front flap.",
     "from": "1941-11", "to": "1945-03", "share": 0.65,
     "extras": [{"kind": "decal", "decal": "red_star", "bearing": 0, "at": 12, "size": 0.032}]},
    {"name": "red star, snow on top", "about": "The same, with snow on the crown.",
     "from": "1941-11", "to": "1945-03", "share": 0.2,
     "paint": [{"kind": "snow", "hex": "EEF0F1", "level": 0.75, "hard": False}],
     "extras": [{"kind": "decal", "decal": "red_star", "bearing": 0, "at": 12, "size": 0.032}]},
    {"name": "plain", "about": "The badge lost, or a cap issued without one.",
     "from": "1941-11", "to": "1945-03", "share": 0.15},
]

SOVS = [   # (the snow suit's hood)
    {"name": "clean", "about": "The hood as issued: white cotton.", "share": 0.5, "from": "1941-11", "to": "1945-03"},
    {"name": "grubby", "about": "Gone grey and stained in the folds within weeks.", "share": 0.5, "from": "1941-11",
     "to": "1945-03",
     "paint": [{"kind": "blotch", "hex": "D6D2C6", "scale": 0.03, "level": -0.05, "hard": False},
               {"kind": "mud", "hex": "BFB9AA", "level": 0.3, "high": 0.3, "hard": False}]},
]

ITA_WEAR = "474B3E"
ITA = [   # (the M33)
    {"name": "plain", "about": "Plain grey-green: the arm-of-service stencil was not on every helmet. (Blackshirt, youth and "
     "M battalions wear only this, never a badge.)",
     "from": "1940-06", "to": "1943-09", "share": 0.3,
     "paint": [{"kind": "wear", "hex": ITA_WEAR}]},
    {"name": "infantry stencil", "about": "The infantry's badge stencilled in black on the front: crossed rifles under the "
     "royal crown, the regiment's number in the ring.",
     "from": "1940-06", "to": "1943-09", "share": 0.45,
     "paint": [{"kind": "wear", "hex": ITA_WEAR}],
     "extras": [{"kind": "decal", "decal": "ita_infantry", "bearing": 0, "at": 30, "size": 0.068}]},
    {"name": "Bersaglieri plume", "about": "The light infantry's black capercaillie feathers on the right side, sweeping back, "
     "and their stencil on the front.",
     "from": "1940-06", "to": "1943-09", "share": 0.06,
     "paint": [{"kind": "wear", "hex": ITA_WEAR}],
     "extras": [{"kind": "plume", "bearing": -95, "at": 12},
                {"kind": "decal", "decal": "ita_bersaglieri", "bearing": 0, "at": 30, "size": 0.066}]},
    {"name": "steppe dust", "about": "A film of pale steppe dust (the Don, summer 1942).",
     "from": "1942-06", "to": "1942-10", "front": "east", "season": "summer", "share": 0.5,
     "paint": [{"kind": "dust", "hex": "8F8C74", "level": 0.3, "hard": False}, {"kind": "wear", "hex": ITA_WEAR}],
     "extras": [{"kind": "decal", "decal": "ita_infantry", "bearing": 0, "at": 30, "size": 0.068}]},
    {"name": "Balkan mud", "about": "Mud and dirt of the Greek and Albanian winter.",
     "from": "1940-10", "to": "1941-04", "front": "south", "share": 0.4,
     "paint": [{"kind": "mud", "hex": "5A5040", "level": 0.2, "high": 0.5, "hard": False}, {"kind": "wear", "hex": ITA_WEAR}]},
    {"name": "frost and snow", "about": "Plain grey-green with snow on top: the usual winter look on the Don.",
     "from": "1941-11", "to": "1943-03", "season": "winter", "front": "east", "share": 0.4,
     "paint": [{"kind": "wear", "hex": ITA_WEAR}, {"kind": "snow", "hex": "E9EAE6", "level": 0.8, "hard": False}],
     "extras": [{"kind": "decal", "decal": "ita_infantry", "bearing": 0, "at": 30, "size": 0.068}]},
    {"name": "whitewash", "about": "Lime brushed over the grey-green, a minority look on the Don.",
     "from": "1941-11", "to": "1943-03", "season": "winter", "front": "east", "share": 0.05, "inside": None,
     "paint": [{"kind": "wash", "hex": "E6E4DC", "level": -0.35, "under": False}]},
    {"name": "net, scrim and foliage", "about": "A string net with burlap strips and scrub through it (Sicily, 1943).",
     "from": "1943-07", "to": "1943-08", "season": "summer", "front": "south", "share": 0.1,
     "paint": [{"kind": "wear", "hex": ITA_WEAR}],
     "extras": [{"kind": "net", "hex": "5E6340", "mesh": 0.03, "under": "5C6553"},
                {"kind": "scrim", "hex": ["A08D63", "6B6B45"], "count": 5},
                {"kind": "foliage", "hex": ["5A6B32", "6F7F3B", "9A8A5A"], "count": 6, "at": [20, 60], "size": 0.1}]},
]

ITAD = [   # (the same M33 in Africa: the file wears it in sand)
    {"name": "sand", "about": "Sand paint over the grey-green, the badge painted over; the green shows at the chips.",
     "from": "1941-01", "to": "1943-05", "share": 0.33,
     "paint": [{"kind": "wear", "hex": "6E6D55"}, {"kind": "fade", "hex": "C3AF85", "level": 0.8, "hard": False}]},
    {"name": "sand, stencil", "about": "Sand paint with the black badge stencilled again on top.",
     "from": "1941-01", "to": "1943-05", "share": 0.14,
     "paint": [{"kind": "wear", "hex": "6E6D55"}],
     "extras": [{"kind": "decal", "decal": "ita_infantry", "bearing": 0, "at": 30, "size": 0.068}]},
    {"name": "sand, masked badge", "about": "The badge masked when the sand went on, so it shows in an oval of the old grey-green.",
     "from": "1941-01", "to": "1943-05", "share": 0.08,
     "paint": [{"kind": "wear", "hex": "6E6D55"}],
     "extras": [{"kind": "decal", "decal": "ita_oval", "bearing": 0, "at": 30, "size": 0.075}]},
    {"name": "grey-green", "about": "Never repainted: grey-green under the desert dust (most helmets in 1941).",
     "from": "1941-01", "to": "1943-05", "share": 0.25, "base": "5C6553",
     "paint": [{"kind": "dust", "hex": "8F8A6E", "level": 0.3, "hard": False}, {"kind": "wear", "hex": ITA_WEAR}]},
    {"name": "grey-green, stencil", "about": "Grey-green with its black badge, dusty.",
     "from": "1941-01", "to": "1943-05", "share": 0.15, "base": "5C6553",
     "paint": [{"kind": "dust", "hex": "8F8A6E", "level": 0.3, "hard": False}, {"kind": "wear", "hex": ITA_WEAR}],
     "extras": [{"kind": "decal", "decal": "ita_infantry", "bearing": 0, "at": 30, "size": 0.068}]},
    {"name": "Bersaglieri plume", "about": "Sand helmet with the light infantry's feathers.",
     "from": "1941-01", "to": "1943-05", "share": 0.05,
     "paint": [{"kind": "wear", "hex": "6E6D55"}],
     "extras": [{"kind": "plume", "bearing": -95, "at": 12}]},
    {"name": "sand, brown blotches", "about": "A few soft brown blotches over the sand (uncommon).",
     "from": "1941-06", "to": "1943-05", "share": 0.02,
     "paint": [{"kind": "blotch", "hex": "7A5A3A", "scale": 0.04, "level": 0.3, "hard": False}, {"kind": "wear", "hex": "6E6D55"}]},
    {"name": "hessian cover", "about": "A cover of sandbag cloth tied over the helmet.",
     "from": "1941-06", "to": "1943-05", "share": 0.03, "base": "A8936A", "inside": None,
     "paint": [{"kind": "blotch", "hex": "9A8560", "scale": 0.02, "level": 0.2, "hard": False}],
     "extras": [{"kind": "band", "hex": "7A6B4F", "at": -8, "wide": 0.005, "thick": 0.002}]},
    {"name": "net and scrim", "about": "A net with burlap strips, little foliage (Tunisia).",
     "from": "1942-11", "to": "1943-05", "share": 0.05,
     "extras": [{"kind": "net", "hex": "7A6B4F", "mesh": 0.03, "under": "B39E73"},
                {"kind": "scrim", "hex": ["A08D63", "8C7B57"], "count": 6}]},
]

JAP = [   # (the soft field cap this file wears, with its neck curtain)
    {"name": "yellow star", "about": "The Army's yellow cloth star on the cap front.",
     "from": "1937-07", "to": "1945-08", "share": 0.7,
     "extras": [{"kind": "decal", "decal": "yellow_star", "bearing": 0, "at": 22, "size": 0.028}]},
    {"name": "brass star", "about": "A brass star instead of the cloth one (officers', and many later caps).",
     "from": "1937-07", "to": "1945-08", "share": 0.15,
     "extras": [{"kind": "decal", "decal": "brass_star", "bearing": 0, "at": 22, "size": 0.026}]},
    {"name": "head net and grass", "about": "The issued head net drawn over the cap, with grass or straw through it.",
     "from": "1937-07", "to": "1945-08", "season": "summer", "share": 0.15,
     "extras": [{"kind": "decal", "decal": "yellow_star", "bearing": 0, "at": 22, "size": 0.028},
                {"kind": "net", "hex": "8F8A5C", "mesh": 0.045, "cord": 0.0028, "lift": 0.005, "from": 8, "under": "9C8656"},
                {"kind": "foliage", "plant": "grass", "hex": ["B8A468", "A89870", "6E7F3A"], "count": 7, "at": [25, 70],
                 "size": 0.13}]},
    {"name": "net and jungle leaves", "about": "Branches and fronds stuffed into the net until the head is a bush (the "
     "Solomons, New Guinea, Burma).",
     "from": "1942-01", "to": "1945-08", "front": "pacific", "share": 0.3,
     "extras": [{"kind": "decal", "decal": "yellow_star", "bearing": 0, "at": 22, "size": 0.028},
                {"kind": "net", "hex": "5E6340", "mesh": 0.045, "cord": 0.0028, "lift": 0.005, "from": 8, "under": "9C8656"},
                {"kind": "foliage", "hex": ["3F5A2A", "556B2F", "2F4420", "7A8A3A", "8A7A3A"], "count": 12, "at": [20, 80],
                 "size": 0.16, "leaves": 7, "leaf": 0.45, "far": 5}]},
]

CHN = [   # (the soft field cap this file wears)
    {"name": "sun badge", "about": "The round cap badge: a white sun of twelve rays on blue (the national emblem; not a rising-sun flag).",
     "from": "1937-07", "to": "1945-08", "share": 0.5,
     "extras": [{"kind": "decal", "decal": "kmt_sun", "bearing": 0, "at": 22, "size": 0.03}]},
    {"name": "khaki cap", "about": "The yellow-green or khaki summer cap of the Central Army, with its badge.",
     "from": "1937-07", "to": "1945-08", "season": "summer", "share": 0.4, "base": "8E8A5C",
     "extras": [{"kind": "decal", "decal": "kmt_sun", "bearing": 0, "at": 22, "size": 0.03}]},
    {"name": "green cap", "about": "A green cap from another dye lot, with its badge.",
     "from": "1937-07", "to": "1945-08", "share": 0.08, "base": "6B6B48",
     "extras": [{"kind": "decal", "decal": "kmt_sun", "bearing": 0, "at": 22, "size": 0.03}]},
    {"name": "no badge", "about": "The badge lost.",
     "from": "1937-07", "to": "1945-08", "share": 0.06},
    {"name": "string and grass", "about": "String tied round the cap with grass tucked in (improvised; uncommon).",
     "from": "1937-07", "to": "1945-08", "season": "summer", "share": 0.04, "base": "8E8A5C",
     "extras": [{"kind": "decal", "decal": "kmt_sun", "bearing": 0, "at": 22, "size": 0.03},
                {"kind": "band", "hex": "A89870", "at": 30, "wide": 0.005, "thick": 0.002},
                {"kind": "foliage", "plant": "grass", "hex": ["6E7F3A", "B8A468"], "count": 5, "at": [29, 33], "size": 0.12,
                 "bearings": [40, -40, 90, -90, 150]}]},
]

USA_OD = "4C4E37"                # (the M1's olive drab, as the file paints it)
USA_WEAR = "3B3D30"              # (the textured paint rubbed thin at the rim: darker, toward the steel)
BURLAP = ["A08C64", "8E7B55", "77744A", "6B6A44"]    # (raw burlap, and hessian dyed green and brown)
USA = [   # (the M1, both files: the rifleman and the BAR man)
    {"name": "plain olive drab", "about": "Issue M1 in rough olive drab (cork or sawdust in the paint to kill the shine), the "
     "rim and chin-strap loops rubbed toward the steel. No badge as issued.",
     "from": "1942-01", "to": "1945-08", "share": 0.4,
     "paint": [{"kind": "wear", "hex": USA_WEAR}]},
    {"name": "net", "about": "A helmet net over the plain helmet (American fine-mesh nets, or the wider British-made ones "
     "issued in England before the landings), worn bare.",
     "from": "1943-07", "to": "1945-08", "share": 0.25,
     "paint": [{"kind": "wear", "hex": USA_WEAR}],
     "extras": [{"kind": "net", "hex": "5A5A3E", "mesh": 0.03, "under": USA_OD}]},
    {"name": "net and burlap scrim", "about": "The 1944 Normandy look: the wide British-made net with strips of burlap and "
     "hessian, green and brown, pushed through it and hanging past the rim.",
     "from": "1944-05", "to": "1945-05", "front": "west", "season": "summer", "share": 0.45,
     "paint": [{"kind": "wear", "hex": USA_WEAR}],
     "extras": [{"kind": "net", "hex": "5C5A40", "mesh": 0.038, "under": USA_OD, "far_mix": 0.35},
                {"kind": "scrim", "hex": BURLAP, "count": 11, "wide": 0.026}]},
    {"name": "net and hedgerow leaves", "about": "Net with sprigs from the hedgerows of the bocage stuck through it "
     "(Normandy, June to August 1944).",
     "from": "1944-06", "to": "1944-10", "front": "west", "season": "summer", "share": 0.2,
     "paint": [{"kind": "wear", "hex": USA_WEAR}],
     "extras": [{"kind": "net", "hex": "5C5A40", "mesh": 0.038, "under": USA_OD},
                {"kind": "scrim", "hex": BURLAP[:2], "count": 4, "wide": 0.022},
                {"kind": "foliage", "hex": LEAF + LEAF_DRY[:1], "count": 11, "at": [8, 40], "size": 0.075}]},
    {"name": "net and jungle leaves", "about": "Net with broad leaves through it, in the Pacific and Burma.",
     "from": "1943-01", "to": "1945-08", "front": "pacific", "share": 0.25,
     "paint": [{"kind": "wear", "hex": USA_WEAR}],
     "extras": [{"kind": "net", "hex": "56583C", "mesh": 0.03, "under": USA_OD},
                {"kind": "foliage", "hex": LEAF, "count": 8, "at": [8, 40], "size": 0.095}]},
    {"name": "sergeant's bar", "about": "A white bar painted across the back, the mark of a sergeant (officers wore it "
     "upright), as many units painted them in Europe from 1944.",
     "from": "1944-03", "to": "1945-08", "front": "west", "share": 0.08,
     "paint": [{"kind": "wear", "hex": USA_WEAR}],
     "extras": [{"kind": "decal", "decal": "white_bar_h", "bearing": 180, "at": 14, "size": 0.07}]},
    {"name": "road dust", "about": "The plain helmet under the white dust of the Normandy lanes in the summer breakout.",
     "from": "1944-07", "to": "1944-09", "front": "west", "season": "summer", "share": 0.15,
     "paint": [{"kind": "dust", "hex": "8C8670", "level": 0.3, "hard": False}, {"kind": "wear", "hex": USA_WEAR}]},
    {"name": "mud", "about": "Mud smeared on by hand (the autumn and winter of 1944 in Lorraine and the Hurtgen).",
     "from": "1942-11", "to": "1945-05", "share": 0.05,
     "paint": [{"kind": "mud", "hex": "4F4433", "level": 0.05, "high": 0.55, "scale": 0.03},
               {"kind": "mud", "hex": "675B47", "level": 0.35, "high": 0.9, "seed": 3, "scale": 0.03}]},
    {"name": "whitewash", "about": "Whitewash or white paint brushed over in the Ardennes snow (December 1944 to February "
     "1945), rubbed back to the drab at the rim.",
     "from": "1944-12", "to": "1945-02", "front": "west", "season": "winter", "share": 0.3, "inside": None,
     "paint": [{"kind": "wash", "hex": "E3E0D5", "level": -0.4, "under": False},
               {"kind": "mud", "hex": "B8B5A4", "level": 0.35, "high": 0.4, "under": False}]},
    {"name": "white cloth", "about": "A piece of bedsheet or mattress cover tied over the helmet in the Ardennes.",
     "from": "1944-12", "to": "1945-02", "front": "west", "season": "winter", "share": 0.15, "base": "E8E6DC", "inside": None,
     "paint": [{"kind": "blotch", "hex": "D6D2C4", "scale": 0.03, "level": 0.25, "hard": False}],
     "extras": [{"kind": "band", "hex": "B9B4A4", "at": -8, "wide": 0.006, "thick": 0.002}]},
]

# The medic (USA_medic): his own helmet carries the red cross in a white disc on four sides (front, back and both
# sides, as units painted them, larger from 1944), made in the file as its own paints (15 white, 16 red: his
# `helmet_paints`). A finish keeps a paint other than the helmet's as it is, so the crosses show on every finish; he
# wears no net (medics kept the crosses seen) and no white cloth over them.
USAM = [dict(r, about=r["about"] + " His red crosses in white discs are left showing.")
        for r in USA if r["name"] not in ("sergeant's bar", "white cloth")
        and not any(x["kind"] == "net" for x in r.get("extras", []))]

# The squad leader (USA_nco): a sergeant's bar on the back of every helmet he wears in Europe (his own helmet has
# none painted), the plain one first. Not on the white finishes (it would not show) nor in the Pacific, where few
# units painted them.
SGT_BAR = {"kind": "decal", "decal": "white_bar_h", "bearing": 180, "at": 14, "size": 0.07}
USAN = [dict(r, name=r["name"] + ", sergeant's bar", extras=r.get("extras", []) + [SGT_BAR],
             about=r["about"] + " With the sergeant's white bar across the back.")
        if r.get("front", "any") in ("any", "west") and r.get("season", "any") != "winter" else r
        for r in USA if r["name"] != "sergeant's bar"]

# The paratrooper (USA_para): his own M2 helmet already wears its net and burlap scrim (paints 21 net, 13 and 20, in
# his `helmet_paints` with the shell), so no finish adds another net or scrim, nor a white cloth that would hide them;
# a finish recolours the shell under them, which shows between the net's cords.
USAP = [dict(r, about=r["about"] + " His own net and scrim stay over it.")
        for r in USA if r["name"] not in ("sergeant's bar", "white cloth")
        and not any(x["kind"] in ("net", "scrim") for x in r.get("extras", []))]

RECIPES = {"GER": GER, "POL": POL, "SOV": SOV, "SOVW": SOVW, "SOVS": SOVS, "ITA": ITA, "ITAD": ITAD, "JAP": JAP, "CHN": CHN,
           "USA": USA, "USAB": USA, "USAZ": USA, "USAL": USA, "USAN": USAN, "USAM": USAM, "USAR": USA,
           "USAP": USAP}
