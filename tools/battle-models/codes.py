"""Each nation's own marking on her bombers, letter by letter, where America's carry nose art (Brian, 2026-11-20:
every nation's bombers keep their own record, and "nose art only for US bombers, other countries didnt do that").
The air thread's brief (/mnt/project-files/air-battles/nose-art/tallies.md, sections 3 and 5) asks the modeller for
"the markings of section 3 as parts or decals the scene can set for each aircraft: letters and digits in each
nation's style and colours".

    overlay("Lancaster")                 ->  the model "LancasterCodes"
    wear("Lancaster", "letter", "KM-G")  ->  the parts that paint KM-G on both her sides
    python export.py <out> LancasterCodes,He111Codes,AmerikabomberCodes,BettyCodes,NellCodes,BomberCodes json glb fbx

WHAT THE SCENE DOES. Load <Name>Codes (air/<Name>Codes.json, .glb, .fbx) and lay it on her as her damage is laid: it
has every part of hers (same names, parents and pivots, no faces of their own) and every clip of hers, so play on it
what she plays. Play its clip "none" first, which scales every letter and digit to nothing; then show at full size
(scale 1) the parts that wear(name, style, mark) names for the aircraft's own mark, and leave the rest hidden. The
mark is the campaign's: `mark` of markOf() in src/campaign/bomber-marks.ts (its lists are in
data/campaign/bomber-marks.json), carried on a raid's ticket in `place.bombers`. The plain Bomber wears the style of
the nation that flies her. In the pictures: rigid.to_blender(layer, livery.look), posed with her as
airshow.Plane.wear poses her livery. The file's `info.file.styles` says, for each style, how a mark fills its places
and where each place is (x, z: her own metres; `high`: the capitals' height).

THE STYLES (`MODELS` says which bomber wears which; America's are not here: the B-17 has her livery):

- "letter", Britain and the Commonwealth, "KM-G": the squadron's two letters and the aircraft's own letter on her
  fuselage either side of the roundel, read "KM (roundel) G" from left to right on both sides (so the two letters
  stand ahead of the roundel on her port side and aft of it on her starboard side). Medium sea grey. The Lancaster;
  the plain Bomber, which Britain flies until 1942.
- "code", Germany, "5J+DM": two characters, the cross, two letters, read "5J (cross) DM" on both sides. The third
  character, the aircraft's own letter, is in her squadron's colour (white, red or yellow; a red one thinly edged in
  white, or it is lost in her dark greens), the rest black. The He 111 and the Amerika bomber. wear() reads a bare
  code without its cross ("5JDM") as two and two.
- "tail", Japan, "K-312": the unit's code (one character or two), a dash and the aircraft's number, white, on her fin
  (on the outer face of each fin of a twin-finned one), on across her rudder where her fin alone is too small for it
  (Japan paints nothing on a rudder). The Betty, the Nell; the plain Bomber (Japan flies it in release 1).
- "squadron", Italy, "278-7": the squadron's number and a dash ahead of the national marking, the aircraft's number
  aft of it (on her starboard side the other way round, so that both read left to right); black on a light finish,
  white on a dark one. The plain Bomber.
- "tactical", the Soviet Union, "White 23": a number in white, red, yellow or blue on her fin, with a thin dark edge
  (white and yellow numbers) or a thin white one (red and blue). The plain Bomber; the Pe-2, whose number is on her
  forward fuselage instead (`ON_SIDE`).
- "number", everyone else: a plain white number on her fuselage aft of the national marking. The campaign's mark is
  the bare number, "14" (its name for the aircraft is "No. 14"); wear() takes either. The plain Bomber.

WHERE. On her fuselage a style is centred on her national marking's height, as tall as `SIZE` says but no taller than
three fifths of her fuselage there. Every character stands toward the marking in its cell (`LEAN`), so that the paint
next to the marking is half a letter from it on both sides, whatever the letter. It is made smaller (to `HIDE_SHRINK`
of it) where that brings every place into plain sight from far abeam (`hidden`). (Her starboard fin, out on her
tailplane, stood between the eye and the Lancaster's starboard "K" with her roundel at x -5.6, so that her letters
were 0.51 m; the art director moved her roundel to -5.1, and they are 0.75 m, all in sight.) And smaller still (to
`SHRINK` of that) where that keeps it a hand clear of a window or a gun (`crowded`). On her fin it is the biggest it
can be up to `SIZE`, clear of her fin's edges and of what is painted there: on her fixed fin alone if it is as big
there or nearly (`ALONE`), else across her rudder's hinge as well. check(name) looks at every place: what lies under
each cell, what of hers hides it from far
abeam (a fault, but for a gun's barrel, or her own wing seen from her level or below), whether it comes near the
marking or her record.

THE PARTS. One part for each place a character can stand, on each side, for each character the campaign can paint
there (and each colour, where the colour changes): <style>.<side>.<place>.<character>[.<colour>], e.g.
letter.port.1.K, letter.port.2.M, letter.port.3.G (her own letter), code.port.3.D.red, tail.starboard.3.- (the
dash), tactical.port.2.7.white. A place is a fixed spot on her, numbered in reading order on each side:
  letter 1 2 (marking) 3;   code 1 2 (cross) 3 4;   squadron 1 2 3 4 (marking) 5 6, 4 the dash;
  tail 1 2 3 4 5 6 on her fin, 3 the dash, a one-character unit code in 2 (so the dash of "K-312" is tail.port.3.-,
  not tail.port.2.- as the brief's example has it: places 1 and 2 hold the unit's code);
  tactical 1 2 3 on her fin, a lone digit in 2, two digits in 1 and 3;
  number 1 2 3 aft of the marking: on her port side from the marking aft, on her starboard side ending at it.
A character painted across a line where she comes apart (the hinge of her rudder, or the cut where her tail breaks
away) is two parts, the piece beyond the line named after the part it rides: on the Betty tail.port.4.3 (on her
rudder) and tail.port.4.3.tail (its piece on her fin); on the He 111 code.port.3.D.red and code.port.3.D.red.plane.
wear() names every piece, so the scene need not know which places are cut.

THE LETTERING. Each character is DejaVu Sans Bold, narrowed to a sign painter's block hand, laid on her skin
(livery.Skin.lay) in bands so that it follows her curve, the right way round on each side. No character is painted
past its cell, its edge and all (a wide letter, an M, a W, a red D in its white edge, is squeezed to the cell's
width), so two side by side are never closer than `SPACE`, about a fifth of their height; the narrowest (J, 1, E, F,
L) are drawn out to a block hand's width (`WIDEN`), stems as they were, so that none leaves a hole beside its
neighbour. Her plain model (the `far` mesh, for far off) has our own block hand of squares instead (`BLOCK`), with
the same edges in squares: the same parts, a fifth to a third of the triangles. Each paint is the layer's own
(`PAINTS`), and reads by its lightness under the battle grade on the finish it is painted on, but these: the German
black is within 14 to 18 of the He 111's and the Amerika bomber's dark greens in lightness (L*), as it was (it reads
in the sun and is lost in shadow); a Soviet red or blue number, and the German red letter, read by their edges.
Nothing is painted across her national marking, her record (the tally panel under her cockpit), a window or her
glass, or a flag or flash on her fin. None of it is any real aircraft's marking; the campaign draws the letters and
numbers (it keeps the RAF's PO and AR clear), and wear() paints none of the few famous codes it could still draw
(`FAMOUS`: it names no parts for them and says so in `warnings`).

The model's own place for the game's number (`marks`, tag "#") lies under some of these codes: when a bomber wears
her codes, the scene should not paint that number as well.
"""
import fnmatch
import math

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

import livery
from rigid import Model, Part

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
DIGITS = "0123456789"
SIDES = (("port", 1.0), ("starboard", -1.0))              # (her own axes: y to port)

# Which styles each bomber wears (planes.BUILDERS names); the plain Bomber is flown by every small or medium nation,
# so she carries every style but Germany's.
MODELS = {"Lancaster": ("letter",), "He111": ("code",), "Amerikabomber": ("code",), "Betty": ("tail",), "Nell": ("tail",), "LeO451": ("number",), "Wellington": ("letter",), "Pe2": ("tactical",), "SM79": ("squadron",),
          "Bomber": ("letter", "tail", "squadron", "tactical", "number")}

# The paints (the layer's own, as livery.PAINTS are the livery's). Each must read by its lightness under the battle
# grade, where every colour is greyed to an eighth of its saturation: `sinks` says where one does not.
PAINTS = {"raf_grey": "A3A6A0",                                         # (medium sea grey)
          "ger_black": "1A1A1A", "ger_white": "E2E0D6", "ger_red": "A8302A", "ger_yellow": "D6B03A",
          "jap_white": "E2E0D6",
          "ita_black": "1A1A1A", "ita_white": "E2E0D6",
          "sov_white": "E2E0D6", "sov_red": "B02A22", "sov_yellow": "D6B03A", "sov_blue": "34528E",
          "sov_edge_dark": "1C1C1C", "sov_edge_white": "E2E0D6", "ger_edge_white": "E2E0D6",
          "num_white": "E2E0D6"}
COLOURS = {"code": ("white", "red", "yellow"), "squadron": ("black", "white"), "tactical": ("white", "red", "yellow", "blue")}
# A character's thin edge, by its style and colour: a Soviet number's always (dark round white and yellow, white round
# red and blue); a German aircraft letter's in red, which is no lighter than her dark greens and is lost in them
# under the battle grade without it (the Luftwaffe edged a coloured letter thinly in white, often).
EDGE = {"tactical": {"white": "sov_edge_dark", "yellow": "sov_edge_dark", "red": "sov_edge_white", "blue": "sov_edge_white"},
        "code": {"red": "ger_edge_white"}}

# What the campaign paints in each place (markOf in src/campaign/bomber-marks.ts, from the lists in
# data/campaign/bomber-marks.json): a place has a part for each of these and no more (wear() warns of any other).
# The RAF's squadron letters are any two but I; a German wing's code a figure 1 to 9 and a letter but I and O, either
# way round; her own letter is one of the twelve aircraft letters of a squadron, and the last its squadron's letter;
# a Japanese unit code one of nine letters, alone or with a 1 or 2 after it. Numbers keep every figure.
SQUADRON_LETTERS = LETTERS.replace("I", "")
WING_CODE = "123456789" + LETTERS.replace("I", "").replace("O", "")
AIRCRAFT_LETTERS, STAFFEL_LETTERS = "ABCDEFGHKLMN", "HKLMNPRSTUVW"
UNIT_CODES = "KRTUVWXYZ"
# Each style: where it is painted, how narrow its hand is, and its places in reading order: (the kind of cell: L a
# letter, A a letter or a figure, D a figure, - the dash; which side of the national marking it stands, 0 before it
# in reading order and 1 after it, "aft" aft of it; the characters it holds; whether its colour changes).
STYLE = {
    "letter": {"on": "side", "narrow": 0.78, "paint": "raf_grey",
               "places": [("L", 0, SQUADRON_LETTERS, False), ("L", 0, SQUADRON_LETTERS, False), ("L", 1, LETTERS, False)]},
    "code": {"on": "side", "narrow": 0.72, "paint": "ger_black",
             "places": [("A", 0, WING_CODE, False), ("A", 0, WING_CODE, False), ("L", 1, AIRCRAFT_LETTERS, True), ("L", 1, STAFFEL_LETTERS, False)]},
    "tail": {"on": "fin", "narrow": 0.74, "paint": "jap_white",
             "places": [("A", 0, UNIT_CODES, False), ("A", 0, UNIT_CODES + "12", False), ("-", 0, "-", False),
                        ("D", 0, DIGITS, False), ("D", 0, DIGITS, False), ("D", 0, DIGITS, False)]},
    "squadron": {"on": "side", "narrow": 0.74, "paint": None,
                 "places": [("D", 0, DIGITS, True), ("D", 0, DIGITS, True), ("D", 0, DIGITS, True), ("-", 0, "-", True),
                            ("D", 1, DIGITS, True), ("D", 1, DIGITS, True)]},
    "tactical": {"on": "fin", "narrow": 0.78, "paint": None,
                 "places": [("D", 0, DIGITS, True), ("D", 0, DIGITS, True), ("D", 0, DIGITS, True)]},
    "number": {"on": "side", "narrow": 0.74, "paint": "num_white",
               "places": [("D", "aft", DIGITS, False), ("D", "aft", DIGITS, False), ("D", "aft", DIGITS, False)]},
}
WIDE = {"L": 0.9, "A": 0.9, "D": 0.8, "-": 0.44}         # (a cell's width, in capital heights before narrowing: an H, a figure, the dash)
# No character is painted past its cell, its edge and all: a wider one (an M, a W, a red D in its white edge) is
# squeezed to the cell's width. So two characters side by side are never closer than the space between their cells.
SPACE = 0.18                                             # (between two cells, in capital heights: the least gap of paint between two characters)
# Where a character stands in its cell when it is narrower than the cell: 1 at the end toward the next place in
# reading order, -1 toward the one before, 0 in the middle. On her fuselage every character stands toward her
# national marking, so that the paint next to the marking is always half a letter from it on both sides, whatever the
# letter; a narrower one leaves its slack on its far side, between it and the next character out (5J: the J stands
# half a letter from the cross, and the 5 as close to the J's cell as it can). On her fin, where there is no marking,
# the characters of a pair (the figures of 23) lean together:
LEAN = {"tail": (1, 0, 0, 1, 0, -1), "tactical": (1, 0, -1)}
# The narrowest capitals and figures, drawn wider as a sign painter draws them in a block hand, so that none leaves a
# hole beside its neighbour: (the height in capital heights at which a cut through it meets its stem alone, its width
# in capital heights). The stem stays as it is; the hook of the J, the arms of the E, F and L and the flag and foot
# of the 1 are drawn out. (The font's J hangs below the line, and drawn within it is no wider than an I and a half.)
WIDEN = {"J": (0.25, 0.78), "1": (0.0, 0.78), "E": (0.25, 0.8), "F": (-0.25, 0.8), "L": (0.0, 0.8)}
DASH = (0.22, 0.1)                                       # (half the dash's length and half its thickness, in capital heights)
OUTLINE = 0.026                                          # (the font's own offset for a character's edge (`EDGE`): about a twentieth of its height)
BLOCK_EDGE = 0.06                                        # (the same edge round a character of her plain model's block hand, in capital heights)

# How tall each style is painted on each bomber (capital height, metres). On her fin it is the most tried: the
# letters are fitted to her fin and come out smaller where it is small. On her fuselage a style is never more than
# three fifths of her height there.
SIZE = {"Lancaster": {"letter": 0.75}, "He111": {"code": 0.65}, "Amerikabomber": {"code": 0.65},
        "Betty": {"tail": 0.45}, "Nell": {"tail": 0.45}, "LeO451": {"number": 0.55}, "Wellington": {"letter": 0.65}, "Pe2": {"tactical": 0.4}, "SM79": {"squadron": 0.55},
        "Bomber": {"letter": 0.6, "tail": 0.45, "squadron": 0.55, "tactical": 0.55, "number": 0.55}}
# A fin style her owner painted on her forward fuselage instead, on both sides, its places about this middle (x, z:
# her own metres) and as tall as `SIZE` says, made smaller only as any style on her fuselage is (`side_places`): the
# Pe-2's number just behind her greenhouse, over her wing root, her stars keeping her fins (the art director,
# 2026-11-23). Under her greenhouse is her record's panel, and her airscrews stand abeam of its front; behind her
# greenhouse her nacelles hide her side's foot from abeam and it turns up above, so her number comes out 0.35 m.
ON_SIDE = {"Pe2": {"tactical": (0.95, 0.34)}}
TALL = 0.6                                              # (of her fuselage's height where the letters stand, at most)
CLEAR = 0.18                                             # (between her letters and a window, a gun, anything not her skin: a hand)
CLEAR_UP = 0.06                                          # (the same above and below them, where her wing root may run close under)
SHRINK = 0.85                                            # (how small her letters may be made to keep them clear, of their size)
# Seen from far off square abeam, nothing of her own may stand between her letters and the eye: a fin out on her
# tailplane (the Lancaster's), her tailplane, a nacelle. Her letters are made smaller (down to HIDE_SHRINK of their
# size) until every place, and HIDE_MARGIN round it, is in plain sight. Only her guns may cross them, as they swing
# (`GUNS`), and her wing (`WINGS`), which reaches out toward the eye under letters that stand just above its root:
# from her own level or below it can cross their foot (more the lower the eye), from 1.5 degrees above her it is
# clear of them, as on the real aircraft (a fin hides them from every height, and is a fault).
HIDE_SHRINK = 0.7
HIDE_MARGIN = 0.05
GUNS = ("flexgun", "turret")
WINGS = ("wing", "aileron", "flap")

# How far apart a glyph's bands are: on her fuselage across its girth, where it curves, and along it; on her fin
# along its chord. (A flat piece bigger than a band would dip under her skin in its middle.)
BANDS = {"side": {True: (0.5, 0.18), False: (1.2, 0.45)}, "fin": {True: (0.35, 10.0), False: (1.0, 10.0)}}
RES = {True: 3, False: 0}                                # (the font's curve resolution on her fine model; her plain one has the block hand)
EDGE_RES = 2                                             # (a character's edge, a hair round its figure, needs no finer curve)
LIFT = 0.012                                             # (off her skin, as her livery's letters: clear of her scribed lines and rivets)
EDGE_UP = 0.004                                          # (an edged character stands this much prouder than its edge, which is at LIFT)
EDGED_FILL = 0.4                                         # (an edged character's bands, of its edge's: a flat piece dips under her round
                                                         #  skin in its middle, and where the character's dipped more than EDGE_UP
                                                         #  its edge showed through it; so: about 2 mm between them everywhere, 1.9
                                                         #  at the least, on the Amerika bomber's red L)
LIFT_PLAIN = 0.035                                       # (her plain model is cut in broad flat facets: its letters stand clear of them)
EDGE_UP_PLAIN = 0.016                                    # (and an edged one's squares this much prouder than their edge, as broadly cut:
                                                         #  where her tail breaks away a piece's fill dips 11 mm, so 3 mm at the least)
FIN_MARGIN = 0.07                                        # (clear of her fin's edges and of anything painted on it)
# The fin styles whose owners paint nothing on her rudder: the plain Bomber's rudder flag (`*FIN`) is the unit table's
# `fin` for her owner, and Japan and the Soviet Union have none. And how much of its size a fin style gives up to keep
# off her rudder (a character across the hinge is cut in two there).
FIN_FREE = {"tail": ("*FIN",), "tactical": ("*FIN",)}
ALONE = 0.85
STEP = 0.02                                              # (the grid her fin is looked at on)

SINK = 20.0                                              # (a paint reads on a finish if their lightnesses L* are this far apart or more)
SKIN_PAINTS = {"upper", "upper2", "under", "nose", "boss", "line", "rivet", "rivet_under", "seam", "seam_under", "stain", "patch"}

# The squadron colour of a German bomber's own letter, by her squadron's letter (her code's last character): the
# 1st, 4th, 7th and 10th squadrons white, the 2nd, 5th, 8th and 11th red, the 3rd, 6th, 9th and 12th yellow.
STAFFEL = dict(zip(STAFFEL_LETTERS, ("white", "red", "yellow") * 4))
# Codes of famous real aircraft, never to be painted: the campaign should not draw them, and wear() names no parts
# for them if it does (and says so in `warnings`).
# ("S for Sugar" and "G for George", whose squadrons' letters the campaign keeps clear; every Lancaster of the dams
# raid; Nettleton's Augsburg Lancaster; "M for Mother", the Lancaster of the most raids; and the two Bettys shot down
# over Bougainville on 18 April 1943.)
FAMOUS = {"letter": ("PO-*", "AR-*", "AJ-*", "KM-B", "PM-M"), "tail": ("T1-323", "T1-326")}


# ---- characters --------------------------------------------------------------------------------------------------
# Her plain model's characters (seen only from far off, where a capital is a few pixels high): our own block hand,
# three squares wide and five high, each a few rectangles; top row first. M and W have two full rows (high in the M,
# low in the W) so as not to read as an H; no N three squares wide reads better than the arch.
BLOCK = {"0": "### #.# #.# #.# ###", "1": ".#. ##. .#. .#. ###", "2": "### ..# ### #.. ###", "3": "### ..# .## ..# ###",
         "4": "#.# #.# ### ..# ..#", "5": "### #.. ### ..# ###", "6": "### #.. ### #.# ###", "7": "### ..# ..# .#. .#.",
         "8": "### #.# ### #.# ###", "9": "### #.# ### ..# ###",
         "A": ".#. #.# ### #.# #.#", "B": "##. #.# ##. #.# ##.", "C": ".## #.. #.. #.. .##", "D": "##. #.# #.# #.# ##.",
         "E": "### #.. ##. #.. ###", "F": "### #.. ##. #.. #..", "G": ".## #.. #.# #.# .##", "H": "#.# #.# ### #.# #.#",
         "I": "### .#. .#. .#. ###", "J": "..# ..# ..# #.# .#.", "K": "#.# #.# ##. #.# #.#", "L": "#.. #.. #.. #.. ###",
         "M": "#.# ### ### #.# #.#", "N": "##. #.# #.# #.# #.#", "O": ".#. #.# #.# #.# .#.", "P": "##. #.# ##. #.. #..",
         "Q": ".#. #.# #.# ##. .##", "R": "##. #.# ##. #.# #.#", "S": ".## #.. .#. ..# ##.", "T": "### .#. .#. .#. .#.",
         "U": "#.# #.# #.# #.# ###", "V": "#.# #.# #.# #.# .#.", "W": "#.# #.# ### ### #.#", "X": "#.# #.# .#. #.# #.#",
         "Y": "#.# #.# .#. .#. .#.", "Z": "### ..# .#. #.. ###"}
BLOCK_W = 0.25                                           # (a square's width, in capital heights: a block capital 0.75 wide)


def block(ch, offset=0.0):
    """A character in the block hand: its rectangles (runs of squares in a row, joined to the same run below), each
    grown by `offset` (capital heights) all round for its edge."""
    rows = BLOCK[ch].split()
    rects, open_ = [], {}
    for r, row in enumerate(rows + ["..."]):
        runs, c = [], 0
        while c < 3:
            if row[c] == "#":
                c0 = c
                while c < 3 and row[c] == "#":
                    c += 1
                runs.append((c0, c))
            else:
                c += 1
        for run in list(open_):
            if run not in runs:
                rects.append((run, open_.pop(run), r))
        for run in runs:
            open_.setdefault(run, r)
    out = []
    for (c0, c1), r0, r1 in rects:
        u0, u1 = (c0 - 1.5) * BLOCK_W - offset, (c1 - 1.5) * BLOCK_W + offset
        v0, v1 = 0.5 - r1 * 0.2 - offset, 0.5 - r0 * 0.2 + offset
        out += [((u0, v0), (u1, v0), (u1, v1)), ((u0, v0), (u1, v1), (u0, v1))]
    return out


_SHAPES = {}


def shape(ch, res=3, offset=0.0):
    """One character's flat triangles, its capitals one unit high and standing on their middle (livery.glyph); a
    letter that hangs below the line in print (J, Q) is drawn within it, as a sign painter draws block capitals, and
    the narrowest are drawn wider (`WIDEN`); the dash is our own plain bar. res 0: the block hand. offset: the edge
    round the character (the font's own offset; for the block hand, capital heights)."""
    key = (ch, res, round(offset, 5))
    if key in _SHAPES:
        return _SHAPES[key]
    if ch == "-":
        a, b = DASH[0] + offset * (2.2 if res else 1.0), DASH[1] + offset * (2.2 if res else 1.0)
        tris = [((-a, -b), (a, -b), (a, b)), ((-a, -b), (a, b), (-a, b))]
    elif res == 0:
        tris = block(ch, offset)
    else:
        tris = _font(ch, res, offset)
        if ch in WIDEN:
            tris = widened(ch, tris, _font(ch, res, 0.0))
    _SHAPES[key] = tris
    return tris


def _font(ch, res, offset):
    tris = livery.glyph(ch, res=res, offset=offset)
    vs = [p[1] for t in tris for p in t]
    lo, hi = min(vs), max(vs)
    if hi - lo > 1.1 + 4.4 * offset:
        k = (1.04 + 4.4 * offset) / (hi - lo)
        mid = 0.5 * (hi + lo)
        tris = [tuple((u, (v - mid) * k) for u, v in t) for t in tris]
    return tris


def runs_at(tris, v):
    """Where a horizontal line at height v crosses a character's paint: [(u0, u1), ...], left to right."""
    segs = []
    for t in tris:
        us = []
        for i in range(3):
            (a, b), (c, d) = t[i], t[(i + 1) % 3]
            if (b - v) * (d - v) < 0.0:
                us.append(a + (c - a) * (v - b) / (d - b))
        if len(us) == 2:
            segs.append((min(us), max(us)))
    segs.sort()
    out = []
    for a, b in segs:
        if out and a <= out[-1][1] + 1e-6:
            out[-1] = (out[-1][0], max(out[-1][1], b))
        else:
            out.append((a, b))
    return out


def widened(ch, tris, fill):
    """A narrow character drawn out to its width in `WIDEN`: what lies left of its stem drawn out leftward and what
    lies right of it rightward, by the same factor, its stem as it was (`fill`: the character without its edge, to
    find the stem by; an edge round it keeps its own thickness beyond the character's ends)."""
    v_at, want = WIDEN[ch]
    a, b = max(runs_at(fill, v_at), key=lambda r: r[1] - r[0])
    wide, mid = width_of(fill)
    lo, hi = mid - wide / 2.0, mid + wide / 2.0
    if want <= wide or (a - lo) + (hi - b) < 1e-6:
        return tris
    s = 1.0 + (want - wide) / ((a - lo) + (hi - b))

    def move(u):
        if u < a:
            return a - (a - max(u, lo)) * s - max(lo - u, 0.0)
        if u > b:
            return b + (min(u, hi) - b) * s + max(u - hi, 0.0)
        return u
    pieces = []
    for t in tris:
        polys = [list(t)]
        for c in (lo, a, b, hi):                         # (cut where the drawing-out changes, so that each piece moves as a whole)
            nxt = []
            for poly in polys:
                for q in cut(poly, 1.0, 0.0, c):
                    if q:
                        nxt.append(q)
            polys = nxt
        for poly in polys:
            pieces += fan(poly)
    return [tuple((move(u), v) for u, v in t) for t in pieces]


def width_of(tris):
    us = [p[0] for t in tris for p in t]
    return max(us) - min(us), 0.5 * (max(us) + min(us))


def drawn(place, ch, res=3, offset=0.0, edged=False):
    """A character as it is painted in a place: in metres, u along the way it is read and v up, from the place's
    middle; narrowed, squeezed to its cell if it is wider (its edge too, if it has one: `edged`), and standing in its
    cell as its place leans (`lean`). `offset`: draw its edge (res, offset as `shape` takes them) instead."""
    st = STYLE[place.style]
    fine = res > 0
    wide, mid = width_of(shape(ch, RES[True] if fine else 0))
    lo, hi = -wide / 2.0, wide / 2.0                     # (its paint either side of its middle, capital heights)
    if edged:
        e_wide, e_mid = width_of(shape(ch, EDGE_RES if fine else 0, OUTLINE if fine else BLOCK_EDGE))
        lo, hi = e_mid - e_wide / 2.0 - mid, e_mid + e_wide / 2.0 - mid
    k = min(st["narrow"], place.cell / ((hi - lo) * place.h))
    ink = (hi - lo) * k * place.h
    shift = place.lean * max(0.0, place.cell - ink) / 2.0 - 0.5 * (lo + hi) * k * place.h
    tris = shape(ch, res, offset)
    return [tuple(((u - mid) * k * place.h + shift, v * place.h) for u, v in t) for t in tris]


# ---- cutting flat shapes ---------------------------------------------------------------------------------------------
def cut(poly, a, b, c):
    """A convex polygon cut by the line a u + b v = c: (the part where a u + b v <= c, the rest), either None."""
    lo, hi = [], []
    n = len(poly)
    for i in range(n):
        p, q = poly[i], poly[(i + 1) % n]
        fp, fq = a * p[0] + b * p[1] - c, a * q[0] + b * q[1] - c
        if fp <= 0.0:
            lo.append(p)
        if fp >= 0.0:
            hi.append(p)
        if (fp < 0.0 < fq) or (fq < 0.0 < fp):
            t = fp / (fp - fq)
            m = (p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t)
            lo.append(m)
            hi.append(m)

    def keep(pts):
        if len(pts) < 3:
            return None
        area = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))
        return pts if abs(area) > 1e-9 else None
    return keep(lo), keep(hi)


def fan(poly):
    return [(poly[0], poly[i], poly[i + 1]) for i in range(1, len(poly) - 1)]


def banded(tris, du, dv):
    """Triangles cut in bands no wider than du along and dv up (metres), so that no piece is longer than a band
    across her curve: the same lines through all of them (no point of one piece in the middle of another's side),
    spread evenly over the whole shape, and none where a band would do."""
    out = [list(t) for t in tris]
    for axis, step in ((0, du), (1, dv)):
        vals = [p[axis] for t in tris for p in t]
        if not vals:
            return []
        lo, hi = min(vals), max(vals)
        n = max(1, math.ceil((hi - lo) / step - 1e-9))
        for k in range(1, n):
            c = lo + (hi - lo) * k / n
            nxt = []
            for poly in out:
                for piece in cut(poly, 1.0 if axis == 0 else 0.0, 1.0 if axis == 1 else 0.0, c):
                    if piece:
                        nxt.append(piece)
            out = nxt
    return [t for poly in out for t in fan(poly)]


def split(tris, line):
    """Triangles parted by a line (a, b, c): (those where a u + b v <= c, the rest)."""
    lo, hi = [], []
    a, b, c = line
    for tri in tris:
        p, q = cut(list(tri), a, b, c)
        if p:
            lo += fan(p)
        if q:
            hi += fan(q)
    return lo, hi


# ---- her, as the layer needs her ---------------------------------------------------------------------------------
class Her:
    """What the layer needs of her: her skin, everything she is made of (to see what lies under a letter), her
    fuselage and fin as her numbers draw them, her national marking, her fin's marks and her record's panel."""

    def __init__(self, name, m):
        import planes
        G = planes.game()
        s = G["PLANES"][name]
        self.name, self.m, self.s = name, m, s
        self.hull = G["Body"](s["hull"], s.get("square", 2.0))
        fins = s.get("fins") or [(s["fin"], 0.0)]
        self.fin = G["Wing"](fins[0][0], 1.0, True, fins[0][1])
        self.fin_y = abs(fins[0][1])                      # (a twin's fins stand this far out each side)
        self.paint_names = list(m.paints)
        self.part_names = [p.name for p in m.parts]
        self.tree = BVHTree.FromPolygons([tuple(v) for v in m.verts], [tuple(f) for f in m.faces])
        self.skin = livery.Skin(m)
        side = [mk for mk in (m.marks or []) if mk.get("place") == "side"]
        if side:
            mk = side[0]
            self.mark = (mk["at"][0], mk["at"][2], mk["radius"], mk["tag"])
        else:                                             # (a band round her painted on her skin in its place, Italy's white one:
            x0, x1 = s["band"]                            #  her marking stands either side of it as of a roundel there)
            xm = 0.5 * (x0 + x1)
            self.mark = (xm, self.hull.z(-xm), 0.5 * abs(x0 - x1), "band")
        self.fin_marks = [mk for mk in (m.marks or []) if mk.get("place") == "fin"]
        self.x_cut = m.parts[m.part_at["tail"]].pivot.x
        self.rudders = [p.name for p in m.parts if p.name.startswith("rudder")]
        boxes = []
        for kind in ("missions", "kills", "lead"):
            for q in ((getattr(m, "tally", None) or {}).get(kind) or {}).get("slots", []):
                r = (m.tally[kind].get("radius") or 0.06)
                boxes.append((q[0] - r, q[0] + r, q[2] - r, q[2] + r))
        self.tally = boxes and (min(b[0] for b in boxes), max(b[1] for b in boxes), min(b[2] for b in boxes), max(b[3] for b in boxes))

    def side_point(self, x, z, w):
        """The point of her fuselage's skin at (x, z) on side w, and its facing: looked for from just off her side, so
        that a fin or a wing standing out beyond it (a twin's fin, abeam of her letters) is not taken for her side."""
        sy = self.side_y(x, z)
        hit = self.skin.tree.ray_cast(Vector((x, w * (sy + 0.5), z)), Vector((0.0, -w, 0.0)), 1.0)
        if hit[0] is None:
            return self.skin.find(x, z, w)
        n = hit[1].normalized()
        return hit[0], (n if n.y * w > 0.0 else -n)

    def cast(self, o, way, far):
        loc, nor, idx, _ = self.tree.ray_cast(Vector(o), Vector(way), far)
        if loc is None:
            return None
        nor = nor.normalized()
        if nor.dot(way) > 0.0:
            nor = -nor
        return loc, nor, self.part_names[self.m.fpart[idx]], self.paint_names[self.m.fpaint[idx]]

    def high(self, x):
        """Her fuselage's height at x."""
        return 2.0 * self.hull.h(-x)

    def top(self, x):
        return self.hull.z(-x) + self.hull.h(-x)

    def side_y(self, x, z):
        """How far out her drawn fuselage is at x and height z (0 above or below it)."""
        h = self.hull
        t = (z - h.z(-x)) / max(h.h(-x), 1e-6)
        if abs(t) >= 1.0:
            return 0.0
        a = math.asin(math.copysign(abs(t) ** (1.0 / h.power), t))
        return abs(h.at(x, a).y)

    # ---- her fin, seen square from one side, on a grid ----
    def fin_grid(self, w, skip=()):
        """Her fin from side w (+1 port, -1 starboard): each spot of a grid 0 (not her fin), 1 (her fin), 2 (her
        rudder), and the grid's corner. Her fin's own marks (but those tagged in `skip`), and their margin, are 0."""
        fin = self.fin
        us = [fin.root + (fin.tip - fin.root) * k / 40.0 for k in range(41)]
        x_hi, x_lo = max(fin.lead(u) for u in us) + 0.05, min(fin.trail(u) for u in us) - 0.05
        z_lo, z_hi = fin.root, fin.tip
        nx, nz = int(math.ceil((x_hi - x_lo) / STEP)) + 1, int(math.ceil((z_hi - z_lo) / STEP)) + 1
        lab = np.zeros((nz, nx), np.int8)
        y_f = self.fin_y * w
        for iz in range(nz):
            z = z_lo + iz * STEP
            for ix in range(nx):
                x = x_lo + ix * STEP
                if self.fin_y == 0.0 and z < self.top(x) + 0.03:
                    continue                             # (her fuselage, under a single fin)
                hit = self.cast((x, y_f + w * 1.2, z), Vector((0.0, -w, 0.0)), 2.4)
                if hit is None:
                    continue
                loc, nor, part, paint = hit
                if abs(nor.y) < 0.85 or abs(loc.y - y_f) > 0.2 or paint.split(".")[0] not in SKIN_PAINTS:
                    continue
                lab[iz, ix] = 1 if part == "tail" else 2 if part.startswith("rudder") else 0
        gx, gz = np.meshgrid(x_lo + STEP * np.arange(nx), z_lo + STEP * np.arange(nz))
        for mk in self.fin_marks:                        # (a flag on her rudder, a flash on her fin: kept clear)
            if mk["tag"] in skip:
                continue
            if mk.get("quad"):
                poly = [tuple(q) for q in mk["quad"]]
            else:
                x, z, r = mk["at"][0], mk["at"][2], mk["radius"]
                poly = [(x - r, z - r), (x + r, z - r), (x + r, z + r), (x - r, z + r)]
            lab[near(gx, gz, poly, FIN_MARGIN)] = 0
        return lab, x_lo, z_lo


def near(xs, zs, poly, margin):
    """Which points of a grid (xs, zs: arrays) are inside a polygon, or within `margin` of it."""
    inside = np.zeros(xs.shape, bool)
    close = np.zeros(xs.shape, bool)
    for i in range(len(poly)):
        (x1, z1), (x2, z2) = poly[i], poly[(i + 1) % len(poly)]
        if z1 != z2:
            inside ^= ((z1 > zs) != (z2 > zs)) & (xs < x1 + (x2 - x1) * (zs - z1) / (z2 - z1))
        dx, dz = x2 - x1, z2 - z1
        t = np.clip(((xs - x1) * dx + (zs - z1) * dz) / max(dx * dx + dz * dz, 1e-12), 0.0, 1.0)
        close |= (xs - x1 - t * dx) ** 2 + (zs - z1 - t * dz) ** 2 < margin * margin
    return inside | close


class Place:
    """One place a character can stand: its style, side and number, its middle (x, z: her own metres), its capital
    height and its cell's width; where it is painted; the frame it is laid in on her skin (at, n, u, v); the lines
    that part it (each (a, b, c) in its own u, v metres, with the part on each side of it); the part it rides."""

    def __init__(self, style, side, k, x, z, h, cell, kind, glyphs, coloured, on, lean=None):
        self.style, self.side, self.k, self.x, self.z, self.h, self.cell = style, side, k, x, z, h, cell
        self.kind, self.glyphs, self.coloured, self.on = kind, glyphs, coloured, on
        self.lean = LEAN[style][k - 1] if lean is None else lean
        self.w = 1.0 if side == "port" else -1.0
        self.lines, self.part = [], None

    def frame(self, her):
        p, n = self.fin_point(her) if self.on == "fin" else her.side_point(self.x, self.z, self.w)
        n = Vector((n.x, n.y, 0.0)).normalized() if abs(n.z) < 0.9 else n
        v_ = (Z - n * Z.dot(n)).normalized()
        u_ = v_.cross(n)
        self.at, self.n, self.u, self.v = p, n, u_, v_
        hit = her.cast(p + n * 0.3, -n, 0.6)
        self.part = hit[2] if hit else "plane"

    def fin_point(self, her):
        y_f = her.fin_y * self.w
        hit = her.skin.tree.ray_cast(Vector((self.x, y_f + self.w * 1.2, self.z)), Vector((0.0, -self.w, 0.0)), 2.4)
        if hit[0] is None:
            return Vector((self.x, y_f, self.z)), Vector((0.0, self.w, 0.0))
        return hit[0], Vector((0.0, self.w, 0.0))

    def name(self, ch, colour=None):
        return f"{self.style}.{self.side}.{self.k}.{ch}" + (f".{colour}" if colour else "")


# ---- where each style stands ---------------------------------------------------------------------------------------
def cell_of(kind, h, narrow):
    """How wide a place is: its kind's width at this height, narrowed as its style is (the dash fills its own)."""
    return WIDE[kind] * h * narrow


def side_places(her, style, h_most, at=None):
    """A style on her fuselage, anchored at her national marking (or about the middle `at`: `ON_SIDE`): its places'
    middles on each side. As tall as h_most, but no taller than three fifths of her fuselage where it stands; smaller
    (to `HIDE_SHRINK` of it) if that brings every place into plain sight from far abeam (`hidden`: her fin, her
    tailplane, a nacelle); and smaller still (to `SHRINK` of that) if that keeps it `CLEAR` of a window, a gun or
    anything else that is not her painted skin."""
    x_m, z_m, r, _ = her.mark

    def run(hh):
        return _side_at(her, style, hh, at) if at is not None else _side_run(her, style, hh, x_m, z_m, r)
    h = h_most
    for _ in range(4):
        out = run(h)
        h_fit = min([h_most] + [TALL * min(her.high(p.x - p.cell / 2.0), her.high(p.x + p.cell / 2.0)) for p in out])
        if abs(h_fit - h) < 0.005:
            break
        h = h_fit

    def biggest(least, score):
        best, k = None, 0
        while True:
            hh = round(h - 0.01 * k, 3)
            if hh < least * h - 1e-6:
                break
            out = run(hh)
            n = score(out)
            if best is None or n < best[0]:
                best = (n, hh, out)
            if n == 0:
                break
            k += 1
        return best
    h = biggest(HIDE_SHRINK, lambda out: hidden(her, out))[1]
    return biggest(SHRINK, lambda out: crowded(her, out) + 1000 * hidden(her, out))[2]


def blocker(her, p, x, z):
    """The part of hers that hides the spot (x, z) of a place from far off square abeam (standing out more than 0.3 m
    beyond her skin there: a fin, her tailplane, a nacelle, her wing, a gun), or None."""
    way = Vector((0.0, -p.w, 0.0))
    far = her.cast((x, p.w * 40.0, z), way, 80.0)
    if far is None or far[0].y * p.w <= 0.0:
        return None
    skin = her.fin_y if p.on == "fin" else her.side_y(x, z)
    return far[2] if abs(far[0].y) - skin > 0.3 else None


def hidden(her, places):
    """How many points of the places' cells, and of HIDE_MARGIN round them, something of hers hides from far off
    square abeam (`blocker`), but her guns and her wings (`GUNS`, `WINGS`). Looked at every 5 cm along her and 2.5 cm
    up."""
    bad = 0
    for p in places:
        x0, x1 = p.x - p.cell / 2.0 - HIDE_MARGIN, p.x + p.cell / 2.0 + HIDE_MARGIN
        z0, z1 = p.z - 0.53 * p.h - HIDE_MARGIN, p.z + 0.53 * p.h + HIDE_MARGIN
        nx, nz = int(math.ceil((x1 - x0) / 0.05)) + 1, int(math.ceil((z1 - z0) / 0.025)) + 1
        for i in range(nx):
            for j in range(nz):
                part = blocker(her, p, x0 + (x1 - x0) * i / (nx - 1), z0 + (z1 - z0) * j / (nz - 1))
                if part and not part.startswith(GUNS + WINGS):
                    bad += 1
    return bad


def crowded(her, places):
    """How many points of the places' cells, and of a margin round them (`CLEAR` fore and aft, `CLEAR_UP` above and
    below), lie on something that is not her painted skin, seen from just off her side: a window or a hatch (a hole
    in her side), a gun, a part standing proud of her. Looked at every 5 cm."""
    bad = 0
    hull = her.hull
    for p in places:
        x0, x1 = p.x - p.cell / 2.0 - CLEAR, p.x + p.cell / 2.0 + CLEAR
        z0, z1 = p.z - 0.53 * p.h - CLEAR_UP, p.z + 0.53 * p.h + CLEAR_UP
        nx, nz = int(math.ceil((x1 - x0) / 0.05)) + 1, int(math.ceil((z1 - z0) / 0.05)) + 1
        for i in range(nx):
            for j in range(nz):
                x, z = x0 + (x1 - x0) * i / (nx - 1), z0 + (z1 - z0) * j / (nz - 1)
                sy = her.side_y(x, z)
                hit = her.cast((x, p.w * (sy + 0.35), z), Vector((0.0, -p.w, 0.0)), 0.7)
                if hit is None:                          # (a hole in her side; or above or below it, where she curves away)
                    bad += abs(z - hull.z(-x)) < 0.85 * hull.h(-x)
                elif hit[3].split(".")[0] not in SKIN_PAINTS or abs(hit[0].y) - sy > 0.08:
                    bad += 1
    return bad


def _side_run(her, style, h, x_m, z_m, r):
    st = STYLE[style]
    cells = [cell_of(kind, h, st["narrow"]) for kind, _, _, _ in st["places"]]
    space, gap = SPACE * h, 0.5 * cell_of("L", h, st["narrow"])
    out = []
    for side, w in SIDES:
        way = -w                                         # (read from her nose aft on her port side, from her tail forward on her starboard)
        group = [g if g != "aft" else (1 if side == "port" else 0) for _, g, _, _ in st["places"]]
        s = {}
        edge = -(r + gap)
        for k in reversed([k for k, g in enumerate(group) if g == 0]):
            s[k] = edge - cells[k] / 2.0
            edge -= cells[k] + space
        edge = r + gap
        for k in [k for k, g in enumerate(group) if g == 1]:
            s[k] = edge + cells[k] / 2.0
            edge += cells[k] + space
        for k, (kind, _, glyphs, coloured) in enumerate(st["places"]):     # (each leaning toward her marking)
            out.append(Place(style, side, k + 1, x_m + s[k] * way, z_m, h, cells[k], kind, glyphs, coloured, "side", 1 if group[k] == 0 else -1))
    return out


def _side_at(her, style, h, at):
    """A fin style's places on her fuselage about the middle `at` (x, z), spaced as on her fin (`fin_cells`), read
    from her nose aft on her port side and from her tail forward on her starboard."""
    st = STYLE[style]
    out = []
    for side, w in SIDES:
        for k, ((s_k, cell), (kind, _, glyphs, coloured)) in enumerate(zip(fin_cells(style, h), st["places"])):
            out.append(Place(style, side, k + 1, at[0] - s_k * w, at[1], h, cell, kind, glyphs, coloured, "side"))
    return out


def _sat(mask, pad):
    m = np.pad(mask.astype(np.int32), pad)
    return np.pad(m.cumsum(0).cumsum(1), ((1, 0), (1, 0)))


def _counts(P, pad, nz, nx, oz0, oz1, ox0, ox1):
    iz = np.arange(nz)[:, None] + pad
    ix = np.arange(nx)[None, :] + pad
    return P[iz + oz1 + 1, ix + ox1 + 1] - P[iz + oz0, ix + ox1 + 1] - P[iz + oz1 + 1, ix + ox0] + P[iz + oz0, ix + ox0]


def fin_cells(style, h):
    """A fin style's cells as (where along the way it is read, from its middle; width), in reading order."""
    st = STYLE[style]
    cells = [cell_of(kind, h, st["narrow"]) for kind, _, _, _ in st["places"]]
    space = SPACE * h
    if style == "tactical":                              # (a lone figure in the middle, two either side of it)
        pitch = cells[0] + space
        return [(-0.5 * pitch, cells[0]), (0.0, cells[1]), (0.5 * pitch, cells[2])]
    total = sum(cells) + space * (len(cells) - 1)
    out, edge = [], -total / 2.0
    for c in cells:
        out.append((edge + c / 2.0, c))
        edge += c + space
    return out


def fin_places(her, style, h_most, grids):
    """A style on her fin: the biggest it can be painted on the outer face of her fin (each fin of a twin), up to
    h_most, clear of its edges and of what her owner paints on it. On her fixed fin alone if it is as big there, or
    nearly (`ALONE`); else across her rudder's hinge too (a character across it is cut in two there), as few
    characters across it as can be, if nothing is painted on her rudder. `grids` keeps her fin's grids from one style
    to the next. Returns ([Place, ...], the grids it was fitted on)."""
    st = STYLE[style]
    skip = FIN_FREE.get(style, ())
    if skip not in grids:
        grids[skip] = {side: her.fin_grid(w, skip) for side, w in SIDES}
    g = grids[skip]
    rudder_ok = bool(her.rudders) and not [mk for mk in her.fin_marks if mk["tag"] not in skip]
    nz, nx = g["port"][0].shape
    x_lo, z_lo = g["port"][1], g["port"][2]
    pad = int(4.0 / STEP)
    sats = {side: {"fin": _sat(g[side][0] == 1, pad), "rudder": _sat(g[side][0] == 2, pad),
                   "any": _sat((g[side][0] == 1) | ((g[side][0] == 2) & rudder_ok), pad)} for side, _ in SIDES}

    def fits(h):
        """Where her code's middle can be at this size: (anywhere, on her fixed fin alone, how many characters
        cross her rudder's hinge), each a grid."""
        ok, alone = np.ones((nz, nx), bool), np.ones((nz, nx), bool)
        across = np.zeros((nz, nx), np.int32)
        for side, w in SIDES:
            for s_, c in fin_cells(style, h):
                dx = s_ * -w
                ox0, ox1 = math.floor((dx - c / 2.0 - FIN_MARGIN) / STEP), math.ceil((dx + c / 2.0 + FIN_MARGIN) / STEP)
                oz0, oz1 = math.floor((-0.53 * h - FIN_MARGIN) / STEP), math.ceil((0.53 * h + FIN_MARGIN) / STEP)
                area = (ox1 - ox0 + 1) * (oz1 - oz0 + 1)
                ok &= _counts(sats[side]["any"], pad, nz, nx, oz0, oz1, ox0, ox1) == area
                alone &= _counts(sats[side]["fin"], pad, nz, nx, oz0, oz1, ox0, ox1) == area
                ox0, ox1 = math.floor((dx - c / 2.0) / STEP), math.ceil((dx + c / 2.0) / STEP)
                oz0, oz1 = math.floor(-0.53 * h / STEP), math.ceil(0.53 * h / STEP)
                on_r = _counts(sats[side]["rudder"], pad, nz, nx, oz0, oz1, ox0, ox1) > 0
                on_f = _counts(sats[side]["fin"], pad, nz, nx, oz0, oz1, ox0, ox1) > 0
                across += (on_r & on_f).astype(np.int32)
        return ok, alone, across

    def biggest(which):
        h = h_most
        while h >= 0.15:
            got = fits(h)
            mask = got[1] if which == "alone" else got[0]
            if mask.any():
                return round(h, 3), got
            h -= 0.01
        return None, None

    h_any, got_any = biggest("any")
    if h_any is None:
        return [], grids
    h_alone, got_alone = biggest("alone") if h_any is not None else (None, None)
    if h_alone is not None and h_alone >= ALONE * h_any - 1e-6:
        h, pick = h_alone, got_alone[1]
    else:
        ok, _, across = got_any
        h, pick = h_any, ok & (across == across[ok].min())
    iz, ix = np.nonzero(pick)
    cz, cx = iz.mean(), ix.mean()                        # (in the middle of where it fits)
    j = int(np.argmin((iz - cz) ** 2 + (ix - cx) ** 2))
    x_c, z_c = x_lo + ix[j] * STEP, z_lo + iz[j] * STEP
    out = []
    for side, w in SIDES:
        for k, ((s_, c), (kind, _, glyphs, coloured)) in enumerate(zip(fin_cells(style, h), st["places"])):
            out.append(Place(style, side, k + 1, x_c + s_ * -w, z_c, h, c, kind, glyphs, coloured, "fin"))
    for p in out:
        p.grid = g[p.side]
    return out, grids


def hinge_line(her, place, grid):
    """Her rudder's hinge where it passes a place on her fin, as two points (x, z), from her fin's grid: where her
    fin gives way to her rudder on each row of the place's height."""
    lab, x_lo, z_lo = grid
    pts = []
    for iz in range(max(0, int((place.z - 0.6 * place.h - z_lo) / STEP)), min(lab.shape[0], int((place.z + 0.6 * place.h - z_lo) / STEP) + 1)):
        fin = np.nonzero(lab[iz] == 1)[0]
        rud = np.nonzero(lab[iz] == 2)[0]
        if len(fin) and len(rud):
            pts.append((x_lo + STEP * 0.5 * (fin.min() + rud.max()), z_lo + iz * STEP))
    if len(pts) < 2:
        return None
    zs = np.array([p[1] for p in pts])
    xs = np.array([p[0] for p in pts])
    b, a = np.polyfit(zs, xs, 1)                         # (x = a + b z)
    z0, z1 = zs.min(), zs.max()
    return (a + b * z0, z0), (a + b * z1, z1)


# ---- the layout of every style on her, worked out once --------------------------------------------------------------
_LAYOUT = {}


def layout(name, model=None):
    """{style: [Place, ...]} for the bomber `name`, from her built model (built here if not given): the same for her
    fine and her plain model, so that both have the same parts."""
    if name in _LAYOUT:
        return _LAYOUT[name]
    import planes
    m = model if model is not None and model.fine else planes.BUILDERS[name](True)[0]
    her = Her(name, m)
    out, grids = {}, {}
    for style in MODELS[name]:
        h_most = SIZE[name][style]
        if style in ON_SIDE.get(name, {}):
            places = side_places(her, style, h_most, ON_SIDE[name][style])
        elif STYLE[style]["on"] == "side":
            places = side_places(her, style, h_most)
        else:
            places, grids = fin_places(her, style, h_most, grids)
        for p in places:
            p.frame(her)
            p.lines = []
            box = (p.x - p.cell / 2.0, p.x + p.cell / 2.0, p.z - 0.55 * p.h, p.z + 0.55 * p.h)
            if p.on == "side" and box[0] < her.x_cut < box[1]:          # (where her tail breaks away)
                a = (her.x_cut - p.at.x) / p.u.x
                # (u grows forward on her starboard side and aft on her port side: below the cut in u is her tail
                #  on the one and her fuselage on the other)
                p.lines.append(((1.0, 0.0, a), "tail" if p.u.x > 0 else "plane", "plane" if p.u.x > 0 else "tail"))
            if p.on == "fin" and her.rudders:
                hl = hinge_line(her, p, p.grid)
                if hl:
                    xs = [hl[0][0] + (hl[1][0] - hl[0][0]) * (zz - hl[0][1]) / max(hl[1][1] - hl[0][1], 1e-6) for zz in (box[2], box[3])]
                    if min(xs) < box[1] and max(xs) > box[0]:
                        (xa, za), (xb, zb) = hl
                        ua, va = (xa - p.at.x) / p.u.x, (za - p.at.z) / p.v.z
                        ub, vb = (xb - p.at.x) / p.u.x, (zb - p.at.z) / p.v.z
                        a_, b_ = (vb - va), -(ub - ua)                  # (the line through both: a u + b v = c)
                        c_ = a_ * ua + b_ * va
                        rud = rudder_of(her, p)
                        u_aft = (-1.0 / p.u.x)                          # (her tail's way, in u)
                        aft_low = a_ * u_aft < 0.0                      # (is aft of the hinge where a u + b v < c?)
                        p.lines.append(((a_, b_, c_), rud if aft_low else "tail", "tail" if aft_low else rud))
            if p.lines:                                  # (the place rides the part its middle is on)
                (a_, b_, c_), lo_part, hi_part = p.lines[0]
                p.part = lo_part if -c_ <= 0.0 else hi_part
        out[style] = places
    _LAYOUT[name] = out
    return out


def rudder_of(her, p):
    """The rudder a place on her fin is next to (a twin's port or starboard one)."""
    if len(her.rudders) == 1:
        return her.rudders[0]
    want = "rudder.port" if p.side == "port" else "rudder.starboard"
    return want if want in her.rudders else her.rudders[0]


def pieces(place, ch, res=3, offset=0.0, edged=False):
    """A character in a place, parted where she comes apart: [(part, suffix, triangles)], the piece on the part its
    place rides first (no suffix), the other named after its part."""
    tris = drawn(place, ch, res, offset, edged)
    out = {}
    if not place.lines:
        out[place.part] = tris
    else:
        line, lo_part, hi_part = place.lines[0]
        lo, hi = split(tris, line)
        for part, t in ((lo_part, lo), (hi_part, hi)):
            if t:
                out.setdefault(part, []).extend(t)
    main = place.part
    return [(part, "" if part == main else "." + part, t) for part, t in sorted(out.items(), key=lambda kv: kv[0] != main)]


def piece_names(place, ch, colour=None):
    """[(part name, the part it rides)] for a character in a place: its piece on the place's own part, and its piece
    beyond a line she comes apart at, if any of it lies there (a tactical number's edge too). Worked out from the
    fine drawing, so that her fine and her plain layer have the same parts."""
    names = []
    edged = bool(edge_of(place.style, colour))
    for offset, res in (((0.0, RES[True]), (OUTLINE, EDGE_RES)) if edged else ((0.0, RES[True]),)):
        for part, suffix, t in pieces(place, ch, res, offset, edged):
            nm = place.name(ch, colour) + suffix
            if t and nm not in [n for n, _ in names]:
                names.append((nm, part))
    names.sort(key=lambda q: q[1] != place.part)
    return names


def edge_of(style, colour):
    """The paint of a character's edge in this style and colour, or None."""
    return EDGE.get(style, {}).get(colour)


def colours_of(place):
    return COLOURS[place.style] if place.coloured else (None,)


def paint_of(style, colour):
    if style == "code":
        return {"white": "ger_white", "red": "ger_red", "yellow": "ger_yellow"}[colour] if colour else "ger_black"
    if style == "squadron":
        return "ita_" + colour
    if style == "tactical":
        return "sov_" + colour
    return STYLE[style]["paint"]


# ---- the layer ---------------------------------------------------------------------------------------------------
def overlay(name, fine=True, model=None):
    """The codes layer for the bomber `name` (`model`: hers, if it is already built), or None for one without codes."""
    import planes
    if name not in MODELS:
        return None
    m = model if model is not None else planes.BUILDERS[name](fine)[0]
    lay_out = layout(name, m)
    d = Model(name + "Codes", m.title + ": her squadron's or unit's marking, letter by letter", "damage", dict(PAINTS), fine)
    d.parts = [Part(p.name, p.parent, p.pivot, axis=p.axis, limits=p.limits, slide=p.slide, slide_limits=p.slide_limits, kind=p.kind, note=p.note) for p in m.parts]
    d.part_at = dict(m.part_at)
    d.clips = {k: dict(c, events=[]) for k, c in m.clips.items()}
    skin = livery.Skin(m)
    kinds = {style: [] for style in MODELS[name]}
    res = RES[fine]
    for style, places in lay_out.items():
        du, dv = BANDS[places[0].on if places else "side"][fine]
        for p in places:
            for ch in p.glyphs:
                for colour in colours_of(p):
                    names = piece_names(p, ch, colour)
                    for nm, part in names:
                        d.part(nm, part, pivot=tuple(p.at - p.n * 0.02), kind="fixed",
                               note=f"{style}: {ch!r} in place {p.k} on her {p.side} side" + (f", {colour}" if colour else "")
                                    + (f" (its piece on her {part})" if part != p.part else ""))
                        kinds[style].append(nm)
                    own = dict(names)
                    paint = paint_of(style, colour)
                    edged = bool(edge_of(style, colour))
                    up = LIFT if fine else LIFT_PLAIN
                    layers = [(0.0, paint, up, res, dv)]
                    if edged:                                        # (its edge first, under it; on her plain model too, in squares,
                                                                     #  as finely banded as its fill: her broad facets' corners)
                        layers = [(OUTLINE if fine else BLOCK_EDGE, edge_of(style, colour), up, EDGE_RES if fine else 0, dv if fine else dv * EDGED_FILL),
                                  (0.0, paint, up + (EDGE_UP if fine else EDGE_UP_PLAIN), res, dv * EDGED_FILL)]
                    for offset, pnt, lift, r, bv in layers:
                        for part, suffix, tris in pieces(p, ch, r, offset, edged):
                            nm = p.name(ch, colour) + suffix
                            if nm not in own:                        # (a sliver the plain drawing has and the fine one has not)
                                nm = names[0][0]
                            d.use(nm)
                            pts, faces = skin.lay(banded(tris, du, bv), p.at, p.n, Z, lift=lift, cell=100.0)
                            d.add(pts, faces, pnt, smooth=False)
    every = [q for v in kinds.values() for q in v]
    d.clips["none"] = {"long": 1.0, "loop": False, "after": "hold", "tracks": [{"part": q, "scale": [[0.0, 0.001]]} for q in every], "events": [],
                       "about": "Every letter and digit hidden: play it, then show at full size (scale 1) the parts codes.wear() names for her mark."}
    d.info = {"file": {"for": name, "codes": kinds, "styles": {style: _about(style, places) for style, places in lay_out.items()},
                       "paints": dict(PAINTS),
                       "about": "Show the parts of her mark at full size and leave the rest at nothing. A part is "
                                "<style>.<side>.<place>.<character>[.<colour>]; a character cut by her rudder's hinge or "
                                "by the line where her tail breaks away has a second part, its piece beyond the line, named "
                                "with that part after it (.rudder, .tail, .plane). `styles` says what each place holds and "
                                "how a mark fills them (codes.wear does it), and `never` the marks to paint nothing for: "
                                "famous real aircraft's codes, read with its dash ('KMB' is KM-B), * for any letters."}}
    d.kinds = kinds
    return d


READ = {
    "letter": "'KM-G': the two letters before the dash in places 1 and 2 (one alone in 2), the letter after it in 3; both sides.",
    "code": "'5J+DM': the two characters before the + in places 1 and 2 (one alone in 2), the aircraft's letter in 3 in her "
            "squadron's colour (white, red or yellow; by default from her squadron's letter: H M R U white, K N S V red, "
            "L P T W yellow), her squadron's letter in 4 (black); both sides. A bare code ('5JDM') is read as two and two.",
    "tail": "'K-312': the unit's code right up to the dash (K alone in 2; K1 in 1 and 2), the dash in 3, the number from 4 "
            "on (312 in 4, 5, 6); both sides.",
    "squadron": "'278-7': the squadron's number right up to the dash (278 in 1, 2, 3), the dash in 4, the aircraft's number "
                "from 5 (7 in 5; 12 in 5 and 6); black on a light finish (lightness L* 50 or more), white on a dark one; both sides.",
    "tactical": "'White 23': its colour (white, red, yellow or blue) and its number: one figure in place 2, two in 1 and 3; both sides.",
    "number": "'14' (the campaign's mark; 'No. 14' as well): its figures, aft of the marking: on her port side from place 1 "
              "(14 in 1 and 2), on her starboard side ending at place 3 (14 in 2 and 3).",
}


def _about(style, places):
    return {"read": READ[style], "where": "her fin" if places and places[0].on == "fin" else "her fuselage",
            "never": list(FAMOUS.get(style, ())),
            "places": [{"side": p.side, "place": p.k, "x": round(p.x, 3), "z": round(p.z, 3), "high": p.h, "cell": round(p.cell, 3),
                        "holds": p.glyphs, "colours": list(colours_of(p)) if p.coloured else [], "rides": p.part} for p in places]}


# ---- who wears what ----------------------------------------------------------------------------------------------
class Worn(list):
    """The parts to show for one aircraft's mark; `warnings` says what could not be painted."""

    def __init__(self, *a):
        super().__init__(*a)
        self.warnings = []


def lightness(hexcol):
    import tallies
    return tallies.lightness(hexcol)


def wear(name, style, mark, colour=None, model=None):
    """The parts to show for one aircraft's `mark` in a style, on both her sides: e.g.
    wear("Lancaster", "letter", "KM-G"), wear("He111", "code", "5J+DM", colour="red"), wear("Betty", "tail", "K-312"),
    wear("Bomber", "tactical", "White 23"), wear("Bomber", "number", "No. 14").
    colour: the aircraft letter's colour for "code" (white, red or yellow; else her squadron's), "black" or "white"
    (or the hex colour of the finish under it) for "squadron", the number's colour for "tactical" (else from the
    mark's own word). A character a style has no part for is left out and said in `warnings` (printed too)."""
    out = Worn()

    def warn(text):
        if text not in out.warnings:
            out.warnings.append(text)
            print(f"codes.wear {name} {style} {mark!r}: {text}", flush=True)

    if name not in MODELS or style not in MODELS[name]:
        warn(f"{name} has no {style!r} codes (she wears {', '.join(MODELS.get(name, ())) or 'none'})")
        return out
    text = str(mark or "").strip().upper()
    if not text:
        warn("no mark: nothing painted")
        return out
    bare = text.replace(" ", "")                         # (read as the styles below read it: 'KMB' is KM-B, 'T1323' T1-323)
    whole = bare if "-" in bare else {"letter": bare[:2] + "-" + bare[2:], "tail": bare[:-3] + "-" + bare[-3:]}.get(style, bare)
    if any(fnmatch.fnmatchcase(whole, pat) for pat in FAMOUS.get(style, ())):
        warn("this is a famous real aircraft's code: nothing painted (the campaign should not draw it)")
        return out
    places = {(p.side, p.k): p for p in layout(name, model)[style]}
    want = []                                            # ([(side, place, character, colour)])

    def fill(ks, chars, right, sides=("port", "starboard"), col=None):
        if len(chars) > len(ks):
            warn(f"{chars!r} is longer than its {len(ks)} places: painted {chars[:len(ks)]!r}")
            chars = chars[:len(ks)]
        use = ks[len(ks) - len(chars):] if right else ks[:len(chars)]
        for side in sides:
            for k, ch in zip(use, chars):
                want.append((side, k, ch, col))

    if style == "letter":
        a, _, b = text.replace(" ", "").partition("-") if "-" in text else (text.replace(" ", "")[:2], "", text.replace(" ", "")[2:])
        fill([1, 2], a, True)
        fill([3], b, False)
    elif style == "code":
        t = text.replace(" ", "")
        if "+" in t or "-" in t:                         # (the cross written as a dash, '5J-DM', is read as the cross)
            a, _, b = t.partition("+" if "+" in t else "-")
        else:                                            # (a bare code, '5JDM': its wing's two characters and two letters)
            a, b = t[:2], t[2:]
            if len(t) != 4:
                warn(f"no '+': read as {a!r} + {b!r}")
        own = (colour or STAFFEL.get(b[1:2], "white") if len(b) > 1 else colour or "white").lower()
        if own not in COLOURS["code"]:
            warn(f"no {own!r} letters: white")
            own = "white"
        fill([1, 2], a, True)
        fill([3], b[:1], False, col=own)
        fill([4], b[1:], False)
    elif style == "tail":
        t = text.replace(" ", "")
        if "-" in t:
            a, _, b = t.partition("-")
        else:
            a, b = t[:-3], t[-3:]
            warn("no dash: read as unit " + repr(a) + " and number " + repr(b))
        fill([1, 2], a, True)
        if "-" in t:
            fill([3], "-", False)
        fill([4, 5, 6], b, False)
    elif style == "squadron":
        t = text.replace(" ", "")
        a, dash, b = t.partition("-")
        col = (colour or "black").lower()
        if col not in COLOURS["squadron"]:
            try:
                col = "black" if lightness(col.lstrip("#")) >= 50.0 else "white"
            except (ValueError, IndexError):
                warn(f"no {colour!r} figures: black")
                col = "black"
        fill([1, 2, 3], a, True, col=col)
        if dash:
            fill([4], "-", False, col=col)
        fill([5, 6], b, False, col=col)
    elif style == "tactical":
        words = text.replace(".", " ").split()
        word = next((w.lower() for w in words if w.lower() in COLOURS["tactical"]), None)
        other = [w for w in words if w.isalpha() and w.lower() not in COLOURS["tactical"]]
        if other and not colour and not word:
            warn(f"no {other[0].lower()!r} figures: white")
        col = (colour or word or "white").lower()
        if col not in COLOURS["tactical"]:
            warn(f"no {col!r} figures: white")
            col = "white"
        figs = "".join(w for w in words if w.isdigit()) or "".join(c for c in text if c.isdigit())
        if len(figs) == 1:
            fill([2], figs, False, col=col)
        else:
            if len(figs) > 2:
                warn(f"{figs!r}: only two figures have places: painted {figs[:2]!r}")
            fill([1, 3], figs[:2], False, col=col)
    elif style == "number":
        figs = "".join(c for c in text if c.isdigit())
        if figs != text.replace("NO.", "").replace("NO", "").replace(" ", "").replace("#", ""):
            warn("only its figures are painted: " + repr(figs))
        fill([1, 2, 3], figs, False, sides=("port",))
        fill([1, 2, 3], figs, True, sides=("starboard",))
    for side, k, ch, col in want:
        p = places.get((side, k))
        if p is None:
            warn(f"no place {k} on her {side} side")
            continue
        if ch not in p.glyphs:
            warn(f"no {ch!r} in place {k} ({side})")
            continue
        out.extend(nm for nm, _ in piece_names(p, ch, col if p.coloured else None))
    return out


# ---- checks ------------------------------------------------------------------------------------------------------
def check(name, model=None):
    """Every place of every style on her, looked at: what lies under its cell (her skin, or glass, a window, a part of
    her standing proud of her side), what of hers hides it from far off square abeam (a fin, her tailplane, a wing
    from her level), and whether it comes near her national marking or her record. Returns a list of (style, side,
    place, trouble). Every trouble is a fault of the layout but one that begins "crossed": a gun's barrel, which
    swings across a character now and then, or her wing seen from her own level or below (`WINGS`), which an eye
    1.5 degrees above her sees clear of the letters."""
    import planes
    m = model if model is not None else planes.BUILDERS[name](True)[0]
    her = Her(name, m)
    x_m, z_m, r, _ = her.mark
    out = []
    for style, places in layout(name, m).items():
        for p in places:
            x0, x1, z0, z1 = p.x - p.cell / 2.0, p.x + p.cell / 2.0, p.z - 0.52 * p.h, p.z + 0.52 * p.h
            bad, seen = {}, {}
            for i in range(9):
                for j in range(9):
                    x, z = x0 + (x1 - x0) * i / 8.0, z0 + (z1 - z0) * j / 8.0
                    way = Vector((0.0, -p.w, 0.0))
                    if p.on == "fin":
                        y_f = her.fin_y * p.w
                        hit = her.cast((x, y_f + p.w * 1.2, z), way, 2.4)
                    else:                                # (from just off her side)
                        sy = her.side_y(x, z)
                        hit = her.cast((x, p.w * (sy + 0.35), z), way, 0.7)
                    part = blocker(her, p, x, z)         # (and from far abeam, for what hides it)
                    if part:
                        seen[part] = seen.get(part, 0) + 1
                    if hit is None:
                        bad["nothing"] = bad.get("nothing", 0) + 1
                        continue
                    loc, nor, part, paint = hit
                    if paint.split(".")[0] not in SKIN_PAINTS:
                        bad[paint] = bad.get(paint, 0) + 1
                    elif p.on == "side" and abs(loc.y) - her.side_y(x, z) > 0.08:
                        bad["proud " + part] = bad.get("proud " + part, 0) + 1
                    elif p.on == "side" and nor.z ** 2 > 0.5:
                        bad["round"] = bad.get("round", 0) + 1
            for k, v in bad.items():
                out.append((style, p.side, p.k, f"{v} of 81 points on {k}"))
            for k, v in seen.items():
                if k.startswith(GUNS):
                    out.append((style, p.side, p.k, f"crossed from far abeam by her {k} (a gun, which swings): {v} of 81 points"))
                elif k.startswith(WINGS):
                    out.append((style, p.side, p.k, f"crossed from far abeam by her {k}, seen from her level (clear from 1.5 degrees above): {v} of 81 points"))
                else:
                    out.append((style, p.side, p.k, f"hidden from far abeam behind her {k}: {v} of 81 points"))
            if p.on == "side" and max(x0, x_m - r) < min(x1, x_m + r) and max(z0, z_m - r) < min(z1, z_m + r):
                out.append((style, p.side, p.k, "across her national marking"))
            if her.tally and p.side == "port" and max(x0, her.tally[0]) < min(x1, her.tally[1]) and max(z0, her.tally[2]) < min(z1, her.tally[3]):
                out.append((style, p.side, p.k, "on her record"))
    return out


def sinks(name, finish=None, model=None):
    """Where a style's paint sinks into the finish under it by lightness (the battle grade greys every colour, and
    lightness is what is left): [(style, colour, paint, finish, the gap in L*, edged?)] for each gap under SINK.
    The finish is her own coat under the places (her upper colours and, where it is sprayed up her sides, her under
    one), or `finish`: the hex colours of the coat an owner gives the plain Bomber."""
    import planes
    import tallies
    m = model if model is not None else planes.BUILDERS[name](True)[0]
    her = Her(name, m)
    names = list(m.paints)
    out = []
    for style, places in layout(name, m).items():
        seen = {}
        for p in places:
            for i in range(5):
                for j in range(5):
                    x, z = p.x - p.cell / 2.0 + p.cell * i / 4.0, p.z - 0.5 * p.h + p.h * j / 4.0
                    o = (x, her.fin_y * p.w + p.w * 1.2, z) if p.on == "fin" else (x, p.w * (her.side_y(x, z) + 0.35), z)
                    loc, nor, idx, _ = her.tree.ray_cast(Vector(o), Vector((0.0, -p.w, 0.0)), 2.4 if p.on == "fin" else 0.7)
                    if loc is None:
                        continue
                    face = m.faces[idx]
                    paint = names[m.fpaint[idx]]
                    if idx < len(m.fcoat) and m.fcoat[idx]:           # (a sprayed coat: the paint of its nearest corner)
                        v = min(face, key=lambda q: (Vector(m.verts[q]) - loc).length)
                        paint = names[m.vcoat[v]] if m.vcoat[v] >= 0 else paint
                    if paint == "upper" and max(m.vblend[q] for q in face) > 0.2:
                        seen["upper2"] = seen.get("upper2", 0) + 1
                    seen[paint] = seen.get(paint, 0) + 1
        under = {f"#{h}": h for h in finish} if finish else {k: m.paints[k] for k, n in seen.items() if k in ("upper", "upper2", "under") and n >= 2}
        for colour in sorted({c for p in places for c in colours_of(p)}, key=str):
            paint = paint_of(style, colour)
            for f, hx in under.items():
                gap = tallies.lightness(PAINTS[paint]) - tallies.lightness(hx)
                if abs(gap) < SINK:
                    out.append((style, colour, paint, f, round(gap, 1), bool(edge_of(style, colour))))
    return out
