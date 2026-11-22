"""What every land vehicle and gun could do, as measured: armour by facing, the gun by range, speed, ground
pressure, rate of fire, reliability. One file for the battle rules and the battle scene, keyed by the names in
index-land.json:

    python performance.py [out folder]        (default: /mnt/project-files/battle-models/land/)

writes performance-land.json there. Nothing here changes a model's own file.

Brian, 22 Nov: the named tanks (Tiger, Panther, T-34, Sherman, Panzer IV ...) have real advantages from their
measured performance against their peers; German tanks are far weaker at the back than in front, so a hit there
pays; the vehicles every small nation shares vary a little from country to country.

Where the numbers come from. The armour plates (thickness, slope from the vertical) and the guns' piercing tables
are the standard published figures for each type, set down from memory by the modeller: every row carries `sure`
(high, medium, low) and the historian checks them. Piercing is each nation's own table at 30 degrees from square-on
against rolled plate; the nations judged a "pierce" a little differently, so across nations it is good to about one
part in ten. `fire` (does she burn when pierced) and `reliability` are judgments from wartime surveys, not
measurements, and say so.

How a shot is judged (the same rule for every pair, so the rules need no ballistics of their own):

    square  = pierce30(range) x 1.2                     what the shot pierces square-on
    need    = mm x quality / cos(slope) ^ (1 + k)       k = 0.5 x mm / calibre - 0.2, held in 0 .. 0.5
                                                        (a slope cheats a small shot more than a big one)
    chance  = smooth step of (square / need): 0 at 0.85, half at 1.00, sure at 1.15
    a face  = the chances on its plates, each by its share of the face as a gunner sees it

`duels` holds that worked out for every gun against every armoured vehicle, face by face, at 100, 500, 1000, 1500
and 2000 m; `reach` is the farthest range at which the chance is still one in two (0: never; 2500: beyond the table).
"""
import json
import math
import os
import sys
import zlib

RANGES = [100, 500, 1000, 1500, 2000]

# ---- the guns ------------------------------------------------------------------------------------------------
# pierce: mm of rolled plate at 30 degrees from square-on, at RANGES. heat: a hollow-charge shell's piercing (the
# same at any range, to `heat_to` metres). he: the high-explosive shell's weight, kg (None: she has none).
# filler: the armour-piercing shell bursts after it pierces (kills more surely than a solid shot).


def G(name, mm, mv, pierce, shell, he=None, filler=True, heat=None, heat_to=800, heat_from=None, sure="medium", note="", later=()):
    return {"name": name, "calibre_mm": mm, "muzzle_velocity": mv, "shell": shell, "pierce_30deg_mm": pierce, "he_kg": he,
            "bursts_inside": filler, "heat_mm": heat, "heat_to_m": heat_to if heat else None, "heat_from": heat_from if heat else None,
            "heat_sure": "low" if heat else None,           # (the historian found the hollow-charge dates on no page: the modeller's reading)
            "later_rounds": [R(pierce, *r) for r in later], "sure": sure, "note": note}


def R(base, month, shell, at_1000_m, scarce=False, sure="medium", only=None):
    """A better round issued later: from `month` the gun's whole piercing row is multiplied by `times` (what the new
    round pierces at 1,000 m over what the first one did). scarce: a few rounds a gun, not the standard load.
    only: the nations it is for, where the gun stands for several armies' guns."""
    row = {"from": month, "shell": shell, "pierce_30deg_at_1000_m": at_1000_m, "times": round(at_1000_m / base[2], 2), "scarce": scarce, "sure": sure}
    if only:
        row["only"] = list(only)
    return row


GUNS = {
    # Germany
    "kwk37": G("7.5 cm KwK 37 L/24", 75, 385, [41, 39, 35, 33, 30], "K.Gr.rot Pz.", he=5.7, heat=75, heat_from="1942-03", sure="high", note="a short support gun; hollow charge from 1942 (about 70 mm at first, 75 late in 1942, 100 in 1943: the dates are the weakest part)"),
    "kwk40_43": G("7.5 cm KwK 40 L/43", 75, 740, [98, 91, 82, 72, 63], "Pz.Gr. 39", he=5.7, sure="high"),
    "kwk40_48": G("7.5 cm KwK 40 L/48", 75, 790, [106, 96, 85, 74, 64], "Pz.Gr. 39", he=5.7, sure="high"),
    "pak40": G("7.5 cm Pak 40 L/46", 75, 792, [108, 96, 85, 74, 64], "Pz.Gr. 39", he=5.7, sure="high"),
    "kwk42": G("7.5 cm KwK 42 L/70", 75, 935, [138, 124, 111, 99, 89], "Pz.Gr. 39/42", he=5.7, sure="high"),
    "kwk36": G("8.8 cm KwK 36 L/56", 88, 773, [120, 110, 100, 91, 84], "Pz.Gr. 39", he=9.4, sure="high"),
    "kwk30": G("2 cm KwK 30/38 L/55", 20, 780, [20, 14, 9, 0, 0], "Pz.Gr.", he=0.12, filler=False, sure="medium"),
    "lefh18": G("10.5 cm leFH 18", 105, 470, [56, 52, 49, 46, 43], "Pz.Gr. rot", he=14.8, heat=100, heat_to=1000, heat_from="1942-01", sure="low", note="a howitzer: the armour-piercing table is the weakest figure here"),
    "sfh18": G("15 cm sFH 18/1 L/30", 150, 495, [0, 0, 0, 0, 0], "none", he=43.5, sure="high", note="a heavy howitzer; her shell's burst breaks about 40 mm of plate"),
    "mg34": G("7.92 mm MG 34", 7.92, 765, [9, 5, 0, 0, 0], "S.m.K.", filler=False, sure="medium"),
    # Soviet Union
    "f34": G("76.2 mm F-34 L/41.5 (and the KV's ZiS-5)", 76.2, 662, [66, 59, 50, 43, 37], "BR-350A", he=6.2, sure="medium", note="Soviet tables count a pierce more strictly than German ones"),
    "zis3": G("76.2 mm ZiS-3 L/42.6", 76.2, 680, [68, 61, 52, 45, 38], "BR-350A", he=6.2, sure="medium"),
    "zis_s53": G("85 mm ZiS-S-53 / D-5", 85, 792, [97, 91, 83, 75, 69], "BR-365", he=9.5, sure="medium"),
    "k20": G("45 mm 20-K L/46", 45, 760, [43, 35, 28, 22, 17], "BR-240", he=2.1, sure="medium"),
    # United States
    "m3_75": G("75 mm M3 L/40", 75, 619, [72, 65, 59, 54, 49], "M61 APC", he=6.7, sure="high", note="the best high-explosive shell of any tank gun of its size"),
    "m7_76": G("3-inch M7 L/50", 76.2, 792, [98, 92, 87, 81, 74], "M62 APC", he=5.8, sure="high",
                 later=[("1944-09", "M93 HVAP (tungsten core)", 133, True)], note="M93: 135 mm at 1,000 yards, from late 1944, a few rounds a gun"),
    "m6_37": G("37 mm M6 L/53.5", 37, 884, [60, 52, 45, 39, 34], "M51 APC", he=0.73, filler=False, sure="high"),
    "m2a1_105": G("105 mm Howitzer M2A1", 105, 472, [0, 0, 0, 0, 0], "none", he=15.0, heat=102, heat_to=1000, heat_from="1942-06", sure="high"),
    "m2_50": G(".50 M2 machine gun", 12.7, 887, [20, 15, 8, 0, 0], "AP M2", filler=False, sure="medium"),
    # Britain
    "qf2": G("QF 2-pounder L/52", 40, 792, [53, 46, 38, 31, 25], "AP shot", he=None, filler=False, sure="medium",
               note="its high-explosive shell was not made until late 1942 and rarely issued: helpless against a dug-in gun. A capped shot came later (about 49 mm at 1,000 yards)"),
    "qf25": G("QF 25-pounder", 87.6, 532, [72, 66, 55, 46, 38], "AP shot (609 m/s on the top charge)", he=11.3, filler=False, sure="low"),
    "qf6": G("QF 6-pounder L/50 (57 mm)", 57, 831, [88, 80, 72, 62, 54], "AP shot (1942)", he=3.0, filler=False, sure="medium",
               later=[("1943-01", "APCBC", 88), ("1944-03", "APDS (discarding sabot)", 140, True)], note="the sabot shot was scarce and less accurate"),
    # France
    "sa35_47": G("47 mm SA 35 L/32", 47, 660, [45, 38, 30, 23, 18], "obus de rupture Mle 1935", he=1.4, sure="medium"),
    "sa35_75": G("75 mm SA 35 L/17 (in the hull)", 75, 220, [42, 40, 35, 0, 0], "obus de rupture Mle 1910M", he=5.5, sure="low", note="laid by turning the whole tank"),
    "mle1897": G("75 mm Mle 1897", 75, 570, [60, 52, 45, 39, 33], "obus de rupture Mle 1910M", he=6.2, sure="low"),
    # Italy
    "c47_32": G("47/32 Mod. 35", 47, 630, [46, 37, 28, 21, 16], "granata perforante", he=2.4, sure="medium"),
    "c75_18": G("75/18 Mod. 34", 75, 425, [42, 38, 33, 29, 25], "granata perforante", he=6.4, heat=70, heat_from="1942-04", sure="low", note="hollow charge (Effetto Pronto) from 1942"),
    "breda38": G("8 mm Breda 38 (a pair)", 8, 790, [8, 5, 0, 0, 0], "perforante", filler=False, sure="low"),
    # Japan
    "t97_57": G("57 mm Type 97 L/18", 57, 350, [25, 20, 15, 12, 0], "Type 92 APHE", he=2.6, sure="low", note="an infantry support gun: little use against a tank"),
    "t1_47": G("47 mm Type 1 L/48", 47, 810, [60, 52, 40, 31, 24], "Type 1 APHE", he=1.4, sure="low", note="Japan's one real tank-killing gun: through a Sherman's side at battle range, not her front"),
    # Poland
    "wz37": G("37 mm wz. 37 (Bofors) L/45", 37, 800, [40, 30, 22, 15, 10], "wz. 36 AP", he=0.7, filler=False, sure="medium"),
    # Our own designs (the vehicles every small nation shares): in the manner of their years, of no one maker
    "any37": G("37 mm gun (our own light tank and armoured car)", 37, 750, [36, 29, 22, 16, 12], "armour-piercing shell", he=0.7, filler=False, sure="design"),
    "any50": G("50 mm gun (our own medium tank)", 50, 685, [54, 46, 36, 28, 22], "capped armour-piercing shell", he=1.8, sure="design"),
    "any75": G("75 mm gun (our own assault gun)", 75, 600, [78, 70, 62, 55, 48], "capped armour-piercing shell", he=6.0, sure="design"),
    "any57": G("57 mm anti-tank gun (the one every other army shares)", 57, 820, [86, 78, 66, 56, 47], "capped armour-piercing shell", he=2.7, filler=False, sure="medium",
                 later=[("1943-01", "APCBC", 88, False, "medium", ("UK", "USA")), ("1944-03", "APDS (discarding sabot)", 140, True, "medium", ("UK", "USA"))],
                 note="as the American M1 and the British 6-pounder it stands for; the later rounds are theirs alone"),
    "any20": G("20 mm automatic gun", 20, 830, [22, 15, 9, 0, 0], "armour-piercing", he=0.12, filler=False, sure="design"),
    "any75w": G("75 mm gun (what-if tank hunter and breakthrough tank)", 75, 780, [104, 94, 83, 72, 62], "capped armour-piercing shell", he=5.8, sure="design"),
    "any37w": G("37 mm gun (what-if light classes)", 37, 800, [44, 36, 28, 21, 15], "armour-piercing shell", he=0.7, filler=False, sure="design"),
    "flame": G("flame projector", 0, 0, [0, 0, 0, 0, 0], "burning oil, to about 100 m", he=None, filler=False, sure="design", note="kills men and open-topped vehicles at close range; nothing to a closed tank's plate"),
}

# ---- the vehicles ----------------------------------------------------------------------------------------------
# A face is a list of plates: (what, mm, slope in degrees from the vertical, share of the face a gunner sees).
# fire: (front, side, rear) how often she burns when a shot comes through that face. explodes: of those fires, how many
# end with her ammunition going up. command: how well her crew can fight her (three men in the turret with a cupola
# and a radio is 1.0; a commander who also lays or loads the gun, less; one man alone in the turret, far less).


def V(real, kind, t, hp, road, cross, km, gp, soft, front, side, rear, top, gun, rpm, ammo, turn, crew, turret_crew,
      radio, optics, command, reliab, fuel, fire, explodes, high, sure, edge, flaw, quality=1.0, open_top=False, arc=360, note="", gun2=None):
    return {"stands_for": real, "kind": kind, "weight_t": t, "power_hp": hp, "hp_per_t": round(hp / t, 1) if t else None,
            "road_kph": road, "cross_country_kph": cross, "range_km": km, "ground_pressure_kg_cm2": gp, "soft_ground": soft,
            "armour": {"front": front, "side": side, "rear": rear, "top_mm": top, "quality": quality, "open_top": open_top},
            "gun": gun, "second_gun": gun2, "rounds_a_minute": rpm, "rounds_carried": ammo, "gun_arc_degrees": arc, "turret_degrees_a_second": turn,
            "crew": crew, "turret_crew": turret_crew, "radio": radio, "optics": optics, "command": command,
            "reliability": reliab, "fuel": fuel, "fire_when_pierced": dict(zip(("front", "side", "rear"), fire)), "explodes_of_fires": explodes,
            "height_m": high, "sure": sure, "edge": edge, "flaw": flaw, "note": note}


P = lambda what, mm, slope, share: (what, mm, slope, share)   # noqa: E731

VEHICLES = {
    # ---------------------------------------------------------------- Germany
    "Tiger": V("Panzerkampfwagen VI Tiger Ausf. E", "heavy tank", 57.0, 700, 38, 20, 140, 1.05, 0.65,
               [P("turret front and mantlet", 110, 0, 0.35), P("driver's plate", 100, 9, 0.35), P("nose", 100, 25, 0.25), P("glacis", 60, 80, 0.05)],
               [P("turret side", 80, 0, 0.35), P("upper hull side", 80, 0, 0.40), P("lower hull side, behind the wheels", 60, 0, 0.25)],
               [P("turret rear", 80, 0, 0.35), P("hull rear", 80, 9, 0.65)], 25,
               "kwk36", 6, 92, 10, 5, 3, 1.0, 1.25, 1.05, 0.60, "petrol", (0.45, 0.65, 0.75), 0.25, 2.93, "high",
               "100 mm in front and a gun that kills any tank of 1943 at 1,500 m: from the front she is all but proof against the 75 and 76 mm guns she meets",
               "her turret takes a minute to turn all the way round with the engine idling (about 20 seconds with it revved), she is slow, thirsty and breaks down; her side and rear are 80 mm, a fifth thinner than her front, with the engine, the petrol and the ammunition bins behind them",
               note="her turret is driven off the engine: 6 degrees a second in low gear, up to 19 in high at 2,000 rpm. 10 is a working value; her model file still says 6, to mend at her next export"),
    "Panther": V("Panzerkampfwagen V Panther Ausf. A and G", "medium tank", 44.8, 700, 46, 24, 200, 0.88, 0.80,
                 [P("turret mantlet", 100, 12, 0.30), P("glacis", 80, 55, 0.50), P("nose", 60, 55, 0.20)],
                 [P("turret side", 45, 25, 0.30), P("upper hull side", 50, 30, 0.35), P("lower hull side", 40, 0, 0.35)],
                 [P("turret rear", 45, 25, 0.35), P("hull rear", 40, 30, 0.65)], 16,
                 "kwk42", 6, 79, 15, 5, 3, 1.0, 1.25, 1.05, 0.55, "petrol", (0.45, 0.65, 0.80), 0.30, 2.99, "high",
                 "the best gun and the best frontal armour of any medium tank: her sloped glacis turns every Allied tank gun of 1943-44 but the biggest, and she is fast across soft ground",
                 "a glass back: 40 to 50 mm on her sides and rear, a third of what her front is worth, so almost any gun kills her from the flank; her final drives fail and her engine bay burns",
                 note="the true weak-backed German tank: side and rear are worth about 50 mm against a front worth about 130. The plates are the Ausf. G's; the Ausf. A's upper hull side was 40 mm at 40 degrees, worth about the same"),
    "PanzerIV": V("Panzerkampfwagen IV Ausf. H", "medium tank", 25.0, 300, 38, 16, 210, 0.89, 0.60,
                  [P("turret front and mantlet", 50, 10, 0.30), P("driver's plate", 80, 10, 0.40), P("nose", 80, 14, 0.25), P("glacis", 20, 72, 0.05)],
                  [P("turret side", 30, 26, 0.30), P("superstructure side", 30, 0, 0.40), P("lower hull side", 30, 0, 0.30)],
                  [P("turret rear", 30, 15, 0.35), P("hull rear", 20, 10, 0.65)], 12,
                  "kwk40_48", 10, 87, 14, 5, 3, 1.0, 1.2, 1.05, 0.85, "petrol", (0.50, 0.70, 0.75), 0.30, 2.68, "high",
                  "a long 75 mm that kills a T-34 or a Sherman from the front at 1,000 m, a three-man turret, good sights and a radio in every tank",
                  "thin and upright everywhere but her hull front: 50 mm on the turret face, 30 mm on her sides and only 20 mm on her tail",
                  note="the skirts are thin spaced plate against anti-tank rifles and hollow charges: they do nothing to a tank gun's shot"),
    "PanzerIV:short": V("Panzerkampfwagen IV Ausf. F1 (the short gun, to early 1942)", "medium tank", 22.3, 300, 42, 20, 200, 0.79, 0.65,
                        [P("turret front and mantlet", 50, 10, 0.30), P("driver's plate", 50, 10, 0.40), P("nose", 50, 14, 0.25), P("glacis", 20, 72, 0.05)],
                        [P("turret side", 30, 26, 0.30), P("superstructure side", 30, 0, 0.40), P("lower hull side", 30, 0, 0.30)],
                        [P("turret rear", 30, 15, 0.35), P("hull rear", 20, 10, 0.65)], 11,
                        "kwk37", 10, 80, 14, 5, 3, 1.0, 1.2, 1.05, 0.88, "petrol", (0.50, 0.70, 0.75), 0.30, 2.68, "high",
                        "a fine high-explosive gun for breaking infantry and guns, the same three-man turret and radio",
                        "her short gun cannot pierce a T-34 or a Matilda from the front at any range",
                        note="the same model with the gun swap `short`; with the 1942 long L/43 gun use this row's armour and gun kwk40_43"),
    "StuG3": V("Sturmgeschuetz III Ausf. G", "assault gun", 23.9, 300, 40, 19, 155, 0.93, 0.60,
               [P("superstructure front and mantlet", 80, 10, 0.55), P("nose", 80, 21, 0.45)],
               [P("superstructure side", 30, 11, 0.50), P("hull side", 30, 0, 0.50)],
               [P("superstructure rear", 30, 0, 0.30), P("hull rear", 50, 10, 0.70)], 11,
               "kwk40_48", 9, 54, 6, 4, 0, 1.0, 1.2, 0.95, 0.88, "petrol", (0.50, 0.65, 0.75), 0.30, 2.16, "high",
               "the Panzer IV's gun and 80 mm in front in a hull barely two metres high: hard to see, hard to hit, deadly from ambush",
               "no turret: her gun swings only 10 degrees each way, so a tank on her flank is on her blind side, and her sides are 30 mm",
               arc=20),
    "Hummel": V("15 cm Panzerhaubitze Hummel", "self-propelled howitzer", 24.0, 300, 42, 20, 215, 0.80, 0.55,
                [P("fighting compartment front", 10, 37, 0.50), P("hull front", 30, 20, 0.50)],
                [P("fighting compartment side", 10, 16, 0.60), P("hull side", 20, 0, 0.40)],
                [P("fighting compartment rear", 10, 10, 0.50), P("hull rear", 20, 10, 0.50)], 0,
                "sfh18", 3, 18, 3, 6, 0, 1.0, 1.1, 0.9, 0.80, "petrol", (0.6, 0.7, 0.75), 0.45, 2.81, "medium",
                "a 150 mm howitzer that keeps up with the tanks: a 43 kg shell out to 13 km",
                "an open box of 10 mm plate that stops rifle bullets and nothing more; only 18 shells aboard", open_top=True, arc=30),
    "Hanomag": V("Sd.Kfz. 251 half-track", "armoured carrier", 7.8, 100, 52, 21, 300, 0.0, 0.60,
                 [P("nose and driver's plate", 14.5, 30, 1.0)], [P("side", 8, 35, 1.0)], [P("rear doors", 8, 30, 1.0)], 0,
                 "mg34", 0, 0, 0, 2, 0, 0.8, 1.0, 1.0, 0.85, "petrol", (0.5, 0.6, 0.6), 0.1, 1.75, "high",
                 "carries ten men under sloped plate that stops rifle fire and splinters, across ground a lorry cannot cross",
                 "open on top and proof against nothing bigger than a rifle bullet", open_top=True),
    "SdKfz222": V("Sd.Kfz. 222 light armoured car", "armoured car", 4.8, 90, 80, 40, 300, 0.0, 0.30,
                  [P("nose and turret face", 14.5, 35, 1.0)], [P("side", 8, 35, 1.0)], [P("rear", 8, 35, 1.0)], 0,
                  "kwk30", 120, 180, 30, 3, 2, 1.0, 1.1, 0.9, 0.85, "petrol", (0.5, 0.6, 0.6), 0.1, 2.0, "medium",
                  "fast, small and quiet with a 20 mm automatic gun: sees first and gets away", "wheels and 8 mm plate: off the road she bogs, and any tank gun ends her", open_top=True),
    "FlakHalftrack": V("Sd.Kfz. 10/4 with the 2 cm Flak 38", "anti-aircraft half-track", 5.5, 100, 65, 25, 300, 0.0, 0.55,
                       [P("gun shield", 8, 20, 1.0)], [P("unarmoured", 0, 0, 1.0)], [P("unarmoured", 0, 0, 1.0)], 0,
                       "kwk30", 220, 280, 45, 7, 0, 0.5, 1.0, 1.0, 0.85, "petrol", (0.6, 0.6, 0.6), 0.1, 2.0, "medium",
                       "220 rounds a minute up to 2,000 m: death to a low aircraft, and to infantry and lorries on the ground", "no armour but the gun's shield", open_top=True),
    "Pak40": V("7.5 cm Pak 40", "anti-tank gun", 1.43, 0, 0, 0, 0, 0.0, 0.0, [P("spaced shield", 8, 25, 1.0)], [P("crew in the open", 0, 0, 1.0)], [P("crew in the open", 0, 0, 1.0)], 0,
               "pak40", 12, 0, 0, 5, 0, 0.0, 1.2, 1.0, 0.95, "none", (0, 0, 0), 0, 1.25, "high",
               "kills any Allied tank of 1942-44 from the front at 1,000 m, and stands barely waist-high in a hedge", "1.4 tonnes: once in place she stays there, and her crew have only a shield", arc=65),
    "leFH18": V("10.5 cm leichte Feldhaubitze 18", "field howitzer", 1.99, 0, 0, 0, 0, 0.0, 0.0, [P("shield", 4, 20, 1.0)], [P("crew in the open", 0, 0, 1.0)], [P("crew in the open", 0, 0, 1.0)], 0,
                "lefh18", 6, 0, 0, 6, 0, 0.0, 1.0, 1.0, 0.95, "none", (0, 0, 0), 0, 1.9, "high",
                "a 14.8 kg shell to 10.7 km, six a minute", "outranged by the Soviet and British guns of her class", arc=56),
    # ---------------------------------------------------------------- Soviet Union
    "T34": V("T-34 of 1942-43 (the six-sided turret)", "medium tank", 30.9, 500, 53, 25, 300, 0.80, 0.90,
             [P("turret front and mantlet", 60, 20, 0.30), P("glacis", 45, 60, 0.50), P("nose", 45, 53, 0.20)],
             [P("turret side", 52, 20, 0.30), P("upper hull side", 40, 41, 0.35), P("lower hull side", 45, 0, 0.35)],
             [P("turret rear", 52, 20, 0.35), P("upper hull rear", 40, 48, 0.40), P("lower hull rear", 40, 45, 0.25)], 20,
             "f34", 5, 100, 24, 4, 2, 0.4, 0.9, 0.75, 0.75, "diesel", (0.45, 0.65, 0.55), 0.35, 2.45, "high",
             "sloped armour all round worth twice its thickness in front, wide tracks that float her over mud and snow, a diesel engine, and the fastest of the mediums",
             "her commander is also her gunner, so she spots and shoots slowly; poor sights, few radios before 1943, and her fuel tanks line the fighting compartment: pierced, she burns, and half-empty tanks blow up",
             note="reliability is 0.65 for tanks built in 1942, 0.8 from mid-1943. Fire: a Soviet study of 72 wrecks from Kursk found 68 in 100 destroyed by the diesel in her fuel tanks burning, with the ammunition mostly whole in its racks: diesel did not save her"),
    "T34-85": V("T-34-85 of 1944", "medium tank", 32.0, 500, 53, 25, 300, 0.85, 0.85,
                [P("turret front and mantlet", 90, 15, 0.35), P("glacis", 45, 60, 0.45), P("nose", 45, 53, 0.20)],
                [P("turret side", 75, 20, 0.35), P("upper hull side", 45, 40, 0.30), P("lower hull side", 45, 0, 0.35)],
                [P("turret rear", 52, 10, 0.35), P("upper hull rear", 40, 48, 0.40), P("lower hull rear", 40, 45, 0.25)], 20,
                "zis_s53", 7, 55, 24, 5, 3, 1.0, 1.0, 1.0, 0.85, "diesel", (0.45, 0.65, 0.55), 0.35, 2.72, "high",
                "an 85 mm gun that pierces a Tiger's side at 1,000 m and a Panther's side at any range, a three-man turret at last, and the T-34's speed and wide tracks",
                "her hull is still the 45 mm of 1940: the long German 75 and the 88 go through her front at any battle range, and her fuel tanks still line the fighting compartment, so pierced she burns"),
    "KV1": V("KV-1 of 1941-42", "heavy tank", 47.0, 600, 35, 13, 225, 0.77, 0.60,
             [P("turret front and mantlet", 90, 15, 0.35), P("upper front", 75, 30, 0.40), P("nose", 75, 25, 0.25)],
             [P("turret side", 75, 15, 0.35), P("hull side", 75, 0, 0.65)],
             [P("turret rear", 75, 15, 0.35), P("hull rear", 70, 15, 0.65)], 35,
             "f34", 4, 114, 12, 5, 3, 0.4, 0.9, 0.85, 0.50, "diesel", (0.25, 0.40, 0.45), 0.35, 2.71, "medium",
             "75 mm or more on every face: in 1941 no German tank gun can pierce her from any side, only the 88 mm flak gun",
             "the same gun as the far cheaper T-34, a clutch and gearbox that fail on the march, too heavy for most bridges, and slow"),
    "SU85": V("SU-85", "tank destroyer", 29.6, 500, 55, 25, 300, 0.80, 0.90,
              [P("front plate", 45, 50, 0.80), P("gun mantlet", 60, 0, 0.20)],
              [P("upper side", 45, 20, 0.50), P("lower hull side", 45, 0, 0.50)],
              [P("upper rear", 45, 48, 0.60), P("lower rear", 45, 45, 0.40)], 20,
              "zis_s53", 6, 48, 6, 4, 0, 1.0, 1.0, 0.95, 0.80, "diesel", (0.30, 0.50, 0.45), 0.45, 2.45, "medium",
              "the 85 mm gun on the T-34's hull months before the T-34-85: kills a Tiger from the side at 1,000 m", "no turret (10 degrees each way) and no machine gun: helpless against infantry close in", arc=20),
    "SU76": V("SU-76M", "self-propelled gun", 10.5, 140, 45, 25, 250, 0.55, 0.85,
              [P("fighting compartment front", 25, 27, 0.45), P("glacis", 25, 60, 0.35), P("nose", 35, 30, 0.20)],
              [P("fighting compartment side", 10, 17, 0.50), P("hull side", 15, 0, 0.50)],
              [P("hull rear", 15, 0, 0.60), P("fighting compartment rear", 10, 0, 0.40)], 0,
              "zis3", 10, 60, 5, 4, 0, 0.6, 0.95, 0.9, 0.78, "petrol", (0.55, 0.65, 0.65), 0.35, 2.10, "medium",
              "the ZiS-3 field gun on light tracks that go where the infantry go, through marsh and snow", "open on top and at the back, 10 to 35 mm of plate, two petrol engines beside the driver", open_top=True, arc=32),
    "ZiS3": V("76 mm divisional gun ZiS-3", "field gun", 1.12, 0, 0, 0, 0, 0.0, 0.0, [P("shield", 5, 25, 1.0)], [P("crew in the open", 0, 0, 1.0)], [P("crew in the open", 0, 0, 1.0)], 0,
              "zis3", 15, 0, 0, 6, 0, 0.0, 0.95, 1.0, 0.95, "none", (0, 0, 0), 0, 1.37, "high",
              "field gun and anti-tank gun in one: 15 rounds a minute, 13 km of range, light enough for its crew to push", "against a Tiger or a Panther's front it is useless beyond point-blank", arc=54),
    # ---------------------------------------------------------------- United States
    "Sherman": V("Medium Tank M4 and M4A1, 75 mm", "medium tank", 30.3, 400, 39, 24, 193, 0.96, 0.55,
                 [P("turret front and mantlet", 89, 15, 0.33), P("glacis", 51, 56, 0.45), P("transmission cover", 51, 30, 0.22)],
                 [P("turret side", 51, 5, 0.30), P("hull side", 38, 0, 0.70)],
                 [P("turret rear", 51, 0, 0.30), P("hull rear", 38, 10, 0.70)], 19,
                 "m3_75", 12, 90, 24, 5, 3, 1.0, 1.0, 1.1, 0.95, "petrol", (0.60, 0.80, 0.70), 0.25, 2.74, "high",
                 "the most reliable tank of the war, a fast power-traversed turret that gets the first shot off, a gun steadied for firing on the move, the best high-explosive shell, and a radio in every tank",
                 "her 75 mm cannot pierce a Tiger or a Panther from the front at any range; tall, narrow-tracked, and until wet stowage (1944) four in five burn when pierced through the side",
                 note="0.60 to 0.80 is the record for the early, dry-stowed tanks, and the culprit was mostly her ammunition, not her petrol; with wet ammunition stowage (1944 on) fire_when_pierced drops to about 0.15"),
    "M10": V("3-inch Gun Motor Carriage M10", "tank destroyer", 29.6, 375, 48, 30, 320, 0.86, 0.60,
             [P("turret mantlet", 57, 45, 0.33), P("glacis", 38, 55, 0.45), P("transmission cover", 51, 30, 0.22)],
             [P("turret side", 25, 15, 0.30), P("upper hull side", 19, 38, 0.40), P("lower hull side", 25, 0, 0.30)],
             [P("turret rear", 25, 0, 0.30), P("upper hull rear", 19, 38, 0.35), P("lower hull rear", 25, 0, 0.35)], 0,
             "m7_76", 8, 54, 4.5, 5, 3, 1.0, 1.0, 1.0, 0.92, "diesel", (0.35, 0.50, 0.45), 0.25, 2.57, "high",
             "a 3-inch gun that pierces a Tiger's side at 1,500 m and a Panzer IV from the front at any range, on the Sherman's reliable gear",
             "her turret is cranked round by hand, 80 seconds for a full turn, it is open to the sky, and her plate is 19 to 38 mm",
             open_top=True, note="the model file says 12 degrees a second for her turret: history says about 4.5 (hand crank). To mend at her next export."),
    "M7": V("105 mm Howitzer Motor Carriage M7", "self-propelled howitzer", 22.9, 400, 39, 24, 193, 0.73, 0.65,
            [P("upper front", 13, 30, 0.50), P("transmission cover", 51, 30, 0.50)], [P("side", 13, 0, 1.0)], [P("rear", 13, 0, 1.0)], 0,
            "m2a1_105", 6, 69, 6, 7, 0, 1.0, 1.0, 1.0, 0.92, "petrol", (0.55, 0.65, 0.65), 0.35, 2.54, "high",
            "a 105 mm howitzer on the Sherman's hull: a 15 kg shell to 11 km wherever the tanks go", "open-topped, 13 mm plate", open_top=True, arc=45),
    "M3Halftrack": V("Half-track Personnel Carrier M3", "armoured carrier", 9.3, 147, 72, 30, 320, 0.0, 0.55,
                     [P("radiator shutters", 6.4, 25, 0.50), P("windscreen shield", 12.7, 25, 0.50)], [P("side", 6.4, 0, 1.0)], [P("rear", 6.4, 0, 1.0)], 0,
                     "m2_50", 0, 0, 0, 3, 0, 1.0, 1.0, 1.0, 0.92, "petrol", (0.5, 0.6, 0.6), 0.1, 2.26, "high",
                     "fast, tough and never short of spares, with a heavy machine gun over the cab", "6 mm of upright plate: a rifle bullet at close range goes through", open_top=True),
    "M16": V("Multiple Gun Motor Carriage M16 (four .50s)", "anti-aircraft half-track", 9.8, 147, 67, 28, 280, 0.0, 0.55,
             [P("windscreen shield", 12.7, 25, 0.50), P("radiator shutters", 6.4, 25, 0.50)], [P("side", 6.4, 0, 1.0)], [P("rear", 6.4, 0, 1.0)], 0,
             "m2_50", 1800, 5000, 60, 5, 1, 1.0, 1.0, 1.0, 0.92, "petrol", (0.5, 0.6, 0.6), 0.1, 2.3, "high",
             "four heavy machine guns in a powered mount: 1,800 rounds a minute, turning 60 degrees a second", "useless against armour", open_top=True),
    "M8": V("Light Armored Car M8", "armoured car", 7.9, 110, 89, 48, 560, 0.0, 0.30,
            [P("turret face", 19, 15, 0.35), P("upper hull front", 16, 60, 0.40), P("lower hull front", 19, 45, 0.25)],
            [P("turret side", 19, 15, 0.35), P("hull side", 9.5, 22, 0.65)], [P("turret rear", 19, 0, 0.35), P("hull rear", 9.5, 0, 0.65)], 0,
            "m6_37", 15, 80, 20, 4, 2, 1.0, 1.0, 0.9, 0.92, "petrol", (0.5, 0.6, 0.6), 0.2, 2.25, "high",
            "89 km/h on a road, 560 km on a tank of fuel, and a 37 mm gun that kills any other armoured car", "thin floor and plate, wheels that bog in mud", open_top=True),
    "Howitzer105": V("105 mm Howitzer M2A1", "field howitzer", 2.26, 0, 0, 0, 0, 0.0, 0.0, [P("shield", 4, 20, 1.0)], [P("crew in the open", 0, 0, 1.0)], [P("crew in the open", 0, 0, 1.0)], 0,
                     "m2a1_105", 6, 0, 0, 8, 0, 0.0, 1.0, 1.0, 0.97, "none", (0, 0, 0), 0, 1.6, "high",
                     "accurate and tireless, and tied to the fastest fire-direction system of the war: many batteries on one target within minutes", "11 km of range, less than the British and Soviet guns", arc=46),
    # ---------------------------------------------------------------- Britain
    "Matilda": V("Infantry Tank Mk II Matilda", "infantry tank", 26.9, 174, 24, 14, 257, 1.12, 0.50,
                 [P("turret front", 75, 0, 0.35), P("nose", 78, 0, 0.35), P("driver's front", 70, 30, 0.30)],
                 [P("turret side", 75, 0, 0.35), P("hull side and skirt", 70, 0, 0.65)],
                 [P("turret rear", 75, 0, 0.35), P("hull rear", 55, 0, 0.65)], 20,
                 "qf2", 15, 93, 14, 4, 3, 1.0, 1.0, 1.0, 0.75, "diesel", (0.20, 0.30, 0.40), 0.20, 2.51, "medium",
                 "70 to 78 mm on every face: in 1940-41 no tank gun and no ordinary anti-tank gun can pierce her from any side",
                 "24 km/h at best, a small gun with no high-explosive shell at all, and a turret too small to take a bigger one",
                 note="her turret is turned by hydraulic power, by hand in an emergency; no page gives its speed, so 14 degrees a second is the modeller's figure (about 26 seconds a full turn)"),
    "Sexton": V("25-pounder self-propelled Sexton", "self-propelled gun", 25.9, 400, 40, 24, 290, 0.80, 0.60,
                [P("upper front", 19, 25, 0.50), P("transmission cover", 51, 30, 0.50)], [P("side", 13, 0, 1.0)], [P("superstructure rear", 13, 0, 0.50), P("hull rear", 38, 0, 0.50)], 0,
                "qf25", 7, 105, 4, 6, 0, 1.0, 1.0, 1.0, 0.92, "petrol", (0.55, 0.65, 0.65), 0.35, 2.44, "medium",
                "the 25-pounder on a tank's hull, with 105 rounds aboard", "open-topped, 13 to 19 mm plate", open_top=True, arc=40),
    "25pdr": V("Ordnance QF 25-pounder", "field gun", 1.8, 0, 0, 0, 0, 0.0, 0.0, [P("shield", 6, 20, 1.0)], [P("crew in the open", 0, 0, 1.0)], [P("crew in the open", 0, 0, 1.0)], 0,
               "qf25", 7, 0, 0, 6, 0, 0.0, 1.0, 1.0, 0.97, "none", (0, 0, 0), 0, 1.65, "high",
               "turns all the way round on its firing platform in seconds, 12 km of range, and a solid shot that stops a 1941 tank", "a lighter shell (11.3 kg) than the 105 mm howitzers"),
    # ---------------------------------------------------------------- France
    "CharB1": V("Char B1 bis", "heavy tank", 31.5, 307, 28, 21, 180, 0.85, 0.60,
                [P("turret front", 56, 0, 0.25), P("upper hull front", 60, 45, 0.35), P("lower hull front", 60, 30, 0.40)],
                [P("turret side", 56, 22, 0.25), P("hull side", 55, 0, 0.75)],
                [P("turret rear", 56, 22, 0.25), P("hull rear", 50, 0, 0.75)], 25,
                "sa35_47", 5, 50, 12, 4, 1, 0.7, 0.9, 0.55, 0.55, "petrol", (0.40, 0.55, 0.60), 0.25, 2.79, "medium",
                "50 to 60 mm on every face in 1940, when German tank guns pierce 30 to 40: one held a village street against a whole column",
                "her commander is alone in the turret and must command, load, lay and fire; the 75 mm in the hull is aimed by turning the tank; six hours of fuel, and a radiator grille on her left side that a small gun can pierce",
                gun2="sa35_75"),
    "Gun75": V("Canon de 75 Mle 1897", "field gun", 1.19, 0, 0, 0, 0, 0.0, 0.0, [P("shield", 5, 20, 1.0)], [P("crew in the open", 0, 0, 1.0)], [P("crew in the open", 0, 0, 1.0)], 0,
               "mle1897", 15, 0, 0, 6, 0, 0.0, 0.9, 1.0, 0.95, "none", (0, 0, 0), 0, 1.4, "medium",
               "15 aimed rounds a minute and more in a hurry: the fastest-firing field gun of its day", "a design of 1897: 6 degrees of swing without shifting the trail, and 8.5 km of range", arc=6),
    # ---------------------------------------------------------------- Italy
    "M13": V("Carro Armato M13/40", "medium tank", 14.0, 125, 31, 15, 200, 0.92, 0.50,
             [P("turret front", 42, 10, 0.35), P("superstructure front", 30, 11, 0.40), P("nose", 30, 30, 0.25)],
             [P("turret side", 25, 22, 0.30), P("hull side", 25, 9, 0.70)],
             [P("turret rear", 25, 20, 0.30), P("hull rear", 25, 20, 0.70)], 14,
             "c47_32", 7, 104, 14, 4, 2, 0.5, 0.9, 0.8, 0.60, "diesel", (0.35, 0.45, 0.50), 0.25, 2.37, "medium",
             "a sound 47 mm gun, as good as the British 2-pounder and with a high-explosive shell besides",
             "riveted plate that cracks and throws its rivets when hit, an engine too weak for the desert, and a two-man turret", quality=0.9),
    "Semovente": V("Semovente da 75/18", "assault gun", 13.1, 125, 32, 15, 230, 0.90, 0.50,
                   [P("superstructure front", 50, 5, 0.60), P("nose", 30, 20, 0.40)],
                   [P("side", 25, 8, 1.0)], [P("rear", 25, 15, 1.0)], 12,
                   "c75_18", 5, 44, 8, 3, 0, 0.7, 0.9, 0.85, 0.65, "diesel", (0.35, 0.45, 0.50), 0.25, 1.85, "medium",
                   "low, with 50 mm in front and a 75 mm whose hollow-charge shell pierces a Grant or a Sherman's side: Italy's best armoured vehicle in the desert",
                   "a crew of three, a slow shell, and the same weak engine as the M13", quality=0.9, arc=38),
    "L3": V("Carro Veloce L3/35", "tankette", 3.2, 43, 42, 15, 125, 0.50, 0.55,
            [P("front", 14, 15, 1.0)], [P("side", 9, 0, 1.0)], [P("rear", 9, 0, 1.0)], 6,
            "breda38", 0, 0, 12, 2, 0, 0.1, 0.8, 0.6, 0.75, "petrol", (0.5, 0.6, 0.6), 0.1, 1.28, "medium",
            "tiny, cheap and quick: 1.3 m high", "two machine guns and plate a heavy machine gun pierces: no match for any real tank", arc=24),
    # ---------------------------------------------------------------- Japan
    "Type97": V("Type 97 Chi-Ha (57 mm)", "medium tank", 15.0, 170, 38, 20, 210, 0.61, 0.75,
                [P("turret front", 25, 10, 0.35), P("hull front", 25, 20, 0.65)],
                [P("turret side", 25, 11, 0.30), P("upper hull side", 25, 25, 0.40), P("lower hull side", 20, 0, 0.30)],
                [P("turret rear", 25, 12, 0.30), P("hull rear", 20, 15, 0.70)], 10,
                "t97_57", 8, 114, 14, 4, 2, 0.3, 0.9, 0.8, 0.80, "diesel", (0.30, 0.40, 0.45), 0.25, 2.23, "medium",
                "a reliable air-cooled diesel and light weight that crosses jungle tracks and weak bridges no Western medium can",
                "25 mm of plate and a short 57 mm made for shelling infantry: against a Sherman she can do nothing from any side but point-blank at the rear"),
    "Type97:47": V("Type 97 Shinhoto Chi-Ha (the new turret and 47 mm gun, from 1942)", "medium tank", 15.8, 170, 38, 20, 210, 0.64, 0.75,
                   [P("turret front", 25, 10, 0.35), P("hull front", 25, 20, 0.65)],
                   [P("turret side", 25, 11, 0.30), P("upper hull side", 25, 25, 0.40), P("lower hull side", 20, 0, 0.30)],
                   [P("turret rear", 25, 12, 0.30), P("hull rear", 20, 15, 0.70)], 10,
                   "t1_47", 10, 104, 14, 4, 2, 0.3, 0.9, 0.8, 0.80, "diesel", (0.30, 0.40, 0.45), 0.25, 2.38, "low",
                   "the same light, reliable tank with a long 47 mm that goes through a Sherman's side and rear at battle range",
                   "still 25 mm of plate: any tank gun or anti-tank gun goes through her from any side",
                   note="no model of her own yet: draw the Type97 model. A row so that Japan's tanks of 1942-45 are not left with the 57 mm; the historian asked for it and has not checked these figures"),
    # ---------------------------------------------------------------- Poland
    "7TP": V("7TP (single turret)", "light tank", 9.9, 110, 37, 18, 150, 0.60, 0.75,
             [P("turret front", 15, 10, 0.35), P("hull front", 17, 20, 0.65)],
             [P("turret side", 15, 10, 0.30), P("hull side", 13, 0, 0.70)],
             [P("turret rear", 15, 10, 0.30), P("hull rear", 10, 0, 0.70)], 8,
             "wz37", 10, 80, 16, 3, 2, 0.3, 1.0, 0.8, 0.80, "diesel", (0.30, 0.40, 0.45), 0.2, 2.27, "medium",
             "the best gun of any light tank in 1939, a diesel engine and a good periscope: better than the Panzer I and II she meets", "17 mm of plate, and there were only about 130 of her"),
    # ---------------------------------------------------------------- our own designs, shared by every small nation
    "LightTank": V("our own light tank of 1936-42 (in the manner of the Vickers and Czech export tanks)", "light tank", 9.5, 120, 42, 20, 200, 0.60, 0.70,
                   [P("turret front", 25, 10, 0.35), P("hull front", 25, 15, 0.65)],
                   [P("turret side", 15, 10, 0.30), P("hull side", 15, 0, 0.70)],
                   [P("turret rear", 15, 10, 0.30), P("hull rear", 13, 0, 0.70)], 8,
                   "any37", 10, 90, 18, 3, 2, 0.4, 0.95, 0.8, 0.80, "petrol", (0.45, 0.55, 0.65), 0.2, 2.15, "design",
                   "cheap, light enough for any bridge, and a match for other light tanks", "25 mm of riveted plate and a 37 mm gun: outclassed by every medium tank"),
    "MediumTank": V("our own medium tank of 1940-45 (welded, sloped front, 50 mm gun)", "medium tank", 21.0, 300, 42, 20, 200, 0.85, 0.60,
                    [P("turret front", 50, 12, 0.35), P("hull front", 50, 30, 0.65)],
                    [P("turret side", 30, 15, 0.30), P("hull side", 30, 0, 0.70)],
                    [P("turret rear", 30, 15, 0.35), P("hull rear", 30, 10, 0.65)], 15,
                    "any50", 9, 80, 16, 5, 3, 0.8, 1.0, 1.0, 0.80, "petrol", (0.45, 0.60, 0.70), 0.25, 2.40, "design",
                    "a sound three-man turret and 50 mm of sloped plate in front: equal to a 1941 medium", "her 50 mm gun cannot pierce a T-34, a Sherman or a late Panzer IV from the front"),
    "AssaultGun": V("our own assault gun (the medium tank's hull, a 75 mm in a fixed box)", "assault gun", 21.5, 300, 40, 19, 190, 0.87, 0.60,
                    [P("box front", 60, 20, 0.60), P("hull front", 50, 30, 0.40)],
                    [P("box side", 30, 10, 0.50), P("hull side", 30, 0, 0.50)],
                    [P("box rear", 25, 0, 0.35), P("hull rear", 30, 10, 0.65)], 12,
                    "any75", 8, 50, 6, 4, 0, 0.8, 1.0, 0.95, 0.82, "petrol", (0.45, 0.60, 0.70), 0.25, 2.10, "design",
                    "a real 75 mm gun and 60 mm in front for the price of a medium tank", "no turret: 12 degrees each way", arc=24),
    "SelfPropelledGun": V("our own self-propelled gun (the medium tank's hull, a 75 mm field gun in an open box)", "self-propelled gun", 19.0, 300, 42, 20, 200, 0.77, 0.60,
                          [P("gun shield and box front", 12, 25, 0.50), P("hull front", 50, 30, 0.50)],
                          [P("box side", 10, 10, 0.60), P("hull side", 30, 0, 0.40)],
                          [P("box rear", 10, 0, 0.50), P("hull rear", 30, 10, 0.50)], 0,
                          "mle1897", 10, 40, 5, 5, 0, 0.6, 0.95, 0.9, 0.82, "petrol", (0.55, 0.65, 0.70), 0.35, 2.20, "design",
                          "a field gun that keeps up with the tanks", "an open box of 10 mm plate", open_top=True, arc=30),
    "ArmouredCar": V("our own armoured car of 1936-45 (37 mm gun in a closed turret)", "armoured car", 7.5, 100, 70, 35, 300, 0.0, 0.30,
                     [P("turret face", 18, 15, 0.35), P("hull front", 15, 25, 0.65)],
                     [P("turret side", 13, 15, 0.35), P("hull side", 10, 10, 0.65)], [P("turret rear", 13, 10, 0.35), P("hull rear", 8, 0, 0.65)], 6,
                     "any37", 10, 60, 18, 4, 2, 0.6, 1.0, 0.85, 0.82, "petrol", (0.5, 0.6, 0.6), 0.15, 2.3, "design",
                     "fast on a road with a gun that kills other scouts", "wheels, and plate against bullets only"),
    "AntiTankGun": V("57 mm anti-tank gun (as the American M1 and the British 6-pounder)", "anti-tank gun", 1.2, 0, 0, 0, 0, 0.0, 0.0, [P("shield", 6, 25, 1.0)], [P("crew in the open", 0, 0, 1.0)], [P("crew in the open", 0, 0, 1.0)], 0,
                     "any57", 15, 0, 0, 5, 0, 0.0, 1.0, 1.0, 0.95, "none", (0, 0, 0), 0, 1.28, "medium",
                     "kills a Panzer IV from the front at 1,000 m and a Tiger or a Panther through the side, and two men can swing it", "nothing to a Tiger's or a Panther's front", arc=90),
    "Truck": V("our own 3-ton lorry", "lorry", 5.5, 75, 65, 20, 350, 0.0, 0.25, [P("unarmoured", 0, 0, 1.0)], [P("unarmoured", 0, 0, 1.0)], [P("unarmoured", 0, 0, 1.0)], 0,
               None, 0, 0, 0, 2, 0, 0.0, 1.0, 1.0, 0.85, "petrol", (0.4, 0.4, 0.4), 0.05, 2.6, "design",
               "carries a section or tows a gun at 65 km/h on a road", "no armour, and off the road in mud it stops", open_top=True),
    "FlakTruck": V("our own lorry with a 20 mm automatic gun", "anti-aircraft lorry", 5.0, 75, 60, 18, 300, 0.0, 0.25, [P("gun shield", 6, 20, 1.0)], [P("unarmoured", 0, 0, 1.0)], [P("unarmoured", 0, 0, 1.0)], 0,
                   "any20", 220, 300, 45, 5, 0, 0.3, 1.0, 1.0, 0.82, "petrol", (0.5, 0.5, 0.5), 0.1, 2.6, "design",
                   "light flak that travels with the column", "no armour", open_top=True),
    # ---------------------------------------------------------------- the what-if classes (our own inventions: design figures, not history)
    "WhatIf_BreakthroughTank": V("what-if class `breaker`", "breakthrough tank", 48.0, 550, 30, 16, 180, 0.90, 0.60,
                                 [P("turret front", 110, 10, 0.35), P("hull front", 100, 30, 0.65)],
                                 [P("turret side", 80, 10, 0.35), P("hull side", 70, 0, 0.65)],
                                 [P("turret rear", 70, 10, 0.35), P("hull rear", 50, 10, 0.65)], 30,
                                 "any75w", 6, 70, 10, 6, 3, 1.0, 1.0, 1.0, 0.65, "petrol", (0.40, 0.55, 0.70), 0.25, 3.0, "design",
                                 "armour to walk through a gun line", "slow, costly, thin at the back"),
    "WhatIf_TankHunterCar": V("what-if class `tankhunter`", "tank hunter armoured car", 11.0, 160, 80, 40, 500, 0.0, 0.35,
                              [P("turret face", 30, 20, 0.35), P("hull front", 25, 40, 0.65)],
                              [P("turret side", 14, 20, 0.35), P("hull side", 12, 20, 0.65)], [P("turret rear", 12, 10, 0.35), P("hull rear", 10, 0, 0.65)], 8,
                              "any75w", 8, 40, 20, 4, 2, 1.0, 1.0, 0.9, 0.85, "petrol", (0.5, 0.6, 0.6), 0.25, 2.4, "design",
                              "a tank-killing gun at 80 km/h", "plate against bullets only"),
    "WhatIf_FlameTank": V("what-if class `flametank`", "flame tank", 23.0, 300, 40, 19, 180, 0.90, 0.60,
                          [P("turret front", 60, 12, 0.35), P("hull front", 60, 30, 0.65)],
                          [P("turret side", 30, 15, 0.30), P("hull side", 30, 0, 0.70)], [P("turret rear", 30, 15, 0.35), P("hull rear", 30, 10, 0.65)], 15,
                          "flame", 0, 0, 16, 4, 2, 0.8, 1.0, 0.95, 0.78, "petrol", (0.55, 0.75, 0.85), 0.35, 2.4, "design",
                          "burns infantry out of bunkers and houses", "must close to 100 m, and burns fiercely herself"),
    "WhatIf_AmphibiousTank": V("what-if class `amphtank`", "amphibious tank", 14.0, 240, 40, 20, 250, 0.55, 0.80,
                               [P("turret front", 30, 12, 0.35), P("hull front", 20, 45, 0.65)],
                               [P("turret side", 15, 12, 0.30), P("hull side", 12, 0, 0.70)], [P("turret rear", 15, 10, 0.35), P("hull rear", 10, 0, 0.65)], 8,
                               "any37w", 10, 100, 16, 4, 2, 0.8, 1.0, 0.9, 0.78, "petrol", (0.45, 0.55, 0.65), 0.2, 2.5, "design",
                               "swims a river or comes ashore from a ship at 10 km/h", "a big thin hull"),
    "WhatIf_SnowTank": V("what-if class `snowtank`", "snow tank", 16.0, 260, 40, 30, 250, 0.35, 1.00,
                         [P("turret front", 35, 12, 0.35), P("hull front", 30, 45, 0.65)],
                         [P("turret side", 20, 12, 0.30), P("hull side", 20, 0, 0.70)], [P("turret rear", 20, 10, 0.35), P("hull rear", 15, 0, 0.65)], 10,
                         "any37w", 10, 90, 14, 4, 2, 0.8, 1.0, 0.9, 0.80, "petrol", (0.45, 0.55, 0.65), 0.2, 2.4, "design",
                         "very wide tracks: keeps going in deep snow and marsh that stops everything else", "a light gun and light plate"),
    "WhatIf_GliderTank": V("what-if class `glidertank`", "glider tank", 7.5, 165, 60, 30, 220, 0.50, 0.70,
                           [P("turret front", 16, 12, 0.35), P("hull front", 14, 30, 0.65)],
                           [P("turret side", 12, 12, 0.30), P("hull side", 10, 0, 0.70)], [P("turret rear", 12, 10, 0.35), P("hull rear", 8, 0, 0.65)], 6,
                           "any37w", 12, 50, 24, 3, 2, 0.8, 1.0, 0.85, 0.80, "petrol", (0.5, 0.6, 0.65), 0.2, 1.9, "design",
                           "light enough to land by glider behind the enemy's line", "plate against rifle bullets only"),
}

# ---- the vehicles every small nation shares: a little different from country to country ------------------------
# Brian: "make the generic ones varying a little bit from country to country so they aren't all exactly the same".
# Each is a multiplier on the shared design's armour (thickness), gun (piercing), speed and reliability, mostly inside
# 0.80 .. 1.20, and wider where the real thing was plainly weaker or stronger than the design (a 37 mm gun standing
# for a 57, a Renault's thick casting: the historian's check of 22 Nov). A lean can change at a date (`later`), and a
# country that was given a great power's own tank takes that tank's row from then on (ROW, written as `use_row`).
# Where a country really fielded such a vehicle the lean follows it (`after`); where it had none, a
# small spread of its own, the same every time (`after`: none). Two great powers use the light tank before their own
# types arrive: for them it is that country's real light tank, with numbers of its own (`as`).
OWN = {"LightTank": {
    "USA": {"after": "Light Tank M3 (Stuart), 1941-42", "as": {"weight_t": 12.7, "power_hp": 250, "road_kph": 58, "cross_country_kph": 30, "gun": "m6_37", "reliability": 0.92,
                                                             "armour": {"front": [P("turret front", 38, 10, 0.35), P("upper hull front", 38, 17, 0.45), P("nose", 44, 20, 0.20)],
                                                                        "side": [P("turret side", 25, 0, 0.30), P("hull side", 25, 0, 0.70)], "rear": [P("turret rear", 25, 0, 0.30), P("hull rear", 25, 0, 0.70)], "top_mm": 10}}},
    "SOV": {"after": "BT-7 and T-26, to 1941", "as": {"weight_t": 13.0, "power_hp": 400, "road_kph": 52, "cross_country_kph": 30, "gun": "k20", "reliability": 0.60, "radio": 0.2, "command": 0.7,
                                                      "armour": {"front": [P("turret front", 15, 10, 0.35), P("hull front", 20, 18, 0.65)],
                                                                 "side": [P("turret side", 15, 10, 0.30), P("hull side", 13, 0, 0.70)], "rear": [P("turret rear", 13, 10, 0.30), P("hull rear", 13, 0, 0.70)], "top_mm": 8}}},
}}
LEAN = {
    # nation: {model: (after, armour, gun, speed, reliability)}
    "FIN": {"LightTank": ("Vickers 6-ton and captured T-26, 45 mm gun", 0.85, 1.15, 0.85, 0.95), "MediumTank": ("captured T-28 and T-34, a handful", 1.05, 1.10, 1.00, 0.90), "AssaultGun": ("StuG III bought in 1943", 1.15, 1.15, 1.00, 1.05), "AntiTankGun": ("37 mm Bofors, then 75 mm Pak", 0.90, 0.40, 1.0, 1.05, {"from": "1943", "gun": 1.20, "after": "75 mm Pak 40 bought from Germany"})},
    "SWE": {"LightTank": ("Stridsvagn m/38 to m/41, Bofors 37 mm", 1.05, 1.10, 1.05, 1.10), "MediumTank": ("Stridsvagn m/42, 75 mm", 1.05, 1.15, 1.00, 1.00), "AssaultGun": ("Stormartillerivagn m/43", 1.00, 1.05, 1.00, 1.05), "AntiTankGun": ("Bofors 37 and 57 mm", 1.0, 1.0, 1.0, 1.10), "ArmouredCar": ("Landsverk Lynx", 1.05, 0.90, 1.05, 1.05)},
    "HUN": {"LightTank": ("38M Toldi, 20 then 40 mm gun", 0.85, 0.85, 1.15, 0.95), "MediumTank": ("40M Turan", 1.05, 0.90, 1.10, 0.85), "AssaultGun": ("43M Zrinyi, 105 mm", 1.15, 0.95, 1.00, 0.90), "ArmouredCar": ("39M Csaba", 0.85, 0.85, 0.95, 0.95)},
    "ROM": {"LightTank": ("R-2 (the Czech LT vz. 35)", 1.00, 1.05, 0.82, 0.88), "MediumTank": ("German mediums supplied 1942-44", 1.05, 1.10, 0.95, 0.90), "AssaultGun": ("TACAM tank destroyers on captured hulls", 0.85, 1.10, 0.95, 0.85), "AntiTankGun": ("47 mm Schneider, then 75 mm Resita", 0.95, 0.50, 1.0, 0.95, {"from": "1944", "gun": 1.20, "after": "75 mm Resita Model 1943"})},
    "SVK": {"LightTank": ("LT vz. 38", 1.00, 1.10, 1.00, 1.15), "MediumTank": ("a few German mediums", 0.95, 0.95, 0.95, 0.95)},
    "BUL": {"LightTank": ("LT vz. 35 and Panzer 38(t)", 1.00, 1.05, 0.90, 1.00), "MediumTank": ("Panzer IV supplied in 1943", 1.15, 1.20, 0.95, 0.95), "AssaultGun": ("StuG III supplied in 1943", 1.15, 1.15, 1.00, 1.00)},
    "YUG": {"LightTank": ("Renault R35", 1.60, 0.60, 0.80, 0.90), "AntiTankGun": ("47 mm Skoda", 0.90, 0.55, 1.0, 0.95)},
    "GRE": {"LightTank": ("a handful of Vickers and captured Italian tankettes", 0.80, 0.85, 0.95, 0.85)},
    "TUR": {"LightTank": ("T-26 of 1933", 0.80, 1.15, 0.80, 0.85), "MediumTank": ("mediums from both sides, 1943 on", 1.00, 1.00, 0.95, 0.90)},
    "ESP": {"LightTank": ("T-26 and Panzer I", 0.80, 1.10, 0.85, 0.85), "MediumTank": ("twenty Panzer IV of 1943", 0.95, 1.05, 0.95, 0.85)},
    "POR": {"LightTank": ("two Vickers 6-ton and Carden-Loyd carriers", 0.80, 0.90, 0.90, 0.90)},
    "SUI": {"LightTank": ("Panzer 39, a long 24 mm gun", 1.15, 0.85, 1.05, 1.15), "AntiTankGun": ("47 mm Pak 41", 0.90, 0.60, 1.0, 1.15)},
    "BEL": {"LightTank": ("T-13 and ACG-1, 47 mm gun", 0.80, 1.15, 0.95, 0.95), "AntiTankGun": ("47 mm FRC", 0.90, 0.60, 1.0, 1.05)},
    "NED": {"LightTank": ("Carden-Loyd light tanks", 0.80, 0.85, 1.00, 0.95), "ArmouredCar": ("Landsverk and DAF M39", 1.05, 1.00, 1.00, 1.05), "AntiTankGun": ("47 mm Boehler", 0.85, 0.50, 1.0, 1.0)},
    "DEN": {"ArmouredCar": ("Landsverk Lynx", 1.05, 0.90, 1.05, 1.05)},
    "IRL": {"LightTank": ("two Landsverk L-60", 0.85, 0.85, 1.05, 1.00), "ArmouredCar": ("Landsverk L180", 1.00, 0.90, 0.95, 1.00)},
    "EST": {"LightTank": ("Renault FT and TKS tankettes", 0.80, 0.80, 0.80, 0.85)},
    "LAT": {"LightTank": ("Vickers Carden-Loyd M1936", 0.80, 0.95, 1.00, 0.90)},
    "LIT": {"LightTank": ("Vickers Carden-Loyd M1936", 0.80, 0.85, 1.00, 0.90)},
    "CHN": {"LightTank": ("Vickers 6-ton, T-26 and Panzer I", 0.80, 1.05, 0.85, 0.80), "MediumTank": ("lend-lease mediums in Burma, 1944", 1.05, 1.10, 0.95, 0.85)},
    "THA": {"LightTank": ("Vickers 6-ton and the Japanese Type 95", 0.80, 0.95, 1.05, 0.90)},
    "MAN": {"LightTank": ("Japanese Type 94 and Type 95", 0.80, 0.95, 1.05, 0.85)},
    "MGL": {"LightTank": ("Soviet BT-5 and BT-7", 0.85, 1.15, 1.20, 0.80), "ArmouredCar": ("Soviet BA-6 and BA-10, 45 mm gun", 0.95, 1.15, 0.90, 0.85)},
    "IRN": {"LightTank": ("Czech TNH and AH-IV", 1.00, 1.05, 1.00, 0.95)},
    "IRQ": {"LightTank": ("Italian L3 tankettes", 0.80, 0.80, 1.00, 0.80)},
    "AFG": {"LightTank": ("a few Italian and Soviet light tanks", 0.80, 0.90, 0.90, 0.80)},
    "BRA": {"LightTank": ("Light Tank M3 by lend-lease, 1942 on", 1.20, 1.20, 1.20, 1.10), "MediumTank": ("M3 Lee and M4 by lend-lease", 1.10, 1.15, 0.95, 1.10)},
    "MEX": {"LightTank": ("Light Tank M3 and M5 by lend-lease", 1.15, 1.15, 1.20, 1.05)},
    "ARG": {"LightTank": ("Vickers Carden-Loyd", 0.80, 0.85, 1.00, 0.90), "MediumTank": ("Nahuel DL 43, 75 mm", 1.20, 1.05, 0.95, 0.80)},
    "CHL": {"LightTank": ("Carden-Loyd, then M3 by lend-lease", 1.00, 1.05, 1.10, 0.95)},
    "PER": {"LightTank": ("Czech LTP", 0.95, 1.05, 1.00, 1.05)},
    "USA": {"AntiTankGun": ("57 mm Gun M1", 1.00, 0.97, 1.0, 1.05)},
    "SOV": {"AntiTankGun": ("57 mm ZiS-2", 0.95, 1.15, 1.0, 0.95), "ArmouredCar": ("BA-10, 45 mm gun", 0.95, 1.15, 0.85, 0.85)},
    "FRA": {"AntiTankGun": ("47 mm SA 37", 0.95, 0.75, 1.0, 1.0), "ArmouredCar": ("Panhard 178, 25 mm gun", 1.15, 0.85, 1.05, 1.05), "LightTank": ("Renault R35 and Hotchkiss H39", 1.60, 0.60, 0.80, 0.90)},
    "ITA": {"AntiTankGun": ("47/32", 0.85, 0.50, 1.0, 1.0), "ArmouredCar": ("Autoblinda AB 41, 20 mm gun", 0.90, 0.80, 1.10, 0.95)},
    "JAP": {"AntiTankGun": ("47 mm Type 1", 0.85, 0.65, 1.0, 1.0), "ArmouredCar": ("Type 92", 0.80, 0.80, 0.90, 0.90), "LightTank": ("Type 95 Ha-Go", 0.50, 0.95, 1.08, 1.00)},
    "POL": {"AntiTankGun": ("37 mm wz. 36 (Bofors)", 0.85, 0.40, 1.0, 1.05), "ArmouredCar": ("wz. 34", 0.80, 0.85, 0.80, 0.90)},
    "GER": {"LightTank": ("Panzer 38(t) and Panzer II", 1.10, 1.10, 1.00, 1.10)},
    "UK": {"AntiTankGun": ("6-pounder", 1.00, 1.03, 1.0, 1.05), "ArmouredCar": ("Daimler armoured car, 2-pounder", 1.05, 1.15, 1.10, 1.05), "LightTank": ("Light Tank Mk VI: machine guns only", 0.80, 0.50, 1.20, 1.00)},
}
# A country that was given a great power's own tank: (the row she takes, from when, what it was, how sure the date is).
# A row written "LightTank@USA" is the United States' own light tank (the M3 Stuart) in OWN.
ROW = {
    "ESP": {"MediumTank": ("PanzerIV", "1943-12", "twenty Panzer IV Ausf. H", "medium")},
    "BUL": {"MediumTank": ("PanzerIV", "1943", "Panzer IV supplied in 1943-44", "medium"), "AssaultGun": ("StuG3", "1943", "StuG III Ausf. G supplied in 1943", "medium")},
    "ROM": {"MediumTank": ("PanzerIV", "1943", "Panzer IV supplied in 1943-44", "medium")},
    "FIN": {"AssaultGun": ("StuG3", "1943", "StuG III Ausf. G bought in 1943", "medium")},
    "CHN": {"MediumTank": ("Sherman", "1944", "lend-lease Shermans in Burma", "medium")},
    "BRA": {"MediumTank": ("Sherman", "1944", "M4 by lend-lease", "low"), "LightTank": ("LightTank@USA", "1942", "Light Tank M3 by lend-lease", "low")},
    "MEX": {"LightTank": ("LightTank@USA", "1943", "Light Tank M3 and M5 by lend-lease: whether any came during the war is not sure", "low")},
    "CHL": {"LightTank": ("LightTank@USA", "1943", "Light Tank M3 by lend-lease", "low")},
}
SHARED = ("LightTank", "MediumTank", "AssaultGun", "SelfPropelledGun", "ArmouredCar", "AntiTankGun", "Truck", "FlakTruck", "Gun75")
NATIONS = ("GER UK USA SOV FRA ITA JAP POL CAN AUS NZL SAF IND BEL NED LUX DEN NOR SWE FIN EST LAT LIT IRL SUI ESP POR HUN SVK ROM YUG BUL GRE TUR IRQ IRN CHN "
           "MEX CUB HAI DOM GUA ELS HON NIC COS PAN COL VEN ECU PER BRA BOL PAR URU ARG CHL THA MAN MGL AFG NEP SAU YEM LBR").split()


def lean_of(nation, model):
    """A nation's lean on a shared design: (after, armour, gun, speed, reliability)."""
    row = LEAN.get(nation, {}).get(model)
    took = ROW.get(nation, {}).get(model)
    use = {"use_row": {"row": took[0], "from": took[1], "after": took[2], "sure": took[3]}} if took else {}
    if row:
        later = {"later": row[5]} if len(row) > 5 else {}
        return {"after": row[0], "armour": row[1], "gun": row[2], "speed": row[3], "reliability": row[4], **later, **use}

    def roll(what):                                     # (a small spread of its own, the same every time: 0.94 .. 1.06)
        return round(0.94 + 0.12 * ((zlib.crc32(f"{nation}:{model}:{what}".encode()) % 1000) / 999.0), 2)
    return {"after": None, "armour": roll("armour"), "gun": roll("gun"), "speed": roll("speed"), "reliability": roll("reliability"), **use}


# ---- the working out -------------------------------------------------------------------------------------------
def pierce_at(gun, metres):
    """mm at 30 degrees a gun pierces at a range (between the table's points, and a little beyond them)."""
    t = gun["pierce_30deg_mm"]
    if metres <= RANGES[0]:
        return float(t[0])
    for i in range(len(RANGES) - 1):
        if metres <= RANGES[i + 1]:
            u = (metres - RANGES[i]) / (RANGES[i + 1] - RANGES[i])
            return t[i] * (1 - u) + t[i + 1] * u
    slope = (t[-1] - t[-2]) / (RANGES[-1] - RANGES[-2])
    return max(0.0, t[-1] + slope * (metres - RANGES[-1]))


def need_of(mm, slope, calibre, quality=1.0):
    """What a shot must pierce square-on to get through a plate at a slope."""
    if mm <= 0:
        return 0.0
    k = max(0.0, min(0.5, 0.5 * mm / max(calibre, 1.0) - 0.2))
    return mm * quality / math.cos(math.radians(min(slope, 85.0))) ** (1.0 + k)


def step(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3.0 - 2.0 * x)


def chance(gun, plates, metres, quality=1.0, scale=1.0, hollow=True):
    """The chance a hit on a face comes through. hollow: she has her hollow-charge shell (see the gun's heat_from)."""
    square = pierce_at(gun, metres) * 1.2 * scale
    if hollow and gun.get("heat_mm") and metres <= gun["heat_to_m"]:
        square = max(square, float(gun["heat_mm"]))
    if square <= 0.0:
        return 0.0
    out = 0.0
    for _, mm, slope, share in plates:
        need = need_of(mm, slope, gun["calibre_mm"], quality)
        out += share * (1.0 if need <= 0.0 else step((square / need - 0.85) / 0.30))
    return out


def worth(plates, quality=1.0, calibre=75.0):
    """A face in one number: the upright plate it is worth against a 75 mm shot. Its plates by their shares, the thin
    ones counting for more (a face is as good as the plates a shot finds, so a glacis no shot can bite does not
    make the plate beside it any thicker)."""
    thin = sum(share / max(need_of(mm, slope, calibre, quality), 1e-6) for _, mm, slope, share in plates if mm > 0)
    return 1.0 / thin if thin > 0 else 0.0


def reach(gun, plates, quality=1.0, hollow=True):
    """The farthest range (to the nearest 50 m) at which a hit on the face comes through one time in two."""
    best = 0
    for metres in range(50, 2501, 50):
        if chance(gun, plates, metres, quality, hollow=hollow) >= 0.5:
            best = metres
    return best


def kill_when_pierced(gun):
    """How often a shot that comes through puts her out of the fight: more for a bigger shell, more if it bursts inside."""
    c = gun["calibre_mm"]
    base = 0.35 if c < 30 else 0.45 if c < 45 else 0.55 if c < 60 else 0.70 if c < 80 else 0.80 if c < 100 else 0.90
    return round(base + (0.05 if gun["bursts_inside"] else -0.07), 2)


def build():
    faces = ("front", "side", "rear")
    vehicles, facing = {}, {}
    for name, v in VEHICLES.items():
        a = v["armour"]
        w = {f: round(worth(a[f], a["quality"])) for f in faces}
        v = dict(v)
        v["armour"] = dict(a, **{f: [{"plate": p[0], "mm": p[1], "slope_deg": p[2], "share": p[3]} for p in a[f]] for f in faces})
        v["worth_mm"] = dict(w, top=a["top_mm"])
        v["weaker_than_front"] = {"side": round(w["front"] / w["side"], 2) if w["side"] else None, "rear": round(w["front"] / w["rear"], 2) if w["rear"] else None}
        g = GUNS.get(v["gun"]) if v["gun"] else None
        v["kills_when_it_pierces"] = kill_when_pierced(g) if g else None
        vehicles[name] = v
        facing[name] = w
    shooters = [n for n, v in VEHICLES.items() if v["gun"] and max(GUNS[v["gun"]]["pierce_30deg_mm"] + [GUNS[v["gun"]]["heat_mm"] or 0]) >= 20]
    targets = [n for n, v in VEHICLES.items() if facing[n]["front"] >= 10 and v["kind"] not in ("anti-tank gun", "field gun", "field howitzer")]
    duels, far = {}, {}
    for s in shooters:
        g = GUNS[VEHICLES[s]["gun"]]
        g2 = GUNS.get(VEHICLES[s].get("second_gun") or "")
        duels[s], far[s] = {}, {}
        for t in targets:
            a = VEHICLES[t]["armour"]
            best = lambda plates, m, h=True: max(chance(g, plates, m, a["quality"], hollow=h), chance(g2, plates, m, a["quality"], hollow=h) if g2 else 0.0)   # noqa: E731
            duels[s][t] = {f: [round(best(a[f], m), 2) for m in RANGES] for f in faces}
            far[s][t] = {f: max(reach(g, a[f], a["quality"]), reach(g2, a[f], a["quality"]) if g2 else 0) for f in faces}
            if g.get("heat_mm"):                        # (before her hollow-charge shell was issued: the plain shot only)
                duels[s][t]["before_hollow_charge"] = {f: [round(best(a[f], m, False), 2) for m in RANGES] for f in faces}
                far[s][t]["before_hollow_charge"] = {f: reach(g, a[f], a["quality"], hollow=False) for f in faces}
    shared = {m: {n: lean_of(n, m) for n in NATIONS} for m in SHARED}
    for m, rows in OWN.items():
        for n, row in rows.items():
            arm = row["as"].get("armour")
            as_ = dict(row["as"])
            if arm:
                as_["armour"] = {f: ([{"plate": p[0], "mm": p[1], "slope_deg": p[2], "share": p[3]} for p in arm[f]] if f in faces else arm[f]) for f in arm}
                as_["worth_mm"] = {f: round(worth(arm[f])) for f in faces}
            shared[m][n] = {"after": row["after"], "as": as_}
    return {
        "what": "What every land vehicle and gun could do, as measured: armour by facing, the gun by range, speed, ground pressure, rate of fire, reliability. Keyed by the names in index-land.json. Made by tools/battle-models/land/performance.py.",
        "asked": "Brian, 22 Nov 2026: the named tanks have real advantages from their measured performance against their peers; German tanks are far weaker at the back, so hits there pay; the shared vehicles vary a little by country.",
        "sure": "every vehicle and gun carries `sure`: high, medium or low (the standard published figures, set down by the modeller), or design (our own invention: no history to check).",
        "checked": "by the historian on 22 Nov 2026 (historian/land-performance-check-1122.md): the Tiger and Panther finding confirmed, and the corrections there are in this file. Not checked: the row Type97:47 and her gun (the modeller's own figures); the month taken for the M93 round (the check says late 1944); the hollow-charge dates, found on no page and marked low.",
        "units": {"armour": "mm, and slope in degrees from the vertical", "pierce_30deg_mm": "mm of rolled plate pierced at 30 degrees from square-on, at ranges_m, by each nation's own table (good to about one part in ten across nations)",
                  "soft_ground": "0 to 1: how well she keeps going in mud, snow and sand (ground pressure, track width, wheels)", "radio": "the share of her kind that can talk to each other",
                  "optics": "1.0 is a good plain sight; the German sights are the best", "command": "1.0 is three men in the turret with a cupola; less where the commander must also lay or load",
                  "reliability": "0 to 1, a judgment: about the share still running after a long march", "fire_when_pierced": "a judgment from wartime surveys: how often she burns when a shot comes through that face; explodes_of_fires: how many of those end with her ammunition going up",
                  "worth_mm": "each face as one number: the upright plate it is worth against a 75 mm shot", "weaker_than_front": "how many times weaker the side and the rear are than the front"},
        "rule": {"square": "pierce30(range) x 1.2", "need": "mm x quality / cos(slope)^(1+k), k = 0.5 x mm / calibre - 0.2 held in 0 .. 0.5", "chance": "smooth step of (square / need): 0 at 0.85, half at 1.00, sure at 1.15",
                 "face": "the chances on its plates, each by its share", "hollow_charge": "a gun with heat_mm pierces that much square-on at any range to heat_to_m, from the month heat_from; before it, use `before_hollow_charge` in duels and reach",
                 "later_rounds": "duels and reach are worked with each gun's first round. From a later round's month multiply that gun's piercing row by its `times` and judge again by this rule (scarce: a few rounds a gun; only: the nations that had it)",
                 "then": "a shot that comes through puts her out of the fight `kills_when_it_pierces` of the time (the shooter's), and she burns `fire_when_pierced` of the time by the face (the target's)"},
        "ranges_m": RANGES,
        "guns": GUNS,
        "vehicles": vehicles,
        "duels": {"what": "duels[shooter][target][face] = the chance a hit on that face comes through, at ranges_m", "table": duels},
        "reach": {"what": "reach[shooter][target][face] = the farthest range in metres at which a hit comes through one time in two (0: never; 2500: beyond the table)", "table": far},
        "shared_by_nation": {"what": "the vehicles every small nation shares, a little different from country to country: multiply the shared design's armour (mm), gun (piercing), speed and reliability by these; `as` replaces the design's own numbers for a great power's real light tank; "
                                     "`later`: from that date the lean changes to the numbers given; `use_row`: from that date she has that tank itself, so take the named row of `vehicles` and no lean (LightTank@USA: the `as` row of USA here)",
                             "table": shared},
        "gaps": ["No model carries the British 17-pounder, the American 76 mm tank gun or 90 mm, the Soviet 122 mm or the German 88 mm L/71: the heaviest Allied gun here is the M10's 3-inch and the T-34-85's 85 mm.",
                 "The M10's turret speed in her model file (12 degrees a second) is faster than history (4.5, by hand: the historian confirms it), and the Tiger's file says 6 where this sheet says 10: both to mend at their next export. Until then take turret_degrees_a_second from here.",
                 "The Type 97 with the 47 mm gun (row Type97:47) has no model of her own and her figures are unchecked.",
                 "The what-if classes' figures are stand-ins by their size and role: the class table (sandbox arms) should overrule them."],
    }


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "/mnt/project-files/battle-models/land/"
    data = build()
    path = os.path.join(out, "performance-land.json")
    with open(path, "w") as f:
        text = json.dumps(data, indent=1)
        # (a row of numbers reads better on one line)
        import re
        text = re.sub(r"\[\s+((?:-?[\d.]+,\s+)*-?[\d.]+)\s+\]", lambda mm: "[" + re.sub(r"\s+", " ", mm.group(1)) + "]", text)
        f.write(text)
    print(f"{path}: {len(data['vehicles'])} vehicles, {len(data['guns'])} guns, {len(data['duels']['table'])} shooters, {os.path.getsize(path) // 1024} KB")


if __name__ == "__main__":
    main()
